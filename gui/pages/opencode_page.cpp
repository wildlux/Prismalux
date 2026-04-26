#include "opencode_page.h"
#include "../prismalux_paths.h"
namespace P = PrismaluxPaths;

#include <QSplitter>
#include <QGroupBox>
#include <QTextCursor>
#include <QFileInfo>
#include <QMessageBox>
#include <QScrollBar>

/* ══════════════════════════════════════════════════════════════
   Costanti
   ══════════════════════════════════════════════════════════════ */
static constexpr int kPollMaxTicks = 30;   // 30 × 1s = 30s timeout avvio
static constexpr int kPollIntervalMs = 1000;


/* ══════════════════════════════════════════════════════════════
   Costruttore / Distruttore
   ══════════════════════════════════════════════════════════════ */
OpenCodePage::OpenCodePage(QWidget* parent) : QWidget(parent) {
     m_nam = new QNetworkAccessManager(this);
     m_pollErrorCount = 0;

    /* ── Top bar: stato + avvia/ferma ── */
    auto* topBar = new QHBoxLayout;
    m_startBtn  = new QPushButton("\xf0\x9f\x9f\xa2  Avvia OpenCode", this);
    m_statusLbl = new QLabel("\xe2\x97\x8f  Offline", this);
    m_statusLbl->setStyleSheet("color:#ef4444; font-weight:bold;");
    topBar->addWidget(m_startBtn);
    topBar->addWidget(m_statusLbl);
    topBar->addStretch();

    /* ── Riga configurazione ── */
    auto* cfgRow = new QHBoxLayout;

    /* Modello: combo con default dal config opencode, editabile */
    cfgRow->addWidget(new QLabel("Modello:", this));
    m_modelCombo = new QComboBox(this);
    m_modelCombo->setEditable(true);
    m_modelCombo->setMinimumWidth(200);
    /* Popolato da fetchModels() una volta avviato il server;
       nel frattempo usa il modello del config ~/.opencode/config.json */
    {
        QSettings oc(QDir::homePath() + "/.opencode/config.json",
                     QSettings::IniFormat);
        const QString saved = oc.value("model", "ollama/qwen2.5-coder:latest").toString();
        /* Il config usa "qwen2.5-coder:latest" (senza provider): aggiungiamo il prefisso */
        const QString dflt = saved.contains('/') ? saved : "ollama/" + saved;
        m_modelCombo->addItem(dflt);
        m_modelCombo->setCurrentText(dflt);
    }
    cfgRow->addWidget(m_modelCombo);

    /* Directory di lavoro */
    cfgRow->addWidget(new QLabel("  Dir:", this));
    m_cwdEdit = new QLineEdit(P::root(), this);
    m_cwdEdit->setMinimumWidth(260);
    cfgRow->addWidget(m_cwdEdit, 1);
    auto* cwdBtn = new QPushButton("\xf0\x9f\x93\x82", this);
    cwdBtn->setFixedWidth(32);
    cwdBtn->setToolTip("Sfoglia directory");
    cfgRow->addWidget(cwdBtn);

    /* ── Splitter: log a sinistra, sessioni a destra ── */
    auto* splitter = new QSplitter(Qt::Horizontal, this);

    /* Log chat */
    m_log = new QTextBrowser(this);
    m_log->setObjectName("chatLog");
    m_log->setOpenLinks(false);
    splitter->addWidget(m_log);

    /* Pannello sessioni */
    auto* sessPanel = new QWidget(this);
    auto* sessLay   = new QVBoxLayout(sessPanel);
    sessLay->setContentsMargins(4, 0, 0, 0);
    sessLay->addWidget(new QLabel("<b>Sessioni recenti</b>", this));
    m_sessList = new QListWidget(this);
    m_sessList->setMaximumWidth(220);
    sessLay->addWidget(m_sessList, 1);
    auto* refreshBtn = new QPushButton("\xf0\x9f\x94\x84  Aggiorna", this);
    sessLay->addWidget(refreshBtn);
    splitter->addWidget(sessPanel);

    splitter->setStretchFactor(0, 4);
    splitter->setStretchFactor(1, 1);

    /* ── Input area ── */
    m_input = new QTextEdit(this);
    m_input->setPlaceholderText(
        "Descrivi il task di codice... (Es: \"Aggiungi un metodo toString() alla classe Foo in src/foo.cpp\")");
    m_input->setFixedHeight(90);

    auto* btnRow = new QHBoxLayout;
    m_sendBtn  = new QPushButton("\xe2\x96\xb6  Invia", this);
    m_abortBtn = new QPushButton("\xe2\x8f\xb9  Ferma", this);
    m_sendBtn->setEnabled(false);
    m_abortBtn->setEnabled(false);
    m_sendBtn->setDefault(true);
    btnRow->addWidget(m_sendBtn);
    btnRow->addWidget(m_abortBtn);
    btnRow->addStretch();

    /* ── Layout principale ── */
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(4);
    lay->addLayout(topBar);
    lay->addLayout(cfgRow);
    lay->addWidget(splitter, 1);
    lay->addWidget(m_input);
    lay->addLayout(btnRow);

    /* ── Connessioni ── */
    connect(m_startBtn,  &QPushButton::clicked, this, &OpenCodePage::onStartStop);
    connect(m_sendBtn,   &QPushButton::clicked, this, &OpenCodePage::onSendClicked);
    connect(m_abortBtn,  &QPushButton::clicked, this, &OpenCodePage::onAbortClicked);
    connect(refreshBtn,  &QPushButton::clicked, this, &OpenCodePage::onSessionsRefresh);
    connect(cwdBtn,      &QPushButton::clicked, this, &OpenCodePage::onCwdBrowse);
    connect(m_sessList,  &QListWidget::itemDoubleClicked,
            this, &OpenCodePage::onSessionSelected);
}

