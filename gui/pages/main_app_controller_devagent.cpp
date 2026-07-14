/* ══════════════════════════════════════════════════════════════
   main_app_controller_devagent.cpp — AppControllerPage: Dev Agent
   ==================================================================
   Dev Agent LangGraph locale — modifica il codice di Prismalux in
   autonomia (read → generate patch → apply → cmake → fix → done).
   Builder + slot + cronologia/git. Split da
   main_app_controller.cpp/main_app_controller_slots.cpp (TODO D-8).
   ══════════════════════════════════════════════════════════════ */
#include "main_app_controller.h"
#include "../prismalux_paths.h"
#include "../log_bus.h"
#include "../dpi_utils.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QListWidget>
#include <QListWidgetItem>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QTextBrowser>
#include <QTextEdit>
#include <QFrame>
#include <QFile>
#include <QMessageBox>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>

namespace P = PrismaluxPaths;

/* ══════════════════════════════════════════════════════════════
   buildDevAgentTab — Dev Agent LangGraph locale
   Modifica il codice di Prismalux in autonomia:
   read → generate patch → apply → cmake → fix errori → done
   ══════════════════════════════════════════════════════════════ */
QWidget* AppControllerPage::buildDevAgentTab()
{
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(dpiScale(12), dpiScale(12),
                            dpiScale(12), dpiScale(12));
    lay->setSpacing(dpiScale(8));

    /* ── Intestazione ── */
    auto* descLbl = new QLabel(
        "\xf0\x9f\xa4\x96  <b>Dev Agent LangGraph</b> \xe2\x80\x94 "
        "<i>Descrivi un task in linguaggio naturale: l\xe2\x80\x99"
        "agente legge i file, genera una patch, compila e si auto-corregge.</i>",
        w);
    descLbl->setObjectName("hintLabel");
    descLbl->setTextFormat(Qt::RichText);
    descLbl->setWordWrap(true);
    lay->addWidget(descLbl);

    /* ── Configurazione ── */
    auto* cfgGroup = new QGroupBox(tr("\xf0\x9f\x94\xa7  Configurazione"), w);
    auto* cfgLay   = new QVBoxLayout(cfgGroup);
    cfgLay->setSpacing(dpiScale(6));

    /* Task input */
    auto* taskRow = new QHBoxLayout;
    auto* taskLbl = new QLabel(tr("Task:"), cfgGroup);
    taskLbl->setFixedWidth(dpiScale(55));
    m_devTaskEdit = new QLineEdit(cfgGroup);
    m_devTaskEdit->setPlaceholderText(
        "Es: Aggiungi tooltip al pulsante PDF in main_ai_ui.cpp riga 138");
    taskRow->addWidget(taskLbl);
    taskRow->addWidget(m_devTaskEdit, 1);
    cfgLay->addLayout(taskRow);

    /* Modello */
    auto* modelRow = new QHBoxLayout;
    auto* modelLbl = new QLabel(tr("Modello:"), cfgGroup);
    modelLbl->setFixedWidth(dpiScale(55));
    m_devModelCombo = new QComboBox(cfgGroup);
    m_devModelCombo->setObjectName("settingCombo");
    m_devModelCombo->addItem("\xf0\x9f\x90\x8d  deepseek-coder:6.7b  (3.8 GB \xe2\x80\x94 gi\xc3\xa0 installato)",
                              "deepseek-coder:6.7b");
    m_devModelCombo->addItem("\xf0\x9f\x90\xa3  qwen2.5-coder:3b  (2 GB \xe2\x80\x94 pi\xc3\xb9 leggero)",
                              "qwen2.5-coder:3b");
    m_devModelCombo->addItem("\xf0\x9f\x90\xa3  qwen2.5-coder:1.5b  (1 GB \xe2\x80\x94 ultra-leggero)",
                              "qwen2.5-coder:1.5b");
    modelRow->addWidget(modelLbl);
    modelRow->addWidget(m_devModelCombo, 1);
    cfgLay->addLayout(modelRow);

    /* Pulsanti controllo */
    auto* ctrlRow = new QHBoxLayout;
    m_devRunBtn = new QPushButton(
        "\xf0\x9f\x9a\x80  Avvia Dev Agent", cfgGroup);
    m_devRunBtn->setObjectName("primaryBtn");
    m_devRunBtn->setFixedHeight(dpiScale(36));

    m_devStopBtn = new QPushButton(
        "\xe2\x8f\xb9  Ferma", cfgGroup);
    m_devStopBtn->setObjectName("actionBtn");
    m_devStopBtn->setFixedHeight(dpiScale(36));
    m_devStopBtn->setEnabled(false);

    m_devInstallBtn = new QPushButton(
        "\xf0\x9f\x93\xa6  Installa LangGraph", cfgGroup);
    m_devInstallBtn->setObjectName("actionBtn");
    m_devInstallBtn->setFixedHeight(dpiScale(36));
    m_devInstallBtn->setToolTip(
        "pip install langgraph langchain-community langchain-ollama unidiff");

    m_devStatusLbl = new QLabel(tr("\xe2\x9a\xab  Pronto"), cfgGroup);
    m_devStatusLbl->setObjectName("statusLabel");

    ctrlRow->addWidget(m_devRunBtn);
    ctrlRow->addWidget(m_devStopBtn);
    ctrlRow->addWidget(m_devInstallBtn);
    ctrlRow->addWidget(m_devStatusLbl, 1);
    cfgLay->addLayout(ctrlRow);

    lay->addWidget(cfgGroup);

    /* ── Log step-by-step ── */
    auto* logGroup = new QGroupBox(
        "\xf0\x9f\x93\x8b  Log agente  (Read \xe2\x86\x92 Generate \xe2\x86\x92 Compile \xe2\x86\x92 Fix \xe2\x86\x92 Done)", w);
    auto* logLay = new QVBoxLayout(logGroup);
    m_devLog = new QTextBrowser(logGroup);
    m_devLog->setReadOnly(true);
    m_devLog->setOpenLinks(false);
    m_devLog->setMinimumHeight(dpiScale(140));
    m_devLog->setPlaceholderText(tr("I passi dell'agente appariranno qui..."));
    m_devLog->setFont(QFont("JetBrains Mono,Fira Code,Consolas", 9));
    connect(m_devLog, &QTextBrowser::anchorClicked,
            this, &AppControllerPage::onPipLinkClicked);
    logLay->addWidget(m_devLog);
    /* ── Splitter orizzontale: Log+Diff | Cronologia+Git ── */
    auto* splitter = new QSplitter(Qt::Horizontal, w);
    splitter->setChildrenCollapsible(false);

    /* ── Colonna sinistra: Log agente + Diff ── */
    auto* leftCol    = new QWidget(splitter);
    auto* leftColLay = new QVBoxLayout(leftCol);
    leftColLay->setContentsMargins(0, 0, 0, 0);
    leftColLay->setSpacing(dpiScale(6));

    leftColLay->addWidget(logGroup, 2);

    /* ── Diff finale ── */
    auto* diffGroup = new QGroupBox(
        "\xf0\x9f\x93\x9d  Diff generato", leftCol);
    auto* diffLay = new QVBoxLayout(diffGroup);
    m_devDiff = new QTextEdit(diffGroup);
    m_devDiff->setReadOnly(true);
    m_devDiff->setMinimumHeight(dpiScale(100));
    m_devDiff->setPlaceholderText(tr("Il diff unificato apparirà qui al termine..."));
    m_devDiff->setFont(QFont("JetBrains Mono,Fira Code,Consolas", 9));
    diffLay->addWidget(m_devDiff);
    leftColLay->addWidget(diffGroup, 1);

    splitter->addWidget(leftCol);

    /* ── Colonna destra: Cronologia + Git (con scroll) ── */
    auto* rightSc = new QScrollArea(splitter);
    rightSc->setWidgetResizable(true);
    rightSc->setFrameShape(QFrame::NoFrame);
    rightSc->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    rightSc->setMinimumWidth(dpiScale(260));
    rightSc->setMaximumWidth(dpiScale(400));

    auto* rightCol    = new QWidget;
    auto* rightColLay = new QVBoxLayout(rightCol);
    rightColLay->setContentsMargins(dpiScale(4), 0, 0, 0);
    rightColLay->setSpacing(dpiScale(8));

    /* ── Cronologia snapshot ── */
    auto* histGroup = new QGroupBox(
        "\xe2\x8f\xaa  Cronologia \xe2\x80\x94 torna indietro nel tempo", rightCol);
    auto* histLay = new QVBoxLayout(histGroup);
    histLay->setSpacing(dpiScale(4));

    auto* histHint = new QLabel(
        "<i>Backup in <code>~/.prismalux/devagent_history/</code>.<br>"
        "Seleziona uno snapshot e clicca Ripristina.</i>", histGroup);
    histHint->setObjectName("hintLabel");
    histHint->setTextFormat(Qt::RichText);
    histHint->setWordWrap(true);
    histLay->addWidget(histHint);

    m_devHistoryList = new QListWidget(histGroup);
    m_devHistoryList->setFixedHeight(dpiScale(110));
    m_devHistoryList->setToolTip(tr("Seleziona uno snapshot da ripristinare"));
    histLay->addWidget(m_devHistoryList);

    auto* histCtrlRow = new QHBoxLayout;
    m_devRestoreBtn = new QPushButton(
        "\xe2\x86\xa9  Ripristina snapshot", histGroup);
    m_devRestoreBtn->setObjectName("actionBtn");
    m_devRestoreBtn->setEnabled(false);
    m_devRestoreBtn->setToolTip(
        "Copia i file originali dallo snapshot selezionato\n"
        "sovrascrivendo le versioni modificate dal Dev Agent.");
    auto* histRefreshBtn = new QPushButton("\xf0\x9f\x94\x84", histGroup);
    histRefreshBtn->setObjectName("actionBtn");
    histRefreshBtn->setFixedWidth(dpiScale(32));
    histRefreshBtn->setToolTip(tr("Aggiorna la lista degli snapshot salvati dal Dev Agent"));
    histCtrlRow->addWidget(m_devRestoreBtn, 1);
    histCtrlRow->addWidget(histRefreshBtn);
    histLay->addLayout(histCtrlRow);
    rightColLay->addWidget(histGroup);

    /* ── Ripristina da Git / GitHub ── */
    auto* gitGroup = new QGroupBox(
        "\xf0\x9f\x90\x99  Ripristina da Git / GitHub", rightCol);
    auto* gitLay = new QVBoxLayout(gitGroup);
    gitLay->setSpacing(dpiScale(4));

    /* Branch + Fetch+Reset */
    auto* fetchRow = new QHBoxLayout;
    auto* branchLbl = new QLabel(tr("Branch:"), gitGroup);
    branchLbl->setFixedWidth(dpiScale(55));
    m_devGitBranchEdit = new QLineEdit(gitGroup);
    m_devGitBranchEdit->setText(tr("master"));
    m_devGitBranchEdit->setFixedWidth(dpiScale(90));
    auto* fetchResetBtn = new QPushButton(
        "\xf0\x9f\x8c\x90  Fetch + Reset da GitHub", gitGroup);
    fetchResetBtn->setObjectName("actionBtn");
    fetchResetBtn->setToolTip(
        "git fetch origin && git reset --hard origin/master\n"
        "Scarica l'ultimo commit da GitHub e sovrascrive il worktree locale.\n"
        "\xe2\x9a\xa0  Annulla TUTTE le modifiche locali non committate.");
    fetchRow->addWidget(branchLbl);
    fetchRow->addWidget(m_devGitBranchEdit);
    fetchRow->addWidget(fetchResetBtn, 1);
    gitLay->addLayout(fetchRow);

    /* Log commit */
    auto* gitLogRow = new QHBoxLayout;
    auto* gitLogLbl = new QLabel(tr("Commit:"), gitGroup);
    gitLogLbl->setFixedWidth(dpiScale(55));
    m_devGitLogList = new QListWidget(gitGroup);
    m_devGitLogList->setFixedHeight(dpiScale(90));
    m_devGitLogList->setToolTip(
        "Seleziona un commit e clicca \"Ripristina al commit\" per\n"
        "riportare i file modificati dal Dev Agent a quello stato.");
    auto* gitLogRefresh = new QPushButton("\xf0\x9f\x94\x84", gitGroup);
    gitLogRefresh->setObjectName("actionBtn");
    gitLogRefresh->setFixedWidth(dpiScale(32));
    gitLogRefresh->setToolTip(tr("Aggiorna log git"));
    gitLogRow->addWidget(gitLogLbl, 0, Qt::AlignTop);
    gitLogRow->addWidget(m_devGitLogList, 1);
    gitLogRow->addWidget(gitLogRefresh, 0, Qt::AlignTop);
    gitLay->addLayout(gitLogRow);

    m_devGitRestoreBtn = new QPushButton(
        "\xe2\x86\xa9  Ripristina file al commit selezionato", gitGroup);
    m_devGitRestoreBtn->setObjectName("actionBtn");
    m_devGitRestoreBtn->setEnabled(false);
    m_devGitRestoreBtn->setToolTip(
        "git checkout {commit} -- {file1} {file2} ...\n"
        "Ripristina solo i file toccati dall'ultimo Dev Agent run.");
    gitLay->addWidget(m_devGitRestoreBtn);

    /* Stash */
    auto* stashRow = new QHBoxLayout;
    auto* stashPushBtn = new QPushButton(
        "\xf0\x9f\x93\xa6  Stash modifiche", gitGroup);
    stashPushBtn->setObjectName("actionBtn");
    stashPushBtn->setToolTip(tr("git stash push — salva le modifiche correnti in uno stash"));

    m_devStashList = new QListWidget(gitGroup);
    m_devStashList->setFixedHeight(dpiScale(60));
    m_devStashList->setToolTip(tr("Lista degli stash git disponibili"));

    m_devGitStashPopBtn = new QPushButton(
        "\xf0\x9f\x93\xa4  Applica stash selezionato", gitGroup);
    m_devGitStashPopBtn->setObjectName("actionBtn");
    m_devGitStashPopBtn->setEnabled(false);

    auto* stashListRefresh = new QPushButton("\xf0\x9f\x94\x84", gitGroup);
    stashListRefresh->setObjectName("actionBtn");
    stashListRefresh->setFixedWidth(dpiScale(32));
    stashListRefresh->setToolTip(tr("Aggiorna lista stash"));

    stashRow->addWidget(stashPushBtn);
    stashRow->addWidget(m_devGitStashPopBtn);
    stashRow->addWidget(stashListRefresh);
    gitLay->addLayout(stashRow);
    gitLay->addWidget(m_devStashList);

    rightColLay->addWidget(gitGroup);
    rightColLay->addStretch(1);
    rightSc->setWidget(rightCol);
    splitter->addWidget(rightSc);

    /* Pesi splitter: colonna sinistra 65%, destra 35% */
    splitter->setStretchFactor(0, 65);
    splitter->setStretchFactor(1, 35);
    lay->addWidget(splitter, 1);

    /* ── Connessioni (unico blocco — nessun duplicato) ── */
    connect(m_devRunBtn,     &QPushButton::clicked,
            this, &AppControllerPage::onDevAgentRunClicked);
    connect(m_devStopBtn,    &QPushButton::clicked,
            this, &AppControllerPage::onDevAgentStopClicked);
    connect(m_devInstallBtn, &QPushButton::clicked,
            this, &AppControllerPage::onDevAgentInstallClicked);
    connect(m_devRestoreBtn, &QPushButton::clicked,
            this, &AppControllerPage::onDevAgentRestoreClicked);
    connect(histRefreshBtn,  &QPushButton::clicked,
            this, &AppControllerPage::onDevAgentLoadHistory);
    connect(m_devHistoryList, &QListWidget::currentRowChanged,
            m_devRestoreBtn, [this](int row) {
        if (m_devRestoreBtn) m_devRestoreBtn->setEnabled(row >= 0);
    });
    /* Git */
    connect(gitLogRefresh,      &QPushButton::clicked,
            this, &AppControllerPage::onDevAgentGitLogClicked);
    connect(m_devGitRestoreBtn, &QPushButton::clicked,
            this, &AppControllerPage::onDevAgentGitRestoreClicked);
    connect(fetchResetBtn,      &QPushButton::clicked,
            this, &AppControllerPage::onDevAgentGitFetchResetClicked);
    connect(stashPushBtn,       &QPushButton::clicked,
            this, &AppControllerPage::onDevAgentGitStashPushClicked);
    connect(stashListRefresh,   &QPushButton::clicked,
            this, &AppControllerPage::onDevAgentGitStashListClicked);
    connect(m_devGitStashPopBtn, &QPushButton::clicked,
            this, &AppControllerPage::onDevAgentGitStashPopClicked);
    connect(m_devGitLogList, &QListWidget::currentRowChanged,
            m_devGitRestoreBtn, [this](int row) {
        if (m_devGitRestoreBtn) m_devGitRestoreBtn->setEnabled(row >= 0);
    });
    connect(m_devStashList, &QListWidget::currentRowChanged,
            m_devGitStashPopBtn, [this](int row) {
        if (m_devGitStashPopBtn) m_devGitStashPopBtn->setEnabled(row >= 0);
    });

    /* Carica cronologia e git log all'avvio del tab */
    QTimer::singleShot(300, this, &AppControllerPage::onDevAgentLoadHistory);
    QTimer::singleShot(400, this, &AppControllerPage::onDevAgentGitLogClicked);
    QTimer::singleShot(500, this, &AppControllerPage::onDevAgentGitStashListClicked);

    return w;
}

