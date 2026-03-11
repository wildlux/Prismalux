"""
Simulatore Visivo di Algoritmi
================================
Guarda gli algoritmi in azione passo per passo, con colori e barre.

Legenda colori:
  GIALLO  = elementi in confronto
  ROSSO   = elementi in scambio
  VERDE   = già nella posizione corretta
  BIANCO  = non ancora elaborato
  CIANO   = trovato (ricerca)

Dipendenze:
  pip install rich
"""

import os
import sys
import shutil
import random
from rich.console import Console
try:
    from check_deps import Ridimensiona, input_nel_box, stampa_breadcrumb, stampa_header
except ImportError:
    class Ridimensiona(BaseException): pass
    def input_nel_box(padding_left=2): return input("\n  ❯ ")
    def stampa_breadcrumb(con, percorso): print(f"  {percorso}")
    def stampa_header(con, res=None): pass
from rich.panel import Panel
from rich.text import Text
from rich.table import Table

console = Console()

# ── Modalità di avanzamento (cicla con A nel menu) ────────────────────────────
# Ogni voce: (animato: bool, pausa_secondi: float, etichetta: str)
_VELOCITA_CICLO = [
    (False, 0.0,  "🐢 Passo  [INVIO]"),
    (True,  1.5,  "🐌 Animato  1.5 s"),
    (True,  0.6,  "🐇 Animato  0.6 s"),
    (True,  0.18, "🐆 Animato  0.18s"),
    (True,  0.04, "⚡ Animato  0.04s"),
]
_modo_idx: int = 0   # indice corrente — modificato da menu_simulatore()

import threading as _th
_textual_hook: "dict | None" = None
# {'frame_cb': callable(titolo, testo, log), 'step_event': Event}

# Nomi algoritmi per la schermata di setup
_NOMI_ALGO: dict[int, str] = {
    1:"Bubble Sort",       2:"Cocktail Sort",       3:"Selection Sort",
    4:"Insertion Sort",    5:"Shell Sort",           6:"Gnome Sort",
    7:"Counting Sort",     8:"Radix Sort",           9:"Bucket Sort",
    10:"Merge Sort",       11:"Quick Sort",          12:"Heap Sort",
    13:"Ricerca Lineare",  14:"Ricerca Binaria",     15:"Jump Search",
    16:"Interpolation Search",
    17:"Stack e Queue",    18:"Linked List",         19:"BST",
    20:"Hash Table",       21:"Min-Heap",
    22:"BFS",              23:"DFS",                 24:"Dijkstra",
    25:"Fibonacci",        26:"Torre di Hanoi",      27:"Crivello di Eratostene",
    28:"Two Pointers",     29:"Sliding Window",      30:"LRU Cache",
    31:"Zaino 0/1",        32:"Coin Change",         33:"LCS",
    34:"Edit Distance",    35:"LIS",
    36:"KMP Search",       37:"Rabin-Karp",          38:"Z-Algorithm",
    39:"Bellman-Ford",     40:"Floyd-Warshall",      41:"Kruskal MST",
    42:"Prim MST",         43:"A* Search",           44:"Topological Sort",
    45:"Activity Selection",46:"Huffman Coding",     47:"Zaino Frazionario",
    48:"N-Queens",         49:"Sudoku Solver",       50:"GCD Euclide",
    51:"Potenza Modulare", 52:"Miller-Rabin",        53:"Karatsuba",
    54:"AVL Tree",         55:"Segment Tree",        56:"Fenwick Tree",
    57:"Union-Find",       58:"Convex Hull",
    59:"Game of Life",     60:"Labirinto + BFS",     61:"Sorting Race",
    62:"N-Regine BT",      63:"Dijkstra Griglia",    64:"Rule 30 Wolfram",
    65:"Fuoco nella Foresta", 66:"Matrix Rain",
    67:"Tim Sort",         68:"Pancake Sort",        69:"Quickselect",
    70:"Comb Sort",        71:"Trie Prefisso",       72:"Deque",
    73:"Bit Manipulation", 74:"Triangolo di Pascal", 75:"Monte Carlo Pi",
    76:"Flood Fill BFS",   77:"Spiral Matrix",       78:"Tarjan SCC",
    79:"Sierpinski",       80:"Cubo di Rubik",
}


def _modo_animato() -> bool:
    return _VELOCITA_CICLO[_modo_idx][0]


def _velocita() -> float:
    return _VELOCITA_CICLO[_modo_idx][1]


def _etichetta_modo() -> str:
    return _VELOCITA_CICLO[_modo_idx][2]


def _larghezza_barra() -> int:
    """Calcola la larghezza massima delle barre in base al terminale attuale."""
    cols, _ = shutil.get_terminal_size(fallback=(80, 24))
    # Sottrai lo spazio per indice, valore e bordi (~20 char fissi)
    return max(10, min(60, cols - 20))


# ── Log passi ─────────────────────────────────────────────────────────────────
_log: list[str] = []   # accumula i messaggi di ogni passo dell'algoritmo corrente


# ── Visualizzazione array ─────────────────────────────────────────────────────

def mostra_array(arr, confronto=(), scambio=(), ordinati=frozenset(),
                 trovato=-1, messaggio="", titolo="Simulatore"):
    """Stampa l'array con barre colorate, cronologia passi e messaggio corrente."""
    global _log
    larghezza_max = _larghezza_barra()

    max_val = max(max(arr), 1) if arr else 1  # min 1 per evitare ZeroDivisionError
    testo = Text()

    # Legenda
    testo.append("  Verde=ordinato  ", style="green")
    testo.append("Giallo=confronto  ", style="yellow")
    testo.append("Rosso=scambio  ", style="red")
    testo.append("Ciano=trovato\n\n", style="cyan")

    for idx, val in enumerate(arr):
        if idx in scambio:
            stile, simbolo = "bold red", " ↕"
        elif idx in confronto:
            stile, simbolo = "bold yellow", " ?"
        elif idx == trovato:
            stile, simbolo = "bold cyan", " ✓"
        elif idx in ordinati:
            stile, simbolo = "green", "  "
        else:
            stile, simbolo = "white", "  "

        barra_len = max(1, int(val / max_val * larghezza_max))
        barra = "█" * barra_len

        testo.append(f"  [{idx:2}] ", style="dim white")
        testo.append(f"{barra} {val:3}{simbolo}\n", style=stile)

    if messaggio:
        _log.append(messaggio)

    # ── Ramo Textual (hook attivo) ────────────────────────────────────────────
    if _textual_hook:
        try:
            _textual_hook['frame_cb'](titolo, testo, list(_log[-8:]))
        except Exception:
            pass
        return

    # ── Comportamento originale terminale ─────────────────────────────────────
    os.system("cls" if sys.platform == "win32" else "clear")
    cols, _ = shutil.get_terminal_size(fallback=(80, 24))
    con = Console(width=cols)

    pannello = Panel(testo, title=f"[bold blue]{titolo}[/]", border_style="blue")
    con.print(pannello)

    # ── Cronologia passi ──────────────────────────────────────────────────────
    if _log:
        visibili = _log[-8:]          # mostra gli ultimi 8 passi
        log_txt = Text()
        for i, msg in enumerate(visibili):
            n_tot = len(_log)
            n_vis = len(visibili)
            num = n_tot - n_vis + i + 1   # numero assoluto del passo
            if i < n_vis - 1:
                log_txt.append(f"  {num:3}. {msg}\n", style="dim")
            else:
                log_txt.append(f"  {num:3}. {msg}\n", style="bold cyan")
        titolo_log = f"[dim]Passi: {len(_log)}[/]"
        con.print(Panel(log_txt, title=titolo_log, border_style="dim", padding=(0, 1)))


def _pausa_fine_demo():
    """Fine demo: in Textual la schermata resta visibile fino a ESC; in terminale chiede INVIO."""
    if _textual_hook:
        pass  # schermata AlgoritmoScreen resta attiva finché l'utente preme ESC
    else:
        _pausa_fine_demo()


def aspetta():
    if _modo_animato():
        import time
        time.sleep(_velocita())
    else:
        if _textual_hook:
            _textual_hook['step_event'].wait()
            _textual_hook['step_event'].clear()
        else:
            input("     Premi INVIO per il passo successivo... ")


def genera_lista(n=8):
    return random.sample(range(5, 95), n)


# ── Bubble Sort ───────────────────────────────────────────────────────────────

def bubble_sort(arr):
    """
    BUBBLE SORT
    Confronta due elementi vicini.
    Se sono nell'ordine sbagliato → li scambia.
    Ripete finché tutto è ordinato.
    """
    arr = arr.copy()
    n = len(arr)
    ordinati = set()

    console.print(Panel(
        "[bold]BUBBLE SORT[/]\n\n"
        "Idea: confronta due numeri vicini.\n"
        "Se quello a sinistra è più grande → scambia.\n"
        "Continua finché nessuno scambio è necessario.",
        border_style="yellow"
    ))
    aspetta()

    passo = 0
    for i in range(n - 1):
        scambiato = False
        for j in range(n - 1 - i):
            passo += 1
            mostra_array(arr, confronto=(j, j + 1), ordinati=ordinati,
                         messaggio=f"Passo {passo}: confronto {arr[j]} e {arr[j+1]}",
                         titolo="Bubble Sort")
            aspetta()

            if arr[j] > arr[j + 1]:
                arr[j], arr[j + 1] = arr[j + 1], arr[j]
                scambiato = True
                mostra_array(arr, scambio=(j, j + 1), ordinati=ordinati,
                             messaggio=f"→ {arr[j]} < {arr[j+1]}: sì, {arr[j+1]} era più grande → SCAMBIO!",
                             titolo="Bubble Sort")
                aspetta()
            else:
                mostra_array(arr, confronto=(j, j + 1), ordinati=ordinati,
                             messaggio=f"→ {arr[j]} ≤ {arr[j+1]}: già nell'ordine giusto, nessuno scambio.",
                             titolo="Bubble Sort")
                aspetta()

        ordinati.add(n - 1 - i)
        if not scambiato:
            break

    mostra_array(arr, ordinati=set(range(n)),
                 messaggio="✓ Lista ordinata! Nessun altro scambio necessario.",
                 titolo="Bubble Sort — COMPLETATO")
    _pausa_fine_demo()
    return arr


# ── Selection Sort ────────────────────────────────────────────────────────────

def selection_sort(arr):
    """
    SELECTION SORT
    Trova il numero più piccolo in tutta la lista.
    Mettilo all'inizio.
    Ripeti sulla parte rimanente.
    """
    arr = arr.copy()
    n = len(arr)
    ordinati = set()

    console.print(Panel(
        "[bold]SELECTION SORT[/]\n\n"
        "Idea: cerca il numero più piccolo di tutti.\n"
        "Mettilo nella prima posizione libera.\n"
        "Ripeti cercando il successivo più piccolo.",
        border_style="yellow"
    ))
    aspetta()

    for i in range(n):
        min_idx = i
        for j in range(i + 1, n):
            mostra_array(arr, confronto=(min_idx, j), ordinati=ordinati,
                         messaggio=f"Confronto: minimo attuale {arr[min_idx]} vs {arr[j]}",
                         titolo="Selection Sort")
            aspetta()
            if arr[j] < arr[min_idx]:
                vecchio = arr[min_idx]
                min_idx = j
                mostra_array(arr, confronto=(min_idx,), ordinati=ordinati,
                             messaggio=f"→ {arr[min_idx]} < {vecchio}: nuovo minimo trovato! [{min_idx}]",
                             titolo="Selection Sort")
                aspetta()
            else:
                mostra_array(arr, confronto=(min_idx, j), ordinati=ordinati,
                             messaggio=f"→ {arr[j]} ≥ {arr[min_idx]}: il minimo resta {arr[min_idx]}",
                             titolo="Selection Sort")
                aspetta()

        if min_idx != i:
            arr[i], arr[min_idx] = arr[min_idx], arr[i]
            mostra_array(arr, scambio=(i, min_idx), ordinati=ordinati,
                         messaggio=f"→ Il minimo è {arr[i]}, lo metto in posizione {i}",
                         titolo="Selection Sort")
            aspetta()

        ordinati.add(i)

    mostra_array(arr, ordinati=set(range(n)),
                 messaggio="✓ Lista ordinata!",
                 titolo="Selection Sort — COMPLETATO")
    _pausa_fine_demo()
    return arr


# ── Insertion Sort ────────────────────────────────────────────────────────────

def insertion_sort(arr):
    """
    INSERTION SORT
    Prendi un elemento.
    Trovaci il posto giusto tra quelli già ordinati.
    Inseriscilo lì.
    """
    arr = arr.copy()
    n = len(arr)
    ordinati = {0}

    console.print(Panel(
        "[bold]INSERTION SORT[/]\n\n"
        "Idea: come ordinare le carte in mano.\n"
        "Prendi una carta → trovale il posto giusto → inseriscila.\n"
        "Ripeti per ogni carta.",
        border_style="yellow"
    ))
    aspetta()

    for i in range(1, n):
        chiave = arr[i]
        j = i - 1

        mostra_array(arr, confronto=(i,), ordinati=ordinati,
                     messaggio=f"Prendo il valore {chiave} dalla posizione {i}",
                     titolo="Insertion Sort")
        aspetta()

        while j >= 0 and arr[j] > chiave:
            arr[j + 1] = arr[j]
            mostra_array(arr, scambio=(j, j + 1), ordinati=ordinati,
                         messaggio=f"→ {arr[j]} > {chiave}: sposto {arr[j]} a destra",
                         titolo="Insertion Sort")
            aspetta()
            j -= 1

        arr[j + 1] = chiave
        ordinati.add(i)
        mostra_array(arr, ordinati=ordinati,
                     messaggio=f"→ {chiave} inserito nella posizione giusta: {j+1}",
                     titolo="Insertion Sort")
        aspetta()

    mostra_array(arr, ordinati=set(range(n)),
                 messaggio="✓ Lista ordinata!",
                 titolo="Insertion Sort — COMPLETATO")
    _pausa_fine_demo()
    return arr


# ── Ricerca Lineare ───────────────────────────────────────────────────────────

def ricerca_lineare(arr, target):
    """
    RICERCA LINEARE
    Guarda ogni elemento uno alla volta.
    Dall'inizio alla fine.
    Fermati quando trovi quello che cerchi.
    """
    console.print(Panel(
        "[bold]RICERCA LINEARE[/]\n\n"
        "Idea: come cercare un nome in un elenco.\n"
        "Leggi uno a uno dall'inizio.\n"
        f"Cerchiamo: [bold cyan]{target}[/]",
        border_style="yellow"
    ))
    aspetta()

    for i in range(len(arr)):
        mostra_array(arr, confronto=(i,),
                     messaggio=f"Guardo posizione {i}: il valore è {arr[i]}",
                     titolo="Ricerca Lineare")
        aspetta()

        if arr[i] == target:
            mostra_array(arr, trovato=i,
                         messaggio=f"✓ Trovato {target} alla posizione {i}! ({i+1} passi)",
                         titolo="Ricerca Lineare — TROVATO!")
            _pausa_fine_demo()
            return i

    mostra_array(arr, messaggio=f"✗ {target} non è presente nella lista.",
                 titolo="Ricerca Lineare — NON TROVATO")
    _pausa_fine_demo()
    return -1


# ── Ricerca Binaria ───────────────────────────────────────────────────────────

def ricerca_binaria(arr, target):
    """
    RICERCA BINARIA
    La lista DEVE essere ordinata.
    Guarda il centro: è quello che cerchi?
    No → cerca solo nella metà giusta.
    """
    arr = sorted(arr)
    n = len(arr)
    sinistra, destra = 0, n - 1

    console.print(Panel(
        "[bold]RICERCA BINARIA[/]\n\n"
        "Idea: come cercare in un dizionario.\n"
        "Apri a metà → stai prima o dopo? → vai in quella metà.\n"
        f"Lista ordinata: {arr}\n"
        f"Cerchiamo: [bold cyan]{target}[/]",
        border_style="yellow"
    ))
    aspetta()

    passo = 0
    while sinistra <= destra:
        passo += 1
        medio = (sinistra + destra) // 2
        zona_attiva = tuple(range(sinistra, destra + 1))

        mostra_array(arr, confronto=zona_attiva,
                     messaggio=f"Passo {passo}: zona [{sinistra}–{destra}], guardo centro [{medio}] = {arr[medio]}",
                     titolo="Ricerca Binaria")
        aspetta()

        if arr[medio] == target:
            mostra_array(arr, trovato=medio,
                         messaggio=f"✓ Trovato {target} in {passo} passi! (ricerca lineare ne avrebbe usati fino a {n})",
                         titolo="Ricerca Binaria — TROVATO!")
            _pausa_fine_demo()
            return medio
        elif arr[medio] < target:
            mostra_array(arr, confronto=tuple(range(medio + 1, destra + 1)),
                         messaggio=f"→ {arr[medio]} < {target}: cerco a DESTRA",
                         titolo="Ricerca Binaria")
            sinistra = medio + 1
        else:
            mostra_array(arr, confronto=tuple(range(sinistra, medio)),
                         messaggio=f"→ {arr[medio]} > {target}: cerco a SINISTRA",
                         titolo="Ricerca Binaria")
            destra = medio - 1
        aspetta()

    mostra_array(arr, messaggio=f"✗ {target} non trovato in {passo} passi.",
                 titolo="Ricerca Binaria — NON TROVATO")
    _pausa_fine_demo()
    return -1


# ── Counting Sort ─────────────────────────────────────────────────────────────

def counting_sort(arr):
    """
    COUNTING SORT
    Conta quante volte appare ogni valore.
    Ricostruisci la lista usando i conteggi.
    Non confronta mai elementi — O(n + max_valore).
    """
    arr = arr.copy()
    n = len(arr)
    max_val = max(arr)

    console.print(Panel(
        "[bold]COUNTING SORT[/]\n\n"
        "Idea: conta quante volte appare ogni numero.\n"
        "Poi ricostruisci la lista in ordine dai conteggi.\n"
        f"Non confronta mai due elementi — O(n + {max_val}).",
        border_style="yellow"
    ))
    aspetta()

    # Fase 1: conta occorrenze
    conteggi = [0] * (max_val + 1)
    for i, val in enumerate(arr):
        conteggi[val] += 1
        mostra_array(arr, confronto=(i,),
                     messaggio=f"Passo {i+1}: arr[{i}]={val} → conteggi[{val}]={conteggi[val]}",
                     titolo="Counting Sort — Fase 1: Conteggio")
        aspetta()

    console.print("\n  [cyan]Tabella conteggi:[/]")
    for v in range(max_val + 1):
        if conteggi[v] > 0:
            console.print(f"    valore [bold]{v}[/] → {conteggi[v]} occorrenza/e")
    aspetta()

    # Fase 2: ricostruzione
    risultato = []
    for valore in range(max_val + 1):
        for _ in range(conteggi[valore]):
            risultato.append(valore)
            mostra_array(risultato,
                         ordinati=set(range(len(risultato))),
                         messaggio=f"Aggiungo {valore} — lista parziale: {risultato}",
                         titolo="Counting Sort — Fase 2: Ricostruzione")
            aspetta()

    mostra_array(risultato, ordinati=set(range(n)),
                 messaggio="✓ Lista ordinata! Nessun confronto tra elementi.",
                 titolo="Counting Sort — COMPLETATO")
    _pausa_fine_demo()
    return risultato


# ── Merge Sort ─────────────────────────────────────────────────────────────────

def merge_sort(arr):
    """
    MERGE SORT (Bottom-Up)
    Parti da coppie → fondi in blocchi da 4 → poi 8 → etc.
    Divide & Conquer — O(n log n) sempre.
    """
    arr = arr.copy()
    n = len(arr)

    console.print(Panel(
        "[bold]MERGE SORT — Bottom-Up[/]\n\n"
        "Idea: parti da coppie, fondi in blocchi sempre più grandi.\n"
        "Divide & Conquer: ogni fusione produce un segmento ordinato.\n"
        "Complessità: O(n log n) — stabile, sempre garantito.",
        border_style="yellow"
    ))
    aspetta()

    ampiezza = 1
    passaggio = 0

    while ampiezza < n:
        passaggio += 1
        console.print(f"\n  [yellow]Passaggio {passaggio}: fondo blocchi da {ampiezza} → {ampiezza * 2}[/]")
        aspetta()

        nuovo = arr.copy()
        for start in range(0, n, ampiezza * 2):
            mid  = min(start + ampiezza, n)
            fine = min(start + ampiezza * 2, n)

            left  = arr[start:mid]
            right = arr[mid:fine]
            i = j = 0
            k = start

            while i < len(left) and j < len(right):
                if left[i] <= right[j]:
                    nuovo[k] = left[i]; i += 1
                else:
                    nuovo[k] = right[j]; j += 1
                k += 1
            while i < len(left):
                nuovo[k] = left[i]; i += 1; k += 1
            while j < len(right):
                nuovo[k] = right[j]; j += 1; k += 1

            mostra_array(nuovo,
                         ordinati=set(range(start, fine)),
                         messaggio=f"Fondi [{start}:{mid}] con [{mid}:{fine}] → segmento ordinato",
                         titolo=f"Merge Sort — Passaggio {passaggio}")
            aspetta()

        arr = nuovo
        ampiezza *= 2

    mostra_array(arr, ordinati=set(range(n)),
                 messaggio=f"✓ Lista ordinata in {passaggio} passaggi — O(n log n)!",
                 titolo="Merge Sort — COMPLETATO")
    _pausa_fine_demo()
    return arr


# ── Quick Sort ─────────────────────────────────────────────────────────────────

def quick_sort(arr):
    """
    QUICK SORT
    Scegli un pivot. Metti i minori a sinistra, i maggiori a destra.
    Ricorsione sulle due parti.
    O(n log n) medio — O(n²) caso peggiore.
    """
    arr = arr.copy()
    n = len(arr)

    console.print(Panel(
        "[bold]QUICK SORT[/]\n\n"
        "Idea: scegli un pivot (ultimo elemento).\n"
        "Tutti i minori a sinistra, tutti i maggiori a destra.\n"
        "Ripeti nelle due metà — O(n log n) medio.",
        border_style="yellow"
    ))
    aspetta()

    ordinati = set()

    def particiona(basso, alto):
        pivot = arr[alto]
        mostra_array(arr, confronto=(alto,), ordinati=ordinati,
                     messaggio=f"Pivot scelto: arr[{alto}] = {pivot}",
                     titolo="Quick Sort — Scelta Pivot")
        aspetta()

        i = basso - 1
        for j in range(basso, alto):
            mostra_array(arr, confronto=(j, alto), ordinati=ordinati,
                         messaggio=f"Confronto arr[{j}]={arr[j]} vs pivot {pivot}",
                         titolo="Quick Sort — Partizione")
            aspetta()
            if arr[j] <= pivot:
                i += 1
                if i != j:
                    arr[i], arr[j] = arr[j], arr[i]
                    mostra_array(arr, scambio=(i, j), ordinati=ordinati,
                                 messaggio=f"→ {arr[j]} ≤ {pivot}: va a sinistra → SCAMBIO posizioni {i} e {j}",
                                 titolo="Quick Sort — Partizione")
                    aspetta()
                else:
                    mostra_array(arr, confronto=(j, alto), ordinati=ordinati,
                                 messaggio=f"→ {arr[j]} ≤ {pivot}: va a sinistra (già in posizione, nessuno scambio)",
                                 titolo="Quick Sort — Partizione")
                    aspetta()
            else:
                mostra_array(arr, confronto=(j, alto), ordinati=ordinati,
                             messaggio=f"→ {arr[j]} > {pivot}: rimane a destra del pivot, nessuno scambio",
                             titolo="Quick Sort — Partizione")
                aspetta()

        arr[i + 1], arr[alto] = arr[alto], arr[i + 1]
        pos_pivot = i + 1
        ordinati.add(pos_pivot)
        mostra_array(arr, scambio=(pos_pivot, alto), ordinati=ordinati,
                     messaggio=f"→ Pivot {pivot} nella posizione definitiva: {pos_pivot}",
                     titolo="Quick Sort — Pivot posizionato")
        aspetta()
        return pos_pivot

    # Iterativo con stack esplicito (evita recursion depth)
    stack = [(0, n - 1)]
    while stack:
        basso, alto = stack.pop()
        if basso < alto:
            p = particiona(basso, alto)
            stack.append((basso, p - 1))
            stack.append((p + 1, alto))

    mostra_array(arr, ordinati=set(range(n)),
                 messaggio="✓ Lista ordinata con Quick Sort!",
                 titolo="Quick Sort — COMPLETATO")
    _pausa_fine_demo()
    return arr


# ── Heap Sort ──────────────────────────────────────────────────────────────────

def heap_sort(arr):
    """
    HEAP SORT
    Costruisci un Max-Heap (il massimo è sempre in cima).
    Estrai la cima → mettila alla fine → aggiusta l'heap.
    O(n log n) — non richiede memoria extra.
    """
    arr = arr.copy()
    n = len(arr)
    ordinati = set()

    console.print(Panel(
        "[bold]HEAP SORT[/]\n\n"
        "Idea: costruisci un Max-Heap (il più grande è sempre in cima).\n"
        "Estrai la cima → mettila alla fine → aggiusta la struttura.\n"
        "Complessità: O(n log n) — non usa memoria extra.",
        border_style="yellow"
    ))
    aspetta()

    def heapify(dimensione, i):
        massimo = i
        sx = 2 * i + 1
        dx = 2 * i + 2
        if sx < dimensione and arr[sx] > arr[massimo]:
            massimo = sx
        if dx < dimensione and arr[dx] > arr[massimo]:
            massimo = dx
        if massimo != i:
            arr[i], arr[massimo] = arr[massimo], arr[i]
            mostra_array(arr, scambio=(i, massimo), ordinati=ordinati,
                         messaggio=f"Heapify: scambio [{i}]={arr[i]} e [{massimo}]={arr[massimo]}",
                         titolo="Heap Sort — Heapify")
            aspetta()
            heapify(dimensione, massimo)

    # Fase 1: costruisci Max-Heap
    console.print("\n  [yellow]Fase 1: Costruisco il Max-Heap[/]")
    aspetta()
    for i in range(n // 2 - 1, -1, -1):
        heapify(n, i)

    mostra_array(arr, confronto=tuple(range(n)),
                 messaggio=f"Max-Heap pronto! La radice (massimo) è: {arr[0]}",
                 titolo="Heap Sort — Max-Heap costruito")
    aspetta()

    # Fase 2: estrai uno a uno
    console.print("\n  [yellow]Fase 2: Estraggo il massimo e lo metto alla fine[/]")
    aspetta()
    for i in range(n - 1, 0, -1):
        arr[0], arr[i] = arr[i], arr[0]
        ordinati.add(i)
        mostra_array(arr, scambio=(0, i), ordinati=ordinati,
                     messaggio=f"Estraggo massimo {arr[i]} → posizione {i}",
                     titolo="Heap Sort — Estrazione")
        aspetta()
        heapify(i, 0)

    ordinati.add(0)
    mostra_array(arr, ordinati=set(range(n)),
                 messaggio="✓ Lista ordinata con Heap Sort!",
                 titolo="Heap Sort — COMPLETATO")
    _pausa_fine_demo()
    return arr


# ── Stack e Queue ──────────────────────────────────────────────────────────────

def demo_stack_queue():
    """Dimostrazione interattiva di Stack (LIFO) e Queue (FIFO)."""
    from collections import deque

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]STACK e QUEUE — Strutture Dati Fondamentali[/]\n\n"
        "[yellow]STACK (Pila) — LIFO: Last In First Out[/]\n"
        "Come una pila di piatti: aggiungi sopra, prendi dall'alto.\n"
        "Python: usa una lista — append() per push, pop() per pop.\n\n"
        "[cyan]QUEUE (Coda) — FIFO: First In First Out[/]\n"
        "Come una fila alla cassa: il primo che entra è il primo a uscire.\n"
        "Python: usa deque — append() per enqueue, popleft() per dequeue.",
        border_style="yellow"
    ))
    aspetta()

    # ── Demo Stack ──
    console.print(Panel("[bold yellow]DEMO STACK (LIFO)[/]\n"
                        "Simulazione: push 10, 20, 30 → pop → push 40 → pop",
                        border_style="yellow"))
    stack: list = []
    operazioni_stack = [
        ("push", 10), ("push", 20), ("push", 30),
        ("pop", None), ("push", 40), ("pop", None), ("pop", None),
    ]
    for op, val in operazioni_stack:
        if op == "push":
            stack.append(val)
            console.print(f"\n  [green]push({val})[/]  → Stack (base→cima): {stack}")
            console.print(f"  Top (cima): [bold]{stack[-1]}[/]")
        else:
            if stack:
                rimosso = stack.pop()
                console.print(f"\n  [red]pop()[/]       → rimosso [bold]{rimosso}[/]  Stack ora: {stack}")
            else:
                console.print("\n  [red]pop()[/]       → Stack VUOTO! (IndexError in Python)")
        aspetta()

    # ── Demo Queue ──
    console.print(Panel("[bold cyan]DEMO QUEUE (FIFO)[/]\n"
                        "Simulazione: enqueue 10, 20, 30 → dequeue → enqueue 40 → dequeue",
                        border_style="cyan"))
    queue: deque = deque()
    operazioni_queue = [
        ("enqueue", 10), ("enqueue", 20), ("enqueue", 30),
        ("dequeue", None), ("enqueue", 40), ("dequeue", None),
    ]
    for op, val in operazioni_queue:
        if op == "enqueue":
            queue.append(val)
            console.print(f"\n  [green]enqueue({val})[/]  → Queue (fronte→fondo): {list(queue)}")
            console.print(f"  Prossimo in uscita: [bold]{queue[0]}[/]")
        else:
            if queue:
                rimosso = queue.popleft()
                console.print(f"\n  [red]dequeue()[/]    → rimosso [bold]{rimosso}[/]  Queue ora: {list(queue)}")
        aspetta()

    console.print("\n  [bold green]✓ Differenza chiave:[/]")
    console.print("  Stack: l'ULTIMO entrato è il PRIMO a uscire (LIFO)")
    console.print("  Queue: il PRIMO entrato è il PRIMO a uscire (FIFO)")
    console.print("\n  [dim]Usi reali: Stack → chiamate funzioni, undo/redo, DFS")
    console.print("            Queue → code di attesa, BFS, job scheduler[/]")
    _pausa_fine_demo()


