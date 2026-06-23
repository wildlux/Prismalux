#include "appcontroller_page.h"
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
#include <QJsonArray>
#include <QDateTime>
#include <QUrl>
#include <QRegularExpression>

static constexpr const char* kAnkiSys =
    "Sei un esperto di didattica e memorizzazione. Genera flashcard Anki di qualita' "
    "per il concetto indicato. Rispondi con JSON:\n"
    "{\"cards\":[{\"front\":\"domanda breve\",\"back\":\"risposta completa con esempi\"},...]}";

static constexpr int kDefaultAnkiPort = 8765;
static constexpr int kDefaultMcpPort  = 7681; /* TinyMCP default WebSocket port */

/* ── Anki tab ───────────────────────────────────────────────── */
QWidget* AppControllerPage::buildAnkiTab()
{
    auto* w    = new QWidget;
    auto* vbox = new QVBoxLayout(w);
    vbox->setContentsMargins(8, 8, 8, 8);
    vbox->setSpacing(8);

    auto* note = new QLabel(
        QString::fromUtf8("\xf0\x9f\x83\x8f") +
        " Connetti ad Anki via AnkiConnect (porta 8765 sul PC).\n"
        "Oppure genera carte AI e visualizza localmente.", w);
    note->setWordWrap(true);
    note->setObjectName("StatusLabel");
    vbox->addWidget(note);

    auto* connRow = new QHBoxLayout;
    m_ankiHost = new QLineEdit(w);
    m_ankiHost->setPlaceholderText("192.168.1.xxx");
    m_ankiHost->setText("192.168.1.");
    connRow->addWidget(m_ankiHost, 1);
    m_ankiConnBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x94\x8c") + " Connetti", w);
    m_ankiConnBtn->setObjectName("PrimaryBtn");
    m_ankiConnBtn->setMinimumHeight(40);
    connRow->addWidget(m_ankiConnBtn);
    vbox->addLayout(connRow);

    /* Decks */
    auto* deckRow = new QHBoxLayout;
    m_ankiDecks = new QListWidget(w);
    m_ankiDecks->setMaximumHeight(100);
    m_ankiDecks->setObjectName("OutputEdit");
    QScroller::grabGesture(m_ankiDecks->viewport(), QScroller::TouchGesture);
    deckRow->addWidget(m_ankiDecks, 1);
    auto* deckBtnCol = new QVBoxLayout;
    auto* getDecksBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x93\x82"), w);
    getDecksBtn->setToolTip("Aggiorna lista deck");
    getDecksBtn->setMinimumSize(44, 40);
    deckBtnCol->addWidget(getDecksBtn);
    m_ankiRevBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x94\x84"), w);
    m_ankiRevBtn->setToolTip("Statistiche review");
    m_ankiRevBtn->setMinimumSize(44, 40);
    deckBtnCol->addWidget(m_ankiRevBtn);
    deckRow->addLayout(deckBtnCol);
    vbox->addLayout(deckRow);

    /* Aggiungi carta */
    auto* form = new QFormLayout;
    form->setSpacing(6);
    m_ankiDeck = new QLineEdit(w);
    m_ankiDeck->setPlaceholderText("Default");
    form->addRow("Deck:", m_ankiDeck);
    m_ankiFront = new QLineEdit(w);
    m_ankiFront->setPlaceholderText("Domanda / fronte carta");
    form->addRow("Fronte:", m_ankiFront);
    m_ankiBack = new QTextEdit(w);
    m_ankiBack->setFixedHeight(60);
    m_ankiBack->setPlaceholderText("Risposta / retro carta");
    m_ankiBack->setObjectName("InputEdit");
    form->addRow("Retro:", m_ankiBack);
    vbox->addLayout(form);

    auto* cardBtnRow = new QHBoxLayout;
    m_ankiAddBtn = new QPushButton(
        QString::fromUtf8("\xe2\x9e\x95") + " Aggiungi carta", w);
    m_ankiAddBtn->setObjectName("PrimaryBtn");
    m_ankiAddBtn->setMinimumHeight(40);
    cardBtnRow->addWidget(m_ankiAddBtn, 1);
    m_ankiGenBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\xa4\x96") + " Genera AI (5)", w);
    m_ankiGenBtn->setMinimumHeight(40);
    cardBtnRow->addWidget(m_ankiGenBtn, 1);
    m_ankiStopBtn = new QPushButton(
        QString::fromUtf8("\xe2\x8f\xb9"), w);
    m_ankiStopBtn->setObjectName("DangerBtn");
    m_ankiStopBtn->setMinimumHeight(40);
    m_ankiStopBtn->setVisible(false);
    cardBtnRow->addWidget(m_ankiStopBtn);
    vbox->addLayout(cardBtnRow);

    m_ankiLog = new QTextEdit(w);
    m_ankiLog->setReadOnly(true);
    m_ankiLog->setObjectName("OutputEdit");
    {   QFont f = m_ankiLog->font(); f.setPointSize(10); m_ankiLog->setFont(f); }
    QScroller::grabGesture(m_ankiLog->viewport(), QScroller::TouchGesture);
    vbox->addWidget(m_ankiLog, 1);

    connect(m_ankiConnBtn, &QPushButton::clicked, this, &AppControllerPage::onAnkiConnect);
    connect(getDecksBtn,   &QPushButton::clicked, this, &AppControllerPage::onAnkiGetDecks);
    connect(m_ankiAddBtn,  &QPushButton::clicked, this, &AppControllerPage::onAnkiAddCard);
    connect(m_ankiGenBtn,  &QPushButton::clicked, this, &AppControllerPage::onAnkiGenerate);
    connect(m_ankiRevBtn,  &QPushButton::clicked, this, &AppControllerPage::onAnkiReview);
    connect(m_ankiStopBtn, &QPushButton::clicked, m_ai, &AiClient::abort);
    return w;
}

