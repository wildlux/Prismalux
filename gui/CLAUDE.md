# CLAUDE.md — Prismalux Qt GUI

## Build
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)
./build/Prismalux_GUI
```
Strutturale (nuovo file/CMakeLists) → rifare `cmake -B build`. Solo .cpp/.h → solo `cmake --build build`.

## Layout tab (mainwindow.cpp)
```
Header (72px): logo · backend · model · CPU/RAM/GPU · spinner · ⚙️
[0] 🤖 Intelligenza Artificiale  Alt+1  Pipeline + Byzantino + CHAT RAG + Agente Autonomo
[1] 🛠 Strumenti                  Alt+2  Studio, Scrittura, Ricerca, Cerca Lavoro, Libri, PDF, Cron
[2] 🎬 Multimedia                        Audio AI · Genera Immagini (SD) · Mappe (Graphviz)
[3] 📁 File AI                           File AI · Wiki & Web · Excel/CSV · PDF · Word/Testo
[4] 💻 Programmazione            Alt+3  Editor + Agentica + Interpreter Python
[5] π  Matematica                Alt+4  Parser formule · Grafico
[6] 🔬 Ricerca                   Alt+5  Paper · Brevetti · Cytoscape · RDKit · Bioconda · Avogadro
[7] 🕹 APP Controller            Alt+6  Blender/FreeCAD/Office/CloudCompare/Anki/KiCAD/TinyMCP/OBS/OpenCode/Godot
[8] 🌐 LAN & WAN                         LAN Android + GNS3 · WAN placeholder
[9] 📚 Impara                    Alt+7  Finanza · Impara con AI · Sfida
ImpostazioniPage: dialog modale (⚙️ header)
```
Note:
- `LavoroPage` è in StrumentiPage (`m_lavoroPage`), NON in mainwindow
- Cron (`m_cronPanel`) in StrumentiPage via `installCronPanel()` con `QTimer::singleShot(0)`
- Cytoscape/RDKit/Bioconda/Avogadro → Ricerca [6]; GNS3 → LAN & WAN [8]; Godot → AppController [7]
- AppController tab indici: 0=Blender 1=FreeCAD 2=Office 3=CloudCompare 4=Anki 5=KiCAD 6=TinyMCP 7=OBS 8=OpenCode 9=Godot

## File chiave
| File | Ruolo |
|------|-------|
| `mainwindow.h/cpp` | Header, tab bar, llama-server manager |
| `ai_client.h/cpp` | HTTP Ollama/llama-server — `chat()`, `fetchModels()`, `fetchEmbedding()` |
| `prismalux_paths.h` | Unico punto di verità per path, porte, QSettings keys |
| `rag_engine.h/cpp` | RAG JLT 256-dim |
| `hardware_monitor.h/cpp` | Thread polling CPU/RAM/GPU ogni 2s |
| `lan_server.h/cpp` | Server TCP LAN per PrismaluxMobile Android (porta 11500) |
| `pages/agenti_page.*` | Pipeline 6 agenti + Byzantino + Agente Autonomo (15 moduli) |
| `pages/ricerca_page.*` | Tab Ricerca [6] — include Cyto/RDKit/Bio/Avo |
| `widgets/ai_error_widget.h` | Header-only Q_OBJECT — `showError(msg, onRetry)` — elencato in CPP_SRCS |
| `widgets/code_interpreter_widget.h/cpp` | Python sandbox: exec, matplotlib PNG, Docker |
| `MCPs/knowledge_mcp/server.py` | Knowledge Updater MCP (JSON-RPC 2.0 stdio) |

## Convenzioni critiche

**Path — mai hardcode:**
```cpp
#include "prismalux_paths.h"
namespace P = PrismaluxPaths;
// P::root(), P::kOllamaPort, P::kLlamaServerPort, P::kOpenCodePort
// P::modelIcon(sizeBytes, name), P::feedbackPath(), P::userKnowledgePath()
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

## Suite di Test
```bash
cmake -B build_tests -DBUILD_TESTS=ON && cmake --build build_tests -j$(nproc)
ctest --test-dir build_tests -j4   # 35/37 PASS
```
- `SimulatoreAlgos`: FLAKY in -j4, PASS standalone → `RESOURCE_LOCK cpu_heavy` in CMakeLists
- `AiStress`: OOM RAM (non bug) — richiede `mistral:7b-instruct` e ≥2 GB RAM libera
- `HardwareMonitor`: richiede `mon.start()` prima di `QVERIFY(spy.wait(...))`
