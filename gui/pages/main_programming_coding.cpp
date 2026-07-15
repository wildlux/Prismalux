/* ══════════════════════════════════════════════════════════════
   main_programming_coding.cpp — ProgrammazionePage: Coding tab
   ==================================================================
   Sub-tab "💻 Programmazione" — editor, esecuzione, pannello AI,
   Loop Fix, showDiff/runLint. Split da
   main_programming.cpp/main_programming_slots.cpp (TODO D-8).
   ══════════════════════════════════════════════════════════════ */
#include "main_programming.h"
#include "main_programming_p.h"
#include "../prismalux_paths.h"
#include "../log_bus.h"
#include "../ai_utils.h"
#include "../dpi_utils.h"
#include "../widgets/ai_error_widget.h"
#include "../widgets/toggle_switch.h"

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
#include <QCheckBox>
#include <QFrame>
#include <QSlider>
#include <QFont>
#include <QTimer>
#include <QTextCursor>
#include <QDir>
#include <QFile>
#include <QTemporaryFile>
#include <QStandardPaths>
#include <QMessageBox>
#include <QRegularExpression>
#include <QAbstractButton>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>

namespace P = PrismaluxPaths;

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

    toolLay->addWidget(new QLabel(tr("Linguaggio:"), toolRow));
    m_lang = new QComboBox(toolRow);
    m_lang->setObjectName("settingCombo");
    m_lang->addItem("Python",     QString("py"));
    m_lang->addItem("C",          QString("c"));
    m_lang->addItem("C++",        QString("cpp"));
    m_lang->addItem("Bash",       QString("sh"));
    m_lang->addItem("JavaScript", QString("js"));
    m_lang->setFixedWidth(dpiScale(130));
    toolLay->addWidget(m_lang);
    toolLay->addSpacing(8);

    auto tagExecP = [](QPushButton* btn, const char* icon, const char* text){
        btn->setProperty("execFull", btn->text());
        btn->setProperty("execIcon", QString::fromUtf8(icon));
        btn->setProperty("execText", QString::fromUtf8(text));
    };

    m_btnRun = new QPushButton(tr("\xe2\x96\xb6  Esegui"), toolRow);
    m_btnRun->setObjectName("actionBtn");
    m_btnRun->setToolTip(tr("Esegui il codice nell’editor (F5)"));
    tagExecP(m_btnRun, "\xe2\x96\xb6", "Esegui");

    auto* btnClear = new QPushButton(tr("\xf0\x9f\x97\x91  Pulisci"), toolRow);
    btnClear->setObjectName("actionBtn");

    toolLay->addWidget(m_btnRun);
    toolLay->addWidget(btnClear);
    toolLay->addStretch(1);

    m_btnAi = new QPushButton(tr("\xf0\x9f\xa4\x96  Chiedi all’AI"), toolRow);
    m_btnAi->setObjectName("actionBtn");
    m_btnAi->setCheckable(true);
    m_btnAi->setToolTip(tr("Apri il pannello AI per scrivere una richiesta"));
    tagExecP(m_btnAi, "\xf0\x9f\xa4\x96", "Chiedi all’AI");
    toolLay->addWidget(m_btnAi);

    m_btnFix = new QPushButton(tr("\xf0\x9f\x94\xa7  Correggi con AI"), toolRow);
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
    m_fixSlider->setFixedWidth(dpiScale(80));
    m_fixSlider->setToolTip(tr("Numero massimo di tentativi Loop Fix (10 = illimitati)"));
    m_fixSliderLbl = new QLabel(QString::number(m_loopMax), toolRow);
    m_fixSliderLbl->setObjectName("cardDesc");
    m_fixSliderLbl->setFixedWidth(dpiScale(22));
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

    auto* editorGroup = new QGroupBox(tr("\xf0\x9f\x96\x8a  Codice"), parent);
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

    auto* outputGroup = new QGroupBox(tr("\xf0\x9f\x93\x9f  Output"), rightCol);
    outputGroup->setObjectName("cardGroup");
    auto* outputLay = new QVBoxLayout(outputGroup);
    outputLay->setContentsMargins(4, 8, 4, 4);
    outputLay->setSpacing(4);

    m_status = new QLabel(tr("Pronto."), outputGroup);
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
        tr("\xf0\x9f\x93\x88  Grafico \xe2\x80\x94 output numerico"), rightCol);
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

    auto* lblModel = new QLabel(tr("\xf0\x9f\xa4\x96  Modello:"), modelRow);
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
    outBtnRefreshMod->setFixedWidth(dpiScale(32));
    outBtnRefreshMod->setToolTip(tr("Aggiorna lista modelli disponibili"));
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

    m_btnSend = new QPushButton(tr("Invia \xe2\x96\xb6"), aiInputRow);
    m_btnSend->setObjectName("m_btnSend");

    m_btnInsert = new QPushButton(
        tr("\xe2\x86\x91  Inserisci in editor"), aiInputRow);
    m_btnInsert->setObjectName("actionBtn");
    m_btnInsert->setToolTip(
        tr("Estrae il primo blocco di codice dalla risposta AI e lo inserisce nell’editor"));
    m_btnInsert->setEnabled(false);

    outBtnCloseAi = new QPushButton("\xe2\x9c\x95", aiInputRow);
    outBtnCloseAi->setObjectName("actionBtn");
    outBtnCloseAi->setFixedWidth(dpiScale(32));
    outBtnCloseAi->setToolTip(tr("Chiudi pannello AI"));

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
        m_btnRun->setText(tr("\xe2\x96\xa0  Stop"));
        m_btnRun->setProperty("danger", true);
    } else {
        m_btnRun->setText(tr("\xe2\x96\xb6  Esegui"));
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
        m_status->setText(tr("\xe2\x9d\x8c  Nessun codice da eseguire."));
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
        LogBus::post("\xe2\x9d\x8c Programmazione: Impossibile creare file temp: " + filePath);
        return;
    }
    f.write(code.toUtf8());
    f.close();

    const QString cmd = buildRunCommand(filePath);
    if (cmd.isEmpty()) {
        m_status->setText(tr("\xe2\x9d\x8c  Linguaggio non supportato."));
        LogBus::post("\xe2\x9d\x8c Programmazione: Linguaggio non supportato.");
        return;
    }

    m_output->clear();
    m_fullOutput.clear();
    m_chart->clearAll();
    if (m_chartGroup) m_chartGroup->hide();
    appendOutput(QString("$ %1\n").arg(cmd));
    setRunning(true);
    m_status->setText(tr("\xe2\x8f\xb3  Esecuzione in corso..."));

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
        m_status->setText(tr("\xe2\x9d\x8c  Nessun codice nell'editor da correggere."));
        LogBus::post("\xe2\x9d\x8c Programmazione: Nessun codice nell'editor da correggere.");
        return;
    }
    if (!m_ai) {
        m_status->setText(tr("\xe2\x9d\x8c  AI non disponibile."));
        LogBus::post("\xe2\x9d\x8c Programmazione: AI non disponibile.");
        return;
    }
    if (m_ai->busy()) {
        m_status->setText(tr("\xe2\x9a\xa0\xef\xb8\x8f  AI occupata. Attendi o premi Stop."));
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

/* ======================================================================
   Sezione 12 — Metodi di supporto (showDiff, runLint)
   Dichiarati nel header ma logica da implementare
   ====================================================================== */

void ProgrammazionePage::showDiff(const QString& before, const QString& after)
{
    /* Diff semplice riga-per-riga: verde = aggiunto, rosso = rimosso */
    if (!m_diffGroup || !m_diffView) return;

    const QStringList oldLines = before.split('\n');
    const QStringList newLines = after.split('\n');

    QString html = "<pre style=\"font-family:monospace;\">";
    /* Mostra le righe di before che non compaiono in after (rimosse) */
    for (const QString& l : oldLines) {
        if (!newLines.contains(l))
            html += "<span style=\"color:#e05050;background:#3a1010;\">- "
                  + l.toHtmlEscaped() + "</span>\n";
    }
    /* Mostra le righe di after che non compaiono in before (aggiunte) */
    for (const QString& l : newLines) {
        if (!oldLines.contains(l))
            html += "<span style=\"color:#50c050;background:#103a10;\">+ "
                  + l.toHtmlEscaped() + "</span>\n";
    }
    html += "</pre>";

    m_diffView->setHtml(html);
    m_diffGroup->setVisible(!before.isEmpty() && before != after);
}

void ProgrammazionePage::runLint()
{
    if (!m_editor || !m_lang || !m_diffGroup || !m_diffView) return;
    const QString code = m_editor->toPlainText().trimmed();
    if (code.isEmpty()) return;

    const QString ext = m_lang->currentData().toString(); /* py c cpp sh js */

    /* Mappa linguaggio → linter + argomento */
    struct LintCfg { QString prog; QStringList args; bool needFile; };
    LintCfg cfg;
    if (ext == "py") {
        cfg = { PrismaluxPaths::findPython(),
                {"-m", "pyflakes"}, true };
    } else if (ext == "cpp" || ext == "c") {
        if (QStandardPaths::findExecutable("clang-tidy").isEmpty()) {
            if (m_status) m_status->setText(
                tr("\xe2\x9a\xa0  clang-tidy non trovato — installa: sudo apt install clang-tidy"));
            return;
        }
        cfg = { "clang-tidy", {"-checks=*,-fuchsia-*"}, true };
    } else if (ext == "js") {
        if (QStandardPaths::findExecutable("eslint").isEmpty()) {
            if (m_status) m_status->setText(
                tr("\xe2\x9a\xa0  eslint non trovato — installa: npm install -g eslint"));
            return;
        }
        cfg = { "eslint", {"--no-eslintrc", "--rule", "no-unused-vars:warn"}, true };
    } else {
        if (m_status) m_status->setText(
            "\xf0\x9f\x94\x8d  Linting non supportato per " + m_lang->currentText());
        return;
    }

    /* Scrive il codice in un file temporaneo */
    auto* tmp = new QTemporaryFile("prismalux_lint_XXXXXX." + ext, this);
    tmp->setAutoRemove(true);
    if (!tmp->open()) { tmp->deleteLater(); return; }
    tmp->write(code.toUtf8());
    tmp->flush();
    const QString tmpPath = tmp->fileName();

    if (m_status)
        m_status->setText(tr("\xf0\x9f\x94\x8d  Analisi in corso (") + m_lang->currentText() + ")...");
    m_diffGroup->setVisible(false);

    auto* proc = new QProcess(this);
    proc->setProcessChannelMode(QProcess::MergedChannels);
    QStringList args = cfg.args;
    if (cfg.needFile) args << tmpPath;

    connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, tmp, ext](int exitCode, QProcess::ExitStatus) {
        const QString out = QString::fromUtf8(proc->readAll()).trimmed();
        proc->deleteLater();
        tmp->deleteLater();

        if (out.isEmpty() && exitCode == 0) {
            if (m_status) m_status->setText(
                "\xe2\x9c\x85  Nessun problema trovato (" + m_lang->currentText() + ")");
            m_diffGroup->setVisible(false);
            return;
        }

        /* Mostra output nel diffView con colori */
        QString html = "<pre style='font-family:monospace;font-size:11px;"
                       "white-space:pre-wrap;'>";
        for (const QString& line : out.split('\n')) {
            const QString lo = line.toLower();
            if (lo.contains("error") || lo.contains("errore"))
                html += "<span style='color:#f87171;'>" + line.toHtmlEscaped() + "</span>\n";
            else if (lo.contains("warning") || lo.contains("warn") || lo.contains("W "))
                html += "<span style='color:#fbbf24;'>" + line.toHtmlEscaped() + "</span>\n";
            else
                html += "<span style='color:#94a3b8;'>" + line.toHtmlEscaped() + "</span>\n";
        }
        html += "</pre>";
        m_diffView->setHtml(html);
        m_diffGroup->setVisible(true);
        if (m_status) m_status->setText(
            (exitCode == 0 ? "\xe2\x9c\x85  " : "\xe2\x9d\x8c  ") +
            m_lang->currentText() + " lint completato");
    });

    connect(proc, &QProcess::errorOccurred, this,
            [this, proc, tmp](QProcess::ProcessError err) {
        if (err == QProcess::FailedToStart) {
            if (m_status) m_status->setText(
                tr("\xe2\x9d\x8c  Linter non avviabile — verifica installazione"));
            proc->deleteLater();
            tmp->deleteLater();
        }
    });

    proc->start(cfg.prog, args);
}

