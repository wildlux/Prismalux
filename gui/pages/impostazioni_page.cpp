#include "impostazioni_page.h"
#include "../widgets/toggle_switch.h"
#include "../widgets/stt_whisper.h"
#include "personalizza_page.h"
#include "manutenzione_page.h"
#include "grafico_page.h"
#include "../theme_manager.h"
#include "../prismalux_paths.h"
#include "../app_config.h"
namespace P = PrismaluxPaths;  /* alias file-scope per P::SK::kXxx */
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QTabWidget>
#include <QScrollArea>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QButtonGroup>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QSettings>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QFileDialog>
#include <QDateTime>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <functional>
#include <QCoreApplication>
#include <QTimer>
#include <QRadioButton>
#include <QCheckBox>
#include <QListWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QGroupBox>
#include <QPlainTextEdit>
#include <QThread>
#include <functional>
#include <QStackedWidget>
#include <QColorDialog>
#include <QPainter>
#include <QPen>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QGuiApplication>
#include <QClipboard>

/* ══════════════════════════════════════════════════════════════
   ImpostazioniPage — 4 tab raggruppate + Grafico (dinamico).
   ┌─────────────────────────────────────────────────────┐
   │  🦙 AI Locale │ 🤖 LLM │ 🎨 Aspetto │ 🔧 Sistema  │
   └─────────────────────────────────────────────────────┘
   Ogni tab principale contiene un QTabWidget interno con
   objectName("settingsInnerTabs") così gli stylesheet
   possono distinguerli dal tab bar esterno.
   ══════════════════════════════════════════════════════════════ */

/* Helper locale: crea QTabWidget interno con stile uniforme */
static QTabWidget* _makeInner(QWidget* parent)
{
    auto* t = new QTabWidget(parent);
    t->setObjectName("settingsInnerTabs");
    t->setDocumentMode(true);
    t->setUsesScrollButtons(true);
    return t;
}

