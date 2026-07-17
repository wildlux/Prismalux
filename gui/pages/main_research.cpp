#include "main_research.h"
#include "../dpi_utils.h"
#include "main_jobs.h"
#include "../prismalux_paths.h"
#include "../widgets/astro_calc.h"
#include "../widgets/external_ai_import.h"
#include "../log_bus.h"
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
#include <QSettings>
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
#include <QFileSystemWatcher>
#include "../widgets/model_combo_box.h"
#include "../widgets/proc_helper.h"
#include <cmath>

/* ── helper: barra azioni output (Esporta PDF / Salva .md) ────────── */
static QWidget* makeOutputBar(QTextEdit* editor, const QString& titolo,
                              QWidget* parent, RicercaPage* rp)
{
    auto* bar  = new QWidget(parent);
    auto* blay = new QHBoxLayout(bar);
    blay->setContentsMargins(0, 0, 0, 0);
    blay->setSpacing(6);

    auto* lblTit = new QLabel(QObject::tr("<b>") + titolo + "</b>");
    auto* btnPdf = new QPushButton(QObject::tr("\xf0\x9f\x96\xa8  Esporta PDF"));
    auto* btnMd  = new QPushButton(QObject::tr("\xf0\x9f\x92\xbe  Salva .md"));
    auto* btnClr = new QPushButton(QObject::tr("\xf0\x9f\x97\x91  Svuota"));
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

    if (rp) {
        QObject::connect(btnPdf, &QPushButton::clicked, rp, &RicercaPage::onOutputBarPdfClicked);
        QObject::connect(btnMd,  &QPushButton::clicked, rp, &RicercaPage::onOutputBarMdClicked);
        QObject::connect(btnClr, &QPushButton::clicked, rp, &RicercaPage::onOutputBarClrClicked);
    }
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
    header->setFixedHeight(dpiScale(36));
    vlay->addWidget(header);

    auto* tabs = new QTabWidget(this);
    tabs->setObjectName("settingsInnerTabs");
    tabs->setDocumentMode(true);
    /* ── Gruppo 1: Genera ── */
    tabs->addTab(buildPaperTab(),             "\xf0\x9f\x93\x84  Paper");
    tabs->addTab(buildBrevettoTab(),          "\xf0\x9f\x94\x8f  Brevetto");
    tabs->addTab(buildDocTecnicoTab(),        "\xf0\x9f\x93\x8b  Doc Tecnico");
    /* ── Gruppo 2: Cerca ── */
    tabs->addTab(buildCercaLetteraturaTab(),  tr("\xf0\x9f\x94\x8d  Cerca Paper/Brevetti"));
    /* Lavoro spostato in UtilityPage [5] */
    /* Cytoscape/RDKit/Bioconda/Avogadro/RAB\xe2\x82\x80-L/BLHM \xe2\x86\x92 BioinformaticaPage [7] */
    /* ── Gruppo 5: Analisi eventi ── */
    tabs->addTab(buildAnalisiPage(),
                 "\xf0\x9f\x8c\x8c  Analisi Fenomeni");
    /* ── Gruppo 6: Astrologia ── */
    tabs->addTab(buildRagGrafoTab(),
                 "\xf0\x9f\x95\xb8  Grafo RAG");   /* 🕸️ */
    tabs->addTab(buildRagTesterTab(),
                 "\xf0\x9f\xa7\xaa  Test RAG");    /* 🧪 */
    tabs->addTab(buildSdsEditingTab(),
                 "\xf0\x9f\xa7\xac  SDS Editing");  /* 🧬 */
    /* Nascosta di default — easter egg stile "developer mode" Android:
       doppio click sulla parola "conoscenza" in Impostazioni →
       Ringraziamenti la sblocca in modo persistente (vedi
       unlockAstraleEasterEgg() in settings_other.cpp). */
    const bool astraleUnlocked = QSettings("Prismalux", "GUI")
        .value(P::SK::kAstraleUnlocked, false).toBool();
    if (astraleUnlocked) {
        tabs->addTab(buildAstraleTab(),
                     "\xe2\xad\x90  Carta Astrale");
    }

    /* Tooltip sui tab per scopribilità */
    tabs->setTabToolTip(0, "Genera paper accademico con AI");
    tabs->setTabToolTip(1, "Genera documento brevettuale PCT/EPO");
    tabs->setTabToolTip(2, "Genera specifiche tecniche e manuali");
    tabs->setTabToolTip(3, "Cerca su arXiv, Semantic Scholar, USPTO");
    tabs->setTabToolTip(4,
        "Analisi Fenomeni: valuta la probabilit\xc3\xa0 che un evento fisico/chimico/alieno/paranormale"
        " sia realmente accaduto, sulla base delle fonti fornite");
    tabs->setTabToolTip(5, "Grafo Conoscenza estratto dai documenti RAG");
    tabs->setTabToolTip(6, "Test comprensione documenti RAG \xe2\x80\x94 domande + valutazione AI");
    tabs->setTabToolTip(7,
        "SDS Editing: pipeline di STUDIO (non clinica) per la Sindrome di Shwachman-Diamond"
        " \xe2\x80\x94 esegue Tools/sds_editing/run_all.py");
    if (astraleUnlocked) {
        tabs->setTabToolTip(8,
            "Carta Astrale / Tema Natale: inserisci data, ora e luogo di nascita"
            " per una lettura astrologica con AI");
    }
    vlay->addWidget(tabs, 1);

    m_sciProgress = new QProgressBar(this);
    m_sciProgress->setRange(0, 0);
    m_sciProgress->setFixedHeight(dpiScale(4));
    m_sciProgress->setTextVisible(false);
    m_sciProgress->setVisible(false);
    vlay->addWidget(m_sciProgress);

    m_sciErrorPanel = new AiErrorWidget(this);
    vlay->addWidget(m_sciErrorPanel);

    /* ModelComboBox gestisce il fetch autonomamente per ogni combo */

    /* ── connessioni AI (una sola volta per tutta la pagina) ──────── */
    connect(m_ai, &AiClient::token,    this, &RicercaPage::onAiToken);
    connect(m_ai, &AiClient::finished, this, &RicercaPage::onAiFinished);
    connect(m_ai, &AiClient::error,    this, &RicercaPage::onAiError);
    connect(m_ai, &AiClient::aborted,  this, &RicercaPage::onAiAborted);
}

