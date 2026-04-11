#include "mainwindow.h"
#include "pages/agenti_page.h"
#include "pages/pratico_page.h"
#include "pages/impara_page.h"
#include "pages/impostazioni_page.h"
#include "pages/programmazione_page.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QSizePolicy>
#include <QFont>
#include <QStatusBar>
#include <QMenu>
#include <QAction>
#include <QProcess>
#include <QTimer>
#include <QDialog>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QLineEdit>
#include <QDir>
#include <QFileInfo>
#include <QSpinBox>
#include <QGroupBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QCheckBox>
#include "prismalux_paths.h"
#include "widgets/spinner_widget.h"
#include "chat_history.h"
#include <QShortcut>
#include <QListWidget>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>

namespace P = PrismaluxPaths;

/* ── Forward declaration helpers math (definiti più in basso) ── */
static bool isMathModel(const QString& filename);
static void showMathDownloadDialog(QWidget* parent, const QString& modelsDir);

/* ══════════════════════════════════════════════════════════════
   ResourceGauge
   ══════════════════════════════════════════════════════════════ */
ResourceGauge::ResourceGauge(const QString& label, QWidget* parent)
    : QWidget(parent)
{
    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(0,0,0,0);
    lay->setSpacing(5);

    m_lbl = new QLabel(label, this);
    m_lbl->setObjectName("gaugeLabel");
    m_lbl->setFixedWidth(34);

    m_bar = new QProgressBar(this);
    m_bar->setObjectName("resBar");
    m_bar->setRange(0,100);
    m_bar->setValue(0);
    m_bar->setTextVisible(false);
    m_bar->setFixedSize(70, 8);

    m_pct = new QLabel("  0.0%", this);
    m_pct->setObjectName("gaugePct");
    m_pct->setFixedWidth(42);

    lay->addWidget(m_lbl);
    lay->addWidget(m_bar);
    lay->addWidget(m_pct);
}

void ResourceGauge::update(double pct, const QString& detail) {
    m_bar->setValue(static_cast<int>(pct));
    m_pct->setText(QString("%1%").arg(pct, 5, 'f', 1));
    setLevel(pct);
    if (!detail.isEmpty())
        setToolTip(detail);
}

void ResourceGauge::setLevel(double pct) {
    /* Soglie QSS: < 70% verde (default), 70-90% giallo, > 90% rosso */
    const QString lvl = (pct >= 90) ? "crit" : (pct >= 70) ? "warn" : "";
    m_bar->setProperty("level", lvl);
    P::repolish(m_bar);  /* forza ricalcolo stile dopo cambio property */
}

/* ══════════════════════════════════════════════════════════════
   MainWindow — costruttore
   ══════════════════════════════════════════════════════════════ */
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("🍺 Prismalux v2.1 — Centro di Controllo");
    setMinimumSize(1060, 680);
    resize(1200, 760);

    /* ── Servizi di background ── */
    m_hw     = new HardwareMonitor(this);
    m_ai     = new AiClient(this);
    m_chatDb = new ChatHistoryDb(this);

    connect(m_hw, &HardwareMonitor::updated,     this, &MainWindow::onHWUpdated);
    connect(m_hw, &HardwareMonitor::hwInfoReady, this, &MainWindow::onHWReady);

    /* ── Layout principale: Header + [Sidebar | Content] ── */
    auto* root   = new QWidget(this);
    auto* rootLay = new QVBoxLayout(root);
    rootLay->setContentsMargins(0,0,0,0);
    rootLay->setSpacing(0);

    rootLay->addWidget(buildHeader());

    auto* body    = new QWidget(root);
    auto* bodyLay = new QHBoxLayout(body);
    bodyLay->setContentsMargins(0,0,0,0);
    bodyLay->setSpacing(0);
    bodyLay->addWidget(buildSidebar());
    bodyLay->addWidget(buildContent(), 1);
    rootLay->addWidget(body, 1);

    setCentralWidget(root);

    /* Status bar */
    statusBar()->showMessage("🍺  Invocazione riuscita. Gli dei ascoltano.");

    /* Avvia monitoraggio hardware */
    m_hw->start();

    /* Timer auto-scarico modello: ogni 90s, se RAM > 40% e AI non occupato,
     * manda keep_alive=0 a Ollama per liberare RAM automaticamente. */
    m_idleUnloadTimer = new QTimer(this);
    m_idleUnloadTimer->setInterval(90'000);  /* 90 secondi */
    connect(m_idleUnloadTimer, &QTimer::timeout, this, [this]{
        if (!m_ai->busy() && m_ai->isModelLoaded() && m_ai->backend() == AiClient::Ollama) {
            const double freePct = m_ai->ramFreePct();
            if (freePct < 60.0) {  /* RAM usata > 40% */
                m_ai->unloadModel();
                statusBar()->showMessage(
                    "\xf0\x9f\x97\x91  Auto-scarico: modello rimosso dalla RAM "
                    "(RAM > 40% — scarico automatico dopo inattivit\xc3\xa0).");
                QTimer::singleShot(5000, this, [this]{
                    statusBar()->showMessage(
                        "\xf0\x9f\x8d\xba  Invocazione riuscita. Gli dei ascoltano.");
                });
            }
        }
    });
    m_idleUnloadTimer->start();

    /* Imposta backend Ollama di default e carica modelli */
    m_ai->setBackend(AiClient::Ollama, P::kLocalHost, P::kOllamaPort, "");
    m_ai->fetchModels();
    connect(m_ai, &AiClient::modelsReady, this, [this](const QStringList& list){
        if (!list.isEmpty()) {
            m_lblModel->setText(list.first());
            statusBar()->showMessage(
                QString("🍺  Backend Ollama | Modello: %1 | Modelli disponibili: %2")
                .arg(list.first()).arg(list.size()));
        }
    });

    /* Carica tema salvato */
    ThemeManager::instance()->loadSaved();

    /* Pagina iniziale */
    navigateTo(0);

    /* Scorciatoie da tastiera */
    auto* sc1 = new QShortcut(QKeySequence("Alt+1"), this);
    auto* sc2 = new QShortcut(QKeySequence("Alt+2"), this);
    auto* sc3 = new QShortcut(QKeySequence("Alt+3"), this);
    auto* sc4 = new QShortcut(QKeySequence("Alt+4"), this);
    connect(sc1, &QShortcut::activated, this, [this]{ navigateTo(0); });
    connect(sc2, &QShortcut::activated, this, [this]{ navigateTo(1); });
    connect(sc3, &QShortcut::activated, this, [this]{ navigateTo(2); });
    connect(sc4, &QShortcut::activated, this, [this]{ navigateTo(3); });

    /* F5 — ricarica lista modelli dal backend attivo */
    auto* scF5 = new QShortcut(QKeySequence("F5"), this);
    connect(scF5, &QShortcut::activated, this, [this]{
        m_ai->fetchModels();
        statusBar()->showMessage("\xf0\x9f\x94\x84  Ricarica modelli...");
    });

    /* Escape — interrompi generazione AI in corso */
    auto* scEsc = new QShortcut(QKeySequence("Escape"), this);
    connect(scEsc, &QShortcut::activated, this, [this]{
        if (m_ai->busy()) {
            m_ai->abort();
            statusBar()->showMessage("\xe2\x9b\x94  Generazione AI interrotta.");
        }
    });

    /* Ctrl+, — apri finestra Impostazioni */
    auto* scSet = new QShortcut(QKeySequence("Ctrl+,"), this);
    connect(scSet, &QShortcut::activated, this, [this]{
        m_settingsDlg->show();
        m_settingsDlg->raise();
        m_settingsDlg->activateWindow();
    });
}

