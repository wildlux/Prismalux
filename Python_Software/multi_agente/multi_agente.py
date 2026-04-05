"""
Sistema Multi-Agente Avanzato con LangGraph + Ollama
=====================================================

Implementa una pipeline di 6 agenti AI specializzati che collaborano per
generare, testare e ottimizzare codice Python. Ogni agente usa un modello
Ollama dedicato, selezionabile dall'utente, e comunica tramite un dizionario
di stato condiviso (TypedDict ``Stato``).

Pipeline principale (``avvia_menu`` / ``avvia_pipeline``):
  Ricercatore → Pianificatore → 2 Programmatori (paralleli)
  → Tester (loop max 3 tentativi) → Ottimizzatore

Motore Byzantino (``motore_byzantino``):
  Anti-allucinazione a 4 agenti: A (Esperto) → B (Avvocato del Diavolo)
  → C (Gemello Indipendente) → D (Giudice Quantico).
  Logica booleana: T = (A ∧ C) ∧ ¬B_valido

Funzionalità principali:
  1. Loop di correzione automatica (max 3 tentativi con auto-retry)
  2. Salvataggio codice su file .py e report HTML
  3. Selezione modello interattiva con auto-detection GPU
  4. Agente OTTIMIZZATORE: built-in C, generator, set, lru_cache
  5. Memoria task precedenti in ~/.multi_agente_memoria.json
  6. Report HTML dark-theme con piano A/B, codice, verdict
  7. Agente RICERCATORE: contesto, librerie, task simili in memoria
  8. Pianificatore: 2 approcci alternativi → 2 programmatori in parallelo
  9. Multi-GPU: distribuzione agenti su più istanze Ollama
 10. Prompt di qualità predefiniti per livello (scolastico/universitario/ricercatore)
 11. Ottimizzatore prompt con AI prima di avviare la pipeline
 12. Bridge Cython: profiling cProfile + compilazione C opzionale

Dipendenze:
  pip install langgraph langchain-ollama langchain-core requests
"""

# ── Controllo dipendenze ──────────────────────────────────────────────────────
# DEVE stare prima di tutti gli altri import: se manca una libreria
# lo script altrimenti crasha con un errore incomprensibile.

import sys
import importlib
import importlib.util
import os

def _check_deps_multiagente() -> None:
    """Verifica che tutte le dipendenze richieste siano installate.

    Deve essere chiamata PRIMA di qualsiasi altro import del modulo perché
    Python esegue gli import a livello di modulo all'avvio. Se si importasse
    prima, ad esempio, ``langgraph`` e questo non fosse installato, Python
    solleverebbe un ``ModuleNotFoundError`` con un traceback incomprensibile
    per l'utente. Chiamando questa funzione per prima, si può stampare un
    messaggio chiaro con i comandi di installazione e uscire con ``sys.exit(1)``.

    In caso di dipendenze mancanti stampa un pannello Rich (se disponibile)
    o un output plain-text con la lista dei pacchetti da installare e il
    comando ``pip install`` pronto all'uso, poi termina il processo.
    """
    _DIPENDENZE = [
        ("langgraph",        "langgraph",          "Grafo degli agenti"),
        ("langchain_ollama", "langchain-ollama",    "Connessione a Ollama"),
        ("langchain_core",   "langchain-core",      "Componenti base LangChain"),
        ("requests",         "requests",            "Chiamate HTTP a Ollama"),
    ]
    mancanti = [(pkg, desc) for imp, pkg, desc in _DIPENDENZE
                if not importlib.util.find_spec(imp)]
    if not mancanti:
        return

    pkgs = " ".join(p for p, _ in mancanti)
    try:
        from rich.console import Console as _CR
        from rich.panel import Panel as _PR
        from rich.text import Text as _TR
        _cr = _CR()
        t = _TR()
        t.append("❌ LIBRERIE MANCANTI — impossibile avviare il multi-agente\n\n", style="bold red")
        for pkg, desc in mancanti:
            t.append(f"  ✗  {pkg:<28} {desc}\n", style="red")
        t.append("\nInstalla con:\n", style="bold")
        t.append(f"  pip install {pkgs}\n", style="cyan")
        _cr.print(_PR(t, border_style="red"))
    except ImportError:
        linea = "─" * 58
        print(f"\n{linea}")
        print(f"  ❌ LIBRERIE MANCANTI — impossibile avviare il multi-agente")
        print(f"{linea}\n")
        for pkg, desc in mancanti:
            print(f"  ✗  {pkg:<28} {desc}")
        print(f"\n  Installa con:\n  pip install {pkgs}\n{linea}\n")
    sys.exit(1)

_check_deps_multiagente()

# ── Import librerie esterne (ora sappiamo che ci sono) ────────────────────────

import re
import json
import threading
import subprocess
import tempfile
import time
try:
    import tty
    import termios
    _HAS_TERMIOS = True
except ImportError:
    _HAS_TERMIOS = False   # Windows: usa fallback testuale
import requests
from datetime import datetime
from typing import TypedDict
from langgraph.graph import StateGraph, END
from langchain_ollama import ChatOllama
from langchain_core.messages import SystemMessage, HumanMessage

try:
    from rich.console import Console as _RichConsole
    from rich.panel import Panel as _RichPanel
    _rich = _RichConsole()
    _RICH_OK = True
except ImportError:
    _RICH_OK = False
    _rich = None

# ── Costanti ──────────────────────────────────────────────────────────────────

MAX_TENTATIVI   = 3
FILE_MEMORIA    = os.path.expanduser("~/.multi_agente_memoria.json")
FILE_LLM_DB     = os.path.expanduser("~/.multi_agente_llm_db.json")
CARTELLA_OUTPUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "esportazioni")
os.makedirs(CARTELLA_OUTPUT, exist_ok=True)

# ── Multi GPU ─────────────────────────────────────────────────────────────────

PORTA_NVIDIA = 11434   # porta principale di Ollama (default, qualunque GPU)
PORTA_INTEL  = 11435   # seconda istanza (Intel Vulkan)
PORTA_CPU    = 11435   # seconda istanza in modalità CPU  (OLLAMA_NUM_GPU=0)
PORTA_AMD    = 11436   # terza istanza (AMD ROCm o Vulkan)

# GPU extra supportate (su porte aggiuntive)
# Per avviarle (usa OLLAMA_HOST, non --port):
#   Intel  → OLLAMA_LLM_LIBRARY=vulkan OLLAMA_HOST=127.0.0.1:11435 ollama serve
#   AMD    → OLLAMA_LLM_LIBRARY=rocm   OLLAMA_HOST=127.0.0.1:11436 ollama serve
#             (fallback Vulkan se ROCm manca: OLLAMA_LLM_LIBRARY=vulkan OLLAMA_HOST=127.0.0.1:11436 ollama serve)
GPU_EXTRA: dict[str, dict] = {
    "Intel": {"porta": PORTA_INTEL, "lib": "vulkan", "emoji": "🔵"},
    "AMD":   {"porta": PORTA_AMD,   "lib": "rocm",   "emoji": "🔴"},
}


def _rileva_gpu_principale() -> tuple[str, str, str]:
    """
    Rileva la GPU realmente presente nel sistema per etichettare correttamente
    la porta principale di Ollama (11434).
    Ritorna (emoji, nome_breve, tipo_accelerazione).
    Prova in ordine: nvidia-smi → rocm-smi → WMI (Windows) → lspci (Linux).
    """
    import re as _re

    # 1. NVIDIA — nvidia-smi è disponibile su Windows e Linux se installato
    try:
        r = subprocess.run(
            ["nvidia-smi", "--query-gpu=name", "--format=csv,noheader"],
            capture_output=True, text=True, timeout=3,
        )
        if r.returncode == 0 and r.stdout.strip():
            nome = r.stdout.strip().splitlines()[0].strip()
            nome = nome.replace("NVIDIA GeForce ", "").replace("NVIDIA ", "")
            return "🟢", nome or "NVIDIA", "CUDA"
    except Exception:
        pass

    # 2. AMD ROCm — rocm-smi (Linux con driver ROCm)
    try:
        r = subprocess.run(
            ["rocm-smi", "--showproductname"],
            capture_output=True, text=True, timeout=3,
        )
        if r.returncode == 0 and r.stdout.strip():
            m = _re.search(r"Card series:\s*(.+)", r.stdout, _re.IGNORECASE)
            nome = m.group(1).strip() if m else "AMD"
            return "🔴", nome, "ROCm"
    except Exception:
        pass

    # 3. Windows — WMI query (funziona con qualsiasi scheda video)
    if sys.platform == "win32":
        try:
            r = subprocess.run(
                ["wmic", "path", "win32_VideoController", "get", "name"],
                capture_output=True, text=True, timeout=5,
            )
            if r.returncode == 0:
                nomi_w = [
                    l.strip() for l in r.stdout.splitlines()
                    if l.strip() and l.strip().lower() != "name"
                ]
                for nome in nomi_w:
                    low = nome.lower()
                    if "nvidia" in low:
                        return "🟢", nome, "CUDA"
                    # Su Windows AMD usa Vulkan (ROCm non disponibile su Windows)
                    if "amd" in low or "radeon" in low or "ati" in low:
                        return "🔴", nome, "Vulkan"
                    if "intel" in low:
                        return "🔵", nome, "Vulkan"
                if nomi_w:
                    return "⚪", nomi_w[0], "GPU"
        except Exception:
            pass

    # 4. Linux — lspci (tutte le distribuzioni)
    try:
        r = subprocess.run(["lspci"], capture_output=True, text=True, timeout=3)
        if r.returncode == 0:
            for riga in r.stdout.splitlines():
                low = riga.lower()
                if not any(k in low for k in ("vga", "display", "3d controller")):
                    continue
                m = _re.search(r"\[([^\]]+)\]", riga)
                nome = m.group(1).strip() if m else ""
                if "nvidia" in low:
                    return "🟢", nome or "NVIDIA", "CUDA"
                if "amd" in low or "radeon" in low or "ati" in low:
                    return "🔴", nome or "AMD", "ROCm/Vulkan"
                if "intel" in low:
                    return "🔵", nome or "Intel", "Vulkan"
    except Exception:
        pass

    # Fallback: CPU (Ollama funziona anche solo CPU)
    return "⚪", "CPU", "CPU"


# Rileva GPU una sola volta all'avvio (chiamata a livello di modulo)
_GPU_PRINC: tuple[str, str, str] = _rileva_gpu_principale()

# Mappa porta → (emoji, nome GPU, tipo accelerazione)
_PORTA_A_GPU: dict[int, tuple[str, str, str]] = {
    PORTA_NVIDIA: _GPU_PRINC,          # rilevata dinamicamente
    PORTA_INTEL:  ("🔵", "Intel",   "Vulkan"),
    PORTA_AMD:    ("🔴", "AMD",     "ROCm"),
}

# Shorthand per la GPU principale (usati nelle stringhe di display)
_GP_EMOJI, _GP_NOME, _GP_ACC = _GPU_PRINC


def _gpu_label(porta: int) -> str:
    """Restituisce es. '🔵 Intel (Vulkan) :11435'"""
    emoji, nome, acc = _PORTA_A_GPU.get(porta, ("⚪", "GPU", "?"))
    return f"{emoji} {nome} ({acc}) :{porta}"


def _gpu_stats(porta: int) -> str:
    """
    Mostra utilizzo GPU in tempo reale:
    - CUDA  → nvidia-smi (VRAM + % load)
    - ROCm  → rocm-smi (% load)
    - altro → modello caricato su quella porta Ollama
    """
    emoji, nome, acc = _PORTA_A_GPU.get(porta, ("⚪", "GPU", ""))

    if "CUDA" in acc:
        try:
            r = subprocess.run(
                ["nvidia-smi",
                 "--query-gpu=utilization.gpu,memory.used,memory.total",
                 "--format=csv,noheader,nounits"],
                capture_output=True, text=True, timeout=3,
            )
            if r.returncode == 0:
                parti = [p.strip() for p in r.stdout.strip().split(",")]
                if len(parti) >= 3:
                    load, used, tot = parti[0], parti[1], parti[2]
                    return f"{emoji} {nome} (CUDA)  GPU:{load}%  VRAM:{used}/{tot} MiB"
        except Exception:
            pass
        return f"{emoji} {nome} (CUDA)"

    if "ROCm" in acc:
        try:
            r = subprocess.run(
                ["rocm-smi", "--showuse"],
                capture_output=True, text=True, timeout=3,
            )
            if r.returncode == 0:
                import re as _re
                m = _re.search(r"(\d+)\s*%", r.stdout)
                load = m.group(1) if m else "?"
                return f"{emoji} {nome} (ROCm)  GPU:{load}%"
        except Exception:
            pass
        return f"{emoji} {nome} (ROCm)"

    # Intel / AMD: chiedi a Ollama quale modello è in esecuzione
    try:
        r = requests.get(f"http://localhost:{porta}/api/ps", timeout=2)
        if r.status_code == 200:
            modelli = r.json().get("models", [])
            if modelli:
                nome_m = modelli[0].get("name", "?")
                vram   = modelli[0].get("size_vram", 0)
                vram_s = f"  VRAM:{vram // 1024 // 1024} MiB" if vram else ""
                return f"{emoji} {nome}  modello:{nome_m}{vram_s}"
    except Exception:
        pass
    return f"{emoji} {nome} :{porta}"


def controlla_istanza(porta: int) -> bool:
    """Verifica se un'istanza Ollama risponde sulla porta specificata."""
    try:
        r = requests.get(f"http://localhost:{porta}/api/tags", timeout=0.3)
        return r.status_code == 200
    except Exception:
        return False


def rileva_gpu_attive() -> dict:
    """Ritorna {nome: info} delle GPU extra con istanza Ollama attiva."""
    return {
        nome: info
        for nome, info in GPU_EXTRA.items()
        if controlla_istanza(info["porta"])
    }


# ── Preferenze modelli per scopo ─────────────────────────────────────────────
# Ogni lista è ordinata per priorità: il primo match disponibile vince.

PREFERENZE_MODELLI = {
    "ricercatore":   ["llama3.2", "qwen3:4b", "qwen3", "mistral", "gemma2", "deepseek-r1"],
    "pianificatore": ["deepseek-r1:7b", "deepseek-r1", "qwen3:8b", "qwen3", "mistral"],
    "programmatore": ["qwen2.5-coder", "deepseek-coder", "codellama", "qwen3:8b", "mistral", "qwen3"],
    "tester":        ["gemma2:2b", "gemma2", "llama3.2", "deepseek-r1:1.5b", "qwen3:4b", "qwen3"],
    "ottimizzatore": ["qwen2.5-coder", "deepseek-coder", "qwen3:8b", "mistral", "qwen3"],
}

_RUOLI_INFO = {
    "ricercatore":   ("🔍 Ricercatore",  "conoscenza generale, cerca contesto e librerie"),
    "pianificatore": ("📐 Pianificatore", "ragionamento profondo, crea 2 piani architetturali"),
    "programmatore": ("💻 Programmatore", "specializzato nel codice Python, gira in parallelo"),
    "tester":        ("🧪 Tester",        "veloce, analizza output, può girare fino a 3 volte"),
    "ottimizzatore": ("✨ Ottimizzatore",  "refactoring, list comprehension, type hints"),
}


def trova_default(disponibili: list, preferenze: list) -> str:
    """Seleziona il modello più adatto tra quelli installati."""
    for pref in preferenze:
        for m in disponibili:
            if pref in m:
                return m
    return disponibili[0] if disponibili else "qwen3:4b"


# ── 3. Selezione modelli interattiva ─────────────────────────────────────────

def get_modelli_ollama(porta: int = PORTA_NVIDIA) -> list[dict]:
    """Interroga l'API Ollama e restituisce i modelli installati.

    Args:
        porta: Porta TCP dell'istanza Ollama da interrogare. Default 11434
            (istanza principale). Passare ``PORTA_INTEL`` o ``PORTA_AMD``
            per istanze secondarie su GPU aggiuntive.

    Returns:
        Lista di dizionari con formato ``{"nome": str, "size_gb": float}``.
        ``size_gb`` è 0.0 per modelli cloud (non scaricati in locale).
        Restituisce lista vuota se Ollama non risponde o si verifica un
        errore di connessione.
    """
    try:
        r = requests.get(f"http://localhost:{porta}/api/tags", timeout=5)
        return [
            {"nome": m["name"], "size_gb": round(m.get("size", 0) / 1e9, 1)}
            for m in r.json().get("models", [])
        ]
    except Exception:
        return []


