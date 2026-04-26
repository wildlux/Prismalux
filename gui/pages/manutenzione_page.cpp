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
        for (const auto& nm : list) {
            m_cmbModel->addItem(nm);
            if (P::isKnownBrokenModel(nm)) {
                const int i = m_cmbModel->count() - 1;
                m_cmbModel->setItemData(i, QBrush(QColor("#ea580c")), Qt::ForegroundRole);
                m_cmbModel->setItemData(i, QBrush(QColor("#fef08a")), Qt::BackgroundRole);
                m_cmbModel->setItemData(i,
                    P::knownBrokenModelTooltip(),
                    Qt::ToolTipRole);
            }
        }
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
    auto* verLbl = new QLabel("\xf0\x9f\x90\xb3  Ollama: <i>verifica in corso...</i>", verRow);
    verLbl->setObjectName("cardDesc");
    verLbl->setTextFormat(Qt::RichText);
    verLay->addWidget(verLbl, 1);
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
    auto* updAllBtn = new QPushButton(
        "\xe2\xac\x87  Aggiorna tutti i modelli Ollama", btnRow);
    updAllBtn->setObjectName("actionBtn");
    auto* updStatusLbl = new QLabel("", btnRow);
    updStatusLbl->setObjectName("cardDesc");
    updStatusLbl->setWordWrap(true);
    btnRowL->addWidget(updAllBtn);
    btnRowL->addWidget(updStatusLbl, 1);
    updLay->addWidget(btnRow);

    /* ── Log aggiornamento (compatto) ── */
    auto* updLog = new QTextEdit(updGroup);
    updLog->setReadOnly(true);
    updLog->setObjectName("chatLog");
    updLog->setFixedHeight(100);
    updLog->setPlaceholderText("Premi \"Aggiorna tutti\" per scaricare le ultime versioni dei modelli Ollama.");
    updLay->addWidget(updLog);

    mainLay->addWidget(updGroup, 0);

    /* ── Lambda: verifica versione Ollama ── */
    auto checkOllamaVer = [=]() {
        verLbl->setText("\xf0\x9f\x90\xb3  Ollama: <i>verifica...</i>");
        auto* proc = new QProcess(page);
        proc->start("ollama", {"--version"});
        connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                page, [=](int code, QProcess::ExitStatus){
            proc->deleteLater();
            const QString out = QString::fromLocal8Bit(proc->readAllStandardOutput()).trimmed()
                              + QString::fromLocal8Bit(proc->readAllStandardError()).trimmed();
            if (code == 0 && !out.isEmpty()) {
                verLbl->setText(QString("\xf0\x9f\x90\xb3  Ollama: <b>%1</b>")
                    .arg(out.toHtmlEscaped()));
                updLog->append(QString("\xf0\x9f\x90\xb3  %1").arg(out));
            } else {
                verLbl->setText("\xf0\x9f\x90\xb3  Ollama: <span style='color:#ef4444;'>non trovato</span>");
            }
        });
        connect(proc, &QProcess::errorOccurred, page, [=](QProcess::ProcessError){
            proc->deleteLater();
            verLbl->setText("\xf0\x9f\x90\xb3  Ollama: <span style='color:#ef4444;'>non trovato nel PATH</span>");
        });
    };

    connect(verBtn, &QPushButton::clicked, page, checkOllamaVer);

    /* ── Lambda: aggiorna tutti i modelli Ollama uno per uno ── */
    connect(updAllBtn, &QPushButton::clicked, page, [=]() {
        updAllBtn->setEnabled(false);
        updLog->clear();
        updStatusLbl->setText("\xf0\x9f\x94\x84  Recupero lista modelli...");

        /* Step 1: ottieni lista modelli da Ollama */
        auto* listProc = new QProcess(page);
        listProc->start("ollama", {"list"});
        connect(listProc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                page, [=](int, QProcess::ExitStatus) {
            listProc->deleteLater();
            const QString raw = QString::fromLocal8Bit(listProc->readAllStandardOutput());
            QStringList models;
            for (const QString& line : raw.split('\n', Qt::SkipEmptyParts)) {
                if (line.trimmed().startsWith("NAME", Qt::CaseInsensitive)) continue;
                const QString name = line.split(QChar(' '), Qt::SkipEmptyParts).value(0).trimmed();
                if (!name.isEmpty()) models << name;
            }

            if (models.isEmpty()) {
                updStatusLbl->setText("\xe2\x9d\x8c  Nessun modello trovato. Ollama in esecuzione?");
                updAllBtn->setEnabled(true);
                return;
            }

            updLog->append(QString("\xf0\x9f\x93\x8b  %1 modelli da aggiornare: %2")
                .arg(models.size()).arg(models.join(", ")));
            updStatusLbl->setText(QString("\xf0\x9f\x94\x84  Aggiornamento 1/%1...").arg(models.size()));

            /* Step 2: aggiorna ogni modello in sequenza usando un contatore */
            auto* idx = new int(0);
            auto* total = new int(models.size());

            /* Funzione ricorsiva tramite funzione lambda condivisa */
            struct Updater {
                static void next(QWidget* parent, QTextEdit* log, QLabel* status,
                                 QPushButton* btn, QStringList mdls, int* i, int* tot) {
                    if (*i >= *tot) {
                        delete i; delete tot;
                        status->setText(QString("\xe2\x9c\x85  Aggiornamento completato! %1 modelli").arg(*tot));
                        log->append("\n\xe2\x9c\x85  Tutti i modelli sono aggiornati.");
                        btn->setEnabled(true);
                        return;
                    }
                    const QString mdl = mdls.at(*i);
                    status->setText(QString("\xf0\x9f\x94\x84  Aggiornamento %1/%2: %3")
                        .arg(*i + 1).arg(*tot).arg(mdl));
                    log->append(QString("\n\xe2\xac\x87  Aggiornamento: <b>%1</b>...").arg(mdl));

                    auto* proc = new QProcess(parent);
                    proc->setProcessChannelMode(QProcess::MergedChannels);
                    proc->start("ollama", {"pull", mdl});
                    QObject::connect(proc, &QProcess::readyRead, parent, [proc, log](){
                        log->moveCursor(QTextCursor::End);
                        const QString chunk = QString::fromLocal8Bit(proc->readAll());
                        /* Mostra solo righe non vuote per non riempire il log */
                        for (const QString& l : chunk.split('\n', Qt::SkipEmptyParts))
                            log->insertPlainText("  " + l.trimmed() + "\n");
                        log->ensureCursorVisible();
                    });
                    QObject::connect(proc,
                        QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                        parent, [proc, log, status, btn, mdls, i, tot, parent](int code, QProcess::ExitStatus){
                            proc->deleteLater();
                            if (code == 0)
                                log->append(QString("  \xe2\x9c\x85  %1 aggiornato.").arg(mdls.at(*i)));
                            else
                                log->append(QString("  \xe2\x9a\xa0  %1: errore (code %2)").arg(mdls.at(*i)).arg(code));
                            ++(*i);
                            next(parent, log, status, btn, mdls, i, tot);
                        });
                }
            };
            Updater::next(page, updLog, updStatusLbl, updAllBtn, models, idx, total);
        });
        connect(listProc, &QProcess::errorOccurred, page, [=](QProcess::ProcessError){
            listProc->deleteLater();
            updStatusLbl->setText("\xe2\x9d\x8c  Ollama non trovato. Verifica il PATH.");
            updAllBtn->setEnabled(true);
        });
    });

    /* Verifica versione Ollama subito all'apertura */
    QTimer::singleShot(200, page, checkOllamaVer);

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

    /* Connessioni: bottoni → selezione (anteprima); Salva → applica+persiste */
    connect(m_btnGpu,    &QPushButton::clicked, this, [this]{ selectComputeMode("gpu");    });
    connect(m_btnCpu,    &QPushButton::clicked, this, [this]{ selectComputeMode("cpu");    });
    connect(m_btnMisto,  &QPushButton::clicked, this, [this]{ selectComputeMode("misto");  });
    connect(m_btnDoppia, &QPushButton::clicked, this, [this]{ selectComputeMode("doppia"); });
    connect(m_btnSaveMode, &QPushButton::clicked, this, [this]{
        applyComputeMode(m_selectedMode);
    });

    /* Ri-applica al cambio modello per ricalcolare num_gpu con i layer reali */
    connect(m_ai, &AiClient::modelChanged, this, [this](const QString&) {
        QSettings s("Prismalux", "GUI");
        const QString saved = s.value(P::SK::kComputeMode, "").toString();
        if (saved == "gpu" || saved == "misto" || saved == "doppia")
            applyComputeMode(saved);
    });

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

        m_btnGpu->setEnabled(hasDedicated);
        m_btnMisto->setEnabled(hasDedicated);
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