/* ── TinyMCP tab ────────────────────────────────────────────── */
QWidget* AppControllerPage::buildMcpTab()
{
    auto* w    = new QWidget;
    auto* vbox = new QVBoxLayout(w);
    vbox->setContentsMargins(8, 8, 8, 8);
    vbox->setSpacing(8);

    auto* note = new QLabel(
        QString::fromUtf8("\xf0\x9f\x94\x8c") +
        " TinyMCP client — connetti al server MCP del PC desktop e\n"
        "esegui tool AI-assistiti via JSON-RPC.", w);
    note->setWordWrap(true);
    note->setObjectName("StatusLabel");
    vbox->addWidget(note);

    auto* form = new QFormLayout;
    form->setSpacing(6);
    m_mcpHost = new QLineEdit(w);
    m_mcpHost->setPlaceholderText("192.168.1.xxx");
    m_mcpHost->setText("192.168.1.");
    form->addRow("IP PC:", m_mcpHost);
    m_mcpPort = new QLineEdit(w);
    m_mcpPort->setText("11500");
    form->addRow("Porta:", m_mcpPort);
    vbox->addLayout(form);

    auto* connRow = new QHBoxLayout;
    m_mcpConnBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x94\x8c") + " Connetti", w);
    m_mcpConnBtn->setObjectName("PrimaryBtn");
    m_mcpConnBtn->setMinimumHeight(44);
    connRow->addWidget(m_mcpConnBtn, 1);
    auto* listBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x93\x8b") + " Lista tool", w);
    listBtn->setMinimumHeight(44);
    connRow->addWidget(listBtn, 1);
    vbox->addLayout(connRow);

    /* Lista tool */
    m_mcpTools = new QListWidget(w);
    m_mcpTools->setMaximumHeight(100);
    m_mcpTools->setObjectName("OutputEdit");
    QScroller::grabGesture(m_mcpTools->viewport(), QScroller::TouchGesture);
    vbox->addWidget(m_mcpTools);

    /* Esecuzione tool */
    auto* execForm = new QFormLayout;
    execForm->setSpacing(6);
    m_mcpToolName = new QLineEdit(w);
    m_mcpToolName->setPlaceholderText("nome_tool");
    execForm->addRow("Tool:", m_mcpToolName);
    m_mcpParams = new QTextEdit(w);
    m_mcpParams->setFixedHeight(60);
    m_mcpParams->setPlaceholderText("{\"param\": \"valore\"}");
    m_mcpParams->setObjectName("InputEdit");
    {   QFont f("Monospace", 11); m_mcpParams->setFont(f); }
    execForm->addRow("Params (JSON):", m_mcpParams);
    vbox->addLayout(execForm);

    m_mcpExecBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x9a\x80") + " Esegui tool", w);
    m_mcpExecBtn->setObjectName("PrimaryBtn");
    m_mcpExecBtn->setMinimumHeight(44);
    vbox->addWidget(m_mcpExecBtn);

    m_mcpLog = new QTextEdit(w);
    m_mcpLog->setReadOnly(true);
    m_mcpLog->setObjectName("OutputEdit");
    {   QFont f("Monospace", 10); m_mcpLog->setFont(f); }
    QScroller::grabGesture(m_mcpLog->viewport(), QScroller::TouchGesture);
    vbox->addWidget(m_mcpLog, 1);

    connect(m_mcpConnBtn, &QPushButton::clicked, this, &AppControllerPage::onMcpConnect);
    connect(listBtn,      &QPushButton::clicked, this, &AppControllerPage::onMcpListTools);
    connect(m_mcpExecBtn, &QPushButton::clicked, this, &AppControllerPage::onMcpExecute);
    connect(m_mcpTools, &QListWidget::itemClicked, this, [this](QListWidgetItem* it) {
        if (!it) return;
        const QString toolName = it->text().split(" ").first();
        if (m_mcpToolName) m_mcpToolName->setText(toolName);
    });
    return w;
}

