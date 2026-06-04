# Prismalux — TODO pendenti

> Aggiornato: 2026-06-03 | Versione: 2.9

---

## 🤖 Dev Agent LangGraph (in sviluppo 2026-06-03)

> Agente AI locale per modificare il codice di Prismalux in autonomia: legge file, genera patch, compila, si corregge sugli errori.

### Stack
- **LangGraph** (Python) come orchestratore del grafo agente
- **Ollama** con modello coding dedicato (default: `deepseek-coder:6.7b` già installato; opzionale: `qwen2.5-coder:3b` ~2GB più leggero)
- **MCP** `devagent_mcp/` — server Python IPC JSON stdin/stdout (pattern identico al Telegram Bot)
- **Tab "Dev Agent"** in AppController [7] → tab 10

### Grafo LangGraph
```
START → read_context → generate_patch → apply_patch → compile
                              ↑ (errori compilazione) ←──────
                              ↓ (OK)
                         run_tests → done
```

### Tool del grafo
- `read_file(path)` — legge un file del progetto
- `list_files(pattern)` — glob sul progetto
- `write_file(path, content)` — scrive la modifica
- `bash(cmd, timeout)` — esegue cmake, ctest, git diff
- `search_code(query)` — grep nel progetto

### UI in AppController
- `QLineEdit` — task in linguaggio naturale ("Aggiungi tooltip al pulsante X in main_ai_ui.cpp")
- `QComboBox` — scegli modello coding (deepseek-coder:6.7b / qwen2.5-coder:3b)
- `QTextEdit` — log step-by-step (Read→Generate→Compile→Fix→Done)
- `QPushButton` — Avvia / Ferma / Applica diff / Annulla
- `QTextEdit` — diff finale (colorato rosso/verde)

### TODO
- [x] **`MCPs/devagent_mcp/server.py`** — 29KB: DevAgentState, 5 nodi LangGraph (con fallback loop Python), tool read/write/bash/search_code, ollama_chat HTTP diretto, parser unified diff + rollback automatico — 2026-06-03
- [x] **`MCPs/devagent_mcp/requirements.txt`** — langgraph, langchain-community, langchain-ollama, unidiff (tutti opzionali con fallback) — 2026-06-03
- [x] **Tab Dev Agent in AppController** — UI Qt completa: task input, model combo (deepseek-coder/qwen2.5-coder), log step-by-step, diff colorato, Avvia/Ferma/Installa — `main_app_controller.h/cpp/slots.cpp` — 2026-06-03
- [ ] **Pulsante download qwen2.5-coder:3b** nella sezione LLM consigliati

---

## 🆕 Sessione 2026-06-03 — Nuovi TODO (turno 6, da PROMPT.txt 19:35)

### 🔧 Bug / Fix
- [x] **Codice fiscale check digit corretto** — bug: posizioni pari con lettere usavano `10+(c-'A')` invece di `(c-'A')`; fix in `pratico_calcs.h`; test `casoRealeRossiMario` aggiornato a RSSMRA90A01H501W; LBLPLA89B15C351G ora corretto — 2026-06-03
- [x] **2 pulsanti Gestore LLM non si comprimono** — `setFixedWidth(200)` → `setMinimumWidth(220)/setMaximumWidth(280)` — `settings_ai.cpp` — 2026-06-03
- [x] **Tile 4ª colonna tronca** — aggiunta QLabel con setWordWrap(true) nella band inferiore della card tema — `settings_visual.cpp` — 2026-06-03

### 🎨 UI / UX
- [x] **Bottone "ℹ" → "ℹ Informazioni"** — rinominato in `main_ai_ui.cpp` riga 152 — 2026-06-03
- [x] **Bottone PDF con testo** — `"\xf0\x9f\x93\x84  PDF"` in `main_ai_ui.cpp` riga 138 — 2026-06-03
- [x] **"Scarica LLM" → "Scarica"** — abbreviato label header in `mainwindow.cpp` riga 645 — 2026-06-03
- [x] **"AI Locale" → "Gestione LLM"** — tab rinominata in `settings_main.cpp` righe 116+187 — 2026-06-03
- [x] **Agentica: selettore modello spostato** — integrato in `buildAgenticaToolbar()` a sinistra di "linguaggio" — `main_programming.cpp` — 2026-06-03
- [x] **Pulsante "Traccia" fisso sotto scroll** — spostato fuori dalla QScrollArea in `buildLeftPanel()`; `outerLay->addWidget(btnPlot/btnReset)` dopo `sc` → sempre visibile senza scroll — `main_graph.cpp` — 2026-06-03

### 🛑 Stop & controllo compilazione
- [x] **Stop compilazione llama.cpp** — pulsante "⏹ Ferma" accanto a Compila; `onLlamaStopClicked()` fa `kill()` su m_proc2/m_proc3 — `main_customize.h/cpp` — 2026-06-03
- [x] **Controllo compilazione recente** — se llama-server compilato <24h fa mostra QMessageBox::question prima di ricompilare — `main_customize.cpp` — 2026-06-03

### 🔊 Voce / TTS
- [x] **espeak-ng** — sostituita nota con "Usa Piper TTS per qualità superiore"; fallback code nel pulsante Parla invariato — `settings_voice.cpp` — 2026-06-03
- [ ] **Test Whisper live** — aggiungere test suite `test_stt_whisper_live` per la funzionalità STT in Impostazioni→Gestione LLM→Voce audio

