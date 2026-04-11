#include "programmazione_page.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QSplitter>
#include <QDialog>
#include <QDialogButtonBox>
#include <QCheckBox>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDir>
#include <QPixmap>
#include <QScrollArea>
#include <QProcess>
#include <QTimer>
#include <QRegularExpression>
#include <QFontDatabase>
#include <QToolButton>
#include <QClipboard>
#include <QApplication>
#include <QShortcut>

/* ══════════════════════════════════════════════════════════════
   Costruttore
   ══════════════════════════════════════════════════════════════ */
ProgrammazionePage::ProgrammazionePage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    /* Titolo pagina */
    auto* hdr = new QWidget(this);
    hdr->setObjectName("pageHeader");
    auto* hdrL = new QHBoxLayout(hdr);
    hdrL->setContentsMargins(20, 12, 20, 10);
    auto* title = new QLabel("\xf0\x9f\x92\xbb  Programmazione", hdr);
    title->setObjectName("pageTitle");
    auto* sub = new QLabel("Genera codice con AI · Esegui · Salva output", hdr);
    sub->setObjectName("pageSubtitle");
    auto* infoBox = new QWidget(hdr);
    auto* infoL   = new QVBoxLayout(infoBox);
    infoL->setContentsMargins(0,0,0,0); infoL->setSpacing(2);
    infoL->addWidget(title);
    infoL->addWidget(sub);
    hdrL->addWidget(infoBox, 1);

    /* Shortcut hint */
    auto* hintLbl = new QLabel(
        "<span style='color:#5a5f80;font-size:11px;'>"
        "Alt+4  |  Ctrl+Enter = genera  |  Escape = interrompi"
        "</span>", hdr);
    hdrL->addWidget(hintLbl);
    lay->addWidget(hdr);

    auto* div = new QFrame(this);
    div->setObjectName("pageDivider");
    div->setFrameShape(QFrame::HLine);
    lay->addWidget(div);

    /* ── Splitter verticale: input / codice / output ── */
    auto* splitter = new QSplitter(Qt::Vertical, this);
    splitter->setObjectName("progSplitter");
    splitter->setHandleWidth(4);
    splitter->addWidget(buildInputPanel());
    splitter->addWidget(buildCodePanel());
    splitter->addWidget(buildOutputPanel());
    splitter->setSizes({150, 280, 200});
    lay->addWidget(splitter, 1);

    /* Connessioni AI */
    connect(m_ai, &AiClient::token,    this, &ProgrammazionePage::onToken);
    connect(m_ai, &AiClient::finished, this, &ProgrammazionePage::onFinished);
    connect(m_ai, &AiClient::error,    this, &ProgrammazionePage::onError);

    /* Popola i combo modello quando la lista è pronta */
    connect(m_ai, &AiClient::modelsReady, this, &ProgrammazionePage::populateModelCombos);

    /* Se i modelli sono già stati caricati, popola subito */
    const QStringList cached = m_ai->models();
    if (!cached.isEmpty())
        populateModelCombos(cached);
    else
        m_ai->fetchModels();  /* richiedi la lista se non ancora disponibile */

    /* Ctrl+S — salva output (nella scheda Programmazione) */
    auto* scSave = new QShortcut(QKeySequence("Ctrl+S"), this);
    scSave->setContext(Qt::WidgetWithChildrenShortcut);
    connect(scSave, &QShortcut::activated, this, &ProgrammazionePage::saveOutput);

    /* Ctrl+Shift+C — copia codice generato negli appunti */
    auto* scCopy = new QShortcut(QKeySequence("Ctrl+Shift+C"), this);
    scCopy->setContext(Qt::WidgetWithChildrenShortcut);
    connect(scCopy, &QShortcut::activated, this, [this]{
        if (!m_lastCode.isEmpty())
            QApplication::clipboard()->setText(m_lastCode);
    });
}

/* ══════════════════════════════════════════════════════════════
   buildInputPanel — task + lingua + pulsanti genera
   ══════════════════════════════════════════════════════════════ */
