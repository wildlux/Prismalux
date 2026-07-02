/* ══════════════════════════════════════════════════════════════
   mainwindow_backend.cpp — MainWindow: backend llama-server/ds4-server
   ============================================================================
   Dialog di avvio server, rilevamento hardware GPU/CPU, catalogo modelli
   matematici + download da Hugging Face, avvio/arresto processo server,
   commutazione backend (Ollama/llama.cpp/DwarfStar).
   Split da mainwindow.cpp (TODO D-8).
   ══════════════════════════════════════════════════════════════ */
#include "mainwindow.h"
#include "prismalux_paths.h"
#include "dpi_utils.h"

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QFileDialog>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QSettings>
#include <QStatusBar>
#include <QMessageBox>
#include <QRadioButton>
#include <QGroupBox>
#include <QButtonGroup>
#include <QVector>
#include <QPair>

namespace P = PrismaluxPaths;

/* ── Forward declaration helpers math (definiti più in basso) ── */
static bool isMathModel(const QString& filename);
static void showMathDownloadDialog(QWidget* parent, const QString& modelsDir);

/* ══════════════════════════════════════════════════════════════
   showServerDialog — dialog avvio llama-server
   Estratto dal lambda di m_btnServer per essere richiamato
   anche dal menu contestuale di m_btnBackend.
   ══════════════════════════════════════════════════════════════ */
void MainWindow::showServerDialog()
{
    const QStringList modelPaths = P::scanGgufFiles();

    auto* dlg = new QDialog(this);
    dlg->setWindowTitle("\xf0\x9f\xa6\x99\xe2\x9a\xa1\xef\xb8\x8f  Avvia llama-server");
    dlg->setFixedWidth(dpiScale(460));
    auto* lay = new QVBoxLayout(dlg);
    lay->setSpacing(10);

    lay->addWidget(new QLabel(
        "<b>Seleziona modello</b> \xe2\x80\x94 il server parte in background,<br>"
        "il backend viene commutato automaticamente.", dlg));

    lay->addWidget(buildServerHwBanner(dlg));

    /* Categorizza i modelli in matematici e generici */
    QStringList mathPaths, otherPaths;
    for (const QString& p : modelPaths) {
        if (isMathModel(QFileInfo(p).fileName())) mathPaths << p;
        else                                        otherPaths << p;
    }

    QComboBox* cmbModel = nullptr;
    QSpinBox*  spPort   = nullptr;
    lay->addWidget(buildServerModelSection(dlg, &cmbModel, mathPaths, otherPaths));
    /* Recupera spPort dal widget costruito sopra */
    spPort = dlg->findChild<QSpinBox*>();

    lay->addWidget(buildServerMathSection(dlg, cmbModel, mathPaths));

    lay->addWidget(new QLabel(
        "<span style='color:#5a5f80;font-size:11px;'>"
        "Binario cercato in: ENGINE_LLM/llama_cpp_studio/llama.cpp/build/bin/llama-server<br>"
        "Usa <i>avvia_qt.sh</i> dalla cartella Prismalux se il server non parte.</span>",
        dlg));

    auto* bb = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    bb->button(QDialogButtonBox::Ok)->setText("\xe2\x96\xb6  Avvia");
    bb->button(QDialogButtonBox::Ok)->setEnabled(!modelPaths.isEmpty());
    lay->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, dlg, &QDialog::reject);

    if (dlg->exec() == QDialog::Accepted && !modelPaths.isEmpty()) {
        auto* chk = dlg->findChild<QCheckBox*>();
        const QString modelPath = cmbModel ? cmbModel->currentData().toString() : QString();
        startLlamaServer(modelPath,
                         spPort ? spPort->value() : P::kLlamaServerPort,
                         chk    ? chk->isChecked() : false);
    }
    dlg->deleteLater();
}

/* ══════════════════════════════════════════════════════════════
   showDwarfStarDialog — Dialog avvio ds4-server (ENGINE_LLM/swarfstar/)
   ══════════════════════════════════════════════════════════════ */
