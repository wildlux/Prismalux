#include "manutenzione_page.h"
#include "../prismalux_paths.h"
namespace P = PrismaluxPaths;
#include <QSettings>
#include <QBrush>
#include <QColor>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QGroupBox>
#include <QProcess>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextEdit>
#include <QFileDialog>
#include <QFileInfo>
#include <QTimer>
#include <QTextCursor>
#include <QFrame>

/* ══════════════════════════════════════════════════════════════
   ManutenzioneePage — costruttore minimale.
   Il layout reale vive in buildBackend() / buildConfigFmt() /
   buildHardware(), esposti come tab piatte da ImpostazioniPage.
   ══════════════════════════════════════════════════════════════ */
ManutenzioneePage::ManutenzioneePage(AiClient* ai, HardwareMonitor* hw, QWidget* parent)
    : QWidget(parent), m_ai(ai), m_hw(hw)
{
    /* Applica la modalità calcolo SUBITO via variabile di processo (impostata
     * in main() prima della creazione di qualsiasi componente).
     * L'env var PRISMALUX_COMPUTE_MODE è la fonte unica e non soffre di
     * race condition con hw-detect.
     *   cpu   → num_gpu=0  (forza RAM)
     *   gpu   → num_gpu=-2 provvisorio; updateHWLabel() affinerà con i layer reali
     *   misto → num_gpu=-3 sentinella; updateHWLabel() userà gpuLayersMisto reali */
    const QByteArray envMode = qgetenv("PRISMALUX_COMPUTE_MODE");
    if (!envMode.isEmpty() && m_ai) {
        if      (envMode == "cpu")   m_ai->setNumGpu(0);
        else if (envMode == "gpu")   m_ai->setNumGpu(-2);   /* provvisorio */
        else if (envMode == "misto") m_ai->setNumGpu(-3);   /* sentinella misto */
    }
}

/* ── Stile condiviso per i QGroupBox ─────────────────────────── */
static const char* GRP_STYLE =
    "QGroupBox { border:1px solid #1e2040; border-radius:6px; "
    "margin-top:12px; color:#8a8fb0; font-size:12px; }"
    "QGroupBox::title { subcontrol-origin:margin; left:10px; padding:0 4px; }";

/* ══════════════════════════════════════════════════════════════
   buildBackend() — Backend AI + selezione modello
   Layout a 2 colonne: sinistra = connessione/modello, destra = config/server
   ══════════════════════════════════════════════════════════════ */
QWidget* ManutenzioneePage::buildBackend()
{
    auto* page    = new QWidget;
    auto* mainLay = new QVBoxLayout(page);
    mainLay->setContentsMargins(16, 14, 16, 14);
    mainLay->setSpacing(12);

    /* ── 2 colonne ── */
    auto* colsRow = new QWidget(page);
    auto* colsLay = new QHBoxLayout(colsRow);
    colsLay->setContentsMargins(0, 0, 0, 0);
    colsLay->setSpacing(16);

    /* ══════════════════════════════════════════════════════════
       Colonna sinistra — Connessione & Modello
       ══════════════════════════════════════════════════════════ */
    auto* leftGroup = new QGroupBox("\xf0\x9f\x94\x8c  Connessione & Modello", colsRow);
    leftGroup->setObjectName("cardGroup");
    leftGroup->setFixedWidth(270);
    auto* leftLay = new QVBoxLayout(leftGroup);
    leftLay->setSpacing(6);

    leftLay->addWidget(new QLabel("Backend:", leftGroup));
    m_cmbBackend = new QComboBox(leftGroup);
    m_cmbBackend->addItem(QString("\xf0\x9f\x90\xb3  Ollama  (:%1)").arg(P::kOllamaPort));
    m_cmbBackend->addItem(QString("\xf0\x9f\xa6\x99  llama-server  (:%1)").arg(P::kLlamaServerPort));
    leftLay->addWidget(m_cmbBackend);

    m_hostEdit = new QLineEdit("127.0.0.1", leftGroup);
    m_hostEdit->setObjectName("chatInput");
    m_hostEdit->setPlaceholderText("Host");
    leftLay->addWidget(new QLabel("Host:", leftGroup));
    leftLay->addWidget(m_hostEdit);

    m_portEdit = new QLineEdit("11434", leftGroup);
    m_portEdit->setObjectName("chatInput");
    m_portEdit->setPlaceholderText("Porta");
    leftLay->addWidget(new QLabel("Porta:", leftGroup));
    leftLay->addWidget(m_portEdit);

    auto* applyBtn = new QPushButton("Applica \xe2\x96\xb6", leftGroup);
    applyBtn->setObjectName("actionBtn");
    leftLay->addWidget(applyBtn);

    auto* sepL = new QFrame(leftGroup);
    sepL->setFrameShape(QFrame::HLine);
    sepL->setObjectName("sidebarSep");
    leftLay->addWidget(sepL);

    leftLay->addWidget(new QLabel("Modello:", leftGroup));
    m_cmbModel = new QComboBox(leftGroup);
    m_cmbModel->addItem("(nessun modello \xe2\x80\x94 backend non raggiungibile)");
    leftLay->addWidget(m_cmbModel);

    auto* mdBtnRow = new QWidget(leftGroup);
    auto* mdBtnL   = new QHBoxLayout(mdBtnRow);
    mdBtnL->setContentsMargins(0, 0, 0, 0);
    mdBtnL->setSpacing(6);
    auto* refreshBtn  = new QPushButton("\xf0\x9f\x94\x84", leftGroup);
    refreshBtn->setObjectName("actionBtn");
    refreshBtn->setFixedWidth(36);
    refreshBtn->setToolTip("Aggiorna lista modelli");
    auto* setModelBtn = new QPushButton("\xe2\x9c\x93  Usa questo", leftGroup);
    setModelBtn->setObjectName("actionBtn");
    mdBtnL->addWidget(refreshBtn);
    mdBtnL->addWidget(setModelBtn, 1);
    leftLay->addWidget(mdBtnRow);

    leftLay->addStretch(1);
    colsLay->addWidget(leftGroup);

    /* ══════════════════════════════════════════════════════════
       Colonna destra — Config formato + avvia llama-server
       ══════════════════════════════════════════════════════════ */
    auto* rightGroup = new QGroupBox("\xe2\x9a\x99\xef\xb8\x8f  Configurazione Avanzata", colsRow);
    rightGroup->setObjectName("cardGroup");
    auto* rightLay = new QVBoxLayout(rightGroup);
    rightLay->setSpacing(8);

    /* ── Formato Config ── */
    auto* fmtTitle = new QLabel("\xf0\x9f\x93\x84  <b>Formato Config</b>  (~/.prismalux_config)", rightGroup);
    fmtTitle->setObjectName("cardTitle");
    fmtTitle->setTextFormat(Qt::RichText);
    rightLay->addWidget(fmtTitle);

    auto* fmtDesc = new QLabel(
        "<b>JSON</b>: standard, leggibile dai tool esterni.&nbsp;"
        "<b>TOON</b>: flat <code>chiave: valore</code>, -12% dimensione.",
        rightGroup);
    fmtDesc->setObjectName("cardDesc");
    fmtDesc->setWordWrap(true);
    rightLay->addWidget(fmtDesc);

    auto* fmtRow  = new QWidget(rightGroup);
    auto* fmtRowL = new QHBoxLayout(fmtRow);
    fmtRowL->setContentsMargins(0, 0, 0, 0);
    fmtRowL->setSpacing(8);
    fmtRowL->addWidget(new QLabel("Formato:", fmtRow));
    m_cmbFmt = new QComboBox(fmtRow);
    m_cmbFmt->addItem("JSON  (.prismalux_config.json)", QString("json"));
    m_cmbFmt->addItem("TOON  (.prismalux_config.toon)", QString("toon"));
    {
        QString cur = detectConfigFmt();
        int idx = m_cmbFmt->findData(cur);
        if (idx >= 0) m_cmbFmt->setCurrentIndex(idx);
    }
    auto* fmtApply = new QPushButton("Converti \xe2\x96\xb6", fmtRow);
    fmtApply->setObjectName("actionBtn");
    m_fmtStatus = new QLabel("", fmtRow);
    m_fmtStatus->setObjectName("cardDesc");
    fmtRowL->addWidget(m_cmbFmt, 1);
    fmtRowL->addWidget(fmtApply);
    fmtRowL->addWidget(m_fmtStatus, 1);
    rightLay->addWidget(fmtRow);

    /* ── separatore ── */
    auto* srvSep = new QFrame(rightGroup);
    srvSep->setFrameShape(QFrame::HLine);
    srvSep->setObjectName("sidebarSep");
    rightLay->addWidget(srvSep);

    /* ── Avvia llama-server (visibile solo con llama-server selezionato) ── */
    m_grpServ = new QGroupBox("\xf0\x9f\xa6\x99  llama.cpp \xe2\x80\x94 Avvia llama-server", rightGroup);
    auto* grpServ = m_grpServ;
    grpServ->setVisible(false);
    grpServ->setStyleSheet(GRP_STYLE);
    auto* srvLay = new QVBoxLayout(grpServ);
    srvLay->setSpacing(8);

    /* Riga modello */
    auto* srvModelRow = new QWidget(grpServ);
    auto* srvModelL   = new QHBoxLayout(srvModelRow);
    srvModelL->setContentsMargins(0,0,0,0); srvModelL->setSpacing(8);
    m_srvModelPath = new QLineEdit(grpServ);
    m_srvModelPath->setObjectName("chatInput");
    m_srvModelPath->setPlaceholderText("percorso/al/modello.gguf");
    auto* srvBrowse = new QPushButton("\xe2\x80\xa6", grpServ);
    srvBrowse->setObjectName("actionBtn");
    srvBrowse->setFixedWidth(32);
    srvModelL->addWidget(new QLabel("Modello:", srvModelRow));
    srvModelL->addWidget(m_srvModelPath, 1);
    srvModelL->addWidget(srvBrowse);
    srvLay->addWidget(srvModelRow);

    /* Riga porta + pulsanti */
    auto* srvCtrlRow = new QWidget(grpServ);
    auto* srvCtrlL   = new QHBoxLayout(srvCtrlRow);
    srvCtrlL->setContentsMargins(0,0,0,0); srvCtrlL->setSpacing(8);
    m_srvPort = new QLineEdit(QString::number(P::kLlamaServerPort), grpServ);
    m_srvPort->setObjectName("chatInput");
    m_srvPort->setFixedWidth(70);
    m_srvStartBtn = new QPushButton("\xe2\x96\xb6  Avvia", grpServ);
    m_srvStartBtn->setObjectName("actionBtn");
    m_srvStopBtn = new QPushButton("\xe2\x96\xa0  Stop", grpServ);
    m_srvStopBtn->setObjectName("actionBtn");
    m_srvStopBtn->setProperty("danger","true");
    m_srvStopBtn->setEnabled(false);
    srvCtrlL->addWidget(new QLabel("Porta:", srvCtrlRow));
    srvCtrlL->addWidget(m_srvPort);
    srvCtrlL->addSpacing(12);
    srvCtrlL->addWidget(m_srvStartBtn);
    srvCtrlL->addWidget(m_srvStopBtn);
    srvCtrlL->addStretch(1);
    srvLay->addWidget(srvCtrlRow);

    /* Log output server */
    m_srvLog = new QTextEdit(grpServ);
    m_srvLog->setReadOnly(true);
    m_srvLog->setObjectName("chatLog");
    m_srvLog->setFixedHeight(90);
    m_srvLog->setPlaceholderText("Log llama-server...");
    srvLay->addWidget(m_srvLog);

    rightLay->addWidget(grpServ);
    rightLay->addStretch(1);

    colsLay->addWidget(rightGroup, 1);
    mainLay->addWidget(colsRow, 1);

    /* ── Connessioni Avvia server ── */
    connect(srvBrowse, &QPushButton::clicked, this, &ManutenzioneePage::onSrvBrowseClicked);
    connect(m_srvStartBtn, &QPushButton::clicked, this, &ManutenzioneePage::onSrvStartClicked);
    connect(m_srvStopBtn,  &QPushButton::clicked, this, &ManutenzioneePage::onSrvStopClicked);

    /* ── Connessioni Config ── */
    connect(fmtApply, &QPushButton::clicked, this, &ManutenzioneePage::onFmtApplyClicked);

    /* ── Connessioni Backend ── */
    connect(applyBtn,   &QPushButton::clicked, this, &ManutenzioneePage::onApplyBtnClicked);
    connect(refreshBtn, &QPushButton::clicked, m_ai, &AiClient::fetchModels);
    connect(m_ai, &AiClient::modelsReady, this, &ManutenzioneePage::onBackendModelsReady);
    connect(m_ai, &AiClient::error,       this, &ManutenzioneePage::onBackendModelsFetchError);
    connect(setModelBtn, &QPushButton::clicked, this, &ManutenzioneePage::onSetModelBtnClicked);
    connect(m_cmbBackend, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ManutenzioneePage::onBackendCmbChanged);

    /* ══════════════════════════════════════════════════════════
       Sezione: Aggiornamento Modelli & Info GPU/RAM
       ══════════════════════════════════════════════════════════ */
    auto* updGroup = new QGroupBox(
        "\xf0\x9f\x94\x84  Aggiornamento Modelli & GPU/RAM", page);
    updGroup->setObjectName("cardGroup");
    auto* updLay = new QVBoxLayout(updGroup);
    updLay->setSpacing(6);
    updLay->setContentsMargins(10, 14, 10, 8);

    /* ── Versione Ollama ── */
    auto* verRow = new QWidget(updGroup);
    auto* verLay = new QHBoxLayout(verRow);
    verLay->setContentsMargins(0, 0, 0, 0);
    verLay->setSpacing(8);
    m_verLbl = new QLabel("\xf0\x9f\x90\xb3  Ollama: <i>verifica in corso...</i>", verRow);
    m_verLbl->setObjectName("cardDesc");
    m_verLbl->setTextFormat(Qt::RichText);
    verLay->addWidget(m_verLbl, 1);
    auto* verBtn = new QPushButton("\xf0\x9f\x94\x8d  Verifica", verRow);
    verBtn->setObjectName("actionBtn");
    verBtn->setFixedWidth(90);
    verLay->addWidget(verBtn);
    updLay->addWidget(verRow);

    /* ── GPU vs RAM hint ── */
    m_ramStatusLbl = new QLabel("", updGroup);
    m_ramStatusLbl->setObjectName("cardDesc");
    m_ramStatusLbl->setWordWrap(true);
    m_ramStatusLbl->setTextFormat(Qt::RichText);
    updLay->addWidget(m_ramStatusLbl);

    /* ── Pulsanti aggiornamento modelli ── */
    auto* btnRow = new QWidget(updGroup);
    auto* btnRowL = new QHBoxLayout(btnRow);
    btnRowL->setContentsMargins(0, 0, 0, 0);
    btnRowL->setSpacing(8);
    m_updAllBtn = new QPushButton(
        "\xe2\xac\x87  Aggiorna tutti i modelli Ollama", btnRow);
    m_updAllBtn->setObjectName("actionBtn");
    m_updLlamaBtn = new QPushButton(
        "\xe2\xac\x87  Aggiorna tutti i modelli llama.cpp", btnRow);
    m_updLlamaBtn->setObjectName("actionBtn");
    m_updStatusLbl = new QLabel("", btnRow);
    m_updStatusLbl->setObjectName("cardDesc");
    m_updStatusLbl->setWordWrap(true);
    btnRowL->addWidget(m_updAllBtn);
    btnRowL->addWidget(m_updLlamaBtn);
    btnRowL->addWidget(m_updStatusLbl, 1);
    updLay->addWidget(btnRow);

    /* ── Log aggiornamento (compatto) ── */
    m_updLog = new QTextEdit(updGroup);
    m_updLog->setReadOnly(true);
    m_updLog->setObjectName("chatLog");
    m_updLog->setFixedHeight(100);
    m_updLog->setPlaceholderText("Premi \"Aggiorna tutti\" per scaricare le ultime versioni dei modelli Ollama.");
    updLay->addWidget(m_updLog);

    mainLay->addWidget(updGroup, 0);

    connect(verBtn, &QPushButton::clicked, this, &ManutenzioneePage::onVerifyOllamaVersion);
    connect(m_updAllBtn,   &QPushButton::clicked, this, &ManutenzioneePage::onUpdAllBtnClicked);
    connect(m_updLlamaBtn, &QPushButton::clicked, this, &ManutenzioneePage::onUpdLlamaBtnClicked);

    /* Verifica versione Ollama subito all'apertura */
    QTimer::singleShot(200, this, &ManutenzioneePage::onVerifyOllamaVersion);

    return page;
}

