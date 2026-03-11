#!/usr/bin/env python3
"""
Prismalux — Avvio GPU Automatico
==================================
Rileva le GPU presenti e avvia le istanze Ollama sulle porte corrette:

  Linux:
    NVIDIA → 11434  (istanza principale — non viene toccata)
    Intel  → 11435  OLLAMA_LLM_LIBRARY=vulkan  OLLAMA_HOST=127.0.0.1:11435
    AMD    → 11436  OLLAMA_LLM_LIBRARY=rocm    OLLAMA_HOST=127.0.0.1:11436
                    (fallback Vulkan se ROCm non installato)

  Windows:
    NVIDIA → 11434  (CUDA, istanza principale)
    AMD/ATI → rilevata ma ROCm non supportato → opzione CPU mode (più veloce)
    Intel  → rilevata, supporto Vulkan limitato

Uso:
  python avvia_gpu.py           # rileva, avvia, chiede se lanciare Prismalux
  python avvia_gpu.py --auto    # come sopra ma avvia Prismalux senza chiedere
  python avvia_gpu.py --stop    # ferma le istanze avviate da questo script
  python avvia_gpu.py --cpu     # forza CPU mode (Windows AMD/ATI lenta)
"""

import os
import sys
import subprocess
import shutil
import time
import json
import importlib.util

# ── Configurazione GPU ────────────────────────────────────────────────────────

# Possibili percorsi ICD Vulkan su Linux (cerca in ordine)
_ICD_BASE = ["/usr/share/vulkan/icd.d", "/etc/vulkan/icd.d"]

def _trova_icd(*nomi: str) -> str:
    """Cerca il primo file ICD Vulkan disponibile (Linux only)."""
    if sys.platform == "win32":
        return ""   # Windows usa il runtime Vulkan di sistema, non file ICD
    for base in _ICD_BASE:
        for nome in nomi:
            p = os.path.join(base, nome)
            if os.path.exists(p):
                return p
    return ""


# Su Windows: AMD non supporta ROCm → istanze extra non avviate
_WIN = sys.platform == "win32"

GPU_CONFIG: dict[str, dict] = {
    "NVIDIA": {
        "porta":   11434,
        "vendor":  ["10de"],
        "emoji":   "🟢",
        "lib":     "cuda",
        "avvia":   False,   # NVIDIA: Ollama già la usa di default
        "icd":     "",      # CUDA non usa VK_ICD_FILENAMES
    },
    "Intel": {
        "porta":   11435,
        "vendor":  ["8086"],
        "emoji":   "🔵",
        "lib":     "vulkan",
        "avvia":   not _WIN,   # Windows: supporto istanze extra non affidabile
        "icd":     _trova_icd("intel_icd.json",
                              "intel_icd.x86_64.json",
                              "intel_hasvk_icd.json"),
    },
    "AMD": {
        "porta":   11436,
        "vendor":  ["1002"],
        "emoji":   "🔴",
        "lib":     "rocm",      # sostituito con "vulkan" se ROCm assente; "cpu" su Windows
        "avvia":   not _WIN,    # Windows: ROCm non supportato → usiamo CPU mode
        "icd":     _trova_icd("radeon_icd.json",
                              "radeon_icd.x86_64.json",
                              "amd_icd64.json"),
    },
}

FILE_PID       = os.path.expanduser("~/.prismalux_gpu_pids.json")
_FILE_SETTINGS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "settings.json")

# ── UI minimale (rich opzionale) ──────────────────────────────────────────────

_RICH = importlib.util.find_spec("rich") is not None
if _RICH:
    from rich.console import Console
    from rich.panel import Panel
    from rich.table import Table
    from rich.text import Text
    _con = Console()
else:
    _con = None

def _out(msg: str) -> None:
    if _RICH and _con:
        _con.print(msg)
    else:
        import re
        print(re.sub(r'\[/?[^\]]*\]', '', msg))

def _sep(char: str = "─", n: int = 56) -> str:
    return char * n

