"""
Cython Studio — Prismalux
=========================
Compila codice Python in C tramite Cython per massima velocita.

Flusso:
  1. Installa Cython via pip se mancante
  2. Opzionale: ottimizza il codice con Ollama → genera .pyx
  3. Crea setup.py cross-platform (Windows/Linux/Mac)
  4. Compila con cythonize (annotate=True) + build_ext --inplace
  5. Mostra report e percorso del modulo compilato

Dipendenze: pip install cython  (installato automaticamente)
Uso standalone:
  python cython_studio.py
Uso come bridge da multi_agente:
  from cython_studio.cython_studio import compila_per_multi_agente
"""

import os
import sys
import re
import subprocess
import importlib.util
import shutil
import glob as _glob_mod
from datetime import datetime

try:
    from rich.console import Console as _RC
    from rich.panel import Panel as _RP
    from rich.text import Text as _RT
    from rich.rule import Rule as _RR
    from rich.table import Table as _RTa
    _rich = _RC()
    _RICH = True
except ImportError:
    _rich = None
    _RICH = False

_ROOT_MA  = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
_CARTELLA = os.path.dirname(os.path.abspath(__file__))

# ── Utilita output ─────────────────────────────────────────────────────────────

def _print(msg: str, style: str = "") -> None:
    if _RICH and _rich:
        _rich.print(f"  [{style}]{msg}[/{style}]" if style else f"  {msg}")
    else:
        print(f"  {msg}", flush=True)

def _header(titolo: str) -> None:
    if _RICH and _rich:
        _rich.print(_RR(f"[bold cyan]{titolo}[/]", style="cyan"))
    else:
        print(f"\n{'─'*60}\n  {titolo}\n{'─'*60}", flush=True)

def _panel(titolo: str, corpo: str, bordo: str = "cyan") -> None:
    if _RICH and _rich:
        _rich.print(_RP(corpo.strip(), title=f"[bold]{titolo}[/]",
                        border_style=bordo, padding=(0, 1)))
    else:
        print(f"\n  [{titolo}]\n{corpo}")

# ── 1. Installa Cython ─────────────────────────────────────────────────────────

def installa_cython() -> bool:
    """Installa Cython via pip se non presente. Ritorna True se pronto."""
    if importlib.util.find_spec("Cython") is not None:
        return True
    _print("Cython non trovato — installazione in corso...", "yellow")
    ok = subprocess.run(
        [sys.executable, "-m", "pip", "install", "--quiet", "cython"],
        capture_output=True,
    ).returncode == 0
    if ok:
        _print("Cython installato correttamente.", "green")
    else:
        _print("Errore durante l'installazione di Cython. Riprova manualmente: pip install cython", "red")
    return ok

# ── 2. Ottimizza con Ollama ────────────────────────────────────────────────────

