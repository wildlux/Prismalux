# Prismalux — TODO prossima sessione

> Aggiornato: 2026-05-15 | Build: `cd gui && cmake --build build -j$(nproc)`

## Test GUI da completare

### Intelligenza Artificiale
- [x] **Chat RAG** ✅ — testato 2026-05-14: streaming OK 46.6s, "Lavoro completato"; fix toggle ▶️ think block (2026-05-14: thinking via message.thinking non passava a extractedThink)
- [~] Pipeline agenti — rimosso/sostituito dall'Agente Autonomo (gestione automatica)
- [x] **Agente Autonomo** ✅ — testato 2026-05-14: "quanto fa 5×7?" → THOUGHT+ACTION(calc)+OBSERVATION+risposta "35"
- [x] **Think mode** ✅ — testato 2026-05-14: Off/Auto/On visibili, Auto attivo, budget slider 2×
- [x] **Tool use nativo** ✅ — 2026-05-14: checkbox "🔧 Tools" nella toolbar; in modalità Chat attiva Ollama function calling (leggi_file/lista_file/calc/cerca_web/python); in Agente Autonomo sempre attivo
- [x] **Drag & drop RAG inline** ✅ — 2026-05-14: pulsante "📎 RAG" nella barra input apre zona RagDropWidget collapsibile; contesto iniettato nella pipeline

### Strumenti / Multimedia
- [x] **Multimedia: Audio AI** ✅ — testato 2026-05-14: cattura voce OK
- [x] **Multimedia: Graphviz** ✅ — 2026-05-14: rendering DOT→PNG funziona (dot v14.1.2); shape "roundedbox" non esiste → usa "box" o "rectangle"
- [x] **Stable Diffusion locale** ✅ — 2026-05-14: fix deadlock pipe (no image_b64 su stdout con --out), progress bar step-by-step, callback_on_step_end; per testare: `pip install diffusers transformers accelerate torch` poi "Controlla"

