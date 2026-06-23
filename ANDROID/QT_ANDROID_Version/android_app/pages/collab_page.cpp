#include "collab_page.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFont>
#include <QTextCursor>
#include <QScroller>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

static constexpr const char* kBtSys =
    "Sei un assistente AI in una chat collaborativa BT. Simula una conversazione di gruppo "
    "costruttiva e informativa. Rispondi come se fossi un collega nel gruppo.";

/* ── BT Chat tab ────────────────────────────────────────────── */
QWidget* CollabPage::buildBtTab()
{
    auto* w    = new QWidget;
    auto* vbox = new QVBoxLayout(w);
    vbox->setContentsMargins(8, 8, 8, 8);
    vbox->setSpacing(8);

    auto* note = new QLabel(
        QString::fromUtf8("\xf0\x9f\x93\xa1") +
        " Chat AI condivisa — simula comunicazione di gruppo via Bluetooth.", w);
    note->setWordWrap(true);
    note->setObjectName("StatusLabel");
    vbox->addWidget(note);

    m_btChat = new QTextEdit(w);
    m_btChat->setReadOnly(true);
    m_btChat->setObjectName("OutputEdit");
    m_btChat->setPlaceholderText("I messaggi della chat appariranno qui...");
    QScroller::grabGesture(m_btChat->viewport(), QScroller::TouchGesture);
    vbox->addWidget(m_btChat, 1);

    auto* inputRow = new QHBoxLayout;
    m_btInput = new QLineEdit(w);
    m_btInput->setPlaceholderText("Scrivi messaggio...");
    m_btInput->setMinimumHeight(40);
    inputRow->addWidget(m_btInput, 1);

    m_btSendBtn = new QPushButton(
        QString::fromUtf8("\xe2\x9e\xa4"), w);
    m_btSendBtn->setObjectName("PrimaryBtn");
    m_btSendBtn->setMinimumSize(44, 44);
    inputRow->addWidget(m_btSendBtn);

    m_btStopBtn = new QPushButton(
        QString::fromUtf8("\xe2\x8f\xb9"), w);
    m_btStopBtn->setObjectName("DangerBtn");
    m_btStopBtn->setMinimumSize(44, 44);
    m_btStopBtn->setVisible(false);
    inputRow->addWidget(m_btStopBtn);
    vbox->addLayout(inputRow);
    return w;
}

/* ── PC Hub tab ─────────────────────────────────────────────── */
QWidget* CollabPage::buildPcHubTab()
{
    auto* w    = new QWidget;
    auto* vbox = new QVBoxLayout(w);
    vbox->setContentsMargins(8, 8, 8, 8);
    vbox->setSpacing(8);

    auto* note = new QLabel(
        QString::fromUtf8("\xf0\x9f\x96\xa5") +
        " Connetti al PC desktop Prismalux via LAN e invia comandi.", w);
    note->setWordWrap(true);
    note->setObjectName("StatusLabel");
    vbox->addWidget(note);

    auto* form = new QFormLayout;
    form->setSpacing(6);

    m_pcIp = new QLineEdit(w);
    m_pcIp->setPlaceholderText("192.168.1.xxx");
    m_pcIp->setText("192.168.1.");
    form->addRow("IP PC:", m_pcIp);

    m_pcPort = new QLineEdit(w);
    m_pcPort->setText("11500");
    m_pcPort->setPlaceholderText("11500");
    form->addRow("Porta:", m_pcPort);
    vbox->addLayout(form);

    m_pcConnBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x94\x8c") + " Testa connessione", w);
    m_pcConnBtn->setObjectName("PrimaryBtn");
    m_pcConnBtn->setMinimumHeight(44);
    vbox->addWidget(m_pcConnBtn);

    auto* actForm = new QFormLayout;
    actForm->setSpacing(6);

    m_pcActionCmb = new QComboBox(w);
    m_pcActionCmb->addItem("Chat AI",                "chat");
    m_pcActionCmb->addItem("Esegui REPL Python",     "repl");
    m_pcActionCmb->addItem("Ottieni modelli",        "models");
    m_pcActionCmb->addItem("Rebuild RAG",            "rag_rebuild");
    actForm->addRow("Azione:", m_pcActionCmb);

    m_pcPayload = new QLineEdit(w);
    m_pcPayload->setPlaceholderText("Prompt o codice...");
    actForm->addRow("Payload:", m_pcPayload);
    vbox->addLayout(actForm);

    m_pcSendBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x9a\x80") + " Invia al PC", w);
    m_pcSendBtn->setObjectName("PrimaryBtn");
    m_pcSendBtn->setMinimumHeight(44);
    vbox->addWidget(m_pcSendBtn);

    m_pcLog = new QTextEdit(w);
    m_pcLog->setReadOnly(true);
    m_pcLog->setObjectName("OutputEdit");
    {   QFont f("Monospace", 10); m_pcLog->setFont(f); }
    QScroller::grabGesture(m_pcLog->viewport(), QScroller::TouchGesture);
    vbox->addWidget(m_pcLog, 1);
    return w;
}