def ottimizza_con_ollama(codice_py: str, modello: str, porta: int = 11434) -> str:
    """
    Usa Ollama per convertire codice Python in Cython ottimizzato (.pyx).
    Ritorna il testo .pyx oppure '' in caso di errore.
    """
    try:
        import requests
        from langchain_ollama import ChatOllama
        from langchain_core.messages import SystemMessage, HumanMessage
    except ImportError:
        _print("requests/langchain-ollama mancante. Salto ottimizzazione AI.", "yellow")
        return ""

    _print(f"Ottimizzazione Cython con {modello}...", "cyan")

    prompt_sistema = (
        "Sei un esperto di Cython. Converti codice Python in Cython (.pyx) ottimizzato. "
        "Non aggiungere spiegazioni, scrivi SOLO il codice .pyx."
    )

    prompt_utente = f"""Converti questo codice Python in Cython ottimizzato (.pyx).

REGOLE OBBLIGATORIE:
1. Prima riga: # cython: language_level=3
2. Seconda riga: # cython: boundscheck=False, wraparound=False, cdivision=True
3. Aggiungi `cimport cython` e `from libc.math cimport ...` se usi math
4. Usa `cdef int/double/float/str` per TUTTE le variabili locali nei loop
5. Usa `cpdef` per funzioni chiamabili sia da Python che da C
6. Usa `cdef` per funzioni pure C (non chiamabili da Python)
7. Annota tutti i parametri: `def funzione(int n, double x) -> double:`
8. Per loop numerici intensivi: `for i in range(n):` con i definito come `cdef int i`
9. NON usare `prange` (richiede OpenMP, non garantito)
10. Mantieni IDENTICA la logica del programma

CODICE PYTHON DA CONVERTIRE:
```python
{codice_py}
```

Scrivi SOLO il codice .pyx completo e funzionante:"""

    try:
        llm = ChatOllama(
            model=modello,
            base_url=f"http://localhost:{porta}",
            num_predict=2000,
            temperature=0.1,
        )
        risposta = llm.invoke([
            SystemMessage(content=prompt_sistema),
            HumanMessage(content=prompt_utente),
        ]).content

        # Estrai blocco codice se in markdown
        match = re.search(r"```(?:cython|python)?\n(.*?)```", risposta, re.DOTALL)
        if match:
            return match.group(1).strip()
        return risposta.strip()

    except Exception as e:
        _print(f"Errore Ollama: {e}", "red")
        return ""

# ── 3. Genera .pyx base (senza AI) ────────────────────────────────────────────

def genera_pyx_base(codice_py: str, nome_modulo: str) -> str:
    """
    Genera un file .pyx minimo aggiungendo solo gli header Cython.
    Usato come fallback quando Ollama non e disponibile.
    """
    header = (
        f"# cython: language_level=3\n"
        f"# cython: boundscheck=False, wraparound=False, cdivision=True\n"
        f"# Modulo: {nome_modulo}  — generato da Prismalux Cython Studio\n"
        f"# Data: {datetime.now().strftime('%Y-%m-%d %H:%M')}\n\n"
        f"cimport cython\n\n"
    )
    return header + codice_py

# ── 4. Crea setup.py ───────────────────────────────────────────────────────────

def crea_setup_py(nome_pyx: str, cartella: str) -> str:
    """
    Crea setup.py nella stessa cartella del .pyx.
    Ritorna il percorso del file creato.
    """
    path_setup = os.path.join(cartella, "setup_cython.py")
    contenuto = f"""# setup_cython.py — generato da Prismalux Cython Studio
# Uso: python setup_cython.py build_ext --inplace
import sys
from setuptools import setup
from Cython.Build import cythonize

setup(
    ext_modules=cythonize(
        "{nome_pyx}",
        annotate=True,          # genera .html con annotazioni C/Python
        compiler_directives={{
            "language_level": 3,
            "boundscheck":    False,
            "wraparound":     False,
            "cdivision":      True,
        }},
    )
)
"""
    with open(path_setup, "w", encoding="utf-8") as f:
        f.write(contenuto)
    return path_setup

# ── 5. Compila .pyx ───────────────────────────────────────────────────────────

def compila_pyx(path_pyx: str) -> tuple[bool, str, str]:
    """
    Compila il file .pyx con cythonize + build_ext --inplace.
    Ritorna (successo, output_log, path_modulo_compilato).
    """
    cartella  = os.path.dirname(path_pyx)
    nome_pyx  = os.path.basename(path_pyx)
    path_setup = crea_setup_py(nome_pyx, cartella)

    _print("Compilazione in corso...", "cyan")

    # Controlla che il compilatore C sia disponibile
    compilatore_ok = True
    if sys.platform == "win32":
        # Cerca cl.exe (MSVC) o gcc (MinGW)
        cl = shutil.which("cl") or shutil.which("gcc")
        if not cl:
            _print("ATTENZIONE: nessun compilatore C trovato su Windows.", "yellow")
            _print("Installa Visual Studio Build Tools o MinGW-w64.", "yellow")
            compilatore_ok = False

    cmd = [sys.executable, path_setup, "build_ext", "--inplace"]
    proc = subprocess.run(
        cmd,
        capture_output=True,
        text=True,
        cwd=cartella,
    )

    log = (proc.stdout + "\n" + proc.stderr).strip()

    # Trova il modulo compilato generato
    modulo_compilato = ""
    ext = "pyd" if sys.platform == "win32" else "so"
    nome_base = os.path.splitext(nome_pyx)[0]
    pattern = os.path.join(cartella, f"{nome_base}*.{ext}")
    trovati = _glob_mod.glob(pattern)
    if trovati:
        modulo_compilato = trovati[0]

    successo = proc.returncode == 0 and bool(modulo_compilato)
    return successo, log, modulo_compilato