# ── Cocktail Sort ─────────────────────────────────────────────────────────────

def cocktail_sort(arr):
    """
    COCKTAIL SHAKER SORT
    Bubble Sort bidirezionale: va avanti E indietro.
    Più veloce del Bubble su liste quasi-ordinate — O(n²).
    """
    arr = arr.copy()
    n = len(arr)
    ordinati = set()

    console.print(Panel(
        "[bold]COCKTAIL SHAKER SORT[/]\n\n"
        "Idea: come Bubble Sort ma bidirezionale.\n"
        "Prima passa da sinistra a destra, poi da destra a sinistra.\n"
        "Più efficiente del Bubble su liste quasi-ordinate — O(n²).",
        border_style="yellow"
    ))
    aspetta()

    inizio, fine = 0, n - 1
    passo = 0
    while inizio < fine:
        scambiato = False
        for i in range(inizio, fine):
            passo += 1
            mostra_array(arr, confronto=(i, i + 1), ordinati=ordinati,
                         messaggio=f"→ Confronto [{i}]={arr[i]} e [{i+1}]={arr[i+1]}",
                         titolo=f"Cocktail Sort — SX→DX (passo {passo})")
            aspetta()
            if arr[i] > arr[i + 1]:
                arr[i], arr[i + 1] = arr[i + 1], arr[i]
                scambiato = True
                mostra_array(arr, scambio=(i, i + 1), ordinati=ordinati,
                             messaggio="SCAMBIO!", titolo="Cocktail Sort")
                aspetta()
        ordinati.add(fine); fine -= 1
        if not scambiato:
            break
        for i in range(fine, inizio, -1):
            passo += 1
            mostra_array(arr, confronto=(i - 1, i), ordinati=ordinati,
                         messaggio=f"← Confronto [{i-1}]={arr[i-1]} e [{i}]={arr[i]}",
                         titolo=f"Cocktail Sort — DX→SX (passo {passo})")
            aspetta()
            if arr[i - 1] > arr[i]:
                arr[i - 1], arr[i] = arr[i], arr[i - 1]
                scambiato = True
                mostra_array(arr, scambio=(i - 1, i), ordinati=ordinati,
                             messaggio="SCAMBIO!", titolo="Cocktail Sort")
                aspetta()
        ordinati.add(inizio); inizio += 1
        if not scambiato:
            break

    mostra_array(arr, ordinati=set(range(n)),
                 messaggio=f"✓ Lista ordinata in {passo} passi!",
                 titolo="Cocktail Sort — COMPLETATO")
    _pausa_fine_demo()
    return arr


# ── Shell Sort ────────────────────────────────────────────────────────────────

def shell_sort(arr):
    """
    SHELL SORT
    Insertion Sort con gap decrescente.
    Prima confronta elementi lontani, poi vicini — O(n log² n).
    """
    arr = arr.copy()
    n = len(arr)

    console.print(Panel(
        "[bold]SHELL SORT[/]\n\n"
        "Idea: come Insertion Sort ma con 'gap' decrescente.\n"
        "Prima confronta elementi lontani, poi sempre più vicini.\n"
        "Gap iniziale = n//2, poi dimezzato ogni passata — O(n log² n).",
        border_style="yellow"
    ))
    aspetta()

    gap = n // 2
    passata = 0
    while gap > 0:
        passata += 1
        console.print(f"\n  [yellow]Passata {passata}: gap = {gap}[/]")
        aspetta()
        for i in range(gap, n):
            chiave = arr[i]
            j = i
            mostra_array(arr, confronto=(i,),
                         messaggio=f"Gap={gap}: prendo arr[{i}]={chiave}",
                         titolo=f"Shell Sort — Gap={gap}")
            aspetta()
            while j >= gap and arr[j - gap] > chiave:
                arr[j] = arr[j - gap]
                mostra_array(arr, scambio=(j, j - gap),
                             messaggio=f"arr[{j-gap}]={arr[j-gap]} > {chiave}: sposto a destra",
                             titolo=f"Shell Sort — Gap={gap}")
                aspetta()
                j -= gap
            arr[j] = chiave
        gap //= 2

    mostra_array(arr, ordinati=set(range(n)),
                 messaggio=f"✓ Lista ordinata in {passata} passate!",
                 titolo="Shell Sort — COMPLETATO")
    _pausa_fine_demo()
    return arr


# ── Gnome Sort ────────────────────────────────────────────────────────────────

def gnome_sort(arr):
    """
    GNOME SORT
    Come un gnomo che ordina vasi: se quello a sinistra è più grande → scambia
    e torna indietro. Altrimenti avanza. O(n²).
    """
    arr = arr.copy()
    n = len(arr)
    ordinati = set()

    console.print(Panel(
        "[bold]GNOME SORT[/]\n\n"
        "Idea: uno gnomo ordina vasi di fiori.\n"
        "Se il vaso a sinistra è più grande → scambia e torna indietro.\n"
        "Altrimenti avanza. Semplicissimo — O(n²).",
        border_style="yellow"
    ))
    aspetta()

    i = passo = 0
    while i < n:
        passo += 1
        if i == 0 or arr[i] >= arr[i - 1]:
            mostra_array(arr, confronto=(i,), ordinati=ordinati,
                         messaggio=f"Passo {passo}: arr[{i}]={arr[i]} OK → avanza",
                         titolo="Gnome Sort")
            aspetta()
            i += 1
        else:
            mostra_array(arr, confronto=(i - 1, i), ordinati=ordinati,
                         messaggio=f"arr[{i-1}]={arr[i-1]} > arr[{i}]={arr[i]} → SCAMBIO, torna indietro",
                         titolo="Gnome Sort")
            aspetta()
            arr[i], arr[i - 1] = arr[i - 1], arr[i]
            i -= 1

    mostra_array(arr, ordinati=set(range(n)),
                 messaggio=f"✓ Lista ordinata in {passo} passi!",
                 titolo="Gnome Sort — COMPLETATO")
    _pausa_fine_demo()
    return arr


# ── Radix Sort ────────────────────────────────────────────────────────────────

def radix_sort(arr):
    """
    RADIX SORT — LSD (Least Significant Digit)
    Ordina cifra per cifra, dalla meno significativa.
    Non comparativo — O(nk) dove k = numero di cifre.
    """
    arr = arr.copy()
    n = len(arr)
    max_val = max(arr)
    num_cifre = len(str(max_val))

    console.print(Panel(
        "[bold]RADIX SORT — LSD (dalla cifra meno significativa)[/]\n\n"
        "Idea: ordina per unità, poi decine, poi centinaia...\n"
        "Non confronta mai due elementi direttamente.\n"
        f"Complessità: O(n × {num_cifre} cifre) — non comparativo.",
        border_style="yellow"
    ))
    aspetta()

    exp = 1
    cifra_num = 0
    NOMI = ["Unità", "Decine", "Centinaia", "Migliaia"]

    while exp <= max_val:
        cifra_num += 1
        nome = NOMI[min(cifra_num - 1, 3)]
        console.print(f"\n  [yellow]Passata {cifra_num}: ordino per {nome} (÷{exp} mod 10)[/]")
        aspetta()

        output = [0] * n
        conteggio = [0] * 10

        for i in range(n):
            c = (arr[i] // exp) % 10
            conteggio[c] += 1
            mostra_array(arr, confronto=(i,),
                         messaggio=f"arr[{i}]={arr[i]} → cifra {nome}: {c}",
                         titolo=f"Radix Sort — {nome}")
            aspetta()

        for i in range(1, 10):
            conteggio[i] += conteggio[i - 1]

        for i in range(n - 1, -1, -1):
            c = (arr[i] // exp) % 10
            output[conteggio[c] - 1] = arr[i]
            conteggio[c] -= 1

        for i in range(n):
            arr[i] = output[i]

        mostra_array(arr, ordinati=set(range(n)),
                     messaggio=f"Dopo ordinamento per {nome}: {arr}",
                     titolo=f"Radix Sort — {nome} completata")
        aspetta()
        exp *= 10

    mostra_array(arr, ordinati=set(range(n)),
                 messaggio="✓ Lista ordinata con Radix Sort!",
                 titolo="Radix Sort — COMPLETATO")
    _pausa_fine_demo()
    return arr


# ── Bucket Sort ───────────────────────────────────────────────────────────────

def bucket_sort(arr):
    """
    BUCKET SORT
    Divide i valori in 'secchi' (intervalli), ordina ogni secchio,
    poi concatena. O(n + k) medio.
    """
    arr_orig = arr.copy()
    n = len(arr_orig)
    max_val, min_val = max(arr_orig), min(arr_orig)
    num_secchi = max(1, n // 2)

    console.print(Panel(
        "[bold]BUCKET SORT[/]\n\n"
        f"Idea: dividi i valori in {num_secchi} 'secchi' (intervalli).\n"
        "Ordina ogni secchio (con Insertion Sort).\n"
        "Unisci tutti i secchi — O(n + k) medio.",
        border_style="yellow"
    ))
    aspetta()

    secchi = [[] for _ in range(num_secchi)]
    for i, val in enumerate(arr_orig):
        idx = int((val - min_val) / (max_val - min_val + 1) * num_secchi)
        idx = min(idx, num_secchi - 1)
        secchi[idx].append(val)
        mostra_array(arr_orig, confronto=(i,),
                     messaggio=f"arr[{i}]={val} → secchio {idx}: {secchi[idx]}",
                     titolo="Bucket Sort — Fase 1: Distribuzione")
        aspetta()

    console.print("\n  [cyan]Secchi dopo distribuzione:[/]")
    for i, s in enumerate(secchi):
        if s:
            console.print(f"    Secchio {i}: {s}")
    aspetta()

    risultato = []
    for i, s in enumerate(secchi):
        s.sort()
        risultato.extend(s)
        if s:
            console.print(f"  Secchio {i} ordinato e unito: {s}")
    aspetta()

    mostra_array(risultato, ordinati=set(range(n)),
                 messaggio="✓ Tutti i secchi uniti — lista ordinata!",
                 titolo="Bucket Sort — COMPLETATO")
    _pausa_fine_demo()
    return risultato


# ── Jump Search ───────────────────────────────────────────────────────────────

def jump_search(arr, target):
    """
    JUMP SEARCH
    Salta di √n posizioni. Quando supera il target, ricerca lineare all'indietro.
    O(√n) — richiede lista ordinata.
    """
    import math
    arr = sorted(arr)
    n = len(arr)
    passo = int(math.sqrt(n))

    console.print(Panel(
        "[bold]JUMP SEARCH[/]\n\n"
        f"Idea: salta di √{n} ≈ {passo} posizioni alla volta.\n"
        "Quando trovi un valore > target → ricerca lineare all'indietro.\n"
        f"Lista ordinata: {arr}\n"
        f"Cerchiamo: [bold cyan]{target}[/]  —  Complessità: O(√n)",
        border_style="yellow"
    ))
    aspetta()

    prev = 0
    curr = passo
    salto = 0

    while curr < n and arr[min(curr, n) - 1] < target:
        salto += 1
        mostra_array(arr, confronto=tuple(range(prev, min(curr, n))),
                     messaggio=f"Salto {salto}: blocco [{prev}:{curr}] — arr[{min(curr,n)-1}]={arr[min(curr,n)-1]} < {target} → salto",
                     titolo="Jump Search — Salto in avanti")
        aspetta()
        prev = curr
        curr += passo

    console.print(f"\n  [cyan]Trovato blocco [{prev}:{min(curr,n)}] — ricerca lineare[/]")
    aspetta()

    for i in range(prev, min(curr, n)):
        mostra_array(arr, confronto=(i,),
                     messaggio=f"Controllo arr[{i}]={arr[i]}",
                     titolo="Jump Search — Ricerca Lineare nel blocco")
        aspetta()
        if arr[i] == target:
            mostra_array(arr, trovato=i,
                         messaggio=f"✓ Trovato {target} in posizione {i}! ({salto} salti + ricerca lineare)",
                         titolo="Jump Search — TROVATO!")
            _pausa_fine_demo()
            return i
        if arr[i] > target:
            break

    mostra_array(arr, messaggio=f"✗ {target} non trovato.",
                 titolo="Jump Search — NON TROVATO")
    _pausa_fine_demo()
    return -1


# ── Interpolation Search ──────────────────────────────────────────────────────

def interpolation_search(arr, target):
    """
    INTERPOLATION SEARCH
    Come cercare in una rubrica: se cerchi 'Z' apri verso la fine.
    Formula: pos = basso + (target-arr[basso])/(arr[alto]-arr[basso]) * (alto-basso)
    O(log log n) medio — O(n) peggiore.
    """
    arr = sorted(arr)
    n = len(arr)

    console.print(Panel(
        "[bold]INTERPOLATION SEARCH[/]\n\n"
        "Idea: come cercare in una rubrica — se cerchi 'Z' apri alla fine.\n"
        "Stima la posizione proporzionalmente al valore cercato.\n"
        f"Lista ordinata: {arr}\n"
        f"Cerchiamo: [bold cyan]{target}[/]  —  O(log log n) medio",
        border_style="yellow"
    ))
    aspetta()

    basso, alto = 0, n - 1
    passo = 0

    while basso <= alto and arr[basso] <= target <= arr[alto]:
        passo += 1
        if arr[alto] == arr[basso]:
            pos = basso
        else:
            pos = basso + int((target - arr[basso]) / (arr[alto] - arr[basso]) * (alto - basso))
        pos = max(basso, min(pos, alto))

        mostra_array(arr, confronto=(pos,),
                     messaggio=f"Passo {passo}: stima posizione {pos} → arr[{pos}]={arr[pos]}",
                     titolo="Interpolation Search")
        aspetta()

        if arr[pos] == target:
            mostra_array(arr, trovato=pos,
                         messaggio=f"✓ Trovato {target} in posizione {pos} dopo {passo} passi!",
                         titolo="Interpolation Search — TROVATO!")
            _pausa_fine_demo()
            return pos
        elif arr[pos] < target:
            basso = pos + 1
            console.print(f"  → arr[{pos}]={arr[pos]} < {target}: cerco a DESTRA [{basso}:{alto}]")
        else:
            alto = pos - 1
            console.print(f"  → arr[{pos}]={arr[pos]} > {target}: cerco a SINISTRA [{basso}:{alto}]")
        aspetta()

    mostra_array(arr, messaggio=f"✗ {target} non trovato in {passo} passi.",
                 titolo="Interpolation Search — NON TROVATO")
    _pausa_fine_demo()
    return -1


# ── Linked List ───────────────────────────────────────────────────────────────

def demo_linked_list():
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]LISTA CONCATENATA (Linked List)[/]\n\n"
        "Ogni nodo ha: VALORE + PUNTATORE al nodo successivo.\n"
        "Nessun accesso diretto per indice — devi scorrere.\n"
        "Inserimento/cancellazione in testa: O(1) — accesso: O(n)",
        border_style="yellow"
    ))
    aspetta()

    class Nodo:
        def __init__(self, val):
            self.val = val
            self.next = None

    def mostra_lista(head, ev=None, msg=""):
        os.system("cls" if sys.platform == "win32" else "clear")
        nodi, curr = [], head
        while curr:
            nodi.append(curr.val)
            curr = curr.next
        catena = ""
        for v in nodi:
            stile = "[bold cyan]" if v == ev else ""
            fine  = "[/]" if v == ev else ""
            catena += f"{stile}[{v}]{fine} → "
        catena += "None"
        console.print(Panel(catena + (f"\n\n  [dim]{msg}[/]" if msg else ""),
                            title="[bold]Linked List[/]", border_style="blue"))

    head = None
    ops = [("testa", 30), ("testa", 20), ("testa", 10),
           ("coda", 40), ("coda", 50), ("rimuovi", 20), ("rimuovi", 30)]

    for op, val in ops:
        if op == "testa":
            n = Nodo(val); n.next = head; head = n
            mostra_lista(head, ev=val, msg=f"inserito({val}) in TESTA — O(1)")
        elif op == "coda":
            n = Nodo(val)
            if not head:
                head = n
            else:
                c = head
                while c.next:
                    c = c.next
                c.next = n
            mostra_lista(head, ev=val, msg=f"inserito({val}) in CODA — O(n): scansione fino alla fine")
        elif op == "rimuovi":
            if head and head.val == val:
                head = head.next
                mostra_lista(head, msg=f"rimosso({val}) dalla testa — O(1)")
            else:
                c = head
                while c and c.next and c.next.val != val:
                    c = c.next
                if c and c.next:
                    c.next = c.next.next
                    mostra_lista(head, msg=f"rimosso({val}) — O(n): trovato e scollegato")
        aspetta()

    console.print("\n  [green]✓ Demo completata![/]")
    console.print("  [dim]Linked List → ottima per inserimenti/cancellazioni frequenti")
    console.print("  Lista Python  → ottima per accesso casuale per indice[/]")
    _pausa_fine_demo()


# ── Binary Search Tree ────────────────────────────────────────────────────────

def demo_bst():
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]BINARY SEARCH TREE (BST)[/]\n\n"
        "Regola: figlio sinistro < nodo < figlio destro.\n"
        "Ricerca e inserimento: O(log n) medio, O(n) peggiore.\n"
        "Visita in-order = lista ordinata!",
        border_style="yellow"
    ))
    aspetta()

    class N:
        def __init__(self, v):
            self.v = v; self.sx = None; self.dx = None

    def ins(r, v):
        if not r:
            return N(v)
        if v < r.v:
            r.sx = ins(r.sx, v)
        else:
            r.dx = ins(r.dx, v)
        return r

    def to_str(n, pre="", is_dx=True):
        if not n:
            return ""
        s = ""
        if n.dx:
            s += to_str(n.dx, pre + ("    " if is_dx else "│   "), True)
        s += pre + ("└── " if is_dx else "┌── ") + str(n.v) + "\n"
        if n.sx:
            s += to_str(n.sx, pre + ("    " if is_dx else "│   "), False)
        return s

    def inorder(n, r=None):
        if r is None:
            r = []
        if n:
            inorder(n.sx, r); r.append(n.v); inorder(n.dx, r)
        return r

    root = None
    for v in [50, 30, 70, 20, 40, 60, 80, 10, 35]:
        root = ins(root, v)
        os.system("cls" if sys.platform == "win32" else "clear")
        console.print(Panel(to_str(root),
                            title=f"[bold]BST dopo inserimento di {v}[/]",
                            border_style="blue"))
        console.print(f"  [dim]In-order (ordinato): {inorder(root)}[/]")
        aspetta()

    target = 40
    console.print(f"\n  [cyan]Ricerca di {target}:[/]")
    aspetta()
    curr = root; percorso = []
    while curr:
        percorso.append(curr.v)
        if curr.v == target:
            console.print(f"  ✓ Trovato! Percorso: {' → '.join(map(str, percorso))}"); break
        elif target < curr.v:
            console.print(f"  {target} < {curr.v} → SINISTRA"); curr = curr.sx
        else:
            console.print(f"  {target} > {curr.v} → DESTRA"); curr = curr.dx
        aspetta()

    _pausa_fine_demo()


# ── Hash Table ────────────────────────────────────────────────────────────────

def demo_hash_table():
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]HASH TABLE — Come funziona dict internamente[/]\n\n"
        "Una funzione hash mappa ogni chiave a un indice (bucket).\n"
        "Collisione: due chiavi nello stesso bucket → chaining.\n"
        "Get/Set: O(1) medio — O(n) peggiore (se tutto collide).",
        border_style="yellow"
    ))
    aspetta()

    SIZE = 7
    tabella = [[] for _ in range(SIZE)]

    def hf(k):
        return hash(k) % SIZE

    def mostra_tab(ev_bucket=None, msg=""):
        os.system("cls" if sys.platform == "win32" else "clear")
        testo = ""
        for i, bucket in enumerate(tabella):
            stile = "[bold cyan]" if i == ev_bucket else "[dim]"
            fine  = "[/]" if i == ev_bucket else "[/]"
            cont  = " → ".join(f"{k}:{v}" for k, v in bucket) if bucket else "vuoto"
            testo += f"  {stile}[{i}][/] {cont}\n"
        console.print(Panel(testo, title="[bold]Hash Table[/]", border_style="blue"))
        if msg:
            console.print(f"  [cyan]{msg}[/]")

    for k, v in [("nome", "Mario"), ("età", 30), ("città", "Catania"),
                 ("prof", "Python"), ("hobby", "coding"), ("anno", 2025)]:
        idx = hf(k)
        tabella[idx].append((k, v))
        msg = f"hash('{k}') = {idx} → bucket [{idx}]"
        if len(tabella[idx]) > 1:
            msg += f"  ⚠️ COLLISIONE! ({len(tabella[idx])} elementi)"
        mostra_tab(ev_bucket=idx, msg=msg)
        aspetta()

    cerca = "città"
    idx = hf(cerca)
    mostra_tab(ev_bucket=idx, msg=f"Ricerca '{cerca}': hash={idx} → bucket [{idx}]")
    aspetta()
    for k, v in tabella[idx]:
        if k == cerca:
            console.print(f"  ✓ Trovato '{cerca}' = '{v}' — O(1)!")
            break
    _pausa_fine_demo()


# ── Min-Heap ──────────────────────────────────────────────────────────────────

def demo_min_heap():
    import heapq
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]MIN-HEAP con heapq[/]\n\n"
        "Il valore minimo è SEMPRE in cima (indice 0).\n"
        "Python usa una lista come heap — parent: (i-1)//2\n"
        "figlio sx: 2i+1, figlio dx: 2i+2\n"
        "heappush/heappop: O(log n) — peek (min): O(1)",
        border_style="yellow"
    ))
    aspetta()

    heap = []
    for v in [15, 3, 7, 1, 8, 5, 12]:
        heapq.heappush(heap, v)
        mostra_array(heap.copy(), confronto=(0,),
                     messaggio=f"heappush({v}) → heap: {heap} — minimo in cima: {heap[0]}",
                     titolo="Min-Heap — heappush")
        aspetta()

    console.print("\n  [cyan]Estraggo tutti in ordine (heapsort!):[/]")
    aspetta()
    estratti = []
    while heap:
        m = heapq.heappop(heap)
        estratti.append(m)
        mostra_array(heap.copy() if heap else [0],
                     confronto=(0,) if heap else (),
                     messaggio=f"heappop() = {m} — estratti finora: {estratti}",
                     titolo="Min-Heap — heappop")
        aspetta()

    console.print(f"\n  ✓ Estratti in ordine crescente: {estratti}")
    console.print("  [dim]heapq → code con priorità, Dijkstra, Top-K elementi[/]")
    _pausa_fine_demo()


# ── BFS ───────────────────────────────────────────────────────────────────────

def demo_bfs():
    from collections import deque
    os.system("cls" if sys.platform == "win32" else "clear")

    grafo = {'A': ['B', 'C'], 'B': ['A', 'D', 'E'],
             'C': ['A', 'F'], 'D': ['B'], 'E': ['B', 'F'], 'F': ['C', 'E']}

    console.print(Panel(
        "[bold]BFS — Breadth-First Search (Visita per Livelli)[/]\n\n"
        "Idea: visita prima tutti i vicini diretti, poi i vicini dei vicini...\n"
        "Usa una Queue (FIFO) — garantisce il percorso più breve.\n"
        "Complessità: O(V + E)  —  V nodi, E archi\n\n"
        "Grafo:   A ─ B ─ D\n"
        "         │   │\n"
        "         C   E ─ F\n"
        "         └───────┘",
        border_style="yellow"
    ))
    aspetta()

    visitati = set(['A'])
    coda = deque(['A'])
    ordine = []
    livello = {'A': 0}

    while coda:
        nodo = coda.popleft()
        ordine.append(nodo)
        os.system("cls" if sys.platform == "win32" else "clear")
        console.print(Panel(
            f"  Nodo corrente:  [bold cyan]{nodo}[/]  (livello {livello[nodo]})\n"
            f"  Coda:           {list(coda)}\n"
            f"  Visitati:       {sorted(visitati)}\n"
            f"  Ordine visita:  {ordine}\n"
            f"  Vicini di {nodo}: {grafo[nodo]}",
            title="[bold]BFS — Visita per Livelli[/]", border_style="blue"))
        aspetta()
        for v in grafo[nodo]:
            if v not in visitati:
                visitati.add(v); coda.append(v)
                livello[v] = livello[nodo] + 1
                console.print(f"  → Aggiungo {v} alla coda (livello {livello[v]})")
        aspetta()

    console.print(Panel(
        f"  Ordine di visita: {' → '.join(ordine)}\n\n"
        + "".join(f"  {n}: livello {l}\n" for n, l in sorted(livello.items(), key=lambda x: x[1])),
        title="[bold]BFS — COMPLETATO[/]", border_style="green"))
    _pausa_fine_demo()


# ── DFS ───────────────────────────────────────────────────────────────────────

def demo_dfs():
    os.system("cls" if sys.platform == "win32" else "clear")

    grafo = {'A': ['B', 'C'], 'B': ['A', 'D', 'E'],
             'C': ['A', 'F'], 'D': ['B'], 'E': ['B', 'F'], 'F': ['C', 'E']}

    console.print(Panel(
        "[bold]DFS — Depth-First Search (Visita in Profondità)[/]\n\n"
        "Idea: vai più in profondità possibile, poi torna indietro (backtrack).\n"
        "Usa uno Stack (implicito nel call stack della ricorsione).\n"
        "Utile per: rilevamento cicli, topological sort, labirinti.\n"
        "Complessità: O(V + E)",
        border_style="yellow"
    ))
    aspetta()

    visitati_lista = []
    call_stack = []

    def dfs(nodo, vis):
        vis.add(nodo)
        visitati_lista.append(nodo)
        call_stack.append(nodo)
        os.system("cls" if sys.platform == "win32" else "clear")
        console.print(Panel(
            f"  Nodo corrente:  [bold cyan]{nodo}[/]\n"
            f"  Call stack:     {call_stack}\n"
            f"  Visitati:       {visitati_lista}\n"
            f"  Vicini di {nodo}: {grafo[nodo]}",
            title="[bold]DFS — Visita in Profondità[/]", border_style="blue"))
        aspetta()
        for v in grafo[nodo]:
            if v not in vis:
                console.print(f"  → Scendo in profondità verso {v}")
                aspetta()
                dfs(v, vis)
                call_stack.pop()
                if call_stack:
                    console.print(f"  ← Backtrack a {call_stack[-1]}")
                    aspetta()

    dfs('A', set())
    console.print(Panel(
        f"  Ordine di visita: {' → '.join(visitati_lista)}\n\n"
        "  BFS → visita per livelli (ampiezza)\n"
        "  DFS → visita in profondità, poi backtrack",
        title="[bold]DFS — COMPLETATO[/]", border_style="green"))
    _pausa_fine_demo()


# ── Dijkstra ──────────────────────────────────────────────────────────────────

def demo_dijkstra():
    import heapq
    os.system("cls" if sys.platform == "win32" else "clear")

    grafo = {
        'A': [('B', 4), ('C', 2)], 'B': [('A', 4), ('D', 3), ('E', 1)],
        'C': [('A', 2), ('B', 5), ('F', 6)], 'D': [('B', 3), ('E', 2)],
        'E': [('B', 1), ('D', 2), ('F', 1)], 'F': [('C', 6), ('E', 1)],
    }

    console.print(Panel(
        "[bold]DIJKSTRA — Percorso più breve[/]\n\n"
        "Idea: espandi sempre il nodo con distanza minima (greedy).\n"
        "Usa una Priority Queue (min-heap) — O((V+E) log V).\n"
        "Funziona solo con pesi NON negativi.\n\n"
        "Grafo:   A─4─B─1─E─1─F\n"
        "         2   3   2\n"
        "         C─5─B   D",
        border_style="yellow"
    ))
    aspetta()

    dist = {n: float('inf') for n in grafo}
    dist['A'] = 0
    prev = {n: None for n in grafo}
    heap = [(0, 'A')]
    visitati = set()

    while heap:
        d, nodo = heapq.heappop(heap)
        if nodo in visitati:
            continue
        visitati.add(nodo)
        ds = "  ".join(f"{n}:{(v if v != float('inf') else '∞')}" for n, v in dist.items())
        os.system("cls" if sys.platform == "win32" else "clear")
        console.print(Panel(
            f"  Espando: [bold cyan]{nodo}[/]  (distanza={d})\n"
            f"  Distanze: {ds}\n"
            f"  Visitati: {sorted(visitati)}\n"
            f"  Vicini: {grafo[nodo]}",
            title="[bold]Dijkstra[/]", border_style="blue"))
        aspetta()
        for v, peso in grafo[nodo]:
            if v in visitati:
                continue
            nd = dist[nodo] + peso
            if nd < dist[v]:
                dist[v] = nd; prev[v] = nodo
                heapq.heappush(heap, (nd, v))
                console.print(f"  → Aggiorno {v}: {nd} via {nodo}+{peso}")
            else:
                console.print(f"  → {v}: {nd} ≥ {dist[v]} (nessun miglioramento)")
        aspetta()

    dest = 'F'
    percorso, c = [], dest
    while c:
        percorso.append(c); c = prev[c]
    percorso.reverse()

    console.print(Panel(
        "  Distanze minime da A:\n" +
        "".join(f"    {n}: {d}\n" for n, d in dist.items()) +
        f"\n  Percorso A → F: {' → '.join(percorso)}  (costo: {dist[dest]})",
        title="[bold]Dijkstra — COMPLETATO[/]", border_style="green"))
    _pausa_fine_demo()