/* ══════════════════════════════════════════════════════════════
   Dev Agent LangGraph — modifica il codice di Prismalux in autonomia
   ══════════════════════════════════════════════════════════════ */

void AppControllerPage::onDevAgentRunClicked()
{
    const QString task = m_devTaskEdit ? m_devTaskEdit->text().trimmed() : QString();
    if (task.isEmpty()) {
        if (m_devStatusLbl) m_devStatusLbl->setText(
            "\xe2\x9d\x8c  Descrivi il task prima di avviare.");
        return;
    }

    /* Percorso server Python */
    const QString scriptPath = P::root() + "/MCPs/devagent_mcp/server.py";
    if (!QFile::exists(scriptPath)) {
        if (m_devLog) m_devLog->append(
            "\xe2\x9d\x8c  <b>server.py non trovato</b> \xe2\x80\x94 "
            "Percorso atteso: <code>" + scriptPath + "</code>");
        return;
    }

    /* Avvia processo se non già in esecuzione */
    if (m_devProc && m_devProc->state() != QProcess::NotRunning) {
        m_devProc->terminate();
        m_devProc->waitForFinished(2000);
    }
    if (!m_devProc) {
        m_devProc = new QProcess(this);
        m_devProc->setProcessChannelMode(QProcess::SeparateChannels);
        connect(m_devProc, &QProcess::readyReadStandardOutput,
                this, &AppControllerPage::onDevAgentReadOutput);
        connect(m_devProc, &QProcess::readyReadStandardError,
                this, &AppControllerPage::onDevAgentReadError);
        connect(m_devProc,
                QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                this, &AppControllerPage::onDevAgentFinished);
    }

    if (m_devLog)  m_devLog->clear();
    if (m_devDiff) m_devDiff->clear();
    m_devPendingOutput.clear();

    const QString model = m_devModelCombo
        ? m_devModelCombo->currentData().toString()
        : QStringLiteral("deepseek-coder:6.7b");

    m_devProc->start(P::findPython(), {scriptPath});
    if (!m_devProc->waitForStarted(P::kProcessStartTimeoutMs)) {
        if (m_devStatusLbl) m_devStatusLbl->setText(
            "\xe2\x9d\x8c  Impossibile avviare il server Dev Agent.");
        return;
    }

    /* Invia il task via stdin */
    const QJsonObject req{
        {"task",         task},
        {"model",        model},
        {"project_root", P::root()}
    };
    m_devProc->write(QJsonDocument(req).toJson(QJsonDocument::Compact) + "\n");

    m_devRunBtn->setEnabled(false);
    m_devStopBtn->setEnabled(true);
    if (m_devStatusLbl) m_devStatusLbl->setText(
        "\xf0\x9f\x9f\xa1  Dev Agent in esecuzione...");
    if (m_devLog) m_devLog->append(
        QString("\xf0\x9f\x9a\x80  <b>Task:</b> %1<br>"
                "\xf0\x9f\xa4\x96  Modello: %2")
            .arg(task.toHtmlEscaped(), model));
}

