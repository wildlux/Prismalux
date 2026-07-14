/* ══════════════════════════════════════════════════════════════
   main_app_controller_office.cpp — AppControllerPage: Office + Anki
   ======================================================================
   Tab OFFICE (LibreOffice UNO bridge) + Tab ANKI MCP — builder + slot.
   Split da main_app_controller.cpp/main_app_controller_slots.cpp (TODO D-8).
   ══════════════════════════════════════════════════════════════ */
#include "main_app_controller.h"
#include "../prismalux_paths.h"
#include "../log_bus.h"
#include "../dpi_utils.h"
#include "../widgets/model_combo_box.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QDialog>
#include <QTextBrowser>
#include <QTextEdit>
#include <QPushButton>
#include <QProcess>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTimer>

namespace P = PrismaluxPaths;

const char* kOfficeSys[] = {
    "Sei un esperto di LibreOffice e python-docx. Genera SOLO codice Python. "
    "PRIORITA': se 'desktop' e' nel namespace (UNO), usa LibreOffice Writer. "
    "FALLBACK: usa python-docx Document() → salva in Path.home()/'Desktop'/'documento.docx'. "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    "Sei un esperto di LibreOffice Calc e openpyxl. Genera SOLO codice Python. "
    "PRIORITA' UNO: usa scalc loadComponentFromURL. "
    "FALLBACK: usa openpyxl Workbook → salva .xlsx in Path.home()/'Desktop'/. "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    "Sei un esperto di LibreOffice Impress e python-pptx. Genera SOLO codice Python. "
    "PRIORITA' UNO: usa simpress loadComponentFromURL. "
    "FALLBACK: usa python-pptx Presentation → salva .pptx in Path.home()/'Desktop'/. "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    "Sei un esperto di LibreOffice UNO. Genera SOLO codice Python per modificare un documento. "
    "Apri con desktop.loadComponentFromURL; modifica; salva con doc.store(). "
    "FALLBACK: usa python-docx Document(path). "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    "Sei un esperto di LibreOffice UNO e python-docx. Genera SOLO codice Python che crea una tabella formattata. "
    "Con UNO in Writer: usa insertTextContent con TextTable. Con UNO in Calc: usa getCellByPosition. "
    "FALLBACK: python-docx add_table o openpyxl. "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    "Sei un esperto di LibreOffice Calc UNO e openpyxl. Genera SOLO codice Python per grafici/analisi dati. "
    "FALLBACK: usa openpyxl BarChart + Reference. Salva e stampa il percorso. "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    "Sei un esperto di LibreOffice UNO API. Genera SOLO codice Python eseguibile via exec(). "
    "Namespace UNO: desktop, uno, PropertyValue, createUnoService, systemPath, mkprops. "
    "Salva sempre in Path.home()/'Desktop'/ e stampa il percorso. "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    nullptr
};

static const char* kOfficeActions[] = {
    "\xf0\x9f\x93\x84 Crea documento Word",
    "\xf0\x9f\x93\x8a Crea foglio Excel",
    "\xf0\x9f\x96\xa5 Crea presentazione",
    "\xe2\x9c\x8f Modifica documento",
    "\xf0\x9f\x93\x8b Inserisci tabella",
    "\xf0\x9f\x93\x88 Grafici e dati",
    "\xf0\x9f\x94\xa7 Script libero",
    nullptr
};

/* ══════════════════════════════════════════════════════════════
   Tab OFFICE
   ══════════════════════════════════════════════════════════════ */