/* ======================================================================
   Sezione 1 — Coding tab (costruttore)
   ====================================================================== */

void ProgrammazionePage::onAutoFixToggled(bool on)
{
    if (!on) {
        /* Spegnimento: reset loop */
        m_loopActive = false;
        m_loopCount  = 0;
        return;
    }

    /* Avviso one-shot — persistito in QSettings ("Non mostrare più") */
    QSettings s("Prismalux", "GUI");
    if (s.value(P::SK::kLoopFixWarning, false).toBool()) return;

    QMessageBox dlg(this);
    dlg.setWindowTitle(tr("Loop Fix — Esecuzione automatica di codice AI"));
    dlg.setIcon(QMessageBox::Warning);
    dlg.setText(
        "<b>Loop Fix eseguir\xc3\xa0 automaticamente il codice</b><br>"
        "generato dall'AI con i tuoi privilegi utente.<br><br>"
        "Assicurati di usare questa funzione <b>solo con AI di cui ti fidi</b> "
        "e con codice che non accede a dati sensibili o al filesystem.<br><br>"
        "Il loop si ferma automaticamente dopo il numero di tentativi indicato dallo slider "
        "o se rileva un errore intenzionale (SyntaxError custom).");
    dlg.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    dlg.button(QMessageBox::Ok)->setText(tr("Ho capito, abilita"));
    dlg.button(QMessageBox::Cancel)->setText(tr("Annulla"));

    auto* chk = new QCheckBox(tr("Non mostrare pi\xc3\xb9 questo avviso"), &dlg);
    dlg.setCheckBox(chk);

    if (dlg.exec() != QMessageBox::Ok) {
        /* Utente ha annullato: disattiva il toggle senza ri-emettere toggled */
        if (m_toggleAutoFix) {
            m_toggleAutoFix->blockSignals(true);
            m_toggleAutoFix->setChecked(false);
            m_toggleAutoFix->blockSignals(false);
        }
        return;
    }
    if (chk->isChecked())
        s.setValue(P::SK::kLoopFixWarning, true);
}