void AppControllerPage::onDevAgentLoadHistory()
{
    const QString scriptPath = P::root() + "/MCPs/devagent_mcp/server.py";
    if (!QFile::exists(scriptPath)) return;

    /* Avvia processo solo per list_history — processo usa stdin e poi esce */
    auto* proc = new QProcess(this);
    proc->setProcessChannelMode(QProcess::SeparateChannels);
    proc->start(P::findPython(), {scriptPath});
    if (!proc->waitForStarted(2000)) {
        if (proc->state() != QProcess::NotRunning) proc->kill();
        proc->deleteLater();
        return;
    }

    proc->write(QJsonDocument(QJsonObject{
        {"cmd", "list_history"}
    }).toJson(QJsonDocument::Compact) + "\n");
    proc->closeWriteChannel();

    connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            proc, [this, proc](int, QProcess::ExitStatus) {
        const QString out = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        proc->deleteLater();
        for (const QString& line : out.split('\n', Qt::SkipEmptyParts)) {
            const auto doc = QJsonDocument::fromJson(line.toUtf8());
            if (!doc.isObject()) continue;
            const QJsonObject obj = doc.object();
            if (obj.value("event").toString() != "history_list") continue;
            if (!m_devHistoryList) return;
            m_devHistoryList->clear();
            const QJsonArray entries = obj.value("entries").toArray();
            for (const QJsonValue& v : entries) {
                const QJsonObject e = v.toObject();
                const QString ts   = e.value("timestamp").toString().left(19).replace("T"," ");
                const QString task = e.value("task").toString().left(60);
                const QString id   = e.value("id").toString();
                const int nf       = e.value("n_files").toInt();
                auto* item = new QListWidgetItem(
                    QString("\xf0\x9f\x95\x90  %1  |  %2  (%3 file)")
                        .arg(ts, task).arg(nf),
                    m_devHistoryList);
                item->setData(Qt::UserRole, id);
                m_devHistoryList->addItem(item);
            }
            if (m_devHistoryList->count() == 0) {
                auto* item = new QListWidgetItem(
                    "(nessuno snapshot salvato ancora)", m_devHistoryList);
                item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
                m_devHistoryList->addItem(item);
            }
        }
    });
}

