#include "mainwindow.h"
#include "ai_client.h"
#include "rag_engine_simple.h"
#include "pages/chat_page.h"
#include "pages/settings_page.h"

#ifdef HAVE_MULTIMEDIA
#include "pages/camera_page.h"
#endif
#ifdef HAVE_BLE
#include "pages/ble_page.h"
#endif

#include <QVBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QSettings>
#include <QScreen>


static constexpr int kBarHeight = 56;

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

    QSettings s("Prismalux", "Mobile");
    const QString host  = s.value("server/host",  "192.168.1.165").toString();
    const int     port  = s.value("server/port",  11434).toInt();
    const QString model = s.value("server/model", "llama3.2:1b").toString();
    m_ai->setServer(host, port, model);

    m_rag = new RagEngineSimple(this);
    const QString ragPath = s.value("rag/indexPath", "").toString();
    if (!ragPath.isEmpty()) m_rag->load(ragPath);

    m_stack = new QStackedWidget(this);

    /* Chat — sempre disponibile */
    m_chatPage = new ChatPage(m_ai, m_rag, this);
    m_stack->addWidget(m_chatPage);   // indice 0

    /* Camera */
#ifdef HAVE_MULTIMEDIA
    m_cameraPage = new CameraPage(m_ai, this);
#else
    m_cameraPage = makeStubPage(
        "\xf0\x9f\x93\xb7",
        "Camera non disponibile\nin questa versione.",
        this);
#endif
    m_stack->addWidget(m_cameraPage); // indice 1

    /* BLE */
#ifdef HAVE_BLE
    m_blePage = new BlePage(this);
#else
    m_blePage = makeStubPage(
        "\xf0\x9f\x94\x8b",
        "Bluetooth non disponibile\nin questa versione.",
        this);
#endif
    m_stack->addWidget(m_blePage);    // indice 2

    /* Impostazioni — sempre disponibile */
    m_settingsPage = new SettingsPage(m_ai, m_rag, this);
    m_stack->addWidget(m_settingsPage); // indice 3

    auto* central = new QWidget(this);
    auto* vbox    = new QVBoxLayout(central);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(0);
    vbox->addWidget(m_stack);
    setCentralWidget(central);

    buildBottomBar();

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

    applyPermissions();

    if (QGuiApplication::screens().first()->size().width() < 800)
        showMaximized();
    else
        resize(420, 820);
}

/* ══════════════════════════════════════════════════════════════
   buildBottomBar
   ══════════════════════════════════════════════════════════════ */
void MainWindow::buildBottomBar()
{
    m_bottomBar = new QToolBar(this);
    m_bottomBar->setObjectName("BottomBar");
    m_bottomBar->setMovable(false);
    m_bottomBar->setFixedHeight(kBarHeight);
    m_bottomBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    addToolBar(Qt::BottomToolBarArea, m_bottomBar);

    m_tabGroup = new QActionGroup(this);
    m_tabGroup->setExclusive(true);

    struct Tab { const char* icon; const char* label; int idx; };
    const Tab tabs[] = {
        { "\xf0\x9f\xa4\x96", "Chat",      m_idxChat     },
        { "\xf0\x9f\x93\xb7", "Camera",    m_idxCamera   },
        { "\xf0\x9f\x94\x8b", "Bluetooth", m_idxBle      },
        { "\xe2\x9a\x99",     "Impost.",   m_idxSettings },
    };

    for (const auto& t : tabs) {
        auto* act = new QAction(QString::fromUtf8(t.icon) + "\n" + t.label, this);
        act->setCheckable(true);
        act->setData(t.idx);
        m_tabGroup->addAction(act);
        m_bottomBar->addAction(act);
        if (t.idx == 0) act->setChecked(true);
    }

    connect(m_tabGroup, &QActionGroup::triggered, this, [this](QAction* a) {
        onTabChanged(a->data().toInt());
    });
}

/* ══════════════════════════════════════════════════════════════
   onTabChanged
   ══════════════════════════════════════════════════════════════ */
void MainWindow::onTabChanged(int index)
{
    m_stack->setCurrentIndex(index);

#ifdef HAVE_MULTIMEDIA
    if (index != m_idxCamera) m_cameraPage->stopCamera();
    else                      m_cameraPage->startCamera();
#endif

#ifdef HAVE_BLE
    if (index != m_idxBle) m_blePage->stopScan();
#endif
}

/* ══════════════════════════════════════════════════════════════
   applyPermissions
   ══════════════════════════════════════════════════════════════ */
void MainWindow::applyPermissions()
{
    /* Permessi runtime dichiarati nel AndroidManifest.xml.
       Camera e BLE sono stub in questa build — aggiungere qui quando abilitati. */
}