/* ══════════════════════════════════════════════════════════════
   buildHeader — barra superiore con logo + gauges hardware
   ══════════════════════════════════════════════════════════════ */
QWidget* MainWindow::buildHeader() {
    auto* hdr = new QWidget(this);
    hdr->setObjectName("header");
    hdr->setFixedHeight(72);

    auto* lay = new QHBoxLayout(hdr);
    lay->setContentsMargins(16, 8, 16, 8);
    lay->setSpacing(12);

    /* Logo */
    auto* beer = new QLabel("🍺", hdr);
    beer->setObjectName("headerBeer");
    lay->addWidget(beer);

    auto* titleBox = new QWidget(hdr);
    auto* titleLay = new QVBoxLayout(titleBox);
    titleLay->setContentsMargins(0,0,0,0);
    titleLay->setSpacing(2);
    auto* title = new QLabel("P R I S M A L U X  v2.1", titleBox);
    title->setObjectName("headerTitle");
    m_lblBackend = new QLabel("Ollama  →  127.0.0.1:11434", titleBox);
    m_lblBackend->setObjectName("headerSub");
    titleLay->addWidget(title);
    titleLay->addWidget(m_lblBackend);
    lay->addWidget(titleBox);

    lay->addStretch(1);

    /* Gauges hardware — una sola riga orizzontale: CPU  RAM  GPU */
    m_gCpu = new ResourceGauge("CPU ", hdr);
    m_gRam = new ResourceGauge("RAM ", hdr);
    m_gGpu = new ResourceGauge("GPU ", hdr);
    lay->addWidget(m_gCpu);
    lay->addWidget(m_gRam);
    lay->addWidget(m_gGpu);

    /* m_lblModel tenuto in vita per i setText interni, ma non mostrato */
    m_lblModel = new QLabel(this);
    m_lblModel->hide();

    auto* beer2 = new QLabel("🍺", hdr);
    beer2->setObjectName("headerBeer");
    lay->addWidget(beer2);

    /* ── Pulsante emergenza RAM 🚨 ── */
    auto* btnEmergency = new QPushButton("🚨", hdr);
    btnEmergency->setObjectName("emergencyBtn");
    btnEmergency->setToolTip(
        "EMERGENZA RAM\n"
        "1. Ferma tutti i modelli Ollama\n"
        "2. Libera cache kernel (richiede password admin)");
    btnEmergency->setFixedSize(42, 36);

    connect(btnEmergency, &QPushButton::clicked, this, [this, btnEmergency]{
        btnEmergency->setEnabled(false);
        statusBar()->showMessage("🚨  Emergenza RAM — arresto modelli Ollama...");

        /* Passo 1: ferma tutti i modelli Ollama via CLI */
        auto* stopProc = new QProcess(this);
        stopProc->start("bash", {"-c",
            "ollama ps --no-trunc 2>/dev/null | awk 'NR>1{print $1}' | "
            "xargs -r -I{} ollama stop {}"});

        connect(stopProc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, btnEmergency, stopProc](int, QProcess::ExitStatus){
            stopProc->deleteLater();
            statusBar()->showMessage("🚨  Modelli fermati — liberazione cache kernel...");

            /* Passo 2: drop_caches con dialogo grafico KDE */
            auto* cacheProc = new QProcess(this);
#ifdef Q_OS_WIN
            cacheProc->start("rundll32.exe", {"advapi32.dll,ProcessIdleTasks"});
#else
            cacheProc->start("pkexec",
                {"sh", "-c", "sync && echo 3 > /proc/sys/vm/drop_caches"});
#endif
            connect(cacheProc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                    this, [this, btnEmergency, cacheProc](int code, QProcess::ExitStatus){
                cacheProc->deleteLater();
                if (code == 0)
                    statusBar()->showMessage(
                        "✅  Emergenza RAM completata — modelli fermati + cache liberata.");
                else
                    statusBar()->showMessage(
                        "⚠️  Modelli fermati. Cache non liberata (password annullata o pkexec mancante).");
                btnEmergency->setEnabled(true);
                /* Ripristina messaggio normale dopo 6 secondi */
                QTimer::singleShot(6000, this, [this]{
                    statusBar()->showMessage("🍺  Invocazione riuscita. Gli dei ascoltano.");
                });
            });
        });
    });

    lay->addWidget(btnEmergency);

    /* ── Pulsante "Scarica LLM" — scarica il modello dalla RAM via API Ollama ── */
    m_btnUnload = new QPushButton("\xf0\x9f\x97\x91", hdr);
    m_btnUnload->setObjectName("actionBtn");
    m_btnUnload->setFixedSize(36, 36);
    m_btnUnload->setToolTip(
        "Scarica il modello dalla RAM (keep_alive=0)\n"
        "Utile quando Ollama tiene il modello caricato\n"
        "anche dopo che hai finito di usarlo.\n\n"
        "Diventa giallo quando la RAM supera il 40%.");
    connect(m_btnUnload, &QPushButton::clicked, this, [this]{
        m_ai->unloadModel();
        statusBar()->showMessage(
            "\xf0\x9f\x97\x91  Richiesta scarico modello inviata a Ollama — "
            "il modello verr\xc3\xa0 rimosso dalla RAM.");
        QTimer::singleShot(4000, this, [this]{
            statusBar()->showMessage("\xf0\x9f\x8d\xba  Invocazione riuscita. Gli dei ascoltano.");
        });
    });
    lay->addWidget(m_btnUnload);

    /* m_btnBackend / m_badgeServer / m_spinServer sono creati in buildSidebar()
     * e posizionati in cima alla colonna sinistra, prima dei navBtn. */

    /* ── Pulsante Monitor Attività (📊) — apre pannello diagnostica ── */
    auto* monBtn = new QPushButton("\xf0\x9f\x93\x8a", hdr);
    monBtn->setObjectName("actionBtn");
    monBtn->setFixedSize(36, 36);
    monBtn->setToolTip(
        "Monitor Attivit\xc3\xa0 AI\n"
        "Misura TTFT, durata, RAM e throughput di ogni inferenza.");
    lay->addWidget(monBtn);

    connect(monBtn, &QPushButton::clicked, this, [this]{
        /* Crea il pannello una sola volta (lazy), poi lo porta in primo piano */
        if (!m_monitorPanel) {
            m_monitorPanel = new MonitorPanel(m_ai, m_hw, this);
            m_monitorPanel->setAttribute(Qt::WA_DeleteOnClose, false);
        }
        m_monitorPanel->show();
        m_monitorPanel->raise();
        m_monitorPanel->activateWindow();
    });

    /* ── Pulsante Impostazioni (⚙️) — apre la pagina Impostazioni ── */
    m_themeBtn = new QPushButton("⚙️", hdr);
    m_themeBtn->setObjectName("themeBtn");
    m_themeBtn->setToolTip("Impostazioni");
    m_themeBtn->setFixedSize(36, 36);
    lay->addWidget(m_themeBtn);

    connect(m_themeBtn, &QPushButton::clicked, this, [this]{
        m_settingsDlg->show();
        m_settingsDlg->raise();
        m_settingsDlg->activateWindow();
    });

    return hdr;
}

