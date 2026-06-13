#pragma once
#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QListWidget>
#include <QJsonArray>
#include "../ai_client.h"

/* ══════════════════════════════════════════════════════════════
   MultiAgentPage mobile — MasterAgent + SubTask sequenziali.

   Flusso:
     1. Utente inserisce obiettivo
     2. MasterAgent → JSON {"task":"...","subtasks":[{id,role,prompt,depends_on:[]}]}
     3. Esecuzione sequenziale BFS su depends_on
     4. Risultati aggregati in area output
   ══════════════════════════════════════════════════════════════ */
struct MobileSubTask {
    int     id        = 0;
    QString role;
    QString prompt;
    QList<int> dependsOn;
    QString result;
    enum State { Pending, Running, Done, Failed } state = Pending;
};

class MultiAgentPage : public QWidget {
    Q_OBJECT
public:
    explicit MultiAgentPage(AiClient* ai, QWidget* parent = nullptr);

private slots:
    void onPlanClicked();
    void onExecuteClicked();
    void onStopClicked();
    void onPlanToken(const QString& t);
    void onPlanFinished(const QString& full);
    void onPlanError(const QString& msg);
    void onTaskToken(const QString& t);
    void onTaskFinished(const QString& full);
    void onTaskError(const QString& msg);
    void runNextTask();

private:
    bool parsePlan(const QString& json);
    void updateTaskList();
    void appendOutput(const QString& text);

    AiClient*     m_ai         = nullptr;
    QLineEdit*    m_goalEdit   = nullptr;
    QPushButton*  m_planBtn    = nullptr;
    QPushButton*  m_execBtn    = nullptr;
    QPushButton*  m_stopBtn    = nullptr;
    QProgressBar* m_progressBar= nullptr;
    QLabel*       m_statusLbl  = nullptr;
    QListWidget*  m_taskList   = nullptr;
    QTextEdit*    m_planJson   = nullptr;
    QTextEdit*    m_output     = nullptr;

    QList<MobileSubTask> m_tasks;
    int     m_currentTask = 0;
    bool    m_busy        = false;
    QString m_planBuffer;
    QString m_taskBuffer;

    QMetaObject::Connection m_tokConn;
    QMetaObject::Connection m_finConn;
    QMetaObject::Connection m_errConn;
};
