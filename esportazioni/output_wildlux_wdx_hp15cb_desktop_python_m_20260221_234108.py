# Task: wildlux@wdx-hp15cb:~/Desktop/Python/multi_agente$ python3 multi_agente.py
# Generato il: 2026-02-21 23:41

import json
import os
import numpy as np
from datetime import datetime
from typing import List, Dict, Optional, Any

# --- 1. DEFINIZIONE DELLE AGENZIE ---

class BaseAgent:
    """Classe base per definire le proprietà comuni degli agenti."""
    def __init__(self, agent_id: int, name: str) -> None:
        self.agent_id = agent_id
        self.name = name

class ScoutAgent(BaseAgent):
    """Agente specializzato nel recupero dati (Ottenimento informazioni)."""
    def collect_data(self) -> List[float]:
        # Dati di esempio (simulazione sensori o API)
        source_values = [22.5, 23.1, 21.8, 25.0, 24.2, 23.9]
        # Utilizzo di list comprehension per garantire il tipo float
        raw_data: List[float] = [float(val) for val in source_values]
        print(f"[Scout - {self.name}] Dati raccolti: {raw_data}")
        return raw_data

class AnalystAgent(BaseAgent):
    """Agente specializzato nell'elaborazione (Analisi)."""
    def analyze(self, data: List[float]) -> Optional[Dict[str, Any]]:
        try:
            # Utilizzo di numpy per analisi numerica e dizionario per i risultati
            analysis_results: Dict[str, Any] = {
                "media": float(np.mean(data)),
                "deviazione": float(np.std(data)),
                "timestamp": datetime.now().strftime("%Y-%m-%d %H:%M:%S")
            }
            print(f"[Analyst - {self.name}] Analisi completata.")
            return analysis_results
        except Exception as e:
            print(f"Errore analisi: {e}")
            return None

class CommunicationAgent(BaseAgent):
    """Agente per lo scambio dati tramite file (Comunicazione)."""
    def __init__(self, agent_id: int, name: str, bus_path: str) -> None:
        super().__init__(agent_id, name)
        self.bus_path = bus_path

    def send_message(self, report: Dict[str, Any]) -> None:
        try:
            # Implementazione interazione tramite file di dati
            with open(self.bus_path, "w") as f:
                json.dump(report, f, indent=4)
            print(f"[Messenger - {self.name}] Report salvato in {self.bus_path}")
        except IOError as e:
            print(f"Errore I/O: {e}")

# --- 2. GESTIONE DELLE ATTIVITÀ (ORCHESTRAZIONE) ---

def run_simulation() -> None:
    """Orchestratore della simulazione multi-agente."""
    BUS_DATA_PATH: str = "comunicazione_agenti.json"
    
    # Istanza degli agenti con nomi variabili migliorati
    scout = ScoutAgent(1, "Alpha")
    analyst = AnalystAgent(2, "Beta")
    messenger = CommunicationAgent(3, "Gamma", BUS_DATA_PATH)

    print("--- Inizio Simulazione Multi-Agente ---")

    try:
        # Fase 1: Ottenimento informazioni
        raw_metrics: List[float] = scout.collect_data()

        # Fase 2: Analisi
        final_report: Optional[Dict[str, Any]] = analyst.analyze(raw_metrics)

        # Fase 3: Comunicazione/Persistenza
        if final_report:
            messenger.send_message(final_report)
            
            # Verifica lettura file (Simulazione ricezione da altro agente)
            if os.path.exists(BUS_DATA_PATH):
                with open(BUS_DATA_PATH, "r") as f:
                    bus_content: Dict[str, Any] = json.load(f)
                    print(f"Verifica Bus Dati: Media rilevata = {bus_content['media']}")

    except Exception as global_error:
        print(f"Errore critico durante la simulazione: {global_error}")
    
    finally:
        print("--- Fine Simulazione ---")

# --- ESECUZIONE ---

if __name__ == "__main__":
    run_simulation()