void MainWindow::showDwarfStarDialog()
{
    const QString bin = P::ds4ServerBin();
    const bool binExists = QFileInfo::exists(bin);

    auto* dlg = new QDialog(this);
    dlg->setWindowTitle("\xe2\xad\x90  Avvia DwarfStar (ds4-server)");
    dlg->setFixedWidth(dpiScale(480));
    auto* lay = new QVBoxLayout(dlg);
    lay->setSpacing(10);

    lay->addWidget(new QLabel(
        "<b>ds4-server</b> \xe2\x80\x94 motore inferenza OpenAI-compatibile per DeepSeek V4 Flash.<br>"
        "Binario: <code>" + bin.toHtmlEscaped() + "</code>", dlg));

    if (!binExists) {
        auto* warn = new QLabel(
            "<span style='color:#ef4444;'>"
            "\xe2\x9d\x8c  Binario non trovato. Compila con:<br>"
            "<code>cd ENGINE_LLM/swarfstar &amp;&amp; make cpu</code></span>", dlg);
        warn->setWordWrap(true);
        lay->addWidget(warn);
    }

    /* Modello .gguf */
    auto* rowMdl = new QHBoxLayout;
    rowMdl->addWidget(new QLabel("Modello (.gguf):", dlg));
    auto* edModel = new QLineEdit(dlg);
    edModel->setPlaceholderText("(opzionale — lascia vuoto se non richiesto)");
    rowMdl->addWidget(edModel, 1);
    auto* btnBrowse = new QPushButton("\xe2\x80\xa6", dlg);
    btnBrowse->setFixedWidth(dpiScale(30));
    connect(btnBrowse, &QPushButton::clicked, dlg, [edModel, dlg]() {
        const QString f = QFileDialog::getOpenFileName(
            dlg, "Seleziona modello .gguf",
            QDir::homePath(),
            "Modelli GGUF (*.gguf);;Tutti i file (*.*)");
        if (!f.isEmpty()) edModel->setText(f);
    });
    rowMdl->addWidget(btnBrowse);
    lay->addLayout(rowMdl);

    /* Porta */
    auto* rowPort = new QHBoxLayout;
    rowPort->addWidget(new QLabel("Porta:", dlg));
    auto* spPort = new QSpinBox(dlg);
    spPort->setRange(1024, 65535);
    spPort->setValue(P::kDwarfStarPort);
    rowPort->addWidget(spPort);
    rowPort->addStretch();
    lay->addLayout(rowPort);

    auto* bb = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    bb->button(QDialogButtonBox::Ok)->setText("\xe2\x96\xb6  Avvia");
    bb->button(QDialogButtonBox::Ok)->setEnabled(binExists);
    lay->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, dlg, &QDialog::reject);

    if (dlg->exec() == QDialog::Accepted && binExists)
        startDs4Server(edModel->text().trimmed(), spPort->value());
    dlg->deleteLater();
}

/* ══════════════════════════════════════════════════════════════
   startDs4Server — Avvia ds4-server come processo figlio.
   Riusa m_serverProc + health polling di llama-server.
   Dopo /health risponde → applyBackend(Ds4Server).
   ══════════════════════════════════════════════════════════════ */
void MainWindow::startDs4Server(const QString& modelPath, int port)
{
    const QString bin = P::ds4ServerBin();
    if (!QFileInfo::exists(bin)) {
        statusBar()->showMessage(
            "\xe2\x9d\x8c  ds4-server non trovato. Compila con: cd ENGINE_LLM/swarfstar && make cpu");
        return;
    }

    if (m_serverProc && m_serverProc->state() != QProcess::NotRunning) {
        m_serverProc->terminate();
        m_serverProc->waitForFinished(3000);
        m_serverProc->deleteLater();
        m_serverProc = nullptr;
    }

    m_serverPort  = port;
    m_serverModel = modelPath.isEmpty() ? "(built-in)" : QFileInfo(modelPath).fileName();
    m_serverIsDs4 = true;

    m_serverProc = new QProcess(this);
    m_serverProc->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_serverProc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MainWindow::onServerProcFinished);
    connect(m_serverProc, &QProcess::errorOccurred,
            this, &MainWindow::onServerProcessError);

    QStringList args = {"--port", QString::number(port), "--host", "127.0.0.1"};
    if (!modelPath.isEmpty()) args << "--model" << modelPath;

    statusBar()->showMessage(
        QString("\xe2\x8f\xb3  Avvio ds4-server sulla porta %1...").arg(port));
    appendLog(
        QString("\xe2\xad\x90 Avvio <b>ds4-server</b> sulla porta <b>%1</b>...").arg(port));

    m_serverProc->start(bin, args);

    /* Health polling: stessa logica di llama-server */
    m_healthTicks = 0;
    m_healthNam   = new QNetworkAccessManager(this);
    m_healthTimer = new QTimer(this);
    m_healthTimer->setInterval(1000);
    connect(m_healthTimer, &QTimer::timeout, this, &MainWindow::onHealthTick);
    m_healthTimer->start();
    if (m_btnBackend) {
        m_btnBackend->setText("\xe2\x8f\xb3  ds4 avvio...");
        P::repolish(m_btnBackend);
    }
}

