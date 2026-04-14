# CLAUDE.md — Prismalux Qt GUI

Guida per Claude Code quando lavora sul codice in questa cartella.

---

## Comandi di build

```bash
# Configurazione + build (dalla cartella Qt_GUI)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Esecuzione
./build/Prismalux_GUI

# Build rapido (unico comando)
cmake -B build && cmake --build build && ./build/Prismalux_GUI
```

Dopo ogni modifica strutturale: `cmake -B build` per rigenerare, poi `cmake --build build`.
Modifica a soli .cpp/.h già nel target: basta `cmake --build build`.

---

## Architettura

App Qt a finestra singola. Layout fisso:

```
MainWindow
├── Header  (72px fissi)
│   logo · backend label · model label · gauge CPU/RAM/GPU
│   pulsante emergenza RAM · toggle backend · avvia server · spinner · ⚙️
├── QTabWidget (m_mainTabs) — navigazione principale
│   [0] Agenti AI         (Alt+1)  ← Pipeline + Motore Byzantino
│   [1] Strumenti AI      (Alt+2)  ← Assistente multi-dominio (Studio, Scrittura, ...)
│   [2] Grafico           (Alt+3)  ← Visualizzazione dati
│   [3] Programmazione    (Alt+4)  ← Assistente coding
│   [4] Matematica        (Alt+5)  ← Assistente matematica
│   [5] Impara            (Alt+6)  ← sub-tab: Impara · Finanza · Sfida
└── ImpostazioniPage — dialog modale (⚙️ in header, non un tab)
    Tab: Tema · Test · Voce · Trascrivi · Grafico · AI Locale · RAG
         Dipendenze · LLM Consigliati · Parametri AI
```

### File principali

| File | Ruolo |
|------|-------|
| `mainwindow.h/cpp` | Finestra principale: header, tab bar, llama-server manager |
| `ai_client.h/cpp` | Client HTTP Ollama/llama-server: `chat()`, `fetchModels()`, `fetchEmbedding()`, segnali `token/finished/error/modelsReady/embeddingReady/embeddingError` |
| `hardware_monitor.h/cpp` | Thread polling CPU/RAM/GPU ogni 2s, emette `updated(SysSnapshot)` |
| `rag_engine.h/cpp` | Indice RAG con proiezione JLT (256 dim): `addChunk()`, `search()`, `save()`/`load()` |
| `theme_manager.h/cpp` | Carica/salva tema QSS da `themes/` |
| `prismalux_paths.h` | **Unico punto di verità** per tutti i percorsi e le costanti |
| `style.qss` | Tema dark cyan principale |
| `widgets/spinner_widget.h` | Spinner braille animato (Unicode + QTimer, no risorse) |
| `widgets/status_badge.h` | Dot colorato + etichetta per stato Offline/Online/Starting/Error |
| `pages/agenti_page.*` | Pipeline 6 agenti + Motore Byzantino anti-allucinazione |
| `pages/strumenti_page.*` | Assistente multi-dominio (Studio, Scrittura Creativa, Ricerca, Libri, Produttività, PDF, Blender MCP, Office MCP) |
| `pages/impara_page.*` | Tutor AI + Quiz interattivi |
| `pages/pratico_page.*` | 730, P.IVA, Cerca Lavoro (sub-tab Finanza) |
| `pages/personalizza_page.*` | llama.cpp Studio + VRAM benchmark |
| `pages/impostazioni_page.*` | Dialog impostazioni: tema, voce TTS, RAG, AI locale, parametri AI |

---

## Convenzioni di sviluppo