def scegli_modelli(solo_locali: bool = True) -> tuple[dict, dict]:
    """Interfaccia interattiva per assegnare un modello Ollama a ogni ruolo.

    Mostra la lista dei modelli disponibili in colonna singola con dimensione,
    esegue una selezione automatica basata su ``PREFERENZE_MODELLI``, poi
    offre all'utente di sovrascrivere singoli ruoli digitando 1–5.
    Se vengono rilevate istanze Ollama extra (Intel/AMD su porte aggiuntive)
    propone anche la distribuzione multi-GPU.

    Args:
        solo_locali: Se ``True`` (default), esclude i modelli cloud (size=0)
            che non sono scaricati in locale. Utile per garantire privacy
            e funzionamento offline.

    Returns:
        Tupla ``(scelti, porte)`` dove:
        - ``scelti``: dizionario ``{ruolo: nome_modello}`` per i 5 ruoli.
        - ``porte``: dizionario ``{ruolo: numero_porta}`` che indica su quale
          istanza Ollama eseguire ciascun agente.
        In caso di Ollama non raggiungibile ritorna i modelli di fallback
        hardcodati tutti su ``PORTA_NVIDIA``.
    """
    tutti = get_modelli_ollama()

    FALLBACK = {
        "ricercatore":   "llama3.2:3b-instruct-q4_K_M",
        "pianificatore": "deepseek-r1:7b",
        "programmatore": "qwen2.5-coder:7b",
        "tester":        "gemma2:2b",
        "ottimizzatore": "qwen2.5-coder:7b",
    }
    _porte_default = {ruolo: PORTA_NVIDIA for ruolo in FALLBACK}

    if not tutti:
        print("\n  ⚠️  Ollama non raggiungibile.")
        if sys.platform == "win32":
            print("  Assicurati che Ollama sia avviato (cerca l'icona in basso a destra).")
        else:
            print("  Avvia Ollama con: ollama serve")
        print("  Uso modelli di default.")
        return FALLBACK, _porte_default

    if solo_locali:
        cloud        = [m for m in tutti if m["size_gb"] == 0]
        modelli_info = [m for m in tutti if m["size_gb"] > 0]
        if cloud:
            nomi_cloud = ", ".join(m["nome"] for m in cloud)
            print(f"\n  (Cloud esclusi in modalità offline: {nomi_cloud})")
    else:
        modelli_info = tutti

    if not modelli_info:
        print("  ⚠️  Nessun modello locale trovato. Uso modelli di default.")
        return FALLBACK, _porte_default

    nomi = [m["nome"] for m in modelli_info]

    # ── Lista modelli in colonna singola ──────────────────────────────────
    sep = "─" * 52
    print(f"\n  {sep}")
    print(f"  Modelli Ollama disponibili:")
    print(f"  {sep}")
    for i, m in enumerate(modelli_info, 1):
        gb_str = f"{m['size_gb']:.1f} GB" if m["size_gb"] > 0 else "cloud"
        print(f"  {i:2}.  {m['nome']:<36}  {gb_str}")
    print(f"  {sep}")

    # ── Selezione automatica per ruolo ────────────────────────────────────
    defaults = {
        ruolo: trova_default(nomi, PREFERENZE_MODELLI.get(ruolo, []))
        for ruolo in PREFERENZE_MODELLI
    }

    # ── Avviso Windows: GPU AMD/ATI lenta → consiglia CPU Xeon ───────────
    if sys.platform == "win32" and any(k in _GP_NOME.upper() for k in ("AMD", "RADEON", "ATI")):
        print("\n  ⚠️  GPU AMD/ATI rilevata su Windows — può essere lenta con Ollama.")
        print("  Se hai una CPU Xeon/i9 potente, apri PowerShell e digita:")
        print("       $env:OLLAMA_NUM_GPU=0 ; ollama serve    ← forza CPU")

    # ── Riepilogo selezione automatica ────────────────────────────────────
    _RUOLI_LISTA = ["ricercatore", "pianificatore", "programmatore", "tester", "ottimizzatore"]
    print(f"\n  Selezione automatica:")
    print(f"  {sep}")
    for idx_r, ruolo in enumerate(_RUOLI_LISTA, 1):
        icona, _ = _RUOLI_INFO[ruolo]
        mod = defaults[ruolo]
        tag = " ← coder ✓" if any(k in mod for k in ("coder", "code")) else ""
        print(f"  [{idx_r}]  {icona:<22}  {mod}{tag}")
    print(f"  {sep}")

    # ── Conferma unica o modifica per numero ─────────────────────────────
    scelti = dict(defaults)
    while True:
        print("\n  INVIO = conferma tutto   |   1–5 = cambia un agente")
        val = input("  ❯ ").strip()
        if not val:
            break
        if val.isdigit() and 1 <= int(val) <= len(_RUOLI_LISTA):
            ruolo_da_cambiare = _RUOLI_LISTA[int(val) - 1]
            icona_rc, _ = _RUOLI_INFO[ruolo_da_cambiare]
            print(f"\n  {icona_rc} — inserisci il numero del modello (1–{len(modelli_info)}):")
            try:
                n = int(input("  ❯ ").strip()) - 1
                if 0 <= n < len(nomi):
                    scelti[ruolo_da_cambiare] = nomi[n]
                    print(f"  ✓  {ruolo_da_cambiare} → {nomi[n]}")
                else:
                    print("  Numero fuori range, mantengo il default.")
            except (ValueError, EOFError):
                print("  Input non valido, mantengo il default.")
            print(f"\n  Selezione aggiornata:")
            for idx_r, ruolo in enumerate(_RUOLI_LISTA, 1):
                icona_u, _ = _RUOLI_INFO[ruolo]
                print(f"  [{idx_r}]  {icona_u:<22}  {scelti[ruolo]}")
        else:
            print("  ⚠  Digita INVIO per confermare, o un numero da 1 a 5 per cambiare agente.")
            print("  (Il testo del task lo inserisci nella schermata successiva)")

    # ── Configurazione Multi GPU (qualsiasi GPU extra con istanza Ollama attiva) ─
    porte = {ruolo: PORTA_NVIDIA for ruolo in scelti}
    gpu_attive = rileva_gpu_attive()   # es. {"Intel": {...}, "AMD": {...}}

    if gpu_attive:
        sep = "─" * 58
        nomi_extra = list(gpu_attive.keys())                  # ["Intel"] o ["AMD"] o entrambi
        prima_extra = nomi_extra[0]                           # GPU extra principale per l'auto
        prima_porta = gpu_attive[prima_extra]["porta"]

        print(f"\n  {sep}")
        n_gpu = 1 + len(gpu_attive)
        print(f"  🎮  Multi GPU rilevato! ({n_gpu} istanze Ollama attive)")
        print(f"  {sep}")
        print(f"  Porta {PORTA_NVIDIA:5} → {_GP_EMOJI} {_GP_NOME} (GPU principale)")
        for nome, info in gpu_attive.items():
            emoji = info["emoji"]
            lib   = info["lib"].upper()
            nomi_m = [m["nome"] for m in get_modelli_ollama(info["porta"])]
            modelli_str = ", ".join(nomi_m) if nomi_m else "⚠️  nessun modello trovato"
            print(f"  Porta {info['porta']:5} → {emoji} {nome} ({lib})  —  {modelli_str}")
        print()
        # Costruisce le etichette per l'opzione 3 (es. "N/I/A")
        lettere = {_GP_NOME: "N"} | {n: n[0] for n in gpu_attive}  # N, I, A
        lettere_str = "/".join(lettere.values())

        print(f"  [1] Auto: Ricercatore + Tester su {prima_extra}, resto su {_GP_NOME} (consigliato)")
        print(f"  [2] Tutto su {_GP_NOME} (porta {PORTA_NVIDIA})")
        print(f"  [3] Configura manualmente ogni ruolo")
        scelta_gpu = input("\n  Assegnazione GPU [1/2/3, Invio=1]: ").strip()

        if scelta_gpu == "2":
            pass  # tutto già su NVIDIA
        elif scelta_gpu == "3":
            righe_gpu = "  ".join(
                f"{n[0]}={n} (:{info['porta']})" for n, info in gpu_attive.items()
            )
            print(f"\n  N={_GP_NOME} (:{PORTA_NVIDIA})  {righe_gpu}")
            print(f"  Lettere: {lettere_str}  (Invio = N)")
            porta_per_lettera = {"N": PORTA_NVIDIA}
            porta_per_lettera |= {n[0]: info["porta"] for n, info in gpu_attive.items()}
            for ruolo in scelti:
                icona, _ = _RUOLI_INFO[ruolo]
                val = input(f"  {icona} → [{lettere_str}, Invio=N]: ").strip().upper()
                porte[ruolo] = porta_per_lettera.get(val[:1], PORTA_NVIDIA)
        else:  # opzione 1: agenti leggeri sulla prima GPU extra
            porte["ricercatore"] = prima_porta
            porte["tester"]      = prima_porta

        print("\n  Assegnazione finale GPU:")
        porta_a_nome = {PORTA_NVIDIA: _GP_NOME}
        porta_a_nome |= {info["porta"]: nome for nome, info in gpu_attive.items()}
        for ruolo, porta in porte.items():
            gpu_label = porta_a_nome.get(porta, f":{porta}")
            print(f"    {ruolo:15} → {gpu_label} (:{porta})")

    return scelti, porte


# ── Preferenze + switch dispositivo ──────────────────────────────────────────

def salva_preferenze(scelti: dict, dispositivo: str,
                     dispositivi_agenti: dict | None = None) -> None:
    """Salva modelli, dispositivo e assegnazione per-agente nella memoria JSON."""
    try:
        dati: dict = {}
        if os.path.exists(FILE_MEMORIA):
            with open(FILE_MEMORIA, encoding="utf-8") as f:
                dati = json.load(f)
        pref: dict = {"modelli": scelti, "dispositivo": dispositivo}
        if dispositivi_agenti:
            pref["dispositivi_agenti"] = dispositivi_agenti
        dati["preferenze"] = pref
        with open(FILE_MEMORIA, "w", encoding="utf-8") as f:
            json.dump(dati, f, indent=2, ensure_ascii=False)
    except Exception:
        pass


def _applica_dispositivo(dispositivo: str) -> None:
    """Riavvia Ollama impostando GPU o CPU tramite OLLAMA_NUM_GPU."""
    import shutil as _sh
    print(f"\n  🔄 Riavvio Ollama in modalità {dispositivo}...")
    if sys.platform == "win32":
        for proc in ("ollama.exe", "ollama app.exe"):
            subprocess.run(["taskkill", "/IM", proc, "/F"], capture_output=True)
    else:
        subprocess.run(["pkill", "-f", "ollama serve"], capture_output=True)
        subprocess.run(["pkill", "-x", "ollama"],       capture_output=True)
    time.sleep(2)
    ollama_bin = _sh.which("ollama")
    if not ollama_bin and sys.platform == "win32":
        for cand in [
            os.path.join(os.environ.get("LOCALAPPDATA", ""),
                         "Programs", "Ollama", "ollama.exe"),
            r"C:\Users\Utente\AppData\Local\Programs\Ollama\ollama.exe",
        ]:
            if os.path.isfile(cand):
                ollama_bin = cand; break
    if not ollama_bin:
        print("  ⚠️  Ollama non trovato nel PATH. Avvialo manualmente.")
        return
    env = os.environ.copy()
    if dispositivo == "CPU":
        env["OLLAMA_NUM_GPU"] = "0"
    else:
        env.pop("OLLAMA_NUM_GPU", None)
    flags = 0
    if sys.platform == "win32":
        flags = subprocess.DETACHED_PROCESS | subprocess.CREATE_NEW_PROCESS_GROUP
    subprocess.Popen([ollama_bin, "serve"], env=env,
                     stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                     creationflags=flags)
    for _ in range(20):
        time.sleep(1)
        try:
            if requests.get("http://localhost:11434/api/tags", timeout=1).status_code == 200:
                print(f"  ✓  Ollama avviato in modalità {dispositivo}.")
                return
        except Exception:
            pass
    print("  ⚠️  Ollama non risponde dopo il riavvio.")


def _avvia_ollama_cpu() -> bool:
    """
    Avvia (se non già attiva) una seconda istanza Ollama su PORTA_CPU (11435)
    in modalità CPU pura (OLLAMA_NUM_GPU=0).
    Restituisce True se il server è pronto, False in caso di errore.
    """
    import shutil as _sh
    # Già in ascolto?
    try:
        if requests.get(f"http://localhost:{PORTA_CPU}/api/tags", timeout=1).status_code == 200:
            return True
    except Exception:
        pass

    ollama_bin = _sh.which("ollama")
    if not ollama_bin and sys.platform == "win32":
        for cand in [
            os.path.join(os.environ.get("LOCALAPPDATA", ""),
                         "Programs", "Ollama", "ollama.exe"),
            r"C:\Users\Utente\AppData\Local\Programs\Ollama\ollama.exe",
        ]:
            if os.path.isfile(cand):
                ollama_bin = cand; break
    if not ollama_bin:
        print(f"  ⚠️  Ollama non trovato nel PATH — istanza CPU non avviata.")
        return False

    print(f"  🖥  Avvio seconda istanza Ollama (CPU) su porta {PORTA_CPU}...")
    env = os.environ.copy()
    env["OLLAMA_NUM_GPU"] = "0"
    env["OLLAMA_HOST"]    = f"127.0.0.1:{PORTA_CPU}"
    flags = 0
    if sys.platform == "win32":
        flags = subprocess.DETACHED_PROCESS | subprocess.CREATE_NEW_PROCESS_GROUP
    subprocess.Popen([ollama_bin, "serve"], env=env,
                     stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                     creationflags=flags)
    for _ in range(20):
        time.sleep(1)
        try:
            if requests.get(f"http://localhost:{PORTA_CPU}/api/tags", timeout=1).status_code == 200:
                print(f"  ✓  Ollama CPU pronto su :{PORTA_CPU}")
                return True
        except Exception:
            pass
    print(f"  ⚠️  Ollama CPU non risponde su :{PORTA_CPU}")
    return False


# ── 5. Memoria ────────────────────────────────────────────────────────────────

def carica_memoria() -> dict:
    if os.path.exists(FILE_MEMORIA):
        with open(FILE_MEMORIA, encoding="utf-8") as f:
            return json.load(f)
    return {"task_eseguiti": []}

def salva_memoria(memoria: dict, stato: "Stato") -> None:
    memoria["task_eseguiti"].append({
        "data":    datetime.now().isoformat(),
        "task":    stato["task"],
        "piano_a": stato.get("piano_a", ""),
        "verdict": stato.get("verdict", "SCONOSCIUTO"),
    })
    with open(FILE_MEMORIA, "w", encoding="utf-8") as f:
        json.dump(memoria, f, indent=2, ensure_ascii=False)
    if _RICH_OK and _rich:
        _rich.print(f"  [dim]📚 Memoria aggiornata: {FILE_MEMORIA}[/]")
    else:
        print(f"\n📚 Memoria aggiornata: {FILE_MEMORIA}")


# ── Database LLM consigliati ──────────────────────────────────────────────────

def carica_llm_db() -> dict:
    if os.path.exists(FILE_LLM_DB):
        with open(FILE_LLM_DB, encoding="utf-8") as f:
            return json.load(f)
    return {"llm_consigliati": [], "ultimo_aggiornamento": None}


def salva_llm_db(db: dict) -> None:
    with open(FILE_LLM_DB, "w", encoding="utf-8") as f:
        json.dump(db, f, indent=2, ensure_ascii=False)
    if _RICH_OK and _rich:
        _rich.print(f"  [dim]🤖 Database LLM aggiornato: {FILE_LLM_DB}[/]")
    else:
        print(f"\n🤖 Database LLM aggiornato: {FILE_LLM_DB}")


def ricerca_llm_migliori(modello_ricercatore: str) -> list:
    """Chiede all'LLM i modelli Ollama migliori per programmare in Python."""
    if _RICH_OK and _rich:
        _rich.print("\n  [cyan]🔍 Cerco i migliori LLM per la programmazione Python...[/]")
    else:
        print("\n  🔍 Cerco i migliori LLM per la programmazione Python...")
    try:
        llm = ChatOllama(model=modello_ricercatore)
        prompt = """Sei un esperto di modelli AI. Elenca i 10 migliori modelli LLM disponibili su Ollama
per programmare in Python (2024-2025). Considera: qwen2.5-coder, deepseek-coder, codellama,
mistral, gemma2, phi3, llama3, deepseek-r1, qwen3 ecc.

Per ogni modello fornisci:
- nome: nome esatto per 'ollama pull' (es. qwen2.5-coder:7b)
- specialita: cosa sa fare meglio in max 10 parole
- dimensione: RAM approssimativa richiesta (es. 4GB, 8GB, 16GB)
- voto: da 1 a 10 per la programmazione Python

Rispondi SOLO con un JSON array valido, senza testo aggiuntivo:
[{"nome": "...", "specialita": "...", "dimensione": "...", "voto": 0}]"""

        risposta = llm.invoke(prompt).content
        match = re.search(r'\[.*?\]', risposta, re.DOTALL)
        if match:
            return json.loads(match.group(0))
    except Exception as e:
        if _RICH_OK and _rich:
            _rich.print(f"  [yellow]⚠️  Errore ricerca LLM: {e}[/]")
        else:
            print(f"  ⚠️  Errore ricerca LLM: {e}")
    return []


def mostra_llm_consigliati(modelli: dict) -> None:
    """Mostra LLM dal database locale, cercandoli se non presenti."""
    db = carica_llm_db()
    ult_agg = db.get("ultimo_aggiornamento")

    if not db["llm_consigliati"]:
        db["llm_consigliati"] = ricerca_llm_migliori(modelli["ricercatore"])
        if db["llm_consigliati"]:
            db["ultimo_aggiornamento"] = datetime.now().isoformat()
            salva_llm_db(db)
    else:
        ult_str = ult_agg[:10] if ult_agg else "?"
        if _RICH_OK and _rich:
            _rich.print(f"\n  [dim]📋 LLM dal database locale (aggiornato: {ult_str})[/]")
        else:
            print(f"\n  📋 LLM dal database locale (aggiornato: {ult_str})")

    if not db["llm_consigliati"]:
        if _RICH_OK and _rich:
            _rich.print("  [yellow]⚠️  Nessun LLM trovato. Verifica che Ollama sia attivo.[/]")
        else:
            print("  ⚠️  Nessun LLM trovato. Verifica che Ollama sia attivo.")
        return

    if _RICH_OK and _rich:
        from rich.table import Table as _T
        tab = _T(title="[bold]🤖 Migliori LLM per Python[/]", show_lines=True,
                 border_style="dim blue")
        tab.add_column("#",      style="dim",    width=3)
        tab.add_column("Modello", style="cyan",   no_wrap=True)
        tab.add_column("Voto",   style="yellow",  width=8)
        tab.add_column("RAM",    style="green",   width=6)
        tab.add_column("Specialità", style="dim")
        for i, m in enumerate(db["llm_consigliati"][:10], 1):
            tab.add_row(str(i), m.get("nome", "?"),
                        f"⭐ {m.get('voto','?')}/10",
                        m.get("dimensione", "?"),
                        m.get("specialita", "?"))
        _rich.print(tab)
        _rich.print("  [dim]Per installare: ollama pull <nome-modello>[/]")
    else:
        print("\n  🤖 Migliori LLM per la programmazione Python:")
        print("  " + "─" * 60)
        for i, m in enumerate(db["llm_consigliati"][:10], 1):
            nome = m.get("nome", "?")
            spec = m.get("specialita", "?")
            dim  = m.get("dimensione", "?")
            voto = m.get("voto", "?")
            print(f"  {i:2}. {nome:<34} ⭐ {voto}/10  ({dim})")
            print(f"       → {spec}")
        print("  " + "─" * 60)
        print("  Per installare: ollama pull <nome-modello>")


# ── Stato condiviso ───────────────────────────────────────────────────────────

class Stato(TypedDict):
    task:               str
    modelli:            dict
    memoria:            dict
    contesto_ricerca:   str
    piano_a:            str
    piano_b:            str
    codice_a:           str
    codice_b:           str
    codice_corrente:    str
    output_esecuzione:  str
    report_tester:      str
    verdict:            str
    tentativi:          int
    codice_ottimizzato:   str
    note_ottimizzazione:  str
    porte:                dict   # {"ricercatore": 11434, "tester": 11435, ...}
    system_prompts:       dict   # {"ricercatore": "...", "programmatore": "..."}


# ── Utilità: esecuzione codice reale ─────────────────────────────────────────

def esegui_codice(codice: str) -> str:
    """Estrae il blocco Python (se in markdown) e lo esegue."""
    match = re.search(r"```(?:python)?\n(.*?)```", codice, re.DOTALL)
    codice_pulito = match.group(1) if match else codice

    with tempfile.NamedTemporaryFile(mode="w", suffix=".py", delete=False, encoding="utf-8") as f:
        f.write(codice_pulito)
        path = f.name

    try:
        proc = subprocess.run(
            ["python3", path],
            capture_output=True, text=True, timeout=15
        )
        if proc.stderr:
            return f"ERRORE:\n{proc.stderr.strip()}"
        return f"OUTPUT:\n{proc.stdout.strip()}" if proc.stdout.strip() else "Nessun output prodotto."
    except subprocess.TimeoutExpired:
        return "ERRORE: timeout 15s superato."
    finally:
        os.unlink(path)


# ── Utility: LLM invoke con system prompt opzionale ──────────────────────────

def _llm_invoke(llm, prompt: str, system_prompt: str = "") -> str:
    """Wrapper LangChain per invocare il modello con system prompt opzionale.

    Astraggono la differenza tra una chiamata con e senza system prompt:
    - Con system prompt: passa una lista ``[SystemMessage, HumanMessage]``
      che permette al modello di ricevere istruzioni di ruolo separate dal
      contenuto del task.
    - Senza system prompt: invoca direttamente con il prompt come stringa,
      utile per chiamate semplici dove non è necessario il contesto di ruolo.

    Args:
        llm: Istanza ``ChatOllama`` già configurata con modello e parametri.
        prompt: Testo del messaggio utente (il task o la query).
        system_prompt: Istruzioni di comportamento per il modello. Se vuoto
            viene ignorato e si usa la chiamata semplice.

    Returns:
        Testo della risposta del modello come stringa.
    """
    if system_prompt:
        return llm.invoke([
            SystemMessage(content=system_prompt),
            HumanMessage(content=prompt),
        ]).content
    return llm.invoke(prompt).content


# ── Upgrade modelli e system prompt specifici ─────────────────────────────────

_UPGRADE_PREFERENZE: dict[str, list] = {
    "ricercatore":   ["qwen3:14b", "llama3.3", "qwen3:8b", "llama3.2", "qwen3:4b", "qwen3", "mistral"],
    "pianificatore": ["deepseek-r1:14b", "deepseek-r1:8b", "deepseek-r1:7b", "qwen3:14b", "qwen3:8b", "qwen3"],
    "programmatore": ["qwen2.5-coder:14b", "qwen2.5-coder:7b", "deepseek-coder:33b", "deepseek-coder", "codellama:13b", "codellama"],
    "tester":        ["qwen3:8b", "qwen3:4b", "gemma2:9b", "gemma2:2b", "llama3.2"],
    "ottimizzatore": ["qwen2.5-coder:14b", "qwen2.5-coder:7b", "deepseek-coder:33b", "deepseek-coder", "codellama"],
}


