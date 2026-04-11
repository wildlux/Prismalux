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
├── Sidebar (210px fissi)
│   navBtn[0] Agenti AI   (Alt+1)
│   navBtn[1] Finanza     (Alt+2)
│   navBtn[2] Impara      (Alt+3)
│   ─────────
│   versione
└── QStackedWidget
    ├── [0] AgentiPage        ← Pipeline + Motore Byzantino
    ├── [1] PraticoPage       ← 730, P.IVA, Cerca Lavoro
    ├── [2] ImparaPage        ← Tutor AI + stub Quiz/Dashboard
    └── [3] ImpostazioniPage  ← Backend, Modello, llama.cpp Studio, HW info
```

### File principali

| File | Ruolo |
|------|-------|
| `mainwindow.h/cpp` | Finestra principale: header, sidebar, stack, llama-server manager |
| `ai_client.h/cpp` | Client HTTP Ollama/llama-server: `chat()`, `fetchModels()`, segnali `token/finished/error/modelsReady` |
| `hardware_monitor.h/cpp` | Thread polling CPU/RAM/GPU ogni 2s, emette `updated(SysSnapshot)` |
| `theme_manager.h/cpp` | Carica/salva tema QSS da `themes/` |
| `prismalux_paths.h` | **Unico punto di verità** per tutti i percorsi e le costanti |
| `style.qss` | Tema dark cyan principale |
| `widgets/spinner_widget.h` | Spinner braille animato (Unicode + QTimer, no risorse) |
| `widgets/status_badge.h` | Dot colorato + etichetta per stato Offline/Online/Starting/Error |
| `pages/agenti_page.*` | Pipeline 6 agenti + Motore Byzantino anti-allucinazione |
| `pages/pratico_page.*` | 730, P.IVA, Cerca Lavoro |
| `pages/impara_page.*` | Tutor AI + stub Quiz/Dashboard |
| `pages/personalizza_page.*` | llama.cpp Studio: compila, gestisci modelli, avvia server/chat |
| `pages/impostazioni_page.*` | Selezione backend/modello, info HW |

---

## Convenzioni di sviluppo

### Percorsi — usa sempre `PrismaluxPaths`
```cpp
#include "prismalux_paths.h"
namespace P = PrismaluxPaths;

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
m_ai->abort();  // → emette aborted()

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

### autoAssignRoles — dual endpoint (bug fix 2026-03-16)
La funzione usa l'endpoint corretto in base al backend attivo:
- **Ollama** → `GET /api/tags` → parsing `models[].{name, size}`
- **llama-server** → `GET /v1/models` → parsing `data[].{id}`, size=0

Con llama-server c'è sempre un solo modello caricato; l'orchestratore usa quello.
Il `setBackend()` al termine usa `m_ai->backend()` (non hardcoded Ollama) per non
resettare il backend scelto dall'utente.

### runAgent — cambio modello senza cambio backend
Ogni agente nella pipeline può usare un modello diverso. Il cambio si fa con:
```cpp
m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), modelloSelezionato);
```
Il backend e l'host rimangono invariati, cambia solo il campo `model`.

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

Il cambio da monospace globale a sans-serif selettivo è stato fatto nella v2
del tema (2026-03-16) per migliorare la leggibilità dell'interfaccia.

---

## Keyboard shortcuts

| Tasto | Azione |
|-------|--------|
| `Alt+1` | Agenti AI |
| `Alt+2` | Finanza Personale |
| `Alt+3` | Impara con AI |

Definiti nel costruttore di `MainWindow` con `QShortcut`.
I tooltip dei navBtn mostrano la shortcut corrispondente.

---

## Stato funzionalità (aggiornato 2026-03-16)

| Funzionalità | Stato | Note |
|---|---|---|
| Header gauge CPU/RAM/GPU | ✅ | polling 2s via HardwareMonitor thread |
| Toggle backend Ollama/llama-server | ✅ | menu dropdown + porta personalizzata |
| Avvia/ferma llama-server | ✅ | dialog modello, profilo math, spinner polling |
| Spinner avvio server | ✅ | braille Unicode, nessuna risorsa grafica |
| Keyboard shortcuts Alt+1/2/3 | ✅ | QShortcut nel costruttore MainWindow |
| Agenti AI — Pipeline 6 agenti | ✅ | backend-aware (Ollama + llama-server) |
| Motore Byzantino anti-allucinazione | ✅ | 4 agenti A/B/C/D |
| Auto-assegnazione ruoli | ✅ | dual endpoint /api/tags + /v1/models |
| Modelli matematici (Qwen2.5-Math) | ✅ | download HF, Q4/Q8, flags ottimali Xeon |
| Assistente 730 | ✅ | streaming AI, guide statiche |
| Partita IVA / forfettario | ✅ | calcolo, streaming AI |
| Cerca Lavoro | ✅ | ricerca web, analisi CV |
| Tutor AI — Oracolo | ✅ | streaming, storico sessione |
| llama.cpp Studio | ✅ | compila, gestisci modelli, avvia server/chat |
| Emergenza RAM 🚨 | ✅ | ferma modelli Ollama + drop_caches pkexec |
| Temi (dark cyan/amber/purple/light) | ✅ | ThemeManager + QRC |
| Quiz Interattivi | 🌫️ stub | — |
| Dashboard Statistica | 🌫️ stub | — |
| Cython Studio | 🌫️ stub | — |

