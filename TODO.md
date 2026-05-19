# Prismalux — TODO prossima sessione

> Aggiornato: 2026-05-19 (sessione VIII) | Build: `cd gui && cmake --build build -j$(nproc)`

---

## App Mobile — 2026-05-19 (sessione VIII)

### Implementato ✅
- [x] **[QUIZ] Fullscreen + risposta corretta** ✅ — `studio_page.cpp` riscritto con `QStackedWidget` interno: indice 0=menu, indice 1=quiz a tutto schermo; quando risposta errata mostra la risposta esatta + spiegazione in rosso/verde
- [x] **[QUIZ] Database CCNA locale** ✅ — `pages/quiz_ccna_db.h/cpp` nuovi: 64 domande hardcoded macro `Q()`, 11 temi OSI/TCP-IP·Switching·Routing·IPv6·Sicurezza·DHCP·Wireless·WAN·IOS·Automazione·QoS; nessuna chiamata AI per CCNA
- [x] **[CHAT] 3 pulsanti backend** ✅ — ☁️ Cloud (configura host/apikey/model via QInputDialog) | 🌐 Server | 📱 Locale; banner colorato stato backend; backend cloud salva in QSettings `cloud/host|key|model`
- [x] **[CHAT] TTS — Leggi ad alta voce** ✅ — pulsante 🔊 Leggi sotto il log; usa `QTextToSpeech` (HAVE_TTS) con locale italiano; fallback: copia negli appunti
- [x] **[BLE] Chat Bluetooth Classic RFCOMM** ✅ — `ble_page.cpp` riscritto: tab Scanner BLE + tab Chat BT Classic (SPP); `QBluetoothServer` lato server; messaggi in plaintext UTF-8 linea per linea; `appendChatMsg()` con timestamp
- [x] **[Impostazioni] Fix scroll orizzontale indesiderato** ✅ — slot `onResetHScroll()` connesso a `horizontalScrollBar::valueChanged` che forza reset a 0
- [x] **[Impostazioni] QR Code per collegamento PC→Mobile** ✅ — già presente: desktop `onQrConnectBtnClicked()` mostra QR con `http://IP:PORT/web?token=TOKEN`; mobile `SettingsPage` ha pulsante "📷 Scansiona QR dal PC"
- [x] **[Impostazioni] Ringraziamenti** ✅ — sezione 🙏 con QGroupBox rich text in `info_page.cpp`
- [x] **[MCP Add-ons]** ✅ — nuova pagina `McpAddonsPage` (indice 6): 8 MCP predefiniti con toggle on/off; aggiungi MCP custom via QInputDialog; salvataggio in QSettings
- [x] **[Audio] Pagina Trascrizione Audio** ✅ — `audio_page.h/cpp` nuovi: registrazione con QMediaRecorder (HAVE_MULTIMEDIA), cronometro, analisi AI (3 modi: Whisper server / descrizione testuale / riassumi); "Invia in Chat" via signal `transcriptionReady`
- [x] **[Hamburger ☰] Fix icona X** ✅ — sostituito immagine con testo U+2630 (font 22pt); chiudi drawer usa ← invece di ✕
- [x] **[Lambda C++] Fix lavoro_page + misure_page** ✅ — sostituiti `[this]{ onFinished(""); }` con slot `onAborted()`; pulsanti azione lavoro usano `setProperty("sysPrompt")` + `onActionBtnClicked()` invece di lambda cattura

### Mancante / TODO prossima sessione

#### 🔴 NON IMPLEMENTATO (richiederebbe librerie esterne)
- [ ] **[Misure] AR — Realtà Aumentata** — l'utente chiede di misurare pareti con AR/VR. Qt6 non supporta ARCore nativamente. Opzioni:
  - **Rapida**: aggiungere widget `QPainter` 2D con piantina stanza interattiva (trascina pareti) + calcolo automatico superfici
  - **Completa**: integrazione ARCore tramite JNI Android (fuori scope Qt); richiede Plugin Qt 3D + ARCore SDK
  - **Raccomandata**: implementare prima la versione QPainter 2D; è utile e realizzabile in una sessione
