#include "main_jobs.h"
#include "../dpi_utils.h"
#include "main_jobs_data.h"
#include "../prismalux_paths.h"
#include "../widgets/proc_helper.h"
#include "../widgets/model_combo_helper.h"
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
#include <QTableWidget>
#include <QHeaderView>
#include <QFormLayout>
#include <QDate>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointer>
#include <QPixmap>
#include <QPainter>
#include <memory>

/* ══════════════════════════════════════════════════════════════
   Costanti di testo — restituite per riferimento (string statica)
   ══════════════════════════════════════════════════════════════ */
const QString& LavoroPage::cvFallback() {
    static const QString s =
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
    return s;
}

const QString& LavoroPage::socraticoBase() {
    static const QString s =
        "\n\nMETODOLOGIA SOCRATICA: Sii rigorosamente onesto. "
        "Non adulare il candidato. Se il profilo presenta lacune rispetto ai requisiti, "
        "indicale chiaramente. Proponi domande critiche che aiutino a migliorare la candidatura. "
        "Per ogni domanda critica fornisci anche un breve suggerimento su come rispondere "
        "o controbattere efficacemente in fase di colloquio (es. 'Risposta consigliata: ...'). "
        "Distingui tra punti di forza reali e affermazioni non verificabili. "
        "L'obiettivo e' la verita', non la compiacenza.";
    return s;
}

/* ══════════════════════════════════════════════════════════════
   caricaCV
   ══════════════════════════════════════════════════════════════ */