# ── Fibonacci + Memoization ───────────────────────────────────────────────────

def demo_fibonacci():
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]FIBONACCI — Ricorsivo vs Memoization vs Bottom-Up[/]\n\n"
        "F(n) = F(n-1) + F(n-2)   base: F(0)=0, F(1)=1\n\n"
        "Ricorsivo puro:   O(2ⁿ) — ricalcola gli stessi sottoproblemi\n"
        "Memoization:      O(n)  — salva i risultati già calcolati\n"
        "Bottom-Up (DP):   O(n)  — calcola dal basso in su",
        border_style="yellow"
    ))
    aspetta()

    n = 12
    chiamate = [0]

    def fib_rec(x):
        chiamate[0] += 1
        return x if x <= 1 else fib_rec(x - 1) + fib_rec(x - 2)

    chiamate[0] = 0
    r = fib_rec(n)
    console.print(f"\n  [red]Ricorsivo puro F({n}) = {r}[/]")
    console.print(f"  Chiamate: [bold red]{chiamate[0]}[/]  — O(2ⁿ) = {2**n} circa")
    aspetta()

    memo = {}
    chiamate[0] = 0

    def fib_memo(x):
        chiamate[0] += 1
        if x in memo:
            return memo[x]
        memo[x] = x if x <= 1 else fib_memo(x - 1) + fib_memo(x - 2)
        return memo[x]

    r = fib_memo(n)
    console.print(f"\n  [yellow]Memoization F({n}) = {r}[/]")
    console.print(f"  Chiamate: [bold yellow]{chiamate[0]}[/]  — O(n)")
    console.print(f"  Cache: {dict(list(memo.items()))}")
    aspetta()

    console.print(f"\n  [green]Bottom-Up (DP) — animato:[/]")
    aspetta()
    dp = [0] * (n + 1)
    dp[1] = 1
    for i in range(2, n + 1):
        dp[i] = dp[i - 1] + dp[i - 2]
        mostra_array(dp, confronto=(i,), ordinati=set(range(i)),
                     messaggio=f"F({i}) = F({i-1})={dp[i-1]} + F({i-2})={dp[i-2]} = {dp[i]}",
                     titolo="Fibonacci — Bottom-Up DP")
        aspetta()

    console.print(f"\n  ✓ Sequenza: {dp}")
    _pausa_fine_demo()


# ── Torre di Hanoi ────────────────────────────────────────────────────────────

def demo_hanoi():
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]TORRE DI HANOI[/]\n\n"
        "3 pioli: A (sorgente), B (ausiliario), C (destinazione).\n"
        "Regola: non mettere mai un disco grande sopra uno piccolo.\n"
        "n dischi → 2ⁿ - 1 mosse — O(2ⁿ) — classico problema ricorsivo.",
        border_style="yellow"
    ))
    aspetta()

    console.print("  Quanti dischi? (2–5, invio = 4)")
    try:
        n = int(input("  N = ") or "4")
        n = max(2, min(n, 5))
    except ValueError:
        n = 4

    pioli = {'A': list(range(n, 0, -1)), 'B': [], 'C': []}
    mosse = [0]

    def mostra_pioli(msg=""):
        os.system("cls" if sys.platform == "win32" else "clear")
        testo = ""
        for riga in range(n, 0, -1):
            for nome in ['A', 'B', 'C']:
                d = pioli[nome]
                if riga <= len(d):
                    barra = "█" * (d[riga - 1] * 2 - 1)
                    testo += f"  {barra:^13}"
                else:
                    testo += f"  {'│':^13}"
            testo += "\n"
        testo += f"  {'A':^13}{'B':^13}{'C':^13}"
        console.print(Panel(testo, title=f"[bold]Torre di Hanoi — Mossa {mosse[0]}[/]",
                            border_style="blue"))
        if msg:
            console.print(f"  [cyan]{msg}[/]")

    mostra_pioli(f"Configurazione iniziale — {2**n - 1} mosse totali")
    aspetta()

    def hanoi(k, da, a, via):
        if k == 1:
            disco = pioli[da].pop()
            pioli[a].append(disco)
            mosse[0] += 1
            mostra_pioli(f"Mossa {mosse[0]}: disco {disco}  {da} → {a}")
            aspetta()
        else:
            hanoi(k - 1, da, via, a)
            hanoi(1, da, a, via)
            hanoi(k - 1, via, a, da)

    hanoi(n, 'A', 'C', 'B')
    console.print(f"\n  ✓ Completato in {mosse[0]} mosse  (2^{n}-1 = {2**n-1})")
    _pausa_fine_demo()


# ── Crivello di Eratostene ────────────────────────────────────────────────────

def demo_sieve():
    import math
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]CRIVELLO DI ERATOSTENE[/]\n\n"
        "Trova tutti i numeri primi fino a N.\n"
        "Segna i multipli di ogni primo come composti.\n"
        "Complessità: O(n log log n) — inventato nel 276 a.C.!",
        border_style="yellow"
    ))
    aspetta()

    console.print("  Fino a quale numero? (10–100, invio = 50)")
    try:
        n = int(input("  N = ") or "50")
        n = max(10, min(n, 100))
    except ValueError:
        n = 50

    primo = [True] * (n + 1)
    primo[0] = primo[1] = False

    def mostra_crivello(p=None, msg=""):
        os.system("cls" if sys.platform == "win32" else "clear")
        testo = "\n  "
        per_riga = 10
        for i in range(2, n + 1):
            if i % per_riga == 2 and i > 2:
                testo += "\n  "
            if not primo[i]:
                testo += f"[dim]{i:4}[/]"
            elif i == p:
                testo += f"[bold yellow]{i:4}[/]"
            else:
                testo += f"[bold green]{i:4}[/]"
        console.print(Panel(
            testo + "\n\n  [green]verde=primo[/]  [dim]grigio=composto[/]  [yellow]giallo=pivot[/]",
            title="[bold]Crivello di Eratostene[/]", border_style="blue"))
        if msg:
            console.print(f"  [cyan]{msg}[/]")

    mostra_crivello(msg="Configurazione iniziale")
    aspetta()

    for p in range(2, math.isqrt(n) + 1):
        if primo[p]:
            for mult in range(p * p, n + 1, p):
                primo[mult] = False
            mostra_crivello(p=p, msg=f"Pivot {p}: segno multipli {p*p}, {p*p+p}, ... come composti")
            aspetta()

    result = [i for i in range(2, n + 1) if primo[i]]
    mostra_crivello(msg=f"✓ Trovati {len(result)} numeri primi fino a {n}: {result}")
    _pausa_fine_demo()


# ── Two Pointers ──────────────────────────────────────────────────────────────

def demo_two_pointers(arr, target):
    arr = sorted(arr)
    n = len(arr)

    console.print(Panel(
        "[bold]TWO POINTERS — Coppia con somma target[/]\n\n"
        "Idea: un puntatore all'inizio, uno alla fine.\n"
        "Somma troppo grande → sposta destra a sinistra.\n"
        "Somma troppo piccola → sposta sinistra a destra.\n"
        f"Lista ordinata: {arr}   Target = {target}\n"
        "Complessità: O(n) dopo ordinamento O(n log n)",
        border_style="yellow"
    ))
    aspetta()

    sx, dx = 0, n - 1
    passo = trovate = 0

    while sx < dx:
        passo += 1
        somma = arr[sx] + arr[dx]
        mostra_array(arr, confronto=(sx, dx),
                     messaggio=f"Passo {passo}: [{sx}]={arr[sx]} + [{dx}]={arr[dx]} = {somma}  (target={target})",
                     titolo="Two Pointers")
        aspetta()
        if somma == target:
            trovate += 1
            console.print(f"  [green]✓ Coppia #{trovate}: {arr[sx]} + {arr[dx]} = {target}[/]")
            sx += 1; dx -= 1
        elif somma < target:
            console.print(f"  → {somma} < {target}: sposto SINISTRA a destra")
            sx += 1
        else:
            console.print(f"  → {somma} > {target}: sposto DESTRA a sinistra")
            dx -= 1
        aspetta()

    console.print(f"\n  {'✓' if trovate else '✗'} {trovate} coppi{'e' if trovate != 1 else 'a'} trovat{'e' if trovate != 1 else 'a'} con somma {target}")
    _pausa_fine_demo()


# ── Sliding Window ────────────────────────────────────────────────────────────

def demo_sliding_window(arr, k):
    n = len(arr)

    console.print(Panel(
        "[bold]SLIDING WINDOW — Massima somma di K elementi contigui[/]\n\n"
        f"Idea: calcola la somma dei primi K={k} elementi.\n"
        "Poi scorri la finestra: +nuovo elemento, -elemento uscito.\n"
        f"Array: {arr}   K = {k}\n"
        "Complessità: O(n) invece di O(n×k) con forza bruta",
        border_style="yellow"
    ))
    aspetta()

    somma = sum(arr[:k])
    somma_max = somma
    start_max = 0

    mostra_array(arr, confronto=tuple(range(k)),
                 messaggio=f"Finestra iniziale [0:{k}] = {arr[:k]} → somma = {somma}",
                 titolo=f"Sliding Window (K={k})")
    aspetta()

    for i in range(k, n):
        somma += arr[i] - arr[i - k]
        mostra_array(arr, confronto=tuple(range(i - k + 1, i + 1)),
                     messaggio=f"Finestra [{i-k+1}:{i+1}]: +{arr[i]} -{arr[i-k]} → somma={somma}  max={somma_max}",
                     titolo=f"Sliding Window (K={k})")
        aspetta()
        if somma > somma_max:
            somma_max = somma; start_max = i - k + 1

    mostra_array(arr, ordinati=set(range(start_max, start_max + k)),
                 messaggio=f"✓ Massima somma = {somma_max}  sottoarray [{start_max}:{start_max+k}] = {arr[start_max:start_max+k]}",
                 titolo="Sliding Window — COMPLETATO")
    _pausa_fine_demo()


# ── LRU Cache ─────────────────────────────────────────────────────────────────

def demo_lru_cache():
    from collections import OrderedDict
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]LRU CACHE — Least Recently Used[/]\n\n"
        "Strategia: quando la cache è piena, rimuovi l'elemento usato MENO di recente.\n"
        "Implementazione O(1): OrderedDict (dict + lista doubly-linked internamente).\n"
        "Usata ovunque: browser, database, OS, DNS — chiesta in ogni colloquio FAANG.",
        border_style="yellow"
    ))
    aspetta()

    CAP = 3
    cache: OrderedDict = OrderedDict()
    hits = misses = 0

    def mostra_cache(msg=""):
        items = list(cache.items())
        celle = ""
        for k, v in reversed(items):
            celle += f"  [bold cyan][{k}={v}][/]"
        for _ in range(CAP - len(items)):
            celle += "  [dim][---][/]"
        stat = ""
        if hits + misses > 0:
            stat = f"  Hits:[green]{hits}[/]  Miss:[red]{misses}[/]  Hit rate:[yellow]{hits/(hits+misses)*100:.0f}%[/]"
        os.system("cls" if sys.platform == "win32" else "clear")
        console.print(Panel(
            f"  Capacità={CAP}  (sinistra=meno recente, destra=più recente)\n\n"
            f"{celle}\n\n{stat}",
            title="[bold]LRU Cache[/]", border_style="blue"))
        if msg:
            console.print(f"  [cyan]{msg}[/]")

    def get(k):
        nonlocal hits, misses
        if k in cache:
            hits += 1; cache.move_to_end(k)
            mostra_cache(f"get({k}) = {cache[k]}  ✓ HIT — spostato in fondo (più recente)")
            return cache[k]
        misses += 1
        mostra_cache(f"get({k})  ✗ MISS — chiave non in cache")
        return -1

    def put(k, v):
        if k in cache:
            cache.move_to_end(k); cache[k] = v
            mostra_cache(f"put({k},{v}) → aggiornato e spostato in fondo")
        else:
            if len(cache) >= CAP:
                rm_k, rm_v = cache.popitem(last=False)
                mostra_cache(f"put({k},{v}) — Cache piena! Rimuovo LRU: {rm_k}={rm_v}")
                aspetta()
            cache[k] = v; cache.move_to_end(k)
            mostra_cache(f"put({k},{v}) → inserito")

    mostra_cache("Cache inizialmente vuota")
    aspetta()

    for op, k, v in [("put", "A", 1), ("put", "B", 2), ("put", "C", 3),
                     ("get", "A", 0), ("put", "D", 4), ("get", "B", 0),
                     ("get", "C", 0), ("put", "E", 5), ("get", "A", 0)]:
        if op == "put":
            put(k, v)
        else:
            get(k)
        aspetta()

    console.print("\n  [green]✓ Demo LRU Cache completata![/]")
    console.print("  [dim]Python: collections.OrderedDict  o  @functools.lru_cache[/]")
    _pausa_fine_demo()


# ── Programmazione Dinamica ───────────────────────────────────────────────────

def demo_knapsack():
    """Zaino 0/1: tabella DP — prendi o lascia ogni oggetto"""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]ZAINO 0/1 (Knapsack) — Programmazione Dinamica[/]\n\n"
        "Problema: oggetti con peso e valore, zaino con capacità limitata.\n"
        "Regola: ogni oggetto si prende INTERO (0 o 1 volta).\n"
        "DP: dp[i][w] = valore massimo con i oggetti e capacità w.\n"
        "Se peso[i] ≤ w: dp[i][w] = max(dp[i-1][w], dp[i-1][w-peso]+val)\n"
        "Altrimenti:      dp[i][w] = dp[i-1][w]",
        border_style="green"
    ))
    aspetta()

    oggetti = [("Laptop", 3, 4000), ("Telefono", 1, 1000), ("Tablet", 2, 3000),
               ("Camera", 2, 2500), ("Libro",    1,  500)]
    capacita = 5
    n = len(oggetti)
    dp = [[0] * (capacita + 1) for _ in range(n + 1)]

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(f"\n  [bold]Oggetti:[/]  (capacità zaino = {capacita} kg)")
    for i, (nome, peso, valore) in enumerate(oggetti):
        console.print(f"  {i+1}. {nome:<12} peso={peso}kg  valore=€{valore}")
    aspetta()

    for i in range(1, n + 1):
        nome, peso, valore = oggetti[i - 1]
        for w in range(capacita + 1):
            if peso <= w:
                dp[i][w] = max(dp[i-1][w], dp[i-1][w-peso] + valore)
            else:
                dp[i][w] = dp[i-1][w]

        os.system("cls" if sys.platform == "win32" else "clear")
        console.print(f"\n  [bold cyan]Elaboro: {nome}[/]  (peso={peso}kg, valore=€{valore})")
        tab = Table(box=None, show_header=True, padding=(0, 1))
        tab.add_column("Oggetto", style="dim", min_width=12)
        for w in range(capacita + 1):
            tab.add_column(f"w={w}", min_width=5, justify="right")
        for ri in range(i + 1):
            nm = oggetti[ri-1][0][:10] if ri > 0 else "—"
            stile = "bold cyan" if ri == i else "dim"
            cells = [f"[{stile}]{nm}[/]"]
            for w in range(capacita + 1):
                s = "bold green" if ri == i and w == capacita else ("yellow" if ri == i else "white")
                cells.append(f"[{s}]{dp[ri][w]}[/]")
            tab.add_row(*cells)
        console.print(tab)
        aspetta()

    w = capacita
    selezionati = []
    for i in range(n, 0, -1):
        if dp[i][w] != dp[i-1][w]:
            selezionati.append(oggetti[i-1])
            w -= oggetti[i-1][1]

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        f"  [bold green]Valore massimo: €{dp[n][capacita]}[/]\n\n"
        "  Oggetti selezionati:\n" +
        "".join(f"  ✓ {nm:<12} peso={p}kg  valore=€{v}\n" for nm, p, v in reversed(selezionati)) +
        f"\n  Peso totale: {sum(p for _, p, _ in selezionati)}kg / {capacita}kg",
        title="[bold]Zaino 0/1 — SOLUZIONE[/]", border_style="green"
    ))
    _pausa_fine_demo()


def demo_coin_change():
    """Coin Change: numero minimo di monete per un importo"""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]COIN CHANGE — Programmazione Dinamica[/]\n\n"
        "Problema: dato un importo e monete di vari tagli, quante monete minime?\n"
        "DP: dp[i] = numero minimo di monete per fare importo i.\n"
        "Relazione: dp[i] = min(dp[i - moneta] + 1) per ogni moneta ≤ i.\n"
        "Complessità: O(importo × num_monete).",
        border_style="green"
    ))
    aspetta()

    monete  = [1, 5, 10, 25]
    importo = 41
    INF = float('inf')
    dp = [INF] * (importo + 1)
    dp[0] = 0
    da_dove = [-1] * (importo + 1)

    for i in range(1, importo + 1):
        for m in monete:
            if m <= i and dp[i - m] + 1 < dp[i]:
                dp[i] = dp[i - m] + 1
                da_dove[i] = m
        if i % 8 == 0 or i == importo:
            os.system("cls" if sys.platform == "win32" else "clear")
            console.print(f"\n  [bold]Coin Change[/]  Monete: {monete}  Importo: {importo}¢")
            console.print(f"\n  Stato DP (celle = monete minime per quell'importo):")
            row = ""
            for j in range(i + 1):
                val = "∞" if dp[j] == INF else str(dp[j])
                s = "bold green" if j == i else ("yellow" if dp[j] != INF else "red")
                row += f"[{s}][{j}]={val}[/]  "
                if (j + 1) % 12 == 0:
                    console.print("  " + row); row = ""
            if row:
                console.print("  " + row)
            aspetta()

    usate = []
    cur = importo
    while cur > 0:
        usate.append(da_dove[cur])
        cur -= da_dove[cur]

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        f"  Importo: [bold]{importo}¢[/]\n"
        f"  Monete disponibili: {monete}\n\n"
        f"  [bold green]Minimo {dp[importo]} monete:[/]  {sorted(usate, reverse=True)}\n"
        f"  Somma verifica: {sum(usate)}¢",
        title="[bold]Coin Change — SOLUZIONE[/]", border_style="green"
    ))
    _pausa_fine_demo()


def demo_lcs():
    """LCS: Longest Common Subsequence — sottosequenza comune più lunga"""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]LCS — Longest Common Subsequence[/]\n\n"
        "Trovare la sottosequenza comune più lunga tra due stringhe.\n"
        "NOTA: sottosequenza NON deve essere contigua (≠ sottostringa).\n"
        "DP: dp[i][j] = LCS tra s1[:i] e s2[:j].\n"
        "Se s1[i]==s2[j]: dp[i][j] = dp[i-1][j-1] + 1\n"
        "Altrimenti:       dp[i][j] = max(dp[i-1][j], dp[i][j-1])\n"
        "Usato in: diff, git, DNA alignment, plagiarism detection.",
        border_style="green"
    ))
    aspetta()

    s1 = "AGGTAB"
    s2 = "GXTXAYB"
    m, n = len(s1), len(s2)
    dp = [[0] * (n + 1) for _ in range(m + 1)]

    for i in range(1, m + 1):
        for j in range(1, n + 1):
            if s1[i-1] == s2[j-1]:
                dp[i][j] = dp[i-1][j-1] + 1
            else:
                dp[i][j] = max(dp[i-1][j], dp[i][j-1])

        os.system("cls" if sys.platform == "win32" else "clear")
        console.print(f"\n  [bold]LCS[/]  s1=[bold cyan]{s1}[/]  s2=[bold yellow]{s2}[/]")
        tab = Table(box=None, show_header=True, padding=(0, 1))
        tab.add_column("", style="dim")
        tab.add_column("ε", justify="right")
        for c in s2:
            tab.add_column(f"[yellow]{c}[/]", justify="right", min_width=3)
        for ri in range(i + 1):
            lbl = f"[cyan]{s1[ri-1]}[/]" if ri > 0 else "ε"
            cells = [lbl] + [str(dp[ri][j]) for j in range(n + 1)]
            tab.add_row(*cells)
        console.print(tab)
        aspetta()

    i, j = m, n
    lcs = []
    while i > 0 and j > 0:
        if s1[i-1] == s2[j-1]:
            lcs.append(s1[i-1]); i -= 1; j -= 1
        elif dp[i-1][j] > dp[i][j-1]:
            i -= 1
        else:
            j -= 1
    lcs.reverse()

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        f"  s1 = [cyan]{s1}[/]\n  s2 = [yellow]{s2}[/]\n\n"
        f"  [bold green]LCS = {''.join(lcs)}[/]  (lunghezza = {dp[m][n]})",
        title="[bold]LCS — SOLUZIONE[/]", border_style="green"
    ))
    _pausa_fine_demo()


def demo_edit_distance():
    """Edit Distance (Levenshtein): minimo operazioni per trasformare una stringa"""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]EDIT DISTANCE — Levenshtein[/]\n\n"
        "Quante operazioni minime per trasformare la parola A nella parola B?\n"
        "Operazioni: inserimento, cancellazione, sostituzione (costo 1 cad.).\n"
        "DP: dp[i][j] = edit distance tra s1[:i] e s2[:j].\n"
        "Usato in: correttori ortografici, diff, fuzzy search, DNA comparison.",
        border_style="green"
    ))
    aspetta()

    s1 = "SUNDAY"
    s2 = "SATURDAY"
    m, n = len(s1), len(s2)
    dp = [[0] * (n + 1) for _ in range(m + 1)]
    for i in range(m + 1): dp[i][0] = i
    for j in range(n + 1): dp[0][j] = j

    for i in range(1, m + 1):
        for j in range(1, n + 1):
            if s1[i-1] == s2[j-1]:
                dp[i][j] = dp[i-1][j-1]
            else:
                dp[i][j] = 1 + min(dp[i-1][j], dp[i][j-1], dp[i-1][j-1])

        os.system("cls" if sys.platform == "win32" else "clear")
        console.print(f"\n  [bold]Edit Distance[/]  '{s1}' → '{s2}'")
        tab = Table(box=None, show_header=True, padding=(0, 1))
        tab.add_column("", style="dim")
        tab.add_column("ε", justify="right")
        for c in s2:
            tab.add_column(f"[yellow]{c}[/]", justify="right", min_width=3)
        for ri in range(i + 1):
            lbl = f"[cyan]{s1[ri-1]}[/]" if ri > 0 else "ε"
            cells = [lbl] + [str(dp[ri][j]) for j in range(n + 1)]
            tab.add_row(*cells)
        console.print(tab)
        aspetta()

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        f"  '{s1}'  →  '{s2}'\n\n"
        f"  [bold green]Edit Distance = {dp[m][n]}[/]\n"
        f"  Servono {dp[m][n]} operazioni (inserimento/cancellazione/sostituzione).",
        title="[bold]Edit Distance — SOLUZIONE[/]", border_style="green"
    ))
    _pausa_fine_demo()


def demo_lis():
    """LIS: Longest Increasing Subsequence — sottosequenza crescente più lunga"""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]LIS — Longest Increasing Subsequence[/]\n\n"
        "Trovare la sottosequenza strettamente crescente più lunga in un array.\n"
        "DP: dp[i] = lunghezza LIS che finisce all'indice i.\n"
        "dp[i] = 1 + max(dp[j] per j<i dove arr[j]<arr[i]).\n"
        "Complessità: O(n²) con DP, O(n log n) con patience sorting.",
        border_style="green"
    ))
    aspetta()

    arr = [10, 22, 9, 33, 21, 50, 41, 60, 80]
    n = len(arr)
    dp = [1] * n
    parent = [-1] * n

    for i in range(1, n):
        for j in range(i):
            if arr[j] < arr[i] and dp[j] + 1 > dp[i]:
                dp[i] = dp[j] + 1
                parent[i] = j

        os.system("cls" if sys.platform == "win32" else "clear")
        console.print(f"\n  [bold]LIS[/]  Array: {arr}")
        console.print(f"  Elaboro indice [bold cyan]{i}[/] = {arr[i]}")
        row_arr = "  "
        row_dp  = "  "
        for k in range(n):
            s = "bold cyan" if k == i else ("green" if k < i else "dim")
            row_arr += f"[{s}][{arr[k]:3}][/]"
            val = str(dp[k]) if k <= i else " ?"
            row_dp  += f"[{s}][{val:3}][/]"
        console.print(f"\n  Valori: {row_arr}")
        console.print(f"  dp[i]:  {row_dp}")
        console.print(f"\n  dp[{i}] = {dp[i]}  (LIS che finisce su {arr[i]})")
        aspetta()

    end = dp.index(max(dp))
    lis = []
    cur = end
    while cur != -1:
        lis.append(arr[cur]); cur = parent[cur]
    lis.reverse()

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        f"  Array: {arr}\n\n"
        f"  dp:    {dp}\n\n"
        f"  [bold green]LIS = {lis}[/]  (lunghezza = {max(dp)})",
        title="[bold]LIS — SOLUZIONE[/]", border_style="green"
    ))
    _pausa_fine_demo()


# ── Algoritmi su Stringhe ─────────────────────────────────────────────────────

def demo_kmp():
    """KMP: ricerca pattern con failure function — O(n+m)"""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]KMP — Knuth-Morris-Pratt Search[/]\n\n"
        "Trova un pattern nel testo senza ripartire da zero dopo ogni mismatch.\n"
        "Costruisce la 'failure function' (lps) per saltare confronti già fatti.\n"
        "lps[i] = prefisso più lungo che è anche suffisso di pattern[:i+1].\n"
        "Complessità: O(n + m)  vs  O(n·m) del naive.",
        border_style="magenta"
    ))
    aspetta()

    testo   = "ABABDABACDABABCABAB"
    pattern = "ABABCABAB"
    n, m = len(testo), len(pattern)

    lps = [0] * m
    lunghezza = 0
    i = 1
    while i < m:
        if pattern[i] == pattern[lunghezza]:
            lunghezza += 1; lps[i] = lunghezza; i += 1
        elif lunghezza:
            lunghezza = lps[lunghezza - 1]
        else:
            lps[i] = 0; i += 1

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(f"  Pattern: [bold cyan]{pattern}[/]")
    console.print(f"  LPS:     {lps}  ← failure function")
    console.print("  lps[i] = prefisso più lungo = suffisso in pattern[:i+1]")
    aspetta()

    trovati = []
    i = j = 0
    while i < n:
        os.system("cls" if sys.platform == "win32" else "clear")
        testo_colorato = Text()
        for k, c in enumerate(testo):
            if k == i:
                testo_colorato.append(c, style="bold yellow")
            elif any(k >= p and k < p + m for p in trovati):
                testo_colorato.append(c, style="bold green")
            else:
                testo_colorato.append(c)
        console.print("  Testo:   "); console.print(testo_colorato)
        console.print(f"  Pattern: [cyan]{pattern}[/]  pos_testo={i}  pos_pattern={j}")

        if testo[i] == pattern[j]:
            i += 1; j += 1
        if j == m:
            trovati.append(i - j)
            console.print(f"\n  [bold green]✓ TROVATO a indice {i-j}![/]")
            j = lps[j - 1]
        elif i < n and testo[i] != pattern[j]:
            if j:
                j = lps[j - 1]
            else:
                i += 1
        aspetta()

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        f"  Testo:   {testo}\n  Pattern: {pattern}\n\n"
        f"  [bold green]Trovato agli indici: {trovati}[/]",
        title="[bold]KMP — COMPLETATO[/]", border_style="magenta"
    ))
    _pausa_fine_demo()


def demo_rabin_karp():
    """Rabin-Karp: ricerca pattern con rolling hash"""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]RABIN-KARP — Rolling Hash Search[/]\n\n"
        "Calcola l'hash del pattern e confrontalo con l'hash di ogni finestra del testo.\n"
        "Rolling hash: aggiorna l'hash senza ricalcolarlo da zero ad ogni passo.\n"
        "Hash(finestra) = (hash_prec - char_uscente + char_entrante) % primo.\n"
        "Utile per cercare MULTIPLI pattern. O(n+m) medio, O(nm) worst case.",
        border_style="magenta"
    ))
    aspetta()

    testo   = "GEEKS FOR GEEKS"
    pattern = "GEEKS"
    d = 256; q = 101
    n, m = len(testo), len(pattern)
    h = pow(d, m - 1, q)
    p_hash = t_hash = 0
    for i in range(m):
        p_hash = (d * p_hash + ord(pattern[i])) % q
        t_hash = (d * t_hash + ord(testo[i])) % q

    trovati = []
    for i in range(n - m + 1):
        os.system("cls" if sys.platform == "win32" else "clear")
        console.print(f"\n  Testo: [white]{testo[:i]}[/][bold yellow]{testo[i:i+m]}[/][white]{testo[i+m:]}[/]")
        console.print(f"  Pattern: [cyan]{pattern}[/]")
        console.print(f"\n  hash_pattern      = [cyan]{p_hash}[/]")
        console.print(f"  hash_finestra[{i}:{i+m}] = [yellow]{t_hash}[/]")

        if p_hash == t_hash:
            if testo[i:i+m] == pattern:
                trovati.append(i)
                console.print(f"  [bold green]✓ Hash match + verifica → TROVATO a {i}![/]")
            else:
                console.print(f"  [bold red]⚠ Hash collision! Verifica → FALSO POSITIVO[/]")
        else:
            console.print(f"  [dim]Hash diverso → skip[/]")

        if i < n - m:
            t_hash = (d * (t_hash - ord(testo[i]) * h) + ord(testo[i + m])) % q
            if t_hash < 0: t_hash += q
        aspetta()

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        f"  Testo: {testo}\n  Pattern: [cyan]{pattern}[/]\n\n"
        f"  [bold green]Trovato agli indici: {trovati}[/]",
        title="[bold]Rabin-Karp — COMPLETATO[/]", border_style="magenta"
    ))
    _pausa_fine_demo()