QWidget* ProgrammazionePage::buildInputPanel() {
    auto* w   = new QWidget(this);
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(16, 10, 16, 8);
    lay->setSpacing(8);

    auto* topRow = new QHBoxLayout;
    auto* lTask  = new QLabel("Cosa vuoi fare:", w);
    lTask->setObjectName("cardTitle");
    topRow->addWidget(lTask);
    topRow->addStretch(1);

    /* Selector linguaggio */
    topRow->addWidget(new QLabel("Linguaggio:", w));
    m_cmbLang = new QComboBox(w);
    m_cmbLang->addItems({"Auto", "Python", "C", "C++", "JavaScript (Node)", "Bash"});
    m_cmbLang->setFixedWidth(155);
    topRow->addWidget(m_cmbLang);

    /* Selector modello AI */
    topRow->addWidget(new QLabel("Modello:", w));
    m_cmbModel = new QComboBox(w);
    m_cmbModel->setFixedWidth(200);
    m_cmbModel->setToolTip(
        "Modello AI per la generazione del codice.\n"
        "I modelli specializzati in programmazione sono mostrati per primi.");
    m_cmbModel->addItem("(caricamento modelli...)");
    topRow->addWidget(m_cmbModel);

    /* Pulsanti */
    m_btnGen = new QPushButton("\xe2\x9c\xa8  Genera codice", w);
    m_btnGen->setObjectName("primaryBtn");
    m_btnGen->setFixedHeight(32);
    m_btnGen->setToolTip("Invia il task all'AI e genera il codice (Ctrl+Enter)");
    topRow->addWidget(m_btnGen);

    m_btnStop = new QPushButton("\xe2\x9b\x94  Stop", w);
    m_btnStop->setObjectName("actionBtn");
    m_btnStop->setFixedHeight(32);
    m_btnStop->setEnabled(false);
    topRow->addWidget(m_btnStop);

    lay->addLayout(topRow);

    m_taskInput = new QPlainTextEdit(w);
    m_taskInput->setObjectName("chatInput");
    m_taskInput->setPlaceholderText(
        "Descrivi cosa vuoi fare...\n"
        "Es: \"Scrivi un programma Python che legge un CSV e calcola la media per colonna\"\n"
        "Es: \"Implementa un albero binario di ricerca in C++ con insert e search\"\n"
        "Es: \"Crea uno script Bash che monitora l'uso del disco ogni 5 secondi\"");
    m_taskInput->setFixedHeight(88);
    lay->addWidget(m_taskInput);

    /* Connetti Ctrl+Enter → genera */
    connect(m_btnGen,  &QPushButton::clicked, this, &ProgrammazionePage::generateCode);
    connect(m_btnStop, &QPushButton::clicked, this, [this]{
        m_ai->abort();
        m_btnStop->setEnabled(false);
        m_btnGen->setEnabled(true);
    });

    /* Installa event filter su m_taskInput per Ctrl+Enter */
    m_taskInput->installEventFilter(this);

    return w;
}

/* ══════════════════════════════════════════════════════════════
   buildCodePanel — codice generato + pulsanti esegui/salva
   ══════════════════════════════════════════════════════════════ */
