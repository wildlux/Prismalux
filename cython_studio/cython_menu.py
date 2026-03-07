"""
cython_studio/cython_menu.py
==============================
Menu interattivo per convertire file Python in Cython (.pyx),
compilarli in estensioni C ottimizzate (.so / .pyd) e confrontarne
le prestazioni con un benchmark rapido.

Voce 6 del menu principale di Prismalux.
"""
import os
import sys
import time
import shutil
import tempfile
import subprocess
import textwrap
import importlib.util

_CART = os.path.dirname(os.path.abspath(__file__))
_ROOT = os.path.dirname(_CART)
for _p in (_ROOT, _CART):
    if _p not in sys.path:
        sys.path.insert(0, _p)

from rich.console import Console
from rich.panel import Panel
from rich.text import Text
from rich.table import Table
from rich.syntax import Syntax
from rich.rule import Rule
from check_deps import stampa_header, risorse

console = Console()

# ── cartella di output per i file .pyx / .so ────────────────────────────────
_OUTPUT_DIR = os.path.join(_CART, "output")
os.makedirs(_OUTPUT_DIR, exist_ok=True)

# ── cartelle da escludere nella navigazione ──────────────────────────────────
_ESCLUDI_DIR  = {".venv", ".git", "__pycache__", "archivio", "build", "dist"}
_ESCLUDI_FILE = {"setup.py", "conf.py"}


# ─────────────────────────────────────────────────────────────────────────────
# Utilità
# ─────────────────────────────────────────────────────────────────────────────

def _cython_disponibile() -> bool:
    return shutil.which("cython") is not None or importlib.util.find_spec("Cython") is not None


def _installa_cython() -> bool:
    console.print("\n  [yellow]Cython non trovato. Installo...[/]")
    r = subprocess.run(
        [sys.executable, "-m", "pip", "install", "--quiet", "cython"],
        capture_output=True,
    )
    if r.returncode == 0:
        console.print("  [green]✓ Cython installato.[/]\n")
        return True
    console.print("  [red]✗ Installazione fallita. Prova manualmente: pip install cython[/]\n")
    return False


def _trova_py_ricorsivo(radice: str) -> list[str]:
    """Raccoglie tutti i .py sotto radice, saltando le cartelle escluse."""
    risultati = []
    for dirpath, dirnames, filenames in os.walk(radice):
        dirnames[:] = [d for d in dirnames if d not in _ESCLUDI_DIR]
        for f in sorted(filenames):
            if f.endswith(".py") and f not in _ESCLUDI_FILE:
                risultati.append(os.path.join(dirpath, f))
    return sorted(risultati)


def _rel(path: str) -> str:
    """Percorso relativo rispetto alla root del progetto."""
    try:
        return os.path.relpath(path, _ROOT)
    except ValueError:
        return path


def _leggi_file(path: str) -> str:
    try:
        with open(path, encoding="utf-8") as fh:
            return fh.read()
    except Exception as e:
        return f"# Errore lettura: {e}"


# ─────────────────────────────────────────────────────────────────────────────
# 1. Selezione file
# ─────────────────────────────────────────────────────────────────────────────

def _seleziona_file() -> str | None:
    """Mostra lista paginata dei .py e restituisce il percorso scelto."""
    tutti = _trova_py_ricorsivo(_ROOT)
    if not tutti:
        console.print("  [red]Nessun file .py trovato.[/]")
        input("  [INVIO] ")
        return None

    pagina, per_pag = 0, 20
    while True:
        console.clear()
        stampa_header(console, risorse())
        inizio = pagina * per_pag
        fine   = inizio + per_pag
        blocco = tutti[inizio:fine]
        tot_pag = (len(tutti) - 1) // per_pag

        t = Text()
        t.append(f"\n  File Python nel progetto  (pag. {pagina+1}/{tot_pag+1})\n\n",
                 style="bold cyan")
        for i, p in enumerate(blocco, start=1):
            t.append(f"    {inizio+i:3}.  {_rel(p)}\n", style="white")
        t.append("\n    P = pagina prec.  N = pagina succ.  0 = annulla\n", style="dim")
        console.print(Panel(t, title="[bold cyan]📂 Seleziona file[/]", border_style="cyan"))

        try:
            raw = input("  Numero file: ").strip().lower()
        except (KeyboardInterrupt, EOFError):
            return None

        if raw == "0":
            return None
        if raw == "n" and pagina < tot_pag:
            pagina += 1
            continue
        if raw == "p" and pagina > 0:
            pagina -= 1
            continue
        if raw.isdigit():
            idx = int(raw) - 1
            if 0 <= idx < len(tutti):
                return tutti[idx]
        console.print("  [red]Scelta non valida.[/]")
        time.sleep(0.8)


