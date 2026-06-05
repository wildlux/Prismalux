# Best Practice — Bonus (Prismalux Qt GUI)

Pattern aggiuntivi scoperti dall'analisi del codebase che non compaiono
nei tre file principali. Stessa struttura: regola + riferimento reale.

---

## SICUREZZA

### B1. toHtmlEscaped() — obbligatorio per input utente in HTML

**Regola**: qualsiasi valore non-costante inserito in una stringa HTML
(`.arg()`, concatenazione) deve passare per `.toHtmlEscaped()`.
QTextBrowser interpreta HTML: un task `<b>` cambia il rendering;
un `<img src=x onerror=...>` può eseguire JavaScript embedded.

```cpp
// SBAGLIATO — task è input utente, può contenere tag HTML
m_log->append(QString("Domanda: <i>%1</i>").arg(task));

// CORRETTO
m_log->append(QString("Domanda: <i>%1</i>").arg(task.toHtmlEscaped()));
```

Stesso problema con nomi file, nomi modello, messaggi di errore:
```cpp
// Tutto ciò che non è una costante letterale va escaped
m_log->append(QString("File: <b>%1</b>").arg(fname.toHtmlEscaped()));
m_log->append(QString("Modello: <b>%1</b>").arg(modelName.toHtmlEscaped()));
m_log->append(QString("[Errore: %1]").arg(err.toHtmlEscaped()));
```

**Riferimento bug reale**: `gui/pages/agenti_page_consiglio.cpp:86` — `.arg(task)` senza
escape in `<i>%1</i>`. Stessa riga 137: `.arg(err)` senza escape.

**Eccezione**: stringhe provenienti da `markdownToHtml()` sono già HTML — non
riscappare. Distinguere sempre tra "testo grezzo" e "HTML già costruito".

---

### B2. HTTP error body — Ollama risponde JSON anche su 4xx/5xx

**Regola**: quando `r->error() != NoError` non basta mostrare il codice HTTP.
Ollama restituisce `{"error":"..."}` nel body anche su 400/422/500.
Leggere il body JSON prima di mostrare il messaggio all'utente.

```cpp
void onReply(QNetworkReply* r) {
    if (r->error() != QNetworkReply::NoError) {
        const QByteArray body = r->readAll();
        const QJsonObject obj = QJsonDocument::fromJson(body).object();
        const QString msg = obj.contains("error")
            ? obj["error"].toString()
            : r->errorString();
        emit error(msg);   // messaggio specifico invece di "Network error 400"
        r->deleteLater();
        return;
    }
    // ...
}
```

**Riferimento**: `gui/ai_client.cpp:757` — parsing `{"error":"..."}` su HTTP 4xx.

---

## GESTIONE OGGETTI QT

### B3. deleteLater() — mai delete diretto nei signal handler

**Regola**: in qualsiasi lambda o slot che risponde a un segnale Qt, non chiamare
`delete` direttamente su oggetti Qt. Usare sempre `deleteLater()`.
`delete` diretto in un signal handler può distruggere l'oggetto mentre
Qt sta ancora iterando i suoi segnali — crash non deterministico.

```cpp
// SBAGLIATO — delete in un signal handler
connect(proc, &QProcess::finished, this, [proc](int code) {
    delete proc;   // ← crash se QProcess ha altri segnali in coda
});

// CORRETTO
connect(proc, &QProcess::finished, this, [proc](int code) {
    proc->deleteLater();   // Qt distrugge dopo che il control loop è libero
});
```

Stesso pattern per `QNetworkReply`, `QDialog`, `QTimer` creati dinamicamente:
```cpp
connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
```

**Riferimento**: `gui/pages/agenti_page_files.cpp:74,127,236` — tutti usano `deleteLater()`.

---

### B4. WA_DeleteOnClose e deleteLater — non mischiare

**Regola**: scegliere UNO dei due approcci per ogni dialog e usarlo
in modo coerente. Mischiare i due causa double-free.

```cpp
// APPROCCIO A — deleteLater manuale (usato nel codebase)
auto* dlg = new QDialog(this);
if (dlg->exec() != QDialog::Accepted) { dlg->deleteLater(); return; }
// ... usa dlg ...
dlg->deleteLater();

// APPROCCIO B — WA_DeleteOnClose (Qt distrugge alla chiusura)
auto* dlg = new QDialog(this);
dlg->setAttribute(Qt::WA_DeleteOnClose);
dlg->show();   // non usare exec() con WA_DeleteOnClose: il puntatore diventa dangling

// SBAGLIATO — misto: double-free
auto* dlg = new QDialog(this);
dlg->setAttribute(Qt::WA_DeleteOnClose);
dlg->exec();
dlg->deleteLater();  // ← Qt lo ha già distrutto alla chiusura di exec()
```

