#include "ble_page.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QClipboard>
#include <QGuiApplication>
#include <QMessageBox>
#include <QBluetoothLocalDevice>
#include <QBluetoothUuid>
#include <QBluetoothAddress>
#include <QDateTime>
#include <QFont>
#include <QScrollBar>
#include <QInputDialog>

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
    m_peerTabBtnFromScan = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x94\x97") + " Connetti", scanPage);  /* 🔗 */
    m_peerTabBtnFromScan->setObjectName("SecondaryBtn");
    tabRow->addWidget(scanTabBtn, 1);
    tabRow->addWidget(m_chatTabBtn, 1);
    tabRow->addWidget(m_peerTabBtnFromScan, 1);
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
    m_peerTabBtnFromChat = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x94\x97") + " Connetti", chatPage);  /* 🔗 */
    m_peerTabBtnFromChat->setObjectName("SecondaryBtn");
    tabRow2->addWidget(m_scanTabBtn, 1);
    tabRow2->addWidget(chatTabBtn2, 1);
    tabRow2->addWidget(m_peerTabBtnFromChat, 1);
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

    /* Indicatore cifratura + pulsante chiave */
    auto* cryptoRow = new QHBoxLayout;
    m_cryptoIndicator = new QLabel(
        QString::fromUtf8("\xf0\x9f\x94\x92")  /* 🔒 */
        + " <small>AES-256-GCM attivo</small>",
        chatPage);
    m_cryptoIndicator->setTextFormat(Qt::RichText);
    m_keyInfoBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x94\x91")  /* 🔑 */
        + " Chiave",
        chatPage);
    m_keyInfoBtn->setObjectName("SecondaryBtn");
    m_keyInfoBtn->setMaximumWidth(100);
    m_keyInfoBtn->setToolTip("Mostra / reimposta chiave di cifratura BLE");
    cryptoRow->addWidget(m_cryptoIndicator, 1);
    cryptoRow->addWidget(m_keyInfoBtn);
    chatVbox->addLayout(cryptoRow);

    /* Input messaggio */
    auto* inputRow = new QHBoxLayout;
    m_chatInput = new QLineEdit(chatPage);
    m_chatInput->setPlaceholderText("Scrivi un messaggio (cifrato automaticamente)...");
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

    /* ═══════════════════════════════════
       PAGINA 2 — Peer BT Classic (client)
       ═══════════════════════════════════ */
    auto* peerPage = new QWidget;
    auto* peerVbox = new QVBoxLayout(peerPage);
    peerVbox->setContentsMargins(8, 8, 8, 8);
    peerVbox->setSpacing(8);

    /* Tab bar */
    auto* tabRow3 = new QHBoxLayout;
    m_scanTabBtnFromPeer = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x94\x8d") + " Scanner", peerPage);
    m_scanTabBtnFromPeer->setObjectName("SecondaryBtn");
    m_chatTabBtnFromPeer = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x92\xac") + " Chat BT", peerPage);
    m_chatTabBtnFromPeer->setObjectName("SecondaryBtn");
    auto* peerTabBtn3 = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x94\x97") + " Connetti", peerPage);
    peerTabBtn3->setObjectName("PrimaryBtn");
    peerTabBtn3->setEnabled(false);
    tabRow3->addWidget(m_scanTabBtnFromPeer, 1);
    tabRow3->addWidget(m_chatTabBtnFromPeer, 1);
    tabRow3->addWidget(peerTabBtn3, 1);
    peerVbox->addLayout(tabRow3);

    /* Status */
    m_peerStatus = new QLabel(
        QString::fromUtf8("\xf0\x9f\x94\x8c")  /* 🔌 */
        + " Cerca dispositivi o seleziona un accoppiato",
        peerPage);
    m_peerStatus->setWordWrap(true);
    peerVbox->addWidget(m_peerStatus);

    /* Pulsante scopri */
    m_discoverBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x94\x8d") + " Cerca dispositivi BT Classic", peerPage);
    m_discoverBtn->setObjectName("PrimaryBtn");
    m_discoverBtn->setMinimumHeight(44);
    peerVbox->addWidget(m_discoverBtn);

    /* Lista peer */
    m_peerList = new QListWidget(peerPage);
    m_peerList->setObjectName("BleList");
    m_peerList->setAlternatingRowColors(true);
    auto* peerListHint = new QLabel(
        QString::fromUtf8("<small style='color:#8890a8'>"
        "\xf0\x9f\x91\x86 Doppio tap su un dispositivo per connettersi</small>"),
        peerPage);
    peerListHint->setTextFormat(Qt::RichText);
    peerVbox->addWidget(m_peerList, 1);
    peerVbox->addWidget(peerListHint);

    /* Chat con il peer connesso */
    m_peerChatLog = new QTextEdit(peerPage);
    m_peerChatLog->setReadOnly(true);
    m_peerChatLog->setPlaceholderText("La chat con il dispositivo connesso apparirà qui.");
    m_peerChatLog->setMinimumHeight(120);
    m_peerChatLog->setVisible(false);
    peerVbox->addWidget(m_peerChatLog);

    auto* peerInputRow = new QHBoxLayout;
    m_peerChatInput = new QLineEdit(peerPage);
    m_peerChatInput->setPlaceholderText("Scrivi un messaggio (cifrato automaticamente)...");
    m_peerChatInput->setObjectName("ChatInput");
    m_peerChatInput->setVisible(false);
    m_peerChatSend = new QPushButton(
        QString::fromUtf8("\xe2\x9e\xa4"), peerPage);  /* ➤ */
    m_peerChatSend->setObjectName("SendBtn");
    m_peerChatSend->setFixedWidth(52);
    m_peerChatSend->setEnabled(false);
    m_peerChatSend->setVisible(false);
    peerInputRow->addWidget(m_peerChatInput, 1);
    peerInputRow->addWidget(m_peerChatSend);
    peerVbox->addLayout(peerInputRow);

    /* Indicatore cifratura peer */
    m_peerCryptoIndic = new QLabel(
        QString::fromUtf8("\xf0\x9f\x94\x92")  /* 🔒 */
        + " <small>AES-256-GCM attivo</small>",
        peerPage);
    m_peerCryptoIndic->setTextFormat(Qt::RichText);
    m_peerCryptoIndic->setVisible(false);
    peerVbox->addWidget(m_peerCryptoIndic);

    /* ─── Sezione "Dispositivi vicini" — scansione attiva ────────
       Aggiunge sotto la lista peer un separatore visivo, un pulsante
       "Scansione dispositivi", la lista dei risultati e il pulsante
       "Connetti al selezionato".                                    */
    auto* nearbyLbl = new QLabel(
        QString::fromUtf8("\xf0\x9f\x93\xa1")  /* 📡 */
        + " <b>Dispositivi vicini</b>",
        peerPage);
    nearbyLbl->setTextFormat(Qt::RichText);
    peerVbox->addWidget(nearbyLbl);

    m_peerScanBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x94\x8d") + " Scansione dispositivi", peerPage);
    m_peerScanBtn->setObjectName("PrimaryBtn");
    m_peerScanBtn->setMinimumHeight(44);
    peerVbox->addWidget(m_peerScanBtn);

    m_deviceList = new QListWidget(peerPage);
    m_deviceList->setObjectName("BleList");
    m_deviceList->setAlternatingRowColors(true);
    m_deviceList->setMaximumHeight(160);
    peerVbox->addWidget(m_deviceList);

    m_connectBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x94\x97") + " Connetti al selezionato", peerPage);
    m_connectBtn->setObjectName("SecondaryBtn");
    m_connectBtn->setMinimumHeight(44);
    m_connectBtn->setEnabled(false);
    peerVbox->addWidget(m_connectBtn);

    m_stack->addWidget(peerPage);   /* indice 2 */

    /* ── BLE Discovery Agent ── */
    m_agent = new QBluetoothDeviceDiscoveryAgent(this);
    m_agent->setLowEnergyDiscoveryTimeout(8000);

    /* ── Classic Discovery Agent (per peer BT) ── */
    m_discoveryAgent = new QBluetoothDeviceDiscoveryAgent(this);
    m_discoveryAgent->setLowEnergyDiscoveryTimeout(0);  /* solo Classic */

    /* ── Active Discovery Agent (sezione Dispositivi vicini) ── */
    m_activeAgent = new QBluetoothDeviceDiscoveryAgent(this);
    m_activeAgent->setLowEnergyDiscoveryTimeout(10000);  /* 10 s, BLE + Classic */

    /* ── Connessioni ── */
    connect(m_scanBtn,    &QPushButton::clicked, this, &BlePage::onStartScan);
    connect(m_chatTabBtn, &QPushButton::clicked, this, &BlePage::onShowChat);
    connect(m_scanTabBtn, &QPushButton::clicked, this, &BlePage::onShowScan);
    connect(m_chatConn,   &QPushButton::clicked, this, &BlePage::onChatConnectClicked);
    connect(m_chatSend,   &QPushButton::clicked, this, &BlePage::onChatSend);
    connect(m_chatInput,  &QLineEdit::returnPressed, this, &BlePage::onChatSend);
    connect(m_list,       &QListWidget::itemClicked, this, &BlePage::onDeviceTapped);

    /* Navigazione verso pagina Peer */
    connect(m_peerTabBtnFromScan, &QPushButton::clicked, this, &BlePage::onShowPeer);
    connect(m_peerTabBtnFromChat, &QPushButton::clicked, this, &BlePage::onShowPeer);
    connect(m_scanTabBtnFromPeer, &QPushButton::clicked, this, &BlePage::onShowScan);
    connect(m_chatTabBtnFromPeer, &QPushButton::clicked, this, &BlePage::onShowChat);

    /* Peer discovery e chat */
    connect(m_discoverBtn,    &QPushButton::clicked,           this, &BlePage::onDiscoverClicked);
    connect(m_peerList,       &QListWidget::itemDoubleClicked, this, &BlePage::onPeerDoubleClicked);
    connect(m_peerChatSend,   &QPushButton::clicked,           this, &BlePage::onBtClientSend);
    connect(m_peerChatInput,  &QLineEdit::returnPressed,       this, &BlePage::onBtClientSend);

    connect(m_discoveryAgent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
            this, &BlePage::onClassicDeviceDiscovered);
    connect(m_discoveryAgent, &QBluetoothDeviceDiscoveryAgent::finished,
            this, &BlePage::onDiscoveryFinished);

    connect(m_agent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
            this, &BlePage::onDeviceDiscovered);
    connect(m_agent, &QBluetoothDeviceDiscoveryAgent::finished,
            this, &BlePage::onScanFinished);
    connect(m_agent,
            QOverload<QBluetoothDeviceDiscoveryAgent::Error>::of(
                &QBluetoothDeviceDiscoveryAgent::errorOccurred),
            this, &BlePage::onScanError);

    /* Scansione attiva — Dispositivi vicini */
    connect(m_peerScanBtn, &QPushButton::clicked,
            this, &BlePage::onScanClicked);
    connect(m_connectBtn,  &QPushButton::clicked,
            this, &BlePage::onConnectToSelected);
    connect(m_deviceList,  &QListWidget::itemSelectionChanged,
            this, &BlePage::onDeviceListSelectionChanged);

    connect(m_activeAgent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
            this, &BlePage::onDeviceFound);
    connect(m_activeAgent, &QBluetoothDeviceDiscoveryAgent::finished,
            this, &BlePage::onPeerScanFinished);

    /* Cifratura: pulsante chiave */
    connect(m_keyInfoBtn, &QPushButton::clicked,
            this, &BlePage::onShowKeyInfo);

    /* ── Carica (o genera) la chiave AES-256 ── */
    m_cryptoKey = BleCrypto::loadOrCreateKey();
    updateCryptoIndicator();

    /* Popola subito con i dispositivi già accoppiati */
    populatePairedDevices();
}

