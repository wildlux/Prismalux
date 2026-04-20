#include "app_controller_page.h"
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QProcess>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QDir>
#include <QFile>
#include <QCoreApplication>
#include <QDialog>
#include <QVBoxLayout>
#include <QTextBrowser>
#include <QGroupBox>

/* ──────────────────────────────────────────────────────────────
   System prompt arrays — identici a kSysPrompts[6..9] di StrumentiPage
   ────────────────────────────────────────────────────────────── */
static const char* kBlenderSys[] = {
    "Sei un esperto di Blender Python API (bpy). Genera SOLO codice Python puro eseguibile in Blender. "
    "Crea o modifica un materiale Principled BSDF sull'oggetto attivo (bpy.context.active_object). "
    "Imposta base_color, metallic, roughness secondo la richiesta. "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    "Sei un esperto di Blender Python API (bpy). Genera SOLO codice Python puro per spostare un oggetto in Blender. "
    "Usa bpy.context.active_object.location = (x, y, z). "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    "Sei un esperto di Blender Python API (bpy). Genera SOLO codice Python puro per ruotare un oggetto in Blender. "
    "Usa bpy.context.active_object.rotation_euler = (rx, ry, rz) con angoli in radianti. "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    "Sei un esperto di Blender Python API (bpy). Genera SOLO codice Python puro per scalare un oggetto in Blender. "
    "Usa bpy.context.active_object.scale = (sx, sy, sz). "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    "Sei un esperto di Blender Python API (bpy). Genera SOLO codice Python puro per cambiare la visibilita' di oggetti. "
    "Usa obj.hide_viewport e obj.hide_render. "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    "Sei un esperto di Blender Python API (bpy). Genera SOLO codice Python puro per configurare e avviare un render. "
    "Usa bpy.context.scene.render per le impostazioni, bpy.ops.render.render(write_still=True) per avviare. "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    "Sei un esperto di Blender Python API (bpy). Genera SOLO codice Python puro eseguibile in Blender via exec(). "
    "Il namespace disponibile: bpy, mathutils, Vector, Euler, Matrix. "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    nullptr
};

static const char* kBlenderActions[] = {
    "\xf0\x9f\x8e\xa8 Cambia Materiale",
    "\xe2\x86\x94 Trasla",
    "\xf0\x9f\x94\x84 Ruota",
    "\xf0\x9f\x93\x90 Scala",
    "\xf0\x9f\x91\x81 Visibilit\xc3\xa0",
    "\xf0\x9f\x8e\xac Avvia Render",
    "\xf0\x9f\x90\x8d Script libero",
    nullptr
};

static const char* kFreeCADSys[] = {
    "Sei un esperto di FreeCAD Python API. Genera SOLO codice Python puro eseguibile in FreeCAD. "
    "Usa: import FreeCAD, Part; doc = FreeCAD.activeDocument() or FreeCAD.newDocument('Doc'); "
    "aggiungi la primitiva richiesta; doc.recompute(). "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    "Sei un esperto di FreeCAD Python API. Genera SOLO codice Python puro per creare uno schizzo (Sketcher). "
    "Usa: import FreeCAD, Sketcher; sketch = doc.addObject('Sketcher::SketchObject','Sketch'); doc.recompute(). "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    "Sei un esperto di FreeCAD Python API. Genera SOLO codice Python puro per operazioni booleane. "
    "Usa Part.fuse(), Part.cut(), Part.common(). doc.recompute() alla fine. "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    "Sei un esperto di FreeCAD Python API. Genera SOLO codice Python puro per esportare la geometria. "
    "Per STL: import Mesh; Mesh.export([obj], '/tmp/output.stl'). "
    "Per STEP: import Import; Import.export([obj], '/tmp/output.step'). "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    "Sei un esperto di FreeCAD Python API. Genera SOLO codice Python puro per modificare proprieta' di oggetti. "
    "Accedi con FreeCAD.activeDocument().getObject('Nome'); modifica .Length, .Width, .Height, ecc.; doc.recompute(). "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    "Sei un esperto di FreeCAD Python API. Genera SOLO codice Python puro per aggiungere vincoli e misure. "
    "Usa sketch.addConstraint(Sketcher.Constraint(...)); per misure 3D usa Draft.makeDimension(). doc.recompute(). "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    "Sei un esperto di FreeCAD Python API. Genera SOLO codice Python puro eseguibile in FreeCAD via exec(). "
    "Namespace: FreeCAD, FreeCADGui, Part, PartDesign, Sketcher, Draft, Mesh, Import, App, Gui. "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    nullptr
};

static const char* kFreeCADActions[] = {
    "\xf0\x9f\x9f\xa6 Crea Primitiva",
    "\xe2\x9c\x8f Crea Schizzo",
    "\xe2\x9c\x82 Booleana",
    "\xf0\x9f\x93\xa4 Esporta STL/STEP",
    "\xf0\x9f\x94\xa7 Modifica propriet\xc3\xa0",
    "\xf0\x9f\x93\x90 Vincoli & misure",
    "\xf0\x9f\x90\x8d Script libero",
    nullptr
};

