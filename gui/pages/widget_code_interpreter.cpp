#include "widget_code_interpreter.h"
#include "../dpi_utils.h"
#include "../widgets/python_exec.h"
#include "../prismalux_paths.h"

namespace P = PrismaluxPaths;
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QSplitter>
#include <QGroupBox>
#include <QComboBox>
#include <QPixmap>
#include <QClipboard>
#include <QApplication>
#include <QScrollArea>
#include <QDateTime>
#include <QFont>
#include <QProcess>

namespace {

/* Prompt sistema per la generazione del codice */
QString buildGenSys(const QString& imgPath)
{
    return QString(
        "Sei un esperto Python. Genera SOLO codice Python eseguibile.\n"
        "Regole OBBLIGATORIE:\n"
        "- Nessun markdown, nessun ```python, nessun ``` — solo codice puro\n"
        "- Nessuna spiegazione, nessun commento descrittivo\n"
        "- Il codice deve girare senza input utente (usa valori hardcoded)\n"
        "- Se usi matplotlib/pyplot, le prime righe DEVONO essere:\n"
        "    import matplotlib\n"
        "    matplotlib.use('Agg')\n"
        "  e alla fine salva con:\n"
        "    plt.savefig('%1', dpi=100, bbox_inches='tight')\n"
        "    plt.close('all')\n"
        "Inizia direttamente con la prima riga di codice Python."
    ).arg(imgPath);
}

/* Prompt sistema per la correzione */
QString buildFixSys(const QString& imgPath)
{
    return QString(
        "Sei un esperto Python. Correggi il codice che ha prodotto un errore.\n"
        "Regole OBBLIGATORIE:\n"
        "- Restituisci SOLO il codice Python corretto, nessuna spiegazione\n"
        "- Nessun markdown, nessun ```python, solo codice puro\n"
        "- Se usi matplotlib, le prime righe devono essere:\n"
        "    import matplotlib\n"
        "    matplotlib.use('Agg')\n"
        "  e alla fine: plt.savefig('%1', dpi=100, bbox_inches='tight'); plt.close('all')\n"
        "Inizia direttamente con la prima riga di codice Python corretto."
    ).arg(imgPath);
}

} // namespace

