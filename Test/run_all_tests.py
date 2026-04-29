#!/usr/bin/env python3
"""
run_all_tests.py — Lancia tutta la suite di test Prismalux.

Esegui:
  python3 run_all_tests.py
  python3 run_all_tests.py --quick      # solo test veloci (skip math+rag)
  python3 run_all_tests.py --only coding honesty
  python3 run_all_tests.py --model mistral:7b-instruct
"""
import argparse
import subprocess
import sys
import time
import os

DIR = os.path.dirname(os.path.abspath(__file__))

SUITES = [
    {
        "key":    "coding",
        "script": "test_coding_mistral.py",
        "desc":   "Coding Python (write/debug/explain/refact)",
        "args":   [],
        "quick":  False,
    },
    {
        "key":    "honesty",
        "script": "test_brutal_honesty.py",
        "desc":   "Brutal Honesty (math/trap/factual/false_premise)",
        "args":   ["--quick"],
        "quick":  False,
    },
    {
        "key":    "levels",
        "script": "test_prompt_levels.py",
        "desc":   "Livelli prompt utente (L1–L4)",
        "args":   [],
        "quick":  True,   # più lento, skip con --quick
    },
    {
        "key":    "math",
        "script": "test_math_column_shift.py",
        "desc":   "Matematica complessa (shift/matrici/accumulatori)",
        "args":   [],
        "quick":  True,   # lento, skip con --quick
    },
    {
        "key":    "rag",
        "script": "test_rag_paper.py",
        "desc":   "RAG — Johnson-Lindenstrauss paper",
        "args":   [],
        "quick":  True,   # richiede PDF, skip con --quick
    },
]


def run_suite(suite: dict, model: str | None) -> tuple[int, float]:
    """Esegue un singolo script. Ritorna (exit_code, secondi)."""
    cmd = [sys.executable, os.path.join(DIR, suite["script"])] + suite["args"]
    if model and suite["key"] in ("coding", "levels"):
        cmd += ["--model", model]
    t0 = time.time()
    ret = subprocess.run(cmd, cwd=DIR)
    return ret.returncode, time.time() - t0


def main() -> None:
    ap = argparse.ArgumentParser(description="Runner suite test Prismalux")
    ap.add_argument("--quick", "-q", action="store_true",
                    help="Salta i test lenti (math, rag, levels)")
    ap.add_argument("--only", nargs="+",
                    choices=[s["key"] for s in SUITES],
                    help="Esegui solo le suite specificate")
    ap.add_argument("--model", default=None,
                    help="Modello Ollama (override per i test che lo supportano)")
    args = ap.parse_args()

    suites = SUITES
    if args.only:
        suites = [s for s in suites if s["key"] in args.only]
    elif args.quick:
        suites = [s for s in suites if not s["quick"]]

    model_str = f" [{args.model}]" if args.model else ""
    print(f"\n{'═'*70}")
    print(f"  🚀  Prismalux Test Runner{model_str}")
    print(f"  Suite: {len(suites)} — " + ", ".join(s["key"] for s in suites))
    print(f"{'═'*70}\n")

    results = []
    for suite in suites:
        print(f"\n{'─'*70}")
        print(f"  ▶  {suite['desc']}")
        print(f"{'─'*70}")
        code, dt = run_suite(suite, args.model)
        results.append((suite["key"], suite["desc"], code, dt))

    # Riepilogo finale
    print(f"\n{'═'*70}")
    print(f"  📋  RIEPILOGO RUNNER")
    print(f"{'═'*70}")
    all_ok = True
    for key, desc, code, dt in results:
        status = "✅" if code == 0 else f"❌ (exit {code})"
        print(f"  {status}  {dt:5.0f}s  {desc}")
        if code != 0:
            all_ok = False

    total = sum(dt for _, _, _, dt in results)
    print(f"\n  Tempo totale: {total:.0f}s  "
          f"({'TUTTO OK' if all_ok else 'QUALCHE TEST FALLITO'})")
    print(f"{'═'*70}\n")

    sys.exit(0 if all_ok else 1)


if __name__ == "__main__":
    main()