static const char* kOfficeSys[] = {
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
   Costruttore
   ══════════════════════════════════════════════════════════════ */
AppControllerPage::AppControllerPage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    m_tabs = new QTabWidget(this);
    m_tabs->setObjectName("innerTabs");
    m_tabs->setTabPosition(QTabWidget::North);

    m_tabs->addTab(buildBlenderTab(),      "\xf0\x9f\x8e\xa8  Blender");
    m_tabs->addTab(buildFreeCADTab(),      "\xf0\x9f\x94\xa9  FreeCAD");
    m_tabs->addTab(buildOfficeTab(),       "\xf0\x9f\x96\xa5  Office");
    m_tabs->addTab(buildCloudCompareTab(), "\xf0\x9f\x94\xb5  CloudCompare");

    lay->addWidget(m_tabs);

    /* Propaga modello corrente a tutte le combo */
    connect(m_ai, &AiClient::modelsReady, this, [this](const QStringList& models) {
        for (auto* cb : {m_blenderModel, m_freecadModel, m_officeModel}) {
            if (!cb) continue;
            const QString cur = cb->count() > 0 ? cb->currentData().toString() : QString();
            cb->blockSignals(true);
            cb->clear();
            for (const auto& m : models) cb->addItem(m, m);
            int idx = cb->findData(cur.isEmpty() ? m_ai->model() : cur);
            if (idx >= 0) cb->setCurrentIndex(idx);
            cb->blockSignals(false);
        }
    });
}

/* ──────────────────────────────────────────────────────────────
   Helper: popola una combo modelli
   ────────────────────────────────────────────────────────────── */
void AppControllerPage::populateModels(QComboBox* combo)
{
    combo->clear();
    const QString cur = m_ai->model();
    if (!cur.isEmpty()) combo->addItem(cur, cur);
    combo->setCurrentIndex(0);
    m_ai->fetchModels();
}

/* ──────────────────────────────────────────────────────────────
   Helper: estrai codice Python dal primo blocco ```...```
   ────────────────────────────────────────────────────────────── */
QString AppControllerPage::extractCode(const QString& text)
{
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
}

/* ──────────────────────────────────────────────────────────────
   Helper: lancia AI con streaming — pattern connHolder one-shot
   ────────────────────────────────────────────────────────────── */
void AppControllerPage::runAi(int tabIdx, const QString& sys, const QString& userMsg,
                               QTextEdit* output, QPushButton* runBtn, QPushButton* stopBtn,
                               QComboBox* modelCombo)
{
    if (m_ai->busy()) {
        output->append("\xe2\x9a\xa0  AI occupata, attendi o premi Stop.");
        return;
    }
    if (userMsg.trimmed().isEmpty()) {
        output->append("\xe2\x9a\xa0  Inserisci la richiesta prima di eseguire.");
        return;
    }

    /* Applica modello selezionato */
    if (modelCombo && modelCombo->count() > 0) {
        const QString sel = modelCombo->currentData().toString();
        if (!sel.isEmpty() && sel != m_ai->model())
            m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), sel);
    }

    m_aiActive  = true;
    m_activeTab = tabIdx;
    runBtn->setEnabled(false);
    stopBtn->setEnabled(true);
    output->append(
        "\n\xf0\x9f\x94\x84  Generazione in corso...\n"
        + QString(40, QChar(0x2500)));

    /* Reset connessione precedente */
    delete m_tokenHolder;
    m_tokenHolder = new QObject(this);

    connect(m_ai, &AiClient::token, m_tokenHolder, [output](const QString& t) {
        output->moveCursor(QTextCursor::End);
        output->insertPlainText(t);
    });

    connect(m_ai, &AiClient::finished, m_tokenHolder,
            [this, output, runBtn, stopBtn](const QString& full) {
        m_aiActive = false;
        runBtn->setEnabled(true);
        stopBtn->setEnabled(false);
        output->append("\n" + QString(40, QChar(0x2500)));
        delete m_tokenHolder;
        m_tokenHolder = nullptr;

        /* Estrai codice e abilita pulsante "Esegui in <App>" */
        const QString code = extractCode(full);
        if (m_activeTab == 0 && !code.isEmpty()) {
            m_blenderCode = code;
            m_blenderExecBtn->setEnabled(true);
            m_blenderStatusLbl->setText(
                "\xf0\x9f\x90\x8d  Codice pronto \xe2\x80\x94 premi Esegui in Blender");
        } else if (m_activeTab == 1 && !code.isEmpty()) {
            m_freecadCode = code;
            m_freecadExecBtn->setEnabled(true);
            m_freecadStatusLbl->setText(
                "\xf0\x9f\x94\xa9  Codice pronto \xe2\x80\x94 premi Esegui in FreeCAD");
        } else if (m_activeTab == 2 && !code.isEmpty()) {
            m_officeCode = code;
            m_officeExecBtn->setEnabled(true);
            m_officeStatusLbl->setText(
                "\xf0\x9f\x93\x84  Codice pronto \xe2\x80\x94 premi Esegui in Office");
        }
    });

    connect(m_ai, &AiClient::error, m_tokenHolder,
            [this, output, runBtn, stopBtn](const QString& msg) {
        m_aiActive = false;
        runBtn->setEnabled(true);
        stopBtn->setEnabled(false);
        output->append("\n\xe2\x9d\x8c  Errore AI: " + msg);
        delete m_tokenHolder;
        m_tokenHolder = nullptr;
    });

    m_ai->chat(sys, userMsg);
}

