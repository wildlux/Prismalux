/* ══════════════════════════════════════════════════════════════
   main_app_controller_electronics.cpp — AppControllerPage: Elettronica
   =========================================================================
   Tab KiCAD MCP + TinyMCP (Arduino/ESP32) + OBS Studio — builder + slot.
   detectSerialPorts() posizionata qui (usata da buildTinyMCPTab, era
   fisicamente lontana nel sorgente originale). Split da
   main_app_controller.cpp/main_app_controller_slots.cpp (TODO D-8).
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
#include <QTcpSocket>
#include <QAbstractSocket>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QFile>
#include <QTimer>

namespace P = PrismaluxPaths;

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

    auto* lbl = new QLabel(tr("KiCAD MCP:"), connRow);
    lbl->setObjectName("hintLabel");

    m_kicadHostEdit = new QLineEdit("localhost:3000", connRow);
    m_kicadHostEdit->setFixedWidth(dpiScale(150));

    auto* pingBtn = new QPushButton(tr("\xf0\x9f\x94\x97  Verifica"), connRow);
    pingBtn->setToolTip(tr("Verifica che il server KiCAD MCP sia raggiungibile"));
    pingBtn->setObjectName("actionBtn");
    pingBtn->setFixedWidth(dpiScale(100));

    m_kicadStatusLbl = new QLabel(tr("\xe2\x9a\xaa  Non connesso"), connRow);
    m_kicadStatusLbl->setObjectName("hintLabel");

    m_kicadExecBtn = new QPushButton(
        tr("\xf0\x9f\x96\xa5  Esegui in KiCAD"), connRow);
    m_kicadExecBtn->setObjectName("actionBtn");
    m_kicadExecBtn->setFixedWidth(dpiScale(160));
    m_kicadExecBtn->setEnabled(false);

    connLay->addWidget(lbl);
    connLay->addWidget(m_kicadHostEdit);
    connLay->addWidget(pingBtn);
    auto* kicadHelpBtn = new QPushButton(tr("\xf0\x9f\x9b\x9f  Aiuto"), connRow);
    kicadHelpBtn->setToolTip(tr("Apri la documentazione KiCAD MCP e guida ai comandi"));
    kicadHelpBtn->setObjectName("actionBtn");
    kicadHelpBtn->setFixedWidth(dpiScale(80));
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

    m_kicadModel = new ModelComboBox(m_ai, toolRow);

    toolLay->addWidget(new QLabel(tr("Azione:"), toolRow));
    toolLay->addWidget(m_kicadAction, 1);
    toolLay->addWidget(new QLabel(tr("Modello AI:"), toolRow));
    toolLay->addWidget(m_kicadModel, 1);
    lay->addWidget(toolRow);

    /* ── Input ── */
    m_kicadInput = new QTextEdit(w);
    m_kicadInput->setPlaceholderText(
        "Descrivi il circuito o l'operazione PCB da eseguire...\n"
        "Es: 'Crea un circuito LED con resistore da 470\xce\xa9 alimentato a 5V'\n"
        "Es: 'Posiziona un ESP32 con connettore USB-C e antenna Wi-Fi'");
    m_kicadInput->setFixedHeight(dpiScale(90));
    lay->addWidget(m_kicadInput);

    /* ── Pulsanti ── */
    auto* btnRow = new QWidget(w);
    auto* btnLay = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(0, 0, 0, 0);
    btnLay->setSpacing(8);

    m_kicadRunBtn  = new QPushButton(tr("\xf0\x9f\xa4\x96  Genera script"), btnRow);
    m_kicadRunBtn->setObjectName("actionBtn");
    m_kicadStopBtn = new QPushButton(tr("\xe2\x8f\xb9  Stop"), btnRow);
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
    m_kicadOutput->setPlaceholderText(tr("Script Python KiCAD appari\xc3\xa0 qui..."));
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

    m_kicadStatusLbl->setText(tr("\xf0\x9f\x94\x84  Invio a KiCAD..."));
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

    auto* lbl = new QLabel(tr("Porta MCU:"), connRow);
    lbl->setObjectName("hintLabel");

    m_mcuPort = new QComboBox(connRow);
    m_mcuPort->setMinimumWidth(dpiScale(150));
    m_mcuPort->setEditable(true);

    auto* detectBtn = new QPushButton(tr("\xf0\x9f\x94\x8d  Rileva"), connRow);
    detectBtn->setObjectName("actionBtn");
    detectBtn->setFixedWidth(dpiScale(90));
    detectBtn->setToolTip(tr("Rileva le porte seriali/USB disponibili per MCU/FPGA"));

    m_mcuStatusLbl = new QLabel(tr("\xe2\x9a\xaa  MCU non connesso"), connRow);
    m_mcuStatusLbl->setObjectName("hintLabel");

    m_mcuFlashBtn = new QPushButton(
        tr("\xe2\x9a\xa1  Flash MCU"), connRow);
    m_mcuFlashBtn->setObjectName("actionBtn");
    m_mcuFlashBtn->setFixedWidth(dpiScale(130));
    m_mcuFlashBtn->setEnabled(false);

    connLay->addWidget(lbl);
    connLay->addWidget(m_mcuPort, 1);
    connLay->addWidget(detectBtn);
    auto* mcuHelpBtn = new QPushButton(tr("\xf0\x9f\x9b\x9f  Aiuto"), connRow);
    mcuHelpBtn->setToolTip(tr("Apri guida al flashing MCU/FPGA: Arduino, STM32, ESP32, AVR"));
    mcuHelpBtn->setObjectName("actionBtn");
    mcuHelpBtn->setFixedWidth(dpiScale(80));
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

    /* ── Banner modulo mancante: pyserial (controllo async, non blocca UI) ── */
    {
        auto* banner    = new QWidget(w);
        auto* bannerLay = new QHBoxLayout(banner);
        bannerLay->setContentsMargins(8, 4, 8, 4);
        bannerLay->setSpacing(10);
        banner->setStyleSheet(
            "background:#78350f;border-radius:6px;border:1px solid #f59e0b;");
        banner->hide();

        auto* warnLbl = new QLabel(
            "\xe2\x9a\xa0  Modulo <b>pyserial</b> non installato "
            "\xe2\x80\x94 comunicazione seriale non disponibile.", banner);
        warnLbl->setObjectName("hintLabel");
        warnLbl->setStyleSheet("color:#fcd34d;background:transparent;border:none;");

        auto* goBtn = new QPushButton(
            tr("\xe2\x9a\x99\xef\xb8\x8f  Installa in Impostazioni"), banner);
        goBtn->setObjectName("actionBtn");
        goBtn->setFixedWidth(dpiScale(195));

        bannerLay->addWidget(warnLbl, 1);
        bannerLay->addWidget(goBtn);
        lay->addWidget(banner);

        QObject::connect(goBtn, &QPushButton::clicked, this,
            [this]() { emit openSettingsDipendenze("pyserial"); });

        auto* chk = new QProcess(w);
        QObject::connect(chk, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            banner, [banner](int code, QProcess::ExitStatus) {
                if (code != 0) banner->show();
            });
        chk->start("python3", {"-c", "import serial"});
    }

    /* ── Azione + Scheda + Modello ── */
    auto* toolRow = new QWidget(w);
    auto* toolLay = new QHBoxLayout(toolRow);
    toolLay->setContentsMargins(0, 0, 0, 0);
    toolLay->setSpacing(8);

    m_mcuAction = new QComboBox(toolRow);
    for (int i = 0; kMCUActions[i]; i++)
        m_mcuAction->addItem(QString::fromUtf8(kMCUActions[i]));

    m_mcuBoardCombo = new QComboBox(toolRow);
    m_mcuBoardCombo->setFixedWidth(dpiScale(160));
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

    m_mcuModel = new ModelComboBox(m_ai, toolRow);

    toolLay->addWidget(new QLabel(tr("Tipo:"), toolRow));
    toolLay->addWidget(m_mcuAction, 1);
    toolLay->addWidget(new QLabel(tr("Scheda:"), toolRow));
    toolLay->addWidget(boardCombo);
    toolLay->addWidget(new QLabel(tr("Modello AI:"), toolRow));
    toolLay->addWidget(m_mcuModel, 1);
    lay->addWidget(toolRow);

    /* ── Input ── */
    m_mcuInput = new QTextEdit(w);
    m_mcuInput->setPlaceholderText(
        "Descrivi il programma da generare per il microcontrollore...\n"
        "Es: 'Fai lampeggiare 3 LED in sequenza con intervallo 500ms'\n"
        "Es: 'Leggi temperatura da DHT22 e invia via UART ogni secondo'");
    m_mcuInput->setFixedHeight(dpiScale(90));
    lay->addWidget(m_mcuInput);

    /* ── Pulsanti ── */
    auto* btnRow = new QWidget(w);
    auto* btnLay = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(0, 0, 0, 0);
    btnLay->setSpacing(8);

    m_mcuRunBtn  = new QPushButton(tr("\xf0\x9f\xa4\x96  Genera codice"), btnRow);
    m_mcuRunBtn->setObjectName("actionBtn");
    m_mcuStopBtn = new QPushButton(tr("\xe2\x8f\xb9  Stop"), btnRow);
    m_mcuStopBtn->setObjectName("actionBtn");
    m_mcuStopBtn->setEnabled(false);
    btnLay->addWidget(m_mcuRunBtn);
    btnLay->addWidget(m_mcuStopBtn);
    btnLay->addStretch();
    lay->addWidget(btnRow);

    /* ── Output ── */
    m_mcuOutput = new QTextBrowser(w);
    m_mcuOutput->setReadOnly(true);
    m_mcuOutput->setObjectName("outputView");
    m_mcuOutput->setOpenLinks(false);
    m_mcuOutput->setPlaceholderText(
        "Il codice C++/Python per microcontrollore apparir\xc3\xa0 qui...\n"
        "Dopo la generazione premi 'Flash MCU' per caricare sulla scheda.");
    connect(m_mcuOutput, &QTextBrowser::anchorClicked,
            this, &AppControllerPage::onPipLinkClicked);
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


