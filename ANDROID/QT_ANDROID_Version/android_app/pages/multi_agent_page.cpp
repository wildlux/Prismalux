#include "multi_agent_page.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QFrame>
#include <QFont>
#include <QListWidgetItem>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QScroller>
#include <QScrollerProperties>

static void applyTouchScroll(QScrollArea* sa)
{
    QScroller::grabGesture(sa->viewport(), QScroller::TouchGesture);
    QScroller* qs = QScroller::scroller(sa->viewport());
    QScrollerProperties sp = qs->scrollerProperties();
    sp.setScrollMetric(QScrollerProperties::OvershootDragDistanceFactor, 0.0);
    sp.setScrollMetric(QScrollerProperties::OvershootScrollDistanceFactor, 0.0);
    sp.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy,
        QVariant::fromValue<QScrollerProperties::OvershootPolicy>(
            QScrollerProperties::OvershootAlwaysOff));
    sp.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy,
        QVariant::fromValue<QScrollerProperties::OvershootPolicy>(
            QScrollerProperties::OvershootAlwaysOff));
    qs->setScrollerProperties(sp);
}

static const char* kMasterPrompt =
    "Sei un MasterAgent. Ricevi un obiettivo e devi decomporlo in sotto-task.\n"
    "Rispondi SOLO con un JSON valido in questo formato esatto:\n"
    "{\n"
    "  \"task\": \"<descrizione obiettivo>\",\n"
    "  \"subtasks\": [\n"
    "    {\"id\": 1, \"role\": \"<nome agente>\", \"prompt\": \"<istruzione>\", \"depends_on\": []},\n"
    "    {\"id\": 2, \"role\": \"<nome agente>\", \"prompt\": \"<istruzione>\", \"depends_on\": [1]}\n"
    "  ]\n"
    "}\n"
    "Regole:\n"
    "- Massimo 5 sotto-task\n"
    "- depends_on contiene gli id dei task che devono completarsi prima\n"
    "- I prompt devono essere in italiano e specifici\n"
    "- NESSUN testo fuori dal JSON, nessun markdown\n"
    "- Il JSON deve essere valido e parseable";

/* ══════════════════════════════════════════════════════════════
   Costruttore
   ══════════════════════════════════════════════════════════════ */
