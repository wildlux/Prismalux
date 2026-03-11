# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Contesto

Progetto Python per un utente di livello intermedio (Catania, Sicilia).
L'utente comunica in italiano. Rispondere sempre in italiano.
Punto di ingresso UNICO: `AVVIA_Prismalux.py` — non creare altri launcher o script `.sh`.

## Struttura del progetto

```
Desktop/Prismalux/
├── AVVIA_Prismalux.py               ← PUNTO DI INGRESSO (app Textual TUI)
├── avvia_gpu.py                     ← avvio GPU multi-istanza Ollama (opzionale)
├── check_deps.py                    ← risorse CPU/RAM/GPU + stampa_header
├── ollama_utils.py                  ← auto-detect modello Ollama locale
├── prismalux_screens_base.py        ← ResourceBar + PrismaluxScreen (base condivisa)
├── settings.json                    ← preferenze utente (bordi, avvia_auto, ...)
├── CLAUDE.md
├── strumento_apprendimento/
│   ├── main.py                      ← ApprendimentoScreen (6 voci)
│   ├── simulatore.py                ← 80 algoritmi con console.print + hook Textual
│   ├── simulatore_screen.py         ← SimulatoreMenuScreen + AlgoritmoScreen
│   ├── tutor_screen.py              ← TutorScreen + SezioneScreen + TopicScreen
│   └── tutor_semplice.py            ← dati sezioni Python/Mat/Fisica/Chimica/Sicurezza
├── strumento_pratico/
│   ├── main.py                      ← PraticoScreen (3 voci)
│   ├── cerca_lavoro.py / lavoro_screen.py  ← ricerca DuckDuckGo + 26 portali
│   ├── cv_reader.py                 ← lettore CV (.txt/.pdf) con Ollama
│   ├── db_lavoro.py                 ← whitelist/blacklist JSON
│   ├── dichiarazione_730.py / screen_730.py
│   ├── partita_iva.py / iva_screen.py
│   ├── cv/                          ← CV utente (.txt o .pdf)
│   └── data/                        ← whitelist.json + blacklist.json
├── multi_agente/
│   ├── multi_agente.py              ← logica 6 agenti AI (LangGraph + Ollama)
│   └── multi_agente_screen.py       ← MultiAgenteScreen + EseguiAgenteScreen
├── llama_cpp_studio/
│   ├── main.py / llama_screen.py    ← Studio per compilare/gestire llama.cpp
│   ├── config.json                  ← config llama.cpp Studio
│   └── models/                      ← modelli .gguf
└── archivio/                        ← vecchi esercizi (non in uso)
```

## Avvio

```bash
cd ~/Desktop/Prismalux
python3.14 AVVIA_Prismalux.py
# oppure con venv attivo:
python AVVIA_Prismalux.py
```

**NON usare avvia.sh o altri launcher shell.** Solo Python puro.

Avvio GPU multi-istanza (opzionale, separato):
```bash
python3.14 avvia_gpu.py           # chiede se avviare Prismalux (salva preferenza)
python3.14 avvia_gpu.py --auto    # avvia tutto senza chiedere
python3.14 avvia_gpu.py --stop    # ferma istanze Ollama extra
```

## Architettura Textual TUI

Tutta l'app è una singola `PrismaluxApp` (Textual 8.0.1) con push/pop screens.
**ZERO suspend(), runpy o processi figli** — tutto avviene dentro l'app.

### Screen hierarchy
```
PrismaluxApp
├── MenuPrincipaleScreen           (launcher, 4 voci)
│   ├── ApprendimentoScreen        (6 voci)
│   │   ├── SimulatoreMenuScreen   (80 algoritmi, buffer numerico)
│   │   │   └── AlgoritmoScreen    (@work thread + RichLog)
│   │   └── TutorScreen → SezioneScreen → TopicScreen (streaming Ollama)
│   ├── PraticoScreen              (3 voci)
│   │   ├── CercaLavoroScreen + sotto-schermate (Input+RichLog)
│   │   ├── Screen730 + QA730Screen
│   │   └── PartitaIVAScreen + CalcolatoreForfettario + QAIVA
│   ├── MultiAgenteScreen + EseguiAgenteScreen (@work thread)
│   └── LlamaCppScreen             (compile, modelli, avvia, server)
```

### Componenti base (`prismalux_screens_base.py`)
- `ResourceBar`: barra risorse CPU/RAM/GPU/DSK in cima a ogni schermata
- `PrismaluxScreen`: Screen base con `compose_base()`, binding ESC/Q

### Import corretto Textual 8.0.1
```python
from textual import work          # NON from textual.worker
```

### Bug noti risolti
- **Key leak**: premere "1" in ApprendimentoScreen causa che "1" arrivi in SimulatoreMenuScreen
  → fix: `_pronto = False` in `on_mount()` + timer 150ms `set_timer(0.15, self._abilita_input)`
- **Digit "0" binding**: con buffer vuoto → `pop_screen()`; con buffer → aggiunge "0"
- **__pycache__**: se un fix non ha effetto, cancellare i __pycache__ e riprovare

## Simulatore algoritmi (`simulatore_screen.py`)

**80 algoritmi** in `simulatore.py` eseguiti in `@work(thread=True)`.