/* ======================================================================
   Sezione 8 — KiCAD tab slots
   ====================================================================== */

void AppControllerPage::onKicadPingClicked()
{
    const QString host = m_kicadHostEdit->text().trimmed();
    auto* sock = new QTcpSocket(this);
    const QStringList parts = host.split(':');
    const int port = parts.size() > 1 ? parts[1].toInt() : 3000;
    m_kicadStatusLbl->setText(tr("\xf0\x9f\x94\x84  Verifica..."));
    connect(sock, &QTcpSocket::connected, this, [this, sock]() {
        m_kicadStatusLbl->setText(tr("\xe2\x9c\x85  KiCAD MCP Server attivo"));
        sock->disconnectFromHost();
        sock->deleteLater();
    });
    connect(sock, &QTcpSocket::errorOccurred, this,
            [this, sock](QAbstractSocket::SocketError) {
        m_kicadStatusLbl->setText(tr("\xe2\x9d\x8c  KiCAD MCP non raggiungibile"));
        sock->deleteLater();
    });
    sock->connectToHost(parts[0], static_cast<quint16>(port));
}

void AppControllerPage::onKicadExecClicked()
{
    if (m_kicadCode.isEmpty()) return;
    execKiCADAction(m_kicadCode);
}

void AppControllerPage::onKicadRunClicked()
{
    const int idx = m_kicadAction->currentIndex();
    if (idx < 0 || !kKiCADSys[idx]) return;
    runAi(5, QString::fromUtf8(kKiCADSys[idx]),
          m_kicadInput->toPlainText(),
          m_kicadOutput, m_kicadRunBtn, m_kicadStopBtn,
          m_kicadModel);
}

