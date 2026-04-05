"""
Prismalux — Punto di ingresso unico
=====================================
Avvio:  python3.14 AVVIA_Prismalux.py
"""
import os
import sys

_ROOT = os.path.dirname(os.path.abspath(__file__))
for _p in (_ROOT, os.path.join(_ROOT, "core")):
    if _p not in sys.path:
        sys.path.insert(0, _p)

# ── Bootstrap venv ─────────────────────────────────────────────────────────────
_VENV_DIR = os.path.join(_ROOT, ".venv")
if sys.platform == "win32":
    _VENV_PY  = os.path.join(_VENV_DIR, "Scripts", "python.exe")
    _VENV_PIP = os.path.join(_VENV_DIR, "Scripts", "pip.exe")
else:
    _VENV_PY  = os.path.join(_VENV_DIR, "bin", "python")
    _VENV_PIP = os.path.join(_VENV_DIR, "bin", "pip")

def _nel_venv() -> bool:
    return os.path.normcase(os.path.realpath(sys.executable)) == \
           os.path.normcase(os.path.realpath(_VENV_PY))

if not _nel_venv():
    _Y = "\033[93m"; _G = "\033[92m"; _X = "\033[0m"
    _REQUIREMENTS = os.path.join(_ROOT, "requirements.txt")
    if not os.path.exists(_VENV_PY):
        print(f"\n  {_Y}🍺 Prima invocazione: creo l'ambiente virtuale...{_X}")
        import subprocess as _sp
        _sp.run([sys.executable, "-m", "venv", _VENV_DIR], check=True)
        _sp.run([_VENV_PIP, "install", "--quiet", "--upgrade", "pip"], check=True)
        print(f"  {_Y}Installo dipendenze (qualche minuto)...{_X}")
        _sp.run([_VENV_PIP, "install", "--quiet", "-r", _REQUIREMENTS], check=True)
        print(f"  {_G}✓ Ambiente pronto!{_X}\n")
    try:
        os.execv(_VENV_PY, [_VENV_PY, __file__] + sys.argv[1:])
    except OSError:
        import subprocess as _sp
        sys.exit(_sp.run([_VENV_PY, __file__] + sys.argv[1:]).returncode)

# ─────────────────────────────────────────────────────────────────────────────
import importlib.metadata
import importlib.util
import subprocess

_PACCHETTI = [
    ("rich",             "rich"),
    ("requests",         "requests"),
    ("ddgs",             "ddgs"),
    ("pypdf",            "pypdf"),
    ("langgraph",        "langgraph"),
    ("langchain_ollama", "langchain-ollama"),
    ("langchain_core",   "langchain-core"),
]


def _auto_installa() -> None:
    _Y = "\033[93m"; _G = "\033[92m"; _R = "\033[91m"; _X = "\033[0m"
    mancanti = [(mod, pkg) for mod, pkg in _PACCHETTI
                if not importlib.util.find_spec(mod)]
    if not mancanti:
        return
    print(f"\n  {_Y}Dipendenze mancanti:{_X}")
    for _, pkg in mancanti:
        print(f"    · {pkg}")
    print(f"\n  Installare adesso? [S/n] ", end="", flush=True)
    if input().strip().lower() in ("n", "no"):
        print(f"\n  {_R}Installazione saltata.{_X}\n")
        return
    for mod, pkg in mancanti:
        print(f"  → {pkg} ... ", end="", flush=True)
        ok = subprocess.run(
            [sys.executable, "-m", "pip", "install", "--quiet", pkg],
            capture_output=True,
        ).returncode == 0
        print(f"{_G}✓{_X}" if ok else f"{_R}✗{_X}")
    print()


_auto_installa()

# ── Import Rich (garantito dopo auto-install) ─────────────────────────────────
from rich.console import Console
from rich.panel import Panel
from rich.text import Text
from rich.rule import Rule
from check_deps import stampa_header, risorse, leggi_tasto

console = Console()


def _add_path(subdir: str) -> None:
    if subdir not in sys.path:
        sys.path.insert(0, subdir)


# ── Manutenzione ──────────────────────────────────────────────────────────────

