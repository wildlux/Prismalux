# Prismalux — C Software

> *"Costruito per i mortali che aspirano alla saggezza."*

Applicazione AI in C puro con GUI Qt6 integrata. Nessuna dipendenza runtime oltre a gcc e Qt6.

---

## Avvio rapido

```bash
# Dal launcher root (controlla Ollama e avvia la GUI)
cd ~/Desktop/Prismalux
./Prismalux

# Oppure direttamente il binario Qt
./C_software/Qt_GUI/build/Prismalux_GUI

# Oppure la TUI da terminale
cd ~/Desktop/Prismalux/C_software
./prismalux --backend ollama
```

---

## Aggiornamento rapido (ricompila tutto)

```bash
cd ~/Desktop/Prismalux
./aggiorna.sh             # TUI + GUI Qt + ZIP Windows (~1s se già compilato)
./aggiorna.sh --tui       # solo TUI C
./aggiorna.sh --gui       # solo GUI Qt6
./aggiorna.sh --zip       # solo ZIP Windows
./aggiorna.sh --no-zip    # TUI + GUI, salta ZIP
```

Da Claude Code: `/aggiorna-prismalux`

---

## Struttura

```
Prismalux/
├── aggiorna.sh             ← ricompila TUI + GUI + ZIP in un comando
├── crea_zip_windows.py     ← genera Prismalux_Windows_full.zip
├── Prismalux_Windows_full.zip ← sorgenti pronti per build su Windows
├── Prismalux               ← launcher root (avvia GUI + check Ollama)
└── C_software/
    ├── src/                ← sorgenti TUI C (14 moduli)
    │   ├── main.c          ← menu principale, config JSON/TOON
    │   ├── multi_agente.c  ← pipeline 6 agenti + guardie pre-pipeline
    │   ├── strumenti.c     ← tutor AI, assistente 730/P.IVA, math locale
    │   ├── simulatore.c    ← 63 simulazioni, 13 categorie
    │   ├── ai.c            ← ai_chat_stream(), astrazione backend
    │   ├── http.c          ← client TCP, Ollama NDJSON, llama-server SSE
    │   ├── modelli.c       ← gestione modelli GGUF e HTTP
    │   ├── backend.c       ← profili backend, config globale
    │   ├── hw_detect.c     ← rilevamento CPU/GPU/RAM cross-platform
    │   ├── agent_scheduler.c ← scheduler Hot/Cold GGUF
    │   ├── prismalux_ui.c  ← header ANSI, box, barre risorse
    │   ├── output.c        ← salva output in ../esportazioni/
    │   ├── terminal.c      ← input raw, tasti speciali
    │   ├── quiz.c          ← stub (quiz interattivi, TODO)
    │   └── llama_wrapper.cpp ← wrapper C++ llama.cpp (solo build statica)
    ├── include/            ← header (14 file)
    ├── Qt_GUI/             ← GUI grafica Qt6 (auto-contenuta)
    │   ├── pages/          ← agenti_page, pratico_page, impara_page,
    │   │                      impostazioni_page, manutenzione_page, personalizza_page
    │   ├── build/
    │   │   └── Prismalux_GUI ← binario GUI già compilato
    │   └── build.sh        ← compila la GUI (richiede Qt6)
    ├── models/             ← file .gguf (backend statico)
    ├── Makefile
    ├── build.bat           ← compilazione Windows (doppio click)
    ├── build.sh            ← build con llama.cpp statico (Linux/Mac)
    └── Prismalux_GUI.desktop ← launcher desktop (doppio click)
```

---

## Compilazione

### TUI — solo Ollama / llama-server (nessuna dipendenza)

```bash
make          # equivale a: make http
./prismalux --backend ollama
```

### GUI Qt6

```bash
cd Qt_GUI
cmake -B build && cmake --build build -j$(nproc)
./build/Prismalux_GUI
```

### Build con llama.cpp statico (inferenza embedded)

```bash
./build.sh      # clona llama.cpp, compila con cmake, linka
make static
./prismalux     # nessun server necessario
```

### Windows

