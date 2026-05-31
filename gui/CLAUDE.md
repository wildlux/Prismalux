# CLAUDE.md — Prismalux Qt GUI  v2.9

## Build
```bash
# Linux (diretto)
cmake -B build_gui gui/ -DCMAKE_BUILD_TYPE=Release && cmake --build build_gui -j$(nproc)
./build_gui/Prismalux_GUI

# Tutte le piattaforme (Windows/Linux/macOS)
python3 build.py
```
Strutturale (nuovo file/CMakeLists) → rifare `cmake -B build_gui`. Solo .cpp/.h → solo `cmake --build build_gui`.

`build.py` — motore di build multipiattaforma. Genera `errore.txt` in root se fallisce.
Windows: `build.bat` trova Python ed esegue `build.py`. Prima volta: `COMPILE_WIN\setup.bat`.

## Layout tab (mainwindow.cpp)
```
Header (72px): logo · backend · model · CPU/RAM/GPU · spinner · ⚙️
[0] 🤖 Intelligenza Artificiale  Alt+1  Pipeline + Byzantino + CHAT RAG + Agente Autonomo
[1] 🛠 Strumenti                  Alt+2  Assistente AI · 💰 Finanza (730/PIVA/Calcolatori/TFR) · ⏱ Cron · Impara · Sfida!
[2] 🎬 Multimedia                        Audio AI (Whisper STT+TTS) · Stable Diffusion · Mappe Graphviz
[3] 📁 File AI                           File AI · Wiki & Web · Excel/CSV · PDF · Word/Testo
[4] 💻 Programmazione            Alt+3  Editor+AI · Agentica · Translitter · Reverse Eng. · Git · REPL · Interpreter · Rete · Driver
[5] π  Matematica                Alt+4  Sequenza→Formula · Costanti · N-esimo · Espressione · Risolvi Passi (SymPy+🔀) · Analisi 1&2 (LaTeX KaTeX)
[6] 🔬 Ricerca                   Alt+5  Paper · Brevetti · Lavoro · Cytoscape—Bio · RDKit · Bioconda · RAB₀-L · BLHM · Analisi Fenomeni · 🕸️ Grafo RAG · Astrale
[7] 🕹 APP Controller            Alt+6  Blender/FreeCAD/Office/CloudCompare/Anki/KiCAD/TinyMCP/OBS/OpenCode/Godot
[8] 🌐 LAN & WAN                         LAN Android (QR/ADB) · GNS3 MCP · WAN Compute (🧠 Solo PC | 🌐 Rete LAN → Multi-Agente + GraphMemory)
ImpostazioniPage: dialog modale (⚙️ header)
```
Note:
- `LavoroPage` è in StrumentiPage (`m_lavoroPage`), NON in mainwindow
- Cron (`m_cronPanel`) in StrumentiPage via `installCronPanel()` con `QTimer::singleShot(0)`
- Cytoscape/RDKit/Bioconda/Avogadro → Ricerca [6]; GNS3 → LAN & WAN [8]; Godot → AppController [7]
- AppController tab indici: 0=Blender 1=FreeCAD 2=Office 3=CloudCompare 4=Anki 5=KiCAD 6=TinyMCP 7=OBS 8=OpenCode 9=Godot
- Web app (lan_server.cpp) tab 🎙️ Voce: TTS (SpeechSynthesis) + STT (MediaRecorder→/api/whisper)

## Struttura cartelle repo

```
Prismalux/
├── gui/                          ← sorgente C++/Qt6 (questo progetto)
│   ├── pages/                    ← una pagina = .h + uno o più .cpp
│   ├── widgets/                  ← componenti header-only e riutilizzabili
│   ├── tests/                    ← suite ctest (41 suite, BUILD_TESTS=ON)
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
├── MCPs/                         ← 18 plugin MCP Python — ognuno ha README.md + requirements.txt
├── BEST_PRACTICE_&_GOAL/         ← regole, TODO, operazioni (non codice)
│   └── REGOLE_IRREMOVIBILI.md    ← 15 convenzioni fisse — leggere prima di toccare il codice
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
| `widgets/ai_error_widget.h` | Header-only Q_OBJECT — `showError(msg, onRetry)` — elencato in CPP_SRCS |
| `widgets/code_interpreter_widget.h/cpp` | Python sandbox: exec, matplotlib PNG, Docker |
| `MCPs/knowledge_mcp/server.py` | Knowledge Updater MCP (JSON-RPC 2.0 stdio) |
| `MCPs/ollama_mcp/server.py` | Ollama model cache MCP — SQLite TTL 5min, 5 tool (list/info/search/sync/pull) |

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

## Suite di Test
```bash
cmake -B build_tests gui/ -DBUILD_TESTS=ON && cmake --build build_tests -j$(nproc)
ctest --test-dir build_tests -j4   # 41 suite (38 no-Ollama, 3 richiedono Ollama reale)
```

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
| `ProgrammazionePage` | `test_programmazione_page` | isIntentionalError, parseNumbers |
| `ThinkingDetect` | `test_thinking_detect` | extractName/Size/Prio, classifyQuery, keyLock |
| `AgentiPipeline` | `test_agenti_pipeline` | buildBubble, markdownToHtml |
| `AgentsConfigDialog` | `test_agents_config_dialog` | struttura, numAgents, RAG condiviso |
| `AgentiByzantine` | `test_agenti_byzantine` | voce combo, num agenti, mock stub |
| `AgenteAutonomo` | `test_agente_autonomo` | ReAct loop, toggle UI, parsing tool call |
| `LanServer` | `test_lan_server` | lifecycle TCP, token, rate limit |
| `Onboarding` | `test_onboarding` | QSettings, token LAN, rate limiter |
| `ImpostazioniPage` | `test_impostazioni_page` | AiChatParams round-trip, ThinkMode, preset |
| `ThemeManager` | `test_theme_manager` | lista temi, ops |
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
| `SttWhisper` | `test_stt_whisper` | paths, availability, permissions, mic |
| `AiIntegration` | `test_ai_integration` | ⚠️ Ollama reale — classifyQuery, chat, params |
| `AiStress` | `test_ai_stress` | ⚠️ Ollama reale — sequential, param matrix |
| `TeamCollab` | `test_team_collab` | ⚠️ Ollama reale — pipeline, quality |

### Note operative
- `SimulatoreAlgos`: FLAKY in `-j4`, PASS standalone → `RESOURCE_LOCK cpu_heavy`
- `AiStress` / `AiIntegration` / `TeamCollab`: richiedono Ollama + `mistral:7b-instruct` ≥2 GB RAM
- `HardwareMonitor`: richiede `mon.start()` prima di `QVERIFY(spy.wait(...))`
- **CAT-E** (TestRisolviPassi): 15 test SymPy reali via `_runPythonSync()` — `QSKIP` se Python mancante
