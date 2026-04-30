#include "ricerca_page.h"
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
    tabs->addTab(buildPaperTab(),      "\xf0\x9f\x93\x84  Paper Scientifico");
    tabs->addTab(buildBrevettoTab(),   "\xf0\x9f\x94\x8f  Brevetto");
    tabs->addTab(buildDocTecnicoTab(), "\xf0\x9f\x93\x8b  Documento Tecnico");
    vlay->addWidget(tabs, 1);

    /* ── connessioni AI (una sola volta per tutta la pagina) ──────── */
    connect(m_ai, &AiClient::token, this, [this](const QString& t) {
        if (m_ai->currentReqId() != m_reqId || !m_outCurrent) return;
        QTextCursor c(m_outCurrent->document());
        c.movePosition(QTextCursor::End);
        c.insertText(t);
        m_outCurrent->ensureCursorVisible();
    });
    connect(m_ai, &AiClient::finished, this, [this](const QString&) {
        if (m_ai->currentReqId() != m_reqId) return;
        if (m_outCurrent)
            m_outCurrent->append(
                "\n\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
                "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
                "\xe2\x94\x80\xe2\x94\x80");
        resetButtons();
    });
    connect(m_ai, &AiClient::error, this, [this](const QString& err) {
        if (m_ai->currentReqId() != m_reqId) return;
        const QString el = err.toLower();
        if (!el.contains("cancel"))
            if (m_outCurrent)
                m_outCurrent->append(
                    "\n\xe2\x9d\x8c  Errore: " + err);
        resetButtons();
    });
    connect(m_ai, &AiClient::aborted, this, [this] {
        if (m_ai->currentReqId() != m_reqId) return;
        if (m_outCurrent)
            m_outCurrent->append(
                "\n\xe2\x8f\xb9  Interrotto.\n"
                "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
                "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
                "\xe2\x94\x80\xe2\x94\x80");
        resetButtons();
    });
}

void RicercaPage::resetButtons()
{
    if (m_btnGenAttivo)  m_btnGenAttivo->setEnabled(true);
    if (m_btnStopAttivo) m_btnStopAttivo->setEnabled(false);
    m_btnGenAttivo  = nullptr;
    m_btnStopAttivo = nullptr;
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
    m_reqId = m_ai->chat(sys, msg);
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
