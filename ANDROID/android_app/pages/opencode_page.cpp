#include "opencode_page.h"
#include <QScrollBar>
#include <QDateTime>

/* ══════════════════════════════════════════════════════════════
   Costruttore — UI mobile: log in alto, input + tasti in basso.
   ══════════════════════════════════════════════════════════════ */
OpenCodePage::OpenCodePage(QWidget* parent)
    : QWidget(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    /* ── Barra stato (IP:porta + sessione) ── */
    m_statusLbl = new QLabel("OpenCode — nessuna sessione", this);
    m_statusLbl->setWordWrap(true);
    QFont sf = m_statusLbl->font();
    sf.setPointSize(11);
    m_statusLbl->setFont(sf);
    m_statusLbl->setStyleSheet("color:#64748b; padding:4px 0;");
    root->addWidget(m_statusLbl);

    /* ── Splitter: log | lista sessioni recenti ── */
    auto* splitter = new QSplitter(Qt::Vertical, this);
    splitter->setChildrenCollapsible(false);

    /* Log principale */
    m_log = new QTextBrowser(splitter);
    m_log->setOpenLinks(false);
    m_log->setFont(QFont("monospace", 11));
    m_log->setMinimumHeight(200);
    splitter->addWidget(m_log);

    /* Lista sessioni recenti (collassabile) */
    auto* sessBox   = new QWidget(splitter);
    auto* sessLayout = new QVBoxLayout(sessBox);
    sessLayout->setContentsMargins(0, 0, 0, 0);
    sessLayout->setSpacing(4);
    auto* sessHdr = new QHBoxLayout;
    auto* sessLbl = new QLabel("Sessioni recenti", sessBox);
    QFont lf = sessLbl->font();
    lf.setBold(true);
    sessLbl->setFont(lf);
    sessHdr->addWidget(sessLbl);
    sessHdr->addStretch();
    auto* refreshBtn = new QPushButton("Aggiorna", sessBox);
    refreshBtn->setFixedHeight(32);
    sessHdr->addWidget(refreshBtn);
    sessLayout->addLayout(sessHdr);
    m_sessList = new QListWidget(sessBox);
    m_sessList->setMaximumHeight(120);
    sessLayout->addWidget(m_sessList);
    splitter->addWidget(sessBox);

    splitter->setStretchFactor(0, 5);
    splitter->setStretchFactor(1, 1);
    root->addWidget(splitter, 1);

    /* ── Input ── */
    m_input = new QTextEdit(this);
    m_input->setPlaceholderText("Scrivi un task di codice…");
    m_input->setMaximumHeight(100);
    m_input->setFont(QFont("monospace", 12));
    root->addWidget(m_input);

    /* ── Tasti ── */
    auto* btnRow = new QHBoxLayout;
    m_newBtn   = new QPushButton("Nuova sessione", this);
    m_sendBtn  = new QPushButton("Invia", this);
    m_abortBtn = new QPushButton("Annulla", this);
    m_newBtn->setMinimumHeight(44);
    m_sendBtn->setMinimumHeight(44);
    m_abortBtn->setMinimumHeight(44);
    m_sendBtn->setStyleSheet("background:#2563eb; color:white; border-radius:6px; font-weight:bold;");
    m_abortBtn->setStyleSheet("background:#dc2626; color:white; border-radius:6px;");
    m_abortBtn->setEnabled(false);
    btnRow->addWidget(m_newBtn, 2);
    btnRow->addWidget(m_sendBtn, 3);
    btnRow->addWidget(m_abortBtn, 2);
    root->addLayout(btnRow);

    /* ── Connessioni ── */
    connect(m_sendBtn,   &QPushButton::clicked, this, &OpenCodePage::onSendClicked);
    connect(m_newBtn,    &QPushButton::clicked, this, &OpenCodePage::onNewSessionClicked);
    connect(m_abortBtn,  &QPushButton::clicked, this, &OpenCodePage::onAbortClicked);
    connect(refreshBtn,  &QPushButton::clicked, this, &OpenCodePage::onRefreshSessions);
    connect(m_sessList,  &QListWidget::itemClicked, this, &OpenCodePage::onSessionSelected);

    /* Timer retry SSE */
    m_sseRetry = new QTimer(this);
    m_sseRetry->setSingleShot(true);
    connect(m_sseRetry, &QTimer::timeout, this, &OpenCodePage::connectSse);

    /* Avvia subito: crea sessione e carica lista */
    createSession();
    fetchSessions();
}

/* ══════════════════════════════════════════════════════════════
   baseUrl — IP dal QSettings del desktop
   ══════════════════════════════════════════════════════════════ */
QString OpenCodePage::baseUrl() const
{
    QSettings s("Prismalux", "Mobile");
    const QString host = s.value("server/host", "192.168.1.165").toString();
    return QString("http://%1:%2").arg(host).arg(kOpenCodeMobilePort);
}

/* ══════════════════════════════════════════════════════════════
   createSession — POST /session
   ══════════════════════════════════════════════════════════════ */