QWidget* AppControllerPage::buildOfficeTab()
{
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(6);

    auto* descLbl = new QLabel(
        "\xf0\x9f\x93\x84  <i>Office Suite \xe2\x80\x94 Suite di produttività per la creazione di documenti di testo, "
        "fogli di calcolo e presentazioni. Compatibile con LibreOffice e Microsoft Office tramite bridge Python-UNO.</i>", w);
    descLbl->setObjectName("hintLabel");
    descLbl->setTextFormat(Qt::RichText);
    descLbl->setWordWrap(true);
    lay->addWidget(descLbl);

    /* ── Barra connessione bridge ── */
    auto* connRow = new QWidget(w);
    auto* connLay = new QHBoxLayout(connRow);
    connLay->setContentsMargins(0, 0, 0, 0);
    connLay->setSpacing(8);

    auto* lbl = new QLabel(tr("Office:"), connRow);
    lbl->setObjectName("hintLabel");

    m_officeStartBtn = new QPushButton(tr("\xe2\x96\xb6  Avvia bridge"), connRow);
    m_officeStartBtn->setObjectName("actionBtn");
    m_officeStartBtn->setFixedWidth(dpiScale(120));

    m_officeStatusLbl = new QLabel(tr("\xe2\x9a\xaa  Bridge inattivo"), connRow);
    m_officeStatusLbl->setObjectName("hintLabel");

    m_officeExecBtn = new QPushButton(
        "\xe2\x96\xb6  Esegui in Office", connRow);
    m_officeExecBtn->setObjectName("actionBtn");
    m_officeExecBtn->setFixedWidth(dpiScale(160));
    m_officeExecBtn->setEnabled(false);

    connLay->addWidget(lbl);
    auto* officeHelpBtn = new QPushButton(tr("\xf0\x9f\x9b\x9f  Aiuto"), connRow);
    officeHelpBtn->setToolTip(tr("Apri la documentazione LibreOffice e guida comandi macro"));
    officeHelpBtn->setObjectName("actionBtn");
    officeHelpBtn->setFixedWidth(dpiScale(80));
    connLay->addWidget(m_officeStartBtn);
    connLay->addWidget(m_officeStatusLbl, 1);
    connLay->addWidget(m_officeExecBtn);
    connLay->addWidget(officeHelpBtn);
    lay->addWidget(connRow);

    /* ── Hint ── */
    auto* hintLbl = new QLabel(
        "\xf0\x9f\x93\xa6 <b>Bridge Office:</b> il bridge Python locale "
        "controlla LibreOffice via UNO. "
        "Clicca <b>Avvia bridge</b> per avviarlo.", w);
    hintLbl->setObjectName("hintLabel");
    hintLbl->setWordWrap(true);
    lay->addWidget(hintLbl);

    /* ── Azione + Modello ── */
    auto* toolRow = new QWidget(w);
    auto* toolLay = new QHBoxLayout(toolRow);
    toolLay->setContentsMargins(0, 0, 0, 0);
    toolLay->setSpacing(8);

    m_officeAction = new QComboBox(toolRow);
    for (int i = 0; kOfficeActions[i]; i++)
        m_officeAction->addItem(QString::fromUtf8(kOfficeActions[i]));

    m_officeModel = new ModelComboBox(m_ai, toolRow);

    toolLay->addWidget(new QLabel(tr("Azione:"), toolRow));
    toolLay->addWidget(m_officeAction, 1);
    toolLay->addWidget(new QLabel(tr("Modello:"), toolRow));
    toolLay->addWidget(m_officeModel, 1);
    lay->addWidget(toolRow);

    /* ── Input ── */
    m_officeInput = new QTextEdit(w);
    m_officeInput->setPlaceholderText(
        "Descrivi il documento da creare "
        "(es. 'Lettera di presentazione professionale', "
        "'Foglio Excel con budget mensile')...");
    m_officeInput->setMaximumHeight(dpiScale(80));
    m_officeInput->setMinimumHeight(dpiScale(60));
    lay->addWidget(m_officeInput);

    /* ── Run/Stop ── */
    auto* btnRow = new QWidget(w);
    auto* btnLay = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(0, 0, 0, 0);
    btnLay->setSpacing(8);
    m_officeRunBtn  = new QPushButton(tr("\xe2\x96\xb6  Genera codice AI"), btnRow);
    m_officeRunBtn->setObjectName("actionBtn");
    m_officeStopBtn = new QPushButton(tr("\xe2\x8f\xb9  Stop"), btnRow);
    m_officeStopBtn->setObjectName("actionBtn");
    m_officeStopBtn->setProperty("danger", true);
    m_officeStopBtn->setEnabled(false);
    btnLay->addWidget(m_officeRunBtn);
    btnLay->addWidget(m_officeStopBtn);
    btnLay->addStretch();
    lay->addWidget(btnRow);

    /* ── Output ── */
    m_officeOutput = new QTextEdit(w);
    m_officeOutput->setReadOnly(true);
    m_officeOutput->setObjectName("outputView");
    m_officeOutput->setPlaceholderText(tr("Output AI e Office apparirà qui..."));
    lay->addWidget(m_officeOutput, 1);

    /* ── Connessioni bridge ── */
    m_officeNam = new QNetworkAccessManager(this);

    connect(m_officeStartBtn, &QPushButton::clicked,
            this, &AppControllerPage::onOfficeStartClicked);
    connect(m_officeExecBtn,  &QPushButton::clicked,
            this, &AppControllerPage::onOfficeExecClicked);
    connect(m_officeRunBtn,   &QPushButton::clicked,
            this, &AppControllerPage::onOfficeRunClicked);
    connect(m_officeStopBtn,  &QPushButton::clicked,
            this, &AppControllerPage::onOfficeStopClicked);
    connect(officeHelpBtn,    &QPushButton::clicked,
            this, &AppControllerPage::onOfficeHelpClicked);

    return w;
}