/* ══════════════════════════════════════════════════════════════
   Tab BLENDER
   ══════════════════════════════════════════════════════════════ */
QWidget* AppControllerPage::buildBlenderTab()
{
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(6);

    /* ── Barra connessione ── */
    auto* connRow = new QWidget(w);
    auto* connLay = new QHBoxLayout(connRow);
    connLay->setContentsMargins(0, 0, 0, 0);
    connLay->setSpacing(8);

    auto* lbl = new QLabel("Blender:", connRow);
    lbl->setObjectName("hintLabel");

    m_blenderHostEdit = new QLineEdit("localhost:6789", connRow);
    m_blenderHostEdit->setFixedWidth(150);

    auto* pingBtn = new QPushButton("\xf0\x9f\x94\x97  Verifica", connRow);
    pingBtn->setObjectName("actionBtn");
    pingBtn->setFixedWidth(100);

    m_blenderStatusLbl = new QLabel("\xe2\x9a\xaa  Non connesso", connRow);
    m_blenderStatusLbl->setObjectName("hintLabel");

    m_blenderExecBtn = new QPushButton(
        "\xe2\x96\xb6  Esegui in Blender", connRow);
    m_blenderExecBtn->setObjectName("actionBtn");
    m_blenderExecBtn->setFixedWidth(160);
    m_blenderExecBtn->setEnabled(false);

    auto* helpBtn = new QPushButton("\xf0\x9f\x9b\x9f  Aiuto", connRow);
    helpBtn->setObjectName("actionBtn");
    helpBtn->setFixedWidth(80);

    connLay->addWidget(lbl);
    connLay->addWidget(m_blenderHostEdit);
    connLay->addWidget(pingBtn);
    connLay->addWidget(m_blenderStatusLbl, 1);
    connLay->addWidget(m_blenderExecBtn);
    connLay->addWidget(helpBtn);
    lay->addWidget(connRow);

    /* ── Hint ── */
    auto* hintLbl = new QLabel(
        "\xf0\x9f\x93\xa6 <b>MCP non connesso?</b> "
        "Installa <a href='https://github.com/ahujasid/blender-mcp'>"
        "blender-mcp</a> \xe2\x86\x92 Blender N \xe2\x86\x92 MCP \xe2\x86\x92 Start.", w);
    hintLbl->setObjectName("hintLabel");
    hintLbl->setOpenExternalLinks(true);
    hintLbl->setWordWrap(true);
    lay->addWidget(hintLbl);

    /* ── Azione + Modello ── */
    auto* toolRow = new QWidget(w);
    auto* toolLay = new QHBoxLayout(toolRow);
    toolLay->setContentsMargins(0, 0, 0, 0);
    toolLay->setSpacing(8);

    m_blenderAction = new QComboBox(toolRow);
    for (int i = 0; kBlenderActions[i]; i++)
        m_blenderAction->addItem(QString::fromUtf8(kBlenderActions[i]));

    m_blenderModel = new QComboBox(toolRow);
    m_blenderModel->setMinimumWidth(180);
    populateModels(m_blenderModel);

    toolLay->addWidget(new QLabel("Azione:", toolRow));
    toolLay->addWidget(m_blenderAction, 1);
    toolLay->addWidget(new QLabel("Modello:", toolRow));
    toolLay->addWidget(m_blenderModel, 1);
    lay->addWidget(toolRow);

    /* ── Input ── */
    m_blenderInput = new QTextEdit(w);
    m_blenderInput->setPlaceholderText(
        "Descrivi cosa fare in Blender "
        "(es. 'Cambia il cubo in rosso metallico', 'Sposta il piano a Y=3')...");
    m_blenderInput->setMaximumHeight(80);
    m_blenderInput->setMinimumHeight(60);
    lay->addWidget(m_blenderInput);

    /* ── Run/Stop ── */
    auto* btnRow = new QWidget(w);
    auto* btnLay = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(0, 0, 0, 0);
    btnLay->setSpacing(8);
    m_blenderRunBtn  = new QPushButton("\xe2\x96\xb6  Genera codice AI", btnRow);
    m_blenderRunBtn->setObjectName("actionBtn");
    m_blenderStopBtn = new QPushButton("\xe2\x8f\xb9  Stop", btnRow);
    m_blenderStopBtn->setObjectName("actionBtn");
    m_blenderStopBtn->setProperty("danger", true);
    m_blenderStopBtn->setEnabled(false);
    btnLay->addWidget(m_blenderRunBtn);
    btnLay->addWidget(m_blenderStopBtn);
    btnLay->addStretch();
    lay->addWidget(btnRow);

    /* ── Output ── */
    m_blenderOutput = new QTextEdit(w);
    m_blenderOutput->setReadOnly(true);
    m_blenderOutput->setObjectName("outputView");
    m_blenderOutput->setPlaceholderText("Output AI e Blender apparirà qui...");
    lay->addWidget(m_blenderOutput, 1);

    /* ── Connessioni ── */
    m_blenderNam = new QNetworkAccessManager(this);

    connect(pingBtn, &QPushButton::clicked, this, [this]() {
        QString addr = m_blenderHostEdit->text().trimmed();
        if (addr.isEmpty()) addr = "localhost:6789";
        QNetworkRequest req(QUrl("http://" + addr + "/status"));
        req.setTransferTimeout(3000);
        auto* reply = m_blenderNam->get(req);
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            reply->deleteLater();
            if (reply->error() == QNetworkReply::NoError) {
                QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
                m_blenderStatusLbl->setText(
                    "\xe2\x9c\x85  Blender " + obj.value("blender").toString("?") + " connesso");
            } else {
                m_blenderStatusLbl->setText("\xe2\x9d\x8c  " + reply->errorString());
            }
        });
    });

    connect(m_blenderExecBtn, &QPushButton::clicked, this, [this]() {
        if (m_blenderCode.isEmpty()) return;
        QString addr = m_blenderHostEdit->text().trimmed();
        if (addr.isEmpty()) addr = "localhost:6789";
        QJsonObject payload; payload["code"] = m_blenderCode;
        QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);
        QNetworkRequest req(QUrl("http://" + addr + "/execute"));
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json; charset=utf-8");
        req.setTransferTimeout(20000);
        m_blenderExecBtn->setEnabled(false);
        m_blenderStatusLbl->setText("\xf0\x9f\x94\x84  Invio a Blender...");
        auto* reply = m_blenderNam->post(req, body);
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            reply->deleteLater();
            m_blenderExecBtn->setEnabled(true);
            QJsonObject res = QJsonDocument::fromJson(reply->readAll()).object();
            if (reply->error() == QNetworkReply::NoError && res["ok"].toBool()) {
                m_blenderStatusLbl->setText("\xe2\x9c\x85  Eseguito");
                m_blenderOutput->append("\n\xe2\x9c\x85  Blender: "
                    + res["output"].toString("OK"));
            } else {
                m_blenderStatusLbl->setText("\xe2\x9d\x8c  Errore");
                m_blenderOutput->append("\n\xe2\x9d\x8c  Blender errore: "
                    + (res["error"].toString().isEmpty()
                       ? reply->errorString() : res["error"].toString()));
            }
        });
    });

    connect(helpBtn, &QPushButton::clicked, this, [this]() {
        auto* dlg = new QDialog(this);
        dlg->setWindowTitle("\xf0\x9f\x8e\xa8  Installazione Blender MCP");
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->resize(540, 480);
        auto* dlay = new QVBoxLayout(dlg);
        auto* browser = new QTextBrowser(dlg);
        browser->setOpenExternalLinks(true);
        browser->setHtml(
            "<h3>\xf0\x9f\x8e\xa8 Blender MCP</h3>"
            "<h4>1. Installa Blender</h4>"
            "<p><code>sudo apt install blender</code> oppure "
            "<a href='https://www.blender.org/download/'>blender.org</a></p>"
            "<h4>2. Clona e installa l'addon</h4>"
            "<pre>git clone https://github.com/ahujasid/blender-mcp</pre>"
            "<p>Blender \xe2\x86\x92 Edit \xe2\x86\x92 Preferences \xe2\x86\x92 Add-ons \xe2\x86\x92 Install "
            "\xe2\x86\x92 seleziona <code>blender_mcp.py</code></p>"
            "<h4>3. Avvia il server</h4>"
            "<p>3D Viewport \xe2\x86\x92 N \xe2\x86\x92 tab MCP \xe2\x86\x92 <b>Start MCP Server</b> (porta 6789)</p>"
            "<h4>4. Collega</h4>"
            "<p>Torna qui \xe2\x86\x92 clicca <b>\xf0\x9f\x94\x97 Verifica</b>.</p>");
        auto* btnClose = new QPushButton("\xe2\x9c\x95  Chiudi", dlg);
        btnClose->setObjectName("actionBtn");
        connect(btnClose, &QPushButton::clicked, dlg, &QDialog::accept);
        dlay->addWidget(browser);
        dlay->addWidget(btnClose);
        dlg->exec();
    });

    connect(m_blenderRunBtn, &QPushButton::clicked, this, [this]() {
        const int idx = m_blenderAction->currentIndex();
        if (idx < 0 || !kBlenderSys[idx]) return;
        runAi(0, QString::fromUtf8(kBlenderSys[idx]),
              m_blenderInput->toPlainText(),
              m_blenderOutput, m_blenderRunBtn, m_blenderStopBtn,
              m_blenderModel);
    });

    connect(m_blenderStopBtn, &QPushButton::clicked, this, [this]() {
        m_ai->abort();
        m_blenderRunBtn->setEnabled(true);
        m_blenderStopBtn->setEnabled(false);
        m_blenderOutput->append("\n\xe2\x8f\xb9  Fermato.");
    });

    return w;
}

