#include "mainwindow.h"
#include "lan_server.h"
#include "log_bus.h"
#include "widgets/onnx_embedder.h"
#include "widgets/whisper_autosetup.h"
#include "pages/main_ai.h"
#include "pages/settings_main.h"
#include "pages/main_maintenance.h"
#include "pages/main_tools.h"
#include "pages/main_graph.h"
/* oracolo_page.h rimosso: OracoloPage sostituita da grafico integrato in AgentiPage */
#include "pages/main_programming.h"
#include "pages/main_math.h"
#include "pages/main_research.h"
#include "pages/main_utility.h"
#include "pages/main_bioinformatica.h"
#include "pages/main_app_controller.h"
#include "pages/main_security.h"
#include "pages/main_lan_wan.h"
#include "pages/main_multi_agent.h"
#include "pages/main_multimedia.h"
#include "pages/main_tools_file.h"
#include "app_config.h"
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
#include <QStandardItemModel>
#include <QStandardItem>
#include <QColor>
#include <QRegularExpression>
#include <QLineEdit>
#include <QDir>
#include <QFileInfo>
#include <QSpinBox>
#include <QGroupBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QCheckBox>
#include "prismalux_paths.h"
#include "dpi_utils.h"
#include "chat_history.h"
#include "widgets/spinner_widget.h"
#include <QShortcut>
#include <QListWidget>
#include <QListWidgetItem>
#include <QTextEdit>
#include <QTextCursor>
#include <QFileDialog>
#include <QTextDocument>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QPdfWriter>
#include <QPageSize>
#include <QPageLayout>
#include <QMarginsF>
#include <QMessageBox>
#include <QApplication>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QAbstractItemView>
#include <QMouseEvent>

namespace P = PrismaluxPaths;


/* ══════════════════════════════════════════════════════════════
   MainWindow — costruttore (stepdown: ogni chiamata è un livello)
   ══════════════════════════════════════════════════════════════ */
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("🍺 Prismalux v3.0 — Centro di Controllo"));
    setWindowIcon(QIcon(P::root() + "/EXPORT/assets/prismalux.png"));
    setMinimumSize(dpiScale(1060), dpiScale(680));
    resize(dpiScale(1200), dpiScale(760));

    setupServices();
    setupLayout();
    setupStatusBar();
    setupAutoOptimizations();
    setupTimers();
    setupBackend();
    setupShortcuts();
    restoreWindowState();
    m_hw->start();   /* avvia dopo che tutti i widget gauge sono creati */
}

/* ── Livello 1: servizi di background ──────────────────────────── */
void MainWindow::setupServices()
{
    m_hw = new HardwareMonitor(this);
    m_ai = new AiClient(this);
    /* Applica SUBITO il modello salvato in QSettings, prima che setupLayout()
     * costruisca la tab AI (eager) — il cui costruttore chiama fetchModels()
     * con m_model ancora vuoto se non lo facciamo qui prima. In quel caso
     * onModelsReply() sceglie come default list.first() (il modello più
     * recente usato in Ollama, non necessariamente quello salvato da
     * Prismalux), mostrato per un istante prima che setupBackend() lo
     * corregga più avanti nel costruttore — effetto "carica un modello,
     * poi lo cambia" percepito dall'utente. setBackend() è idempotente:
     * la chiamata ripetuta in setupBackend() resta innocua. */
    {
        QSettings s("Prismalux", "GUI");
        const QString savedModel = s.value(P::SK::kActiveModel, "").toString();
        m_ai->setBackend(AiClient::Ollama, P::kLocalHost, P::kOllamaPort, savedModel);
    }
    connect(m_hw, &HardwareMonitor::updated,     this, &MainWindow::onHWUpdated);
    connect(m_hw, &HardwareMonitor::hwInfoReady, this, &MainWindow::onHWReady);

    /* Refresh automatico lista modelli ogni 5 minuti */
    m_modelRefreshTimer = new QTimer(this);
    m_modelRefreshTimer->setInterval(5 * 60 * 1000);
    connect(m_modelRefreshTimer, &QTimer::timeout, this, [this] {
        m_ai->invalidateModelCache();
        m_ai->fetchModels();
    });
    m_modelRefreshTimer->start();

    /* LogBus globale — qualsiasi scheda può usare LogBus::post() per inviare qui.
       category vuota o sconosciuta → tab "Sistema" (comportamento invariato). */
    connect(LogBus::instance(), &LogBus::event, this,
            [this](const QString& msg, const QString& category){
                appendLog(msg, category.compare("3d", Qt::CaseInsensitive) == 0 ? Log3D : LogSistema);
            });

    /* ONNX embedder locale — caricato in background se i file modello esistono */
    m_onnxEmbedder = new OnnxEmbedder(this);
    if (OnnxEmbedder::defaultModelExists()) {
        QTimer::singleShot(500, this, [this]() {
            const bool ok = m_onnxEmbedder->loadModel(
                OnnxEmbedder::defaultModelPath(),
                OnnxEmbedder::defaultVocabPath());
            if (ok) {
                m_ai->setOnnxEmbedder(m_onnxEmbedder);
                appendLog(tr("ONNX embedder: modello caricato (%1 dim)").arg(m_onnxEmbedder->dims()));
            }
        });
    }

    /* m_hw->start() è chiamato alla fine del costruttore, dopo che tutti
     * i widget (m_gCpu, m_gRam, m_gGpu) sono stati creati da setupLayout(). */
}