---

## Bug risolti (storico decisioni)

### 2026-03-16 — backend llama-server mostra modelli Ollama in Agenti AI
**Causa**: 3 chiamate `setBackend(AiClient::Ollama, ...)` hardcoded in `agenti_page.cpp`:
- `autoAssignRoles()` → usava sempre `/api/tags` (Ollama) ignorando il backend attivo
- `parseAutoAssign()` → forzava Ollama dopo l'auto-assign
- `runAgent()` → forzava Ollama a ogni passo della pipeline

**Effetto**: passando a llama-server, le combo si svuotavano o mostravano i vecchi
modelli Ollama. Durante l'esecuzione il backend tornava silenziosamente a Ollama.

**Fix**: sostituito tutte e 3 le occorrenze con `m_ai->backend()`.
Per `autoAssignRoles()` aggiunto branch dual-endpoint con parsing JSON differenziato.

### 2026-03-16 — `populateCombo` mostrava sempre solo math models
**Causa**: `primary = mathOnly ? mathPaths : mathPaths` — entrambi i rami identici.
**Fix**: `if (!mathOnly) { ... otherPaths ... }` nel ramo else.

### 2026-03-16 — `waitForStarted()` bloccava il thread UI
**Causa**: `QProcess::waitForStarted()` è bloccante fino a 4s.
**Fix**: sostituito con `errorOccurred` signal (non bloccante) + polling `/health` asincrono.

### 2026-03-16 — 30 NAM creati durante polling /health
**Causa**: `new QNetworkAccessManager` dentro il lambda del timer (un per tick).
**Fix**: un solo NAM creato fuori dal loop, passato per cattura al lambda.

### 2026-03-16 — `new int(0)` per counter polling
**Causa**: allocazione heap manuale, rischio leak se il timer viene distrutto.
**Fix**: `timer->setProperty("ticks", 0)` — il counter vive nel QObject stesso.

### 2026-03-16 — vtable SpinnerWidget non generato
**Causa**: `widgets/spinner_widget.h` con `Q_OBJECT` non era in `CPP_SRCS` del CMakeLists.
AUTOMOC processa solo file esplicitamente elencati come sorgenti del target.
**Fix**: aggiunto `widgets/spinner_widget.h` e `widgets/status_badge.h` a `CPP_SRCS`.

---

## Ottimizzazioni future identificate

### Alta priorità
- **`AgentiPage::autoAssignRoles` con llama-server**: quando llama-server ha un solo
  modello, l'auto-assign non ha senso (non può scegliere modelli diversi per ruolo).
  Disabilitare il pulsante "Auto-assegna" o mostrare un messaggio esplicativo.
- **Cache modelli**: `fetchModels()` è chiamato ogni volta che si cambia backend.
  Un campo `m_lastBackend` + TTL di 30s eviterebbe fetch ridondanti.
- **`navigateTo()` con animazione**: il cambio pagina è istantaneo. Un fade breve
  (QGraphicsOpacityEffect) migliorerebbe la UX senza impatto sulle prestazioni.

### Media priorità
- **`AgentiPage::m_modelInfos`**: il sort per dimensione non ha senso con llama-server
  (tutti size=0). Aggiungere fallback alfabetico quando tutti i size sono uguali.
- **`StatusBadge` nell'header**: il widget esiste ma non è ancora usato. Aggiungere
  un badge "● Online"/"● Avvio" accanto a m_lblBackend per lo stato del server.
- **Tooltip ricchi sui gauge**: mostrare GB usati/totali RAM e nome GPU nel tooltip
  della ResourceGauge (i dati arrivano già da `SysSnapshot`).

### Bassa priorità
- **`buildModelBar()` port 8080**: in `impostazioni_page.cpp` c'è ancora un 8080 hardcoded.
  Sostituire con `P::kLlamaServerPort`.
- **Typo**: `buildCythoStudio` → `buildCythonoStudio` in `personalizza_page.cpp`.
- **`generateQuestion()` in ImparaPage**: variabili `static` di connessione rischiano
  race condition se la funzione viene chiamata prima che la precedente termini.
  Usare un flag `m_generating` (già presente come `m_busy` nel design originale).
