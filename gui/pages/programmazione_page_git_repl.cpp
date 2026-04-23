/* ══════════════════════════════════════════════════════════════
   programmazione_page_git_repl.cpp

   Implementazioni per i sub-tab Git MCP e Python REPL.
   Fanno parte di ProgrammazionePage (dichiarazioni in programmazione_page.h).
   ══════════════════════════════════════════════════════════════ */
#include "programmazione_page.h"
#include "../prismalux_paths.h"

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
#include <QStandardPaths>
#include <QKeyEvent>
#include <QDir>

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

   Operazioni git con assistenza AI:
   - Status / Diff / Log / Branch — lettura sicura, nessuna conferma
   - Add + Commit — richiede messaggio e conferma dialogo
   - Pull / Push — richiede conferma dialogo
   - AI panel: analizza output / genera commit message automaticamente
   ══════════════════════════════════════════════════════════════ */
QWidget* ProgrammazionePage::buildGitMcp(QWidget* parent)
{
    QFont monoFont;
    monoFont.setFamily("JetBrains Mono");
    monoFont.setStyleHint(QFont::Monospace);
    monoFont.setPointSize(10);

    auto* w   = new QWidget(parent);
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(12, 12, 12, 12);
    lay->setSpacing(8);

    /* Descrizione */
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

    /* ── Repo path ── */
    auto* repoRow = new QWidget(w);
    auto* repoLay = new QHBoxLayout(repoRow);
    repoLay->setContentsMargins(0, 0, 0, 0);
    repoLay->setSpacing(8);
    repoLay->addWidget(new QLabel("Repository:", repoRow));

    m_gitRepoPath = new QLineEdit(repoRow);
    m_gitRepoPath->setObjectName("chatInput");
    m_gitRepoPath->setText(QDir::cleanPath(P::root() + "/.."));
    m_gitRepoPath->setPlaceholderText("/percorso/repository");
    repoLay->addWidget(m_gitRepoPath, 1);

    auto* btnBrowse = new QPushButton("\xf0\x9f\x93\x82", repoRow);
    btnBrowse->setObjectName("actionBtn");
    btnBrowse->setFixedWidth(34);
    btnBrowse->setToolTip("Scegli cartella repository");
    repoLay->addWidget(btnBrowse);

    lay->addWidget(repoRow);

    /* ── Pulsanti azioni rapide ── */
    m_gitActRow = new QWidget(w);
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

    auto* btnStatus = mkBtn("\xf0\x9f\x93\x8b  Status", "git status");
    auto* btnDiff   = mkBtn("\xf0\x9f\x94\x8d  Diff",   "git diff (modifiche non staged)");
    auto* btnDiffSt = mkBtn("\xf0\x9f\x94\x8d  Diff staged", "git diff --staged");
    auto* btnLog    = mkBtn("\xf0\x9f\x93\x9c  Log",    "git log --oneline -20");
    auto* btnBranch = mkBtn("\xf0\x9f\x8c\xbf  Branch", "git branch -a");
    auto* btnPull   = mkBtn("\xe2\xac\x87  Pull",       "git pull (richiede conferma)");
    actLay->addStretch(1);

    m_btnGitStop = new QPushButton("\xe2\x96\xa0  Stop", m_gitActRow);
    m_btnGitStop->setObjectName("actionBtn");
    m_btnGitStop->setProperty("danger", "true");
    m_btnGitStop->setEnabled(false);
    actLay->addWidget(m_btnGitStop);

    lay->addWidget(m_gitActRow);

    /* ── Commit row ── */
    auto* commitRow = new QWidget(w);
    auto* commitLay = new QHBoxLayout(commitRow);
    commitLay->setContentsMargins(0, 0, 0, 0);
    commitLay->setSpacing(6);

    m_gitCommitMsg = new QLineEdit(commitRow);
    m_gitCommitMsg->setObjectName("chatInput");
    m_gitCommitMsg->setPlaceholderText("Messaggio di commit...");
    commitLay->addWidget(m_gitCommitMsg, 1);

    auto* btnAddCommit = new QPushButton(
        "\xe2\x9c\x85  Add + Commit", commitRow);
    btnAddCommit->setObjectName("actionBtn");
    btnAddCommit->setProperty("highlight", "true");
    btnAddCommit->setToolTip("git add -A  →  git commit -m \"...\"");
    commitLay->addWidget(btnAddCommit);

    auto* btnPush = new QPushButton("\xe2\xac\x86  Push", commitRow);
    btnPush->setObjectName("actionBtn");
    btnPush->setToolTip("git push (richiede conferma)");
    commitLay->addWidget(btnPush);

    lay->addWidget(commitRow);

    /* ── Output git ── */
    auto* outGroup = new QGroupBox(
        "\xf0\x9f\x96\xa5  Output git", w);
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
        "L'output dei comandi git appari\xc3\xa0 qui.\n\n"
        "Premi Status per iniziare.");
    outLay->addWidget(m_gitOutput, 1);

    /* Sub-toolbar output */
    auto* outActRow = new QWidget(outGroup);
    auto* outActLay = new QHBoxLayout(outActRow);
    outActLay->setContentsMargins(0, 2, 0, 0);
    outActLay->setSpacing(8);

    auto* btnClearGit = new QPushButton(
        "\xf0\x9f\x97\x91  Pulisci", outActRow);
    btnClearGit->setObjectName("actionBtn");
    outActLay->addWidget(btnClearGit);

    auto* btnGitAi = new QPushButton(
        "\xf0\x9f\xa4\x96  Analizza con AI", outActRow);
    btnGitAi->setObjectName("actionBtn");
    btnGitAi->setToolTip("L'AI spiega l'output e suggerisce le prossime operazioni");
    outActLay->addWidget(btnGitAi);

    auto* btnGenCommit = new QPushButton(
        "\xe2\x9c\x8f  Genera commit msg", outActRow);
    btnGenCommit->setObjectName("actionBtn");
    btnGenCommit->setProperty("highlight", "true");
    btnGenCommit->setToolTip(
        "Esegue git diff --staged, poi chiede all'AI di scrivere "
        "un messaggio di commit convenzionale");
    outActLay->addWidget(btnGenCommit);

    outActLay->addStretch(1);
    outLay->addWidget(outActRow);
    lay->addWidget(outGroup, 1);

    /* ── AI panel (nascosto di default) ── */
    m_gitAiPanel = new QWidget(w);
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
    auto* btnRefGit = new QPushButton("\xf0\x9f\x94\x84", aiModRow);
    btnRefGit->setObjectName("actionBtn");
    btnRefGit->setFixedWidth(32);
    aiModLay->addWidget(btnRefGit);
    auto* btnCloseGitAi = new QPushButton("\xe2\x9c\x95", aiModRow);
    btnCloseGitAi->setObjectName("actionBtn");
    btnCloseGitAi->setFixedWidth(32);
    aiModLay->addWidget(btnCloseGitAi);
    aiLay->addWidget(aiModRow);

    m_gitAiOutput = new QPlainTextEdit(m_gitAiPanel);
    m_gitAiOutput->setObjectName("chatLog");
    m_gitAiOutput->setFont(monoFont);
    m_gitAiOutput->setReadOnly(true);
    m_gitAiOutput->setMaximumBlockCount(2000);
    m_gitAiOutput->setMaximumHeight(200);
    m_gitAiOutput->setPlaceholderText(
        "L'analisi AI appari\xc3\xa0 qui...");
    aiLay->addWidget(m_gitAiOutput);

    m_gitAiPanel->hide();
    lay->addWidget(m_gitAiPanel);

    /* ══════ Connessioni ══════ */

    connect(btnBrowse, &QPushButton::clicked, this, [this]{
        const QString d = QFileDialog::getExistingDirectory(
            this, "Scegli repository git", m_gitRepoPath->text());
        if (!d.isEmpty()) m_gitRepoPath->setText(d);
    });

    connect(m_btnGitStop, &QPushButton::clicked, this, [this]{
        if (m_gitProc && m_gitProc->state() != QProcess::NotRunning)
            m_gitProc->kill();
    });

    connect(btnClearGit, &QPushButton::clicked, this, [this]{
        m_gitOutput->clear();
    });

    connect(btnCloseGitAi, &QPushButton::clicked, this, [this]{
        m_gitAiPanel->hide();
    });

    /* Popola modelli AI */
    auto populateGitModels = [this]() {
        if (!m_ai) return;
        m_gitAiModel->setEnabled(false);
        const QString cur = m_ai->model();
        auto* holder = new QObject(this);
        connect(m_ai, &AiClient::modelsReady, holder,
                [this, holder, cur](const QStringList& list) {
            holder->deleteLater();
            m_gitAiModel->clear();
            for (const QString& mdl : list) {
                const QString n = mdl.toLower();
                if (n.contains("embed") || n.contains("minilm") ||
                    n.contains("rerank") || n.contains("bge-"))
                    continue;
                m_gitAiModel->addItem(mdl, mdl);
            }
            if (m_gitAiModel->count() == 0)
                m_gitAiModel->addItem(cur.isEmpty() ? "(nessun modello)" : cur, cur);
            else {
                const int idx = m_gitAiModel->findData(cur);
                if (idx >= 0) m_gitAiModel->setCurrentIndex(idx);
            }
            m_gitAiModel->setEnabled(true);
        });
        m_ai->fetchModels();
    };

    connect(btnRefGit, &QPushButton::clicked, this, populateGitModels);
    QTimer::singleShot(0, this, [populateGitModels]{ populateGitModels(); });

    /* Azioni git rapide — lettura, nessuna conferma */
    connect(btnStatus, &QPushButton::clicked, this,
            [this]{ gitRun("status"); });
    connect(btnDiff,   &QPushButton::clicked, this,
            [this]{ gitRun("diff"); });
    connect(btnDiffSt, &QPushButton::clicked, this,
            [this]{ gitRun("diff", {"--staged"}); });
    connect(btnLog,    &QPushButton::clicked, this,
            [this]{ gitRun("log", {"--oneline", "-20"}); });
    connect(btnBranch, &QPushButton::clicked, this,
            [this]{ gitRun("branch", {"-a"}); });

    /* Pull — con conferma */
    connect(btnPull, &QPushButton::clicked, this, [this]{
        QMessageBox dlg(this);
        dlg.setWindowTitle("Conferma pull");
        dlg.setIcon(QMessageBox::Question);
        dlg.setText("<b>git pull</b><br><br>"
                    "Scarica e integra i commit dal remote. Procedere?");
        dlg.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
        dlg.button(QMessageBox::Ok)->setText("Pull");
        if (dlg.exec() == QMessageBox::Ok) gitRun("pull");
    });

    /* Add + Commit — con conferma */
    connect(btnAddCommit, &QPushButton::clicked, this, [this]{
        const QString msg = m_gitCommitMsg->text().trimmed();
        if (msg.isEmpty()) {
            m_gitOutput->appendPlainText(
                "\xe2\x9d\x8c  Scrivi un messaggio di commit prima di procedere.\n");
            return;
        }
        QMessageBox dlg(this);
        dlg.setWindowTitle("Conferma add + commit");
        dlg.setIcon(QMessageBox::Question);
        dlg.setText(QString(
            "<b>git add -A</b><br>"
            "<b>git commit -m \"%1\"</b><br><br>"
            "Procedere?").arg(msg.toHtmlEscaped()));
        dlg.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
        dlg.button(QMessageBox::Ok)->setText("Conferma");
        if (dlg.exec() != QMessageBox::Ok) return;

        /* Catena: add -A → finished → commit -m msg */
        m_gitPendingCommit = msg;
        gitRun("add", {"-A"});
    });

    /* Push — con conferma */
    connect(btnPush, &QPushButton::clicked, this, [this]{
        QMessageBox dlg(this);
        dlg.setWindowTitle("Conferma push");
        dlg.setIcon(QMessageBox::Warning);
        dlg.setText("<b>git push</b><br><br>"
                    "Invier\xc3\xa0 i commit al remote. Procedere?");
        dlg.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
        dlg.button(QMessageBox::Ok)->setText("Push");
        if (dlg.exec() == QMessageBox::Ok) gitRun("push");
    });

    /* Analizza output con AI */
    connect(btnGitAi, &QPushButton::clicked, this, [this, populateGitModels]{
        m_gitAiPanel->show();
        if (m_gitAiModel->count() <= 1) populateGitModels();
        const QString ctx = m_gitOutput->toPlainText().trimmed();
        if (ctx.isEmpty()) {
            m_gitAiOutput->setPlainText(
                "\xe2\x9d\x8c  Nessun output git. "
                "Premi Status o Diff prima di analizzare.");
            return;
        }
        gitAiRequest(
            "Analizza questo output git. Spiega cosa sta succedendo "
            "e suggerisci le prossime operazioni consigliate.", ctx);
    });

    /* Genera commit message dall'AI */
    connect(btnGenCommit, &QPushButton::clicked, this, [this, populateGitModels]{
        m_gitAiPanel->show();
        if (m_gitAiModel->count() <= 1) populateGitModels();
        /* Prima otteniamo il diff staged, poi chiediamo all'AI di generare il msg.
           Usiamo un processo separato per non intralciare m_gitProc. */
        if (!m_gitRepoPath) return;
        const QString repo = m_gitRepoPath->text().trimmed();
        if (repo.isEmpty()) return;

        auto* tmpProc = new QProcess(this);
        tmpProc->setProcessChannelMode(QProcess::MergedChannels);
        tmpProc->setWorkingDirectory(repo);

        connect(tmpProc,
                QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, tmpProc](int, QProcess::ExitStatus){
            const QString diff = QString::fromLocal8Bit(tmpProc->readAll()).trimmed();
            tmpProc->deleteLater();

            const QString ctx = diff.isEmpty()
                ? "(nessuna modifica staged — usa git add prima)"
                : diff.left(4000);
            gitAiRequest(
                "Scrivi un messaggio di commit convenzionale (Conventional Commits) "
                "per questo diff. Formato: <type>(<scope>): <descrizione breve>. "
                "Poi metti il messaggio finale tra [COMMIT] e [/COMMIT].", ctx);
        });
        tmpProc->start("git", {"-C", repo, "diff", "--staged"});
    });

    return w;
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
        return;
    }

    /* Abbandona processo precedente */
    if (m_gitProc && m_gitProc->state() != QProcess::NotRunning) {
        m_gitProc->kill();
        m_gitProc->waitForFinished(2000);
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

    connect(m_gitProc, &QProcess::readyRead, this, [this]{
        if (!m_gitProc) return;
        const QString out = QString::fromLocal8Bit(m_gitProc->readAll());
        if (m_gitOutput) {
            m_gitOutput->moveCursor(QTextCursor::End);
            m_gitOutput->insertPlainText(out);
            m_gitOutput->ensureCursorVisible();
        }
    });

    connect(m_gitProc,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus status){
        if (auto* p = qobject_cast<QProcess*>(sender())) p->deleteLater();
        m_gitProc = nullptr;
        if (m_gitActRow)  m_gitActRow->setEnabled(true);
        if (m_btnGitStop) m_btnGitStop->setEnabled(false);

        /* Catena add → commit */
        if (!m_gitPendingCommit.isEmpty()) {
            if (exitCode == 0 && status == QProcess::NormalExit) {
                const QString msg = m_gitPendingCommit;
                m_gitPendingCommit.clear();
                gitRun("commit", {"-m", msg});
            } else {
                m_gitPendingCommit.clear();
                if (m_gitOutput)
                    m_gitOutput->appendPlainText(
                        "\xe2\x9d\x8c  git add fallito — commit annullato.\n");
            }
        }
    });

    connect(m_gitProc, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError err){
        if (err != QProcess::FailedToStart) return;
        if (m_gitOutput)
            m_gitOutput->appendPlainText(
                "\xe2\x9d\x8c  git non trovato nel PATH. "
                "Installa git e riprova.\n");
        if (m_gitActRow)  m_gitActRow->setEnabled(true);
        if (m_btnGitStop) m_btnGitStop->setEnabled(false);
        m_gitPendingCommit.clear();
        if (auto* p = qobject_cast<QProcess*>(sender())) p->deleteLater();
        m_gitProc = nullptr;
    });

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

    if (m_gitTokenHolder) { delete m_gitTokenHolder; m_gitTokenHolder = nullptr; }
    m_gitTokenHolder = new QObject(this);

    connect(m_ai, &AiClient::token, m_gitTokenHolder, [this](const QString& tok){
        m_gitAiOutput->moveCursor(QTextCursor::End);
        m_gitAiOutput->insertPlainText(tok);
        m_gitAiOutput->ensureCursorVisible();
    });
    connect(m_ai, &AiClient::finished, m_gitTokenHolder,
            [this](const QString& full){
        if (m_gitTokenHolder) { delete m_gitTokenHolder; m_gitTokenHolder = nullptr; }
        /* Se la risposta contiene [COMMIT]...[/COMMIT], popola il campo commit msg */
        static const QRegularExpression reCommit(
            R"(\[COMMIT\]([\s\S]*?)\[/COMMIT\])");
        const auto m = reCommit.match(full);
        if (m.hasMatch() && m_gitCommitMsg) {
            const QString msg = m.captured(1).trimmed();
            if (!msg.isEmpty()) m_gitCommitMsg->setText(msg);
        }
    });
    connect(m_ai, &AiClient::error, m_gitTokenHolder,
            [this](const QString& msg){
        if (m_gitTokenHolder) { delete m_gitTokenHolder; m_gitTokenHolder = nullptr; }
        m_gitAiOutput->moveCursor(QTextCursor::End);
        m_gitAiOutput->insertPlainText(
            QString("\n\xe2\x9d\x8c  Errore: %1").arg(msg));
    });

    m_ai->chat(sys, user);
}