ImpostazioniPage::ImpostazioniPage(AiClient* ai, HardwareMonitor* hw, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    m_tabs = new QTabWidget(this);
    auto* tabs = m_tabs;
    tabs->setObjectName("settingsTabs");

    /* Factory widget senza presenza visiva propria — espongono solo buildXxx() */
    m_manutenzione = new ManutenzioneePage(ai, hw, this);
    m_manutenzione->hide();
    m_personalizza = new PersonalizzaPage(this);
    m_personalizza->hide();

    /* ════════════════════════════════════════════════════════════
       Gruppo 1: 🦙 AI Locale
       Connessione · Hardware · AI Locale · Parametri AI · RAG
       · Voce & Audio · Avanzate
       ════════════════════════════════════════════════════════════ */
    {
        m_tabAiLocale = _makeInner(this);
        auto* t = m_tabAiLocale;

        t->addTab(m_manutenzione->buildBackend(),
                  "\xf0\x9f\x94\x8c  Connessione");

        t->addTab(m_manutenzione->buildHardware(),
                  "\xf0\x9f\x96\xa5  Hardware");

        t->addTab(buildAiLocaleTab(),
                  "\xf0\x9f\xa6\x99  AI Locale");

        {
            auto* sc = new QScrollArea;
            sc->setWidgetResizable(true);
            sc->setFrameShape(QFrame::NoFrame);
            sc->setWidget(buildAiParamsTab());
            t->addTab(sc, "\xe2\x9a\x99\xef\xb8\x8f  Parametri AI");
        }

        {
            auto* sc = new QScrollArea;
            sc->setWidgetResizable(true);
            sc->setFrameShape(QFrame::NoFrame);
            sc->setWidget(buildRagTab());
            t->addTab(sc, "\xf0\x9f\x93\x9a  RAG");
        }

        {
            auto* w = new QWidget;
            auto* l = new QHBoxLayout(w);
            l->setContentsMargins(14, 12, 14, 12);
            l->setSpacing(14);
            l->addWidget(buildVoceTab());
            l->addWidget(buildTrascriviTab());
            t->addTab(w, "\xf0\x9f\x8e\xa4  Voce & Audio");
        }

        {
            auto* sc = new QScrollArea;
            sc->setWidgetResizable(true);
            sc->setFrameShape(QFrame::NoFrame);
            sc->setWidget(buildSandboxTab());
            t->addTab(sc, "\xf0\x9f\x90\xb3  Sandbox");
        }

        t->addTab(m_personalizza->buildLoraTab(),
                  "\xf0\x9f\xa7\xa0  Fine-tuning");

        t->addTab(m_personalizza->buildLlamaStudio(),
                  "\xf0\x9f\xa6\x99  llama.cpp Studio");

        {
            auto* w = new QWidget;
            auto* l = new QVBoxLayout(w);
            l->setContentsMargins(0, 0, 0, 0);
            l->setSpacing(0);
            auto* mon = new MonitorPanel(ai, hw, w);
            mon->setWindowFlags(Qt::Widget);
            mon->setMinimumSize(0, 0);
            l->addWidget(mon, 1);
            t->addTab(w, "\xf0\x9f\x93\x8a  Avanzate");
        }

        tabs->addTab(t, "\xf0\x9f\xa6\x99  AI Locale");
    }

    /* ════════════════════════════════════════════════════════════
       Gruppo 2: 🤖 LLM
       LLM Consigliati · Classifica · Test
       ════════════════════════════════════════════════════════════ */
    {
        m_tabLlm = _makeInner(this);
        auto* t  = m_tabLlm;

        t->addTab(buildLlmConsigliatiTab(),
                  "\xf0\x9f\xa4\x96  LLM");

        t->addTab(buildLlmClassificaTab(),
                  "\xf0\x9f\x93\x8a  Classifica");

        {
            auto* sc  = new QScrollArea;
            sc->setWidgetResizable(true);
            sc->setFrameShape(QFrame::NoFrame);
            auto* w   = new QWidget;
            auto* l   = new QVBoxLayout(w);
            l->setContentsMargins(0, 0, 0, 0);
            l->setSpacing(0);
            l->addWidget(buildTestTab());
            l->addStretch();
            sc->setWidget(w);
            t->addTab(sc, "\xf0\x9f\xa7\xaa  Test");
        }

        tabs->addTab(t, "\xf0\x9f\xa4\x96  LLM");
    }

    /* ════════════════════════════════════════════════════════════
       Gruppo 3: 🎨 Visuale
       Aspetto (temi) · Grafico (aggiunto dinamicamente via setGraficoCanvas)
       ════════════════════════════════════════════════════════════ */
    {
        m_tabVisuale = _makeInner(this);
        m_tabVisuale->addTab(buildTemaTab(), "\xf0\x9f\x8e\xa8  Aspetto");
        // Il sub-tab "Grafico" viene aggiunto dopo dalla chiamata setGraficoCanvas()
        tabs->addTab(m_tabVisuale, "\xf0\x9f\x8e\xa8  Visuale");
    }

    /* ════════════════════════════════════════════════════════════
       Gruppo 4: 🔧 Sistema
       Pulizia · Bug Tracker · Cron
       ════════════════════════════════════════════════════════════ */
    {
        m_tabSistema = _makeInner(this);
        auto* t      = m_tabSistema;

        {
            auto* sc = new QScrollArea;
            sc->setWidgetResizable(true);
            sc->setFrameShape(QFrame::NoFrame);
            sc->setWidget(buildPuliziaTab());
            t->addTab(sc, "\xf0\x9f\xa7\xb9  Pulizia");
        }

        t->addTab(m_manutenzione->buildBugTracker(),
                  "\xf0\x9f\x94\x8d  Bug Tracker");

        t->addTab(m_manutenzione->buildCronTab(),
                  "\xe2\x8f\xb0  Cron");

        /* LAN Android spostata in Strumenti AI */

        tabs->addTab(t, "\xf0\x9f\x94\xa7  Sistema");
    }

    /* ════════════════════════════════════════════════════════════
       Tab 5: 🔌 MCP — configurazione Model Context Protocol
       ════════════════════════════════════════════════════════════ */
    tabs->addTab(buildMcpTab(), "\xf0\x9f\x94\x8c  MCP");

    /* ════════════════════════════════════════════════════════════
       Tab 6: 📜 Ringraziamenti — licenza MIT + crediti
       ════════════════════════════════════════════════════════════ */
    tabs->addTab(buildRingraziamentiTab(), "\xf0\x9f\x93\x9c  Ringraziamenti");

    lay->addWidget(tabs);
}

/* ══════════════════════════════════════════════════════════════
   switchToTab — porta in primo piano il tab che contiene @p name
   ══════════════════════════════════════════════════════════════ */
void ImpostazioniPage::switchToTab(const QString& name)
{
    if (!m_tabs) return;
    for (int i = 0; i < m_tabs->count(); i++) {
        if (m_tabs->tabText(i).contains(name, Qt::CaseInsensitive)) {
            m_tabs->setCurrentIndex(i);
            return;
        }
        // Cerca nei QTabWidget annidati (settingsInnerTabs)
        if (auto* inner = qobject_cast<QTabWidget*>(m_tabs->widget(i))) {
            for (int j = 0; j < inner->count(); j++) {
                if (inner->tabText(j).contains(name, Qt::CaseInsensitive)) {
                    m_tabs->setCurrentIndex(i);
                    inner->setCurrentIndex(j);
                    return;
                }
            }
        }
    }
}

