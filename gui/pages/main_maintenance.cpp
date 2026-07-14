#include "main_maintenance.h"
#include "../dpi_utils.h"
#include "../prismalux_paths.h"
#include "../widgets/model_combo_helper.h"
#include "../widgets/widget_docker_update.h"
#include "../widgets/widget_python_update.h"
#include "../log_bus.h"
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
#include <QJsonArray>
#include <QCryptographicHash>
#include <QDateTime>
#include <QTextEdit>
#include <QFileDialog>
#include <QScrollArea>
#include <QFileInfo>
#include <QTimer>
#include <QTextCursor>
#include <QFrame>
#include <QPointer>
#include <QApplication>
#include <QClipboard>

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

    installKnowledgeBackupTimer();
    loadGgufHashes();
}

/* ── Stile condiviso per i QGroupBox ─────────────────────────── */
static const char* GRP_STYLE =
    "QGroupBox { border:1px solid #1e2040; border-radius:6px; "
    "margin-top:12px; color:#8a8fb0; font-size:12px; }"
    "QGroupBox::title { subcontrol-origin:margin; left:10px; padding:0 4px; }";

/* ══════════════════════════════════════════════════════════════
   buildBackend() — entry point: assembla le sezioni in una pagina.
   ══════════════════════════════════════════════════════════════ */
QWidget* ManutenzioneePage::buildBackend()
{
    return buildBackendPage();
}

/* ── Livello 1: struttura pagina backend ─────────────────────── */
QWidget* ManutenzioneePage::buildBackendPage()
{
    auto* page    = new QWidget;
    auto* mainLay = new QVBoxLayout(page);
    mainLay->setContentsMargins(16, 14, 16, 14);
    mainLay->setSpacing(12);

    auto* colsRow = new QWidget(page);
    auto* colsLay = new QHBoxLayout(colsRow);
    colsLay->setContentsMargins(0, 0, 0, 0);
    colsLay->setSpacing(16);

    /* Colonna sinistra: Connessione & Modello + Configurazione Avanzata sotto */
    auto* leftStack    = new QWidget(colsRow);
    auto* leftStackLay = new QVBoxLayout(leftStack);
    leftStackLay->setContentsMargins(0, 0, 0, 0);
    leftStackLay->setSpacing(12);
    auto* leftGroup  = buildConnectionModelGroup(leftStack);
    auto* rightGroup = buildAdvancedConfigGroup(leftStack);
    leftStackLay->addWidget(leftGroup);
    leftStackLay->addWidget(rightGroup);
    leftStackLay->addStretch();

    /* Colonna destra allargata: Aggiornamento Modelli GPU/RAM */
    auto* updGroup   = buildUpdateGroup(colsRow);
    colsLay->addWidget(leftStack);
    colsLay->addWidget(updGroup, 1);
    mainLay->addWidget(colsRow, 1);

    QTimer::singleShot(200, this, &ManutenzioneePage::onVerifyOllamaVersion);

    return page;
}

/* ── Livello 2a: colonna sinistra — Connessione & Modello ─────── */
QGroupBox* ManutenzioneePage::buildConnectionModelGroup(QWidget* parent)
{
    auto* grp = new QGroupBox(tr("\xf0\x9f\x94\x8c  Connessione & Modello"), parent);
    grp->setObjectName("cardGroup");
    grp->setFixedWidth(dpiScale(270));
    auto* lay = new QVBoxLayout(grp);
    lay->setSpacing(6);

    lay->addWidget(new QLabel(tr("Backend:"), grp));
    m_cmbBackend = new QComboBox(grp);
    m_cmbBackend->addItem(QString("\xf0\x9f\x90\xb3  Ollama  (:%1)").arg(P::kOllamaPort));
    m_cmbBackend->addItem(QString("\xf0\x9f\xa6\x99  llama-server  (:%1)").arg(P::kLlamaServerPort));
    m_cmbBackend->setAccessibleName(tr("Selettore backend AI"));
    lay->addWidget(m_cmbBackend);

    m_hostEdit = new QLineEdit("127.0.0.1", grp);
    m_hostEdit->setObjectName("chatInput");
    m_hostEdit->setPlaceholderText(tr("Host"));
    m_hostEdit->setAccessibleName(tr("Indirizzo host del backend AI"));
    lay->addWidget(new QLabel(tr("Host:"), grp));
    lay->addWidget(m_hostEdit);

    m_portEdit = new QLineEdit(QString::number(P::kOllamaPort), grp);
    m_portEdit->setObjectName("chatInput");
    m_portEdit->setPlaceholderText(tr("Porta"));
    m_portEdit->setAccessibleName(tr("Porta del backend AI"));
    lay->addWidget(new QLabel(tr("Porta:"), grp));
    lay->addWidget(m_portEdit);

    auto* applyBtn = new QPushButton(tr("Applica \xe2\x96\xb6"), grp);
    applyBtn->setObjectName("actionBtn");
    applyBtn->setAccessibleName(tr("Applica configurazione backend"));
    lay->addWidget(applyBtn);

    auto* sep = new QFrame(grp);
    sep->setFrameShape(QFrame::HLine);
    sep->setObjectName("sidebarSep");
    lay->addWidget(sep);

    lay->addWidget(new QLabel(tr("Modello:"), grp));
    m_cmbModel = new QComboBox(grp);
    m_cmbModel->addItem("(nessun modello \xe2\x80\x94 backend non raggiungibile)");
    m_cmbModel->setAccessibleName(tr("Lista modelli AI disponibili"));
    lay->addWidget(m_cmbModel);

    lay->addWidget(buildModelButtonRow(grp));
    lay->addStretch(1);

    connect(applyBtn, &QPushButton::clicked, this, &ManutenzioneePage::onApplyBtnClicked);
    connect(m_cmbBackend, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ManutenzioneePage::onBackendCmbChanged);
    connect(m_ai, &AiClient::modelsReady, this, &ManutenzioneePage::onBackendModelsReady);
    connect(m_ai, &AiClient::error,       this, &ManutenzioneePage::onBackendModelsFetchError);

    return grp;
}

/* ── Livello 3: riga pulsanti modello (refresh + usa questo) ──── */
QWidget* ManutenzioneePage::buildModelButtonRow(QGroupBox* parent)
{
    auto* row  = new QWidget(parent);
    auto* lay  = new QHBoxLayout(row);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(6);

    auto* refreshBtn = new QPushButton("\xf0\x9f\x94\x84", parent);
    refreshBtn->setObjectName("actionBtn");
    refreshBtn->setFixedWidth(dpiScale(36));
    refreshBtn->setToolTip(tr("Aggiorna lista modelli"));
    refreshBtn->setAccessibleName(tr("Aggiorna lista modelli AI"));
    auto* setModelBtn = new QPushButton(tr("\xe2\x9c\x93  Usa questo"), parent);
    setModelBtn->setObjectName("actionBtn");
    setModelBtn->setAccessibleName(tr("Attiva il modello selezionato"));
    lay->addWidget(refreshBtn);
    lay->addWidget(setModelBtn, 1);

    connect(refreshBtn,   &QPushButton::clicked, m_ai, &AiClient::fetchModels);
    connect(setModelBtn,  &QPushButton::clicked, this, &ManutenzioneePage::onSetModelBtnClicked);

    return row;
}