/* ══════════════════════════════════════════════════════════════
   buildPythonRepl — sub-tab "🐍 REPL"

   Sessione Python persistente tramite QProcess: le variabili
   definite in un'esecuzione rimangono disponibili nelle successive.
   Supporta frecce su/giù per la cronologia dei comandi.
   ══════════════════════════════════════════════════════════════ */
QWidget* ProgrammazionePage::buildPythonRepl(QWidget* parent)
{
    QFont monoFont;
    monoFont.setFamily("JetBrains Mono");
    monoFont.setStyleHint(QFont::Monospace);
    monoFont.setPointSize(10);

    auto* w   = new QWidget(parent);
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(12, 12, 12, 12);
    lay->setSpacing(8);

    /* ── Header ── */
    auto* hdr = new QWidget(w);
    auto* hdrLay = new QHBoxLayout(hdr);
    hdrLay->setContentsMargins(0, 0, 0, 0);
    hdrLay->setSpacing(10);

    auto* desc = new QLabel(
        "\xf0\x9f\x90\x8d  <b>Python REPL</b> \xe2\x80\x94 "
        "Sessione interattiva persistente. Le variabili rimangono tra un'esecuzione e l'altra.", w);
    desc->setObjectName("hintLabel");
    desc->setWordWrap(true);
    hdrLay->addWidget(desc, 1);

    m_replStatus = new QLabel("\xe2\x9d\x8c  Non avviato", hdr);
    m_replStatus->setObjectName("hintLabel");
    hdrLay->addWidget(m_replStatus);

    auto* btnRestart = new QPushButton("\xf0\x9f\x94\x84  Riavvia REPL", hdr);
    btnRestart->setObjectName("actionBtn");
    btnRestart->setToolTip("Riavvia il processo Python (resetta tutte le variabili)");
    hdrLay->addWidget(btnRestart);

    auto* btnImport = new QPushButton(
        "\xe2\x86\x90  Importa dall'editor", hdr);
    btnImport->setObjectName("actionBtn");
    btnImport->setToolTip(
        "Esegue nel REPL il codice presente nel tab \xf0\x9f\x92\xbb Programmazione");
    hdrLay->addWidget(btnImport);

    lay->addWidget(hdr);

    auto* sep = new QFrame(w);
    sep->setFrameShape(QFrame::HLine);
    sep->setObjectName("sidebarSep");
    lay->addWidget(sep);

    /* ── Output REPL ── */
    auto* outGroup = new QGroupBox(
        "\xf0\x9f\x96\xa5  Output sessione Python", w);
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

    auto* btnClearRepl = new QPushButton(
        "\xf0\x9f\x97\x91  Pulisci output", outGroup);
    btnClearRepl->setObjectName("actionBtn");
    outLay->addWidget(btnClearRepl, 0, Qt::AlignLeft);

    lay->addWidget(outGroup, 1);

    /* ── Input line ── */
    auto* inputRow = new QWidget(w);
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

    auto* btnSendRepl = new QPushButton("Invia \xe2\x96\xb6", inputRow);
    btnSendRepl->setObjectName("actionBtn");
    btnSendRepl->setEnabled(false);
    inputLay->addWidget(btnSendRepl);

    lay->addWidget(inputRow);

    /* Cronologia con frecce su/giù */
    m_replInput->installEventFilter(
        new ReplHistFilter(m_replInput, m_replHistory, m_replHistIdx, this));

    /* ══════ Connessioni ══════ */

    connect(btnRestart, &QPushButton::clicked, this, [this]{ replStart(); });

    connect(btnClearRepl, &QPushButton::clicked, this, [this]{
        m_replOutput->clear();
    });

    /* Invia riga */
    auto sendLine = [this, btnSendRepl]{
        if (!m_replProc || m_replProc->state() != QProcess::Running) return;
        replSend();
    };
    connect(btnSendRepl, &QPushButton::clicked, this, sendLine);
    connect(m_replInput, &QLineEdit::returnPressed, this, sendLine);

    /* Abilita/disabilita input in base all'avvio */
    auto setReplInputEnabled = [this, btnSendRepl](bool on){
        if (m_replInput)  m_replInput->setEnabled(on);
        if (btnSendRepl)  btnSendRepl->setEnabled(on);
        if (on && m_replInput) m_replInput->setFocus();
    };

    /* Importa codice dall'editor nel REPL */
    connect(btnImport, &QPushButton::clicked, this, [this]{
        if (!m_replProc || m_replProc->state() != QProcess::Running) {
            m_replOutput->appendPlainText(
                "\xe2\x9d\x8c  Avvia prima il REPL con \xf0\x9f\x94\x84 Riavvia.\n");
            return;
        }
        const QString code = m_editor->toPlainText().trimmed();
        if (code.isEmpty()) return;

        /* Scrivi su file temp, poi exec() nel REPL */
        const QString tmp = QStandardPaths::writableLocation(
            QStandardPaths::TempLocation) + "/prismalux_repl_import.py";
        QFile f(tmp);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return;
        f.write(code.toUtf8());
        f.close();

        m_replOutput->appendPlainText(
            "\n# === Importa da editor ===\n");
        /* exec(open(...).read()) è più robusto di exec() su stringa multiline */
        const QString cmd = QString("exec(open(r'%1').read())\n")
                            .arg(QDir::toNativeSeparators(tmp).replace('\\', '/'));
        m_replProc->write(cmd.toUtf8());
    });

    /* Avvia REPL automaticamente quando il tab viene mostrato la prima volta */
    connect(m_innerTabs, &QTabWidget::currentChanged, this,
            [this, setReplInputEnabled](int idx){
        if (m_innerTabs->widget(idx) != m_replOutput->parentWidget()->parentWidget()->parentWidget())
            return;
        if (!m_replProc || m_replProc->state() == QProcess::NotRunning)
            replStart();
    });

    /* Collega enable/disable input ai segnali di replStart (definiti in replStart()) */
    /* Il collegamento avviene in replStart() stesso — vedi sotto */
    Q_UNUSED(setReplInputEnabled);

    return w;
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
        m_replStatus->setText("\xe2\x8f\xb3  Avvio...");
    if (m_replInput) m_replInput->setEnabled(false);

    m_replProc = new QProcess(this);
    m_replProc->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_replProc, &QProcess::readyRead, this, [this]{
        if (!m_replProc) return;
        const QString out = QString::fromLocal8Bit(m_replProc->readAll());
        if (m_replOutput) {
            m_replOutput->moveCursor(QTextCursor::End);
            m_replOutput->insertPlainText(out);
            m_replOutput->ensureCursorVisible();
        }
    });

    connect(m_replProc, &QProcess::started, this, [this]{
        if (m_replStatus)
            m_replStatus->setText("\xe2\x9c\x85  Sessione attiva");
        if (m_replInput) {
            m_replInput->setEnabled(true);
            m_replInput->setFocus();
        }
    });

    connect(m_replProc,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int code, QProcess::ExitStatus){
        if (m_replStatus)
            m_replStatus->setText(
                code == 0 ? "\xe2\xac\x9c  REPL terminato"
                          : QString("\xe2\x9d\x8c  REPL uscito (code %1)").arg(code));
        if (m_replInput) m_replInput->setEnabled(false);
        if (m_replOutput)
            m_replOutput->appendPlainText(
                "\n\xe2\x80\x94\xe2\x80\x94  Python REPL terminato  \xe2\x80\x94\xe2\x80\x94\n");
        if (auto* p = qobject_cast<QProcess*>(sender())) p->deleteLater();
        m_replProc = nullptr;
    });

    connect(m_replProc, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError err){
        if (err != QProcess::FailedToStart) return;
        if (m_replStatus)
            m_replStatus->setText("\xe2\x9d\x8c  python3 non trovato");
        if (m_replOutput)
            m_replOutput->appendPlainText(
                "\xe2\x9d\x8c  python3 non trovato nel PATH. "
                "Installa Python 3.\n");
        if (m_replInput) m_replInput->setEnabled(false);
        if (auto* p = qobject_cast<QProcess*>(sender())) p->deleteLater();
        m_replProc = nullptr;
    });

#ifdef _WIN32
    m_replProc->start("python", {"-u", "-i"});
#else
    m_replProc->start("python3", {"-u", "-i"});
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