/* ══════════════════════════════════════════════════════════════
   Tab FREECAD
   ══════════════════════════════════════════════════════════════ */
QWidget* AppControllerPage::buildFreeCADTab()
{
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(6);

    /* ── Barra connessione ── */
    auto* connRow = new QWidget(w);
    auto* connLay = new QHBoxLayout(connRow);
    connLay->setContentsMargins(0, 0, 0, 0);
    connLay->setSpacing(8);

    auto* lbl = new QLabel("FreeCAD:", connRow);
    lbl->setObjectName("hintLabel");

    m_freecadHostEdit = new QLineEdit("localhost:9876", connRow);
    m_freecadHostEdit->setFixedWidth(150);

    auto* pingBtn = new QPushButton("\xf0\x9f\x94\x97  Verifica", connRow);
    pingBtn->setObjectName("actionBtn");
    pingBtn->setFixedWidth(100);

    m_freecadStatusLbl = new QLabel("\xe2\x9a\xaa  Non connesso", connRow);
    m_freecadStatusLbl->setObjectName("hintLabel");

    m_freecadExecBtn = new QPushButton(
        "\xe2\x96\xb6  Esegui in FreeCAD", connRow);
    m_freecadExecBtn->setObjectName("actionBtn");
    m_freecadExecBtn->setFixedWidth(160);
    m_freecadExecBtn->setEnabled(false);

    connLay->addWidget(lbl);
    connLay->addWidget(m_freecadHostEdit);
    connLay->addWidget(pingBtn);
    connLay->addWidget(m_freecadStatusLbl, 1);
    connLay->addWidget(m_freecadExecBtn);
    lay->addWidget(connRow);

    /* ── Hint ── */
    auto* hintLbl = new QLabel(
        "\xf0\x9f\x93\xa6 <b>Bridge FreeCAD:</b> installa "
        "<a href='https://github.com/manuelbb-upb/FreeCAD-MCP'>"
        "FreeCAD-MCP</a> e avvia il server sulla porta 9876.", w);
    hintLbl->setObjectName("hintLabel");
    hintLbl->setOpenExternalLinks(true);
    hintLbl->setWordWrap(true);
    lay->addWidget(hintLbl);

    /* ── Azione + Modello ── */
    auto* toolRow = new QWidget(w);
    auto* toolLay = new QHBoxLayout(toolRow);
    toolLay->setContentsMargins(0, 0, 0, 0);
    toolLay->setSpacing(8);

    m_freecadAction = new QComboBox(toolRow);
    for (int i = 0; kFreeCADActions[i]; i++)
        m_freecadAction->addItem(QString::fromUtf8(kFreeCADActions[i]));

    m_freecadModel = new QComboBox(toolRow);
    m_freecadModel->setMinimumWidth(180);
    populateModels(m_freecadModel);

    toolLay->addWidget(new QLabel("Azione:", toolRow));
    toolLay->addWidget(m_freecadAction, 1);
    toolLay->addWidget(new QLabel("Modello:", toolRow));
    toolLay->addWidget(m_freecadModel, 1);
    lay->addWidget(toolRow);

    /* ── Input ── */
    m_freecadInput = new QTextEdit(w);
    m_freecadInput->setPlaceholderText(
        "Descrivi cosa modellare in FreeCAD "
        "(es. 'Crea un box 20x10x5mm', 'Esporta il modello attivo in STL')...");
    m_freecadInput->setMaximumHeight(80);
    m_freecadInput->setMinimumHeight(60);
    lay->addWidget(m_freecadInput);

    /* ── Run/Stop ── */
    auto* btnRow = new QWidget(w);
    auto* btnLay = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(0, 0, 0, 0);
    btnLay->setSpacing(8);
    m_freecadRunBtn  = new QPushButton("\xe2\x96\xb6  Genera codice AI", btnRow);
    m_freecadRunBtn->setObjectName("actionBtn");
    m_freecadStopBtn = new QPushButton("\xe2\x8f\xb9  Stop", btnRow);
    m_freecadStopBtn->setObjectName("actionBtn");
    m_freecadStopBtn->setProperty("danger", true);
    m_freecadStopBtn->setEnabled(false);
    btnLay->addWidget(m_freecadRunBtn);
    btnLay->addWidget(m_freecadStopBtn);
    btnLay->addStretch();
    lay->addWidget(btnRow);

    /* ── Output ── */
    m_freecadOutput = new QTextEdit(w);
    m_freecadOutput->setReadOnly(true);
    m_freecadOutput->setObjectName("outputView");
    m_freecadOutput->setPlaceholderText("Output AI e FreeCAD apparirà qui...");
    lay->addWidget(m_freecadOutput, 1);

    /* ── Connessioni ── */
    connect(pingBtn, &QPushButton::clicked, this, [this]() {
        QString addr = m_freecadHostEdit->text().trimmed();
        if (addr.isEmpty()) addr = "localhost:9876";
        const QString host = addr.contains(':') ? addr.section(':', 0, 0) : addr;
        const int     port = addr.contains(':') ? addr.section(':', 1).toInt() : 9876;
        m_freecadStatusLbl->setText("\xf0\x9f\x94\x84  Connessione...");
        auto* sock = new QTcpSocket(this);
        sock->connectToHost(host, static_cast<quint16>(port));
        connect(sock, &QTcpSocket::connected, this, [this, sock]() {
            sock->disconnectFromHost(); sock->deleteLater();
            m_freecadStatusLbl->setText("\xe2\x9c\x85  FreeCAD connesso");
        });
        connect(sock, &QAbstractSocket::errorOccurred, this,
                [this, sock](QAbstractSocket::SocketError) {
            m_freecadStatusLbl->setText("\xe2\x9d\x8c  " + sock->errorString());
            sock->deleteLater();
        });
        QTimer::singleShot(3000, sock, [sock, this]() {
            if (sock->state() != QAbstractSocket::ConnectedState) {
                m_freecadStatusLbl->setText("\xe2\x9d\x8c  Timeout");
                sock->abort(); sock->deleteLater();
            }
        });
    });

    connect(m_freecadExecBtn, &QPushButton::clicked, this, [this]() {
        if (m_freecadCode.isEmpty()) return;
        QString addr = m_freecadHostEdit->text().trimmed();
        if (addr.isEmpty()) addr = "localhost:9876";
        const QString host = addr.contains(':') ? addr.section(':', 0, 0) : addr;
        const int     port = addr.contains(':') ? addr.section(':', 1).toInt() : 9876;
        QJsonObject payload; payload["type"] = "run_script";
        QJsonObject params; params["script"] = m_freecadCode;
        payload["params"] = params;
        QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);
        m_freecadExecBtn->setEnabled(false);
        m_freecadStatusLbl->setText("\xf0\x9f\x94\x84  Invio a FreeCAD...");
        auto* sock = new QTcpSocket(this);
        sock->connectToHost(host, static_cast<quint16>(port));
        connect(sock, &QTcpSocket::connected, this, [sock, body]() {
            sock->write(body); sock->flush();
        });
        connect(sock, &QTcpSocket::readyRead, this, [this, sock]() {
            const QByteArray data = sock->readAll();
            sock->disconnectFromHost(); sock->deleteLater();
            m_freecadExecBtn->setEnabled(true);
            QJsonObject res = QJsonDocument::fromJson(data).object();
            const QString st = res["status"].toString();
            if (st == "ok" || st == "success") {
                m_freecadStatusLbl->setText("\xe2\x9c\x85  Eseguito");
                m_freecadOutput->append("\n\xe2\x9c\x85  FreeCAD: "
                    + res["result"].toString("OK"));
            } else {
                m_freecadStatusLbl->setText("\xe2\x9d\x8c  " + res["message"].toString());
                m_freecadOutput->append("\n\xe2\x9d\x8c  FreeCAD errore: "
                    + res["message"].toString());
            }
        });
        connect(sock, &QAbstractSocket::errorOccurred, this,
                [this, sock](QAbstractSocket::SocketError) {
            m_freecadStatusLbl->setText("\xe2\x9d\x8c  " + sock->errorString());
            m_freecadExecBtn->setEnabled(true);
            sock->deleteLater();
        });
    });

    connect(m_freecadRunBtn, &QPushButton::clicked, this, [this]() {
        const int idx = m_freecadAction->currentIndex();
        if (idx < 0 || !kFreeCADSys[idx]) return;
        runAi(1, QString::fromUtf8(kFreeCADSys[idx]),
              m_freecadInput->toPlainText(),
              m_freecadOutput, m_freecadRunBtn, m_freecadStopBtn,
              m_freecadModel);
    });

    connect(m_freecadStopBtn, &QPushButton::clicked, this, [this]() {
        m_ai->abort();
        m_freecadRunBtn->setEnabled(true);
        m_freecadStopBtn->setEnabled(false);
        m_freecadOutput->append("\n\xe2\x8f\xb9  Fermato.");
    });

    return w;
}