/* ── Costruttore ────────────────────────────────────────────── */
AppControllerPage::AppControllerPage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(10, 8, 10, 8);
    vbox->setSpacing(8);

    auto* title = new QLabel(
        QString::fromUtf8("\xf0\x9f\x95\xb9") + " App Controller", this);
    {   QFont f = title->font(); f.setPointSize(14); f.setBold(true); title->setFont(f); }
    vbox->addWidget(title);

    m_modeCmb = new QComboBox(this);
    m_modeCmb->addItem(QString::fromUtf8("\xf0\x9f\x83\x8f") + " Anki FlashCard");
    m_modeCmb->addItem(QString::fromUtf8("\xf0\x9f\x94\x8c") + " TinyMCP Client");
    m_modeCmb->setMinimumHeight(44);
    vbox->addWidget(m_modeCmb);

    m_modeStack = new QStackedWidget(this);
    m_modeStack->addWidget(buildAnkiTab());
    m_modeStack->addWidget(buildMcpTab());
    vbox->addWidget(m_modeStack, 1);

    m_status = new QLabel("", this);
    m_status->setObjectName("StatusLabel");
    vbox->addWidget(m_status);

    connect(m_modeCmb, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &AppControllerPage::onModeChanged);
    connect(m_ai, &AiClient::token,    this, &AppControllerPage::onToken);
    connect(m_ai, &AiClient::finished, this, &AppControllerPage::onFinished);
    connect(m_ai, &AiClient::error,    this, &AppControllerPage::onAiError);
    connect(m_ai, &AiClient::aborted,  this, &AppControllerPage::onAborted);
}

/* ── Helpers ────────────────────────────────────────────────── */
void AppControllerPage::appendAnkiLog(const QString& msg, bool ok)
{
    const QString now   = QDateTime::currentDateTime().toString("hh:mm:ss");
    const QString color = ok ? "#22c55e" : "#ef4444";
    if (m_ankiLog)
        m_ankiLog->append(
            QString("<span style='color:%1'>[%2] %3</span>")
                .arg(color, now, msg.toHtmlEscaped()));
    m_status->setText(msg.left(80));
}

void AppControllerPage::appendMcpLog(const QString& msg, bool ok)
{
    const QString now   = QDateTime::currentDateTime().toString("hh:mm:ss");
    const QString color = ok ? "#22c55e" : "#ef4444";
    if (m_mcpLog)
        m_mcpLog->append(
            QString("<span style='color:%1'>[%2] %3</span>")
                .arg(color, now, msg.toHtmlEscaped()));
    m_status->setText(msg.left(80));
}

