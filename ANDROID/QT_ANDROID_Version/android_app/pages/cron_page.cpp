#include "cron_page.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFont>
#include <QDateTime>
#include <QSettings>
#include <QScroller>
#include <QProcess>
#include <QUuid>

CronPage::CronPage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(10, 8, 10, 8);
    vbox->setSpacing(8);

    auto* title = new QLabel(
        QString::fromUtf8("\xe2\x8f\xb0") + " Cron Scheduler", this);
    {   QFont f = title->font(); f.setPointSize(14); f.setBold(true); title->setFont(f); }
    vbox->addWidget(title);

    /* Form aggiunta task */
    auto* form = new QFormLayout;
    form->setSpacing(6);
    m_edName = new QLineEdit(this);
    m_edName->setPlaceholderText("es: Recap giornaliero");
    form->addRow("Nome:", m_edName);

    m_spinMin = new QSpinBox(this);
    m_spinMin->setRange(1, 1440);
    m_spinMin->setValue(60);
    m_spinMin->setSuffix(" min");
    form->addRow("Ogni:", m_spinMin);

    m_cmbType = new QComboBox(this);
    m_cmbType->addItem("Chat AI", "chat");
    m_cmbType->addItem("Script Python", "python");
    m_cmbType->addItem("Comando shell", "shell");
    form->addRow("Tipo:", m_cmbType);

    m_edCmd = new QLineEdit(this);
    m_edCmd->setPlaceholderText("Prompt AI o codice da eseguire");
    form->addRow("Comando:", m_edCmd);
    vbox->addLayout(form);

    auto* btnRow = new QHBoxLayout;
    m_addBtn = new QPushButton(
        QString::fromUtf8("\xe2\x95\x8b") + " Aggiungi", this);
    m_addBtn->setObjectName("PrimaryBtn"); m_addBtn->setMinimumHeight(40);
    btnRow->addWidget(m_addBtn, 1);

    m_delBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x97\x91") + " Elimina", this);
    m_delBtn->setMinimumHeight(40);
    btnRow->addWidget(m_delBtn);

    m_runBtn = new QPushButton(
        QString::fromUtf8("\xe2\x96\xb6") + " Esegui ora", this);
    m_runBtn->setMinimumHeight(40);
    btnRow->addWidget(m_runBtn);

    m_togBtn = new QPushButton(
        QString::fromUtf8("\xe2\x8f\xba") + " Abilita/Dis.", this);
    m_togBtn->setMinimumHeight(40);
    btnRow->addWidget(m_togBtn);
    vbox->addLayout(btnRow);

    /* Lista task */
    m_list = new QListWidget(this);
    m_list->setObjectName("OutputEdit");
    m_list->setMaximumHeight(150);
    QScroller::grabGesture(m_list->viewport(), QScroller::TouchGesture);
    vbox->addWidget(m_list);

    /* Log */
    auto* logLabel = new QLabel("Log esecuzioni:", this);
    logLabel->setObjectName("StatusLabel");
    vbox->addWidget(logLabel);
    m_log = new QTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setObjectName("OutputEdit");
    {   QFont f = m_log->font(); f.setPointSize(10); m_log->setFont(f); }
    QScroller::grabGesture(m_log->viewport(), QScroller::TouchGesture);
    vbox->addWidget(m_log, 1);

    connect(m_addBtn, &QPushButton::clicked, this, &CronPage::onAddClicked);
    connect(m_delBtn, &QPushButton::clicked, this, &CronPage::onDeleteClicked);
    connect(m_runBtn, &QPushButton::clicked, this, &CronPage::onRunNowClicked);
    connect(m_togBtn, &QPushButton::clicked, this, &CronPage::onToggleClicked);
    connect(m_ai, &AiClient::token,    this, &CronPage::onToken);
    connect(m_ai, &AiClient::finished, this, &CronPage::onFinished);

    loadTasks();
    refreshList();
}

CronPage::~CronPage()
{
    for (auto& t : m_tasks)
        if (t.timer) { t.timer->stop(); delete t.timer; t.timer = nullptr; }
}

void CronPage::loadTasks()
{
    QSettings s("Prismalux", "CronMobile");
    const int n = s.beginReadArray("tasks");
    for (int i = 0; i < n; ++i) {
        s.setArrayIndex(i);
        CronTaskMob t;
        t.id          = s.value("id").toString();
        t.name        = s.value("name").toString();
        t.type        = s.value("type", "chat").toString();
        t.cmd         = s.value("cmd").toString();
        t.intervalMin = s.value("interval", 60).toInt();
        t.enabled     = s.value("enabled", true).toBool();
        if (!t.id.isEmpty() && !t.name.isEmpty()) {
            m_tasks.append(t);
            scheduleTask(m_tasks.last());
        }
    }
    s.endArray();
}

