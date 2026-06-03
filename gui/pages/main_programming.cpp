#include "main_programming.h"
#include "widget_code_interpreter.h"
#include "../prismalux_paths.h"
#include "../ai_utils.h"
#include "../widgets/ai_error_widget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QProcess>
#include <QGroupBox>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QApplication>
#include <QDateTime>
#include <QTextCursor>
#include <QFrame>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QMessageBox>
#include <QCheckBox>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTextEdit>
#include <QSlider>
#include <QTimer>
#include <QFileDialog>
#include <QFile>
#include <QKeyEvent>
#include <QSpinBox>
#include <QNetworkInterface>
#include <QHostAddress>
#include <QTableWidget>
#include <QHeaderView>
#include "../widgets/toggle_switch.h"

namespace P = PrismaluxPaths;
#include <QBrush>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QColor>
#include <QDesktopServices>

/* ══════════════════════════════════════════════════════════════
   isIntentionalError — rileva errori volutamente creati dall'utente.

   Regola: se l'output contiene "SyntaxError" E il sorgente contiene
   un'istruzione `raise SyntaxError(...)` → errore custom intenzionale.
   Il loop si ferma: l'utente vuole quel comportamento nel codice.

   Esteso anche ad altri pattern comuni di errori custom intenzionali:
   - raise ValueError / raise TypeError / raise CustomException
     quando la classe è definita nel sorgente stesso.
   ══════════════════════════════════════════════════════════════ */
bool ProgrammazionePage::isIntentionalError(const QString& errorOut,
                                             const QString& source)
{
    /* Pattern 1: SyntaxError nell'output + raise SyntaxError nel sorgente */
    if (errorOut.contains("SyntaxError")) {
        static const QRegularExpression reRaiseSyntax(
            R"(\braise\s+SyntaxError\b)");
        if (reRaiseSyntax.match(source).hasMatch())
            return true;
    }

    /* Pattern 2: qualsiasi "raise XxxError" dove la classe è definita nel sorgente
       (es. class MyError(Exception): pass; raise MyError()) */
    {
        static const QRegularExpression reRaise(
            R"(\braise\s+(\w+)\s*[(\n])");
        static const QRegularExpression reClass(
            R"(\bclass\s+(\w+)\s*\()");

        /* Raccogli le classi definite nel sorgente */
        QSet<QString> definedClasses;
        auto mClass = reClass.globalMatch(source);
        while (mClass.hasNext())
            definedClasses.insert(mClass.next().captured(1));

        /* Controlla se una raise usa una di quelle classi */
        auto mRaise = reRaise.globalMatch(source);
        while (mRaise.hasNext()) {
            const QString raised = mRaise.next().captured(1);
            if (definedClasses.contains(raised) && errorOut.contains(raised))
                return true;
        }
    }

    return false;
}

/* Dimensione font mono DPI-aware: segue il font applicazione (già scalato da Qt/OS).
   Su 4K con HiDPI abilitato, QApplication::font().pointSize() sarà già più grande. */
static int monoFontPt(int fallback = 11) {
    const int appPt = QApplication::font().pointSize();
    return (appPt > 0) ? appPt : fallback;
}

/* Event filter: intercetta drop di file sull'editor e ne inserisce il contenuto. */
class EditorFileDropFilter : public QObject {
public:
    using QObject::QObject;
protected:
    bool eventFilter(QObject* obj, QEvent* ev) override {
        if (ev->type() != QEvent::Drop) return false;
        auto* de = static_cast<QDropEvent*>(ev);
        if (!de->mimeData()->hasUrls()) return false;
        auto* editor = qobject_cast<QPlainTextEdit*>(obj);
        if (!editor) return false;

        QString content;
        for (const QUrl& url : de->mimeData()->urls()) {
            if (!url.isLocalFile()) continue;
            QFile f(url.toLocalFile());
            if (f.open(QIODevice::ReadOnly | QIODevice::Text))
                content += QString::fromUtf8(f.readAll());
        }
        if (!content.isEmpty()) {
            QTextCursor cur = editor->textCursor();
            cur.select(QTextCursor::Document);
            cur.insertText(content);        /* entra nello stack undo */
            de->acceptProposedAction();
            return true;
        }
        return false;
    }
};

/* ══════════════════════════════════════════════════════════════
   ProgrammazionePage — costruttore
   ══════════════════════════════════════════════════════════════ */
ProgrammazionePage::~ProgrammazionePage()
{
    if (m_replProc) {
        m_replProc->disconnect();
        m_replProc->kill();
    }
}

ProgrammazionePage::ProgrammazionePage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    auto* mainLay = new QVBoxLayout(this);
    mainLay->setContentsMargins(12, 12, 12, 12);
    mainLay->setSpacing(8);

    m_innerTabs = new QTabWidget(this);
    m_innerTabs->setObjectName("innerTabs");
    mainLay->addWidget(m_innerTabs, 1);

    buildInnerTabs();
    m_editor->setPlainText(currentTemplate());
}

/* ── Livello 1: crea tutti i tab interni ── */
void ProgrammazionePage::buildInnerTabs()
{
    /* Titolo nella tab bar */
    auto* titleCorner = new QLabel(
        "\xf0\x9f\x92\xbb  Programmazione", m_innerTabs);
    titleCorner->setObjectName("pageTitle");
    titleCorner->setContentsMargins(4, 0, 16, 0);
    m_innerTabs->setCornerWidget(titleCorner, Qt::TopLeftCorner);

    m_innerTabs->addTab(buildCodingTab(m_innerTabs),
        "\xf0\x9f\x92\xbb  Programmazione");
    m_innerTabs->addTab(buildAgentica(m_innerTabs),
        "\xf0\x9f\xa4\x96  Agentica");
    m_innerTabs->addTab(buildTranslitter(m_innerTabs),
        "\xf0\x9f\x94\x80  Translitter");
    m_innerTabs->addTab(buildReverseEngineering(m_innerTabs),
        "\xf0\x9f\x94\x8d  Reverse Eng.");
    m_innerTabs->addTab(buildGitMcp(m_innerTabs),
        "\xf0\x9f\x94\xa7  Git");
    m_innerTabs->addTab(buildPythonRepl(m_innerTabs),
        "\xf0\x9f\x90\x8d  REPL");
    m_innerTabs->addTab(new CodeInterpreterWidget(m_ai, m_innerTabs),
        "\xf0\x9f\xa7\xaa  Interpreter");

    /* Rete & Network: sub-tab interni */
    auto* reteWrap = new QWidget(m_innerTabs);
    auto* reteLay  = new QVBoxLayout(reteWrap);
    reteLay->setContentsMargins(0, 0, 0, 0);
    reteLay->setSpacing(0);
    auto* reteTabs = new QTabWidget(reteWrap);
    reteTabs->setObjectName("innerTabs");
    reteTabs->addTab(buildNetworkAnalyzer(reteTabs),
        "\xf0\x9f\x94\xa1  Cattura pacchetti");
    reteTabs->addTab(buildReteLan(reteTabs),
        "\xf0\x9f\x8c\x90  Scan LAN");
    reteLay->addWidget(reteTabs);
    m_innerTabs->addTab(reteWrap,
        "\xf0\x9f\x8c\x90  Rete & Network");

    m_innerTabs->addTab(buildDriverKernelTab(m_innerTabs),
        "\xf0\x9f\x94\xa7  Driver & Kernel");
}

/* ── Livello 1: tab Coding completo ── */
QWidget* ProgrammazionePage::buildCodingTab(QWidget* parent)
{
    auto* codingTab = new QWidget(parent);
    auto* codingLay = new QVBoxLayout(codingTab);
    codingLay->setContentsMargins(0, 8, 0, 0);
    codingLay->setSpacing(8);

    QPushButton* btnClear      = nullptr;
    QPushButton* btnRefreshMod = nullptr;
    QPushButton* btnCloseAi    = nullptr;

    codingLay->addWidget(buildCodingToolbar(codingTab, btnClear));

    auto* mainSplit = new QSplitter(Qt::Horizontal, codingTab);
    mainSplit->setChildrenCollapsible(false);
    mainSplit->addWidget(buildEditorColumn(mainSplit));
    mainSplit->addWidget(buildOutputColumn(mainSplit));
    mainSplit->setSizes({500, 450});
    codingLay->addWidget(mainSplit, 1);

    codingLay->addWidget(buildAiPanel(codingTab, btnRefreshMod, btnCloseAi));

    m_fixErrPanel = new AiErrorWidget(codingTab);
    codingLay->addWidget(m_fixErrPanel);

    setupCodingConnections(btnClear, btnRefreshMod, btnCloseAi);
    return codingTab;
}

/* ── Livello 2: toolbar del tab Coding ── */
QWidget* ProgrammazionePage::buildCodingToolbar(QWidget* parent,
                                                 QPushButton*& outBtnClear)
{
    auto* toolRow = new QWidget(parent);
    auto* toolLay = new QHBoxLayout(toolRow);
    toolLay->setContentsMargins(0, 0, 0, 0);
    toolLay->setSpacing(8);

    toolLay->addWidget(new QLabel("Linguaggio:", toolRow));
    m_lang = new QComboBox(toolRow);
    m_lang->setObjectName("settingCombo");
    m_lang->addItem("Python",     QString("py"));
    m_lang->addItem("C",          QString("c"));
    m_lang->addItem("C++",        QString("cpp"));
    m_lang->addItem("Bash",       QString("sh"));
    m_lang->addItem("JavaScript", QString("js"));
    m_lang->setFixedWidth(130);
    toolLay->addWidget(m_lang);
    toolLay->addSpacing(8);

    auto tagExecP = [](QPushButton* btn, const char* icon, const char* text){
        btn->setProperty("execFull", btn->text());
        btn->setProperty("execIcon", QString::fromUtf8(icon));
        btn->setProperty("execText", QString::fromUtf8(text));
    };

    m_btnRun = new QPushButton("\xe2\x96\xb6  Esegui", toolRow);
    m_btnRun->setObjectName("actionBtn");
    m_btnRun->setToolTip("Esegui il codice nell’editor (F5)");
    tagExecP(m_btnRun, "\xe2\x96\xb6", "Esegui");

    auto* btnClear = new QPushButton("\xf0\x9f\x97\x91  Pulisci", toolRow);
    btnClear->setObjectName("actionBtn");

    toolLay->addWidget(m_btnRun);
    toolLay->addWidget(btnClear);
    toolLay->addStretch(1);

    m_btnAi = new QPushButton("\xf0\x9f\xa4\x96  Chiedi all’AI", toolRow);
    m_btnAi->setObjectName("actionBtn");
    m_btnAi->setCheckable(true);
    m_btnAi->setToolTip("Apri il pannello AI per scrivere una richiesta");
    tagExecP(m_btnAi, "\xf0\x9f\xa4\x96", "Chiedi all’AI");
    toolLay->addWidget(m_btnAi);

    m_btnFix = new QPushButton("\xf0\x9f\x94\xa7  Correggi con AI", toolRow);
    m_btnFix->setObjectName("actionBtn");
    m_btnFix->setProperty("highlight", "true");
    tagExecP(m_btnFix, "\xf0\x9f\x94\xa7", "Correggi con AI");
    m_btnFix->setToolTip(
        "Invia il codice a qwen2.5-coder (o il miglior modello coder disponibile)\n"
        "e chiedi di trovare e correggere tutti i bug.\n"
        "Se c’era un errore di esecuzione, viene incluso nel contesto.");
    toolLay->addWidget(m_btnFix);

    /* Toggle "Loop Fix" */
    toolLay->addSpacing(10);
    m_toggleAutoFix = new ToggleSwitch("Loop Fix", toolRow);
    m_toggleAutoFix->setToolTip(
        "ON  \xe2\x86\x92 esegue in loop: errore \xe2\x86\x92 AI corregge \xe2\x86\x92 riesegue, "
        "fino a successo (max 6 tentativi).\n"
        "Si ferma se trova un SyntaxError creato deliberatamente (raise SyntaxError).\n"
        "OFF \xe2\x86\x92 esecuzione singola, correzione manuale.");
    connect(m_toggleAutoFix, &QAbstractButton::toggled,
            this, &ProgrammazionePage::onAutoFixToggled);
    toolLay->addWidget(m_toggleAutoFix);

    /* Slider iterazioni Loop Fix */
    toolLay->addSpacing(6);
    m_fixSlider = new QSlider(Qt::Horizontal, toolRow);
    m_fixSlider->setRange(1, 10);
    m_fixSlider->setValue(m_loopMax);
    m_fixSlider->setFixedWidth(80);
    m_fixSlider->setToolTip("Numero massimo di tentativi Loop Fix (10 = illimitati)");
    m_fixSliderLbl = new QLabel(QString::number(m_loopMax), toolRow);
    m_fixSliderLbl->setObjectName("cardDesc");
    m_fixSliderLbl->setFixedWidth(22);
    m_fixSliderLbl->setAlignment(Qt::AlignCenter);
    connect(m_fixSlider, &QSlider::valueChanged,
            this, &ProgrammazionePage::onFixSliderChanged);
    toolLay->addWidget(m_fixSlider);
    toolLay->addWidget(m_fixSliderLbl);

    outBtnClear = btnClear;
    return toolRow;
}