# ── Rilevamento GPU ───────────────────────────────────────────────────────────

def _lspci_gpu() -> list[str]:
    """Rileva GPU via lspci -mm -n (Linux, più affidabile)."""
    trovate: list[str] = []
    try:
        r = subprocess.run(["lspci", "-mm", "-n"],
                           capture_output=True, text=True, timeout=5)
        for riga in r.stdout.splitlines():
            parti = [p.strip().strip('"') for p in riga.split('"')]
            if len(parti) < 4:
                continue
            classe = parti[1].replace("Class ", "").lower()
            vendor = parti[3].lower().replace("0x", "")
            # Classi display: 0300 VGA, 0302 3D controller, 0380 Display
            if not any(classe.startswith(c) for c in ("0300", "0302", "0380")):
                continue
            for nome, cfg in GPU_CONFIG.items():
                if any(v in vendor for v in cfg["vendor"]) and nome not in trovate:
                    trovate.append(nome)
    except Exception:
        pass
    return trovate


def _sysfs_gpu() -> list[str]:
    """Fallback Linux: rileva GPU da /sys/bus/pci/devices/ (no lspci)."""
    trovate: list[str] = []
    base = "/sys/bus/pci/devices/"
    if not os.path.isdir(base):
        return trovate
    for dev in os.listdir(base):
        try:
            with open(os.path.join(base, dev, "class")) as f:
                classe = f.read().strip().lower()
            if not any(classe.startswith(c) for c in
                       ("0x0300", "0x0302", "0x0380")):
                continue
            with open(os.path.join(base, dev, "vendor")) as f:
                vendor = f.read().strip().lower().replace("0x", "")
            for nome, cfg in GPU_CONFIG.items():
                if any(v in vendor for v in cfg["vendor"]) and nome not in trovate:
                    trovate.append(nome)
        except Exception:
            continue
    return trovate


def _wmic_gpu() -> list[str]:
    """Rileva GPU su Windows via wmic (win32_VideoController)."""
    trovate: list[str] = []
    try:
        r = subprocess.run(
            ["wmic", "path", "win32_VideoController", "get", "name"],
            capture_output=True, text=True, timeout=5
        )
        if r.returncode != 0:
            return trovate
        for riga in r.stdout.splitlines():
            nome_gpu = riga.strip()
            if not nome_gpu or nome_gpu.lower() == "name":
                continue
            low = nome_gpu.lower()
            if "nvidia" in low and "NVIDIA" not in trovate:
                trovate.append("NVIDIA")
            elif any(k in low for k in ("amd", "radeon", "ati")) and "AMD" not in trovate:
                trovate.append("AMD")
            elif "intel" in low and "Intel" not in trovate:
                trovate.append("Intel")
    except Exception:
        pass
    return trovate


def _tool_conferma() -> list[str]:
    """Conferma NVIDIA/AMD via tool dedicati (nvidia-smi / rocm-smi)."""
    conferme: list[str] = []
    for cmd, nome in [(["nvidia-smi"], "NVIDIA"), (["rocm-smi"], "AMD")]:
        try:
            r = subprocess.run(cmd, capture_output=True, timeout=3)
            if r.returncode == 0 and nome not in conferme:
                conferme.append(nome)
        except Exception:
            pass
    return conferme


def rileva_gpu() -> list[str]:
    """Rileva GPU presenti. Usa WMIC su Windows, lspci+sysfs su Linux."""
    if sys.platform == "win32":
        trovate = _wmic_gpu()
        for nome in _tool_conferma():
            if nome not in trovate:
                trovate.append(nome)
        return trovate or ["NVIDIA"]

    # Linux / macOS
    trovate = _lspci_gpu()
    if not trovate:
        trovate = _sysfs_gpu()
    for nome in _tool_conferma():
        if nome not in trovate:
            trovate.append(nome)
    return trovate or ["NVIDIA"]