QWidget* ProgrammazionePage::buildCodePanel() {
    auto* w   = new QWidget(this);
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(16, 8, 16, 8);
    lay->setSpacing(4);

    /* ── Toolbar esterna: badge + Salva + Copia + "Correggi con AI" ── */
    auto* toolbar = new QHBoxLayout;

    m_langBadge = new QLabel("Codice generato", w);
    m_langBadge->setObjectName("cardTitle");
    toolbar->addWidget(m_langBadge);
    toolbar->addStretch(1);

    m_btnSave = new QPushButton("\xf0\x9f\x92\xbe  Salva...", w);
    m_btnSave->setObjectName("actionBtn");
    m_btnSave->setFixedHeight(28);
    m_btnSave->setEnabled(false);
    m_btnSave->setToolTip("Salva codice, output, prompt o risposta AI su file (Ctrl+S)");
    toolbar->addWidget(m_btnSave);

    m_btnCopyCode = new QPushButton("\xf0\x9f\x93\x8b  Copia", w);
    m_btnCopyCode->setObjectName("actionBtn");
    m_btnCopyCode->setFixedHeight(28);
    m_btnCopyCode->setEnabled(false);
    m_btnCopyCode->setToolTip("Copia il codice negli appunti (Ctrl+Shift+C)");
    toolbar->addWidget(m_btnCopyCode);

    auto* sep = new QFrame(w);
    sep->setFrameShape(QFrame::VLine);
    sep->setFixedWidth(1);
    sep->setStyleSheet("background:#3a3f5c;");
    toolbar->addWidget(sep);

    /* ── Pulsante "Correggi con AI" ── */
    m_btnFix = new QPushButton("\xf0\x9f\x94\xa7  Correggi con AI", w);
    m_btnFix->setObjectName("actionBtn");
    m_btnFix->setFixedHeight(28);
    m_btnFix->setEnabled(false);
    m_btnFix->setToolTip("Invia il codice + errori all'AI per la correzione");
    toolbar->addWidget(m_btnFix);

    /* Selettore modello per il fix: "con [modello]" */
    auto* fixModelLbl = new QLabel("con:", w);
    fixModelLbl->setObjectName("cardDesc");
    toolbar->addWidget(fixModelLbl);

    m_cmbFixModel = new QComboBox(w);
    m_cmbFixModel->setFixedWidth(175);
    m_cmbFixModel->setFixedHeight(28);
    m_cmbFixModel->setToolTip(
        "Modello AI da usare per la correzione.\n"
        "Puoi scegliere un modello diverso da quello usato per generare il codice.");
    m_cmbFixModel->addItem("(caricamento modelli...)");
    toolbar->addWidget(m_cmbFixModel);

    /* Slider "fino a: N" (1=1 tentativo, 2-9=N tentativi, 0=∞ finché risolto) */
    auto* fixIterLbl = new QLabel("fino a:", w);
    fixIterLbl->setObjectName("cardDesc");
    toolbar->addWidget(fixIterLbl);

    m_spinFixIter = new QSpinBox(w);
    m_spinFixIter->setRange(0, 10);
    m_spinFixIter->setValue(3);
    m_spinFixIter->setFixedWidth(52);
    m_spinFixIter->setFixedHeight(28);
    m_spinFixIter->setSpecialValueText("\xe2\x88\x9e");  /* "∞" per valore 0 */
    m_spinFixIter->setToolTip(
        "Numero di tentativi di correzione AI.\n"
        "0 (∞) = ripeti finché il codice non compila.\n"
        "1-10 = tenta al massimo N volte.");
    toolbar->addWidget(m_spinFixIter);

    lay->addLayout(toolbar);

    /* Connette il pulsante "Correggi con AI" → avvia auto-fix manuale */
    connect(m_btnFix, &QPushButton::clicked, this, [this]{
        if (!m_lastCode.isEmpty() && !m_ai->busy()) {
            m_fixAttempts = 0;
            autoFixCode(m_procOutput.isEmpty()
                        ? QString("[Correzione richiesta manualmente]")
                        : m_procOutput);
        }
    });

    /* Status auto-fix (nascosto di default) */
    m_fixStatus = new QLabel(w);
    m_fixStatus->setStyleSheet(
        "color:#ffd740; font-size:11px; padding:1px 0; font-style:italic;");
    m_fixStatus->hide();
    lay->addWidget(m_fixStatus);

    /* ── Contenitore codice: bar interna (▶ Esegui / ⬛ Ferma) + code view ── */
    auto* codeContainer = new QFrame(w);
    codeContainer->setObjectName("codeContainer");
    codeContainer->setStyleSheet(
        "QFrame#codeContainer { border:1px solid #2d3154; border-radius:6px; }");
    auto* ccLay = new QVBoxLayout(codeContainer);
    ccLay->setContentsMargins(0, 0, 0, 0);
    ccLay->setSpacing(0);

    /* Bar interna — stesso sfondo del code view, allineata a destra */
    auto* innerBar = new QWidget(codeContainer);
    innerBar->setObjectName("codeInnerBar");
    innerBar->setStyleSheet(
        "QWidget#codeInnerBar {"
        "  background:#12152a;"     /* stesso bg del #chatLog */
        "  border-bottom:1px solid #2d3154;"
        "  border-radius:0px;"
        "}");
    innerBar->setFixedHeight(32);
    auto* innerLay = new QHBoxLayout(innerBar);
    innerLay->setContentsMargins(6, 2, 6, 2);
    innerLay->setSpacing(4);
    innerLay->addStretch(1);

    m_btnRun = new QPushButton("\xe2\x96\xb6  Esegui", innerBar);
    m_btnRun->setObjectName("codeRunBtn");
    m_btnRun->setFixedHeight(24);
    m_btnRun->setEnabled(false);
    m_btnRun->setToolTip("Esegui il codice generato (compila se necessario)");
    m_btnRun->setStyleSheet(
        "QPushButton#codeRunBtn {"
        "  background:#1e6b3c; color:#e0ffe0; border:none; border-radius:4px;"
        "  padding:0 10px; font-size:12px; font-weight:bold;"
        "}"
        "QPushButton#codeRunBtn:hover  { background:#28894f; }"
        "QPushButton#codeRunBtn:disabled { background:#1a2a22; color:#456055; }");

    m_btnKill = new QPushButton("\xe2\xac\x9b  Ferma", innerBar);
    m_btnKill->setObjectName("codeKillBtn");
    m_btnKill->setFixedHeight(24);
    m_btnKill->setEnabled(false);
    m_btnKill->setStyleSheet(
        "QPushButton#codeKillBtn {"
        "  background:#5a1a1a; color:#ffcccc; border:none; border-radius:4px;"
        "  padding:0 10px; font-size:12px;"
        "}"
        "QPushButton#codeKillBtn:hover  { background:#7a2222; }"
        "QPushButton#codeKillBtn:disabled { background:#221a1a; color:#5a3333; }");

    innerLay->addWidget(m_btnRun);
    innerLay->addWidget(m_btnKill);

    m_codeView = new QTextEdit(codeContainer);
    m_codeView->setObjectName("chatLog");
    m_codeView->setReadOnly(true);
    m_codeView->setPlaceholderText("Il codice generato apparirà qui...");
    m_codeView->setStyleSheet("QTextEdit#chatLog { border:none; border-radius:0px; }");

    ccLay->addWidget(innerBar);
    ccLay->addWidget(m_codeView, 1);
    lay->addWidget(codeContainer, 1);

    connect(m_btnRun,      &QPushButton::clicked, this, &ProgrammazionePage::runCode);
    connect(m_btnKill,     &QPushButton::clicked, this, &ProgrammazionePage::killProcess);
    connect(m_btnSave,     &QPushButton::clicked, this, &ProgrammazionePage::saveOutput);
    connect(m_btnCopyCode, &QPushButton::clicked, this, [this]{
        if (!m_lastCode.isEmpty())
            QApplication::clipboard()->setText(m_lastCode);
    });

    return w;
}

/* ══════════════════════════════════════════════════════════════
   buildOutputPanel — output esecuzione
   ══════════════════════════════════════════════════════════════ */
QWidget* ProgrammazionePage::buildOutputPanel() {
    auto* w   = new QWidget(this);
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(16, 8, 16, 12);
    lay->setSpacing(6);

    auto* outHdr = new QHBoxLayout;
    auto* outLbl = new QLabel("\xf0\x9f\x96\xa5  Output esecuzione", w);
    outLbl->setObjectName("cardTitle");
    outHdr->addWidget(outLbl);
    outHdr->addStretch(1);

    auto* btnClear = new QPushButton("Pulisci", w);
    btnClear->setObjectName("actionBtn");
    btnClear->setFixedHeight(26);
    outHdr->addWidget(btnClear);
    lay->addLayout(outHdr);

    m_outputView = new QTextEdit(w);
    m_outputView->setObjectName("chatLog");
    m_outputView->setReadOnly(true);
    m_outputView->setPlaceholderText("Output stdout/stderr del programma...");
    lay->addWidget(m_outputView, 1);

    connect(btnClear, &QPushButton::clicked, m_outputView, &QTextEdit::clear);

    return w;
}

