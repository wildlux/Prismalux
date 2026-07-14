#!/usr/bin/env python3
# =============================================================================
# ⚠️  STRUMENTO DIDATTICO — esegue in sequenza i moduli demo di sds_editing.
# =============================================================================
"""
run_all.py - Esegue tutti i moduli in sequenza
"""

import subprocess
import sys
import os


def run_script(name):
    print(f"\n== Esecuzione di {name} ==")
    result = subprocess.run([sys.executable, name], capture_output=True, text=True)
    print(result.stdout)
    if result.stderr:
        print("stderr:", result.stderr)
    return result.returncode == 0


def main():
    scripts = [
        "sds_analyzer.py",
        "bruteforce_sds_stable.py",
        "hybrid_optimizer.py",
        "llm_meta_analyst.py",
    ]
    for script in scripts:
        if not os.path.exists(script):
            print(f"[!] {script} non trovato, salto.")
            continue
        if not run_script(script):
            print(f"[!] {script} ha fallito, interrompo la catena.")
            break
    print("\nCompletato.")


if __name__ == "__main__":
    main()