/* ══════════════════════════════════════════════════════════════
   buildConfigFmt() — Formato file di configurazione (JSON/TOON)
   ══════════════════════════════════════════════════════════════ */
QWidget* ManutenzioneePage::buildConfigFmt()
{
    auto* page   = new QWidget;
    auto* cfgLay = new QVBoxLayout(page);
    cfgLay->setContentsMargins(16, 14, 16, 14);
    cfgLay->setSpacing(14);

    auto* grpFmt = new QGroupBox("\xf0\x9f\x93\x84  Formato Config", page);
    grpFmt->setStyleSheet(GRP_STYLE);
    auto* fmtLay = new QVBoxLayout(grpFmt);
    fmtLay->setSpacing(8);

    auto* fmtDesc = new QLabel(
        "Formato del file di configurazione <b>~/.prismalux_config</b>.<br>"
        "<b>JSON</b>: standard, leggibile dai tool esterni.<br>"
        "<b>TOON</b>: flat <code>chiave: valore</code>, -12% dimensione, stessa velocit\xc3\xa0.",
        grpFmt);
    fmtDesc->setObjectName("cardDesc");
    fmtDesc->setWordWrap(true);
    fmtLay->addWidget(fmtDesc);

    auto* fmtRow  = new QWidget(grpFmt);
    auto* fmtRowL = new QHBoxLayout(fmtRow);
    fmtRowL->setContentsMargins(0,0,0,0); fmtRowL->setSpacing(10);

    fmtRowL->addWidget(new QLabel("Formato:", fmtRow));
    m_cmbFmt = new QComboBox(fmtRow);
    m_cmbFmt->addItem("JSON  (.prismalux_config.json)", QString("json"));
    m_cmbFmt->addItem("TOON  (.prismalux_config.toon)", QString("toon"));
    {
        QString cur = detectConfigFmt();
        int idx = m_cmbFmt->findData(cur);
        if (idx >= 0) m_cmbFmt->setCurrentIndex(idx);
    }

    auto* fmtApply = new QPushButton("Converti e salva \xe2\x96\xb6", fmtRow);
    fmtApply->setObjectName("actionBtn");

    m_fmtStatus = new QLabel("", fmtRow);
    m_fmtStatus->setObjectName("cardDesc");

    fmtRowL->addWidget(m_cmbFmt, 1);
    fmtRowL->addWidget(fmtApply);
    fmtRowL->addWidget(m_fmtStatus, 1);
    fmtLay->addWidget(fmtRow);

    connect(fmtApply, &QPushButton::clicked, this, &ManutenzioneePage::onFmtApplyClicked);

    cfgLay->addWidget(grpFmt);
    cfgLay->addStretch(1);

    return page;
}

/* ══════════════════════════════════════════════════════════════
   buildHardware() — Info Hardware + Ottimizzazione RAM
   Layout a 2 colonne: sinistra = info hw, destra = ottimizzazione RAM
   ══════════════════════════════════════════════════════════════ */
