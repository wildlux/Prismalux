#!/usr/bin/env python3
# =============================================================================
# ⚠️  STRUMENTO DIDATTICO — NON PRODUCE RISULTATI CLINICI
# `score_candidate` usa euristiche con pesi arbitrari (GC, ripetizioni, Tm).
# Serve a dimostrare la meccanica del brute force + checkpoint, NON a scegliere
# guide reali. Per la progettazione vera servono PEGG/DeepPrime + GuideScan2.
# =============================================================================
"""
bruteforce_sds_stable.py - Brute force stabile (demo) con checkpoint
"""

import os
import sys
import json
import csv
import logging
from tqdm import tqdm

try:
    os.nice(19)
except Exception:
    pass

logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s',
                    handlers=[logging.FileHandler("bruteforce.log"), logging.StreamHandler()])
logger = logging.getLogger(__name__)


def load_config():
    with open("config.json", "r") as f:
        return json.load(f)


def generate_candidates(sequence, window_size):
    for start in range(0, len(sequence) - window_size + 1):
        window = sequence[start:start + window_size]
        for offset in range(0, window_size - 20 + 1):
            candidate = window[offset:offset + 20]
            if len(candidate) == 20:
                yield {'sequence': candidate, 'start': start + offset, 'window_start': start}


def score_candidate(cand):
    # NB: euristica DIMOSTRATIVA, non un modello biologico.
    seq = cand['sequence']
    gc = (seq.count('G') + seq.count('C')) / 20 * 100
    gc_score = 1.0 - min(abs(gc - 50) / 50, 1.0)
    repeat_penalty = sum([0.2 for b in ['A', 'T', 'G', 'C'] if b * 5 in seq])
    repeat_score = max(1.0 - repeat_penalty, 0.0)
    tm = sum([4 if c in 'GC' else 2 for c in seq])
    tm_score = 1.0 - min(abs(tm - 60) / 30, 1.0)
    off_target_penalty = 0.3 if 'GG' in seq[3:7] else 0.0
    total = round((gc_score * 0.4 + repeat_score * 0.3 + tm_score * 0.3 - off_target_penalty * 0.2), 3)
    return {'gc': round(gc, 1), 'tm': round(tm, 1), 'gc_score': round(gc_score, 3),
            'repeat_score': round(repeat_score, 3), 'tm_score': round(tm_score, 3),
            'off_target_penalty': off_target_penalty, 'total_score': max(0.0, min(1.0, total))}


def load_checkpoint(fname):
    if os.path.exists(fname):
        with open(fname, 'r') as f:
            data = json.load(f)
            data['processed_set'] = set(data.get('processed_set', []))
            return data
    return {'processed_count': 0, 'best_guides': [], 'processed_set': set()}


def save_checkpoint(fname, state):
    to_save = state.copy()
    to_save['processed_set'] = list(state['processed_set'])
    with open(fname, 'w') as f:
        json.dump(to_save, f)


def main():
    config = load_config()
    target = config['target_sequence']
    window = config['window_size']
    checkpoint_file = config['checkpoint_file']
    results_file = config['results_file']
    save_interval = config['save_interval']

    state = load_checkpoint(checkpoint_file)
    processed = state['processed_set']
    best = state['best_guides']
    count = state['processed_count']

    total_candidates = (len(target) - window + 1) * (window - 20 + 1)
    logger.info(f"Totale candidati stimato: {total_candidates}, gia processati: {count}")

    gen = generate_candidates(target, window)
    with tqdm(total=total_candidates, initial=count, desc="Brute Force", unit="guide") as pbar:
        for cand in gen:
            seq = cand['sequence']
            if seq in processed:
                pbar.update(1)
                continue
            scores = score_candidate(cand)
            cand['scores'] = scores
            if scores['total_score'] > 0.6:
                best.append(cand)
                best = sorted(best, key=lambda x: x['scores']['total_score'], reverse=True)[:100]
            processed.add(seq)
            count += 1
            pbar.update(1)
            pbar.set_postfix({'Best': best[0]['scores']['total_score'] if best else 0})

            if count % save_interval == 0:
                state['processed_count'] = count
                state['processed_set'] = processed
                state['best_guides'] = best
                save_checkpoint(checkpoint_file, state)

    state['processed_count'] = count
    state['processed_set'] = processed
    state['best_guides'] = best
    save_checkpoint(checkpoint_file, state)

    with open(results_file, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['Sequenza', 'Posizione', 'GC%', 'TM', 'Total Score', 'Dettagli'])
        for g in best:
            writer.writerow([g['sequence'], g['start'], g['scores']['gc'], g['scores']['tm'],
                             g['scores']['total_score'],
                             f"GC={g['scores']['gc_score']}, Rep={g['scores']['repeat_score']}, TM={g['scores']['tm_score']}"])

    logger.info(f"Completato! {count} guide valutate. Risultati in {results_file}")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        logger.info("Interrotto. Checkpoint salvato.")
        sys.exit(0)
