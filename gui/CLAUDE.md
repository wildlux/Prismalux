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
[0] 🤖 Intelligenza Artificiale  Alt+1  Pipeline + Byzantino + CHAT RAG
[1] 🛠 Strumenti AI              Alt+2  Studio, Scrittura, Ricerca, 💼 Cerca Lavoro, Libri, Produttività, Documenti
[2] 💻 Programmazione            Alt+3  Editor + sub-tab Agentica
[3] π  Matematica                Alt+4  Matematica · Grafico
[4] 🔬 Ricerca                   Alt+5  Paper · Brevetti · Documenti tecnici
[5] 🕹 APP Controller            Alt+6  Blender/Office/Anki/KiCAD/TinyMCP/OpenCode
[6] 📚 Impara                    Alt+7  Finanza · Impara con AI · Sfida te stesso
ImpostazioniPage: dialog modale (⚙️ header)
```
Note: Cerca Lavoro è in Strumenti AI (cat 3, tra Ricerca e Libri) — attivata da lavoroBtn (checkable).
LavoroPage è istanziata dentro StrumentiPage (m_lavoroPage), NON in mainwindow.cpp.

## File chiave
| File | Ruolo |
|------|-------|
| `mainwindow.h/cpp` | Header, tab bar, llama-server manager |
| `ai_client.h/cpp` | HTTP Ollama/llama-server — `chat()`, `fetchModels()`, `fetchEmbedding()` |
| `prismalux_paths.h` | **Unico punto di verità** per path, porte, costanti QSettings |
| `rag_engine.h/cpp` | RAG JLT 256-dim — `addChunk()`, `search()`, `save()/load()` |
| `hardware_monitor.h/cpp` | Thread polling CPU/RAM/GPU ogni 2s |
| `pages/agenti_page.*` | Pipeline 6 agenti + Byzantino (12 moduli) |
| `pages/opencode_page.*` | OpenCode serve HTTP+SSE — port 8092 |

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

## AiClient — API
```cpp
m_ai->setBackend(Backend, host, port, model);  // non emette segnali
m_ai->fetchModels();           // → modelsReady(QStringList)
m_ai->chat(sys, msg);          // → token(chunk) + finished(full) | error(msg)
m_ai->abort();                 // → aborted() — NON chiama onFinished()
m_ai->fetchEmbedding(t);       // → embeddingReady(vec) | embeddingError(msg)
m_ai->setNumGpu(n);            // n≥0 → invia num_gpu=n; n<0 → non inviato (Ollama auto)
m_ai->fetchModelLayers(cb);    // → cb(int layers) via /api/show
m_ai->unloadModel();           // DELETE /api/show con keep_alive=0
// .backend() .host() .port() .model() .busy() .numGpu()
```

## Modalità Calcolo LLM (manutenzione_page.cpp)
Quattro bottoni: **GPU** · **CPU** · **Misto** · **Doppia GPU**. Salvato in `QSettings(P::SK::kComputeMode)` + `PRISMALUX_COMPUTE_MODE` env.

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

## AgentiPage
- `m_modePipeline=false` → CHAT RAG (1 agente); `true` → pipeline
- `m_pageModel` (QString privato): isola la scelta LLM da `modelChanged`
- Invio = modalità corrente · Stop da fermo = toggle CHAT↔Avvia
- `abort()` → `onAborted` rimuove testo da `m_agentBlockStart` a End
- qwen3.5 rimosso da `s_knownBroken` (fix 2026-04-24) — nessun workaround necessario

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