void LavoroPage::caricaCV(const QString& path) {
    if (!m_cvStatus) return;
    QFileInfo fi(path);
    if (!fi.exists()) {
        m_cvStatus->setText(tr("\xe2\x9d\x8c File non trovato"));
        m_cvStatus->setStyleSheet("color:#F44336;");
        return;
    }
    const auto pr = ProcHelper::run("pdftotext", {path, "-"}, 8000);
    if (pr.ok) {
        const QString txt = pr.out.trimmed();
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
    /* AiClient::fetchModels() non emette mai error() su fallimento di rete
       (operazione in background, vedi ai_client.cpp) — emette sempre
       modelsReady({}), quindi il contratto reale è gestire qui la lista
       vuota. Prima veniva passata direttamente a populate(), che con una
       lista vuota lascia il combo silenziosamente vuoto, senza dire
       all'utente "Ollama non raggiungibile" (stesso fix già presente in
       ManutenzioneePage::onBackendModelsReady). */
    if (models.isEmpty()) { ModelComboHelper::setError(m_cmbModello); return; }
    ModelComboHelper::populate(m_cmbModello, m_ai, models, m_modelloLbl);
}

/* ══════════════════════════════════════════════════════════════
   Costruttore LavoroPage
   ══════════════════════════════════════════════════════════════ */
LavoroPage::LavoroPage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    /* Contenuto storico (CV/Offerte/Tracker/Calcolatore) in una sotto-tab
       "Candidature", per lasciare spazio alla nuova sotto-tab "Assistente
       Candidature" senza toccare il layout esistente. */
    auto* candidatureTab = new QWidget(this);
    auto* lay = new QVBoxLayout(candidatureTab);
    lay->setContentsMargins(16, 12, 16, 12);
    lay->setSpacing(4);

    /* ── Riga superiore: CV + Modello LLM ──
       FIX layout (segnalato dall'utente: pulsanti tagliati/sovrapposti):
       la pagina vive nella colonna centrale dello splitter a 3 colonne
       dell'Assistente Candidature (~1/3 della finestra, non piena
       larghezza come in origine). CV box e LLM box affiancati non ci
       stanno: impilati in verticale. */
    auto* topRow = new QWidget(this);
    auto* topL   = new QVBoxLayout(topRow);
    topL->setContentsMargins(0,0,0,0); topL->setSpacing(6);

    /* CV Picker — riga controlli + riga stato: lo stato ("✅ CV caricato:
       nomefile.pdf (N car.)") può essere lungo, sulla stessa riga di
       path+Sfoglia forzava la larghezza minima del box oltre la colonna. */
    m_cvBox = new QGroupBox(tr("\xf0\x9f\x93\x84  Curriculum Vitae"), topRow);
    auto* cvVLay = new QVBoxLayout(m_cvBox);
    cvVLay->setSpacing(4);
    auto* cvRow = new QWidget(m_cvBox);
    auto* cvLay = new QHBoxLayout(cvRow);
    cvLay->setContentsMargins(0, 0, 0, 0);
    cvLay->setSpacing(8);

    /* Foto profilo — placeholder 48×48 arrotondato */
    m_fotoLbl = new QLabel(m_cvBox);
    m_fotoLbl->setObjectName("fotoProfiloLbl");
    m_fotoLbl->setFixedSize(48, 48);
    m_fotoLbl->setAlignment(Qt::AlignCenter);
    m_fotoLbl->setToolTip(tr("Foto profilo per il CV"));
    m_fotoLbl->setStyleSheet(
        "QLabel#fotoProfiloLbl{"
        "  border: 2px solid palette(mid);"
        "  border-radius: 24px;"
        "  background: palette(base);"
        "  color: palette(text);"
        "  font-size: 22px;"
        "}");
    m_fotoLbl->setText("\xf0\x9f\x91\xa4");  /* 👤 */
    auto* fotoPicker = new QPushButton("\xf0\x9f\x93\xb7", m_cvBox);
    fotoPicker->setObjectName("actionBtn");
    fotoPicker->setFixedSize(26, 26);
    fotoPicker->setToolTip(tr("Seleziona foto profilo (JPG / PNG)"));
    connect(fotoPicker, &QPushButton::clicked, this, &LavoroPage::onFotoBtnClicked);

    auto* fotoWrap = new QWidget(m_cvBox);
    auto* fotoWrapLay = new QVBoxLayout(fotoWrap);
    fotoWrapLay->setContentsMargins(0, 0, 0, 0);
    fotoWrapLay->setSpacing(2);
    fotoWrapLay->addWidget(m_fotoLbl);
    fotoWrapLay->addWidget(fotoPicker, 0, Qt::AlignHCenter);
    cvLay->addWidget(fotoWrap);

    m_cvPath = new QLineEdit(m_cvBox);
    m_cvPath->setPlaceholderText(tr("Percorso file PDF..."));
    m_cvPath->setReadOnly(true);
    m_cvPath->setAccessibleName(tr("Percorso curriculum vitae"));
    m_cvPath->setAccessibleDescription("Campo sola lettura con il percorso del file PDF del curriculum");

    auto* sfogliaBtn = new QPushButton(tr("\xf0\x9f\x93\x82 Sfoglia..."), m_cvBox);
    sfogliaBtn->setObjectName("actionBtn");
    sfogliaBtn->setFixedWidth(dpiScale(90));
    sfogliaBtn->setAccessibleName(tr("Sfoglia file curriculum"));

    m_cvStatus = new QLabel(tr("Nessun CV caricato"), m_cvBox);
    m_cvStatus->setObjectName("pageSubtitle");
    m_cvStatus->setWordWrap(true);

    cvLay->addWidget(m_cvPath, 1);
    cvLay->addWidget(sfogliaBtn);
    cvVLay->addWidget(cvRow);
    cvVLay->addWidget(m_cvStatus);

    /* LLM Selector — combo+refresh su una riga, nome modello sotto: il
       nome completo (es. "antconsales/antonio-gemma3-evo-q4:latest") è
       più largo della colonna, in linea col combo tagliava tutto. */
    m_llmBox = new QGroupBox(tr("\xf0\x9f\xa4\x96  Modello AI"), topRow);
    auto* llmVLay = new QVBoxLayout(m_llmBox);
    llmVLay->setSpacing(4);
    auto* llmRow = new QWidget(m_llmBox);
    auto* llmLay = new QHBoxLayout(llmRow);
    llmLay->setContentsMargins(0, 0, 0, 0);
    llmLay->setSpacing(8);

    m_cmbModello = new QComboBox(m_llmBox);
    m_cmbModello->setObjectName("cmbModello");
    m_cmbModello->setMinimumWidth(180);
    m_cmbModello->addItem("\xf0\x9f\x94\x84 Caricamento modelli...");
    m_cmbModello->setAccessibleName(tr("Selettore modello AI per analisi lavoro"));

    {
        const QString cur = m_ai->model();
        m_modelloLbl = new QLabel(cur.isEmpty() ? "\xf0\x9f\xa4\x96 —" : "\xf0\x9f\xa4\x96 " + cur, m_llmBox);
    }
    m_modelloLbl->setObjectName("pageSubtitle");

    auto* fetchBtn = new QPushButton("\xf0\x9f\x94\x84", m_llmBox);
    fetchBtn->setObjectName("actionBtn");
    fetchBtn->setFixedWidth(dpiScale(34));
    fetchBtn->setToolTip(tr("Aggiorna lista modelli"));

    m_toggleBtn = new QPushButton("\xe2\x96\xb2", topRow);
    m_toggleBtn->setObjectName("actionBtn");
    m_toggleBtn->setFixedSize(22, 22);
    m_toggleBtn->setToolTip(tr("Comprimi riga filtri"));

    llmLay->addWidget(m_cmbModello, 1);
    llmLay->addWidget(fetchBtn);
    llmLay->addWidget(m_toggleBtn);
    /* Il nome modello lungo non deve mai forzare la larghezza minima
       della colonna: policy Ignored (si tronca invece di allargare). */
    m_modelloLbl->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    llmVLay->addWidget(llmRow);
    llmVLay->addWidget(m_modelloLbl);

    topL->addWidget(m_cvBox);
    topL->addWidget(m_llmBox);
    lay->addWidget(topRow);

    connect(sfogliaBtn,  &QPushButton::clicked,
            this, &LavoroPage::onSfogliaBtnClicked);

    // Pre-carica il primo PDF trovato in ~/CURRICULUM/ (se esiste)
    const QDir cvDir(QDir::homePath() + "/CURRICULUM");
    const auto pdfList = cvDir.entryInfoList({"*.pdf"}, QDir::Files, QDir::Time);
    if (!pdfList.isEmpty()) {
        const QString defaultCv = pdfList.first().absoluteFilePath();
        m_cvPath->setText(defaultCv);
        caricaCV(defaultCv);
    }

    // Connessioni modello
    connect(m_ai, &AiClient::modelsReady, this, &LavoroPage::popolaModelli);
    connect(fetchBtn, &QPushButton::clicked, m_ai, &AiClient::fetchModels);
    connect(m_cmbModello, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LavoroPage::onModelloIndexChanged);
    m_ai->fetchModels();

    /* ── Filtri ──
       FIX layout: era un'unica riga orizzontale (~900px minimi con le
       larghezze fisse dei combo + 3 pulsanti) — nella colonna stretta i
       pulsanti finali (Analizza URL/CV/stop) sparivano fuori dal bordo.
       Ora 3 sotto-righe impilate; i combo si allargano/restringono con
       la colonna invece di avere larghezza fissa. */
    m_filtriRow = new QWidget(this);
    auto* filtriV = new QVBoxLayout(m_filtriRow);
    filtriV->setContentsMargins(0,0,0,0); filtriV->setSpacing(4);

    auto* tipoRow = new QWidget(m_filtriRow);
    auto* filtriL = new QHBoxLayout(tipoRow);
    filtriL->setContentsMargins(0,0,0,0); filtriL->setSpacing(10);

    filtriL->addWidget(new QLabel(tr("\xf0\x9f\x94\x8d Tipo:"), m_filtriRow));
    m_filtroTipo = new QComboBox(m_filtriRow);
    m_filtroTipo->setObjectName("filtroTipo");
    m_filtroTipo->setAccessibleName(tr("Filtro tipo di lavoro"));
    m_filtroTipo->setMinimumWidth(dpiScale(120));

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
    filtriL->addWidget(m_filtroTipo, 1);
    filtriV->addWidget(tipoRow);

    auto* livelloRow = new QWidget(m_filtriRow);
    auto* livelloL = new QHBoxLayout(livelloRow);
    livelloL->setContentsMargins(0,0,0,0); livelloL->setSpacing(10);
    livelloL->addWidget(new QLabel(tr("\xf0\x9f\x8e\x93 Istruzione:"), m_filtriRow));
    m_filtroLivello = new QComboBox(m_filtriRow);
    m_filtroLivello->setObjectName("filtroLivello");
    m_filtroLivello->setMinimumWidth(dpiScale(120));
    m_filtroLivello->setAccessibleName(tr("Filtro livello di istruzione richiesto"));

    const struct { const char* label; const char* data; } livelli[] = {
        {"Tutti i livelli",                      "tutti"},
        {"\xf0\x9f\x9f\xa2 Media inferiore (nessun titolo)",  "media"},
        {"\xf0\x9f\x9f\xa1 Diploma superiore",                 "diploma"},
        {"\xf0\x9f\x9f\xa0 Laurea triennale",                  "laurea_t"},
        {"\xf0\x9f\x94\xb4 Laurea magistrale / Master",        "laurea_m"},
    };
    for (const auto& l : livelli)
        m_filtroLivello->addItem(l.label, QString(l.data));
    m_filtroLivello->setCurrentIndex(2);  // default: Diploma
    livelloL->addWidget(m_filtroLivello, 1);

    auto* filtriBtn = new QPushButton("\xf0\x9f\x94\x84", m_filtriRow);
    filtriBtn->setObjectName("actionBtn");
    filtriBtn->setFixedWidth(dpiScale(28));
    filtriBtn->setToolTip(tr("Aggiorna filtri"));
    connect(filtriBtn, &QPushButton::clicked, this, &LavoroPage::applicaFiltri);
    connect(m_filtroTipo,    QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LavoroPage::applicaFiltri);
    connect(m_filtroLivello, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LavoroPage::applicaFiltri);
    livelloL->addWidget(filtriBtn);
    filtriV->addWidget(livelloRow);

    m_analizzaUrlBtn = new QPushButton(tr("\xf0\x9f\x94\x97 Analizza URL"), m_filtriRow);
    m_analizzaUrlBtn->setObjectName("actionBtn");
    m_analizzaUrlBtn->setToolTip(tr("Analizza uno o più URL di annunci di lavoro"));
    m_analizzaUrlBtn->setAccessibleName(tr("Analizza annuncio di lavoro tramite URL"));
    m_analizzaCvBtn = new QPushButton(tr("\xf0\x9f\xa4\x96 Analizza CV"), m_filtriRow);
    m_analizzaCvBtn->setObjectName("actionBtn");
    m_analizzaCvBtn->setToolTip(tr("Chiedi all'AI di analizzare il tuo CV con una domanda predefinita o personalizzata"));
    m_analizzaCvBtn->setAccessibleName(tr("Analizza curriculum vitae con AI"));
    m_stopAiBtn = new QPushButton("\xe2\x8f\xb9", m_filtriRow);
    m_stopAiBtn->setObjectName("actionBtn");
    m_stopAiBtn->setProperty("danger", true);
    m_stopAiBtn->setFixedWidth(dpiScale(28));
    m_stopAiBtn->setEnabled(false);
    m_stopAiBtn->setToolTip(tr("Interrompi elaborazione AI"));
    m_stopAiBtn->setAccessibleName(tr("Interrompi elaborazione AI"));
    auto* aiRow = new QWidget(m_filtriRow);
    auto* aiRowL = new QHBoxLayout(aiRow);
    aiRowL->setContentsMargins(0,0,0,0); aiRowL->setSpacing(10);
    aiRowL->addWidget(m_analizzaUrlBtn);
    aiRowL->addWidget(m_analizzaCvBtn);
    aiRowL->addWidget(m_stopAiBtn);
    aiRowL->addStretch(1);
    filtriV->addWidget(aiRow);
    lay->addWidget(m_filtriRow);

    connect(m_toggleBtn, &QPushButton::clicked,
            this, &LavoroPage::onToggleBtnClicked);

    /* ── Splitter: (lista+tracker) | output AI ── */
    auto* splitter = new QSplitter(Qt::Vertical, this);

    /* ── Riquadro superiore: lista offerte sopra + tracker sotto ──
       FIX layout: era Qt::Horizontal (lista sx | tracker dx), ma nella
       colonna stretta dell'Assistente ognuno riceveva ~250px — la tabella
       tracker a 5 colonne risultava illeggibile e i widget si
       accavallavano. Impilati in verticale. */
    auto* topSplitter = new QSplitter(Qt::Vertical, splitter);

    /* ──── SINISTRA: lista offerte ──── */
    auto* listaPane = new QWidget(topSplitter);
    auto* listaPaneLay = new QVBoxLayout(listaPane);
    listaPaneLay->setContentsMargins(0,0,0,0); listaPaneLay->setSpacing(4);

    m_offerteLista = new QListWidget(listaPane);
    m_offerteLista->setObjectName("offerteList");
    m_offerteLista->setWordWrap(true);
    m_offerteLista->setAlternatingRowColors(true);
    listaPaneLay->addWidget(m_offerteLista, 1);

    /* ── Pannello link dinamici (offerta selezionata) ── */
    m_linksLbl = new QLabel(listaPane);
    m_linksLbl->setObjectName("hintLabel");
    m_linksLbl->setOpenExternalLinks(true);
    m_linksLbl->setTextFormat(Qt::RichText);
    m_linksLbl->setWordWrap(true);
    m_linksLbl->setText(tr("<i>Seleziona un'offerta per vedere i link</i>"));
    listaPaneLay->addWidget(m_linksLbl);

    /* FIX layout: 5 pulsanti + etichetta su un'unica riga non ci stanno
       nella colonna stretta dell'Assistente — 2 righe (genera / copia)
       + etichetta a capo sotto. */
    auto* azioniRow = new QWidget(listaPane);
    auto* azioniV   = new QVBoxLayout(azioniRow);
    azioniV->setContentsMargins(0,4,0,0); azioniV->setSpacing(4);
    auto* genRow = new QWidget(azioniRow);
    auto* azioniL = new QHBoxLayout(genRow);
    azioniL->setContentsMargins(0,0,0,0); azioniL->setSpacing(8);
    auto* copiaRow = new QWidget(azioniRow);
    auto* copiaL = new QHBoxLayout(copiaRow);
    copiaL->setContentsMargins(0,0,0,0); copiaL->setSpacing(8);

    auto* genBtn   = new QPushButton(tr("\xf0\x9f\xa4\x96 Lettera via AI"), azioniRow);
    genBtn->setObjectName("actionBtn");
    genBtn->setToolTip(tr("Genera una lettera di candidatura via email per l'offerta selezionata"));
    auto* genCoverBtn = new QPushButton(tr("\xf0\x9f\x93\x84 Cover letter via AI"), azioniRow);
    genCoverBtn->setObjectName("actionBtn");
    genCoverBtn->setToolTip(tr("Genera una cover letter professionale da allegare alla candidatura"));
    m_emailBtn = new QPushButton(tr("\xe2\x9c\x89 Copia Email"), azioniRow);
    m_emailBtn->setObjectName("actionBtn");
    m_emailBtn->setEnabled(false);
    m_emailBtn->setToolTip(tr("Disponibile solo per offerte con email diretta"));
    m_copiaBtn = new QPushButton(tr("\xf0\x9f\x93\x8b Copia Lettera"), azioniRow);
    m_copiaBtn->setObjectName("actionBtn");
    m_copiaBtn->setEnabled(false);
    m_copiaBtn->setToolTip(tr("Copia la lettera email generata negli appunti"));
    m_copiaCoverBtn = new QPushButton(tr("\xf0\x9f\x93\x8b Copia Cover"), azioniRow);
    m_copiaCoverBtn->setObjectName("actionBtn");
    m_copiaCoverBtn->setEnabled(false);
    m_copiaCoverBtn->setToolTip(tr("Copia la cover letter negli appunti"));
    m_selLbl = new QLabel(tr("Seleziona un'offerta (doppio clic = genera subito)"), azioniRow);
    m_selLbl->setObjectName("pageSubtitle");
    m_selLbl->setWordWrap(true);
    azioniL->addWidget(genBtn); azioniL->addWidget(genCoverBtn); azioniL->addStretch(1);
    copiaL->addWidget(m_emailBtn); copiaL->addWidget(m_copiaBtn); copiaL->addWidget(m_copiaCoverBtn);
    copiaL->addStretch(1);
    azioniV->addWidget(genRow);
    azioniV->addWidget(copiaRow);
    azioniV->addWidget(m_selLbl);
    listaPaneLay->addWidget(azioniRow);
    topSplitter->addWidget(listaPane);

    /* ──── DESTRA: tracker candidature + calcolatore ──── */
    auto* trackerPane = new QWidget(topSplitter);
    auto* trackerLay  = new QVBoxLayout(trackerPane);
    trackerLay->setContentsMargins(4,0,0,0); trackerLay->setSpacing(6);

    /* Intestazione tracker */
    auto* trkHdrRow = new QWidget(trackerPane);
    auto* trkHdrL   = new QHBoxLayout(trkHdrRow);
    trkHdrL->setContentsMargins(0,0,0,0); trkHdrL->setSpacing(6);
    auto* trkTitleLbl = new QLabel(
        tr("\xf0\x9f\x93\x8b  <b>Tracker Candidature</b>"), trkHdrRow);
    trkTitleLbl->setTextFormat(Qt::RichText);
    m_trackerAddBtn = new QPushButton(tr("\xe2\x9e\x95 Aggiungi"), trkHdrRow);
    m_trackerAddBtn->setObjectName("actionBtn");
    m_trackerAddBtn->setAccessibleName(tr("Aggiungi candidatura al tracker"));
    m_trackerDelBtn = new QPushButton(tr("\xf0\x9f\x97\x91 Rimuovi"), trkHdrRow);
    m_trackerDelBtn->setObjectName("actionBtn");
    m_trackerDelBtn->setAccessibleName(tr("Rimuovi candidatura selezionata dal tracker"));
    trkHdrL->addWidget(trkTitleLbl, 1);
    trkHdrL->addWidget(m_trackerAddBtn);
    trkHdrL->addWidget(m_trackerDelBtn);
    trackerLay->addWidget(trkHdrRow);

    /* Tabella candidature */
    m_trackerTable = new QTableWidget(0, 5, trackerPane);
    m_trackerTable->setHorizontalHeaderLabels({
        tr("Azienda"), tr("Ruolo"), tr("Data invio"), tr("Stato"), tr("Note")
    });
    m_trackerTable->horizontalHeader()->setStretchLastSection(true);
    m_trackerTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_trackerTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    m_trackerTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_trackerTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_trackerTable->verticalHeader()->setDefaultSectionSize(28);
    m_trackerTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_trackerTable->setEditTriggers(
        QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    m_trackerTable->setAlternatingRowColors(true);
    trackerLay->addWidget(m_trackerTable, 1);

    /* Calcolatore euro/ore */
    auto* calcBox  = new QGroupBox(tr("\xf0\x9f\x92\xb6  Calcolatore Euro / Ore"), trackerPane);
    auto* calcGrid = new QFormLayout(calcBox);
    calcGrid->setSpacing(4);

    m_calcOre     = new QLineEdit("40", calcBox);
    m_calcMensile = new QLineEdit(calcBox);
    m_calcAnnuo   = new QLineEdit(calcBox);
    m_calcOrario  = new QLineEdit(calcBox);
    m_calcOre->setAccessibleName(tr("Ore di lavoro settimanali"));
    m_calcMensile->setAccessibleName(tr("Stipendio lordo mensile in euro"));
    m_calcAnnuo->setAccessibleName(tr("Stipendio lordo annuo in euro"));
    m_calcOrario->setAccessibleName(tr("Paga oraria in euro"));
    m_calcNettoLbl = new QLabel("\xe2\x80\x94", calcBox);
    m_calcNettoLbl->setTextFormat(Qt::RichText);

    m_calcOre->setPlaceholderText(tr("es. 40"));
    m_calcMensile->setPlaceholderText(tr("es. 1500.00"));
    m_calcAnnuo->setPlaceholderText(tr("es. 18000.00"));
    m_calcOrario->setPlaceholderText(tr("es. 10.50"));

    calcGrid->addRow("Ore/settimana:", m_calcOre);
    calcGrid->addRow("Lordo mensile (\xe2\x82\xac):", m_calcMensile);
    calcGrid->addRow("Lordo annuo (\xe2\x82\xac):", m_calcAnnuo);
    calcGrid->addRow("Tariffa oraria (\xe2\x82\xac/h):", m_calcOrario);
    calcGrid->addRow("Netto stimato:", m_calcNettoLbl);

    m_mercatoLbl = new QLabel("", calcBox);
    m_mercatoLbl->setObjectName("hintLabel");
    m_mercatoLbl->setWordWrap(true);
    m_mercatoLbl->setTextFormat(Qt::RichText);
    calcGrid->addRow(m_mercatoLbl);   /* full-width hint: annuncio vs. mercato */

    trackerLay->addWidget(calcBox);

    topSplitter->addWidget(trackerPane);
    topSplitter->setStretchFactor(0, 1);
    topSplitter->setStretchFactor(1, 1);
    splitter->addWidget(topSplitter);

    auto* botPane = new QWidget(splitter);
    auto* botLay  = new QVBoxLayout(botPane);
    botLay->setContentsMargins(0,0,0,0); botLay->setSpacing(6);

    /* ── Email sopra | Cover Letter sotto ──
       FIX layout: era orizzontale — due editor di testo affiancati in
       ~250px l'uno erano illeggibili nella colonna dell'Assistente. */
    auto* colSplit = new QSplitter(Qt::Vertical, botPane);

    /* Colonna sinistra — lettera email */
    auto* emailCol = new QWidget(colSplit);
    auto* emailLay = new QVBoxLayout(emailCol);
    emailLay->setContentsMargins(0,0,2,0); emailLay->setSpacing(2);
    auto* emailHdr = new QLabel(tr("\xe2\x9c\x89\xef\xb8\x8f  Lettera Email"), emailCol);
    emailHdr->setObjectName("pageSubtitle");
    emailLay->addWidget(emailHdr);

    m_lavoroLog = new QTextEdit(emailCol);
    m_lavoroLog->setObjectName("chatLog");
    m_lavoroLog->setReadOnly(true);
    m_lavoroLog->setPlaceholderText(
        "Lettera di candidatura via email.\n"
        "Seleziona un'offerta e clicca \"Genera Lettera con AI\".");
    m_lavoroLog->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_lavoroLog, &QTextEdit::customContextMenuRequested,
            this, &LavoroPage::onLavoroLogContextMenu);
    emailLay->addWidget(m_lavoroLog, 1);
    colSplit->addWidget(emailCol);

    /* Colonna destra — cover letter */
    auto* coverCol = new QWidget(colSplit);
    auto* coverLay = new QVBoxLayout(coverCol);
    coverLay->setContentsMargins(2,0,0,0); coverLay->setSpacing(2);
    auto* coverHdr = new QLabel(tr("\xf0\x9f\x93\x84  Cover Letter"), coverCol);
    coverHdr->setObjectName("pageSubtitle");
    coverLay->addWidget(coverHdr);

    m_coverLog = new QTextEdit(coverCol);
    m_coverLog->setObjectName("chatLog");
    m_coverLog->setReadOnly(true);
    m_coverLog->setPlaceholderText(
        "Cover Letter professionale da allegare alla candidatura.\n"
        "Clicca \"Genera Cover Letter\" dopo aver selezionato un'offerta.");
    m_coverLog->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_coverLog, &QTextEdit::customContextMenuRequested,
            this, &LavoroPage::onCoverLogContextMenu);
    coverLay->addWidget(m_coverLog, 1);
    colSplit->addWidget(coverCol);

    colSplit->setStretchFactor(0, 1);
    colSplit->setStretchFactor(1, 1);
    botLay->addWidget(colSplit, 1);

    m_waitLbl = new QLabel(tr("\xe2\x8f\xb3  AI in elaborazione..."), botPane);
    m_waitLbl->setStyleSheet("color:#E5C400; font-style:italic; padding:2px 0;");
    m_waitLbl->setVisible(false);
    botLay->addWidget(m_waitLbl);
    splitter->addWidget(botPane);

    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    lay->addWidget(splitter, 1);

    /* ── Connessioni ── */
    m_nam = new QNetworkAccessManager(this);

    connect(m_offerteLista, &QListWidget::currentItemChanged,
            this, &LavoroPage::onOfferteItemChanged);
    connect(m_emailBtn,    &QPushButton::clicked, this, &LavoroPage::onEmailBtnClicked);
    connect(m_lavoroLog,   &QTextEdit::textChanged, this, &LavoroPage::onLavoroLogTextChanged);
    connect(m_copiaBtn,    &QPushButton::clicked, this, &LavoroPage::onCopiaLettBtnClicked);
    connect(m_coverLog,    &QTextEdit::textChanged, this, &LavoroPage::onCoverLogTextChanged);
    connect(m_copiaCoverBtn, &QPushButton::clicked, this, &LavoroPage::onCopiaCoverBtnClicked);
    connect(genBtn,        &QPushButton::clicked, this, &LavoroPage::onGenBtnClicked);
    connect(genCoverBtn,   &QPushButton::clicked, this, &LavoroPage::onGenCoverBtnClicked);
    connect(m_offerteLista, &QListWidget::itemDoubleClicked,
            this, &LavoroPage::onOfferteItemDoubleClicked);
    connect(m_stopAiBtn,   &QPushButton::clicked, m_ai, &AiClient::abort);
    connect(m_analizzaUrlBtn, &QPushButton::clicked, this, &LavoroPage::onAnalizzaUrlBtnClicked);
    connect(m_analizzaCvBtn,  &QPushButton::clicked, this, &LavoroPage::onAnalizzaCvBtnClicked);

    /* ── Segnali AI ── */
    connect(m_ai, &AiClient::token,    this, &LavoroPage::onAiToken);
    connect(m_ai, &AiClient::finished, this, &LavoroPage::onAiFinished);
    connect(m_ai, &AiClient::error,    this, &LavoroPage::onAiError);
    connect(m_ai, &AiClient::aborted,  this, &LavoroPage::onAiAborted);

    /* ── Tracker candidature ── */
    connect(m_trackerAddBtn, &QPushButton::clicked,
            this, &LavoroPage::onTrackerAddRow);
    connect(m_trackerDelBtn, &QPushButton::clicked,
            this, &LavoroPage::onTrackerRemoveRow);
    connect(m_trackerTable, &QTableWidget::itemChanged,
            this, &LavoroPage::onTrackerSave);

    /* ── Calcolatore euro/ore ── */
    connect(m_calcOre,     &QLineEdit::textEdited, this, &LavoroPage::onCalcChanged);
    connect(m_calcMensile, &QLineEdit::textEdited, this, &LavoroPage::onCalcChanged);
    connect(m_calcAnnuo,   &QLineEdit::textEdited, this, &LavoroPage::onCalcChanged);
    connect(m_calcOrario,  &QLineEdit::textEdited, this, &LavoroPage::onCalcChanged);

    applicaFiltri();
    loadTracker();

    /* ── La pagina Lavoro È l'Assistente Candidature: browser a sinistra,
       tutto il contenuto storico (CV/Offerte/Tracker/Calcolatore/Cover
       Letter, costruito sopra in candidatureTab) in colonna a destra
       insieme ai controlli di registrazione/riproduzione. Nessuna tab
       esterna separata. ── */
    auto* outerLay = new QVBoxLayout(this);
    outerLay->setContentsMargins(0, 0, 0, 0);
