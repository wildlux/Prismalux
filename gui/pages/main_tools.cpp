#include "main_tools.h"
#include "main_tools_p.h"
#include "../dpi_utils.h"
#include "main_learn.h"
#include "main_tools_file.h"
#include "main_quiz.h"
#include "widget_ram_calculator.h"
#include "../prismalux_paths.h"
#include "../widgets/ai_error_widget.h"

#include <QScrollArea>
#include <QNetworkInterface>
#include <QHostAddress>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QButtonGroup>
#include <QStackedWidget>
#include <QCheckBox>
#include <QComboBox>

namespace P = PrismaluxPaths;

/* ══════════════════════════════════════════════════════════════
   Costruttore — stepdown: ogni metodo fa UNA cosa a UN livello.
   ══════════════════════════════════════════════════════════════ */
StrumentiPage::StrumentiPage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    setAcceptDrops(true);
    initHiddenWidgets();
    buildLayout();
    setupConnections();
    setupTabOrder();
}

/* ──────────────────────────────────────────────────────────────
   Livello 1 — initHiddenWidgets
   Crea m_navList e m_cmbSub nascosti (usati dai slot via row/index).
   ────────────────────────────────────────────────────────────── */
void StrumentiPage::initHiddenWidgets()
{
    m_navList = new QListWidget(this);
    m_navList->hide();
    for (int i = 0; i < 6; i++) m_navList->addItem("");
    m_navList->setCurrentRow(0);

    m_cmbSub = new QComboBox(this);
    m_cmbSub->hide();
    for (int i = 0; i < 6; i++) m_cmbSub->addItem("");
    m_cmbSub->setCurrentIndex(0);
}

/* ──────────────────────────────────────────────────────────────
   Livello 1 — buildLayout
   Crea il QVBoxLayout principale e assembla tutte le sezioni.
   Tab 0-5: una per categoria (griglia azioni); Tab 6: Cron;
   Tab 7-9: Finanza / Impara / Sfida.
   L'area I/O (lblSel + righe speciali + input + output) è un
   widget condiviso sotto i tab, visibile solo per i tab 0-5.
   ────────────────────────────────────────────────────────────── */