QWidget* ManutenzioneePage::buildHardware()
{
    auto* page     = new QWidget;
    auto* mainLay  = new QVBoxLayout(page);
    mainLay->setContentsMargins(16, 14, 16, 14);
    mainLay->setSpacing(12);

#ifndef Q_OS_WIN
    /* Banner zRAM — mostrato da updateHWLabel() se RAM libera < 20% */
    m_zramWarnLbl = new QLabel(
        "\xe2\x9a\xa0  <b>RAM libera bassa (&lt;20%)</b> \xe2\x80\x94 "
        "attiva zRAM per guadagnare ~30-40% di memoria effettiva:<br>"
        "<code>sudo systemctl enable --now systemd-zram-setup@zram0</code><br>"
        "Oppure usa i pulsanti <b>Abilita zRAM</b> qui sotto \xe2\x86\x93", page);
    m_zramWarnLbl->setTextFormat(Qt::RichText);
    m_zramWarnLbl->setWordWrap(true);
    m_zramWarnLbl->setStyleSheet(
        "background:#6b3a00; color:#ffe0a0; "
        "border:1px solid #c07800; border-radius:5px; padding:8px;");
    m_zramWarnLbl->hide();
    mainLay->addWidget(m_zramWarnLbl);
#endif

    /* ── 2 colonne ── */
    auto* colsRow = new QWidget(page);
    auto* colsLay = new QHBoxLayout(colsRow);
    colsLay->setContentsMargins(0, 0, 0, 0);
    colsLay->setSpacing(16);

    /* ══════════════════════════════════════════════════════════
       Colonna sinistra — Info Hardware
       ══════════════════════════════════════════════════════════ */
    auto* leftGroup = new QGroupBox("\xf0\x9f\x96\xa5  Info Hardware", colsRow);
    leftGroup->setObjectName("cardGroup");
    leftGroup->setFixedWidth(220);
    auto* leftLay = new QVBoxLayout(leftGroup);

    m_hwLabel = new QLabel("\xe2\x8f\xb3  Rilevamento hardware in corso...", leftGroup);
    m_hwLabel->setObjectName("cardDesc");
    m_hwLabel->setWordWrap(true);
    m_hwLabel->setStyleSheet("font-family:'Consolas','Courier New',monospace; "
                              "color:#a0a4c0; padding:4px;");
    leftLay->addWidget(m_hwLabel);
    leftLay->addStretch(1);
    colsLay->addWidget(leftGroup);

    /* ══════════════════════════════════════════════════════════
       Colonna destra — Ottimizzazione RAM
       Linux  : zRAM con algoritmo zstd (Meta/Facebook).
       Windows: Memory Compression integrata (Enable-MMAgent).
       ══════════════════════════════════════════════════════════ */
    auto* rightGroup = new QGroupBox("\xf0\x9f\x92\xbe  Ottimizzazione RAM", colsRow);
    rightGroup->setObjectName("cardGroup");
    auto* ramLay = new QVBoxLayout(rightGroup);
    ramLay->setSpacing(8);

    /* Alias per retrocompatibilità con i lambda e connessioni sotto */
    auto* grpRam = rightGroup;

    m_ramStatusLbl = new QLabel(
        "Premi \"\xf0\x9f\x94\x8d Rileva\" per controllare lo stato della compressione RAM.",
        rightGroup);
    m_ramStatusLbl->setObjectName("cardDesc");
    m_ramStatusLbl->setWordWrap(true);
    ramLay->addWidget(m_ramStatusLbl);

    auto* ramDesc = new QLabel(
#ifdef Q_OS_WIN
        "<b>Windows</b>: Memory Compression \xc3\xa8 integrata nel sistema "
        "(Windows 10/11). Attivandola il kernel comprime automaticamente "
        "le pagine RAM poco usate. Non richiede riavvio.",
#else
        "<b>Linux</b>: zRAM crea un dispositivo swap compresso in RAM. "
        "Algoritmo <b>zstd</b> (Meta/Facebook). "
        "<i>Singola</i>: 1 device, 50% RAM, lz4. "
        "<i>Doppia</i>: compatta + 2 device zstd (75% RAM).",
#endif
        rightGroup);
    ramDesc->setObjectName("cardDesc");
    ramDesc->setWordWrap(true);
    ramLay->addWidget(ramDesc);

#ifndef Q_OS_WIN
    /* ── Checkbox auto-avvio zRAM Doppia ── */
    auto* autoZramRow = new QWidget(rightGroup);
    auto* autoZramLay = new QHBoxLayout(autoZramRow);
    autoZramLay->setContentsMargins(0, 0, 0, 0);
    autoZramLay->setSpacing(8);
    auto* autoZramCb = new QCheckBox(
        "\xf0\x9f\x92\xbe\xf0\x9f\x92\xbe  Abilita Doppia zstd automaticamente all'avvio", rightGroup);
    autoZramCb->setObjectName("cardDesc");
    {
        QSettings s("Prismalux", "GUI");
        autoZramCb->setChecked(s.value(P::SK::kAutoZramDoppia, true).toBool());
    }
    autoZramLay->addWidget(autoZramCb);
    autoZramLay->addStretch();
    ramLay->addWidget(autoZramRow);
    connect(autoZramCb, &QCheckBox::toggled, this, &ManutenzioneePage::onAutoZramCbToggled);
#endif

    auto* btnRow = new QWidget(rightGroup);
    auto* btnL   = new QHBoxLayout(btnRow);
    btnL->setContentsMargins(0,0,0,0); btnL->setSpacing(8);

    auto* detectBtn = new QPushButton("\xf0\x9f\x94\x8d  Rileva stato", grpRam);
    detectBtn->setObjectName("actionBtn");
    btnL->addWidget(detectBtn);

#ifdef Q_OS_WIN
    auto* compBtn    = new QPushButton("\xf0\x9f\x92\xbe  Attiva compressione", grpRam);
    compBtn->setObjectName("actionBtn");
    auto* disableBtn = new QPushButton("\xe2\x8f\xb9  Disattiva", grpRam);
    disableBtn->setObjectName("actionBtn");
    btnL->addWidget(compBtn);
    btnL->addWidget(disableBtn);
#else
    auto* singBtn    = new QPushButton("\xf0\x9f\x92\xbe  Singola (lz4)", grpRam);
    singBtn->setObjectName("actionBtn");
    auto* doppiaBtn  = new QPushButton("\xf0\x9f\x92\xbe\xf0\x9f\x92\xbe  Doppia (zstd)", grpRam);
    doppiaBtn->setObjectName("actionBtn");
    auto* disableBtn = new QPushButton("\xe2\x8f\xb9  Disattiva zRAM", grpRam);
    disableBtn->setObjectName("actionBtn");
    btnL->addWidget(singBtn);
    btnL->addWidget(doppiaBtn);
    btnL->addWidget(disableBtn);
#endif
    btnL->addStretch(1);
    ramLay->addWidget(btnRow);

    m_ramLog = new QTextEdit(grpRam);
    m_ramLog->setReadOnly(true);
    m_ramLog->setMaximumHeight(110);
    m_ramLog->setStyleSheet(
        "font-family:'Consolas','Courier New',monospace; font-size:10px;");
    m_ramLog->setPlaceholderText("Output comandi...");
    ramLay->addWidget(m_ramLog);

    colsLay->addWidget(grpRam, 1);
    mainLay->addWidget(colsRow);

    /* ══════════════════════════════════════════════════════════════
       Pannello Modalità Calcolo LLM — full width
       ══════════════════════════════════════════════════════════════ */
    auto* computeGroup = new QGroupBox(
        "\xf0\x9f\x92\xbb  Modalit\xc3\xa0 Calcolo LLM", page);
    computeGroup->setObjectName("cardGroup");
    auto* compLay = new QVBoxLayout(computeGroup);
    compLay->setSpacing(8);

    auto* compDesc = new QLabel(
        "Scegli dove eseguire il modello. "
        "Il default viene rilevato automaticamente confrontando RAM e VRAM.", computeGroup);
    compDesc->setObjectName("cardDesc");
    compDesc->setWordWrap(true);
    compLay->addWidget(compDesc);

    auto* btnRow2 = new QWidget(computeGroup);
    auto* btnL2   = new QHBoxLayout(btnRow2);
    btnL2->setContentsMargins(0, 0, 0, 0);
    btnL2->setSpacing(10);

    m_btnGpu    = new QPushButton("\xf0\x9f\x9a\x80  GPU  (VRAM)", computeGroup);
    m_btnCpu    = new QPushButton("\xf0\x9f\x96\xa5  CPU  (RAM)",  computeGroup);
    m_btnMisto  = new QPushButton("\xe2\x9a\x96\xef\xb8\x8f  Misto GPU+CPU",  computeGroup);
    m_btnDoppia = new QPushButton("\xf0\x9f\x94\x97  Doppia GPU", computeGroup);
    m_btnGpu->setObjectName("actionBtn");
    m_btnCpu->setObjectName("actionBtn");
    m_btnMisto->setObjectName("actionBtn");
    m_btnDoppia->setObjectName("actionBtn");
    m_btnGpu->setMinimumWidth(140);
    m_btnCpu->setMinimumWidth(140);
    m_btnMisto->setMinimumWidth(140);
    m_btnDoppia->setMinimumWidth(140);
    m_btnGpu->setToolTip(
        "Tutti i layer su GPU dedicata (NVIDIA/AMD).\n"
        "Massima velocit\xc3\xa0 se il modello entra in VRAM.\n"
        "num_gpu = layer count reale del modello.");
    m_btnCpu->setToolTip(
        "Tutti i layer su CPU (RAM di sistema).\n"
        "Pi\xc3\xb9 lento, nessun consumo di VRAM.\n"
        "num_gpu = 0.");
    m_btnMisto->setToolTip(
        "Riempie la GPU dedicata al massimo della sua VRAM,\n"
        "i layer rimanenti vanno su CPU/RAM.\n"
        "num_gpu = min(layer_model, layer_capacity_NVIDIA).");
    m_btnDoppia->setToolTip(
        "GPU dedicata (NVIDIA) + Intel iGPU insieme.\n"
        "Richiede llama-server compilato con CUDA+SYCL.\n"
        "Con Ollama: usa solo NVIDIA (Intel iGPU ignorata da CUDA).");
    m_btnDoppia->setEnabled(false);   /* abilitato solo se iGPU Intel rilevata */

    btnL2->addWidget(m_btnGpu);
    btnL2->addWidget(m_btnCpu);
    btnL2->addWidget(m_btnMisto);
    btnL2->addWidget(m_btnDoppia);
    btnL2->addStretch(1);
    compLay->addWidget(btnRow2);

    m_computeInfo = new QLabel(
        "\xe2\x8f\xb3  In attesa rilevamento hardware...", computeGroup);
    m_computeInfo->setObjectName("cardDesc");
    m_computeInfo->setWordWrap(true);
    compLay->addWidget(m_computeInfo);

    /* Riga salva */
    auto* saveRow  = new QWidget(computeGroup);
    auto* saveRowL = new QHBoxLayout(saveRow);
    saveRowL->setContentsMargins(0, 4, 0, 0);
    saveRowL->setSpacing(10);

    m_btnSaveMode = new QPushButton(
        "\xf0\x9f\x92\xbe  Salva modalit\xc3\xa0", computeGroup);
    m_btnSaveMode->setObjectName("actionBtn");
    m_btnSaveMode->setEnabled(false);
    m_btnSaveMode->setToolTip(
        "Applica la modalit\xc3\xa0 selezionata e la salva per i prossimi avvii.");
    saveRowL->addWidget(m_btnSaveMode);
    saveRowL->addStretch(1);
    compLay->addWidget(saveRow);

    mainLay->addWidget(computeGroup);
    mainLay->addStretch(1);

    /* Bottoni → apply+persist immediato (nessun passaggio "Salva" necessario) */
    connect(m_btnGpu,    &QPushButton::clicked, this, &ManutenzioneePage::onBtnGpuClicked);
    connect(m_btnCpu,    &QPushButton::clicked, this, &ManutenzioneePage::onBtnCpuClicked);
    connect(m_btnMisto,  &QPushButton::clicked, this, &ManutenzioneePage::onBtnMistoClicked);
    connect(m_btnDoppia, &QPushButton::clicked, this, &ManutenzioneePage::onBtnDoppiaClicked);
    connect(m_btnSaveMode, &QPushButton::clicked, this, &ManutenzioneePage::onBtnSaveModeClicked);

    /* Ri-applica al cambio modello per ricalcolare num_gpu con i layer reali */
    connect(m_ai, &AiClient::modelChanged, this, &ManutenzioneePage::onAiModelChangedApplyMode);

#ifdef Q_OS_WIN
    connect(detectBtn,  &QPushButton::clicked, this, &ManutenzioneePage::onDetectBtnClicked);
    connect(compBtn,    &QPushButton::clicked, this, &ManutenzioneePage::onCompBtnClicked);
    connect(disableBtn, &QPushButton::clicked, this, &ManutenzioneePage::onDisableRamBtnClicked);
#else
    connect(detectBtn,  &QPushButton::clicked, this, &ManutenzioneePage::onDetectBtnClicked);
    connect(singBtn,    &QPushButton::clicked, this, &ManutenzioneePage::onSingBtnClicked);
    connect(doppiaBtn,  &QPushButton::clicked, this, &ManutenzioneePage::onDoppiaBtnClicked);
    connect(disableBtn, &QPushButton::clicked, this, &ManutenzioneePage::onDisableRamBtnClicked);
#endif

    /* ── NPU (Neural Processing Unit) ── */
    {
        auto* npuGroup = new QGroupBox(
            "\xf0\x9f\xa7\xa0  NPU \xe2\x80\x94 Neural Processing Unit", page);
        npuGroup->setObjectName("cardGroup");
        auto* npuLay = new QVBoxLayout(npuGroup);

        auto* npuDesc = new QLabel(
            "Le NPU accelerano l\xe2\x80\x99"
            "inferenza AI con consumo energetico ridotto rispetto a GPU/CPU.\n"
            "Supportate: <b>Intel NPU</b> (Core Ultra) \xe2\x80\x94 "
            "<b>AMD NPU</b> (Ryzen AI \xe2\x80\x94 beta).", npuGroup);
        npuDesc->setWordWrap(true);
        npuDesc->setTextFormat(Qt::RichText);
        npuDesc->setObjectName("hintLabel");
        npuLay->addWidget(npuDesc);

        /* Rilevamento automatico */
        auto* npuStatusLbl = new QLabel("\xe2\x8f\xb3  Rilevamento in corso...", npuGroup);
        npuStatusLbl->setObjectName("cardDesc");
        npuStatusLbl->setWordWrap(true);
        npuLay->addWidget(npuStatusLbl);

        /* Installa Intel NPU */
        auto* btnIntelNpu = new QPushButton(
            "\xf0\x9f\x94\xb5  Installa intel-npu-acceleration-library", npuGroup);
        btnIntelNpu->setObjectName("actionBtn");
        btnIntelNpu->setToolTip(
            "pip install intel-npu-acceleration-library\n"
            "Richiede: Intel Core Ultra (Meteor Lake+) con driver NPU");
        npuLay->addWidget(btnIntelNpu, 0, Qt::AlignLeft);

        auto* npuHint = new QLabel(
            "\xe2\x84\xb9  AMD NPU (XDNA): usa <b>https://github.com/amd/iron</b> "
            "\xe2\x80\x94 attualmente in beta, stabilit\xc3\xa0 non garantita.\n"
            "Intel NPU: stabile, richiede <code>pip install intel-npu-acceleration-library</code>.",
            npuGroup);
        npuHint->setWordWrap(true);
        npuHint->setTextFormat(Qt::RichText);
        npuHint->setObjectName("hintLabel");
        npuLay->addWidget(npuHint);

        mainLay->addWidget(npuGroup);

        /* Rilevamento asincrono */
        auto detectNpu = [npuStatusLbl]() {
            QStringList found;
#ifdef Q_OS_LINUX
            if (QFile::exists("/dev/accel/accel0"))
                found << "\xe2\x9c\x85  Intel NPU rilevata (/dev/accel/accel0)";
            /* Controlla se il modulo XDNA (AMD NPU) è caricato */
            QFile modules("/proc/modules");
            if (modules.open(QFile::ReadOnly)) {
                const QString content = modules.readAll();
                if (content.contains("amdxdna"))
                    found << "\xe2\x9c\x85  AMD NPU rilevata (modulo amdxdna)";
            }
#endif
            if (found.isEmpty())
                npuStatusLbl->setText(
                    "\xe2\x9d\x8c  Nessuna NPU rilevata (o driver non installato)");
            else
                npuStatusLbl->setText(found.join("\n"));
        };
        detectNpu();

        connect(btnIntelNpu, &QPushButton::clicked, this, &ManutenzioneePage::onBtnIntelNpuClicked);
    }

    return page;
}