def _upgrade_modelli(modelli: dict, nomi_disponibili: list[str]) -> dict:
    """
    Seleziona un modello diverso/più potente per ogni ruolo.
    Preferisce un modello diverso dall'attuale; se non possibile, mantiene l'attuale.
    """
    nuovi: dict[str, str] = {}
    for ruolo, attuale in modelli.items():
        preferenze = _UPGRADE_PREFERENZE.get(ruolo, [])
        candidato = attuale
        for pref in preferenze:
            for nome in nomi_disponibili:
                if pref in nome and nome != attuale:
                    candidato = nome
                    break
            if candidato != attuale:
                break
        # se non trovato diverso, prova comunque il default di preferenza
        if candidato == attuale:
            candidato = trova_default(nomi_disponibili, preferenze)
        nuovi[ruolo] = candidato
    return nuovi


def _genera_system_prompts(task: str, report_tester: str) -> dict[str, str]:
    """Genera system prompt adattivi per ogni ruolo in base al task e agli errori.

    Viene chiamata solo quando la pipeline entra nel loop di auto-retry
    (tentativo 2 o 3): in questi casi aggiunge contesto specifico sull'errore
    precedente e specializzazioni per tipo di problema (matematica, file,
    stringhe, web). I prompt risultanti vengono inseriti nello stato come
    ``system_prompts`` e vengono passati a ``_llm_invoke`` insieme al prompt
    principale di ogni agente.

    Logica di rilevamento tipo problema: analisi lessicale semplice del testo
    del task (parole chiave come "calcola", "file", "regex", "http") che
    attiva appendici specializzate al prompt base dell'agente.

    Args:
        task: Testo del task originale da cui rilevare il tipo di problema.
        report_tester: Output del tester dell'ultimo tentativo fallito.
            Viene usato per estrarre il messaggio di errore da includere
            nel prompt del ricercatore e del programmatore.

    Returns:
        Dizionario ``{ruolo: system_prompt}`` con un prompt specifico per
        ciascuno dei 5 ruoli (ricercatore, pianificatore, programmatore,
        tester, ottimizzatore). Tutti i prompt includono il contesto
        ``"Stai elaborando un task che ha già fallito i test precedenti"``
        per orientare il modello verso un approccio correttivo.
    """
    tl = task.lower()
    errore = ""
    if report_tester:
        m = re.search(r"(?:ERRORE|Error|Traceback)[^\n]*", report_tester, re.IGNORECASE)
        errore = m.group(0).strip() if m else ""

    # Rilevamento tipo problema
    is_math   = any(k in tl for k in ("calcola", "numero", "conteggio", "somma", "media",
                                       "statistica", "matematica", "algoritmo", "range",
                                       "divisione", "moltiplicazione"))
    is_file   = any(k in tl for k in ("file", "leggi", "scrivi", "csv", "json",
                                       "cartella", "directory", "percorso"))
    is_string = any(k in tl for k in ("stringa", "testo", "parola", "carattere",
                                       "regex", "pattern", "maiuscol", "minuscol"))
    is_web    = any(k in tl for k in ("web", "http", "api", "requests", "url", "download"))

    base = (
        "Stai elaborando un task che ha già fallito i test precedenti. "
        "Devi essere extra preciso e correggere i problemi precedenti."
    )
    errore_ctx = f" Errore precedente da correggere: {errore}" if errore else ""

    sp: dict[str, str] = {
        "ricercatore": (
            f"{base}{errore_ctx} "
            "Analizza attentamente cosa ha causato il fallimento e suggerisci "
            "l'approccio tecnico CORRETTO e DIVERSO da quello già tentato."
        ),
        "pianificatore": (
            f"{base} "
            "Proponi approcci COMPLETAMENTE ALTERNATIVI a quelli già tentati. "
            "Non ripetere la stessa struttura logica. Pensa out-of-the-box."
        ),
        "programmatore": (
            f"{base}{errore_ctx} "
            "Prima di scrivere il codice, verifica mentalmente ogni riga. "
            "Scrivi codice che funzioni al primo tentativo. Niente scorciatoie."
        ),
        "tester": (
            f"{base} "
            f"Il task richiede: {task[:120]}. "
            "Sii rigoroso: PASSATO solo se l'output corrisponde esattamente alle aspettative."
        ),
        "ottimizzatore": (
            f"{base} "
            "Mantieni la logica CORRETTA. Priorità: "
            "1) built-in C (sum/min/max/map/filter/any/all) — più veloci di qualsiasi loop; "
            "2) generator expression — evita liste inutili in RAM; "
            "3) set() per ricerche O(1); "
            "4) @lru_cache per funzioni pure chiamate più volte; "
            "5) lambda + list comprehension dove le built-in non bastano."
        ),
    }

    # Specializzazioni per tipo problema
    if is_math:
        sp["ricercatore"]   += " SPECIALITÀ MATH: gestisci range() con step negativo, valori limite, edge cases con negativi e zero."
        sp["programmatore"] += " SPECIALITÀ MATH: usa range(start, stop, step) con step negativo per contare verso il basso."
    elif is_file:
        sp["ricercatore"]   += " SPECIALITÀ FILE: controlla encoding UTF-8, usa context manager, gestisci FileNotFoundError."
        sp["programmatore"] += " SPECIALITÀ FILE: usa 'with open(...) as f:', gestisci eccezioni, controlla i percorsi."
    elif is_string:
        sp["ricercatore"]   += " SPECIALITÀ STRINGHE: considera encoding, slicing, metodi str standard."
        sp["programmatore"] += " SPECIALITÀ STRINGHE: attento a immutabilità stringhe, usa join() per concatenare molti pezzi."
    elif is_web:
        sp["ricercatore"]   += " SPECIALITÀ WEB: gestisci timeout, codici HTTP errore, encoding della risposta."
        sp["programmatore"] += " SPECIALITÀ WEB: usa requests con timeout=10, controlla r.status_code, usa r.raise_for_status()."

    return sp


# ── Helper Rich per agenti ───────────────────────────────────────────────────

def _agente_header(titolo: str, porta: int) -> None:
    """Stampa header agente: Rule colorata con titolo e GPU."""
    if _RICH_OK and _rich:
        from rich.rule import Rule
        _rich.print("")
        _rich.print(Rule(f"[bold]{titolo}[/] — [dim]{_gpu_label(porta)}[/]", style="blue"))
        _rich.print(f"  [dim]{_gpu_stats(porta)}[/]")
    else:
        print(f"\n{'─'*60}", flush=True)
        print(f"  {titolo}  —  {_gpu_label(porta)}", flush=True)
        print(f"  {_gpu_stats(porta)}", flush=True)


def _agente_sezione(label: str, contenuto: str) -> None:
    """Stampa sezione output agente in un Panel Rich o plain print."""
    if _RICH_OK and _rich:
        from rich.panel import Panel as _P
        _rich.print(_P(contenuto.strip(), title=f"[bold]{label}[/]",
                       border_style="dim blue", padding=(0, 1)))
    else:
        print(f"\n{label}:\n{contenuto}", flush=True)


def _agente_info(msg: str, style: str = "dim") -> None:
    """Stampa messaggio informativo di un agente."""
    if _RICH_OK and _rich:
        _rich.print(f"  [{style}]{msg}[/{style}]")
    else:
        print(f"  {msg}", flush=True)


def _agente_tempo(nome: str, secondi: float) -> None:
    """Stampa tempo di esecuzione di un agente."""
    if _RICH_OK and _rich:
        _rich.print(f"  [dim]⏱  {nome}: {secondi:.1f}s[/]")
    else:
        print(f"  ⏱  {nome}: {secondi:.1f}s", flush=True)


# ── 7. Agente RICERCATORE ────────────────────────────────────────────────────

def agente_ricercatore(stato: Stato) -> dict:
    """Primo agente della pipeline: raccoglie contesto tecnico e librerie.

    Analizza il task e produce un contesto che guida gli agenti successivi.
    Il ricercatore:
    - Elenca i file Python già presenti nella cartella output (per contesto)
    - Cerca nella memoria i task simili usando matching su parole intere
      (regex ``\b<parola>\b``) escludendo stop words italiane: questo evita
      falsi positivi come "la" dentro "tabella"
    - Chiede al modello di analizzare il task in 4-5 righe: cosa fa, librerie,
      approccio tecnico, insidie

    Args:
        stato: Dizionario di stato LangGraph contenente almeno ``task``,
            ``modelli``, ``porte``, ``memoria`` e opzionalmente
            ``system_prompts["ricercatore"]``.

    Returns:
        Dizionario con chiave ``contesto_ricerca`` (stringa) da aggiungere
        allo stato condiviso per i nodi successivi.
    """
    _t0 = time.time()
    porta = stato["porte"]["ricercatore"]
    _agente_header("🔎  RICERCATORE", porta)
    _agente_info("Raccogliendo contesto e librerie utili...")

    # File .py nella cartella corrente
    file_py = [f for f in os.listdir(CARTELLA_OUTPUT) if f.endswith(".py")]
    lista_file = ", ".join(file_py) if file_py else "nessuno"

    # Task simili dalla memoria — matching su parole chiave (no stop words, no sottostringhe)
    _STOP = {"da", "a", "il", "la", "lo", "le", "gli", "di", "in", "con", "per",
             "su", "tra", "fra", "e", "o", "ma", "che", "se", "non", "è", "mi",
             "ti", "si", "ci", "vi", "un", "una", "uno", "i", "al", "del",
             "della", "dei", "delle", "degli", "ho", "ha", "hanno"}
    parole_chiave = [
        p for p in stato["task"].lower().split()
        if len(p) > 3 and p not in _STOP and not p.isdigit()
    ]
    task_simili = []
    for t in stato["memoria"].get("task_eseguiti", []):
        testo_prev = t["task"].lower()
        # match solo su parole intere (non sottostringhe)
        if parole_chiave and any(
            re.search(r'\b' + re.escape(p) + r'\b', testo_prev)
            for p in parole_chiave
        ):
            task_simili.append(f"  • {t['task']} → {t['verdict']}")
    contesto_memoria = "\n".join(task_simili) if task_simili else "  nessuno"

    llm = ChatOllama(model=stato["modelli"]["ricercatore"],
                     base_url=f"http://localhost:{stato['porte']['ricercatore']}",
                     num_predict=600, temperature=0.4, num_ctx=4096)
    prompt = f"""Sei un ricercatore Python esperto. Il tuo unico compito è analizzare il TASK qui sotto.

TASK: {stato['task']}

IMPORTANTE: rispondi ESCLUSIVAMENTE per questo task. Non farti influenzare dalla memoria.
La memoria è solo un riferimento tecnico, non descrive il task corrente.

Fornisci in 4-5 righe:
1. Cosa deve fare esattamente il programma (interpreta letteralmente il task)
2. Librerie Python necessarie (usa solo stdlib se basta)
3. Approccio tecnico più semplice e diretto
4. Insidie da evitare

File Python già presenti: {lista_file}
Task simili in memoria (solo riferimento tecnico, NON il task corrente):\n{contesto_memoria}"""

    _agente_info("Interrogo il modello...")
    _sp = stato.get("system_prompts", {}).get("ricercatore", "")
    contesto = _llm_invoke(llm, prompt, _sp)
    _agente_sezione("🔍 CONTESTO", contesto)
    _agente_tempo("Ricercatore", time.time() - _t0)
    return {"contesto_ricerca": contesto}


# ── 8. Agente PIANIFICATORE (2 piani) ────────────────────────────────────────

def agente_pianificatore(stato: Stato) -> dict:
    """Secondo agente: genera due approcci architetturali alternativi al task.

    Produce due piani distinti (APPROCCIO A e APPROCCIO B) che verranno
    assegnati a due programmatori paralleli. La diversità degli approcci
    aumenta la probabilità che almeno uno produca codice corretto al primo
    tentativo. Usa un modello con capacità di ragionamento più profondo
    (es. deepseek-r1) per garantire coerenza logica nei piani.

    Il parsing della risposta usa ``re.split(r"APPROCCIO\\s+B", ...)`` per
    separare i due piani: se il modello non rispetta il formato, tutto il
    testo viene assegnato a piano_a e piano_b viene lasciato uguale a testo.

    Args:
        stato: Stato corrente con ``task``, ``contesto_ricerca``,
            ``modelli``, ``porte``.

    Returns:
        Dizionario con ``piano_a`` e ``piano_b`` (stringhe di testo).
    """
    _t0 = time.time()
    porta = stato["porte"]["pianificatore"]
    _agente_header("📋  PIANIFICATORE", porta)
    _agente_info("Creo 2 approcci alternativi...")

    llm = ChatOllama(model=stato["modelli"]["pianificatore"],
                     base_url=f"http://localhost:{stato['porte']['pianificatore']}",
                     num_predict=500, temperature=0.5, num_ctx=3072)
    prompt = f"""Sei un pianificatore Python. Crea DUE approcci alternativi per questo programma.
Rispondi in italiano.

TASK: {stato['task']}
CONTESTO: {stato['contesto_ricerca']}

Formato:
APPROCCIO A: <nome>
1. ...
2. ...

APPROCCIO B: <nome diverso da A>
1. ...
2. ...

Max 4 passi per approccio. Niente codice."""

    _sp = stato.get("system_prompts", {}).get("pianificatore", "")
    testo = _llm_invoke(llm, prompt, _sp)
    parti = re.split(r"APPROCCIO\s+B", testo, flags=re.IGNORECASE)
    piano_a = parti[0].strip()
    piano_b = ("APPROCCIO B" + parti[1]).strip() if len(parti) > 1 else testo

    _agente_sezione("📋 PIANO A", piano_a)
    _agente_sezione("📋 PIANO B", piano_b)
    _agente_tempo("Pianificatore", time.time() - _t0)
    return {"piano_a": piano_a, "piano_b": piano_b}


# ── 8. Programmatori in parallelo ────────────────────────────────────────────

def agente_programmatori_parallelo(stato: Stato) -> dict:
    """Terzo agente (doppio, parallelo): implementa i due piani in codice Python.

    Lancia due thread simultanei, uno per piano A e uno per piano B, ognuno
    con la propria istanza ChatOllama. Questo dimezza il tempo di attesa
    rispetto all'esecuzione sequenziale. In configurazione multi-GPU i due
    programmatori usano porte diverse (``programmatore_a`` e
    ``programmatore_b``) per sfruttare GPU fisicamente distinte.

    Vincolo ai modelli: usa temperature=0.15 (creatività molto bassa) per
    massimizzare la correttezza sintattica e logica del codice generato.
    I programmatori usano preferenzialmente modelli coder (qwen2.5-coder,
    deepseek-coder, codellama).

    Args:
        stato: Stato corrente con ``piano_a``, ``piano_b``, ``task``,
            ``contesto_ricerca``, ``modelli``, ``porte``.

    Returns:
        Dizionario con ``codice_a`` e ``codice_b`` (stringhe con il codice
        Python, possibilmente incapsulato in blocchi markdown ```python).
    """
    _t0 = time.time()
    porta = stato["porte"]["programmatore"]
    _agente_header("💻  PROGRAMMATORI", porta)
    _agente_info("Scrivo 2 versioni in parallelo...")

    risultato = {}
    porta_a = stato["porte"].get("programmatore_a", stato["porte"]["programmatore"])
    porta_b = stato["porte"].get("programmatore_b", stato["porte"]["programmatore"])

    def scrivi(piano: str, label: str, porta: int) -> None:
        llm = ChatOllama(model=stato["modelli"]["programmatore"],
                         base_url=f"http://localhost:{porta}",
                         num_predict=1200, temperature=0.15, num_ctx=4096)
        prompt = f"""Sei un programmatore Python esperto.
Scrivi codice Python seguendo questo piano:

{piano}

TASK: {stato['task']}
CONTESTO: {stato['contesto_ricerca']}

IMPORTANTE: NON usare input(). Usa valori di esempio hardcodati.
Scrivi SOLO il codice Python con commenti brevi."""
        _sp = stato.get("system_prompts", {}).get("programmatore", "")
        risultato[label] = _llm_invoke(llm, prompt, _sp)
        _agente_info(f"💻 CODICE {label} pronto.", style="green")

    t_a = threading.Thread(target=scrivi, args=(stato["piano_a"], "A", porta_a))
    t_b = threading.Thread(target=scrivi, args=(stato["piano_b"], "B", porta_b))
    t_a.start(); t_b.start()
    t_a.join();  t_b.join()

    _agente_sezione("💻 CODICE A", risultato.get("A", ""))
    _agente_sezione("💻 CODICE B", risultato.get("B", ""))
    _agente_tempo("Programmatori", time.time() - _t0)
    return {"codice_a": risultato.get("A", ""), "codice_b": risultato.get("B", "")}


# ── Agente TESTER ─────────────────────────────────────────────────────────────

def agente_tester(stato: Stato) -> dict:
    """Quarto agente (con loop): esegue il codice e valuta il risultato.

    Ha due modalità operative:
    - **Primo tentativo** (tentativi==1): esegue entrambi i codici A e B
      tramite ``esegui_codice()``, sceglie quello senza errori (preferisce A),
      poi chiede al modello LLM di analizzare l'output e emettere un verdict.
    - **Tentativi successivi**: esegue solo ``codice_corrente`` (già corretto
      dall'agente correttore) e chiede al modello di rivalutare.

    La stringa ``"PASSATO"`` nel report del modello determina il verdict.
    Il grafo LangGraph usa ``decide_dopo_tester`` per decidere se procedere
    all'ottimizzatore o tornare al correttore (fino a ``MAX_TENTATIVI=3``).

    Args:
        stato: Stato corrente con ``codice_a``, ``codice_b``,
            ``codice_corrente``, ``tentativi``, ``task``, ``modelli``, ``porte``.

    Returns:
        Dizionario con ``codice_corrente``, ``output_esecuzione``,
        ``report_tester``, ``verdict`` ("PASSATO" o "FALLITO"),
        ``tentativi`` (incrementato).
    """
    _t0 = time.time()
    tentativi = stato.get("tentativi", 0) + 1
    porta = stato["porte"]["tester"]
    _agente_header(f"🧪  TESTER  (tentativo {tentativi}/{MAX_TENTATIVI})", porta)

    # Prima esecuzione: confronta A e B e sceglie il migliore
    if tentativi == 1:
        _agente_info("Eseguo entrambi i codici...")
        out_a = esegui_codice(stato["codice_a"])
        out_b = esegui_codice(stato["codice_b"])
        _agente_sezione("⚙️  OUTPUT A", out_a)
        _agente_sezione("⚙️  OUTPUT B", out_b)

        if "ERRORE" not in out_a:
            codice_scelto, output = stato["codice_a"], out_a
            _agente_info("✓ Scelto: APPROCCIO A", style="green")
        elif "ERRORE" not in out_b:
            codice_scelto, output = stato["codice_b"], out_b
            _agente_info("✓ Scelto: APPROCCIO B", style="green")
        else:
            codice_scelto, output = stato["codice_a"], out_a
            _agente_info("⚠️  Entrambi con errori. Tenterò di correggere A.", style="yellow")
    else:
        # Tentativi successivi: testa il codice corretto
        _agente_info("Eseguo codice corretto...")
        codice_scelto = stato["codice_corrente"]
        output = esegui_codice(codice_scelto)
        _agente_sezione("⚙️  OUTPUT", output)

    llm = ChatOllama(model=stato["modelli"]["tester"],
                     base_url=f"http://localhost:{stato['porte']['tester']}",
                     num_predict=300, temperature=0.2, num_ctx=3072)
    prompt = f"""Sei un tester Python. Analizza in 3 righe:

TASK: {stato['task']}
OUTPUT: {output}

Ultima riga DEVE essere esattamente:
VERDICT: ✅ PASSATO   oppure   VERDICT: ❌ FALLITO"""

    _agente_info("Analizzo output...")
    _sp = stato.get("system_prompts", {}).get("tester", "")
    report = _llm_invoke(llm, prompt, _sp)
    verdict = "PASSATO" if "PASSATO" in report else "FALLITO"
    _agente_sezione("🧪 REPORT TESTER", report)
    _agente_tempo("Tester", time.time() - _t0)

    return {
        "codice_corrente":   codice_scelto,
        "output_esecuzione": output,
        "report_tester":     report,
        "verdict":           verdict,
        "tentativi":         tentativi,
    }


# ── 1. Agente CORRETTORE (loop) ───────────────────────────────────────────────