void ProgrammazionePage::onFixSliderChanged(int v)
{
    m_loopMax = v;
    if (m_fixSliderLbl)
        m_fixSliderLbl->setText(v == 10 ? "\xe2\x88\x9e" : QString::number(v));
}

void ProgrammazionePage::onLangChanged(int /*idx*/)
{
    /* Mappa ext -> CodeHighlighter::Language */
    static const struct { const char* ext; CodeHighlighter::Language lang; } kLangMap[] = {
        { "py",  CodeHighlighter::Python  },
        { "c",   CodeHighlighter::C       },
        { "cpp", CodeHighlighter::Cpp     },
        { "sh",  CodeHighlighter::Bash    },
        { "js",  CodeHighlighter::Python  }, /* fallback */
    };

    const QString ext = m_lang->currentData().toString();
    for (const auto& e : kLangMap) {
        if (ext == e.ext) {
            if (m_highlighter) m_highlighter->setLanguage(e.lang);
            break;
        }
    }
    if (m_editor) m_editor->setPlainText(currentTemplate());
}

void ProgrammazionePage::onBtnClearClicked()
{
    if (m_output) m_output->clear();
    if (m_status) m_status->setText(tr("Pronto."));
    if (m_chartGroup) m_chartGroup->hide();
    m_fullOutput.clear();
}