void AppControllerPage::onDevAgentRestoreClicked()
{
    if (!m_devHistoryList) return;
    auto* selected = m_devHistoryList->currentItem();
    if (!selected || !(selected->flags() & Qt::ItemIsEnabled)) return;

    const QString backupId = selected->data(Qt::UserRole).toString();
    if (backupId.isEmpty()) return;

    const auto ans = QMessageBox::question(
        this, "\xe2\x86\xa9  Ripristina snapshot",
        QString("Ripristinare lo snapshot:\n<b>%1</b>\n\n"
                "I file modificati torneranno allo stato precedente.\n"
                "Questa operazione non pu\xc3\xb2 essere annullata.")
            .arg(backupId),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ans != QMessageBox::Yes) return;

    const QString scriptPath = P::root() + "/MCPs/devagent_mcp/server.py";
    if (!QFile::exists(scriptPath)) {
        if (m_devStatusLbl) m_devStatusLbl->setText(tr("\xe2\x9d\x8c  server.py non trovato"));
        return;
    }

    if (m_devLog) m_devLog->append(
        QString("\xe2\x8f\xaa  <b>Ripristino snapshot:</b> %1...").arg(backupId));
    if (m_devStatusLbl) m_devStatusLbl->setText(
        "\xe2\x8f\xaa  Ripristino in corso...");

    auto* proc = new QProcess(this);
    proc->setProcessChannelMode(QProcess::SeparateChannels);
    proc->start(P::findPython(), {scriptPath});
    if (!proc->waitForStarted(2000)) {
        if (m_devStatusLbl) m_devStatusLbl->setText(tr("\xe2\x9d\x8c  Impossibile avviare il processo"));
        proc->deleteLater();
        return;
    }
    proc->write(QJsonDocument(QJsonObject{
        {"cmd",       "restore"},
        {"backup_id", backupId}
    }).toJson(QJsonDocument::Compact) + "\n");
    proc->closeWriteChannel();

    connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            proc, [this, proc, backupId](int, QProcess::ExitStatus) {
        const QString out = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        proc->deleteLater();
        for (const QString& line : out.split('\n', Qt::SkipEmptyParts)) {
            const auto doc = QJsonDocument::fromJson(line.toUtf8());
            if (!doc.isObject()) continue;
            const QJsonObject obj = doc.object();
            if (obj.value("event").toString() != "restore_done") continue;
            const bool ok  = obj.value("success").toBool();
            const QString msg = obj.value("msg").toString();
            if (m_devLog) m_devLog->append(
                (ok ? "\xe2\x9c\x85  <b>Ripristino completato</b>: " : "\xe2\x9d\x8c  ") +
                msg.toHtmlEscaped());
            if (m_devStatusLbl) m_devStatusLbl->setText(
                ok ? "\xe2\x9c\x85  Ripristinato" : "\xe2\x9d\x8c  Ripristino fallito");
        }
    });
}

