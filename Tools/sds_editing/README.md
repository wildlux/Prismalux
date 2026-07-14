# SDS Editing — strumenti di studio (Prismalux)

Pipeline **didattica** per esplorare, in locale, il flusso di lavoro di
progettazione di guide per l'editing genico applicato alla Sindrome di
Shwachman-Diamond (mutazione comune `SBDS c.258+2T>C`).

## ⚠️ Leggi prima di usarlo

Questi script servono per **imparare la pipeline** (brute force → ottimizzazione
→ analisi con LLM locale → report), **non** per progettare una terapia.

- Le funzioni `score_candidate`, `simulate_splicing` e simili sono **euristiche
  e in parte segnaposto** (pesi arbitrari, valori casuali). Non hanno validità
  biologica clinica.
- Non portare l'output di questi script a un medico come "ipotesi terapeutica".
- Per un uso reale servono strumenti validati (**SpliceAI**, **PEGG/DeepPrime**,
  **GuideScan2**) e soprattutto la collaborazione con un centro di ricerca.

**In Italia**, riferimenti reali per l'SDS:
- Centro Fibrosi Cistica di Verona — Laboratorio di Ricerca Preclinica
- Registro Italiano SDS (RI-SDS)
- Associazione Italiana Sindrome di Shwachman (AISS) — `aiss@shwachman.it`

## Installazione

```bash
pip install -r requirements.txt
# (opzionale) LLM locale per llm_meta_analyst.py
curl -fsSL https://ollama.com/install.sh | sh
ollama pull mistral
```

## Configurazione

Modifica `config.json`:
- `target_sequence`: sequenza di DNA intorno alla mutazione (**va fornita da un
  genetista** — quella di default è fittizia).
- `blood_csv`: CSV con gli esami (`data, globuli_bianchi, emoglobina, piastrine, neutrofili`).

## Esecuzione

```bash
python3 sds_analyzer.py            # report PDF esami del sangue
python3 bruteforce_sds_stable.py   # brute force (demo) con checkpoint
python3 hybrid_optimizer.py        # ciclo DNA/RNA (demo)
python3 llm_meta_analyst.py        # analisi pattern con LLM locale
python3 sharding_master.py         # divide il lavoro su N nodi
python3 run_all.py                 # tutto in sequenza
```

## Output

| File | Contenuto |
|------|-----------|
| `SDS_Report.pdf` | Grafici andamento esami del sangue |
| `best_guides.csv` | Guide candidate ordinate per punteggio (demo) |
| `checkpoint.json` | Stato del brute force (ripresa) |
| `hybrid_report.json` | Risultato del ciclo DNA/RNA (demo) |
| `llm_report.txt` | Ipotesi generate dall'LLM locale |

## Privacy

Tutto gira in locale. Se usi `llm_meta_analyst.py`, l'LLM è **Ollama in locale**:
nessun dato esce dal PC. Vengono passate all'LLM solo statistiche aggregate.