def rocm_disponibile() -> bool:
    try:
        return subprocess.run(["rocm-smi"], capture_output=True,
                              timeout=3).returncode == 0
    except Exception:
        return False


def _nvidia_stats() -> str:
    """Legge utilizzo GPU, VRAM usata/totale dalla GTX via nvidia-smi."""
    try:
        r = subprocess.run(
            ["nvidia-smi",
             "--query-gpu=name,utilization.gpu,memory.used,memory.total",
             "--format=csv,noheader,nounits"],
            capture_output=True, text=True, timeout=3
        )
        if r.returncode == 0:
            p = [x.strip() for x in r.stdout.strip().split(",")]
            if len(p) >= 4:
                return f"{p[0]}  GPU:{p[1]}%  VRAM:{p[2]}/{p[3]} MiB"
    except Exception:
        pass
    return ""


def _modelli_su_porta(porta: int) -> list[str]:
    """Ritorna i modelli disponibili su una porta Ollama."""
    import urllib.request, json as _json
    try:
        resp = urllib.request.urlopen(
            f"http://127.0.0.1:{porta}/api/tags", timeout=3)
        data = _json.loads(resp.read())
        return [m["name"] for m in data.get("models", [])]
    except Exception:
        return []


def mostra_verifica_gpu(gpu_trovate: list[str]) -> None:
    """Pannello di verifica: mostra GPU attive, modelli disponibili, stats."""
    sep = _sep()
    _out(f"\n  [bold cyan]{sep}[/]")
    _out("  [bold cyan]  Verifica GPU attive[/]")
    _out(f"  [bold cyan]{sep}[/]")

    # NVIDIA sempre mostrata (è quella principale)
    nv = _nvidia_stats()
    if nv:
        _out(f"  🟢 [bold]NVIDIA[/] [dim](CUDA  :{GPU_CONFIG['NVIDIA']['porta']})[/]")
        _out(f"     [green]{nv}[/]")
    else:
        _out(f"  🟢 [bold]NVIDIA[/] [dim](:{GPU_CONFIG['NVIDIA']['porta']})[/]  "
             f"[green]attiva[/]")

    modelli_nvidia = _modelli_su_porta(GPU_CONFIG["NVIDIA"]["porta"])
    if modelli_nvidia:
        _out(f"     Modelli: [dim]{', '.join(modelli_nvidia[:4])}"
             f"{'...' if len(modelli_nvidia) > 4 else ''}[/]")

    # GPU extra (Intel, AMD)
    for nome in gpu_trovate:
        cfg = GPU_CONFIG.get(nome)
        if not cfg or not cfg["avvia"]:
            continue
        porta = cfg["porta"]
        emoji = cfg["emoji"]
        if not porta_attiva(porta):
            _out(f"\n  {emoji} [bold]{nome}[/]  [red]❌ non risponde su :{porta}[/]")
            continue
        lib = cfg.get("lib", "vulkan")
        _out(f"\n  {emoji} [bold]{nome}[/] [dim]({lib.upper()}  :{porta})[/]  "
             f"[green]✅ attiva[/]")
        modelli = _modelli_su_porta(porta)
        if modelli:
            _out(f"     Modelli: [dim]{', '.join(modelli[:4])}"
                 f"{'...' if len(modelli) > 4 else ''}[/]")
        else:
            _out("     [dim]Nessun modello ancora scaricato su questa GPU[/]")
            _out(f"     [dim]→ Scarica con: OLLAMA_HOST=127.0.0.1:{porta} "
                 f"ollama pull deepseek-r1:1.5b[/]")

    _out(f"\n  [bold cyan]{sep}[/]\n")

# ── Stato istanze Ollama ──────────────────────────────────────────────────────

def porta_attiva(porta: int) -> bool:
    """Verifica se Ollama risponde su questa porta (usa solo stdlib)."""
    import urllib.request, urllib.error
    try:
        urllib.request.urlopen(
            f"http://127.0.0.1:{porta}/api/tags", timeout=2)
        return True
    except Exception:
        return False