#ifdef HAVE_JOB_ASSISTANT
    outerLay->addWidget(buildAssistenteTab(this, candidatureTab));
#else
    outerLay->addWidget(candidatureTab);
#endif
}

/* ══════════════════════════════════════════════════════════════
   HELPER — Tracker: percorso file JSON
   ══════════════════════════════════════════════════════════════ */
QString LavoroPage::trackerPath() const {
    return P::root() + "/candidature.json";
}

QComboBox* LavoroPage::makeStatoCombo(QWidget* parent) {
    auto* c = new QComboBox(parent);
    c->addItems({
        "\xe2\x8f\xb3 In attesa",
        "\xf0\x9f\x93\xa9 Risposto",
        "\xf0\x9f\x93\x85 Colloquio",
        "\xe2\x9d\x8c Rifiutato",
        "\xe2\x9c\x85 Assunto"
    });
    connect(c, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LavoroPage::onTrackerSave);
    return c;
}

/* ══════════════════════════════════════════════════════════════
   SLOT — Tracker candidature: Aggiungi riga
   ══════════════════════════════════════════════════════════════ */
void LavoroPage::onTrackerAddRow() {
    if (!m_trackerTable) return;
    const int row = m_trackerTable->rowCount();
    m_trackerTable->blockSignals(true);
    m_trackerTable->insertRow(row);
    m_trackerTable->setItem(row, 0, new QTableWidgetItem(""));
    m_trackerTable->setItem(row, 1, new QTableWidgetItem(""));
    m_trackerTable->setItem(row, 2, new QTableWidgetItem(
        QDate::currentDate().toString("yyyy-MM-dd")));
    m_trackerTable->setCellWidget(row, 3, makeStatoCombo(m_trackerTable));
    m_trackerTable->setItem(row, 4, new QTableWidgetItem(""));
    m_trackerTable->blockSignals(false);
    m_trackerTable->scrollToBottom();
    m_trackerTable->editItem(m_trackerTable->item(row, 0));
    onTrackerSave();
}