BlePage::~BlePage()
{
    stopScan();
    if (m_discoveryAgent && m_discoveryAgent->isActive()) m_discoveryAgent->stop();
    if (m_activeAgent    && m_activeAgent->isActive())    m_activeAgent->stop();
    if (m_socket)   { m_socket->disconnectFromService(); }
    if (m_btClient) { m_btClient->disconnectFromService(); }
    if (m_btServer) { m_btServer->close(); }
}

/* ── Navigazione tab ─────────────────────────────────────────── */
void BlePage::onShowScan() { m_stack->setCurrentIndex(0); }
void BlePage::onShowChat() { m_stack->setCurrentIndex(1); }
void BlePage::onShowPeer() { m_stack->setCurrentIndex(2); }

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

/* ── onDeviceTapped — mostra dettagli e offre connessione client ─ */
void BlePage::onDeviceTapped(QListWidgetItem* item)
{
    if (!item) return;
    const QString mac  = item->data(Qt::UserRole).toString();
    const QString name = item->data(Qt::UserRole + 1).toString();

    const int choice = QMessageBox::question(this,
        "Dispositivo: " + name,
        QString("Indirizzo: %1\n\nVuoi connetterti a questo dispositivo\nper la chat Bluetooth?").arg(mac),
        "Connetti", "Solo copia MAC", "Annulla", 0, 2);

    if (choice == 0) {
        QGuiApplication::clipboard()->setText(mac);
        connectToDevice(mac, name);
    } else if (choice == 1) {
        QGuiApplication::clipboard()->setText(mac);
        m_chatStatus->setText(
            QString::fromUtf8("\xf0\x9f\x93\x8b")  /* 📋 */
            + " MAC copiato: " + mac);
    }
}