def agente_correttore(stato: Stato) -> dict:
    """Agente di correzione: interpreta l'errore del tester e riscrive il codice.

    Viene attivato dal grafo solo quando il tester emette ``FALLITO`` e il
    numero di tentativi è inferiore a ``MAX_TENTATIVI``. Usa lo stesso modello
    del programmatore (specializzato nel codice) e riceve il messaggio di
    errore completo insieme al codice difettoso, chiedendo al modello di
    produrre SOLO il codice corretto senza spiegazioni.

    Il nodo "correttore" nel grafo ha un arco diretto verso "tester", creando
    il loop di correzione: correttore → tester → (se PASSATO) ottimizzatore
                                                → (se FALLITO) correttore...

    Args:
        stato: Stato con ``output_esecuzione`` (errore), ``codice_corrente``
            (codice da correggere), ``task``, ``tentativi``, ``modelli``,
            ``porte``.

    Returns:
        Dizionario con ``codice_corrente`` aggiornato con il codice corretto.
    """
    _t0 = time.time()
    porta = stato["porte"]["programmatore"]
    _agente_header(f"🔧  CORRETTORE  (tentativo {stato['tentativi']}/{MAX_TENTATIVI})", porta)
    _agente_info("Correggo gli errori trovati dal tester...")

    llm = ChatOllama(model=stato["modelli"]["programmatore"],
                     base_url=f"http://localhost:{stato['porte']['programmatore']}",
                     num_predict=1200, temperature=0.15, num_ctx=4096)
    prompt = f"""Sei un programmatore Python. Correggi questo codice che ha prodotto un errore.

ERRORE:
{stato['output_esecuzione']}

CODICE:
{stato['codice_corrente']}

TASK: {stato['task']}

Scrivi SOLO il codice corretto, nessuna spiegazione."""

    _sp = stato.get("system_prompts", {}).get("programmatore", "")
    codice_corretto = _llm_invoke(llm, prompt, _sp)
    _agente_sezione("🔧 CODICE CORRETTO", codice_corretto)
    _agente_tempo("Correttore", time.time() - _t0)
    return {"codice_corrente": codice_corretto}


# ── 4. Agente OTTIMIZZATORE ───────────────────────────────────────────────────

def agente_ottimizzatore(stato: Stato) -> dict:
    _t0 = time.time()
    porta = stato["porte"]["ottimizzatore"]
    _agente_header("✨  OTTIMIZZATORE", porta)
    _agente_info("Rendo il codice più pythonic...")

    llm = ChatOllama(model=stato["modelli"]["ottimizzatore"],
                     base_url=f"http://localhost:{stato['porte']['ottimizzatore']}",
                     num_predict=1200, temperature=0.15, num_ctx=4096)
    prompt = f"""Sei un esperto Python senior. Ottimizza questo codice applicando le regole nell'ordine esatto indicato.

PRINCIPIO FONDAMENTALE: le funzioni built-in map(), filter(), sum(), min(), max(), any(), all()
sono implementate in C dentro Python e girano MOLTO più veloci di un loop Python puro.
Dai SEMPRE la precedenza a queste funzioni rispetto a loop o list comprehension equivalenti.

━━━ REGOLE IN ORDINE DI PRIORITÀ ━━━

1. FUNZIONI BUILT-IN C (MASSIMA PRIORITÀ) — più veloci di qualsiasi loop Python:
   • sum()  → sostituisce: totale = 0; for x in items: totale += x
               DOPO:  totale = sum(items)
               Con trasformazione: sum(x*2 for x in items)
   • min() / max() → sostituisce loop di confronto manuale
               DOPO:  minimo = min(items)   massimo = max(items)
               Con chiave: min(items, key=lambda x: x["voto"])
   • map()  → sostituisce: for x in items: risultato.append(f(x))
               DOPO:  risultato = list(map(f, items))
               Con lambda: list(map(lambda x: x*2, items))
   • filter() → sostituisce: for x in items: if cond: risultato.append(x)
               DOPO:  risultato = list(filter(lambda x: x > 0, items))
   • any() / all() → sostituisce loop con flag booleano
               DOPO:  esiste = any(x > 0 for x in items)
                       tutti  = all(x > 0 for x in items)

2. LAMBDA — funzioni "usa e getta" da usare INSIEME alle built-in C:
   • Con map:    list(map(lambda x: x.upper(), parole))
   • Con filter: list(filter(lambda x: len(x) > 3, parole))
   • Con sort:   items.sort(key=lambda x: x["nome"])
   • Con min/max: max(items, key=lambda x: x["punteggio"])
   Evita di definire una funzione separata se viene usata una sola volta.

3. LIST COMPREHENSION — usa solo quando map()/filter() non bastano:
   • Con doppia logica: [f(x) if cond else g(x) for x in items]
   • Con due loop:      [x+y for x in a for y in b]
   • Regola: se puoi usare map() o filter(), preferiscili (sono in C).

4. OPERATORE TERNARIO — sostituisce if-else per assegnazione singola:
     PRIMA:  if ok: stato = "attivo"
             else:  stato = "inattivo"
     DOPO:   stato = "attivo" if ok else "inattivo"

5. GENERATOR EXPRESSIONS — se costruisci una sequenza solo per iterarla (es. passarla a sum/min/max),
   usa una generator expression invece di una list per non caricare tutto in RAM:
     DOPO:  totale = sum(x*2 for x in items)   ← niente lista intermedia in memoria

6. set() PER RICERCHE — se fai `if x in lista` dentro un loop, converti la lista in set():
   I set usano tabelle hash → ricerca O(1) invece di O(n):
     PRIMA:  validi = ["a","b","c"]; if x in validi: ...
     DOPO:   validi = {"a","b","c"}; if x in validi: ...   # O(1)

7. @lru_cache — se una funzione pura viene chiamata più volte con gli stessi argomenti:
     from functools import lru_cache
     @lru_cache(maxsize=None)
     def calcola(n): ...

8. TYPE HINTS — aggiungi annotazioni alle funzioni se mancano.
9. NOMI VARIABILI — migliora nomi poco chiari (x, tmp, l, ecc.).
10. NON CAMBIARE LA LOGICA — il comportamento deve rimanere identico.

CODICE:
{stato['codice_corrente']}

Scrivi SOLO il codice ottimizzato. Aggiungi commenti brevi dove trasformi il codice:
  # C built-in     → map/filter/sum/min/max/any/all
  # generator      → niente lista in RAM
  # O(1) set       → ricerca veloce
  # lru_cache      → calcoli memorizzati"""

    _sp = stato.get("system_prompts", {}).get("ottimizzatore", "")
    codice_ottimizzato = _llm_invoke(llm, prompt, _sp)
    _agente_sezione("✨ CODICE OTTIMIZZATO", codice_ottimizzato)

    _durata_ms = round((time.time() - _t0) * 1000)

    # Calcola riduzione righe (escluse righe vuote e commenti)
    def _righe_utili(s: str) -> int:
        return sum(1 for r in s.splitlines()
                   if r.strip() and not r.strip().startswith("#"))
    r_prima = _righe_utili(stato["codice_corrente"])
    r_dopo  = _righe_utili(codice_ottimizzato)

    # Calcola caratteri risparmiati
    c_prima = len(stato["codice_corrente"])
    c_dopo  = len(codice_ottimizzato)
    c_delta = c_prima - c_dopo

    if r_prima > 0:
        delta = (r_prima - r_dopo) / r_prima * 100
        if delta > 0:
            rigo_str  = f"righe {r_prima}→{r_dopo} (-{delta:.0f}%)"
            rigo_nota = f"ridotto del {delta:.0f}% ({r_prima}→{r_dopo} righe)"
        elif delta < 0:
            rigo_str  = f"righe {r_prima}→{r_dopo} (+{abs(delta):.0f}%)"
            rigo_nota = f"espanso del {abs(delta):.0f}% ({r_prima}→{r_dopo} righe)"
        else:
            rigo_str  = f"righe invariate ({r_dopo})"
            rigo_nota = f"stesse righe ({r_dopo}), codice rifattorizzato"
    else:
        rigo_str = rigo_nota = "n/d"

    char_str = (f"-{c_delta} caratteri" if c_delta > 0
                else f"+{abs(c_delta)} caratteri" if c_delta < 0
                else "caratteri invariati")

    note_ott = f"{rigo_nota} · {_durata_ms} ms"

    if _RICH_OK and _rich:
        from rich.table import Table as _TottTab
        _tott = _TottTab(show_header=False, box=None, padding=(0, 3))
        _tott.add_column(style="bold magenta", no_wrap=True)
        _tott.add_column(style="cyan")
        _tott.add_row("⏱  Tempo ottimizzazione", f"[bold]{_durata_ms} ms[/]")
        _tott.add_row("📏 Righe di codice",       rigo_str)
        _tott.add_row("✂️  Caratteri",             char_str)
        from rich.panel import Panel as _PottPan
        _rich.print(_PottPan(_tott,
                             title="[bold magenta]✨ Ottimizzazione completata[/]",
                             border_style="magenta", padding=(0, 1)))
    else:
        print(f"  ✨ Ottimizzazione: {note_ott}", flush=True)
        print(f"  ⏱  Ottimizzatore: {_durata_ms} ms", flush=True)

    return {"codice_ottimizzato": codice_ottimizzato, "note_ottimizzazione": note_ott}


# ── 1. Decisione dopo il Tester ──────────────────────────────────────────────

def decide_dopo_tester(stato: Stato) -> str:
    if stato["verdict"] == "PASSATO":
        return "ottimizzatore"
    if stato.get("tentativi", 0) >= MAX_TENTATIVI:
        _agente_info(f"⚠️  Raggiunti {MAX_TENTATIVI} tentativi. Procedo con il codice attuale.", style="yellow")
        return "ottimizzatore"
    return "correttore"


# ── 2 + 6. Salvataggio file .py e report HTML ────────────────────────────────

def salva_risultati(stato: Stato) -> tuple[str, str]:
    nome_base = re.sub(r"[^a-z0-9]+", "_", stato["task"].lower())[:35]
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")

    # Estrai codice pulito
    codice_raw = stato.get("codice_ottimizzato") or stato.get("codice_corrente", "")
    match = re.search(r"```(?:python)?\n(.*?)```", codice_raw, re.DOTALL)
    codice_pulito = match.group(1).strip() if match else codice_raw.strip()

    # 2. Salva .py
    path_py = os.path.join(CARTELLA_OUTPUT, f"output_{nome_base}_{timestamp}.py")
    with open(path_py, "w", encoding="utf-8") as f:
        f.write(f"# Task: {stato['task']}\n")
        f.write(f"# Generato il: {datetime.now().strftime('%Y-%m-%d %H:%M')}\n\n")
        f.write(codice_pulito)
    if _RICH_OK and _rich:
        _rich.print(f"\n  [bold green]💾 Codice salvato:[/] [dim]{path_py}[/]")
    else:
        print(f"\n💾 Codice salvato: {path_py}")

    # 6. Salva HTML
    path_html = os.path.join(CARTELLA_OUTPUT, f"report_{nome_base}_{timestamp}.html")
    verdict_str = "✅ PASSATO" if stato.get("verdict") == "PASSATO" else "❌ FALLITO"
    verdict_cls = "passato" if stato.get("verdict") == "PASSATO" else "fallito"

    def esc(s: str) -> str:
        return str(s).replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")

    html = f"""<!DOCTYPE html>
<html lang="it">
<head>
<meta charset="UTF-8">
<title>Report Multi-Agente — {esc(stato['task'][:50])}</title>
<style>
  body{{font-family:monospace;max-width:960px;margin:40px auto;padding:20px;background:#1e1e1e;color:#d4d4d4}}
  h1{{color:#569cd6}}
  h2{{color:#4ec9b0;border-bottom:1px solid #333;padding-bottom:4px;margin-top:30px}}
  pre{{background:#252526;padding:14px;border-radius:6px;overflow-x:auto;white-space:pre-wrap;font-size:0.9em}}
  .passato{{color:#4caf50;font-size:1.4em;font-weight:bold}}
  .fallito{{color:#f44336;font-size:1.4em;font-weight:bold}}
  .meta{{color:#858585;font-size:0.85em}}
  .grid{{display:grid;grid-template-columns:1fr 1fr;gap:16px}}
</style>
</head>
<body>
<h1>Report Multi-Agente</h1>
<p class="meta">Generato il {datetime.now().strftime('%Y-%m-%d %H:%M:%S')} &nbsp;|&nbsp;
Tentativi: {stato.get('tentativi', 0)}/{MAX_TENTATIVI}</p>

<h2>Task</h2>
<pre>{esc(stato['task'])}</pre>

<h2>Ricerca</h2>
<pre>{esc(stato.get('contesto_ricerca', ''))}</pre>

<h2>Piani alternativi</h2>
<div class="grid">
<div><h2 style="color:#ce9178">Piano A</h2><pre>{esc(stato.get('piano_a', ''))}</pre></div>
<div><h2 style="color:#ce9178">Piano B</h2><pre>{esc(stato.get('piano_b', ''))}</pre></div>
</div>

<h2>Codice Ottimizzato</h2>
<pre>{esc(codice_pulito)}</pre>

<h2>Output Esecuzione</h2>
<pre>{esc(stato.get('output_esecuzione', ''))}</pre>

<h2>Report Tester</h2>
<pre>{esc(stato.get('report_tester', ''))}</pre>

<h2>Verdict Finale</h2>
<p class="{verdict_cls}">{verdict_str}</p>
</body>
</html>"""

    with open(path_html, "w", encoding="utf-8") as f:
        f.write(html)
    if _RICH_OK and _rich:
        _rich.print(f"  [bold green]📄 Report HTML:[/] [dim]{path_html}[/]")
    else:
        print(f"📄 Report HTML: {path_html}")

    return path_py, path_html


# ── Costruzione del grafo LangGraph ──────────────────────────────────────────

def costruisci_grafo():
    grafo = StateGraph(Stato)

    grafo.add_node("ricercatore",   agente_ricercatore)
    grafo.add_node("pianificatore", agente_pianificatore)
    grafo.add_node("programmatori", agente_programmatori_parallelo)
    grafo.add_node("tester",        agente_tester)
    grafo.add_node("correttore",    agente_correttore)
    grafo.add_node("ottimizzatore", agente_ottimizzatore)

    grafo.set_entry_point("ricercatore")
    grafo.add_edge("ricercatore",   "pianificatore")
    grafo.add_edge("pianificatore", "programmatori")
    grafo.add_edge("programmatori", "tester")
    grafo.add_conditional_edges(
        "tester",
        decide_dopo_tester,
        {"ottimizzatore": "ottimizzatore", "correttore": "correttore"}
    )
    grafo.add_edge("correttore",    "tester")       # loop di correzione
    grafo.add_edge("ottimizzatore", END)

    return grafo.compile()


# ── API pubblica per Textual (nessun suspend) ────────────────────────────────

def avvia_pipeline(task: str, print_cb=None) -> dict:
    """
    Esegue la pipeline multi-agente completa.
    Carica modelli dalle preferenze salvate (memoria JSON).
    Se print_cb è fornito, ogni riga di stdout viene inviata a print_cb(testo).
    Ritorna il dizionario stato finale (con codice, verdict, ecc.).
    """
    import sys as _sys_ap

    memoria = carica_memoria()
    pref = memoria.get("preferenze", {})
    pref_modelli = pref.get("modelli", {})

    tutti = get_modelli_ollama()
    nomi_locali = [m["nome"] for m in tutti if m.get("size_gb", 0) > 0]

    # Controlla se Intel GPU è disponibile su porta 11435
    _intel_attiva = False
    try:
        _r = requests.get(f"http://localhost:{PORTA_INTEL}/api/tags", timeout=1)
        _intel_attiva = _r.status_code == 200
    except Exception:
        pass

    # Se Intel è attiva, unisci i modelli di entrambe le porte
    nomi_intel: list[str] = []
    if _intel_attiva:
        tutti_intel = get_modelli_ollama(PORTA_INTEL)
        nomi_intel = [m["nome"] for m in tutti_intel if m.get("size_gb", 0) > 0]
        nomi_tutti = list(dict.fromkeys(nomi_locali + nomi_intel))
    else:
        nomi_tutti = nomi_locali

    RL = ["ricercatore", "pianificatore", "programmatore", "tester", "ottimizzatore"]

    modelli: dict[str, str] = {}
    for ruolo in RL:
        sv = pref_modelli.get(ruolo, "")
        if sv and sv in nomi_tutti:
            modelli[ruolo] = sv
        else:
            modelli[ruolo] = (
                trova_default(nomi_tutti, PREFERENZE_MODELLI.get(ruolo, []))
                if nomi_tutti else "qwen3:4b"
            )

    # Distribuisce gli agenti su NVIDIA+Intel quando Intel è disponibile.
    # I 2 programmatori girano su GPU separate → vera parallelizzazione.
    if _intel_attiva:
        def _porta(modello: str) -> int:
            return PORTA_INTEL if modello in nomi_intel else PORTA_NVIDIA

        porte = {
            "ricercatore":     _porta(modelli["ricercatore"]),
            "pianificatore":   _porta(modelli["pianificatore"]),
            "programmatore":   _porta(modelli["programmatore"]),
            "programmatore_a": PORTA_NVIDIA,
            "programmatore_b": PORTA_INTEL,
            "tester":          _porta(modelli["tester"]),
            "ottimizzatore":   _porta(modelli["ottimizzatore"]),
        }
    else:
        porte = {ruolo: PORTA_NVIDIA for ruolo in RL}
        porte["programmatore_a"] = PORTA_NVIDIA
        porte["programmatore_b"] = PORTA_NVIDIA

    stato_iniziale: Stato = {
        "task":               task,
        "modelli":            modelli,
        "porte":              porte,
        "memoria":            memoria,
        "contesto_ricerca":   "",
        "piano_a":            "",
        "piano_b":            "",
        "codice_a":           "",
        "codice_b":           "",
        "codice_corrente":    "",
        "output_esecuzione":  "",
        "report_tester":      "",
        "verdict":            "",
        "tentativi":          0,
        "codice_ottimizzato":   "",
        "note_ottimizzazione":  "",
        "system_prompts":       {},
    }

    app_grafo = costruisci_grafo()

    if print_cb:
        class _Writer:
            def write(self, t: str):
                if t and t.strip():
                    print_cb(t)
            def flush(self): pass
        old = _sys_ap.stdout
        _sys_ap.stdout = _Writer()  # type: ignore[assignment]
        try:
            risultato = app_grafo.invoke(stato_iniziale)
        finally:
            _sys_ap.stdout = old
    else:
        risultato = app_grafo.invoke(stato_iniziale)

    salva_risultati(risultato)
    salva_memoria(memoria, risultato)
    return risultato


# ── UI selezione modalità offline/online ─────────────────────────────────────