RicercaPage::~RicercaPage() {}

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
    editTitolo->setPlaceholderText(tr("es. BLHM: A Novel Hybrid LLM Architecture"));
    auto* editAutori  = new QLineEdit;
    editAutori->setPlaceholderText(tr("es. Mario Rossi, Luigi Verdi"));
    auto* editKw      = new QLineEdit;
    editKw->setPlaceholderText(tr("es. LLM, ontology, parallel inference"));
    auto* cmbTipo     = new QComboBox;
    cmbTipo->addItems({"Preprint arXiv", "Conference Paper", "Journal Article",
                       "Workshop Paper", "Technical Report"});
    auto* cmbLingua   = new QComboBox;
    cmbLingua->addItems({"English", "Italiano"});
    auto* editAbstract = new QTextEdit;
    editAbstract->setPlaceholderText(
        tr("Descrivi l'idea, l'approccio e i risultati principali.\n"
        "L'AI espanderà tutto in un paper completo."));
    editAbstract->setFixedHeight(dpiScale(140));

    form->addRow("Titolo:",    editTitolo);
    form->addRow("Autori:",    editAutori);
    form->addRow("Keywords:", editKw);
    form->addRow("Tipo:",      cmbTipo);
    form->addRow("Lingua:",    cmbLingua);
    form->addRow("Abstract /\nDescrizione:", editAbstract);

    m_paperModel = new ModelComboBox(m_ai, formW);
    form->addRow("\xf0\x9f\xa4\x96  Modello:", m_paperModel);

    auto* btnRow  = new QWidget;
    auto* btnLay  = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(0, 4, 0, 0);
    auto* btnGen  = new QPushButton(tr("\xf0\x9f\x9a\x80  Genera Paper"));
    auto* btnStop = new QPushButton(tr("\xe2\x96\xa0  Stop"));
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
        tr("L'output del paper apparirà qui.\n"
        "Puoi modificarlo dopo la generazione."));
    outEdit->setFont(QFont("Monospace", 10));

    outLay->addWidget(makeOutputBar(outEdit, "Paper Scientifico", page, this));
    outLay->addWidget(outEdit, 1);

    hlay->addWidget(formScroll);
    hlay->addWidget(outW, 1);

    /* ── connessioni ── */
    auto doGenera = [=]{
        const QString titolo = editTitolo->text().trimmed();
        if (titolo.isEmpty()) {
            outEdit->setPlainText(
                "\xe2\x9d\x8c  Inserisci almeno il titolo del paper.");
            LogBus::post("\xe2\x9d\x8c Ricerca: Inserisci almeno il titolo del paper.");
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
        if (m_paperModel && m_paperModel->count() > 0) {
            const QString sel = m_paperModel->currentData().toString();
            if (!sel.isEmpty() && sel != m_ai->model())
                m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), sel);
        }
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
        tr("es. Sistema ibrido di inferenza ontologica per LLM"));
    auto* editInventori = new QLineEdit;
    editInventori->setPlaceholderText(tr("es. Mario Rossi (IT)"));
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
        tr("es. Ridurre la complessità computazionale O(n²) "
        "dei transformer per query ontologiche"));
    auto* editDesc = new QTextEdit;
    editDesc->setPlaceholderText(
        tr("Descrivi l'invenzione in dettaglio:\n"
        "come funziona, cosa la distingue dallo stato dell'arte,\n"
        "applicazioni pratiche."));
    editDesc->setFixedHeight(dpiScale(140));

    form->addRow("Titolo:", editTitolo);
    form->addRow("Inventori:", editInventori);
    form->addRow("IPC:", cmbIpc);
    form->addRow("Problema\nrisolto:", editProblema);
    form->addRow("Descrizione\ninvenzione:", editDesc);

    m_brevettoModel = new ModelComboBox(m_ai, formW);
    form->addRow("\xf0\x9f\xa4\x96  Modello:", m_brevettoModel);

    auto* btnRow  = new QWidget;
    auto* btnLay  = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(0, 4, 0, 0);
    auto* btnGen  = new QPushButton(tr("\xf0\x9f\x94\x8f  Genera Brevetto"));
    auto* btnStop = new QPushButton(tr("\xe2\x96\xa0  Stop"));
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
        tr("Il testo del brevetto apparirà qui.\n"
        "Sezioni: Titolo, Campo, Background, Sommario, "
        "Descrizione dettagliata, Rivendicazioni, Abstract."));
    outEdit->setFont(QFont("Monospace", 10));

    outLay->addWidget(makeOutputBar(outEdit, "Brevetto", page, this));
    outLay->addWidget(outEdit, 1);

    hlay->addWidget(formScroll);
    hlay->addWidget(outW, 1);

    /* ── connessioni ── */
    connect(btnGen, &QPushButton::clicked, page, [=]{
        const QString titolo = editTitolo->text().trimmed();
        if (titolo.isEmpty()) {
            outEdit->setPlainText(
                "\xe2\x9d\x8c  Inserisci il titolo dell'invenzione.");
            LogBus::post("\xe2\x9d\x8c Ricerca: Inserisci il titolo dell'invenzione.");
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
        if (m_brevettoModel && m_brevettoModel->count() > 0) {
            const QString sel = m_brevettoModel->currentData().toString();
            if (!sel.isEmpty() && sel != m_ai->model())
                m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), sel);
        }
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
        tr("es. BLHM — Documento Tecnico v1.0"));
    auto* editAutore = new QLineEdit;
    editAutore->setPlaceholderText(tr("es. wildlux"));
    auto* editVers   = new QLineEdit;
    editVers->setPlaceholderText(tr("es. 1.0"));
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
        tr("Descrivi il progetto/sistema:\n"
        "- Cosa fa\n- Come funziona\n"
        "- Dati e risultati misurati\n"
        "- Specifiche tecniche rilevanti"));
    editDesc->setFixedHeight(dpiScale(150));

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
        tr("\xf0\x9f\x93\x8b  Genera Documento"));
    auto* btnStop = new QPushButton(tr("\xe2\x96\xa0  Stop"));
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
        tr("Il documento tecnico apparirà qui.\n"
        "Sezioni: Sommario Esecutivo, Introduzione, Architettura, "
        "Specifiche, Calcoli, Risultati, Limitazioni, Conclusioni."));
    outEdit->setFont(QFont("Monospace", 10));

    outLay->addWidget(makeOutputBar(outEdit, "Documento Tecnico", page, this));
    outLay->addWidget(outEdit, 1);

    hlay->addWidget(formScroll);
    hlay->addWidget(outW, 1);

    /* ── connessioni ── */
    connect(btnGen, &QPushButton::clicked, page, [=]{
        const QString nome = editNome->text().trimmed();
        if (nome.isEmpty()) {
            outEdit->setPlainText(
                "\xe2\x9d\x8c  Inserisci il nome del progetto.");
            LogBus::post("\xe2\x9d\x8c Ricerca: Inserisci il nome del progetto.");
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
        statusLbl->setText(QObject::tr("\xe2\x9d\x8c  Impossibile creare script temporaneo"));
        LogBus::post("\xe2\x9d\x8c Ricerca: Impossibile creare script temporaneo.");
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
                ? QObject::tr("\xe2\x9c\x85  Completato")
                : QObject::tr("\xe2\x9d\x8c  Terminato con errore"));
            execBtn->setEnabled(true);
        });
        QObject::connect(procRef, &QProcess::errorOccurred,
            parent, [procRef](QProcess::ProcessError err) {
            if (err == QProcess::FailedToStart)
                qWarning() << "[research_sci] processo non avviato:" << procRef->program();
        });
    }
    execBtn->setEnabled(false);
    statusLbl->setText(QObject::tr("\xf0\x9f\x94\x84  Esecuzione..."));
    if (isBash) {
        const QString bash = QStandardPaths::findExecutable("bash");
        procRef->start(bash.isEmpty() ? "bash" : bash, {tmpPath});
    } else {
        procRef->start(P::findPython(), {tmpPath});
    }
    if (procRef->state() == QProcess::NotRunning) {
        statusLbl->setText(QObject::tr("\xe2\x9d\x8c  Interprete non trovato"));
        LogBus::post("\xe2\x9d\x8c Ricerca: Interprete non trovato.");
    }
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
        tr("\xf0\x9f\x94\x8d  <b>Cerca Paper e Brevetti</b> \xe2\x80\x94 "
        "Ricerca su <b>arXiv</b>, <b>Semantic Scholar</b> e <b>USPTO</b> "
        "senza account o API key. Poi analizza i risultati con AI."), w);
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
        tr("Es: quantum computing error correction / battery cathode material ..."));

    m_litSource = new QComboBox(w);
    m_litSource->addItem("\xf0\x9f\x93\x84  arXiv",           "arxiv");
    m_litSource->addItem("\xf0\x9f\x94\xac  Semantic Scholar", "semantic");
    m_litSource->addItem("\xf0\x9f\x94\x8f  USPTO Brevetti",   "uspto");
    m_litSource->setFixedWidth(dpiScale(180));

    m_litSearchBtn = new QPushButton(tr("\xf0\x9f\x94\x8d  Cerca"), w);
    m_litSearchBtn->setObjectName("actionBtn");
    m_litSearchBtn->setFixedWidth(dpiScale(90));

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
    m_litResults->setPlaceholderText(tr("I risultati appariranno qui..."));
    lay->addWidget(m_litResults, 1);

    m_litAiBtn = new QPushButton(
        tr("\xf0\x9f\xa4\x96  Analizza con AI"), w);
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
   SLOT — AI globali (Paper / Brevetto / DocTecnico via avvia())
   ══════════════════════════════════════════════════════════════ */

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
                        m_sciStatusLbl->setText(tr("\xe2\x9c\x85  Codice pronto \xe2\x80\x94 premi Esegui"));
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
    LogBus::post("\xe2\x9d\x8c Ricerca: Errore AI: " + msg);
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
    LogBus::post("\xe2\x9d\x8c Ricerca: Errore AI: " + err);
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
        m_astraleRunBtn->setText(tr("\xe2\xad\x90  Leggi gli Astri"));
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
        m_litStatus->setText(tr("\xe2\x9a\xa0  Inserisci una query."));
        return;
    }
    m_litResults->clear();
    m_litAiBtn->setEnabled(false);
    m_litSearchBtn->setEnabled(false);
    const QString src = m_litSource->currentData().toString();
    m_litStatus->setText(tr("\xf0\x9f\x94\x84  Ricerca in corso..."));

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
            ? tr("\xe2\x8f\xb1  Timeout (15s) \xe2\x80\x94 premi Cerca per ritentare")
            : tr("\xe2\x9d\x8c  Errore: ") + reply->errorString()
              + tr(" \xe2\x80\x94 premi Cerca per ritentare"));
        if (!isTimeout)
            LogBus::post("\xe2\x9d\x8c Ricerca: Errore rete: " + reply->errorString());
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
    m_litStatus->setText(tr("\xf0\x9f\xa4\x96  Analisi AI..."));
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
    m_litStatus->setText(tr("\xe2\x9c\x85  Analisi completata"));
}

