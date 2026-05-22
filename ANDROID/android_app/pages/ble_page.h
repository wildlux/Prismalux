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

/* --------------------------------------------------------------
   BlePage — Bluetooth: scanner BLE + chat in chiaro (Classic BT).

   Tre modalità (QStackedWidget interno):
     0 — Scanner BLE: cerca dispositivi vicini (BLE)
     1 — Chat BT: connessione RFCOMM server/ascolta
     2 — Peer BT: scopri e connetti dispositivi accoppiati (client)
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

    /* Navigazione */
    void onShowScan();
    void onShowChat();
    void onShowPeer();

private:
    QString rssiIcon(int rssi) const;
    void    addOrUpdateDevice(const QBluetoothDeviceInfo& info);
    void    appendChatMsg(const QString& sender, const QString& text);
    void    appendPeerMsg(const QString& sender, const QString& text);
    void    startServer();
    void    connectToDevice(const QString& address, const QString& name);
    void    populatePairedDevices();

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

    QMap<QString, int> m_deviceIndex;
    QMap<QString, int> m_peerIndex;
};
