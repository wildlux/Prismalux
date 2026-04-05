#include "strumenti_page.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QButtonGroup>
#include <QStackedWidget>
#include <QFileDialog>
#include <QProcess>
#include <QFileInfo>

/* ══════════════════════════════════════════════════════════════
   Tabella system prompt: [navIdx][subIdx]
   ══════════════════════════════════════════════════════════════ */
static const char* kSysPrompts[6][8] = {
    /* 0 — Studio */
    {
        "Sei un tutor esperto. Spiega in modo chiaro e strutturato con esempi pratici. Rispondi in italiano.",
        "Crea 10 flashcard nel formato Q: ... R: ... dal testo fornito. Rispondi in italiano.",
        "Crea un riassunto breve del testo. Rispondi in italiano.",
        "Genera 10 domande d'esame probabili con risposta sintetica. Rispondi in italiano.",
        "Crea una mappa concettuale in formato albero ASCII del concetto o testo fornito. Rispondi in italiano.",
        "Crea 5 esercizi pratici progressivi con soluzione. Rispondi in italiano.",
        nullptr, nullptr
    },
    /* 1 — Scrittura Creativa */
    {
        "Sei uno scrittore creativo. Scrivi una storia coinvolgente basata sull'idea fornita. Rispondi in italiano.",
        "Continua la storia in modo coerente con lo stile e i personaggi esistenti. Rispondi in italiano.",
        "Crea un personaggio dettagliato: nome, aspetto, backstory, motivazioni, difetti e punti di forza. Rispondi in italiano.",
        "Scrivi una poesia originale sul tema indicato. Rispondi in italiano.",
        "Scrivi un dialogo naturale e coinvolgente tra personaggi in base alla scena descritta. Rispondi in italiano.",
        "Sviluppa una trama in 3 atti: setup, confronto, risoluzione. Rispondi in italiano.",
        nullptr, nullptr
    },
    /* 2 — Ricerca */
    {
        "Fai una ricerca approfondita sull'argomento. Struttura: panoramica, punti chiave, dettagli, fonti consigliate. Rispondi in italiano.",
        "Confronta le opzioni indicate con una tabella pro/contro per ciascuna. Rispondi in italiano.",
        "Verifica la veridicita' dell'affermazione. Indica: VERO/FALSO/PARZIALMENTE VERO con spiegazione. Rispondi in italiano.",
        "Genera una bibliografia in formato APA sull'argomento con 5-10 fonti plausibili. Rispondi in italiano.",
        "Analizza il problema da almeno 4 prospettive diverse (economica, sociale, tecnica, etica). Rispondi in italiano.",
        "Crea una guida passo-passo dettagliata. Rispondi in italiano.",
        nullptr, nullptr
    },
    /* 3 — Libri */
    {
        "Analizza il testo: temi principali, stile, struttura narrativa, simbolismi. Rispondi in italiano.",
        "Crea un riassunto strutturato per capitoli o sezioni del testo. Rispondi in italiano.",
        "Analizza i personaggi principali: caratterizzazione, archi narrativi, relazioni. Rispondi in italiano.",
        "Crea una scheda libro completa: titolo, autore, anno, genere, trama, temi, punti di forza/debolezza, voto /10. Rispondi in italiano.",
        "Genera 8 domande aperte per una discussione critica del libro. Rispondi in italiano.",
        "Suggerisci 5 libri simili per temi e stile, con breve motivazione. Rispondi in italiano.",
        nullptr, nullptr
    },
    /* 4 — Produttivita' */
    {
        "Crea un piano progetto dettagliato: obiettivi, WBS, milestone, rischi, risorse. Rispondi in italiano.",
        "Organizza i task in Must/Should/Could/Won't (MoSCoW) con priorita' e tempi stimati. Rispondi in italiano.",
        "Scrivi un'email professionale chiara e convincente basata sul briefing fornito. Rispondi in italiano.",
        "Crea un'agenda dettagliata con punti di discussione, tempi e obiettivi per la riunione. Rispondi in italiano.",
        "Fai un brainstorming usando la tecnica dei 6 cappelli di de Bono sull'argomento. Rispondi in italiano.",
        "Trasforma l'obiettivo vago in 3-5 obiettivi SMART. Rispondi in italiano.",
        "Crea una matrice decisionale con criteri ponderati per le opzioni indicate. Rispondi in italiano.",
        nullptr
    },
    /* 5 — Documenti PDF */
    {
        "Analizza il documento fornito: struttura, temi principali, argomenti chiave e conclusioni. Rispondi in italiano.",
        "Crea un riassunto conciso e chiaro del documento. Rispondi in italiano.",
        "Estrai le informazioni chiave: fatti, dati, date, nomi e numeri importanti. Rispondi in italiano.",
        "Rispondi alle domande poste usando SOLO le informazioni contenute nel documento. Rispondi in italiano.",
        "Identifica punti deboli, contraddizioni, lacune argomentative o affermazioni non supportate. Rispondi in italiano.",
        "Ricava una lista di punti di azione, raccomandazioni o prossimi passi dal documento. Rispondi in italiano.",
        nullptr, nullptr
    },
};

