#include "strumenti_page.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QButtonGroup>
#include <QStackedWidget>
#include <QFileDialog>
#include <QProcess>
#include <QFileInfo>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonDocument>
#include <QUrl>
#include <QTimer>
#include <QDir>
#include <QFile>
#include <QCoreApplication>

/* ══════════════════════════════════════════════════════════════
   Tabella system prompt: [navIdx][subIdx]
   ══════════════════════════════════════════════════════════════ */
static const char* kSysPrompts[8][8] = {
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
    /* 6 — Blender MCP */
    {
        /* Cambia Materiale */
        "Sei un esperto di Blender Python API (bpy). Genera SOLO codice Python puro eseguibile in Blender. "
        "Crea o modifica un materiale Principled BSDF sull'oggetto attivo (bpy.context.active_object). "
        "Imposta base_color, metallic, roughness secondo la richiesta. "
        "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

        /* Trasla */
        "Sei un esperto di Blender Python API (bpy). Genera SOLO codice Python puro per spostare un oggetto in Blender. "
        "Usa bpy.context.active_object.location = (x, y, z). "
        "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

        /* Ruota */
        "Sei un esperto di Blender Python API (bpy). Genera SOLO codice Python puro per ruotare un oggetto in Blender. "
        "Usa bpy.context.active_object.rotation_euler = (rx, ry, rz) con angoli in radianti (import math; math.radians(...)). "
        "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

        /* Scala */
        "Sei un esperto di Blender Python API (bpy). Genera SOLO codice Python puro per scalare un oggetto in Blender. "
        "Usa bpy.context.active_object.scale = (sx, sy, sz). "
        "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

        /* Visibilita' */
        "Sei un esperto di Blender Python API (bpy). Genera SOLO codice Python puro per cambiare la visibilita' di oggetti. "
        "Usa obj.hide_viewport e obj.hide_render. Itera su bpy.data.objects se necessario. "
        "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

        /* Avvia Render */
        "Sei un esperto di Blender Python API (bpy). Genera SOLO codice Python puro per configurare e avviare un render. "
        "Usa bpy.context.scene.render per le impostazioni, bpy.ops.render.render(write_still=True) per avviare. "
        "Imposta l'output path se richiesto. "
        "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

        /* Script libero */
        "Sei un esperto di Blender Python API (bpy). Genera SOLO codice Python puro eseguibile in Blender via exec(). "
        "Il namespace disponibile: bpy, mathutils, Vector, Euler, Matrix. "
        "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

        nullptr
    },
    /* 7 — Office / LibreOffice (UNO o file-based) */
    {
        /* Crea documento Writer / Word */
        "Sei un esperto di LibreOffice e python-docx. Genera SOLO codice Python. "
        "PRIORITA': se 'desktop' e' nel namespace (modalita' UNO), usa LibreOffice Writer direttamente: "
        "  doc = desktop.loadComponentFromURL('private:factory/swriter', '_blank', 0, ()); "
        "  text = doc.getText(); cur = text.createTextCursor(); text.insertString(cur, '...', False); "
        "  out = Path.home()/'Desktop'/'documento.odt'; doc.storeToURL(systemPath(str(out)), ()); print(out). "
        "FALLBACK (no desktop): usa python-docx: Document() → salva in Path.home()/'Desktop'/'documento.docx'. "
        "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

        /* Crea foglio Calc / Excel */
        "Sei un esperto di LibreOffice Calc e openpyxl. Genera SOLO codice Python. "
        "PRIORITA' UNO: doc = desktop.loadComponentFromURL('private:factory/scalc', '_blank', 0, ()); "
        "  sheet = doc.Sheets.getByIndex(0); cell = sheet.getCellByPosition(col, row); "
        "  cell.setString('testo') oppure cell.setValue(numero); "
        "  out = Path.home()/'Desktop'/'foglio.ods'; doc.storeToURL(systemPath(str(out)), ()); print(out). "
        "FALLBACK: usa openpyxl Workbook → salva .xlsx in Path.home()/'Desktop'/. "
        "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

        /* Crea Presentazione Impress / PowerPoint */
        "Sei un esperto di LibreOffice Impress e python-pptx. Genera SOLO codice Python. "
        "PRIORITA' UNO: doc = desktop.loadComponentFromURL('private:factory/simpress', '_blank', 0, ()); "
        "  slide = doc.DrawPages[0]; slide.Name = 'Slide 1'; "
        "  out = Path.home()/'Desktop'/'presentazione.odp'; doc.storeToURL(systemPath(str(out)), ()); print(out). "
        "FALLBACK: usa python-pptx Presentation → salva .pptx in Path.home()/'Desktop'/. "
        "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

        /* Modifica documento aperto */
        "Sei un esperto di LibreOffice UNO. Genera SOLO codice Python. "
        "Con UNO apri il documento con desktop.loadComponentFromURL(systemPath(percorso), '_blank', 0, ()); "
        "modifica testo/celle/slide; salva con doc.store() (sovrascrive) o doc.storeToURL(url, ()). "
        "Se il percorso non e' fornito, usa Path.home()/'Desktop'/'documento.odt'. "
        "FALLBACK senza UNO: usa python-docx Document(path). "
        "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

        /* Tabella */
        "Sei un esperto di LibreOffice UNO e python-docx. Genera SOLO codice Python che crea una tabella formattata. "
        "Con UNO in Writer: doc.getText().insertTextContent(cur, doc.createInstance('com.sun.star.text.TextTable'), False). "
        "Con UNO in Calc: accedi alle celle con sheet.getCellByPosition(col, row). "
        "FALLBACK: python-docx Document + add_table oppure openpyxl Workbook. "
        "Salva e stampa il percorso. "
        "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

        /* Grafici e dati */
        "Sei un esperto di LibreOffice Calc UNO e openpyxl. Genera SOLO codice Python per grafici/analisi dati. "
        "Con UNO in Calc: inserisci dati nelle celle, poi crea grafico con createInstance('com.sun.star.chart.ChartDocument'). "
        "FALLBACK: usa openpyxl BarChart + Reference. "
        "Inserisci dati di esempio se non forniti. Salva e stampa il percorso. "
        "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

        /* Script libero */
        "Sei un esperto di LibreOffice UNO API. Genera SOLO codice Python eseguibile via exec(). "
        "Namespace UNO: desktop, uno, PropertyValue, createUnoService, systemPath, mkprops. "
        "Namespace file: Document/Pt (python-docx), Workbook/Font (openpyxl), Presentation/Inches (python-pptx). "
        "Namespace comune: Path, os, datetime. "
        "Salva sempre in Path.home()/'Desktop'/ e stampa il percorso o la conferma. "
        "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

        nullptr
    },
};