# ── 6. Pipeline completa ───────────────────────────────────────────────────────

def pipeline_cython(
    codice_py: str,
    nome_modulo: str,
    cartella_output: str,
    modello_ollama: str = "",
    porta_ollama: int = 11434,
    usa_ai: bool = True,
) -> dict:
    """
    Pipeline completa: Python → .pyx → compile → .so/.pyd
    Ritorna dict con: pyx_path, so_path, html_path, log, successo
    """
    os.makedirs(cartella_output, exist_ok=True)

    # Genera .pyx
    if usa_ai and modello_ollama:
        codice_pyx = ottimizza_con_ollama(codice_py, modello_ollama, porta_ollama)
        if not codice_pyx:
            _print("Fallback: genero .pyx base senza AI.", "yellow")
            codice_pyx = genera_pyx_base(codice_py, nome_modulo)
            usa_ai = False
    else:
        codice_pyx = genera_pyx_base(codice_py, nome_modulo)

    # Salva .pyx
    pyx_path = os.path.join(cartella_output, f"{nome_modulo}.pyx")
    with open(pyx_path, "w", encoding="utf-8") as f:
        f.write(codice_pyx)
    _print(f"File .pyx salvato: {pyx_path}", "green")

    # Compila
    if not installa_cython():
        return {
            "pyx_path": pyx_path, "so_path": "", "html_path": "",
            "log": "Cython non disponibile.", "successo": False,
        }

    successo, log, so_path = compila_pyx(pyx_path)

    # Cerca .html annotato
    nome_base = os.path.splitext(os.path.basename(pyx_path))[0]
    html_path = os.path.join(cartella_output, f"{nome_base}.html")
    if not os.path.exists(html_path):
        html_path = ""

    return {
        "pyx_path":  pyx_path,
        "so_path":   so_path,
        "html_path": html_path,
        "log":       log,
        "successo":  successo,
        "ai_usata":  usa_ai,
    }

# ── Bridge per Multi-Agente ────────────────────────────────────────────────────

def compila_per_multi_agente(
    path_py: str,
    modello_ollama: str = "",
    porta_ollama: int = 11434,
) -> dict:
    """
    API di bridge: prende un file .py prodotto dal multi-agente
    e lo compila in Cython. Ritorna il dict risultato della pipeline.
    """
    with open(path_py, encoding="utf-8") as f:
        codice_py = f.read()

    nome_base = os.path.splitext(os.path.basename(path_py))[0]
    # Pulisci nome per essere un identificatore Python valido
    nome_modulo = re.sub(r"[^a-zA-Z0-9_]", "_", nome_base)
    if nome_modulo[0].isdigit():
        nome_modulo = "m_" + nome_modulo

    cartella_output = os.path.dirname(path_py)

    return pipeline_cython(
        codice_py=codice_py,
        nome_modulo=nome_modulo,
        cartella_output=cartella_output,
        modello_ollama=modello_ollama,
        porta_ollama=porta_ollama,
        usa_ai=bool(modello_ollama),
    )

# ── Seleziona modello Ollama disponibile ───────────────────────────────────────