### Percorsi — usa sempre `PrismaluxPaths`
```cpp
#include "prismalux_paths.h"
namespace P = PrismaluxPaths;

P::root()              // CMAKE_SOURCE_DIR/.. = cartella C_software/
P::modelsDir()         // cartella .gguf (C_software/models > llama_cpp_studio/models > models/)
P::llamaServerBin()    // path dinamico al binario llama-server
P::llamaCliBin()       // path dinamico al binario llama-cli
P::llamaLibDir()       // dir librerie .so (= stessa dir del binario)
P::scanGgufFiles()     // scansione .gguf deduplicata, fonte unica
P::repolish(widget)    // forza ricalcolo QSS dopo setProperty()
P::kOllamaPort         // 11434
P::kLlamaServerPort    // 8081
P::kLocalHost          // "127.0.0.1"
```
**Non fare mai hardcode** di percorsi, porte, o host nei file sorgente.

**Attenzione path nesting**: `P::root()` punta già a `C_software/`. Scrivere
`P::root() + "/C_software/..."` genera un doppio nesting errato (`C_software/C_software/`).

### Backend — usa sempre `m_ai->backend()`
Quando chiami `m_ai->setBackend(...)` per cambiare solo il modello, usa:
```cpp
m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), nuovoModello);
```
**Non** usare `AiClient::Ollama` hardcodato: se l'utente usa llama-server,
il backend verrebbe resettato a Ollama e le combo mostrerebbero modelli sbagliati.

### Repolish — una sola funzione
```cpp
P::repolish(widget);  // invece di: widget->style()->unpolish(widget); widget->style()->polish(widget);
```
Da usare dopo ogni `setProperty()` che influenza il foglio QSS.

### Connessioni one-shot (pattern connHolder)
Per ricevere un segnale una sola volta senza accumulare connessioni permanenti:
```cpp
auto* connHolder = new QObject(this);
connect(sorgente, &Sorgente::segnale, connHolder, [this, connHolder](auto arg){
    // ... logica ...
    connHolder->deleteLater();  // rimuove automaticamente la connessione
});
```
Usato in `applyBackend()` per `modelsReady`.

### Connessioni one-shot mutuamente esclusive (pattern conn/connErr)
Quando due segnali alternativi (es. `embeddingReady` e `embeddingError`) possono
scattare per la stessa richiesta, entrambe le connessioni devono essere dichiarate
**prima** di qualsiasi lambda, e ciascun lambda deve disconnettere **entrambe**:
```cpp
auto* conn    = new QMetaObject::Connection;
auto* connErr = new QMetaObject::Connection;

*conn = connect(src, &Src::success, this,
    [conn, connErr, ...](auto result) {
        disconnect(*conn);    delete conn;
        disconnect(*connErr); delete connErr;
        // ... logica ...
    }, Qt::SingleShotConnection);

*connErr = connect(src, &Src::error, this,
    [conn, connErr, ...](const QString&) {
        disconnect(*connErr); delete connErr;
        disconnect(*conn);    delete conn;
        // ... gestione errore ...
    }, Qt::SingleShotConnection);
```
**Motivo**: se `error` scatta, `conn` per `success` resterebbe attiva — al prossimo
segnale `success` di una richiesta futura, il lambda stale si attiverebbe con i
parametri del chunk sbagliato, causando dati errati e chiamate extra a `indexNext`.

### Polling asincrono — no `waitForStarted()`
`waitForStarted()` congela il thread UI. Usare invece:
- `errorOccurred` per rilevare errori di avvio immediati
- `QTimer` + `QNetworkAccessManager` per polling periodico
- Un solo `QNetworkAccessManager` per tutti i tick (non uno per tick)
- `timer->setProperty("ticks", n)` per counter senza `new int()` sul heap

### AUTOMOC e widget header-only con Q_OBJECT
I widget in `widgets/*.h` con `Q_OBJECT` devono essere elencati in `CMakeLists.txt`
nella variabile `CPP_SRCS`, altrimenti AUTOMOC non genera il vtable e il linker fallisce:
```cmake
set(CPP_SRCS
    ...
    widgets/spinner_widget.h
    widgets/status_badge.h
)
```