void OpenCodePage::createSession()
{
    if (m_createReply) return;
    setStatus("Creazione sessione…");

    QNetworkRequest req(QUrl(baseUrl() + "/session"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    m_createReply = m_nam->post(req, QByteArray("{}"));
    connect(m_createReply, &QNetworkReply::finished, this, &OpenCodePage::onCreateSessionReply);
}

void OpenCodePage::onCreateSessionReply()
{
    auto* reply = m_createReply;
    m_createReply = nullptr;
    if (!reply) return;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        setStatus(QString("Errore connessione: %1\n"
                          "Assicurati che sul PC sia attivo 'opencode serve'.")
                  .arg(reply->errorString()), false);
        return;
    }
    const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
    m_sessionId = obj.value("id").toString();
    if (m_sessionId.isEmpty()) {
        setStatus("Sessione non ricevuta — risposta OpenCode non valida.", false);
        return;
    }
    setStatus(QString("Sessione: %1").arg(m_sessionId.left(8) + "…"));
    appendSystem(QString("<span style='color:#16a34a'>Sessione creata: %1</span>")
                 .arg(m_sessionId.left(16)));
    connectSse();
    fetchSessions();
}

/* ══════════════════════════════════════════════════════════════
   sendMessage — POST /session/{id}/message
   ══════════════════════════════════════════════════════════════ */
void OpenCodePage::onSendClicked()
{
    const QString text = m_input->toPlainText().trimmed();
    if (text.isEmpty() || m_sessionId.isEmpty()) return;
    m_input->clear();
    sendMessage();
    appendSystem(QString("<b>Tu:</b> %1").arg(text.toHtmlEscaped()));
    m_sendBtn->setEnabled(false);
    m_abortBtn->setEnabled(true);
}

void OpenCodePage::sendMessage()
{
    if (m_sendReply || m_sessionId.isEmpty()) return;
    const QString text = m_input->toPlainText().trimmed();
    if (text.isEmpty()) return;

    QNetworkRequest req(QUrl(baseUrl() + "/session/" + m_sessionId + "/message"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QJsonObject body;
    body["content"] = text;
    m_sendReply = m_nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(m_sendReply, &QNetworkReply::finished, this, &OpenCodePage::onSendMessageReply);
}

void OpenCodePage::onSendMessageReply()
{
    auto* reply = m_sendReply;
    m_sendReply = nullptr;
    if (reply) reply->deleteLater();
}

/* ══════════════════════════════════════════════════════════════
   SSE — GET /event (stream token)
   ══════════════════════════════════════════════════════════════ */
void OpenCodePage::connectSse()
{
    if (m_sse) return;
    m_sseBuf.clear();

    QNetworkRequest req(QUrl(baseUrl() + "/event"));
    req.setRawHeader("Accept", "text/event-stream");
    req.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                     QNetworkRequest::AlwaysNetwork);
    m_sse = m_nam->get(req);
    connect(m_sse, &QNetworkReply::readyRead, this, &OpenCodePage::onSseData);
    connect(m_sse, &QNetworkReply::finished,  this, &OpenCodePage::onSseDone);
}

void OpenCodePage::disconnectSse()
{
    if (!m_sse) return;
    m_sse->abort();
    m_sse->deleteLater();
    m_sse = nullptr;
    m_sseBuf.clear();
}

void OpenCodePage::onSseData()
{
    if (!m_sse) return;
    m_sseBuf += m_sse->readAll();
    while (true) {
        const int nl = m_sseBuf.indexOf('\n');
        if (nl < 0) break;
        const QByteArray line = m_sseBuf.left(nl).trimmed();
        m_sseBuf.remove(0, nl + 1);
        if (!line.isEmpty()) processLine(line);
    }
}

void OpenCodePage::onSseDone()
{
    if (!m_sse) return;
    const bool wasError = (m_sse->error() != QNetworkReply::NoError
                           && m_sse->error() != QNetworkReply::OperationCanceledError);
    m_sse->deleteLater();
    m_sse = nullptr;
    m_sseBuf.clear();
    finalizeBlock();

    if (wasError) {
        ++m_sseErrors;
        if (m_sseErrors <= 5)
            m_sseRetry->start(3000);
        else
            setStatus("SSE disconnessa — OpenCode non raggiungibile.", false);
    } else {
        m_sseErrors = 0;
        m_sseRetry->start(1000);    /* riconnetti dopo 1s (server ha chiuso) */
    }
}

void OpenCodePage::processLine(const QByteArray& line)
{
    if (!line.startsWith("data:")) return;
    const QByteArray data = line.mid(5).trimmed();
    if (data.isEmpty() || data == "[DONE]") return;
    const QJsonObject ev = QJsonDocument::fromJson(data).object();
    if (!ev.isEmpty()) handleEvent(ev);
}

void OpenCodePage::handleEvent(const QJsonObject& ev)
{
    const QString type = ev.value("type").toString();

    if (type == "message.updated") {
        const QJsonObject msg = ev.value("message").toObject();
        const QString id = msg.value("id").toString();
        if (m_aiMsgId.isEmpty()) m_aiMsgId = id;
        if (id != m_aiMsgId) return;

        const QString role = msg.value("role").toString();
        if (role != "assistant") return;

        const QJsonArray parts = msg.value("parts").toArray();
        for (const QJsonValue& pv : parts) {
            const QJsonObject part = pv.toObject();
            if (part.value("type").toString() != "text") continue;
            const QString text = part.value("text").toString();
            if (!text.isEmpty()) appendToken(text);
        }
    } else if (type == "session.idle") {
        finalizeBlock();
        m_aiMsgId.clear();
        m_sendBtn->setEnabled(true);
        m_abortBtn->setEnabled(false);
        fetchSessions();
    } else if (type == "session.error") {
        appendSystem(QString("<span style='color:#dc2626'>Errore: %1</span>")
                     .arg(ev.value("error").toString().toHtmlEscaped()));
        m_sendBtn->setEnabled(true);
        m_abortBtn->setEnabled(false);
    }
}

/* ══════════════════════════════════════════════════════════════
   Nuova sessione / Annulla / Seleziona sessione
   ══════════════════════════════════════════════════════════════ */
void OpenCodePage::onNewSessionClicked()
{
    disconnectSse();
    m_sessionId.clear();
    m_aiMsgId.clear();
    m_inBlock = false;
    m_log->clear();
    m_sseErrors = 0;
    createSession();
}

void OpenCodePage::onAbortClicked()
{
    if (m_sessionId.isEmpty()) return;
    QNetworkRequest req(QUrl(baseUrl() + "/session/" + m_sessionId + "/abort"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    auto* rep = m_nam->post(req, QByteArray("{}"));
    connect(rep, &QNetworkReply::finished, rep, &QNetworkReply::deleteLater);
    m_abortBtn->setEnabled(false);
    m_sendBtn->setEnabled(true);
}

void OpenCodePage::onSessionSelected(QListWidgetItem* item)
{
    if (!item) return;
    const QString id = item->data(Qt::UserRole).toString();
    if (id.isEmpty() || id == m_sessionId) return;
    disconnectSse();
    m_sessionId = id;
    m_aiMsgId.clear();
    m_inBlock = false;
    m_log->clear();
    m_sseErrors = 0;
    setStatus(QString("Sessione: %1").arg(m_sessionId.left(8) + "…"));
    appendSystem(QString("<span style='color:#2563eb'>Sessione cambiata: %1</span>")
                 .arg(m_sessionId.left(16)));
    connectSse();
}

void OpenCodePage::onRefreshSessions()
{
    fetchSessions();
}

/* ══════════════════════════════════════════════════════════════
   fetchSessions — GET /session
   ══════════════════════════════════════════════════════════════ */
void OpenCodePage::fetchSessions()
{
    if (m_fetchReply) return;
    m_fetchReply = m_nam->get(QNetworkRequest(QUrl(baseUrl() + "/session")));
    connect(m_fetchReply, &QNetworkReply::finished, this, &OpenCodePage::onFetchSessionsReply);
}

void OpenCodePage::onFetchSessionsReply()
{
    auto* reply = m_fetchReply;
    m_fetchReply = nullptr;
    if (!reply) return;
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) return;

    const QJsonArray arr = QJsonDocument::fromJson(reply->readAll()).array();
    m_sessList->clear();
    for (const QJsonValue& v : arr) {
        const QJsonObject s = v.toObject();
        const QString id    = s.value("id").toString();
        const QString title = s.value("title").toString();
        const QString label = title.isEmpty()
            ? id.left(16) + "…"
            : title.left(30) + (title.length() > 30 ? "…" : "");
        auto* item = new QListWidgetItem(label, m_sessList);
        item->setData(Qt::UserRole, id);
        if (id == m_sessionId)
            item->setSelected(true);
    }
}

/* ══════════════════════════════════════════════════════════════
   Helpers UI
   ══════════════════════════════════════════════════════════════ */
void OpenCodePage::appendToken(const QString& text)
{
    if (!m_inBlock) {
        m_inBlock = true;
        m_log->append("<div style='margin:4px 0; color:#1e293b;'>"
                      "<b>OpenCode:</b> <span id='aiblock'>");
    }
    QTextCursor cur = m_log->textCursor();
    cur.movePosition(QTextCursor::End);
    cur.insertText(text);
    m_log->verticalScrollBar()->setValue(m_log->verticalScrollBar()->maximum());
}

void OpenCodePage::finalizeBlock()
{
    if (!m_inBlock) return;
    m_inBlock = false;
    m_log->append("</span></div>");
    m_log->verticalScrollBar()->setValue(m_log->verticalScrollBar()->maximum());
}

void OpenCodePage::appendSystem(const QString& html)
{
    finalizeBlock();
    m_log->append("<div style='margin:2px 0; font-size:10pt;'>" + html + "</div>");
    m_log->verticalScrollBar()->setValue(m_log->verticalScrollBar()->maximum());
}

void OpenCodePage::setStatus(const QString& msg, bool ok)
{
    m_statusLbl->setText(msg);
    m_statusLbl->setStyleSheet(ok ? "color:#64748b; padding:4px 0;"
                                  : "color:#dc2626; padding:4px 0;");
}