/* ── Livello 1: layout principale Header + [Sidebar | Content] ─── */
void MainWindow::setupLayout()
{
    auto* root    = new QWidget(this);
    auto* rootLay = new QVBoxLayout(root);
    rootLay->setContentsMargins(0, 0, 0, 0);
    rootLay->setSpacing(0);
    rootLay->addWidget(buildHeader());

    auto* body    = new QWidget(root);
    auto* bodyLay = new QHBoxLayout(body);
    bodyLay->setContentsMargins(0, 0, 0, 0);
    bodyLay->setSpacing(0);

    m_bodySplitter = new QSplitter(Qt::Horizontal, body);
    m_bodySplitter->setObjectName("bodySplitter");
    m_bodySplitter->setHandleWidth(dpiScale(4));
    m_bodySplitter->setChildrenCollapsible(false);
    m_bodySplitter->addWidget(buildSidebar());   // index 0 — sidebar
    m_bodySplitter->addWidget(buildContent());   // index 1 — contenuto
    m_bodySplitter->setStretchFactor(0, 0);
    m_bodySplitter->setStretchFactor(1, 1);
    m_bodySplitter->setCollapsible(0, true);     // sidebar collassabile verso sinistra
    m_bodySplitter->setSizes({dpiScale(335), 1});

    /* Trascina il separatore oltre la soglia → nascondi sidebar (come hamburger) */
    connect(m_bodySplitter, &QSplitter::splitterMoved, this, [this](int pos, int) {
        if (m_sidebarWidget && m_sidebarWidget->isVisible() && pos < dpiScale(60))
            m_sidebarWidget->setVisible(false);
    });

    bodyLay->addWidget(m_bodySplitter, 1);
    rootLay->addWidget(body, 1);

    setCentralWidget(root);
}