void AppControllerPage::onDevAgentStopClicked()
{
    if (m_devProc && m_devProc->state() != QProcess::NotRunning) {
        m_devProc->terminate();
        if (!m_devProc->waitForFinished(P::kProcessStartTimeoutMs))
            m_devProc->kill();
    }
    if (m_devRunBtn)  m_devRunBtn->setEnabled(true);
    if (m_devStopBtn) m_devStopBtn->setEnabled(false);
    if (m_devStatusLbl) m_devStatusLbl->setText(tr("\xe2\x9a\xab  Fermato"));
    if (m_devLog) m_devLog->append("\xf0\x9f\x94\xb4  Dev Agent fermato.");
}

void AppControllerPage::onDevAgentInstallClicked()
{
    if (m_devInstallBtn) m_devInstallBtn->setEnabled(false);
    if (m_devStatusLbl)  m_devStatusLbl->setText(
        "\xe2\x8f\xb3  Installazione dipendenze LangGraph...");

    auto* proc = new QProcess(this);
    proc->setProcessChannelMode(QProcess::MergedChannels);
    connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            proc, [this, proc](int code, QProcess::ExitStatus) {
        const QString out = QString::fromUtf8(proc->readAll()).trimmed().right(200);
        if (code == 0) {
            if (m_devStatusLbl) m_devStatusLbl->setText(
                "\xe2\x9c\x85  LangGraph installato correttamente.");
            if (m_devLog) m_devLog->append(
                "\xe2\x9c\x85  <b>LangGraph installato</b> \xe2\x80\x94 pronto per l\xe2\x80\x99"
                "avvio del Dev Agent.");
        } else {
            if (m_devStatusLbl) m_devStatusLbl->setText(
                "\xe2\x9d\x8c  Installazione fallita (code " + QString::number(code) + ")");
            if (m_devLog) m_devLog->append(
                "<span style='color:#f87171;'>"
                "\xe2\x9d\x8c  Errore installazione:<br><pre>" + out.toHtmlEscaped() + "</pre>"
                "Riprova manualmente: "
                "<a href='pip://langgraph langchain-community langchain-ollama unidiff'"
                " style='color:#fbbf24;'>"
                "\xf0\x9f\x94\xa7 Installa LangGraph + dipendenze</a>"
                "</span>");
        }
        if (m_devInstallBtn) m_devInstallBtn->setEnabled(true);
        proc->deleteLater();
    });
    proc->start(P::findPython(),
        {"-m", "pip", "install", "--quiet", "--break-system-packages",
         "langgraph", "langchain-community", "langchain-ollama", "unidiff"});
}

