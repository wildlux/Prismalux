#!/usr/bin/env python3
# =============================================================================
# ⚠️  STRUMENTO DIDATTICO — NON PRODUCE RISULTATI CLINICI
# L'LLM (Ollama locale) commenta STATISTICHE aggregate: puo' suggerire spunti
# di lettura, NON scoperte biologiche. Le sue ipotesi vanno sempre verificate
# con strumenti validati e ricercatori. Nessun dato esce dal PC (LLM in locale).
# =============================================================================
"""
llm_meta_analyst.py - Usa Ollama (locale) per commentare i pattern del brute force
"""

import json
import csv
import subprocess
import os
import sys
from collections import Counter, defaultdict


def load_data(checkpoint_file="checkpoint.json", results_file="best_guides.csv"):
    if not os.path.exists(checkpoint_file):
        print(f"[x] Checkpoint {checkpoint_file} non trovato.")
        return None
    with open(checkpoint_file, 'r') as f:
        cp = json.load(f)
    best = []
    if os.path.exists(results_file):
        with open(results_file, 'r') as f:
            reader = csv.DictReader(f)
            best = list(reader)
    return {'checkpoint': cp, 'best_guides': best}


def extract_patterns(data):
    best = data['best_guides']
    if not best:
        return None
    features = []
    for g in best[:20]:
        seq = g.get('Sequenza', '')
        try:
            score = float(g.get('Total Score', 0))
            gc = float(g.get('GC%', 0))
            tm = float(g.get('TM', 0))
            features.append({'seq': seq, 'score': score, 'gc': gc, 'tm': tm})
        except (ValueError, TypeError):
            continue
    if not features:
        return None
    pos_counts = defaultdict(Counter)
    for f in features:
        for i, base in enumerate(f['seq']):
            pos_counts[i][base] += 1
    return {
        'num_top': len(features),
        'avg_score': sum(f['score'] for f in features) / len(features),
        'avg_gc': sum(f['gc'] for f in features) / len(features),
        'avg_tm': sum(f['tm'] for f in features) / len(features),
        'position_consensus': {str(k): dict(v) for k, v in pos_counts.items()},
        'sequences': [f['seq'] for f in features]
    }


def ask_llm(prompt, model="mistral"):
    try:
        cmd = ["ollama", "run", model]
        proc = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE, text=True)
        output, err = proc.communicate(prompt, timeout=120)
        if proc.returncode != 0:
            print(f"[x] Errore Ollama: {err}")
            return None
        return output.strip()
    except FileNotFoundError:
        print("[x] Ollama non installato. Installa con: curl -fsSL https://ollama.com/install.sh | sh")
        return None
    except subprocess.TimeoutExpired:
        print("[x] Tempo scaduto. Modello troppo lento per la CPU.")
        return None


def build_prompt(patterns):
    top_seqs = "\n".join([f"- {s}" for s in patterns['sequences'][:10]])
    stats = f"""
- Numero guide top: {patterns['num_top']}
- Punteggio medio: {patterns['avg_score']:.3f}
- GC medio: {patterns['avg_gc']:.1f}%
- TM medio: {patterns['avg_tm']:.1f}
- Consenso posizioni: {json.dumps(patterns['position_consensus'], indent=2)}
"""
    prompt = f"""
Sei un bioinformatico. Questi sono punteggi EURISTICI (non validati) di uno
strumento didattico sulla mutazione SBDS c.258+2T>C. Commenta con prudenza.

DATI (euristici, demo):
{stats}

SEQUENZE TOP 10:
{top_seqs}

Domande:
1. Ci sono pattern ricorrenti nelle posizioni delle basi? Descrivili.
2. Quali strumenti VALIDATI (SpliceAI, PEGG, GuideScan2) userei per verificarli davvero?
3. Quali di questi spunti sarebbe onesto NON portare a un medico senza validazione?

Non dare certezze. Distingui sempre ipotesi da fatti.
"""
    return prompt


def main():
    print("Meta-analista LLM (commenti su statistiche demo)")
    data = load_data()
    if not data:
        sys.exit(1)
    patterns = extract_patterns(data)
    if not patterns:
        print("Nessun pattern estratto.")
        sys.exit(1)
    prompt = build_prompt(patterns)
    print("Invio prompt all'LLM locale...")
    answer = ask_llm(prompt, model="mistral")
    if answer:
        print("\n" + "=" * 60)
        print("RAPPORTO LLM (spunti, non conclusioni):")
        print("=" * 60)
        print(answer)
        with open("llm_report.txt", "w") as f:
            f.write(prompt + "\n\nRISPOSTA:\n" + answer)
        print("\nReport salvato in llm_report.txt")
    else:
        print("[x] Nessuna risposta.")


if __name__ == "__main__":
    main()
