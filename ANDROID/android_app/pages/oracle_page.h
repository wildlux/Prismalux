#pragma once
#include <QWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include "../ai_client.h"

/* ══════════════════════════════════════════════════════════════
   OraclePage — Chat rapida con pillole azione tocco-singolo.

   Layout verticale:
     Barra pillole orizzontale scrollabile (touch)
     QTextEdit output (streaming)
     QTextEdit input + Invia / Stop
     Label status
   ══════════════════════════════════════════════════════════════ */
class OraclePage : public QWidget {
    Q_OBJECT
public:
    explicit OraclePage(AiClient* ai, QWidget* parent = nullptr);

private slots:
    void onPillClicked();
    void onSendClicked();
    void onToken(const QString& t);
    void onFinished(const QString& full);
    void onAiError(const QString& msg);
    void onAborted();
    void setBusy(bool busy);

private:
    AiClient*    m_ai        = nullptr;
    QTextEdit*   m_output    = nullptr;
    QTextEdit*   m_input     = nullptr;
    QPushButton* m_sendBtn   = nullptr;
    QPushButton* m_stopBtn   = nullptr;
    QLabel*      m_statusLbl = nullptr;
};