void AppControllerPage::onDevAgentReadOutput()
{
    if (!m_devProc) return;
    m_devPendingOutput += QString::fromUtf8(m_devProc->readAllStandardOutput());

    /* Processa righe complete */
    while (m_devPendingOutput.contains('\n')) {
        const int nl = m_devPendingOutput.indexOf('\n');
        const QString line = m_devPendingOutput.left(nl).trimmed();
        m_devPendingOutput.remove(0, nl + 1);
        if (line.isEmpty()) continue;

        const auto doc = QJsonDocument::fromJson(line.toUtf8());
        if (!doc.isObject()) {
            if (m_devLog) m_devLog->append(line.toHtmlEscaped());
            continue;
        }
        const QJsonObject obj = doc.object();
        const QString evt = obj.value("event").toString();

        if (evt == "step") {
            const QString node = obj.value("node").toString();
            const QString msg  = obj.value("msg").toString();
            const QJsonArray files = obj.value("files").toArray();
            QString html = QString("\xf0\x9f\x94\xb9  <b>%1</b>").arg(node.toHtmlEscaped());
            if (!msg.isEmpty())   html += " \xe2\x80\x94 " + msg.toHtmlEscaped();
            if (!files.isEmpty()) {
                QStringList fl;
                for (const auto& f : files) fl << f.toString();
                html += "<br><small>" + fl.join(", ").toHtmlEscaped() + "</small>";
            }
            if (m_devLog) m_devLog->append(html);

        } else if (evt == "compile_output") {
            const bool ok = obj.value("ok").toBool();
            const QString out = obj.value("output").toString().right(300);
            const QString icon = ok ? "\xe2\x9c\x85" : "\xe2\x9d\x8c";
            if (m_devLog) m_devLog->append(
                icon + "  <b>Compilazione</b> " + (ok ? "OK" : "ERRORI") +
                "<br><pre style='font-size:9px'>" + out.toHtmlEscaped() + "</pre>");

        } else if (evt == "backup_created") {
            /* Aggiunge la nuova entry in cima alla lista cronologia */
            const QString bid  = obj.value("backup_id").toString();
            const QString ts   = obj.value("timestamp").toString().left(19).replace("T"," ");
            const QString task = obj.value("task").toString().left(60);
            const int     nf   = obj.value("n_files").toInt();
            if (m_devHistoryList) {
                auto* item = new QListWidgetItem(
                    QString("\xf0\x9f\x95\x90  %1  |  %2  (%3 file)")
                        .arg(ts, task).arg(nf));
                item->setData(Qt::UserRole, bid);
                m_devHistoryList->insertItem(0, item);
            }
            if (m_devLog) m_devLog->append(
                QString("\xf0\x9f\x92\xbe  Snapshot salvato: <b>%1</b> (%2 file)")
                    .arg(bid.toHtmlEscaped()).arg(nf));

        } else if (evt == "done") {
            const bool success = obj.value("success").toBool();
            const QString msg  = obj.value("msg").toString();
            const QString diff = obj.value("diff").toString();
            if (m_devDiff && !diff.isEmpty()) {
                /* Colora diff: righe + verde, righe - rosso */
                QString html;
                for (const QString& l : diff.split('\n')) {
                    if (l.startsWith('+'))
                        html += QString("<span style='color:#4ade80'>%1</span><br>")
                                    .arg(l.toHtmlEscaped());
                    else if (l.startsWith('-'))
                        html += QString("<span style='color:#f87171'>%1</span><br>")
                                    .arg(l.toHtmlEscaped());
                    else
                        html += l.toHtmlEscaped() + "<br>";
                }
                m_devDiff->setHtml("<pre style='font-size:9px'>" + html + "</pre>");
            }
            const QString icon = success ? "\xf0\x9f\x9f\xa2" : "\xf0\x9f\x94\xb4";
            if (m_devLog) m_devLog->append(
                icon + "  <b>" + (success ? "Completato" : "Fallito") + "</b>"
                + (msg.isEmpty() ? "" : " \xe2\x80\x94 " + msg.toHtmlEscaped()));
            if (m_devStatusLbl) m_devStatusLbl->setText(
                success ? "\xf0\x9f\x9f\xa2  Completato" : "\xf0\x9f\x94\xb4  Fallito");
        }
    }
}

void AppControllerPage::onDevAgentReadError()
{
    if (!m_devProc) return;
    const QString err = QString::fromUtf8(
        m_devProc->readAllStandardError()).trimmed();
    if (!err.isEmpty() && m_devLog)
        m_devLog->append(
            "<span style='color:#888'><i>" + err.left(200).toHtmlEscaped() + "</i></span>");
}

void AppControllerPage::onDevAgentFinished(int code, QProcess::ExitStatus)
{
    if (m_devRunBtn)  m_devRunBtn->setEnabled(true);
    if (m_devStopBtn) m_devStopBtn->setEnabled(false);
    if (code != 0 && m_devStatusLbl)
        m_devStatusLbl->setText(
            QString("\xe2\x9d\x8c  Processo terminato (code %1)").arg(code));
}

/* ── Helper interno: avvia processo Python one-shot per comandi git ── */
static QProcess* devGitProc(QWidget* parent, const QString& scriptPath)
{
    auto* proc = new QProcess(parent);
    proc->setProcessChannelMode(QProcess::SeparateChannels);
    proc->start(PrismaluxPaths::findPython(), {scriptPath});
    if (!proc->waitForStarted(P::kProcessStartTimeoutMs)) {
        if (proc->state() != QProcess::NotRunning) proc->kill();
        proc->deleteLater();
        return nullptr;
    }
    return proc;
}

void AppControllerPage::onDevAgentGitLogClicked()
{
    const QString script = P::root() + "/MCPs/devagent_mcp/server.py";
    if (!QFile::exists(script)) return;
    auto* proc = devGitProc(this, script);
    if (!proc) return;

    proc->write(QJsonDocument(QJsonObject{
        {"cmd",          "git_log"},
        {"project_root", P::root()},
        {"n",            25}
    }).toJson(QJsonDocument::Compact) + "\n");
    proc->closeWriteChannel();

    connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            proc, [this, proc](int, QProcess::ExitStatus) {
        const QString raw = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        proc->deleteLater();
        if (!m_devGitLogList) return;
        m_devGitLogList->clear();
        for (const QString& line : raw.split('\n', Qt::SkipEmptyParts)) {
            const auto doc = QJsonDocument::fromJson(line.toUtf8());
            if (!doc.isObject()) continue;
            const QJsonObject obj = doc.object();
            if (obj.value("event").toString() != "git_log") continue;
            const QJsonArray entries = obj.value("entries").toArray();
            for (const QJsonValue& v : entries) {
                const QJsonObject e  = v.toObject();
                const QString hash   = e.value("hash").toString();
                const QString shash  = e.value("short_hash").toString();
                const QString date   = e.value("date").toString();
                const QString subj   = e.value("subject").toString();
                auto* item = new QListWidgetItem(
                    QString("%1  %2  %3").arg(shash, date, subj.left(60)));
                item->setData(Qt::UserRole, hash);
                m_devGitLogList->addItem(item);
            }
            if (m_devGitLogList->count() == 0) {
                auto* item = new QListWidgetItem("(nessun commit trovato)");
                item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
                m_devGitLogList->addItem(item);
            }
        }
    });
}

