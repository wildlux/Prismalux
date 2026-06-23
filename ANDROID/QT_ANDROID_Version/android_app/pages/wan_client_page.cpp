#include "wan_client_page.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFont>
#include <QTextCursor>
#include <QScroller>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>
#include <QSettings>

static constexpr int  kWanPort     = 11600;
static constexpr char kNodeName[]  = "PrismaluxMobile";

WanClientPage::WanClientPage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(10, 8, 10, 8);
    vbox->setSpacing(8);

    /* Titolo */
    auto* title = new QLabel(
        QString::fromUtf8("\xf0\x9f\x8c\x90") + " WAN Compute Client", this);
    {
        QFont f = title->font();
        f.setPointSize(14);
        f.setBold(true);
        title->setFont(f);
    }
    vbox->addWidget(title);

    /* Connessione */
    auto* form = new QFormLayout;
    form->setSpacing(8);

    QSettings s("Prismalux", "Mobile");
    m_hostEdit = new QLineEdit(this);
    m_hostEdit->setPlaceholderText("192.168.1.X");
    m_hostEdit->setText(s.value("wan/host", "192.168.1.165").toString());
    m_hostEdit->setMinimumHeight(40);
    form->addRow("Server IP:", m_hostEdit);

    m_portSpin = new QSpinBox(this);
    m_portSpin->setRange(1024, 65535);
    m_portSpin->setValue(s.value("wan/port", kWanPort).toInt());
    m_portSpin->setMinimumHeight(40);
    form->addRow("Porta:", m_portSpin);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setText(s.value("wan/nodeName", kNodeName).toString());
    m_nameEdit->setMinimumHeight(40);
    form->addRow("Nome nodo:", m_nameEdit);

    vbox->addLayout(form);

    /* Bottoni Connetti/Disconnetti */
    auto* btnRow = new QHBoxLayout;
    m_connectBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x94\x8c") + " Connetti", this);
    m_connectBtn->setObjectName("PrimaryBtn");
    m_connectBtn->setMinimumHeight(44);
    connect(m_connectBtn, &QPushButton::clicked,
            this, &WanClientPage::onConnectClicked);
    btnRow->addWidget(m_connectBtn);

    m_disconnectBtn = new QPushButton(
        QString::fromUtf8("\xe2\x9b\x94\xef\xb8\x8f") + " Disconnetti", this);
    m_disconnectBtn->setObjectName("DangerBtn");
    m_disconnectBtn->setMinimumHeight(44);
    m_disconnectBtn->setEnabled(false);
    connect(m_disconnectBtn, &QPushButton::clicked,
            this, &WanClientPage::onDisconnectClicked);
    btnRow->addWidget(m_disconnectBtn);
    vbox->addLayout(btnRow);

    /* Status */
    m_statusLbl = new QLabel(
        QString::fromUtf8("\xe2\x9a\xab\xef\xb8\x8f") + " Non connesso", this);
    m_statusLbl->setObjectName("StatusLabel");
    vbox->addWidget(m_statusLbl);

    /* Task corrente */
    m_taskLbl = new QLabel(
        QString::fromUtf8("\xf0\x9f\x93\x8b") + " Task corrente:", this);
    {
        QFont f = m_taskLbl->font();
        f.setBold(true);
        m_taskLbl->setFont(f);
    }
    vbox->addWidget(m_taskLbl);

    m_taskOutput = new QTextEdit(this);
    m_taskOutput->setReadOnly(true);
    m_taskOutput->setObjectName("OutputEdit");
    m_taskOutput->setFixedHeight(100);
    m_taskOutput->setPlaceholderText("In attesa di task dal server...");
    {
        QFont f;
        f.setFamily("monospace");
        f.setPointSize(10);
        m_taskOutput->setFont(f);
    }
    vbox->addWidget(m_taskOutput);

    /* Log */
    auto* logLbl = new QLabel(
        QString::fromUtf8("\xf0\x9f\x93\x9c") + " Log:", this);
    {
        QFont f = logLbl->font();
        f.setBold(true);
        logLbl->setFont(f);
    }
    vbox->addWidget(logLbl);

    m_logEdit = new QTextEdit(this);
    m_logEdit->setReadOnly(true);
    m_logEdit->setObjectName("OutputEdit");
    {
        QFont f;
        f.setFamily("monospace");
        f.setPointSize(10);
        m_logEdit->setFont(f);
    }
    QScroller::grabGesture(m_logEdit->viewport(), QScroller::TouchGesture);
    vbox->addWidget(m_logEdit, 1);

    /* Socket */
    m_sock = new QTcpSocket(this);
    connect(m_sock, &QTcpSocket::connected,
            this, &WanClientPage::onSocketConnected);
    connect(m_sock, &QTcpSocket::disconnected,
            this, &WanClientPage::onSocketDisconnected);
    connect(m_sock, &QTcpSocket::readyRead,
            this, &WanClientPage::onSocketReadyRead);
    connect(m_sock, &QAbstractSocket::errorOccurred,
            this, &WanClientPage::onSocketError);

    /* AI signals */
    connect(m_ai, &AiClient::token,    this, &WanClientPage::onAiToken);
    connect(m_ai, &AiClient::finished, this, &WanClientPage::onAiFinished);
    connect(m_ai, &AiClient::error,    this, &WanClientPage::onAiError);
}

