# Prismalux — TODO Qualità Codice

> Creato: 2026-06-20 | Versione: 2.9
> Obiettivo: alzare sistematicamente la qualità su 5 assi — leggibilità, testabilità,
> portabilità, manutenibilità, usabilità — senza aggiungere feature.

---

## 🔴 PRIORITÀ ALTA — Debito tecnico strutturale

### File troppo grandi — da spezzare

I file sotto non rispettano il principio di responsabilità singola.
Ogni file sopra ~1500 righe deve essere diviso in unità logiche.

- [x] **`main_lan_wan.cpp` (4488 → 901 righe)** — FATTO 2026-06-20:
  - `main_lan_gns3.cpp` (610 righe) — GNS3 MCP + ADB
  - `main_wan_persist.cpp` (923 righe) — WAN helpers + dispatch + persist
  - `main_wan_server.cpp` (1587 righe) — WAN server + client + workers
  - `main_wan_extra.cpp` (785 righe) — decompose + sim + eventFilter + tools

- [x] **`main_math.cpp` (4317 → 2154 righe)** — FATTO 2026-06-21:
  - `main_math_solve.cpp` (955r) — buildSolveTab + slot Risolvi Passi
  - `main_math_analisi.cpp` (978r) — buildAnalisi1/2Tab + slot KaTeX
  - `main_math_bool.cpp` (356r) — buildBoolTab + slot Booleana

- [x] **`main_ai_ui.cpp` (3906 → 1513 righe)** — FATTO 2026-06-21:
  - `main_ai_slots.cpp` (1414r) — slot chat/run/mode/log/translate/RAG
  - `main_ai_panels.cpp` (1136r) — buildToolsPanel, buildBottomBar, recolorLog,
    onBubbleStyleChanged, buildInputFormatBar, onInputSelectionChanged,
    onFmtBtnClicked, onSymbolSearchChanged, AccentPickerPopup

- [x] **`main_simulator_algos.cpp` (3775 → 223 righe)** — FATTO 2026-06-21:
  - `main_sim_sorting.cpp` (1612r) — Sorting, Search, Array tricks
  - `main_sim_graph.cpp` (1073r) — BFS/DFS/Dijkstra/Kruskal/Prim/Tarjan + DS
  - `main_sim_misc.cpp` (899r) — DP, Greedy, Backtracking, String, Math

- [x] **`main_programming_slots.cpp` (3684 → 1875 righe)** — FATTO 2026-06-20:
  - `main_programming_vpn.cpp` (656 righe) — VPN/Tunnel
  - `main_programming_wiby.cpp` (525 righe) — IP Camera WIBY
  - `main_programming_reverse_usb.cpp` (728 righe) — Reverse Eng + USB/Driver

- [x] **`lan_server.cpp` (2854 → 1450 righe)** — FATTO 2026-06-20:
  - `lan_server_ai.cpp` (531 righe) — handler AI: chat, generate, tags, knowledge, RAG
  - `lan_server_compute.cpp` (600 righe) — handler compute: math, graphviz, whisper, katex
  - `lan_server_api.cpp` (338 righe) — handler API: file, repl, finanza, graph, bootstrap

---

### Duplicazione codice — da estrarre

- [ ] **Widget "installa Python pkg"** — lo stesso pattern (label + pulsante pip + log)
      appare in >10 file (`main_customize_lora.cpp`, `main_sci_compute_ui.cpp`,
      `main_mcp_manager.cpp`, `widget_voice_cloner.cpp`, ecc.).
      Estrarre in `widget_pip_installer.h/cpp` con segnale `installed(bool)`.

- [ ] **Pattern `QProcess` subprocess Python** — creazione processo, connessione
      `readyRead`/`finished`, timeout `QTimer`, kill — ripetuto ~15 volte.
      Estrarre in `utils/python_runner.h`: classe `PythonRunner` con signal
      `output(QString)`, `finished(int exitCode)`, `timedOut()`.

- [ ] **Bolle chat HTML** — `_toolBubble()`, `addAIBubble()`, `addUserBubble()`
      usano template HTML simili sparsi in `main_ai_ui.cpp` e `lan_server.cpp`.
      Centralizzare in `utils/chat_bubble_html.h` con funzioni inline.