def attendi_porta(porta: int, timeout: int = 20) -> bool:
    """Attende che Ollama sia pronto (max timeout secondi)."""
    scadenza = time.time() + timeout
    while time.time() < scadenza:
        if porta_attiva(porta):
            return True
        time.sleep(0.8)
    return False

# ── Avvio / stop istanze ──────────────────────────────────────────────────────

def _carica_pids() -> dict:
    if os.path.exists(FILE_PID):
        try:
            with open(FILE_PID) as f:
                return json.load(f)
        except Exception:
            pass
    return {}


def _salva_pids(pids: dict) -> None:
    with open(FILE_PID, "w") as f:
        json.dump(pids, f)


def _carica_settings() -> dict:
    """Legge settings.json dalla cartella del progetto."""
    try:
        with open(_FILE_SETTINGS) as f:
            return json.load(f)
    except Exception:
        return {}


def _salva_settings(aggiornamenti: dict) -> None:
    """Aggiorna settings.json senza sovrascrivere le chiavi esistenti."""
    try:
        correnti = _carica_settings()
        correnti.update(aggiornamenti)
        with open(_FILE_SETTINGS, "w") as f:
            json.dump(correnti, f, indent=2)
    except Exception:
        pass


def avvia_istanza(nome: str, cfg: dict) -> bool:
    """
    Avvia Ollama in background per la GPU indicata.
    Ritorna True se l'istanza è pronta, False se fallisce.
    """
    porta = cfg["porta"]

    # Scegli libreria (Linux)
    if nome == "AMD":
        lib = cfg["lib"] if rocm_disponibile() else "vulkan"
        if lib == "vulkan":
            _out("  [yellow]⚠  ROCm non trovato — uso Vulkan come fallback[/]")
    else:
        lib = cfg["lib"]

    env = os.environ.copy()
    env["OLLAMA_LLM_LIBRARY"] = lib
    env["OLLAMA_HOST"]         = f"127.0.0.1:{porta}"

    # Forza il driver Vulkan corretto (Linux only)
    if sys.platform != "win32":
        icd = cfg.get("icd", "")
        if icd:
            env["VK_ICD_FILENAMES"] = icd
            _out(f"  [dim]VK_ICD_FILENAMES={icd}[/]")
        else:
            env.pop("VK_ICD_FILENAMES", None)

    # Avvio processo: cross-platform
    kwargs: dict = {
        "env":    env,
        "stdout": subprocess.DEVNULL,
        "stderr": subprocess.DEVNULL,
    }
    if sys.platform == "win32":
        # Su Windows: DETACHED_PROCESS + CREATE_NEW_PROCESS_GROUP
        kwargs["creationflags"] = (
            subprocess.DETACHED_PROCESS | subprocess.CREATE_NEW_PROCESS_GROUP
        )
    else:
        kwargs["start_new_session"] = True   # sopravvive alla chiusura su Linux

    try:
        proc = subprocess.Popen(["ollama", "serve"], **kwargs)
    except FileNotFoundError:
        _out("  [red]❌  'ollama' non trovato nel PATH[/]")
        if sys.platform == "win32":
            _out("  [dim]Verifica che Ollama sia installato e nel PATH di Windows.[/]")
        return False

    # Salva PID per poterlo fermare dopo
    pids = _carica_pids()
    pids[nome] = proc.pid
    _salva_pids(pids)

    _out(f"  [dim]Avviato PID {proc.pid} — attendo risposta su :{porta}...[/]")
    return attendi_porta(porta)