void AppControllerPage::onKicadStopClicked()
{
    m_ai->abort();
    m_kicadRunBtn->setEnabled(true);
    m_kicadStopBtn->setEnabled(false);
    m_kicadOutput->append("\n\xe2\x8f\xb9  Fermato.");
}

void AppControllerPage::onKicadHelpClicked()
{
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(tr("\xf0\x9f\x96\xa5  Installazione KiCAD MCP Server"));
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->resize(540, 460);
    auto* dlay    = new QVBoxLayout(dlg);
    auto* browser = new QTextBrowser(dlg);
    browser->setOpenExternalLinks(true);
    browser->setHtml(
        "<h3>\xf0\x9f\x96\xa5 KiCAD MCP Server</h3>"
        "<h4>1. Installa KiCAD 7+</h4>"
        "<p><code>sudo apt install kicad</code> oppure "
        "<a href='https://www.kicad.org/download/'>kicad.org</a></p>"
        "<h4>2. Installa e avvia il server MCP</h4>"
        "<pre>git clone https://github.com/mixelpixx/KiCAD-MCP-Server\n"
        "cd KiCAD-MCP-Server\n"
        "npm install\n"
        "node server.js</pre>"
        "<p>Il server ascolta sulla porta <b>3000</b>.</p>"
        "<h4>3. Abilita Scripting Console in KiCAD</h4>"
        "<p>PCB Editor \xe2\x86\x92 Strumenti \xe2\x86\x92 <b>Console Scripting</b> "
        "(deve essere abilitata nel build di KiCAD)</p>"
        "<h4>4. Collega</h4>"
        "<p>Torna qui \xe2\x86\x92 clicca <b>\xf0\x9f\x94\x97 Verifica</b>.</p>");
    auto* btnClose = new QPushButton(tr("\xe2\x9c\x95  Chiudi"), dlg);
    btnClose->setObjectName("actionBtn");
    connect(btnClose, &QPushButton::clicked, dlg, &QDialog::accept);
    dlay->addWidget(browser);
    dlay->addWidget(btnClose);
    dlg->exec();
}

