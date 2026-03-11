# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build commands

```bash
# Default — HTTP only (Ollama + llama-server), requires only gcc
make              # or: make http

# With embedded llama.cpp (one-time setup, ~10-15 min)
./build.sh        # clones llama.cpp, runs cmake, links everything
make static       # requires build.sh to have run first

# Other targets
make vram_bench          # Ollama VRAM profiler (standalone)
make vram_bench_static   # same with embedded llama.cpp
make windows             # produces prismalux.exe + vram_bench.exe (MinGW/MSYS2)
make clean

# Run
./prismalux --backend ollama
./prismalux --backend llama-server --port 8080
./prismalux --help
```

There is no test suite. Validate with `make http && ./prismalux --backend ollama`.
After structural changes: `make clean && make`.

## Architecture

Single-binary C TUI application. All AI calls go through one abstraction function that dispatches to three backends. The monolithic `prismalux.c` has been split into separate modules under `src/`.

### Directory structure

```
C_software/
├── src/
│   ├── main.c              ← SEZIONE 12 + CONFIG + main()
│   ├── backend.c           ← SEZIONE 1 (BackendCtx, profili, sync, scan_gguf_dir)
│   ├── terminal.c          ← SEZIONE 2 (getch_single, read_key_ext, clear_screen)
│   ├── http.c              ← SEZIONE 4+5 (TCP, JSON, Ollama stream, llama-server SSE)
│   ├── ai.c                ← SEZIONE 6 (ai_chat_stream, backend_ready, stream_cb)
│   ├── modelli.c           ← SEZIONE 8 (load_gguf_model, choose_backend/model, gestisci_modelli)
│   ├── output.c            ← SEZIONE 9 (salva_output)
│   ├── multi_agente.c      ← SEZIONE 10 (pipeline 6 agenti)
│   ├── strumenti.c         ← SEZIONE 11 (menu_tutor, pratico_730, pratico_piva)
│   ├── hw_detect.c         ← rilevamento hardware cross-platform
│   ├── agent_scheduler.c   ← Hot/Cold GGUF scheduler
│   ├── prismalux_ui.c      ← print_header(), box_*(), input_line()
│   ├── vram_bench.c        ← binario standalone VRAM benchmark
│   └── llama_wrapper.cpp   ← wrapper C++ llama.cpp (solo -DWITH_LLAMA_STATIC)
├── include/
│   ├── common.h            ← platform includes, sock_t, lw_stubs, MAX_BUF/MAX_MODELS/BAR_LEN
│   ├── backend.h           ← BackendCtx typedef, Backend enum, extern globals
│   ├── terminal.h          ← getch_single(), read_key_ext(), clear_screen(), KEY_F2
│   ├── http.h              ← tcp_connect(), js_str(), http_ollama_*/list()
│   ├── ai.h                ← ai_chat_stream(), backend_ready(), ensure_ready_or_return()
│   ├── modelli.h           ← load_gguf_model(), choose_backend(), gestisci_modelli()
│   ├── output.h            ← salva_output()
│   ├── multi_agente.h      ← menu_multi_agente()
│   ├── strumenti.h         ← menu_tutor(), menu_pratico()
│   ├── hw_detect.h         ← HWInfo, HWDevice, hw_detect(), hw_thread_*
│   ├── agent_scheduler.h   ← AgentScheduler, as_init(), as_load(), as_evict()
│   ├── prismalux_ui.h      ← SysRes, print_header(), box_*(), ANSI macros
│   └── llama_wrapper.h     ← lw_init(), lw_chat(), lw_free()
├── models/                 ← file .gguf
├── Makefile
├── build.sh
├── build.bat
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

## Pending work

- **Strumento Pratico**: add Cerca Lavoro and CV Reader (exist in Python version)
- **Impara con AI**: implement Quiz interattivi (AI generates MCQ), Dashboard statistica, Analisi dati (currently stubs)
- **Personalizzazione**: implement llama.cpp Studio and Cython Studio submenus (currently stubs showing instructions)
- **CPU+GPU dialog**: split model layers across NVIDIA + iGPU using `agent_scheduler` with per-device VRAM budgets
