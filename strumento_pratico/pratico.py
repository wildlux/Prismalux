"""
strumento_pratico/pratico.py
==============================
Menu unificato per lo Strumento Pratico (Rich puro).
"""
import os
import sys
import time

_CART = os.path.dirname(os.path.abspath(__file__))
_ROOT = os.path.dirname(_CART)
for _p in (_ROOT, _CART):
    if _p not in sys.path:
        sys.path.insert(0, _p)

from rich.console import Console
from rich.panel import Panel
from rich.text import Text
from check_deps import stampa_header, risorse

console = Console()


def menu_pratico() -> None:
    while True:
        console.clear()
        stampa_header(console, risorse())

        t = Text()
        t.append("\n      1.  🔍  Cerca Lavoro\n\n",        style="bold magenta")
        t.append("      2.  📋  Dichiarazione 730\n\n",     style="bold green")
        t.append("      3.  💼  Partita IVA\n\n",           style="bold magenta")
        t.append("      0.  ←   Torna\n",                   style="dim")
        console.print(Panel(t, title="[bold green]🛠 Strumento Pratico[/]",
                            border_style="green"))

        try:
            scelta = input("  Scelta: ").strip()
        except (KeyboardInterrupt, EOFError):
            break

        if scelta == "0":
            break

        elif scelta == "1":
            try:
                from cerca_lavoro import menu_cerca_lavoro
                menu_cerca_lavoro()
            except KeyboardInterrupt:
                pass
            except Exception as e:
                console.print(f"\n  [red]Errore: {e}[/]")
                input("  [INVIO] ")

        elif scelta == "2":
            try:
                from dichiarazione_730 import menu_730
                menu_730()
            except KeyboardInterrupt:
                pass
            except Exception as e:
                console.print(f"\n  [red]Errore: {e}[/]")
                input("  [INVIO] ")

        elif scelta == "3":
            try:
                from partita_iva import menu_partita_iva
                menu_partita_iva()
            except KeyboardInterrupt:
                pass
            except Exception as e:
                console.print(f"\n  [red]Errore: {e}[/]")
                input("  [INVIO] ")

        else:
            console.print("  [yellow]Scelta non valida.[/]")
            time.sleep(0.8)


if __name__ == "__main__":
    menu_pratico()
