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
#include "pages/impara_page.h"
#include "pages/ricerca_page.h"
#include "pages/matematica_page.h"
#include "pages/sintetizzatore_page.h"
#include "pages/assistente_page.h"

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

    /* Impara con AI — Tutor + Quiz */
    m_imparaPage = new ImparaPage(m_ai, this);
    m_stack->addWidget(m_imparaPage);   // indice 11

    /* Ricerca & Sviluppo — paper, brevetti, doc tecnico */
    m_ricercaPage = new RicercaPage(m_ai, this);
    m_stack->addWidget(m_ricercaPage);  // indice 12

    /* Laboratorio Matematico */
    m_matematicaPage = new MatematicaPage(m_ai, this);
    m_stack->addWidget(m_matematicaPage); // indice 13

    /* Sintetizzatore Toni */
    m_sintetizzatorePage = new SintetizzatorePage(this);
    m_stack->addWidget(m_sintetizzatorePage); // indice 14

    /* Assistente AI a categorie (Studio/Scrittura/Ricerca/Libri/Produttività/Documenti) */
    m_assistentePage = new AssistentePage(m_ai, this);
    m_stack->addWidget(m_assistentePage); // indice 15

    auto* central = new QWidget(this);

#ifdef PRISMALUX_FORM_FACTOR_TABLET
    /* ── Layout TABLET: NavRail fisso a sinistra + stack a destra ── */
    auto* hbox = new QHBoxLayout(central);
    hbox->setContentsMargins(0, 0, 0, 0);
    hbox->setSpacing(0);
    buildNavRail();
    hbox->addWidget(m_navRail);
    hbox->addWidget(m_stack, 1);
    setCentralWidget(central);
#else
    /* ── Layout SMARTPHONE: header bar in alto + stack sotto ── */
    auto* vbox    = new QVBoxLayout(central);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(0);
    buildHeaderBar();
    vbox->addWidget(m_headerBar);
    vbox->addWidget(m_stack);
    setCentralWidget(central);

    buildDrawer();
#endif

    connect(m_studioPage, &StudioPage::quizFullscreen,
            this, &MainWindow::onQuizFullscreen);

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

    auto* menuBtn = new HamburgerButton(m_headerBar);
    menuBtn->setFixedSize(kHeaderHeight, kHeaderHeight);
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
        { "\xf0\x9f\xa4\x96",              "Chat",               m_idxChat         },
        { "\xf0\x9f\x9b\xa0\xef\xb8\x8f", "Assistente AI",      m_idxAssistente   },
        { "\xf0\x9f\x93\x9a",              "Studia",             m_idxStudio     },
        { "\xf0\x9f\xa7\xa0",              "Impara con AI",      m_idxImpara     },
        { "\xcf\x80",                       "Matematica",         m_idxMatematica },
        { "\xf0\x9f\x94\xac",              "Ricerca e Sviluppo", m_idxRicerca    },
        { "\xf0\x9f\x92\xbc",              "Lavoro",             m_idxLavoro     },
        { "\xf0\x9f\x93\xa1",              "OBS Control",        m_idxObs        },
        { "\xf0\x9f\x93\x90",              "Misure",             m_idxMisure     },
        { "\xf0\x9f\x93\xb7",              "Camera",             m_idxCamera     },
        { "\xf0\x9f\x94\x8c",              "MCP Add-ons",        m_idxMcp        },
        { "\xf0\x9f\x94\x8b",              "Bluetooth",          m_idxBle        },
        { "\xf0\x9f\x8e\x99\xef\xb8\x8f", "Trascrizione Audio",  m_idxAudio          },
        { "\xf0\x9f\x94\x8a",             "Sintetizzatore",      m_idxSintetizzatore },
        { "\xe2\x9a\x99\xef\xb8\x8f",     "Impostazioni",        m_idxSettings       },
        { "\xf0\x9f\x8d\xba",             "Informazioni",        m_idxInfo           },
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
   buildNavRail — [TABLET] barra laterale fissa 240 px con pulsanti
   per ogni pagina (icona + testo), connessi con slot nominati.
   ══════════════════════════════════════════════════════════════ */