def _manutenzione() -> None:
    def _aggiorna_pacchetti():
        console.print("\n  Aggiornamento pip e pacchetti...\n")
        subprocess.run([sys.executable, "-m", "pip", "install", "--upgrade", "pip"])
        for mod, pkg in _PACCHETTI:
            if importlib.util.find_spec(mod):
                subprocess.run([sys.executable, "-m", "pip", "install", "--upgrade", pkg])
        console.print("\n  [green]✓ Completato.[/]")
        input("  [INVIO] ")

    def _aggiorna_ollama():
        if sys.platform != "win32":
            subprocess.run("curl -fsSL https://ollama.com/install.sh | sh", shell=True)
        else:
            console.print("  Windows: scarica l'installer da https://ollama.com/download")
        input("\n  [INVIO] ")

    def _pulisci_cache():
        import shutil
        subprocess.run([sys.executable, "-m", "pip", "cache", "purge"], capture_output=True)
        rimossi = 0
        for dirpath, dirnames, _ in os.walk(_ROOT):
            dirnames[:] = [d for d in dirnames if d not in (".venv", ".git")]
            for d in list(dirnames):
                if d == "__pycache__":
                    try:
                        shutil.rmtree(os.path.join(dirpath, d))
                        rimossi += 1
                    except Exception:
                        pass
        console.print(f"\n  [green]🧹 Cache pulita — {rimossi} cartelle __pycache__ rimosse.[/]")
        input("  [INVIO] ")

    def _aggiorna_modelli():
        try:
            r = subprocess.run(["ollama", "list"], capture_output=True, text=True, timeout=15)
            modelli = [riga.split()[0] for riga in r.stdout.strip().splitlines()[1:]
                       if riga.strip()]
            if not modelli:
                console.print("  ⚠  Nessun modello installato.")
            else:
                for nome in modelli:
                    console.print(f"  Aggiornamento: [cyan]{nome}[/]")
                    subprocess.run(["ollama", "pull", nome])
                console.print("\n  [green]✅ Tutti i modelli aggiornati.[/]")
        except FileNotFoundError:
            console.print("  [red]Ollama non trovato nel PATH.[/]")
        except Exception as e:
            console.print(f"  [red]Errore: {e}[/]")
        input("\n  [INVIO] ")

    def _pulisci_esportazioni():
        import shutil, glob
        cartella = os.path.join(_ROOT, "esportazioni")
        if not os.path.isdir(cartella):
            console.print("\n  [dim]Cartella esportazioni non trovata.[/]")
            input("  [INVIO] ")
            return

        # Conta e categorizza i file presenti
        tutti = [f for f in os.listdir(cartella) if os.path.isfile(os.path.join(cartella, f))]
        output_py   = [f for f in tutti if f.startswith("output_") and f.endswith(".py")]
        report_html = [f for f in tutti if f.startswith("report_") and f.endswith(".html")]
        quiz_json   = [f for f in tutti if f in ("quiz_database.json", "quiz_storico.json")]
        altri       = [f for f in tutti if f not in output_py + report_html + quiz_json]

        console.print(f"\n  Contenuto [cyan]esportazioni/[/]:")
        console.print(f"    Output multi-agente (.py)  : [yellow]{len(output_py)}[/] file")
        console.print(f"    Report HTML multi-agente   : [yellow]{len(report_html)}[/] file")
        console.print(f"    Dati quiz (DB + storico)   : [green]{len(quiz_json)}[/] file")
        if altri:
            console.print(f"    Altri                      : [dim]{len(altri)}[/] file")

        console.print("\n  Cosa vuoi cancellare?")
        console.print("  [yellow]1[/]  Output multi-agente (.py + .html)")
        console.print("  [red]2[/]  Dati quiz (azzera database e storico)")
        console.print("  [red]3[/]  Tutto (output + quiz)")
        console.print("  [dim]0[/]  Annulla")
        try:
            scelta = input("\n  Scelta: ").strip()
        except (KeyboardInterrupt, EOFError):
            return

        da_eliminare = []
        if scelta == "1":
            da_eliminare = output_py + report_html
        elif scelta == "2":
            da_eliminare = quiz_json
        elif scelta == "3":
            da_eliminare = output_py + report_html + quiz_json
        else:
            return

        if not da_eliminare:
            console.print("  [dim]Nessun file corrispondente trovato.[/]")
            input("  [INVIO] ")
            return

        console.print(f"\n  Eliminare [bold red]{len(da_eliminare)}[/] file? [S/n] ", end="")
        if input().strip().lower() in ("n", "no"):
            return

        rimossi = 0
        for nome in da_eliminare:
            try:
                os.remove(os.path.join(cartella, nome))
                rimossi += 1
            except Exception:
                pass
        console.print(f"\n  [green]✓ {rimossi} file rimossi da esportazioni/[/]")
        input("  [INVIO] ")

    def _pulisci_cache_compilazione():
        import shutil, glob
        rimossi = []

        # Estensioni e cartelle generate da Cython e dal compilatore C
        pattern_file = ["*.so", "*.pyd", "*.c", "*.html"]
        cartelle_build = ["build", "dist", "__cython_build__"]

        for pat in pattern_file:
            for percorso in glob.glob(os.path.join(_ROOT, "**", pat), recursive=True):
                # Salta .venv e .git
                parti = percorso.replace(_ROOT, "").split(os.sep)
                if any(p in {".venv", ".git"} for p in parti):
                    continue
                try:
                    os.remove(percorso)
                    rimossi.append(os.path.relpath(percorso, _ROOT))
                except Exception:
                    pass

        for nome_cartella in cartelle_build:
            for percorso in glob.glob(os.path.join(_ROOT, "**", nome_cartella), recursive=True):
                parti = percorso.replace(_ROOT, "").split(os.sep)
                if any(p in {".venv", ".git"} for p in parti):
                    continue
                try:
                    shutil.rmtree(percorso)
                    rimossi.append(os.path.relpath(percorso, _ROOT) + "/")
                except Exception:
                    pass

        if rimossi:
            console.print(f"\n  [green]⚡ Cache compilazione pulita — {len(rimossi)} elementi rimossi:[/]")
            for r in rimossi:
                console.print(f"    [dim]· {r}[/]")
        else:
            console.print("\n  [dim]Nessun file di compilazione trovato.[/]")
        input("\n  [INVIO] ")

    def _esegui_tutto():
        """Esegue in sequenza le operazioni 1-6 senza fermarsi per input."""
        import shutil, glob as _glob2, importlib as _imp
        console.print("\n  [bold yellow]⚡ Manutenzione completa — eseguo tutte le operazioni...[/]\n")

        # 1. Aggiorna pip
        console.print("  [bold cyan]─── 1. Aggiorna pip e pacchetti ───[/]")
        try:
            subprocess.run([sys.executable, "-m", "pip", "install", "--upgrade", "pip"], check=False)
            r = subprocess.run([sys.executable, "-m", "pip", "list", "--outdated", "--format=columns"],
                               capture_output=True, text=True)
            pkgs = [line.split()[0] for line in r.stdout.splitlines()[2:] if line.strip()]
            if pkgs:
                subprocess.run([sys.executable, "-m", "pip", "install", "--upgrade"] + pkgs, check=False)
            console.print(f"  [green]✓ Aggiornati {len(pkgs)} pacchetti.[/]")
        except Exception as e:
            console.print(f"  [red]Errore: {e}[/]")

        # 2. Aggiorna Ollama
        console.print("\n  [bold cyan]─── 2. Aggiorna Ollama ───[/]")
        try:
            if sys.platform != "win32":
                res = subprocess.run(["bash", "-c", "curl -fsSL https://ollama.com/install.sh | sh"],
                                     capture_output=True, text=True, timeout=120)
                console.print("  [green]✓ Ollama aggiornato.[/]" if res.returncode == 0 else "  [yellow]Ollama: aggiornamento non riuscito.[/]")
            else:
                console.print("  [yellow]Su Windows aggiorna Ollama manualmente da ollama.com[/]")
        except Exception as e:
            console.print(f"  [red]Errore: {e}[/]")

        # 3. Pulisci __pycache__ e cache pip
        console.print("\n  [bold cyan]─── 3. Pulisci __pycache__ e cache pip ───[/]")
        try:
            rimossi = 0
            for d in _glob2.glob(os.path.join(_ROOT, "**", "__pycache__"), recursive=True):
                shutil.rmtree(d, ignore_errors=True)
                rimossi += 1
            subprocess.run([sys.executable, "-m", "pip", "cache", "purge"], capture_output=True)
            console.print(f"  [green]✓ {rimossi} cartelle __pycache__ rimosse.[/]")
        except Exception as e:
            console.print(f"  [red]Errore: {e}[/]")

        # 4. Aggiorna modelli Ollama
        console.print("\n  [bold cyan]─── 4. Aggiorna modelli Ollama ───[/]")
        try:
            r = subprocess.run(["ollama", "list"], capture_output=True, text=True)
            modelli_ol = [l.split()[0] for l in r.stdout.splitlines()[1:] if l.strip()]
            for m in modelli_ol:
                subprocess.run(["ollama", "pull", m], capture_output=True)
            console.print(f"  [green]✓ {len(modelli_ol)} modelli Ollama aggiornati.[/]")
        except Exception as e:
            console.print(f"  [red]Ollama non raggiungibile: {e}[/]")

        # 5. Pulisci cache compilazione Cython
        console.print("\n  [bold cyan]─── 5. Pulisci cache Cython ───[/]")
        try:
            rimossi = []
            for pat in ["*.so", "*.pyd", "*.c", "*.html"]:
                for p in _glob2.glob(os.path.join(_ROOT, "**", pat), recursive=True):
                    parti = p.replace(_ROOT, "").split(os.sep)
                    if any(x in {".venv", ".git"} for x in parti):
                        continue
                    try:
                        os.remove(p)
                        rimossi.append(p)
                    except Exception:
                        pass
            for bd in ["build", "dist"]:
                for p in _glob2.glob(os.path.join(_ROOT, "**", bd), recursive=True):
                    parti = p.replace(_ROOT, "").split(os.sep)
                    if any(x in {".venv", ".git"} for x in parti):
                        continue
                    shutil.rmtree(p, ignore_errors=True)
            console.print(f"  [green]✓ {len(rimossi)} file compilazione rimossi.[/]")
        except Exception as e:
            console.print(f"  [red]Errore: {e}[/]")

        # 6. Pulisci esportazioni
        console.print("\n  [bold cyan]─── 6. Pulisci esportazioni ───[/]")
        try:
            cartella = os.path.join(_ROOT, "esportazioni")
            estensioni = {".py", ".html", ".json", ".txt", ".pyx", ".c", ".so", ".pyd"}
            da_eliminare = [f for f in os.listdir(cartella)
                            if os.path.splitext(f)[1].lower() in estensioni] if os.path.isdir(cartella) else []
            for nome in da_eliminare:
                try:
                    os.remove(os.path.join(cartella, nome))
                except Exception:
                    pass
            console.print(f"  [green]✓ {len(da_eliminare)} file rimossi da esportazioni/[/]")
        except Exception as e:
            console.print(f"  [red]Errore: {e}[/]")

        console.print("\n  [bold green]✓ Manutenzione completa eseguita.[/]")
        input("\n  [INVIO] ")

    _AZIONI_MAN = {
        "1": _aggiorna_pacchetti,
        "2": _aggiorna_ollama,
        "3": _pulisci_cache,
        "4": _aggiorna_modelli,
        "5": _pulisci_cache_compilazione,
        "6": _pulisci_esportazioni,
        "7": _esegui_tutto,
    }

    while True:
        console.clear()
        stampa_header(console, risorse())
        t = Text()
        t.append("\n      1.  🐍  Aggiorna pip e tutti i pacchetti\n\n",        style="bold magenta")
        t.append("      2.  🤖  Aggiorna Ollama (binario)\n\n",                  style="bold green")
        t.append("      3.  🧹  Cancella __pycache__ e cache pip\n\n",           style="bold magenta")
        t.append("      4.  📦  Aggiorna tutti i modelli Ollama installati\n\n", style="bold green")
        t.append("      5.  ⚡  Cancella cache compilazione Cython (.so .c .html build/)\n\n", style="bold cyan")
        t.append("      6.  🗑️   Pulisci cartella esportazioni (output, quiz, report)\n\n",   style="bold yellow")
        t.append("      7.  🚀  Esegui TUTTO (1-6 in sequenza)\n\n",                          style="bold red")
        t.append("      0 / ⌫   Torna  |  ESC = esci dal programma\n",                       style="dim")
        console.print(Panel(t, title="[bold]🔧 Manutenzione[/]", border_style="blue"))
        console.print("  Scelta: ", end="")
        try:
            tasto = leggi_tasto()
        except (KeyboardInterrupt, EOFError):
            break
        if tasto == "ESC":
            console.print("\n  Uscire dal programma? [S/n] ", end="")
            if input().strip().lower() not in ("n", "no"):
                sys.exit(0)
        if tasto in ("\x7f", "\x08", "0"):
            break
        azione = _AZIONI_MAN.get(tasto)
        if azione:
            try:
                azione()
            except KeyboardInterrupt:
                pass


