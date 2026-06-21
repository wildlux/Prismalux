# TODO — Pendenze v3.0

*Creato: 2026-06-21 | Versione: Prismalux v3.0*

---

## ✅ BUG-1 — `checkForUpdates()` versione hardcoded a 2.9 *(completato 2026-06-21)*
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

## ✅ FEAT-1 — `runLint()` da stub a implementazione reale *(completato 2026-06-21)*
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

## ✅ FEAT-2 — TTFT (Time to First Token) nell'header *(completato 2026-06-21)*
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

## ✅ FEAT-3 — Android APK rebuild v3.0 *(versione aggiornata 2026-06-22)*
**Priorità:** Media (solo se si distribuisce l'APK)  
**File:** `ANDROID/` + `EXPORT/android/build_apk.sh`  
**Problema:** L'APK in `ANDROID/PrismaluxMobile.apk` è compilato con versione precedente.
Il roadmap `TODO_ANDROID_ROADMAP.md` segna tutti i task ✅ ma l'APK non è stato ricostruito.

**Verificare e aggiornare:**
- Stringa versione in `ANDROID/AndroidManifest.xml` (`android:versionName`)
- `ANDROID/res/values/strings.xml` se contiene la versione
- Ricompilare con `./EXPORT/android/build_apk.sh` dopo aver aggiornato i riferimenti

---

## ✅ FEAT-4 — CloudCompare integrazione reale *(completato 2026-06-22)*
**Priorità:** Media — tab visibile nella UI come placeholder  
**File:** `gui/pages/main_tools.cpp` riga 304, `gui/pages/main_app_controller.cpp` riga 1358  
**Problema:** Il tab "CloudCompare" in StrumentiPage (indice 9) e in AppController hanno solo stub:
```cpp
/* 9 — CloudCompare (prossimamente) — stub */
{ "/* CloudCompare — funzionalità non ancora disponibile */", nullptr, ... }
```
AppController mostra un `QTextEdit` con placeholder "Output CloudCompare apparirà qui (prossimamente)...".

**Piano implementazione:**
- Rilevare percorso CloudCompare installato (`which cloudcompare` o path manuale in Impostazioni)
- Pulsante "Apri CloudCompare" → `QProcess::startDetached("cloudcompare")`
- Pulsante "Apri file .las/.ply/.pcd" → `QFileDialog` → `cloudcompare file.las`
- Azioni AI: "Descrivi nuvola di punti" (vision se disponibile), "Genera script Python Open3D"
- Se non installato → banner con link `https://cloudcompare.org/`

---

## ✅ SEC-1 — API key cloud salvata in file in chiaro *(completato 2026-06-21)*
**Priorità:** Media — security, non è un crash ma è un rischio  
**File:** `gui/prismalux_paths.h` riga 989, `gui/pages/settings_ai.cpp` riga 502  
**Problema:** La API key per servizi cloud (OpenAI, ecc.) viene salvata in
`~/.prismalux/cloud_api.key` come testo in chiaro. Il commento nel codice
dice esplicitamente "spostare in keychain in futuro":
```cpp
///< API key cloud (in chiaro su QSettings — spostare in keychain in futuro)
```
QKeychain è già una dipendenza del progetto (usato per i token LAN in `gui/pages/settings_lan.cpp`).

**Fix:**
- In `P::saveCloudApiKey(key)`: sostituire `QFile` write con `QKeychain::WritePasswordJob`
- In `P::loadCloudApiKey()`: usare `QKeychain::ReadPasswordJob`
- Servizio keychain: `"Prismalux"`, account: `"cloud_api_key"`
- Cancellare il file `~/.prismalux/cloud_api.key` se esiste dopo la migrazione
- Aggiungere fallback al file se QKeychain non disponibile (es. build senza keychain)

---

## ✅ FEAT-5 — Pipeline DPO/feedback dai 👍/👎 *(completato 2026-06-22)*
**Priorità:** Bassa — feature ricerca, nessun impatto immediato  
**File:** `gui/prismalux_paths.h` riga 473, `gui/pages/main_ai_feedback.cpp`  
**Problema:** Il sistema raccoglie feedback 👍/👎 per ogni risposta AI e lo salva in JSONL
(`feedbackPath()` → `~/.prismalux/feedback.jsonl`), ma il dato non viene mai usato.
Il commento dice "base per DPO futuro".

**Opzioni implementazione (in ordine crescente di complessità):**
1. **Analytics locali** (semplice): tab in Impostazioni che mostra statistiche — N risposte positive/negative, per modello, per orario
2. **Filtro qualità** (medio): se una risposta ha feedback negativo, al retry successivo sulla stessa domanda abbassa la temperatura e allunga il contesto
3. **Export DPO dataset** (avanzato): pulsante "Esporta dataset fine-tuning" → genera `{"prompt":..., "chosen":..., "rejected":...}` in formato Alpaca/ShareGPT per fine-tuning locale con Unsloth/llama.cpp

---

## Riferimento incrociato

Per la strategia completa sulla gestione "LLM non sa la risposta" vedere:
→ [`TODO_LLM_NON_SA.md`](TODO_LLM_NON_SA.md) (5 task, priorità alta)