static const char* kSubActions[8][8] = {
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
    /* 6 — Blender MCP */
    { "\xf0\x9f\x8e\xa8 Cambia Materiale",
      "\xe2\x86\x94 Trasla",
      "\xf0\x9f\x94\x84 Ruota",
      "\xf0\x9f\x93\x90 Scala",
      "\xf0\x9f\x91\x81 Visibilit\xc3\xa0",
      "\xf0\x9f\x8e\xac Avvia Render",
      "\xf0\x9f\x90\x8d Script libero",
      nullptr },
    /* 7 — Office / LibreOffice */
    { "\xf0\x9f\x93\x84 Crea documento Word",
      "\xf0\x9f\x93\x8a Crea foglio Excel",
      "\xf0\x9f\x96\xa5 Crea presentazione",
      "\xe2\x9c\x8f Modifica documento",
      "\xf0\x9f\x93\x8b Inserisci tabella",
      "\xf0\x9f\x93\x88 Grafici e dati",
      "\xf0\x9f\x94\xa7 Script libero",
      nullptr },
};

static const char* kPlaceholders[8] = {
    "Incolla il testo o descrivi il concetto da studiare...",
    "Descrivi l'idea, il personaggio o la scena...",
    "Inserisci l'argomento da ricercare o l'affermazione da verificare...",
    "Incolla il testo del libro o il titolo e l'autore...",
    "Descrivi il progetto, il task o l'obiettivo...",
    "Incolla il testo del documento oppure carica un PDF con il pulsante sopra...",
    "Descrivi cosa fare in Blender (es. 'Cambia il cubo in rosso metallico', 'Sposta il piano a Y=3', 'Ruota 45 gradi sull asse Z')...",
    "Descrivi il documento da creare (es. 'Crea una lettera di presentazione professionale', 'Foglio Excel con budget mensile', 'Slide riassuntive su Python')...",
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
    for (int i = 0; i < 8; i++) m_navList->addItem("");
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
        "\xf0\x9f\x8e\xa8  Blender",
        "\xf0\x9f\x96\xa5  Office",
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

    for (int cat = 0; cat < 8; cat++) {
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
                m_currentCat = cat;
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
        m_currentCat = cat;
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
        m_blenderRow->setVisible(cat == 6);
        m_officeRow->setVisible(cat == 7);
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

    /* ── Riga Blender Bridge (visibile solo per categoria Blender) ── */
    m_blenderRow = new QWidget(this);
    auto* blenderLay = new QHBoxLayout(m_blenderRow);
    blenderLay->setContentsMargins(0, 4, 0, 0);
    blenderLay->setSpacing(8);

    auto* blenderLbl = new QLabel("Blender:", m_blenderRow);
    blenderLbl->setObjectName("hintLabel");

    m_blenderHostEdit = new QLineEdit("localhost:6789", m_blenderRow);
    m_blenderHostEdit->setFixedWidth(160);
    m_blenderHostEdit->setPlaceholderText("localhost:6789");

    auto* blenderPingBtn = new QPushButton("\xf0\x9f\x94\x97  Verifica", m_blenderRow);
    blenderPingBtn->setObjectName("actionBtn");
    blenderPingBtn->setFixedWidth(100);

    m_blenderStatusLbl = new QLabel("\xe2\x9a\xaa  Non connesso", m_blenderRow);
    m_blenderStatusLbl->setObjectName("hintLabel");

    m_blenderExecBtn = new QPushButton(
        "\xe2\x96\xb6  Esegui in Blender", m_blenderRow);
    m_blenderExecBtn->setObjectName("actionBtn");
    m_blenderExecBtn->setFixedWidth(160);
    m_blenderExecBtn->setEnabled(false);

    blenderLay->addWidget(blenderLbl);
    blenderLay->addWidget(m_blenderHostEdit);
    blenderLay->addWidget(blenderPingBtn);
    blenderLay->addWidget(m_blenderStatusLbl, 1);
    blenderLay->addWidget(m_blenderExecBtn);
    m_blenderRow->setVisible(false);
    lay->addWidget(m_blenderRow);

    m_blenderNam = new QNetworkAccessManager(this);

    /* Ping /status → verifica connessione Blender */
    connect(blenderPingBtn, &QPushButton::clicked, this, [this]() {
        QString addr = m_blenderHostEdit->text().trimmed();
        if (addr.isEmpty()) addr = "localhost:6789";
        QNetworkRequest req(QUrl("http://" + addr + "/status"));
        req.setTransferTimeout(3000);
        auto* reply = m_blenderNam->get(req);
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            reply->deleteLater();
            if (reply->error() == QNetworkReply::NoError) {
                QJsonObject obj = QJsonDocument::fromJson(
                    reply->readAll()).object();
                QString ver = obj.value("blender").toString("?");
                m_blenderStatusLbl->setText(
                    "\xe2\x9c\x85  Blender " + ver + " connesso");
            } else {
                m_blenderStatusLbl->setText(
                    "\xe2\x9d\x8c  " + reply->errorString());
            }
        });
    });

    /* POST /execute → invia codice bpy a Blender */
    connect(m_blenderExecBtn, &QPushButton::clicked, this, [this]() {
        if (m_blenderCode.isEmpty()) return;
        QString addr = m_blenderHostEdit->text().trimmed();
        if (addr.isEmpty()) addr = "localhost:6789";

        QJsonObject payload;
        payload["code"] = m_blenderCode;
        QByteArray body =
            QJsonDocument(payload).toJson(QJsonDocument::Compact);

        QNetworkRequest req(QUrl("http://" + addr + "/execute"));
        req.setHeader(QNetworkRequest::ContentTypeHeader,
                      "application/json; charset=utf-8");
        req.setTransferTimeout(20000);

        m_blenderExecBtn->setEnabled(false);
        m_blenderStatusLbl->setText(
            "\xf0\x9f\x94\x84  Invio a Blender...");

        auto* reply = m_blenderNam->post(req, body);
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            reply->deleteLater();
            m_blenderExecBtn->setEnabled(true);
            if (reply->error() == QNetworkReply::NoError) {
                QJsonObject res = QJsonDocument::fromJson(
                    reply->readAll()).object();
                if (res["ok"].toBool()) {
                    m_blenderStatusLbl->setText("\xe2\x9c\x85  Eseguito");
                    QString out = res["output"].toString();
                    m_output->append(
                        "\n\xe2\x9c\x85  Blender: " +
                        (out.isEmpty() ? "OK" : out));
                } else {
                    m_blenderStatusLbl->setText("\xe2\x9d\x8c  Errore Blender");
                    m_output->append(
                        "\n\xe2\x9d\x8c  Blender errore:\n" +
                        res["error"].toString());
                }
            } else {
                m_blenderStatusLbl->setText(
                    "\xe2\x9d\x8c  " + reply->errorString());
                m_output->append(
                    "\n\xe2\x9d\x8c  Connessione a Blender fallita: " +
                    reply->errorString());
            }
        });
    });

    /* ── Riga Office Bridge (visibile solo per categoria Office) ── */
    m_officeRow = new QWidget(this);
    auto* officeLay = new QHBoxLayout(m_officeRow);
    officeLay->setContentsMargins(0, 4, 0, 0);
    officeLay->setSpacing(8);

    auto* officeLbl = new QLabel("Office:", m_officeRow);
    officeLbl->setObjectName("hintLabel");

    m_officeStartBtn = new QPushButton(
        "\xe2\x96\xb6  Avvia bridge", m_officeRow);
    m_officeStartBtn->setObjectName("actionBtn");
    m_officeStartBtn->setFixedWidth(120);

    m_officeStatusLbl = new QLabel(
        "\xe2\x9a\xaa  Bridge non avviato", m_officeRow);
    m_officeStatusLbl->setObjectName("hintLabel");

    m_officeExecBtn = new QPushButton(
        "\xf0\x9f\x96\xa5  Esegui in Office", m_officeRow);
    m_officeExecBtn->setObjectName("actionBtn");
    m_officeExecBtn->setFixedWidth(160);
    m_officeExecBtn->setEnabled(false);

    officeLay->addWidget(officeLbl);
    officeLay->addWidget(m_officeStartBtn);
    officeLay->addWidget(m_officeStatusLbl, 1);
    officeLay->addWidget(m_officeExecBtn);
    m_officeRow->setVisible(false);
    lay->addWidget(m_officeRow);

    m_officeNam = new QNetworkAccessManager(this);

    /* Legge il token Bearer scritto dal bridge in ~/.prismalux_office_token.
       Chiamata prima di ogni request — il token cambia ad ogni avvio del bridge. */
    auto _readToken = [this]() {
        const QString tokenFile =
            QDir::homePath() + "/.prismalux_office_token";
        QFile f(tokenFile);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
        m_officeBridgeToken = QString::fromUtf8(f.readAll()).trimmed();
    };

    /* Trova il percorso del bridge relativo all'eseguibile */
    auto _bridgePath = []() -> QString {
        // cartella sorgente (sviluppo): <exe>/../../office_bridge/
        QDir d(QCoreApplication::applicationDirPath());
        for (int i = 0; i < 4; i++) {
            QString p = d.filePath("office_bridge/prismalux_office_bridge.py");
            if (QFile::exists(p)) return p;
            d.cdUp();
        }
        return {};
    };

    /* Avvia / ferma bridge Python */
    connect(m_officeStartBtn, &QPushButton::clicked, this, [this, _bridgePath, _readToken]() {
        // Se già in esecuzione → ferma
        if (m_officeBridgeProc &&
            m_officeBridgeProc->state() == QProcess::Running) {
            m_officeBridgeProc->terminate();
            m_officeBridgeProc->waitForFinished(2000);
            m_officeStartBtn->setText("\xe2\x96\xb6  Avvia bridge");
            m_officeStatusLbl->setText("\xe2\x9a\xaa  Bridge fermato");
            return;
        }

        QString path = _bridgePath();
        if (path.isEmpty()) {
            m_officeStatusLbl->setText(
                "\xe2\x9d\x8c  prismalux_office_bridge.py non trovato");
            return;
        }

        if (!m_officeBridgeProc) {
            m_officeBridgeProc = new QProcess(this);
            m_officeBridgeProc->setProcessChannelMode(
                QProcess::MergedChannels);
            connect(m_officeBridgeProc,
                    QOverload<int,QProcess::ExitStatus>::of(
                        &QProcess::finished),
                    this, [this](int, QProcess::ExitStatus) {
                m_officeStartBtn->setText("\xe2\x96\xb6  Avvia bridge");
                m_officeStatusLbl->setText("\xe2\x9a\xaa  Bridge fermato");
            });
        }

        m_officeStatusLbl->setText("\xf0\x9f\x94\x84  Avvio bridge...");
        m_officeBridgeProc->start("python3", {path});
        if (!m_officeBridgeProc->waitForStarted(3000)) {
            // fallback a python su Windows
            m_officeBridgeProc->start("python", {path});
        }
        if (m_officeBridgeProc->state() == QProcess::Running) {
            m_officeStartBtn->setText("\xe2\x8f\xb9  Ferma bridge");
            // Verifica /status dopo 1 secondo
            QTimer::singleShot(1200, this, [this, _readToken]() {
                _readToken();
                QNetworkRequest req(
                    QUrl("http://localhost:6790/status"));
                req.setTransferTimeout(2000);
                req.setRawHeader("Authorization",
                    ("Bearer " + m_officeBridgeToken).toUtf8());
                auto* r = m_officeNam->get(req);
                connect(r, &QNetworkReply::finished, this, [this, r]() {
                    r->deleteLater();
                    if (r->error() == QNetworkReply::NoError) {
                        QJsonObject obj =
                            QJsonDocument::fromJson(r->readAll()).object();
                        QJsonObject libs = obj["libraries"].toObject();
                        QStringList ok;
                        for (auto it = libs.begin(); it != libs.end(); ++it)
                            if (it.value().toBool()) ok << it.key();
                        m_officeStatusLbl->setText(
                            "\xe2\x9c\x85  " +
                            (ok.isEmpty() ? "Bridge pronto (nessuna lib)" :
                             "Pronto: " + ok.join(", ")));
                    } else {
                        m_officeStatusLbl->setText(
                            "\xe2\x9a\xa0  Bridge avviato (verifica fallita)");
                    }
                });
            });
        } else {
            m_officeStatusLbl->setText(
                "\xe2\x9d\x8c  Errore avvio (python3 non trovato?)");
        }
    });

    /* POST /execute → invia codice office al bridge */
    connect(m_officeExecBtn, &QPushButton::clicked, this, [this, _readToken]() {
        if (m_officeCode.isEmpty()) return;

        _readToken();  // aggiorna token in caso il bridge sia stato riavviato
        QJsonObject payload;
        payload["code"] = m_officeCode;
        QByteArray body =
            QJsonDocument(payload).toJson(QJsonDocument::Compact);

        QNetworkRequest req(QUrl("http://localhost:6790/execute"));
        req.setHeader(QNetworkRequest::ContentTypeHeader,
                      "application/json; charset=utf-8");
        req.setRawHeader("Authorization",
            ("Bearer " + m_officeBridgeToken).toUtf8());
        req.setTransferTimeout(30000);

        m_officeExecBtn->setEnabled(false);
        m_officeStatusLbl->setText("\xf0\x9f\x94\x84  Esecuzione...");

        auto* reply = m_officeNam->post(req, body);
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            reply->deleteLater();
            m_officeExecBtn->setEnabled(true);
            if (reply->error() == QNetworkReply::NoError) {
                QJsonObject res =
                    QJsonDocument::fromJson(reply->readAll()).object();
                if (res["ok"].toBool()) {
                    m_officeStatusLbl->setText("\xe2\x9c\x85  Completato");
                    m_output->append(
                        "\n\xe2\x9c\x85  Office: " +
                        res["output"].toString());
                } else {
                    m_officeStatusLbl->setText("\xe2\x9d\x8c  Errore");
                    m_output->append(
                        "\n\xe2\x9d\x8c  Office errore:\n" +
                        res["error"].toString());
                }
            } else {
                m_officeStatusLbl->setText(
                    "\xe2\x9d\x8c  Bridge non raggiungibile");
                m_output->append(
                    "\n\xe2\x9d\x8c  Bridge non raggiungibile (avvialo prima).");
            }
        });
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

