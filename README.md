<div align="center">

<img src="https://img.shields.io/badge/Prismalux-AI%20Platform-cyan?style=for-the-badge&logo=data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAyNCAyNCI+PHBhdGggZmlsbD0iI2ZmZiIgZD0iTTEyIDJMMyA3bDkgNSA5LTV6TTMgMTdsOSA1IDktNXYtNWwtOSA1LTktNXoiLz48L3N2Zz4=" alt="Prismalux"/>

# 🍺 Prismalux

### *"Costruito per i mortali che aspirano alla saggezza."*

[![C++/Qt6](https://img.shields.io/badge/GUI-C%2B%2B%20%2F%20Qt6-green?style=flat-square&logo=qt)](https://www.qt.io/)
[![Version](https://img.shields.io/badge/versione-2.5-blue?style=flat-square)](CHANGELOG)
[![License](https://img.shields.io/badge/License-MIT-lightgrey?style=flat-square)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20Windows-informational?style=flat-square)](https://github.com/wildlux/Prismalux)
[![Build](https://img.shields.io/badge/Build-CMake%20%2B%20Qt6-brightgreen?style=flat-square)](C_software/Qt_GUI/CMakeLists.txt)

**Piattaforma AI locale. GUI in C++/Qt6.**  
Multi-agente, anti-allucinazione, matematica locale, 110 simulazioni algoritmi.  
RAG in-page per ogni categoria · MCP Blender / FreeCAD / Office · Network Analyzer · Disegno→3D.  
Zero dipendenze cloud. Zero abbonamenti. Tutto sul tuo hardware.

</div>

---

## Indice

- [Cos'è Prismalux](#cosè-prismalux)
- [Avvio rapido](#avvio-rapido)
- [Compilazione](#compilazione)
- [GUI Qt6 — Tab e funzionalità](#gui-qt6--tab-e-funzionalità)
- [Pipeline Multi-Agente](#pipeline-multi-agente)
- [Motore Byzantino (anti-allucinazione)](#motore-byzantino-anti-allucinazione)
- [Classificatore query e think budget](#classificatore-query-e-think-budget)
- [Motore Matematico Locale](#motore-matematico-locale)
- [Simulatore Algoritmi](#simulatore-algoritmi-110-simulazioni)
- [RAG — Retrieval-Augmented Generation](#rag--retrieval-augmented-generation)
- [Classifica LLM open-weight](#classifica-llm-open-weight)
- [Backend AI supportati](#backend-ai-supportati)
- [Modelli consigliati](#modelli-consigliati)
- [Test automatici](#test-automatici)
- [Struttura del progetto](#struttura-del-progetto)
- [Requisiti](#requisiti)

---

## Cos'è Prismalux

Prismalux è una **piattaforma AI locale** con GUI grafica in **C++/Qt6**. Nasce dall'idea che l'intelligenza artificiale debba girare sul tuo hardware, rispettando la tua privacy, senza connessioni a server esterni.

| Componente | Linguaggio | Descrizione |
|---|---|---|
| GUI grafica — tutte le pagine e widget | **C++/Qt6** | 10+ pagine, 45+ tipi grafico, TTS/STT |
| Client AI (Ollama / llama-server) | **C++** | QNetworkAccessManager, streaming NDJSON |
| Rilevamento hardware | **C** | CPU/GPU/RAM cross-platform |
| Temi visivi | **QSS** | dark-cyan, dark-amber, dark-purple, light |

```
  ╔══════════════════════════════════════════════════════════╗
  ║  🍺  PRISMALUX  v2.2  —  AI Platform                     ║
  ║  Backend: Ollama  ·  Modello: qwen3:30b                  ║
  ║  CPU: ████████░░ 78%  RAM: █████░░░░░ 52%  GPU: 3.2GB    ║
  ╚══════════════════════════════════════════════════════════╝
```

---

## Avvio rapido

```bash
# Clona il repository
git clone git@github.com:wildlux/Prismalux.git
cd Prismalux/C_software/Qt_GUI

# Compila (richiede Qt6 + Ollama attivo)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Avvia
./build/Prismalux_GUI
```

> **Prerequisiti**: `Qt6`, `cmake`, `gcc/clang`, `Ollama` con almeno un modello installato.

```bash
# Installa Ollama e scarica un modello per iniziare
curl -fsSL https://ollama.com/install.sh | sh
ollama pull qwen3:8b        # consigliato: ~5 GB, ottimo italiano + think nativo
```

---

## Compilazione

### Linux (Ubuntu/Debian)

```bash
sudo apt install qt6-base-dev cmake build-essential
cd C_software/Qt_GUI
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/Prismalux_GUI
```

### Linux (Arch/Manjaro)

```bash
sudo pacman -S qt6-base cmake gcc
```

### Windows (MSYS2 MINGW64)

```bash
pacman -S --needed mingw-w64-x86_64-qt6-base mingw-w64-x86_64-cmake mingw-w64-x86_64-gcc
cd C_software/Qt_GUI
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/Prismalux_GUI.exe
```

> Su Windows: avvia `MSYS2 MINGW64` shell, non MSYS2 generico.

### Build con test

```bash
cmake -B build_tests -DBUILD_TESTS=ON
cmake --build build_tests -j$(nproc)
./build_tests/test_signal_lifetime
./build_tests/test_rag_engine
./build_tests/test_code_utils
```

---

## GUI Qt6 — Tab e funzionalità

L'interfaccia è organizzata in **9 pagine** accessibili da sidebar o con `Alt+N`:

| # | Tab | Shortcut | Contenuto principale |
|---|---|---|---|
| 1 | 🤖 **Agenti AI** | `Alt+1` | Pipeline 6 agenti · Motore Byzantino · Pipeline · Consiglio Scientifico · TTS/STT |
| 2 | 🔮 **Oracolo** | `Alt+2` | Chat singola streaming · History compression · RAG contestuale |
| 3 | 📐 **Matematica** | `Alt+3` | Sequenza→Formula · Costanti precisione arbitraria · N-esimo · Espressione locale |
| 4 | 📊 **Grafico** | `Alt+4` | 45+ tipi di grafico · zoom/pan · export PNG · analisi AI immagine |
| 5 | 💻 **Programmazione** | `Alt+5` | Editor + correzione AI · esecuzione Python · **sub-tab Agentica** (pipeline, RAG, refactor, testgen, debug, byzantino) |
| 6 | 💰 **Strumenti** | `Alt+6` | Assistente 730 · Partita IVA · Mutuo · PAC · Pensione INPS · Cerca Lavoro |
| 7 | 📚 **Impara** | `Alt+7` | Tutor AI · Simulatore 110 algoritmi · Quiz interattivi |
| 8 | 🔭 **Materie** | `Alt+8` | Studio per materia (fisica, chimica, storia, diritto…) |
| 9 | ⚙️ **Impostazioni** | — | Hardware · AI Locale · RAG · LLM · **Classifica** · Voce · Aspetto |

### Tab Agenti AI — dettaglio

```
[▶ Avvia / ⏹ Stop]  [⚡ Singolo]  [Correggi con AI ▼]  [fino a: N ∞]

┌── Modalità ──────────────────────────────────────────────┐
│  ○ Pipeline 6 agenti     ○ Byzantino     ○ Consiglio     │
└──────────────────────────────────────────────────────────┘

Pipeline attiva:
  Ricercatore → Pianificatore → Programmatori A+B (paralleli)
              → Tester (max 3 tentativi) → Ottimizzatore
```

- **Invio = risposta singola rapida** (`⚡ Singolo` — 1 agente): per domande veloci senza attendere la pipeline completa. Il pulsante `▶ Avvia` lancia la pipeline multi-agente solo con click esplicito.
- **Modello sincronizzato**: il modello impostato in Impostazioni (o da qualsiasi altro tab) viene propagato automaticamente a tutti i tab via segnale `AiClient::modelChanged` — Agenti, Agentica, Matematica, Oracolo usano tutti la stessa fonte senza feedback loop.
- **TTS** (Text-to-Speech): risposta AI letta ad alta voce (Piper + aplay su Linux, SAPI su Windows) — catena sicura senza shell intermediaria
- **STT** (Speech-to-Text): dettatura con Whisper locale
- **Correggi con AI**: rileva il codice nell'ultima risposta, lo riesegue, corregge automaticamente fino a N volte

### Tab Programmazione — sub-tab Agentica

Il tab Programmazione contiene due schede interne:

| Sub-tab | Contenuto |
|---------|-----------|
| `💻 Programmazione` | Editor codice + correzione AI + esecuzione Python sandbox |
| `🤖 Agentica` | 6 tipi di agente AI con streaming output |

**Tipi di agente disponibili**:

| Tipo | Comportamento |
|------|--------------|
| Pipeline 6 agenti | Analizza → Progetta → Implementa → Testa → Ottimizza → Documenta |
| RAG + Codice | Recupera contesto dai documenti in `RAG/`, genera codice contestuale |
| Refactor | Analizza e ristruttura il codice fornito |
| Genera Test | Crea suite di test unitari/integrazione |
| Debug assistito | Diagnosi sistematica + fix del bug |
| Byzantino | 3 prospettive indipendenti + sintesi anti-allucinazione |

Il pulsante **"Apri in editor"** estrae il primo blocco di codice dall'output e lo inserisce direttamente nel tab Programmazione.

### Tab Grafico — 45+ tipi

Linee, Barre, Area, Torta, Scatter, Istogramma, Heatmap, Boxplot, Violino,
Candlestick (OHLC), Gantt, Sankey, Chord, Treemap, Radar, Waterfall,
Smith Chart, Spirale di Fibonacci, Funnel, Bubble, Stem, ErrorBar,
Ternario, Superficie 3D, Contour, e molti altri.

---

## Pipeline Multi-Agente

```
┌─────────────────────────────────────────────────────────────────────┐
│                     GUARDIE PRE-PIPELINE                            │
│  Math locale  →  Tipo task  →  RAM check  →  Pre-calcolo espressioni│
└──────────────────────────┬──────────────────────────────────────────┘
                           ▼
              ┌────────────────────────┐
              │    Ricercatore (0)     │  raccoglie contesto
              └────────────┬───────────┘
                           ▼
              ┌────────────────────────┐
              │   Pianificatore (1)    │  struttura il piano
              └────────┬───────┬───────┘
                       │       │  paralleli
              ┌────────▼─┐  ┌──▼───────┐
              │  Prog. A  │  │  Prog. B  │  scrivono codice
              └────────┬─┘  └──┬────────┘
                       └───┬───┘
                           ▼
              ┌────────────────────────┐
              │  Tester (4) — max 3x   │  esegue + corregge
              └────────────┬───────────┘
                           ▼
              ┌────────────────────────┐
              │  Ottimizzatore (5)     │  refactor finale
              └────────────────────────┘
```

**Guardie pre-pipeline** — attive prima di avviare qualsiasi agente:

| Guardia | Trigger | Azione |
|---|---|---|
| Math locale | `"quanto fa 4+4?"` | Risposta istantanea, 0 token AI |
| Tipo task | task non sembra codice | Avviso utente |
| RAM check | > 75% RAM occupata | Avviso; blocco a > 92% |
| Pre-calcolo | espressioni nel task | Inietta `expr[=N]` nel prompt |

**Esecuzione Python sicura**: ogni esecuzione di codice Python generato dall'AI richiede conferma esplicita dell'utente (dialog modale). Anche l'auto-pip-install richiede approvazione separata.

---

## Motore Byzantino (anti-allucinazione)

Ispirato al [Problema dei Generali Bizantini](https://en.wikipedia.org/wiki/Byzantine_fault), verifica ogni affermazione critica con **quattro agenti logici indipendenti**:

```
┌─────────────┐     ┌──────────────────────────────────┐
│  A — Originale │──→│  B — Avvocato del Diavolo        │
│  genera     │     │  cerca errori, contraddizioni,   │
│  la risposta│     │  casi limite nel ragionamento     │
└─────────────┘     └──────────────────────────────────┘
       │                        │
       ▼                        ▼
┌─────────────┐     ┌──────────────────────────────────┐
│  C — Gemello│     │  D — Giudice                     │
│  verifica   │──→  │  T = (A ∧ C) ∧ ¬B_valido         │
│  da zero    │     │  Se B trova falle → incertezza   │
└─────────────┘     └──────────────────────────────────┘
```

**Regola**: se A e C concordano **E** B non trova errori validi → risposta confermata. Altrimenti l'incertezza viene dichiarata esplicitamente, mai nascosta.

---

## Classificatore query e think budget

`AiClient::classifyQuery()` categorizza ogni domanda per ottimizzare token e tempo:

| Categoria | Criteri | `num_predict` | `think` |
|---|---|---|---|
| `QuerySimple` | ≤ 30 char, no keyword complesse | 512 | `false` |
| `QueryAuto` | 30-200 char, neutre | default | non inviato |
| `QueryComplex` | > 200 char **o** keyword (spiega/analizza/codice…) | default × 2* | `true`* |

\* Solo per modelli think-capable: `qwen3`, `qwen3.5`, `deepseek-r1`, `qwen2.5`

**Think budget** — i modelli con blocco `<think>` consumano token nel ragionamento interno. Prismalux raddoppia automaticamente `num_predict` su `QueryComplex` per evitare risposte troncate.

---

## Motore Matematico Locale

Il parser ricorsivo valuta espressioni complesse in **microsecondi**, senza mai inviare un token all'AI:

```
  ✔  sin(pi/4) + sqrt(2) = 1.41421356     [0.003 ms]
  ✔  sconto 20% su 150 € → 120 €          [0.002 ms]
  ✔  primi tra 1 e 50 → 2 3 5 7 11...     [0.011 ms]
  ✔  gcd(48, 18) = 6                       [0.001 ms]
```

Operatori e funzioni supportate:

| Categoria | Funzioni |
|---|---|
| Costanti | `pi`, `e`, `phi` |
| Log/Exp | `ln`, `log` (base 10), `log2`, `log10`, `exp` |
| Trigonometria | `sin/seno`, `cos/coseno`, `tan/tangente`, `asin`, `acos`, `atan`, `atan2` |
| Iperboliche | `sinh`, `cosh`, `tanh` |
| Radici | `sqrt/radq`, `cbrt` + alias italiani |
| Arrotondamento | `abs`, `floor`, `ceil`, `round`, `trunc`, `sign` |
| Teoria num. | `gcd/mcd`, `lcm/mcm`, `pow`, `min`, `max` |

Precedenza: `()` > `^` > `*/%` > `+-`

---

## Simulatore Algoritmi (110 simulazioni)

**110 simulazioni, 13 categorie** — ogni algoritmo visualizzato passo per passo nella GUI:

| Categoria | N | Algoritmi inclusi |
|---|---|---|
| **Ordinamento** | 15 | Bubble, Selection, Insertion, Merge, Quick, Heap, Shell, Cocktail, Counting, Gnome, Comb, Pancake, Radix, Bucket, Tim |
| **Ricerca** | 4 | Binaria, Lineare, Jump, Interpolation |
| **Matematica** | 10 | Fibonacci DP, Crivello di Eratostene, Potenza Modulare, Pascal, Monte Carlo π, Torre di Hanoi, Miller-Rabin, Horner-Ruffini... |
| **Prog. Dinamica** | 5 | Zaino 0/1, LCS, Coin Change, Edit Distance, LIS |
| **Grafi** | 7 | BFS, Dijkstra, DFS, Topological Sort, Bellman-Ford, Floyd-Warshall, Kruskal |
| **Tecniche** | 3 | Two Pointers, Sliding Window, Bit Manipulation |
| **Visualizzazioni** | 3 | Game of Life, Triangolo di Sierpinski, Rule 30 |
| **Fisica** | 1 | Caduta libera con resistenza dell'aria |
| **Chimica** | 3 | pH, Gas ideali (PV=nRT), Stechiometria |
| **Stringhe** | 3 | KMP, Rabin-Karp, Z-Algorithm |
| **Strutture Dati** | 5 | Stack/Queue, Linked List, BST, Hash Table, Min-Heap |
| **Greedy** | 3 | Activity Selection, Zaino Frazionario, Codifica di Huffman |
| **Backtracking** | 2 | N-Regine (N=5), Sudoku (4×4) |

Ogni simulazione mostra: array/struttura passo per passo · complessità O-grande · descrizione teorica.

---

## RAG — Retrieval-Augmented Generation

Prismalux include un motore RAG locale che arricchisce le risposte AI con i tuoi documenti:

- **Formati supportati**: `.txt`, `.md`, `.pdf` (via `pdftotext`/Poppler), `.cpp`, `.py`, `.h`
- **Algoritmo**: chunking → embedding → JLT projection → cosine similarity
- **Modello embedding**: `nomic-embed-text` (Ollama) — configurabile da Impostazioni
- **Indicizzazione in background**: l'indicizzazione continua anche cambiando tab o finestra — guidata da `QNetworkReply` callbacks, non blocca l'UI
- **Stop cooperativo**: pulsante "⏹ Ferma indicizzazione" interrompe il processo in modo pulito preservando i chunk già elaborati
- **Counter affidabile**: il contatore "Documenti indicizzati" si aggiorna solo se almeno un embedding ha successo; fallimenti silenziosi mostrano avviso con hint su `nomic-embed-text`
- **Privacy**: l'indicizzazione avviene interamente in locale, nessun dato inviato a cloud
- **Documenti AdE**: pulsante "Scarica documenti AdE 2026" (730/fascicolo) integrato nella scheda RAG

Configurazione in Impostazioni → RAG:

```
Cartella documenti: Prismalux/RAG/        ← default aggiornato
Risultati per query: 5
JLT transform: on/off (riduzione dimensioni embedding)
No-persist: mantieni indice solo in memoria (modalità privacy)

[ ⏹  Ferma indicizzazione ]  [ 🔄 Reindicizza ora ]
```

La cartella `RAG/` nella root del progetto è la directory default per i documenti.  
Contiene ad esempio `2103.00564v1.pdf` — paper Johnson–Lindenstrauss Transforms, lo stesso
algoritmo usato internamente dal motore RAG per la proiezione degli embedding.

---

## Classifica LLM open-weight

Il tab **📊 Classifica** in Impostazioni mostra una ranking oggettiva di **22 modelli open-weight** testabili localmente, basata su:

- **ArtificialAnalysis Intelligence Index** (benchmark pubblici MMLU/GPQA/HumanEval)
- **Benchmark locali Prismalux** (`test_prompt_levels.py`, 4 livelli × 4 domande)

Filtri disponibili: RAM massima (≤8/16/32/64 GB) · categoria · ordinamento per score/RAM/velocità.

Modelli testati localmente (sessione 2026-04-15):

| Modello | Score Prismalux | Tempo medio | Note |
|---|---|---|---|
| `mistral:7b-instruct` | 66.2/100 | ~74s | Stabile, buona copertura keyword |
| `qwen3:4b` | 55.4/100 | ~71s | Con fix think budget (T6): ottimale |

Il pulsante **"ollama pull \<modello\>"** per ogni riga permette l'installazione diretta senza uscire dall'app.

---

## Backend AI supportati

Prismalux supporta **tre backend** intercambiabili a caldo:

| Backend | Descrizione | Configurazione |
|---|---|---|
| **Ollama** | Server locale, gestione automatica modelli | host + porta (default 127.0.0.1:11434) |
| **llama-server** | Server OpenAI-compatibile (llama.cpp) | host + porta personalizzabili |
| **llama statico** | Inferenza embedded, nessun server esterno | path binario + path modello .gguf |

Il modello scelto dall'utente viene **persistito su QSettings** e ripristinato automaticamente al prossimo avvio.

---

## Modelli consigliati

### Setup minimo (8 GB RAM)

```bash
ollama pull qwen3:4b           # ~3 GB — velocissimo, think nativo
ollama pull llama3.2-vision    # ~8 GB — unico modello vision stabile su Ollama
```

### Setup bilanciato (16-32 GB RAM)

```bash
ollama pull qwen3:8b           # ~5 GB — ottimo italiano, consigliato per uso quotidiano
ollama pull qwen2.5-coder:7b   # ~5 GB — top coding nella categoria 7B
ollama pull deepseek-r1:7b     # ~5 GB — ragionamento matematico, chain-of-thought nativo
```

### Setup avanzato (64 GB RAM — es. Xeon)

```bash
ollama pull qwen3:30b          # ~18 GB — qualità vicina ai modelli cloud
ollama pull deepseek-r1:32b    # ~20 GB — ragionamento matematico avanzato
ollama pull qwen2.5-coder:32b  # ~20 GB — miglior coding open-weight < 64 GB
ollama pull llama3.1:70b       # ~40 GB — massima qualità generale
```

> Su Xeon con GPU AMD lenta: `OLLAMA_NUM_GPU=0 ollama serve` per forzare CPU pura e sfruttare AVX-512.

---

## Test automatici

### Qt GUI (C++)

```bash
cd C_software/Qt_GUI
cmake -B build_tests -DBUILD_TESTS=ON
cmake --build build_tests -j$(nproc)
./build_tests/test_signal_lifetime   # lifecycle segnali Qt
./build_tests/test_rag_engine        # RAG retrieval precision/recall
./build_tests/test_code_utils        # extractPythonCode + _sanitizePyCode
```

| Suite Qt | Test | Cosa verifica |
|---|---|---|
| `test_signal_lifetime` | 36+ | Dangling observer, signal leakage, invariant violation checkbox |
| `test_rag_engine` | 15 | Chunking, embedding, search, save/load round-trip, stop cooperativo, multi-documento |
| `test_code_utils` | 14+ | Estrazione codice Python da risposta AI, sanitizzazione |

`test_rag_engine` include test su comportamento di sessione già indicizzata (test 11–15): verifica che l'indice caricato da disco produca gli stessi risultati dell'indice in memoria, che il top-k sia preservato, e che una reindicizzazione fallita non sovrascriva i dati validi.

### Test Python — cartella `Test/`

```bash
cd Prismalux/Test
python3 test_prompt_levels.py   # benchmark LLM multi-livello
python3 test_rag_paper.py       # domande sul paper JLT con simulazione RAG
python3 test_brutal_honesty.py  # verifica onestà AI su affermazioni false
python3 test_math_column_shift.py  # tracciamento variabili + somme colonne
```

Tutti i test Python condividono `test_utils.py` (helper `ask_ollama`, `bar`).

#### `test_prompt_levels.py` — 16 scenari (4 livelli × 4 domande)
- **L1**: utente con poca esperienza (prompt brevi, vaghi)
- **L2**: esperienza media (domande chiare, keyword tecniche)
- **L3**: alta esperienza (prompt strutturati, multi-step)
- **L4**: prompt engineering avanzato (zero-shot, chain-of-thought)

#### `test_rag_paper.py` — simulazione RAG completa

Testa il flusso RAG end-to-end su `RAG/2103.00564v1.pdf`:
1. Estrae testo dal PDF (`pdftotext` → fallback `pypdf`)
2. Seleziona chunk rilevanti per keyword scoring
3. Inietta il contesto nel system prompt
4. Verifica keyword nella risposta

Risultati (qwen2.5-coder:7b): **5/5 ✅ 100%**

#### Risultati test per modello

| Test | qwen2.5-coder:7b | Note |
|------|-----------------|------|
| `test_rag_paper` | **5/5 (100%)** | Con contesto RAG |
| `test_brutal_honesty` | **9/15 (60%)** | Verifica critica affermazioni |
| `test_math_column_shift` | **2/8 (25%)** | 5 timeout — tracciamento variabili limite |
| `test_prompt_levels` | 66.2/100 (mistral) | 55.4/100 (qwen3:4b) |

---

## Struttura del progetto

```
Prismalux/
│
├── C_software/
│   └── Qt_GUI/                     ← GUI Qt6 (v2.2)
│       ├── CMakeLists.txt           ← build (Qt6/Qt5 fallback, AUTOMOC)
│       ├── CLAUDE.md                ← architettura, convenzioni, ottimizzazioni
│       ├── main.cpp
│       ├── mainwindow.h/cpp         ← finestra principale (header · sidebar · stack)
│       ├── hardware_monitor.h/cpp   ← polling CPU/RAM/GPU
│       ├── ai_client.h/cpp          ← client Ollama/llama-server · classifyQuery · think budget
│       ├── rag_engine.h/cpp         ← RAG: chunking, JLT, cosine similarity
│       ├── theme_manager.h/cpp      ← carica/salva tema QSS
│       ├── chat_history.h/cpp       ← compressione history (summary + kMaxRecentTurns)
│       ├── prismalux_paths.h        ← unico punto di verità per percorsi e QSettings
│       ├── hw_detect.c/h            ← rilevamento GPU cross-platform (C puro)
│       │
│       ├── pages/
│       │   ├── agenti_page.*        ← entry point + coordinatore (12 moduli)
│       │   ├── agenti_page_ui.*     ← costruzione UI
│       │   ├── agenti_page_voice.*  ← TTS (Piper/SAPI) + STT (Whisper)
│       │   ├── agenti_page_exec.*   ← esecuzione Python con dialog conferma
│       │   ├── agenti_page_stream.* ← streaming NDJSON da Ollama
│       │   ├── agenti_page_pipeline.*  ← orchestrazione 6 agenti
│       │   ├── agenti_page_byzantine.* ← motore Byzantino A/B/C/D
│       │   ├── agenti_page_math.*   ← guardia math locale
│       │   ├── agenti_page_models.* ← selezione modello per ruolo
│       │   ├── agenti_page_files.*  ← allegati e contesto documenti
│       │   ├── agenti_page_bubbles.*   ← chat bubble render
│       │   ├── agenti_page_consiglio.* ← modalità Consiglio Scientifico
│       │   ├── oracolo_page.*       ← chat singola + history + RAG
│       │   ├── matematica_page.*    ← sequenze, costanti, N-esimo, espressioni
│       │   ├── grafico_canvas.*     ← GraficoCanvas: 45+ tipi, zoom/pan, PNG export
│       │   ├── grafico_page.*       ← GraficoPage: UI, plot(), analyzeImage()
│       │   ├── simulatore_algos.*   ← implementazioni 110 algoritmi (zero UI)
│       │   ├── simulatore_page.*    ← SimulatorePage UI + BigOWidget
│       │   ├── programmazione_page.*   ← editor + correzione AI + run Python
│       │   ├── strumenti_page.*     ← 730, P.IVA, mutuo, PAC, pensione, lavoro
│       │   ├── impara_page.*        ← tutor AI + quiz (tab unificato)
│       │   ├── materie_page.*       ← studio per materia
│       │   ├── impostazioni_page.*  ← tutti i tab impostazioni
│       │   └── agents_config_dialog.*  ← configurazione agenti personalizzata
│       │
│       ├── widgets/
│       │   ├── chat_bubble.h/cpp    ← bubble chat (sent/received/thinking)
│       │   ├── chart_widget.h/cpp   ← widget grafico embedding
│       │   ├── formula_parser.h/cpp ← parser LaTeX-like per formule
│       │   ├── spinner_widget.h     ← spinner braille animato
│       │   ├── status_badge.h       ← dot colorato stato (Offline/Online/Error)
│       │   ├── toggle_switch.h      ← toggle switch animato
│       │   ├── code_highlighter.h   ← syntax highlight codice
│       │   ├── python_exec.h        ← esecuzione Python sandboxed
│       │   └── whisper_autosetup.h  ← auto-download modello Whisper
│       │
│       ├── themes/
│       │   ├── dark_cyan.qss        ← tema default
│       │   ├── dark_amber.qss
│       │   ├── dark_purple.qss
│       │   ├── dark_ocean.qss
│       │   └── light.qss
│       │
│       └── tests/
│           ├── test_signal_lifetime.cpp
│           ├── test_rag_engine.cpp
│           ├── test_code_utils.cpp
│           └── mock_ai_client.h
│
├── RAG/                             ← documenti per il motore RAG
│   └── 2103.00564v1.pdf             ← paper Johnson–Lindenstrauss Transforms
│
└── Test/                            ← test Python automatici
    ├── test_utils.py                ← helper condiviso (ask_ollama, bar)
    ├── test_prompt_levels.py        ← benchmark LLM 4 livelli × 4 domande
    ├── test_rag_paper.py            ← simulazione RAG su paper JLT (5/5 ✅)
    ├── test_brutal_honesty.py       ← onestà AI su affermazioni false
    └── test_math_column_shift.py    ← tracciamento variabili multiple
```

---

## Requisiti

### Linux

| Dipendenza | Scopo | Installazione |
|---|---|---|
| `gcc` ≥ 9 / `clang` ≥ 10 | Compilazione | `apt install build-essential` |
| `Qt6 Base` | GUI | `apt install qt6-base-dev` |
| `cmake` ≥ 3.16 | Build system | `apt install cmake` |
| `Ollama` | Backend AI | `curl -fsSL https://ollama.com/install.sh \| sh` |
| `piper` + `aplay` | TTS Linux | opzionale, per lettura ad alta voce |
| `pdftotext` | RAG su PDF | `apt install poppler-utils` |
| `python3` | Esecuzione codice AI | già presente su Ubuntu |

### Windows

| Dipendenza | Note |
|---|---|
| [Ollama per Windows](https://ollama.com/download) | Backend AI |
| [MSYS2](https://www.msys2.org/) | Toolchain: `pacman -S mingw-w64-x86_64-qt6-base mingw-w64-x86_64-cmake mingw-w64-x86_64-gcc` |
| Windows SAPI | TTS integrato nel sistema, nessuna installazione richiesta |

---

## Argomenti e discipline trattate

### 💻 Informatica e Programmazione
- Pipeline AI multi-agente — Ricercatore, Pianificatore, Programmatori paralleli, Tester, Ottimizzatore
- Correzione codice con AI — analisi errori, refactoring, ottimizzazione, esecuzione sandbox
- 110 simulazioni algoritmi — ordinamento, ricerca, grafi, DP, greedy, backtracking
- RAG locale — risposte basate su documenti propri con JLT similarity search

### 🔢 Matematica
- Analisi numerica — Horner-Ruffini, Monte Carlo π, potenza modulare
- Calcolo — espressioni arbitrarie: `sin`, `cos`, `tan`, `ln`, `log`, `sqrt`, `cbrt`...
- Teoria dei numeri — crivello di Eratostene, numeri primi, Fibonacci, fattorizzazione
- Costanti ad alta precisione — π, e, φ, √2, γ (fino a 10.000 cifre via mpmath)
- Sequenze → Formula — rilevamento pattern + sympy per interpolazione

### 📊 Statistica e Finanza
- Dichiarazione 730 — guida interattiva con AI
- Partita IVA / Regime Forfettario — calcolo coefficienti, tasse, contributi INPS
- Mutuo — piano di ammortamento, rata mensile, interessi totali
- PAC — proiezioni rendimento, montante finale
- Pensione INPS — stima assegno, simulazioni contributive

### 🔬 Fisica e Chimica
- Caduta libera con resistenza dell'aria
- Gas ideali (PV = nRT), pH, stechiometria

### 🤖 AI e LLM
- Motore Byzantino — anti-allucinazione a 4 agenti (A/B/C/D)
- Classificazione query (Simple/Auto/Complex) + gestione think budget
- Classifica 22 modelli open-weight con score oggettivi e filtro per RAM
- RAG locale — indicizzazione PDF/TXT/MD con embedding Ollama
- TTS/STT — voce input/output completamente locale

---

<div align="center">

**Prismalux** è un progetto personale aperto. Contributi, segnalazioni e idee sono benvenuti.

*"La birra è conoscenza divina — ogni sorso un dato in più."* 🍺

[![GitHub](https://img.shields.io/badge/GitHub-wildlux%2FPrismalux-181717?style=flat-square&logo=github)](https://github.com/wildlux/Prismalux)

</div>