# ─────────────────────────────────────────────────────────────────────────────
# 2. Analisi con suggerimenti Cython
# ─────────────────────────────────────────────────────────────────────────────

def _analizza_file(path: str) -> None:
    """Mostra il sorgente con suggerimenti su dove aggiungere tipo-annotazioni."""
    codice = _leggi_file(path)
    console.clear()
    stampa_header(console, risorse())
    console.print(Rule(f"[cyan]Analisi: {_rel(path)}[/]"))

    # Mostra il codice con syntax highlight
    syn = Syntax(codice, "python", theme="monokai", line_numbers=True)
    console.print(syn)

    # Suggerimenti automatici
    righe = codice.splitlines()
    suggerimenti = []
    for n, riga in enumerate(righe, start=1):
        s = riga.strip()
        if s.startswith("def ") and "self" not in s:
            suggerimenti.append((n, "cpdef", riga.strip()))
        elif s.startswith("def ") and "self" in s:
            suggerimenti.append((n, "cpdef (metodo)", riga.strip()))
        elif s.startswith("for ") and " in range(" in s:
            suggerimenti.append((n, "loop → cdef int i", riga.strip()))
        elif "= []" in s or "= {}" in s or "= ()" in s:
            suggerimenti.append((n, "tipizza variabile", riga.strip()))

    if suggerimenti:
        console.print(Rule("[yellow]Suggerimenti Cython[/]"))
        tbl = Table(show_header=True, header_style="bold yellow", box=None)
        tbl.add_column("Riga", style="cyan", width=6)
        tbl.add_column("Ottimizzazione", style="yellow", width=22)
        tbl.add_column("Codice originale", style="white")
        for n, sug, orig in suggerimenti[:30]:
            tbl.add_row(str(n), sug, orig[:80])
        console.print(tbl)
    else:
        console.print("\n  [green]Nessuna ottimizzazione ovvia trovata — il file è già snello.[/]")

    console.print()
    input("  [INVIO] per continuare ")


# ─────────────────────────────────────────────────────────────────────────────
# 3. Converti .py → .pyx
# ─────────────────────────────────────────────────────────────────────────────

_INTESTAZIONE_PYX = """\
# ─── File generato automaticamente da Prismalux Cython Studio ────────────────
# Passaggi consigliati per ottimizzare questo file:
#
#  1. Aggiungere dichiarazioni di tipo:
#       cdef int x = 0
#       cdef double[:] arr = np.array([1.0, 2.0])
#
#  2. Trasformare le funzioni Python pure in funzioni C-pura:
#       def  foo(x):     →  cpdef int foo(int x):
#
#  3. Disabilitare i controlli Python costosi dove sicuro:
#       # cython: boundscheck=False, wraparound=False
#
#  4. Usare typed memoryviews per array NumPy:
#       cdef double[:, :] mat
#
# Per compilare:
#   python cython_studio/setup_build.py build_ext --inplace
# ─────────────────────────────────────────────────────────────────────────────

"""

def _converti_pyx(path: str) -> str | None:
    """Copia il .py in output/ come .pyx con intestazione e annotazioni base."""
    nome_base = os.path.splitext(os.path.basename(path))[0]
    dest = os.path.join(_OUTPUT_DIR, nome_base + ".pyx")

    codice = _leggi_file(path)

    # Aggiunge dichiarazioni Cython di base sulle funzioni senza type hints
    righe_out = []
    for riga in codice.splitlines():
        s = riga.strip()
        # Suggerisce cpdef per funzioni semplici (non classmethod/staticmethod)
        if s.startswith("def ") and "self" not in s and "cls" not in s:
            righe_out.append("# OTTIMIZZA: considera 'cpdef <tipo_ritorno> " + s[4:])
        righe_out.append(riga)

    with open(dest, "w", encoding="utf-8") as fh:
        fh.write(_INTESTAZIONE_PYX)
        fh.write("\n".join(righe_out))

    console.print(f"\n  [green]✓ File .pyx creato:[/] {_rel(dest)}")
    return dest