/* ══════════════════════════════════════════════════════════════
   generateCode — invia task all'AI
   ══════════════════════════════════════════════════════════════ */
void ProgrammazionePage::generateCode() {
    const QString task = m_taskInput->toPlainText().trimmed();
    if (task.isEmpty()) return;
    if (m_ai->busy()) return;

    m_lastPrompt = task;
    m_lastAiResponse.clear();
    m_lastCode.clear();
    m_codeView->clear();
    m_outputView->clear();
    m_btnRun->setEnabled(false);
    m_btnSave->setEnabled(false);
    m_btnCopyCode->setEnabled(false);
    m_fixAttempts = 0;
    m_isFix = false;
    m_fixStatus->hide();
    m_langBadge->setText("Generazione in corso...");

    m_btnGen->setEnabled(false);
    m_btnStop->setEnabled(true);

    /* Imposta il modello scelto dall'utente (rimuove prefisso emoji 💻 se presente) */
    QString selectedModel = m_cmbModel->currentText().trimmed();
    if (selectedModel.startsWith("\xf0\x9f\x92\xbb"))   /* rimuovi "💻  " */
        selectedModel = selectedModel.mid(6).trimmed();
    if (!selectedModel.isEmpty() &&
        !selectedModel.startsWith("(") &&
        selectedModel != m_ai->model())
    {
        m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), selectedModel);
    }

    m_ai->chat(buildSystemPrompt(), task);
}

/* ══════════════════════════════════════════════════════════════
   buildSystemPrompt — system prompt per la lingua selezionata
   ══════════════════════════════════════════════════════════════ */
QString ProgrammazionePage::buildSystemPrompt() const {
    const QString lang = m_cmbLang->currentText();

    QString langSpec;
    if      (lang == "Python")             langSpec = "Python 3";
    else if (lang == "C")                  langSpec = "C (C11, gcc)";
    else if (lang == "C++")                langSpec = "C++ (C++17, g++)";
    else if (lang == "JavaScript (Node)")  langSpec = "JavaScript (Node.js)";
    else if (lang == "Bash")               langSpec = "Bash";
    else                                   langSpec = "";  /* Auto */

    if (langSpec.isEmpty()) {
        return
            "Sei un esperto di programmazione. "
            "Scegli il linguaggio più adatto al task dell'utente. "
            "Genera codice corretto, conciso e funzionante. "
            "Rispondi SOLO con il codice in un singolo blocco markdown "
            "```linguaggio ... ``` (nessuna spiegazione prima o dopo). "
            "Se il codice richiede librerie esterne, includile nell'import/include.";
    }

    return QString(
        "Sei un esperto di programmazione %1. "
        "Genera codice %1 corretto, conciso e funzionante per il task dell'utente. "
        "Rispondi SOLO con il codice in un singolo blocco markdown "
        "```%2 ... ``` (nessuna spiegazione prima o dopo). "
        "Se il codice usa librerie standard, importale normalmente.")
        .arg(langSpec, langSpec.toLower().split(" ").first());
}

/* ══════════════════════════════════════════════════════════════
   isCodingModel — true se il nome suggerisce specializzazione per codice
   ══════════════════════════════════════════════════════════════ */
bool ProgrammazionePage::isCodingModel(const QString& name) {
    const QString lo = name.toLower();
    return lo.contains("coder")     || lo.contains("codellama")  ||
           lo.contains("starcoder") || lo.contains("devstral")   ||
           lo.contains("deepseek")  || lo.contains("wizard")     ||
           lo.contains("phind")     || lo.contains("granite")    ||
           lo.contains("code")      || lo.contains("qwen2.5")    ||
           lo.contains("mistral")   || lo.contains("llama3");
}

/* ══════════════════════════════════════════════════════════════
   populateModelCombos — riempie m_cmbModel e m_cmbFixModel
   Mette i coding-model in cima, ordinati alfabeticamente, poi gli altri.
   ══════════════════════════════════════════════════════════════ */
void ProgrammazionePage::populateModelCombos(const QStringList& models) {
    /* Separa coding da generici */
    QStringList coding, generic;
    for (const QString& m : models) {
        if (isCodingModel(m)) coding << m;
        else                   generic << m;
    }
    coding.sort(Qt::CaseInsensitive);
    generic.sort(Qt::CaseInsensitive);

    /* Funzione locale per riempire un combo mantenendo la selezione */
    auto fill = [&](QComboBox* cmb) {
        const QString prev = cmb->currentText();
        cmb->blockSignals(true);
        cmb->clear();
        for (const QString& m : coding)
            cmb->addItem("\xf0\x9f\x92\xbb  " + m);   /* 💻 coding */
        if (!coding.isEmpty() && !generic.isEmpty()) {
            cmb->insertSeparator(cmb->count());
        }
        for (const QString& m : generic)
            cmb->addItem(m);

        /* Ripristina selezione precedente se esiste ancora */
        int idx = cmb->findText(prev);
        if (idx >= 0) cmb->setCurrentIndex(idx);
        else if (cmb->count() > 0) cmb->setCurrentIndex(0);
        cmb->blockSignals(false);
    };

    fill(m_cmbModel);
    fill(m_cmbFixModel);
}

