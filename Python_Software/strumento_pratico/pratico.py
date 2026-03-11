"""
strumento_pratico/pratico.py
==============================
Menu unificato per lo Strumento Pratico (Rich puro).
"""
import os
import sys

_CART = os.path.dirname(os.path.abspath(__file__))
_ROOT = os.path.dirname(_CART)
for _p in (_ROOT, _CART, os.path.join(_ROOT, "core")):
    if _p not in sys.path:
        sys.path.insert(0, _p)

from rich.console import Console
from rich.panel import Panel
from rich.text import Text
from check_deps import stampa_header, risorse, leggi_tasto

console = Console()


def menu_pratico() -> None:
    def _azione_1():
        from cerca_lavoro import menu_cerca_lavoro
        menu_cerca_lavoro()

    def _azione_2():
        from dichiarazione_730 import menu_730
        menu_730()

    def _azione_3():
        from partita_iva import menu_partita_iva
        menu_partita_iva()

    _AZIONI = {
        "1": _azione_1,
        "2": _azione_2,
        "3": _azione_3,
    }

    while True:
        console.clear()
        stampa_header(console, risorse())

        t = Text()
        t.append("\n      1.  🔍  Cerca Lavoro\n\n",        style="bold magenta")
        t.append("      2.  📋  Dichiarazione 730\n\n",     style="bold green")
        t.append("      3.  💼  Partita IVA\n\n",           style="bold magenta")
        t.append("      0 / ⌫   Torna  |  ESC = esci dal programma\n", style="dim")
        console.print(Panel(t, title="[bold green]🛠 Strumento Pratico[/]",
                            border_style="green"))
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

        azione = _AZIONI.get(tasto)
        if azione:
            try:
                azione()
            except KeyboardInterrupt:
                pass
            except Exception as e:
                console.print(f"\n  [red]Errore: {e}[/]")
                input("  [INVIO] ")


if __name__ == "__main__":
    menu_pratico()
