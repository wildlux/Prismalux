/* ══════════════════════════════════════════════════════════════
   mainwindow_tabs.cpp — MainWindow: sidebar chat + costruzione tab principali
   ============================================================================
   ChatListDelegate (checkbox Gmail-style), sidebar cronologia chat,
   QTabWidget principale, lazy loading dei tab (ensureTabBuilt +
   create*Widget), barra di navigazione a menù orizzontale.
   Split da mainwindow.cpp (TODO D-8).
   ══════════════════════════════════════════════════════════════ */
#include "mainwindow.h"
#include "prismalux_paths.h"
#include "dpi_utils.h"
#include "pages/main_ai.h"
#include "pages/settings_main.h"
#include "pages/main_tools.h"
#include "pages/main_multimedia.h"
#include "pages/main_math.h"
#include "pages/main_utility.h"
#include "pages/main_lan_wan.h"
#include "pages/main_bioinformatica.h"
#include "pages/main_app_controller.h"
#include "pages/main_security.h"
#include "pages/main_programming.h"
#include "pages/main_research.h"

#include <QApplication>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QAbstractItemView>
#include <QAbstractItemModel>
#include <QMouseEvent>
#include <QItemSelectionModel>
#include <QModelIndex>
#include <QRect>
#include <QRectF>
#include <QPen>
#include <QColor>
#include <QPalette>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QCheckBox>
#include <QLineEdit>
#include <QListWidget>
#include <QFrame>
#include <QTabWidget>
#include <QButtonGroup>
#include <QSettings>

namespace P = PrismaluxPaths;

/* ══════════════════════════════════════════════════════════════
   ChatListDelegate — checkbox Gmail-style nella sidebar chat
   ══════════════════════════════════════════════════════════════ */
class ChatListDelegate : public QStyledItemDelegate {
    static constexpr int kCbZone = 26; // px riservati a sinistra per la checkbox
public:
    explicit ChatListDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent) {}

    void paint(QPainter* p, const QStyleOptionViewItem& opt,
               const QModelIndex& idx) const override
    {
        p->save();
        QStyleOptionViewItem o(opt);
        initStyleOption(&o, idx);
        QStyle* sty = o.widget ? o.widget->style() : QApplication::style();

        // 1. Background intera riga (selezione / hover)
        sty->drawPrimitive(QStyle::PE_PanelItemViewItem, &o, p, o.widget);

        // 2. Checkbox a sinistra
        const bool sel = (o.state & QStyle::State_Selected) != 0;
        const bool hov = (o.state & QStyle::State_MouseOver) != 0;
        drawCb(p, o.rect, sel, hov, o.palette);

        // 3. Testo spostato a destra della zona checkbox
        QRect tr = o.rect.adjusted(kCbZone + 2, 0, -4, 0);
        QColor tc = sel ? o.palette.highlightedText().color()
                        : o.palette.text().color();
        p->setPen(tc);
        p->setFont(o.font);
        p->drawText(tr, Qt::AlignVCenter | Qt::AlignLeft,
            o.fontMetrics.elidedText(o.text, Qt::ElideRight, tr.width()));

        p->restore();
    }

    bool editorEvent(QEvent* evt, QAbstractItemModel* /*model*/,
                     const QStyleOptionViewItem& opt,
                     const QModelIndex& idx) override
    {
        if (evt->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(evt);
            if (me->button() == Qt::LeftButton && cbRect(opt.rect).contains(me->pos())) {
                // Click nella zona checkbox: toggle selezione senza aprire la chat
                auto* view = qobject_cast<QAbstractItemView*>(
                    const_cast<QWidget*>(opt.widget));
                if (view) {
                    const bool was = view->selectionModel()->isSelected(idx);
                    view->selectionModel()->select(
                        idx,
                        was ? QItemSelectionModel::Deselect
                            : QItemSelectionModel::Select);
                }
                return true; // evento consumato — itemClicked non parte
            }
        }
        return false;
    }