def demo_z_algorithm():
    """Z-Algorithm: array Z per ricerca pattern in O(n+m)"""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]Z-ALGORITHM — Ricerca Pattern Lineare[/]\n\n"
        "Costruisce l'array Z: Z[i] = lunghezza del prefisso più lungo\n"
        "che corrisponde a una sottostringa che inizia a posizione i.\n"
        "Trucco: concatena pattern + '$' + testo, cerca dove Z[i] = len(pattern).\n"
        "Complessità: O(n + m).",
        border_style="magenta"
    ))
    aspetta()

    pattern = "aab"
    testo   = "baabaa"
    s = pattern + "$" + testo
    n = len(s)
    Z = [0] * n
    Z[0] = n
    l = r = 0

    for i in range(1, n):
        if i < r:
            Z[i] = min(r - i, Z[i - l])
        while i + Z[i] < n and s[Z[i]] == s[i + Z[i]]:
            Z[i] += 1
        if i + Z[i] > r:
            l, r = i, i + Z[i]

        os.system("cls" if sys.platform == "win32" else "clear")
        console.print(f"\n  Stringa: [cyan]{pattern}[/][dim]$[/][white]{testo}[/]")
        console.print(f"  Posizione: [bold yellow]{i}[/]  char='{s[i]}'")
        console.print(f"\n  Array Z finora:")
        row = "  "
        for k in range(i + 1):
            s2 = "bold green" if Z[k] == len(pattern) else ("bold yellow" if k == i else "white")
            row += f"[{s2}][Z{k}={Z[k]}][/]  "
        console.print(row)
        if Z[i] == len(pattern):
            pos = i - len(pattern) - 1
            console.print(f"\n  [bold green]✓ Pattern trovato nel testo a indice {pos}![/]")
        aspetta()

    trovati = [i - len(pattern) - 1 for i in range(len(pattern) + 1, n) if Z[i] == len(pattern)]
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        f"  Pattern: [cyan]{pattern}[/]\n  Testo:   {testo}\n\n"
        f"  Z array: {Z}\n\n"
        f"  [bold green]Trovato agli indici: {trovati}[/]",
        title="[bold]Z-Algorithm — COMPLETATO[/]", border_style="magenta"
    ))
    _pausa_fine_demo()


# ── Grafi Avanzati ────────────────────────────────────────────────────────────

def demo_bellman_ford():
    """Bellman-Ford: shortest path con pesi negativi — O(V·E)"""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]BELLMAN-FORD — Percorso Minimo (pesi negativi)[/]\n\n"
        "A differenza di Dijkstra, funziona anche con archi di peso NEGATIVO.\n"
        "Idea: rilassa tutti gli archi V-1 volte.\n"
        "Se dopo V-1 iterazioni c'è ancora un arco rilassabile → ciclo negativo!\n"
        "Complessità: O(V × E)  vs  Dijkstra O((V+E) log V).",
        border_style="blue"
    ))
    aspetta()

    nodi = ["A", "B", "C", "D", "E"]
    archi = [("A","B",4),("A","C",2),("B","C",-3),("B","D",5),
             ("C","D",6),("D","E",1),("C","E",8)]
    sorgente = "A"
    INF = float('inf')
    dist = {nd: INF for nd in nodi}
    dist[sorgente] = 0

    for iterazione in range(len(nodi) - 1):
        aggiornato = False
        for u, v, w in archi:
            if dist[u] != INF and dist[u] + w < dist[v]:
                dist[v] = dist[u] + w
                aggiornato = True

        os.system("cls" if sys.platform == "win32" else "clear")
        console.print(f"\n  [bold]Bellman-Ford[/]  Iterazione [bold cyan]{iterazione+1}[/] di {len(nodi)-1}")
        console.print(f"  Archi: {[(u,v,w) for u,v,w in archi]}\n")
        tab = Table(box=None, show_header=True, padding=(0, 2))
        tab.add_column("Nodo", style="bold")
        tab.add_column(f"Distanza da {sorgente}", justify="right")
        for nd, d in dist.items():
            s = "green" if d != INF else "dim"
            tab.add_row(nd, f"[{s}]{'∞' if d==INF else d}[/]")
        console.print(tab)
        if not aggiornato:
            console.print("  [dim]Nessun aggiornamento — convergenza anticipata[/]")
        aspetta()

    ciclo_neg = any(dist[u] != INF and dist[u] + w < dist[v] for u, v, w in archi)
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        f"  Distanze minime da {sorgente}:\n\n" +
        "".join(f"  {nd}: {d}\n" for nd, d in dist.items()) +
        ("\n  [bold red]⚠ CICLO NEGATIVO RILEVATO![/]" if ciclo_neg else "\n  [bold green]✓ Nessun ciclo negativo[/]"),
        title="[bold]Bellman-Ford — COMPLETATO[/]", border_style="blue"
    ))
    _pausa_fine_demo()


def demo_floyd_warshall():
    """Floyd-Warshall: tutti i percorsi minimi — O(V³)"""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]FLOYD-WARSHALL — Tutti i Percorsi Minimi[/]\n\n"
        "Dijkstra/Bellman-Ford: percorsi da UNA sorgente.\n"
        "Floyd-Warshall: percorsi minimi tra TUTTE le coppie di nodi.\n"
        "Idea: usa ogni nodo k come intermediario:\n"
        "  dist[i][j] = min(dist[i][j], dist[i][k] + dist[k][j])\n"
        "Complessità: O(V³) — pratico per grafi piccoli (V ≤ 500).",
        border_style="blue"
    ))
    aspetta()

    INF = float('inf')
    nodi = ["A", "B", "C", "D"]
    dist = [
        [0,   5,   INF, 10],
        [INF, 0,   3,   INF],
        [INF, INF, 0,   1],
        [INF, INF, INF, 0],
    ]
    n = len(nodi)

    def stampa_matrice(titolo, k=-1):
        os.system("cls" if sys.platform == "win32" else "clear")
        console.print(f"\n  [bold]{titolo}[/]")
        tab = Table(box=None, show_header=True, padding=(0, 2))
        tab.add_column("", style="dim")
        for nd in nodi:
            tab.add_column(nd, justify="right", min_width=5)
        for i in range(n):
            cells = [f"[bold]{nodi[i]}[/]"]
            for j in range(n):
                val = "∞" if dist[i][j] == INF else str(dist[i][j])
                s = "bold green" if dist[i][j] != INF and i != j else ("dim" if dist[i][j] == INF else "white")
                cells.append(f"[{s}]{val}[/]")
            tab.add_row(*cells)
        console.print(tab)
        if k >= 0:
            console.print(f"\n  Intermediario [bold cyan]{nodi[k]}[/]: verifico se passarvi migliora le distanze")

    stampa_matrice("Matrice iniziale (∞ = nessun arco diretto)")
    aspetta()

    for k in range(n):
        for i in range(n):
            for j in range(n):
                if dist[i][k] + dist[k][j] < dist[i][j]:
                    dist[i][j] = dist[i][k] + dist[k][j]
        stampa_matrice(f"Dopo intermediario {nodi[k]}", k)
        aspetta()

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "  Matrice distanze minime tra tutte le coppie calcolata!\n"
        "  Ogni cella dist[i][j] = percorso più breve da i a j.\n\n"
        "  [bold green]✓ Floyd-Warshall completato[/]",
        title="[bold]Floyd-Warshall — COMPLETATO[/]", border_style="blue"
    ))
    _pausa_fine_demo()


def demo_kruskal():
    """Kruskal: Minimum Spanning Tree con Union-Find"""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]KRUSKAL — Minimum Spanning Tree[/]\n\n"
        "Problema: collega tutti i nodi con il minimo costo totale degli archi.\n"
        "Idea: ordina gli archi per peso, aggiungili se non creano un ciclo.\n"
        "Usa Union-Find per rilevare i cicli in O(α(n)) ≈ O(1).\n"
        "Complessità: O(E log E) per l'ordinamento.",
        border_style="blue"
    ))
    aspetta()

    archi = [(1,"A","B"),(3,"B","C"),(4,"A","C"),(2,"A","D"),
             (5,"B","D"),(6,"B","E"),(7,"C","E"),(3,"D","E"),(4,"D","F"),(8,"E","F")]
    archi_ord = sorted(archi)
    parent = {nd: nd for _, u, v in archi for nd in (u, v)}
    rank   = {nd: 0  for nd in parent}

    def find(x):
        while parent[x] != x:
            parent[x] = parent[parent[x]]; x = parent[x]
        return x

    def union(x, y):
        rx, ry = find(x), find(y)
        if rx == ry: return False
        if rank[rx] < rank[ry]: rx, ry = ry, rx
        parent[ry] = rx
        if rank[rx] == rank[ry]: rank[rx] += 1
        return True

    mst = []
    costo = 0
    for peso, u, v in archi_ord:
        os.system("cls" if sys.platform == "win32" else "clear")
        console.print(f"\n  [bold]Kruskal MST[/]  Elaboro arco [{u}--{v}] peso={peso}")
        console.print(f"  MST finora: {mst}  costo={costo}")
        if union(u, v):
            mst.append((u, v, peso))
            costo += peso
            console.print(f"  [bold green]✓ Aggiunto! Nessun ciclo.[/]")
        else:
            console.print(f"  [bold red]✗ Scartato — creerebbe un ciclo.[/]")
        aspetta()

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "  MST (Minimum Spanning Tree):\n" +
        "".join(f"  {u} -- {v}  peso={p}\n" for u, v, p in mst) +
        f"\n  [bold green]Costo totale MST: {costo}[/]",
        title="[bold]Kruskal — COMPLETATO[/]", border_style="blue"
    ))
    _pausa_fine_demo()


def demo_prim():
    """Prim: MST con coda a priorità — alternativa a Kruskal"""
    import heapq
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]PRIM — Minimum Spanning Tree[/]\n\n"
        "Alternativa a Kruskal: costruisce l'MST partendo da un nodo.\n"
        "Ogni passo: aggiungi il nodo non visitato con il costo minore dal MST.\n"
        "Usa una min-heap (coda a priorità).\n"
        "Complessità: O((V+E) log V) con heap.",
        border_style="blue"
    ))
    aspetta()

    grafo = {
        "A": [("B",2),("C",3)],
        "B": [("A",2),("C",1),("D",5)],
        "C": [("A",3),("B",1),("D",4),("E",6)],
        "D": [("B",5),("C",4),("E",2)],
        "E": [("C",6),("D",2)],
    }
    visitati = set()
    mst = []
    costo = 0
    heap = [(0, "A", "A")]

    while heap:
        peso, u, da = heapq.heappop(heap)
        if u in visitati: continue
        visitati.add(u)
        if da != u:
            mst.append((da, u, peso))
            costo += peso

        os.system("cls" if sys.platform == "win32" else "clear")
        console.print(f"\n  [bold]Prim MST[/]  Visito nodo [bold cyan]{u}[/]  (da {da}, peso={peso})")
        console.print(f"  Visitati: {sorted(visitati)}")
        console.print(f"  MST archi: {mst}  costo={costo}")
        for vicino, p in grafo.get(u, []):
            if vicino not in visitati:
                heapq.heappush(heap, (p, vicino, u))
                console.print(f"  → coda: [{u}--{vicino} peso={p}]")
        aspetta()

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "  MST (Minimum Spanning Tree):\n" +
        "".join(f"  {u} -- {v}  peso={p}\n" for u, v, p in mst) +
        f"\n  [bold green]Costo totale MST: {costo}[/]",
        title="[bold]Prim — COMPLETATO[/]", border_style="blue"
    ))
    _pausa_fine_demo()


def demo_astar():
    """A*: ricerca percorso con euristica — più veloce di Dijkstra su griglie"""
    import heapq
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]A* (A-STAR) — Ricerca con Euristica[/]\n\n"
        "Come Dijkstra ma più veloce: usa un'euristica h(n) per guidare la ricerca.\n"
        "  f(n) = g(n) + h(n)\n"
        "  g(n) = costo reale dallo start a n\n"
        "  h(n) = stima del costo da n al goal (euristica ammissibile)\n"
        "Con h=distanza Manhattan su griglia, A* trova il percorso ottimale.\n"
        "Usato in: GPS, videogiochi, robotica.",
        border_style="blue"
    ))
    aspetta()

    griglia = [
        [0,0,0,0,0],
        [0,1,1,0,0],
        [0,0,1,0,0],
        [0,0,0,0,1],
        [0,0,0,0,0],
    ]
    start = (0, 0)
    goal  = (4, 4)
    R, C  = len(griglia), len(griglia[0])

    def h(a, b): return abs(a[0]-b[0]) + abs(a[1]-b[1])

    open_set = [(h(start,goal), 0, start, [start])]
    visitati = set()

    while open_set:
        f, g, curr, path = heapq.heappop(open_set)
        if curr in visitati: continue
        visitati.add(curr)

        os.system("cls" if sys.platform == "win32" else "clear")
        console.print(f"\n  [bold]A*[/]  Nodo: [bold cyan]{curr}[/]  g={g}  h={h(curr,goal):.0f}  f={f:.0f}")
        for r in range(R):
            riga = "  "
            for c in range(C):
                cell = (r, c)
                if griglia[r][c] == 1:       riga += "[bold red]██[/] "
                elif cell == curr:           riga += "[bold cyan]CC[/] "
                elif cell == start:          riga += "[bold green]ST[/] "
                elif cell == goal:           riga += "[bold yellow]GL[/] "
                elif cell in path:           riga += "[blue]··[/] "
                elif cell in visitati:       riga += "[dim]vv[/] "
                else:                        riga += "   "
            console.print(riga)
        console.print(f"\n  Percorso: {path}")

        if curr == goal:
            console.print(f"\n  [bold green]✓ Goal raggiunto! Costo={g}[/]")
            aspetta()
            break

        for dr, dc in [(-1,0),(1,0),(0,-1),(0,1)]:
            nr, nc = curr[0]+dr, curr[1]+dc
            vicino = (nr, nc)
            if 0<=nr<R and 0<=nc<C and griglia[nr][nc]==0 and vicino not in visitati:
                ng = g + 1
                heapq.heappush(open_set, (ng + h(vicino,goal), ng, vicino, path+[vicino]))
        aspetta()

    _pausa_fine_demo()


def demo_topological_sort():
    """Topological Sort (Kahn): ordinamento di un DAG — prerequisiti, dipendenze"""
    from collections import deque
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]TOPOLOGICAL SORT — Ordinamento Topologico (Kahn)[/]\n\n"
        "Applicabile SOLO a DAG (Directed Acyclic Graph — senza cicli).\n"
        "Risultato: ordine lineare dei nodi dove per ogni arco u→v, u viene prima di v.\n"
        "Algoritmo di Kahn: usa il conteggio dei gradi entranti (in-degree).\n"
        "Usato in: dipendenze software, scheduling, compilatori, pipeline dati.",
        border_style="blue"
    ))
    aspetta()

    grafo_desc = {
        "Env Setup": ["Database", "Backend"],
        "Database":  ["Backend", "Tests"],
        "Backend":   ["API", "Tests"],
        "API":       ["Frontend"],
        "Tests":     ["Deploy"],
        "Frontend":  ["Deploy"],
        "Deploy":    [],
    }
    nodi = list(grafo_desc.keys())
    in_degree = {nd: 0 for nd in nodi}
    for u, vicini in grafo_desc.items():
        for v in vicini:
            in_degree[v] += 1

    coda = deque([nd for nd in nodi if in_degree[nd] == 0])
    ordine = []

    while coda:
        nodo = coda.popleft()
        ordine.append(nodo)

        os.system("cls" if sys.platform == "win32" else "clear")
        console.print(f"\n  [bold]Topological Sort[/]  Elaboro: [bold cyan]{nodo}[/]")
        console.print(f"  Ordine finora: {ordine}")
        console.print(f"\n  In-degrees:")
        for nd, d in in_degree.items():
            s = "bold green" if d==0 and nd not in ordine else ("dim" if nd in ordine else "white")
            console.print(f"  [{s}]  {nd}: {d}[/]")

        for vicino in grafo_desc[nodo]:
            in_degree[vicino] -= 1
            if in_degree[vicino] == 0:
                coda.append(vicino)
                console.print(f"  → [yellow]{vicino}[/] entra in coda (in-degree=0)")
        aspetta()

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "  Ordine di esecuzione:\n\n" +
        "".join(f"  {i+1}. {nd}\n" for i, nd in enumerate(ordine)) +
        ("\n  [bold green]✓ Ordinamento valido[/]" if len(ordine)==len(nodi) else "\n  [bold red]⚠ Ciclo rilevato![/]"),
        title="[bold]Topological Sort — COMPLETATO[/]", border_style="blue"
    ))
    _pausa_fine_demo()


# ── Greedy ────────────────────────────────────────────────────────────────────

def demo_activity_selection():
    """Activity Selection: massimo numero di attività non sovrapposte"""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]ACTIVITY SELECTION — Greedy[/]\n\n"
        "Problema: N attività con orario inizio/fine. Quante puoi fare senza sovrapporti?\n"
        "Strategia Greedy: ordina per orario di FINE, scegli sempre la prima disponibile.\n"
        "Complessità: O(n log n) per l'ordinamento.\n"
        "Dimostrabile ottimale per scambio (exchange argument).",
        border_style="bright_yellow"
    ))
    aspetta()

    attivita = [("A",1,4),("B",3,5),("C",0,6),("D",5,7),("E",3,9),
                ("F",5,9),("G",6,10),("H",8,11),("I",8,12),("L",2,14),("M",12,16)]
    attivita_ord = sorted(attivita, key=lambda x: x[2])
    selezionate = []
    fine_prec = -1

    for nome, inizio, fine in attivita_ord:
        os.system("cls" if sys.platform == "win32" else "clear")
        console.print(f"\n  [bold]Activity Selection[/]  Attività ordinate per fine:")
        nomi_sel = [x[0] for x in selezionate]
        for nd, s, e in attivita_ord:
            if nd in nomi_sel:
                style = "bold green"
            elif nd == nome:
                style = "bold yellow"
            else:
                style = "dim"
            barra = "─" * (e - s)
            console.print(f"  [{style}]  {nd}: [{s:2}-{e:2}]  {'·'*s}{barra}[/]")

        console.print(f"\n  Valuto [bold yellow]{nome}[/]: inizio={inizio}, fine={fine}, fine_prec={fine_prec}")
        if inizio >= fine_prec:
            selezionate.append((nome, inizio, fine))
            fine_prec = fine
            console.print(f"  [bold green]✓ Selezionata! (inizia dopo fine precedente)[/]")
        else:
            console.print(f"  [bold red]✗ Scartata (si sovrappone)[/]")
        aspetta()

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        f"  Attività selezionate ({len(selezionate)}):\n" +
        "".join(f"  ✓ {nd}: [{s}-{e}]\n" for nd, s, e in selezionate) +
        f"\n  [bold green]Massimo {len(selezionate)} attività non sovrapposte[/]",
        title="[bold]Activity Selection — COMPLETATO[/]", border_style="bright_yellow"
    ))
    _pausa_fine_demo()


def demo_huffman():
    """Huffman Coding: compressione dati ottimale senza perdita"""
    import heapq
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]HUFFMAN CODING — Compressione Dati[/]\n\n"
        "Caratteri più frequenti → codici più corti. Caratteri rari → codici più lunghi.\n"
        "Costruisce albero binario: unisci sempre i 2 nodi meno frequenti.\n"
        "Ottimo per compressione senza perdita (ZIP, PNG, MP3 usano varianti Huffman).\n"
        "Compressione tipica: 20-90% rispetto a codifica fissa a 8 bit.",
        border_style="bright_yellow"
    ))
    aspetta()

    testo = "AABRACADABRA"
    freq = {}
    for c in testo: freq[c] = freq.get(c, 0) + 1

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(f"  Testo: [bold]{testo}[/]  ({len(testo)} caratteri × 8 bit = {len(testo)*8} bit senza compressione)")
    console.print(f"\n  Frequenze:")
    for c, f in sorted(freq.items(), key=lambda x: -x[1]):
        console.print(f"  '{c}': {f}  {'█'*f}")
    aspetta()

    heap = [(f, c, None, None) for c, f in freq.items()]
    heapq.heapify(heap)
    passo = 0

    while len(heap) > 1:
        f1, l1, sx1, dx1 = heapq.heappop(heap)
        f2, l2, sx2, dx2 = heapq.heappop(heap)
        merged_label = f"({l1}+{l2})"
        merged_freq  = f1 + f2
        heapq.heappush(heap, (merged_freq, merged_label, (f1,l1,sx1,dx1), (f2,l2,sx2,dx2)))
        passo += 1

        os.system("cls" if sys.platform == "win32" else "clear")
        console.print(f"\n  [bold]Huffman — passo {passo}[/]")
        console.print(f"  Unisco: [cyan]{l1}[/](freq={f1}) + [yellow]{l2}[/](freq={f2}) → [bold]{merged_label}[/](freq={merged_freq})")
        console.print(f"\n  Coda attuale:")
        for item in sorted(heap):
            console.print(f"  freq={item[0]}  {item[1]}")
        aspetta()

    def genera_codici(nodo, codice="", codici=None):
        if codici is None: codici = {}
        if nodo is None: return codici
        f, label, sx, dx = nodo
        if sx is None and dx is None:
            codici[label] = codice or "0"
        genera_codici(sx, codice + "0", codici)
        genera_codici(dx, codice + "1", codici)
        return codici

    radice = heap[0]
    codici = genera_codici(radice)
    codici = {k: v for k, v in codici.items() if len(k) == 1}

    bit_compressi = sum(freq[c] * len(codici[c]) for c in freq if c in codici)
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        f"  Testo: {testo}\n\n  Codici Huffman:\n" +
        "".join(f"  '{c}' (freq={freq[c]}): [bold green]{codici.get(c,'?')}[/]  ({len(codici.get(c,'?'))} bit)\n"
                for c in sorted(freq)) +
        f"\n  Bit originali:  {len(testo)*8}\n"
        f"  Bit compressi:  [bold green]{bit_compressi}[/]\n"
        f"  Risparmio:      [bold green]{100 - bit_compressi*100//(len(testo)*8)}%[/]",
        title="[bold]Huffman — COMPLETATO[/]", border_style="bright_yellow"
    ))
    _pausa_fine_demo()


def demo_fractional_knapsack():
    """Zaino Frazionario: Greedy, puoi spezzare gli oggetti"""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]ZAINO FRAZIONARIO — Greedy[/]\n\n"
        "A differenza dello Zaino 0/1, puoi prendere FRAZIONI di ogni oggetto.\n"
        "Strategia Greedy: ordina per densità valore/peso, prendi i migliori prima.\n"
        "Dimostrabile ottimale con argomento di scambio.\n"
        "Complessità: O(n log n) per l'ordinamento.",
        border_style="bright_yellow"
    ))
    aspetta()

    oggetti  = [("Oro",10,60),("Argento",20,100),("Rame",30,120),("Ferro",5,10)]
    capacita = 50
    oggetti_ord = sorted(oggetti, key=lambda x: x[2]/x[1], reverse=True)
    peso_rim = capacita
    valore_tot = 0
    selezionati = []

    for nome, peso, valore in oggetti_ord:
        densita = valore / peso
        os.system("cls" if sys.platform == "win32" else "clear")
        console.print(f"\n  [bold]Zaino Frazionario[/]  Capacità rimasta: {peso_rim}/{capacita} kg")
        console.print(f"\n  Oggetti (ordinati per densità valore/peso):")
        for nd, p, v in oggetti_ord:
            d = v/p
            s = "bold cyan" if nd == nome else "dim"
            console.print(f"  [{s}]  {nd:<10} peso={p}kg  valore=€{v}  densità={d:.2f}[/]")

        if peso_rim <= 0:
            console.print(f"\n  Zaino pieno!"); aspetta(); break

        fraz  = min(1.0, peso_rim / peso)
        preso = fraz * peso
        guad  = fraz * valore
        selezionati.append((nome, fraz, preso, guad))
        peso_rim   -= preso
        valore_tot += guad

        console.print(f"\n  Prendo [bold green]{fraz*100:.0f}%[/] di {nome}: {preso:.1f}kg → €{guad:.1f}")
        aspetta()

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "  Oggetti presi:\n" +
        "".join(f"  ✓ {nd}: {f*100:.0f}%  ({p:.1f}kg)  €{v:.1f}\n" for nd,f,p,v in selezionati) +
        f"\n  [bold green]Valore massimo = €{valore_tot:.1f}[/]",
        title="[bold]Zaino Frazionario — COMPLETATO[/]", border_style="bright_yellow"
    ))
    _pausa_fine_demo()


# ── Backtracking ──────────────────────────────────────────────────────────────

def demo_nqueens():
    """N-Queens: posiziona N regine su scacchiera N×N senza attacchi"""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]N-QUEENS — Backtracking[/]\n\n"
        "Posiziona N regine su una scacchiera N×N: nessuna deve attaccarne un'altra\n"
        "(stessa riga, colonna o diagonale).\n"
        "Strategia: prova colonna per colonna, torna indietro se impossibile.\n"
        "Complessità: O(N!) nel caso peggiore.",
        border_style="red"
    ))
    aspetta()

    try:
        N = int(input("  N (4-8, invio=6): ") or "6")
        N = max(4, min(N, 8))
    except ValueError:
        N = 6

    scacchiera = [-1] * N
    passi = [0]

    def stampa_scacchiera(msg=""):
        os.system("cls" if sys.platform == "win32" else "clear")
        console.print(f"\n  [bold]N-Queens (N={N})[/]  Passi: {passi[0]}  {msg}")
        for r in range(N):
            riga = "  "
            for c in range(N):
                if scacchiera[r] == c:
                    riga += "[bold yellow]♛ [/]"
                elif (r + c) % 2 == 0:
                    riga += "[dim]░░[/]"
                else:
                    riga += "  "
            console.print(riga)

    def sicuro(riga, col):
        for r in range(riga):
            c = scacchiera[r]
            if c == col or abs(c - col) == abs(r - riga):
                return False
        return True

    soluzioni = [0]
    prima = [None]

    def risolvi(riga):
        if riga == N:
            soluzioni[0] += 1
            if prima[0] is None:
                prima[0] = scacchiera[:]
            return True
        for col in range(N):
            passi[0] += 1
            if sicuro(riga, col):
                scacchiera[riga] = col
                if passi[0] <= 25:
                    stampa_scacchiera(f"Riga {riga} → col {col}")
                    input("  INVIO → ")
                if risolvi(riga + 1):
                    return True
                scacchiera[riga] = -1
        return False

    risolvi(0)
    if prima[0]:
        scacchiera[:] = prima[0]
    stampa_scacchiera("Prima soluzione!")
    console.print(Panel(
        f"  N = {N}\n"
        f"  Prima soluzione: {prima[0]}\n"
        f"  Soluzioni totali: [bold green]{soluzioni[0]}[/]\n"
        f"  Passi totali: {passi[0]}",
        title="[bold]N-Queens — COMPLETATO[/]", border_style="red"
    ))
    _pausa_fine_demo()


def demo_sudoku():
    """Sudoku Solver con backtracking"""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]SUDOKU SOLVER — Backtracking[/]\n\n"
        "Risolve un puzzle Sudoku 9×9 con backtracking.\n"
        "Per ogni cella vuota: prova 1-9, verifica riga/colonna/box 3×3.\n"
        "Se nessun valore funziona: torna indietro (backtrack).\n"
        "Esponenziale nel worst case, ma pratico con propagazione vincoli.",
        border_style="red"
    ))
    aspetta()

    griglia = [
        [5,3,0,0,7,0,0,0,0],
        [6,0,0,1,9,5,0,0,0],
        [0,9,8,0,0,0,0,6,0],
        [8,0,0,0,6,0,0,0,3],
        [4,0,0,8,0,3,0,0,1],
        [7,0,0,0,2,0,0,0,6],
        [0,6,0,0,0,0,2,8,0],
        [0,0,0,4,1,9,0,0,5],
        [0,0,0,0,8,0,0,7,9],
    ]
    originale = [r[:] for r in griglia]
    passi = [0]

    def stampa(msg=""):
        os.system("cls" if sys.platform == "win32" else "clear")
        console.print(f"\n  [bold]Sudoku Solver[/]  Passi: {passi[0]}  {msg}")
        for r, riga in enumerate(griglia):
            testo = "  "
            for c, val in enumerate(riga):
                if c in (3, 6): testo += "[dim]│[/] "
                if val == 0:               testo += "[bold red]·[/] "
                elif originale[r][c] != 0: testo += f"[bold white]{val}[/] "
                else:                      testo += f"[bold green]{val}[/] "
            console.print(testo)
            if r in (2, 5):
                console.print("  " + "─"*20)

    def valido(r, c, num):
        if num in griglia[r]: return False
        if num in [griglia[i][c] for i in range(9)]: return False
        br, bc = 3*(r//3), 3*(c//3)
        for i in range(br, br+3):
            for j in range(bc, bc+3):
                if griglia[i][j] == num: return False
        return True

    def risolvi():
        for r in range(9):
            for c in range(9):
                if griglia[r][c] == 0:
                    for num in range(1, 10):
                        if valido(r, c, num):
                            griglia[r][c] = num
                            passi[0] += 1
                            if passi[0] <= 15:
                                stampa(f"Provo {num} in ({r},{c})")
                                input("  INVIO → ")
                            if risolvi(): return True
                            griglia[r][c] = 0
                    return False
        return True

    risolvi()
    stampa("Risolto!")
    console.print(f"\n  [bold green]✓ Sudoku risolto in {passi[0]} passi![/]")
    _pausa_fine_demo()


# ── Algoritmi Matematici ──────────────────────────────────────────────────────

def demo_gcd():
    """GCD con algoritmo di Euclide — O(log min(a,b))"""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]GCD — Algoritmo di Euclide[/]\n\n"
        "MCD (Massimo Comun Divisore) con l'algoritmo più antico del mondo (300 a.C.).\n"
        "Idea: MCD(a, b) = MCD(b, a % b)  →  Caso base: MCD(a, 0) = a\n"
        "Complessità: O(log min(a,b)) — estremamente veloce.\n"
        "Usato in: crittografia RSA, frazioni, MCM, algoritmo esteso.",
        border_style="cyan"
    ))
    aspetta()

    try:
        a = int(input("  Primo numero (invio=48): ") or "48")
        b = int(input("  Secondo numero (invio=18): ") or "18")
    except ValueError:
        a, b = 48, 18

    a_orig, b_orig = a, b
    passo = 0
    while b != 0:
        passo += 1
        resto = a % b
        os.system("cls" if sys.platform == "win32" else "clear")
        console.print(Panel(
            f"  [bold]Passo {passo}:[/]\n\n"
            f"  MCD({a}, {b})\n"
            f"  = MCD({b}, {a} % {b})\n"
            f"  = MCD({b}, [bold cyan]{resto}[/])\n\n"
            f"  [dim]{a} = {b} × {a//b} + {resto}[/]",
            title="[bold]Euclide[/]", border_style="cyan"
        ))
        a, b = b, resto
        aspetta()

    lcm = (a_orig * b_orig) // a
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        f"  MCD({a_orig}, {b_orig}) = [bold green]{a}[/]\n\n"
        f"  MCM({a_orig}, {b_orig}) = {a_orig}×{b_orig} / MCD = [bold cyan]{lcm}[/]\n\n"
        f"  Passi: {passo}",
        title="[bold]GCD — COMPLETATO[/]", border_style="cyan"
    ))
    _pausa_fine_demo()


