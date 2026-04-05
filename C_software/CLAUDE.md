# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build commands

```bash
# ── Aggiornamento rapido (TUI + GUI + ZIP) ──────────────────────
cd ~/Desktop/Prismalux
./aggiorna.sh            # tutto in un comando (~1s se già compilato)
./aggiorna.sh --tui      # solo TUI C
./aggiorna.sh --gui      # solo GUI Qt6
./aggiorna.sh --zip      # solo ZIP Windows
./aggiorna.sh --no-zip   # TUI + GUI, salta ZIP

# ── TUI — HTTP only (Ollama + llama-server), solo gcc ───────────
make              # or: make http
./prismalux --backend ollama

# ── GUI Qt6 ─────────────────────────────────────────────────────
cd Qt_GUI && ./build.sh
# oppure: cmake --build build -j$(nproc)
./Qt_GUI/build/Prismalux_GUI

# ── Launcher root (avvia GUI, controlla Ollama) ─────────────────
../Prismalux          # script in ~/Desktop/Prismalux/

# ── Build con llama.cpp statico (one-time setup, ~10-15 min) ────
./build.sh        # clones llama.cpp, runs cmake, links everything
make static       # requires build.sh to have run first

# ── Other targets ───────────────────────────────────────────────
make vram_bench          # Ollama VRAM profiler (standalone)
make vram_bench_static   # same with embedded llama.cpp
make windows             # produces prismalux.exe + vram_bench.exe (MinGW/MSYS2)
make clean

# ── Test automatici ─────────────────────────────────────────────
make test                  # test_engine: TCP, JSON, ai_chat_stream, python3 (18/18)
make test_multiagente && ./test_multiagente  # pipeline end-to-end (8/8)
```

Validate manually with `make http && ./prismalux --backend ollama`.
After structural changes: `make clean && make`.

### Slash command Claude Code
`/aggiorna-prismalux` — esegue `aggiorna.sh` direttamente dalla chat Claude Code.
Definito in `~/.claude/commands/aggiorna-prismalux.md`.

## Architecture

C TUI + Qt6 GUI. All AI calls go through `ai_chat_stream()` which dispatches to three backends. The TUI is split into 14 modules under `src/`. The Qt GUI lives in `Qt_GUI/` (self-contained, copied inside `C_software/`).

### Directory structure

```
C_software/
├── src/                    ← sorgenti TUI C (14 moduli)
│   ├── main.c              ← SEZIONE 12 + CONFIG + main()
│   ├── backend.c           ← SEZIONE 1 (BackendCtx, profili, sync, scan_gguf_dir)
│   ├── terminal.c          ← SEZIONE 2 (getch_single, read_key_ext, clear_screen)
│   ├── http.c              ← SEZIONE 4+5 (TCP, JSON, Ollama stream, llama-server SSE)
│   ├── ai.c                ← SEZIONE 6 (ai_chat_stream, backend_ready, stream_cb)
│   ├── modelli.c           ← SEZIONE 8 (load_gguf_model, choose_backend/model, gestisci_modelli)
│   ├── output.c            ← SEZIONE 9 (salva_output)
│   ├── multi_agente.c      ← SEZIONE 10 (pipeline 6 agenti + guardie math/RAM)
│   ├── strumenti.c         ← SEZIONE 11 (menu_tutor, math locale, pratico_730, pratico_piva)
│   ├── simulatore.c        ← 35 simulazioni, 9 categorie
│   ├── quiz.c              ← stub (quiz interattivi, TODO)
│   ├── hw_detect.c         ← rilevamento hardware cross-platform
│   ├── agent_scheduler.c   ← Hot/Cold GGUF scheduler
│   ├── prismalux_ui.c      ← print_header(), box_*(), input_line()
│   ├── vram_bench.c        ← binario standalone VRAM benchmark
│   └── llama_wrapper.cpp   ← wrapper C++ llama.cpp (solo -DWITH_LLAMA_STATIC)
├── include/                ← header (14 file)
├── Qt_GUI/                 ← GUI grafica Qt6 (auto-contenuta)
│   ├── pages/              ← agenti_page, simulatore_page, tutor_page, ecc.
│   ├── build/
│   │   └── Prismalux_GUI   ← binario GUI compilato
│   └── build.sh
├── models/                 ← file .gguf
├── Makefile
├── build.sh
├── build.bat               ← build Windows (doppio click)
├── Prismalux_GUI.desktop   ← launcher desktop
└── CLAUDE.md
```

