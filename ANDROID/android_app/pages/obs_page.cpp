#include "obs_page.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QFont>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QListWidgetItem>
#include <QSettings>
#include <QTimer>

#ifdef HAVE_OBS_WS
#include <QWebSocket>
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#endif

/* ══════════════════════════════════════════════════════════════
   Costruttore
   ══════════════════════════════════════════════════════════════ */
ObsPage::ObsPage(QWidget* parent)
    : QWidget(parent)
{
    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(8, 8, 8, 8);
    vbox->setSpacing(8);

    auto* title = new QLabel(QString::fromUtf8("\xf0\x9f\x93\xa1 OBS Studio Control"), this);
    QFont tf = title->font();
    tf.setPointSize(14);
    tf.setBold(true);
    title->setFont(tf);
    title->setAlignment(Qt::AlignCenter);
    vbox->addWidget(title);

#ifndef HAVE_OBS_WS
    auto* warn = new QLabel(
        QString::fromUtf8("\xe2\x9a\xa0\xef\xb8\x8f")
        + " Qt6::WebSockets non disponibile in questa build.\n"
          "Ricompila con il modulo WebSockets per abilitare OBS control.", this);
    warn->setWordWrap(true);
    warn->setAlignment(Qt::AlignCenter);
    vbox->addWidget(warn);
    vbox->addStretch();
    return;
#else
    /* ── Impostazioni connessione ── */
    QSettings s("Prismalux", "Mobile");
    auto* connGroup = new QGroupBox("Connessione OBS", this);
    auto* form = new QFormLayout(connGroup);
    form->setSpacing(6);

    m_hostEdit = new QLineEdit(s.value("obs/host", "192.168.1.100").toString(), this);
    m_hostEdit->setPlaceholderText("192.168.1.100");
    form->addRow("Host:", m_hostEdit);

    m_portEdit = new QLineEdit(s.value("obs/port", "4455").toString(), this);
    m_portEdit->setPlaceholderText("4455");
    form->addRow("Porta:", m_portEdit);

    m_pwdEdit = new QLineEdit(s.value("obs/pwd", "").toString(), this);
    m_pwdEdit->setEchoMode(QLineEdit::Password);
    m_pwdEdit->setPlaceholderText("Password OBS (opzionale)");
    form->addRow("Password:", m_pwdEdit);

    auto* btnRow = new QHBoxLayout;
    m_btnConnect = new QPushButton(QString::fromUtf8("\xf0\x9f\x94\x8c Connetti"), this);
    m_btnConnect->setMinimumHeight(40);
    m_btnDisconn = new QPushButton(QString::fromUtf8("\xf0\x9f\x94\x8c\xe2\x83\xb3 Disconnetti"), this);
    m_btnDisconn->setMinimumHeight(40);
    m_btnDisconn->setEnabled(false);
    btnRow->addWidget(m_btnConnect);
    btnRow->addWidget(m_btnDisconn);
    form->addRow(btnRow);
    vbox->addWidget(connGroup);

    /* ── Status ── */
    m_statusLbl = new QLabel(QString::fromUtf8("\xe2\x8f\xba") + " Non connesso", this);
    m_statusLbl->setAlignment(Qt::AlignCenter);
    vbox->addWidget(m_statusLbl);

    /* ── Scena corrente ── */
    m_curSceneLbl = new QLabel("Scena attiva: —", this);
    m_curSceneLbl->setAlignment(Qt::AlignCenter);
    QFont sf = m_curSceneLbl->font();
    sf.setBold(true);
    m_curSceneLbl->setFont(sf);
    vbox->addWidget(m_curSceneLbl);

    /* ── Lista scene ── */
    auto* sceneGroup = new QGroupBox(
        QString::fromUtf8("\xf0\x9f\x8e\xac")
        + "  Scene (tap per attivare)", this);
    auto* sceneVbox = new QVBoxLayout(sceneGroup);
    m_sceneList = new QListWidget(this);
    m_sceneList->setEnabled(false);
    m_sceneList->setMinimumHeight(120);
    sceneVbox->addWidget(m_sceneList);

    m_btnRefresh = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x94\x84 Aggiorna scene"), this);
    m_btnRefresh->setEnabled(false);
    m_btnRefresh->setMinimumHeight(40);
    sceneVbox->addWidget(m_btnRefresh);
    vbox->addWidget(sceneGroup);

    /* ── Lista microfoni ── */
    auto* micGroup = new QGroupBox(
        QString::fromUtf8("\xf0\x9f\x8e\x99")
        + "  Microfoni (tap per silenziare/attivare)", this);
    auto* micVbox = new QVBoxLayout(micGroup);
    m_micList = new QListWidget(this);
    m_micList->setEnabled(false);
    m_micList->setMinimumHeight(100);
    micVbox->addWidget(m_micList);

    m_btnRefreshMic = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x94\x84 Aggiorna microfoni"), this);
    m_btnRefreshMic->setEnabled(false);
    m_btnRefreshMic->setMinimumHeight(40);
    micVbox->addWidget(m_btnRefreshMic);
    vbox->addWidget(micGroup);

    /* ── Controlli rec/stream ── */
    auto* ctrlRow = new QHBoxLayout;
    m_btnRecord = new QPushButton(
        QString::fromUtf8("\xe2\x8f\xba Avvia REC"), this);
    m_btnRecord->setEnabled(false);
    m_btnRecord->setMinimumHeight(44);
    m_btnStream = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x93\xa1 Avvia STREAM"), this);
    m_btnStream->setEnabled(false);
    m_btnStream->setMinimumHeight(44);
    ctrlRow->addWidget(m_btnRecord);
    ctrlRow->addWidget(m_btnStream);
    vbox->addLayout(ctrlRow);

    /* ── WebSocket ── */
    m_ws = new QWebSocket("PrismaluxMobile", QWebSocketProtocol::VersionLatest, this);
    connect(m_ws, &QWebSocket::connected,    this, &ObsPage::onWsConnected);
    connect(m_ws, &QWebSocket::disconnected, this, &ObsPage::onWsDisconnected);
    connect(m_ws, &QWebSocket::textMessageReceived, this, &ObsPage::onWsMessage);
    connect(m_ws, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
            this, &ObsPage::onWsError);

    /* ── Segnali UI ── */
    connect(m_btnConnect,    &QPushButton::clicked, this, &ObsPage::onConnectClicked);
    connect(m_btnDisconn,    &QPushButton::clicked, this, &ObsPage::onDisconnectClicked);
    connect(m_btnRefresh,    &QPushButton::clicked, this, &ObsPage::onRefreshClicked);
    connect(m_btnRefreshMic, &QPushButton::clicked, this, &ObsPage::requestInputList);
    connect(m_btnRecord,     &QPushButton::clicked, this, &ObsPage::onRecordClicked);
    connect(m_btnStream,     &QPushButton::clicked, this, &ObsPage::onStreamClicked);
    connect(m_sceneList, &QListWidget::itemClicked,
            this, &ObsPage::onSceneItemClicked);
    connect(m_micList, &QListWidget::itemClicked,
            this, &ObsPage::onMicItemClicked);
#endif
}

