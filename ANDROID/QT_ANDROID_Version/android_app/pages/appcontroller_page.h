#pragma once
#include <QWidget>
#include <QStackedWidget>
#include <QComboBox>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QSpinBox>
#include <QJsonObject>
#include "../ai_client.h"

/* AppController mobile — Anki FlashCard + TinyMCP client. */
class AppControllerPage : public QWidget {
    Q_OBJECT
public:
    explicit AppControllerPage(AiClient* ai, QWidget* parent = nullptr);

private slots:
    /* Anki */
    void onAnkiConnect();
    void onAnkiGetDecks();
    void onAnkiAddCard();
    void onAnkiGenerate();
    void onAnkiReview();
    void onAnkiNetReply();

    /* TinyMCP */
    void onMcpConnect();
    void onMcpListTools();
    void onMcpExecute();
    void onMcpNetReply();

    /* AI */
    void onToken(const QString& t);
    void onFinished(const QString& full);
    void onAiError(const QString& msg);
    void onAborted();
    void setAiBusy(bool busy);

    void onModeChanged(int idx);

private:
    QWidget* buildAnkiTab();
    QWidget* buildMcpTab();
    void ankiRequest(const QString& action, const QJsonObject& params = {});
    void mcpRequest(const QString& endpoint, const QJsonObject& body = {});
    void appendAnkiLog(const QString& msg, bool ok = true);
    void appendMcpLog(const QString& msg, bool ok = true);

    AiClient*       m_ai        = nullptr;
    QComboBox*      m_modeCmb   = nullptr;
    QStackedWidget* m_modeStack = nullptr;

    /* Anki tab */
    QLineEdit*   m_ankiHost    = nullptr;
    QPushButton* m_ankiConnBtn = nullptr;
    QListWidget* m_ankiDecks   = nullptr;
    QLineEdit*   m_ankiFront   = nullptr;
    QTextEdit*   m_ankiBack    = nullptr;
    QLineEdit*   m_ankiDeck    = nullptr;
    QPushButton* m_ankiAddBtn  = nullptr;
    QPushButton* m_ankiGenBtn  = nullptr;
    QPushButton* m_ankiRevBtn  = nullptr;
    QPushButton* m_ankiStopBtn = nullptr;
    QTextEdit*   m_ankiLog     = nullptr;

    /* TinyMCP tab */
    QLineEdit*   m_mcpHost     = nullptr;
    QLineEdit*   m_mcpPort     = nullptr;
    QPushButton* m_mcpConnBtn  = nullptr;
    QListWidget* m_mcpTools    = nullptr;
    QLineEdit*   m_mcpToolName = nullptr;
    QTextEdit*   m_mcpParams   = nullptr;
    QPushButton* m_mcpExecBtn  = nullptr;
    QTextEdit*   m_mcpLog      = nullptr;

    QLabel*      m_status      = nullptr;
    bool         m_aiMode      = false;
};
