#include "ricerca_page.h"
#include "lavoro_page.h"
#include "../prismalux_paths.h"
#include "../widgets/astro_calc.h"
namespace P = PrismaluxPaths;
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QTabWidget>
#include <QSplitter>
#include <QScrollArea>
#include <QGroupBox>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QTextEdit>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QDateTime>
#include <QPrinter>
#include <QTextDocument>
#include <QPageSize>
#include <QDesktopServices>
#include <QUrl>
#include <QProcess>
#include <QTimer>
#include <QTcpSocket>
#include <QPointer>
#include <QTextCursor>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QRadioButton>
#include <QButtonGroup>
#include <QGroupBox>
#include <QDateTimeEdit>
#include <QFormLayout>
#include <QTextBrowser>
#include <QDoubleSpinBox>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QStandardPaths>
#include <cmath>

/* ── helper: barra azioni output (Esporta PDF / Salva .md) ────────── */
static QWidget* makeOutputBar(QTextEdit* editor, const QString& titolo,
                              QWidget* parent)
{
    auto* bar  = new QWidget(parent);
    auto* blay = new QHBoxLayout(bar);
    blay->setContentsMargins(0, 0, 0, 0);
    blay->setSpacing(6);

    auto* lblTit = new QLabel("<b>" + titolo + "</b>");
    auto* btnPdf = new QPushButton("\xf0\x9f\x96\xa8  Esporta PDF");
    auto* btnMd  = new QPushButton("\xf0\x9f\x92\xbe  Salva .md");
    auto* btnClr = new QPushButton("\xf0\x9f\x97\x91  Svuota");
    btnPdf->setObjectName("actionBtn");
    btnMd->setObjectName("actionBtn");
    btnClr->setObjectName("actionBtn");

    blay->addWidget(lblTit);
    blay->addStretch();
    blay->addWidget(btnMd);
    blay->addWidget(btnPdf);
    blay->addWidget(btnClr);

    /* Store editor and titolo as button properties so named slots can retrieve them */
    btnPdf->setProperty("outputEditor", QVariant::fromValue<QObject*>(editor));
    btnPdf->setProperty("outputTitolo", titolo);
    btnMd->setProperty("outputEditor",  QVariant::fromValue<QObject*>(editor));
    btnMd->setProperty("outputTitolo",  titolo);
    btnClr->setProperty("outputEditor", QVariant::fromValue<QObject*>(editor));

    auto* rp = qobject_cast<RicercaPage*>(parent);
    QObject::connect(btnPdf, &QPushButton::clicked, rp, &RicercaPage::onOutputBarPdfClicked);
    QObject::connect(btnMd,  &QPushButton::clicked, rp, &RicercaPage::onOutputBarMdClicked);
    QObject::connect(btnClr, &QPushButton::clicked, rp, &RicercaPage::onOutputBarClrClicked);
    return bar;
}

/* ═══════════════════════════════════════════════════════════════════
   Costruttore principale
   ═══════════════════════════════════════════════════════════════════ */
RicercaPage::RicercaPage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    auto* vlay = new QVBoxLayout(this);
    vlay->setContentsMargins(0, 0, 0, 0);
    vlay->setSpacing(0);

    /* titolo sezione */
    auto* header = new QLabel(
        "  \xf0\x9f\x94\xac  <b>Ricerca e Sviluppo</b>"
        "  <span style='color:gray;font-size:11px;'>"
        "Paper scientifici \xe2\x80\x94 Brevetti \xe2\x80\x94 Documenti tecnici"
        "</span>");
    header->setTextFormat(Qt::RichText);
    header->setObjectName("pageHeader");
    header->setFixedHeight(36);
    vlay->addWidget(header);

    auto* tabs = new QTabWidget(this);
    tabs->setObjectName("settingsInnerTabs");
    tabs->setDocumentMode(true);
    /* ── Gruppo 1: Genera ── */
    tabs->addTab(buildPaperTab(),             "\xf0\x9f\x93\x84  Paper");
    tabs->addTab(buildBrevettoTab(),          "\xf0\x9f\x94\x8f  Brevetto");
    tabs->addTab(buildDocTecnicoTab(),        "\xf0\x9f\x93\x8b  Doc Tecnico");
    /* ── Gruppo 2: Cerca ── */
    tabs->addTab(buildCercaLetteraturaTab(),  "\xf0\x9f\x94\x8d  Cerca Paper/Brevetti");
    tabs->addTab(new LavoroPage(m_ai, this),  "\xf0\x9f\x92\xbc  Lavoro");
    /* ── Gruppo 3: Scienze ── */
    tabs->addTab(buildCytoscapeTab(),         "\xf0\x9f\x94\xac  Cytoscape \xe2\x80\x94 Bioinformatica");
    tabs->addTab(buildRDKitTab(),             "\xf0\x9f\xa7\xaa  RDKit");
    tabs->addTab(buildBiocondaTab(),          "\xf0\x9f\x8c\xbf  Bioconda");
    tabs->addTab(buildAvogadroTab(),          "\xf0\x9f\xa7\xb4  Avogadro");
    /* ── Gruppo 4: R&D originale ── */
    tabs->addTab(buildRab0lTab(),
                 "\xf0\x9f\xa7\xac  RAB\xe2\x82\x80-L");
    tabs->addTab(buildBlhmTab(),
                 "\xf0\x9f\xa7\xa0  BLHM");
    /* ── Gruppo 5: Analisi eventi ── */
    tabs->addTab(buildAnalisiPage(),
                 "\xf0\x9f\x8c\x8c  Analisi Fenomeni");
    /* ── Gruppo 6: Astrologia ── */
    tabs->addTab(buildRagGrafoTab(),
                 "\xf0\x9f\x95\xb8  Grafo RAG");   /* 🕸️ */
    tabs->addTab(buildAstraleTab(),
                 "\xe2\xad\x90  Carta Astrale");

    /* Tooltip sui tab per scopribilità */
    tabs->setTabToolTip(0, "Genera paper accademico con AI");
    tabs->setTabToolTip(1, "Genera documento brevettuale PCT/EPO");
    tabs->setTabToolTip(2, "Genera specifiche tecniche e manuali");
    tabs->setTabToolTip(3, "Cerca su arXiv, Semantic Scholar, USPTO");
    tabs->setTabToolTip(4, "Offerte di lavoro, tracker candidature e calcolatore euro/ore");
    tabs->setTabToolTip(5, "Cytoscape — Bioinformatica: analisi reti biologiche (proteomica, genomica, pathways)");
    tabs->setTabToolTip(6, "Chemioinformatica con RDKit");
    tabs->setTabToolTip(7, "Pipeline bioinformatica con Bioconda");
    tabs->setTabToolTip(8, "Modellazione molecolare 3D");
    tabs->setTabToolTip(9,
        "RAB\xe2\x82\x80-L: rappresentazione DNA su spirale logaritmica base-80 (wildlux, 2025)");
    tabs->setTabToolTip(10,
        "BLHM: calcolatore R_merged per architettura Brain-Loop-Human-MultiContext (wildlux, 2026)");
    tabs->setTabToolTip(11,
        "Analisi Fenomeni: valuta la probabilit\xc3\xa0 che un evento fisico/chimico/alieno/paranormale"
        " sia realmente accaduto, sulla base delle fonti fornite");
    tabs->setTabToolTip(12,
        "Carta Astrale / Tema Natale: inserisci data, ora e luogo di nascita"
        " per una lettura astrologica con AI");
    vlay->addWidget(tabs, 1);

    m_sciProgress = new QProgressBar(this);
    m_sciProgress->setRange(0, 0);
    m_sciProgress->setFixedHeight(4);
    m_sciProgress->setTextVisible(false);
    m_sciProgress->setVisible(false);
    vlay->addWidget(m_sciProgress);

    m_sciErrorPanel = new AiErrorWidget(this);
    vlay->addWidget(m_sciErrorPanel);

    /* Propaga modelli a tutti le combo science */
    connect(m_ai, &AiClient::modelsReady, this, &RicercaPage::onSciModelsReady);

    /* ── connessioni AI (una sola volta per tutta la pagina) ──────── */
    connect(m_ai, &AiClient::token,    this, &RicercaPage::onAiToken);
    connect(m_ai, &AiClient::finished, this, &RicercaPage::onAiFinished);
    connect(m_ai, &AiClient::error,    this, &RicercaPage::onAiError);
    connect(m_ai, &AiClient::aborted,  this, &RicercaPage::onAiAborted);
}

void RicercaPage::resetButtons()
{
    if (m_btnGenAttivo)  m_btnGenAttivo->setEnabled(true);
    if (m_btnStopAttivo) m_btnStopAttivo->setEnabled(false);
    m_btnGenAttivo  = nullptr;
    m_btnStopAttivo = nullptr;
    if (m_sciProgress) m_sciProgress->setVisible(false);
}

void RicercaPage::avvia(const QString& sys, const QString& msg,
                        QTextEdit* out, QPushButton* btnGen,
                        QPushButton* btnStop)
{
    out->clear();
    out->append(
        "\xf0\x9f\xa4\x96  Generazione in corso...\n"
        "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
        "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
        "\xe2\x94\x80\xe2\x94\x80\n");
    m_outCurrent    = out;
    m_btnGenAttivo  = btnGen;
    m_btnStopAttivo = btnStop;
    btnGen->setEnabled(false);
    btnStop->setEnabled(true);
    if (m_sciProgress) m_sciProgress->setVisible(true);
    m_ai->chat(sys, msg);
}

/* ─────────────────────────────────────────────────────────────────
   buildPaperTab
   ───────────────────────────────────────────────────────────────── */
QWidget* RicercaPage::buildPaperTab()
{
    auto* page  = new QWidget;
    auto* hlay  = new QHBoxLayout(page);
    hlay->setContentsMargins(8, 8, 8, 8);
    hlay->setSpacing(10);

    /* ── pannello sinistra: form ── */
    auto* formScroll = new QScrollArea;
    formScroll->setFrameShape(QFrame::NoFrame);
    formScroll->setWidgetResizable(true);
    formScroll->setMaximumWidth(380);

    auto* formW = new QWidget;
    auto* form  = new QFormLayout(formW);
    form->setSpacing(8);
    form->setLabelAlignment(Qt::AlignRight);

    auto* editTitolo  = new QLineEdit;
    editTitolo->setPlaceholderText("es. BLHM: A Novel Hybrid LLM Architecture");
    auto* editAutori  = new QLineEdit;
    editAutori->setPlaceholderText("es. Mario Rossi, Luigi Verdi");
    auto* editKw      = new QLineEdit;
    editKw->setPlaceholderText("es. LLM, ontology, parallel inference");
    auto* cmbTipo     = new QComboBox;
    cmbTipo->addItems({"Preprint arXiv", "Conference Paper", "Journal Article",
                       "Workshop Paper", "Technical Report"});
    auto* cmbLingua   = new QComboBox;
    cmbLingua->addItems({"English", "Italiano"});
    auto* editAbstract = new QTextEdit;
    editAbstract->setPlaceholderText(
        "Descrivi l'idea, l'approccio e i risultati principali.\n"
        "L'AI espanderà tutto in un paper completo.");
    editAbstract->setFixedHeight(140);

    form->addRow("Titolo:",    editTitolo);
    form->addRow("Autori:",    editAutori);
    form->addRow("Keywords:", editKw);
    form->addRow("Tipo:",      cmbTipo);
    form->addRow("Lingua:",    cmbLingua);
    form->addRow("Abstract /\nDescrizione:", editAbstract);

    auto* btnRow  = new QWidget;
    auto* btnLay  = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(0, 4, 0, 0);
    auto* btnGen  = new QPushButton("\xf0\x9f\x9a\x80  Genera Paper");
    auto* btnStop = new QPushButton("\xe2\x96\xa0  Stop");
    btnGen->setObjectName("actionBtn");
    btnStop->setObjectName("actionBtn");
    btnStop->setEnabled(false);
    btnLay->addWidget(btnGen, 1);
    btnLay->addWidget(btnStop);
    form->addRow("", btnRow);

    formScroll->setWidget(formW);

    /* ── pannello destra: output ── */
    auto* outW   = new QWidget;
    auto* outLay = new QVBoxLayout(outW);
    outLay->setContentsMargins(0, 0, 0, 0);
    outLay->setSpacing(4);

    auto* outEdit = new QTextEdit;
    outEdit->setReadOnly(false);
    outEdit->setPlaceholderText(
        "L'output del paper apparirà qui.\n"
        "Puoi modificarlo dopo la generazione.");
    outEdit->setFont(QFont("Monospace", 10));

    outLay->addWidget(makeOutputBar(outEdit, "Paper Scientifico", page));
    outLay->addWidget(outEdit, 1);

    hlay->addWidget(formScroll);
    hlay->addWidget(outW, 1);

    /* ── connessioni ── */
    auto doGenera = [=]{
        const QString titolo = editTitolo->text().trimmed();
        if (titolo.isEmpty()) {
            outEdit->setPlainText(
                "\xe2\x9d\x8c  Inserisci almeno il titolo del paper.");
            return;
        }
        const QString lingua = cmbLingua->currentText();
        const QString sys =
            "You are an expert scientific writer. "
            "Generate a complete, well-structured academic paper in "
            + lingua +
            " suitable for submission to arXiv. "
            "Include all standard sections: Abstract, 1. Introduction, "
            "2. Related Work, 3. Methodology, 4. Experiments & Results, "
            "5. Discussion, 6. Conclusion, References. "
            "Use precise academic language. Include relevant mathematical "
            "notation (LaTeX-style). Make the paper original and coherent. "
            "Cite related real works where appropriate.";
        const QString msg =
            "Title: " + titolo + "\n"
            "Authors: " + editAutori->text().trimmed() + "\n"
            "Keywords: " + editKw->text().trimmed() + "\n"
            "Type: " + cmbTipo->currentText() + "\n\n"
            "Abstract / Initial description:\n" +
            editAbstract->toPlainText().trimmed();
        avvia(sys, msg, outEdit, btnGen, btnStop);
    };
    connect(btnGen,  &QPushButton::clicked, page, doGenera);
    connect(btnStop, &QPushButton::clicked, m_ai, &AiClient::abort);

    return page;
}

/* ─────────────────────────────────────────────────────────────────
   buildBrevettoTab
   ───────────────────────────────────────────────────────────────── */
QWidget* RicercaPage::buildBrevettoTab()
{
    auto* page  = new QWidget;
    auto* hlay  = new QHBoxLayout(page);
    hlay->setContentsMargins(8, 8, 8, 8);
    hlay->setSpacing(10);

    /* ── form ── */
    auto* formScroll = new QScrollArea;
    formScroll->setFrameShape(QFrame::NoFrame);
    formScroll->setWidgetResizable(true);
    formScroll->setMaximumWidth(380);

    auto* formW = new QWidget;
    auto* form  = new QFormLayout(formW);
    form->setSpacing(8);
    form->setLabelAlignment(Qt::AlignRight);

    auto* editTitolo   = new QLineEdit;
    editTitolo->setPlaceholderText(
        "es. Sistema ibrido di inferenza ontologica per LLM");
    auto* editInventori = new QLineEdit;
    editInventori->setPlaceholderText("es. Mario Rossi (IT)");
    auto* cmbIpc = new QComboBox;
    cmbIpc->setEditable(true);
    cmbIpc->addItems({
        "G06N  — Computer Science / AI",
        "G06F  — Electric Digital Data Processing",
        "H04L  — Transmission of Digital Information",
        "A61B  — Medical Devices / Diagnosis",
        "B60W  — Vehicles / Control Systems",
        "C12N  — Biology / Microbiology",
        "F01D  — Mechanical Engineering / Machines",
        "H01L  — Semiconductor Devices",
        "G01N  — Chemical / Physical Analysis",
        "E04B  — Building / Construction",
    });
    auto* editProblema = new QLineEdit;
    editProblema->setPlaceholderText(
        "es. Ridurre la complessità computazionale O(n²) "
        "dei transformer per query ontologiche");
    auto* editDesc = new QTextEdit;
    editDesc->setPlaceholderText(
        "Descrivi l'invenzione in dettaglio:\n"
        "come funziona, cosa la distingue dallo stato dell'arte,\n"
        "applicazioni pratiche.");
    editDesc->setFixedHeight(140);

    form->addRow("Titolo:", editTitolo);
    form->addRow("Inventori:", editInventori);
    form->addRow("IPC:", cmbIpc);
    form->addRow("Problema\nrisolto:", editProblema);
    form->addRow("Descrizione\ninvenzione:", editDesc);

    auto* btnRow  = new QWidget;
    auto* btnLay  = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(0, 4, 0, 0);
    auto* btnGen  = new QPushButton("\xf0\x9f\x94\x8f  Genera Brevetto");
    auto* btnStop = new QPushButton("\xe2\x96\xa0  Stop");
    btnGen->setObjectName("actionBtn");
    btnStop->setObjectName("actionBtn");
    btnStop->setEnabled(false);
    btnLay->addWidget(btnGen, 1);
    btnLay->addWidget(btnStop);
    form->addRow("", btnRow);
    formScroll->setWidget(formW);

    /* ── output ── */
    auto* outW   = new QWidget;
    auto* outLay = new QVBoxLayout(outW);
    outLay->setContentsMargins(0, 0, 0, 0);
    outLay->setSpacing(4);

    auto* outEdit = new QTextEdit;
    outEdit->setReadOnly(false);
    outEdit->setPlaceholderText(
        "Il testo del brevetto apparirà qui.\n"
        "Sezioni: Titolo, Campo, Background, Sommario, "
        "Descrizione dettagliata, Rivendicazioni, Abstract.");
    outEdit->setFont(QFont("Monospace", 10));

    outLay->addWidget(makeOutputBar(outEdit, "Brevetto", page));
    outLay->addWidget(outEdit, 1);

    hlay->addWidget(formScroll);
    hlay->addWidget(outW, 1);

    /* ── connessioni ── */
    connect(btnGen, &QPushButton::clicked, page, [=]{
        const QString titolo = editTitolo->text().trimmed();
        if (titolo.isEmpty()) {
            outEdit->setPlainText(
                "\xe2\x9d\x8c  Inserisci il titolo dell'invenzione.");
            return;
        }
        const QString sys =
            "Sei un esperto brevettuale internazionale (PCT/EPO/USPTO). "
            "Genera un documento brevettuale completo in italiano con le "
            "seguenti sezioni obbligatorie:\n"
            "TITOLO DELL'INVENZIONE\n"
            "CAMPO DELL'INVENZIONE\n"
            "BACKGROUND DELL'ARTE NOTA\n"
            "SOMMARIO DELL'INVENZIONE\n"
            "DESCRIZIONE DETTAGLIATA\n"
            "RIVENDICAZIONI (almeno 5: indipendenti e dipendenti)\n"
            "ABSTRACT (max 150 parole)\n\n"
            "Usa il linguaggio tecnico-legale dei brevetti. "
            "Le rivendicazioni devono essere precise, numerate e strutturate "
            "correttamente secondo le norme EPO.";
        const QString msg =
            "Titolo: " + titolo + "\n"
            "Inventori: " + editInventori->text().trimmed() + "\n"
            "Classificazione IPC: " + cmbIpc->currentText() + "\n"
            "Problema da risolvere: " + editProblema->text().trimmed() + "\n\n"
            "Descrizione dell'invenzione:\n" +
            editDesc->toPlainText().trimmed();
        avvia(sys, msg, outEdit, btnGen, btnStop);
    });
    connect(btnStop, &QPushButton::clicked, m_ai, &AiClient::abort);

    return page;
}

/* ─────────────────────────────────────────────────────────────────
   buildDocTecnicoTab
   ───────────────────────────────────────────────────────────────── */