void AppControllerPage::onDevAgentGitRestoreClicked()
{
    if (!m_devGitLogList) return;
    auto* sel = m_devGitLogList->currentItem();
    if (!sel || !(sel->flags() & Qt::ItemIsEnabled)) return;

    const QString commit = sel->data(Qt::UserRole).toString();
    if (commit.isEmpty()) return;

    const QString label  = sel->text().left(80);
    const auto ans = QMessageBox::question(
        this, "\xe2\x86\xa9  Ripristina al commit",
        QString("Ripristinare il worktree al commit:\n<b>%1</b>\n\n"
                "Tutte le modifiche locali non committate andranno perse.\n"
                "Questa operazione non pu\xc3\xb2 essere annullata.")
            .arg(label),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ans != QMessageBox::Yes) return;

    const QString script = P::root() + "/MCPs/devagent_mcp/server.py";
    if (!QFile::exists(script)) return;
    auto* proc = devGitProc(this, script);
    if (!proc) return;

    if (m_devLog) m_devLog->append(
        QString("\xe2\x8f\xaa  <b>git reset --hard</b> %1...").arg(commit.left(8)));
    if (m_devStatusLbl) m_devStatusLbl->setText(tr("\xe2\x8f\xaa  Ripristino commit..."));

    proc->write(QJsonDocument(QJsonObject{
        {"cmd",          "git_restore"},
        {"project_root", P::root()},
        {"commit",       commit},
        {"files",        QJsonArray{}}
    }).toJson(QJsonDocument::Compact) + "\n");
    proc->closeWriteChannel();

    connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            proc, [this, proc, commit](int, QProcess::ExitStatus) {
        const QString raw = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        proc->deleteLater();
        for (const QString& line : raw.split('\n', Qt::SkipEmptyParts)) {
            const auto doc = QJsonDocument::fromJson(line.toUtf8());
            if (!doc.isObject()) continue;
            const QJsonObject obj = doc.object();
            if (obj.value("event").toString() != "git_restore_done") continue;
            const bool ok  = obj.value("success").toBool();
            const QString msg = obj.value("msg").toString();
            const QString icon = ok ? "\xf0\x9f\x9f\xa2" : "\xf0\x9f\x94\xb4";
            if (m_devLog) m_devLog->append(
                icon + "  <b>Ripristino commit</b> " + commit.left(8).toHtmlEscaped()
                + "<br>" + msg.toHtmlEscaped());
            if (m_devStatusLbl) m_devStatusLbl->setText(
                ok ? "\xf0\x9f\x9f\xa2  Ripristinato" : "\xf0\x9f\x94\xb4  Errore ripristino");
        }
    });
}

void AppControllerPage::onDevAgentGitFetchResetClicked()
{
    const QString branch = m_devGitBranchEdit
        ? m_devGitBranchEdit->text().trimmed()
        : QStringLiteral("master");

    const auto ans = QMessageBox::warning(
        this, "\xf0\x9f\x8c\x90  Fetch + Reset da GitHub",
        QString("Eseguire:\n  git fetch origin\n  git reset --hard origin/%1\n\n"
                "\xe2\x9a\xa0  Tutte le modifiche locali non committate "
                "andranno PERSE.\n"
                "Assicurati di aver fatto uno stash o un commit prima di procedere.")
            .arg(branch),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (ans != QMessageBox::Yes) return;

    const QString script = P::root() + "/MCPs/devagent_mcp/server.py";
    if (!QFile::exists(script)) return;
    auto* proc = devGitProc(this, script);
    if (!proc) return;

    if (m_devLog) m_devLog->append(
        QString("\xf0\x9f\x8c\x90  <b>git fetch origin && git reset --hard origin/%1</b>...")
            .arg(branch.toHtmlEscaped()));
    if (m_devStatusLbl) m_devStatusLbl->setText(tr("\xf0\x9f\x8c\x90  Fetch in corso..."));

    proc->write(QJsonDocument(QJsonObject{
        {"cmd",          "git_fetch_reset"},
        {"project_root", P::root()},
        {"remote",       "origin"},
        {"branch",       branch}
    }).toJson(QJsonDocument::Compact) + "\n");
    proc->closeWriteChannel();

    connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            proc, [this, proc, branch](int, QProcess::ExitStatus) {
        const QString raw = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        proc->deleteLater();
        for (const QString& line : raw.split('\n', Qt::SkipEmptyParts)) {
            const auto doc = QJsonDocument::fromJson(line.toUtf8());
            if (!doc.isObject()) continue;
            const QJsonObject obj = doc.object();
            if (obj.value("event").toString() != "git_fetch_reset_done") continue;
            const bool ok     = obj.value("success").toBool();
            const QString msg = obj.value("msg").toString();
            const QString icon = ok ? "\xf0\x9f\x9f\xa2" : "\xf0\x9f\x94\xb4";
            if (m_devLog) m_devLog->append(
                icon + "  <b>Fetch+Reset origin/" + branch.toHtmlEscaped() + "</b><br>"
                + "<pre style='font-size:9px'>" + msg.toHtmlEscaped() + "</pre>");
            if (m_devStatusLbl) m_devStatusLbl->setText(
                ok ? "\xf0\x9f\x9f\xa2  Sincronizzato con GitHub"
                   : "\xf0\x9f\x94\xb4  Errore fetch/reset");
            if (ok) {
                /* Aggiorna il log commit dopo il fetch */
                QTimer::singleShot(200, this, &AppControllerPage::onDevAgentGitLogClicked);
            }
        }
    });
}

