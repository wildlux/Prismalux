/* ══════════════════════════════════════════════════════════════
   main_tools_bridges.cpp — StrumentiPage: bridge MCP esterni
   =========================================================================
   Blender/Office/FreeCAD/Sketch/CloudCompare — builder riga + slot
   bridge uniti per feature (pattern main_programming_vpn.cpp). Split
   da main_tools.cpp (TODO D-8).
   ══════════════════════════════════════════════════════════════ */
#include "main_tools.h"
#include "../prismalux_paths.h"
#include "../log_bus.h"
#include "../dpi_utils.h"
#include "../widgets/proc_helper.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextBrowser>
#include <QProcess>
#include <QTcpSocket>
#include <QAbstractSocket>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QFileDialog>
#include <QTimer>
#include <QTextCursor>

namespace P = PrismaluxPaths;

/* ──────────────────────────────────────────────────────────────
   Livello 3 — buildBlenderRow
   Riga connessione Blender bridge + NAM.
   Connette blenderPingBtn e blenderHelpBtn internamente.
   ────────────────────────────────────────────────────────────── */
QWidget* StrumentiPage::buildBlenderRow()
{
    auto* row = new QWidget(this);
    auto* lay = new QHBoxLayout(row);
    lay->setContentsMargins(0, 4, 0, 0);
    lay->setSpacing(8);

    auto* lbl = new QLabel("Blender:", row);
    lbl->setObjectName("hintLabel");

    m_blenderHostEdit = new QLineEdit("localhost:6789", row);
    m_blenderHostEdit->setFixedWidth(dpiScale(160));
    m_blenderHostEdit->setPlaceholderText(tr("localhost:6789"));

    auto* blenderPingBtn = new QPushButton("\xf0\x9f\x94\x97  Verifica", row);
    blenderPingBtn->setObjectName("actionBtn");
    blenderPingBtn->setToolTip(
        "Testa la connessione TCP al bridge Blender (porta 9001 default)");
    blenderPingBtn->setFixedWidth(dpiScale(100));

    m_blenderStatusLbl = new QLabel("\xe2\x9a\xaa  Non connesso", row);
    m_blenderStatusLbl->setObjectName("hintLabel");

    m_blenderExecBtn = new QPushButton(
        "\xe2\x96\xb6  Esegui in Blender", row);
    m_blenderExecBtn->setObjectName("actionBtn");
    m_blenderExecBtn->setToolTip(
        "Invia il codice Python generato a Blender via bridge MCP");
    m_blenderExecBtn->setFixedWidth(dpiScale(160));
    m_blenderExecBtn->setEnabled(false);

    auto* blenderHelpBtn = new QPushButton(
        "\xf0\x9f\x9b\x9f \xf0\x9f\x94\xa7  Aiuto", row);
    blenderHelpBtn->setObjectName("actionBtn");
    blenderHelpBtn->setFixedWidth(dpiScale(90));

    lay->addWidget(lbl);
    lay->addWidget(m_blenderHostEdit);
    lay->addWidget(blenderPingBtn);
    lay->addWidget(m_blenderStatusLbl, 1);
    lay->addWidget(m_blenderExecBtn);
    lay->addWidget(blenderHelpBtn);

    row->setVisible(false);

    m_blenderNam = new QNetworkAccessManager(this);

    connect(blenderPingBtn,  &QPushButton::clicked,
            this, &StrumentiPage::onBlenderPingBtnClicked);
    connect(blenderHelpBtn,  &QPushButton::clicked,
            this, &StrumentiPage::onBlenderHelpBtnClicked);
    return row;
}

/* ──────────────────────────────────────────────────────────────
   Livello 3 — buildBlenderHintRow
   Avviso installazione addon Blender MCP.
   ────────────────────────────────────────────────────────────── */
QWidget* StrumentiPage::buildBlenderHintRow()
{
    auto* row = new QWidget(this);
    auto* lay = new QHBoxLayout(row);
    lay->setContentsMargins(0, 0, 0, 4);
    lay->setSpacing(0);
    auto* lbl = new QLabel(row);
    lbl->setObjectName("hintLabel");
    lbl->setOpenExternalLinks(true);
    lbl->setWordWrap(true);
    lbl->setText(
        "\xf0\x9f\x93\xa6 <b>MCP non connesso?</b> "
        "Installa il server Blender MCP: "
        "<a href='https://github.com/ahujasid/blender-mcp'>"
        "github.com/ahujasid/blender-mcp</a> "
        "\xe2\x86\x92 Blender \xc2\xbb Preferences \xc2\xbb Add-ons \xc2\xbb Install "
        "\xe2\x86\x92 avvia il server dal pannello N.");
    lay->addWidget(lbl);
    row->setVisible(false);
    return row;
}

/* ──────────────────────────────────────────────────────────────
   Livello 3 — buildOfficeRow
   Riga connessione Office bridge + NAM.
   Connette officeHelpBtn internamente.
   ────────────────────────────────────────────────────────────── */