ObsPage::~ObsPage()
{
#ifdef HAVE_OBS_WS
    if (m_ws) m_ws->close();
#endif
}

/* ══════════════════════════════════════════════════════════════
   Slot UI
   ══════════════════════════════════════════════════════════════ */
void ObsPage::onConnectClicked()
{
#ifdef HAVE_OBS_WS
    const QString host = m_hostEdit->text().trimmed();
    const int     port = m_portEdit->text().trimmed().toInt();
    m_password = m_pwdEdit->text();

    if (host.isEmpty() || port <= 0) {
        m_statusLbl->setText(
            QString::fromUtf8("\xe2\x9a\xa0\xef\xb8\x8f") + " Host/porta non validi");
        return;
    }
    QSettings s("Prismalux", "Mobile");
    s.setValue("obs/host", host);
    s.setValue("obs/port", port);
    s.setValue("obs/pwd", m_password);

    m_statusLbl->setText(QString::fromUtf8("\xe2\x8f\xb3") + " Connessione...");
    m_btnConnect->setEnabled(false);
    const QUrl url(QString("ws://%1:%2").arg(host).arg(port));
    m_ws->open(url);
#endif
}

void ObsPage::onDisconnectClicked()
{
#ifdef HAVE_OBS_WS
    m_ws->close();
#endif
}