/* ── Livello 2b: colonna destra — Configurazione Avanzata ─────── */
QGroupBox* ManutenzioneePage::buildAdvancedConfigGroup(QWidget* parent)
{
    auto* grp = new QGroupBox(tr("\xe2\x9a\x99\xef\xb8\x8f  Configurazione Avanzata"), parent);
    grp->setObjectName("cardGroup");
    auto* lay = new QVBoxLayout(grp);
    lay->setSpacing(8);

    buildConfigFmtSection(grp, lay);

    auto* srvSep = new QFrame(grp);
    srvSep->setFrameShape(QFrame::HLine);
    srvSep->setObjectName("sidebarSep");
    lay->addWidget(srvSep);

    buildLlamaServerSection(grp, lay);
    lay->addStretch(1);

    return grp;
}

/* ── Livello 3a: sezione Formato Config ──────────────────────── */
void ManutenzioneePage::buildConfigFmtSection(QGroupBox* grp, QVBoxLayout* lay)
{
    auto* fmtTitle = new QLabel(
        "\xf0\x9f\x93\x84  <b>Formato Config</b>  (~/.prismalux_config)", grp);
    fmtTitle->setObjectName("cardTitle");
    fmtTitle->setTextFormat(Qt::RichText);
    lay->addWidget(fmtTitle);

    auto* fmtDesc = new QLabel(
        "<b>JSON</b>: standard, leggibile dai tool esterni.&nbsp;"
        "<b>TOON</b>: flat <code>chiave: valore</code>, -12% dimensione.",
        grp);
    fmtDesc->setObjectName("cardDesc");
    fmtDesc->setWordWrap(true);
    lay->addWidget(fmtDesc);

    auto* fmtRow  = new QWidget(grp);
    auto* fmtRowL = new QHBoxLayout(fmtRow);
    fmtRowL->setContentsMargins(0, 0, 0, 0);
    fmtRowL->setSpacing(8);
    fmtRowL->addWidget(new QLabel(tr("Formato:"), fmtRow));

    m_cmbFmt = new QComboBox(fmtRow);
    m_cmbFmt->addItem("JSON  (.prismalux_config.json)", QString("json"));
    m_cmbFmt->addItem("TOON  (.prismalux_config.toon)", QString("toon"));
    {
        const QString cur = detectConfigFmt();
        const int idx = m_cmbFmt->findData(cur);
        if (idx >= 0) m_cmbFmt->setCurrentIndex(idx);
    }
    auto* fmtApply = new QPushButton(tr("Converti \xe2\x96\xb6"), fmtRow);
    fmtApply->setObjectName("actionBtn");
    m_fmtStatus = new QLabel("", fmtRow);
    m_fmtStatus->setObjectName("cardDesc");
    fmtRowL->addWidget(m_cmbFmt, 1);
    fmtRowL->addWidget(fmtApply);
    fmtRowL->addWidget(m_fmtStatus, 1);
    lay->addWidget(fmtRow);

    connect(fmtApply, &QPushButton::clicked, this, &ManutenzioneePage::onFmtApplyClicked);
}

/* ── Livello 3b: sezione llama-server ────────────────────────── */
void ManutenzioneePage::buildLlamaServerSection(QGroupBox* grp, QVBoxLayout* lay)
{
    m_grpServ = new QGroupBox(
        "\xf0\x9f\xa6\x99  llama.cpp \xe2\x80\x94 Avvia llama-server", grp);
    m_grpServ->setVisible(false);
    m_grpServ->setStyleSheet(GRP_STYLE);
    auto* srvLay = new QVBoxLayout(m_grpServ);
    srvLay->setSpacing(8);

    /* Riga modello */
    auto* srvModelRow = new QWidget(m_grpServ);
    auto* srvModelL   = new QHBoxLayout(srvModelRow);
    srvModelL->setContentsMargins(0, 0, 0, 0);
    srvModelL->setSpacing(8);
    m_srvModelPath = new QLineEdit(m_grpServ);
    m_srvModelPath->setObjectName("chatInput");
    m_srvModelPath->setPlaceholderText(tr("percorso/al/modello.gguf"));
    auto* srvBrowse = new QPushButton("\xe2\x80\xa6", m_grpServ);
    srvBrowse->setObjectName("actionBtn");
    srvBrowse->setFixedWidth(dpiScale(32));
    srvBrowse->setToolTip(tr("Scegli file modello .gguf per llama-server"));
    srvModelL->addWidget(new QLabel(tr("Modello:"), srvModelRow));
    srvModelL->addWidget(m_srvModelPath, 1);
    srvModelL->addWidget(srvBrowse);
    srvLay->addWidget(srvModelRow);

    /* Riga porta + pulsanti avvio/stop */
    auto* srvCtrlRow = new QWidget(m_grpServ);
    auto* srvCtrlL   = new QHBoxLayout(srvCtrlRow);
    srvCtrlL->setContentsMargins(0, 0, 0, 0);
    srvCtrlL->setSpacing(8);
    m_srvPort = new QLineEdit(QString::number(P::kLlamaServerPort), m_grpServ);
    m_srvPort->setObjectName("chatInput");
    m_srvPort->setFixedWidth(dpiScale(70));
    m_srvStartBtn = new QPushButton(tr("\xe2\x96\xb6  Avvia"), m_grpServ);
    m_srvStartBtn->setObjectName("actionBtn");
    m_srvStopBtn = new QPushButton(tr("\xe2\x96\xa0  Stop"), m_grpServ);
    m_srvStopBtn->setObjectName("actionBtn");
    m_srvStopBtn->setProperty("danger", "true");
    m_srvStopBtn->setEnabled(false);
    srvCtrlL->addWidget(new QLabel(tr("Porta:"), srvCtrlRow));
    srvCtrlL->addWidget(m_srvPort);
    srvCtrlL->addSpacing(12);
    srvCtrlL->addWidget(m_srvStartBtn);
    srvCtrlL->addWidget(m_srvStopBtn);
    srvCtrlL->addStretch(1);
    srvLay->addWidget(srvCtrlRow);

    /* Log output */
    m_srvLog = new QTextEdit(m_grpServ);
    m_srvLog->setReadOnly(true);
    m_srvLog->setObjectName("chatLog");
    m_srvLog->setMinimumHeight(80);
    m_srvLog->setPlaceholderText(tr("Log llama-server..."));
    srvLay->addWidget(m_srvLog);

    lay->addWidget(m_grpServ);

    connect(srvBrowse,    &QPushButton::clicked, this, &ManutenzioneePage::onSrvBrowseClicked);
    connect(m_srvStartBtn, &QPushButton::clicked, this, &ManutenzioneePage::onSrvStartClicked);
    connect(m_srvStopBtn,  &QPushButton::clicked, this, &ManutenzioneePage::onSrvStopClicked);
}