void WanClientPage::onConnectClicked()
{
    const QString host = m_hostEdit->text().trimmed();
    const int     port = m_portSpin->value();
    if (host.isEmpty()) return;

    QSettings s("Prismalux", "Mobile");
    s.setValue("wan/host",     host);
    s.setValue("wan/port",     port);
    s.setValue("wan/nodeName", m_nameEdit->text().trimmed());

    m_statusLbl->setText(
        QString::fromUtf8("\xf0\x9f\x94\x84") + " Connessione a " + host + "...");
    m_connectBtn->setEnabled(false);
    m_sock->connectToHost(host, static_cast<quint16>(port));
}

void WanClientPage::onDisconnectClicked()
{
    m_sock->disconnectFromHost();
}

void WanClientPage::onSocketConnected()
{
    setConnected(true);
    appendLog(QString::fromUtf8("\xe2\x9c\x85") + " Connesso al server WAN");
    /* Registra il nodo */
    sendJson(QJsonObject{
        {"t",    "hello"},
        {"name", m_nameEdit->text().trimmed()},
        {"caps", QJsonArray{"llm_prompt", "ai_chat", "echo"}}
    });
}

void WanClientPage::onSocketDisconnected()
{
    setConnected(false);
    if (!m_currentTaskId.isEmpty()) {
        m_ai->abort();
        m_currentTaskId.clear();
    }
    appendLog(QString::fromUtf8("\xe2\x9d\x8c") + " Disconnesso dal server");
}

void WanClientPage::onSocketError(QAbstractSocket::SocketError)
{
    setConnected(false);
    appendLog(
        QString::fromUtf8("\xe2\x9d\x8c") + " Errore: " + m_sock->errorString());
    m_statusLbl->setText(
        QString::fromUtf8("\xe2\x9d\x8c") + " " + m_sock->errorString());
}

void WanClientPage::onSocketReadyRead()
{
    m_readBuf += m_sock->readAll();
    while (true) {
        const int nl = m_readBuf.indexOf('\n');
        if (nl == -1) break;
        const QByteArray line = m_readBuf.left(nl).trimmed();
        m_readBuf = m_readBuf.mid(nl + 1);
        if (line.isEmpty()) continue;
        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(line, &err);
        if (doc.isNull() || !doc.isObject()) continue;
        handleMessage(doc.object());
    }
}