def demo_modular_exp():
    """Potenza modulare veloce: base^exp % mod in O(log exp)"""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]POTENZA MODULARE — Fast Exponentiation[/]\n\n"
        "Calcola base^esponente mod m in O(log esponente) invece di O(esponente).\n"
        "Idea: se exp pari → b^e = (b^(e/2))²  se dispari → b^e = b × b^(e-1)\n"
        "Fondamentale in crittografia: RSA usa b^e mod n con e,n enormi.\n"
        "Senza questo algoritmo, RSA sarebbe impossibile da calcolare.",
        border_style="cyan"
    ))
    aspetta()

    try:
        b = int(input("  Base (invio=2): ") or "2")
        e = int(input("  Esponente (invio=10): ") or "10")
        m = int(input("  Modulo (invio=1000): ") or "1000")
    except ValueError:
        b, e, m = 2, 10, 1000

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(f"\n  Calcolo {b}^{e} mod {m}\n")
    risultato = 1
    base = b % m
    exp  = e
    passo = 0

    while exp > 0:
        passo += 1
        if exp % 2 == 1:
            risultato = (risultato * base) % m
            console.print(f"  Passo {passo}: exp={exp} (dispari) → risultato={risultato}  base={base}")
        else:
            console.print(f"  Passo {passo}: exp={exp} (pari)    → salta              base={base}")
        base = (base * base) % m
        exp //= 2
        aspetta()

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        f"  {b}^{e} mod {m} = [bold green]{risultato}[/]\n\n"
        f"  Metodo naive: {e} moltiplicazioni.\n"
        f"  Fast exp: [bold cyan]{passo}[/] passi  (O(log {e})).",
        title="[bold]Potenza Modulare — COMPLETATO[/]", border_style="cyan"
    ))
    _pausa_fine_demo()


def demo_miller_rabin():
    """Miller-Rabin: test di primalità probabilistico — O(k log²n)"""
    import math
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]MILLER-RABIN — Test di Primalità[/]\n\n"
        "Determina se un numero è probabilmente primo in O(k × log²n).\n"
        "Con k iterazioni, probabilità di errore ≤ 4^(-k).\n"
        "Usa il Piccolo Teorema di Fermat con condizioni extra per maggiore accuratezza.\n"
        "Usato in: generazione chiavi RSA (testa numeri da 512-4096 bit).",
        border_style="cyan"
    ))
    aspetta()

    def mr_test(n, a):
        if n % 2 == 0: return n == 2
        d, r = n - 1, 0
        while d % 2 == 0: d //= 2; r += 1
        x = pow(a, d, n)
        if x == 1 or x == n - 1: return True
        for _ in range(r - 1):
            x = (x * x) % n
            if x == n - 1: return True
        return False

    numeri = [2, 3, 4, 17, 100, 561, 997, 1009, 7919, 104729]
    basi   = [2, 3, 5, 7]

    for n in numeri:
        risultato = all(mr_test(n, a) for a in basi if a < n)
        os.system("cls" if sys.platform == "win32" else "clear")
        console.print(f"\n  [bold]Miller-Rabin[/]  n = [bold]{n}[/]  basi={basi}\n")
        for a in basi:
            if a < n:
                ok = mr_test(n, a)
                s = "green" if ok else "red"
                console.print(f"  Base {a}: [bold {s}]{'Passa ✓' if ok else 'Fallisce ✗'}[/]")
        s = "bold green" if risultato else "bold red"
        label = "PROBABILMENTE PRIMO" if risultato else "COMPOSTO (non primo)"
        console.print(f"\n  Risultato: [{s}]{label}[/]")
        vero = all(n % i != 0 for i in range(2, int(math.sqrt(n))+1)) and n > 1
        console.print(f"  Verifica esatta: {'Primo ✓' if vero else 'Composto ✓'}")
        aspetta()

    _pausa_fine_demo()


def demo_karatsuba():
    """Karatsuba: moltiplicazione veloce — O(n^1.585) vs O(n²)"""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]KARATSUBA — Moltiplicazione Veloce[/]\n\n"
        "Divide-et-impera per moltiplicare numeri grandi.\n"
        "Naive:     n² operazioni  (4 moltiplicazioni ricorsive)\n"
        "Karatsuba: n^1.585 operazioni  (solo 3 moltiplicazioni ricorsive!)\n\n"
        "Trick: usa (a+b)(c+d) - ac - bd per evitare la quarta moltiplicazione.\n"
        "Usato in: GMP (big integers), crittografia, Python interno per n > 70 cifre.",
        border_style="cyan"
    ))
    aspetta()

    def karatsuba(x, y, livello=0):
        indent = "  " * livello
        console.print(f"\n{indent}[bold cyan]karatsuba({x}, {y})[/]  livello={livello}")
        if x < 10 or y < 10:
            console.print(f"{indent}  [dim]Caso base: {x} × {y} = {x*y}[/]")
            if livello <= 1: aspetta()
            return x * y

        n = max(len(str(x)), len(str(y)))
        m = n // 2
        a, b = x // (10**m), x % (10**m)
        c, d = y // (10**m), y % (10**m)

        console.print(f"{indent}  x={x}: a={a}, b={b}  m={m}")
        console.print(f"{indent}  y={y}: c={c}, d={d}")
        if livello == 0: aspetta()

        ac      = karatsuba(a, c, livello + 1)
        bd      = karatsuba(b, d, livello + 1)
        ab_cd   = karatsuba(a + b, c + d, livello + 1)
        middle  = ab_cd - ac - bd
        risultato = ac * (10**(2*m)) + middle * (10**m) + bd

        os.system("cls" if sys.platform == "win32" else "clear") if livello == 0 else None
        console.print(f"\n{indent}[bold green]Risultato {x} × {y}:[/]")
        console.print(f"{indent}  ac={ac}  bd={bd}  middle={middle}")
        console.print(f"{indent}  = {ac}×10^{2*m} + {middle}×10^{m} + {bd} = [bold green]{risultato}[/]")
        if livello == 0: aspetta()
        return risultato

    try:
        x = int(input("  Primo numero (invio=1234): ") or "1234")
        y = int(input("  Secondo numero (invio=5678): ") or "5678")
    except ValueError:
        x, y = 1234, 5678

    os.system("cls" if sys.platform == "win32" else "clear")
    risultato = karatsuba(x, y)
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        f"  {x} × {y}\n\n"
        f"  Karatsuba: [bold green]{risultato}[/]\n"
        f"  Verifica:  [bold green]{x * y}[/]\n"
        f"  Match: {'✓' if risultato == x*y else '✗'}",
        title="[bold]Karatsuba — COMPLETATO[/]", border_style="cyan"
    ))
    _pausa_fine_demo()


# ── Strutture Dati Avanzate ───────────────────────────────────────────────────

def demo_avl_tree():
    """AVL Tree: BST bilanciato con rotazioni automatiche"""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]AVL TREE — BST Auto-Bilanciato[/]\n\n"
        "Problema BST: nel caso peggiore degrada a lista → O(n) per ricerca.\n"
        "AVL risolve: mantiene |altezza(sx) - altezza(dx)| ≤ 1 con rotazioni.\n"
        "Tipi di rotazione: Destra, Sinistra, Destra-Sinistra, Sinistra-Destra.\n"
        "Garantisce: ricerca, inserimento, cancellazione sempre O(log n).",
        border_style="green"
    ))
    aspetta()

    class Nodo:
        def __init__(self, val):
            self.val = val; self.sx = self.dx = None; self.h = 1

    def altezza(nd): return nd.h if nd else 0
    def aggiorna_h(nd):
        if nd: nd.h = 1 + max(altezza(nd.sx), altezza(nd.dx))
    def fattore(nd): return altezza(nd.sx) - altezza(nd.dx) if nd else 0

    def rot_dx(y):
        x = y.sx; t = x.dx
        x.dx = y; y.sx = t
        aggiorna_h(y); aggiorna_h(x)
        return x

    def rot_sx(x):
        y = x.dx; t = y.sx
        y.sx = x; x.dx = t
        aggiorna_h(x); aggiorna_h(y)
        return y

    def inserisci(nodo, val, log):
        if not nodo: return Nodo(val)
        if val < nodo.val:   nodo.sx = inserisci(nodo.sx, val, log)
        elif val > nodo.val: nodo.dx = inserisci(nodo.dx, val, log)
        else: return nodo
        aggiorna_h(nodo)
        bf = fattore(nodo)
        if bf > 1 and val < nodo.sx.val:
            log.append(f"  [bold yellow]Rotazione DESTRA su {nodo.val}[/]  (caso LL)")
            return rot_dx(nodo)
        if bf < -1 and val > nodo.dx.val:
            log.append(f"  [bold yellow]Rotazione SINISTRA su {nodo.val}[/]  (caso RR)")
            return rot_sx(nodo)
        if bf > 1 and val > nodo.sx.val:
            log.append(f"  [bold yellow]Rotazione SX-DX su {nodo.val}[/]  (caso LR)")
            nodo.sx = rot_sx(nodo.sx); return rot_dx(nodo)
        if bf < -1 and val < nodo.dx.val:
            log.append(f"  [bold yellow]Rotazione DX-SX su {nodo.val}[/]  (caso RL)")
            nodo.dx = rot_dx(nodo.dx); return rot_sx(nodo)
        return nodo

    def stampa_livelli(radice):
        from collections import deque
        if not radice: return
        coda = deque([(radice, 0)])
        liv_prec = 0; riga = ""
        while coda:
            nd, liv = coda.popleft()
            if liv != liv_prec:
                console.print(f"  L{liv_prec}: {riga}"); riga = ""; liv_prec = liv
            bf = fattore(nd)
            col = "green" if bf == 0 else ("yellow" if abs(bf)==1 else "red")
            riga += f"[bold {col}][{nd.val}(bf={bf})] [/]"
            if nd.sx: coda.append((nd.sx, liv+1))
            if nd.dx: coda.append((nd.dx, liv+1))
        console.print(f"  L{liv_prec}: {riga}")

    valori = [10, 20, 30, 40, 50, 25]
    radice = None
    for v in valori:
        log = []
        radice = inserisci(radice, v, log)
        os.system("cls" if sys.platform == "win32" else "clear")
        console.print(f"\n  [bold]AVL Tree[/]  Inserisco: [bold cyan]{v}[/]")
        for msg in log: console.print(msg)
        console.print(f"\n  Albero per livelli (bf = fattore bilanciamento):")
        stampa_livelli(radice)
        console.print(f"\n  [green]Altezza: {altezza(radice)}  (log₂({len(valori)}) ≈ {len(valori).bit_length()-1})[/]")
        aspetta()

    _pausa_fine_demo()


def demo_segment_tree():
    """Segment Tree: query su intervalli in O(log n)"""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]SEGMENT TREE — Query su Intervalli[/]\n\n"
        "Struttura per rispondere a query su intervalli (somma, min, max) in O(log n).\n"
        "Costruzione: O(n)  Query: O(log n)  Aggiornamento: O(log n).\n"
        "Più flessibile del Fenwick Tree — supporta qualsiasi operazione associativa.\n"
        "Usato in: competitive programming, range queries, interval scheduling.",
        border_style="green"
    ))
    aspetta()

    arr = [1, 3, 5, 7, 9, 11]
    n = len(arr)
    tree = [0] * (4 * n)

    def costruisci(nodo, start, end):
        if start == end:
            tree[nodo] = arr[start]; return
        mid = (start + end) // 2
        costruisci(2*nodo, start, mid)
        costruisci(2*nodo+1, mid+1, end)
        tree[nodo] = tree[2*nodo] + tree[2*nodo+1]

    def query(nodo, start, end, l, r):
        if r < start or end < l: return 0
        if l <= start and end <= r: return tree[nodo]
        mid = (start + end) // 2
        return query(2*nodo, start, mid, l, r) + query(2*nodo+1, mid+1, end, l, r)

    def aggiorna(nodo, start, end, idx, val):
        if start == end:
            arr[idx] = val; tree[nodo] = val; return
        mid = (start + end) // 2
        if idx <= mid: aggiorna(2*nodo, start, mid, idx, val)
        else:          aggiorna(2*nodo+1, mid+1, end, idx, val)
        tree[nodo] = tree[2*nodo] + tree[2*nodo+1]

    costruisci(1, 0, n-1)
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(f"\n  [bold]Segment Tree[/]  Array: {arr}")
    console.print(f"\n  Albero costruito (nodi 1-{4*n}):")
    console.print(f"  [bold green]tree[1] = {tree[1]}[/]  (somma totale)")
    console.print(f"  tree[2]={tree[2]}  tree[3]={tree[3]}  (metà sx + metà dx)")
    aspetta()

    for l, r in [(1, 3), (2, 5), (0, 5)]:
        ris = query(1, 0, n-1, l, r)
        os.system("cls" if sys.platform == "win32" else "clear")
        console.print(f"\n  [bold]Segment Tree[/]  Array: {arr}")
        console.print(f"\n  Query somma({l}, {r}):")
        console.print(f"  Elementi: {arr[l:r+1]}")
        console.print(f"  Somma vera:    {sum(arr[l:r+1])}")
        console.print(f"  Segment Tree:  [bold green]{ris}[/]  — O(log n) invece di O(n)!")
        aspetta()

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(f"\n  [bold]Aggiornamento:[/] arr[3] = 10  (era {arr[3]})")
    aggiorna(1, 0, n-1, 3, 10)
    console.print(f"  Array aggiornato: {arr}")
    ris = query(1, 0, n-1, 1, 4)
    console.print(f"  Query somma(1,4) = [bold green]{ris}[/]")
    aspetta()
    _pausa_fine_demo()


def demo_fenwick_tree():
    """Fenwick Tree (BIT): prefix sum aggiornabile in O(log n)"""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]FENWICK TREE — Binary Indexed Tree (BIT)[/]\n\n"
        "Prefix sum aggiornabili in O(log n) con memoria O(n).\n"
        "Più semplice del Segment Tree: usa il bit meno significativo (LSB) come indice.\n"
        "  update(i, delta): O(log n)\n"
        "  prefix_sum(i):    O(log n)\n"
        "Usato in: inversion count, range sum, competitive programming.",
        border_style="green"
    ))
    aspetta()

    n = 8
    arr  = [3, 2, -1, 6, 5, 4, -3, 3]
    tree = [0] * (n + 1)

    def update(i, delta):
        i += 1
        while i <= n:
            tree[i] += delta; i += i & (-i)

    def prefix_sum(i):
        i += 1; s = 0
        while i > 0:
            s += tree[i]; i -= i & (-i)
        return s

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(f"\n  [bold]Fenwick Tree[/]  Array: {arr}")
    console.print(f"\n  Costruzione (update per ogni elemento):")
    for i, v in enumerate(arr):
        update(i, v)
        console.print(f"  update({i}, {v:3})  → tree = {tree[1:]}")
    aspetta()

    console.print(f"\n  [bold]Query prefix sum:[/]")
    for i in range(n):
        ps   = prefix_sum(i)
        vero = sum(arr[:i+1])
        ok   = "✓" if ps == vero else "✗"
        console.print(f"  prefix_sum({i}) = [bold green]{ps:3}[/]  (somma arr[0..{i}] = {vero})  {ok}")
    aspetta()

    console.print(f"\n  [bold]Range sum:[/]")
    for l, r in [(2, 5), (1, 4), (0, 7)]:
        rs   = prefix_sum(r) - (prefix_sum(l-1) if l > 0 else 0)
        vero = sum(arr[l:r+1])
        console.print(f"  range_sum({l},{r}) = [bold green]{rs:3}[/]  (vero={vero})  {'✓' if rs==vero else '✗'}")
    aspetta()
    _pausa_fine_demo()


def demo_union_find():
    """Union-Find (DSU): componenti connesse in O(α(n)) ≈ O(1)"""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]UNION-FIND — Disjoint Set Union (DSU)[/]\n\n"
        "Gestisce insiemi disgiunti con due operazioni:\n"
        "  find(x):    trova il rappresentante dell'insieme di x\n"
        "  union(x,y): unisce i due insiemi\n"
        "Ottimizzazioni: path compression + union by rank → O(α(n)) ≈ O(1).\n"
        "Usato in: componenti connesse, Kruskal MST, rilevamento cicli.",
        border_style="green"
    ))
    aspetta()

    n = 8
    parent = list(range(n))
    rank   = [0] * n

    def find(x):
        while parent[x] != x:
            parent[x] = parent[parent[x]]; x = parent[x]
        return x

    def union(x, y):
        rx, ry = find(x), find(y)
        if rx == ry: return False
        if rank[rx] < rank[ry]: rx, ry = ry, rx
        parent[ry] = rx
        if rank[rx] == rank[ry]: rank[rx] += 1
        return True

    def stampa_componenti():
        comp = {}
        for i in range(n):
            r = find(i); comp.setdefault(r, []).append(i)
        console.print(f"\n  Componenti:")
        for r, nodi_c in sorted(comp.items()):
            console.print(f"  {{{', '.join(map(str, sorted(nodi_c)))}}}")

    for x, y in [(0,1),(2,3),(0,2),(4,5),(6,7),(4,6),(1,5)]:
        os.system("cls" if sys.platform == "win32" else "clear")
        console.print(f"\n  [bold]Union-Find[/]  union({x}, {y})")
        console.print(f"  parent prima: {parent}")
        merged = union(x, y)
        console.print(f"  parent dopo:  {parent}")
        stampa_componenti()
        if merged:
            console.print(f"\n  [bold green]✓ {x} e {y} ora nello stesso insieme[/]")
        else:
            console.print(f"\n  [dim]  {x} e {y} erano già nello stesso insieme[/]")
        aspetta()

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        f"  Nodi: 0-{n-1}\n"
        f"  parent finale: {parent}\n\n"
        f"  [bold green]✓ Union-Find completato[/]\n"
        f"  find(): O(α(n)) ≈ O(1) con path compression + union by rank",
        title="[bold]Union-Find — COMPLETATO[/]", border_style="green"
    ))
    _pausa_fine_demo()


# ── Geometrici ────────────────────────────────────────────────────────────────

def demo_convex_hull():
    """Convex Hull con Graham Scan — O(n log n)"""
    import math
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]CONVEX HULL — Graham Scan[/]\n\n"
        "Trovare l'involucro convesso minimo che contiene tutti i punti.\n"
        "Immagina: pianta chiodi sul piano, tira un elastico attorno a tutti → hull.\n"
        "Graham Scan: ordina per angolo polare dal punto più in basso, poi stack.\n"
        "Complessità: O(n log n).\n"
        "Usato in: computer graphics, GIS, robotica, collision detection.",
        border_style="magenta"
    ))
    aspetta()

    punti = [(0,0),(4,2),(8,4),(8,0),(0,4),(2,6),(6,2),(-2,2),(2,-2),(1,3)]

    def cross(o, a, b):
        return (a[0]-o[0])*(b[1]-o[1]) - (a[1]-o[1])*(b[0]-o[0])

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(f"\n  [bold]Punti:[/] {punti}")
    aspetta()

    base = min(punti, key=lambda p: (p[1], p[0]))
    console.print(f"\n  Punto base (più in basso): [bold cyan]{base}[/]")
    aspetta()

    def angolo(p):
        return math.atan2(p[1]-base[1], p[0]-base[0])

    ordinati = sorted(punti, key=angolo)
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(f"\n  Punti ordinati per angolo da {base}:")
    for p in ordinati:
        console.print(f"  {p}  angolo={math.degrees(angolo(p)):6.1f}°")
    aspetta()

    hull = []
    for i, p in enumerate(ordinati):
        while len(hull) >= 2 and cross(hull[-2], hull[-1], p) <= 0:
            rimosso = hull.pop()
            os.system("cls" if sys.platform == "win32" else "clear")
            console.print(f"\n  [bold red]Rimuovo {rimosso}[/] (svolta non sinistrorsa)")
            console.print(f"  Hull: {hull}  → aggiungo {p}")
            aspetta()
        hull.append(p)
        os.system("cls" if sys.platform == "win32" else "clear")
        console.print(f"\n  [bold]Graham Scan[/]  passo {i+1}/{len(ordinati)}")
        console.print(f"  Aggiungo: [bold cyan]{p}[/]")
        console.print(f"  Hull: [bold green]{hull}[/]")
        aspetta()

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        f"  Punti totali: {len(punti)}\n\n"
        f"  [bold green]Convex Hull ({len(hull)} vertici):[/]\n" +
        "".join(f"  → {p}\n" for p in hull),
        title="[bold]Convex Hull — COMPLETATO[/]", border_style="magenta"
    ))
    _pausa_fine_demo()


# ── Conway's Game of Life ────────────────────────────────────────────────────

def demo_game_of_life():
    import time
    try:
        from rich.live import Live
    except ImportError:
        console.print("[red]Richiede rich>=12.0[/]")
        input("\n  [INVIO] ")
        return

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]CONWAY'S GAME OF LIFE[/]\n\n"
        "Automa cellulare: ogni cella vive o muore in base ai vicini.\n\n"
        "  • Viva con 2–3 vicini vivi   → sopravvive\n"
        "  • Viva con <2 o >3 vicini    → muore\n"
        "  • Morta con esattamente 3    → nasce\n\n"
        "Premi [bold]Ctrl+C[/] per fermare.",
        border_style="green"
    ))
    time.sleep(2)

    cols, rows = shutil.get_terminal_size(fallback=(80, 24))
    W = min(cols - 6, 70)
    H = min(rows - 8, 22)
    grid = [[random.random() < 0.3 for _ in range(W)] for _ in range(H)]

    def passo(g):
        new_g = [[False] * W for _ in range(H)]
        for r in range(H):
            for c in range(W):
                vivi = sum(
                    g[(r + dr) % H][(c + dc) % W]
                    for dr in (-1, 0, 1)
                    for dc in (-1, 0, 1)
                    if (dr, dc) != (0, 0)
                )
                new_g[r][c] = vivi in (2, 3) if g[r][c] else vivi == 3
        return new_g

    def rendi(g, gen):
        t = Text()
        vive = sum(sum(row) for row in g)
        t.append(f"  Generazione {gen}  |  Celle vive: {vive}  |  Ctrl+C per fermare\n\n", style="dim")
        for row in g:
            t.append("  ")
            for cell in row:
                t.append("█" if cell else " ", style="green" if cell else "dim")
            t.append("\n")
        return Panel(t, title="[bold green]Conway's Game of Life[/]", border_style="green")

    try:
        from rich.live import Live
        with Live(rendi(grid, 0), refresh_per_second=12) as live:
            for gen in range(1, 500):
                time.sleep(0.08)
                grid = passo(grid)
                live.update(rendi(grid, gen))
                if not any(any(row) for row in grid):
                    break
    except KeyboardInterrupt:
        pass
    _pausa_fine_demo()


# ── Labirinto — Generazione + Soluzione BFS ───────────────────────────────────