QWidget* RicercaPage::buildDocTecnicoTab()
{
    auto* page  = new QWidget;
    auto* hlay  = new QHBoxLayout(page);
    hlay->setContentsMargins(8, 8, 8, 8);
    hlay->setSpacing(10);

    /* ── form ── */
    auto* formScroll = new QScrollArea;
    formScroll->setFrameShape(QFrame::NoFrame);
    formScroll->setWidgetResizable(true);
    formScroll->setMaximumWidth(380);

    auto* formW = new QWidget;
    auto* form  = new QFormLayout(formW);
    form->setSpacing(8);
    form->setLabelAlignment(Qt::AlignRight);

    auto* editNome   = new QLineEdit;
    editNome->setPlaceholderText(
        "es. BLHM — Documento Tecnico v1.0");
    auto* editAutore = new QLineEdit;
    editAutore->setPlaceholderText("es. wildlux");
    auto* editVers   = new QLineEdit;
    editVers->setPlaceholderText("es. 1.0");
    auto* cmbLingua  = new QComboBox;
    cmbLingua->addItems({"Italiano", "English"});
    auto* cmbStile   = new QComboBox;
    cmbStile->addItems({
        "Tecnico formale (con formule e tabelle)",
        "Tecnico semplificato (divulgativo)",
        "Standard industriale (ISO/IEC style)",
        "Accademico (con citazioni)",
    });
    auto* editDesc   = new QTextEdit;
    editDesc->setPlaceholderText(
        "Descrivi il progetto/sistema:\n"
        "- Cosa fa\n- Come funziona\n"
        "- Dati e risultati misurati\n"
        "- Specifiche tecniche rilevanti");
    editDesc->setFixedHeight(150);

    form->addRow("Nome progetto:", editNome);
    form->addRow("Autore:", editAutore);
    form->addRow("Versione:", editVers);
    form->addRow("Lingua:", cmbLingua);
    form->addRow("Stile:", cmbStile);
    form->addRow("Descrizione\nprogetto:", editDesc);

    auto* btnRow  = new QWidget;
    auto* btnLay  = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(0, 4, 0, 0);
    auto* btnGen  = new QPushButton(
        "\xf0\x9f\x93\x8b  Genera Documento");
    auto* btnStop = new QPushButton("\xe2\x96\xa0  Stop");
    btnGen->setObjectName("actionBtn");
    btnStop->setObjectName("actionBtn");
    btnStop->setEnabled(false);
    btnLay->addWidget(btnGen, 1);
    btnLay->addWidget(btnStop);
    form->addRow("", btnRow);
    formScroll->setWidget(formW);

    /* ── output ── */
    auto* outW   = new QWidget;
    auto* outLay = new QVBoxLayout(outW);
    outLay->setContentsMargins(0, 0, 0, 0);
    outLay->setSpacing(4);

    auto* outEdit = new QTextEdit;
    outEdit->setReadOnly(false);
    outEdit->setPlaceholderText(
        "Il documento tecnico apparirà qui.\n"
        "Sezioni: Sommario Esecutivo, Introduzione, Architettura, "
        "Specifiche, Calcoli, Risultati, Limitazioni, Conclusioni.");
    outEdit->setFont(QFont("Monospace", 10));

    outLay->addWidget(makeOutputBar(outEdit, "Documento Tecnico", page));
    outLay->addWidget(outEdit, 1);

    hlay->addWidget(formScroll);
    hlay->addWidget(outW, 1);

    /* ── connessioni ── */
    connect(btnGen, &QPushButton::clicked, page, [=]{
        const QString nome = editNome->text().trimmed();
        if (nome.isEmpty()) {
            outEdit->setPlainText(
                "\xe2\x9d\x8c  Inserisci il nome del progetto.");
            return;
        }
        const QString lingua = cmbLingua->currentText();
        const QString sys =
            "Sei un tecnico specializzato nella redazione di documenti "
            "tecnici professionali. Genera un documento tecnico completo "
            "in " + lingua + " con lo stile: " +
            cmbStile->currentText() + ".\n"
            "Struttura obbligatoria:\n"
            "1. Sommario Esecutivo\n"
            "2. Introduzione e Scopo\n"
            "3. Architettura / Design del Sistema\n"
            "4. Specifiche Tecniche\n"
            "5. Analisi e Calcoli (con formule dove pertinente)\n"
            "6. Risultati Sperimentali o di Simulazione\n"
            "7. Limitazioni e Rischi\n"
            "8. Conclusioni\n\n"
            "Includi tabelle numeriche, formule e analisi quantitative "
            "dove possibile. Sii preciso e dettagliato.";
        const QString vers = editVers->text().trimmed();
        const QString msg =
            "Progetto: " + nome + (vers.isEmpty() ? "" : " v" + vers) + "\n"
            "Autore: " + editAutore->text().trimmed() + "\n"
            "Data: " + QDate::currentDate().toString("yyyy-MM-dd") + "\n\n"
            "Descrizione del progetto / sistema:\n" +
            editDesc->toPlainText().trimmed();
        avvia(sys, msg, outEdit, btnGen, btnStop);
    });
    connect(btnStop, &QPushButton::clicked, m_ai, &AiClient::abort);

    return page;
}

/* ─────────────────────────────────────────────────────────────────
   esportaPdf — stampa il contenuto del QTextEdit in PDF
   ───────────────────────────────────────────────────────────────── */
void RicercaPage::esportaPdf(QTextEdit* editor,
                              const QString& titolo, QWidget* parent)
{
    const QString testo = editor->toPlainText().trimmed();
    if (testo.isEmpty()) return;

    const QString defaultName =
        titolo.simplified().replace(' ', '_') + "_" +
        QDate::currentDate().toString("yyyyMMdd") + ".pdf";
    const QString path = QFileDialog::getSaveFileName(
        parent, "Salva PDF", QDir::homePath() + "/" + defaultName,
        "PDF (*.pdf)");
    if (path.isEmpty()) return;

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(path);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setPageMargins(QMarginsF(20, 20, 20, 20), QPageLayout::Millimeter);

    QTextDocument doc;
    doc.setPlainText(testo);
    doc.setDefaultFont(QFont("Helvetica", 10));
    doc.print(&printer);

    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

/* ─────────────────────────────────────────────────────────────────
   salvaMarkdown — salva come .md (o .txt) per editing esterno
   ───────────────────────────────────────────────────────────────── */
void RicercaPage::salvaMarkdown(QTextEdit* editor,
                                 const QString& titolo, QWidget* parent)
{
    const QString testo = editor->toPlainText().trimmed();
    if (testo.isEmpty()) return;

    const QString defaultName =
        titolo.simplified().replace(' ', '_') + "_" +
        QDate::currentDate().toString("yyyyMMdd") + ".md";
    const QString path = QFileDialog::getSaveFileName(
        parent, "Salva Markdown",
        QDir::homePath() + "/" + defaultName,
        "Markdown (*.md);;Testo (*.txt)");
    if (path.isEmpty()) return;

    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QTextStream s(&f);
        s << testo;
    }
}

/* ══════════════════════════════════════════════════════════════
   Helper science: popola combo modelli
   ══════════════════════════════════════════════════════════════ */
void RicercaPage::sciPopulateModels(QComboBox* combo)
{
    combo->clear();
    const QString cur = m_ai->model();
    if (!cur.isEmpty()) combo->addItem(cur, cur);
    combo->setCurrentIndex(0);
    m_ai->fetchModels();
}

/* ══════════════════════════════════════════════════════════════
   Helper science: lancia AI con streaming e gestisce ExecBtn
   ══════════════════════════════════════════════════════════════ */
void RicercaPage::avviaSci(const QString& sys, const QString& userMsg,
                            QTextEdit* out, QPushButton* runBtn, QPushButton* stopBtn,
                            QComboBox* modelCombo,
                            QPushButton* execBtn, QString* codeRef, QLabel* statusLbl)
{
    if (m_ai->busy()) {
        out->append("\xe2\x9a\xa0  AI occupata, attendi o premi Stop.");
        return;
    }
    if (userMsg.trimmed().isEmpty()) {
        out->append("\xe2\x9a\xa0  Inserisci la richiesta prima di eseguire.");
        return;
    }

    if (modelCombo && modelCombo->count() > 0) {
        const QString sel = modelCombo->currentData().toString();
        if (!sel.isEmpty() && sel != m_ai->model())
            m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), sel);
    }


    m_sciOut        = out;
    m_sciRunBtn     = runBtn;
    m_sciStopBtn    = stopBtn;
    m_sciExecBtn    = execBtn;
    m_sciCodeRef    = codeRef;
    m_sciStatusLbl  = statusLbl;
    m_sciModelCombo = modelCombo;
    m_sciSys        = sys;
    m_sciUserMsg    = userMsg;

    QObject::disconnect(m_sciTokenConn);
    QObject::disconnect(m_sciFinishedConn);
    QObject::disconnect(m_sciErrorConn);
    m_sciTokenConn    = connect(m_ai, &AiClient::token,    this, &RicercaPage::onSciToken);
    m_sciFinishedConn = connect(m_ai, &AiClient::finished, this, &RicercaPage::onSciFinished);
    m_sciErrorConn    = connect(m_ai, &AiClient::error,    this, &RicercaPage::onSciError);

    runBtn->setEnabled(false);
    if (m_sciProgress) m_sciProgress->setVisible(true);
    stopBtn->setEnabled(true);
    out->append(
        "\n\xf0\x9f\x94\x84  Generazione in corso...\n"
        + QString(40, QChar(0x2500)));

    m_ai->chat(sys, userMsg);
}

/* ══════════════════════════════════════════════════════════════
   System prompts — Cytoscape MCP (bioinformatica)
   ══════════════════════════════════════════════════════════════ */
namespace {
static const char* kCytoSysR[] = {
    "Sei un esperto di Cytoscape e py2cytoscape. "
    "Genera SOLO codice Python che usa la CyREST API (localhost:1234). "
    "import requests; BASE='http://localhost:1234/v1'. "
    "Usa endpoints /networks, /styles, /layouts, /commands. "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```.",

    "Sei un esperto di bioinformatica e Cytoscape. "
    "Genera SOLO codice Python CyREST per creare un grafo di rete proteica/biologica. "
    "Includi nodi (proteine/geni) e archi (interazioni). Rispondi SOLO con codice Python tra ``` e ```.",

    "Sei un esperto di analisi reti e Cytoscape. "
    "Genera SOLO codice Python CyREST per calcolare centralit\xc3\xa0, clustering, componenti connesse. "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```.",

    "Sei un esperto di Cytoscape. "
    "Genera SOLO codice Python CyREST per applicare un layout (force-directed, hierarchical, ecc.) "
    "e cambiare lo stile visivo (colori, dimensioni nodi). Rispondi SOLO con codice Python tra ``` e ```.",

    "Sei un esperto di Cytoscape. "
    "Genera SOLO codice Python CyREST libero. "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```.",

    nullptr
};
static const char* kCytoActionsR[] = {
    "\xf0\x9f\x94\xac  Nuova rete",
    "\xf0\x9f\xa7\xac  Rete biologica/PPI",
    "\xf0\x9f\x93\x88  Analisi centralit\xc3\xa0",
    "\xf0\x9f\x8e\xa8  Layout & stile",
    "\xf0\x9f\x90\x8d  Script libero",
    nullptr
};

static const char* kRDKitSysR[] = {
    "Sei un esperto di RDKit. "
    "Genera SOLO codice Python RDKit per analizzare una molecola SMILES: "
    "peso molecolare, formula, anelli, stereocentri, descrittori chimici. "
    "Rispondi SOLO con codice Python tra ``` e ```.",

    "Sei un esperto di RDKit e similarit\xc3\xa0 molecolare. "
    "Genera SOLO codice Python RDKit per calcolare fingerprint Morgan e Tanimoto similarity. "
    "Rispondi SOLO con codice Python tra ``` e ```.",

    "Sei un esperto di RDKit. "
    "Genera SOLO codice Python RDKit per ottimizzare la geometria 3D (MMFF94) "
    "e salvare in SDF. Rispondi SOLO con codice Python tra ``` e ```.",

    "Sei un esperto di RDKit. "
    "Genera SOLO codice Python RDKit libero. "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```.",

    nullptr
};
static const char* kRDKitActionsR[] = {
    "\xe2\x9a\x97  Analisi molecola",
    "\xf0\x9f\x94\x8d  Similarit\xc3\xa0 Tanimoto",
    "\xf0\x9f\x94\xac  Ottimizza geometria 3D",
    "\xf0\x9f\x90\x8d  Script libero",
    nullptr
};

static const char* kBioSysR[] = {
    "Sei un esperto di bioinformatica e pipeline Bioconda. "
    "Genera SOLO script Bash o Snakemake eseguibili. "
    "Usa tool Bioconda: bwa, samtools, gatk, blast, fastqc, trimmomatic, bowtie2. "
    "Rispondi SOLO con il blocco script tra ``` e ```.",

    "Sei un esperto di analisi genomica. "
    "Genera SOLO script Bash per una pipeline di allineamento NGS: "
    "FastQC \xe2\x86\x92 Trimmomatic \xe2\x86\x92 BWA mem \xe2\x86\x92 Samtools sort/index. "
    "Rispondi SOLO con codice Bash tra ``` e ```.",

    "Sei un esperto di BLAST e banche dati genomiche. "
    "Genera SOLO script Bash per eseguire BLAST (blastn/blastp) su sequenze FASTA. "
    "Rispondi SOLO con codice Bash tra ``` e ```.",

    "Sei un esperto di variant calling GATK. "
    "Genera SOLO script Bash per variant calling: "
    "HaplotypeCaller \xe2\x86\x92 GenotypeGVCFs \xe2\x86\x92 VQSR/hard filter. "
    "Rispondi SOLO con codice Bash tra ``` e ```.",

    "Sei un esperto di bioinformatica Python (Biopython). "
    "Genera SOLO codice Python con Biopython per parsing FASTA/GenBank/PDB. "
    "Rispondi SOLO con codice Python tra ``` e ```.",

    nullptr
};
static const char* kBioActionsR[] = {
    "\xf0\x9f\x8c\xbf  Pipeline NGS alignment",
    "\xf0\x9f\x94\xac  BLAST search",
    "\xf0\x9f\xa7\xac  Variant calling GATK",
    "\xf0\x9f\x90\x8d  Biopython parsing",
    "\xf0\x9f\x94\xa7  Script libero",
    nullptr
};
} // namespace

/* ── Helper: esegui script Python/Bash generato dall'AI ── */
static void runSciScript(const QString& code, bool isBash,
                         QLabel* statusLbl, QPushButton* execBtn,
                         QTextEdit* output, QProcess*& procRef, QObject* parent)
{
    if (code.isEmpty()) return;
    const QString suffix  = isBash ? ".sh" : ".py";
    const QString tmpPath = P::safeTempPath() + "/prismalux_bio_script" + suffix;
    QFile f(tmpPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        statusLbl->setText("\xe2\x9d\x8c  Impossibile creare script temporaneo");
        return;
    }
    f.write(code.toUtf8());
    f.close();

    if (!procRef) {
        procRef = new QProcess(parent);
        procRef->setProcessChannelMode(QProcess::MergedChannels);
        QObject::connect(procRef, &QProcess::readyRead, parent, [procRef, output](){
            output->append(QString::fromUtf8(procRef->readAll()).trimmed());
        });
        QObject::connect(procRef,
            QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            parent, [statusLbl, execBtn](int code2, QProcess::ExitStatus){
            statusLbl->setText(code2 == 0
                ? "\xe2\x9c\x85  Completato"
                : "\xe2\x9d\x8c  Terminato con errore");
            execBtn->setEnabled(true);
        });
    }
    execBtn->setEnabled(false);
    statusLbl->setText("\xf0\x9f\x94\x84  Esecuzione...");
    if (isBash) {
        const QString bash = QStandardPaths::findExecutable("bash");
        procRef->start(bash.isEmpty() ? "bash" : bash, {tmpPath});
    } else {
        procRef->start(P::findPython(), {tmpPath});
    }
    if (procRef->state() == QProcess::NotRunning)
        statusLbl->setText("\xe2\x9d\x8c  Interprete non trovato");
}

/* ══════════════════════════════════════════════════════════════
   buildCercaLetteraturaTab — ricerca paper/brevetti su database online
   Sorgenti: arXiv, Semantic Scholar, USPTO
   ══════════════════════════════════════════════════════════════ */
QWidget* RicercaPage::buildCercaLetteraturaTab()
{
    auto* w   = new QWidget(this);
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(6);

    auto* desc = new QLabel(
        "\xf0\x9f\x94\x8d  <b>Cerca Paper e Brevetti</b> \xe2\x80\x94 "
        "Ricerca su <b>arXiv</b>, <b>Semantic Scholar</b> e <b>USPTO</b> "
        "senza account o API key. Poi analizza i risultati con AI.", w);
    desc->setObjectName("hintLabel");
    desc->setWordWrap(true);
    desc->setTextFormat(Qt::RichText);
    lay->addWidget(desc);

    /* Barra ricerca */
    auto* row = new QWidget(w);
    auto* rl  = new QHBoxLayout(row);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(6);

    m_litQuery = new QLineEdit(w);
    m_litQuery->setPlaceholderText(
        "Es: quantum computing error correction / battery cathode material ...");

    m_litSource = new QComboBox(w);
    m_litSource->addItem("\xf0\x9f\x93\x84  arXiv",           "arxiv");
    m_litSource->addItem("\xf0\x9f\x94\xac  Semantic Scholar", "semantic");
    m_litSource->addItem("\xf0\x9f\x94\x8f  USPTO Brevetti",   "uspto");
    m_litSource->setFixedWidth(180);

    m_litSearchBtn = new QPushButton("\xf0\x9f\x94\x8d  Cerca", w);
    m_litSearchBtn->setObjectName("actionBtn");
    m_litSearchBtn->setFixedWidth(90);

    rl->addWidget(m_litQuery, 1);
    rl->addWidget(m_litSource);
    rl->addWidget(m_litSearchBtn);
    lay->addWidget(row);

    m_litStatus = new QLabel("", w);
    m_litStatus->setObjectName("hintLabel");
    lay->addWidget(m_litStatus);

    m_litResults = new QTextEdit(w);
    m_litResults->setReadOnly(true);
    m_litResults->setObjectName("outputView");
    m_litResults->setPlaceholderText("I risultati appariranno qui...");
    lay->addWidget(m_litResults, 1);

    m_litAiBtn = new QPushButton(
        "\xf0\x9f\xa4\x96  Analizza con AI", w);
    m_litAiBtn->setObjectName("actionBtn");
    m_litAiBtn->setEnabled(false);
    lay->addWidget(m_litAiBtn);

    m_litNet = new QNetworkAccessManager(this);

    /* ── ricerca ── */
    connect(m_litSearchBtn, &QPushButton::clicked, this, &RicercaPage::onLitSearchClicked);

    /* Enter nella query = cerca */
    connect(m_litQuery, &QLineEdit::returnPressed,
            m_litSearchBtn, &QPushButton::click);

    /* ── Analizza con AI ── */
    connect(m_litAiBtn, &QPushButton::clicked, this, &RicercaPage::onLitAiClicked);

    return w;
}

/* ══════════════════════════════════════════════════════════════
   buildCytoscapeTab — analisi reti biologiche e sociali
   ══════════════════════════════════════════════════════════════ */
