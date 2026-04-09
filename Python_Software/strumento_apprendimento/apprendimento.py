"""
strumento_apprendimento/apprendimento.py
=========================================
Menu unificato: Simulatore 80 algoritmi + Tutor AI (Rich puro).
"""
import os
import sys
import time
import threading
import builtins
import re

_CART = os.path.dirname(os.path.abspath(__file__))
_ROOT = os.path.dirname(_CART)
for _p in (_ROOT, _CART, os.path.join(_ROOT, "core")):
    if _p not in sys.path:
        sys.path.insert(0, _p)

from rich.console import Console
from rich.panel import Panel
from rich.text import Text
from rich.live import Live
from rich.rule import Rule
from check_deps import stampa_header, risorse, leggi_tasto

console = Console()


# ══════════════════════════════════════════════════════════════════════════════
#  _ConsoleFake — intercetta console.print() degli algoritmi
# ══════════════════════════════════════════════════════════════════════════════

class _ConsoleFake:
    """
    Sostituisce sim.console durante l'esecuzione animata (Rich Live).
    Ogni console.print() viene catturato e passato al frame_cb del hook.
    """

    def __init__(self, hook: dict, width: int = 100):
        self._hook  = hook
        self._width = width

    def print(self, *args, **kwargs) -> None:
        import io
        for _markup in (True, False):
            try:
                buf = io.StringIO()
                tmp = Console(file=buf, no_color=False, width=self._width,
                                   highlight=False, markup=_markup)
                tmp.print(*args, **kwargs)
                break
            except Exception:
                if not _markup:
                    return
        testo_raw = buf.getvalue().rstrip("\n")
        if not testo_raw:
            return
        try:
            t = Text.from_ansi(testo_raw)
            hook = self._hook
            if hook and hook.get("frame_cb"):
                hook["frame_cb"](t)
        except Exception:
            pass

    @property
    def width(self) -> int:
        return self._width

    def __call__(self, *args, **kwargs) -> "_ConsoleFake":
        return _ConsoleFake(self._hook, kwargs.get("width", self._width))


# ══════════════════════════════════════════════════════════════════════════════
#  Catalogo algoritmi (80 voci, speculare a simulatore_screen.py)
# ══════════════════════════════════════════════════════════════════════════════

