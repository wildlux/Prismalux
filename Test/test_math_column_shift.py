#!/usr/bin/env python3
"""
test_math_column_shift.py — Test matematico complesso per modelli LLM.
Spostamento variabili, somme di colonne, pipeline a catena, accumulatori.

Ogni problema ha una risposta ESATTA verificabile con numpy/Python.
Il modello deve calcolare i valori numerici senza scrivere codice.

Esegui: python3 test_math_column_shift.py
"""
import time
import numpy as np
from test_utils import load_params, ask_ollama, bar, PARAMS_FILE

MODEL   = "qwen2.5-coder:7b"
TIMEOUT = 90

SYSTEM = (
    "Sei un assistente matematico preciso. Calcola ogni valore passo per passo. "
    "La risposta finale deve essere un numero, chiaramente indicato come 'Risposta: <numero>'. "
    "Non scrivere codice Python. Rispondi in italiano."
)


# ── Risposte attese (verificate localmente con numpy) ──────────
def _p1() -> int:
    A = np.array([[3,7,2],[5,1,8],[4,6,3]])
    return int(np.roll(A, 1, axis=1)[:, 1].sum())

def _p2() -> int:
    A = np.array([[1,2,3,4],[5,6,7,8],[9,10,11,12]])
    B = np.roll(A, 2, axis=0)
    return int((B[:,0] + B[:,3]).sum())

def _p3() -> int:
    a, b, c = 15, 8, 3
    a, b    = b, a
    c       = a + b
    a       = b * c - a
    b       = (c - a) % 7
    return a + b + c

def _p4() -> int:
    v = [1,2,3,4,5]
    v = v[-2:] + v[:-2]
    v = [v[i]+v[i+1] for i in range(len(v)-1)]
    v = v[1:] + v[:1]
    return sum(v)

def _p5() -> int:
    M = np.array([[2,4,6,8],[1,3,5,7],[9,0,1,2]])
    M = np.roll(M, -1, axis=1)
    M[1] = M[0] + M[2]
    return int(M.sum())

def _p6() -> int:
    dati = [3,7,2,9,4,6,1,8,5]
    acc  = sum(x if i%2==0 else -x for i,x in enumerate(dati))
    return acc * 2 + len(dati)

def _p7() -> int:
    r0 = np.array([1,2,3,4,5])
    r1 = np.array([6,7,8,9,10])
    for k in range(1,5):
        r0 = np.roll(r0, k) + r1
    return int(r0[2])

def _p8() -> int:
    a, b, c, d = 12, 5, 3, 7
    a = a*b + c
    b = a - d*c
    c = (a+b) // d
    d = a%c + b%c
    a, d = d, a
    b, c = c, b
    return a + b + c + d


# ── Definizione dei problemi ───────────────────────────────────
TESTS = [
    (1,
     "Matrice A 3×3:\n"
     "A = [[3,7,2],[5,1,8],[4,6,3]]\n"
     "Shift circolare DESTRA di 1 sulle colonne (roll axis=1, shift=+1). "
     "Calcola la somma della colonna 1 del risultato.",
     _p1()),
    (2,
     "Matrice A 3×4:\n"
     "A = [[1,2,3,4],[5,6,7,8],[9,10,11,12]]\n"
     "Shift circolare GIÙ di 2 sulle righe (roll axis=0, shift=+2). "
     "Per ogni riga calcola col_0 + col_3, poi somma i 3 valori.",
     _p2()),
    (3,
     "a=15, b=8, c=3\n"
     "Step 1: scambia a↔b\n"
     "Step 2: c = a + b\n"
     "Step 3: a = b × c − a\n"
     "Step 4: b = (c − a) mod 7\n"
     "Calcola a + b + c.",
     _p3()),
    (4,
     "v = [1,2,3,4,5]\n"
     "Step 1: shift circolare DESTRA di 2 (v = v[-2:]+v[:-2])\n"
     "Step 2: somma adiacenti → v[i]+v[i+1] per i=0..3 (4 elementi)\n"
     "Step 3: shift circolare SINISTRA di 1 (v = v[1:]+v[:1])\n"
     "Step 4: somma tutti gli elementi.",
     _p4()),
    (5,
     "M 3×4 = [[2,4,6,8],[1,3,5,7],[9,0,1,2]]\n"
     "1. Shift circolare colonne SINISTRA di 1 (roll axis=1, shift=−1)\n"
     "2. R = somma element-wise riga 0 e riga 2\n"
     "3. Sostituisci riga 1 con R\n"
     "4. Calcola la somma totale di tutti gli elementi.",
     _p5()),
    (6,
     "dati = [3,7,2,9,4,6,1,8,5]\n"
     "acc = 0\n"
     "Per i=0..8: se i pari → acc+=dati[i], se i dispari → acc−=dati[i]\n"
     "Poi: acc = acc × 2 + 9\n"
     "Calcola acc.",
     _p6()),
    (7,
     "row0=[1,2,3,4,5]  row1=[6,7,8,9,10]\n"
     "Per k=1,2,3,4:\n"
     "  1. shift circolare DESTRA di k posizioni su row0\n"
     "  2. row0 = row0 + row1 (element-wise)\n"
     "Dopo 4 iterazioni, qual è row0[2] (terzo elemento)?",
     _p7()),
    (8,
     "a=12, b=5, c=3, d=7\n"
     "Step 1: a = a×b + c\n"
     "Step 2: b = a − d×c\n"
     "Step 3: c = (a+b) // d  (divisione intera)\n"
     "Step 4: d = (a mod c) + (b mod c)\n"
     "Step 5: scambia a↔d, poi scambia b↔c\n"
     "Calcola a + b + c + d.",
     _p8()),
]


