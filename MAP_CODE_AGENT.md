# MAP_CODE_AGENT.md

> **Mappa completa del codice per agenti.**
> Generata il **2026-10-20** sullo stato del working tree (include le modifiche non committate).
> Scopo: permettere a un agente di orientarsi, trovare il punto giusto e capire i flussi
> senza rileggere 383 file e ~150.000 righe.
>
> Documento **vivo**: aggiornarlo quando cambiano moduli, flussi o file di stato.
> Le righe indicate sono approssimative e possono scorrere dopo un refactor.

---

## 0. Come usare questo documento

1. Leggi §1–§4 per capire dove sei.
2. Usa §5–§12 come indice: ogni file ha scopo, classi e funzioni chiave.
3. Prima di modificare un flusso, leggi la sezione "Trappole" del modulo (§16) e i flussi numerati (§13).
4. Per la persistenza usa §14: elenca ogni chiave QSettings e ogni file su disco.
5. Per compilare/testare usa §3.

Legenda: `P::` = namespace `PrismaluxPaths`; `SK::` = chiavi QSettings; `D-xx` = voce in `TODO.md`.

---

## 1. Architettura in 30 secondi

```
main.cpp
  └─ MainWindow (QMainWindow)
       ├─ header (gauge HW, backend, log, impostazioni)
       ├─ sidebar (chat history)
       ├─ QTabWidget con 8 tab lazy
       │    ├─ 0 AgentiPage        (AI, eager)
       │    ├─ 1 StrumentiPage     + RicercaPage
       │    ├─ 2 MultimediaPage
       │    ├─ 3 ProgrammazionePage
       │    ├─ 4 MatematicaPage    + GraficoPage
       │    ├─ 5 UtilityPage       (+ LanWanPage, Multi-Agente, Quiz, ...)
       │    ├─ 6 BioinformaticaPage
       │    └─ 7 AppControllerPage (TeleComanda)
       ├─ ImpostazioniPage (QDialog lazy)
       ├─ Dialog log (Sistema / AI / 3D)
       └─ servizi condivisi:
            ├─ AiClient          → HTTP/streaming verso Ollama / llama-server / ds4 / cloud
            ├─ DecisionEngine    → classifica + verifica (separato dagli LLM)
            │    └─ DecisionOnnx → 2 modelli locali (ONNX Runtime)
            ├─ RagEngine         → indice vettoriale JLT (256 dim)
            ├─ GraphMemory       → knowledge graph SQLite
            ├─ HardwareMonitor   → CPU/RAM/GPU/temp
            ├─ ChatHistory       → sessioni JSON
            ├─ ThemeManager      → QSS
            └─ LanServer         → HTTP/HTTPS LAN (creato da LanWanPage o --server)
```

Regole d'oro:

- **Un solo `AiClient` condiviso** per la chat visibile. Ogni altra esigenza (riassunti, traduzioni,
  sub-agenti, quiz) crea un `AiClient` separato, altrimenti i segnali `token/finished/error`
  si sovrappongono.
- **Un solo request per `AiClient`** (`busy()`). Le pagine filtrano con `currentReqId()`.
- **`DecisionEngine` non è un LLM**: non compare mai nelle liste LLM; vive nel registro
  `~/.prismalux/decision_models.json`.
- **Le tab sono lazy**: la 0 è eager, le altre vengono costruite al primo clic o da timer.
- **La persistenza è doppia**: QSettings (`Prismalux/GUI`) per la configurazione,
  file in `~/.prismalux/` per stato e cache.

---

## 2. Struttura del repository

| Percorso | Contenuto / scopo |
|---|---|
| `gui/` | Tutto il frontend C++/Qt6 attivo. |
| `gui/main.cpp` | Entry point GUI e modalità headless `--server`. |
| `gui/mainwindow.*` | Shell dell'app, tab, backend, chat history, impostazioni/log. |
| `gui/pages/` | 202 file: tutte le pagine/tab e i loro widget (`main_*`, `settings_*`, `widget_*`). |
| `gui/widgets/` | 49 file: widget riusabili e helper (bolle chat, gauge, mappa, STT/TTS, sicurezza path). |
| `gui/tests/` | 80 sorgenti di test + target CMake. |
| `gui/themes/` | `base.qss` e temi dark/light; copiati accanto all'eseguibile. |
| `gui/i18n/` | `prismalux_it.ts`, `prismalux_en.ts` → `.qm`. |
| `gui/lan_web/` | UI web servita dal server LAN (`index.html`, `webchat.html`). `webchat.html` è un file unico senza build step: **navigazione a due livelli** su 6 sezioni desktop (AI, Strumenti, Media, Programmazione, Matematica, Utilità) con sotto-tab, ultima sotto-tab ricordata per sezione e deep-link `#<tab>`; CSS con token colore (`--bg`…`--inp`) e strutturali (`--radius*`, `--sp-1..5`, `--fs-*`, `--font-ui`/`--font-mono`, `--tab-h`, `--row-h`) in `:root`, due breakpoint responsive (768/420 px), sezioni comprimibili `details.plx-details` con stato in `localStorage`. Deve rispecchiare la GUI desktop: piano e decisioni "solo desktop" in `TODO.md` §LAN & WAN / LW-1…LW-8. I temi restano indipendenti da QSettings (`localStorage['plx-theme']`). |
| `gui/fonts/` | Font opzionali caricati all'avvio. |
| `gui/utils/` | Helper di test (`mock_ollama_server.h`, `python_runner.h`). |
| `gui/tools/` | `gen_decision_onnx.py`: rigenera i due modelli ONNX e i metadati. `eval_decision_onnx.py`: benchmark held-out dei due modelli. |
| `MCPs/` | ~60 plugin MCP Python (JSON-RPC stdio), `_shared/path_safety.py`, `net_safety.py`. |
| `ANDROID/`, `IOS/` | App mobile e progetto iOS. |
| `EXPORT/` | Script AppImage/Windows/APK, note di release. |
| `Frameworks/` | Runtime locali: ffmpeg, OpenCV, ONNX Runtime, whisper.cpp. |
| `Tools/` | Script STT, depth, PLY→OBJ, ArUco, IPA, Docker, toolchain. |
| `Test/` | Test di integrazione Python e `ctest_lotti.sh` (batch termico). |
| `RAG/`, `KNOWLEDGE_USER/`, `TOOL_TIP/` | Documenti RAG, conoscenza utente, prompt/note. |
| `PATH.md`, `README.md`, `COMANDI.md` | Mappe e documentazione esistenti. |
| `TODO.md` | Backlog unico (D-xx). |
| `aggiorna.sh` | Build canonica + sync `.desktop` (`--gui`, `--test`, `--install`). |
| `build_gui/` | Build Release canonica. |
| `build_tests/` | Build dei test (usata in questa sessione). |

---

## 3. Build, test, esecuzione

### 3.1 Comandi canonici

```bash
# GUI Release
cmake -B build_gui gui/ -DCMAKE_BUILD_TYPE=Release
cmake --build build_gui -j$(nproc)
./build_gui/Prismalux_GUI

# Test
cmake -B build_tests gui/ -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build_tests -j$(nproc)
ctest --test-dir build_tests --output-on-failure

# Wrapper di progetto
./aggiorna.sh            # build + .desktop
./aggiorna.sh --gui      # solo build
./aggiorna.sh --test     # build + ctest (esclude live/rete)
./aggiorna.sh --install  # build + icona + desktop entry
```

> Il progetto raccomanda di **non** lanciare l'intera suite con `-j4` su macchine
> termicamente limitate: usare `Test/ctest_lotti.sh`.

### 3.2 Opzioni CMake

| Opzione | Default | Effetto |
|---|---|---|
| `BUILD_TESTS` | OFF | 80 eseguibili `test_*`, `test_shared_objs`, `test_ai_client_objs`. |
| `ENABLE_SANITIZERS` | OFF | ASAN + UBSAN. |
| `ENABLE_CLANG_TIDY` | OFF | clang-tidy sul target GUI. |

### 3.3 Dipendenze opzionali e define

| Dipendenza | Define | Uso |
|---|---|---|
| Qt6 Core/Widgets/Network/PrintSupport/Concurrent | — | Base (fallback Qt5 5.15). |
| Qt6 Sql | `HAVE_QT_SQL` | Quiz CCNA, cache decisionale, export RAG, GraphMemory. |
| OpenCV / ArUco | `VISION3D_USE_OPENCV`, `VISION3D_USE_ARUCO` | Vision3D. |
| Qt6 Multimedia | `HAVE_QT_MULTIMEDIA` | Voce/audio. |
| Qt6 WebEngineWidgets | `HAVE_WEBENGINE_WIDGETS` | KaTeX/`LatexView`, job assistant. |
| Qt6 WebChannel | `HAVE_WEBSOCKET`, `HAVE_JOB_ASSISTANT` | Web UI. |
| Qt6 Svg | `HAVE_SVG` | OpenMoji. |
| Qt6Keychain | `HAVE_QKEYCHAIN` | Segreti (fallback file 0600). |
| ONNX Runtime | `HAVE_ONNX` | `OnnxEmbedder` locale + backend decisionale `DecisionOnnx`. |
| LinguistTools | — | `.qm`. |

Altre define: `PRISMALUX_ROOT="<source>/.."` (path di sviluppo, verificato a runtime).

### 3.4 Target

- `Prismalux_GUI` — eseguibile principale (`CPP_SRCS` + `C_SRCS`).
- `test_shared_objs` — OBJECT library con quasi tutte le sorgenti (per i test pesanti).
- `test_ai_client_objs` — OBJECT library leggera: `ai_client.cpp`, `decision_engine.cpp`,
  `decision_onnx.cpp`, `rag_engine.cpp`, `widgets/onnx_embedder.cpp`.
- 80 target `test_*` (elenco in §15).

Post-build: copia `themes/` accanto al binario; su Windows `windeployqt`, su macOS `macdeployqt`.

### 3.5 Docker e distribuzione

| File | Scopo |
|---|---|
| `Tools/docker/Dockerfile` | Runtime GUI multi-stage: Ubuntu 24.04 + Qt 6.4, utente `prismalux`, temi e Quiz CCNA inclusi, volume `/home/prismalux/.prismalux`. |
| `Tools/docker/Dockerfile.appimage` | Builder AppImage riproducibile: Ubuntu 24.04 + Qt 6.4, usa `EXPORT/linux/crea_appimage.sh` e produce `/out/Prismalux-x86_64.AppImage`. |
| `Tools/docker/docker-compose.yml` | Compose per GUI: context `../..`, X11/Wayland, `network_mode: host` per Ollama, volume dati. |
| `Tools/docker/avvia_docker.sh` | Avvio GUI con mount X11/Wayland, GPU opzionale e volume persistente. |
| `Tools/docker/build_appimage_docker.sh` | Build del builder e estrazione di AppImage + checksum in `EXPORT/linux/`. |

