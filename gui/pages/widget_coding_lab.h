#pragma once
/*
 * widget_coding_lab.h — "Lab Coding": genera codice, lo testa e mostra solo
 * il risultato finale. Poi chiede "cosa vuoi modificare?" con aneddoto tecnico.
 * Livello linguaggio: perito informatico — chiaro, tecnico, con spunto di crescita.
 */
#include <QWidget>
#include <QTextBrowser>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QProcess>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QMetaObject>
#include "../ai_client.h"

class CodingLabWidget : public QWidget {
    Q_OBJECT
public:
    explicit CodingLabWidget(AiClient* ai, QWidget* parent = nullptr);
    ~CodingLabWidget() override;

private slots:
    void onRunClicked();
    void onModifyClicked();
    void onAbortClicked();

    /* Pipeline: generazione → test → fix → aneddoto */
    void onCodeToken(const QString& t);
    void onCodeFinished();
    void onCodeError(const QString& e);

    void onCodeRunOutput();
    void onCodeRunFinished(int exitCode, QProcess::ExitStatus status);

    void onFixToken(const QString& t);
    void onFixFinished();
    void onFixError(const QString& e);

    void onAnecdoteToken(const QString& t);
    void onAnecdoteFinished();

private:
    enum State { Idle, Generating, Testing, Fixing, Anecdote, Done };

    void startGeneration(const QString& userRequest);
    void runCode(const QString& code);
    void startFix();
    void startAnecdote();
    void showResult();
    void setPhase(const QString& msg);
    void setIdle();
    void disconnectAi();
    static QString extractCode(const QString& raw);

    AiClient*     m_ai;
    State         m_state       = Idle;
    int           m_retries     = 0;
    static constexpr int kMaxRetries = 2;

    QString  m_currentCode;
    QString  m_codeBuffer;    /* accumula token AI */
    QString  m_runOutput;     /* stdout+stderr del processo */
    QString  m_runError;      /* stderr separato */
    QString  m_anecdoteBuffer;
    QString  m_lastRequest;   /* per il contesto modifica */

    QProcess* m_proc = nullptr;

    QMetaObject::Connection m_connToken;
    QMetaObject::Connection m_connFinished;
    QMetaObject::Connection m_connError;

    /* UI */
    QTextEdit*    m_descInput   = nullptr;
    QTextEdit*    m_modInput    = nullptr;
    QPushButton*  m_runBtn      = nullptr;
    QPushButton*  m_modBtn      = nullptr;
    QPushButton*  m_abortBtn    = nullptr;
    QLabel*       m_phaseLbl    = nullptr;
    QTextBrowser* m_codeView    = nullptr;
    QTextBrowser* m_outputView  = nullptr;
    QTextBrowser* m_anecdoteView = nullptr;
    QFrame*       m_resultArea  = nullptr;
    QFrame*       m_modArea     = nullptr;
};
