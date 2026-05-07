# CLAUDE.md — Prismalux Qt GUI

> **Progetto sibling Android**: `../ANDROID/android_app/` — PrismaluxMobile (Qt6/C++, client Ollama LAN).
> Struttura analoga a `gui/`: stesso `ai_client`, stesso pattern pagine, RAG semplificato keyword-based.
> Implementazione completa pianificata dopo stabilità 100% desktop.

## Build
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)
./build/Prismalux_GUI
```
Strutturale → `cmake -B build` prima. Solo .cpp/.h → solo `cmake --build build`.

## Layout tab (mainwindow.cpp)
```
Header (72px): logo · backend · model · CPU/RAM/GPU · spinner · ⚙️
[0] 🤖 Intelligenza Artificiale  Alt+1  Pipeline + Byzantino + CHAT RAG + Agente Autonomo
[1] 🛠 Strumenti AI              Alt+2  Studio, Scrittura, Ricerca, 💼 Cerca Lavoro, Libri,
                                        Produttività, Documenti, ⏰ Cron, 🗂 File AI, 📖 Wiki, 🎨 Stable Diffusion, 📱 LAN Android
[2] 💻 Programmazione            Alt+3  Editor + sub-tab Agentica + 🧪 Interpreter
[3] π  Matematica                Alt+4  Matematica · Grafico
[4] 🔬 Ricerca                   Alt+5  Paper · Brevetti · Documenti tecnici
[5] 🕹 APP Controller            Alt+6  Blender/Office/Anki/KiCAD/TinyMCP/OpenCode
[6] 📚 Impara                    Alt+7  Finanza · Impara con AI · Sfida te stesso
ImpostazioniPage: dialog modale (⚙️ header)
```
Note: Cerca Lavoro è in Strumenti AI (cat 3, tra Ricerca e Libri) — attivata da lavoroBtn (checkable).
LavoroPage è istanziata dentro StrumentiPage (m_lavoroPage), NON in mainwindow.cpp.
LAN Android è in Strumenti AI come pannello laterale (m_lanPanel): `LanServer` TCP in `lan_server.h/cpp`.
ImpostazioniPage → sezione "Ollama LAN": avvia/ferma `ollama serve` con `OLLAMA_HOST=0.0.0.0:11434`.
Cron è `m_cronPanel` in StrumentiPage (checkable `cronBtn`), non in Manutenzione.
File AI è `m_fileSearchPanel` in StrumentiPage: Python subprocess walk + score keyword + analisi AI.
Wiki è `m_wikiPanel` in StrumentiPage: Wikipedia REST API multilingua + 4 azioni AI rapide.
🎨 Stable Diffusion è `m_sdPanel` (`StableDiffusionWidget`) in StrumentiPage: REST API AUTOMATIC1111/Forge (`/sdapi/v1/txt2img`). Lista modelli automatica, output PNG inline.
🧪 Interpreter è sub-tab di Programmazione: `code_interpreter_widget.h/cpp` — Python sandbox, matplotlib inline.

## File chiave
| File | Ruolo |
|------|-------|
| `mainwindow.h/cpp` | Header, tab bar, llama-server manager |
| `ai_client.h/cpp` | HTTP Ollama/llama-server — `chat()`, `fetchModels()`, `fetchEmbedding()` |
| `prismalux_paths.h` | **Unico punto di verità** per path, porte, costanti QSettings |
| `rag_engine.h/cpp` | RAG JLT 256-dim — `addChunk()`, `search()`, `save()/load()` |
| `hardware_monitor.h/cpp` | Thread polling CPU/RAM/GPU ogni 2s |
| `lan_server.h/cpp` | Server TCP LAN per PrismaluxMobile Android — porta default 11500 |
| `pages/agenti_page.*` | Pipeline 6 agenti + Byzantino + Agente Autonomo (15 moduli) |
| `pages/agenti_page_auto.cpp` | Agente Autonomo ReAct: THOUGHT→ACTION→OBSERVATION→FINAL_ANSWER |
| `pages/agenti_page_feedback.cpp` | Feedback 👍/👎 → `~/.prismalux/feedback.jsonl` |
| `pages/manutenzione_page_lan.cpp` | Pannello LAN (toggle Start/Stop, porta, IP, client connessi) |
| `pages/opencode_page.*` | OpenCode serve HTTP+SSE — port 8092 |
| `widgets/code_interpreter_widget.h/cpp` | Python sandbox inline: exec, matplotlib PNG, Docker/locale |

## Convenzioni critiche

**Path — mai hardcode:**
```cpp
#include "prismalux_paths.h"
namespace P = PrismaluxPaths;
// P::root(), P::kOllamaPort(11434), P::kLlamaServerPort(8081), P::kOpenCodePort(8092)
// P::kLocalHost, P::openCodeBin(), P::llamaServerBin(), P::scanGgufFiles()
```

**Backend — usa sempre `m_ai->backend()`:**
```cpp
m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), nuovoModello);
// NON hardcodare AiClient::Ollama — rompe llama-server
```

**Connessione one-shot:**
```cpp
auto* h = new QObject(this);
connect(src, &Src::sig, h, [this, h](auto v){ h->deleteLater(); /* logica */ });
```

**Embedding (mutuamente esclusive):** dichiara `conn/connErr` prima; ogni lambda disconnette entrambe. Usa `Qt::SingleShotConnection`.

**Chat log:** `m_log->moveCursor(QTextCursor::End); m_log->insertHtml(html);`

**Emoji hex:** `"\xf0\x9f\x92\xac"` (💬) `"\xe2\x9c\x85"` (✅) `"\xe2\x9d\x8c"` (❌)  
Concatenazione: `"\xe2\x86\x92" "B"` — non `"\xe2\x86\x92B"` (B = cifra hex valida → errore).

**QSettings:** sempre via `P::SK::` — centralizzate in `prismalux_paths.h` namespace `SK`.

**Widget header-only con Q_OBJECT:** elenca in `CPP_SRCS` (AUTOMOC).

**Polling asincrono:** no `waitForStarted()` — usa `errorOccurred` + `QTimer`.

**Combo senza loop:**
```cpp
combo->blockSignals(true); combo->setCurrentIndex(idx); combo->blockSignals(false);
```

**Icone modelli + UserRole (pattern standard per tutte le combo modello):**
```cpp
// Aggiunta item: icona in displayText, nome raw in UserRole
for (const QString& m : models) {
    qint64 sz = m_ai ? m_ai->modelSizeBytes(m) : 0;
    combo->addItem(P::modelIcon(sz, m) + m, m);   // UserRole = nome raw
}
// Lettura nome modello — MAI currentText():
QString modello = combo->currentData(Qt::UserRole).toString();
if (modello.isEmpty()) modello = combo->currentText();  // fallback legacy
// Ripristino selezione:
int idx = combo->findData(saved);
if (idx < 0) idx = combo->findText(saved);  // fallback se pre-icone
if (idx >= 0) combo->setCurrentIndex(idx);
```
`P::modelIcon(sizeBytes, name)` è in `prismalux_paths.h` — ☁️ se size==0 o name ends "cloud", altrimenti 🌍📍.

**Tema QSS — come funziona:**
- Unico punto di caricamento: `ThemeManager::applyTheme()` in `theme_manager.cpp`
- Carica `themes/<id>.qss` + appende `themes/base.qss` (struttura scrollbar, regole custom widget)
- `style.qss` nella root `gui/` NON viene caricato — è un file legacy/riferimento
- Per aggiungere stili a tutti i temi: usa `themes/base.qss`; per stili tema-specifici: modifica il `.qss` del tema
- **CRITICO — parent ThemeManager**: usare `static ThemeManager inst(nullptr)` — MAI `static ThemeManager inst(qApp)`.
  Con `qApp` come parent, Qt aggiunge `inst` al children-tree di QApplication; alla distruzione di `app`,
  `deleteChildren()` chiama `delete &inst` su un oggetto stack → heap corruption → ABRT Signal 6.
  Fix applicato 2026-05-06 in `theme_manager.cpp:15`.

## AiClient — API
```cpp
m_ai->setBackend(Backend, host, port, model);  // non emette segnali
m_ai->fetchModels();           // → modelsReady(QStringList)
// Overload 1 — legacy, nessuna storia
m_ai->chat(sys, msg);          // → token(chunk) + finished(full) | error(msg)
// Overload 2 — con storia compressa + tipo query (routing think)
m_ai->chat(sys, msg, historyJsonArray, QueryType);
// QueryType: QueryAuto=classifyQuery(), QuerySimple=no think, QueryComplex=think
m_ai->abort();                 // → aborted() — NON chiama onFinished()
m_ai->fetchEmbedding(t);       // → embeddingReady(vec) | embeddingError(msg)
m_ai->setNumGpu(n);            // n≥0 → invia num_gpu=n; n<0 → non inviato (Ollama auto)
m_ai->fetchModelLayers(cb);    // → cb(int layers) via /api/show
m_ai->unloadModel();           // keep_alive=0 → scarica modello dalla RAM Ollama
// .backend() .host() .port() .model() .busy() .numGpu() .currentReqId()
```

## Think Mode — Ragionamento AI
`AiChatParams` (persistito in `~/.prismalux/ai_params.json`) ha due campi:
- `thinkMode` int: 0=auto (classificatore decide), 1=off (forzato), 2=on (forzato)
- `thinkBudget` int 1-4: moltiplicatore `num_predict` quando thinking attivo

**Logica in `AiClient::chat()`**:
1. Se modello NON think-capable (qwen3/deepseek-r1/qwq/qwen2.5) → `think` non inviato
2. Se `thinkMode=1` (off) → `think=false` sempre
3. Se `thinkMode=2` (on) → `think=true` per modelli capable
4. Se `thinkMode=0` (auto) → classificatore `classifyQuery()`: Simple→think=false, Complex→think=true

**Fix thinking+content (2026-05-07):** quando Ollama restituisce sia `message.thinking` che `message.content`,
`m_thinkingAccum` contiene il reasoning e `m_accum` contiene il content. Il `finished()` ora emette
`"<think>" + m_thinkingAccum + "</think>" + m_accum` così `agenti_page_stream.cpp` può estrarlo
e mostrare il toggle ▶️ Ragionamento nella bolla.

**Toggle inline nella bolla** (`agenti_page_ui.cpp::anchorClicked`):
- Link `think:toggle:ID` → mostra/nasconde il box ragionamento sotto la bolla
- ▶️ (arrotolato) ↔ 🔻 (aperto); testo in `m_thinkTexts[idx]`
- `m_thinkShown (QSet<int>)` tiene traccia delle bolle aperte
- Il box usa manipolazione HTML diretta su `m_log->setHtml()` (non è possibile modificare solo un nodo)

**Classificatore `AiClient::classifyQuery(text)`**:
- ≤30 char → `QuerySimple` (think=false, num_predict=512)
- >200 char o keyword: spiega/calcola/perché/passo passo/dimostra/analizza → `QueryComplex`
- Resto → `QueryAuto`

**UI — Impostazioni → AI Locale → "Ragionamento AI"**:
- 3 QPushButton checkable esclusivi (objectName=`thinkModeBtn`): Off / Auto / On
- QSlider 1-4 per budget (disabilitato se Off)
- Label RAM warning se < 12 GB
- Cambio immediato via `AiChatParams::save()` + `m_ai->setChatParams()`
- Style in `themes/base.qss` — funziona su tutti i 23 temi

## Modalità Calcolo LLM (manutenzione_page.cpp)
Quattro bottoni: **GPU** · **CPU** · **Misto** · **Doppia GPU**. Salvato in `QSettings(P::SK::kComputeMode)` + `PRISMALUX_COMPUTE_MODE` env.

**GPU e Misto sono sempre abilitati** (fix 2026-05-07): in precedenza venivano disabilitati se
`hasDedicated=false` (nessuna GPU rilevata). Ora sono sempre cliccabili — se non c'è GPU dedicata
Ollama gestisce il fallback su CPU senza errori. Solo **Doppia GPU** rimane condizionata alla
presenza simultanea di GPU dedicata + Intel iGPU.

| Modo | `num_gpu` inviato | Comportamento |
|------|-------------------|---------------|
| `gpu` | `= layers` (da `/api/show`) | tutti i layer su GPU dedicata (NVIDIA/AMD) |
| `cpu` | `0` | tutto su RAM, GPU ignorata |
| `misto` | `= min(layers, m_gpuLayersFull)` | riempie NVIDIA al massimo, overflow su CPU/RAM |
| `doppia` | `-2` (auto, solo NVIDIA per Ollama) | NVIDIA + Intel iGPU via llama-server CUDA+SYCL |

**Regola critica**: Ollama NON clipa `num_gpu` → valore > layer count reale causa ISE 500.
Usare sempre `fetchModelLayers()` prima di GPU o Misto. Con `-2` AiClient non manda `num_gpu`.

**HWInfo — 3 tipologie di device**:
- `hw.primary` → CPU (RAM)
- `hw.secondary` → GPU dedicata: NVIDIA > AMD > Intel iGPU (priorità decrescente)
- `hw.igpu` → Intel iGPU (index in `dev[]`, `-1` se assente)
- `DEV_INTEL` ha sempre `n_gpu_layers=0` — non usata per inferenza Ollama

**Misto ottimizzato**: `num_gpu = min(layer_model, capacity_NVIDIA)` — riempie la VRAM NVIDIA al massimo.
Se `gpuLayers >= total`: il modello entra interamente in GPU → suggerisci modalità GPU pura.

**Doppia GPU (NVIDIA + Intel iGPU)**:
- Ollama: CUDA non vede Intel → usa solo NVIDIA, mostra comando llama-server per info
- llama-server CUDA+SYCL: `--device CUDA0,SYCL0 --tensor-split {nv_ratio},{ig_ratio} -ngl 99`
- Rapporto calcolato da `m_nvidiaVramMb / (m_nvidiaVramMb + m_igpuVramMb)`
- `m_igpuVramMb`: apertura GGTT letta da `/sys/bus/pci/devices/XXXX:XX:XX.X/resource`

**Nota 4 GB VRAM**: 3.4 GB modello + KV cache ~200 MB + RAG ~270 MB = ~3.9 GB → ai limiti.
Budget netto = VRAM_disponibile - 470 MB. Usare Misto se il margine è < 200 MB.

## Presets & Flag llama-server (Impostazioni → AI Locale)

**Presets a un clic** (scrivono in `AiChatParams` + QSettings, applicati a ogni avvio del server):

| Preset | num_ctx | num_predict | temp | Extra |
|--------|---------|-------------|------|-------|
| **8 GB RAM** | 4096 | 1024 | 0.05 | Flash Attention ON |
| **📜 Contesto lungo** | 16384 (≥16 GB RAM) / 8192 | default | default | Flash Attention ON |

**Flag llama-server passati da `mainwindow.cpp`** (solo backend llama-server, non Ollama):

| Flag | Dove si attiva | Effetto |
|------|---------------|---------|
| `--swa-full` | sempre attivo | Forza full attention su modelli SWA (Qwen3/Gemma3/Mistral) — evita perdita di contesto |
| `--cache-type-k q8_0` | sempre attivo | KV cache quantizzata a 8 bit → −25-35% RAM, qualità invariata |
| `--cache-type-v q8_0` | sempre attivo | come sopra per i value |
| `--mlock` | toggle QCheckBox "Blocca modello in RAM" | Nessuno swap, latenza minore su sistemi con RAM sufficiente |
| `flash_attn=true` | toggle QCheckBox "Flash Attention" | Accelera su GPU CUDA/Metal; su CPU non ha effetto |

**num_ctx dinamico in `ai_client.cpp`**: se RAM rilevata < 10 GB e num_ctx > 4096, viene silenziosamente ridotto a 4096 prima di ogni chat — protegge sistemi a 8 GB da OOM.

## AgentiPage
- `m_modePipeline=false` → CHAT RAG (1 agente); `true` → pipeline
- `m_pageModel` (QString privato): isola la scelta LLM da `modelChanged`
- Invio = modalità corrente · Stop da fermo = toggle CHAT↔Avvia
- `abort()` → `onAborted` rimuove testo da `m_agentBlockStart` a End
- qwen3.5 rimosso da `s_knownBroken` (fix 2026-04-24) — nessun workaround necessario

**Banner "Agente Autonomo attivato" (fix 2026-05-07):**
Il banner viene mostrato nel `m_btnRun::clicked` handler (al PRIMO invio effettivo con `m_autoEnabled=true`),
NON nel `m_btnModeToggle::toggled` handler. `m_autoMsgShown=true` dopo il primo show — non riappare mai più.
Ragione: il banner non deve apparire se l'utente clicca il toggle e poi torna indietro senza inviare nulla.

**Pulsante vocale "Conversa" / "Conversa con [Nome]":**
`m_btnVoiceLoop` mostra "🎙 Conversa" se personalità = "nessuna", altrimenti "🎙 Conversa con [Nome]".
`P::personalityName()` legge il QSettings `ai/personality` e ritorna il nome visualizzato.

**OpMode enum:**
```
Idle | Pipeline | Byzantine | MathTheory | PipelineControl
Translating | KnowledgeExtract | ConsiglioScientifico | AutonomousAgent
```

## Agente Autonomo ReAct (agenti_page_auto.cpp)

Toggle "🤖 Auto" nella toolbar (indigo quando attivo); incompatibile con Tools.

**Ciclo:**
```
THOUGHT: … → ACTION: tool(input) → [esecuzione async] → OBSERVATION: result
→ THOUGHT: … → … → FINAL_ANSWER: testo finale
```
- Max 8 step (`m_autoMaxSteps`) — step card colorata per ogni iterazione
- Storia multi-turno in `m_autoHistory (QJsonArray)` — ogni step aggiunge assistant+user
- Prima chiamata: `m_ai->chat(sys, task)` · successive: `m_ai->chat(sys, lastMsg, history, QueryComplex)`
- Strumenti disponibili: `calc`, `ricerca`, `python`, `leggi_file`, `lista_file`, `scrivi_file`
- `scrivi_file` richiede dialog di conferma utente (sicurezza)

**Pattern invio Run:**
```cpp
if (m_autoEnabled && !m_modePipeline) {
    m_autoHistory = QJsonArray(); m_autoStep = 0; m_autoBuf.clear();
    m_autoLastUserMsg = task;
    runAutonomousAgent();
}
```

## Code Interpreter (code_interpreter_widget.h/cpp)

Sub-tab "🧪 Interpreter" in Programmazione. Esegue Python localmente o in Docker sandbox.

- **Sandbox Docker**: `--network none --memory 256m --pids-limit 64` — nessun accesso a filesystem/rete
- **Locale**: `QTemporaryFile` + `QProcess`, timeout 30s, kill automatico
- matplotlib PNG rilevato in stdout → mostrato in QLabel scalato inline
- Errore → auto-retry con fix AI (max 3 tentativi): invia errore al modello, riceve codice corretto, riprova
- `PrismaluxPaths::isSandboxReady()` — verifica se Docker è disponibile; mostra avviso diverso a seconda

## Web Reading nel RAG

Pulsante **🌐** in ogni `RagDropWidget` → `QInputDialog` URL → `QNetworkAccessManager` async.

- Strip HTML: rimuove `<script>/<style>`, converte tag blocco in `\n`, decodifica entità
- Limiti: 12 KB per pagina, 32 KB totale RAG (era 4 KB/16 KB solo file)
- `ragContext()` include `[N] host/path` come citazione + istruzione all'AI per citare le fonti
- Disponibile in: `RagDropWidget` (agenti, shared RAG, programmazione_page, strumenti_page)

## Memoria Automatica Cross-Sessione

Estrattore silenzioso a fine chat aggiorna `user_knowledge.md` senza intervento manuale.

- Trigger: ogni 4 scambi in CHAT RAG singola (`m_singleChatTurns >= kChatExtractEvery`)
- Pipeline: già attiva con ≥2 agenti (invariato)
- Risposta "NULLA" → skip silenzioso; label distingue "Chat: ..." vs "Pipeline: ..."
- Implementato in `agenti_page_knowledge.cpp::runKnowledgeExtract()`

## Feedback 👍/👎 (agenti_page_feedback.cpp)

Bottoni `fb:up:ID` / `fb:down:ID` nelle bolle agente → handler in `anchorClicked` → `saveFeedback()`.

```cpp
// prismalux_paths.h
P::feedbackPath()   // ~/.prismalux/feedback.jsonl
```

**Formato JSONL** (una riga per voto):
```json
{"timestamp":"2026-05-07T14:23:00","model":"qwen3:8b","prompt":"…","response":"…","rating":1}
```
- `rating`: `+1` = 👍 · `-1` = 👎
- `prompt`: bolla utente precedente (cerca per indice decrescente), fallback `m_taskOriginal`
- `response`: `m_bubbleTexts[bubbleIdx]`
- Feedback visivo: riga colorata inline nella chat (verde/rosso)

## Sistema Memoria Persistente (Knowledge) — 2026-05-06

Inietta il profilo utente in ogni system prompt AI senza che l'utente debba ripetersi.

**File dati**: `KNOWLEDGE_USER/user_knowledge.md` — max ~2000 token (~8000 char).
Non va in git (dati personali).

**Helper in `prismalux_paths.h`** (tutti inline, namespace `P`):
```cpp
P::userKnowledgePath()          // percorso assoluto del file
P::readUserKnowledge()          // legge con cache 30s — sicuro durante streaming
P::prependKnowledge(sys)        // prepende knowledge se kInjectUserKnowledge=true
P::invalidateKnowledgeCache()   // forza rilettura al prossimo accesso
```
**QSettings key**: `P::SK::kInjectUserKnowledge` (`"ai/injectUserKnowledge"`, default `true`)

**Punti di iniezione**:
- Pipeline / Byzantino / Chat RAG / ConsiglioScientifico: tramite `_buildSys()` in `agenti_page_math.cpp` che restituisce `P::prependKnowledge(sys)`
- `strumenti_page.cpp`, `programmazione_page.cpp`, `lavoro_page.cpp`: chiamata diretta `P::prependKnowledge(sys)` prima di ogni `m_ai->chat()`

**MCP aggiornamento** (`KNOWLEDGE_USER/MCP_UPDATE_DATA/server.py`, JSON-RPC 2.0 stdio):
- `update_knowledge(section, content, mode)` — aggiorna/sostituisce una sezione
- `auto_extract_and_update(summary)` — parsing prefissi `PREFERENZE:` / `PROGETTO:` / `PROCEDURA:` / `DECISIONE:` / `CONTESTO:`
- Rotazione automatica: `ragionamenti` max 15 voci, `contesto` max 10 voci

**UI**:
- Toggle "📖 Memoria persistente (Knowledge)" in ImpostazioniPage → AI Locale + bottone "Apri file"
- Bottone 📖 toolbar AgentiPage → `onSaveKnowledge()` (dialog sezione+testo+modalità → QProcess server.py)
- Estrattore nascosto fine pipeline → `runKnowledgeExtract()` in `agenti_page_knowledge.cpp` — attivo se knowledge ON e ≥2 agenti; riassume con prefissi fissi → `auto_extract_and_update`
- LanServer: endpoint `GET /knowledge` → contenuto file per PrismaluxMobile Android

## RAG condiviso tra agenti (2026-04-30)
- `AgentsConfigDialog::m_sharedRag` — `RagDropWidget` singolo sopra la griglia agenti
- `sharedRagWidget()` — accessor pubblico usato da `AgentiPage::runAgent()`
- In `runAgent()`: prima inietta `sharedRag->ragContext()`, poi `ragWidget(idx)->ragContext()`
- Ordine nel prompt: `[RAG condiviso] + [RAG specifico agente] + [output agenti precedenti]`
- Il RAG condiviso è visibile a tutti gli agenti; il RAG per-agente è specifico al ruolo

## Fix Windows (2026-04-30)
- `totalRamBytes()` in `prismalux_paths.h`: aggiunto ramo `#elif defined(Q_OS_WIN)` via `GlobalMemoryStatusEx()` — ora i modelli troppo grandi appaiono in rosso nella combo anche su Windows
- Messaggio fallback download Whisper (`agenti_page_stt.cpp`): su `Q_OS_WIN` mostra comandi PowerShell (`New-Item` + `Invoke-WebRequest`) invece di `mkdir -p` + `wget`
- `Avvia_Prismalux.bat` creato nella root: cerca l'exe in `COMPILE_WIN\build\` e `gui\build_win\` e lo lancia con working directory corretta