def _scegli_modello_ollama() -> tuple[str, int]:
    """Chiede all'utente un modello Ollama. Ritorna (modello, porta)."""
    try:
        import requests
        r = requests.get("http://localhost:11434/api/tags", timeout=3)
        if r.status_code == 200:
            modelli = [
                m["name"] for m in r.json().get("models", [])
                if m.get("size", 0) > 0
            ]
            if modelli:
                _print(f"Modelli disponibili: {', '.join(modelli[:8])}", "dim")
                _print("Scegli modello (vuoto = primo della lista):", "dim")
                scelta = input("  ❯ ").strip()
                if scelta and scelta in modelli:
                    return scelta, 11434
                return modelli[0], 11434
    except Exception:
        pass
    _print("Ollama non raggiungibile. Uso .pyx base senza AI.", "yellow")
    return "", 11434

# ── Menu Cython Studio ─────────────────────────────────────────────────────────

def _sfoglia_file_py() -> str:
    """Elenca i .py nella cartella multi_agente e nella root. Ritorna il percorso scelto."""
    cartelle_cerca = [
        os.path.join(_ROOT_MA, "multi_agente"),
        _ROOT_MA,
    ]
    file_trovati: list[str] = []
    for c in cartelle_cerca:
        if os.path.isdir(c):
            for f in sorted(os.listdir(c)):
                if f.endswith(".py") and not f.startswith("_") and f not in (
                    "AVVIA_Prismalux.py", "avvia_gpu.py", "check_deps.py",
                    "ollama_utils.py", "pratico.py", "apprendimento.py",
                ):
                    file_trovati.append(os.path.join(c, f))

    if not file_trovati:
        _print("Nessun file .py trovato.", "yellow")
        return ""

    _header("File Python disponibili")
    for i, p in enumerate(file_trovati, 1):
        nome = os.path.relpath(p, _ROOT_MA)
        _print(f"  {i:2}. {nome}", "dim")

    _print("Scegli numero (0 = annulla):", "dim")
    try:
        n = int(input("  ❯ ").strip())
        if 1 <= n <= len(file_trovati):
            return file_trovati[n - 1]
    except (ValueError, EOFError):
        pass
    return ""

def _mostra_risultato(ris: dict) -> None:
    """Mostra il riepilogo della compilazione."""
    _header("RISULTATO COMPILAZIONE CYTHON")
    ok = ris.get("successo", False)
    if _RICH and _rich:
        tab = _RTa(show_header=False, box=None, padding=(0, 2))
        tab.add_column(style="dim", width=20)
        tab.add_column()
        tab.add_row("Stato",    "[bold green]SUCCESS[/]" if ok else "[bold red]FALLITO[/]")
        tab.add_row("AI usata", "[green]Si[/]" if ris.get("ai_usata") else "[yellow]No (base)[/]")
        tab.add_row("File .pyx",  f"[dim]{ris.get('pyx_path', '—')}[/]")
        tab.add_row("Modulo .so/.pyd", f"[green]{ris.get('so_path', '—')}[/]" if ok else "[dim]non generato[/]")
        if ris.get("html_path"):
            tab.add_row("Annotazioni HTML", f"[dim]{ris['html_path']}[/]")
        _rich.print(_RP(tab, title="[bold cyan]Cython Studio — Riepilogo[/]",
                        border_style="green" if ok else "red", padding=(1, 2)))
    else:
        print(f"\n  Stato: {'SUCCESS' if ok else 'FALLITO'}")
        print(f"  File .pyx: {ris.get('pyx_path', '—')}")
        print(f"  Modulo:    {ris.get('so_path', '—')}")

    if not ok:
        log = ris.get("log", "")
        if log:
            _panel("Log compilazione", log[:1200], bordo="red")

    if ok:
        _print("Il modulo compilato si importa con:", "dim")
        nome = os.path.splitext(os.path.basename(ris.get("pyx_path", "")))[0]
        _print(f'  import {nome}', "cyan")
        if ris.get("html_path"):
            _print(f"Apri {ris['html_path']} nel browser per vedere le annotazioni C.", "dim")