void CronPage::saveTasks()
{
    QSettings s("Prismalux", "CronMobile");
    s.beginWriteArray("tasks", m_tasks.size());
    for (int i = 0; i < m_tasks.size(); ++i) {
        s.setArrayIndex(i);
        s.setValue("id",       m_tasks[i].id);
        s.setValue("name",     m_tasks[i].name);
        s.setValue("type",     m_tasks[i].type);
        s.setValue("cmd",      m_tasks[i].cmd);
        s.setValue("interval", m_tasks[i].intervalMin);
        s.setValue("enabled",  m_tasks[i].enabled);
    }
    s.endArray();
}

void CronPage::scheduleTask(CronTaskMob& t)
{
    if (t.timer) { t.timer->stop(); delete t.timer; t.timer = nullptr; }
    if (!t.enabled) return;
    t.timer = new QTimer(this);
    t.timer->setInterval(t.intervalMin * 60 * 1000);
    t.timer->setSingleShot(false);
    connect(t.timer, &QTimer::timeout, this, [this, &t]{ runTask(t); });
    t.timer->start();
}

void CronPage::runTask(CronTaskMob& t)
{
    const QString now = QDateTime::currentDateTime().toString("hh:mm:ss");
    appendLog(now + " — " + t.name + " (" + t.type + ")...", true);
    if (t.type == "chat") {
        m_ai->chat("Esegui il task richiesto in modo conciso.", t.cmd.isEmpty() ? t.name : t.cmd);
    } else if (t.type == "python" || t.type == "shell") {
        auto* proc = new QProcess(this);
        const QStringList args = t.type == "python"
            ? QStringList{"-c", t.cmd} : QStringList{"-c", t.cmd};
        const QString prog = t.type == "python" ? "python3" : "sh";
        connect(proc, &QProcess::finished, this, [this, proc, &t](int code, QProcess::ExitStatus){
            const QString out = proc->readAllStandardOutput().trimmed();
            appendLog("  -> " + (out.isEmpty() ? "(nessun output)" : out.left(120)), code == 0);
            proc->deleteLater();
        });
        proc->start(prog, args);
    }
}

void CronPage::refreshList()
{
    m_list->clear();
    for (const auto& t : m_tasks) {
        const QString icon = t.enabled
            ? QString::fromUtf8("\xe2\x9c\x85") : QString::fromUtf8("\xe2\x8f\xb8");
        m_list->addItem(
            icon + " " + t.name +
            QString("  [ogni %1 min \xc2\xb7 %2]").arg(t.intervalMin).arg(t.type));
    }
}

void CronPage::appendLog(const QString& msg, bool ok)
{
    const QString color = ok ? "#22c55e" : "#ef4444";
    m_log->append(
        QString("<span style='color:%1'>%2</span>").arg(color, msg.toHtmlEscaped()));
}

void CronPage::onAddClicked()
{
    const QString name = m_edName->text().trimmed();
    if (name.isEmpty()) return;
    CronTaskMob t;
    t.id          = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    t.name        = name;
    t.type        = m_cmbType->currentData().toString();
    t.cmd         = m_edCmd->text().trimmed();
    t.intervalMin = m_spinMin->value();
    t.enabled     = true;
    m_tasks.append(t);
    scheduleTask(m_tasks.last());
    saveTasks();
    refreshList();
    m_edName->clear();
    m_edCmd->clear();
    appendLog("Task aggiunto: " + name, true);
}

void CronPage::onDeleteClicked()
{
    const int row = m_list->currentRow();
    if (row < 0 || row >= m_tasks.size()) return;
    if (m_tasks[row].timer) { m_tasks[row].timer->stop(); delete m_tasks[row].timer; }
    appendLog("Task rimosso: " + m_tasks[row].name, false);
    m_tasks.removeAt(row);
    saveTasks();
    refreshList();
}

void CronPage::onRunNowClicked()
{
    const int row = m_list->currentRow();
    if (row < 0 || row >= m_tasks.size()) return;
    runTask(m_tasks[row]);
}

void CronPage::onToggleClicked()
{
    const int row = m_list->currentRow();
    if (row < 0 || row >= m_tasks.size()) return;
    m_tasks[row].enabled = !m_tasks[row].enabled;
    scheduleTask(m_tasks[row]);
    saveTasks();
    refreshList();
    appendLog((m_tasks[row].enabled ? "Abilitato: " : "Disabilitato: ") + m_tasks[row].name,
              m_tasks[row].enabled);
}

void CronPage::onToken(const QString& t)
{
    m_log->moveCursor(QTextCursor::End);
    m_log->insertPlainText(t);
    m_log->moveCursor(QTextCursor::End);
}

void CronPage::onFinished(const QString&)
{
    appendLog("  -> Completato.", true);
}
