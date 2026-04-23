#include "manutenzione_page.h"
#include "../prismalux_paths.h"
namespace P = PrismaluxPaths;
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

/* ══════════════════════════════════════════════════════════════
   ManutenzioneePage — costruttore minimale.
   Il layout reale vive in buildBackend() / buildConfigFmt() /
   buildHardware(), esposti come tab piatte da ImpostazioniPage.
   ══════════════════════════════════════════════════════════════ */
ManutenzioneePage::ManutenzioneePage(AiClient* ai, HardwareMonitor* hw, QWidget* parent)
    : QWidget(parent), m_ai(ai), m_hw(hw)
{}

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
    leftGroup->setFixedWidth(220);
    auto* leftLay = new QVBoxLayout(leftGroup);
    leftLay->setSpacing(6);

    leftLay->addWidget(new QLabel("Backend:", leftGroup));
    m_cmbBackend = new QComboBox(leftGroup);
    m_cmbBackend->addItem(QString("\xf0\x9f\x90\xb3  Ollama  (:%1)").arg(P::kOllamaPort));
    m_cmbBackend->addItem(QString("\xf0\x9f\xa6\x99  llama-server  (:%1)").arg(P::kLlamaServerPort));
    leftLay->addWidget(m_cmbBackend);

    auto* hostEdit = new QLineEdit("127.0.0.1", leftGroup);
    hostEdit->setObjectName("chatInput");
    hostEdit->setPlaceholderText("Host");
    leftLay->addWidget(new QLabel("Host:", leftGroup));
    leftLay->addWidget(hostEdit);

    auto* portEdit = new QLineEdit("11434", leftGroup);
    portEdit->setObjectName("chatInput");
    portEdit->setPlaceholderText("Porta");
    leftLay->addWidget(new QLabel("Porta:", leftGroup));
    leftLay->addWidget(portEdit);

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
    auto* grpServ = new QGroupBox("\xf0\x9f\xa6\x99  llama.cpp \xe2\x80\x94 Avvia llama-server", rightGroup);
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
    connect(srvBrowse, &QPushButton::clicked, this, [this]{
        QString path = QFileDialog::getOpenFileName(this,
            "Seleziona modello .gguf",
            P::modelsDir(),
            "Modelli GGUF (*.gguf *.bin)");
        if (!path.isEmpty()) m_srvModelPath->setText(path);
    });

    connect(m_srvStartBtn, &QPushButton::clicked, this, [this]{
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
        connect(m_srvProc, &QProcess::readyRead, this, [this]{
            m_srvLog->moveCursor(QTextCursor::End);
            m_srvLog->insertPlainText(QString::fromLocal8Bit(m_srvProc->readAll()));
            m_srvLog->ensureCursorVisible();
        });
        connect(m_srvProc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this](int code, QProcess::ExitStatus){
            m_srvLog->append(QString("\n\xf0\x9f\x94\xb4  Server terminato (code %1).").arg(code));
            m_srvStartBtn->setEnabled(true);
            m_srvStopBtn->setEnabled(false);
            m_srvProc = nullptr;
        });
#ifdef _WIN32
        m_srvProc->start("cmd", {"/c", cmd});
#else
        m_srvProc->start("sh", {"-c", cmd});
#endif
        connect(m_srvProc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError err){
            if (err == QProcess::FailedToStart) {
                m_srvLog->append("\xe2\x9d\x8c  llama-server non trovato. Compilalo nella scheda \xf0\x9f\xa6\x99 llama.cpp.");
                m_srvStartBtn->setEnabled(true);
                m_srvStopBtn->setEnabled(false);
                m_srvProc = nullptr;
            }
        });
    });

    connect(m_srvStopBtn, &QPushButton::clicked, this, [this]{
        if (m_srvProc) { m_srvProc->terminate(); }
        m_srvStopBtn->setEnabled(false);
    });

    /* ── Connessioni Config ── */
    connect(fmtApply, &QPushButton::clicked, this, [this]{
        QString newFmt = m_cmbFmt->currentData().toString();
        QString err    = convertConfig(newFmt);
        if (err.isEmpty())
            m_fmtStatus->setText(
                QString("\xe2\x9c\x85  Config salvato in formato %1")
                .arg(newFmt.toUpper()));
        else
            m_fmtStatus->setText(
                QString("\xe2\x9d\x8c  %1").arg(err));
    });

    /* ── Connessioni Backend ── */
    connect(applyBtn, &QPushButton::clicked, this, [=]{
        AiClient::Backend bk = (m_cmbBackend->currentIndex() == 0)
                                ? AiClient::Ollama : AiClient::LlamaServer;
        m_ai->setBackend(bk, hostEdit->text(), portEdit->text().toInt(), m_ai->model());
        m_ai->fetchModels();
    });

    connect(refreshBtn, &QPushButton::clicked, m_ai, &AiClient::fetchModels);

    connect(m_ai, &AiClient::modelsReady, this, [=](const QStringList& list){
        m_cmbModel->clear();
        if (list.isEmpty()) {
            m_cmbModel->addItem("(nessun modello \xe2\x80\x94 backend non raggiungibile)");
            return;
        }
        for (auto& nm : list) m_cmbModel->addItem(nm);
        int idx = m_cmbModel->findText(m_ai->model());
        if (idx >= 0) m_cmbModel->setCurrentIndex(idx);
    });

    connect(setModelBtn, &QPushButton::clicked, this, [=]{
        QString sel = m_cmbModel->currentText();
        if (!sel.isEmpty() && !sel.startsWith("("))
            m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), sel);
    });

    connect(m_cmbBackend, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [=](int idx){
        /* Aggiorna porta e applica subito il backend — ricalcola i modelli in automatico */
        const int port = (idx == 0) ? P::kOllamaPort : P::kLlamaServerPort;
        portEdit->setText(QString::number(port));
        AiClient::Backend bk = (idx == 0) ? AiClient::Ollama : AiClient::LlamaServer;
        m_cmbModel->clear();
        m_cmbModel->addItem("(\xe2\x8f\xb3  aggiornamento modelli...)");
        m_ai->setBackend(bk, hostEdit->text(), port, "");
        m_ai->fetchModels();
        /* Mostra la sezione llama-server solo quando selezionato */
        grpServ->setVisible(idx == 1);
    });

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

    connect(fmtApply, &QPushButton::clicked, this, [this]{
        QString newFmt = m_cmbFmt->currentData().toString();
        QString err    = convertConfig(newFmt);
        if (err.isEmpty())
            m_fmtStatus->setText(
                QString("\xe2\x9c\x85  Config salvato in formato %1")
                .arg(newFmt.toUpper()));
        else
            m_fmtStatus->setText(
                QString("\xe2\x9d\x8c  %1").arg(err));
    });

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
    mainLay->addWidget(colsRow, 1);

    /* ── Helper: esegui processo nel log ── */
    auto runCmd = [this](const QString& prog, const QStringList& args,
                         const QString& label) {
        m_ramLog->append(QString("\xe2\x96\xb6 %1\n").arg(label));
        auto* proc = new QProcess(this);
        proc->setProcessChannelMode(QProcess::MergedChannels);
        connect(proc, &QProcess::readyRead, this, [this, proc]{
            m_ramLog->moveCursor(QTextCursor::End);
            m_ramLog->insertPlainText(
                QString::fromLocal8Bit(proc->readAll()));
            m_ramLog->ensureCursorVisible();
        });
        connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, proc](int code, QProcess::ExitStatus){
            m_ramLog->append(code == 0 ? "\xe2\x9c\x85 OK\n" :
                             QString("\xe2\x9d\x8c Codice uscita: %1\n").arg(code));
            proc->deleteLater();
        });
        proc->start(prog, args);
        if (!proc->waitForStarted(3000)) {
            m_ramLog->append(
                QString("\xe2\x9d\x8c  Impossibile avviare: %1\n").arg(prog));
            proc->deleteLater();
        }
    };