/* ══════════════════════════════════════════════════════════════
   extractCode — parsing blocco ```lang ... ``` dalla risposta
   ══════════════════════════════════════════════════════════════ */
void ProgrammazionePage::extractCode(const QString& response) {
    /* Pattern: ```lang\n...``` — cattura linguaggio e codice */
    static const QRegularExpression re(
        "```([a-zA-Z0-9+#_-]*)\\s*\\n([\\s\\S]*?)```",
        QRegularExpression::MultilineOption);

    auto match = re.match(response);
    if (match.hasMatch()) {
        m_detectedLang = match.captured(1).trimmed().toLower();
        m_lastCode     = match.captured(2);
    } else {
        /* Nessun blocco: prendi tutta la risposta come codice */
        m_detectedLang = "";
        m_lastCode     = response.trimmed();
    }

    /* Normalizza nome linguaggio per il badge */
    QString badge = m_detectedLang.isEmpty() ? "codice" : m_detectedLang;
    if (badge == "python" || badge == "python3") badge = "Python 3";
    else if (badge == "c++") badge = "C++17";
    else if (badge == "c")   badge = "C";
    else if (badge == "javascript" || badge == "js") badge = "JavaScript (Node)";
    else if (badge == "bash" || badge == "sh")       badge = "Bash";

    m_langBadge->setText(QString("Codice \xe2\x80\x94 %1").arg(badge));

    /* Mostra il codice */
    m_codeView->setPlainText(m_lastCode);

    /* Per matplotlib: inietta savefig se c'è plt.show() */
    if ((m_detectedLang == "python" || m_detectedLang == "python3") &&
        m_lastCode.contains("plt.show()"))
    {
        m_plotPath = "/tmp/prisma_plot.png";
        m_lastCode.replace("plt.show()",
            "plt.savefig('/tmp/prisma_plot.png', bbox_inches='tight')\n"
            "# plt.show() — sostituito con savefig per esecuzione headless");
        m_codeView->setPlainText(m_lastCode);
    } else {
        m_plotPath.clear();
    }

    m_btnRun->setEnabled(!m_lastCode.isEmpty());
    m_btnCopyCode->setEnabled(!m_lastCode.isEmpty());
    m_btnSave->setEnabled(true);
}

/* ══════════════════════════════════════════════════════════════
   runCode — compila e/o esegui il codice con QProcess
   ══════════════════════════════════════════════════════════════ */