/* ── Livello 2c: gruppo Aggiornamento Modelli & GPU/RAM ──────── */
QGroupBox* ManutenzioneePage::buildUpdateGroup(QWidget* parent)
{
    auto* grp = new QGroupBox(
        "\xf0\x9f\x94\x84  Aggiornamento Modelli & GPU/RAM", parent);
    grp->setObjectName("cardGroup");
    auto* lay = new QVBoxLayout(grp);
    lay->setSpacing(6);
    lay->setContentsMargins(10, 14, 10, 8);

    /* Versione Ollama */
    auto* verRow = new QWidget(grp);
    auto* verLay = new QHBoxLayout(verRow);
    verLay->setContentsMargins(0, 0, 0, 0);
    verLay->setSpacing(8);
    m_verLbl = new QLabel(
        "\xf0\x9f\x90\xb3  Ollama: <i>verifica in corso...</i>", verRow);
    m_verLbl->setObjectName("cardDesc");
    m_verLbl->setTextFormat(Qt::RichText);
    verLay->addWidget(m_verLbl, 1);
    auto* verBtn = new QPushButton(tr("\xf0\x9f\x94\x8d  Verifica"), verRow);
    verBtn->setObjectName("actionBtn");
    verBtn->setFixedWidth(dpiScale(90));
    verLay->addWidget(verBtn);
    lay->addWidget(verRow);

    /* GPU vs RAM hint */
    m_ramStatusLbl = new QLabel("", grp);
    m_ramStatusLbl->setObjectName("cardDesc");
    m_ramStatusLbl->setWordWrap(true);
    m_ramStatusLbl->setTextFormat(Qt::RichText);
    lay->addWidget(m_ramStatusLbl);

    /* Pulsanti aggiornamento modelli */
    auto* btnRow  = new QWidget(grp);
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
    lay->addWidget(btnRow);

    /* Log aggiornamento */
    m_updLog = new QTextEdit(grp);
    m_updLog->setReadOnly(true);
    m_updLog->setObjectName("chatLog");
    m_updLog->setMinimumHeight(80);
    m_updLog->setPlaceholderText(
        "Premi \"Aggiorna tutti\" per scaricare le ultime versioni dei modelli Ollama.");
    lay->addWidget(m_updLog);

    /* ── Scarica nuovo modello ── */
    auto* sep1 = new QFrame(grp); sep1->setFrameShape(QFrame::HLine);
    sep1->setObjectName("sidebarSep"); lay->addWidget(sep1);

    auto* dlRow  = new QWidget(grp);
    auto* dlRowL = new QHBoxLayout(dlRow);
    dlRowL->setContentsMargins(0,0,0,0); dlRowL->setSpacing(8);
    dlRowL->addWidget(new QLabel(tr("\xe2\xac\x87  Scarica nuovo modello Ollama:"), dlRow));
    m_downloadModelEdit = new QLineEdit(dlRow);
    m_downloadModelEdit->setPlaceholderText(tr("es. llama3.2:3b  \xe2\x80\xa2  qwen2.5-coder:7b"));
    m_downloadModelEdit->setAccessibleName(tr("Nome modello Ollama da scaricare"));
    m_btnDownloadModel = new QPushButton(tr("\xe2\xac\x87  Scarica"), dlRow);
    m_btnDownloadModel->setObjectName("actionBtn");
    m_btnDownloadModel->setAccessibleName(tr("Avvia download modello Ollama"));
    m_downloadStatusLbl = new QLabel("", dlRow);
    m_downloadStatusLbl->setObjectName("cardDesc");
    dlRowL->addWidget(m_downloadModelEdit, 1);
    dlRowL->addWidget(m_btnDownloadModel);
    dlRowL->addWidget(m_downloadStatusLbl, 1);
    lay->addWidget(dlRow);

    /* ── Verifica integrità GGUF ── */
    auto* sep2 = new QFrame(grp); sep2->setFrameShape(QFrame::HLine);
    sep2->setObjectName("sidebarSep"); lay->addWidget(sep2);

    auto* ggufRow  = new QWidget(grp);
    auto* ggufRowL = new QHBoxLayout(ggufRow);
    ggufRowL->setContentsMargins(0,0,0,0); ggufRowL->setSpacing(8);
    m_btnVerifyGguf = new QPushButton(
        "\xf0\x9f\x94\x92  Verifica integrit\xc3\xa0 GGUF", ggufRow);
    m_btnVerifyGguf->setObjectName("actionBtn");
    m_btnVerifyGguf->setToolTip(tr("Calcola SHA-256 dei file .gguf in models/ e confronta con le firme salvate"));
    m_btnVerifyGguf->setAccessibleName(tr("Verifica integrità file modelli GGUF tramite SHA-256"));
    m_ggufStatusLbl = new QLabel("", ggufRow);
    m_ggufStatusLbl->setObjectName("cardDesc");
    m_ggufStatusLbl->setWordWrap(true);
    ggufRowL->addWidget(m_btnVerifyGguf);
    ggufRowL->addWidget(m_ggufStatusLbl, 1);
    lay->addWidget(ggufRow);

    /* ── Backup automatico KNOWLEDGE_USER/ ── */
    auto* sep3 = new QFrame(grp); sep3->setFrameShape(QFrame::HLine);
    sep3->setObjectName("sidebarSep"); lay->addWidget(sep3);

    auto* bkRow  = new QWidget(grp);
    auto* bkRowL = new QHBoxLayout(bkRow);
    bkRowL->setContentsMargins(0,0,0,0); bkRowL->setSpacing(8);
    auto* bkBtn = new QPushButton(
        "\xf0\x9f\x92\xbe  Backup conoscenza ora", bkRow);
    bkBtn->setObjectName("actionBtn");
    bkBtn->setAccessibleName(tr("Esegui backup manuale della cartella KNOWLEDGE_USER"));
    m_backupStatusLbl = new QLabel(tr("Backup automatico ogni 24h."), bkRow);
    m_backupStatusLbl->setObjectName("cardDesc");
    bkRowL->addWidget(bkBtn);
    bkRowL->addWidget(m_backupStatusLbl, 1);
    lay->addWidget(bkRow);

    connect(verBtn,           &QPushButton::clicked, this, &ManutenzioneePage::onVerifyOllamaVersion);
    connect(m_updAllBtn,      &QPushButton::clicked, this, &ManutenzioneePage::onUpdAllBtnClicked);
    connect(m_updLlamaBtn,    &QPushButton::clicked, this, &ManutenzioneePage::onUpdLlamaBtnClicked);
    connect(m_btnDownloadModel,&QPushButton::clicked, this, &ManutenzioneePage::onDownloadModelClicked);
    connect(m_btnVerifyGguf,  &QPushButton::clicked, this, &ManutenzioneePage::onVerifyGgufClicked);
    connect(bkBtn,            &QPushButton::clicked, this, &ManutenzioneePage::onManualBackupClicked);

    return grp;
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

    auto* grpFmt = new QGroupBox(tr("\xf0\x9f\x93\x84  Formato Config"), page);
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

    fmtRowL->addWidget(new QLabel(tr("Formato:"), fmtRow));
    m_cmbFmt = new QComboBox(fmtRow);
    m_cmbFmt->addItem("JSON  (.prismalux_config.json)", QString("json"));
    m_cmbFmt->addItem("TOON  (.prismalux_config.toon)", QString("toon"));
    {
        QString cur = detectConfigFmt();
        int idx = m_cmbFmt->findData(cur);
        if (idx >= 0) m_cmbFmt->setCurrentIndex(idx);
    }

    auto* fmtApply = new QPushButton(tr("Converti e salva \xe2\x96\xb6"), fmtRow);
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
   buildHardware() — entry point: assembla le sezioni in una pagina.
   ══════════════════════════════════════════════════════════════ */
QWidget* ManutenzioneePage::buildHardware()
{
    return buildHardwarePage();
}

/* ── Livello 1: struttura pagina hardware ────────────────────── */
QWidget* ManutenzioneePage::buildHardwarePage()
{
    auto* page    = new QWidget;
    auto* mainLay = new QVBoxLayout(page);
    mainLay->setContentsMargins(16, 14, 16, 14);
    mainLay->setSpacing(12);

#ifndef Q_OS_WIN
    mainLay->addWidget(buildZramWarningBanner(page));
#endif

    auto* colsRow = new QWidget(page);
    auto* colsLay = new QHBoxLayout(colsRow);
    colsLay->setContentsMargins(0, 0, 0, 0);
    colsLay->setSpacing(16);
    colsLay->addWidget(buildInfoHardwareGroup(colsRow));
    colsLay->addWidget(buildRamOptGroup(colsRow), 1);
    mainLay->addWidget(colsRow);

    auto* computeGroup = buildComputeModeGroup(page);
    auto* npuGroup     = buildNpuGroup(page);

    auto* bottomRow = new QHBoxLayout;
    bottomRow->setSpacing(12);
    bottomRow->addWidget(computeGroup, 55);
    bottomRow->addWidget(npuGroup,     45);
    mainLay->addLayout(bottomRow);

    mainLay->addStretch(1);

    auto* sc = new QScrollArea;
    sc->setWidgetResizable(true);
    sc->setFrameShape(QFrame::NoFrame);
    sc->setWidget(page);
    return sc;
}

/* ── Livello 2a: banner avviso RAM bassa (Linux only) ─────────── */
QLabel* ManutenzioneePage::buildZramWarningBanner(QWidget* parent)
{
    m_zramWarnLbl = new QLabel(
        "\xe2\x9a\xa0  <b>RAM libera bassa (&lt;20%)</b> \xe2\x80\x94 "
        "attiva zRAM per guadagnare ~30-40% di memoria effettiva:<br>"
        "<code>sudo systemctl enable --now systemd-zram-setup@zram0</code><br>"
        "Oppure usa i pulsanti <b>Abilita zRAM</b> qui sotto \xe2\x86\x93", parent);
    m_zramWarnLbl->setTextFormat(Qt::RichText);
    m_zramWarnLbl->setWordWrap(true);
    m_zramWarnLbl->setStyleSheet(
        "background:#6b3a00; color:#ffe0a0; "
        "border:1px solid #c07800; border-radius:5px; padding:8px;");
    m_zramWarnLbl->hide();
    return m_zramWarnLbl;
}

/* ── Livello 2b: colonna sinistra — Info Hardware ─────────────── */
QGroupBox* ManutenzioneePage::buildInfoHardwareGroup(QWidget* parent)
{
    auto* grp = new QGroupBox(tr("\xf0\x9f\x96\xa5  Info Hardware"), parent);
    grp->setObjectName("cardGroup");
    grp->setFixedWidth(dpiScale(220));
    auto* lay = new QVBoxLayout(grp);

    m_hwLabel = new QLabel(tr("\xe2\x8f\xb3  Rilevamento hardware in corso..."), grp);
    m_hwLabel->setObjectName("cardDesc");
    m_hwLabel->setWordWrap(true);
    m_hwLabel->setStyleSheet(
        "font-family:'Consolas','Courier New',monospace; "
        "color:#a0a4c0; padding:4px;");
    lay->addWidget(m_hwLabel);
    lay->addStretch(1);

    return grp;
}

/* ── Livello 2c: colonna destra — Ottimizzazione RAM ──────────── */
QGroupBox* ManutenzioneePage::buildRamOptGroup(QWidget* parent)
{
    auto* grp = new QGroupBox(tr("\xf0\x9f\x92\xbe  Ottimizzazione RAM"), parent);
    grp->setObjectName("cardGroup");
    auto* lay = new QVBoxLayout(grp);
    lay->setSpacing(8);

    m_ramStatusLbl = new QLabel(
        "Premi \"\xf0\x9f\x94\x8d Rileva\" per controllare lo stato della compressione RAM.",
        grp);
    m_ramStatusLbl->setObjectName("cardDesc");
    m_ramStatusLbl->setWordWrap(true);
    lay->addWidget(m_ramStatusLbl);

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
        grp);
    ramDesc->setObjectName("cardDesc");
    ramDesc->setWordWrap(true);
    lay->addWidget(ramDesc);

#ifndef Q_OS_WIN
    auto* autoZramRow = new QWidget(grp);
    auto* autoZramLay = new QHBoxLayout(autoZramRow);
    autoZramLay->setContentsMargins(0, 0, 0, 0);
    autoZramLay->setSpacing(8);
    auto* autoZramCb = new QCheckBox(
        "\xf0\x9f\x92\xbe\xf0\x9f\x92\xbe  Abilita Doppia zstd automaticamente all'avvio", grp);
    autoZramCb->setObjectName("cardDesc");
    {
        QSettings s("Prismalux", "GUI");
        autoZramCb->setChecked(s.value(P::SK::kAutoZramDoppia, true).toBool());
    }
    autoZramLay->addWidget(autoZramCb);
    autoZramLay->addStretch();
    lay->addWidget(autoZramRow);
    connect(autoZramCb, &QCheckBox::toggled, this, &ManutenzioneePage::onAutoZramCbToggled);
#endif

    lay->addWidget(buildRamButtonRow(grp));

    m_ramLog = new QTextEdit(grp);
    m_ramLog->setReadOnly(true);
    m_ramLog->setMaximumHeight(110);
    m_ramLog->setStyleSheet(
        "font-family:'Consolas','Courier New',monospace; font-size:10px;");
    m_ramLog->setPlaceholderText(tr("Output comandi..."));
    lay->addWidget(m_ramLog);

    return grp;
}