/* ── Livello 2: colonna sinistra con editor ── */
QWidget* ProgrammazionePage::buildEditorColumn(QWidget* parent)
{
    QFont monoFont;
    monoFont.setFamily("JetBrains Mono");
    monoFont.setStyleHint(QFont::Monospace);
    monoFont.setPointSize(monoFontPt(11));

    auto* editorGroup = new QGroupBox("\xf0\x9f\x96\x8a  Codice", parent);
    editorGroup->setObjectName("cardGroup");
    auto* editorLay = new QVBoxLayout(editorGroup);
    editorLay->setContentsMargins(4, 8, 4, 4);

    m_editor = new QPlainTextEdit(editorGroup);
    m_editor->setObjectName("codeEditor");
    m_editor->setFont(monoFont);
    m_editor->setTabStopDistance(32);
    m_editor->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_editor->setPlaceholderText(
        "# Scrivi il codice qui,\n"
        "# oppure usa \xf0\x9f\xa4\x96 Chiedi all’AI per generarlo.\n"
        "# Trascina un file qui per aprirlo.");
    m_editor->setAcceptDrops(true);
    m_editor->installEventFilter(new EditorFileDropFilter(m_editor));
    editorLay->addWidget(m_editor);

    m_highlighter = new CodeHighlighter(m_editor->document());
    m_highlighter->setLanguage(CodeHighlighter::Python);

    return editorGroup;
}

/* ── Livello 2: colonna destra con output + grafico ── */
QWidget* ProgrammazionePage::buildOutputColumn(QWidget* parent)
{
    QFont monoFont;
    monoFont.setFamily("JetBrains Mono");
    monoFont.setStyleHint(QFont::Monospace);
    monoFont.setPointSize(monoFontPt(11));

    auto* rightCol = new QWidget(parent);
    auto* rightLay = new QVBoxLayout(rightCol);
    rightLay->setContentsMargins(0, 0, 0, 0);
    rightLay->setSpacing(6);

    auto* outputGroup = new QGroupBox("\xf0\x9f\x93\x9f  Output", rightCol);
    outputGroup->setObjectName("cardGroup");
    auto* outputLay = new QVBoxLayout(outputGroup);
    outputLay->setContentsMargins(4, 8, 4, 4);
    outputLay->setSpacing(4);

    m_status = new QLabel("Pronto.", outputGroup);
    m_status->setObjectName("hintLabel");
    outputLay->addWidget(m_status);

    m_output = new QPlainTextEdit(outputGroup);
    m_output->setObjectName("chatLog");
    m_output->setReadOnly(true);
    m_output->setFont(monoFont);
    m_output->setMaximumBlockCount(2000);
    m_output->setMinimumHeight(100);
    outputLay->addWidget(m_output, 1);
    rightLay->addWidget(outputGroup, 1);

    auto* chartGroup = new QGroupBox(
        "\xf0\x9f\x93\x88  Grafico \xe2\x80\x94 output numerico", rightCol);
    chartGroup->setObjectName("cardGroup");
    auto* chartLay = new QVBoxLayout(chartGroup);
    chartLay->setContentsMargins(4, 8, 4, 4);

    m_chart = new ChartWidget(chartGroup);
    m_chart->setMinimumHeight(180);
    chartLay->addWidget(m_chart);
    chartGroup->hide();
    rightLay->addWidget(chartGroup);
    m_chartGroup = chartGroup;

    return rightCol;
}

/* ── Livello 2: pannello AI (nascosto di default) ── */
QWidget* ProgrammazionePage::buildAiPanel(QWidget* parent,
                                           QPushButton*& outBtnRefreshMod,
                                           QPushButton*& outBtnCloseAi)
{
    QFont monoFont;
    monoFont.setFamily("JetBrains Mono");
    monoFont.setStyleHint(QFont::Monospace);
    monoFont.setPointSize(monoFontPt(11));

    m_aiPanel = new QWidget(parent);
    m_aiPanel->setObjectName("aiPanel");
    auto* aiLay = new QVBoxLayout(m_aiPanel);
    aiLay->setContentsMargins(8, 8, 8, 8);
    aiLay->setSpacing(6);

    auto* aiSep = new QFrame(m_aiPanel);
    aiSep->setFrameShape(QFrame::HLine);
    aiSep->setObjectName("sidebarSep");
    aiLay->addWidget(aiSep);

    /* Riga selezione modello */
    auto* modelRow = new QWidget(m_aiPanel);
    auto* modelLay = new QHBoxLayout(modelRow);
    modelLay->setContentsMargins(0, 0, 0, 0);
    modelLay->setSpacing(8);

    auto* lblModel = new QLabel("\xf0\x9f\xa4\x96  Modello:", modelRow);
    lblModel->setObjectName("cardDesc");
    modelLay->addWidget(lblModel);

    m_modelCombo = new QComboBox(modelRow);
    m_modelCombo->setObjectName("settingCombo");
    m_modelCombo->addItem(
        m_ai ? (m_ai->model().isEmpty()
                    ? "(nessun modello)"
                    : m_ai->model())
             : "(AI non disponibile)",
        m_ai ? m_ai->model() : QString());
    modelLay->addWidget(m_modelCombo, 1);

    outBtnRefreshMod = new QPushButton("\xf0\x9f\x94\x84", modelRow);
    outBtnRefreshMod->setObjectName("actionBtn");
    outBtnRefreshMod->setFixedWidth(32);
    outBtnRefreshMod->setToolTip("Aggiorna lista modelli disponibili");
    modelLay->addWidget(outBtnRefreshMod);
    aiLay->addWidget(modelRow);

    /* Riga input */
    auto* aiInputRow = new QWidget(m_aiPanel);
    auto* aiInputLay = new QHBoxLayout(aiInputRow);
    aiInputLay->setContentsMargins(0, 0, 0, 0);
    aiInputLay->setSpacing(8);

    auto* aiIcon = new QLabel("\xf0\x9f\xa4\x96", aiInputRow);
    aiIcon->setObjectName("cardIcon");
    aiInputLay->addWidget(aiIcon);

    m_aiInput = new QLineEdit(aiInputRow);
    m_aiInput->setObjectName("chatInput");
    m_aiInput->setPlaceholderText(
        "Scrivi la tua richiesta...  es: \"scrivi una funzione che stampa Fibonacci\", "
        "\"spiega questo codice\", \"trova i bug\"");
    aiInputLay->addWidget(m_aiInput, 1);

    m_btnSend = new QPushButton("Invia \xe2\x96\xb6", aiInputRow);
    m_btnSend->setObjectName("m_btnSend");

    m_btnInsert = new QPushButton(
        "\xe2\x86\x91  Inserisci in editor", aiInputRow);
    m_btnInsert->setObjectName("actionBtn");
    m_btnInsert->setToolTip(
        "Estrae il primo blocco di codice dalla risposta AI e lo inserisce nell’editor");
    m_btnInsert->setEnabled(false);

    outBtnCloseAi = new QPushButton("\xe2\x9c\x95", aiInputRow);
    outBtnCloseAi->setObjectName("actionBtn");
    outBtnCloseAi->setFixedWidth(32);
    outBtnCloseAi->setToolTip("Chiudi pannello AI");

    aiInputLay->addWidget(m_btnSend);
    aiInputLay->addWidget(m_btnInsert);
    aiInputLay->addWidget(outBtnCloseAi);
    aiLay->addWidget(aiInputRow);

    m_aiOutput = new QPlainTextEdit(m_aiPanel);
    m_aiOutput->setObjectName("chatLog");
    m_aiOutput->setReadOnly(true);
    m_aiOutput->setFont(monoFont);
    m_aiOutput->setMaximumBlockCount(3000);
    m_aiOutput->setMinimumHeight(100);
    m_aiOutput->setMaximumHeight(220);
    m_aiOutput->setPlaceholderText(
        "Qui apparir\xc3\xa0 la risposta dell’AI.\n\n"
        "Scrivi la tua richiesta sopra e premi Invia.");
    aiLay->addWidget(m_aiOutput);

    m_aiPanel->hide();
    return m_aiPanel;
}

/* ── Livello 2: connessioni del tab Coding ── */
void ProgrammazionePage::setupCodingConnections(QPushButton* btnClear,
                                                 QPushButton* btnRefreshMod,
                                                 QPushButton* btnCloseAi)
{
    connect(m_lang, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ProgrammazionePage::onLangChanged);

    if (btnClear)
        connect(btnClear, &QPushButton::clicked,
                this, &ProgrammazionePage::onBtnClearClicked);

    connect(m_btnAi, &QPushButton::toggled,
            this, &ProgrammazionePage::onBtnAiToggled);

    if (btnCloseAi)
        connect(btnCloseAi, &QPushButton::clicked,
                this, &ProgrammazionePage::onBtnCloseAiClicked);

    connect(m_btnSend,  &QPushButton::clicked,
            this, &ProgrammazionePage::sendToAi);
    connect(m_aiInput,  &QLineEdit::returnPressed,
            this, &ProgrammazionePage::sendToAi);
    connect(m_btnInsert, &QPushButton::clicked,
            this, &ProgrammazionePage::onBtnInsertClicked);
    connect(m_btnRun,  &QPushButton::clicked,
            this, &ProgrammazionePage::onBtnRunClicked);
    connect(m_btnFix,  &QPushButton::clicked,
            this, &ProgrammazionePage::onBtnFixClicked);
    connect(m_ai, &AiClient::modelChanged,
            this, &ProgrammazionePage::onModelChanged);

    if (btnRefreshMod)
        connect(btnRefreshMod, &QPushButton::clicked,
                this, &ProgrammazionePage::populateAiModels);

    QTimer::singleShot(0, this, &ProgrammazionePage::populateAiModels);

    /* Tab order */
    QWidget::setTabOrder(m_lang,       m_btnRun);
    QWidget::setTabOrder(m_btnRun,     m_btnAi);
    QWidget::setTabOrder(m_btnAi,      m_btnFix);
    QWidget::setTabOrder(m_btnFix,     m_editor);
    QWidget::setTabOrder(m_editor,     m_modelCombo);
    QWidget::setTabOrder(m_modelCombo, m_aiInput);
    QWidget::setTabOrder(m_aiInput,    m_btnSend);
    QWidget::setTabOrder(m_btnSend,    m_btnInsert);
}

/* ══════════════════════════════════════════════════════════════
   parseNumbers — estrae tutti i numeri dall'output (righe o
   spazi separati). Ritorna lista vuota se meno di 3 valori o
   se l'output contiene soprattutto testo.
   ══════════════════════════════════════════════════════════════ */
QVector<double> ProgrammazionePage::parseNumbers(const QString& text)
{
    QVector<double> nums;
    int nonNumericLines = 0;

    for (const QString& rawLine : text.split('\n')) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty() || line.startsWith('$')) continue;

        /* Tokenizza: spazi, virgole, tab, punto-e-virgola */
        static QRegularExpression splitter("[\\s,;]+");
        const QStringList tokens = line.split(splitter, Qt::SkipEmptyParts);

        bool allNumeric = true;
        QVector<double> lineVals;
        for (const QString& tok : tokens) {
            bool ok = false;
            double v = tok.toDouble(&ok);
            if (!ok) { allNumeric = false; break; }
            lineVals << v;
        }

        if (allNumeric && !lineVals.isEmpty()) {
            nums << lineVals;
        } else {
            nonNumericLines++;
        }
    }

    /* Mostra grafico solo se i numeri dominano l'output */
    if (nums.size() < 3) return {};
    if (nonNumericLines > nums.size()) return {};  /* più testo che numeri */
    return nums;
}

/* ══════════════════════════════════════════════════════════════
   tryShowChart — popola il ChartWidget con i numeri dell'output
   ══════════════════════════════════════════════════════════════ */
void ProgrammazionePage::tryShowChart()
{
    const QVector<double> vals = parseNumbers(m_fullOutput);
    if (vals.isEmpty()) return;

    m_chart->clearAll();
    m_chart->setTitle(m_lang->currentText() + " — output numerico");
    m_chart->setAxisLabels("indice", "valore");

    ChartWidget::Series s;
    s.name = "output";
    s.mode = ChartWidget::Line;
    for (int i = 0; i < vals.size(); ++i)
        s.pts << QPointF(i + 1, vals[i]);
    m_chart->addSeries(s);
    m_chart->update();
}

