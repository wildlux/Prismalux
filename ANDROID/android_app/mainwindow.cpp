#include "mainwindow.h"
#include "ai_client.h"
#include "rag_engine_simple.h"
#include "pages/chat_page.h"
#include "pages/camera_page.h"
#include "pages/ble_page.h"
#include "pages/settings_page.h"

#include <QVBoxLayout>
#include <QToolButton>
#include <QSettings>
#include <QScreen>

#ifdef Q_OS_ANDROID
#include <QCoreApplication>
#include <QtCore/private/qandroidextras_p.h>
#endif

/* ── Altezza bottom bar (px fisici — scalata da Qt) ── */
static constexpr int kBarHeight = 56;

/* ══════════════════════════════════════════════════════════════
   Costruttore
   ══════════════════════════════════════════════════════════════ */
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("PrismaluxMobile");
    setObjectName("MainWindow");

    /* ── Risorsa condivisa: AI client ── */
    m_ai = new AiClient(this);

    /* ── Carica configurazione salvata (IP, porta, modello) ── */
    QSettings s("Prismalux", "Mobile");
    const QString host  = s.value("server/host",  "192.168.1.100").toString();
    const int     port  = s.value("server/port",  11434).toInt();
    const QString model = s.value("server/model", "llama3.2:1b").toString();
    m_ai->setServer(host, port, model);

    /* ── Risorsa condivisa: motore RAG leggero ── */
    m_rag = new RagEngineSimple(this);
    const QString ragPath = s.value("rag/indexPath", "").toString();
    if (!ragPath.isEmpty()) m_rag->load(ragPath);

    /* ── Stack di pagine ── */
    m_stack = new QStackedWidget(this);

    m_chatPage     = new ChatPage(m_ai, m_rag, this);
    m_cameraPage   = new CameraPage(m_ai, this);
    m_blePage      = new BlePage(this);
    m_settingsPage = new SettingsPage(m_ai, m_rag, this);

    m_stack->addWidget(m_chatPage);     // indice 0
    m_stack->addWidget(m_cameraPage);   // indice 1
    m_stack->addWidget(m_blePage);      // indice 2
    m_stack->addWidget(m_settingsPage); // indice 3

    /* ── Layout centrale ── */
    auto* central = new QWidget(this);
    auto* vbox    = new QVBoxLayout(central);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(0);
    vbox->addWidget(m_stack);
    setCentralWidget(central);

    /* ── Bottom navigation bar ── */
    buildBottomBar();

    /* ── Propagazione eventi tra pagine ── */
    connect(m_settingsPage, &SettingsPage::serverChanged,
            m_ai, &AiClient::setServer);
    connect(m_settingsPage, &SettingsPage::ragIndexChanged,
            m_rag, [this](const QString& path) {
                m_rag->load(path);
                m_chatPage->onRagReloaded();
            });
    connect(m_cameraPage, &CameraPage::textExtracted,
            m_chatPage, &ChatPage::prependContext);

    /* ── Permessi Android ── */
    applyPermissions();

    /* ── Dimensioni: full-screen su mobile ── */
    if (QGuiApplication::screens().first()->size().width() < 800)
        showMaximized();
    else
        resize(420, 820);
}

/* ══════════════════════════════════════════════════════════════
   buildBottomBar — QToolBar in basso con 4 azioni
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
        { "\xf0\x9f\xa4\x96", "Chat",     0 },  // 🤖
        { "\xf0\x9f\x93\xb7", "Camera",   1 },  // 📷
        { "\xf0\x9f\x94\x8b", "Bluetooth",2 },  // 🔋
        { "\xe2\x9a\x99",     "Impost.",  3 },  // ⚙
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
    /* Ferma la camera se usciamo dalla pagina Camera */
    if (index != 1) m_cameraPage->stopCamera();
    else            m_cameraPage->startCamera();
    /* Ferma il BLE scan se usciamo */
    if (index != 2) m_blePage->stopScan();
}

/* ══════════════════════════════════════════════════════════════
   applyPermissions — richiesta runtime su Android
   Su desktop non fa nulla.
   ══════════════════════════════════════════════════════════════ */
void MainWindow::applyPermissions()
{
#ifdef Q_OS_ANDROID
    /* Qt6.5+: QCoreApplication::requestPermission() */
    const QStringList perms = {
        "android.permission.CAMERA",
        "android.permission.BLUETOOTH_SCAN",
        "android.permission.BLUETOOTH_CONNECT",
        "android.permission.READ_MEDIA_DOCUMENTS",
    };
    for (const QString& p : perms) {
        QCoreApplication::requestPermission(p).then([p](QPermission perm) {
            if (perm.status() != Qt::PermissionStatus::Granted)
                qWarning("[Permissions] Non concesso: %s", p.toUtf8().constData());
        });
    }
#endif
}
