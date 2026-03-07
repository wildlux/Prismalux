# Task: creami la formula del pigreco usando il teorema di eratostane
# Generato il: 2026-02-21 20:03

import math

def sieve_of_eratosthenes(n: int) -> list[int]:
    """
    Formula per pi usando il teorema di Eratostene 
    """
    primes = [True] * (n + 1)
    primes[0] = primes[1] = False  # 0 e 1 non sono primi

    for i in range(2, int(n**0.5) + 1):
        if primes[i]:
            for j in range(i * i, n + 1, i):
                primes[j] = False 
    return [i for i, v in enumerate(primes) if v]


pi = 4