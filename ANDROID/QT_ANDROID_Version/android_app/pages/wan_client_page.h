#pragma once
#include <QWidget>
#include <QTcpSocket>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>
#include "../ai_client.h"

/* ══════════════════════════════════════════════════════════════
   WanClientPage — Mobile come nodo worker del server WAN desktop.

   Protocollo: JSON newline-delimited su TCP (porta 11600).
   Il mobile si connette al server desktop, riceve task AI,
   li esegue via AiClient e restituisce i risultati.

   Tipi task supportati: llm_prompt, ai_chat, echo.
   ══════════════════════════════════════════════════════════════ */
class WanClientPage : public QWidget {
    Q_OBJECT
public:
    explicit WanClientPage(AiClient* ai, QWidget* parent = nullptr);

private slots:
    void onConnectClicked();
    void onDisconnectClicked();
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketReadyRead();
    void onSocketError(QAbstractSocket::SocketError err);
    void onAiToken(const QString& t);
    void onAiFinished(const QString& full);
    void onAiError(const QString& msg);

private:
    void sendJson(const QJsonObject& obj);
    void handleMessage(const QJsonObject& msg);
    void startTask(const QString& id, const QString& kind, const QString& payload);
    void finishTask(const QString& result);
    void appendLog(const QString& msg);
    void setConnected(bool connected);

    AiClient*   m_ai         = nullptr;
    QTcpSocket* m_sock       = nullptr;

    QLineEdit*   m_hostEdit  = nullptr;
    QSpinBox*    m_portSpin  = nullptr;
    QLineEdit*   m_nameEdit  = nullptr;
    QPushButton* m_connectBtn   = nullptr;
    QPushButton* m_disconnectBtn= nullptr;
    QLabel*      m_statusLbl  = nullptr;
    QTextEdit*   m_logEdit    = nullptr;
    QTextEdit*   m_taskOutput = nullptr;
    QLabel*      m_taskLbl    = nullptr;

    QString m_currentTaskId;
    QString m_accum;
    QByteArray m_readBuf;
};