### 📋 Feature
- [x] **Copia pip clipboard** — pulsante 📋 aggiunto in: main_tools_file (3x), widget_stable_diffusion, main_lan_wan GNS3, main_maintenance NPU — 2026-06-03
- [x] **Chat: Canc = conferma, Shift+Canc = 5s undo** — eventFilter su m_chatList; QMessageBox::question per Canc; label "Annulla (5s)" + QTimer per Shift+Canc — `mainwindow.h/cpp/slots.cpp` — 2026-06-03
- [ ] **Emoji Telegram nel software** — usare le emoji Unicode standard (già supportate da Qt); verificare che il font sistema abbia coverage completa; eventualmente bundlare Noto Emoji
- [x] **Tool use per modelli abilitati** — 9 tool disponibili: calc, fetch_url, ricerca, leggi_file, lista_file, python, **search_rag** (RAG locale), **graph_memory** (GraphMemory SQLite), **get_knowledge** (Knowledge Base); bolla HTML stilizzata in-place con status running→done/error — `main_ai_tools.cpp`, `main_ai_pipeline.cpp` — 2026-06-04
- [x] **Grafico 3D: Punti/Wireframe/Superficie + movimento Blender** — `m_renderModeCombo` per Scatter3D/Grafo3D; `setRenderMode(int)` slot; Blender orbit (MMB/Alt+LMB) + pan (Shift+MMB) — `main_graph.h/cpp`, `main_graph_canvas.cpp` — 2026-06-03
- [x] **Sezione Lavoro web** — tab "💼 Lavoro" in webchat.html + endpoint `GET /api/lavoro?q=QUERY` in lan_server.cpp; ricerca case-insensitive su azienda/ruolo/sede — 2026-06-03
- [x] **Quiz errori con spiegazione** — aggiunta introduzione "Ripassiamo insieme..." + legenda Verde/Rosso nel dialog "Rivedi errori" — `main_learn.cpp` — 2026-06-03

### 📖 Documentazione
- [x] **README.md API esterne** — tabella "Riferimenti API ufficiali" con Blender/FreeCAD/Bioconda/GDScript/KiCAD/Ollama pip — `README.md` — 2026-06-03
- [x] **Regola test** — aggiunta regola in CLAUDE.md: ogni nuova suite test deve essere registrata nella tabella Suite con nome ctest, categoria e numero PASS — 2026-06-03
> Build: `python3 build.py`  (Linux/macOS/Windows — oppure `cmake --build build_gui -j$(nproc)` su Linux)

---

## 🆕 Sessione 2026-06-03 — Nuovi TODO (turno 2)

### 🔧 Bug / Fix llama.cpp
- [x] **llama-server crash durante startup → non ricade più su Ollama** — `onServerProcFinished()` ora distingue crash-durante-startup (m_healthTimer attivo) da terminazione normale; in caso di crash startup mostra diagnostica+ultime righe di log senza commutare il backend — `mainwindow_slots.cpp` — 2026-06-03
- [x] **Timeout caricamento modello** — `MAX_HEALTH_TICKS` portato da 180s a 300s (5 minuti) per modelli 33b+ su CPU — `mainwindow_slots.cpp` — 2026-06-03
- [x] **`--flash-attn` argomento invalido** — nuove versioni llama.cpp richiedono `--flash-attn auto|on|off` invece del flag booleano; corretto in `mainwindow.cpp` — 2026-06-03

---

## 🆕 Sessione 2026-06-03 — Nuovi TODO

### 🔧 Bug / Fix preesistenti (corretti oggi)
- [x] **`LanServer::onLlmFinishedQueued`** — dichiarazione orfana in `lan_server.h` rimossa — 2026-06-03
- [x] **`QuizPage::onRivediErroriClicked`** — spostata da `private slots` a `public slots` — 2026-06-03
- [x] **`lan_server.cpp` include** — `lavoro_data.h` → `main_jobs_data.h` — 2026-06-03

### 🎮 Feature
- [x] **Sfida "Rivedi errori"** — struct `WrongAnswer`, salvataggio sbagliati, pulsante con count, QDialog scrollabile opzioni colorate+spiegazione — `main_learn.h/cpp` — 2026-06-03

---

## 🚀 BOINC-like WAN Compute — TODO 2026-06-03

> Goal: trasformare il WAN Compute da "coda distribuita basilare" a sistema fault-tolerant simile a BOINC per ricerca personale (analisi AI, Python, grafici distribuiti su 2-10 PC LAN).

### 🔴 Critici — Fault Tolerance (senza questi si perde il lavoro)
- [x] **Riassegnazione task su disconnessione nodo** — `onWanNodeDisconnected()`: task "running" del nodo morto tornano "pending"; `retryCount++`; dopo 3 tentativi → "failed" — `main_lan_wan.cpp` — 2026-06-03
- [x] **Heartbeat 30s server→nodo** — `QTimer` 30s invia `{"t":"ping"}`; nodo risponde `{"t":"pong"}`; se nodo non risponde per 90s → rimosso → task riassegnati — `main_lan_wan.h/cpp` — 2026-06-03
- [x] **Priority scheduler** — campo `priority` (0=bassa/1=normale/2=alta) in `WanTask`; `wanDispatch()` sceglie il task pending con priorità massima invece del primo FIFO — `main_lan_wan.h/cpp` — 2026-06-03
- [x] **`startedAt` tracking** — campo `QDateTime startedAt` in `WanTask`; settato in `wanDispatch()`; ETA stimata nella stats label — 2026-06-03
- [x] **Stats label BOINC-style** — label sotto le tabelle: nodi idle/working, task in coda/running/completati/falliti — `main_lan_wan.cpp` — 2026-06-03