/* ══════════════════════════════════════════════════════════════
   onHWReady / updateHWLabel
   ══════════════════════════════════════════════════════════════ */
void ManutenzioneePage::onHWReady(const HWInfo& hw) {
    updateHWLabel(hw);
}

void ManutenzioneePage::updateHWLabel(const HWInfo& hw) {
    if (!m_hwLabel) return;

    /* ── Reset valori VRAM tracciati ── */
    m_nvidiaVramMb = 0;
    m_igpuVramMb   = 0;
    long cpuRamMb  = 0;

    /* ── Costruisce testo con 3 tipologie distinte ── */
    QString txt;
    for (int i = 0; i < hw.count; i++) {
        const HWDevice& d = hw.dev[i];

        if (d.type == DEV_CPU) {
            cpuRamMb = d.mem_mb;
            txt += QString("[CPU]  %1\n"
                           "       RAM %2 MB  (libera %3 MB)\n\n")
                   .arg(d.name).arg(d.mem_mb).arg(d.avail_mb);

        } else if (d.type == DEV_INTEL) {
            m_igpuVramMb = d.mem_mb;   /* apertura GGTT */
            txt += "[Intel GPU integrato]  " + QString(d.name) + "\n";
            if (d.mem_mb > 0)
                txt += QString("       Apertura VRAM %1 MB  "
                               "(condivisa con RAM — stolen memory dal BIOS)\n\n")
                       .arg(d.mem_mb);
            else
                txt += "       VRAM condivisa con RAM  "
                       "(dimensione configurable nel BIOS, tipicamente 512 MB-2 GB)\n\n";

        } else {
            /* GPU dedicata: NVIDIA, AMD, Apple */
            if (i == hw.secondary) m_nvidiaVramMb = d.avail_mb;
            const bool isMain = (i == hw.secondary);
            txt += QString("[%1]  %2%3\n"
                           "       VRAM %4 MB  (usabile %5 MB,  max layer stimati: %6)\n\n")
                   .arg(hw_dev_type_str(d.type)).arg(d.name)
                   .arg(isMain ? "  \xe2\x86\x90 GPU per inferenza" : "")
                   .arg(d.mem_mb).arg(d.avail_mb).arg(d.n_gpu_layers);
        }
    }

    /* Riga riepilogo VRAM combinata (solo se iGPU + GPU dedicata entrambe presenti) */
    if (m_nvidiaVramMb > 0 && m_igpuVramMb > 0) {
        const long combined = m_nvidiaVramMb + m_igpuVramMb;
        txt += QString("── VRAM combinata: %1 MB  (NVIDIA/AMD %2 MB + Intel %3 MB) ──\n"
                       "   Budget netto (tolti KV-cache ~200 MB + RAG ~270 MB): ~%4 MB\n")
               .arg(combined).arg(m_nvidiaVramMb).arg(m_igpuVramMb)
               .arg(qMax(0LL, combined - 470LL));
    } else if (m_nvidiaVramMb > 0) {
        txt += QString("── Budget modello (GPU: %1 MB - KV ~200 MB - RAG ~270 MB): ~%2 MB ──\n")
               .arg(m_nvidiaVramMb)
               .arg(qMax(0LL, m_nvidiaVramMb - 470LL));
    }

    if (txt.isEmpty()) txt = "Nessun dispositivo rilevato.";
    m_hwLabel->setText(txt.trimmed());

#ifndef Q_OS_WIN
    if (m_zramWarnLbl) {
        bool lowRam = false;
        for (int i = 0; i < hw.count; i++) {
            if (hw.dev[i].type == DEV_CPU && hw.dev[i].mem_mb > 0) {
                lowRam = (hw.dev[i].avail_mb * 100 / hw.dev[i].mem_mb) < 20;
                break;
            }
        }
        m_zramWarnLbl->setVisible(lowRam);
    }
#endif

    /* ── Consiglio GPU vs RAM ── */
    if (m_ramStatusLbl && cpuRamMb > 0) {
        QString advice;
        const long combined = m_nvidiaVramMb + m_igpuVramMb;
        if (m_nvidiaVramMb == 0 && m_igpuVramMb == 0) {
            advice = "\xf0\x9f\x92\xbb  <b>Solo CPU:</b> nessuna GPU dedicata rilevata. "
                     "RAM disponibile: %1 MB.";
            advice = advice.arg(cpuRamMb);
        } else if (m_nvidiaVramMb > 0 && m_igpuVramMb > 0) {
            advice = "\xf0\x9f\x94\x97  <b>Doppia GPU:</b> NVIDIA/AMD %1 MB + Intel iGPU %2 MB = "
                     "<b>%3 MB combinati</b>.<br>"
                     "Con Ollama usa GPU (NVIDIA) o Misto. "
                     "Per sfruttare entrambe: <b>Doppia GPU</b> via llama-server CUDA+SYCL.";
            advice = advice.arg(m_nvidiaVramMb).arg(m_igpuVramMb).arg(combined);
        } else if (m_nvidiaVramMb >= cpuRamMb) {
            advice = "\xf0\x9f\x9a\x80  <b>GPU consigliata:</b> VRAM %1 MB \xe2\x89\xa5 RAM %2 MB. "
                     "Tutti i modelli piccoli/medi entrano in VRAM.";
            advice = advice.arg(m_nvidiaVramMb).arg(cpuRamMb);
        } else {
            advice = "\xe2\x9a\x96\xef\xb8\x8f  VRAM %1 MB &lt; RAM %2 MB. "
                     "Usa <b>GPU</b> per modelli che entrano in VRAM, "
                     "<b>Misto</b> per modelli pi\xc3\xb9 grandi.";
            advice = advice.arg(m_nvidiaVramMb).arg(cpuRamMb);
        }
        m_ramStatusLbl->setText(advice);
    }

    /* ── Auto-detect modalità calcolo ── */
    if (m_btnGpu) {
        /* GPU dedicata: mai Intel iGPU da sola per inferenza Ollama */
        const bool hasDedicated = (hw.secondary >= 0
                                   && hw.dev[hw.secondary].type != DEV_INTEL);
        const bool hasIgpu      = (hw.igpu >= 0);

        if (hasDedicated) {
            m_gpuLayersFull = hw.dev[hw.secondary].n_gpu_layers;
        } else {
            m_gpuLayersFull = 0;
        }

        /* GPU e Misto: sempre cliccabili — se non c'è GPU dedicata Ollama usa CPU */
        m_btnGpu->setEnabled(true);
        m_btnMisto->setEnabled(true);
        /* Doppia GPU: attiva solo se iGPU Intel + GPU dedicata entrambe presenti */
        m_btnDoppia->setEnabled(hasDedicated && hasIgpu);

        QSettings s("Prismalux", "GUI");
        QString mode = s.value(P::SK::kComputeMode, "").toString();
        if (mode.isEmpty()) {
#ifdef Q_OS_WIN
            mode = "cpu";
#else
            mode = hasDedicated ? "gpu" : "cpu";
#endif
        }
        /* Se la modalità salvata era "doppia" ma iGPU non c'è più, degrada a gpu */
        if (mode == "doppia" && !hasIgpu) mode = "gpu";

        applyComputeMode(mode);
    }
}

