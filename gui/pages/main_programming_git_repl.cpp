/* ══════════════════════════════════════════════════════════════
   programmazione_page_git_repl.cpp

   Implementazioni per i sub-tab Git MCP e Python REPL.
   Fanno parte di ProgrammazionePage (dichiarazioni in programmazione_page.h).
   ══════════════════════════════════════════════════════════════ */
#include "../dpi_utils.h"
#include "main_programming.h"
#include "../prismalux_paths.h"
#include "../log_bus.h"
#include "../ai_utils.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QProcess>
#include <QGroupBox>
#include <QFrame>
#include <QTimer>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextCursor>
#include <QFont>
#include <QApplication>
#include <QStandardPaths>
#include <QKeyEvent>
#include <QDir>
#include <QFile>
#include <QBrush>
#include <QColor>

namespace P = PrismaluxPaths;

/* ──────────────────────────────────────────────────────────────
   Filtro tastiera per la cronologia REPL (frecce su/giù)
   ────────────────────────────────────────────────────────────── */
class ReplHistFilter : public QObject {
public:
    ReplHistFilter(QLineEdit* input, QStringList& hist, int& idx, QObject* parent)
        : QObject(parent), m_input(input), m_hist(hist), m_idx(idx) {}

    bool eventFilter(QObject* obj, QEvent* ev) override {
        if (obj != m_input || ev->type() != QEvent::KeyPress) return false;
        auto* ke = static_cast<QKeyEvent*>(ev);
        if (ke->key() == Qt::Key_Up) {
            if (m_hist.isEmpty()) return true;
            if (m_idx < 0) m_idx = m_hist.size() - 1;
            else if (m_idx > 0) --m_idx;
            m_input->setText(m_hist[m_idx]);
            return true;
        }
        if (ke->key() == Qt::Key_Down) {
            if (m_idx < 0) return true;
            if (m_idx < m_hist.size() - 1) {
                m_input->setText(m_hist[++m_idx]);
            } else {
                m_idx = -1;
                m_input->clear();
            }
            return true;
        }
        return false;
    }
private:
    QLineEdit*  m_input;
    QStringList& m_hist;
    int&         m_idx;
};

/* ══════════════════════════════════════════════════════════════
   buildGitMcp — sub-tab "🔧 Git"
   ══════════════════════════════════════════════════════════════ */
QWidget* ProgrammazionePage::buildGitMcp(QWidget* parent)
{
    auto* w   = new QWidget(parent);
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(12, 12, 12, 12);
    lay->setSpacing(8);

    /* Descrizione + separatore */
    auto* desc = new QLabel(
        "\xf0\x9f\x94\xa7  <b>Git MCP</b> \xe2\x80\x94 "
        "Operazioni git con assistenza AI: analisi diff, "
        "generazione commit message, suggerimenti.", w);
    desc->setObjectName("hintLabel");
    desc->setWordWrap(true);
    lay->addWidget(desc);

    auto* sep1 = new QFrame(w);
    sep1->setFrameShape(QFrame::HLine);
    sep1->setObjectName("sidebarSep");
    lay->addWidget(sep1);

    QPushButton* btnBrowse     = nullptr;  /* wired inside buildGitRepoRow */
    QPushButton* btnAddCommit  = nullptr;
    QPushButton* btnPull       = nullptr;  /* unused: wired inside buildGitActionsRow */
    QPushButton* btnPush       = nullptr;
    QPushButton* btnClearGit   = nullptr;
    QPushButton* btnGitAi      = nullptr;
    QPushButton* btnGenCommit  = nullptr;
    QPushButton* btnRefGit     = nullptr;
    QPushButton* btnCloseGitAi = nullptr;

    lay->addWidget(buildGitRepoRow(w, btnBrowse));      /* browse wired inside */
    lay->addWidget(buildGitActionsRow(w));               /* all action btns wired inside */
    lay->addWidget(buildGitCommitRow(w, btnAddCommit, btnPull, btnPush));
    lay->addWidget(buildGitOutputGroup(w, btnClearGit, btnGitAi, btnGenCommit), 1);
    lay->addWidget(buildGitAiPanel(w, btnRefGit, btnCloseGitAi));

    setupGitConnections(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                        nullptr, btnAddCommit, btnPush,
                        btnClearGit, btnGitAi, btnGenCommit,
                        btnRefGit, btnCloseGitAi);
    return w;
}

/* ── Riga percorso repository ── */
QWidget* ProgrammazionePage::buildGitRepoRow(QWidget* parent,
                                               QPushButton*& outBtnBrowse)
{
    auto* repoRow = new QWidget(parent);
    auto* repoLay = new QHBoxLayout(repoRow);
    repoLay->setContentsMargins(0, 0, 0, 0);
    repoLay->setSpacing(8);
    repoLay->addWidget(new QLabel("Repository:", repoRow));

    m_gitRepoPath = new QLineEdit(repoRow);
    m_gitRepoPath->setObjectName("chatInput");
    m_gitRepoPath->setText(QDir::cleanPath(P::root() + "/.."));
    m_gitRepoPath->setPlaceholderText(tr("/percorso/repository"));
    repoLay->addWidget(m_gitRepoPath, 1);

    outBtnBrowse = new QPushButton("\xf0\x9f\x93\x82", repoRow);
    outBtnBrowse->setObjectName("actionBtn");
    outBtnBrowse->setFixedWidth(dpiScale(34));
    outBtnBrowse->setToolTip(tr("Scegli cartella repository"));
    repoLay->addWidget(outBtnBrowse);

    connect(outBtnBrowse, &QPushButton::clicked,
            this, &ProgrammazionePage::onBtnGitBrowseClicked);
    return repoRow;
}

