#include "mainwindow.h"
#include "ai_client.h"
#include "local_llm_client.h"
#include "rag_engine_simple.h"
#include "pages/chat_page.h"
#include "pages/studio_page.h"
#include "pages/lavoro_page.h"
#include "pages/obs_page.h"
#include "pages/misure_page.h"
#include "pages/settings_page.h"
#include "pages/info_page.h"
#include "pages/mcpaddons_page.h"
#include "pages/audio_page.h"

#ifdef HAVE_MULTIMEDIA
#include "pages/camera_page.h"
#endif
#ifdef HAVE_BLE
#include "pages/ble_page.h"
#endif

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSizePolicy>
#include <QFont>
#include <QToolButton>
#include <QSettings>
#include <QScreen>
#include <QEvent>
#include <QResizeEvent>
#include <QPixmap>

static constexpr int kHeaderHeight = 52;

/* ── Widget stub per funzionalità non disponibili ── */
static QWidget* makeStubPage(const QString& icon, const QString& text, QWidget* parent)
{
    auto* w   = new QWidget(parent);
    auto* lbl = new QLabel(icon + "\n\n" + text, w);
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setWordWrap(true);
    auto* vb  = new QVBoxLayout(w);
    vb->addStretch();
    vb->addWidget(lbl);
    vb->addStretch();
    return w;
}

/* ══════════════════════════════════════════════════════════════
   Costruttore
   ══════════════════════════════════════════════════════════════ */
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("PrismaluxMobile");
    setObjectName("MainWindow");

    m_ai = new AiClient(this);

    /* LLM locale — creato sempre; il modello è opzionale (download su richiesta) */
    m_localLlm = new LocalLlmClient(this);
    m_ai->setLocalLlm(m_localLlm);
    /* Auto-carica il modello se già scaricato in sessioni precedenti */
    if (LocalLlmClient::modelExists())
        m_localLlm->loadModel(LocalLlmClient::defaultModelPath());

    QSettings s("Prismalux", "Mobile");
    const QString host  = s.value("server/host",  "192.168.1.165").toString();
    const int     port  = s.value("server/port",  11434).toInt();
    const QString model = s.value("server/model", "llama3.2:3b").toString();
    const QString token = s.value("server/token", "").toString();
    m_ai->setServer(host, port, model);
    if (!token.isEmpty()) m_ai->setToken(token);

    m_rag = new RagEngineSimple(this);
    const QString ragPath = s.value("rag/indexPath", "").toString();
    if (!ragPath.isEmpty()) m_rag->load(ragPath);

    m_stack = new QStackedWidget(this);

    /* Chat — sempre disponibile */
    m_chatPage = new ChatPage(m_ai, m_rag, this);
    m_stack->addWidget(m_chatPage);     // indice 0

    /* Studio AI — CCNA, Matematica, Python, ecc. */
    m_studioPage = new StudioPage(m_ai, this);
    m_stack->addWidget(m_studioPage);   // indice 1

    /* Lavoro AI */
    m_lavoroPage = new LavoroPage(m_ai, this);
    m_stack->addWidget(m_lavoroPage);   // indice 2

    /* OBS Control */
    m_obsPage = new ObsPage(this);
    m_stack->addWidget(m_obsPage);      // indice 3

    /* Misure & Fotogrammetria */
    m_misurePage = new MisurePage(m_ai, this);
    m_stack->addWidget(m_misurePage);   // indice 4

    /* Camera */
#ifdef HAVE_MULTIMEDIA
    m_cameraPage = new CameraPage(m_ai, this);
#else
    m_cameraPage = makeStubPage(
        "\xf0\x9f\x93\xb7",
        "Camera non disponibile\nin questa versione.",
        this);
#endif
    m_stack->addWidget(m_cameraPage);   // indice 5

    /* MCP Add-ons */
    m_mcpPage = new McpAddonsPage(this);
    m_stack->addWidget(m_mcpPage);      // indice 6

    /* BLE */
#ifdef HAVE_BLE
    m_blePage = new BlePage(this);
#else
    m_blePage = makeStubPage(
        "\xf0\x9f\x94\x8b",
        "Bluetooth non disponibile\nin questa versione.",
        this);
#endif
    m_stack->addWidget(m_blePage);      // indice 7

    /* Audio — registrazione e trascrizione */
    m_audioPage = new AudioPage(m_ai, this);
    m_stack->addWidget(m_audioPage);    // indice 8

    /* Impostazioni — sempre disponibile */
    m_settingsPage = new SettingsPage(m_ai, m_rag, m_localLlm, this);
    m_stack->addWidget(m_settingsPage); // indice 9

    /* Informazioni — guida e crediti */
    m_infoPage = new InfoPage(this);
    m_stack->addWidget(m_infoPage);     // indice 10

    auto* central = new QWidget(this);
    auto* vbox    = new QVBoxLayout(central);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(0);
    buildHeaderBar();
    vbox->addWidget(m_headerBar);
    vbox->addWidget(m_stack);
    setCentralWidget(central);

    buildDrawer();

    connect(m_settingsPage, &SettingsPage::serverChanged,
            m_ai, qOverload<const QString&, int, const QString&>(&AiClient::setServer));
    connect(m_settingsPage, &SettingsPage::ragIndexChanged,
            m_rag, [this](const QString& path) {
                m_rag->load(path);
                m_chatPage->onRagReloaded();
            });