void RicercaPage::onLitAiError(const QString& e)
{
    disconnect(m_litAiTokenConn);    m_litAiTokenConn    = {};
    disconnect(m_litAiFinishedConn); m_litAiFinishedConn = {};
    disconnect(m_litAiErrorConn);    m_litAiErrorConn    = {};
    m_litAiBtn->setEnabled(true);
    m_litStatus->setText(tr("\xe2\x9d\x8c  ") + e);
    LogBus::post("\xe2\x9d\x8c Ricerca: Errore AI letteratura: " + e);
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
        tr("<b>\xf0\x9f\x8c\x8c  Analisi Fenomeni</b>"
        "  <span style='color:gray;font-size:11px;'>"
        "L'AI valuta se un evento \xc3\xa8 realmente accaduto sulla base delle fonti."
        "</span>"));
    introLbl->setTextFormat(Qt::RichText);
    root->addWidget(introLbl);

    /* ── Categoria ── */
    auto* catBox  = new QGroupBox(tr("Categoria evento"));
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

    auto* evtGroup = new QGroupBox(tr("Descrizione dell\xe2\x80\x99" "evento"));
    auto* evtLay   = new QVBoxLayout(evtGroup);
    m_analisiEventEdit = new QTextEdit;
    m_analisiEventEdit->setPlaceholderText(
        tr("Descrivi l\xe2\x80\x99" "evento nel dettaglio:\n"
        "- cosa \xc3\xa8 stato osservato / riportato\n"
        "- quando, dove, da chi\n"
        "- eventuali effetti fisici o testimonianze"));
    m_analisiEventEdit->setMinimumHeight(120);
    evtLay->addWidget(m_analisiEventEdit);

    auto* srcGroup = new QGroupBox(tr("Fonti (URL, citazioni, testo grezzo)"));
    auto* srcLay   = new QVBoxLayout(srcGroup);
    m_analisiSrcEdit = new QTextEdit;
    m_analisiSrcEdit->setPlaceholderText(
        tr("Incolla qui le tue fonti:\n"
        "- link ad articoli scientifici o giornali\n"
        "- estratti di testo, PDF, rapporti ufficiali\n"
        "- citazioni di testimoni o esperti\n"
        "(se non hai fonti, scrivi: nessuna fonte disponibile)"));
    m_analisiSrcEdit->setMinimumHeight(120);
    srcLay->addWidget(m_analisiSrcEdit);

    midSplit->addWidget(evtGroup);
    midSplit->addWidget(srcGroup);
    midSplit->setStretchFactor(0, 1);
    midSplit->setStretchFactor(1, 1);
    root->addWidget(midSplit);

    /* ── Allegati (file) ── */
    auto* fileBox = new QGroupBox(tr("\xf0\x9f\x93\x82  File allegati (PDF, TXT, MD, CSV, JSON)"));
    auto* fileLay = new QVBoxLayout(fileBox);
    auto* fileBtnRow = new QWidget;
    auto* fileBtnLay = new QHBoxLayout(fileBtnRow);
    fileBtnLay->setContentsMargins(0,0,0,0); fileBtnLay->setSpacing(8);
    auto* addFileBtn  = new QPushButton(tr("\xe2\x9e\x95  Aggiungi file\xe2\x80\xa6"));
    addFileBtn->setObjectName("actionBtn");
    auto* remFileBtn  = new QPushButton(tr("\xe2\x9c\x96  Rimuovi selezionato"));
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
    m_analisiFileList->setFixedHeight(dpiScale(72));
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

    auto* modelLbl = new QLabel(tr("Modello:"));
    m_analisiModelCombo = new ModelComboBox(m_ai, this);

    m_analisiRunBtn  = new QPushButton(tr("\xf0\x9f\x94\x8d  Analizza AI"));
    m_analisiStopBtn = new QPushButton(tr("\xe2\x96\xa0  Stop"));
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
    m_analisiProbLbl = new QLabel(tr("\xf0\x9f\x93\x8a  Probabilit\xc3\xa0:"));
    m_analisiProbBar = new QProgressBar;
    m_analisiProbBar->setRange(0, 100);
    m_analisiProbBar->setValue(0);
    m_analisiProbBar->setTextVisible(true);
    m_analisiProbBar->setFormat("%p%");
    m_analisiProbBar->setFixedHeight(dpiScale(22));
    probLay->addWidget(m_analisiProbLbl);
    probLay->addWidget(m_analisiProbBar, 1);
    probRow->setVisible(false);
    root->addWidget(probRow);

    /* ── Output ── */
    m_analisiOutput = new QTextEdit;
    m_analisiOutput->setReadOnly(false);
    m_analisiOutput->setFont(QFont("Monospace", 10));
    m_analisiOutput->setPlaceholderText(
        tr("Il risultato dell\xe2\x80\x99" "analisi AI apparir\xc3\xa0 qui.\n\n"
        "La risposta include:\n"
        "  \xf0\x9f\x93\x8a  Probabilit\xc3\xa0 (0-100%)\n"
        "  \xe2\x9c\x85  Evidenze a supporto\n"
        "  \xe2\x9d\x8c  Elementi contraddittori\n"
        "  \xf0\x9f\x94\xac  Spiegazione scientifica pi\xc3\xb9 probabile\n"
        "  \xf0\x9f\x94\x80  Ipotesi alternativa\n"
        "  \xe2\x9a\x96  Verdetto motivato"));
    root->addWidget(makeOutputBar(m_analisiOutput, "Analisi Fenomeni", page, this));
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
                content = ProcHelper::run("pdftotext", {path, "-"}, 10000).out;
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
    LogBus::post("\xe2\x9d\x8c Ricerca: Errore analisi AI: " + msg);
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
    m_ragGm->pruneByImportance(800);   /* tieni top-800 nodi al lancio */
    connect(m_ragGm, &GraphMemory::backupDone,
            this, &RicercaPage::onRagGmBackupDone);
    m_ragGm->enableAutoBackup(6, 800, 5);
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
    connect(m_ragGraph, &RagGraph::fileCopied,
            this, &RicercaPage::onRagGraphFileCopied);
    connect(m_ragGm, &GraphMemory::changed, this, &RicercaPage::onRagGraphMemChanged);

    /* QFileSystemWatcher — auto-trigger quando vengono aggiunti nuovi file RAG.
     * Debounce di 2s per evitare trigger multipli durante copie batch di file. */
    m_ragDirWatcher   = new QFileSystemWatcher(this);
    m_ragAutoDebounce = new QTimer(this);
    m_ragAutoDebounce->setSingleShot(true);
    m_ragAutoDebounce->setInterval(2000);

    const QString ragDocs  = QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
                             + "/prismalux_rag_docs";
    const QString ragLocal = P::ragDir();
    if (QDir(ragDocs).exists())  m_ragDirWatcher->addPath(ragDocs);
    if (QDir(ragLocal).exists()) m_ragDirWatcher->addPath(ragLocal);

    connect(m_ragDirWatcher, &QFileSystemWatcher::directoryChanged,
            m_ragAutoDebounce, qOverload<>(&QTimer::start));
    connect(m_ragAutoDebounce, &QTimer::timeout, this, [this]() {
        if (!m_ragGraph || m_ragGraph->isRunning()) return;
        onRagRunClicked();
    });

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
        tr("\xf0\x9f\x95\xb8  <b>Grafo della Conoscenza RAG</b>"
        " — entit\xc3\xa0 e relazioni estratte dai tuoi documenti"),  /* entità */
        topBar);
    titleLbl->setTextFormat(Qt::RichText);
    titleLbl->setObjectName("cardDesc");
    topLay->addWidget(titleLbl, 1);

    m_ragModelCombo = new ModelComboBox(m_ai, topBar);
    m_ragModelCombo->setToolTip(tr("Modello LLM per estrazione entit\xc3\xa0"));
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

    /* Sorgente del grafo visualizzato (D-44) — la logica usa SEMPRE
     * currentData(), mai il testo: le etichette restano traducibili. */
    m_ragSourceCombo = new QComboBox(ctrlBar);
    m_ragSourceCombo->setObjectName("settingCombo");
    m_ragSourceCombo->addItem(tr("\xf0\x9f\x95\xb8  RAG (documenti)"),        "rag");    /* 🕸 */
    m_ragSourceCombo->addItem(tr("\xf0\x9f\xa7\xa0  Hermes (conversazioni)"), "hermes"); /* 🧠 */
    m_ragSourceCombo->addItem(tr("\xf0\x9f\xa4\x96  Multi-Agente"),           "multi");  /* 🤖 */
    m_ragSourceCombo->setToolTip(
        tr("Sorgente del grafo visualizzato: documenti indicizzati (RAG), "
           "memoria conversazioni Hermes (tab AI) o memoria del Multi-Agente"));
    connect(m_ragSourceCombo, &QComboBox::currentIndexChanged,
            this, &RicercaPage::onRagSourceChanged);
    ctrlLay->addWidget(m_ragSourceCombo);

    m_ragRunBtn = new QPushButton(
        tr("\xf0\x9f\x94\x84  Analizza RAG"), w);  /* 🔄 */
    m_ragRunBtn->setObjectName("actionBtn");
    m_ragRunBtn->setProperty("highlight", "true");
    m_ragRunBtn->setToolTip(
        tr("Scansiona ~/prismalux_rag_docs/ e Prismalux/RAG/,"
        " estrae entit\xc3\xa0 e relazioni con LLM"));
    connect(m_ragRunBtn, &QPushButton::clicked, this, &RicercaPage::onRagRunClicked);
    ctrlLay->addWidget(m_ragRunBtn);

    m_ragPauseBtn = new QPushButton(tr("\xe2\x8f\xb8  Pausa"), w);  /* ⏸ */
    m_ragPauseBtn->setObjectName("navBtn");
    m_ragPauseBtn->setEnabled(false);
    m_ragPauseBtn->setToolTip(tr("Sospendi l'indicizzazione RAG tra un file e l'altro; riprendi senza ripartire da zero"));
    connect(m_ragPauseBtn, &QPushButton::clicked, this, &RicercaPage::onRagPauseClicked);
    ctrlLay->addWidget(m_ragPauseBtn);

    m_ragStopBtn = new QPushButton(tr("\xe2\x96\xa0  Stop"), w);  /* ■ */
    m_ragStopBtn->setObjectName("stopBtn");
    m_ragStopBtn->setEnabled(false);
    connect(m_ragStopBtn, &QPushButton::clicked, this, &RicercaPage::onRagStopClicked);
    ctrlLay->addWidget(m_ragStopBtn);

    m_ragClearBtn = new QPushButton(tr("\xf0\x9f\x97\x91  Svuota"), w);  /* 🗑 */
    m_ragClearBtn->setObjectName("navBtn");
    connect(m_ragClearBtn, &QPushButton::clicked, this, &RicercaPage::onRagClearClicked);
    ctrlLay->addWidget(m_ragClearBtn);

    auto* btnRefreshDot = new QPushButton(tr("\xf0\x9f\x8c\xbf  Rigenera Grafo"), w);
    btnRefreshDot->setObjectName("navBtn");
    btnRefreshDot->setToolTip(tr("Rigenera la visualizzazione Graphviz dal grafo corrente"));
    connect(btnRefreshDot, &QPushButton::clicked, this, &RicercaPage::onRagRefreshDot);
    ctrlLay->addWidget(btnRefreshDot);

    /* D-51: import di chat esportate da AI esterne nel grafo RAG */
    auto* btnImportAi = new QPushButton(tr("\xf0\x9f\x93\xa5  Importa chat AI"), w);
    btnImportAi->setObjectName("navBtn");
    btnImportAi->setToolTip(
        tr("Carica conversazioni esportate da AI esterne nel grafo RAG.\n"
           "JSON riconosciuti: ChatGPT (conversations.json), Claude"
           " (chat_messages), formato generico role/content.\n"
           "Gemini, Grok e altri senza standard: qualsiasi file di testo"
           " viene indicizzato cos\xc3\xac com'\xc3\xa8."));
    connect(btnImportAi, &QPushButton::clicked,
            this, &RicercaPage::onRagImportAiClicked);
    ctrlLay->addWidget(btnImportAi);

    ctrlLay->addStretch(1);

    m_ragStatus = new QLabel(tr("\xf0\x9f\x95\xb8  Pronto."), ctrlBar);
    m_ragStatus->setObjectName("statusLabel");
    ctrlLay->addWidget(m_ragStatus, 2);

    m_ragTempLbl = new QLabel("", ctrlBar);
    m_ragTempLbl->setObjectName("ragTempLabel");
    m_ragTempLbl->setStyleSheet(
        "QLabel#ragTempLabel{color:#94a3b8;font-size:11px;padding:0 6px;}");
    m_ragTempLbl->setToolTip(
        tr("Temperatura rilevata — se >80°C l'indicizzazione RAG viene rallentata automaticamente."));
    m_ragTempLbl->setVisible(false);
    ctrlLay->addWidget(m_ragTempLbl);

    m_ragProgress = new QProgressBar(ctrlBar);
    m_ragProgress->setRange(0, 0);
    m_ragProgress->setVisible(false);
    m_ragProgress->setFixedHeight(dpiScale(8));
    m_ragProgress->setFixedWidth(dpiScale(120));
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
    m_ragSearchEdit->setPlaceholderText(tr("Cerca nodo..."));
    m_ragSearchEdit->setObjectName("settingCombo");
    connect(m_ragSearchEdit, &QLineEdit::textChanged,
            this, &RicercaPage::onRagSearchChanged);
    searchRow->addWidget(m_ragSearchEdit, 1);
    leftLay->addLayout(searchRow);

    auto* nodeLbl = new QLabel(tr("<b>Nodi del grafo</b>"), leftPanel);
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
    m_ragNodeDetail->setPlaceholderText(tr("Clicca un nodo per i dettagli..."));
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
        tr("<p style='color:#64748b'>Il grafo apparir\xc3\xa0 qui dopo l'analisi.<br>"
        "Richiede <b>graphviz</b> installato: <code>sudo apt install graphviz</code></p>"));
    m_ragImgLbl->setTextFormat(Qt::RichText);
    imgScroll->setWidget(m_ragImgLbl);
    vizTabs->addTab(imgScroll, tr("\xf0\x9f\x96\xbc  Grafo"));  /* 🖼 */

    /* Tab 2: DOT sorgente */
    m_ragDotView = new QTextEdit(vizTabs);
    m_ragDotView->setObjectName("chatLog");
    m_ragDotView->setReadOnly(true);
    m_ragDotView->setLineWrapMode(QTextEdit::NoWrap);
    m_ragDotView->setPlaceholderText(tr("Graphviz DOT del grafo..."));
    vizTabs->addTab(m_ragDotView, tr("\xf0\x9f\x93\x9d  DOT"));  /* 📝 */

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
    const QString ragLocal = P::ragDir();

    if (!QDir(ragDocs).exists() && !QDir(ragLocal).exists()) {
        if (m_ragStatus)
            m_ragStatus->setText(tr("\xe2\x9a\xa0\xef\xb8\x8f  Nessuna cartella RAG trovata. "
                                 "Aggiungi documenti in ~/prismalux_rag_docs/"));
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
                    m_ragStatus->setText(tr("\xe2\x9a\xa0\xef\xb8\x8f  ") + f + ": " + e.left(60));
            });

    if (QDir(ragDocs).exists()) m_ragGraph->addDirectory(ragDocs);
    if (QDir(ragLocal).exists()) m_ragGraph->addDirectory(ragLocal);

    /* Aggiorna watcher con eventuali nuove dir create dopo l'avvio */
    if (m_ragDirWatcher) {
        if (QDir(ragDocs).exists()  && !m_ragDirWatcher->directories().contains(ragDocs))
            m_ragDirWatcher->addPath(ragDocs);
        if (QDir(ragLocal).exists() && !m_ragDirWatcher->directories().contains(ragLocal))
            m_ragDirWatcher->addPath(ragLocal);
    }

    if (m_ragGraph->stats().totalFiles == 0) {
        m_ragStatus->setText(tr("\xe2\x9a\xa0\xef\xb8\x8f  Nessun file trovato nelle cartelle RAG."));
        return;
    }

    m_ragRunBtn->setEnabled(false);
    if (m_ragPauseBtn) m_ragPauseBtn->setEnabled(true);
    m_ragStopBtn->setEnabled(true);
    if (m_ragProgress) m_ragProgress->setVisible(true);
    m_ragStatus->setText(QString("\xf0\x9f\x94\x84  Analisi in corso (%1 file)...")
                         .arg(m_ragGraph->stats().totalFiles));

    m_ragGraph->startIngest();
}