static const char* kSubActions[6][8] = {
    { "\xf0\x9f\x92\xa1 Spiega concetto",
      "\xf0\x9f\x83\x8f Flashcard Q&A",
      "\xf0\x9f\x93\x9d Riassunto",
      "\xe2\x9d\x93 Domande d'esame",
      "\xf0\x9f\x97\xba Mappa concettuale",
      "\xf0\x9f\x8f\x8b Esercizi pratici",
      nullptr, nullptr },
    { "\xf0\x9f\x93\x96 Genera storia",
      "\xe2\x9e\xa1 Continua storia",
      "\xf0\x9f\xa7\x99 Crea personaggio",
      "\xf0\x9f\x8c\xb8 Scrivi poesia",
      "\xf0\x9f\x92\xac Genera dialogo",
      "\xf0\x9f\x8e\xac Sviluppa trama",
      nullptr, nullptr },
    { "\xf0\x9f\x94\x8d Ricerca approfondita",
      "\xe2\x9a\x96 Confronta opzioni",
      "\xe2\x9c\x85 Fact-checking",
      "\xf0\x9f\x93\x9a Genera bibliografia",
      "\xf0\x9f\x94\xad Analisi multi-prospettiva",
      "\xf0\x9f\x9b\xa4 Guida how-to",
      nullptr, nullptr },
    { "\xf0\x9f\x93\x9c Analisi letteraria",
      "\xf0\x9f\x93\x91 Riassunto capitoli",
      "\xf0\x9f\xa7\x91 Studio personaggi",
      "\xf0\x9f\x93\x8b Scheda libro",
      "\xf0\x9f\x92\xad Domande discussione",
      "\xf0\x9f\x93\x9a Consigli lettura",
      nullptr, nullptr },
    { "\xf0\x9f\x93\x8a Piano progetto",
      "\xf0\x9f\x93\x8c Lista MoSCoW",
      "\xe2\x9c\x89 Bozza email",
      "\xf0\x9f\x97\x93 Prepara riunione",
      "\xf0\x9f\xa7\xa0 Brainstorming",
      "\xf0\x9f\x8e\xaf Obiettivi SMART",
      "\xe2\x9a\x96 Matrice decisionale",
      nullptr },
    { "\xf0\x9f\x94\x8d Analisi documento",
      "\xf0\x9f\x93\x9d Riassunto",
      "\xf0\x9f\x94\x8e Estrai informazioni",
      "\xe2\x9d\x93 Q&A documento",
      "\xf0\x9f\x9a\xa8 Punti critici",
      "\xe2\x9c\x85 Punti di azione",
      nullptr, nullptr },
};

static const char* kPlaceholders[6] = {
    "Incolla il testo o descrivi il concetto da studiare...",
    "Descrivi l'idea, il personaggio o la scena...",
    "Inserisci l'argomento da ricercare o l'affermazione da verificare...",
    "Incolla il testo del libro o il titolo e l'autore...",
    "Descrivi il progetto, il task o l'obiettivo...",
    "Incolla il testo del documento oppure carica un PDF con il pulsante sopra...",
};

/* ══════════════════════════════════════════════════════════════
   Costruttore — layout immediato:
   [tab categorie]
   [griglia bottoni azione — visibili tutti, cliccabili direttamente]
   [✅ Azione selezionata]
   [area input]  [▶ Esegui / ⏹ Stop]
   [output AI]
   ══════════════════════════════════════════════════════════════ */