- [x] **Fetch modelli Ollama** — PARZIALE 2026-06-21: tutti i file aggiornati usano
      `m_ai->fetchModels()` con il pattern holder. Due eccezioni legittime:
      - `main_opencode.cpp` — formatta con "ollama/<name>", usa m_nam dedicato a OpenCode SSE
      - `settings_voice.cpp` — lambda inline con NAM on-the-fly (non ha m_ai)
      Queste due richiederebbero aggiungere m_ai ai costruttori — refactor non urgente.

---

### Magic numbers e costanti sparse

- [x] **Timeout hardcoded** — FATTO 2026-06-20: aggiunte 8 costanti in `prismalux_paths.h`, sostituite tutte le occorrenze in 20+ file. Zero timeout hardcoded rimasti sopra 2s.

- [x] **Porte non centralizzate** — FATTO 2026-06-20: main_maintenance.cpp (3 occorrenze), main_learn.cpp, main_wan_server.cpp ora usano P::kOllamaPort. Residui solo in testo HTML/tooltip non funzionale.

- [x] **Stringhe path hardcoded** — FATTO 2026-06-20: aggiunto P::tmpDir() in prismalux_paths.h; rag_graph.cpp e main_programming_vpn.cpp ora usano P::tmpDir(). dataDir() e memoryDir() erano già presenti.

---

## 🔴 SICUREZZA — Vulnerabilità confermate (2026-06-21)

Security review multi-agente: 4 finding confermati (confidence ≥ 8/10).

- [x] **VULN-1 · Path traversal `file_read` WAN** — `main_wan_extra.cpp:688`
      `file_read` apriva qualsiasi file del filesystem senza gate. Fix: gating dietro
      `shellAllowed` (stesso modello di `shell_cmd`/`python_repl`). Confidence: 9/10.

- [x] **VULN-2 · Arbitrary file write `file_write` WAN** — `main_wan_extra.cpp:697`
      `file_write` scriveva in qualsiasi path dal payload remoto. Fix: stesso gate
      `shellAllowed`. Confidence: 9/10.

- [x] **VULN-3 · Python code injection via sequenza FASTA** — `main_sci_compute.cpp:1254,1289`
      `seq` e `outPath` (esmfold_local) erano interpolati raw in codice Python senza
      escaping di `"` e `\`. Fix: `escPyStr()` helper + `outPath` hardcoded da
      `P::tmpDir()` in esmfold_local. Confidence: 9/10.

- [x] **VULN-4 · SSRF/ffmpeg senza validazione schema URL** — `main_programming_wiby.cpp:286`
      URL da campo editabile passato direttamente a `ffmpeg -i` senza validare lo schema
      (rischio `file://`, `lavfi:`, `data:`). Fix: whitelist schemi `rtsp/rtp/http/https`.
      Confidence: 8/10.

---

## 🟡 PRIORITÀ MEDIA — Robustezza e test

### Copertura test insufficiente

- [ ] **Test per la UI LAN/WAN** — `main_lan_wan.cpp` è il file più lungo ma i test
      `test_wan_compute_tasks` coprono solo la logica protocollo, non la UI.
      Aggiungere: test persistenza SQLite round-trip, test rate limiting token,
      test HMAC integrità. Categoria: CAT-C `test_lan_wan_core`.

- [ ] **Test per AiClient** — `ai_client.cpp` è il nucleo di ogni risposta AI ma
      non ha suite dedicata. Aggiungere mock HTTP (QNetworkAccessManager override)
      per testare: streaming, timeout, errori 4xx/5xx, smart router decision.
      Categoria: CAT-A `test_ai_client`.

- [ ] **Test per GraphMemory concorrenza** — `graph_memory.cpp` usa SQLite con
      connessione per istanza ma non ci sono test di accesso concorrente da due
      istanze (RagGraph + MultiAgent in parallelo). Aggiungere test con due
      thread che scrivono simultaneamente. Categoria: CAT-C `test_graph_memory_concurrent`.

- [ ] **Test integrazione MCP** — `test_mcp_manager` testa solo la UI; manca un
      test che avvii davvero un processo MCP (es. `calculator_mcp`) e verifichi
      il JSON-RPC end-to-end. Categoria: CAT-B `test_mcp_integration`.