QWidget* RicercaPage::buildCytoscapeTab()
{
    auto* w   = new QWidget(this);
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(6);

    auto* descLbl = new QLabel(
        "\xf0\x9f\x94\xac  <i>Cytoscape \xe2\x80\x94 Piattaforma open-source per la visualizzazione e l\xe2\x80\x99" "analisi "
        "di reti biologiche e molecolari. Usato in bioinformatica, proteomica e genomica per mappare interazioni proteina-proteina e pathways.</i>", w);
    descLbl->setObjectName("hintLabel");
    descLbl->setTextFormat(Qt::RichText);
    descLbl->setWordWrap(true);
    lay->addWidget(descLbl);

    auto* connRow = new QWidget(w);
    auto* connLay = new QHBoxLayout(connRow);
    connLay->setContentsMargins(0, 0, 0, 0);
    connLay->setSpacing(8);

    auto* lbl = new QLabel("CyREST:", connRow);
    lbl->setObjectName("hintLabel");
    m_cytoHostEdit = new QLineEdit("localhost:1234", connRow);
    m_cytoHostEdit->setFixedWidth(150);
    auto* pingBtn = new QPushButton("\xf0\x9f\x94\x97  Verifica", connRow);
    pingBtn->setObjectName("actionBtn");
    pingBtn->setFixedWidth(100);
    m_cytoStatusLbl = new QLabel("\xe2\x9a\xaa  Non connesso", connRow);
    m_cytoStatusLbl->setObjectName("hintLabel");
    m_cytoExecBtn = new QPushButton("\xf0\x9f\x94\xac  Esegui su Cytoscape", connRow);
    m_cytoExecBtn->setObjectName("actionBtn");
    m_cytoExecBtn->setFixedWidth(180);
    m_cytoExecBtn->setEnabled(false);

    connLay->addWidget(lbl);
    connLay->addWidget(m_cytoHostEdit);
    connLay->addWidget(pingBtn);
    connLay->addWidget(m_cytoStatusLbl, 1);
    connLay->addWidget(m_cytoExecBtn);
    lay->addWidget(connRow);

    auto* hintLbl = new QLabel(
        "\xf0\x9f\x94\xac <b>Cytoscape MCP:</b> analisi reti biologiche e sociali. "
        "Avvia Cytoscape (abilita CyREST su porta 1234).<br>"
        "Installa: <code>pip install py2cytoscape requests</code>", w);
    hintLbl->setObjectName("hintLabel");
    hintLbl->setWordWrap(true);
    lay->addWidget(hintLbl);

    auto* toolRow = new QWidget(w);
    auto* toolLay = new QHBoxLayout(toolRow);
    toolLay->setContentsMargins(0, 0, 0, 0);
    toolLay->setSpacing(8);
    m_cytoAction = new QComboBox(toolRow);
    for (int i = 0; kCytoActionsR[i]; i++)
        m_cytoAction->addItem(QString::fromUtf8(kCytoActionsR[i]));
    m_cytoModel = new QComboBox(toolRow);
    m_cytoModel->setMinimumWidth(180);
    sciPopulateModels(m_cytoModel);
    toolLay->addWidget(new QLabel("Analisi:", toolRow));
    toolLay->addWidget(m_cytoAction, 1);
    toolLay->addWidget(new QLabel("Modello:", toolRow));
    toolLay->addWidget(m_cytoModel, 1);
    lay->addWidget(toolRow);

    m_cytoInput = new QTextEdit(w);
    m_cytoInput->setPlaceholderText(
        "Descrivi la rete da analizzare...\n"
        "Es: 'Crea un grafo di interazione proteica con 10 proteine e mostra i hub'\n"
        "Es: 'Analizza la centralit\xc3\xa0 di betweenness nella rete caricata'");
    m_cytoInput->setFixedHeight(80);
    lay->addWidget(m_cytoInput);

    auto* btnRow = new QWidget(w);
    auto* btnLay = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(0, 0, 0, 0);
    m_cytoRunBtn  = new QPushButton("\xf0\x9f\xa4\x96  Genera script Cytoscape", btnRow);
    m_cytoRunBtn->setObjectName("actionBtn");
    m_cytoStopBtn = new QPushButton("\xe2\x8f\xb9  Stop", btnRow);
    m_cytoStopBtn->setObjectName("actionBtn");
    m_cytoStopBtn->setProperty("danger", true);
    m_cytoStopBtn->setEnabled(false);
    btnLay->addWidget(m_cytoRunBtn);
    btnLay->addWidget(m_cytoStopBtn);
    btnLay->addStretch();
    lay->addWidget(btnRow);

    m_cytoOutput = new QTextEdit(w);
    m_cytoOutput->setReadOnly(true);
    m_cytoOutput->setObjectName("outputView");
    m_cytoOutput->setPlaceholderText("Script Python CyREST appare qui...");
    lay->addWidget(m_cytoOutput, 1);

    connect(pingBtn, &QPushButton::clicked, this, &RicercaPage::onCytoPingClicked);

    connect(m_cytoExecBtn, &QPushButton::clicked, this, &RicercaPage::onCytoExecClicked);
    connect(m_cytoRunBtn, &QPushButton::clicked, this, &RicercaPage::onCytoRunClicked);
    connect(m_cytoStopBtn, &QPushButton::clicked, this, &RicercaPage::onCytoStopClicked);

    return w;
}

/* ══════════════════════════════════════════════════════════════
   buildRDKitTab — chemioinformatica Python
   ══════════════════════════════════════════════════════════════ */
QWidget* RicercaPage::buildRDKitTab()
{
    auto* w   = new QWidget(this);
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(6);

    /* ── Banner MedGemma ── */
    auto* medgemmaFrame = new QFrame(w);
    medgemmaFrame->setFrameShape(QFrame::StyledPanel);
    medgemmaFrame->setStyleSheet(
        "QFrame { background:#0d2a1a; border:2px solid #2e7d52;"
        " border-radius:6px; padding:2px; }");
    auto* medgemmaLay = new QHBoxLayout(medgemmaFrame);
    medgemmaLay->setContentsMargins(10, 8, 10, 8);
    auto* medgemmaLbl = new QLabel(
        "\xf0\x9f\xa7\xac <b>Per analisi bioinformatiche usa modelli specializzati:</b> "
        "<code>ollama run medgemma</code> | "
        "<code>ollama run medgemma1.5</code> | "
        "<code>ollama run functiongemma</code> "
        "<span style='color:#7dcf9a;'>"
        "(modelli Google specializzati in medicina e biologia)</span>",
        medgemmaFrame);
    medgemmaLbl->setTextFormat(Qt::RichText);
    medgemmaLbl->setWordWrap(true);
    medgemmaLbl->setStyleSheet("color:#c8f0d8; background:transparent; border:none;");
    medgemmaLay->addWidget(medgemmaLbl);
    lay->addWidget(medgemmaFrame);

    auto* descLbl = new QLabel(
        "\xf0\x9f\x94\xac  <i>RDKit \xe2\x80\x94 Libreria open-source di chemioinformatica per la manipolazione, "
        "analisi e disegno di molecole. Usata in drug discovery, QSAR, fingerprinting molecolare e chimica computazionale.</i>", w);
    descLbl->setObjectName("hintLabel");
    descLbl->setTextFormat(Qt::RichText);
    descLbl->setWordWrap(true);
    lay->addWidget(descLbl);

    auto* connRow = new QWidget(w);
    auto* connLay = new QHBoxLayout(connRow);
    connLay->setContentsMargins(0, 0, 0, 0);
    connLay->setSpacing(8);
    m_rdkitStatusLbl = new QLabel("\xe2\x9a\xaa  RDKit locale", connRow);
    m_rdkitStatusLbl->setObjectName("hintLabel");
    auto* checkBtn = new QPushButton("\xf0\x9f\x94\x8d  Verifica rdkit", connRow);
    checkBtn->setObjectName("actionBtn");
    checkBtn->setFixedWidth(130);
    m_rdkitExecBtn = new QPushButton("\xf0\x9f\x94\xac  Esegui script RDKit", connRow);
    m_rdkitExecBtn->setObjectName("actionBtn");
    m_rdkitExecBtn->setFixedWidth(170);
    m_rdkitExecBtn->setEnabled(false);
    connLay->addWidget(m_rdkitStatusLbl, 1);
    connLay->addWidget(checkBtn);
    connLay->addWidget(m_rdkitExecBtn);
    lay->addWidget(connRow);

    auto* hintLbl = new QLabel(
        "\xf0\x9f\xa7\xaa <b>RDKit MCP:</b> chemioinformatica Python. "
        "Installa con: <code>pip install rdkit</code> "
        "o <code>conda install -c conda-forge rdkit</code>", w);
    hintLbl->setObjectName("hintLabel");
    hintLbl->setWordWrap(true);
    lay->addWidget(hintLbl);

    auto* toolRow = new QWidget(w);
    auto* toolLay = new QHBoxLayout(toolRow);
    toolLay->setContentsMargins(0, 0, 0, 0);
    toolLay->setSpacing(8);
    m_rdkitAction = new QComboBox(toolRow);
    for (int i = 0; kRDKitActionsR[i]; i++)
        m_rdkitAction->addItem(QString::fromUtf8(kRDKitActionsR[i]));
    m_rdkitModel = new QComboBox(toolRow);
    m_rdkitModel->setMinimumWidth(180);
    sciPopulateModels(m_rdkitModel);
    toolLay->addWidget(new QLabel("Analisi:", toolRow));
    toolLay->addWidget(m_rdkitAction, 1);
    toolLay->addWidget(new QLabel("Modello:", toolRow));
    toolLay->addWidget(m_rdkitModel, 1);
    lay->addWidget(toolRow);

    m_rdkitInput = new QTextEdit(w);
    m_rdkitInput->setPlaceholderText(
        "Descrivi la molecola o l'analisi da eseguire...\n"
        "Es: 'Calcola MW, LogP e HBD/HBA per la caffeina (SMILES: Cn1cnc2c1c(=O)n(c(=O)n2C)C)'\n"
        "Es: 'Trova la similarit\xc3\xa0 Tanimoto tra aspirina e ibuprofene'");
    m_rdkitInput->setFixedHeight(80);
    lay->addWidget(m_rdkitInput);

    auto* btnRow = new QWidget(w);
    auto* btnLay = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(0, 0, 0, 0);
    m_rdkitRunBtn  = new QPushButton("\xf0\x9f\xa4\x96  Genera script RDKit", btnRow);
    m_rdkitRunBtn->setObjectName("actionBtn");
    m_rdkitStopBtn = new QPushButton("\xe2\x8f\xb9  Stop", btnRow);
    m_rdkitStopBtn->setObjectName("actionBtn");
    m_rdkitStopBtn->setProperty("danger", true);
    m_rdkitStopBtn->setEnabled(false);
    btnLay->addWidget(m_rdkitRunBtn);
    btnLay->addWidget(m_rdkitStopBtn);
    btnLay->addStretch();
    lay->addWidget(btnRow);

    m_rdkitOutput = new QTextEdit(w);
    m_rdkitOutput->setReadOnly(true);
    m_rdkitOutput->setObjectName("outputView");
    m_rdkitOutput->setPlaceholderText("Script Python RDKit appare qui...");
    lay->addWidget(m_rdkitOutput, 1);

    connect(checkBtn, &QPushButton::clicked, this, &RicercaPage::onRdkitCheckClicked);

    connect(m_rdkitExecBtn, &QPushButton::clicked, this, &RicercaPage::onRdkitExecClicked);
    connect(m_rdkitRunBtn, &QPushButton::clicked, this, &RicercaPage::onRdkitRunClicked);
    connect(m_rdkitStopBtn, &QPushButton::clicked, this, &RicercaPage::onRdkitStopClicked);

    return w;
}

/* ══════════════════════════════════════════════════════════════
   buildBiocondaTab — bioinformatica pipeline
   ══════════════════════════════════════════════════════════════ */
QWidget* RicercaPage::buildBiocondaTab()
{
    auto* w   = new QWidget(this);
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(6);

    /* ── Banner MedGemma ── */
    auto* medgemmaFrame = new QFrame(w);
    medgemmaFrame->setFrameShape(QFrame::StyledPanel);
    medgemmaFrame->setStyleSheet(
        "QFrame { background:#0d2a1a; border:2px solid #2e7d52;"
        " border-radius:6px; padding:2px; }");
    auto* medgemmaLay = new QHBoxLayout(medgemmaFrame);
    medgemmaLay->setContentsMargins(10, 8, 10, 8);
    auto* medgemmaLbl = new QLabel(
        "\xf0\x9f\xa7\xac <b>Per analisi bioinformatiche usa modelli specializzati:</b> "
        "<code>ollama run medgemma</code> | "
        "<code>ollama run medgemma1.5</code> | "
        "<code>ollama run functiongemma</code> "
        "<span style='color:#7dcf9a;'>"
        "(modelli Google specializzati in medicina e biologia)</span>",
        medgemmaFrame);
    medgemmaLbl->setTextFormat(Qt::RichText);
    medgemmaLbl->setWordWrap(true);
    medgemmaLbl->setStyleSheet("color:#c8f0d8; background:transparent; border:none;");
    medgemmaLay->addWidget(medgemmaLbl);
    lay->addWidget(medgemmaFrame);

    auto* descLbl = new QLabel(
        "\xf0\x9f\x8c\xbf  <i>Bioconda \xe2\x80\x94 Repository specializzato di software bioinformatico installabile "
        "via conda/mamba. Usato per pipeline NGS, analisi genomica, sequenziamento RNA-seq, allineamento e annotazione.</i>", w);
    descLbl->setObjectName("hintLabel");
    descLbl->setTextFormat(Qt::RichText);
    descLbl->setWordWrap(true);
    lay->addWidget(descLbl);

    auto* connRow = new QWidget(w);
    auto* connLay = new QHBoxLayout(connRow);
    connLay->setContentsMargins(0, 0, 0, 0);
    connLay->setSpacing(8);
    m_bioStatusLbl = new QLabel("\xe2\x9a\xaa  Bioconda/conda", connRow);
    m_bioStatusLbl->setObjectName("hintLabel");
    auto* checkBtn = new QPushButton("\xf0\x9f\x94\x8d  Verifica conda", connRow);
    checkBtn->setObjectName("actionBtn");
    checkBtn->setFixedWidth(130);
    m_bioExecBtn = new QPushButton("\xf0\x9f\x8c\xbf  Esegui pipeline", connRow);
    m_bioExecBtn->setObjectName("actionBtn");
    m_bioExecBtn->setFixedWidth(150);
    m_bioExecBtn->setEnabled(false);
    connLay->addWidget(m_bioStatusLbl, 1);
    connLay->addWidget(checkBtn);
    connLay->addWidget(m_bioExecBtn);
    lay->addWidget(connRow);

    auto* hintLbl = new QLabel(
        "\xf0\x9f\x8c\xbf <b>Bioconda MCP:</b> bioinformatica pipeline. "
        "Richiede conda/mamba + canale Bioconda:<br>"
        "<code>conda config --add channels bioconda</code> "
        "<code>conda config --add channels conda-forge</code><br>"
        "Installa tool: <code>conda install -c bioconda bwa samtools gatk4 blast fastqc</code>", w);
    hintLbl->setObjectName("hintLabel");
    hintLbl->setWordWrap(true);
    lay->addWidget(hintLbl);

    auto* toolRow = new QWidget(w);
    auto* toolLay = new QHBoxLayout(toolRow);
    toolLay->setContentsMargins(0, 0, 0, 0);
    toolLay->setSpacing(8);
    m_bioAction = new QComboBox(toolRow);
    for (int i = 0; kBioActionsR[i]; i++)
        m_bioAction->addItem(QString::fromUtf8(kBioActionsR[i]));
    m_bioModel = new QComboBox(toolRow);
    m_bioModel->setMinimumWidth(180);
    sciPopulateModels(m_bioModel);
    toolLay->addWidget(new QLabel("Pipeline:", toolRow));
    toolLay->addWidget(m_bioAction, 1);
    toolLay->addWidget(new QLabel("Modello:", toolRow));
    toolLay->addWidget(m_bioModel, 1);
    lay->addWidget(toolRow);

    m_bioInput = new QTextEdit(w);
    m_bioInput->setPlaceholderText(
        "Descrivi la pipeline bioinformatica da creare...\n"
        "Es: 'Pipeline di allineamento WGS: FASTQ input, output BAM sorted e indexed'\n"
        "Es: 'Cerca la sequenza ATGGCTAGCTA nel database nr con BLAST, salva risultati XML'");
    m_bioInput->setFixedHeight(80);
    lay->addWidget(m_bioInput);

    auto* btnRow = new QWidget(w);
    auto* btnLay = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(0, 0, 0, 0);
    m_bioRunBtn  = new QPushButton("\xf0\x9f\xa4\x96  Genera pipeline", btnRow);
    m_bioRunBtn->setObjectName("actionBtn");
    m_bioStopBtn = new QPushButton("\xe2\x8f\xb9  Stop", btnRow);
    m_bioStopBtn->setObjectName("actionBtn");
    m_bioStopBtn->setProperty("danger", true);
    m_bioStopBtn->setEnabled(false);
    btnLay->addWidget(m_bioRunBtn);
    btnLay->addWidget(m_bioStopBtn);
    btnLay->addStretch();
    lay->addWidget(btnRow);

    m_bioOutput = new QTextEdit(w);
    m_bioOutput->setReadOnly(true);
    m_bioOutput->setObjectName("outputView");
    m_bioOutput->setPlaceholderText("Script Bash/Python bioinformatica appare qui...");
    lay->addWidget(m_bioOutput, 1);

    connect(checkBtn, &QPushButton::clicked, this, &RicercaPage::onBioCheckClicked);

    connect(m_bioExecBtn, &QPushButton::clicked, this, &RicercaPage::onBioExecClicked);
    connect(m_bioRunBtn, &QPushButton::clicked, this, &RicercaPage::onBioRunClicked);
    connect(m_bioStopBtn, &QPushButton::clicked, this, &RicercaPage::onBioStopClicked);

    return w;
}

/* ══════════════════════════════════════════════════════════════
   buildAvogadroTab — modellazione molecolare 3D
   ══════════════════════════════════════════════════════════════ */
namespace {
static const char* kAvoSysR[] = {
    "Sei un esperto di Avogadro2 e chemioinformatica Python. "
    "Genera SOLO codice Python che usa avogadro (avogadro.core, avogadro.io, avogadro.calc). "
    "Oppure genera file molecolari (XYZ, SDF, PDB) ben formattati. "
    "Rispondi SOLO con il blocco codice tra ``` e ```.",

    "Sei un esperto di Avogadro2. "
    "Genera SOLO codice Python avogadro per caricare una molecola da SMILES o file "
    "e calcolarne energia con MMFF o UFF. Rispondi SOLO con codice Python tra ``` e ```.",

    "Sei un esperto di Avogadro2. "
    "Genera SOLO un file XYZ ben formato della molecola descritta, "
    "con coordinate ottimizzate approssimative. Rispondi SOLO con il blocco XYZ tra ``` e ```.",

    "Sei un esperto di chimica computazionale e Avogadro2. "
    "Genera SOLO codice Python per ottimizzare la geometria molecolare e salvare in SDF. "
    "Rispondi SOLO con codice Python tra ``` e ```.",

    "Sei un esperto di Avogadro2. "
    "Genera SOLO codice Python avogadro libero. "
    "Rispondi SOLO con il blocco codice tra ``` e ```.",

    nullptr
};
static const char* kAvoActionsR[] = {
    "\xf0\x9f\xa7\xaa  Carica & calcola",
    "\xf0\x9f\x94\x8b  Energia MMFF/UFF",
    "\xf0\x9f\x93\x90  Genera file XYZ",
    "\xf0\x9f\x94\xa7  Ottimizza geometria",
    "\xf0\x9f\x90\x8d  Script libero",
    nullptr
};
} // namespace

QWidget* RicercaPage::buildAvogadroTab()
{
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(6);

    auto* descLbl = new QLabel(
        "\xf0\x9f\xa7\xaa  <i>Avogadro \xe2\x80\x94 Editor molecolare 3D open-source per la visualizzazione e "
        "il calcolo di strutture chimiche. Usato in chimica computazionale, cristallografia, modellazione di farmaci e insegnamento.</i>", w);
    descLbl->setObjectName("hintLabel");
    descLbl->setTextFormat(Qt::RichText);
    descLbl->setWordWrap(true);
    lay->addWidget(descLbl);

    auto* connRow = new QWidget(w);
    auto* connLay = new QHBoxLayout(connRow);
    connLay->setContentsMargins(0, 0, 0, 0);
    connLay->setSpacing(8);
    m_avoStatusLbl = new QLabel("\xe2\x9a\xaa  Avogadro locale", connRow);
    m_avoStatusLbl->setObjectName("hintLabel");
    auto* checkBtn = new QPushButton("\xf0\x9f\x94\x8d  Verifica avogadro", connRow);
    checkBtn->setObjectName("actionBtn");
    checkBtn->setFixedWidth(150);
    m_avoExecBtn = new QPushButton("\xf0\x9f\xa7\xaa  Esegui script", connRow);
    m_avoExecBtn->setObjectName("actionBtn");
    m_avoExecBtn->setFixedWidth(150);
    m_avoExecBtn->setEnabled(false);
    connLay->addWidget(m_avoStatusLbl, 1);
    connLay->addWidget(checkBtn);
    connLay->addWidget(m_avoExecBtn);
    lay->addWidget(connRow);

    auto* hintLbl = new QLabel(
        "\xf0\x9f\xa7\xaa <b>Avogadro MCP:</b> modellazione molecolare 3D Python. "
        "Installa con: <code>pip install avogadro</code> "
        "o <code>conda install -c conda-forge avogadro2</code>", w);
    hintLbl->setObjectName("hintLabel");
    hintLbl->setWordWrap(true);
    lay->addWidget(hintLbl);

    auto* toolRow = new QWidget(w);
    auto* toolLay = new QHBoxLayout(toolRow);
    toolLay->setContentsMargins(0, 0, 0, 0);
    toolLay->setSpacing(8);
    m_avoAction = new QComboBox(toolRow);
    for (int i = 0; kAvoActionsR[i]; i++)
        m_avoAction->addItem(QString::fromUtf8(kAvoActionsR[i]));
    m_avoModel = new QComboBox(toolRow);
    m_avoModel->setMinimumWidth(180);
    sciPopulateModels(m_avoModel);
    toolLay->addWidget(new QLabel("Azione:", toolRow));
    toolLay->addWidget(m_avoAction, 1);
    toolLay->addWidget(new QLabel("Modello:", toolRow));
    toolLay->addWidget(m_avoModel, 1);
    lay->addWidget(toolRow);

    m_avoInput = new QTextEdit(w);
    m_avoInput->setPlaceholderText(
        "Descrivi la molecola da modellare...\n"
        "Es: 'Genera la struttura 3D ottimizzata dell'acido acetilsalicilico (aspirina)'\n"
        "Es: 'Carica benzene e calcola l'energia MMFF94'");
    m_avoInput->setFixedHeight(80);
    lay->addWidget(m_avoInput);

    auto* btnRow = new QWidget(w);
    auto* btnLay = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(0, 0, 0, 0);
    m_avoRunBtn  = new QPushButton("\xf0\x9f\xa4\x96  Genera script Avogadro", btnRow);
    m_avoRunBtn->setObjectName("actionBtn");
    m_avoStopBtn = new QPushButton("\xe2\x8f\xb9  Stop", btnRow);
    m_avoStopBtn->setObjectName("actionBtn");
    m_avoStopBtn->setProperty("danger", true);
    m_avoStopBtn->setEnabled(false);
    btnLay->addWidget(m_avoRunBtn);
    btnLay->addWidget(m_avoStopBtn);
    btnLay->addStretch();
    lay->addWidget(btnRow);

    m_avoOutput = new QTextEdit(w);
    m_avoOutput->setReadOnly(true);
    m_avoOutput->setObjectName("outputView");
    m_avoOutput->setPlaceholderText("Script Python Avogadro appare qui...");
    lay->addWidget(m_avoOutput, 1);

    connect(checkBtn, &QPushButton::clicked, this, &RicercaPage::onAvoCheckClicked);

    connect(m_avoExecBtn, &QPushButton::clicked, this, &RicercaPage::onAvoExecClicked);

    connect(m_avoRunBtn, &QPushButton::clicked, this, &RicercaPage::onAvoRunClicked);
    connect(m_avoStopBtn, &QPushButton::clicked, this, &RicercaPage::onAvoStopClicked);

    return w;
}