OpenCodePage::~OpenCodePage() {
    stopServer();
    if (m_pollTimer) {
        m_pollTimer->stop();
        m_pollTimer->deleteLater();
        m_pollTimer = nullptr;
    }
}


/* ══════════════════════════════════════════════════════════════
   Server lifecycle
   ══════════════════════════════════════════════════════════════ */
void OpenCodePage::onStartStop() {
    if (m_running) stopServer();
    else           startServer();
}

void OpenCodePage::startServer() {
    const QString bin = P::openCodeBin();
    if (!QFileInfo::exists(bin)) {
        appendSystem(
            "<span style='color:#ef4444'>\xe2\x9d\x8c  OpenCode non trovato in: "
            + bin.toHtmlEscaped()
            + "<br>Installalo con: <code>curl -fsSL https://opencode.ai/install | bash</code></span>");
        return;
    }

    m_proc = new QProcess(this);
    m_proc->setProgram(bin);
    m_proc->setArguments({
        "serve",
        "--port", QString::number(m_port)
    });
    m_proc->setWorkingDirectory(m_cwdEdit->text().trimmed());

    connect(m_proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError err) {
        if (err == QProcess::FailedToStart) {
            m_statusLbl->setText("\xe2\x97\x8f  Errore avvio");
            m_statusLbl->setStyleSheet("color:#ef4444; font-weight:bold;");
            appendSystem("<span style='color:#ef4444'>\xe2\x9d\x8c  opencode serve: FailedToStart</span>");
            m_proc->deleteLater(); m_proc = nullptr;
        }
    });
    connect(m_proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int code, QProcess::ExitStatus) {
        disconnectSseStream();
        setServerState(false);
        appendSystem(QString("<span style='color:#f59e0b'>"
                             "\xe2\x9a\xa0  OpenCode terminato (exit %1)</span>").arg(code));
        if (m_pollTimer) { m_pollTimer->stop(); }
        m_proc->deleteLater(); m_proc = nullptr;
    });

    m_proc->start();
    m_statusLbl->setText("\xe2\x97\x90  Avvio in corso...");
    m_statusLbl->setStyleSheet("color:#f59e0b; font-weight:bold;");
    m_startBtn->setEnabled(false);

    /* Poll /session/status ogni secondo finché il server risponde */
    m_pollTicks = 0;
    if (!m_pollTimer) {
        m_pollTimer = new QTimer(this);
        m_pollTimer->setInterval(kPollIntervalMs);
        connect(m_pollTimer, &QTimer::timeout, this, &OpenCodePage::onPollTick);
    }
    m_pollTimer->start();
}