#ifdef HAVE_MULTIMEDIA
    connect(m_cameraPage, &CameraPage::textExtracted,
            m_chatPage, &ChatPage::prependContext);
#endif
    connect(m_audioPage, &AudioPage::transcriptionReady,
            m_chatPage,  &ChatPage::prependContext);

    applyPermissions();

    if (QGuiApplication::screens().first()->size().width() < 800)
        showMaximized();
    else
        resize(420, 820);
}

/* ══════════════════════════════════════════════════════════════
   buildHeaderBar — barra superiore con ☰ + titolo pagina.
   ══════════════════════════════════════════════════════════════ */
void MainWindow::buildHeaderBar()
{
    m_headerBar = new QWidget(this);
    m_headerBar->setObjectName("HeaderBar");
    m_headerBar->setFixedHeight(kHeaderHeight);

    auto* hbox = new QHBoxLayout(m_headerBar);
    hbox->setContentsMargins(4, 0, 8, 0);
    hbox->setSpacing(4);

    auto* menuBtn = new QToolButton(m_headerBar);
    menuBtn->setObjectName("HamburgerBtn");
    menuBtn->setFixedSize(kHeaderHeight, kHeaderHeight);
    /* ☰ tre linee orizzontali — sempre visibile anche senza risorse immagine */
    menuBtn->setText(QString::fromUtf8("\xe2\x98\xb0"));   /* ☰ U+2630 */
    {
        QFont mf = menuBtn->font();
        mf.setPointSize(22);
        menuBtn->setFont(mf);
    }
    hbox->addWidget(menuBtn);

    m_titleLbl = new QLabel("PrismaluxMobile", m_headerBar);
    m_titleLbl->setObjectName("HeaderTitle");
    QFont tf = m_titleLbl->font();
    tf.setPointSize(15);
    tf.setBold(true);
    m_titleLbl->setFont(tf);
    hbox->addWidget(m_titleLbl, 1);

    connect(menuBtn, &QToolButton::clicked, this, &MainWindow::onToggleDrawer);
}

/* ══════════════════════════════════════════════════════════════
   buildDrawer — cassetto laterale con tutte le voci di navigazione.
   Sovrapposto al contenuto, visibile solo quando aperto.
   ══════════════════════════════════════════════════════════════ */
void MainWindow::buildDrawer()
{
    /* Overlay scuro (dietro il cassetto, davanti allo stack) */
    m_overlay = new QWidget(centralWidget());
    m_overlay->setObjectName("DrawerOverlay");
    m_overlay->setAutoFillBackground(true);
    QPalette ovPal;
    ovPal.setColor(QPalette::Window, QColor(0, 0, 0, 140));
    m_overlay->setPalette(ovPal);
    m_overlay->setVisible(false);
    m_overlay->installEventFilter(this);

    /* Cassetto vero e proprio */
    m_drawer = new QWidget(centralWidget());
    m_drawer->setObjectName("NavDrawer");
    m_drawer->setVisible(false);

    auto* vbox = new QVBoxLayout(m_drawer);
    vbox->setContentsMargins(0, 0, 0, 12);
    vbox->setSpacing(2);

    /* Intestazione cassetto: logo + titolo */
    auto* hdr    = new QWidget(m_drawer);
    auto* hdrBox = new QHBoxLayout(hdr);
    hdrBox->setContentsMargins(12, 0, 12, 0);
    hdrBox->setSpacing(10);
    auto* logoLbl = new QLabel(hdr);
    {
        QPixmap pm(":/images/prismalux_logo.png");
        logoLbl->setPixmap(pm.scaled(36, 36,
            Qt::KeepAspectRatio, Qt::SmoothTransformation));
        logoLbl->setFixedSize(36, 36);
    }
    hdrBox->addWidget(logoLbl);
    auto* hdrLbl = new QLabel("Prismalux", hdr);
    QFont hf = hdrLbl->font();
    hf.setPointSize(16); hf.setBold(true);
    hdrLbl->setFont(hf);
    hdrBox->addWidget(hdrLbl, 1);
    auto* closeBtn = new QToolButton(hdr);
    closeBtn->setObjectName("HamburgerBtn");
    closeBtn->setText(QString::fromUtf8("\xe2\x86\x90"));  /* ← freccia chiudi */
    {
        QFont cf = closeBtn->font();
        cf.setPointSize(20);
        closeBtn->setFont(cf);
    }
    closeBtn->setFixedSize(44, 44);
    hdrBox->addWidget(closeBtn);
    hdr->setFixedHeight(kHeaderHeight);
    vbox->addWidget(hdr);

    /* Separatore */
    auto* sep = new QFrame(m_drawer);
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Sunken);
    vbox->addWidget(sep);
    vbox->addSpacing(4);

    struct NavItem { const char* icon; const char* label; int idx; };
    const NavItem items[] = {
        { "\xf0\x9f\xa4\x96",           "Chat",           m_idxChat     },
        { "\xf0\x9f\x93\x9a",           "Studia",         m_idxStudio   },
        { "\xf0\x9f\x92\xbc",           "Lavoro",         m_idxLavoro   },
        { "\xf0\x9f\x93\xa1",           "OBS Control",    m_idxObs      },
        { "\xf0\x9f\x93\x90",           "Misure",         m_idxMisure   },
        { "\xf0\x9f\x93\xb7",           "Camera",         m_idxCamera   },
        { "\xf0\x9f\x94\x8c",           "MCP Add-ons",    m_idxMcp      },
        { "\xf0\x9f\x94\x8b",           "Bluetooth",      m_idxBle      },
        { "\xf0\x9f\x8e\x99\xef\xb8\x8f", "Trascrizione Audio", m_idxAudio },
        { "\xe2\x9a\x99\xef\xb8\x8f",   "Impostazioni",   m_idxSettings },
        { "\xf0\x9f\x8d\xba",           "Informazioni",   m_idxInfo     },
    };
    for (const auto& item : items) {
        auto* btn = new QPushButton(
            "  " + QString::fromUtf8(item.icon) + "   " + item.label, m_drawer);
        btn->setObjectName("DrawerNavBtn");
        btn->setMinimumHeight(52);
        btn->setFlat(true);
        btn->setProperty("pageIndex", item.idx);
        connect(btn, &QPushButton::clicked, this, &MainWindow::onDrawerNavClicked);
        vbox->addWidget(btn);
    }
    vbox->addStretch();

    connect(closeBtn, &QToolButton::clicked, this, &MainWindow::onToggleDrawer);
    updateDrawerGeometry();
}