/* ══════════════════════════════════════════════════════════════
   SLOT — Tracker candidature: Rimuovi riga selezionata
   ══════════════════════════════════════════════════════════════ */
void LavoroPage::onTrackerRemoveRow() {
    if (!m_trackerTable) return;
    const int row = m_trackerTable->currentRow();
    if (row < 0) return;
    m_trackerTable->removeRow(row);
    onTrackerSave();
}

/* ══════════════════════════════════════════════════════════════
   SLOT — Tracker candidature: salva su JSON
   ══════════════════════════════════════════════════════════════ */
void LavoroPage::onTrackerSave() {
    if (!m_trackerTable) return;
    QJsonArray arr;
    for (int r = 0; r < m_trackerTable->rowCount(); ++r) {
        auto cell = [&](int c) -> QString {
            auto* it = m_trackerTable->item(r, c);
            return it ? it->text() : QString();
        };
        auto* statoW = qobject_cast<QComboBox*>(m_trackerTable->cellWidget(r, 3));
        QJsonObject obj;
        obj["azienda"] = cell(0);
        obj["ruolo"]   = cell(1);
        obj["data"]    = cell(2);
        obj["stato"]   = statoW ? statoW->currentText() : cell(3);
        obj["note"]    = cell(4);
        arr.append(obj);
    }
    QFile f(trackerPath());
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(arr).toJson());
}