/* ══════════════════════════════════════════════════════════════
   selectComputeMode — evidenzia la selezione senza salvare
   Abilita il pulsante "Salva" per la conferma esplicita.
   ══════════════════════════════════════════════════════════════ */
void ManutenzioneePage::selectComputeMode(const QString& mode)
{
    if (!m_btnGpu) return;
    m_selectedMode = mode;

    /* Evidenzia bottone selezionato (anteprima) */
    auto highlight = [](QPushButton* btn, bool active) {
        if (btn) btn->setStyleSheet(active
            ? "font-weight:bold; border:2px solid #ffa726;"   /* arancione = non salvato */
            : "");
    };
    highlight(m_btnGpu,    mode == "gpu");
    highlight(m_btnCpu,    mode == "cpu");
    highlight(m_btnMisto,  mode == "misto");
    highlight(m_btnDoppia, mode == "doppia");

    if (m_computeInfo) {
        QString preview;
        if (mode == "gpu")
            preview = "\xf0\x9f\x9f\xa0  <b>GPU selezionata</b> — tutti i layer su NVIDIA/AMD. "
                      "Premi Salva per applicare.";
        else if (mode == "cpu")
            preview = "\xf0\x9f\x9f\xa0  <b>CPU selezionata</b> — tutti i layer su RAM. "
                      "Premi Salva per applicare.";
        else if (mode == "misto")
            preview = "\xf0\x9f\x9f\xa0  <b>Misto selezionato</b> — riempie NVIDIA al massimo, "
                      "layer in eccesso su CPU/RAM. Premi Salva per applicare.";
        else if (mode == "doppia") {
            const long combined = m_nvidiaVramMb + m_igpuVramMb;
            if (combined > 0)
                preview = QString("\xf0\x9f\x9f\xa0  <b>Doppia GPU selezionata</b> — "
                                  "NVIDIA %1 MB + Intel iGPU %2 MB = %3 MB combinati.<br>"
                                  "Premi Salva per le istruzioni specifiche per il tuo backend.")
                          .arg(m_nvidiaVramMb).arg(m_igpuVramMb).arg(combined);
            else
                preview = "\xf0\x9f\x9f\xa0  <b>Doppia GPU selezionata</b> — "
                          "Premi Salva per le istruzioni.";
        }
        m_computeInfo->setText(preview);
    }

    if (m_btnSaveMode) m_btnSaveMode->setEnabled(true);
}

/* ══════════════════════════════════════════════════════════════
   applyComputeMode — salva su QSettings e applica ad AiClient
   Chiamato da "Salva modalità" oppure al boot per ripristinare.
   ATTENZIONE: Ollama NON clipa num_gpu al layer count reale —
   un valore troppo alto (999) causa "memory layout cannot be allocated".
   Usare sempre la stima da hw_detect (n_gpu_layers = avail_mb/80).
   ══════════════════════════════════════════════════════════════ */
void ManutenzioneePage::applyComputeMode(const QString& mode)
{
    if (!m_btnGpu) return;
    m_selectedMode = mode;

    /* Persiste su QSettings e aggiorna la variabile di processo: fonte unica
     * per tutti i componenti (incluso AiClient) durante tutta la sessione. */
    {
        QSettings s("Prismalux", "GUI");
        s.setValue(P::SK::kComputeMode, mode);
    }
    qputenv("PRISMALUX_COMPUTE_MODE", mode.toUtf8());

    /* Evidenzia bottone salvato (bordo azzurro = salvato) */
    auto highlight = [](QPushButton* btn, bool active) {
        if (btn) btn->setStyleSheet(active
            ? "font-weight:bold; border:2px solid #4fc3f7;"
            : "");
    };
    highlight(m_btnGpu,    mode == "gpu");
    highlight(m_btnCpu,    mode == "cpu");
    highlight(m_btnMisto,  mode == "misto");
    highlight(m_btnDoppia, mode == "doppia");
    if (m_btnSaveMode) m_btnSaveMode->setEnabled(false);

    if (!m_ai) return;

    if (mode == "gpu") {
        /* Recupera layer count reale via /api/show — Ollama NON clipa num_gpu,
         * un valore > layer count reale causa ISE 500. */
        if (m_computeInfo)
            m_computeInfo->setText(
                "\xe2\x8f\xb3  <b>GPU</b>: recupero layer count dal modello...");

        m_ai->fetchModelLayers([this](int layers) {
            if (!m_ai) return;
            m_ai->unloadModel();
            m_ai->setNumGpu(layers > 0 ? layers : -2);
            if (m_computeInfo)
                m_computeInfo->setText(layers > 0
                    ? QString("\xe2\x9c\x85  <b>GPU (NVIDIA/AMD)</b> — tutti i %1 layer su VRAM "
                              "(num_gpu=%1). Ricaricato alla prossima richiesta.")
                        .arg(layers)
                    : "\xe2\x9c\x85  <b>GPU salvata</b> — Ollama auto-rileva (CUDA/ROCm). "
                      "Ricaricato alla prossima richiesta.");
        });

    } else if (mode == "cpu") {
        m_ai->unloadModel();
        m_ai->setNumGpu(0);
        if (m_computeInfo)
            m_computeInfo->setText(
                "\xe2\x9c\x85  <b>CPU</b> — tutti i layer su RAM (num_gpu=0). "
                "Ricaricato su CPU alla prossima richiesta.");

    } else if (mode == "misto") {
        /* Misto ottimizzato: riempie la GPU dedicata al massimo della VRAM disponibile.
         * num_gpu = min(layer_count_modello, layer_capacity_NVIDIA).
         * I layer in eccesso vanno su CPU/RAM — nessun errore ISE. */
        if (m_computeInfo)
            m_computeInfo->setText(
                "\xe2\x8f\xb3  <b>Misto</b>: recupero layer count dal modello...");

        m_ai->fetchModelLayers([this](int layers) {
            if (!m_ai) return;
            /* Riempie NVIDIA al massimo: min(layer modello, capacit\xc3\xa0 VRAM NVIDIA).
             * Fallback conservativo se layers=0 (modello non caricato): 8 layer. */
            const int capacity = (m_gpuLayersFull > 0) ? m_gpuLayersFull : 8;
            const int gpuLayers = (layers > 0) ? qMin(layers, capacity) : 8;
            const int total     = (layers > 0) ? layers : 16;
            m_ai->unloadModel();
            m_ai->setNumGpu(gpuLayers);
            if (m_computeInfo) {
                if (gpuLayers >= total)
                    m_computeInfo->setText(
                        QString("\xe2\x9c\x85  <b>Misto</b> — tutti i %1 layer su GPU "
                                "(il modello entra interamente in VRAM: valuta modalit\xc3\xa0 GPU pura).")
                        .arg(gpuLayers));
                else
                    m_computeInfo->setText(
                        QString("\xe2\x9c\x85  <b>Misto</b> — %1/%2 layer su GPU (NVIDIA, num_gpu=%1), "
                                "%3 layer su CPU/RAM. Ricaricato alla prossima richiesta.")
                        .arg(gpuLayers).arg(total).arg(total - gpuLayers));
            }
        });

    } else if (mode == "doppia") {
        /* Doppia GPU: NVIDIA + Intel iGPU.
         * Con Ollama: CUDA non vede la Intel iGPU — usiamo GPU mode (NVIDIA) e informiamo.
         * Con llama-server (CUDA+SYCL): mostriamo il comando --tensor-split ottimale. */
        m_ai->unloadModel();
        m_ai->setNumGpu(-2);   /* Ollama auto GPU (NVIDIA via CUDA) */

        if (m_computeInfo) {
            const bool isOllama = (m_ai->backend() == AiClient::Ollama);
            const long combined = m_nvidiaVramMb + m_igpuVramMb;
            const double nvRatio = (combined > 0)
                ? (double)m_nvidiaVramMb / (double)combined : 0.7;
            const double igRatio = 1.0 - nvRatio;

            if (isOllama) {
                m_computeInfo->setText(
                    QString("\xe2\x9c\x85  <b>Doppia GPU (Ollama)</b> — "
                            "NVIDIA %1 MB attiva, Intel iGPU ignorata da CUDA.<br>"
                            "Per sfruttare entrambe passa a <b>llama-server</b> compilato con CUDA+SYCL:<br>"
                            "<code>llama-server --device CUDA0,SYCL0 "
                            "--tensor-split %2,%3 -ngl 99 -m modello.gguf</code><br>"
                            "VRAM combinata: %4 MB  (NVIDIA %1 MB + Intel %5 MB)")
                    .arg(m_nvidiaVramMb)
                    .arg(nvRatio, 0, 'f', 2).arg(igRatio, 0, 'f', 2)
                    .arg(combined).arg(m_igpuVramMb));
            } else {
                /* llama-server: mostra comando completo con split ottimale */
                m_computeInfo->setText(
                    QString("\xe2\x9c\x85  <b>Doppia GPU (llama-server CUDA+SYCL)</b><br>"
                            "Avvia llama-server con:<br>"
                            "<code>llama-server --device CUDA0,SYCL0 "
                            "--tensor-split %1,%2 -ngl 99 -m modello.gguf</code><br>"
                            "Rapporto: NVIDIA %3 MB / Intel %4 MB = "
                            "<b>%5 MB combinati</b><br>"
                            "Budget netto (KV ~200 MB + RAG ~270 MB): ~%6 MB")
                    .arg(nvRatio, 0, 'f', 2).arg(igRatio, 0, 'f', 2)
                    .arg(m_nvidiaVramMb).arg(m_igpuVramMb).arg(combined)
                    .arg(qMax(0LL, combined - 470LL)));
            }
        }
    }
}

/* ══════════════════════════════════════════════════════════════
   helpers formato config
   ══════════════════════════════════════════════════════════════ */
QString ManutenzioneePage::detectConfigFmt()
{
    QString home     = QDir::homePath();
    QString pathToon = home + "/.prismalux_config.toon";
    QString pathJson = home + "/.prismalux_config.json";

    if (QFile::exists(pathToon)) return "toon";

    if (QFile::exists(pathJson)) {
        QFile f(pathJson);
        if (f.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
            f.close();
            if (!doc.isNull()) {
                QString fmt = doc.object().value("config_fmt").toString();
                if (fmt == "toon") return "toon";
            }
        }
        return "json";
    }
    return "json";
}