void AppControllerPage::onKicadExecReply()
{
    auto* reply = m_kicadPendingReply;
    m_kicadPendingReply = nullptr;
    if (!reply) return;
    reply->deleteLater();
    m_kicadExecBtn->setEnabled(true);
    if (reply->error() == QNetworkReply::NoError) {
        const QJsonObject res = QJsonDocument::fromJson(reply->readAll()).object();
        if (res["ok"].toBool(true)) {
            m_kicadStatusLbl->setText(tr("\xe2\x9c\x85  Eseguito in KiCAD"));
            m_kicadOutput->append("\n\xe2\x9c\x85  KiCAD: "
                + res["output"].toString("OK"));
        } else {
            m_kicadStatusLbl->setText(tr("\xe2\x9d\x8c  Errore KiCAD"));
            m_kicadOutput->append("\n\xe2\x9d\x8c  Errore: "
                + res["error"].toString(reply->errorString()));
        }
    } else {
        m_kicadStatusLbl->setText(tr("\xe2\x9d\x8c  ") + reply->errorString());
        m_kicadOutput->append("\n\xe2\x9d\x8c  " + reply->errorString());
    }
}

/* ======================================================================
   Sezione 9 — TinyMCP tab slots
   ====================================================================== */

void AppControllerPage::onMcuDetectClicked()
{
    detectSerialPorts();
}

void AppControllerPage::onMcuFlashClicked()
{
    if (m_mcuCode.isEmpty()) return;
    const QString port  = m_mcuPort->currentText().trimmed();
    const QString board = m_mcuBoardCombo ? m_mcuBoardCombo->currentText() : QString();
    if (port.isEmpty()) {
        m_mcuOutput->append("\n\xe2\x9a\xa0  Seleziona una porta seriale prima di flashare.");
        return;
    }
    const QString tmpPath = QDir::tempPath() + "/prismalux_mcu_sketch.ino";
    QFile f(tmpPath);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(m_mcuCode.toUtf8());
        f.close();
    }
    m_mcuOutput->append(
        QString("\n\xf0\x9f\x93\x9d  Codice salvato: %1\n"
                "\xe2\x9a\xa1  Per flashare su %2 (porta %3):\n"
                "   arduino-cli compile --fqbn arduino:avr:uno %1\n"
                "   arduino-cli upload -p %3 --fqbn arduino:avr:uno %1\n"
                "   (adatta l'fqbn alla tua scheda)")
            .arg(tmpPath, board, port));
    m_mcuStatusLbl->setText(
        QString("\xf0\x9f\x93\x9d  Salvato \xe2\x80\x94 usa arduino-cli per flashare"));
}