/* ── Pulsanti azioni rapide (connessioni cablate internamente) ── */
QWidget* ProgrammazionePage::buildGitActionsRow(QWidget* parent)
{
    m_gitActRow = new QWidget(parent);
    auto* actLay = new QHBoxLayout(m_gitActRow);
    actLay->setContentsMargins(0, 0, 0, 0);
    actLay->setSpacing(6);

    auto mkBtn = [&](const char* label, const char* tip) -> QPushButton* {
        auto* b = new QPushButton(QString::fromUtf8(label), m_gitActRow);
        b->setObjectName("actionBtn");
        b->setToolTip(QString::fromUtf8(tip));
        actLay->addWidget(b);
        return b;
    };

    auto* btnStatus = mkBtn("\xf0\x9f\x93\x8b  Status",     "git status");
    auto* btnDiff   = mkBtn("\xf0\x9f\x94\x8d  Diff",        "git diff (modifiche non staged)");
    auto* btnDiffSt = mkBtn("\xf0\x9f\x94\x8d  Diff staged", "git diff --staged");
    auto* btnLog    = mkBtn("\xf0\x9f\x93\x9c  Log",         "git log --oneline -20");
    auto* btnBranch = mkBtn("\xf0\x9f\x8c\xbf  Branch",      "git branch -a");
    auto* btnPull   = mkBtn("\xe2\xac\x87  Pull",            "git pull (richiede conferma)");
    actLay->addStretch(1);

    m_btnGitStop = new QPushButton("\xe2\x96\xa0  Stop", m_gitActRow);
    m_btnGitStop->setObjectName("actionBtn");
    m_btnGitStop->setProperty("danger", "true");
    m_btnGitStop->setEnabled(false);
    actLay->addWidget(m_btnGitStop);

    connect(btnStatus, &QPushButton::clicked, this, &ProgrammazionePage::onBtnGitStatusClicked);
    connect(btnDiff,   &QPushButton::clicked, this, &ProgrammazionePage::onBtnGitDiffClicked);
    connect(btnDiffSt, &QPushButton::clicked, this, &ProgrammazionePage::onBtnGitDiffStagedClicked);
    connect(btnLog,    &QPushButton::clicked, this, &ProgrammazionePage::onBtnGitLogClicked);
    connect(btnBranch, &QPushButton::clicked, this, &ProgrammazionePage::onBtnGitBranchClicked);
    connect(btnPull,   &QPushButton::clicked, this, &ProgrammazionePage::onBtnGitPullClicked);
    connect(m_btnGitStop, &QPushButton::clicked, this, &ProgrammazionePage::onBtnGitStopClicked);

    return m_gitActRow;
}

/* ── Riga commit ── */
QWidget* ProgrammazionePage::buildGitCommitRow(QWidget* parent,
                                                QPushButton*& outBtnAddCommit,
                                                QPushButton*& /*outBtnPull*/,
                                                QPushButton*& outBtnPush)
{
    auto* commitRow = new QWidget(parent);
    auto* commitLay = new QHBoxLayout(commitRow);
    commitLay->setContentsMargins(0, 0, 0, 0);
    commitLay->setSpacing(6);

    m_gitCommitMsg = new QLineEdit(commitRow);
    m_gitCommitMsg->setObjectName("chatInput");
    m_gitCommitMsg->setPlaceholderText(tr("Messaggio di commit..."));
    commitLay->addWidget(m_gitCommitMsg, 1);

    outBtnAddCommit = new QPushButton("\xe2\x9c\x85  Add + Commit", commitRow);
    outBtnAddCommit->setObjectName("actionBtn");
    outBtnAddCommit->setProperty("highlight", "true");
    outBtnAddCommit->setToolTip(tr("git add -A  \xe2\x86\x92  git commit -m \"...\""));
    commitLay->addWidget(outBtnAddCommit);

    outBtnPush = new QPushButton("\xe2\xac\x86  Push", commitRow);
    outBtnPush->setObjectName("actionBtn");
    outBtnPush->setToolTip(tr("git push (richiede conferma)"));
    commitLay->addWidget(outBtnPush);

    return commitRow;
}