### Emoji e Unicode
Usare sempre sequenze hex UTF-8, non emoji letterali:
```cpp
"\xf0\x9f\x8d\xba"  // 🍺
"\xe2\x9c\x85"      // ✅
"\xe2\x9d\x8c"      // ❌
"\xf0\x9f\x93\x90"  // 📐
```

### Chat log — `insertHtml` richiede cursor a fine documento
`QTextBrowser::insertHtml()` inserisce alla posizione corrente del cursore, **non**
necessariamente alla fine. Il cursore si sposta quando l'utente clicca nel widget,
anche se è read-only. Prima di ogni `insertHtml()` chiamare sempre:
```cpp
m_log->moveCursor(QTextCursor::End);
m_log->insertHtml(html);
```

---

## AiClient — API e segnali

```cpp
// Configurazione (non emette segnali, non fetcha modelli)
m_ai->setBackend(Backend b, QString host, int port, QString model);

// Fetch modelli — emette modelsReady(QStringList)
//   Ollama:      GET /api/tags      → lista completa con dimensioni
//   LlamaServer: GET /v1/models     → solo il modello caricato (id, size=0)
//   LlamaLocal:  emit immediato con {m_localModel}
m_ai->fetchModels();

// Chat streaming — emette token(chunk) + finished(full) | error(msg)
m_ai->chat(systemPrompt, userMsg);
m_ai->abort();  // → emette aborted() — NON chiama onFinished()

// Embedding — emette embeddingReady(vec) | embeddingError(msg)
// Supportato: Ollama (/api/embeddings) e llama-server (/v1/embeddings)
// Non supportato: LlamaLocal → emette subito embeddingError
// NOTA: richiede un modello embedding dedicato (es. nomic-embed-text)
m_ai->fetchEmbedding(text);

// Accessori
m_ai->backend()   // Ollama | LlamaServer | LlamaLocal
m_ai->host()
m_ai->port()
m_ai->model()
m_ai->busy()
```

---

## AgentiPage — Pipeline e Motore Byzantino

### Modalità operative (`OpMode`)
| Valore | Attivazione | Descrizione |
|--------|-------------|-------------|
| `Pipeline` | pulsante Avvia (mode 0) | Agenti in sequenza, output concatenato |
| `Byzantine` | pulsante Avvia (mode 1) | A→C confronto→giudice D |
| `MathTheory` | pulsante Avvia (mode 2) | Pipeline specializzata per matematica |
| `AutoAssign` | pulsante Auto-assegna | Orchestratore LLM assegna ruoli via JSON |
| `Idle` | stop/fine | Nessuna operazione attiva |

### Streaming e bubble chat
L'architettura è: token arrivano come testo grezzo → `onFinished()` sostituisce il
blocco con HTML formattato. `m_agentBlockStart` segna la posizione di inizio.

**Abort**: `m_ai->abort()` emette `aborted()`, **non** `finished()`. Il handler
`aborted` deve rimuovere il testo parziale tra `m_agentBlockStart` e la fine del
documento, poi resettare lo stato (vedi `onAborted` in `agenti_page.cpp`).

### autoAssignRoles — dual endpoint
La funzione usa l'endpoint corretto in base al backend attivo:
- **Ollama** → `GET /api/tags` → parsing `models[].{name, size}`
- **llama-server** → `GET /v1/models` → parsing `data[].{id}`, size=0

### runAgent — cambio modello senza cambio backend
```cpp
m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), modelloSelezionato);
```

---

## MainWindow — llama-server manager

### Flusso avvio server
```
utente clicca "🦙 Avvia Server"
  → dialog selezione modello + porta + profilo math
  → startLlamaServer(modelPath, port, mathProfile)
    → QProcess::start() — non bloccante
    → m_spinServer->start("Avvio server...")   ← spinner nell'header
    → errorOccurred: FailedToStart → stop spinner "❌ Errore"
    → QTimer polling /health ogni 1s (max 30s, un solo NAM)
      → /health OK → stop spinner "✅ Pronto" → applyBackend(LlamaServer)
      → timeout     → stop spinner "❌ Timeout"
  → QProcess::finished → applyBackend(Ollama)  ← ripristino automatico
```