def scegli_modalita() -> bool:
    """
    Pannello Rich interattivo per scegliere offline/online.
    Tab / Shift+Tab / ← → per alternare. Invio per confermare.
    Ritorna True se online, False se offline (default).
    """
    modalita_online = False

    def render(online: bool) -> None:
        os.system("cls" if sys.platform == "win32" else "clear")
        if _RICH_OK and _rich:
            if online:
                corpo = (
                    "\n"
                    "   [dim]🔒 OFFLINE[/]"
                    "   [dim]───────────────────────[/]"
                    "   [bold green]🌐 ONLINE  ◀[/]\n\n"
                    "   [green]→ Usa modelli cloud (più potenti e veloci)[/]\n"
                    "   [yellow]⚠️  I task vengono inviati a server esterni[/]\n"
                    "   [yellow]   Non inserire dati personali o sensibili[/]\n"
                )
                bordo = "yellow"
            else:
                corpo = (
                    "\n"
                    "   [bold cyan]🔒 OFFLINE  ◀[/]"
                    "   [dim]───────────────────────[/]"
                    "   [dim]🌐 ONLINE[/]\n\n"
                    "   [cyan]→ Usa solo Ollama in locale sul tuo PC[/]\n"
                    "   [cyan]→ I tuoi dati non escono mai dalla macchina[/]\n"
                )
                bordo = "cyan"

            corpo += "\n   [dim]  Tab · Shift+Tab · ←→   per cambiare     Invio per confermare[/]\n"
            _rich.print(_RichPanel(
                corpo,
                title="[bold blue]Centro di Controllo — Strumenti Python[/]",
                border_style=bordo,
                padding=(0, 2),
            ))
        else:
            stato = "ONLINE" if online else "OFFLINE"
            print(f"\n  Modalità: [{stato}]  (Tab per cambiare, Invio per confermare)")

    render(modalita_online)

    if not _HAS_TERMIOS:
        # Windows: fallback testuale
        val = input("  Modalità [1=offline / 2=online, invio = offline]: ").strip()
        return val == "2"

    fd = sys.stdin.fileno()
    old = termios.tcgetattr(fd)
    try:
        tty.setraw(fd)
        while True:
            ch = sys.stdin.read(1)
            if ch in ('\r', '\n'):
                break
            elif ch == '\t':                        # Tab → alterna
                modalita_online = not modalita_online
                render(modalita_online)
            elif ch == '\x1b':                      # Escape sequence
                ch2 = sys.stdin.read(1)
                if ch2 == '[':
                    ch3 = sys.stdin.read(1)
                    if ch3 == 'Z':                  # Shift+Tab → alterna
                        modalita_online = not modalita_online
                        render(modalita_online)
                    elif ch3 == 'C':                # Freccia destra → online
                        if not modalita_online:
                            modalita_online = True
                            render(modalita_online)
                    elif ch3 == 'D':                # Freccia sinistra → offline
                        if modalita_online:
                            modalita_online = False
                            render(modalita_online)
            elif ch == '\x03':                      # Ctrl+C
                raise KeyboardInterrupt
    except termios.error:
        # stdin non è un tty (pipe/redirect) → fallback testuale
        termios.tcsetattr(fd, termios.TCSADRAIN, old)
        val = input("  Modalità [1=offline / 2=online, invio = offline]: ").strip()
        return val == "2"
    finally:
        try:
            termios.tcsetattr(fd, termios.TCSADRAIN, old)
        except Exception:
            pass

    return modalita_online


# ── Salvataggio con conferma ──────────────────────────────────────────────────

def _salva_con_conferma(risultato: Stato) -> tuple[str, str]:
    """
    Chiede all'utente se vuole salvare codice .py e/o report HTML.
    Ritorna (path_py, path_html) con '(non salvato)' per le voci rifiutate.
    """
    _NON_SALVATO = "(non salvato)"

    if _RICH_OK and _rich:
        from rich.panel import Panel as _PS
        from rich.text import Text as _TS
        from rich.rule import Rule as _RS
        _t = _TS()
        _t.append("  [S / Invio]  ", style="bold green")
        _t.append("Salva tutto  (codice .py + report HTML)\n")
        _t.append("  [C]          ", style="bold cyan")
        _t.append("Solo codice  (.py, senza HTML)\n")
        _t.append("  [N]          ", style="bold red")
        _t.append("Non salvare  (risultato disponibile solo a schermo)")
        _rich.print(_PS(_t, title="[bold]💾 Vuoi salvare i risultati?[/]",
                        border_style="yellow", padding=(0, 1)))
        _rich.print(_RS(style="dim"))
        scelta = input("  Scelta [S/C/N]: ").strip().upper()
    else:
        print("\n  💾 Vuoi salvare i risultati?")
        print("  [S/Invio] Tutto  [C] Solo codice  [N] Non salvare")
        scelta = input("  Scelta: ").strip().upper()

    if scelta in ("", "S"):
        path_py, path_html = salva_risultati(risultato)
        return path_py, path_html

    elif scelta == "C":
        # Salva solo il .py, salta l'HTML
        nome_base  = re.sub(r"[^a-z0-9]+", "_", risultato["task"].lower())[:35]
        timestamp  = datetime.now().strftime("%Y%m%d_%H%M%S")
        codice_raw = risultato.get("codice_ottimizzato") or risultato.get("codice_corrente", "")
        match      = re.search(r"```(?:python)?\n(.*?)```", codice_raw, re.DOTALL)
        codice_pulito = match.group(1).strip() if match else codice_raw.strip()
        path_py = os.path.join(CARTELLA_OUTPUT, f"output_{nome_base}_{timestamp}.py")
        with open(path_py, "w", encoding="utf-8") as f:
            f.write(f"# Task: {risultato['task']}\n")
            f.write(f"# Generato il: {datetime.now().strftime('%Y-%m-%d %H:%M')}\n\n")
            f.write(codice_pulito)
        if _RICH_OK and _rich:
            _rich.print(f"  [bold green]💾 Codice salvato:[/] [dim]{path_py}[/]")
            _rich.print(f"  [dim]Report HTML: non salvato.[/]")
        else:
            print(f"  💾 Codice salvato: {path_py}")
        return path_py, _NON_SALVATO

    else:  # N
        if _RICH_OK and _rich:
            _rich.print("  [dim]Risultati non salvati su disco.[/]")
        else:
            print("  Risultati non salvati.")
        return _NON_SALVATO, _NON_SALVATO


# ── 10 Prompt di Qualita per categoria ───────────────────────────────────────

_PROMPT_QUALITA: list[dict] = [
    {
        "cat": "Matematica Scolastica",
        "emoji": "📐",
        "livelli": ["scolastico", "universitario", "ricercatore"],
        "prompt": (
            "Crea un programma che risolve equazioni di secondo grado ax²+bx+c=0: "
            "calcola il discriminante, mostra le radici reali o complesse, "
            "e disegna una parabola ASCII con asse di simmetria e vertice evidenziati."
        ),
    },
    {
        "cat": "Matematica di Frontiera",
        "emoji": "∞",
        "livelli": ["universitario", "ricercatore"],
        "prompt": (
            "Implementa un solver numerico per ODE (equazioni differenziali ordinarie) "
            "con metodi Runge-Kutta 4° ordine e Adams-Bashforth multistep. "
            "Confronta l'errore di troncamento globale su y'=sin(x)*y, "
            "visualizza la convergenza e l'ordine di accuratezza stimato empiricamente."
        ),
    },
    {
        "cat": "Informatica Base",
        "emoji": "💻",
        "livelli": ["scolastico", "universitario"],
        "prompt": (
            "Crea un programma per gestire una rubrica telefonica da riga di comando: "
            "aggiungi, cerca, modifica, elimina contatti. "
            "Salva i dati in JSON, valida i numeri di telefono con regex, "
            "e mostra la lista ordinata per cognome."
        ),
    },
    {
        "cat": "Informatica Alto Livello",
        "emoji": "⚙️",
        "livelli": ["universitario", "ricercatore"],
        "prompt": (
            "Implementa un interprete Lisp minimale in Python: "
            "supporta lambda, let, if, define, liste e aritmetica. "
            "Usa un environment a catena (closure), gestisci tail-call optimization "
            "e aggiungi un REPL con history tramite readline."
        ),
    },
    {
        "cat": "Sicurezza Reti Base",
        "emoji": "🔒",
        "livelli": ["scolastico", "universitario"],
        "prompt": (
            "Crea uno scanner di porte TCP didattico con socket Python: "
            "leggi una lista di porte comuni (20-443), prova la connessione con timeout 0.5s, "
            "identifica i servizi noti (HTTP, SSH, FTP, DNS), "
            "e genera un report colorato con Rich mostrando stato aperto/chiuso."
        ),
    },
    {
        "cat": "Sicurezza Enterprise",
        "emoji": "🛡️",
        "livelli": ["universitario", "ricercatore"],
        "prompt": (
            "Implementa un sistema di audit log tamper-evident: "
            "ogni voce di log e firmata con HMAC-SHA256 usando una chiave derivata da KDF (scrypt). "
            "Supporta verifica dell'integrita della catena, rotazione chiavi con re-firma, "
            "e export in formato CEF (Common Event Format) per SIEM."
        ),
    },
    {
        "cat": "Fisica Scolastica",
        "emoji": "🔭",
        "livelli": ["scolastico", "universitario"],
        "prompt": (
            "Simula il moto di un proiettile con e senza resistenza dell'aria (drag quadratico). "
            "Chiedi angolo e velocita iniziale, calcola gittata e altezza massima, "
            "visualizza le due traiettorie sovrapposte in ASCII art "
            "e mostra la differenza percentuale di gittata."
        ),
    },
    {
        "cat": "Fisica Universitaria",
        "emoji": "⚛️",
        "livelli": ["universitario", "ricercatore"],
        "prompt": (
            "Simula la diffusione quantistica di un pacchetto d'onda gaussiano "
            "tramite il metodo Crank-Nicolson (schema implicito stabile). "
            "Mostra l'evoluzione temporale della funzione d'onda |psi|² ASCII, "
            "calcola il valor medio di posizione e impulso, "
            "verifica la conservazione della norma ad ogni step."
        ),
    },
    {
        "cat": "Chimica Scolastica",
        "emoji": "🧪",
        "livelli": ["scolastico", "universitario"],
        "prompt": (
            "Crea un bilanciatore di equazioni chimiche con il metodo algebrico: "
            "leggi la formula (es. 'H2+O2=H2O'), costruisci la matrice degli elementi, "
            "risolvi il sistema lineare con frazioni per trovare i coefficienti interi minimi, "
            "e visualizza l'equazione bilanciata con i coefficienti colorati."
        ),
    },
    {
        "cat": "Chimica da Ricercatore",
        "emoji": "🔬",
        "livelli": ["ricercatore"],
        "prompt": (
            "Implementa un simulatore Monte Carlo di dinamica molecolare semplificata: "
            "N particelle in una box 2D con potenziale di Lennard-Jones. "
            "Applica condizioni periodiche al contorno, calcola energia totale e pressione "
            "via viriale, equilibra il sistema e mostra la funzione di distribuzione radiale g(r)."
        ),
    },
]

_LIVELLI_UTENTE = {
    "1": ("scolastico",    "📚 Scolastico",    "Argomenti chiari, step-by-step, esempi pratici"),
    "2": ("universitario", "🎓 Universitario", "Rigore formale, teoria + implementazione"),
    "3": ("ricercatore",   "🔬 Ricercatore",   "Massima profondita, approcci avanzati, letteratura"),
}


def _chiedi_livello_utente() -> str:
    """
    Chiede all'utente il suo livello di conoscenza.
    Ritorna la chiave livello: 'scolastico' | 'universitario' | 'ricercatore'.
    """
    if _RICH_OK and _rich:
        from rich.panel import Panel as _PL
        from rich.text import Text as _TL
        t = _TL()
        t.append("\n  Qual e il tuo livello di conoscenza sull'argomento?\n\n")
        for k, (_, label, descr) in _LIVELLI_UTENTE.items():
            t.append(f"  [{k}]  {label:<22}", style="bold cyan")
            t.append(f" {descr}\n", style="dim")
        t.append("\n  [Invio] = Universitario (default)\n", style="dim")
        _rich.print(_PL(t, title="[bold]Livello conoscenza interlocutore[/]",
                        border_style="blue", padding=(0, 1)))
    else:
        print("\n  Livello di conoscenza:")
        for k, (_, label, descr) in _LIVELLI_UTENTE.items():
            print(f"  [{k}]  {label}  —  {descr}")
        print("  [Invio] = Universitario")

    scelta = input("  ❯ ").strip()
    info = _LIVELLI_UTENTE.get(scelta)
    livello = info[0] if info else "universitario"

    label = _LIVELLI_UTENTE.get(scelta, _LIVELLI_UTENTE["2"])[1]
    if _RICH_OK and _rich:
        _rich.print(f"  [green]✓ Livello: {label}[/]")
    else:
        print(f"  Livello: {label}")
    return livello


def _menu_prompt_qualita(livello: str) -> str:
    """
    Mostra i 10 prompt di qualita filtrati per livello.
    Ritorna il prompt scelto oppure '' se l'utente vuole scrivere il proprio.
    """
    # Filtra per livello
    disponibili = [p for p in _PROMPT_QUALITA if livello in p["livelli"]]

    if _RICH_OK and _rich:
        from rich.panel import Panel as _PQ
        from rich.text import Text as _TQ
        t = _TQ()
        t.append("\n  Prompt predefiniti disponibili per il tuo livello:\n\n")
        for i, p in enumerate(disponibili, 1):
            t.append(f"  {i:2}. {p['emoji']} {p['cat']}\n", style="bold cyan")
        t.append("\n  [0 / Invio]  Scrivi il tuo task personalizzato\n", style="dim")
        _rich.print(_PQ(t, title="[bold]Biblioteca Prompt Prismalux[/]",
                        border_style="magenta", padding=(0, 1)))
    else:
        print("\n  Prompt predefiniti:")
        for i, p in enumerate(disponibili, 1):
            print(f"  {i:2}. {p['emoji']} {p['cat']}")
        print("  [0 / Invio]  Task personalizzato")

    try:
        scelta = input("  ❯ ").strip()
        if scelta.isdigit():
            n = int(scelta)
            if 1 <= n <= len(disponibili):
                scelto = disponibili[n - 1]
                if _RICH_OK and _rich:
                    _rich.print(f"\n  [green]✓ Prompt selezionato: {scelto['emoji']} {scelto['cat']}[/]")
                    _rich.print(f"  [dim]{scelto['prompt'][:120]}...[/]")
                else:
                    print(f"  Selezionato: {scelto['cat']}")
                return scelto["prompt"]
    except (ValueError, EOFError):
        pass
    return ""


def _ottimizza_prompt_con_ai(task: str, livello: str, modello: str, porta: int = 11434) -> str:
    """
    Usa Ollama per ottimizzare il task prompt in base al livello utente.
    Ritorna il prompt ottimizzato oppure '' in caso di errore/rifiuto.
    """
    try:
        from langchain_ollama import ChatOllama
        from langchain_core.messages import SystemMessage, HumanMessage
    except ImportError:
        return ""

    livello_info = {
        "scolastico":    "studente delle scuole superiori, linguaggio chiaro e semplice",
        "universitario": "studente universitario o professionista, rigore tecnico appropriato",
        "ricercatore":   "ricercatore esperto, massima profondita tecnica, terminologia specialistica",
    }
    desc_livello = livello_info.get(livello, livello_info["universitario"])

    sistema = (
        "Sei un esperto di prompt engineering per sistemi multi-agente AI. "
        "Il tuo compito e migliorare i task prompt per ottenere codice Python di qualita superiore."
    )

    utente = f"""Ottimizza questo task prompt per un sistema multi-agente che genera codice Python.
L'interlocutore e: {desc_livello}.

TASK ORIGINALE:
{task}

REGOLE per l'ottimizzazione:
1. Aggiungi requisiti tecnici specifici mancanti (input/output, edge cases)
2. Specifica il formato dell'output atteso
3. Indica le librerie Python preferite se pertinente
4. Adatta complessita e terminologia al livello dell'interlocutore
5. Mantieni la lunghezza ragionevole (max 4 righe)
6. NON inventare requisiti non impliciti nel task originale

Rispondi SOLO con il task ottimizzato, senza spiegazioni:"""

    try:
        llm = ChatOllama(
            model=modello,
            base_url=f"http://localhost:{porta}",
            num_predict=400,
            temperature=0.3,
        )
        risposta = llm.invoke([
            SystemMessage(content=sistema),
            HumanMessage(content=utente),
        ]).content
        return risposta.strip()
    except Exception as e:
        if _RICH_OK and _rich:
            _rich.print(f"  [yellow]Ottimizzazione prompt fallita: {e}[/]")
        return ""


def _chiedi_ottimizza_prompt(task: str, livello: str, modello: str, porta: int) -> str:
    """
    Chiede all'utente se vuole ottimizzare il prompt con AI.
    Mostra originale e ottimizzato. Ritorna il task finale da usare.
    """
    if _RICH_OK and _rich:
        from rich.panel import Panel as _PO
        from rich.text import Text as _TO
        t = _TO()
        t.append("  Vuoi che Prismalux ottimizzi automaticamente il task\n")
        t.append("  per massimizzare la qualita del codice generato?\n\n")
        t.append("  [S]  Si — mostra originale + ottimizzato, poi scegli\n", style="bold green")
        t.append("  [N / Invio]  No — usa il task cosi com'e\n", style="dim")
        _rich.print(_PO(t, title="[bold magenta]Ottimizzatore Prompt Prismalux[/]",
                        border_style="magenta", padding=(0, 1)))
    else:
        print("\n  Ottimizzare il prompt con AI? [S/n]")

    scelta = input("  ❯ ").strip().lower()
    if scelta not in ("s", "si", "y", "yes"):
        return task

    # Genera versione ottimizzata
    if _RICH_OK and _rich:
        _rich.print("  [dim]Ottimizzazione in corso...[/]")
    ottimizzato = _ottimizza_prompt_con_ai(task, livello, modello, porta)

    if not ottimizzato:
        if _RICH_OK and _rich:
            _rich.print("  [yellow]Ottimizzazione non disponibile. Uso task originale.[/]")
        return task

    # Mostra confronto
    if _RICH_OK and _rich:
        from rich.panel import Panel as _PC
        _rich.print(_PC(task, title="[bold]ORIGINALE[/]", border_style="dim", padding=(0, 1)))
        _rich.print(_PC(ottimizzato, title="[bold green]OTTIMIZZATO[/]", border_style="green", padding=(0, 1)))
        _rich.print("\n  [C / Invio] Usa ottimizzato   [O] Usa originale")
    else:
        print(f"\n  ORIGINALE:\n  {task}")
        print(f"\n  OTTIMIZZATO:\n  {ottimizzato}")
        print("\n  [C/Invio] Ottimizzato  [O] Originale")

    scelta2 = input("  ❯ ").strip().lower()
    if scelta2 in ("o", "orig", "originale"):
        if _RICH_OK and _rich:
            _rich.print("  [dim]Uso il task originale.[/]")
        return task

    if _RICH_OK and _rich:
        _rich.print("  [green]Uso il task ottimizzato.[/]")
    return ottimizzato


def _profila_con_cprofile(path_py: str) -> list:
    """
    Esegue cProfile sul file Python generato e ritorna lista di blocchi
    categorizzati per priorità di ottimizzazione Cython.

    Ritorna lista di dict:
      {func, file, linea, ncalls, cumtime, perc, prio}
    dove prio = '🔴 ALTA' | '🟡 MEDIA' | '🟢 BASSA'
    """
    import pstats
    prof_tmp = path_py + ".profdata"
    try:
        r = subprocess.run(
            [sys.executable, "-m", "cProfile", "-o", prof_tmp, path_py],
            capture_output=True, text=True, timeout=30,
        )
        if not os.path.isfile(prof_tmp):
            return []

        stats = pstats.Stats(prof_tmp)
        stats.sort_stats("cumulative")

        totale = sum(row[3] for row in stats.stats.values())
        if totale <= 0:
            return []

        risultati = []
        for (file, lineno, func), (ncalls, _, _tt, cumtime, _) in sorted(
            stats.stats.items(), key=lambda x: -x[1][3]
        )[:40]:
            # salta built-in, frozen, moduli std non modificabili
            if any(x in str(file) for x in ("<built-in>", "<frozen", "{built-in")):
                continue
            perc = (cumtime / totale) * 100
            if perc < 1.0:
                continue
            prio = "🔴 ALTA" if perc >= 10 else "🟡 MEDIA" if perc >= 3 else "🟢 BASSA"
            risultati.append({
                "func":    func,
                "file":    os.path.basename(str(file)),
                "linea":   lineno,
                "ncalls":  ncalls,
                "cumtime": cumtime,
                "perc":    perc,
                "prio":    prio,
            })
        return risultati[:10]
    except Exception:
        return []
    finally:
        try:
            os.remove(prof_tmp)
        except Exception:
            pass