private:
    QRect cbRect(const QRect& row) const {
        constexpr int sz = 14;
        return {row.left() + (kCbZone - sz) / 2,
                row.center().y() - sz / 2, sz, sz};
    }

    void drawCb(QPainter* p, const QRect& row, bool checked,
                bool hov, const QPalette& pal) const
    {
        QRect r = cbRect(row);
        p->setRenderHint(QPainter::Antialiasing, true);
        if (checked) {
            p->setPen(Qt::NoPen);
            p->setBrush(pal.highlight());
            p->drawRoundedRect(r, 2.5, 2.5);
            // Spunta bianca
            p->setPen(QPen(pal.highlightedText().color(), 1.8f,
                           Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p->setBrush(Qt::NoBrush);
            const float l = r.left(), t = r.top(),
                        w = r.width(), h = r.height();
            QPointF pts[] = {
                {l + w * 0.18f, t + h * 0.52f},
                {l + w * 0.43f, t + h * 0.76f},
                {l + w * 0.83f, t + h * 0.25f},
            };
            p->drawPolyline(pts, 3);
        } else {
            QColor bc = hov ? pal.mid().color().lighter(160)
                            : pal.mid().color();
            p->setPen(QPen(bc, 1.3f));
            p->setBrush(Qt::NoBrush);
            p->drawRoundedRect(QRectF(r), 2.5, 2.5);
        }
    }
};

/* ══════════════════════════════════════════════════════════════
   buildSidebar — colonna sinistra con bottoni navigazione
   ══════════════════════════════════════════════════════════════ */
QWidget* MainWindow::buildSidebar() {
    auto* bar = new QWidget(this);
    bar->setObjectName("sidebar");
    bar->setMinimumWidth(dpiScale(160));

    auto* lay = new QVBoxLayout(bar);
    lay->setContentsMargins(0, 8, 0, 8);
    lay->setSpacing(2);

    /* ── Pulsanti: Nuova chat e Cancella in colonna verticale ── */
    {
        auto* btnRow = new QWidget(bar);
        btnRow->setObjectName("chatBtnRow");
        auto* btnLay = new QVBoxLayout(btnRow);
        btnLay->setContentsMargins(dpiScale(8), 0, dpiScale(8), 0);
        btnLay->setSpacing(dpiScale(4));

        auto* newChatBtn = new QPushButton("\xe2\x9c\x8f\xef\xb8\x8f  Nuova chat", btnRow);
        newChatBtn->setObjectName("actionBtn");
        newChatBtn->setFixedHeight(dpiScale(30));
        newChatBtn->setStyleSheet("text-align: left; padding-left: 8px;");
        newChatBtn->setToolTip("Inizia una nuova conversazione (reset log)");
        connect(newChatBtn, &QPushButton::clicked, this, &MainWindow::onNewChatClicked);
        btnLay->addWidget(newChatBtn);

        m_btnDeleteChats = new QPushButton("\xf0\x9f\x97\x91  Cancella chat", btnRow);
        m_btnDeleteChats->setObjectName("actionBtn");
        m_btnDeleteChats->setProperty("danger", true);
        m_btnDeleteChats->setFixedHeight(dpiScale(30));
        m_btnDeleteChats->setLayoutDirection(Qt::LeftToRight);
        m_btnDeleteChats->setStyleSheet("text-align: left; padding-left: 8px;");
        m_btnDeleteChats->setEnabled(false);
        m_btnDeleteChats->setToolTip("Elimina le chat selezionate (Canc)");
        connect(m_btnDeleteChats, &QPushButton::clicked,
                this, &MainWindow::onDeleteSelectedChatsClicked);
        btnLay->addWidget(m_btnDeleteChats);

        lay->addWidget(btnRow);
    }

    /* ── Riga ricerca: [☐] [🔍 Cerca chat...] ── */
    {
        auto* searchRow = new QWidget(bar);
        searchRow->setObjectName("chatSearchRow");
        auto* searchLay = new QHBoxLayout(searchRow);
        searchLay->setContentsMargins(dpiScale(6), 0, dpiScale(6), 0);
        searchLay->setSpacing(dpiScale(4));

        m_chkSelectAll = new QCheckBox(searchRow);
        m_chkSelectAll->setObjectName("selectAllChk");
        m_chkSelectAll->setToolTip(tr("Seleziona / deseleziona tutte le chat visibili"));
        m_chkSelectAll->setTristate(true);
        searchLay->addWidget(m_chkSelectAll);

        m_chatSearch = new QLineEdit(searchRow);
        m_chatSearch->setPlaceholderText("\xf0\x9f\x94\x8d  Cerca chat...");
        m_chatSearch->setClearButtonEnabled(true);
        m_chatSearch->setObjectName("chatSearchEdit");
        searchLay->addWidget(m_chatSearch, 1);

        lay->addWidget(searchRow);
    }

    /* ── Lista chat history ── */
    m_chatList = new QListWidget(bar);
    m_chatList->setObjectName("chatList");
    m_chatList->setFrameShape(QFrame::NoFrame);
    m_chatList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_chatList->setSpacing(2);
    m_chatList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_chatList->setMouseTracking(true);
    m_chatList->viewport()->setMouseTracking(true);
    m_chatList->setItemDelegate(new ChatListDelegate(m_chatList));
    m_chatList->installEventFilter(this);
    lay->addWidget(m_chatList, 1);   /* stretch=1: occupa lo spazio disponibile */

    /* Filtra la lista in tempo reale */
    connect(m_chatSearch, &QLineEdit::textChanged, this, &MainWindow::onChatSearchChanged);
    connect(m_chatList, &QListWidget::itemClicked, this, &MainWindow::onChatItemClicked);
    connect(m_chatList, &QListWidget::itemSelectionChanged,
            this, &MainWindow::onChatSelectionChanged);

    /* Checkbox "Seleziona tutto" */
    connect(m_chkSelectAll, &QCheckBox::toggled,
            this, &MainWindow::onSelectAllToggled);

    /* ── Context menu tasto destro sulle chat ── */
    m_chatList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_chatList, &QListWidget::customContextMenuRequested,
            this, &MainWindow::onChatContextMenuRequested);

    refreshChatList();

    m_sidebarWidget = bar;

    return bar;
}