```bash
# GUI in Docker
docker compose -f Tools/docker/docker-compose.yml build
bash Tools/docker/avvia_docker.sh

# AppImage riproducibile
bash Tools/docker/build_appimage_docker.sh
```

Nota AppImage: le librerie NSS/NSPR non vengono incluse, perché Qt WebEngine deve usare i moduli NSS del sistema; OpenSSL (`libssl`/`libcrypto`) viene invece incluso per il backend TLS di Qt.

---

## 4. Avvio

### 4.1 `gui/main.cpp` (368 righe)

| Simbolo | Cosa fa |
|---|---|
| `instanceSocketName()` | Nome socket `QLocalServer` per utente. |
| `SingleInstanceGuard` | Attiva la finestra esistente se parte una seconda istanza. |
| `buildSplashPixmap()` | Disegna lo splash. |
| `fixPrismaluxPermissions()` | `~/.prismalux` 0640, chat 0700/0600. |
| `loadBundledFonts()` | Carica `fonts/*.ttf|otf`. |
| `setupEmojiFallback()` | Noto/Segoe emoji. |
| `runHeadlessServer()` | Modalità `--server`: `QCoreApplication` + `AiClient` + `LanServer`. |
| `main()` | Avvio GUI. |

Flusso GUI:

1. Scansione `argv` per `--server` **prima** di creare `QApplication`.
2. `QApplication`, `PLog::installMessageHandler()`.
3. `fixPrismaluxPermissions()`.
4. Metadati app (`Prismalux`, `3.0`, `Prismalux`).
5. Single-instance via socket locale.
6. Splash + pump eventi (workaround Wayland).
7. Font, emoji, traduttore, `PRISMALUX_COMPUTE_MODE`.
8. Disabilita dialoghi nativi (`AA_DontUseNativeDialogs`).
9. `MainWindow`, show/raise/activate, splash close, `SingleInstanceGuard`, `app.exec()`.

Flusso headless:

1. `--port`, `--token`; token da CLI → `LanServer::loadLanToken()` → UUID generato e salvato.
2. `AiClient` + `LanServer`, `setHeadless(true)` (niente `/api/launch`), bind `127.0.0.1`.
3. `start(port)`, SIGINT/SIGTERM, event loop.

---

## 5. MainWindow

`MainWindow` è un `QMainWindow` splittato per responsabilità.

| File | Righe | Responsabilità | Metodi chiave |
|---|---:|---|---|
| `gui/mainwindow.h` | 496 | API, enum, membri, slot. | ctor 72; `aiClient()` 76; `ensureTabBuilt()` 122; `applyBackend()` 244; `closeEvent()` 322. |
| `gui/mainwindow.cpp` | 472 | Costruzione, servizi, layout, status bar, timer, backend, distruttore, shortcut. | ctor 83; `setupServices()` 103; `setupLayout()` 168; `setupStatusBar()` 205; `setupAutoOptimizations()` 286; `setupTimers()` 312; `setupBackend()` 379; `~MainWindow()` 432. |
| `gui/mainwindow_header.cpp` | 170 | Header: logo, gauge, pulsanti. | `buildHeader()` 17; `buildGaugesSection()` 107; `buildActionButtons()` 142. |
| `gui/mainwindow_monitor.cpp` | 307 | Snapshot HW, warning termici, navigazione, chiusura. | `onContextUsage()` 33; `onHWUpdated()` 61; `changeEvent()` 194; `closeEvent()` 202; `navigateTo()` 301. |
| `gui/mainwindow_chat.cpp` | 577 | Chat history, migrazione HTML, titoli/riassunti, onboarding. | `migrateLegacyChat()` 69; `onChatCompleted()` 209; `requestSessionSummary()` 246; `refreshChatList()` 337; `showOnboardingWizard()` 404. |
| `gui/mainwindow_settings.cpp` | 300 | Dialog Impostazioni/Log lazy, routing log. | `applyContentSettings()` 35; `ensureSettingsDialog()` 52; `openSettingsDialog()` 144; `ensureLogDialog()` 155; `appendLog()` 222. |
| `gui/mainwindow_tabs.cpp` | 584 | Sidebar, delegate, tab lazy, nav menu. | `ChatListDelegate` 55; `buildSidebar()` 153; `buildContent()` 254; `buildAiTab()` 371; `ensureTabBuilt()` 396; factories 429–535. |
| `gui/mainwindow_backend.cpp` | 1014 | Dialog llama-server/ds4, split CPU/GPU (D-54), ciclo processo, health polling, switch backend, download math. | `showServerDialog()` 53; `showDeviceSplitDialog()` 144; `showDwarfStarDialog()` 369; `startDs4Server()` 439; `applyBackend()` 643; `startLlamaServer()` 809; `stopLlamaServer()` 979. |
| `gui/mainwindow_slots.cpp` | 1357 | Maggior parte degli slot: zRAM, whisper, idle unload, modelli, header, RAM, health, CRUD chat, log, tab, zoom, update, TTFT, ricerca tab. | `onZramSetupTimer()` 53; `onIdleUnloadTimer()` 116; `onBackendBtnClicked()` 234; `onHealthTick()` 429; `onMainTabChanged()` 844; `buildSearchIndex()` 1150. |

### 5.1 Ordine di costruzione

```
MainWindow::MainWindow()
  setupServices()          → HardwareMonitor, AiClient, DecisionEngine, OnnxEmbedder, LogBus
  setupLayout()            → header + splitter(sidebar, content)
  setupStatusBar()         → pipeline, download, zoom
  setupAutoOptimizations() → preset RAM, zRAM
  setupTimers()            → idle unload, onboarding, whisper, RAG, pre-build tab, update
  setupBackend()           → Ollama/modello, Smart Router, tema, ricerca, navigateTo(0)
  setupShortcuts()
  restoreWindowState()
  m_hw->start()
```

`setupServices()` crea **una** istanza di `DecisionEngine` e la passa a `AiClient`
(`setDecisionEngine`) e poi a `ImpostazioniPage` (`setDecisionEngine`).

### 5.2 Tab e lazy loading

Solo la tab 0 è eager. Le altre sono placeholder sostituiti da `ensureTabBuilt(idx)`.

| idx | Tab | Widget | Pre-build |
|---:|---|---|---|
| 0 | 🤖 Intelligenza artificiale | `AgentiPage` | eager |
| 1 | 🛠️ Strumenti (+ Ricerca) | `StrumentiPage`, `RicercaPage` | 400 ms |
| 2 | 🎬 Media | `MultimediaPage` | 2500 ms |
| 3 | 💻 Programmazione | `ProgrammazionePage` | 700 ms |
| 4 | 📐 Matematica (+ Grafico) | `MatematicaPage`, `GraficoPage` | 3700 ms |
| 5 | 🔧 Utilità | `UtilityPage` (+ `LanWanPage`) | 8000 ms |
| 6 | 🧬 Bioinformatica | `BioinformaticaPage` | 4900 ms |
| 7 | 🕹️ TeleComanda | `AppControllerPage` | 5500 ms |

Invarianti:
- `m_tabBuilt[idx]` impedisce doppie costruzioni.
- Il placeholder viene rimosso e il widget inserito **allo stesso indice**.
- Se la build arriva da un timer, la tab corrente viene ripristinata.
- `onMainTabChanged()` chiede conferma se si lascia Programmazione con lavoro non salvato.
- La ricerca tab (`buildSearchIndex`, `onTabSearchTextChanged`) indicizza tab e sotto-tab.

### 5.3 Rischi noti

- I commenti in `mainwindow.h` parlano di 9 tab, ma `buildContent()` ne crea 8.
- `navigateTo()` rifiuta `idx > 6`, mentre `onShortcutAlt7()` usa `8`: shortcut incoerente.
- `OracoloPage` è compilata ma non istanziata (codice legacy/orfano).

### 5.4 Split CPU/GPU (D-54)

Il modulo è `gui/device_split.h/.cpp` (logica pura, nessun hardware):

- `compute(devices, maxLayers=99)` considera solo GPU abilitate con budget ≥ 2048 MB;
  layer = `budgetMb / 80` (cap 99), `nGpuLayers` = somma (cap 99), e con ≥ 2 GPU
  produce `--tensor-split` normalizzato a 1.0 (ultima quota calcolata per far
  sommare esattamente 1.0). Warning per budget insufficiente, cap raggiunto e
  dispositivi eterogenei.
- `save/load/savedBudget/clear` persistono in QSettings: `llama/splitEnabled`,
  `llama/splitNgl`, `llama/splitTensor`, `llama/splitSummary`,
  `llama/splitWarning`, `llama/splitBudgets` (JSON nome→MB).
- `MainWindow::showDeviceSplitDialog()` (in `mainwindow_backend.cpp`) mostra una
  riga per GPU con checkbox, budget e layer calcolati; **Reset** ripristina l'80%
  della VRAM libera; **Dispositivi llama.cpp** esegue `llama-server --list-devices`
  per verificare l'ordine reale (quello di `--tensor-split`).
- `startLlamaServer()` carica il piano: se attivo e `nGpuLayers > 0` sostituisce
  `-ngl` e aggiunge `--tensor-split <quote>` + `--split-mode layer`. Senza piano
  resta la scelta automatica della GPU con più VRAM.
- Limite: `--tensor-split` segue l'ordine di llama.cpp, non di HWInfo; mischiare
  NVIDIA + iGPU Intel/AMD richiede una build con più backend. I layer non
  offloadati restano in CPU.

---

## 6. `gui/widgets/` — widget e helper

### 6.1 Widget UI