void AppControllerPage::onMcuRunClicked()
{
    const int idx = m_mcuAction->currentIndex();
    if (idx < 0 || !kMCUSys[idx]) return;
    const QString boardName = m_mcuBoardCombo ? m_mcuBoardCombo->currentText() : QString();
    const QString sys = QString::fromUtf8(kMCUSys[idx])
        + QString("\nScheda target: %1").arg(boardName);
    runAi(6, sys,
          m_mcuInput->toPlainText(),
          m_mcuOutput, m_mcuRunBtn, m_mcuStopBtn,
          m_mcuModel);
}

void AppControllerPage::onMcuStopClicked()
{
    m_ai->abort();
    m_mcuRunBtn->setEnabled(true);
    m_mcuStopBtn->setEnabled(false);
    m_mcuOutput->append("\n\xe2\x8f\xb9  Fermato.");
}

void AppControllerPage::onMcuHelpClicked()
{
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(tr("\xe2\x9a\xa1  TinyMCP \xe2\x80\x94 Microcontrollori"));
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->resize(560, 500);
    auto* dlay    = new QVBoxLayout(dlg);
    auto* browser = new QTextBrowser(dlg);
    browser->setOpenExternalLinks(true);
    browser->setHtml(
        "<h3>\xe2\x9a\xa1 TinyMCP \xe2\x80\x94 Microcontrollori</h3>"
        "<h4>Arduino CLI (Arduino Uno / Nano / Mega)</h4>"
        "<pre>sudo apt install arduino-cli\n"
        "arduino-cli core install arduino:avr</pre>"
        "<h4>ESP32 / ESP8266</h4>"
        "<pre>pip install esptool\n"
        "arduino-cli core install esp32:esp32</pre>"
        "<h4>MicroPython (ESP)</h4>"
        "<p>Scarica il firmware da "
        "<a href='https://micropython.org/download/'>micropython.org</a>, poi:</p>"
        "<pre>esptool.py erase_flash\n"
        "esptool.py write_flash 0x1000 firmware.bin</pre>"
        "<h4>Raspberry Pi Pico</h4>"
        "<p>Tieni premuto BOOTSEL \xe2\x86\x92 collega USB \xe2\x86\x92 trascina il file "
        "<code>.uf2</code> nel drive che appare.</p>"
        "<h4>Connessione</h4>"
        "<p>Collega il microcontrollore via USB \xe2\x86\x92 clicca "
        "<b>\xf0\x9f\x94\x8d Rileva</b> \xe2\x86\x92 genera il codice \xe2\x86\x92 "
        "<b>\xe2\x9a\xa1 Flash MCU</b> per salvare e ottenere il comando di upload.</p>");
    auto* btnClose = new QPushButton(tr("\xe2\x9c\x95  Chiudi"), dlg);
    btnClose->setObjectName("actionBtn");
    connect(btnClose, &QPushButton::clicked, dlg, &QDialog::accept);
    dlay->addWidget(browser);
    dlay->addWidget(btnClose);
    dlg->exec();
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
        m_mcuStatusLbl->setText(tr("\xe2\x9a\xaa  Nessun MCU rilevato — connetti via USB"));
    } else {
        for (const auto& p : found)
            m_mcuPort->addItem(p);
        m_mcuStatusLbl->setText(
            QString("\xe2\x9c\x85  %1 porta/e trovata/e").arg(found.size()));
    }
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

    auto* lbl = new QLabel(tr("OBS WebSocket:"), connRow);
    lbl->setObjectName("hintLabel");

    m_obsHostEdit = new QLineEdit("localhost:4455", connRow);
    m_obsHostEdit->setFixedWidth(dpiScale(150));

    auto* pingBtn = new QPushButton(tr("\xf0\x9f\x94\x97  Verifica"), connRow);
    pingBtn->setToolTip(tr("Verifica connessione con OBS WebSocket (porta 4455)"));
    pingBtn->setObjectName("actionBtn");
    pingBtn->setFixedWidth(dpiScale(100));

    m_obsStatusLbl = new QLabel(tr("\xe2\x9a\xaa  Non connesso"), connRow);
    m_obsStatusLbl->setObjectName("hintLabel");

    m_obsExecBtn = new QPushButton(tr("\xf0\x9f\x94\xb4  Esegui in OBS"), connRow);
    m_obsExecBtn->setObjectName("actionBtn");
    m_obsExecBtn->setFixedWidth(dpiScale(150));
    m_obsExecBtn->setEnabled(false);

    auto* obsHelpBtn = new QPushButton(tr("\xf0\x9f\x9b\x9f  Aiuto"), connRow);
    obsHelpBtn->setToolTip(tr("Apri la documentazione OBS WebSocket e guida scene/sorgenti"));
    obsHelpBtn->setObjectName("actionBtn");
    obsHelpBtn->setFixedWidth(dpiScale(80));

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

    /* ── Banner modulo mancante: obsws-python (controllo async, non blocca UI) ── */
    {
        auto* banner    = new QWidget(w);
        auto* bannerLay = new QHBoxLayout(banner);
        bannerLay->setContentsMargins(8, 4, 8, 4);
        bannerLay->setSpacing(10);
        banner->setStyleSheet(
            "background:#78350f;border-radius:6px;border:1px solid #f59e0b;");
        banner->hide();

        auto* warnLbl = new QLabel(
            "\xe2\x9a\xa0  Modulo <b>obsws-python</b> non installato "
            "\xe2\x80\x94 OBS MCP non sar\xc3\xa0 disponibile.", banner);
        warnLbl->setObjectName("hintLabel");
        warnLbl->setStyleSheet("color:#fcd34d;background:transparent;border:none;");

        auto* goBtn = new QPushButton(
            tr("\xe2\x9a\x99\xef\xb8\x8f  Installa in Impostazioni"), banner);
        goBtn->setObjectName("actionBtn");
        goBtn->setFixedWidth(dpiScale(195));

        bannerLay->addWidget(warnLbl, 1);
        bannerLay->addWidget(goBtn);
        lay->addWidget(banner);

        QObject::connect(goBtn, &QPushButton::clicked, this,
            [this]() { emit openSettingsDipendenze("obsws-python"); });

        auto* chk = new QProcess(w);
        QObject::connect(chk, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            banner, [banner](int code, QProcess::ExitStatus) {
                if (code != 0) banner->show();
            });
        chk->start("python3", {"-c", "import obsws_python"});
    }

    /* ── Azione + Modello ── */
    auto* toolRow = new QWidget(w);
    auto* toolLay = new QHBoxLayout(toolRow);
    toolLay->setContentsMargins(0, 0, 0, 0);
    toolLay->setSpacing(8);

    m_obsAction = new QComboBox(toolRow);
    for (int i = 0; kOBSActions[i]; i++)
        m_obsAction->addItem(QString::fromUtf8(kOBSActions[i]));

    m_obsModel = new ModelComboBox(m_ai, toolRow);

    toolLay->addWidget(new QLabel(tr("Azione:"), toolRow));
    toolLay->addWidget(m_obsAction, 1);
    toolLay->addWidget(new QLabel(tr("Modello AI:"), toolRow));
    toolLay->addWidget(m_obsModel, 1);
    lay->addWidget(toolRow);

    /* ── Input ── */
    m_obsInput = new QTextEdit(w);
    m_obsInput->setPlaceholderText(
        "Descrivi cosa vuoi fare in OBS...\n"
        "Es: 'Avvia la registrazione e cambia scena su Gameplay'\n"
        "Es: 'Silenzia il microfono e abbassa il volume del desktop al 50%'");
    m_obsInput->setFixedHeight(dpiScale(90));
    lay->addWidget(m_obsInput);

    /* ── Pulsanti ── */
    auto* btnRow = new QWidget(w);
    auto* btnLay = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(0, 0, 0, 0);
    btnLay->setSpacing(8);

    m_obsRunBtn  = new QPushButton(tr("\xf0\x9f\xa4\x96  Genera script"), btnRow);
    m_obsRunBtn->setObjectName("actionBtn");
    m_obsStopBtn = new QPushButton(tr("\xe2\x8f\xb9  Stop"), btnRow);
    m_obsStopBtn->setObjectName("actionBtn");
    m_obsStopBtn->setProperty("danger", true);
    m_obsStopBtn->setEnabled(false);
    btnLay->addWidget(m_obsRunBtn);
    btnLay->addWidget(m_obsStopBtn);
    btnLay->addStretch();
    lay->addWidget(btnRow);

    /* ── Output ── */
    m_obsOutput = new QTextBrowser(w);
    m_obsOutput->setReadOnly(true);
    m_obsOutput->setObjectName("outputView");
    m_obsOutput->setOpenLinks(false);
    m_obsOutput->setPlaceholderText(tr("Script Python OBS apparir\xc3\xa0 qui..."));
    connect(m_obsOutput, &QTextBrowser::anchorClicked,
            this, &AppControllerPage::onPipLinkClicked);
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