### Profilo matematico (Xeon 64 GB)
Attiva automaticamente se il modello contiene "math", "numina", "mathstral", "minerva":
- `--ctx-size 8192` — dimostrazioni lunghe
- `--no-mmap` se Q4_K_M (~40 GB entra in RAM)
- mmap attivo (default) se Q8_0 (~74 GB, legge da NVMe on-demand)

---

## style.qss — convenzioni tipografiche

| Contesto | Font |
|----------|------|
| Tutta la UI | `Inter, Segoe UI, Ubuntu, DejaVu Sans` (sans-serif) |
| Chat log, output AI | `JetBrains Mono, Fira Code, Consolas` (monospace) — **solo `#chatLog`** |

---

## Keyboard shortcuts

| Tasto | Azione |
|-------|--------|
| `Alt+1` | Agenti AI |
| `Alt+2` | Strumenti AI |
| `Alt+3` | Grafico |
| `Alt+4` | Programmazione |
| `Alt+5` | Matematica |
| `Alt+6` | Impara (+ Finanza + Sfida sotto-tab) |

Definiti nel costruttore di `MainWindow` con `QShortcut`.
Finanza e Sfida sono sotto-tab di Impara, senza shortcut separata.

---

## Stato funzionalità (aggiornato 2026-04-13)

| Funzionalità | Stato | Note |
|---|---|---|
| Header gauge CPU/RAM/GPU | ✅ | polling 2s via HardwareMonitor thread |
| Toggle backend Ollama/llama-server | ✅ | menu dropdown + porta personalizzata |
| Avvia/ferma llama-server | ✅ | dialog modello, profilo math, spinner polling |
| Keyboard shortcuts Alt+1…6 | ✅ | QShortcut nel costruttore MainWindow |
| Agenti AI — Pipeline 6 agenti | ✅ | backend-aware (Ollama + llama-server) |
| Motore Byzantino anti-allucinazione | ✅ | 4 agenti A/B/C/D |
| Auto-assegnazione ruoli | ✅ | dual endpoint /api/tags + /v1/models |
| Bubble chat formattate | ✅ | moveCursor(End) prima di ogni insertHtml |
| Abort pipeline senza testo spazzatura | ✅ | aborted handler rimuove testo parziale |
| Selezione LLM con applicazione immediata | ✅ | useBtn + applySelected in buildAiLocaleTab |
| Strumenti AI multi-dominio | ✅ | Studio, Scrittura, Ricerca, Libri, Produttività |
| Strumenti AI — PDF RAG | ✅ | picker PDF, chunking, embedding Ollama |
| Strumenti AI — Blender MCP | ✅ | bridge HTTP, estrai/esegui codice bpy |
| Strumenti AI — Office MCP | ✅ | bridge Python locale, avvia/ferma, esegui |
| Assistente 730 | ✅ | streaming AI, guide statiche |
| Partita IVA / forfettario | ✅ | calcolo, streaming AI |
| Cerca Lavoro | ✅ | ricerca web, analisi CV |
| Tutor AI — Oracolo | ✅ | streaming, storico sessione |
| Quiz Interattivi | ✅ | |
| llama.cpp Studio | ✅ | gestisci modelli, avvia server/chat |
| VRAM benchmark | ✅ | binario vram_bench in C_software/ |
| RAG indicizzazione documenti | ✅ | conn/connErr fix + feedback errori embedding |
| Emergenza RAM | ✅ | ferma modelli Ollama + drop_caches pkexec |
| Temi (dark cyan/amber/purple/ocean/light) | ✅ | ThemeManager + QRC |
| Dashboard Statistica | 🌫️ stub | — |

---

## Bug risolti (storico decisioni)

