#!/usr/bin/env python3
"""
test_coding_mistral.py — Suite di test coding per mistral:7b-instruct.

Categorie:
  WRITE   — scrivi la funzione (valutata con exec + assert)
  DEBUG   — trova e correggi il bug (valutata con exec + assert)
  EXPLAIN — spiega il codice (valutata con keyword coverage)
  REFACT  — refactoring (valutata con keyword + exec se applicabile)

Esegui:
  python3 test_coding_mistral.py
  python3 test_coding_mistral.py --verbose
  python3 test_coding_mistral.py --save
  python3 test_coding_mistral.py --only write
  python3 test_coding_mistral.py --model qwen3:4b
"""
import argparse
import json
import os
import re
import sys
import textwrap
import time
from datetime import datetime

sys.path.insert(0, os.path.dirname(__file__))
from test_utils import ask_ollama, bar

MODEL   = "mistral:7b-instruct"
TIMEOUT = 120

SYSTEM = (
    "Sei un assistente esperto di programmazione Python. "
    "Rispondi SEMPRE in italiano. "
    "Quando scrivi codice Python: usa SOLO blocchi ```python ... ```. "
    "Non aggiungere testo superfluo fuori dal blocco codice a meno che non sia richiesto. "
    "Il codice deve essere corretto, eseguibile e senza import non necessari."
)

PARAMS = {
    "temperature": 0.2,
    "top_p": 0.9,
    "top_k": 40,
    "repeat_penalty": 1.05,
    "num_predict": 1024,
    "num_ctx": 4096,
}