CodeInterpreterWidget::CodeInterpreterWidget(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    auto* mainLay = new QVBoxLayout(this);
    mainLay->setContentsMargins(8, 8, 8, 4);
    mainLay->setSpacing(6);

    /* ── Intestazione ── */
    auto* titleLbl = new QLabel(
        "\xf0\x9f\xa7\xaa <b>Code Interpreter</b>  "
        "<span style='color:#888;font-size:11px;'>"
        "Descrivi \xe2\x86\x92 AI genera Python \xe2\x86\x92 Esegui \xe2\x86\x92 "
        "Fix automatico (max 3 tentativi)</span>", this);
    titleLbl->setTextFormat(Qt::RichText);
    mainLay->addWidget(titleLbl);

    /* ── Riga input + combo modello ── */
    auto* topRow = new QHBoxLayout;

    m_modelCombo = new QComboBox(this);
    m_modelCombo->setMinimumWidth(dpiScale(200));
    m_modelCombo->setMaximumWidth(dpiScale(300));
    m_modelCombo->setToolTip(tr("Modello AI per la generazione"));
    topRow->addWidget(new QLabel("Modello:", this));
    topRow->addWidget(m_modelCombo);
    topRow->addStretch();

    mainLay->addLayout(topRow);

    /* Popola combo con modelli disponibili */
    if (m_ai) {
        auto populateCombo = [this]{
            const QStringList models = m_ai->models();
            if (models.isEmpty()) return;
            m_modelCombo->blockSignals(true);
            m_modelCombo->clear();
            for (const QString& m : models) {
                qint64 sz = m_ai->modelSizeBytes(m);
                m_modelCombo->addItem(P::modelIcon(sz, m) + m, m);
            }
            /* Preseleziona modello attivo */
            int idx = m_modelCombo->findData(m_ai->model());
            if (idx < 0) idx = m_modelCombo->findText(m_ai->model());
            if (idx >= 0) m_modelCombo->setCurrentIndex(idx);
            m_modelCombo->blockSignals(false);
        };
        populateCombo();
        connect(m_ai, &AiClient::modelsReady, this, populateCombo);
        connect(m_ai, &AiClient::modelChanged, this, [this](const QString& nm){
            int idx = m_modelCombo->findData(nm);
            if (idx < 0) idx = m_modelCombo->findText(nm);
            if (idx >= 0) { m_modelCombo->blockSignals(true);
                            m_modelCombo->setCurrentIndex(idx);
                            m_modelCombo->blockSignals(false); }
        });
    }

    /* ── Descrizione ── */
    auto* inputGroup = new QGroupBox(
        "\xf0\x9f\x93\x9d  Descrizione — cosa deve fare il codice:", this);
    auto* inputLay = new QVBoxLayout(inputGroup);
    inputLay->setContentsMargins(4, 4, 4, 4);
    m_input = new QTextEdit(inputGroup);
    m_input->setMaximumHeight(dpiScale(72));
    m_input->setPlaceholderText(
        "Es: disegna un grafico a barre dei 5 colori preferiti  \xe2\x80\xa2  "
        "calcola i primi 20 numeri di Fibonacci  \xe2\x80\xa2  "
        "simula 1000 lanci di dado e mostra l'istogramma");
    inputLay->addWidget(m_input);
    mainLay->addWidget(inputGroup);

    /* ── Barra pulsanti ── */
    auto* btnLay = new QHBoxLayout;
    btnLay->setSpacing(6);
    m_btnRun  = new QPushButton(
        "\xe2\x9a\xa1  Genera e Esegui con AI", this);
    m_btnRun->setObjectName("primaryBtn");
    m_btnStop = new QPushButton("\xe2\x8f\xb9  Stop", this);
    m_btnStop->setEnabled(false);
    m_btnCopyCode = new QPushButton(
        "\xf0\x9f\x93\x8b  Copia codice", this);
    m_btnClear = new QPushButton(
        "\xf0\x9f\x97\x91  Pulisci", this);
    btnLay->addWidget(m_btnRun);
    btnLay->addWidget(m_btnStop);
    btnLay->addStretch();
    btnLay->addWidget(m_btnCopyCode);
    btnLay->addWidget(m_btnClear);
    mainLay->addLayout(btnLay);

    /* ── Splitter: codice | output ── */
    auto* splitter = new QSplitter(Qt::Horizontal, this);

    /* Pannello codice */
    auto* codeGroup = new QGroupBox(
        "\xf0\x9f\x92\xbb  Codice generato:", splitter);
    auto* codeLay = new QVBoxLayout(codeGroup);
    codeLay->setContentsMargins(4, 4, 4, 4);
    m_codeView = new QPlainTextEdit(codeGroup);
    m_codeView->setReadOnly(true);
    {
        QFont f("Monospace", 10);
        f.setStyleHint(QFont::TypeWriter);
        m_codeView->setFont(f);
    }
    m_codeView->setPlaceholderText(
        "Il codice generato dall'AI apparir\xc3\xa0 qui.");
    codeLay->addWidget(m_codeView);

    /* Pannello output + immagine */
    auto* outGroup = new QGroupBox(
        "\xf0\x9f\x93\x9f  Output:", splitter);
    auto* outLay = new QVBoxLayout(outGroup);
    outLay->setContentsMargins(4, 4, 4, 4);
    m_output = new QTextEdit(outGroup);
    m_output->setReadOnly(true);
    {
        QFont f("Monospace", 10);
        f.setStyleHint(QFont::TypeWriter);
        m_output->setFont(f);
    }
    outLay->addWidget(m_output, 1);

    /* Immagine matplotlib (nascosta finché non arriva un PNG) */
    auto* imgScroll = new QScrollArea(outGroup);
    imgScroll->setWidgetResizable(true);
    imgScroll->setMaximumHeight(dpiScale(320));
    imgScroll->hide();
    m_imageLabel = new QLabel(imgScroll);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    imgScroll->setWidget(m_imageLabel);
    outLay->addWidget(imgScroll);
    /* Salviamo il puntatore allo scroll per poterlo mostrare/nascondere */
    m_imageLabel->setProperty("scrollParent",
                              QVariant::fromValue(static_cast<QWidget*>(imgScroll)));

    splitter->addWidget(codeGroup);
    splitter->addWidget(outGroup);
    splitter->setSizes({420, 420});
    mainLay->addWidget(splitter, 1);

    /* ── Barra di stato ── */
    m_status = new QLabel(
        "Pronto. Descrivi cosa vuoi che il codice faccia.", this);
    m_status->setWordWrap(true);
    m_status->setStyleSheet("color:#aaa;font-size:11px;");
    mainLay->addWidget(m_status);

    /* ── Indicatore sandbox ── */
    const bool sandboxOk = P::isSandboxReady();
    m_sandboxLabel = new QLabel(
        sandboxOk
            ? "\xf0\x9f\x94\x92  Sandbox Docker attiva  \xe2\x80\x94  esecuzione isolata"
            : "\xe2\x9a\xa0\xef\xb8\x8f  Sandbox Docker non disponibile  \xe2\x80\x94  esecuzione locale",
        this);
    m_sandboxLabel->setStyleSheet(
        sandboxOk ? "color:#4caf50;font-size:10px;" : "color:#ff9800;font-size:10px;");
    mainLay->addWidget(m_sandboxLabel);

    /* ── Connessioni ── */
    connect(m_btnRun,  &QPushButton::clicked, this,
            &CodeInterpreterWidget::runGenerate);

    connect(m_btnStop, &QPushButton::clicked, this, [this]{
        if (m_tokenHolder) { delete m_tokenHolder; m_tokenHolder = nullptr; }
        if (m_ai) m_ai->abort();
        m_busy = false;
        setRunning(false);
        m_status->setText(tr("\xe2\x8f\xb9  Interrotto."));
    });

    connect(m_btnCopyCode, &QPushButton::clicked, this, [this]{
        const QString code = m_codeView->toPlainText().trimmed();
        if (!code.isEmpty()) {
            QApplication::clipboard()->setText(code);
            m_status->setText(
                "\xf0\x9f\x93\x8b  Codice copiato negli appunti.");
        }
    });

    connect(m_btnClear, &QPushButton::clicked, this, [this]{
        m_input->clear();
        m_codeView->clear();
        m_output->clear();
        m_aiAccum.clear();
        if (auto* sc = qobject_cast<QWidget*>(
                m_imageLabel->property("scrollParent").value<QWidget*>()))
            sc->hide();
        m_status->setText(tr("Pronto."));
    });
}