_CATEGORIE = [
    ("🟡 ORDINAMENTO BASE — O(n²)", "yellow", [
        (1, "Bubble Sort"), (2, "Cocktail Sort"), (3, "Selection Sort"),
        (4, "Insertion Sort"), (5, "Shell Sort"), (6, "Gnome Sort"),
    ]),
    ("🟠 NON-COMPARATIVO", "magenta", [
        (7, "Counting Sort"), (8, "Radix Sort"), (9, "Bucket Sort"),
    ]),
    ("🔴 EFFICIENTE — O(n log n)", "red", [
        (10, "Merge Sort"), (11, "Quick Sort"), (12, "Heap Sort"),
    ]),
    ("🔵 RICERCA", "cyan", [
        (13, "Ricerca Lineare"), (14, "Ricerca Binaria"),
        (15, "Jump Search"), (16, "Interpolation Search"),
    ]),
    ("🟢 STRUTTURE DATI BASE", "green", [
        (17, "Stack e Queue"), (18, "Linked List"), (19, "BST"),
        (20, "Hash Table"), (21, "Min-Heap"),
    ]),
    ("🔷 GRAFI BASE", "blue", [
        (22, "BFS"), (23, "DFS"), (24, "Dijkstra"),
    ]),
    ("⭐ CLASSICI / SLIDING", "bright_yellow", [
        (25, "Fibonacci"), (26, "Torre di Hanoi"), (27, "Crivello Eratostene"),
        (28, "Two Pointers"), (29, "Sliding Window"), (30, "LRU Cache"),
    ]),
    ("💚 PROGRAMMAZIONE DINAMICA", "green", [
        (31, "Zaino 0/1"), (32, "Coin Change"), (33, "LCS"),
        (34, "Edit Distance"), (35, "LIS"),
    ]),
    ("🟣 ALGORITMI SU STRINGHE", "magenta", [
        (36, "KMP Search"), (37, "Rabin-Karp"), (38, "Z-Algorithm"),
    ]),
    ("🌐 GRAFI AVANZATI", "blue", [
        (39, "Bellman-Ford"), (40, "Floyd-Warshall"), (41, "Kruskal MST"),
        (42, "Prim MST"), (43, "A* Search"), (44, "Topological Sort"),
    ]),
    ("💛 GREEDY", "yellow", [
        (45, "Activity Selection"), (46, "Huffman Coding"), (47, "Zaino Frazionario"),
    ]),
    ("🔴 BACKTRACKING + 🔢 MATEMATICI", "red", [
        (48, "N-Queens"), (49, "Sudoku Solver"), (50, "GCD Euclide"),
        (51, "Potenza Modulare"), (52, "Miller-Rabin"), (53, "Karatsuba"),
        (80, "Cubo di Rubik"),
    ]),
    ("🏗️ STRUTTURE AVANZATE + 📐 GEOMETRICI", "green", [
        (54, "AVL Tree"), (55, "Segment Tree"), (56, "Fenwick Tree"),
        (57, "Union-Find"), (58, "Convex Hull"),
    ]),
    ("🎮 ANIMAZIONI LIVE — Ctrl+C per fermare", "bright_green", [
        (59, "Game of Life"), (60, "Labirinto BFS"), (61, "Sorting Race"),
        (62, "N-Regine BT"), (63, "Dijkstra Griglia"), (64, "Rule 30 Wolfram"),
        (65, "Fuoco Foresta"), (66, "Matrix Rain"),
        (76, "Flood Fill BFS"), (77, "Spiral Matrix"), (79, "Sierpinski"),
    ]),
    ("🔀 ALTRI ORDINAMENTI + 🌲 BIT/MATH/TRIE", "bright_white", [
        (67, "Tim Sort"), (68, "Pancake Sort"), (69, "Quickselect"),
        (70, "Comb Sort"), (71, "Trie Prefisso"), (72, "Deque"),
        (73, "Bit Manipulation"), (74, "Triangolo Pascal"),
        (75, "Monte Carlo Pi"), (78, "Tarjan SCC"),
    ]),
]

_NUM_TO_INFO: dict[int, tuple[str, str]] = {}
for _titolo, _colore, _algos in _CATEGORIE:
    for _n, _nome in _algos:
        _NUM_TO_INFO[_n] = (_nome, _colore)


# ══════════════════════════════════════════════════════════════════════════════
#  Esecuzione algoritmo con Rich Live (modalità animata)
# ══════════════════════════════════════════════════════════════════════════════

_VELOCITA_SCELTE = [
    ("⚡ Istantaneo  0.04s", 0.04),
    ("🐇 Veloce     0.18s", 0.18),
    ("🐕 Normale    0.6s  (default)", 0.6),
    ("🐌 Lento      1.5s", 1.5),
    ("🐢 Manuale    [INVIO]", 0.0),
]


def _scegli_velocita() -> float:
    """Chiede all'utente la velocità prima di avviare l'algoritmo."""
    console.print("\n  [bold cyan]Velocità di visualizzazione:[/]")
    for i, (etichetta, _) in enumerate(_VELOCITA_SCELTE, 1):
        console.print(f"    {i}. {etichetta}")
    console.print()
    try:
        raw = input("  Scelta [default = 3]: ").strip()
    except (KeyboardInterrupt, EOFError):
        return 0.6
    if not raw:
        return 0.6
    try:
        idx = int(raw) - 1
        if 0 <= idx < len(_VELOCITA_SCELTE):
            return _VELOCITA_SCELTE[idx][1]
    except ValueError:
        pass
    return 0.6