/* ── Gruppo output git + sub-toolbar ── */
QWidget* ProgrammazionePage::buildGitOutputGroup(QWidget* parent,
                                                   QPushButton*& outBtnClearGit,
                                                   QPushButton*& outBtnGitAi,
                                                   QPushButton*& outBtnGenCommit)
{
    QFont monoFont;
    monoFont.setFamily("JetBrains Mono");
    monoFont.setStyleHint(QFont::Monospace);
    const int appPt = QApplication::font().pointSize();
    monoFont.setPointSize(appPt > 0 ? appPt : 10);

    auto* outGroup = new QGroupBox("\xf0\x9f\x96\xa5  Output git", parent);
    outGroup->setObjectName("cardGroup");
    auto* outLay = new QVBoxLayout(outGroup);
    outLay->setContentsMargins(4, 8, 4, 4);
    outLay->setSpacing(4);

    m_gitOutput = new QPlainTextEdit(outGroup);
    m_gitOutput->setObjectName("chatLog");
    m_gitOutput->setFont(monoFont);
    m_gitOutput->setReadOnly(true);
    m_gitOutput->setMaximumBlockCount(3000);
    m_gitOutput->setPlaceholderText(
        "L'output dei comandi git appari\xc3\xa0 qui.\n\nPremi Status per iniziare.");
    outLay->addWidget(m_gitOutput, 1);

    auto* outActRow = new QWidget(outGroup);
    auto* outActLay = new QHBoxLayout(outActRow);
    outActLay->setContentsMargins(0, 2, 0, 0);
    outActLay->setSpacing(8);

    outBtnClearGit = new QPushButton("\xf0\x9f\x97\x91  Pulisci", outActRow);
    outBtnClearGit->setObjectName("actionBtn");
    outActLay->addWidget(outBtnClearGit);

    outBtnGitAi = new QPushButton("\xf0\x9f\xa4\x96  Analizza con AI", outActRow);
    outBtnGitAi->setObjectName("actionBtn");
    outBtnGitAi->setToolTip(
        "L'AI spiega l'output e suggerisce le prossime operazioni");
    outActLay->addWidget(outBtnGitAi);

    outBtnGenCommit = new QPushButton("\xe2\x9c\x8f  Genera commit msg", outActRow);
    outBtnGenCommit->setObjectName("actionBtn");
    outBtnGenCommit->setProperty("highlight", "true");
    outBtnGenCommit->setToolTip(
        "Esegue git diff --staged, poi chiede all'AI di scrivere "
        "un messaggio di commit convenzionale");
    outActLay->addWidget(outBtnGenCommit);
    outActLay->addStretch(1);

    outLay->addWidget(outActRow);
    return outGroup;
}

/* ── Pannello AI git (nascosto di default) ── */
QWidget* ProgrammazionePage::buildGitAiPanel(QWidget* parent,
                                               QPushButton*& outBtnRefGit,
                                               QPushButton*& outBtnCloseGitAi)
{
    QFont monoFont;
    monoFont.setFamily("JetBrains Mono");
    monoFont.setStyleHint(QFont::Monospace);
    const int appPt = QApplication::font().pointSize();
    monoFont.setPointSize(appPt > 0 ? appPt : 10);

    m_gitAiPanel = new QWidget(parent);
    m_gitAiPanel->setObjectName("aiPanel");
    auto* aiLay = new QVBoxLayout(m_gitAiPanel);
    aiLay->setContentsMargins(0, 4, 0, 0);
    aiLay->setSpacing(4);

    auto* sep2 = new QFrame(m_gitAiPanel);
    sep2->setFrameShape(QFrame::HLine);
    sep2->setObjectName("sidebarSep");
    aiLay->addWidget(sep2);

    auto* aiModRow = new QWidget(m_gitAiPanel);
    auto* aiModLay = new QHBoxLayout(aiModRow);
    aiModLay->setContentsMargins(0, 0, 0, 0);
    aiModLay->setSpacing(8);
    aiModLay->addWidget(new QLabel("\xf0\x9f\xa4\x96  Modello:", aiModRow));

    m_gitAiModel = new QComboBox(aiModRow);
    m_gitAiModel->setObjectName("settingCombo");
    m_gitAiModel->addItem(
        m_ai ? (m_ai->model().isEmpty() ? "(nessun modello)" : m_ai->model())
             : "(AI non disponibile)",
        m_ai ? m_ai->model() : QString());
    aiModLay->addWidget(m_gitAiModel, 1);

    outBtnRefGit = new QPushButton("\xf0\x9f\x94\x84", aiModRow);
    outBtnRefGit->setObjectName("actionBtn");
    outBtnRefGit->setFixedWidth(dpiScale(32));
    aiModLay->addWidget(outBtnRefGit);

    outBtnCloseGitAi = new QPushButton("\xe2\x9c\x95", aiModRow);
    outBtnCloseGitAi->setObjectName("actionBtn");
    outBtnCloseGitAi->setFixedWidth(dpiScale(32));
    aiModLay->addWidget(outBtnCloseGitAi);
    aiLay->addWidget(aiModRow);

    m_gitAiOutput = new QPlainTextEdit(m_gitAiPanel);
    m_gitAiOutput->setObjectName("chatLog");
    m_gitAiOutput->setFont(monoFont);
    m_gitAiOutput->setReadOnly(true);
    m_gitAiOutput->setMaximumBlockCount(2000);
    m_gitAiOutput->setMaximumHeight(200);
    m_gitAiOutput->setPlaceholderText(tr("L'analisi AI appari\xc3\xa0 qui..."));
    aiLay->addWidget(m_gitAiOutput);

    m_gitAiPanel->hide();
    return m_gitAiPanel;
}