/* ══════════════════════════════════════════════════════════════
   updateDrawerGeometry — posiziona cassetto e overlay
   rispetto all'area sotto la header bar.
   ══════════════════════════════════════════════════════════════ */
void MainWindow::updateDrawerGeometry()
{
    if (!m_drawer || !centralWidget()) return;
    const QRect cr = centralWidget()->rect();
    /* top = 0: il drawer copre tutto dall'alto (Material Design drawer).
       Il drawer ha già il suo header interno "Prismalux | ✕" che
       sostituisce visivamente la header bar mentre è aperto. */
    const int h  = cr.height();
    const int dw = qMin(static_cast<int>(cr.width() * 0.75), 300);
    m_drawer->setGeometry(0, 0, dw, h);
    if (m_overlay)
        m_overlay->setGeometry(dw, 0, cr.width() - dw, h);
}

void MainWindow::resizeEvent(QResizeEvent* e)
{
    QMainWindow::resizeEvent(e);
    updateDrawerGeometry();
}

bool MainWindow::eventFilter(QObject* obj, QEvent* ev)
{
    if (obj == m_overlay && ev->type() == QEvent::MouseButtonPress) {
        if (m_drawerOpen) onToggleDrawer();
        return true;
    }
    return QMainWindow::eventFilter(obj, ev);
}

void MainWindow::onToggleDrawer()
{
    m_drawerOpen = !m_drawerOpen;
    if (m_drawerOpen) {
        updateDrawerGeometry();
        m_overlay->raise();
        m_drawer->raise();
    }
    m_drawer->setVisible(m_drawerOpen);
    if (m_overlay) m_overlay->setVisible(m_drawerOpen);
}

void MainWindow::onDrawerNavClicked()
{
    auto* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    const int idx = btn->property("pageIndex").toInt();
    if (m_drawerOpen) onToggleDrawer();
    onTabChanged(idx);
}

/* ══════════════════════════════════════════════════════════════
   onTabChanged
   ══════════════════════════════════════════════════════════════ */
void MainWindow::onTabChanged(int index)
{
    m_stack->setCurrentIndex(index);

    /* Aggiorna il titolo nella header bar */
    static const struct { int idx; const char* name; } kTitles[] = {
        { 0, "Chat" },     { 1, "Studia" },   { 2, "Lavoro" },
        { 3, "OBS Control" }, { 4, "Misure" }, { 5, "Camera" },
        { 6, "MCP Add-ons" }, { 7, "Bluetooth" }, { 8, "Trascrizione Audio" },
        { 9, "Impostazioni" }, { 10, "Informazioni" },
    };
    if (m_titleLbl) {
        for (const auto& t : kTitles) {
            if (t.idx == index) { m_titleLbl->setText(t.name); break; }
        }
    }

#ifdef HAVE_MULTIMEDIA
    if (index != m_idxCamera) m_cameraPage->stopCamera();
    else                      m_cameraPage->startCamera();
#endif

#ifdef HAVE_BLE
    if (index != m_idxBle) m_blePage->stopScan();
#endif
    Q_UNUSED(index);
}

/* ══════════════════════════════════════════════════════════════
   applyPermissions
   ══════════════════════════════════════════════════════════════ */
void MainWindow::applyPermissions()
{
    /* Permessi runtime dichiarati nel AndroidManifest.xml.
       Camera e BLE sono stub in questa build — aggiungere qui quando abilitati. */
}
