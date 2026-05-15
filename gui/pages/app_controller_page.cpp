#include "app_controller_page.h"
#include "opencode_page.h"
#include "../prismalux_paths.h"
namespace P = PrismaluxPaths;
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
#include <QProgressBar>
#include <QPointer>
#include <QSharedPointer>
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
#include <QDateTime>

/* ──────────────────────────────────────────────────────────────
   System prompt arrays — identici a kSysPrompts[6..9] di StrumentiPage
   ────────────────────────────────────────────────────────────── */
const char* kBlenderSys[] = {
    /* 0 — Crea Primitiva */
    "Sei un esperto di Blender Python API (bpy). Genera SOLO codice Python per aggiungere una primitiva 3D. "
    "REGOLA 1: la prima riga DEVE essere 'import bpy'. "
    "REGOLA 2: usa SEMPRE bpy.ops.mesh.primitive_*_add() — VIETATO costruire mesh da vertici. "
    "Primitive: primitive_cube_add, primitive_uv_sphere_add, primitive_cylinder_add, "
    "primitive_plane_add, primitive_cone_add, primitive_torus_add. "
    "Parametri tipici: size=2, location=(0,0,0). "
    "Non aggiungere variabili 'result'. "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    /* 1 — Cambia Materiale */
    "Sei un esperto di Blender Python API. Genera SOLO codice Python eseguibile in Blender. "
    "Prima riga OBBLIGATORIA: 'import bpy'. "
    "Crea o modifica un materiale Principled BSDF sull'oggetto attivo (bpy.context.active_object). "
    "Imposta base_color, metallic, roughness secondo la richiesta. "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    /* 2 — Trasla */
    "Sei un esperto di Blender Python API. Genera SOLO codice Python eseguibile in Blender. "
    "Prima riga OBBLIGATORIA: 'import bpy'. "
    "Usa bpy.context.active_object.location = (x, y, z). "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    /* 3 — Ruota */
    "Sei un esperto di Blender Python API. Genera SOLO codice Python eseguibile in Blender. "
    "Prima riga OBBLIGATORIA: 'import bpy'. "
    "Usa bpy.context.active_object.rotation_euler = (rx, ry, rz) con angoli in radianti. "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    /* 4 — Scala */
    "Sei un esperto di Blender Python API. Genera SOLO codice Python eseguibile in Blender. "
    "Prima riga OBBLIGATORIA: 'import bpy'. "
    "Usa bpy.context.active_object.scale = (sx, sy, sz). "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    /* 5 — Visibilità */
    "Sei un esperto di Blender Python API. Genera SOLO codice Python eseguibile in Blender. "
    "Prima riga OBBLIGATORIA: 'import bpy'. "
    "Usa obj.hide_viewport e obj.hide_render. "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    /* 6 — Avvia Render */
    "Sei un esperto di Blender Python API. Genera SOLO codice Python eseguibile in Blender. "
    "Prima riga OBBLIGATORIA: 'import bpy'. "
    "Usa bpy.context.scene.render per le impostazioni, bpy.ops.render.render(write_still=True) per avviare. "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    /* 7 — Script libero */
    "Sei un esperto di Blender Python API (bpy). Genera SOLO codice Python eseguibile in Blender. "
    "REGOLA 1: la prima riga DEVE essere 'import bpy' (obbligatorio, altrimenti il codice fallisce). "
    "REGOLA 2: per forme 3D usa bpy.ops.mesh.primitive_*_add() — non costruire mesh da vertici. "
    "Non aggiungere variabili 'result'. "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    nullptr
};

static const char* kBlenderActions[] = {
    "\xf0\x9f\xa7\x8a Crea Primitiva",
    "\xf0\x9f\x8e\xa8 Cambia Materiale",
    "\xe2\x86\x94 Trasla",
    "\xf0\x9f\x94\x84 Ruota",
    "\xf0\x9f\x93\x90 Scala",
    "\xf0\x9f\x91\x81 Visibilit\xc3\xa0",
    "\xf0\x9f\x8e\xac Avvia Render",
    "\xf0\x9f\x90\x8d Script libero",
    nullptr
};

