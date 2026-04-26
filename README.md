<div align="center">

# 🍺 Prismalux

### *"Costruito per i mortali che aspirano alla saggezza."*

[![C++/Qt6](https://img.shields.io/badge/GUI-C%2B%2B%20%2F%20Qt6-green?style=flat-square&logo=qt)](https://www.qt.io/)
[![Version](https://img.shields.io/badge/versione-2.8-blue?style=flat-square)](CHANGELOG)
[![License](https://img.shields.io/badge/License-MIT-lightgrey?style=flat-square)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20Windows-informational?style=flat-square)](https://github.com/wildlux/Prismalux)

**Piattaforma AI locale. GUI in C++/Qt6.**  <br>
Multi-agente · anti-allucinazione · RAG in-page · matematica locale · 110 simulazioni algoritmi.  <br>
MCPs: Blender / Office / Anki / KiCAD / Arduino+ESP32 · Network Analyzer · Disegno→3D  <br>
Zero dipendenze cloud · Zero abbonamenti · Tutto locale sul tuo hardware

</div>

---

## Avvio rapido

```bash
git clone git@github.com:wildlux/Prismalux.git
cd Prismalux/gui
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)
./build/Prismalux_GUI
```

Prerequisiti: `Qt6`, `cmake`, `gcc/clang`, `Ollama` + almeno un modello.

```bash
curl -fsSL https://ollama.com/install.sh | sh
ollama pull qwen3:8b   # ~5 GB, ottimo italiano + think nativo
```

---

## Build

**Linux (apt):** `sudo apt install qt6-base-dev cmake build-essential`  
**Linux (pacman):** `sudo pacman -S qt6-base cmake gcc`  
**Windows (MSYS2 MINGW64):** `pacman -S --needed mingw-w64-x86_64-qt6-base mingw-w64-x86_64-cmake mingw-w64-x86_64-gcc`

**Con test:**
```bash
cmake -B build_tests -DBUILD_TESTS=ON && cmake --build build_tests -j$(nproc)
# test_signal_lifetime · test_rag_engine · test_code_utils
# test_lavoro_page · test_app_controller · test_programmazione_page
```

| Suite | Test | Cosa verifica |
|-------|------|---------------|
| `test_signal_lifetime` | 36+ | Dangling observer, signal leakage |
| `test_rag_engine` | 15 | Chunking, embedding, search, round-trip |
| `test_code_utils` | 14+ | Estrazione Python da risposta AI |
| `test_lavoro_page` | 37 | Isolamento AiClient, filtri, sincronizzazione modello |
| `test_app_controller` | 100+ | extractCode, state machine, Anki JSON, KiCAD, MCU |
| `test_programmazione_page` | 46 | isIntentionalError, parseNumbers, widget+segnali |

---

## Tab GUI

| # | Tab | Shortcut | Contenuto |
|---|-----|----------|-----------|
| 0 | 🤖 Intelligenza Artificiale | Alt+1 | Pipeline 6 agenti · Byzantino · CHAT RAG · TTS/STT |
| 1 | 🛠 Strumenti AI | Alt+2 | Studio, Scrittura, Ricerca, MCP |
| 2 | 💻 Programmazione | Alt+3 | Editor + correzione AI + Agentica (pipeline/RAG/refactor/test/debug/byzantino) |
| 3 | π Matematica | Alt+4 | Sequenze→Formula · espressioni locali · costanti alta precisione · 45+ grafici |
| 4 | 🕹 APP Controller | Alt+5 | Blender · Office · Anki · KiCAD · TinyMCP (Arduino/ESP32/STM32…) |
| 5 | 📚 Impara | Alt+6 | Tutor AI · Cerca Lavoro · 730/IVA/Mutuo/PAC/Pensione · Sfida |
| 6 | 🖥 OpenCode | — | opencode serve HTTP + SSE stream |
| ⚙️ | Impostazioni | — | Hardware · RAG · Classifica LLM · Voce · Aspetto |

---

## Funzionalità principali

### Pipeline Multi-Agente
```
Ricercatore → Pianificatore → Prog.A ┐
                                      ├─ Tester (max 3x) → Ottimizzatore
                              Prog.B ┘
```
Guardie pre-pipeline: math locale zero-AI · tipo task · RAM check · pre-calcolo espressioni.  
Esecuzione codice Python generato dall'AI richiede dialog di conferma esplicita.

### Motore Byzantino (anti-allucinazione)
4 agenti logici: **A** genera · **B** avvocato del diavolo · **C** verifica indipendente · **D** giudica.  
`T = (A ∧ C) ∧ ¬B_valido` — se B trova falle, l'incertezza viene dichiarata, mai nascosta.

### RAG locale
- Formati: `.txt`, `.md`, `.pdf`, `.cpp`, `.py`, `.h`
- Pipeline: chunking → embedding (`nomic-embed-text`) → JLT 256-dim → cosine similarity
- Indicizzazione in background · stop cooperativo · privacy totale (zero cloud)

### Motore Matematico Locale
Parser ricorsivo zero-AI in microsecondi: `sin/cos/tan/asin/acos/atan`, `ln/log/log2/exp`, `sqrt/cbrt`, `gcd/lcm`, `abs/floor/ceil/round`, costanti `pi/e/phi`.  
Precedenza: `()` > `^` > `*/%` > `+-`.

### Simulatore Algoritmi — 110 simulazioni, 13 categorie
Ordinamento (15) · Ricerca (4) · Matematica (10) · Prog.Dinamica (5) · Grafi (7) · Tecniche (3) · Strutture Dati (5) · Stringhe (3) · Greedy (3) · Backtracking (2) · Visualizzazioni (3) · Fisica (1) · Chimica (3).

### Classificatore query + think budget
| Categoria | Criteri | `num_predict` | `think` |
|-----------|---------|---------------|---------|
| `Simple` | ≤30 char, no keyword | 512 | false |
| `Auto` | 30-200 char | default | — |
| `Complex` | >200 char o keyword complesse | ×2* | true* |

\* Solo per modelli think-capable: `qwen3`, `qwen3.5`, `deepseek-r1`, `qwen2.5`.

### Modalità Calcolo LLM
Tre bottoni in Impostazioni → Hardware:

| Modalità | `num_gpu` | Comportamento |
|----------|-----------|---------------|
| **GPU** | `= N layer` (da `/api/show`) | tutti i layer su VRAM |
| **CPU** | `0` | tutto su RAM, GPU ignorata |
| **Misto GPU+CPU** | `= N/2 layer` | metà layer su GPU, metà su CPU |

La selezione viene salvata su disco e ripristinata all'avvio.  
**Nota VRAM 4 GB**: un modello da 3–3.5 GB è ai limiti (OS + KV cache aggiungono ~400–600 MB). Usare **Misto** se il modello non entra interamente nella VRAM.

---

## Backend AI

| Backend | Endpoint | Note |
|---------|----------|------|
| **Ollama** | `127.0.0.1:11434` | Gestione automatica modelli |
| **llama-server** | host+porta personalizzabili | OpenAI-compatibile (llama.cpp) |
| **llama statico** | path binario + .gguf | Embedded, nessun server |

Il modello scelto viene persistito su QSettings e ripristinato all'avvio.

---

## Modelli consigliati

```bash
# GPU 4 GB VRAM (es. GTX 1650 / RX 580)
ollama pull qwen3:4b            # ~2.5 GB — veloce, think nativo  ← entra in VRAM
# Se il modello non entra del tutto, usare modalità Misto in Impostazioni

# 8 GB RAM (solo CPU)
ollama pull qwen3:4b            # ~2.5 GB — veloce, think nativo
# 16-32 GB
ollama pull qwen3:8b            # ~5 GB — ottimo italiano (consigliato)
ollama pull deepseek-r1:7b      # ~5 GB — ragionamento matematico
ollama pull qwen2.5-coder:7b    # ~5 GB — top coding 7B
# 64 GB (es. Xeon)
ollama pull qwen3:30b           # ~18 GB — qualità vicina ai modelli cloud
ollama pull deepseek-r1:32b     # ~20 GB — ragionamento avanzato
# Su AMD GPU lenta: OLLAMA_NUM_GPU=0 ollama serve  (forza CPU + AVX-512)
```

---

## Struttura

```
Prismalux/
├── gui/          ← GUI C++/Qt6 (CMakeLists.txt, pages/, widgets/, themes/, tests/)
├── llama.cpp/    ← clone llama.cpp
├── models/       ← modelli GGUF
├── MCPs/         ← plugin MCP (blender_addon, office_bridge)
├── RAG/          ← documenti indicizzabili
├── Test/         ← test Python (benchmark, RAG, onestà AI, math)
├── scripts/      ← crea_appimage.sh, crea_zip_windows.py
├── hybrid_llm/   ← ricerca BLHM (Bayesian Hybrid LLM Model)
└── README.md · LICENSE · aggiorna.sh
```

---

## Requisiti

**Linux:** `qt6-base-dev cmake build-essential ollama` + opzionale: `piper aplay poppler-utils python3`  
**Windows:** Ollama + MSYS2 MINGW64 (`mingw-w64-x86_64-qt6-base cmake gcc`) + SAPI (TTS integrato nel sistema)

---

<div align="center">

*"La birra è conoscenza divina — ogni sorso un dato in più."* 🍺

[![GitHub](https://img.shields.io/badge/GitHub-wildlux%2FPrismalux-181717?style=flat-square&logo=github)](https://github.com/wildlux/Prismalux)

</div>
