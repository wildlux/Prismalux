#include "ble_page.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QClipboard>
#include <QGuiApplication>
#include <QMessageBox>
#include <QBluetoothLocalDevice>
#include <QBluetoothUuid>
#include <QDateTime>
#include <QFont>

/* UUID custom per il servizio chat Prismalux */
static const QBluetoothUuid kChatUuid(
    QString("00001101-0000-1000-8000-00805F9B34FB"));  /* SPP */

/* ══════════════════════════════════════════════════════════════
   Costruttore
   ══════════════════════════════════════════════════════════════ */
BlePage::BlePage(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("BlePage");

    m_stack = new QStackedWidget(this);
    auto* outerVbox = new QVBoxLayout(this);
    outerVbox->setContentsMargins(0, 0, 0, 0);
    outerVbox->addWidget(m_stack);

    /* ═══════════════════════════════════
       PAGINA 0 — Scanner BLE
       ═══════════════════════════════════ */
    auto* scanPage = new QWidget;
    auto* scanVbox = new QVBoxLayout(scanPage);
    scanVbox->setContentsMargins(8, 8, 8, 8);
    scanVbox->setSpacing(8);

    /* Tab bar */
    auto* tabRow = new QHBoxLayout;
    auto* scanTabBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x94\x8d") + " Scanner", scanPage);  /* 🔍 */
    scanTabBtn->setObjectName("PrimaryBtn");
    scanTabBtn->setEnabled(false);
    m_chatTabBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x92\xac") + " Chat BT", scanPage);  /* 💬 */
    m_chatTabBtn->setObjectName("SecondaryBtn");
    tabRow->addWidget(scanTabBtn, 1);
    tabRow->addWidget(m_chatTabBtn, 1);
    scanVbox->addLayout(tabRow);

    /* Header */
    m_statusLbl = new QLabel("Premi Scansiona per cercare dispositivi", scanPage);
    m_statusLbl->setObjectName("StatusLabel");
    m_statusLbl->setWordWrap(true);
    m_countLbl = new QLabel("", scanPage);
    m_countLbl->setObjectName("CountLabel");
    auto* headerRow = new QHBoxLayout;
    headerRow->addWidget(m_statusLbl, 1);
    headerRow->addWidget(m_countLbl);
    scanVbox->addLayout(headerRow);

    m_list = new QListWidget(scanPage);
    m_list->setObjectName("BleList");
    m_list->setAlternatingRowColors(true);
    scanVbox->addWidget(m_list, 1);

    m_scanBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x94\x8b") + "  Scansiona BLE", scanPage);
    m_scanBtn->setObjectName("PrimaryBtn");
    scanVbox->addWidget(m_scanBtn);

    m_stack->addWidget(scanPage);   /* indice 0 */

    /* ═══════════════════════════════════
       PAGINA 1 — Chat Bluetooth
       ═══════════════════════════════════ */
    auto* chatPage = new QWidget;
    auto* chatVbox = new QVBoxLayout(chatPage);
    chatVbox->setContentsMargins(8, 8, 8, 8);
    chatVbox->setSpacing(8);

    /* Tab bar */
    auto* tabRow2 = new QHBoxLayout;
    m_scanTabBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x94\x8d") + " Scanner", chatPage);
    m_scanTabBtn->setObjectName("SecondaryBtn");
    auto* chatTabBtn2 = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x92\xac") + " Chat BT", chatPage);
    chatTabBtn2->setObjectName("PrimaryBtn");
    chatTabBtn2->setEnabled(false);
    tabRow2->addWidget(m_scanTabBtn, 1);
    tabRow2->addWidget(chatTabBtn2, 1);
    chatVbox->addLayout(tabRow2);

    /* Status connessione */
    m_chatStatus = new QLabel(
        QString::fromUtf8("\xf0\x9f\x94\x8c")  /* 🔌 */
        + " Non connesso — premi Connetti oppure Ascolta",
        chatPage);
    m_chatStatus->setWordWrap(true);
    chatVbox->addWidget(m_chatStatus);

    /* Log chat */
    m_chatLog = new QTextEdit(chatPage);
    m_chatLog->setReadOnly(true);
    m_chatLog->setPlaceholderText(
        "La chat apparirà qui.\n\n"
        "Premi Ascolta per attendere connessioni in entrata,\n"
        "oppure Connetti per connetterti a un dispositivo dalla lista scanner.");
    m_chatLog->setMinimumHeight(200);
    chatVbox->addWidget(m_chatLog, 1);

    /* Riga connetti/ascolta */
    auto* connRow = new QHBoxLayout;
    m_chatConn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x93\xb6") + " Ascolta", chatPage);  /* 📶 */
    m_chatConn->setObjectName("PrimaryBtn");
    m_chatConn->setMinimumHeight(44);
    auto* infoLbl = new QLabel(
        "<small>Stessa rete BT richiesta.\nEntrambi i telefoni devono essere abbinati.</small>",
        chatPage);
    infoLbl->setTextFormat(Qt::RichText);
    infoLbl->setWordWrap(true);
    connRow->addWidget(m_chatConn, 1);
    chatVbox->addLayout(connRow);
    chatVbox->addWidget(infoLbl);

    /* Input messaggio */
    auto* inputRow = new QHBoxLayout;
    m_chatInput = new QLineEdit(chatPage);
    m_chatInput->setPlaceholderText("Scrivi un messaggio in chiaro...");
    m_chatInput->setObjectName("ChatInput");
    m_chatSend = new QPushButton(
        QString::fromUtf8("\xe2\x9e\xa4"), chatPage);  /* ➤ */
    m_chatSend->setObjectName("SendBtn");
    m_chatSend->setFixedWidth(52);
    m_chatSend->setEnabled(false);
    inputRow->addWidget(m_chatInput, 1);
    inputRow->addWidget(m_chatSend);
    chatVbox->addLayout(inputRow);

    m_stack->addWidget(chatPage);   /* indice 1 */

    /* ── BLE Discovery Agent ── */
    m_agent = new QBluetoothDeviceDiscoveryAgent(this);
    m_agent->setLowEnergyDiscoveryTimeout(8000);

    /* ── Connessioni ── */
    connect(m_scanBtn,    &QPushButton::clicked, this, &BlePage::onStartScan);
    connect(m_chatTabBtn, &QPushButton::clicked, this, &BlePage::onShowChat);
    connect(m_scanTabBtn, &QPushButton::clicked, this, &BlePage::onShowScan);
    connect(m_chatConn,   &QPushButton::clicked, this, &BlePage::onChatConnectClicked);
    connect(m_chatSend,   &QPushButton::clicked, this, &BlePage::onChatSend);
    connect(m_chatInput,  &QLineEdit::returnPressed, this, &BlePage::onChatSend);
    connect(m_list,       &QListWidget::itemClicked, this, &BlePage::onDeviceTapped);

    connect(m_agent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
            this, &BlePage::onDeviceDiscovered);
    connect(m_agent, &QBluetoothDeviceDiscoveryAgent::finished,
            this, &BlePage::onScanFinished);
    connect(m_agent,
            QOverload<QBluetoothDeviceDiscoveryAgent::Error>::of(
                &QBluetoothDeviceDiscoveryAgent::errorOccurred),
            this, &BlePage::onScanError);
}

