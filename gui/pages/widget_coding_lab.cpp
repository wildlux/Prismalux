#include "widget_coding_lab.h"
#include "../prismalux_paths.h"
namespace P = PrismaluxPaths;
#include <QScrollBar>
#include <QTemporaryFile>
#include <QDir>
#include <QRegularExpression>
#include <QFont>
#include <QTimer>
#include <QFile>

/* ══════════════════════════════════════════════════════════════
   Costruttore
   ══════════════════════════════════════════════════════════════ */
CodingLabWidget::CodingLabWidget(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 10, 12, 10);
    root->setSpacing(8);

    /* ── Titolo ── */
    auto* titleLbl = new QLabel(
        "\xf0\x9f\xa7\xaa <b>Lab Coding</b> \xe2\x86\x92"
        " <i>\xe2\x80\x9c" "Fammi vedere il risultato adesso\xe2\x80\x9d</i>"
        " \xe2\x80\x94 esegue e mostra",
        this);
    titleLbl->setWordWrap(true);
    root->addWidget(titleLbl);

    /* ── Descrizione task ── */
    m_descInput = new QTextEdit(this);
    m_descInput->setPlaceholderText(
        tr("Es: \"Fammi un programma che calcola i numeri primi fino a 100\"\n"
        "    \"Ordina questa lista di nomi in ordine alfabetico\"\n"
        "    \"Scrivi una funzione che controlla se una stringa è palindroma\""));
    m_descInput->setMaximumHeight(90);
    QFont f = m_descInput->font();
    f.setPointSize(12);
    m_descInput->setFont(f);
    root->addWidget(m_descInput);

    /* ── Tasti azione ── */
    auto* btnRow = new QHBoxLayout;
    m_runBtn   = new QPushButton(tr("\xf0\x9f\x9a\x80  Genera & Testa"), this);
    m_abortBtn = new QPushButton(tr("\xe2\x9c\x96  Ferma"), this);
    m_runBtn->setMinimumHeight(40);
    m_abortBtn->setMinimumHeight(40);
    m_runBtn->setStyleSheet(
        "QPushButton { background:#2563eb; color:white; border-radius:6px;"
        " font-size:13pt; font-weight:bold; padding:0 16px; }"
        "QPushButton:hover { background:#1d4ed8; }");
    m_abortBtn->setStyleSheet(
        "QPushButton { background:#64748b; color:white; border-radius:6px; padding:0 12px; }"
        "QPushButton:hover { background:#475569; }");
    m_abortBtn->setEnabled(false);
    btnRow->addWidget(m_runBtn, 3);
    btnRow->addWidget(m_abortBtn, 1);
    root->addLayout(btnRow);

    /* ── Fase corrente ── */
    m_phaseLbl = new QLabel(tr("Pronto."), this);
    m_phaseLbl->setStyleSheet("color:#64748b; font-style:italic;");
    root->addWidget(m_phaseLbl);

    /* ── Area risultato (nascosta fino alla fine) ── */
    m_resultArea = new QFrame(this);
    m_resultArea->setFrameShape(QFrame::StyledPanel);
    m_resultArea->setStyleSheet("QFrame { border:1px solid #e2e8f0; border-radius:8px; }");
    m_resultArea->setVisible(false);
    auto* resLayout = new QVBoxLayout(m_resultArea);
    resLayout->setContentsMargins(10, 8, 10, 8);
    resLayout->setSpacing(6);

    auto* codeLbl = new QLabel(tr("<b>Codice generato:</b>"), m_resultArea);
    resLayout->addWidget(codeLbl);
    m_codeView = new QTextBrowser(m_resultArea);
    m_codeView->setFont(QFont("monospace", 11));
    m_codeView->setMinimumHeight(150);
    m_codeView->setMaximumHeight(300);
    m_codeView->setStyleSheet(
        "QTextBrowser { background:#1e293b; color:#e2e8f0; border-radius:6px; padding:8px; }");
    resLayout->addWidget(m_codeView);

    auto* outLbl = new QLabel(tr("<b>Output del programma:</b>"), m_resultArea);
    resLayout->addWidget(outLbl);
    m_outputView = new QTextBrowser(m_resultArea);
    m_outputView->setFont(QFont("monospace", 11));
    m_outputView->setMinimumHeight(80);
    m_outputView->setMaximumHeight(200);
    m_outputView->setStyleSheet(
        "QTextBrowser { background:#0f172a; color:#a3e635; border-radius:6px; padding:8px; }");
    resLayout->addWidget(m_outputView);

    auto* anecLbl = new QLabel(
        tr("\xf0\x9f\x92\xa1 <b>Aneddoto per crescere:</b>"), m_resultArea);
    resLayout->addWidget(anecLbl);
    m_anecdoteView = new QTextBrowser(m_resultArea);
    m_anecdoteView->setMinimumHeight(60);
    m_anecdoteView->setMaximumHeight(120);
    m_anecdoteView->setStyleSheet(
        "QTextBrowser { background:#fefce8; border:1px solid #fbbf24;"
        " border-radius:6px; padding:8px; color:#78350f; }");
    m_anecdoteView->setWordWrapMode(QTextOption::WordWrap);
    resLayout->addWidget(m_anecdoteView);

    root->addWidget(m_resultArea, 1);

    /* ── Area modifica (appare dopo il risultato) ── */
    m_modArea = new QFrame(this);
    m_modArea->setFrameShape(QFrame::StyledPanel);
    m_modArea->setStyleSheet(
        "QFrame { border:2px solid #2563eb; border-radius:8px; background:#eff6ff; }");
    m_modArea->setVisible(false);
    auto* modLayout = new QVBoxLayout(m_modArea);
    modLayout->setContentsMargins(10, 8, 10, 8);
    modLayout->setSpacing(6);

    auto* modTitle = new QLabel(
        tr("<b>\xf0\x9f\x94\xa7 Cosa vuoi modificare?</b>"), m_modArea);
    modLayout->addWidget(modTitle);
    m_modInput = new QTextEdit(m_modArea);
    m_modInput->setPlaceholderText(tr("Es: \"Mostra anche i numeri dispari\" oppure \"Aggiungi i commenti al codice\""));
    m_modInput->setMaximumHeight(70);
    modLayout->addWidget(m_modInput);
    m_modBtn = new QPushButton(tr("\xf0\x9f\x94\x84  Applica modifica"), m_modArea);
    m_modBtn->setMinimumHeight(38);
    m_modBtn->setStyleSheet(
        "QPushButton { background:#2563eb; color:white; border-radius:6px; font-weight:bold; }"
        "QPushButton:hover { background:#1d4ed8; }");
    modLayout->addWidget(m_modBtn);

    root->addWidget(m_modArea);

    /* ── Connessioni ── */
    connect(m_runBtn,  &QPushButton::clicked, this, &CodingLabWidget::onRunClicked);
    connect(m_abortBtn, &QPushButton::clicked, this, &CodingLabWidget::onAbortClicked);
    connect(m_modBtn,  &QPushButton::clicked, this, &CodingLabWidget::onModifyClicked);
}