/* ══════════════════════════════════════════════════════════════
   extractCodeBlock — estrae il primo blocco ```...``` dalla
   risposta AI per inserirlo nell'editor.
   ══════════════════════════════════════════════════════════════ */
QString ProgrammazionePage::extractCodeBlock() const
{
    const QString text = m_aiOutput->toPlainText();
    static QRegularExpression re("```(?:\\w+)?\\n([\\s\\S]*?)```");
    const auto m = re.match(text);
    if (m.hasMatch()) return m.captured(1).trimmed();
    return {};
}

/* ══════════════════════════════════════════════════════════════
   buildRunCommand — comando shell per il linguaggio selezionato
   ══════════════════════════════════════════════════════════════ */
QString ProgrammazionePage::buildRunCommand(const QString& filePath) const
{
    const QString ext    = m_lang->currentData().toString();
    const QString fp     = QDir::toNativeSeparators(filePath);

    QString tmp = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (tmp.isEmpty() || QDir(tmp).isRelative()) tmp = QDir::homePath();
    const QString outBin = QDir::toNativeSeparators(
        tmp + "/prismalux_code_bin"
#ifdef _WIN32
        + ".exe"
#endif
    );

    if (ext == "py") {
#ifdef _WIN32
        /* Su Windows: usa il Python Launcher "py -3" (sempre disponibile
         * se Python è installato da python.org); fallback a "python" se py
         * non è nel PATH. "python3" spesso non esiste su Windows. */
        return QString("py -3 \"%1\" 2>&1 || python \"%1\" 2>&1").arg(fp);
#else
        return QString("python3 \"%1\"").arg(fp);
#endif
    }
    if (ext == "sh")  return QString("bash \"%1\"").arg(fp);
    if (ext == "js")  return QString("node \"%1\"").arg(fp);
    if (ext == "c")
        return QString("gcc -Wall -o \"%1\" \"%2\" && \"%1\"").arg(outBin, fp);
    if (ext == "cpp")
        return QString("g++ -Wall -std=c++17 -o \"%1\" \"%2\" && \"%1\"").arg(outBin, fp);
    return {};
}

/* ══════════════════════════════════════════════════════════════
   tempFilePath — percorso assoluto del file temporaneo.
   Bug Windows: quando l'app gira dentro C:\...\AppData\Local\Temp\
   il processo viene avviato con CWD = cartella app, e QDir::tempPath()
   può restituire un path relativo ("Temp") invece di assoluto.
   Fix: QStandardPaths::TempLocation è sempre assoluto su ogni OS.
   ══════════════════════════════════════════════════════════════ */
QString ProgrammazionePage::tempFilePath(const QString& ext) const
{
    QString tmp = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    /* Fallback: se per qualche ragione è vuoto o relativo, usa home */
    if (tmp.isEmpty() || QDir(tmp).isRelative())
        tmp = QDir::homePath();
    /* Usa sempre slash forward — Qt normalizza su tutti i OS */
    return tmp + "/prismalux_code." + ext;
}

/* ══════════════════════════════════════════════════════════════
   currentTemplate — codice di esempio per il linguaggio attivo
   ══════════════════════════════════════════════════════════════ */
QString ProgrammazionePage::currentTemplate() const
{
    const QString ext = m_lang->currentData().toString();

    if (ext == "py") return
        "# Successione di Fibonacci — output numerico\n"
        "def fibonacci(n):\n"
        "    a, b = 0, 1\n"
        "    for _ in range(n):\n"
        "        print(a)\n"
        "        a, b = b, a + b\n\n"
        "fibonacci(15)\n";

    if (ext == "c") return
        "/* Fattoriale ricorsivo */\n"
        "#include <stdio.h>\n\n"
        "long long fact(int n) {\n"
        "    return n <= 1 ? 1 : n * fact(n - 1);\n"
        "}\n\n"
        "int main() {\n"
        "    for (int i = 0; i <= 12; i++)\n"
        "        printf(\"%lld\\n\", fact(i));\n"
        "    return 0;\n"
        "}\n";

    if (ext == "cpp") return
        "// Successione quadrati\n"
        "#include <iostream>\n\n"
        "int main() {\n"
        "    for (int i = 1; i <= 20; i++)\n"
        "        std::cout << i * i << '\\n';\n"
        "    return 0;\n"
        "}\n";

    if (ext == "sh") return
        "#!/bin/bash\n"
        "# Numeri da 1 a 20\n"
        "for i in $(seq 1 20); do\n"
        "    echo $i\n"
        "done\n";

    if (ext == "js") return
        "// Successione di potenze di 2\n"
        "for (let i = 0; i < 15; i++) {\n"
        "    console.log(Math.pow(2, i));\n"
        "}\n";

    return "# Codice qui\n";
}

/* ══════════════════════════════════════════════════════════════
   appendOutput — aggiunge testo al pannello output
   ══════════════════════════════════════════════════════════════ */
void ProgrammazionePage::appendOutput(const QString& text)
{
    m_output->moveCursor(QTextCursor::End);
    m_output->insertPlainText(text);
    m_output->ensureCursorVisible();
}

/* ══════════════════════════════════════════════════════════════
   setRunning — aggiorna stato pulsanti toolbar
   ══════════════════════════════════════════════════════════════ */
void ProgrammazionePage::setRunning(bool running)
{
    if (running) {
        m_btnRun->setText("\xe2\x96\xa0  Stop");
        m_btnRun->setProperty("danger", true);
    } else {
        m_btnRun->setText("\xe2\x96\xb6  Esegui");
        m_btnRun->setProperty("danger", false);
    }
    m_btnRun->setEnabled(true);
    P::repolish(m_btnRun);
    m_lang->setEnabled(!running);
    m_btnFix->setEnabled(!running);
    /* Disabilita "Invia" durante lo streaming per impedire
       doppie connessioni al segnale token (causa output duplicato). */
    if (m_btnSend) m_btnSend->setEnabled(!running);
}

/* ══════════════════════════════════════════════════════════════
   runCode — esegue il codice presente nell'editor.

   Metodo estratto dalla lambda del pulsante "Esegui" per:
     1. Permettere chiamate dirette da _doFix (Loop Fix) senza
        simulare un click UI (antipattern m_btnRun->click()).
     2. Rendere la logica testabile indipendentemente dall'UI.
     3. Eliminare la cattura di variabili locali del costruttore.
   ══════════════════════════════════════════════════════════════ */
void ProgrammazionePage::runCode()
{
    const QString code = m_editor->toPlainText().trimmed();
    if (code.isEmpty()) {
        m_status->setText("\xe2\x9d\x8c  Nessun codice da eseguire.");
        return;
    }

    if (m_aiMode && m_ai && m_ai->busy()) m_ai->abort();
    m_aiMode = false;

    const QString ext      = m_lang->currentData().toString();
    const QString filePath = tempFilePath(ext);
    m_procFilePath = filePath;

    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_status->setText(QString("\xe2\x9d\x8c  Impossibile creare file temp: %1").arg(filePath));
        return;
    }
    f.write(code.toUtf8());
    f.close();

    const QString cmd = buildRunCommand(filePath);
    if (cmd.isEmpty()) {
        m_status->setText("\xe2\x9d\x8c  Linguaggio non supportato.");
        return;
    }

    m_output->clear();
    m_fullOutput.clear();
    m_chart->clearAll();
    if (m_chartGroup) m_chartGroup->hide();
    appendOutput(QString("$ %1\n").arg(cmd));
    setRunning(true);
    m_status->setText("\xe2\x8f\xb3  Esecuzione in corso...");

    if (m_proc) { m_proc->kill(); m_proc->deleteLater(); m_proc = nullptr; }
    m_proc = new QProcess(this);
    m_proc->setProcessChannelMode(QProcess::MergedChannels);
    m_proc->setWorkingDirectory(P::root());

    connect(m_proc, &QProcess::readyRead,
            this, &ProgrammazionePage::onProcReadyRead);

    connect(m_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ProgrammazionePage::onProcFinished);

    connect(m_proc, &QProcess::errorOccurred,
            this, &ProgrammazionePage::onProcErrorOccurred);
#ifdef _WIN32
    m_proc->start("cmd", {"/c", cmd});
#else
    m_proc->start("sh", {"-c", cmd});
#endif
}

/* ══════════════════════════════════════════════════════════════
   selectCoderModel — seleziona qwen2.5-coder o il miglior modello
   coder disponibile nella combo del pannello AI.

   Priorità:
     1. qwen2.5-coder  (raccomandato dall'utente)
     2. qualsiasi modello con "coder" nel nome
     3. codellama, starcoder, codegemma
     4. deepseek-coder
     5. nessun cambiamento (lascia il modello corrente)
   ══════════════════════════════════════════════════════════════ */
void ProgrammazionePage::selectCoderModel()
{
    if (!m_modelCombo || m_modelCombo->count() == 0) return;

    /* Priorità decrescente — primo match vince */
    const QStringList priorities = {
        "qwen2.5-coder",    /* raccomandato */
        "coder",            /* qualsiasi coder */
        "codellama",
        "starcoder",
        "codegemma",
        "deepseek-coder",
    };

    for (const QString& prio : priorities) {
        for (int i = 0; i < m_modelCombo->count(); ++i) {
            const QString data = m_modelCombo->itemData(i).toString().toLower();
            if (data.contains(prio)) {
                m_modelCombo->setCurrentIndex(i);
                return;
            }
        }
    }
    /* Nessun modello coder trovato — mantieni il corrente */
}

/* ══════════════════════════════════════════════════════════════
   triggerFix — apre il pannello AI, seleziona il coder model,
   compone la richiesta di fix e la invia automaticamente.

   includeError = true  → aggiunge l'output di errore al contesto
   includeError = false → chiede semplicemente di revisionare il codice
   ══════════════════════════════════════════════════════════════ */
void ProgrammazionePage::triggerFix(bool includeError)
{
    const QString codice = m_editor->toPlainText().trimmed();
    if (codice.isEmpty()) {
        m_status->setText("\xe2\x9d\x8c  Nessun codice nell'editor da correggere.");
        return;
    }
    if (!m_ai) {
        m_status->setText("\xe2\x9d\x8c  AI non disponibile.");
        return;
    }
    if (m_ai->busy()) {
        m_status->setText("\xe2\x9a\xa0\xef\xb8\x8f  AI occupata. Attendi o premi Stop.");
        return;
    }

    const QString lang = m_lang->currentText();
    const QString ext  = m_lang->currentData().toString();

    /* Apri il pannello AI se chiuso */
    if (!m_btnAi->isChecked()) {
        m_btnAi->setChecked(true);
        /* Se i modelli non sono ancora caricati aspettiamo che siano pronti
           prima di selezionare il coder — one-shot via slot nominato. */
        if (m_modelCombo->count() <= 1) {
            m_pendingFixIncludeError = includeError;
            m_pendingFixCodice = codice;
            m_pendingFixLang   = lang;
            m_pendingFixExt    = ext;
            disconnect(m_modelsReadyForFixConn);
            m_modelsReadyForFixConn = connect(
                m_ai, &AiClient::modelsReady,
                this, &ProgrammazionePage::onModelsReadyForFix);
            return;
        }
    }

    selectCoderModel();
    _doFix(includeError, codice, lang, ext);
}