/* ── connectToDevice — connessione client RFCOMM/SPP ────────── */
void BlePage::connectToDevice(const QString& address, const QString& name)
{
    if (m_socket && m_socket->state() == QBluetoothSocket::ConnectedState) {
        m_socket->disconnectFromService();
        if (m_socket) { m_socket->deleteLater(); m_socket = nullptr; }
    }

    m_socket = new QBluetoothSocket(QBluetoothServiceInfo::RfcommProtocol, this);
    connect(m_socket, &QBluetoothSocket::connected,
            this, &BlePage::onClientConnected);
    connect(m_socket, &QBluetoothSocket::readyRead,
            this, &BlePage::onSocketReadyRead);
    connect(m_socket, &QBluetoothSocket::disconnected,
            this, &BlePage::onSocketDisconnected);
    connect(m_socket,
            QOverload<QBluetoothSocket::SocketError>::of(&QBluetoothSocket::errorOccurred),
            this, &BlePage::onClientConnectError);

    m_isServer = false;
    m_chatStatus->setText(
        QString::fromUtf8("\xe2\x8f\xb3")   /* ⏳ */
        + " Connessione a " + name + " in corso...");
    m_chatSend->setEnabled(false);

    /* Connessione RFCOMM alla porta SPP standard (canale 1) */
    m_socket->connectToService(
        QBluetoothAddress(address),
        QBluetoothUuid(kChatUuid));

    m_stack->setCurrentIndex(1);   /* passa alla tab chat */
    appendChatMsg("Sistema",
        QString::fromUtf8("\xe2\x8f\xb3") + " Connessione a " + name + "...");
}