# ══════════════════════════════════════════════════════════════
#  Suite di test
#  (id, categoria, prompt, assert_code, keywords, note)
#  assert_code: stringa Python eseguita dopo exec(codice_estratto)
#               None se la valutazione è solo keyword-based
# ══════════════════════════════════════════════════════════════
TESTS = [

    # ── WRITE: scrivi funzione ──────────────────────────────────
    ("W1", "write",
     "Scrivi una funzione Python chiamata `fibonacci(n)` che restituisce "
     "il numero di Fibonacci all'indice n (0-indexed). "
     "fibonacci(0)=0, fibonacci(1)=1. Usa la ricorsione con memoization.",
     textwrap.dedent("""\
         assert fibonacci(0) == 0, "fib(0)"
         assert fibonacci(1) == 1, "fib(1)"
         assert fibonacci(10) == 55, "fib(10)"
         assert fibonacci(20) == 6765, "fib(20)"
     """),
     ["def fibonacci", "memo", "cache"],
     "Ricorsione con memoization. fibonacci(10)=55, fibonacci(20)=6765."),

    ("W2", "write",
     "Scrivi una funzione Python `is_balanced(s: str) -> bool` che verifica "
     "se una stringa contiene parentesi bilanciate. "
     "Tipi accettati: () [] {}. "
     "Esempio: is_balanced('([{}])') → True, is_balanced('([)]') → False.",
     textwrap.dedent("""\
         assert is_balanced("([{}])") == True
         assert is_balanced("([)]") == False
         assert is_balanced("") == True
         assert is_balanced("{[}") == False
         assert is_balanced("((()))") == True
     """),
     ["def is_balanced", "stack", "append", "pop"],
     "Stack per parentesi. is_balanced('([{}])') → True."),

    ("W3", "write",
     "Scrivi una funzione Python `merge_sorted(a: list, b: list) -> list` "
     "che unisce due liste ordinate in una lista ordinata. "
     "Complessità O(n+m). Non usare sorted().",
     textwrap.dedent("""\
         assert merge_sorted([1,3,5], [2,4,6]) == [1,2,3,4,5,6]
         assert merge_sorted([], [1,2]) == [1,2]
         assert merge_sorted([1], []) == [1]
         assert merge_sorted([1,1,2], [1,3]) == [1,1,1,2,3]
     """),
     ["def merge_sorted", "while", "append"],
     "Merge O(n+m) senza sorted(). merge_sorted([1,3,5],[2,4,6])=[1,2,3,4,5,6]."),

    ("W4", "write",
     "Scrivi una funzione Python `count_words(text: str) -> dict` che conta "
     "le occorrenze di ogni parola in una stringa (case-insensitive, "
     "ignora punteggiatura). "
     "Esempio: count_words('Ciao ciao mondo') → {'ciao': 2, 'mondo': 1}.",
     textwrap.dedent("""\
         r = count_words("Ciao ciao mondo")
         assert r.get("ciao") == 2
         assert r.get("mondo") == 1
         r2 = count_words("Hello, hello! World.")
         assert r2.get("hello") == 2
         assert r2.get("world") == 1
     """),
     ["def count_words", "lower", "split"],
     "count_words case-insensitive, ignora punteggiatura."),

    ("W5", "write",
     "Scrivi una funzione Python `rotate_matrix(matrix: list[list[int]]) -> list[list[int]]` "
     "che ruota una matrice NxN di 90° in senso orario. "
     "Esempio: [[1,2],[3,4]] → [[3,1],[4,2]].",
     textwrap.dedent("""\
         assert rotate_matrix([[1,2],[3,4]]) == [[3,1],[4,2]]
         assert rotate_matrix([[1,2,3],[4,5,6],[7,8,9]]) == [[7,4,1],[8,5,2],[9,6,3]]
     """),
     ["def rotate_matrix", "zip", "reversed"],
     "Rotazione 90° orario. [[1,2],[3,4]] → [[3,1],[4,2]]."),

    # ── DEBUG: trova e correggi il bug ──────────────────────────
    ("D1", "debug",
     "Il seguente codice Python ha un bug. Trovalo e correggi la funzione "
     "in modo che i test passino:\n\n"
     "```python\n"
     "def binary_search(arr, target):\n"
     "    lo, hi = 0, len(arr)\n"
     "    while lo < hi:\n"
     "        mid = (lo + hi) // 2\n"
     "        if arr[mid] == target:\n"
     "            return mid\n"
     "        elif arr[mid] < target:\n"
     "            lo = mid\n"
     "        else:\n"
     "            hi = mid\n"
     "    return -1\n"
     "```\n\n"
     "Test attesi:\n"
     "binary_search([1,3,5,7,9], 5) == 2\n"
     "binary_search([1,3,5,7,9], 1) == 0\n"
     "binary_search([1,3,5,7,9], 9) == 4\n"
     "binary_search([1,3,5,7,9], 4) == -1",
     textwrap.dedent("""\
         assert binary_search([1,3,5,7,9], 5) == 2, "trovato centro"
         assert binary_search([1,3,5,7,9], 1) == 0, "primo elemento"
         assert binary_search([1,3,5,7,9], 9) == 4, "ultimo elemento"
         assert binary_search([1,3,5,7,9], 4) == -1, "non presente"
     """),
     ["lo = mid + 1", "hi = mid", "def binary_search"],
     "Bug: lo = mid + 1 invece di lo = mid (loop infinito su len-1 fix)."),

    ("D2", "debug",
     "Il seguente codice Python ha un bug. Trovalo e correggi:\n\n"
     "```python\n"
     "def flatten(lst):\n"
     "    result = []\n"
     "    for item in lst:\n"
     "        if isinstance(item, list):\n"
     "            result.extend(flatten(item))\n"
     "        else:\n"
     "            result.append(item)\n"
     "    return result\n"
     "\n"
     "def deep_sum(lst):\n"
     "    return sum(flatten(lst))\n"
     "\n"
     "# Bug qui:\n"
     "def running_average(numbers):\n"
     "    total = 0\n"
     "    avgs = []\n"
     "    for i, n in enumerate(numbers):\n"
     "        total += n\n"
     "        avgs.append(total / i)  # BUG\n"
     "    return avgs\n"
     "```\n\n"
     "Test: running_average([4, 2, 6]) == [4.0, 3.0, 4.0]",
     textwrap.dedent("""\
         assert running_average([4, 2, 6]) == [4.0, 3.0, 4.0], "media progressiva"
         assert running_average([10]) == [10.0], "singolo"
         assert running_average([1, 1, 1, 1]) == [1.0, 1.0, 1.0, 1.0], "costante"
     """),
     ["i + 1", "def running_average", "total"],
     "Bug: total / i → ZeroDivisionError al primo, fix: total / (i+1)."),

    ("D3", "debug",
     "Il seguente codice Python ha un bug silenzioso (non dà errore ma "
     "produce risultati sbagliati). Trovalo e correggi:\n\n"
     "```python\n"
     "def remove_duplicates(lst):\n"
     "    seen = set()\n"
     "    result = []\n"
     "    for item in lst:\n"
     "        if item not in seen:\n"
     "            result.append(item)\n"
     "        seen.add(item)  # BUG: indentazione sbagliata\n"
     "    return result\n"
     "```\n\n"
     "Test: remove_duplicates([1,2,1,3,2,4]) == [1,2,3,4]",
     textwrap.dedent("""\
         assert remove_duplicates([1,2,1,3,2,4]) == [1,2,3,4]
         assert remove_duplicates([]) == []
         assert remove_duplicates([1,1,1]) == [1]
     """),
     ["seen.add", "def remove_duplicates"],
     "Bug: seen.add dentro il blocco else (indentazione) oppure fuori — deve essere dopo l'if."),

    # ── EXPLAIN: spiega il codice ────────────────────────────────
    ("E1", "explain",
     "Spiega in italiano cosa fa il seguente codice Python e qual è la "
     "sua complessità temporale:\n\n"
     "```python\n"
     "def mystery(arr):\n"
     "    n = len(arr)\n"
     "    for i in range(n):\n"
     "        min_idx = i\n"
     "        for j in range(i+1, n):\n"
     "            if arr[j] < arr[min_idx]:\n"
     "                min_idx = j\n"
     "        arr[i], arr[min_idx] = arr[min_idx], arr[i]\n"
     "    return arr\n"
     "```",
     None,
     ["selection sort", "O(n²)", "minimo", "ordinament", "scamb"],
     "Selection sort O(n²). Deve identificare l'algoritmo e la complessità."),

    ("E2", "explain",
     "Spiega cosa fa questo generatore Python e perché è più efficiente "
     "di una lista equivalente:\n\n"
     "```python\n"
     "def primes():\n"
     "    sieve = {}\n"
     "    n = 2\n"
     "    while True:\n"
     "        if n not in sieve:\n"
     "            yield n\n"
     "            sieve[n*n] = [n]\n"
     "        else:\n"
     "            for p in sieve[n]:\n"
     "                sieve.setdefault(n+p, []).append(p)\n"
     "            del sieve[n]\n"
     "        n += 1\n"
     "```",
     None,
     ["generatore", "primo", "crivello", "memoria", "yield", "infinit"],
     "Sieve of Eratosthenes come generatore infinito. Deve menzionare memoria e yield."),

    # ── REFACT: refactoring ──────────────────────────────────────
    ("R1", "refact",
     "Riscrivi questa funzione usando list comprehension e il modulo "
     "`collections.Counter` per renderla più Pythonica:\n\n"
     "```python\n"
     "def top_n_words(text, n):\n"
     "    words = text.lower().split()\n"
     "    counts = {}\n"
     "    for word in words:\n"
     "        if word in counts:\n"
     "            counts[word] += 1\n"
     "        else:\n"
     "            counts[word] = 1\n"
     "    sorted_items = []\n"
     "    for word, count in counts.items():\n"
     "        sorted_items.append((count, word))\n"
     "    sorted_items.sort(reverse=True)\n"
     "    result = []\n"
     "    for i in range(min(n, len(sorted_items))):\n"
     "        result.append(sorted_items[i][1])\n"
     "    return result\n"
     "```",
     textwrap.dedent("""\
         r = top_n_words("il gatto il cane il gatto cane", 2)
         assert r[0] == "il", f"primo={r[0]}"
         assert "gatto" in r or "cane" in r, f"secondo={r}"
     """),
     ["Counter", "most_common", "def top_n_words"],
     "Usa Counter e most_common(). Risultato identico all'originale."),

    ("R2", "refact",
     "Riscrivi questa classe Python seguendo i principi SOLID, "
     "eliminando la duplicazione e aggiungendo type hints:\n\n"
     "```python\n"
     "class Stats:\n"
     "    def media(self, dati):\n"
     "        tot = 0\n"
     "        for x in dati: tot += x\n"
     "        return tot / len(dati)\n"
     "\n"
     "    def varianza(self, dati):\n"
     "        tot = 0\n"
     "        for x in dati: tot += x\n"
     "        m = tot / len(dati)\n"
     "        var = 0\n"
     "        for x in dati: var += (x - m) ** 2\n"
     "        return var / len(dati)\n"
     "\n"
     "    def deviazione(self, dati):\n"
     "        tot = 0\n"
     "        for x in dati: tot += x\n"
     "        m = tot / len(dati)\n"
     "        var = 0\n"
     "        for x in dati: var += (x - m) ** 2\n"
     "        return (var / len(dati)) ** 0.5\n"
     "```",
     textwrap.dedent("""\
         s = Stats()
         dati = [2, 4, 4, 4, 5, 5, 7, 9]
         m = s.media(dati)
         assert abs(m - 5.0) < 0.01, f"media={m}"
         v = s.varianza(dati)
         assert abs(v - 4.0) < 0.01, f"varianza={v}"
         d = s.deviazione(dati)
         assert abs(d - 2.0) < 0.01, f"deviazione={d}"
     """),
     ["def media", "def varianza", "def deviazione", "list", "float"],
     "Elimina duplicazione calcolo media. Type hints. Risultati corretti."),
]