## Launcher Windows
- `build.bat` (root) → **compila** il sorgente (una tantum o dopo aggiornamenti)
- `Avvia_Prismalux.bat` (root) → **avvia** il programma già compilato (ogni volta)

## LAN Server Android (lan_server.h/cpp)

`LanServer` è un server TCP Qt che espone le API Ollama all'app PrismaluxMobile.

```cpp
LanServer* srv = new LanServer(m_ai, this);
srv->start(11500);      // avvia su porta 11500 (default)
srv->stop();
srv->isRunning();       // bool
srv->clientCount();     // int — conta SOLO IP unici che hanno usato le API (non browser)
srv->connectedIPs();    // QStringList
// segnali: statusChanged, clientConnected, clientDisconnected, requestHandled
```

**Pannelli UI:**
- **Strumenti AI → 📱 LAN Android** (`m_lanPanel` in `StrumentiPage`): toggle Start/Stop, QSpinBox porta, label IP locale, client connessi (solo APK), **due bottoni QR affiancati**.
- **Impostazioni → Ollama LAN**: avvia `ollama serve` con `OLLAMA_HOST=0.0.0.0:11434` via `QProcess`.

**Due bottoni QR nel pannello LAN Android** (`strumenti_page.cpp`):
| Bottone | URL | Azione sul telefono |
|---------|-----|---------------------|
| 📱 QR Scarica APK | `http://IP:PORT/apk` | Download diretto `PrismaluxMobile.apk` |
| 🌐 QR Pagina Download | `http://IP:PORT/` | Apre la pagina HTML con pulsante "⬇ Scarica APK Android" |