def _esegui_algoritmo(num: int, nome: str, velocita: float) -> None:
    """Esegue l'algoritmo num in un thread con Rich Live (o modalità manuale)."""
    import simulatore as sim

    # Salva originali
    _console_orig = sim.console
    _Console_orig = getattr(sim, "Console", None)
    _input_orig   = builtins.input
    _os_sys_orig  = __import__("os").system
    _aspetta_orig = sim.aspetta

    stop_event = threading.Event()

    # ── Intercetta input() — usa valore di default dal prompt ─────────────────
    def _input_auto(prompt: str = "") -> str:
        m = re.search(r"invio=([^\s:)]+)", str(prompt))
        return m.group(1) if m else ""

    # ── Intercetta os.system("clear"/"cls") — no-op ───────────────────────────
    import os as _os
    def _os_system_noop(cmd: str) -> int:
        if "clear" in cmd or "cls" in cmd:
            return 0
        return _os_sys_orig(cmd)

    # ── Modalità ANIMATA (Rich Live) ──────────────────────────────────────────
    if velocita > 0:
        frame: dict = {"t": Text(f"  ▶  Avvio {nome}...")}

        def frame_cb(testo: Text) -> None:
            frame["t"] = testo

        hook = {"frame_cb": frame_cb}
        proxy = _ConsoleFake(hook)
        sim.console = proxy
        if _Console_orig is not None:
            sim.Console = proxy

        sim.aspetta = lambda: time.sleep(velocita)
        builtins.input = _input_auto
        _os.system = _os_system_noop

        def _run() -> None:
            try:
                sim.lancia_algo_textual(num)
            except Exception as e:
                frame["t"] = Text(f"  [Errore: {e}]")
            finally:
                stop_event.set()

        t_thread = threading.Thread(target=_run, daemon=True)

        try:
            with Live(Panel(frame["t"], title=f"[bold]{nome}[/]"),
                      refresh_per_second=8, console=console) as live:
                t_thread.start()
                while not stop_event.is_set():
                    live.update(Panel(frame["t"], title=f"[bold]{nome}[/]"))
                    time.sleep(0.08)
                # Ultimo frame dopo completamento
                live.update(Panel(frame["t"], title=f"[bold green]✓ {nome} — Completato[/]"))
        except KeyboardInterrupt:
            stop_event.set()

    # ── Modalità MANUALE (terminale diretto) ──────────────────────────────────
    else:
        sim.console = console   # output diretto nel terminale
        if _Console_orig is not None:
            sim.Console = Console

        def _aspetta_manuale() -> None:
            try:
                input("  [INVIO] avanza... ")
            except (KeyboardInterrupt, EOFError):
                raise KeyboardInterrupt

        sim.aspetta = _aspetta_manuale
        builtins.input = _input_auto
        _os.system = _os_system_noop

        try:
            console.clear()
            console.print(Rule(f"[bold]{nome}[/] — modalità manuale (INVIO=avanza, Ctrl+C=stop)"))
            sim.lancia_algo_textual(num)
        except KeyboardInterrupt:
            pass
        finally:
            stop_event.set()

    # ── Ripristina tutto ───────────────────────────────────────────────────────
    sim._textual_hook = None
    sim.aspetta       = _aspetta_orig
    sim.console       = _console_orig
    if _Console_orig is not None:
        sim.Console = _Console_orig
    builtins.input = _input_orig
    _os.system     = _os_sys_orig

    if velocita > 0:
        console.print(f"\n  [bold green]✓ {nome} completato.[/]")
    input("  [INVIO] per tornare al menu... ")


# ══════════════════════════════════════════════════════════════════════════════
#  Menu simulatore — lista 80 algoritmi
# ══════════════════════════════════════════════════════════════════════════════