void ProgrammazionePage::onBtnAiToggled(bool on)
{
    if (!m_aiPanel) return;
    m_aiPanel->setVisible(on);
    if (on) {
        if (m_modelCombo->count() <= 1)
            populateAiModels();
        if (m_aiInput) m_aiInput->setFocus();
    }
}

void ProgrammazionePage::onBtnCloseAiClicked()
{
    if (m_btnAi) m_btnAi->setChecked(false);
    if (m_aiPanel) m_aiPanel->hide();
}

void ProgrammazionePage::sendToAi()
{
    if (!m_ai || !m_aiInput || !m_aiOutput) return;

    const QString request = m_aiInput->text().trimmed();
    if (request.isEmpty()) return;

    if (m_ai->busy()) {
        m_aiOutput->appendPlainText(
            "\xe2\x9a\xa0\xef\xb8\x8f  AI occupata. Attendi o premi Stop.");
        return;
    }

    /* Applica il modello scelto nella combo */
    if (m_modelCombo) {
        const QString sel = m_modelCombo->currentData().toString();
        if (!sel.isEmpty() && sel != m_ai->model())
            m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), sel);
    }

    /* Guardia embedding */
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
            return;
        }
    }

    const QString code = m_editor ? m_editor->toPlainText() : QString();
    const QString lang = m_lang ? m_lang->currentText() : "Python";

    const QString sys = QString(
        "Sei un assistente programmatore esperto specializzato in %1. "
        "Rispondi alla richiesta dell'utente riguardante il codice che ti mostra. "
        "Se generi codice, mettilo in un blocco ```%2 ... ```. "
        "Rispondi SEMPRE in italiano.")
        .arg(lang, m_lang ? m_lang->currentData().toString() : "py");

    const QString user = code.isEmpty()
        ? request
        : QString("Codice attuale:\n```%1\n%2\n```\n\nRichiesta: %3")
          .arg(m_lang ? m_lang->currentData().toString() : "py", code, request);

    m_aiOutput->clear();
    {
        const QString mn = m_ai->model().isEmpty() ? "AI" : m_ai->model();
        m_aiOutput->insertPlainText(
            QString("\xf0\x9f\xa4\x96  %1\n%2\n\n")
            .arg(mn, QString(qMax(mn.length(), 20), '-')));
    }

    m_aiMode = true;
    m_btnInsert->setEnabled(false);
    setRunning(true);

    disconnect(m_aiTokenConn);
    disconnect(m_aiFinishedConn);
    disconnect(m_aiErrorConn);
    m_aiTokenConn    = connect(m_ai, &AiClient::token,    this, &ProgrammazionePage::onAiToken);
    m_aiFinishedConn = connect(m_ai, &AiClient::finished, this, &ProgrammazionePage::onAiFinished);
    m_aiErrorConn    = connect(m_ai, &AiClient::error,    this, &ProgrammazionePage::onAiError);

    m_ai->chat(P::prependKnowledge(sys), user);
}