### Source files

| File | Role |
|---|---|
| `src/main.c` | Menus principale (impara, personalizzazione, manutenzione), config save/load, main() |
| `src/backend.c` | BackendCtx, profili globali, _sync_to_profile(), scan_gguf_dir() |
| `src/terminal.c` | getch_single(), read_key_ext(), clear_screen(), term_restore() |
| `src/http.c` | TCP helpers, JSON helpers, Ollama NDJSON stream, llama-server SSE |
| `src/ai.c` | ai_chat_stream(), stream_cb(), backend_ready(), check_risorse_ok() |
| `src/modelli.c` | load_gguf_model(), choose_backend(), choose_model(), gestisci_modelli(), menu_modelli() |
| `src/output.c` | salva_output() — scrive in ../esportazioni/ |
| `src/multi_agente.c` | Pipeline 6 agenti, ma_extract_code(), ma_run_python(), ma_valuta_output() |
| `src/strumenti.c` | menu_tutor(), pratico_730(), pratico_piva(), menu_pratico() |
| `src/hw_detect.c/h` | Cross-platform CPU/GPU/RAM detection; hw_thread_create/join |
| `src/agent_scheduler.c/h` | Hot/Cold GGUF model scheduler |
| `src/prismalux_ui.c/h` | print_header(), box_*(), input_line(), SysRes, ANSI color macros |
| `src/llama_wrapper.cpp/h` | C++ wrapper llama.cpp; solo con `-DWITH_LLAMA_STATIC` |
| `src/vram_bench.c` | Standalone VRAM benchmarking (Ollama HTTP or llama-static nvidia-smi delta) |

### Sections mapping (from original prismalux.c)

```
SEZIONE 1  → src/backend.c    (BackendCtx, profili, sync, scan_gguf_dir)
SEZIONE 2  → src/terminal.c   (getch_single, term_restore, read_key_ext, KEY_F2)
SEZIONE 4  → src/http.c       (TCP helpers, JSON, http_get/post_stream)
SEZIONE 5  → src/http.c       (http_ollama_stream NDJSON, http_llamaserver_stream SSE)
SEZIONE 6  → src/ai.c         (ai_chat_stream abstraction)
SEZIONE 8  → src/modelli.c    (choose_backend, choose_model, load_gguf_model)
SEZIONE 9  → src/output.c     (salva_output)
SEZIONE 10 → src/multi_agente.c (pipeline 6 agenti)
SEZIONE 11 → src/strumenti.c  (menu_tutor, pratico_730, pratico_piva)
SEZIONE 12 → src/main.c       (menu_impara, menu_personalizzazione, menu_manutenzione, main)
CONFIG     → src/main.c       (salva_config, carica_config)
```

### Key globals

All globals are **defined** in `src/backend.c` and declared `extern` in `include/backend.h` (and also in `include/prismalux_ui.h` for backward compatibility):

```c
BackendCtx g_ctx;          // active backend (backend type, host, port, model)
HWInfo     g_hw;           // hardware snapshot filled once at startup by hw_detect()
int        g_hw_ready;     // 1 after hw_detect() completes
char       g_hdr_backend[32]; // backend name for print_header()
char       g_hdr_model[80];   // model name for print_header()
char       g_models_dir[512]; // path to .gguf directory (default "./models")

// Per-backend profiles — persist last-used settings when switching backends:
g_prof_ollama_host / g_prof_ollama_port / g_prof_ollama_model
g_prof_lserver_host / g_prof_lserver_port / g_prof_lserver_model
g_prof_llama_model
```

