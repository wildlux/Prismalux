# Task: contami da 1 a 100 ma solo i numeri pari non scrivere sul disco
# Generato il: 2026-02-22 11:52

from typing import List

def main():
    # Genera una lista comprensiva dei numeri pari da 1 a 100
    numeri_pari: List[int] = [numero for numero in range(1, 101) if numero % 2 == 0]
    
    # Stampa i numeri pari a video
    print("Numeri pari da 1 a 100:")
    print(*numeri_pari, sep=' ')

if __name__ == "__main__":
    main()