Entrambi abilitati solo con server attivo. Dialog: QR 260×260 + URL selezionabile + "📋 Copia URL".

**Endpoint HTTP serviti:**
| Path | Metodo | Risposta |
|------|--------|---------|
| `/api/tags` | GET | lista modelli Ollama (JSON) |
| `/api/chat` | POST | streaming NDJSON |
| `/api/generate` | POST | streaming NDJSON |
| `/knowledge` | GET/POST | contenuto `user_knowledge.md` |
| `/apk` | GET | serve `ANDROID/PrismaluxMobile.apk` (download diretto) |
| `/` `/download` `/index` | GET | pagina HTML benvenuto + pulsante "⬇ Scarica APK Android" |

**Contatore client — solo APK, non browser** (`m_appClientIps: QSet<QString>`):
- `clientConnected` emesso in `processSession()` solo per path API (`/api/*`, `/knowledge`)
- Browser che apre `/` o `/apk` non incrementa il contatore
- IP unici per sessione; reset a 0 allo stop del server

**SO_REUSEADDR — nessun errore al riavvio rapido:**
Su Linux/macOS, `start()` crea il socket manualmente con `SO_REUSEADDR` prima del `bind`, poi
passa il file descriptor a `QTcpServer::setSocketDescriptor()`. Evita "Address already in use"
dovuto al TIME_WAIT del kernel (~60s) dopo la chiusura del programma.
Su Windows: fallback al `QTcpServer::listen()` standard (comportamento invariato).