void OpenCodePage::stopServer() {
    if (m_pollTimer) m_pollTimer->stop();
    disconnectSseStream();
    if (m_proc && m_proc->state() != QProcess::NotRunning) {
        m_proc->terminate();
        if (!m_proc->waitForFinished(3000))
            m_proc->kill();
    }
    setServerState(false);
}

void OpenCodePage::onPollTick() {
     if (++m_pollTicks > kPollMaxTicks) {
         m_pollTimer->stop();
         m_statusLbl->setText("\xe2\x97\x8f  Timeout avvio");
         m_statusLbl->setStyleSheet("color:#ef4444; font-weight:bold;");
         m_startBtn->setEnabled(true);
         appendSystem("<span style='color:#ef4444'>\xe2\x9d\x8c  OpenCode non risponde entro 30s.</span>");
         return;
     }

     auto* reply = m_nam->get(QNetworkRequest(QUrl(baseUrl() + "/session/status")));
     connect(reply, &QNetworkReply::finished, this, [this, reply] {
         reply->deleteLater();
         if (reply->error() == QNetworkReply::NoError) {
             m_pollErrorCount = 0; // Reset error counter on success
             m_pollTimer->stop();
             setServerState(true);
             connectSseStream();
             fetchModels();
             fetchSessions();
             appendSystem("<span style='color:#22c55e'>\xe2\x9c\x85  OpenCode pronto su "
                          + baseUrl().toHtmlEscaped() + "</span>");
         } else {
             // Handle network errors with retry limit
             m_pollErrorCount++;
             if (m_pollErrorCount >= 5) { // Max 5 consecutive errors
                 m_pollTimer->stop();
                 m_statusLbl->setText("\xe2\x97\x8f  Errore di rete");
                 m_statusLbl->setStyleSheet("color:#ef4444; font-weight:bold;");
                 m_startBtn->setEnabled(true);
                 appendSystem(QString("<span style='color:#ef4444'>\xe2\x9d\x8c  Impossibile connettersi a OpenCode dopo %1 tentativi: %2</span>")
                              .arg(m_pollErrorCount)
                              .arg(reply->errorString().toHtmlEscaped()));
                 return;
             }
             /* altrimenti aspetta il prossimo tick */
         }
     });
 }

void OpenCodePage::setServerState(bool running) {
    m_running = running;
    m_startBtn->setEnabled(true);
    m_startBtn->setText(running
        ? "\xf0\x9f\x94\xb4  Ferma OpenCode"
        : "\xf0\x9f\x9f\xa2  Avvia OpenCode");
    m_statusLbl->setText(running
        ? "\xe2\x97\x8f  Online"
        : "\xe2\x97\x8f  Offline");
    m_statusLbl->setStyleSheet(running
        ? "color:#22c55e; font-weight:bold;"
        : "color:#ef4444; font-weight:bold;");
    m_sendBtn->setEnabled(running);
    m_cwdEdit->setEnabled(!running);
}

QString OpenCodePage::baseUrl() const {
    return QString("http://127.0.0.1:%1").arg(m_port);
}


/* ══════════════════════════════════════════════════════════════
   SSE stream
   ══════════════════════════════════════════════════════════════ */
void OpenCodePage::connectSseStream() {
    disconnectSseStream();

    QNetworkRequest req(QUrl(baseUrl() + "/event"));
    req.setRawHeader("Accept", "text/event-stream");
    req.setRawHeader("Cache-Control", "no-cache");
    req.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);

    m_sse = m_nam->get(req);
    connect(m_sse, &QNetworkReply::readyRead, this, &OpenCodePage::onSseData);
    connect(m_sse, &QNetworkReply::finished,  this, &OpenCodePage::onSseDone);
}

void OpenCodePage::disconnectSseStream() {
    if (!m_sse) return;
    m_sse->abort();
    m_sse->deleteLater();
    m_sse = nullptr;
    m_sseBuf.clear();
}

void OpenCodePage::onSseData() {
    if (!m_sse) return;
    m_sseBuf += m_sse->readAll();

    /* Processa riga per riga: ogni evento SSE termina con \n o \n\n */
    while (true) {
        int idx = m_sseBuf.indexOf('\n');
        if (idx < 0) break;
        const QByteArray line = m_sseBuf.left(idx).trimmed();
        m_sseBuf.remove(0, idx + 1);
        if (!line.isEmpty())
            processLine(line);
    }
}