/* ── Costruttore ────────────────────────────────────────────── */
CollabPage::CollabPage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(10, 8, 10, 8);
    vbox->setSpacing(8);

    auto* title = new QLabel(
        QString::fromUtf8("\xf0\x9f\x94\x97") + " Collab — BT + PC Hub", this);
    {   QFont f = title->font(); f.setPointSize(14); f.setBold(true); title->setFont(f); }
    vbox->addWidget(title);

    m_modeCmb = new QComboBox(this);
    m_modeCmb->addItem(QString::fromUtf8("\xf0\x9f\x93\xa1") + " Chat BT (AI)");
    m_modeCmb->addItem(QString::fromUtf8("\xf0\x9f\x96\xa5") + " PC Hub (LAN)");
    m_modeCmb->setMinimumHeight(44);
    vbox->addWidget(m_modeCmb);

    m_modeStack = new QStackedWidget(this);
    m_modeStack->addWidget(buildBtTab());
    m_modeStack->addWidget(buildPcHubTab());
    vbox->addWidget(m_modeStack, 1);

    m_status = new QLabel("", this);
    m_status->setObjectName("StatusLabel");
    vbox->addWidget(m_status);

    connect(m_modeCmb, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &CollabPage::onModeChanged);
    connect(m_btSendBtn,  &QPushButton::clicked, this, &CollabPage::onSendChatClicked);
    connect(m_btStopBtn,  &QPushButton::clicked, m_ai, &AiClient::abort);
    connect(m_pcConnBtn,  &QPushButton::clicked, this, &CollabPage::onConnectPcClicked);
    connect(m_pcSendBtn,  &QPushButton::clicked, this, &CollabPage::onSendToPcClicked);
    connect(m_ai, &AiClient::token,    this, &CollabPage::onToken);
    connect(m_ai, &AiClient::finished, this, &CollabPage::onFinished);
    connect(m_ai, &AiClient::error,    this, &CollabPage::onAiError);
}

/* ── Slots ──────────────────────────────────────────────────── */
void CollabPage::onModeChanged(int idx)
{
    m_modeStack->setCurrentIndex(idx);
}

void CollabPage::onSendChatClicked()
{
    const QString msg = m_btInput ? m_btInput->text().trimmed() : QString{};
    if (msg.isEmpty()) return;
    appendChat("Tu", msg);
    if (m_btInput) m_btInput->clear();
    setBusy(true);
    m_ai->chat(kBtSys, msg);
}

