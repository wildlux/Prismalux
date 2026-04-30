#include "lavoro_page.h"
#include "lavoro_data.h"
#include "../prismalux_paths.h"
namespace P = PrismaluxPaths;
#include <QBrush>
#include <QColor>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QPushButton>
#include <QGroupBox>
#include <QTextEdit>
#include <QLineEdit>
#include <QComboBox>
#include <QListWidget>
#include <QSplitter>
#include <QMenu>
#include <QGuiApplication>
#include <QClipboard>
#include <QProcess>
#include <QFileDialog>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QUrl>
#include <QDesktopServices>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTextDocument>
#include <QRegularExpression>
#include <QDialog>
#include <memory>

/* ══════════════════════════════════════════════════════════════
   caricaCV
   ══════════════════════════════════════════════════════════════ */
void LavoroPage::caricaCV(const QString& path) {
    if (!m_cvStatus) return;
    QFileInfo fi(path);
    if (!fi.exists()) {
        m_cvStatus->setText("\xe2\x9d\x8c File non trovato");
        m_cvStatus->setStyleSheet("color:#F44336;");
        return;
    }
    QProcess proc;
    proc.start("pdftotext", {path, "-"});
    if (proc.waitForFinished(8000) && proc.exitCode() == 0) {
        const QString txt = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        if (!txt.isEmpty()) {
            m_cvText = txt;
            m_cvStatus->setText(QString("\xe2\x9c\x85 CV caricato: %1 (%2 car.)")
                .arg(fi.fileName()).arg(m_cvText.size()));
            m_cvStatus->setStyleSheet("color:#4CAF50;");
            return;
        }
    }
    m_cvText.clear();
    m_cvStatus->setText(QString("\xf0\x9f\x93\x84 CV selezionato: %1 (profilo integrato)").arg(fi.fileName()));
    m_cvStatus->setStyleSheet("color:#E5C400;");
}

/* ══════════════════════════════════════════════════════════════
   applicaFiltri
   ══════════════════════════════════════════════════════════════ */
void LavoroPage::applicaFiltri() {
    if (!m_offerteLista || !m_filtroTipo || !m_filtroLivello) return;

    const QString tipo    = m_filtroTipo->currentData().toString();
    const QString livello = m_filtroLivello->currentData().toString();

    m_offerteLista->clear();
    for (const auto& o : offerteFiltrate(tipo, livello)) {
        const QString emailStr = o.email.isEmpty() ? ""
                               : QString("  \xe2\x9c\x89 %1").arg(o.email);
        const QString text = tipoIcon(o.tipo) + o.azienda + " \xe2\x80\x94 " + o.ruolo
                           + "\n   \xf0\x9f\x93\x8d " + o.sede + livLabel(o.livello) + emailStr;

        auto* item = new QListWidgetItem(text, m_offerteLista);
        item->setSizeHint(QSize(-1, 52));
        item->setData(Qt::UserRole, QVariant::fromValue(o));
        if (!o.email.isEmpty())
            item->setForeground(QColor("#4CAF50"));
    }
}

/* ══════════════════════════════════════════════════════════════
   popolaModelli — aggiorna il combo con i modelli disponibili
   ══════════════════════════════════════════════════════════════ */
void LavoroPage::popolaModelli(const QStringList& models) {
    if (!m_cmbModello) return;
    const QString current = m_cmbModello->currentText();
    m_cmbModello->blockSignals(true);
    m_cmbModello->clear();
    for (const auto& m : models) {
        m_cmbModello->addItem(m);
        if (P::isKnownBrokenModel(m)) {
            const int i = m_cmbModello->count() - 1;
            m_cmbModello->setItemData(i, QBrush(QColor("#ea580c")), Qt::ForegroundRole);
            m_cmbModello->setItemData(i, QBrush(QColor("#fef08a")), Qt::BackgroundRole);
            m_cmbModello->setItemData(i,
                P::knownBrokenModelTooltip(),
                Qt::ToolTipRole);
        }
    }
    // ripristina selezione precedente se ancora disponibile
    const int idx = m_cmbModello->findText(current);
    m_cmbModello->setCurrentIndex(idx >= 0 ? idx : 0);
    m_cmbModello->blockSignals(false);
    if (m_modelloLbl)
        m_modelloLbl->setText("\xf0\x9f\xa4\x96 " + m_cmbModello->currentText());
}

/* ══════════════════════════════════════════════════════════════
   Costruttore LavoroPage
   ══════════════════════════════════════════════════════════════ */
