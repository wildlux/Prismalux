# Best Practice — User Experience (Prismalux Qt GUI)

Pattern UX derivati dal codebase esistente. Ogni regola ha un riferimento
al file dove il pattern è già implementato correttamente.

---

## 1. Operazioni lente — mai bloccare il thread UI

**Regola**: qualsiasi operazione che può durare più di ~100ms deve essere asincrona.
`waitForStarted()` e `waitForFinished()` congelano l'interfaccia.

```cpp
// SBAGLIATO
proc.start("cmake", args);
proc.waitForFinished(20000);  // UI congelata 20s

// CORRETTO — polling asincrono
auto* proc = new QProcess(this);
connect(proc, &QProcess::finished, this, [this, proc](int code) {
    proc->deleteLater();
    onBuildFinished(code);
});
proc->start("cmake", args);
```

**Riferimento**: `gui/mainwindow.cpp` — polling `/health` llama-server con QTimer + QNetworkAccessManager.
Un solo NAM creato fuori dal loop (non uno per tick).

---

## 2. Spinner — sempre visibile durante operazioni di rete

**Regola**: ogni operazione che coinvolge l'AI (chat, fetch modelli, avvio server)
deve avere uno spinner animato nell'header. L'utente sa che il sistema sta lavorando.

```cpp
// Avvio
m_spinServer->start("Avvio server...");

// Fine con stato
m_spinServer->stop("✅ Pronto");       // successo
m_spinServer->stop("❌ Timeout");     // fallimento
m_spinServer->stop("❌ Errore");      // errore avvio
```

**Riferimento**: `gui/mainwindow.cpp` — `startLlamaServer()`.
Widget: `gui/widgets/spinner_widget.h` — animazione braille Unicode + QTimer (no risorse esterne).

---

## 3. Chat log — cursore sempre alla fine prima di insertHtml

**Regola**: `QTextBrowser::insertHtml()` inserisce alla posizione corrente del
cursore. Se l'utente ha cliccato nel log, il cursore non è alla fine.
Chiamare sempre `moveCursor(End)` prima di ogni inserimento.

```cpp
// SBAGLIATO — bubble inserite nel mezzo se utente ha cliccato
m_log->insertHtml(bubbleHtml);

// CORRETTO
m_log->moveCursor(QTextCursor::End);
m_log->insertHtml(bubbleHtml);
```

**Riferimento**: `gui/pages/agenti_page_bubbles.cpp`.
**Conseguenza se non rispettato**: bubble dell'agente B appaiono in mezzo alla risposta dell'agente A.

---

## 4. Abort AI — rimuovere testo parziale dal log

**Regola**: `m_ai->abort()` emette `aborted()`, NON `finished()`.
Il testo parziale (token non formattati) deve essere rimosso da `m_agentBlockStart`
alla fine del documento.

```cpp
connect(m_ai, &AiClient::aborted, this, [this]() {
    // Rimuovi testo parziale
    QTextCursor cur = m_log->textCursor();
    cur.setPosition(m_agentBlockStart);
    cur.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    cur.removeSelectedText();
    m_opMode = OpMode::Idle;
    updateButtons();
});
```

**Riferimento**: `gui/pages/agenti_page_stream.cpp` — `onAborted()`.
**Guard necessaria**: `if (m_opMode == OpMode::Idle) return;` all'inizio di `onFinished()`.

---

## 5. Selezione modello — sincronizzazione unica fonte

**Regola**: tutti i QComboBox modello in una pagina si aggiornano tramite un unico
handler di `AiClient::modelChanged`. Senza `blockSignals(true/false)` si genera
un loop di feedback.

```cpp
auto syncCombo = [](QComboBox* combo, const QString& newModel) {
    if (!combo) return;
    int idx = combo->findData(newModel);
    if (idx < 0) idx = combo->findText(newModel, Qt::MatchContains);
    if (idx >= 0 && idx != combo->currentIndex()) {
        combo->blockSignals(true);
        combo->setCurrentIndex(idx);
        combo->blockSignals(false);
    } else if (idx < 0) {
        // Modello non in lista — aggiorna item 0 come placeholder
        combo->blockSignals(true);
        combo->setItemText(0, newModel);
        combo->setItemData(0, newModel);
        combo->setCurrentIndex(0);
        combo->blockSignals(false);
    }
};
connect(m_ai, &AiClient::modelChanged, this, [this, syncCombo](const QString& m) {
    syncCombo(m_cmbLLM,  m);
    syncCombo(m_cmbTool, m);
});
```

**Riferimento**: `gui/pages/programmazione_page.cpp` ~718 — syncCombo con 4 combo.

---

## 6. Backend-aware — mai hardcode AiClient::Ollama

**Regola**: quando si cambia solo il modello (non il backend), usare `m_ai->backend()`.
Hardcodare `AiClient::Ollama` resetta il backend se l'utente sta usando llama-server.

```cpp
// SBAGLIATO
m_ai->setBackend(AiClient::Ollama, host, port, nuovoModello);

// CORRETTO
m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), nuovoModello);
```