/* ══════════════════════════════════════════════════════════════
   runGenerate — chiamata al click del pulsante principale
   ══════════════════════════════════════════════════════════════ */
void CodeInterpreterWidget::runGenerate()
{
    if (!m_ai) return;
    if (m_busy) return;

    const QString desc = m_input->toPlainText().trimmed();
    if (desc.isEmpty()) {
        m_status->setText(
            "\xe2\x9d\x8c  Scrivi una descrizione prima di generare.");
        return;
    }

    /* Percorso immagine univoco per questo run */
    m_imagePath = PrismaluxPaths::safeTempPath()
        + QString("/prisma_plot_%1.png")
          .arg(QDateTime::currentMSecsSinceEpoch());

    setRunning(true);
    m_codeView->clear();
    m_output->clear();
    m_aiAccum.clear();
    if (auto* sc = qobject_cast<QWidget*>(
            m_imageLabel->property("scrollParent").value<QWidget*>()))
        sc->hide();

    m_status->setText(
        "\xf0\x9f\xa4\x96  AI sta generando il codice...");

    /* Sceglie il modello selezionato nella combo */
    QString model = m_modelCombo->currentData(Qt::UserRole).toString();
    if (model.isEmpty()) model = m_modelCombo->currentText();
    if (!model.isEmpty() && model != m_ai->model())
        m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), model);

    const QString sys = P::prependKnowledge(buildGenSys(m_imagePath));

    connectAI([this](const QString& code){
        if (code.isEmpty()) {
            m_status->setText(
                "\xe2\x9d\x8c  L'AI non ha prodotto codice valido. "
                "Riprova con una descrizione pi\xc3\xb9 precisa.");
            setRunning(false);
            return;
        }
        m_codeView->setPlainText(code);
        m_status->setText(
            "\xe2\x9a\x99  Esecuzione in corso... (tentativo 1/"
            + QString::number(kMaxAttempts) + ")");
        executeStep(code, 1);
    });

    m_ai->chat(sys, desc);
}

/* ══════════════════════════════════════════════════════════════
   connectAI — collega token/finished/error e chiama onDone(code)
   ══════════════════════════════════════════════════════════════ */