#ifdef PRISMALUX_FORM_FACTOR_TABLET
void MainWindow::buildNavRail()
{
    m_navRail = new QWidget(this);
    m_navRail->setObjectName("NavRail");
    m_navRail->setFixedWidth(240);

    m_navLayout = new QVBoxLayout(m_navRail);
    m_navLayout->setContentsMargins(4, 12, 4, 12);
    m_navLayout->setSpacing(2);

    /* Logo + titolo in cima al rail */
    auto* logoRow   = new QWidget(m_navRail);
    auto* logoHBox  = new QHBoxLayout(logoRow);
    logoHBox->setContentsMargins(8, 0, 8, 0);
    logoHBox->setSpacing(8);
    auto* logoLbl = new QLabel(logoRow);
    {
        QPixmap pm(":/images/prismalux_logo.png");
        logoLbl->setPixmap(pm.scaled(32, 32,
            Qt::KeepAspectRatio, Qt::SmoothTransformation));
        logoLbl->setFixedSize(32, 32);
    }
    logoHBox->addWidget(logoLbl);
    auto* appLbl = new QLabel("Prismalux", logoRow);
    {
        QFont f = appLbl->font();
        f.setPointSize(14); f.setBold(true);
        appLbl->setFont(f);
    }
    logoHBox->addWidget(appLbl, 1);
    logoRow->setFixedHeight(48);
    m_navLayout->addWidget(logoRow);

    /* Separatore */
    auto* sep = new QFrame(m_navRail);
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Sunken);
    m_navLayout->addWidget(sep);
    m_navLayout->addSpacing(4);

    struct NavRailItem { const char* icon; const char* label; };
    /* Ordine deve corrispondere agli indici m_idx* */
    static const NavRailItem kItems[15] = {
        { "\xf0\x9f\xa4\x96",              "Chat"               }, /*  0 */
        { "\xf0\x9f\x93\x9a",              "Studia"             }, /*  1 */
        { "\xf0\x9f\x92\xbc",              "Lavoro"             }, /*  2 */
        { "\xf0\x9f\x93\xa1",              "OBS Control"        }, /*  3 */
        { "\xf0\x9f\x93\x90",              "Misure"             }, /*  4 */
        { "\xf0\x9f\x93\xb7",              "Camera"             }, /*  5 */
        { "\xf0\x9f\x94\x8c",              "MCP Add-ons"        }, /*  6 */
        { "\xf0\x9f\x94\x8b",              "Bluetooth"          }, /*  7 */
        { "\xf0\x9f\x8e\x99\xef\xb8\x8f", "Trascrizione Audio" }, /*  8 */
        { "\xe2\x9a\x99\xef\xb8\x8f",     "Impostazioni"       }, /*  9 */
        { "\xf0\x9f\x8d\xba",             "Informazioni"        }, /* 10 */
        { "\xf0\x9f\xa7\xa0",              "Impara con AI"      }, /* 11 */
        { "\xf0\x9f\x94\xac",              "Ricerca e Sviluppo" }, /* 12 */
        { "\xcf\x80",                       "Matematica"         }, /* 13 */
        { "\xf0\x9f\x94\x8a",             "Sintetizzatore"     }, /* 14 */
    };

    /* Array di puntatori ai slot — stessa posizione dell'indice pagina */
    using SlotPtr = void (MainWindow::*)();
    static const SlotPtr kSlots[15] = {
        &MainWindow::onNavTablet_0,
        &MainWindow::onNavTablet_1,
        &MainWindow::onNavTablet_2,
        &MainWindow::onNavTablet_3,
        &MainWindow::onNavTablet_4,
        &MainWindow::onNavTablet_5,
        &MainWindow::onNavTablet_6,
        &MainWindow::onNavTablet_7,
        &MainWindow::onNavTablet_8,
        &MainWindow::onNavTablet_9,
        &MainWindow::onNavTablet_10,
        &MainWindow::onNavTablet_11,
        &MainWindow::onNavTablet_12,
        &MainWindow::onNavTablet_13,
        &MainWindow::onNavTablet_14,
    };

    for (int i = 0; i < 15; ++i) {
        auto* btn = new QPushButton(m_navRail);
        btn->setObjectName("NavRailBtn");
        btn->setText("  " + QString::fromUtf8(kItems[i].icon)
                     + "  " + kItems[i].label);
        btn->setMinimumHeight(48);
        btn->setFlat(true);
        btn->setCheckable(true);
        connect(btn, &QPushButton::clicked, this, kSlots[i]);
        m_navLayout->addWidget(btn);
    }
    m_navLayout->addStretch();
}