QString ManutenzioneePage::convertConfig(const QString& newFmt)
{
    QString home     = QDir::homePath();
    QString pathJson = home + "/.prismalux_config.json";
    QString pathToon = home + "/.prismalux_config.toon";

    struct Cfg {
        QString backend      = "ollama";
        QString ollamaHost   = "127.0.0.1";
        int     ollamaPort   = 11434;
        QString ollamaModel;
        QString lserverHost  = "127.0.0.1";
        int     lserverPort  = 8080;
        QString lserverModel;
        QString llamaModel;
        QString guiPath      = "Qt_GUI/build/Prismalux_GUI";
    } cfg;

    if (QFile::exists(pathJson)) {
        QFile f(pathJson);
        if (!f.open(QIODevice::ReadOnly))
            return QString("Impossibile aprire %1").arg(pathJson);
        QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        f.close();
        if (doc.isNull()) return "Config JSON non valido";
        QJsonObject o    = doc.object();
        cfg.backend      = o.value("backend").toString(cfg.backend);
        cfg.ollamaHost   = o.value("ollama_host").toString(cfg.ollamaHost);
        cfg.ollamaPort   = o.value("ollama_port").toInt(cfg.ollamaPort);
        cfg.ollamaModel  = o.value("ollama_model").toString();
        cfg.lserverHost  = o.value("lserver_host").toString(cfg.lserverHost);
        cfg.lserverPort  = o.value("lserver_port").toInt(cfg.lserverPort);
        cfg.lserverModel = o.value("lserver_model").toString();
        cfg.llamaModel   = o.value("llama_model").toString();
        cfg.guiPath      = o.value("gui_path").toString(cfg.guiPath);
    } else if (QFile::exists(pathToon)) {
        QFile f(pathToon);
        if (!f.open(QIODevice::ReadOnly))
            return QString("Impossibile aprire %1").arg(pathToon);
        QString buf = QString::fromUtf8(f.readAll());
        f.close();
        auto tget = [&](const QString& key) -> QString {
            QString pat = "\n" + key + ": ";
            int pos = buf.indexOf(pat);
            if (pos < 0) {
                if (buf.startsWith(key + ": ")) pos = -1;
                else return QString();
            }
            int vstart = (pos < 0) ? (key.length() + 2) : (pos + pat.length());
            int vend   = buf.indexOf('\n', vstart);
            return ((vend < 0) ? buf.mid(vstart) : buf.mid(vstart, vend - vstart)).trimmed();
        };
        cfg.backend      = tget("backend");
        cfg.ollamaHost   = tget("ollama_host");
        cfg.ollamaPort   = tget("ollama_port").toInt();
        cfg.ollamaModel  = tget("ollama_model");
        cfg.lserverHost  = tget("lserver_host");
        cfg.lserverPort  = tget("lserver_port").toInt();
        cfg.lserverModel = tget("lserver_model");
        cfg.llamaModel   = tget("llama_model");
        cfg.guiPath      = tget("gui_path");
        if (cfg.ollamaPort  == 0) cfg.ollamaPort  = 11434;
        if (cfg.lserverPort == 0) cfg.lserverPort = 8080;
    }

    QString destPath = (newFmt == "toon") ? pathToon : pathJson;
    QFile out(destPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return QString("Impossibile scrivere %1").arg(destPath);

    QTextStream ts(&out);
    if (newFmt == "toon") {
        ts << "# Prismalux config \xe2\x80\x94 TOON\n\n";
        ts << "backend: "       << cfg.backend      << "\n\n";
        ts << "ollama_host: "   << cfg.ollamaHost   << "\n";
        ts << "ollama_port: "   << cfg.ollamaPort   << "\n";
        ts << "ollama_model: "  << cfg.ollamaModel  << "\n\n";
        ts << "lserver_host: "  << cfg.lserverHost  << "\n";
        ts << "lserver_port: "  << cfg.lserverPort  << "\n";
        ts << "lserver_model: " << cfg.lserverModel << "\n\n";
        ts << "llama_model: "   << cfg.llamaModel   << "\n\n";
        ts << "gui_path: "      << cfg.guiPath      << "\n";
        ts << "config_fmt: toon\n";
    } else {
        QJsonObject o;
        o["backend"]       = cfg.backend;
        o["ollama_host"]   = cfg.ollamaHost;
        o["ollama_port"]   = cfg.ollamaPort;
        o["ollama_model"]  = cfg.ollamaModel;
        o["lserver_host"]  = cfg.lserverHost;
        o["lserver_port"]  = cfg.lserverPort;
        o["lserver_model"] = cfg.lserverModel;
        o["llama_model"]   = cfg.llamaModel;
        o["gui_path"]      = cfg.guiPath;
        o["config_fmt"]    = "json";
        ts << QJsonDocument(o).toJson(QJsonDocument::Indented);
    }
    out.close();

    if (newFmt == "toon" && QFile::exists(pathJson))  QFile::remove(pathJson);
    else if (newFmt == "json" && QFile::exists(pathToon)) QFile::remove(pathToon);

    return QString();
}

/* ══════════════════════════════════════════════════════════════
   Slot — Avvia llama-server
   ══════════════════════════════════════════════════════════════ */
void ManutenzioneePage::onSrvBrowseClicked()
{
    QString path = QFileDialog::getOpenFileName(this,
        "Seleziona modello .gguf",
        P::modelsDir(),
        "Modelli GGUF (*.gguf *.bin)");
    if (!path.isEmpty()) m_srvModelPath->setText(path);
}

void ManutenzioneePage::onSrvStartClicked()
{
    if (m_srvModelPath->text().trimmed().isEmpty()) {
        m_srvLog->append("\xe2\x9d\x8c  Seleziona un file .gguf prima di avviare il server.");
        return;
    }
    m_srvLog->clear();
    m_srvStartBtn->setEnabled(false);
    m_srvStopBtn->setEnabled(true);

    QString serverBin = P::llamaServerBin();
#ifdef _WIN32
    serverBin += ".exe";
    serverBin = QDir::toNativeSeparators(serverBin);
#endif
    QString cmd = QString("\"%1\" -m \"%2\" --port %3 --host 127.0.0.1 -c 4096")
                  .arg(serverBin)
                  .arg(m_srvModelPath->text().trimmed())
                  .arg(m_srvPort->text().trimmed());
    m_srvLog->append(QString("\xf0\x9f\x9a\x80  %1\n").arg(cmd));

    if (m_srvProc) { m_srvProc->kill(); m_srvProc->deleteLater(); m_srvProc = nullptr; }
    m_srvProc = new QProcess(this);
    m_srvProc->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_srvProc, &QProcess::readyRead,
            this, &ManutenzioneePage::onSrvProcReadyRead);
    connect(m_srvProc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ManutenzioneePage::onSrvProcFinished);
    connect(m_srvProc, &QProcess::errorOccurred,
            this, &ManutenzioneePage::onSrvProcErrorOccurred);
#ifdef _WIN32
    m_srvProc->start("cmd", {"/c", cmd});
#else
    m_srvProc->start("sh", {"-c", cmd});
#endif
}

void ManutenzioneePage::onSrvProcReadyRead()
{
    m_srvLog->moveCursor(QTextCursor::End);
    m_srvLog->insertPlainText(QString::fromLocal8Bit(m_srvProc->readAll()));
    m_srvLog->ensureCursorVisible();
}

void ManutenzioneePage::onSrvProcFinished(int code, QProcess::ExitStatus)
{
    m_srvLog->append(QString("\n\xf0\x9f\x94\xb4  Server terminato (code %1).").arg(code));
    m_srvStartBtn->setEnabled(true);
    m_srvStopBtn->setEnabled(false);
    m_srvProc = nullptr;
}

void ManutenzioneePage::onSrvProcErrorOccurred(QProcess::ProcessError err)
{
    if (err == QProcess::FailedToStart) {
        m_srvLog->append("\xe2\x9d\x8c  llama-server non trovato. Compilalo nella scheda \xf0\x9f\xa6\x99 llama.cpp.");
        m_srvStartBtn->setEnabled(true);
        m_srvStopBtn->setEnabled(false);
        m_srvProc = nullptr;
    }
}

void ManutenzioneePage::onSrvStopClicked()
{
    if (m_srvProc) { m_srvProc->terminate(); }
    m_srvStopBtn->setEnabled(false);
}

/* ══════════════════════════════════════════════════════════════
   Slot — Formato config
   ══════════════════════════════════════════════════════════════ */
void ManutenzioneePage::onFmtApplyClicked()
{
    QString newFmt = m_cmbFmt->currentData().toString();
    QString err    = convertConfig(newFmt);
    if (err.isEmpty())
        m_fmtStatus->setText(
            QString("\xe2\x9c\x85  Config salvato in formato %1")
            .arg(newFmt.toUpper()));
    else
        m_fmtStatus->setText(
            QString("\xe2\x9d\x8c  %1").arg(err));
}

/* ══════════════════════════════════════════════════════════════
   Slot — Backend / Connessione
   ══════════════════════════════════════════════════════════════ */
void ManutenzioneePage::onApplyBtnClicked()
{
    AiClient::Backend bk = (m_cmbBackend->currentIndex() == 0)
                            ? AiClient::Ollama : AiClient::LlamaServer;
    m_ai->setBackend(bk, m_hostEdit->text(), m_portEdit->text().toInt(), m_ai->model());
    m_ai->fetchModels();
}

void ManutenzioneePage::onBackendModelsReady(const QStringList& list)
{
    m_cmbModel->clear();
    if (list.isEmpty()) {
        m_cmbModel->addItem("(nessun modello \xe2\x80\x94 backend non raggiungibile)");
        return;
    }
    for (const auto& nm : list) {
        const qint64 sz = m_ai->modelSizeBytes(nm);
        m_cmbModel->addItem(P::modelIcon(sz, nm) + nm, nm);  /* UserRole = nome raw */
        if (P::isKnownBrokenModel(nm)) {
            const int i = m_cmbModel->count() - 1;
            m_cmbModel->setItemData(i, QBrush(QColor("#ea580c")), Qt::ForegroundRole);
            m_cmbModel->setItemData(i, QBrush(QColor("#fef08a")), Qt::BackgroundRole);
            m_cmbModel->setItemData(i,
                P::knownBrokenModelTooltip(),
                Qt::ToolTipRole);
        }
    }
    int idx = m_cmbModel->findData(m_ai->model());
    if (idx >= 0) m_cmbModel->setCurrentIndex(idx);
}