/* ── Livello 1: status bar con barra progresso pipeline + zoom ───── */
void MainWindow::setupStatusBar()
{
    m_statusProgress = new QProgressBar(this);
    m_statusProgress->setRange(0, 100);
    m_statusProgress->setValue(0);
    m_statusProgress->setFixedWidth(dpiScale(220));
    m_statusProgress->setFixedHeight(dpiScale(14));
    m_statusProgress->setTextVisible(true);
    m_statusProgress->setFormat("");
    m_statusProgress->setObjectName("statusProgress");
    m_statusProgress->setVisible(false);
    statusBar()->addPermanentWidget(m_statusProgress);

    /* Label download LLM — sempre nella status bar, nascosta finché non inizia un download */
    m_dlStatusLbl = new QLabel(this);
    m_dlStatusLbl->setObjectName("dlStatusLbl");
    m_dlStatusLbl->setVisible(false);
    statusBar()->addWidget(m_dlStatusLbl);

    /* ── Pulsanti Zoom +/- (basso destra) ── */
    auto* zoomBar = new QWidget(this);
    zoomBar->setObjectName("zoomBar");
    auto* zoomLay = new QHBoxLayout(zoomBar);
    zoomLay->setContentsMargins(4, 0, 4, 0);
    zoomLay->setSpacing(4);

    auto* zoomMinusBtn = new QPushButton("\xe2\x88\x92", zoomBar);  /* − */
    zoomMinusBtn->setObjectName("zoomBtn");
    zoomMinusBtn->setFixedSize(dpiSize(26, 22));
    zoomMinusBtn->setToolTip(tr("Riduci testo (minimo 50%)"));

    m_zoomPctLbl = new QLabel("100%", zoomBar);
    m_zoomPctLbl->setObjectName("zoomBarLabel");
    m_zoomPctLbl->setFixedWidth(dpiScale(44));
    m_zoomPctLbl->setAlignment(Qt::AlignCenter);

    auto* zoomPlusBtn = new QPushButton("+", zoomBar);
    zoomPlusBtn->setObjectName("zoomBtn");
    zoomPlusBtn->setFixedSize(dpiSize(26, 22));
    zoomPlusBtn->setToolTip(tr("Aumenta testo (massimo 200%)"));

    auto* zoomResetBtn = new QPushButton("\xe2\x97\x8f", zoomBar);  /* ● */
    zoomResetBtn->setObjectName("zoomResetBtn");
    zoomResetBtn->setFixedSize(dpiSize(18, 18));
    zoomResetBtn->setToolTip(tr("Reimposta zoom a 100%"));

    zoomLay->addWidget(zoomMinusBtn);
    zoomLay->addWidget(m_zoomPctLbl);
    zoomLay->addWidget(zoomPlusBtn);
    zoomLay->addWidget(zoomResetBtn);

    /* Carica valore salvato (default 100%) e aggiorna label */
    {
        QSettings s("Prismalux", "GUI");
        m_zoomPct = qBound(50, s.value("ui/zoomPct", 100).toInt(), 200);
    }
    m_zoomPctLbl->setText(QString::number(m_zoomPct) + "%");

    /* Imposta subito lo zoom nel ThemeManager: loadSaved() lo userà */
    ThemeManager::instance()->setZoomScale(m_zoomPct / 100.0);

    /* Timer debounce: riapplica il tema 200ms dopo l'ultimo click +/- */
    m_zoomDebounce = new QTimer(this);
    m_zoomDebounce->setSingleShot(true);
    m_zoomDebounce->setInterval(200);

    statusBar()->addPermanentWidget(zoomBar);

    connect(zoomMinusBtn, &QPushButton::clicked,
            this, &MainWindow::onZoomMinusBtnClicked);
    connect(zoomPlusBtn, &QPushButton::clicked,
            this, &MainWindow::onZoomPlusBtnClicked);
    connect(zoomResetBtn, &QPushButton::clicked,
            this, &MainWindow::onZoomResetBtnClicked);
    connect(m_zoomDebounce, &QTimer::timeout,
            this, &MainWindow::onZoomApplyDebounced);

    statusBar()->showMessage("\xf0\x9f\x8d\xba  Invocazione riuscita. Gli dei ascoltano.");
}

/* ── Livello 1: preset RAM primo avvio + zRAM ────────────────────── */
void MainWindow::setupAutoOptimizations()
{
    if (!QFile::exists(AiChatParams::filePath())) {
        AiChatParams p = AiChatParams::load();
        const qint64 ramMb = P::totalRamBytes() / (1024LL * 1024LL);
        if (ramMb > 0 && ramMb < 10000) {
            p.num_ctx     = 4096;
            p.num_predict = 1024;
            p.temperature = 0.1;
            statusBar()->showMessage(
                "\xf0\x9f\x8e\x9b  Preset 8 GB RAM applicato automaticamente.", 5000);
        } else if (ramMb >= 16000) {
            p.num_ctx = 16384;
            statusBar()->showMessage(
                "\xf0\x9f\x8e\x9b  Preset Contesto Lungo applicato automaticamente.", 5000);
        }
        AiChatParams::save(p);
        if (m_ai) m_ai->setChatParams(p);
    }

#ifndef Q_OS_WIN
    QTimer::singleShot(3000, this, &MainWindow::onZramSetupTimer);
#endif
}