/* ══════════════════════════════════════════════════════════════
   showServerDialog — dialog avvio llama-server
   Estratto dal lambda di m_btnServer per essere richiamato
   anche dal menu contestuale di m_btnBackend.
   ══════════════════════════════════════════════════════════════ */
void MainWindow::showServerDialog()
{
    const QStringList modelPaths = P::scanGgufFiles();

    auto* dlg = new QDialog(this);
    dlg->setWindowTitle("\xf0\x9f\xa6\x99  Avvia llama-server");
    dlg->setFixedWidth(460);
    auto* lay = new QVBoxLayout(dlg);
    lay->setSpacing(10);

    lay->addWidget(new QLabel(
        "<b>Seleziona modello</b> \xe2\x80\x94 il server parte in background,<br>"
        "il backend viene commutato automaticamente.", dlg));

    QStringList mathPaths, otherPaths;
    for (const QString& p : modelPaths) {
        if (isMathModel(QFileInfo(p).fileName())) mathPaths << p;
        else                                        otherPaths << p;
    }

    auto* cmbModel = new QComboBox(dlg);

    auto populateCombo = [&](bool mathOnly) {
        cmbModel->clear();
        if (mathPaths.isEmpty() && otherPaths.isEmpty()) {
            cmbModel->addItem("(nessun .gguf trovato in models/)");
            cmbModel->setEnabled(false);
            return;
        }
        cmbModel->setEnabled(true);
        for (const QString& p : mathPaths)
            cmbModel->addItem("\xf0\x9f\x93\x90 " + QFileInfo(p).fileName(), p);
        if (!mathOnly)
            for (const QString& p : otherPaths)
                cmbModel->addItem(QFileInfo(p).fileName(), p);
    };
    populateCombo(false);
    lay->addWidget(cmbModel);

    auto* rowPort = new QHBoxLayout;
    rowPort->addWidget(new QLabel("Porta:", dlg));
    auto* spPort = new QSpinBox(dlg);
    spPort->setRange(1024, 65535);
    spPort->setValue(P::kLlamaServerPort);
    spPort->setFixedWidth(90);
    rowPort->addWidget(spPort);
    rowPort->addStretch();
    lay->addLayout(rowPort);

    auto* chkMath = new QCheckBox("\xf0\x9f\x93\x90  Profilo matematico (Xeon 64 GB)", dlg);
    chkMath->setToolTip(
        "Abilita flag ottimali per calcolo scientifico:\n"
        "  --ctx-size 8192  (dimostrazioni lunghe)\n"
        "  --no-mmap        (Q4_K_M: carica tutto in RAM, pi\xc3\xb9 veloce)\n"
        "  mmap attivo      (Q8_0: legge dal SSD on-demand, auto)");
    lay->addWidget(chkMath);

    auto* lblMathStatus = new QLabel(dlg);
    lblMathStatus->setWordWrap(true);
    lay->addWidget(lblMathStatus);

    auto* btnMathDl = new QPushButton(
        "\xe2\xac\x87  Scarica modello matematico da Hugging Face", dlg);
    btnMathDl->setObjectName("actionBtn");
    btnMathDl->setVisible(false);
    lay->addWidget(btnMathDl);

    auto updateMathUI = [&](bool mathOn) {
        if (!mathOn) { populateCombo(false); lblMathStatus->hide(); btnMathDl->hide(); return; }
        populateCombo(false);
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
            cmbModel->setCurrentIndex(0);
        }
    };

    connect(chkMath, &QCheckBox::toggled, dlg, updateMathUI);
    connect(cmbModel, QOverload<int>::of(&QComboBox::currentIndexChanged), dlg,
            [chkMath, cmbModel](int){
        const QString name = cmbModel->currentText().toLower();
        chkMath->setChecked(name.contains("72b") || name.contains("70b") || name.contains("math"));
    });
    connect(btnMathDl, &QPushButton::clicked, dlg, [dlg]{
        showMathDownloadDialog(dlg, P::modelsDir());
    });

    {
        const QString name = cmbModel->currentText().toLower();
        bool initMath = !mathPaths.isEmpty() || name.contains("math") || name.contains("72b");
        chkMath->setChecked(initMath);
        updateMathUI(initMath);
    }

    lay->addWidget(new QLabel(
        "<span style='color:#5a5f80;font-size:11px;'>"
        "Binario cercato in: llama_cpp_studio/llama.cpp/build/bin/llama-server<br>"
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
        QString modelPath = cmbModel->currentData().toString();
        startLlamaServer(modelPath, spPort->value(), chkMath->isChecked());
    }
    dlg->deleteLater();
}

/* ══════════════════════════════════════════════════════════════
   applyBackend — cambia backend, aggiorna label + pulsante
   ══════════════════════════════════════════════════════════════ */