**Riferimento**: `gui/pages/agenti_page_consiglio.cpp:62-64` — approccio A corretto.

---

### B5. AUTOMOC — widget header-only con Q_OBJECT in CPP_SRCS

**Regola**: ogni file `.h` con `Q_OBJECT` deve comparire in `CPP_SRCS`
in `CMakeLists.txt`. AUTOMOC cerca `Q_OBJECT` solo nei file elencati.
Se mancante: il linker fallisce con "undefined reference to vtable for X"
— errore non ovvio che sembra un problema di linking, non di CMake.

```cmake
# In CMakeLists.txt — widget header-only con Q_OBJECT
set(CPP_SRCS
    mainwindow.cpp
    ai_client.cpp
    # ... altri .cpp ...

    # Widget header-only: AUTOMOC non li trova da solo
    widgets/spinner_widget.h
    widgets/status_badge.h
    widgets/toggle_switch.h
)
```

**Riferimento**: `gui/CMakeLists.txt:75-83` — 8 widget header-only elencati.
**Bug se non rispettato**: `undefined reference to vtable for SpinnerWidget` al link.

---

### B6. QThread::idealThreadCount() — mai nproc via shell

**Regola**: per il numero di job di compilazione paralleli, usare
`QThread::idealThreadCount()` invece di chiamare `$(nproc)` via shell.
Più portabile (funziona su Windows), nessuna shell injection possibile.

```cpp
// SBAGLIATO — richiede shell, non funziona su Windows
proc->start("cmake", {"--build", dir, "-j", "$(nproc)"});

// CORRETTO — Qt cross-platform
const int jobs = qMax(1, QThread::idealThreadCount());
proc->start("cmake", {"--build", dir, "-j", QString::number(jobs)});
```

**Riferimento**: `gui/pages/personalizza_page.cpp:215` — `QThread::idealThreadCount()`.

---

## GESTIONE LLM (integrazioni)

### B7. m_ai->busy() — guard obbligatoria prima di ogni chat()

**Regola**: ogni funzione che chiama `m_ai->chat()` deve controllare
`m_ai->busy()` prima. Senza guard, click rapidi o segnali sovrapposti
avviano più richieste parallele sulla stessa connessione.

Due strategie valide — scegliere una e applicarla in modo coerente:

```cpp
// STRATEGIA A — rifiuta la seconda richiesta (preferita per pipeline)
void MyPage::onSendClicked() {
    if (m_ai->busy()) {
        m_statusLabel->setText("Risposta in corso...");
        return;
    }
    m_ai->chat(sys, msg);
}

// STRATEGIA B — interrompe la corrente e avvia la nuova (preferita per chat rapida)
void MyPage::onSendClicked() {
    if (m_ai->busy()) m_ai->abort();
    m_ai->chat(sys, msg);
    m_myReqId = m_ai->currentReqId();
}
```

**Non lasciare mai un invio senza guard**: il secondo `chat()` crea un
secondo `QNetworkReply` mentre il primo è ancora connesso — i token si
mescolano e lo stato `m_reply` diventa incoerente.

**Riferimento**: `gui/pages/oracolo_page.cpp:444`, `programmazione_page.cpp:584`,
`strumenti_page.cpp:1099` — tutti i punti di invio con guard.

---

### B8. Throttle anti-spam — 400ms già in AiClient

**Regola**: AiClient ha già un throttle interno di 400ms per `chat()`,
`generate()` e `fetchEmbedding()`. Se una chiamata arriva entro 400ms
dalla precedente, viene ignorata silenziosamente (ritorna `m_reqId` attuale).

Questo è trasparente al chiamante — **non aggiungere un secondo throttle
manuale** nelle pagine: creerebbe un'asimmetria dove il UI si aspetta
una risposta che AiClient ha silenziosamente scartato.

```cpp
// In ai_client.cpp — throttle già presente (non duplicare)
if (m_throttleTimer.isValid() && m_throttleTimer.elapsed() < 400) return m_reqId;
m_throttleTimer.restart();
```

**Implicazione**: i test di "rapid-fire" devono chiamare `resetBusy()`
(via `MockAiClient`) tra una chiamata e l'altra per resettare il timer.