/* ══════════════════════════════════════════════════════════════
   SLOT — AI globali (Paper / Brevetto / DocTecnico via avvia())
   ══════════════════════════════════════════════════════════════ */
void RicercaPage::onSciModelsReady(const QStringList& models)
{
    for (auto* combo : {m_cytoModel, m_rdkitModel, m_bioModel, m_avoModel,
                        m_analisiModelCombo, m_astraleModel}) {
        if (!combo) continue;
        const QString cur = combo->currentData().toString();
        combo->clear();
        for (const QString& m : models)
            combo->addItem(m, m);
        const int idx = combo->findData(cur);
        combo->setCurrentIndex(idx >= 0 ? idx : 0);
    }
}

void RicercaPage::onSciToken(const QString& t)
{
    if (!m_sciOut) return;
    m_sciOut->moveCursor(QTextCursor::End);
    m_sciOut->insertPlainText(t);
}

void RicercaPage::onSciFinished(const QString& full)
{
    QObject::disconnect(m_sciTokenConn);
    QObject::disconnect(m_sciFinishedConn);
    QObject::disconnect(m_sciErrorConn);
    m_sciTokenConn = m_sciFinishedConn = m_sciErrorConn = {};

    if (m_sciRunBtn)  m_sciRunBtn->setEnabled(true);
    if (m_sciStopBtn) m_sciStopBtn->setEnabled(false);
    if (m_sciProgress) m_sciProgress->setVisible(false);
    if (m_sciOut) m_sciOut->append("\n" + QString(40, QChar(0x2500)));

    if (m_sciExecBtn && m_sciCodeRef && full.contains("```")) {
        int start = full.indexOf("```python");
        if (start == -1) start = full.indexOf("```");
        if (start != -1) {
            start = full.indexOf('\n', start) + 1;
            const int end = full.indexOf("```", start);
            if (end != -1) {
                *m_sciCodeRef = full.mid(start, end - start).trimmed();
                if (!m_sciCodeRef->isEmpty()) {
                    m_sciExecBtn->setEnabled(true);
                    if (m_sciStatusLbl)
                        m_sciStatusLbl->setText("\xe2\x9c\x85  Codice pronto \xe2\x80\x94 premi Esegui");
                }
            }
        }
    }
}

void RicercaPage::onSciError(const QString& msg)
{
    QObject::disconnect(m_sciTokenConn);
    QObject::disconnect(m_sciFinishedConn);
    QObject::disconnect(m_sciErrorConn);
    m_sciTokenConn = m_sciFinishedConn = m_sciErrorConn = {};

    if (m_sciRunBtn)  m_sciRunBtn->setEnabled(true);
    if (m_sciStopBtn) m_sciStopBtn->setEnabled(false);
    if (m_sciProgress) m_sciProgress->setVisible(false);
    m_sciErrorPanel->showError(msg, [this]{
        avviaSci(m_sciSys, m_sciUserMsg, m_sciOut, m_sciRunBtn, m_sciStopBtn,
                 m_sciModelCombo, m_sciExecBtn, m_sciCodeRef, m_sciStatusLbl);
    });
}

void RicercaPage::onAiToken(const QString& t)
{
    if (!m_outCurrent) return;
    m_outCurrent->moveCursor(QTextCursor::End);
    m_outCurrent->insertPlainText(t);
}

void RicercaPage::onAiFinished(const QString&)
{
    if (!m_outCurrent) return;
    resetButtons();
}

void RicercaPage::onAiError(const QString& err)
{
    if (!m_outCurrent) return;
    m_outCurrent->append("\n\xe2\x9d\x8c  Errore: " + err);
    resetButtons();
}

void RicercaPage::onAiAborted()
{
    resetButtons();
    if (m_litAiTokenConn)    { disconnect(m_litAiTokenConn);    m_litAiTokenConn    = {}; }
    if (m_litAiFinishedConn) { disconnect(m_litAiFinishedConn); m_litAiFinishedConn = {}; }
    if (m_litAiErrorConn)    { disconnect(m_litAiErrorConn);    m_litAiErrorConn    = {}; }
    if (m_litAiBtn) m_litAiBtn->setEnabled(true);
    if (m_analisiTokenConn)    { disconnect(m_analisiTokenConn);    m_analisiTokenConn    = {}; }
    if (m_analisiFinishedConn) { disconnect(m_analisiFinishedConn); m_analisiFinishedConn = {}; }
    if (m_analisiErrorConn)    { disconnect(m_analisiErrorConn);    m_analisiErrorConn    = {}; }
    if (m_analisiRunBtn)  m_analisiRunBtn->setEnabled(true);
    if (m_analisiStopBtn) m_analisiStopBtn->setEnabled(false);
    if (m_astraleTokenConn)    { disconnect(m_astraleTokenConn);    m_astraleTokenConn    = {}; }
    if (m_astraleFinishedConn) { disconnect(m_astraleFinishedConn); m_astraleFinishedConn = {}; }
    if (m_astraleErrorConn)    { disconnect(m_astraleErrorConn);    m_astraleErrorConn    = {}; }
    if (m_astraleRunBtn) {
        m_astraleRunBtn->setText("\xe2\xad\x90  Leggi gli Astri");
        m_astraleRunBtn->setProperty("running", false);
    }
}

/* ══════════════════════════════════════════════════════════════
   SLOT — Cerca Letteratura
   ══════════════════════════════════════════════════════════════ */
void RicercaPage::onLitSearchClicked()
{
    const QString q = m_litQuery->text().trimmed();
    if (q.isEmpty()) {
        m_litStatus->setText("\xe2\x9a\xa0  Inserisci una query.");
        return;
    }
    m_litResults->clear();
    m_litAiBtn->setEnabled(false);
    m_litSearchBtn->setEnabled(false);
    const QString src = m_litSource->currentData().toString();
    m_litStatus->setText("\xf0\x9f\x94\x84  Ricerca in corso...");

    QUrl url;
    if (src == "arxiv") {
        url = QUrl("https://export.arxiv.org/api/query?search_query=all:"
                   + QUrl::toPercentEncoding(q) + "&max_results=8&sortBy=relevance");
    } else if (src == "semantic") {
        url = QUrl(QString("https://api.semanticscholar.org/graph/v1/paper/search"
                   "?query=") + QUrl::toPercentEncoding(q) +
                   "&limit=8&fields=title,authors,year,abstract,externalIds,url");
    } else {
        url = QUrl("https://developer.uspto.gov/ibd-api/v1/patent/publications"
                   "?searchText=" + QUrl::toPercentEncoding(q) +
                   "&start=0&rows=8&output=application%2Fjson");
    }

    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", "Prismalux/1.0 (Qt6; research)");
    req.setTransferTimeout(15000);
    auto* reply = m_litNet->get(req);
    reply->setProperty("litSrc", src);
    connect(reply, &QNetworkReply::finished, this, &RicercaPage::onLitReplyFinished);
}

void RicercaPage::onLitReplyFinished()
{
    auto* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    reply->deleteLater();
    m_litSearchBtn->setEnabled(true);

    if (reply->error() != QNetworkReply::NoError) {
        const bool isTimeout =
            (reply->error() == QNetworkReply::OperationCanceledError);
        m_litStatus->setText(
            isTimeout
            ? "\xe2\x8f\xb1  Timeout (15s) \xe2\x80\x94 premi Cerca per ritentare"
            : "\xe2\x9d\x8c  Errore: " + reply->errorString()
              + " \xe2\x80\x94 premi Cerca per ritentare");
        return;
    }
    const QByteArray data = reply->readAll();
    const QString src = reply->property("litSrc").toString();
    QString out;

    if (src == "arxiv") {
        QString xml = QString::fromUtf8(data);
        QRegularExpression reEntry("<entry>(.*?)</entry>",
            QRegularExpression::DotMatchesEverythingOption);
        auto entries = reEntry.globalMatch(xml);
        int n = 0;
        while (entries.hasNext()) {
            auto em = entries.next();
            QString e = em.captured(1);
            auto title   = QRegularExpression("<title>(.*?)</title>",
                QRegularExpression::DotMatchesEverythingOption)
                .match(e).captured(1).trimmed();
            auto summary = QRegularExpression("<summary>(.*?)</summary>",
                QRegularExpression::DotMatchesEverythingOption)
                .match(e).captured(1).trimmed().left(300);
            auto id      = QRegularExpression("<id>(.*?)</id>",
                QRegularExpression::DotMatchesEverythingOption)
                .match(e).captured(1).trimmed();
            if (title.isEmpty()) continue;
            out += QString("\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80 %1. %2\n").arg(++n).arg(title);
            out += QString("    %1\n").arg(summary);
            out += QString("    \xf0\x9f\x94\x97 %1\n\n").arg(id);
        }
        if (out.isEmpty()) out = "Nessun risultato trovato.";
        m_litStatus->setText(QString("\xe2\x9c\x85  %1 risultati da arXiv").arg(n));

    } else if (src == "semantic") {
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonArray papers = doc.object().value("data").toArray();
        int n = 0;
        for (const auto& pv : papers) {
            auto p = pv.toObject();
            QString title = p.value("title").toString();
            int year      = p.value("year").toInt();
            QString abstr = p.value("abstract").toString().left(280);
            QString url   = p.value("url").toString();
            QJsonArray authors = p.value("authors").toArray();
            QStringList auList;
            for (const auto& a : authors)
                auList << a.toObject().value("name").toString();
            out += QString("\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80 %1. %2 (%3)\n")
                   .arg(++n).arg(title).arg(year);
            if (!auList.isEmpty())
                out += "    \xf0\x9f\x91\xa4 " + auList.join(", ") + "\n";
            if (!abstr.isEmpty())
                out += "    " + abstr + "\n";
            if (!url.isEmpty())
                out += "    \xf0\x9f\x94\x97 " + url + "\n";
            out += "\n";
        }
        if (out.isEmpty()) out = "Nessun risultato trovato.";
        m_litStatus->setText(
            QString("\xe2\x9c\x85  %1 risultati da Semantic Scholar").arg(n));

    } else {
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonArray pubs = doc.object()
            .value("results").toObject()
            .value("hits").toArray();
        if (pubs.isEmpty())
            pubs = doc.object().value("patents").toArray();
        int n = 0;
        for (const auto& pv : pubs) {
            auto p = pv.toObject();
            QString title = p.value("inventionTitle").toString();
            if (title.isEmpty()) title = p.value("patentTitle").toString();
            QString appNum = p.value("applicationNumberText").toString();
            QString date   = p.value("filingDate").toString();
            QString abstr  = p.value("abstractText").toString().left(280);
            out += QString("\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80 %1. %2\n").arg(++n).arg(title);
            if (!appNum.isEmpty()) out += "    N\xc2\xb0 " + appNum + "\n";
            if (!date.isEmpty())   out += "    \xf0\x9f\x93\x85 " + date + "\n";
            if (!abstr.isEmpty())  out += "    " + abstr + "\n";
            out += "\n";
        }
        if (out.isEmpty()) {
            out = "Nessun risultato trovato (USPTO). "
                  "Prova arXiv o Semantic Scholar.";
        }
        m_litStatus->setText(
            QString("\xe2\x9c\x85  %1 risultati da USPTO").arg(n));
    }

    m_litResults->setPlainText(out);
    m_litAiBtn->setEnabled(!out.isEmpty() &&
        out != "Nessun risultato trovato.");
}

void RicercaPage::onLitAiClicked()
{
    const QString ctx = m_litResults->toPlainText().trimmed();
    if (ctx.isEmpty()) return;
    const QString q = m_litQuery->text().trimmed();
    m_litAiBtn->setEnabled(false);
    m_litStatus->setText("\xf0\x9f\xa4\x96  Analisi AI...");
    m_litResults->append("\n" + QString(50, QChar(0x2500)) + "\n");

    const QString sys =
        "Sei un ricercatore scientifico. Analizza i seguenti risultati di ricerca "
        "e fornisci: 1) i paper/brevetti pi\xc3\xb9 rilevanti, 2) trend emergenti, "
        "3) gap nella letteratura, 4) suggerimenti per ricerche future. "
        "Sii conciso e preciso.";
    const QString msg = "Query: " + q + "\n\nRisultati:\n" + ctx.left(6000);

    if (m_litAiTokenConn)    disconnect(m_litAiTokenConn);
    if (m_litAiFinishedConn) disconnect(m_litAiFinishedConn);
    if (m_litAiErrorConn)    disconnect(m_litAiErrorConn);

    m_litAiTokenConn    = connect(m_ai, &AiClient::token,    this, &RicercaPage::onLitAiToken);
    m_litAiFinishedConn = connect(m_ai, &AiClient::finished, this, &RicercaPage::onLitAiFinished);
    m_litAiErrorConn    = connect(m_ai, &AiClient::error,    this, &RicercaPage::onLitAiError);
    m_ai->chat(sys, msg);
}

void RicercaPage::onLitAiToken(const QString& t)
{
    QTextCursor c = m_litResults->textCursor();
    c.movePosition(QTextCursor::End);
    m_litResults->setTextCursor(c);
    m_litResults->insertPlainText(t);
}

void RicercaPage::onLitAiFinished(const QString&)
{
    disconnect(m_litAiTokenConn);    m_litAiTokenConn    = {};
    disconnect(m_litAiFinishedConn); m_litAiFinishedConn = {};
    disconnect(m_litAiErrorConn);    m_litAiErrorConn    = {};
    m_litAiBtn->setEnabled(true);
    m_litStatus->setText("\xe2\x9c\x85  Analisi completata");
}

void RicercaPage::onLitAiError(const QString& e)
{
    disconnect(m_litAiTokenConn);    m_litAiTokenConn    = {};
    disconnect(m_litAiFinishedConn); m_litAiFinishedConn = {};
    disconnect(m_litAiErrorConn);    m_litAiErrorConn    = {};
    m_litAiBtn->setEnabled(true);
    m_litStatus->setText("\xe2\x9d\x8c  " + e);
}

/* ══════════════════════════════════════════════════════════════
   SLOT — Cytoscape
   ══════════════════════════════════════════════════════════════ */
void RicercaPage::onCytoPingClicked()
{
    const QString addr = m_cytoHostEdit->text().trimmed();
    const QString host = addr.contains(':') ? addr.section(':', 0, 0) : addr;
    const int port = addr.contains(':') ? addr.section(':', 1).toInt() : 1234;
    m_cytoStatusLbl->setText("\xf0\x9f\x94\x84  Connessione...");
    if (m_cytoSock) { m_cytoSock->abort(); m_cytoSock->deleteLater(); m_cytoSock = nullptr; }
    m_cytoSock = new QTcpSocket(this);
    m_cytoSock->connectToHost(host, static_cast<quint16>(port));
    connect(m_cytoSock, &QTcpSocket::connected,
            this, &RicercaPage::onCytoSockConnected);
    connect(m_cytoSock, &QAbstractSocket::errorOccurred,
            this, &RicercaPage::onCytoSockError);
    QTimer::singleShot(3000, this, &RicercaPage::onCytoPingTimeout);
}

void RicercaPage::onCytoSockConnected()
{
    auto* sock = qobject_cast<QTcpSocket*>(sender());
    if (sock) { sock->disconnectFromHost(); sock->deleteLater(); }
    if (m_cytoSock == sock) m_cytoSock = nullptr;
    m_cytoStatusLbl->setText("\xe2\x9c\x85  Server raggiungibile");
    m_cytoExecBtn->setEnabled(!m_cytoCode.isEmpty());
}

void RicercaPage::onCytoSockError(QAbstractSocket::SocketError)
{
    auto* sock = qobject_cast<QTcpSocket*>(sender());
    const QString errStr = sock ? sock->errorString() : QString();
    m_cytoStatusLbl->setText("\xe2\x9d\x8c  " + errStr);
    if (sock) sock->deleteLater();
    if (m_cytoSock == sock) m_cytoSock = nullptr;
}

void RicercaPage::onCytoPingTimeout()
{
    if (m_cytoSock && m_cytoSock->state() != QAbstractSocket::ConnectedState) {
        m_cytoStatusLbl->setText("\xe2\x9d\x8c  Timeout");
        m_cytoSock->abort();
        m_cytoSock->deleteLater();
        m_cytoSock = nullptr;
    }
}

void RicercaPage::onCytoExecClicked()
{
    runSciScript(m_cytoCode, false,
                 m_cytoStatusLbl, m_cytoExecBtn, m_cytoOutput, m_cytoProc, this);
}

void RicercaPage::onCytoRunClicked()
{
    const int idx = m_cytoAction->currentIndex();
    if (idx < 0 || !kCytoSysR[idx]) return;
    avviaSci(QString::fromUtf8(kCytoSysR[idx]),
             m_cytoInput->toPlainText(),
             m_cytoOutput, m_cytoRunBtn, m_cytoStopBtn,
             m_cytoModel, m_cytoExecBtn, &m_cytoCode, m_cytoStatusLbl);
}

void RicercaPage::onCytoStopClicked()
{
    m_ai->abort();
    m_cytoRunBtn->setEnabled(true);
    m_cytoStopBtn->setEnabled(false);
}

/* ══════════════════════════════════════════════════════════════
   SLOT — RDKit
   ══════════════════════════════════════════════════════════════ */
void RicercaPage::onRdkitCheckClicked()
{
    auto* proc = new QProcess(this);
    proc->start(P::findPython(), {"-c", "import rdkit; print('rdkit', rdkit.__version__)"});
    connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &RicercaPage::onRdkitCheckFinished);
}

void RicercaPage::onRdkitCheckFinished(int code, QProcess::ExitStatus)
{
    auto* proc = qobject_cast<QProcess*>(sender());
    if (!proc) return;
    const QString out = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
    m_rdkitStatusLbl->setText(code == 0
        ? "\xe2\x9c\x85  " + out
        : "\xe2\x9d\x8c  rdkit non trovato \xe2\x80\x94 pip install rdkit");
    proc->deleteLater();
}

void RicercaPage::onRdkitExecClicked()
{
    runSciScript(m_rdkitCode, false,
                 m_rdkitStatusLbl, m_rdkitExecBtn, m_rdkitOutput, m_rdkitProc, this);
}

void RicercaPage::onRdkitRunClicked()
{
    const int idx = m_rdkitAction->currentIndex();
    if (idx < 0 || !kRDKitSysR[idx]) return;
    avviaSci(QString::fromUtf8(kRDKitSysR[idx]),
             m_rdkitInput->toPlainText(),
             m_rdkitOutput, m_rdkitRunBtn, m_rdkitStopBtn,
             m_rdkitModel, m_rdkitExecBtn, &m_rdkitCode, m_rdkitStatusLbl);
}

void RicercaPage::onRdkitStopClicked()
{
    m_ai->abort();
    m_rdkitRunBtn->setEnabled(true);
    m_rdkitStopBtn->setEnabled(false);
}

/* ══════════════════════════════════════════════════════════════
   SLOT — Bioconda
   ══════════════════════════════════════════════════════════════ */