/* ======================================================================
   Sezione 10 — OBS tab slots
   ====================================================================== */

void AppControllerPage::onObsPingClicked()
{
    if (QProcess::execute(P::findPython(), {"-c", "import obsws_python"}) != 0) {
        m_obsStatusLbl->setText(tr("\xe2\x9d\x8c  obsws-python non installato"));
        m_obsOutput->append(
            "<span style='color:#f87171;'>"
            "\xe2\x9d\x8c  Modulo mancante \xe2\x80\x94 clicca per installarlo: "
            "<a href='pip://obsws-python' style='color:#fbbf24;'>"
            "\xf0\x9f\x94\xa7 Installa obsws-python</a>"
            "</span>");
        return;
    }
    const QString addr = m_obsHostEdit->text().trimmed();
    const QString host = addr.contains(':') ? addr.section(':', 0, 0) : addr;
    const int     port = addr.contains(':') ? addr.section(':', 1).toInt() : 4455;
    m_obsStatusLbl->setText(tr("\xf0\x9f\x94\x84  Connessione..."));
    auto* sock = new QTcpSocket(this);
    sock->connectToHost(host, static_cast<quint16>(port));
    connect(sock, &QTcpSocket::connected, this, [this, sock]() {
        sock->disconnectFromHost(); sock->deleteLater();
        m_obsStatusLbl->setText(tr("\xe2\x9c\x85  OBS WebSocket attivo"));
        m_obsExecBtn->setEnabled(!m_obsCode.isEmpty());
    });
    connect(sock, &QAbstractSocket::errorOccurred, this,
            [this, sock](QAbstractSocket::SocketError) {
        m_obsStatusLbl->setText(tr("\xe2\x9d\x8c  ") + sock->errorString());
        sock->deleteLater();
    });
    QPointer<QTcpSocket> sockPtr(sock);
    QTimer::singleShot(3000, this, [sockPtr, this]() {
        if (sockPtr && sockPtr->state() != QAbstractSocket::ConnectedState) {
            m_obsStatusLbl->setText(tr("\xe2\x9d\x8c  Timeout \xe2\x80\x94 OBS non raggiungibile"));
            sockPtr->abort(); sockPtr->deleteLater();
        }
    });
}