/* ======================================================================
   Sezione 5 — Office tab slots
   ====================================================================== */

/** Legge il token Bearer dal file ~/.prismalux_office_token */
static void s_readOfficeBridgeToken(QString& outToken)
{
    const QString tokenFile = QDir::homePath() + "/.prismalux_office_token";
    QFile f(tokenFile);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    outToken = QString::fromUtf8(f.readAll()).trimmed();
}

/** Cerca prismalux_office_bridge.py risalendo la dir-tree */
static QString s_findOfficeBridgePath()
{
    QDir d(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 4; i++) {
        QString p = d.filePath("MCPs/office_bridge/prismalux_office_bridge.py");
        if (QFile::exists(p)) return p;
        d.cdUp();
    }
    return {};
}

void AppControllerPage::onOfficeStartClicked()
{
    if (m_officeBridgeProc &&
        m_officeBridgeProc->state() == QProcess::Running) {
        m_officeBridgeProc->terminate();
        m_officeBridgeProc->waitForFinished(2000);
        m_officeStartBtn->setText(tr("\xe2\x96\xb6  Avvia bridge"));
        m_officeStatusLbl->setText(tr("\xe2\x9a\xaa  Bridge fermato"));
        return;
    }
    const QString path = s_findOfficeBridgePath();
    if (path.isEmpty()) {
        m_officeStatusLbl->setText(
            "\xe2\x9d\x8c  prismalux_office_bridge.py non trovato");
        return;
    }
    if (!m_officeBridgeProc) {
        m_officeBridgeProc = new QProcess(this);
        m_officeBridgeProc->setProcessChannelMode(QProcess::MergedChannels);
        connect(m_officeBridgeProc,
                QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                this, &AppControllerPage::onOfficeBridgeFinished);
    }
    m_officeStatusLbl->setText(tr("\xf0\x9f\x94\x84  Avvio bridge..."));
    m_officeBridgeProc->start(P::findPython(), {path});
    if (m_officeBridgeProc->state() == QProcess::Running) {
        m_officeStartBtn->setText(tr("\xe2\x8f\xb9  Ferma bridge"));
        QTimer::singleShot(1200, this, &AppControllerPage::onOfficeStatusReply);
    } else {
        m_officeStatusLbl->setText(
            "\xe2\x9d\x8c  Errore avvio (python3 non trovato?)");
    }
}

void AppControllerPage::onOfficeBridgeFinished(int /*exitCode*/, QProcess::ExitStatus /*status*/)
{
    m_officeStartBtn->setText(tr("\xe2\x96\xb6  Avvia bridge"));
    m_officeStatusLbl->setText(tr("\xe2\x9a\xaa  Bridge fermato"));
}

