#include "manutenzione_page.h"
#include "../prismalux_paths.h"
namespace P = PrismaluxPaths;
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QPushButton>
#include <QLineEdit>
#include <QGroupBox>
#include <QProcess>
#include <QScrollArea>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>

ManutenzioneePage::ManutenzioneePage(AiClient* ai, HardwareMonitor* hw, QWidget* parent)
    : QWidget(parent), m_ai(ai), m_hw(hw)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(24, 20, 24, 16);
    lay->setSpacing(14);

    auto* title = new QLabel("🔧  Manutenzione", this);
    title->setObjectName("pageTitle");
    auto* sub   = new QLabel("Gestione backend AI, selezione modello, informazioni hardware", this);
    sub->setObjectName("pageSubtitle");
    lay->addWidget(title); lay->addWidget(sub);
    auto* div = new QFrame(this); div->setObjectName("pageDivider");
    div->setFrameShape(QFrame::HLine); lay->addWidget(div);

    /* ── Sezione Backend ── */
    auto* grpBackend = new QGroupBox("🔌  Backend AI", this);
    grpBackend->setStyleSheet(
        "QGroupBox { border:1px solid #1e2040; border-radius:6px; "
        "margin-top:12px; color:#8a8fb0; font-size:12px; }"
        "QGroupBox::title { subcontrol-origin:margin; left:10px; padding:0 4px; }");
    auto* bkLay = new QVBoxLayout(grpBackend);
    bkLay->setSpacing(10);

    /* Backend selector */
    auto* lbBk = new QLabel("Backend:", grpBackend);
    m_cmbBackend = new QComboBox(grpBackend);
    m_cmbBackend->addItem(QString("Ollama (localhost:%1)").arg(P::kOllamaPort));
    m_cmbBackend->addItem(QString("llama-server (localhost:%1)").arg(P::kLlamaServerPort));
    bkLay->addWidget(lbBk);
    bkLay->addWidget(m_cmbBackend);

    /* Host */
    auto* lbHost = new QLabel("Host:", grpBackend);
    auto* hostEdit = new QLineEdit("127.0.0.1", grpBackend);
    hostEdit->setObjectName("chatInput");
    bkLay->addWidget(lbHost);
    bkLay->addWidget(hostEdit);

    /* Porta */
    auto* lbPort = new QLabel("Porta:", grpBackend);
    auto* portEdit = new QLineEdit("11434", grpBackend);
    portEdit->setObjectName("chatInput");
    bkLay->addWidget(lbPort);
    bkLay->addWidget(portEdit);

    /* Applica */
    auto* applyBtn = new QPushButton("Applica \xe2\x96\xb6", grpBackend);
    applyBtn->setObjectName("actionBtn");
    bkLay->addWidget(applyBtn);

    lay->addWidget(grpBackend);

    /* ── Sezione Modelli ── */
    auto* grpModel = new QGroupBox("🦾  Modello AI", this);
    grpModel->setStyleSheet(grpBackend->styleSheet());
    auto* mdLay = new QVBoxLayout(grpModel);
    mdLay->setSpacing(10);

    auto* mdRow = new QWidget(grpModel);
    auto* mdRL  = new QHBoxLayout(mdRow);
    mdRL->setContentsMargins(0,0,0,0); mdRL->setSpacing(10);
    m_cmbModel = new QComboBox(mdRow);
    m_cmbModel->addItem("(nessun modello — backend non raggiungibile)");
    auto* refreshBtn = new QPushButton("🔄  Aggiorna lista", mdRow);
    refreshBtn->setObjectName("actionBtn");
    auto* setModelBtn = new QPushButton("✓  Usa questo", mdRow);
    setModelBtn->setObjectName("actionBtn");
    mdRL->addWidget(m_cmbModel, 1);
    mdRL->addWidget(refreshBtn);
    mdRL->addWidget(setModelBtn);
    mdLay->addWidget(mdRow);

    lay->addWidget(grpModel);

    /* ── Sezione Formato Config ── */
    auto* grpFmt = new QGroupBox("\xf0\x9f\x93\x84  Formato Config", this);
    grpFmt->setStyleSheet(grpBackend->styleSheet());
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

    auto* fmtRow = new QWidget(grpFmt);
    auto* fmtRowL = new QHBoxLayout(fmtRow);
    fmtRowL->setContentsMargins(0,0,0,0); fmtRowL->setSpacing(10);

    fmtRowL->addWidget(new QLabel("Formato:", fmtRow));
    m_cmbFmt = new QComboBox(fmtRow);
    m_cmbFmt->addItem("JSON  (.prismalux_config.json)", QString("json"));
    m_cmbFmt->addItem("TOON  (.prismalux_config.toon)", QString("toon"));

    /* Pre-seleziona il formato attivo */
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

    lay->addWidget(grpFmt);

    /* ── Sezione Hardware ── */
    auto* grpHW = new QGroupBox("🖥️  Info Hardware", this);
    grpHW->setStyleSheet(grpBackend->styleSheet());
    auto* hwLay = new QVBoxLayout(grpHW);

    m_hwLabel = new QLabel("⏳  Rilevamento hardware in corso...", grpHW);
    m_hwLabel->setObjectName("cardDesc");
    m_hwLabel->setWordWrap(true);
    m_hwLabel->setStyleSheet("font-family:'Consolas','Courier New',monospace; "
                              "color:#a0a4c0; padding:4px;");
    hwLay->addWidget(m_hwLabel);

    lay->addWidget(grpHW);

    /* ══════════════════════════════════════════════════════════════
       Sezione Ottimizzazione RAM
       Linux  : zRAM con algoritmo zstd (sviluppato da Meta/Facebook).
                Singola = 1 device (50% RAM, lz4 — veloce).
                Doppia  = compatta + 2 device zstd (75% RAM — massima).
       Windows: Memory Compression integrata (Enable-MMAgent).
       Richiede privilegi di amministratore / pkexec su Linux.
       ══════════════════════════════════════════════════════════════ */
    auto* grpRam = new QGroupBox(
        "\xf0\x9f\x92\xbe  Ottimizzazione RAM", this);
    grpRam->setStyleSheet(grpBackend->styleSheet());
    auto* ramLay = new QVBoxLayout(grpRam);
    ramLay->setSpacing(8);

    /* Riga stato corrente */
    m_ramStatusLbl = new QLabel(
        "Stato: premi \"\xf0\x9f\x94\x8d Rileva\" per controllare lo stato della compressione RAM.",
        grpRam);
    m_ramStatusLbl->setObjectName("cardDesc");
    m_ramStatusLbl->setWordWrap(true);
    ramLay->addWidget(m_ramStatusLbl);

    /* Descrizione algoritmo */
    auto* ramDesc = new QLabel(
#ifdef Q_OS_WIN
        "<b>Windows</b>: Memory Compression \xc3\xa8 integrata nel sistema "
        "(Windows 10/11). Attivandola il kernel comprime automaticamente "
        "le pagine RAM poco usate. Non richiede riavvio.",
#else
        "<b>Linux</b>: zRAM crea un dispositivo swap compresso direttamente in RAM. "
        "Algoritmo <b>zstd</b> (sviluppato da Meta/Facebook — miglior rapporto "
        "velocit\xc3\xa0/compressione). "
        "<i>Singola</i>: 1 device, 50% RAM, lz4 (veloce). "
        "<i>Doppia</i>: compatta la memoria fisica + 2 device zstd (75% RAM, "
        "massima compressione).",
#endif
        grpRam);
    ramDesc->setObjectName("cardDesc");
    ramDesc->setWordWrap(true);
    ramLay->addWidget(ramDesc);

    /* Pulsanti azione */
    auto* btnRow = new QWidget(grpRam);
    auto* btnL   = new QHBoxLayout(btnRow);
    btnL->setContentsMargins(0,0,0,0); btnL->setSpacing(8);

    auto* detectBtn = new QPushButton(
        "\xf0\x9f\x94\x8d  Rileva stato", grpRam);
    detectBtn->setObjectName("actionBtn");
    btnL->addWidget(detectBtn);