/* ══════════════════════════════════════════════════════════════
   buildContent — wrapper con navMenuBar + QTabWidget con tutti i tab
   ══════════════════════════════════════════════════════════════ */
QWidget* MainWindow::buildContent()
{
    auto* wrapper = new QWidget(this);
    auto* wLay    = new QVBoxLayout(wrapper);
    wLay->setContentsMargins(0, 0, 0, 0);
    wLay->setSpacing(0);

    /* QTabWidget principale */
    m_mainTabs = new QTabWidget(wrapper);
    m_mainTabs->setObjectName("mainTabs");
    m_mainTabs->setTabPosition(QTabWidget::North);
    m_mainTabs->setMovable(false);
    m_mainTabs->setAccessibleName("Sezioni principali di Prismalux");

    /* Backend button + status badge come corner widget sinistro */
    if (m_btnBackend) {
        m_btnBackend->setFixedHeight(dpiScale(28));
        m_btnBackend->setMinimumWidth(dpiScale(110));
        m_cornerContainer = new QWidget(m_mainTabs);
        auto* cornerLay = new QHBoxLayout(m_cornerContainer);
        cornerLay->setContentsMargins(4, 0, 8, 0);
        cornerLay->setSpacing(6);
        cornerLay->addWidget(m_btnBackend);
        m_mainTabs->setCornerWidget(m_cornerContainer, Qt::TopLeftCorner);
    }

    /* ── Tab 0: EAGER — unico tab costruito subito (blocca lo show il meno possibile).
     * Tutti gli altri (incluse Strumenti[1] e Programmazione[3], prima eager) sono
     * placeholder sostituiti in background da onPreBuildTabN() dopo il primo show():
     * costruire ~12 sotto-tab di Strumenti + Programmazione prima di mostrare la
     * finestra costava da solo ~1s di setStyleSheet()/reparenting su widget già
     * esistenti — vedi ensureTabBuilt() per il meccanismo di sostituzione. ── */
    buildAiTab();             /* 0 — primo tab visibile */
    static const struct { const char* label; } kPlaceholders[] = {
        { "\xf0\x9f\x9b\xa0\xef\xb8\x8f  Strumenti" },      /* 1 */
        { "\xf0\x9f\x8e\xac  Media" },                      /* 2 */
        { "\xf0\x9f\x92\xbb  Programmazione" },             /* 3 */
        { "\xf0\x9f\x93\x90  Matematica" },                 /* 4 */
        { "\xf0\x9f\x94\xa7  Utilit\xc3\xa0" },             /* 5 */
        { "\xf0\x9f\xa7\xac  Bioinformatica" },              /* 6 */
        { "\xf0\x9f\x95\xb9\xef\xb8\x8f  TeleComanda" },     /* 7 */
    };
    for (const auto& ph : kPlaceholders) {
        auto* w = new QWidget(m_mainTabs);
        (new QVBoxLayout(w))->setContentsMargins(0,0,0,0);
        m_mainTabs->addTab(w, QString::fromUtf8(ph.label));
    }

    /* Inizializza la mappa: solo AI[0] già costruito, tutto il resto è placeholder.
     * Il pre-build in background di tutte le altre tab è schedulato in
     * setupTimers() (Strumenti/Programmazione per primi, sono "contenitori"
     * per Ricerca e DevAgent+Security richiesti dalle tab successive). */
    m_tabBuilt = {true, false, false, false, false, false, false, false};
    /* 🔍 Ricerca schede — corner widget destro: hover apre, uscita chiude */
    {
        auto* srchWrap = new QWidget(m_mainTabs);
        srchWrap->setObjectName("tabSearchWrap");
        m_tabSearchWrap = srchWrap;
        auto* srchLay  = new QHBoxLayout(srchWrap);
        srchLay->setContentsMargins(2, 2, 6, 2);
        srchLay->setSpacing(4);

        auto* srchBtn = new QPushButton("\xf0\x9f\x94\x8d", srchWrap);
        srchBtn->setObjectName("tabSearchBtn");
        srchBtn->setFlat(true);
        srchBtn->setFixedSize(dpiScale(28), dpiScale(28));
        srchBtn->setCursor(Qt::PointingHandCursor);
        srchBtn->setToolTip(tr("Cerca scheda"));

        m_tabSearchEdit = new QLineEdit(srchWrap);
        m_tabSearchEdit->setObjectName("tabSearchEdit");
        m_tabSearchEdit->setPlaceholderText(tr("Cerca scheda..."));
        m_tabSearchEdit->setClearButtonEnabled(true);
        m_tabSearchEdit->setMaximumWidth(0);   /* collassato all'avvio */
        m_tabSearchEdit->setFixedHeight(dpiScale(24));

        srchLay->addWidget(srchBtn);
        srchLay->addWidget(m_tabSearchEdit);
        m_mainTabs->setCornerWidget(srchWrap, Qt::TopRightCorner);

        /* Ricerca live */
        connect(m_tabSearchEdit, &QLineEdit::textChanged,
                this, &MainWindow::onTabSearchTextChanged);
        /* Invio → chiude */
        connect(m_tabSearchEdit, &QLineEdit::returnPressed, this, [this]{
            m_tabSearchEdit->clear();
            m_tabSearchEdit->setMaximumWidth(0);
        });

        /* Escape e hover gestiti in eventFilter */
        srchBtn->installEventFilter(this);
        srchWrap->installEventFilter(this);
        m_tabSearchEdit->installEventFilter(this);
        /* Click ovunque fuori dal popup/campo → chiude (il popup non è
           Qt::Popup, quindi serve un filtro applicativo per il click-away) */
        qApp->installEventFilter(this);
    }

    /* Salva etichette originali e applica modalità da QSettings */
    for (int i = 0; i < m_mainTabs->count(); i++)
        m_tabOrigLabels << m_mainTabs->tabText(i);
    {
        QSettings s("Prismalux", "GUI");
        applyTabMode(s.value(P::SK::kNavTabMode, "icons_text").toString());
    }

    buildNavMenuBar(wrapper, wLay);

    wLay->addWidget(m_navMenuBar);
    wLay->addWidget(m_mainTabs, 1);

    applyContentSettings();

    return wrapper;
}