void WanClientPage::handleMessage(const QJsonObject& msg)
{
    const QString t = msg["t"].toString();

    if (t == "welcome") {
        const QString nodeId = msg["node_id"].toString();
        appendLog(QString::fromUtf8("\xf0\x9f\x86\x94") + " ID nodo: " + nodeId);
        m_statusLbl->setText(
            QString::fromUtf8("\xe2\x9c\x85") + " Connesso — idle, in attesa di task");
        /* Segnala disponibilità */
        sendJson(QJsonObject{{"t", "poll"}});

    } else if (t == "task") {
        const QString id      = msg["id"].toString();
        const QString kind    = msg["kind"].toString();
        const QString payload = msg["payload"].toString();
        appendLog(QString::fromUtf8("\xf0\x9f\x94\xa7") + " Task: " + id + " (" + kind + ")");
        startTask(id, kind, payload);

    } else if (t == "idle") {
        m_statusLbl->setText(
            QString::fromUtf8("\xe2\x9c\x85") + " Connesso — idle, in attesa di task");

    } else if (t == "ack") {
        appendLog(QString::fromUtf8("\xe2\x9c\x85") + " Ack ricevuto per " + msg["id"].toString());
        m_statusLbl->setText(
            QString::fromUtf8("\xe2\x9c\x85") + " Connesso — idle");
        sendJson(QJsonObject{{"t", "poll"}});
    }
}

void WanClientPage::startTask(const QString& id, const QString& kind, const QString& payload)
{
    m_currentTaskId = id;
    m_accum.clear();
    m_taskOutput->clear();
    m_statusLbl->setText(
        QString::fromUtf8("\xf0\x9f\x94\xa7") + " Eseguo: " + kind);

    if (kind == "echo") {
        finishTask(payload);
        return;
    }

    /* Interpreta il payload come JSON {sys, msg} o stringa semplice */
    QString sysPrompt = "Sei un assistente AI preciso.";
    QString userMsg   = payload;

    QJsonParseError e;
    const QJsonDocument doc = QJsonDocument::fromJson(payload.toUtf8(), &e);
    if (e.error == QJsonParseError::NoError && doc.isObject()) {
        const QJsonObject jo = doc.object();
        if (jo.contains("sys"))  sysPrompt = jo["sys"].toString();
        if (jo.contains("msg"))  userMsg   = jo["msg"].toString();
        if (jo.contains("prompt")) userMsg = jo["prompt"].toString();
    }

    m_taskOutput->setPlainText("Task: " + kind + "\nInput: " + userMsg.left(80) + "...\n\n");
    m_ai->chat(sysPrompt, userMsg);
}

void WanClientPage::finishTask(const QString& result)
{
    if (m_currentTaskId.isEmpty()) return;
    sendJson(QJsonObject{
        {"t",      "result"},
        {"id",     m_currentTaskId},
        {"result", result}
    });
    appendLog(QString::fromUtf8("\xf0\x9f\x9f\xa2") + " Risultato inviato per " + m_currentTaskId);
    m_currentTaskId.clear();
}

void WanClientPage::onAiToken(const QString& t)
{
    if (m_currentTaskId.isEmpty()) return;
    m_accum += t;
    m_taskOutput->moveCursor(QTextCursor::End);
    m_taskOutput->insertPlainText(t);
    m_taskOutput->moveCursor(QTextCursor::End);
}

void WanClientPage::onAiFinished(const QString& full)
{
    if (m_currentTaskId.isEmpty()) return;
    finishTask(full.isEmpty() ? m_accum : full);
}

void WanClientPage::onAiError(const QString& msg)
{
    if (m_currentTaskId.isEmpty()) return;
    finishTask("[ERROR] " + msg);
    appendLog(QString::fromUtf8("\xe2\x9d\x8c") + " Errore AI: " + msg);
}

void WanClientPage::sendJson(const QJsonObject& obj)
{
    if (m_sock && m_sock->state() == QAbstractSocket::ConnectedState)
        m_sock->write(QJsonDocument(obj).toJson(QJsonDocument::Compact) + "\n");
}

void WanClientPage::appendLog(const QString& msg)
{
    const QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");
    m_logEdit->append("[" + ts + "] " + msg);
}

void WanClientPage::setConnected(bool connected)
{
    m_connectBtn->setEnabled(!connected);
    m_disconnectBtn->setEnabled(connected);
    m_hostEdit->setEnabled(!connected);
    m_portSpin->setEnabled(!connected);
    m_nameEdit->setEnabled(!connected);
    if (!connected) {
        m_statusLbl->setText(
            QString::fromUtf8("\xe2\x9a\xab\xef\xb8\x8f") + " Non connesso");
        m_connectBtn->setEnabled(true);
    }
}