/* ══════════════════════════════════════════════════════════════
   HELPER — carica tracker dal JSON all'avvio
   ══════════════════════════════════════════════════════════════ */
void LavoroPage::loadTracker() {
    if (!m_trackerTable) return;
    QFile f(trackerPath());
    if (!f.open(QIODevice::ReadOnly)) return;
    const QJsonArray arr = QJsonDocument::fromJson(f.readAll()).array();
    m_trackerTable->blockSignals(true);
    m_trackerTable->setRowCount(0);
    for (const auto& v : arr) {
        const QJsonObject obj = v.toObject();
        const int row = m_trackerTable->rowCount();
        m_trackerTable->insertRow(row);
        m_trackerTable->setItem(row, 0, new QTableWidgetItem(obj["azienda"].toString()));
        m_trackerTable->setItem(row, 1, new QTableWidgetItem(obj["ruolo"].toString()));
        m_trackerTable->setItem(row, 2, new QTableWidgetItem(obj["data"].toString()));
        auto* combo = makeStatoCombo(m_trackerTable);
        const int idx = combo->findText(obj["stato"].toString());
        if (idx >= 0) combo->setCurrentIndex(idx);
        m_trackerTable->setCellWidget(row, 3, combo);
        m_trackerTable->setItem(row, 4, new QTableWidgetItem(obj["note"].toString()));
    }
    m_trackerTable->blockSignals(false);
}

/* ══════════════════════════════════════════════════════════════
   SLOT — Calcolatore Euro / Ore
   ══════════════════════════════════════════════════════════════ */
void LavoroPage::onCalcChanged() {
    if (m_calcBusy) return;
    if (!m_calcMensile || !m_calcAnnuo || !m_calcOrario || !m_calcOre) return;
    m_calcBusy = true;

    const double ore     = m_calcOre->text().replace(',', '.').toDouble();
    const double oreAnno = ore > 0 ? ore * 52.0 : 2080.0;

    auto* src = qobject_cast<QLineEdit*>(sender());
    double mensile = 0.0, annuo = 0.0, orario = 0.0;

    if (src == m_calcMensile) {
        mensile = m_calcMensile->text().replace(',', '.').toDouble();
        annuo   = mensile * 12.0;
        orario  = oreAnno > 0 ? annuo / oreAnno : 0.0;
    } else if (src == m_calcAnnuo) {
        annuo   = m_calcAnnuo->text().replace(',', '.').toDouble();
        mensile = annuo / 12.0;
        orario  = oreAnno > 0 ? annuo / oreAnno : 0.0;
    } else if (src == m_calcOrario) {
        orario  = m_calcOrario->text().replace(',', '.').toDouble();
        annuo   = orario * oreAnno;
        mensile = annuo / 12.0;
    } else {
        /* Ore cambiate: ricalcola da mensile se disponibile */
        mensile = m_calcMensile->text().replace(',', '.').toDouble();
        if (mensile <= 0)
            mensile = m_calcAnnuo->text().replace(',', '.').toDouble() / 12.0;
        annuo  = mensile * 12.0;
        orario = oreAnno > 0 ? annuo / oreAnno : 0.0;
    }

    if (src != m_calcMensile && mensile > 0)
        m_calcMensile->setText(QString::number(mensile, 'f', 2));
    if (src != m_calcAnnuo && annuo > 0)
        m_calcAnnuo->setText(QString::number(annuo, 'f', 2));
    if (src != m_calcOrario && orario > 0)
        m_calcOrario->setText(QString::number(orario, 'f', 2));

    /* Netto stimato ≈ lordo × 0.72 (IRPEF media dipendente Italia) */
    if (mensile > 0 && m_calcNettoLbl) {
        const double netto = mensile * 0.72;
        m_calcNettoLbl->setText(
            QString("\xe2\x89\x88 <b>%1 \xe2\x82\xac/mese</b>"
                    "  <span style='color:gray;font-size:10px;'>"
                    "stima -28%% IRPEF</span>")
            .arg(netto, 0, 'f', 2));
    }

    m_calcBusy = false;
}

/* ══════════════════════════════════════════════════════════════
   SLOT — CV
   ══════════════════════════════════════════════════════════════ */
void LavoroPage::onSfogliaBtnClicked() {
    const QString path = QFileDialog::getOpenFileName(
        this, "Seleziona Curriculum Vitae",
        QDir::homePath(), "PDF (*.pdf);;Testo (*.txt);;Tutti (*)");
    if (!path.isEmpty()) {
        m_cvPath->setText(path);
        caricaCV(path);
    }
}

/* ══════════════════════════════════════════════════════════════
   SLOT — Modello
   ══════════════════════════════════════════════════════════════ */
void LavoroPage::onModelloIndexChanged(int) {
    if (!m_cmbModello) return;
    const QString raw = m_cmbModello->currentData(Qt::UserRole).toString();
    const QString name = raw.isEmpty() ? m_cmbModello->currentText() : raw;
    if (m_modelloLbl) m_modelloLbl->setText(tr("\xf0\x9f\xa4\x96 ") + name);
    if (!name.isEmpty() && !name.startsWith("\xf0\x9f\x94\x84"))
        m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), name);
}

/* ══════════════════════════════════════════════════════════════
   SLOT — Toggle
   ══════════════════════════════════════════════════════════════ */
void LavoroPage::onToggleBtnClicked() {
    if (!m_cvBox || !m_llmBox || !m_filtriRow || !m_toggleBtn) return;
    const bool nowVisible = !m_cvBox->isVisible();
    m_cvBox->setVisible(nowVisible);
    m_llmBox->setVisible(nowVisible);
    m_filtriRow->setVisible(nowVisible);
    m_toggleBtn->setText(nowVisible ? "\xe2\x96\xb2" : "\xe2\x96\xbc");
    m_toggleBtn->setToolTip(nowVisible ? "Comprimi" : "Espandi");
}

/* ══════════════════════════════════════════════════════════════
   SLOT — Lista offerte
   ══════════════════════════════════════════════════════════════ */
void LavoroPage::onOfferteItemChanged(QListWidgetItem* cur, QListWidgetItem*) {
    if (!cur) {
        if (m_selLbl) m_selLbl->setText(tr("Seleziona un'offerta dalla lista"));
        if (m_emailBtn) m_emailBtn->setEnabled(false);
        if (m_linksLbl) m_linksLbl->setText(tr("<i>Seleziona un'offerta per vedere i link</i>"));
        return;
    }
    const auto o = cur->data(Qt::UserRole).value<Offerta>();
    if (m_selLbl) m_selLbl->setText(o.azienda + " \xe2\x80\x94 " + o.ruolo);
    if (m_emailBtn) {
        m_emailBtn->setEnabled(!o.email.isEmpty());
        if (!o.email.isEmpty()) m_emailBtn->setToolTip(tr("\xf0\x9f\x93\x8b Copia: ") + o.email);
    }
    if (!m_linksLbl) return;

    const QString sd   = QUrl::toPercentEncoding(o.sede);
    const QString azRl = QUrl::toPercentEncoding(o.azienda + " " + o.ruolo + " lavoro");
    const QString azLav = QUrl::toPercentEncoding(o.azienda + " lavora con noi careers");

    const QString urlAnnuncio = "https://www.google.com/search?q=" + azRl;
    const QString urlMaps     = "https://www.google.com/maps/search/" + sd;

    const bool hasDirectUrl = o.requisiti.startsWith("http");
    const QString urlLavora = hasDirectUrl
        ? o.requisiti
        : "https://www.google.com/search?q=" + azLav;

    m_linksLbl->setText(
        "\xf0\x9f\x94\x97 <a href='" + urlAnnuncio + "'>Cerca annuncio</a>"
        " &nbsp;|&nbsp;"
        "\xf0\x9f\x8f\xa2 <a href='" + urlLavora   + "'>Lavora con noi</a>"
        " &nbsp;|&nbsp;"
        "\xf0\x9f\x97\xba <a href='" + urlMaps     + "'>" + o.sede + " \xe2\x80\x94 Google Maps</a>");

    precompilaStipendioDaOfferta(o);
}

void LavoroPage::onEmailBtnClicked() {
    auto* cur = m_offerteLista ? m_offerteLista->currentItem() : nullptr;
    if (!cur) return;
    const auto o = cur->data(Qt::UserRole).value<Offerta>();
    if (!o.email.isEmpty()) QGuiApplication::clipboard()->setText(o.email);
}

void LavoroPage::onLavoroLogTextChanged() {
    if (m_copiaBtn && m_lavoroLog)
        m_copiaBtn->setEnabled(!m_lavoroLog->toPlainText().trimmed().isEmpty());
}

void LavoroPage::onCopiaLettBtnClicked() {
    if (m_lavoroLog)
        QGuiApplication::clipboard()->setText(m_lavoroLog->toPlainText());
}