`salva_config()` is defined in `src/main.c` and forward-declared in `src/modelli.c` and `src/multi_agente.c` where needed.

### AI abstraction (`ai_chat_stream`)

`ai_chat_stream(ctx, sys_prompt, user_msg, callback, udata, out_buf, out_max)` is the **only** call site for AI inference. Dispatches based on `ctx->backend`:
- `BACKEND_LLAMA` → `lw_chat()` (only when `WITH_LLAMA_STATIC` is defined)
- `BACKEND_OLLAMA` → `http_ollama_stream()` (streaming NDJSON)
- `BACKEND_LLAMASERVER` → `http_llamaserver_stream()` (streaming SSE, OpenAI API)

When `WITH_LLAMA_STATIC` is not defined, all `lw_*` symbols are replaced by `static inline` no-op stubs in `include/common.h`. Any new `lw_*` function added to `llama_wrapper.h` needs a matching stub there.

### Multi-Agent pipeline (SEZIONE 10 → src/multi_agente.c)

```
Ricercatore (0) → Pianificatore (1) → Programmatore A (2) ─┐ parallel
                                    → Programmatore B (3) ─┘
                  → Tester (4, correction loop max 3) → Ottimizzatore (5)
```

Each agent is an `Agent` struct with its own `BackendCtx`. `_MA_PREF[6][9]` lists preferred model name substrings per role; `ma_trova_default()` finds the first match in the available model list.

Key functions: `ma_extract_code()` parses ` ```python...``` ` blocks; `ma_run_python()` runs code via `popen("python3 tmp.py 2>&1")`; `ma_valuta_output()` detects errors (traceback/error/exception keywords). Parallel programmatori use `hw_thread_create()` from `hw_detect.h`.

### llama_wrapper.cpp (static build only)

- `flash_attn = true` — reduces VRAM, speeds inference
- `type_k = type_v = GGML_TYPE_Q8_0` — compressed KV cache
- `n_threads = hardware_concurrency() / 2` — physical cores only
- Sampler: top_k=40, top_p=0.90, temp=0.30, repeat_penalty=1.2, frequency_penalty=0.1
- `llama_memory_clear()` is called before each `lw_chat()` — context fully reset between agents

### Config persistence

`~/.prismalux_config.json` stores the active backend plus all three backend profiles (flat JSON keys: `ollama_host`, `ollama_port`, `ollama_model`, `lserver_*`, `llama_model`). `_sync_to_profile()` syncs `g_ctx` to the matching profile before any backend switch or save.

## Development conventions

- **Emoji**: always use UTF-8 hex sequences, not raw emoji literals — e.g. `"\xf0\x9f\x94\x8d"` for 🔍, `"\xf0\x9f\xa4\x96"` for 🤖. This avoids encoding issues on some compilers.
- **UI width**: always clamp `W = term_width() - 2`, min 60, max 130. Use `disp_len()` for padding — it handles ANSI escapes and multi-byte UTF-8.
- **Italian in AI prompts**: all non-code agent system prompts must include `"Rispondi SEMPRE e SOLO in italiano."`. Code-producing agents use `"I commenti nel codice sono in italiano."`.
- **No external libraries**: HTTP is raw TCP; JSON is parsed with `js_str()`/`js_encode()`. Do not add dependencies.
- **No `system()` for AI**: always use `ai_chat_stream()`.
- **Menu navigation convention**: `0` or `ESC (27)` = back/cancel in submenus; `Q` = quit at main menu level.
- **One task at a time**: compile and test before moving to next feature. `make http` is the fast feedback loop.
- **Section markers**: each logical group uses `/* ══════ SEZIONE N — title ══════ */`.
- **Include path**: always use `-Iinclude` — all headers live in `include/`, all sources in `src/`.
- **Forward declarations**: `salva_config()` defined in `src/main.c` must be forward-declared in any `src/*.c` that calls it.