/* ── Parte interna di triggerFix: compone prompt e lancia chat ── */
void ProgrammazionePage::_doFix(bool includeError,
                                 const QString& codice,
                                 const QString& lang,
                                 const QString& ext)
{
    if (m_fixErrPanel) m_fixErrPanel->hide();

    /* Salva il modello attivo PRIMA di cambiarlo — verrà ripristinato al termine */
    m_fixOriginalModel = m_ai->model();

    /* Applica il modello coder scelto nella combo */
    if (m_modelCombo) {
        const QString sel = m_modelCombo->currentData().toString();
        if (!sel.isEmpty() && sel != m_ai->model())
            m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), sel);
    }

    /* Guardia embedding — controlla DOPO aver applicato la selezione combo */
    {
        const QString mn = m_ai->model().toLower();
        const bool isEmbed = mn.contains("embed") || mn.contains("minilm") ||
                             mn.contains("rerank") || mn.contains("bge-") ||
                             mn.contains("e5-") || mn.contains("gte-") ||
                             mn.contains("-embed");
        if (isEmbed) {
            m_aiOutput->clear();
            m_aiOutput->insertPlainText(
                QString("\xe2\x9a\xa0\xef\xb8\x8f  \"%1\" \xc3\xa8 un modello di embedding:\n"
                        "non supporta la chat.\n\n"
                        "Seleziona un modello diverso dalla combo qui sopra\n"
                        "(es. qwen2.5-coder, llama3, deepseek-r1...).")
                .arg(m_ai->model()));
            /* Ripristina il modello originale */
            if (!m_fixOriginalModel.isEmpty() && m_fixOriginalModel != m_ai->model())
                m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), m_fixOriginalModel);
            return;
        }
    }

    const QString sys = QString(
        "Sei un esperto programmatore specializzato in debug e code review. "
        "L'utente ti mostra codice %1 che ha uno o pi\xc3\xb9 problemi. "
        "Il tuo compito:\n"
        "1. Identifica TUTTI i bug, errori logici e problemi di stile.\n"
        "2. Scrivi il codice corretto completo in un blocco ```%2 ... ```.\n"
        "3. Sotto il blocco, spiega BREVEMENTE ogni correzione con un elenco puntato.\n"
        "I commenti nel codice devono essere in italiano. "
        "Rispondi SEMPRE e SOLO in italiano.")
        .arg(lang, ext);

    QString user;
    if (includeError && !m_lastError.isEmpty()) {
        user = QString(
            "Codice con errore:\n```%1\n%2\n```\n\n"
            "Output di errore:\n```\n%3\n```\n\n"
            "Trova e correggi tutti i bug. Mostra il codice corretto completo.")
            .arg(ext, codice, m_lastError);
    } else {
        user = QString(
            "Codice da revisionare:\n```%1\n%2\n```\n\n"
            "Trova e correggi tutti i bug, gli errori logici e i problemi di qualit\xc3\xa0. "
            "Mostra il codice corretto completo.")
            .arg(ext, codice);
    }

    m_aiOutput->clear();
    /* Intestazione modello — visibile subito, prima che arrivi il primo token */
    {
        const QString modelName = m_ai->model().isEmpty() ? "AI" : m_ai->model();
        m_aiOutput->insertPlainText(
            QString("\xf0\x9f\xa4\x96  Modello: %1\n%2\n\n")
                .arg(modelName, QString(modelName.length() + 12, '-')));
    }
    m_aiMode = true;
    m_btnInsert->setEnabled(false);
    setRunning(true);
    m_status->setText(QString("\xf0\x9f\x94\xa7  %1 sta analizzando il codice...")
                      .arg(m_ai->model().isEmpty() ? "AI" : m_ai->model()));


    disconnect(m_aiTokenConn);
    disconnect(m_aiFinishedConn);
    disconnect(m_aiErrorConn);
    m_aiTokenConn    = connect(m_ai, &AiClient::token,    this, &ProgrammazionePage::onFixToken);
    m_aiFinishedConn = connect(m_ai, &AiClient::finished, this, &ProgrammazionePage::onFixFinished);
    m_aiErrorConn    = connect(m_ai, &AiClient::error,    this, &ProgrammazionePage::onFixError);
    m_ai->chat(P::prependKnowledge(sys), user);
}

/* ══════════════════════════════════════════════════════════════
   buildAgentica — sub-tab "🤖 Agentica"
   ══════════════════════════════════════════════════════════════ */
QWidget* ProgrammazionePage::buildAgentica(QWidget* parent)
{
    auto* w   = new QWidget(parent);
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(12, 12, 12, 12);
    lay->setSpacing(10);

    buildAgenticaHeader(lay, w);
    lay->addWidget(buildAgenticaToolbar(w));
    lay->addWidget(buildAgenticaTaskGroup(w));
    lay->addWidget(buildAgenticaModelRow(w));
    lay->addWidget(buildAgenticaOutputGroup(w), 1);
    setupAgenticaConnections(nullptr, nullptr);
    return w;
}

/* ── Header: descrizione + separatore ── */
void ProgrammazionePage::buildAgenticaHeader(QVBoxLayout* lay, QWidget* w)
{
    auto* desc = new QLabel(
        "\xf0\x9f\xa4\x96  <b>Programmazione Agentica</b> \xe2\x80\x94 "
        "Genera sistemi AI multi-step: pipeline, RAG, tool-use e agenti autonomi.", w);
    desc->setObjectName("hintLabel");
    desc->setWordWrap(true);
    lay->addWidget(desc);

    auto* sep = new QFrame(w);
    sep->setFrameShape(QFrame::HLine);
    sep->setObjectName("sidebarSep");
    lay->addWidget(sep);
}

/* ── Toolbar: tipo agente + linguaggio + pulsanti ── */
QWidget* ProgrammazionePage::buildAgenticaToolbar(QWidget* parent)
{
    auto* toolRow = new QWidget(parent);
    auto* toolLay = new QHBoxLayout(toolRow);
    toolLay->setContentsMargins(0, 0, 0, 0);
    toolLay->setSpacing(8);

    toolLay->addWidget(new QLabel("Tipo agente:", toolRow));
    m_agentType = new QComboBox(toolRow);
    m_agentType->setObjectName("settingCombo");
    m_agentType->addItem(
        "Pipeline codice (3 agenti: Analisi \xe2\x86\x92 Impl \xe2\x86\x92 Test)",
        QString("pipeline"));
    m_agentType->addItem("RAG + Codice (ricerca documenti + generazione)",
                         QString("rag"));
    m_agentType->addItem("Refactoring AI (pattern + pulizia)",
                         QString("refactor"));
    m_agentType->addItem("Generatore test unitari",
                         QString("testgen"));
    m_agentType->addItem("Debugging agentico (analisi + fix + verifica)",
                         QString("debug"));
    m_agentType->addItem(
        "Motore Byzantino \xe2\x80\x94 anti-allucinazione (A"
        "\xe2\x86\x92" "B" "\xe2\x86\x92" "C" "\xe2\x86\x92" "D)",
        QString("byzantine"));
    m_agentType->setMinimumWidth(300);
    toolLay->addWidget(m_agentType);

    toolLay->addSpacing(12);
    toolLay->addWidget(new QLabel("Linguaggio:", toolRow));
    m_agentLang = new QComboBox(toolRow);
    m_agentLang->setObjectName("settingCombo");
    m_agentLang->addItems({"Python", "C", "C++", "JavaScript", "Bash"});
    m_agentLang->setFixedWidth(110);
    toolLay->addWidget(m_agentLang);
    toolLay->addStretch(1);

    m_btnAgentRun = new QPushButton("\xe2\x96\xb6  Genera", toolRow);
    m_btnAgentRun->setObjectName("actionBtn");
    m_btnAgentRun->setProperty("highlight", "true");
    m_btnAgentRun->setToolTip("Genera il codice agente (F5)");
    toolLay->addWidget(m_btnAgentRun);

    m_btnAgentStop = new QPushButton("\xe2\x96\xa0  Stop", toolRow);
    m_btnAgentStop->setObjectName("actionBtn");
    m_btnAgentStop->setProperty("danger", "true");
    m_btnAgentStop->setEnabled(false);
    m_btnAgentStop->setToolTip("Interrompi la generazione");
    toolLay->addWidget(m_btnAgentStop);

    return toolRow;
}

/* ── Task group: campo testo descrizione ── */
QWidget* ProgrammazionePage::buildAgenticaTaskGroup(QWidget* parent)
{
    QFont monoFont;
    monoFont.setFamily("JetBrains Mono");
    monoFont.setStyleHint(QFont::Monospace);
    monoFont.setPointSize(monoFontPt(11));

    auto* taskGroup = new QGroupBox(
        "\xf0\x9f\x93\x9d  Descrizione del task (cosa vuoi costruire?)", parent);
    taskGroup->setObjectName("cardGroup");
    auto* taskLay = new QVBoxLayout(taskGroup);
    taskLay->setContentsMargins(4, 8, 4, 4);

    m_agentTask = new QPlainTextEdit(taskGroup);
    m_agentTask->setObjectName("agentTaskEditor");
    m_agentTask->setFont(monoFont);
    m_agentTask->setMaximumHeight(140);
    m_agentTask->setPlaceholderText(
        "Descrivi il sistema che vuoi costruire...\n\n"
        "Esempi:\n"
        "  \xe2\x80\xa2 \"Sistema RAG in Python che indicizza PDF e risponde a domande con citazioni\"\n"
        "  \xe2\x80\xa2 \"Pipeline 3 agenti: Analista \xe2\x86\x92 Programmatore \xe2\x86\x92 Tester per un parser CSV\"\n"
        "  \xe2\x80\xa2 \"Refactoring con pattern Strategy di questo codice C++\"\n"
        "  \xe2\x80\xa2 \"Test unitari completi (pytest) per una classe BankAccount\"");
    taskLay->addWidget(m_agentTask);
    return taskGroup;
}

/* ── Riga selezione modello ── */
QWidget* ProgrammazionePage::buildAgenticaModelRow(QWidget* parent)
{
    auto* modelRow = new QWidget(parent);
    auto* modelLay = new QHBoxLayout(modelRow);
    modelLay->setContentsMargins(0, 0, 0, 0);
    modelLay->setSpacing(8);

    auto* lblModel = new QLabel("\xf0\x9f\xa4\x96  Modello AI:", modelRow);
    lblModel->setObjectName("cardDesc");
    modelLay->addWidget(lblModel);

    m_agentModel = new QComboBox(modelRow);
    m_agentModel->setObjectName("settingCombo");
    m_agentModel->addItem(
        m_ai ? (m_ai->model().isEmpty() ? "(nessun modello)" : m_ai->model())
             : "(AI non disponibile)",
        m_ai ? m_ai->model() : QString());
    modelLay->addWidget(m_agentModel, 1);

    auto* btnRefAgent = new QPushButton("\xf0\x9f\x94\x84", modelRow);
    btnRefAgent->setObjectName("actionBtn");
    btnRefAgent->setFixedWidth(32);
    btnRefAgent->setToolTip("Aggiorna lista modelli disponibili");
    modelLay->addWidget(btnRefAgent);

    connect(btnRefAgent, &QPushButton::clicked,
            this, &ProgrammazionePage::populateAgentModels);
    QTimer::singleShot(0, this, &ProgrammazionePage::populateAgentModels);

    return modelRow;
}

/* ── Output group: streaming + pulsanti ── */
QWidget* ProgrammazionePage::buildAgenticaOutputGroup(QWidget* parent)
{
    QFont monoFont;
    monoFont.setFamily("JetBrains Mono");
    monoFont.setStyleHint(QFont::Monospace);
    monoFont.setPointSize(monoFontPt(11));

    auto* outGroup = new QGroupBox(
        "\xf0\x9f\xa4\x96  Output agente (streaming)", parent);
    outGroup->setObjectName("cardGroup");
    auto* outLay = new QVBoxLayout(outGroup);
    outLay->setContentsMargins(4, 8, 4, 4);
    outLay->setSpacing(6);

    m_agentOutput = new QTextEdit(outGroup);
    m_agentOutput->setObjectName("chatLog");
    m_agentOutput->setReadOnly(true);
    m_agentOutput->setFont(monoFont);
    m_agentOutput->setPlaceholderText(
        "L'output dell'agente appari\xc3\xa0 qui in streaming...\n\n"
        "Scrivi il task sopra e premi \xe2\x96\xb6 Genera.");
    outLay->addWidget(m_agentOutput, 1);

    auto* outBtnRow = new QWidget(outGroup);
    auto* outBtnLay = new QHBoxLayout(outBtnRow);
    outBtnLay->setContentsMargins(0, 2, 0, 0);
    outBtnLay->setSpacing(8);

    m_btnAgentInsert = new QPushButton(
        "\xe2\x86\x91  Apri in editor Programmazione", outBtnRow);
    m_btnAgentInsert->setObjectName("actionBtn");
    m_btnAgentInsert->setEnabled(false);
    m_btnAgentInsert->setToolTip(
        "Estrae il primo blocco codice e lo apre nel tab \xf0\x9f\x92\xbb Programmazione");
    outBtnLay->addWidget(m_btnAgentInsert);

    auto* btnClearAgent = new QPushButton(
        "\xf0\x9f\x97\x91  Pulisci output", outBtnRow);
    btnClearAgent->setObjectName("actionBtn");
    outBtnLay->addWidget(btnClearAgent);
    outBtnLay->addStretch(1);

    outLay->addWidget(outBtnRow);

    connect(btnClearAgent, &QPushButton::clicked,
            this, &ProgrammazionePage::onBtnClearAgentClicked);
    connect(m_btnAgentInsert, &QPushButton::clicked,
            this, &ProgrammazionePage::onBtnAgentInsertClicked);

    return outGroup;
}