/* ── Slot tablet: uno per pagina (regola no-lambda) ── */
void MainWindow::onNavTablet_0()  { onTabChanged(m_idxChat);           }
void MainWindow::onNavTablet_1()  { onTabChanged(m_idxStudio);         }
void MainWindow::onNavTablet_2()  { onTabChanged(m_idxLavoro);         }
void MainWindow::onNavTablet_3()  { onTabChanged(m_idxObs);            }
void MainWindow::onNavTablet_4()  { onTabChanged(m_idxMisure);         }
void MainWindow::onNavTablet_5()  { onTabChanged(m_idxCamera);         }
void MainWindow::onNavTablet_6()  { onTabChanged(m_idxMcp);            }
void MainWindow::onNavTablet_7()  { onTabChanged(m_idxBle);            }
void MainWindow::onNavTablet_8()  { onTabChanged(m_idxAudio);          }
void MainWindow::onNavTablet_9()  { onTabChanged(m_idxSettings);       }
void MainWindow::onNavTablet_10() { onTabChanged(m_idxInfo);           }
void MainWindow::onNavTablet_11() { onTabChanged(m_idxImpara);         }
void MainWindow::onNavTablet_12() { onTabChanged(m_idxRicerca);        }
void MainWindow::onNavTablet_13() { onTabChanged(m_idxMatematica);     }
void MainWindow::onNavTablet_14() { onTabChanged(m_idxSintetizzatore); }
#endif /* PRISMALUX_FORM_FACTOR_TABLET */

/* ══════════════════════════════════════════════════════════════
   updateDrawerGeometry — posiziona cassetto e overlay
   rispetto all'area sotto la header bar.
   Solo per SMARTPHONE: il tablet non ha drawer.
   ══════════════════════════════════════════════════════════════ */
void MainWindow::updateDrawerGeometry()
{
#ifndef PRISMALUX_FORM_FACTOR_TABLET
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
#endif
}

void MainWindow::resizeEvent(QResizeEvent* e)
{
    QMainWindow::resizeEvent(e);
#ifndef PRISMALUX_FORM_FACTOR_TABLET
    updateDrawerGeometry();
#endif
}

bool MainWindow::eventFilter(QObject* obj, QEvent* ev)
{
#ifndef PRISMALUX_FORM_FACTOR_TABLET
    if (obj == m_overlay && ev->type() == QEvent::MouseButtonPress) {
        if (m_drawerOpen) onToggleDrawer();
        return true;
    }
#else
    Q_UNUSED(obj);
    Q_UNUSED(ev);
#endif
    return QMainWindow::eventFilter(obj, ev);
}

void MainWindow::onToggleDrawer()
{
#ifndef PRISMALUX_FORM_FACTOR_TABLET
    m_drawerOpen = !m_drawerOpen;
    if (m_drawerOpen) {
        updateDrawerGeometry();
        m_overlay->raise();
        m_drawer->raise();
    }
    m_drawer->setVisible(m_drawerOpen);
    if (m_overlay) m_overlay->setVisible(m_drawerOpen);
#endif
}

void MainWindow::onDrawerNavClicked()
{
#ifndef PRISMALUX_FORM_FACTOR_TABLET
    auto* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    const int idx = btn->property("pageIndex").toInt();
    if (m_drawerOpen) onToggleDrawer();
    onTabChanged(idx);
#endif
}

/* ══════════════════════════════════════════════════════════════
   onTabChanged
   ══════════════════════════════════════════════════════════════ */
void MainWindow::onTabChanged(int index)
{
    m_stack->setCurrentIndex(index);

    /* Aggiorna il titolo nella header bar */
    static const struct { int idx; const char* name; } kTitles[] = {
        {  0, "Chat"               }, {  1, "Studia"             },
        {  2, "Lavoro"             }, {  3, "OBS Control"        },
        {  4, "Misure"             }, {  5, "Camera"             },
        {  6, "MCP Add-ons"        }, {  7, "Bluetooth"          },
        {  8, "Trascrizione Audio" }, {  9, "Impostazioni"       },
        { 10, "Informazioni"       }, { 11, "Impara con AI"      },
        { 12, "Ricerca e Sviluppo" }, { 13, "Matematica"         },
        { 14, "Sintetizzatore"     },
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
   onQuizFullscreen — nasconde/mostra l'header bar durante il quiz
   per dare alla pagina quiz uno spazio a tutto schermo pulito.
   Sul tablet nasconde il NavRail; sullo smartphone nasconde la header bar.
   ══════════════════════════════════════════════════════════════ */
void MainWindow::onQuizFullscreen(bool on)
{
#ifdef PRISMALUX_FORM_FACTOR_TABLET
    if (m_navRail)
        m_navRail->setVisible(!on);
#else
    if (m_headerBar)
        m_headerBar->setVisible(!on);
#endif
}