def ferma_istanze() -> None:
    """Ferma le istanze avviate da questo script (legge i PID salvati)."""
    pids = _carica_pids()
    if not pids:
        _out("  Nessuna istanza da fermare (file PID non trovato).")
        return
    for nome, pid in pids.items():
        try:
            if sys.platform == "win32":
                # Windows: taskkill /F forza la chiusura
                r = subprocess.run(
                    ["taskkill", "/PID", str(pid), "/F"],
                    capture_output=True, timeout=5
                )
                if r.returncode == 0:
                    _out(f"  [cyan]⏹  {nome} (PID {pid}) fermato[/]")
                else:
                    _out(f"  [dim]{nome} (PID {pid}) già terminato[/]")
            else:
                os.kill(pid, 15)   # SIGTERM
                _out(f"  [cyan]⏹  {nome} (PID {pid}) fermato[/]")
        except ProcessLookupError:
            _out(f"  [dim]{nome} (PID {pid}) già terminato[/]")
        except Exception as e:
            _out(f"  [red]Errore nel fermare {nome}: {e}[/]")
    os.remove(FILE_PID)

# ── Main ──────────────────────────────────────────────────────────────────────

def _avviso_amd_windows() -> bool:
    """
    Su Windows con GPU AMD/ATI: mostra avviso e chiede se usare CPU mode.
    Ritorna True se l'utente sceglie CPU mode.
    """
    _out("\n  [bold yellow]⚠  GPU AMD/ATI rilevata su Windows[/]")
    _out("  [dim]ROCm non è supportato su Windows → Ollama usa la GPU via driver standard.[/]")
    _out("  [dim]Se la GPU è lenta (es. ATI vecchia), la CPU Xeon è molto più veloce.[/]")
    _out("\n  [bold]Scegli modalità Ollama:[/]")
    _out("  [cyan]  1.[/] GPU normale  (default Windows, usa la scheda video)")
    _out("  [cyan]  2.[/] CPU mode     (riavvio Ollama con CPU — consigliato se hai Xeon/i9)")
    _out("  [dim]  Invio = GPU normale[/]")
    val = input("\n  ❯ ").strip()
    return val == "2"


def _riavvia_ollama_cpu_win() -> bool:
    """
    Windows: ferma Ollama in esecuzione e lo riavvia in CPU mode.
    OLLAMA_NUM_GPU=0 deve essere impostato nel processo server, non nel nostro script.
    Ritorna True se il nuovo server risponde entro 25 secondi.
    """
    _out("\n  [cyan]⏹  Fermo il server Ollama in esecuzione...[/]")
    # Termina sia ollama.exe che l'eventuale tray app (ollama app.exe)
    for proc_name in ("ollama.exe", "ollama app.exe"):
        subprocess.run(
            ["taskkill", "/IM", proc_name, "/F"],
            capture_output=True
        )
    time.sleep(2)   # lascia tempo al socket di liberarsi

    # Cerca il binario Ollama nel PATH e nelle posizioni comuni
    ollama_bin = shutil.which("ollama")
    if not ollama_bin:
        for percorso in [
            r"C:\Users\Utente\AppData\Local\Programs\Ollama\ollama.exe",
            r"C:\Program Files\Ollama\ollama.exe",
            os.path.expanduser(r"~\AppData\Local\Programs\Ollama\ollama.exe"),
        ]:
            if os.path.isfile(percorso):
                ollama_bin = percorso
                break

    if not ollama_bin:
        _out("  [red]❌  'ollama' non trovato. Verifica che Ollama sia installato.[/]")
        return False

    env = os.environ.copy()
    env["OLLAMA_NUM_GPU"] = "0"   # forza inferenza CPU

    _out("  [cyan]🚀  Riavvio Ollama in CPU mode (OLLAMA_NUM_GPU=0)...[/]")
    try:
        subprocess.Popen(
            [ollama_bin, "serve"],
            env=env,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            creationflags=subprocess.DETACHED_PROCESS | subprocess.CREATE_NEW_PROCESS_GROUP,
        )
    except Exception as e:
        _out(f"  [red]❌  Impossibile avviare Ollama: {e}[/]")
        return False

    _out("  [dim]Attendo che il server sia pronto...[/]")
    ok = attendi_porta(11434, timeout=25)
    if ok:
        _out("  [green]✅  Ollama in CPU mode pronto su :11434[/]")
    else:
        _out("  [red]❌  Ollama non risponde entro 25s. Controlla che non ci siano errori.[/]")
    return ok


