#pragma once
#include <QWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QListWidget>
#include "../ai_client.h"

/* ══════════════════════════════════════════════════════════════
   SecurityPage mobile — analisi sicurezza codice con 4 agenti sequenziali.

   Agenti:
     1. Injection    — SQL/XSS/Command injection
     2. Segreti      — password, token, chiavi API hardcoded
     3. Memoria      — buffer overflow, null pointer, use-after-free
     4. Config       — HTTPS, certificati, valori hardcoded

   Mobile: esecuzione sequenziale (risparmio batteria).
   ══════════════════════════════════════════════════════════════ */
class SecurityPage : public QWidget {
    Q_OBJECT
public:
    explicit SecurityPage(AiClient* ai, QWidget* parent = nullptr);

private slots:
    void onAnalyzeClicked();
    void onStopClicked();
    void onToken(const QString& t);
    void onFinished(const QString& full);
    void onAiError(const QString& msg);
    void runNextAgent();

private:
    void appendResult(const QString& agentName, const QString& text);

    AiClient*     m_ai          = nullptr;
    QTextEdit*    m_codeInput   = nullptr;
    QPushButton*  m_analyzeBtn  = nullptr;
    QPushButton*  m_stopBtn     = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QLabel*       m_statusLbl   = nullptr;
    QTextEdit*    m_output      = nullptr;
    QListWidget*  m_agentList   = nullptr;

    int     m_currentAgent = 0;
    bool    m_busy         = false;
    QString m_currentResult;

    QMetaObject::Connection m_tokConn;
    QMetaObject::Connection m_finConn;
    QMetaObject::Connection m_errConn;
};