void RicercaPage::onBioCheckClicked()
{
    auto* proc = new QProcess(this);
    proc->start("conda", {"--version"});
    connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &RicercaPage::onBioCheckFinished);
}

void RicercaPage::onBioCheckFinished(int code, QProcess::ExitStatus)
{
    auto* proc = qobject_cast<QProcess*>(sender());
    if (!proc) return;
    const QString out = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
    m_bioStatusLbl->setText(code == 0
        ? "\xe2\x9c\x85  " + out + " disponibile"
        : "\xe2\x9d\x8c  conda non trovato \xe2\x80\x94 installa Miniforge");
    proc->deleteLater();
}

void RicercaPage::onBioExecClicked()
{
    const bool isBash = m_bioCode.startsWith("#!")
                     || m_bioCode.contains("#!/bin/bash")
                     || m_bioCode.contains("bwa ")
                     || m_bioCode.contains("samtools ")
                     || m_bioCode.contains("gatk ")
                     || m_bioCode.contains("blast");
    runSciScript(m_bioCode, isBash,
                 m_bioStatusLbl, m_bioExecBtn, m_bioOutput, m_bioProc, this);
}

void RicercaPage::onBioRunClicked()
{
    const int idx = m_bioAction->currentIndex();
    if (idx < 0 || !kBioSysR[idx]) return;
    avviaSci(QString::fromUtf8(kBioSysR[idx]),
             m_bioInput->toPlainText(),
             m_bioOutput, m_bioRunBtn, m_bioStopBtn,
             m_bioModel, m_bioExecBtn, &m_bioCode, m_bioStatusLbl);
}

void RicercaPage::onBioStopClicked()
{
    m_ai->abort();
    m_bioRunBtn->setEnabled(true);
    m_bioStopBtn->setEnabled(false);
}

/* ══════════════════════════════════════════════════════════════
   SLOT — Avogadro
   ══════════════════════════════════════════════════════════════ */
void RicercaPage::onAvoCheckClicked()
{
    auto* proc = new QProcess(this);
    proc->start(P::findPython(),
        {"-c", "import avogadro; print('avogadro OK')"});
    connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &RicercaPage::onAvoCheckFinished);
}

void RicercaPage::onAvoCheckFinished(int code, QProcess::ExitStatus)
{
    auto* proc = qobject_cast<QProcess*>(sender());
    if (!proc) return;
    m_avoStatusLbl->setText(code == 0
        ? "\xe2\x9c\x85  avogadro disponibile"
        : "\xe2\x9d\x8c  avogadro non trovato \xe2\x80\x94 pip install avogadro");
    proc->deleteLater();
}

void RicercaPage::onAvoExecClicked()
{
    runSciScript(m_avoCode, false,
                 m_avoStatusLbl, m_avoExecBtn, m_avoOutput, m_avoProc, this);
}

void RicercaPage::onAvoRunClicked()
{
    const int idx = m_avoAction->currentIndex();
    if (idx < 0 || !kAvoSysR[idx]) return;
    avviaSci(QString::fromUtf8(kAvoSysR[idx]),
             m_avoInput->toPlainText(),
             m_avoOutput, m_avoRunBtn, m_avoStopBtn,
             m_avoModel, m_avoExecBtn, &m_avoCode, m_avoStatusLbl);
}

void RicercaPage::onAvoStopClicked()
{
    m_ai->abort();
    m_avoRunBtn->setEnabled(true);
    m_avoStopBtn->setEnabled(false);
}

/* ══════════════════════════════════════════════════════════════════════
   RAB₀-L — Radial Algebraic Base-80 Logarithmic
   Spirale logaritmica per analisi sequenze DNA (wildlux, 2025)
   ══════════════════════════════════════════════════════════════════════ */
QWidget* RicercaPage::buildRab0lTab()
{
    auto* w    = new QWidget;
    auto* vlay = new QVBoxLayout(w);
    vlay->setContentsMargins(10, 8, 10, 8);
    vlay->setSpacing(6);

    /* ── barra input ── */
    auto* bar  = new QWidget;
    auto* hlay = new QHBoxLayout(bar);
    hlay->setContentsMargins(0, 0, 0, 0);
    hlay->setSpacing(8);

    m_rab0lSeq1 = new QLineEdit;
    m_rab0lSeq1->setPlaceholderText("Sequenza 1  (es. ATCGATCGGCTA)");
    m_rab0lSeq2 = new QLineEdit;
    m_rab0lSeq2->setPlaceholderText("Sequenza 2  (opzionale — calcola SIM)");

    auto* btnAn = new QPushButton("\xe2\x96\xb6  Analizza");
    btnAn->setObjectName("primaryBtn");
    auto* btnCl = new QPushButton("\xf0\x9f\x97\x91");
    btnCl->setToolTip("Pulisci");
    btnCl->setFixedWidth(32);

    m_rab0lSimLbl = new QLabel;
    m_rab0lSimLbl->setTextFormat(Qt::RichText);

    hlay->addWidget(new QLabel("Seq 1:"));
    hlay->addWidget(m_rab0lSeq1, 3);
    hlay->addWidget(new QLabel("Seq 2:"));
    hlay->addWidget(m_rab0lSeq2, 3);
    hlay->addWidget(btnAn);
    hlay->addWidget(btnCl);
    hlay->addWidget(m_rab0lSimLbl, 2);

    m_rab0lCanvas = new Rab0lCanvas(w);

    vlay->addWidget(bar);
    vlay->addWidget(m_rab0lCanvas, 1);

    connect(btnAn, &QPushButton::clicked, this, &RicercaPage::onRab0lAnalyzeClicked);
    connect(m_rab0lSeq1, &QLineEdit::returnPressed, this, &RicercaPage::onRab0lAnalyzeClicked);
    connect(btnCl, &QPushButton::clicked, this, &RicercaPage::onRab0lClearClicked);

    return w;
}

void RicercaPage::onRab0lAnalyzeClicked()
{
    static const QRegularExpression kNonDna("[^ATCGatcg]");

    QString s1 = m_rab0lSeq1->text().trimmed().toUpper();
    QString s2 = m_rab0lSeq2->text().trimmed().toUpper();
    s1.remove(kNonDna);
    s2.remove(kNonDna);

    if (s1.isEmpty()) return;

    if (s2.isEmpty()) {
        m_rab0lCanvas->setSeq1Only(s1);
        m_rab0lSimLbl->setText(
            QString("<b>N=%1</b>  <span style='color:gray'>"
                    "inserisci Seq 2 per confronto SIM</span>").arg(s1.size()));
    } else {
        m_rab0lCanvas->setSequences(s1, s2);
        const auto& r = m_rab0lCanvas->result();
        QString col = (r.sim >= 0.8) ? "#4CAF50"
                    : (r.sim >= 0.4) ? "#FFC107" : "#F44336";
        m_rab0lSimLbl->setText(
            QString("<b>SIM = <span style='color:%1'>%2</span></b>"
                    "  |  N=%3  |  SNP=%4")
            .arg(col)
            .arg(r.sim, 0, 'f', 3)
            .arg(r.len)
            .arg(r.snp));
    }
}

/* ══════════════════════════════════════════════════════════════════════
   BLHM — Brain-Loop-Human-MultiContext
   Tab interni: Calcolatore · Note · DNA (wildlux, 2026)
   ══════════════════════════════════════════════════════════════════════ */
QWidget* RicercaPage::buildBlhmTab()
{
    auto* w    = new QWidget;
    auto* vlay = new QVBoxLayout(w);
    vlay->setContentsMargins(10, 8, 10, 8);
    vlay->setSpacing(6);

    auto* hdr = new QLabel(
        "  \xf0\x9f\xa7\xa0  <b>BLHM \xe2\x80\x94 Brain-Loop-Human-MultiContext</b>"
        "  <span style='color:gray;font-size:11px;'>"
        "R\xe2\x82\x98 = 0.50\xc2\xb7R\xe2\x82\x99 + 0.35\xc2\xb7R\xe2\x82\x97"
        " + 0.15\xc2\xb7R\xe2\x82\x9a"
        "  \xe2\x80\x94  match(w,q) = prefisso_comune / max(|w|,|q|)"
        "</span>");
    hdr->setTextFormat(Qt::RichText);
    hdr->setObjectName("pageHeader");
    hdr->setFixedHeight(36);
    vlay->addWidget(hdr);

    auto* inner = new QTabWidget(w);
    inner->setDocumentMode(true);

    /* ══════════════ TAB 1: CALCOLATORE ══════════════ */
    {
        auto* calcW   = new QWidget;
        auto* calcLay = new QVBoxLayout(calcW);
        calcLay->setContentsMargins(0, 4, 0, 0);
        calcLay->setSpacing(6);

        auto* split = new QSplitter(Qt::Horizontal, calcW);

        /* ── SINISTRA: tabella pesi ibridi ── */
        auto* leftW   = new QWidget;
        auto* leftLay = new QVBoxLayout(leftW);
        leftLay->setContentsMargins(0, 0, 4, 0);

        auto* lblPesi = new QLabel(
            "Pesi ibridi  <span style='color:gray;font-size:11px;'>"
            "(doppio click per modificare)</span>");
        lblPesi->setTextFormat(Qt::RichText);

        m_blhmTable = new QTableWidget(0, 4, leftW);
        m_blhmTable->setHorizontalHeaderLabels(
            {"Percorso ontologico", "factory_w", "link_w", "user_w"});
        m_blhmTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        m_blhmTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
        m_blhmTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
        m_blhmTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
        m_blhmTable->setColumnWidth(1, 80);
        m_blhmTable->setColumnWidth(2, 80);
        m_blhmTable->setColumnWidth(3, 80);
        m_blhmTable->setAlternatingRowColors(true);
        m_blhmTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_blhmTable->setMinimumWidth(340);

        struct BW { const char* path; double fw, lw, uw; };
        static const BW kDef[] = {
            {"Bio",               0.900, 0.462, 0.0},
            {"Bio,Ani",           0.700, 0.631, 0.0},
            {"Bio,Ani,Mam",       0.600, 0.710, 0.0},
            {"Bio,Ani,Mam,Cane",  1.000, 1.000, 0.0},
            {"Bio,Ani,Mam,Cane",  0.999, 0.999, 0.0},
            {"Bio,Ani,Mam,Gatto", 0.700, 0.704, 0.0},
            {"Bio,Ani,Mam,Lupo",  0.700, 0.784, 0.0},
        };
        m_blhmTable->setRowCount(int(std::size(kDef)));
        for (int r = 0; r < int(std::size(kDef)); ++r) {
            m_blhmTable->setItem(r, 0, new QTableWidgetItem(kDef[r].path));
            m_blhmTable->setItem(r, 1, new QTableWidgetItem(
                QString::number(kDef[r].fw, 'f', 3)));
            m_blhmTable->setItem(r, 2, new QTableWidgetItem(
                QString::number(kDef[r].lw, 'f', 3)));
            m_blhmTable->setItem(r, 3, new QTableWidgetItem(
                QString::number(kDef[r].uw, 'f', 3)));
        }

        auto* btnAdd = new QPushButton("+ Aggiungi peso");
        btnAdd->setObjectName("actionBtn");
        auto* btnDel = new QPushButton("\xf0\x9f\x97\x91  Rimuovi riga");
        btnDel->setObjectName("actionBtn");
        auto* rowBar = new QWidget;
        auto* rowLay = new QHBoxLayout(rowBar);
        rowLay->setContentsMargins(0, 0, 0, 0);
        rowLay->addWidget(btnAdd);
        rowLay->addWidget(btnDel);
        rowLay->addStretch();

        leftLay->addWidget(lblPesi);
        leftLay->addWidget(m_blhmTable, 1);
        leftLay->addWidget(rowBar);

        /* ── DESTRA: query + output ── */
        auto* rightW   = new QWidget;
        auto* rightLay = new QVBoxLayout(rightW);
        rightLay->setContentsMargins(4, 0, 0, 0);

        auto* qBar  = new QWidget;
        auto* qHlay = new QHBoxLayout(qBar);
        qHlay->setContentsMargins(0, 0, 0, 0);
        qHlay->setSpacing(6);

        m_blhmQuery = new QLineEdit;
        m_blhmQuery->setPlaceholderText("Query path  es. Bio,Ani,Mam,Cane");
        m_blhmQuery->setText("Bio,Ani,Mam,Cane");

        auto* btnCalc = new QPushButton("\xf0\x9f\xa7\xae  Calcola R");
        btnCalc->setObjectName("primaryBtn");

        qHlay->addWidget(new QLabel("Query:"));
        qHlay->addWidget(m_blhmQuery, 1);
        qHlay->addWidget(btnCalc);

        m_blhmOutput = new QTextEdit;
        m_blhmOutput->setReadOnly(true);
        QFont mono("monospace");
        mono.setStyleHint(QFont::Monospace);
        mono.setPointSize(9);
        m_blhmOutput->setFont(mono);

        rightLay->addWidget(qBar);
        rightLay->addWidget(m_blhmOutput, 1);

        split->addWidget(leftW);
        split->addWidget(rightW);
        split->setStretchFactor(0, 2);
        split->setStretchFactor(1, 3);

        calcLay->addWidget(split, 1);

        connect(btnCalc, &QPushButton::clicked,       this, &RicercaPage::onBlhmComputeClicked);
        connect(m_blhmQuery, &QLineEdit::returnPressed, this, &RicercaPage::onBlhmComputeClicked);
        connect(btnAdd, &QPushButton::clicked, this, &RicercaPage::onBlhmAddRowClicked);
        connect(btnDel, &QPushButton::clicked, this, &RicercaPage::onBlhmDeleteRowClicked);

        inner->addTab(calcW, "\xf0\x9f\xa7\xae  Calcolatore");
    }

    /* ══════════════ TAB 2: NOTE ══════════════ */
    {
        auto* noteW   = new QWidget;
        auto* noteLay = new QVBoxLayout(noteW);
        noteLay->setContentsMargins(0, 4, 0, 0);
        noteLay->setSpacing(6);

        auto* bar  = new QWidget;
        auto* blay = new QHBoxLayout(bar);
        blay->setContentsMargins(0, 0, 0, 0);
        blay->setSpacing(6);

        auto* lbl = new QLabel(
            "<b>\xf0\x9f\x93\x9d  Appunti BLHM / RAB\xe2\x82\x80-L</b>"
            "  <span style='color:gray;font-size:11px;'>"
            "salvato in RAG/BLHM_note.md</span>");
        lbl->setTextFormat(Qt::RichText);

        auto* btnLoad = new QPushButton("\xf0\x9f\x93\x82  Carica");
        btnLoad->setObjectName("actionBtn");
        auto* btnSave = new QPushButton("\xf0\x9f\x92\xbe  Salva");
        btnSave->setObjectName("primaryBtn");
        auto* btnClr  = new QPushButton("\xf0\x9f\x97\x91");
        btnClr->setFixedWidth(32);
        btnClr->setToolTip("Svuota");

        blay->addWidget(lbl);
        blay->addStretch();
        blay->addWidget(btnLoad);
        blay->addWidget(btnSave);
        blay->addWidget(btnClr);

        m_blhmNoteEdit = new QTextEdit;
        m_blhmNoteEdit->setPlaceholderText(
            "Prendi appunti sui documenti BLHM e RAB\xe2\x82\x80-L...\n"
            "Puoi usare **grassetto**, # titoli, - elenchi (Markdown).\n\n"
            "I file PDF di riferimento sono in RAG/:\n"
            "  \xe2\x80\xa2 BLHM_arXiv_Paper.pdf\n"
            "  \xe2\x80\xa2 BLHM_Documento_Tecnico.pdf\n"
            "  \xe2\x80\xa2 RAB80L_Report.pdf");
        QFont noteFont;
        noteFont.setPointSize(10);
        m_blhmNoteEdit->setFont(noteFont);

        /* auto-carica note esistenti */
        {
            QFile f(P::root() + "/RAG/BLHM_note.md");
            if (f.open(QIODevice::ReadOnly | QIODevice::Text))
                m_blhmNoteEdit->setPlainText(QTextStream(&f).readAll());
        }

        noteLay->addWidget(bar);
        noteLay->addWidget(m_blhmNoteEdit, 1);

        connect(btnSave, &QPushButton::clicked, this, &RicercaPage::onBlhmNoteSave);
        connect(btnLoad, &QPushButton::clicked, this, &RicercaPage::onBlhmNoteLoad);
        connect(btnClr,  &QPushButton::clicked, this, &RicercaPage::onBlhmNotesClearClicked);

        inner->addTab(noteW, "\xf0\x9f\x93\x9d  Note");
    }

    /* ══════════════ TAB 3: DNA VIEWER ══════════════ */
    {
        auto* dnaW   = new QWidget;
        auto* dnaLay = new QVBoxLayout(dnaW);
        dnaLay->setContentsMargins(0, 4, 0, 0);
        dnaLay->setSpacing(6);

        auto* bar  = new QWidget;
        auto* hlay = new QHBoxLayout(bar);
        hlay->setContentsMargins(0, 0, 0, 0);
        hlay->setSpacing(6);

        m_blhmDnaSeq1 = new QLineEdit;
        m_blhmDnaSeq1->setPlaceholderText("Sequenza DNA 1  (A/T/C/G)");

        m_blhmDnaSeq2 = new QLineEdit;
        m_blhmDnaSeq2->setPlaceholderText("Sequenza DNA 2  (opzionale, per confronto SIM)");

        auto* btnAn = new QPushButton("\xf0\x9f\xa7\xac  Analizza");
        btnAn->setObjectName("primaryBtn");
        auto* btnCl = new QPushButton("\xf0\x9f\x97\x91");
        btnCl->setToolTip("Pulisci");
        btnCl->setFixedWidth(32);

        m_blhmDnaSimLbl = new QLabel;
        m_blhmDnaSimLbl->setTextFormat(Qt::RichText);

        hlay->addWidget(new QLabel("Seq 1:"));
        hlay->addWidget(m_blhmDnaSeq1, 3);
        hlay->addWidget(new QLabel("Seq 2:"));
        hlay->addWidget(m_blhmDnaSeq2, 3);
        hlay->addWidget(btnAn);
        hlay->addWidget(btnCl);
        hlay->addWidget(m_blhmDnaSimLbl, 2);

        m_blhmDnaCanvas = new Rab0lCanvas(dnaW);

        dnaLay->addWidget(bar);
        dnaLay->addWidget(m_blhmDnaCanvas, 1);

        connect(btnAn, &QPushButton::clicked, this, &RicercaPage::onBlhmDnaAnalyzeClicked);
        connect(m_blhmDnaSeq1, &QLineEdit::returnPressed, this, &RicercaPage::onBlhmDnaAnalyzeClicked);
        connect(btnCl, &QPushButton::clicked, this, &RicercaPage::onBlhmDnaClearClicked);

        inner->addTab(dnaW, "\xf0\x9f\xa7\xac  DNA");
    }

    /* ══════════════ TAB 4: ENGINE C ══════════════ */
    {
        auto* engW   = new QWidget;
        auto* engLay = new QVBoxLayout(engW);
        engLay->setContentsMargins(0, 6, 0, 4);
        engLay->setSpacing(8);

        auto* descLbl = new QLabel(
            "<b>\xe2\x9a\x99  Engine C</b>  "
            "<span style='color:gray;font-size:11px;'>"
            "Thread pool POSIX (pattern ds4) \xc2\xb7 3 cicli paralleli: "
            "Factory \xc2\xb7 Link \xc2\xb7 User"
            "</span>");
        descLbl->setTextFormat(Qt::RichText);
        engLay->addWidget(descLbl);

        /* ── barra superiore: sync + run + latenza ── */
        auto* topBar  = new QWidget;
        auto* topHLay = new QHBoxLayout(topBar);
        topHLay->setContentsMargins(0, 0, 0, 0);
        topHLay->setSpacing(8);

        auto* btnSync = new QPushButton(
            "\xe2\x86\x93  Importa dal Calcolatore");
        btnSync->setObjectName("actionBtn");
        auto* btnRun = new QPushButton(
            "\xe2\x96\xb6  Esegui Inferenza (3 thread)");
        btnRun->setObjectName("primaryBtn");

        m_blhmEngineLatencyLbl = new QLabel("\xe2\x80\x94");
        topHLay->addWidget(btnSync);
        topHLay->addWidget(btnRun);
        topHLay->addStretch();
        topHLay->addWidget(new QLabel("Latenza:"));
        topHLay->addWidget(m_blhmEngineLatencyLbl);
        engLay->addWidget(topBar);

        m_blhmEngineStatusLbl = new QLabel(
            "Grafo non inizializzato. Premi \"Importa dal Calcolatore\".");
        m_blhmEngineStatusLbl->setStyleSheet("color:#888;font-size:11px;");
        engLay->addWidget(m_blhmEngineStatusLbl);

        /* ── 3 pannelli colorati: Factory / Link / User ── */
        auto* panelsW    = new QWidget;
        auto* panelsHLay = new QHBoxLayout(panelsW);
        panelsHLay->setContentsMargins(0, 0, 0, 0);
        panelsHLay->setSpacing(8);

        QFont mono("monospace");
        mono.setStyleHint(QFont::Monospace);
        mono.setPointSize(9);

        struct PanelSpec { const char* title; const char* color; QLabel** lbl; };
        PanelSpec panels[] = {
            { "Factory  R\xe2\x82\x99", "#4a9eff", &m_blhmEngineFactoryLbl },
            { "Link  R\xe2\x82\x97",    "#4ec94e", &m_blhmEngineLinkLbl    },
            { "User  R\xe2\x82\x9a",    "#ff9944", &m_blhmEngineUserLbl    },
        };
        for (auto& ps : panels) {
            auto* frame = new QFrame;
            frame->setFrameShape(QFrame::StyledPanel);
            frame->setStyleSheet(
                QString("QFrame{border:2px solid %1;border-radius:6px;padding:4px;}").arg(ps.color));
            auto* flay = new QVBoxLayout(frame);
            flay->setContentsMargins(6, 4, 6, 4);
            flay->setSpacing(3);
            auto* titleL = new QLabel(
                QString("<b style='color:%1;'>%2</b>").arg(ps.color, ps.title));
            titleL->setTextFormat(Qt::RichText);
            *ps.lbl = new QLabel("score: \xe2\x80\x94\nn_active: \xe2\x80\x94\ntop: \xe2\x80\x94");
            (*ps.lbl)->setAlignment(Qt::AlignTop | Qt::AlignLeft);
            (*ps.lbl)->setTextInteractionFlags(Qt::TextSelectableByMouse);
            (*ps.lbl)->setFont(mono);
            flay->addWidget(titleL);
            flay->addWidget(*ps.lbl);
            panelsHLay->addWidget(frame);
        }
        engLay->addWidget(panelsW);

        /* ── R_merged combinato ── */
        auto* combBar  = new QWidget;
        auto* combHLay = new QHBoxLayout(combBar);
        combHLay->setContentsMargins(0, 0, 0, 0);
        combHLay->setSpacing(8);
        combHLay->addWidget(new QLabel("<b>R_merged:</b>"));
        m_blhmEngineCombinedLbl = new QLabel("\xe2\x80\x94");
        m_blhmEngineCombinedLbl->setStyleSheet(
            "font-size:20px;font-weight:bold;");
        combHLay->addWidget(m_blhmEngineCombinedLbl);
        combHLay->addStretch();
        engLay->addWidget(combBar);

        auto* sep1 = new QFrame;
        sep1->setFrameShape(QFrame::HLine);
        sep1->setFrameShadow(QFrame::Sunken);
        engLay->addWidget(sep1);

        /* ── Auto-finetuning Hebbiano ── */
        auto* autoftHdr = new QLabel(
            "<b>Auto-finetuning Hebbiano</b>  "
            "<span style='color:gray;font-size:11px;'>"
            "link_w += lr \xc2\xb7 match  (factory_w mai modificato)"
            "</span>");
        autoftHdr->setTextFormat(Qt::RichText);
        engLay->addWidget(autoftHdr);

        auto* autoftBar  = new QWidget;
        auto* autoftHLay = new QHBoxLayout(autoftBar);
        autoftHLay->setContentsMargins(0, 0, 0, 0);
        autoftHLay->setSpacing(8);

        autoftHLay->addWidget(new QLabel("LR:"));
        m_blhmEngineLrSpin = new QDoubleSpinBox;
        m_blhmEngineLrSpin->setRange(0.001, 1.0);
        m_blhmEngineLrSpin->setSingleStep(0.01);
        m_blhmEngineLrSpin->setValue(0.05);
        m_blhmEngineLrSpin->setDecimals(3);
        m_blhmEngineLrSpin->setFixedWidth(80);
        auto* btnAutoft = new QPushButton("Applica Autoft");
        btnAutoft->setObjectName("actionBtn");
        autoftHLay->addWidget(m_blhmEngineLrSpin);
        autoftHLay->addWidget(btnAutoft);
        autoftHLay->addStretch();
        engLay->addWidget(autoftBar);

        m_blhmEngineAutoftOut = new QTextEdit;
        m_blhmEngineAutoftOut->setReadOnly(true);
        m_blhmEngineAutoftOut->setMaximumHeight(100);
        m_blhmEngineAutoftOut->setFont(mono);
        m_blhmEngineAutoftOut->setPlaceholderText(
            "Delta link_w dopo auto-finetuning (before \xe2\x86\x92 after)...");
        engLay->addWidget(m_blhmEngineAutoftOut);

        auto* sep2 = new QFrame;
        sep2->setFrameShape(QFrame::HLine);
        sep2->setFrameShadow(QFrame::Sunken);
        engLay->addWidget(sep2);

        /* ── Salva / Carica .blhm ── */
        auto* ioBar  = new QWidget;
        auto* ioHLay = new QHBoxLayout(ioBar);
        ioHLay->setContentsMargins(0, 0, 0, 0);
        ioHLay->setSpacing(8);
        auto* btnSaveBlhm = new QPushButton(
            "\xf0\x9f\x92\xbe  Salva .blhm");
        btnSaveBlhm->setObjectName("actionBtn");
        auto* btnLoadBlhm = new QPushButton(
            "\xf0\x9f\x93\x82  Carica .blhm");
        btnLoadBlhm->setObjectName("actionBtn");
        ioHLay->addWidget(btnSaveBlhm);
        ioHLay->addWidget(btnLoadBlhm);
        ioHLay->addStretch();
        engLay->addWidget(ioBar);
        engLay->addStretch();

        connect(btnSync,      &QPushButton::clicked, this,
                &RicercaPage::onBlhmEngineSyncFromCalcClicked);
        connect(btnRun,       &QPushButton::clicked, this,
                &RicercaPage::onBlhmEngineRunClicked);
        connect(btnAutoft,    &QPushButton::clicked, this,
                &RicercaPage::onBlhmEngineAutoftClicked);
        connect(btnSaveBlhm,  &QPushButton::clicked, this,
                &RicercaPage::onBlhmEngineSaveClicked);
        connect(btnLoadBlhm,  &QPushButton::clicked, this,
                &RicercaPage::onBlhmEngineLoadClicked);

        inner->addTab(engW, "\xe2\x9a\x99  Engine C");
    }

    vlay->addWidget(inner, 1);
    return w;
}