/* ── Connessioni Agentica ── */
void ProgrammazionePage::setupAgenticaConnections(QPushButton* /*btnRefAgent*/,
                                                   QPushButton* /*btnClearAgent*/)
{
    /* Model refresh and model population already wired in buildAgenticaModelRow. */
    /* Output button connections already wired in buildAgenticaOutputGroup. */
    connect(m_btnAgentRun,  &QPushButton::clicked,
            this, &ProgrammazionePage::runAgente);
    connect(m_btnAgentStop, &QPushButton::clicked,
            this, &ProgrammazionePage::onBtnAgentStopClicked);
}

/* ══════════════════════════════════════════════════════════════
   runAgente — esegue la generazione agentica AI
   Costruisce il system prompt in base al tipo di agente scelto
   e lancia la chiamata streaming a m_ai->chat().
   ══════════════════════════════════════════════════════════════ */
void ProgrammazionePage::runAgente()
{
    if (!m_ai || !m_agentTask) return;

    const QString task = m_agentTask->toPlainText().trimmed();
    if (task.isEmpty()) {
        m_agentOutput->setPlainText(
            "\xe2\x9d\x8c  Scrivi una descrizione del task prima di premere Genera.");
        return;
    }
    if (m_ai->busy()) {
        m_agentOutput->setPlainText(
            "\xe2\x9a\xa0\xef\xb8\x8f  AI occupata. Attendi o premi Stop.");
        return;
    }

    const QString tipo = m_agentType ? m_agentType->currentData().toString() : "pipeline";
    const QString lang = m_agentLang ? m_agentLang->currentText() : "Python";

    /* Applica il modello scelto nel tab Agentica */
    if (m_agentModel) {
        const QString sel = m_agentModel->currentData().toString();
        if (!sel.isEmpty() && sel != m_ai->model())
            m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), sel);
    }

    /* ── Costruzione system prompt in base al tipo agente ── */
    QString sys;
    QString user;

    if (tipo == "pipeline") {
        sys = QString(
            "Sei un architetto software esperto in sistemi multi-agente. "
            "Il tuo compito \xc3\xa8 progettare e implementare una pipeline a 3 agenti in %1:\n"
            "  Agente 1 (Analista): analizza il problema, estrae requisiti\n"
            "  Agente 2 (Implementatore): scrive il codice completo\n"
            "  Agente 3 (Tester): genera test unitari per il codice\n\n"
            "Per ogni agente:\n"
            "1. Mostra il prompt/istruzione che riceve\n"
            "2. Mostra il codice che produce in un blocco ```%2 ... ```\n"
            "3. Breve spiegazione\n\n"
            "Usa commenti in italiano. Rispondi SEMPRE in italiano.").arg(lang, lang.toLower());
        user = QString("Task: %1").arg(task);

    } else if (tipo == "rag") {
        sys = QString(
            "Sei un esperto di sistemi RAG (Retrieval-Augmented Generation) in %1. "
            "Implementa un sistema RAG completo che:\n"
            "  1. Indicizza documenti (chunking + embedding)\n"
            "  2. Cerca i chunk rilevanti per una query (similarit\xc3\xa0 coseno)\n"
            "  3. Costruisce il prompt con i chunk trovati\n"
            "  4. Chiama il LLM e restituisce la risposta con citazioni\n\n"
            "Usa llama.cpp / Ollama HTTP API per l'inferenza locale. "
            "Struttura il codice in classi/funzioni chiare. "
            "Includi un blocco ```%2 ... ``` con il codice completo. "
            "Commenta in italiano. Rispondi SEMPRE in italiano.").arg(lang, lang.toLower());
        user = QString("Costruisci: %1").arg(task);

    } else if (tipo == "refactor") {
        sys = QString(
            "Sei un esperto di refactoring e design pattern in %1. "
            "Analizza il codice richiesto e:\n"
            "  1. Identifica i problemi (smell, violazioni SOLID, duplicazioni)\n"
            "  2. Proponi il design pattern pi\xc3\xb9 adatto\n"
            "  3. Scrivi il codice refactored completo in ```%2 ... ```\n"
            "  4. Lista le modifiche applicate (puntato)\n\n"
            "Commenta le decisioni architetturali. Rispondi SEMPRE in italiano.").arg(lang, lang.toLower());
        user = QString("Refactoring richiesto: %1").arg(task);

    } else if (tipo == "testgen") {
        sys = QString(
            "Sei un esperto di testing e TDD in %1. "
            "Genera una suite di test unitari completa che:\n"
            "  1. Copre tutti i casi normali (happy path)\n"
            "  2. Copra i casi limite (edge cases: valori vuoti, None/null, overflow)\n"
            "  3. Includa test negativi (input invalidi, eccezioni attese)\n"
            "  4. Usi il framework pi\xc3\xb9 appropriato (pytest per Python, etc.)\n\n"
            "Mostra i test in un blocco ```%2 ... ```. "
            "Aggiungi commenti esplicativi in italiano. Rispondi SEMPRE in italiano.").arg(lang, lang.toLower());
        user = QString("Genera test per: %1").arg(task);

    } else if (tipo == "debug") {
        sys = QString(
            "Sei un esperto debugger che usa un approccio a 3 step in %1:\n"
            "  Step 1 (Analisi): identifica tutti i potenziali bug e le cause radice\n"
            "  Step 2 (Fix): scrivi il codice corretto in ```%2 ... ```\n"
            "  Step 3 (Verifica): spiega come verificare che i bug siano risolti\n\n"
            "Per ogni bug trovato: descrivi il problema, la causa, e il fix applicato. "
            "Rispondi SEMPRE in italiano.").arg(lang, lang.toLower());
        user = QString("Analizza e correggi: %1").arg(task);

    } else { /* byzantine */
        sys = QString(
            "Sei un sistema a 4 agenti anti-allucinazione (Motore Byzantino) per codice %1:\n\n"
            "  Agente A (Originale): genera la soluzione\n"
            "  Agente B (Avvocato del Diavolo): trova attivamente errori in A\n"
            "  Agente C (Gemello indipendente): risolve lo stesso problema da zero\n"
            "  Agente D (Giudice): confronta A e C, valuta le critiche di B, emette la soluzione finale\n\n"
            "Per ogni agente mostra il suo ragionamento e l'output. "
            "La soluzione finale di D deve essere in un blocco ```%2 ... ```. "
            "Commenta in italiano. Rispondi SEMPRE in italiano.").arg(lang, lang.toLower());
        user = QString("Task da risolvere con Motore Byzantino: %1").arg(task);
    }

    /* ── Avvio streaming ── */
    m_agentOutput->clear();
    {
        const QString modelName = m_ai->model().isEmpty() ? "AI" : m_ai->model();
        const QString tipoLabel = m_agentType
            ? m_agentType->currentText()
            : tipo;
        m_agentOutput->setPlainText(
            QString("\xf0\x9f\xa4\x96  Modello: %1\n"
                    "\xf0\x9f\x94\xa7  Tipo: %2\n"
                    "\xf0\x9f\x92\xbb  Linguaggio: %3\n%4\n\n")
            .arg(modelName, tipoLabel, lang,
                 QString(qMax(modelName.length(), 20), '-')));
    }
    m_btnAgentRun->setEnabled(false);
    m_btnAgentStop->setEnabled(true);
    m_btnAgentInsert->setEnabled(false);

    disconnect(m_agentTokenConn);
    disconnect(m_agentFinishedConn);
    disconnect(m_agentErrorConn);
    m_agentTokenConn    = connect(m_ai, &AiClient::token,    this, &ProgrammazionePage::onAgentToken);
    m_agentFinishedConn = connect(m_ai, &AiClient::finished, this, &ProgrammazionePage::onAgentFinished);
    m_agentErrorConn    = connect(m_ai, &AiClient::error,    this, &ProgrammazionePage::onAgentError);
    m_ai->chat(P::prependKnowledge(sys), user);
}

/* ══════════════════════════════════════════════════════════════
   buildReverseEngineering — sub-tab "🔍 Reverse Eng."
   ══════════════════════════════════════════════════════════════ */
QWidget* ProgrammazionePage::buildReverseEngineering(QWidget* parent)
{
    auto* w   = new QWidget(parent);
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(12, 12, 12, 12);
    lay->setSpacing(8);

    buildRevHeader(lay, w);
    lay->addWidget(buildRevFileRow(w));  /* btnLoad wired inside */

    QPushButton* btnRefRev   = nullptr;
    QPushButton* btnClearRev = nullptr;

    lay->addWidget(buildRevOptionsRow(w, btnRefRev));
    lay->addWidget(buildRevPreviewGroup(w));
    lay->addWidget(buildRevOutputGroup(w, btnClearRev), 1);
    setupRevConnections(nullptr, btnRefRev, btnClearRev);
    return w;
}

/* ── Header: descrizione + separatore ── */
void ProgrammazionePage::buildRevHeader(QVBoxLayout* lay, QWidget* w)
{
    auto* desc = new QLabel(
        "\xf0\x9f\x94\x8d  <b>Reverse Engineering</b> \xe2\x80\x94 "
        "Carica un file compilato o offuscato: l'AI analizza i byte, "
        "estrae le stringhe leggibili e ricostruisce il codice sorgente approssimativo.", w);
    desc->setObjectName("hintLabel");
    desc->setWordWrap(true);
    lay->addWidget(desc);

    auto* sep = new QFrame(w);
    sep->setFrameShape(QFrame::HLine);
    sep->setObjectName("sidebarSep");
    lay->addWidget(sep);
}

/* ── Riga caricamento file ── */
QWidget* ProgrammazionePage::buildRevFileRow(QWidget* parent)
{
    auto* fileRow = new QWidget(parent);
    auto* fileLay = new QHBoxLayout(fileRow);
    fileLay->setContentsMargins(0, 0, 0, 0);
    fileLay->setSpacing(8);

    auto* btnLoad = new QPushButton("\xf0\x9f\x93\x82  Carica file...", fileRow);
    btnLoad->setObjectName("actionBtn");
    fileLay->addWidget(btnLoad);

    m_revFilePath = new QLabel("(nessun file caricato)", fileRow);
    m_revFilePath->setObjectName("hintLabel");
    fileLay->addWidget(m_revFilePath, 1);

    connect(btnLoad, &QPushButton::clicked,
            this, &ProgrammazionePage::onBtnRevLoadClicked);

    return fileRow;
}

/* ── Riga opzioni + modello AI ── */
QWidget* ProgrammazionePage::buildRevOptionsRow(QWidget* parent,
                                                  QPushButton*& outBtnRefRev)
{
    /* Opzioni superiori */
    auto* optRow = new QWidget(parent);
    auto* optLay = new QHBoxLayout(optRow);
    optLay->setContentsMargins(0, 0, 0, 0);
    optLay->setSpacing(8);

    optLay->addWidget(new QLabel("Linguaggio target:", optRow));
    m_revTargetLang = new QComboBox(optRow);
    m_revTargetLang->setObjectName("settingCombo");
    m_revTargetLang->addItems({"Auto-rileva", "C", "C++", "Python",
                                "Assembly x86", "Java", "Rust"});
    m_revTargetLang->setFixedWidth(140);
    optLay->addWidget(m_revTargetLang);

    optLay->addSpacing(8);
    optLay->addWidget(new QLabel("Dettaglio:", optRow));
    m_revDetail = new QComboBox(optRow);
    m_revDetail->setObjectName("settingCombo");
    m_revDetail->addItem("Struttura (rapido)",  QString("fast"));
    m_revDetail->addItem("Completo (lento)",    QString("full"));
    m_revDetail->addItem("Con commenti estesi", QString("commented"));
    m_revDetail->setFixedWidth(180);
    optLay->addWidget(m_revDetail);

    optLay->addStretch(1);

    m_btnRevAnalyze = new QPushButton(
        "\xf0\x9f\x94\x8d  Analizza e Ricostruisci", optRow);
    m_btnRevAnalyze->setObjectName("actionBtn");
    m_btnRevAnalyze->setProperty("highlight", "true");
    m_btnRevAnalyze->setEnabled(false);
    m_btnRevAnalyze->setToolTip("Invia il file all'AI per la ricostruzione del sorgente");
    optLay->addWidget(m_btnRevAnalyze);

    m_btnRevStop = new QPushButton("\xe2\x96\xa0  Stop", optRow);
    m_btnRevStop->setObjectName("actionBtn");
    m_btnRevStop->setProperty("danger", "true");
    m_btnRevStop->setEnabled(false);
    optLay->addWidget(m_btnRevStop);

    /* Selezione modello — costruita nello stesso widget composito */
    /* Wrap both rows in a container */
    auto* container = new QWidget(parent);
    auto* contLay   = new QVBoxLayout(container);
    contLay->setContentsMargins(0, 0, 0, 0);
    contLay->setSpacing(4);
    contLay->addWidget(optRow);

    auto* modelRow = new QWidget(container);
    auto* modelLay = new QHBoxLayout(modelRow);
    modelLay->setContentsMargins(0, 0, 0, 0);
    modelLay->setSpacing(8);

    auto* lblMod = new QLabel("\xf0\x9f\xa4\x96  Modello AI:", modelRow);
    lblMod->setObjectName("cardDesc");
    modelLay->addWidget(lblMod);

    m_revModel = new QComboBox(modelRow);
    m_revModel->setObjectName("settingCombo");
    m_revModel->addItem(
        m_ai ? (m_ai->model().isEmpty() ? "(nessun modello)" : m_ai->model())
             : "(AI non disponibile)",
        m_ai ? m_ai->model() : QString());
    modelLay->addWidget(m_revModel, 1);

    outBtnRefRev = new QPushButton("\xf0\x9f\x94\x84", modelRow);
    outBtnRefRev->setObjectName("actionBtn");
    outBtnRefRev->setFixedWidth(32);
    outBtnRefRev->setToolTip("Aggiorna lista modelli disponibili");
    modelLay->addWidget(outBtnRefRev);

    contLay->addWidget(modelRow);

    return container;
}