void AppControllerPage::onOfficeStatusReply()
{
    /* Chiamata da QTimer::singleShot 1200ms dopo avvio bridge */
    s_readOfficeBridgeToken(m_officeBridgeToken);
    QNetworkRequest req(QUrl("http://localhost:6790/status"));
    req.setTransferTimeout(2000);
    req.setRawHeader("Authorization",
                     ("Bearer " + m_officeBridgeToken).toUtf8());
    m_officeStatusReply = m_officeNam->get(req);
    connect(m_officeStatusReply, &QNetworkReply::finished,
            this, [this]() {
        auto* r = m_officeStatusReply;
        m_officeStatusReply = nullptr;
        if (!r) return;
        r->deleteLater();
        if (r->error() == QNetworkReply::NoError) {
            QJsonObject obj  = QJsonDocument::fromJson(r->readAll()).object();
            QJsonObject libs = obj["libraries"].toObject();
            QStringList ok;
            for (auto it = libs.begin(); it != libs.end(); ++it)
                if (it.value().toBool()) ok << it.key();
            m_officeStatusLbl->setText(
                "\xe2\x9c\x85  Pronto: " +
                (ok.isEmpty() ? "bridge attivo" : ok.join(", ")));
        } else {
            m_officeStatusLbl->setText(
                "\xe2\x9a\xa0  Bridge avviato (verifica fallita)");
        }
    });
}