/* ── Livello 2: banner GPU/CPU rilevato ──────────────────────────── */
QWidget* MainWindow::buildServerHwBanner(QWidget* parent)
{
    QString hwLine;
    if (m_hw && m_hw->hwReady()) {
        const HWInfo& hw = m_hw->hwInfo();
        int bestGpu = -1;
        for (int i = 0; i < hw.count; i++) {
            if (hw.dev[i].type != DEV_CPU) {
                if (bestGpu < 0 || hw.dev[i].avail_mb > hw.dev[bestGpu].avail_mb)
                    bestGpu = i;
            }
        }
        if (bestGpu >= 0) {
            const HWDevice& g = hw.dev[bestGpu];
            int ngl = (g.n_gpu_layers > 0) ? g.n_gpu_layers : 99;
            hwLine = QString(
                "<span style='color:#16a34a;'>"
                "\xf0\x9f\x8e\xae  <b>GPU rilevata:</b> %1 &mdash; %2 MB VRAM liberi "
                "&rarr; <b>-ngl %3</b> (accelerazione GPU attiva)</span>")
                .arg(QString::fromUtf8(g.name)).arg(g.avail_mb).arg(ngl);
        } else {
            const HWDevice& c = hw.dev[hw.primary];
            hwLine = QString(
                "<span style='color:#b45309;'>"
                "\xf0\x9f\x96\xa5  <b>CPU:</b> %1 &mdash; nessuna GPU rilevata "
                "&rarr; <b>-ngl 0</b> (inferenza RAM)</span>")
                .arg(QString::fromUtf8(c.name));
        }
    } else {
        hwLine = "<span style='color:#6b7280;'>\xe2\x8f\xb3  Rilevamento hardware in corso...</span>";
    }
    auto* lbl = new QLabel(hwLine, parent);
    lbl->setWordWrap(true);
    return lbl;
}

/* ── Livello 2: combo modello + porta ────────────────────────────── */
QWidget* MainWindow::buildServerModelSection(QWidget* parent,
                                              QComboBox** outCombo,
                                              const QStringList& mathPaths,
                                              const QStringList& otherPaths)
{
    auto* container = new QWidget(parent);
    auto* vlay = new QVBoxLayout(container);
    vlay->setContentsMargins(0, 0, 0, 0);
    vlay->setSpacing(6);

    auto* cmbModel = new QComboBox(container);
    if (mathPaths.isEmpty() && otherPaths.isEmpty()) {
        cmbModel->addItem("(nessun .gguf trovato in models/)");
        cmbModel->setEnabled(false);
    } else {
        for (const QString& p : mathPaths)
            cmbModel->addItem("\xf0\x9f\x93\x90 " + QFileInfo(p).fileName(), p);
        for (const QString& p : otherPaths)
            cmbModel->addItem(QFileInfo(p).fileName(), p);
    }
    vlay->addWidget(cmbModel);

    auto* rowPort = new QHBoxLayout;
    rowPort->addWidget(new QLabel("Porta:", container));
    auto* spPort = new QSpinBox(container);
    spPort->setRange(1024, 65535);
    spPort->setValue(P::kLlamaServerPort);
    spPort->setFixedWidth(dpiScale(90));
    rowPort->addWidget(spPort);
    rowPort->addStretch();
    vlay->addLayout(rowPort);

    if (outCombo) *outCombo = cmbModel;
    return container;
}