# ─────────────────────────────────────────────────────────────────────────────
# 4. Compila .pyx → estensione C (.so / .pyd)
# ─────────────────────────────────────────────────────────────────────────────

_SETUP_TEMPLATE = """\
from setuptools import setup, Extension
from Cython.Build import cythonize
import sys, os

ext = Extension(
    name="{nome}",
    sources=["{pyx}"],
    extra_compile_args=["-O2"],
)

setup(
    name="{nome}",
    ext_modules=cythonize(
        ext,
        compiler_directives={{
            "language_level": "3",
            "boundscheck": False,
            "wraparound": False,
        }},
        annotate=True,   # genera {nome}.html con mappa colori
    ),
)
"""

def _compila_pyx(pyx_path: str) -> bool:
    """Compila il .pyx in una shared library usando Cython + setuptools."""
    if not _cython_disponibile():
        if not _installa_cython():
            return False

    nome_base = os.path.splitext(os.path.basename(pyx_path))[0]

    # Crea setup temporaneo nella stessa cartella dell'output
    setup_path = os.path.join(_OUTPUT_DIR, f"_setup_{nome_base}.py")
    with open(setup_path, "w", encoding="utf-8") as fh:
        fh.write(_SETUP_TEMPLATE.format(
            nome=nome_base,
            pyx=os.path.basename(pyx_path),
        ))

    console.print(f"\n  [yellow]⚙  Compilazione {nome_base}.pyx...[/]\n")

    cmd = [sys.executable, setup_path, "build_ext", "--inplace"]
    result = subprocess.run(
        cmd,
        cwd=_OUTPUT_DIR,
        capture_output=False,   # mostra output live
    )

    os.remove(setup_path)

    if result.returncode == 0:
        console.print(f"\n  [green]✅ Compilazione completata![/]")
        # Cerca il .so / .pyd prodotto
        for f in os.listdir(_OUTPUT_DIR):
            if f.startswith(nome_base) and (f.endswith(".so") or f.endswith(".pyd")):
                console.print(f"  [cyan]Estensione:[/] {f}")
        html = os.path.join(_OUTPUT_DIR, nome_base + ".html")
        if os.path.exists(html):
            console.print(f"  [cyan]Annotazione HTML:[/] {_rel(html)}")
        return True
    else:
        console.print("\n  [red]✗ Compilazione fallita. Controlla i messaggi sopra.[/]")
        return False


# ─────────────────────────────────────────────────────────────────────────────
# 5. Benchmark Python vs Cython
# ─────────────────────────────────────────────────────────────────────────────

_BENCH_CODICE = """\
# Funzione di test: somma da 0 a N
def somma_range(n):
    s = 0
    for i in range(n):
        s += i
    return s
"""

_BENCH_PYX = """\
# cython: language_level=3, boundscheck=False, wraparound=False
cpdef long long somma_range(long long n):
    cdef long long s = 0
    cdef long long i
    for i in range(n):
        s += i
    return s
"""