/* Stesso pattern già documentato in gui/CLAUDE.md dopo un coredump reale
   (~VoiceClonerWidget, fix D-20): m_proc è un QProcess figlio con segnali
   collegati a lambda che toccano widget membro — se ancora attivo alla
   chiusura, il suo ~QProcess emette finished() sincrono durante
   deleteChildren() su widget già semi-distrutti. */
CodingLabWidget::~CodingLabWidget()
{
    const auto procs = findChildren<QProcess*>();
    for (QProcess* p : procs) {
        p->disconnect(this);
        p->blockSignals(true);
        if (p->state() != QProcess::NotRunning) {
            p->kill();
            p->waitForFinished(1000);
        }
    }
}

/* ══════════════════════════════════════════════════════════════
   Slot pulsanti
   ══════════════════════════════════════════════════════════════ */
void CodingLabWidget::onRunClicked()
{
    const QString desc = m_descInput->toPlainText().trimmed();
    if (desc.isEmpty()) return;
    m_lastRequest = desc;
    m_retries = 0;
    m_resultArea->setVisible(false);
    m_modArea->setVisible(false);
    startGeneration(desc);
}

void CodingLabWidget::onModifyClicked()
{
    const QString mod = m_modInput->toPlainText().trimmed();
    if (mod.isEmpty()) return;
    m_modInput->clear();

    /* Richiesta = task originale + codice corrente + modifica richiesta */
    const QString req =
        "Partendo da questo codice Python esistente:\n"
        "```python\n" + m_currentCode + "\n```\n\n"
        "Apporta questa modifica: " + mod + "\n\n"
        "Obiettivo originale era: " + m_lastRequest;

    m_lastRequest = mod;
    m_retries = 0;
    m_resultArea->setVisible(false);
    m_modArea->setVisible(false);
    startGeneration(req);
}