/* ── Livello 1: timer idle-unload, wizard primo avvio, whisper ───── */
void MainWindow::setupTimers()
{
    /* Timer auto-scarico modello ogni 90s */
    m_idleUnloadTimer = new QTimer(this);
    m_idleUnloadTimer->setInterval(90'000);
    connect(m_idleUnloadTimer, &QTimer::timeout, this, &MainWindow::onIdleUnloadTimer);
    m_idleUnloadTimer->start();

    /* Wizard primo avvio — mostrato una sola volta */
    QSettings ss("Prismalux", "GUI");
    if (!ss.value(P::SK::kSetupDone, false).toBool())
        QTimer::singleShot(800, this, &MainWindow::showOnboardingWizard);

    /* Auto-setup whisper.cpp in background */
    QTimer::singleShot(1500, this, &MainWindow::onStartWhisperTimer);

    /* Auto-indicizza RAG (incluso Matematica.pdf via OCR) se l'indice è vuoto */
    QTimer::singleShot(6000, this, &MainWindow::onAutoRagIndex);

    /* Pre-build tab lazy in background: evita freeze al primo clic.
       Strumenti[1]/Programmazione[3] per primi — sono "contenitori" per Ricerca
       e DevAgent+Security, richiesti dalle tab successive nella coda.
       Tab [5] (Utility) per ultimo — costruisce 4 pagine + SQLite.
       Il singleShot(0) esterno fa partire i ritardi 400/700ms solo quando
       l'event loop è realmente idle (dopo show()): schedulati direttamente
       qui, a metà costruttore, il tempo di conteggio verrebbe "mangiato"
       dal lavoro sincrono ancora davanti (setupBackend(), hw->start()...),
       facendoli scattare quasi subito e competere con la primissima
       mappatura della finestra — sintomo: la finestra sembra minimizzarsi
       un istante dopo essere apparsa. */
    QTimer::singleShot(0, this, [this]{
        QTimer::singleShot(400,  this, &MainWindow::onPreBuildTab1);
        QTimer::singleShot(700,  this, &MainWindow::onPreBuildTab3);
        QTimer::singleShot(2500, this, &MainWindow::onPreBuildTab2);
        QTimer::singleShot(3700, this, &MainWindow::onPreBuildTab4);
        QTimer::singleShot(4900, this, &MainWindow::onPreBuildTab6);
        QTimer::singleShot(5500, this, &MainWindow::onPreBuildTab7);
        QTimer::singleShot(8000, this, &MainWindow::onPreBuildTab5);
    });

    /* Controlla aggiornamenti GitHub 10s dopo l'avvio */
    QTimer::singleShot(10000, this, &MainWindow::checkForUpdates);

    /* Avviso cartella RAG mancante — visibile 2s dopo l'avvio */
    QTimer::singleShot(2000, this, [this] {
        const QString ragDir = AppConfig::s().value(
            PrismaluxPaths::SK::kRagDocsDir, "").toString().trimmed();
        if (!ragDir.isEmpty() && QDir(ragDir).exists()) return;
        auto* bar = statusBar();
        auto* lbl = new QLabel(this);
        lbl->setObjectName("ragWarnStatusLbl");
        lbl->setText(
            "\xf0\x9f\x93\x82  Cartella RAG non configurata &mdash; "
            "<a href='openrag'>Imposta in Impostazioni &rarr; RAG</a>");
        lbl->setTextFormat(Qt::RichText);
        lbl->setOpenExternalLinks(false);
        connect(lbl, &QLabel::linkActivated, this, [this](const QString&) {
            openSettingsDialog();
            if (m_impPage) m_impPage->switchToTab("RAG");
        });
        bar->addWidget(lbl);
        /* Scompare automaticamente dopo 20s o quando l'utente configura il path */
        QTimer::singleShot(20000, lbl, &QLabel::hide);
    });
}

/* ── Livello 1: backend Ollama, modelli iniziali, tema ───────────── */
void MainWindow::setupBackend()
{
    {
        QSettings s("Prismalux", "GUI");
        const QString savedModel = s.value(P::SK::kActiveModel, "").toString();
        m_ai->setBackend(AiClient::Ollama, P::kLocalHost, P::kOllamaPort, savedModel);

        /* Migrazione una-tantum: se c'è ancora la key in QSettings, spostala
           nel file 0600 e rimuovila da QSettings. */
        if (s.contains(P::SK::kCloudApiKey)) {
            const QString legacyKey = s.value(P::SK::kCloudApiKey).toString();
            if (!legacyKey.isEmpty() && LanServer::loadSecret(QStringLiteral("cloud_api_key")).isEmpty())
                LanServer::saveSecret(QStringLiteral("cloud_api_key"), legacyKey);
            s.remove(P::SK::kCloudApiKey);
        }
        /* Carica configurazione Smart Router all'avvio.
           API key via QKeychain (o file 0600 fallback) — non da QSettings. */
        m_ai->setSmartRouter(
            s.value(P::SK::kSmartRouterEnabled, false).toBool(),
            s.value(P::SK::kCloudApiUrl, "").toString(),
            s.value(P::SK::kCloudApiModel, "gpt-4o-mini").toString(),
            LanServer::loadSecret(QStringLiteral("cloud_api_key")));
    }
    /* Invalida la cache modelli: il primo fetch interroga sempre Ollama live.
       Questo garantisce che su una macchina diversa non venga mai mostrata
       la lista modelli della macchina su cui è stato compilato il binario. */
    m_ai->invalidateModelCache();
    m_lblModel->setText(tr("(interrogo Ollama...)"));
    m_ai->fetchModels();
    connect(m_ai, &AiClient::modelsReady,   this, &MainWindow::onInitialModelsReady);
    connect(m_ai, &AiClient::modelChanged,  this, &MainWindow::onModelChanged);

    /* TTFT tracking nell'header */
    connect(m_ai, &AiClient::requestStarted, this, &MainWindow::onTtftRequestStarted);
    connect(m_ai, &AiClient::token,          this, &MainWindow::onTtftToken);
    connect(m_ai, &AiClient::aborted,        this, [this]() {
        if (m_ttftLbl) m_ttftLbl->setVisible(false);
    });

    ThemeManager::instance()->loadSaved();
    buildSearchIndex();
    navigateTo(0);
}

/* ── Livello 1: scorciatoie da tastiera Alt+1…7 ──────────────────── */
void MainWindow::setupShortcuts()
{
    auto* sc1 = new QShortcut(QKeySequence("Alt+1"), this);
    auto* sc2 = new QShortcut(QKeySequence("Alt+2"), this);
    auto* sc3 = new QShortcut(QKeySequence("Alt+3"), this);
    auto* sc4 = new QShortcut(QKeySequence("Alt+4"), this);
    auto* sc5 = new QShortcut(QKeySequence("Alt+5"), this);
    auto* sc6 = new QShortcut(QKeySequence("Alt+6"), this);
    auto* sc7 = new QShortcut(QKeySequence("Alt+7"), this);
    connect(sc1, &QShortcut::activated, this, &MainWindow::onShortcutAlt1);
    connect(sc2, &QShortcut::activated, this, &MainWindow::onShortcutAlt2);
    connect(sc3, &QShortcut::activated, this, &MainWindow::onShortcutAlt3);
    connect(sc4, &QShortcut::activated, this, &MainWindow::onShortcutAlt4);
    connect(sc5, &QShortcut::activated, this, &MainWindow::onShortcutAlt5);
    connect(sc6, &QShortcut::activated, this, &MainWindow::onShortcutAlt6);
    connect(sc7, &QShortcut::activated, this, &MainWindow::onShortcutAlt7);
}

/* ── Livello 1: ripristina geometry e state dell'ultima sessione ──── */
void MainWindow::restoreWindowState()
{
    QSettings s("Prismalux", "GUI");
    const QByteArray geo   = s.value("mainwindow/geometry").toByteArray();
    const QByteArray state = s.value("mainwindow/state").toByteArray();
    if (!geo.isEmpty())   restoreGeometry(geo);
    if (!state.isEmpty()) restoreState(state);
}