def _benchmark() -> None:
    """Compila e confronta una funzione di test Python vs Cython."""
    if not _cython_disponibile():
        if not _installa_cython():
            return

    console.clear()
    stampa_header(console, risorse())
    console.print(Rule("[bold yellow]⚡ Benchmark Python vs Cython[/]"))

    N = 10_000_000
    console.print(f"\n  Test: [cyan]somma_range({N:,})[/] — loop puro\n")

    # ── Python puro ──────────────────────────────────────────────────────────
    console.print("  [yellow]Esecuzione Python puro...[/]")
    t0 = time.perf_counter()
    s_py = sum(range(N))
    t_py = time.perf_counter() - t0
    console.print(f"  Risultato: {s_py:,}  — [bold]{t_py:.4f}s[/]")

    # ── Cython ───────────────────────────────────────────────────────────────
    tmpdir = tempfile.mkdtemp(prefix="prismalux_bench_")
    pyx_file = os.path.join(tmpdir, "_bench_cython.pyx")
    setup_file = os.path.join(tmpdir, "_bench_setup.py")

    with open(pyx_file, "w") as fh:
        fh.write(_BENCH_PYX)
    with open(setup_file, "w") as fh:
        fh.write(_SETUP_TEMPLATE.format(
            nome="_bench_cython",
            pyx="_bench_cython.pyx",
        ))

    console.print("\n  [yellow]Compilazione modulo Cython di test...[/]")
    r = subprocess.run(
        [sys.executable, setup_file, "build_ext", "--inplace"],
        cwd=tmpdir,
        capture_output=True,
        text=True,
    )

    if r.returncode != 0:
        console.print(f"\n  [red]Compilazione fallita:[/]\n{r.stderr[:600]}")
        shutil.rmtree(tmpdir, ignore_errors=True)
        input("\n  [INVIO] ")
        return

    # Carica modulo dinamico
    if tmpdir not in sys.path:
        sys.path.insert(0, tmpdir)

    try:
        import importlib
        spec = None
        for f in os.listdir(tmpdir):
            if f.startswith("_bench_cython") and (f.endswith(".so") or f.endswith(".pyd")):
                spec = importlib.util.spec_from_file_location("_bench_cython",
                                                               os.path.join(tmpdir, f))
                break
        if spec is None:
            raise ImportError("Modulo compilato non trovato.")
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)

        console.print("  [yellow]Esecuzione Cython compilato...[/]")
        t0 = time.perf_counter()
        s_cy = mod.somma_range(N)
        t_cy = time.perf_counter() - t0
        console.print(f"  Risultato: {s_cy:,}  — [bold]{t_cy:.4f}s[/]")

        # ── Tabella risultati ────────────────────────────────────────────────
        speedup = t_py / t_cy if t_cy > 0 else float("inf")
        console.print()
        tbl = Table(show_header=True, header_style="bold white", box=None)
        tbl.add_column("Versione",  style="cyan",  width=18)
        tbl.add_column("Tempo (s)", style="white", width=12)
        tbl.add_column("Speedup",   style="green", width=12)
        tbl.add_row("Python puro",    f"{t_py:.4f}", "1.0×")
        tbl.add_row("Cython compil.", f"{t_cy:.4f}", f"[bold green]{speedup:.1f}×[/]")
        console.print(tbl)

        if speedup >= 10:
            console.print("\n  [bold green]🚀 Eccellente! Cython è molto più veloce.[/]")
        elif speedup >= 2:
            console.print(f"\n  [green]✅ Buon miglioramento: {speedup:.1f}× più veloce.[/]")
        else:
            console.print("\n  [yellow]⚠  Poca differenza: aggiungi type hints nel .pyx per guadagnare di più.[/]")

    except Exception as e:
        console.print(f"\n  [red]Errore caricamento modulo: {e}[/]")
    finally:
        if tmpdir in sys.path:
            sys.path.remove(tmpdir)
        shutil.rmtree(tmpdir, ignore_errors=True)

    console.print()
    input("  [INVIO] per continuare ")


# ─────────────────────────────────────────────────────────────────────────────
# 6. Mostra file convertiti
# ─────────────────────────────────────────────────────────────────────────────

def _mostra_output() -> None:
    console.clear()
    stampa_header(console, risorse())
    console.print(Rule("[bold cyan]📋 File nella cartella output[/]"))

    if not os.path.exists(_OUTPUT_DIR):
        console.print("  (nessun file)")
        input("\n  [INVIO] ")
        return

    file_list = sorted(os.listdir(_OUTPUT_DIR))
    if not file_list:
        console.print("  (cartella vuota)")
    else:
        tbl = Table(show_header=True, header_style="bold white", box=None)
        tbl.add_column("#",       style="dim",   width=5)
        tbl.add_column("File",    style="cyan",  width=40)
        tbl.add_column("Dim.",    style="white", width=12)
        tbl.add_column("Tipo",    style="yellow",width=12)
        for i, f in enumerate(file_list, 1):
            fp = os.path.join(_OUTPUT_DIR, f)
            dim = os.path.getsize(fp)
            dim_s = f"{dim/1024:.1f} KB" if dim >= 1024 else f"{dim} B"
            ext = os.path.splitext(f)[1]
            tipo = {"pyx":"Cython src", ".so":"Estensione",
                    ".pyd":"Estensione", ".c":"C generato",
                    ".html":"Annotaz."}.get(ext, ext or "—")
            tbl.add_row(str(i), f, dim_s, tipo)
        console.print(tbl)
        console.print(f"\n  Cartella: [dim]{_OUTPUT_DIR}[/]")

    console.print()
    input("  [INVIO] per continuare ")


