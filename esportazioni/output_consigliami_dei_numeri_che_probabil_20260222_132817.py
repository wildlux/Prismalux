# Task: consigliami dei numeri che probabilmente usciranno per il superenalotto

MIGLIORAMENTO RICHIESTO: i commenti

Codice precedente da migliorare:
```python
import random

def find_primes(limit: int) -> list[int]:
    """Utilizza l'algoritmo di Eratostene per trovare i numeri primi fino al limite specificato."""
    primes = [True] * (limit + 1)
    p = 2
    while p * p <= limit:
        if primes[p]:
            for i in range(p * p, limit + 1, p):
                primes[i] = False
        p += 1
    return [p for p in range(2, limit) if primes[p]]

# Imposta il limite per trovare i numeri primi (esempio: 50)
limit = 50
prime_numbers = find_primes(limit)

# Seleziona casualmente alcuni numeri primi come candidati al Superenalotto
numbers_to_draw = random.sample(prime_numbers, 5)  # Estrai 5 numeri casuali

# Aggiungi un numero casuale tra 1 e 90 per completare la combinazione
random_number = random.randint(1, 90)
numbers_to_draw.append(random_number)

# Ordina i numeri in ordine crescente
numbers_to_draw.sort()

# Stampa la combinazione di numeri suggerita
print("Combinazione del Superenalotto sugerita:", numbers_to_draw)
```
# Generato il: 2026-02-22 13:28

import random

def suggerisci_numeri_superenalotto() -> None:
    """Funzione per suggerire numeri del Superenalotto"""
    
    # Lista dei numeri che vengono estratti dal superenalotto
    numeri_da_estrarre = list(range(1, 91))
    
    # Selezione casuale 6 numeri dalla lista
    numeri_estratti = random.sample(numeri_da_estrarre, 6)
    
    # Aggiungi un numero casuale tra 1 e 90 per completare la combinazione
    numero_casuale = random.randint(1, 90)
    numeri_estratti.append(numero_casuale)
    
    # Ordina i numeri in ordine crescente
    numeri_estratti.sort()
    
    # Stampa la combinazione di numeri suggerita
    print("Combinazione del Superenalotto sugerita:", numeri_estratti)

# Chiama il funzione
suggerisci_numeri_superenalotto()