def _offri_compilazione_cython(path_py: str, modello: str, porta: int) -> None:
    """
    Ultima domanda del workflow multi-agente:
    1. Profila con cProfile e mostra blocchi caldi (priorità ALTA/MEDIA/BASSA)
    2. Offre di compilare in Cython i blocchi ad alta priorità.
    """
    if not path_py or path_py == "(non salvato)":
        return

    # ── Analisi cProfile ──────────────────────────────────────────────────────
    if _RICH_OK and _rich:
        from rich.rule import Rule as _RCp
        _rich.print(_RCp("[bold yellow]⏱  Analisi prestazioni con cProfile[/]", style="yellow"))
        _rich.print("  [dim]Esecuzione rapida del codice per rilevare i colli di bottiglia...[/]")

    blocchi = _profila_con_cprofile(path_py)

    if _RICH_OK and _rich:
        from rich.table import Table as _TCp
        from rich.panel import Panel as _PCp
        from rich.text import Text as _TxtCp

        if blocchi:
            tab = _TCp(show_header=True, header_style="bold dim", box=None, padding=(0, 1))
            tab.add_column("Priorità",  style="bold", width=12)
            tab.add_column("Funzione",  style="white", max_width=28)
            tab.add_column("File",      style="dim",   max_width=22)
            tab.add_column("Chiamate",  style="cyan",  justify="right", width=9)
            tab.add_column("Cumul. s",  style="cyan",  justify="right", width=9)
            tab.add_column("% totale",  style="bold",  justify="right", width=9)
            for b in blocchi:
                colore = "red" if "ALTA" in b["prio"] else "yellow" if "MEDIA" in b["prio"] else "green"
                tab.add_row(
                    f"[{colore}]{b['prio']}[/]",
                    b["func"],
                    b["file"],
                    str(b["ncalls"]),
                    f"{b['cumtime']:.3f}",
                    f"{b['perc']:.1f}%",
                )
            _rich.print(_PCp(tab,
                title="[bold yellow]⏱  Bottleneck rilevati — candidati Cython[/]",
                border_style="yellow", padding=(0, 1)))
            # suggerimento
            alte = [b["func"] for b in blocchi if "ALTA" in b["prio"]]
            if alte:
                _rich.print(f"  [bold red]  → Priorità ALTA:[/] [cyan]{', '.join(alte[:3])}[/]")
                _rich.print("  [dim]    Questi blocchi trarranno il massimo beneficio dalla compilazione Cython.[/]")
        else:
            _rich.print("  [dim]  cProfile: esecuzione troppo rapida o errore — profilo non disponibile.[/]")
    else:
        if blocchi:
            print("\n  ⏱  Bottleneck rilevati:")
            for b in blocchi:
                print(f"  {b['prio']:12s}  {b['func']:25s}  {b['perc']:.1f}%  ({b['cumtime']:.3f}s)")

    if _RICH_OK and _rich:
        from rich.panel import Panel as _PCy
        from rich.rule import Rule as _RuleCy
        from rich.text import Text as _TCy
        _rich.print(_RuleCy("[bold cyan]⚡ Ultimo passaggio — Cython[/]", style="cyan"))
        t = _TCy()
        t.append("\n  Vuoi ottimizzarlo con Cython? ", style="bold white")
        t.append("(Python → C compilato, fino a 100× più veloce)\n\n")
        t.append("  [bold cyan][S][/bold cyan]  Si — compila ora\n")
        t.append("  [dim][N / Invio]  No — fine workflow[/dim]\n")
        _rich.print(_PCy(t, title="[bold cyan]⚡ Cython Studio[/]",
                         border_style="cyan", padding=(0, 1)))
    else:
        print("\n  ⚡ Vuoi ottimizzarlo con Cython? [S/n] ")

    try:
        scelta = input("  ❯ ").strip().lower()
    except (KeyboardInterrupt, EOFError):
        return
    if scelta not in ("s", "si", "y", "yes"):
        return

    # Importa bridge Cython Studio
    try:
        _cs_path = os.path.join(
            os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
            "Ottimizzazioni_Avanzate", "cython_studio"
        )
        if _cs_path not in sys.path:
            sys.path.insert(0, _cs_path)
        from cython_studio import compila_per_multi_agente, installa_cython, _mostra_risultato

        if not installa_cython():
            if _RICH_OK and _rich:
                _rich.print("  [red]Cython non disponibile. Impossibile compilare.[/]")
            return

        if _RICH_OK and _rich:
            from rich.rule import Rule as _RCy
            _rich.print(_RCy("[bold cyan]CYTHON STUDIO — Compilazione[/]", style="cyan"))

        ris = compila_per_multi_agente(
            path_py=path_py,
            modello_ollama=modello,
            porta_ollama=porta,
        )
        _mostra_risultato(ris)

    except Exception as e:
        if _RICH_OK and _rich:
            _rich.print(f"  [red]Errore Cython Studio: {e}[/]")
        else:
            print(f"  Errore Cython: {e}")

    input("\n  [INVIO] per continuare ")


# ── Main ──────────────────────────────────────────────────────────────────────