/* ── Livello 2: tab [0] Intelligenza Artificiale ─────────────────── */
void MainWindow::buildAiTab()
{
    auto* agentiPage = new AgentiPage(m_ai, this);
    connect(agentiPage, &AgentiPage::chatCompleted,
            this,       &MainWindow::onChatCompleted);
    connect(agentiPage, &AgentiPage::pipelineStatus,
            this,       &MainWindow::onPipelineStatus);
    connect(agentiPage, &AgentiPage::requestOpenSettings,
            this, &MainWindow::onGraficoRequestSettings);
    connect(agentiPage, &AgentiPage::requestShowInGrafico,
            this, &MainWindow::onRequestShowInGrafico);
    m_mainTabs->addTab(agentiPage, "\xf0\x9f\xa4\x96  Intelligenza artificiale");  /* 0 */
}

/* ══════════════════════════════════════════════════════════════
   Lazy tab loading — ensureTabBuilt + factory create*Widget
   ══════════════════════════════════════════════════════════════ */

/** Sostituisce il placeholder all'indice idx col widget reale.
 *  Sicuro da chiamare multiple volte (guard su m_tabBuilt) e sia da un
 *  clic utente (currentChanged) sia da un timer di pre-build in background:
 *  in quest'ultimo caso la tab visibile all'utente NON viene toccata —
 *  solo se idx era già quella corrente (clic esplicito) resta selezionata. */