QWidget* StrumentiPage::buildOfficeRow()
{
    auto* row = new QWidget(this);
    auto* lay = new QHBoxLayout(row);
    lay->setContentsMargins(0, 4, 0, 0);
    lay->setSpacing(8);

    auto* lbl = new QLabel("Office:", row);
    lbl->setObjectName("hintLabel");

    m_officeStartBtn = new QPushButton("\xe2\x96\xb6  Avvia bridge", row);
    m_officeStartBtn->setObjectName("actionBtn");
    m_officeStartBtn->setToolTip(
        "Avvia il bridge Python che si connette a LibreOffice via UNO API");
    m_officeStartBtn->setFixedWidth(dpiScale(120));

    m_officeStatusLbl = new QLabel("\xe2\x9a\xaa  Bridge non avviato", row);
    m_officeStatusLbl->setObjectName("hintLabel");

    m_officeExecBtn = new QPushButton("\xf0\x9f\x96\xa5  Esegui in Office", row);
    m_officeExecBtn->setObjectName("actionBtn");
    m_officeExecBtn->setToolTip(
        "Invia il codice Python UNO generato a LibreOffice tramite bridge");
    m_officeExecBtn->setFixedWidth(dpiScale(160));
    m_officeExecBtn->setEnabled(false);

    auto* officeHelpBtn = new QPushButton(
        "\xf0\x9f\x9b\x9f \xf0\x9f\x94\xa7  Aiuto", row);
    officeHelpBtn->setObjectName("actionBtn");
    officeHelpBtn->setFixedWidth(dpiScale(90));

    lay->addWidget(lbl);
    lay->addWidget(m_officeStartBtn);
    lay->addWidget(m_officeStatusLbl, 1);
    lay->addWidget(m_officeExecBtn);
    lay->addWidget(officeHelpBtn);

    row->setVisible(false);

    m_officeNam = new QNetworkAccessManager(this);

    connect(officeHelpBtn, &QPushButton::clicked,
            this, &StrumentiPage::onOfficeHelpBtnClicked);
    return row;
}

/* ──────────────────────────────────────────────────────────────
   Livello 3 — buildOfficeHintRow
   Avviso installazione MCP per Microsoft Office.
   ────────────────────────────────────────────────────────────── */
QWidget* StrumentiPage::buildOfficeHintRow()
{
    auto* row = new QWidget(this);
    auto* lay = new QHBoxLayout(row);
    lay->setContentsMargins(0, 0, 0, 4);
    lay->setSpacing(0);
    auto* lbl = new QLabel(row);
    lbl->setObjectName("hintLabel");
    lbl->setOpenExternalLinks(true);
    lbl->setWordWrap(true);
    lbl->setText(
        "\xf0\x9f\x93\xa6 <b>Vuoi controllare Microsoft Office/365 via MCP?</b> "
        "Installa un server MCP dedicato: cerca <b>office365 MCP</b> su "
        "<a href='https://github.com/modelcontextprotocol/servers'>"
        "github.com/modelcontextprotocol/servers</a> "
        "oppure su npm (<code>npm install -g mcp-server-office365</code>), "
        "poi registralo con: <code>claude mcp add office365 node /path/to/server.js</code>. "
        "\xe2\x86\x92 Il bridge Python qui sopra funziona gi\xc3\xa0 con LibreOffice senza MCP.");
    lay->addWidget(lbl);
    row->setVisible(false);
    return row;
}

/* ──────────────────────────────────────────────────────────────
   Livello 3 — buildFreecadRow
   Riga connessione FreeCAD bridge (cat=8).
   Connette freecadPingBtn e freecadHelpBtn internamente.
   ────────────────────────────────────────────────────────────── */
QWidget* StrumentiPage::buildFreecadRow()
{
    auto* row = new QWidget(this);
    auto* lay = new QHBoxLayout(row);
    lay->setContentsMargins(0, 4, 0, 0);
    lay->setSpacing(8);

    auto* lbl = new QLabel("FreeCAD:", row);
    lbl->setObjectName("hintLabel");

    m_freecadHostEdit = new QLineEdit("localhost:9876", row);
    m_freecadHostEdit->setFixedWidth(dpiScale(160));
    m_freecadHostEdit->setPlaceholderText(tr("localhost:9876"));

    auto* freecadPingBtn = new QPushButton("\xf0\x9f\x94\x97  Verifica", row);
    freecadPingBtn->setObjectName("actionBtn");
    freecadPingBtn->setToolTip(
        "Testa la connessione TCP al bridge FreeCAD (porta 9876 default)");
    freecadPingBtn->setFixedWidth(dpiScale(100));

    m_freecadStatusLbl = new QLabel("\xe2\x9a\xaa  Non connesso", row);
    m_freecadStatusLbl->setObjectName("hintLabel");

    m_freecadExecBtn = new QPushButton(
        "\xf0\x9f\x94\xa9  Esegui in FreeCAD", row);
    m_freecadExecBtn->setObjectName("actionBtn");
    m_freecadExecBtn->setToolTip(
        "Invia il codice Python FreeCAD generato via bridge MCP");
    m_freecadExecBtn->setFixedWidth(dpiScale(170));
    m_freecadExecBtn->setEnabled(false);

    auto* freecadHelpBtn = new QPushButton(
        "\xf0\x9f\x9b\x9f \xf0\x9f\x94\xa7  Aiuto", row);
    freecadHelpBtn->setObjectName("actionBtn");
    freecadHelpBtn->setFixedWidth(dpiScale(90));

    lay->addWidget(lbl);
    lay->addWidget(m_freecadHostEdit);
    lay->addWidget(freecadPingBtn);
    lay->addWidget(m_freecadStatusLbl, 1);
    lay->addWidget(m_freecadExecBtn);
    lay->addWidget(freecadHelpBtn);

    row->setVisible(false);

    connect(freecadPingBtn,  &QPushButton::clicked,
            this, &StrumentiPage::onFreecadPingBtnClicked);
    connect(freecadHelpBtn,  &QPushButton::clicked,
            this, &StrumentiPage::onFreecadHelpBtnClicked);
    return row;
}

/* ──────────────────────────────────────────────────────────────
   Livello 3 — buildFreecadHintRow
   Avviso installazione workbench FreeCAD MCP.
   ────────────────────────────────────────────────────────────── */
QWidget* StrumentiPage::buildFreecadHintRow()
{
    auto* row = new QWidget(this);
    auto* lay = new QHBoxLayout(row);
    lay->setContentsMargins(0, 0, 0, 4);
    lay->setSpacing(0);
    auto* lbl = new QLabel(row);
    lbl->setObjectName("hintLabel");
    lbl->setOpenExternalLinks(true);
    lbl->setWordWrap(true);
    lbl->setText(
        "\xf0\x9f\x93\xa6 <b>MCP non connesso?</b> "
        "Installa il workbench FreeCAD MCP: "
        "<a href='https://github.com/bonninr/freecad_mcp'>"
        "github.com/bonninr/freecad_mcp</a> "
        "\xe2\x86\x92 copia in ~/.FreeCAD/Mod/freecad_mcp "
        "\xe2\x86\x92 apri FreeCAD \xc2\xbb seleziona workbench <b>FreeCAD MCP</b> "
        "\xe2\x86\x92 clicca <b>Start RPC Server</b> (porta 9876).");
    lay->addWidget(lbl);
    row->setVisible(false);
    return row;
}