void RicercaPage::onBlhmComputeClicked()
{
    /* parse query path */
    QStringList qPath;
    for (const auto& p : m_blhmQuery->text().split(','))
        if (!p.trimmed().isEmpty()) qPath << p.trimmed();
    if (qPath.isEmpty()) return;

    double rFactory = 0.0, rLink = 0.0, rUser = 0.0;
    QString out;
    out += QString("Query: [%1]\n\n").arg(qPath.join(", "));
    out += QString("%-32s  match   fw\xc2\xb7m    lw\xc2\xb7m\xc2\xb2   uw\n")
               .arg("Percorso");
    out += QString(65, '-') + "\n";

    const int rows = m_blhmTable->rowCount();
    for (int r = 0; r < rows; ++r) {
        auto* pathItem = m_blhmTable->item(r, 0);
        auto* fwItem   = m_blhmTable->item(r, 1);
        auto* lwItem   = m_blhmTable->item(r, 2);
        auto* uwItem   = m_blhmTable->item(r, 3);
        if (!pathItem) continue;

        QStringList wPath;
        for (const auto& p : pathItem->text().split(','))
            if (!p.trimmed().isEmpty()) wPath << p.trimmed();

        /* match = livelli_in_comune_dall_alto / max(|w|,|q|) */
        int common = 0;
        int minLen = qMin(wPath.size(), qPath.size());
        for (int i = 0; i < minLen; ++i) {
            if (wPath[i] == qPath[i]) ++common;
            else break;
        }
        int    denom = qMax(wPath.size(), qPath.size());
        double match = (denom > 0) ? double(common) / denom : 0.0;

        double fw = fwItem ? fwItem->text().toDouble() : 0.0;
        double lw = lwItem ? lwItem->text().toDouble() : 0.0;
        double uw = uwItem ? uwItem->text().toDouble() : 0.0;

        rFactory += fw * match;
        rLink    += lw * match * match;
        rUser    += uw;

        QString pathStr = QString("[%1]").arg(pathItem->text())
                              .leftJustified(32, ' ');
        out += QString("%1  %2   %3   %4   %5\n")
            .arg(pathStr)
            .arg(match,         5, 'f', 3)
            .arg(fw * match,    6, 'f', 3)
            .arg(lw*match*match,6, 'f', 3)
            .arg(uw,            6, 'f', 3);
    }

    double rMerged = 0.50*rFactory + 0.35*rLink + 0.15*rUser;

    out += "\n" + QString(65, '=') + "\n";
    out += QString("R_factory  = %1\n").arg(rFactory, 0, 'f', 3);
    out += QString("R_link     = %1\n").arg(rLink,    0, 'f', 3);
    out += QString("R_user     = %1\n").arg(rUser,    0, 'f', 3);
    out += QString(65, '-') + "\n";
    out += QString("R_merged   = 0.50 \xc3\x97 %1 + 0.35 \xc3\x97 %2 + 0.15 \xc3\x97 %3\n")
               .arg(rFactory, 0, 'f', 3)
               .arg(rLink,    0, 'f', 3)
               .arg(rUser,    0, 'f', 3);
    out += QString("           = %1 + %2 + %3\n")
               .arg(0.50*rFactory, 0, 'f', 3)
               .arg(0.35*rLink,    0, 'f', 3)
               .arg(0.15*rUser,    0, 'f', 3);
    out += QString("           = %1\n").arg(rMerged, 0, 'f', 3);

    m_blhmOutput->setPlainText(out);
}

void RicercaPage::onBlhmNoteSave()
{
    const QString path = P::root() + "/RAG/BLHM_note.md";
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream(&f) << m_blhmNoteEdit->toPlainText();
}

void RicercaPage::onBlhmNoteLoad()
{
    const QString path = P::root() + "/RAG/BLHM_note.md";
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    m_blhmNoteEdit->setPlainText(QTextStream(&f).readAll());
}

/* ═══════════════════════════════════════════════════════════════════
   buildAnalisiPage — 🌌 Analisi Fenomeni
   Valuta la probabilità che un evento fisico/chimico/alieno/paranormale
   sia realmente accaduto, sulla base delle fonti fornite dall'utente.
   ═══════════════════════════════════════════════════════════════════ */
QWidget* RicercaPage::buildAnalisiPage()
{
    auto* page = new QWidget;
    auto* root = new QVBoxLayout(page);
    root->setContentsMargins(12, 10, 12, 10);
    root->setSpacing(10);

    /* ── Descrizione ── */
    auto* introLbl = new QLabel(
        "<b>\xf0\x9f\x8c\x8c  Analisi Fenomeni</b>"
        "  <span style='color:gray;font-size:11px;'>"
        "L'AI valuta se un evento \xc3\xa8 realmente accaduto sulla base delle fonti."
        "</span>");
    introLbl->setTextFormat(Qt::RichText);
    root->addWidget(introLbl);

    /* ── Categoria ── */
    auto* catBox  = new QGroupBox("Categoria evento");
    auto* catLay  = new QHBoxLayout(catBox);
    catLay->setSpacing(16);

    m_analisiCatGroup = new QButtonGroup(page);
    const struct { const char* label; const char* id; } kCats[] = {
        { "\xe2\x9a\x9b  Fisico",        "Fisico"        },
        { "\xf0\x9f\xa7\xaa  Chimico",   "Chimico"       },
        { "\xf0\x9f\x9b\xb8  Alieno-UAP","Alieno-UAP"    },
        { "\xf0\x9f\x91\xbb  Paranormale","Paranormale"  },
        { "\xe2\x9d\x93  Altro",          "Altro"        },
    };
    for (int i = 0; i < 5; ++i) {
        auto* rb = new QRadioButton(QString::fromUtf8(kCats[i].label));
        rb->setProperty("catId", QString::fromUtf8(kCats[i].id));
        m_analisiCatGroup->addButton(rb, i);
        catLay->addWidget(rb);
        if (i == 0) rb->setChecked(true);
    }
    catLay->addStretch();
    root->addWidget(catBox);

    /* ── Descrizione evento + Fonti affiancate ── */
    auto* midSplit = new QSplitter(Qt::Horizontal);
    midSplit->setHandleWidth(6);

    auto* evtGroup = new QGroupBox("Descrizione dell\xe2\x80\x99" "evento");
    auto* evtLay   = new QVBoxLayout(evtGroup);
    m_analisiEventEdit = new QTextEdit;
    m_analisiEventEdit->setPlaceholderText(
        "Descrivi l\xe2\x80\x99" "evento nel dettaglio:\n"
        "- cosa \xc3\xa8 stato osservato / riportato\n"
        "- quando, dove, da chi\n"
        "- eventuali effetti fisici o testimonianze");
    m_analisiEventEdit->setMinimumHeight(120);
    evtLay->addWidget(m_analisiEventEdit);

    auto* srcGroup = new QGroupBox("Fonti (URL, citazioni, testo grezzo)");
    auto* srcLay   = new QVBoxLayout(srcGroup);
    m_analisiSrcEdit = new QTextEdit;
    m_analisiSrcEdit->setPlaceholderText(
        "Incolla qui le tue fonti:\n"
        "- link ad articoli scientifici o giornali\n"
        "- estratti di testo, PDF, rapporti ufficiali\n"
        "- citazioni di testimoni o esperti\n"
        "(se non hai fonti, scrivi: nessuna fonte disponibile)");
    m_analisiSrcEdit->setMinimumHeight(120);
    srcLay->addWidget(m_analisiSrcEdit);

    midSplit->addWidget(evtGroup);
    midSplit->addWidget(srcGroup);
    midSplit->setStretchFactor(0, 1);
    midSplit->setStretchFactor(1, 1);
    root->addWidget(midSplit);

    /* ── Allegati (file) ── */
    auto* fileBox = new QGroupBox("\xf0\x9f\x93\x82  File allegati (PDF, TXT, MD, CSV, JSON)");
    auto* fileLay = new QVBoxLayout(fileBox);
    auto* fileBtnRow = new QWidget;
    auto* fileBtnLay = new QHBoxLayout(fileBtnRow);
    fileBtnLay->setContentsMargins(0,0,0,0); fileBtnLay->setSpacing(8);
    auto* addFileBtn  = new QPushButton("\xe2\x9e\x95  Aggiungi file\xe2\x80\xa6");
    addFileBtn->setObjectName("actionBtn");
    auto* remFileBtn  = new QPushButton("\xe2\x9c\x96  Rimuovi selezionato");
    auto* fileHintLbl = new QLabel(
        "<span style='color:gray;font-size:11px;'>"
        "I file vengono letti e inclusi nel prompt AI. "
        "PDF: estratto via pdftotext; testo grezzo per gli altri formati."
        "</span>");
    fileHintLbl->setTextFormat(Qt::RichText);
    fileBtnLay->addWidget(addFileBtn);
    fileBtnLay->addWidget(remFileBtn);
    fileBtnLay->addWidget(fileHintLbl, 1);
    m_analisiFileList = new QListWidget;
    m_analisiFileList->setFixedHeight(72);
    m_analisiFileList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_analisiFileList->setAlternatingRowColors(true);
    fileLay->addWidget(fileBtnRow);
    fileLay->addWidget(m_analisiFileList);
    root->addWidget(fileBox);

    connect(addFileBtn, &QPushButton::clicked, this, &RicercaPage::onAnalisiAddFilesClicked);
    connect(remFileBtn, &QPushButton::clicked, this, &RicercaPage::onAnalisiRemoveFileClicked);

    /* ── Riga modello + bottoni ── */
    auto* ctrlRow = new QWidget;
    auto* ctrlLay = new QHBoxLayout(ctrlRow);
    ctrlLay->setContentsMargins(0, 0, 0, 0);
    ctrlLay->setSpacing(8);

    auto* modelLbl = new QLabel("Modello:");
    m_analisiModelCombo = new QComboBox;
    m_analisiModelCombo->setMinimumWidth(220);
    sciPopulateModels(m_analisiModelCombo);

    m_analisiRunBtn  = new QPushButton("\xf0\x9f\x94\x8d  Analizza AI");
    m_analisiStopBtn = new QPushButton("\xe2\x96\xa0  Stop");
    m_analisiRunBtn->setObjectName("actionBtn");
    m_analisiStopBtn->setObjectName("actionBtn");
    m_analisiStopBtn->setEnabled(false);

    ctrlLay->addWidget(modelLbl);
    ctrlLay->addWidget(m_analisiModelCombo, 1);
    ctrlLay->addStretch();
    ctrlLay->addWidget(m_analisiRunBtn);
    ctrlLay->addWidget(m_analisiStopBtn);
    root->addWidget(ctrlRow);

    /* ── Barra probabilità (nascosta finché non arriva il risultato) ── */
    auto* probRow = new QWidget;
    auto* probLay = new QHBoxLayout(probRow);
    probLay->setContentsMargins(0, 0, 0, 0);
    probLay->setSpacing(8);
    m_analisiProbLbl = new QLabel("\xf0\x9f\x93\x8a  Probabilit\xc3\xa0:");
    m_analisiProbBar = new QProgressBar;
    m_analisiProbBar->setRange(0, 100);
    m_analisiProbBar->setValue(0);
    m_analisiProbBar->setTextVisible(true);
    m_analisiProbBar->setFormat("%p%");
    m_analisiProbBar->setFixedHeight(22);
    probLay->addWidget(m_analisiProbLbl);
    probLay->addWidget(m_analisiProbBar, 1);
    probRow->setVisible(false);
    root->addWidget(probRow);

    /* ── Output ── */
    m_analisiOutput = new QTextEdit;
    m_analisiOutput->setReadOnly(false);
    m_analisiOutput->setFont(QFont("Monospace", 10));
    m_analisiOutput->setPlaceholderText(
        "Il risultato dell\xe2\x80\x99" "analisi AI apparir\xc3\xa0 qui.\n\n"
        "La risposta include:\n"
        "  \xf0\x9f\x93\x8a  Probabilit\xc3\xa0 (0-100%)\n"
        "  \xe2\x9c\x85  Evidenze a supporto\n"
        "  \xe2\x9d\x8c  Elementi contraddittori\n"
        "  \xf0\x9f\x94\xac  Spiegazione scientifica pi\xc3\xb9 probabile\n"
        "  \xf0\x9f\x94\x80  Ipotesi alternativa\n"
        "  \xe2\x9a\x96  Verdetto motivato");
    root->addWidget(makeOutputBar(m_analisiOutput, "Analisi Fenomeni", page));
    root->addWidget(m_analisiOutput, 1);

    /* ── Connessioni bottoni ── */
    connect(m_analisiRunBtn,  &QPushButton::clicked, this, &RicercaPage::onAnalisiRunClicked);
    connect(m_analisiStopBtn, &QPushButton::clicked, this, &RicercaPage::onAnalisiStopClicked);

    /* Rende accessibile probRow dai slot tramite setProperty */
    m_analisiOutput->setProperty("probRow", QVariant::fromValue<QWidget*>(probRow));

    return page;
}

/* ─────────────────────────────────────────────────────────────────
   SLOT Analisi Fenomeni
   ───────────────────────────────────────────────────────────────── */
