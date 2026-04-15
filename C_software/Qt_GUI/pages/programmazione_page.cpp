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
#include <QSet>
#include <QSettings>
#include <QMessageBox>
#include <QCheckBox>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTextEdit>
#include <QTimer>
#include "../widgets/toggle_switch.h"

namespace P = PrismaluxPaths;

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

    /* ── Inner tab widget: Programmazione | Agentica ── */
    m_innerTabs = new QTabWidget(this);
    m_innerTabs->setObjectName("innerTabs");
    mainLay->addWidget(m_innerTabs, 1);

    auto* codingTab = new QWidget(m_innerTabs);
    auto* codingLay = new QVBoxLayout(codingTab);
    codingLay->setContentsMargins(0, 8, 0, 0);
    codingLay->setSpacing(8);

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

    auto tagExecP = [](QPushButton* btn, const char* icon, const char* text){
        btn->setProperty("execFull", btn->text());
        btn->setProperty("execIcon", QString::fromUtf8(icon));
        btn->setProperty("execText", QString::fromUtf8(text));
    };

    m_btnRun = new QPushButton("\xe2\x96\xb6  Esegui", toolRow);
    m_btnRun->setObjectName("actionBtn");
    m_btnRun->setToolTip("Esegui il codice nell'editor (F5)");
    tagExecP(m_btnRun, "\xe2\x96\xb6", "Esegui");

    m_btnStop = new QPushButton("\xe2\x96\xa0  Stop", toolRow);
    m_btnStop->setObjectName("actionBtn");
    m_btnStop->setProperty("danger", "true");
    m_btnStop->setEnabled(false);
    tagExecP(m_btnStop, "\xe2\x96\xa0", "Stop");

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
    tagExecP(m_btnAi, "\xf0\x9f\xa4\x96", "Chiedi all'AI");
    toolLay->addWidget(m_btnAi);

    m_btnFix = new QPushButton("\xf0\x9f\x94\xa7  Correggi con AI", toolRow);
    m_btnFix->setObjectName("actionBtn");
    m_btnFix->setProperty("highlight", "true");
    tagExecP(m_btnFix, "\xf0\x9f\x94\xa7", "Correggi con AI");
    m_btnFix->setToolTip(
        "Invia il codice a qwen2.5-coder (o il miglior modello coder disponibile)\n"
        "e chiedi di trovare e correggere tutti i bug.\n"
        "Se c'era un errore di esecuzione, viene incluso nel contesto.");
    toolLay->addWidget(m_btnFix);

    /* ── Toggle "Loop Fix" ── */
    toolLay->addSpacing(10);
    m_toggleAutoFix = new ToggleSwitch("Loop Fix", toolRow);
    m_toggleAutoFix->setToolTip(
        "ON  \xe2\x86\x92 esegue in loop: errore \xe2\x86\x92 AI corregge \xe2\x86\x92 riesegue, "
        "fino a successo (max 6 tentativi).\n"
        "Si ferma se trova un SyntaxError creato deliberatamente (raise SyntaxError).\n"
        "OFF \xe2\x86\x92 esecuzione singola, correzione manuale.");
    /* Toggle: warning one-shot alla prima attivazione, reset loop allo spegnimento */
    connect(m_toggleAutoFix, &QAbstractButton::toggled, this, [this](bool on){
        if (!on) {
            /* Spento: ferma il loop in corso */
            m_loopActive = false;
            m_loopCount  = 0;
            return;
        }

        /* Attivato: mostra warning una sola volta (salvato in QSettings) */
        QSettings s("Prismalux", "GUI");
        if (!s.value(P::SK::kLoopFixWarning, false).toBool()) {
            QMessageBox dlg(this);
            dlg.setWindowTitle("Loop Fix — Esecuzione automatica di codice AI");
            dlg.setIcon(QMessageBox::Warning);
            dlg.setText(
                "<b>Loop Fix eseguir\xc3\xa0 automaticamente il codice</b><br>"
                "generato dall'AI con i tuoi privilegi utente.<br><br>"
                "Assicurati di usare questa funzione <b>solo con AI di cui ti fidi</b> "
                "e con codice che non accede a dati sensibili o al filesystem.<br><br>"
                "Il loop si ferma automaticamente dopo 6 tentativi o se rileva "
                "un errore intenzionale (SyntaxError custom).");
            dlg.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
            dlg.button(QMessageBox::Ok)->setText("Ho capito, abilita");
            dlg.button(QMessageBox::Cancel)->setText("Annulla");

            /* Checkbox "Non mostrare più" */
            auto* chk = new QCheckBox("Non mostrare più questo avviso", &dlg);
            dlg.setCheckBox(chk);

            if (dlg.exec() != QMessageBox::Ok) {
                /* Utente ha annullato: disattiva il toggle senza ri-emettere toggled */
                m_toggleAutoFix->blockSignals(true);
                m_toggleAutoFix->setChecked(false);
                m_toggleAutoFix->blockSignals(false);
                return;
            }
            if (chk->isChecked())
                s.setValue(P::SK::kLoopFixWarning, true);
        }
    });
    toolLay->addWidget(m_toggleAutoFix);

    codingLay->addWidget(toolRow);

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

    /* ── Syntax Highlighter — attivato subito sull'editor ── */
    m_highlighter = new CodeHighlighter(m_editor->document());
    m_highlighter->setLanguage(CodeHighlighter::Python);  /* default */
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
    m_chartGroup = chartGroup;

    mainSplit->addWidget(rightCol);
    mainSplit->setSizes({500, 450});
    codingLay->addWidget(mainSplit, 1);

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

    m_btnSend = new QPushButton("Invia \xe2\x96\xb6", aiInputRow);
    m_btnSend->setObjectName("actionBtn");
    tagExecP(m_btnSend, "\xe2\x96\xb6", "Invia");
    auto* btnSend = m_btnSend;  /* alias locale per il codice sottostante */

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
    codingLay->addWidget(m_aiPanel);

    /* ── Registra il tab Coding e aggiungi il tab Agentica ── */
    m_innerTabs->addTab(codingTab,
        "\xf0\x9f\x92\xbb  Programmazione");
    m_innerTabs->addTab(buildAgentica(m_innerTabs),
        "\xf0\x9f\xa4\x96  Agentica");

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

    /* Score di preferenza per coding: piu' alto = meglio per programmazione */
    auto codingScore = [](const QString& name) -> int {
        const QString n = name.toLower();
        if (n.contains("qwen2.5-coder") || n.contains("qwen2_5-coder")) {
            if (n.contains("32b")) return 100;
            if (n.contains("14b")) return 95;
            if (n.contains("7b"))  return 90;
            return 85;
        }
        if (n.contains("deepseek-coder")) {
            if (n.contains("v2") || n.contains("33b")) return 88;
            if (n.contains("7b") || n.contains("6.7b")) return 82;
            return 78;
        }
        if (n.contains("coder") || n.contains("codellama") ||
            n.contains("starcoder") || n.contains("codegemma")) return 70;
        if (n.contains("deepseek") && !n.contains("math")) return 40;
        if (n.contains("qwen")     && !n.contains("math")) return 35;
        if (n.contains("phi"))                              return 30;
        return 0;
    };

    /* Lambda: recupera modelli, popola la combo, auto-seleziona il miglior coder */
    auto populateAiModels = [this, codingBadge, isEmbedOnly, codingScore]() {
        if (!m_ai) return;
        m_modelCombo->setEnabled(false);
        const QString cur = m_ai->model();
        auto* holder = new QObject(this);
        connect(m_ai, &AiClient::modelsReady, holder,
                [this, holder, codingBadge, isEmbedOnly, codingScore, cur]
                (const QStringList& list) {
            holder->deleteLater();
            m_modelCombo->clear();

            int selIdx    = 0;
            int curIdx    = -1;
            int bestCoder = -1;
            int bestScore = -1;

            for (const QString& mdl : list) {
                if (isEmbedOnly(mdl)) continue;
                const int pos = m_modelCombo->count();
                m_modelCombo->addItem(mdl + codingBadge(mdl), mdl);
                if (mdl == cur) curIdx = pos;
                const int sc = codingScore(mdl);
                if (sc > bestScore) { bestScore = sc; bestCoder = pos; }
            }

            if (m_modelCombo->count() == 0) {
                m_modelCombo->addItem(
                    cur.isEmpty() ? "(nessun modello chat)" : cur, cur);
            } else {
                /* Auto-seleziona il miglior coder se il modello corrente non è già
               un coder SPECIALIZZATO (≥85 = qwen2.5-coder, deepseek-coder).
               "Buono per Coding" (<85) NON blocca la selezione ottimale. */
                const bool curIsCoder = (curIdx >= 0 && codingScore(cur) >= 85);
                if (bestCoder >= 0 && !curIsCoder)
                    selIdx = bestCoder;
                else if (curIdx >= 0)
                    selIdx = curIdx;
                m_modelCombo->setCurrentIndex(selIdx);

                /* Imposta il modello scelto sul backend */
                const QString chosen = m_modelCombo->currentData().toString();
                if (!chosen.isEmpty() && chosen != cur)
                    m_ai->setBackend(m_ai->backend(), m_ai->host(),
                                     m_ai->port(), chosen);
            }
            m_modelCombo->setEnabled(true);
        });
        m_ai->fetchModels();
    };

    connect(btnRefreshMod, &QPushButton::clicked, this, populateAiModels);

    /* Auto-popola modelli al caricamento della pagina: seleziona "Ottimizzato Coding" subito */
    QTimer::singleShot(0, this, [populateAiModels]{ populateAiModels(); });

    /* ══════════════════════════════════════════════════════════
       Connessioni
       ══════════════════════════════════════════════════════════ */

    /* Cambia linguaggio → sostituisce sempre il codice nell'editor con il template */
    /* Mappa indice combo → CodeHighlighter::Language */
    static const CodeHighlighter::Language kLangMap[] = {
        CodeHighlighter::Python,
        CodeHighlighter::C,
        CodeHighlighter::Cpp,
        CodeHighlighter::Bash,
        CodeHighlighter::JavaScript,
    };

    connect(m_lang, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx){
        m_editor->setPlainText(currentTemplate());
        if (m_highlighter && idx >= 0 && idx < 5)
            m_highlighter->setLanguage(kLangMap[idx]);
    });

    /* Template iniziale */
    m_editor->setPlainText(currentTemplate());

    /* Pulisci output */
    connect(btnClear, &QPushButton::clicked, this, [this]{
        m_output->clear();
        m_fullOutput.clear();
        m_chart->clearAll();
        m_chartGroup->hide();
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

        /* Resetta il holder precedente prima di crearne uno nuovo —
           evita doppie connessioni che causano output duplicato. */
        if (m_tokenHolder) { delete m_tokenHolder; m_tokenHolder = nullptr; }
        m_tokenHolder = new QObject(this);
        connect(m_ai, &AiClient::token, m_tokenHolder, [this](const QString& tok){
            m_aiOutput->moveCursor(QTextCursor::End);
            m_aiOutput->insertPlainText(tok);
            m_aiOutput->ensureCursorVisible();
        });
        connect(m_ai, &AiClient::finished, m_tokenHolder, [this](const QString&){
            if (m_tokenHolder) { delete m_tokenHolder; m_tokenHolder = nullptr; }
            m_aiMode = false;
            setRunning(false);
            m_status->setText("\xe2\x9c\x85  Risposta AI completata.");
            m_btnInsert->setEnabled(!extractCodeBlock().isEmpty());
        });
        connect(m_ai, &AiClient::error, m_tokenHolder, [this](const QString& msg){
            if (m_tokenHolder) { delete m_tokenHolder; m_tokenHolder = nullptr; }
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
    connect(m_btnRun, &QPushButton::clicked, this, [this]{ runCode(); });

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

    /* Sincronizza i combo modello (Coding + Agentica) quando il modello cambia
       da Impostazioni o da qualsiasi altra scheda.
       Fonte unica: segnale AiClient::modelChanged.
       Cerca prima per data (nome esatto), poi per testo (contiene). */
    auto syncCombo = [](QComboBox* combo, const QString& newModel) {
        if (!combo) return;
        int idx = combo->findData(newModel);
        if (idx < 0) idx = combo->findText(newModel, Qt::MatchContains);
        if (idx >= 0 && idx != combo->currentIndex()) {
            combo->blockSignals(true);
            combo->setCurrentIndex(idx);
            combo->blockSignals(false);
        } else if (idx < 0) {
            /* Modello non ancora nella lista: aggiorna la voce iniziale */
            combo->blockSignals(true);
            combo->setItemText(0, newModel);
            combo->setItemData(0, newModel);
            combo->setCurrentIndex(0);
            combo->blockSignals(false);
        }
    };

    connect(m_ai, &AiClient::modelChanged, this, [this, syncCombo](const QString& newModel) {
        syncCombo(m_modelCombo,  newModel);   /* tab Coding */
        syncCombo(m_agentModel,  newModel);   /* tab Agentica */
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
    m_btnRun->setEnabled(!running);
    m_btnStop->setEnabled(running);
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

    connect(m_proc, &QProcess::readyRead, this, [this]{
        const QString out = QString::fromLocal8Bit(m_proc->readAll());
        m_fullOutput += out;
        appendOutput(out);
    });

    connect(m_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, filePath](int code, QProcess::ExitStatus){
        m_lastExitCode = code;
        if (code == 0) {
            m_lastError.clear();
            if (m_loopActive) {
                m_status->setText(
                    QString("\xe2\x9c\x85  Risolto in %1 tentativo/i!  \xc2\xb7  %2")
                        .arg(m_loopCount)
                        .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
                m_loopActive = false;
                m_loopCount  = 0;
            } else {
                m_status->setText(QString("\xe2\x9c\x85  Completato  \xc2\xb7  %1")
                                  .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
            }
            m_btnFix->setText("\xf0\x9f\x94\xa7  Correggi con AI");
        } else {
            const QStringList lines = m_fullOutput.split('\n');
            int start = qMax(0, lines.size() - 20);
            m_lastError = lines.mid(start).join('\n').trimmed();

            const QString sourceNow = m_editor->toPlainText();

            if (m_toggleAutoFix && m_toggleAutoFix->isChecked()) {
                if (isIntentionalError(m_lastError, sourceNow)) {
                    m_loopActive = false;
                    m_loopCount  = 0;
                    m_status->setText(
                        "\xf0\x9f\x9b\x91  Errore intenzionale rilevato (SyntaxError custom) "
                        "\xe2\x80\x94 loop terminato. Il codice \xc3\xa8 come vuoi tu.");
                } else if (m_loopCount >= kLoopMax) {
                    m_loopActive = false;
                    m_loopCount  = 0;
                    m_status->setText(
                        QString("\xe2\x9a\xa0  Limite iterazioni raggiunto (%1 tentativi) "
                                "\xe2\x80\x94 loop terminato.").arg(kLoopMax));
                } else {
                    m_loopActive = true;
                    m_loopCount++;
                    m_status->setText(
                        QString("\xf0\x9f\x94\x84  Tentativo %1/%2 \xe2\x80\x94 "
                                "AI sta correggendo...")
                            .arg(m_loopCount).arg(kLoopMax));
                    m_btnFix->setText("\xf0\x9f\x94\xa7  Correggi errore");
                    QTimer::singleShot(400, this, [this]{ triggerFix(true); });
                }
            } else {
                m_status->setText(
                    QString("\xe2\x9d\x8c  Errore (exit %1)  \xc2\xb7  "
                            "Premi \xf0\x9f\x94\xa7 per correggere con AI").arg(code));
                m_btnFix->setText("\xf0\x9f\x94\xa7  Correggi errore");
            }
        }
        setRunning(false);
        m_proc = nullptr;
        const QString ext2 = m_lang->currentData().toString();
        if (ext2 != "c" && ext2 != "cpp") QFile::remove(filePath);
        tryShowChart();
        if (m_chartGroup)
            m_chartGroup->setVisible(!m_chart->sizeHint().isEmpty()
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

    if (m_tokenHolder) { delete m_tokenHolder; m_tokenHolder = nullptr; }
    m_tokenHolder = new QObject(this);
    connect(m_ai, &AiClient::token, m_tokenHolder, [this](const QString& tok){
        m_aiOutput->moveCursor(QTextCursor::End);
        m_aiOutput->insertPlainText(tok);
        m_aiOutput->ensureCursorVisible();
    });
    connect(m_ai, &AiClient::finished, m_tokenHolder,
            [this, originalModel](const QString&){
        if (m_tokenHolder) { delete m_tokenHolder; m_tokenHolder = nullptr; }
        m_aiMode = false;
        setRunning(false);
        m_btnFix->setText("\xf0\x9f\x94\xa7  Correggi con AI");
        const QString fixed = extractCodeBlock();
        if (!fixed.isEmpty()) {
            m_editor->setPlainText(fixed);
            m_lastExitCode = 0;
            m_lastError.clear();
            if (m_loopActive && m_toggleAutoFix && m_toggleAutoFix->isChecked()) {
                /* Loop attivo: ri-esegui il codice corretto */
                m_status->setText(
                    QString("\xf0\x9f\x94\x84  Codice corretto \xe2\x80\x94 "
                            "ri-esecuzione [tentativo %1/%2]...")
                        .arg(m_loopCount).arg(kLoopMax));
                QTimer::singleShot(600, this, [this]{ runCode(); });
            } else {
                m_status->setText("\xe2\x9c\x85  Codice corretto inserito automaticamente nell'editor.");
            }
        } else {
            m_loopActive = false;  /* nessun codice estratto — ferma il loop */
            m_status->setText("\xe2\x9c\x85  Correzione completata (nessun blocco codice estratto).");
        }
        m_btnInsert->setEnabled(false);
        if (!originalModel.isEmpty() && originalModel != m_ai->model())
            m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), originalModel);
    });
    connect(m_ai, &AiClient::error, m_tokenHolder,
            [this, originalModel](const QString& msg){
        if (m_tokenHolder) { delete m_tokenHolder; m_tokenHolder = nullptr; }
        m_aiMode = false;
        setRunning(false);
        m_aiOutput->appendPlainText(QString("\n\xe2\x9d\x8c  Errore: %1").arg(msg));
        m_status->setText("\xe2\x9d\x8c  Errore AI durante la correzione.");
        if (!originalModel.isEmpty() && originalModel != m_ai->model())
            m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), originalModel);
    });

    m_ai->chat(sys, user);
}

/* ══════════════════════════════════════════════════════════════
   buildAgentica — sub-tab "🤖 Agentica"
   Genera template di codice per sistemi agentici: pipeline,
   RAG, refactoring AI, test generation, debugging multi-step.
   ══════════════════════════════════════════════════════════════ */
QWidget* ProgrammazionePage::buildAgentica(QWidget* parent)
{
    QFont monoFont;
    monoFont.setFamily("JetBrains Mono");
    monoFont.setStyleHint(QFont::Monospace);
    monoFont.setPointSize(11);

    auto* w   = new QWidget(parent);
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(12, 12, 12, 12);
    lay->setSpacing(10);

    /* ── Descrizione ── */
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

    /* ── Riga toolbar: Tipo agente + Linguaggio ── */
    auto* toolRow = new QWidget(w);
    auto* toolLay = new QHBoxLayout(toolRow);
    toolLay->setContentsMargins(0, 0, 0, 0);
    toolLay->setSpacing(8);

    toolLay->addWidget(new QLabel("Tipo agente:", toolRow));
    m_agentType = new QComboBox(toolRow);
    m_agentType->setObjectName("settingCombo");
    m_agentType->addItem("Pipeline codice (3 agenti: Analisi \xe2\x86\x92 Impl \xe2\x86\x92 Test)",
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

    lay->addWidget(toolRow);

    /* ── Task description ── */
    auto* taskGroup = new QGroupBox(
        "\xf0\x9f\x93\x9d  Descrizione del task (cosa vuoi costruire?)", w);
    taskGroup->setObjectName("cardGroup");
    auto* taskLay = new QVBoxLayout(taskGroup);
    taskLay->setContentsMargins(4, 8, 4, 4);

    m_agentTask = new QPlainTextEdit(taskGroup);
    m_agentTask->setObjectName("codeEditor");
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
    lay->addWidget(taskGroup);

    /* ── Selezione modello ── */
    auto* modelRow = new QWidget(w);
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
    lay->addWidget(modelRow);

    /* ── Output agente ── */
    auto* outGroup = new QGroupBox("\xf0\x9f\xa4\x96  Output agente (streaming)", w);
    outGroup->setObjectName("cardGroup");
    auto* outLay  = new QVBoxLayout(outGroup);
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

    /* Pulsanti sotto l'output */
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
    lay->addWidget(outGroup, 1);

    /* ══════════════════════════════════════════════════════════
       Connessioni — Agentica
       ══════════════════════════════════════════════════════════ */

    /* Popola modelli per il tab Agentica */
    auto populateAgentModels = [this]() {
        if (!m_ai) return;
        m_agentModel->setEnabled(false);
        const QString cur = m_ai->model();
        auto* holder = new QObject(this);
        connect(m_ai, &AiClient::modelsReady, holder,
                [this, holder, cur](const QStringList& list) {
            holder->deleteLater();
            m_agentModel->clear();
            bool foundCur = false;
            for (const QString& mdl : list) {
                const QString n = mdl.toLower();
                const bool isEmbed = n.contains("embed") || n.contains("minilm") ||
                                     n.contains("rerank") || n.contains("bge-");
                if (isEmbed) continue;
                m_agentModel->addItem(mdl, mdl);
                if (mdl == cur) foundCur = true;
            }
            if (m_agentModel->count() == 0)
                m_agentModel->addItem(cur.isEmpty() ? "(nessun modello)" : cur, cur);
            else if (foundCur)
                m_agentModel->setCurrentText(cur);
            m_agentModel->setEnabled(true);
        });
        m_ai->fetchModels();
    };

    connect(btnRefAgent, &QPushButton::clicked, this, populateAgentModels);
    QTimer::singleShot(0, this, [populateAgentModels]{ populateAgentModels(); });

    /* Genera */
    connect(m_btnAgentRun, &QPushButton::clicked, this, [this]{ runAgente(); });

    /* Stop */
    connect(m_btnAgentStop, &QPushButton::clicked, this, [this]{
        if (m_ai && m_ai->busy()) m_ai->abort();
    });

    /* Pulisci */
    connect(btnClearAgent, &QPushButton::clicked, this, [this]{
        m_agentOutput->clear();
        m_btnAgentInsert->setEnabled(false);
    });

    /* Apri in editor Programmazione */
    connect(m_btnAgentInsert, &QPushButton::clicked, this, [this]{
        /* Estrai il primo blocco codice dall'output */
        const QString text = m_agentOutput->toPlainText();
        static const QRegularExpression reFence(
            R"(```[a-zA-Z]*\n([\s\S]*?)```)",
            QRegularExpression::MultilineOption);
        const auto match = reFence.match(text);
        const QString code = match.hasMatch()
                             ? match.captured(1)
                             : text;
        if (code.trimmed().isEmpty()) return;
        m_editor->setPlainText(code.trimmed());
        /* Torna al tab Programmazione (indice 0) */
        if (m_innerTabs) m_innerTabs->setCurrentIndex(0);
    });

    return w;
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

    if (m_agentTokenHolder) { delete m_agentTokenHolder; m_agentTokenHolder = nullptr; }
    m_agentTokenHolder = new QObject(this);

    connect(m_ai, &AiClient::token, m_agentTokenHolder, [this](const QString& tok){
        m_agentOutput->moveCursor(QTextCursor::End);
        m_agentOutput->insertPlainText(tok);
        m_agentOutput->ensureCursorVisible();
    });
    connect(m_ai, &AiClient::finished, m_agentTokenHolder, [this](const QString&){
        if (m_agentTokenHolder) { delete m_agentTokenHolder; m_agentTokenHolder = nullptr; }
        m_btnAgentRun->setEnabled(true);
        m_btnAgentStop->setEnabled(false);
        /* Abilita "Apri in editor" solo se c'è un blocco codice */
        const QString text = m_agentOutput->toPlainText();
        m_btnAgentInsert->setEnabled(text.contains("```"));
    });
    connect(m_ai, &AiClient::error, m_agentTokenHolder, [this](const QString& msg){
        if (m_agentTokenHolder) { delete m_agentTokenHolder; m_agentTokenHolder = nullptr; }
        m_btnAgentRun->setEnabled(true);
        m_btnAgentStop->setEnabled(false);
        m_agentOutput->moveCursor(QTextCursor::End);
        m_agentOutput->insertPlainText(QString("\n\xe2\x9d\x8c  Errore: %1").arg(msg));
    });

    m_ai->chat(sys, user);
}