void ObsPage::onRefreshClicked()
{
#ifdef HAVE_OBS_WS
    requestSceneList();
#endif
}

void ObsPage::onRecordClicked()
{
#ifdef HAVE_OBS_WS
    const QString requestType = m_recording ? "StopRecord" : "StartRecord";
    QJsonObject req;
    req["op"] = 6;
    QJsonObject d;
    d["requestType"] = requestType;
    d["requestId"]   = QString::number(m_msgId++);
    d["requestData"] = QJsonObject{};
    req["d"] = d;
    sendJson(req);
#endif
}

void ObsPage::onStreamClicked()
{
#ifdef HAVE_OBS_WS
    const QString requestType = m_streaming ? "StopStream" : "StartStream";
    QJsonObject req;
    req["op"] = 6;
    QJsonObject d;
    d["requestType"] = requestType;
    d["requestId"]   = QString::number(m_msgId++);
    d["requestData"] = QJsonObject{};
    req["d"] = d;
    sendJson(req);
#endif
}

void ObsPage::onSceneItemClicked(QListWidgetItem* item)
{
#ifdef HAVE_OBS_WS
    if (!item) return;
    const QString sceneName = item->text();
    QJsonObject req;
    req["op"] = 6;
    QJsonObject d;
    d["requestType"] = "SetCurrentProgramScene";
    d["requestId"]   = QString::number(m_msgId++);
    QJsonObject rd;
    rd["sceneName"] = sceneName;
    d["requestData"] = rd;
    req["d"] = d;
    sendJson(req);
    m_curSceneLbl->setText("Scena attiva: " + sceneName);
#else
    Q_UNUSED(item)
#endif
}

void ObsPage::onMicItemClicked(QListWidgetItem* item)
{
#ifdef HAVE_OBS_WS
    if (!item || !m_connected) return;
    /* Il testo dell'item ha il formato "🔊 Nome microfono" o "🔇 Nome microfono" */
    QString text = item->text();
    const bool wasMuted = text.startsWith(QString::fromUtf8("\xf0\x9f\x94\x87"));
    /* Estrae il nome rimuovendo l'icona e lo spazio */
    const QString inputName = text.mid(text.indexOf(' ') + 1);

    const bool newMuted = !wasMuted;
    m_inputMuted[inputName] = newMuted;

    /* Aggiorna visivamente l'icona subito */
    item->setText(
        (newMuted
            ? QString::fromUtf8("\xf0\x9f\x94\x87")   /* 🔇 */
            : QString::fromUtf8("\xf0\x9f\x94\x8a"))  /* 🔊 */
        + " " + inputName);

    /* Invia a OBS */
    QJsonObject req;
    req["op"] = 6;
    QJsonObject d;
    d["requestType"] = "SetInputMute";
    d["requestId"]   = QString::number(m_msgId++);
    QJsonObject rd;
    rd["inputName"] = inputName;
    rd["inputMuted"] = newMuted;
    d["requestData"] = rd;
    req["d"] = d;
    sendJson(req);
#else
    Q_UNUSED(item)
#endif
}

/* ══════════════════════════════════════════════════════════════
   Slot WebSocket
   ══════════════════════════════════════════════════════════════ */
void ObsPage::onWsConnected()
{
#ifdef HAVE_OBS_WS
    /* Il server manda Hello (op=0) in automatico — gestiamo in onWsMessage */
    m_statusLbl->setText(
        QString::fromUtf8("\xf0\x9f\x94\x97") + " Connesso — attesa Hello...");
#endif
}

void ObsPage::onWsDisconnected()
{
#ifdef HAVE_OBS_WS
    setConnected(false);
    m_statusLbl->setText(
        QString::fromUtf8("\xe2\x8f\xba") + " Disconnesso");
    m_btnConnect->setEnabled(true);
#endif
}

void ObsPage::onWsMessage(const QString& msg)
{
#ifdef HAVE_OBS_WS
    const QJsonObject obj = QJsonDocument::fromJson(msg.toUtf8()).object();
    handleMessage(obj);
#else
    Q_UNUSED(msg)
#endif
}

