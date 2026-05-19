#pragma once
#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QStackedWidget>
#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothDeviceInfo>
#include <QBluetoothSocket>
#include <QBluetoothServer>

/* --------------------------------------------------------------
   BlePage — Bluetooth: scanner BLE + chat in chiaro (Classic BT).

   Due modalità (QStackedWidget interno):
     0 — Scanner BLE: cerca dispositivi vicini
     1 — Chat BT: connessione RFCOMM in chiaro tra telefoni
   -------------------------------------------------------------- */
class BlePage : public QWidget {
    Q_OBJECT
public:
    explicit BlePage(QWidget* parent = nullptr);
    ~BlePage() override;

    void stopScan();

private slots:
    /* Scanner */
    void onStartScan();
    void onDeviceDiscovered(const QBluetoothDeviceInfo& info);
    void onScanFinished();
    void onScanError(QBluetoothDeviceDiscoveryAgent::Error error);
    void onDeviceTapped(QListWidgetItem* item);

    /* Chat BT */
    void onChatSend();
    void onChatConnectClicked();
    void onNewConnection();
    void onSocketReadyRead();
    void onSocketDisconnected();
    void onSocketError(QBluetoothSocket::SocketError error);

    /* Navigazione */
    void onShowScan();
    void onShowChat();

private:
    QString rssiIcon(int rssi) const;
    void    addOrUpdateDevice(const QBluetoothDeviceInfo& info);
    void    appendChatMsg(const QString& sender, const QString& text);
    void    startServer();

    /* Stack 0 = scanner, 1 = chat */
    QStackedWidget* m_stack      = nullptr;

    /* Scanner */
    QBluetoothDeviceDiscoveryAgent* m_agent    = nullptr;
    QListWidget*   m_list      = nullptr;
    QPushButton*   m_scanBtn   = nullptr;
    QLabel*        m_statusLbl = nullptr;
    QLabel*        m_countLbl  = nullptr;
    QPushButton*   m_chatTabBtn = nullptr;

    /* Chat */
    QTextEdit*     m_chatLog   = nullptr;
    QLineEdit*     m_chatInput = nullptr;
    QPushButton*   m_chatSend  = nullptr;
    QPushButton*   m_chatConn  = nullptr;
    QLabel*        m_chatStatus = nullptr;
    QPushButton*   m_scanTabBtn = nullptr;

    /* Socket BT RFCOMM */
    QBluetoothServer* m_btServer = nullptr;
    QBluetoothSocket* m_socket   = nullptr;
    bool m_isServer = false;

    QMap<QString, int> m_deviceIndex;
};