/* ── Preview hex dump ── */
QWidget* ProgrammazionePage::buildRevPreviewGroup(QWidget* parent)
{
    QFont monoFont;
    monoFont.setFamily("JetBrains Mono");
    monoFont.setStyleHint(QFont::Monospace);
    monoFont.setPointSize(monoFontPt(10));

    auto* prevGroup = new QGroupBox(
        "\xf0\x9f\x93\x8b  Anteprima file (hex dump + stringhe estratte)", parent);
    prevGroup->setObjectName("cardGroup");
    auto* prevLay = new QVBoxLayout(prevGroup);
    prevLay->setContentsMargins(4, 8, 4, 4);

    m_revPreview = new QPlainTextEdit(prevGroup);
    m_revPreview->setObjectName("revPreviewEditor");
    m_revPreview->setFont(monoFont);
    m_revPreview->setReadOnly(true);
    m_revPreview->setMaximumHeight(160);
    m_revPreview->setPlaceholderText(
        "Carica un file per vedere il hex dump e le stringhe ASCII estratte...");
    prevLay->addWidget(m_revPreview);

    return prevGroup;
}

/* ── Output AI + pulsanti ── */
QWidget* ProgrammazionePage::buildRevOutputGroup(QWidget* parent,
                                                   QPushButton*& outBtnClearRev)
{
    QFont monoFont;
    monoFont.setFamily("JetBrains Mono");
    monoFont.setStyleHint(QFont::Monospace);
    monoFont.setPointSize(monoFontPt(10));

    auto* outGroup = new QGroupBox(
        "\xf0\x9f\xa4\x96  Codice sorgente ricostruito (streaming AI)", parent);
    outGroup->setObjectName("cardGroup");
    auto* outLay = new QVBoxLayout(outGroup);
    outLay->setContentsMargins(4, 8, 4, 4);
    outLay->setSpacing(6);

    m_revOutput = new QTextEdit(outGroup);
    m_revOutput->setObjectName("chatLog");
    m_revOutput->setReadOnly(true);
    m_revOutput->setFont(monoFont);
    m_revOutput->setPlaceholderText(
        "Il codice ricostruito dall'AI appari\xc3\xa0 qui in streaming...\n\n"
        "Carica un file e premi \xf0\x9f\x94\x8d Analizza e Ricostruisci.");
    outLay->addWidget(m_revOutput, 1);

    auto* revBtnRow = new QWidget(outGroup);
    auto* revBtnLay = new QHBoxLayout(revBtnRow);
    revBtnLay->setContentsMargins(0, 2, 0, 0);
    revBtnLay->setSpacing(8);

    m_btnRevInsert = new QPushButton(
        "\xe2\x86\x91  Apri in editor Programmazione", revBtnRow);
    m_btnRevInsert->setObjectName("actionBtn");
    m_btnRevInsert->setEnabled(false);
    m_btnRevInsert->setToolTip(
        "Estrae il primo blocco codice e lo apre nel tab \xf0\x9f\x92\xbb Programmazione");
    revBtnLay->addWidget(m_btnRevInsert);

    outBtnClearRev = new QPushButton(
        "\xf0\x9f\x97\x91  Pulisci output", revBtnRow);
    outBtnClearRev->setObjectName("actionBtn");
    revBtnLay->addWidget(outBtnClearRev);
    revBtnLay->addStretch(1);

    outLay->addWidget(revBtnRow);
    return outGroup;
}

/* ── Connessioni Reverse Engineering ── */
void ProgrammazionePage::setupRevConnections(QPushButton* /*btnLoad*/,
                                              QPushButton* btnRefRev,
                                              QPushButton* btnClearRev)
{
    /* btnLoad already wired inside buildRevFileRow */
    if (btnRefRev) {
        connect(btnRefRev, &QPushButton::clicked,
                this, &ProgrammazionePage::populateRevModels);
        QTimer::singleShot(0, this, &ProgrammazionePage::populateRevModels);
    }

    connect(m_btnRevAnalyze, &QPushButton::clicked,
            this, &ProgrammazionePage::runReverseEngineering);
    connect(m_btnRevStop,    &QPushButton::clicked,
            this, &ProgrammazionePage::onBtnRevStopClicked);

    if (btnClearRev)
        connect(btnClearRev, &QPushButton::clicked,
                this, &ProgrammazionePage::onBtnClearRevClicked);

    connect(m_btnRevInsert, &QPushButton::clicked,
            this, &ProgrammazionePage::onBtnRevInsertClicked);
}

/* ══════════════════════════════════════════════════════════════
   runReverseEngineering — compone il prompt RE e lancia lo streaming.

   Invia all'LLM: tipo file, hex dump, stringhe estratte, linguaggio
   target e livello di dettaglio. L'AI ricostruisce il sorgente
   approssimativo in un blocco ```lang ... ```.
   ══════════════════════════════════════════════════════════════ */
void ProgrammazionePage::runReverseEngineering()
{
    if (!m_ai || m_revFileData.isEmpty()) return;
    if (m_ai->busy()) {
        m_revOutput->setPlainText(
            "\xe2\x9a\xa0\xef\xb8\x8f  AI occupata. Attendi o premi Stop.");
        return;
    }

    /* Applica modello scelto */
    if (m_revModel) {
        const QString sel = m_revModel->currentData().toString();
        if (!sel.isEmpty() && sel != m_ai->model())
            m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), sel);
    }

    const QString lang     = m_revTargetLang ? m_revTargetLang->currentText() : "C";
    const QString detail   = m_revDetail     ? m_revDetail->currentData().toString() : "full";
    const QString fileInfo = m_revFilePath   ? m_revFilePath->text() : "";
    const QString preview  = m_revPreview    ? m_revPreview->toPlainText() : "";

    /* Estensione fence per il blocco codice */
    const QString fence = (lang == "Auto-rileva")  ? "c"
                        : (lang == "Assembly x86") ? "asm"
                        : lang.toLower().section(' ', 0, 0);

    const QString detailDesc =
        (detail == "fast")
        ? "Ricostruisci solo la struttura: dichiarazioni di funzioni/classi, tipi, "
          "costanti e include, senza implementazione completa."
        : (detail == "full")
        ? "Ricostruisci il codice il pi\xc3\xb9 completo possibile, "
          "implementando ogni funzione con la logica dedotta."
        : "Ricostruisci il codice completo con commenti estesi che spiegano "
          "la logica di ogni sezione e le deduzioni RE.";

    const QString langTarget = (lang == "Auto-rileva")
        ? "il linguaggio pi\xc3\xb9 probabile (deducilo dal tipo di file e dai byte)"
        : lang;

    const QString sys = QString(
        "Sei un esperto di reverse engineering e analisi binaria. "
        "Ti viene fornita l'analisi di un file: tipo rilevato, hex dump dei primi byte "
        "e le stringhe ASCII estratte. "
        "Il tuo compito \xc3\xa8 ricostruire il codice sorgente originale approssimativo in %1.\n\n"
        "Metodologia:\n"
        "  1. Identifica architettura, OS target e librerie dai magic byte e dalle stringhe\n"
        "  2. Deduci le funzionalit\xc3\xa0 del programma dalle stringhe leggibili\n"
        "  3. %2\n"
        "  4. Documenta le ipotesi con commenti NOTE RE:\n\n"
        "Struttura la risposta:\n"
        "  [ANALISI] tipo file, architettura, librerie rilevate, funzionalit\xc3\xa0 dedotte\n"
        "  [SORGENTE RICOSTRUITO] codice in un blocco ```%3 ... ```\n"
        "  [NOTE RE] assunzioni, confidenza, punti incerti\n\n"
        "Il codice deve essere plausibile e sintatticamente corretto. "
        "Rispondi SEMPRE in italiano."
    ).arg(langTarget, detailDesc, fence);

    const QString user = QString(
        "File da analizzare:\n%1\n\n"
        "Dati estratti:\n```\n%2\n```\n\n"
        "Ricostruisci il codice sorgente in %3."
    ).arg(fileInfo, preview, langTarget);

    /* ── Avvio streaming ── */
    m_revOutput->clear();
    {
        const QString modelName = m_ai->model().isEmpty() ? "AI" : m_ai->model();
        m_revOutput->setPlainText(
            QString("\xf0\x9f\xa4\x96  Modello: %1\n"
                    "\xf0\x9f\x94\x8d  Linguaggio: %2\n"
                    "\xf0\x9f\x93\x8b  Dettaglio: %3\n%4\n\n")
            .arg(modelName, lang,
                 m_revDetail ? m_revDetail->currentText() : detail,
                 QString(qMax(modelName.length(), 24), '-')));
    }

    m_btnRevAnalyze->setEnabled(false);
    m_btnRevStop->setEnabled(true);
    m_btnRevInsert->setEnabled(false);

    disconnect(m_revTokenConn);
    disconnect(m_revFinishedConn);
    disconnect(m_revErrorConn);
    m_revTokenConn    = connect(m_ai, &AiClient::token,    this, &ProgrammazionePage::onRevToken);
    m_revFinishedConn = connect(m_ai, &AiClient::finished, this, &ProgrammazionePage::onRevFinished);
    m_revErrorConn    = connect(m_ai, &AiClient::error,    this, &ProgrammazionePage::onRevError);
    m_ai->chat(P::prependKnowledge(sys), user);
}

/* ══════════════════════════════════════════════════════════════
   Network Analyzer — tshark/tcpdump wrapper + AI analysis
   ══════════════════════════════════════════════════════════════ */
QWidget* ProgrammazionePage::buildNetworkAnalyzer(QWidget* parent)
{
    /* Rileva tool prima di buildNetToolbar (serve per il tooltip Fix permessi) */
    m_netTool = QStandardPaths::findExecutable("tshark");
    if (m_netTool.isEmpty()) m_netTool = QStandardPaths::findExecutable("tcpdump");

    auto* w   = new QWidget(parent);
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(6);

    buildNetToolbar(lay, w);
    lay->addWidget(buildNetLogSplitter(w), 1);

    /* Stato tool */
    if (m_netTool.isEmpty()) {
        m_netStatus->setText(
            "\xe2\x9d\x8c  Nessun tool trovato. Installa tshark: sudo apt install tshark");
        m_btnNetStart->setEnabled(false);
    } else {
        m_netStatus->setText(
            QString("\xe2\x9c\x85  Tool rilevato: %1").arg(m_netTool));
    }

    setupNetConnections();
    return w;
}