void LavoroPage::onCoverLogTextChanged() {
    if (m_copiaCoverBtn && m_coverLog)
        m_copiaCoverBtn->setEnabled(!m_coverLog->toPlainText().trimmed().isEmpty());
}

void LavoroPage::onCopiaCoverBtnClicked() {
    if (m_coverLog)
        QGuiApplication::clipboard()->setText(m_coverLog->toPlainText());
}

void LavoroPage::onGenBtnClicked()     { genLettera(); }
void LavoroPage::onGenCoverBtnClicked(){ genCover(); }
void LavoroPage::onOfferteItemDoubleClicked(QListWidgetItem*) { genLettera(); }

/* ══════════════════════════════════════════════════════════════
   SLOT — Context menu log
   ══════════════════════════════════════════════════════════════ */
void LavoroPage::onLavoroLogContextMenu(const QPoint& pos) {
    if (!m_lavoroLog) return;
    const QString sel = m_lavoroLog->textCursor().selectedText();
    const bool hasSel = !sel.isEmpty();
    QMenu menu(m_lavoroLog);
    QAction* actCopy = menu.addAction(tr("\xf0\x9f\x97\x82  Copia ") + QString(hasSel ? "selezione" : "tutto"));
    QAction* actRead = menu.addAction(tr("\xf0\x9f\x8e\x99  Leggi ") + QString(hasSel ? "selezione" : "tutto"));
    QAction* chosen  = menu.exec(m_lavoroLog->mapToGlobal(pos));
    const QString txt = hasSel ? sel : m_lavoroLog->toPlainText();
    if (chosen == actCopy) QGuiApplication::clipboard()->setText(txt);
    else if (chosen == actRead) {
        QStringList words = txt.split(' ', Qt::SkipEmptyParts);
        if (words.size() > 400) words = words.mid(words.size() - 400);
        QProcess::startDetached("espeak-ng", {"-v", "it+f3", "--punct=none", words.join(" ")});
    }
}

void LavoroPage::onCoverLogContextMenu(const QPoint& pos) {
    if (!m_coverLog) return;
    const QString sel = m_coverLog->textCursor().selectedText();
    const bool hasSel = !sel.isEmpty();
    QMenu menu(m_coverLog);
    QAction* actCopy = menu.addAction(tr("\xf0\x9f\x97\x82  Copia ") + QString(hasSel ? "selezione" : "tutto"));
    QAction* chosen  = menu.exec(m_coverLog->mapToGlobal(pos));
    if (chosen == actCopy)
        QGuiApplication::clipboard()->setText(hasSel ? sel : m_coverLog->toPlainText());
}

/* ══════════════════════════════════════════════════════════════
   genLettera — genera lettera email AI
   ══════════════════════════════════════════════════════════════ */
void LavoroPage::genLettera() {
    auto* cur = m_offerteLista ? m_offerteLista->currentItem() : nullptr;
    if (!cur) {
        if (m_lavoroLog) m_lavoroLog->append("\xe2\x9a\xa0  Seleziona prima un'offerta.");
        return;
    }
    const auto o = cur->data(Qt::UserRole).value<Offerta>();
    const QString cvInfo  = m_cvText.isEmpty() ? cvFallback() : m_cvText.left(3500);
    const QString modello = m_cmbModello
        ? ModelComboHelper::currentModel(m_cmbModello)
        : m_ai->model();

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
    ).arg(cvInfo, o.azienda, o.ruolo, o.sede, o.requisiti, emailHint, socraticoBase());

    if (m_lavoroLog) {
        m_lavoroLog->clear();
        m_lavoroLog->append(QString(
            "\xf0\x9f\x92\xbc  [CERCA LAVORO \xe2\x86\x92 Genera Lettera]\n"
            "\xf0\x9f\xa4\x96  Modello: %1\n"
            "\xf0\x9f\x8f\xa2  Azienda: %2 \xe2\x80\x94 %3\n"
            "\xf0\x9f\x93\x8d  Sede: %4\n").arg(modello, o.azienda, o.ruolo, o.sede));
        if (!o.email.isEmpty())
            m_lavoroLog->append(QString("\xe2\x9c\x89  Email destinatario: %1\n").arg(o.email));
        m_lavoroLog->append("\n\xf0\x9f\x93\x9d  Lettera:\n");
    }

    if (m_analizzaUrlBtn) m_analizzaUrlBtn->setEnabled(false);
    if (m_analizzaCvBtn)  m_analizzaCvBtn->setEnabled(false);
    if (m_stopAiBtn)      m_stopAiBtn->setEnabled(true);
    if (m_waitLbl)        m_waitLbl->setVisible(true);
    m_myReqId = m_ai->chat(P::prependKnowledge(sys),
        QString("Genera la lettera di candidatura per il ruolo di %1 presso %2.")
            .arg(o.ruolo, o.azienda));
}

/* ══════════════════════════════════════════════════════════════
   genCover — genera cover letter AI
   ══════════════════════════════════════════════════════════════ */
void LavoroPage::genCover() {
    auto* cur = m_offerteLista ? m_offerteLista->currentItem() : nullptr;
    if (!cur) {
        if (m_coverLog) m_coverLog->append("\xe2\x9a\xa0  Seleziona prima un'offerta.");
        return;
    }
    const auto o = cur->data(Qt::UserRole).value<Offerta>();
    const QString cvInfo  = m_cvText.isEmpty() ? cvFallback() : m_cvText.left(3500);
    const QString modello = m_cmbModello
        ? ModelComboHelper::currentModel(m_cmbModello)
        : m_ai->model();
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
    ).arg(cvInfo, o.azienda, o.ruolo, o.sede, o.requisiti, socraticoBase());

    if (m_coverLog) {
        m_coverLog->clear();
        m_coverLog->append(QString(
            "\xf0\x9f\x93\x84  [COVER LETTER]\n"
            "\xf0\x9f\xa4\x96  Modello: %1\n"
            "\xf0\x9f\x8f\xa2  %2 \xe2\x80\x94 %3\n\n").arg(modello, o.azienda, o.ruolo));
    }

    if (m_analizzaUrlBtn) m_analizzaUrlBtn->setEnabled(false);
    if (m_analizzaCvBtn)  m_analizzaCvBtn->setEnabled(false);
    if (m_stopAiBtn)      m_stopAiBtn->setEnabled(true);
    if (m_waitLbl)        m_waitLbl->setVisible(true);
    m_myCoverReqId = m_ai->chat(P::prependKnowledge(sysCover),
        QString("Scrivi la cover letter per %1 presso %2.").arg(o.ruolo, o.azienda));
}

/* ══════════════════════════════════════════════════════════════
   sendFn — invia analisi CV all'AI
   ══════════════════════════════════════════════════════════════ */
void LavoroPage::sendFn(const QString& msg) {
    if (msg.isEmpty()) return;
    const QString cvInfo  = m_cvText.isEmpty() ? cvFallback().left(1500) : m_cvText.left(2000);
    const QString modello = m_cmbModello
        ? ModelComboHelper::currentModel(m_cmbModello)
        : m_ai->model();
    if (!modello.isEmpty() && !modello.startsWith("\xf0\x9f\x94\x84"))
        m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), modello);

    const QString sys = QString(
        "Sei un career coach italiano esperto in ricerca del lavoro, CV e colloqui.\n"
        "Il candidato ha questo profilo:\n%1\n"
        "Fornisci consigli concreti e pratici. Rispondi SEMPRE in italiano.%2"
    ).arg(cvInfo, socraticoBase());

    if (m_lavoroLog) {
        m_lavoroLog->append(QString("\n\xf0\x9f\xa4\x96 [ANALISI CV \xe2\x86\x92 %1]\n")
                            .arg(msg.left(50)));
        m_lavoroLog->append("\xf0\x9f\xa4\x96  AI: ");
    }
    if (m_analizzaUrlBtn) m_analizzaUrlBtn->setEnabled(false);
    if (m_analizzaCvBtn)  m_analizzaCvBtn->setEnabled(false);
    if (m_stopAiBtn)      m_stopAiBtn->setEnabled(true);
    if (m_waitLbl)        m_waitLbl->setVisible(true);
    m_myReqId = m_ai->chat(P::prependKnowledge(sys), msg);
}

/* ══════════════════════════════════════════════════════════════
   aiDone — ripristina UI dopo elaborazione AI
   ══════════════════════════════════════════════════════════════ */
void LavoroPage::aiDone() {
    if (m_analizzaUrlBtn) m_analizzaUrlBtn->setEnabled(true);
    if (m_analizzaCvBtn)  m_analizzaCvBtn->setEnabled(true);
    if (m_stopAiBtn)      m_stopAiBtn->setEnabled(false);
    if (m_waitLbl)        m_waitLbl->setVisible(false);
}