BlePage::~BlePage()
{
    stopScan();
    if (m_socket) { m_socket->disconnectFromService(); }
    if (m_btServer) { m_btServer->close(); }
}

/* ── Navigazione tab ─────────────────────────────────────────── */
void BlePage::onShowScan() { m_stack->setCurrentIndex(0); }
void BlePage::onShowChat() { m_stack->setCurrentIndex(1); }

/* ── stopScan ────────────────────────────────────────────────── */
void BlePage::stopScan()
{
    if (m_agent && m_agent->isActive()) {
        m_agent->stop();
        m_scanBtn->setText(QString::fromUtf8("\xf0\x9f\x94\x8b") + "  Scansiona BLE");
        m_scanBtn->setEnabled(true);
    }
}

/* ── onStartScan ─────────────────────────────────────────────── */
void BlePage::onStartScan()
{
    if (m_agent->isActive()) { stopScan(); return; }
    m_list->clear();
    m_deviceIndex.clear();
    m_statusLbl->setText(QString::fromUtf8("\xf0\x9f\x94\x8d") + "  Ricerca in corso\xe2\x80\xa6");
    m_countLbl->setText("");
    m_scanBtn->setText(QString::fromUtf8("\xe2\x9c\x95") + "  Ferma");
    m_agent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
}