- [ ] **[Audio] Trascrizione Whisper reale** — attualmente `onTranscribeClicked()` usa AI testuale. Per trascrizione vera serve:
  - Endpoint `POST /v1/audio/transcriptions` (openai-compatible) in `AiClient`
  - Upload `multipart/form-data` del file WAV
  - Campo URL server Whisper in `settings_page.cpp`

#### 🟠 TECH DEBT MOBILE
- [ ] **[Lambda] settings_page.cpp** — 5 lambda rimaste in `connect()` (righe 411, 475, 520, 633, 642, 659). Convertire a slot nominati come da regola progetto
- [ ] **[Lambda] camera_page.cpp:89** — 1 lambda in `connect(m_sendBtn, &QPushButton::clicked, this, [this]{...})`. Convertire a slot `onSendBtnClicked()`
- [ ] **[Audio] Livello microfono reale** — `onRecordTick()` simula il livello con valore pseudo-casuale. Per il livello reale usare `QAudioSource` + callback buffer + RMS calculation
- [ ] **[Misure] Piantina stanza 2D** — aggiungere un `QWidget` con `paintEvent()` che disegna la planimetria della stanza in base ai valori di `QDoubleSpinBox`; update live al variare delle dimensioni

#### 🟡 MIGLIORAMENTI
- [ ] **[Quiz] Più domande CCNA** — `quiz_ccna_db.cpp` ha 64 domande; target 200+ per coprire tutti i 60 domini exam 200-301; aggiungere anche CompTIA Network+
- [ ] **[Quiz] Salva punteggio** — store in QSettings il punteggio cumulativo per tema + data ultimo quiz; mostrare statistiche nella pagina Studio
- [ ] **[Chat] Cronologia persistente** — attualmente la chat è in memoria solo. Salvare i turni in SQLite locale (già usato dal quiz) per recuperarli tra sessioni
- [ ] **[BLE] Scoperta peer attiva** — `BlePage` lato chat BT avvia il server ma il client deve connettersi manualmente. Aggiungere scansione dispositivi Bluetooth Classic + UI per scegliere a quale connettersi
- [ ] **[BLE] Cifratura chat BT** — la chat è in chiaro (volutamente per semplicità). Opzione: AES-256 con pre-shared key scambiata via QR code

---

---

## App Mobile — 2026-05-15

### Android
- [x] **SMARTPHONE** ✅ — `ANDROID/SMARTPHONE/CMakeLists.txt` creato; referenzia `../android_app/` con define `PRISMALUX_FORM_FACTOR_SMARTPHONE`; build: `cmake -B build-smartphone -DCMAKE_TOOLCHAIN_FILE=<Qt>/qt.toolchain.cmake -DANDROID_ABI=arm64-v8a && make apk`
- [x] **TABLET** ✅ — `ANDROID/TABLET/CMakeLists.txt` + `tablet_nav_rail.cpp/.h` creati; define `PRISMALUX_FORM_FACTOR_TABLET`; `TabletNavRail` = sidebar verticale Material 3 da 80px che sostituisce la bottom bar dello smartphone
- [ ] **[C++] Adatta `mainwindow.cpp` di android_app ai form factor** — leggere `PRISMALUX_FORM_FACTOR_TABLET` a runtime (`#ifdef`) per scegliere tra `BottomBar` e `TabletNavRail`; layout split-panel (sidebar + stack) per tablet

### iOS (PySide6 — PROTOTIPO DESKTOP)
- [x] **IPHONE** ✅ — `IOS/IPHONE/app.py` + `main_window.py` + `pages/{chat,studio,settings}_page.py`; finestra 390×844dp; bottom nav bar + chat con bolle; `pip install PySide6 && python app.py`
- [x] **IPAD** ✅ — `IOS/IPAD/app.py` + `main_window.py`; finestra 820×1180dp; sidebar laterale 220px stile split-view; riusa le pagine iPhone
- [ ] **[DEPLOY] PySide6 NON supporta iOS nativamente** — opzioni reali per produzione:
  - **BeeWare** (`briefcase create iOS`) — Python nativo su iPhone/iPad, toolkit Toga
  - **Kivy + kivy-ios** — `toolchain build python3` → Xcode → App Store
  - **Qt6 C++ for iOS** — porta `android_app/` a iOS (stesso codice, toolchain Qt Xcode)
  - **Raccomandato**: portare `android_app/` (Qt6 C++) su iOS; è la strada più veloce dato che la base esiste già