LavoroPage::LavoroPage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(16, 12, 16, 12);
    lay->setSpacing(4);

    /* ── Riga superiore: CV + Modello LLM ── */
    auto* topRow = new QWidget(this);
    auto* topL   = new QHBoxLayout(topRow);
    topL->setContentsMargins(0,0,0,0); topL->setSpacing(12);

    /* CV Picker */
    auto* cvBox = new QGroupBox("\xf0\x9f\x93\x84  Curriculum Vitae", topRow);
    auto* cvLay = new QHBoxLayout(cvBox);
    cvLay->setSpacing(8);

    m_cvPath = new QLineEdit(cvBox);
    m_cvPath->setPlaceholderText("Percorso file PDF...");
    m_cvPath->setReadOnly(true);

    auto* sfogliaBtn = new QPushButton("\xf0\x9f\x93\x82 Sfoglia...", cvBox);
    sfogliaBtn->setObjectName("actionBtn");
    sfogliaBtn->setFixedWidth(90);

    m_cvStatus = new QLabel("Nessun CV caricato", cvBox);
    m_cvStatus->setObjectName("pageSubtitle");

    cvLay->addWidget(m_cvPath, 1);
    cvLay->addWidget(sfogliaBtn);
    cvLay->addWidget(m_cvStatus);

    /* LLM Selector */
    auto* llmBox = new QGroupBox("\xf0\x9f\xa4\x96  Modello AI", topRow);
    auto* llmLay = new QHBoxLayout(llmBox);
    llmLay->setSpacing(8);

    m_cmbModello = new QComboBox(llmBox);
    m_cmbModello->setObjectName("cmbModello");
    m_cmbModello->setMinimumWidth(180);
    m_cmbModello->addItem("\xf0\x9f\x94\x84 Caricamento modelli...");

    {
        const QString cur = m_ai->model();
        m_modelloLbl = new QLabel(cur.isEmpty() ? "\xf0\x9f\xa4\x96 —" : "\xf0\x9f\xa4\x96 " + cur, llmBox);
    }
    m_modelloLbl->setObjectName("pageSubtitle");

    auto* fetchBtn = new QPushButton("\xf0\x9f\x94\x84", llmBox);
    fetchBtn->setObjectName("actionBtn");
    fetchBtn->setFixedWidth(34);
    fetchBtn->setToolTip("Aggiorna lista modelli");

    llmLay->addWidget(m_cmbModello, 1);
    llmLay->addWidget(fetchBtn);
    llmLay->addWidget(m_modelloLbl);

    auto* toggleBtn = new QPushButton("\xe2\x96\xb2", topRow);
    toggleBtn->setObjectName("actionBtn");
    toggleBtn->setFixedSize(22, 22);
    toggleBtn->setToolTip("Comprimi riga filtri");

    topL->addWidget(cvBox, 3);
    topL->addWidget(llmBox, 2);
    topL->addWidget(toggleBtn);
    lay->addWidget(topRow);

    connect(sfogliaBtn, &QPushButton::clicked, this, [this]{
        const QString path = QFileDialog::getOpenFileName(
            this, "Seleziona Curriculum Vitae",
            QDir::homePath(), "PDF (*.pdf);;Testo (*.txt);;Tutti (*)");
        if (!path.isEmpty()) {
            m_cvPath->setText(path);
            caricaCV(path);
        }
    });

    // Pre-carica CV di Paolo automaticamente
    const QString defaultCv = "/home/wildlux/CURRICULUM/IT_CV_18_05_2025_Paolo_Lo_Bello.pdf";
    if (QFile::exists(defaultCv)) {
        m_cvPath->setText(defaultCv);
        caricaCV(defaultCv);
    }

    // Connessioni modello
    connect(m_ai, &AiClient::modelsReady, this, &LavoroPage::popolaModelli);
    connect(fetchBtn, &QPushButton::clicked, m_ai, &AiClient::fetchModels);
    connect(m_cmbModello, &QComboBox::currentTextChanged, this, [this](const QString& m){
        if (m_modelloLbl) m_modelloLbl->setText("\xf0\x9f\xa4\x96 " + m);
        if (!m.isEmpty() && !m.startsWith("\xf0\x9f\x94\x84"))
            m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), m);
    });
    m_ai->fetchModels();

    /* ── Filtri ── */
    auto* filtriRow = new QWidget(this);
    auto* filtriL   = new QHBoxLayout(filtriRow);
    filtriL->setContentsMargins(0,0,0,0); filtriL->setSpacing(10);

    filtriL->addWidget(new QLabel("\xf0\x9f\x94\x8d Tipo:", filtriRow));
    m_filtroTipo = new QComboBox(filtriRow);
    m_filtroTipo->setObjectName("filtroTipo");
    m_filtroTipo->setFixedWidth(185);

    const struct { const char* label; const char* data; } tipi[] = {
        {"Tutti i tipi",                   "tutti"},
        {"\xf0\x9f\x92\xbb IT / Informatica",              "IT"},
        {"\xf0\x9f\x9b\x92 Retail / Vendite dettaglio",   "Retail"},
        {"\xf0\x9f\x8d\xbd Ristorazione",                  "Ristorazione"},
        {"\xf0\x9f\x8f\x97 Edilizia",                      "Edilizia"},
        {"\xf0\x9f\x93\xa6 Logistica / Magazzino",         "Logistica"},
        {"\xf0\x9f\x92\xb0 Finanza / Assicurazioni",       "Finanza"},
        {"\xf0\x9f\x8f\xa5 Sanitario",                     "Sanitario"},
        {"\xe2\x9a\x99\xef\xb8\x8f Produzione",            "Produzione"},
        {"\xf0\x9f\x94\xa7 Tecnico / Impianti",            "Tecnico"},
        {"\xe2\x9c\x88\xef\xb8\x8f Turismo / Crociere",   "Turismo"},
        {"\xf0\x9f\x93\x8b Admin / Segreteria",            "Admin"},
        {"\xf0\x9f\x93\x8a Commerciale / Marketing",       "Commerciale"},
        {"\xf0\x9f\x94\xb9 Altro",                         "Altro"},
    };
    for (const auto& t : tipi)
        m_filtroTipo->addItem(t.label, QString(t.data));
    filtriL->addWidget(m_filtroTipo);

    filtriL->addSpacing(16);
    filtriL->addWidget(new QLabel("\xf0\x9f\x8e\x93 Istruzione:", filtriRow));
    m_filtroLivello = new QComboBox(filtriRow);
    m_filtroLivello->setObjectName("filtroLivello");
    m_filtroLivello->setFixedWidth(210);

    const struct { const char* label; const char* data; } livelli[] = {
        {"Tutti i livelli",                      "tutti"},
        {"\xf0\x9f\x9f\xa2 Media inferiore (nessun titolo)",  "media"},
        {"\xf0\x9f\x9f\xa1 Diploma superiore",                 "diploma"},
        {"\xf0\x9f\x9f\xa0 Laurea triennale",                  "laurea_t"},
        {"\xf0\x9f\x94\xb4 Laurea magistrale / Master",        "laurea_m"},
    };
    for (const auto& l : livelli)
        m_filtroLivello->addItem(l.label, QString(l.data));
    m_filtroLivello->setCurrentIndex(2);  // default: Diploma — profilo di Paolo
    filtriL->addWidget(m_filtroLivello);

    auto* filtriBtn = new QPushButton("\xf0\x9f\x94\x84", filtriRow);
    filtriBtn->setObjectName("actionBtn");
    filtriBtn->setFixedWidth(28);
    filtriBtn->setToolTip("Aggiorna filtri");
    connect(filtriBtn, &QPushButton::clicked, this, &LavoroPage::applicaFiltri);
    connect(m_filtroTipo,    QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LavoroPage::applicaFiltri);
    connect(m_filtroLivello, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LavoroPage::applicaFiltri);
    filtriL->addWidget(filtriBtn);

    m_analizzaUrlBtn = new QPushButton("\xf0\x9f\x94\x97 Analizza URL", filtriRow);
    m_analizzaUrlBtn->setObjectName("actionBtn");
    m_analizzaUrlBtn->setToolTip("Analizza uno o più URL di annunci di lavoro");
    m_analizzaCvBtn = new QPushButton("\xf0\x9f\xa4\x96 Analizza CV", filtriRow);
    m_analizzaCvBtn->setObjectName("actionBtn");
    m_analizzaCvBtn->setToolTip("Chiedi all'AI di analizzare il tuo CV con una domanda predefinita o personalizzata");
    m_stopAiBtn = new QPushButton("\xe2\x8f\xb9", filtriRow);
    m_stopAiBtn->setObjectName("actionBtn");
    m_stopAiBtn->setProperty("danger", true);
    m_stopAiBtn->setFixedWidth(28);
    m_stopAiBtn->setEnabled(false);
    m_stopAiBtn->setToolTip("Interrompi elaborazione AI");
    filtriL->addWidget(m_analizzaUrlBtn);
    filtriL->addWidget(m_analizzaCvBtn);
    filtriL->addWidget(m_stopAiBtn);
    filtriL->addStretch(1);
    lay->addWidget(filtriRow);

    connect(toggleBtn, &QPushButton::clicked, this, [=]{
        const bool nowVisible = !cvBox->isVisible();
        cvBox->setVisible(nowVisible);
        llmBox->setVisible(nowVisible);
        filtriRow->setVisible(nowVisible);
        toggleBtn->setText(nowVisible ? "\xe2\x96\xb2" : "\xe2\x96\xbc");
        toggleBtn->setToolTip(nowVisible ? "Comprimi" : "Espandi");
    });

    /* ── Splitter: lista | output AI ── */
    auto* splitter = new QSplitter(Qt::Vertical, this);

    auto* topPane = new QWidget(splitter);
    auto* topLay  = new QVBoxLayout(topPane);
    topLay->setContentsMargins(0,0,0,0); topLay->setSpacing(4);

    m_offerteLista = new QListWidget(topPane);
    m_offerteLista->setObjectName("offerteList");
    m_offerteLista->setWordWrap(true);
    m_offerteLista->setAlternatingRowColors(true);
    topLay->addWidget(m_offerteLista, 1);

    /* ── Pannello link dinamici (offerta selezionata) ── */
    m_linksLbl = new QLabel(topPane);
    m_linksLbl->setObjectName("hintLabel");
    m_linksLbl->setOpenExternalLinks(true);
    m_linksLbl->setTextFormat(Qt::RichText);
    m_linksLbl->setWordWrap(true);
    m_linksLbl->setText("<i>Seleziona un'offerta per vedere i link</i>");
    topLay->addWidget(m_linksLbl);

    auto* azioniRow = new QWidget(topPane);
    auto* azioniL   = new QHBoxLayout(azioniRow);
    azioniL->setContentsMargins(0,4,0,0); azioniL->setSpacing(8);

    auto* genBtn   = new QPushButton("\xf0\x9f\xa4\x96 Lettera via AI", azioniRow);
    genBtn->setObjectName("actionBtn");
    genBtn->setToolTip("Genera una lettera di candidatura via email per l'offerta selezionata");
    auto* genCoverBtn = new QPushButton("\xf0\x9f\x93\x84 Cover letter via AI", azioniRow);
    genCoverBtn->setObjectName("actionBtn");
    genCoverBtn->setToolTip("Genera una cover letter professionale da allegare alla candidatura");
    auto* emailBtn = new QPushButton("\xe2\x9c\x89 Copia Email", azioniRow);
    emailBtn->setObjectName("actionBtn");
    emailBtn->setEnabled(false);
    emailBtn->setToolTip("Disponibile solo per offerte con email diretta");
    auto* copiaBtn = new QPushButton("\xf0\x9f\x93\x8b Copia Lettera", azioniRow);
    copiaBtn->setObjectName("actionBtn");
    copiaBtn->setEnabled(false);
    copiaBtn->setToolTip("Copia la lettera email generata negli appunti");
    auto* copiaCoverBtn = new QPushButton("\xf0\x9f\x93\x8b Copia Cover", azioniRow);
    copiaCoverBtn->setObjectName("actionBtn");
    copiaCoverBtn->setEnabled(false);
    copiaCoverBtn->setToolTip("Copia la cover letter negli appunti");
    auto* selLbl = new QLabel("Seleziona un'offerta (doppio clic = genera subito)", azioniRow);
    selLbl->setObjectName("pageSubtitle");
    azioniL->addWidget(genBtn); azioniL->addWidget(genCoverBtn);
    azioniL->addWidget(emailBtn); azioniL->addWidget(copiaBtn); azioniL->addWidget(copiaCoverBtn);
    azioniL->addWidget(selLbl, 1);
    topLay->addWidget(azioniRow);
    splitter->addWidget(topPane);

    auto* botPane = new QWidget(splitter);
    auto* botLay  = new QVBoxLayout(botPane);
    botLay->setContentsMargins(0,0,0,0); botLay->setSpacing(6);

    /* ── Layout orizzontale: Email (sx) | Cover Letter (dx) ── */
    auto* colSplit = new QSplitter(Qt::Horizontal, botPane);

    /* Colonna sinistra — lettera email */
    auto* emailCol = new QWidget(colSplit);
    auto* emailLay = new QVBoxLayout(emailCol);
    emailLay->setContentsMargins(0,0,2,0); emailLay->setSpacing(2);
    auto* emailHdr = new QLabel("\xe2\x9c\x89\xef\xb8\x8f  Lettera Email", emailCol);
    emailHdr->setObjectName("pageSubtitle");
    emailLay->addWidget(emailHdr);

    m_lavoroLog = new QTextEdit(emailCol);
    m_lavoroLog->setObjectName("chatLog");
    m_lavoroLog->setReadOnly(true);
    m_lavoroLog->setPlaceholderText(
        "Lettera di candidatura via email.\n"
        "Seleziona un'offerta e clicca \"Genera Lettera con AI\".");
    m_lavoroLog->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_lavoroLog, &QTextEdit::customContextMenuRequested, m_lavoroLog, [this](const QPoint& pos){
        const QString sel = m_lavoroLog->textCursor().selectedText();
        const bool hasSel = !sel.isEmpty();
        QMenu menu(m_lavoroLog);
        QAction* actCopy = menu.addAction("\xf0\x9f\x97\x82  Copia " + QString(hasSel ? "selezione" : "tutto"));
        QAction* actRead = menu.addAction("\xf0\x9f\x8e\x99  Leggi " + QString(hasSel ? "selezione" : "tutto"));
        QAction* chosen  = menu.exec(m_lavoroLog->mapToGlobal(pos));
        const QString txt = hasSel ? sel : m_lavoroLog->toPlainText();
        if (chosen == actCopy) QGuiApplication::clipboard()->setText(txt);
        else if (chosen == actRead) {
            QStringList words = txt.split(' ', Qt::SkipEmptyParts);
            if (words.size() > 400) words = words.mid(words.size() - 400);
            QProcess::startDetached("espeak-ng", {"-v", "it+f3", "--punct=none", words.join(" ")});
        }
    });
    emailLay->addWidget(m_lavoroLog, 1);
    colSplit->addWidget(emailCol);

    /* Colonna destra — cover letter */
    auto* coverCol = new QWidget(colSplit);
    auto* coverLay = new QVBoxLayout(coverCol);
    coverLay->setContentsMargins(2,0,0,0); coverLay->setSpacing(2);
    auto* coverHdr = new QLabel("\xf0\x9f\x93\x84  Cover Letter", coverCol);
    coverHdr->setObjectName("pageSubtitle");
    coverLay->addWidget(coverHdr);

    m_coverLog = new QTextEdit(coverCol);
    m_coverLog->setObjectName("chatLog");
    m_coverLog->setReadOnly(true);
    m_coverLog->setPlaceholderText(
        "Cover Letter professionale da allegare alla candidatura.\n"
        "Clicca \"Genera Cover Letter\" dopo aver selezionato un'offerta.");
    m_coverLog->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_coverLog, &QTextEdit::customContextMenuRequested, m_coverLog, [this](const QPoint& pos){
        const QString sel = m_coverLog->textCursor().selectedText();
        const bool hasSel = !sel.isEmpty();
        QMenu menu(m_coverLog);
        QAction* actCopy = menu.addAction("\xf0\x9f\x97\x82  Copia " + QString(hasSel ? "selezione" : "tutto"));
        QAction* chosen  = menu.exec(m_coverLog->mapToGlobal(pos));
        if (chosen == actCopy)
            QGuiApplication::clipboard()->setText(hasSel ? sel : m_coverLog->toPlainText());
    });
    coverLay->addWidget(m_coverLog, 1);
    colSplit->addWidget(coverCol);

    colSplit->setStretchFactor(0, 1);
    colSplit->setStretchFactor(1, 1);
    botLay->addWidget(colSplit, 1);

    auto* waitLbl = new QLabel("\xe2\x8f\xb3  AI in elaborazione...", botPane);
    waitLbl->setStyleSheet("color:#E5C400; font-style:italic; padding:2px 0;");
    waitLbl->setVisible(false);
    botLay->addWidget(waitLbl);
    splitter->addWidget(botPane);

    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    lay->addWidget(splitter, 1);

    /* ── Logica ── */

    // Profilo CV di fallback — Paolo Lo Bello (parametri di default)
    const QString cvFallback =
        "Paolo Lo Bello, nato il 15/02/1989, Catania (36 anni).\n"
        "Email: wildlux@gmail.com | Tel: +39 340 96 25 057\n"
        "Patente B Europea. Dislessico (certificato ASL Catania).\n\n"
        "TITOLO DI STUDIO:\n"
        "- Perito Informatico — ITIS G. Marconi, Catania (2003-2010) — voto 67/100 — MQRF Level 4\n"
        "- CCNA Cisco Certificate Associate — ICE Malta (2023-2024)\n"
        "- Certificato E-Commerce (Joomla, PHP, HTML, Web Marketing) — CESIS (2010-2012)\n"
        "- Certificato Photoshop CS5 — ITIS Galileo Ferraris Acireale (2012)\n"
        "- Inglese A2 (ELA Malta, 2019)\n\n"
        "ESPERIENZE LAVORATIVE:\n"
        "- Lidl LTD Malta (lug 2024): cassa POS, muletto elettrico, controllo stock e date\n"
        "- Scott Supermarket Malta (apr 2020 - mag 2024): muletto manuale, facing product, ricollocamento\n"
        "- Playmobil/Poultons Ltd Malta (dic 2019 - feb 2020): operatore macchina, controllo qualita'\n"
        "- Convenience Shop Malta (giu-ago 2019): assistente negozio, cassa, scaffali\n"
        "- Karma Swim Catania (2014-2015): grafico freelance — Adobe Illustrator\n"
        "- Techno Work srl Catania (nov 2013): Python developer su Raspberry Pi 3, GNU/Linux\n"
        "- Almaviva Misterbianco (gen-feb 2012): call center Mediaset Premium\n"
        "- Mics SRL Misterbianco (giu-lug 2011): inbound call operator Enel Energia\n"
        "- Gio' Casa Misterbianco (ago 2005): assistenza e vendita condizionatori\n\n"
        "COMPETENZE TECNICHE:\n"
        "- Reti: CCNA Cisco, SSH, Kleopatra, PuTTY, FileZilla\n"
        "- Sviluppo: Python, C++, HTML, PHP, SQL, MySQL, JavaScript, Node.js, Assembler x86\n"
        "- OS: GNU/Linux, macOS, Windows\n"
        "- Web: WordPress, Joomla, Prestashop, Django\n"
        "- Grafica: Adobe Photoshop CS5, GIMP, Adobe Illustrator\n"
        "- 3D: Blender (2007-oggi) — mesh, rigging, rendering, video promozionali\n"
        "- Virtual: VirtualBox, Docker\n"
        "- Office: LibreOffice, Microsoft Office, gestione email";

    // Prompt Socratico comune — applicato a tutte le richieste AI di questa sezione
    const QString socraticoBase =
        "\n\nMETODOLOGIA SOCRATICA: Sii rigorosamente onesto. "
        "Non adulare il candidato. Se il profilo presenta lacune rispetto ai requisiti, "
        "indicale chiaramente. Proponi domande critiche che aiutino a migliorare la candidatura. "
        "Per ogni domanda critica fornisci anche un breve suggerimento su come rispondere "
        "o controbattere efficacemente in fase di colloquio (es. 'Risposta consigliata: ...'). "
        "Distingui tra punti di forza reali e affermazioni non verificabili. "
        "L'obiettivo e' la verita', non la compiacenza.";

    /* ── Logica URL Analyzer ── */
    m_nam = new QNetworkAccessManager(this);

    /* ── Analisi URL (uno o più in parallelo) ── */
    auto analizzaUrls = [=](const QStringList& urlList) {
        if (urlList.isEmpty()) return;

        m_analizzaUrlBtn->setEnabled(false);
        m_analizzaCvBtn->setEnabled(false);
        m_stopAiBtn->setEnabled(true);
        waitLbl->setVisible(true);

        m_lavoroLog->clear();
        m_lavoroLog->append(QString(
            "\xf0\x9f\x94\x97 [ANALISI URL] — %1 link\n").arg(urlList.size()));

        auto pending  = std::make_shared<int>(urlList.size());
        auto combined = std::make_shared<QString>();

        for (const QString& rawUrl : urlList) {
            if (!rawUrl.startsWith("http://") && !rawUrl.startsWith("https://")) {
                m_lavoroLog->append(QString("\xe2\x9a\xa0 URL non valido: %1").arg(rawUrl));
                if (--(*pending) == 0) {
                    m_analizzaUrlBtn->setEnabled(true); m_analizzaCvBtn->setEnabled(true);
                    m_stopAiBtn->setEnabled(false); waitLbl->setVisible(false);
                }
                continue;
            }
            m_lavoroLog->append(QString("\xf0\x9f\x8c\x90 Scarico: %1").arg(rawUrl));

            QNetworkRequest req{QUrl(rawUrl)};
            req.setHeader(QNetworkRequest::UserAgentHeader,
                          "Mozilla/5.0 (X11; Linux x86_64) Prismalux/2.8");
            req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::NoLessSafeRedirectPolicy);
            req.setTransferTimeout(15000);
            auto* reply = m_nam->get(req);

            connect(reply, &QNetworkReply::finished, this, [=]{
                reply->deleteLater();
                if (reply->error() == QNetworkReply::NoError) {
                    QString html = QString::fromUtf8(reply->read(512 * 1024));
                    QTextDocument doc; doc.setHtml(html);
                    QString testo = doc.toPlainText()
                        .replace(QRegularExpression("[ \\t]{2,}"), " ")
                        .replace(QRegularExpression("\\n{3,}"), "\n\n").trimmed();
                    if (testo.size() > 1800) testo = testo.left(1800);
                    *combined += QString("\n\n=== %1 ===\n%2").arg(rawUrl, testo);
                } else {
                    m_lavoroLog->append(
                        QString("\xe2\x9d\x8c Errore: %1").arg(reply->errorString()));
                }

                if (--(*pending) > 0) return;   /* aspetta gli altri reply */

                /* Tutti i download completati — invio all'AI */
                if (combined->trimmed().isEmpty()) {
                    m_lavoroLog->append(
                        "\xe2\x9a\xa0 Nessun contenuto leggibile (potrebbero richiedere JavaScript/login).");
                    m_analizzaUrlBtn->setEnabled(true); m_analizzaCvBtn->setEnabled(true);
                    m_stopAiBtn->setEnabled(false); waitLbl->setVisible(false);
                    return;
                }

                m_lavoroLog->append("\xe2\x9c\x85 Download completato \xe2\x86\x92 Analisi AI...\n");
                const QString cvInfo  = m_cvText.isEmpty() ? cvFallback.left(2000) : m_cvText.left(2000);
                const QString modello = m_cmbModello ? m_cmbModello->currentText() : m_ai->model();
                if (!modello.isEmpty() && !modello.startsWith("\xf0\x9f\x94\x84"))
                    m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), modello);

                const int nUrl = urlList.size();
                const QString sys = nUrl == 1
                    ? QString(
                        "Sei un esperto di carriera. Analizza l'annuncio e rispondi in italiano.\n\n"
                        "=== PROFILO CANDIDATO ===\n%1\n\n"
                        "=== TESTO ANNUNCIO (da URL) ===\n%2\n\n"
                        "1. \xf0\x9f\x8f\xa2 RUOLO E AZIENDA (2-3 righe)\n"
                        "2. \xe2\x9c\x85 REQUISITI FONDAMENTALI\n"
                        "3. \xe2\xad\x90 NICE-TO-HAVE\n"
                        "4. \xf0\x9f\xa4\x96 COMPATIBILIT\xc3\x80 CON IL PROFILO\n"
                        "5. \xf0\x9f\x8e\xaf RACCOMANDAZIONE s\xc3\xac/no\n\nMax 400 parole.%3")
                        .arg(cvInfo, *combined, socraticoBase)
                    : QString(
                        "Sei un esperto di carriera. Analizza i seguenti annunci in italiano.\n\n"
                        "=== PROFILO CANDIDATO ===\n%1\n\n"
                        "=== ANNUNCI ===\n%2\n\n"
                        "Per ogni annuncio: ruolo/azienda, requisiti chiave, "
                        "compatibilit\xc3\xa0 col profilo, consiglio s\xc3\xac/no. Max 500 parole.%3")
                        .arg(cvInfo, *combined, socraticoBase);

                m_myReqId = m_ai->chat(sys,
                    nUrl == 1
                    ? "Analizza questo annuncio e valuta la compatibilit\xc3\xa0 col mio profilo."
                    : "Analizza questi annunci e valuta quale si adatta meglio al mio profilo.");
            });

            connect(m_stopAiBtn, &QPushButton::clicked, reply, [reply]{ reply->abort(); });
        }
    };

    connect(m_offerteLista, &QListWidget::currentItemChanged, this,
        [=](QListWidgetItem* cur, QListWidgetItem*){
            if (!cur) {
                selLbl->setText("Seleziona un'offerta dalla lista");
                emailBtn->setEnabled(false);
                m_linksLbl->setText("<i>Seleziona un'offerta per vedere i link</i>");
                return;
            }
            const auto o = cur->data(Qt::UserRole).value<Offerta>();
            selLbl->setText(o.azienda + " \xe2\x80\x94 " + o.ruolo);
            emailBtn->setEnabled(!o.email.isEmpty());
            if (!o.email.isEmpty()) emailBtn->setToolTip("\xf0\x9f\x93\x8b Copia: " + o.email);

            /* ── Link dinamici ── */
            const QString sd   = QUrl::toPercentEncoding(o.sede);
            const QString azRl = QUrl::toPercentEncoding(o.azienda + " " + o.ruolo + " lavoro");
            const QString azLav = QUrl::toPercentEncoding(o.azienda + " lavora con noi careers");

            const QString urlAnnuncio = "https://www.google.com/search?q=" + azRl;
            const QString urlMaps     = "https://www.google.com/maps/search/" + sd;

            /* Per offerte GDO il campo requisiti contiene l'URL diretto del portale */
            const bool hasDirectUrl = o.requisiti.startsWith("http");
            const QString urlLavora = hasDirectUrl
                ? o.requisiti
                : "https://www.google.com/search?q=" + azLav;

            m_linksLbl->setText(
                "\xf0\x9f\x94\x97 <a href='" + urlAnnuncio + "'>Cerca annuncio</a>"
                " &nbsp;|&nbsp;"
                "\xf0\x9f\x8f\xa2 <a href='" + urlLavora   + "'>Lavora con noi</a>"
                " &nbsp;|&nbsp;"
                "\xf0\x9f\x97\xba <a href='" + urlMaps     + "'>" + o.sede + " — Google Maps</a>");
        });

    connect(emailBtn, &QPushButton::clicked, this, [=]{
        auto* cur = m_offerteLista->currentItem();
        if (!cur) return;
        const auto o = cur->data(Qt::UserRole).value<Offerta>();
        if (!o.email.isEmpty()) QGuiApplication::clipboard()->setText(o.email);
    });
    connect(m_lavoroLog, &QTextEdit::textChanged, this, [=]{
        copiaBtn->setEnabled(!m_lavoroLog->toPlainText().trimmed().isEmpty());
    });
    connect(copiaBtn, &QPushButton::clicked, this, [this]{
        QGuiApplication::clipboard()->setText(m_lavoroLog->toPlainText());
    });
    connect(m_coverLog, &QTextEdit::textChanged, this, [=]{
        copiaCoverBtn->setEnabled(!m_coverLog->toPlainText().trimmed().isEmpty());
    });
    connect(copiaCoverBtn, &QPushButton::clicked, this, [this]{
        QGuiApplication::clipboard()->setText(m_coverLog->toPlainText());
    });

    auto genLettera = [=]{
        auto* cur = m_offerteLista->currentItem();
        if (!cur) { m_lavoroLog->append("\xe2\x9a\xa0  Seleziona prima un'offerta."); return; }
        const auto o = cur->data(Qt::UserRole).value<Offerta>();
        const QString cvInfo  = m_cvText.isEmpty() ? cvFallback : m_cvText.left(3500);
        const QString modello = m_cmbModello ? m_cmbModello->currentText() : m_ai->model();

        // Applica il modello selezionato
        if (!modello.isEmpty() && !modello.startsWith("\xf0\x9f\x94\x84"))
            m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), modello);

        QString emailHint;
        if (!o.email.isEmpty())
            emailHint = QString("\n\nNOTA: L'email di candidatura e': %1").arg(o.email);

        const QString sys = QString(
            "Sei un esperto di risorse umane e scrittura professionale italiana.\n"
            "Scrivi una lettera di candidatura formale, concisa e personalizzata.\n\n"
            "=== CURRICULUM VITAE DEL CANDIDATO ===\n%1\n\n"
            "=== OFFERTA DI LAVORO ===\n"
            "Azienda: %2\nRuolo: %3\nSede: %4\nRequisiti: %5%6\n\n"
            "=== ISTRUZIONI ===\n"
            "- Tono professionale e diretto\n"
            "- Evidenzia SOLO le competenze rilevanti per QUESTO ruolo specifico\n"
            "- Massimo 280 parole\n"
            "- Inizia con 'Gentile Ufficio Risorse Umane di %2,'\n"
            "- Termina con disponibilit\xc3\xa0 per colloquio\n"
            "- Scrivi SOLO in italiano\n"
            "- Non inventare informazioni non presenti nel CV%7"
        ).arg(cvInfo, o.azienda, o.ruolo, o.sede, o.requisiti, emailHint, socraticoBase);

        // Header di output: TAG identifica chiaramente la sorgente
        m_lavoroLog->clear();
        m_lavoroLog->append(QString(
            "\xf0\x9f\x92\xbc  [CERCA LAVORO \xe2\x86\x92 Genera Lettera]\n"
            "\xf0\x9f\xa4\x96  Modello: %1\n"
            "\xf0\x9f\x8f\xa2  Azienda: %2 \xe2\x80\x94 %3\n"
            "\xf0\x9f\x93\x8d  Sede: %4\n").arg(modello, o.azienda, o.ruolo, o.sede));
        if (!o.email.isEmpty())
            m_lavoroLog->append(QString("\xe2\x9c\x89  Email destinatario: %1\n").arg(o.email));
        m_lavoroLog->append("\n\xf0\x9f\x93\x9d  Lettera:\n");

        m_analizzaUrlBtn->setEnabled(false); m_analizzaCvBtn->setEnabled(false);
        m_stopAiBtn->setEnabled(true); waitLbl->setVisible(true);
        m_myReqId = m_ai->chat(sys, QString("Genera la lettera di candidatura per il ruolo di %1 presso %2.")
                                        .arg(o.ruolo, o.azienda));
    };

    auto genCover = [=]{
        auto* cur = m_offerteLista->currentItem();
        if (!cur) { m_coverLog->append("\xe2\x9a\xa0  Seleziona prima un'offerta."); return; }
        const auto o = cur->data(Qt::UserRole).value<Offerta>();
        const QString cvInfo  = m_cvText.isEmpty() ? cvFallback : m_cvText.left(3500);
        const QString modello = m_cmbModello ? m_cmbModello->currentText() : m_ai->model();
        if (!modello.isEmpty() && !modello.startsWith("\xf0\x9f\x94\x84"))
            m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), modello);

        const QString sysCover = QString(
            "Sei un esperto di risorse umane. Scrivi una COVER LETTER professionale (non una lettera email).\n"
            "La cover letter \xc3\xa8 un documento formale di massimo 3 paragrafi da allegare al CV.\n\n"
            "=== CURRICULUM VITAE ===\n%1\n\n"
            "=== OFFERTA DI LAVORO ===\n"
            "Azienda: %2\nRuolo: %3\nSede: %4\nRequisiti: %5\n\n"
            "=== STRUTTURA RICHIESTA ===\n"
            "1) APERTURA: chi sei, perch\xc3\xa9 ti candidi a questa azienda specifica\n"
            "2) CORPO: 2-3 competenze specifiche rilevanti per il ruolo (con esempi concreti)\n"
            "3) CHIUSURA: disponibilit\xc3\xa0 colloquio, contatti\n\n"
            "Tono: formale, conciso, max 300 parole. Solo in italiano. No adulazione generica.%6"
        ).arg(cvInfo, o.azienda, o.ruolo, o.sede, o.requisiti, socraticoBase);

        m_coverLog->clear();
        m_coverLog->append(QString(
            "\xf0\x9f\x93\x84  [COVER LETTER]\n"
            "\xf0\x9f\xa4\x96  Modello: %1\n"
            "\xf0\x9f\x8f\xa2  %2 \xe2\x80\x94 %3\n\n").arg(modello, o.azienda, o.ruolo));

        m_analizzaUrlBtn->setEnabled(false); m_analizzaCvBtn->setEnabled(false);
        m_stopAiBtn->setEnabled(true); waitLbl->setVisible(true);
        m_myCoverReqId = m_ai->chat(sysCover,
            QString("Scrivi la cover letter per %1 presso %2.").arg(o.ruolo, o.azienda));
    };

    connect(genBtn, &QPushButton::clicked, this, genLettera);
    connect(genCoverBtn, &QPushButton::clicked, this, genCover);
    connect(m_offerteLista, &QListWidget::itemDoubleClicked, this, [=](QListWidgetItem*){ genLettera(); });

    auto sendFn = [=](const QString& msg) {
        if (msg.isEmpty()) return;
        const QString cvInfo  = m_cvText.isEmpty() ? cvFallback.left(1500) : m_cvText.left(2000);
        const QString modello = m_cmbModello ? m_cmbModello->currentText() : m_ai->model();
        if (!modello.isEmpty() && !modello.startsWith("\xf0\x9f\x94\x84"))
            m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), modello);

        const QString sys = QString(
            "Sei un career coach italiano esperto in ricerca del lavoro, CV e colloqui.\n"
            "Il candidato ha questo profilo:\n%1\n"
            "Fornisci consigli concreti e pratici. Rispondi SEMPRE in italiano.%2"
        ).arg(cvInfo, socraticoBase);

        m_lavoroLog->append(QString("\n\xf0\x9f\xa4\x96 [ANALISI CV \xe2\x86\x92 %1]\n")
                                .arg(msg.left(50)));
        m_lavoroLog->append("\xf0\x9f\xa4\x96  AI: ");
        m_analizzaUrlBtn->setEnabled(false);
        m_analizzaCvBtn->setEnabled(false);
        m_stopAiBtn->setEnabled(true);
        waitLbl->setVisible(true);
        m_myReqId = m_ai->chat(sys, msg);
    };
    connect(m_stopAiBtn, &QPushButton::clicked, m_ai, &AiClient::abort);

    /* ── Dialog "Analizza URL" ── */
    connect(m_analizzaUrlBtn, &QPushButton::clicked, this, [=]{
        auto* dlg = new QDialog(this);
        dlg->setWindowTitle("Analizza URL annunci");
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->resize(540, 200);
        auto* dLay = new QVBoxLayout(dlg);
        dLay->addWidget(new QLabel(
            "Incolla uno o pi\xc3\xb9 URL di annunci di lavoro (uno per riga):", dlg));
        auto* urlEdit = new QTextEdit(dlg);
        urlEdit->setPlaceholderText(
            "https://www.linkedin.com/jobs/...\n"
            "https://it.indeed.com/...\n"
            "https://www.infojobs.it/...");
        dLay->addWidget(urlEdit, 1);
        auto* btnRow = new QWidget(dlg);
        auto* btnL   = new QHBoxLayout(btnRow);
        btnL->setContentsMargins(0,4,0,0);
        auto* okBtn  = new QPushButton("Analizza", btnRow);
        okBtn->setObjectName("actionBtn");
        auto* noBtn  = new QPushButton("Annulla", btnRow);
        noBtn->setObjectName("actionBtn");
        btnL->addStretch(1); btnL->addWidget(okBtn); btnL->addWidget(noBtn);
        dLay->addWidget(btnRow);
        connect(noBtn, &QPushButton::clicked, dlg, &QDialog::reject);
        connect(okBtn, &QPushButton::clicked, dlg, [=]{
            QStringList urls;
            for (const QString& line : urlEdit->toPlainText().split('\n', Qt::SkipEmptyParts))
                if (!line.trimmed().isEmpty()) urls << line.trimmed();
            if (!urls.isEmpty()) { dlg->accept(); analizzaUrls(urls); }
        });
        dlg->exec();
    });

    /* ── Dialog "Analizza CV" ── */
    connect(m_analizzaCvBtn, &QPushButton::clicked, this, [=]{
        auto* dlg = new QDialog(this);
        dlg->setWindowTitle("Analizza CV");
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->resize(530, 330);
        auto* dLay = new QVBoxLayout(dlg);
        dLay->addWidget(new QLabel(
            "Seleziona un'analisi predefinita (doppio clic = avvia subito):", dlg));
        auto* listW = new QListWidget(dlg);
        listW->setMaximumHeight(140);
        const QStringList presets = {
            "Punti deboli del CV rispetto al mercato del lavoro IT",
            "Falle che un HR potrebbe notare nella candidatura",
            "Competenze mancanti pi\xc3\xb9 richieste nel settore IT e Retail",
            "Suggerimenti per superare il filtro ATS (Applicant Tracking System)",
            "Valutazione coerenza e credibilit\xc3\xa0 del percorso lavorativo",
            "Come migliorare il CV per una candidatura GDO / supermercati",
        };
        listW->addItems(presets);
        dLay->addWidget(listW);
        dLay->addWidget(new QLabel("Oppure scrivi una domanda personalizzata:", dlg));
        auto* promptEdit = new QLineEdit(dlg);
        promptEdit->setObjectName("chatInput");
        promptEdit->setPlaceholderText(
            "Es: 'Cosa manca per essere assunto come programmatore junior?'");
        dLay->addWidget(promptEdit);
        connect(listW, &QListWidget::itemClicked, dlg,
                [=](QListWidgetItem* item){ promptEdit->setText(item->text()); });
        auto* btnRow = new QWidget(dlg);
        auto* btnL   = new QHBoxLayout(btnRow);
        btnL->setContentsMargins(0,4,0,0);
        auto* okBtn  = new QPushButton("Analizza", btnRow);
        okBtn->setObjectName("actionBtn");
        auto* noBtn  = new QPushButton("Annulla", btnRow);
        noBtn->setObjectName("actionBtn");
        btnL->addStretch(1); btnL->addWidget(okBtn); btnL->addWidget(noBtn);
        dLay->addWidget(btnRow);
        auto doAnalyze = [=]{
            const QString msg = promptEdit->text().trimmed();
            if (!msg.isEmpty()) { dlg->accept(); sendFn(msg); }
        };
        connect(noBtn,     &QPushButton::clicked,      dlg, &QDialog::reject);
        connect(okBtn,     &QPushButton::clicked,      dlg, doAnalyze);
        connect(promptEdit,&QLineEdit::returnPressed,  dlg, doAnalyze);
        connect(listW, &QListWidget::itemDoubleClicked, dlg, [=](QListWidgetItem* item){
            promptEdit->setText(item->text()); dlg->accept(); sendFn(item->text());
        });
        dlg->exec();
    });

    /* ── Segnali AI — lettera email (m_myReqId) e cover letter (m_myCoverReqId) ── */
    connect(m_ai, &AiClient::token, this, [this](const QString& t){
        const quint64 rid = m_ai->currentReqId();
        if (rid == m_myReqId) {
            QTextCursor c(m_lavoroLog->document()); c.movePosition(QTextCursor::End);
            c.insertText(t); m_lavoroLog->ensureCursorVisible();
        } else if (rid == m_myCoverReqId) {
            QTextCursor c(m_coverLog->document()); c.movePosition(QTextCursor::End);
            c.insertText(t); m_coverLog->ensureCursorVisible();
        }
    });
    auto aiDone = [this, waitLbl]{
        m_analizzaUrlBtn->setEnabled(true);
        m_analizzaCvBtn->setEnabled(true);
        m_stopAiBtn->setEnabled(false);
        waitLbl->setVisible(false);
    };
    connect(m_ai, &AiClient::finished, this, [this, aiDone](const QString&){
        const quint64 rid = m_ai->currentReqId();
        if (rid == m_myReqId)
            m_lavoroLog->append("\n\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80");
        else if (rid == m_myCoverReqId)
            m_coverLog->append("\n\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80");
        else return;
        aiDone();
    });
    connect(m_ai, &AiClient::error, this, [this, aiDone](const QString& err){
        const quint64 rid = m_ai->currentReqId();
        if (rid != m_myReqId && rid != m_myCoverReqId) return;
        const QString el = err.toLower();
        if (!el.contains("canceled") && !el.contains("operation canceled")) {
            QTextEdit* log = (rid == m_myReqId) ? m_lavoroLog : m_coverLog;
            log->append(QString("\n\xe2\x9d\x8c  Errore: %1").arg(err));
        }
        aiDone();
    });
    connect(m_ai, &AiClient::aborted, this, [this, aiDone]{
        const quint64 rid = m_ai->currentReqId();
        if (rid != m_myReqId && rid != m_myCoverReqId) return;
        QTextEdit* log = (rid == m_myReqId) ? m_lavoroLog : m_coverLog;
        log->append("\n\xe2\x8f\xb9  Interrotto.\n\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80");
        aiDone();
    });

    applicaFiltri();
}
