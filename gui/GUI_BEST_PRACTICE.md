# PATTERN PREDEFINITI — GUI Prismalux Qt6

> **Questo è IL documento dei pattern predefiniti del progetto** (agg. 2026-07-17).
> Consultarlo PRIMA di scrivere nuova UI o nuovo codice pagina: se un caso rientra
> in un pattern qui sotto, si applica il pattern — non si reinventa.
> Convenzioni C++/API complete: `gui/CLAUDE.md`. Regole fisse: `TOOL_TIP/BEST_PRACTICE_&_GOAL/REGOLE_IRREMOVIBILI.md`.

## Pattern UI predefiniti (in ordine di frequenza d'uso)

### 1. Zone dense → CollapsibleSection (stile Blender)
**Quando**: molti widget impilati, colonne a 2+ card, guide/testi statici ingombranti.
```cpp
#include "../widgets/collapsible_section.h"
// Caso A — contenuto libero:
lay->addWidget(new CollapsibleSection(tr("Titolo"), contenuto, /*startOpen=*/true, parent));
// Caso B — QGroupBox esistente (1 riga, il titolo migra nell'header da solo):
lay->addWidget(CollapsibleSection::fromGroupBox(box));
```
- `startOpen=true` di default = comportamento invariato finché l'utente non ripiega.
- Chiusa di default SOLO per contenuto statico/occasionale (guide, help, "Avanzato").
- Sostituire i toggle ▶/▼ fatti a mano con questo widget (fatto in WAN Compute, D-52).
- **NON** avvolgere il contenuto principale di una tab (es. la scena 3D di Vision3D).
- Già applicato: Lavoro (4 sezioni), WAN Compute (3), Vision3D Preparazione (6), Impostazioni Gestione LLM + Parametri AI (7).

### 2. Dimensioni → sempre `dpiScale(N)` (`dpi_utils.h`)
`setFixedWidth(dpiScale(80))`, mai il numero nudo. No-op a 96dpi, scala su HiDPI/Wayland.

### 3. Larghezze pannelli → dal contenuto reale, mai fisse
Un `setFixedWidth()` su una colonna con pulsanti taglia col tema/lingua sbagliati (bug D-49):
```cpp
const int w = qBound(dpiScale(248),
                     panel->minimumSizeHint().width() + scrollbarW + margine,
                     dpiScale(400));
```

### 4. Combo → logica su `currentData()`, MAI sul testo
`combo->addItem(tr("Etichetta"), "chiave-stabile");` poi `if (combo->currentData() == "chiave-stabile")`.
Le etichette sono traducibili (D-42): confrontare il testo rompe la lingua EN. Vale anche per i QSettings.

### 5. Tab costose → lazy (`ensureTabBuilt` / `LazyTabLoader`)
Tab principali: placeholder + `ensureTabBuilt(idx)` (mainwindow_tabs.cpp). QTabWidget interni:
`widgets/lazy_tab_loader.h` (`addEager` la prima, `addLazy(label, factory)` le altre). Dettagli in CLAUDE.md.

### 6. Splitter con form in alto → minimo esplicito
Il sizeHint piccolo di una QScrollArea vince su `setSizes()` (bug D-46): dare `setMinimumHeight()`
alla sezione che deve restare leggibile, `setSizes()` solo come proporzione iniziale.

### 7. Nuovi file in `gui/pages/` → nome parlante
`main_*` (pagina/tab) · `settings_*` (Impostazioni) · `widget_*` (componente) · `dialog_*` (dialog).
Mai `*_page.cpp`. Widget riusabili header-only → `gui/widgets/` (+ CPP_SRCS se Q_OBJECT, vedi AUTOMOC sotto).

### 8. Stringhe visibili → `tr()` sempre; tabelle statiche → `P::trTab()` + `QT_TRANSLATE_NOOP`
Cataloghi senza `<location>` (`lconvert -locations none`), lupdate mirato mai `-recursive` (D-42).

### 9. connect() → slot nominati (regola progetto)
Lambda solo con context object (4° arg) che possiede tutti i puntatori catturati, max 2 righe. Dettagli in CLAUDE.md.

---

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

# Nota storica
La sezione "TODO — Cose da fare" che viveva qui è stata rimossa il 2026-07-17:
l'unico file TODO del progetto è `TODO.md` in root (le 2 voci ancora aperte sono
state spostate lì: split CPU+GPU multi-dispositivo, animazione fade tra tab).
