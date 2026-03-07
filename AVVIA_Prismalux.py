"""
Prismalux — Punto di ingresso unico
=====================================
Avvio:  python3.14 AVVIA_Prismalux.py
"""
import os
import sys

_ROOT = os.path.dirname(os.path.abspath(__file__))
if _ROOT not in sys.path:
    sys.path.insert(0, _ROOT)

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
from check_deps import stampa_header, risorse

console = Console()


def _add_path(subdir: str) -> None:
    if subdir not in sys.path:
        sys.path.insert(0, subdir)


# ── Manutenzione ──────────────────────────────────────────────────────────────

def _manutenzione() -> None:
    while True:
        console.clear()
        stampa_header(console, risorse())
        t = Text()
        t.append("\n      1.  🐍  Aggiorna pip e tutti i pacchetti\n\n",        style="bold magenta")
        t.append("      2.  🤖  Aggiorna Ollama (binario)\n\n",                  style="bold green")
        t.append("      3.  🧹  Cancella __pycache__ e cache pip\n\n",           style="bold magenta")
        t.append("      4.  📦  Aggiorna tutti i modelli Ollama installati\n\n", style="bold green")
        t.append("      0.  ←   Torna al menu principale\n",                      style="dim")
        console.print(Panel(t, title="[bold]🔧 Manutenzione[/]", border_style="blue"))
        try:
            scelta = input("  Scelta: ").strip()
        except (KeyboardInterrupt, EOFError):
            break
        if scelta in ("0", ""):
            break
        elif scelta == "1":
            console.print("\n  Aggiornamento pip e pacchetti...\n")
            subprocess.run([sys.executable, "-m", "pip", "install", "--upgrade", "pip"])
            for mod, pkg in _PACCHETTI:
                if importlib.util.find_spec(mod):
                    subprocess.run([sys.executable, "-m", "pip", "install", "--upgrade", pkg])
            console.print("\n  [green]✓ Completato.[/]")
            input("  [INVIO] ")
        elif scelta == "2":
            if sys.platform != "win32":
                subprocess.run("curl -fsSL https://ollama.com/install.sh | sh", shell=True)
            else:
                console.print("  Windows: scarica l'installer da https://ollama.com/download")
            input("\n  [INVIO] ")
        elif scelta == "3":
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
        elif scelta == "4":
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


# ── Menu principale ───────────────────────────────────────────────────────────

def menu_principale() -> None:
    while True:
        console.clear()
        stampa_header(console, risorse())

        t = Text()
        t.append("\n      1.  🤖  Multi-Agente LLM classico\n\n",                                     style="bold magenta")
        t.append("      2.  🛠   Strumento Pratico\n\n",                                               style="bold green")
        t.append("      3.  📚  Impara con AI\n\n",                                                    style="bold magenta")
        t.append("      4.  ⚙️   llama.cpp Studio — ottimizzazione massima\n\n",                       style="bold green")
        t.append("      5.  🔧  Manutenzione — aggiorna qualsiasi componente di questo programma\n\n", style="bold magenta")
        t.append("      6.  ⚡  Cython Studio — compila Python in C per massima velocita\n\n",        style="bold cyan")
        t.append("      Q.  🚪  Esci\n",                                                               style="bold red")
        console.print(Panel(t, title="[bold yellow]🍺 Prismalux — Centro di Comando[/]",
                            border_style="yellow"))

        try:
            scelta = input("  Scelta: ").strip().lower()
        except (KeyboardInterrupt, EOFError):
            break

        if scelta == "1":
            try:
                _add_path(os.path.join(_ROOT, "multi_agente"))
                import multi_agente_ui
                multi_agente_ui.menu_multiagente()
            except KeyboardInterrupt:
                pass
            except Exception as e:
                console.print(f"\n  [red]Errore: {e}[/]")
                input("  [INVIO] ")

        elif scelta == "2":
            try:
                _add_path(os.path.join(_ROOT, "strumento_pratico"))
                import pratico
                pratico.menu_pratico()
            except KeyboardInterrupt:
                pass
            except Exception as e:
                console.print(f"\n  [red]Errore: {e}[/]")
                input("  [INVIO] ")

        elif scelta == "3":
            try:
                _add_path(os.path.join(_ROOT, "strumento_apprendimento"))
                import apprendimento
                apprendimento.menu_apprendimento()
            except KeyboardInterrupt:
                pass
            except Exception as e:
                console.print(f"\n  [red]Errore: {e}[/]")
                input("  [INVIO] ")

        elif scelta == "4":
            try:
                _add_path(os.path.join(_ROOT, "llama_cpp_studio"))
                import llama_studio
                llama_studio.menu_llama()
            except KeyboardInterrupt:
                pass
            except Exception as e:
                console.print(f"\n  [red]Errore: {e}[/]")
                input("  [INVIO] ")

        elif scelta == "5":
            try:
                _manutenzione()
            except KeyboardInterrupt:
                pass

        elif scelta == "6":
            try:
                _add_path(os.path.join(_ROOT, "cython_studio"))
                import cython_studio
                cython_studio.menu_cython_studio()
            except KeyboardInterrupt:
                pass
            except Exception as e:
                console.print(f"\n  [red]Errore Cython Studio: {e}[/]")
                input("  [INVIO] ")

        elif scelta in ("q", "0"):
            break

    console.print("\n  [dim]🍺 Alla prossima libagione di sapere.[/]\n")


if __name__ == "__main__":
    menu_principale()