**Riferimento**: `gui/pages/agenti_page_pipeline.cpp` — `runAgent()`.

---

## 7. Repolish — unica funzione per refresh QSS

**Regola**: dopo ogni `setProperty()` che influenza il foglio di stile QSS,
chiamare `P::repolish(widget)` invece di duplicare unpolish+polish.

```cpp
// SBAGLIATO
widget->style()->unpolish(widget);
widget->style()->polish(widget);

// CORRETTO
P::repolish(widget);  // da prismalux_paths.h
```

**Riferimento**: `gui/mainwindow.cpp` ~1204 — `m_btnBackend` dopo cambio property.
`gui/prismalux_paths.h` — implementazione `repolish()`.

---

## 8. Enter key — azione rapida, non pipeline completa

**Regola**: il tasto Invio in un campo input deve eseguire l'azione più veloce
(1 agente), non la pipeline completa (6 agenti in sequenza).
La pipeline lenta richiede click esplicito.

```cpp
// In agenti_page_ui.cpp
m_input->installEventFilter(new EnterFilter(m_btnQuick, m_input));
// EnterFilter: se Enter && input non vuoto → click su btnQuick (⚡ Singolo)
// La pipeline ▶ Avvia richiede click esplicito del mouse
```

**Riferimento**: `gui/pages/agenti_page_ui.cpp` — `EnterFilter`.

---

## 9. Messaggi di errore — specifici, mai generici

**Regola**: i messaggi di errore devono indicare cosa fare, non solo che c'è stato un errore.

```cpp
// SBAGLIATO
m_statusLabel->setText("Errore embedding.");

// CORRETTO
m_statusLabel->setText(
    "Embedding falliti — installa il modello dedicato:\n"
    "  ollama pull nomic-embed-text\n"
    "Poi reindicizza i documenti.");
```

**Riferimento**: `gui/pages/impostazioni_page.cpp` — `refreshStatus()` nel tab RAG.
Pattern: se `cnt == 0 && lastIdx != "Mai"` → avviso rosso con hint specifico.

---

## 10. Polling asincrono — un solo NAM per tutti i tick

**Regola**: non creare un `QNetworkAccessManager` per ogni tick del timer.
Crearne uno fuori dal loop e catturarlo nel lambda.

```cpp
// SBAGLIATO — 30 NAM creati in 30 secondi
connect(timer, &QTimer::timeout, this, [this, port]() {
    auto* nam = new QNetworkAccessManager(this);  // ← uno per tick
    nam->get(QNetworkRequest(healthUrl));
});

// CORRETTO — un solo NAM
auto* nam = new QNetworkAccessManager(this);
connect(timer, &QTimer::timeout, this, [this, nam, port]() {
    nam->get(QNetworkRequest(healthUrl));
});
```

**Riferimento**: `gui/mainwindow.cpp` — polling `/health` llama-server.

---

## 11. Proprietà timer — setProperty invece di new int()

**Regola**: per counter nei lambda QTimer, usare `setProperty` invece di
allocare `new int(0)` sul heap con delete manuale.

```cpp
// SBAGLIATO
auto* count = new int(0);
connect(timer, &QTimer::timeout, this, [count]() {
    if (++(*count) >= 30) { delete count; timer->stop(); }
});

// CORRETTO
timer->setProperty("ticks", 0);
connect(timer, &QTimer::timeout, this, [timer]() {
    int ticks = timer->property("ticks").toInt() + 1;
    timer->setProperty("ticks", ticks);
    if (ticks >= 30) timer->stop();
});
```

**Riferimento**: `gui/mainwindow.cpp` ~1128.

---

## 12. Exec button — abilitato solo con codice reale

**Regola**: i pulsanti "Esegui in App" si abilitano solo se la risposta AI contiene
almeno un blocco backtick (` ``` `). Testo puro non contiene codice eseguibile.

```cpp
const bool hasBlock = response.contains("```");
m_btnExecBlender->setEnabled(hasBlock);
m_btnExecOffice->setEnabled(hasBlock);
```

**Riferimento**: `gui/pages/app_controller_page.cpp` — `hasBlock` guard in `runAi()`.

---

## 13. Font — monospace solo per chat log

**Regola**: l'intera UI usa sans-serif (`Inter, Segoe UI, Ubuntu`).
Il monospace (`JetBrains Mono, Fira Code, Consolas`) è riservato
esclusivamente a `#chatLog`.

```css
/* In style.qss / tutti i temi */
#chatLog {
    font-family: "JetBrains Mono", "Fira Code", Consolas, monospace;
}
```

**Riferimento**: `gui/style.qss` + `gui/themes/*.qss`.

---

## 14. Status badge — dot colorato per stati binari

**Regola**: per stati Offline/Online/Starting/Error usare `StatusBadge`
invece di cambiare testo o colore di un QLabel manualmente.

```cpp
// Preferito
auto* badge = new StatusBadge(StatusBadge::Offline, "Ollama", this);
badge->setState(StatusBadge::Online);  // cambia dot + tooltip
```

**Riferimento**: `gui/widgets/status_badge.h`.