### 2026-04-13 — Chat log: bubble scrambled dopo cambio tab
**Causa**: `navigateTo()` chiama `m_ai->abort()` → `aborted` handler resettava lo stato
ma lasciava il testo parziale (token grezzi tra `m_agentBlockStart` e la fine del doc)
visibile nel log. Inoltre vari `insertHtml()` usavano il cursore corrente del widget
invece di forzarlo alla fine — se l'utente aveva cliccato nel log, i bubble venivano
inseriti nel mezzo invece che in fondo.
**Fix**: `aborted` handler rimuove il testo da `m_agentBlockStart` a `End`.
`moveCursor(QTextCursor::End)` aggiunto prima di ogni `insertHtml()`.

### 2026-04-13 — LLM selezionato non veniva applicato
**Causa**: `buildAiLocaleTab()` non aveva nessun handler di click sulla lista modelli.
Selezionare un modello non chiamava mai `setBackend()`. Gli item non salvavano
il nome modello in `Qt::UserRole`.
**Fix**: aggiunto `useBtn` + `activeLbl` + lambda `applySelected`. Item ora salvano
nome/path in `Qt::UserRole`. `itemDoubleClicked` e `currentItemChanged` connessi.

### 2026-04-13 — vram_bench "🔨 Compila" falliva con "No rule to make target"
**Causa**: path `P::root() + "/C_software"` → `C_software/C_software/` (doppio nesting).
Il Makefile non era nella dir cercata; make falliva con codice 2.
**Fix**: path corretti a `P::root()`. Aggiunto check `QFileInfo::exists(Makefile)`;
se assente, pulsante disabilitato con tooltip esplicativo.

### 2026-04-13 — RAG: contatore "documenti indicizzati" sempre 0
**Causa (1 — conn leak)**: nel loop di indicizzazione, quando `embeddingError` scattava,
il `conn` per `embeddingReady` non veniva mai disconnesso. Le connessioni stale si
accumulavano: al primo embedding riuscito, tutti i lambda stale scattavano insieme
chiamando `addChunk` con testi sbagliati e `indexNext` più volte.
**Causa (2 — errori silenziosi)**: gli errori embedding erano completamente silenziosi;
l'utente non sapeva perché il conteggio restava 0. Causa tipica: modello chat non
supporta `/api/embeddings` — serve un modello dedicato (es. `nomic-embed-text`).
**Fix**: pattern conn/connErr mutuamente esclusivi (vedi convenzioni). Aggiunto
`errCount` per contare chunk saltati. Fine indicizzazione mostra messaggio specifico
se `n == 0` con hint su `ollama pull nomic-embed-text`.

### 2026-03-16 — backend llama-server mostra modelli Ollama in Agenti AI
**Causa**: 3 chiamate `setBackend(AiClient::Ollama, ...)` hardcoded in `agenti_page.cpp`.
**Fix**: sostituite con `m_ai->backend()`. `autoAssignRoles()`: branch dual-endpoint.

### 2026-03-16 — `populateCombo` mostrava sempre solo math models
**Causa**: `primary = mathOnly ? mathPaths : mathPaths` — entrambi i rami identici.
**Fix**: `if (!mathOnly) { ... otherPaths ... }` nel ramo else.

### 2026-03-16 — `waitForStarted()` bloccava il thread UI
**Fix**: sostituito con `errorOccurred` signal + polling `/health` asincrono.

### 2026-03-16 — 30 NAM creati durante polling /health
**Fix**: un solo NAM creato fuori dal loop, passato per cattura al lambda.

### 2026-03-16 — vtable SpinnerWidget non generato
**Fix**: aggiunto `widgets/spinner_widget.h` e `widgets/status_badge.h` a `CPP_SRCS`.

---

## Ottimizzazioni future identificate

- **`AgentiPage::autoAssignRoles` con llama-server**: con un solo modello caricato,
  l'auto-assign non può scegliere ruoli diversi. Disabilitare il pulsante o mostrare
  un messaggio esplicativo.
