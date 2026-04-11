# GUI Best Practice & Ottimizzazioni — Prismalux Qt6

## Principi generali (stile ChatGPT / minimal dark)

- **Sidebar** = solo history conversazioni + ⚙️ Impostazioni in fondo. Nessun navBtn per sezioni.
- **Sezioni principali** = tab orizzontali nel corpo centrale (non nella sidebar).
- **Dialog** = usare `Qt::Window` + `WA_DeleteOnClose false` per riutilizzabilità.
- **Colori**: sfondo `#141414` (più scuro del tema), sidebar `#1a1a1a`, card `#1f1f1f`.
- **Font body**: `Inter, Segoe UI, Ubuntu` — NO monospace globale (solo `#chatLog`).
- **Spacing**: `setContentsMargins(0,0,0,0)` / `setSpacing(0)` dove si vuole massima densità; 8-16px altrimenti.

---

## Regole layout Qt6

### QTabWidget
- NON passare `tabs` come parent a widget factory (ManutenzioneePage, PersonalizzaPage).
  Usare `this` come parent — altrimenti diventano child visibili del QTabWidget e
  intercettano i click sulla tab bar. ← **Bug risolto 2026-03-22**
- Per tab annidati: usare `objectName` diverso per ogni QTabWidget (evita rendering blank in Qt6).
- Aggiungere `cfgGrid->setRowStretch(N+1, 1)` alle griglie per evitare che lo spazio extra
  venga distribuito tra le righe invece di accumularsi in fondo. ← **Bug risolto 2026-03-22**

### QGridLayout
- Impostare sempre `setRowStretch(lastRow+1, 1)` se il widget può essere ridimensionato
  oltre il contenuto (es. dentro QScrollArea con `setWidgetResizable(true)`).
- `setColumnStretch` sui campi che devono espandersi orizzontalmente.

### QScrollArea
- `setFrameShape(QFrame::NoFrame)` per bordi invisibili.
- `setWidgetResizable(true)` fa crescere il widget interno — serve `setRowStretch` per evitare
  distribuzione errata dello spazio.

### MonitorPanel / dialog embedded
- Per embeddare un QDialog come tab: `setWindowFlags(Qt::Widget)` + `setMinimumSize(0,0)`.
  Le connessioni AiClient continuano a funzionare normalmente.

---

## Pattern connessioni Qt

### Connessione one-shot (evita accumulo)
```cpp
auto* connHolder = new QObject(this);
connect(sorgente, &Sorgente::segnale, connHolder, [this, connHolder](auto arg){
    // logica
    connHolder->deleteLater();
});
```

### Cambio modello senza cambio backend
```cpp
m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), nuovoModello);
// NON usare AiClient::Ollama hardcoded
```

### Counter polling senza heap
```cpp
timer->setProperty("ticks", 0);
// dentro il lambda: timer->setProperty("ticks", timer->property("ticks").toInt() + 1)
```

---

## Percorsi — sempre via PrismaluxPaths

```cpp
#include "prismalux_paths.h"
namespace P = PrismaluxPaths;

P::modelsDir()          // cartella .gguf
P::llamaServerBin()     // binario llama-server
P::scanGgufFiles()      // scansione .gguf deduplicata
P::repolish(widget)     // forza ricalcolo QSS dopo setProperty()
P::kOllamaPort          // 11434
P::kLlamaServerPort     // 8081
P::kLocalHost           // "127.0.0.1"
```
**Non fare mai hardcode** di percorsi, porte, host nei sorgenti.

---

## Emoji e Unicode nei sorgenti C++

Sempre sequenze hex UTF-8, mai emoji letterali:
```cpp
"\xf0\x9f\x8d\xba"  // 🍺
"\xe2\x9c\x85"      // ✅
"\xf0\x9f\x93\x8a"  // 📊
"\xe2\x9a\x99\xef\xb8\x8f"  // ⚙️
```

---

## Polling asincrono (NO waitForStarted)

`waitForStarted()` blocca il thread UI fino a 4s. Usare invece:
```cpp
connect(proc, &QProcess::errorOccurred, this, [](QProcess::ProcessError e){ ... });
// + QTimer polling HTTP ogni 1s, un solo QNetworkAccessManager fuori dal lambda
```

---

## AUTOMOC — widget header-only con Q_OBJECT

I widget in `widgets/*.h` con `Q_OBJECT` devono essere in `CPP_SRCS` nel CMakeLists.txt:
```cmake
set(CPP_SRCS
    ...
    widgets/spinner_widget.h
    widgets/status_badge.h
)
```
Altrimenti AUTOMOC non genera il vtable → linker error.

---

## ZIP esportazione

- Nome fisso: **`Prismalux_Windows_full.zip`** — sempre sovrascritto, mai con data nel nome.
- Script: `crea_zip_windows.py` (root Prismalux/)
- Comando rapido: `./aggiorna.sh --zip`

---

# TODO — Cose da fare

## Alta priorità
- [ ] **Quiz Interattivi C** — implementare `src/quiz.c` (stub presente; versione Python pronta)
- [ ] **Voce 🎙** — pulsante presente in agenti_page.cpp ma disabilitato (stub); implementare riconoscimento vocale Qt
- [ ] **CPU+GPU dialog** — split model layers tra NVIDIA + iGPU via agent_scheduler con budget VRAM per dispositivo
- [ ] **RAM inter-agente** — il check pre-pipeline non copre la crescita RAM *durante* l'esecuzione

## Media priorità
- [ ] **Dashboard Statistica C** — stub; versione Python pronta in `dashboard_statistica.py`
- [ ] **Analisi Dati AI C** — stub; versione Python pronta in `tutor_dati.py`
- [ ] **Cerca Lavoro + CV Reader** — presenti in Python, non ancora nel C/Qt
- [ ] **StatusBadge nell'header** — widget esiste ma non usato; aggiungere `● Online`/`● Avvio` accanto a m_lblBackend
- [ ] **Cache modelli** — `fetchModels()` chiamato ad ogni cambio backend; aggiungere TTL 30s con `m_lastBackend`
- [ ] **Tooltip ricchi sui gauge** — mostrare GB usati/totali RAM e nome GPU (dati già disponibili in SysSnapshot)
- [ ] **Auto-assign con llama-server** — quando c'è un solo modello caricato, disabilitare il pulsante "Auto-assegna" o mostrare messaggio

## Bassa priorità
- [ ] **buildModelBar() porta 8080** — in impostazioni_page.cpp c'è ancora 8080 hardcoded; sostituire con `P::kLlamaServerPort`
- [ ] **Typo** — `buildCythoStudio` → `buildCythonoStudio` in personalizza_page.cpp
- [ ] **generateQuestion() in ImparaPage** — variabili `static` di connessione: rischio race condition; usare flag `m_generating`
- [ ] **Refactor funzioni lunghe** — `load_gguf_model()`, `multi_agent_run()`, `gestisci_modelli()`, `choose_model()` (>150 righe)
- [ ] **Test su strada** — eseguire `./prismalux` manualmente su tutti i menu, edge case
- [ ] **Animazione navigazione** — fade breve (QGraphicsOpacityEffect) tra tab per migliorare UX
- [ ] **sort modelli llama-server** — `m_modelInfos` ordina per size ma con llama-server tutti size=0; fallback alfabetico

## Ottimizzazioni identificate nel CLAUDE.md Qt_GUI
- [ ] Disabilitare "Auto-assegna" con llama-server (un solo modello disponibile → nessuna scelta)
- [ ] Cache fetchModels con TTL 30s per evitare fetch ridondanti
- [ ] Navigazione animata (fade) tra le sezioni principali