def _menu_simulatore() -> None:
    while True:
        console.clear()
        stampa_header(console, risorse())

        t = Text()
        t.append("\n")
        for titolo, colore, algos in _CATEGORIE:
            t.append(f"  {titolo}\n", style=f"bold {colore}")
            riga = ""
            for num, nome in algos:
                voce = f"    {num:3}. {nome}"
                riga += f"{voce:<30}"
            t.append(riga + "\n\n")
        t.append("  0 / ESC = torna  |  Digita numero + Invio\n", style="dim")

        console.print(Panel(t, title="[bold cyan]🔵 Simulatore — 80 Algoritmi[/]",
                            border_style="cyan"))

        try:
            raw = input("  Numero algoritmo: ").strip()
        except (KeyboardInterrupt, EOFError):
            break

        if raw in ("0", ""):
            break
        try:
            num = int(raw)
        except ValueError:
            console.print("  [yellow]Inserisci un numero valido.[/]")
            time.sleep(1)
            continue

        if num not in _NUM_TO_INFO:
            console.print(f"  [yellow]Algoritmo {num} non trovato.[/]")
            time.sleep(1)
            continue

        nome, _ = _NUM_TO_INFO[num]
        velocita = _scegli_velocita()
        try:
            _esegui_algoritmo(num, nome, velocita)
        except KeyboardInterrupt:
            pass


# ══════════════════════════════════════════════════════════════════════════════
#  Menu apprendimento principale
# ══════════════════════════════════════════════════════════════════════════════

def menu_apprendimento() -> None:
    def _azione_2():
        from tutor_semplice import menu_python
        menu_python()

    def _azione_3():
        from tutor_semplice import menu_matematica
        menu_matematica()

    def _azione_4():
        from tutor_semplice import menu_fisica
        menu_fisica()

    def _azione_5():
        from tutor_semplice import menu_chimica
        menu_chimica()

    def _azione_6():
        from tutor_semplice import menu_sicurezza
        menu_sicurezza()

    def _azione_7():
        from protocolli import menu_protocolli
        menu_protocolli()

    def _azione_8():
        from quiz_interattivi import menu_quiz
        menu_quiz()

    def _azione_9():
        from dashboard_statistica import menu_dashboard
        menu_dashboard()

    _AZIONI = {
        "1": _menu_simulatore,
        "2": _azione_2,
        "3": _azione_3,
        "4": _azione_4,
        "5": _azione_5,
        "6": _azione_6,
        "7": _azione_7,
        "8": _azione_8,
        "9": _azione_9,
    }

    while True:
        console.clear()
        stampa_header(console, risorse())

        t = Text()
        t.append("\n      1.  🔵  Simulatore Visivo — 80 Algoritmi\n\n",         style="bold magenta")
        t.append("      2.  🐍  Tutor AI — Python & Algoritmi\n\n",              style="bold green")
        t.append("      3.  📐  Tutor AI — Matematica\n\n",                      style="bold magenta")
        t.append("      4.  ⚛️  Tutor AI — Fisica\n\n",                          style="bold green")
        t.append("      5.  🧪  Tutor AI — Chimica\n\n",                         style="bold magenta")
        t.append("      6.  🔒  Tutor AI — Sicurezza Informatica\n\n",           style="bold green")
        t.append("      7.  🌐  Simulatore Protocolli di Rete\n\n",              style="bold magenta")
        t.append("      8.  📚  Quiz Interattivi — test con punteggio\n\n",      style="bold cyan")
        t.append("      9.  📊  Dashboard Statistica — progressi e grafici\n\n", style="bold green")
        t.append("      0 / ⌫   Torna  |  ESC = esci dal programma\n",          style="dim")
        console.print(Panel(t, title="[bold yellow]📚 Apprendimento[/]",
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
        if tasto in ("\x7f", "\x08", "0"):
            break

        azione = _AZIONI.get(tasto)
        if azione:
            try:
                azione()
            except KeyboardInterrupt:
                pass
            except Exception as e:
                console.print(f"  [red]Errore: {e}[/]")
                input("  [INVIO] ")


if __name__ == "__main__":
    menu_apprendimento()
