#pragma once
#include <QWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QAbstractSocket>

class QWebSocket;
class QListWidgetItem;

/* ══════════════════════════════════════════════════════════════
   ObsPage — Controllo OBS Studio via WebSocket v5 (porta 4455).

   Funzioni: connetti, cambia scena, avvia/ferma registrazione e
   streaming. Auth SHA-256 come da protocollo obs-websocket v5.

   Richiede Qt6::WebSockets (define HAVE_OBS_WS).
   ══════════════════════════════════════════════════════════════ */
class ObsPage : public QWidget {
    Q_OBJECT
public:
    explicit ObsPage(QWidget* parent = nullptr);
    ~ObsPage() override;

private slots:
    void onConnectClicked();
    void onDisconnectClicked();
    void onWsConnected();
    void onWsDisconnected();
    void onWsMessage(const QString& msg);
    void onWsError(QAbstractSocket::SocketError err);
    void onSceneDoubleClicked(QListWidgetItem* item);
    void onRecordClicked();
    void onStreamClicked();
    void onRefreshClicked();

private:
    void sendJson(const QJsonObject& obj);
    void handleMessage(const QJsonObject& obj);
    void handleIdentify(const QJsonObject& authData);
    void handleSceneList(const QJsonObject& data);
    void requestSceneList();
    void requestStatus();
    void setConnected(bool on);

    QWebSocket*  m_ws         = nullptr;

    QLineEdit*   m_hostEdit   = nullptr;
    QLineEdit*   m_portEdit   = nullptr;
    QLineEdit*   m_pwdEdit    = nullptr;
    QPushButton* m_btnConnect = nullptr;
    QPushButton* m_btnDisconn = nullptr;
    QLabel*      m_statusLbl  = nullptr;
    QListWidget* m_sceneList  = nullptr;
    QPushButton* m_btnRecord  = nullptr;
    QPushButton* m_btnStream  = nullptr;
    QPushButton* m_btnRefresh = nullptr;
    QLabel*      m_curSceneLbl = nullptr;

    bool    m_connected  = false;
    bool    m_recording  = false;
    bool    m_streaming  = false;
    int     m_msgId      = 1;
    QString m_password;
};