/* ── Connessioni Git (browse+actions wired in their build helpers) ── */
void ProgrammazionePage::setupGitConnections(QPushButton* /*btnBrowse*/,
                                               QPushButton* /*btnStatus*/,
                                               QPushButton* /*btnDiff*/,
                                               QPushButton* /*btnDiffSt*/,
                                               QPushButton* /*btnLog*/,
                                               QPushButton* /*btnBranch*/,
                                               QPushButton* /*btnPull*/,
                                               QPushButton* btnAddCommit,
                                               QPushButton* btnPush,
                                               QPushButton* btnClearGit,
                                               QPushButton* btnGitAi,
                                               QPushButton* btnGenCommit,
                                               QPushButton* btnRefGit,
                                               QPushButton* btnCloseGitAi)
{
    if (btnRefGit) {
        connect(btnRefGit, &QPushButton::clicked,
                this, &ProgrammazionePage::populateGitModels);
        QTimer::singleShot(0, this, &ProgrammazionePage::populateGitModels);
    }

    if (btnAddCommit)  connect(btnAddCommit,  &QPushButton::clicked, this, &ProgrammazionePage::onBtnAddCommitClicked);
    if (btnPush)       connect(btnPush,       &QPushButton::clicked, this, &ProgrammazionePage::onBtnPushClicked);
    if (btnClearGit)   connect(btnClearGit,   &QPushButton::clicked, this, &ProgrammazionePage::onBtnClearGitClicked);
    if (btnGitAi)      connect(btnGitAi,      &QPushButton::clicked, this, &ProgrammazionePage::onBtnGitAiClicked);
    if (btnGenCommit)  connect(btnGenCommit,  &QPushButton::clicked, this, &ProgrammazionePage::onBtnGenCommitClicked);
    if (btnCloseGitAi) connect(btnCloseGitAi, &QPushButton::clicked, this, &ProgrammazionePage::onBtnCloseGitAiClicked);
}

/* ══════════════════════════════════════════════════════════════
   gitRun — esegue un comando git nella repo selezionata.

   Disabilita m_gitActRow durante l'esecuzione.
   Se m_gitPendingCommit è non-vuota dopo un "add -A" riuscito,
   lancia automaticamente "commit -m {msg}" in sequenza.
   ══════════════════════════════════════════════════════════════ */
void ProgrammazionePage::gitRun(const QString& subcmd, const QStringList& args)
{
    const QString repo = m_gitRepoPath ? m_gitRepoPath->text().trimmed() : QString();
    if (repo.isEmpty()) {
        if (m_gitOutput)
            m_gitOutput->appendPlainText(
                "\xe2\x9d\x8c  Imposta il percorso del repository.\n");
        LogBus::post("\xe2\x9d\x8c Git: Percorso del repository non impostato.");
        return;
    }
    if (!QDir(repo).exists()) {
        if (m_gitOutput)
            m_gitOutput->appendPlainText(
                QString("\xe2\x9d\x8c  Cartella non trovata: %1\n").arg(repo));
        LogBus::post("\xe2\x9d\x8c Git: Cartella non trovata: " + repo);
        return;
    }
    if (!QDir(repo + "/.git").exists() && !QFile::exists(repo + "/.git")) {
        if (m_gitOutput)
            m_gitOutput->appendPlainText(
                QString("\xe2\x9d\x8c  \"%1\" non \xc3\xa8 un repository git.\n"
                        "      Seleziona la cartella radice del progetto (quella che contiene .git).\n")
                .arg(repo));
        LogBus::post("\xe2\x9d\x8c Git: \"" + repo + "\" non e' un repository git.");
        return;
    }

    /* Abbandona processo precedente: disconnect prima di kill evita segnali
       su oggetti già in uscita quando finished() arriva dopo deleteLater */
    if (m_gitProc && m_gitProc->state() != QProcess::NotRunning) {
        m_gitProc->disconnect();
        m_gitProc->kill();
        m_gitProc->deleteLater();
        m_gitProc = nullptr;
    }

    /* Mostra comando nell'output */
    QStringList cmdArgs = {"-C", repo, subcmd};
    cmdArgs << args;
    if (m_gitOutput)
        m_gitOutput->appendPlainText(
            QString("\n$ git %1\n")
            .arg((QStringList{subcmd} + args).join(' ')));

    /* Disabilita azioni, abilita Stop */
    if (m_gitActRow)  m_gitActRow->setEnabled(false);
    if (m_btnGitStop) m_btnGitStop->setEnabled(true);

    m_gitProc = new QProcess(this);
    m_gitProc->setProcessChannelMode(QProcess::MergedChannels);
    m_gitProc->setWorkingDirectory(repo);

    connect(m_gitProc, &QProcess::readyRead,
            this, &ProgrammazionePage::onGitReadyRead);
    connect(m_gitProc,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ProgrammazionePage::onGitFinished);
    connect(m_gitProc, &QProcess::errorOccurred,
            this, &ProgrammazionePage::onGitErrorOccurred);
    m_gitProc->start("git", cmdArgs);
}

/* ══════════════════════════════════════════════════════════════
   gitAiRequest — invia all'AI una richiesta contestuale sull'output git.
   ══════════════════════════════════════════════════════════════ */