| File | Classe | Metodi chiave | Scopo |
|---|---|---|---|
| `ai_error_widget.h` | `AiErrorWidget` | `showError()`, `retryRequested()` | Card errore con retry/apri impostazioni. |
| `chart_widget.*` | `ChartWidget` | `addSeries()`, `setRange()`, `paintEvent()` | Grafico multi-serie leggero. |
| `chat_bubble.*` | `ChatBubble` | `appendToken()`, `finalizeStream()`, `plainText()`, `embedChart()` | Bolla streaming con azioni. |
| `chat_log_browser.h` | `ChatLogBrowser` | `paintEvent()` | Angoli arrotondati reali (Qt rich text non li supporta). |
| `code_highlighter.h` | `CodeHighlighter` | `setLanguage()`, `highlightBlock()` | Syntax highlight. |
| `collapsible_section.h` | `CollapsibleSection` | `fromGroupBox()`, `setOpen()` | Card collassabile persistita. |
| `job_recorder_bridge.h` | `JobRecorderBridge` | `elementClicked()` | Bridge JS→C++ recorder. |
| `latex_view.h` | `LatexView` | `setLatexHtml()` | KaTeX (WebEngine) o fallback. |
| `model_combo_box.h` | `ModelComboBox` | `selectedModel()`, `refresh()` | Selettore modelli con badge. |
| `model_combo_helper.h` | `ModelComboHelper` | `populate()`, `connectFetch()` | Popolamento combo esistente. |
| `natal_chart_widget.*` | `NatalChartWidget` | `setData()`, `computeAspects()` | Carta natale. |
| `qr_code_widget.*` | `QrCodeWidget` | `setText()`, `renderImage()` | QR. |
| `resource_gauge.*` | `ResourceGauge` | `updateWithText()`, `setLevel()` | Gauge CPU/RAM/GPU/VRAM/CTX. |
| `spinner_widget.h` | `SpinnerWidget` | `start()`, `stop()` | Spinner. |
| `status_badge.h` | `StatusBadge` | `setStatus()` | Badge di stato. |
| `toggle_switch.h` | `ToggleSwitch` | `paintEvent()` | Switch animato. |
| `tri_mode_button.h` | `TriModeButton` | `setMode()`, `zoneAt()` | Selettore Chat/Agentico/Conversa. |
| `widget_dep_check.h` | `DepCheckPanel` | `runAllChecks()`, `onInstallAllClicked()` | Verifica/installa dipendenze. |
| `widget_docker_update.h` | `DockerUpdatePanel` | `onUpdateAllClicked()` | Update container. |
| `widget_pip_installer.h` | `PipInstaller` | `setPackages()`, `onInstall()` | Card pip. |
| `widget_python_update.h` | `PythonUpdatePanel` | `onCheckClicked()`, `onUpdateAllClicked()` | Update pacchetti Python. |
| `world_map_widget.*` | `WorldMapWidget` | `setCoords()`, `downloadVisibleTiles()` | Mappa OSM con cache. |
| `zoomable_image_view.h` | `ZoomableImageView` | `setPixmap()`, `setSvgFile()` | Pan/zoom immagine. |
| `formula_parser.*` | `FormulaParser` | `eval()`, `tryExtract()` | Parser matematico (alias italiani). |
| `astro_calc.*` | `AstroCalc` | `compute()` | Effemeridi. |
| `vsop87b_data.h` | tabelle | — | Dati VSOP87B (coesivi). |

### 6.2 Infrastruttura

| File | Simbolo | Scopo |
|---|---|---|
| `external_ai_import.h` | `ExternalAiImport` | Import conversazioni ChatGPT/Claude → `ChatHistory`/RAG. |
| `ffmpeg_utils.h` | `FfmpegUtils` | Trova ffmpeg, costruisce argomenti Whisper. |
| `opencv_utils.h` | `OpencvUtils` | Rileva OpenCV/Python/OCR. |
| `path_guard.h` | `PathGuard` | Blocca path sensibili per i tool file. |
| `proc_helper.h` | `ProcHelper` | `QProcess` sincrono con timeout. |
| `python_exec.h` | `PythonExec` | Esegue script Python temporaneo. |
| `stt_daemon.h` | `SttDaemon` | Demone STT persistente. |
| `stt_whisper.h` | `SttWhisper` | faster-whisper / whisper-cli, diarizzazione. |
| `tts_speak.h` | `TtsSpeak` | Piper → espeak-ng → spd-say. |
| `whisper_autosetup.h` | `WhisperAutoSetup` | Build whisper.cpp + modello. |
| `thermal_guard.h` | `ThermalGuard` | Isteresi 82/75 °C. |
| `log_utils.h` | `PLog` | Log `[CAT][LVL]`, message handler. |

---

## 7. `gui/ai_client.*` — client LLM unico

`AiClient` è l'unico punto di accesso ai backend.

### 7.1 Backend

`enum Backend { Ollama, LlamaServer, LlamaLocal, Ds4Server }`

| Backend | Endpoint / processo |
|---|---|
| Ollama | `POST /api/chat`, `/api/generate`, `/api/tags`, `/api/show`, `/api/embeddings` |
| LlamaServer | OpenAI-compatibile: `/v1/chat/completions`, `/v1/models`, `/v1/embeddings` |
| LlamaLocal | `QProcess llama-cli` con prompt ChatML |
| Ds4Server | OpenAI-compatibile (DwarfStar/ds4) |

### 7.2 Tipi e parametri

`AiChatParams` — unica fonte `~/.prismalux/ai_params.json`:

| Campo | Default |
|---|---|
| `temperature` | 0.1 |
| `top_p` | 0.85 |
| `top_k` | 20 |
| `repeat_penalty` | 1.20 |
| `num_predict` | 2048 |
| `num_ctx` | 8192 |
| `honesty_prefix` | true |
| `caveman_mode` | false |
| `thinkMode` | 0 auto / 1 off / 2 on |
| `thinkBudget` | 1–4 |
| `flash_attn` | true |

`AiChatParams::load()` crea il file con i default se manca; `save()` scrive JSON indentato.

### 7.3 Segnali

`token(QString)`, `finished(QString)`, `aborted()`, `error(QString)`,
`modelsReady(QStringList)`, `modelChanged(QString)`,
`embeddingReady(QVector<float>)`, `embeddingError(QString)`,
`transcriptionReady(QString)`, `transcriptionError(QString)`,
`requestStarted(model, backend)`, `toolCallRequired(name, QJsonObject)`.
`routedToCloud(bool)`, `decisionClassified(intent, confidence, cached)`,
`decisionRetry(reason)`, `decisionVerified(approved, reason)`.
`AgentiPage` consuma i tre segnali decisionali per il badge `decisionBadge`
nella toolbar: intento+confidenza durante la generazione, esito dopo la
riverifica. I segnali arrivano solo con Decision Model attivo.

### 7.4 Metodi

| Metodo | Cosa fa |
|---|---|
| `setBackend(b, host, port, model)` | Imposta backend; forza host loopback per backend locali; invalida cache modelli. |
| `setLocalBackend(bin, gguf)` | Backend `LlamaLocal`. |
| `classifyQuery(text)` | Euristica simple/complex/auto. |
| `detectQueryDomain(text)` | Regex dominio → general/math/physics/chemistry/electronics/unit/coding. |
| `detectQueryIsEnglish(text)` | Conta parole funzionali inglesi. |
| `dateTimeDirective(text)` | Inietta data/ora se la domanda è temporale. |
| `scrubPii(text)` | Maschera IBAN/CF/email/telefono (solo cloud). |
| `abort()` | Azzera reply **prima** di abort, pulisce accumuli e stato decision, emette `aborted`. |
| `fetchModels()` | Cache 30 s → `/api/tags` o `/v1/models` (o scan GGUF). |
| `unloadModel()` | Ollama `keep_alive=0`, solo se caricato. |
| `chat(sys,user)` / `chat(sys,user,history,qt)` | Pipeline completa (vedi §13.1). |
| `generate(sys,prompt,qt)` | Solo Ollama `/api/generate`, stream. |
| `chatWithImage(...)` | Vision (Ollama images / OpenAI content array). |
| `onReadyRead()` | Parser NDJSON/SSE, accumula, emette token (se non buffering). |
| `onFinished()` | Errori, thinking, tool call, verifica, `finished`. |
| `replyWithTool(s)` | Riprende la conversazione con risultati tool. |
| `fetchModelLayers(cb)` | `/api/show` → block_count. |
| `fetchEmbedding(text)` | ONNX → Ollama → OpenAI. |
| `_fetchEmbeddingForRag(...)` | Embedding con callback, senza segnali globali. |
| `transcribeAudio(file,url)` | Multipart `/v1/audio/transcriptions`. |

### 7.5 Composizione del system prompt

Ordine finale (ogni passaggio antepone):

```
kHonestyPrefix
  → _languageDirective(userMsg)
  → kCavemanPrefix (se attivo)
  → dateTimeDirective
  → prependConstitution(sys)
  → system prompt originale
```

`prependKnowledge`/`prependMathKnowledge` sono usati dalla pipeline di `AgentiPage`,
non dentro `AiClient`.

### 7.6 Parametri inviati (Ollama)

- `num_ctx` scende a 4096 se RAM totale < 10 GB.
- `num_predict`: 512 per query semplici senza think; `num_predict * thinkBudget` con thinking.
- `think` attivo se: non disattivato, modello think-capable, non noto rotto,
  e (`thinkOn` o query complessa).
- Override per modello da `~/.prismalux/model_params.json`: **vince su tutto**.
- `keep_alive`: 0 se RAM > 85%, 60 s > 70%, 600 s > 40%, altrimenti 1800 s.
- Cloud: rimuove `options/keep_alive`, imposta modello cloud e `max_tokens`, applica `scrubPii`.
- Timeout trasferimento: 300 s.

---

## 8. `gui/decision_engine.*` — strato di decisione

Non è un LLM conversazionale: classifica e verifica.

### 8.1 Tipi

- `enum DecisionBackend { BackendOllama, BackendDwarfstar, BackendOnnx, BackendLlamaServer }`
- `enum Role { RoleClassifier, RoleVerifier }`
- `struct ModelSpec { name, ref, backend, role, note, sizeBytes; isInstalled() }`
- `struct ClassifyResult { ok, intent, confidence, fromCache, error }`
- `struct VerifyResult { ok, approved, confidence, reason, error }`
- `struct CacheStats { entries, bytes }`

### 8.2 Metodi