/* ── Livello 2: checkbox profilo math + status + download ────────── */
QWidget* MainWindow::buildServerMathSection(QWidget* parent,
                                             QComboBox* cmbModel,
                                             const QStringList& mathPaths)
{
    auto* container = new QWidget(parent);
    auto* vlay = new QVBoxLayout(container);
    vlay->setContentsMargins(0, 0, 0, 0);
    vlay->setSpacing(4);

    auto* chkMath = new QCheckBox("\xf0\x9f\x93\x90  Profilo matematico (Xeon 64 GB)", container);
    chkMath->setToolTip(
        "Abilita flag ottimali per calcolo scientifico:\n"
        "  --ctx-size 8192  (dimostrazioni lunghe)\n"
        "  --no-mmap        (Q4_K_M: carica tutto in RAM, pi\xc3\xb9 veloce)\n"
        "  mmap attivo      (Q8_0: legge dal SSD on-demand, auto)");
    vlay->addWidget(chkMath);

    auto* lblMathStatus = new QLabel(container);
    lblMathStatus->setWordWrap(true);
    vlay->addWidget(lblMathStatus);

    auto* btnMathDl = new QPushButton(
        "\xe2\xac\x87  Scarica modello matematico da Hugging Face", container);
    btnMathDl->setObjectName("actionBtn");
    btnMathDl->setVisible(false);
    vlay->addWidget(btnMathDl);

    /* updateMathUI — aggiorna status e visibilità del download button */
    auto updateMathUI = [=](bool mathOn) {
        if (!mathOn) { lblMathStatus->hide(); btnMathDl->hide(); return; }
        if (mathPaths.isEmpty()) {
            lblMathStatus->setText(
                "<span style='color:#ff5252;'>"
                "\xe2\x9a\xa0  Nessun modello matematico trovato in models/.<br>"
                "Scarica Qwen2.5-Math-72B (Q4_K_M ~40 GB) o Qwen2.5-Math-7B (Q4_K_M ~4.7 GB).</span>");
            lblMathStatus->show(); btnMathDl->show();
        } else {
            lblMathStatus->setText(
                QString("<span style='color:#69f0ae;'>"
                        "\xe2\x9c\x85  %1 modello/i matematico/i trovato/i (in cima con \xf0\x9f\x93\x90)."
                        "</span>").arg(mathPaths.size()));
            lblMathStatus->show(); btnMathDl->hide();
            if (cmbModel) cmbModel->setCurrentIndex(0);
        }
    };

    connect(chkMath, &QCheckBox::toggled, container, updateMathUI);
    if (cmbModel) {
        connect(cmbModel, QOverload<int>::of(&QComboBox::currentIndexChanged),
                chkMath, [chkMath, cmbModel](int) {
            const QString name = cmbModel->currentText().toLower();
            chkMath->setChecked(name.contains("72b") || name.contains("70b") || name.contains("math"));
        });
    }
    connect(btnMathDl, &QPushButton::clicked, this, &MainWindow::onMathDlBtnClicked);

    /* Stato iniziale */
    const QString initName = cmbModel ? cmbModel->currentText().toLower() : QString();
    const bool initMath = !mathPaths.isEmpty() || initName.contains("math") || initName.contains("72b");
    chkMath->setChecked(initMath);
    updateMathUI(initMath);

    return container;
}

/* ══════════════════════════════════════════════════════════════
   applyBackend — cambia backend, aggiorna label + pulsante
   ══════════════════════════════════════════════════════════════ */