const char* kFreeCADSys[] = {
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
   System prompts — OBS MCP
   ══════════════════════════════════════════════════════════════ */
const char* kOBSSys[] = {
    "Sei un esperto di OBS Studio e obs-websocket 5.x. "
    "Genera SOLO codice Python usando obsws-python: "
    "`import obsws_python as obs; cl = obs.ReqClient(host='localhost', port=4455)`. "
    "Controlla streaming/registrazione con StartStream/StopStream/StartRecord/StopRecord. "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    "Sei un esperto di OBS Studio e obs-websocket 5.x. "
    "Genera SOLO codice Python con obsws-python per cambiare scena. "
    "Usa: cl.set_current_program_scene(scene_name='NomeScena'). "
    "Elenca le scene disponibili con cl.get_scene_list() se necessario. "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    "Sei un esperto di OBS Studio e obs-websocket 5.x. "
    "Genera SOLO codice Python con obsws-python per gestire le sorgenti (source). "
    "Usa cl.set_scene_item_enabled() per visibilit\xc3\xa0, cl.get_input_list() per elenco. "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    "Sei un esperto di OBS Studio e obs-websocket 5.x. "
    "Genera SOLO codice Python con obsws-python per il controllo audio e volume. "
    "Usa cl.set_input_volume(input_name=..., input_volume_mul=0.0..1.0) o input_volume_db. "
    "Muta/smuta con cl.set_input_mute(). "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    "Sei un esperto di OBS Studio e obs-websocket 5.x. "
    "Genera SOLO codice Python con obsws-python per scattare uno screenshot o salvare un replay. "
    "Usa cl.save_source_screenshot() o cl.save_replay_buffer(). "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    "Sei un esperto di OBS Studio e obs-websocket 5.x. "
    "Genera SOLO codice Python con obsws-python eseguibile come script. "
    "import obsws_python as obs; cl = obs.ReqClient(host='localhost', port=4455). "
    "Usa tutta l'API obs-websocket 5.x disponibile. "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    nullptr
};

static const char* kOBSActions[] = {
    "\xf0\x9f\x94\xb4  Streaming & Recording",
    "\xf0\x9f\x8e\xac  Cambia scena",
    "\xf0\x9f\x96\xa5  Gestisci sorgenti",
    "\xf0\x9f\x94\x8a  Volume & Audio",
    "\xf0\x9f\x93\xb8  Screenshot / Replay",
    "\xf0\x9f\x90\x8d  Script libero",
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
    m_tabs->addTab(buildAnkiTab(),         "\xf0\x9f\x83\x8f  Anki MCP");
    m_tabs->addTab(buildKiCADTab(),        "\xf0\x9f\x96\xa5  KiCAD MCP");
    m_tabs->addTab(buildTinyMCPTab(),      "\xf0\x9f\xa4\x96  TinyMCP");
    m_tabs->addTab(buildOBSTab(),          "\xf0\x9f\x94\xb4  OBS MCP");
    m_tabs->addTab(buildGodotTab(),          "\xf0\x9f\x8e\xae  Godot");
    m_tabs->addTab(new OpenCodePage(m_tabs), "\xf0\x9f\x96\xa5  OpenCode");

    lay->addWidget(m_tabs);

    m_aiProgress = new QProgressBar(this);
    m_aiProgress->setRange(0, 0);
    m_aiProgress->setFixedHeight(4);
    m_aiProgress->setTextVisible(false);
    m_aiProgress->setVisible(false);
    lay->addWidget(m_aiProgress);

    m_aiErrorPanel = new AiErrorWidget(this);
    lay->addWidget(m_aiErrorPanel);

    /* Propaga modello corrente a tutte le combo */
    connect(m_ai, &AiClient::modelsReady, this, &AppControllerPage::onModelsReady);
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
    /* Fallback generico: salta il language tag (tutto fino al primo \n) */
    start = text.indexOf("```");
    if (start != -1) {
        start += 3;
        const int nl = text.indexOf('\n', start);
        if (nl != -1) {
            start = nl + 1;
            int end = text.indexOf("```", start);
            if (end != -1) return text.mid(start, end - start).trimmed();
        }
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

    /* Salva stato sessione per gli slot nominati */
    m_runAiTabIdx    = tabIdx;
    m_runAiSys       = sys;
    m_runAiUserMsg   = userMsg;
    m_runAiOutput    = output;
    m_runAiRunBtn    = runBtn;
    m_runAiStopBtn   = stopBtn;
    m_runAiModelCombo = modelCombo;

    m_aiActive  = true;
    m_activeTab = tabIdx;
    runBtn->setEnabled(false);
    stopBtn->setEnabled(true);
    if (m_aiProgress) m_aiProgress->setVisible(true);
    output->append(
        "\n\xf0\x9f\x94\x84  Generazione in corso...\n"
        + QString(40, QChar(0x2500)));

    /* Disconnette connessioni precedenti e ricrea token holder */
    disconnect(m_connToken);
    disconnect(m_connFinished);
    disconnect(m_connError);
    delete m_tokenHolder;
    m_tokenHolder = new QObject(this);

    m_connToken    = connect(m_ai, &AiClient::token,    this, &AppControllerPage::onRunAiToken);
    m_connFinished = connect(m_ai, &AiClient::finished, this, &AppControllerPage::onRunAiFinished);
    m_connError    = connect(m_ai, &AiClient::error,    this, &AppControllerPage::onRunAiError);

    m_ai->chat(sys, userMsg);
}

/* ──────────────────────────────────────────────────────────────
   Slot di runAi — token streaming
   ────────────────────────────────────────────────────────────── */
void AppControllerPage::onRunAiToken(const QString& t)
{
    if (!m_runAiOutput) return;
    m_runAiOutput->moveCursor(QTextCursor::End);
    m_runAiOutput->insertPlainText(t);
}

/* ──────────────────────────────────────────────────────────────
   Slot di runAi — generazione completata
   ────────────────────────────────────────────────────────────── */
void AppControllerPage::onRunAiFinished(const QString& full)
{
    m_aiActive = false;
    if (m_runAiRunBtn)  m_runAiRunBtn->setEnabled(true);
    if (m_runAiStopBtn) m_runAiStopBtn->setEnabled(false);
    if (m_aiProgress)   m_aiProgress->setVisible(false);
    if (m_runAiOutput)  m_runAiOutput->append("\n" + QString(40, QChar(0x2500)));
    /* Disconnette segnali AI e libera token holder */
    disconnect(m_connToken);
    disconnect(m_connFinished);
    disconnect(m_connError);
    if (m_tokenHolder) {
        m_tokenHolder->deleteLater();
        m_tokenHolder = nullptr;
    }

    /* Abilita exec solo se c'era un blocco backtick reale (non testo puro) */
    const bool hasBlock = full.contains("```");
    const QString code = extractCode(full);
    if (m_activeTab == 0 && hasBlock && !code.isEmpty()) {
        const QString model  = m_runAiModelCombo ? m_runAiModelCombo->currentData().toString() : "AI";
        const QString ts     = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm");
        /* Commento singola riga con modello e data */
        const QString header = "# LLM: " + model + "  \xe2\x80\x94  " + ts + "\n";
        /* Garantisce import bpy: senza di esso exec() di Blender restituisce {} */
        const QString imports = code.contains("import bpy") ? "" : "import bpy\n";
        m_blenderCode = code;
        m_blenderCodeEdit->setPlainText(header + imports + code);
        m_blenderStatusLbl->setText(
            "\xf0\x9f\x90\x8d  Codice pronto \xe2\x80\x94 premi Esegui in Blender");
    } else if (m_activeTab == 1 && hasBlock && !code.isEmpty()) {
        m_freecadCode = code;
        m_freecadExecBtn->setEnabled(true);
        m_freecadStatusLbl->setText(
            "\xf0\x9f\x94\xa9  Codice pronto \xe2\x80\x94 premi Esegui in FreeCAD");
    } else if (m_activeTab == 2 && hasBlock && !code.isEmpty()) {
        m_officeCode = code;
        m_officeExecBtn->setEnabled(true);
        m_officeStatusLbl->setText(
            "\xf0\x9f\x93\x84  Codice pronto \xe2\x80\x94 premi Esegui in Office");
    } else if (m_activeTab == 5 && hasBlock && !code.isEmpty()) {
        m_kicadCode = code;
        m_kicadExecBtn->setEnabled(true);
        m_kicadStatusLbl->setText(
            "\xf0\x9f\x96\xa5  Codice pronto \xe2\x80\x94 premi Esegui in KiCAD");
    } else if (m_activeTab == 6 && hasBlock && !code.isEmpty()) {
        m_mcuCode = code;
        m_mcuFlashBtn->setEnabled(true);
        m_mcuStatusLbl->setText(
            "\xf0\x9f\xa4\x96  Codice pronto \xe2\x80\x94 premi Flash MCU");
    } else if (m_activeTab == 7 && hasBlock && !code.isEmpty()) {
        m_obsCode = code;
        m_obsExecBtn->setEnabled(true);
        m_obsStatusLbl->setText(
            "\xf0\x9f\x94\xb4  Codice pronto \xe2\x80\x94 premi Esegui in OBS");
    } else if (m_activeTab == 9 && hasBlock && !code.isEmpty()) {
        m_godotCode = code;
        m_godotExecBtn->setEnabled(true);
        m_godotStatusLbl->setText(
            "\xf0\x9f\x8e\xae  Codice pronto \xe2\x80\x94 premi Esegui in Godot");
    }
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

    auto* descLbl = new QLabel(
        "\xf0\x9f\x8e\xa8  <i>Blender \xe2\x80\x94 Software open-source di modellazione 3D, "
        "animazione, sculpting e rendering. Usato da artisti, designer e sviluppatori di videogiochi.</i>", w);
    descLbl->setObjectName("hintLabel");
    descLbl->setTextFormat(Qt::RichText);
    descLbl->setWordWrap(true);
    lay->addWidget(descLbl);

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
    /* Script libero come default: il modello capisce da solo cosa fare */
    m_blenderAction->setCurrentIndex(m_blenderAction->count() - 1);

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
        "(es. 'Crea un cubo', 'Crea una sfera rossa', 'Sposta il piano a Y=3')...");
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

    /* ── Output AI ── */
    m_blenderOutput = new QTextEdit(w);
    m_blenderOutput->setReadOnly(true);
    m_blenderOutput->setObjectName("outputView");
    m_blenderOutput->setPlaceholderText("Output AI apparirà qui...");
    m_blenderOutput->setMaximumHeight(160);
    lay->addWidget(m_blenderOutput, 0);

    /* ── Editor codice Python (diretto + popolato dall'AI) ── */
    auto* codeLbl = new QLabel(
        "\xf0\x9f\x90\x8d  <b>Codice Python da eseguire in Blender</b> "
        "<span style='color:#888;font-size:11px;'>"
        "(generato dall'AI o scrivi direttamente)</span>", w);
    codeLbl->setTextFormat(Qt::RichText);
    lay->addWidget(codeLbl);
    m_blenderCodeEdit = new QTextEdit(w);
    m_blenderCodeEdit->setObjectName("outputView");
    m_blenderCodeEdit->setPlaceholderText(
        "# Il codice Python generato dall'AI apparirà qui.\n"
        "# Puoi anche scrivere direttamente codice bpy:\n"
        "import bpy\n"
        "bpy.ops.mesh.primitive_cube_add(size=2, location=(0, 0, 0))");
    m_blenderCodeEdit->setFont(QFont("Monospace", 10));
    m_blenderCodeEdit->setMinimumHeight(120);
    lay->addWidget(m_blenderCodeEdit, 1);

    /* ── Connessioni ── */
    m_blenderNam = new QNetworkAccessManager(this);

    connect(m_blenderCodeEdit, &QTextEdit::textChanged,
            this, &AppControllerPage::onBlenderCodeChanged);
    connect(pingBtn,           &QPushButton::clicked,
            this, &AppControllerPage::onBlenderPingClicked);
    connect(m_blenderExecBtn,  &QPushButton::clicked,
            this, &AppControllerPage::onBlenderExecClicked);
    connect(helpBtn,           &QPushButton::clicked,
            this, &AppControllerPage::onBlenderHelpClicked);
    connect(m_blenderRunBtn,   &QPushButton::clicked,
            this, &AppControllerPage::onBlenderRunClicked);
    connect(m_blenderStopBtn,  &QPushButton::clicked,
            this, &AppControllerPage::onBlenderStopClicked);

    return w;
}

/* ══════════════════════════════════════════════════════════════
   blenderSendTcp — helper TCP null-terminated per Blender MCP
   buf è QSharedPointer: safe anche se readyRead spara più volte
   o se timeout e readyRead coincidono — nessun double-free.
   ══════════════════════════════════════════════════════════════ */
void AppControllerPage::blenderSendTcp(const QString& host, int port,
                                       const QByteArray& jsonMsg,
                                       std::function<void(const QJsonObject&, bool)> cb)
{
    auto* sock = new QTcpSocket(this);
    auto  buf  = QSharedPointer<QByteArray>::create();
    /* done: flag one-shot per evitare di chiamare cb due volte */
    auto done  = QSharedPointer<bool>::create(false);
    sock->connectToHost(host, static_cast<quint16>(port));

    connect(sock, &QTcpSocket::connected, sock, [sock, jsonMsg]() {
        sock->write(jsonMsg + '\0');
        sock->flush();
    });
    connect(sock, &QTcpSocket::readyRead, sock, [sock, buf, cb, done]() {
        if (*done) return;
        buf->append(sock->readAll());
        const int nullPos = buf->indexOf('\0');
        if (nullPos < 0) return;          // risposta parziale — aspetta
        *done = true;
        const QByteArray resp = buf->left(nullPos);
        sock->disconnectFromHost();
        sock->deleteLater();
        const QJsonObject obj = QJsonDocument::fromJson(resp).object();
        cb(obj, obj.contains("result") || obj.value("ok").toBool(true));
    });
    connect(sock, &QAbstractSocket::errorOccurred, sock,
            [sock, cb, done](QAbstractSocket::SocketError) {
        if (*done) return;
        *done = true;
        const QString err = sock->errorString();
        sock->deleteLater();
        cb(QJsonObject{{"error", err}}, false);
    });
    QTimer::singleShot(8000, sock, [sock, cb, done]() {
        if (*done) return;
        *done = true;
        sock->abort();
        sock->deleteLater();
        cb(QJsonObject{{"error", "Timeout (8s)"}}, false);
    });
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

    auto* descLbl = new QLabel(
        "\xf0\x9f\x94\xa9  <i>FreeCAD \xe2\x80\x94 Software CAD parametrico open-source per la progettazione "
        "meccanica e industriale 3D. Supporta modellazione solida, simulazione FEM e architettura BIM.</i>", w);
    descLbl->setObjectName("hintLabel");
    descLbl->setTextFormat(Qt::RichText);
    descLbl->setWordWrap(true);
    lay->addWidget(descLbl);

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
    auto* freecadHelpBtn = new QPushButton("\xf0\x9f\x9b\x9f  Aiuto", connRow);
    freecadHelpBtn->setObjectName("actionBtn");
    freecadHelpBtn->setFixedWidth(80);
    connLay->addWidget(m_freecadStatusLbl, 1);
    connLay->addWidget(m_freecadExecBtn);
    connLay->addWidget(freecadHelpBtn);
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
    connect(pingBtn,          &QPushButton::clicked,
            this, &AppControllerPage::onFreecadPingClicked);
    connect(m_freecadExecBtn, &QPushButton::clicked,
            this, &AppControllerPage::onFreecadExecClicked);
    connect(m_freecadRunBtn,  &QPushButton::clicked,
            this, &AppControllerPage::onFreecadRunClicked);
    connect(m_freecadStopBtn, &QPushButton::clicked,
            this, &AppControllerPage::onFreecadStopClicked);
    connect(freecadHelpBtn,   &QPushButton::clicked,
            this, &AppControllerPage::onFreecadHelpClicked);

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
    auto* officeHelpBtn = new QPushButton("\xf0\x9f\x9b\x9f  Aiuto", connRow);
    officeHelpBtn->setObjectName("actionBtn");
    officeHelpBtn->setFixedWidth(80);
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

/* ══════════════════════════════════════════════════════════════
   Tab CLOUDCOMPARE
   ══════════════════════════════════════════════════════════════ */
QWidget* AppControllerPage::buildCloudCompareTab()
{
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(16, 16, 16, 16);
    lay->setSpacing(12);

    auto* descLbl = new QLabel(
        "\xf0\x9f\x94\xb5  <i>CloudCompare \xe2\x80\x94 Software open-source per l\xe2\x80\x99" "elaborazione e l\xe2\x80\x99" "analisi "
        "di nuvole di punti 3D e mesh poligonali. Usato in rilevamento topografico, archeologia e ingegneria civile.</i>", w);
    descLbl->setObjectName("hintLabel");
    descLbl->setTextFormat(Qt::RichText);
    descLbl->setWordWrap(true);
    lay->addWidget(descLbl);

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

    auto* ccHelpBtn = new QPushButton("\xf0\x9f\x9b\x9f  Aiuto — CloudComPy", group);
    ccHelpBtn->setObjectName("actionBtn");
    ccHelpBtn->setFixedWidth(200);
    glay->addWidget(ccHelpBtn, 0, Qt::AlignLeft);
    glay->addStretch();

    connect(ccHelpBtn, &QPushButton::clicked,
            this, &AppControllerPage::onCcHelpClicked);

    m_ccOutput = new QTextEdit(w);
    m_ccOutput->setReadOnly(true);
    m_ccOutput->setObjectName("outputView");
    m_ccOutput->setPlaceholderText("Output CloudCompare apparirà qui (prossimamente)...");

    lay->addWidget(group, 1);
    lay->addWidget(m_ccOutput, 1);

    return w;
}

/* ══════════════════════════════════════════════════════════════
   System prompts — Anki MCP
   ══════════════════════════════════════════════════════════════ */
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
   System prompts — KiCAD MCP
   ══════════════════════════════════════════════════════════════ */
const char* kKiCADSys[] = {
    "Sei un esperto di elettronica e progettazione PCB con KiCAD. "
    "Genera SOLO script Python per KiCAD Scripting Console (pcbnew o schematic). "
    "Usa l'API KiCAD Python: import pcbnew; board = pcbnew.GetBoard(). "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    "Sei un esperto di elettronica. Genera uno schema circuitale testuale (netlist) "
    "o uno script KiCAD Python per creare componenti e connessioni. "
    "Documenta chiaramente i pin e i valori dei componenti. "
    "Rispondi con il blocco di codice tra ``` e ```.",

    "Sei un esperto di PCB layout con KiCAD. "
    "Genera uno script Python per posizionare componenti su un PCB KiCAD. "
    "Usa pcbnew.FOOTPRINT, pcbnew.PCB_TRACK per tracce, pcbnew.ToMM() per conversioni. "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    "Sei un esperto di verifica circuiti. "
    "Analizza lo schema o il layout descritto e identifica: "
    "problemi di routing, conflitti di rete, footprint mancanti, errori DRC tipici. "
    "Fornisci una lista strutturata di problemi e soluzioni.",

    nullptr
};

static const char* kKiCADActions[] = {
    "\xf0\x9f\x96\xa5  Script PCB (Python)",
    "\xf0\x9f\x94\x8c  Schema circuito",
    "\xf0\x9f\x93\x90  Layout componenti",
    "\xf0\x9f\x94\x8d  Analisi DRC",
    nullptr
};

/* ══════════════════════════════════════════════════════════════
   System prompts — TinyMCP (Microcontroller)
   ══════════════════════════════════════════════════════════════ */
const char* kMCUSys[] = {
    "Sei un esperto di microcontrollori Arduino/AVR/ESP32. "
    "Genera SOLO codice C/C++ Arduino completo e compilabile. "
    "Includi: #include necessari, setup(), loop(). "
    "Rispondi SOLO con il blocco codice C++ tra ``` e ```, senza spiegazioni.",

    "Sei un esperto di microcontrollori ESP32/ESP8266 con MicroPython. "
    "Genera SOLO codice MicroPython completo. "
    "Includi import, configurazione pin, loop principale. "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```, senza spiegazioni.",

    "Sei un esperto di comunicazione seriale e protocolli IoT. "
    "Genera codice Arduino per comunicazione UART, I2C, SPI o MQTT. "
    "Documenta il pinout usato nei commenti. "
    "Rispondi SOLO con il blocco codice C++ tra ``` e ```, senza spiegazioni.",

    "Sei un esperto di sensori e attuatori per microcontrollori. "
    "Genera codice Arduino per il sensore/attuatore richiesto. "
    "Indica i pin da usare e la libreria necessaria. "
    "Rispondi SOLO con il blocco codice C++ tra ``` e ```, senza spiegazioni.",

    nullptr
};

static const char* kMCUActions[] = {
    "\xe2\x9a\xa1  Arduino C++ sketch",
    "\xf0\x9f\x90\x8d  MicroPython (ESP)",
    "\xf0\x9f\x93\xa1  Comunicazione seriale / IoT",
    "\xf0\x9f\x94\xa7  Sensori & Attuatori",
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

    auto* lbl = new QLabel("AnkiConnect:", connRow);
    lbl->setObjectName("hintLabel");

    m_ankiHostEdit = new QLineEdit("localhost:8765", connRow);
    m_ankiHostEdit->setFixedWidth(150);

    auto* pingBtn = new QPushButton("\xf0\x9f\x94\x97  Verifica", connRow);
    pingBtn->setObjectName("actionBtn");
    pingBtn->setFixedWidth(100);

    m_ankiStatusLbl = new QLabel("\xe2\x9a\xaa  Non connesso", connRow);
    m_ankiStatusLbl->setObjectName("hintLabel");

    m_ankiSendBtn = new QPushButton(
        "\xf0\x9f\x83\x8f  Invia ad Anki", connRow);
    m_ankiSendBtn->setObjectName("actionBtn");
    m_ankiSendBtn->setFixedWidth(150);
    m_ankiSendBtn->setEnabled(false);

    connLay->addWidget(lbl);
    connLay->addWidget(m_ankiHostEdit);
    connLay->addWidget(pingBtn);
    auto* ankiHelpBtn = new QPushButton("\xf0\x9f\x9b\x9f  Aiuto", connRow);
    ankiHelpBtn->setObjectName("actionBtn");
    ankiHelpBtn->setFixedWidth(80);
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
    m_ankiDeckEdit->setFixedWidth(120);
    m_ankiDeckEdit->setToolTip("Nome deck Anki destinazione");
    auto* deckEdit = m_ankiDeckEdit;  // alias per il layout

    m_ankiModel = new QComboBox(toolRow);
    m_ankiModel->setMinimumWidth(180);
    populateModels(m_ankiModel);

    toolLay->addWidget(new QLabel("Tipo:", toolRow));
    toolLay->addWidget(m_ankiAction, 1);
    toolLay->addWidget(new QLabel("Deck:", toolRow));
    toolLay->addWidget(deckEdit);
    toolLay->addWidget(new QLabel("Modello AI:", toolRow));
    toolLay->addWidget(m_ankiModel, 1);
    lay->addWidget(toolRow);

    /* ── Input ── */
    m_ankiInput = new QTextEdit(w);
    m_ankiInput->setPlaceholderText(
        "Descrivi l'argomento per cui vuoi generare carte Anki...\n"
        "Es: 'Algoritmi di ordinamento (bubble sort, merge sort, quicksort)'\n"
        "Es: 'Vocabolario inglese — verbi irregolari comuni'");
    m_ankiInput->setFixedHeight(90);
    lay->addWidget(m_ankiInput);

    /* ── Pulsanti ── */
    auto* btnRow = new QWidget(w);
    auto* btnLay = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(0, 0, 0, 0);
    btnLay->setSpacing(8);

    m_ankiRunBtn  = new QPushButton("\xf0\x9f\xa4\x96  Genera carte", btnRow);
    m_ankiRunBtn->setObjectName("actionBtn");
    m_ankiStopBtn = new QPushButton("\xe2\x8f\xb9  Stop", btnRow);
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
    m_ankiOutput->setPlaceholderText("Le carte generate appariranno qui in formato JSON...");
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

    m_ankiStatusLbl->setText("\xf0\x9f\x94\x84  Invio carte ad Anki...");
    m_ankiPendingCount = notes.size();
    m_ankiPendingReply = m_ankiNam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(m_ankiPendingReply, &QNetworkReply::finished,
            this, &AppControllerPage::onAnkiAddNotesReply);
}

/* ══════════════════════════════════════════════════════════════
   Tab KICAD MCP
   ══════════════════════════════════════════════════════════════ */
QWidget* AppControllerPage::buildKiCADTab()
{
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(6);

    auto* descLbl = new QLabel(
        "\xf0\x9f\x96\xa5  <i>KiCAD \xe2\x80\x94 Suite EDA (Electronic Design Automation) open-source per la "
        "progettazione di schemi elettrici e circuiti stampati (PCB). Standard de facto per l\xe2\x80\x99" "hardware open-source.</i>", w);
    descLbl->setObjectName("hintLabel");
    descLbl->setTextFormat(Qt::RichText);
    descLbl->setWordWrap(true);
    lay->addWidget(descLbl);

    /* ── Barra connessione ── */
    auto* connRow = new QWidget(w);
    auto* connLay = new QHBoxLayout(connRow);
    connLay->setContentsMargins(0, 0, 0, 0);
    connLay->setSpacing(8);

    auto* lbl = new QLabel("KiCAD MCP:", connRow);
    lbl->setObjectName("hintLabel");

    m_kicadHostEdit = new QLineEdit("localhost:3000", connRow);
    m_kicadHostEdit->setFixedWidth(150);

    auto* pingBtn = new QPushButton("\xf0\x9f\x94\x97  Verifica", connRow);
    pingBtn->setObjectName("actionBtn");
    pingBtn->setFixedWidth(100);

    m_kicadStatusLbl = new QLabel("\xe2\x9a\xaa  Non connesso", connRow);
    m_kicadStatusLbl->setObjectName("hintLabel");

    m_kicadExecBtn = new QPushButton(
        "\xf0\x9f\x96\xa5  Esegui in KiCAD", connRow);
    m_kicadExecBtn->setObjectName("actionBtn");
    m_kicadExecBtn->setFixedWidth(160);
    m_kicadExecBtn->setEnabled(false);

    connLay->addWidget(lbl);
    connLay->addWidget(m_kicadHostEdit);
    connLay->addWidget(pingBtn);
    auto* kicadHelpBtn = new QPushButton("\xf0\x9f\x9b\x9f  Aiuto", connRow);
    kicadHelpBtn->setObjectName("actionBtn");
    kicadHelpBtn->setFixedWidth(80);
    connLay->addWidget(m_kicadStatusLbl, 1);
    connLay->addWidget(m_kicadExecBtn);
    connLay->addWidget(kicadHelpBtn);
    lay->addWidget(connRow);

    /* ── Hint ── */
    auto* hintLbl = new QLabel(
        "\xf0\x9f\x93\xa6 <b>KiCAD MCP Server:</b> "
        "installa e avvia <a href='https://github.com/mixelpixx/KiCAD-MCP-Server'>"
        "KiCAD-MCP-Server</a> (porta 3000). "
        "Richiede KiCAD 7+ con Scripting Console abilitata.", w);
    hintLbl->setObjectName("hintLabel");
    hintLbl->setOpenExternalLinks(true);
    hintLbl->setWordWrap(true);
    lay->addWidget(hintLbl);

    /* ── Azione + Modello ── */
    auto* toolRow = new QWidget(w);
    auto* toolLay = new QHBoxLayout(toolRow);
    toolLay->setContentsMargins(0, 0, 0, 0);
    toolLay->setSpacing(8);

    m_kicadAction = new QComboBox(toolRow);
    for (int i = 0; kKiCADActions[i]; i++)
        m_kicadAction->addItem(QString::fromUtf8(kKiCADActions[i]));

    m_kicadModel = new QComboBox(toolRow);
    m_kicadModel->setMinimumWidth(180);
    populateModels(m_kicadModel);

    toolLay->addWidget(new QLabel("Azione:", toolRow));
    toolLay->addWidget(m_kicadAction, 1);
    toolLay->addWidget(new QLabel("Modello AI:", toolRow));
    toolLay->addWidget(m_kicadModel, 1);
    lay->addWidget(toolRow);

    /* ── Input ── */
    m_kicadInput = new QTextEdit(w);
    m_kicadInput->setPlaceholderText(
        "Descrivi il circuito o l'operazione PCB da eseguire...\n"
        "Es: 'Crea un circuito LED con resistore da 470\xce\xa9 alimentato a 5V'\n"
        "Es: 'Posiziona un ESP32 con connettore USB-C e antenna Wi-Fi'");
    m_kicadInput->setFixedHeight(90);
    lay->addWidget(m_kicadInput);

    /* ── Pulsanti ── */
    auto* btnRow = new QWidget(w);
    auto* btnLay = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(0, 0, 0, 0);
    btnLay->setSpacing(8);

    m_kicadRunBtn  = new QPushButton("\xf0\x9f\xa4\x96  Genera script", btnRow);
    m_kicadRunBtn->setObjectName("actionBtn");
    m_kicadStopBtn = new QPushButton("\xe2\x8f\xb9  Stop", btnRow);
    m_kicadStopBtn->setObjectName("actionBtn");
    m_kicadStopBtn->setEnabled(false);
    btnLay->addWidget(m_kicadRunBtn);
    btnLay->addWidget(m_kicadStopBtn);
    btnLay->addStretch();
    lay->addWidget(btnRow);

    /* ── Output ── */
    m_kicadOutput = new QTextEdit(w);
    m_kicadOutput->setReadOnly(true);
    m_kicadOutput->setObjectName("outputView");
    m_kicadOutput->setPlaceholderText("Script Python KiCAD appari\xc3\xa0 qui...");
    lay->addWidget(m_kicadOutput, 1);

    /* ── NAM per KiCAD ── */
    m_kicadNam = new QNetworkAccessManager(this);

    /* ── Connessioni ── */
    connect(pingBtn,          &QPushButton::clicked,
            this, &AppControllerPage::onKicadPingClicked);
    connect(m_kicadExecBtn,   &QPushButton::clicked,
            this, &AppControllerPage::onKicadExecClicked);
    connect(m_kicadRunBtn,    &QPushButton::clicked,
            this, &AppControllerPage::onKicadRunClicked);
    connect(m_kicadStopBtn,   &QPushButton::clicked,
            this, &AppControllerPage::onKicadStopClicked);
    connect(kicadHelpBtn,     &QPushButton::clicked,
            this, &AppControllerPage::onKicadHelpClicked);

    return w;
}

/* ══════════════════════════════════════════════════════════════
   execKiCADAction — invia script Python al KiCAD MCP Server
   ══════════════════════════════════════════════════════════════ */
void AppControllerPage::execKiCADAction(const QString& code)
{
    if (code.isEmpty()) return;
    const QString host = m_kicadHostEdit->text().trimmed();
    QJsonObject body;
    body["action"] = "execute_script";
    body["code"]   = code;

    QNetworkRequest req(QUrl("http://" + host + "/execute"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setTransferTimeout(15000);

    m_kicadStatusLbl->setText("\xf0\x9f\x94\x84  Invio a KiCAD...");
    m_kicadExecBtn->setEnabled(false);
    m_kicadPendingReply = m_kicadNam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(m_kicadPendingReply, &QNetworkReply::finished,
            this, &AppControllerPage::onKicadExecReply);
}

/* ══════════════════════════════════════════════════════════════
   Tab TINYMCP (Microcontroller)
   ══════════════════════════════════════════════════════════════ */
QWidget* AppControllerPage::buildTinyMCPTab()
{
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(6);

    auto* descLbl = new QLabel(
        "\xf0\x9f\xa4\x96  <i>TinyMCP \xe2\x80\x94 Bridge AI per la programmazione di microcontrollori "
        "Arduino, ESP32 e STM32. Genera codice C/C++ tramite AI, lo compila e lo flasha direttamente sul dispositivo via porta seriale.</i>", w);
    descLbl->setObjectName("hintLabel");
    descLbl->setTextFormat(Qt::RichText);
    descLbl->setWordWrap(true);
    lay->addWidget(descLbl);

    /* ── Barra porta seriale ── */
    auto* connRow = new QWidget(w);
    auto* connLay = new QHBoxLayout(connRow);
    connLay->setContentsMargins(0, 0, 0, 0);
    connLay->setSpacing(8);

    auto* lbl = new QLabel("Porta MCU:", connRow);
    lbl->setObjectName("hintLabel");

    m_mcuPort = new QComboBox(connRow);
    m_mcuPort->setMinimumWidth(150);
    m_mcuPort->setEditable(true);

    auto* detectBtn = new QPushButton("\xf0\x9f\x94\x8d  Rileva", connRow);
    detectBtn->setObjectName("actionBtn");
    detectBtn->setFixedWidth(90);

    m_mcuStatusLbl = new QLabel("\xe2\x9a\xaa  MCU non connesso", connRow);
    m_mcuStatusLbl->setObjectName("hintLabel");

    m_mcuFlashBtn = new QPushButton(
        "\xe2\x9a\xa1  Flash MCU", connRow);
    m_mcuFlashBtn->setObjectName("actionBtn");
    m_mcuFlashBtn->setFixedWidth(130);
    m_mcuFlashBtn->setEnabled(false);

    connLay->addWidget(lbl);
    connLay->addWidget(m_mcuPort, 1);
    connLay->addWidget(detectBtn);
    auto* mcuHelpBtn = new QPushButton("\xf0\x9f\x9b\x9f  Aiuto", connRow);
    mcuHelpBtn->setObjectName("actionBtn");
    mcuHelpBtn->setFixedWidth(80);
    connLay->addWidget(m_mcuStatusLbl, 1);
    connLay->addWidget(m_mcuFlashBtn);
    connLay->addWidget(mcuHelpBtn);
    lay->addWidget(connRow);

    /* ── Hint ── */
    auto* hintLbl = new QLabel(
        "\xf0\x9f\x94\xa7 <b>TinyMCP</b>: genera codice per microcontrollori "
        "(Arduino, ESP32, AVR, STM32). "
        "Flash via <b>avrdude</b> o <b>esptool.py</b> (richiede installazione separata).", w);
    hintLbl->setObjectName("hintLabel");
    hintLbl->setWordWrap(true);
    lay->addWidget(hintLbl);

    /* ── Azione + Scheda + Modello ── */
    auto* toolRow = new QWidget(w);
    auto* toolLay = new QHBoxLayout(toolRow);
    toolLay->setContentsMargins(0, 0, 0, 0);
    toolLay->setSpacing(8);

    m_mcuAction = new QComboBox(toolRow);
    for (int i = 0; kMCUActions[i]; i++)
        m_mcuAction->addItem(QString::fromUtf8(kMCUActions[i]));

    m_mcuBoardCombo = new QComboBox(toolRow);
    m_mcuBoardCombo->setFixedWidth(160);
    m_mcuBoardCombo->addItems({
        "Arduino Uno/Nano",
        "Arduino Mega",
        "ESP32",
        "ESP8266",
        "STM32 (Blue Pill)",
        "Raspberry Pi Pico",
        "ATtiny85"
    });
    auto* boardCombo = m_mcuBoardCombo;  // alias per il layout

    m_mcuModel = new QComboBox(toolRow);
    m_mcuModel->setMinimumWidth(180);
    populateModels(m_mcuModel);

    toolLay->addWidget(new QLabel("Tipo:", toolRow));
    toolLay->addWidget(m_mcuAction, 1);
    toolLay->addWidget(new QLabel("Scheda:", toolRow));
    toolLay->addWidget(boardCombo);
    toolLay->addWidget(new QLabel("Modello AI:", toolRow));
    toolLay->addWidget(m_mcuModel, 1);
    lay->addWidget(toolRow);

    /* ── Input ── */
    m_mcuInput = new QTextEdit(w);
    m_mcuInput->setPlaceholderText(
        "Descrivi il programma da generare per il microcontrollore...\n"
        "Es: 'Fai lampeggiare 3 LED in sequenza con intervallo 500ms'\n"
        "Es: 'Leggi temperatura da DHT22 e invia via UART ogni secondo'");
    m_mcuInput->setFixedHeight(90);
    lay->addWidget(m_mcuInput);

    /* ── Pulsanti ── */
    auto* btnRow = new QWidget(w);
    auto* btnLay = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(0, 0, 0, 0);
    btnLay->setSpacing(8);

    m_mcuRunBtn  = new QPushButton("\xf0\x9f\xa4\x96  Genera codice", btnRow);
    m_mcuRunBtn->setObjectName("actionBtn");
    m_mcuStopBtn = new QPushButton("\xe2\x8f\xb9  Stop", btnRow);
    m_mcuStopBtn->setObjectName("actionBtn");
    m_mcuStopBtn->setEnabled(false);
    btnLay->addWidget(m_mcuRunBtn);
    btnLay->addWidget(m_mcuStopBtn);
    btnLay->addStretch();
    lay->addWidget(btnRow);

    /* ── Output ── */
    m_mcuOutput = new QTextEdit(w);
    m_mcuOutput->setReadOnly(true);
    m_mcuOutput->setObjectName("outputView");
    m_mcuOutput->setPlaceholderText(
        "Il codice C++/Python per microcontrollore apparir\xc3\xa0 qui...\n"
        "Dopo la generazione premi 'Flash MCU' per caricare sulla scheda.");
    lay->addWidget(m_mcuOutput, 1);

    /* ── Connessioni ── */
    connect(detectBtn,      &QPushButton::clicked,
            this, &AppControllerPage::onMcuDetectClicked);
    connect(m_mcuFlashBtn,  &QPushButton::clicked,
            this, &AppControllerPage::onMcuFlashClicked);
    connect(m_mcuRunBtn,    &QPushButton::clicked,
            this, &AppControllerPage::onMcuRunClicked);
    connect(m_mcuStopBtn,   &QPushButton::clicked,
            this, &AppControllerPage::onMcuStopClicked);
    connect(mcuHelpBtn,     &QPushButton::clicked,
            this, &AppControllerPage::onMcuHelpClicked);

    /* Rileva porte al costruttore */
    detectSerialPorts();

    return w;
}

/* ══════════════════════════════════════════════════════════════
   Tab OBS MCP
   ══════════════════════════════════════════════════════════════ */
QWidget* AppControllerPage::buildOBSTab()
{
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(6);

    auto* descLbl = new QLabel(
        "\xf0\x9f\x94\xb4  <i>OBS Studio \xe2\x80\x94 Software open-source per la registrazione video e lo "
        "streaming live su Twitch, YouTube e altri servizi. Controllabile via WebSocket per automazione di scene, sorgenti e filtri.</i>", w);
    descLbl->setObjectName("hintLabel");
    descLbl->setTextFormat(Qt::RichText);
    descLbl->setWordWrap(true);
    lay->addWidget(descLbl);

    /* ── Barra connessione ── */
    auto* connRow = new QWidget(w);
    auto* connLay = new QHBoxLayout(connRow);
    connLay->setContentsMargins(0, 0, 0, 0);
    connLay->setSpacing(8);

    auto* lbl = new QLabel("OBS WebSocket:", connRow);
    lbl->setObjectName("hintLabel");

    m_obsHostEdit = new QLineEdit("localhost:4455", connRow);
    m_obsHostEdit->setFixedWidth(150);

    auto* pingBtn = new QPushButton("\xf0\x9f\x94\x97  Verifica", connRow);
    pingBtn->setObjectName("actionBtn");
    pingBtn->setFixedWidth(100);

    m_obsStatusLbl = new QLabel("\xe2\x9a\xaa  Non connesso", connRow);
    m_obsStatusLbl->setObjectName("hintLabel");

    m_obsExecBtn = new QPushButton("\xf0\x9f\x94\xb4  Esegui in OBS", connRow);
    m_obsExecBtn->setObjectName("actionBtn");
    m_obsExecBtn->setFixedWidth(150);
    m_obsExecBtn->setEnabled(false);

    auto* obsHelpBtn = new QPushButton("\xf0\x9f\x9b\x9f  Aiuto", connRow);
    obsHelpBtn->setObjectName("actionBtn");
    obsHelpBtn->setFixedWidth(80);

    connLay->addWidget(lbl);
    connLay->addWidget(m_obsHostEdit);
    connLay->addWidget(pingBtn);
    connLay->addWidget(m_obsStatusLbl, 1);
    connLay->addWidget(m_obsExecBtn);
    connLay->addWidget(obsHelpBtn);
    lay->addWidget(connRow);

    /* ── Hint ── */
    auto* hintLbl = new QLabel(
        "\xf0\x9f\x94\xb4 <b>OBS MCP:</b> installa "
        "<a href='https://github.com/royshil/obs-mcp'>obs-mcp</a> "
        "e abilita OBS WebSocket (Strumenti \xe2\x86\x92 WebSocket Server, porta <b>4455</b>). "
        "Richiede <code>pip install obsws-python</code>.", w);
    hintLbl->setObjectName("hintLabel");
    hintLbl->setOpenExternalLinks(true);
    hintLbl->setWordWrap(true);
    lay->addWidget(hintLbl);

    /* ── Azione + Modello ── */
    auto* toolRow = new QWidget(w);
    auto* toolLay = new QHBoxLayout(toolRow);
    toolLay->setContentsMargins(0, 0, 0, 0);
    toolLay->setSpacing(8);

    m_obsAction = new QComboBox(toolRow);
    for (int i = 0; kOBSActions[i]; i++)
        m_obsAction->addItem(QString::fromUtf8(kOBSActions[i]));

    m_obsModel = new QComboBox(toolRow);
    m_obsModel->setMinimumWidth(180);
    populateModels(m_obsModel);

    toolLay->addWidget(new QLabel("Azione:", toolRow));
    toolLay->addWidget(m_obsAction, 1);
    toolLay->addWidget(new QLabel("Modello AI:", toolRow));
    toolLay->addWidget(m_obsModel, 1);
    lay->addWidget(toolRow);

    /* ── Input ── */
    m_obsInput = new QTextEdit(w);
    m_obsInput->setPlaceholderText(
        "Descrivi cosa vuoi fare in OBS...\n"
        "Es: 'Avvia la registrazione e cambia scena su Gameplay'\n"
        "Es: 'Silenzia il microfono e abbassa il volume del desktop al 50%'");
    m_obsInput->setFixedHeight(90);
    lay->addWidget(m_obsInput);

    /* ── Pulsanti ── */
    auto* btnRow = new QWidget(w);
    auto* btnLay = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(0, 0, 0, 0);
    btnLay->setSpacing(8);

    m_obsRunBtn  = new QPushButton("\xf0\x9f\xa4\x96  Genera script", btnRow);
    m_obsRunBtn->setObjectName("actionBtn");
    m_obsStopBtn = new QPushButton("\xe2\x8f\xb9  Stop", btnRow);
    m_obsStopBtn->setObjectName("actionBtn");
    m_obsStopBtn->setProperty("danger", true);
    m_obsStopBtn->setEnabled(false);
    btnLay->addWidget(m_obsRunBtn);
    btnLay->addWidget(m_obsStopBtn);
    btnLay->addStretch();
    lay->addWidget(btnRow);

    /* ── Output ── */
    m_obsOutput = new QTextEdit(w);
    m_obsOutput->setReadOnly(true);
    m_obsOutput->setObjectName("outputView");
    m_obsOutput->setPlaceholderText("Script Python OBS apparir\xc3\xa0 qui...");
    lay->addWidget(m_obsOutput, 1);

    /* ── Connessioni ── */
    connect(pingBtn,       &QPushButton::clicked,
            this, &AppControllerPage::onObsPingClicked);
    connect(m_obsExecBtn,  &QPushButton::clicked,
            this, &AppControllerPage::onObsExecClicked);
    connect(m_obsRunBtn,   &QPushButton::clicked,
            this, &AppControllerPage::onObsRunClicked);
    connect(m_obsStopBtn,  &QPushButton::clicked,
            this, &AppControllerPage::onObsStopClicked);
    connect(obsHelpBtn,    &QPushButton::clicked,
            this, &AppControllerPage::onObsHelpClicked);

    return w;
}

/* ══════════════════════════════════════════════════════════════
   detectSerialPorts — scansione /dev/ttyUSB* /dev/ttyACM* (Linux)
   ══════════════════════════════════════════════════════════════ */
void AppControllerPage::detectSerialPorts()
{
    if (!m_mcuPort) return;
    m_mcuPort->clear();

    QStringList found;

#ifdef Q_OS_LINUX
    const QStringList patterns = {"/dev/ttyUSB", "/dev/ttyACM", "/dev/ttyS"};
    for (const auto& pat : patterns) {
        for (int i = 0; i < 8; i++) {
            const QString dev = pat + QString::number(i);
            if (QFile::exists(dev)) found << dev;
        }
    }
#elif defined(Q_OS_WIN)
    for (int i = 1; i <= 16; i++)
        found << QString("COM%1").arg(i);
#elif defined(Q_OS_MAC)
    QDir dev("/dev");
    const auto entries = dev.entryList({"cu.usbmodem*","cu.usbserial*","cu.SLAB*"});
    for (const auto& e : entries)
        found << "/dev/" + e;
#endif

    if (found.isEmpty()) {
        m_mcuPort->addItem("(nessuna porta rilevata)");
        m_mcuStatusLbl->setText("\xe2\x9a\xaa  Nessun MCU rilevato — connetti via USB");
    } else {
        for (const auto& p : found)
            m_mcuPort->addItem(p);
        m_mcuStatusLbl->setText(
            QString("\xe2\x9c\x85  %1 porta/e trovata/e").arg(found.size()));
    }
}