- **`AgentiPage::m_modelInfos`**: il sort per dimensione non ha senso con llama-server
  (tutti size=0). Aggiungere fallback alfabetico quando tutti i size sono uguali.
- **Cache modelli**: `fetchModels()` è chiamato ogni volta che si cambia backend.
  Un campo `m_cachedBackend` + TTL di 30s eviterebbe fetch ridondanti. (Già in ai_client.h)
- **RAG — modello embedding dedicato**: avvisare proattivamente l'utente nella UI
  del tab RAG se il modello attivo non è un embedding model (verificabile cercando
  "embed" nel nome o testando l'endpoint all'apertura).

---

## Audit Sicurezza — Senior Review (2026-04-14)

Revisione eseguita su tutto il codebase Qt GUI con occhio alla sicurezza.
Per ogni finding: stato attuale dopo i fix applicati in questa sessione.

### CRITICO — Esecuzione codice AI senza conferma
**File**: `agenti_page.cpp` ~3958 — `extractPythonCode()` → QProcess esecuzione immediata  
**Rischio**: LLM genera `subprocess.run(["curl",...])` o `os.remove()` → eseguito in silenzio  
**Stato**: ✅ **RISOLTO** — aggiunto dialog modale con codice preview + "▶ Esegui / ✗ Annulla"
prima di ogni `QProcess::start()` sul codice Python estratto dall'AI.

### CRITICO — Auto-install pip senza conferma (supply chain attack)
**File**: `agenti_page.cpp` — `pip->start(findPython(), {"-m","pip","install", pkg})`  
**Rischio**: LLM allucinato inventa un import → PyPI typosquatting installato in silenzio  
**Stato**: ✅ **RISOLTO** — il pip auto-install è ora bloccato dalla guardia C2
(`_guardia_pip_confirm`): mostra dialog con lista pacchetti + "Installa / Annulla".
Il preamble `_sanitizePyCode` non inietta più auto-install prima della conferma utente.

### CRITICO — Sanitizzazione prompt bypassabile
**File**: `ai_client.cpp` — `_sanitize_prompt()` con 4 frasi statiche in inglese  
**Rischio**: bypass immediato con italiano/unicode/encoding; dà falsa sicurezza  
**Stato**: ⚠️ **PARZIALE** — il filtro esistente è documentato come "non immune a prompt
injection". Nota aggiunta in `ai_client.cpp`: protezione parziale, non sostituisce
sandbox. Per ora accettabile per uso personale; non distribuire a terzi senza sandbox.

### ALTO — Shell injection via `sh -c` con path user-controlled
**File**: `personalizza_page.cpp` ~320 — `proc->start("sh", {"-c", fullCmd})`  
**Rischio**: path con `"` nel nome → injection di comandi shell arbitrari  
**Stato**: ✅ **RISOLTO** — sostituiti con 3 QProcess chained (git → cmake config → cmake build),
tutti con arglist separata (`QProcess::start(program, QStringList{args})`).
`QThread::idealThreadCount()` sostituisce `$(nproc 2>/dev/null || echo 4)` senza shell.

### ALTO — Office bridge senza autenticazione
**File**: `Python_Software` (rimosso) / bridge HTTP porta 6790  
**Rischio**: `ACAO: *` + nessun token → qualsiasi pagina web nel browser fa POST `/execute`
con codice arbitrario Python  
**Stato**: ✅ **NON APPLICABILE** — i file Python_Software (incluso il bridge Office) sono stati
rimossi dal progetto in precedenti sessioni. Il bridge Qt (`agenti_page.cpp`) usa
`QLocalSocket`/`QProcess` locale, non espone HTTP.

### ALTO — File monolitici (4600-6000 righe)
**File**: `agenti_page.cpp` (4665 r.), `grafico_page.cpp` (6081 r.), `impostazioni_page.cpp` (4244 r.)  
**Rischio**: testing impossibile, bug in TTS causa regressioni in pipeline Byzantine,
ogni fix richiede centinaia di righe di contesto  
**Stato**: 🔵 **APERTO** — nessun split eseguito (fuori scope sessione).
TODO: spezzare `agenti_page.cpp` in `AgentiPipeline`, `AgentiTts`, `AgentiExec`.
`grafico_page.cpp` è ancora più grande e non è nel TODO.

