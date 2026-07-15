# CLAUDE.md — Prismalux Qt GUI  v3.0 (agg. 2026-06-21)

## Build
```bash
# Linux (diretto) — build dir = build_gui/ nella root del progetto
cmake -B build_gui gui/ -DCMAKE_BUILD_TYPE=Release && cmake --build build_gui -j$(nproc)
./build_gui/Prismalux_GUI

# Oppure con aggiorna.sh (aggiorna anche .desktop)
./aggiorna.sh

# Tutte le piattaforme (Windows/Linux/macOS)
python3 build.py
```
Strutturale (nuovo file/CMakeLists) → rifare `cmake -B build_gui gui/`. Solo .cpp/.h → solo `cmake --build build_gui`.
**Nota path**: la build dir canonica è `build_gui/` (nella root `Prismalux/`), **non** `gui/build_gui/`. Il file `.desktop` e `aggiorna.sh` puntano entrambi a `build_gui/Prismalux_GUI`.

`build.py` — motore di build multipiattaforma. Genera `errore.txt` in root se fallisce.
Windows: `build.bat` trova Python ed esegue `build.py`. Prima volta: `COMPILE_WIN\setup.bat`.

### Build macOS (N10 — non ancora testato su hardware reale)
```bash
# Requisiti: Homebrew + Qt6 + cmake
brew install qt6 cmake ninja

# Opzione A — build.py automatico (rileva Homebrew Qt6)
python3 build.py

# Opzione B — cmake diretto (Qt6 da Homebrew Apple Silicon)
export PATH="/opt/homebrew/opt/qt6/bin:$PATH"
cmake -B gui/build_gui gui/ -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt6
cmake --build gui/build_gui -j$(nproc)
open gui/build_gui/Prismalux_GUI.app
```
CMakeLists.txt genera `Prismalux_GUI.app` bundle + copia i framework Qt con `macdeployqt` automaticamente.
Nota: `QSslServer` (TLS LAN) richiede `brew install openssl` e linkage esplicito — aggiungere se necessario.

## Layout tab (mainwindow.cpp)
```
Header (72px): logo · backend · model · CPU/RAM/GPU · spinner · ⚙️
[0] 🤖 Intelligenza Artificiale  Alt+1  Pipeline + Byzantino + CHAT RAG + Agente Autonomo
[1] 🛠 Strumenti                  Alt+2  Assistente AI · 💰 Finanza (730/PIVA/Calcolatori/TFR) · ⏱ Cron · Impara · Sfida! · 📁 File AI
[2] 🎬 Multimedia                        Audio AI · Genera Immagini · 🕸 Mappe concettuali · 🗺 Mappa OSM · Sintetizzatore · 🎤 Clona Voce · OCR webcam
[3] 💻 Programmazione            Alt+3  Editor+AI · Agentica · Translitter · Reverse Eng. · Git · REPL · Interpreter · Rete · Driver
[4] π  Matematica                Alt+4  Sequenza→Formula · Costanti · N-esimo · Espressione · Risolvi Passi (SymPy+🔀) · Analisi 1&2 (LaTeX KaTeX)
[5] 🔧 Utility                           Fotovoltaico · Idroponica · Lavoro · Finanza · 🚴 Bici · LAN & WAN
[6] 🔬 Ricerca                   Alt+5  Paper · Brevetti · Analisi Fenomeni · 🕸️ Grafo RAG · Test RAG · Astrale
[7] 🧬 Bioinformatica                   Cytoscape · RDKit · Bioconda · Avogadro · RAB₀-L · BLHM
[8] 🕹 APP Controller            Alt+6  Blender/FreeCAD/Office/CloudCompare/Anki/KiCAD/TinyMCP/OBS/OpenCode/Godot
ImpostazioniPage: dialog modale (⚙️ header)
```
Note:
- `LavoroPage` è in UtilityPage [5], NON più in RicercaPage
- `PraticoPage` (Finanza), `SolarCalcWidget`, `IdroWidget` e `BikeWidget` sono in UtilityPage [5]
- `LanWanPage` è sub-tab di UtilityPage [5] — NON più tab principale separata
- Cytoscape/RDKit/Bioconda/Avogadro/RAB₀-L/BLHM → BioinformaticaPage [7]
- DevAgent e SecurityAnalyzerPage → ProgrammazionePage (aggiunti via addExternalTab da mainwindow)
- Gestione MCP → ImpostazioniPage (tab "Gestione MCP" dopo tab "MCP")
- Cron (`m_cronPanel`) in StrumentiPage via `installCronPanel()` con `QTimer::singleShot(0)`
- AppController tab indici: 0=Blender 1=FreeCAD 2=Office 3=CloudCompare 4=Anki 5=KiCAD 6=TinyMCP 7=OBS 8=Godot 9=GameModding 10=OpenCode 11=Telegram 12=WhatsApp
- Game Modding tabIdx logico: 9 in AppController (giochi: GTA V, Skyrim SE, Terraria, WoW, Noita, Minecraft, Stardew, RimWorld)
- Multimedia tab indici: 0=Audio AI 1=Genera Immagini 2=Mappe concettuali 3=Mappa OSM 4=Sintetizzatore 5=Clona Voce 6=OCR webcam 7=Analizza Video 8=Scan 3D (`Vision3DWidget`) 9=IPA (`IpaGuideWidget`, guida statica tabella fonemica + 8 fasi apprendimento lingue, immagine `Tools/ipa/phonemic_chart.jpg`)
- Strumenti sub-tab indici: 0-5=categorie assistente 6=Cron 7=Impara 8=Sfida 9=File AI 10=RAM LLM (`RamCalculatorWidget`)
- Web app (lan_server.cpp) tab 🎙️ Voce: TTS (SpeechSynthesis) + STT (MediaRecorder→/api/whisper)
- ProgrammazionePage sub-tab extra: Dev Agent (costruito da AppController.buildDevAgentTab()), Sicurezza (SecurityAnalyzerPage)

## Avvio lazy delle tab principali (`ensureTabBuilt`)