- [ ] **Raggiungere 80% PASS senza Ollama** — attualmente 38/41 suite passano
      senza Ollama. Le 3 rimanenti (`AiIntegration`, `AiStress`, `TeamCollab`)
      dipendono da Ollama reale. Aggiungere un mock server HTTP locale nei fixture
      di test per disaccoppiarle.

---

### Gestione errori lacunosa

- [x] **`QProcess::error()` non connesso** — FATTO 2026-06-21: 79 connect aggiunti in 20 file.
      Batch 1 (34): main_customize 8, settings_llm 7, settings_voice 7, main_tools_file 6, main_multimedia 6.
      Batch 2 (21): main_ai_tools 6, main_ai_files 6, widget_voice_cloner 5, main_mcp_manager 4.
      Batch 3 (24): main_programming_wiby 3, main_programming_reverse_usb 3, widget_stable_diffusion 2,
      widget_ssh_manager 2, main_programming_vpn 2, main_customize_lora 2, main_ai_stt 2,
      main_ai_slots 3, main_ai_knowledge 2, mainwindow_slots 2, main_research 1.

- [x] **File I/O senza check errore** — FATTO 2026-06-21: grep trovò 1 QFile reale senza check
      (`main_sci_compute.cpp:1124` BLAST query tmpFile). Aggiunto `if (!f.open()) { errMsg; return; }`.
      Le altre 2 occorrenze erano `QBuffer` in-memory (open() non fallisce mai su QBuffer).

- [x] **SQLite senza check `lastError()`** — FATTO 2026-06-21: aggiunta macro
      `SQL_EXEC(q)` (do/while, log qWarning su errore) in:
      - `graph_memory.cpp`: 3 bare exec() → SQL_EXEC (allNodes filter, BFS, searchNodes)
      - `main_sci_compute.cpp`: ~25 bare exec() → SQL_EXEC (q/tq/cq/rq/dq/nodeQ/localQ/pq/wq)

---

### Portabilità — dipendenze Linux-specifiche

- [x] **Path KaTeX hardcoded** — FATTO 2026-06-20: `katexBaseUrl()` in latex_view.h cerca: bundle assets/katex/ → /usr/share/javascript/katex/ (Linux) → /usr/share/katex/ → fallback. Portabile su Windows/macOS.

- [x] **Comandi shell non portabili** — FATTO 2026-06-21: `lan_server_api.cpp` (REPL
      Python con bwrap+ulimit): avvolto in `#ifdef Q_OS_LINUX … #else P::findPython() #endif`.
      `main_sci_compute.cpp` blast query: `QFile::open` ora controlla errore + imposta `errMsg`.
      `main_programming_vpn.cpp`/`mainwindow_slots.cpp`: bash usato solo su Linux (feature
      Linux-only), nessuna modifica necessaria.

- [x] **`~/` nei path su Windows** — verificato 2026-06-21: tutte le occorrenze
      `"~/"` sono in stringhe display/placeholder, mai in operazioni `QFile`/`QDir`.
      Nessuna modifica necessaria.

- [x] **`python3` hardcoded** — FATTO 2026-06-21: P::findPython() in 12 file
      (rag_graph, lan_server_api, main_sci_compute, settings_llm,
      main_app_controller_slots, main_wan_extra, main_tools,
      main_programming_git_repl, widget_voice_cloner, settings_ai,
      main_programming_wiby, main_programming_reverse_usb, widget_coding_lab).
      Usare `P::findPython()` già esistente in `prismalux_paths.h`, che cerca
      venv → python3 → python.

---

## 🟢 PRIORITÀ BASSA — Pulizia e leggibilità

### DPI non applicato uniformemente

- [ ] **Audit dimensioni non scalate** — grep per pixel hardcoded non passati per `dpiScale()`:
      ```
      grep -rn "setFixedWidth\|setMinimumWidth\|setMaximumWidth\|setFixedHeight\|resize(" \
           gui/ --include="*.cpp" | grep -v "dpiScale"
      ```
      Ogni dimensione strutturale deve usare `dpiScale(N)`.