/* ── rssiIcon ────────────────────────────────────────────────── */
QString BlePage::rssiIcon(int rssi) const
{
    if (rssi >= -60) return QString::fromUtf8("\xf0\x9f\x9f\xa2");
    if (rssi >= -80) return QString::fromUtf8("\xf0\x9f\x9f\xa1");
    return             QString::fromUtf8("\xf0\x9f\x94\xb4");
}

/* ── addOrUpdateDevice ───────────────────────────────────────── */
void BlePage::addOrUpdateDevice(const QBluetoothDeviceInfo& info)
{
    const QString mac  = info.address().toString();
    const QString name = info.name().isEmpty() ? "(senza nome)" : info.name();
    const int     rssi = info.rssi();
    const QString text = QString("%1  %2\n    %3  %4 dBm")
                         .arg(rssiIcon(rssi), name, mac, QString::number(rssi));
    auto it = m_deviceIndex.find(mac);
    if (it != m_deviceIndex.end()) {
        if (auto* item = m_list->item(it.value()))
            item->setText(text);
    } else {
        auto* item = new QListWidgetItem(text, m_list);
        item->setData(Qt::UserRole,     mac);
        item->setData(Qt::UserRole + 1, name);
        m_deviceIndex[mac] = m_list->count() - 1;
    }
}

void BlePage::onDeviceDiscovered(const QBluetoothDeviceInfo& info)
{
    addOrUpdateDevice(info);
    m_countLbl->setText(QString("%1 trovati").arg(m_list->count()));
}

void BlePage::onScanFinished()
{
    m_statusLbl->setText(
        m_list->count() == 0
            ? QString::fromUtf8("\xe2\x9a\xa0") + "  Nessun dispositivo trovato"
            : QString::fromUtf8("\xe2\x9c\x85") + QString("  Scansione completata — %1 dispositivi").arg(m_list->count()));
    m_scanBtn->setText(QString::fromUtf8("\xf0\x9f\x94\x8b") + "  Scansiona BLE");
    m_scanBtn->setEnabled(true);
}

void BlePage::onScanError(QBluetoothDeviceDiscoveryAgent::Error)
{
    m_statusLbl->setText(
        QString::fromUtf8("\xe2\x9d\x8c") + "  Errore BLE: " + m_agent->errorString() +
        "\n" + QString::fromUtf8("\xf0\x9f\x92\xa1") + "  Verifica che il Bluetooth sia attivo.");
    m_scanBtn->setText(QString::fromUtf8("\xf0\x9f\x94\x8b") + "  Scansiona BLE");
    m_scanBtn->setEnabled(true);
}

/* ── onDeviceTapped — mostra dettagli e copia MAC ───────────── */
void BlePage::onDeviceTapped(QListWidgetItem* item)
{
    if (!item) return;
    const QString mac  = item->data(Qt::UserRole).toString();
    const QString name = item->data(Qt::UserRole + 1).toString();
    QGuiApplication::clipboard()->setText(mac);

    QMessageBox::information(this, "Dispositivo BLE",
        QString("Nome: %1\nMAC: %2\n\n"
                "(Indirizzo copiato negli appunti)\n\n"
                "Vai in Chat BT e premi Ascolta/Connetti\n"
                "per aprire una sessione di chat Bluetooth Classic.")
        .arg(name, mac));
}

/* ══════════════════════════════════════════════════════════════
   Chat Bluetooth Classic (RFCOMM / SPP)
   ══════════════════════════════════════════════════════════════ */

void BlePage::appendChatMsg(const QString& sender, const QString& text)
{
    const QString ts = QDateTime::currentDateTime().toString("HH:mm");
    m_chatLog->append(
        QString("<b>[%1] %2:</b> %3").arg(ts, sender, text.toHtmlEscaped()));
    m_chatLog->verticalScrollBar()->setValue(
        m_chatLog->verticalScrollBar()->maximum());
}