#ifdef Q_OS_WIN
    connect(detectBtn, &QPushButton::clicked, this, [=]{
        m_ramLog->clear();
        runCmd("powershell",
            {"-NoProfile", "-Command",
             "Get-MMAgent | Select-Object MemoryCompression,"
             "ApplicationLaunchPrefetching,OperationAPI | Format-List"},
            "Stato Memory Compression (Windows)");
    });
    connect(compBtn, &QPushButton::clicked, this, [=]{
        m_ramLog->clear();
        runCmd("powershell",
            {"-NoProfile", "-Command",
             "Enable-MMAgent -MemoryCompression; "
             "Write-Host 'Memory Compression attivata.'"},
            "Attivazione Memory Compression");
    });
    connect(disableBtn, &QPushButton::clicked, this, [=]{
        m_ramLog->clear();
        runCmd("powershell",
            {"-NoProfile", "-Command",
             "Disable-MMAgent -MemoryCompression; "
             "Write-Host 'Memory Compression disattivata.'"},
            "Disattivazione Memory Compression");
    });
#else
    connect(detectBtn, &QPushButton::clicked, this, [=]{
        m_ramLog->clear();
        runCmd("bash", {"-c",
            "echo '=== Swap attivo ==='; "
            "cat /proc/swaps; "
            "echo; echo '=== zRAM dispositivi ==='; "
            "zramctl 2>/dev/null || ls /sys/block/zram* 2>/dev/null || "
            "echo 'nessun device zRAM attivo'; "
            "echo; echo '=== RAM libera ==='; "
            "grep -E 'MemTotal|MemFree|MemAvailable|SwapTotal|SwapFree'"
            " /proc/meminfo"},
            "Rilevamento stato zRAM");
    });

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

    connect(singBtn, &QPushButton::clicked, this, [=]{
        m_ramLog->clear();
        m_ramLog->append("\xe2\x9a\xa0  Richiesta autorizzazione amministratore (pkexec)...\n");
        runCmd("pkexec", {"bash", "-c", SCRIPT_SINGOLA},
               "Attivazione zRAM singolo (lz4, 50% RAM)");
    });

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

    connect(doppiaBtn, &QPushButton::clicked, this, [=]{
        m_ramLog->clear();
        m_ramLog->append("\xe2\x9a\xa0  Richiesta autorizzazione amministratore (pkexec)...\n");
        runCmd("pkexec", {"bash", "-c", SCRIPT_DOPPIA},
               "Attivazione zRAM doppio (zstd, 75% RAM \xe2\x80\x94 algoritmo Meta)");
    });

    static const char* SCRIPT_DISABILITA =
        "for dev in /dev/zram*; do swapoff \"$dev\" 2>/dev/null && "
        "echo \"swapoff $dev OK\"; done; "
        "rmmod zram 2>/dev/null && echo 'modulo zram rimosso' || "
        "echo 'zram non era caricato'; "
        "echo; cat /proc/swaps";

    connect(disableBtn, &QPushButton::clicked, this, [=]{
        m_ramLog->clear();
        m_ramLog->append("\xe2\x9a\xa0  Richiesta autorizzazione amministratore (pkexec)...\n");
        runCmd("pkexec", {"bash", "-c", SCRIPT_DISABILITA},
               "Disattivazione zRAM");
    });
#endif

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
    QString txt;
    for (int i = 0; i < hw.count; i++) {
        const HWDevice& d = hw.dev[i];
        QString tag;
        if (i == hw.primary)   tag = "  \xe2\x86\x90 PRIMARIO";
        if (i == hw.secondary) tag = "  \xe2\x86\x90 secondario";

        if (d.type == DEV_CPU)
            txt += QString("[CPU]  %1\n       RAM %2 MB  (disponibile %3 MB)%4\n\n")
                   .arg(d.name).arg(d.mem_mb).arg(d.avail_mb).arg(tag);
        else
            txt += QString("[%1]  %2\n       VRAM %3 MB  (usabile %4 MB, layers=%5)%6\n\n")
                   .arg(hw_dev_type_str(d.type)).arg(d.name)
                   .arg(d.mem_mb).arg(d.avail_mb).arg(d.n_gpu_layers).arg(tag);
    }
    if (txt.isEmpty()) txt = "Nessun dispositivo rilevato.";
    m_hwLabel->setText(txt.trimmed());
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