void RicercaPage::onAnalisiRunClicked()
{
    const QString evento = m_analisiEventEdit->toPlainText().trimmed();
    if (evento.isEmpty()) {
        m_analisiOutput->setPlainText(
            "\xe2\x9a\xa0  Inserisci la descrizione dell\xe2\x80\x99" "evento prima di procedere.");
        return;
    }
    if (m_ai->busy()) {
        m_analisiOutput->append("\xe2\x9a\xa0  AI occupata. Attendi o premi Stop.");
        return;
    }

    /* Categoria selezionata */
    QString categoria = "Generico";
    if (auto* btn = m_analisiCatGroup->checkedButton())
        categoria = btn->property("catId").toString();

    /* Nasconde la barra probabilità in attesa del nuovo risultato */
    if (auto* pr = m_analisiOutput->property("probRow").value<QWidget*>())
        pr->setVisible(false);

    /* Selezione modello */
    if (m_analisiModelCombo && m_analisiModelCombo->count() > 0) {
        const QString sel = m_analisiModelCombo->currentData().toString();
        if (!sel.isEmpty() && sel != m_ai->model())
            m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), sel);
    }

    /* System prompt strutturato */
    static const QString kSys =
        "Sei un analista scientifico e critico. Ricevi la descrizione di un evento "
        "e le fonti fornite dall\xe2\x80\x99" "utente.\n"
        "Il tuo compito \xc3\xa8 valutare in modo rigoroso, obiettivo e imparziale "
        "se l\xe2\x80\x99" "evento potrebbe essere realmente accaduto.\n"
        "Usa il pensiero critico e la metodologia scientifica. "
        "Non essere n\xc3\xa9 troppo scettico n\xc3\xa9 troppo credulone.\n"
        "Analizza le fonti con attenzione critica.\n\n"
        "Rispondi SEMPRE con questa struttura esatta:\n\n"
        "## PROBABILIT\xc3\x80: [numero intero da 0 a 100]%\n"
        "(stima obiettiva e motivata basata sulle evidenze)\n\n"
        "## Evidenze a supporto\n"
        "(lista puntata dei fatti/dati che supportano la veridicità dell\xe2\x80\x99" "evento)\n\n"
        "## Elementi contraddittori\n"
        "(fatti, leggi fisiche, incongruenze che contraddicono l\xe2\x80\x99" "evento)\n\n"
        "## Spiegazione scientifica pi\xc3\xb9 probabile\n"
        "(la spiegazione pi\xc3\xb9 razionale e parsimoniosa alla luce della scienza attuale)\n\n"
        "## Ipotesi alternativa\n"
        "(un\xe2\x80\x99" "altra spiegazione plausibile, anche non mainstream, coerente con le evidenze)\n\n"
        "## Verdetto motivato\n"
        "(conclusione finale con ragionamento integrato su tutte le evidenze)";

    /* Legge il contenuto dei file allegati */
    QString fileContent;
    if (m_analisiFileList) {
        for (int fi = 0; fi < m_analisiFileList->count(); ++fi) {
            const QString path = m_analisiFileList->item(fi)->data(Qt::UserRole).toString();
            const QFileInfo fi2(path);
            QString content;
            if (fi2.suffix().toLower() == "pdf") {
                QProcess pdfProc;
                pdfProc.start("pdftotext", {path, "-"});
                pdfProc.waitForFinished(10000);
                content = pdfProc.readAllStandardOutput();
                if (content.trimmed().isEmpty())
                    content = "[PDF " + fi2.fileName() + ": impossibile estrarre testo — installa poppler-utils]";
            } else {
                QFile f(path);
                if (f.open(QIODevice::ReadOnly | QIODevice::Text))
                    content = QTextStream(&f).readAll();
            }
            if (!content.trimmed().isEmpty())
                fileContent += "\n---\nFile: " + fi2.fileName() + "\n" + content.trimmed() + "\n";
        }
    }

    const QString userMsg =
        "**Categoria evento:** " + categoria + "\n\n"
        "**Descrizione dell\xe2\x80\x99" "evento:**\n" + evento + "\n\n"
        "**Fonti disponibili:**\n" + m_analisiSrcEdit->toPlainText().trimmed() +
        (fileContent.isEmpty() ? "" : "\n\n**Documenti allegati:**\n" + fileContent);

    /* Connessioni one-shot */
    QObject::disconnect(m_analisiTokenConn);
    QObject::disconnect(m_analisiFinishedConn);
    QObject::disconnect(m_analisiErrorConn);
    m_analisiTokenConn    = connect(m_ai, &AiClient::token,
                                    this, &RicercaPage::onAnalisiToken);
    m_analisiFinishedConn = connect(m_ai, &AiClient::finished,
                                    this, &RicercaPage::onAnalisiFinished);
    m_analisiErrorConn    = connect(m_ai, &AiClient::error,
                                    this, &RicercaPage::onAnalisiError);

    m_analisiRunBtn->setEnabled(false);
    m_analisiStopBtn->setEnabled(true);
    if (m_sciProgress) m_sciProgress->setVisible(true);

    m_analisiOutput->clear();
    m_analisiOutput->append(
        "\xf0\x9f\x94\x84  Analisi in corso...\n"
        + QString(50, QChar(0x2500)));

    m_ai->chat(kSys, userMsg);
}

void RicercaPage::onAnalisiStopClicked()
{
    m_ai->abort();
}

void RicercaPage::onAnalisiToken(const QString& t)
{
    m_analisiOutput->moveCursor(QTextCursor::End);
    m_analisiOutput->insertPlainText(t);
}

void RicercaPage::onAnalisiFinished(const QString& full)
{
    QObject::disconnect(m_analisiTokenConn);
    QObject::disconnect(m_analisiFinishedConn);
    QObject::disconnect(m_analisiErrorConn);
    m_analisiTokenConn = m_analisiFinishedConn = m_analisiErrorConn = {};

    m_analisiRunBtn->setEnabled(true);
    m_analisiStopBtn->setEnabled(false);
    if (m_sciProgress) m_sciProgress->setVisible(false);
    m_analisiOutput->append("\n" + QString(50, QChar(0x2500)));

    /* Estrae la percentuale dalla riga "## PROBABILITÀ: XX%" */
    static const QRegularExpression kProbRx(
        "PROBABILIT[AÀ][^:]*:\\s*(\\d+)\\s*%",
        QRegularExpression::CaseInsensitiveOption);
    const auto match = kProbRx.match(full);
    if (match.hasMatch()) {
        const int pct = qBound(0, match.captured(1).toInt(), 100);
        m_analisiProbBar->setValue(pct);

        /* Colore barra: verde >60%, giallo 30-60%, rosso <30% */
        const QString color = pct >= 60 ? "#4CAF50"
                            : pct >= 30 ? "#FFC107" : "#F44336";
        m_analisiProbBar->setStyleSheet(
            QString("QProgressBar::chunk { background: %1; border-radius:2px; }").arg(color));

        if (auto* pr = m_analisiOutput->property("probRow").value<QWidget*>())
            pr->setVisible(true);
    }
}

void RicercaPage::onAnalisiError(const QString& msg)
{
    QObject::disconnect(m_analisiTokenConn);
    QObject::disconnect(m_analisiFinishedConn);
    QObject::disconnect(m_analisiErrorConn);
    m_analisiTokenConn = m_analisiFinishedConn = m_analisiErrorConn = {};

    m_analisiRunBtn->setEnabled(true);
    m_analisiStopBtn->setEnabled(false);
    if (m_sciProgress) m_sciProgress->setVisible(false);
    m_sciErrorPanel->showError(msg, [this]{ onAnalisiRunClicked(); });
}
/* ─────────────────────────────────────────────────────────────────
   Slot — Output bar (PDF / Markdown / Svuota)
   Ogni pulsante porta editor e titolo come QObject* property.
   ───────────────────────────────────────────────────────────────── */
void RicercaPage::onOutputBarPdfClicked()
{
    auto* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    auto* editor = qobject_cast<QTextEdit*>(
        btn->property("outputEditor").value<QObject*>());
    const QString titolo = btn->property("outputTitolo").toString();
    if (editor)
        RicercaPage::esportaPdf(editor, titolo, this);
}

void RicercaPage::onOutputBarMdClicked()
{
    auto* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    auto* editor = qobject_cast<QTextEdit*>(
        btn->property("outputEditor").value<QObject*>());
    const QString titolo = btn->property("outputTitolo").toString();
    if (editor)
        RicercaPage::salvaMarkdown(editor, titolo, this);
}

void RicercaPage::onOutputBarClrClicked()
{
    auto* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    auto* editor = qobject_cast<QTextEdit*>(
        btn->property("outputEditor").value<QObject*>());
    if (editor)
        editor->clear();
}

/* ─────────────────────────────────────────────────────────────────
   Slot — RAB₀-L clear
   ───────────────────────────────────────────────────────────────── */
void RicercaPage::onRab0lClearClicked()
{
    if (m_rab0lSeq1)   m_rab0lSeq1->clear();
    if (m_rab0lSeq2)   m_rab0lSeq2->clear();
    if (m_rab0lSimLbl) m_rab0lSimLbl->clear();
    if (m_rab0lCanvas) m_rab0lCanvas->clearAll();
}

/* ─────────────────────────────────────────────────────────────────
   Slot — BLHM tabella pesi
   ───────────────────────────────────────────────────────────────── */
void RicercaPage::onBlhmAddRowClicked()
{
    if (!m_blhmTable) return;
    const int r = m_blhmTable->rowCount();
    m_blhmTable->setRowCount(r + 1);
    m_blhmTable->setItem(r, 0, new QTableWidgetItem("Bio"));
    m_blhmTable->setItem(r, 1, new QTableWidgetItem("0.500"));
    m_blhmTable->setItem(r, 2, new QTableWidgetItem("0.500"));
    m_blhmTable->setItem(r, 3, new QTableWidgetItem("0.000"));
    m_blhmTable->scrollToBottom();
    m_blhmTable->editItem(m_blhmTable->item(r, 0));
}

void RicercaPage::onBlhmDeleteRowClicked()
{
    if (!m_blhmTable || m_blhmTable->selectedItems().isEmpty()) return;
    m_blhmTable->removeRow(m_blhmTable->currentRow());
}

/* ─────────────────────────────────────────────────────────────────
   Slot — BLHM note / DNA clear
   ───────────────────────────────────────────────────────────────── */
void RicercaPage::onBlhmNotesClearClicked()
{
    if (m_blhmNoteEdit) m_blhmNoteEdit->clear();
}

void RicercaPage::onBlhmDnaClearClicked()
{
    if (m_blhmDnaSeq1)   m_blhmDnaSeq1->clear();
    if (m_blhmDnaSeq2)   m_blhmDnaSeq2->clear();
    if (m_blhmDnaSimLbl) m_blhmDnaSimLbl->clear();
    if (m_blhmDnaCanvas) m_blhmDnaCanvas->clearAll();
}

/* ─────────────────────────────────────────────────────────────────
   RicercaPage destructor — libera il grafo C e spegne il thread pool
   ───────────────────────────────────────────────────────────────── */
RicercaPage::~RicercaPage()
{
    if (m_blhmGraph) {
        blhm_graph_free(m_blhmGraph);
        m_blhmGraph = nullptr;
    }
    blhm_pool_shutdown();
}

/* ─────────────────────────────────────────────────────────────────
   BLHM Engine C — sincronizza la tabella Qt nel grafo C
   ───────────────────────────────────────────────────────────────── */
void RicercaPage::onBlhmEngineSyncFromCalcClicked()
{
    if (!m_blhmGraph)
        m_blhmGraph = blhm_graph_create(256);
    blhm_graph_clear(m_blhmGraph);

    const int rows = m_blhmTable ? m_blhmTable->rowCount() : 0;
    for (int r = 0; r < rows; ++r) {
        auto* pathItem = m_blhmTable->item(r, 0);
        auto* fwItem   = m_blhmTable->item(r, 1);
        auto* lwItem   = m_blhmTable->item(r, 2);
        auto* uwItem   = m_blhmTable->item(r, 3);
        if (!pathItem) continue;

        BLHMWeight w = {};
        w.factory_w = fwItem ? float(fwItem->text().toDouble()) : 0.0f;
        w.link_w    = lwItem ? float(lwItem->text().toDouble()) : 0.0f;
        w.user_w    = uwItem ? float(uwItem->text().toDouble()) : 0.0f;

        QStringList parts;
        for (const auto& p : pathItem->text().split(','))
            if (!p.trimmed().isEmpty()) parts << p.trimmed();

        w.path_len = qMin(int(parts.size()), BLHM_MAX_DEPTH);
        for (int i = 0; i < w.path_len; ++i) {
            QByteArray ba = parts[i].toUtf8();
            w.path[i] = blhm_label_register(m_blhmGraph, ba.constData());
        }

        blhm_graph_add(m_blhmGraph, &w);
    }

    if (m_blhmEngineStatusLbl)
        m_blhmEngineStatusLbl->setText(
            QString("Grafo inizializzato: %1 pesi importati dal Calcolatore.")
                .arg(blhm_graph_count(m_blhmGraph)));
}

/* ── Esegui inferenza (3 thread POSIX in parallelo) ── */
void RicercaPage::onBlhmEngineRunClicked()
{
    if (!m_blhmGraph || blhm_graph_count(m_blhmGraph) == 0) {
        if (m_blhmEngineStatusLbl)
            m_blhmEngineStatusLbl->setText(
                "Grafo vuoto \xe2\x80\x94 premi prima \"\xe2\x86\x93 Importa dal Calcolatore\".");
        return;
    }

    /* legge la query dal tab Calcolatore */
    QStringList qParts;
    if (m_blhmQuery) {
        for (const auto& p : m_blhmQuery->text().split(','))
            if (!p.trimmed().isEmpty()) qParts << p.trimmed();
    }
    if (qParts.isEmpty()) {
        if (m_blhmEngineStatusLbl)
            m_blhmEngineStatusLbl->setText(
                "Query vuota. Imposta un percorso nel tab Calcolatore.");
        return;
    }

    QVector<int32_t> qPath;
    qPath.reserve(qParts.size());
    for (const auto& part : qParts) {
        QByteArray ba = part.toUtf8();
        int id = blhm_label_find(m_blhmGraph, ba.constData());
        if (id < 0)
            id = blhm_label_register(m_blhmGraph, ba.constData());
        qPath.append(int32_t(id));
    }

    BLHMResult res = blhm_infer(m_blhmGraph,
                                qPath.data(), qPath.size(),
                                nullptr, 0);

    auto fmtCycle = [](const BLHMCycleResult& c) -> QString {
        QString s = QString("score: %1\nn_active: %2")
                        .arg(double(c.score), 0, 'f', 4)
                        .arg(c.n_active);
        if (c.n_top > 0) {
            s += "\ntop:";
            for (int i = 0; i < c.n_top; ++i)
                s += QString(" #%1(%2)").arg(c.top_idx[i])
                         .arg(double(c.top_score[i]), 0, 'f', 3);
        }
        return s;
    };

    if (m_blhmEngineFactoryLbl)  m_blhmEngineFactoryLbl->setText(fmtCycle(res.factory));
    if (m_blhmEngineLinkLbl)     m_blhmEngineLinkLbl->setText(fmtCycle(res.link));
    if (m_blhmEngineUserLbl)     m_blhmEngineUserLbl->setText(fmtCycle(res.user));
    if (m_blhmEngineCombinedLbl) m_blhmEngineCombinedLbl->setText(
        QString::number(double(res.combined), 'f', 4));
    if (m_blhmEngineLatencyLbl)  m_blhmEngineLatencyLbl->setText(
        QString("%1 ms").arg(res.latency_ms, 0, 'f', 2));
    if (m_blhmEngineStatusLbl)   m_blhmEngineStatusLbl->setText(
        QString("Inferenza completata. Attivi factory/link/user: %1/%2/%3")
            .arg(res.factory.n_active)
            .arg(res.link.n_active)
            .arg(res.user.n_active));
}

/* ── Auto-finetuning Hebbiano: link_w += lr · match ── */
void RicercaPage::onBlhmEngineAutoftClicked()
{
    if (!m_blhmGraph || blhm_graph_count(m_blhmGraph) == 0) {
        if (m_blhmEngineAutoftOut)
            m_blhmEngineAutoftOut->setPlainText("Grafo vuoto.");
        return;
    }

    const int n = blhm_graph_count(m_blhmGraph);
    QVector<float> before(n);
    for (int i = 0; i < n; ++i)
        before[i] = blhm_graph_get(m_blhmGraph, i)->link_w;

    QStringList qParts;
    if (m_blhmQuery) {
        for (const auto& p : m_blhmQuery->text().split(','))
            if (!p.trimmed().isEmpty()) qParts << p.trimmed();
    }
    QVector<int32_t> qPath;
    for (const auto& part : qParts) {
        QByteArray ba = part.toUtf8();
        int id = blhm_label_find(m_blhmGraph, ba.constData());
        if (id < 0) id = blhm_label_register(m_blhmGraph, ba.constData());
        qPath.append(int32_t(id));
    }

    float lr = m_blhmEngineLrSpin ? float(m_blhmEngineLrSpin->value()) : 0.05f;
    blhm_autoft(m_blhmGraph, qPath.data(), qPath.size(), lr);

    QString out = QString("Autoft  LR=%1  query=[%2]\n\n")
                      .arg(double(lr), 0, 'f', 3)
                      .arg(m_blhmQuery ? m_blhmQuery->text() : QString());
    out += QString("%-32s  before      after       delta\n").arg("Peso");
    out += QString(66, '-') + "\n";

    for (int i = 0; i < n; ++i) {
        const BLHMWeight* w = blhm_graph_get(m_blhmGraph, i);
        float after = w->link_w;
        float delta = after - before[i];

        QStringList parts;
        for (int j = 0; j < w->path_len; ++j) {
            const char* nm = blhm_label_name(m_blhmGraph, w->path[j]);
            parts << QString(nm ? nm : "?");
        }
        QString pathStr = QString("[%1]").arg(parts.join(",")).leftJustified(32, ' ');

        out += QString("%1  %2   %3   %4\n")
                   .arg(pathStr)
                   .arg(double(before[i]), 9, 'f', 4)
                   .arg(double(after),     9, 'f', 4)
                   .arg(double(delta),     9, 'f', 4);
    }

    if (m_blhmEngineAutoftOut)
        m_blhmEngineAutoftOut->setPlainText(out);
}

/* ── Salva grafo in file .blhm ── */
void RicercaPage::onBlhmEngineSaveClicked()
{
    if (!m_blhmGraph || blhm_graph_count(m_blhmGraph) == 0) {
        QMessageBox::warning(this, "BLHM Engine",
            "Grafo vuoto \xe2\x80\x94 importa prima i pesi dal Calcolatore.");
        return;
    }
    QString path = QFileDialog::getSaveFileName(
        this, "Salva grafo BLHM",
        P::root() + "/KNOWLEDGE_USER/blhm_graph.blhm",
        "BLHM Graph (*.blhm);;Tutti i file (*)");
    if (path.isEmpty()) return;

    if (!blhm_save(m_blhmGraph, path.toUtf8().constData())) {
        QMessageBox::critical(this, "BLHM Engine",
            "Errore durante il salvataggio del file .blhm.");
    } else {
        if (m_blhmEngineStatusLbl)
            m_blhmEngineStatusLbl->setText(
                QString("Grafo salvato \xe2\x86\x92 %1").arg(path));
    }
}

/* ── Carica grafo da file .blhm ── */
void RicercaPage::onBlhmEngineLoadClicked()
{
    QString path = QFileDialog::getOpenFileName(
        this, "Carica grafo BLHM",
        P::root() + "/KNOWLEDGE_USER",
        "BLHM Graph (*.blhm);;Tutti i file (*)");
    if (path.isEmpty()) return;

    BLHMGraph* g = blhm_load(path.toUtf8().constData());
    if (!g) {
        QMessageBox::critical(this, "BLHM Engine",
            "Impossibile leggere il file .blhm (formato non valido o corrotto).");
        return;
    }
    if (m_blhmGraph) blhm_graph_free(m_blhmGraph);
    m_blhmGraph = g;

    if (m_blhmEngineStatusLbl)
        m_blhmEngineStatusLbl->setText(
            QString("Grafo caricato: %1 pesi da \"%2\".")
                .arg(blhm_graph_count(m_blhmGraph))
                .arg(QFileInfo(path).fileName()));
}

/* ─────────────────────────────────────────────────────────────────
   onBlhmDnaAnalyzeClicked (originale invariato dopo questo punto)
   ───────────────────────────────────────────────────────────────── */
void RicercaPage::onBlhmDnaAnalyzeClicked()
{
    static const QRegularExpression kNonDna("[^ATCGatcg]");

    QString s1 = m_blhmDnaSeq1->text().trimmed().toUpper();
    QString s2 = m_blhmDnaSeq2->text().trimmed().toUpper();
    s1.remove(kNonDna);
    s2.remove(kNonDna);

    if (s1.isEmpty()) return;

    if (s2.isEmpty()) {
        m_blhmDnaCanvas->setSeq1Only(s1);
        m_blhmDnaSimLbl->setText(
            QString("<b>N=%1</b>  <span style='color:gray'>"
                    "inserisci Seq 2 per confronto SIM</span>").arg(s1.size()));
    } else {
        m_blhmDnaCanvas->setSequences(s1, s2);
        const auto& r = m_blhmDnaCanvas->result();
        QString col = (r.sim >= 0.8) ? "#4CAF50"
                    : (r.sim >= 0.4) ? "#FFC107" : "#F44336";
        m_blhmDnaSimLbl->setText(
            QString("<b>SIM = <span style='color:%1'>%2</span></b>"
                    "  |  N=%3  |  SNP=%4")
            .arg(col)
            .arg(r.sim, 0, 'f', 3)
            .arg(r.len)
            .arg(r.snp));
    }
}