## Menu structure

```
Main menu
├── 1. Multi-Agente LLM    → menu_multi_agente()           [src/multi_agente.c]
├── 2. Strumento Pratico   → menu_pratico() → pratico_730() / pratico_piva()  [src/strumenti.c]
├── 3. Impara con AI       → menu_impara() → menu_tutor() + stubs             [src/main.c]
├── 4. Personalizzazione   → menu_personalizzazione() + stubs                 [src/main.c]
├── 5. Manutenzione        → menu_manutenzione() → menu_modelli() / choose_backend() / hw_print()  [src/main.c]
└── Q/ESC  Esci
```

`menu_modelli()` dispatches to `gestisci_modelli()` (llama-static: lists .gguf + download + remove) or `choose_model()` (HTTP: lists from server).

## Stato del progetto — Checklist (aggiornato 2026-03-29)

### Completate ✅
| Voce | Note |
|------|------|
| Partenza da Python a C puro | Prototipazione completata, struttura modulare `src/` + `include/` |
| Algoritmo anti-allucinazione | Motore Byzantino 4 agenti (A/B/C/D) — ideato dall'utente, esclusivo del C |
| Ottimizzazione LLM piccoli | Agent scheduler Hot/Cold, VRAM bench, controlli RAM/VRAM pre-caricamento |
| Cartelle pulite (design C puro) | Split da `prismalux.c` monolitico a 14 moduli `src/` + 14 header `include/` |
| Collegamento Ollama locale | HTTP NDJSON stream su `localhost:11434`, auto-detect modelli, profilo persistente |
| llama.cpp scaricato/collegato | `./build.sh` + `src/llama_wrapper.cpp`, `make static` |
| Download LLM da Hugging Face | `gestisci_modelli()` → 8 modelli curati + URL personalizzato + `wget` background |
| LLM compilato dai pesi open source | Build con `-DGGML_NATIVE=ON` adatta ai SIMD del CPU locale |
| Preferenze utente persistenti | `~/.prismalux_config.json` con profili per-backend (ollama/llama-server/llama-static) |
| Commenti del codice | Completati su tutti i `src/*.c`: sezioni, inline doc, logiche algoritmi |
| Documentazione del codice | Tutti gli header `include/*.h` + `llama_wrapper.h` con doc-block |
| Test multi-agente (controllo) | `test_multiagente.c` — pipeline 6 agenti **8/8 OK** (2026-03-29) con deepseek-r1:1.5b |
| Simulatore Algoritmi espanso | `src/simulatore.c` — 63 simulazioni, 13 categorie |
| Tutor ottimizzato (zero-token) | `src/strumenti.c` — `_stampa_argomenti()` locale, `_math_locale()` parser |
| Guardia math pipeline | `_guardia_math()` — aritmetica, linguaggio naturale, sconto, %, primi, sommatoria |
| Guardia tipo task pipeline | `_task_sembra_codice()` avvisa se task non sembra programmazione |
| Guardia RAM pre-pipeline | `_check_ram_pipeline()` blocca/avvisa a 75%/92% |
| Timing calcoli locali | TUI + Qt GUI mostrano ms e s dopo ogni calcolo locale |
| Qt GUI integrata | `Qt_GUI/` auto-contenuta dentro `C_software/` |
| Launcher `./Prismalux` | Script root che avvia la Qt GUI con check Ollama |
| `.desktop` aggiornati | Entrambi puntano a `C_software/Qt_GUI/build/Prismalux_GUI` |
| **Qt GUI — Impostazioni 6 tab tematiche** | 6 tab merged (Connessione, Hardware, AI Locale, Voce & Audio, Aspetto, Avanzate) — da 11 originali — 2026-03-30 |
| **Qt GUI — pre-calcolo math** | `_inject_math()` calcola le espressioni nel task prima di passarle all'AI; `_math_sys()` informa l'AI che i valori sono già verificati |
| **Qt GUI — traduzione automatica** | `_is_likely_english()` + `OpMode::Translating` — input inglese viene tradotto in italiano via AI prima della pipeline |
| **Qt GUI — RAM check agenti** | `runPipeline()` / `runByzantine()` / `runMathTheory()` bloccano a ≥92% RAM, avvisano a ≥75% |
| **Qt GUI — gestione modelli GGUF** | Scheda llama.cpp: card con delete + 7 preset download + URL personalizzato |
| **Script `aggiorna.sh`** | Ricompila TUI + GUI + ZIP Windows in un comando; slash command `/aggiorna-prismalux` |
| **Qt GUI — Impostazioni riorganizzate** | 11 tab flat (Backend, Hardware, llama.cpp, Monitor, Tema, Test, Voce, Trascrivi, RAG, Dipendenze, LLM Consigliati) — 2026-03-29 |
| **Suite test completa (2026-03-29)** | test_engine 18/18 · test_multiagente 8/8 · test_toon_config 55/55 · test_stress 74/74 · test_golden 53/53 · test_memory 12/12 · test_perf 26/26 · test_sha256 20/20 · test_version 35/35 · test_cs_random 17/17 · test_guardia_math 93/93 · test_math_locale 120/120 · test_agent_scheduler 87/87 · test_rag 30/30 |
| **Guardia math — livello Wolfram/TI Derive** | `_gp_prim()` esteso: ln/log/log2, trig+alias italiano, sinh/cosh/tanh, cbrt, abs, floor/ceil/round, pow, min/max, gcd/lcm, `_gp_err` flag anti-testo — 200/200 test — 2026-03-30 |
| **Qt GUI — crash refreshModels fix** | `personalizza_page.cpp`: sostituito `std::function` auto-referenziale con `shared_ptr<function>` — eliminato stack overflow nel copy constructor — 2026-03-30 |
| **Qt GUI — crash Impostazioni fix** | `mainwindow.cpp`: `ensureSettingsDialog()` deduplica 3 blocchi identici, rimuove `Qt::Window` (causa crash Windows API parent-child) — 2026-03-30 |