#ifdef Q_OS_WIN
    auto* compBtn = new QPushButton(
        "\xf0\x9f\x92\xbe  Attiva compressione", grpRam);
    compBtn->setObjectName("actionBtn");
    auto* disableBtn = new QPushButton(
        "\xe2\x8f\xb9  Disattiva", grpRam);
    disableBtn->setObjectName("actionBtn");
    btnL->addWidget(compBtn);
    btnL->addWidget(disableBtn);
#else
    auto* singBtn = new QPushButton(
        "\xf0\x9f\x92\xbe  Singola (lz4)", grpRam);
    singBtn->setObjectName("actionBtn");
    auto* doppiaBtn = new QPushButton(
        "\xf0\x9f\x92\xbe\xf0\x9f\x92\xbe  Doppia (zstd)", grpRam);
    doppiaBtn->setObjectName("actionBtn");
    auto* disableBtn = new QPushButton(
        "\xe2\x8f\xb9  Disattiva zRAM", grpRam);
    disableBtn->setObjectName("actionBtn");
    btnL->addWidget(singBtn);
    btnL->addWidget(doppiaBtn);
    btnL->addWidget(disableBtn);
#endif
    btnL->addStretch(1);
    ramLay->addWidget(btnRow);

    /* Log operazioni */
    m_ramLog = new QTextEdit(grpRam);
    m_ramLog->setReadOnly(true);
    m_ramLog->setMaximumHeight(110);
    m_ramLog->setStyleSheet(
        "font-family:'Consolas','Courier New',monospace; font-size:10px;");
    m_ramLog->setPlaceholderText("Output comandi...");
    ramLay->addWidget(m_ramLog);

    lay->addWidget(grpRam);
    lay->addStretch(1);

    /* ── Helper: esegui processo e mostra output nel log ── */
    auto runCmd = [this](const QString& prog, const QStringList& args,
                         const QString& label) {
        m_ramLog->append(QString("▶ %1\n").arg(label));
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

    /* ── Connessione: Rileva stato ── */
#ifdef Q_OS_WIN
    connect(detectBtn, &QPushButton::clicked, this, [=]{
        m_ramLog->clear();
        runCmd("powershell",
            {"-NoProfile", "-Command",
             "Get-MMAgent | Select-Object MemoryCompression,"
             "ApplicationLaunchPrefetching,OperationAPI |"
             " Format-List"},
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

    /* ── Script singola (lz4) ── */
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
        m_ramLog->append(
            "\xe2\x9a\xa0  Richiesta autorizzazione amministratore (pkexec)...\n");
        runCmd("pkexec", {"bash", "-c", SCRIPT_SINGOLA},
               "Attivazione zRAM singolo (lz4, 50% RAM)");
    });

    /* ── Script doppia (zstd — algoritmo Meta) ── */
    static const char* SCRIPT_DOPPIA =
        /* 1. Compatta la memoria fisica prima di zRAM */
        "echo 'Step 1: compattazione memoria fisica...'; "
        "echo 1 | tee /proc/sys/vm/compact_memory; "
        "sleep 1; "
        /* 2. Disabilita eventuali zRAM esistenti */
        "for dev in /dev/zram*; do swapoff \"$dev\" 2>/dev/null; done; "
        "rmmod zram 2>/dev/null || true; "
        "sleep 0.3; "
        /* 3. Carica zRAM con 2 device */
        "modprobe zram num_devices=2; "
        "sleep 0.3; "
        "TOTAL=$(grep MemTotal /proc/meminfo | awk '{print $2}'); "
        /* 4. Device 0: zstd, 50% RAM */
        "echo 'Step 2: device 0 (zstd, 50% RAM)...'; "
        "(echo zstd | tee /sys/block/zram0/comp_algorithm) || "
        " (echo lzo-rle | tee /sys/block/zram0/comp_algorithm); "
        "echo $(( TOTAL * 512 )) | tee /sys/block/zram0/disksize; "
        "mkswap /dev/zram0; "
        "swapon -p 100 /dev/zram0; "
        /* 5. Device 1: zstd, 25% RAM */
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
        m_ramLog->append(
            "\xe2\x9a\xa0  Richiesta autorizzazione amministratore (pkexec)...\n");
        runCmd("pkexec", {"bash", "-c", SCRIPT_DOPPIA},
               "Attivazione zRAM doppio (zstd, 75% RAM — algoritmo Meta)");
    });

    /* ── Disabilita ── */
    static const char* SCRIPT_DISABILITA =
        "for dev in /dev/zram*; do swapoff \"$dev\" 2>/dev/null && "
        "echo \"swapoff $dev OK\"; done; "
        "rmmod zram 2>/dev/null && echo 'modulo zram rimosso' || "
        "echo 'zram non era caricato'; "
        "echo; cat /proc/swaps";

    connect(disableBtn, &QPushButton::clicked, this, [=]{
        m_ramLog->clear();
        m_ramLog->append(
            "\xe2\x9a\xa0  Richiesta autorizzazione amministratore (pkexec)...\n");
        runCmd("pkexec", {"bash", "-c", SCRIPT_DISABILITA},
               "Disattivazione zRAM");
    });