void AppControllerPage::ankiRequest(const QString& action, const QJsonObject& params)
{
    const QString ip = m_ankiHost ? m_ankiHost->text().trimmed() : "127.0.0.1";
    const QString url = QString("http://%1:%2").arg(ip).arg(kDefaultAnkiPort);
    QJsonObject body;
    body["action"]  = action;
    body["version"] = 6;
    if (!params.isEmpty()) body["params"] = params;

    auto* nam = new QNetworkAccessManager(this);
    QUrl ankiUrl(url);
    QNetworkRequest req(ankiUrl);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    auto* rep = nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(rep, &QNetworkReply::finished, this, &AppControllerPage::onAnkiNetReply);
    rep->setProperty("action", action);
    rep->setProperty("nam_ptr", QVariant::fromValue(static_cast<QObject*>(nam)));
}

void AppControllerPage::mcpRequest(const QString& endpoint, const QJsonObject& body)
{
    const QString ip   = m_mcpHost ? m_mcpHost->text().trimmed() : "127.0.0.1";
    const QString port = m_mcpPort ? m_mcpPort->text().trimmed()  : "11500";
    const QString url  = QString("http://%1:%2%3").arg(ip, port, endpoint);

    auto* nam = new QNetworkAccessManager(this);
    QUrl mcpUrl(url);
    QNetworkRequest req(mcpUrl);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    auto* rep = nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(rep, &QNetworkReply::finished, this, &AppControllerPage::onMcpNetReply);
    rep->setProperty("nam_ptr", QVariant::fromValue(static_cast<QObject*>(nam)));
}

/* ── Slots Anki ─────────────────────────────────────────────── */
void AppControllerPage::onAnkiConnect()
{
    ankiRequest("version");
    appendAnkiLog("Verifica connessione AnkiConnect...", true);
}

void AppControllerPage::onAnkiGetDecks()
{
    ankiRequest("deckNames");
}

void AppControllerPage::onAnkiAddCard()
{
    const QString front = m_ankiFront ? m_ankiFront->text().trimmed() : "";
    const QString back  = m_ankiBack  ? m_ankiBack->toPlainText().trimmed() : "";
    const QString deck  = (m_ankiDeck && !m_ankiDeck->text().trimmed().isEmpty())
                            ? m_ankiDeck->text().trimmed() : "Default";
    if (front.isEmpty() || back.isEmpty()) {
        appendAnkiLog("Compila fronte e retro della carta.", false);
        return;
    }
    QJsonObject note;
    note["deckName"]  = deck;
    note["modelName"] = "Basic";
    QJsonObject fields;
    fields["Front"] = front;
    fields["Back"]  = back;
    note["fields"]  = fields;
    QJsonArray options;
    QJsonObject dup; dup["allowDuplicate"] = false;
    note["options"] = dup;
    QJsonArray tags; note["tags"] = tags;
    QJsonObject params; params["note"] = note;
    ankiRequest("addNote", params);
    appendAnkiLog("Aggiunta carta: " + front, true);
}

void AppControllerPage::onAnkiGenerate()
{
    const QString topic = m_ankiFront ? m_ankiFront->text().trimmed() : "";
    if (topic.isEmpty()) {
        appendAnkiLog("Scrivi il tema nel campo Fronte per generare carte AI.", false);
        return;
    }
    m_aiMode = true;
    setAiBusy(true);
    appendAnkiLog("Generazione 5 carte AI su: " + topic, true);
    m_ai->chat(kAnkiSys,
        "Genera 5 flashcard Anki sul tema: " + topic);
}

void AppControllerPage::onAnkiReview()
{
    QJsonObject params;
    params["days"] = 1;
    ankiRequest("getNumCardsReviewedByDay");
    ankiRequest("getCollectionStatsHTML");
}