def demo_maze_bfs():
    import time
    from collections import deque
    try:
        from rich.live import Live
    except ImportError:
        console.print("[red]Richiede rich>=12.0[/]")
        input("\n  [INVIO] ")
        return

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]LABIRINTO — Generazione + Soluzione BFS[/]\n\n"
        "Fase 1: [yellow]Recursive Backtracking[/] scava i corridoi passo per passo\n"
        "Fase 2: [cyan]BFS[/] trova il percorso più breve da [yellow]S[/] a [red]E[/]\n\n"
        "  Verde  = percorso soluzione\n"
        "  Ciano  = frontiera BFS in espansione\n"
        "  Blu    = celle già visitate",
        border_style="magenta"
    ))
    time.sleep(2)

    cols, rows = shutil.get_terminal_size(fallback=(80, 24))
    W = min((cols - 6) // 2, 31)
    H = min(rows - 8, 19)
    W = W if W % 2 == 1 else W - 1
    H = H if H % 2 == 1 else H - 1
    W = max(11, W)
    H = max(9, H)

    maze = [[0] * W for _ in range(H)]
    COLORI = {
        0: ("█", "dim white"),
        1: (" ", "white"),
        2: ("·", "blue"),
        3: ("·", "bold green"),
        4: ("S", "bold yellow"),
        5: ("E", "bold red"),
    }

    def rendi_maze(titolo, fase=""):
        t = Text()
        t.append(f"  {fase}\n\n", style="dim")
        for row in maze:
            t.append("  ")
            for cell in row:
                ch, st = COLORI.get(cell, ("?", "white"))
                t.append(ch, style=st)
            t.append("\n")
        return Panel(t, title=f"[bold magenta]{titolo}[/]", border_style="magenta")

    # Fase 1: Generazione Recursive Backtracking iterativo
    maze[1][1] = 1
    stack = [(1, 1)]
    passi_gen = 0
    try:
        with Live(rendi_maze("Generazione Labirinto"), refresh_per_second=25) as live:
            while stack:
                r, c = stack[-1]
                vicini = []
                for dr, dc in [(-2, 0), (2, 0), (0, -2), (0, 2)]:
                    nr, nc = r + dr, c + dc
                    if 0 < nr < H - 1 and 0 < nc < W - 1 and maze[nr][nc] == 0:
                        vicini.append((nr, nc, r + dr // 2, c + dc // 2))
                if vicini:
                    nr, nc, mr, mc = random.choice(vicini)
                    maze[mr][mc] = 1
                    maze[nr][nc] = 1
                    stack.append((nr, nc))
                    passi_gen += 1
                    if passi_gen % 2 == 0:
                        live.update(rendi_maze("Generazione...", f"Scavando corridoi: {passi_gen} passi"))
                        time.sleep(0.015)
                else:
                    stack.pop()
            live.update(rendi_maze("Labirinto pronto!", "Avvio BFS tra 1 secondo..."))
            time.sleep(1)
    except KeyboardInterrupt:
        _pausa_fine_demo()
        return

    maze[1][1] = 4
    maze[H - 2][W - 2] = 5

    # Fase 2: BFS
    start, end = (1, 1), (H - 2, W - 2)
    coda = deque([(start, [start])])
    visitati = {start}
    passi_bfs = 0
    try:
        with Live(rendi_maze("BFS — ricerca percorso..."), refresh_per_second=20) as live:
            trovato = False
            while coda and not trovato:
                (r, c), percorso = coda.popleft()
                for dr, dc in [(-1, 0), (1, 0), (0, -1), (0, 1)]:
                    nr, nc = r + dr, c + dc
                    if 0 <= nr < H and 0 <= nc < W and (nr, nc) not in visitati:
                        if maze[nr][nc] in (1, 5):
                            visitati.add((nr, nc))
                            nuovo = percorso + [(nr, nc)]
                            passi_bfs += 1
                            if maze[nr][nc] == 5:
                                for pr, pc in nuovo[1:-1]:
                                    maze[pr][pc] = 3
                                maze[1][1] = 4
                                maze[H - 2][W - 2] = 5
                                live.update(rendi_maze(
                                    "Percorso trovato!",
                                    f"BFS: {passi_bfs} passi | Soluzione: {len(nuovo)} celle"
                                ))
                                trovato = True
                                break
                            coda.append(((nr, nc), nuovo))
                            maze[nr][nc] = 2
                            if passi_bfs % 3 == 0:
                                live.update(rendi_maze("BFS in corso...", f"Celle visitate: {len(visitati)}"))
                                time.sleep(0.03)
    except KeyboardInterrupt:
        pass
    _pausa_fine_demo()


# ── Sorting Race — 3 algoritmi in gara ────────────────────────────────────────

def demo_sorting_race():
    import time
    try:
        from rich.live import Live
    except ImportError:
        console.print("[red]Richiede rich>=12.0[/]")
        input("\n  [INVIO] ")
        return

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]SORTING RACE — 3 Algoritmi in Gara[/]\n\n"
        "Stessa lista, stessi dati — algoritmi diversi a confronto.\n\n"
        "  [yellow]Bubble Sort[/]  — confronta vicini, semplice\n"
        "  [cyan]Merge Sort[/]   — divide e fondi, O(n log n) garantito\n"
        "  [green]Quick Sort[/]   — pivot, il più veloce in media\n\n"
        "I frame avanzano automaticamente.",
        border_style="yellow"
    ))
    time.sleep(2)

    cols, _ = shutil.get_terminal_size(fallback=(80, 24))
    n = 14
    originale = random.sample(range(5, 95), n)
    larg = max(10, min(28, (cols - 30) // 3))

    stato = {
        "bubble": {"arr": originale.copy(), "done": False, "passi": 0,
                   "color": "yellow", "i": 0, "j": 0, "ordinati": set()},
        "merge":  {"arr": originale.copy(), "done": False, "passi": 0,
                   "color": "cyan",   "amp": 1, "ordinati": set()},
        "quick":  {"arr": originale.copy(), "done": False, "passi": 0,
                   "color": "green",  "stack": [(0, n - 1)], "ordinati": set()},
    }

    def passo_bubble(st):
        arr, i, j = st["arr"], st["i"], st["j"]
        if i >= n - 1:
            st["done"] = True; st["ordinati"] = set(range(n)); return
        if j < n - 1 - i:
            if arr[j] > arr[j + 1]:
                arr[j], arr[j + 1] = arr[j + 1], arr[j]
            st["j"] = j + 1
        else:
            st["ordinati"].add(n - 1 - i); st["i"] = i + 1; st["j"] = 0
        st["passi"] += 1

    def passo_merge(st):
        arr, amp = st["arr"], st["amp"]
        if amp >= n:
            st["done"] = True; st["ordinati"] = set(range(n)); return
        for start in range(0, n, amp * 2):
            mid, fine = min(start + amp, n), min(start + amp * 2, n)
            left, right = arr[start:mid], arr[mid:fine]
            i = j = 0; k = start
            while i < len(left) and j < len(right):
                if left[i] <= right[j]: arr[k] = left[i]; i += 1
                else:                   arr[k] = right[j]; j += 1
                k += 1
            while i < len(left):  arr[k] = left[i];  i += 1; k += 1
            while j < len(right): arr[k] = right[j]; j += 1; k += 1
            for idx in range(start, fine): st["ordinati"].add(idx)
        st["amp"] *= 2; st["passi"] += 1

    def passo_quick(st):
        arr = st["arr"]
        if not st["stack"]:
            st["done"] = True; st["ordinati"] = set(range(n)); return
        basso, alto = st["stack"].pop()
        if basso >= alto: return
        pivot = arr[alto]; i = basso - 1
        for j in range(basso, alto):
            if arr[j] <= pivot: i += 1; arr[i], arr[j] = arr[j], arr[i]
        arr[i + 1], arr[alto] = arr[alto], arr[i + 1]
        pos = i + 1; st["ordinati"].add(pos)
        st["stack"].append((basso, pos - 1)); st["stack"].append((pos + 1, alto))
        st["passi"] += 1

    def rendi_gara():
        max_v = max(max(s["arr"]) for s in stato.values()) or 1
        tab = Table.grid(expand=True)
        tab.add_column(ratio=1); tab.add_column(ratio=1); tab.add_column(ratio=1)
        nomi = [
            (f"[yellow]Bubble Sort[/] [{stato['bubble']['passi']} passi]"
             + (" [bold green]✓[/]" if stato["bubble"]["done"] else "")),
            (f"[cyan]Merge Sort[/] [{stato['merge']['passi']} passi]"
             + (" [bold green]✓[/]" if stato["merge"]["done"] else "")),
            (f"[green]Quick Sort[/] [{stato['quick']['passi']} passi]"
             + (" [bold green]✓[/]" if stato["quick"]["done"] else "")),
        ]
        tab.add_row(*nomi)
        tab.add_row("", "", "")
        for idx in range(n):
            cols_riga = []
            for nome, colore in [("bubble","yellow"),("merge","cyan"),("quick","green")]:
                val = stato[nome]["arr"][idx]
                bl = max(1, int(val / max_v * larg))
                st = "green" if idx in stato[nome]["ordinati"] else colore
                t = Text(("█" * bl).ljust(larg + 1) + f"{val:3}", style=st)
                cols_riga.append(t)
            tab.add_row(*cols_riga)
        return Panel(tab, title="[bold]Sorting Race[/]", border_style="yellow", padding=(1, 2))

    try:
        with Live(rendi_gara(), refresh_per_second=8) as live:
            while not all(s["done"] for s in stato.values()):
                if not stato["bubble"]["done"]: passo_bubble(stato["bubble"])
                if not stato["merge"]["done"]:  passo_merge(stato["merge"])
                if not stato["quick"]["done"]:  passo_quick(stato["quick"])
                live.update(rendi_gara())
                time.sleep(0.12)
            time.sleep(2)
    except KeyboardInterrupt:
        pass
    _pausa_fine_demo()


# ── N-Regine Live — backtracking visivo su scacchiera ────────────────────────

def demo_nqueens_live():
    import time
    try:
        from rich.live import Live
    except ImportError:
        console.print("[red]Richiede rich>=12.0[/]")
        input("\n  [INVIO] ")
        return

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]N-REGINE — Backtracking Animato[/]\n\n"
        "Piazza N regine su una scacchiera N×N senza che si attacchino.\n\n"
        "  [cyan]Q[/]       = regina posizionata\n"
        "  [yellow]?[/]       = posizione che stiamo testando\n"
        "  [red]×[/]       = cella attaccata (non valida)\n"
        "  [dim]░[/]       = cella libera\n\n"
        "Premi Ctrl+C per fermare.",
        border_style="cyan"
    ))
    time.sleep(2)

    cols, rows = shutil.get_terminal_size(fallback=(80, 24))
    N = min(8, max(4, (rows - 10) // 2))

    board = [-1] * N   # board[col] = riga regina, -1 = vuota

    def is_safe(col, row):
        for c in range(col):
            r = board[c]
            if r == row or abs(r - row) == abs(c - col):
                return False
        return True

    # Pre-calcola i frame (board_state, col_cursor, row_cursor, n_sol)
    frames = []
    soluzioni = [0]

    def solve(col):
        if col == N:
            soluzioni[0] += 1
            frames.append((board[:], -1, -1, soluzioni[0]))
            return
        for row in range(N):
            frames.append((board[:], col, row, soluzioni[0]))
            if is_safe(col, row):
                board[col] = row
                frames.append((board[:], col, row, soluzioni[0]))
                solve(col + 1)
                board[col] = -1

    solve(0)

    def render(b, col_cur, row_cur, n_sol):
        t = Text()
        t.append(f"  N={N}  |  Soluzioni trovate: {n_sol}  |  Frame: {len(frames)}\n\n", style="dim")
        t.append("     ")
        for c in range(N):
            t.append(f" {c} ", style="dim")
        t.append("\n")
        for r in range(N):
            t.append(f"  {r}  ", style="dim")
            for c in range(N):
                is_queen   = b[c] == r
                is_cursor  = c == col_cur and r == row_cur
                is_dark    = (r + c) % 2 == 1
                is_attacked = False
                if not is_queen and not is_cursor:
                    for qc in range(N):
                        qr = b[qc]
                        if qr >= 0 and (qr == r or abs(qr - r) == abs(qc - c)):
                            is_attacked = True
                            break
                if is_cursor:
                    t.append(" ? ", style="bold yellow")
                elif is_queen:
                    t.append(" Q ", style="bold cyan")
                elif is_attacked:
                    t.append(" × ", style="red")
                elif is_dark:
                    t.append(" ░ ", style="dim")
                else:
                    t.append("   ")
            t.append("\n")
        return Panel(t, title="[bold cyan]N-Regine — Backtracking[/]", border_style="cyan")

    try:
        with Live(render([-1]*N, -1, -1, 0), refresh_per_second=15) as live:
            for b, col, row, n_sol in frames:
                live.update(render(b, col, row, n_sol))
                time.sleep(0.05)
            time.sleep(2)
    except KeyboardInterrupt:
        pass
    _pausa_fine_demo()


# ── Dijkstra su Griglia con Pesi ─────────────────────────────────────────────

def demo_dijkstra_griglia():
    import time
    import heapq
    try:
        from rich.live import Live
    except ImportError:
        console.print("[red]Richiede rich>=12.0[/]")
        input("\n  [INVIO] ")
        return

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]DIJKSTRA SU GRIGLIA — Percorso Minimo[/]\n\n"
        "Ogni cella ha un costo (1–9). Dijkstra trova il percorso\n"
        "con costo totale minimo da [yellow]S[/] a [red]E[/].\n\n"
        "  [cyan]numero[/]  = cella visitata (distanza accumulata)\n"
        "  [white]numero[/]  = cella non ancora raggiunta\n"
        "  [bold green]*[/]       = percorso ottimale trovato\n"
        "  [dim]███[/]     = muro (non attraversabile)",
        border_style="blue"
    ))
    time.sleep(2)

    cols, rows = shutil.get_terminal_size(fallback=(80, 24))
    W = min((cols - 6) // 3, 22)
    H = min(rows - 8, 16)

    grid = [[random.randint(1, 9) for _ in range(W)] for _ in range(H)]
    for _ in range(W * H // 6):
        r, c = random.randint(1, H - 2), random.randint(1, W - 2)
        grid[r][c] = 0

    sr, sc, er, ec = 0, 0, H - 1, W - 1
    grid[sr][sc] = 1
    grid[er][ec] = 1

    INF = float("inf")
    dist   = [[INF] * W for _ in range(H)]
    prev   = [[None] * W for _ in range(H)]
    visited = set()
    dist[sr][sc] = 0

    def render_dijk(step, current=None, path=None):
        t = Text()
        t.append(f"  Passo {step} | Visitati: {len(visited)}", style="dim")
        if path:
            t.append(f" | Costo percorso: {int(dist[er][ec])}", style="bold green")
        t.append("\n\n")
        for r in range(H):
            t.append("  ")
            for c in range(W):
                pos = (r, c)
                if pos == (sr, sc):
                    t.append(" S ", style="bold yellow")
                elif pos == (er, ec):
                    t.append(" E ", style="bold red")
                elif path and pos in path:
                    t.append(" * ", style="bold green")
                elif grid[r][c] == 0:
                    t.append("███", style="dim")
                elif pos == current:
                    t.append(f" {grid[r][c]} ", style="bold cyan")
                elif pos in visited:
                    d = int(dist[r][c])
                    t.append(f"{d:2} ", style="cyan" if d < 30 else "blue")
                else:
                    t.append(f" {grid[r][c]} ", style="dim")
            t.append("\n")
        return Panel(t, title="[bold blue]Dijkstra su Griglia[/]", border_style="blue")

    heap = [(0, sr, sc)]
    step = 0
    path_cells = set()

    try:
        with Live(render_dijk(0), refresh_per_second=15) as live:
            while heap:
                cost, r, c = heapq.heappop(heap)
                if (r, c) in visited:
                    continue
                visited.add((r, c))
                step += 1
                if (r, c) == (er, ec):
                    cur = (er, ec)
                    while cur:
                        path_cells.add(cur)
                        cur = prev[cur[0]][cur[1]]
                    live.update(render_dijk(step, None, path_cells))
                    time.sleep(3)
                    break
                for dr, dc in [(-1, 0), (1, 0), (0, -1), (0, 1)]:
                    nr, nc = r + dr, c + dc
                    if 0 <= nr < H and 0 <= nc < W and (nr, nc) not in visited and grid[nr][nc] > 0:
                        new_cost = cost + grid[nr][nc]
                        if new_cost < dist[nr][nc]:
                            dist[nr][nc] = new_cost
                            prev[nr][nc] = (r, c)
                            heapq.heappush(heap, (new_cost, nr, nc))
                if step % 2 == 0:
                    live.update(render_dijk(step, (r, c)))
                    time.sleep(0.04)
    except KeyboardInterrupt:
        pass
    _pausa_fine_demo()


# ── Rule 30 — Automa Cellulare 1D di Wolfram ─────────────────────────────────

def demo_rule30():
    import time
    try:
        from rich.live import Live
    except ImportError:
        console.print("[red]Richiede rich>=12.0[/]")
        input("\n  [INVIO] ")
        return

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]RULE 30 — Automa Cellulare 1D (Wolfram)[/]\n\n"
        "Ogni riga nasce dalla precedente con una regola semplicissima:\n"
        "guarda 3 celle (sinistra, centro, destra) → applica la Regola 30.\n\n"
        "Nonostante la regola sia semplice, il pattern è [bold]caotico[/]\n"
        "e imprevedibile — usato da Mathematica come generatore casuale.\n\n"
        "  [yellow]█[/] = cella viva   [dim]spazio[/] = cella morta",
        border_style="yellow"
    ))
    time.sleep(2)

    cols, rows = shutil.get_terminal_size(fallback=(80, 24))
    W = min(cols - 6, 78)
    MAX_ROWS = min(rows - 6, 35)
    RULE = 30

    def applica(row):
        n = len(row)
        new_row = [0] * n
        for i in range(n):
            pattern = (row[(i - 1) % n] << 2) | (row[i] << 1) | row[(i + 1) % n]
            new_row[i] = (RULE >> pattern) & 1
        return new_row

    current = [0] * W
    current[W // 2] = 1
    rows_data = [current[:]]
    for _ in range(MAX_ROWS - 1):
        current = applica(current)
        rows_data.append(current[:])

    def render(n_shown):
        t = Text()
        t.append(f"  Rule {RULE} | {n_shown}/{MAX_ROWS} generazioni | Ctrl+C per fermare\n\n", style="dim")
        for row in rows_data[:n_shown]:
            t.append("  ")
            for cell in row:
                t.append("█" if cell else " ", style="yellow" if cell else "")
            t.append("\n")
        return Panel(t, title=f"[bold yellow]Rule {RULE} — Wolfram[/]", border_style="yellow")

    try:
        with Live(render(1), refresh_per_second=20) as live:
            for i in range(2, MAX_ROWS + 1):
                live.update(render(i))
                time.sleep(0.06)
            time.sleep(3)
    except KeyboardInterrupt:
        pass
    _pausa_fine_demo()


# ── Fuoco nella Foresta ───────────────────────────────────────────────────────

def demo_forest_fire():
    import time
    try:
        from rich.live import Live
    except ImportError:
        console.print("[red]Richiede rich>=12.0[/]")
        input("\n  [INVIO] ")
        return

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]FUOCO NELLA FORESTA — Simulazione[/]\n\n"
        "Automa cellulare: il fuoco si propaga agli alberi vicini.\n\n"
        "  [green]♣[/]  = albero sano\n"
        "  [bold red]▲[/]  = in fiamme\n"
        "  [dim]·[/]  = cenere (già bruciato)\n\n"
        "Il fuoco parte dal bordo sinistro. Ctrl+C per fermare.",
        border_style="red"
    ))
    time.sleep(2)

    cols, rows = shutil.get_terminal_size(fallback=(80, 24))
    W = min((cols - 6) // 2, 50)
    H = min(rows - 8, 22)

    EMPTY, TREE, BURNING, ASH = 0, 1, 2, 3
    CHARS = {EMPTY: (" ", ""), TREE: ("♣", "green"),
             BURNING: ("▲", "bold red"), ASH: ("·", "dim")}

    grid = [[TREE if random.random() < 0.72 else EMPTY for _ in range(W)] for _ in range(H)]
    for r in range(H):
        if grid[r][0] == TREE:
            grid[r][0] = BURNING

    def step(g):
        ng = [row[:] for row in g]
        for r in range(H):
            for c in range(W):
                if g[r][c] == BURNING:
                    ng[r][c] = ASH
                    for dr, dc in [(-1, 0), (1, 0), (0, -1), (0, 1)]:
                        nr, nc = r + dr, c + dc
                        if 0 <= nr < H and 0 <= nc < W and g[nr][nc] == TREE:
                            if random.random() < 0.82:
                                ng[nr][nc] = BURNING
        return ng

    def render(g, n):
        alberi   = sum(cell == TREE    for row in g for cell in row)
        fuoco    = sum(cell == BURNING for row in g for cell in row)
        t = Text()
        t.append(f"  Passo {n} | Alberi: {alberi} | Fuoco: {fuoco} | Ctrl+C\n\n", style="dim")
        for row in g:
            t.append("  ")
            for cell in row:
                ch, st = CHARS[cell]
                t.append(ch + " ", style=st)
            t.append("\n")
        return Panel(t, title="[bold red]Fuoco nella Foresta[/]", border_style="red")

    try:
        with Live(render(grid, 0), refresh_per_second=10) as live:
            for i in range(1, 300):
                time.sleep(0.1)
                grid = step(grid)
                live.update(render(grid, i))
                if not any(cell == BURNING for row in grid for cell in row):
                    time.sleep(2)
                    break
    except KeyboardInterrupt:
        pass
    _pausa_fine_demo()


# ── Pioggia Matrix ────────────────────────────────────────────────────────────

def demo_matrix_rain():
    import time
    try:
        from rich.live import Live
    except ImportError:
        console.print("[red]Richiede rich>=12.0[/]")
        input("\n  [INVIO] ")
        return

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]MATRIX RAIN[/]\n\n"
        "Cascate di caratteri in stile Matrix.\n"
        "Ogni colonna cade a velocità diversa.\n\n"
        "  [bold white]bianco[/]  = testa del flusso (più luminosa)\n"
        "  [bold green]verde[/]   = corpo del flusso\n"
        "  [dim]verde tenue[/] = scia in dissolvenza\n\n"
        "Premi Ctrl+C per fermare.",
        border_style="green"
    ))
    time.sleep(2)

    cols, rows = shutil.get_terminal_size(fallback=(80, 24))
    W = min((cols - 4) // 2, 46)
    H = min(rows - 4, 26)
    CHARS = "アイウエオカキクケコサシスセソタチツテトナニヌネノ0123456789ABCDEF@#$%"

    streams = [
        {
            "head":   random.randint(-H, 0),
            "speed":  random.uniform(0.4, 1.2),
            "length": random.randint(4, H // 2),
            "timer":  random.random(),
        }
        for _ in range(W)
    ]

    grid       = [[" "] * W for _ in range(H)]
    brightness = [[0.0]    * W for _ in range(H)]

    def render():
        t = Text()
        t.append("  Ctrl+C per fermare\n\n", style="dim")
        for r in range(H):
            t.append("  ")
            for c in range(W):
                ch = grid[r][c]
                b  = brightness[r][c]
                if   b > 0.92: t.append(ch + " ", style="bold white")
                elif b > 0.65: t.append(ch + " ", style="bold green")
                elif b > 0.35: t.append(ch + " ", style="green")
                elif b > 0.12: t.append(ch + " ", style="dim green")
                else:          t.append("  ")
            t.append("\n")
        return Panel(t, title="[bold green]Matrix Rain[/]", border_style="green")

    try:
        with Live(render(), refresh_per_second=15) as live:
            while True:
                # Dissolvenza
                for r in range(H):
                    for c in range(W):
                        brightness[r][c] = max(0.0, brightness[r][c] - 0.14)
                # Avanza ogni flusso
                for c, st in enumerate(streams):
                    st["timer"] += st["speed"]
                    if st["timer"] >= 1.0:
                        st["timer"] -= 1.0
                        st["head"] += 1
                        h = st["head"]
                        if 0 <= h < H:
                            grid[h][c] = random.choice(CHARS)
                            brightness[h][c] = 1.0
                            for off in range(1, st["length"]):
                                tr = h - off
                                if 0 <= tr < H:
                                    brightness[tr][c] = max(
                                        brightness[tr][c],
                                        1.0 - off / st["length"]
                                    )
                        if st["head"] > H + st["length"]:
                            st["head"]  = random.randint(-H, -H // 3)
                            st["speed"] = random.uniform(0.4, 1.2)
                            st["length"]= random.randint(4, H // 2)
                live.update(render())
                time.sleep(0.067)
    except KeyboardInterrupt:
        pass
    _pausa_fine_demo()


# ── Tim Sort ──────────────────────────────────────────────────────────────────

def tim_sort(arr):
    """
    TIM SORT
    Algoritmo ibrido usato da Python (list.sort()) e Java.
    Fase 1: Insertion Sort su piccole 'run'.
    Fase 2: Merge Sort per fondere le run.
    O(n log n) — stabile — ottimo su dati parzialmente ordinati.
    """
    arr = arr.copy()
    n = len(arr)
    RUN = 4  # semplificato per visualizzazione (reale: 32–64)

    console.print(Panel(
        "[bold]TIM SORT[/]\n\n"
        "Algoritmo ibrido usato da [bold cyan]Python list.sort()[/] e Java!\n"
        "Fase 1: Insertion Sort su piccole run di dimensione RUN.\n"
        "Fase 2: Merge Sort per fondere le run in blocchi crescenti.\n"
        f"O(n log n) — stabile — ottimo su dati quasi-ordinati.\n"
        f"RUN size = {RUN} (in produzione: 32-64).",
        border_style="yellow"
    ))
    aspetta()

    # Fase 1: Insertion Sort su ogni run
    console.print(f"\n  [yellow]Fase 1: Insertion Sort su ogni blocco da {RUN}[/]")
    aspetta()
    for start in range(0, n, RUN):
        fine = min(start + RUN, n)
        for i in range(start + 1, fine):
            chiave = arr[i]
            j = i - 1
            mostra_array(arr, confronto=(i,), ordinati=set(range(start, i)),
                         messaggio=f"Blocco [{start}:{fine}]: Insertion Sort — prendo arr[{i}]={chiave}",
                         titolo="Tim Sort — Fase 1: Insertion Sort")
            aspetta()
            while j >= start and arr[j] > chiave:
                arr[j + 1] = arr[j]
                j -= 1
            arr[j + 1] = chiave
        mostra_array(arr, ordinati=set(range(start, fine)),
                     messaggio=f"Blocco [{start}:{fine}] ordinato: {arr[start:fine]}",
                     titolo="Tim Sort — Blocco pronto")
        aspetta()

    # Fase 2: Merge Sort delle run
    console.print("\n  [yellow]Fase 2: Merge Sort delle run[/]")
    aspetta()
    ampiezza = RUN
    passaggio = 0
    while ampiezza < n:
        passaggio += 1
        nuovo = arr.copy()
        for start in range(0, n, ampiezza * 2):
            mid  = min(start + ampiezza, n)
            fine = min(start + ampiezza * 2, n)
            left  = arr[start:mid]
            right = arr[mid:fine]
            i = j = 0; k = start
            while i < len(left) and j < len(right):
                if left[i] <= right[j]: nuovo[k] = left[i]; i += 1
                else:                   nuovo[k] = right[j]; j += 1
                k += 1
            while i < len(left):  nuovo[k] = left[i];  i += 1; k += 1
            while j < len(right): nuovo[k] = right[j]; j += 1; k += 1
            mostra_array(nuovo, ordinati=set(range(start, fine)),
                         messaggio=f"Fondi run [{start}:{mid}] + [{mid}:{fine}]",
                         titolo=f"Tim Sort — Fase 2 pass. {passaggio}")
            aspetta()
        arr = nuovo
        ampiezza *= 2

    mostra_array(arr, ordinati=set(range(n)),
                 messaggio="✓ Lista ordinata con Tim Sort! (come Python list.sort())",
                 titolo="Tim Sort — COMPLETATO")
    _pausa_fine_demo()
    return arr


# ── Pancake Sort ──────────────────────────────────────────────────────────────

def pancake_sort(arr):
    """
    PANCAKE SORT
    Ordina solo con 'flip': inverti i primi k elementi.
    Come capovolgere una pila di frittelle con una spatola!
    Strategia: trova il massimo, flip in cima, flip in posizione finale.
    O(n²) flip — educativo, mai usato in produzione.
    """
    arr = arr.copy()
    n = len(arr)
    ordinati = set()

    console.print(Panel(
        "[bold]PANCAKE SORT[/]\n\n"
        "Ordina solo con 'flip': inverti i primi K elementi.\n"
        "Come girare una pila di frittelle con una spatola!\n"
        "Strategia per ogni posizione finale (da n a 2):\n"
        "  1. Trova il massimo nella parte non ordinata.\n"
        "  2. flip() per portarlo in cima.\n"
        "  3. flip() per metterlo nella posizione finale.\n"
        "Complessità: O(n²) flip — mai usato in produzione, ma educativo.",
        border_style="yellow"
    ))
    aspetta()

    flip_count = 0

    def flip(k):
        nonlocal flip_count
        arr[:k + 1] = arr[:k + 1][::-1]
        flip_count += 1

    for dimensione in range(n, 1, -1):
        max_idx = arr[:dimensione].index(max(arr[:dimensione]))

        if max_idx == dimensione - 1:
            ordinati.add(dimensione - 1)
            mostra_array(arr, ordinati=ordinati,
                         messaggio=f"arr[{dimensione-1}]={arr[dimensione-1]} già in posizione corretta",
                         titolo="Pancake Sort")
            aspetta()
            continue

        if max_idx != 0:
            mostra_array(arr, confronto=tuple(range(max_idx + 1)), ordinati=ordinati,
                         messaggio=f"Massimo={arr[max_idx]} a pos {max_idx} → flip({max_idx}): lo porto in cima",
                         titolo="Pancake Sort — Flip 1")
            aspetta()
            flip(max_idx)
            mostra_array(arr, scambio=tuple(range(max_idx + 1)), ordinati=ordinati,
                         messaggio=f"Dopo flip({max_idx}): massimo {arr[0]} ora in cima!",
                         titolo="Pancake Sort — Flip 1 fatto")
            aspetta()

        mostra_array(arr, confronto=tuple(range(dimensione)), ordinati=ordinati,
                     messaggio=f"flip({dimensione-1}): porto {arr[0]} nella posizione {dimensione-1}",
                     titolo="Pancake Sort — Flip 2")
        aspetta()
        flip(dimensione - 1)
        ordinati.add(dimensione - 1)
        mostra_array(arr, ordinati=ordinati,
                     messaggio=f"✓ {arr[dimensione-1]} in posizione corretta! (flip totali: {flip_count})",
                     titolo="Pancake Sort — Flip 2 fatto")
        aspetta()

    ordinati.add(0)
    mostra_array(arr, ordinati=set(range(n)),
                 messaggio=f"✓ Lista ordinata in {flip_count} flip!",
                 titolo="Pancake Sort — COMPLETATO")
    _pausa_fine_demo()
    return arr


# ── Quickselect ───────────────────────────────────────────────────────────────

def quickselect(arr):
    """
    QUICKSELECT
    Trova il k-esimo elemento più piccolo in O(n) medio.
    Come Quick Sort ma ricorre SOLO nella metà che contiene il k-esimo.
    Usato per mediana, percentili, top-K elementi.
    """
    arr = arr.copy()
    n = len(arr)

    console.print(Panel(
        "[bold]QUICKSELECT — k-esimo elemento minimo[/]\n\n"
        "Come Quick Sort ma molto più veloce per trovare un solo elemento.\n"
        "Dopo la partizione: ricorre SOLO nella metà che contiene il k-esimo.\n"
        "Complessità: O(n) medio — O(n²) caso peggiore.\n"
        "Usato in: mediana in O(n), percentili, Top-K senza ordinamento completo.",
        border_style="cyan"
    ))
    aspetta()

    console.print(f"  Array: {arr}")
    try:
        k = int(input(f"  Trova il k-esimo minimo (1–{n}, invio={n//2+1}): ") or str(n//2+1))
        k = max(1, min(k, n))
    except ValueError:
        k = n // 2 + 1
    k_zero = k - 1
    console.print(f"  Cerco il {k}° elemento più piccolo (indice 0-based: {k_zero})")
    aspetta()

    def particiona(lo, hi):
        pivot = arr[hi]
        i = lo - 1
        mostra_array(arr, confronto=(hi,),
                     messaggio=f"Pivot: arr[{hi}]={pivot}  range=[{lo}:{hi}]",
                     titolo="Quickselect — Partizione")
        aspetta()
        for j in range(lo, hi):
            if arr[j] <= pivot:
                i += 1
                if i != j:
                    arr[i], arr[j] = arr[j], arr[i]
                    mostra_array(arr, scambio=(i, j),
                                 messaggio=f"arr[{j}]={arr[j]} ≤ {pivot} → scambio con pos {i}",
                                 titolo="Quickselect — Partizione")
                    aspetta()
        arr[i + 1], arr[hi] = arr[hi], arr[i + 1]
        return i + 1

    lo, hi = 0, n - 1
    while lo <= hi:
        p = particiona(lo, hi)
        mostra_array(arr, ordinati={p},
                     messaggio=f"Pivot {arr[p]} in pos {p}  (cerco indice {k_zero})",
                     titolo="Quickselect")
        aspetta()
        if p == k_zero:
            mostra_array(arr, trovato=p,
                         messaggio=f"✓ Il {k}° elemento più piccolo = {arr[p]}",
                         titolo="Quickselect — TROVATO!")
            _pausa_fine_demo()
            return arr[p]
        elif p < k_zero:
            lo = p + 1
            console.print(f"  → pos {p} < {k_zero}: cerco a DESTRA [{lo}:{hi}]")
        else:
            hi = p - 1
            console.print(f"  → pos {p} > {k_zero}: cerco a SINISTRA [{lo}:{hi}]")
        aspetta()

    _pausa_fine_demo()


# ── Comb Sort ─────────────────────────────────────────────────────────────────

def comb_sort(arr):
    """
    COMB SORT
    Bubble Sort con gap decrescente (shrink factor 1.3).
    Elimina le 'tartarughe' (elementi grandi vicino alla fine).
    O(n²) peggiore — O(n log n) medio.
    """
    arr = arr.copy()
    n = len(arr)
    ordinati = set()

    console.print(Panel(
        "[bold]COMB SORT[/]\n\n"
        "Miglioramento del Bubble Sort: confronta elementi con gap decrescente.\n"
        "Gap iniziale = n, ridotto × (1/1.3) ad ogni passata.\n"
        "Quando gap=1 → come Bubble Sort normale.\n"
        "Elimina le 'tartarughe' (piccoli valori in fondo lenti a risalire).\n"
        "Complessità: O(n log n) medio — O(n²) peggiore.",
        border_style="yellow"
    ))
    aspetta()

    gap = n
    shrink = 1.3
    ordinato = False
    passata = 0

    while not ordinato:
        gap = int(gap / shrink)
        if gap <= 1:
            gap = 1
            ordinato = True
        passata += 1
        console.print(f"\n  [yellow]Passata {passata}: gap = {gap}[/]")
        aspetta()

        for i in range(n - gap):
            mostra_array(arr, confronto=(i, i + gap), ordinati=ordinati,
                         messaggio=f"Confronto arr[{i}]={arr[i]} e arr[{i+gap}]={arr[i+gap]} (gap={gap})",
                         titolo=f"Comb Sort — gap={gap}")
            aspetta()
            if arr[i] > arr[i + gap]:
                arr[i], arr[i + gap] = arr[i + gap], arr[i]
                ordinato = False
                mostra_array(arr, scambio=(i, i + gap), ordinati=ordinati,
                             messaggio="SCAMBIO!",
                             titolo=f"Comb Sort — gap={gap}")
                aspetta()

    mostra_array(arr, ordinati=set(range(n)),
                 messaggio=f"✓ Lista ordinata in {passata} passate!",
                 titolo="Comb Sort — COMPLETATO")
    _pausa_fine_demo()
    return arr


# ── Trie ──────────────────────────────────────────────────────────────────────

def demo_trie():
    """Trie (Albero Prefisso): struttura per ricerca e autocomplete su stringhe."""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]TRIE — Albero Prefisso[/]\n\n"
        "Struttura ad albero che condivide i prefissi comuni tra parole.\n"
        "  Inserimento:   O(m)  dove m = lunghezza parola\n"
        "  Ricerca:       O(m)\n"
        "  Autocomplete:  O(m + k)  dove k = numero risultati\n\n"
        "Usato in: autocomplete, correttori ortografici, dizionari, IP routing.\n"
        "Vantaggio: tutte le parole con prefisso 'ca' in O(2) passi!",
        border_style="green"
    ))
    aspetta()

    class NodoTrie:
        def __init__(self):
            self.figli = {}
            self.fine_parola = False

    radice = NodoTrie()

    def _stampa_trie(nodo, prefix=""):
        figli_list = list(sorted(nodo.figli.items()))
        for idx, (ch, figlio) in enumerate(figli_list):
            is_last = idx == len(figli_list) - 1
            conn = "└── " if is_last else "├── "
            marker = " [bold green]✓[/]" if figlio.fine_parola else ""
            console.print(f"  {prefix}{conn}[bold]{ch}[/]{marker}")
            ext = "    " if is_last else "│   "
            _stampa_trie(figlio, prefix + ext)

    def inserisci(parola):
        curr = radice
        percorso = ""
        for ch in parola:
            if ch not in curr.figli:
                curr.figli[ch] = NodoTrie()
            curr = curr.figli[ch]
            percorso += ch
            os.system("cls" if sys.platform == "win32" else "clear")
            console.print(f"\n  [bold]Trie — Inserimento:[/] [cyan]{parola}[/]")
            console.print(f"  Carattere: [bold yellow]{ch}[/]  Percorso: [bold]{percorso}[/]\n")
            _stampa_trie(radice)
            aspetta()
        curr.fine_parola = True
        os.system("cls" if sys.platform == "win32" else "clear")
        console.print(f"\n  [bold green]✓ '{parola}' inserita![/]\n")
        _stampa_trie(radice)
        aspetta()

    def cerca(parola):
        curr = radice
        for ch in parola:
            if ch not in curr.figli:
                os.system("cls" if sys.platform == "win32" else "clear")
                console.print(f"\n  [bold red]✗ '{parola}' NON trovata![/]  (manca '{ch}')\n")
                _stampa_trie(radice)
                aspetta()
                return False
            curr = curr.figli[ch]
        trovata = curr.fine_parola
        os.system("cls" if sys.platform == "win32" else "clear")
        if trovata:
            console.print(f"\n  [bold green]✓ '{parola}' TROVATA![/]\n")
        else:
            console.print(f"\n  [bold yellow]'{parola}' è un prefisso ma NON una parola completa[/]\n")
        _stampa_trie(radice)
        aspetta()
        return trovata

    def autocomplete(prefisso):
        curr = radice
        for ch in prefisso:
            if ch not in curr.figli:
                return []
            curr = curr.figli[ch]
        risultati = []
        def dfs_t(nodo, pref):
            if nodo.fine_parola:
                risultati.append(pref)
            for c, figlio in sorted(nodo.figli.items()):
                dfs_t(figlio, pref + c)
        dfs_t(curr, prefisso)
        return risultati

    parole = ["cane", "casa", "caro", "car", "gatto", "gara", "gate"]
    console.print(f"\n  Inserisco: {parole}\n")
    aspetta()
    for p in parole:
        inserisci(p)

    for pref in ["ca", "ga", "car"]:
        sugg = autocomplete(pref)
        os.system("cls" if sys.platform == "win32" else "clear")
        console.print(f"\n  [bold]Autocomplete '{pref}':[/]  [bold green]{sugg}[/]")
        console.print(f"  O({len(pref)}) per il nodo radice del prefisso, poi visita sottoalbero\n")
        _stampa_trie(radice)
        aspetta()

    for parola in ["car", "ca", "gatto", "gelato"]:
        cerca(parola)

    _pausa_fine_demo()


# ── Deque Demo ────────────────────────────────────────────────────────────────

def demo_deque():
    """Deque: coda bidirezionale con inserimento/rimozione O(1) da entrambe le estremità."""
    from collections import deque
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]DEQUE — Double-Ended Queue (Coda Bidirezionale)[/]\n\n"
        "Struttura con inserimento/rimozione da ENTRAMBE le estremità in O(1).\n"
        "Python: [bold cyan]collections.deque[/]\n\n"
        "  append(x)      → aggiunge a destra   O(1)\n"
        "  appendleft(x)  → aggiunge a sinistra O(1)\n"
        "  pop()          → rimuove da destra   O(1)\n"
        "  popleft()      → rimuove da sinistra O(1)\n\n"
        "Usato per: sliding window, BFS, undo/redo bidirezionale, palindromi.",
        border_style="green"
    ))
    aspetta()

    dq = deque()

    def mostra_deque(msg="", hl_sx=False, hl_dx=False):
        os.system("cls" if sys.platform == "win32" else "clear")
        items = list(dq)
        if not items:
            box = "[dim]◄ [ vuoto ] ►[/]"
        else:
            sx = "[bold red]◄[/]" if hl_sx else "[dim]◄[/]"
            dx = "[bold red]►[/]" if hl_dx else "[dim]►[/]"
            contenuto = " | ".join(f"[bold cyan]{v}[/]" for v in items)
            box = f"{sx} [ {contenuto} ] {dx}"
        sx_v = str(dq[0]) if dq else "—"
        dx_v = str(dq[-1]) if dq else "—"
        console.print(Panel(
            f"  {box}\n\n  Lunghezza: {len(dq)}  |  Sx: {sx_v}  |  Dx: {dx_v}",
            title="[bold]Deque[/]", border_style="blue"))
        if msg:
            console.print(f"  [cyan]{msg}[/]")

    ops = [
        ("append",     10,   "append(10) → aggiunge 10 a destra"),
        ("append",     20,   "append(20) → aggiunge 20 a destra"),
        ("appendleft", 5,    "appendleft(5) → aggiunge 5 a sinistra"),
        ("append",     30,   "append(30) → aggiunge 30 a destra"),
        ("appendleft", 1,    "appendleft(1) → aggiunge 1 a sinistra"),
        ("popleft",    None, "popleft() → rimuove da sinistra"),
        ("pop",        None, "pop() → rimuove da destra"),
        ("appendleft", 100,  "appendleft(100)"),
        ("pop",        None, "pop() → rimuove da destra"),
        ("popleft",    None, "popleft() → rimuove da sinistra"),
    ]

    mostra_deque("Deque inizialmente vuota")
    aspetta()
    for op, val, desc in ops:
        if op == "append":
            dq.append(val)
            mostra_deque(desc, hl_dx=True)
        elif op == "appendleft":
            dq.appendleft(val)
            mostra_deque(desc, hl_sx=True)
        elif op == "pop" and dq:
            rimosso = dq.pop()
            mostra_deque(f"pop() = {rimosso}  rimosso da destra", hl_dx=True)
        elif op == "popleft" and dq:
            rimosso = dq.popleft()
            mostra_deque(f"popleft() = {rimosso}  rimosso da sinistra", hl_sx=True)
        aspetta()

    # Bonus: palindromo
    console.print("\n  [bold]Bonus: controllo palindromo con Deque[/]")
    aspetta()
    for parola in ["radar", "python", "level", "ciao"]:
        dq_p = deque(parola)
        os.system("cls" if sys.platform == "win32" else "clear")
        console.print(f"\n  Parola: [bold]{parola}[/]  → Deque: {list(dq_p)}")
        is_pal = True
        while len(dq_p) > 1:
            sx = dq_p.popleft()
            dx = dq_p.pop()
            console.print(f"  '{sx}' vs '{dx}' → {'✓' if sx==dx else '✗'}")
            if sx != dx:
                is_pal = False
                break
        s = "bold green" if is_pal else "bold red"
        console.print(f"\n  [{s}]'{parola}' {'È' if is_pal else 'NON è'} un palindromo[/]")
        aspetta()

    _pausa_fine_demo()