void CodingLabWidget::onAbortClicked()
{
    m_ai->abort();
    if (m_proc && m_proc->state() != QProcess::NotRunning)
        m_proc->kill();
    setIdle();
}

/* ══════════════════════════════════════════════════════════════
   Pipeline: 1. Generazione codice
   ══════════════════════════════════════════════════════════════ */
void CodingLabWidget::startGeneration(const QString& userRequest)
{
    m_state = Generating;
    m_codeBuffer.clear();
    m_runOutput.clear();
    m_runError.clear();
    m_abortBtn->setEnabled(true);
    m_runBtn->setEnabled(false);
    setPhase(m_retries == 0
        ? "\xf0\x9f\xa4\x96 Genero il codice…"
        : QString("\xf0\x9f\x94\x84 Correggo l'errore (tentativo %1/%2)…")
              .arg(m_retries).arg(kMaxRetries));

    const QString sys =
        "Sei un assistente di programmazione. "
        "Genera codice Python che risolva esattamente quello che ti viene chiesto. "
        "Regole: rispondi con SOLO il codice Python, racchiuso in ```python ... ```. "
        "Il codice deve essere auto-contenuto, stampare il risultato con print() e terminare senza errori. "
        "Non aggiungere spiegazioni fuori dai blocchi di codice.";

    disconnectAi();
    m_connToken    = connect(m_ai, &AiClient::token,    this, &CodingLabWidget::onCodeToken);
    m_connFinished = connect(m_ai, &AiClient::finished, this, &CodingLabWidget::onCodeFinished);
    m_connError    = connect(m_ai, &AiClient::error,    this, &CodingLabWidget::onCodeError);
    m_ai->chat(sys, userRequest);
}

void CodingLabWidget::onCodeToken(const QString& t)   { m_codeBuffer += t; }

void CodingLabWidget::onCodeFinished()
{
    disconnectAi();
    m_currentCode = extractCode(m_codeBuffer);
    if (m_currentCode.isEmpty()) {
        setPhase(tr("\xe2\x9d\x8c Nessun codice valido generato. Riprova con una descrizione diversa."));
        setIdle();
        return;
    }
    runCode(m_currentCode);
}

void CodingLabWidget::onCodeError(const QString& e)
{
    disconnectAi();
    setPhase(tr("\xe2\x9d\x8c Errore AI: ") + e);
    setIdle();
}

/* ══════════════════════════════════════════════════════════════
   Pipeline: 2. Esecuzione codice (nascosta all'utente)
   ══════════════════════════════════════════════════════════════ */