/* ── Livello 3: riga pulsanti RAM/zRAM ────────────────────────── */
QWidget* ManutenzioneePage::buildRamButtonRow(QGroupBox* grp)
{
    auto* row = new QWidget(grp);
    auto* lay = new QHBoxLayout(row);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(8);

    auto* detectBtn = new QPushButton(tr("\xf0\x9f\x94\x8d  Rileva stato"), grp);
    detectBtn->setObjectName("actionBtn");
    lay->addWidget(detectBtn);

#ifdef Q_OS_WIN
    auto* compBtn    = new QPushButton(tr("\xf0\x9f\x92\xbe  Attiva compressione"), grp);
    compBtn->setObjectName("actionBtn");
    auto* disableBtn = new QPushButton(tr("\xe2\x8f\xb9  Disattiva"), grp);
    disableBtn->setObjectName("actionBtn");
    lay->addWidget(compBtn);
    lay->addWidget(disableBtn);
    connect(compBtn,    &QPushButton::clicked, this, &ManutenzioneePage::onCompBtnClicked);
    connect(disableBtn, &QPushButton::clicked, this, &ManutenzioneePage::onDisableRamBtnClicked);
#else
    auto* singBtn   = new QPushButton(tr("\xf0\x9f\x92\xbe  Singola (lz4)"), grp);
    singBtn->setObjectName("actionBtn");
    auto* doppiaBtn = new QPushButton(tr("\xf0\x9f\x92\xbe\xf0\x9f\x92\xbe  Doppia (zstd)"), grp);
    doppiaBtn->setObjectName("actionBtn");
    auto* disableBtn = new QPushButton(tr("\xe2\x8f\xb9  Disattiva zRAM"), grp);
    disableBtn->setObjectName("actionBtn");
    lay->addWidget(singBtn);
    lay->addWidget(doppiaBtn);
    lay->addWidget(disableBtn);
    connect(singBtn,    &QPushButton::clicked, this, &ManutenzioneePage::onSingBtnClicked);
    connect(doppiaBtn,  &QPushButton::clicked, this, &ManutenzioneePage::onDoppiaBtnClicked);
    connect(disableBtn, &QPushButton::clicked, this, &ManutenzioneePage::onDisableRamBtnClicked);
#endif
    lay->addStretch(1);

    connect(detectBtn, &QPushButton::clicked, this, &ManutenzioneePage::onDetectBtnClicked);

    return row;
}

