#include "ricerca_page.h"
#include "lavoro_page.h"
#include "../prismalux_paths.h"
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

    QObject::connect(btnPdf, &QPushButton::clicked, parent, [editor, titolo, parent]{
        RicercaPage::esportaPdf(editor, titolo, parent);
    });
    QObject::connect(btnMd, &QPushButton::clicked, parent, [editor, titolo, parent]{
        RicercaPage::salvaMarkdown(editor, titolo, parent);
    });
    QObject::connect(btnClr, &QPushButton::clicked, parent, [editor]{ editor->clear(); });
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
    tabs->addTab(new LavoroPage(m_ai, this),  "\xf0\x9f\x92\xbc  Cerca Lavoro");
    /* ── Gruppo 3: Scienze ── */
    tabs->addTab(buildCytoscapeTab(),         "\xf0\x9f\x94\xac  Cytoscape");
    tabs->addTab(buildRDKitTab(),             "\xf0\x9f\xa7\xaa  RDKit");
    tabs->addTab(buildBiocondaTab(),          "\xf0\x9f\x8c\xbf  Bioconda");
    tabs->addTab(buildAvogadroTab(),          "\xf0\x9f\xa7\xb4  Avogadro");

    /* Tooltip sui tab per scopribilità */
    tabs->setTabToolTip(0, "Genera paper accademico con AI");
    tabs->setTabToolTip(1, "Genera documento brevettuale PCT/EPO");
    tabs->setTabToolTip(2, "Genera specifiche tecniche e manuali");
    tabs->setTabToolTip(3, "Cerca su arXiv, Semantic Scholar, USPTO");
    tabs->setTabToolTip(4, "Ricerca offerte di lavoro");
    tabs->setTabToolTip(5, "Analisi reti biologiche e sociali");
    tabs->setTabToolTip(6, "Chemioinformatica con RDKit");
    tabs->setTabToolTip(7, "Pipeline bioinformatica con Bioconda");
    tabs->setTabToolTip(8, "Modellazione molecolare 3D");
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


    runBtn->setEnabled(false);
    if (m_sciProgress) m_sciProgress->setVisible(true);
    stopBtn->setEnabled(true);
    out->append(
        "\n\xf0\x9f\x94\x84  Generazione in corso...\n"
        + QString(40, QChar(0x2500)));

    delete m_sciTokenHolder;
    m_sciTokenHolder = new QObject(this);

    connect(m_ai, &AiClient::token, m_sciTokenHolder, [out](const QString& t) {
        out->moveCursor(QTextCursor::End);
        out->insertPlainText(t);
    });

    connect(m_ai, &AiClient::finished, m_sciTokenHolder,
            [this, out, runBtn, stopBtn, execBtn, codeRef, statusLbl](const QString& full) {

        runBtn->setEnabled(true);
        stopBtn->setEnabled(false);
        if (m_sciProgress) m_sciProgress->setVisible(false);
        out->append("\n" + QString(40, QChar(0x2500)));
        m_sciTokenHolder->deleteLater();
        m_sciTokenHolder = nullptr;

        if (execBtn && codeRef && full.contains("```")) {
            /* estrai primo blocco ``` */
            int start = full.indexOf("```python");
            if (start == -1) start = full.indexOf("```");
            if (start != -1) {
                start = full.indexOf('\n', start) + 1;
                int end = full.indexOf("```", start);
                if (end != -1) {
                    *codeRef = full.mid(start, end - start).trimmed();
                    if (!codeRef->isEmpty()) {
                        execBtn->setEnabled(true);
                        if (statusLbl)
                            statusLbl->setText("\xe2\x9c\x85  Codice pronto \xe2\x80\x94 premi Esegui");
                    }
                }
            }
        }
    });

    connect(m_ai, &AiClient::error, m_sciTokenHolder,
            [this, out, runBtn, stopBtn, sys, userMsg, modelCombo, execBtn, codeRef, statusLbl]
            (const QString& msg) {

        runBtn->setEnabled(true);
        stopBtn->setEnabled(false);
        if (m_sciProgress) m_sciProgress->setVisible(false);
        m_sciTokenHolder->deleteLater();
        m_sciTokenHolder = nullptr;
        m_sciErrorPanel->showError(msg, [this, sys, userMsg, out, runBtn, stopBtn,
                                         modelCombo, execBtn, codeRef, statusLbl]{
            avviaSci(sys, userMsg, out, runBtn, stopBtn, modelCombo, execBtn, codeRef, statusLbl);
        });
    });

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
    if (isBash)
        procRef->start("bash", {tmpPath});
    else
        procRef->start(P::findPython(), {tmpPath});
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
    for (auto* combo : {m_cytoModel, m_rdkitModel, m_bioModel, m_avoModel}) {
        if (!combo) continue;
        const QString cur = combo->currentData().toString();
        combo->clear();
        for (const QString& m : models)
            combo->addItem(m, m);
        const int idx = combo->findData(cur);
        combo->setCurrentIndex(idx >= 0 ? idx : 0);
    }
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