void MainWindow::ensureTabBuilt(int idx)
{
    if (idx < 0 || idx >= m_tabBuilt.size() || m_tabBuilt.value(idx)) return;
    m_tabBuilt[idx] = true;   /* guard anticipato — evita ri-entrata da currentChanged */

    const QString text = m_mainTabs->tabText(idx);
    const int prevCurrent = m_mainTabs->currentIndex();

    QWidget* page = nullptr;
    switch (idx) {
    case 1: page = createStrumentiWidget();      break;
    case 2: page = createMultimediaWidget();     break;
    case 3: page = createProgrammazioneWidget(); break;
    case 4: page = createMatematicaWidget();     break;
    case 5: page = createUtilityWidget();        break;
    case 6: page = createBioinformaticaWidget(); break;
    case 7: page = createAppControllerWidget();  break;
    default: return;
    }

    if (!page) return;

    /* Sostituisce il placeholder in-place senza cambiare indice */
    m_mainTabs->blockSignals(true);
    m_mainTabs->removeTab(idx);
    m_mainTabs->insertTab(idx, page, text);
    m_mainTabs->blockSignals(false);
    /* Ripristina la tab visibile prima della sostituzione: un pre-build in
     * background non deve rubare il focus. Se idx era già quella corrente
     * (clic esplicito dell'utente), il comportamento resta identico a prima. */
    m_mainTabs->setCurrentIndex(prevCurrent == idx ? idx : prevCurrent);
}

QWidget* MainWindow::createStrumentiWidget()
{
    m_strumentiPage = new StrumentiPage(m_ai, this);
    connect(m_strumentiPage, &StrumentiPage::cronPanelFirstOpen,
            this, &MainWindow::onCronPanelFirstOpen);
    buildRicercaTab();   /* Ricerca è sotto-tab finale di Strumenti — costruita insieme */
    return m_strumentiPage;
}

QWidget* MainWindow::createProgrammazioneWidget()
{
    buildProgrammazioneTab();
    return m_progPage;
}

QWidget* MainWindow::createMultimediaWidget()
{
    return new MultimediaPage(m_ai, this);
}

QWidget* MainWindow::createMatematicaWidget()
{
    auto* grafPage = new GraficoPage(m_ai, this);
    m_grafCanvas = grafPage->canvas();
    if (m_impPage) m_impPage->setGraficoCanvas(m_grafCanvas);
    connect(grafPage, &GraficoPage::requestOpenSettings,
            this, &MainWindow::onGraficoRequestSettings);

    auto* container = new QWidget;
    auto* lay = new QVBoxLayout(container);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);
    auto* sub = new QTabWidget(container);
    sub->setObjectName("mathSubTabs");
    sub->setTabPosition(QTabWidget::North);
    sub->addTab(new MatematicaPage(m_ai, container), "\xf0\x9f\x93\x90  Matematica");
    sub->addTab(grafPage, "\xf0\x9f\x93\x88  Grafico");
    connect(sub, &QTabWidget::currentChanged, this, &MainWindow::onMathSubTabChanged);
    lay->addWidget(sub);
    return container;
}

QWidget* MainWindow::createUtilityWidget()
{
    m_utilityPage = new UtilityPage(m_ai, this);
    ensureTabBuilt(1);   /* assicura Strumenti+Ricerca per MultiAgent (no-op se già costruita) */
    ensureTabBuilt(3);   /* assicura Programmazione per "Rete & Network" in LanWanPage */
    buildLanWanTab();                        /* sub-tab dentro Utility */
    buildMultiAgentTab();                    /* cross-pollination */
    return m_utilityPage;
}

QWidget* MainWindow::createBioinformaticaWidget()
{
    return new BioinformaticaPage(m_ai, this);
}