void ProgrammazionePage::gitAiRequest(const QString& request, const QString& context)
{
    if (!m_ai || !m_gitAiOutput) return;
    if (m_ai->busy()) {
        m_gitAiOutput->setPlainText(
            "\xe2\x9a\xa0\xef\xb8\x8f  AI occupata. Attendi.");
        return;
    }

    /* Applica modello scelto */
    if (m_gitAiModel) {
        const QString sel = m_gitAiModel->currentData().toString();
        if (!sel.isEmpty() && sel != m_ai->model())
            m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), sel);
    }

    const QString sys =
        "Sei un esperto di git e workflow di sviluppo. "
        "Analizza l'output git fornito dall'utente e rispondi alla sua richiesta. "
        "Rispondi SEMPRE in italiano. Sii conciso e pratico.";

    const QString user = QString(
        "Output git:\n```\n%1\n```\n\nRichiesta: %2")
        .arg(context.left(3000), request);

    m_gitAiOutput->clear();
    {
        const QString mn = m_ai->model().isEmpty() ? "AI" : m_ai->model();
        m_gitAiOutput->insertPlainText(
            QString("\xf0\x9f\xa4\x96  %1\n%2\n\n")
            .arg(mn, QString(qMax(mn.length(), 20), '-')));
    }

    disconnect(m_gitAiTokenConn);
    disconnect(m_gitAiFinishedConn);
    disconnect(m_gitAiErrorConn);
    m_gitAiTokenConn    = connect(m_ai, &AiClient::token,    this, &ProgrammazionePage::onGitAiToken);
    m_gitAiFinishedConn = connect(m_ai, &AiClient::finished, this, &ProgrammazionePage::onGitAiFinished);
    m_gitAiErrorConn    = connect(m_ai, &AiClient::error,    this, &ProgrammazionePage::onGitAiError);

    m_ai->chat(sys, user);
}

/* ══════════════════════════════════════════════════════════════
   buildPythonRepl — sub-tab "🐍 REPL"
   ══════════════════════════════════════════════════════════════ */
QWidget* ProgrammazionePage::buildPythonRepl(QWidget* parent)
{
    auto* w   = new QWidget(parent);
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(12, 12, 12, 12);
    lay->setSpacing(8);

    QPushButton* btnRestart   = nullptr;
    QPushButton* btnImport    = nullptr;
    QPushButton* btnClearRepl = nullptr;

    lay->addWidget(buildReplHeader(w, btnRestart, btnImport));

    auto* sep = new QFrame(w);
    sep->setFrameShape(QFrame::HLine);
    sep->setObjectName("sidebarSep");
    lay->addWidget(sep);

    lay->addWidget(buildReplOutputGroup(w, btnClearRepl), 1);
    lay->addWidget(buildReplInputRow(w));

    m_replInput->installEventFilter(
        new ReplHistFilter(m_replInput, m_replHistory, m_replHistIdx, this));

    setupReplConnections(btnRestart, btnImport, btnClearRepl);
    return w;
}

/* ── Header: descrizione + status + pulsanti ── */
QWidget* ProgrammazionePage::buildReplHeader(QWidget* parent,
                                               QPushButton*& outBtnRestart,
                                               QPushButton*& outBtnImport)
{
    auto* hdr    = new QWidget(parent);
    auto* hdrLay = new QHBoxLayout(hdr);
    hdrLay->setContentsMargins(0, 0, 0, 0);
    hdrLay->setSpacing(10);

    auto* desc = new QLabel(
        "\xf0\x9f\x90\x8d  <b>Python REPL</b> \xe2\x80\x94 "
        "Sessione interattiva persistente. "
        "Le variabili rimangono tra un'esecuzione e l'altra.", parent);
    desc->setObjectName("hintLabel");
    desc->setWordWrap(true);
    hdrLay->addWidget(desc, 1);

    m_replStatus = new QLabel("\xe2\x9d\x8c  Non avviato", hdr);
    m_replStatus->setObjectName("hintLabel");
    hdrLay->addWidget(m_replStatus);

    outBtnRestart = new QPushButton("\xf0\x9f\x94\x84  Riavvia REPL", hdr);
    outBtnRestart->setObjectName("actionBtn");
    outBtnRestart->setToolTip(tr("Riavvia il processo Python (resetta tutte le variabili)"));
    hdrLay->addWidget(outBtnRestart);

    outBtnImport = new QPushButton("\xe2\x86\x90  Importa dall'editor", hdr);
    outBtnImport->setObjectName("actionBtn");
    outBtnImport->setToolTip(
        "Esegue nel REPL il codice presente nel tab \xf0\x9f\x92\xbb Programmazione");
    hdrLay->addWidget(outBtnImport);

    return hdr;
}

/* ── Gruppo output REPL ── */
QWidget* ProgrammazionePage::buildReplOutputGroup(QWidget* parent,
                                                    QPushButton*& outBtnClear)
{
    QFont monoFont;
    monoFont.setFamily("JetBrains Mono");
    monoFont.setStyleHint(QFont::Monospace);
    const int appPt = QApplication::font().pointSize();
    monoFont.setPointSize(appPt > 0 ? appPt : 10);

    auto* outGroup = new QGroupBox(
        "\xf0\x9f\x96\xa5  Output sessione Python", parent);
    outGroup->setObjectName("cardGroup");
    auto* outLay = new QVBoxLayout(outGroup);
    outLay->setContentsMargins(4, 8, 4, 4);
    outLay->setSpacing(4);

    m_replOutput = new QPlainTextEdit(outGroup);
    m_replOutput->setObjectName("chatLog");
    m_replOutput->setFont(monoFont);
    m_replOutput->setReadOnly(true);
    m_replOutput->setMaximumBlockCount(5000);
    m_replOutput->setPlaceholderText(
        "Premi \xf0\x9f\x94\x84 Riavvia REPL per avviare una sessione Python...");
    outLay->addWidget(m_replOutput, 1);

    outBtnClear = new QPushButton("\xf0\x9f\x97\x91  Pulisci output", outGroup);
    outBtnClear->setObjectName("actionBtn");
    outLay->addWidget(outBtnClear, 0, Qt::AlignLeft);

    return outGroup;
}

