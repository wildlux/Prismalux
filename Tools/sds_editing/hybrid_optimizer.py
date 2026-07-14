#!/usr/bin/env python3
# =============================================================================
# ⚠️  STRUMENTO DIDATTICO — NON PRODUCE RISULTATI CLINICI
# `simulate_splicing` restituisce un valore in gran parte CASUALE: e' un
# SEGNAPOSTO per mostrare la logica del ciclo DNA->RNA->DNA, non una predizione.
# Per una predizione di splicing reale usa SpliceAI / MaxEntScan.
# =============================================================================
"""
hybrid_optimizer.py - Ciclo ibrido DNA/RNA con feedback iterativo (demo)
"""

import json
import random
import logging

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


class HybridOptimizer:
    def __init__(self, config):
        self.target = config['target_sequence']
        self.mutation_pos = config.get('mutation_pos', 0)
        self.max_iter = config.get('max_iterations', 100)
        self.history = []
        self.best_dna = None
        self.best_rna = None
        self.config = config

    def simulate_splicing(self, seq):
        # SEGNAPOSTO: sostituire con SpliceAI/MaxEntScan per un uso reale.
        gc = (seq.count('G') + seq.count('C')) / max(len(seq), 1)
        return 0.5 + 0.5 * gc + random.uniform(-0.1, 0.1)

    def apply_prime_editing(self, guide):
        corrected = list(self.target)
        if self.mutation_pos < len(corrected):
            corrected[self.mutation_pos] = 'C'  # correzione T->C (demo)
        return ''.join(corrected)

    def apply_rna_correction(self, rna_design):
        return self.target + " (RNA corretto)"

    def analyze_failure(self, seq):
        if self.simulate_splicing(seq) < 0.6:
            return "splicing_weak"
        return "unknown"

    def generate_dna_guide(self):
        start = random.randint(0, len(self.target) - 20)
        return self.target[start:start + 20]

    def generate_rna_design_based_on_failure(self, reason):
        if reason == "splicing_weak":
            return "U1snRNA_" + self.target[:20]
        return "trans_splice_" + self.target[:20]

    def derive_dna_from_rna(self, rna_design):
        return rna_design[:20]

    def modify_rna_design(self, old_design):
        return old_design + "_mod"

    def run(self):
        for iteration in range(self.max_iter):
            logger.info(f"Iterazione {iteration + 1}")
            dna_guide = self.generate_dna_guide()
            corrected = self.apply_prime_editing(dna_guide)
            dna_score = self.simulate_splicing(corrected)
            self.history.append({'type': 'DNA', 'guide': dna_guide, 'score': dna_score})
            if dna_score > 0.8:
                self.best_dna = dna_guide
                logger.info(f"[ok] DNA funzionante (demo)! Guida: {dna_guide}")
                break
            reason = self.analyze_failure(corrected)
            rna_design = self.generate_rna_design_based_on_failure(reason)
            rna_score = self.simulate_splicing(self.apply_rna_correction(rna_design))
            self.history.append({'type': 'RNA', 'design': rna_design, 'score': rna_score})
            if rna_score > 0.7:
                logger.info(f"[ok] RNA funzionante (demo)! Design: {rna_design}")
                self.derive_dna_from_rna(rna_design)
                self.best_rna = rna_design
            else:
                logger.info("[x] RNA fallito, modifico il design...")
                rna_design = self.modify_rna_design(rna_design)
        return self.summarize()

    def summarize(self):
        return {
            'best_dna_guide': self.best_dna,
            'best_rna_design': self.best_rna,
            'history': self.history,
            'iterations': len(self.history)
        }


if __name__ == "__main__":
    config = json.load(open("config.json"))
    opt = HybridOptimizer(config)
    result = opt.run()
    with open("hybrid_report.json", "w") as f:
        json.dump(result, f, indent=2)
    print("Report ibrido salvato in hybrid_report.json")