#endif

    /* ── Connessioni ── */
    connect(applyBtn, &QPushButton::clicked, this, [=]{
        AiClient::Backend bk = (m_cmbBackend->currentIndex()==0)
                                ? AiClient::Ollama : AiClient::LlamaServer;
        m_ai->setBackend(bk, hostEdit->text(), portEdit->text().toInt(), m_ai->model());
        m_ai->fetchModels();
    });

    connect(refreshBtn, &QPushButton::clicked, m_ai, &AiClient::fetchModels);

    connect(m_ai, &AiClient::modelsReady, this, [=](const QStringList& list){
        m_cmbModel->clear();
        if (list.isEmpty()) {
            m_cmbModel->addItem("(nessun modello — backend non raggiungibile)");
            return;
        }
        for (auto& nm : list) m_cmbModel->addItem(nm);
        /* Pre-seleziona il modello corrente */
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
        portEdit->setText(idx == 0
            ? QString::number(P::kOllamaPort)
            : QString::number(P::kLlamaServerPort));
    });
}

void ManutenzioneePage::onHWReady(const HWInfo& hw) {
    updateHWLabel(hw);
}

/* ══════════════════════════════════════════════════════════════
   helpers formato config
   ══════════════════════════════════════════════════════════════ */

/** Rileva il formato attivo leggendo quale file esiste e, se JSON,
 *  controlla il campo config_fmt dentro di esso. */
