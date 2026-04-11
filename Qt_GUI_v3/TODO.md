# TODO — Prismalux Qt GUI
> Aggiornato: 2026-04-01

---

## 🔴 ALTA PRIORITÀ

### 1. Spostare pulsante Ollama/llama-server
**File**: `mainwindow.cpp` → `buildHeader()` + `buildSidebar()`

Il pulsante `m_btnBackend` ("🐦 Ollama") è in alto a destra nell'header, vicino ai gauge CPU/RAM/GPU. Non c'entra nulla col monitoraggio hardware.

**Soluzione**: spostarlo nella sidebar, SOPRA il primo navBtn `[0] Agenti AI`, come riga sempre visibile.
- Togli `m_btnBackend` + `m_badgeServer` + `m_spinServer` dall'header
- Aggiungili in cima alla sidebar (prima dei navBtn)
- La sidebar diventa: [backend selector] ─── [Agenti AI] [Finanza] [Impara] ─── [versione]

---

### 2. Semplificare le Impostazioni (meno click)
**File**: `pages/impostazioni_page.cpp`, `pages/manutenzione_page.*`, `pages/personalizza_page.*`

Attualmente: Impostazioni → card → sottopagina → sottosottopagina (3-4 click)

**Soluzione proposta**: collassare le 2 card in un unico schermo con tab orizzontali:
```
[🔧 Backend & Modelli] [🔩 Avanzato (llama.cpp/Cython)] [🖥️ Hardware]
```
- Tab 0 "Backend & Modelli": selezione Ollama/llama-server + lista modelli (contenuto da `manutenzione_page`)
- Tab 1 "Avanzato": VRAM Benchmark + llama.cpp Studio + Cython Studio (da `personalizza_page`)
- Tab 2 "Hardware": CPU/GPU/RAM rilevati + benchmark
- Elimina le card intermedie + i bottoni "← Impostazioni" ridondanti
- Risparmio: da 3-4 click a 1 click + tab

---

### 3. Scheda "Programmazione" (NUOVA — da creare)
**File nuovo**: `pages/programmazione_page.h/cpp`
**File CMakeLists.txt**: da aggiornare

**Funzionamento richiesto**:
- Input task/prompt → agente AI genera codice
- Il codice generato viene eseguito localmente con subprocess
- Il codice ha qualsiasi errore e quindi non permette l'esecuzione prendi come input l'errore e risolvi fino a avere zero errori.quando lo fai fai comparire " sto risolvengo bug sintattici e o logici " 

**Come eseguire il codice per linguaggio**:

| Linguaggio | Metodo di esecuzione |
|------------|---------------------|
| **Python** | `QProcess("python3", {"-c", codice})` — versione rilevata da `python3 --version` |
| **C** | Scrivi `.c` in temp → `gcc -o /tmp/prisma_out file.c -lm` → esegui `/tmp/prisma_out` |
| **C++** | Scrivi `.cpp` in temp → `g++ -std=c++17 -o /tmp/prisma_out file.cpp` → esegui |
| **JavaScript** | Prova `node -e "codice"` se Node.js installato, altrimenti apri in QWebEngineView (browser embedded) |
| **Bash/Shell** | `QProcess("bash", {"-c", codice})` |
| **Java** | Scrivi `.java` in temp → `javac` → `java` |
| **Rust** | `rustc` → esegui (solo se cargo installato) |