void RicercaPage::onRagPauseClicked()
{
    if (!m_ragGraph) return;
    if (m_ragGraph->isPaused()) {
        m_ragGraph->resumeIngest();
        if (m_ragPauseBtn) m_ragPauseBtn->setText(tr("\xe2\x8f\xb8  Pausa"));  /* ⏸ */
        if (m_ragStatus) m_ragStatus->setText(tr("\xf0\x9f\x94\x84  Analisi ripresa..."));
    } else {
        m_ragGraph->pauseIngest();
        if (m_ragPauseBtn) m_ragPauseBtn->setText(tr("\xe2\x8f\xb5  Riprendi"));  /* ⏵ */
        if (m_ragStatus) m_ragStatus->setText(tr("\xe2\x8f\xb8  Analisi in pausa dopo il file corrente."));
    }
}

void RicercaPage::onRagStopClicked()
{
    if (m_ragGraph) m_ragGraph->stopIngest();
    if (m_ragRunBtn)   m_ragRunBtn->setEnabled(true);
    if (m_ragPauseBtn) { m_ragPauseBtn->setEnabled(false); m_ragPauseBtn->setText(tr("\xe2\x8f\xb8  Pausa")); }
    if (m_ragStopBtn)  m_ragStopBtn->setEnabled(false);
    if (m_ragProgress) m_ragProgress->setVisible(false);
    if (m_ragStatus)   m_ragStatus->setText(tr("\xe2\x96\xa0  Analisi interrotta."));
}