void AppControllerPage::onAnkiNetReply()
{
    auto* rep = qobject_cast<QNetworkReply*>(sender());
    if (!rep) return;
    const QString action = rep->property("action").toString();
    auto* nam = qobject_cast<QNetworkAccessManager*>(
        rep->property("nam_ptr").value<QObject*>());

    if (rep->error() != QNetworkReply::NoError) {
        appendAnkiLog("Errore AnkiConnect: " + rep->errorString(), false);
    } else {
        const QByteArray raw = rep->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(raw);
        if (action == "deckNames") {
            if (m_ankiDecks) {
                m_ankiDecks->clear();
                const QJsonArray arr = doc.object().value("result").toArray();
                for (const QJsonValue& v : arr)
                    m_ankiDecks->addItem(v.toString());
                appendAnkiLog(
                    QString("%1 deck trovati.").arg(arr.size()), true);
            }
        } else if (action == "addNote") {
            const bool ok = !doc.object().value("error").toString().isEmpty() == false;
            appendAnkiLog(ok ? "Carta aggiunta con successo."
                             : "Errore: " + doc.object().value("error").toString(), ok);
        } else if (action == "version") {
            const int ver = doc.object().value("result").toInt();
            appendAnkiLog(
                ver > 0 ? QString("AnkiConnect v%1 — connesso!").arg(ver)
                        : "AnkiConnect non risponde.", ver > 0);
        } else {
            appendAnkiLog(QString(raw.left(160)), true);
        }
    }
    rep->deleteLater();
    if (nam) nam->deleteLater();
}

/* ── Slots TinyMCP ──────────────────────────────────────────── */
void AppControllerPage::onMcpConnect()
{
    mcpRequest("/api/tags", {});
    appendMcpLog("Test connessione al PC Prismalux...", true);
}

void AppControllerPage::onMcpListTools()
{
    /* Lista tool via endpoint /api/tools del server LAN */
    const QString ip   = m_mcpHost ? m_mcpHost->text().trimmed() : "127.0.0.1";
    const QString port = m_mcpPort ? m_mcpPort->text().trimmed()  : "11500";
    const QString url  = QString("http://%1:%2/api/tags").arg(ip, port);

    auto* nam = new QNetworkAccessManager(this);
    auto* rep = nam->get(QNetworkRequest(QUrl(url)));
    connect(rep, &QNetworkReply::finished, this, [this, rep, nam] {
        if (rep->error() == QNetworkReply::NoError) {
            const QByteArray raw = rep->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(raw);
            const QJsonArray models = doc.object().value("models").toArray();
            if (m_mcpTools) {
                m_mcpTools->clear();
                /* Mostra i modelli come "tool" disponibili */
                for (const QJsonValue& v : models) {
                    const QString name = v.toObject().value("name").toString();
                    const qint64 sz    = v.toObject().value("size").toInteger();
                    m_mcpTools->addItem(name + QString("  [%1 MB]").arg(sz / 1048576));
                }
                if (models.isEmpty())
                    m_mcpTools->addItem("chat · repl · rag · knowledge");
                appendMcpLog(
                    QString("PC raggiunto: %1 modelli disponibili.").arg(models.size()), true);
            }
        } else {
            appendMcpLog("Errore: " + rep->errorString(), false);
        }
        rep->deleteLater(); nam->deleteLater();
    });
}

void AppControllerPage::onMcpExecute()
{
    const QString tool    = m_mcpToolName ? m_mcpToolName->text().trimmed() : "";
    const QString paramsStr = m_mcpParams ? m_mcpParams->toPlainText().trimmed() : "{}";
    if (tool.isEmpty()) {
        appendMcpLog("Specifica il nome del tool.", false);
        return;
    }
    QJsonObject params;
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(paramsStr.toUtf8(), &err);
    if (err.error == QJsonParseError::NoError && doc.isObject())
        params = doc.object();

    /* Mappa i tool noti agli endpoint del LAN server */
    QString endpoint;
    QJsonObject body;
    if (tool == "chat" || tool == "ai") {
        endpoint = "/api/chat";
        body["model"]   = "";
        body["message"] = params.value("message").toString("ciao");
        body["stream"]  = false;
    } else if (tool == "repl") {
        endpoint = "/api/repl";
        body["code"] = params.value("code").toString("print('hello')");
        body["lang"] = "python";
    } else if (tool == "rag") {
        endpoint = "/api/rag";
        body["action"] = "rebuild";
    } else if (tool == "knowledge") {
        endpoint = "/knowledge";
    } else {
        /* Fallback: prova come tool arbitrario via /api/tool */
        endpoint = "/api/tool";
        body["name"]   = tool;
        body["params"] = params;
    }
    mcpRequest(endpoint, body);
    appendMcpLog("Esecuzione tool: " + tool, true);
}