/* ══════════════════════════════════════════════════════════════
   Tab OFFICE
   ══════════════════════════════════════════════════════════════ */
QWidget* AppControllerPage::buildOfficeTab()
{
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(6);

    /* ── Barra connessione bridge ── */
    auto* connRow = new QWidget(w);
    auto* connLay = new QHBoxLayout(connRow);
    connLay->setContentsMargins(0, 0, 0, 0);
    connLay->setSpacing(8);

    auto* lbl = new QLabel("Office:", connRow);
    lbl->setObjectName("hintLabel");

    m_officeStartBtn = new QPushButton("\xe2\x96\xb6  Avvia bridge", connRow);
    m_officeStartBtn->setObjectName("actionBtn");
    m_officeStartBtn->setFixedWidth(120);

    m_officeStatusLbl = new QLabel("\xe2\x9a\xaa  Bridge inattivo", connRow);
    m_officeStatusLbl->setObjectName("hintLabel");

    m_officeExecBtn = new QPushButton(
        "\xe2\x96\xb6  Esegui in Office", connRow);
    m_officeExecBtn->setObjectName("actionBtn");
    m_officeExecBtn->setFixedWidth(160);
    m_officeExecBtn->setEnabled(false);

    connLay->addWidget(lbl);
    connLay->addWidget(m_officeStartBtn);
    connLay->addWidget(m_officeStatusLbl, 1);
    connLay->addWidget(m_officeExecBtn);
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

    m_officeModel = new QComboBox(toolRow);
    m_officeModel->setMinimumWidth(180);
    populateModels(m_officeModel);

    toolLay->addWidget(new QLabel("Azione:", toolRow));
    toolLay->addWidget(m_officeAction, 1);
    toolLay->addWidget(new QLabel("Modello:", toolRow));
    toolLay->addWidget(m_officeModel, 1);
    lay->addWidget(toolRow);

    /* ── Input ── */
    m_officeInput = new QTextEdit(w);
    m_officeInput->setPlaceholderText(
        "Descrivi il documento da creare "
        "(es. 'Lettera di presentazione professionale', "
        "'Foglio Excel con budget mensile')...");
    m_officeInput->setMaximumHeight(80);
    m_officeInput->setMinimumHeight(60);
    lay->addWidget(m_officeInput);

    /* ── Run/Stop ── */
    auto* btnRow = new QWidget(w);
    auto* btnLay = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(0, 0, 0, 0);
    btnLay->setSpacing(8);
    m_officeRunBtn  = new QPushButton("\xe2\x96\xb6  Genera codice AI", btnRow);
    m_officeRunBtn->setObjectName("actionBtn");
    m_officeStopBtn = new QPushButton("\xe2\x8f\xb9  Stop", btnRow);
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
    m_officeOutput->setPlaceholderText("Output AI e Office apparirà qui...");
    lay->addWidget(m_officeOutput, 1);

    /* ── Connessioni bridge ── */
    m_officeNam = new QNetworkAccessManager(this);

    auto _readToken = [this]() {
        const QString tokenFile = QDir::homePath() + "/.prismalux_office_token";
        QFile f(tokenFile);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
        m_officeBridgeToken = QString::fromUtf8(f.readAll()).trimmed();
    };

    auto _bridgePath = []() -> QString {
        QDir d(QCoreApplication::applicationDirPath());
        for (int i = 0; i < 4; i++) {
            QString p = d.filePath("office_bridge/prismalux_office_bridge.py");
            if (QFile::exists(p)) return p;
            d.cdUp();
        }
        return {};
    };

    connect(m_officeStartBtn, &QPushButton::clicked, this,
            [this, _bridgePath, _readToken]() {
        if (m_officeBridgeProc &&
            m_officeBridgeProc->state() == QProcess::Running) {
            m_officeBridgeProc->terminate();
            m_officeBridgeProc->waitForFinished(2000);
            m_officeStartBtn->setText("\xe2\x96\xb6  Avvia bridge");
            m_officeStatusLbl->setText("\xe2\x9a\xaa  Bridge fermato");
            return;
        }
        const QString path = _bridgePath();
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
                    this, [this](int, QProcess::ExitStatus) {
                m_officeStartBtn->setText("\xe2\x96\xb6  Avvia bridge");
                m_officeStatusLbl->setText("\xe2\x9a\xaa  Bridge fermato");
            });
        }
        m_officeStatusLbl->setText("\xf0\x9f\x94\x84  Avvio bridge...");
        m_officeBridgeProc->start("python3", {path});
        if (m_officeBridgeProc->state() == QProcess::Running) {
            m_officeStartBtn->setText("\xe2\x8f\xb9  Ferma bridge");
            QTimer::singleShot(1200, this, [this, _readToken]() {
                _readToken();
                QNetworkRequest req(QUrl("http://localhost:6790/status"));
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
                            "\xe2\x9c\x85  Pronto: " +
                            (ok.isEmpty() ? "bridge attivo" : ok.join(", ")));
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

    connect(m_officeExecBtn, &QPushButton::clicked, this, [this, _readToken]() {
        if (m_officeCode.isEmpty()) return;
        _readToken();
        QJsonObject payload; payload["code"] = m_officeCode;
        QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);
        QNetworkRequest req(QUrl("http://localhost:6790/execute"));
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json; charset=utf-8");
        req.setRawHeader("Authorization", ("Bearer " + m_officeBridgeToken).toUtf8());
        req.setTransferTimeout(30000);
        m_officeExecBtn->setEnabled(false);
        m_officeStatusLbl->setText("\xf0\x9f\x94\x84  Invio a Office...");
        auto* reply = m_officeNam->post(req, body);
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            reply->deleteLater();
            m_officeExecBtn->setEnabled(true);
            if (reply->error() == QNetworkReply::NoError) {
                QJsonObject res = QJsonDocument::fromJson(reply->readAll()).object();
                if (res["ok"].toBool()) {
                    m_officeStatusLbl->setText("\xe2\x9c\x85  Eseguito");
                    m_officeOutput->append("\n\xe2\x9c\x85  Office: "
                        + res["output"].toString("OK"));
                } else {
                    m_officeStatusLbl->setText("\xe2\x9d\x8c  Errore");
                    m_officeOutput->append("\n\xe2\x9d\x8c  Office errore: "
                        + res["error"].toString());
                }
            } else {
                m_officeStatusLbl->setText("\xe2\x9d\x8c  " + reply->errorString());
            }
        });
    });

    connect(m_officeRunBtn, &QPushButton::clicked, this, [this]() {
        const int idx = m_officeAction->currentIndex();
        if (idx < 0 || !kOfficeSys[idx]) return;
        runAi(2, QString::fromUtf8(kOfficeSys[idx]),
              m_officeInput->toPlainText(),
              m_officeOutput, m_officeRunBtn, m_officeStopBtn,
              m_officeModel);
    });

    connect(m_officeStopBtn, &QPushButton::clicked, this, [this]() {
        m_ai->abort();
        m_officeRunBtn->setEnabled(true);
        m_officeStopBtn->setEnabled(false);
        m_officeOutput->append("\n\xe2\x8f\xb9  Fermato.");
    });

    return w;
}