void StrumentiPage::onFinished(const QString& full) {
    if (!m_active) return;
    m_active = false;
    m_waitLbl->setVisible(false);
    m_btnRun->setEnabled(true);
    m_btnStop->setEnabled(false);
    m_output->append("\n" + QString(40, QChar(0x2500)));

    /* ── Blender / Office: estrai codice Python dal blocco ```...``` ── */
    if ((m_currentCat == 6 || m_currentCat == 7) && !full.isEmpty()) {
        // Lambda locale: estrae il primo blocco ```python...``` o ```...```
        auto extractCode = [](const QString& text) -> QString {
            int start = text.indexOf("```python");
            if (start != -1) {
                start = text.indexOf('\n', start) + 1;
                int end = text.indexOf("```", start);
                if (end != -1) return text.mid(start, end - start).trimmed();
            }
            start = text.indexOf("```");
            if (start != -1) {
                start += 3;
                if (start < text.size() && text[start] == '\n') start++;
                int end = text.indexOf("```", start);
                if (end != -1) return text.mid(start, end - start).trimmed();
            }
            return text.trimmed();
        };

        const QString code = extractCode(full);
        if (m_currentCat == 6) {
            m_blenderCode = code;
            if (!m_blenderCode.isEmpty()) {
                m_blenderExecBtn->setEnabled(true);
                m_blenderStatusLbl->setText(
                    "\xf0\x9f\x90\x8d  Codice pronto \xe2\x80\x94 premi Esegui in Blender");
            }
        } else { // cat 7 — Office
            m_officeCode = code;
            if (!m_officeCode.isEmpty()) {
                m_officeExecBtn->setEnabled(true);
                m_officeStatusLbl->setText(
                    "\xf0\x9f\x93\x84  Codice pronto \xe2\x80\x94 premi Esegui in Office");
            }
        }
    }
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