# ─────────────────────────────────────────────────────────────────────────────
# 7. Pulisci file compilati
# ─────────────────────────────────────────────────────────────────────────────

def _pulisci() -> None:
    console.clear()
    stampa_header(console, risorse())
    console.print(Rule("[bold red]🗑  Pulizia file compilati[/]"))

    estensioni_da_rimuovere = {".so", ".pyd", ".c", ".html"}
    da_rimuovere = []

    # File nella cartella output
    if os.path.exists(_OUTPUT_DIR):
        for f in os.listdir(_OUTPUT_DIR):
            if os.path.splitext(f)[1] in estensioni_da_rimuovere:
                da_rimuovere.append(os.path.join(_OUTPUT_DIR, f))

    # Cartella build (setuptools)
    build_dir = os.path.join(_OUTPUT_DIR, "build")
    if os.path.exists(build_dir):
        da_rimuovere.append(build_dir)

    if not da_rimuovere:
        console.print("\n  Nessun file da rimuovere.")
        input("  [INVIO] ")
        return

    console.print("\n  Verranno rimossi:\n")
    for p in da_rimuovere:
        console.print(f"    [red]·[/] {_rel(p)}")

    try:
        conf = input("\n  Confermi? [s/N] ").strip().lower()
    except (KeyboardInterrupt, EOFError):
        return

    if conf == "s":
        for p in da_rimuovere:
            try:
                if os.path.isdir(p):
                    shutil.rmtree(p)
                else:
                    os.remove(p)
            except Exception as e:
                console.print(f"  [red]Errore: {e}[/]")
        console.print("\n  [green]✅ Pulizia completata.[/]")
    else:
        console.print("\n  Annullato.")

    input("  [INVIO] ")


# ─────────────────────────────────────────────────────────────────────────────
# 8. Guida Cython integrata
# ─────────────────────────────────────────────────────────────────────────────

_GUIDA = """
╔══════════════════════════════════════════════════════════════════════════════╗
║              📚  GUIDA RAPIDA CYTHON  —  Prismalux Edition                  ║
╠══════════════════════════════════════════════════════════════════════════════╣
║                                                                              ║
║  COS'È CYTHON?                                                               ║
║  Cython è un superset di Python: un file .pyx è Python valido ma permette   ║
║  di aggiungere dichiarazioni di tipo C per compilare funzioni ottimizzate.   ║
║                                                                              ║
║  I 3 tipi di funzione:                                                       ║
║    def   foo(x):         → funzione Python normale (chiamabile da Python)    ║
║    cdef  int bar(int x): → funzione C pura (NON visibile da Python)          ║
║    cpdef int baz(int x): → ibrido: C internamente, visibile da Python ✓     ║
║                                                                              ║
║  DICHIARAZIONI DI TIPO:                                                      ║
║    cdef int     i = 0           # intero C                                   ║
║    cdef double  x = 3.14        # float 64-bit                               ║
║    cdef list    lst              # oggetto Python                             ║
║    cdef double[:] arr           # typed memoryview (array NumPy veloce)      ║
║                                                                              ║
║  DIRETTIVE COMPILATORE (in cima al .pyx):                                    ║
║    # cython: boundscheck=False   → no controllo indici (più veloce)          ║
║    # cython: wraparound=False    → no indici negativi (più veloce)           ║
║    # cython: cdivision=True      → divisione C anziché Python                ║
║    # cython: language_level=3    → Python 3 (obbligatorio)                   ║
║                                                                              ║
║  WORKFLOW TIPICO:                                                            ║
║    1. Copia il .py in .pyx (Voce 3 del menu)                                 ║
║    2. Aggiungi cdef/cpdef alle funzioni critiche                              ║
║    3. Compila con la Voce 4                                                  ║
║    4. Importa il modulo .so come un normale import Python                    ║
║                                                                              ║
║  ESEMPIO COMPLETO:                                                           ║
║    # mio_modulo.pyx                                                          ║
║    # cython: language_level=3, boundscheck=False                             ║
║                                                                              ║
║    cpdef double media(double[:] arr):                                        ║
║        cdef int n = arr.shape[0]                                             ║
║        cdef double s = 0.0                                                   ║
║        cdef int i                                                            ║
║        for i in range(n):                                                    ║
║            s += arr[i]                                                       ║
║        return s / n                                                          ║
║                                                                              ║
║  REGOLA D'ORO: ottimizza SOLO le funzioni lente (profila prima con cProfile) ║
╚══════════════════════════════════════════════════════════════════════════════╝
"""

