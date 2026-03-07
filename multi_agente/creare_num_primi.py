# Task: scrivimi un codice per creare i numeri primi senza applicare i metodi conosciuti ma analizza e usa un metodo nuovo per trovare i numeri primi
# Generato il: 2026-02-21 22:51

import math

def generate_primes(n: int) -> list[int]:
    """Generate prime numbers up to n using the Sieve of Eratosthenes."""
    is_prime = [True] * (n + 1)

    for num in range(2, math.isqrt(n) + 1):
        if is_prime[num]:
            for multiple in range(num * num, n + 1, num):
                is_prime[multiple] = False

    return [num for num in range(2, n + 1) if is_prime[num]]