void MainWindow::applyBackend(AiClient::Backend b, const QString& host, int port) {
    m_ai->setBackend(b, host, port, "");
    if (m_btnBackend) m_btnBackend->setStyleSheet("color:#f59e0b;");
    m_ai->fetchModels();

    refreshBackendBtn();

    QString bkName, bkIcon;
    if (b == AiClient::Ds4Server) {
        bkName = "DwarfStar";
        bkIcon = "\xe2\xad\x90  DwarfStar";
    } else if (b == AiClient::Ollama) {
        bkName = "Ollama";
        bkIcon = "\xf0\x9f\xa6\x99  Ollama";
    } else {
        bkName = "llama.cpp";
        bkIcon = "\xf0\x9f\xa6\x99  llama.cpp";
    }
    m_lblBackend->setText(bkIcon + "  \xe2\x86\x92  " + host + ":" + QString::number(port));
    /* Lo stato viene mostrato nel testo di m_btnBackend — nessun widget extra. */
    m_lblModel->setText("(caricamento modelli...)");

    appendLog(QString("\xf0\x9f\x94\x84 Backend: <b>%1</b> @ %2:%3 — recupero modelli...")
              .arg(bkName, host, QString::number(port)));

    statusBar()->showMessage(
        QString("🔄  Backend cambiato: %1 @ %2:%3 — recupero modelli...")
        .arg(bkName, host, QString::number(port)));

    /* Quando arrivano i modelli, seleziona il primo e aggiorna status (connessione unica) */
    m_pendingBkName = bkName;
    connect(m_ai, &AiClient::modelsReady, this,
            &MainWindow::onApplyBackendModelsReady, Qt::SingleShotConnection);
}

/* ══════════════════════════════════════════════════════════════
   Helper — rilevamento modelli matematici
   ══════════════════════════════════════════════════════════════ */
static bool isMathModel(const QString& filename) {
    const QString n = filename.toLower();
    return n.contains("math") || n.contains("numina") ||
           n.contains("mathstral") || n.contains("minerva") ||
           n.contains("deepseek-math");
}

/* Struttura per la lista curata HF di modelli matematici */
struct MathModelEntry {
    QString name;
    QString description;
    QString urlQ4, fileQ4, sizeQ4, flagsQ4;
    QString urlQ8, fileQ8, sizeQ8, flagsQ8;
};

static QVector<MathModelEntry> mathModelCatalog() {
    return {
        {
            "Qwen2.5-Math-72B-Instruct",
            "Matematica universitaria/ricerca — ottimale per Xeon 64 GB",
            "https://huggingface.co/bartowski/Qwen2.5-Math-72B-Instruct-GGUF/resolve/main/Qwen2.5-Math-72B-Instruct-Q4_K_M.gguf",
            "Qwen2.5-Math-72B-Instruct-Q4_K_M.gguf", "~40 GB",
            "--no-mmap --ctx-size 8192 --threads 24",
            "https://huggingface.co/bartowski/Qwen2.5-Math-72B-Instruct-GGUF/resolve/main/Qwen2.5-Math-72B-Instruct-Q8_0.gguf",
            "Qwen2.5-Math-72B-Instruct-Q8_0.gguf", "~74 GB",
            "--ctx-size 8192 --threads 24"
        },
        {
            "Qwen2.5-Math-7B-Instruct",
            "Matematica avanzata — leggero, per test o RAM < 16 GB",
            "https://huggingface.co/bartowski/Qwen2.5-Math-7B-Instruct-GGUF/resolve/main/Qwen2.5-Math-7B-Instruct-Q4_K_M.gguf",
            "Qwen2.5-Math-7B-Instruct-Q4_K_M.gguf", "~4.7 GB",
            "--no-mmap --ctx-size 4096",
            "https://huggingface.co/bartowski/Qwen2.5-Math-7B-Instruct-GGUF/resolve/main/Qwen2.5-Math-7B-Instruct-Q8_0.gguf",
            "Qwen2.5-Math-7B-Instruct-Q8_0.gguf", "~7.7 GB",
            "--ctx-size 4096"
        },
    };
}