# ── Bit Manipulation ──────────────────────────────────────────────────────────

def demo_bit_manipulation():
    """Demo visiva delle operazioni bitwise fondamentali."""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]BIT MANIPULATION — Operazioni Bitwise[/]\n\n"
        "I computer lavorano con bit (0 e 1) a livello hardware.\n"
        "Le operazioni bitwise sono [bold cyan]ultra-veloci[/] — una singola istruzione CPU.\n\n"
        "  AND (&)   → 1 solo se entrambi i bit sono 1\n"
        "  OR  (|)   → 1 se almeno un bit è 1\n"
        "  XOR (^)   → 1 solo se i bit sono diversi\n"
        "  NOT (~)   → inverte tutti i bit\n"
        "  SHL (<<)  → shift sinistro = moltiplicazione per 2\n"
        "  SHR (>>)  → shift destro  = divisione intera per 2",
        border_style="cyan"
    ))
    aspetta()

    def mostra_op(a, b, op_str, risultato, nome, desc):
        os.system("cls" if sys.platform == "win32" else "clear")
        a_bin = format(a & 0xFF, '08b')
        b_bin = format(b & 0xFF, '08b') if b is not None else None
        r_bin = format(risultato & 0xFF, '08b')
        console.print(f"\n  [bold]{nome}[/]  {desc}\n")
        console.print(f"  a = [cyan]{a:3}[/]  =  [bold]{a_bin}[/]")
        if b is not None:
            console.print(f"  b = [yellow]{b:3}[/]  =  [bold]{b_bin}[/]")
        console.print(f"  {'─'*35}")
        console.print(f"  r = [bold green]{risultato & 0xFF:3}[/]  =  [bold green]{r_bin}[/]")
        console.print(f"\n  [dim]Python: {a} {op_str} {b if b is not None else ''} = {risultato & 0xFF}[/]")
        aspetta()

    a, b = 60, 13   # 60 = 0b00111100,  13 = 0b00001101
    mostra_op(a, b, "&",  a & b, "AND  ( & )", "1 dove entrambi sono 1")
    mostra_op(a, b, "|",  a | b, "OR   ( | )", "1 dove almeno uno è 1")
    mostra_op(a, b, "^",  a ^ b, "XOR  ( ^ )", "1 dove i bit sono diversi")
    mostra_op(a, None, "~", ~a,  "NOT  ( ~ )", "inverte tutti i bit (complemento a 2)")

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print("\n  [bold]SHIFT SINISTRO (<<)[/]  =  moltiplicazione per potenza di 2\n")
    for i in range(1, 6):
        v = 1 << i
        console.print(f"  1 << {i}  =  {v:4}  =  {format(v,'08b')}  (1 × 2^{i})")
    aspetta()

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print("\n  [bold]SHIFT DESTRO (>>)[/]  =  divisione intera per potenza di 2\n")
    val = 64
    for i in range(1, 6):
        v = val >> i
        console.print(f"  {val} >> {i}  =  {v:4}  =  {format(v,'08b')}  ({val} ÷ 2^{i})")
    aspetta()

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print("\n  [bold]Trucchi Bitwise comuni:[/]\n")
    n = 42
    console.print(f"  n = {n}  =  {format(n,'08b')}\n")
    console.print(f"  n & 1       = {n & 1}   {'(dispari)' if n & 1 else '(pari)'}   [dim]# controlla parità[/]")
    console.print(f"  n & (n-1)   = {n & (n-1)}  [dim]# azzera il bit meno significativo[/]")
    console.print(f"  n & -n      = {n & -n}   [dim]# estrae il bit meno significativo[/]")
    console.print(f"  n ^ n       = {n ^ n}    [dim]# XOR con se stesso = 0[/]")
    console.print(f"  n ^ 0       = {n ^ 0}   [dim]# XOR con 0 = n invariato[/]")
    console.print(f"  bin({n}).count('1') = {bin(n).count('1')}  [dim]# numero di bit accesi[/]")
    aspetta()

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print("\n  [bold]Controlla se n è potenza di 2:[/]  n & (n-1) == 0\n")
    for test in [1, 2, 3, 4, 8, 10, 16, 18, 32, 64]:
        is_pow2 = test > 0 and (test & (test - 1)) == 0
        s = "bold green" if is_pow2 else "dim"
        console.print(f"  [{s}]{test:3} & {test-1:3} = {test & (test-1):3}  →  {'✓ Potenza di 2' if is_pow2 else 'No'}[/]")
    aspetta()

    _pausa_fine_demo()


# ── Triangolo di Pascal ───────────────────────────────────────────────────────

def demo_pascal_triangle():
    """Triangolo di Pascal: costruzione animata, proprietà e connessione a Sierpinski."""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]TRIANGOLO DI PASCAL[/]\n\n"
        "Ogni numero = somma dei due sopra di lui.\n"
        "Riga n = coefficienti binomiali C(n,k).\n\n"
        "Proprietà nascoste:\n"
        "  • Somma riga n = 2ⁿ\n"
        "  • Righe = potenze di 11:  11⁰=1, 11¹=11, 11²=121...\n"
        "  • Diagonali = numeri triangolari, tetrahedral...\n"
        "  • Parità (pari/dispari) → [bold cyan]Triangolo di Sierpinski![/]\n"
        "  • Usato in: probabilità, combinatoria, divide & conquer.",
        border_style="cyan"
    ))
    aspetta()

    try:
        n = int(input("  Quante righe? (5–14, invio=10): ") or "10")
        n = max(5, min(n, 14))
    except ValueError:
        n = 10

    triangle = [[1]]
    for i in range(1, n):
        riga = [1]
        for j in range(1, i):
            riga.append(triangle[i-1][j-1] + triangle[i-1][j])
        riga.append(1)
        triangle.append(riga)

    # Animazione costruzione riga per riga
    for i in range(n):
        os.system("cls" if sys.platform == "win32" else "clear")
        console.print(f"\n  [bold]Triangolo di Pascal — Riga {i}:[/]  {triangle[i]}\n")
        for j, riga in enumerate(triangle[:i+1]):
            spazi = "  " * (n - j - 1)
            parti = []
            for k, val in enumerate(riga):
                if j == i:
                    parti.append(f"[bold cyan]{val}[/]")
                elif k == 0 or k == len(riga) - 1:
                    parti.append(f"[green]{val}[/]")
                else:
                    parti.append(str(val))
            console.print("  " + spazi + "  ".join(parti))
        if i > 0:
            console.print(f"\n  Somma riga {i} = {sum(triangle[i])} = 2^{i}")
        aspetta()

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print("\n  [bold]Somme delle righe:[/]\n")
    for i, riga in enumerate(triangle):
        s = sum(riga)
        console.print(f"  Riga {i:2}: somma = [bold green]{s:5}[/]  = 2^{i} = {2**i}  {'✓' if s == 2**i else '✗'}")
    aspetta()

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print("\n  [bold]Parità (dispari=█ pari=spazio) → Triangolo di Sierpinski![/]\n")
    for j, riga in enumerate(triangle):
        spazi = "  " * (n - j - 1)
        visuale = "  ".join("[bold yellow]█[/]" if v % 2 == 1 else " " for v in riga)
        console.print("  " + spazi + visuale)
    aspetta()

    _pausa_fine_demo()


# ── Monte Carlo Pi ────────────────────────────────────────────────────────────

def demo_monte_carlo_pi():
    """Stima π con il metodo Monte Carlo — algoritmo probabilistico."""
    import time
    import math
    try:
        from rich.live import Live
    except ImportError:
        console.print("[red]Richiede rich>=12.0[/]")
        input("\n  [INVIO] ")
        return

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]MONTE CARLO PI — Algoritmo Probabilistico[/]\n\n"
        "Stima π lanciando punti casuali in un quadrato 1×1.\n"
        "Se il punto cade nel quarto di cerchio → lo contiamo.\n\n"
        "  x² + y² ≤ 1  →  dentro il cerchio  [green]·[/]\n"
        "  x² + y² > 1  →  fuori dal cerchio   [red]×[/]\n\n"
        "  π ≈ 4 × (punti nel cerchio) / (punti totali)\n\n"
        "Algoritmi probabilistici: soluzioni approssimate ma veloci!\n"
        "Premi Ctrl+C per fermare.",
        border_style="cyan"
    ))
    time.sleep(2)

    cols, rows = shutil.get_terminal_size(fallback=(80, 24))
    W = min((cols - 6) // 2, 40)
    H = min(rows - 10, 20)

    griglia   = [[' '] * W for _ in range(H)]
    stili     = [[''] * W for _ in range(H)]
    totale = dentro = 0
    pi_stimato = 0.0

    def render():
        t = Text()
        err = abs(pi_stimato - math.pi) if totale > 0 else 0.0
        t.append(
            f"  Punti: {totale:5}  |  Nel cerchio: {dentro:5}  |  "
            f"π ≈ [bold cyan]{pi_stimato:.5f}[/]  |  errore: {err:.5f}\n\n",
            style="dim")
        for r in range(H):
            t.append("  ")
            for c in range(W):
                t.append(griglia[r][c] + " ", style=stili[r][c] or "dim")
            t.append("\n")
        return Panel(t, title="[bold cyan]Monte Carlo π[/]", border_style="cyan")

    try:
        with Live(render(), refresh_per_second=12) as live:
            for _ in range(6000):
                x = random.random()
                y = random.random()
                totale += 1
                gr = min(H - 1, int(y * H))
                gc = min(W - 1, int(x * W))
                if x * x + y * y <= 1.0:
                    dentro += 1
                    griglia[gr][gc] = '·'
                    stili[gr][gc]   = "bold green"
                else:
                    griglia[gr][gc] = '×'
                    stili[gr][gc]   = "bold red"
                if totale > 0:
                    pi_stimato = 4.0 * dentro / totale
                if totale % 25 == 0:
                    live.update(render())
                    time.sleep(0.001)
            live.update(render())
            time.sleep(3)
    except KeyboardInterrupt:
        pass

    console.print(f"\n  π reale   = {math.pi:.6f}")
    console.print(f"  π stimato = {pi_stimato:.6f}")
    console.print(f"  Errore    = {abs(pi_stimato - math.pi):.6f}")
    console.print(f"  Punti usati: {totale}")
    _pausa_fine_demo()


# ── Flood Fill ────────────────────────────────────────────────────────────────

def demo_flood_fill():
    """Flood Fill — BFS su griglia (algoritmo del secchiello di Paint)."""
    import time
    from collections import deque
    try:
        from rich.live import Live
    except ImportError:
        console.print("[red]Richiede rich>=12.0[/]")
        input("\n  [INVIO] ")
        return

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]FLOOD FILL — Riempimento a Cascata[/]\n\n"
        "L'algoritmo dietro il [bold cyan]Secchiello[/] di Paint!\n"
        "Partendo da un punto sorgente, colora tutte le celle\n"
        "connesse con lo stesso colore di sfondo.\n\n"
        "  [dim]░[/]   = cella libera\n"
        "  [bold red]█[/]   = muro (blocca il fill)\n"
        "  [bold blue]▓[/]   = riempita (BFS ha raggiunto qui)\n"
        "  [bold yellow]S[/]   = punto di partenza\n\n"
        "Implementazione: BFS — espansione a onde dal punto S.\n"
        "Complessità: O(W×H)  —  usato in editor grafici, giochi, image segmentation.\n"
        "Premi Ctrl+C per fermare.",
        border_style="blue"
    ))
    time.sleep(2)

    cols, rows = shutil.get_terminal_size(fallback=(80, 24))
    W = min((cols - 6) // 2, 35)
    H = min(rows - 8, 18)

    LIBERO, MURO, RIEMPITO, START = 0, 1, 2, 3
    mappa = [[LIBERO] * W for _ in range(H)]

    # Muri casuali (~18%)
    for _ in range(W * H // 5):
        r, c = random.randint(0, H - 1), random.randint(0, W - 1)
        mappa[r][c] = MURO

    # Alcune pareti per creare stanze
    for _ in range(2):
        if random.random() < 0.5:
            c = random.randint(W // 4, 3 * W // 4)
            for r in range(random.randint(0, H // 3), random.randint(2 * H // 3, H)):
                if 0 <= r < H: mappa[r][c] = MURO
        else:
            r = random.randint(H // 4, 3 * H // 4)
            for c in range(random.randint(0, W // 3), random.randint(2 * W // 3, W)):
                if 0 <= c < W: mappa[r][c] = MURO

    sr, sc = 1, 1
    mappa[sr][sc] = START

    CHARS = {
        LIBERO:   ("░", "dim"),
        MURO:     ("█", "bold red"),
        RIEMPITO: ("▓", "bold blue"),
        START:    ("S", "bold yellow"),
    }

    def render(step, coda_len):
        t = Text()
        t.append(f"  Passo {step} | In coda: {coda_len} | Ctrl+C per fermare\n\n", style="dim")
        for r in range(H):
            t.append("  ")
            for c in range(W):
                ch, st = CHARS[mappa[r][c]]
                t.append(ch + " ", style=st)
            t.append("\n")
        return Panel(t, title="[bold blue]Flood Fill — BFS[/]", border_style="blue")

    coda = deque([(sr, sc)])
    visitati = {(sr, sc)}
    step = 0

    try:
        with Live(render(0, 1), refresh_per_second=15) as live:
            while coda:
                batch = min(4, len(coda))
                for _ in range(batch):
                    if not coda:
                        break
                    r, c = coda.popleft()
                    if mappa[r][c] != START:
                        mappa[r][c] = RIEMPITO
                    for dr, dc in [(-1, 0), (1, 0), (0, -1), (0, 1)]:
                        nr, nc = r + dr, c + dc
                        if 0 <= nr < H and 0 <= nc < W and (nr, nc) not in visitati and mappa[nr][nc] == LIBERO:
                            visitati.add((nr, nc))
                            coda.append((nr, nc))
                step += 1
                if step % 2 == 0:
                    live.update(render(step, len(coda)))
                    time.sleep(0.04)
            live.update(render(step, 0))
            time.sleep(2)
    except KeyboardInterrupt:
        pass

    riempite = sum(cell == RIEMPITO for row in mappa for cell in row)
    console.print(f"\n  ✓ Flood Fill completato!  Celle riempite: {riempite}")
    _pausa_fine_demo()


# ── Spiral Matrix ─────────────────────────────────────────────────────────────

def demo_spiral_matrix():
    """Spiral Matrix — attraversamento a spirale con frecce direzionali e colori per anello."""
    import time
    try:
        from rich.live import Live
    except ImportError:
        console.print("[red]Richiede rich>=12.0[/]")
        input("\n  [INVIO] ")
        return

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]SPIRAL MATRIX — Attraversamento a Spirale[/]\n\n"
        "Ogni cella visitata mostra la freccia della direzione di percorrenza.\n"
        "Ogni anello (giro) della spirale ha un colore diverso.\n\n"
        "  [bold cyan]→[/]  top  (sx→dx)    [bold yellow]↓[/]  right (su→giù)\n"
        "  [bold green]←[/]  bottom (dx→sx) [bold magenta]↑[/]  left (giù→su)\n"
        "  [bold white]★[/]  = cella corrente\n"
        "  [dim]##[/]  = non ancora visitata\n\n"
        "Algoritmo: 4 bordi mobili (top/bottom/left/right),\n"
        "restringiti di 1 dopo ogni lato completato.\n"
        "Chiesto in colloqui tecnici FAANG. Ctrl+C per fermare.",
        border_style="magenta"
    ))
    time.sleep(2)

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print("  Scegli dimensione matrice (invio = 5×8):")
    try:
        N = int(input("  Righe  (3–8,  invio=5): ") or "5")
        N = max(3, min(8, N))
        M = int(input(f"  Colonne (3–12, invio=8): ") or "8")
        M = max(3, min(12, M))
    except ValueError:
        N, M = 5, 8

    # Valori della matrice (1..N*M)
    matrix = [[i * M + j + 1 for j in range(M)] for i in range(N)]

    # Calcola spirale con: (riga, col, direzione, anello)
    RING_COLORS = ["cyan", "yellow", "green", "magenta", "red", "blue"]
    DIR_ARROW   = {'R': '→', 'D': '↓', 'L': '←', 'U': '↑'}
    DIR_COLOR   = {'R': "bold cyan", 'D': "bold yellow",
                   'L': "bold green", 'U': "bold magenta"}

    ordine = []  # (r, c, dir, ring)
    top, bottom, left, right, ring = 0, N - 1, 0, M - 1, 0
    while top <= bottom and left <= right:
        for c in range(left,  right + 1):      ordine.append((top, c, 'R', ring))
        top += 1
        for r in range(top,   bottom + 1):     ordine.append((r, right, 'D', ring))
        right -= 1
        if top <= bottom:
            for c in range(right, left - 1, -1): ordine.append((bottom, c, 'L', ring))
            bottom -= 1
        if left <= right:
            for r in range(bottom, top - 1, -1):  ordine.append((r, left, 'U', ring))
            left += 1
        ring += 1

    # Quante cifre ha il numero più grande? (larghezza cella)
    CW  = len(str(N * M))   # larghezza valore
    CEL = CW + 2            # larghezza totale cella (freccia + valore + spazio)

    # Dizionario celle visitate: (r,c) -> (dir, ring)
    visited  = {}
    cur_pos  = None
    cur_dir  = None
    cur_ring = 0

    def render(step_n, sequenza):
        t = Text()
        seq_tail = sequenza[-10:] if len(sequenza) > 10 else sequenza
        t.append(f"  Passo {step_n:3}/{len(ordine)}  |  Spirale: {seq_tail}\n\n", style="dim")

        for r in range(N):
            t.append("  ")
            for c in range(M):
                val    = matrix[r][c]
                val_s  = str(val).rjust(CW)

                if (r, c) == cur_pos:
                    # Cella corrente: stella gialla
                    col = RING_COLORS[cur_ring % len(RING_COLORS)]
                    t.append(f"★{val_s} ", style=f"bold white on {col}")
                elif (r, c) in visited:
                    d, rg = visited[(r, c)]
                    arrow = DIR_ARROW[d]
                    t.append(f"{arrow}{val_s} ", style=DIR_COLOR[d])
                else:
                    # Non ancora visitata
                    t.append(f"{'·' * (CW + 1)} ", style="dim")
            t.append("\n")

        # Legenda anelli
        t.append("\n  Legenda anelli: ", style="dim")
        max_ring = max((v[1] for v in visited.values()), default=0)
        for i in range(max_ring + 1):
            col = RING_COLORS[i % len(RING_COLORS)]
            t.append(f" Anello {i+1} ", style=f"bold {col}")

        return Panel(
            t,
            title=f"[bold magenta]Spiral Matrix {N}×{M}[/]",
            border_style="magenta"
        )

    sequenza = []
    delay = max(0.08, min(0.35, 5.0 / (N * M)))  # velocità adattiva
    try:
        with Live(render(0, []), refresh_per_second=10) as live:
            for step_n, (r, c, d, rg) in enumerate(ordine):
                cur_pos  = (r, c)
                cur_dir  = d
                cur_ring = rg
                visited[(r, c)] = (d, rg)
                sequenza.append(matrix[r][c])
                live.update(render(step_n + 1, sequenza))
                time.sleep(delay)
            cur_pos = None
            live.update(render(len(ordine), sequenza))
            time.sleep(3)
    except KeyboardInterrupt:
        pass

    console.print(f"\n  Sequenza spirale completa ({len(sequenza)} elementi):")
    console.print(f"  {sequenza}")
    _pausa_fine_demo()


# ── Cubo di Rubik — BFS su 2×2×2 ─────────────────────────────────────────────

def demo_rubik():
    """Cubo di Rubik 2×2×2 — risoluzione BFS passo per passo con visualizzazione."""
    import time
    from collections import deque

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]CUBO DI RUBIK 2×2×2 — Backtracking / BFS[/]\n\n"
        "Il Cubo 2×2×2 (Pocket Cube) ha [bold]3.674.160[/] stati possibili.\n"
        "Ogni faccia ha 4 celle; 6 facce = 24 celle totali.\n\n"
        "  [bold cyan]U[/] = Up (sopra)    [bold yellow]D[/] = Down (sotto)\n"
        "  [bold green]F[/] = Front (fronte) [bold blue]B[/] = Back (retro)\n"
        "  [bold red]R[/] = Right (destra)  [bold magenta]L[/] = Left (sinistra)\n\n"
        "Ogni mossa ruota 90° una faccia (senso orario).\n"
        "Algoritmo: [bold]BFS[/] — trova la soluzione con il minimo numero di mosse.\n\n"
        "Il cubo viene mescolato con mosse casuali, poi il BFS lo risolve.",
        border_style="cyan"
    ))
    time.sleep(2)

    # ── Rappresentazione stato ──────────────────────────────────────────────
    # Stato = tupla 24 interi (0-5 = W Y G B R O) che rappresentano i colori
    # delle 24 celle nelle posizioni fisse:
    # U: 0-3, D: 4-7, F: 8-11, B: 12-15, R: 16-19, L: 20-23

    COLORI   = ["W", "Y", "G", "B", "R", "O"]   # bianco giallo verde blu rosso arancio
    STILI    = ["bold white", "bold yellow", "bold green", "bold blue", "bold red", "bold magenta"]
    STATO_OK = tuple(
        c for face in range(6) for _ in range(4) for c in [face]
    )  # (0,0,0,0, 1,1,1,1, 2,2,2,2, 3,3,3,3, 4,4,4,4, 5,5,5,5)

    # ── Mosse: ogni mossa è una permutazione degli indici ──────────────────
    # Cicli di 4 celle che si spostano. Ogni ciclo è (a,b,c,d): a→b→c→d→a
    MOSSE_CICLI: dict[str, list[tuple]] = {
        "U": [(0,2,3,1), (8,16,13,20), (9,18,12,22), (10,17,15,21), (11,19,14,23)],
        "D": [(4,5,7,6), (10,22,15,19), (11,20,14,17), (9,23,13,18), (8,21,12,16)],
        "F": [(8,9,11,10), (2,16,5,23), (3,18,4,21), (1,17,6,22), (0,19,7,20)],
        "B": [(12,13,15,14), (1,20,6,17), (0,22,7,19), (3,21,4,18), (2,23,5,16)],
        "R": [(16,18,19,17), (2,12,7,8), (3,15,6,11), (1,13,4,9), (0,14,5,10)],
        "L": [(20,21,23,22), (0,9,7,14), (1,11,6,12), (3,8,4,15), (2,10,5,13)],
    }
    NOMI_MOSSE = ["U", "D", "F", "B", "R", "L"]

    def applica(stato: tuple, nome: str) -> tuple:
        s = list(stato)
        for (a, b, c, d) in MOSSE_CICLI[nome]:
            s[b], s[c], s[d], s[a] = s[a], s[b], s[c], s[d]
        return tuple(s)

    # ── Cubo visivo 2×2×2 ──────────────────────────────────────────────────
    LAYOUT = [
        # (faccia, offset_nella_faccia, riga_display, col_display)
        # U in alto al centro
        (0,0,0,4),(0,1,0,5),(0,2,1,4),(0,3,1,5),
        # L a sinistra
        (5,0,2,0),(5,1,2,1),(5,2,3,0),(5,3,3,1),
        # F al centro
        (2,0,2,4),(2,1,2,5),(2,2,3,4),(2,3,3,5),
        # R a destra
        (4,0,2,8),(4,1,2,9),(4,2,3,8),(4,3,3,9),
        # B all'estrema destra
        (3,0,2,12),(3,1,2,13),(3,2,3,12),(3,3,3,13),
        # D in basso al centro
        (1,0,4,4),(1,1,4,5),(1,2,5,4),(1,3,5,5),
    ]

    def disegna_cubo(stato: tuple) -> Text:
        griglia = [[" " for _ in range(16)] for _ in range(7)]
        stili_g = [["" for _ in range(16)] for _ in range(7)]
        for (faccia, off, riga, col) in LAYOUT:
            idx = faccia * 4 + off
            c = stato[idx]
            griglia[riga][col] = COLORI[c]
            stili_g[riga][col] = STILI[c]
        t = Text()
        for r in range(7):
            t.append("  ")
            for c in range(14):
                ch = griglia[r][c]
                if ch != " ":
                    t.append(f"[{ch}]", style=stili_g[r][c])
                else:
                    t.append(" ")
            t.append("\n")
        return t

    # ── Genera scramble casuale ─────────────────────────────────────────────
    import random
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print("  Quante mosse di scramble? (2–7, invio = 5):")
    try:
        n_mosse = int(input("  ❯ ") or "5")
        n_mosse = max(2, min(7, n_mosse))
    except ValueError:
        n_mosse = 5

    stato_iniziale = STATO_OK
    scramble = []
    ultima = ""
    for _ in range(n_mosse):
        disponibili = [m for m in NOMI_MOSSE if m != ultima]
        mossa = random.choice(disponibili)
        scramble.append(mossa)
        stato_iniziale = applica(stato_iniziale, mossa)
        ultima = mossa

    # ── BFS ────────────────────────────────────────────────────────────────
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        f"  Scramble: [bold yellow]{' '.join(scramble)}[/]  ({n_mosse} mosse)\n\n"
        "  BFS in corso — esplora tutti gli stati livello per livello...",
        border_style="yellow", title="[bold]Cubo di Rubik 2×2×2[/]"
    ))

    visitati: dict[tuple, tuple | None] = {stato_iniziale: None}
    coda: deque[tuple[tuple, list[str]]] = deque([(stato_iniziale, [])])
    soluzione: list[str] = []
    stati_esplorati = 0

    while coda:
        stato_cur, mosse_cur = coda.popleft()
        stati_esplorati += 1
        if stato_cur == STATO_OK:
            soluzione = mosse_cur
            break
        if len(mosse_cur) >= 7:   # limite profondità (ottimale per 2×2×2 ≤7 mosse)
            continue
        for m in NOMI_MOSSE:
            ns = applica(stato_cur, m)
            if ns not in visitati:
                visitati[ns] = stato_cur
                coda.append((ns, mosse_cur + [m]))

    # ── Mostra soluzione passo per passo ───────────────────────────────────
    os.system("cls" if sys.platform == "win32" else "clear")

    stati_path = [stato_iniziale]
    s = stato_iniziale
    for m in soluzione:
        s = applica(s, m)
        stati_path.append(s)

    etichette = ["(Scramblato)"] + [f"Mossa {i+1}: [bold cyan]{m}[/]" for i, m in enumerate(soluzione)]

    for i, (st, etich) in enumerate(zip(stati_path, etichette)):
        os.system("cls" if sys.platform == "win32" else "clear")
        t = Text()
        t.append(f"  Scramble:  {' '.join(scramble)}\n", style="dim")
        t.append(f"  Soluzione: {' '.join(soluzione) if soluzione else '—  già risolto!'}\n", style="dim")
        t.append(f"  Passo {i}/{len(soluzione)}  —  {stati_esplorati} stati esplorati\n\n", style="dim")
        t.append_text(disegna_cubo(st))

        console.print(Panel(
            t,
            title=f"[bold magenta]Cubo di Rubik — {etich}[/]",
            border_style="magenta"
        ))
        if i < len(stati_path) - 1:
            time.sleep(1.2)
        else:
            console.print("\n  [bold green]✅ Cubo risolto![/]" if soluzione else "\n  [bold green]✅ Era già risolto![/]")

    _pausa_fine_demo()