/* ── Toolbar: interfacce, protocollo, porta, max, pulsanti, status ── */
void ProgrammazionePage::buildNetToolbar(QVBoxLayout* lay, QWidget* w)
{
    auto* toolRow = new QHBoxLayout;
    toolRow->setSpacing(6);

    toolRow->addWidget(new QLabel("\xf0\x9f\x93\xa1  Interfaccia:", w));
    m_netIface = new QComboBox(w);
    m_netIface->setMinimumWidth(110);
    for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
        if (iface.flags().testFlag(QNetworkInterface::IsLoopBack)) continue;
        const QString n = iface.name();
        m_netIface->addItem(AiUtils::ifaceEmoji(n) + n, n);
    }
    if (m_netIface->count() == 0) m_netIface->addItem("\xf0\x9f\x8c\x90  any", "any");
    toolRow->addWidget(m_netIface);

    toolRow->addWidget(new QLabel("Protocollo:", w));
    m_netProto = new QComboBox(w);
    m_netProto->addItem("Tutti",  QString(""));
    m_netProto->addItem("TCP",    QString("tcp"));
    m_netProto->addItem("UDP",    QString("udp"));
    m_netProto->addItem("ICMP",   QString("icmp"));
    m_netProto->addItem("ARP",    QString("arp"));
    m_netProto->addItem("DNS",    QString("udp port 53"));
    m_netProto->addItem("HTTP",   QString("tcp port 80"));
    m_netProto->addItem("HTTPS",  QString("tcp port 443"));
    m_netProto->setMinimumWidth(90);
    toolRow->addWidget(m_netProto);

    toolRow->addWidget(new QLabel("Porta:", w));
    m_netPort = new QSpinBox(w);
    m_netPort->setRange(0, 65535);
    m_netPort->setValue(0);
    m_netPort->setSpecialValueText("Tutte");
    m_netPort->setFixedWidth(80);
    m_netPort->setToolTip("0 = tutte le porte");
    toolRow->addWidget(m_netPort);

    toolRow->addWidget(new QLabel("Max:", w));
    m_netMaxPkts = new QSpinBox(w);
    m_netMaxPkts->setRange(10, 5000);
    m_netMaxPkts->setValue(200);
    m_netMaxPkts->setFixedWidth(70);
    toolRow->addWidget(m_netMaxPkts);

    m_btnNetStart    = new QPushButton("\xe2\x96\xb6  Start",          w);
    m_btnNetStop     = new QPushButton("\xe2\x96\xa0  Stop",           w);
    m_btnNetClear    = new QPushButton("\xf0\x9f\x97\x91  Clear",      w);
    m_btnNetAnalyze  = new QPushButton("\xf0\x9f\xa4\x96  Analisi AI", w);
    m_btnNetFixPerms = new QPushButton("\xf0\x9f\x94\x91  Fix permessi", w);
    m_btnNetStop->setEnabled(false);
    m_btnNetAnalyze->setEnabled(false);
    m_btnNetFixPerms->setToolTip(
        "Applica CAP_NET_RAW al tool di cattura (richiede password amministratore).\n"
        "In alternativa: sudo setcap cap_net_raw+eip " + m_netTool);
    m_btnNetFixPerms->setVisible(
        !QStandardPaths::findExecutable("pkexec").isEmpty());
    toolRow->addWidget(m_btnNetStart);
    toolRow->addWidget(m_btnNetStop);
    toolRow->addWidget(m_btnNetClear);
    toolRow->addWidget(m_btnNetAnalyze);
    toolRow->addWidget(m_btnNetFixPerms);
    lay->addLayout(toolRow);

    m_netStatus = new QLabel(w);
    m_netStatus->setObjectName("statusLabel");
    lay->addWidget(m_netStatus);
}

/* ── Splitter log pacchetti + output AI ── */
QWidget* ProgrammazionePage::buildNetLogSplitter(QWidget* parent)
{
    auto* splitter = new QSplitter(Qt::Vertical, parent);

    m_netLog = new QPlainTextEdit(splitter);
    m_netLog->setReadOnly(true);
    m_netLog->setMaximumBlockCount(5000);
    QFont mono("JetBrains Mono", 9);
    mono.setStyleHint(QFont::Monospace);
    m_netLog->setFont(mono);
    m_netLog->setPlaceholderText("I pacchetti catturati appariranno qui...");
    splitter->addWidget(m_netLog);

    m_netAiOutput = new QTextEdit(splitter);
    m_netAiOutput->setReadOnly(true);
    m_netAiOutput->setPlaceholderText(
        "\xf0\x9f\xa4\x96  L'analisi AI apparira' qui dopo aver cliccato 'Analisi AI'...");
    splitter->addWidget(m_netAiOutput);
    splitter->setSizes({300, 150});
    return splitter;
}

/* ── Connessioni Network Analyzer ── */
void ProgrammazionePage::setupNetConnections()
{
    connect(m_btnNetStart,    &QPushButton::clicked, this, &ProgrammazionePage::netStart);
    connect(m_btnNetStop,     &QPushButton::clicked, this, &ProgrammazionePage::netStop);
    connect(m_btnNetClear,    &QPushButton::clicked, this, &ProgrammazionePage::onBtnNetClearClicked);
    connect(m_btnNetAnalyze,  &QPushButton::clicked, this, &ProgrammazionePage::netAiAnalyze);
    connect(m_btnNetFixPerms, &QPushButton::clicked, this, &ProgrammazionePage::netFixPermissions);
}

void ProgrammazionePage::netStart()
{
    if (m_netProc && m_netProc->state() != QProcess::NotRunning) return;

    const QString iface  = m_netIface->currentData().toString();
    const int     maxPkt = m_netMaxPkts->value();

    /* Costruisce il filtro BPF dai campi strutturati */
    QString filter = m_netProto->currentData().toString(); // es. "tcp", "udp port 53", ""
    const int port = m_netPort->value();
    if (port > 0) {
        /* Se il filtro contiene già "port" (DNS/HTTP/HTTPS), ignora il campo porta */
        if (!filter.contains("port")) {
            const QString proto = filter.isEmpty() ? "" : filter + " ";
            filter = proto + "port " + QString::number(port);
        }
    }

    m_netLog->clear();
    m_netAiOutput->clear();
    m_btnNetAnalyze->setEnabled(false);

    if (!m_netProc) m_netProc = new QProcess(this);

    QStringList args;
    m_netUseTshark = m_netTool.contains("tshark");

    if (m_netUseTshark) {
        args << "-i" << iface
             << "-c" << QString::number(maxPkt)
             << "-T" << "fields"
             << "-e" << "frame.number"
             << "-e" << "ip.src"
             << "-e" << "ip.dst"
             << "-e" << "_ws.col.Protocol"
             << "-e" << "frame.len"
             << "-e" << "_ws.col.Info"
             << "-E" << "separator=|"
             << "-l";
        if (!filter.isEmpty()) args << "-Y" << filter;
    } else {
        args << "-i" << iface
             << "-c" << QString::number(maxPkt)
             << "-l" << "-n" << "-q";
        if (!filter.isEmpty())
            args << filter.split(' ', Qt::SkipEmptyParts);
    }

    connect(m_netProc, &QProcess::readyReadStandardOutput,
            this, &ProgrammazionePage::onNetReadyRead);
    connect(m_netProc, &QProcess::readyReadStandardError,
            this, &ProgrammazionePage::onNetReadyReadStderr);
    connect(m_netProc,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ProgrammazionePage::onNetFinished);
    m_netProc->start(m_netTool, args);
    if (!m_netProc->waitForStarted(2000)) {
        m_netStatus->setText(
            "\xe2\x9d\x8c  Impossibile avviare il tool. Permessi root necessari?");
        return;
    }
    m_btnNetStart->setEnabled(false);
    m_btnNetStop->setEnabled(true);
    const QString filterDesc = filter.isEmpty() ? "tutto il traffico" : filter;
    m_netStatus->setText(
        QString("\xf0\x9f\x94\xb4  Cattura su <b>%1</b> — filtro: <code>%2</code>")
        .arg(iface, filterDesc));
}

void ProgrammazionePage::netStop()
{
    if (!m_netProc) return;
    m_netProc->terminate();
    QTimer::singleShot(1500, this, &ProgrammazionePage::onNetStopTimer);
}

void ProgrammazionePage::netAiAnalyze()
{
    const QString logText = m_netLog->toPlainText().trimmed();
    if (logText.isEmpty()) return;

    QStringList lines = logText.split('\n', Qt::SkipEmptyParts);
    if (lines.size() > 150) lines = lines.mid(lines.size() - 150);
    const QString snippet = lines.join('\n');

    const QString sys =
        "Sei un esperto di sicurezza di rete e analisi del traffico. "
        "Analizza il seguente dump di pacchetti e fornisci:\n"
        "1. Panoramica del traffico (protocolli dominanti, host attivi)\n"
        "2. Anomalie o comportamenti sospetti\n"
        "3. Potenziali problemi di sicurezza (porte inusuali, scansioni, flood)\n"
        "4. Suggerimenti pratici per approfondire o mitigare i rischi.\n"
        "Rispondi in italiano, in modo conciso e strutturato.";

    const QString user =
        QString("Ecco i pacchetti catturati:\n\n```\n%1\n```").arg(snippet);

    m_netAiOutput->clear();
    m_netAiOutput->setPlainText("\xf0\x9f\xa4\x96  Analisi in corso...\n\n");
    m_btnNetAnalyze->setEnabled(false);

    disconnect(m_netAiTokenConn);
    disconnect(m_netAiFinishedConn);
    disconnect(m_netAiErrorConn);
    m_netAiTokenConn    = connect(m_ai, &AiClient::token,    this, &ProgrammazionePage::onNetAiToken);
    m_netAiFinishedConn = connect(m_ai, &AiClient::finished, this, &ProgrammazionePage::onNetAiFinished);
    m_netAiErrorConn    = connect(m_ai, &AiClient::error,    this, &ProgrammazionePage::onNetAiError);
    m_ai->chat(P::prependKnowledge(sys), user);
}

/* ══════════════════════════════════════════════════════════════
   buildReteLan — sub-tab "🌐 Rete LAN"
   Scanner LAN: ARP cache, nmap ping-sweep, info interfacce,
   calcolo subnet (rete/broadcast/host) — nessun AI richiesto.
   ══════════════════════════════════════════════════════════════ */
QWidget* ProgrammazionePage::buildReteLan(QWidget* parent)
{
    auto* w   = new QWidget(parent);
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(12, 12, 12, 12);
    lay->setSpacing(8);

    /* ── Info interfacce locali ── */
    m_lanInfoLbl = new QLabel(w);
    m_lanInfoLbl->setWordWrap(true);
    m_lanInfoLbl->setTextFormat(Qt::RichText);
    m_lanInfoLbl->setObjectName("hintLabel");
    lanRefreshInfo();

    /* ── Pulsanti di scansione ── */
    auto* btnRow = new QWidget(w);
    auto* btnLay = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(0, 0, 0, 0);
    btnLay->setSpacing(8);

    m_lanScanArp  = new QPushButton("\xf0\x9f\x94\x8d  ARP Cache (rapido)", btnRow);
    m_lanScanNmap = new QPushButton("\xf0\x9f\x8c\x90  Scan nmap (completo)", btnRow);
    m_lanStopBtn  = new QPushButton("\xe2\x8f\xb9  Stop", btnRow);
    m_lanStopBtn->setObjectName("actionBtn");
    m_lanStopBtn->setProperty("danger", true);
    m_lanStopBtn->setEnabled(false);
    auto* btnRefresh = new QPushButton("\xf0\x9f\x94\x84  Aggiorna info", btnRow);

    btnLay->addWidget(m_lanScanArp);
    btnLay->addWidget(m_lanScanNmap);
    btnLay->addWidget(m_lanStopBtn);
    btnLay->addStretch();
    btnLay->addWidget(btnRefresh);

    /* ── Tabella risultati: IP | MAC | Hostname | Stato ── */
    m_lanTable = new QTableWidget(0, 4, w);
    m_lanTable->setHorizontalHeaderLabels({" IP", " MAC Address", " Hostname", " Stato"});
    m_lanTable->setColumnWidth(0, 130);
    m_lanTable->setColumnWidth(1, 158);
    m_lanTable->setColumnWidth(3, 100);
    m_lanTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_lanTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_lanTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_lanTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    m_lanTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_lanTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_lanTable->setAlternatingRowColors(true);
    m_lanTable->verticalHeader()->setVisible(false);

    /* ── Status scan / info subnet ── */
    m_lanStatusLbl = new QLabel(w);
    m_lanStatusLbl->setObjectName("hintLabel");
    m_lanStatusLbl->setText(
        "\xf0\x9f\x94\x8c Premi un pulsante per scansionare la rete locale.");

    lay->addWidget(btnRow);
    lay->addWidget(m_lanTable, 1);
    lay->addWidget(m_lanStatusLbl);
    lay->addWidget(m_lanInfoLbl);

    /* Connessioni */
    connect(m_lanScanArp, &QPushButton::clicked,
            this, &ProgrammazionePage::onLanScanArpClicked);
    connect(m_lanScanNmap, &QPushButton::clicked,
            this, &ProgrammazionePage::onLanScanNmapClicked);
    connect(m_lanStopBtn, &QPushButton::clicked,
            this, &ProgrammazionePage::onLanStopBtnClicked);
    connect(btnRefresh, &QPushButton::clicked,
            this, &ProgrammazionePage::lanRefreshInfo);
    return w;
}

/* ══════════════════════════════════════════════════════════════
   buildDriverKernelTab — sub-tab "🔧 Driver & Kernel"
   Sub-tab NVIDIA / AMD / Kernel con guida AI e comandi read-only.
   ══════════════════════════════════════════════════════════════ */