/* ──────────────────────────────────────────────────────────────
   Livello 3 — buildSketchRow
   Riga Sketch → 3D Model (Blender cat=6 + FreeCAD cat=8).
   Connette sketchFileBtn internamente.
   ────────────────────────────────────────────────────────────── */
QWidget* StrumentiPage::buildSketchRow()
{
    auto* row = new QWidget(this);
    auto* lay = new QHBoxLayout(row);
    lay->setContentsMargins(0, 4, 0, 4);
    lay->setSpacing(8);

    auto* iconLbl = new QLabel("\xf0\x9f\x8f\x97  Disegno:", row);
    iconLbl->setObjectName("hintLabel");
    lay->addWidget(iconLbl);

    auto* sketchFileBtn = new QPushButton(
        "\xf0\x9f\x93\x82  Carica disegno / PDF", row);
    sketchFileBtn->setObjectName("actionBtn");
    sketchFileBtn->setFixedWidth(dpiScale(190));
    lay->addWidget(sketchFileBtn);

    m_sketchFileLbl = new QLabel("Nessun file", row);
    m_sketchFileLbl->setObjectName("hintLabel");
    lay->addWidget(m_sketchFileLbl);

    m_sketchNotes = new QLineEdit(row);
    m_sketchNotes->setPlaceholderText(
        "Quote / note  es: 50x30x10mm, acciaio, foro \xc3\x98 8...");
    lay->addWidget(m_sketchNotes, 1);

    m_btnSketchGen = new QPushButton(
        "\xf0\x9f\x8f\x97  Genera modello 3D", row);
    m_btnSketchGen->setObjectName("actionBtn");
    m_btnSketchGen->setFixedWidth(dpiScale(170));
    lay->addWidget(m_btnSketchGen);

    row->setVisible(false);

    connect(sketchFileBtn, &QPushButton::clicked,
            this, &StrumentiPage::onSketchFileBtnClicked);
    return row;
}

/* ──────────────────────────────────────────────────────────────
   Livello 3 — buildCloudCompareRow
   Pannello "prossimamente" per CloudCompare (cat = 9).
   ────────────────────────────────────────────────────────────── */
QWidget* StrumentiPage::buildCloudCompareRow()
{
    auto* row = new QWidget(this);
    auto* lay = new QVBoxLayout(row);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(8);

    /* Pulsante Apri CloudCompare (se installato) */
    auto* btnRow = new QWidget(row);
    auto* btnLay = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(0, 0, 0, 0);
    btnLay->addStretch(1);

    auto* openBtn = new QPushButton(
        "\xf0\x9f\x94\xb5  Apri CloudCompare", btnRow);
    openBtn->setObjectName("actionBtn");
    openBtn->setToolTip(tr("Avvia CloudCompare se installato sul sistema"));
    connect(openBtn, &QPushButton::clicked, row, []() {
        if (!QProcess::startDetached("cloudcompare", {})) {
            QProcess::startDetached("CloudCompare", {});
        }
    });
    btnLay->addWidget(openBtn);
    btnLay->addStretch(1);
    lay->addWidget(btnRow);

    auto* banner = new QLabel(row);
    banner->setObjectName("hintLabel");
    banner->setOpenExternalLinks(true);
    banner->setWordWrap(true);
    banner->setAlignment(Qt::AlignCenter);
    banner->setText(
        "<b>\xf0\x9f\x94\xb5 CloudCompare \xe2\x80\x94 bridge AI in sviluppo</b><br><br>"
        "Il bridge AI non \xc3\xa8 ancora stabile. Nel frattempo puoi aprire CloudCompare "
        "direttamente con il pulsante sopra.<br><br>"
        "<b>Installa CloudCompare:</b><br>"
        "\xe2\x80\xa2 Ubuntu/Debian: <code>sudo apt install cloudcompare</code><br>"
        "\xe2\x80\xa2 Flatpak: <code>flatpak install flathub org.cloudcompare.CloudCompare</code><br>"
        "\xe2\x80\xa2 Windows: scarica da "
        "<a href='https://www.danielgm.net/cc/'>danielgm.net/cc</a><br><br>"
        "<b>Bridge futuro (MCP):</b><br>"
        "\xe2\x80\xa2 Demo sperimentale: "
        "<a href='https://github.com/truebelief/CloudCompareMCP'>CloudCompareMCP</a><br>"
        "\xe2\x80\xa2 Wrapper Python ufficiale: "
        "<a href='https://github.com/CloudCompare/CloudComPy'>CloudComPy</a>");
    lay->addWidget(banner);
    row->setVisible(false);
    return row;
}

/* ══════════════════════════════════════════════════════════════
   Slot: Blender — aiuto installazione
   ══════════════════════════════════════════════════════════════ */