# ══════════════════════════════════════════════════════════════
#  Estrae il primo blocco ```python ... ``` dalla risposta
# ══════════════════════════════════════════════════════════════
def extract_code(response: str) -> str:
    m = re.search(r"```python\s*(.*?)```", response, re.DOTALL)
    if m:
        return m.group(1).strip()
    # fallback: cerca blocco generico
    m = re.search(r"```\s*(.*?)```", response, re.DOTALL)
    if m:
        return m.group(1).strip()
    return ""


# ══════════════════════════════════════════════════════════════
#  Valutazione: esegue il codice + assert
# ══════════════════════════════════════════════════════════════
def eval_code(code: str, assert_code: str) -> tuple[bool, str]:
    """Esegue code + assert_code in un namespace isolato. Ritorna (ok, messaggio)."""
    if not code:
        return False, "nessun blocco ```python``` trovato nella risposta"
    ns: dict = {}
    try:
        exec(code, ns)
    except Exception as e:
        return False, f"errore exec codice: {e}"
    try:
        exec(assert_code, ns)
        return True, "tutti gli assert passati"
    except AssertionError as e:
        return False, f"assert fallito: {e}"
    except Exception as e:
        return False, f"errore negli assert: {e}"


def eval_keywords(response: str, keywords: list[str]) -> tuple[float, int]:
    """Conta quante keyword (case-insensitive) appaiono nella risposta."""
    lo = response.lower()
    hits = sum(1 for kw in keywords if kw.lower() in lo)
    return hits / len(keywords) if keywords else 1.0, hits