MultiAgentPage::MultiAgentPage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    setObjectName("MultiAgentPage");

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    applyTouchScroll(scroll);

    auto* inner = new QWidget;
    auto* vbox  = new QVBoxLayout(inner);
    vbox->setContentsMargins(8, 8, 8, 8);
    vbox->setSpacing(10);
    scroll->setWidget(inner);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(scroll);

    /* ── Titolo ── */
    auto* title = new QLabel(
        QString::fromUtf8("\xf0\x9f\x95\xb8\xef\xb8\x8f  Multi-Agente"), inner);
    QFont tf = title->font();
    tf.setPointSize(15); tf.setBold(true);
    title->setFont(tf);
    title->setAlignment(Qt::AlignCenter);
    vbox->addWidget(title);

    auto* desc = new QLabel(
        "L'AI scompone l'obiettivo in sotto-task e li esegue sequenzialmente.", inner);
    desc->setWordWrap(true);
    desc->setStyleSheet("color:#8890a8; font-size:12px;");
    desc->setAlignment(Qt::AlignCenter);
    vbox->addWidget(desc);

    /* ── Obiettivo ── */
    auto* goalGroup = new QGroupBox(
        QString::fromUtf8("\xf0\x9f\x8e\xaf  Obiettivo"), inner);
    goalGroup->setObjectName("SettingsGroup");
    auto* goalVbox = new QVBoxLayout(goalGroup);

    m_goalEdit = new QLineEdit(inner);
    m_goalEdit->setPlaceholderText(
        "es. \"Scrivi un articolo sul machine learning e crea un quiz\"");
    m_goalEdit->setMinimumHeight(48);
    {
        QFont f = m_goalEdit->font();
        f.setPointSize(13);
        m_goalEdit->setFont(f);
    }
    goalVbox->addWidget(m_goalEdit);
    vbox->addWidget(goalGroup);

    /* ── Bottoni Piano + Esegui + Stop ── */
    auto* btnRow = new QHBoxLayout;
    m_planBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x97\xba\xef\xb8\x8f  Genera Piano"), inner);  /* 🗺️ */
    m_planBtn->setObjectName("PrimaryBtn");
    m_planBtn->setMinimumHeight(52);
    {
        QFont f = m_planBtn->font();
        f.setBold(true); f.setPointSize(13);
        m_planBtn->setFont(f);
    }
    m_execBtn = new QPushButton(
        QString::fromUtf8("\xe2\x96\xb6\xef\xb8\x8f  Esegui"), inner);  /* ▶️ */
    m_execBtn->setObjectName("SecondaryBtn");
    m_execBtn->setMinimumHeight(52);
    m_execBtn->setEnabled(false);

    m_stopBtn = new QPushButton(
        QString::fromUtf8("\xe2\x8f\xb9  Stop"), inner);
    m_stopBtn->setObjectName("StopBtn");
    m_stopBtn->setMinimumHeight(52);
    m_stopBtn->setEnabled(false);

    btnRow->addWidget(m_planBtn,  3);
    btnRow->addWidget(m_execBtn,  2);
    btnRow->addWidget(m_stopBtn,  1);
    vbox->addLayout(btnRow);

    /* ── Progress + status ── */
    m_progressBar = new QProgressBar(inner);
    m_progressBar->setRange(0, 100);
    m_progressBar->setFixedHeight(6);
    m_progressBar->setVisible(false);
    vbox->addWidget(m_progressBar);

    m_statusLbl = new QLabel("", inner);
    m_statusLbl->setStyleSheet("color:#8890a8; font-size:12px;");
    m_statusLbl->setVisible(false);
    vbox->addWidget(m_statusLbl);

    /* ── Lista task ── */
    auto* taskGroup = new QGroupBox(
        QString::fromUtf8("\xf0\x9f\xa4\x96  Sotto-task"), inner);
    taskGroup->setObjectName("SettingsGroup");
    auto* taskVbox = new QVBoxLayout(taskGroup);

    m_taskList = new QListWidget(inner);
    m_taskList->setFixedHeight(180);
    m_taskList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_taskList->addItem("(nessun piano generato)");
    taskVbox->addWidget(m_taskList);
    vbox->addWidget(taskGroup);

    /* ── JSON piano (collassato) ── */
    auto* jsonGroup = new QGroupBox(
        QString::fromUtf8("\xf0\x9f\x93\x8b  Piano JSON"), inner);
    jsonGroup->setObjectName("SettingsGroup");
    auto* jsonVbox = new QVBoxLayout(jsonGroup);

    m_planJson = new QTextEdit(inner);
    m_planJson->setReadOnly(true);
    m_planJson->setMaximumHeight(120);
    m_planJson->setPlaceholderText("Il JSON del piano apparirà qui.");
    jsonVbox->addWidget(m_planJson);
    vbox->addWidget(jsonGroup);

    /* ── Output risultati ── */
    auto* outGroup = new QGroupBox(
        QString::fromUtf8("\xf0\x9f\x93\x84  Risultati"), inner);
    outGroup->setObjectName("SettingsGroup");
    auto* outVbox = new QVBoxLayout(outGroup);

    m_output = new QTextEdit(inner);
    m_output->setReadOnly(true);
    m_output->setMinimumHeight(200);
    m_output->setPlaceholderText(
        "I risultati dei sotto-task appariranno qui.\n\n"
        "1. Inserisci l'obiettivo\n"
        "2. Premi \"Genera Piano\" per scomporre in task\n"
        "3. Premi \"Esegui\" per avviare la pipeline");
    outVbox->addWidget(m_output);
    vbox->addWidget(outGroup, 1);

    vbox->addStretch();

    /* ── Connessioni ── */
    connect(m_planBtn, &QPushButton::clicked, this, &MultiAgentPage::onPlanClicked);
    connect(m_execBtn, &QPushButton::clicked, this, &MultiAgentPage::onExecuteClicked);
    connect(m_stopBtn, &QPushButton::clicked, this, &MultiAgentPage::onStopClicked);
    connect(m_goalEdit, &QLineEdit::returnPressed, this, &MultiAgentPage::onPlanClicked);
}