| Metodo | Cosa fa |
|---|---|
| `taxonomy()` | A→coding, B→reasoning, C→chat, D→other. Ordine stabile (protocollo). |
| `catalog()` / `saveCatalog()` | Legge/scrive `~/.prismalux/decision_models.json`. |
| `registerModel()` | Aggiunge/aggiorna un modello nel registro (con backend per i non-Ollama). |
| `loadConfig()` / `saveConfig()` | Chiavi `decision/*`, inclusi i modelli per tipologia. |
| `modelForIntentSetting(intent)` | Valore configurato anche se non installato (UI). |
| `modelForIntent(intent)` | Configurato **se installato**, altrimenti predefinito. |
| `setModelForIntent()` | Aggiorna la mappa in memoria. |
| `active()` | `enabled && modello non vuoto && installato`. Per ONNX serve `m_model == "onnx-decision"`. |
| `refreshInstalled()` | GET `/api/tags` (Ollama) o `/v1/models` (DwarfStar/llama-server); **ONNX**: carica i due modelli locali, registra/mostra `onnx-decision`, emette `installedReady`. |
| `onTagsReply()` | `m_installed = catalog() ∩ backend`. |
| `downloadModel()` | `ollama pull`, `./download_model.sh`, registrazione alias per llama-server, **ONNX**: verifica i modelli inclusi (non scarica). |
| `_cacheOpen()` | SQLite `classify_cache(k,intent,confidence,ts)`. |
| `_cacheKey()` | SHA-256(`model + \x1f + text`) primi 32 hex. Per ONNX il modello nella chiave è sempre `onnx-decision`. |
| `_cacheGet/_cachePut()` | Memoria + DB con TTL. |
| `classify()` | Cache → **ONNX locale** oppure `_chatOnce(predefinito, ..., 16)`. |
| `verify()` | `modelForIntent(intent)` → `_chatOnce(..., 8)` oppure **ONNX locale**, verdetto Y/N. |
| `parseLetter()` | Una sola lettera A–D; più lettere = eco del prompt → vuoto (solo backend HTTP). |
| `_chatOnce()` | Ollama `/api/chat`; DwarfStar/llama-server `/v1/chat/completions`, temp 0, think false. ONNX → `nullptr` (gestito prima). |
| `_extractContent()` | `message.content` o `choices[0].message.content`. |
| `_seedCatalog()` | Seed: Ollama, 2 DwarfStar, `onnx-decision`; migrazione one-time per cataloghi esistenti. |
| `_ensureOnnx()` | Crea il runner pigramente; non ritenta dopo un errore finché `refreshInstalled()` non lo ricrea. |
| `onnxError()` | Ultimo errore del runner ONNX (per la UI). |

### 8.3 Garanzia di separazione

`installedModels()` è **sempre** un sottoinsieme di `catalog()`: un modello installato ma
non registrato non può essere scelto; uno registrato non compare nella lista LLM.

### 8.4 Fallback deliberati

- Output classificatore illeggibile → `intent="other"`, confidenza 0 (non cacheato).
- Verdetto illeggibile → **approva** (non si boccia una risposta per colpa del revisore).
- Errore di rete in verifica → `ok=false` → la risposta viene mostrata senza retry.
- Backend ONNX senza runtime o modelli → `classify()` torna errore esplicito e
  `verify()` approva (fail-open): nessun fallback silenzioso su Ollama.

### 8.5 Backend ONNX (`gui/decision_onnx.*`)

Due modelli lineari locali, **non LLM**, inclusi nel repository:

| File | Input | Output | Dimensione |
|---|---|---|---:|
| `Frameworks/onnx/models/decision_classifier.onnx` | feature `[1,512]` | logits `[1,4]` → A/B/C/D | ~8 KB |
| `Frameworks/onnx/models/decision_verifier.onnx` | feature `[1,512]` | logits `[1,2]` → N/Y | ~4 KB |

- **Feature hashing** (`DecisionOnnx::featuresText/featuresPair`): normalizzazione
  lowercase + separatori, token parola, 4-grammi di carattere, bigrammi di parole,
  FNV-1a 32 bit su UTF-8, L2-normalize. Il classificatore usa 512 bucket di testo.
  Il verificatore usa i **bucket 0..63 riservati ai segnali strutturati** (indice
  fisso, niente collisioni con le parole) e i bucket 64..511 (448) per il testo.
  Segnali: `x_ov_*`, `x_len_*`, `x_ratio_*`, `x_refusal`, `x_empty`, tipologia
  domanda (`x_q_num/who/where/when/how/why/explain/open/fact/capital`), tipo
  risposta (`x_a_num/proper/code/proc/causal/vague/short/digit`) e interazioni
  (`x_num_*`, `x_how_*`, `x_why_*`, `x_fact_*`, `x_short_*`, `x_vague_neg`).
- **Guardie deterministiche** (`DecisionOnnx::verify`): dopo il modello, regole
  lessicali decidono i casi ovvi — rifiuto/vuoto → N; risposta breve con cifra o
  nome proprio nuovo → Y; domanda numerica senza cifre → N; capitale senza nome
  proprio nuovo → N; risposta generica → N; how/why/explain con procedura/causa
  o spiegazione lunga → Y. Il campo `Result.reason` spiega la guardia applicata;
  `DecisionEngine` lo mostra nel motivo del rifiuto. **Non è un fact-checker**:
  non sa che «Berlino» non è la capitale della Francia.
- **Parità C++↔Python**: la stessa funzione è in `gui/tools/gen_decision_onnx.py`.
  La fixture `gui/tests/data/decision_onnx_features.json` congela i vettori attesi
  e i casi delle guardie; `test_decision_onnx` confronta tutti i 512 valori e
  verifica label/reason delle guardie. Se cambi una delle due implementazioni,
  rigenera i `.onnx` e la fixture.
- **Rigenerazione**: `python3 gui/tools/gen_decision_onnx.py --check --fixture gui/tests/data/decision_onnx_features.json`
  (scrive i due `.onnx` + `decision_onnx_meta.json`; servono `numpy`, `onnx`,
  `onnxruntime`).
- **Percorsi**: `Frameworks/onnx/models` oppure `PRISMALUX_DECISION_ONNX_DIR`
  (usato dai test per simulare modelli mancanti).
- **Runtime**: compilato solo con `HAVE_ONNX`; senza runtime `load()` fallisce con
  errore esplicito e la UI lo mostra. CMake cerca l'header nei tre layout
  (Debian `/usr/include/onnxruntime/...`, prebuilt `core/session`, include dir
  dedicata) e propaga include/lib anche alle object library dei test.
- **UI**: in LLM & DM → Decision Model il backend ONNX mostra **Verifica** invece
  di **Scarica**; il modello selezionabile è `onnx-decision`. Il salvataggio forza
  questo nome quando il backend è ONNX, così `active()` non resta legato a un
  modello Ollama.
- **Qualità**: `python3 gui/tools/eval_decision_onnx.py` misura 48 prompt
  classificatore + 33 coppie verificatore dev + 35 coppie blind (opzioni `--json`,
  `--min-classifier`, `--min-verifier` per il verificatore effettivo,
  `--min-verifier-raw`, `--min-verifier-blind`). Risultato 2026-10-20:
  classificatore 37/48 = 77.1% (invariato); verificatore dev grezzo 30/33 = 90.9%,
  dev con guardie 33/33 = 100%; blind grezzo 31/35 = 88.6%, blind con guardie
  35/35 = 100%. **Attenzione**: su questi set le guardie coprono tutti i casi, quindi
  il 100% è il comportamento ibrido, non accuratezza appresa; la qualità del modello
  puro è la riga "grezzo". I casi di fatto sbagliato (Berlino/Monaco) sono in
  `OUT_OF_SCOPE` e non contano: il verificatore è relevance/refusal-only. Per il
  classificatore serve ancora un dataset etichettato o un modello più espressivo.
  Diagnostica live (manuale, richiede Ollama): `python3 gui/tools/live_decision_ollama.py`
  (default `tev1:4b`, `think:false`); ultimo esito classify 3/4, verify 6/6.

---

## 9. `gui/rag_engine.*`, `rag_graph.*`, `graph_memory.*`

### 9.1 `RagEngine`

Indice vettoriale con **Johnson–Lindenstrauss**: embedding originale → 256 dimensioni.

| Metodo | Cosa fa |
|---|---|
| `project(fullEmb)` | Proietta con matrice casuale deterministica (seed 20240328). |
| `addChunk(text, emb, source, mtime)` | Proietta e appende. Nessun dedup. |
| `isFileIndexed(source, mtime)` | Vero se esiste un chunk con stesso source/mtime. |
| `removeChunksForFile()` / `removeLegacyChunks()` | Pulizia incrementale. |
| `indexedFileMap()` | Mappa path→mtime. |
| `searchScored(query, k)` | Coseno su tutti i chunk, top-k con score. |
| `search(query, k)` | Wrapper senza score. |
| `save(path)` | JSON; ruota il precedente in `.bak`. |
| `load(path)` | Legge seed/input_dim, rigenera R, carica chunk. |
| `saveDb(path)` | SQLite `rag_meta` + `rag_chunks`, BLOB float32 little-endian. |
| `loadDb(path)` | Valida in un engine temporaneo e sostituisce solo a fine parsing. |
| `chunks()/seed()/inputDim()/chunkCount()` | Accesso e diagnostica. |

Persistenza di lavoro: `~/.prismalux_rag.json`.

### 9.2 `RagGraph`

Estrae entità/relazioni dai documenti via LLM e le salva in `GraphMemory`.

| Metodo | Cosa fa |
|---|---|
| `addDirectory(dir)` | Scansione ricorsiva e accoda file testuali/immagini/video/Office. |
| `addFile(path)` | Copia in `ragDir()` se fuori, con guard anti-traversal. |
| `startIngest()` / `stopIngest()` | Avvia/ferma; **stop chiama `m_ai->abort()`** (client condiviso). |
| `pauseIngest()` / `resumeIngest()` | Pausa dopo il file corrente. |
| `processNextFile()` | Estrae testo (pdftotext/tesseract/ffmpeg/whisper/libreoffice) e chiama `extractEntities`. |
| `extractEntities()` | Prompt JSON-only → `AiClient::chat`. |
| `parseAndStore()` | Crea nodi documento/entità e archi in `GraphMemory`. |
| `searchChunks()` / `searchNodes()` | Ricerca nel grafo. |

### 9.3 `GraphMemory`

Knowledge graph SQLite (`~/.prismalux/graph_memory.db`, `rag_graph.db`, `hermes_memory.db`).

Schema:

```sql
CREATE TABLE gm_nodes (
  id TEXT PRIMARY KEY, type TEXT DEFAULT 'entity', label TEXT NOT NULL,
  content TEXT DEFAULT '', meta TEXT DEFAULT '{}',
  importance REAL DEFAULT 1.0, created_at TEXT NOT NULL
);
CREATE TABLE gm_edges (
  id TEXT PRIMARY KEY, from_id TEXT NOT NULL, to_id TEXT NOT NULL,
  relation TEXT DEFAULT 'relates_to', weight REAL DEFAULT 1.0,
  meta TEXT DEFAULT '{}',
  FOREIGN KEY(from_id) REFERENCES gm_nodes(id) ON DELETE CASCADE,
  FOREIGN KEY(to_id)   REFERENCES gm_nodes(id) ON DELETE CASCADE
);
CREATE INDEX idx_edges_from ON gm_edges(from_id);
CREATE INDEX idx_edges_to   ON gm_edges(to_id);
```

Metodi: `open()`, `addNode()`, `addEdge()`, `neighbours()` (BFS), `searchNodes()`,
`toDot()`, `pruneByImportance()`, export/backup.

---

## 10. Altri moduli AI

