# Task: wildlux@wdx-hp15cb:~/Desktop/Python/multi_agente$ python3 multi_agente.py
# Generato il: 2026-02-21 23:39

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from multiprocessing import Pool
from typing import List, Dict

def compute_chunk_statistics(data_chunk: pd.DataFrame) -> Dict[str, float]:
    """
    Calcola statistiche descrittive (media, deviazione standard, massimo) 
    per un determinato subset di dati.
    """
    return {
        "media": float(data_chunk['valore'].mean()),
        "deviazione": float(data_chunk['valore'].std()),
        "max": float(data_chunk['valore'].max())
    }

if __name__ == "__main__":
    # Parametri di configurazione
    num_processes: int = 4
    num_samples: int = 100

    # Generazione dataset iniziale
    input_df = pd.DataFrame({
        'id': range(num_samples),
        'valore': np.random.randn(num_samples) * 10
    })

    # Suddivisione del DataFrame in chunk tramite list comprehension
    # La logica del calcolo chunk_step implementa la ceiling division
    chunk_step: int = -(-len(input_df) // num_processes)
    data_chunks: List[pd.DataFrame] = [
        input_df.iloc[i : i + chunk_step] 
        for i in range(0, len(input_df), chunk_step)
    ]

    # Esecuzione parallela della logica degli agenti
    with Pool(processes=num_processes) as pool:
        execution_results: List[Dict[str, float]] = pool.map(compute_chunk_statistics, data_chunks)

    # Consolidamento dei risultati in un DataFrame per l'analisi
    summary_df = pd.DataFrame(execution_results)
    
    print("Sintesi elaborazione agenti:")
    print(summary_df)

    # Visualizzazione dei risultati
    summary_df.plot(kind='bar', y='media', title='Media calcolata dagli Agenti', legend=False)
    plt.xlabel('Agente ID')
    plt.ylabel('Valore Medio')
    plt.tight_layout()
    plt.show()