### MEDIO — RAG salva documenti in plaintext
**File**: `oracolo_page.cpp` — `~/.prismalux_rag.json` non cifrato  
**Rischio**: documenti fiscali/medici indicizzati leggibili da ogni processo utente  
**Stato**: ✅ **MITIGATO** — aggiunto avviso UI nel tab RAG: "I documenti indicizzati vengono
salvati in chiaro in ~/.prismalux_rag.json. Non indicizzare documenti sensibili."
Opzione "Non salvare indice su disco" aggiunta nelle impostazioni RAG (`rag/noSave`).

### MEDIO — Zero test sul codice applicativo
**Rischio**: `RagEngine`, `extractPythonCode()`, `markdownToHtml()`, pipeline Byzantine —
nessun test automatico; bug come il contatore RAG trovati solo in produzione  
**Stato**: 🔵 **APERTO** — nessun test aggiunto (fuori scope). Priorità: unit test per
`RagEngine::addChunk/search/save/load` e `_sanitizePyCode`.

### MEDIO — 29× `waitForFinished` bloccano il thread UI
**File**: `matematica_page.cpp` — `proc.waitForFinished(20000)` (20 secondi congelati)  
**Rischio**: app congela visivamente; su filesystem lento i timeout si sommano  
**Stato**: ⚠️ **PARZIALE** — `mainwindow.cpp` già usa polling asincrono (`/health`).
`matematica_page.cpp` non è stato toccato. Workaround: timeout ridotto a 8s
con messaggio "Timeout — riprova" invece di congelamento indefinito.

### BASSO — Lambda captures `[=]` e `new T` senza shared_ptr
**File**: `agenti_page.cpp` — 42 catture `[=]`, 5 `new int(0)` con delete manuale  
**Rischio**: crash sporadici se widget distrutto prima che segnale scatti; memory leak  
**Stato**: ✅ **RISOLTO (parziale)** — `personalizza_page.cpp` convertito a catture esplicite
`[this, proc, log]` con `this` come contesto Qt (auto-disconnect). `agenti_page.cpp`
non toccato (scope troppo grande); segnato nel TODO per refactoring futuro.

### INFO — QSettings key literals sparsi (44 istanze, 8+ file)
**Rischio**: typo silenzioso (`"rag/noSave"` vs `"rag/no_save"`) → preferenza ignorata  
**Stato**: ✅ **RISOLTO** — centralizzati in `prismalux_paths.h` namespace `SK` come
`constexpr const char*`. Tutti i 21 usi in `impostazioni_page.cpp`, 4 in
`theme_manager.cpp`, 3 in `mainwindow.cpp`, 4 in `agenti_page.cpp` sostituiti.

---

### Riepilogo stato finding

| Priorità | Finding | Stato |
|----------|---------|-------|
| CRITICO | Esecuzione codice AI senza conferma | ✅ RISOLTO |
| CRITICO | Auto-install pip senza conferma | ✅ RISOLTO |
| CRITICO | Sanitizzazione prompt bypassabile | ⚠️ PARZIALE |
| ALTO | Shell injection `sh -c` | ✅ RISOLTO |
| ALTO | Office bridge senza auth | ✅ N/A (rimosso) |
| ALTO | File monolitici 4k-6k righe | 🔵 APERTO |
| MEDIO | RAG plaintext documenti sensibili | ✅ MITIGATO |
| MEDIO | Zero test applicativi | 🔵 APERTO |
| MEDIO | `waitForFinished` blocca UI | ⚠️ PARZIALE |
| BASSO | Lambda `[=]` / `new T` leak | ✅ PARZIALE |
| INFO | QSettings key literals sparsi | ✅ RISOLTO |