### Da completare ✖️
| Voce | Priorità | Note |
|------|----------|------|
| Test su strada | Alta | Eseguire `./prismalux` manualmente, tutti i menu, edge case |
| Riconoscimento vocale Qt | Bassa | Pulsante 🎙 Voce presente ma disabilitato (stub) |

### Funzionalità stub (da implementare in C)
- **Quiz Interattivi** — `src/quiz.c` scheletro presente; versione Python in `quiz_interattivi.py`
- **Dashboard Statistica** — stub; versione Python pronta in `dashboard_statistica.py`
- **Analisi Dati con AI** — stub; versione Python pronta in `tutor_dati.py`
- **llama.cpp Studio** — stub Qt presente; versione Python in `llama_cpp_studio/main.py`
- **Cython Studio** — stub Qt presente; versione Python in `cython_studio/cython_studio.py`
- **Cerca Lavoro + CV Reader** — in Python, non ancora nel C

## Architettura guardie pre-pipeline (src/multi_agente.c)

Prima di `multi_agent_run()` scattano tre controlli in sequenza:
```
input utente
    │
    ├─ _guardia_math()       → aritmetica locale (0 token, 0 AI):
    │   ├── sconto X% su Y
    │   ├── X% di Y
    │   ├── primi da X a Y
    │   ├── somma da X a Y
    │   ├── "quanto fa/vale/calcola ..." → parser ricorsivo
    │   └── espressione pura (solo cifre+op)
    │
    ├─ _task_sembra_codice() → avvisa se non sembra programmazione
    │
    └─ _check_ram_pipeline() → RAM < 75% OK, 75-92% avviso, > 92% blocco forte
```