# ══════════════════════════════════════════════════════════════
#  Stampa
# ══════════════════════════════════════════════════════════════
CAT_ICON = {"write": "✍️", "debug": "🐛", "explain": "🔍", "refact": "♻️"}
CAT_NAME = {"write": "WRITE", "debug": "DEBUG", "explain": "EXPLAIN", "refact": "REFACT"}

def _print_header(model: str, tests: list) -> None:
    cats = {t[1] for t in tests}
    print(f"\n{'═'*70}")
    print(f"  🤖  Test Coding — {model}")
    print(f"{'═'*70}")
    print(f"  Test  : {len(tests)} casi")
    cats_str = "  ".join(f"{CAT_ICON[c]} {CAT_NAME[c]}" for c in sorted(cats))
    print(f"  Tipi  : {cats_str}")
    print(f"  Params: temp={PARAMS['temperature']}  top_k={PARAMS['top_k']}  "
          f"num_predict={PARAMS['num_predict']}")
    print(f"{'═'*70}\n")


def _run_one(test: tuple, model: str, verbose: bool) -> dict:
    tid, cat, prompt, assert_code, keywords, note = test
    icon = CAT_ICON.get(cat, "?")
    print(f"  {icon} [{tid}] {CAT_NAME[cat]}: {note[:65]}")

    t0 = time.time()
    resp = ask_ollama(prompt, SYSTEM, model, PARAMS, TIMEOUT)
    dt = time.time() - t0

    if resp.startswith("[ERRORE"):
        print(f"  ❌ Errore Ollama: {resp[:80]}\n")
        return {"id": tid, "cat": cat, "ok": False, "dt": dt, "reason": resp[:80]}

    # Valutazione
    if assert_code is not None:
        code = extract_code(resp)
        ok, reason = eval_code(code, assert_code)
        kw_cov, kw_hits = eval_keywords(resp, keywords)
    else:
        ok, reason = False, ""
        kw_cov, kw_hits = eval_keywords(resp, keywords)
        ok = kw_cov >= 0.5   # explain: ok se ≥50% keyword

    kw_bar = bar(kw_hits, len(keywords), 10)
    status = "✅" if ok else "❌"
    print(f"  {status}  {dt:.1f}s  kw [{kw_bar}] {kw_hits}/{len(keywords)}  {reason}")

    if verbose or not ok:
        code_block = extract_code(resp)
        if code_block:
            print("     ┌─ codice estratto " + "─"*40)
            for line in code_block.splitlines()[:25]:
                print(f"     │ {line}")
            if len(code_block.splitlines()) > 25:
                print(f"     │ ... [{len(code_block.splitlines())-25} righe omesse]")
            print("     └" + "─"*50)
        elif not assert_code:
            print("     ┌─ risposta " + "─"*48)
            for line in resp[:400].splitlines():
                print(f"     │ {line}")
            print("     └" + "─"*50)
    print()

    return {
        "id": tid, "cat": cat, "ok": ok, "dt": round(dt, 2),
        "kw_cov": round(kw_cov, 2), "reason": reason,
    }