void StrumentiPage::buildLayout()
{
    auto* rootLay = new QVBoxLayout(this);
    rootLay->setContentsMargins(0, 0, 0, 0);
    rootLay->setSpacing(0);

    m_tabs = new QTabWidget(this);

    /* ── Tab 0-5: una griglia azioni per ogni categoria ── */
    static const char* kCatTabLabels[] = {
        "\xf0\x9f\x93\x9a Studio",
        "\xe2\x9c\x8d\xef\xb8\x8f Scrittura",
        "\xf0\x9f\x94\x8d Ricerca",
        "\xf0\x9f\x93\x96 Libri",
        "\xe2\x9a\xa1 Produttivit\xc3\xa0",
        "\xf0\x9f\x93\x84 Documenti",
    };

    for (int cat = 0; cat < 6; cat++) {
        auto* page = new QWidget(m_tabs);
        auto* grid = new QGridLayout(page);
        grid->setContentsMargins(8, 8, 8, 8);
        grid->setSpacing(8);

        auto* actGroup = new QButtonGroup(page);
        actGroup->setExclusive(true);

        int col = 0, row = 0;
        for (int act = 0; kSubActions[cat][act] != nullptr; act++) {
            auto* abtn = new QPushButton(
                QString::fromUtf8(kSubActions[cat][act]), page);
            abtn->setCheckable(true);
            abtn->setChecked(act == 0);
            abtn->setObjectName("strActBtn");
            actGroup->addButton(abtn, act);
            grid->addWidget(abtn, row, col);
            if (++col > 2) { col = 0; row++; }
            abtn->setProperty("strCat", cat);
            abtn->setProperty("strAct", act);
            connect(abtn, &QPushButton::clicked,
                    this, &StrumentiPage::onActBtnClicked);
        }
        for (int c = 0; c < 3; c++) grid->setColumnStretch(c, 1);
        m_tabs->addTab(page, QString::fromUtf8(kCatTabLabels[cat]));
    }

    /* ── Tab 6: Cron (lazy-init via cronPanelFirstOpen) ── */
    buildCronPanel();
    m_tabs->addTab(m_cronPanel, "\xe2\x8f\xb1 Cron");

    /* Finanza spostato in tab Utility */
    /* Fotovoltaico/Idroponica spostati in tab Utility */

    /* ── Tab 7: Impara con AI ── */
    m_tabs->addTab(new ImparaPage(m_ai, m_tabs),
                   "\xf0\x9f\x8f\x9b  Impara con AI");

    /* ── Tab 8: Sfida — AiClient separato per evitare cross-talk ── */
    m_quizAi = new AiClient(this);
    m_quizAi->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), m_ai->model());
    connect(m_ai, &AiClient::modelsReady,
            this, &StrumentiPage::onQuizAiModelsReady);
    m_tabs->addTab(new QuizPage(m_quizAi, m_tabs),
                   "\xf0\x9f\x8e\xaf  Sfida!");

    /* ── Tab 9: File AI (spostato da tab principale a sub-tab di Strumenti) ── */
    m_tabs->addTab(new StrumentiFilePage(m_ai, m_tabs),
                   "\xf0\x9f\x93\x81  File AI"); /* 📁 */

    /* ── Tab 10: Calcolatore RAM per LLM locali ── */
    m_tabs->addTab(new RamCalculatorWidget(m_tabs),
                   "\xf0\x9f\xa7\xae  RAM LLM"); /* 🧮 */

    /* rootLay[0] = m_tabs (stretch 0 per tab categoria, 1 per le altre) */
    rootLay->addWidget(m_tabs, 0);

    /* ── Area I/O condivisa (sotto i tab, visibile solo per tab 0-5) ── */
    m_sharedIoArea = new QWidget(this);
    auto* ioLay = new QVBoxLayout(m_sharedIoArea);
    ioLay->setContentsMargins(16, 8, 16, 8);
    ioLay->setSpacing(8);

    m_lblSel = new QLabel(m_sharedIoArea);
    m_lblSel->setObjectName("cardDesc");
    m_lblSel->setText("\xe2\x9c\x85  <b>" +
        QString::fromUtf8(kSubActions[0][0]) + "</b>");
    m_lblSel->setTextFormat(Qt::RichText);
    ioLay->addWidget(m_lblSel);

    buildSpecialRows(ioLay);
    ioLay->addWidget(buildCodeModelRow());
    ioLay->addWidget(buildInputRow());
    m_errPanel = new AiErrorWidget(m_sharedIoArea);
    ioLay->addWidget(m_errPanel);
    ioLay->addWidget(buildOutputArea(), 1);

    /* rootLay[1] = m_sharedIoArea (stretch 1 per tab categoria, 0 per le altre) */
    rootLay->addWidget(m_sharedIoArea, 1);

    connect(m_tabs, &QTabWidget::currentChanged,
            this, &StrumentiPage::onCatTabChanged);
    onCatTabChanged(0);
}

/* ──────────────────────────────────────────────────────────────
   Livello 1 — setupConnections
   Tutti i connect() sui membri principali (AI, bottoni d'azione).
   ────────────────────────────────────────────────────────────── */