def main() -> None:
    auto_mode = "--auto" in sys.argv
    stop_mode = "--stop" in sys.argv
    cpu_mode  = "--cpu"  in sys.argv

    os.system("cls" if sys.platform == "win32" else "clear")
    _out(f"\n  [bold yellow]🍺  Prismalux — Avvio GPU Automatico[/]")
    _out(f"  [dim]{_sep()}[/]\n")

    # ── Modalità stop ─────────────────────────────────────────────────────────
    if stop_mode:
        _out("  [bold]Fermo le istanze Ollama secondarie...[/]\n")
        ferma_istanze()
        _out("\n  [dim]Premi INVIO per uscire.[/]")
        input()
        return

    # ── Rilevamento hardware ───────────────────────────────────────────────────
    _out("  [bold]Rilevamento GPU in corso...[/]")
    gpu_trovate = rileva_gpu()

    if _RICH and _con:
        t = Table(show_header=True, header_style="bold cyan",
                  show_lines=False, box=None, padding=(0, 2))
        t.add_column("GPU",     style="bold", width=12)
        t.add_column("Porta",   width=7)
        t.add_column("Lib",     width=14)
        t.add_column("Stato",   width=22)
    else:
        t = None

    righe: list[tuple] = []
    da_avviare: list[tuple[str, dict]] = []

    for nome in gpu_trovate:
        cfg  = GPU_CONFIG[nome]
        porta = cfg["porta"]
        emoji = cfg["emoji"]

        # Libreria effettiva
        if sys.platform == "win32":
            if nome == "NVIDIA":
                lib = "cuda"
            elif nome == "AMD":
                lib = "cpu (forzato)" if cpu_mode else "gpu (default)"
            else:
                lib = "gpu"
        elif nome == "AMD":
            lib = cfg["lib"] if rocm_disponibile() else "vulkan (fallback)"
        else:
            lib = cfg["lib"]

        if porta_attiva(porta):
            stato = ("[green]✅ attiva[/]", "✅ attiva")
        elif not cfg["avvia"]:
            stato = ("[dim]— gestita da Ollama[/]", "— Ollama default")
        else:
            stato = ("[yellow]⏳ da avviare[/]", "⏳ da avviare")
            da_avviare.append((nome, cfg))

        righe.append((f"{emoji} {nome}", str(porta), lib,
                      stato[0] if _RICH else stato[1]))

    if _RICH and _con and t:
        for r in righe:
            t.add_row(*r)
        _con.print(t)
    else:
        for r in righe:
            print(f"  {r[0]:<14} :{r[1]}  {r[2]:<22} {r[3]}")

    # ── Avviso AMD su Windows (se non già in --cpu mode) ─────────────────────
    if sys.platform == "win32" and not cpu_mode:
        gpu_amd = any(
            any(k in n.upper() for k in ("AMD", "RADEON", "ATI"))
            for n in gpu_trovate
        )
        if gpu_amd and "NVIDIA" not in gpu_trovate:
            cpu_mode = _avviso_amd_windows()

    # ── Se CPU mode: riavvia Ollama con OLLAMA_NUM_GPU=0 ─────────────────────
    # IMPORTANTE: impostare la variabile solo nel nostro script non basta —
    # Ollama server gira come processo separato. Bisogna fermarlo e riavviarlo
    # con OLLAMA_NUM_GPU=0 nel suo environment.
    if cpu_mode and sys.platform == "win32":
        ok = _riavvia_ollama_cpu_win()
        if not ok:
            _out("  [yellow]⚠  Continuo comunque, ma Ollama potrebbe usare la GPU.[/]\n")

    # GPU non in lista hardware ma con istanza già attiva (Prismalux le usa)
    for nome, cfg in GPU_CONFIG.items():
        if nome not in gpu_trovate and porta_attiva(cfg["porta"]):
            _out(f"\n  [dim]Nota: istanza Ollama attiva su :{cfg['porta']} "
                 f"ma GPU {nome} non rilevata nell'hardware[/]")

    # ── Avvio istanze mancanti (Linux/macOS) ──────────────────────────────────
    if da_avviare:
        _out(f"\n  [bold]Avvio {len(da_avviare)} istanza/e Ollama secondaria/e...[/]\n")
        fallite: list[str] = []
        for nome, cfg in da_avviare:
            _out(f"  {cfg['emoji']} [bold]{nome}[/]  →  porta {cfg['porta']}")
            ok = avvia_istanza(nome, cfg)
            if ok:
                _out(f"  [green]✅ {nome} pronta su :{cfg['porta']}[/]\n")
            else:
                _out(f"  [red]❌ {nome} non risponde entro 20s. "
                     f"Controlla i log di Ollama.[/]\n")
                fallite.append(nome)
        if fallite:
            _out(f"  [yellow]⚠  Istanze non avviate: {', '.join(fallite)}[/]")
            _out("  [dim]Prismalux userà solo le GPU attive.[/]\n")
    else:
        _out("\n  [green]Tutte le GPU rilevate hanno già un'istanza attiva.[/]\n")

    # ── Verifica e riepilogo finale ───────────────────────────────────────────
    mostra_verifica_gpu(gpu_trovate)
    attive = [f"{GPU_CONFIG[n]['emoji']}{n}:{GPU_CONFIG[n]['porta']}"
              for n in gpu_trovate if porta_attiva(GPU_CONFIG[n]["porta"])]
    _out(f"  [bold green]GPU operative: {' · '.join(attive)}[/]\n")

    # ── Avvia Prismalux ───────────────────────────────────────────────────────
    root = os.path.dirname(os.path.abspath(__file__))
    launcher = os.path.join(root, "AVVIA_Prismalux.py")

    if auto_mode:
        avvia = True
    else:
        settings = _carica_settings()
        pref = settings.get("avvia_auto")
        if pref is True:
            avvia = True
            _out("  [dim]Avvio automatico attivo (preferenza salvata in settings.json).[/]")
        elif pref is False:
            avvia = False
            _out("  [dim]Avvio automatico disattivato (preferenza salvata in settings.json).[/]")
        else:
            # Prima volta: chiedi e salva la risposta
            _out("  [dim]Suggerimento: rispondi 'sempre' per non chiedere più.[/]")
            risposta = input("  Avviare Prismalux adesso? [S/n/sempre]: ").strip().lower()
            if risposta in ("sempre", "always", "s sempre", "si sempre"):
                avvia = True
                _salva_settings({"avvia_auto": True})
                _out("  [dim]✓ Preferenza salvata: avvio automatico attivato.[/]")
            elif risposta in ("n no", "no sempre", "mai"):
                avvia = False
                _salva_settings({"avvia_auto": False})
                _out("  [dim]✓ Preferenza salvata: avvio automatico disattivato.[/]")
            else:
                avvia = risposta not in ("n", "no")

    if avvia and os.path.exists(launcher):
        _out("\n  [bold yellow]🍺 Avvio Prismalux...[/]\n")
        if sys.platform == "win32":
            # os.execv() su Windows può avere problemi con la console
            subprocess.run([sys.executable, launcher])
        else:
            os.execv(sys.executable, [sys.executable, launcher])
    elif avvia:
        _out(f"  [red]AVVIA_Prismalux.py non trovato in {root}[/]")
    else:
        _out("\n  [dim]Istanze Ollama avviate. Avvia Prismalux quando vuoi.[/]")
        cmd = "python avvia_gpu.py --stop" if sys.platform == "win32" else "python3 avvia_gpu.py --stop"
        _out(f"  [dim]Per fermarle: {cmd}[/]\n")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        _out("\n\n  [dim]Annullato.[/]")