## Architettura motore matematico locale (src/strumenti.c + src/multi_agente.c)

Entrambi i file contengono un recursive descent parser indipendente:
- `strumenti.c`: `_mp_expr/_mp_term/_mp_power/_mp_primary` — usato da `_math_locale()` nel Tutor
- `multi_agente.c`: `_gp_expr/_gp_term/_gp_pow/_gp_prim` — usato da `_guardia_math()` nella Pipeline
- Precedenza: `()` > `^` > `*/%` > `+-`
- `_gp_fmt(v, buf, sz)` — formatta senza `.0` inutili
- `_gp_err` flag (int statico) — 1 se parser ha incontrato identificatore sconosciuto; usato per rifiutare testo in linguaggio naturale come "la pasta"

### Funzioni matematiche supportate da `_gp_prim()` (livello Wolfram/TI Derive)
Costanti: `pi`, `e`, `phi`
Log/exp: `ln`, `log` (log₁₀ come TI Derive), `log2`, `log10`, `exp`
Trig (radianti + alias italiano): `sin`/`seno`, `cos`/`coseno`, `tan`/`tangente`, `asin`/`arcoseno`, `acos`/`arcocoseno`, `atan`/`arcotangente`, `atan2`
Iperboliche: `sinh`, `cosh`, `tanh`
Radici: `sqrt`/`radq`, `cbrt` + italiano "radice cubica di X", "radice quadrata di X"
Logaritmo naturale italiano: "logaritmo naturale di X" → `ln`
Arrotondamento: `abs`, `floor`, `ceil`, `round`, `trunc`, `sign`
Potenza: `pow(x,n)` + operatore `^`
Min/Max: `min(a,b)`, `max(a,b)`
Teoria numeri: `gcd`/`mcd`, `lcm`/`mcm`
Parole skip: `di`, `del`, `della` (ignorati nel parser per supportare "radice di X")

## Simulatore Algoritmi — categorie (src/simulatore.c)

13 categorie, 63 simulazioni totali (aggiornato 2026-03-18):
| Cat | N | Funzioni |
|-----|---|----------|
| Ordinamento | 15 | bubble, selection, insertion, merge, quick, heap, shell, cocktail, counting, gnome, comb, pancake, radix, bucket, tim |
| Ricerca | 4 | binaria, lineare, jump_search, interpolation_search |
| Matematica | 9 | gcd_euclide, crivo, potenza_modulare, triangolo_pascal, monte_carlo_pi, fibonacci_dp, torre_hanoi, fib_ricorsivo, miller_rabin |
| Prog. Dinamica | 5 | zaino, lcs, coin_change, edit_distance, lis |
| Grafi | 7 | bfs_griglia, dijkstra, dfs_griglia, topo_sort, bellman_ford, floyd_warshall, kruskal |
| Tecniche | 3 | two_pointers, sliding_window, bit_manipulation |
| Visualizzazioni | 3 | game_of_life, sierpinski, rule30 |
| Fisica | 1 | caduta_libera |
| Chimica | 3 | ph, gas_ideali, stechiometria |
| Stringhe [NEW] | 3 | kmp_search, rabin_karp, z_algorithm |
| Strutture Dati [NEW] | 5 | stack_queue, linked_list, bst, hash_table, min_heap |
| Greedy [NEW] | 3 | activity_selection, zaino_frazionario, huffman |
| Backtracking [NEW] | 2 | n_queens (N=5), sudoku (4x4) |

Menu: `1`-`9` categorie base, `a`=Stringhe, `b`=Strutture Dati, `c`=Greedy, `d`=Backtracking

## Architettura Qt GUI — Impostazioni (Qt_GUI/pages/)

Le impostazioni usano un singolo `QTabWidget` con **6 tab tematiche** — nessun tab annidato
(il nesting con stesso `objectName` causa contenuto vuoto in Qt6).
Ridotto da 11 tab originali a 6 con nomi comprensibili per utenti non esperti.