void BlePage::onClientConnected()
{
    const QString peer = m_socket ? m_socket->peerName() : "Remoto";
    m_chatStatus->setText(
        QString::fromUtf8("\xf0\x9f\x9f\xa2")   /* 🟢 */
        + " Connesso a: " + peer);
    m_chatSend->setEnabled(true);
    m_chatConn->setText(QString::fromUtf8("\xe2\x9d\x8c") + " Disconnetti");
    appendChatMsg("Sistema",
        QString::fromUtf8("\xf0\x9f\x9f\xa2") + " Connesso a " + peer + ".");
}

void BlePage::onClientConnectError(QBluetoothSocket::SocketError)
{
    const QString err = m_socket ? m_socket->errorString() : "errore socket";
    m_chatStatus->setText(
        QString::fromUtf8("\xe2\x9d\x8c")   /* ❌ */
        + " Connessione fallita: " + err);
    m_chatSend->setEnabled(false);
    m_chatConn->setText(QString::fromUtf8("\xf0\x9f\x93\xb6") + " Ascolta");
    appendChatMsg("Sistema",
        QString::fromUtf8("\xe2\x9d\x8c") + " Errore: " + err);
    if (m_socket) { m_socket->deleteLater(); m_socket = nullptr; }
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
    /* Link-layer security: lasciamo NoSecurity per compatibilità RFCOMM Classic;
       la confidenzialità è garantita a livello applicativo da BleCrypto. */
    m_btServer->setSecurityFlags(QBluetooth::NoSecurity);
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
    appendChatMsg("Sistema",
        QString::fromUtf8("\xf0\x9f\x94\x92")  /* 🔒 */
        + " Cifratura AES-256-GCM attiva. Assicurati che entrambi i dispositivi usino la stessa chiave.");
}