---

## Printing Press concepts — 2026-05-15

### Implementato
- [x] **gns3_mcp v2** ✅ — SQLite cache (`~/.prismalux/gns3_cache.db`): tabelle projects/templates/nodes/links con FTS5 su nodi; tool `sync` (API→SQLite); `get_topology` compound (progetto+nodi+link in un tool); `search_nodes` FTS5; `compact=true` per output token-efficiente; fallback API live se cache vuota
- [x] **knowledge_mcp search** ✅ — tool `search_knowledge` (regex full-text, case-insensitive, top 5 estratti con contesto); main loop convertito ad `asyncio`; `asyncio.to_thread()` per `fcntl.flock` I/O bloccante (non blocca più l'event loop)
- [x] **sd_local cache** ✅ — SQLite `~/.prismalux/sd_models_cache.db`: tabelle models + generations; `_record_model_use()` e `_record_generation()` chiamati dopo ogni run; `--list-models` mostra modelli usati in precedenza con device/usi/data

### Prossimi passi (Printing Press)
- [ ] **Ollama MCP** — generare un MCP Ollama con SQLite cache lista modelli (sync da `/api/tags`), `get_model_info` compound, search by size/name; sostituisce fetch-on-demand in AiClient
- [ ] **Install Go + Printing Press** — per generare MCP in Go di qualità superiore: `sudo apt install golang-go && git clone github.com/mvanhorn/cli-printing-press`

---

## Audit esperto V — 2026-05-15 (sicurezza · UI · Python · C++)

### Nuovi problemi identificati

#### 🔴 CRITICO
- [~] **[C++] Lambda connect() — conversione in corso** — partiti da ~512, ora **238 rimanenti** (2026-05-15). Completati: `mainwindow.cpp`, `impara_page.cpp`, `agenti_page_tts.cpp`, `manutenzione_page_bugs.cpp`, `grafico_page.cpp`, `quiz_page.cpp`, `lan_server.cpp`, `ai_client.cpp`. Prossimi: `ricerca_page.cpp` (26), `agenti_page_ui.cpp` (25), `manutenzione_page.cpp` (17).
- [ ] **[SEC] Supply chain MCP** — `requirements.txt` senza versioni pinned (es. `PySide6>=6.7.0`). Un `pip install --upgrade` silenzioso può portare una dipendenza malevola. Fix: `pip-compile --generate-hashes` → `requirements.lock`.

#### 🟠 IMPORTANTE
- [ ] **[UI] Nessun feedback visivo su errori di rete** — quando `AiClient` fallisce (timeout, Ollama giù, JSON malformato) l'utente vede il bottone tornare idle senza un messaggio. Il modello corretto: banner rosso contestuale + pulsante "Riprova". File coinvolti: tutti i `*_page.cpp` che chiamano `requestChat()`.
- [ ] **[UI] DPI/scaling non gestito su Linux Wayland** — alcuni widget usano dimensioni hardcoded in px (es. `setFixedWidth(80)`, `setFixedHeight(52)`) che risultano minuscoli su display HiDPI 2×. Fix: sostituire con `logicalDpiX() / 96.0 * N` o usare `em` via QFontMetrics.
- [ ] **[C++] `ai_client.cpp:1063` — lambda con `reply` raw nel connect** — `connect(reply, &QNetworkReply::finished, this, [reply, callback] {...})` cattura `reply` senza `QPointer`. Se il `QNetworkAccessManager` distrugge `reply` prima che la lambda venga eseguita → crash. Fix: `QPointer<QNetworkReply> safeReply(reply)` + check `if (!safeReply) return;`.
- [ ] **[Python] MCP senza `asyncio.timeout()`** — le chiamate HTTP nei MCP non hanno timeout esplicito. Una risposta lenta blocca indefinitamente il thread MCP. Fix: `async with asyncio.timeout(30):` intorno alle chiamate rete.

#### 🟡 PIANIFICABILE
- [ ] **[SEC] Token LAN in QSettings in chiaro** — il token Bearer è salvato in `~/.config/Prismalux/GUI.conf` in plaintext. Su un sistema multiutente è leggibile. Fix: `QKeychain` (Linux: libsecret, macOS: Keychain, Windows: DPAPI).
- [ ] **[UI] Dark/Light automatico da OS** — 23 temi ma nessuno segue `QStyleHints::colorScheme()` (Qt 6.5+). Aggiungere un tema "Sistema" che rileva automaticamente dark/light. File: `theme_manager.cpp`.
- [ ] **[UI] Focus trap nei dialog** — dialog `QDialog` aprono ma il focus non parte dal primo campo interattivo. Aggiungere `firstWidget->setFocus()` in `showEvent()`.
- [ ] **[C++] `monitor_panel.cpp:68,78` — lambda senza context object** — `connect(clearBtn, &QPushButton::clicked, this, [this]{...})` — qui `this` c'è ma come lambda, non come slot. Convertire a slot nominato `onClearClicked()` per rispettare la regola no-lambda.
- [ ] **[Python] Logging strutturato MCP** — tutti i MCP usano `print()`. Sostituire con `logging.getLogger(__name__)` + `PRISMALUX_LOG_LEVEL` da env. Essenziale per produzione.

#### 🟢 TECH DEBT
- [ ] **[Python] asyncio.to_thread per I/O sync in knowledge_mcp** — `_write_raw()` e `_read_raw()` sono sync con `fcntl.flock` bloccante dentro `async def`. Un file lento blocca l'event loop. Fix: `await asyncio.to_thread(_write_raw, content)`.
- [ ] **[C++] `lan_server.cpp:225,234` — lambda negli ssl connect** — due lambda in `connect(sslSock, &QSslSocket::encrypted, ...)` e `connect(sslSock, &QSslSocket::disconnected, ...)`. Convertire a slot `onEncrypted()` / `onDisconnected()`.
- [ ] **[Python] Type checking assente** — nessun `mypy` / `pyright` sui file MCP. Aggiungere `pyproject.toml` con `[tool.mypy]` e run in CI.

---

## Audit esperto IV — 2026-05-15 (sicurezza · UI · Python · C++)

### 🔴 CRITICO

- [~] **[SEC] exec() MCP — validazione input** (parziale 2026-05-15 sessione V) — `blender_addon` e `office_bridge` hanno ora `_validate_code()` che blocca `import os/subprocess/ctypes`, `os.system`, `eval(`, `exec(` prima di eseguire. `freecad_mcp`/`kicad_mcp` inviano il codice all'app via HTTP (exec avviene dentro FreeCAD/KiCAD, fuori dal nostro processo). Sandbox subprocess completa rimane aperta per blender_addon e office_bridge.
- [x] **[SEC] bioconda_mcp — shell injection condizionale** ✅ — 2026-05-15: `_run()` ora usa sempre `shell=False`; se `cmd` è una stringa viene convertita con `shlex.split()`; aggiunto `import shlex`.

### 🟠 IMPORTANTE

- [ ] **[C++] 462 lambda connect() senza contesto esplicito** — grep trova ~462 `connect()` con lambda che catturano `this` senza passare `this` come 3° argomento (context object). Se il sender sopravvive al receiver, la lambda invoca metodi su oggetto già distrutto. Fix per ogni occorrenza: aggiungere `this` come terzo parametro o usare `QPointer<T>` nella capture.
- [x] **[C++] heap primitives con `delete` manuale in lambda** ✅ — 2026-05-15 sessione V: 3 pattern migrati a `std::shared_ptr` in `impostazioni_page_ai.cpp`: download chain (`idx`,`errN`,`dlNext`), indexing chain (`errCount`,`indexNext`), connection handles (`conn`,`connErr`). Aggiunto `#include <memory>`.
- [x] **[C++] `delete m_tokenHolder` bypassa Qt parent ownership** ✅ — 2026-05-15 sessione V: `programmazione_page.cpp:946` — `delete m_tokenHolder` → `m_tokenHolder->deleteLater()`. Qt svuota la coda eventi prima di distruggere, evitando double-free su segnali pending.
- [x] **[C++] QSettings diretti in `impostazioni_page_ai.cpp`** ✅ — 2026-05-15: 24 istanze `QSettings("Prismalux","GUI")` migrate ad `AppConfig::s()`; 28 call-site totali dopo (alcuni blocchi usano `auto& cfg = AppConfig::s()` multi-uso). 0 istanze rimaste.
- [x] **[UI] Tab order sistematico su pagine chiave** ✅ — 2026-05-15 sessione V: `setTabOrder` aggiunto a `impara_page.cpp` (buildTutor + buildQuiz), `programmazione_page.cpp` (editor→AI→insert chain), `strumenti_page.cpp` (rag→run chain). Scope completo su tutte le ~15k righe rimane aperto.
- [ ] **[UI] Stati di errore muti** — `fetchModels()`, fetch RAG, pipeline fallita: nessun messaggio contestuale. Il bottone torna allo stato idle senza spiegare l'errore. Fix: pattern `AiErrorWidget::showError(msg, retry)` dove mancante.

### 🟡 PIANIFICABILE

- [x] **[SEC] TLS — conta sessioni solo dopo handshake** ✅ — 2026-05-15 sessione V: `m_pendingTls` counter in `lan_server.h`; `onNewConnection()` incrementa alla connessione TCP e aggiunge a `m_sessions` solo su `QSslSocket::encrypted()`; check DoS usa `m_sessions.size() + m_pendingTls`.
- [x] **[SEC] HSTS mancante con TLS attivo** ✅ — 2026-05-15: `httpOkHeader()` e `httpStreamHeader()` cambiate da `static` a `const` member function; aggiunto `Strict-Transport-Security: max-age=31536000` condizionato su `m_tlsEnabled`. `[[nodiscard]]` aggiunto alle stesse.
- [ ] **[Python] Type checking assente** — nessun `mypy` / `pyright` sui file MCP. Aggiungere `pyproject.toml` con `[tool.mypy]` e run in CI.
- [ ] **[Python] requirements.lock con hash** — `pip-compile --generate-hashes` genera `requirements.lock` auditabile. Senza hash SHA256, `pip install --upgrade` può installare versione compromessa (supply chain).
- [ ] **[Python] Logging strutturato** — i MCP usano `print()`. Sostituire con `logging.getLogger(__name__)` con livelli configurabili da variabile d'ambiente `PRISMALUX_LOG_LEVEL`.
- [x] **[C++] `[[nodiscard]]` su funzioni critiche** ✅ — 2026-05-15: `[[nodiscard]]` aggiunto a `timingSafeEqual`, `_ensureCert`, `checkChatRateLimit`, `httpOkHeader`, `httpStreamHeader` in `lan_server.h`.
- [ ] **[UI] Dark/Light auto da sistema** — 23 temi ma nessuno segue `QStyleHints::colorScheme()` (Qt 6.5+). Aggiungere un tema "Sistema" che applica automaticamente dark/light in base all'OS.

### 🟢 TECH DEBT

- [ ] **[Python] asyncio.to_thread per I/O sync in knowledge_mcp** — `_write_raw()` e `_read_raw()` sono sync con `fcntl.flock` bloccante all'interno di un `async def`. Un file lento blocca l'event loop. Fix: `await asyncio.to_thread(_write_raw, content)`.
- [x] **[C++] QSettings diretti in `impostazioni_page.cpp`** ✅ — 2026-05-15 sessione V: `saveStyle`/`loadStyle` migrati ad `AppConfig::s()` con chiavi prefissate `"ChartStyle/bgColor"` etc. (il `/` è il separatore di gruppo nativo di QSettings). Aggiunto `#include "../app_config.h"`.
- [x] **[C++] QSettings diretto in `agenti_page_tools.cpp:301`** ✅ — 2026-05-15: sostituito con `AppConfig::s()`; aggiunto `#include "../app_config.h"`.

---

## Fix 2026-05-15 (sessione IV)
- [x] **Ringraziamenti aggiornati** ✅ — badge "12 MCP" corretto a "17 MCP" (conteggio reale kMCPs[]); versione v2.8→v2.9; aggiunto link "MIT" alla licenza GitHub accanto a Bug/Wiki/Release

## Fix 2026-05-15 (sessione III)
- [x] **llama.cpp Studio reintegrato** ✅ — tab "🦙 llama.cpp Studio" rimontato in Impostazioni → AI Locale (dopo Fine-tuning); aggiunta card "Compila/Aggiorna" nel sotto-menu (prima era raggiungibile solo dal banner al primo avvio)

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
- [x] **[SEC] LAN server TLS (HTTPS)** ✅ — 2026-05-15: `QSslServer` con cert self-signed generato via `openssl req -x509 -rsa:2048` in `~/.prismalux/`; fallback HTTP se openssl/ssl non disponibile; badge "🔒 HTTPS / 🔓 HTTP" in LanWanPage; URL QR e Chat Web usano `serverScheme()` dinamico; JS usa URL relativi → nessun mixed content
- [x] **[SEC] `/apk` endpoint pubblico senza auth** ✅ — aggiunto `/apk` all'insieme `isApi` in `lan_server.cpp`; ora richiede Bearer token come le API

### 🟠 IMPORTANTE

- [x] **[SEC] Token Bearer in QSettings plain text** ✅ — 2026-05-15 sessione III: `loadLanToken()`/`saveLanToken()` in `lan_wan_page.cpp`; file dedicato `~/.prismalux/lan_token.key` con permessi 0600 (`QFileDevice::ReadOwner|WriteOwner`); migrazione automatica da QSettings al primo avvio; `P::lanTokenPath()` aggiunto in `prismalux_paths.h`
- [x] **[SEC] DoS: nessun limite connessioni simultanee LAN server** ✅ — 2026-05-15 sessione III: `kMaxSessions = 32` in `lan_server.h`; check in `onNewConnection()` → risponde 503 + `deleteLater()` se già 32 sessioni aperte
- [x] **[SEC] Security headers HTTP assenti** ✅ — 2026-05-15 sessione III: `X-Content-Type-Options: nosniff`, `X-Frame-Options: DENY`, `Referrer-Policy: no-referrer` aggiunti a `httpOkHeader()` e `httpStreamHeader()`; `nosniff`+`DENY` anche in `sendError()`; aggiunto caso 429 mancante in `sendError()`
- [x] **[SEC] `m_llamaBin` non validato prima di `QProcess::start`** ✅ — `ai_client.cpp`: regex `[;&|` + "`" + `$<>\\]` + `QFileInfo::isExecutable()` prima di start; emit error se path invalido
- [x] **[SEC] Nessun rate limiting su `/api/chat`** ✅ — 2026-05-15: `handleChat()` + `handleGenerate()` hanno ora `m_chatRateCount` (max 30 req/min per IP) con reset ogni 60s; stesso timer condiviso
- [x] **[UX] Nessun undo/redo esplicito nell'editor** ✅ — 2026-05-15: `setPlainText(code)` → `QTextCursor::select(Document)+insertText(code)` in 3 punti (Inserisci AI, Correggi AI, Agentica); le azioni entrano nello stack undo nativo di QPlainTextEdit; placeholder aggiornato
- [x] **[UX] Nessuna conferma su azioni distruttive** ✅ — `QMessageBox::question` aggiunto ai 3 pulsanti "Inserisci nell'editor" (AI panel, Agentica, Reverse Eng.) quando l'editor ha già del codice
- [x] **[C++] Lambda `[this]` senza `QPointer` su reply async** ✅ — 2026-05-15: `ai_client.cpp::fetchEmbedding` usa `QPointer<QNetworkReply>` nella lambda; guard `if (!reply) return`
- [~] **[C++] `QTimer::singleShot` con raw `this` in ~8 file** — tutti gli usi passano `this` come context (secondo param): Qt disconnette automaticamente la lambda alla distruzione → già sicuri

### 🟡 PIANIFICABILE

- [x] **[SEC] Timing attack token comparison** ✅ — 2026-05-15: `LanServer::timingSafeEqual()` (loop XOR constant-time, volatile); sostituisce `!=` nel check Bearer
- [x] **[SEC] Nessun log accesso persistente** ✅ — 2026-05-15: `LanServer::appendAccessLog()` scrive su `~/.prismalux/access.log` (IP + METHOD + path + timestamp ISO) ad ogni request in `processSession()`
- [~] **[UX] Accessibilità WCAG** (parziale 2026-05-15 sessione IV) — aggiunti `setAccessibleName`+`setAccessibleDescription` ai widget più critici: campo chat input, Avvia/Ferma, Voce, Documenti, Immagini, Simboli, Traduci, RAG, selettore modello AI, toggle Chat/Autonomo (`agenti_page_ui.cpp`); tab bar principale, pulsanti ⚙️ e 📋 header, backend button (`mainwindow.cpp`); radio Ollama/llama.cpp, pulsanti Aggiorna/Usa modello (`impostazioni_page_ai.cpp`); `setTabOrder` nella griglia input chat. Scope sistematico su tutte le ~15k righe UI rimane aperto.
- [ ] **[UX] i18n assente** — 30 `tr()` su ~15.000 righe di UI; tutto hardcoded in italiano; introdurre `tr()` sistematico e `.ts` file per future traduzioni
- [x] **[UX] Scrollbar non tematizzate** ✅ — 2026-05-15: aggiunto `QScrollBar::handle:horizontal` con colore neutro semi-trasparente in `base.qss`; si applica a tutti i 23 temi (nessuno ridefiniva il selettore orizzontale)
- [x] **[UX] Font size hardcoded** ✅ — 2026-05-15: `monoFontPt(fallback)` in `programmazione_page.cpp` usa `QApplication::font().pointSize()` come base DPI-aware; stessa logica in `_translitter.cpp` e `_git_repl.cpp`
- [x] **[C++] `QSettings` singleton AppConfig** ✅ — 2026-05-15: `app_config.h` header-only Meyers singleton; migrati 6 file (agenti_page_bubbles/ui/pipeline, lan_wan_page, impostazioni_page_ai, stt_whisper.h) → 0 istanze `QSettings("Prismalux","GUI")` nel codice
- [x] **[C++] `clang-tidy` al CMakeLists** ✅ — 2026-05-15: opzione `ENABLE_CLANG_TIDY=ON`, `find_program` auto (clang-tidy-20..17), `CXX_CLANG_TIDY` property; `.clang-tidy` committato (bugprone + modernize + performance + clang-analyzer)
- [x] **[C++] `Q_DISABLE_COPY` su singleton** ✅ — 2026-05-15: aggiunto `Q_DISABLE_COPY(AiClient)` e `Q_DISABLE_COPY(ThemeManager)` nelle rispettive classi

### 🟢 TECH DEBT

- [x] **[UX] Spinner fetch modelli** ✅ — 2026-05-15: `refreshBtn` in `impara_page.cpp` e `codeModelRefresh` in `strumenti_page.cpp` si disabilitano con ⏳ durante `fetchModels()`, ripristinati 🔄 al segnale `modelsReady`
- [x] **[UX] Drag-and-drop file mancante** ✅ — 2026-05-15: `EditorFileDropFilter` (event filter QObject) installato su `m_editor`; drag di qualsiasi file .txt/.py/.cpp/ecc → contenuto inserito a cursore via `QTextCursor::insertText` (undoable); placeholder aggiornato
- [x] **[Python] requirements.txt senza pin precisi** ✅ — 2026-05-15: gns3_mcp `requests==2.33.1,gns3fy<1.0`; stable_diffusion `diffusers<1.0, transformers<5.0, Pillow==12.1.1`
- [x] **[Python] MCP non installabili come package** ✅ — 2026-05-15: aggiunto `pyproject.toml` a gns3_mcp, stable_diffusion_local, knowledge_mcp (setuptools backend, requires-python>=3.10)
- [x] **[Python] Knowledge MCP scrittura non atomica** ✅ — 2026-05-15 sessione III: `_write_raw()` usa `tempfile.mkstemp` + `os.fsync` + `os.replace()` (rename atomica POSIX); cleanup temp in caso di eccezione; il file non può restare corrotto se il processo crasha a metà scrittura
