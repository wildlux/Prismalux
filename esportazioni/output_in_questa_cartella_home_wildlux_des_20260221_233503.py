# Task: in questa cartella /home/wildlux/Desktop/Python/multi_agente/ c'e un file che si chiama creare_num_primi.py che non funziona puoi sistemarlo?
# Generato il: 2026-02-21 23:35

import time
import random
from typing import List

def sieve_of_eratosthenes(limit: int) -> List[int]:
    """
    Ottimizzazione Algoritmica: Crivello di Eratostene usando liste Python standard.
    """
    if limit < 2:
        return []
    
    # Inizializza lista booleana: True indica potenziale numero primo
    is_prime_map = [True] * (limit + 1)
    is_prime_map[0] = is_prime_map[1] = False
    
    # Itera fino alla radice quadrata del limite
    for i in range(2, int(limit**0.5) + 1):
        if is_prime_map[i]:
            # Segna i multipli di i partendo da i*i come False
            for multiple in range(i * i, limit + 1, i):
                is_prime_map[multiple] = False
            
    return [number for number, is_prime in enumerate(is_prime_map) if is_prime]

def is_prime_miller_rabin(number: int, iterations: int = 5) -> bool:
    """
    Analisi e Ottimizzazione: Test di Miller-Rabin per numeri grandi.
    Metodo probabilistico molto più veloce della divisione per tentativi.
    """
    if number < 2: 
        return False
    if number in (2, 3): 
        return True
    if number % 2 == 0: 
        return False

    # Scrivi number-1 come 2^exponent_of_two * odd_multiplier
    exponent_of_two, odd_multiplier = 0, number - 1
    while odd_multiplier % 2 == 0:
        exponent_of_two += 1
        odd_multiplier //= 2

    # Test per 'iterations' volte
    for _ in range(iterations):
        witness = random.randint(2, number - 2)
        x = pow(witness, odd_multiplier, number)
        
        if x == 1 or x == number - 1:
            continue
            
        for _ in range(exponent_of_two - 1):
            x = pow(x, 2, number)
            if x == number - 1:
                break
        else:
            return False
            
    return True

def run_performance_test() -> None:
    """
    Performance & Debug: Valuta l'efficienza del codice.
    """
    # Caso 1: Generazione bulk
    limit_test = 1_000_000
    start_time = time.perf_counter()
    primes_found = sieve_of_eratosthenes(limit_test)
    end_time = time.perf_counter()
    
    print(f"--- TEST GENERAZIONE (Sieve) ---")
    print(f"Range: 0 - {limit_test}")
    print(f"Primi trovati: {len(primes_found)}")
    print(f"Tempo esecuzione: {end_time - start_time:.6f} secondi")

    # Caso 2: Test singolo numero grande
    large_n = 104729  # Il 10.000-esimo numero primo
    start_time = time.perf_counter()
    is_prime_result = is_prime_miller_rabin(large_n)
    end_time = time.perf_counter()
    
    print(f"\n--- TEST SINGOLO (Miller-Rabin) ---")
    print(f"Numero verificato: {large_n}")
    print(f"Risultato: {'Primo' if is_prime_result else 'Non Primo'}")
    print(f"Tempo esecuzione: {end_time - start_time:.6f} secondi")

def run_integrity_test() -> None:
    """
    Test Rigoroso: Verifica correttezza sui casi base e limiti.
    """
    print("\n--- TEST RIGOROSO ---")
    test_cases = {0: False, 1: False, 2: True, 3: True, 4: False, 13: True, 25: False}
    
    results = [is_prime_miller_rabin(num) == expected for num, expected in test_cases.items()]
    errors = results.count(False)
    
    if errors == 0:
        print("Tutti i test di integrità sono passati con successo.")
    else:
        # Debug degli errori se necessario
        for i, (num, expected) in enumerate(test_cases.items()):
            if not results[i]:
                print(f"ERRORE logico: {num} dovrebbe essere {expected}")

if __name__ == "__main__":
    run_performance_test()
    run_integrity_test()