# ══════════════════════════════════════════════════════════════
#  Riepilogo
# ══════════════════════════════════════════════════════════════
def _print_summary(model: str, records: list[dict]) -> None:
    n = len(records)
    ok_total = sum(1 for r in records if r["ok"])
    pct = ok_total / n * 100 if n else 0

    print(f"{'═'*70}")
    print(f"  📊  RIEPILOGO  {model}")
    print(f"{'═'*70}")
    print(f"  Totale   : {ok_total}/{n}  {bar(ok_total, n)}  {pct:.0f}%\n")

    cats = sorted({r["cat"] for r in records})
    for cat in cats:
        recs = [r for r in records if r["cat"] == cat]
        c_ok = sum(1 for r in recs if r["ok"])
        avg_dt = sum(r["dt"] for r in recs) / len(recs)
        icon = CAT_ICON[cat]
        print(f"  {icon} {CAT_NAME[cat]:8} {c_ok}/{len(recs)}  "
              f"{bar(c_ok, len(recs), 10)}  {avg_dt:.1f}s/test")

    print()
    for r in records:
        icon = CAT_ICON[r["cat"]]
        status = "✅" if r["ok"] else "❌"
        print(f"  [{r['id']:3}] {icon} {status}  {r['dt']:.1f}s  kw={r.get('kw_cov',0):.0%}  {r['reason'][:45]}")

    print()
    if pct >= 80:
        print("  🥇 Eccellente — il modello scrive e debugga bene Python.")
    elif pct >= 60:
        print("  🥈 Buono — qualche difficoltà su casi edge o refactoring.")
    elif pct >= 40:
        print("  🥉 Sufficiente — difficoltà su debug o compiti complessi.")
    else:
        print("  ⚠️  Insufficiente — il modello non è affidabile per coding.")
    print(f"{'═'*70}\n")


# ══════════════════════════════════════════════════════════════
#  Salvataggio JSON
# ══════════════════════════════════════════════════════════════
def _save(model: str, records: list[dict]) -> None:
    path = os.path.expanduser("~/.prismalux/coding_results.json")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    history = []
    if os.path.exists(path):
        try:
            with open(path) as f:
                history = json.load(f)
        except Exception:
            pass
    history.append({
        "timestamp": datetime.now().isoformat(),
        "model": model,
        "n_tests": len(records),
        "score": sum(1 for r in records if r["ok"]),
        "results": records,
    })
    with open(path, "w") as f:
        json.dump(history, f, indent=2, ensure_ascii=False)
    print(f"  💾  Salvato → {path}  ({len(history)} sessioni totali)\n")


# ══════════════════════════════════════════════════════════════
#  Main
# ══════════════════════════════════════════════════════════════
def main() -> None:
    ap = argparse.ArgumentParser(description="Test coding per modelli LLM")
    ap.add_argument("--model", default=MODEL, help=f"Modello Ollama (default: {MODEL})")
    ap.add_argument("--verbose", "-v", action="store_true",
                    help="Mostra codice estratto per tutti i test (non solo i falliti)")
    ap.add_argument("--save", "-s", action="store_true",
                    help="Salva risultati in ~/.prismalux/coding_results.json")
    ap.add_argument("--only", choices=["write", "debug", "explain", "refact"],
                    help="Esegui solo una categoria")
    args = ap.parse_args()

    model  = args.model
    tests  = [t for t in TESTS if args.only is None or t[1] == args.only]

    _print_header(model, tests)

    records = [_run_one(t, model, args.verbose) for t in tests]

    _print_summary(model, records)

    if args.save:
        _save(model, records)

    n_ok = sum(1 for r in records if r["ok"])
    sys.exit(0 if n_ok == len(records) else 1)


if __name__ == "__main__":
    main()