/* Dialog download modelli matematici da Hugging Face */
static void showMathDownloadDialog(QWidget* parent, const QString& modelsDir) {
    const auto catalog = mathModelCatalog();

    auto* dlg = new QDialog(parent);
    dlg->setWindowTitle("📐  Scarica modello matematico da Hugging Face");
    dlg->setMinimumWidth(dpiScale(620));
    auto* lay = new QVBoxLayout(dlg);
    lay->setSpacing(12);

    lay->addWidget(new QLabel(
        "<b>Seleziona modello e variante di quantizzazione:</b>", dlg));

    /* Una riga per ogni modello con radio Q4 / Q8 */
    QVector<QPair<QRadioButton*, QRadioButton*>> rows;
    auto* grp = new QButtonGroup(dlg);

    for (const auto& m : catalog) {
        auto* box = new QGroupBox(m.name, dlg);
        box->setToolTip(m.description);
        auto* bl = new QVBoxLayout(box);
        bl->addWidget(new QLabel(
            "<span style='color:#5a5f80;font-size:11px;'>" + m.description + "</span>", box));

        auto* rq4 = new QRadioButton(
            QString("Q4_K_M  %1  — carica tutto in RAM%2")
            .arg(m.sizeQ4)
            .arg(m.name.contains("72B") ? "  ✅ consigliato per Xeon 64 GB" : ""), box);
        auto* rq8 = new QRadioButton(
            QString("Q8_0    %1  — qualità massima, richiede NVMe SSD%2")
            .arg(m.sizeQ8)
            .arg(m.name.contains("72B") ? " (mmap automatico)" : ""), box);

        grp->addButton(rq4);
        grp->addButton(rq8);
        bl->addWidget(rq4);
        bl->addWidget(rq8);
        lay->addWidget(box);
        rows.append(qMakePair(rq4, rq8));
    }

    /* Seleziona Q4_K_M del 72B come default */
    if (!rows.isEmpty()) rows[0].first->setChecked(true);

    auto* bb = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    bb->button(QDialogButtonBox::Ok)->setText("⬇  Scarica");
    lay->addWidget(bb);
    QObject::connect(bb, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
    QObject::connect(bb, &QDialogButtonBox::rejected, dlg, &QDialog::reject);

    if (dlg->exec() != QDialog::Accepted) { dlg->deleteLater(); return; }

    /* Trova la selezione */
    QString url, fname, flags;
    for (int i = 0; i < rows.size() && i < catalog.size(); ++i) {
        if (rows[i].first->isChecked()) {
            url = catalog[i].urlQ4; fname = catalog[i].fileQ4; flags = catalog[i].flagsQ4;
        } else if (rows[i].second->isChecked()) {
            url = catalog[i].urlQ8; fname = catalog[i].fileQ8; flags = catalog[i].flagsQ8;
        }
    }

    dlg->deleteLater();
    if (url.isEmpty()) return;

    QString dest = modelsDir + "/" + fname;

    /* Avvia wget in background — args separati: immune a injection da caratteri
       speciali in url/dest, e startDetached è già asincrono (non serve &). */
    QProcess::startDetached("wget", {"-c", "--progress=bar:force", url, "-O", dest});

    QMessageBox info(parent);
    info.setWindowTitle("Download avviato");
    info.setIcon(QMessageBox::Information);
    info.setText(QString("<b>Download avviato in background:</b><br><code>%1</code>").arg(fname));
    info.setInformativeText(
        QString("Destinazione: <code>%1</code><br><br>"
                "Flag consigliati dopo il download:<br>"
                "<code>llama-server -m %2 --port 8081 --host 127.0.0.1 %3</code>")
        .arg(dest, dest, flags));
    info.exec();
}

/* ══════════════════════════════════════════════════════════════
   startLlamaServer — avvia llama-server in background poi
   commuta il backend automaticamente quando il /health risponde
   ══════════════════════════════════════════════════════════════ */
void MainWindow::startLlamaServer(const QString& modelPath, int port, bool mathProfile) {
    /* Percorso binario — rilevato dinamicamente da PrismaluxPaths */
    const QString bin = P::llamaServerBin();

    if (!QFileInfo::exists(bin)) {
        statusBar()->showMessage(
            "❌  llama-server non trovato. Compilalo in Impostazioni → llama.cpp Studio → Compila.");
        return;
    }

    m_serverPort  = port;
    m_serverModel = QFileInfo(modelPath).fileName();
    m_serverIsDs4 = false;

    /* Variabile d'ambiente per le librerie condivise (.so nella stessa dir del binario) */
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("LD_LIBRARY_PATH",
        P::llamaLibDir() + ":" + env.value("LD_LIBRARY_PATH"));

    m_serverProc = new QProcess(this);
    m_serverProc->setProcessEnvironment(env);
    m_serverProc->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_serverProc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MainWindow::onServerProcFinished);

    /* ── Rilevamento GPU/CPU per n_gpu_layers ─────────────────────────────
     * Logica:
     *   • GPU NVIDIA/AMD/Intel con VRAM disponibile ≥ 2 GB → -ngl <valore ottimale>
     *     (usa hw_gpu_layers() già calcolato da hw_detect)
     *   • CPU-only (es. Xeon) o GPU con VRAM < 2 GB → -ngl 0 (tutto in RAM)
     * Il valore n_gpu_layers è pre-calcolato da hw_detect.c tenendo conto
     * dell'80% della VRAM disponibile al netto del sistema operativo.
     * ───────────────────────────────────────────────────────────────────── */
    int ngl = 0;
    QString hwDesc = "CPU (RAM)";
    if (m_hw && m_hw->hwReady()) {
        const HWInfo& hw = m_hw->hwInfo();
        /* Cerca la GPU con più VRAM disponibile tra tutti i device.
         * La GPU è SEMPRE preferita alla CPU per l'inferenza, anche se
         * hw.primary punta alla CPU (es. Xeon con 64 GB batte GPU con 8 GB
         * per memoria totale, ma non per velocità di inferenza). */
        int bestGpu = -1;
        for (int i = 0; i < hw.count; i++) {
            if (hw.dev[i].type != DEV_CPU) {
                if (bestGpu < 0 || hw.dev[i].avail_mb > hw.dev[bestGpu].avail_mb)
                    bestGpu = i;
            }
        }
        if (bestGpu >= 0) {
            const HWDevice& g = hw.dev[bestGpu];
            ngl = (g.n_gpu_layers > 0) ? g.n_gpu_layers : 99;
            hwDesc = QString("%1 GPU: %2 — %3 MB VRAM liberi → -ngl %4")
                     .arg(hw_dev_type_str(g.type))
                     .arg(QString::fromUtf8(g.name))
                     .arg(g.avail_mb)
                     .arg(ngl);
        } else {
            const HWDevice& c = hw.dev[hw.primary];
            hwDesc = QString("CPU: %1 → -ngl 0 (RAM)").arg(QString::fromUtf8(c.name));
        }
    }

    /* Determina se il modello è Q4 (per --no-mmap) o Q8/più grande */
    bool isQ4 = QFileInfo(modelPath).fileName().contains("Q4", Qt::CaseInsensitive);

    QStringList args = {
        "-m", modelPath,
        "--port", QString::number(port),
        "--host", "127.0.0.1",
        "--log-disable",
        "-ngl", QString::number(ngl),
        /* Flash Attention: riduce RAM/VRAM KV cache ~30-50% senza perdita di qualità.
           Nuove versioni richiedono valore esplicito: --flash-attn auto|on|off */
        "--flash-attn", "auto",
        /* KV cache quantizzata q8_0: dimezza la RAM usata dalla KV cache
           rispetto al default f16, con minima perdita di qualità su testi lunghi. */
        "--cache-type-k", "q8_0",
        "--cache-type-v", "q8_0",
        /* SWA full: forza cache a dimensione piena per i layer sliding-window
           (Qwen3, Gemma3, Mistral). Evita perdita di contesto oltre la finestra SWA.
           Ignorato dai modelli senza sliding-window attention. */
        "--swa-full"
    };
    if (mathProfile) {
        args << "--ctx-size" << "8192";
        /* Q4_K_M (~40 GB): carica tutto in RAM — più veloce senza mmap */
        if (isQ4 && ngl == 0) args << "--no-mmap";
        /* Q8_0 (~74 GB): mmap attivo per default (llama.cpp legge dal SSD on-demand) */
    }

    {
        QSettings s("Prismalux", "GUI");
        if (s.value(P::SK::kMlockModel, false).toBool())
            args << "--mlock";

        if (s.value(P::SK::kRpcEnabled, false).toBool()) {
            const QString nodes = s.value(P::SK::kRpcNodes, "").toString().trimmed();
            if (!nodes.isEmpty())
                args << "--rpc" << nodes;
        }
    }

    {
        QSettings s2("Prismalux", "GUI");
        const bool rpcOn   = s2.value(P::SK::kRpcEnabled, false).toBool();
        const QString rpcNodes = s2.value(P::SK::kRpcNodes, "").toString().trimmed();
        const QString rpcDesc  = (rpcOn && !rpcNodes.isEmpty())
            ? QString(" | RPC: %1").arg(rpcNodes) : "";
        statusBar()->showMessage(
            QString("\xe2\x8f\xb3  Avvio llama-server — %1%2%3 — porta %4")
            .arg(hwDesc)
            .arg(mathProfile ? " | profilo matematico" : "")
            .arg(rpcDesc)
            .arg(port));
    }

    m_serverProc->start(bin, args);

    /*
     * Usiamo il segnale errorOccurred invece di waitForStarted() bloccante:
     * waitForStarted congela il thread UI per fino a 4s.
     * errorOccurred viene emesso immediatamente se il processo non parte.
     */
    connect(m_serverProc, &QProcess::errorOccurred,
            this, &MainWindow::onServerProcessError);

    appendLog(QString("\xf0\x9f\x9f\xa1 Avvio <b>llama-server</b> su porta %1...").arg(port));
    statusBar()->showMessage(
        QString("⏳  llama-server avviato — attendo che sia pronto (porta %1)...").arg(port));

    /* Mostra stato caricamento direttamente nel pulsante backend */
    if (m_btnBackend) m_btnBackend->setText("\xe2\x8f\xb3  Caricamento...");

    /*
     * Polling /health ogni 1s, max 180 tentativi (3 minuti).
     * Usa m_healthTimer/m_healthNam/m_healthTicks come membri per evitare lambda.
     */
    if (m_healthTimer) { m_healthTimer->stop(); m_healthTimer->deleteLater(); m_healthTimer = nullptr; }
    if (m_healthNam)   { m_healthNam->deleteLater(); m_healthNam = nullptr; }
    m_healthTicks = 0;
    m_healthTimer = new QTimer(this);
    m_healthNam   = new QNetworkAccessManager(this);
    connect(m_healthTimer, &QTimer::timeout, this, &MainWindow::onHealthTick);
    m_healthTimer->start(1000);
}