void StrumentiPage::setupConnections()
{
    /* Bottone Cron (null nel layout a tab — Cron è il tab 6) */
    if (m_cronBtn)
        connect(m_cronBtn, &QPushButton::toggled,
                this, &StrumentiPage::onCronBtnToggled);

    /* Avvia / Stop tool (bottone unificato) */
    connect(m_btnRun, &QPushButton::clicked,
            this, &StrumentiPage::onBtnRunClicked);

    /* Bottoni office/blender/freecad exec (membri) */
    connect(m_officeStartBtn, &QPushButton::clicked,
            this, &StrumentiPage::onOfficeStartBtnClicked);
    connect(m_officeExecBtn, &QPushButton::clicked,
            this, &StrumentiPage::onOfficeExecBtnClicked);
    connect(m_blenderExecBtn, &QPushButton::clicked,
            this, &StrumentiPage::onBlenderExecBtnClicked);
    connect(m_freecadExecBtn, &QPushButton::clicked,
            this, &StrumentiPage::onFreecadExecBtnClicked);
    connect(m_btnSketchGen, &QPushButton::clicked,
            this, &StrumentiPage::onSketchGenBtnClicked);

    /* Modello codice — aggiorna lista */
    connect(m_codeModelRefresh, &QPushButton::clicked,
            this, &StrumentiPage::onCodeModelRefreshClicked);

    /* Segnali AiClient */
    connect(m_ai, &AiClient::token,      this, &StrumentiPage::onToken);
    connect(m_ai, &AiClient::finished,   this, &StrumentiPage::onFinished);
    connect(m_ai, &AiClient::error,      this, &StrumentiPage::onError);
    connect(m_ai, &AiClient::aborted,    this, &StrumentiPage::onAiAborted);
    connect(m_ai, &AiClient::modelsReady, this, &StrumentiPage::onCodeModelsReady);
    connect(m_ai, &AiClient::error,      this, &StrumentiPage::onCodeModelError);
}

/* ──────────────────────────────────────────────────────────────
   Livello 1 — setupTabOrder
   Definisce l'ordine di navigazione via Tab tra i widget chiave.
   ────────────────────────────────────────────────────────────── */
void StrumentiPage::setupTabOrder()
{
    QWidget::setTabOrder(m_ragCheck,       m_codeModelCombo);
    QWidget::setTabOrder(m_codeModelCombo, m_inputArea);
    QWidget::setTabOrder(m_inputArea,      m_btnRun);
}

/* ──────────────────────────────────────────────────────────────
   Livello 2 — buildActionStack
   Crea lo QStackedWidget con una pagina di bottoni per categoria.
   ────────────────────────────────────────────────────────────── */
QStackedWidget* StrumentiPage::buildActionStack()
{
    auto* stack = new QStackedWidget(this);
    stack->setMaximumHeight(180);

    for (int cat = 0; cat < 6; cat++) {
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
            abtn->setObjectName("strActBtn");
            actGroup->addButton(abtn, act);
            grid->addWidget(abtn, row, col);
            if (++col > 2) { col = 0; row++; }

            abtn->setProperty("strCat", cat);
            abtn->setProperty("strAct", act);
            connect(abtn, &QPushButton::clicked,
                    this, &StrumentiPage::onActBtnClicked);
        }
        for (int c = 0; c < 3; c++) grid->setColumnStretch(c, 1);
        stack->addWidget(page);
    }
    return stack;
}

/* ──────────────────────────────────────────────────────────────
   Livello 2 — buildCatScrollArea
   Crea la barra tab-categoria + pulsante Cron, avvolta in QScrollArea.
   ────────────────────────────────────────────────────────────── */