void OpenCodePage::onSseDone() {
    /* Il server ha chiuso la connessione SSE — non è un errore se il server è in esecuzione */
    if (m_running) {
        /* Riconnetti dopo un breve delay */
        QTimer::singleShot(1500, this, &OpenCodePage::connectSseStream);
    }
}

void OpenCodePage::processLine(const QByteArray& line) {
    /* Formato SSE: "data: {...}" oppure "event: nome" (ignorato) */
    if (!line.startsWith("data: ")) return;
    const QByteArray payload = line.mid(6);

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(payload, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return;

    handleEvent(doc.object());
}

void OpenCodePage::handleEvent(const QJsonObject& ev) {
    /* Formato SSE OpenCode:
       {"type":"<tipo>","properties":{...}}
       Le properties variano per tipo. */
    const QString type = ev["type"].toString();
    const QJsonObject props = ev["properties"].toObject();

    /* ── Messaggio creato: distingui user vs assistant ── */
    if (type == "message.updated") {
        const QJsonObject info = props["info"].toObject();
        if (info["role"].toString() == "assistant")
            m_aiMsgId = info["id"].toString();
        return;
    }

    /* ── Parte di messaggio (token) ── */
    if (type == "message.part.updated") {
        const QJsonObject part = props["part"].toObject();
        /* Mostra solo token del messaggio AI, non l'echo del messaggio utente */
        if (!m_aiMsgId.isEmpty() && part["messageID"].toString() != m_aiMsgId)
            return;
        if (part["type"].toString() != "text") return;
        const QString text = part["text"].toString();
        if (!text.isEmpty()) appendToken(text);
        return;
    }

    /* ── Sessione tornata idle = risposta completata ── */
    if (type == "session.idle") {
        finalizeBlock();
        m_abortBtn->setEnabled(false);
        m_sendBtn->setEnabled(true);
        m_input->setEnabled(true);
        m_aiMsgId.clear();
        return;
    }

    /* ── Errore ── */
    if (type == "session.error") {
        const QJsonObject err = props["error"].toObject();
        const QString msg = err["data"].toObject()["message"].toString();
        finalizeBlock();
        appendSystem("<span style='color:#ef4444'>\xe2\x9d\x8c  Errore OpenCode: "
                     + (msg.isEmpty() ? err["name"].toString() : msg).toHtmlEscaped()
                     + "</span>");
        m_abortBtn->setEnabled(false);
        m_sendBtn->setEnabled(true);
        m_input->setEnabled(true);
        m_aiMsgId.clear();
        return;
    }

    /* ── Tool use: lettura/scrittura file, comandi shell ── */
    if (type == "message.part.updated") return;  // già gestito sopra
    if (type == "tool.use" || type == "tool.call") {
        const QString tool = ev["name"].toString();
        const QJsonObject input = ev["input"].toObject();
        QString info;
        if (tool == "read" || tool == "write" || tool == "edit")
            info = input["file_path"].toString();
        else if (tool == "bash" || tool == "shell")
            info = input["command"].toString().left(60);
        if (!info.isEmpty())
            appendSystem("<span style='color:#60a5fa'>\xf0\x9f\x94\xa7  "
                         + tool.toHtmlEscaped() + ": <code>"
                         + info.toHtmlEscaped() + "</code></span>");
        return;
    }

    /* ── Aggiornamento sessione (title, stato busy) ── */
    if (type == "session.updated") {
        const QJsonObject info = props["info"].toObject();
        const QString id    = info["id"].toString();
        const QString title = info["title"].toString();
        if (id.isEmpty()) return;
        for (int i = 0; i < m_sessList->count(); i++) {
            if (m_sessList->item(i)->data(Qt::UserRole).toString() == id) {
                m_sessList->item(i)->setText(title.isEmpty() ? id.left(12) + "..." : title);
                return;
            }
        }
        /* Nuova sessione */
        auto* item = new QListWidgetItem(title.isEmpty() ? id.left(12) + "..." : title);
        item->setData(Qt::UserRole, id);
        m_sessList->insertItem(0, item);
        return;
    }

    /* Ignora heartbeat, server.connected, session.status, ecc. */
}


/* ══════════════════════════════════════════════════════════════
   Invio messaggio
   ══════════════════════════════════════════════════════════════ */
void OpenCodePage::onSendClicked() {
    const QString text = m_input->toPlainText().trimmed();
    if (text.isEmpty()) return;

    m_sendBtn->setEnabled(false);
    m_abortBtn->setEnabled(true);
    m_input->setEnabled(false);

    /* Mostra il task dell'utente nel log */
    appendSystem("<div style='background:#1e3a5f; border-left:3px solid #3b82f6; "
                 "padding:6px 10px; margin:4px 0; border-radius:4px;'>"
                 "<b style='color:#93c5fd'>\xf0\x9f\x91\xa4 Tu:</b> "
                 + text.toHtmlEscaped().replace("\n", "<br>") + "</div>");

    /* Separa provider e modello: "ollama/qwen2.5:3b" → {ollama, qwen2.5:3b} */
    const QString modelStr = m_modelCombo->currentText().trimmed();
    const int slash = modelStr.indexOf('/');
    const QString providerID = (slash >= 0) ? modelStr.left(slash)  : "ollama";
    const QString modelID    = (slash >= 0) ? modelStr.mid(slash+1) : modelStr;

    if (m_sessionId.isEmpty()) {
        /* Crea nuova sessione e poi invia */
        QJsonObject body;
        body["title"] = text.left(50);
        body["cwd"]   = m_cwdEdit->text().trimmed();
        body["model"] = QJsonObject{{"providerID", providerID}, {"modelID", modelID}};

        QNetworkRequest req(QUrl(baseUrl() + "/session"));
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        auto* reply = m_nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));

         connect(reply, &QNetworkReply::finished, this, [this, reply, text, modelID, providerID] {
             reply->deleteLater();
             if (reply->error() != QNetworkReply::NoError) {
                 appendSystem("<span style='color:#ef4444'>\xe2\x9d\x8c  Errore creazione sessione: "
                              + reply->errorString().toHtmlEscaped() + "</span>");
                 m_sendBtn->setEnabled(true); m_abortBtn->setEnabled(false);
                 m_input->setEnabled(true);
                 return;
             }
             const QJsonObject sess = QJsonDocument::fromJson(reply->readAll()).object();
             m_sessionId = sess["id"].toString();
             if (!m_sessionId.isEmpty())
                 sendMessage(m_sessionId);
         });
    } else {
        sendMessage(m_sessionId);
    }
}