### 🟡 Sicurezza
- [x] **TLS self-signed LAN** — `QSslServer` + `_ensureCert()` + checkbox "Abilita TLS" in Manutenzione LAN; fallback HTTP se openssl non disponibile — `lan_server.h/cpp`, `main_maintenance_lan.cpp` — 2026-06-03
- [x] **Coda FIFO multi-utente LAN** — `PendingLlmRequest` + `m_llmQueue` (max 10); `handleChat/Generate` accodano se occupato; `closeStreamSession/onClientDisconnected` servono il prossimo — `lan_server.h/cpp` — 2026-06-03
- [x] **Parser HTTP hardening** — whitelist metodi (GET/POST/PUT/DELETE/PATCH/HEAD/OPTIONS); blocco path con null byte o >2048 char; validazione Content-Length (negativo o >25MB → 400); tutti con `disconnectFromHost()` — `lan_server.cpp` — 2026-06-03

### 🟢 Dashboard BOINC-style
- [x] **Throughput real-time** — `m_wanThroughputLbl` aggiornato ogni 5s: task/ora (finestra 60min) + ETA stimata — `main_lan_wan.h/cpp` — 2026-06-03
- [x] **ETA globale** — calcolata da media `startedAt→now` sui task completati, divisa per worker attivi — 2026-06-03
- [x] **Esportazione CSV** — `m_wanExportBtn` → QFileDialog → CSV completo (id/kind/stato/nodo/retry/priority/created/startedAt/payload/result) — 2026-06-03
- [x] **Nodo multi-worker** — `WanWorker` struct (sock+ai+pollTimer per worker); SpinBox 1-4; `onWanCliConBtnClicked` crea N worker ognuno indipendente; 8 slot nominati per-worker; `wanWorkerHandleTask` dispatcher 28 task — `main_lan_wan.h/cpp` — 2026-06-03

### 📦 Headless server (produzione LAN)
- [x] **`--server` CLI flag** — `main.cpp::runHeadlessServer()` con `--port` e `--token`
- [x] **systemd user unit** — `EXPORT/linux/prismalux-server.service` creato — 2026-06-03
- [x] **Watchdog crash-restart** — `Restart=on-failure` + `StartLimitBurst=5` + `TimeoutStartSec/StopSec` + `OLLAMA_HOST=127.0.0.1` nell'env della unit — `EXPORT/linux/prismalux-server.service` — 2026-06-03
- [x] **Log strutturato JSON** — `appendAccessLog` scrive JSON a riga singola `{"t":"…","ip":"…","m":"…","p":"…"}`; rotazione automatica >10 MB (mantiene `.1`) — `lan_server.cpp` — 2026-06-03

---

## 🆕 Sessione 2026-06-02 — Nuovi TODO

### 🔧 Bug / Fix UI
- [x] **QR code** — aumentato di ~20px (+8%): 260→280 dialog, 256→276 inline; testo spostato sotto con `\n\n` — 2026-06-02
- [x] **Docker "Scarica" bottone** — nascondere il bottone dopo che l'immagine è stata scaricata con successo — 2026-06-02
- [x] **π calcolo** — bug `TypeError: object of type 'int' has no len()` in `matematica_page.cpp` (rimosso `.__len__()`) — 2026-06-02
- [x] **Grafico 2D/3D** — rimosso stretch factor 1 da `m_paramStack` — 2026-06-02
- [x] **Voce Piper** — aggiunto scroll indipendente su pannello sinistro (Piper) e destro (Whisper), non si sovrappongono più — 2026-06-02
- [x] **Simulatore** — aggiunto `setMaxVisibleItems(20)` + `ScrollBarAsNeeded` sul combo algoritmi — 2026-06-02
- [x] **Sfida** — salvataggio domande sbagliate (`WrongAnswer`), bottone "Rivedi errori" con dialog scroll+colori — 2026-06-02

### 🎨 UI / UX
- [x] **Selettore LLM globale** — aggiunto `ModelComboBox` in `ricerca_page.cpp`: tab Paper e Brevetto ora hanno selettore modello — 2026-06-02
- [x] **Visuale → Aspetto** — 4 colonne invece di 3 per la griglia dei temi — 2026-06-02
- [x] **Visuale → Grafico** — spostato "preset stile grafico" sotto "carattere etichette" — 2026-06-02
- [x] **AI Locale + Connessione** — `buildUpdateGroup` spostato nella `colsRow` a destra di `buildAdvancedConfigGroup` in `manutenzione_page.cpp` — 2026-06-02

### 📊 Feature / Analisi
- [x] **Analisi 1 e Analisi 2** — aggiunto `GraficoCanvas` con auto-plot al cambio topic (`plotEx`); split orizzontale testo+grafico — 2026-06-02

### 🔨 Build / Infrastruttura
- [x] **llama.cpp compilazione Vulkan** — fallback automatico a `GGML_VULKAN=OFF` se cmake detect "glslc" mancante; avviso nel log — 2026-06-02
- [x] **Anki MCP server v0.18.5** — aggiornato `MCPs/anki_mcp/server.py` con API compatibile v0.18.5: +create_notes_bulk, +get_note, +update_note, +delete_notes, +get_note_types, +get_tags, +sync_anki, +get_cards_due — 2026-06-02

### 🔁 Rinomina codice
- [x] **Rinomina pagine GUI** — 96 file rinominati con `git mv`; tutti gli include in `mainwindow.cpp`, `mainwindow_slots.cpp`, `gui/pages/` e `gui/tests/` aggiornati; `CMakeLists.txt` aggiornato — 2026-06-02

### 📱 Messaging
- [x] **Telegram** — aggiunta sezione "Contatti promozionali" in `buildTelegramTab()`: lista contatti, messaggio, "Invia a tutti" via bot API Telegram — 2026-06-02
- [x] **WhatsApp** — aggiunta sezione "Contatti promozionali" in `buildWhatsAppTab()`: lista numeri, messaggio, "Invia a tutti" via bridge URL — 2026-06-02

