# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build commands

```bash
# Default — HTTP only (Ollama + llama-server), requires only gcc
make              # or: make http
# Equivalent direct command:
gcc -O2 -Wall -o prismalux prismalux.c prismalux_ui.c hw_detect.c agent_scheduler.c -lpthread -lm

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

Single-binary C TUI application. All AI calls go through one abstraction function that dispatches to three backends. The entire application logic lives in `prismalux.c` (~2300 lines, 12 numbered sections).

### Source files

| File | Role |
|---|---|
| `prismalux.c` | All menus, business logic, config persistence, main() |
| `prismalux_ui.c/h` | `print_header()`, `box_*()`, `input_line()`, `SysRes`, ANSI color macros |
| `hw_detect.c/h` | Cross-platform CPU/GPU/RAM detection; `hw_thread_create/join` wrapping pthreads/Win32 |
| `agent_scheduler.c/h` | Hot/Cold GGUF model scheduler: evicts/loads models per agent turn based on VRAM |
| `llama_wrapper.cpp/h` | C++ wrapper around llama.cpp C API; only compiled with `-DWITH_LLAMA_STATIC` |
| `vram_bench.c` | Standalone VRAM benchmarking (Ollama HTTP or llama-static nvidia-smi delta) |

### Sections in `prismalux.c`

```
SEZIONE 1  — Backend enum + BackendCtx struct + per-backend profile globals
SEZIONE 2  — Terminal utilities (getch_single, term_restore, read_key_ext, KEY_F2)
SEZIONE 4  — TCP/HTTP helpers (http_get, http_post_stream, js_str, js_encode)
SEZIONE 5  — HTTP streaming: http_ollama_stream (NDJSON) + http_llamaserver_stream (SSE)
SEZIONE 6  — ai_chat_stream() abstraction — single dispatch point for all AI calls
SEZIONE 8  — Backend/model selection UI (choose_backend, choose_model, load_gguf_model)
SEZIONE 9  — Output saving to ../esportazioni/
SEZIONE 10 — Multi-Agent 6-agent pipeline
SEZIONE 11 — Tutor + Strumento Pratico (730, P.IVA)
SEZIONE 12 — Main menu hierarchy + config save/load + main()
```

### Key globals

```c
BackendCtx g_ctx;          // active backend (backend type, host, port, model)
HWInfo     g_hw;           // hardware snapshot filled once at startup by hw_detect()
int        g_hw_ready;     // 1 after hw_detect() completes
char       g_models_dir[]; // path to .gguf directory (default "./models")

// Per-backend profiles — persist last-used settings when switching backends:
g_prof_ollama_host / g_prof_ollama_port / g_prof_ollama_model
g_prof_lserver_host / g_prof_lserver_port / g_prof_lserver_model
g_prof_llama_model
```

### AI abstraction (`ai_chat_stream`)

`ai_chat_stream(ctx, sys_prompt, user_msg, callback, udata, out_buf, out_max)` is the **only** call site for AI inference. Dispatches based on `ctx->backend`:
- `BACKEND_LLAMA` → `lw_chat()` (only when `WITH_LLAMA_STATIC` is defined)
- `BACKEND_OLLAMA` → `http_ollama_stream()` (streaming NDJSON)
- `BACKEND_LLAMASERVER` → `http_llamaserver_stream()` (streaming SSE, OpenAI API)

When `WITH_LLAMA_STATIC` is not defined, all `lw_*` symbols are replaced by no-op stubs at the top of `prismalux.c`. Any new `lw_*` function added to `llama_wrapper.h` needs a matching stub there.

### Multi-Agent pipeline (SEZIONE 10)

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

## Menu structure

```
Main menu
├── 1. Multi-Agente LLM    → menu_multi_agente()
├── 2. Strumento Pratico   → menu_pratico() → pratico_730() / pratico_piva()
├── 3. Impara con AI       → menu_impara() → menu_tutor() + stubs
├── 4. Personalizzazione   → menu_personalizzazione() + stubs
├── 5. Manutenzione        → menu_manutenzione() → menu_modelli() / choose_backend() / hw_print()
└── Q/ESC  Esci
```

`menu_modelli()` dispatches to `gestisci_modelli()` (llama-static: lists .gguf + download + remove) or `choose_model()` (HTTP: lists from server).

## Pending work

- **Strumento Pratico**: add Cerca Lavoro and CV Reader (exist in Python version)
- **Impara con AI**: implement Quiz interattivi (AI generates MCQ), Dashboard statistica, Analisi dati (currently stubs)
- **Personalizzazione**: implement llama.cpp Studio and Cython Studio submenus (currently stubs showing instructions)
- **CPU+GPU dialog**: split model layers across NVIDIA + iGPU using `agent_scheduler` with per-device VRAM budgets