void OpenCodePage::sendMessage(const QString& sessionId) {
    const QString text = m_input->toPlainText().trimmed();
    const QString modelStr = m_modelCombo->currentText().trimmed();
    const int slash = modelStr.indexOf('/');
    const QString providerID = (slash >= 0) ? modelStr.left(slash)  : "ollama";
    const QString modelID    = (slash >= 0) ? modelStr.mid(slash+1) : modelStr;

    /* Il body richiede "parts" (array di parti), non "text" direttamente */
    QJsonObject body;
    body["parts"] = QJsonArray{ QJsonObject{{"type", "text"}, {"text", text}} };
    body["model"] = QJsonObject{{"providerID", providerID}, {"modelID", modelID}};

    QNetworkRequest req(QUrl(baseUrl() + "/session/" + sessionId + "/message"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    auto* reply = m_nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));

    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            appendSystem("<span style='color:#ef4444'>\xe2\x9d\x8c  Errore invio messaggio: "
                         + reply->errorString().toHtmlEscaped() + "</span>");
            m_sendBtn->setEnabled(true); m_abortBtn->setEnabled(false);
            m_input->setEnabled(true);
        }
        /* Il completamento arriva via SSE, non qui */
    });

    m_input->clear();

    /* Apri blocco risposta nel log */
    m_inBlock = false;
    appendSystem("<div style='color:#a3e635; font-weight:bold; margin-top:8px;'>"
                 "\xf0\x9f\xa4\x96 OpenCode:</div>");
}

void OpenCodePage::onAbortClicked() {
    if (!m_sessionId.isEmpty())
        abortSession(m_sessionId);
    finalizeBlock();
    m_abortBtn->setEnabled(false);
    m_sendBtn->setEnabled(true);
    m_input->setEnabled(true);
}