/* ── Livello 2d: pannello Modalità Calcolo LLM ────────────────── */
QGroupBox* ManutenzioneePage::buildComputeModeGroup(QWidget* parent)
{
    auto* grp = new QGroupBox(
        "\xf0\x9f\x92\xbb  Modalit\xc3\xa0 Calcolo LLM", parent);
    grp->setObjectName("cardGroup");
    auto* lay = new QVBoxLayout(grp);
    lay->setSpacing(8);

    auto* compDesc = new QLabel(
        "Scegli dove eseguire il modello. "
        "Il default viene rilevato automaticamente confrontando RAM e VRAM.", grp);
    compDesc->setObjectName("cardDesc");
    compDesc->setWordWrap(true);
    lay->addWidget(compDesc);

    /* Riga pulsanti modalità */
    auto* btnRow = new QWidget(grp);
    auto* btnLay = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(0, 0, 0, 0);
    btnLay->setSpacing(10);

    m_btnGpu    = new QPushButton(tr("\xf0\x9f\x9a\x80  GPU  (VRAM)"), grp);
    m_btnCpu    = new QPushButton(tr("\xf0\x9f\x96\xa5  CPU  (RAM)"),  grp);
    m_btnMisto  = new QPushButton(tr("\xe2\x9a\x96\xef\xb8\x8f  Misto GPU+CPU"), grp);
    m_btnDoppia = new QPushButton(tr("\xf0\x9f\x94\x97  Doppia GPU"), grp);
    for (auto* b : {m_btnGpu, m_btnCpu, m_btnMisto, m_btnDoppia}) {
        b->setObjectName("actionBtn");
        b->setMinimumWidth(140);
    }
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
    m_btnDoppia->setEnabled(false);

    btnLay->addWidget(m_btnGpu);
    btnLay->addWidget(m_btnCpu);
    btnLay->addWidget(m_btnMisto);
    btnLay->addWidget(m_btnDoppia);
    btnLay->addStretch(1);
    lay->addWidget(btnRow);

    m_computeInfo = new QLabel(
        "\xe2\x8f\xb3  In attesa rilevamento hardware...", grp);
    m_computeInfo->setObjectName("cardDesc");
    m_computeInfo->setWordWrap(true);
    lay->addWidget(m_computeInfo);

    /* Riga salva */
    auto* saveRow = new QWidget(grp);
    auto* saveLay = new QHBoxLayout(saveRow);
    saveLay->setContentsMargins(0, 4, 0, 0);
    saveLay->setSpacing(10);
    m_btnSaveMode = new QPushButton(
        "\xf0\x9f\x92\xbe  Salva modalit\xc3\xa0", grp);
    m_btnSaveMode->setObjectName("actionBtn");
    m_btnSaveMode->setEnabled(false);
    m_btnSaveMode->setToolTip(
        "Applica la modalit\xc3\xa0 selezionata e la salva per i prossimi avvii.");
    saveLay->addWidget(m_btnSaveMode);
    saveLay->addStretch(1);
    lay->addWidget(saveRow);

    connect(m_btnGpu,     &QPushButton::clicked, this, &ManutenzioneePage::onBtnGpuClicked);
    connect(m_btnCpu,     &QPushButton::clicked, this, &ManutenzioneePage::onBtnCpuClicked);
    connect(m_btnMisto,   &QPushButton::clicked, this, &ManutenzioneePage::onBtnMistoClicked);
    connect(m_btnDoppia,  &QPushButton::clicked, this, &ManutenzioneePage::onBtnDoppiaClicked);
    connect(m_btnSaveMode,&QPushButton::clicked, this, &ManutenzioneePage::onBtnSaveModeClicked);
    connect(m_ai, &AiClient::modelChanged, this, &ManutenzioneePage::onAiModelChangedApplyMode);

    return grp;
}