**Path APK** — usare sempre `P::root() + "/ANDROID/PrismaluxMobile.apk"` (con `/` iniziale).
Senza il `/`, la stringa si concatena al nome della cartella padre invece di entrarci.

**Due modalità complementari:**
| Modalità | Quando usarla |
|----------|--------------|
| LanServer (porta 11500) | Proxy Qt dentro Prismalux — controlla client, serve APK e pagina HTML |
| Ollama LAN (porta 11434) | Ollama esposto direttamente — più performante, nessun proxy |

## OpenCodePage — protocollo
OpenCode è un **sub-tab di APP Controller** (tab [4]), non un tab principale.

```
POST /session                  {title, cwd, model:{providerID,modelID}}
POST /session/{id}/message     {parts:[{type:"text",text:"..."}], model:{...}}
GET  /event                    SSE: data:{type,properties}
POST /session/{id}/abort       {}
```
SSE: `message.updated` (role→m_aiMsgId) · `message.part.updated` (token) · `session.idle` (fine) · `session.error` · `session.updated`

**Convenzioni OpenCodePage:**
- Porta: usa sempre `P::kOpenCodePort` — mai il valore 8092 hardcoded
- Errori di rete: retry counter con limite (max 5 tentativi), poi mostra errore e smette
- QTimer nel distruttore: `m_pollTimer->stop(); delete m_pollTimer;` prima di `stopServer()` — evita callback su oggetto già distrutto