def menu_cython_studio() -> None:
    """Menu interattivo del Cython Studio."""
    # Importa header Prismalux se disponibile
    try:
        if _ROOT_MA not in sys.path:
            sys.path.insert(0, _ROOT_MA)
        from check_deps import stampa_header as _sh
        from rich.console import Console as _Hcon
        _hcon = _Hcon()
        _sh(_hcon)
    except Exception:
        pass

    while True:
        if _RICH and _rich:
            t = _RT()
            t.append("\n  1.  Compila file .py (scegli da lista)\n\n",    style="bold cyan")
            t.append("  2.  Compila inserendo percorso manuale\n\n",       style="bold cyan")
            t.append("  3.  Info su Cython e ambiente\n\n",                style="bold green")
            t.append("  0.  Torna al menu principale\n",                   style="dim")
            _rich.print(_RP(t, title="[bold cyan]⚡ Cython Studio — Python → C[/]",
                            border_style="cyan"))
        else:
            print("\n  ⚡ Cython Studio")
            print("  1. Compila file .py (lista)")
            print("  2. Percorso manuale")
            print("  3. Info ambiente")
            print("  0. Torna")

        try:
            scelta = input("  Scelta: ").strip()
        except (KeyboardInterrupt, EOFError):
            break

        if scelta == "0":
            break

        elif scelta in ("1", "2"):
            if scelta == "1":
                path_py = _sfoglia_file_py()
            else:
                _print("Inserisci il percorso del file .py:", "dim")
                path_py = input("  ❯ ").strip().strip('"')

            if not path_py or not os.path.isfile(path_py):
                _print("File non trovato.", "red")
                input("  [INVIO] ")
                continue

            # Chiedi se usare AI
            _print("Usare Ollama per ottimizzare il codice Cython? [S/n]:", "dim")
            usa_ai_inp = input("  ❯ ").strip().lower()
            usa_ai = usa_ai_inp not in ("n", "no")

            modello, porta = ("", 11434)
            if usa_ai:
                modello, porta = _scegli_modello_ollama()

            with open(path_py, encoding="utf-8") as f:
                codice = f.read()

            nome_base  = os.path.splitext(os.path.basename(path_py))[0]
            nome_mod   = re.sub(r"[^a-zA-Z0-9_]", "_", nome_base)
            if nome_mod[:1].isdigit():
                nome_mod = "m_" + nome_mod
            cartella = os.path.dirname(path_py)

            ris = pipeline_cython(
                codice_py=codice,
                nome_modulo=nome_mod,
                cartella_output=cartella,
                modello_ollama=modello if usa_ai else "",
                porta_ollama=porta,
                usa_ai=usa_ai,
            )
            _mostra_risultato(ris)
            input("\n  [INVIO] per continuare ")

        elif scelta == "3":
            _header("Info ambiente Cython")
            cython_ok = importlib.util.find_spec("Cython") is not None
            compilatore = shutil.which("gcc") or shutil.which("cl") or "non trovato"
            python_v = f"{sys.version_info.major}.{sys.version_info.minor}.{sys.version_info.micro}"

            if _RICH and _rich:
                tab = _RTa(show_header=False, box=None, padding=(0, 2))
                tab.add_column(style="dim", width=22)
                tab.add_column()
                tab.add_row("Python", python_v)
                tab.add_row("Cython", "[green]installato[/]" if cython_ok else "[red]non installato[/]")
                tab.add_row("Compilatore C", compilatore)
                tab.add_row("Piattaforma", sys.platform)
                _rich.print(_RP(tab, title="[bold]Info ambiente[/]", border_style="dim"))
            else:
                print(f"  Python:       {python_v}")
                print(f"  Cython:       {'installato' if cython_ok else 'mancante'}")
                print(f"  Compilatore:  {compilatore}")
                print(f"  Piattaforma:  {sys.platform}")

            if not cython_ok:
                _print("Premi INVIO per installare Cython ora.", "yellow")
                input("  ❯ ")
                installa_cython()

            input("\n  [INVIO] ")

        else:
            _print("Scelta non valida.", "yellow")


if __name__ == "__main__":
    menu_cython_studio()