/* ══════════════════════════════════════════════════════════════
   ChartPreviewWidget — mini bar chart con i colori del ChartStyle.
   Nessun Q_OBJECT: ridisegna via paintEvent ogni volta che il canvas
   emette update() oppure viene chiamato refresh() esplicitamente.
   ══════════════════════════════════════════════════════════════ */
class ChartPreviewWidget : public QWidget {
public:
    GraficoCanvas* m_canvas = nullptr;

    explicit ChartPreviewWidget(GraficoCanvas* canvas, QWidget* parent = nullptr)
        : QWidget(parent), m_canvas(canvas)
    {
        setFixedHeight(140);
        setMinimumWidth(240);
    }

    void refresh() { update(); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        GraficoCanvas::ChartStyle def;
        const GraficoCanvas::ChartStyle& s = m_canvas ? m_canvas->style() : def;

        const QRect r = rect();

        // sfondo
        p.fillRect(r, s.bgColor);

        // cornice sottile
        p.setPen(QPen(s.axisColor, 1));
        p.drawRect(r.adjusted(0, 0, -1, -1));

        // margini area di disegno
        const int lm = 36, bm = 20, tm = 14, rm = 12;
        const int xA = r.left()  + lm;
        const int yA = r.bottom() - bm;
        const int xR = r.right() - rm;
        const int yT = r.top()   + tm;
        const int plotH = yA - yT;
        const int plotW = xR - xA;

        // griglia orizzontale
        if (s.showGrid) {
            p.setPen(QPen(s.gridColor, 1, Qt::DotLine));
            const int nLines = 4;
            for (int i = 1; i <= nLines; i++) {
                int gy = yA - i * plotH / nLines;
                p.drawLine(xA, gy, xR, gy);
            }
        }

        // etichette asse Y
        QFont fLbl;
        fLbl.setPointSize(7);
        p.setFont(fLbl);
        p.setPen(s.textColor);
        const int nTicks = 4;
        for (int i = 0; i <= nTicks; i++) {
            int gy = yA - i * plotH / nTicks;
            p.drawText(r.left() + 2, gy + 4, QString::number(i * 25));
            // tick
            p.setPen(QPen(s.axisColor, 1));
            p.drawLine(xA - 3, gy, xA, gy);
            p.setPen(s.textColor);
        }

        // assi
        p.setPen(QPen(s.axisColor, 1.5));
        p.drawLine(xA, yT, xA, yA);    // asse Y
        p.drawLine(xA, yA, xR, yA);    // asse X

        // barre campione (6 valori fissi)
        static const double kVals[] = { 0.55, 0.80, 0.35, 0.90, 0.65, 0.45 };
        const int N = 6;
        static const QColor kDefPal[] = {
            {0x00,0xbf,0xd8},{0xff,0x79,0x5a},{0x73,0xe2,0x73},
            {0xff,0xd7,0x6e},{0xb0,0x90,0xff},{0xff,0x82,0xc2}
        };
        const int gap = 4;
        const int barW = (plotW - gap * (N + 1)) / N;
        for (int i = 0; i < N; i++) {
            QColor c = (!s.palette.isEmpty() && i < s.palette.size())
                        ? s.palette[i] : kDefPal[i];
            int bh = int(kVals[i] * plotH);
            int bx = xA + gap + i * (barW + gap);
            p.fillRect(bx, yA - bh, barW, bh, c);
        }

        // etichetta "anteprima" in basso a destra
        p.setPen(s.textColor.darker(110));
        QFont fIt;
        fIt.setPointSize(7);
        fIt.setItalic(true);
        p.setFont(fIt);
        p.drawText(r.adjusted(0, 0, -rm, -3),
                   Qt::AlignBottom | Qt::AlignRight, "anteprima stile");
    }
};

/* ══════════════════════════════════════════════════════════════
   buildGraficoTab — controlli posizione assi 2D e 3D
   ══════════════════════════════════════════════════════════════ */