QWidget* StrumentiPage::buildCatScrollArea()
{
    static const char* kCatLabels[] = {
        "\xf0\x9f\x93\x9a Studio",
        "\xe2\x9c\x8d\xef\xb8\x8f Scrittura",
        "\xf0\x9f\x94\x8d Ricerca",
        "\xf0\x9f\x93\x96 Libri",
        "\xe2\x9a\xa1 Produttivit\xc3\xa0",
        "\xf0\x9f\x93\x84 Documenti",
    };
    static const char* kCatTooltips[] = {
        "Studio \xe2\x80\x94 spiega concetti, riassumi argomenti, analizza testi",
        "Scrittura \xe2\x80\x94 crea email, articoli, storie e testi creativi",
        "Ricerca \xe2\x80\x94 cerca info, verifica fatti, analisi critiche",
        "Libri \xe2\x80\x94 riassunti, analisi romanzi, guide alla lettura",
        "Produttivit\xc3\xa0 \xe2\x80\x94 organizza task, crea liste, gestisci il tempo",
        "Documenti \xe2\x80\x94 analizza PDF/TXT, estrai dati, riassunti",
    };

    auto* catBar = new QWidget(this);
    auto* catLay = new QHBoxLayout(catBar);
    catLay->setContentsMargins(0, 0, 0, 0);
    catLay->setSpacing(0);

    m_catGroup = new QButtonGroup(this);
    m_catGroup->setExclusive(true);

    for (int cat = 0; cat < 6; cat++) {
        auto* catBtn = new QPushButton(
            QString::fromUtf8(kCatLabels[cat]), catBar);
        catBtn->setCheckable(true);
        catBtn->setChecked(cat == 0);
        catBtn->setObjectName("strCatBtn");
        catBtn->setToolTip(QString::fromUtf8(kCatTooltips[cat]));
        m_catGroup->addButton(catBtn, cat);
        catLay->addWidget(catBtn);
    }
    catLay->addSpacing(12);

    /* Pulsante Cron — stored as member */
    m_cronBtn = new QPushButton("\xe2\x8f\xb1 Cron", catBar);
    m_cronBtn->setCheckable(true);
    m_cronBtn->setObjectName("strCatBtn");
    m_cronBtn->setToolTip(
        "Pianifica comandi periodici con il Cron Scheduler integrato");
    catLay->addWidget(m_cronBtn);

    /* Cambio categoria */
    connect(m_catGroup, QOverload<int>::of(&QButtonGroup::idClicked),
            this, &StrumentiPage::onCatGroupIdClicked);

    auto* catScroll = new QScrollArea(this);
    catScroll->setFrameShape(QFrame::NoFrame);
    catScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    catScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    catScroll->setWidget(catBar);
    catScroll->setWidgetResizable(true);
    catScroll->setFixedHeight(dpiScale(52));
    return catScroll;
}

/* ──────────────────────────────────────────────────────────────
   Livello 2 — buildSpecialRows
   Aggiunge al layout principale tutte le righe condizionali.
   ────────────────────────────────────────────────────────────── */
void StrumentiPage::buildSpecialRows(QVBoxLayout* lay)
{
    m_ragRow = buildRagRow();
    lay->addWidget(m_ragRow);

    m_pdfRow = buildPdfRow();
    lay->addWidget(m_pdfRow);

    m_blenderRow = buildBlenderRow();
    lay->addWidget(m_blenderRow);

    m_blenderHintRow = buildBlenderHintRow();
    lay->addWidget(m_blenderHintRow);

    m_officeRow = buildOfficeRow();
    lay->addWidget(m_officeRow);

    m_officeHintRow = buildOfficeHintRow();
    lay->addWidget(m_officeHintRow);

    m_freecadRow = buildFreecadRow();
    lay->addWidget(m_freecadRow);

    m_freecadHintRow = buildFreecadHintRow();
    lay->addWidget(m_freecadHintRow);

    m_sketchRow = buildSketchRow();
    lay->addWidget(m_sketchRow);

    m_cloudCompareRow = buildCloudCompareRow();
    lay->addWidget(m_cloudCompareRow);
}

/* ──────────────────────────────────────────────────────────────
   Livello 2 — buildCodeModelRow
   Riga selezione modello LLM per tutte le categorie.
   ────────────────────────────────────────────────────────────── */