**Miglioramenti pendenti:**
- Password/auth OpenCode (attualmente connette senza credenziali)
- Reconnection robusta dopo caduta del server (ora richiede riavvio manuale)
- Caching lista modelli (ora ri-richiesta a ogni apertura del tab)

## Suite di Test — Note Tecniche (2026-05-06)

**Build e run:**
```bash
cmake -B build_tests -DBUILD_TESTS=ON && cmake --build build_tests -j$(nproc)
ctest --test-dir build_tests -j4          # tutte le 26 suite (sicuro: RESOURCE_LOCK serializza Ollama)
ctest --test-dir build_tests -R NomeSuite # solo una suite
```
Stato corrente: **100% pass (33/33 suite no-Ollama)** — i 2 fallimenti (AiIntegration/AiStress) richiedono `mistral:7b-instruct` in Ollama.

**ATTENZIONE blocco sistema**: `AiIntegration`, `AiStress`, `TeamCollab` usano Ollama reale.
Con `-j4` senza `RESOURCE_LOCK` girano in parallelo e saturano RAM/CPU → freeze.
Fix applicato (2026-05-07): `RESOURCE_LOCK ollama` in CMakeLists — ctest li serializza automaticamente.

**HardwareMonitor — il thread non si avvia da solo:**
```cpp
HardwareMonitor mon;
mon.start();   // OBBLIGATORIO — il costruttore connette segnali ma NON avvia il thread
QSignalSpy spy(&mon, &HardwareMonitor::hwInfoReady);
QVERIFY(spy.wait(30000));
```
Senza `mon.start()`, `hwInfoReady` non viene mai emesso.