/* ─────────────────────────────────────────────────────────────────
   Slot Analisi Fenomeni — gestione file allegati
   ───────────────────────────────────────────────────────────────── */
void RicercaPage::onAnalisiAddFilesClicked()
{
    const QStringList paths = QFileDialog::getOpenFileNames(
        this,
        "Aggiungi file all'analisi",
        QDir::homePath(),
        "Documenti (*.pdf *.txt *.md *.csv *.json *.log *.rst *.tex)");
    if (!m_analisiFileList) return;
    for (const QString& p : paths) {
        const QFileInfo fi(p);
        /* Evita duplicati */
        bool found = false;
        for (int i = 0; i < m_analisiFileList->count(); ++i)
            if (m_analisiFileList->item(i)->data(Qt::UserRole).toString() == p)
                { found = true; break; }
        if (found) continue;
        auto* item = new QListWidgetItem(
            fi.suffix().toUpper() == "PDF" ? "\xf0\x9f\x93\x84  " : "\xf0\x9f\x93\x9d  " +
            fi.fileName() + "  (" +
            QString::number(qRound(fi.size() / 1024.0)) + " KB)");
        item->setData(Qt::UserRole, p);
        item->setToolTip(p);
        m_analisiFileList->addItem(item);
    }
}

void RicercaPage::onAnalisiRemoveFileClicked()
{
    if (!m_analisiFileList) return;
    const auto items = m_analisiFileList->selectedItems();
    for (auto* it : items) delete it;
}

/* ══════════════════════════════════════════════════════════════
   buildRagGrafoTab — 🕸️ Grafo della Conoscenza RAG
   ══════════════════════════════════════════════════════════════ */
QWidget* RicercaPage::buildRagGrafoTab()
{
    /* Inizializza GraphMemory e RagGraph */
    const QString gmPath = QStandardPaths::writableLocation(
                               QStandardPaths::HomeLocation)
                           + "/.prismalux/rag_graph.db";
    m_ragGm    = new GraphMemory(gmPath, this);
    m_ragGm->open();
    m_ragGraph = new RagGraph(m_ai, m_ragGm, this);

    connect(m_ragGraph, &RagGraph::progressUpdated,
            this, &RicercaPage::onRagGraphProgress);
    connect(m_ragGraph, &RagGraph::finished,
            this, &RicercaPage::onRagGraphFinished);
    connect(m_ragGraph, &RagGraph::fileError,
            this, [this](const QString& f, const QString& e) {
                if (m_ragStatus)
                    m_ragStatus->setText(QString("\xe2\x9a\xa0\xef\xb8\x8f  %1: %2").arg(f, e.left(60)));
            });
    connect(m_ragGm, &GraphMemory::changed, this, &RicercaPage::onRagGraphMemChanged);

    auto* w = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    /* ── Barra superiore ── */
    auto* topBar = new QWidget(w);
    topBar->setObjectName("modelBarMath");
    auto* topLay = new QHBoxLayout(topBar);
    topLay->setContentsMargins(12, 8, 12, 8);
    topLay->setSpacing(8);

    auto* titleLbl = new QLabel(
        "\xf0\x9f\x95\xb8  <b>Grafo della Conoscenza RAG</b>"
        " — entit\xc3\xa0 e relazioni estratte dai tuoi documenti",  /* entità */
        topBar);
    titleLbl->setTextFormat(Qt::RichText);
    titleLbl->setObjectName("cardDesc");
    topLay->addWidget(titleLbl, 1);

    m_ragModelCombo = new QComboBox(topBar);
    m_ragModelCombo->setObjectName("settingCombo");
    m_ragModelCombo->setMinimumWidth(160);
    m_ragModelCombo->setToolTip("Modello LLM per estrazione entit\xc3\xa0");
    if (m_ai) {
        const QString cur = m_ai->model();
        m_ragModelCombo->addItem(cur.isEmpty() ? "(caricamento...)" : cur, cur);
    }
    topLay->addWidget(m_ragModelCombo);

    lay->addWidget(topBar);

    auto* sep1 = new QFrame(w);
    sep1->setFrameShape(QFrame::HLine); sep1->setFrameShadow(QFrame::Sunken);
    lay->addWidget(sep1);

    /* ── Barra controlli ── */
    auto* ctrlBar = new QWidget(w);
    ctrlBar->setObjectName("modelBarMath");
    auto* ctrlLay = new QHBoxLayout(ctrlBar);
    ctrlLay->setContentsMargins(12, 6, 12, 6);
    ctrlLay->setSpacing(8);

    m_ragRunBtn = new QPushButton(
        "\xf0\x9f\x94\x84  Analizza RAG", w);  /* 🔄 */
    m_ragRunBtn->setObjectName("actionBtn");
    m_ragRunBtn->setProperty("highlight", "true");
    m_ragRunBtn->setToolTip(
        "Scansiona ~/prismalux_rag_docs/ e Prismalux/RAG/,"
        " estrae entit\xc3\xa0 e relazioni con LLM");
    connect(m_ragRunBtn, &QPushButton::clicked, this, &RicercaPage::onRagRunClicked);
    ctrlLay->addWidget(m_ragRunBtn);

    m_ragStopBtn = new QPushButton("\xe2\x96\xa0  Stop", w);  /* ■ */
    m_ragStopBtn->setObjectName("stopBtn");
    m_ragStopBtn->setEnabled(false);
    connect(m_ragStopBtn, &QPushButton::clicked, this, &RicercaPage::onRagStopClicked);
    ctrlLay->addWidget(m_ragStopBtn);

    m_ragClearBtn = new QPushButton("\xf0\x9f\x97\x91  Svuota", w);  /* 🗑 */
    m_ragClearBtn->setObjectName("navBtn");
    connect(m_ragClearBtn, &QPushButton::clicked, this, &RicercaPage::onRagClearClicked);
    ctrlLay->addWidget(m_ragClearBtn);

    auto* btnRefreshDot = new QPushButton("\xf0\x9f\x8c\xbf  Rigenera Grafo", w);
    btnRefreshDot->setObjectName("navBtn");
    btnRefreshDot->setToolTip("Rigenera la visualizzazione Graphviz dal grafo corrente");
    connect(btnRefreshDot, &QPushButton::clicked, this, &RicercaPage::onRagRefreshDot);
    ctrlLay->addWidget(btnRefreshDot);

    ctrlLay->addStretch(1);

    m_ragStatus = new QLabel("\xf0\x9f\x95\xb8  Pronto.", ctrlBar);
    m_ragStatus->setObjectName("statusLabel");
    ctrlLay->addWidget(m_ragStatus, 2);

    m_ragProgress = new QProgressBar(ctrlBar);
    m_ragProgress->setRange(0, 0);
    m_ragProgress->setVisible(false);
    m_ragProgress->setFixedHeight(8);
    m_ragProgress->setFixedWidth(120);
    ctrlLay->addWidget(m_ragProgress);

    lay->addWidget(ctrlBar);

    auto* sep2 = new QFrame(w);
    sep2->setFrameShape(QFrame::HLine); sep2->setFrameShadow(QFrame::Sunken);
    lay->addWidget(sep2);

    /* ── Splitter principale: lista nodi | visualizzazione ── */
    auto* splitter = new QSplitter(Qt::Horizontal, w);
    splitter->setHandleWidth(4);

    /* ── Pannello sx: ricerca + lista nodi ── */
    auto* leftPanel = new QWidget(splitter);
    auto* leftLay   = new QVBoxLayout(leftPanel);
    leftLay->setContentsMargins(8, 6, 4, 6);
    leftLay->setSpacing(4);

    auto* searchRow = new QHBoxLayout;
    auto* searchLbl = new QLabel("\xf0\x9f\x94\x8d", leftPanel);
    searchRow->addWidget(searchLbl);
    m_ragSearchEdit = new QLineEdit(leftPanel);
    m_ragSearchEdit->setPlaceholderText("Cerca nodo...");
    m_ragSearchEdit->setObjectName("settingCombo");
    connect(m_ragSearchEdit, &QLineEdit::textChanged,
            this, &RicercaPage::onRagSearchChanged);
    searchRow->addWidget(m_ragSearchEdit, 1);
    leftLay->addLayout(searchRow);

    auto* nodeLbl = new QLabel("<b>Nodi del grafo</b>", leftPanel);
    nodeLbl->setTextFormat(Qt::RichText);
    nodeLbl->setObjectName("cardDesc");
    leftLay->addWidget(nodeLbl);

    m_ragNodeList = new QListWidget(leftPanel);
    m_ragNodeList->setObjectName("chatLog");
    m_ragNodeList->setAlternatingRowColors(true);
    connect(m_ragNodeList, &QListWidget::itemClicked,
            this, &RicercaPage::onRagNodeClicked);
    leftLay->addWidget(m_ragNodeList, 1);

    m_ragNodeDetail = new QTextEdit(leftPanel);
    m_ragNodeDetail->setObjectName("chatLog");
    m_ragNodeDetail->setReadOnly(true);
    m_ragNodeDetail->setMaximumHeight(120);
    m_ragNodeDetail->setPlaceholderText("Clicca un nodo per i dettagli...");
    leftLay->addWidget(m_ragNodeDetail);

    splitter->addWidget(leftPanel);

    /* ── Pannello dx: Graphviz + DOT ── */
    auto* rightPanel = new QWidget(splitter);
    auto* rightLay   = new QVBoxLayout(rightPanel);
    rightLay->setContentsMargins(4, 6, 8, 6);
    rightLay->setSpacing(4);

    auto* vizTabs = new QTabWidget(rightPanel);
    vizTabs->setObjectName("settingsInnerTabs");
    vizTabs->setDocumentMode(true);

    /* Tab 1: Immagine Graphviz */
    auto* imgScroll = new QScrollArea(vizTabs);
    imgScroll->setWidgetResizable(true);
    imgScroll->setFrameShape(QFrame::NoFrame);
    m_ragImgLbl = new QLabel(imgScroll);
    m_ragImgLbl->setAlignment(Qt::AlignCenter);
    m_ragImgLbl->setText(
        "<p style='color:#64748b'>Il grafo apparir\xc3\xa0 qui dopo l'analisi.<br>"
        "Richiede <b>graphviz</b> installato: <code>sudo apt install graphviz</code></p>");
    m_ragImgLbl->setTextFormat(Qt::RichText);
    imgScroll->setWidget(m_ragImgLbl);
    vizTabs->addTab(imgScroll, "\xf0\x9f\x96\xbc  Grafo");  /* 🖼 */

    /* Tab 2: DOT sorgente */
    m_ragDotView = new QTextEdit(vizTabs);
    m_ragDotView->setObjectName("chatLog");
    m_ragDotView->setReadOnly(true);
    m_ragDotView->setLineWrapMode(QTextEdit::NoWrap);
    m_ragDotView->setPlaceholderText("Graphviz DOT del grafo...");
    vizTabs->addTab(m_ragDotView, "\xf0\x9f\x93\x9d  DOT");  /* 📝 */

    rightLay->addWidget(vizTabs, 1);
    splitter->addWidget(rightPanel);

    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
    lay->addWidget(splitter, 1);

    return w;
}

/* ══════════════════════════════════════════════════════════════
   Slot: onRagRunClicked
   ══════════════════════════════════════════════════════════════ */
void RicercaPage::onRagRunClicked()
{
    if (!m_ragGraph || m_ragGraph->isRunning()) return;
    if (!m_ai) return;

    /* Seleziona modello */
    const QString sel = m_ragModelCombo ? m_ragModelCombo->currentData().toString() : QString();
    if (!sel.isEmpty() && sel != m_ai->model())
        m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), sel);

    /* Aggiungi directory RAG */
    const QString ragDocs = QDir::homePath() + "/prismalux_rag_docs";
    const QString ragLocal = P::root() + "/RAG";

    if (!QDir(ragDocs).exists() && !QDir(ragLocal).exists()) {
        if (m_ragStatus)
            m_ragStatus->setText("\xe2\x9a\xa0\xef\xb8\x8f  Nessuna cartella RAG trovata. "
                                 "Aggiungi documenti in ~/prismalux_rag_docs/");
        return;
    }

    /* Reset RagGraph (ricrea per pulire la coda) */
    delete m_ragGraph;
    m_ragGraph = new RagGraph(m_ai, m_ragGm, this);
    connect(m_ragGraph, &RagGraph::progressUpdated,
            this, &RicercaPage::onRagGraphProgress);
    connect(m_ragGraph, &RagGraph::finished,
            this, &RicercaPage::onRagGraphFinished);
    connect(m_ragGraph, &RagGraph::fileError,
            this, [this](const QString& f, const QString& e) {
                if (m_ragStatus)
                    m_ragStatus->setText("\xe2\x9a\xa0\xef\xb8\x8f  " + f + ": " + e.left(60));
            });

    if (QDir(ragDocs).exists()) m_ragGraph->addDirectory(ragDocs);
    if (QDir(ragLocal).exists()) m_ragGraph->addDirectory(ragLocal);

    if (m_ragGraph->stats().totalFiles == 0) {
        m_ragStatus->setText("\xe2\x9a\xa0\xef\xb8\x8f  Nessun file trovato nelle cartelle RAG.");
        return;
    }

    m_ragRunBtn->setEnabled(false);
    m_ragStopBtn->setEnabled(true);
    if (m_ragProgress) m_ragProgress->setVisible(true);
    m_ragStatus->setText(QString("\xf0\x9f\x94\x84  Analisi in corso (%1 file)...")
                         .arg(m_ragGraph->stats().totalFiles));

    m_ragGraph->startIngest();
}

void RicercaPage::onRagStopClicked()
{
    if (m_ragGraph) m_ragGraph->stopIngest();
    if (m_ragRunBtn)   m_ragRunBtn->setEnabled(true);
    if (m_ragStopBtn)  m_ragStopBtn->setEnabled(false);
    if (m_ragProgress) m_ragProgress->setVisible(false);
    if (m_ragStatus)   m_ragStatus->setText("\xe2\x96\xa0  Analisi interrotta.");
}

void RicercaPage::onRagClearClicked()
{
    onRagStopClicked();
    if (m_ragGm) m_ragGm->clearAll();
    if (m_ragNodeList) m_ragNodeList->clear();
    if (m_ragNodeDetail) m_ragNodeDetail->clear();
    if (m_ragImgLbl) m_ragImgLbl->clear();
    if (m_ragDotView) m_ragDotView->clear();
    if (m_ragStatus) m_ragStatus->setText("\xf0\x9f\x97\x91  Grafo svuotato.");
}

/* ══════════════════════════════════════════════════════════════
   Slot: progress e finish da RagGraph
   ══════════════════════════════════════════════════════════════ */
void RicercaPage::onRagGraphProgress(int cur, int tot, const QString& file)
{
    if (m_ragStatus)
        m_ragStatus->setText(QString("\xf0\x9f\x94\x84  %1/%2: %3")
                             .arg(cur).arg(tot).arg(file.left(40)));
}

void RicercaPage::onRagGraphFinished(const RagGraphStats& stats)
{
    if (m_ragRunBtn)   m_ragRunBtn->setEnabled(true);
    if (m_ragStopBtn)  m_ragStopBtn->setEnabled(false);
    if (m_ragProgress) m_ragProgress->setVisible(false);

    if (m_ragStatus)
        m_ragStatus->setText(
            QString("\xe2\x9c\x85  Completato: %1 file, %2 entit\xc3\xa0, %3 relazioni.")
            .arg(stats.processedFiles)
            .arg(stats.totalEntities)
            .arg(stats.totalRelations));

    onRagRefreshDot();
}

/* ══════════════════════════════════════════════════════════════
   Slot: aggiornamento lista nodi su changed()
   ══════════════════════════════════════════════════════════════ */
void RicercaPage::onRagGraphMemChanged()
{
    if (!m_ragGm || !m_ragNodeList) return;
    const QString filter = m_ragSearchEdit ? m_ragSearchEdit->text() : QString();
    const auto nodes = filter.isEmpty()
        ? m_ragGm->allNodes()
        : m_ragGm->searchNodes(filter, 100);

    m_ragNodeList->clear();
    for (const auto& n : nodes) {
        auto* item = new QListWidgetItem(m_ragNodeList);
        /* Icona per tipo */
        QString icon;
        const QString t = n.type;
        if (t == "framework" || t == "libreria" || t == "tecnologia")
            icon = "\xf0\x9f\x94\xa7 ";   /* 🔧 */
        else if (t == "formula" || t == "algoritmo" || t == "concetto")
            icon = "\xcf\x80 ";            /* π */
        else if (t == "persona")
            icon = "\xf0\x9f\x91\xa4 ";   /* 👤 */
        else if (t == "documento")
            icon = "\xf0\x9f\x93\x84 ";   /* 📄 */
        else
            icon = "\xf0\x9f\x94\xb5 ";   /* 🔵 */

        item->setText(icon + n.label + " [" + t + "]");
        item->setData(Qt::UserRole, n.id);
        item->setToolTip(n.content.left(120));
    }
}

void RicercaPage::onRagSearchChanged(const QString& /*q*/)
{
    onRagGraphMemChanged();
}

/* ══════════════════════════════════════════════════════════════
   Slot: click nodo → dettaglio + vicini
   ══════════════════════════════════════════════════════════════ */
void RicercaPage::onRagNodeClicked(QListWidgetItem* item)
{
    if (!item || !m_ragGm || !m_ragNodeDetail) return;
    const QString nodeId = item->data(Qt::UserRole).toString();
    const auto node = m_ragGm->nodeById(nodeId);
    if (!node.has_value()) return;

    QString detail;
    detail += "<b>" + node->label.toHtmlEscaped() + "</b>";
    detail += " <span style='color:#64748b'>[" + node->type + "]</span>";
    detail += "<br>Importanza: " + QString::number(node->importance, 'f', 2);
    if (!node->content.isEmpty())
        detail += "<br><br>" + node->content.left(300).toHtmlEscaped();

    /* Vicini */
    const auto nbrs = m_ragGm->neighbours(nodeId, 1);
    if (!nbrs.isEmpty()) {
        detail += "<br><br><b>Connesso a:</b> ";
        QStringList nbrLabels;
        for (const auto& nb : nbrs)
            nbrLabels << nb.label.toHtmlEscaped();
        detail += nbrLabels.join(", ");
    }

    m_ragNodeDetail->setHtml(detail);
}

/* ══════════════════════════════════════════════════════════════
   Slot: rigenera DOT e immagine Graphviz
   ══════════════════════════════════════════════════════════════ */
void RicercaPage::onRagRefreshDot()
{
    if (!m_ragGm) return;

    const QString dot = m_ragGm->toDot("Grafo RAG Prismalux", 80);
    if (m_ragDotView) m_ragDotView->setPlainText(dot);

    /* Genera PNG con Graphviz */
    m_ragTmpDot = QDir::tempPath() + "/prismalux_rag_graph.dot";
    m_ragTmpPng = QDir::tempPath() + "/prismalux_rag_graph.png";

    QFile df(m_ragTmpDot);
    if (df.open(QFile::WriteOnly | QFile::Text))
        QTextStream(&df) << dot;

    if (m_ragDotProc && m_ragDotProc->state() != QProcess::NotRunning)
        m_ragDotProc->kill();

    m_ragDotProc = new QProcess(this);
    connect(m_ragDotProc,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &RicercaPage::onRagDotProcFinished);

    m_ragDotProc->start("dot", {"-Tpng", m_ragTmpDot, "-o", m_ragTmpPng});
    if (!m_ragDotProc->waitForStarted(2000)) {
        if (m_ragImgLbl)
            m_ragImgLbl->setText(
                "<p style='color:#f87171'>\xe2\x9d\x8c  Graphviz non trovato.<br>"
                "Installa con: <code>sudo apt install graphviz</code></p>");
        m_ragDotProc->deleteLater();
        m_ragDotProc = nullptr;
    }
}

void RicercaPage::onRagDotProcFinished(int code, QProcess::ExitStatus /*status*/)
{
    if (m_ragDotProc) { m_ragDotProc->deleteLater(); m_ragDotProc = nullptr; }
    if (code != 0 || !m_ragImgLbl) return;

    QPixmap px(m_ragTmpPng);
    if (!px.isNull()) {
        const int maxW = m_ragImgLbl->parentWidget()
                         ? m_ragImgLbl->parentWidget()->width() - 20 : 800;
        if (px.width() > maxW)
            px = px.scaledToWidth(maxW, Qt::SmoothTransformation);
        m_ragImgLbl->setPixmap(px);
    }
}
