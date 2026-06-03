#pragma once
#include <QWidget>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QStackedWidget>
#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothDeviceInfo>
#include <QBluetoothSocket>
#include <QBluetoothServer>
#include <QBluetoothLocalDevice>
#include <QByteArray>
#include "ble_crypto.h"

/* --------------------------------------------------------------
   BlePage — Bluetooth: scanner BLE + chat cifrata (Classic BT).

   Tre modalità (QStackedWidget interno):
     0 — Scanner BLE: cerca dispositivi vicini (BLE)
     1 — Chat BT: connessione RFCOMM server/ascolta  [AES-256-GCM]
     2 — Peer BT: scopri e connetti dispositivi accoppiati (client) [AES-256-GCM]

   Sicurezza:
     Ogni messaggio viene cifrato con BleCrypto::encryptToWire()
     prima di essere scritto sul socket, e decifrato con
     BleCrypto::decryptFromWire() al momento della lettura.
     La chiave simmetrica (256 bit) è generata casualmente al
     primo avvio e persistita in QSettings ("BleCrypto/session_key").
     Può essere condivisa col peer via QR o PIN (vedere onShareKey()).
   -------------------------------------------------------------- */
class BlePage : public QWidget {
    Q_OBJECT
public:
    explicit BlePage(QWidget* parent = nullptr);
    ~BlePage() override;

    void stopScan();

private slots:
    /* Scanner BLE */
    void onStartScan();
    void onDeviceDiscovered(const QBluetoothDeviceInfo& info);
    void onScanFinished();
    void onScanError(QBluetoothDeviceDiscoveryAgent::Error error);
    void onDeviceTapped(QListWidgetItem* item);

    /* Chat BT (server/ascolta) */
    void onChatSend();
    void onChatConnectClicked();
    void onNewConnection();
    void onSocketReadyRead();
    void onSocketDisconnected();
    void onSocketError(QBluetoothSocket::SocketError error);
    void onClientConnected();
    void onClientConnectError(QBluetoothSocket::SocketError error);

    /* Peer BT Classic — discovery e connessione client */
    void onDiscoverClicked();
    void onClassicDeviceDiscovered(const QBluetoothDeviceInfo& device);
    void onDiscoveryFinished();
    void onPeerDoubleClicked(QListWidgetItem* item);
    void onBtClientConnected();
    void onBtClientReadyRead();
    void onBtClientError(QBluetoothSocket::SocketError err);
    void onBtClientSend();

    /* Scansione attiva dispositivi vicini (sezione "Dispositivi vicini" nel tab Connetti) */
    void onScanClicked();
    void onDeviceFound(const QBluetoothDeviceInfo& info);
    void onPeerScanFinished();
    void onConnectToSelected();
    void onDeviceListSelectionChanged();

    /* Navigazione */
    void onShowScan();
    void onShowChat();
    void onShowPeer();

    /* Cifratura — gestione chiave */
    void onShowKeyInfo();   /* mostra chiave corrente (Base64) per condivisione manuale */
    void onResetKey();      /* genera nuova chiave e la salva */
    void onToggleCrypto();  /* attiva/disattiva cifratura AES-256-GCM a runtime */

private:
    QString rssiIcon(int rssi) const;
    void    addOrUpdateDevice(const QBluetoothDeviceInfo& info);
    void    appendChatMsg(const QString& sender, const QString& text);
    void    appendPeerMsg(const QString& sender, const QString& text);
    void    startServer();
    void    connectToDevice(const QString& address, const QString& name);
    void    populatePairedDevices();
    void    updateCryptoIndicator();  /* aggiorna icona lucchetto nella UI */

    /* Stack 0 = scanner BLE, 1 = chat server, 2 = peer client */
    QStackedWidget* m_stack      = nullptr;

    /* Scanner BLE */
    QBluetoothDeviceDiscoveryAgent* m_agent    = nullptr;
    QListWidget*   m_list      = nullptr;
    QPushButton*   m_scanBtn   = nullptr;
    QLabel*        m_statusLbl = nullptr;
    QLabel*        m_countLbl  = nullptr;
    QPushButton*   m_chatTabBtn = nullptr;
    QPushButton*   m_peerTabBtnFromScan = nullptr;

    /* Chat BT (server) */
    QTextEdit*     m_chatLog   = nullptr;
    QLineEdit*     m_chatInput = nullptr;
    QPushButton*   m_chatSend  = nullptr;
    QPushButton*   m_chatConn  = nullptr;
    QLabel*        m_chatStatus = nullptr;
    QPushButton*   m_scanTabBtn = nullptr;
    QPushButton*   m_peerTabBtnFromChat = nullptr;

    /* Socket BT RFCOMM (server) */
    QBluetoothServer* m_btServer = nullptr;
    QBluetoothSocket* m_socket   = nullptr;
    bool m_isServer = false;

    /* Peer BT Classic (client) */
    QBluetoothDeviceDiscoveryAgent* m_discoveryAgent = nullptr;
    QBluetoothSocket*               m_btClient       = nullptr;
    QListWidget*                    m_peerList        = nullptr;
    QPushButton*                    m_discoverBtn     = nullptr;
    QLabel*                         m_peerStatus      = nullptr;
    QTextEdit*                      m_peerChatLog     = nullptr;
    QLineEdit*                      m_peerChatInput   = nullptr;
    QPushButton*                    m_peerChatSend    = nullptr;
    QPushButton*                    m_scanTabBtnFromPeer = nullptr;
    QPushButton*                    m_chatTabBtnFromPeer = nullptr;

    /* Sezione "Dispositivi vicini" — scansione attiva nel tab Connetti */
    QBluetoothDeviceDiscoveryAgent* m_activeAgent    = nullptr;
    QListWidget*                    m_deviceList      = nullptr;
    QPushButton*                    m_peerScanBtn     = nullptr;
    QPushButton*                    m_connectBtn      = nullptr;
    QList<QBluetoothDeviceInfo>     m_foundDevices;

    QMap<QString, int> m_deviceIndex;
    QMap<QString, int> m_peerIndex;
    QMap<QString, int> m_foundIndex;  /* MAC → indice in m_deviceList */

    /* ── Cifratura ────────────────────────────────────────────── */
    QByteArray   m_cryptoKey;                /* chiave AES-256 corrente */
    /* AUDIT-FIX 2026-06-03: cifratura abilitata di default — opt-out, non opt-in */
    bool         m_cryptoEnabled = true;     /* default: AES-256-GCM attivo */
    QLabel*      m_cryptoIndicator  = nullptr;  /* icona lucchetto nella chat */
    QLabel*      m_peerCryptoIndic  = nullptr;  /* icona lucchetto nella peer chat */
    QPushButton* m_keyInfoBtn       = nullptr;  /* mostra/resetta chiave */
    QPushButton* m_cryptoToggleBtn  = nullptr;  /* toggle 🔓/🔒 */
};