void CodeInterpreterWidget::connectAI(std::function<void(const QString&)> onDone)
{
    if (m_tokenHolder) { delete m_tokenHolder; m_tokenHolder = nullptr; }
    m_tokenHolder = new QObject(this);
    m_aiAccum.clear();

    connect(m_ai, &AiClient::token, m_tokenHolder,
            [this](const QString& chunk){
        m_aiAccum += chunk;
        /* Mostra in anteprima nel codeView (senza blocchi think) */
        const QString preview = extractCode(stripThink(m_aiAccum).trimmed());
        if (!preview.isEmpty())
            m_codeView->setPlainText(preview);
    });

    connect(m_ai, &AiClient::finished, m_tokenHolder,
            [this, onDone](const QString&){
        if (m_tokenHolder) { delete m_tokenHolder; m_tokenHolder = nullptr; }
        if (!m_busy) return; /* interrotto dall'utente */

        const QString code = patchCode(
            extractCode(stripThink(m_aiAccum).trimmed()));
        m_codeView->setPlainText(code);
        onDone(code);
    });

    connect(m_ai, &AiClient::error, m_tokenHolder,
            [this](const QString& msg){
        if (m_tokenHolder) { delete m_tokenHolder; m_tokenHolder = nullptr; }
        m_status->setText("\xe2\x9d\x8c  Errore AI: " + msg);
        setRunning(false);
    });
}

/* ══════════════════════════════════════════════════════════════
   executeStep — esegue @p code: Docker (se sandbox) o PythonExec
   ══════════════════════════════════════════════════════════════ */