void BlePage::onSocketReadyRead()
{
    if (!m_socket) return;
    while (m_socket->canReadLine()) {
        const QByteArray raw = m_socket->readLine().trimmed();
        if (raw.isEmpty()) continue;
        const QString peer = m_socket->isValid()
            ? m_socket->peerName()
            : "Remoto";

        if (m_cryptoEnabled) {
            const QString plain = BleCrypto::decryptFromWire(raw, m_cryptoKey);
            if (plain.isNull()) {
                /* Decifratura fallita: potrebbe essere tag errato o chiave diversa */
                appendChatMsg("Sistema",
                    QString::fromUtf8("\xe2\x9a\xa0")  /* ⚠ */
                    + " Messaggio da " + peer
                    + " non decifrabile (chiave diversa o frame corrotto).");
            } else {
                appendChatMsg(peer, plain);
            }
        } else {
            appendChatMsg(peer, QString::fromUtf8(raw));
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

/* ── onChatSend — cifra e invia messaggio al socket ─────────── */
void BlePage::onChatSend()
{
    const QString msg = m_chatInput->text().trimmed();
    if (msg.isEmpty() || !m_socket
        || m_socket->state() != QBluetoothSocket::ConnectedState) return;

    if (m_cryptoEnabled) {
        const QByteArray wire = BleCrypto::encryptToWire(msg, m_cryptoKey);
        if (wire.isEmpty()) {
            appendChatMsg("Errore",
                QString::fromUtf8("\xe2\x9d\x8c") + " Cifratura fallita — messaggio non inviato.");
            return;
        }
        m_socket->write(wire + "\n");
    } else {
        m_socket->write((msg + "\n").toUtf8());
    }
    appendChatMsg("Tu", msg);
    m_chatInput->clear();
}

/* ══════════════════════════════════════════════════════════════
   Peer BT Classic — scopri e connetti dispositivi (client)
   ══════════════════════════════════════════════════════════════ */

/* ── populatePairedDevices — carica dispositivi già accoppiati ── */
void BlePage::populatePairedDevices()
{
    QBluetoothLocalDevice localDev;
    if (!localDev.isValid()) return;

    const QList<QBluetoothAddress> paired =
        localDev.connectedDevices();   /* dispositivi accoppiati/connessi */

    for (const QBluetoothAddress& addr : paired) {
        const QString mac  = addr.toString();
        if (m_peerIndex.contains(mac)) continue;

        const QString name = localDev.pairingStatus(addr) ==
                             QBluetoothLocalDevice::Paired
                             ? mac : mac;   /* il nome arriva solo con discovery */

        auto* item = new QListWidgetItem(
            QString::fromUtf8("\xf0\x9f\x94\x97") + "  " + mac + "\n    (accoppiato)",
            m_peerList);
        item->setData(Qt::UserRole,     mac);
        item->setData(Qt::UserRole + 1, mac);
        m_peerIndex[mac] = m_peerList->count() - 1;
    }
}

/* ── onDiscoverClicked ────────────────────────────────────────── */
void BlePage::onDiscoverClicked()
{
    if (!m_discoveryAgent) return;

    if (m_discoveryAgent->isActive()) {
        m_discoveryAgent->stop();
        m_discoverBtn->setText(
            QString::fromUtf8("\xf0\x9f\x94\x8d") + " Cerca dispositivi BT Classic");
        m_peerStatus->setText(
            QString::fromUtf8("\xe2\x9c\x95") + " Ricerca fermata.");
        return;
    }

    m_peerList->clear();
    m_peerIndex.clear();
    populatePairedDevices();   /* ripopola accoppiati */

    m_peerStatus->setText(
        QString::fromUtf8("\xf0\x9f\x94\x8d") + " Ricerca BT Classic in corso\xe2\x80\xa6");
    m_discoverBtn->setText(
        QString::fromUtf8("\xe2\x9c\x95") + " Ferma ricerca");

    m_discoveryAgent->start(QBluetoothDeviceDiscoveryAgent::ClassicMethod);
}

/* ── onClassicDeviceDiscovered ────────────────────────────────── */
void BlePage::onClassicDeviceDiscovered(const QBluetoothDeviceInfo& device)
{
    if (!(device.coreConfigurations() &
          QBluetoothDeviceInfo::BaseRateCoreConfiguration)) return;  /* solo Classic */

    const QString mac  = device.address().toString();
    const QString name = device.name().isEmpty() ? "(senza nome)" : device.name();
    const QString text = QString::fromUtf8("\xf0\x9f\x93\xb1") +
                         "  " + name + "\n    " + mac;

    if (m_peerIndex.contains(mac)) {
        if (auto* item = m_peerList->item(m_peerIndex[mac]))
            item->setText(text);
    } else {
        auto* item = new QListWidgetItem(text, m_peerList);
        item->setData(Qt::UserRole,     mac);
        item->setData(Qt::UserRole + 1, name);
        m_peerIndex[mac] = m_peerList->count() - 1;
    }
}

/* ── onDiscoveryFinished ─────────────────────────────────────── */
void BlePage::onDiscoveryFinished()
{
    m_peerStatus->setText(
        m_peerList->count() == 0
            ? QString::fromUtf8("\xe2\x9a\xa0") + "  Nessun dispositivo trovato"
            : QString::fromUtf8("\xe2\x9c\x85") +
              QString("  %1 dispositivi trovati. Doppio tap per connetterti.")
                  .arg(m_peerList->count()));
    m_discoverBtn->setText(
        QString::fromUtf8("\xf0\x9f\x94\x8d") + " Cerca dispositivi BT Classic");
}

/* ── onPeerDoubleClicked — avvia connessione RFCOMM client ───── */
void BlePage::onPeerDoubleClicked(QListWidgetItem* item)
{
    if (!item) return;
    const QString mac  = item->data(Qt::UserRole).toString();
    const QString name = item->data(Qt::UserRole + 1).toString();

    if (mac.isEmpty()) return;

    /* Chiudi connessione precedente se esiste */
    if (m_btClient) {
        if (m_btClient->state() == QBluetoothSocket::ConnectedState)
            m_btClient->disconnectFromService();
        m_btClient->deleteLater();
        m_btClient = nullptr;
    }

    m_btClient = new QBluetoothSocket(QBluetoothServiceInfo::RfcommProtocol, this);

    connect(m_btClient, &QBluetoothSocket::connected,
            this, &BlePage::onBtClientConnected);
    connect(m_btClient, &QBluetoothSocket::readyRead,
            this, &BlePage::onBtClientReadyRead);
    connect(m_btClient,
            QOverload<QBluetoothSocket::SocketError>::of(
                &QBluetoothSocket::errorOccurred),
            this, &BlePage::onBtClientError);

    m_peerStatus->setText(
        QString::fromUtf8("\xe2\x8f\xb3") + " Connessione a " + name + " in corso...");
    m_peerChatSend->setEnabled(false);

    m_btClient->connectToService(
        QBluetoothAddress(mac),
        QBluetoothUuid(kChatUuid));
}

/* ── onBtClientConnected ─────────────────────────────────────── */
void BlePage::onBtClientConnected()
{
    const QString peer = m_btClient ? m_btClient->peerName() : "Remoto";
    m_peerStatus->setText(
        QString::fromUtf8("\xf0\x9f\x9f\xa2") + " Connesso a: " + peer);
    m_peerChatLog->setVisible(true);
    m_peerChatInput->setVisible(true);
    m_peerChatSend->setVisible(true);
    m_peerChatSend->setEnabled(true);
    if (m_peerCryptoIndic) m_peerCryptoIndic->setVisible(true);
    appendPeerMsg("Sistema",
        QString::fromUtf8("\xf0\x9f\x9f\xa2") + " Connesso a " + peer + ".");
    appendPeerMsg("Sistema",
        QString::fromUtf8("\xf0\x9f\x94\x92")  /* 🔒 */
        + " Cifratura AES-256-GCM attiva. Assicurati che entrambi i dispositivi usino la stessa chiave.");
}

/* ── onBtClientReadyRead — legge e decifra frame ─────────────── */
void BlePage::onBtClientReadyRead()
{
    if (!m_btClient) return;
    while (m_btClient->canReadLine()) {
        const QByteArray raw = m_btClient->readLine().trimmed();
        if (raw.isEmpty()) continue;
        const QString peer = m_btClient->isValid()
            ? m_btClient->peerName()
            : "Remoto";

        if (m_cryptoEnabled) {
            const QString plain = BleCrypto::decryptFromWire(raw, m_cryptoKey);
            if (plain.isNull()) {
                appendPeerMsg("Sistema",
                    QString::fromUtf8("\xe2\x9a\xa0")  /* ⚠ */
                    + " Messaggio da " + peer
                    + " non decifrabile (chiave diversa o frame corrotto).");
            } else {
                appendPeerMsg(peer, plain);
            }
        } else {
            appendPeerMsg(peer, QString::fromUtf8(raw));
        }
    }
}

/* ── onBtClientError ─────────────────────────────────────────── */
void BlePage::onBtClientError(QBluetoothSocket::SocketError)
{
    const QString err = m_btClient ? m_btClient->errorString() : "errore socket";
    m_peerStatus->setText(
        QString::fromUtf8("\xe2\x9d\x8c") + " Connessione fallita: " + err);
    m_peerChatSend->setEnabled(false);
    appendPeerMsg("Sistema",
        QString::fromUtf8("\xe2\x9d\x8c") + " Errore: " + err);
    if (m_btClient) { m_btClient->deleteLater(); m_btClient = nullptr; }
}

/* ── onBtClientSend — cifra e invia messaggio tramite client ─── */
void BlePage::onBtClientSend()
{
    const QString msg = m_peerChatInput->text().trimmed();
    if (msg.isEmpty() || !m_btClient
        || m_btClient->state() != QBluetoothSocket::ConnectedState) return;

    if (m_cryptoEnabled) {
        const QByteArray wire = BleCrypto::encryptToWire(msg, m_cryptoKey);
        if (wire.isEmpty()) {
            appendPeerMsg("Errore",
                QString::fromUtf8("\xe2\x9d\x8c") + " Cifratura fallita — messaggio non inviato.");
            return;
        }
        m_btClient->write(wire + "\n");
    } else {
        m_btClient->write((msg + "\n").toUtf8());
    }
    appendPeerMsg("Tu", msg);
    m_peerChatInput->clear();
}

/* ── appendPeerMsg — aggiunge messaggio al log peer ─────────── */
void BlePage::appendPeerMsg(const QString& sender, const QString& text)
{
    const QString ts = QDateTime::currentDateTime().toString("HH:mm");
    m_peerChatLog->append(
        QString("<b>[%1] %2:</b> %3").arg(ts, sender, text.toHtmlEscaped()));
    m_peerChatLog->verticalScrollBar()->setValue(
        m_peerChatLog->verticalScrollBar()->maximum());
}

/* ══════════════════════════════════════════════════════════════
   Scansione attiva — Dispositivi vicini
   ══════════════════════════════════════════════════════════════ */

/* ── onScanClicked — avvia/ferma scansione dispositivi vicini ── */
void BlePage::onScanClicked()
{
    if (!m_activeAgent) return;

    if (m_activeAgent->isActive()) {
        m_activeAgent->stop();
        m_peerScanBtn->setText(
            QString::fromUtf8("\xf0\x9f\x94\x8d") + " Scansione dispositivi");
        m_peerScanBtn->setEnabled(true);
        return;
    }

    m_deviceList->clear();
    m_foundDevices.clear();
    m_foundIndex.clear();
    m_connectBtn->setEnabled(false);

    m_peerScanBtn->setText(
        QString::fromUtf8("\xe2\x9c\x95") + " Scansione\xe2\x80\xa6");
    m_peerScanBtn->setEnabled(false);
    m_peerStatus->setText(
        QString::fromUtf8("\xf0\x9f\x93\xa1") + "  Ricerca dispositivi vicini in corso\xe2\x80\xa6");

    m_activeAgent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod
                         | QBluetoothDeviceDiscoveryAgent::ClassicMethod);
}

/* ── onDeviceFound — aggiunge dispositivo trovato alla lista ─── */
void BlePage::onDeviceFound(const QBluetoothDeviceInfo& info)
{
    const QString mac  = info.address().toString();
    if (mac.isEmpty()) return;

    const QString name = info.name().isEmpty() ? "(senza nome)" : info.name();
    const QString rssi = info.rssi() != 0
                         ? QString(" %1 dBm").arg(info.rssi())
                         : QString();
    const QString text = QString::fromUtf8("\xf0\x9f\x93\xb1")  /* 📱 */
                         + "  " + name + "\n    " + mac + rssi;

    if (m_foundIndex.contains(mac)) {
        /* Aggiorna voce esistente (es. RSSI cambiato) */
        if (auto* item = m_deviceList->item(m_foundIndex[mac]))
            item->setText(text);
    } else {
        m_foundDevices.append(info);
        auto* item = new QListWidgetItem(text, m_deviceList);
        item->setData(Qt::UserRole,     mac);
        item->setData(Qt::UserRole + 1, name);
        m_foundIndex[mac] = m_deviceList->count() - 1;
    }
}

/* ── onPeerScanFinished — ripristina pulsante al termine scan ── */
void BlePage::onPeerScanFinished()
{
    m_peerScanBtn->setText(
        QString::fromUtf8("\xf0\x9f\x94\x8d") + " Scansione dispositivi");
    m_peerScanBtn->setEnabled(true);

    m_peerStatus->setText(
        m_deviceList->count() == 0
            ? QString::fromUtf8("\xe2\x9a\xa0")  /* ⚠ */
              + "  Nessun dispositivo trovato nelle vicinanze"
            : QString::fromUtf8("\xe2\x9c\x85")  /* ✅ */
              + QString("  %1 dispositivi trovati — seleziona e premi Connetti")
                  .arg(m_deviceList->count()));
}

/* ── onDeviceListSelectionChanged — abilita pulsante Connetti ── */
void BlePage::onDeviceListSelectionChanged()
{
    m_connectBtn->setEnabled(!m_deviceList->selectedItems().isEmpty());
}

/* ══════════════════════════════════════════════════════════════
   Cifratura BLE — gestione chiave
   ══════════════════════════════════════════════════════════════ */

/* ── updateCryptoIndicator — aggiorna etichetta lucchetto ──────── */
void BlePage::updateCryptoIndicator()
{
    const QString shortKey = m_cryptoKey.toBase64().left(8) + "...";

#ifdef HAVE_OPENSSL_CRYPTO
    const QString algo = "AES-256-GCM";
#else
    const QString algo = "HMAC-XOR (fallback)";
#endif

    const QString text = m_cryptoEnabled
        ? QString::fromUtf8("\xf0\x9f\x94\x92")  /* 🔒 */
          + QString(" <small><b>%1</b> — chiave: %2</small>").arg(algo, shortKey)
        : QString::fromUtf8("\xf0\x9f\x94\x93")  /* 🔓 */
          + " <small>Cifratura <b>disabilitata</b></small>";

    if (m_cryptoIndicator)  m_cryptoIndicator->setText(text);
    if (m_peerCryptoIndic)  m_peerCryptoIndic->setText(text);
}

/* ── onShowKeyInfo — mostra la chiave e offre reset/copia ──────── */
void BlePage::onShowKeyInfo()
{
    const QString keyB64 = m_cryptoKey.toBase64();
    const QString fingerprint = keyB64.left(16) + "...";

#ifdef HAVE_OPENSSL_CRYPTO
    const QString algoInfo = "AES-256-GCM (OpenSSL 3)";
#else
    const QString algoInfo = "HMAC-SHA256-XOR (fallback — nessun OpenSSL)";
#endif

    const QString info =
        QString("Algoritmo: %1\n\n"
                "Chiave corrente (Base64, 44 char = 256 bit):\n%2\n\n"
                "Condividi questa chiave con l'altro dispositivo tramite:\n"
                "  • Copia e incolla via messaggio sicuro\n"
                "  • QR code (funzione in sviluppo)\n\n"
                "Vuoi generare una NUOVA chiave?\n"
                "(i messaggi precedenti non saranno più leggibili)")
            .arg(algoInfo, keyB64);

    const int choice = QMessageBox::question(
        this,
        QString::fromUtf8("\xf0\x9f\x94\x91") + " Chiave cifratura BLE",
        info,
        "Copia chiave",
        "Nuova chiave",
        "Annulla",
        0, 2);

    if (choice == 0) {
        QGuiApplication::clipboard()->setText(keyB64);
        appendChatMsg("Sistema",
            QString::fromUtf8("\xf0\x9f\x93\x8b")  /* 📋 */
            + " Chiave copiata negli appunti. Inviala al peer in modo sicuro.");
    } else if (choice == 1) {
        onResetKey();
    }
}

/* ── onResetKey — genera e salva nuova chiave ──────────────────── */
void BlePage::onResetKey()
{
    m_cryptoKey = BleCrypto::generateKey();
    BleCrypto::saveKey(m_cryptoKey);
    updateCryptoIndicator();

    const QString newKeyB64 = m_cryptoKey.toBase64();
    QGuiApplication::clipboard()->setText(newKeyB64);

    appendChatMsg("Sistema",
        QString::fromUtf8("\xf0\x9f\x94\x91")  /* 🔑 */
        + " Nuova chiave generata e copiata negli appunti.");
    appendChatMsg("Sistema",
        QString::fromUtf8("\xe2\x9a\xa0")  /* ⚠ */
        + " Attenzione: il peer deve usare la stessa chiave per decifrare.");
}

/* ── onConnectToSelected — connette al dispositivo selezionato ── */
void BlePage::onConnectToSelected()
{
    QListWidgetItem* item = m_deviceList->currentItem();
    if (!item) return;

    const QString mac  = item->data(Qt::UserRole).toString();
    const QString name = item->data(Qt::UserRole + 1).toString();
    if (mac.isEmpty()) return;

    /* Chiudi connessione client precedente se presente */
    if (m_btClient) {
        if (m_btClient->state() == QBluetoothSocket::ConnectedState)
            m_btClient->disconnectFromService();
        m_btClient->deleteLater();
        m_btClient = nullptr;
    }

    m_btClient = new QBluetoothSocket(QBluetoothServiceInfo::RfcommProtocol, this);

    connect(m_btClient, &QBluetoothSocket::connected,
            this, &BlePage::onBtClientConnected);
    connect(m_btClient, &QBluetoothSocket::readyRead,
            this, &BlePage::onBtClientReadyRead);
    connect(m_btClient,
            QOverload<QBluetoothSocket::SocketError>::of(
                &QBluetoothSocket::errorOccurred),
            this, &BlePage::onBtClientError);

    m_peerStatus->setText(
        QString::fromUtf8("\xe2\x8f\xb3")  /* ⏳ */
        + " Connessione a " + name + " (" + mac + ") in corso...");
    m_peerChatSend->setEnabled(false);
    m_connectBtn->setEnabled(false);

    m_btClient->connectToService(
        QBluetoothAddress(mac),
        QBluetoothUuid(kChatUuid));
}
