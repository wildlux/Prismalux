#pragma once
#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QTextEdit>
#include <QTimer>
#include "../ai_client.h"

struct CronTaskMob {
    QString id, name, type, cmd;
    int intervalMin = 60;
    bool enabled    = true;
    QTimer* timer   = nullptr;
};

/* Cron Scheduler mobile — task periodici con chat AI / REPL Python. */
class CronPage : public QWidget {
    Q_OBJECT
public:
    explicit CronPage(AiClient* ai, QWidget* parent = nullptr);
    ~CronPage() override;

private slots:
    void onAddClicked();
    void onDeleteClicked();
    void onRunNowClicked();
    void onToggleClicked();
    void onToken(const QString& t);
    void onFinished(const QString& full);

private:
    void loadTasks();
    void saveTasks();
    void scheduleTask(CronTaskMob& t);
    void runTask(CronTaskMob& t);
    void refreshList();
    void appendLog(const QString& msg, bool ok = true);

    AiClient*    m_ai       = nullptr;
    QListWidget* m_list     = nullptr;
    QLineEdit*   m_edName   = nullptr;
    QSpinBox*    m_spinMin  = nullptr;
    QComboBox*   m_cmbType  = nullptr;
    QLineEdit*   m_edCmd    = nullptr;
    QPushButton* m_addBtn   = nullptr;
    QPushButton* m_delBtn   = nullptr;
    QPushButton* m_runBtn   = nullptr;
    QPushButton* m_togBtn   = nullptr;
    QTextEdit*   m_log      = nullptr;
    QList<CronTaskMob> m_tasks;
};