void ProgrammazionePage::runCode() {
    if (m_lastCode.isEmpty()) return;
    if (m_proc && m_proc->state() != QProcess::NotRunning) return;

    /* Distingui run manuale (utente) da run auto-fix */
    const bool isAutoFixRun = m_isFix;
    m_isFix = false;  /* reset: siamo nella fase di esecuzione, non AI */

    if (!isAutoFixRun) {
        /* Run manuale: azzera contatori e nascondi status */
        m_fixAttempts = 0;
        m_fixStatus->hide();
    }

    m_procOutput.clear();
    m_outputView->clear();
    setRunning(true);

    /* Determina il linguaggio effettivo */
    QString lang = m_detectedLang;
    if (lang.isEmpty()) {
        const QString sel = m_cmbLang->currentText();
        if (sel == "Python")             lang = "python";
        else if (sel == "C")             lang = "c";
        else if (sel == "C++")           lang = "c++";
        else if (sel == "JavaScript (Node)") lang = "javascript";
        else if (sel == "Bash")          lang = "bash";
        else                             lang = "python";   /* default Auto */
    }

    /* Scrivi il codice su file temporaneo */
    QString ext;
    if      (lang == "python" || lang == "python3") ext = "py";
    else if (lang == "c")                           ext = "c";
    else if (lang == "c++" || lang == "cpp")        ext = "cpp";
    else if (lang == "javascript" || lang == "js")  ext = "js";
    else if (lang == "bash" || lang == "sh")        ext = "sh";
    else                                            ext = "txt";

    const QString srcPath = QString("/tmp/prisma_prog.%1").arg(ext);
    QFile f(srcPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        appendOutput("Errore: impossibile scrivere il file temporaneo.\n", true);
        setRunning(false);
        return;
    }
    QTextStream(&f) << m_lastCode;
    f.close();

    m_proc = new QProcess(this);
    m_proc->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_proc, &QProcess::readyReadStandardOutput, this, [this]{
        const QByteArray data = m_proc->readAllStandardOutput();
        m_procOutput += QString::fromLocal8Bit(data);
        appendOutput(data);
    });

    connect(m_proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, lang](int code, QProcess::ExitStatus) {
        appendOutput(QString("\n\xe2\x94\x80\xe2\x94\x80  Terminato con codice %1\n").arg(code));

        /* Se c'è un plot matplotlib, mostralo */
        if (!m_plotPath.isEmpty() && QFile::exists(m_plotPath)) {
            appendOutput(QString("[Plot salvato: %1 — apri con un visualizzatore immagini]\n")
                         .arg(m_plotPath));
            m_btnSave->setEnabled(true);
        }

        /* maxFix: 0 = infinito (usiamo 99 come limite pratico), altrimenti usa il valore dello spinbox */
        const int maxFix = (m_spinFixIter->value() == 0) ? 99 : m_spinFixIter->value();
        if (code != 0 && m_fixAttempts < maxFix) {
            /* Errore: tenta auto-fix */
            autoFixCode(m_procOutput);
        } else {
            setRunning(false);
            if (code == 0 && m_fixAttempts > 0) {
                /* Fix riuscito: mostra messaggio di successo */
                const int maxFixShown = (m_spinFixIter->value() == 0) ? 99 : m_spinFixIter->value();
                m_fixStatus->setText(
                    QString("\xe2\x9c\x85  Corretto automaticamente al tentativo %1/%2")
                    .arg(m_fixAttempts).arg(maxFixShown));
                m_fixStatus->show();
            }
        }

        m_proc->deleteLater();
        m_proc = nullptr;
    });

    /* Avvia in base al linguaggio */
    if (lang == "python" || lang == "python3") {
        /* Inietta matplotlib.use('Agg') per evitare errori di display headless */
        if (m_lastCode.contains("import matplotlib") ||
            m_lastCode.contains("from matplotlib"))
        {
            QString patched = m_lastCode;
            patched.prepend("import matplotlib\nmatplotlib.use('Agg')\n");
            QFile fp(srcPath);
            if (fp.open(QIODevice::WriteOnly | QIODevice::Text))
                QTextStream(&fp) << patched;
        }
        m_proc->start("python3", {srcPath});

    } else if (lang == "c") {
        /* Prima compila, poi esegui */
        const QString outBin = "/tmp/prisma_prog_out";
        auto* compProc = new QProcess(this);
        compProc->setProcessChannelMode(QProcess::MergedChannels);
        appendOutput("[Compilazione gcc...]\n");

        connect(compProc, &QProcess::readyReadStandardOutput, this, [this, compProc]{
            const QByteArray data = compProc->readAllStandardOutput();
            m_procOutput += QString::fromLocal8Bit(data);
            appendOutput(data, true);
        });
        connect(compProc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, outBin, compProc](int code, QProcess::ExitStatus) {
            compProc->deleteLater();
            if (code != 0) {
                appendOutput("[Compilazione fallita — vedi errori sopra]\n", true);
                if (m_fixAttempts < (m_spinFixIter->value() == 0 ? 99 : m_spinFixIter->value()))
                    autoFixCode(m_procOutput);
                else
                    setRunning(false);
                m_proc->deleteLater(); m_proc = nullptr;
                return;
            }
            appendOutput("[Compilazione OK — avvio...]\n");
            m_proc->start(outBin, {});
        });
        compProc->start("gcc", {srcPath, "-o", outBin, "-lm"});
        return; /* il processo principale parte nel slot finished */

    } else if (lang == "c++" || lang == "cpp") {
        const QString outBin = "/tmp/prisma_prog_out";
        auto* compProc = new QProcess(this);
        compProc->setProcessChannelMode(QProcess::MergedChannels);
        appendOutput("[Compilazione g++ C++17...]\n");

        connect(compProc, &QProcess::readyReadStandardOutput, this, [this, compProc]{
            const QByteArray data = compProc->readAllStandardOutput();
            m_procOutput += QString::fromLocal8Bit(data);
            appendOutput(data, true);
        });
        connect(compProc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, outBin, compProc](int code, QProcess::ExitStatus) {
            compProc->deleteLater();
            if (code != 0) {
                appendOutput("[Compilazione fallita — vedi errori sopra]\n", true);
                if (m_fixAttempts < (m_spinFixIter->value() == 0 ? 99 : m_spinFixIter->value()))
                    autoFixCode(m_procOutput);
                else
                    setRunning(false);
                m_proc->deleteLater(); m_proc = nullptr;
                return;
            }
            appendOutput("[Compilazione OK — avvio...]\n");
            m_proc->start(outBin, {});
        });
        compProc->start("g++", {"-std=c++17", srcPath, "-o", outBin});
        return;

    } else if (lang == "javascript" || lang == "js") {
        m_proc->start("node", {srcPath});

    } else if (lang == "bash" || lang == "sh") {
        m_proc->start("bash", {srcPath});

    } else {
        appendOutput(
            QString("Linguaggio \"%1\" non supportato per l'esecuzione diretta.\n").arg(lang),
            true);
        setRunning(false);
        m_proc->deleteLater(); m_proc = nullptr;
        return;
    }

    /* Errore di avvio processo */
    connect(m_proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError err) {
        if (err == QProcess::FailedToStart) {
            appendOutput("Errore: interprete/compilatore non trovato nel PATH.\n"
                         "Verifica che python3 / gcc / g++ / node / bash siano installati.\n",
                         true);
            setRunning(false);
        }
    });
}

/* ══════════════════════════════════════════════════════════════
   killProcess — termina il processo in esecuzione
   ══════════════════════════════════════════════════════════════ */
void ProgrammazionePage::killProcess() {
    if (m_proc && m_proc->state() != QProcess::NotRunning) {
        m_proc->kill();
        appendOutput("\n[Processo terminato forzatamente]\n", true);
    }
}