/* ══════════════════════════════════════════════════════════════
   onPlanClicked — chiede al MasterAgent di scomporre l'obiettivo
   ══════════════════════════════════════════════════════════════ */
void MultiAgentPage::onPlanClicked()
{
    const QString goal = m_goalEdit->text().trimmed();
    if (goal.isEmpty() || m_busy) return;

    m_busy = true;
    m_planBuffer.clear();
    m_tasks.clear();
    m_planJson->clear();
    m_output->clear();
    m_taskList->clear();
    m_taskList->addItem(
        QString::fromUtf8("\xf0\x9f\x94\x84")   /* 🔄 */
        + "  Generazione piano in corso...");

    m_planBtn->setEnabled(false);
    m_execBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);
    m_progressBar->setRange(0, 0);
    m_progressBar->setVisible(true);
    m_statusLbl->setText(
        QString::fromUtf8("\xf0\x9f\x97\xba\xef\xb8\x8f")   /* 🗺️ */
        + "  MasterAgent sta pianificando...");
    m_statusLbl->setVisible(true);

    m_tokConn = connect(m_ai, &AiClient::token, this, &MultiAgentPage::onPlanToken);
    m_finConn = connect(m_ai, &AiClient::finished, this, &MultiAgentPage::onPlanFinished);
    m_errConn = connect(m_ai, &AiClient::error, this, &MultiAgentPage::onPlanError);

    m_ai->chat(kMasterPrompt, "Obiettivo: " + goal);
}

void MultiAgentPage::onPlanToken(const QString& t) { m_planBuffer += t; }

void MultiAgentPage::onPlanFinished(const QString& full)
{
    disconnect(m_tokConn);
    disconnect(m_finConn);
    disconnect(m_errConn);
    m_busy = false;

    const QString json = full.isEmpty() ? m_planBuffer : full;
    m_planJson->setPlainText(json);

    if (parsePlan(json)) {
        updateTaskList();
        m_execBtn->setEnabled(true);
        m_statusLbl->setText(
            QString::fromUtf8("\xe2\x9c\x85")
            + QString("  Piano generato: %1 sotto-task. Premi Esegui.").arg(m_tasks.size()));
    } else {
        m_statusLbl->setText(
            QString::fromUtf8("\xe2\x9d\x8c")
            + "  Il piano JSON non è valido. Riprova o modifica l'obiettivo.");
        m_taskList->clear();
        m_taskList->addItem("Errore nel parsing del piano JSON.");
    }

    m_planBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
    m_progressBar->setRange(0, 100);
    m_progressBar->setVisible(false);
}

void MultiAgentPage::onPlanError(const QString& msg)
{
    disconnect(m_tokConn);
    disconnect(m_finConn);
    disconnect(m_errConn);
    m_busy = false;
    m_planBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
    m_progressBar->setVisible(false);
    m_statusLbl->setText(
        QString::fromUtf8("\xe2\x9d\x8c") + "  Errore MasterAgent: " + msg);
}

/* ══════════════════════════════════════════════════════════════
   parsePlan — parse JSON → m_tasks
   ══════════════════════════════════════════════════════════════ */
