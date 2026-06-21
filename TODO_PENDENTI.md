# TODO — Pendenze v3.0

*Creato: 2026-06-21 | Versione: Prismalux v3.0*

---

## BUG-1 — `checkForUpdates()` versione hardcoded a 2.9
**Priorità:** Alta — bug silenzioso immediato  
**File:** `gui/mainwindow_slots.cpp` righe 987 e 1000  
**Problema:** Il controllo aggiornamenti confronta il tag GitHub con la stringa `"2.9"` invece di `"3.0"`.
Con v3.0 installata, la funzione restituisce subito senza notificare mai versioni future.
Anche lo User-Agent HTTP è rimasto `"Prismalux/2.9"`.

```cpp
// Riga 987 — da correggere:
req.setRawHeader("User-Agent", "Prismalux/2.9");  // → "Prismalux/3.0"

// Riga 1000 — da correggere:
if (tag == "2.9") return;  // → if (tag == "3.0") return;
```

**Fix:** 2 righe. Leggere la versione da `QCoreApplication::applicationVersion()`
(già impostata da CMakeLists `project(VERSION 3.0)`) invece di hardcodarla:
```cpp
const QString curVer = QCoreApplication::applicationVersion(); // "3.0"
req.setRawHeader("User-Agent", QString("Prismalux/%1").arg(curVer).toLatin1());
if (tag == curVer) return;
```

---

## FEAT-1 — `runLint()` da stub a implementazione reale
**Priorità:** Media — funzionalità visibile nella UI ma non operativa  
**File:** `gui/pages/main_programming_slots.cpp` riga 1629  
**Problema:** Il pulsante "Lint" nella pagina Programmazione esiste ma il metodo è vuoto:
```cpp
void ProgrammazionePage::runLint() {
    /* TODO: implementare quando il linting per-lingua sarà definito */
}
```

**Piano implementazione:**
- Rilevare lingua dal tab attivo nell'editor (già disponibile: `m_editorTabs->currentWidget()`)
- Scrivere il codice in un file temporaneo con `P::safeTempPath()`
- Eseguire il linter appropriato via `QProcess`:
  | Lingua | Linter | Pacchetto |
  |--------|--------|-----------|
  | Python | `pyflakes` | pip |
  | C/C++ | `clang-tidy` | apt |
  | JS/TS | `eslint` | npm |
- Mostrare output annotato nel `m_diffView` già presente
- `errorOccurred` handler per linter non installato → suggerire installazione

---

## FEAT-2 — TTFT (Time to First Token) nell'header
**Priorità:** Bassa — miglioramento UX, nessun impatto funzionale  
**File:** `gui/pages/main_ai_stream.cpp` + `gui/mainwindow.h/cpp`  
**Problema:** Il progetto del 2026-06-11 prevedeva `"⚡Nms"` nell'header accanto al nome modello.
Non è mai stato implementato.

**Piano implementazione:**
- In `AgentiPage`: avviare `QElapsedTimer m_ttftTimer` in `onBtnRunClicked()`
- In `onToken()` (primo token ricevuto): fermare il timer, emettere `ttftMeasured(int ms)`
- In `MainWindow`: collegare il segnale → aggiornare label header con `"⚡42ms"`
- Reset a `""` quando non in streaming
- Soglie colore: verde < 500ms · arancio 500-2000ms · rosso > 2000ms

---

## FEAT-3 — Android APK rebuild v3.0
**Priorità:** Media (solo se si distribuisce l'APK)  
**File:** `ANDROID/` + `EXPORT/android/build_apk.sh`  
**Problema:** L'APK in `ANDROID/PrismaluxMobile.apk` è compilato con versione precedente.
Il roadmap `TODO_ANDROID_ROADMAP.md` segna tutti i task ✅ ma l'APK non è stato ricostruito.

**Verificare e aggiornare:**
- Stringa versione in `ANDROID/AndroidManifest.xml` (`android:versionName`)
- `ANDROID/res/values/strings.xml` se contiene la versione
- Ricompilare con `./EXPORT/android/build_apk.sh` dopo aver aggiornato i riferimenti

---

## Riferimento incrociato

Per la strategia completa sulla gestione "LLM non sa la risposta" vedere:
→ [`TODO_LLM_NON_SA.md`](TODO_LLM_NON_SA.md) (5 task, priorità alta)