void ManutenzioneePage::onBackendModelsFetchError(const QString& msg)
{
    /* Mostra errore solo se il combo è ancora in stato "aggiornamento..." */
    if (m_cmbModel->count() == 1 &&
        m_cmbModel->itemText(0).contains("\xe2\x8f\xb3")) {
        m_cmbModel->clear();
        m_cmbModel->addItem("\xe2\x9a\xa0  " + msg, "");
    }
}

void ManutenzioneePage::onSetModelBtnClicked()
{
    const QString raw = m_cmbModel->currentData(Qt::UserRole).toString();
    const QString sel = raw.isEmpty() ? m_cmbModel->currentText() : raw;
    if (!sel.isEmpty() && !sel.startsWith("("))
        m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), sel);
}

void ManutenzioneePage::onBackendCmbChanged(int idx)
{
    /* Aggiorna porta e applica subito il backend — ricalcola i modelli in automatico */
    const int port = (idx == 0) ? P::kOllamaPort : P::kLlamaServerPort;
    m_portEdit->setText(QString::number(port));
    AiClient::Backend bk = (idx == 0) ? AiClient::Ollama : AiClient::LlamaServer;
    m_cmbModel->clear();
    m_cmbModel->addItem("(\xe2\x8f\xb3  aggiornamento modelli...)");
    m_ai->setBackend(bk, m_hostEdit->text(), port, "");
    m_ai->fetchModels();
    /* Mostra la sezione llama-server solo quando selezionato */
    if (m_grpServ) m_grpServ->setVisible(idx == 1);
}

/* ══════════════════════════════════════════════════════════════
   Slot — Verifica versione Ollama
   ══════════════════════════════════════════════════════════════ */
void ManutenzioneePage::onVerifyOllamaVersion()
{
    if (m_verLbl) m_verLbl->setText("\xf0\x9f\x90\xb3  Ollama: <i>verifica...</i>");
    if (m_ollamaVerProc) {
        m_ollamaVerProc->kill();
        m_ollamaVerProc->deleteLater();
        m_ollamaVerProc = nullptr;
    }
    m_ollamaVerProc = new QProcess(this);
    connect(m_ollamaVerProc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ManutenzioneePage::onOllamaVerProcFinished);
    connect(m_ollamaVerProc, &QProcess::errorOccurred,
            this, &ManutenzioneePage::onOllamaVerProcError);
    m_ollamaVerProc->start("ollama", {"--version"});
}

void ManutenzioneePage::onOllamaVerProcFinished(int code, QProcess::ExitStatus)
{
    if (!m_ollamaVerProc) return;
    const QString out = QString::fromLocal8Bit(m_ollamaVerProc->readAllStandardOutput()).trimmed()
                      + QString::fromLocal8Bit(m_ollamaVerProc->readAllStandardError()).trimmed();
    m_ollamaVerProc->deleteLater();
    m_ollamaVerProc = nullptr;

    if (m_verLbl) {
        if (code == 0 && !out.isEmpty()) {
            m_verLbl->setText(QString("\xf0\x9f\x90\xb3  Ollama: <b>%1</b>")
                .arg(out.toHtmlEscaped()));
        } else {
            m_verLbl->setText("\xf0\x9f\x90\xb3  Ollama: <span style='color:#ef4444;'>non trovato</span>");
        }
    }
    if (m_updLog && code == 0 && !out.isEmpty())
        m_updLog->append(QString("\xf0\x9f\x90\xb3  %1").arg(out));
}

void ManutenzioneePage::onOllamaVerProcError(QProcess::ProcessError)
{
    if (m_ollamaVerProc) {
        m_ollamaVerProc->deleteLater();
        m_ollamaVerProc = nullptr;
    }
    if (m_verLbl)
        m_verLbl->setText("\xf0\x9f\x90\xb3  Ollama: <span style='color:#ef4444;'>non trovato nel PATH</span>");
}

/* ══════════════════════════════════════════════════════════════
   Slot — Aggiorna tutti i modelli Ollama
   ══════════════════════════════════════════════════════════════ */
void ManutenzioneePage::onUpdAllBtnClicked()
{
    if (m_updAllBtn) m_updAllBtn->setEnabled(false);
    if (m_updLog) m_updLog->clear();
    if (m_updStatusLbl) m_updStatusLbl->setText("\xf0\x9f\x94\x84  Recupero lista modelli...");

    if (m_listProc) {
        m_listProc->kill();
        m_listProc->deleteLater();
        m_listProc = nullptr;
    }
    m_listProc = new QProcess(this);
    connect(m_listProc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ManutenzioneePage::onListProcFinished);
    connect(m_listProc, &QProcess::errorOccurred,
            this, &ManutenzioneePage::onListProcError);
    m_listProc->start("ollama", {"list"});
}

void ManutenzioneePage::onListProcFinished(int, QProcess::ExitStatus)
{
    if (!m_listProc) return;
    const QString raw = QString::fromLocal8Bit(m_listProc->readAllStandardOutput());
    m_listProc->deleteLater();
    m_listProc = nullptr;

    QStringList models;
    for (const QString& line : raw.split('\n', Qt::SkipEmptyParts)) {
        if (line.trimmed().startsWith("NAME", Qt::CaseInsensitive)) continue;
        const QString name = line.split(QChar(' '), Qt::SkipEmptyParts).value(0).trimmed();
        if (!name.isEmpty()) models << name;
    }

    if (models.isEmpty()) {
        if (m_updStatusLbl) m_updStatusLbl->setText("\xe2\x9d\x8c  Nessun modello trovato. Ollama in esecuzione?");
        if (m_updAllBtn) m_updAllBtn->setEnabled(true);
        return;
    }

    if (m_updLog)
        m_updLog->append(QString("\xf0\x9f\x93\x8b  %1 modelli da aggiornare: %2")
            .arg(models.size()).arg(models.join(", ")));
    if (m_updStatusLbl)
        m_updStatusLbl->setText(QString("\xf0\x9f\x94\x84  Aggiornamento 1/%1...").arg(models.size()));

    /* Aggiornamento sequenziale tramite struct ricorsiva */
    auto* idx   = new int(0);
    auto* total = new int(models.size());

    struct Updater {
        static void next(QWidget* parent, QTextEdit* log, QLabel* status,
                         QPushButton* btn, QStringList mdls, int* i, int* tot) {
            if (*i >= *tot) {
                delete i; delete tot;
                if (status) status->setText(QString("\xe2\x9c\x85  Aggiornamento completato! %1 modelli").arg(*tot));
                if (log) log->append("\n\xe2\x9c\x85  Tutti i modelli sono aggiornati.");
                if (btn) btn->setEnabled(true);
                return;
            }
            const QString mdl = mdls.at(*i);
            if (status) status->setText(QString("\xf0\x9f\x94\x84  Aggiornamento %1/%2: %3")
                .arg(*i + 1).arg(*tot).arg(mdl));
            if (log) log->append(QString("\n\xe2\xac\x87  Aggiornamento: <b>%1</b>...").arg(mdl));

            auto* proc = new QProcess(parent);
            proc->setProcessChannelMode(QProcess::MergedChannels);
            proc->start("ollama", {"pull", mdl});
            QObject::connect(proc, &QProcess::readyRead, parent, [proc, log](){
                if (log) {
                    log->moveCursor(QTextCursor::End);
                    const QString chunk = QString::fromLocal8Bit(proc->readAll());
                    for (const QString& l : chunk.split('\n', Qt::SkipEmptyParts))
                        log->insertPlainText("  " + l.trimmed() + "\n");
                    log->ensureCursorVisible();
                }
            });
            QObject::connect(proc,
                QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                parent, [proc, log, status, btn, mdls, i, tot, parent](int code, QProcess::ExitStatus){
                    proc->deleteLater();
                    if (log) {
                        if (code == 0)
                            log->append(QString("  \xe2\x9c\x85  %1 aggiornato.").arg(mdls.at(*i)));
                        else
                            log->append(QString("  \xe2\x9a\xa0  %1: errore (code %2)").arg(mdls.at(*i)).arg(code));
                    }
                    ++(*i);
                    next(parent, log, status, btn, mdls, i, tot);
                });
        }
    };
    Updater::next(this, m_updLog, m_updStatusLbl, m_updAllBtn, models, idx, total);
}

void ManutenzioneePage::onListProcError(QProcess::ProcessError)
{
    if (m_listProc) {
        m_listProc->deleteLater();
        m_listProc = nullptr;
    }
    if (m_updStatusLbl) m_updStatusLbl->setText("\xe2\x9d\x8c  Ollama non trovato. Verifica il PATH.");
    if (m_updAllBtn) m_updAllBtn->setEnabled(true);
}

/* ══════════════════════════════════════════════════════════════
   Slot — Aggiorna llama.cpp (git pull) + scansiona GGUF
   ══════════════════════════════════════════════════════════════ */
void ManutenzioneePage::onUpdLlamaBtnClicked()
{
    if (m_updLlamaBtn) m_updLlamaBtn->setEnabled(false);
    if (m_updLog) m_updLog->clear();
    if (m_updStatusLbl) m_updStatusLbl->setText("\xf0\x9f\x94\x84  Scansione modelli GGUF...");

    const QStringList ggufFiles = P::scanGgufFiles();
    if (m_updLog) {
        m_updLog->append(QString("\xf0\x9f\x93\x81  Modelli GGUF trovati: %1").arg(ggufFiles.size()));
        for (const QString& path : ggufFiles) {
            const QFileInfo fi(path);
            const double gb = fi.size() / (1024.0 * 1024.0 * 1024.0);
            m_updLog->append(QString("  \xf0\x9f\x9f\xa4  %1  (%2 GB)")
                .arg(fi.fileName()).arg(gb, 0, 'f', 1));
        }
        if (ggufFiles.isEmpty())
            m_updLog->append("\xe2\x9a\xa0  Nessun file .gguf trovato in models/");
    }

    QString llamaDir;
    for (const QString& c : {P::root() + "/llama_cpp_studio/llama.cpp",
                              P::root() + "/llama.cpp"}) {
        if (QDir(c + "/.git").exists()) { llamaDir = c; break; }
    }

    if (llamaDir.isEmpty()) {
        if (m_updLog) {
            m_updLog->append("\n\xe2\x84\xb9  Repository llama.cpp non trovato — aggiornamento git non disponibile.");
            m_updLog->append("   I file .gguf vanno aggiornati manualmente da HuggingFace Hub.");
        }
        if (m_updStatusLbl) m_updStatusLbl->setText("\xe2\x9c\x85  Scansione completata.");
        if (m_updLlamaBtn) m_updLlamaBtn->setEnabled(true);
        return;
    }

    if (m_updLog) m_updLog->append(QString("\n\xf0\x9f\x94\x84  git pull: %1").arg(llamaDir));
    if (m_updStatusLbl) m_updStatusLbl->setText("\xf0\x9f\x94\x84  Aggiornamento llama.cpp...");

    if (m_gitProc) {
        m_gitProc->kill();
        m_gitProc->deleteLater();
        m_gitProc = nullptr;
    }
    m_gitProc = new QProcess(this);
    m_gitProc->setWorkingDirectory(llamaDir);
    m_gitProc->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_gitProc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ManutenzioneePage::onGitProcFinished);
    connect(m_gitProc, &QProcess::errorOccurred,
            this, &ManutenzioneePage::onGitProcError);
    m_gitProc->start("git", {"pull", "--ff-only"});
}

