# Best Practice — Sicurezza (Prismalux Qt GUI)

Regole derivate dall'audit di sicurezza 2026-04-14 e dai fix applicati.
Ogni regola include il file/riga di riferimento dove il pattern è già implementato.

---

## 1. Esecuzione codice AI — sempre con conferma esplicita

**Regola**: mai avviare un QProcess su codice estratto da un LLM senza mostrare
all'utente un dialog con anteprima del codice e pulsanti "Esegui / Annulla".

```cpp
// SBAGLIATO — esecuzione silenziosa
proc->start(findPython(), {"-c", codeFromAI});

// CORRETTO — dialog di conferma prima
auto* dlg = new QDialog(this);
// ... mostra codice in QTextEdit read-only ...
if (dlg->exec() != QDialog::Accepted) return;
proc->start(findPython(), {"-c", codeFromAI});
```

**Riferimento**: `gui/pages/agenti_page_exec.cpp` — funzione `runExtractedCode()`.

---

## 2. pip install — blocco senza conferma

**Regola**: pip non si esegue mai in automatico. L'utente deve vedere la lista
dei pacchetti e approvare esplicitamente. Motivo: typosquatting su PyPI.

```cpp
// SBAGLIATO
pip->start(findPython(), {"-m", "pip", "install", pkg});

// CORRETTO
QStringList pkgs = extractImports(code);
if (pkgs.isEmpty()) { runCode(); return; }
// dialog: "Questi pacchetti verranno installati: X, Y. Procedere?"
if (!confirmDialog(pkgs)) return;
pip->start(findPython(), {"-m", "pip", "install", pkgs});
```

**Riferimento**: `gui/pages/agenti_page_exec.cpp` — guardia `_guardia_pip_confirm`.

---

## 3. QProcess — arglist separata, mai `sh -c` con input utente

**Regola**: usare sempre `QProcess::start(program, QStringList{args})`.
`sh -c stringa` con path user-controlled consente shell injection.

```cpp
// SBAGLIATO — injection se path contiene spazi o "
proc->start("sh", {"-c", "cmake " + userPath});

// CORRETTO — argomenti separati
proc->start("cmake", {"--build", userPath, "-j", QString::number(QThread::idealThreadCount())});
```

**Riferimento**: `gui/pages/personalizza_page.cpp` — compilazione llama.cpp Studio
(3 QProcess chained: git → cmake config → cmake build).

---

## 4. Sanitizzazione output AI — non affermare sicurezza totale

**Regola**: il filtro parole-chiave su prompt (es. lista di frasi vietate) non è
protezione reale contro prompt injection. Documentarlo come mitigazione parziale,
non come difesa completa.

```cpp
// In ai_client.cpp — commento obbligatorio accanto al filtro
// ATTENZIONE: questo filtro è una mitigazione parziale.
// Non è immune a bypass via unicode/encoding/italiano.
// Per uso multi-utente o produzione: usare sandbox di esecuzione.
```

**Riferimento**: `gui/ai_client.cpp` — funzione `_sanitize_prompt()`.

---

## 5. Percorsi e host — solo loopback, nessun hardcode

**Regola**: tutti i percorsi passano per `PrismaluxPaths`, tutte le connessioni
di rete usano `P::kLocalHost` (`127.0.0.1`). Mai hardcode di indirizzi IP.

```cpp
// SBAGLIATO
QUrl url("http://localhost:11434/api/chat");

// CORRETTO
QUrl url(QString("http://%1:%2/api/chat").arg(P::kLocalHost).arg(P::kOllamaPort));
```

**Riferimento**: `gui/prismalux_paths.h` — costanti `kLocalHost`, `kOllamaPort`, `kLlamaServerPort`.

---

## 6. Output AI — limitare stack trace visibili all'utente

**Regola**: stderr di Python/llama-server può contenere centinaia di righe con
path assoluti interni. Filtrare prima di mostrare all'utente.

```cpp
// SBAGLIATO
m_log->append(proc->readAllStandardError());

// CORRETTO
m_log->append(P::sanitizeErrorOutput(proc->readAllStandardError()));
```

**Riferimento**: `gui/prismalux_paths.h` — `sanitizeErrorOutput(raw, maxLines=30)`.
Mantiene prime 8 + ultime 12 righe, omette il centro.

---

## 7. QSettings — chiavi centralizzate

**Regola**: nessuna stringa letterale per chiavi QSettings nei file sorgente.
Ogni chiave ha un'unica definizione in `namespace SK` in `prismalux_paths.h`.

```cpp
// SBAGLIATO (typo silenzioso)
ss.setValue("rag/noSave", true);
ss.value("rag/no_save", false);   // ← mai letta

// CORRETTO
ss.setValue(P::SK::kRagNoSave, true);
ss.value(P::SK::kRagNoSave, false);
```

**Riferimento**: `gui/prismalux_paths.h` — `namespace SK` con ~20 costanti.

---

## 8. RAG — avvisare per dati sensibili

**Regola**: i documenti RAG vengono salvati in chiaro in `~/.prismalux_rag.json`.
Il tab RAG deve mostrare sempre l'avviso e offrire l'opzione "Non salvare su disco".

```cpp
// In buildRagTab():
m_privacyLabel->setText(
    "I documenti vengono salvati in chiaro in ~/.prismalux_rag.json.\n"
    "Non indicizzare documenti fiscali o medici sensibili.");
```

**Riferimento**: `gui/pages/impostazioni_page.cpp` — `buildRagTab()`.
Chiave: `P::SK::kRagNoSave` → se true, `m_rag.save()` non viene chiamato.

---

## 9. Lambda — catture esplicite, contesto Qt come parent

**Regola**: evitare `[=]`. Usare catture esplicite con `this` come contesto Qt
(auto-disconnect quando il widget viene distrutto).

```cpp
// SBAGLIATO — [=] cattura tutto, widget può essere distrutto prima che scatti
connect(src, &Src::signal, [=]() { m_label->setText("ok"); });

// CORRETTO — this come contesto Qt + catture esplicite
connect(src, &Src::signal, this, [this, proc](int code) {
    if (code == 0) m_label->setText("ok");
});
```

**Riferimento**: `gui/pages/personalizza_page.cpp` — conversione da `[=]` a catture esplicite.

---

## 10. Connessioni segnali mutuamente esclusivi — pattern conn/connErr

**Regola**: quando due segnali alternativi (successo/errore) possono scattare per
la stessa richiesta, entrambe le connessioni devono essere dichiarate prima e
ciascuna deve disconnettere entrambe. Altrimenti connessioni stale si accumulano.

```cpp
auto* conn    = new QMetaObject::Connection;
auto* connErr = new QMetaObject::Connection;

*conn = connect(src, &Src::success, this,
    [conn, connErr, ...](auto result) {
        disconnect(*conn);    delete conn;
        disconnect(*connErr); delete connErr;
        // logica ...
    }, Qt::SingleShotConnection);

*connErr = connect(src, &Src::error, this,
    [conn, connErr, ...](const QString&) {
        disconnect(*connErr); delete connErr;
        disconnect(*conn);    delete conn;
        // gestione errore ...
    }, Qt::SingleShotConnection);
```

**Riferimento**: `gui/pages/impostazioni_page.cpp` ~1754 — loop RAG embedding.
**Causa bug se non rispettato**: connessioni stale scattano su richieste future,
addChunk viene chiamato con dati sbagliati, contatore RAG rimane a 0.