/* ══════════════════════════════════════════════════════════════
   Tab CLOUDCOMPARE
   ══════════════════════════════════════════════════════════════ */
QWidget* AppControllerPage::buildCloudCompareTab()
{
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(16, 16, 16, 16);
    lay->setSpacing(12);

    auto* group = new QGroupBox(
        "\xf0\x9f\x94\xb5  CloudCompare \xe2\x80\x94 Analisi nuvole di punti", w);
    auto* glay = new QVBoxLayout(group);

    auto* statusLbl = new QLabel(
        "\xe2\x8f\xb3  <b>In sviluppo</b><br>"
        "Il bridge CloudComPy \xc3\xa8 in fase di integrazione.<br><br>"
        "<b>Funzionalit\xc3\xa0 pianificate:</b><br>"
        "\xe2\x80\xa2 Caricamento file LAS / PLY / E57<br>"
        "\xe2\x80\xa2 Calcolo normali e filtraggio statistico<br>"
        "\xe2\x80\xa2 Registrazione ICP tra nuvole<br>"
        "\xe2\x80\xa2 Calcolo distanze Hausdorff<br>"
        "\xe2\x80\xa2 Segmentazione e classificazione AI<br>"
        "\xe2\x80\xa2 Esportazione PLY / LAS / CSV<br><br>"
        "<b>Bridge:</b> CloudComPy (Python wrapper di CloudCompare)<br>"
        "Repository: "
        "<a href='https://github.com/CloudCompare/CloudComPy'>"
        "github.com/CloudCompare/CloudComPy</a>",
        group);
    statusLbl->setWordWrap(true);
    statusLbl->setOpenExternalLinks(true);
    statusLbl->setObjectName("hintLabel");
    glay->addWidget(statusLbl);
    glay->addStretch();

    m_ccOutput = new QTextEdit(w);
    m_ccOutput->setReadOnly(true);
    m_ccOutput->setObjectName("outputView");
    m_ccOutput->setPlaceholderText("Output CloudCompare apparirà qui (prossimamente)...");

    lay->addWidget(group, 1);
    lay->addWidget(m_ccOutput, 1);

    return w;
}