**Riferimento**: `gui/ai_client.cpp:304,524,596`.

---

### B9. Cache modelli — non chiamare fetchModels() ripetutamente

**Regola**: `fetchModels()` ha una cache interna TTL 30s (`m_cacheValid`).
Se backend e porta non sono cambiati e sono passati meno di 30s,
emette subito `modelsReady` dalla cache — nessuna chiamata HTTP.

Il chiamante non deve implementare la propria cache o aggiungere
guard `if (!m_cacheValid)`: quella logica è già in AiClient.

```cpp
// CORRETTO — chiama fetchModels() ogni volta che serve la lista
// AiClient decide autonomamente se fare la chiamata HTTP o servire la cache
connect(m_ai, &AiClient::modelsReady, this, [this](const QStringList& m) {
    populateCombo(m);
});
m_ai->fetchModels();   // se chiamato entro 30s, usa la cache

// SBAGLIATO — guard manuale ridondante che può desincronizzarsi
if (!m_modelsCached) {
    m_ai->fetchModels();
    m_modelsCached = true;
}
```

**La cache viene invalidata automaticamente** quando: backend cambia,
porta cambia, la chiamata HTTP fallisce.

**Riferimento**: `gui/ai_client.cpp:185-236`, `gui/ai_client.h:188-195`.

---

## TESTING

### B10. MockAiClient — test di streaming senza rete

**Regola**: per testare componenti che usano AiClient, usare `MockAiClient`
invece di creare un vero AiClient. MockAiClient sovrascrive `fetchModels()`
e offre metodi manuali `emitToken/emitFinished/emitError`.

```cpp
// Struttura base di un test con MockAiClient
class MyPageTest : public QObject {
    Q_OBJECT
private slots:
    void testStreamingUpdatesLog() {
        auto* ai  = new MockAiClient;
        auto* page = new MyPage(ai);

        ai->resetBusy();           // resetta busy dopo il costruttore
        page->startRequest("ciao");

        // Simula streaming
        ai->emitToken("prima ");
        ai->emitToken("parola");
        ai->emitFinished("prima parola");

        QVERIFY(page->logText().contains("prima parola"));
    }
};
```

**Tre classi di bug che MockAiClient rivela**:
- **Dangling Observer**: token ricevuti dopo che la pagina è stata distrutta
- **Signal Leakage**: token di una richiesta ricevuti da una pagina non attiva
- **Invariant Violation**: stato UI non coerente con stato logico

**Riferimento**: `gui/tests/mock_ai_client.h` — implementazione completa con commenti.

---

### B11. Vettori embedding — assi distinti per test deterministici

**Regola**: nei test che verificano top-1 con cosine similarity,
usare vettori su assi diversi (un solo componente non-zero per vettore,
su indici distinti). Vettori con stesso asse ma magnitudini diverse
hanno cosine similarity = 1.0 tra loro — top-1 diventa non deterministico.

```cpp
// SBAGLIATO — tutti su asse 0, cosine similarity identica
QVector<float> e0 = makeEmb(d, 1.0f);  // [1, 0, 0, ...]
QVector<float> e1 = makeEmb(d, 2.0f);  // [2, 0, 0, ...]  ← stessa direzione
QVector<float> e2 = makeEmb(d, 3.0f);  // [3, 0, 0, ...]  ← stessa direzione
// top-1 per query makeEmb(d, 10.0f): risultato arbitrario tra e0, e1, e2

// CORRETTO — assi separati, similarità univoca
QVector<float> e0(d, 0.0f); e0[0] = 1.0f;  // asse 0
QVector<float> e1(d, 0.0f); e1[1] = 1.0f;  // asse 1
QVector<float> e2(d, 0.0f); e2[2] = 1.0f;  // asse 2
// query su asse 0 → top-1 = e0, deterministico al 100%
```

**Riferimento**: `gui/tests/test_rag_engine.cpp` — fix al test 11 (`alreadyIndexedSearch`).
La funzione `makeEmb(d, val)` è corretta solo per test che NON verificano top-1.

---

### B12. Test funzioni pure — nessun mock necessario

**Regola**: le funzioni pure (nessun side effect, nessuna rete) si testano
direttamente senza mock. Non serve `MockAiClient`, non serve `QApplication`.
Queste funzioni sono le più veloci e affidabili da testare.

Funzioni pure già nel codebase che si prestano a test diretti:

```cpp
// Tutte testabili con QCOMPARE/QVERIFY senza setup
AgentiPage::extractPythonCode(text)     // estrae blocco ```python
AgentiPage::markdownToHtml(md)          // converte markdown in HTML
LavoroData::offerteFiltrate(tipo, liv)  // filtra offerte per tipo/livello
LavoroData::tipoIcon(tipo)              // mappa tipo → emoji
LavoroData::livLabel(livello)           // mappa livello → label
RagEngine::search(query, k)             // ricerca cosine similarity
AiClient::classifyQuery(text)           // classifica query Simple/Complex/Auto
```

```cpp
// Esempio — test classifyQuery senza nessun mock
void TestClassify::simpleQuery() {
    QCOMPARE(AiClient::classifyQuery("ciao"),
             AiClient::QuerySimple);    // ≤30 chars, no keywords
}
void TestClassify::complexQuery() {
    QCOMPARE(AiClient::classifyQuery("spiega la differenza tra TCP e UDP"),
             AiClient::QueryComplex);   // keyword "spiega" + "differenza"
}
```

**Riferimento**: `gui/tests/test_code_utils.cpp` — 14 test su `extractPythonCode`,
`gui/tests/test_lavoro_page.cpp` — test su `offerteFiltrate`.

---

### B13. Confronto float nei test — mai == diretto

**Regola**: per confrontare float nei test Qt usare `qFuzzyCompare` o
una soglia esplicita. `QCOMPARE` su `float` usa `==` esatto — fallisce
per differenze di arrotondamento tra piattaforme.

```cpp
// SBAGLIATO — fallisce su alcune piattaforme per floating point
QCOMPARE(result.score, 1.0f);

// CORRETTO — soglia esplicita
QVERIFY(qAbs(result.score - 1.0f) < 1e-5f);

// OPPURE — qFuzzyCompare (soglia relativa ~1e-5)
QVERIFY(qFuzzyCompare(result.score, 1.0f));
```

**Attenzione**: `qFuzzyCompare(0.0f, 0.0f)` restituisce `false` (bug Qt noto).
Per valori vicini a zero usare `qAbs(val) < 1e-6f` oppure
`qFuzzyIsNull(val)`.

**Riferimento**: `gui/tests/test_rag_engine.cpp` — confronti su score cosine similarity.

---

## MANUTENIBILITÀ

### B14. File oltre 2000 righe — soglia di split

**Regola**: quando un file supera le 2000 righe, valutare lo split.
La soglia non è arbitraria: oltre 2000 righe i tempi di compilazione
incrementale aumentano sensibilmente e ogni modifica ricompila l'intero file.

Strategia di split già applicata nel progetto:

| File originale | Dimensione | Split applicato |
|---|---|---|
| `agenti_page.cpp` | ~4600 r. | → 12 moduli per funzionalità |
| `grafico_page.cpp` | 6081 r. | → canvas + page |
| `simulatore_page.cpp` | 4712 r. | → algos + page |

File ancora sopra soglia da considerare:
- `impostazioni_page.cpp` — 5240 righe → candidato a split per tab
  (`buildRagTab` → `rag_tab.cpp`, `buildVoiceTab` → `voice_tab.cpp`, ecc.)

**Criterio di split**: separare per **responsabilità**, non per dimensione.
Un file con due responsabilità distinte va sempre splittato,
anche se è corto. Un file lungo con una sola responsabilità coesa
è accettabile.

---

### B15. Font custom — caricamento automatico da fonts/

**Regola**: i font `.ttf`/`.otf` vanno copiati in `gui/fonts/` e vengono
caricati automaticamente da `main.cpp` tramite `QFontDatabase::addApplicationFont`.
Non serve codice aggiuntivo: il loop carica tutto ciò che trova nella cartella.

```cpp
// In main.cpp — caricamento automatico già implementato
QDir fontsDir(QCoreApplication::applicationDirPath() + "/fonts");
for (const QFileInfo& fi : fontsDir.entryInfoList({"*.ttf","*.otf"}, QDir::Files))
    QFontDatabase::addApplicationFont(fi.absoluteFilePath());
```

Per usare il font nel QSS basta il nome famiglia:
```css
QLabel { font-family: "Inter", "Segoe UI", sans-serif; }
```

**Il font deve esistere sul sistema O essere in fonts/** per funzionare.
Il fallback (secondo font nella lista CSS) garantisce la portabilità
su macchine senza il font installato.

**Riferimento**: `gui/main.cpp:6-13`, `gui/fonts/LEGGIMI.txt`.