/* ══════════════════════════════════════════════════════════════
   analizzaUrls — scarica + analizza URL di annunci
   ══════════════════════════════════════════════════════════════ */
void LavoroPage::analizzaUrls(const QStringList& urlList) {
    if (urlList.isEmpty()) return;

    if (m_analizzaUrlBtn) m_analizzaUrlBtn->setEnabled(false);
    if (m_analizzaCvBtn)  m_analizzaCvBtn->setEnabled(false);
    if (m_stopAiBtn)      m_stopAiBtn->setEnabled(true);
    if (m_waitLbl)        m_waitLbl->setVisible(true);

    if (m_lavoroLog) {
        m_lavoroLog->clear();
        m_lavoroLog->append(QString(
            "\xf0\x9f\x94\x97 [ANALISI URL] — %1 link\n").arg(urlList.size()));
    }

    auto pending  = std::make_shared<int>(urlList.size());
    auto combined = std::make_shared<QString>();

    for (const QString& rawUrl : urlList) {
        if (!rawUrl.startsWith("http://") && !rawUrl.startsWith("https://")) {
            if (m_lavoroLog)
                m_lavoroLog->append(QString("\xe2\x9a\xa0 URL non valido: %1").arg(rawUrl));
            if (--(*pending) == 0) aiDone();
            continue;
        }
        if (m_lavoroLog)
            m_lavoroLog->append(QString("\xf0\x9f\x8c\x90 Scarico: %1").arg(rawUrl));

        QNetworkRequest req{QUrl(rawUrl)};
        req.setHeader(QNetworkRequest::UserAgentHeader,
                      "Mozilla/5.0 (X11; Linux x86_64) Prismalux/2.8");
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
        req.setTransferTimeout(15000);
        auto* reply = m_nam->get(req);

        /* one-shot: context object = reply → distrutto insieme al reply */
        auto* ctx = new QObject(this);
        connect(reply, &QNetworkReply::finished, ctx,
                [guard=QPointer<LavoroPage>(this), ctx, reply, rawUrl, pending, combined, urlList]{
            if (!guard) return;
            ctx->deleteLater();
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
                if (guard->m_lavoroLog)
                    guard->m_lavoroLog->append(
                        QString("\xe2\x9d\x8c Errore: %1").arg(reply->errorString()));
            }

            if (--(*pending) > 0) return;

            /* Tutti i download completati */
            if (combined->trimmed().isEmpty()) {
                if (guard->m_lavoroLog)
                    guard->m_lavoroLog->append(
                        "\xe2\x9a\xa0 Nessun contenuto leggibile (potrebbero richiedere JavaScript/login).");
                guard->aiDone();
                return;
            }

            if (guard->m_lavoroLog)
                guard->m_lavoroLog->append("\xe2\x9c\x85 Download completato \xe2\x86\x92 Analisi AI...\n");
            const QString cvInfo  = guard->m_cvText.isEmpty() ? guard->cvFallback().left(2000) : guard->m_cvText.left(2000);
            const QString modello = guard->m_cmbModello
                ? ModelComboHelper::currentModel(guard->m_cmbModello)
                : guard->m_ai->model();
            if (!modello.isEmpty() && !modello.startsWith("\xf0\x9f\x94\x84"))
                guard->m_ai->setBackend(guard->m_ai->backend(), guard->m_ai->host(), guard->m_ai->port(), modello);

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
                    .arg(cvInfo, *combined, guard->socraticoBase())
                : QString(
                    "Sei un esperto di carriera. Analizza i seguenti annunci in italiano.\n\n"
                    "=== PROFILO CANDIDATO ===\n%1\n\n"
                    "=== ANNUNCI ===\n%2\n\n"
                    "Per ogni annuncio: ruolo/azienda, requisiti chiave, "
                    "compatibilit\xc3\xa0 col profilo, consiglio s\xc3\xac/no. Max 500 parole.%3")
                    .arg(cvInfo, *combined, guard->socraticoBase());

            guard->m_myReqId = guard->m_ai->chat(P::prependKnowledge(sys),
                nUrl == 1
                ? "Analizza questo annuncio e valuta la compatibilit\xc3\xa0 col mio profilo."
                : "Analizza questi annunci e valuta quale si adatta meglio al mio profilo.");
        });

        /* Abort se utente preme Stop */
        auto* abortCtx = new QObject(this);
        connect(m_stopAiBtn, &QPushButton::clicked, abortCtx, [abortCtx, reply]{
            abortCtx->deleteLater();
            reply->abort();
        });
    }
}

/* ══════════════════════════════════════════════════════════════
   SLOT — Analizza URL
   ══════════════════════════════════════════════════════════════ */
void LavoroPage::onAnalizzaUrlBtnClicked() {
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(tr("Analizza URL annunci"));
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->resize(540, 200);
    auto* dLay = new QVBoxLayout(dlg);
    dLay->addWidget(new QLabel(
        tr("Incolla uno o pi\xc3\xb9 URL di annunci di lavoro (uno per riga):"), dlg));
    auto* urlEdit = new QTextEdit(dlg);
    urlEdit->setPlaceholderText(
        "https://www.linkedin.com/jobs/...\n"
        "https://it.indeed.com/...\n"
        "https://www.infojobs.it/...");
    dLay->addWidget(urlEdit, 1);
    auto* btnRow = new QWidget(dlg);
    auto* btnL   = new QHBoxLayout(btnRow);
    btnL->setContentsMargins(0,4,0,0);
    auto* okBtn  = new QPushButton(tr("Analizza"), btnRow);
    okBtn->setObjectName("actionBtn");
    auto* noBtn  = new QPushButton(tr("Annulla"), btnRow);
    noBtn->setObjectName("actionBtn");
    btnL->addStretch(1); btnL->addWidget(okBtn); btnL->addWidget(noBtn);
    dLay->addWidget(btnRow);
    connect(noBtn, &QPushButton::clicked, dlg, &QDialog::reject);
    /* one-shot — context = dlg */
    auto* ctx = new QObject(dlg);
    connect(okBtn, &QPushButton::clicked, ctx, [this, ctx, dlg, urlEdit]{
        ctx->deleteLater();
        QStringList urls;
        for (const QString& line : urlEdit->toPlainText().split('\n', Qt::SkipEmptyParts))
            if (!line.trimmed().isEmpty()) urls << line.trimmed();
        if (!urls.isEmpty()) { dlg->accept(); analizzaUrls(urls); }
    });
    dlg->exec();
}

/* ══════════════════════════════════════════════════════════════
   SLOT — Analizza CV
   ══════════════════════════════════════════════════════════════ */
void LavoroPage::onAnalizzaCvBtnClicked() {
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(tr("Analizza CV"));
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->resize(530, 330);
    auto* dLay = new QVBoxLayout(dlg);
    dLay->addWidget(new QLabel(
        tr("Seleziona un'analisi predefinita (doppio clic = avvia subito):"), dlg));
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
    dLay->addWidget(new QLabel(tr("Oppure scrivi una domanda personalizzata:"), dlg));
    auto* promptEdit = new QLineEdit(dlg);
    promptEdit->setObjectName("chatInput");
    promptEdit->setPlaceholderText(
        tr("Es: 'Cosa manca per essere assunto come programmatore junior?'"));
    dLay->addWidget(promptEdit);
    /* itemClicked → popola promptEdit — context = dlg, cattura solo figli di dlg */
    connect(listW, &QListWidget::itemClicked, dlg,
            [promptEdit](QListWidgetItem* item){ promptEdit->setText(item->text()); });
    auto* btnRow = new QWidget(dlg);
    auto* btnL   = new QHBoxLayout(btnRow);
    btnL->setContentsMargins(0,4,0,0);
    auto* okBtn  = new QPushButton(tr("Analizza"), btnRow);
    okBtn->setObjectName("actionBtn");
    auto* noBtn  = new QPushButton(tr("Annulla"), btnRow);
    noBtn->setObjectName("actionBtn");
    btnL->addStretch(1); btnL->addWidget(okBtn); btnL->addWidget(noBtn);
    dLay->addWidget(btnRow);

    connect(noBtn, &QPushButton::clicked, dlg, &QDialog::reject);

    auto* ctx = new QObject(dlg);
    auto doAnalyze = [this, ctx, dlg, promptEdit]{
        ctx->deleteLater();
        const QString msg = promptEdit->text().trimmed();
        if (!msg.isEmpty()) { dlg->accept(); sendFn(msg); }
    };
    connect(okBtn,      &QPushButton::clicked,     ctx, doAnalyze);
    connect(promptEdit, &QLineEdit::returnPressed,  ctx, doAnalyze);
    connect(listW, &QListWidget::itemDoubleClicked, ctx, [this, ctx, dlg, promptEdit](QListWidgetItem* item){
        ctx->deleteLater();
        promptEdit->setText(item->text()); dlg->accept(); sendFn(item->text());
    });
    dlg->exec();
}