### Come funziona l'intercetto
`simulatore.py` usa `console.print()` e `os.system("clear")` che bypasserebbero Textual.
In `AlgoritmoScreen._esegui()`:
1. `sim.console` e `sim.Console` → `_ConsoleFake(hook)` che reindirizza print → `frame_cb`
2. `builtins.input` → `_input_textual()` che estrae il default dal prompt (pattern `invio=X`)
3. `os.system` → no-op per "clear"/"cls"
4. `sim._modo_idx` → impostato su animato 0.6s di default (non passo-passo)

### Velocità (`AlgoritmoScreen._VELOCITA`)
```python
_VELOCITA = [
    ("🐇 Animato  0.6s  [A=cambia]", 2),   # default
    ("🐌 Animato  1.5s  [A=cambia]", 1),
    ("⚡ Animato  0.04s [A=cambia]", 4),
    ("🐢 Passo manuale  [INVIO]",    0),
]
```
Tasto `A` per ciclare. In modalità manuale: INVIO/SPAZIO avanza.

### Bug noti risolti in simulatore.py
- `max_val = max(max(arr), 1) if arr else 1`  ← evita ZeroDivisionError con array [0]
- `_ConsoleFake` con doppio tentativo markup=True/False ← evita MarkupError Rich

## Navigazione — convenzioni globali

Tutto in Textual usa binding nativi:
- `ESC` / `0` → `pop_screen()` (torna al menu precedente)
- `Q` → `action_esci()` → `app.exit()`
- Input non valido → notifica, non crash

## Preferenze utente (`settings.json`)

File JSON nella cartella del progetto. Chiavi rilevanti:
```json
{
  "bordi": true,
  "avvia_auto": true
}
```
- `avvia_auto`: `true` = avvia Prismalux senza chiedere dopo GPU setup; `false` = non avviare; assente = chiedi
- `avvia_gpu.py` legge/scrive questa chiave tramite `_carica_settings()` / `_salva_settings()`

## I 4 strumenti

### 1. Apprendimento (`strumento_apprendimento/`)
- **Simulatore**: 80 algoritmi passo-passo o animati, in Textual TUI
- **Tutor AI**: Python (215 arg), Matematica (153), Fisica (~90), Chimica (~65), Sicurezza (45+)
  - Streaming Ollama in `TutorScreen` con `RichLog`
  - All'avvio sessione: scelta modello Ollama + modalità ragionamento (no/durante/fine)
- Dipendenze: `pip install textual rich requests`

### 2. Strumento Pratico (`strumento_pratico/`)
- **Cerca lavoro**: DuckDuckGo (`ddgs`), 26 portali con `site:`, whitelist/blacklist
- **CV reader**: legge `.txt`/`.pdf`, analisi Ollama
- **730**: guida + Q&A Ollama
- **Partita IVA**: guida + calcolatore forfettario + Q&A Ollama
- Dipendenze: `pip install textual rich requests ddgs` + opzionale `pypdf`

### 3. Multi-Agente AI (`multi_agente/`)
- 6 agenti: Ricercatore → Pianificatore → 2 Programmatori → Tester (loop) → Ottimizzatore
- LangGraph + ChatOllama, selezione modello interattiva per ruolo
- Dipendenze: `pip install langgraph langchain-ollama langchain-core requests`

### 4. llama.cpp Studio (`llama_cpp_studio/`)
- Compila llama.cpp (git clone + cmake), gestisce modelli .gguf, avvia chat o server :8080
- `CompilaScreen`: `subprocess.Popen` → log live in `RichLog`
- `AvviaScreen`: chat = `app.suspend()` (llama-cli interattivo), server = `ServerRunScreen`
- Config in `llama_cpp_studio/config.json`, modelli in `llama_cpp_studio/models/`

## Ambiente

- **Python**: 3.14 (manca `_sqlite3` → usare JSON, mai SQLite)
- **Textual**: 8.0.1 (`from textual import work`)
- **Ollama**: `http://localhost:11434` — modello auto-rilevato da `ollama_utils.py`
  - `_auto_modello()` filtra modelli cloud (size=0), usa solo modelli locali
  - Timeout: 300s (modelli lenti al primo avvio)
- **ddgs** (non `duckduckgo_search`, è stato rinominato)
- **Venv**: `.venv/` nella cartella progetto

## Ollama utils (`ollama_utils.py`)

- `MODELLO = ""` → auto-detect al primo uso via `_get_modello_default()`
- `scegli_modello_interattivo(argomento)` → lista modelli locali, utente sceglie
- `chiedi_ollama_think`: doppio fallback (no-think → auto_modello locale) in caso 404

## check_deps.py

- `risorse()` → dict con cpu, ram_perc, ram_gb, gpu_perc, vram_perc, disco_perc, cpu_name
- `stampa_header(con, res=None)` → pannello giallo con barre ██░░
- `_cpu_perc_reale()` → legge `/proc/stat` con campionamento differenziale
- Fallback Windows: `ctypes` per RAM, `wmic` per CPU name, `C:\` per disco

## Livello dell'utente

Intermedio. Conosce: variabili, if/else, cicli, liste, funzioni, classi base.
Argomenti da approfondire: list comprehension, generatori/yield, `is` vs `==`,
`zip()`, `lambda`, argomenti default mutabili, `self` nei metodi di classe.