| File | Classe / funzione | Scopo |
|---|---|---|
| `model_processor.*` | `ModelProcessor`, `ModelProfile` | Pre/post-processing per profilo regex (forza lingua, rimuove think/markdown). Solo `AgentiPage`. |
| `context_compressor.*` | `ContextCompressor` | Finestra scorrevole + summary asincrono via LLM. Usato da `AgentiPage`. |
| `ai_memory.*` | `AIMemory` | Memoria utente in git `~/.ai-memory/` con revert. |
| `ai_thinking_detect.h` | `_extractThinkingToken()` | Rileva campi thinking per nome semantico, poi stringa più lunga. |
| `ai_utils.h` | `AiUtils` | `populateModelCombo`, emoji interfacce. |
| `hardware_monitor.*` | `HWDetectThread`, `HardwareMonitor` | Rilevamento HW una tantum + polling ~2 s. |
| `monitor_panel.*` | `MonitorPanel` | Diagnostica inferenza: TTFT, durata, token, RAM, score. |
| `theme_manager.*` | `ThemeManager` | QSS, temi esterni, sistema chiaro/scuro, zoom. 26 temi built-in allineati per ID/nome/palette alla webchat LAN (`test_theme_manager::paritaTemiConWebchat`); le preferenze restano separate: QSettings `theme` sul desktop, `localStorage['plx-theme']` nel web. `settings_visual.cpp` mostra 6 card per riga e ricava l'anteprima leggendo `bg`, `header` e `accent` dal QSS reale; `base.qss` è escluso dai temi custom. Performance: `apply()` salta il repolish se l'ID è già attivo (`reapply()` forza), congela tutti i top-level durante `setStyleSheet`; `AgentiPage::recolorLog()` parte solo su cambio dark↔light ed è differito. Redis non è usato per il cambio tema: il QSS è già in RAM e il collo di bottiglia è il repolish Qt di `setStyleSheet()`, non un lookup; Redis aggiungerebbe solo IPC/dipendenza. |
| `log_bus.*` | `LogBus` | Bus log process-wide. |
| `app_config.h` | `AppConfig::s()` | Singleton `QSettings("Prismalux","GUI")`. |
| `chat_history.*` | `ChatHistory` | Sessioni JSON atomiche in `~/.prismalux_chats/`. |
| `https_redirect_server.*` | `HttpsRedirectServer` | TLS o redirect 301. |
| `lan_server*.cpp` | `LanServer` | HTTP/HTTPS 11500: chat, RAG, knowledge, graph, whisper, sync, math, APK, webchat. Build Qt 6.4 (Ubuntu 24.04/CI): `lan_server.cpp` deve includere `<QTimeZone>` perché `QDateTime::timeZone()` restituisce un tipo solo forward-dichiarato. |
| `blhm_engine.h/.c` | BLHM | Motore pesi Brain-Loop-Human. |
| `hw_detect.h/.c` | `HWInfo` | CPU/GPU cross-platform. |
| `qrcodegen.h/.c` | QR | Generatore QR vendorizzato. |

---

## 11. `gui/pages/` — indice

### 11.1 AI (`AgentiPage`)

| File | Righe | Scopo |
|---|---:|---|
| `main_ai.h` | 709 | API, segnali, stato. |
| `main_ai.cpp` | 164 | ctor 57, dtor 109, `prepareClose()` 125. |
| `main_ai_ui.cpp` | 1738 | `setupUI()` 77, toolbar, chat log, input, pannelli. |
| `main_ai_panels.cpp` | 1184 | Tools panel, MCP toggle, format bar. |
| `main_ai_pipeline.cpp` | 2442 | Schemi tool 121–345, `runPipeline()` 531, `advancePipeline()` 860, `runAgent()` 1037, `_finishedPipeline()` 1342, sandbox Docker 2140. |
| `main_ai_stream.cpp` | 644 | `onToken()` 35, `onFinished()` 180, `onError()` 312, badge Decision Model (`onDecisionClassified/Retry/Verified`) in coda. |
| `main_ai_slots.cpp` | 1589 | Send/stop/mode/model/agent, salvataggio chat. |
| `main_ai_tools.cpp` | 2332 | MCP launch 118, `onToolMcpCall()` 192, discovery 395, `runToolCall()` 490, batch nativo ~2220. |
| `main_ai_tools_calls.cpp` | 1361 | Un `onTool<Name>()` per tool. |
| `main_ai_math.cpp` | 2009 | Guardie matematiche locali, data/random/scienza. |
| `main_ai_exec.cpp` | 91 | Estrazione codice eseguibile. |
| `main_ai_files.cpp` | 516 | Drag&drop immagini/PDF/audio/codice. |
| `main_ai_format.cpp` | 887 | `markdownToHtml`, syntax highlight. |
| `main_ai_bubbles.cpp` | 388 | HTML bolle. |
| `main_ai_auto.cpp` | 343 | ReAct autonomo (max 8 step). |
| `main_ai_byzantine.cpp` | 265 | Voto multi-modello. |
| `main_ai_advice.cpp` | 310 | Consiglio scientifico multi-modello + Jaccard. |
| `main_ai_hermes.cpp` | 242 | Memoria Hermes (`hermes_memory.db`). |
| `main_ai_knowledge.cpp` | 370 | `auto_extract_and_update`. |
| `main_ai_feedback.cpp` | 63 | 👍/👎 → `feedback.jsonl`. |
| `main_ai_stt.cpp` | 493 | Mic, VAD, Whisper. |
| `main_ai_tts.cpp` | 146 | Piper/espeak/spd-say. |
| `dialog_agents_config.*` | 1305 | Configurazione agenti. |

Strumenti esposti al modello (`_buildOllamaTools`):
`calc`, `fetch_url`, `ricerca`, `leggi_file`, `lista_file`, `python`, `search_rag`,
`graph_memory`, `get_knowledge`, `spawn_agent`, `algoritmo`, `codice_fiscale`,
`finanza_calcola`, `valida_documento`, `carta_astrale`, `converti`,
`disegna_grafico`, `mcp_call`.

### 11.2 Altre pagine

| Famiglia | File | Classe / scopo |
|---|---|---|
| Multi-agente | `main_multi_agent.*` | `AgentiMultiPage`: pool subtask, DAG, output DOT/JSON. |
| MCP manager | `main_mcp_manager.*` | `McpManagerPage`: discovery `MCPs/`, venv, manifest SHA-256, smoke test. |
| Strumenti | `main_tools*` | `StrumentiPage` + `StrumentiFilePage` (File AI, Wiki, Excel, PDF, Word). |
| Ricerca | `main_research*` | `RicercaPage`: paper, brevetti, RAG tester, graph, astrologia nascosta. |
| Media | `main_multimedia.*` | 10 tab: audio, SD, Graphviz, OSM, synth, voice clone, OCR, video, 3D, IPA. |
| Programmazione | `main_programming*` | 11 tab: coding, git/repl, VPN, driver/kernel, network, reveng, USB, WIBY, agentica, translitter. |
| Matematica | `main_math*` | Sequenze, costanti, Nth, booleana, espressioni, solver. |
| Grafico | `main_graph*` | `GraficoPage` + `GraficoCanvas` (40+ tipi). |
| Utilità | `main_utility.*` | Solar, idro, lavoro, pratico, bike, mod giochi. |
| LAN/WAN | `main_lan_wan*`, `main_wan_*` | `LanWanPage`, server WAN TLS, worker, persistenza task. |
| Bioinformatica | `main_bioinformatica.*` | Wrapper Cytoscape/RDKit/Bioconda/Avogadro/RAB0-L/BLHM. |
| TeleComanda | `main_app_controller*` | Blender, FreeCAD, LibreOffice, CloudCompare, electronics, DevAgent, Telegram, WhatsApp. |
| Scientific compute | `main_sci_compute*`, `main_sci_protein.*` | Coda WU SQLite, nodi, quorum, ESMFold/AlphaFold. |
| Jobs | `main_jobs*` | CV, offerte, lettere, tracker. |
| Learn/Quiz | `main_learn*`, `main_quiz*`, `main_subjects.*` | Tutor, quiz, CCNA DB. |
| Maintenance | `main_maintenance*` | Backend, HW, bug tracker, cron, LAN, update. |
| Security | `main_security.*` | Audit codice multi-agente + OSV. |
| Simulator | `main_simulator*`, `main_sim_*` | Algoritmi passo-passo, sorting, grafi. |
| Customize | `main_customize*` | llama.cpp Studio, compile, download, LoRA. |
| Distillazione | `main_distillazione.*` | Dataset JSONL. |
| OpenCode | `main_opencode.*` | Integrazione OpenCode. |
| Oracle (legacy) | `main_oracle.*` | Chat con RAG, non istanziata. |

### 11.3 Widget di pagina (`pages/widget_*`)

`widget_ai_script_base`, `widget_avogadro`, `widget_bike`, `widget_bioconda`,
`widget_blhm`, `widget_code_interpreter`, `widget_coding_lab`, `widget_cytoscape`,
`widget_formula_builder`, `widget_idro`, `widget_ipa_guide`, `widget_mod_giochi`,
`widget_rab0l`, `widget_ram_calculator`, `widget_rdkit`, `widget_solar_calc`,
`widget_ssh_manager`, `widget_stable_diffusion`, `widget_synthesizer`,
`widget_vision3d`, `widget_voice_cloner`, `widget_webdev`.

---

## 12. Impostazioni (`ImpostazioniPage`)

`gui/pages/settings_main.h/.cpp` è il contenitore; i builder sono splittati.

### 12.1 Tab esterne

| Tab | File | Note |
|---|---|---|
| 🦙 AI Locale | `settings_ai.cpp` | Eager. |
| 🎨 Visuale | `settings_visual.cpp` | Eager. |
| 🤖 LLM & DM | `settings_llm.cpp` | Lazy. Contiene LLM, **Decision Model**, Classifica, Benchmark, Test. |
| 🔧 Sistema | `settings_system.cpp` | Lingua, pulizia, consigli. |
| 🔌 MCP | `settings_other.cpp` | `McpManagerPage`. |
| 🔧 Profili Modello | `settings_model_profiles.*` | Profili regex. |
| 🔒 Sicurezza WAN | `settings_security.cpp` | TLS, WireGuard. |
| 📜 Ringraziamenti | `settings_other.cpp` | Crediti, easter egg. |

### 12.2 LLM & DM → Decision Model

`buildDecisionModelTab()` (`settings_llm.cpp`):

1. **Configurazione**: attivazione opt-in, backend (Ollama/DwarfStar/ONNX/llama-server),
   modello predefinito, host/porta, soglia, riverifica, cache TTL, registro, download.