void ObsPage::onWsError(QAbstractSocket::SocketError)
{
#ifdef HAVE_OBS_WS
    const QString errStr = m_ws->errorString();
    m_statusLbl->setText(
        QString::fromUtf8("\xe2\x9d\x8c") + " Errore: " + errStr);
    m_btnConnect->setEnabled(true);
    setConnected(false);
#endif
}

/* ══════════════════════════════════════════════════════════════
   Logica protocollo obs-websocket v5
   ══════════════════════════════════════════════════════════════ */
void ObsPage::sendJson(const QJsonObject& obj)
{
#ifdef HAVE_OBS_WS
    m_ws->sendTextMessage(QJsonDocument(obj).toJson(QJsonDocument::Compact));
#else
    Q_UNUSED(obj)
#endif
}

void ObsPage::handleMessage(const QJsonObject& obj)
{
#ifdef HAVE_OBS_WS
    const int op = obj["op"].toInt(-1);

    if (op == 0) {
        /* Hello — contiene eventuale auth */
        const QJsonObject d = obj["d"].toObject();
        handleIdentify(d);
        return;
    }
    if (op == 2) {
        /* Identified — connessione riuscita */
        setConnected(true);
        requestSceneList();
        requestInputList();
        requestStatus();
        return;
    }
    if (op == 7) {
        /* RequestResponse */
        const QJsonObject d    = obj["d"].toObject();
        const QString     type = d["requestType"].toString();
        const QJsonObject data = d["responseData"].toObject();

        if (type == "GetSceneList") {
            handleSceneList(data);
        } else if (type == "GetInputList") {
            handleInputList(data);
        } else if (type == "GetStreamStatus") {
            m_streaming = data["outputActive"].toBool();
            m_btnStream->setText(
                m_streaming
                ? QString::fromUtf8("\xf0\x9f\x93\xa1 Ferma STREAM")
                : QString::fromUtf8("\xf0\x9f\x93\xa1 Avvia STREAM"));
        } else if (type == "GetRecordStatus") {
            m_recording = data["outputActive"].toBool();
            m_btnRecord->setText(
                m_recording
                ? QString::fromUtf8("\xe2\x8f\xb9 Ferma REC")
                : QString::fromUtf8("\xe2\x8f\xba Avvia REC"));
        } else if (type == "StartRecord" || type == "StopRecord") {
            /* Verifica lo stato reale dopo 300ms invece di aggiornare ottimisticamente */
            QTimer::singleShot(300, this, &ObsPage::requestStatus);
        } else if (type == "StartStream" || type == "StopStream") {
            QTimer::singleShot(300, this, &ObsPage::requestStatus);
        }
        return;
    }
    if (op == 5) {
        /* Event */
        const QJsonObject d    = obj["d"].toObject();
        const QString     type = d["eventType"].toString();
        const QJsonObject data = d["eventData"].toObject();
        if (type == "CurrentProgramSceneChanged") {
            m_curSceneLbl->setText(
                "Scena attiva: " + data["sceneName"].toString());
        }
    }
#else
    Q_UNUSED(obj)
#endif
}

void ObsPage::handleIdentify(const QJsonObject& d)
{
#ifdef HAVE_OBS_WS
    /* Costruisce il messaggio Identify (op=1) con auth opzionale */
    QJsonObject identify;
    identify["op"] = 1;
    QJsonObject id;
    id["rpcVersion"] = 1;

    if (d.contains("authentication") && !m_password.isEmpty()) {
        const QJsonObject auth = d["authentication"].toObject();
        const QString challenge = auth["challenge"].toString();
        const QString salt      = auth["salt"].toString();

        /* secret = base64(SHA256(password + salt)) */
        const QByteArray secretRaw = QCryptographicHash::hash(
            (m_password + salt).toUtf8(), QCryptographicHash::Sha256);
        const QString secret = secretRaw.toBase64();

        /* token = base64(SHA256(secret + challenge)) */
        const QByteArray tokenRaw = QCryptographicHash::hash(
            (secret + challenge).toUtf8(), QCryptographicHash::Sha256);
        id["authentication"] = QString::fromLatin1(tokenRaw.toBase64());
    }
    identify["d"] = id;
    sendJson(identify);
#else
    Q_UNUSED(d)
#endif
}