void ProgrammazionePage::onBtnInsertClicked()
{
    const QString code = extractCodeBlock();
    if (code.isEmpty()) return;
    if (m_editor && !m_editor->toPlainText().trimmed().isEmpty()) {
        if (QMessageBox::question(this,
                "Sovrascrivere il codice?",
                "L'editor contiene codice.\nVuoi sostituirlo con la risposta AI?",
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
            return;
    }
    if (m_editor) m_editor->setPlainText(code);
}

void ProgrammazionePage::onBtnRunClicked()
{
    if (m_proc && m_proc->state() != QProcess::NotRunning) {
        m_proc->kill();
        setRunning(false);
        if (m_status) m_status->setText(tr("Esecuzione interrotta."));
        return;
    }
    runCode();
}

void ProgrammazionePage::onBtnFixClicked()
{
    /* Salva codice originale per diff */
    m_originalCode = m_editor ? m_editor->toPlainText() : QString();
    triggerFix(m_lastExitCode != 0 && !m_lastError.isEmpty());
}

void ProgrammazionePage::onModelChanged(const QString& newModel)
{
    /* Sincronizza tutte le combo modello */
    const auto syncCombo = [&newModel](QComboBox* cb) {
        if (!cb) return;
        int idx = cb->findData(newModel);
        if (idx < 0) idx = cb->findText(newModel, Qt::MatchContains);
        if (idx >= 0) {
            cb->blockSignals(true);
            cb->setCurrentIndex(idx);
            cb->blockSignals(false);
        } else {
            /* Aggiorna il primo elemento (modello attivo) */
            cb->blockSignals(true);
            cb->setItemText(0, newModel);
            cb->setItemData(0, newModel);
            cb->setCurrentIndex(0);
            cb->blockSignals(false);
        }
    };
    syncCombo(m_modelCombo);
    syncCombo(m_agentModel);
    syncCombo(m_revModel);
    syncCombo(m_gitAiModel);
}

void ProgrammazionePage::populateAiModels()
{
    if (m_ai && m_modelCombo) AiUtils::populateModelCombo(m_ai, m_modelCombo, this);
}

/* ======================================================================
   Sezione 2 — runCode process slots
   ====================================================================== */

void ProgrammazionePage::onProcReadyRead()
{
    if (!m_proc) return;
    const QString out = QString::fromLocal8Bit(m_proc->readAll());
    m_fullOutput += out;
    appendOutput(out);
}

void ProgrammazionePage::onProcFinished(int code, QProcess::ExitStatus /*status*/)
{
    m_lastExitCode = code;
    setRunning(false);

    if (code == 0) {
        if (m_status) m_status->setText(tr("\xe2\x9c\x85  Completato con successo."));
        tryShowChart();
        m_lastError.clear();
        m_loopActive = false;
        m_loopCount  = 0;
    } else {
        m_lastError = m_fullOutput.right(2000);
        if (m_status)
            m_status->setText(QString("\xe2\x9d\x8c  Errore (exit %1).").arg(code));
        LogBus::post(QString("\xe2\x9d\x8c Programmazione: Processo uscito con errore (exit %1).").arg(code));

        /* Loop Fix automatico */
        const QString src = m_editor ? m_editor->toPlainText() : QString();
        if (m_toggleAutoFix && m_toggleAutoFix->isChecked()
            && !isIntentionalError(m_fullOutput, src))
        {
            const int maxLoop = (m_loopMax >= 10) ? 9999 : m_loopMax;
            if (m_loopCount < maxLoop) {
                ++m_loopCount;
                if (m_status)
                    m_status->setText(
                        QString("\xf0\x9f\x94\x84  Loop Fix tentativo %1/%2...")
                        .arg(m_loopCount)
                        .arg(m_loopMax >= 10 ? QString("\xe2\x88\x9e") : QString::number(m_loopMax)));
                m_loopActive = true;
                m_originalCode = src;
                QTimer::singleShot(200, this, &ProgrammazionePage::onLoopFixTimer);
                return;
            } else {
                m_loopActive = false;
                if (m_status)
                    m_status->setText(QString("\xe2\x9d\x8c  Loop Fix esaurito (%1 tentativi).").arg(m_loopCount));
                LogBus::post(QString("\xe2\x9d\x8c Programmazione: Loop Fix esaurito (%1 tentativi).").arg(m_loopCount));
                m_loopCount = 0;
            }
        }
    }

    /* Pulizia file temp */
    if (!m_procFilePath.isEmpty())
        QFile::remove(m_procFilePath);
}

void ProgrammazionePage::onProcErrorOccurred(QProcess::ProcessError err)
{
    if (err == QProcess::FailedToStart) {
        if (m_status)
            m_status->setText(
                "\xe2\x9d\x8c  Impossibile avviare il processo. "
                "Controlla che il compilatore/interprete sia nel PATH.");
        LogBus::post("\xe2\x9d\x8c Programmazione: Impossibile avviare il processo.");
        setRunning(false);
    }
}

void ProgrammazionePage::onLoopFixTimer()
{
    triggerFix(true);
}

void ProgrammazionePage::onLoopRunTimer()
{
    runCode();
}

void ProgrammazionePage::onModelsReadyForFix(const QStringList&)
{
    disconnect(m_modelsReadyForFixConn);
    selectCoderModel();
    _doFix(m_pendingFixIncludeError, m_pendingFixCodice,
           m_pendingFixLang, m_pendingFixExt);
}

/* ======================================================================
   Sezione 3 — AI panel slots (Coding)
   ====================================================================== */

void ProgrammazionePage::onAiToken(const QString& tok)
{
    if (!m_aiOutput) return;
    m_aiOutput->moveCursor(QTextCursor::End);
    m_aiOutput->insertPlainText(tok);
    m_aiOutput->ensureCursorVisible();
}

void ProgrammazionePage::onAiFinished(const QString& /*full*/)
{
    disconnect(m_aiTokenConn);
    disconnect(m_aiFinishedConn);
    disconnect(m_aiErrorConn);
    m_aiMode = false;
    setRunning(false);
    if (m_btnInsert) m_btnInsert->setEnabled(!extractCodeBlock().isEmpty());
}

void ProgrammazionePage::onAiError(const QString& msg)
{
    disconnect(m_aiTokenConn);
    disconnect(m_aiFinishedConn);
    disconnect(m_aiErrorConn);
    m_aiMode = false;
    setRunning(false);
    if (m_aiOutput) {
        m_aiOutput->moveCursor(QTextCursor::End);
        m_aiOutput->insertPlainText(
            QString("\n\xe2\x9d\x8c  Errore AI: %1").arg(msg));
    }
    LogBus::post("\xe2\x9d\x8c Programmazione: Errore AI: " + msg);
}

/* ======================================================================
   Sezione 4 — Fix slots
   ====================================================================== */

void ProgrammazionePage::onFixToken(const QString& tok)
{
    if (!m_aiOutput) return;
    m_aiOutput->moveCursor(QTextCursor::End);
    m_aiOutput->insertPlainText(tok);
    m_aiOutput->ensureCursorVisible();
}

void ProgrammazionePage::onFixFinished(const QString& full)
{
    disconnect(m_aiTokenConn);
    disconnect(m_aiFinishedConn);
    disconnect(m_aiErrorConn);

    /* Ripristina il modello originale */
    if (!m_fixOriginalModel.isEmpty() && m_fixOriginalModel != m_ai->model())
        m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), m_fixOriginalModel);

    m_aiMode = false;
    setRunning(false);
    if (m_fixErrPanel) m_fixErrPanel->hide();

    /* Estrai il codice e sostituisci l'editor */
    static const QRegularExpression re(
        "```(?:\\w+)?\\n([\\s\\S]*?)```");
    const auto m = re.match(full);
    if (m.hasMatch()) {
        const QString newCode = m.captured(1).trimmed();
        if (!newCode.isEmpty() && m_editor) {
            showDiff(m_originalCode, newCode);
            m_editor->setPlainText(newCode);
            m_editor->document()->setModified(true);
        }
    }

    if (m_btnInsert) m_btnInsert->setEnabled(!extractCodeBlock().isEmpty());

    /* Loop Fix: riesegui dopo il fix */
    if (m_loopActive) {
        if (m_status)
            m_status->setText(
                QString("\xf0\x9f\x94\x84  Loop Fix: rieseguo dopo il fix (%1)...")
                .arg(m_loopCount));
        QTimer::singleShot(300, this, &ProgrammazionePage::onLoopRunTimer);
    }
}

void ProgrammazionePage::onFixError(const QString& msg)
{
    disconnect(m_aiTokenConn);
    disconnect(m_aiFinishedConn);
    disconnect(m_aiErrorConn);

    /* Ripristina il modello originale */
    if (m_ai && !m_fixOriginalModel.isEmpty() && m_fixOriginalModel != m_ai->model())
        m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), m_fixOriginalModel);

    m_aiMode = false;
    m_loopActive = false;
    setRunning(false);
    if (m_fixErrPanel)
        m_fixErrPanel->showError(msg, [this]{ onBtnFixClicked(); });
    if (m_aiOutput) {
        m_aiOutput->moveCursor(QTextCursor::End);
        m_aiOutput->insertPlainText(
            QString("\n\xe2\x9d\x8c  Errore Fix AI: %1").arg(msg));
    }
    LogBus::post("\xe2\x9d\x8c Programmazione: Errore Fix AI: " + msg);
}