2. **Modello per tipologia**: quattro combo (coding/reasoning/chat/other).
   La classificazione usa il predefinito; la **verifica** usa il modello scelto per
   la tipologia rilevata. Se non installato → fallback al predefinito.

Chiavi: `decision/model`, `decision/model/{coding,reasoning,chat,other}`.

### 12.3 RAG

`buildRagTab()` (`settings_ai.cpp`):
cartella documenti, max risultati, JL transform, stato indice, lista file con spunte,
modello embedding, reindicizza, **Esporta/Importa database**.

Export/import:
- `RagEngine::saveDb()` → SQLite `rag_meta` + `rag_chunks`.
- `RagEngine::loadDb()` → engine temporaneo; sostituisce solo se valido.
- Il JSON `~/.prismalux_rag.json` resta il formato di lavoro.

### 12.4 Altre sezioni

- `settings_slots.cpp`: reindex RAG, embedding loop, auto-index, salvataggio parametri AI.
- `settings_voice.cpp`: voce, trascrizione, diarizzazione.
- `settings_system.cpp`: lingua, pulizia, consigli HW.
- `settings_other.cpp`: MCP, AiMemory, feedback, ringraziamenti.
- `settings_security.cpp`: certificati, pin, WireGuard.
- `settings_model_profiles.*`: profili pre/post processing.

---

## 13. Flussi end-to-end

### 13.1 Chat base

1. `AgentiPage::onBtnRunClicked()` (`main_ai_slots.cpp:931`) → `runPipeline()` (`main_ai_pipeline.cpp:531`).
2. Guardie locali (math/data/knowledge/cache) prima dell'LLM.
3. `ModelProcessor::preProcess()`, iniezione RAG/Hermes/knowledge, costruzione prompt.
4. `AiClient::chat(sys,user,history,qt)`.
5. `onReadyRead()` → `token`; `onFinished()` → `finished`.
6. `AgentiPage::onToken()` (`main_ai_stream.cpp:35`) aggiorna la bolla.
7. `AgentiPage::onFinished()` (`main_ai_stream.cpp:180`) finalizza e emette `chatCompleted`.
8. `MainWindow::onChatCompleted()` (`mainwindow_chat.cpp:209`) salva in `ChatHistory`;
   `requestSessionSummary()` genera titolo/riassunti con `m_summaryAi`.

### 13.2 Chat con RAG globale

1. Solo overload `chat(sys,user)`: se `m_globalRag && enabled && chunkCount>0 && !busy()`.
2. `_fetchEmbeddingForRag(userMsg)` (ONNX o HTTP).
3. `searchScored(vec, 3)`; inietta hit con score ≥ 0.30, max 600 char ciascuno.
4. Rientra in `chat(sys + contesto, user, {}, classifyQuery)`.
5. Su errore embedding → chat senza RAG.

### 13.3 Chat con Decision Model

1. `chat()` non interna, engine attivo, `DecIdle` → `DecClassifying`.
2. `DecisionEngine::classify()` (cache → ONNX locale oppure `_chatOnce`). Con ONNX il
   callback è sincrono (pochi ms, nessuna rete), ma il contratto è lo stesso.
3. Callback: `DecGenerating`, `m_decIntent` se confidenza ≥ soglia, aggiunge
   `[TIPO DI RICHIESTA RILEVATO: ...]` al system, `m_decSkipGate=true`,
   `m_decBuffering=verifyEnabled()`, rientra in `chat()`.
4. La generazione accumula in `m_accum` **senza** emettere token.
5. `onFinished`: se `DecGenerating && buffering` → `DecVerifying`, `verify(...)`.
6. Verdetto:
   - approvato → `DecIdle`, `token(answer)` in blocco, `finished(answer)`;
   - bocciato e retry disponibile → `DecRetry`, system + obiezione, nuovo tentativo;
   - retry esaurito/errore → mostra comunque la risposta.
7. Tool call: ramo tool **prima** della verifica; una tool call non viene verificata.

> **Fix 2026-10-20**: la ri-entrata interna ora viene riconosciuta **prima** di `busy()`
> (altrimenti `DecGenerating` bloccava la chiamata), e dopo `abort()` vengono ripristinati
> `m_decStage=DecGenerating` e `m_decBuffering` (altrimenti la verifica non partiva).
> Questo bug rendeva la pipeline completamente inerte. Coperto da `test_decision_pipeline`
> (10 test con backend HTTP finto: ordine classify→LLM, buffering, retry, doppio N, streaming).

### 13.4 Tool calling (Ollama)

1. `setActiveTools(array)`.
2. `chat()` include `tools` solo per Ollama.
3. `onReadyRead` raccoglie `message.tool_calls` in `m_pendingToolCalls`.
4. `onFinished` emette `toolCallRequired` e **non** emette `finished`.
5. `AgentiPage::onNativeToolCall()` → `processToolBatch()` → `runToolCall()`.
6. `replyWithTool(s)` riprende il turno; `clearActiveTools()` a fine turno.

### 13.5 MCP

1. Primo uso → `startMcpDiscovery()` (idempotente).
2. `toolSystemSuffix()` pubblicizza i tool MCP.
3. Modello emette `mcp_call` → `onToolMcpCall()`.
4. `_launchMcpProcess()` avvia `MCPs/<plugin>/server.py` col Python della venv.
5. JSON-RPC: `initialize` (id=1), `tools/call` (id=2); stdout parsato.
6. Risultato → `processToolBatch()`.
7. Sicurezza lato server: `_shared/path_safety.py`, `net_safety.py`.

### 13.6 Indicizzazione RAG

1. `onReindexBtnClicked()` (`settings_slots.cpp:727`).
2. `startEmbeddingPhase()` 857: scansione cartella, chunk, embedding (`rag/embedModel`).
3. Rimozione file modificati/eliminati/esclusi; `removeLegacyChunks()`.
4. `indexingProgress/Finished` → status bar e `RicercaPage::onAutoRagTrigger`.
5. Salvataggio `~/.prismalux_rag.json`; `AiClient::setGlobalRag(&m_rag)`.

### 13.7 Download modello decisionale

1. `onDecisionDownloadClicked()` → `DecisionEngine::downloadModel()`.
2. `QProcess` merged: `ollama pull` o `./download_model.sh`. Per llama-server non si
   scarica nulla: si registra l'alias e si verifica su `/v1/models`. Per ONNX non si
   scarica nulla: si ricaricano i due modelli inclusi e si registra `onnx-decision`.
3. `downloadProgress` → label; `downloadFinished`.
4. Se ok, registra il modello col backend corrente e `refreshInstalled()`.
5. `installedReady` → `refreshDecisionUi()` + `refreshDecisionTypeCombos()`.

### 13.8 LAN/headless

1. `LanWanPage` crea `LanServer` (`main_lan_wan.cpp:632`), oppure `--server`.
2. Token Bearer (keychain/file 0600).
3. Endpoint: chat, generate, models, RAG, knowledge, graphviz, whisper, sync, math,
   APK, index, webchat, KaTeX, file/repl/finance/git/wiki/graph.
4. Coda LLM, rate limit, auth.

### 13.9 Split CPU/GPU (D-54)

1. In `showServerDialog()` il pulsante **Split CPU/GPU…** apre
   `showDeviceSplitDialog()` (`mainwindow_backend.cpp`).
2. Il dialog legge `m_hw->hwInfo()`, precompila i budget dall'80% della VRAM libera
   (o dal JSON salvato) e calcola live layer e summary con `DeviceSplit::compute()`.
3. **Salva piano** → `DeviceSplit::save()` (QSettings `llama/split*`).
4. `startLlamaServer()` carica il piano; se attivo sostituisce `-ngl` e aggiunge
   `--tensor-split`/`--split-mode layer`; altrimenti usa la GPU con più VRAM.
5. **Dispositivi llama.cpp** esegue `llama-server --list-devices` per mostrare
   l'ordine effettivo dei device (quello usato da `--tensor-split`).

---

## 14. Persistenza

### 14.1 File e cartelle

| Percorso | Formato | Contenuto |
|---|---|---|
| `~/.prismalux/` | directory | Dati applicazione (creata da `P::dataDir()`). |
| `~/.prismalux/ai_params.json` | JSON | `AiChatParams`. |
| `~/.prismalux/model_params.json` | JSON | Override per modello (`num_predict`, `num_ctx`, ...). |
| `~/.prismalux/decision_models.json` | JSON | Registro decision model. |
| `~/.prismalux/decision_cache.db` | SQLite | `classify_cache(k,intent,confidence,ts)`. |
| `Frameworks/onnx/models/decision_classifier.onnx` | ONNX | Classificatore A/B/C/D (~8 KB, versionato). |
| `Frameworks/onnx/models/decision_verifier.onnx` | ONNX | Verificatore N/Y (~4 KB, versionato). |
| `Frameworks/onnx/models/decision_onnx_meta.json` | JSON | Metadati generatore/accuratezza (non letto a runtime). |
| `~/.prismalux/hermes_memory.db` | SQLite | Grafo memoria Hermes (`gm_nodes/gm_edges`). |
| `~/.prismalux/graph_memory.db` | SQLite | Grafo Multi-Agente/Ricerca. |
| `~/.prismalux/rag_graph.db` | SQLite | Grafo RAG. |
| `~/.prismalux/quiz_ccna.db` | SQLite | Quiz CCNA. |
| `~/.prismalux/sci_compute.db` | SQLite | `work_units`, `sci_nodes`, `wu_results`. |
| `~/.prismalux/wan_tasks.db` (o equivalente) | SQLite | `wan_tasks`. |
| `~/.prismalux/feedback.jsonl` | JSONL | Feedback 👍/👎. |
| `~/.prismalux/cron_jobs.json` | JSON | Job pianificati. |
| `~/.prismalux/response_cache.json` | JSON | Cache risposte. |
| `~/.prismalux/lan_token.key` | testo 0600 | Token LAN. |
| `~/.prismalux/lan_cert.pem`, `lan_key.pem` | PEM | TLS LAN. |
| `~/.prismalux/wan_cert.pin`, `wan_server.pin` | testo | Pin WAN. |
| `~/.prismalux/wireguard_private.key` | testo 0600 | Chiave WireGuard. |
| `~/.prismalux/access.log` | log | Accessi LAN. |
| `~/.prismalux/venv/` | venv Python | Dipendenze MCP/Python. |
| `~/.prismalux/piper/` | binari/voci | TTS. |
| `~/.prismalux/generated/`, `sci_results/`, `distillazione/` | output | Artefatti. |
| `~/.prismalux/devagent_history/` | JSON | Cronologia DevAgent. |
| `~/.prismalux_rag.json` | JSON | Indice RAG di lavoro (`seed`, `input_dim`, `chunks`). |
| `~/.prismalux_chats/` | JSON per sessione | Chat history (`chat_history.json` legacy). |
| `~/.ai-memory/` | git + YAML/MD | Memoria utente (`git log`, revert). |
| `~/PRISMALUX_RAG` (AppImage) / `<root>/RAG` | documenti | Cartella RAG. |
| `<root>/TOOL_TIP/KNOWLEDGE_USER/user_knowledge.md` | Markdown | Conoscenza utente iniettata. |
| `<root>/COSTITUZIONE_PRISMALUX.txt` (o `P::constitutionPath()`) | testo | Costituzione nel system prompt. |