# ── Menu principale ───────────────────────────────────────────────────────────

def menu_principale() -> None:
    def _azione_1():
        _add_path(os.path.join(_ROOT, "multi_agente"))
        import multi_agente_ui
        multi_agente_ui.menu_multiagente()

    def _azione_2():
        _add_path(os.path.join(_ROOT, "strumento_pratico"))
        import pratico
        pratico.menu_pratico()

    def _azione_3():
        _add_path(os.path.join(_ROOT, "strumento_apprendimento"))
        import apprendimento
        apprendimento.menu_apprendimento()

    def _azione_4():
        """Sottomenu Personalizzazione Avanzata Estrema (llama.cpp + Cython)."""
        _AZIONI_AVX = {
            "1": _avx_llama,
            "2": _avx_cython,
        }
        while True:
            console.clear()
            stampa_header(console, risorse())
            t = Text()
            t.append("\n   Strumenti per chi vuole il massimo delle prestazioni.\n", style="dim italic")
            t.append("   Non è necessario usarli per lavorare con Prismalux.\n\n",   style="dim italic")
            t.append("      1.  🦙  llama.cpp Studio\n",            style="bold green")
            t.append("            Compila ed esegui modelli AI in locale senza\n",  style="dim")
            t.append("            Ollama, con supporto GPU (CUDA/Vulkan/Metal).\n\n", style="dim")
            t.append("      2.  ⚡  Cython Studio\n",               style="bold cyan")
            t.append("            Converti script Python in C per velocità 10x–100x.\n", style="dim")
            t.append("            (Integrato automaticamente nel Multi-Agente)\n\n", style="dim")
            t.append("      0.  ←   Torna al menu principale\n",    style="dim")
            console.print(Panel(t,
                title="[bold yellow]🔩 Personalizzazione Avanzata Estrema[/]",
                border_style="yellow"))
            console.print("  Scelta: ", end="")
            try:
                tasto = leggi_tasto()
            except (KeyboardInterrupt, EOFError):
                break
            if tasto in ("\x7f", "\x08", "0", "ESC"):
                break
            azione_avx = _AZIONI_AVX.get(tasto)
            if azione_avx:
                try:
                    azione_avx()
                except KeyboardInterrupt:
                    pass
                except SystemExit as e:
                    if e.code == 0:
                        raise

    def _avx_llama():
        _add_path(os.path.join(os.path.dirname(_ROOT), "llama_cpp_studio"))
        import llama_studio
        llama_studio.menu_llama()

    def _avx_cython():
        _add_path(os.path.join(_ROOT, "Ottimizzazioni_Avanzate", "cython_studio"))
        import cython_studio
        cython_studio.menu_cython_studio()

    _AZIONI = {
        "1": _azione_1,
        "2": _azione_2,
        "3": _azione_3,
        "4": _azione_4,
        "5": _manutenzione,
    }

    while True:
        console.clear()
        stampa_header(console, risorse())

        t = Text()
        t.append("\n      1.  🤖  Multi-Agente LLM\n\n",                                              style="bold magenta")
        t.append("      2.  🛠   Strumento Pratico\n\n",                                               style="bold green")
        t.append("      3.  📚  Impara con AI\n\n",                                                    style="bold magenta")
        t.append("      4.  🔩  Personalizzazione Avanzata Estrema\n\n",                               style="bold yellow")
        t.append("      5.  🔧  Manutenzione\n\n",                                                     style="bold green")
        t.append("      Q.  🚪  Esci  |  ESC = esci subito\n",                                        style="bold red")
        console.print(Panel(t, title="[bold yellow]🍺 Prismalux — Centro di Comando[/]",
                            border_style="yellow"))
        console.print("  Scelta: ", end="")

        try:
            tasto = leggi_tasto()
        except (KeyboardInterrupt, EOFError):
            break

        if tasto == "ESC":
            console.print("\n  Uscire dal programma? [S/n] ", end="")
            if input().strip().lower() not in ("n", "no"):
                sys.exit(0)
        if tasto in ("\x7f", "\x08", "Q", "0"):
            break

        azione = _AZIONI.get(tasto)
        if azione:
            try:
                azione()
            except KeyboardInterrupt:
                pass
            except SystemExit as e:
                if e.code == 0:
                    raise          # Q = esci davvero
                # codice != 0 (es. 2 = "torna") → continua il loop
            except Exception as e:
                console.print(f"\n  [red]Errore: {e}[/]")
                input("  [INVIO] ")

    console.print("\n  [dim]🍺 Alla prossima libagione di sapere.[/]\n")


if __name__ == "__main__":
    menu_principale()