/* ── Ferma llama-server avviato dalla GUI ── */
void MainWindow::stopLlamaServer() {
    if (!m_serverProc) return;
    m_serverProc->terminate();
    statusBar()->showMessage("🛑  Arresto llama-server in corso...");
}

/* ── Aggiorna testo e colore del pulsante backend ── */
void MainWindow::refreshBackendBtn() {
    if (!m_btnBackend) return;
    switch (m_ai->backend()) {
    case AiClient::Ds4Server:
        m_btnBackend->setText("\xe2\xad\x90  DwarfStar");
        m_btnBackend->setProperty("backendActive", "llama");
        break;
    case AiClient::Ollama:
        m_btnBackend->setText("\xf0\x9f\xa6\x99  Ollama");
        m_btnBackend->setProperty("backendActive", "ollama");
        break;
    default:
        m_btnBackend->setText("\xf0\x9f\xa6\x99\xe2\x9a\xa1\xef\xb8\x8f  llama-server");
        m_btnBackend->setProperty("backendActive", "llama");
        break;
    }
    P::repolish(m_btnBackend);
}

/* ══════════════════════════════════════════════════════════════
   onMathDlBtnClicked — slot del pulsante "Scarica modello matematico"
   dentro showServerDialog() (ex lambda)
   ══════════════════════════════════════════════════════════════ */
void MainWindow::onMathDlBtnClicked()
{
    /* Il pulsante è figlio di dlg (il QDialog di showServerDialog) */
    auto* dlg = qobject_cast<QDialog*>(sender()->parent());
    showMathDownloadDialog(dlg ? static_cast<QWidget*>(dlg) : this, P::modelsDir());
}