/* ══════════════════════════════════════════════════════════════
   SLOT — Segnali AI
   ══════════════════════════════════════════════════════════════ */
void LavoroPage::onAiToken(const QString& t) {
    const quint64 rid = m_ai->currentReqId();
    if (rid == m_myReqId && m_lavoroLog) {
        QTextCursor c(m_lavoroLog->document()); c.movePosition(QTextCursor::End);
        c.insertText(t); m_lavoroLog->ensureCursorVisible();
    } else if (rid == m_myCoverReqId && m_coverLog) {
        QTextCursor c(m_coverLog->document()); c.movePosition(QTextCursor::End);
        c.insertText(t); m_coverLog->ensureCursorVisible();
    }
}

void LavoroPage::onAiFinished(const QString&) {
    const quint64 rid = m_ai->currentReqId();
    if (rid == m_myReqId && m_lavoroLog)
        m_lavoroLog->append("\n\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80");
    else if (rid == m_myCoverReqId && m_coverLog)
        m_coverLog->append("\n\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80");
    else return;
    aiDone();
}

void LavoroPage::onAiError(const QString& err) {
    const quint64 rid = m_ai->currentReqId();
    if (rid != m_myReqId && rid != m_myCoverReqId) return;
    const QString el = err.toLower();
    if (!el.contains("canceled") && !el.contains("operation canceled")) {
        QTextEdit* log = (rid == m_myReqId) ? m_lavoroLog : m_coverLog;
        if (log) log->append(QString("\n\xe2\x9d\x8c  Errore: %1").arg(err));
    }
    aiDone();
}

void LavoroPage::onAiAborted() {
    const quint64 rid = m_ai->currentReqId();
    if (rid != m_myReqId && rid != m_myCoverReqId) return;
    QTextEdit* log = (rid == m_myReqId) ? m_lavoroLog : m_coverLog;
    if (log) log->append("\n\xe2\x8f\xb9  Interrotto.\n\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80");
    aiDone();
}

/* ══════════════════════════════════════════════════════════════
   HELPER — Precompila calcolatore con stipendio offerta + mercato
   ══════════════════════════════════════════════════════════════ */
void LavoroPage::precompilaStipendioDaOfferta(const Offerta& o) {
    if (!m_calcMensile || !m_calcAnnuo || !m_calcOrario || !m_mercatoLbl) return;

    /* Stipendio tipico italiano per settore + livello (lordo annuo, fonte ISTAT/Unioncamere) */
    struct R { int minA; int maxA; };
    const auto mkt = [&]() -> R {
        const QString& t = o.tipo; const QString& l = o.livello;
        if (t == "IT") {
            if (l == "laurea_t") return {28000, 50000};
            if (l == "diploma")  return {22000, 38000};
            return {16000, 26000};
        }
        if (t == "Retail")      return l == "diploma" ? R{16000, 22000} : R{13000, 18000};
        if (t == "GDO")         return {13000, 19000};
        if (t == "Ristorazione") return l == "diploma" ? R{15000, 22000} : R{12000, 17000};
        if (t == "Logistica")   return l == "diploma" ? R{17000, 25000} : R{14000, 21000};
        if (t == "Tecnico")     return l == "laurea_t" ? R{22000, 35000} : R{16000, 26000};
        if (t == "Commerciale") return l == "diploma"  ? R{18000, 30000} : R{14000, 22000};
        if (t == "Admin")       return {15000, 22000};
        if (t == "Finanza")     return l == "laurea_t" ? R{24000, 42000} : R{18000, 28000};
        if (t == "Edilizia")    return {16000, 24000};
        if (t == "Turismo")     return {13000, 19000};
        return {14000, 22000};
    }();

    /* Prova a estrarre stipendio dall'annuncio (es. "€1.500/mese" o "18.000 euro/anno") */
    static const QRegularExpression rxEuro(
        R"((?:€|euro|eur)\s*([\d\.]+(?:[,\.]\d{1,2})?)\s*(?:/\s*(?:mese|anno|giorno|ora|h))?|)"
        R"(([\d\.]+(?:[,\.]\d{1,2})?)\s*(?:€|euro|eur)\s*(?:/\s*(?:mese|anno|giorno|ora|h))?)",
        QRegularExpression::CaseInsensitiveOption);
    double offertaAnnuo = 0.0;
    const auto m = rxEuro.match(o.requisiti);
    if (m.hasMatch()) {
        QString ns = m.captured(1).isEmpty() ? m.captured(2) : m.captured(1);
        /* Normalizza formato italiano: 1.500,00 → 1500.00 */
        if (ns.contains(','))
            ns.remove('.').replace(',', '.');
        else
            ns.remove('.');
        const double val = ns.toDouble();
        if (val > 0) {
            /* Euristica: < 5000 → mensile, 5000-200000 → annuo */
            offertaAnnuo = (val < 5000) ? val * 12.0 : val;
        }
    }

    /* Pre-compila il calcolatore con il valore noto o il minimo di mercato */
    const double annuo = offertaAnnuo > 0 ? offertaAnnuo : (double)mkt.minA;
    if (annuo > 0 && !m_calcBusy) {
        m_calcBusy = true;
        const double mensile  = annuo / 12.0;
        const double ore      = m_calcOre->text().replace(',', '.').toDouble();
        const double oreAnno  = (ore > 0 ? ore : 40.0) * 52.0;
        const double orario   = annuo / oreAnno;
        m_calcAnnuo->setText(QString::number(annuo,   'f', 0));
        m_calcMensile->setText(QString::number(mensile, 'f', 2));
        m_calcOrario->setText(QString::number(orario,  'f', 2));
        if (m_calcNettoLbl) {
            const double netto = mensile * 0.72;
            m_calcNettoLbl->setText(
                QString("\xe2\x89\x88 <b>%1 \xe2\x82\xac/mese</b>"
                        "  <span style='color:gray;font-size:10px;'>stima -28%% IRPEF</span>")
                .arg(netto, 0, 'f', 2));
        }
        m_calcBusy = false;
    }

    /* Etichetta informativa: offerta vs. mercato tipico */
    const QString offertaStr = offertaAnnuo > 0
        ? QString("\xe2\x82\xac%1/anno").arg((int)offertaAnnuo)   /* €X/anno */
        : "non indicato";
    const QString livLabel = o.livello == "laurea_t" ? "Laurea"
                           : o.livello == "diploma"  ? "Diploma"
                           :                          "Qualsiasi titolo";
    m_mercatoLbl->setText(
        QString("<small>"
                "\xf0\x9f\x93\xa2 <b>Annuncio:</b> %1"
                " &nbsp;\xe2\x80\xa2&nbsp; "
                "\xf0\x9f\x8f\x9b <b>Mercato %2 (%3):</b>"
                " \xe2\x82\xac%4k\xe2\x80\x93%5k/anno"
                " (\xe2\x89\x88\xe2\x82\xac%6/mese)"
                "</small>")
        .arg(offertaStr)          /* %1 */
        .arg(o.tipo)              /* %2 */
        .arg(livLabel)            /* %3 */
        .arg(mkt.minA / 1000)    /* %4 */
        .arg(mkt.maxA / 1000)    /* %5 */
        .arg(mkt.minA / 12));    /* %6 */
}

/* ══════════════════════════════════════════════════════════════
   onFotoBtnClicked — seleziona e mostra foto profilo CV
   Pixmap scalata a 48×48 con maschera circolare per il tema QSS.
   ══════════════════════════════════════════════════════════════ */
void LavoroPage::onFotoBtnClicked()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Seleziona foto profilo"), QDir::homePath(),
        tr("Immagini (*.jpg *.jpeg *.png *.bmp *.gif)"));
    if (path.isEmpty()) return;

    QPixmap src(path);
    if (src.isNull()) return;

    m_fotoPath = path;

    /* Scala + ritaglia circolare */
    const int sz = 44;
    QPixmap scaled = src.scaled(sz, sz, Qt::KeepAspectRatioByExpanding,
                                 Qt::SmoothTransformation);
    /* Centra il crop quadrato */
    const int ox = (scaled.width()  - sz) / 2;
    const int oy = (scaled.height() - sz) / 2;
    scaled = scaled.copy(ox, oy, sz, sz);

    QPixmap circle(sz, sz);
    circle.fill(Qt::transparent);
    QPainter p(&circle);
    p.setRenderHint(QPainter::Antialiasing);
    p.setClipRegion(QRegion(0, 0, sz, sz, QRegion::Ellipse));
    p.drawPixmap(0, 0, scaled);

    m_fotoLbl->setPixmap(circle);
    m_fotoLbl->setText("");
    m_fotoLbl->setToolTip(QFileInfo(path).fileName());
}