```
ImpostazioniPage
└── QTabWidget "settingsTabs"
    ├── 🔌 Connessione  ← buildBackend() + buildLlmConsigliatiTab() (scrollabile)
    ├── 🖥 Hardware     ← buildHardware() + buildVramBench() (scrollabile, invariato)
    ├── 🦙 AI Locale    ← buildLlamaStudio() (stretch=1) + buildDipendenzeTab() (scroll max 280px)
    ├── 🎤 Voce & Audio ← buildVoceTab() + buildTrascriviTab() (scrollabile)
    ├── 🎨 Aspetto      ← buildTemaTab()
    ├── 📊 Avanzate     ← MonitorPanel (stretch=2) + buildRagTab() + buildTestTab() (scroll)
    └── 📈 Grafico      ← buildGraficoTab() (aggiunto lazy in setGraficoCanvas())
```

`switchToTab("LLM")` è aliasato a "Connessione" — i link esistenti da `grafico_page.cpp`
e `agenti_page.cpp` continuano a funzionare senza modifiche.

`ManutenzioneePage` ha costruttore minimale (nessun layout proprio);
le sezioni sono esposte come metodi pubblici `QWidget* buildXxx()`.

### ensureSettingsDialog() — pattern anti-crash (mainwindow.cpp)
Unico punto di creazione del dialog Impostazioni. Chiamato dai 3 siti:
- `connect(m_settingsBtn, ...)` — click pulsante ⚙ header
- `connect(agentiPage, &AgentiPage::requestOpenSettings, ...)` — link da AgentiPage
- `connect(grafPage, &GraficoPage::requestOpenSettings, ...)` — link da GraficoPage
**IMPORTANTE**: NON impostare `Qt::Window` su QDialog con parent — causa crash su Windows
(bug nella gestione parent-child della Windows API).

### AgentiPage — funzionalità avanzate (Qt_GUI/pages/agenti_page.cpp)

| Funzione | Descrizione |
|---|---|
| `_inject_math(task)` | Regex sulle espressioni numeriche nel task → inietta `expr[=N]` |
| `_math_sys(task, base)` | Aggiunge nota al system prompt se task contiene `[=` |
| `_is_likely_english(text)` | Conta parole funzionali inglesi; ≥2 hit → testo inglese |
| `OpMode::Translating` | Stato intermedio: AI traduce l'input, poi riavvia la modalità originale |
| RAM pre-check | Legge `/proc/meminfo` in `runPipeline/Byzantine/MathTheory`; blocca ≥92%, avvisa ≥75% |

**Layout input area** (QGridLayout 3 colonne):
```
┌────────────────────────────┬──────────────┬──────────────┐
│                            │  ▶ Avvia     │  ⚡ Singolo  │
│  QTextEdit m_input         ├──────────────┼──────────────┤
│  (rowspan=2)               │  ■ Stop      │  🎙 Voce     │
└────────────────────────────┴──────────────┴──────────────┘
```

## Pending work (tecnico)

- **Quiz Interattivi**: implementare `src/quiz.c` (stub presente; versione Python in `quiz_interattivi.py`)
- **Strumento Pratico**: aggiungere Cerca Lavoro e CV Reader (esistono in Python)
- **Dashboard + Analisi Dati**: stub C; versioni Python pronte
- **Riconoscimento vocale**: pulsante 🎙 presente e disabilitato in `agenti_page.cpp`
- **Refactor funzioni lunghe**: `load_gguf_model()`, `multi_agent_run()`, `gestisci_modelli()`, `choose_model()` — funzionano ma sono > 150 righe
- **CPU+GPU dialog**: split model layers tra NVIDIA + iGPU usando `agent_scheduler` con budget VRAM per dispositivo
- **RAM inter-agente**: il check pre-pipeline non copre la crescita RAM durante l'esecuzione