void AppControllerPage::onMcpNetReply()
{
    auto* rep = qobject_cast<QNetworkReply*>(sender());
    if (!rep) return;
    auto* nam = qobject_cast<QNetworkAccessManager*>(
        rep->property("nam_ptr").value<QObject*>());

    if (rep->error() != QNetworkReply::NoError) {
        appendMcpLog("Errore: " + rep->errorString(), false);
    } else {
        const QString resp = rep->readAll().trimmed().left(400);
        appendMcpLog("Risposta: " + resp, true);
    }
    rep->deleteLater();
    if (nam) nam->deleteLater();
}

/* ── Slots AI ───────────────────────────────────────────────── */
void AppControllerPage::onToken(const QString& t)
{
    if (!m_aiMode || !m_ankiLog) return;
    m_ankiLog->moveCursor(QTextCursor::End);
    m_ankiLog->insertPlainText(t);
    m_ankiLog->moveCursor(QTextCursor::End);
}

void AppControllerPage::onFinished(const QString& full)
{
    if (!m_aiMode) return;
    setAiBusy(false);
    m_aiMode = false;

    /* Parsa le carte generate */
    QRegularExpression re("\\{[\\s\\S]*\\}",
        QRegularExpression::NoPatternOption);
    auto match = re.match(full);
    if (!match.hasMatch()) {
        appendAnkiLog("Nessun JSON valido nella risposta AI.", false);
        return;
    }
    QJsonDocument doc = QJsonDocument::fromJson(match.captured(0).toUtf8());
    const QJsonArray cards = doc.object().value("cards").toArray();
    if (cards.isEmpty()) {
        appendAnkiLog("Nessuna carta trovata nel JSON.", false);
        return;
    }
    const QString deck = (m_ankiDeck && !m_ankiDeck->text().trimmed().isEmpty())
                           ? m_ankiDeck->text().trimmed() : "Default";
    int sent = 0;
    for (const QJsonValue& v : cards) {
        const QString f = v.toObject().value("front").toString();
        const QString b = v.toObject().value("back").toString();
        if (f.isEmpty() || b.isEmpty()) continue;
        QJsonObject note, fields, dup;
        note["deckName"] = deck;
        note["modelName"] = "Basic";
        fields["Front"] = f; fields["Back"] = b;
        note["fields"] = fields;
        dup["allowDuplicate"] = false;
        note["options"] = dup;
        note["tags"] = QJsonArray();
        QJsonObject params; params["note"] = note;
        ankiRequest("addNote", params);
        ++sent;
    }
    appendAnkiLog(
        QString("%1 carte generate e inviate ad Anki.").arg(sent), sent > 0);
}

void AppControllerPage::onAiError(const QString& msg)
{
    if (!m_aiMode) return;
    setAiBusy(false);
    m_aiMode = false;
    appendAnkiLog(QString::fromUtf8("\xe2\x9d\x8c ") + msg, false);
}

void AppControllerPage::onAborted()
{
    if (!m_aiMode) return;
    setAiBusy(false);
    m_aiMode = false;
    appendAnkiLog("Interrotto.", false);
}

void AppControllerPage::setAiBusy(bool busy)
{
    if (m_ankiGenBtn) m_ankiGenBtn->setEnabled(!busy);
    if (m_ankiAddBtn) m_ankiAddBtn->setEnabled(!busy);
    if (m_ankiStopBtn) m_ankiStopBtn->setVisible(busy);
}

void AppControllerPage::onModeChanged(int idx)
{
    m_modeStack->setCurrentIndex(idx);
}