bool MultiAgentPage::parsePlan(const QString& jsonStr)
{
    /* Cerca il primo '{' e l'ultimo '}' — l'LLM a volte aggiunge testo prima/dopo */
    const int start = jsonStr.indexOf('{');
    const int end   = jsonStr.lastIndexOf('}');
    if (start < 0 || end <= start) return false;

    const QString clean = jsonStr.mid(start, end - start + 1);
    const QJsonDocument doc = QJsonDocument::fromJson(clean.toUtf8());
    if (doc.isNull() || !doc.isObject()) return false;

    const QJsonArray subtasks = doc.object().value("subtasks").toArray();
    if (subtasks.isEmpty()) return false;

    m_tasks.clear();
    for (const QJsonValue& v : subtasks) {
        const QJsonObject obj = v.toObject();
        MobileSubTask t;
        t.id     = obj.value("id").toInt();
        t.role   = obj.value("role").toString();
        t.prompt = obj.value("prompt").toString();
        for (const QJsonValue& d : obj.value("depends_on").toArray())
            t.dependsOn << d.toInt();
        m_tasks << t;
    }
    return !m_tasks.isEmpty();
}

/* ══════════════════════════════════════════════════════════════
   updateTaskList — aggiorna la QListWidget con lo stato attuale
   ══════════════════════════════════════════════════════════════ */
void MultiAgentPage::updateTaskList()
{
    m_taskList->clear();
    for (const MobileSubTask& t : m_tasks) {
        const char* icon =
            (t.state == MobileSubTask::Done)    ? "\xe2\x9c\x85"  /* ✅ */
          : (t.state == MobileSubTask::Running) ? "\xf0\x9f\x94\x84"  /* 🔄 */
          : (t.state == MobileSubTask::Failed)  ? "\xe2\x9d\x8c"  /* ❌ */
                                                : "\xe2\xae\x9f";  /* ⬟ pending */

        const QString label = QString::fromUtf8(icon)
            + QString("  [%1] %2: %3").arg(t.id).arg(t.role).arg(t.prompt.left(50));
        auto* item = new QListWidgetItem(label, m_taskList);
        item->setSizeHint(QSize(0, 48));
        Q_UNUSED(item);
    }
}

/* ══════════════════════════════════════════════════════════════
   onExecuteClicked — avvia esecuzione sequenziale BFS
   ══════════════════════════════════════════════════════════════ */
void MultiAgentPage::onExecuteClicked()
{
    if (m_tasks.isEmpty() || m_busy) return;

    m_busy = true;
    m_currentTask = 0;
    m_output->clear();

    /* Reset stati */
    for (auto& t : m_tasks) t.state = MobileSubTask::Pending;
    updateTaskList();

    m_execBtn->setEnabled(false);
    m_planBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);
    m_progressBar->setRange(0, m_tasks.size());
    m_progressBar->setValue(0);
    m_progressBar->setVisible(true);

    runNextTask();
}

/* ══════════════════════════════════════════════════════════════
   runNextTask — avvia il prossimo task (BFS semplice: ordine lineare)
   ══════════════════════════════════════════════════════════════ */