void MainWindow::applyBackend(AiClient::Backend b, const QString& host, int port) {
    m_ai->setBackend(b, host, port, "");
    m_ai->fetchModels();

    refreshBackendBtn();

    const QString bkName = (b == AiClient::Ollama) ? "Ollama" : "llama-server";
    m_lblBackend->setText(bkName + "  →  " + host + ":" + QString::number(port));
    m_lblModel->setText("(caricamento modelli...)");

    statusBar()->showMessage(
        QString("🔄  Backend cambiato: %1 @ %2:%3 — recupero modelli...")
        .arg(bkName, host, QString::number(port)));

    /* Quando arrivano i modelli, seleziona il primo e aggiorna status (connessione unica) */
    auto* connHolder = new QObject(this);
    connect(m_ai, &AiClient::modelsReady, connHolder, [this, bkName, connHolder](const QStringList& list){
        if (!list.isEmpty()) {
            m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), list.first());
            m_lblModel->setText(list.first());
            statusBar()->showMessage(
                QString("✅  %1 | Modello: %2 | %3 disponibili")
                .arg(bkName, list.first(), QString::number(list.size())));
        } else {
            m_lblModel->setText("(server non raggiungibile)");
            statusBar()->showMessage(
                QString("⚠️  %1 non risponde — avvialo prima di usare l'AI").arg(bkName));
        }
        connHolder->deleteLater(); /* rimuove la connessione dopo il primo segnale */
    });
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
    dlg->setMinimumWidth(620);
    auto* lay = new QVBoxLayout(dlg);
    lay->setSpacing(12);

    lay->addWidget(new QLabel(
        "<b>Seleziona modello e variante di quantizzazione:</b>", dlg));

    /* Una riga per ogni modello con radio Q4 / Q8.
     * Si usa QFrame invece di QGroupBox: QGroupBox applica auto-esclusività
     * ai radio button figli indipendentemente dal QButtonGroup globale,
     * causando conflitti su Windows che impediscono la selezione. */
    QVector<QPair<QRadioButton*, QRadioButton*>> rows;
    auto* grp = new QButtonGroup(dlg);

    for (const auto& m : catalog) {
        /* Titolo modello */
        auto* titleLbl = new QLabel(
            QString("<b style='font-size:12px;'>%1</b>").arg(m.name), dlg);
        lay->addWidget(titleLbl);

        /* Contenitore neutro: QFrame, nessuna auto-esclusività interna */
        auto* box = new QFrame(dlg);
        box->setFrameShape(QFrame::StyledPanel);
        box->setStyleSheet("QFrame { border:1px solid #44475a; border-radius:6px; padding:4px; }");
        auto* bl = new QVBoxLayout(box);
        bl->setSpacing(4);
        bl->addWidget(new QLabel(
            "<span style='color:#888;font-size:11px;'>" + m.description + "</span>", box));

        auto* rq4 = new QRadioButton(
            QString("Q4_K_M  %1  \xe2\x80\x94 carica tutto in RAM%2")
            .arg(m.sizeQ4)
            .arg(m.name.contains("72B") ? "  \xe2\x9c\x85 consigliato per Xeon 64 GB" : ""), box);
        auto* rq8 = new QRadioButton(
            QString("Q8_0    %1  \xe2\x80\x94 qualit\xc3\xa0 massima, richiede NVMe SSD%2")
            .arg(m.sizeQ8)
            .arg(m.name.contains("72B") ? " (mmap automatico)" : ""), box);

        /* Disabilita auto-esclusività locale: la gestisce solo il QButtonGroup */
        rq4->setAutoExclusive(false);
        rq8->setAutoExclusive(false);
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

    /* Avvia wget in background */
    QString cmd = QString("wget -c --progress=bar:force \"%1\" -O \"%2\" 2>&1 &")
                  .arg(url, dest);
    QProcess::startDetached("bash", {"-c", cmd});

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

    /* Variabile d'ambiente per le librerie condivise (.so nella stessa dir del binario) */
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("LD_LIBRARY_PATH",
        P::llamaLibDir() + ":" + env.value("LD_LIBRARY_PATH"));

    m_serverProc = new QProcess(this);
    m_serverProc->setProcessEnvironment(env);
    m_serverProc->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_serverProc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int code, QProcess::ExitStatus) {
        statusBar()->showMessage(
            QString("🔴  llama-server terminato (code %1). Backend tornato a Ollama.").arg(code));
        m_serverProc->deleteLater();
        m_serverProc = nullptr;
        /* Ripristina Ollama come backend */
        applyBackend(AiClient::Ollama, P::kLocalHost, P::kOllamaPort);
    });

    /* Determina se il modello è Q4 (per --no-mmap) o Q8/più grande */
    bool isQ4 = QFileInfo(modelPath).fileName().contains("Q4", Qt::CaseInsensitive);

    QStringList args = {
        "-m", modelPath,
        "--port", QString::number(port),
        "--host", "127.0.0.1",
        "--log-disable"
    };
    if (mathProfile) {
        args << "--ctx-size" << "8192";
        /* Q4_K_M (~40 GB): carica tutto in RAM — più veloce senza mmap */
        if (isQ4) args << "--no-mmap";
        /* Q8_0 (~74 GB): mmap attivo per default (llama.cpp legge dal SSD on-demand) */
        statusBar()->showMessage(
            QString("⏳  Profilo matematico: ctx=8192%1 | avvio server su porta %2...")
            .arg(isQ4 ? " + no-mmap" : " + mmap(SSD)").arg(port));
    }
    m_serverProc->start(bin, args);

    /*
     * Usiamo il segnale errorOccurred invece di waitForStarted() bloccante:
     * waitForStarted congela il thread UI per fino a 4s.
     * errorOccurred viene emesso immediatamente se il processo non parte.
     */
    connect(m_serverProc, &QProcess::errorOccurred,
            this, [this](QProcess::ProcessError err){
        if (err == QProcess::FailedToStart) {
            if (m_badgeServer) m_badgeServer->setStatus(StatusBadge::Error,   "Errore");
            if (m_spinServer)  m_spinServer->stop("\xe2\x9d\x8c Errore", 3000);
            statusBar()->showMessage(
                "❌  Impossibile avviare llama-server. Verifica il percorso binario.");
            m_serverProc->deleteLater();
            m_serverProc = nullptr;
        }
    });

    statusBar()->showMessage(
        QString("⏳  llama-server avviato — attendo che sia pronto (porta %1)...").arg(port));

    /* Avvia lo spinner e imposta badge "Starting" (giallo) */
    if (m_badgeServer) m_badgeServer->setStatus(StatusBadge::Starting, "Avvio...");
    if (m_spinServer)  m_spinServer->start("Avvio server...");

    /*
     * Polling /health ogni 1s, max 30 tentativi.
     *
     * Usiamo un QTimer con un singolo QNetworkAccessManager creato fuori
     * dal ciclo (non uno nuovo per ogni tick: ogni NAM porta overhead di
     * thread e pool di connessioni). Il counter è un int membro del QTimer
     * tramite setProperty per evitare new int() sul heap con delete manuale.
     */
    auto* timer = new QTimer(this);
    auto* nam   = new QNetworkAccessManager(this); /* un solo NAM per tutti i tick */
    timer->setProperty("ticks", 0);

    connect(timer, &QTimer::timeout, this, [this, timer, nam, port]{
        const int ticks = timer->property("ticks").toInt() + 1;
        timer->setProperty("ticks", ticks);

        /* Timeout: 30 secondi senza risposta */
        if (ticks > 30 || !m_serverProc) {
            timer->stop();
            timer->deleteLater();
            nam->deleteLater();
            if (m_badgeServer) m_badgeServer->setStatus(StatusBadge::Error,   "Timeout");
            if (m_spinServer)  m_spinServer->stop("\xe2\x9d\x8c Timeout", 3000);
            if (m_serverProc)
                statusBar()->showMessage(
                    QString("⚠️  llama-server non risponde dopo 30s sulla porta %1").arg(port));
            return;
        }

        /* GET /health — timeout breve per non sovrapporre i tick */
        QNetworkRequest req(QUrl(
            QString("http://%1:%2/health").arg(P::kLocalHost).arg(port)));
        req.setTransferTimeout(900);
        auto* reply = nam->get(req);

        connect(reply, &QNetworkReply::finished, this, [this, timer, nam, port, reply]{
            const bool ok = (reply->error() == QNetworkReply::NoError);
            reply->deleteLater();
            if (ok) {
                /* Server pronto: ferma il polling, aggiorna badge e commuta il backend */
                timer->stop();
                timer->deleteLater();
                nam->deleteLater();
                if (m_badgeServer) m_badgeServer->setStatus(StatusBadge::Online,   "Online");
                if (m_spinServer)  m_spinServer->stop("\xe2\x9c\x85 Pronto", 2500);
                statusBar()->showMessage(
                    QString("✅  llama-server pronto su porta %1 — backend commutato.").arg(port));
                applyBackend(AiClient::LlamaServer, P::kLocalHost, port);
            }
            /* Se non ok: il timer ritenterà al prossimo tick */
        });
    });
    timer->start(1000);
}