QWidget* StrumentiPage::buildCodeModelRow()
{
    m_codeModelRow = new QWidget(this);
    auto* lay = new QHBoxLayout(m_codeModelRow);
    lay->setContentsMargins(0, 2, 0, 2);
    lay->setSpacing(8);

    auto* lbl = new QLabel("\xf0\x9f\xa4\x96  Modello:", m_codeModelRow);
    lbl->setObjectName("hintLabel");
    lay->addWidget(lbl);

    m_codeModelCombo = new QComboBox(m_codeModelRow);
    m_codeModelCombo->setMinimumWidth(200);
    m_codeModelCombo->setToolTip(
        "Modello LLM usato per questa sezione.\n"
        "Indipendente dal modello globale.\n"
        "I modelli evidenziati in verde sono consigliati per la categoria attiva.");
    m_codeModelCombo->addItem(m_ai->model().isEmpty()
        ? "(modello globale)" : m_ai->model(), m_ai->model());
    lay->addWidget(m_codeModelCombo);

    m_codeModelRefresh = new QPushButton("\xf0\x9f\x94\x84", m_codeModelRow);
    m_codeModelRefresh->setFixedWidth(dpiScale(32));
    m_codeModelRefresh->setToolTip(tr("Aggiorna lista modelli"));
    lay->addWidget(m_codeModelRefresh);

    m_codeModelInfo = new QLabel(m_codeModelRow);
    m_codeModelInfo->setObjectName("hintLabel");
    m_codeModelInfo->setWordWrap(false);
    m_codeModelInfo->setText(
        "\xe2\x9c\xa8 Consigliati: <b>mistral</b>, <b>llama3</b>, <b>qwen3</b>"
        " \xe2\x80\x94 buona comprensione e spiegazione");
    m_codeModelInfo->setTextFormat(Qt::RichText);
    lay->addWidget(m_codeModelInfo, 1);

    m_codeModelRow->setVisible(true);
    return m_codeModelRow;
}

/* ──────────────────────────────────────────────────────────────
   Livello 2 — buildInputRow
   Riga inputArea + btnRun + indicatori di attesa.
   ────────────────────────────────────────────────────────────── */
QWidget* StrumentiPage::buildInputRow()
{
    m_inputRow = new QWidget(this);
    auto* lay = new QHBoxLayout(m_inputRow);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(8);

    m_inputArea = new QTextEdit(m_inputRow);
    m_inputArea->setObjectName("chatInput");
    m_inputArea->setPlaceholderText(QString::fromUtf8(kPlaceholders[0]));
    m_inputArea->setMaximumHeight(90);
    m_inputArea->setMinimumHeight(60);
    lay->addWidget(m_inputArea, 1);

    auto* btnCol = new QVBoxLayout;
    btnCol->setSpacing(6);

    m_btnRun = new QPushButton("\xe2\x96\xb6  Esegui", m_inputRow);
    m_btnRun->setObjectName("actionBtn");
    m_btnRun->setToolTip(tr("Invia la richiesta al modello AI (Invio+Ctrl)"));
    m_btnRun->setAccessibleName("Esegui richiesta AI");
    m_btnRun->setFixedWidth(dpiScale(110));

    m_waitLbl = new QLabel(m_inputRow);
    m_waitLbl->setStyleSheet(
        "color:#E5C400;font-style:italic;font-size:11px;");
    m_waitLbl->setVisible(false);
    m_waitLbl->setWordWrap(true);

    m_waitBar = new QProgressBar(m_inputRow);
    m_waitBar->setRange(0, 0);
    m_waitBar->setFixedHeight(dpiScale(4));
    m_waitBar->setTextVisible(false);
    m_waitBar->setVisible(false);

    btnCol->addWidget(m_btnRun);
    btnCol->addWidget(m_waitLbl);
    btnCol->addWidget(m_waitBar);
    btnCol->addStretch();
    lay->addLayout(btnCol);
    return m_inputRow;
}

/* ──────────────────────────────────────────────────────────────
   Livello 2 — buildOutputArea
   QTextEdit di output AI in modalità sola lettura.
   ────────────────────────────────────────────────────────────── */
QWidget* StrumentiPage::buildOutputArea()
{
    m_output = new QTextEdit(this);
    m_output->setObjectName("chatLog");
    m_output->setReadOnly(true);
    m_output->setPlaceholderText(
        "L'output dell'AI appare qui...\n\n"
        "\xf0\x9f\x8d\xba  Invocazione riuscita. Gli dei ascoltano.");
    return m_output;
}

/* ──────────────────────────────────────────────────────────────
   Livello 2 — buildCronPanel
   Pannello placeholder per il Cron Scheduler (lazy-init).
   ────────────────────────────────────────────────────────────── */