void CodingLabWidget::runCode(const QString& code)
{
    m_state = Testing;
    m_runOutput.clear();
    m_runError.clear();
    setPhase(tr("\xf0\x9f\xa7\xaa Testo il codice…"));

    /* Scrivi il codice in un file temporaneo */
    auto* tmp = new QTemporaryFile(QDir::tempPath() + "/prismalux_lab_XXXXXX.py", this);
    tmp->setAutoRemove(false);
    if (!tmp->open()) {
        setPhase(tr("\xe2\x9d\x8c Impossibile creare file temporaneo."));
        setIdle();
        tmp->deleteLater();
        return;
    }
    tmp->write(code.toUtf8());
    tmp->flush();
    const QString tmpPath = tmp->fileName();
    tmp->close();

    if (m_proc) {
        m_proc->kill();
        m_proc->deleteLater();
    }
    m_proc = new QProcess(this);
    m_proc->setProgram(P::findPython());
    m_proc->setArguments({ tmpPath });
    m_proc->setProcessChannelMode(QProcess::SeparateChannels);
    m_proc->start();

    connect(m_proc, &QProcess::readyReadStandardOutput,
            this, &CodingLabWidget::onCodeRunOutput);
    connect(m_proc, &QProcess::readyReadStandardError,
            this, &CodingLabWidget::onCodeRunOutput);
    connect(m_proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &CodingLabWidget::onCodeRunFinished);
    connect(m_proc, &QProcess::errorOccurred, this,
        [this](QProcess::ProcessError err) {
            if (err == QProcess::FailedToStart)
                m_outputView->append(tr("<span style='color:red;'>Errore: Python non trovato nel PATH</span>"));
        });

    /* Timeout 15s */
    QTimer::singleShot(15000, m_proc, [this, tmpPath]() {
        if (m_proc && m_proc->state() != QProcess::NotRunning) {
            m_proc->kill();
            QFile::remove(tmpPath);
        }
    });

    /* Rimuovi file temp a processo terminato */
    connect(m_proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            m_proc, [tmpPath](int, QProcess::ExitStatus) { QFile::remove(tmpPath); });
}

void CodingLabWidget::onCodeRunOutput()
{
    if (!m_proc) return;
    m_runOutput += m_proc->readAllStandardOutput();
    m_runError  += m_proc->readAllStandardError();
}

void CodingLabWidget::onCodeRunFinished(int exitCode, QProcess::ExitStatus)
{
    /* Svuota buffer residui */
    if (m_proc) {
        m_runOutput += m_proc->readAllStandardOutput();
        m_runError  += m_proc->readAllStandardError();
    }

    if (exitCode == 0) {
        /* Codice funzionante → aneddoto */
        startAnecdote();
    } else {
        /* Errore → prova a correggere */
        ++m_retries;
        if (m_retries > kMaxRetries) {
            /* Troppi tentativi: mostra comunque il risultato con errore */
            m_codeView->setPlainText(m_currentCode);
            m_outputView->setHtml(
                "<span style='color:#ef4444;'>\xe2\x9c\x96 Errore:</span><br>" +
                m_runError.toHtmlEscaped().replace("\n", "<br>"));
            m_anecdoteView->setPlainText(
                "Non sono riuscito a correggere l'errore in " +
                QString::number(kMaxRetries) + " tentativi. "
                "Descrivi il task in modo diverso e riprova.");
            showResult();
            return;
        }
        startFix();
    }
}

/* ══════════════════════════════════════════════════════════════
   Pipeline: 3. Fix automatico
   ══════════════════════════════════════════════════════════════ */
void CodingLabWidget::startFix()
{
    m_state = Fixing;
    m_codeBuffer.clear();
    setPhase(QString("\xf0\x9f\x94\x84 Correggo l'errore (tentativo %1/%2)…")
             .arg(m_retries).arg(kMaxRetries));

    const QString sys =
        "Sei un assistente di programmazione. "
        "Il codice Python seguente ha prodotto un errore. Correggilo. "
        "Rispondi con SOLO il codice corretto, racchiuso in ```python ... ```. "
        "Nessuna spiegazione fuori dai blocchi di codice.";

    const QString msg =
        "Codice con errore:\n```python\n" + m_currentCode + "\n```\n\n"
        "Errore prodotto:\n" + m_runError.trimmed();

    disconnectAi();
    m_connToken    = connect(m_ai, &AiClient::token,    this, &CodingLabWidget::onFixToken);
    m_connFinished = connect(m_ai, &AiClient::finished, this, &CodingLabWidget::onFixFinished);
    m_connError    = connect(m_ai, &AiClient::error,    this, &CodingLabWidget::onFixError);
    m_ai->chat(sys, msg);
}

void CodingLabWidget::onFixToken(const QString& t)   { m_codeBuffer += t; }