void MultiAgentPage::runNextTask()
{
    /* Cerca il prossimo task con dipendenze soddisfatte */
    while (m_currentTask < m_tasks.size()) {
        const MobileSubTask& t = m_tasks[m_currentTask];
        if (t.state != MobileSubTask::Pending) { ++m_currentTask; continue; }

        /* Verifica dipendenze */
        bool depsOk = true;
        for (int dep : t.dependsOn) {
            bool found = false;
            for (const MobileSubTask& other : m_tasks) {
                if (other.id == dep && other.state == MobileSubTask::Done)
                    { found = true; break; }
            }
            if (!found) { depsOk = false; break; }
        }
        if (!depsOk) { ++m_currentTask; continue; }
        break;
    }

    if (m_currentTask >= m_tasks.size()) {
        /* Pipeline completata */
        m_busy = false;
        m_planBtn->setEnabled(true);
        m_execBtn->setEnabled(true);
        m_stopBtn->setEnabled(false);
        m_progressBar->setValue(m_tasks.size());
        m_statusLbl->setText(
            QString::fromUtf8("\xf0\x9f\x8e\x89")   /* 🎉 */
            + "  Pipeline completata!");
        m_output->append(
            "\n---\n**Pipeline completata** — "
            + QString::number(m_tasks.size()) + " sotto-task eseguiti.");
        return;
    }

    MobileSubTask& t = m_tasks[m_currentTask];
    t.state = MobileSubTask::Running;
    updateTaskList();

    m_statusLbl->setText(
        QString::fromUtf8("\xf0\x9f\x94\x84")
        + QString("  Task %1/%2: %3 (%4)…")
            .arg(m_currentTask + 1).arg(m_tasks.size())
            .arg(t.id).arg(t.role));
    m_progressBar->setValue(m_currentTask);

    /* Costruisce contesto dai task precedenti completati */
    QString context;
    for (int dep : t.dependsOn) {
        for (const MobileSubTask& prev : m_tasks) {
            if (prev.id == dep && prev.state == MobileSubTask::Done && !prev.result.isEmpty())
                context += QString("\n\n[Risultato task %1 — %2]:\n%3")
                               .arg(dep).arg(prev.role).arg(prev.result.left(1000));
        }
    }

    m_taskBuffer.clear();
    const QString sys =
        QString("Sei un agente specializzato con il ruolo: %1.\n"
                "Esegui l'istruzione seguente. Sii preciso e conciso. Rispondi in italiano.")
            .arg(t.role);
    const QString prompt = t.prompt + context;

    m_tokConn = connect(m_ai, &AiClient::token, this, &MultiAgentPage::onTaskToken);
    m_finConn = connect(m_ai, &AiClient::finished, this, &MultiAgentPage::onTaskFinished);
    m_errConn = connect(m_ai, &AiClient::error, this, &MultiAgentPage::onTaskError);

    m_ai->chat(sys, prompt);
}

/* ── Token streaming ── */
void MultiAgentPage::onTaskToken(const QString& t) { m_taskBuffer += t; }

/* ── Task completato ── */
void MultiAgentPage::onTaskFinished(const QString& full)
{
    disconnect(m_tokConn);
    disconnect(m_finConn);
    disconnect(m_errConn);

    const QString result = full.isEmpty() ? m_taskBuffer : full;
    if (m_currentTask < m_tasks.size()) {
        m_tasks[m_currentTask].result = result;
        m_tasks[m_currentTask].state  = MobileSubTask::Done;
        appendOutput(
            QString("## Task %1 — %2\n\n%3")
                .arg(m_tasks[m_currentTask].id)
                .arg(m_tasks[m_currentTask].role)
                .arg(result));
    }

    m_progressBar->setValue(m_currentTask + 1);
    ++m_currentTask;
    updateTaskList();
    runNextTask();
}

/* ── Errore task ── */
void MultiAgentPage::onTaskError(const QString& msg)
{
    disconnect(m_tokConn);
    disconnect(m_finConn);
    disconnect(m_errConn);

    if (m_currentTask < m_tasks.size()) {
        m_tasks[m_currentTask].state = MobileSubTask::Failed;
        appendOutput(
            QString("## ❌ Task %1 — %2 (errore)\n\n%3")
                .arg(m_tasks[m_currentTask].id)
                .arg(m_tasks[m_currentTask].role)
                .arg(msg));
    }

    ++m_currentTask;
    updateTaskList();
    runNextTask();
}

/* ── Stop ── */
void MultiAgentPage::onStopClicked()
{
    m_ai->abort();
    disconnect(m_tokConn);
    disconnect(m_finConn);
    disconnect(m_errConn);
    m_busy = false;
    m_planBtn->setEnabled(true);
    m_execBtn->setEnabled(!m_tasks.isEmpty());
    m_stopBtn->setEnabled(false);
    m_progressBar->setVisible(false);
    m_statusLbl->setText(
        QString::fromUtf8("\xe2\x9b\x94")   /* ⛔ */
        + "  Esecuzione interrotta.");
}

void MultiAgentPage::appendOutput(const QString& text)
{
    m_output->append("\n---\n" + text + "\n");
}