def avvia_menu() -> None:
    """Punto di ingresso Multi-Agente AI (standalone o da AVVIA_Prismalux)."""
    try:
        # ── Import UI condivisa ────────────────────────────────────────────────
        import shutil as _sh_ma
        _root_ma = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        if _root_ma not in sys.path:
            sys.path.insert(0, _root_ma)
        if os.path.join(_root_ma, "core") not in sys.path:
            sys.path.insert(0, os.path.join(_root_ma, "core"))
        from check_deps import stampa_header as _stampa_h
        from rich.console import Console as _Con_ma
        from rich.table import Table as _Tab_ma
        from rich.text import Text as _Tex_ma
        from rich.rule import Rule as _Rule_ma

        def _leggi_tasto_raw_ma() -> str:
            """Tasto singolo senza INVIO: TAB, ENTER, BACKSPACE, 0-9, lettere."""
            if not _HAS_TERMIOS:
                # Windows: legge una riga intera — niente prompt extra, il testo
                # è già stato scritto da sys.stdout.write prima della chiamata
                s = input("").strip().upper()
                if s in ('T', 'TAB'):
                    return 'TAB'
                if s in ('\x7f', '\x08'):   # solo veri caratteri backspace/del
                    return 'BACKSPACE'
                if not s:                   # Invio senza testo = conferma (ENTER)
                    return 'ENTER'
                return s[:1]
            fd = sys.stdin.fileno()
            old = termios.tcgetattr(fd)
            try:
                tty.setraw(fd)
                ch = sys.stdin.read(1)
                if ch == '\x03':
                    raise KeyboardInterrupt
                if ch == '\t':
                    return 'TAB'
                if ch in ('\r', '\n'):
                    return 'ENTER'
                if ch in ('\x7f', '\x08'):   # DEL / BS → Backspace
                    return 'BACKSPACE'
                return ch.upper() if ch.isalpha() else ch
            finally:
                termios.tcsetattr(fd, termios.TCSADRAIN, old)

        def _disegna_menu_ma(modalita_online: bool, gpu_attive: dict | None = None) -> None:
            os.system("cls" if sys.platform == "win32" else "clear")
            _cols = _sh_ma.get_terminal_size(fallback=(120, 40)).columns
            _con = _Con_ma(width=_cols)
            _stampa_h(_con, breadcrumb="Prismalux › 🤖 Multi-Agente AI")

            _sx = _Tex_ma()
            _sx.append("  Sistema Multi-Agente AI\n", style="bold magenta")
            _sx.append("  6 agenti collaborano per sviluppare codice.\n\n", style="dim")
            _sx.append("  🔎  Ricercatore\n",                  style="cyan")
            _sx.append("  📐  Pianificatore\n",                 style="blue")
            _sx.append("  💻  2 Programmatori in parallelo\n",  style="green")
            _sx.append("  🧪  Tester (loop correzione)\n",      style="yellow")
            _sx.append("  ✨  Ottimizzatore\n\n",               style="magenta")
            if gpu_attive:
                nomi_g = " + ".join(
                    f"{info['emoji']}{n}" for n, info in gpu_attive.items()
                )
                _sx.append(f"  🎮  Multi GPU — {_GP_EMOJI}{_GP_NOME} + {nomi_g}\n", style="bold green")
                porte_str = " · ".join(
                    f"{n}:{info['porta']}" for n, info in gpu_attive.items()
                )
                _sx.append(f"      {_GP_NOME}:{PORTA_NVIDIA} · {porte_str}\n\n", style="dim green")
            _sx.append("  Modalità corrente:\n", style="dim")
            if modalita_online:
                _sx.append("  🌐 ONLINE   ", style="bold green")
                _sx.append("[Tab per cambiare]\n\n", style="dim")
                _sx.append("  ⚠  Task inviati a server esterni.\n", style="yellow")
            else:
                _sx.append("  🔒 OFFLINE  ", style="bold cyan")
                _sx.append("[Tab per cambiare]\n\n", style="dim")
                _sx.append("  ✓  Dati sempre sul tuo PC.\n", style="cyan")
            _righe_sx = _sx.plain.count("\n")

            _win = sys.platform == "win32"
            _dx_righe = [
                ("  Scegli modalità",    "dim italic", 1),
                ("  e premi Invio",      "dim italic", 1),
                ("  per avviare.",       "dim italic", 2),
                ("  ─────────────────",  "dim",        2),
                ("  Invio   avvia",      "dim",        1),
                ("  T+Invio modalità" if _win else "  Tab     modalità", "dim", 1),
                ("  0+Invio torna"    if _win else "  0 / ⌫   torna",   "dim", 1),
                ("  Q+Invio esci"     if _win else "  Q       esci",    "dim", 1),
            ]
            if _win:
                _dx_righe += [("  (Windows: digita", "dim", 1),
                               ("   + Invio per ok)", "dim", 1)]
            _righe_dx = sum(_nl for _, _, _nl in _dx_righe)
            _delta = max(0, _righe_sx - _righe_dx)

            _dx = _Tex_ma()
            for _t, _s, _nl in _dx_righe:
                _dx.append(_t + "\n" * _nl, style=_s)
            if _delta:
                _dx.append("\n" * _delta)

            _body = _Tab_ma.grid(expand=True)
            _body.add_column(ratio=3)
            _body.add_column(min_width=24, ratio=1)
            _body.add_row(
                _RichPanel(_sx, title="[bold magenta]Multi-Agente AI[/]",
                           border_style="bold magenta", padding=(1, 2)),
                _RichPanel(_dx, border_style="magenta", padding=(1, 1)),
            )
            _con.print(_body)
            _con.print(_Rule_ma(style="dim"))
            if sys.platform == "win32":
                sys.stdout.write("  Digita: Invio=avvia  T+Invio=modalità  0+Invio=torna  Q+Invio=esci  ❯ ")
            else:
                sys.stdout.write("  Premi INVIO per avviare  (Tab = modalità  |  0/⌫ = torna  |  Q = esci)  ❯ ")
            sys.stdout.flush()

        # ── Scheda 2: Modelli + Dispositivo ────────────────────────────────────
        def _scheda_seleziona_modelli_ma(memoria_in: dict, solo_locali: bool) -> tuple[dict, dict, str]:
            pref             = memoria_in.get("preferenze", {})
            pref_modelli     = pref.get("modelli", {})
            disp_salvato     = pref.get("dispositivo", "GPU")
            pref_disp_ag     = pref.get("dispositivi_agenti", {})

            tutti = get_modelli_ollama()
            _FB = {"ricercatore": "llama3.2:3b-instruct-q4_K_M",
                   "pianificatore": "deepseek-r1:7b",
                   "programmatore": "qwen2.5-coder:7b",
                   "tester": "gemma2:2b",
                   "ottimizzatore": "qwen2.5-coder:7b"}
            _porte_fb = {r: PORTA_NVIDIA for r in _FB}

            if not tutti:
                # ── Ollama non risponde: chiedi se aspettare ───────────────────
                _attesa_sec = 0
                while True:
                    os.system("cls" if sys.platform == "win32" else "clear")
                    _cols_err = _sh_ma.get_terminal_size(fallback=(120, 40)).columns
                    _con_err  = _Con_ma(width=_cols_err)
                    _stampa_h(_con_err)
                    if _RICH_OK and _rich:
                        from rich.panel import Panel as _PErr
                        from rich.text import Text as _TErr
                        _t = _TErr()
                        if _attesa_sec == 0:
                            _t.append("\n  ⚠️  Ollama non risponde su localhost:11434\n\n",
                                      style="bold red")
                        else:
                            _t.append(f"\n  ⏳  Attesa in corso... ({_attesa_sec}s)\n\n",
                                      style="bold yellow")
                            _t.append("  Ollama è ancora lento a rispondere.\n\n", style="dim")
                        _t.append("  Potrebbe essere in fase di avvio (primo caricamento modello).\n\n",
                                  style="white")
                        _t.append("  Avvia Ollama se non è già attivo:\n", style="dim")
                        _t.append("     ollama serve\n\n", style="bold cyan")
                        if sys.platform == "win32":
                            _t.append("  Su Windows con GPU AMD/ATI, prova con CPU:\n", style="yellow")
                            _t.append("     $env:OLLAMA_NUM_GPU=0 ; ollama serve\n\n", style="bold cyan")
                        _t.append("  [A]  Aspetta ancora 10 secondi e riprova\n", style="bold green")
                        _t.append("  [0]  Torna al menu principale\n", style="dim")
                        _con_err.print(_PErr(_t,
                                            title="[bold red]❌ Ollama non disponibile[/]",
                                            border_style="red" if _attesa_sec == 0 else "yellow",
                                            padding=(0, 1)))
                    else:
                        print(f"\n  ⚠️  Ollama non risponde (attesa: {_attesa_sec}s).")
                        print("  Avvia 'ollama serve' e riprova.")
                        print("  [A] Aspetta 10s  |  [0] Torna al menu")

                    if sys.platform == "win32":
                        sys.stdout.write("  Scelta [A+Invio=aspetta / 0+Invio=torna]: ")
                    else:
                        sys.stdout.write("  Scelta [A=aspetta / 0=torna]: ")
                    sys.stdout.flush()

                    try:
                        _sc = input("").strip().upper()
                    except (KeyboardInterrupt, EOFError):
                        sys.exit(2)

                    if _sc in ("0", "Q", ""):
                        sys.exit(2)

                    if _sc in ("A",):
                        # Aspetta 10s con conto alla rovescia visivo
                        for _i in range(10, 0, -1):
                            sys.stdout.write(f"\r  ⏳  Tentativo tra {_i}s...   ")
                            sys.stdout.flush()
                            time.sleep(1)
                        sys.stdout.write("\r  🔄  Tentativo connessione...        \n")
                        sys.stdout.flush()
                        _attesa_sec += 10
                        tutti = get_modelli_ollama()
                        if tutti:
                            break   # Ollama ha risposto, continua normalmente

            modelli_info = [m for m in tutti if not solo_locali or m["size_gb"] > 0]
            if solo_locali:
                cloud = [m for m in tutti if m["size_gb"] == 0]
                if cloud:
                    nomi_cloud = ", ".join(m["nome"] for m in cloud)
                    print(f"\n  (Cloud esclusi in modalità offline: {nomi_cloud})")
            if not modelli_info:
                return _FB, _porte_fb, disp_salvato

            nomi = [m["nome"] for m in modelli_info]
            _RL  = ["ricercatore", "pianificatore", "programmatore", "tester", "ottimizzatore"]

            scelti = {}
            for ruolo in _RL:
                sv = pref_modelli.get(ruolo, "")
                scelti[ruolo] = sv if sv in nomi else trova_default(nomi, PREFERENZE_MODELLI.get(ruolo, []))

            # dispositivi per-agente: carica salvati, default = disp_salvato
            _dispositivi_agenti: dict[str, str] = {}
            for ruolo in _RL:
                _dispositivi_agenti[ruolo] = pref_disp_ag.get(ruolo, disp_salvato)
            dispositivo = disp_salvato   # global (per retrocompatibilità e salvataggio)

            def _disp_icon(d: str) -> str:
                return "🎮" if d == "GPU" else "🖥"

            # lista modelli nell'ordine visualizzato — aggiornata da _ridisegna,
            # usata da selezione "M" per tradurre n° display → nome modello
            _display_order: list = []

            def _ridisegna(msg: str = "") -> None:
                os.system("cls" if sys.platform == "win32" else "clear")
                _c2 = _sh_ma.get_terminal_size(fallback=(120, 40)).columns
                _k2 = _Con_ma(width=_c2)
                _stampa_h(_k2, breadcrumb="Prismalux › 🤖 Multi-Agente AI › ⚙️  Modelli")

                # larghezza max nome e max stringa GB — per allineamento colonne dinamico
                _max_n = max((len(m['nome']) for m in modelli_info), default=10)
                _gb_w  = max(
                    (len("cloud" if m["size_gb"] == 0 else f"{m['size_gb']:.1f}GB")
                     for m in modelli_info),
                    default=5,
                )
                # larghezza fissa prefisso: "  NN. " = 6 + separatore " " = 1 + _gb_w
                # → _hdr_pad = 2("  ") + 2(num) + 2(". ") + _max_n + 1(" ") + _gb_w
                _hdr_pad_w = 7 + _max_n + _gb_w

                # mappa modello → [(idx_ruolo, dispositivo), ...]
                _m2a: dict = {}
                for _ir, _ru in enumerate(_RL, 1):
                    _m2a.setdefault(scelti[_ru], []).append((_ir, _dispositivi_agenti[_ru]))

                sx2 = _Tex_ma()
                sx2.append("  Modelli disponibili:\n", style="dim")

                # intestazione colonne agenti (allineata dinamicamente alle righe modello)
                sx2.append(" " * _hdr_pad_w, style="")
                for _ir_h, _ru_h in enumerate(_RL, 1):
                    _ic_h, _ = _RUOLI_INFO[_ru_h]
                    sx2.append(f" {_ir_h}{_ic_h.split()[0]} ", style="bold dim")
                sx2.append("\n")

                # ordina: assegnati prima (per indice agente min), poi gli altri
                def _sort_key_mod(entry):
                    _idx0, _m0 = entry
                    _ag0 = _m2a.get(_m0['nome'], [])
                    return (0, min(ir for ir, _ in _ag0)) if _ag0 else (1, _idx0)

                _ordered = sorted(enumerate(modelli_info, 1), key=_sort_key_mod)
                # aggiorna ordine display condiviso con codice selezione "M"
                _display_order[:] = [_m0 for _, _m0 in _ordered]

                # una riga per modello + 5 celle agente (tabella spunte)
                for _disp_i, (_, _m) in enumerate(_ordered, 1):
                    _gb = "cloud" if _m["size_gb"] == 0 else f"{_m['size_gb']:.1f}GB"
                    _ag = _m2a.get(_m['nome'], [])
                    _assigned = {ir: dv for ir, dv in _ag}
                    _base = f"  {_disp_i:2}. {_m['nome']:<{_max_n}} {_gb:<{_gb_w}}"
                    sx2.append(_base, style="bold white" if _ag else "dim")
                    for _ir_c in range(1, 6):
                        if _ir_c in _assigned:
                            _dv_c = _assigned[_ir_c]
                            sx2.append(f" {_disp_icon(_dv_c)}  ",
                                       style="bold green" if _dv_c == "GPU" else "bold yellow")
                        else:
                            sx2.append("  ·  ", style="dim")
                    sx2.append("\n")

                # distribuzione + AMD warning + msg — nessuna riga vuota extra
                _ngpu = sum(1 for d in _dispositivi_agenti.values() if d == "GPU")
                _ncpu = sum(1 for d in _dispositivi_agenti.values() if d == "CPU")
                sx2.append("\n  Distribuzione: ", style="dim")
                if _ncpu > 0 and _ngpu > 0:
                    sx2.append(f"🎮 {_ngpu} GPU + 🖥 {_ncpu} CPU (parallelo)\n",
                               style="bold green")
                elif _ncpu == 0:
                    sx2.append(f"🎮 GPU — {_GP_EMOJI} {_GP_NOME} ({_GP_ACC})\n", style="green")
                else:
                    sx2.append("🖥 CPU — nessuna GPU\n", style="yellow")
                sx2.append(f"  💾 Salvato: {disp_salvato}", style="dim")
                if sys.platform == "win32" and any(
                    k in _GP_NOME.upper() for k in ("AMD", "RADEON", "ATI")
                ):
                    sx2.append(
                        "\n  ⚠️  AMD/ATI Windows: $env:OLLAMA_NUM_GPU=0 ; ollama serve",
                        style="bold yellow")
                if msg:
                    sx2.append(f"\n  {msg}", style="bold yellow")

                _k2.print(_RichPanel(sx2, title="[bold cyan]⚙️  Modelli & Dispositivo[/]",
                                     border_style="bold cyan", padding=(1, 2)))
                _k2.print(_Rule_ma(style="dim"))
                if sys.platform == "win32":
                    sys.stdout.write(
                        "  Invio=ok  1-5=agente  G+Invio=GPU  C+Invio=CPU  M+Invio=mix  0+Invio=torna  ❯ "
                    )
                else:
                    sys.stdout.write(
                        "  INVIO=ok  |  1–5=agente  |  G=tutti GPU  C=tutti CPU  M=GPU/CPU mix  |  0/⌫=torna  ❯ "
                    )
                sys.stdout.flush()

            # ── Dispatch dizionario per il menu modelli ────────────────────────
            def _torna_da_modelli() -> None:
                print("\n  Ritorno al menu principale...")
                sys.exit(2)

            def _imposta_tutti_gpu() -> None:
                nonlocal dispositivo
                dispositivo = "GPU"
                for r in _RL:
                    _dispositivi_agenti[r] = "GPU"
                _ridisegna(f"✓ Tutti → 🎮 GPU ({_GP_NOME})")

            def _imposta_tutti_cpu() -> None:
                nonlocal dispositivo
                dispositivo = "CPU"
                for r in _RL:
                    _dispositivi_agenti[r] = "CPU"
                _ridisegna("✓ Tutti → 🖥 CPU (solo processore)")

            def _imposta_mix() -> None:
                nonlocal dispositivo
                # Alterna GPU/CPU: ricercatore GPU, pianificatore CPU, programmatore GPU...
                for i, r in enumerate(_RL):
                    _dispositivi_agenti[r] = "GPU" if i % 2 == 0 else "CPU"
                dispositivo = "GPU"  # maggioranza (3 GPU su 5)
                _ridisegna(f"✓ Mix → 🎮 GPU/CPU alternati ({_GP_NOME} + CPU)")

            _CONF_AZIONI: dict = {
                '0':         _torna_da_modelli,
                'BACKSPACE': _torna_da_modelli,
                'G':         _imposta_tutti_gpu,
                'C':         _imposta_tutti_cpu,
                'M':         _imposta_mix,
            }

            _ridisegna()
            while True:
                val = _leggi_tasto_raw_ma()
                if val == 'ENTER':
                    break
                elif val in _CONF_AZIONI:
                    _CONF_AZIONI[val]()
                elif val.isdigit() and 1 <= int(val) <= len(_RL):
                    # ── Sotto-menu "ripensamento" per agente ──────────────────
                    ruolo_da_cambiare = _RL[int(val) - 1]
                    icona_rc, _ = _RUOLI_INFO[ruolo_da_cambiare]
                    mod_attuale  = scelti[ruolo_da_cambiare]
                    disp_attuale = _dispositivi_agenti[ruolo_da_cambiare]
                    disp_icon    = "🎮 GPU" if disp_attuale == "GPU" else "🖥  CPU"

                    while True:
                        os.system("cls" if sys.platform == "win32" else "clear")
                        _c_rip = _sh_ma.get_terminal_size(fallback=(120, 40)).columns
                        _k_rip = _Con_ma(width=_c_rip)
                        _stampa_h(_k_rip,
                            breadcrumb=f"Prismalux › 🤖 Multi-Agente › ⚙️  Modelli › {icona_rc}")
                        if _RICH_OK and _rich:
                            from rich.panel import Panel as _PRip
                            from rich.text import Text as _TRip
                            _sx_rip = _TRip()
                            _sx_rip.append(f"\n  Agente selezionato:\n\n", style="dim")
                            _sx_rip.append(f"  {icona_rc}\n\n", style="bold cyan")
                            _sx_rip.append(f"  Modello attuale :  ", style="dim")
                            _sx_rip.append(f"{mod_attuale}\n", style="bold white")
                            _sx_rip.append(f"  Dispositivo     :  ", style="dim")
                            _sx_rip.append(f"{disp_icon}\n\n", style="bold green" if disp_attuale == "GPU" else "bold yellow")
                            _sx_rip.append("  ────────────────────────────────\n\n", style="dim")
                            _sx_rip.append("  [M]  Cambia modello\n", style="bold cyan")
                            _sx_rip.append(f"  [G]  Sposta su GPU  🎮\n", style="bold green")
                            _sx_rip.append(f"  [C]  Sposta su CPU  🖥\n", style="bold yellow")
                            _sx_rip.append("  [0 / Invio]  Annulla\n", style="dim")
                            _dx_rip = _TRip()
                            _dx_rip.append("  ─────────────────\n\n", style="dim")
                            _dx_rip.append("  M   modello\n",   style="dim")
                            _dx_rip.append("  G   → GPU\n",     style="bold green")
                            _dx_rip.append("  C   → CPU\n",     style="bold yellow")
                            _dx_rip.append("  0   annulla\n",   style="dim")
                            _body_rip = _Tab_ma.grid(expand=True)
                            _body_rip.add_column(ratio=3)
                            _body_rip.add_column(min_width=22, ratio=1)
                            _body_rip.add_row(
                                _PRip(_sx_rip,
                                      title=f"[bold cyan]⚙️  Configura agente[/]",
                                      border_style="bold cyan", padding=(1, 2)),
                                _PRip(_dx_rip, border_style="cyan", padding=(1, 1)),
                            )
                            _k_rip.print(_body_rip)
                            _k_rip.print(_Rule_ma(style="dim"))
                        else:
                            print(f"\n  Agente: {icona_rc}")
                            print(f"  Modello: {mod_attuale}  Dispositivo: {disp_icon}")
                            print("  [M]=modello  [G]=GPU  [C]=CPU  [0]=annulla")

                        if sys.platform == "win32":
                            sys.stdout.write("  Scelta [M+Invio / G+Invio / C+Invio / 0+Invio]: ")
                        else:
                            sys.stdout.write("  Scelta [M / G / C / 0]: ")
                        sys.stdout.flush()

                        try:
                            _sc_rip = input("").strip().upper()
                        except (EOFError, KeyboardInterrupt):
                            _sc_rip = "0"

                        if _sc_rip in ("0", "", "ENTER"):
                            _ridisegna()   # ridisegna schermata principale modelli
                            break

                        elif _sc_rip == "G":
                            _dispositivi_agenti[ruolo_da_cambiare] = "GPU"
                            disp_attuale = "GPU"
                            disp_icon    = "🎮 GPU"
                            _ridisegna(f"✓ {icona_rc} spostato su 🎮 GPU")
                            break

                        elif _sc_rip == "C":
                            _dispositivi_agenti[ruolo_da_cambiare] = "CPU"
                            disp_attuale = "CPU"
                            disp_icon    = "🖥  CPU"
                            _ridisegna(f"✓ {icona_rc} spostato su 🖥 CPU")
                            break

                        elif _sc_rip == "M":
                            # Selezione nuovo modello (usa _display_order = ordine visualizzato)
                            _ridisegna(f"Inserisci n° modello per {icona_rc} (1–{len(_display_order)}):")
                            sys.stdout.write("  n° ❯ ")
                            sys.stdout.flush()
                            try:
                                n = int(input("").strip()) - 1
                                if 0 <= n < len(_display_order):
                                    _nome_scelto = _display_order[n]['nome']
                                    scelti[ruolo_da_cambiare] = _nome_scelto
                                    mod_attuale = _nome_scelto
                                    _ridisegna(f"✓ {icona_rc} → {_nome_scelto}  ({disp_icon})")
                                    break
                                else:
                                    _ridisegna("⚠️  Numero fuori range. Riprova.")
                            except (ValueError, EOFError):
                                _ridisegna("⚠️  Input non valido. Riprova.")
                        else:
                            _ridisegna("⚠️  Premi M=modello · G=GPU · C=CPU · 0=annulla")
                else:
                    _ridisegna("⚠️  Premi 1–5 agente · G=tutti GPU · C=tutti CPU · M=mix · INVIO=ok · ⌫=torna")

            # Il "dispositivo" globale riflette la maggioranza
            n_gpu = sum(1 for d in _dispositivi_agenti.values() if d == "GPU")
            dispositivo = "GPU" if n_gpu >= 3 else "CPU"

            # Riavvia Ollama principale solo se tutti su GPU o tutti su CPU
            if all(d == "CPU" for d in _dispositivi_agenti.values()):
                if dispositivo != disp_salvato:
                    _applica_dispositivo("CPU")
            elif all(d == "GPU" for d in _dispositivi_agenti.values()):
                if disp_salvato == "CPU":
                    _applica_dispositivo("GPU")

            salva_preferenze(scelti, dispositivo, _dispositivi_agenti)

            # Costruisce mappa porte basata sul dispositivo per-agente
            porte = {
                ruolo: PORTA_NVIDIA if _dispositivi_agenti[ruolo] == "GPU" else PORTA_CPU
                for ruolo in scelti
            }
            return scelti, porte, dispositivo

        # ── Scheda 3: Task input ────────────────────────────────────────────────
        def _preriscalda_modello_ma(modello: str, porta: int) -> None:
            """Carica il modello in RAM Ollama con richiesta vuota (keep-alive)."""
            try:
                import requests as _req_pw
                _req_pw.post(
                    f"http://localhost:{porta}/api/generate",
                    json={"model": modello, "prompt": "", "keep_alive": "10m"},
                    timeout=30,
                )
            except Exception:
                pass

        def _disegna_task_ma(memoria_in: dict) -> None:
            os.system("cls" if sys.platform == "win32" else "clear")
            _c3 = _sh_ma.get_terminal_size(fallback=(120, 40)).columns
            _k3 = _Con_ma(width=_c3)
            _stampa_h(_k3, breadcrumb="Prismalux › 🤖 Multi-Agente AI › 📝 Task")
            sx3 = _Tex_ma()
            task_prec = memoria_in.get("task_eseguiti", [])
            if task_prec:
                sx3.append("  Task recenti:\n", style="dim")
                for t in reversed(task_prec[-5:]):
                    v = "✅" if t.get("verdict") == "PASSATO" else "❌"
                    sx3.append(f"  {v} {t['task'][:65]}\n", style="dim")
            else:
                sx3.append("  Primo task — nessuno storico.\n", style="dim italic")
            dx3 = _Tex_ma()
            dx3.append("  ─────────────────\n\n", style="dim")
            dx3.append("  INVIO  avvia\n",         style="bold green")
            dx3.append("  vuoto  esempio\n",        style="dim")
            dx3.append("  0      torna\n",          style="dim")
            dx3.append("  Q      esci\n",           style="dim")
            body3 = _Tab_ma.grid(expand=True)
            body3.add_column(ratio=3)
            body3.add_column(min_width=22, ratio=1)
            body3.add_row(
                _RichPanel(sx3, title="[bold magenta]📝 Descrivi il tuo task[/]",
                           border_style="bold magenta", padding=(1, 2)),
                _RichPanel(dx3, border_style="magenta", padding=(1, 1)),
            )
            _k3.print(body3)
            _k3.print(_Rule_ma(style="dim"))
            print("\n  Cosa vuoi realizzare?  (vuoto = esempio  |  0 = torna  |  Q = esci)")
            sys.stdout.write("  ❯ ")
            sys.stdout.flush()

        # ── Menu: TAB alterna modalità, ENTER avvia ────────────────────────────
        modalita_online = False
        gpu_attive = rileva_gpu_attive()
        _disegna_menu_ma(modalita_online, gpu_attive)

        def _toggle_modalita() -> None:
            nonlocal modalita_online
            modalita_online = not modalita_online
            _disegna_menu_ma(modalita_online, gpu_attive)

        _MENU_AZIONI: dict = {
            'TAB':       _toggle_modalita,
            'BACKSPACE': lambda: (print("\n  Ritorno al menu principale..."), sys.exit(2)),
            '0':         lambda: (print("\n  Ritorno al menu principale..."), sys.exit(2)),
            'Q':         lambda: (print("\n  🍺 Alla prossima libagione di sapere."), sys.exit(0)),
        }
        while True:
            _k = _leggi_tasto_raw_ma()
            if _k == 'ENTER':
                break
            fn = _MENU_AZIONI.get(_k)
            if fn:
                fn()

        # ── Memoria (prima dei modelli, per caricare le preferenze salvate) ───
        memoria = carica_memoria()

        # ── Scheda 2: Modelli + Dispositivo ───────────────────────────────────
        modelli, porte, dispositivo = _scheda_seleziona_modelli_ma(
            memoria, solo_locali=not modalita_online
        )

        # ── Preriscaldamento: carica il modello del ricercatore in RAM Ollama ─
        # Parte in background mentre l'utente digita il task (scheda 3),
        # così il primo agente trova il modello già pronto.
        import threading as _threading_ma
        _mod_pw  = modelli.get("ricercatore", "")
        _port_pw = porte.get("ricercatore", PORTA_NVIDIA)
        if _mod_pw:
            _threading_ma.Thread(
                target=_preriscalda_modello_ma,
                args=(_mod_pw, _port_pw),
                daemon=True,
            ).start()

        # ── Scheda 3: Task input ───────────────────────────────────────────────
        _disegna_task_ma(memoria)
        task = input("").strip()
        if task == "0":
            print("\n  Ritorno al menu principale...")
            sys.exit(2)
        elif task.upper() == "Q":
            print("\n  🍺 Alla prossima libagione di sapere.")
            sys.exit(0)
        elif not task:
            task = "un programma che calcola il pi greco con la formula di Leibniz"
            print(f"  Task di esempio: {task}")

        # ── Livello interlocutore ──────────────────────────────────────────────
        livello_utente = _chiedi_livello_utente()

        # ── Biblioteca prompt — sostituisce il task se l'utente sceglie un prompt ─
        prompt_dalla_lib = _menu_prompt_qualita(livello_utente)
        if prompt_dalla_lib:
            # Adatta il prompt al livello scelto aggiungendo un suffisso contestuale
            _suffisso_livello = {
                "scolastico":    " Spiega ogni passaggio con commenti chiari nel codice.",
                "universitario": " Usa buone pratiche Python (type hints, docstring, gestione errori).",
                "ricercatore":   " Massimizza efficienza algoritmica, usa strutture dati ottimali.",
            }
            task = prompt_dalla_lib + _suffisso_livello.get(livello_utente, "")

        # ── Ottimizzazione prompt con AI ───────────────────────────────────────
        # Usa il modello del ricercatore per ottimizzare (gia selezionato)
        _mod_ottimizza = modelli.get("ricercatore", "")
        _porta_ottimizza = porte.get("ricercatore", PORTA_NVIDIA)
        task = _chiedi_ottimizza_prompt(task, livello_utente, _mod_ottimizza, _porta_ottimizza)

        stato_iniziale: Stato = {
            "task":               task,
            "modelli":            modelli,
            "porte":              porte,
            "memoria":            memoria,
            "contesto_ricerca":   "",
            "piano_a":            "",
            "piano_b":            "",
            "codice_a":           "",
            "codice_b":           "",
            "codice_corrente":    "",
            "output_esecuzione":  "",
            "report_tester":      "",
            "verdict":            "",
            "tentativi":            0,
            "codice_ottimizzato":   "",
            "note_ottimizzazione":  "",
            "system_prompts":       {},
        }

        app = costruisci_grafo()

        # ── Avvia istanza CPU se necessario ────────────────────────────────────
        _cpu_agenti = [r for r, p in porte.items() if p == PORTA_CPU]
        if _cpu_agenti:
            _avvia_ollama_cpu()

        # ── Piano di esecuzione: GPU + CPU split ───────────────────────────────
        from rich.table import Table as _TabP
        from rich.rule import Rule as _RuleP
        _cols_p = _sh_ma.get_terminal_size(fallback=(120, 40)).columns
        _con_p  = _Con_ma(width=_cols_p)
        _ha_split = any(p == PORTA_CPU for p in porte.values())
        titolo_piano = ("🎮🖥  Piano di esecuzione (CPU+GPU paralleli)"
                        if _ha_split else "🎮  Piano di esecuzione GPU")
        _tab_piano = _TabP(title=f"[bold]{titolo_piano}[/]",
                           show_header=False, box=None, padding=(0, 2))
        _tab_piano.add_column()
        _tab_piano.add_column()
        _ruoli_ordine = ["ricercatore", "pianificatore", "programmatore",
                         "tester", "ottimizzatore"]
        for ruolo in _ruoli_ordine:
            p = porte.get(ruolo, PORTA_NVIDIA)
            icona_r, _ = _RUOLI_INFO[ruolo]
            if p == PORTA_CPU:
                _tab_piano.add_row(f"[cyan]{icona_r}[/]",
                                   f"[yellow]🖥  CPU :{p}[/]")
            else:
                emoji, nome_g, acc = _PORTA_A_GPU.get(p, ("⚪", "GPU", "?"))
                _tab_piano.add_row(f"[cyan]{icona_r}[/]",
                                   f"{emoji} [green]{nome_g}[/] [dim]({acc}) :{p}[/]")
        _con_p.print(_tab_piano)
        _con_p.print(_RuleP(style="dim"))

        _con_p.print("\n[bold yellow]🍺 Invocazione riuscita. Gli dei ascoltano.[/]")
        _con_p.print(_RuleP("[bold green]PIPELINE AVVIATA — ogni agente stampa il suo stato[/]",
                            style="green"))

        _t_pipeline = time.time()
        risultato = app.invoke(stato_iniziale)
        _durata_pipeline = time.time() - _t_pipeline

        # 2 + 6. Salva file e HTML (con conferma)
        path_py, path_html = _salva_con_conferma(risultato)

        # 5. Aggiorna memoria
        salva_memoria(memoria, risultato)

        # ── Bridge Cython — offri compilazione C ──────────────────────────────
        _offri_compilazione_cython(
            path_py=path_py,
            modello=modelli.get("ottimizzatore", modelli.get("programmatore", "")),
            porta=porte.get("ottimizzatore", PORTA_NVIDIA),
        )

        # ── Loop soddisfazione ────────────────────────────────────────────────
        task_corrente = task
        while True:
            from rich.panel import Panel as _PanR
            from rich.table import Table as _TabR
            from rich.rule import Rule as _RuleR
            from rich.text import Text as _TextR
            _cols_r = _sh_ma.get_terminal_size(fallback=(120, 40)).columns
            _con_r  = _Con_ma(width=_cols_r)
            minuti   = int(_durata_pipeline) // 60
            secondi  = _durata_pipeline % 60
            durata_str = f"{minuti}m {secondi:.0f}s" if minuti else f"{secondi:.1f}s"
            verdict_ok = risultato.get("verdict") == "PASSATO"
            verdict_str = "[bold green]✅ PASSATO[/]" if verdict_ok else "[bold red]❌ FALLITO[/]"
            task_str = risultato['task'][:80] + ("..." if len(risultato['task']) > 80 else "")

            _tab_ris = _TabR(show_header=False, box=None, padding=(0, 2))
            _tab_ris.add_column(style="dim", width=24)
            _tab_ris.add_column()
            _tab_ris.add_row("Task:",        f"[cyan]{task_str}[/]")
            _tab_ris.add_row("Tentativi:",   f"{risultato.get('tentativi', 0)}/{MAX_TENTATIVI}")
            _tab_ris.add_row("Verdict:",     verdict_str)
            _tab_ris.add_row("⏱  Durata:",   durata_str)
            if risultato.get("note_ottimizzazione"):
                _tab_ris.add_row("✨ Ottimizzazione:", risultato["note_ottimizzazione"])
            _ns = "(non salvato)"
            _tab_ris.add_row("Codice:",
                             f"[dim]{path_py}[/]" if path_py != _ns
                             else "[dim italic]non salvato[/]")
            _tab_ris.add_row("Report HTML:",
                             f"[dim]{path_html}[/]" if path_html != _ns
                             else "[dim italic]non salvato[/]")
            _tab_ris.add_row("Memoria:",     f"[dim]{FILE_MEMORIA}[/]")

            _con_r.print()
            _con_r.print(_PanR(_tab_ris, title="[bold]RIEPILOGO FINALE[/]",
                               border_style="green" if verdict_ok else "red",
                               padding=(1, 2)))

            _hint = _TextR()
            _hint.append("  [S / Invio]  ", style="bold green")
            _hint.append("Sì → torna al menu principale\n")
            _hint.append("  [⌫ Backspace] ", style="bold green")
            _hint.append("Sì → torna al menu principale\n")
            _hint.append("  [N]           ", style="bold yellow")
            _hint.append("No → suggerisco LLM migliori + miglioro il codice\n")
            _hint.append("  [Q]           ", style="bold red")
            _hint.append("Esci dal programma")
            _con_r.print(_PanR(_hint, title="[dim]Sei soddisfatto del risultato?[/]",
                               border_style="dim", padding=(0, 1)))
            _con_r.print(_RuleR(style="dim"))
            if sys.platform == "win32":
                sys.stdout.write("  Scelta [S+Invio=sì / N+Invio=no / Q+Invio=esci]: ")
            else:
                sys.stdout.write("  Scelta [S / ⌫ / N / Q]: ")
            sys.stdout.flush()
            soddisfatto = _leggi_tasto_raw_ma()

            print()   # a capo dopo il tasto
            if soddisfatto in ('Q',):
                print("\n  🍺 Alla prossima libagione di sapere.")
                sys.exit(0)

            elif soddisfatto in ('ENTER', 'BACKSPACE', 'S'):
                break  # torna al launcher

            elif soddisfatto == 'N':
                # ── Mostra LLM consigliati ─────────────────────────────────
                mostra_llm_consigliati(modelli)

                if _RICH_OK and _rich:
                    _rich.print("\n  [bold]Cosa vuoi migliorare o aggiungere al codice?[/]")
                    _rich.print("  [dim](es: 'aggiungi gestione errori', 'usa logging', vuoto=skip)[/]")
                else:
                    print("\n  Cosa vuoi migliorare?  (vuoto = skip)")
                aggiunta = input("\n  ❯ ").strip()
                if not aggiunta:
                    continue

                codice_prec = (risultato.get("codice_ottimizzato")
                               or risultato.get("codice_corrente", ""))
                task_base_miglioramento = (
                    f"{task_corrente}\n\n"
                    f"MIGLIORAMENTO RICHIESTO: {aggiunta}\n\n"
                    f"Codice precedente da migliorare:\n{codice_prec}"
                )

                # ── Loop auto-retry (max 3 tentativi: manuale → auto → upgrade) ─
                _modelli_iter = dict(modelli)
                _porte_iter   = dict(porte)
                _sp_iter: dict[str, str] = {}
                _tutti_nomi   = [m["nome"] for m in get_modelli_ollama() if m.get("size_gb", 0) > 0]

                for _iter in range(3):
                    from rich.rule import Rule as _RuleI
                    if _iter == 0:
                        if _RICH_OK and _rich:
                            _rich.print("\n  [bold cyan]♻️  Applico il miglioramento...[/]")
                            _rich.print(_RuleI(style="dim"))
                        msg_pipeline = "PIPELINE AVVIATA — miglioramento in corso..."
                    elif _iter == 1:
                        # auto-retry con hint "approccio alternativo"
                        if _RICH_OK and _rich:
                            _rich.print("\n  [bold yellow]🔄 Tentativo automatico 2/3 — approccio alternativo...[/]")
                            _rich.print(_RuleI(style="yellow"))
                        task_base_miglioramento += "\n\nNOTA: Il tentativo precedente ha fallito. Usa un approccio COMPLETAMENTE DIVERSO."
                        _sp_iter = _genera_system_prompts(task_corrente, risultato.get("report_tester", ""))
                        msg_pipeline = "PIPELINE 2/3 — approccio alternativo..."
                    else:
                        # Tentativo 3: chiedi upgrade modelli
                        if _RICH_OK and _rich:
                            _con_up = _Con_ma(width=_sh_ma.get_terminal_size(fallback=(120,40)).columns)
                            from rich.panel import Panel as _PanU
                            from rich.text import Text as _TxtU
                            _u = _TxtU()
                            _u.append("  [U] Upgrade modelli  ", style="bold magenta")
                            _u.append("→ switch a modelli più potenti + system prompt specifici\n")
                            _u.append("  [S] Continua stesso  ", style="bold green")
                            _u.append("→ riprova con gli stessi modelli\n")
                            _u.append("  [Q] Abbandona        ", style="bold red")
                            _u.append("→ torna al riepilogo")
                            _con_up.print(_PanU(_u,
                                title="[bold red]⚠  Ancora FALLITO dopo 2 tentativi[/]",
                                border_style="red", padding=(0,1)))
                            scelta_up = input("  Scelta [U/S/Q]: ").strip().upper()
                        else:
                            print("\n  ⚠  Ancora FALLITO dopo 2 tentativi.")
                            print("  [U] Upgrade modelli  [S] Continua  [Q] Abbandona")
                            scelta_up = input("  Scelta: ").strip().upper()

                        if scelta_up == "Q":
                            break
                        elif scelta_up == "U":
                            # Upgrade: modelli più potenti + system prompt specifici
                            _modelli_iter = _upgrade_modelli(_modelli_iter, _tutti_nomi)
                            _porte_iter   = {r: PORTA_NVIDIA for r in _modelli_iter}
                            _sp_iter      = _genera_system_prompts(task_corrente, risultato.get("report_tester", ""))
                            # mostra upgrade
                            if _RICH_OK and _rich:
                                _rich.print("\n  [bold magenta]🚀 Upgrade modelli:[/]")
                                for r, m in _modelli_iter.items():
                                    ic, _ = _RUOLI_INFO[r]
                                    old_m = modelli.get(r, "?")
                                    cambio = f"[dim]{old_m}[/] → [bold]{m}[/]" if m != old_m else f"[dim]{m} (invariato)[/]"
                                    _rich.print(f"    {ic}  {cambio}")
                                _rich.print("  [dim]System prompt specifici attivati per ogni ruolo.[/]")
                            msg_pipeline = "PIPELINE 3/3 — modelli potenziati + system prompt specifici..."
                        else:
                            # S: stessi modelli, terzo tentativo
                            _sp_iter = _genera_system_prompts(task_corrente, risultato.get("report_tester", ""))
                            msg_pipeline = "PIPELINE 3/3 — terzo tentativo..."

                    if _RICH_OK and _rich:
                        _rich.print(_RuleI(f"[bold green]{msg_pipeline}[/]", style="green"))
                    else:
                        print(f"\n{'═'*60}\n  {msg_pipeline}\n{'═'*60}")

                    _stato_iter: Stato = {
                        "task":               task_base_miglioramento,
                        "modelli":            _modelli_iter,
                        "porte":              _porte_iter,
                        "memoria":            memoria,
                        "system_prompts":     _sp_iter,
                        "contesto_ricerca":   "",
                        "piano_a":            "",
                        "piano_b":            "",
                        "codice_a":           "",
                        "codice_b":           "",
                        "codice_corrente":    "",
                        "output_esecuzione":  "",
                        "report_tester":      "",
                        "verdict":            "",
                        "tentativi":          0,
                        "codice_ottimizzato": "",
                        "note_ottimizzazione": "",
                    }
                    _t_iter = time.time()
                    risultato = app.invoke(_stato_iter)
                    _durata_pipeline = time.time() - _t_iter
                    path_py, path_html = _salva_con_conferma(risultato)
                    salva_memoria(memoria, risultato)
                    task_corrente = task_base_miglioramento

                    if risultato.get("verdict") == "PASSATO":
                        if _RICH_OK and _rich:
                            _rich.print("\n  [bold green]✅ PASSATO al tentativo automatico! Ottimo.[/]")
                        break  # esce dal for, torna al while (riepilogo)

                # fine for _iter — torna al while (mostra riepilogo)
        # ── Fine loop soddisfazione ───────────────────────────────────────────

    except KeyboardInterrupt:
        print("\n\n🍺 Alla prossima libagione di sapere.")