QWidget* StrumentiPage::buildCronPanel()
{
    m_cronPanel = new QWidget(this);
    auto* cl = new QVBoxLayout(m_cronPanel);
    cl->setContentsMargins(16, 16, 16, 16);
    auto* lbl = new QLabel(
        "\xe2\x8f\xb3  Caricamento Cron Scheduler in corso...");
    lbl->setTextFormat(Qt::RichText);
    lbl->setWordWrap(true);
    lbl->setObjectName("hintLabel");
    cl->addWidget(lbl);
    cl->addStretch();
    m_cronPanel->setVisible(false);
    return m_cronPanel;
}

/* ──────────────────────────────────────────────────────────────
   Livello 3 — buildRagRow
   Riga RAG in-page: checkbox, aggiungi, info, svuota.
   Connette ragAddBtn e ragClearBtn internamente.
   ────────────────────────────────────────────────────────────── */
QWidget* StrumentiPage::buildRagRow()
{
    auto* row = new QWidget(this);
    row->setObjectName("ragRow");
    auto* lay = new QHBoxLayout(row);
    lay->setContentsMargins(8, 4, 8, 4);
    lay->setSpacing(8);

    m_ragCheck = new QCheckBox("\xf0\x9f\x93\x9a  RAG documenti", row);
    m_ragCheck->setToolTip(
        "Se attivo, i documenti caricati vengono usati come contesto\n"
        "per ogni richiesta AI in questa sezione.\n"
        "Puoi anche trascinare PDF/TXT/MD direttamente sulla finestra.");
    lay->addWidget(m_ragCheck);

    auto* ragAddBtn = new QPushButton("\xf0\x9f\x93\x82  Aggiungi", row);
    ragAddBtn->setObjectName("actionBtn");
    ragAddBtn->setFixedWidth(dpiScale(100));
    ragAddBtn->setToolTip(tr("Aggiungi PDF, TXT o Markdown all'indice RAG"));
    lay->addWidget(ragAddBtn);

    m_ragInfoLbl = new QLabel("Nessun documento caricato", row);
    m_ragInfoLbl->setObjectName("hintLabel");
    lay->addWidget(m_ragInfoLbl, 1);

    auto* ragClearBtn = new QPushButton("\xf0\x9f\x97\x91  Svuota", row);
    ragClearBtn->setObjectName("actionBtn");
    ragClearBtn->setToolTip(tr("Rimuove tutti i documenti dall'indice RAG in-page"));
    ragClearBtn->setFixedWidth(dpiScale(80));
    lay->addWidget(ragClearBtn);

    connect(ragAddBtn,   &QPushButton::clicked,
            this, &StrumentiPage::onRagAddBtnClicked);
    connect(ragClearBtn, &QPushButton::clicked,
            this, &StrumentiPage::onRagClearBtnClicked);
    return row;
}

/* ──────────────────────────────────────────────────────────────
   Livello 3 — buildPdfRow
   Riga PDF picker (visibile solo per categoria Documenti).
   Connette pdfBtn internamente.
   ────────────────────────────────────────────────────────────── */
QWidget* StrumentiPage::buildPdfRow()
{
    auto* row = new QWidget(this);
    auto* lay = new QHBoxLayout(row);
    lay->setContentsMargins(0, 4, 0, 0);
    lay->setSpacing(8);

    auto* pdfBtn = new QPushButton("\xf0\x9f\x93\x84  Carica PDF", row);
    pdfBtn->setObjectName("actionBtn");
    pdfBtn->setToolTip(
        "Seleziona un file PDF da usare come contesto per la generazione AI");
    pdfBtn->setFixedWidth(dpiScale(130));

    m_pdfPathLbl = new QLabel("Nessun PDF caricato", row);
    m_pdfPathLbl->setObjectName("hintLabel");
    m_pdfPathLbl->setWordWrap(false);

    lay->addWidget(pdfBtn);
    lay->addWidget(m_pdfPathLbl, 1);
    row->setVisible(false);

    connect(pdfBtn, &QPushButton::clicked,
            this, &StrumentiPage::onPdfBtnClicked);
    return row;
}