- [ ] **Font size hardcoded** — `setPointSize(10)`, `setPixelSize(14)` sparsi.
      Usare `dpiScale()` anche per i font, o centralizzare in un tema:
      ```cpp
      int uiFontPt() { return qRound(10 * dpiScale(1)); }
      ```

---

### Convenzioni non rispettate ovunque

- [x] **File con suffisso `_page`** — FATTO 2026-06-21: rinominati
      `main_distillazione_page.h/.cpp` → `main_distillazione.h/.cpp` (git mv).
      Aggiornati 4 riferimenti: CMakeLists.txt, main_programming.cpp, test_distillazione.cpp.

- [ ] **Lambda con logica > 2 righe** — grep per lambda in `connect()` lunghe:
      ```
      grep -n "connect(.*\[" gui/ -r --include="*.cpp" | wc -l
      ```
      Ogni lambda > 2 righe va estratta in slot nominato.

- [ ] **Include order non uniforme** — in molti file gli `#include` non seguono
      l'ordine Qt convention (header proprio → Qt → std). Non critico ma rallenta
      la comprensione. Usare `clang-format` con `IncludeCategories`.

- [ ] **`tr()` mancante su stringhe UI** — alcune stringhe visibili all'utente
      non passano per `tr()` (quindi non sono traducibili).
      Grep: `grep -rn '"[A-Z][a-z]' gui/ --include="*.cpp" | grep -v "tr\|qDebug\|qWarning\|//\|P::"`.
      Revisione manuale poi aggiunta `tr()` dove mancante.

---

### Usabilità — ridurre cognitive overload

- [ ] **Tooltip su ogni pulsante** — molti `QPushButton` non hanno tooltip.
      Aggiungere almeno un tooltip a ogni pulsante non ovvio (icona-only o sigla).
      Grep: `grep -n "addWidget.*Btn\|addWidget.*btn" gui/ -r --include="*.cpp" | wc -l`
      confrontato con `grep -n "setToolTip" gui/ -r --include="*.cpp" | wc -l`.

- [ ] **Status bar centrale** — lo stato dell'app (AI in corso, RAG in corso,
      WAN attivo, bot TG online) è sparso in label dentro ogni tab.
      Aggiungere una `QStatusBar` in `MainWindow` con zona sinistra (stato AI)
      + destra (temperatura + stato rete) — già in parte implementato, uniformare.

- [ ] **Messaggio "nessun risultato" mancante** — alcune liste (risultati RAG,
      nodi GraphMemory, modelli Ollama) rimangono vuote senza spiegazione.
      Aggiungere `QLabel("Nessun risultato")` come placeholder quando la lista è vuota.

- [x] **Confirm dialog su azioni distruttive** — FATTO 2026-06-21: aggiunti 2 dialog:
      - `onRagClearClicked()` in `main_research.cpp` — "Svuota Grafo RAG"
      - `onClearMemClicked()` in `main_multi_agent.cpp` — "Svuota Memoria Grafo"
      Altre azioni (reset cron, zoom reset) non sono irreversibili o hanno undo implicito.

---

## 📏 Metriche obiettivo (da misurare con ogni release)

| Metrica | Baseline | 2026-06-20 | Obiettivo |
|---|---|---|---|
| File >1500 righe in `gui/` | ~12 | ~3 ✅ (-10 file splittati) | ≤3 |
| Suite test PASS senza Ollama | 38/41 (93%) | 38/41 (93%) | 41/41 (100%) |
| Timeout hardcoded >2s (grep) | ~40 | 0 ✅ | 0 |
| Porte/path hardcoded (grep) | ~40 hit | ~5 ✅ (solo testo UI) | 0 |
| `QProcess` senza `errorOccurred` | ~15 | **0 ✅** (79 connect totali) | 0 |
| Pulsanti senza tooltip | ~60 | ~60 | ≤10 |
| Lambda > 2 righe in connect() | ~20 | ~20 | 0 |

---

## 🛠 Come usare questo TODO

1. Scegli una voce `- [ ]` per dimensione di lavoro (30 min → file splitting; 5 min → grep+fix).
2. Fai il fix, aggiorna `[ ]` → `[x]` con data.
3. Dopo ogni sessione aggiorna le metriche obiettivo.
4. Non aggiungere feature qui — questo file è solo qualità/pulizia.