QString ManutenzioneePage::detectConfigFmt()
{
    QString home = QDir::homePath();
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
    return "json"; /* default se nessun file esiste ancora */
}

/**
 * convertConfig(newFmt) — legge il config corrente e lo riscrive nel
 * formato richiesto, poi rimuove il vecchio file.
 *
 * Ritorna "" se ok, messaggio di errore altrimenti.
 *
 * La struttura dati è identica in entrambi i formati:
 *   backend, ollama_host, ollama_port, ollama_model,
 *   lserver_host, lserver_port, lserver_model,
 *   llama_model, gui_path, config_fmt
 */
QString ManutenzioneePage::convertConfig(const QString& newFmt)
{
    QString home = QDir::homePath();
    QString pathJson = home + "/.prismalux_config.json";
    QString pathToon = home + "/.prismalux_config.toon";

    /* ── Leggi il config corrente ──────────────────────────────── */
    struct Cfg {
        QString backend      = "ollama";
        QString ollamaHost   = "127.0.0.1";
        int     ollamaPort   = 11434;
        QString ollamaModel;
        QString lserverHost  = "127.0.0.1";
        int     lserverPort  = P::kLlamaServerPort;
        QString lserverModel;
        QString llamaModel;
        QString guiPath      = "Qt_GUI/build/Prismalux_GUI";
    } cfg;

    /* Prova JSON */
    if (QFile::exists(pathJson)) {
        QFile f(pathJson);
        if (!f.open(QIODevice::ReadOnly))
            return QString("Impossibile aprire %1").arg(pathJson);
        QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        f.close();
        if (doc.isNull())
            return "Config JSON non valido";
        QJsonObject o = doc.object();
        cfg.backend      = o.value("backend").toString(cfg.backend);
        cfg.ollamaHost   = o.value("ollama_host").toString(cfg.ollamaHost);
        cfg.ollamaPort   = o.value("ollama_port").toInt(cfg.ollamaPort);
        cfg.ollamaModel  = o.value("ollama_model").toString();
        cfg.lserverHost  = o.value("lserver_host").toString(cfg.lserverHost);
        cfg.lserverPort  = o.value("lserver_port").toInt(cfg.lserverPort);
        cfg.lserverModel = o.value("lserver_model").toString();
        cfg.llamaModel   = o.value("llama_model").toString();
        cfg.guiPath      = o.value("gui_path").toString(cfg.guiPath);
    }
    /* Prova TOON */
    else if (QFile::exists(pathToon)) {
        QFile f(pathToon);
        if (!f.open(QIODevice::ReadOnly))
            return QString("Impossibile aprire %1").arg(pathToon);
        QString buf = QString::fromUtf8(f.readAll());
        f.close();
        /* Parser TOON flat: cerca "key: value\n" */
        auto tget = [&](const QString& key) -> QString {
            /* cerca "\nkey: " oppure inizio buffer */
            QString pat = "\n" + key + ": ";
            int pos = buf.indexOf(pat);
            if (pos < 0) {
                /* fallback: prima riga */
                if (buf.startsWith(key + ": "))
                    pos = -1;  /* vedi sotto */
                else
                    return QString();
            }
            int vstart = (pos < 0) ? (key.length() + 2)
                                   : (pos + pat.length());
            int vend   = buf.indexOf('\n', vstart);
            QString v  = (vend < 0) ? buf.mid(vstart)
                                    : buf.mid(vstart, vend - vstart);
            return v.trimmed();
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
        if (cfg.lserverPort == 0) cfg.lserverPort = P::kLlamaServerPort;
    }
    /* Nessun config esistente: crea nuovo nel formato richiesto con defaults */

    /* ── Scrivi nel nuovo formato ──────────────────────────────── */
    QString destPath = (newFmt == "toon") ? pathToon : pathJson;
    QFile out(destPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return QString("Impossibile scrivere %1").arg(destPath);

    QTextStream ts(&out);

    if (newFmt == "toon") {
        ts << "# Prismalux config — TOON\n\n";
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
        QJsonDocument doc(o);
        ts << doc.toJson(QJsonDocument::Indented);
    }
    out.close();

    /* ── Rimuovi il vecchio file ──────────────────────────────── */
    if (newFmt == "toon" && QFile::exists(pathJson))
        QFile::remove(pathJson);
    else if (newFmt == "json" && QFile::exists(pathToon))
        QFile::remove(pathToon);

    return QString(); /* successo */
}

void ManutenzioneePage::updateHWLabel(const HWInfo& hw) {
    QString txt;
    for (int i = 0; i < hw.count; i++) {
        const HWDevice& d = hw.dev[i];
        QString tag;
        if (i == hw.primary)   tag = "  ← PRIMARIO";
        if (i == hw.secondary) tag = "  ← secondario";

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