QWidget* MainWindow::createAppControllerWidget()
{
    auto* appCtrl = new AppControllerPage(m_ai, this);
    connect(appCtrl, &AppControllerPage::openSettingsDipendenze,
            this,    &MainWindow::onOpenSettingsDipendenze);
    ensureTabBuilt(3);   /* assicura Programmazione prima di agganciare DevAgent+Sicurezza */
    if (m_progPage) {
        m_progPage->addExternalTab(appCtrl->buildDevAgentTab(),
                                   "\xf0\x9f\xa4\x96  Dev Agent");
        m_progPage->addExternalTab(new SecurityAnalyzerPage(m_ai, m_progPage),
                                   "\xf0\x9f\x94\x90  Sicurezza");
    }
    return appCtrl;
}

/* ── Costruisce m_progPage — chiamata da createProgrammazioneWidget() ── */
void MainWindow::buildProgrammazioneTab()
{
    m_progPage = new ProgrammazionePage(m_ai, this);
}

/* ── Livello 2: tab [6] Ricerca ──────────────────────────────────── */
void MainWindow::buildRicercaTab()
{
    m_ricercaPage = new RicercaPage(m_ai, this);
    /* Ricerca unita a Strumenti come sotto-tab finale */
    if (m_strumentiPage)
        m_strumentiPage->addExternalTab(m_ricercaPage,
                                        "\xf0\x9f\x94\xac  Ricerca");
}

/* ── Livello 2: tab [7] LAN & WAN ────────────────────────────────── */
void MainWindow::buildLanWanTab()
{
    m_lanWanPage = new LanWanPage(m_ai, this);
    m_agentiMultiPage = m_lanWanPage->multiAgentTab();

    /* Sposta "Rete & Network" da ProgrammazionePage a LanWanPage */
    if (m_progPage)
        m_lanWanPage->addExtraTab(
            m_progPage->buildReteNetworkWidget(m_lanWanPage),
            "\xf0\x9f\x94\xa1  Rete & Network");

    /* Inserisce LAN & WAN dentro Utility [5] invece di un tab principale separato */
    if (m_utilityPage)
        m_utilityPage->addTab(m_lanWanPage, "\xf0\x9f\x8c\x90  LAN & WAN");
}

/* ── Livello 2: ex tab [9] Multi-Agente — ora embedded in LAN & WAN ── */
void MainWindow::buildMultiAgentTab()
{
    /* Nessun tab separato: Multi-Agente vive dentro LanWanPage.
     * Qui impostiamo solo la cross-pollination con il RagGraph. */
    if (m_ricercaPage && m_agentiMultiPage)
        m_agentiMultiPage->setExtRagMemory(m_ricercaPage->ragGraphMemory());
}

/* ── Livello 2: barra navigazione menu + sincronizzazione tab ────── */
void MainWindow::buildNavMenuBar(QWidget* wrapper, QVBoxLayout* /*wLay*/)
{
    m_navMenuBar = new QFrame(wrapper);
    m_navMenuBar->setObjectName("navMenuBar");
    m_navMenuBar->setFixedHeight(dpiScale(40));
    m_navMenuBar->hide();
    auto* nmLay = new QHBoxLayout(m_navMenuBar);
    nmLay->setContentsMargins(8, 2, 8, 2);
    nmLay->setSpacing(2);

    auto* btnGroup = new QButtonGroup(m_navMenuBar);
    btnGroup->setExclusive(true);
    for (int i = 0; i < m_mainTabs->count(); i++) {
        auto* btn = new QPushButton(m_tabOrigLabels.at(i), m_navMenuBar);
        btn->setObjectName("navMenuBtn");
        btn->setCheckable(true);
        btn->setChecked(i == 0);
        btn->setFlat(true);
        btnGroup->addButton(btn, i);
        m_navBtns << btn;
        nmLay->addWidget(btn);
    }
    connect(btnGroup, &QButtonGroup::idClicked,
            m_mainTabs, &QTabWidget::setCurrentIndex);
    nmLay->addStretch();

    /* Clone del backend button all'estrema destra della nav bar */
    if (m_btnBackend) {
        auto* backendClone = new QPushButton(m_navMenuBar);
        backendClone->setObjectName("navMenuBackend");
        backendClone->setFlat(true);
        backendClone->setFixedHeight(dpiScale(30));
        m_navBackendClone = backendClone;
        connect(m_btnBackend,  &QPushButton::clicked, this, &MainWindow::onSyncNavBackendClone);
        onSyncNavBackendClone();
        connect(backendClone, &QPushButton::clicked, m_btnBackend, &QPushButton::click);
        nmLay->addWidget(backendClone);
    }

    connect(m_mainTabs, &QTabWidget::currentChanged, this, &MainWindow::onMainTabChanged);
}