### 14.2 Schema SQLite

**Decision cache**

```sql
CREATE TABLE IF NOT EXISTS classify_cache (
  k TEXT PRIMARY KEY,
  intent TEXT NOT NULL,
  confidence REAL NOT NULL,
  ts INTEGER NOT NULL
);
```

**RAG export** (`saveDb`)

```sql
CREATE TABLE rag_meta (k TEXT PRIMARY KEY, v TEXT);
CREATE TABLE rag_chunks (
  id INTEGER PRIMARY KEY,
  text TEXT NOT NULL,
  source TEXT,
  mtime INTEGER,
  vec BLOB NOT NULL          -- float32 little-endian
);
```

**Knowledge graph** — vedi §9.3.

**Sci compute**

```sql
CREATE TABLE work_units (
  id TEXT PRIMARY KEY, type TEXT NOT NULL, label TEXT DEFAULT '',
  params TEXT DEFAULT '{}', status TEXT DEFAULT 'pending',
  priority INTEGER DEFAULT 1, replicas INTEGER DEFAULT 1,
  depends_on TEXT DEFAULT '', pipeline_id TEXT DEFAULT '', created_at INTEGER
);
CREATE TABLE sci_nodes (
  id TEXT PRIMARY KEY, name TEXT DEFAULT '', address TEXT DEFAULT '',
  port INTEGER DEFAULT 11601, cpu_cores INTEGER DEFAULT 1,
  ram_gb INTEGER DEFAULT 4, gpu_name TEXT DEFAULT '',
  tools TEXT DEFAULT '[]', status TEXT DEFAULT 'unknown', last_seen INTEGER DEFAULT 0
);
CREATE TABLE wu_results (
  wu_id TEXT NOT NULL, node_id TEXT NOT NULL,
  output TEXT DEFAULT '', output_hash TEXT DEFAULT '',
  completed_at INTEGER DEFAULT 0, PRIMARY KEY (wu_id, node_id)
);
```

**WAN**

```sql
CREATE TABLE wan_tasks (
  id TEXT PRIMARY KEY, kind TEXT, payload TEXT, status TEXT, node TEXT,
  result TEXT, created TEXT, started_at TEXT, retry_count INTEGER, priority INTEGER
);
```

### 14.3 QSettings (`Prismalux` / `GUI`)

Le chiavi vivono in `PrismaluxPaths::SK` (`gui/prismalux_paths.h`).
Gruppi principali:

| Gruppo | Chiavi |
|---|---|
| Aspetto | `theme`, `bubble_radius`, `tabs_top`, `icons_text`, `ui/language`, `theme/followSystem`, `ui/triModeEmojiStyle`, `ui/triModeShape` |
| Navigazione | `nav/navStyle`, `nav/tabMode`, `nav/execBtnMode` |
| RAG | `rag/maxResults`, `rag/noSave`, `rag/docCount`, `rag/docsDir`, `rag/jlTransform`, `rag/excludedFiles`, `rag/lastIndexed`, `rag/embedModel` |
| STT/TTS | `stt/model_path`, `stt/http_url`, `stt/fast_whisper_model`, `stt/fast_whisper_enabled`, `stt/diarize_enabled`, `stt/diarize_n_speakers` |
| AI | `ai/activeModel`, `ai/activeBackend`, `ai/activeHost`, `ai/activePort`, `ai/computeMode`, `ai/injectUserKnowledge`, `ai/hermesEnabled`, `ai/constitutionEnabled`, `ai/mlockModel`, `ai/autoZramDoppia`, `ai/autoOptApplied`, `ai/personality`, `ai/cavemanMode`, `ai/autoModelRouting` |
| Smart Router | `ai/smartRouterEnabled`, `ai/cloudApiUrl`, `ai/cloudApiModel`, `ai/cloudApiKey` (legacy; il segreto vive in keychain/file) |
| Decision | `decision/enabled`, `decision/model`, `decision/host`, `decision/port`, `decision/threshold`, `decision/verify`, `decision/cacheTtlHours`, `decision/backend`, `decision/model/{coding,reasoning,chat,other}` |
| Sandbox | `sandbox/enabled`, `sandbox/image`, `sandbox/memoryMb` |
| LAN | `lan/accessToken`, `lan/autoStart`, `lan/port` |
| RPC cluster | `llama/rpcEnabled`, `llama/rpcNodes`, `llama/rpcSshUser`, `llama/rpcServerPath` |
| Split CPU/GPU (D-54) | `llama/splitEnabled`, `llama/splitNgl`, `llama/splitTensor`, `llama/splitSummary`, `llama/splitWarning`, `llama/splitBudgets` (JSON nome→MB) |
| Chat | `chat/maxRecentTurns` |
| Vision3D | `vision3d/vlmModel` |
| Mappa OSM | `osmMap/lastLat`, `osmMap/lastLon`, `osmMap/lastZoom`, `osmMap/numericLabels` |
| Setup / easter egg | `setup/done`, `research/astraleUnlocked`, `loop_fix_warning_shown` |
| Profili modello | gruppo `ModelProfiles` (array JSON). |

---

## 15. Test

82 target in `gui/CMakeLists.txt`. Elenco:

```
test_signal_lifetime test_rag_engine test_code_utils test_random_tool
test_external_ai_import test_lavoro_page test_mcp_manager test_app_controller
test_programmazione_page test_vision3d test_ai_memory test_translitter
test_thinking_detect test_ai_integration test_ai_stress test_team_collab
test_agente_autonomo_live test_chat_live test_multi_agente_live
test_lavoro_data test_tutor_data test_rag_engine_avanzato test_chat_history
test_simulatore_algos test_formula_parser test_strumenti_rag test_theme_manager
test_theme_manager_crash test_hardware_monitor test_manutenzione_cron
test_chat_history_stress test_matematica_page test_qr_code_widget
test_hw_detect_amd test_knowledge_injection test_agenti_pipeline
test_voice_cloner test_tool_dispatchers_d33 test_agents_config_dialog
test_ai_client test_ai_math test_ai_knowledge_lookup test_ai_routing
test_decision_engine test_decision_pipeline test_decision_onnx test_decision_badge test_decision_live test_device_split test_lan_wan_core test_mcp_integration test_lan_server
test_lan_server_endpoints test_lan_server_queue test_wan_compute_tasks
test_sci_compute test_sintetizzatore test_stt_whisper test_stt_whisper_live
test_onboarding test_impostazioni_page test_manutenzione_bugs test_grafico
test_agenti_byzantine test_agente_autonomo test_opencode_page test_docker_sandbox
test_impara_quiz test_monitor_panel test_graph_memory test_graph_memory_concurrent
test_pratico_finanza test_rag_graph_pipeline test_astrale test_repl_python
test_file_parser test_gns3_mcp test_blhm_rab0l test_distillazione
test_dep_check_panel test_perceptor_scripts test_streamlink_mcp
```

Categorie:

- **Puri / senza rete**: `test_decision_engine`, `test_decision_pipeline`, `test_decision_onnx`, `test_decision_badge`, `test_device_split`, `test_rag_engine`, `test_rag_engine_avanzato`,
  `test_formula_parser`, `test_chat_history`, `test_graph_memory`, `test_thinking_detect`,
  `test_hw_detect_amd`, `test_qr_code_widget`, `test_simulatore_algos`, `test_pratico_finanza`.
- **Live / richiedono Ollama o rete**: `test_chat_live`, `test_decision_live`,
  `test_multi_agente_live`, `test_agente_autonomo_live`, `test_stt_whisper_live`,
  `test_ai_stress`, `test_ai_integration`, `test_lan_server*`.
- **GUI offscreen**: usare `QT_QPA_PLATFORM=offscreen`. D-73:
  `PRISMALUX_D73_SCREENSHOT=/tmp/d73.png QT_QPA_PLATFORM=offscreen ./build_tests/test_impostazioni_page`
  esegue anche i controlli su LLM & DM/Decision Model/RAG e salva lo screenshot.
  D-72 badge: `PRISMALUX_D72_SCREENSHOT=/tmp/d72_badge.png QT_QPA_PLATFORM=offscreen ./build_tests/test_decision_badge`;
  pipeline live: `PRISMALUX_D72_LIVE_SCREENSHOT=/tmp/d72_live.png QT_QPA_PLATFORM=offscreen ./build_tests/test_decision_live`.
- **Qualità ONNX**: `python3 gui/tools/eval_decision_onnx.py --json /tmp/onnx-quality.json`.

Esecuzione mirata:

```bash
cmake --build build_tests -j8 --target test_decision_engine
QT_QPA_PLATFORM=offscreen ./build_tests/test_decision_engine
```

---

## 16. Convenzioni e trappole

### 16.1 AiClient

- **Un solo request** per istanza (`busy()` include `m_reply`, `m_localBusy`, `m_decStage`).
  I segnali sono broadcast: filtrare con `currentReqId()`.
- **Ri-entrate interne**: la pipeline decisionale e il retry richiamano `chat()` dall'interno.
  `m_decSkipGate` va letto **prima** di `busy()`, altrimenti lo stadio `DecGenerating`
  blocca la ri-entrata; inoltre `abort()` azzera lo stato decisionale e va seguito dal
  ripristino di `m_decStage`/`m_decBuffering` (fix 2026-10-20).
- **Mai `finished()` dopo `error()`**: evita il "Byzantine loop".
- **Ownership reply**: azzerare `m_reply` **prima** di `abort()`/`deleteLater()`.
- **DecisionEngine — abort di richieste in volo**: stesso principio per `m_tagsReply`,
  `m_classifyReply`, `m_verifyReply`: salvare il puntatore, azzerare il `QPointer`, poi
  `abort()`. Abort emette `finished()` in modo sincrono e il vecchio codice faceva
  `nullptr->deleteLater()` (SEGV, trovato da `test_decision_pipeline`).
- **`/v1/models` non ha `size`**: per i backend OpenAI la presenza nella lista è la
  prova di installazione; `onTagsReply()` inserisce un sentinel (1) perché
  `isInstalled()` storicamente guarda `sizeBytes > 0`.
