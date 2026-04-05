#include "programmazione_page.h"
#include "../prismalux_paths.h"

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
#include <QDateTime>
#include <QTextCursor>
#include <QFrame>
#include <QRegularExpression>

namespace P = PrismaluxPaths;

/* ══════════════════════════════════════════════════════════════
   ProgrammazionePage — costruttore
   ══════════════════════════════════════════════════════════════ */
ProgrammazionePage::ProgrammazionePage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    QFont monoFont;
    monoFont.setFamily("JetBrains Mono");
    monoFont.setStyleHint(QFont::Monospace);
    monoFont.setPointSize(11);

    auto* mainLay = new QVBoxLayout(this);
    mainLay->setContentsMargins(12, 12, 12, 12);
    mainLay->setSpacing(8);

    /* ── Titolo ── */
    auto* titleRow = new QWidget(this);
    auto* titleLay = new QHBoxLayout(titleRow);
    titleLay->setContentsMargins(0, 0, 0, 0);
    titleLay->setSpacing(12);
    auto* title = new QLabel("\xf0\x9f\x92\xbb  Programmazione", titleRow);
    title->setObjectName("pageTitle");
    titleLay->addWidget(title);
    titleLay->addStretch(1);
    mainLay->addWidget(titleRow);

    /* ── Toolbar ── */
    auto* toolRow = new QWidget(this);
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

    m_btnRun = new QPushButton("\xe2\x96\xb6  Esegui", toolRow);
    m_btnRun->setObjectName("actionBtn");
    m_btnRun->setToolTip("Esegui il codice nell'editor (F5)");

    m_btnStop = new QPushButton("\xe2\x96\xa0  Stop", toolRow);
    m_btnStop->setObjectName("actionBtn");
    m_btnStop->setProperty("danger", "true");
    m_btnStop->setEnabled(false);

    auto* btnClear = new QPushButton("\xf0\x9f\x97\x91  Pulisci", toolRow);
    btnClear->setObjectName("actionBtn");

    toolLay->addWidget(m_btnRun);
    toolLay->addWidget(m_btnStop);
    toolLay->addWidget(btnClear);
    toolLay->addStretch(1);

    m_btnAi = new QPushButton("\xf0\x9f\xa4\x96  Chiedi all'AI", toolRow);
    m_btnAi->setObjectName("actionBtn");
    m_btnAi->setCheckable(true);
    m_btnAi->setToolTip("Apri il pannello AI per scrivere una richiesta");
    toolLay->addWidget(m_btnAi);

    m_btnFix = new QPushButton("\xf0\x9f\x94\xa7  Correggi con AI", toolRow);
    m_btnFix->setObjectName("actionBtn");
    m_btnFix->setProperty("highlight", "true");
    m_btnFix->setToolTip(
        "Invia il codice a qwen2.5-coder (o il miglior modello coder disponibile)\n"
        "e chiedi di trovare e correggere tutti i bug.\n"
        "Se c'era un errore di esecuzione, viene incluso nel contesto.");
    toolLay->addWidget(m_btnFix);
    mainLay->addWidget(toolRow);

    /* ── Splitter principale (editor | output+grafico) ── */
    auto* mainSplit = new QSplitter(Qt::Horizontal, this);
    mainSplit->setChildrenCollapsible(false);

    /* Colonna sinistra: editor */
    auto* editorGroup = new QGroupBox("\xf0\x9f\x96\x8a  Codice", mainSplit);
    editorGroup->setObjectName("cardGroup");
    auto* editorLay = new QVBoxLayout(editorGroup);
    editorLay->setContentsMargins(4, 8, 4, 4);

    m_editor = new QPlainTextEdit(editorGroup);
    m_editor->setObjectName("codeEditor");
    m_editor->setFont(monoFont);
    m_editor->setTabStopDistance(32);
    m_editor->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_editor->setPlaceholderText("# Scrivi il codice qui,\n# oppure usa 🤖 Chiedi all'AI per generarlo.");
    editorLay->addWidget(m_editor);
    mainSplit->addWidget(editorGroup);

    /* Colonna destra: output + grafico */
    auto* rightCol = new QWidget(mainSplit);
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

    /* Grafico (nascosto finché l'output non contiene numeri) */
    auto* chartGroup = new QGroupBox("\xf0\x9f\x93\x88  Grafico — output numerico", rightCol);
    chartGroup->setObjectName("cardGroup");
    auto* chartLay = new QVBoxLayout(chartGroup);
    chartLay->setContentsMargins(4, 8, 4, 4);

    m_chart = new ChartWidget(chartGroup);
    m_chart->setMinimumHeight(180);
    chartLay->addWidget(m_chart);
    chartGroup->hide(); /* nascosto di default */
    rightLay->addWidget(chartGroup);

    /* Salva il puntatore al chartGroup per show/hide */
    auto* chartGroupPtr = chartGroup;

    mainSplit->addWidget(rightCol);
    mainSplit->setSizes({500, 450});
    mainLay->addWidget(mainSplit, 1);

    /* ── Pannello AI (nascosto di default) ── */
    m_aiPanel = new QWidget(this);
    m_aiPanel->setObjectName("aiPanel");
    auto* aiLay = new QVBoxLayout(m_aiPanel);
    aiLay->setContentsMargins(8, 8, 8, 8);
    aiLay->setSpacing(6);

    /* Separatore visivo */
    auto* aiSep = new QFrame(m_aiPanel);
    aiSep->setFrameShape(QFrame::HLine);
    aiSep->setObjectName("sidebarSep");
    aiLay->addWidget(aiSep);

    /* ── Riga selezione modello ── */
    auto* modelRow = new QWidget(m_aiPanel);
    auto* modelLay = new QHBoxLayout(modelRow);
    modelLay->setContentsMargins(0, 0, 0, 0);
    modelLay->setSpacing(8);

    auto* lblModel = new QLabel("\xf0\x9f\xa4\x96  Modello:", modelRow);
    lblModel->setObjectName("cardDesc");
    modelLay->addWidget(lblModel);

    m_modelCombo = new QComboBox(modelRow);
    m_modelCombo->setObjectName("settingCombo");
    /* Voce iniziale: mostra il modello attivo al momento (verrà aggiornata) */
    m_modelCombo->addItem(
        m_ai ? (m_ai->model().isEmpty()
                    ? "(nessun modello)"
                    : m_ai->model())
             : "(AI non disponibile)",
        m_ai ? m_ai->model() : QString());
    modelLay->addWidget(m_modelCombo, 1);

    auto* btnRefreshMod = new QPushButton("\xf0\x9f\x94\x84", modelRow);
    btnRefreshMod->setObjectName("actionBtn");
    btnRefreshMod->setFixedWidth(32);
    btnRefreshMod->setToolTip("Aggiorna lista modelli disponibili");
    modelLay->addWidget(btnRefreshMod);

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

    auto* btnSend = new QPushButton("Invia \xe2\x96\xb6", aiInputRow);
    btnSend->setObjectName("actionBtn");

    m_btnInsert = new QPushButton("\xe2\x86\x91  Inserisci in editor", aiInputRow);
    m_btnInsert->setObjectName("actionBtn");
    m_btnInsert->setToolTip("Estrae il primo blocco di codice dalla risposta AI e lo inserisce nell'editor");
    m_btnInsert->setEnabled(false);

    auto* btnCloseAi = new QPushButton("\xe2\x9c\x95", aiInputRow);
    btnCloseAi->setObjectName("actionBtn");
    btnCloseAi->setFixedWidth(32);
    btnCloseAi->setToolTip("Chiudi pannello AI");

    aiInputLay->addWidget(btnSend);
    aiInputLay->addWidget(m_btnInsert);
    aiInputLay->addWidget(btnCloseAi);
    aiLay->addWidget(aiInputRow);

    /* Output AI (streaming) */
    m_aiOutput = new QPlainTextEdit(m_aiPanel);
    m_aiOutput->setObjectName("chatLog");
    m_aiOutput->setReadOnly(true);
    m_aiOutput->setFont(monoFont);
    m_aiOutput->setMaximumBlockCount(3000);
    m_aiOutput->setMinimumHeight(100);
    m_aiOutput->setMaximumHeight(220);
    m_aiOutput->setPlaceholderText(
        "Qui apparir\xc3\xa0 la risposta dell'AI.\n\n"
        "Scrivi la tua richiesta sopra e premi Invia.");
    aiLay->addWidget(m_aiOutput);

    m_aiPanel->hide();
    mainLay->addWidget(m_aiPanel);

    /* ══════════════════════════════════════════════════════════
       Lambda: badge di raccomandazione per coding
       ══════════════════════════════════════════════════════════ */
    auto codingBadge = [](const QString& name) -> QString {
        const QString n = name.toLower();
        /* Modelli specializzati in coding */
        if (n.contains("coder") || n.contains("codellama") ||
                n.contains("starcoder") || n.contains("codegemma"))
            return "  \xf0\x9f\x92\xbb Ottimizzato Coding";
        /* Modelli generali ottimi per coding (evita math puri) */
        if ((n.contains("phi")) ||
                (n.contains("deepseek") && !n.contains("math")) ||
                (n.contains("qwen")     && !n.contains("math")))
            return "  \xe2\x9c\x94 Buono per Coding";
        return QString();
    };

    /* Restituisce true se il modello è solo per embedding (non supporta /api/chat) */
    auto isEmbedOnly = [](const QString& name) -> bool {
        const QString n = name.toLower();
        return n.contains("embed") || n.contains("minilm") ||
               n.contains("rerank") || n.contains("bge-") ||
               n.contains("e5-") || n.contains("gte-") ||
               n.contains("arctic-embed") || n.contains("-embed");
    };

    /* Lambda: recupera modelli e popola la combo — esclude embedding */
    auto populateAiModels = [this, codingBadge, isEmbedOnly]() {
        if (!m_ai) return;
        m_modelCombo->setEnabled(false);
        const QString cur = m_ai->model();
        auto* holder = new QObject(this);
        connect(m_ai, &AiClient::modelsReady, holder,
                [this, holder, codingBadge, isEmbedOnly, cur](const QStringList& list) {
            holder->deleteLater();
            m_modelCombo->clear();
            int selIdx = 0;
            for (const QString& mdl : list) {
                if (isEmbedOnly(mdl)) continue;   /* salta modelli embedding */
                m_modelCombo->addItem(mdl + codingBadge(mdl), mdl);
                if (mdl == cur)
                    selIdx = m_modelCombo->count() - 1;
            }
            if (m_modelCombo->count() == 0)
                m_modelCombo->addItem(
                    cur.isEmpty() ? "(nessun modello chat)" : cur, cur);
            else
                m_modelCombo->setCurrentIndex(selIdx);
            m_modelCombo->setEnabled(true);
        });
        m_ai->fetchModels();
    };

    connect(btnRefreshMod, &QPushButton::clicked, this, populateAiModels);

    /* ══════════════════════════════════════════════════════════
       Connessioni
       ══════════════════════════════════════════════════════════ */

    /* Cambia linguaggio → sostituisce sempre il codice nell'editor con il template */
    connect(m_lang, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int){
        m_editor->setPlainText(currentTemplate());
    });

    /* Template iniziale */
    m_editor->setPlainText(currentTemplate());

    /* Pulisci output */
    connect(btnClear, &QPushButton::clicked, this, [this, chartGroupPtr]{
        m_output->clear();
        m_fullOutput.clear();
        m_chart->clearAll();
        chartGroupPtr->hide();
        m_status->setText("Pronto.");
    });

    /* ── Apri/chiudi pannello AI ── */
    connect(m_btnAi, &QPushButton::toggled, this, [this, populateAiModels](bool on){
        m_aiPanel->setVisible(on);
        if (on) {
            /* Prima apertura o modello cambiato → aggiorna lista */
            if (m_modelCombo->count() <= 1)
                populateAiModels();
            m_aiInput->setFocus();
            m_aiInput->selectAll();
        }
    });
    connect(btnCloseAi, &QPushButton::clicked, this, [this]{
        m_btnAi->setChecked(false);
    });

    /* ── Invia richiesta all'AI ── */
    auto sendToAi = [this](){
        const QString richiesta = m_aiInput->text().trimmed();
        if (richiesta.isEmpty()) {
            m_aiOutput->setPlainText("\xe2\x9d\x8c  Scrivi la tua richiesta prima di premere Invia.");
            return;
        }
        if (!m_ai) {
            m_aiOutput->setPlainText("\xe2\x9d\x8c  AI non disponibile.");
            return;
        }
        if (m_ai->busy()) {
            m_aiOutput->setPlainText("\xe2\x9a\xa0\xef\xb8\x8f  AI occupata. Attendi o premi Stop.");
            return;
        }

        const QString lang = m_lang->currentText();
        const QString codiceAttuale = m_editor->toPlainText().trimmed();

        /* System prompt: capisce se l'utente vuole codice nuovo o analisi di codice esistente */
        QString sys;
        QString user;

        if (codiceAttuale.isEmpty()) {
            /* Editor vuoto → generazione codice */
            sys = QString(
                "Sei un programmatore esperto. L'utente ti chiede di scrivere codice in %1. "
                "Scrivi codice completo e funzionante, commentato in italiano. "
                "Metti il codice in un blocco ```%2 ... ```. "
                "Spiega brevemente cosa fa il codice dopo il blocco. "
                "Rispondi SEMPRE in italiano.")
                .arg(lang, lang.toLower());
            user = richiesta;
        } else {
            /* Editor ha codice → analisi/modifica basata sulla richiesta */
            sys = QString(
                "Sei un programmatore esperto. Ti viene mostrato del codice in %1 e una richiesta. "
                "Rispondi alla richiesta in italiano. "
                "Se riscrivi o modifichi il codice, usa un blocco ```%2 ... ```. "
                "Rispondi SEMPRE in italiano.")
                .arg(lang, lang.toLower());
            user = QString("Codice attuale:\n```%1\n%2\n```\n\nRichiesta: %3")
                   .arg(lang.toLower(), codiceAttuale, richiesta);
        }

        /* Applica il modello scelto dall'utente (se diverso dall'attivo) */
        if (m_modelCombo) {
            const QString sel = m_modelCombo->currentData().toString();
            if (!sel.isEmpty() && sel != m_ai->model())
                m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), sel);
        }

        /* Guardia: blocca modelli embedding che non supportano /api/chat */
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

        m_aiOutput->clear();
        /* Intestazione modello */
        {
            const QString modelName = m_ai->model().isEmpty() ? "AI" : m_ai->model();
            m_aiOutput->insertPlainText(
                QString("\xf0\x9f\xa4\x96  Modello: %1\n%2\n\n")
                    .arg(modelName, QString(modelName.length() + 12, '-')));
        }
        m_aiMode = true;
        m_btnInsert->setEnabled(false);
        setRunning(true);
        m_status->setText("\xf0\x9f\xa4\x96  AI in risposta...");

        auto* holder = new QObject(this);
        connect(m_ai, &AiClient::token, holder, [this](const QString& tok){
            m_aiOutput->moveCursor(QTextCursor::End);
            m_aiOutput->insertPlainText(tok);
            m_aiOutput->ensureCursorVisible();
        });
        connect(m_ai, &AiClient::finished, holder, [this, holder](const QString&){
            holder->deleteLater();
            m_aiMode = false;
            setRunning(false);
            m_status->setText("\xe2\x9c\x85  Risposta AI completata.");
            /* Abilita inserimento se c'è un blocco di codice nella risposta */
            m_btnInsert->setEnabled(!extractCodeBlock().isEmpty());
        });
        connect(m_ai, &AiClient::error, holder, [this, holder](const QString& msg){
            holder->deleteLater();
            m_aiMode = false;
            setRunning(false);
            m_aiOutput->appendPlainText(QString("\n\xe2\x9d\x8c  Errore: %1").arg(msg));
            m_status->setText("\xe2\x9d\x8c  Errore AI.");
        });

        m_ai->chat(sys, user);
    };

    connect(btnSend, &QPushButton::clicked, this, sendToAi);
    connect(m_aiInput, &QLineEdit::returnPressed, this, sendToAi);

    /* ── Inserisci codice dall'AI in editor ── */
    connect(m_btnInsert, &QPushButton::clicked, this, [this]{
        const QString code = extractCodeBlock();
        if (!code.isEmpty()) {
            m_editor->setPlainText(code);
            m_status->setText("\xe2\x9c\x85  Codice inserito nell'editor.");
        }
    });

    /* ── Esegui ── */
    connect(m_btnRun, &QPushButton::clicked, this, [this, chartGroupPtr]{
        const QString code = m_editor->toPlainText().trimmed();
        if (code.isEmpty()) {
            m_status->setText("\xe2\x9d\x8c  Nessun codice da eseguire.");
            return;
        }

        if (m_aiMode && m_ai && m_ai->busy()) m_ai->abort();
        m_aiMode = false;

        const QString ext      = m_lang->currentData().toString();
        const QString filePath = tempFilePath(ext);

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
        chartGroupPtr->hide();
        appendOutput(QString("$ %1\n").arg(cmd));
        setRunning(true);
        m_status->setText("\xe2\x8f\xb3  Esecuzione in corso...");

        if (m_proc) { m_proc->kill(); m_proc->deleteLater(); m_proc = nullptr; }
        m_proc = new QProcess(this);
        m_proc->setProcessChannelMode(QProcess::MergedChannels);
        m_proc->setWorkingDirectory(P::root());

        connect(m_proc, &QProcess::readyRead, this, [this]{
            const QString out = QString::fromLocal8Bit(m_proc->readAll());
            m_fullOutput += out;
            appendOutput(out);
        });

        connect(m_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, filePath, chartGroupPtr](int code, QProcess::ExitStatus){
            m_lastExitCode = code;
            if (code == 0) {
                m_lastError.clear();
                m_status->setText(QString("\xe2\x9c\x85  Completato  \xc2\xb7  %1")
                                  .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
                m_btnFix->setText("\xf0\x9f\x94\xa7  Correggi con AI");
            } else {
                /* Salva le ultime righe di errore per passarle all'AI */
                const QStringList lines = m_fullOutput.split('\n');
                int start = qMax(0, lines.size() - 20); /* ultime 20 righe */
                m_lastError = lines.mid(start).join('\n').trimmed();
                m_status->setText(QString("\xe2\x9d\x8c  Errore (exit %1)  \xc2\xb7  "
                                          "Premi \xf0\x9f\x94\xa7 per correggere con AI").arg(code));
                /* Evidenzia il pulsante Fix per guidare l'utente */
                m_btnFix->setText("\xf0\x9f\x94\xa7  Correggi errore");
            }
            setRunning(false);
            m_proc = nullptr;
            /* Rimuovi file temporaneo (non C/C++ dove il bin è separato) */
            const QString ext2 = m_lang->currentData().toString();
            if (ext2 != "c" && ext2 != "cpp") QFile::remove(filePath);
            /* Prova a mostrare il grafico se l'output è numerico */
            tryShowChart();
            chartGroupPtr->setVisible(!m_chart->sizeHint().isEmpty()
                                      && !parseNumbers(m_fullOutput).isEmpty());
        });

        connect(m_proc, &QProcess::errorOccurred, this,
                [this](QProcess::ProcessError err){
            if (err == QProcess::FailedToStart) {
                m_status->setText("\xe2\x9d\x8c  Comando non trovato. Installa le dipendenze.");
                setRunning(false);
                m_proc = nullptr;
            }
        });

#ifdef _WIN32
        m_proc->start("cmd", {"/c", cmd});
#else
        m_proc->start("sh", {"-c", cmd});
#endif
    });

    /* ── Stop ── */
    connect(m_btnStop, &QPushButton::clicked, this, [this]{
        if (m_proc) { m_proc->kill(); m_status->setText("\xe2\x9c\x8b  Interrotto."); }
        if (m_aiMode && m_ai && m_ai->busy()) {
            m_ai->abort();
            m_aiMode = false;
            m_status->setText("\xe2\x9c\x8b  Risposta AI interrotta.");
        }
        setRunning(false);
    });

    /* ── Correggi con AI ── */
    connect(m_btnFix, &QPushButton::clicked, this, [this]{
        const bool hasError = (m_lastExitCode != 0 && !m_lastError.isEmpty());
        triggerFix(hasError);
    });
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
    const QString outBin = QDir::toNativeSeparators(
        QDir::tempPath() + QDir::separator() + "prisma_code_bin"
#ifdef _WIN32
        + ".exe"
#endif
    );

    if (ext == "py")  return QString("python3 \"%1\"").arg(fp);
    if (ext == "sh")  return QString("bash \"%1\"").arg(fp);
    if (ext == "js")  return QString("node \"%1\"").arg(fp);
    if (ext == "c")
        return QString("gcc -Wall -o \"%1\" \"%2\" && \"%1\"").arg(outBin, fp);
    if (ext == "cpp")
        return QString("g++ -Wall -std=c++17 -o \"%1\" \"%2\" && \"%1\"").arg(outBin, fp);
    return {};
}