---

## 📂 Feature pendenti

- [x] **Auto-copia documento in RAG/** — quando l'utente carica un file (drag & drop o browse)
  per l'indicizzazione RAG e il file **non risiede già** in `Prismalux/RAG/`,
  copiarlo automaticamente dentro quella cartella prima di indicizzarlo.

  **Perché:** i documenti fuori da `RAG/` vengono indicizzati in sessione ma non
  ritrovati al riavvio (RagGraph scansiona solo `~/prismalux_rag_docs/` e `P::root()+"/RAG/"`).
  La copia garantisce persistenza senza che l'utente debba spostarli manualmente.

  **Comportamento atteso:**
  - File già in `RAG/` → indicizza senza toccare nulla
  - File esterno → copia in `RAG/<nomefile>` (sovrascrittura opzionale se esiste già)
  - Mostra messaggio: `"📄 Copiato in RAG/ — il documento è ora persistente"`
  - Se la copia fallisce (permessi, disco pieno) → indicizza comunque dalla posizione originale
    e avvisa l'utente

  **Punti di intervento:**
  - `agenti_page_ui.cpp` — zona drop RAG (drag & drop PDF/txt/md)
  - `impostazioni_page_ai.cpp` o `impostazioni_page_slots.cpp` — browse file per indicizzazione
  - `rag_graph.cpp::addFile()` — eventuale copia automatica a livello di engine

---

## 🚨 CRITICO — WAN Compute è RCE non autenticato (audit codice 2026-05-31)

> Audit diretto di `lan_wan_page.cpp` (2977 righe). **Confermato nel codice**, non sospetto.
> Questo è il problema di sicurezza più grave del progetto: esecuzione di codice arbitrario
> da rete, senza autenticazione né cifratura, in entrambe le direzioni.

### 🔴🔴 Bloccante assoluto — NON esporre il WAN Compute finché non risolto

- [x] **Worker esegue comandi shell arbitrari dal server, senza auth** — opt-in checkbox + sandbox 2026-06-01
  - Il worker (`onWanCliSockReadyRead`, riga 2131) riceve un messaggio `task` e chiama
    `wanCliHandleTask` che per `kind=="shell_cmd"`/`"git_cmd"` esegue **`bash -c payload`**
    e per `python_repl`/`eval_script`/`matplotlib_plot` esegue **`python3 -c payload`**
    (righe 2837-2867) — payload grezzo, nessun controllo token, nessuna conferma.
  - **Impatto:** chiunque controlli (o impersoni, manca TLS) il server a cui ti connetti
    ti manda `{"t":"task","kind":"shell_cmd","payload":"<comando>"}` → RCE sulla tua macchina.
  - Nota: `math_expr` (riga 2849) è gestito con escaping "sicuro per espressioni" — quindi
    il rischio era noto, ma `shell_cmd`/`python_repl` restano completamente aperti.

- [x] **Server registra ed esegue task da chiunque, senza verificare il token** — token auth hello 2026-06-01
  - `onWanNodeReadyRead` accetta `hello`/`poll`/`result`/`spawn_tasks` da qualsiasi socket;
    **non c'è alcun controllo del Bearer token** (caricato ma mai verificato lato WAN).
  - `spawn_tasks` (riga 1919) lascia a un client non autenticato **iniettare nuovi task**
    (`kind`+`payload` arbitrari, es. `shell_cmd`) nella coda, che il server poi **distribuisce
    ad altri nodi onesti** → propagazione tipo worm dell'RCE.

- [x] **WAN bind 0.0.0.0 senza TLS** — checkbox "Esponi su tutte le interfacce" (default OFF→127.0.0.1); check Ollama esposto su avvio; UI TLS self-signed con openssl — 2026-06-02
  - Il server WAN ascolta su tutte le interfacce; nessuna cifratura → MITM banale su WiFi
    può impersonare il server e iniettare task shell ai worker.

- [x] **Capability `"shell"` di default** — `lan_wan_page.cpp` — rimossa 2026-06-01
  - I nodi annunciano `caps = {"ai","shell"}` di default → opt-in automatico all'esecuzione
    di comandi shell. Dovrebbe essere opt-out esplicito con conferma utente.

### Rimedi (ordine)
1. **Auth obbligatoria su entrambi i lati WAN**: verificare il Bearer token in `onWanNodeReadyRead`
   (server) e autenticare il server prima di eseguire task (worker). Rifiutare connessioni senza token.
2. **Sandbox/whitelist per i task pericolosi**: `shell_cmd`/`python_repl`/`eval_script` dietro
   conferma esplicita per-task o disabilitati di default; eseguire in sandbox (container/seccomp).
3. **TLS sul canale WAN** o restrizione a `127.0.0.1` + tunnel fidato.
4. **Capability opt-out**: niente `"shell"` di default; l'utente abilita esplicitamente.
5. Finché non fatto: **disabilitare la modalità Rete LAN del WAN Compute** o avviso a tutto schermo.

---

## 🔍 Superfici di sicurezza ancora NON auditate (2026-05-31)

> Onestà sullo stato: auditati a fondo solo `lan_server.cpp/.h` e `lan_wan_page.cpp`.
> Le seguenti superfici NON sono state verificate — non assumere che siano sicure.

- [x] **18 plugin MCP Python** — audit completato 2026-06-02: 10 corretti (bioconda whitelist, avogadro/graphviz/rdkit path traversal, freecad/godot/kicad code injection, ollama/opencode/tinymcp arg validation), 5 già sicuri (anki, cytoscape, gns3, knowledge, obs)
  - Verificare che nessun plugin costruisca comandi shell / path da input non validato
    (command injection, path traversal). Validare i parametri lato server prima dell'inoltro.

- [x] **~20 file C++ con `QProcess`** — audit completato 2026-06-02: 2 vulnerabilità HIGH corrette (WAN `math_expr` eval injection in `main_lan_wan.cpp:2930`; LAN `expr`/`simplify` Python injection in `lan_server.cpp:3868`); restanti 23 file SICURI
  - File coinvolti: `programmazione_page_slots.cpp`, `app_controller_page.cpp`,
    `strumenti_file_page.cpp`, `ai_client.cpp`, `agenti_page_tools.cpp`, `pratico_page.cpp`,
    `lavoro_page.cpp`, `mainwindow_slots.cpp`, ecc.
  - Cercare costruzione di comandi/argomenti da input utente o da output LLM senza whitelist.

- [x] **App Android BLE audit** — `ble_crypto.h`: QRandomGenerator::securelySeeded() per salt/key, QSettings("Prismalux","Mobile"), isEmpty() guard; `ble_page.cpp`: Authorization flag (link-layer pairing); `ble_page.h`: m_cryptoEnabled=true di default — 2026-06-03

- [x] ~~`GraphMemory` SQL injection~~ — coperto da test (`test_graph_memory`, 65 test incl. SQL injection).

---

## 🔐 Hardening sicurezza LAN/Web (audit codice 2026-05-31)

> Audit del codice esistente (`lan_server.cpp/.h`, `prismalux_paths.h`).
> Verificati direttamente: token, bind, rate limiting, sandbox, path.
> **Da verificare** (sospetti fondati, non confermati): XSS web chat, robustezza parser HTTP.

### 🔴 Critici

- [x] **Token nell'URL `?token=` su HTTP in chiaro** — rimosso fallback API 2026-06-01
  - Il TLS è disabilitato di proposito (`lan_server.cpp:148-150`) ma il token è accettato
    come query string e incluso nel QR code → viaggia in chiaro, finisce nei log proxy,
    sniffabile su WiFi. Il Bearer token autentica ma **non cifra il canale**.
  - **Rimedio:** accettare il token SOLO via header `Authorization: Bearer`; rimuovere il
    fallback `?token=` e l'inclusione del token nell'URL del QR (passare il token in un
    campo separato che l'app Android usa per costruire l'header).

- [x] **Auth opzionale (token vuoto = nessuna auth)** — token auto-generato UUID 32-char se vuoto, salvato in keyring, segnale `tokenAutoGenerated` — 2026-06-02
  - Se l'utente non imposta un token, `/api/chat`, `/api/generate`, ecc. sono aperte a
    chiunque sulla rete. È opt-in; per un server che espone un LLM dovrebbe essere opt-out.
  - **Rimedio:** generare un token di default al primo avvio del server; disattivazione
    solo esplicita con avviso.

### 🟡 Importanti

- [x] **Bind LAN configurabile** — `setBindAddress(QHostAddress)` pubblico; headless usa `LocalHost` di default; UI può passare IP LAN specifico — `lan_server.h/cpp` — 2026-06-03
  - Il server ascolta su tutte le interfacce, non solo la LAN. Su hotspot/rete pubblica
    è esposto oltre l'intenzione. Esiste già `P::kLocalHost` documentata come
    "unico valore accettato per sicurezza" ma non applicabile qui (serve raggiungibilità dal telefono).
  - **Rimedio:** bind sull'interfaccia LAN specifica, oppure avviso esplicito quando
    l'interfaccia attiva è una rete pubblica/non fidata.

- [x] **KaTeX da CDN jsdelivr** — endpoint `/katex/` serve da `/usr/share/javascript/katex/` con path-traversal protection + cache 24h; CDN rimosso — 2026-06-02
  - La chat web carica JS/CSS da `cdn.jsdelivr.net`: rischio supply-chain, privacy
    (il telefono contatta un terzo) e niente offline. Sul desktop KaTeX è locale
    (`/usr/share/javascript/katex/`) → coerenza rotta.
  - **Rimedio:** servire KaTeX come risorsa Qt locale anche nella web app.

- [x] ~~XSS nella web chat~~ — **VERIFICATO SICURO** (`lan_server.cpp:2333`)
  - I messaggi chat (utente + output LLM) sono inseriti via `d.textContent=t` → escaping
    automatico del browser. Nessun XSS sul flusso chat.

- [x] **XSS residuo nella lista offerte lavoro** — sostituito innerHTML con createElement/textContent 2026-06-01
  - La lista `/api/lavoro` (dati esterni Indeed) e alcuni pannelli tool costruiscono il DOM
    con `innerHTML` concatenando stringhe. Se un titolo/descrizione offerta contiene HTML
    → injection nel browser del client.
  - **Rimedio:** usare `textContent`/`createElement` anche qui, o sanificare i dati esterni
    prima dell'inserimento.

- [x] **CORS `Access-Control-Allow-Origin: *`** — rimosso del tutto (app Android nativa + web app same-origin non ne hanno bisogno) — 2026-06-02
  - Qualsiasi pagina web aperta sul telefono/PC del client può chiamare le API del server
    (il token in `localStorage` resta protetto dalla same-origin per la lettura, ma le
    richieste cross-origin partono comunque). Header di sicurezza X-Frame-Options/nosniff
    già presenti — buono — ma il CORS wildcard è troppo permissivo.
  - **Rimedio:** restringere l'origin agli host attesi o rimuovere il CORS se non serve.

- [x] **Parser HTTP manuale verificato** — limite 4MB buffer totale (riga 442), Content-Length max 25MB, whitelist 7 metodi, null-byte+len>2048 → 400, nessun difetto trovato — 2026-06-03
  - Parsing manuale di header/`Content-Length`/body: classe di bug nota
    (request smuggling, edge case encoding). Nessun difetto evidente trovato.
  - **Azione:** fuzzing del parser; valutare migrazione a un parser HTTP collaudato.

### 🧹 Manutenibilità correlata

- [x] **Estrarre la web UI dalle stringhe C++** — `lan_server.cpp` da 4574 → 2136 righe (-2438); HTML/JS ora in `gui/lan_web/webchat.html` (509 righe, 14 placeholder) + `gui/lan_web/index.html`; caricati via `QFile(":/lan/webchat.html")` + `replace()` placeholder `{{MODEL}}`/`{{AUTH_HEADERS_JS}}` — `lan_web.qrc` — 2026-06-03

---

## 🚀 Produzione LAN — readiness operativa (2026-05-31)

> Cosa manca per far girare il server su una macchina della LAN in modo stabile.
> Verificato nel codice: server embedded nella GUI, singolo `m_ai`, singolo `m_streamSock`.

### 🔴 Bloccanti

- [x] **Modalità headless (server senza GUI)** — `--server CLI flag` + `runHeadlessServer()` + systemd unit — già implementato 2026-06-03
  - *Headless* = avviare SOLO il server da CLI, senza aprire finestre, così gira su un
    mini-PC anche senza monitor e resta attivo 24/7.
  - **Rimedio:** flag tipo `prismalux --server --port N` che istanzia `LanServer` senza
    `QApplication` GUI (o con `QGuiApplication`/`QCoreApplication`); avvio come servizio
    (systemd user unit su Linux, servizio/Task Scheduler su Windows) con restart-on-crash.

- [x] **Coda FIFO multi-utente** — `PendingLlmRequest` + `m_llmQueue` (max 10) + `serveLlmQueue()` — già implementato 2026-06-03
  - *Coda* = se due client chattano insieme le richieste vengono servite in fila, una alla
    volta, invece di mescolare gli stream (oggi il 2° stream ruba il socket al 1°).
  - *Isolamento* = ogni utente ha contesto/cronologia separati; oggi tutti condividono lo
    stesso `~/.prismalux` → un client può vedere dati/risposte di un altro.
  - **Rimedio minimo:** coda FIFO delle richieste LLM (serializzazione esplicita).
  - **Rimedio completo:** un `AiClient` per sessione + namespace dati per-utente.

### 🟡 Importanti

- [x] **`/api/launch` disabilitato in headless** — flag `m_headless`; `setHeadless(true)` in `runHeadlessServer`; risponde 403 se headless — `lan_server.h/cpp`, `main.cpp` — 2026-06-03
  - Whitelist presente (no comando arbitrario) ma chiunque col token apre un terminale
    sul display della macchina server. Su host headless non ha senso ed è una capability
    pericolosa esposta in rete.
  - **Rimedio:** disabilitare `/api/launch` in modalità server/headless o renderlo opt-in.

- [x] **IP DHCP auto-aggiornamento QR** — `QTimer` 30s in `buildLanAndroidTab()` → `onIpWatchTick()`: se IP cambia aggiorna QR inline e mostra avviso in `m_urlDisplayLbl` — `main_lan_wan.h/cpp` — 2026-06-03
  - **Rimedio:** IP statico/reservation per il server; sull'host aprire SOLO la porta del
    server e tenere Ollama (`11434`) in ascolto solo su localhost, mai esposto in LAN.

- [x] **Backup DB** — GroupBox "Backup dati" in Impostazioni→Pulizia: copia graph_memory.db, rag_graph.db, chat_history.db, access.log in cartella scelta con timestamp; pulsante "Apri cartella dati" — `settings_system.cpp` — 2026-06-03
  - **Rimedio:** rotazione log; backup periodico dei DB `~/.prismalux` (chat, RAG, GraphMemory).

---

## 🔒 Sicurezza — punti aggiuntivi da chiudere (2026-05-31)

> Per stare tranquilli oltre all'hardening LAN già elencato sopra.

### 🔴 Da fare prima di esporre il server

- [x] **Ollama esposto solo in locale** — `onOllamaCheckDone()` controlla sia `ss -tlnp | grep 11434` sia env `OLLAMA_HOST`; avviso arancione se esposto; check attivato ad ogni avvio server WAN (non solo in exposeAll) — `main_lan_wan.cpp` — 2026-06-03
  - Ollama non ha auth: se ascolta su `0.0.0.0` chiunque in LAN usa l'LLM senza token,
    bypassando del tutto l'auth di Prismalux.

- [x] **TLS opzionale su LAN** — `QSslServer` + `_ensureCert()` + checkbox "Abilita TLS" in Manutenzione LAN — già implementato 2026-06-03
  - Senza TLS chat e dati LLM viaggiano in chiaro sul WiFi (combinato col token-in-URL
    già segnalato è un doppio problema). Codice cert già predisposto (`lan_server.cpp:115`).

### 🟡 Igiene generale

- [x] **Audit segreti** — GroupBox in Impostazioni→Pulizia: 5 controlli (.env in .gitignore, *.key in .gitignore, permessi 0600, token LAN non in QSettings, KNOWLEDGE_USER/RAG in .gitignore) — `settings_system.cpp` — 2026-06-03
  - Nessun token in `QSettings`/codice (già usa QKeychain); verificare che `KNOWLEDGE_USER/`,
    `RAG/`, `.env`, `*.key` siano in `.gitignore` e con permessi `0600`.

- [x] **Rate limiting esteso a tutti gli endpoint** — aggiunto checkHeavyRateLimit 6/min per whisper/graphviz/mcp/launch 2026-06-01
  - Estendere a `/api/whisper`, `/api/graphviz`, `/api/launch`, `/api/mcp` per evitare
    abuso/DoS (whisper e graphviz lanciano processi).

- [x] **Limite upload + tipi file** — Whisper: 413 se >25 MB, 415 se non audio/*; MCP: validazione method regex + params oggetto — 2026-06-02
  - Verificare limite dimensione e validazione tipo/estensione prima di passare a processi
    esterni (python3, dot, whisper).

- [x] **Validazione input MCP** — verificato JSON object + method regex `[a-zA-Z0-9/_-]{1,64}` + params oggetto prima dell'inoltro — 2026-06-02
  - Assicurarsi che i parametri passati ai MCP siano validati lato server, non solo lato MCP.

- [x] **Scanner dipendenze OSV** — tab "Dipendenze" in SecurityAnalyzerPage: legge `requirements.lock`, POST a `api.osv.dev/v1/querybatch`, mostra CVE in rosso/verde — `main_security.h/cpp` — 2026-06-03

---

## 🛡️ Analizzatore di Sicurezza Difensiva

> Scopo: aiutare l'utente a **trovare e correggere** falle — mai a sfruttarle.
> Posizionamento naturale: sub-tab in **Programmazione [4]** o pannello dedicato in **Ricerca [6]**.

- [x] **SecurityAnalyzerPage** — 4 agenti Ollama paralleli in AppController [7], 2026-06-01

  ### Analisi codice (LLM locale)
  - Carica file sorgente (C++/Python/JS/Bash) o incolla testo
  - LLM analizza e segnala:
    - Credenziali/token hardcoded nel codice
    - Comandi shell non sanificati (injection risk)
    - Buffer/memory issues (C/C++: strcpy, gets, sprintf senza bounds)
    - Dipendenze con versioni note vulnerabili
    - Path traversal, open redirect, SSRF nei server HTTP
  - Output: lista falle con **severità** (critica/alta/media/bassa) + **rimedio suggerito**
  - "Spiega il rischio" — bottone per approfondimento AI sulla singola falla

  ### Scanner dipendenze (offline)
  - Legge `requirements.txt`, `requirements.lock`, `CMakeLists.txt`, `package.json`
  - Confronta con una copia locale del database OSV/NVD (aggiornabile manualmente)
  - Segnala: pacchetto → CVE → CVSS score → versione sicura disponibile
  - Esporta report in Markdown/PDF

  ### Audit configurazione locale
  - Controlla porte aperte in ascolto sul PC (confronta con quelle note di Prismalux)
  - Verifica permessi file sensibili (`KNOWLEDGE_USER/`, token LAN, `.claude/`)
  - Controlla che i token LAN siano nel keychain e non in QSettings in chiaro
  - Segnala `.env` o file con credenziali senza `.gitignore`

  ### Audit dipendenze Prismalux stesso
  - Hash SHA-256 dei GGUF in `KNOWLEDGE_USER/model_hashes.json` — già implementato, da integrare nella UI
  - Verifica integrità dei 18 MCP Python (hash file sorgente al primo avvio, confronto ad ogni riavvio)
  - Alert se un MCP viene modificato esternamente

  ### Stack previsto
  - LLM locale (Ollama) per analisi semantica del codice
  - `pip-audit` o parsing offline di OSV JSON per CVE dipendenze
  - `ss -tlnp` / `QNetworkInterface` per porte locali
  - Nessuna chiamata cloud, nessun invio di codice all'esterno

---

## 🧪 Test mancanti (gap analysis 2026-05-30)

> Suite attuali: 51 (48 no-Ollama, 3 richiedono Ollama). Le aree sotto non hanno ancora suite ctest.

### 🔴 Critici — logica complessa, zero test

- [x] **test_pratico_finanza** — 27 PASS: TestNormalizzaComune (7) + TestCercaBelfiore (8) + TestCalcolaCodiceFiscale (12) — 2026-06-03
- [x] **test_wan_compute_tasks** — 33 PASS: costruzione+sicurezza (7), protocollo TCP (7), formato JSON (8), math_expr (5), sicurezza shell (6) — 2026-06-03
- [x] **test_rag_graph_pipeline** — 25 PASS: costruzione (7), parsing JSON LLM (8), ricerca (5), segnali (5) — 2026-06-03

### 🟡 Importanti — funzionalità usate quotidianamente

- [x] **test_docker_sandbox** — 29 PASS, 2 SKIP (Docker assente): costruzione, docker --version, PythonExec `print('ok')`, sicurezza sandbox — 2026-06-03
- [x] **test_translitter** — 37 PASS: langFence helper (15), costruzione widget (12), kLangs unicità (10) — 2026-06-03
- [x] **test_file_parser** — 38 PASS: costruzione+tab (16), pdftotext (5), CSV (8), file non validi (9) — 2026-06-03
- [x] **test_repl_python** — 23 PASS, 1 SKIP (race condition doppio start nota): costruzione (6), python3 check (4), I/O asincrono (5), robustezza (8) — 2026-06-03

### 🟠 Bot locali — App Controller (tab già presenti, da implementare)

- [x] **Telegram Bot locale** — `app_controller_page.cpp::buildTelegramTab()`: python-telegram-bot subprocess, IPC JSON stdin/stdout, whitelist ID, risposte AI locale, pulsanti Avvia/Ferma, log in-app — già implementato 2026-06-02
  - Token bot da @BotFather configurabile in Impostazioni
  - Messaggi in entrata → AI locale risponde
  - Comandi: `/ask`, `/status`, `/task`, `/stop`
  - Notifiche proattive (task WAN completato, alert sistema)
  - Whitelist ID Telegram (sicurezza)
  - Stack: python-telegram-bot o Telethon via MCP (`github.com/chigwell/telegram-mcp`)

- [x] **WhatsApp Bot locale** — sezione "Bot AI Rispondente" aggiunta a `buildWhatsAppTab()`: whitelist numeri, checkbox auto-risposta, Avvia/Ferma, polling bridge ogni 2s, risposta AI locale via `m_ai->chat()`, log messaggi — 2026-06-03
  - Bridge locale via Baileys/whatsapp-web.js (no API ufficiali)
  - QR code autenticazione WhatsApp Web integrato nel tab
  - Messaggi in entrata → prompt AI → risposta automatica
  - Comandi: `!ask`, `!status`, `!immagine`
  - Whitelist numeri autorizzati
  - Nessun account Business richiesto
  - Stack: MCP whatsapp-mcp (`github.com/lharries/whatsapp-mcp`)

### 🟢 Nice-to-have — feature specializzate

- [x] **test_astrale** — 31 PASS: RicercaPage (12), NatalChartWidget (10), AstroCalc::compute() (9) — 2026-06-03
- [x] **test_blhm_rab0l** — 38 PASS: Rab0lCanvas (12), BLHM Engine C (14), UI BLHM/RAB₀-L (12) — 2026-06-03
- [x] **test_gns3_mcp** — 18 PASS, 2 SKIP (GNS3 non avviato): costruzione (8), server mock (2 skip), azioni/combo (8) — 2026-06-03
- [x] **test_multi_agente_live** — 13 test: CAT-A costruzione (5), CAT-B decomposizione JSON (2, Ollama), CAT-C esecuzione SubTask (3, Ollama), CAT-D GraphMemory persistenza (4, Ollama) — TIMEOUT 300s — 2026-06-03

---

## ✅ Implementati

### Sessione 2026-05-29 — TODO completati

- [x] **FEAT-1 parallelo** — Pool di 3 `AiClient` (`kMaxParallel = 3`)
  in `AgentiMultiPage`. `runNextPendingTask()` avvia TUTTI i task con
  dipendenze soddisfatte contemporaneamente (fino a 3). `initPool()` sinc
  i client col modello corrente ad ogni decomposizione.

- [x] **Cross-pollination agenti→grafo RAG** — `AgentiMultiPage::setExtRagMemory()`
  riceve la `GraphMemory` di RagGraph; `onTaskResultDone()` scrive ogni
  risultato come nodo `"fact"` anche nel grafo RAG. Collegato in
  `MainWindow::buildMultiAgentTab()`.

- [x] **Auto-trigger RagGraph** — `RicercaPage::onAutoRagTrigger()` collega
  `ImpostazioniPage::indexingFinished` → avvio automatico del RagGraph
  dopo 800 ms (delay per flush FS). Cablato in `ensureSettingsDialog()`.

### Sessione 2026-05-29 — FEAT-1 Multi-Agente + FEAT-2 Grafo RAG

- [x] **GraphMemory** (`gui/graph_memory.h/cpp`) — fondazione comune:
  SQLite-backed, nodi+archi tipizzati, BFS neighbours, ricerca testuale,
  export DOT/JSON/TXT, pruneByImportance, segnale `changed()` real-time

- [x] **FEAT-1 Multi-Agente** (`gui/pages/agenti_multi_page.h/cpp`) — tab [9] 🕸️:
  MasterAgent decompone JSON → SubTask con `depends_on`,
  sub-agenti sequenziali con contesto condiviso, sintesi finale,
  GraphMemory live (nodi, archi, export TXT, clear), viste Grafo DOT / JSON

- [x] **FEAT-2 RagGraph** (`gui/rag_graph.h/cpp`) — estrazione LLM:
  scansiona `~/prismalux_rag_docs/` e `Prismalux/RAG/`, estrae entità+relazioni
  via LLM (JSON), persiste in GraphMemory separata (`~/.prismalux/rag_graph.db`)

- [x] **FEAT-2 Tab 🕸️ Grafo RAG** in RicercaPage:
  lista nodi filtrabile, click→dettaglio+vicini, Graphviz PNG dark-theme,
  DOT source, progresso file per file, stop, svuota

### Sessione 2026-05-29 — Voce + Donazione

- [x] **TTS + STT web** — tab "🎙️ Voce" (ex Whisper):
  SpeechSynthesis (voce/velocità configurabili), MediaRecorder→/api/whisper,
  Invia in Chat, barra livello microfono

- [x] **TTS Android** — box "🔊 Sintesi Vocale" in AudioPage:
  QTextToSpeech, pulsante Parla/Stop, voce italiana

- [x] **Fix STT Android** — rimosso `m_ai->transcribeAudio()` inesistente,
  ora usa `uploadWhisper()` direttamente

- [x] **Donazione PayPal** — README badge + sezione, `.github/FUNDING.yml`,
  pulsante in Impostazioni→Ringraziamenti e APK Android Info page

### Sessioni precedenti

- [x] LaTeX KaTeX rendering (Analisi 1/2, output AI) + LatexView widget
- [x] Randomizer 🔀 52 formule Risolvi Passi
- [x] Test SymPy CAT-E (15 test, 62/62 PASS)
- [x] Scheda TFR con C.F. automatico (D.M. 1976 + Belfiore)
- [x] WAN Calcolo Distribuito (TCP:11600, 28 task, cron)
- [x] Ollama MCP (18° plugin, cache SQLite, 5 tool)
- [x] Quiz CCNA 209 domande (15 temi)
- [x] DPI dpiScale(), i18n QTranslator, supply chain hash