- **Single-slot ausiliari**: `fetchModelLayers`, `fetchEmbedding`, `transcribeAudio`
  non supportano due richieste concorrenti.
- **ONNX embedder**: segnali globali, nessun request-id.
- **`m_pendingToolCalls`** non viene azzerato in `abort()`: possibile tool spurio.
- **`generate()`** non incrementa `m_reqId`, non usa decision/RAG, non prepende la costituzione.
- **`chatWithImage()`**: `BKNAMES[]` ha 3 elementi ma l'enum 4 → out-of-bounds con `Ds4Server`.
- **`m_lastChatMessages`** è aggiornato solo da `chat()`.
- **Host locali**: `_enforce_local_host` blocca host non-loopback.
- **PII**: solo ramo cloud.
- **RAG globale**: puntatore non owned; se la pagina proprietaria muore, lambda dangling.
- **Embedding**: cambiare `rag/embedModel` dopo l'indicizzazione rende i vettori incoerenti.

### 16.2 DecisionEngine

- `active()` dipende da `m_installed`, popolato solo da un refresh riuscito.
- Callback droppate se arriva una seconda classify/verify.
- Nessun vincolo localhost (a differenza di `AiClient`).
- Cache key non include soglia/versione tassonomia.
- `./download_model.sh` è relativo alla CWD.
- Se il catalogo è vuoto il modello scaricato non viene registrato.
- `decision_engine.cpp` include QtSql incondizionatamente: build senza Qt6::Sql può fallire.
- **ONNX**: `refreshInstalled()` è sincrono (carica i modelli) e ricrea il runner; se i
  file mancano o sono invalidi `m_installed` resta vuoto e `onnxError()` spiega perché.
- **ONNX**: `active()` richiede `m_model == "onnx-decision"`; il salvataggio delle
  impostazioni forza quel nome quando il backend è ONNX.
- **ONNX**: le feature C++ devono restare identiche a `gen_decision_onnx.py`; la fixture
  `tests/data/decision_onnx_features.json` è il guardiano. Cambiare tokenizzazione,
  riservare diversamente i 64 bucket o cambiare le liste di cue senza rigenerare i
  modelli produce predizioni casuali ma nessun errore evidente. Le guardie usano
  `newProper` = nomi propri **non già presenti nella domanda**: un tema come "Git"
  non deve far passare una risposta generica.
- **ONNX**: le guardie deterministiche sono volutamente nel backend, non nel modello;
  non vanno spinte a "fact-checking". I test live con `tev1:4b` (D-72) hanno dato
  classificazione 3/4 e verifica 6/6: il modello reale non è perfetto, quindi non
  usarlo come unica fonte di verità.
- **ONNX**: la guardia numerica richiede cifre o un numero scritto in lettere; una
  risposta puramente simbolica (es. formula senza cifre) può essere bocciata. La
  guardia "capitale" richiede un nome proprio nuovo: una risposta con un pronome
  ("la sua capitale...") può essere bocciata. Sono euristiche, non verità.
- **ONNX**: `PRISMALUX_DECISION_ONNX_DIR` è letto a ogni `defaultDir()`; i test lo usano
  per simulare una cartella vuota. In build senza `HAVE_ONNX` `load()` fallisce con un
  errore esplicito, non con un crash.

### 16.3 RagEngine

- Non thread-safe.
- `loadDb()` se manca `input_dim` assume 256 → indice corrotto.
- `save()` non atomico ma ruota `.bak`; `load()` non fa fallback automatico.
- Nessun dedup in `addChunk`.
- Cambiare seed dopo l'indicizzazione invalida la coerenza.
- `clear()` non resetta il seed.

### 16.4 RagGraph

- Estrazione testo **sincrona** sul thread GUI (timeout fino a 120 s).
- Usa il client condiviso: durante l'ingest altre chat vengono ignorate.
- `stopIngest()` aborta il client condiviso.
- `m_chunkMap` mai popolata → `searchChunks` restituisce chunk sintetici.
- La coda non viene svuotata da `startIngest` → restart riprocessa tutto.

### 16.5 UI / pagine

- Tab lazy: non assumere che i widget esistano prima della costruzione.
- `refreshDecisionUi()` ora ha guardie null (sotto-scheda lazy).
- Ogni pagina con `QProcess` deve disconnettere prima di `kill()` (D-20/D-21).
- `AgentiPage::prepareClose()` va chiamato alla chiusura.
- Le impostazioni condividono l'istanza di `DecisionEngine` con `AiClient`: le modifiche
  sono immediate.
- `ImpostazioniPage` è un god-object (~30 builder): cercare per nome del builder.
- `widget_vision3d.cpp` è il file più grande (3641 righe).
- Unicode: alcuni tool di editing falliscono sui caratteri box-drawing; usare Python
  o riscrittura completa del file.

### 16.6 File enormi (>1000 righe)

`mainwindow_slots.cpp`, `ai_client.cpp`, `settings_llm.cpp`, `settings_ai.cpp`,
`settings_voice.cpp`, `settings_slots.cpp`, `settings_other.cpp`, `settings_main.cpp`,
`widget_vision3d.cpp`, `main_multimedia.cpp`, `main_research.cpp`, `main_graph.cpp`,
`main_math.cpp`, `main_ai_pipeline.cpp`, `main_ai_tools.cpp`, `main_ai_math.cpp`,
`main_ai_ui.cpp`, `main_sci_compute.cpp`, `main_maintenance.cpp`, `main_wan_server.cpp`,
`main_sim_sorting.cpp`, `main_programming_coding.cpp`, `main_tools_file.cpp`,
`main_jobs.cpp`, `lan_server.cpp`, `prismalux_paths.h`, `CMakeLists.txt`.

### 16.7 DeviceSplit (D-54)

- `compute()` **non** legge QSettings: è puro e testabile; il dialog e
  `startLlamaServer()` chiamano `load()` separatamente.
- Il budget è sempre clampato a `availMb`; sotto 2048 MB il device è escluso con warning.
- `--tensor-split` è prodotto solo con ≥ 2 GPU; la somma delle quote è forzata a 1.0
  calcolando l'ultima come `1 - somma(precedenti)`.
- Il piano salvato può essere obsoleto se cambia l'hardware: il dialog va riaperso per
  ricalcolare i budget. `startLlamaServer()` non verifica che i device esistano ancora.
- L'ordine di `--tensor-split` è quello di llama.cpp, non di `HWInfo`: usare
  `--list-devices` nel dialog prima di fidarsi delle quote.
- Mischiare vendor diversi non è bloccato (solo warning): una build llama.cpp con un
  solo backend ignorerà/rifiuterà i device non supportati.

---

## 17. Glossario

| Termine | Significato |
|---|---|
| **LLM** | Modello di conversazione (Ollama/llama-server/cloud). |
| **Decision Model** | Modello separato che classifica la richiesta e verifica la risposta. Non conversa. |
| **Backend** | Motore di inferenza: Ollama, llama-server, LlamaLocal, DwarfStar/ds4, ONNX (solo decision model). |
| **DwarfStar / ds4** | Motore OpenAI-compatible per modelli grandi; `POST /v1/chat/completions`. |
| **llama-server** | Server llama.cpp OpenAI-compatible; stesso protocollo di ds4, modelli GGUF locali. |
| **DecisionOnnx** | Runner locale ONNX Runtime per classificatore A/B/C/D e verificatore N/Y; nessuna rete. |
| **Feature hashing** | Token → FNV-1a 32 bit → 512 bucket, L2-normalize; identico in C++ e Python. |
| **DeviceSplit (D-54)** | Budget VRAM per GPU → layer per device → `-ngl`/`--tensor-split` per llama-server. |
| **RAG** | Retrieval-Augmented Generation: documenti indicizzati iniettati nel prompt. |
| **JLT** | Johnson–Lindenstrauss Transform: proiezione 256 dim che preserva le distanze. |
| **Chunk** | Frammento di documento indicizzato (~400–600 char). |
| **Embedding** | Vettore che rappresenta il testo; prodotto da Ollama/ONNX. |
| **GraphMemory** | Knowledge graph SQLite con nodi/archi tipizzati. |
| **Hermes** | Memoria persistente di `AgentiPage` in `hermes_memory.db`. |
| **MCP** | Model Context Protocol: tool esterni via JSON-RPC stdio. |
| **Tool call** | Function calling nativo di Ollama: il modello chiede un tool, il client lo esegue. |
| **ReAct** | Ciclo Thought/Action/Observation per l'agente autonomo. |
| **Byzantine** | Voto multi-modello per robustezza. |
| **Smart Router** | Routing LOCAL→CLOUD con scrub PII. |
| **Pipeline** | Sequenza controller → agenti → merge in `AgentiPage`. |
| **Lazy tab** | Tab costruita al primo clic (o da timer), non all'avvio. |
| **TTFT** | Time To First Token. |
| **WAN compute** | Calcolo distribuito su nodi remoti con coda e quorum. |
| **Sandbox** | Esecuzione codice in container Docker (`--network none`). |

---

## 18. Ordine di lettura consigliato

1. `PATH.md` (mappa esistente) e questo documento.
2. `gui/main.cpp`.
3. `gui/mainwindow.h` + `mainwindow.cpp`.
4. `gui/mainwindow_tabs.cpp` (lazy tab).
5. `gui/pages/main_ai.h`, `main_ai.cpp`, `main_ai_stream.cpp`, `main_ai_pipeline.cpp`.
6. `gui/ai_client.h/.cpp`.
7. `gui/decision_engine.h/.cpp`, `rag_engine.h/.cpp`, `rag_graph.*`, `graph_memory.*`.
8. `gui/pages/settings_main.h`, `settings_llm.cpp`, `settings_ai.cpp`, `settings_slots.cpp`.
9. `gui/lan_server*.cpp`.
10. `gui/widgets/`.

---

## 19. Stato della mappa

| Area | Copertura |
|---|---|
| Root, build, mainwindow, widget | ✅ |
| AI core (client, decision, ONNX, RAG, graph, memoria, contesto) | ✅ |
| Pages (tutte le famiglie) | ✅ per famiglia/file |
| Settings, Decision Model per tipologia, RAG export/import | ✅ |
| D-54 split CPU/GPU (`device_split`, dialog, `--tensor-split`) | ✅ logica+test; hardware multi-GPU non disponibile |
| MCP, tool, agenti, sandbox | ✅ |
| Persistenza (QSettings, file, SQLite) | ✅ |
| Test (80 target) | ✅ elenco |
| Flussi | ✅ 9 flussi principali |
| Trappole | ✅ per modulo |

**Da aggiornare quando**: si aggiunge un file/target, si cambia una chiave QSettings,
si modifica uno schema SQLite o un flusso principale.