# ══════════════════════════════════════════════════════════════════════════════
# MOTORE BYZANTINO — Verifica a 4 agenti anti-allucinazione
# Pipeline: A (Originale) → B (Avvocato del Diavolo)
#         → C (Gemello Indipendente) → D (Giudice Quantico)
# Logica booleana: T = (A ∧ C) ∧ ¬B_valido
# ══════════════════════════════════════════════════════════════════════════════

def _bq_chiama_ollama(modello: str, porta: int, system_prompt: str, user_msg: str) -> str:
    """Chiama Ollama con streaming e restituisce la risposta completa."""
    url = f"http://127.0.0.1:{porta}/api/chat"
    payload = {
        "model": modello,
        "messages": [
            {"role": "system", "content": system_prompt},
            {"role": "user",   "content": user_msg},
        ],
        "stream": True,
    }
    risposta = []
    try:
        with requests.post(url, json=payload, stream=True, timeout=300) as r:
            r.raise_for_status()
            for line in r.iter_lines():
                if not line:
                    continue
                try:
                    chunk = json.loads(line)
                    token = chunk.get("message", {}).get("content", "")
                    if token:
                        risposta.append(token)
                        print(token, end="", flush=True)
                    if chunk.get("done"):
                        break
                except json.JSONDecodeError:
                    continue
    except requests.exceptions.ConnectionError:
        print("\n  ❌ Ollama non risponde. Avvia con: ollama serve")
        return ""
    except Exception as e:
        print(f"\n  ❌ Errore: {e}")
        return ""
    print()  # newline finale
    return "".join(risposta)


def motore_byzantino() -> None:
    """
    Motore Byzantino — verifica a 4 agenti anti-allucinazione.
    Implementa ByzantineQuantumEngine da ALLUCINAZIONI_RISOLTE.txt
    """
    import shutil as _sh_bq

    try:
        _root_bq = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        if _root_bq not in sys.path:
            sys.path.insert(0, _root_bq)
        from check_deps import stampa_header as _stampa_h_bq
        from rich.console import Console as _Con_bq
        from rich.panel import Panel as _Panel_bq
        from rich.text import Text as _Text_bq
        from rich.rule import Rule as _Rule_bq
        from rich.prompt import Prompt as _Prompt_bq
    except ImportError as e:
        print(f"\n  ❌ Import mancante: {e}\n  pip install rich requests")
        return

    _cols = _sh_bq.get_terminal_size(fallback=(120, 40)).columns
    _con  = _Con_bq(width=_cols)

    # ── Scelta modello e porta ─────────────────────────────────────────────────
    os.system("cls" if sys.platform == "win32" else "clear")
    _stampa_h_bq(_con)
    _con.print("\n  [bold cyan]🔮  Motore Byzantino — Verifica a 4 Agenti[/]")
    _con.print("[dim]  Pipeline: [cyan]A[/] (Esperto) → [red]B[/] (Avvocato) → "
               "[yellow]C[/] (Gemello) → [magenta]D[/] (Giudice)[/]")
    _con.print("[dim]  Logica: T = (A ∧ C) ∧ ¬B_valido[/]\n")

    # Recupera modelli disponibili (solo locali: i cloud crashano Ollama)
    tutti_modelli = get_modelli_ollama(PORTA_NVIDIA)
    if not tutti_modelli:
        _con.print("[bold red]  ❌ Ollama non risponde. Avvia con: ollama serve[/]")
        input("\n  [INVIO per tornare] ")
        return

    modelli_info = [m for m in tutti_modelli if m["size_gb"] > 0]
    if not modelli_info:
        _con.print("[bold red]  ❌ Nessun modello locale trovato. Esegui: ollama pull qwen3:4b[/]")
        input("\n  [INVIO per tornare] ")
        return

    nomi_modelli = [m["nome"] for m in modelli_info]
    modello_default = trova_default(nomi_modelli,
        ["qwen3:8b", "qwen3", "deepseek-r1:7b", "mistral", "llama3.2"])

    _con.print(f"  Modello: [cyan]{modello_default}[/]  [dim](INVIO = usa questo)[/]")
    scelta = _Prompt_bq.ask("  Modello", default=modello_default,
                             choices=nomi_modelli + [""], show_choices=False)
    modello = scelta.strip() or modello_default

    # ── Query utente ───────────────────────────────────────────────────────────
    _con.print()
    query = _Prompt_bq.ask(
        "  [bold yellow]❯ Inserisci la query da verificare[/]"
    ).strip()
    if not query or query in ("0", "esci", "q"):
        return

    sys_esperto = (
        "Sei un esperto altamente qualificato. Rispondi SEMPRE e SOLO in italiano "
        "in modo preciso e dettagliato. Fornisci solo fatti verificabili. "
        "Se non sei sicuro di qualcosa, dillo esplicitamente."
    )
    sys_avvocato = (
        "Sei l'Avvocato del Diavolo. Rispondi SEMPRE e SOLO in italiano. "
        "Il tuo ruolo è dimostrare che l'affermazione ricevuta è sbagliata o incompleta. "
        "Cerca contraddizioni, errori logici, storici o tecnici."
    )
    sys_gemello = (
        "Sei un validatore indipendente. Rispondi SEMPRE e SOLO in italiano. "
        "NON hai visto la risposta di altri agenti. Rispondi in modo autonomo e obiettivo. "
        "Sii preciso e fornisci solo fatti verificabili."
    )
    sys_giudice = (
        "Sei il Giudice Quantico Super-Osservatore. Rispondi SEMPRE e SOLO in italiano. "
        "Analizza il dibattito tra i tre agenti e emetti un verdetto finale "
        "basato sulla logica booleana. Sii imparziale, preciso e conciso."
    )

    # ── [A] Agente Originale / Esperto ────────────────────────────────────────
    os.system("cls" if sys.platform == "win32" else "clear")
    _stampa_h_bq(_con)
    _con.print(_Rule_bq("[bold cyan]🧠  [A] Agente Originale (Esperto)[/]"))
    out_a = _bq_chiama_ollama(modello, PORTA_NVIDIA, sys_esperto, query)

    # ── [B] Avvocato del Diavolo ──────────────────────────────────────────────
    _con.print(_Rule_bq("[bold red]⚔️   [B] Avvocato del Diavolo[/]"))
    prompt_b = (
        f"Hai ricevuto questa risposta da un esperto:\n\n{out_a}\n\n"
        "Il tuo UNICO scopo è trovare errori, contraddizioni, imprecisioni o punti deboli. "
        "Sii critico e severo. "
        "Se la risposta è completamente corretta, scrivi esattamente: 'NESSUN ERRORE RILEVATO'. "
        "Altrimenti elenca gli errori trovati."
    )
    out_b = _bq_chiama_ollama(modello, PORTA_NVIDIA, sys_avvocato, prompt_b)

    # ── [C] Gemello Indipendente ──────────────────────────────────────────────
    _con.print(_Rule_bq("[bold yellow]🔄  [C] Gemello Indipendente[/]"))
    out_c = _bq_chiama_ollama(modello, PORTA_NVIDIA, sys_gemello, query)

    # ── [D] Giudice Quantico ──────────────────────────────────────────────────
    _con.print(_Rule_bq("[bold magenta]⚖️   [D] Giudice Quantico (verdetto finale)[/]"))
    prompt_d = (
        f"Query originale: {query}\n\n"
        f"RISPOSTA A (Esperto Originale):\n{out_a}\n\n"
        f"RISPOSTA B (Avvocato del Diavolo):\n{out_b}\n\n"
        f"RISPOSTA C (Gemello Indipendente, non ha visto A):\n{out_c}\n\n"
        "Applica la logica booleana: T = (A concordante con C) AND (B non ha trovato errori validi).\n"
        "Emetti il verdetto:\n"
        "- Se T=1.0: scrivi 'VERDETTO: VERIFICATO' e una sintesi della risposta corretta.\n"
        "- Se T<1.0: scrivi 'VERDETTO: INCERTO' e spiega le discordanze rilevate."
    )
    out_d = _bq_chiama_ollama(modello, PORTA_NVIDIA, sys_giudice, prompt_d)

    # ── Collision score e verdetto visivo ─────────────────────────────────────
    b_senza_errori = ("NESSUN ERRORE" in out_b.upper())
    d_verificato   = ("VERIFICATO"    in out_d.upper())
    score = 1.0 if (b_senza_errori and d_verificato) else 0.5

    _con.print()
    if score >= 1.0:
        _con.print(_Panel_bq(
            "[bold green]✨  T = 1.0 — VERIFICATO\n"
            "La verità è confermata: A e C concordano, B non ha trovato errori.[/]",
            border_style="green"
        ))
    else:
        motivo = ("L'Avvocato ha sollevato obiezioni valide."
                  if not b_senza_errori
                  else "Discordanza tra A e C rilevata.")
        _con.print(_Panel_bq(
            f"[bold yellow]⚠   T = 0.5 — INCERTO\n{motivo}[/]",
            border_style="yellow"
        ))

    # ── Salvataggio opzionale ─────────────────────────────────────────────────
    salva = _Prompt_bq.ask(
        "\n  [dim]💾 Vuoi salvare il risultato?[/]",
        choices=["s", "n"], default="s"
    )
    if salva.lower() == "s":
        ts = datetime.now().strftime("%Y%m%d_%H%M%S")
        path_out = os.path.join(CARTELLA_OUTPUT, f"byzantino_{ts}.txt")
        try:
            with open(path_out, "w", encoding="utf-8") as _f:
                _f.write(f"=== PRISMALUX MOTORE BYZANTINO ===\n")
                _f.write(f"Data: {ts}\nQuery: {query}\nModello: {modello}\n")
                _f.write(f"Score: {score}  |  Verdetto: {'VERIFICATO' if d_verificato else 'INCERTO'}\n\n")
                _f.write(f"[A - Esperto Originale]\n{out_a}\n\n")
                _f.write(f"[B - Avvocato del Diavolo]\n{out_b}\n\n")
                _f.write(f"[C - Gemello Indipendente]\n{out_c}\n\n")
                _f.write(f"[D - Giudice Quantico]\n{out_d}\n")
            _con.print(f"  [green]✓ Salvato: {path_out}[/]")
        except OSError as e:
            _con.print(f"  [red]✗ Errore salvataggio: {e}[/]")

    input("\n  [INVIO per tornare] ")


# ── Wrapper sottomenu Agenti AI ───────────────────────────────────────────────

def menu_agenti() -> None:
    """Sottomenu: Pipeline 6 agenti | Motore Byzantino."""
    import shutil as _sh_ag
    try:
        _root_ag = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        if _root_ag not in sys.path:
            sys.path.insert(0, _root_ag)
        from check_deps import stampa_header as _stampa_h_ag
        from rich.console import Console as _Con_ag
    except ImportError:
        # fallback senza Rich
        while True:
            os.system("cls" if sys.platform == "win32" else "clear")
            print("\n  🤖  Agenti AI\n")
            print("  1  Pipeline 6 Agenti")
            print("  2  Motore Byzantino")
            print("  0  Torna\n")
            c = input("  Scelta: ").strip()
            if c == "1":
                avvia_menu()
            elif c == "2":
                motore_byzantino()
            elif c in ("0", "q", "Q", ""):
                break
        return

    while True:
        os.system("cls" if sys.platform == "win32" else "clear")
        _cols_ag = _sh_ag.get_terminal_size(fallback=(120, 40)).columns
        _con_ag  = _Con_ag(width=_cols_ag)
        _stampa_h_ag(_con_ag)

        from rich.text import Text as _Tx_ag
        t = _Tx_ag()
        t.append("\n  🤖  Agenti AI\n\n", style="bold cyan")
        t.append("  1️⃣   Pipeline 6 Agenti     ", style="bold")
        t.append("ricercatore → pianificatore → 2 prog → tester → ottimizzatore\n", style="dim")
        t.append("  2️⃣   Motore Byzantino       ", style="bold")
        t.append("verifica a 4 agenti anti-allucinazione  (T = (A∧C)∧¬B)\n", style="dim")
        t.append("  0️⃣   Torna\n", style="dim")
        _con_ag.print(t)

        if sys.platform == "win32":
            scelta = input("  Scelta: ").strip()
        else:
            import tty as _tty_ag, termios as _term_ag
            sys.stdout.write("  Scelta: ")
            sys.stdout.flush()
            fd = sys.stdin.fileno()
            old = _term_ag.tcgetattr(fd)
            try:
                _tty_ag.setraw(fd)
                scelta = sys.stdin.read(1)
            finally:
                _term_ag.tcsetattr(fd, _term_ag.TCSADRAIN, old)
            print(scelta)

        if scelta == "1":
            avvia_menu()
        elif scelta == "2":
            motore_byzantino()
        elif scelta in ("0", "q", "Q", "\x1b", ""):
            break


if __name__ == "__main__":
    avvia_menu()