/* ── Livello 2e: pannello NPU ─────────────────────────────────── */
QGroupBox* ManutenzioneePage::buildNpuGroup(QWidget* parent)
{
    auto* grp = new QGroupBox(
        "\xf0\x9f\xa7\xa0  NPU \xe2\x80\x94 Neural Processing Unit", parent);
    grp->setObjectName("cardGroup");
    auto* lay = new QVBoxLayout(grp);

    auto* npuDesc = new QLabel(
        "Le NPU accelerano l\xe2\x80\x99"
        "inferenza AI con consumo energetico ridotto rispetto a GPU/CPU.\n"
        "Supportate: <b>Intel NPU</b> (Core Ultra) \xe2\x80\x94 "
        "<b>AMD NPU</b> (Ryzen AI \xe2\x80\x94 beta).", grp);
    npuDesc->setWordWrap(true);
    npuDesc->setTextFormat(Qt::RichText);
    npuDesc->setObjectName("hintLabel");
    lay->addWidget(npuDesc);

    auto* npuStatusLbl = new QLabel(tr("\xe2\x8f\xb3  Rilevamento in corso..."), grp);
    npuStatusLbl->setObjectName("cardDesc");
    npuStatusLbl->setWordWrap(true);
    lay->addWidget(npuStatusLbl);

    /* Rileva NPU al momento della costruzione */
    QStringList found;
#ifdef Q_OS_LINUX
    if (QFile::exists("/dev/accel/accel0"))
        found << "\xe2\x9c\x85  Intel NPU rilevata (/dev/accel/accel0)";
    QFile modules("/proc/modules");
    if (modules.open(QFile::ReadOnly)) {
        const QString content = modules.readAll();
        if (content.contains("amdxdna"))
            found << "\xe2\x9c\x85  AMD NPU rilevata (modulo amdxdna)";
    }
#endif
    npuStatusLbl->setText(found.isEmpty()
        ? "\xe2\x9d\x8c  Nessuna NPU rilevata (o driver non installato)"
        : found.join("\n"));

    auto* btnIntelNpu = new QPushButton(
        "\xf0\x9f\x94\xb5  Installa intel-npu-acceleration-library", grp);
    btnIntelNpu->setObjectName("actionBtn");
    btnIntelNpu->setToolTip(
        "pip install intel-npu-acceleration-library\n"
        "Richiede: Intel Core Ultra (Meteor Lake+) con driver NPU");
    lay->addWidget(btnIntelNpu, 0, Qt::AlignLeft);

    auto* npuHintRow = new QHBoxLayout;
    auto* npuHint = new QLabel(
        "\xe2\x84\xb9  AMD NPU (XDNA): usa <b>https://github.com/amd/iron</b> "
        "\xe2\x80\x94 attualmente in beta, stabilit\xc3\xa0 non garantita.\n"
        "Intel NPU: stabile, richiede <code>pip install intel-npu-acceleration-library</code>.",
        grp);
    npuHint->setWordWrap(true);
    npuHint->setTextFormat(Qt::RichText);
    npuHint->setObjectName("hintLabel");
    auto* copyBtnNpu = new QPushButton("\xf0\x9f\x93\x8b", grp);  /* 📋 */
    copyBtnNpu->setObjectName("actionBtn");
    copyBtnNpu->setFixedWidth(dpiScale(28));
    copyBtnNpu->setFixedHeight(dpiScale(24));
    copyBtnNpu->setToolTip(tr("Copia comando pip negli appunti"));
    connect(copyBtnNpu, &QPushButton::clicked, grp, [=]() {
        QApplication::clipboard()->setText(tr("pip install intel-npu-acceleration-library"));
    });
    npuHintRow->addWidget(npuHint, 1);
    npuHintRow->addWidget(copyBtnNpu);
    lay->addLayout(npuHintRow);
    lay->addStretch(1);

    connect(btnIntelNpu, &QPushButton::clicked,
            this, &ManutenzioneePage::onBtnIntelNpuClicked);

    return grp;
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

        m_ai->fetchModelLayers([guard=QPointer<ManutenzioneePage>(this)](int layers) {
            if (!guard || !guard->m_ai) return;
            guard->m_ai->unloadModel();
            guard->m_ai->setNumGpu(layers > 0 ? layers : -2);
            if (guard->m_computeInfo)
                guard->m_computeInfo->setText(layers > 0
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

        m_ai->fetchModelLayers([guard=QPointer<ManutenzioneePage>(this)](int layers) {
            if (!guard || !guard->m_ai) return;
            /* Riempie NVIDIA al massimo: min(layer modello, capacit\xc3\xa0 VRAM NVIDIA).
             * Fallback conservativo se layers=0 (modello non caricato): 8 layer. */
            const int capacity = (guard->m_gpuLayersFull > 0) ? guard->m_gpuLayersFull : 8;
            const int gpuLayers = (layers > 0) ? qMin(layers, capacity) : 8;
            const int total     = (layers > 0) ? layers : 16;
            guard->m_ai->unloadModel();
            guard->m_ai->setNumGpu(gpuLayers);
            if (guard->m_computeInfo) {
                if (gpuLayers >= total)
                    guard->m_computeInfo->setText(
                        QString("\xe2\x9c\x85  <b>Misto</b> — tutti i %1 layer su GPU "
                                "(il modello entra interamente in VRAM: valuta modalit\xc3\xa0 GPU pura).")
                        .arg(gpuLayers));
                else
                    guard->m_computeInfo->setText(
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
        int     ollamaPort   = P::kOllamaPort;
        QString ollamaModel;
        QString lserverHost  = "127.0.0.1";
        int     lserverPort  = P::kLlamaServerPort;
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
        if (cfg.ollamaPort  == 0) cfg.ollamaPort  = P::kOllamaPort;
        if (cfg.lserverPort == 0) cfg.lserverPort = P::kLlamaServerPort;
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
        LogBus::post("\xe2\x9d\x8c Manutenzione: Nessun file .gguf selezionato per llama-server.");
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
        LogBus::post("\xe2\x9d\x8c Manutenzione: llama-server non trovato nel PATH.");
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
    else {
        m_fmtStatus->setText(
            QString("\xe2\x9d\x8c  %1").arg(err));
        LogBus::post("\xe2\x9d\x8c Manutenzione: Errore conversione config: " + err);
    }
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
    if (list.isEmpty()) { ModelComboHelper::setError(m_cmbModel); return; }
    ModelComboHelper::populate(m_cmbModel, m_ai, list);
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
    if (m_verLbl) m_verLbl->setText(tr("\xf0\x9f\x90\xb3  Ollama: <i>verifica...</i>"));
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

ManutenzioneePage::~ManutenzioneePage()
{
    /* Blocca i segnali di tutti i QProcess figli prima che Qt li distrugga.
     * Senza questo, il distruttore di QProcess chiama waitForFinished() che
     * emette finished() su slot che trovano widget già parzialmente distrutti
     * → SIGSEGV (coredump 2026-05-31: onOllamaVerProcFinished → setText). */
    for (QProcess* p : {m_srvProc, m_ollamaVerProc, m_listProc, m_gitProc,
                        m_ramCmdProc, m_updPullProc, m_downloadProc, m_sha256Proc}) {
        if (p) { p->blockSignals(true); p->kill(); }
    }
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
            m_verLbl->setText(tr("\xf0\x9f\x90\xb3  Ollama: <span style='color:#ef4444;'>non trovato</span>"));
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
        m_verLbl->setText(tr("\xf0\x9f\x90\xb3  Ollama: <span style='color:#ef4444;'>non trovato nel PATH</span>"));
}

/* ══════════════════════════════════════════════════════════════
   Slot — Aggiorna tutti i modelli Ollama
   ══════════════════════════════════════════════════════════════ */
void ManutenzioneePage::onUpdAllBtnClicked()
{
    if (m_updAllBtn) m_updAllBtn->setEnabled(false);
    if (m_updLog) m_updLog->clear();
    if (m_updStatusLbl) m_updStatusLbl->setText(tr("\xf0\x9f\x94\x84  Recupero lista modelli..."));

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
        if (m_updStatusLbl) m_updStatusLbl->setText(tr("\xe2\x9d\x8c  Nessun modello trovato. Ollama in esecuzione?"));
        LogBus::post("\xe2\x9d\x8c Manutenzione: Nessun modello trovato. Ollama in esecuzione?");
        if (m_updAllBtn) m_updAllBtn->setEnabled(true);
        return;
    }

    if (m_updLog)
        m_updLog->append(QString("\xf0\x9f\x93\x8b  %1 modelli da aggiornare: %2")
            .arg(models.size()).arg(models.join(", ")));
    if (m_updStatusLbl)
        m_updStatusLbl->setText(QString("\xf0\x9f\x94\x84  Aggiornamento 1/%1...").arg(models.size()));

    /* Aggiornamento sequenziale: stato salvato in membri, avanzamento via slot nominati */
    m_updModels = models;
    m_updIdx    = 0;
    m_updTotal  = models.size();
    updNextModel();
}

void ManutenzioneePage::onListProcError(QProcess::ProcessError)
{
    if (m_listProc) {
        m_listProc->deleteLater();
        m_listProc = nullptr;
    }
    if (m_updStatusLbl) m_updStatusLbl->setText(tr("\xe2\x9d\x8c  Ollama non trovato. Verifica il PATH."));
    LogBus::post("\xe2\x9d\x8c Manutenzione: Ollama non trovato nel PATH.");
    if (m_updAllBtn) m_updAllBtn->setEnabled(true);
}

/* ── Livello 3: avvia ollama pull per il modello corrente ────── */
void ManutenzioneePage::updNextModel()
{
    if (m_updIdx >= m_updTotal) {
        if (m_updStatusLbl)
            m_updStatusLbl->setText(
                QString("\xe2\x9c\x85  Aggiornamento completato! %1 modelli").arg(m_updTotal));
        if (m_updLog)
            m_updLog->append("\n\xe2\x9c\x85  Tutti i modelli sono aggiornati.");
        if (m_updAllBtn) m_updAllBtn->setEnabled(true);
        return;
    }

    const QString mdl = m_updModels.at(m_updIdx);
    if (m_updStatusLbl)
        m_updStatusLbl->setText(
            QString("\xf0\x9f\x94\x84  Aggiornamento %1/%2: %3")
            .arg(m_updIdx + 1).arg(m_updTotal).arg(mdl));
    if (m_updLog)
        m_updLog->append(
            QString("\n\xe2\xac\x87  Aggiornamento: <b>%1</b>...").arg(mdl));

    if (m_updPullProc) {
        m_updPullProc->kill();
        m_updPullProc->deleteLater();
        m_updPullProc = nullptr;
    }
    m_updPullProc = new QProcess(this);
    m_updPullProc->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_updPullProc, &QProcess::readyRead,
            this, &ManutenzioneePage::onUpdPullReadyRead);
    connect(m_updPullProc,
            QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ManutenzioneePage::onUpdPullFinished);
    m_updPullProc->start("ollama", {"pull", mdl});
}

void ManutenzioneePage::onUpdPullReadyRead()
{
    if (!m_updPullProc || !m_updLog) return;
    const QString out = QString::fromLocal8Bit(m_updPullProc->readAll());
    if (!out.trimmed().isEmpty())
        m_updLog->append(out.trimmed());
}

void ManutenzioneePage::onUpdPullFinished(int code, QProcess::ExitStatus)
{
    const QString mdl = (m_updIdx < m_updModels.size()) ? m_updModels.at(m_updIdx) : "?";
    if (m_updLog) {
        if (code == 0)
            m_updLog->append(
                QString("\xe2\x9c\x85  <b>%1</b> aggiornato.").arg(mdl));
        else
            m_updLog->append(
                QString("\xe2\x9a\xa0  <b>%1</b> — errore (codice %2).").arg(mdl).arg(code));
    }
    if (m_updPullProc) {
        m_updPullProc->deleteLater();
        m_updPullProc = nullptr;
    }
    ++m_updIdx;
    updNextModel();
}

/* ══════════════════════════════════════════════════════════════
   Slot — Aggiorna llama.cpp (git pull) + scansiona GGUF
   ══════════════════════════════════════════════════════════════ */
void ManutenzioneePage::onUpdLlamaBtnClicked()
{
    if (m_updLlamaBtn) m_updLlamaBtn->setEnabled(false);
    if (m_updLog) m_updLog->clear();
    if (m_updStatusLbl) m_updStatusLbl->setText(tr("\xf0\x9f\x94\x84  Scansione modelli GGUF..."));

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
    for (const QString& c : {P::root() + "/ENGINE_LLM/llama_cpp_studio/llama.cpp",
                              P::root() + "/ENGINE_LLM/llama.cpp"}) {
        if (QDir(c + "/.git").exists()) { llamaDir = c; break; }
    }

    if (llamaDir.isEmpty()) {
        if (m_updLog) {
            m_updLog->append("\n\xe2\x84\xb9  Repository llama.cpp non trovato — aggiornamento git non disponibile.");
            m_updLog->append("   I file .gguf vanno aggiornati manualmente da HuggingFace Hub.");
        }
        if (m_updStatusLbl) m_updStatusLbl->setText(tr("\xe2\x9c\x85  Scansione completata."));
        if (m_updLlamaBtn) m_updLlamaBtn->setEnabled(true);
        return;
    }

    if (m_updLog) m_updLog->append(QString("\n\xf0\x9f\x94\x84  git pull: %1").arg(llamaDir));
    if (m_updStatusLbl) m_updStatusLbl->setText(tr("\xf0\x9f\x94\x84  Aggiornamento llama.cpp..."));

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
            m_updStatusLbl->setText(tr("\xe2\x9c\x85  llama.cpp aggiornato."));
        else
            m_updStatusLbl->setText(tr("\xe2\x9a\xa0  git pull fallito (verifica connessione)."));
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
    LogBus::post("\xe2\x9d\x8c Manutenzione: git non trovato nel PATH.");
    if (m_updStatusLbl) m_updStatusLbl->setText(tr("\xe2\x9a\xa0  git non disponibile."));
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
    if (!m_ramCmdProc->waitForStarted(P::kProcessStartTimeoutMs)) {
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
    runRamCmd(P::findPython(), {"-m", "pip", "install", "--break-system-packages",
               "intel-npu-acceleration-library"},
               "pip install intel-npu-acceleration-library");
}

/* ══════════════════════════════════════════════════════════════
   SCARICA NUOVO MODELLO OLLAMA
   ══════════════════════════════════════════════════════════════ */
void ManutenzioneePage::onDownloadModelClicked()
{
    if (!m_downloadModelEdit) return;
    const QString model = m_downloadModelEdit->text().trimmed();
    if (model.isEmpty()) {
        if (m_downloadStatusLbl) m_downloadStatusLbl->setText(
            "\xe2\x9d\x8c  Inserisci il nome del modello.");
        return;
    }
    if (m_downloadProc && m_downloadProc->state() != QProcess::NotRunning) {
        m_downloadProc->kill();
        return;
    }

    if (m_updLog) {
        m_updLog->append(QString("\n\xe2\xac\x87  Download: <b>%1</b>...")
                         .arg(model.toHtmlEscaped()));
    }
    if (m_downloadStatusLbl) m_downloadStatusLbl->setText(tr("\xe2\x8f\xb3  Download..."));
    if (m_btnDownloadModel)  m_btnDownloadModel->setText(tr("\xe2\x8f\xb9  Annulla"));
    emit downloadStarted(model);

    if (m_downloadProc) { m_downloadProc->deleteLater(); m_downloadProc = nullptr; }
    m_downloadProc = new QProcess(this);
    m_downloadProc->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_downloadProc, &QProcess::readyRead,
            this, &ManutenzioneePage::onDownloadProcReadyRead);
    connect(m_downloadProc,
            QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ManutenzioneePage::onDownloadProcFinished);
    m_downloadProc->start("ollama", {"pull", model});
}

void ManutenzioneePage::onDownloadProcReadyRead()
{
    if (!m_downloadProc || !m_updLog) return;
    const QString out = QString::fromLocal8Bit(m_downloadProc->readAll());
    m_updLog->moveCursor(QTextCursor::End);
    m_updLog->insertPlainText(out);
    m_updLog->moveCursor(QTextCursor::End);

    /* Estrai l'ultima riga significativa per il segnale di progresso globale */
    const QStringList lines = out.split('\n', Qt::SkipEmptyParts);
    for (int i = lines.size() - 1; i >= 0; --i) {
        const QString line = lines[i].trimmed();
        if (!line.isEmpty()) {
            emit downloadProgress(line);
            break;
        }
    }
}

void ManutenzioneePage::onDownloadProcFinished(int code, QProcess::ExitStatus)
{
    const QString model = m_downloadModelEdit ? m_downloadModelEdit->text().trimmed()
                                              : QString("modello");
    if (m_btnDownloadModel) m_btnDownloadModel->setText(tr("\xe2\xac\x87  Scarica"));
    if (code == 0) {
        if (m_downloadStatusLbl) m_downloadStatusLbl->setText(
            "\xe2\x9c\x85  Download completato.");
        if (m_updLog) m_updLog->append(
            "\xe2\x9c\x85  <b>Modello scaricato con successo.</b>\n");
    } else {
        if (m_downloadStatusLbl) m_downloadStatusLbl->setText(
            "\xe2\x9d\x8c  Download fallito.");
        if (m_updLog) m_updLog->append(
            "\xe2\x9d\x8c  Download terminato con errore.\n");
    }
    emit downloadFinished(code == 0, model);
    if (m_downloadProc) { m_downloadProc->deleteLater(); m_downloadProc = nullptr; }
    if (m_ai) m_ai->fetchModels();
}

/* ══════════════════════════════════════════════════════════════
   VERIFICA INTEGRITÀ GGUF (SHA-256)
   ══════════════════════════════════════════════════════════════ */
static QString ggufHashesPath()
{
    namespace P = PrismaluxPaths;
    const QString dir = P::root() + "/KNOWLEDGE_USER";
    QDir().mkpath(dir);
    return dir + "/model_hashes.json";
}

void ManutenzioneePage::loadGgufHashes()
{
    m_ggufHashes.clear();
    QFile f(ggufHashesPath());
    if (!f.open(QIODevice::ReadOnly)) return;
    const QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
    for (auto it = obj.begin(); it != obj.end(); ++it)
        m_ggufHashes.insert(it.key(), it.value().toString());
}

void ManutenzioneePage::saveGgufHashes()
{
    QJsonObject obj;
    for (auto it = m_ggufHashes.cbegin(); it != m_ggufHashes.cend(); ++it)
        obj.insert(it.key(), it.value());
    QFile f(ggufHashesPath());
    if (!f.open(QIODevice::WriteOnly)) return;
    f.write(QJsonDocument(obj).toJson());
}

void ManutenzioneePage::onVerifyGgufClicked()
{
    namespace P = PrismaluxPaths;
    if (m_sha256Proc && m_sha256Proc->state() != QProcess::NotRunning) return;

    const QString modelsDir = P::root() + "/models";
    QDir d(modelsDir);
    const QStringList ggufFiles = d.entryList({"*.gguf"}, QDir::Files);

    if (ggufFiles.isEmpty()) {
        if (m_ggufStatusLbl) m_ggufStatusLbl->setText(
            "\xe2\x84\xb9  Nessun file .gguf trovato in models/");
        return;
    }

    m_ggufToVerify.clear();
    for (const QString& fn : ggufFiles)
        m_ggufToVerify.append(modelsDir + "/" + fn);

    m_ggufVerifyIdx = 0;
    m_sha256Accum.clear();

    if (m_updLog) m_updLog->append(
        QString("\n\xf0\x9f\x94\x92  Verifica integrit\xc3\xa0 %1 file GGUF...\n")
        .arg(ggufFiles.size()));
    if (m_ggufStatusLbl) m_ggufStatusLbl->setText(
        "\xe2\x8f\xb3  Verifica in corso...");
    if (m_btnVerifyGguf) m_btnVerifyGguf->setEnabled(false);

    ggufVerifyNext();
}

void ManutenzioneePage::ggufVerifyNext()
{
    if (m_ggufVerifyIdx >= m_ggufToVerify.size()) {
        /* Tutti verificati */
        saveGgufHashes();
        if (m_ggufStatusLbl) m_ggufStatusLbl->setText(
            "\xe2\x9c\x85  Verifica completata. Firme salvate.");
        if (m_btnVerifyGguf) m_btnVerifyGguf->setEnabled(true);
        return;
    }

    const QString path = m_ggufToVerify.at(m_ggufVerifyIdx);
    m_sha256Accum.clear();

    if (m_sha256Proc) { m_sha256Proc->deleteLater(); m_sha256Proc = nullptr; }
    m_sha256Proc = new QProcess(this);
    m_sha256Proc->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_sha256Proc, &QProcess::readyRead,
            this, &ManutenzioneePage::onSha256ProcReadyRead);
    connect(m_sha256Proc,
            QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ManutenzioneePage::onSha256ProcFinished);
    m_sha256Proc->start("sha256sum", {path});
}

void ManutenzioneePage::onSha256ProcReadyRead()
{
    if (m_sha256Proc)
        m_sha256Accum += QString::fromLocal8Bit(m_sha256Proc->readAll());
}

void ManutenzioneePage::onSha256ProcFinished(int code, QProcess::ExitStatus)
{
    if (m_sha256Proc) { m_sha256Proc->deleteLater(); m_sha256Proc = nullptr; }

    const QString path = m_ggufToVerify.value(m_ggufVerifyIdx);
    const QString filename = QFileInfo(path).fileName();

    if (code == 0) {
        const QString computed = m_sha256Accum.section(' ', 0, 0).trimmed();
        const QString stored   = m_ggufHashes.value(filename);

        QString line;
        if (stored.isEmpty()) {
            /* Prima verifica: salva la firma */
            m_ggufHashes.insert(filename, computed);
            line = QString("\xf0\x9f\x94\x92  %1 \xe2\x80\x94 firma registrata").arg(filename);
        } else if (stored == computed) {
            line = QString("\xe2\x9c\x85  %1 \xe2\x80\x94 OK").arg(filename);
        } else {
            line = QString("\xe2\x9d\x8c  %1 \xe2\x80\x94 <b>FIRMA NON CORRISPONDENTE</b> "
                           "(file corrotto o sostituito!)").arg(filename);
            m_ggufHashes.insert(filename, computed);   /* aggiorna con nuovo hash */
        }
        if (m_updLog) {
            m_updLog->moveCursor(QTextCursor::End);
            m_updLog->append(line);
        }
    } else {
        if (m_updLog) m_updLog->append(
            QString("\xe2\x9d\x8c  Errore sha256sum su %1").arg(filename));
    }

    ++m_ggufVerifyIdx;
    ggufVerifyNext();
}

/* ══════════════════════════════════════════════════════════════
   BACKUP AUTOMATICO KNOWLEDGE_USER/
   ══════════════════════════════════════════════════════════════ */
void ManutenzioneePage::installKnowledgeBackupTimer()
{
    m_backupTimer = new QTimer(this);
    m_backupTimer->setInterval(24 * 60 * 60 * 1000);   /* 24 ore */
    m_backupTimer->setSingleShot(false);
    connect(m_backupTimer, &QTimer::timeout,
            this, &ManutenzioneePage::onManualBackupClicked);
    m_backupTimer->start();
}

void ManutenzioneePage::onManualBackupClicked()
{
    performKnowledgeBackup();
}

void ManutenzioneePage::performKnowledgeBackup()
{
    namespace P = PrismaluxPaths;
    const QString srcDir  = P::root() + "/KNOWLEDGE_USER";
    const QString dateTag = QDateTime::currentDateTime().toString("yyyy-MM-dd_HHmmss");
    const QString dstDir  = srcDir + "/.backup/" + dateTag;

    if (!QDir().mkpath(dstDir)) {
        if (m_backupStatusLbl) m_backupStatusLbl->setText(
            "\xe2\x9d\x8c  Impossibile creare la cartella di backup.");
        return;
    }

    /* Copia ricorsiva dei file in KNOWLEDGE_USER/ (un livello) */
    QDir src(srcDir);
    int copied = 0;
    for (const QString& fn : src.entryList(QDir::Files)) {
        QFile::copy(src.absoluteFilePath(fn), dstDir + "/" + fn);
        ++copied;
    }

    /* Mantieni solo gli ultimi 7 backup */
    QDir backupRoot(srcDir + "/.backup");
    QStringList entries = backupRoot.entryList(QDir::Dirs | QDir::NoDotAndDotDot,
                                               QDir::Name);
    while (entries.size() > 7) {
        const QString oldest = entries.takeFirst();
        QDir old(backupRoot.absoluteFilePath(oldest));
        for (const QString& f : old.entryList(QDir::Files))
            QFile::remove(old.absoluteFilePath(f));
        backupRoot.rmdir(oldest);
    }

    const QString msg = QString("\xf0\x9f\x92\xbe  Backup %1: %2 file  \xe2\x80\x94  %3")
        .arg(dateTag).arg(copied)
        .arg(QDateTime::currentDateTime().toString("HH:mm:ss"));
    if (m_backupStatusLbl) m_backupStatusLbl->setText(msg);
    if (m_updLog)          m_updLog->append(msg);
}

/* ══════════════════════════════════════════════════════════════
   buildSystemUpdates — aggiornamento container Docker + librerie
   Python di Prismalux (requirements.txt). Vedi widget_docker_update.h
   e widget_python_update.h per la logica di ciascun pannello.
   ══════════════════════════════════════════════════════════════ */
QWidget* ManutenzioneePage::buildSystemUpdates()
{
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* page = new QWidget;
    auto* lay  = new QVBoxLayout(page);
    lay->setContentsMargins(16, 14, 16, 14);
    lay->setSpacing(12);

    auto* titleLbl = new QLabel(
        "\xf0\x9f\x94\x84  Aggiornamenti Sistema", page);
    titleLbl->setObjectName("sectionTitle");
    lay->addWidget(titleLbl);

    lay->addWidget(new DockerUpdatePanel(page));
    lay->addWidget(new PythonUpdatePanel(page));
    lay->addStretch();

    scroll->setWidget(page);
    return scroll;
}