void CollabPage::onConnectPcClicked()
{
    const QString url = QString("http://%1:%2/api/tags")
        .arg(m_pcIp ? m_pcIp->text() : "")
        .arg(m_pcPort ? m_pcPort->text() : "11500");
    auto* nam  = new QNetworkAccessManager(this);
    auto* rep  = nam->get(QNetworkRequest(QUrl(url)));
    m_status->setText(
        QString::fromUtf8("\xe2\x8f\xb3") + " Test connessione...");
    connect(rep, &QNetworkReply::finished, this, [this, rep, nam] {
        if (rep->error() == QNetworkReply::NoError) {
            m_status->setText(
                QString::fromUtf8("\xe2\x9c\x85") + " Connesso al PC Prismalux.");
            if (m_pcLog) m_pcLog->append(
                QString::fromUtf8("\xe2\x9c\x85") + " OK — " +
                rep->readAll().left(80));
        } else {
            m_status->setText(
                QString::fromUtf8("\xe2\x9d\x8c") + " " + rep->errorString());
        }
        rep->deleteLater();
        nam->deleteLater();
    });
}

void CollabPage::onSendToPcClicked()
{
    const QString ip   = m_pcIp   ? m_pcIp->text().trimmed()   : "";
    const QString port = m_pcPort ? m_pcPort->text().trimmed()  : "11500";
    const QString act  = m_pcActionCmb ? m_pcActionCmb->currentData().toString() : "chat";
    const QString pay  = m_pcPayload   ? m_pcPayload->text().trimmed() : "";

    QString endpoint;
    QJsonObject body;

    if (act == "chat") {
        endpoint = "/api/chat";
        body["model"]   = "";
        body["message"] = pay;
        body["stream"]  = false;
    } else if (act == "repl") {
        endpoint = "/api/repl";
        body["code"] = pay;
        body["lang"] = "python";
    } else if (act == "models") {
        endpoint = "/api/tags";
    } else if (act == "rag_rebuild") {
        endpoint = "/api/rag";
        body["action"] = "rebuild";
    }

    const QString url = QString("http://%1:%2%3").arg(ip, port, endpoint);
    auto* nam = new QNetworkAccessManager(this);
    QUrl qurl(url);
    QNetworkRequest req(qurl);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    auto* rep = nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));

    m_status->setText(QString::fromUtf8("\xe2\x8f\xb3") + " Invio a " + url);
    connect(rep, &QNetworkReply::finished, this, [this, rep, nam, act] {
        if (rep->error() == QNetworkReply::NoError) {
            const QString resp = rep->readAll().left(300);
            if (m_pcLog) m_pcLog->append(
                QString::fromUtf8("\xe2\x9c\x85 [%1] %2").arg(act, resp));
            m_status->setText(QString::fromUtf8("\xe2\x9c\x85") + " Risposta ricevuta.");
        } else {
            m_status->setText(
                QString::fromUtf8("\xe2\x9d\x8c ") + rep->errorString());
        }
        rep->deleteLater(); nam->deleteLater();
    });
}

void CollabPage::onPcActionClicked() { onSendToPcClicked(); }

void CollabPage::appendChat(const QString& from, const QString& msg)
{
    if (!m_btChat) return;
    const bool isUser = (from == "Tu");
    const QString color = isUser ? "#6c63ff" : "#22c55e";
    m_btChat->append(
        QString("<b style='color:%1'>%2:</b> %3")
            .arg(color, from, msg.toHtmlEscaped()));
}

void CollabPage::onToken(const QString& t)
{
    if (m_modeCmb && m_modeCmb->currentIndex() == 0) {
        m_btChat->moveCursor(QTextCursor::End);
        m_btChat->insertPlainText(t);
        m_btChat->moveCursor(QTextCursor::End);
    }
}

void CollabPage::onFinished(const QString& full)
{
    setBusy(false);
    if (m_modeCmb && m_modeCmb->currentIndex() == 0)
        appendChat("AI", full);
}

void CollabPage::onAiError(const QString& msg)
{
    setBusy(false);
    m_status->setText(QString::fromUtf8("\xe2\x9d\x8c ") + msg);
}

void CollabPage::setBusy(bool busy)
{
    if (m_btSendBtn) m_btSendBtn->setEnabled(!busy);
    if (m_btStopBtn) m_btStopBtn->setVisible(busy);
    if (busy) m_status->setText(
        QString::fromUtf8("\xe2\x8f\xb3") + " AI sta rispondendo...");
}