void AppControllerPage::onDevAgentGitStashPushClicked()
{
    const QString script = P::root() + "/MCPs/devagent_mcp/server.py";
    if (!QFile::exists(script)) return;
    auto* proc = devGitProc(this, script);
    if (!proc) return;

    if (m_devLog) m_devLog->append("\xf0\x9f\x93\xa6  <b>git stash push</b>...");
    if (m_devStatusLbl) m_devStatusLbl->setText(tr("\xf0\x9f\x93\xa6  Stash in corso..."));

    proc->write(QJsonDocument(QJsonObject{
        {"cmd",          "git_stash_push"},
        {"project_root", P::root()},
        {"message",      "devagent stash"}
    }).toJson(QJsonDocument::Compact) + "\n");
    proc->closeWriteChannel();

    connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            proc, [this, proc](int, QProcess::ExitStatus) {
        const QString raw = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        proc->deleteLater();
        for (const QString& line : raw.split('\n', Qt::SkipEmptyParts)) {
            const auto doc = QJsonDocument::fromJson(line.toUtf8());
            if (!doc.isObject()) continue;
            const QJsonObject obj = doc.object();
            if (obj.value("event").toString() != "git_stash_done") continue;
            const bool ok     = obj.value("success").toBool();
            const QString msg = obj.value("msg").toString();
            const QString icon = ok ? "\xf0\x9f\x9f\xa2" : "\xf0\x9f\x94\xb4";
            if (m_devLog) m_devLog->append(
                icon + "  <b>Stash push</b>: " + msg.toHtmlEscaped());
            if (m_devStatusLbl) m_devStatusLbl->setText(
                ok ? "\xf0\x9f\x9f\xa2  Stash salvato"
                   : "\xf0\x9f\x94\xb4  Stash fallito");
            if (ok)
                QTimer::singleShot(100, this,
                    &AppControllerPage::onDevAgentGitStashListClicked);
        }
    });
}

void AppControllerPage::onDevAgentGitStashListClicked()
{
    const QString script = P::root() + "/MCPs/devagent_mcp/server.py";
    if (!QFile::exists(script)) return;
    auto* proc = devGitProc(this, script);
    if (!proc) return;

    proc->write(QJsonDocument(QJsonObject{
        {"cmd",          "git_stash_list"},
        {"project_root", P::root()}
    }).toJson(QJsonDocument::Compact) + "\n");
    proc->closeWriteChannel();

    connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            proc, [this, proc](int, QProcess::ExitStatus) {
        const QString raw = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        proc->deleteLater();
        if (!m_devStashList) return;
        m_devStashList->clear();
        for (const QString& line : raw.split('\n', Qt::SkipEmptyParts)) {
            const auto doc = QJsonDocument::fromJson(line.toUtf8());
            if (!doc.isObject()) continue;
            const QJsonObject obj = doc.object();
            if (obj.value("event").toString() != "git_stash_list") continue;
            const QJsonArray entries = obj.value("entries").toArray();
            for (const QJsonValue& v : entries) {
                const QJsonObject e   = v.toObject();
                const QString ref     = e.value("ref").toString();
                const QString subject = e.value("subject").toString();
                const QString when    = e.value("when").toString();
                auto* item = new QListWidgetItem(
                    QString("%1  %2  (%3)").arg(ref, subject.left(50), when));
                item->setData(Qt::UserRole, ref);
                m_devStashList->addItem(item);
            }
            if (m_devStashList->count() == 0) {
                auto* item = new QListWidgetItem("(nessuno stash)");
                item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
                m_devStashList->addItem(item);
            }
            if (m_devGitStashPopBtn)
                m_devGitStashPopBtn->setEnabled(m_devStashList->count() > 0
                    && (m_devStashList->item(0)->flags() & Qt::ItemIsEnabled));
        }
    });
}

void AppControllerPage::onDevAgentGitStashPopClicked()
{
    if (!m_devStashList) return;
    auto* sel = m_devStashList->currentItem();
    if (!sel || !(sel->flags() & Qt::ItemIsEnabled)) return;

    const QString ref = sel->data(Qt::UserRole).toString();
    if (ref.isEmpty()) return;

    const QString script = P::root() + "/MCPs/devagent_mcp/server.py";
    if (!QFile::exists(script)) return;
    auto* proc = devGitProc(this, script);
    if (!proc) return;

    if (m_devLog) m_devLog->append(
        QString("\xf0\x9f\x93\xa4  <b>git stash pop</b> %1...").arg(ref.toHtmlEscaped()));
    if (m_devStatusLbl) m_devStatusLbl->setText(tr("\xf0\x9f\x93\xa4  Applico stash..."));

    proc->write(QJsonDocument(QJsonObject{
        {"cmd",          "git_stash_pop"},
        {"project_root", P::root()},
        {"ref",          ref}
    }).toJson(QJsonDocument::Compact) + "\n");
    proc->closeWriteChannel();

    connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            proc, [this, proc](int, QProcess::ExitStatus) {
        const QString raw = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        proc->deleteLater();
        for (const QString& line : raw.split('\n', Qt::SkipEmptyParts)) {
            const auto doc = QJsonDocument::fromJson(line.toUtf8());
            if (!doc.isObject()) continue;
            const QJsonObject obj = doc.object();
            if (obj.value("event").toString() != "git_stash_done") continue;
            const bool ok     = obj.value("success").toBool();
            const QString msg = obj.value("msg").toString();
            const QString icon = ok ? "\xf0\x9f\x9f\xa2" : "\xf0\x9f\x94\xb4";
            if (m_devLog) m_devLog->append(
                icon + "  <b>Stash pop</b>: " + msg.toHtmlEscaped());
            if (m_devStatusLbl) m_devStatusLbl->setText(
                ok ? "\xf0\x9f\x9f\xa2  Stash applicato"
                   : "\xf0\x9f\x94\xb4  Stash pop fallito");
            if (ok)
                QTimer::singleShot(100, this,
                    &AppControllerPage::onDevAgentGitStashListClicked);
        }
    });
}