/* ══════════════════════════════════════════════════════════════
   tempFilePath — percorso file temporaneo
   ══════════════════════════════════════════════════════════════ */
QString ProgrammazionePage::tempFilePath(const QString& ext) const
{
    return QDir::tempPath() + QDir::separator() + "prisma_code." + ext;
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
    m_btnRun->setEnabled(!running);
    m_btnStop->setEnabled(running);
    m_lang->setEnabled(!running);
    m_btnFix->setEnabled(!running);
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
           prima di selezionare il coder — usiamo un one-shot su modelsReady */
        if (m_modelCombo->count() <= 1) {
            auto* holder = new QObject(this);
            connect(m_ai, &AiClient::modelsReady, holder,
                    [this, holder, includeError, codice, lang, ext]
                    (const QStringList&){
                holder->deleteLater();
                selectCoderModel();
                _doFix(includeError, codice, lang, ext);
            });
            return; /* il lambda riprende dopo fetch */
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
    /* Salva il modello attivo PRIMA di cambiarlo — verrà ripristinato al termine */
    const QString originalModel = m_ai->model();

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
            if (!originalModel.isEmpty() && originalModel != m_ai->model())
                m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), originalModel);
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

    auto* holder = new QObject(this);
    connect(m_ai, &AiClient::token, holder, [this](const QString& tok){
        m_aiOutput->moveCursor(QTextCursor::End);
        m_aiOutput->insertPlainText(tok);
        m_aiOutput->ensureCursorVisible();
    });
    connect(m_ai, &AiClient::finished, holder,
            [this, holder, originalModel](const QString&){
        holder->deleteLater();
        m_aiMode = false;
        setRunning(false);
        m_btnFix->setText("\xf0\x9f\x94\xa7  Correggi con AI");
        /* Inserisci automaticamente il codice corretto nell'editor */
        const QString fixed = extractCodeBlock();
        if (!fixed.isEmpty()) {
            m_editor->setPlainText(fixed);
            m_lastExitCode = 0;
            m_lastError.clear();
            m_status->setText("\xe2\x9c\x85  Codice corretto inserito automaticamente nell'editor.");
        } else {
            m_status->setText("\xe2\x9c\x85  Correzione completata (nessun blocco codice estratto).");
        }
        m_btnInsert->setEnabled(false);
        /* Ripristina il modello originale — non contaminare le altre schede */
        if (!originalModel.isEmpty() && originalModel != m_ai->model())
            m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), originalModel);
    });
    connect(m_ai, &AiClient::error, holder,
            [this, holder, originalModel](const QString& msg){
        holder->deleteLater();
        m_aiMode = false;
        setRunning(false);
        m_aiOutput->appendPlainText(QString("\n\xe2\x9d\x8c  Errore: %1").arg(msg));
        m_status->setText("\xe2\x9d\x8c  Errore AI durante la correzione.");
        /* Ripristina il modello originale anche in caso di errore */
        if (!originalModel.isEmpty() && originalModel != m_ai->model())
            m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), originalModel);
    });

    m_ai->chat(sys, user);
}
