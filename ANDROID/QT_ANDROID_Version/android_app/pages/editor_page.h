#pragma once
#include <QWidget>
#include <QTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include "../ai_client.h"

/* Editor testo + REPL Python integrato per Android. */
class EditorPage : public QWidget {
    Q_OBJECT
public:
    explicit EditorPage(AiClient* ai, QWidget* parent = nullptr);

private slots:
    void onRunClicked();
    void onClearClicked();
    void onAiExplainClicked();
    void onAiFixClicked();
    void onToken(const QString& t);
    void onFinished(const QString& full);
    void onAiError(const QString& msg);
    void onAborted();
    void setBusy(bool busy);

private:
    AiClient*    m_ai       = nullptr;
    QComboBox*   m_langCmb  = nullptr;
    QTextEdit*   m_editor   = nullptr;
    QTextEdit*   m_output   = nullptr;
    QPushButton* m_runBtn   = nullptr;
    QPushButton* m_clrBtn   = nullptr;
    QPushButton* m_explBtn  = nullptr;
    QPushButton* m_fixBtn   = nullptr;
    QPushButton* m_stopBtn  = nullptr;
    QLabel*      m_status   = nullptr;
    bool         m_aiMode   = false;
};