def _guida() -> None:
    console.clear()
    stampa_header(console, risorse())
    console.print(_GUIDA)
    input("  [INVIO] per continuare ")


# ─────────────────────────────────────────────────────────────────────────────
# Menu principale Cython Studio
# ─────────────────────────────────────────────────────────────────────────────

def menu_cython() -> None:
    while True:
        console.clear()
        stampa_header(console, risorse())

        stato_cython = "[green]✓ disponibile[/]" if _cython_disponibile() \
                       else "[red]✗ non installato[/]"

        t = Text()
        t.append(f"\n      Cython: {stato_cython}\n\n", style="")
        t.append("      1.  📂  Seleziona e analizza un file Python\n\n",     style="bold cyan")
        t.append("      2.  🔍  Analisi approfondita (suggerimenti tipo)\n\n", style="bold magenta")
        t.append("      3.  📝  Converti .py → .pyx\n\n",                     style="bold cyan")
        t.append("      4.  ⚙️   Compila .pyx → estensione C (.so/.pyd)\n\n", style="bold magenta")
        t.append("      5.  ⚡  Benchmark Python vs Cython\n\n",               style="bold yellow")
        t.append("      6.  📋  Mostra file convertiti\n\n",                   style="bold cyan")
        t.append("      7.  🗑️   Pulisci file compilati\n\n",                  style="bold red")
        t.append("      8.  📚  Guida Cython rapida\n\n",                      style="bold green")
        t.append("      0.  ←   Torna al menu principale\n",                   style="dim")
        console.print(Panel(
            t,
            title="[bold yellow]⚡ Cython Studio — Velocizza il tuo Python[/]",
            border_style="yellow",
        ))

        try:
            scelta = input("  Scelta: ").strip()
        except (KeyboardInterrupt, EOFError):
            break

        if scelta == "0":
            break

        elif scelta == "1":
            path = _seleziona_file()
            if path:
                _analizza_file(path)

        elif scelta == "2":
            path = _seleziona_file()
            if path:
                _analizza_file(path)

        elif scelta == "3":
            path = _seleziona_file()
            if path:
                dest = _converti_pyx(path)
                if dest:
                    console.print(f"\n  Vuoi aprire il file con nano? [s/N] ", end="")
                    try:
                        r = input().strip().lower()
                        if r == "s" and shutil.which("nano"):
                            subprocess.run(["nano", dest])
                    except (KeyboardInterrupt, EOFError):
                        pass
                input("  [INVIO] ")

        elif scelta == "4":
            # Lista i .pyx disponibili
            pyx_files = [f for f in os.listdir(_OUTPUT_DIR) if f.endswith(".pyx")] \
                        if os.path.exists(_OUTPUT_DIR) else []
            if not pyx_files:
                console.print("\n  [red]Nessun file .pyx trovato. Prima esegui la voce 3.[/]")
                input("  [INVIO] ")
                continue
            console.print("\n  File .pyx disponibili:\n")
            for i, f in enumerate(pyx_files, 1):
                console.print(f"    {i}. {f}")
            try:
                raw = input("\n  Numero: ").strip()
                if raw.isdigit() and 1 <= int(raw) <= len(pyx_files):
                    pyx_path = os.path.join(_OUTPUT_DIR, pyx_files[int(raw)-1])
                    _compila_pyx(pyx_path)
                    input("\n  [INVIO] ")
            except (KeyboardInterrupt, EOFError):
                pass

        elif scelta == "5":
            _benchmark()

        elif scelta == "6":
            _mostra_output()

        elif scelta == "7":
            _pulisci()

        elif scelta == "8":
            _guida()

        else:
            console.print("  [red]Scelta non valida.[/]")
            time.sleep(0.5)
