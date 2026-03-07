# Task: wildlux@wdx-hp15cb:~/Desktop/Python/multi_agente$ python3 multi_agente.py
# Generato il: 2026-02-21 23:37

import numpy as np
import math
from typing import List, Dict, Union

# --- 1. DEFINIZIONE AGENTI ---

class TermGeneratorAgent:
    """Calcola i termini della serie di Leibniz per Pi: (-1)^k / (2k + 1)"""
    def generate_terms(self, start_idx: int, end_idx: int) -> np.ndarray:
        indices = np.arange(start_idx, end_idx)
        return ((-1.0)**indices) / (2.0 * indices + 1.0)

class AggregatorAgent:
    """Somma i termini e applica il moltiplicatore (4)"""
    def aggregate(self, terms: np.ndarray) -> float:
        return 4.0 * float(np.sum(terms))

class EvaluatorAgent:
    """Verifica l'accuratezza rispetto al valore reale di Pi"""
    def evaluate_error(self, pi_estimate: float) -> float:
        return abs(math.pi - pi_estimate)

class StrategyAgent:
    """Decide se continuare l'esecuzione in base alla precisione raggiunta"""
    def check_convergence(self, current_error: float, threshold: float) -> bool:
        return current_error > threshold

# --- 2. REGOLAMENTO E PROTOCOLLO DI COMUNICAZIONE ---

class MultiAgentSystem:
    def __init__(self, target_precision: float = 1e-5):
        self.term_gen = TermGeneratorAgent()
        self.aggregator = AggregatorAgent()
        self.evaluator = EvaluatorAgent()
        self.strategy = StrategyAgent()
        
        self.target_precision = target_precision
        self.history: List[Dict[str, Union[int, float]]] = []

    def run(self, max_steps: int = 10) -> List[Dict[str, Union[int, float]]]:
        batch_size = 1000
        total_terms_processed = 0
        terms_buffer = np.array([], dtype=np.float64)

        for step in range(max_steps):
            # Azione Agente 1: Generazione
            new_batch = self.term_gen.generate_terms(total_terms_processed, total_terms_processed + batch_size)
            terms_buffer = np.concatenate([terms_buffer, new_batch])
            total_terms_processed += batch_size

            # Azione Agente 2: Aggregazione
            pi_estimate = self.aggregator.aggregate(terms_buffer)

            # Azione Agente 3: Valutazione
            error = self.evaluator.evaluate_error(pi_estimate)
            
            # Registrazione dati
            self.history.append({"step": step, "estimate": pi_estimate, "error": error})

            # Azione Agente 4: Adattamento Strategico
            if not self.strategy.check_convergence(error, self.target_precision):
                break
                
        return self.history

# --- 3. ESECUZIONE E VERIFICA ---

PRECISIONE_TARGET = 0.0001
MAX_ITERAZIONI = 50

system = MultiAgentSystem(target_precision=PRECISIONE_TARGET)
results = system.run(max_steps=MAX_ITERAZIONI)

print("Risultati Finali:")
if results:
    last_res = results[-1]
    print(f"Step: {last_res['step']}, Estimate: {last_res['estimate']}, Error: {last_res['error']}")

# Estrazione dati tramite list comprehension
steps = [res['step'] for res in results]
estimates = [res['estimate'] for res in results]
errors = [res['error'] for res in results]

# --- 4. VISUALIZZAZIONE E VALIDAZIONE INDIPENDENTE ---

try:
    import matplotlib.pyplot as plt
    plt.figure(figsize=(10, 5))
    plt.plot(steps, estimates, label='Stima Agenti', color='blue', marker='o', markersize=4)
    plt.axhline(y=math.pi, color='red', linestyle='--', label='Valore Reale (math.pi)')
    plt.title('Convergenza Formula di Pi tramite Sistema Multi-Agente')
    plt.xlabel('Step di Iterazione')
    plt.ylabel('Valore Pi')
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.show()
except ImportError:
    print("\n[INFO] Matplotlib non trovato. Impossibile generare il grafico.")

if errors:
    final_error = errors[-1]
    print(f"\nVerifica Indipendente:")
    print(f"Errore Finale: {final_error:.10f}")
    print(f"Target Raggiunto: {final_error <= PRECISIONE_TARGET}")