void ManutenzioneePage::onGitProcFinished(int code, QProcess::ExitStatus)
{
    if (!m_gitProc) return;
    const QString out = QString::fromLocal8Bit(m_gitProc->readAll()).trimmed();
    m_gitProc->deleteLater();
    m_gitProc = nullptr;

    if (m_updLog) {
        if (!out.isEmpty())
            m_updLog->append("  " + QString(out).replace('\n', "\n  "));
        m_updLog->append("\n\xe2\x84\xb9  I file .gguf non hanno aggiornamento automatico.\n"
                         "   Scarica le versioni aggiornate da HuggingFace Hub.");
    }
    if (m_updStatusLbl) {
        if (code == 0)
            m_updStatusLbl->setText("\xe2\x9c\x85  llama.cpp aggiornato.");
        else
            m_updStatusLbl->setText("\xe2\x9a\xa0  git pull fallito (verifica connessione).");
    }
    if (m_updLlamaBtn) m_updLlamaBtn->setEnabled(true);
}

void ManutenzioneePage::onGitProcError(QProcess::ProcessError)
{
    if (m_gitProc) {
        m_gitProc->deleteLater();
        m_gitProc = nullptr;
    }
    if (m_updLog) m_updLog->append("\xe2\x9d\x8c  git non trovato nel PATH.");
    if (m_updStatusLbl) m_updStatusLbl->setText("\xe2\x9a\xa0  git non disponibile.");
    if (m_updLlamaBtn) m_updLlamaBtn->setEnabled(true);
}

/* ══════════════════════════════════════════════════════════════
   Slot — zRAM / RAM (helper + bottoni)
   ══════════════════════════════════════════════════════════════ */
void ManutenzioneePage::runRamCmd(const QString& prog, const QStringList& args,
                                   const QString& label)
{
    if (m_ramLog) m_ramLog->append(QString("\xe2\x96\xb6 %1\n").arg(label));
    if (m_ramCmdProc) {
        m_ramCmdProc->kill();
        m_ramCmdProc->deleteLater();
        m_ramCmdProc = nullptr;
    }
    m_ramCmdProc = new QProcess(this);
    m_ramCmdProc->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_ramCmdProc, &QProcess::readyRead,
            this, &ManutenzioneePage::onRamCmdReadyRead);
    connect(m_ramCmdProc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ManutenzioneePage::onRamCmdFinished);
    m_ramCmdProc->start(prog, args);
    if (!m_ramCmdProc->waitForStarted(3000)) {
        if (m_ramLog)
            m_ramLog->append(QString("\xe2\x9d\x8c  Impossibile avviare: %1\n").arg(prog));
        m_ramCmdProc->deleteLater();
        m_ramCmdProc = nullptr;
    }
}

void ManutenzioneePage::onRamCmdReadyRead()
{
    if (!m_ramCmdProc || !m_ramLog) return;
    m_ramLog->moveCursor(QTextCursor::End);
    m_ramLog->insertPlainText(QString::fromLocal8Bit(m_ramCmdProc->readAll()));
    m_ramLog->ensureCursorVisible();
}

void ManutenzioneePage::onRamCmdFinished(int code, QProcess::ExitStatus)
{
    if (m_ramLog)
        m_ramLog->append(code == 0 ? "\xe2\x9c\x85 OK\n" :
                         QString("\xe2\x9d\x8c Codice uscita: %1\n").arg(code));
    if (m_ramCmdProc) {
        m_ramCmdProc->deleteLater();
        m_ramCmdProc = nullptr;
    }
}

void ManutenzioneePage::onAutoZramCbToggled(bool on)
{
    QSettings s("Prismalux", "GUI");
    s.setValue(P::SK::kAutoZramDoppia, on);
}

void ManutenzioneePage::onBtnGpuClicked()    { applyComputeMode("gpu");    }
void ManutenzioneePage::onBtnCpuClicked()    { applyComputeMode("cpu");    }
void ManutenzioneePage::onBtnMistoClicked()  { applyComputeMode("misto");  }
void ManutenzioneePage::onBtnDoppiaClicked() { applyComputeMode("doppia"); }
void ManutenzioneePage::onBtnSaveModeClicked() { applyComputeMode(m_selectedMode); }

void ManutenzioneePage::onAiModelChangedApplyMode(const QString&)
{
    QSettings s("Prismalux", "GUI");
    const QString saved = s.value(P::SK::kComputeMode, "").toString();
    if (saved == "gpu" || saved == "misto" || saved == "doppia")
        applyComputeMode(saved);
}

void ManutenzioneePage::onDetectBtnClicked()
{
    if (m_ramLog) m_ramLog->clear();
#ifdef Q_OS_WIN
    runRamCmd("powershell",
        {"-NoProfile", "-Command",
         "Get-MMAgent | Select-Object MemoryCompression,"
         "ApplicationLaunchPrefetching,OperationAPI | Format-List"},
        "Stato Memory Compression (Windows)");
#else
    runRamCmd("bash", {"-c",
        "echo '=== Swap attivo ==='; "
        "cat /proc/swaps; "
        "echo; echo '=== zRAM dispositivi ==='; "
        "zramctl 2>/dev/null || ls /sys/block/zram* 2>/dev/null || "
        "echo 'nessun device zRAM attivo'; "
        "echo; echo '=== RAM libera ==='; "
        "grep -E 'MemTotal|MemFree|MemAvailable|SwapTotal|SwapFree'"
        " /proc/meminfo"},
        "Rilevamento stato zRAM");
#endif
}

void ManutenzioneePage::onCompBtnClicked()
{
    if (m_ramLog) m_ramLog->clear();
    runRamCmd("powershell",
        {"-NoProfile", "-Command",
         "Enable-MMAgent -MemoryCompression; "
         "Write-Host 'Memory Compression attivata.'"},
        "Attivazione Memory Compression");
}

void ManutenzioneePage::onDisableRamBtnClicked()
{
    if (m_ramLog) m_ramLog->clear();
#ifdef Q_OS_WIN
    runRamCmd("powershell",
        {"-NoProfile", "-Command",
         "Disable-MMAgent -MemoryCompression; "
         "Write-Host 'Memory Compression disattivata.'"},
        "Disattivazione Memory Compression");
#else
    static const char* SCRIPT_DISABILITA =
        "for dev in /dev/zram*; do swapoff \"$dev\" 2>/dev/null && "
        "echo \"swapoff $dev OK\"; done; "
        "rmmod zram 2>/dev/null && echo 'modulo zram rimosso' || "
        "echo 'zram non era caricato'; "
        "echo; cat /proc/swaps";
    if (m_ramLog) m_ramLog->append("\xe2\x9a\xa0  Richiesta autorizzazione amministratore (pkexec)...\n");
    runRamCmd("pkexec", {"bash", "-c", SCRIPT_DISABILITA}, "Disattivazione zRAM");
#endif
}

void ManutenzioneePage::onSingBtnClicked()
{
    static const char* SCRIPT_SINGOLA =
        "for dev in /dev/zram*; do swapoff \"$dev\" 2>/dev/null; done; "
        "rmmod zram 2>/dev/null || true; "
        "modprobe zram num_devices=1; "
        "sleep 0.3; "
        "echo lz4 | tee /sys/block/zram0/comp_algorithm; "
        "BYTES=$(( $(grep MemTotal /proc/meminfo | awk '{print $2}') * 512 )); "
        "echo $BYTES | tee /sys/block/zram0/disksize; "
        "mkswap /dev/zram0; "
        "swapon -p 100 /dev/zram0; "
        "echo '---'; "
        "echo 'zRAM singolo attivo (lz4):'; "
        "zramctl 2>/dev/null || cat /sys/block/zram0/disksize";
    if (m_ramLog) m_ramLog->clear();
    if (m_ramLog) m_ramLog->append("\xe2\x9a\xa0  Richiesta autorizzazione amministratore (pkexec)...\n");
    runRamCmd("pkexec", {"bash", "-c", SCRIPT_SINGOLA},
               "Attivazione zRAM singolo (lz4, 50% RAM)");
}

void ManutenzioneePage::onDoppiaBtnClicked()
{
    static const char* SCRIPT_DOPPIA =
        "echo 'Step 1: compattazione memoria fisica...'; "
        "echo 1 | tee /proc/sys/vm/compact_memory; "
        "sleep 1; "
        "for dev in /dev/zram*; do swapoff \"$dev\" 2>/dev/null; done; "
        "rmmod zram 2>/dev/null || true; "
        "sleep 0.3; "
        "modprobe zram num_devices=2; "
        "sleep 0.3; "
        "TOTAL=$(grep MemTotal /proc/meminfo | awk '{print $2}'); "
        "echo 'Step 2: device 0 (zstd, 50% RAM)...'; "
        "(echo zstd | tee /sys/block/zram0/comp_algorithm) || "
        " (echo lzo-rle | tee /sys/block/zram0/comp_algorithm); "
        "echo $(( TOTAL * 512 )) | tee /sys/block/zram0/disksize; "
        "mkswap /dev/zram0; "
        "swapon -p 100 /dev/zram0; "
        "echo 'Step 3: device 1 (zstd, 25% RAM)...'; "
        "(echo zstd | tee /sys/block/zram1/comp_algorithm) || "
        " (echo lzo-rle | tee /sys/block/zram1/comp_algorithm); "
        "echo $(( TOTAL * 256 )) | tee /sys/block/zram1/disksize; "
        "mkswap /dev/zram1; "
        "swapon -p 50 /dev/zram1; "
        "echo '---'; "
        "echo 'zRAM doppio attivo (zstd):'; "
        "zramctl 2>/dev/null; "
        "cat /proc/swaps";
    if (m_ramLog) m_ramLog->clear();
    if (m_ramLog) m_ramLog->append("\xe2\x9a\xa0  Richiesta autorizzazione amministratore (pkexec)...\n");
    runRamCmd("pkexec", {"bash", "-c", SCRIPT_DOPPIA},
               "Attivazione zRAM doppio (zstd, 75% RAM \xe2\x80\x94 algoritmo Meta)");
}

void ManutenzioneePage::onBtnIntelNpuClicked()
{
    if (m_ramLog) m_ramLog->append("\xf0\x9f\x94\xb5  Installazione intel-npu-acceleration-library...\n");
    runRamCmd("pip", {"install", "intel-npu-acceleration-library"},
               "pip install intel-npu-acceleration-library");
}