void OpenCodePage::abortSession(const QString& sessionId) {
    QNetworkRequest req(QUrl(baseUrl() + "/session/" + sessionId + "/abort"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    auto* reply = m_nam->post(req, QByteArray("{}"));
    connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
}


/* ══════════════════════════════════════════════════════════════
   Sessioni
   ══════════════════════════════════════════════════════════════ */
void OpenCodePage::fetchSessions() {
    auto* reply = m_nam->get(QNetworkRequest(QUrl(baseUrl() + "/session")));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;

        const QJsonArray arr = QJsonDocument::fromJson(reply->readAll()).array();
        m_sessList->clear();
        for (auto v : arr) {
            const QJsonObject obj = v.toObject();
            const QString id    = obj["id"].toString();
            const QString title = obj["title"].toString();
            auto* item = new QListWidgetItem(title.isEmpty() ? id.left(16) + "..." : title);
            item->setData(Qt::UserRole, id);
            m_sessList->addItem(item);
        }
    });
}

void OpenCodePage::onSessionsRefresh() {
    if (m_running) fetchSessions();
}

void OpenCodePage::onSessionSelected(QListWidgetItem* item) {
    m_sessionId = item->data(Qt::UserRole).toString();
    m_log->clear();
    appendSystem("<span style='color:#60a5fa'>\xf0\x9f\x94\x81  Sessione ripristinata: "
                 + item->text().toHtmlEscaped() + "</span>");
}


/* ══════════════════════════════════════════════════════════════
   Modelli — carica da Ollama (GET /api/tags) e formatta "ollama/<name>"
   ══════════════════════════════════════════════════════════════ */
void OpenCodePage::fetchModels() {
    /* OpenCode non espone un endpoint JSON per i modelli.
       Leggiamo direttamente da Ollama e aggiungiamo il prefisso "ollama/". */
    const QString ollamaUrl = QString("http://%1:%2/api/tags")
                              .arg(P::kLocalHost).arg(P::kOllamaPort);
    auto* reply = m_nam->get(QNetworkRequest(QUrl(ollamaUrl)));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;

        const QJsonArray models =
            QJsonDocument::fromJson(reply->readAll()).object()["models"].toArray();
        if (models.isEmpty()) return;

        const QString cur = m_modelCombo->currentText();
        m_modelCombo->blockSignals(true);
        m_modelCombo->clear();

        for (auto v : models) {
            const QString name = v.toObject()["name"].toString();
            if (!name.isEmpty())
                m_modelCombo->addItem("ollama/" + name);
        }

        const int idx = m_modelCombo->findText(cur);
        if (idx >= 0) m_modelCombo->setCurrentIndex(idx);
        else if (m_modelCombo->count() > 0 && !cur.isEmpty())
            m_modelCombo->setCurrentText(cur);

        m_modelCombo->blockSignals(false);
    });
}


/* ══════════════════════════════════════════════════════════════
   Helpers UI
   ══════════════════════════════════════════════════════════════ */
void OpenCodePage::appendToken(const QString& text) {
    m_log->moveCursor(QTextCursor::End);
    if (!m_inBlock) {
        /* Apri span monospace per i token */
        m_log->insertHtml("<span style='font-family:\"JetBrains Mono\",monospace; "
                          "color:#e2e8f0;'>");
        m_inBlock = true;
    }
    /* Inserisce il testo grezzo con escaping HTML */
    m_log->insertHtml(text.toHtmlEscaped().replace("\n", "<br>"));
    m_log->verticalScrollBar()->setValue(m_log->verticalScrollBar()->maximum());
}

void OpenCodePage::finalizeBlock() {
    if (!m_inBlock) return;
    m_log->moveCursor(QTextCursor::End);
    m_log->insertHtml("</span><hr style='border:none;border-top:1px solid #334155;'>");
    m_inBlock = false;
}

void OpenCodePage::appendSystem(const QString& html) {
    finalizeBlock();
    m_log->moveCursor(QTextCursor::End);
    m_log->insertHtml("<div style='margin:2px 0;'>" + html + "</div>");
    m_log->verticalScrollBar()->setValue(m_log->verticalScrollBar()->maximum());
}


/* ══════════════════════════════════════════════════════════════
   Helpers UI — directory
   ══════════════════════════════════════════════════════════════ */
void OpenCodePage::onCwdBrowse() {
    const QString dir = QFileDialog::getExistingDirectory(
        this, "Seleziona directory di lavoro", m_cwdEdit->text());
    if (!dir.isEmpty()) m_cwdEdit->setText(dir);
}
