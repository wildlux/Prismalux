# Prismalux — TODO pendenti

> Aggiornato: 2026-05-28 (tutte le voci implementate) | Versione: 2.9
> Build: `cmake --build build_gui -j$(nproc)`

---

## ✅ Implementati (sessione 2026-05-28)

### Voci originali del TODO consolidato

- [x] **[DPI] Scaling HiDPI Wayland** — `dpiScale()` applicato in 6 file:
  `agenti_page_consiglio.cpp`, `agents_config_dialog.cpp`, `app_controller_page.cpp`,
  `impara_page.cpp`, `agenti_page_knowledge.cpp`, `code_interpreter_widget.cpp`.
  Aggiunti `#include "../dpi_utils.h"` dove mancante.

- [x] **[UI] Stati di errore muti** — `fetchAndFillMathModels()` e `onLoadModelsOnce()`
  in `matematica_page.cpp` connettono `m_ai->error` via holder: se il backend non è
  raggiungibile, `setStatus()` mostra il messaggio contestuale invece di restare muto.

- [x] **[SEC] Supply chain MCP — hash completi** — `MCPs/gns3_mcp/requirements.lock`:
  hash SHA256 reale aggiunto per `gns3fy==0.8.0`. Tutti i 6 pacchetti ora pinned con hash verificato.

- [x] **Ollama MCP** — `MCPs/ollama_mcp/server.py` + `pyproject.toml`:
  JSON-RPC 2.0 stdio, SQLite cache in `~/.prismalux/ollama_models_cache.db`, TTL 5 min.
  Tools: `list_models`, `get_model_info`, `search_models`, `sync`, `pull_model`.
  Dipendenze: solo stdlib Python (sqlite3, urllib, asyncio).

- [x] **[UX] i18n — infrastruttura internazionalizzazione** — `gui/main.cpp`: carica
  `QTranslator` dalla locale di sistema; cerca `i18n/prismalux_<lang>.qm` accanto
  all'eseguibile; stub `.ts` già presenti in `gui/i18n/`. Versione app corretta: 2.1→2.9.
  Per aggiungere una lingua: creare `prismalux_en.ts`, tradurre le stringhe `tr()`,
  eseguire cmake per generare i `.qm`.

### Precedenti (da sessioni precedenti)

- [x] **[Quiz] Più domande CCNA** — 64 → 209 domande in 15 temi
- [x] **[Python] Type checking MCP** — `[tool.mypy]` in tutti i pyproject.toml
- [x] **[SEC] requirements.lock parziale** — hash reali per requests/certifi/idna/urllib3/charset-normalizer

---

> Il TODO è ora **vuoto**. Tutte le voci sono state implementate o verificate come già presenti nel codice.
> Per nuove funzionalità apri una conversazione e descrivi cosa serve.