/* ── Ferma llama-server avviato dalla GUI ── */
void MainWindow::stopLlamaServer() {
    if (!m_serverProc) return;
    m_serverProc->terminate();
    if (m_badgeServer) m_badgeServer->setStatus(StatusBadge::Offline, "Server");
    statusBar()->showMessage("🛑  Arresto llama-server in corso...");
}

/* ── Aggiorna testo e colore del pulsante backend ── */
void MainWindow::refreshBackendBtn() {
    if (!m_btnBackend) return;
    if (m_ai->backend() == AiClient::Ollama) {
        m_btnBackend->setText("\xf0\x9f\x90\xa6  Ollama");
        m_btnBackend->setProperty("backendActive", "ollama");
    } else {
        m_btnBackend->setText("\xf0\x9f\xa6\x99  llama-server");
        m_btnBackend->setProperty("backendActive", "llama");
    }
    P::repolish(m_btnBackend);
}

/* ══════════════════════════════════════════════════════════════
   buildSidebar — colonna sinistra con bottoni navigazione
   ══════════════════════════════════════════════════════════════ */
QWidget* MainWindow::buildSidebar() {
    auto* bar = new QWidget(this);
    bar->setObjectName("sidebar");

    auto* lay = new QVBoxLayout(bar);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    /* ── Backend selector ── posizionato sopra i navBtn ── */
    auto* backendWrap = new QWidget(bar);
    backendWrap->setObjectName("sidebarBackendRow");
    auto* brl = new QHBoxLayout(backendWrap);
    brl->setContentsMargins(8, 8, 8, 6);
    brl->setSpacing(6);

    m_btnBackend = new QPushButton("\xf0\x9f\x90\xa6  Ollama", backendWrap);
    m_btnBackend->setObjectName("backendBtn");
    m_btnBackend->setToolTip("Backend AI — click per cambiare tra Ollama e llama-server");
    m_btnBackend->setFixedHeight(32);

    connect(m_btnBackend, &QPushButton::clicked, this, [this]{
        auto* menu = new QMenu(m_btnBackend);
        menu->setObjectName("backendMenu");

        const bool serverRunning = m_serverProc &&
                                   m_serverProc->state() != QProcess::NotRunning;
        const bool isOllama = (m_ai->backend() == AiClient::Ollama);

        auto* actOllama = menu->addAction("\xf0\x9f\x90\xa6  Ollama  \xe2\x80\x94  localhost:11434");
        actOllama->setCheckable(true);
        actOllama->setChecked(isOllama);
        connect(actOllama, &QAction::triggered, this, [this]{
            applyBackend(AiClient::Ollama, P::kLocalHost, P::kOllamaPort);
        });

        menu->addSeparator();

        if (serverRunning) {
            auto* actInfo = menu->addAction(
                QString("\xf0\x9f\xa6\x99  llama-server :%1  \xe2\x97\x8f in esecuzione")
                .arg(m_serverPort));
            actInfo->setEnabled(false);
            auto* actStop = menu->addAction(
                QString("\xf0\x9f\x94\xb4  Ferma server  :%1").arg(m_serverPort));
            connect(actStop, &QAction::triggered, this, &MainWindow::stopLlamaServer);
        } else {
            auto* actLSrv = menu->addAction("\xf0\x9f\xa6\x99  Avvia llama-server...");
            actLSrv->setCheckable(true);
            actLSrv->setChecked(!isOllama);
            connect(actLSrv, &QAction::triggered, this, &MainWindow::showServerDialog);
        }

        menu->addSeparator();
        auto* actCustom = menu->addAction("\xe2\x9c\x8f\xef\xb8\x8f  Connessione personalizzata...");
        connect(actCustom, &QAction::triggered, this, [this]{
            auto* dlg = new QDialog(this);
            dlg->setWindowTitle("Connessione personalizzata");
            dlg->setFixedWidth(320);
            auto* lay = new QVBoxLayout(dlg);
            auto* row1 = new QHBoxLayout;
            row1->addWidget(new QLabel("Host:", dlg));
            auto* eHost = new QLineEdit(m_ai->host(), dlg);
            row1->addWidget(eHost);
            lay->addLayout(row1);
            auto* row2 = new QHBoxLayout;
            row2->addWidget(new QLabel("Porta:", dlg));
            auto* ePort = new QLineEdit(QString::number(m_ai->port()), dlg);
            ePort->setFixedWidth(80);
            row2->addWidget(ePort);
            row2->addStretch();
            lay->addLayout(row2);
            auto* cmbBk = new QComboBox(dlg);
            cmbBk->addItem("Ollama");
            cmbBk->addItem("llama-server");
            cmbBk->setCurrentIndex(m_ai->backend() == AiClient::Ollama ? 0 : 1);
            lay->addWidget(cmbBk);
            auto* bb = new QDialogButtonBox(
                QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
            lay->addWidget(bb);
            connect(bb, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
            connect(bb, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
            if (dlg->exec() == QDialog::Accepted) {
                AiClient::Backend bk = (cmbBk->currentIndex() == 0)
                    ? AiClient::Ollama : AiClient::LlamaServer;
                applyBackend(bk, eHost->text(), ePort->text().toInt());
            }
            dlg->deleteLater();
        });

        menu->exec(m_btnBackend->mapToGlobal(QPoint(0, m_btnBackend->height())));
        menu->deleteLater();
    });

    brl->addWidget(m_btnBackend, 1);

    m_badgeServer = new StatusBadge("", backendWrap);
    m_badgeServer->setStatus(StatusBadge::Offline);
    brl->addWidget(m_badgeServer);

    lay->addWidget(backendWrap);

    /* Spinner avvio server — visibile solo durante il polling /health */
    m_spinServer = new SpinnerWidget("", bar);
    m_spinServer->setContentsMargins(10, 0, 10, 4);
    lay->addWidget(m_spinServer);

    /* Separatore tra backend selector e navBtn */
    auto* sepBk = new QFrame(bar);
    sepBk->setFrameShape(QFrame::HLine);
    sepBk->setObjectName("sidebarSep");
    lay->addWidget(sepBk);

    /* Voci menu: [icona + titolo + descrizione + shortcut tooltip] */
    struct NavItem { const char* icon; const char* title; const char* desc; const char* tip; };
    static const NavItem ITEMS[4] = {
        {"🤖", "Agenti AI",        "Pipeline 6 agenti\nMotore Byzantino",    "Alt+1"},
        {"\xf0\x9f\x92\xb0", "Finanza\nPersonale", "730, P.IVA\nCerca Lavoro", "Alt+2"},
        {"📚", "Impara\ncon AI",    "Tutor · Quiz\nDashboard Progressi",      "Alt+3"},
        {"\xf0\x9f\x92\xbb", "Programmazione", "Genera · Esegui\nSalva output",  "Alt+4"},
    };

    for (int i = 0; i < 4; i++) {
        auto* btn = new QPushButton(bar);
        btn->setObjectName("navBtn");
        btn->setCheckable(false);
        btn->setFlat(true);
        btn->setFixedHeight(80);
        btn->setToolTip(QString("%1 — Scorciatoia: %2").arg(ITEMS[i].title, ITEMS[i].tip));
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        /* Layout interno: icona | titolo + descrizione */
        auto* w   = new QWidget(btn);
        auto* wl  = new QHBoxLayout(w);
        wl->setContentsMargins(12, 4, 8, 4);
        wl->setSpacing(10);

        auto* ico = new QLabel(ITEMS[i].icon, w);
        ico->setObjectName("cardIcon");
        ico->setFixedWidth(38);
        ico->setAlignment(Qt::AlignCenter);

        auto* txt  = new QWidget(w);
        auto* txtL = new QVBoxLayout(txt);
        txtL->setContentsMargins(0,0,0,0);
        txtL->setSpacing(2);

        auto* lTitle = new QLabel(ITEMS[i].title, txt);
        lTitle->setObjectName("cardTitle");
        auto* lDesc  = new QLabel(ITEMS[i].desc, txt);
        lDesc->setObjectName("cardDesc");

        txtL->addWidget(lTitle);
        txtL->addWidget(lDesc);
        wl->addWidget(ico);
        wl->addWidget(txt, 1);

        /* Bottone trasparente sopra */
        auto* btnLay = new QHBoxLayout(btn);
        btnLay->setContentsMargins(0,0,0,0);
        btnLay->addWidget(w);

        connect(btn, &QPushButton::clicked, this, [this, i]{ navigateTo(i); });
        m_navBtns[i] = btn;
        lay->addWidget(btn);

        /* Separatore sottile tra Finanza e Impara (dopo idx 1) */
        if (i == 1) {
            auto* sep2 = new QFrame(bar);
            sep2->setFrameShape(QFrame::HLine);
            sep2->setObjectName("sidebarSep");
            lay->addWidget(sep2);
        }
    }

    /* ── Separatore + intestazione cronologia ── */
    auto* sep = new QFrame(bar);
    sep->setFrameShape(QFrame::HLine);
    sep->setObjectName("sidebarSep");
    lay->addWidget(sep);

    auto* chatHdr = new QLabel("\xf0\x9f\x95\x90  Cronologia", bar);
    chatHdr->setObjectName("cardTitle");
    chatHdr->setContentsMargins(12, 6, 12, 2);
    lay->addWidget(chatHdr);

    /* ── Lista chat — si espande per riempire lo spazio disponibile ── */
    m_chatList = new QListWidget(bar);
    m_chatList->setObjectName("chatHistoryList");
    m_chatList->setSpacing(1);
    m_chatList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_chatList->setContextMenuPolicy(Qt::CustomContextMenu);
    m_chatList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    lay->addWidget(m_chatList, 1);

    /* Click su un elemento → richiama la chat */
    connect(m_chatList, &QListWidget::itemClicked, this, [this](QListWidgetItem* item){
        recallChat(item->data(Qt::UserRole).toInt());
    });

    /* Tasto destro → menu contestuale con elimina */
    connect(m_chatList, &QListWidget::customContextMenuRequested,
            this, [this](const QPoint& pos){
        auto* item = m_chatList->itemAt(pos);
        if (!item) return;
        const int id = item->data(Qt::UserRole).toInt();
        QMenu menu(this);
        auto* actDel = menu.addAction("\xf0\x9f\x97\x91  Elimina");
        connect(actDel, &QAction::triggered, this, [this, id]{
            m_chatDb->deleteChat(id);
            refreshChatSidebar();
        });
        menu.exec(m_chatList->mapToGlobal(pos));
    });

    /* Carica la cronologia iniziale */
    refreshChatSidebar();

    /* Versione in fondo */
    auto* ver = new QLabel("Prismalux v2.1\n\"Costruito per i mortali\"", bar);
    ver->setObjectName("cardDesc");
    ver->setAlignment(Qt::AlignCenter);
    ver->setContentsMargins(8, 8, 8, 12);
    lay->addWidget(ver);

    return bar;
}

/* ══════════════════════════════════════════════════════════════
   buildContent — QStackedWidget con le 5 pagine
   ══════════════════════════════════════════════════════════════ */
QWidget* MainWindow::buildContent() {
    m_stack = new QStackedWidget(this);
    m_stack->setObjectName("contentArea");

    auto* agPage = new AgentiPage(m_ai, this);
    m_stack->addWidget(agPage);                              /* 0 */
    m_stack->addWidget(new PraticoPage(m_ai, this));         /* 1 */
    m_stack->addWidget(new ImparaPage(m_ai, this));          /* 2 */
    m_stack->addWidget(new ProgrammazionePage(m_ai, this));  /* 3 */

    /* Barra progresso pipeline → inserita nello status bar */
    statusBar()->addWidget(agPage->progressBar(), 1);

    /* Quando la pipeline parte → libera la status bar dal testo */
    connect(agPage, &AgentiPage::pipelineStarted, this, [this]{
        statusBar()->clearMessage();
    });

    /* Quando una chat finisce → salva nel DB e aggiorna sidebar */
    connect(agPage, &AgentiPage::chatCompleted, this,
            [this](const QString& mode, const QString& model,
                   const QString& task,  const QString& response){
        m_chatDb->saveChat(mode, model, task, response);
        refreshChatSidebar();
    });

    /* Snapshot parziale dopo ogni agente: aggiorna il DB con l'HTML corrente.
     * Questo preserva lo stile della chat anche se l'utente cambia scheda a metà pipeline.
     * Il DB viene consultato da recallChat per ripristinare fedelmente testo + colori. */
    connect(agPage, &AgentiPage::agentStepCompleted, this,
            [this](const QString& mode, const QString& model,
                   const QString& task,  const QString& html){
        /* Salva o aggiorna lo snapshot: usa la task come chiave per trovare
         * una chat esistente con lo stesso task e modo, aggiornandola in-place. */
        const auto all = m_chatDb->loadAll();
        for (const auto& entry : all) {
            if (entry.mode == mode && entry.task == task) {
                /* Già esistente: ricarica con HTML aggiornato (deleteChat + saveChat) */
                m_chatDb->deleteChat(entry.id);
                break;
            }
        }
        m_chatDb->saveChat(mode + " [in corso]", model, task, html);
        refreshChatSidebar();
    });

    /* Impostazioni: finestra separata non-modal */
    m_settingsDlg = new QDialog(this);
    m_settingsDlg->setWindowTitle("\xe2\x9a\x99\xef\xb8\x8f  Impostazioni — Prismalux");
    m_settingsDlg->setMinimumSize(820, 600);
    auto* dlgLay = new QVBoxLayout(m_settingsDlg);
    dlgLay->setContentsMargins(0, 0, 0, 0);
    m_impPage = new ImpostazioniPage(m_ai, m_hw, m_settingsDlg);
    dlgLay->addWidget(m_impPage);

    return m_stack;
}

/* ══════════════════════════════════════════════════════════════
   Navigazione
   ══════════════════════════════════════════════════════════════ */
void MainWindow::navigateTo(int idx) {
    if (idx < 0 || idx > 3) return;
    if (idx == m_activePage) return;

    if (m_ai->busy()) m_ai->abort();

    /* Deattiva bottone sidebar precedente */
    if (m_activePage >= 0 && m_activePage < 4) {
        m_navBtns[m_activePage]->setProperty("active", false);
        P::repolish(m_navBtns[m_activePage]);
    }

    m_activePage = idx;
    m_navBtns[idx]->setProperty("active", true);
    P::repolish(m_navBtns[idx]);

    /* Animazione fade 0.8→1.0 sulla nuova pagina (150 ms, non bloccante) */
    QWidget* newPage = m_stack->widget(idx);
    auto* effect = new QGraphicsOpacityEffect(newPage);
    newPage->setGraphicsEffect(effect);
    auto* anim = new QPropertyAnimation(effect, "opacity", newPage);
    anim->setDuration(150);
    anim->setStartValue(0.75);
    anim->setEndValue(1.0);
    connect(anim, &QPropertyAnimation::finished, newPage, [newPage]{
        newPage->setGraphicsEffect(nullptr);  /* rimuove l'effetto dopo l'animazione */
    });

    m_stack->setCurrentIndex(idx);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

/* ══════════════════════════════════════════════════════════════
   Slot hardware
   ══════════════════════════════════════════════════════════════ */
void MainWindow::onHWUpdated(SysSnapshot snap) {
    m_gCpu->update(snap.cpu_pct,
                   snap.cpu_name.isEmpty() ? QString("%1%").arg(snap.cpu_pct, 0, 'f', 1)
                                           : snap.cpu_name);
    double rp = snap.ram_total > 0 ? snap.ram_used/snap.ram_total*100.0 : 0;
    m_gRam->update(rp, QString("%1 / %2 GB usati")
                   .arg(snap.ram_used,0,'f',1)
                   .arg(snap.ram_total,0,'f',1));

    /* Aggiorna percentuale RAM libera in AiClient (usata per throttle pre-chat) */
    double freePct = snap.ram_total > 0 ? (snap.ram_total - snap.ram_used) / snap.ram_total * 100.0 : 100.0;
    m_ai->setRamFreePct(freePct);

    /* Colora il pulsante "Scarica LLM": grigio se RAM ok, giallo se >40%, rosso se >75% */
    if (m_btnUnload) {
        if (rp > 75.0)
            m_btnUnload->setStyleSheet(
                "QPushButton { background: #c62828; color: #fff; border-radius: 4px; font-size:16px; }");
        else if (rp > 40.0)
            m_btnUnload->setStyleSheet(
                "QPushButton { background: #E5C400; color: #1e1e2e; border-radius: 4px; font-size:16px; }");
        else
            m_btnUnload->setStyleSheet("");  /* stile default dal QSS */
    }

    /* Auto-abort: se RAM >90% usata e AI occupato, interrompe subito.
     * Evita che il sistema si blocchi completamente durante l'inference. */
    if (rp > 90.0 && m_ai->busy()) {
        m_ai->abort();
        statusBar()->showMessage(
            "\xe2\x9a\xa0  RAM critica — inference AI interrotta automaticamente per proteggere il sistema.");
    }
    if (snap.gpu_ready)
        m_gGpu->update(snap.gpu_pct,
                       snap.gpu_name.isEmpty()
                           ? QString("%1%").arg(snap.gpu_pct, 0, 'f', 1)
                           : QString("%1 — %2%").arg(snap.gpu_name).arg(snap.gpu_pct, 0, 'f', 1));
    else
        m_gGpu->update(0, "GPU: rilevamento in corso...");
}

/* ── Chiusura finestra — pulizia RAM residua ── */
void MainWindow::closeEvent(QCloseEvent* ev) {

    /* Se l'AI sta elaborando, chiedi se fermare e scaricare il modello */
    if (m_ai->busy()) {
        QMessageBox dlg(this);
        dlg.setWindowTitle("Prismalux — Chiusura");
        dlg.setIcon(QMessageBox::Question);
        dlg.setText("<b>Un agente AI \xc3\xa8 ancora in elaborazione.</b><br>"
                    "Vuoi fermare la generazione e scaricare il modello dalla RAM?");
        dlg.setInformativeText(
            "Modello attivo: <b>" + m_ai->model() + "</b><br>"
            "Se non lo scarichi rimarr\xc3\xa0 in memoria anche dopo la chiusura.");
        auto* btnUnload = dlg.addButton("Ferma e scarica dalla RAM", QMessageBox::AcceptRole);
        dlg.addButton("Chiudi comunque",                               QMessageBox::DestructiveRole);
        auto* btnCancel = dlg.addButton("Annulla",                     QMessageBox::RejectRole);
        dlg.setDefaultButton(btnCancel);
        dlg.exec();

        if (dlg.clickedButton() == btnCancel || dlg.clickedButton() == nullptr) {
            ev->ignore();   /* Annulla — non chiudere */
            return;
        }

        /* Ferma la generazione corrente */
        m_ai->abort();

        if (dlg.clickedButton() == btnUnload) {
            /* Ollama: DELETE /api/delete scarica il modello dalla VRAM/RAM */
            QString host  = m_ai->host();
            int     port  = m_ai->port();
            QString model = m_ai->model();
            if (!model.isEmpty()) {
                QNetworkAccessManager* nam = new QNetworkAccessManager(this);
                QNetworkRequest req(QUrl(
                    QString("http://%1:%2/api/delete").arg(host).arg(port)));
                req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
                QJsonObject body; body["name"] = model;
                QNetworkReply* reply = nam->sendCustomRequest(
                    req, "DELETE", QJsonDocument(body).toJson());
                /* Attendi al massimo 2 secondi poi chiudi comunque */
                QTimer::singleShot(2000, reply, &QNetworkReply::abort);
            }
        }
    }

    /* Ferma llama-server avviato dalla GUI (se in esecuzione) */
    if (m_serverProc && m_serverProc->state() != QProcess::NotRunning) {
        m_serverProc->terminate();
        m_serverProc->waitForFinished(3000);
    }

    /* Libera cache RAM residua (best-effort, senza dialogo) */
#ifndef Q_OS_WIN
    QProcess::startDetached("bash", {"-c",
        "ollama ps --no-trunc 2>/dev/null | awk 'NR>1{print $1}' | "
        "xargs -r -I{} ollama stop {} 2>/dev/null; "
        "sync 2>/dev/null"});
#else
    QProcess::startDetached("cmd.exe", {"/c",
        "for /f \"skip=1 tokens=1\" %m in ('ollama ps --no-trunc 2^>nul') do ollama stop %m"});
#endif

    ev->accept();
}

void MainWindow::onHWReady(HWInfo hw) {
    const HWDevice& pd = hw.dev[hw.primary];

    /* Aggiorna label GPU nell'header */
    if (pd.type != DEV_CPU) {
        QString gpuName = QString::fromLocal8Bit(pd.name);
        m_lblModel->setText(gpuName);  /* temporaneo, sovrascritto dal modello AI */
    } else {
        /* Xeon → nessun allarme */
        QString cpuName = QString::fromLocal8Bit(hw.dev[0].name);
        if (cpuName.contains("Xeon", Qt::CaseInsensitive))
            statusBar()->showMessage("✓ Xeon rilevato — elaborazione CPU ad alta performance");
    }

    /* Notifica la pagina impostazioni (che contiene manutenzione) */
    if (m_impPage) m_impPage->onHWReady(hw);
}

/* ══════════════════════════════════════════════════════════════
   refreshChatSidebar — ricarica m_chatList dal database
   Ogni item mostra: icona modalità + task troncato + data
   ══════════════════════════════════════════════════════════════ */
void MainWindow::refreshChatSidebar() {
    if (!m_chatList || !m_chatDb) return;
    m_chatList->clear();

    const auto entries = m_chatDb->loadAll();
    for (const ChatEntry& e : entries) {
        /* Riga 1: icona modalità + task troncato (max 30 char) */
        const QString icon =
            (e.mode == "Byzantino")  ? "\xf0\x9f\x94\xae" :
            (e.mode == "MathTheory") ? "\xf0\x9f\xa7\xae" :
            (e.mode == "Singolo")    ? "\xe2\x9a\xa1" : "\xf0\x9f\x94\x84";
        QString short_task = e.task.left(28).replace('\n', ' ');
        if (e.task.length() > 28) short_task += "\xe2\x80\xa6";  /* … */

        /* Riga 2: data locale */
        const QDateTime dt = QDateTime::fromString(e.timestamp, Qt::ISODate);
        const QString   dateStr = dt.toLocalTime().toString("dd/MM HH:mm");

        auto* item = new QListWidgetItem(
            icon + "  " + short_task + "\n    " + dateStr, m_chatList);
        item->setData(Qt::UserRole, e.id);
        item->setToolTip(e.task);
        m_chatList->addItem(item);
    }
}

/* ══════════════════════════════════════════════════════════════
   recallChat — carica una chat nella pagina Agenti (idx 0)
   ══════════════════════════════════════════════════════════════ */
void MainWindow::recallChat(int id) {
    const ChatEntry entry = m_chatDb->loadChat(id);
    if (entry.id <= 0) return;

    navigateTo(0);
    auto* agPage = qobject_cast<AgentiPage*>(m_stack->widget(0));
    if (agPage) agPage->recallChat(entry);
}