# ── Verifica risposta ──────────────────────────────────────────
def _check(response: str, expected: int) -> bool:
    exp = str(expected)
    clean = response.replace(",", ".").replace(" ", "")
    return exp in response or exp in clean


# ── Divide et impera: funzioni UI ─────────────────────────────
def _print_header(params: dict) -> None:
    print(f"\n🔢 TEST MATEMATICO COMPLESSO — Spostamento Variabili & Somme Colonne")
    print(f"   Modello: {MODEL}  |  {len(TESTS)} problemi")
    print(f"   Params : {PARAMS_FILE}")
    print(f"   temp={params.get('temperature',0.1)}  "
          f"top_k={params.get('top_k',40)}  "
          f"repeat={params.get('repeat_penalty',1.1)}")
    print("─" * 60)
    print("📐 Risposte attese (calcolate localmente):")
    for tid, _, expected in TESTS:
        print(f"   P{tid}: {expected}")
    print("─" * 60)


def _run_one(tid: int, question: str, expected: int,
             params: dict) -> tuple[bool, float]:
    """Interroga il modello su un problema. Ritorna (corretto, secondi)."""
    preview = question.split('\n')[0][:55]
    print(f"\n🧮 P{tid}/{len(TESTS)}: {preview}...")
    t0  = time.time()
    ans = ask_ollama(question, SYSTEM, MODEL, params, TIMEOUT)
    dt  = time.time() - t0
    ok  = _check(ans, expected)
    print(f"   {'✅' if ok else '❌'}  Atteso: {expected:>6}  |  {dt:.1f}s")
    if not ok:
        for line in [l.strip() for l in ans.splitlines() if l.strip()][-3:]:
            print(f"      > {line[:80]}")
    return ok, dt


def _print_summary(results: list[tuple[bool, float]]) -> None:
    n       = len(TESTS)
    correct = sum(ok for ok, _ in results)
    pct     = correct / n * 100

    print("\n" + "─" * 60)
    print(f"\n📊 RISULTATI  {MODEL}")
    print(f"   Corretti : {correct}/{n}  {bar(correct,n)}  {pct:.0f}%\n")
    for i, (ok, dt) in enumerate(results):
        tid = TESTS[i][0]
        exp = TESTS[i][2]
        print(f"   P{tid:02d}  {'✅' if ok else '❌'}  atteso={exp:<8} {dt:.1f}s")

    print("\n📌 Interpretazione:")
    if pct >= 87:   print("   🥇 Eccellente — catene di operazioni complesse gestite.")
    elif pct >= 62: print("   🥈 Buono — qualche errore su shift multi-iterazione.")
    elif pct >= 37: print("   🥉 Sufficiente — difficoltà con operazioni concatenate.")
    else:           print("   ⚠️  Insufficiente — il modello non traccia variabili multiple.")
    print()


# ── Orchestratore ──────────────────────────────────────────────
def main() -> None:
    params = load_params({"model": MODEL})
    _print_header(params)

    results = [_run_one(tid, q, exp, params) for tid, q, exp in TESTS]
    _print_summary(results)


if __name__ == "__main__":
    main()