StrumentiPage::StrumentiPage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    /* Widget interni nascosti — usati dai slot tramite row/index */
    m_navList = new QListWidget(this);
    m_navList->hide();
    for (int i = 0; i < 6; i++) m_navList->addItem("");
    m_navList->setCurrentRow(0);

    m_cmbSub = new QComboBox(this);
    m_cmbSub->hide();
    for (int i = 0; i < 8; i++) m_cmbSub->addItem("");
    m_cmbSub->setCurrentIndex(0);

    /* Layout principale */
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(16, 10, 16, 10);
    lay->setSpacing(8);

    /* ── Barra categorie: tab orizzontali ── */
    static const char* kCatLabels[] = {
        "\xf0\x9f\x93\x9a  Studio",
        "\xe2\x9c\x8d\xef\xb8\x8f  Scrittura",
        "\xf0\x9f\x94\x8d  Ricerca",
        "\xf0\x9f\x93\x96  Libri",
        "\xe2\x9a\xa1  Produttivit\xc3\xa0",
        "\xf0\x9f\x93\x84  Documenti",
    };
    static const char* kCatStyle =
        "QPushButton{"
          "border:none;border-bottom:2px solid transparent;"
          "padding:7px 18px;background:transparent;"
          "color:#64748b;font-size:13px;font-weight:bold;}"
        "QPushButton:checked{"
          "border-bottom:2px solid #00a37f;color:#e2e8f0;}"
        "QPushButton:hover{color:#94a3b8;}";

    auto* catBar = new QWidget(this);
    auto* catLay = new QHBoxLayout(catBar);
    catLay->setContentsMargins(0, 0, 0, 0);
    catLay->setSpacing(0);

    auto* catGroup = new QButtonGroup(this);
    catGroup->setExclusive(true);

    /* Stack: una pagina per categoria con i bottoni azione */
    auto* actStack = new QStackedWidget(this);
    actStack->setMaximumHeight(180);

    static const char* kActStyle =
        "QPushButton{"
          "border:1px solid #334155;border-radius:8px;"
          "padding:8px 10px;background:#1a2032;"
          "color:#94a3b8;font-size:12px;text-align:left;}"
        "QPushButton:checked{"
          "background:#0e3d2e;border-color:#00a37f;"
          "color:#e2e8f0;font-weight:bold;}"
        "QPushButton:hover{background:#1e293b;color:#cbd5e1;}";

    /* Label azione corrente */
    auto* lblSel = new QLabel(this);
    lblSel->setObjectName("cardDesc");
    const QString firstAction = QString::fromUtf8(kSubActions[0][0]);
    lblSel->setText("\xe2\x9c\x85  <b>" + firstAction + "</b>");
    lblSel->setTextFormat(Qt::RichText);

    for (int cat = 0; cat < 6; cat++) {
        /* Tab categoria */
        auto* catBtn = new QPushButton(
            QString::fromUtf8(kCatLabels[cat]), catBar);
        catBtn->setCheckable(true);
        catBtn->setChecked(cat == 0);
        catBtn->setStyleSheet(kCatStyle);
        catGroup->addButton(catBtn, cat);
        catLay->addWidget(catBtn);

        /* Pagina azioni per questa categoria */
        auto* page = new QWidget;
        auto* grid = new QGridLayout(page);
        grid->setContentsMargins(0, 6, 0, 2);
        grid->setSpacing(8);

        auto* actGroup = new QButtonGroup(page);
        actGroup->setExclusive(true);

        int col = 0, row = 0;
        for (int act = 0; kSubActions[cat][act] != nullptr; act++) {
            auto* abtn = new QPushButton(
                QString::fromUtf8(kSubActions[cat][act]), page);
            abtn->setCheckable(true);
            abtn->setChecked(act == 0);
            abtn->setStyleSheet(kActStyle);
            actGroup->addButton(abtn, act);
            grid->addWidget(abtn, row, col);
            if (++col > 2) { col = 0; row++; }

            connect(abtn, &QPushButton::clicked, this,
                    [this, cat, act, lblSel]() {
                m_navList->setCurrentRow(cat);
                m_cmbSub->setCurrentIndex(act);
                m_inputArea->setPlaceholderText(
                    QString::fromUtf8(kPlaceholders[cat]));
                lblSel->setText(
                    "\xe2\x9c\x85  <b>" +
                    QString::fromUtf8(kSubActions[cat][act]) +
                    "</b>");
            });
        }
        for (int c = 0; c < 3; c++) grid->setColumnStretch(c, 1);
        actStack->addWidget(page);
    }
    catLay->addStretch();

    /* Cambio categoria */
    connect(catGroup, QOverload<int>::of(&QButtonGroup::idClicked),
            this, [this, actStack, lblSel](int cat) {
        actStack->setCurrentIndex(cat);
        m_navList->setCurrentRow(cat);
        m_cmbSub->setCurrentIndex(0);
        m_inputArea->setPlaceholderText(
            QString::fromUtf8(kPlaceholders[cat]));
        lblSel->setText(
            "\xe2\x9c\x85  <b>" +
            QString::fromUtf8(kSubActions[cat][0]) +
            "</b>");
        m_pdfRow->setVisible(cat == 5);
    });

    lay->addWidget(catBar);
    lay->addWidget(actStack);
    lay->addWidget(lblSel);

    /* ── Riga PDF picker (visibile solo per categoria Documenti) ── */
    m_pdfRow = new QWidget(this);
    auto* pdfLay = new QHBoxLayout(m_pdfRow);
    pdfLay->setContentsMargins(0, 4, 0, 0);
    pdfLay->setSpacing(8);

    auto* pdfBtn = new QPushButton("\xf0\x9f\x93\x84  Carica PDF", m_pdfRow);
    pdfBtn->setObjectName("actionBtn");
    pdfBtn->setFixedWidth(130);

    m_pdfPathLbl = new QLabel("Nessun PDF caricato", m_pdfRow);
    m_pdfPathLbl->setObjectName("hintLabel");
    m_pdfPathLbl->setWordWrap(false);

    pdfLay->addWidget(pdfBtn);
    pdfLay->addWidget(m_pdfPathLbl, 1);
    m_pdfRow->setVisible(false);
    lay->addWidget(m_pdfRow);

    connect(pdfBtn, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getOpenFileName(
            this, "Seleziona PDF", "",
            "PDF (*.pdf);;Tutti i file (*)");
        if (path.isEmpty()) return;
        m_pdfPath = path;
        m_pdfPathLbl->setText(
            QFileInfo(path).fileName());
    });

    /* ── Input + pulsanti affiancati ── */
    auto* inputRow = new QWidget(this);
    auto* inputLay = new QHBoxLayout(inputRow);
    inputLay->setContentsMargins(0, 0, 0, 0);
    inputLay->setSpacing(8);

    m_inputArea = new QTextEdit(inputRow);
    m_inputArea->setObjectName("chatInput");
    m_inputArea->setPlaceholderText(QString::fromUtf8(kPlaceholders[0]));
    m_inputArea->setMaximumHeight(90);
    m_inputArea->setMinimumHeight(60);
    inputLay->addWidget(m_inputArea, 1);

    auto* btnCol = new QVBoxLayout;
    btnCol->setSpacing(6);

    m_btnRun = new QPushButton("\xe2\x96\xb6  Esegui", inputRow);
    m_btnRun->setObjectName("actionBtn");
    m_btnRun->setFixedWidth(110);

    m_btnStop = new QPushButton("\xe2\x8f\xb9  Stop", inputRow);
    m_btnStop->setObjectName("actionBtn");
    m_btnStop->setProperty("danger", true);
    m_btnStop->setEnabled(false);
    m_btnStop->setFixedWidth(110);

    m_waitLbl = new QLabel(inputRow);
    m_waitLbl->setStyleSheet(
        "color:#E5C400;font-style:italic;font-size:11px;");
    m_waitLbl->setVisible(false);
    m_waitLbl->setWordWrap(true);

    btnCol->addWidget(m_btnRun);
    btnCol->addWidget(m_btnStop);
    btnCol->addWidget(m_waitLbl);
    btnCol->addStretch();
    inputLay->addLayout(btnCol);
    lay->addWidget(inputRow);

    /* ── Output AI ── */
    m_output = new QTextEdit(this);
    m_output->setObjectName("chatLog");
    m_output->setReadOnly(true);
    m_output->setPlaceholderText(
        "L'output dell'AI appare qui...\n\n"
        "\xf0\x9f\x8d\xba  Invocazione riuscita. Gli dei ascoltano.");
    lay->addWidget(m_output, 1);

    /* ── Avvia tool ── */
    connect(m_btnRun, &QPushButton::clicked, this, [this] {
        const int navIdx = m_navList->currentRow();
        const int subIdx = m_cmbSub->currentIndex();
        if (navIdx < 0 || subIdx < 0) return;

        QString userMsg = m_inputArea->toPlainText().trimmed();

        /* Categoria Documenti: estrae testo PDF se caricato */
        if (navIdx == 5 && !m_pdfPath.isEmpty()) {
            QProcess proc;
            proc.start("pdftotext", {m_pdfPath, "-"});
            if (!proc.waitForFinished(15000)) {
                m_output->append(
                    "\xe2\x9a\xa0  pdftotext non risponde. "
                    "Assicurati che poppler-utils sia installato "
                    "(sudo apt install poppler-utils).");
                return;
            }
            QString pdfText = QString::fromUtf8(
                proc.readAllStandardOutput()).trimmed();
            if (pdfText.isEmpty()) {
                m_output->append(
                    "\xe2\x9a\xa0  Impossibile estrarre testo dal PDF. "
                    "Il file potrebbe essere scansionato (immagine).");
                return;
            }
            /* Antepone il documento come contesto */
            userMsg = userMsg.isEmpty()
                ? "DOCUMENTO:\n" + pdfText
                : "DOCUMENTO:\n" + pdfText + "\n\nRICHIESTA:\n" + userMsg;
        }

        if (userMsg.isEmpty()) {
            m_output->append(
                "\xe2\x9a\xa0  Inserisci del testo oppure carica un PDF.");
            return;
        }
        const char* sys = kSysPrompts[navIdx][subIdx];
        if (!sys) return;
        runTool(QString::fromUtf8(sys), userMsg);
    });

    connect(m_btnStop, &QPushButton::clicked, m_ai, &AiClient::abort);

    connect(m_ai, &AiClient::token,    this, &StrumentiPage::onToken);
    connect(m_ai, &AiClient::finished, this, &StrumentiPage::onFinished);
    connect(m_ai, &AiClient::error,    this, &StrumentiPage::onError);
    connect(m_ai, &AiClient::aborted,  this, [this] {
        m_active = false;
        m_waitLbl->setVisible(false);
        m_btnRun->setEnabled(true);
        m_btnStop->setEnabled(false);
        m_output->append("\n\xe2\x8f\xb9  Interrotto.");
    });
}

