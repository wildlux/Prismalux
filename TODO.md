# Prismalux — TODO prossima sessione

> Aggiornato: 2026-05-14 | Build: `cd gui && cmake --build build -j$(nproc)`

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
- [~] **[UX] ImpostazioniPage = 7080 righe, God Dialog** — refactor UI di largo respiro; rimandato a sessione dedicata