void AppControllerPage::onObsExecClicked()
{
    if (m_obsCode.isEmpty()) return;
    const QString tmpPath = QDir::tempPath() + "/prismalux_obs_script.py";
    QFile f(tmpPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    f.write(m_obsCode.toUtf8());
    f.close();

    if (!m_obsExecProc) {
        m_obsExecProc = new QProcess(this);
        m_obsExecProc->setProcessChannelMode(QProcess::MergedChannels);
        connect(m_obsExecProc, &QProcess::readyRead,
                this, &AppControllerPage::onObsProcReadyRead);
        connect(m_obsExecProc,
                QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                this, &AppControllerPage::onObsProcFinished);
    }
    m_obsExecBtn->setEnabled(false);
    m_obsStatusLbl->setText(tr("\xf0\x9f\x94\x84  Esecuzione script..."));
    m_obsExecProc->start(P::findPython(), {tmpPath});
    if (m_obsExecProc->state() == QProcess::NotRunning)
        m_obsStatusLbl->setText(tr("\xe2\x9d\x8c  Python non trovato"));
}

void AppControllerPage::onObsProcReadyRead()
{
    m_obsOutput->append(
        QString::fromUtf8(m_obsExecProc->readAll()).trimmed());
}

void AppControllerPage::onObsProcFinished(int code, QProcess::ExitStatus /*status*/)
{
    m_obsStatusLbl->setText(code == 0
        ? "\xe2\x9c\x85  Script eseguito"
        : "\xe2\x9d\x8c  Script terminato con errore");
    m_obsExecBtn->setEnabled(true);
}

void AppControllerPage::onObsRunClicked()
{
    const int idx = m_obsAction->currentIndex();
    if (idx < 0 || !kOBSSys[idx]) return;
    runAi(7, QString::fromUtf8(kOBSSys[idx]),
          m_obsInput->toPlainText(),
          m_obsOutput, m_obsRunBtn, m_obsStopBtn,
          m_obsModel);
}

void AppControllerPage::onObsStopClicked()
{
    m_ai->abort();
    m_obsRunBtn->setEnabled(true);
    m_obsStopBtn->setEnabled(false);
    m_obsOutput->append("\n\xe2\x8f\xb9  Fermato.");
}

void AppControllerPage::onObsHelpClicked()
{
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(tr("\xf0\x9f\x94\xb4  Installazione OBS MCP"));
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->resize(560, 500);
    auto* dlay    = new QVBoxLayout(dlg);
    auto* browser = new QTextBrowser(dlg);
    browser->setOpenExternalLinks(true);
    browser->setHtml(
        "<h3>\xf0\x9f\x94\xb4 OBS MCP</h3>"
        "<h4>1. Installa OBS Studio</h4>"
        "<p><code>sudo apt install obs-studio</code> oppure "
        "<a href='https://obsproject.com/'>obsproject.com</a></p>"
        "<h4>2. Abilita OBS WebSocket</h4>"
        "<p>OBS \xe2\x86\x92 <b>Strumenti</b> \xe2\x86\x92 <b>WebSocket Server Settings</b> "
        "\xe2\x86\x92 abilita il server (porta <b>4455</b>). "
        "Disabilita la password per uso locale o impostala nel codice.</p>"
        "<h4>3. Installa obsws-python</h4>"
        "<pre>pip install obsws-python</pre>"
        "<h4>4. (Opzionale) Installa obs-mcp</h4>"
        "<p>Il plugin MCP ufficiale: "
        "<a href='https://github.com/royshil/obs-mcp'>github.com/royshil/obs-mcp</a><br>"
        "Permette di controllare OBS da client MCP come Claude Desktop.</p>"
        "<h4>5. Collega</h4>"
        "<p>Avvia OBS \xe2\x86\x92 clicca <b>\xf0\x9f\x94\x97 Verifica</b> \xe2\x86\x92 "
        "genera lo script \xe2\x86\x92 <b>\xf0\x9f\x94\xb4 Esegui in OBS</b>.</p>");
    auto* btnClose = new QPushButton(tr("\xe2\x9c\x95  Chiudi"), dlg);
    btnClose->setObjectName("actionBtn");
    connect(btnClose, &QPushButton::clicked, dlg, &QDialog::accept);
    dlay->addWidget(browser);
    dlay->addWidget(btnClose);
    dlg->exec();
}

