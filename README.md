<div align="center">

<img src="https://img.shields.io/badge/Prismalux-AI%20Platform-cyan?style=for-the-badge&logo=data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAyNCAyNCI+PHBhdGggZmlsbD0iI2ZmZiIgZD0iTTEyIDJMMyA3bDkgNSA5LTV6TTMgMTdsOSA1IDktNXYtNWwtOSA1LTktNXoiLz48L3N2Zz4=" alt="Prismalux"/>

# Prismalux

### *"Costruito per i mortali che aspirano alla saggezza."*

[![C](https://img.shields.io/badge/C-pure-blue?style=flat-square&logo=c)](https://en.wikipedia.org/wiki/C_(programming_language))
[![Qt6](https://img.shields.io/badge/Qt-6-green?style=flat-square&logo=qt)](https://www.qt.io/)
[![Python](https://img.shields.io/badge/Python-3.x-yellow?style=flat-square&logo=python)](https://www.python.org/)
[![License](https://img.shields.io/badge/License-MIT-lightgrey?style=flat-square)](LICENSE)
[![Tests](https://img.shields.io/badge/Tests-897%2F897%20%E2%9C%85-brightgreen?style=flat-square)](#test-automatici)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20Windows-informational?style=flat-square)](https://github.com/wildlux/Prismalux)

**Piattaforma AI locale in C puro + GUI Qt6.**  
Multi-agente, anti-allucinazione, matematica locale, simulatore algoritmi.  
Zero dipendenze cloud. Zero abbonamenti. Gira tutto sul tuo hardware.

</div>

---

## Indice

- [Cos'è Prismalux](#cosè-prismalux)
- [Funzionalità principali](#funzionalità-principali)
- [Avvio rapido](#avvio-rapido)
- [Compilazione](#compilazione)
- [Backend AI](#backend-ai)
- [GUI Qt6](#gui-qt6)
- [Pipeline Multi-Agente](#pipeline-multi-agente)
- [Motore Byzantino](#motore-byzantino-anti-allucinazione)
- [Simulatore Algoritmi](#simulatore-algoritmi)
- [Motore Matematico Locale](#motore-matematico-locale)
- [Test automatici](#test-automatici)
- [Struttura del progetto](#struttura-del-progetto)

---

## Cos'è Prismalux

Prismalux è un **assistente AI locale** scritto interamente in C, con una GUI moderna in Qt6 e un motore Python opzionale. Nasce dall'idea che l'intelligenza artificiale debba girare sul tuo hardware, rispettando la tua privacy, senza connessioni a cloud esterni.

```
  ╔══════════════════════════════════════════════════════════╗
  ║  🍺  PRISMALUX  —  AI Platform                         ║
  ║  Backend: Ollama  ·  Modello: qwen2.5-coder:7b         ║
  ║  CPU: ████████░░ 78%  RAM: █████░░░░░ 52%  GPU: 3.2GB  ║
  ╚══════════════════════════════════════════════════════════╝
```

### Perché C puro?

| | Python | **Prismalux (C + Qt6)** |
|---|---|---|
| Avvio TUI | ~2–3 secondi | **istantaneo** |
| Avvio GUI | ~1–2 secondi | **~0.3 secondi** |
| Dipendenze | Python + pip + 6 librerie + venv | **gcc + Qt6** |
| Math locale | no | **sì (~0.003 ms, zero token AI)** |
| Multi-backend | solo Ollama | **Ollama + llama-server + llama static** |
| Motore Byzantino | no | **sì (4 agenti logici anti-allucinazione)** |

---

## Funzionalità principali

### 🤖 Pipeline Multi-Agente (6 agenti)
Ogni agente ha un ruolo distinto. I Programmatori A e B lavorano **in parallelo**. Il Tester esegue e corregge il codice in loop (max 3 tentativi).

```
Ricercatore → Pianificatore → Programmatore A ─┐ (paralleli)
                            → Programmatore B ─┘
             → Tester (loop correzione) → Ottimizzatore
```

### 🛡 Motore Byzantino (anti-allucinazione)
Quattro agenti logici verificano ogni risposta critica prima di presentarla:

```
A (Generatore) ──→ B (Avvocato del Diavolo, cerca errori)
                ──→ C (Gemello Indipendente, verifica separata)
                ──→ D (Giudice): T = (A ∧ C) ∧ ¬B_valido
```

### ⚡ Math locale zero-token
Il parser ricorsivo risponde istantaneamente senza consumare token AI:

```
  ✔  sin(pi/4) + sqrt(2) = 1.41421356
  Tempo: 0.003 ms

  ✔  sconto 20% su 150 € → 30 € di risparmio, prezzo finale: 120 €
  Tempo: 0.002 ms

  ✔  primi tra 1 e 50 → 2 3 5 7 11 13 17 19 23 29 31 37 41 43 47
  Tempo: 0.011 ms
```

Supporta: `sin/cos/tan`, `ln/log/log2`, `sqrt/cbrt`, `pi/e/phi`, `gcd/lcm`, `floor/ceil/round`, alias italiani ("seno", "radice quadrata di", "logaritmo naturale di")...

### 🎓 Simulatore Algoritmi (66 simulazioni, 13 categorie)
Ogni algoritmo è visualizzato **passo per passo** nel terminale con animazioni ANSI.

### 📊 GUI Qt6 moderna
8 tab tematiche, dark theme, monitor hardware live, grafici interattivi, streaming AI.

---

## Avvio rapido

```bash
# Clona il repository
git clone git@github.com:wildlux/Prismalux.git
cd Prismalux

# Avvia la GUI Qt6 (richiede Ollama attivo)
./Prismalux

# Oppure la TUI da terminale
cd C_software
make && ./prismalux --backend ollama
```

> **Prerequisiti**: `gcc`, `Qt6`, `Ollama` con almeno un modello installato.
> Per Ollama: `curl -fsSL https://ollama.com/install.sh | sh && ollama pull qwen2.5-coder:7b`

---

## Compilazione

### TUI — solo Ollama / llama-server (zero dipendenze extra)

```bash
cd C_software
make              # equivale a: make http
./prismalux --backend ollama
```

### GUI Qt6

```bash
cd C_software
./aggiorna.sh --gui                     # usa lo script aggiorna
# oppure manualmente:
cd Qt_GUI && cmake -B build && cmake --build build -j$(nproc)
./build/Prismalux_GUI
```

### Build con llama.cpp statico (inferenza completamente embedded)

```bash
cd C_software
./build.sh        # clona llama.cpp, compila con CMake, linka tutto
make static
./prismalux       # nessun server Ollama necessario
```

### Aggiornamento rapido (ricompila tutto in ~1s se già compilato)

```bash
./aggiorna.sh             # TUI + GUI Qt6 + ZIP Windows
./aggiorna.sh --tui       # solo TUI C
./aggiorna.sh --gui       # solo GUI Qt6
./aggiorna.sh --zip       # solo ZIP Windows
```

### Windows

Scarica `Prismalux_Windows_full.zip` dalla sezione [Releases](https://github.com/wildlux/Prismalux/releases), estrai e fai doppio click su `build.bat`.

Requisiti Windows:
- [Ollama per Windows](https://ollama.com/download)
- [MSYS2](https://www.msys2.org/) con `gcc`, `cmake`, `make` — oppure WinLibs/Scoop

---

## Backend AI

Prismalux supporta **tre backend** intercambiabili a caldo dalla GUI o da riga di comando:

| Backend | Descrizione | Comando |
|---|---|---|
| **Ollama** | Server locale, auto-gestisce i modelli | `./prismalux --backend ollama` |
| **llama-server** | Server OpenAI-compatibile, modello fisso | `./prismalux --backend llama-server` |
| **llama statico** | Inferenza embedded, nessun server | `./prismalux` (richiede `./build.sh`) |

La configurazione viene salvata automaticamente in `~/.prismalux_config.json`:

```json
{
  "backend": "ollama",
  "ollama_host": "127.0.0.1",
  "ollama_port": 11434,
  "ollama_model": "qwen2.5-coder:7b"
}
```

---

## GUI Qt6

L'interfaccia grafica è organizzata in **8 tab** accessibili con `Alt+1` – `Alt+8`:

| Tab | Shortcut | Contenuto |
|---|---|---|
| 🤖 **Agenti AI** | `Alt+1` | Pipeline 6 agenti · Motore Byzantino · Consiglio Scientifico |
| 🔮 **Oracolo** | `Alt+2` | Chat singola streaming con qualsiasi modello |
| 📐 **Matematica** | `Alt+3` | Sequenza→Formula · Costanti precisione arbitraria · N-esimo · Espressione |
| 📊 **Grafico** | `Alt+4` | 40+ tipi di grafico, dati in testo libero |
| 💻 **Programmazione** | `Alt+5` | Editor + correzione AI, nome modello in header |
| 💰 **Finanza** | `Alt+6` | Assistente 730 · Partita IVA · Mutuo · PAC · Pensione INPS |
| 📚 **Impara** | `Alt+7` | Tutor AI · Simulatore algoritmi · Quiz |
| ⚙️ **Impostazioni** | — | Backend · Hardware · llama.cpp · Monitor · Tema |

### Tab Matematica — dettaglio

```
┌─────────────────────────────────────────────────────────────┐
│  🔢 Sequenza  │  π φ e √ const  │  N-esimo  │ Espressione  │
├─────────────────────────────────────────────────────────────┤
│  1, 1, 2, 3, 5, 8, 13 ...   con: [qwen2.5-coder ▼] [🔄]   │
│  📂 Apri file (TXT/CSV/XLSX/DOCX/PDF)  [Analizza con AI]   │
│  ✔ Pattern rilevato: successione di Fibonacci               │
└─────────────────────────────────────────────────────────────┘
```

Supporta import di sequenze da: `.txt`, `.csv`, `.xlsx`, `.xls`, `.docx`, `.doc`, `.pdf`

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
| Auto-traduzione | input in inglese | Traduce in italiano via AI |

---

## Motore Byzantino (anti-allucinazione)

Ispirato al [Problema dei Generali Bizantini](https://en.wikipedia.org/wiki/Byzantine_fault), il motore verifica ogni affermazione critica con **quattro agenti logici indipendenti**:

```
┌─────────────┐     ┌──────────────────────────────────┐
│  A — Originale │──→│  B — Avvocato del Diavolo        │
│  genera     │     │  cerca errori, contraddizioni,   │
│  la risposta│     │  casi limite nel ragionamento     │
└─────────────┘     └──────────────────────────────────┘
       │                        │
       ▼                        ▼
┌─────────────┐     ┌──────────────────────────────────┐
│  C — Gemello │     │  D — Giudice                     │
│  verifica   │──→  │  T = (A ∧ C) ∧ ¬B_valido         │
│  da zero    │     │  Se B trova falle → incertezza    │
└─────────────┘     └──────────────────────────────────┘
```

**Regola**: se A e C concordano **E** B non trova errori validi → risposta confermata. Altrimenti l'incertezza viene dichiarata esplicitamente, mai nascosta.

---

## Simulatore Algoritmi

**66 simulazioni, 13 categorie** — ogni algoritmo visualizzato passo per passo:

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

---

## Motore Matematico Locale

Il parser ricorsivo (`_gp_prim` / `_mp_prim`) valuta espressioni complesse in **microsecondi**, senza mai inviare un token all'AI:

```
Costanti    pi, e, phi
Log/Exp     ln, log (base 10), log2, log10, exp
Trig        sin/seno, cos/coseno, tan/tangente
            asin/arcoseno, acos/arcocoseno, atan/arcotangente, atan2
Iperboliche sinh, cosh, tanh
Radici      sqrt/radq, cbrt  +  "radice quadrata di X", "radice cubica di X"
Log it.     "logaritmo naturale di X"  →  ln(X)
Arrot.      abs, floor, ceil, round, trunc, sign
Teoria num. gcd/mcd, lcm/mcm, pow
Min/Max     min(a,b), max(a,b)
```

**Precedenza**: `()` > `^` > `*/%` > `+-`

---

## Test automatici

Suite completa: **897/897 test superati** (nessun test saltato, nessun fallimento):

```bash
cd C_software
make test                          # 18/18 — Core TUI
make test_multiagente && ./test_multiagente   # 8/8  — Pipeline end-to-end
make test_sim_smoke && ./test_sim_smoke       # 66/66 — Smoke test simulatore
make test_hw_platform && ./test_hw_platform  # 28/28 — Rilevamento hardware
```

| Suite | Pass | Cosa verifica |
|---|---|---|
| `test_engine` | 18/18 | TCP, JSON, stream AI, python3 |
| `test_guardia_math` | 200/200 | Parser `_gp_*`: 200 espressioni math |
| `test_math_locale` | 120/120 | Parser `_mp_*`: tutor math |
| `test_golden` | 53/53 | Regression AI: keyword/forbidden/lingua |
| `test_toon_config` | 55/55 | Config .toon + persistenza |
| `test_memory` | 12/12 | Memory leak via `/proc/self/status` |
| `test_sha256` | 20/20 | SHA-256 puro C, integrità .gguf |
| `test_version` | 35/35 | Versioni, semver, changelog |
| `test_stress` | 74/74 | JSON, buffer, Unicode, API pubblica |
| `test_perf` | 26/26 | Timing TTFT, throughput, cold start |
| `test_cs_random` | 0 fail | Context Store: precision/recall/stress |
| `test_agent_scheduler` | 87/87 | Hot/Cold scheduler completo |
| `test_rag` | 30/30 | RAG retrieval precision/recall |
| `test_multiagente` | 8/8 | Pipeline 6 agenti end-to-end |
| `test_sim_smoke` | 66/66 | Tutte le 66 simulazioni, no crash, timeout 5s |
| `test_hw_platform` | 28/28 | CPU/GPU/RAM, nvidia-smi, thread |

---

## Struttura del progetto

```
Prismalux/
├── Prismalux               ← launcher root (avvia GUI + check Ollama)
├── aggiorna.sh             ← ricompila TUI + GUI + ZIP in un comando
├── crea_zip_windows.py     ← genera il pacchetto Windows
│
├── C_software/
│   ├── src/                ← sorgenti TUI C (14+ moduli)
│   │   ├── main.c          ← menu principale, config JSON/TOON
│   │   ├── multi_agente.c  ← pipeline 6 agenti + 3 guardie pre-pipeline
│   │   ├── strumenti.c     ← tutor AI, 730/P.IVA, math locale
│   │   ├── simulatore.c    ← 66 simulazioni, 13 categorie
│   │   ├── ai.c            ← ai_chat_stream(), astrazione backend
│   │   ├── http.c          ← TCP raw, Ollama NDJSON, llama-server SSE
│   │   ├── hw_detect.c     ← CPU/GPU/RAM cross-platform
│   │   ├── agent_scheduler.c ← scheduler Hot/Cold GGUF
│   │   ├── rag.c / rag_embed.c ← Retrieval-Augmented Generation
│   │   ├── jlt.c / jlt_index.c ← JLT similarity search
│   │   └── llama_wrapper.cpp   ← wrapper C++ llama.cpp (build statica)
│   │
│   ├── include/            ← header (14+ file)
│   ├── Qt_GUI/             ← GUI Qt6 (auto-contenuta)
│   │   ├── pages/          ← agenti, oracolo, matematica, grafico,
│   │   │                      programmazione, finanza, impara, impostazioni
│   │   ├── widgets/        ← spinner, chart, formula parser, chat bubble
│   │   ├── themes/         ← dark-cyan, amber, purple, light
│   │   └── build.sh        ← compila la GUI (richiede Qt6)
│   ├── rag_docs/           ← documenti per RAG locale
│   ├── models/             ← file .gguf (backend statico)
│   ├── Makefile
│   └── build.bat           ← build Windows (doppio click)
│
└── Python_Software/        ← componenti Python opzionali
    ├── AVVIA_Prismalux.py  ← launcher Python (TUI Textual)
    ├── multi_agente/       ← motore Byzantino Python
    └── tests/              ← test Python
```

---

## Requisiti

### Linux
- `gcc` ≥ 9 o `clang` ≥ 10
- `Qt6` (base + charts + multimedia): `sudo apt install qt6-base-dev`
- `Ollama`: `curl -fsSL https://ollama.com/install.sh | sh`
- `python3` + `mpmath` + `sympy` (solo per tab Matematica avanzata)

### Windows
- [Ollama per Windows](https://ollama.com/download)
- [MSYS2](https://www.msys2.org/) — `pacman -S mingw-w64-x86_64-gcc cmake make`
- Qt6 per Windows (opzionale, per la GUI)

---

## Configurazione AI consigliata

| Uso | Modello consigliato | Dimensione |
|---|---|---|
| Codice (veloce) | `qwen2.5-coder:7b` | ~4 GB |
| Codice (preciso) | `deepseek-coder:33b` | ~19 GB |
| Ragionamento | `deepseek-r1:7b` | ~4 GB |
| Matematica | `qwen2.5-math:7b` | ~4 GB |
| Chat generale | `llama3.2:3b` | ~2 GB |

```bash
ollama pull qwen2.5-coder:7b   # installazione consigliata per iniziare
```

---

<div align="center">

**Prismalux** è un progetto personale aperto. Contributi, segnalazioni e idee sono benvenuti.

*"La birra è conoscenza divina — ogni sorso un dato in più."* 🍺

</div>