/* ══════════════════════════════════════════════════════════════
   autoFixCode — invia codice + errore all'AI e rilancia runCode()
   Chiamato quando il processo termina con exit code ≠ 0.
   ══════════════════════════════════════════════════════════════ */
void ProgrammazionePage::autoFixCode(const QString& errOut) {
    ++m_fixAttempts;

    const int maxFix = (m_spinFixIter->value() == 0) ? 99 : m_spinFixIter->value();
    const QString maxStr = (m_spinFixIter->value() == 0)
                           ? "\xe2\x88\x9e"   /* ∞ */
                           : QString::number(maxFix);
    m_fixStatus->setText(
        QString("\xf0\x9f\x94\xa7  Sto risolvendo bug sintattici e/o logici... "
                "(tentativo %1/%2)")
        .arg(m_fixAttempts).arg(maxStr));
    m_fixStatus->show();

    /* Disabilita UI durante la chiamata AI di fix */
    m_btnRun->setEnabled(false);
    m_btnKill->setEnabled(false);
    m_btnGen->setEnabled(false);
    m_btnFix->setEnabled(false);
    m_btnStop->setEnabled(true);

    /* Prepara il messaggio di fix per l'AI */
    const QString lang = m_detectedLang.isEmpty() ? "codice" : m_detectedLang;
    const QString msg = QString(
        "Il seguente codice %1 ha prodotto questo errore:\n\n"
        "```\n%2\n```\n\n"
        "Codice attuale:\n\n"
        "```%3\n%4\n```\n\n"
        "Correggi il codice in modo che compili ed esegua senza errori. "
        "Rispondi SOLO con il codice corretto in un singolo blocco markdown "
        "```%3 ... ``` — zero spiegazioni, solo il codice funzionante.")
        .arg(lang,
             errOut.trimmed().left(2000),  /* limita a 2000 char per non riempire il context */
             lang,
             m_lastCode);

    const QString sysPrompt =
        "Sei un esperto debugger. Il tuo unico compito \xc3\xa8 correggere il codice. "
        "Rispondi SOLO con il codice corretto in un singolo blocco markdown ```lang ... ```. "
        "Nessuna spiegazione, nessun testo fuori dal blocco codice.";

    m_lastAiResponse.clear();
    m_codeView->clear();

    /* Imposta il modello scelto per il fix */
    QString fixModel = m_cmbFixModel->currentText().trimmed();
    if (fixModel.startsWith("\xf0\x9f\x92\xbb"))
        fixModel = fixModel.mid(6).trimmed();
    if (!fixModel.isEmpty() && !fixModel.startsWith("(") && fixModel != m_ai->model())
        m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), fixModel);

    m_isFix = true;  /* onFinished() sa che deve chiamare runCode() */
    m_ai->chat(sysPrompt, msg);
}

/* ══════════════════════════════════════════════════════════════
   saveOutput — dialog con checkbox per scegliere cosa esportare
   ══════════════════════════════════════════════════════════════ */
void ProgrammazionePage::saveOutput() {
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle("\xf0\x9f\x92\xbe  Salva output — Prismalux");
    dlg->setFixedWidth(380);
    auto* lay = new QVBoxLayout(dlg);
    lay->setSpacing(10);

    lay->addWidget(new QLabel(
        "<b>Seleziona cosa esportare:</b><br>"
        "<span style='color:#8a8fb0;font-size:11px;'>"
        "I file saranno salvati nella cartella scelta.</span>", dlg));

    const bool hasCode    = !m_lastCode.isEmpty();
    const bool hasOutput  = !m_outputView->toPlainText().isEmpty();
    const bool hasPrompt  = !m_lastPrompt.isEmpty();
    const bool hasAiResp  = !m_lastAiResponse.isEmpty();
    const bool hasPlot    = !m_plotPath.isEmpty() && QFile::exists(m_plotPath);

    QString ext = "txt";
    if (m_detectedLang == "python" || m_detectedLang == "python3") ext = "py";
    else if (m_detectedLang == "c")                                  ext = "c";
    else if (m_detectedLang == "c++" || m_detectedLang == "cpp")     ext = "cpp";
    else if (m_detectedLang == "javascript" || m_detectedLang == "js") ext = "js";
    else if (m_detectedLang == "bash" || m_detectedLang == "sh")     ext = "sh";

    auto* chkCode   = new QCheckBox(
        QString("\xf0\x9f\x93\x84  Codice generato (.%1)").arg(ext), dlg);
    auto* chkOutput = new QCheckBox("\xf0\x9f\x96\xa5  Output esecuzione (.txt)", dlg);
    auto* chkPrompt = new QCheckBox("\xf0\x9f\x92\xac  Prompt inviato (.txt)", dlg);
    auto* chkAiResp = new QCheckBox("\xf0\x9f\xa4\x96  Risposta completa AI (.md)", dlg);
    auto* chkPlot   = new QCheckBox("\xf0\x9f\x93\x88  Grafico generato (.png)", dlg);

    chkCode->setChecked(hasCode);    chkCode->setEnabled(hasCode);
    chkOutput->setChecked(hasOutput); chkOutput->setEnabled(hasOutput);
    chkPrompt->setChecked(hasPrompt); chkPrompt->setEnabled(hasPrompt);
    chkAiResp->setChecked(hasAiResp); chkAiResp->setEnabled(hasAiResp);
    chkPlot->setChecked(hasPlot);    chkPlot->setEnabled(hasPlot);

    lay->addWidget(chkCode);
    lay->addWidget(chkOutput);
    lay->addWidget(chkPrompt);
    lay->addWidget(chkAiResp);
    if (hasPlot) lay->addWidget(chkPlot);

    auto* bb = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, dlg);
    lay->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, dlg, &QDialog::reject);

    if (dlg->exec() != QDialog::Accepted) {
        dlg->deleteLater();
        return;
    }

    /* Cartella destinazione */
    const QString dir = QFileDialog::getExistingDirectory(
        this, "Seleziona cartella di destinazione",
        QDir::homePath());
    if (dir.isEmpty()) { dlg->deleteLater(); return; }

    const QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    int saved = 0;

    auto writeFile = [&](const QString& name, const QString& content) {
        QFile f(dir + "/" + name);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream(&f) << content;
            ++saved;
        }
    };

    if (chkCode->isChecked() && hasCode)
        writeFile(QString("prismalux_codice_%1.%2").arg(stamp, ext), m_lastCode);

    if (chkOutput->isChecked() && hasOutput)
        writeFile(QString("prismalux_output_%1.txt").arg(stamp),
                  m_outputView->toPlainText());

    if (chkPrompt->isChecked() && hasPrompt)
        writeFile(QString("prismalux_prompt_%1.txt").arg(stamp), m_lastPrompt);

    if (chkAiResp->isChecked() && hasAiResp)
        writeFile(QString("prismalux_ai_%1.md").arg(stamp), m_lastAiResponse);

    if (chkPlot && chkPlot->isChecked() && hasPlot) {
        QFile::copy(m_plotPath,
            QString("%1/prismalux_plot_%2.png").arg(dir, stamp));
        ++saved;
    }

    dlg->deleteLater();

    appendOutput(QString("\n\xe2\x9c\x85  Salvati %1 file in: %2\n").arg(saved).arg(dir));
}