/* ══════════════════════════════════════════════════════════════
   runTool
   ══════════════════════════════════════════════════════════════ */
void StrumentiPage::runTool(const QString& sys, const QString& userMsg) {
    if (m_ai->busy()) {
        m_output->append(
            "\xe2\x9a\xa0  Un'altra operazione e' in corso. Attendi.");
        return;
    }
    m_output->clear();
    m_btnRun->setEnabled(false);
    m_btnStop->setEnabled(true);
    m_waitLbl->setText("\xf0\x9f\x94\x84  Elaborazione in corso...");
    m_waitLbl->setVisible(true);
    m_active = true;
    m_ai->chat(sys, userMsg);
}

/* ══════════════════════════════════════════════════════════════
   Slot AI
   ══════════════════════════════════════════════════════════════ */
void StrumentiPage::onToken(const QString& t) {
    if (!m_active) return;
    m_waitLbl->setVisible(false);
    QTextCursor c(m_output->document());
    c.movePosition(QTextCursor::End);
    c.insertText(t);
    m_output->ensureCursorVisible();
}

void StrumentiPage::onFinished(const QString&) {
    if (!m_active) return;
    m_active = false;
    m_waitLbl->setVisible(false);
    m_btnRun->setEnabled(true);
    m_btnStop->setEnabled(false);
    m_output->append("\n" + QString(40, QChar(0x2500)));
}

void StrumentiPage::onError(const QString& msg) {
    if (!m_active) return;
    m_active = false;
    m_waitLbl->setVisible(false);
    m_btnRun->setEnabled(true);
    m_btnStop->setEnabled(false);
    m_output->append(
        QString("\n\xe2\x9d\x8c  Errore: %1").arg(msg));
    m_output->append(
        "\xf0\x9f\x92\xa1  Verifica la connessione al backend AI.");
}

/* Stub richiesti dall'header */
QWidget* StrumentiPage::buildSubPage(const QStringList&, const QString&) { return nullptr; }
QWidget* StrumentiPage::buildStudio()           { return nullptr; }
QWidget* StrumentiPage::buildScritturaCreativa(){ return nullptr; }
QWidget* StrumentiPage::buildRicerca()          { return nullptr; }
QWidget* StrumentiPage::buildLibri()            { return nullptr; }
QWidget* StrumentiPage::buildProduttivita()     { return nullptr; }
QString  StrumentiPage::sysPromptForAction(int, int) { return {}; }