**Rilevamento automatico linguaggio**: analizza il blocco ```lang dal markdown AI

**Output**: mostra stdout + stderr in un QTextEdit con scrollback

---

### 4. Salvataggio output scheda Programmazione
**File**: `pages/programmazione_page.cpp` (nuovo)

Pulsante "💾 Salva..." che apre un dialog con checkboxes:
```
☑ Codice generato (.py / .c / .cpp / .js / ecc.)
☑ Output esecuzione (.txt)
☑ Prompt inviato all'AI (.txt)
☑ Risposta completa AI (.md)
☐ Grafico generato (se matplotlib → .png auto-catturato)
```
- Usa `QFileDialog::getSaveFileName()` per la cartella destinazione
- Genera un file per tipo selezionato (es. `prismalux_output_20260401_143022.py`)
- Per Python con matplotlib: salva automaticamente i plot con `plt.savefig('/tmp/prisma_plot.png')` iniettato nel codice prima dell'esecuzione

---

## 🟡 MEDIA PRIORITÀ

### 5. Tasti rapidi — completare e documentare
**File**: `mainwindow.cpp` (costruttore), `CLAUDE.md` tabella shortcuts

Shortcut esistenti: `Alt+1`, `Alt+2`, `Alt+3`

**Da aggiungere**:
| Tasto | Azione |
|-------|--------|
| `Ctrl+,` | Apri Impostazioni |
| `Ctrl+Shift+C` | Copia ultimo output AI |
| `Ctrl+S` | Salva output (nella scheda attiva) |
| `Ctrl+Enter` | Invia prompt (invece di solo click) |
| `F5` | Ricarica lista modelli |
| `Escape` | Annulla generazione AI in corso |

**Mostrare i tasti nei tooltip dei navBtn** (già fatto per Alt+1/2/3 — estendere alle nuove shortcut)

---

### 6. Header — ridurre affollamento
**File**: `mainwindow.cpp` → `buildHeader()`

Attualmente nell'header ci sono: logo · backend · model · stretch · CPU · RAM · GPU · modello · 🍺 · 🚨 · 🗑 · 🐦Ollama · badge · spinner · 📊 · ⚙️

**Semplificazioni**:
- Togli la seconda 🍺 (ridondante)
- Unisci 🚨 e 🗑 in un unico pulsante "🛟 RAM" con menu a tendina
- Sposta 🐦 backend → sidebar (vedi task #1)
- `m_lblModel` è già visibile: togli la label duplicata in `gaugeBox2`

---

### 7. Temi — integrazione in Impostazioni tab
**File**: `pages/impostazioni_page.cpp`

Attualmente i temi si cambiano solo da... (non è chiaro dove). Aggiungere selettore tema visuale (4 card con anteprima colore) nel tab "Backend & Modelli" o in un tab dedicato "🎨 Aspetto".

---

## 🟢 BASSA PRIORITÀ / FUTURE

### 8. Scheda Programmazione — funzionalità extra
- [ ] Syntax highlighting dell'editor (QSyntaxHighlighter o integrazione KSyntaxHighlighting)
- [ ] Esecuzione in sandbox (bubblewrap/firejail su Linux)
- [ ] Timeout di sicurezza per esecuzione (es. 30s max)
- [ ] Cronologia esecuzioni (ultimi 10 codici eseguiti)
- [ ] Template predefiniti per linguaggio

### 9. Impostazioni — persistenza su file JSON
- [ ] Salvare backend scelto, porta, modello preferito in `~/.config/prismalux/settings.json`
- [ ] Al prossimo avvio ripristinare automaticamente l'ultimo backend usato

### 10. Quiz Interattivi e Dashboard Statistica
- [ ] Implementare in Qt (attualmente stub 🌫️)
- [ ] Collegare le statistiche dei quiz a un grafico QChart

### 11. Cython Studio (stub Qt)
- [ ] Portare da Python: flusso guidato, benchmark, integrazione multi-agente

---

## ✅ COMPLETATI (Sprint 1 + 2 + 3)

- [x] **[1]** Pulsante backend spostato in sidebar (sopra Agenti AI)
- [x] **[2]** Impostazioni semplificate con QTabWidget (2 tab: Backend & Modelli / Avanzato)
- [x] **[3]** Tasti rapidi: F5=ricarica modelli, Escape=interrompi AI, Ctrl+,=impostazioni, Alt+4=Programmazione
- [x] **[4]** Scheda Programmazione creata: AI genera codice → esegui → output → salva
- [x] **[Sprint 3]** Auto-fix loop: codice con errori → AI corregge automaticamente (max 3 tentativi) con messaggio "Sto risolvendo bug..."
- [x] **[Sprint 3]** Pulsante "📋 Copia codice" + shortcut Ctrl+Shift+C
- [x] **[Sprint 3]** Shortcut Ctrl+S = salva output nella scheda Programmazione

---

## 📋 ORDINE CONSIGLIATO DI IMPLEMENTAZIONE

```
Sprint 1 (UX immediata):
  [1] Sposta backend btn in sidebar
  [2] Semplifica impostazioni con tab
  [5] Aggiungi Ctrl+Enter e altri tasti rapidi

Sprint 2 (feature principale):
  [3] Crea scheda Programmazione (bare minimum: AI → codice → esegui → output)
  [4] Aggiungi salvataggio output

Sprint 3 (rifinitura):
  [6] Pulisci header
  [7] Temi in impostazioni
  [8] Funzionalità extra Programmazione
```

---

## 🗒️ NOTE TECNICHE

### Aggiungere una nuova pagina alla sidebar
1. Crea `pages/programmazione_page.h/cpp`
2. Aggiungi al `CMakeLists.txt` nella variabile `CPP_SRCS`
3. In `mainwindow.h`: aggiungi `m_navBtns[4]` (o ridimensiona array a 4)
4. In `mainwindow.cpp` → `buildSidebar()`: aggiungi il 4° navBtn con `Alt+4`
5. In `buildContent()`: `m_stack->addWidget(new ProgrammazionePage(m_ai, this))`
6. In `navigateTo()`: aggiungi il caso idx=3

### QProcess per esecuzione codice — schema base
```cpp
auto* proc = new QProcess(this);
proc->setProcessChannelMode(QProcess::MergedChannels);
connect(proc, &QProcess::readyReadStandardOutput, this, [this, proc]{
    m_outputArea->append(proc->readAllStandardOutput());
});
connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
        this, [this](int code, QProcess::ExitStatus){
    m_outputArea->append(QString("\n[Uscito con codice %1]").arg(code));
});
proc->start("python3", {"-c", codice});
```

### Cattura plot matplotlib
Iniettare nel codice Python prima dell'esecuzione:
```python
import matplotlib
matplotlib.use('Agg')  # non-interattivo
# ... codice utente ...
import matplotlib.pyplot as plt
plt.savefig('/tmp/prismalux_plot.png', bbox_inches='tight')
```
Poi caricare `/tmp/prismalux_plot.png` con `QPixmap` e mostrarlo in un `QLabel`.