void AppControllerPage::onOfficeExecClicked()
{
    if (m_officeCode.isEmpty()) return;
    s_readOfficeBridgeToken(m_officeBridgeToken);
    QJsonObject payload; payload["code"] = m_officeCode;
    QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    QNetworkRequest req(QUrl("http://localhost:6790/execute"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json; charset=utf-8");
    req.setRawHeader("Authorization", ("Bearer " + m_officeBridgeToken).toUtf8());
    req.setTransferTimeout(30000);
    m_officeExecBtn->setEnabled(false);
    m_officeStatusLbl->setText(tr("\xf0\x9f\x94\x84  Invio a Office..."));
    m_officeExecReply = m_officeNam->post(req, body);
    connect(m_officeExecReply, &QNetworkReply::finished,
            this, &AppControllerPage::onOfficeExecReply);
}

void AppControllerPage::onOfficeExecReply()
{
    auto* reply = m_officeExecReply;
    m_officeExecReply = nullptr;
    if (!reply) return;
    reply->deleteLater();
    m_officeExecBtn->setEnabled(true);
    if (reply->error() == QNetworkReply::NoError) {
        QJsonObject res = QJsonDocument::fromJson(reply->readAll()).object();
        if (res["ok"].toBool()) {
            m_officeStatusLbl->setText(tr("\xe2\x9c\x85  Eseguito"));
            m_officeOutput->append("\n\xe2\x9c\x85  Office: "
                + res["output"].toString("OK"));
        } else {
            m_officeStatusLbl->setText(tr("\xe2\x9d\x8c  Errore"));
            m_officeOutput->append("\n\xe2\x9d\x8c  Office errore: "
                + res["error"].toString());
        }
    } else {
        m_officeStatusLbl->setText(tr("\xe2\x9d\x8c  ") + reply->errorString());
    }
}

void AppControllerPage::onOfficeRunClicked()
{
    const int idx = m_officeAction->currentIndex();
    if (idx < 0 || !kOfficeSys[idx]) return;
    runAi(2, QString::fromUtf8(kOfficeSys[idx]),
          m_officeInput->toPlainText(),
          m_officeOutput, m_officeRunBtn, m_officeStopBtn,
          m_officeModel);
}

void AppControllerPage::onOfficeStopClicked()
{
    m_ai->abort();
    m_officeRunBtn->setEnabled(true);
    m_officeStopBtn->setEnabled(false);
    m_officeOutput->append("\n\xe2\x8f\xb9  Fermato.");
}

void AppControllerPage::onOfficeHelpClicked()
{
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(tr("\xf0\x9f\x96\xa5  Installazione Office Bridge"));
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->resize(540, 420);
    auto* dlay    = new QVBoxLayout(dlg);
    auto* browser = new QTextBrowser(dlg);
    browser->setOpenExternalLinks(true);
    browser->setHtml(
        "<h3>\xf0\x9f\x96\xa5 Office Bridge (LibreOffice)</h3>"
        "<h4>1. Installa LibreOffice + python-uno</h4>"
        "<p><code>sudo apt install libreoffice python3-uno</code></p>"
        "<h4>2. Avvia il bridge</h4>"
        "<p>Clicca <b>\xe2\x96\xb6 Avvia bridge</b> in questa scheda: avvia automaticamente "
        "<code>prismalux_office_bridge.py</code> (porta <b>6790</b>).</p>"
        "<h4>3. Genera ed esegui</h4>"
        "<p>Scrivi l'istruzione nel campo testo \xe2\x86\x92 "
        "<b>Genera codice AI</b> \xe2\x86\x92 <b>Esegui in Office</b>.</p>"
        "<h4>Nota</h4>"
        "<p>Il bridge controlla LibreOffice Writer / Calc / Impress via API UNO. "
        "LibreOffice deve essere installato ma non necessariamente aperto.</p>");
    auto* btnClose = new QPushButton(tr("\xe2\x9c\x95  Chiudi"), dlg);
    btnClose->setObjectName("actionBtn");
    connect(btnClose, &QPushButton::clicked, dlg, &QDialog::accept);
    dlay->addWidget(browser);
    dlay->addWidget(btnClose);
    dlg->exec();
}

const char* kAnkiSys[] = {
    "Sei un esperto di apprendimento attivo e memorizzazione. "
    "Genera carte Anki in formato JSON array. Ogni carta ha: "
    "{\"front\": \"domanda o concetto\", \"back\": \"risposta concisa\", \"tags\": [\"tag1\"]}. "
    "Genera da 5 a 10 carte per l'argomento richiesto. Rispondi SOLO con il JSON array tra ``` e ```.",

    "Sei un esperto di lingue e traduzione. "
    "Genera carte Anki per lo studio del vocabolario in formato JSON array. "
    "Ogni carta: {\"front\": \"parola in italiano\", \"back\": \"traduzione + esempio\", \"tags\": [\"vocabolario\"]}. "
    "Genera 8 carte. Rispondi SOLO con il JSON array tra ``` e ```.",

    "Sei un esperto di informatica e programmazione. "
    "Genera carte Anki tecniche in formato JSON array per l'argomento richiesto. "
    "Ogni carta: {\"front\": \"concetto o definizione breve\", \"back\": \"spiegazione + esempio codice se rilevante\", \"tags\": [\"tech\"]}. "
    "Genera 6-8 carte. Rispondi SOLO con il JSON array tra ``` e ```.",

    "Sei un esperto di scienze. "
    "Genera carte Anki scientifiche in formato JSON array per l'argomento richiesto. "
    "Ogni carta: {\"front\": \"concetto\", \"back\": \"spiegazione accurata\", \"tags\": [\"scienze\"]}. "
    "Rispondi SOLO con il JSON array tra ``` e ```.",

    nullptr
};

static const char* kAnkiActions[] = {
    "\xf0\x9f\x93\x9a  Genera carte (generico)",
    "\xf0\x9f\x8c\x8d  Carte vocabolario",
    "\xf0\x9f\x92\xbb  Carte informatica",
    "\xf0\x9f\x94\xac  Carte scienze",
    nullptr
};

/* ══════════════════════════════════════════════════════════════
   Tab ANKI MCP
   ══════════════════════════════════════════════════════════════ */
QWidget* AppControllerPage::buildAnkiTab()
{
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(6);

    auto* descLbl = new QLabel(
        "\xf0\x9f\x83\x8f  <i>Anki \xe2\x80\x94 Applicazione open-source per la memorizzazione tramite flashcard "
        "con algoritmo di ripetizione spaziata (SRS). Ideale per lingue, medicina, diritto e materie tecniche.</i>", w);
    descLbl->setObjectName("hintLabel");
    descLbl->setTextFormat(Qt::RichText);
    descLbl->setWordWrap(true);
    lay->addWidget(descLbl);

    /* ── Barra connessione AnkiConnect ── */
    auto* connRow = new QWidget(w);
    auto* connLay = new QHBoxLayout(connRow);
    connLay->setContentsMargins(0, 0, 0, 0);
    connLay->setSpacing(8);

    auto* lbl = new QLabel(tr("AnkiConnect:"), connRow);
    lbl->setObjectName("hintLabel");

    m_ankiHostEdit = new QLineEdit("localhost:8765", connRow);
    m_ankiHostEdit->setFixedWidth(dpiScale(150));

    auto* pingBtn = new QPushButton(tr("\xf0\x9f\x94\x97  Verifica"), connRow);
    pingBtn->setToolTip(tr("Verifica connessione con AnkiConnect (porta 8765)"));
    pingBtn->setObjectName("actionBtn");
    pingBtn->setFixedWidth(dpiScale(100));

    m_ankiStatusLbl = new QLabel(tr("\xe2\x9a\xaa  Non connesso"), connRow);
    m_ankiStatusLbl->setObjectName("hintLabel");

    m_ankiSendBtn = new QPushButton(
        "\xf0\x9f\x83\x8f  Invia ad Anki", connRow);
    m_ankiSendBtn->setObjectName("actionBtn");
    m_ankiSendBtn->setFixedWidth(dpiScale(150));
    m_ankiSendBtn->setEnabled(false);

    connLay->addWidget(lbl);
    connLay->addWidget(m_ankiHostEdit);
    connLay->addWidget(pingBtn);
    auto* ankiHelpBtn = new QPushButton(tr("\xf0\x9f\x9b\x9f  Aiuto"), connRow);
    ankiHelpBtn->setToolTip(tr("Apri la documentazione AnkiConnect e guida AI per flashcard"));
    ankiHelpBtn->setObjectName("actionBtn");
    ankiHelpBtn->setFixedWidth(dpiScale(80));
    connLay->addWidget(m_ankiStatusLbl, 1);
    connLay->addWidget(m_ankiSendBtn);
    connLay->addWidget(ankiHelpBtn);
    lay->addWidget(connRow);

    /* ── Hint ── */
    auto* hintLbl = new QLabel(
        "\xf0\x9f\x83\x8f <b>AnkiConnect non attivo?</b> "
        "Installa il plugin <b>AnkiConnect</b> in Anki (Tools \xe2\x86\x92 Add-ons), "
        "poi avvia Anki. Server sulla porta 8765.", w);
    hintLbl->setObjectName("hintLabel");
    hintLbl->setWordWrap(true);
    lay->addWidget(hintLbl);

    /* ── Azione + Deck + Modello AI ── */
    auto* toolRow = new QWidget(w);
    auto* toolLay = new QHBoxLayout(toolRow);
    toolLay->setContentsMargins(0, 0, 0, 0);
    toolLay->setSpacing(8);

    m_ankiAction = new QComboBox(toolRow);
    for (int i = 0; kAnkiActions[i]; i++)
        m_ankiAction->addItem(QString::fromUtf8(kAnkiActions[i]));

    m_ankiDeckEdit = new QLineEdit("Default", toolRow);
    m_ankiDeckEdit->setFixedWidth(dpiScale(120));
    m_ankiDeckEdit->setToolTip(tr("Nome deck Anki destinazione"));
    auto* deckEdit = m_ankiDeckEdit;  // alias per il layout

    m_ankiModel = new ModelComboBox(m_ai, toolRow);

    toolLay->addWidget(new QLabel(tr("Tipo:"), toolRow));
    toolLay->addWidget(m_ankiAction, 1);
    toolLay->addWidget(new QLabel(tr("Deck:"), toolRow));
    toolLay->addWidget(deckEdit);
    toolLay->addWidget(new QLabel(tr("Modello AI:"), toolRow));
    toolLay->addWidget(m_ankiModel, 1);
    lay->addWidget(toolRow);

    /* ── Input ── */
    m_ankiInput = new QTextEdit(w);
    m_ankiInput->setPlaceholderText(
        "Descrivi l'argomento per cui vuoi generare carte Anki...\n"
        "Es: 'Algoritmi di ordinamento (bubble sort, merge sort, quicksort)'\n"
        "Es: 'Vocabolario inglese — verbi irregolari comuni'");
    m_ankiInput->setFixedHeight(dpiScale(90));
    lay->addWidget(m_ankiInput);

    /* ── Pulsanti ── */
    auto* btnRow = new QWidget(w);
    auto* btnLay = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(0, 0, 0, 0);
    btnLay->setSpacing(8);

    m_ankiRunBtn  = new QPushButton(tr("\xf0\x9f\xa4\x96  Genera carte"), btnRow);
    m_ankiRunBtn->setObjectName("actionBtn");
    m_ankiStopBtn = new QPushButton(tr("\xe2\x8f\xb9  Stop"), btnRow);
    m_ankiStopBtn->setObjectName("actionBtn");
    m_ankiStopBtn->setEnabled(false);
    btnLay->addWidget(m_ankiRunBtn);
    btnLay->addWidget(m_ankiStopBtn);
    btnLay->addStretch();
    lay->addWidget(btnRow);

    /* ── Output ── */
    m_ankiOutput = new QTextEdit(w);
    m_ankiOutput->setReadOnly(true);
    m_ankiOutput->setObjectName("outputView");
    m_ankiOutput->setPlaceholderText(tr("Le carte generate appariranno qui in formato JSON..."));
    lay->addWidget(m_ankiOutput, 1);

    /* ── NAM per AnkiConnect ── */
    m_ankiNam = new QNetworkAccessManager(this);

    /* ── Connessioni ── */
    connect(pingBtn,        &QPushButton::clicked,
            this, &AppControllerPage::onAnkiPingClicked);
    connect(m_ankiSendBtn,  &QPushButton::clicked,
            this, &AppControllerPage::onAnkiSendClicked);
    connect(m_ankiRunBtn,   &QPushButton::clicked,
            this, &AppControllerPage::onAnkiRunClicked);
    connect(m_ankiStopBtn,  &QPushButton::clicked,
            this, &AppControllerPage::onAnkiStopClicked);
    connect(ankiHelpBtn,    &QPushButton::clicked,
            this, &AppControllerPage::onAnkiHelpClicked);

    return w;
}

/* ══════════════════════════════════════════════════════════════
   execAnkiAction — invia carte JSON ad AnkiConnect
   ══════════════════════════════════════════════════════════════ */
void AppControllerPage::execAnkiAction(const QString& deck, const QString& cardsJson)
{
    if (cardsJson.isEmpty()) {
        m_ankiOutput->append("\n\xe2\x9a\xa0  Nessun JSON di carte trovato nell'output.");
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(cardsJson.toUtf8());
    if (!doc.isArray()) {
        m_ankiOutput->append("\n\xe2\x9a\xa0  Il JSON non \xc3\xa8 un array valido. Ricontrolla l'output.");
        return;
    }

    const QJsonArray cards = doc.array();
    QJsonArray notes;
    for (const auto& v : cards) {
        const QJsonObject c = v.toObject();
        QJsonObject note;
        note["deckName"]  = deck;
        note["modelName"] = "Basic";
        QJsonObject fields;
        fields["Front"] = c.value("front").toString(c.value("Front").toString());
        fields["Back"]  = c.value("back").toString(c.value("Back").toString());
        note["fields"] = fields;
        QJsonArray tags;
        for (const auto& t : c.value("tags").toArray())
            tags.append(t);
        note["tags"] = tags;
        notes.append(note);
    }

    QJsonObject body;
    body["action"]  = "addNotes";
    body["version"] = 6;
    QJsonObject params;
    params["notes"] = notes;
    body["params"]  = params;

    QNetworkRequest req(QUrl("http://" + m_ankiHostEdit->text().trimmed()));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setTransferTimeout(10000);

    m_ankiStatusLbl->setText(tr("\xf0\x9f\x94\x84  Invio carte ad Anki..."));
    m_ankiPendingCount = notes.size();
    m_ankiPendingReply = m_ankiNam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(m_ankiPendingReply, &QNetworkReply::finished,
            this, &AppControllerPage::onAnkiAddNotesReply);
}

/* ======================================================================
   Sezione 7 — Anki tab slots
   ====================================================================== */

void AppControllerPage::onAnkiPingClicked()
{
    const QString host = m_ankiHostEdit->text().trimmed();
    QUrl url("http://" + host);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setTransferTimeout(3000);
    QJsonObject body;
    body["action"]  = "version";
    body["version"] = 6;
    if (m_ankiPingReply) { m_ankiPingReply->abort(); m_ankiPingReply->deleteLater(); }
    m_ankiPingReply = m_ankiNam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    m_ankiStatusLbl->setText(tr("\xf0\x9f\x94\x84  Verifica..."));
    connect(m_ankiPingReply, &QNetworkReply::finished,
            this, &AppControllerPage::onAnkiPingReplyFinished);
}

void AppControllerPage::onAnkiPingReplyFinished()
{
    if (!m_ankiPingReply) return;
    m_ankiPingReply->deleteLater();
    const bool ok = (m_ankiPingReply->error() == QNetworkReply::NoError);
    m_ankiPingReply = nullptr;
    if (ok) {
        m_ankiStatusLbl->setText(tr("\xe2\x9c\x85  AnkiConnect attivo"));
        m_ankiSendBtn->setEnabled(true);
    } else {
        m_ankiStatusLbl->setText(tr("\xe2\x9d\x8c  Anki non raggiungibile (avvia Anki)"));
    }
}

void AppControllerPage::onAnkiSendClicked()
{
    if (m_ankiOutput->toPlainText().trimmed().isEmpty()) return;
    const QString raw  = m_ankiOutput->toPlainText();
    const QString json = extractCode(raw);
    execAnkiAction(m_ankiDeckEdit ? m_ankiDeckEdit->text().trimmed() : "Default", json);
}

void AppControllerPage::onAnkiRunClicked()
{
    const int idx = m_ankiAction->currentIndex();
    if (idx < 0 || !kAnkiSys[idx]) return;
    runAi(4, QString::fromUtf8(kAnkiSys[idx]),
          m_ankiInput->toPlainText(),
          m_ankiOutput, m_ankiRunBtn, m_ankiStopBtn,
          m_ankiModel);
}

void AppControllerPage::onAnkiStopClicked()
{
    m_ai->abort();
    m_ankiRunBtn->setEnabled(true);
    m_ankiStopBtn->setEnabled(false);
    m_ankiOutput->append("\n\xe2\x8f\xb9  Fermato.");
}

void AppControllerPage::onAnkiHelpClicked()
{
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(tr("\xf0\x9f\x83\x8f  Installazione AnkiConnect"));
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->resize(540, 420);
    auto* dlay    = new QVBoxLayout(dlg);
    auto* browser = new QTextBrowser(dlg);
    browser->setOpenExternalLinks(true);
    browser->setHtml(
        "<h3>\xf0\x9f\x83\x8f Anki MCP \xe2\x80\x94 AnkiConnect</h3>"
        "<h4>1. Installa Anki</h4>"
        "<p><a href='https://apps.ankiweb.net/'>apps.ankiweb.net</a> "
        "oppure <code>sudo apt install anki</code></p>"
        "<h4>2. Installa il plugin AnkiConnect</h4>"
        "<p>Anki \xe2\x86\x92 <b>Strumenti</b> \xe2\x86\x92 <b>Componenti aggiuntivi</b> "
        "\xe2\x86\x92 <b>Sfoglia e installa</b> \xe2\x86\x92 inserisci il codice:</p>"
        "<pre>2055492159</pre>"
        "<h4>3. Avvia Anki</h4>"
        "<p>AnkiConnect si avvia automaticamente con Anki (porta <b>8765</b>). "
        "Non chiudere Anki durante l'uso.</p>"
        "<h4>4. Collega</h4>"
        "<p>Torna qui \xe2\x86\x92 clicca <b>\xf0\x9f\x94\x97 Verifica</b>.</p>");
    auto* btnClose = new QPushButton(tr("\xe2\x9c\x95  Chiudi"), dlg);
    btnClose->setObjectName("actionBtn");
    connect(btnClose, &QPushButton::clicked, dlg, &QDialog::accept);
    dlay->addWidget(browser);
    dlay->addWidget(btnClose);
    dlg->exec();
}

void AppControllerPage::onAnkiAddNotesReply()
{
    auto* reply = m_ankiPendingReply;
    m_ankiPendingReply = nullptr;
    if (!reply) return;
    reply->deleteLater();
    const int count = m_ankiPendingCount;
    if (reply->error() == QNetworkReply::NoError) {
        m_ankiStatusLbl->setText(
            QString("\xe2\x9c\x85  %1 carte inviate ad Anki").arg(count));
        m_ankiOutput->append(
            QString("\n\xe2\x9c\x85  Inviate %1 carte nel deck.").arg(count));
    } else {
        m_ankiStatusLbl->setText(tr("\xe2\x9d\x8c  ") + reply->errorString());
        m_ankiOutput->append("\n\xe2\x9d\x8c  Errore invio: " + reply->errorString());
    }
}