void ObsPage::requestSceneList()
{
#ifdef HAVE_OBS_WS
    QJsonObject req;
    req["op"] = 6;
    QJsonObject d;
    d["requestType"] = "GetSceneList";
    d["requestId"]   = QString::number(m_msgId++);
    d["requestData"] = QJsonObject{};
    req["d"] = d;
    sendJson(req);
#endif
}

void ObsPage::requestInputList()
{
#ifdef HAVE_OBS_WS
    if (!m_connected) return;
    QJsonObject req;
    req["op"] = 6;
    QJsonObject d;
    d["requestType"] = "GetInputList";
    d["requestId"]   = QString::number(m_msgId++);
    d["requestData"] = QJsonObject{};
    req["d"] = d;
    sendJson(req);
#endif
}

void ObsPage::requestStatus()
{
#ifdef HAVE_OBS_WS
    auto send = [this](const QString& t) {
        QJsonObject req;
        req["op"] = 6;
        QJsonObject d;
        d["requestType"] = t;
        d["requestId"]   = QString::number(m_msgId++);
        d["requestData"] = QJsonObject{};
        req["d"] = d;
        sendJson(req);
    };
    send("GetStreamStatus");
    send("GetRecordStatus");
#endif
}

void ObsPage::handleSceneList(const QJsonObject& data)
{
#ifdef HAVE_OBS_WS
    m_sceneList->clear();
    const QJsonArray scenes = data["scenes"].toArray();
    for (auto it = scenes.constEnd(); it != scenes.constBegin();) {
        --it;
        const QString name = (*it).toObject()["sceneName"].toString();
        if (!name.isEmpty())
            m_sceneList->addItem(name);
    }
    const QString cur = data["currentProgramSceneName"].toString();
    if (!cur.isEmpty())
        m_curSceneLbl->setText("Scena attiva: " + cur);
#else
    Q_UNUSED(data)
#endif
}

void ObsPage::handleInputList(const QJsonObject& data)
{
#ifdef HAVE_OBS_WS
    m_micList->clear();
    m_inputMuted.clear();
    const QJsonArray inputs = data["inputs"].toArray();
    for (const QJsonValue& v : inputs) {
        const QJsonObject inp   = v.toObject();
        const QString     name  = inp["inputName"].toString();
        const QString     kind  = inp["inputKind"].toString();
        /* Filtra solo sorgenti audio (microfoni, cattura desktop, ecc.) */
        if (!kind.contains("input") && !kind.contains("audio") &&
            !kind.contains("alsa")  && !kind.contains("pulse") &&
            !kind.contains("wasapi")&& !kind.contains("coreaudio") &&
            !kind.contains("capture"))
            continue;
        if (name.isEmpty()) continue;
        m_inputMuted[name] = false;  /* stato iniziale: non silenziato */
        auto* item = new QListWidgetItem(
            QString::fromUtf8("\xf0\x9f\x94\x8a") + " " + name,  /* 🔊 */
            m_micList);
        item->setSizeHint(QSize(0, 48));
    }
    if (m_micList->count() == 0) {
        m_micList->addItem("(nessuna sorgente audio trovata)");
    }
#else
    Q_UNUSED(data)
#endif
}

void ObsPage::setConnected(bool on)
{
    m_connected = on;
#ifdef HAVE_OBS_WS
    m_btnConnect->setEnabled(!on);
    m_btnDisconn->setEnabled(on);
    m_btnRecord->setEnabled(on);
    m_btnStream->setEnabled(on);
    m_btnRefresh->setEnabled(on);
    m_btnRefreshMic->setEnabled(on);
    m_sceneList->setEnabled(on);
    m_micList->setEnabled(on);
    if (on) {
        m_statusLbl->setText(
            QString::fromUtf8("\xf0\x9f\x9f\xa2") + " Connesso a OBS");
    } else {
        m_sceneList->clear();
        m_micList->clear();
        m_inputMuted.clear();
        m_curSceneLbl->setText("Scena attiva: —");
        m_recording = false;
        m_streaming = false;
        m_btnRecord->setText(QString::fromUtf8("\xe2\x8f\xba Avvia REC"));
        m_btnStream->setText(QString::fromUtf8("\xf0\x9f\x93\xa1 Avvia STREAM"));
    }
#endif
}