void StrumentiPage::onBlenderHelpBtnClicked()
{
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(tr("\xf0\x9f\x8e\xa8  Installazione Blender MCP"));
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->resize(560, 500);
    auto* dlay = new QVBoxLayout(dlg);
    auto* browser = new QTextBrowser(dlg);
    browser->setOpenExternalLinks(true);
    browser->setHtml(
        "<h3>\xf0\x9f\x8e\xa8 Installazione Blender MCP</h3>"
        "<p>Collega Blender a Prismalux/Claude Code per controllarlo via AI (bpy).</p>"
        "<hr>"
        "<h4>1. Installa Blender</h4>"
        "<p>Scarica da <a href='https://www.blender.org/download/'>blender.org/download</a>"
        " oppure: <code>sudo apt install blender</code></p>"
        "<h4>2. Scarica l&apos;addon Blender MCP</h4>"
        "<pre>git clone https://github.com/ahujasid/blender-mcp</pre>"
        "<h4>3. Installa l&apos;addon in Blender</h4>"
        "<ol>"
        "<li>Apri Blender</li>"
        "<li><b>Edit \xe2\x86\x92 Preferences \xe2\x86\x92 Add-ons \xe2\x86\x92 Install</b></li>"
        "<li>Seleziona il file <code>blender_mcp.py</code> dalla cartella clonata</li>"
        "<li>Abilita l&apos;addon <b>\"Blender MCP\"</b> nella lista</li>"
        "</ol>"
        "<h4>4. Avvia il server MCP in Blender</h4>"
        "<ol>"
        "<li>In 3D Viewport premi <b>N</b> per aprire il pannello laterale</li>"
        "<li>Seleziona il tab <b>MCP</b></li>"
        "<li>Clicca <b>Start MCP Server</b> (porta default: <b>6789</b>)</li>"
        "</ol>"
        "<h4>5. Collega Prismalux</h4>"
        "<p>Torna in Prismalux \xe2\x86\x92 <b>Strumenti \xe2\x86\x92 Blender</b>"
        " \xe2\x86\x92 clicca <b>\xf0\x9f\x94\x97 Verifica</b>.</p>"
        "<h4>6. (Opzionale) Registra in Claude Code</h4>"
        "<pre>claude mcp add blender node /percorso/blender-mcp/server.js</pre>"
        "<hr>"
        "<p>\xe2\x9c\x85 Una volta connesso, i pulsanti <b>Esegui in Blender</b>"
        " inviano il codice bpy generato dall&apos;AI direttamente a Blender.</p>");
    auto* btnClose = new QPushButton("\xe2\x9c\x95  Chiudi", dlg);
    btnClose->setObjectName("actionBtn");
    connect(btnClose, &QPushButton::clicked, dlg, &QDialog::accept);
    dlay->addWidget(browser);
    dlay->addWidget(btnClose);
    dlg->exec();
}

/* ══════════════════════════════════════════════════════════════
   Slot: Blender — ping /status
   ══════════════════════════════════════════════════════════════ */
void StrumentiPage::onBlenderPingBtnClicked()
{
    QString addr = m_blenderHostEdit->text().trimmed();
    if (addr.isEmpty()) addr = "localhost:6789";
    QNetworkRequest req(QUrl("http://" + addr + "/status"));
    req.setTransferTimeout(3000);
    auto* reply = m_blenderNam->get(req);
    connect(reply, &QNetworkReply::finished,
            this, &StrumentiPage::onBlenderPingReplyFinished);
}

void StrumentiPage::onBlenderPingReplyFinished()
{
    auto* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    reply->deleteLater();
    if (reply->error() == QNetworkReply::NoError) {
        QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        QString ver = obj.value("blender").toString("?");
        m_blenderStatusLbl->setText("\xe2\x9c\x85  Blender " + ver + " connesso");
    } else {
        m_blenderStatusLbl->setText("\xe2\x9d\x8c  " + reply->errorString());
        LogBus::post("\xe2\x9d\x8c Strumenti: Blender ping errore: " + reply->errorString());
    }
}

/* ══════════════════════════════════════════════════════════════
   Slot: Blender — POST /execute
   ══════════════════════════════════════════════════════════════ */