void CodingLabWidget::onFixFinished()
{
    disconnectAi();
    const QString fixed = extractCode(m_codeBuffer);
    if (!fixed.isEmpty()) m_currentCode = fixed;
    runCode(m_currentCode);
}

void CodingLabWidget::onFixError(const QString& e)
{
    disconnectAi();
    setPhase(tr("\xe2\x9d\x8c Errore AI durante correzione: ") + e);
    setIdle();
}

/* ══════════════════════════════════════════════════════════════
   Pipeline: 4. Aneddoto tecnico (dopo codice OK)
   ══════════════════════════════════════════════════════════════ */
void CodingLabWidget::startAnecdote()
{
    m_state = Anecdote;
    m_anecdoteBuffer.clear();
    setPhase(tr("\xf0\x9f\x92\xa1 Preparo un aneddoto tecnico…"));

    const QString sys =
        "Sei un mentore per un perito informatico che vuole crescere come sviluppatore. "
        "Parla in modo diretto e amichevole, senza essere pedante. "
        "In 1-2 frasi max, dai un aneddoto tecnico interessante o un consiglio pratico "
        "direttamente collegato al codice che hai appena generato. "
        "Deve essere qualcosa che un dev senior direbbe ad un junior per farlo crescere.";

    const QString msg =
        "Ho appena generato e testato con successo questo codice Python:\n"
        "```python\n" + m_currentCode + "\n```\n"
        "Output: " + m_runOutput.trimmed().left(200);

    disconnectAi();
    m_connToken    = connect(m_ai, &AiClient::token,    this, &CodingLabWidget::onAnecdoteToken);
    m_connFinished = connect(m_ai, &AiClient::finished, this, &CodingLabWidget::onAnecdoteFinished);
    m_connError    = connect(m_ai, &AiClient::error,    this, &CodingLabWidget::onAnecdoteFinished);
    m_ai->chat(sys, msg);
}

void CodingLabWidget::onAnecdoteToken(const QString& t)  { m_anecdoteBuffer += t; }

void CodingLabWidget::onAnecdoteFinished()
{
    disconnectAi();
    /* Popola le viste risultato */
    m_codeView->setPlainText(m_currentCode);
    m_outputView->setPlainText(
        m_runOutput.trimmed().isEmpty() ? "(nessun output)" : m_runOutput.trimmed());
    m_anecdoteView->setPlainText(m_anecdoteBuffer.trimmed());
    showResult();
}

/* ══════════════════════════════════════════════════════════════
   Helpers
   ══════════════════════════════════════════════════════════════ */
void CodingLabWidget::showResult()
{
    m_state = Done;
    m_resultArea->setVisible(true);
    m_modArea->setVisible(true);
    m_modInput->clear();
    m_modInput->setFocus();
    setPhase(tr("\xe2\x9c\x85 Tutto pronto! Leggi il risultato e dimmi cosa vuoi cambiare."));
    setIdle();
}

void CodingLabWidget::setPhase(const QString& msg)
{
    m_phaseLbl->setText(msg);
}

void CodingLabWidget::setIdle()
{
    m_runBtn->setEnabled(true);
    m_abortBtn->setEnabled(false);
}

void CodingLabWidget::disconnectAi()
{
    if (m_connToken)    { disconnect(m_connToken);    m_connToken    = {}; }
    if (m_connFinished) { disconnect(m_connFinished); m_connFinished = {}; }
    if (m_connError)    { disconnect(m_connError);    m_connError    = {}; }
}

/* Estrae il codice Python dal blocco ```python ... ``` */
QString CodingLabWidget::extractCode(const QString& raw)
{
    static const QRegularExpression re(
        "```(?:python)?\\s*\\n([\\s\\S]*?)\\n```",
        QRegularExpression::CaseInsensitiveOption);
    const auto m = re.match(raw);
    if (m.hasMatch()) return m.captured(1).trimmed();
    /* Fallback: se non ci sono backtick, prova a usare tutto il testo */
    const QString trimmed = raw.trimmed();
    if (trimmed.startsWith("import ") || trimmed.startsWith("def ") ||
        trimmed.startsWith("print(") || trimmed.startsWith("#"))
        return trimmed;
    return {};
}