Doppio click su `build.bat` — trova gcc automaticamente (MSYS2 / WinLibs / Scoop).
Il file `Prismalux_Windows_full.zip` contiene tutti i sorgenti pronti per la build.
Richiede Ollama per Windows: https://ollama.com/download

---

## Backend AI

| Backend | Avvio | Requisito |
|---|---|---|
| `ollama` | `./prismalux --backend ollama` | `ollama serve` attivo |
| `llama-server` | `./prismalux --backend llama-server --port 8080` | llama-server attivo |
| `llama` (statico) | `./prismalux` | `./build.sh` + file `.gguf` in `models/` |

---

## Funzionalità principali

### 1. Pipeline Multi-Agente (6 agenti)

```
Ricercatore → Pianificatore → Programmatore A ─┐ (paralleli)
                            → Programmatore B ─┘
             → Tester (loop correzione, max 3) → Ottimizzatore
```

**Guardie pre-pipeline** — attive prima di avviare qualsiasi agente:

| Guardia | Comportamento |
|---|---|
| Math locale | "quanto fa 4+4?" → risposta istantanea, 0 token AI |
| Tipo task | avvisa se il task non sembra programmazione |
| RAM | blocca se RAM > 92%, avvisa a 75–92% |
| Pre-calcolo espressioni | `_inject_math()` valuta le espressioni numeriche nel task e le inietta come `expr[=N]` prima di inviare all'AI |
| Traduzione automatica | `_is_likely_english()` — input inglese viene tradotto in italiano via AI prima della pipeline |

### 2. Motore Byzantino (anti-allucinazione)

Verifica a 4 agenti logici per domande critiche:
- **A** Generatore Originale
- **B** Avvocato del Diavolo (cerca contraddizioni)
- **C** Gemello Indipendente (verifica senza vedere A)
- **D** Giudice: `T = (A ∧ C) ∧ ¬B_valido`

### 3. Simulatore Algoritmi (63 simulazioni, 13 categorie)

| Categoria | N | Algoritmi |
|---|---|---|
| Ordinamento | 15 | bubble, selection, insertion, merge, quick, heap, shell, cocktail, counting, gnome, comb, pancake, radix, bucket, tim |
| Ricerca | 4 | binaria, lineare, jump, interpolation |
| Matematica | 9 | fibonacci, crivo, potenza_mod, pascal, monte_carlo_pi, hanoi, miller_rabin... |
| Prog. Dinamica | 5 | zaino, lcs, coin_change, edit_distance, lis |
| Grafi | 7 | BFS, Dijkstra, DFS, topo_sort, Bellman-Ford, Floyd-Warshall, Kruskal |
| Tecniche | 3 | two_pointers, sliding_window, bit_manipulation |
| Visualizzazioni | 3 | Game of Life, Sierpinski, Rule 30 |
| Fisica | 1 | caduta libera |
| Chimica | 3 | pH, gas ideali, stechiometria |
| Stringhe | 3 | KMP, Rabin-Karp, Z-algorithm |
| Strutture Dati | 5 | stack/queue, linked_list, BST, hash_table, min_heap |
| Greedy | 3 | activity_selection, zaino frazionario, Huffman |
| Backtracking | 2 | N-regine (N=5), Sudoku (4×4) |

Menu: `1`–`9` categorie base, `a`=Stringhe, `b`=Strutture Dati, `c`=Greedy, `d`=Backtracking

### 4. Tutor AI (zero token per risposte locali)

- Domande su Prismalux → risposta locale istantanea
- Matematica semplice → parser ricorsivo locale con tempo di esecuzione
- Tutto il resto → AI con system prompt minimale

### 5. Strumento Pratico

- Assistente Dichiarazione 730
- Assistente Partita IVA / Regime Forfettario

### 6. GUI Qt6 — Tab principali

| Tab | Contenuto |
|---|---|
| 🤖 Agenti AI | Pipeline 6 agenti, Motore Byzantino, Consiglio Scientifico, allegati doc/immagini |
| 🔮 Oracolo | Chat singola con AI, streaming |
| 💰 Finanza | Assistente 730, Partita IVA, Mutuo, PAC, Pensione INPS |
| 📚 Impara | Tutor AI, quiz, simulatore algoritmi |
| 🎯 Sfida te stesso! | Quiz interattivi |
| 🛠 Strumenti AI | Studio, Scrittura Creativa, Ricerca, Libri, Produttività |