void StrumentiPage::onBlenderExecBtnClicked()
{
    if (m_blenderCode.isEmpty()) return;
    QString addr = m_blenderHostEdit->text().trimmed();
    if (addr.isEmpty()) addr = "localhost:6789";

    QJsonObject payload;
    payload["code"] = m_blenderCode;
    QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    QNetworkRequest req(QUrl("http://" + addr + "/execute"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json; charset=utf-8");
    req.setTransferTimeout(20000);

    m_blenderExecBtn->setEnabled(false);
    m_blenderStatusLbl->setText(tr("\xf0\x9f\x94\x84  Invio a Blender..."));

    auto* reply = m_blenderNam->post(req, body);
    connect(reply, &QNetworkReply::finished,
            this, &StrumentiPage::onBlenderExecReplyFinished);
}

void StrumentiPage::onBlenderExecReplyFinished()
{
    auto* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    reply->deleteLater();
    m_blenderExecBtn->setEnabled(true);
    if (reply->error() == QNetworkReply::NoError) {
        QJsonObject res = QJsonDocument::fromJson(reply->readAll()).object();
        if (res["ok"].toBool()) {
            m_blenderStatusLbl->setText(tr("\xe2\x9c\x85  Eseguito"));
            QString out = res["output"].toString();
            m_output->append("\n\xe2\x9c\x85  Blender: " + (out.isEmpty() ? "OK" : out));
        } else {
            m_blenderStatusLbl->setText(tr("\xe2\x9d\x8c  Errore Blender"));
            m_output->append("\n\xe2\x9d\x8c  Blender errore:\n" + res["error"].toString());
            LogBus::post("\xe2\x9d\x8c Strumenti: Blender errore: " + res["error"].toString());
        }
    } else {
        m_blenderStatusLbl->setText("\xe2\x9d\x8c  " + reply->errorString());
        m_output->append("\n\xe2\x9d\x8c  Connessione a Blender fallita: " + reply->errorString());
        LogBus::post("\xe2\x9d\x8c Strumenti: Connessione a Blender fallita: " + reply->errorString());
    }
}

/* ══════════════════════════════════════════════════════════════
   Slot: Office — aiuto installazione
   ══════════════════════════════════════════════════════════════ */
void StrumentiPage::onOfficeHelpBtnClicked()
{
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(tr("\xf0\x9f\x96\xa5  Setup Office \xe2\x80\x94 LibreOffice & Microsoft 365"));
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->resize(580, 560);
    auto* dlay = new QVBoxLayout(dlg);
    auto* browser = new QTextBrowser(dlg);
    browser->setOpenExternalLinks(true);
    browser->setHtml(
        "<h3>\xf0\x9f\x96\xa5 Setup Office: LibreOffice + Microsoft 365</h3>"
        "<hr>"
        "<h4>\xf0\x9f\x93\x97 LibreOffice \xe2\x80\x94 bridge integrato in Prismalux</h4>"
        "<p>Il bridge Python \xc3\xa8 gi\xc3\xa0 incluso. Nessun MCP esterno richiesto.</p>"
        "<ol>"
        "<li><b>Installa LibreOffice</b>:<br>"
        "<a href='https://www.libreoffice.org/download/libreoffice-fresh/'>"
        "libreoffice.org/download</a><br>"
        "oppure: <code>sudo apt install libreoffice</code></li>"
        "<li><b>Installa le dipendenze Python</b>:<br>"
        "<code>pip install python-docx openpyxl python-pptx</code></li>"
        "<li>In Prismalux \xe2\x86\x92 <b>Strumenti \xe2\x86\x92 Office</b>"
        " \xe2\x86\x92 clicca <b>\xe2\x96\xb6 Avvia bridge</b></li>"
        "<li>Il bridge si connette a LibreOffice via <b>UNO API</b></li>"
        "<li>Clicca <b>\xf0\x9f\x96\xa5 Esegui in Office</b> dopo aver generato il codice</li>"
        "</ol>"
        "<p>\xe2\x9c\x85 Supporta: Writer (.odt/.docx), Calc (.ods/.xlsx), Impress (.odp/.pptx)</p>"
        "<hr>"
        "<h4>\xf0\x9f\x93\x98 Microsoft Office 365 \xe2\x80\x94 MCP esterno</h4>"
        "<p>Per controllare Microsoft 365 (Word, Excel, PowerPoint online) serve un MCP dedicato.</p>"
        "<ol>"
        "<li><b>Installa Node.js</b>: <a href='https://nodejs.org/'>nodejs.org</a></li>"
        "<li><b>Cerca un server MCP Office 365</b> nella lista ufficiale:<br>"
        "<a href='https://github.com/modelcontextprotocol/servers'>"
        "github.com/modelcontextprotocol/servers</a></li>"
        "<li><b>Segui le istruzioni</b> del server scelto per installarlo</li>"
        "<li><b>Registra in Claude Code</b>:<br>"
        "<code>claude mcp add office365 node /percorso/al/server/index.js</code></li>"
        "<li>Richiede un account <b>Microsoft 365</b> attivo e le credenziali OAuth</li>"
        "</ol>"
        "<p>\xe2\x9a\xa0 Microsoft Office richiede autenticazione Microsoft 365 &mdash; "
        "non funziona con versioni desktop standalone senza cloud.</p>"
        "<hr>"
        "<p>\xf0\x9f\x92\xa1 <b>Consiglio</b>: per uso locale usa <b>LibreOffice</b> "
        "(gratuito, bridge immediato). Per collaborazione cloud usa <b>Microsoft 365 MCP</b>.</p>");
    auto* btnClose = new QPushButton("\xe2\x9c\x95  Chiudi", dlg);
    btnClose->setObjectName("actionBtn");
    connect(btnClose, &QPushButton::clicked, dlg, &QDialog::accept);
    dlay->addWidget(browser);
    dlay->addWidget(btnClose);
    dlg->exec();
}

/* ══════════════════════════════════════════════════════════════
   Slot: Office — avvia/ferma bridge Python
   ══════════════════════════════════════════════════════════════ */
void StrumentiPage::onOfficeStartBtnClicked()
{
    if (m_officeBridgeProc &&
        m_officeBridgeProc->state() == QProcess::Running) {
        m_officeBridgeProc->terminate();
        m_officeBridgeProc->waitForFinished(2000);
        m_officeStartBtn->setText(tr("\xe2\x96\xb6  Avvia bridge"));
        m_officeStatusLbl->setText(tr("\xe2\x9a\xaa  Bridge fermato"));
        return;
    }

    QString path = _officeBridgePath();
    if (path.isEmpty()) {
        m_officeStatusLbl->setText(tr("\xe2\x9d\x8c  prismalux_office_bridge.py non trovato"));
        LogBus::post("\xe2\x9d\x8c Strumenti: prismalux_office_bridge.py non trovato.");
        return;
    }

    if (!m_officeBridgeProc) {
        m_officeBridgeProc = new QProcess(this);
        m_officeBridgeProc->setProcessChannelMode(QProcess::MergedChannels);
        connect(m_officeBridgeProc,
                QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &StrumentiPage::onOfficeBridgeProcFinished);
    }

    m_officeStatusLbl->setText(tr("\xf0\x9f\x94\x84  Avvio bridge..."));
    m_officeBridgeProc->start(P::findPython(), {path});
    if (!m_officeBridgeProc->waitForStarted(P::kProcessStartTimeoutMs))
        m_officeBridgeProc->start("python", {path});

    if (m_officeBridgeProc->state() == QProcess::Running) {
        m_officeStartBtn->setText(tr("\xe2\x8f\xb9  Ferma bridge"));
        QTimer::singleShot(1200, this, &StrumentiPage::onOfficeStatusCheckTimeout);
    } else {
        m_officeStatusLbl->setText(tr("\xe2\x9d\x8c  Errore avvio (python3 non trovato?)"));
        LogBus::post("\xe2\x9d\x8c Strumenti: Office bridge errore avvio (python3 non trovato?).");
    }
}

void StrumentiPage::onOfficeBridgeProcFinished(int, QProcess::ExitStatus)
{
    m_officeStartBtn->setText(tr("\xe2\x96\xb6  Avvia bridge"));
    m_officeStatusLbl->setText(tr("\xe2\x9a\xaa  Bridge fermato"));
}

void StrumentiPage::onOfficeStatusCheckTimeout()
{
    _readOfficeBridgeToken();
    QNetworkRequest req(QUrl("http://localhost:6790/status"));
    req.setTransferTimeout(2000);
    req.setRawHeader("Authorization", ("Bearer " + m_officeBridgeToken).toUtf8());
    auto* r = m_officeNam->get(req);
    connect(r, &QNetworkReply::finished,
            this, &StrumentiPage::onOfficeBridgeStatusReplyFinished);
}

void StrumentiPage::onOfficeBridgeStatusReplyFinished()
{
    auto* r = qobject_cast<QNetworkReply*>(sender());
    if (!r) return;
    r->deleteLater();
    if (r->error() == QNetworkReply::NoError) {
        QJsonObject obj = QJsonDocument::fromJson(r->readAll()).object();
        QJsonObject libs = obj["libraries"].toObject();
        QStringList ok;
        for (auto it = libs.begin(); it != libs.end(); ++it)
            if (it.value().toBool()) ok << it.key();
        m_officeStatusLbl->setText(
            "\xe2\x9c\x85  " +
            (ok.isEmpty() ? "Bridge pronto (nessuna lib)" : "Pronto: " + ok.join(", ")));
    } else {
        m_officeStatusLbl->setText(tr("\xe2\x9a\xa0  Bridge avviato (verifica fallita)"));
    }
}

/* ══════════════════════════════════════════════════════════════
   Slot: Office — POST /execute
   ══════════════════════════════════════════════════════════════ */
void StrumentiPage::onOfficeExecBtnClicked()
{
    if (m_officeCode.isEmpty()) return;
    _readOfficeBridgeToken();

    QJsonObject payload;
    payload["code"] = m_officeCode;
    QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    QNetworkRequest req(QUrl("http://localhost:6790/execute"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json; charset=utf-8");
    req.setRawHeader("Authorization", ("Bearer " + m_officeBridgeToken).toUtf8());
    req.setTransferTimeout(30000);

    m_officeExecBtn->setEnabled(false);
    m_officeStatusLbl->setText(tr("\xf0\x9f\x94\x84  Esecuzione..."));

    auto* reply = m_officeNam->post(req, body);
    connect(reply, &QNetworkReply::finished,
            this, &StrumentiPage::onOfficeExecReplyFinished);
}

void StrumentiPage::onOfficeExecReplyFinished()
{
    auto* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    reply->deleteLater();
    m_officeExecBtn->setEnabled(true);
    if (reply->error() == QNetworkReply::NoError) {
        QJsonObject res = QJsonDocument::fromJson(reply->readAll()).object();
        if (res["ok"].toBool()) {
            m_officeStatusLbl->setText(tr("\xe2\x9c\x85  Completato"));
            m_output->append("\n\xe2\x9c\x85  Office: " + res["output"].toString());
        } else {
            m_officeStatusLbl->setText(tr("\xe2\x9d\x8c  Errore"));
            m_output->append("\n\xe2\x9d\x8c  Office errore:\n" + res["error"].toString());
            LogBus::post("\xe2\x9d\x8c Strumenti: Office errore: " + res["error"].toString());
        }
    } else {
        m_officeStatusLbl->setText(tr("\xe2\x9d\x8c  Bridge non raggiungibile"));
        m_output->append("\n\xe2\x9d\x8c  Bridge non raggiungibile (avvialo prima).");
        LogBus::post("\xe2\x9d\x8c Strumenti: Office bridge non raggiungibile.");
    }
}

/* ══════════════════════════════════════════════════════════════
   Slot: FreeCAD — aiuto installazione
   ══════════════════════════════════════════════════════════════ */
void StrumentiPage::onFreecadHelpBtnClicked()
{
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(tr("\xf0\x9f\x94\xa9  Installazione FreeCAD MCP"));
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->resize(560, 500);
    auto* dlay = new QVBoxLayout(dlg);
    auto* browser = new QTextBrowser(dlg);
    browser->setOpenExternalLinks(true);
    browser->setHtml(
        "<h3>\xf0\x9f\x94\xa9 Installazione FreeCAD MCP</h3>"
        "<p>Collega FreeCAD a Prismalux/Claude Code per modellazione 3D via AI.</p>"
        "<hr>"
        "<h4>1. Installa FreeCAD</h4>"
        "<p>Scarica da <a href='https://www.freecad.org/downloads.php'>freecad.org/downloads</a>"
        " oppure:<br>"
        "<code>sudo apt install freecad</code><br>"
        "Per la versione pi\xc3\xb9 recente usa FlatPak o AppImage dal sito ufficiale.</p>"
        "<h4>2. Installa il workbench FreeCAD MCP</h4>"
        "<pre>git clone https://github.com/bonninr/freecad_mcp \\\n"
        "      ~/.FreeCAD/Mod/freecad_mcp</pre>"
        "<h4>3. Riavvia FreeCAD</h4>"
        "<p>FreeCAD carica i workbench da <code>~/.FreeCAD/Mod/</code> all&apos;avvio.</p>"
        "<h4>4. Seleziona il workbench FreeCAD MCP</h4>"
        "<ol>"
        "<li>Dal menu workbench in alto seleziona <b>FreeCAD MCP</b></li>"
        "<li>Apparir\xc3\xa0 il pannello con il pulsante <b>Start RPC Server</b></li>"
        "<li>Clicca <b>Start RPC Server</b> (porta default: <b>9876</b>)</li>"
        "</ol>"
        "<h4>5. Collega Prismalux</h4>"
        "<p>Torna in Prismalux \xe2\x86\x92 <b>Strumenti \xe2\x86\x92 FreeCAD</b>"
        " \xe2\x86\x92 clicca <b>\xf0\x9f\x94\x97 Verifica</b>.</p>"
        "<h4>6. (Opzionale) Registra in Claude Code</h4>"
        "<pre>claude mcp add freecad python3 \\\n"
        "  ~/.FreeCAD/Mod/freecad_mcp/src/freecad_bridge.py</pre>"
        "<hr>"
        "<p>\xe2\x9c\x85 Una volta connesso, usa <b>Script libero</b> o le azioni predefinite"
        " per generare ed eseguire codice Python direttamente in FreeCAD.</p>"
        "<p>\xf0\x9f\x8f\x97 Usa anche il pannello <b>Disegno \xe2\x86\x92 Modello 3D</b>"
        " per generare modelli da schizzi o PDF con dimensioni.</p>");
    auto* btnClose = new QPushButton("\xe2\x9c\x95  Chiudi", dlg);
    btnClose->setObjectName("actionBtn");
    connect(btnClose, &QPushButton::clicked, dlg, &QDialog::accept);
    dlay->addWidget(browser);
    dlay->addWidget(btnClose);
    dlg->exec();
}

/* ══════════════════════════════════════════════════════════════
   Slot: FreeCAD — ping TCP
   ══════════════════════════════════════════════════════════════ */
void StrumentiPage::onFreecadPingBtnClicked()
{
    QString addr = m_freecadHostEdit->text().trimmed();
    if (addr.isEmpty()) addr = "localhost:9876";
    const QString host = addr.contains(':') ? addr.section(':', 0, 0) : addr;
    const int port     = addr.contains(':') ? addr.section(':', 1).toInt() : 9876;

    m_freecadStatusLbl->setText(tr("\xf0\x9f\x94\x84  Connessione..."));

    if (m_freecadPingSock) {
        m_freecadPingSock->abort();
        m_freecadPingSock->deleteLater();
    }
    m_freecadPingSock = new QTcpSocket(this);
    connect(m_freecadPingSock.data(), &QTcpSocket::connected,
            this, &StrumentiPage::onFreecadSockConnected);
    connect(m_freecadPingSock.data(), &QAbstractSocket::errorOccurred,
            this, &StrumentiPage::onFreecadSockError);
    m_freecadPingSock->connectToHost(host, static_cast<quint16>(port));
    QTimer::singleShot(3000, this, &StrumentiPage::onFreecadPingTimeout);
}

void StrumentiPage::onFreecadSockConnected()
{
    if (m_freecadPingSock) {
        m_freecadPingSock->disconnectFromHost();
        m_freecadPingSock->deleteLater();
    }
    m_freecadStatusLbl->setText(tr("\xe2\x9c\x85  FreeCAD connesso"));
}

void StrumentiPage::onFreecadSockError(QAbstractSocket::SocketError)
{
    if (!m_freecadPingSock) return;
    m_freecadStatusLbl->setText("\xe2\x9d\x8c  " + m_freecadPingSock->errorString());
    LogBus::post("\xe2\x9d\x8c Strumenti: FreeCAD errore socket: " + m_freecadPingSock->errorString());
    m_freecadPingSock->deleteLater();
}

void StrumentiPage::onFreecadPingTimeout()
{
    if (m_freecadPingSock &&
        m_freecadPingSock->state() != QAbstractSocket::ConnectedState) {
        m_freecadStatusLbl->setText(
            "\xe2\x9d\x8c  Timeout \xe2\x80\x94 FreeCAD non risponde");
        LogBus::post("\xe2\x9d\x8c Strumenti: FreeCAD timeout connessione.");
        m_freecadPingSock->abort();
        m_freecadPingSock->deleteLater();
    }
}

/* ══════════════════════════════════════════════════════════════
   Slot: FreeCAD — esegui script via TCP JSON
   ══════════════════════════════════════════════════════════════ */
void StrumentiPage::onFreecadExecBtnClicked()
{
    if (m_freecadCode.isEmpty()) return;
    QString addr = m_freecadHostEdit->text().trimmed();
    if (addr.isEmpty()) addr = "localhost:9876";
    const QString host = addr.contains(':') ? addr.section(':', 0, 0) : addr;
    const int port     = addr.contains(':') ? addr.section(':', 1).toInt() : 9876;

    QJsonObject payload;
    payload["type"] = "run_script";
    QJsonObject params;
    params["script"] = m_freecadCode;
    payload["params"] = params;
    const QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    m_freecadExecBtn->setEnabled(false);
    m_freecadStatusLbl->setText(tr("\xf0\x9f\x94\x84  Invio a FreeCAD..."));

    if (m_freecadExecSock) {
        m_freecadExecSock->abort();
        m_freecadExecSock->deleteLater();
    }
    m_freecadExecSock = new QTcpSocket(this);
    m_freecadExecSock->setProperty("execBody", body);
    connect(m_freecadExecSock.data(), &QTcpSocket::connected,
            this, &StrumentiPage::onFreecadExecSockConnected);
    connect(m_freecadExecSock.data(), &QTcpSocket::readyRead,
            this, &StrumentiPage::onFreecadExecSockReadyRead);
    connect(m_freecadExecSock.data(), &QAbstractSocket::errorOccurred,
            this, &StrumentiPage::onFreecadExecSockError);
    m_freecadExecSock->connectToHost(host, static_cast<quint16>(port));
}

void StrumentiPage::onFreecadExecSockConnected()
{
    if (!m_freecadExecSock) return;
    const QByteArray body = m_freecadExecSock->property("execBody").toByteArray();
    m_freecadExecSock->write(body);
    m_freecadExecSock->flush();
}

void StrumentiPage::onFreecadExecSockReadyRead()
{
    if (!m_freecadExecSock) return;
    const QByteArray data = m_freecadExecSock->readAll();
    m_freecadExecSock->disconnectFromHost();
    m_freecadExecSock->deleteLater();
    m_freecadExecBtn->setEnabled(true);
    const QJsonObject res = QJsonDocument::fromJson(data).object();
    const QString status  = res["status"].toString();
    if (status == "ok" || status == "success") {
        m_freecadStatusLbl->setText(tr("\xe2\x9c\x85  Eseguito"));
        const QString out = res["result"].toString();
        m_output->moveCursor(QTextCursor::End);
        m_output->append("\n\xe2\x9c\x85  FreeCAD: " + (out.isEmpty() ? "OK" : out));
    } else {
        const QString err = res["message"].toString();
        m_freecadStatusLbl->setText("\xe2\x9d\x8c  " + err);
        m_output->moveCursor(QTextCursor::End);
        m_output->append("\n\xe2\x9d\x8c  FreeCAD errore: " + err);
        LogBus::post("\xe2\x9d\x8c Strumenti: FreeCAD errore: " + err);
    }
}

void StrumentiPage::onFreecadExecSockError(QAbstractSocket::SocketError)
{
    if (!m_freecadExecSock) return;
    m_freecadStatusLbl->setText("\xe2\x9d\x8c  " + m_freecadExecSock->errorString());
    LogBus::post("\xe2\x9d\x8c Strumenti: FreeCAD errore exec socket: " + m_freecadExecSock->errorString());
    m_freecadExecBtn->setEnabled(true);
    m_freecadExecSock->deleteLater();
}

/* ══════════════════════════════════════════════════════════════
   Slot: Sketch — seleziona file disegno
   ══════════════════════════════════════════════════════════════ */
void StrumentiPage::onSketchFileBtnClicked()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        "Seleziona disegno o schema",
        "",
        "Immagini e PDF (*.png *.jpg *.jpeg *.bmp *.webp *.pdf);;"
        "Tutti i file (*)");
    if (path.isEmpty()) return;
    m_sketchFilePath = path;
    m_sketchFileLbl->setText(QFileInfo(path).fileName());
    const QString ext = QFileInfo(path).suffix().toLower();
    m_sketchIsImage = (ext == "png" || ext == "jpg" || ext == "jpeg"
                    || ext == "bmp" || ext == "webp");
}

/* ══════════════════════════════════════════════════════════════
   Slot: Sketch — genera modello 3D
   ══════════════════════════════════════════════════════════════ */
void StrumentiPage::onSketchGenBtnClicked()
{
    if (m_ai->busy()) {
        m_output->append("\xe2\x9a\xa0  AI occupata, attendi.");
        return;
    }

    const bool isBlender = (m_currentCat == 6);
    const QString notes  = m_sketchNotes->text().trimmed();

    if (m_sketchFilePath.isEmpty() && notes.isEmpty()) {
        m_output->append(
            "\xe2\x9a\xa0  Carica un disegno (immagine o PDF) oppure "
            "inserisci quote e note del modello.");
        return;
    }

    const QString sysBlender =
        "Sei un esperto di Blender Python API (bpy). "
        "Analizza il disegno tecnico e genera SOLO codice Python puro "
        "eseguibile in Blender che ricrea il modello 3D corrispondente. "
        "Usa bpy.ops, bpy.data e bpy.context. "
        "Se le dimensioni non sono esplicite, usa proporzioni visive. "
        "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.";

    const QString sysFreecad =
        "Sei un esperto di FreeCAD Python API. "
        "Analizza il disegno tecnico e genera SOLO codice Python puro "
        "eseguibile in FreeCAD che ricrea il modello 3D corrispondente. "
        "Usa: import FreeCAD, Part; doc = FreeCAD.newDocument('Sketch3D'); "
        "aggiungi solidi, applica vincoli e chiama doc.recompute(). "
        "Se le dimensioni non sono esplicite, usa proporzioni visive. "
        "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.";

    const QString sys = isBlender ? sysBlender : sysFreecad;

    QString userMsg = notes.isEmpty()
        ? "Crea il modello 3D corrispondente a questo disegno."
        : QString("Crea il modello 3D da questo disegno. Quote e note: %1").arg(notes);

    if (m_codeModelCombo && m_codeModelCombo->count() > 0) {
        const QString sel = m_codeModelCombo->currentData().toString();
        if (!sel.isEmpty() && sel != m_ai->model())
            m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), sel);
        m_codeModelInfo->setText(
            QString("\xf0\x9f\xa4\x96  Usando: <b>%1</b>").arg(
                m_codeModelCombo->currentText()));
    }

    m_output->clear();
    _setRunBusy(true);
    m_waitLbl->setText(tr("\xf0\x9f\x94\x84  Analisi disegno in corso..."));
    m_waitLbl->setVisible(true);
    m_waitBar->setVisible(true);
    m_active = true;

    if (!m_sketchFilePath.isEmpty() && m_sketchIsImage) {
        QFile f(m_sketchFilePath);
        if (!f.open(QIODevice::ReadOnly)) {
            m_output->append("\xe2\x9d\x8c  Impossibile aprire il file immagine.");
            LogBus::post("\xe2\x9d\x8c Strumenti: Impossibile aprire il file immagine.");
            m_active = false;
            _setRunBusy(false);
            m_waitLbl->setVisible(false);
            m_waitBar->setVisible(false);
            return;
        }
        const QByteArray raw = f.readAll();
        f.close();
        const QString ext  = QFileInfo(m_sketchFilePath).suffix().toLower();
        const QString mime = (ext == "png")  ? "image/png"  :
                             (ext == "webp") ? "image/webp" : "image/jpeg";
        m_ai->chatWithImage(P::prependKnowledge(sys), userMsg, raw.toBase64(), mime);

    } else if (!m_sketchFilePath.isEmpty() && !m_sketchIsImage) {
        const QString pdfText = ProcHelper::run("pdftotext", {m_sketchFilePath, "-"}, 15000).out.trimmed();
        if (!pdfText.isEmpty())
            userMsg += "\n\nCONTENUTO SCHEMA PDF:\n" + pdfText.left(3000);
        else
            userMsg += "\n(Schema PDF allegato ma testo non estraibile — "
                       "usa un modello vision o inserisci le quote manualmente)";
        m_ai->chat(P::prependKnowledge(sys), userMsg);

    } else {
        m_ai->chat(P::prependKnowledge(sys), userMsg);
    }
}

