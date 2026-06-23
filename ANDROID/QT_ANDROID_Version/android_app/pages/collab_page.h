#pragma once
#include <QWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QStackedWidget>
#include <QComboBox>
#include "../ai_client.h"

/* Collab — BT chat + PC Hub via LAN server. */
class CollabPage : public QWidget {
    Q_OBJECT
public:
    explicit CollabPage(AiClient* ai, QWidget* parent = nullptr);

private slots:
    void onSendChatClicked();
    void onConnectPcClicked();
    void onSendToPcClicked();
    void onPcActionClicked();
    void onToken(const QString& t);
    void onFinished(const QString& full);
    void onAiError(const QString& msg);
    void onModeChanged(int idx);
    void setBusy(bool busy);

private:
    QWidget* buildBtTab();
    QWidget* buildPcHubTab();
    void appendChat(const QString& from, const QString& msg);

    AiClient*       m_ai        = nullptr;
    QComboBox*      m_modeCmb   = nullptr;
    QStackedWidget* m_modeStack = nullptr;

    /* BT Chat (simulata via AI) */
    QTextEdit*   m_btChat      = nullptr;
    QLineEdit*   m_btInput     = nullptr;
    QPushButton* m_btSendBtn   = nullptr;
    QPushButton* m_btStopBtn   = nullptr;

    /* PC Hub */
    QLineEdit*   m_pcIp        = nullptr;
    QLineEdit*   m_pcPort      = nullptr;
    QPushButton* m_pcConnBtn   = nullptr;
    QComboBox*   m_pcActionCmb = nullptr;
    QLineEdit*   m_pcPayload   = nullptr;
    QPushButton* m_pcSendBtn   = nullptr;
    QTextEdit*   m_pcLog       = nullptr;

    QLabel*      m_status      = nullptr;
};