**AppController B-3/4/8/9 — pattern probe token:**
I 4 test che emettono token via `MockAiClient` dopo `activateSession()` possono incontrare una
race condition: il backend Ollama reale (MockAiClient eredita da AiClient con `"test-model"`)
risponde con errore prima che il test emetta il probe, distruggendo `m_tokenHolder`.
Soluzione applicata — dopo `activateSession()`:
```cpp
m_ai->emitToken("__PROBE__");
QApplication::processEvents();
if (!out->toPlainText().contains("__PROBE__"))
    QSKIP("m_tokenHolder già distrutto — richiede modello 'test-model' installato");
out->clear();
```

**ragChunkText — boundary condition:**
Con `chunkSize=600, overlap=80`, un testo di esattamente 600 char deve produrre **1 chunk**.
Fix applicato in `strumenti_page.cpp`: break immediato dopo il primo chunk se `pos + chunkSize >= t.size()`.
```cpp
while (pos < t.size()) {
    chunks << t.mid(pos, chunkSize);
    if (pos + chunkSize >= t.size()) break;  // ← evita chunk overlap-only finale
    pos += chunkSize - overlap;
}
```

**numbersFromText — regex negativi dopo virgola:**
Regex originale `(?<![,\d])` impediva di catturare `-3` in `"-5,-3,-1,1"` (la virgola prima del `-` falliva il lookbehind). Fix: `(?<!\d)` — blocca solo se preceduto da cifra, non da punteggiatura.

**ctest timeout HardwareMonitor:**
`set_tests_properties(HardwareMonitor PROPERTIES TIMEOUT 90)` — hw_detect + 2 thread paralleli
≈ 15-20 s totali sul sistema di sviluppo (Intel HD 630 + NVIDIA 1050 Mobile, driver parziale).