# ── Tarjan SCC ────────────────────────────────────────────────────────────────

def demo_tarjan_scc():
    """Tarjan: Strongly Connected Components — O(V+E)."""
    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]TARJAN — Componenti Fortemente Connesse (SCC)[/]\n\n"
        "Un SCC = gruppo di nodi dove ogni nodo raggiunge tutti gli altri.\n"
        "Algoritmo di Tarjan: DFS con stack + array disc/low.\n\n"
        "  disc[v]  = tempo di scoperta di v nella DFS\n"
        "  low[v]   = disco minimo raggiungibile da v (inclusi antenati via archi back)\n"
        "  Se low[v] == disc[v]  →  v è la radice di un SCC!\n\n"
        "Complessità: O(V+E)  —  usato in compilatori, analisi dipendenze, 2-SAT.",
        border_style="blue"
    ))
    aspetta()

    grafo = {
        "A": ["B"], "B": ["C", "D"], "C": ["A"],    # SCC {A,B,C}
        "D": ["E"], "E": ["D"],                       # SCC {D,E}
        "F": ["G", "D"], "G": ["H"], "H": ["F"],      # SCC {F,G,H}
    }
    nodi = ["A", "B", "C", "D", "E", "F", "G", "H"]

    disc     = {nd: -1 for nd in nodi}
    low      = {nd: -1 for nd in nodi}
    on_stack = {nd: False for nd in nodi}
    stack    = []
    timer    = [0]
    sccs     = []

    def tarjan_dfs(v):
        disc[v] = low[v] = timer[0]
        timer[0] += 1
        stack.append(v)
        on_stack[v] = True
        os.system("cls" if sys.platform == "win32" else "clear")
        console.print(f"\n  [bold]Tarjan SCC[/]  Visito: [bold cyan]{v}[/]")
        console.print(f"  disc[{v}] = low[{v}] = {disc[v]}")
        console.print(f"  Stack: {stack}")
        aspetta()

        for u in grafo.get(v, []):
            if disc[u] == -1:
                tarjan_dfs(u)
                low[v] = min(low[v], low[u])
                console.print(f"  ← Risalgo a {v}: low[{v}] = min({low[v]}, low[{u}]={low[u]}) = {low[v]}")
                aspetta()
            elif on_stack[u]:
                low[v] = min(low[v], disc[u])
                console.print(f"  Arco back verso {u} (sullo stack): low[{v}] → {low[v]}")
                aspetta()

        if low[v] == disc[v]:
            scc = []
            while True:
                u = stack.pop()
                on_stack[u] = False
                scc.append(u)
                if u == v:
                    break
            sccs.append(scc)
            os.system("cls" if sys.platform == "win32" else "clear")
            console.print(f"\n  [bold green]✓ SCC trovata! Radice: {v}[/]")
            console.print(f"  Nodi: {{{', '.join(sorted(scc))}}}")
            console.print(f"  SCC finora: {['{'+', '.join(sorted(s))+'}' for s in sccs]}")
            aspetta()

    for nd in nodi:
        if disc[nd] == -1:
            tarjan_dfs(nd)

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "  Strongly Connected Components:\n\n" +
        "".join(f"  SCC {i+1}: {{{', '.join(sorted(scc))}}}\n" for i, scc in enumerate(sccs)) +
        f"\n  [bold green]✓ Totale: {len(sccs)} SCC — algoritmo Tarjan O(V+E)[/]",
        title="[bold]Tarjan SCC — COMPLETATO[/]", border_style="blue"
    ))
    _pausa_fine_demo()


# ── Sierpinski Triangle ───────────────────────────────────────────────────────

def demo_sierpinski():
    """Triangolo di Sierpinski — frattale con algoritmo del caos."""
    import time
    try:
        from rich.live import Live
    except ImportError:
        console.print("[red]Richiede rich>=12.0[/]")
        input("\n  [INVIO] ")
        return

    os.system("cls" if sys.platform == "win32" else "clear")
    console.print(Panel(
        "[bold]TRIANGOLO DI SIERPINSKI — Frattale[/]\n\n"
        "Frattale ricorsivo: dividi un triangolo in 4, rimuovi il centrale, ripeti.\n\n"
        "Algoritmo del Caos (Chaos Game):\n"
        "  1. Scegli un vertice a caso tra i 3 del triangolo\n"
        "  2. Spostati a metà strada verso quel vertice\n"
        "  3. Disegna un punto\n"
        "  4. Ripeti 50.000 volte\n\n"
        "Risultato: emerge il frattale!\n"
        "[bold cyan]Connessione al Triangolo di Pascal:[/] colorando di giallo\n"
        "i numeri dispari di Pascal ottieni Sierpinski!\n\n"
        "Premi Ctrl+C per fermare.",
        border_style="yellow"
    ))
    time.sleep(2)

    cols, rows = shutil.get_terminal_size(fallback=(80, 24))
    W = min(cols - 6, 68)
    H = min(rows - 6, 30)

    # Vertici del triangolo grande
    v = [(W // 2, 0), (0, H - 1), (W - 1, H - 1)]

    griglia = [[' '] * W for _ in range(H)]
    colori  = [[''] * W for _ in range(H)]
    PALETTE = ["bold yellow", "bold cyan", "bold green"]

    px, py = W // 2, H // 2

    def render(n_points):
        t = Text()
        t.append(f"  Punti: {n_points} | Vertici: {v} | Ctrl+C per fermare\n\n", style="dim")
        for r in range(H):
            t.append("  ")
            for c in range(W):
                ch = griglia[r][c]
                st = colori[r][c]
                t.append(ch, style=st if st else "")
            t.append("\n")
        return Panel(t, title="[bold yellow]Sierpinski — Algoritmo del Caos[/]", border_style="yellow")

    try:
        with Live(render(0), refresh_per_second=15) as live:
            for n in range(50000):
                vi = random.randint(0, 2)
                vx, vy = v[vi]
                px = (px + vx) // 2
                py = (py + vy) // 2
                if 0 <= py < H and 0 <= px < W:
                    griglia[py][px] = '█'
                    colori[py][px]  = PALETTE[vi]
                if n % 400 == 0:
                    live.update(render(n))
                    time.sleep(0.001)
            live.update(render(50000))
            time.sleep(3)
    except KeyboardInterrupt:
        pass

    _pausa_fine_demo()


# ── Menu simulatore ───────────────────────────────────────────────────────────

def lancia_algo_textual(s: int) -> None:
    """
    Lancia l'algoritmo numero s con parametri auto-generati.
    Usato da AlgoritmoScreen (Textual, nessun input() interattivo).
    """
    VUOLE_ARRAY     = set(range(1, 17)) | {28, 29, 67, 68, 69, 70}
    VUOLE_TARGET    = {13, 14, 15, 16}
    VUOLE_K         = {29}
    VUOLE_TARGET_TP = {28}

    arr = genera_lista(8) if s in VUOLE_ARRAY else None
    target = None
    k = 3

    if arr and s in {14, 15, 16}:
        arr = sorted(arr)
    if arr and s in VUOLE_TARGET:
        target = arr[len(arr) // 2]
    if arr and s in VUOLE_TARGET_TP:
        target = arr[0] + arr[1]
    if arr and s in VUOLE_K:
        k = max(2, min(3, len(arr) - 1))

    _log.clear()
    match s:
        case 1:  bubble_sort(arr)
        case 2:  cocktail_sort(arr)
        case 3:  selection_sort(arr)
        case 4:  insertion_sort(arr)
        case 5:  shell_sort(arr)
        case 6:  gnome_sort(arr)
        case 7:  counting_sort(arr)
        case 8:  radix_sort(arr)
        case 9:  bucket_sort(arr)
        case 10: merge_sort(arr)
        case 11: quick_sort(arr)
        case 12: heap_sort(arr)
        case 13: ricerca_lineare(arr, target)
        case 14: ricerca_binaria(arr, target)
        case 15: jump_search(arr, target)
        case 16: interpolation_search(arr, target)
        case 17: demo_stack_queue()
        case 18: demo_linked_list()
        case 19: demo_bst()
        case 20: demo_hash_table()
        case 21: demo_min_heap()
        case 22: demo_bfs()
        case 23: demo_dfs()
        case 24: demo_dijkstra()
        case 25: demo_fibonacci()
        case 26: demo_hanoi()
        case 27: demo_sieve()
        case 28: demo_two_pointers(arr, target)
        case 29: demo_sliding_window(arr, k)
        case 30: demo_lru_cache()
        case 31: demo_knapsack()
        case 32: demo_coin_change()
        case 33: demo_lcs()
        case 34: demo_edit_distance()
        case 35: demo_lis()
        case 36: demo_kmp()
        case 37: demo_rabin_karp()
        case 38: demo_z_algorithm()
        case 39: demo_bellman_ford()
        case 40: demo_floyd_warshall()
        case 41: demo_kruskal()
        case 42: demo_prim()
        case 43: demo_astar()
        case 44: demo_topological_sort()
        case 45: demo_activity_selection()
        case 46: demo_huffman()
        case 47: demo_fractional_knapsack()
        case 48: demo_nqueens()
        case 49: demo_sudoku()
        case 50: demo_gcd()
        case 51: demo_modular_exp()
        case 52: demo_miller_rabin()
        case 53: demo_karatsuba()
        case 54: demo_avl_tree()
        case 55: demo_segment_tree()
        case 56: demo_fenwick_tree()
        case 57: demo_union_find()
        case 58: demo_convex_hull()
        case 59: demo_game_of_life()
        case 60: demo_maze_bfs()
        case 61: demo_sorting_race()
        case 62: demo_nqueens_live()
        case 63: demo_dijkstra_griglia()
        case 64: demo_rule30()
        case 65: demo_forest_fire()
        case 66: demo_matrix_rain()
        case 67: tim_sort(arr)
        case 68: pancake_sort(arr)
        case 69: quickselect(arr)
        case 70: comb_sort(arr)
        case 71: demo_trie()
        case 72: demo_deque()
        case 73: demo_bit_manipulation()
        case 74: demo_pascal_triangle()
        case 75: demo_monte_carlo_pi()
        case 76: demo_flood_fill()
        case 77: demo_spiral_matrix()
        case 78: demo_tarjan_scc()
        case 79: demo_sierpinski()
        case 80: demo_rubik()


def menu_simulatore():
    VUOLE_ARRAY    = set(range(1, 17)) | {28, 29, 67, 68, 69, 70}
    VUOLE_TARGET   = {13, 14, 15, 16}
    VUOLE_K        = {29}
    VUOLE_TARGET_TP = {28}

    def _v(n, nome):
        return f"  [dim]{n:2}.[/] {nome}"

    while True:
        try:
            os.system("cls" if sys.platform == "win32" else "clear")
        except Ridimensiona:
            continue
        cols, _ = shutil.get_terminal_size(fallback=(80, 24))
        con = Console(width=cols)
        stampa_breadcrumb(con, "Prismalux › 📚 Apprendimento › Simulatore")

        tab = Table(box=None, show_header=False, expand=False, padding=(0, 1, 0, 0))
        tab.add_column(min_width=22, no_wrap=True)
        tab.add_column(min_width=22, no_wrap=True)
        tab.add_column(min_width=22, no_wrap=True)
        tab.add_column(min_width=22, no_wrap=True)

        def sep(titolo, colore):
            tab.add_row(f"[bold {colore}]{titolo}[/]", "", "", "")

        sep("🟡 ORDINAMENTO BASE — O(n²)", "yellow")
        tab.add_row(_v(1,"Bubble Sort"),      _v(2,"Cocktail Sort"),     _v(3,"Selection Sort"),   _v(4,"Insertion Sort"))
        tab.add_row(_v(5,"Shell Sort"),        _v(6,"Gnome Sort"),        "","")
        tab.add_row("","","","")

        sep("🟠 NON-COMPARATIVO", "magenta")
        tab.add_row(_v(7,"Counting Sort"),     _v(8,"Radix Sort"),        _v(9,"Bucket Sort"),      "")
        tab.add_row("","","","")

        sep("🔴 EFFICIENTE — O(n log n)", "red")
        tab.add_row(_v(10,"Merge Sort"),       _v(11,"Quick Sort"),       _v(12,"Heap Sort"),       "")
        tab.add_row("","","","")

        sep("🔵 RICERCA", "cyan")
        tab.add_row(_v(13,"Ricerca Lineare"),  _v(14,"Ricerca Binaria"),  _v(15,"Jump Search"),     _v(16,"Interpolation Search"))
        tab.add_row("","","","")

        sep("🟢 STRUTTURE DATI BASE", "green")
        tab.add_row(_v(17,"Stack e Queue"),    _v(18,"Linked List"),      _v(19,"BST"),             _v(20,"Hash Table"))
        tab.add_row(_v(21,"Min-Heap"),         "","","")
        tab.add_row("","","","")

        sep("🔷 GRAFI BASE", "blue")
        tab.add_row(_v(22,"BFS"),              _v(23,"DFS"),              _v(24,"Dijkstra"),        "")
        tab.add_row("","","","")

        sep("⭐ ALGORITMI CLASSICI", "bright_yellow")
        tab.add_row(_v(25,"Fibonacci"),        _v(26,"Torre di Hanoi"),   _v(27,"Crivello Eratos."),_v(28,"Two Pointers"))
        tab.add_row(_v(29,"Sliding Window"),   _v(30,"LRU Cache"),        "","")
        tab.add_row("","","","")

        sep("💚 PROGRAMMAZIONE DINAMICA", "green")
        tab.add_row(_v(31,"Zaino 0/1"),        _v(32,"Coin Change"),      _v(33,"LCS"),             _v(34,"Edit Distance"))
        tab.add_row(_v(35,"LIS"),              "","","")
        tab.add_row("","","","")

        sep("🟣 ALGORITMI SU STRINGHE", "magenta")
        tab.add_row(_v(36,"KMP Search"),       _v(37,"Rabin-Karp"),       _v(38,"Z-Algorithm"),     "")
        tab.add_row("","","","")

        sep("🌐 GRAFI AVANZATI", "blue")
        tab.add_row(_v(39,"Bellman-Ford"),     _v(40,"Floyd-Warshall"),   _v(41,"Kruskal MST"),     _v(42,"Prim MST"))
        tab.add_row(_v(43,"A* Search"),        _v(44,"Topological Sort"), "","")
        tab.add_row("","","","")

        sep("💛 GREEDY", "yellow")
        tab.add_row(_v(45,"Activity Selection"),_v(46,"Huffman Coding"),  _v(47,"Zaino Frazionario"),"")
        tab.add_row("","","","")

        sep("🔴 BACKTRACKING  |  🔢 MATEMATICI", "red")
        tab.add_row(_v(48,"N-Queens"),         _v(49,"Sudoku Solver"),    _v(50,"GCD Euclide"),     _v(51,"Potenza Modulare"))
        tab.add_row(_v(52,"Miller-Rabin"),     _v(53,"Karatsuba"),        _v(80,"Cubo di Rubik"),  "")
        tab.add_row("","","","")

        sep("🏗️ STRUTTURE DATI AVANZATE  |  📐 GEOMETRICI", "green")
        tab.add_row(_v(54,"AVL Tree"),         _v(55,"Segment Tree"),     _v(56,"Fenwick Tree"),    _v(57,"Union-Find"))
        tab.add_row(_v(58,"Convex Hull"),      "","","")
        tab.add_row("","","","")

        sep("🎮 ANIMAZIONI LIVE — auto-animate, Ctrl+C per fermare", "bright_green")
        tab.add_row(_v(59,"Game of Life"),     _v(60,"Labirinto + BFS"),  _v(61,"Sorting Race"),    _v(62,"N-Regine BT"))
        tab.add_row(_v(63,"Dijkstra Griglia"), _v(64,"Rule 30 Wolfram"),  _v(65,"Fuoco Foresta"),   _v(66,"Matrix Rain"))
        tab.add_row(_v(76,"Flood Fill BFS"),   _v(77,"Spiral Matrix"),    _v(79,"Sierpinski"),      "")
        tab.add_row("","","","")

        sep("🔀 ALTRI ORDINAMENTI  |  🌲 BIT / MATH / TRIE", "bright_white")
        tab.add_row(_v(67,"Tim Sort"),         _v(68,"Pancake Sort"),     _v(69,"Quickselect"),     _v(70,"Comb Sort"))
        tab.add_row(_v(71,"Trie Prefisso"),    _v(72,"Deque"),            _v(73,"Bit Manipulation"),_v(74,"Triangolo Pascal"))
        tab.add_row(_v(75,"Monte Carlo Pi"),   _v(78,"Tarjan SCC"),       "","")
        tab.add_row("","","","")

        tab.add_row(_v(0,"Torna al menu principale"), "","","")

        con.print(Panel(
            tab,
            title=(
                f"[bold]Libreria Mondiale degli Algoritmi — 80 algoritmi[/]"
                f"  [dim]| {_etichetta_modo()} | [bold]A[/]=cambia velocità[/]"
            ),
            border_style="blue",
            padding=(1, 2),
        ))

        try:
            scelta_raw = input("\n  ❯ ").strip()
        except Ridimensiona:
            continue

        if scelta_raw == "0":
            break

        # Tasto A → cicla tra le 5 modalità di avanzamento
        if scelta_raw.upper() == "A":
            global _modo_idx
            _modo_idx = (_modo_idx + 1) % len(_VELOCITA_CICLO)
            continue

        try:
            s = int(scelta_raw)
        except ValueError:
            continue

        if s < 1 or s > 80:
            continue

        arr = target = k = None
        _contesto: list[str] = []

        def _setup(prompt: str | None = None) -> None:
            """Ridisegna header Prismalux + pannello configurazione algoritmo."""
            os.system("cls" if sys.platform == "win32" else "clear")
            cols2, _ = shutil.get_terminal_size(fallback=(80, 24))
            c2 = Console(width=cols2)
            stampa_breadcrumb(c2, "Prismalux › 📚 Apprendimento › Simulatore")
            stampa_header(c2)
            nome = _NOMI_ALGO.get(s, f"Algoritmo {s}")
            txt = Text()
            txt.append("  Algoritmo: ", style="dim")
            txt.append(nome + "\n", style="bold cyan")
            if _contesto:
                txt.append("\n")
                for riga in _contesto:
                    txt.append(f"  ✅ {riga}\n", style="green")
            if prompt:
                txt.append("\n")
                txt.append(f"  {prompt}\n", style="white")
            c2.print(Panel(
                txt,
                title="⚙️  Configurazione",
                border_style="cyan",
                padding=(1, 2),
            ))
            c2.print("[dim]  0 = torna al menu  │  Q = esci[/]\n")

        if s in VUOLE_ARRAY:
            _setup("Quanti numeri?  (4–12, invio = 8 casuali)")
            try:
                num = int(input("  ❯ ") or "8")
                num = max(4, min(num, 12))
            except ValueError:
                num = 8
            arr = genera_lista(num)
            _contesto.append(f"Numeri: {num}   →   Lista: {arr}")
            _setup()

        if s in VUOLE_TARGET:
            arr_ord = sorted(arr)
            lista_vis = arr_ord if s in (14, 15, 16) else arr
            label = "Lista ordinata" if s in (14, 15, 16) else "Lista"
            _contesto.append(f"{label}: {lista_vis}")
            _setup("Cerca il numero:")
            try:
                target = int(input("  ❯ "))
            except ValueError:
                target = arr[len(arr) // 2]
            _contesto.append(f"Target: {target}")

        if s in VUOLE_TARGET_TP:
            _contesto.append(f"Lista: {arr}")
            _setup("Target (somma da cercare):")
            try:
                target = int(input("  ❯ ") or str(arr[0] + arr[1]))
            except (ValueError, IndexError):
                target = arr[0] + arr[-1]
            _contesto.append(f"Target somma: {target}")

        if s in VUOLE_K:
            _setup(f"K (dimensione finestra, 2–{len(arr)-1}):")
            try:
                k = int(input("  ❯ ") or "3")
                k = max(2, min(k, len(arr) - 1))
            except ValueError:
                k = 3
            _contesto.append(f"K: {k}")

        if not (s in VUOLE_ARRAY or s in VUOLE_TARGET
                or s in VUOLE_TARGET_TP or s in VUOLE_K):
            _setup("Premi INVIO per avviare la simulazione...")
            raw = input("  ❯ ").strip().upper()
            if raw == "0":
                continue
            if raw == "Q":
                import sys as _sys; _sys.exit(0)

        _log.clear()   # reset cronologia ad ogni nuovo algoritmo
        try:
            match s:
                case 1:  bubble_sort(arr)
                case 2:  cocktail_sort(arr)
                case 3:  selection_sort(arr)
                case 4:  insertion_sort(arr)
                case 5:  shell_sort(arr)
                case 6:  gnome_sort(arr)
                case 7:  counting_sort(arr)
                case 8:  radix_sort(arr)
                case 9:  bucket_sort(arr)
                case 10: merge_sort(arr)
                case 11: quick_sort(arr)
                case 12: heap_sort(arr)
                case 13: ricerca_lineare(arr, target)
                case 14: ricerca_binaria(arr, target)
                case 15: jump_search(arr, target)
                case 16: interpolation_search(arr, target)
                case 17: demo_stack_queue()
                case 18: demo_linked_list()
                case 19: demo_bst()
                case 20: demo_hash_table()
                case 21: demo_min_heap()
                case 22: demo_bfs()
                case 23: demo_dfs()
                case 24: demo_dijkstra()
                case 25: demo_fibonacci()
                case 26: demo_hanoi()
                case 27: demo_sieve()
                case 28: demo_two_pointers(arr, target)
                case 29: demo_sliding_window(arr, k)
                case 30: demo_lru_cache()
                case 31: demo_knapsack()
                case 32: demo_coin_change()
                case 33: demo_lcs()
                case 34: demo_edit_distance()
                case 35: demo_lis()
                case 36: demo_kmp()
                case 37: demo_rabin_karp()
                case 38: demo_z_algorithm()
                case 39: demo_bellman_ford()
                case 40: demo_floyd_warshall()
                case 41: demo_kruskal()
                case 42: demo_prim()
                case 43: demo_astar()
                case 44: demo_topological_sort()
                case 45: demo_activity_selection()
                case 46: demo_huffman()
                case 47: demo_fractional_knapsack()
                case 48: demo_nqueens()
                case 49: demo_sudoku()
                case 50: demo_gcd()
                case 51: demo_modular_exp()
                case 52: demo_miller_rabin()
                case 53: demo_karatsuba()
                case 54: demo_avl_tree()
                case 55: demo_segment_tree()
                case 56: demo_fenwick_tree()
                case 57: demo_union_find()
                case 58: demo_convex_hull()
                case 59: demo_game_of_life()
                case 60: demo_maze_bfs()
                case 61: demo_sorting_race()
                case 62: demo_nqueens_live()
                case 63: demo_dijkstra_griglia()
                case 64: demo_rule30()
                case 65: demo_forest_fire()
                case 66: demo_matrix_rain()
                case 67: tim_sort(arr)
                case 68: pancake_sort(arr)
                case 69: quickselect(arr)
                case 70: comb_sort(arr)
                case 71: demo_trie()
                case 72: demo_deque()
                case 73: demo_bit_manipulation()
                case 74: demo_pascal_triangle()
                case 75: demo_monte_carlo_pi()
                case 76: demo_flood_fill()
                case 77: demo_spiral_matrix()
                case 78: demo_tarjan_scc()
                case 79: demo_sierpinski()
                case 80: demo_rubik()

        except KeyboardInterrupt:
            console.print("\n\n  Simulazione interrotta.")
            input("  [INVIO per tornare al menu] ")


if __name__ == "__main__":
    try:
        menu_simulatore()
    except KeyboardInterrupt:
        console.print("\n\n  🍺 Alla prossima libagione di sapere.\n")