/* ── Riga input con prompt e cronologia ── */
QWidget* ProgrammazionePage::buildReplInputRow(QWidget* parent)
{
    QFont monoFont;
    monoFont.setFamily("JetBrains Mono");
    monoFont.setStyleHint(QFont::Monospace);
    const int appPt = QApplication::font().pointSize();
    monoFont.setPointSize(appPt > 0 ? appPt : 10);

    auto* inputRow = new QWidget(parent);
    auto* inputLay = new QHBoxLayout(inputRow);
    inputLay->setContentsMargins(0, 0, 0, 0);
    inputLay->setSpacing(6);

    auto* promptLbl = new QLabel(">>>", inputRow);
    promptLbl->setObjectName("cardDesc");
    promptLbl->setFont(monoFont);
    inputLay->addWidget(promptLbl);

    m_replInput = new QLineEdit(inputRow);
    m_replInput->setObjectName("chatInput");
    m_replInput->setFont(monoFont);
    m_replInput->setPlaceholderText(
        "Scrivi un'espressione Python e premi Invio...  "
        "\xe2\x86\x91\xe2\x86\x93 per la cronologia");
    m_replInput->setEnabled(false);
    inputLay->addWidget(m_replInput, 1);

    m_btnSendRepl = new QPushButton("Invia \xe2\x96\xb6", inputRow);
    m_btnSendRepl->setObjectName("actionBtn");
    m_btnSendRepl->setEnabled(false);
    inputLay->addWidget(m_btnSendRepl);

    return inputRow;
}

/* ── Connessioni REPL ── */
void ProgrammazionePage::setupReplConnections(QPushButton* btnRestart,
                                               QPushButton* btnImport,
                                               QPushButton* btnClearRepl)
{
    if (btnRestart)   connect(btnRestart,   &QPushButton::clicked, this, &ProgrammazionePage::onBtnReplRestartClicked);
    if (btnImport)    connect(btnImport,    &QPushButton::clicked, this, &ProgrammazionePage::onBtnReplImportClicked);
    if (btnClearRepl) connect(btnClearRepl, &QPushButton::clicked, this, &ProgrammazionePage::onBtnReplClearClicked);

    connect(m_btnSendRepl, &QPushButton::clicked,
            this, &ProgrammazionePage::sendReplLine);
    connect(m_replInput, &QLineEdit::returnPressed,
            this, &ProgrammazionePage::sendReplLine);
    connect(m_innerTabs, &QTabWidget::currentChanged,
            this, &ProgrammazionePage::onReplTabChanged);
}

/* ══════════════════════════════════════════════════════════════
   replStart — avvia (o riavvia) il processo Python interattivo.

   Usa python3 -u -i: -u = unbuffered, -i = forza interattivo.
   Output (stdout + stderr) viene inviato a m_replOutput via readyRead.
   ══════════════════════════════════════════════════════════════ */
void ProgrammazionePage::replStart()
{
    /* Ferma processo esistente */
    if (m_replProc && m_replProc->state() != QProcess::NotRunning) {
        m_replProc->kill();
        m_replProc->waitForFinished(2000);
        m_replProc->deleteLater();
        m_replProc = nullptr;
    }

    if (m_replOutput)
        m_replOutput->appendPlainText(
            "\n\xe2\x8f\xa9  Avvio sessione Python...\n");
    if (m_replStatus)
        m_replStatus->setText(tr("\xe2\x8f\xb3  Avvio..."));
    if (m_replInput) m_replInput->setEnabled(false);

    m_replProc = new QProcess(this);
    m_replProc->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_replProc, &QProcess::readyRead,
            this, &ProgrammazionePage::onReplReadyRead);

    connect(m_replProc, &QProcess::started,
            this, &ProgrammazionePage::onReplStarted);

    connect(m_replProc,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ProgrammazionePage::onReplFinished);

    connect(m_replProc, &QProcess::errorOccurred,
            this, &ProgrammazionePage::onReplErrorOccurred);

#ifdef _WIN32
    m_replProc->start("python", {"-u", "-i"});
#else
    m_replProc->start(P::findPython(), {"-u", "-i"});
#endif
}

/* ══════════════════════════════════════════════════════════════
   replSend — invia la riga di input al processo Python,
   aggiunge alla cronologia, pulisce il campo.
   ══════════════════════════════════════════════════════════════ */
void ProgrammazionePage::replSend()
{
    if (!m_replProc || m_replProc->state() != QProcess::Running) return;
    if (!m_replInput) return;

    const QString line = m_replInput->text();

    /* Cronologia (evita duplicati consecutivi) */
    if (!line.isEmpty()) {
        if (m_replHistory.isEmpty() || m_replHistory.last() != line)
            m_replHistory.append(line);
        m_replHistIdx = -1;
    }

    m_replInput->clear();
    m_replProc->write((line + "\n").toUtf8());
}

/* ======================================================================
   Sezione 9 — Git MCP slots
   ====================================================================== */

void ProgrammazionePage::populateGitModels()
{
    if (m_ai && m_gitAiModel) AiUtils::populateModelCombo(m_ai, m_gitAiModel, this);
}

void ProgrammazionePage::onBtnGitBrowseClicked()
{
    const QString d = QFileDialog::getExistingDirectory(
        this, "Scegli repository git",
        m_gitRepoPath ? m_gitRepoPath->text() : QDir::homePath());
    if (!d.isEmpty() && m_gitRepoPath)
        m_gitRepoPath->setText(d);
}

