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

- [ ] **`main_ai_ui.cpp` (3906 righe)** — separare in:
  - `main_ai_chat.cpp` — bolle, sessioni, input
  - `main_ai_pipeline.cpp` — già separato; verifica overlap
  - `main_ai_toolbar.cpp` — header, modello, DPI

- [ ] **`main_simulator_algos.cpp` (3775 righe)** — separare in:
  - `main_sim_sorting.cpp` — algoritmi ordinamento
  - `main_sim_graph.cpp` — BFS/DFS/Dijkstra
  - `main_sim_canvas.cpp` — paint / animazione

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

- [ ] **Fetch modelli Ollama** — `fetchModels()` reimplementata in almeno 4 file
      (`settings_ai.cpp`, `main_oracle.cpp`, `main_sci_compute.cpp`, `main_wan_compute.cpp`).
      Centralizzare in `AiClient::fetchModels(callback)` già esistente —
      verificare che tutti i file usino quello e non versioni locali.

---

### Magic numbers e costanti sparse

- [x] **Timeout hardcoded** — FATTO 2026-06-20: aggiunte 8 costanti in `prismalux_paths.h`, sostituite tutte le occorrenze in 20+ file. Zero timeout hardcoded rimasti sopra 2s.

- [x] **Porte non centralizzate** — FATTO 2026-06-20: main_maintenance.cpp (3 occorrenze), main_learn.cpp, main_wan_server.cpp ora usano P::kOllamaPort. Residui solo in testo HTML/tooltip non funzionale.

- [x] **Stringhe path hardcoded** — FATTO 2026-06-20: aggiunto P::tmpDir() in prismalux_paths.h; rag_graph.cpp e main_programming_vpn.cpp ora usano P::tmpDir(). dataDir() e memoryDir() erano già presenti.

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

- [ ] **`QProcess::error()` non connesso** — in molti slot che avviano processi
      Python/shell manca la connessione a `QProcess::errorOccurred`. Grep:
      `grep -rn "QProcess" gui/ --include="*.cpp" | grep -v "errorOccurred\|error()"`.
      Ogni `QProcess` deve connettere `errorOccurred` e mostrare il motivo all'utente.

- [ ] **File I/O senza check errore** — `QFile::open()` spesso non verifica il
      valore di ritorno in percorsi non critici. Grep:
      `grep -n "\.open(" gui/ -r --include="*.cpp" | grep -v "if\s*(.*open\|!.*open"`.
      Ogni `open()` deve avere `if (!f.open(...)) { log errore; return; }`.

- [ ] **SQLite senza check `lastError()`** — query fallite silenziosamente in
      `graph_memory.cpp`, `wan_tasks.db`, `sci_nodes.db`.
      Aggiungere macro:
      ```cpp
      #define SQL_CHECK(q) if ((q).lastError().isValid()) \
          qWarning() << "[SQL]" << (q).lastError().text();
      ```
      e applicarla a ogni `QSqlQuery::exec()`.

---

### Portabilità — dipendenze Linux-specifiche

- [x] **Path KaTeX hardcoded** — FATTO 2026-06-20: `katexBaseUrl()` in latex_view.h cerca: bundle assets/katex/ → /usr/share/javascript/katex/ (Linux) → /usr/share/katex/ → fallback. Portabile su Windows/macOS.

- [ ] **Comandi shell non portabili** — `main_sci_compute.cpp` usa `bash -c`,
      `ulimit`, `bwrap` senza check della piattaforma.
      Aggiungere:
      ```cpp
      #ifdef Q_OS_LINUX
          // bwrap / ulimit
      #else
          // fallback: solo timeout
      #endif
      ```

- [ ] **`~/` nei path su Windows** — `QDir::homePath()+"/.prismalux/"` è corretto
      ma alcune stringhe usano ancora `"~/.prismalux/"` direttamente.
      Grep: `grep -rn '"~/' gui/ --include="*.cpp"` → sostituire con `P::dataDir()`.

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

- [ ] **File con suffisso `_page`** — verificare che nessun file nuovo in `gui/pages/`
      usi il suffisso vietato `*_page.h/.cpp`.
      Grep: `find gui/pages -name "*_page.h" -o -name "*_page.cpp"`.

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

- [ ] **Confirm dialog su azioni distruttive** — alcune azioni (svuota cronologia,
      cancella nodo GraphMemory, reset WAN queue) non chiedono conferma.
      Ogni azione irreversibile deve avere `QMessageBox::question` con Sì/No.

---

## 📏 Metriche obiettivo (da misurare con ogni release)

| Metrica | Baseline | 2026-06-20 | Obiettivo |
|---|---|---|---|
| File >1500 righe in `gui/` | ~12 | ~3 ✅ (-10 file splittati) | ≤3 |
| Suite test PASS senza Ollama | 38/41 (93%) | 38/41 (93%) | 41/41 (100%) |
| Timeout hardcoded >2s (grep) | ~40 | 0 ✅ | 0 |
| Porte/path hardcoded (grep) | ~40 hit | ~5 ✅ (solo testo UI) | 0 |
| `QProcess` senza `errorOccurred` | ~15 | ~15 | 0 |
| Pulsanti senza tooltip | ~60 | ~60 | ≤10 |
| Lambda > 2 righe in connect() | ~20 | ~20 | 0 |

---

## 🛠 Come usare questo TODO

1. Scegli una voce `- [ ]` per dimensione di lavoro (30 min → file splitting; 5 min → grep+fix).
2. Fai il fix, aggiorna `[ ]` → `[x]` con data.
3. Dopo ogni sessione aggiorna le metriche obiettivo.
4. Non aggiungere feature qui — questo file è solo qualità/pulizia.