void CodeInterpreterWidget::executeStep(const QString& code, int attempt)
{
    appendOutput(QString("\n\xe2\x80\x94\xe2\x80\x94 Tentativo %1/%2 \xe2\x80\x94\xe2\x80\x94\n")
                 .arg(attempt).arg(kMaxAttempts));

    if (P::isSandboxReady()) {
        executeStepDocker(code, attempt);
        return;
    }

    auto* exec = new PythonExec(code, this);

    /* Timeout */
    auto* timer = new QTimer(exec);
    timer->setSingleShot(true);
    timer->setInterval(kTimeoutMs);
    connect(timer, &QTimer::timeout, exec, [exec, this]{
        exec->abort();
        appendOutput("\n\xe2\x8f\xb1  Timeout (30s) \xe2\x80\x94 processo terminato.\n");
    });

    connect(exec, &PythonExec::output, this,
            [this](const QString& chunk){ appendOutput(chunk); });

    connect(exec, &PythonExec::finished, this,
            [this, attempt, code](int exitCode, const QString& fullOut){
        if (!m_busy) return; /* interrotto dall'utente */

        if (exitCode == 0) {
            m_status->setText(
                QString("\xe2\x9c\x85  Completato  \xc2\xb7  %1")
                .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
            if (QFile::exists(m_imagePath))
                showImage(m_imagePath);
            setRunning(false);
        } else {
            const QStringList lines = fullOut.split('\n');
            const int start = qMax(0, lines.size() - 20);
            const QString errSnippet = lines.mid(start).join('\n').trimmed();

            if (attempt >= kMaxAttempts) {
                m_status->setText(
                    QString("\xe2\x9d\x8c  Fallito dopo %1 tentativi. "
                            "Prova una descrizione diversa.").arg(attempt));
                setRunning(false);
                return;
            }
            m_status->setText(
                QString("\xf0\x9f\x94\x84  Errore \xe2\x80\x94 AI corregge "
                        "(tentativo %1/%2)...")
                .arg(attempt + 1).arg(kMaxAttempts));
            requestFix(code, errSnippet, attempt);
        }
    });

    timer->start();
    exec->start();
}

/* ══════════════════════════════════════════════════════════════
   executeStepDocker — sandbox Docker isolato (no network, 256MB)
   ══════════════════════════════════════════════════════════════ */
void CodeInterpreterWidget::executeStepDocker(const QString& code, int attempt)
{
    const QString docker = P::findDocker();
    if (docker.isEmpty()) {
        appendOutput("\xe2\x9a\xa0\xef\xb8\x8f  Docker non trovato, fallback locale.\n");
        /* Ricade su PythonExec senza ricorsione */
        auto* exec = new PythonExec(code, this);
        connect(exec, &PythonExec::output, this,
                [this](const QString& c){ appendOutput(c); });
        connect(exec, &PythonExec::finished, this,
                [this, attempt, code](int rc, const QString& out){
            if (!m_busy) return;
            if (rc == 0) {
                m_status->setText(QString("\xe2\x9c\x85  Completato  \xc2\xb7  %1")
                    .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
                if (QFile::exists(m_imagePath)) showImage(m_imagePath);
                setRunning(false);
            } else {
                const QStringList lines = out.split('\n');
                const QString err = lines.mid(qMax(0, lines.size()-20)).join('\n').trimmed();
                if (attempt >= kMaxAttempts) { m_status->setText(tr("\xe2\x9d\x8c  Fallito.")); setRunning(false); return; }
                requestFix(code, err, attempt);
            }
        });
        exec->start();
        return;
    }

    if (m_dockerProc) { m_dockerProc->kill(); m_dockerProc->deleteLater(); m_dockerProc = nullptr; }

    m_dockerProc = new QProcess(this);
    m_dockerProc->setProgram(docker);
    m_dockerProc->setArguments({
        "run", "--rm", "-i",
        "--memory=256m", "--cpus=0.5",
        "--network=none",
        "--security-opt=no-new-privileges",
        "python:3.11-slim", "python3", "-"
    });
    m_dockerProc->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_dockerProc, &QProcess::readyReadStandardOutput, this, [this]{
        if (m_dockerProc)
            appendOutput(QString::fromUtf8(m_dockerProc->readAllStandardOutput()));
    });

    QString accumulated;
    connect(m_dockerProc, &QProcess::readyReadStandardOutput, this, [this, &accumulated]{
        if (m_dockerProc)
            accumulated += QString::fromUtf8(m_dockerProc->readAllStandardOutput());
    });

    connect(m_dockerProc,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, attempt, code](int exitCode, QProcess::ExitStatus){
        if (!m_busy) return;
        const QString fullOut = m_dockerProc
            ? QString::fromUtf8(m_dockerProc->readAllStandardOutput())
            : QString{};
        if (m_dockerProc) { m_dockerProc->deleteLater(); m_dockerProc = nullptr; }

        if (exitCode == 0) {
            m_status->setText(QString("\xe2\x9c\x85  \xf0\x9f\x94\x92 Docker  \xc2\xb7  %1")
                .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
            if (QFile::exists(m_imagePath)) showImage(m_imagePath);
            setRunning(false);
        } else {
            const QStringList lines = fullOut.split('\n');
            const QString err = lines.mid(qMax(0, lines.size()-20)).join('\n').trimmed();
            if (attempt >= kMaxAttempts) {
                m_status->setText(QString("\xe2\x9d\x8c  Fallito dopo %1 tentativi.").arg(attempt));
                setRunning(false);
                return;
            }
            m_status->setText(QString("\xf0\x9f\x94\x84  Errore Docker \xe2\x80\x94 AI corregge (%1/%2)...")
                .arg(attempt+1).arg(kMaxAttempts));
            requestFix(code, err, attempt);
        }
    });

    /* Timeout */
    auto* timer = new QTimer(m_dockerProc);
    timer->setSingleShot(true);
    timer->setInterval(kTimeoutMs);
    connect(timer, &QTimer::timeout, this, [this]{
        if (m_dockerProc) {
            m_dockerProc->kill();
            appendOutput("\n\xe2\x8f\xb1  Timeout Docker (30s) \xe2\x80\x94 contenitore terminato.\n");
        }
    });

    m_dockerProc->start();
    if (m_dockerProc->waitForStarted(P::kProcessStartTimeoutMs)) {
        m_dockerProc->write(code.toUtf8());
        m_dockerProc->closeWriteChannel();
        timer->start();
    } else {
        appendOutput("\xe2\x9d\x8c  Impossibile avviare Docker. Fallback locale.\n");
        m_dockerProc->deleteLater(); m_dockerProc = nullptr;
        /* Fallback diretto senza loop infinito */
        auto* exec = new PythonExec(code, this);
        connect(exec, &PythonExec::output, this, [this](const QString& c){ appendOutput(c); });
        connect(exec, &PythonExec::finished, this,
                [this, attempt, code](int rc, const QString& out){
            if (!m_busy) return;
            if (rc == 0) { setRunning(false); return; }
            if (attempt >= kMaxAttempts) { setRunning(false); return; }
            requestFix(code, out.right(1000), attempt);
        });
        exec->start();
    }
}

/* ══════════════════════════════════════════════════════════════
   requestFix — chiede all'AI di correggere il codice con errore
   ══════════════════════════════════════════════════════════════ */
void CodeInterpreterWidget::requestFix(const QString& code,
                                       const QString& errSnippet,
                                       int attempt)
{
    const QString sys = buildFixSys(m_imagePath);
    const QString prompt = QString(
        "CODICE:\n%1\n\nERRORE:\n%2").arg(code, errSnippet);

    connectAI([this, attempt](const QString& fixedCode){
        if (fixedCode.isEmpty()) {
            m_status->setText(
                "\xe2\x9d\x8c  L'AI non ha prodotto una correzione valida.");
            setRunning(false);
            return;
        }
        m_codeView->setPlainText(fixedCode);
        m_status->setText(
            QString("\xe2\x9a\x99  Esecuzione codice corretto "
                    "(tentativo %1/%2)...")
            .arg(attempt + 1).arg(kMaxAttempts));
        executeStep(fixedCode, attempt + 1);
    });

    m_ai->chat(sys, prompt);
}

/* ══════════════════════════════════════════════════════════════
   showImage — carica il PNG matplotlib e lo mostra inline
   ══════════════════════════════════════════════════════════════ */
void CodeInterpreterWidget::showImage(const QString& path)
{
    QPixmap px(path);
    if (px.isNull()) return;

    /* Scala l'immagine per adattarla al widget (max 600px larghezza) */
    if (px.width() > 600)
        px = px.scaledToWidth(600, Qt::SmoothTransformation);

    m_imageLabel->setPixmap(px);
    if (auto* sc = qobject_cast<QWidget*>(
            m_imageLabel->property("scrollParent").value<QWidget*>()))
        sc->show();

    appendOutput("\n\xf0\x9f\x96\xbc  Grafico generato e mostrato sopra.\n");
}

/* ══════════════════════════════════════════════════════════════
   patchCode — inietta matplotlib Agg + savefig se assente
   ══════════════════════════════════════════════════════════════ */
QString CodeInterpreterWidget::patchCode(const QString& raw) const
{
    if (raw.isEmpty()) return raw;

    /* Nessun matplotlib → niente da patchare */
    const bool hasPlot = raw.contains("matplotlib") ||
                         raw.contains("pyplot") ||
                         raw.contains("plt.");
    if (!hasPlot) return raw;

    QString code = raw;

    /* Assicura matplotlib.use('Agg') subito dopo il primo import matplotlib */
    if (!code.contains("matplotlib.use(")) {
        static const QRegularExpression reImp(
            "(import matplotlib(?:\\.[^\\n]*)?)\\n");
        const QRegularExpressionMatch m = reImp.match(code);
        if (m.hasMatch())
            code.insert(m.capturedEnd(), "matplotlib.use('Agg')\n");
        else
            code.prepend("import matplotlib\nmatplotlib.use('Agg')\n");
    }

    /* Assicura savefig alla fine se assente */
    if (!code.contains("savefig")) {
        code += QString(
            "\ntry:\n"
            "    import matplotlib.pyplot as _plt\n"
            "    _plt.savefig('%1', dpi=100, bbox_inches='tight')\n"
            "    _plt.close('all')\n"
            "except Exception:\n"
            "    pass\n"
        ).arg(m_imagePath);
    }

    return code;
}

/* ══════════════════════════════════════════════════════════════
   extractCode — rimuove i fence markdown se presenti
   ══════════════════════════════════════════════════════════════ */
QString CodeInterpreterWidget::extractCode(const QString& text)
{
    if (text.isEmpty()) return text;
    static const QRegularExpression reFence(
        "```(?:python)?\\s*\\n([\\s\\S]*?)```",
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch m = reFence.match(text);
    if (m.hasMatch()) return m.captured(1).trimmed();
    /* Rimuovi backtick singoli di apertura senza chiusura (stream incompleto) */
    QString clean = text;
    if (clean.startsWith("```python\n") || clean.startsWith("```\n"))
        clean = clean.mid(clean.indexOf('\n') + 1);
    return clean.trimmed();
}

/* ══════════════════════════════════════════════════════════════
   stripThink — rimuove i blocchi <think>...</think>
   ══════════════════════════════════════════════════════════════ */
QString CodeInterpreterWidget::stripThink(const QString& text)
{
    if (!text.contains("<think")) return text;
    static const QRegularExpression reTh(
        "<think>[\\s\\S]*?</think>",
        QRegularExpression::CaseInsensitiveOption);
    QString out = text;
    out.remove(reTh);
    return out;
}

/* ══════════════════════════════════════════════════════════════
   appendOutput / setRunning
   ══════════════════════════════════════════════════════════════ */
void CodeInterpreterWidget::appendOutput(const QString& text)
{
    m_output->moveCursor(QTextCursor::End);
    m_output->insertPlainText(text);
}

void CodeInterpreterWidget::setRunning(bool running)
{
    m_busy = running;
    m_btnRun->setEnabled(!running);
    m_btnStop->setEnabled(running);
    m_modelCombo->setEnabled(!running);
}