void ProgrammazionePage::onBtnGitStopClicked()
{
    if (m_gitProc && m_gitProc->state() != QProcess::NotRunning)
        m_gitProc->kill();
}

void ProgrammazionePage::onBtnClearGitClicked()
{
    if (m_gitOutput) m_gitOutput->clear();
}

void ProgrammazionePage::onBtnCloseGitAiClicked()
{
    if (m_gitAiPanel) m_gitAiPanel->hide();
}

void ProgrammazionePage::onBtnGitPullClicked()
{
    if (QMessageBox::question(this, "git pull",
            "Eseguire git pull?\n\n"
            "Le modifiche remote verranno unite al branch corrente.",
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;
    gitRun("pull");
}

void ProgrammazionePage::onBtnAddCommitClicked()
{
    const QString msg = m_gitCommitMsg ? m_gitCommitMsg->text().trimmed() : QString();
    if (msg.isEmpty()) {
        QMessageBox::warning(this, "Messaggio mancante",
            "Inserisci un messaggio di commit prima di procedere.");
        return;
    }
    if (QMessageBox::question(this, "Add + Commit",
            QString("Eseguire:\n  git add -A\n  git commit -m \"%1\"\n\n"
                    "Tutte le modifiche verranno staged e committate.").arg(msg),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    m_gitPendingCommit = msg;
    gitRun("add", {"-A"});
}

void ProgrammazionePage::onBtnPushClicked()
{
    if (QMessageBox::question(this, "git push",
            "Eseguire git push?\n\n"
            "I commit locali verranno inviati al repository remoto.",
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;
    gitRun("push");
}

void ProgrammazionePage::onBtnGitAiClicked()
{
    if (!m_gitAiPanel) return;
    m_gitAiPanel->show();
    const QString context = m_gitOutput ? m_gitOutput->toPlainText() : QString();
    gitAiRequest(
        "Analizza l'output git, spiega cosa rappresenta e suggerisci "
        "le prossime azioni da eseguire.",
        context);
}

void ProgrammazionePage::onBtnGenCommitClicked()
{
    if (!m_gitAiPanel) return;
    m_gitAiPanel->show();

    /* Prima esegue diff --staged per avere il contesto completo */
    const QString context = m_gitOutput ? m_gitOutput->toPlainText() : QString();
    gitAiRequest(
        "Genera un messaggio di commit convenzionale (Conventional Commits) "
        "per le modifiche mostrate nell'output git diff --staged. "
        "Formato: <tipo>(<scope>): <descrizione breve>\n\n"
        "Wrap il messaggio tra [COMMIT] e [/COMMIT] in modo che io possa "
        "estrarlo automaticamente.",
        context);
}

void ProgrammazionePage::onBtnGitStatusClicked()     { gitRun("status"); }
void ProgrammazionePage::onBtnGitDiffClicked()       { gitRun("diff"); }
void ProgrammazionePage::onBtnGitDiffStagedClicked() { gitRun("diff", {"--cached"}); }
void ProgrammazionePage::onBtnGitLogClicked()        { gitRun("log", {"--oneline", "-20"}); }
void ProgrammazionePage::onBtnGitBranchClicked()     { gitRun("branch", {"-a"}); }

void ProgrammazionePage::onGitReadyRead()
{
    if (!m_gitProc) return;
    const QString out = QString::fromLocal8Bit(m_gitProc->readAll());
    if (m_gitOutput) m_gitOutput->appendPlainText(out);
}

void ProgrammazionePage::onGitFinished(int exitCode, QProcess::ExitStatus /*status*/)
{
    if (m_gitActRow)  m_gitActRow->setEnabled(true);
    if (m_btnGitStop) m_btnGitStop->setEnabled(false);

    if (m_gitOutput)
        m_gitOutput->appendPlainText(
            exitCode == 0
            ? "\n\xe2\x9c\x85  Operazione completata.\n"
            : QString("\n\xe2\x9d\x8c  Exit code: %1\n").arg(exitCode));

    /* Auto-commit dopo add -A riuscito */
    if (!m_gitPendingCommit.isEmpty() && exitCode == 0) {
        const QString msg = m_gitPendingCommit;
        m_gitPendingCommit.clear();
        gitRun("commit", {"-m", msg});
    }
}

void ProgrammazionePage::onGitErrorOccurred(QProcess::ProcessError err)
{
    Q_UNUSED(err)
    if (m_gitActRow)  m_gitActRow->setEnabled(true);
    if (m_btnGitStop) m_btnGitStop->setEnabled(false);
    if (m_gitOutput)
        m_gitOutput->appendPlainText(
            "\xe2\x9d\x8c  Impossibile avviare il processo git. "
            "Verifica che git sia installato e nel PATH.\n");
}

void ProgrammazionePage::onGitAiToken(const QString& tok)
{
    if (!m_gitAiOutput) return;
    m_gitAiOutput->moveCursor(QTextCursor::End);
    m_gitAiOutput->insertPlainText(tok);
    m_gitAiOutput->ensureCursorVisible();
}

void ProgrammazionePage::onGitAiFinished(const QString& full)
{
    disconnect(m_gitAiTokenConn);
    disconnect(m_gitAiFinishedConn);
    disconnect(m_gitAiErrorConn);
    /* Se la risposta contiene [COMMIT]...[/COMMIT], popola il campo commit msg */
    static const QRegularExpression reCommit(
        R"(\[COMMIT\]([\s\S]*?)\[/COMMIT\])");
    const auto m = reCommit.match(full);
    if (m.hasMatch() && m_gitCommitMsg) {
        const QString msg = m.captured(1).trimmed();
        if (!msg.isEmpty()) m_gitCommitMsg->setText(msg);
    }
}

void ProgrammazionePage::onGitAiError(const QString& msg)
{
    disconnect(m_gitAiTokenConn);
    disconnect(m_gitAiFinishedConn);
    disconnect(m_gitAiErrorConn);
    if (m_gitAiOutput) {
        m_gitAiOutput->moveCursor(QTextCursor::End);
        m_gitAiOutput->insertPlainText(
            QString("\n\xe2\x9d\x8c  Errore: %1").arg(msg));
    }
}

/* ======================================================================
   Sezione 10 — Python REPL slots
   ====================================================================== */

void ProgrammazionePage::onReplReadyRead()
{
    if (!m_replProc) return;
    const QString out = QString::fromLocal8Bit(m_replProc->readAll());
    if (m_replOutput) {
        m_replOutput->moveCursor(QTextCursor::End);
        m_replOutput->insertPlainText(out);
        m_replOutput->ensureCursorVisible();
    }
}

void ProgrammazionePage::onReplStarted()
{
    if (m_replStatus)
        m_replStatus->setText(tr("\xe2\x9c\x85  Sessione attiva"));
    if (m_replInput) {
        m_replInput->setEnabled(true);
        m_replInput->setFocus();
    }
    if (m_btnSendRepl) m_btnSendRepl->setEnabled(true);
}

void ProgrammazionePage::onReplFinished(int code, QProcess::ExitStatus /*status*/)
{
    if (m_replStatus)
        m_replStatus->setText(
            code == 0
            ? "\xe2\xac\x9c  REPL terminato"
            : QString("\xe2\x9d\x8c  REPL uscito (code %1)").arg(code));
    if (m_replInput)   m_replInput->setEnabled(false);
    if (m_btnSendRepl) m_btnSendRepl->setEnabled(false);
    if (m_replOutput)
        m_replOutput->appendPlainText(
            "\n\xe2\x80\x94\xe2\x80\x94  Python REPL terminato  \xe2\x80\x94\xe2\x80\x94\n");
    if (auto* p = qobject_cast<QProcess*>(sender())) p->deleteLater();
    m_replProc = nullptr;
}

void ProgrammazionePage::onReplErrorOccurred(QProcess::ProcessError err)
{
    if (err != QProcess::FailedToStart) return;
    if (m_replStatus)
        m_replStatus->setText(tr("\xe2\x9d\x8c  python3 non trovato"));
    if (m_replOutput)
        m_replOutput->appendPlainText(
            "\xe2\x9d\x8c  python3 non trovato nel PATH. "
            "Installa Python 3.\n");
    if (m_replInput)   m_replInput->setEnabled(false);
    if (m_btnSendRepl) m_btnSendRepl->setEnabled(false);
    if (auto* p = qobject_cast<QProcess*>(sender())) p->deleteLater();
    m_replProc = nullptr;
}

void ProgrammazionePage::onReplTabChanged(int idx)
{
    /* Controlla se il widget del tab corrente contiene m_replOutput.
       La gerarchia e': replOutput -> outGroup -> w (QWidget).
       m_innerTabs->widget(idx) == w */
    if (!m_replOutput) return;
    /* Naviga 3 livelli su: m_replOutput->parent = outGroup,
       outGroup->parent = w, w e' il widget del tab */
    QWidget* replWidget =
        m_replOutput->parentWidget()   /* outGroup */
            ? qobject_cast<QWidget*>(m_replOutput->parentWidget()->parent())  /* w */
            : nullptr;
    if (!replWidget) return;
    if (m_innerTabs->widget(idx) != replWidget) return;
    if (!m_replProc || m_replProc->state() == QProcess::NotRunning)
        replStart();
}

void ProgrammazionePage::onBtnReplRestartClicked()
{
    replStart();
}

void ProgrammazionePage::onBtnReplClearClicked()
{
    if (m_replOutput) m_replOutput->clear();
}

void ProgrammazionePage::onBtnReplImportClicked()
{
    if (!m_replProc || m_replProc->state() != QProcess::Running) {
        if (m_replOutput)
            m_replOutput->appendPlainText(
                "\xe2\x9d\x8c  Avvia prima il REPL con "
                "\xf0\x9f\x94\x84 Riavvia.\n");
        return;
    }
    if (!m_editor) return;
    const QString code = m_editor->toPlainText().trimmed();
    if (code.isEmpty()) return;

    /* Scrivi su file temp, poi exec() nel REPL */
    const QString tmp = QStandardPaths::writableLocation(
        QStandardPaths::TempLocation) + "/prismalux_repl_import.py";
    QFile f(tmp);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    f.write(code.toUtf8());
    f.close();

    if (m_replOutput)
        m_replOutput->appendPlainText("\n# === Importa da editor ===\n");
    /* exec(open(...).read()) e' piu' robusto di exec() su stringa multiline */
    const QString cmd = QString("exec(open(r'%1').read())\n")
                        .arg(QDir::toNativeSeparators(tmp).replace('\\', '/'));
    m_replProc->write(cmd.toUtf8());
}

void ProgrammazionePage::sendReplLine()
{
    if (!m_replProc || m_replProc->state() != QProcess::Running) return;
    replSend();
}