QWidget* ImpostazioniPage::buildGraficoTab(GraficoCanvas* canvas)
{
    static const char* kAxisItems[] = {
        "Origine dati (x=0, y=0)",
        "\xe2\x86\x99  Basso sinistra",
        "\xe2\x86\x98  Basso destra",
        "\xe2\x86\x96  Alto sinistra",
        "\xe2\x86\x97  Alto destra"
    };

    /* ── Helper: bottone color picker ──────────────────────────────
       Mostra il colore attuale come sfondo; click apre QColorDialog.
       onChange(QColor) viene chiamato ogni volta che il colore cambia. */
    auto makeColorBtn = [](QWidget* parent, QColor initial,
                           std::function<void(QColor)> onChange) -> QPushButton* {
        auto* btn = new QPushButton(parent);
        btn->setFixedSize(44, 26);
        auto applyColor = [btn](QColor c) {
            btn->setStyleSheet(
                QString("QPushButton { background:%1; border:1px solid rgba(255,255,255,0.25);"
                        " border-radius:4px; }").arg(c.name()));
            btn->setProperty("currentColor", c.name());
        };
        applyColor(initial);
        QObject::connect(btn, &QPushButton::clicked, btn,
            [btn, onChange, applyColor]() {
                QColor cur = QColor(btn->property("currentColor").toString());
                QColor c   = QColorDialog::getColor(cur, btn, "Scegli colore",
                                                    QColorDialog::ShowAlphaChannel);
                if (c.isValid()) { applyColor(c); onChange(c); }
            });
        return btn;
    };

    /* ── Preview widget — creato prima di saveStyle per poterlo catturare ── */
    auto* preview = new ChartPreviewWidget(canvas);

    /* ── Helper: salva stile in AppConfig + aggiorna preview ── */
    auto saveStyle = [canvas, preview]() {
        if (!canvas) return;
        const auto& s = canvas->style();
        auto& cfg = AppConfig::s();
        cfg.setValue("ChartStyle/bgColor",   s.bgColor.name(QColor::HexArgb));
        cfg.setValue("ChartStyle/axisColor", s.axisColor.name(QColor::HexArgb));
        cfg.setValue("ChartStyle/gridColor", s.gridColor.name(QColor::HexArgb));
        cfg.setValue("ChartStyle/textColor", s.textColor.name(QColor::HexArgb));
        cfg.setValue("ChartStyle/fontFamily",s.fontFamily);
        cfg.setValue("ChartStyle/fontSize",  s.fontSize);
        cfg.setValue("ChartStyle/showGrid",  s.showGrid);
        QStringList pal;
        for (const QColor& c : s.palette) pal << c.name(QColor::HexArgb);
        cfg.setValue("ChartStyle/palette", pal);
        if (preview) preview->refresh();
    };

    /* ── Carica stile salvato all'avvio ── */
    if (canvas) {
        auto& cfg = AppConfig::s();
        GraficoCanvas::ChartStyle s;
        auto loadColor = [&](const char* key, QColor def) {
            QString v = cfg.value(key).toString();
            return v.isEmpty() ? def : QColor(v);
        };
        s.bgColor    = loadColor("ChartStyle/bgColor",   s.bgColor);
        s.axisColor  = loadColor("ChartStyle/axisColor", s.axisColor);
        s.gridColor  = loadColor("ChartStyle/gridColor", s.gridColor);
        s.textColor  = loadColor("ChartStyle/textColor", s.textColor);
        s.fontFamily = cfg.value("ChartStyle/fontFamily", s.fontFamily).toString();
        s.fontSize   = cfg.value("ChartStyle/fontSize",   s.fontSize).toInt();
        s.showGrid   = cfg.value("ChartStyle/showGrid",   s.showGrid).toBool();
        const QStringList pal = cfg.value("ChartStyle/palette").toStringList();
        for (const QString& c : pal) s.palette << QColor(c);
        canvas->setStyle(s);
    }

    /* ═══════════════════════════════════════════════════════════════
       Layout principale: scroll area verticale
       ═══════════════════════════════════════════════════════════════ */
    auto* page  = new QWidget;
    auto* scroll= new QScrollArea(page);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* rootLay = new QVBoxLayout(page);
    rootLay->setContentsMargins(0,0,0,0);
    rootLay->addWidget(scroll);

    auto* inner = new QWidget;
    auto* outer = new QVBoxLayout(inner);
    outer->setContentsMargins(20,20,20,20);
    outer->setSpacing(16);
    scroll->setWidget(inner);

    /* ── Sezione 0: Anteprima stile ───────────────────────────── */
    auto* prevCard = new QFrame(inner);
    prevCard->setObjectName("cardFrame");
    auto* prevLay  = new QVBoxLayout(prevCard);
    prevLay->setContentsMargins(14,12,14,12);
    prevLay->setSpacing(8);

    auto* prevTitle = new QLabel("\xf0\x9f\x96\xbc  Anteprima stile", prevCard);  /* 🖼 */
    prevTitle->setObjectName("cardTitle");
    prevLay->addWidget(prevTitle);

    auto* prevDesc = new QLabel(
        "Aggiornata in tempo reale ad ogni modifica dei colori, palette e preset.", prevCard);
    prevDesc->setObjectName("cardDesc");
    prevDesc->setWordWrap(true);
    prevLay->addWidget(prevDesc);

    preview->setParent(prevCard);
    prevLay->addWidget(preview);
    outer->addWidget(prevCard);

    /* ── Sezione 1: Posizione Assi ─────────────────────────────── */
    auto* axCard = new QFrame(inner);
    axCard->setObjectName("cardFrame");
    auto* axLay  = new QVBoxLayout(axCard);
    axLay->setContentsMargins(14,12,14,12);
    axLay->setSpacing(10);

    auto* axTitle = new QLabel("\xf0\x9f\x93\x88  Posizione assi", axCard);  /* 📈 */
    axTitle->setObjectName("cardTitle");
    axLay->addWidget(axTitle);

    auto* axDesc = new QLabel(
        "Gizmo assi 2D (Cartesiano, Scatter) e 3D (Scatter 3D, Grafo 3D).", axCard);
    axDesc->setWordWrap(true);
    axDesc->setObjectName("cardDesc");
    axLay->addWidget(axDesc);

    auto* axForm = new QFormLayout;
    axForm->setSpacing(8);
    axForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto* cmb2d = new QComboBox(axCard);
    cmb2d->setObjectName("settingCombo");
    for (auto* s : kAxisItems) cmb2d->addItem(s);
    cmb2d->setCurrentIndex(0);
    if (canvas)
        QObject::connect(cmb2d, QOverload<int>::of(&QComboBox::currentIndexChanged),
                         canvas, [canvas](int idx){ canvas->setAxes2dPos(idx); });
    axForm->addRow("Assi 2D:", cmb2d);

    auto* cmb3d = new QComboBox(axCard);
    cmb3d->setObjectName("settingCombo");
    for (int i = 1; i < 5; i++) cmb3d->addItem(kAxisItems[i]);
    cmb3d->setCurrentIndex(1);
    if (canvas)
        QObject::connect(cmb3d, QOverload<int>::of(&QComboBox::currentIndexChanged),
                         canvas, [canvas](int idx){ canvas->setAxes3dPos(idx + 1); });
    axForm->addRow("Assi 3D:", cmb3d);
    axLay->addLayout(axForm);
    outer->addWidget(axCard);

    /* ── Sezione 2: Colori ─────────────────────────────────────── */
    auto* colCard = new QFrame(inner);
    colCard->setObjectName("cardFrame");
    auto* colLay  = new QVBoxLayout(colCard);
    colLay->setContentsMargins(14,12,14,12);
    colLay->setSpacing(10);

    auto* colTitle = new QLabel("\xf0\x9f\x8e\xa8  Colori grafico", colCard);  /* 🎨 */
    colTitle->setObjectName("cardTitle");
    colLay->addWidget(colTitle);

    auto* colForm = new QFormLayout;
    colForm->setSpacing(8);
    colForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    GraficoCanvas::ChartStyle defStyle;  /* valori default per inizializzare bottoni */
    if (canvas) defStyle = canvas->style();

    auto* btnBg = makeColorBtn(colCard, defStyle.bgColor, [canvas, saveStyle](QColor c){
        if (canvas) { canvas->style().bgColor = c; canvas->update(); saveStyle(); }
    });
    colForm->addRow("Sfondo:", btnBg);

    auto* btnAxis = makeColorBtn(colCard, defStyle.axisColor, [canvas, saveStyle](QColor c){
        if (canvas) { canvas->style().axisColor = c; canvas->update(); saveStyle(); }
    });
    colForm->addRow("Assi:", btnAxis);

    auto* btnGrid = makeColorBtn(colCard, defStyle.gridColor, [canvas, saveStyle](QColor c){
        if (canvas) { canvas->style().gridColor = c; canvas->update(); saveStyle(); }
    });

    auto* gridRow = new QWidget(colCard);
    auto* gridRowLay = new QHBoxLayout(gridRow);
    gridRowLay->setContentsMargins(0,0,0,0);
    gridRowLay->setSpacing(8);
    gridRowLay->addWidget(btnGrid);
    auto* chkGrid = new QCheckBox("Visibile", colCard);
    chkGrid->setChecked(defStyle.showGrid);
    if (canvas)
        QObject::connect(chkGrid, &QCheckBox::toggled, canvas,
            [canvas, saveStyle](bool on){ canvas->style().showGrid = on; canvas->update(); saveStyle(); });
    gridRowLay->addWidget(chkGrid);
    gridRowLay->addStretch();
    colForm->addRow("Griglia:", gridRow);

    auto* btnText = makeColorBtn(colCard, defStyle.textColor, [canvas, saveStyle](QColor c){
        if (canvas) { canvas->style().textColor = c; canvas->update(); saveStyle(); }
    });
    colForm->addRow("Testo assi:", btnText);

    colLay->addLayout(colForm);
    outer->addWidget(colCard);

    /* ── Sezione 3: Palette serie ──────────────────────────────── */
    auto* palCard = new QFrame(inner);
    palCard->setObjectName("cardFrame");
    auto* palLay  = new QVBoxLayout(palCard);
    palLay->setContentsMargins(14,12,14,12);
    palLay->setSpacing(10);

    auto* palTitle = new QLabel("\xf0\x9f\x8e\xb2  Colori serie (palette)", palCard);  /* 🎲 */
    palTitle->setObjectName("cardTitle");
    palLay->addWidget(palTitle);

    auto* palDesc = new QLabel("8 colori ciclici per le linee/barre. Svuota per usare la palette interna.", palCard);
    palDesc->setObjectName("cardDesc");
    palDesc->setWordWrap(true);
    palLay->addWidget(palDesc);

    /* 8 default palette interna */
    static const QColor kDefaultPal[] = {
        {0x00,0xbf,0xd8}, {0xff,0x79,0x5a}, {0x73,0xe2,0x73},
        {0xff,0xd7,0x6e}, {0xb0,0x90,0xff}, {0xff,0x82,0xc2},
        {0x5a,0xd7,0xff}, {0xff,0xa0,0x50}
    };

    auto* palBtnRow = new QWidget(palCard);
    auto* palBtnLay = new QHBoxLayout(palBtnRow);
    palBtnLay->setContentsMargins(0,0,0,0);
    palBtnLay->setSpacing(6);

    /* Raccoglie i bottoni per poter aggiornare la palette completa */
    QVector<QPushButton*> palBtns;
    for (int pi = 0; pi < 8; pi++) {
        QColor init = (canvas && canvas->style().palette.size() > pi)
                       ? canvas->style().palette[pi]
                       : kDefaultPal[pi];
        auto* pb = makeColorBtn(palBtnRow, init, [canvas, pi, saveStyle, &palBtns](QColor c){
            if (!canvas) return;
            auto& pal = canvas->style().palette;
            if (pal.size() < 8) {
                /* inizializza con i default */
                pal.clear();
                for (const QColor& dc : kDefaultPal) pal << dc;
            }
            if (pi < pal.size()) pal[pi] = c;
            canvas->update();
            saveStyle();
        });
        palBtns.append(pb);
        palBtnLay->addWidget(pb);
    }
    palBtnLay->addStretch();
    palLay->addWidget(palBtnRow);

    auto* btnResetPal = new QPushButton(
        "\xe2\x86\xaa  Ripristina palette default", palCard);  /* ↪ */
    btnResetPal->setObjectName("actionBtn");
    QObject::connect(btnResetPal, &QPushButton::clicked, palCard,
        [canvas, saveStyle, palBtns]() mutable {
            if (!canvas) return;
            canvas->style().palette.clear();
            canvas->update();
            saveStyle();
            /* aggiorna aspetto bottoni */
            for (int pi = 0; pi < palBtns.size(); pi++) {
                palBtns[pi]->setStyleSheet(
                    QString("QPushButton { background:%1; border:1px solid rgba(255,255,255,0.25);"
                            " border-radius:4px; }").arg(kDefaultPal[pi].name()));
                palBtns[pi]->setProperty("currentColor", kDefaultPal[pi].name());
            }
        });
    palLay->addWidget(btnResetPal);
    outer->addWidget(palCard);

    /* ── Sezione 4: Font ───────────────────────────────────────── */
    auto* fntCard = new QFrame(inner);
    fntCard->setObjectName("cardFrame");
    auto* fntLay  = new QVBoxLayout(fntCard);
    fntLay->setContentsMargins(14,12,14,12);
    fntLay->setSpacing(10);

    auto* fntTitle = new QLabel("\xf0\x9f\x94\xa4  Carattere etichette", fntCard);  /* 🔤 */
    fntTitle->setObjectName("cardTitle");
    fntLay->addWidget(fntTitle);

    auto* fntForm = new QFormLayout;
    fntForm->setSpacing(8);
    fntForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto* cmbFont = new QComboBox(fntCard);
    cmbFont->setObjectName("settingCombo");
    cmbFont->addItems({"Inter, Ubuntu, sans-serif",
                       "JetBrains Mono, Fira Code, monospace",
                       "Georgia, serif",
                       "Courier New, monospace",
                       "Arial, Helvetica, sans-serif"});
    if (canvas) {
        int fi = cmbFont->findText(canvas->style().fontFamily, Qt::MatchContains);
        if (fi >= 0) cmbFont->setCurrentIndex(fi);
    }
    QObject::connect(cmbFont, &QComboBox::currentTextChanged, fntCard,
        [canvas, saveStyle](const QString& t){
            if (canvas) { canvas->style().fontFamily = t; canvas->update(); saveStyle(); }
        });
    fntForm->addRow("Famiglia:", cmbFont);

    auto* spnSize = new QSpinBox(fntCard);
    spnSize->setRange(6, 24);
    spnSize->setValue(canvas ? canvas->style().fontSize : 8);
    spnSize->setObjectName("settingCombo");
    QObject::connect(spnSize, QOverload<int>::of(&QSpinBox::valueChanged), fntCard,
        [canvas, saveStyle](int v){
            if (canvas) { canvas->style().fontSize = v; canvas->update(); saveStyle(); }
        });
    fntForm->addRow("Dimensione:", spnSize);

    fntLay->addLayout(fntForm);
    outer->addWidget(fntCard);

    /* ── Sezione 5: Preset temi ────────────────────────────────── */
    auto* preCard = new QFrame(inner);
    preCard->setObjectName("cardFrame");
    auto* preLay  = new QVBoxLayout(preCard);
    preLay->setContentsMargins(14,12,14,12);
    preLay->setSpacing(10);

    auto* preTitle = new QLabel("\xe2\x9c\xa8  Preset stile grafico", preCard);  /* ✨ */
    preTitle->setObjectName("cardTitle");
    preLay->addWidget(preTitle);

    struct Preset {
        const char* name;
        QColor bg, axis, grid, text;
        QVector<QColor> pal;
    };
    static const Preset kPresets[] = {
        { "Scuro (default)",
          {0x18,0x18,0x18}, {0x55,0x55,0x55}, {0x35,0x35,0x35}, {0x99,0x99,0x99},
          {} },
        { "Chiaro",
          {0xf8,0xf9,0xfa}, {0x99,0x99,0x99}, {0xdd,0xdd,0xdd}, {0x44,0x44,0x44},
          {{0x1a,0x73,0xe8},{0xd9,0x30,0x25},{0x18,0x8a,0x38},
           {0xfb,0x8c,0x00},{0x8e,0x24,0xaa},{0x00,0x89,0x7b},
           {0x00,0x78,0xd4},{0xe6,0x4a,0x19}} },
        { "Sepia",
          {0x2c,0x24,0x1a}, {0x80,0x6a,0x50}, {0x4a,0x38,0x28}, {0xc4,0xa8,0x80},
          {{0xd4,0x8a,0x2a},{0xa0,0x5a,0x2a},{0x8a,0xb8,0x70},
           {0xd4,0xbe,0x7a},{0x9a,0x70,0xb8},{0xd4,0x80,0x80},
           {0x70,0xb8,0xc0},{0xd4,0xa0,0x60}} },
        { "Matrix",
          {0x00,0x0d,0x00}, {0x00,0x66,0x00}, {0x00,0x33,0x00}, {0x00,0xcc,0x00},
          {{0x00,0xff,0x41},{0x00,0xcc,0x33},{0x39,0xff,0x14},
           {0x00,0xff,0x7f},{0x7f,0xff,0x00},{0x00,0xe5,0xff},
           {0xad,0xff,0x2f},{0x00,0xff,0xd7}} },
        { "Blueprint",
          {0x0a,0x14,0x2e}, {0x3a,0x6e,0xd4}, {0x1a,0x3a,0x70}, {0x6a,0xaa,0xff},
          {{0x4d,0xa6,0xff},{0xff,0x7f,0x50},{0x66,0xff,0xb3},
           {0xff,0xd7,0x00},{0xcc,0x99,0xff},{0xff,0x66,0x99},
           {0x66,0xcc,0xff},{0xff,0xa0,0x40}} },
    };

    auto* preRow = new QWidget(preCard);
    auto* preRowLay = new QHBoxLayout(preRow);
    preRowLay->setContentsMargins(0,0,0,0);
    preRowLay->setSpacing(8);

    for (const Preset& pr : kPresets) {
        auto* pb = new QPushButton(pr.name, preCard);
        pb->setObjectName("actionBtn");
        const Preset prCopy = pr;
        QObject::connect(pb, &QPushButton::clicked, preCard,
            [canvas, prCopy, saveStyle,
             btnBg, btnAxis, btnGrid, btnText,
             chkGrid, palBtns, cmbFont, spnSize]() mutable {
                if (!canvas) return;
                auto& st = canvas->style();
                st.bgColor   = prCopy.bg;
                st.axisColor = prCopy.axis;
                st.gridColor = prCopy.grid;
                st.textColor = prCopy.text;
                st.palette   = prCopy.pal;
                st.showGrid  = true;
                canvas->update();
                saveStyle();
                /* Aggiorna UI */
                auto setBtn = [](QPushButton* b, QColor c) {
                    b->setStyleSheet(
                        QString("QPushButton { background:%1; border:1px solid rgba(255,255,255,0.25);"
                                " border-radius:4px; }").arg(c.name()));
                    b->setProperty("currentColor", c.name());
                };
                setBtn(btnBg,   prCopy.bg);
                setBtn(btnAxis, prCopy.axis);
                setBtn(btnGrid, prCopy.grid);
                setBtn(btnText, prCopy.text);
                chkGrid->setChecked(true);
                static const QColor kDefaultPal[] = {
                    {0x00,0xbf,0xd8},{0xff,0x79,0x5a},{0x73,0xe2,0x73},{0xff,0xd7,0x6e},
                    {0xb0,0x90,0xff},{0xff,0x82,0xc2},{0x5a,0xd7,0xff},{0xff,0xa0,0x50}
                };
                for (int pi = 0; pi < palBtns.size(); pi++) {
                    QColor c = (prCopy.pal.size() > pi) ? prCopy.pal[pi] : kDefaultPal[pi];
                    setBtn(palBtns[pi], c);
                }
            });
        preRowLay->addWidget(pb);
    }
    preRowLay->addStretch();
    preLay->addWidget(preRow);
    outer->addWidget(preCard);

    /* ── Reset totale ──────────────────────────────────────────── */
    auto* btnReset = new QPushButton(
        "\xf0\x9f\x94\x84  Ripristina tutto ai valori default", inner);  /* 🔄 */
    btnReset->setObjectName("actionBtn");
    QObject::connect(btnReset, &QPushButton::clicked, inner,
        [canvas, saveStyle,
         btnBg, btnAxis, btnGrid, btnText,
         chkGrid, palBtns, cmbFont, spnSize]() mutable {
            if (!canvas) return;
            GraficoCanvas::ChartStyle def;
            canvas->setStyle(def);
            saveStyle();
            auto setBtn = [](QPushButton* b, QColor c) {
                b->setStyleSheet(
                    QString("QPushButton { background:%1; border:1px solid rgba(255,255,255,0.25);"
                            " border-radius:4px; }").arg(c.name()));
                b->setProperty("currentColor", c.name());
            };
            setBtn(btnBg,   def.bgColor);
            setBtn(btnAxis, def.axisColor);
            setBtn(btnGrid, def.gridColor);
            setBtn(btnText, def.textColor);
            chkGrid->setChecked(def.showGrid);
            static const QColor kDefaultPal[] = {
                {0x00,0xbf,0xd8},{0xff,0x79,0x5a},{0x73,0xe2,0x73},{0xff,0xd7,0x6e},
                {0xb0,0x90,0xff},{0xff,0x82,0xc2},{0x5a,0xd7,0xff},{0xff,0xa0,0x50}
            };
            for (int pi = 0; pi < palBtns.size(); pi++)
                setBtn(palBtns[pi], kDefaultPal[pi]);
            cmbFont->setCurrentIndex(0);
            spnSize->setValue(8);
        });
    outer->addWidget(btnReset);
    outer->addStretch();

    return page;
}

/* ══════════════════════════════════════════════════════════════
   setGraficoCanvas — aggiunge il tab Grafico con il canvas reale
   ══════════════════════════════════════════════════════════════ */
void ImpostazioniPage::setGraficoCanvas(GraficoCanvas* canvas)
{
    if (!m_tabVisuale || !canvas) return;
    /* Evita duplicati se chiamato più volte */
    for (int i = 0; i < m_tabVisuale->count(); i++) {
        if (m_tabVisuale->tabText(i).contains("Grafico", Qt::CaseInsensitive))
            return;
    }
    m_tabVisuale->addTab(buildGraficoTab(canvas), "\xf0\x9f\x93\x88  Grafico");
}

/* ══════════════════════════════════════════════════════════════
   buildAiLocaleTab — layout a 2 colonne:
     Sinistra : selettore gestore LLM (Ollama / llama.cpp)
     Destra   : modelli disponibili per il gestore selezionato

   Ollama  → fetchModels() → QStringList con nome modello
   llama.cpp → P::scanGgufFiles() → path assoluti file .gguf
   ══════════════════════════════════════════════════════════════ */
