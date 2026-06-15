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
- [N/A] **Quiz Interattivi C** — `src/quiz.c` e Python non esistono più; quiz CCNA implementato in Qt (`QuizCcnaPage`)
- [ ] **Voce 🎙** — STT implementato in Android (`AudioPage`); desktop usa Whisper via `/api/whisper` in LanServer
- [ ] **CPU+GPU dialog** — split model layers tra NVIDIA + iGPU via agent_scheduler con budget VRAM per dispositivo
- [x] **RAM inter-agente** — FATTO 2026-06-15: in `advancePipeline()` (quando `m_currentAgent > 0`), legge `/proc/meminfo`; se RAM ≥92% interrompe con messaggio senza dialog bloccante. Il check pre-pipeline (≥92% block, ≥75% warn) rimane in `checkRam()`

## Media priorità
- [N/A] **Dashboard Statistica C** / **Analisi Dati AI C** — i file Python non esistono più; funzionalità integrate nelle tab Qt
- [x] **Cerca Lavoro + CV Reader** — FATTO: `LavoroPage` in `StrumentiPage` (`m_lavoroPage`) con AI + CV reader
- [x] **StatusBadge nell'header** — FATTO 2026-06-15: `m_badgeServer` creato in `buildContent()` nel corner container accanto a `m_btnBackend`. Offline→Starting→Online/Error in `applyBackend()`, `onInitialModelsReady()`, `onApplyBackendModelsReady()`
- [x] **Cache modelli** — FATTO: TTL 30s implementato in `AiClient::fetchModels()` (`m_cacheTimer`/`m_cacheValid`)
- [x] **Tooltip ricchi sui gauge** — FATTO 2026-06-15: `ResourceGauge::update()` usa il `detail` come `setToolTip()`; RAM mostra "X/Y GB", CPU mostra nome CPU, GPU mostra "nome | VRAM X/Y GB"
- [N/A] **Auto-assign con llama-server** — pulsante "Auto-assegna" non esiste più nella UI v2.9

## Bassa priorità
- [x] **buildModelBar() porta 8080** — FATTO 2026-06-15: `main_maintenance.cpp` + `main_learn.cpp` usano `P::kLlamaServerPort`
- [N/A] **Typo buildCythoStudio** — `personalizza_page.cpp` non esiste più nella v2.9
- [x] **generateQuestion() in ImparaPage** — FATTO: guard `m_quizBusy` in `ImparaPage::generateQuestion()` previene click multipli
- [N/A] **Refactor funzioni lunghe** — le funzioni C/Python citate non esistono più; equivalenti Qt già strutturate
- [x] **sort modelli llama-server** — FATTO 2026-06-15: `list.sort(Qt::CaseInsensitive)` in `AiClient::onModelsReply()` ramo LlamaServer
- [ ] **Animazione navigazione** — fade breve (QGraphicsOpacityEffect) tra tab per migliorare UX

## Ottimizzazioni identificate nel CLAUDE.md Qt_GUI
- [x] Cache fetchModels con TTL 30s — FATTO (vedi sopra)
- [ ] Navigazione animata (fade) tra le sezioni principali