### LAN & WAN
- [x] **Android server QR** ✅ — 2026-05-14: aggiunto "📱 QR Connetti" sempre visibile (mostra http://IP:porta senza avviare il server); QR APK e Pagina Download rimangono attivi solo con server ON
- [x] **App rete locale multi-PC** ✅ — 2026-05-14: route `/web` in LanServer serve pagina HTML chat (dark UI, streaming ndjson, history, system prompt); pulsante "🌐 Chat Web" apre browser
- [x] **GNS3** ✅ — 2026-05-14: server v2.2.59 su porta 3080 OK; topologia SW1+PC1+PC2 creata via REST API; fix: compute_id='local' obbligatorio in POST /nodes (aggiunto ai prompt AI); gns3fy installato

### Impostazioni
- [x] **Cambia tema** ✅ — testato, 23 temi funzionanti
- [x] **Modalità calcolo** ✅ — testato 2026-05-14: CPU/GPU/Misto visibili, salvataggio OK
- [~] NPU — N/A (hardware non presente)

### Generale
- [x] **Tutti i 10 tab** ✅ — testato 2026-05-14: nessun crash

## Fix audit sicurezza + qualità ✅ (2026-05-14)

### 🔴 CRITICAL
- [x] **[SEC] Sanitizza `msg` in `sendError()`** — escape `[\r\n"\\]` con QRegularExpression, troncato a 200 char
- [x] **[SEC] Limite max buffer `s.buf`** — cap 4 MB, disconnette client con 400 Bad Request

### 🟠 HIGH
- [x] **[SEC] Token accesso LAN server** ✅ — 2026-05-14: campo 🔑 token (EchoMode=Password) in LanWanPage; salvato in QSettings `lan/accessToken`; header `Authorization: Bearer TOKEN` controllato su `/api/tags`, `/api/chat`, `/api/generate`, `/knowledge`; `/apk`, `/`, `/web` pubblici; Chat Web inietta l'header in JS fetch; 401 Unauthorized se token errato
- [x] **[SEC] Rate limiting `/knowledge`** — max 32 KB payload + max 10 req/min per IP + JSON null-check
- [x] **[C++] QPointer guard lambda con timer** — 4 occorrenze fixate: GNS3, FreeCAD (strumenti), FreeCAD+OBS (app_controller), Cytoscape (ricerca)
- [x] **[UX] Onboarding first-run dialog** — wizard 3 step (backend · modello · tema); attivato a 800ms dal primo avvio; chiave `setup/done` in QSettings

### 🟡 MEDIUM
- [x] **[UX] Raggruppa tab RicercaPage** — tooltip su tutti i 9 tab + ordine riorganizzato (Genera · Cerca · Scienze)
- [x] **[UX] Spinner GNS3 exec** — progress bar 4px indeterminata, visibile durante esecuzione Python
- [x] **[C++] JSON null-check** — `fromJson().isNull()` check su handleChat, handleGenerate, handleKnowledge
- [x] **[C++] Guard processo GNS3** — `m_gns3ExecProc` tracciato; kill+waitForFinished prima di avviare nuovo processo

### 🟢 LOW
- [x] **[Python] requirements.txt MCPs** — creati per stable_diffusion_local, knowledge_mcp, gns3_mcp
- [x] **[UX] Timestamp bolle chat** — orario HH:mm accanto a "Tu" in buildUserBubble; bolle AI avevano già il timestamp
- [x] **[UX] Export chat** — già presente: "💾 Esporta" (.md/.html/.txt) + "📄" (PDF) nella toolbar chat

## Implementati in questa sessione (2026-05-14 II)

- [x] **Cerca Paper/Brevetti** ✅ — Tab "🔍 Cerca Paper/Brevetti" in Ricerca; sorgenti: arXiv (Atom XML), Semantic Scholar (JSON), USPTO (JSON); "Analizza con AI" streaming; nessuna API key richiesta

## Già testato ✅
- Blender: "crea un cubo" + "crea una sfera" (2026-05-13)
- VAD con sox: "Conversa" in Intelligenza Artificiale (2026-05-14)
- Cron: job eseguito, fix `<think>` strip nel log (2026-05-14)

## Non testabile (software non installato)
- Anki, Godot GDScript, Cytoscape, RDKit, Bioconda, Avogadro

---

## Audit esperto II — 2026-05-14 (sicurezza + UI + C++)

### 🔴 CRITICO
- [x] **[BUG] `buildUserBubble` fa `toLower()` sul messaggio** ✅ — rimosso `text.toLower()`; il messaggio viene inviato all'AI preservando maiuscole/minuscole
- [x] **[BUG] QSettings inconsistenza `lan_wan_page.cpp`** ✅ — fix: `QSettings ss;` → `QSettings("Prismalux","GUI")` nel save/load token (puntava a GUI.conf sbagliato)
- [~] **[C++] QEventLoop::exec() in matematica_page** — by design: sostituisce `waitForFinished()` mantenendo eventi Qt attivi (paint/resize); basso rischio re-entrancy per operazioni brevi <20s

### 🟠 HIGH
- [x] **[SEC] Token LAN quote non sanitizzate nel JS** ✅ — escape `\` e `'` prima di iniettare il token nel literal JS della pagina /web
- [~] **[SEC] Chat history in chiaro su disco** — noto, bassa priorità per tool locale; non aggiunto in questa sessione
- [~] **[SEC] Python REPL senza sandbox** — Docker CodeInterpreter è opt-in; REPL base è per design (utente che scrive codice proprio)
- [~] **[C++] Connessioni permanenti a `m_ai`** — pattern corretto: context=`this` disconnette automaticamente quando il widget è distrutto; non è un bug attivo
- [x] **[UX] Spinner in Strumenti/Ricerca/AppController** ✅ — aggiunta `QProgressBar` indeterminata 4px sotto l'area output in tutte e 3 le pagine; mostrata/nascosta insieme ai pulsanti run/stop

### 🟡 MEDIUM
- [x] **[UX] Toggle 👁 per campo token LAN** ✅ — pulsante flat 👁 checkable che alterna `EchoMode::Normal ↔ Password`
- [x] **[UX] Empty state lista chat vuota** ✅ — `refreshChatList()`: se nessuna sessione, mostra placeholder non-selezionabile "💬 Nessuna chat salvata — Inizia una conversazione"
- [x] **[UX] Ricerca nella chat history** ✅ — QLineEdit "🔍 Cerca chat..." sopra la lista sidebar con filtro in tempo reale (case-insensitive, nascondi item non matching)
- [x] **[C++] Timeout + retry su QNetworkReply** ✅ — `req.setTransferTimeout(15000)` su arXiv/Semantic Scholar/USPTO; messaggio "⏱ Timeout (15s) — premi Cerca per ritentare"
- [x] **[C++] `/tmp/output.stl` e `/tmp/output.step` hardcoded** ✅ — prompt FreeCAD aggiornato: usa `Path.home()/'Desktop'/'output.*'`

### 🟢 LOW
- [~] **[C++] `QSettings("Prismalux","GUI")` ad ogni bolla** — overhead accettabile (poche decine di bolle/sessione); non ottimizzato
- [x] **[TEST] Test per feature recenti** ✅ — `test_onboarding.cpp`: 18 test (CAT-A onboarding, CAT-B token QSettings, CAT-C rate limiter, CAT-D token state machine); CAT-G token aggiunto a `test_lan_server.cpp` (5 test); tutti 23 PASS
- [x] **[UX] Tooltip pulsanti principali** ✅ — aggiunto setToolTip a: ragClear, pdfCarica, blenderPing, blenderEsegui, officeBridge, officeEsegui, freecadPing, freecadEsegui, btnRun; setAccessibleName su btnRun
- [x] **[C++] `agenti_page_stream.cpp` refactoring** ✅ — 2026-05-14: estratti 6 handler da `onFinished()` nei file dedicati (pipeline/byzantine/knowledge); stream.cpp da 957→330 righe; dispatcher pulito 8 righe
- [x] **[UX] ImpostazioniPage God Dialog** ✅ — 2026-05-14: divisa in 7 file (861+1972+1900+1010+942+414+387 righe); fix `ThemeVisual` mancante in `_visuale.cpp`; build OK

---

## Audit esperto III — 2026-05-15 (sicurezza + UI + C++ + Python)

### 🔴 IMMEDIATO

- [x] **[Python] MCP SyntaxError — godot_mcp/server.py:68** ✅ — variabili locali `_name`/`_ntype`/`_path`/`_prop` prima delle f-string; rimossi `args[\"...\"]` dentro `{}`
- [x] **[Python] MCP SyntaxError — freecad_mcp/server.py:101** ✅ — stessa fix: `_out`, `_op`, `_bname`; anche `op_map[_op]` al posto di `op_map[args['operation']]` dentro l'f-string
- [x] **[Python] MCP SyntaxError — kicad_mcp/server.py:78** ✅ — `_lib`, `_fp`, `_x`, `_y` estratti prima del blocco codice; fix anche in `tool_export_gerber` (`_outdir`)
- [ ] **[SEC] LAN server HTTP puro — nessun TLS** — chat, token e knowledge viaggiano in chiaro; usare `QSslServer` + certificato self-signed generato automaticamente al primo avvio
- [x] **[SEC] `/apk` endpoint pubblico senza auth** ✅ — aggiunto `/apk` all'insieme `isApi` in `lan_server.cpp`; ora richiede Bearer token come le API

### 🟠 IMPORTANTE

- [ ] **[SEC] Token Bearer in QSettings plain text** — `lan_wan_page.cpp:158-170` scrive il token in `~/.config/Prismalux/GUI.conf`; usare `QKeychain` o cifratura AES locale
- [x] **[SEC] `m_llamaBin` non validato prima di `QProcess::start`** ✅ — `ai_client.cpp`: regex `[;&|` + "`" + `$<>\\]` + `QFileInfo::isExecutable()` prima di start; emit error se path invalido
- [ ] **[SEC] Nessun rate limiting su `/api/chat`** — solo `/knowledge` ha il limiter; un client malevolo può saturare Ollama
- [ ] **[UX] Nessun undo/redo esplicito nell'editor** — `QPlainTextEdit` ha Ctrl+Z nativo ma nessuna azione custom (inserisci-da-AI, incolla-template) va nello stack; aggiungere `QUndoStack` + shortcut documentati
- [x] **[UX] Nessuna conferma su azioni distruttive** ✅ — `QMessageBox::question` aggiunto ai 3 pulsanti "Inserisci nell'editor" (AI panel, Agentica, Reverse Eng.) quando l'editor ha già del codice
- [ ] **[C++] Lambda `[this]` senza `QPointer` su reply async** — `ai_client.cpp` connessioni `[this, reply]` su `QNetworkReply`; se reply distrutto prima della lambda → crash; usare `QPointer<QNetworkReply>`
- [ ] **[C++] `QTimer::singleShot` con raw `this` in ~8 file** — pattern `[this]{ m_xxx->... }` senza guard; aggiungere `QPointer<>` sulle variabili member catturate

### 🟡 PIANIFICABILE

- [ ] **[SEC] Timing attack token comparison** — `lan_server.cpp` confronta token con `==`; usare confronto constant-time
- [ ] **[SEC] Nessun log accesso persistente** — impossibile forensics post-incidente; aggiungere append su file `~/.prismalux/access.log` con IP + route + timestamp
- [ ] **[UX] Accessibilità zero (WCAG)** — 1 sola `setAccessibleName` in tutto il progetto; screen reader inutilizzabile; aggiungere `setAccessibleName`/`setTabOrder` sistematici
- [ ] **[UX] i18n assente** — 30 `tr()` su ~15.000 righe di UI; tutto hardcoded in italiano; introdurre `tr()` sistematico e `.ts` file per future traduzioni
- [ ] **[UX] Scrollbar non tematizzate** — ThemeManager applica QSS ma non alle scrollbar → look OS-nativo che rompe coerenza visiva su Windows/KDE
- [ ] **[UX] Font size hardcoded** — `monoFont.setPointSize(11)` in 5 file; su display 4K risulta minuscolo; usare `QFontDatabase` + DPI scaling
- [ ] **[C++] `QSettings` aperto ad ogni chiamata** — 12+ istanze `QSettings("Prismalux","GUI")` sparse; creare un singleton `AppConfig` con cache in memoria
- [ ] **[C++] Aggiungere `clang-tidy` al CMakeLists** — `ENABLE_SANITIZERS` c'è, ma zero analisi statica; aggiungere target `tidy` con `.clang-tidy` committato
- [ ] **[C++] `Q_DISABLE_COPY` su singleton** — `ThemeManager` e `AiClient` copiabili per errore

### 🟢 TECH DEBT

- [ ] **[UX] Feedback mancante su operazioni lunghe** — fetch modelli, avvio llama-server: l'utente vede blocco senza spinner in `strumenti_page.cpp` e `impara_page.cpp`
- [ ] **[UX] Drag-and-drop file mancante** — su editor codice e su RAG loader sarebbe naturale; nessun `setAcceptDrops(true)` nei widget chiave
- [ ] **[Python] requirements.txt senza pin precisi** — `requests>=2.31` invece di `==2.32.3`; build non riproducibile tra 6 mesi
- [ ] **[Python] MCP non installabili come package** — nessun `pyproject.toml`; dipendono dal CWD; aggiungere `pyproject.toml` minimo a ogni MCP