Solo la tab [0] AI è costruita eager nel costruttore di `MainWindow` (blocca `show()`
il meno possibile). Tutte le altre (incluse Strumenti[1]/Programmazione[3], eager
fino a prima dell'ottimizzazione avvio) partono come placeholder vuoti e vengono
sostituite in-place da `ensureTabBuilt(idx)`:
- al primo clic utente (`onMainTabChanged` → `ensureTabBuilt(idx)`)
- o da un timer di pre-build in background (`onPreBuildTabN`, schedulati in
  `setupTimers()`): 1@50ms, 3@250ms, 2@2500ms, 4@3700ms, 6@4900ms, 7@5500ms, 5@8000ms
  — Strumenti/Programmazione per primi perché "contenitori" di Ricerca e
  DevAgent+Sicurezza richiesti da altre tab lazy più avanti nella coda.

`ensureTabBuilt()` preserva la tab visibile all'utente durante un pre-build in
background (`prevCurrent` salvato/ripristinato) — solo un clic esplicito
sull'indice appena costruito lo rende quello corrente.

**Perché**: costruire Strumenti (11 sotto-tab) + Programmazione prima di
`w.show()` costava da sola ~250ms di widget, ma soprattutto forzava
`ThemeManager::apply()`'s `qApp->setStyleSheet()` (~950ms) e il reparenting
in `QSplitter::addWidget()` (~375ms) a operare su un albero widget già enorme
— questi due costi scalano con quanti widget esistono già, non con cosa fanno.
Risultato: costruttore `MainWindow` da ~2000ms a ~850ms.

Dipendenze cross-tab risolte con `ensureTabBuilt()` difensivo invece di
assumere l'ordine di costruzione:
- `createStrumentiWidget()` costruisce Ricerca insieme a Strumenti (`buildRicercaTab()`)
- `createUtilityWidget()` chiama `ensureTabBuilt(1)`/`ensureTabBuilt(3)` prima di
  agganciare Ricerca/Rete&Network
- `createAppControllerWidget()` chiama `ensureTabBuilt(3)` prima di agganciare
  Dev Agent + Sicurezza a `m_progPage`

## Struttura cartelle repo

```
Prismalux/
├── gui/                          ← sorgente C++/Qt6 (questo progetto)
│   ├── pages/                    ← una pagina = .h + uno o più .cpp
│   ├── widgets/                  ← componenti header-only e riutilizzabili
│   ├── tests/                    ← suite ctest (76 suite, BUILD_TESTS=ON)
│   ├── themes/                   ← temi QSS
│   ├── CMakeLists.txt
│   └── CLAUDE.md                 ← questo file
├── EXPORT/                       ← script e artefatti di distribuzione
│   ├── linux/   crea_appimage.sh · install_launcher.sh · Prismalux-x86_64.AppImage
│   ├── windows/ crea_zip_windows.py · build_installer_windows.bat · ZIP
│   ├── android/ build_apk.sh · installa_xiaomi.sh · test_apk.sh
│   └── macos/   (futuro)
├── ANDROID/                      ← app Android Qt6 (BLE, CCNA, TTS, LAN)
│   └── PrismaluxMobile.apk       ← path hardcoded: P::root()+"/ANDROID/PrismaluxMobile.apk"
├── MCPs/                         ← 50 plugin MCP Python — ognuno ha README.md + requirements.txt
├── BEST_PRACTICE_&_GOAL/         ← regole, TODO, operazioni (non codice)
│   └── REGOLE_IRREMOVIBILI.md    ← 15 convenzioni fisse — leggere prima di toccare il codice
├── EXTERNAL_DeviceS/             ← script e config dispositivi fisici (WIBY cam, Tuya, PTZ — non in git)
├── Test/                         ← test Python AI integration + utility
├── RAG/                          ← documenti RAG (locale, non in git)
├── KNOWLEDGE_USER/               ← memoria utente (locale, non in git)
├── build.py                      ← build multipiattaforma Windows/Linux/macOS (motore unico)
├── build.bat                     ← launcher Windows: trova Python → esegue build.py
├── aggiorna.sh / aggiorna.bat    ← build + AppImage/ZIP completo
├── requirements.txt              ← dipendenze Python (numpy, pandas, opencv, scipy, ecc.)
└── COMPILE_WIN/                  ← toolchain portatile Windows (setup.bat scarica ~600 MB + Python embedded)
```

## File chiave
| File | Ruolo |
|------|-------|
| `mainwindow.h/cpp` | Header, tab bar, llama-server manager |
| `ai_client.h/cpp` | HTTP Ollama/llama-server — `chat()`, `fetchModels()`, `fetchEmbedding()` |
| `prismalux_paths.h` | Unico punto di verità per path, porte, QSettings keys |
| `dpi_utils.h` | `dpiScale(N)` — scala px per DPI/Wayland HiDPI. Usare SEMPRE per dimensioni strutturali |
| `rag_engine.h/cpp` | RAG JLT 256-dim |
| `hardware_monitor.h/cpp` | Thread polling CPU/RAM/GPU ogni 2s |
| `lan_server.h/cpp` | Server TCP LAN per PrismaluxMobile Android (porta 11500) |
| `lan_wan_page.h/cpp` | LAN Android + GNS3 MCP + WAN Compute (porta 11600) |
| `pages/agenti_page.*` | Pipeline 6 agenti + Byzantino + Agente Autonomo (15 moduli) |
| `pages/pratico_page.*` | 730, P.IVA, Calcolatori Finanza, Scheda TFR (C.F. auto D.M. 1976 + Belfiore) |
| `pages/ricerca_page.*` | Tab Ricerca [6] — include Cyto/RDKit/Bio/Avo + Analisi Fenomeni (allegati PDF) |
| `pages/matematica_page.*` | Matematica SymPy; errore fetchModels→ setStatus() via holder |
| `pages/agenti_multi_page.*` | Multi-Agente: MasterAgent, SubTask, GraphMemory live — embedded in WAN Compute [8] |
| `graph_memory.h/cpp` | GraphMemory SQLite-backed: nodi/archi, BFS neighbours, DOT/JSON/TXT export |
| `rag_graph.h/cpp` | RagGraph: scansiona RAG dirs, estrae entità+relazioni LLM → GraphMemory |
| `widgets/latex_view.h` | LatexView (QWebEngineView+KaTeX o QTextEdit fallback) per formule |
| `widgets/tri_mode_button.h` | TriModeButton: ovale QPainter 3 settori (Chat/Agentico/Conversa) + hub azione; emoji SVG OpenMoji opzionale; Shift+Tab cicla modalità |
| `widgets/ai_error_widget.h` | Header-only Q_OBJECT — `showError(msg, onRetry)` — elencato in CPP_SRCS |
| `widgets/stt_daemon.h` + `Tools/scripts/stt_daemon.py` | Demone STT persistente: modello faster-whisper caricato una volta, ~1.7s/frase vs ~11s one-shot; warm-start in `_sttStartRecording()`, fallback automatico allo script one-shot se assente |
| `widgets/code_interpreter_widget.h/cpp` | Python sandbox: exec, matplotlib PNG, Docker |
| `pages/widget_vision3d.h/cpp` | Vision3D (Multimedia→Scan 3D): server HTTPS porta `P::kVision3DPort`, foto da telefoni → VLM Ollama + box OpenCV + depth (`Tools/scripts/depth_infer.py`) + scala ArUco (`Tools/aruco/` PDF stampabili); cert condiviso `LanServer::_ensureCert`. UI a due sotto-schede: "Preparazione" (QR collegamento + stampa bersagli ArUco + device/galleria/ultimo scatto) e "Assembla punti e texture" (scena 3D + ricostruzione COLMAP) — vedi sotto |
| `widgets/world_map_widget.h/cpp` | WorldMapWidget OpenStreetMap tiles + Nominatim + routing OSRM (waypoint, polyline, draw) |
| `MCPs/knowledge_mcp/server.py` | Knowledge Updater MCP (JSON-RPC 2.0 stdio) |
| `MCPs/ollama_mcp/server.py` | Ollama model cache MCP — SQLite TTL 5min, 5 tool (list/info/search/sync/pull) |
| `MCPs/devagent_mcp/server.py` | Dev Agent LangGraph — loop Python fallback, 28 tool IPC, git log/restore/fetch/stash |

## Convenzioni critiche

**Path — mai hardcode:**
```cpp
#include "prismalux_paths.h"
namespace P = PrismaluxPaths;
// P::root(), P::kOllamaPort, P::kLlamaServerPort, P::kOpenCodePort
// P::kWanComputePort (11600), P::modelIcon(sizeBytes, name)
// P::feedbackPath(), P::userKnowledgePath()
```

**DPI — usare `dpiScale()` per tutte le dimensioni strutturali:**
```cpp
#include "dpi_utils.h"
setFixedWidth(dpiScale(80));    // non setFixedWidth(80)
setFixedHeight(dpiScale(36));   // non setFixedHeight(36)
// dpiScale() è un no-op a 96dpi, scala su HiDPI/Wayland 2×
```

**fetchModels() con gestione errore:**
```cpp
auto* holder = new QObject(this);
connect(m_ai, &AiClient::modelsReady, holder, [this, holder](const QStringList& l) {
    holder->deleteLater();
    fillCombo(l);
});
connect(m_ai, &AiClient::error, holder, [this, holder](const QString&) {
    holder->deleteLater();
    setStatus("❌ Backend non raggiungibile — avvia Ollama.");
});
m_ai->fetchModels();
// NON lasciare error senza handler: il combo resta vuoto senza feedback
```

**Backend:** usa sempre `m_ai->backend()` — non hardcodare `AiClient::Ollama`.

**Emoji hex:** concatena quando il char successivo è cifra hex:
`"\xe2\x80\x9c" "Costruito..."` — non `"\xe2\x80\x9cCostruito..."` (C è hex valida).

**Emoji in `const char*` desc**: ASCII puro per evitare warning "hex escape out of range" quando una sequenza `\xNN` è seguita da cifre 0-9/a-f.

**HAVE_SVG**: `Qt6::Svg` è opzionale. CMakeLists: `find_package(Qt6 COMPONENTS Svg QUIET)` + `add_compile_definitions(HAVE_SVG)`. Metti `Q_UNUSED(var)` nel blocco `#else`, non in `#ifdef`.

**m_btnVoiceLoop**: presente come membro ma `buildToolbarVoiceLoop()` non è chiamata (funzionalità Conversa ora nel TriModeButton). Tutti gli accessi devono avere `if (m_btnVoiceLoop)`.

**Combo modello:**
```cpp
combo->addItem(P::modelIcon(sz, m) + m, m);
QString modello = combo->currentData(Qt::UserRole).toString();
```

**Lambda nelle `connect()` — regola del progetto:**

Preferire slot nominati. Lambda accettabile solo se:
1. il context object (4° argomento) è sempre specificato
2. tutti i puntatori catturati sono figli del context object

```cpp
// ✅ context = this, cattura solo figli di this
connect(btn, &QPushButton::clicked, this, [this]{ m_stack->setCurrentIndex(1); });

// ✅ context = bar, cattura widget figli di bar
connect(m_ai, &AiClient::modelsReady, bar, [=](const QStringList& l){ cmb->clear(); ... });

// ❌ nessun context — use-after-free
connect(reply, &QNetworkReply::finished, [this, reply]{ ... });

// ❌ static QMetaObject::Connection — condivisa tra istanze → zombie
static QMetaObject::Connection c;
```

Logica non banale (> 2 righe o accesso a più variabili membro) → slot nominato.
Pattern one-shot preferito: `QMetaObject::Connection` come membro, disconnect esplicito.

**ThemeManager:** `static ThemeManager inst(nullptr)` — MAI `inst(qApp)` → ABRT shutdown.

**LanServer shutdown:** `blockSignals(true)` prima di `stop()` — evita SIGSEGV.

**QProcess figli nei distruttori widget:** `~QProcess` di un processo ancora attivo emette `finished()` **sincrono** (via `waitForFinished()` interno) durante `deleteChildren()` — i lambda connessi toccano label/bottoni già distrutti → SEGV. Il context object `this` nel connect NON protegge: la connessione è ancora viva mentre l'oggetto è semi-distrutto. Nel distruttore del widget: `findChildren<QProcess*>()` → `disconnect(this)` + `blockSignals(true)` + `kill()` + `waitForFinished(1000)` (copre anche i QProcess anonimi, non solo i membri m_*). Caso reale: `~VoiceClonerWidget` + probe di `checkTtsInstalled()` (coredump 2026-07-03, fix D-20).

**webchat.html tab bar (v2.9+):** 2 righe per sezione (`#tabbar` → `flex-direction:column`). Riga 1: AI (Chat/Agenti/RAG/Grafo/Know.) + Strumenti (Finanza/TFR/Lavoro/File AI/REPL). Riga 2: Learn (Impara/Media/Voce/App) + Dev (Coding/Matema./Graphviz/Git/Wiki/Sistema). CSS: `.tab-row` wraps; `.sec-lbl` label di sezione con bordo destro.

**webchat.html font size:** CSS var `--fz:14px` su `:root`. Pulsanti `#fz-dn` / `#fz-up` (A-/A+) in `#hdr` aggiornano `--fz` + `localStorage.plx-fz`. `.msg` usa `font-size:var(--fz)`.

**onUpdateQrInline():** legge token da `m_lanTokenEdit`; se vuoto → fallback `LanServer::loadLanToken()`. URL completo con token nel tooltip del QR. Copy button usa stesso URL con token + `/web` path.

## TriModeButton (`widgets/tri_mode_button.h`)

Widget header-only (QPainter), ovale a 3 settori selezionabili + hub centrale ovale (pulsante azione).

| Settore | Modo | Emoji |
|---------|------|-------|
| Top | `Chat = 0` | 💬 (U+1F4AC) |
| Lower-right | `Agentico = 1` | 👔 (U+1F454) |
| Lower-left | `Conversa = 2` | 🎙 (U+1F399) |

```cpp
m_modeBtn = new TriModeButton(parent);
connect(m_modeBtn, &TriModeButton::modeChanged, this, &AgentiPage::onModeBtnChanged);
connect(m_modeBtn, &TriModeButton::actionClicked, this, &AgentiPage::onBtnRunClicked);
m_modeBtn->setActionText("\xf0\x9f\x93\xa4 Invia");
m_modeBtn->setActionEnabled(true);
m_modeBtn->setActionDanger(false);   // hub rosso durante stream
m_modeBtn->setMode(TriModeButton::Chat, /*emitSignal=*/false);
```

**Shortcut Shift+Tab** (`Qt::Key_Backtab`): cicla Chat→Agentico→Conversa solo quando la pagina AI è visibile (`Qt::WidgetWithChildrenShortcut`).

**Emoji style**: letto da `QSettings("Prismalux","GUI").value("ui/triModeEmojiStyle","system")`.
- `"system"` → testo emoji con outline 8-offset (fallback universale)
- `"openmoji"` + `HAVE_SVG` → `QSvgRenderer` da `gui/resources/emoji/1F4AC.svg` ecc. (OpenMoji CC BY-SA 4.0)

I renderer SVG sono `static QSvgRenderer* rend[3]` (inizializzati on-demand). Impostazione in Visuale → Aspetto → "Icone modalità AI".

**Colori**: usano palette del tema attivo. Settore attivo → accento HSV. Settore inattivo hover → `QPalette::Button`. Divisori → `QPalette::Mid` (non Shadow). Hub base → `QPalette::Button`.

## AgentiPage — pannelli Tool

Due pannelli separati con **comportamento radio** (solo uno visibile alla volta):

| Widget | Pulsante | Contenuto |
|--------|----------|-----------|
| `m_toolsPanel` | `m_btnToolsToggle` "⚡ Tool Veloci" | Function Tools in-process, grid 2 colonne + hint modello tool-capable |
| `m_mcpPanel` | `m_btnMcpToggle` "🔌 Tool Lenti (MCP)" | MCP subprocess, scroll grid 4 colonne |

Apertura di un pannello chiude automaticamente gli altri due (Simboli incluso) tramite `blockSignals(true/false)`.

**Bubble radius in tempo reale**: `ImpostazioniPage::bubbleStyleChanged()` → `AgentiPage::onBubbleStyleChanged()` — regex `(border-radius:)(\d+)(px;padding:10px 14px)` su `m_log->toHtml()` per aggiornare solo le celle delle bolle principali (non tool strip che usa `padding:8px 12px`).

## History conversazione singola (`main_ai.h` + `main_ai_pipeline.cpp`)
`m_chatPairs` (`QVector<QPair<QString,QString>>`) accumula i turni della chat singola (non pipeline).
- Reset automatico quando il log è vuoto (pulsante "Nuova chat")
- Limit: `kChatHistoryMax = 10` turni — i più vecchi vengono rimossi
- Ogni turno completato viene accodato in `_finishedPipeline()` (solo se `m_maxShots == 1`)
- `runAgent()` costruisce `histArray` (JSON `[{role,content}...]`) e chiama `m_ai->chat(sys, user, histArray, QueryAuto)`

## Ricerca online e inserimento manuale (`main_ai_ui.cpp`)
Handler link nel log AI:
- `websearch:<base64url>` → QInputDialog pre-compilato con query originale (l'utente può ridurla al soggetto)
- `insertinfo:<base64url>` → QInputDialog::getMultiLineText per inserire risposta manuale → salvata in `RAG/RICERCA/<ts>_<slug>.md` + emit `onlineSearchResultReady`
- `autoapply-params` → applica Temperatura=0.3, Context=16384, Top-P=0.9, MaxTokens=4096 via `AiChatParams::save()` + `m_ai->setChatParams()`

Il link "aggiorna informazioni" compare automaticamente nei casi NORESULT e errore/offline
della ricerca online, così l'utente può inserire la risposta se la conosce.

## AiClient — API
```cpp
m_ai->chat(sys, msg);                        // → token + finished | error
m_ai->chat(sys, msg, historyJson, QueryType);// con storia compressa
m_ai->abort();                               // → aborted() — NON chiama onFinished()
m_ai->fetchModels();                         // → modelsReady(QStringList)
m_ai->fetchEmbedding(t);                     // → embeddingReady(vec) | embeddingError(msg)
m_ai->setNumGpu(n);                          // n<0 = Ollama auto
m_ai->fetchModelLayers(cb);                  // → cb(int layers)
m_ai->unloadModel();                         // keep_alive=0
// QueryType: QueryAuto | QuerySimple | QueryComplex
```

## Think Mode
`AiChatParams` (`~/.prismalux/ai_params.json`): `thinkMode` (0=auto/1=off/2=on), `thinkBudget` (1-4).
Think-capable: qwen3, deepseek-r1, qwq, qwen2.5.
`classifyQuery()`: ≤30 char → Simple; >200 char o keyword → Complex; resto → Auto.
`finished()` emette `"<think>…</think>" + content` — toggle ▶️ inline nella bolla.

## Modalità Calcolo LLM
| Modo | `num_gpu` | Note |
|------|-----------|------|
| GPU | `= layers` (da `/api/show`) | tutti layer su GPU |
| CPU | `0` | tutto in RAM |
| Misto | `= min(layers, capacity_NVIDIA)` | riempie VRAM, overflow CPU |
| Doppia GPU | `-2` | AiClient non manda `num_gpu` |

**Critico:** Ollama NON clipa `num_gpu` → usare sempre `fetchModelLayers()` prima di GPU/Misto.

## Memoria Persistente (Knowledge)
- File: `KNOWLEDGE_USER/user_knowledge.md` (non in git)
- MCP: `MCPs/knowledge_mcp/server.py` (JSON-RPC 2.0 stdio)
- Helper: `P::prependKnowledge(sys)`, `P::readUserKnowledge()` (cache 30s), `P::invalidateKnowledgeCache()`
- Key: `P::SK::kInjectUserKnowledge`
- Estrattore silenzioso: `agenti_page_knowledge.cpp::runKnowledgeExtract()`

## LAN Server Android
```cpp
LanServer* srv = new LanServer(m_ai, this);
srv->start(11500); srv->stop(); srv->isRunning(); srv->clientCount();
```
Endpoint: `/api/tags`, `/api/chat`, `/api/generate`, `/knowledge`, `/apk`, `/`.
APK path: `P::root() + "/ANDROID/PrismaluxMobile.apk"` (con `/` iniziale).
`SO_REUSEADDR` su Linux — evita "Address already in use" al riavvio rapido.

## RAG e OpenCode
RAG condiviso: `AgentsConfigDialog::m_sharedRag` — iniettato prima del RAG per-agente.
OpenCode: porta sempre `P::kOpenCodePort`. SSE events: `message.updated`, `session.idle`, `session.error`.

## Tool Executor (`main_ai_exec.cpp` + `main_ai_pipeline.cpp`)

`ExecCode { QString lang; QString code; }` — struct ritornata da `extractExecutableCode(text)`.
Lingue supportate (in ordine di priorità): **python/py** → **cpp/c++/cxx** → **c**.
Regex richiede hint linguaggio esplicito — blocchi ` ``` ` senza hint ignorati.

| Linguaggio | Runner | Flags |
|------------|--------|-------|
| python/py | `python3 -c <tmpfile>` | ModuleNotFoundError → auto pip |
| c | `gcc -o bin src.c -lm -Wall -Wextra && bin` | timeout 10s |
| cpp/c++ | `g++ -o bin src.cpp -lm -Wall -Wextra && bin` | timeout 10s |

Quando l'utente clicca "No" al dialog → banner ▶ **Riesegui in sandbox** nel log.
`m_pendingExecCodes` (QMap<int,QString>) conserva `"lang:codice"` per il riesegui.

## Bolla Utente (`main_ai_bubbles.cpp`)
- `buildUserBubble(text, bubbleIdx, displayHtml)` — `displayHtml` è HTML leggero da `extractInputHtml()`
- `id='ubbl:N'` sulla `<table>` della bolla utente — usato dal gestore **Rifai**
- Rifai: `retry:` handler tronca il log HTML da `<table id='ubbl:N'>` (non salta la bolla)
- **Non** usare `m_skipNextUserBubble` (rimosso) — il troncamento è pulito e reversibile

## extractInputHtml (`main_ai_p.h`)
Converte QTextEdit in HTML leggero per la bolla utente. Regola critica sul colore:
```cpp
// ✅ Propaga colore SOLO se impostato esplicitamente dall'utente
if (fmt.hasProperty(QTextFormat::ForegroundBrush)) {
    const QColor fg = fmt.foreground().color();
    if (fg.isValid() && fg.alpha() > 0 &&
        fg != QColor(Qt::black) && fg != QColor(Qt::white))
        text = "<span style='color:...'>text</span>";
}
```
**Perché**: il tema QSS imposta il colore del QTextEdit via `QPalette`, ma Qt può propagarlo
nel `QTextCharFormat`. Senza `hasProperty()`, il colore scuro del tema (es. `#1e293b`)
veniva iniettato nella bolla utente con sfondo `#162544` → testo invisibile (nero su blu scuro).
`hasProperty(ForegroundBrush)` distingue colore-utente esplicito da colore-tema ereditato.

## Notazione KaTeX nel system prompt
`kFmtFull` include istruzioni complete per formule matematiche stile LibreOffice Math:
operatori (`\times \cdot \pm \oplus`), relazioni (`\leq \approx \equiv \rightarrow`),
insiemi (`\mathbb{R} \mathbb{Z} \in \subset \cap`), funzioni (`\sin \cos \ln \exp`),
attributi (`\vec{a} \hat{a} \dot{a}`), parentesi scalabili (`\left( \right)`).
Delimitatori: `\(...\)` inline, `\[...\]` display.

## Avvio lazy di ImpostazioniPage (`widgets/lazy_tab_loader.h`)

`ImpostazioniPage` costruiva **tutte** le sue ~30 tab/sotto-tab eager nel
costruttore — nessuna laziness, a differenza di `MainWindow`. Costo
originale misurato (profiling temporaneo con `QElapsedTimer`, poi rimosso):
~1000-1400ms alla prima apertura di Impostazioni (`ensureSettingsDialog()`
in mainwindow.cpp la costruisce una sola volta, riusata alle aperture
successive — costo one-shot per sessione, non ripetuto ad ogni click su ⚙️).

**`LazyTabLoader`** (`widgets/lazy_tab_loader.h`) generalizza il pattern
`ensureTabBuilt()` di `MainWindow` per qualunque `QTabWidget`: `addEager()`
per la prima tab (obbligatoria — `QTabWidget` non emette `currentChanged`
per lo stato iniziale), `addLazy(label, factory)` per le altre, costruite
al primo `currentChanged` verso quell'indice (clic utente o ricerca tab)
sostituendo un placeholder vuoto via `removeTab`+`insertTab`+
`setCurrentIndex` esplicito (necessario: `removeTab` sulla tab corrente
può spostare la selezione su un vicino). Nessun `Q_OBJECT`/moc richiesto
— `onCurrentChanged` è un metodo pubblico normale, il connect per
puntatore-a-membro non lo richiede.

**Applicato a due livelli**:
- **Esterno** (`m_tabs`, ~10 tab): solo "AI Locale" (default) e "Visuale"
  (serve subito a `setGraficoCanvas()`, chiamata da mainwindow.cpp appena
  dopo il costruttore) restano eager. LLM/Sistema/MCP/Gestione MCP/
  Feedback/Ringraziamenti/Sicurezza WAN/Profili Modello sono lazy —
  `buildGroupLlm()` (settings_llm.cpp) e `buildGroupSistema()`
  (settings_system.cpp) costruiscono i due gruppi con sotto-tab quando
  richiesti.
- **Interno** (dentro "AI Locale", 12 sotto-tab): solo "Connessione"
  resta eager, le altre 11 (Hardware/Gestione LLM/Parametri AI/RAG/
  Voce&Audio/Sandbox/Moduli Python/Aggiornamenti Sistema/Fine-tuning/
  llama.cpp Studio/Avanzate) sono lazy. Stesso trattamento per i gruppi
  LLM (solo "LLM Consigliati" eager) e Sistema (solo "Pulizia" eager) una
  volta che l'utente li apre.

**Risultato misurato** (profiling temporaneo, poi rimosso): costruttore
`ImpostazioniPage` da ~1000-1400ms a **~140ms** (prima apertura nel
processo) / **~50ms** (aperture successive, cache calde) — riduzione
~90%. Vedi anche il fix precedente di `buildTestTab()` (25 pagine
`QStackedWidget` da eager a lazy, ~465ms→~45ms), ora reso ridondante
dal fatto che l'intero gruppo LLM è lazy, ma mantenuto perché comunque
corretto anche a gruppo già costruito.

**Trappola incontrata**: `findChild<QTabWidget*>("settingsInnerTabs")`
nei test trova il PRIMO match per objectName — con più `QTabWidget`
interni che condividono lo stesso objectName (AI Locale, Visuale, ...) va
usato `findChildren` e cercato esplicitamente per testo della tab, non
assunto quale istanza si ottenga.

**Non ancora affrontato**: `buildPuliziaTab()` (tab Sistema→Pulizia,
eager nel gruppo Sistema) chiama `calcFilesSize()` — scansione ricorsiva
sincrona (`QDirIterator` + `Subdirectories`) su più cartelle (EXPORT/,
/tmp, cache) — costo isolato ~185ms. Non più sul percorso critico
dell'apertura di Impostazioni (parte solo al clic su "Sistema"), ma
resta bloccante per chi visita quella tab. Candidato a diventare
asincrono (QtConcurrent + label "calcolo...").

Test: `test_impostazioni_page.cpp` CAT-E (`TestLazyTabsNavigation`) — tab
LLM/Sistema si costruiscono al clic, la selezione resta su quella
cliccata, ri-selezionare una tab già costruita non la ricostruisce.

## Aggiornamenti Sistema (`widgets/widget_docker_update.h` + `widget_python_update.h`)

Tab Impostazioni → AI Locale → "🔄 Aggiornamenti Sistema" (`ManutenzioneePage::buildSystemUpdates()`).

**DockerUpdatePanel**: `docker ps -a` → elenco container, `docker pull <image>` per
uno o tutti (dedup per immagine). Dopo un pull riuscito, `docker inspect` rileva
le label `com.docker.compose.project.working_dir`/`.service`: se presenti, offre
"Ricrea ora" (`docker compose up -d --force-recreate`); altrimenti mostra solo un
promemoria manuale — **nessuna ricreazione automatica di container non-compose**,
per non rischiare di perdere dati nei volumi anonimi.

**PythonUpdatePanel**: scope limitato ai 33 pacchetti di `requirements.txt` (non
l'intero ambiente Python). `pip list --format=json` + `pip list --outdated
--format=json` (2 chiamate) per popolare installata/disponibile. Campo
"Versione target" editabile per scegliere anche un downgrade esplicito.
Flusso update: `pip install pkg==target` → se riesce, `python3 -c "import
<modulo>"` come check di compatibilità → se l'import fallisce, rollback
automatico a `pip install pkg==<versione precedente>` (salvata prima
dell'update) e la coda "Aggiorna tutti" si **ferma lì** (gli altri pacchetti
restano alla versione già in uso). Mappa pip→import name hardcoded in
`PythonUpdatePanel::packages()` (es. `opencv-python`→`cv2`, `Pillow`→`PIL`,
`beautifulsoup4`→`bs4`, `PyJWT`→`jwt`).

## Importa da AI esterne (`widgets/external_ai_import.h`)

Tab Impostazioni → AI Locale → Feedback → sezione "📥 Importa da AI esterne"
(`settingsDoImportExternalAi()` in `settings_other.cpp`).

`ExternalAiImport::parseFile(path, detectedFormat, errorMsg)` rileva ed
elabora (formati verificati contro fonti pubbliche, non indovinati):
- **OpenAI ChatGPT** — export dati account: `conversations.json` con
  `mapping` (albero nodi) + `current_node`; `parseOpenAiConversation()`
  risale i `parent` da `current_node` alla radice, poi inverte.
- **Anthropic Claude** — export dati account: `chat_messages: [{sender,
  text}]`, ordine sequenziale (schema non documentato ufficialmente da
  Anthropic, dedotto da fonti terze concordanti).
- **Generico OpenAI-compatible** — `{"messages":[{"role","content"}]}` o
  array bare. **DeepSeek e Qwen non hanno un export ufficiale proprio**
  (verificato — nessuna fonte lo documenta): le loro chat, se esportate con
  estensioni browser terze, tipicamente seguono questo schema.
- **NON gestito**: Google Takeout (Gemini Apps) — l'export esiste ma i
  nomi dei campi JSON non sono documentati in modo affidabile da nessuna
  fonte trovata; niente parser "a intuito".

Ogni conversazione riconosciuta diventa una sessione in `ChatHistory`
(`newSession()` + `saveSession()` con log HTML generato ad-hoc, non le
bolle live di `AgentiPage` — import, non serve pixel-identico), visibile
nella cronologia chat esistente. Ruoli normalizzati: `user`/`human`→`user`,
`assistant`/`model`/`bot`→`pipeline` (schema `ChatMessage::role` interno).

## Scheda TFR — Codice Fiscale automatico (`pratico_page.cpp`)
- `calcolaCodiceFiscale(cognome, nome, nascita, maschio, belfiore)` — algoritmo D.M. 23/12/1976
- `cercaBelfiore(comune)` — QHash ~120 comuni IT + ~30 paesi esteri (codici Z)
- `normalizzaComune(s)` — strip accenti + toLower per lookup case-insensitive
- UI: Nome/Cognome | Data nascita + Sesso + Comune + Belfiore (auto) | C.F. read-only + 🔓
- Aggiornamento live ad ogni cambio campo; RAG auto-fill include anche dati anagrafici nascita

## WAN Calcolo Distribuito (`lan_wan_page.h/.cpp`)
- Porta: `P::kWanComputePort = 11600`; protocollo JSON newline su TCP
- `WanNode`/`WanTask` struct; `wanDispatch()` assegna pending→idle
- `wanCliHandleTask(id, kind, payload)` — dispatcher 28 tipi task
- `wanPopulateKindCombo(QComboBox*)` — QStandardItemModel con separatori NoItemFlags
- `wanKindTemplate(kind)` — template payload automatico
- `m_execModeStack`: index 0 = `AgentiMultiPage` (Solo PC), index 1 = pannello LAN/WAN
- Radio "🧠 Solo questo PC | 🌐 Rete LAN" — controlla `m_execModeStack`
- `m_multiAgentTab` creato dentro `buildWanComputeTab()` (non nel costruttore)
- API pubblica `multiAgentTab()` usata da `mainwindow.cpp` per cross-pollination RagGraph

## GraphMemory API (`graph_memory.h`)
```cpp
GraphMemory gm("~/.prismalux/graph_memory.db");
gm.open();
// Scrittura
QString nodeId = gm.addNode("entity", "Qt6", "Framework GUI C++", 0.9f);
gm.addEdge(nodeId, otherNodeId, "usa", 0.8f);
gm.updateNode(nodeId, "Framework GUI C++ cross-platform", 0.95f);
// Lettura
auto nodes = gm.allNodes();          // tutti i nodi, ordinati per importanza
auto nodes = gm.allNodes("entity");  // filtro per tipo
auto nbrs  = gm.neighbours(id, 2);  // vicini entro 2 hop (BFS)
auto found = gm.searchNodes("Qt");   // ricerca testuale
auto node  = gm.nodeByLabel("Qt6");  // std::optional<GmNode>
// Export
QString dot  = gm.toDot("Titolo", 150);   // Graphviz DOT
QString json = gm.toJson(200);             // JSON compatto
gm.exportTxt("~/memory.txt", 100);         // snapshot testuale
// Manutenzione
gm.pruneByImportance(500);  // tieni solo top-500 nodi
gm.clearAll();
connect(&gm, &GraphMemory::changed, this, &MyWidget::onGmChanged);
```
Tipi nodo: `entity | concept | fact | task | result | documento | rag_chunk`
Tipi arco: `relates_to | elaborates | contradicts | causes | part_of | task_of | derived_from | usa | basato_su`

## RagGraph API (`rag_graph.h`)
```cpp
RagGraph rg(m_ai, m_gm);
rg.addDirectory("~/prismalux_rag_docs");
rg.addFile("/path/to/doc.pdf", "MyDoc");
rg.startIngest();  // asincrono — segnali progressUpdated + finished
rg.stopIngest();
// Ricerca
auto chunks = rg.searchChunks("Qt6", 10);   // QVector<RagGraphChunk>
auto nodes  = rg.searchNodes("formula", 20);
```
Percorsi RAG analizzati: `~/prismalux_rag_docs/` + `P::root() + "/RAG/"`
DB separato da FEAT-1: `~/.prismalux/rag_graph.db`

## Multi-Agente (`agenti_multi_page.h`)
- `GraphMemory* graphMemory()` — accesso alla memoria condivisa dall'esterno
- Piano MasterAgent: JSON `{"task":"...","subtasks":[{"id":1,"role":"...","prompt":"...","depends_on":[]}]}`
- Esecuzione: sequenziale rispettando `depends_on` (BFS ordering)
- Ogni risultato salvato come nodo tipo `"result"` in GraphMemory
- DB: `~/.prismalux/graph_memory.db`

## LaTeX KaTeX (`widgets/latex_view.h`)
```cpp
LatexView* v = new LatexView(parent);
v->setLatexHtml(htmlWithLatex, "#1e293b", "#e2e8f0");
// delimitatori: \(...\) inline, \[...\] display
// richiede Qt6::WebEngineWidgets + libjs-katex (/usr/share/javascript/katex/)
// fallback automatico a QTextEdit se HAVE_WEBENGINE_WIDGETS non definito
```

## WorldMapWidget routing (`widgets/world_map_widget.h`)
```cpp
WorldMapWidget* map = new WorldMapWidget(parent);
// Modalità itinerario (click = aggiungi waypoint invece di singolo marker)
map->setRouteMode(true);
map->addWaypoint(lat, lon, "A");        // aggiunge tappa con etichetta
map->clearRoute();                       // pulisce waypoint + polyline
map->setRouteLine(pts);                  // imposta polyline da OSRM (QVector<QPair<double,double>>)
const auto& wps = map->waypoints();      // legge waypoint attuali
// Segnali
connect(map, &WorldMapWidget::waypointAdded, this, [](int idx, double lat, double lon){});
// Decoder polyline Google Encoded (usato da OSRM):
//   decodePolyline(QByteArray encoded) → QVector<QPair<double,double>>
// OSRM endpoint: https://router.project-osrm.org/route/v1/{driving|foot|bike}/{lon,lat;...}
//   ?overview=full&geometries=polyline
// Disegno: drawWaypoints (A=verde/B=rosso/tappe=arancio) + drawRouteLine (blu 3.5px)
```

## Vision3D — sotto-schede e modalità Oggetto/Scena (`pages/widget_vision3d.h/cpp`)

**UI desktop a due sotto-schede** (`QTabWidget` in `buildUi()`), perché prima
va fatta bene la preparazione e solo dopo si assembla:
- **"\xf0\x9f\x93\xb7 Preparazione"**: dentro, layout **a due colonne**
  (`colA`/`colB`, `QHBoxLayout` con due `QVBoxLayout` figli dentro il
  vecchio `leftLay`, che ora è orizzontale) per ridurre lo scroll verticale.
  Colonna A (setup, fatto una volta): Connessione (QR `m_prepQr`, aggiornato
  da `start()`/`stop()`), stampa bersagli ArUco, Sessione (combo
  `m_prepSessionCombo` + "\xe2\x9e\x95 Nuova"), Device attivi. Colonna B (roba che
  cresce durante lo scatto): Galleria scatti (+ "\xf0\x9f\x93\xa5 Importa foto" /
  "\xf0\x9f\x97\x91 Elimina selezionate" / "\xf0\x9f\x97\x91\xf0\x9f\x97\x91 Elimina tutte"), Ultimo scatto
  analizzato — vedi sotto.
- **"\xf0\x9f\xa7\xb1 Assembla punti e texture"**: Scena 3D (`m_scene`) + posa manuale +
  Ricostruzione COLMAP (`m_sessionCombo`/`m_qualityCombo`/`m_reconBtn`).
- Interfaccia/porta/Avvia server/VLM restano fuori dalle tab (globali).
  **Nessun pannello "Log:" locale** (rimosso, vedi sotto).

**Ultimo scatto analizzato — 6 mappe + salvataggio**: la griglia
(`QGridLayout` in `prevBox`) mostra ora 6 miniature (`makeVision3dThumb()`,
160x160): Originale/`m_lastThumb`, Box (OpenCV)/`m_lastBoxes`,
Depth/`m_lastDepth`, Bordi/`m_lastEdges`, Bump/`m_lastBump`,
Normal/`m_lastNormal`, seguite da descrizione VLM e `m_lastSensorInfo`.
Le tre nuove mappe sono calcolate in `Vision3DWidget::edgeMap()` (Canny su
grayscale sfocato), `bumpMap()` (equalizzazione CLAHE su grayscale) e
`normalMap()` (gradienti Sobel X/Y → normale per pixel, codifica
tangent-space in BGR), tutte guardate da `VISION3D_USE_OPENCV` col solito
fallback "serve OpenCV" quando assente. Salvate su disco accanto alla foto
come `<base>_edges.jpg`/`_bump.jpg`/`_normal.jpg` da `analyze()`, che ora
accetta un solo parametro `const Vision3DWants& wants` (struct con
`desc/boxes/depth/edges/bump/normal`) invece di tre `bool` separati, per non
far crescere all'infinito la firma. La risposta JSON di `/upload` include
`edges_image`/`bump_image`/`normal_image` (o `*_unavailable:true` se
richieste ma non calcolabili, stesso pattern di `depth_unavailable`).
**Salvataggio**: tasto destro su una qualunque delle 6 miniature
(`Qt::CustomContextMenu` + `onThumbContextMenu()`) salva quella singola
immagine via `QFileDialog::getSaveFileName`; il pulsante
"\xf0\x9f\x92\xbe Salva tutte le mappe..." (`onSaveAllMapsClicked()`) copia in una
cartella a scelta tutti i file `<base>*.jpg` esistenti per lo scatto
selezionato (`m_selectedShotKey`) — non ricalcola nulla, si limita a
copiare quello che `analyze()` ha già scritto su disco. `deleteShot()`
rimuove anche `_edges.jpg`/`_bump.jpg`/`_normal.jpg` (bug: mancavano quando
sono state introdotte le 3 mappe, uno scatto cancellato le lasciava
orfane su disco).

**Doppio click = calcola SUBITO quella mappa**: le 5 miniature di analisi
(non "Originale", niente da ricalcolare) hanno un `eventFilter()` installato
da `buildUi()` (`thumb->installEventFilter(this)`) perché `QLabel` non ha un
segnale `doubleClicked` nativo — `Vision3DWidget::eventFilter()` intercetta
`QEvent::MouseButtonDblClick`, riconosce quale miniatura è stata cliccata
per identità di puntatore (`watched == m_lastEdges` ecc.) e chiama
`recomputeThumb()`: legge `<base>.jpg` da disco, richiama la funzione giusta
(`detectBoxes()`/`depthMap()`/`edgeMap()`/`bumpMap()`/`normalMap()`), salva
il risultato con lo stesso suffisso usato da `analyze()` e aggiorna solo
quella miniatura. Serve perché bordi/bump/normal sono chip OFF di default
sul telefono (e box/depth potrebbero non essere state richieste): niente
bisogno di rifare la foto per riavere una mappa mancante. I 6 `QLabel` hanno
ora un `objectName` (`v3dThumbOriginal`/`v3dThumbBoxes`/`v3dThumbDepth`/
`v3dThumbEdges`/`v3dThumbBump`/`v3dThumbNormal`), aggiunto anche per
poterli trovare nei test con `findChild<QLabel*>(nome)`.

**Eliminazione device**: tasto destro su una riga di "Device attivi"
(`Qt::CustomContextMenu` + `onDeviceTableContextMenu()`) elimina quel
device — rimuove `scan_output/<sessione>/<deviceId>/` intera (foto + tutte
le mappe + sidecar), lo toglie da `m_devices`, aggiorna la tabella e
ricarica la galleria. Utile per pulire un telefono di test o riconnesso con
un nuovo ID, senza eliminare le foto in galleria una per una.

**Conferma eliminazione con Invio**: i due dialoghi di conferma
("Elimina foto"/"Elimina tutte le foto", `QMessageBox::question`) hanno ora
`QMessageBox::Yes` come pulsante di default invece di `No` — chi ha già
premuto "Elimina" ha già deciso, il dialogo è solo l'ultima conferma e con
Invio si conclude senza dover puntare il mouse su "Sì".

**"Salva sul PC" in galleria**: tasto destro su uno o più scatti
selezionati (`m_gallery`, stessa `ExtendedSelection` di "Elimina
selezionate") → `onGalleryContextMenu()` mostra "\xF0\x9F\x92\xBE Salva sul PC"
(con conteggio se multi-selezione) e copia la foto originale (`<base>.jpg`)
di ognuno in una cartella a scelta, con suffisso `_N` automatico se il nome
esiste già a destinazione. Click destro su un elemento FUORI dalla
selezione corrente lo seleziona da solo prima di aprire il menu (comportamento
da file manager, non opera sulla selezione multipla precedente per errore).

**"Bordi" (edges) — indagine e correzione**: segnalato "non funziona".
Causa reale più probabile: il chip "Bordi" sul telefono era **spento per
default** (aggiunto OFF nella sessione dei 6 formati mappa, a differenza di
Box/Depth che sono ON) — ora `class="chip on" data-w="edges"` e
`wants.edges` partono `true`. **Falsa pista scartata**: avevo inizialmente
sospettato le soglie Canny fisse (50/150, le stesse di `detectBoxes()`) e
provato soglie "adattive" (mediana dell'istogramma ±33%) pensando che una
foto reale a basso contrasto potesse restare sotto soglia — verificato
EMPIRICAMENTE con OpenCV in standalone Python prima di committare: su
un'immagine a due toni con lo sfondo che occupa la maggioranza dei pixel
(caso realistico: oggetto piccolo su un tavolo), la mediana rispecchia lo
sfondo e alza le soglie troppo, **peggiorando** il risultato (0 bordi
rilevati contro 396 con le soglie fisse originali sullo stesso test) —
scartata, `edgeMap()` è tornata alle soglie fisse 50/150. Lezione: soglie
"adattive" basate sulla luminosità dei pixel non hanno relazione diretta
con la magnitudine del gradiente che Canny confronta davvero: serve
statistica sul gradiente (Sobel), non sui pixel grezzi, se mai si vorrà
riprovare. Test di non-regressione aggiunto
(`uploadWithEdgesDetectsModerateContrastEdge`) su un'immagine a contrasto
moderato (non il quadrato nero-su-bianco ad alto contrasto usato altrove),
verifica che l'immagine `edges_image` risultante contenga davvero pixel
chiari (bordi rilevati), non solo che il campo sia presente.

**"1. 2. 3." nella card Connessione ora vanno a capo**: `connHint`
contiene tag HTML (`<b>`/`<i>`), quindi Qt lo rende in **rich text**
(`Qt::mightBeRichText()` lo rileva dai tag) — in rich text un `\n` letterale
NON crea una riga nuova (viene collassato a spazio come farebbe un
browser), a differenza del plain text dove `\n` è onorato. Serviva `<br>`
esplicito fra i tre punti: bug non ovvio perché lo stesso codice, senza tag
HTML, si sarebbe comportato diversamente.

**Descrizione/metadati — sessione e scatto, entrambi editabili**:
- **Sessione**: `createSession(rawName, description = "")` (nuovo parametro
  opzionale, retrocompatibile) persiste subito la descrizione in
  `scan_output/<session>/session.json` (`description`/`created`/`updated`).
  API pubbliche `sessionDescription(session) const` / `setSessionDescription
  (session, description)` per leggerla/cambiarla in qualsiasi momento, non
  solo alla creazione. `onNewSessionClicked()` ora chiede anche una
  descrizione opzionale (secondo `QInputDialog::getMultiLineText`, Annulla =
  salta solo quella, non l'intera creazione). Nuovo pulsante "✏️" accanto a
  "➕ Nuova" (`onEditSessionDescriptionClicked()`) per modificarla dopo.
  Mostrata come **tooltip** sui due combo sessione (`m_prepSessionCombo`/
  `m_sessionCombo`) invece di un pannello dedicato — `refreshSessionDescription
  Tooltip()` appende la descrizione ai tooltip statici di base (costanti
  `kPrepSessionComboTip`/`kSessionComboTip`, condivise fra `buildUi()` e
  questa funzione perché altrimenti il testo esplicativo andrebbe perso ad
  ogni refresh), chiamata da `onSessionComboChanged()`, dal costruttore e
  dai due slot di modifica/creazione.
- **Scatto**: **bug reale scoperto e corretto** — la descrizione VLM non è
  MAI stata persistita sul PC prima d'ora: `analyze()` scrive il sidecar
  `.json` PRIMA di chiamare `vlmDescribe()` (serve la chiamata di rete per
  saperla), quindi un `meta["description"]` lì dentro sarebbe sempre stato
  vuoto; il valore arrivava solo nella risposta HTTP al telefono e spariva.
  Fix: dopo `r.description = vlmDescribe(jpeg)`, read-modify-write del
  sidecar già su disco per aggiungerci `description` (solo se non vuota).
  `loadSessionIntoUi()` ora legge `description` dal sidecar invece di
  scrivere sempre `QString()` nella cache di galleria (`Qt::UserRole+1`,
  prima commentata esplicitamente "descrizione VLM non persistita").
  Editabile a mano con `onEditShotDescriptionClicked()` — nuovo pulsante
  "✏️" accanto a `m_lastDesc` (avvolti insieme in un `descRow` QHBoxLayout,
  al posto del solo QLabel nella grid), sostituisce/integra quella del VLM,
  salvata nello stesso campo `description` del sidecar + aggiornata anche
  nella cache di galleria per restare coerente senza dover ricaricare la
  sessione da disco.

**Test riflessione col flash**: chip "\xf0\x9f\x94\xa5 Test flash a ogni scatto" lato
telefono (card "Test riflessione superficie"); al primo `on` chiede conferma
con `confirm()` (spiega cosa fa), non ad ogni scatto successivo per non
essere fastidioso su una sessione lunga. In `doShoot()`, `tryTorch(on)`
usa `track.getCapabilities().torch` + `track.applyConstraints({advanced:
[{torch:on}]})`: se il device/browser non espone la torcia avvisa una sola
volta (`flashWarned`) invece di interrompere ogni scatto. Il campo `flash`
(bool) viaggia nel body di `/upload` come gli altri sensori extra
(`kExtraKeys`/`kExtraJsonKeys` → sidecar `flash_on`), e viene riecheggiato
nella risposta come `flash_used` per mostrare un avviso nel telefono. Non
calcola automaticamente riflettente/trasparente/rifrangente: è solo un
aiuto visivo, il confronto tocca all'utente (scatta con e senza).

**Modalità Oggetto/Scena** (chip `#modeChips` nella pagina HTML del telefono,
prima della "Cosa calcolare"): scelta PRIMA di scattare, di default "object".
- `object` — l'oggetto resta fermo al centro, il telefono orbita intorno.
- `scene` — il telefono resta fermo in un punto, si ruota su se stessi
  (panoramica/paesaggio).
- Il valore (`scanMode` in JS) va nel body di `/upload` come `mode`, letto in
  `analyze()` (parametro opzionale, default `"object"`) e persistito in ogni
  sidecar `.json` come `scan_mode` — dato per un'eventuale logica di
  ricostruzione futura differenziata, non ancora usato per cambiare il
  calcolo di COLMAP.

**Log centralizzato, non più locale**: `Vision3DWidget::appendLog()` non
scrive più in un `QPlainTextEdit` proprio (rimosso, con relativo `m_log`) —
chiama `LogBus::post(s, "3d")`. `LogBus` (`log_bus.h`) ora accetta una
categoria opzionale (`post(msg, category = "")`); `MainWindow` (connessione
in `mainwindow.cpp`) mappa `category=="3d"` (case-insensitive) alla nuova
tab **"\xf0\x9f\xa7\xa9 3D"** del dialog Messaggi (`MainWindow::Log3D`,
`m_logView3D` — vedi `mainwindow_settings.cpp::ensureLogDialog()`), le altre
categorie/vuota restano sulla tab "Sistema" com'era prima. Il segnale
`Vision3DWidget::logMessage(line)` resta emesso (con timestamp locale) per
eventuali embedder esterni, indipendente da LogBus.

**Sensori extra salvati ad ogni scatto** (sidecar `.json`, oltre a
`heading_deg`/`pitch_deg`/`roll_deg` già esistenti): accelerometro e
giroscopio grezzi via `DeviceMotionEvent` (`accel`, `accel_gravity`,
`rotation_rate`, ognuno `{x,y,z}` o `{alpha,beta,gamma}`) + altitudine via
Geolocation GPS (`altitude_m`, `altitude_accuracy_m`, `latitude`,
`longitude` — quota sul livello del mare, **non** altezza dal suolo locale:
il browser non ha un sensore per quello). Tutto best-effort: permesso
negato/non disponibile → campo assente o `null`, non blocca lo scatto. Lato
JS: `onMotion()`/`onGeoSuccess()` aggiornano variabili globali, l'angolo E
i sensori vengono "congelati" in variabili locali (`shotAccel` ecc.)
all'inizio di `doShoot()` per lo stesso motivo di `shotHeading` (vedi sopra
il bug fix su `markCovered`). Lato C++: `Vision3DWidget::analyze()` accetta
un `QJsonObject extraSensors` opzionale, le cui chiavi vengono copiate
così come sono nel sidecar — l'handler `/upload` in `handleRequest()`
costruisce `extraSensors` solo con le chiavi presenti nel body (vedi
`kExtraKeys`/`kExtraJsonKeys`).

**Sensori sempre attivi con la fotocamera** (non più un pulsante opzionale
da ricordarsi di premere): `startCamera()` chiama `enableSensors()` **prima**
dell'`await` su `getUserMedia()`, nello stesso gesto utente del click su
"Attiva fotocamera" — importante per iOS/Safari, che lega il permesso
sensori al gesto originale: chiederlo dopo un altro `await` rischia di farlo
cadere silenziosamente. `enableSensors()` è ora idempotente (`if(sensorOn)
return;` in testa), perché viene richiamata ad ogni avvio/flip camera senza
il rischio di registrare i listener due volte. `stopCamera()` continua a
chiamare `disableSensors()` com'era già (senza fotocamera i sensori non
servono). Il pulsante `#enableSensor` resta visibile come **retry manuale**
per il solo caso in cui l'attivazione automatica fallisca (permesso negato,
browser che blocca i sensori — vedi `sensorBlockedHint()`).

**Sfera-guida sul telefono** (canvas `#targetRing`, overlay a tutto schermo
sopra `<video>`, non un badge in un angolo): `buildTargets()` genera 1-3
anelli (`targetRings`, in base a `targetQuality` low/medium/high) di
bersagli equidistanti per bussola; `drawTargetSphere()` li disegna come
ellissi piatte impilate verticalmente, l'anello "attivo" (`activeRingIndex()`,
in base a `curPitch`) evidenziato, la lancetta (`curHeading`) verde quando
allineata a un bersaglio scoperto (`nearestUncoveredDiff()` ≤ 8°) altrimenti
blu. L'istruzione "ruota di N°" / "SCATTA ORA!" è disegnata **dentro** il
canvas stesso (non solo nel div `#rotateHint` sotto, che mentre si guarda il
mirino non si vede). La rotazione del disegno segue il rollio reale
(`curRoll`, ±45°) — niente animazione automatica. `markCovered()` usa
sempre l'angolo "congelato" al momento dello scatto, mai `curHeading` letto
dopo l'attesa della risposta del PC (bug reale corretto: senza congelare,
un telefono che continua a ruotare durante l'analisi VLM/depth marcava
coperto il pallino sbagliato).

**Puntini numerati** (per chi non capisce cosa fare guardando solo cerchio +
pallini): ogni bersaglio dell'anello attivo mostra il proprio numero
1-based (`i+1`, alone nero + colore stato con `strokeText`/`fillText`),
calcolato direttamente dall'indice in `ring.headings[]` — il punto 1 è
`t.deg===0`, che con `PHASE_DEG=0` è già il punto "in basso" verso chi tiene
il telefono (stesso riferimento della freccia nello screenshot che ha
motivato quel fix). `nearestUncoveredTarget()` (nuova, wrappa la vecchia
`nearestUncoveredDiff()` che resta per compatibilità) restituisce sia la
differenza in gradi sia l'indice 1-based del bersaglio più vicino ancora
scoperto: usato sia nel testo grande dentro il canvas ("↻ punto 7: ruota di
23°" / "✅ SCATTA ORA! (punto 7)") sia in `updateRotateHint()`
(`#rotateHint`), così lo stesso numero è coerente ovunque compaia.

**Scatto automatico** (`autoShoot`/`checkAutoShoot()`, pulsante `#autoShoot`,
richiede Sensori attivi): scatta da sola quando ci si allinea con un
bersaglio scoperto sull'anello attivo, con isteresi (`tolFire` stretta per
scattare, `tolRearm` larga per riarmarsi) per non scattare a raffica sullo
stesso punto.

**Ferma fotocamera = ferma anche i sensori** (`stopCamera()` →
`disableSensors()`): prima fermare la fotocamera lasciava giroscopio,
accelerometro e GPS accesi in background (bug reale: il pulsante diceva
"Attiva fotocamera" ma orientamento/Auto ON continuavano ad aggiornarsi).
`disableSensors()` rimuove i listener `deviceorientation(absolute)`/
`devicemotion`, interrompe il GPS (`clearWatch(geoWatchId)`), azzera tutte
le variabili sensore e spegne l'Auto ON.

**Oggetto/Scena cambia davvero la geometria della sfera-guida**, non solo
l'etichetta: `buildTargets()` forza `nRings=1` (un solo anello, giro
d'orizzonte a 360°) quando `scanMode==='scene'` — non ha senso orbitare in
quota intorno a un paesaggio. In `object` resta il comportamento a 1-3
anelli per qualità (low/medium/high). Il testo di `#targetHint` viene
riscritto dinamicamente da `buildTargets()` in base al modo; cambiare chip
Oggetto/Scena richiama `buildTargets()` e ricostruisce i bersagli sul
momento (`scanMode=c.dataset.m; buildTargets();`).

**I dati sensore extra sono visibili nella UI desktop**, non solo salvati
su disco: `v3dSensorInfoHtml(QJsonObject)` (funzione statica in
`widget_vision3d.cpp`) riassume modalità/quota GPS/accelerometro in una
riga, mostrata nel nuovo `QLabel* m_lastSensorInfo`
(objectName `v3dLastSensorInfo`, sotto la descrizione VLM nel box "Ultimo
scatto analizzato" — tab Preparazione). Popolato in due punti: subito dopo
ogni upload dentro `analyze()` (da `scanMode`+`extraSensors` già in
memoria, nessuna rilettura da disco) e in `onGalleryItemClicked()`
rileggendo il sidecar `.json` dello scatto selezionato in galleria.

**Sessioni multiple, nominabili, con import/elimina foto** — tre metodi
**pubblici** (non solo slot privati dietro ai dialog UI, così restano
programmabili/testabili direttamente, come da convenzione già usata per
`setOutputDir()` ecc.):
- `QString createSession(const QString& rawName)` — sanifica il nome
  (solo lettere/numeri/`-`/`_`, es. "Villa @ Mare!!" → `VillaMare`) e crea
  `scan_output/<nome>/` vuota; "" se non restava nulla di valido. Chiamato
  da `onNewSessionClicked()` (bottone "\xe2\x9e\x95 Nuova" in Preparazione, prompt
  `QInputDialog`) — usa lo stesso nome anche nel campo sessione del
  telefono per unire gli scatti.
- `int importPhotoFiles(const QString& session, const QStringList& files)`
  — copia foto già esistenti su disco in
  `scan_output/<session>/import/<session>_import_NNN_a000.jpg`, stessa
  numerazione `*_a???.jpg` degli scatti da telefono (nessuna modifica al
  parsing di `loadSessionIntoUi()`), sidecar `.json` con `has_sensors:
  false` + `imported: true` (angolo indicativo 0° — poi "\xe2\x86\x94 Distribuisci
  sul cerchio" per spaziarle). Ritorna quante immagini sono state
  importate con successo (le non decodificabili vengono saltate, non
  contano). Chiamato da `onImportPhotosClicked()` (`QFileDialog::getOpenFileNames`).
- `void deleteShot(const QString& base)` — rimuove `.jpg`/`_boxes.jpg`/
  `_depth.jpg`/`.json` di uno scatto (`base` = stesso percorso-senza-estensione
  usato in `Qt::UserRole` della galleria). Chiamato da
  `onDeletePhotoClicked()` previa conferma `QMessageBox`.

**Due combo sessione sincronizzati**: `m_sessionCombo` (tab Assembla,
autorevole — unico a scatenare `loadSessionIntoUi()`) e
`m_prepSessionCombo` (tab Preparazione, nuovo). `populateSessions()` li
popola entrambi dallo stesso elenco; `onSessionComboChanged()` rispecchia
la selezione su `m_prepSessionCombo` (bloccato per non ri-scatenare),
`onPrepSessionComboChanged()` fa l'inverso spostando la selezione su
`m_sessionCombo` (che a quel punto scatena il caricamento) — un solo
punto di verità per il caricamento, nessun doppio `loadSessionIntoUi()`.

**Galleria: selezione multipla + tasto Canc + elimina tutte**: `m_gallery`
usa `QAbstractItemView::ExtendedSelection` (Ctrl/Shift+click, come un file
manager) invece di `SingleSelection`. `onDeletePhotoClicked()` ora itera
`m_gallery->selectedItems()` (non più solo `currentItem()`), una sola
`QMessageBox` di conferma col conteggio. Un `QShortcut(QKeySequence::Delete,
m_gallery)` con `Qt::WidgetWithChildrenShortcut` richiama lo stesso slot —
tasto Canc con la galleria attiva, non solo il pulsante. Nuovo
`onDeleteAllPhotosClicked()` (pulsante "\xf0\x9f\x97\x91\xf0\x9f\x97\x91 Elimina tutte") itera
tutti gli item della sessione corrente.

**Combo VLM popolato subito, non solo ad "Avvia server"**: prima
`fetchVlmModels()` partiva solo dentro `start()`, quindi il combo restava
con un solo elemento (il default scritto a mano) finché non si avviava
tutto il server — sembrava vuoto/non funzionante anche con Ollama già
acceso. Ora `fetchVlmModels()` è chiamato anche nel costruttore.
`onVlmTagsReady()` inoltre:
- ordina i modelli **vision-capable prima** (`v3dLooksLikeVisionModel()`,
  stessi indizi di nome di prima: moondream/-vl/llava/vision/minicpm-v/
  bakllava — ora funzione statica condivisa, non più duplicata inline);
- imposta un tooltip per voce (`Qt::ToolTipRole`): "\xf0\x9f\x91\x81 Vision — legge
  davvero l'immagine" vs "\xe2\x9a\xa0 Solo testo — risponde ma IGNORA l'immagine" —
  **non** modifica il testo/nome del modello (altrimenti la chiamata
  Ollama userebbe un nome corrotto);
- su errore di rete o Ollama senza modelli installati, `appendLog()` un
  messaggio esplicito invece di lasciare il combo silenziosamente com'era
  (dubbio legittimo: "il VLM è davvero attivo?").

## Dev Agent (`MCPs/devagent_mcp/server.py` + `main_app_controller.*`)
Layout tab Dev Agent [AppController → 10]:
- **Colonna sinistra 65%** (QSplitter H): Log agente step-by-step + Diff generato
- **Colonna destra 35%** (QScrollArea): Cronologia snapshot + Ripristina da Git/GitHub

Comandi IPC Python (stdin JSON newline):
```json
{"cmd": "list_history"}                          // → event: history_list
{"cmd": "restore", "backup_id": "..."}           // → event: restore_done
{"cmd": "git_log", "project_root": "...", "n": 25}        // → event: git_log
{"cmd": "git_restore", "project_root":"...", "commit":"...", "files":[]}  // → reset hard
{"cmd": "git_fetch_reset", "project_root":"...", "branch":"master"}       // → fetch+reset
{"cmd": "git_stash_push", "project_root":"...", "message":"..."}          // → stash push
{"cmd": "git_stash_list", "project_root":"..."}  // → event: git_stash_list
{"cmd": "git_stash_pop",  "project_root":"...", "ref":"stash@{0}"}        // → stash pop
```
Slot Qt: `onDevAgentGitLogClicked`, `onDevAgentGitRestoreClicked`, `onDevAgentGitFetchResetClicked`,
`onDevAgentGitStashPushClicked`, `onDevAgentGitStashListClicked`, `onDevAgentGitStashPopClicked`

## Analisi 1/2 (Matematica)
- Pulsante **"Risolvi nella scheda accanto"** — copia la formula in "Risolvi Passi" e cambia tab
- Pulsante **"Disegna grafico"** — traccia f(x) / f(x,y) nel GraficoCanvas a destra dello splitter
- Sezione vision AI ("Grafico → Formula") posizionata SOTTO i pulsanti Traccia/Reset vista (fuori QScrollArea)

## BikeWidget (`pages/widget_bike.h/cpp`)
Widget in UtilityPage [5] → tab "🚴 Bici". Selettori: tipo bici (5 opzioni) + sezione (5 categorie).
Categorie:
- **Problemi comuni** — catena, gomme, freni + sezioni specifiche per tipo bici (pieghevole: snodi; MTB: ammortizzatori; Gravel: tubeless; City: dinamo/cambio interno)
- **Regolazione cambio** — procedura vite H/L + barrel adjuster, cambio anteriore, trucchi (Loctite ghiera, Di2)
- **Regolazione freni** — dischi idraulici (spurgo, centraggio pinza) + V-brake/cantilever
- **Manopole e attacchi** — lock-on/slip-on, stem threadless/filettato, leve che ruotano, sella che scende
- **Manutenzione periodica** — checklist ogni uscita/settimana/mese/anno + kit base indispensabile

## Suite di Test
```bash
# Percorso canonico — usa SEMPRE gui/build_tests (non Test/build_tests che è obsoleto)
cmake -B gui/build_tests gui/ -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release && cmake --build gui/build_tests -j$(nproc)
ctest --test-dir gui/build_tests --exclude-regex "AiIntegration|AiStress|TeamCollab|MultiAgenteLive|AgenteAutonomoLive" -j4 --output-on-failure
# Oppure con aggiorna.sh:
./aggiorna.sh --test
```
**NOTA**: `Test/build_tests/` è deprecato — punta a codice obsoleto. Eliminarlo se presente.

### Suite per categoria

| Suite | Target ctest | Note |
|-------|-------------|------|
| `GraphMemory` | `test_graph_memory` | 65 test: nodi, archi BFS, ricerca, export, prune, changed(), SQL injection |
| `SignalLifetime` | `test_signal_lifetime` | dangling observer, leakage, invariant violation |
| `RagEngine` | `test_rag_engine` | JLT 256-dim, chunk, save/load |
| `RagEngineAvanzato` | `test_rag_engine_avanzato` | edge search, performance |
| `StrumentiRag` | `test_strumenti_rag` | extractText, sysPromptForAction |
| `MatematicaPage` | `test_matematica_page` | parseSeq, detectPattern, **CAT-E** 15 test SymPy reali |
| `FormulaParser` | `test_formula_parser` | eval, sample, tryExtract |
| `SimulatoreAlgos` | `test_simulatore_algos` | BubbleSort, ricerca, BigO — `RESOURCE_LOCK cpu_heavy` |
| `CodeUtils` | `test_code_utils` | extractPythonCode, sanitizePyCode |
| `RandomTool` | `test_random_tool` | distribuzioni, seed |
| `ChatHistory` | `test_chat_history` | save/load, remove, robustezza |
| `ChatHistoryStress` | `test_chat_history_stress` | concorrenza 4 thread, durabilità |
| `LavoroPage` | `test_lavoro_page` | filtri, modelli, database — `RESOURCE_LOCK cpu_heavy` |
| `LavoroData` | `test_lavoro_data` | offerte, filtrate, icone |
| `TutorData` | `test_tutor_data` | invarianti, unicità, semantica |
| `AppController` | `test_app_controller` | state machine, routing, signal isolation |
| `McpManager` | `test_mcp_manager` | 8 test — venv path, smoke test JSON-RPC parsing, scan MCPs/, costruzione widget |
| `ProgrammazionePage` | `test_programmazione_page` | isIntentionalError, parseNumbers |
| `ThinkingDetect` | `test_thinking_detect` | extractName/Size/Prio, classifyQuery, keyLock |
| `AgentiPipeline` | `test_agenti_pipeline` | buildBubble, markdownToHtml; CAT-E `guardiaDataOra`; CAT-F `_inject_help` (segnaposto `{{PROVA:cmd}}` → link `prova:BASE64`); CAT-G ogni esempio della tabella zero-token DEVE rispondere in locale; CAT-H `_parseEventoRequest` (titolo/data/orari/luogo, QR evento zero-LLM); CAT-I `_icsEscapeText` (RFC 5545 §3.3.11: virgola/punto-e-virgola/newline/backslash, ordine backslash-prima); CAT-J `_buildGoogleCalendarIntentUrl` (struttura `intent://...#Intent;scheme=https;package=com.google.android.calendar;...;end`, fallback percent-decodificato torna uguale all'URL https originale); CAT-K (D-16) 7 nuove domande zero-LLM (statistica/Pasqua/fase lunare/base N/progressioni/lira-euro/scadenza), valori verificati con Python indipendente; CAT-L copertura T-D14a÷T-D14d/T-D17 (TODO.md) chiamando le guardie direttamente — 4 bug reali trovati e corretti durante la verifica (giorni lavorativi date-swallowing, Fibonacci ordinale non riconosciuto, posizione-array UTF-8 rotto in raw string, `genCountInversions` doppio conteggio); CAT-M copertura T-help — tabella markdown di `_inject_help()` renderizza davvero come `<table>` HTML (non pipe grezze) via `markdownToHtml()`, esempio di grafico verificato cambiare su 30 chiamate consecutive |
| `AgentsConfigDialog` | `test_agents_config_dialog` | struttura, numAgents, RAG condiviso |
| `AgentiByzantine` | `test_agenti_byzantine` | voce combo, num agenti, mock stub |
| `AgenteAutonomo` | `test_agente_autonomo` | ReAct loop, toggle UI, parsing tool call |
| `LanServer` | `test_lan_server` | lifecycle TCP, token, rate limit |
| `LanWanCore` | `test_lan_wan_core` | timingSafeEqual, token LAN, rate limit, lifecycle; CAT-E `LanWanPage` rubrica persone (`m_accessListTable` round-trip QSettings `lan/accessList`, addRow, remove, persistenza tra istanze); CAT-F (T-D19/D-19) fallback TLS→HTTP: HOME/PATH sabotati → `start()` reale in HTTP con `isTlsRequested()!=isTlsEnabled()`, label ambra "TLS non disponibile" via slot reale `onLanServerStatusChanged(true)` |
| `LanServerEndpoints` | `test_lan_server_endpoints` | /knowledge (GET/POST), /apk, requestHandled signal |
| `Onboarding` | `test_onboarding` | QSettings, token LAN, rate limiter |
| `ImpostazioniPage` | `test_impostazioni_page` | AiChatParams round-trip, ThinkMode, preset; CAT-E navigazione lazy a due livelli (LazyTabLoader) |
| `ThemeManager` | `test_theme_manager` | lista temi, ops |
| `ThemeManagerCrash` | `test_theme_manager_crash` | fix ABRT Signal 6 (parent=nullptr), lifetime singleton |
| `Grafico` | `test_grafico` | canvas, formula parser integrazione |
| `HardwareMonitor` | `test_hardware_monitor` | rilevamento CPU/RAM/GPU, thread |
| `HwDetectAmd` | `test_hw_detect_amd` | AMD via DRM sysfs |
| `MonitorPanel` | `test_monitor_panel` | struttura, aggiornamento HW |
| `ManutenzioneeCron` | `test_manutenzione_cron` | cronShouldRun, nextRun, detectConfigFmt |
| `ManutenzioneeBugs` | `test_manutenzione_bugs` | costruzione, bugtracker widget |
| `KnowledgeInjection` | `test_knowledge_injection` | readKnowledge, prependKnowledge, cache 30s |
| `OpenCodePage` | `test_opencode_page` | costruzione, struttura, stato iniziale |
| `ImparaQuiz` | `test_impara_quiz` | ImparaPage, QuizPage, MateriePage |
| `QrCodeWidget` | `test_qr_code_widget` | generazione, rendering, lifetime |
| `Sintetizzatore` | `test_sintetizzatore` | Tono, OscoCanvas, widget |
| `SttWhisper` | `test_stt_whisper` | paths, availability, permissions, mic; CAT-I `SttDaemon` protocollo con demone stub (ready/transcribe/errore/stop) |
| `SttWhisperLive` | `test_stt_whisper_live` | 21 PASS, 1 SKIP — paths, ImpostazioniPage voce, transcribe() contract, savePreferredModel round-trip |
| `AiIntegration` | `test_ai_integration` | ⚠️ Ollama reale — classifyQuery, chat, params |
| `AiStress` | `test_ai_stress` | ⚠️ Ollama reale — sequential, param matrix |
| `TeamCollab` | `test_team_collab` | ⚠️ Ollama reale — pipeline, quality |
| `PraticoFinanza` | `test_pratico_finanza` | 27 PASS — normalizzaComune, cercaBelfiore, calcolaCodiceFiscale (D.M.1976) |
| `WanComputeTasks` | `test_wan_compute_tasks` | 33 PASS — costruzione, TCP, JSON, sicurezza shell |
| `SciCompute` | `test_sci_compute` | 35 PASS — costruzione, isSafeToolName (9), isSafePath (8), taskTypes invarianti (6) |
| `RagGraphPipeline` | `test_rag_graph_pipeline` | 25 PASS — costruzione, parsing LLM mock, ricerca, segnali |
| `Translitter` | `test_translitter` | 37 PASS — langFence helper, widget, kLangs unicità |
| `DockerSandbox` | `test_docker_sandbox` | 29 PASS, 2 SKIP — PythonExec, Docker, sicurezza |
| `FileParser` | `test_file_parser` | 38 PASS — StrumentiFilePage, pdftotext, CSV, file non validi |
| `ReplPython` | `test_repl_python` | 23 PASS, 1 SKIP — costruzione, python3 I/O async, robustezza |
| `Astrale` | `test_astrale` | 31 PASS — RicercaPage, NatalChartWidget, AstroCalc::compute() |
| `BlhmRab0l` | `test_blhm_rab0l` | 38 PASS — Rab0lCanvas, BLHM Engine C, UI BLHM/RAB₀-L |
| `Gns3Mcp` | `test_gns3_mcp` | 18 PASS, 2 SKIP — costruzione, azioni combo (GNS3 non avviato) |
| `MultiAgenteLive` | `test_multi_agente_live` | 7 PASS, 6 SKIP — CAT-A costruzione; ⚠️ CAT-B/C/D Ollama |
| `AgenteAutonomoLive` | `test_agente_autonomo_live` | ⚠️ Ollama reale + modello tool-capable — E2E modalità Agentico 👔: ReAct con TOOL_CALL calcolatrice (37*43→1591 dal tool locale) + FINAL_ANSWER; LENTA (minuti), `PRISMALUX_TEST_MODEL` per override |
| `ChatLive` | `test_chat_live` | ⚠️ Ollama reale — E2E Chat 💬 via UI: guardie locali zero-LLM (math+ora, `!m_ai->busy()`), risposta streamed con nonce casuale, memoria conversazione 2 turni (m_chatPairs); LENTA, `PRISMALUX_TEST_MODEL` per override |
| `DepCheckPanel` | `test_dep_check_panel` | 19 PASS — costruzione, righe per dep, `runAllChecks()`/segnali `allOk()`/`someMissing(int)` |
| `PerceptorScripts` | `test_perceptor_scripts` | 15 PASS — CAT-C `speaker_diarize.py` (8, fixture WAV via `espeak-ng`, JSON, CUDA_VISIBLE_DEVICES, `--transcript`); CAT-D `fast_whisper_transcribe.py` (7, modello tiny, CPU forzata via env, errori) |
| `StreamlinkMcp` | `test_streamlink_mcp` | 18 PASS — CAT-A protocollo JSON-RPC 2.0 (`MCPs/streamlink_mcp/server.py`: tools/list, errori -32601/-32700); CAT-B `_validate_url()` SSRF (RFC1918, schema non-http, URL pubblico consentito) |
| `AiClient` | `test_ai_client` | 68 PASS — CAT-A classifyQuery, CAT-B detectQueryDomain, CAT-B2 `detectQueryIsEnglish` (D-28), CAT-B3 `dateTimeDirective` (D-31), CAT-B4 `scrubPii` (D-32), CAT-C SmartRouter API, CAT-D abort()/aborted(), CAT-E mock HTTP 4xx/5xx |
| `AiMath` | `test_ai_math` | 23 PASS — `normalizeItFormats()` (D-24): CAT-A virgola decimale, CAT-B liste non toccate, CAT-C punto migliaia, CAT-D guardia IPv4 (192.168.x.x mai mutilato), CAT-E date con mese in lettere, CAT-F nessuna regressione |
| `AiKnowledgeLookup` | `test_ai_knowledge_lookup` | 24 PASS — `_knowledgeLookup()` (D-26): CAT-A match univoco, CAT-B nessun trigger di domanda, CAT-C troppo generico, CAT-D match ambiguo→vuoto, CAT-E nessun match/knowledge vuoto, CAT-F intestazioni escluse/`]` protetto/query lunga |
| `AiRouting` | `test_ai_routing` | 30 PASS — `_pickRoutedModel()`/`isCoderModel()` (D-27): CAT-A routing OFF→mai cambia, CAT-B immagine→vision (priorità su codice), CAT-C dominio codice→coder, CAT-D dominio generale/STEM→nessun cambio, CAT-E riconoscimento nomi modello coder |
| `AiMemory` | `test_ai_memory` | 12 PASS — costruzione/init, logFeedback/saveInteraction/getRelevantContext/updatePreference/gitLog (richiede `git` in PATH) |
| `Distillazione` | `test_distillazione` | 6 PASS — costruzione `DistillazionePage`, stato iniziale widget, pulsanti disabilitati all'avvio |
| `GraphMemoryConcurrent` | `test_graph_memory_concurrent` | 17 PASS — isolamento connessioni SQLite, lettura/scrittura sequenziale 2 istanze, scrittura concorrente 2 thread, lettura cross-istanza |
| `McpIntegration` | `test_mcp_integration` | 21 PASS, 1 SKIP — struttura `McpManagerPage`, `scanMcpServers`, protocollo JSON-RPC smoke test; ⚠️ CAT-D venv assente in questo ambiente |
| `ExternalAiImport` | `test_external_ai_import` | 9 PASS — CAT-A parsing OpenAI (mapping/current_node), Anthropic (chat_messages), generico role/content, multi-conversazione; CAT-B JSON non valido/formato ignoto/file assente/content vuoto |
| `Vision3D` | `test_vision3d` | 48 PASS — CAT-A costruzione (29, incluso `galleryHasContextMenuForSaving` per "Salva sul PC"), gestione sessioni (`createSession()` crea+sanifica+rifiuta nomi non validi e ora accetta una descrizione opzionale persistita subito, `importPhotoFiles()` crea scatti leggibili senza sensori e scarta immagini non decodificabili, `deleteShot()` rimuove tutti i file collegati incluse le 3 mappe edges/bump/normal), `sessionDescription()`/`setSessionDescription()` round-trip su `session.json` (`sessionDescriptionRoundTrip`), descrizione nel sidecar `.json` di uno scatto caricata in galleria dopo il reload sessione (`shotDescriptionLoadedFromSidecarIntoGallery`, segnale pubblico `currentTextChanged` chiamato direttamente per forzare `loadSessionIntoUi()` privata), 2 pulsanti "✏️" per le descrizioni sessione/scatto (`hasEditDescriptionButtons`), galleria `ExtendedSelection` + scorciatoia Canc (`QShortcut`) + pulsante "Elimina tutte", pulsante "Salva tutte le mappe" + 6 miniature con `Qt::CustomContextMenu` abilitato (`hasSaveAllMapsButtonAndSixThumbs`), tabella device con `Qt::CustomContextMenu` per l'eliminazione (`deviceTableHasContextMenuForDeletion`), doppio click su una miniatura mancante la calcola e la salva su disco (`doubleClickOnMissingMapRecomputesIt`, evento inviato via `QCoreApplication::sendEvent` — passa comunque dall'`eventFilter` installato), fetch modelli VLM alla costruzione senza crash, UI a due sotto-schede (`QTabWidget`: "\xf0\x9f\x93\xb7 Preparazione" a due colonne con QR di collegamento `m_prepQr` + stampa bersagli ArUco, e "\xf0\x9f\xa7\xb1 Assembla punti e texture" con scena 3D + ricostruzione — niente più `QPlainTextEdit` locale), porta default `P::kVision3DPort`, setter, combo interfacce (LAN 192.168 prima, no docker/virbr, loopback in coda), galleria `v3dGallery` + combo VLM `v3dVlmCombo`, scena 3D `v3dScene` (addShot dedup/clear) + tasselli-bersaglio sfera `setTargetQuality`, ricostruzione `v3dReconBtn`/`v3dSessionCombo`, posa manuale `v3dPoseHead`/`v3dPosePitch` (wrapping, disabilitati senza selezione) + `v3dDistribBtn`, guida `v3dHelpBtn` (dialog con QR + link stampa ArUco), messaggio "pronto" su `LogBus` con categoria `"3d"` (non più log locale) + sonda COLMAP a caldo `v3dRecheckBtn`/`v3dReconHint`, contatore foto/qualità `v3dPhotoReq` (low≥10 medium≥20 high≥40); CAT-B stop idempotente, distruzione pulita, start porta privilegiata (7); CAT-C e2e server HTTPS reale con curl su bind 127.0.0.1 (12: GET pagina con selettore Oggetto/Scena `modeChips` default "object" + hint "SCATTA ORA"/`readyToShoot` + `onMotion`/`watchPosition`, 404, POST /upload salva jpg+json con `scan_mode` nel sidecar (default "object", esplicito "scene" da chip telefono), POST /upload con accel/accelGravity/rotationRate/altitude/altitudeAccuracy/latitude/longitude → verifica persistiti nel sidecar come `accel`/`accel_gravity`/`rotation_rate`/`altitude_m`/`altitude_accuracy_m`/`latitude`/`longitude` **e** mostrati subito nel `QLabel` `v3dLastSensorInfo` (non solo salvati su disco), pagina HTML con geometria sfera-guida dipendente da `scanMode` (`scanMode==='scene' ? 1` anello) e ricostruzione bersagli al cambio chip (`buildTargets()`), GET /download obj/mtl/ply, POST /upload con `wants:["boxes"]` su quadrato ad alto contrasto → verifica `detectBoxes()` usa davvero OpenCV quando `VISION3D_USE_OPENCV` è definito: `boxes_image` presente + `boxes_count`≥1, altrimenti assente, POST /upload con `wants:["edges","bump","normal"]` → stesso pattern per `edgeMap()`/`bumpMap()`/`normalMap()` (`uploadWithEdgesBumpNormalUsesOpenCV`), POST /upload con `wants:["edges"]` su contrasto moderato → verifica che `edges_image` contenga davvero pixel di bordo, non solo che il campo esista (`uploadWithEdgesDetectsModerateContrastEdge`), POST /upload con `flash:true` → `flash_used` nella risposta e `flash_on` nel sidecar (`uploadWithFlashIsPersistedAndEchoed`)) — QSKIP senza curl/openssl. Pipeline recon 3 passi: sparse → PLY → OBJ+MTL colorati via `Tools/scripts/ply_to_obj.py` |
| `VoiceCloner` | `test_voice_cloner` | 7 PASS — CAT-A costruzione (attende completamento delle 3 probe `checkTtsInstalled()` prima di distruggere); CAT-B (T-D20/D-20) distruzione IMMEDIATA senza processare eventi, con le probe QProcess quasi certamente ancora in esecuzione — replica da riga di comando lo scenario mai verificato a schermo "apri Clona Voce, chiudi subito l'app" (coredump reale 2026-07-03), ripetuto 5 volte di seguito; nessun crash/coredump del processo di test |
| `ToolDispatchersD33` | `test_tool_dispatchers_d33` | 9 PASS — copertura T-D33 (TODO.md) SENZA modello reale: chiama direttamente i dispatcher privati `AgentiPage::onTool*()` (pattern `#define private public`, `MockAiClient`, nessuna rete) con gli stessi argomenti strutturati elencati in T-D33 — `algoritmo` (mcd, hanoi_passi N=6→63 mosse, nqueens N=8→92 soluzioni OEIS A000170), `codice_fiscale`/`carta_astrale`/`valida_documento` (confrontati con le funzioni sottostanti già coperte da PraticoFinanza/Astrale — verifica il wrapper, non riderivano l'algoritmo), `finanza_calcola` (mutuo 100000€/3%/20anni→554.60€), `converti` (200ml farina→106g). Esclusi (fuori scope): `disegna_grafico` (tocca la UI) e la scelta autonoma del tool da parte del modello (richiede un vero LLM tool-capable) |

### Note operative
- `SimulatoreAlgos`: FLAKY in `-j4`, PASS standalone → `RESOURCE_LOCK cpu_heavy`
- `AiStress` / `AiIntegration` / `TeamCollab`: richiedono Ollama + `mistral:7b-instruct` ≥2 GB RAM
- `HardwareMonitor`: richiede `mon.start()` prima di `QVERIFY(spy.wait(...))`
- **CAT-E** (TestRisolviPassi): 15 test SymPy reali via `_runPythonSync()` — `QSKIP` se Python mancante

### Regola: come aggiungere una nuova suite di test

**Ogni volta che si crea una nuova suite test, obbligatoriamente:**
1. Creare `gui/tests/test_<nome>.cpp` con almeno CAT-A costruzione (no Ollama)
2. Aggiungere il target in `gui/CMakeLists.txt` (copia struttura da `test_programmazione_page`)
3. Aggiungere `add_test(NAME <NomePascal> COMMAND test_<nome>)` vicino ai simili
4. Aggiornare la tabella "Suite per categoria" qui sopra con: Suite | Target | PASS count | Note
5. Aggiornare il conteggio in `ctest --test-dir gui/build_tests` (riga 291 di questo file)
6. Nel test: usare `QSKIP` in `initTestCase()` per dipendenze esterne (Ollama, Docker, ecc.)

**Priorità categorie test:**
- CAT-A: costruzione widget senza crash (nessuna dipendenza esterna)
- CAT-B: funzioni pure / helper statiche
- CAT-C: integrazione con processo esterno (QSKIP se mancante)
- CAT-D: integrazione Ollama reale (QSKIP se non disponibile)