void RicercaPage::onRagClearClicked()
{
    /* D-44: svuota la SORGENTE selezionata, non sempre il grafo RAG —
     * il nome nella conferma dice esattamente quale DB viene cancellato. */
    GraphMemory* gm = ragViewGm();
    const QString srcName = m_ragSourceCombo
        ? m_ragSourceCombo->currentText().trimmed() : tr("Grafo RAG");
    const auto ans = QMessageBox::question(
        this, tr("Svuota grafo"),
        tr("Tutti i nodi e le relazioni di \"%1\" verranno cancellati in modo irreversibile.\n"
           "Continuare?").arg(srcName),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (ans != QMessageBox::Yes) return;

    const QString src = m_ragSourceCombo
        ? m_ragSourceCombo->currentData().toString() : QString();
    if (src != "hermes" && src != "multi")
        onRagStopClicked();   /* ferma l'indicizzazione solo per il RAG */
    if (gm) gm->clearAll();
    if (m_ragNodeList) m_ragNodeList->clear();
    if (m_ragNodeDetail) m_ragNodeDetail->clear();
    if (m_ragImgLbl) m_ragImgLbl->clear();
    if (m_ragDotView) m_ragDotView->clear();
    if (m_ragStatus) m_ragStatus->setText(tr("\xf0\x9f\x97\x91  Grafo svuotato."));
}

/* ══════════════════════════════════════════════════════════════
   Slot: temperatura CPU/GPU — aggiornamento label + throttle RAG
   ══════════════════════════════════════════════════════════════ */
void RicercaPage::onThermalUpdate(double cpuTempC, double gpuTempC)
{
    const double maxTemp = qMax(cpuTempC, gpuTempC);
    if (maxTemp <= 0) return;

    /* Aggiorna label temperatura in-tab */
    if (m_ragTempLbl) {
        QStringList parts;
        if (cpuTempC > 0) parts << QString("CPU %1\xc2\xb0" "C").arg((int)cpuTempC);
        if (gpuTempC > 0) parts << QString("GPU %1\xc2\xb0" "C").arg((int)gpuTempC);
        m_ragTempLbl->setText(tr("\xf0\x9f\x8c\xa1  ") + parts.join(" | "));
        const QString col = maxTemp >= 90 ? "#f87171" : maxTemp >= 75 ? "#f59e0b" : "#94a3b8";
        m_ragTempLbl->setStyleSheet(
            QString("QLabel#ragTempLabel{color:%1;font-size:11px;padding:0 6px;}").arg(col));
        m_ragTempLbl->setVisible(true);
    }

    /* Thermal throttle: sopra 80°C sospendi il RAG tra un file e l'altro */
    if (!m_ragGraph || !m_ragGraph->isRunning()) return;

    if (maxTemp >= 80.0 && !m_ragGraph->isPaused()) {
        m_ragGraph->pauseIngest();
        if (m_ragPauseBtn) m_ragPauseBtn->setText(tr("\xe2\x8f\xb5  Riprendi"));
        if (m_ragStatus)
            m_ragStatus->setText(
                tr("\xf0\x9f\x8c\xa1  Temperatura elevata (%1\xc2\xb0" "C) — "
                   "indicizzazione sospesa automaticamente.")
                .arg((int)maxTemp));
    } else if (maxTemp < 75.0 && m_ragGraph->isPaused()) {
        /* Auto-riprende solo se la pausa era termica (non manuale) */
        m_ragGraph->resumeIngest();
        if (m_ragPauseBtn) m_ragPauseBtn->setText(tr("\xe2\x8f\xb8  Pausa"));
        if (m_ragStatus) m_ragStatus->setText(tr("\xf0\x9f\x94\x84  Temperatura ok — analisi ripresa."));
    }
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
    if (m_ragPauseBtn) { m_ragPauseBtn->setEnabled(false); m_ragPauseBtn->setText(tr("\xe2\x8f\xb8  Pausa")); }
    if (m_ragStopBtn)  m_ragStopBtn->setEnabled(false);
    if (m_ragProgress) m_ragProgress->setVisible(false);

    if (m_ragStatus)
        m_ragStatus->setText(
            tr("\xe2\x9c\x85  Completato: %1 file, %2 entit\xc3\xa0, %3 relazioni.")
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
    GraphMemory* gm = ragViewGm();
    if (!gm || !m_ragNodeList) return;
    const QString filter = m_ragSearchEdit ? m_ragSearchEdit->text() : QString();
    const auto nodes = filter.isEmpty()
        ? gm->allNodes()
        : gm->searchNodes(filter, 100);

    m_ragNodeList->clear();
    if (nodes.isEmpty()) {
        const QString src = m_ragSourceCombo
            ? m_ragSourceCombo->currentData().toString() : QString();
        QString emptyMsg;
        if (!filter.isEmpty())
            emptyMsg = tr("Nessun nodo trovato per: \"%1\"").arg(filter);
        else if (src == "hermes")
            emptyMsg = tr("Memoria Hermes vuota — attiva \xf0\x9f\xa7\xa0 Hermes"
                          " nella tab AI e conversa");
        else if (src == "multi")
            emptyMsg = tr("Memoria Multi-Agente vuota — esegui un piano"
                          " nella tab Multi-Agente");
        else
            emptyMsg = tr("Grafo vuoto — indicizza documenti con \"Analizza Grafo\"");
        auto* item = new QListWidgetItem(emptyMsg, m_ragNodeList);
        item->setFlags(Qt::NoItemFlags);
        return;
    }
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
        else if (t == "conversation")
            icon = "\xf0\x9f\x92\xac ";   /* 💬 (Hermes) */
        else if (t == "reflection")
            icon = "\xf0\x9f\xa7\xa0 ";   /* 🧠 (Hermes) */
        else if (t == "task" || t == "result")
            icon = "\xf0\x9f\x93\x8b ";   /* 📋 (Multi-Agente) */
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
   D-44 — sorgente del grafo visualizzato
   ══════════════════════════════════════════════════════════════ */
GraphMemory* RicercaPage::ragViewGm() const
{
    const QString src = m_ragSourceCombo
        ? m_ragSourceCombo->currentData().toString() : QString();
    if (src == "hermes" && m_hermesViewGm) return m_hermesViewGm;
    if (src == "multi"  && m_multiViewGm)  return m_multiViewGm;
    return m_ragGm;
}

/* ══════════════════════════════════════════════════════════════
   D-51 — importa chat esportate da AI esterne nel grafo RAG.
   Conversione in ~/prismalux_rag_docs/chat_ai_importate/ (dentro le
   cartelle già scandite da "Analizza RAG" e dal watcher), poi ingest.
   ══════════════════════════════════════════════════════════════ */
void RicercaPage::onRagImportAiClicked()
{
    const QStringList files = QFileDialog::getOpenFileNames(
        this, tr("Importa conversazioni AI esterne"), QDir::homePath(),
        tr("Export AI e testo (*.json *.txt *.md);;Tutti i file (*)"));
    if (files.isEmpty()) return;

    const QString destDir = QDir::homePath()
        + "/prismalux_rag_docs/chat_ai_importate";
    int nConv = 0, nText = 0, nErr = 0;
    const int written = ExternalAiImport::importFilesToDir(
        files, destDir, &nConv, &nText, &nErr);

    if (m_ragStatus) {
        if (written == 0) {
            m_ragStatus->setText(
                tr("\xe2\x9a\xa0\xef\xb8\x8f  Nessun file importabile "
                   "(%1 scartati: vuoti o binari).").arg(nErr));
            return;
        }
        m_ragStatus->setText(
            tr("\xf0\x9f\x93\xa5  Importati %1 file (%2 conversazioni, %3 testi"
               "%4) — avvio indicizzazione...")
                .arg(written).arg(nConv).arg(nText)
                .arg(nErr > 0 ? tr(", %1 scartati").arg(nErr) : QString()));
    }
    if (written == 0) return;

    /* Il risultato riguarda il grafo RAG: riporta la vista su quella
       sorgente se l'utente stava guardando Hermes/Multi-Agente. */
    if (m_ragSourceCombo && m_ragSourceCombo->currentIndex() != 0)
        m_ragSourceCombo->setCurrentIndex(0);

    if (m_ragGraph && !m_ragGraph->isRunning())
        onRagRunClicked();
}

void RicercaPage::onRagSourceChanged(int /*idx*/)
{
    const QString src = m_ragSourceCombo
        ? m_ragSourceCombo->currentData().toString() : QString();

    /* Apertura lazy della vista sull'altro DB: connessione SQLite propria
     * (m_connName è per-istanza), il GraphMemory proprietario resta in
     * AgentiPage/AgentiMultiPage. Nessun prune qui: la manutenzione la fa
     * solo il proprietario. */
    if (src == "hermes" && !m_hermesViewGm) {
        m_hermesViewGm = new GraphMemory(
            QDir::homePath() + "/.prismalux/hermes_memory.db", this);
        if (!m_hermesViewGm->open()) {
            delete m_hermesViewGm;
            m_hermesViewGm = nullptr;
        } else {
            connect(m_hermesViewGm, &GraphMemory::changed,
                    this, &RicercaPage::onRagGraphMemChanged);
        }
    }
    if (src == "multi" && !m_multiViewGm) {
        m_multiViewGm = new GraphMemory(
            QDir::homePath() + "/.prismalux/graph_memory.db", this);
        if (!m_multiViewGm->open()) {
            delete m_multiViewGm;
            m_multiViewGm = nullptr;
        } else {
            connect(m_multiViewGm, &GraphMemory::changed,
                    this, &RicercaPage::onRagGraphMemChanged);
        }
    }

    /* "Analizza RAG" resta sempre attivo: agisce esplicitamente e solo
     * sul DB RAG, qualunque sia la sorgente visualizzata. */
    if (m_ragNodeDetail) m_ragNodeDetail->clear();
    if (m_ragStatus && m_ragSourceCombo)
        m_ragStatus->setText(tr("\xf0\x9f\x97\x84  Sorgente: %1")   /* 🗄 */
                             .arg(m_ragSourceCombo->currentText().trimmed()));
    onRagGraphMemChanged();
    onRagRefreshDot();
}

/* ══════════════════════════════════════════════════════════════
   Slot: click nodo → dettaglio + vicini
   ══════════════════════════════════════════════════════════════ */
void RicercaPage::onRagNodeClicked(QListWidgetItem* item)
{
    GraphMemory* gm = ragViewGm();
    if (!item || !gm || !m_ragNodeDetail) return;
    const QString nodeId = item->data(Qt::UserRole).toString();
    const auto node = gm->nodeById(nodeId);
    if (!node.has_value()) return;

    QString detail;
    detail += "<b>" + node->label.toHtmlEscaped() + "</b>";
    detail += " <span style='color:#64748b'>[" + node->type + "]</span>";
    detail += "<br>Importanza: " + QString::number(node->importance, 'f', 2);
    if (!node->content.isEmpty())
        detail += "<br><br>" + node->content.left(300).toHtmlEscaped();

    /* Vicini */
    const auto nbrs = gm->neighbours(nodeId, 1);
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
    GraphMemory* gm = ragViewGm();
    if (!gm) return;

    const QString src = m_ragSourceCombo
        ? m_ragSourceCombo->currentData().toString() : QString();
    const QString dotTitle = src == "hermes" ? "Memoria Hermes"
                           : src == "multi"  ? "Memoria Multi-Agente"
                                             : "Grafo RAG Prismalux";
    const QString dot = gm->toDot(dotTitle, 80);
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
        LogBus::post("\xe2\x9d\x8c RAG Graph: Graphviz non trovato nel PATH.");
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

/* ══════════════════════════════════════════════════════════════
   onAutoRagTrigger — scatta dopo indexingFinished in ImpostazioniPage.
   Avvia il RagGraph con un breve ritardo per lasciare che il FS
   finisca di scrivere i file di indice RAG.
   ══════════════════════════════════════════════════════════════ */
void RicercaPage::onAutoRagTrigger(int nChunks, bool aborted)
{
    if (aborted || nChunks == 0) return;            /* nessun documento nuovo */
    if (m_ragGraph && m_ragGraph->isRunning()) return;  /* già in corso */

    /* Ritardo di 800 ms per lasciare che il FS completi la scrittura */
    QTimer::singleShot(800, this, &RicercaPage::onRagRunClicked);
}

void RicercaPage::onRagGraphFileCopied(const QString& filename, const QString& dest)
{
    Q_UNUSED(dest)
    if (m_ragStatus)
        m_ragStatus->setText(
            QString("\xf0\x9f\x93\x84  Copiato in RAG/ \xe2\x80\x94 %1 ora persistente")
            .arg(filename));
}

void RicercaPage::onRagGmBackupDone(const QString& path, bool ok)
{
    if (ok)
        LogBus::post("\xf0\x9f\x92\xbe RagGraph: backup creato \xe2\x86\x92 " + path);
    else
        LogBus::post("\xe2\x9d\x8c RagGraph: backup fallito");
}

/* ══════════════════════════════════════════════════════════════
   SDS Editing — pipeline di STUDIO (non clinica)
   Lancia Tools/sds_editing/run_all.py e ne mostra l'output in tempo reale.
   ══════════════════════════════════════════════════════════════ */
QWidget* RicercaPage::buildSdsEditingTab()
{
    auto* page = new QWidget;
    auto* root = new QVBoxLayout(page);
    root->setContentsMargins(dpiScale(10), dpiScale(10), dpiScale(10), dpiScale(10));

    auto* header = new QLabel(
        "  \xf0\x9f\xa7\xac  <b>SDS Editing</b> "
        "<span style='color:gray;font-size:11px;'>"
        "Sindrome di Shwachman-Diamond \xe2\x80\x94 pipeline didattica</span>");
    header->setTextFormat(Qt::RichText);
    root->addWidget(header);

    auto* warn = new QLabel(
        "\xe2\x9a\xa0\xef\xb8\x8f  <b>Strumento di studio, non clinico.</b> "
        "Gli score sono euristici/segnaposto: l'output NON va usato come ipotesi "
        "terapeutica. Per un uso reale servono strumenti validati (SpliceAI, PEGG, "
        "GuideScan2) e un centro di ricerca (in Italia: Centro Fibrosi Cistica di "
        "Verona / AISS).");
    warn->setTextFormat(Qt::RichText);
    warn->setWordWrap(true);
    warn->setStyleSheet(
        "QLabel{background:#3a2a00;color:#fbbf24;border:1px solid #a16207;"
        "border-radius:6px;padding:8px;}");
    root->addWidget(warn);

    auto* ctrl = new QHBoxLayout;
    m_sdsRunBtn = new QPushButton(tr("\xe2\x96\xb6  Esegui run_all.py"));
    m_sdsStopBtn = new QPushButton(tr("\xe2\x8f\xb9  Stop"));
    m_sdsStopBtn->setEnabled(false);
    m_sdsStatus = new QLabel(QString());
    m_sdsStatus->setStyleSheet("color:gray;");
    ctrl->addWidget(m_sdsRunBtn);
    ctrl->addWidget(m_sdsStopBtn);
    ctrl->addWidget(m_sdsStatus, 1);
    root->addLayout(ctrl);

    connect(m_sdsRunBtn,  &QPushButton::clicked, this, &RicercaPage::onSdsRunClicked);
    connect(m_sdsStopBtn, &QPushButton::clicked, this, &RicercaPage::onSdsStopClicked);

    m_sdsOut = new QTextEdit;
    m_sdsOut->setReadOnly(true);
    m_sdsOut->setLineWrapMode(QTextEdit::NoWrap);
    m_sdsOut->setStyleSheet(
        "QTextEdit{font-family:monospace;background:#0b0f19;color:#c8d3e6;}");
    root->addWidget(m_sdsOut, 1);

    return page;
}

void RicercaPage::onSdsRunClicked()
{
    if (m_sdsProc && m_sdsProc->state() != QProcess::NotRunning) return;

    const QString dir = P::root() + "/Tools/sds_editing";
    const QString script = dir + "/run_all.py";
    if (!QFile::exists(script)) {
        m_sdsOut->setPlainText(
            QString("\xe2\x9d\x8c  Script non trovato:\n%1").arg(script));
        return;
    }

    m_sdsOut->clear();
    m_sdsStatus->setText(tr("\xe2\x8f\xb3  In esecuzione\xe2\x80\xa6"));
    m_sdsRunBtn->setEnabled(false);
    m_sdsStopBtn->setEnabled(true);

    m_sdsProc = new QProcess(this);
    m_sdsProc->setWorkingDirectory(dir);
    m_sdsProc->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_sdsProc, &QProcess::readyReadStandardOutput,
            this, &RicercaPage::onSdsReadyRead);
    connect(m_sdsProc,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &RicercaPage::onSdsProcFinished);

    m_sdsProc->start(P::findPython(), {script});
    if (!m_sdsProc->waitForStarted(3000)) {
        m_sdsOut->setPlainText(
            "\xe2\x9d\x8c  Impossibile avviare Python. "
            "Verifica che Python e le dipendenze (requirements.txt) siano installati.");
        m_sdsProc->deleteLater();
        m_sdsProc = nullptr;
        m_sdsStatus->setText(QString());
        m_sdsRunBtn->setEnabled(true);
        m_sdsStopBtn->setEnabled(false);
    }
}

void RicercaPage::onSdsStopClicked()
{
    if (m_sdsProc && m_sdsProc->state() != QProcess::NotRunning) {
        m_sdsProc->kill();
        m_sdsStatus->setText(tr("\xe2\x8f\xb9  Interrotto"));
    }
}

void RicercaPage::onSdsReadyRead()
{
    if (!m_sdsProc || !m_sdsOut) return;
    const QString chunk = QString::fromLocal8Bit(m_sdsProc->readAllStandardOutput());
    m_sdsOut->moveCursor(QTextCursor::End);
    m_sdsOut->insertPlainText(chunk);
    m_sdsOut->moveCursor(QTextCursor::End);
}

void RicercaPage::onSdsProcFinished(int code, QProcess::ExitStatus /*status*/)
{
    m_sdsStatus->setText(code == 0
        ? tr("\xe2\x9c\x85  Completato")
        : tr("\xe2\x9d\x8c  Uscito con codice %1").arg(code));
    m_sdsRunBtn->setEnabled(true);
    m_sdsStopBtn->setEnabled(false);
    if (m_sdsProc) { m_sdsProc->deleteLater(); m_sdsProc = nullptr; }
}