/* ══════════════════════════════════════════════════════════════
   appendOutput — aggiunge testo al pannello output con colore
   ══════════════════════════════════════════════════════════════ */
void ProgrammazionePage::appendOutput(const QString& text, bool isError) {
    QTextCursor c = m_outputView->textCursor();
    c.movePosition(QTextCursor::End);
    QTextCharFormat fmt;
    fmt.setForeground(isError ? QColor("#ff6b6b") : QColor("#e0e0e0"));
    c.insertText(text, fmt);
    m_outputView->setTextCursor(c);
    m_outputView->ensureCursorVisible();
}

/* ══════════════════════════════════════════════════════════════
   setRunning — abilita/disabilita UI durante esecuzione
   ══════════════════════════════════════════════════════════════ */
void ProgrammazionePage::setRunning(bool running) {
    m_btnRun->setEnabled(!running && !m_lastCode.isEmpty());
    m_btnKill->setEnabled(running);
    m_btnGen->setEnabled(!running);
    /* Abilita "Correggi con AI" solo quando non in esecuzione E c'è codice */
    m_btnFix->setEnabled(!running && !m_lastCode.isEmpty());
}

/* ══════════════════════════════════════════════════════════════
   Event filter — Ctrl+Enter nel task input
   ══════════════════════════════════════════════════════════════ */
bool ProgrammazionePage::eventFilter(QObject* obj, QEvent* ev) {
    if (obj == m_taskInput && ev->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(ev);
        if ((ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) &&
            ke->modifiers() & Qt::ControlModifier)
        {
            generateCode();
            return true;
        }
    }
    return QWidget::eventFilter(obj, ev);
}

/* ══════════════════════════════════════════════════════════════
   Slot AI
   ══════════════════════════════════════════════════════════════ */
void ProgrammazionePage::onToken(const QString& t) {
    m_lastAiResponse += t;
    /* Mostra streaming nel code view (sarà sostituito da extractCode alla fine) */
    m_codeView->setPlainText(m_lastAiResponse);
    QTextCursor c = m_codeView->textCursor();
    c.movePosition(QTextCursor::End);
    m_codeView->setTextCursor(c);
}

void ProgrammazionePage::onFinished(const QString& full) {
    m_lastAiResponse = full;
    m_btnStop->setEnabled(false);
    m_btnGen->setEnabled(true);
    extractCode(full);

    if (m_isFix) {
        /* Siamo in un ciclo auto-fix: esegui il codice corretto */
        runCode();
    }
}

void ProgrammazionePage::onError(const QString& msg) {
    m_btnStop->setEnabled(false);
    m_btnGen->setEnabled(true);
    m_isFix = false;  /* interrompi il loop auto-fix in caso di errore AI */
    if (m_fixAttempts > 0) {
        const QString maxStr2 = (m_spinFixIter->value() == 0)
                                ? "\xe2\x88\x9e" : QString::number(m_spinFixIter->value());
        m_fixStatus->setText(
            QString("\xe2\x9d\x8c  Auto-fix interrotto (errore AI al tentativo %1/%2)")
            .arg(m_fixAttempts).arg(maxStr2));
        m_fixStatus->show();
        setRunning(false);
    } else {
        m_langBadge->setText("Errore generazione");
    }
    appendOutput(QString("Errore AI: %1\n").arg(msg), true);
}