### 7. GUI Qt6 — Impostazioni (tab piatte)

| Tab | Contenuto |
|---|---|
| 🔌 Backend | Selezione backend (Ollama/llama-server), host/porta, avvia llama-server |
| 🖥️ Hardware | Info CPU/GPU/RAM + ottimizzazione zRAM |
| 🔬 VRAM Bench | Benchmark consumo VRAM per modello |
| 🦙 llama.cpp | Compila/aggiorna, gestisci modelli GGUF, scarica modelli matematica |
| 📊 Monitor | Monitor attività AI in tempo reale |

### 8. Protezione RAM

- **Emergenza RAM** 🚨 nell'header: scarica tutti i modelli Ollama + libera cache kernel
- **Scarica LLM** 💾: scarica il modello corrente da RAM (keep_alive=0)
- **checkRam()** in ogni pipeline: blocca a ≥92%, avvisa a 75–92%

---

## Calcoli locali — output esempio

```
  ✔  4+4 = 8
  Tempo: 0.003 ms (0.000003 s)

  ✔  sconto 20% su 150
  Risparmio: 30   Prezzo finale: 120
  Tempo: 0.002 ms (0.000002 s)

  ✔  primi 10 numeri primi
  2 3 5 7 11 13 17 19 23 29
  Tempo: 0.011 ms (0.000011 s)
```

---

## Agent Scheduler (Hot/Cold)

Per la build statica con llama.cpp, il scheduler gestisce il carico GPU:
- Profilo VRAM per modello (`vram_profile.json`) generato da `./vram_bench`
- Agenti `sticky-hot` restano in VRAM, altri vengono evicti
- `n_gpu_layers` calcolato per agente in base a VRAM disponibile

```bash
make vram_bench        # profila tutti i modelli Ollama
./vram_bench           # genera vram_profile.json
```

---

## Config persistente

`~/.prismalux_config.json` — salva backend attivo e profili per-backend:

```json
{
  "backend": "ollama",
  "ollama_host": "127.0.0.1",
  "ollama_port": 11434,
  "ollama_model": "qwen2.5-coder:7b"
}
```

Formato alternativo `~/.prismalux_config.toon` (flat key:value, -12% dimensione).
Convertibile dalla tab ⚙️ Config nella GUI.

---

## Test automatici

```bash
make test                              # 18/18 test engine (TCP, JSON, AI, python3)
make test_multiagente && ./test_multiagente   # 8/8 pipeline end-to-end
make test_sim_smoke && ./test_sim_smoke       # 66/66 smoke test simulatore
```

Suite completa (tutti i target):

| File | Pass | Categoria |
|---|---|---|
| `test_engine` | 18/18 | Core TUI: TCP, JSON, stream AI |
| `test_multiagente` | 8/8 | Pipeline 6-agenti end-to-end |
| `test_buildsys` | 36/36 | Qt GUI: `_buildSys` prompt per famiglia LLM |
| `test_toon_config` | 55/55 | Parser .toon + persistenza config |
| `test_stress` | 74/74 | API pubblica: JSON, buffer, Unicode, TUI |
| `test_cs_random` | 17/17 | Context Store: precision/recall/stress |
| `test_guardia_math` | 93/93 | Parser `_gp_*`: guardia pre-pipeline |
| `test_math_locale` | 120/120 | Parser `_mp_*`: motore Tutor |
| `test_agent_scheduler` | 87/87 | Hot/Cold scheduler completo |
| `test_perf` | 26/26 | Timing TTFT, throughput, cold start |
| `test_sha256` | 20/20 | SHA-256 puro C, integrità file .gguf |
| `test_golden` | 53/53 | Regression AI: keyword/forbidden/lingua |
| `test_memory` | 12/12 | Memory leak detection via /proc/self/status |
| `test_rag` | 30/30 | RAG retrieval precision/recall |
| `test_sim_smoke` | 66/66 | Smoke test: tutte le 63+ simulazioni, no crash, output ≥10 char, timeout 5s |

