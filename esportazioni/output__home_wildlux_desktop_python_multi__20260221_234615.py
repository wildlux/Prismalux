# Task: '/home/wildlux/Desktop/Python/multi_agente/La risposta ai numeri primi rivisto.pdf' ho questo pdf puoi crearmi un codice python con i procedimenti che vedi in questo file?
# Generato il: 2026-02-21 23:46

import math
import numpy as np
from typing import List, Tuple

class PrimeFinderAgent:
    """Agente specializzato nella generazione efficiente di numeri primi."""
    def __init__(self, upper_limit: int):
        self.upper_limit = upper_limit

    def get_primes(self) -> np.ndarray:
        """Implementa il Crivello di Eratostene ottimizzato tramite NumPy."""
        sieve = np.ones(self.upper_limit + 1, dtype=bool)
        sieve[0:2] = False
        for p in range(2, int(math.sqrt(self.upper_limit)) + 1):
            if sieve[p]:
                sieve[p*p : self.upper_limit + 1 : p] = False
        return np.where(sieve)[0]

class PatternAnalystAgent:
    """Agente che analizza la struttura e i 'gap' tra i numeri primi."""
    def analyze_gaps(self, primes: np.ndarray) -> np.ndarray:
        """Calcola la differenza tra numeri primi consecutivi."""
        return np.diff(primes) if primes.size >= 2 else np.array([])

    def identify_twins(self, primes: np.ndarray) -> List[Tuple[int, int]]:
        """Trova i numeri primi gemelli (p, p+2) tramite list comprehension."""
        return [(primes[i], primes[i+1]) 
                for i in range(len(primes) - 1) 
                if primes[i+1] - primes[i] == 2]

class DistributionOptimizerAgent:
    """Agente per la validazione statistica e la risposta del sistema."""
    def calculate_density(self, primes: np.ndarray, upper_limit: int) -> Tuple[float, float]:
        """Confronta la densità reale con il Teorema dei Numeri Primi."""
        actual_density = len(primes) / upper_limit if upper_limit > 0 else 0.0
        theoretical_density = 1 / math.log(upper_limit) if upper_limit > 1 else 0.0
        return actual_density, theoretical_density

def main() -> None:
    # Parametri di analisi
    ANALYSIS_LIMIT = 1000
    
    # Inizializzazione Agenti (Architettura Multi-Agente)
    finder = PrimeFinderAgent(ANALYSIS_LIMIT)
    analyst = PatternAnalystAgent()
    optimizer = DistributionOptimizerAgent()

    # Esecuzione Workflow
    # 1. Estrazione numeri primi
    found_primes = finder.get_primes()
    
    # 2. Analisi dei Gap e dei Pattern
    prime_gaps = analyst.analyze_gaps(found_primes)
    twin_primes = analyst.identify_twins(found_primes)
    
    # 3. Calcolo metriche di distribuzione
    actual_density, theoretical_density = optimizer.calculate_density(found_primes, ANALYSIS_LIMIT)

    # Output dei risultati
    print(f"--- Analisi Numeri Primi (Limite: {ANALYSIS_LIMIT}) ---")
    print(f"Primi trovati: {len(found_primes)}")
    print(f"Primi gemelli identificati: {len(twin_primes)}")
    print(f"Primi gemelli (esempi): {twin_primes[:5]}...")
    
    mean_gap = np.mean(prime_gaps) if prime_gaps.size > 0 else 0.0
    print(f"Gap medio calcolato: {mean_gap:.2f}")
    print(f"Densità Reale: {actual_density:.4f}")
    print(f"Densità Teorica (1/ln(N)): {theoretical_density:.4f}")
    
    # Calcolo dello scostamento (residuo)
    model_deviation = abs(actual_density - theoretical_density)
    print(f"Scostamento dal modello teorico: {model_deviation:.4f}")

if __name__ == "__main__":
    main()