QWidget* ProgrammazionePage::buildDriverKernelTab(QWidget* parent)
{
    auto* wrap = new QWidget(parent);
    auto* lay  = new QVBoxLayout(wrap);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    auto* tabs = new QTabWidget(wrap);
    tabs->setObjectName("innerTabs");

    /* ════════════════════════════════
       Sub-tab 1: NVIDIA
       ════════════════════════════════ */
    {
        auto* w   = new QWidget(tabs);
        auto* vl  = new QVBoxLayout(w);
        vl->setContentsMargins(12, 12, 12, 12);
        vl->setSpacing(8);

        auto* info = new QLabel(w);
        info->setWordWrap(true);
        info->setTextFormat(Qt::RichText);
        info->setObjectName("hintLabel");
        info->setText(
            "<b>\xf0\x9f\x9f\xa2 Driver NVIDIA</b><br>"
            "Gestisci i driver proprietari NVIDIA per Linux. "
            "<code>nvidia-smi</code> mostra lo stato della GPU, i processi attivi "
            "e la versione del driver installato. "
            "DKMS garantisce che il modulo kernel venga ricompilato automaticamente "
            "dopo ogni aggiornamento del kernel."
        );
        vl->addWidget(info);

        /* Area output condivisa — puntata da m_driverOutput */
        m_driverOutput = new QTextEdit(w);
        m_driverOutput->setReadOnly(true);
        m_driverOutput->setObjectName("codeOutput");
        m_driverOutput->setPlaceholderText(
            "L'output dei comandi apparir\xc3\xa0 qui...");
        vl->addWidget(m_driverOutput, 1);

        auto* btnRow = new QWidget(w);
        auto* bl     = new QHBoxLayout(btnRow);
        bl->setContentsMargins(0, 0, 0, 0);
        bl->setSpacing(8);

        auto* btnDetect   = new QPushButton(
            "\xf0\x9f\x94\x8d  Rilevamento GPU", btnRow);
        auto* btnDownload = new QPushButton(
            "\xf0\x9f\x93\xa6  Scarica Driver", btnRow);
        auto* btnGuide    = new QPushButton(
            "\xf0\x9f\x92\xa1  Guida AI", btnRow);
        auto* btnDkms     = new QPushButton(
            "\xf0\x9f\x94\x84  Reinstalla DKMS", btnRow);

        bl->addWidget(btnDetect);
        bl->addWidget(btnDownload);
        bl->addWidget(btnGuide);
        bl->addWidget(btnDkms);
        bl->addStretch();
        vl->addWidget(btnRow);

        connect(btnDetect,   &QPushButton::clicked,
                this, &ProgrammazionePage::onNvidiaDetectClicked);
        connect(btnGuide,    &QPushButton::clicked,
                this, &ProgrammazionePage::onNvidiaGuideClicked);
        connect(btnDownload, &QPushButton::clicked,
                this, &ProgrammazionePage::onNvidiaDownloadClicked);
        connect(btnDkms,     &QPushButton::clicked,
                this, &ProgrammazionePage::onNvidiaDkmsClicked);

        tabs->addTab(w, "\xf0\x9f\x9f\xa2  Driver NVIDIA");
    }

    /* ════════════════════════════════
       Sub-tab 2: AMD
       ════════════════════════════════ */
    {
        auto* w   = new QWidget(tabs);
        auto* vl  = new QVBoxLayout(w);
        vl->setContentsMargins(12, 12, 12, 12);
        vl->setSpacing(8);

        auto* info = new QLabel(w);
        info->setWordWrap(true);
        info->setTextFormat(Qt::RichText);
        info->setObjectName("hintLabel");
        info->setText(
            "<b>\xf0\x9f\x94\xb4 Driver AMD</b><br>"
            "Il driver <code>amdgpu</code> \xc3\xa8 gi\xc3\xa0 incluso nel kernel Linux "
            "e viene caricato automaticamente per le GPU AMD/Radeon. "
            "<b>ROCm</b> \xc3\xa8 il framework AMD per compute GPU (ML/AI) "
            "e va installato separatamente. "
            "<code>lspci | grep VGA</code> mostra la GPU rilevata dal sistema."
        );
        vl->addWidget(info);

        auto* outAmd = new QTextEdit(w);
        outAmd->setReadOnly(true);
        outAmd->setObjectName("codeOutput");
        outAmd->setPlaceholderText(
            "L'output dei comandi apparir\xc3\xa0 qui...");
        vl->addWidget(outAmd, 1);

        auto* btnRow = new QWidget(w);
        auto* bl     = new QHBoxLayout(btnRow);
        bl->setContentsMargins(0, 0, 0, 0);
        bl->setSpacing(8);

        auto* btnDetect   = new QPushButton(
            "\xf0\x9f\x94\x8d  Rilevamento GPU", btnRow);
        auto* btnDownload = new QPushButton(
            "\xf0\x9f\x93\xa6  Scarica ROCm", btnRow);
        auto* btnGuide    = new QPushButton(
            "\xf0\x9f\x92\xa1  Guida AI", btnRow);

        bl->addWidget(btnDetect);
        bl->addWidget(btnDownload);
        bl->addWidget(btnGuide);
        bl->addStretch();
        vl->addWidget(btnRow);

        m_driverAmdOutput = outAmd;

        connect(btnDetect,   &QPushButton::clicked,
                this, &ProgrammazionePage::onAmdDetectClicked);
        connect(btnGuide,    &QPushButton::clicked,
                this, &ProgrammazionePage::onAmdGuideClicked);
        connect(btnDownload, &QPushButton::clicked,
                this, &ProgrammazionePage::onAmdDownloadClicked);

        tabs->addTab(w, "\xf0\x9f\x94\xb4  Driver AMD");
    }

    /* ════════════════════════════════
       Sub-tab 3: Kernel Linux
       ════════════════════════════════ */
    {
        auto* w   = new QWidget(tabs);
        auto* vl  = new QVBoxLayout(w);
        vl->setContentsMargins(12, 12, 12, 12);
        vl->setSpacing(8);

        auto* info = new QLabel(w);
        info->setWordWrap(true);
        info->setTextFormat(Qt::RichText);
        info->setObjectName("hintLabel");
        info->setText(
            "<b>\xf0\x9f\x90\xa7 Kernel Linux</b><br>"
            "Visualizza la versione kernel attiva con <code>uname -r</code>. "
            "Su sistemi Debian/Ubuntu puoi elencare i kernel installati con "
            "<code>dpkg --list | grep linux-image</code>. "
            "<b>Attenzione:</b> compilare un kernel personalizzato \xc3\xa8 un'operazione "
            "avanzata riservata a utenti esperti."
        );
        vl->addWidget(info);

        auto* outKernel = new QTextEdit(w);
        outKernel->setReadOnly(true);
        outKernel->setObjectName("codeOutput");
        outKernel->setPlaceholderText(
            "L'output dei comandi apparir\xc3\xa0 qui...");
        vl->addWidget(outKernel, 1);

        auto* btnRow = new QWidget(w);
        auto* bl     = new QHBoxLayout(btnRow);
        bl->setContentsMargins(0, 0, 0, 0);
        bl->setSpacing(8);

        auto* btnVer    = new QPushButton(
            "\xf0\x9f\x94\x8d  Versione attuale", btnRow);
        auto* btnList   = new QPushButton(
            "\xf0\x9f\x93\x8b  Lista kernel", btnRow);
        auto* btnGuide  = new QPushButton(
            "\xf0\x9f\x92\xa1  Compila Kernel (AI)", btnRow);
        auto* btnSafety = new QPushButton(
            "\xe2\x9a\xa0  Nota sicurezza", btnRow);

        bl->addWidget(btnVer);
        bl->addWidget(btnList);
        bl->addWidget(btnGuide);
        bl->addWidget(btnSafety);
        bl->addStretch();
        vl->addWidget(btnRow);

        m_driverKernelOutput = outKernel;

        connect(btnVer,    &QPushButton::clicked,
                this, &ProgrammazionePage::onKernelVersionClicked);
        connect(btnList,   &QPushButton::clicked,
                this, &ProgrammazionePage::onKernelListClicked);
        connect(btnGuide,  &QPushButton::clicked,
                this, &ProgrammazionePage::onKernelGuideClicked);
        connect(btnSafety, &QPushButton::clicked,
                this, &ProgrammazionePage::onKernelSafetyClicked);

        tabs->addTab(w, "\xf0\x9f\x90\xa7  Kernel Linux");
    }

    lay->addWidget(tabs);
    return wrap;
}

void ProgrammazionePage::lanRefreshInfo()
{
    if (!m_lanInfoLbl) return;

    auto isPhysical = [](const QString& n) {
        return n.startsWith("en") || n.startsWith("eth") ||
               n.startsWith("wl") || n.startsWith("em") || n.startsWith("ib");
    };

    struct IfRow { QString icon, name, ip, pfx, net, mac; bool physical; };
    QList<IfRow> rows;

    for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
        if (iface.flags().testFlag(QNetworkInterface::IsLoopBack)) continue;
        if (!iface.flags().testFlag(QNetworkInterface::IsUp)) continue;
        const QString hw = iface.hardwareAddress().toUpper();
        for (const QNetworkAddressEntry& e : iface.addressEntries()) {
            if (e.ip().protocol() != QAbstractSocket::IPv4Protocol) continue;
            const quint32 ipRaw  = e.ip().toIPv4Address();
            const quint32 mskRaw = e.netmask().toIPv4Address();
            int pfx = 0; quint32 tmp = mskRaw;
            while (tmp) { pfx += (tmp >> 31) & 1; tmp <<= 1; }
            const bool phys = isPhysical(iface.name());
            const QString icon = iface.name().startsWith("wl")
                ? "\xf0\x9f\x93\xa1"   /* 📡 */
                : phys
                    ? "\xf0\x9f\x94\x8c"   /* 🔌 */
                    : "\xf0\x9f\x94\xa7";  /* 🔧 */
            rows.append({ icon, iface.name(), e.ip().toString(),
                          QString::number(pfx),
                          QHostAddress(ipRaw & mskRaw).toString(),
                          hw.isEmpty() ? "N/D" : hw, phys });
        }
    }

    if (rows.isEmpty()) {
        m_lanInfoLbl->setText("<i>\xf0\x9f\x9f\xa1 Nessuna interfaccia IPv4 attiva</i>");
        return;
    }

    /* Fisiche prima, poi virtuali */
    std::stable_partition(rows.begin(), rows.end(), [](const IfRow& r){ return r.physical; });

    QString html =
        "<table cellspacing='0' cellpadding='3' style='border-collapse:collapse'>"
        "<tr>"
        "<td style='color:#888;padding-right:16px'><small><b>Interfaccia</b></small></td>"
        "<td style='color:#888;padding-right:16px'><small><b>Indirizzo IP</b></small></td>"
        "<td style='color:#888;padding-right:16px'><small><b>Rete</b></small></td>"
        "<td style='color:#888'><small><b>MAC Address</b></small></td>"
        "</tr>";

    bool virtSepAdded = false;
    for (const IfRow& r : rows) {
        if (!r.physical && !virtSepAdded) {
            /* separatore prima delle virtuali */
            html += "<tr><td colspan='4' style='padding-top:4px;padding-bottom:2px'>"
                    "<small><span style='color:#555'>"
                    "\xe2\x94\x80\xe2\x94\x80"
                    " Virtuali / Bridge "
                    "\xe2\x94\x80\xe2\x94\x80"
                    "</span></small></td></tr>";
            virtSepAdded = true;
        }
        const QString style = r.physical
            ? "padding-right:16px"
            : "padding-right:16px;color:#777";
        const QString nameCell = r.physical
            ? QString("<b>%1 %2</b>").arg(r.icon, r.name)
            : QString("<small>%1 %2</small>").arg(r.icon, r.name);
        const QString dataStyle = r.physical ? "" : "color:#777";
        html += QString(
            "<tr>"
            "<td style='%1'>%2</td>"
            "<td style='%1'><small><code style='%3'>%4/%5</code></small></td>"
            "<td style='%1'><small><code style='%3'>%6</code></small></td>"
            "<td><small><code style='%3'>%7</code></small></td>"
            "</tr>")
            .arg(style, nameCell, dataStyle,
                 r.ip, r.pfx, r.net, r.mac);
    }
    html += "</table>";
    m_lanInfoLbl->setText(html);
}

bool ProgrammazionePage::hasUnsavedWork() const {
    return m_editor && m_editor->document() && m_editor->document()->isModified();
}

void ProgrammazionePage::saveCurrentFile() {
    if (!m_editor) return;
    const QString path = m_currentFilePath.isEmpty()
        ? QFileDialog::getSaveFileName(this, "Salva codice",
              QDir::homePath() + "/codice.py",
              "Python (*.py);;C++ (*.cpp);;Testo (*.txt);;Tutti (*.*)")
        : m_currentFilePath;
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
    QTextStream(&f) << m_editor->toPlainText();
    m_editor->document()->setModified(false);
}