**Totale: 715/715**

---

## Completati — storico decisioni tecniche

### Qt GUI — Impostazioni (da 11 tab a 6 tab tematiche)

Riduzione da 11 tab piatte a 6 raggruppate logicamente:

| Tab nuova | Contenuto unificato |
|---|---|
| 🔌 Connessione | Backend + LLM Consigliati |
| 🖥 Hardware | Info CPU/GPU/RAM + VRAM Bench |
| 🦙 AI Locale | llama.cpp Studio + Dipendenze |
| 🎤 Voce & Audio | Piper TTS + Whisper STT |
| 🎨 Aspetto | Tema (dark/amber/purple/light) |
| 📊 Avanzate | Monitor AI + RAG + Test Suite |

`switchToTab("LLM")` aliasato a "Connessione" — link esistenti da `grafico_page.cpp` e `agenti_page.cpp` invariati.

### Qt GUI — Bug risolti (2026-03-30)

| Bug | File | Fix |
|---|---|---|
| `m_hwLabel` non inizializzato a `nullptr` | `manutenzione_page.h:26` | Aggiunto `= nullptr` — guard sicura contro valori garbage |
| `buildModelBar()` porta 8080 hardcoded | `impostazioni_page.cpp` | Sostituito con `P::kLlamaServerPort` |
| Typo `buildCythoStudio` | `personalizza_page.cpp` | Rinominato `buildCythonoStudio` |
| Crash Impostazioni (Windows API parent-child) | `mainwindow.cpp` | Rimosso `Qt::Window` su QDialog con parent; deduplica con `ensureSettingsDialog()` |
| Stack overflow `refreshModels` | `personalizza_page.cpp` | Sostituito `std::function` auto-referenziale con `shared_ptr<function>` |

### Test Smoke Simulatore (2026-03-31)

`test_sim_smoke.c` — esegue ogni simulazione in un processo figlio isolato:
- stdin → `/dev/null` (getch_single ritorna EOF, sim avanza senza bloccarsi)
- stdout → pipe (cattura output, verifica ≥10 char)
- timeout 5s per simulazione via `select()` + `gettimeofday()`
- `g_sim_headless = 1` nel child: salta `nanosleep(200ms)` in `cpu_percent()` — evita timeout per algoritmi O(n²) con molti step
- `signal(SIGPIPE, SIG_IGN)` nel child: gestisce simulazioni che generano >64KB senza crash

### TODO — Miglioramenti da implementare

- [x] **Avviso dimensione modello** — `checkModelSize()` in AgentiPage: dialog se peso > 70% RAM libera
- [x] **Richieste AI sovrapposte** — `navigateTo()` chiama `m_ai->abort()` prima di cambiare pagina
- [x] **Indicatore "⏳ Elaborazione"** — `waitLbl` aggiunto in ImparaPage e PraticoPage
- [x] **Quiz: timeout generazione AI** — `alarm(30)` + `SIGALRM` con retry fino a 3 tentativi; dopo 3 fallimenti → messaggio esplicativo e uscita (TUI C)

---

## Confronto Python vs C

| | Python | C + Qt |
|---|---|---|
| Dipendenze | Python + pip + 6 librerie + venv | gcc + Qt6 |
| Avvio TUI | ~2-3 secondi | istantaneo |
| Avvio GUI | ~1-2 secondi | ~0.3 secondi |
| Backend AI | solo Ollama | Ollama + llama-server + llama statico |
| Multi-agente | backend unico | backend diverso per ogni agente |
| Math locale | no | sì (parser ricorsivo, ~0.003 ms) |
| Motore Byzantino | no | sì (A/B/C/D, 4 agenti logici) |
| Pre-calcolo espressioni | no | sì (`_inject_math()` + `_math_sys()`) |
| Traduzione automatica input | no | sì (`_is_likely_english()` + `OpMode::Translating`) |
| Simulatore | ~120 algoritmi Python | 63 simulazioni C visive (13 categorie) |
| Impostazioni GUI | — | 7 tab piatte, nessun menu intermedio |