void BlePage::startServer()
{
    if (m_btServer) { m_btServer->close(); delete m_btServer; m_btServer = nullptr; }
    m_btServer = new QBluetoothServer(QBluetoothServiceInfo::RfcommProtocol, this);
    connect(m_btServer, &QBluetoothServer::newConnection,
            this, &BlePage::onNewConnection);
    if (!m_btServer->listen(QBluetoothAddress(), 0)) {
        m_chatStatus->setText(
            QString::fromUtf8("\xe2\x9d\x8c") + " Impossibile avviare il server BT.");
        return;
    }
    m_btServer->setSecurityFlags(QBluetooth::NoSecurity);   /* in chiaro */
    m_chatStatus->setText(
        QString::fromUtf8("\xf0\x9f\x93\xb6")  /* 📶 */
        + " In attesa di connessioni BT...\n"
        + "Porta RFCOMM: " + QString::number(m_btServer->serverPort()));
    m_isServer = true;
}

/* ── onChatConnectClicked — alterna tra ascolta e disconnetti ── */
void BlePage::onChatConnectClicked()
{
    if (m_socket && m_socket->state() == QBluetoothSocket::ConnectedState) {
        m_socket->disconnectFromService();
        return;
    }
    if (m_btServer && m_btServer->isListening()) {
        m_btServer->close();
        m_chatStatus->setText(
            QString::fromUtf8("\xf0\x9f\x94\x8c")  /* 🔌 */
            + " Server fermato.");
        m_chatConn->setText(QString::fromUtf8("\xf0\x9f\x93\xb6") + " Ascolta");
        return;
    }
    /* Avvia il server RFCOMM in ascolto */
    m_chatConn->setText(QString::fromUtf8("\xe2\x9c\x95") + " Ferma");
    startServer();
}

void BlePage::onNewConnection()
{
    if (!m_btServer) return;
    m_socket = m_btServer->nextPendingConnection();
    if (!m_socket) return;

    connect(m_socket, &QBluetoothSocket::readyRead,
            this, &BlePage::onSocketReadyRead);
    connect(m_socket, &QBluetoothSocket::disconnected,
            this, &BlePage::onSocketDisconnected);
    connect(m_socket,
            QOverload<QBluetoothSocket::SocketError>::of(&QBluetoothSocket::errorOccurred),
            this, &BlePage::onSocketError);

    m_chatStatus->setText(
        QString::fromUtf8("\xf0\x9f\x9f\xa2")  /* 🟢 */
        + " Connesso a: " + m_socket->peerName()
        + " (" + m_socket->peerAddress().toString() + ")");
    m_chatSend->setEnabled(true);
    m_chatConn->setText(QString::fromUtf8("\xe2\x9d\x8c") + " Disconnetti");

    appendChatMsg("Sistema",
        QString::fromUtf8("\xf0\x9f\x9f\xa2") + " "
        + m_socket->peerName() + " si è connesso.");
}

void BlePage::onSocketReadyRead()
{
    if (!m_socket) return;
    while (m_socket->canReadLine()) {
        const QString line = QString::fromUtf8(m_socket->readLine()).trimmed();
        if (!line.isEmpty()) {
            const QString peer = m_socket->isValid()
                ? m_socket->peerName()
                : "Remoto";
            appendChatMsg(peer, line);
        }
    }
}

void BlePage::onSocketDisconnected()
{
    appendChatMsg("Sistema",
        QString::fromUtf8("\xf0\x9f\x94\xb4") + " Connessione chiusa.");
    m_chatStatus->setText(
        QString::fromUtf8("\xf0\x9f\x94\x8c")  /* 🔌 */
        + " Disconnesso.");
    m_chatSend->setEnabled(false);
    m_chatConn->setText(QString::fromUtf8("\xf0\x9f\x93\xb6") + " Ascolta");
    if (m_socket) { m_socket->deleteLater(); m_socket = nullptr; }
}

void BlePage::onSocketError(QBluetoothSocket::SocketError)
{
    m_chatStatus->setText(
        QString::fromUtf8("\xe2\x9d\x8c") + " Errore BT: "
        + (m_socket ? m_socket->errorString() : "socket non disponibile"));
}

/* ── onChatSend — invia messaggio al socket ─────────────────── */
void BlePage::onChatSend()
{
    const QString msg = m_chatInput->text().trimmed();
    if (msg.isEmpty() || !m_socket
        || m_socket->state() != QBluetoothSocket::ConnectedState) return;

    m_socket->write((msg + "\n").toUtf8());
    appendChatMsg("Tu", msg);
    m_chatInput->clear();
}
