#include "impostazioni_page.h"
#include "../widgets/stt_whisper.h"
#include "personalizza_page.h"
#include "manutenzione_page.h"
#include "grafico_page.h"
#include "../theme_manager.h"
#include "../prismalux_paths.h"
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
#include <QSettings>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QFileDialog>
#include <QDateTime>
#include <QStandardPaths>
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

/* ══════════════════════════════════════════════════════════════
   ImpostazioniPage — 6 tab tematiche (da 11 originali).
   Nomi comprensibili anche per utenti non esperti.
   Nessun QTabWidget annidato (evita rendering blank in Qt6).
   ══════════════════════════════════════════════════════════════ */
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

    /* ────────────────────────────────────────────────────────────
       Tab 1: 🔌 Connessione — Backend (compatto, nessun scroll)
       ──────────────────────────────────────────────────────────── */
    tabs->addTab(m_manutenzione->buildBackend(), "\xf0\x9f\x94\x8c  Connessione");

    /* ────────────────────────────────────────────────────────────
       Tab 2: 🖥 Hardware — Rilevamento hw
       ──────────────────────────────────────────────────────────── */
    tabs->addTab(m_manutenzione->buildHardware(), "\xf0\x9f\x96\xa5  Hardware");

    /* ────────────────────────────────────────────────────────────
       Tab 3: 🦙 AI Locale — selezione gestore + modelli disponibili
       ──────────────────────────────────────────────────────────── */
    tabs->addTab(buildAiLocaleTab(), "\xf0\x9f\xa6\x99  AI Locale");

    /* ────────────────────────────────────────────────────────────
       Tab 4: 🤖 LLM — catalogo compatto Ollama + GGUF (filtro+lista)
       ──────────────────────────────────────────────────────────── */
    tabs->addTab(buildLlmConsigliatiTab(), "\xf0\x9f\xa4\x96  LLM");

    /* ────────────────────────────────────────────────────────────
       Tab 5: 📊 Classifica — ranking oggettivo modelli open-weight
       Basato su: ArtificialAnalysis.ai + benchmark locali Prismalux
       ──────────────────────────────────────────────────────────── */
    tabs->addTab(buildLlmClassificaTab(), "\xf0\x9f\x93\x8a  Classifica");

    /* ────────────────────────────────────────────────────────────
       Tab 4: 🎤 Voce & Audio — TTS (sinistra) + STT (destra)
       Layout 2 colonne — nessun scroll, tutto visibile.
       ──────────────────────────────────────────────────────────── */
    {
        auto* outer = new QWidget;
        auto* oLay  = new QHBoxLayout(outer);
        oLay->setContentsMargins(14, 12, 14, 12);
        oLay->setSpacing(14);
        oLay->addWidget(buildVoceTab());
        oLay->addWidget(buildTrascriviTab());
        tabs->addTab(outer, "\xf0\x9f\x8e\xa4  Voce & Audio");
    }

    /* ────────────────────────────────────────────────────────────
       Tab 5: 🎨 Aspetto — Selezione tema visivo
       ──────────────────────────────────────────────────────────── */
    tabs->addTab(buildTemaTab(), "\xf0\x9f\x8e\xa8  Aspetto");

    /* ────────────────────────────────────────────────────────────
       Tab 6: 📊 Avanzate — Monitor AI (solo MonitorPanel)
       ──────────────────────────────────────────────────────────── */
    {
        auto* outer = new QWidget;
        auto* oLay  = new QVBoxLayout(outer);
        oLay->setContentsMargins(0, 0, 0, 0);
        oLay->setSpacing(0);

        auto* monPanel = new MonitorPanel(ai, hw, outer);
        monPanel->setWindowFlags(Qt::Widget);
        monPanel->setMinimumSize(0, 0);
        oLay->addWidget(monPanel, 1);

        tabs->addTab(outer, "\xf0\x9f\x93\x8a  Avanzate");
    }

    /* ────────────────────────────────────────────────────────────
       Tab 7: 📚 RAG — Recupero Documenti Aumentato (tab dedicato)
       ──────────────────────────────────────────────────────────── */
    {
        auto* scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidget(buildRagTab());
        tabs->addTab(scroll, "\xf0\x9f\x93\x9a  RAG");
    }

    /* ────────────────────────────────────────────────────────────
       Tab 8: 🧪 Test — Registro suite di test (tab dedicato)
       ──────────────────────────────────────────────────────────── */
    {
        auto* scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        auto* inner  = new QWidget;
        auto* iLay   = new QVBoxLayout(inner);
        iLay->setContentsMargins(0, 0, 0, 0);
        iLay->setSpacing(0);
        iLay->addWidget(buildTestTab());
        iLay->addStretch();
        scroll->setWidget(inner);
        tabs->addTab(scroll, "\xf0\x9f\xa7\xaa  Test");
    }

    /* ────────────────────────────────────────────────────────────
       Tab 9: ⚙️ Parametri AI — temperature, repeat_penalty, ecc.
       ──────────────────────────────────────────────────────────── */
    {
        auto* scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidget(buildAiParamsTab());
        tabs->addTab(scroll, "\xe2\x9a\x99\xef\xb8\x8f  Parametri AI");
    }

    lay->addWidget(tabs);
}

/* ══════════════════════════════════════════════════════════════
   switchToTab — porta in primo piano il tab che contiene @p name
   ══════════════════════════════════════════════════════════════ */
void ImpostazioniPage::switchToTab(const QString& name)
{
    if (!m_tabs) return;
    /* "LLM" ha ora il suo tab dedicato — ricerca diretta */
    const QString resolved = name;
    for (int i = 0; i < m_tabs->count(); i++) {
        if (m_tabs->tabText(i).contains(resolved, Qt::CaseInsensitive)) {
            m_tabs->setCurrentIndex(i);
            return;
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

    /* ── Helper: salva stile in QSettings + aggiorna preview ── */
    auto saveStyle = [canvas, preview]() {
        if (!canvas) return;
        const auto& s = canvas->style();
        QSettings cfg("Prismalux", "GUI");
        cfg.beginGroup("ChartStyle");
        cfg.setValue("bgColor",   s.bgColor.name(QColor::HexArgb));
        cfg.setValue("axisColor", s.axisColor.name(QColor::HexArgb));
        cfg.setValue("gridColor", s.gridColor.name(QColor::HexArgb));
        cfg.setValue("textColor", s.textColor.name(QColor::HexArgb));
        cfg.setValue("fontFamily",s.fontFamily);
        cfg.setValue("fontSize",  s.fontSize);
        cfg.setValue("showGrid",  s.showGrid);
        QStringList pal;
        for (const QColor& c : s.palette) pal << c.name(QColor::HexArgb);
        cfg.setValue("palette", pal);
        cfg.endGroup();
        if (preview) preview->refresh();
    };

    /* ── Carica stile salvato all'avvio ── */
    if (canvas) {
        QSettings cfg("Prismalux", "GUI");
        cfg.beginGroup("ChartStyle");
        GraficoCanvas::ChartStyle s;
        auto loadColor = [&](const char* key, QColor def) {
            QString v = cfg.value(key).toString();
            return v.isEmpty() ? def : QColor(v);
        };
        s.bgColor    = loadColor("bgColor",   s.bgColor);
        s.axisColor  = loadColor("axisColor", s.axisColor);
        s.gridColor  = loadColor("gridColor", s.gridColor);
        s.textColor  = loadColor("textColor", s.textColor);
        s.fontFamily = cfg.value("fontFamily", s.fontFamily).toString();
        s.fontSize   = cfg.value("fontSize",   s.fontSize).toInt();
        s.showGrid   = cfg.value("showGrid",   s.showGrid).toBool();
        const QStringList pal = cfg.value("palette").toStringList();
        for (const QString& c : pal) s.palette << QColor(c);
        cfg.endGroup();
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
    if (!m_tabs || !canvas) return;
    /* Evita duplicati se chiamato più volte */
    for (int i = 0; i < m_tabs->count(); i++) {
        if (m_tabs->tabText(i).contains("Grafico", Qt::CaseInsensitive))
            return;
    }
    m_tabs->addTab(buildGraficoTab(canvas), "\xf0\x9f\x93\x88  Grafico");
}

/* ══════════════════════════════════════════════════════════════
   buildAiLocaleTab — layout a 2 colonne:
     Sinistra : selettore gestore LLM (Ollama / llama.cpp)
     Destra   : modelli disponibili per il gestore selezionato

   Ollama  → fetchModels() → QStringList con nome modello
   llama.cpp → P::scanGgufFiles() → path assoluti file .gguf
   ══════════════════════════════════════════════════════════════ */
QWidget* ImpostazioniPage::buildAiLocaleTab()
{
    auto* page    = new QWidget;
    auto* mainLay = new QVBoxLayout(page);
    mainLay->setContentsMargins(16, 16, 16, 12);
    mainLay->setSpacing(12);

    /* Titolo */
    auto* titleLbl = new QLabel("\xf0\x9f\xa6\x99  AI Locale \xe2\x80\x94 Gestori & Modelli", page);
    titleLbl->setObjectName("sectionTitle");
    mainLay->addWidget(titleLbl);

    /* ── 2 colonne ── */
    auto* colsRow = new QWidget(page);
    auto* colsLay = new QHBoxLayout(colsRow);
    colsLay->setContentsMargins(0, 0, 0, 0);
    colsLay->setSpacing(16);

    /* ── Colonna sinistra: selettore gestore ── */
    auto* leftGroup = new QGroupBox("\xf0\x9f\x94\xa7  Gestore LLM", colsRow);
    leftGroup->setObjectName("cardGroup");
    leftGroup->setFixedWidth(200);
    auto* leftLay = new QVBoxLayout(leftGroup);
    leftLay->setSpacing(10);

    auto* btnOllama = new QRadioButton("\xf0\x9f\x90\xb3  Ollama", leftGroup);
    btnOllama->setChecked(true);
    auto* btnLlama  = new QRadioButton("\xf0\x9f\xa6\x99  llama.cpp", leftGroup);

    leftLay->addWidget(btnOllama);
    leftLay->addWidget(btnLlama);

    auto* sepLeft = new QFrame(leftGroup);
    sepLeft->setFrameShape(QFrame::HLine);
    sepLeft->setObjectName("sidebarSep");
    leftLay->addWidget(sepLeft);

    auto* statusLbl = new QLabel("", leftGroup);
    statusLbl->setObjectName("cardDesc");
    statusLbl->setWordWrap(true);
    leftLay->addWidget(statusLbl);

    auto* refreshBtn = new QPushButton("\xf0\x9f\x94\x84  Aggiorna lista", leftGroup);
    refreshBtn->setObjectName("actionBtn");
    leftLay->addWidget(refreshBtn);

    /* llama.cpp Studio rimosso: funzionalità integrate in scheda LLM */

    leftLay->addStretch(1);
    colsLay->addWidget(leftGroup);

    /* ── Colonna destra: lista modelli ── */
    auto* rightGroup = new QGroupBox("\xf0\x9f\x93\xa6  Modelli disponibili", colsRow);
    rightGroup->setObjectName("cardGroup");
    auto* rightLay = new QVBoxLayout(rightGroup);
    rightLay->setSpacing(8);

    /* Hint sopra la lista */
    auto* hintLbl = new QLabel("Seleziona un gestore per vedere i modelli installati.", rightGroup);
    hintLbl->setObjectName("hintLabel");
    hintLbl->setWordWrap(true);
    rightLay->addWidget(hintLbl);

    auto* modelList = new QListWidget(rightGroup);
    modelList->setObjectName("modelsList");
    modelList->setAlternatingRowColors(true);
    rightLay->addWidget(modelList, 1);

    colsLay->addWidget(rightGroup, 1);
    mainLay->addWidget(colsRow);

    /* ── Dipendenze — occupa tutto lo spazio residuo ── */
    auto* sepBot = new QFrame(page);
    sepBot->setFrameShape(QFrame::HLine);
    sepBot->setObjectName("sidebarSep");
    mainLay->addWidget(sepBot);

    mainLay->addWidget(buildDipendenzeTab(), 1);

    /* ── Logica: popola la lista in base al gestore selezionato ── */

    /* Label che mostra il modello attivo corrente */
    auto* activeLbl = new QLabel(
        QString("\xf0\x9f\x9f\xa2  Modello attivo: <b>%1</b>").arg(
            m_ai->model().isEmpty() ? "(nessuno)" : m_ai->model()),
        leftGroup);
    activeLbl->setObjectName("cardDesc");
    activeLbl->setWordWrap(true);
    activeLbl->setTextFormat(Qt::RichText);
    leftLay->insertWidget(2, activeLbl);  /* sotto i radio button */

    /* Pulsante "Usa modello selezionato" */
    auto* useBtn = new QPushButton("\xe2\x9c\x94  Usa modello", leftGroup);
    useBtn->setObjectName("actionBtn");
    useBtn->setEnabled(false);
    useBtn->setToolTip("Imposta il modello selezionato come modello attivo");
    leftLay->insertWidget(3, useBtn);

    /* Lambda: applica il modello selezionato e lo persiste su QSettings */
    auto applySelected = [=]() {
        auto* cur = modelList->currentItem();
        if (!cur) return;
        const QString model = cur->data(Qt::UserRole).toString();
        if (model.isEmpty() || model.startsWith("(")) return;
        m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), model);
        /* Salva su QSettings → sopravvive al riavvio */
        QSettings s("Prismalux", "GUI");
        s.setValue(PrismaluxPaths::SK::kActiveModel,   model);
        s.setValue(PrismaluxPaths::SK::kActiveBackend, static_cast<int>(m_ai->backend()));
        activeLbl->setText(QString("\xf0\x9f\x9f\xa2  Modello attivo: <b>%1</b>").arg(model));
        statusLbl->setText(QString("\xe2\x9c\x85 Attivo: %1").arg(model));
    };

    /* Lambda: carica modelli Ollama tramite AiClient::fetchModels */
    auto populateOllama = [=]() {
        modelList->clear();
        hintLbl->setText("Caricamento modelli Ollama...");
        statusLbl->setText("\xe2\x8f\xb3 Ollama...");
        useBtn->setEnabled(false);
        auto* holder = new QObject(page);
        connect(m_ai, &AiClient::modelsReady, holder,
                [=](const QStringList& list) {
            holder->deleteLater();
            modelList->clear();
            if (list.isEmpty()) {
                statusLbl->setText("\xe2\x9d\x8c Nessun modello");
                hintLbl->setText("Ollama non raggiungibile o nessun modello installato.\n"
                                 "Avvia Ollama e riprova.");
                auto* ph = new QListWidgetItem("(Ollama non raggiungibile)");
                modelList->addItem(ph);
            } else {
                statusLbl->setText(QString("\xe2\x9c\x85 %1 modelli").arg(list.size()));
                hintLbl->setText(QString("%1 modelli Ollama. Doppio clic o 'Usa modello' per attivare.")
                                 .arg(list.size()));
                for (const QString& m : list) {
                    auto* item = new QListWidgetItem(m);
                    item->setData(Qt::UserRole, m);  /* modello = nome Ollama */
                    /* Evidenzia il modello attivo */
                    if (m == m_ai->model())
                        item->setForeground(QColor("#4ade80"));
                    modelList->addItem(item);
                }
            }
        });
        m_ai->fetchModels();
    };

    /* Lambda: scansiona file .gguf dalla cartella models/ */
    auto populateLlama = [=]() {
        modelList->clear();
        useBtn->setEnabled(false);
        const QStringList files = PrismaluxPaths::scanGgufFiles();
        if (files.isEmpty()) {
            statusLbl->setText("0 modelli");
            hintLbl->setText("Nessun file .gguf trovato.\n"
                             "Scarica un modello dalla scheda LLM.");
            auto* ph = new QListWidgetItem("(Nessun modello GGUF nella cartella models/)");
            modelList->addItem(ph);
        } else {
            statusLbl->setText(QString("%1 file GGUF").arg(files.size()));
            hintLbl->setText(QString("%1 modelli GGUF. Doppio clic o 'Usa modello' per attivare.")
                             .arg(files.size()));
            for (const QString& path : files) {
                QFileInfo fi(path);
                double mb = fi.size() / 1024.0 / 1024.0;
                QString szStr = mb >= 1024.0
                    ? QString("%1 GB").arg(mb / 1024.0, 0, 'f', 1)
                    : QString("%1 MB").arg(mb, 0, 'f', 0);
                auto* item = new QListWidgetItem(
                    QString("%1   \xe2\x80\x94  %2").arg(fi.fileName(), szStr));
                item->setData(Qt::UserRole, path);  /* UserRole = path completo */
                if (path == m_ai->model())
                    item->setForeground(QColor("#4ade80"));
                modelList->addItem(item);
            }
        }
    };

    /* Cambio gestore */
    connect(btnOllama, &QRadioButton::toggled, page, [=](bool checked) {
        if (!checked) return;
        populateOllama();
    });
    connect(btnLlama, &QRadioButton::toggled, page, [=](bool checked) {
        if (!checked) return;
        populateLlama();
    });

    /* Aggiorna lista */
    connect(refreshBtn, &QPushButton::clicked, page, [=]() {
        if (btnOllama->isChecked()) populateOllama();
        else populateLlama();
    });

    /* Abilita "Usa modello" quando si seleziona un item */
    connect(modelList, &QListWidget::currentItemChanged, page,
            [=](QListWidgetItem* cur, QListWidgetItem*) {
        if (!cur) { useBtn->setEnabled(false); return; }
        const QString m = cur->data(Qt::UserRole).toString();
        useBtn->setEnabled(!m.isEmpty() && !m.startsWith("("));
    });

    /* Doppio clic = attiva subito */
    connect(modelList, &QListWidget::itemDoubleClicked, page,
            [=](QListWidgetItem*) { applySelected(); });

    /* Pulsante "Usa modello" */
    connect(useBtn, &QPushButton::clicked, page, applySelected);

    /* Aggiorna label modello attivo quando cambia dall'esterno */
    connect(m_ai, &AiClient::modelsReady, page, [=](const QStringList&) {
        activeLbl->setText(QString("\xf0\x9f\x9f\xa2  Modello attivo: <b>%1</b>").arg(
            m_ai->model().isEmpty() ? "(nessuno)" : m_ai->model()));
    });

    /* Popola Ollama subito all'apertura */
    QTimer::singleShot(0, page, [=]() { populateOllama(); });

    return page;
}

/* ══════════════════════════════════════════════════════════════
   buildDipendenzeTab — lista dipendenze esterne con verifica
   ══════════════════════════════════════════════════════════════ */
QWidget* ImpostazioniPage::buildDipendenzeTab()
{
    struct Dep {
        const char* name;
        const char* exec;     /* eseguibile da cercare — vuoto = sempre OK */
        const char* desc;
        const char* install;
        bool        optional;
    };

    static const Dep kDeps[] = {
        /* ── Backend AI ── */
        { "Qt6 / Qt5",    "",              "Framework GUI — Core, Widgets, Network",
          "sudo apt install qt6-base-dev libqt6network6-dev",         false },
        { "Ollama",       "ollama",        "Server LLM locale (NDJSON streaming)",
          "curl -fsSL https://ollama.com/install.sh | sh",            false },
        { "llama-server", "llama-server",  "Server llama.cpp alternativo a Ollama (SSE)",
          "./build.sh  nella cartella C_software",                     true  },
        { "llama-cli",    "llama-cli",     "Chat diretta con modelli GGUF offline",
          "./build.sh  nella cartella C_software",                     true  },

        /* ── Documenti ── */
        { "pdftotext",    "pdftotext",     "Estrae testo da PDF (Strumenti > Documenti)",
          "sudo apt install poppler-utils",                            false },

        /* ── Download modelli ── */
        { "wget",         "wget",          "Scarica modelli GGUF da HuggingFace",
          "sudo apt install wget",                                     false },

        /* ── Audio — TTS ── */
        { "aplay",        "aplay",         "Riproduce audio PCM raw (sintesi vocale)",
          "sudo apt install alsa-utils",                               false },
        { "piper",        "piper",         "Text-to-Speech offline ONNX (voce italiana)",
          "https://github.com/rhasspy/piper  — scarica il binario",   true  },
        { "espeak-ng",    "espeak-ng",     "TTS fallback leggero (se Piper assente)",
          "sudo apt install espeak-ng",                                true  },
        { "spd-say",      "spd-say",       "TTS fallback via speech-dispatcher",
          "sudo apt install speech-dispatcher",                        true  },

        /* ── Audio — STT ── */
        { "arecord",      "arecord",       "Registra audio per riconoscimento vocale",
          "sudo apt install alsa-utils",                               false },
        { "whisper-cli",  "whisper-cli",   "Riconoscimento vocale offline (whisper.cpp)",
          "sudo apt install whisper-cpp",                              true  },

        /* ── GPU / Hardware ── */
        { "nvidia-smi",   "nvidia-smi",    "Info GPU NVIDIA e VRAM benchmark",
          "Installare i driver NVIDIA proprietari",                    true  },
    };

    /* ── Costruzione pagina ── */
    auto* page  = new QWidget;
    auto* outer = new QVBoxLayout(page);
    outer->setContentsMargins(16, 16, 16, 16);
    outer->setSpacing(10);

    auto* title = new QLabel(
        "\xf0\x9f\x93\xa6  Dipendenze esterne");
    title->setObjectName("sectionTitle");
    outer->addWidget(title);

    auto* desc = new QLabel(
        "Librerie e strumenti necessari o opzionali. "
        "Premi \xc2\xab Verifica tutto \xc2\xbb per controllare "
        "cosa \xc3\xa8 disponibile nel sistema.");
    desc->setWordWrap(true);
    desc->setObjectName("hintLabel");
    outer->addWidget(desc);

    auto* verifyBtn = new QPushButton(
        "\xf0\x9f\x94\x8d  Verifica tutto");
    verifyBtn->setObjectName("actionBtn");
    verifyBtn->setFixedWidth(160);
    outer->addWidget(verifyBtn, 0, Qt::AlignLeft);

    /* ── Scroll area ── */
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* container  = new QWidget;
    auto* listLayout = new QVBoxLayout(container);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->setSpacing(2);

    /* Intestazione colonne */
    {
        auto* hdr    = new QWidget;
        auto* hdrLay = new QHBoxLayout(hdr);
        hdrLay->setContentsMargins(8, 2, 8, 2);
        hdrLay->setSpacing(10);
        auto makeHdr = [](const QString& t, int w = -1) {
            auto* l = new QLabel("<b>" + t + "</b>");
            l->setObjectName("hintLabel");
            if (w > 0) l->setFixedWidth(w);
            return l;
        };
        hdrLay->addWidget(makeHdr("", 20));
        hdrLay->addWidget(makeHdr("Nome", 160));
        hdrLay->addWidget(makeHdr("Descrizione"), 1);
        hdrLay->addWidget(makeHdr("Installazione", 260));
        listLayout->addWidget(hdr);
    }

    /* Separatore */
    auto* sepTop = new QFrame;
    sepTop->setFrameShape(QFrame::HLine);
    sepTop->setObjectName("sidebarSep");
    listLayout->addWidget(sepTop);

    /* Raccoglie i puntatori ai label di stato per aggiornarli */
    QVector<QLabel*>  statusDots;
    QVector<QString>  execs;

    const int nDeps = static_cast<int>(sizeof(kDeps) / sizeof(kDeps[0]));
    for (int i = 0; i < nDeps; ++i) {
        const Dep& d = kDeps[i];

        auto* row    = new QWidget;
        auto* rowLay = new QHBoxLayout(row);
        rowLay->setContentsMargins(8, 5, 8, 5);
        rowLay->setSpacing(10);
        row->setObjectName((i % 2 == 0) ? "depRowEven" : "depRowOdd");

        /* Pallino stato */
        auto* dot = new QLabel("\xe2\x97\x8f");   /* ● grigio */
        dot->setFixedWidth(20);
        dot->setAlignment(Qt::AlignCenter);
        dot->setStyleSheet("color:#4a5568;font-size:10px;");

        /* Nome + "(opzionale)" */
        QString nameHtml = QString("<b>%1</b>").arg(d.name);
        if (d.optional)
            nameHtml += " <span style='color:#4a5568;font-size:10px;'>"
                        "(opzionale)</span>";
        auto* nameLbl = new QLabel(nameHtml);
        nameLbl->setTextFormat(Qt::RichText);
        nameLbl->setFixedWidth(160);

        /* Descrizione */
        auto* descLbl = new QLabel(d.desc);
        descLbl->setObjectName("hintLabel");

        /* Comando di installazione */
        auto* installLbl = new QLabel(
            QString("<code style='color:#4a5568;font-size:11px;'>%1</code>")
                .arg(d.install));
        installLbl->setTextFormat(Qt::RichText);
        installLbl->setFixedWidth(260);
        installLbl->setWordWrap(true);

        rowLay->addWidget(dot);
        rowLay->addWidget(nameLbl);
        rowLay->addWidget(descLbl, 1);
        rowLay->addWidget(installLbl);

        listLayout->addWidget(row);
        statusDots.append(dot);
        execs.append(QString::fromUtf8(d.exec));
    }
    listLayout->addStretch();
    scroll->setWidget(container);
    outer->addWidget(scroll, 1);

    /* Legenda */
    auto* legend = new QLabel(
        "\xe2\x97\x8f  Non verificato \xc2\xa0\xc2\xa0"
        "<span style='color:#4ade80;'>\xe2\x97\x8f</span>  Trovato \xc2\xa0\xc2\xa0"
        "<span style='color:#f87171;'>\xe2\x97\x8f</span>  Non trovato");
    legend->setTextFormat(Qt::RichText);
    legend->setObjectName("hintLabel");
    outer->addWidget(legend);

    /* ── Connessione verifica ── */
    QObject::connect(verifyBtn, &QPushButton::clicked, verifyBtn,
        [statusDots, execs]() {
        for (int i = 0; i < statusDots.size(); ++i) {
            const QString& ex = execs[i];
            if (ex.isEmpty()) {
                /* Qt / sistema — sempre disponibile */
                statusDots[i]->setStyleSheet(
                    "color:#4ade80;font-size:10px;");   /* verde */
                continue;
            }
            QString found = QStandardPaths::findExecutable(ex);
            if (!found.isEmpty()) {
                statusDots[i]->setStyleSheet(
                    "color:#4ade80;font-size:10px;");   /* verde */
            } else {
                statusDots[i]->setStyleSheet(
                    "color:#f87171;font-size:10px;");   /* rosso */
            }
        }
    });

    return page;
}

/* ══════════════════════════════════════════════════════════════
   buildRagTab — configurazione RAG (Retrieval-Augmented Generation)
   ══════════════════════════════════════════════════════════════ */
QWidget* ImpostazioniPage::buildRagTab()
{
    auto* page  = new QWidget;
    auto* outer = new QVBoxLayout(page);
    outer->setContentsMargins(20, 20, 20, 20);
    outer->setSpacing(14);

    /* Titolo */
    auto* title = new QLabel("\xf0\x9f\x94\x8d  RAG \xe2\x80\x94 Recupero Documenti Aumentato");
    title->setObjectName("sectionTitle");
    outer->addWidget(title);

    /* Descrizione */
    auto* desc = new QLabel(
        "Base di conoscenza locale: configura la cartella da cui l'AI attinge "
        "per rispondere con fatti precisi dai tuoi documenti.");
    desc->setWordWrap(true);
    desc->setObjectName("hintLabel");
    outer->addWidget(desc);

    /* ── [M2] Warning privacy ── */
    auto* privacyLbl = new QLabel(
        "\xe2\x9a\xa0\xef\xb8\x8f  <b>Attenzione privacy</b>: i chunk indicizzati (frammenti di testo dai tuoi "
        "documenti) vengono salvati in chiaro in <code>~/.prismalux_rag.json</code>. "
        "Non indicizzare documenti riservati (dichiarazioni fiscali, dati medici) "
        "se non sei l'unico utente di questo sistema.");
    privacyLbl->setWordWrap(true);
    privacyLbl->setTextFormat(Qt::RichText);
    privacyLbl->setObjectName("hintLabel");
    privacyLbl->setStyleSheet("color: #e8a020;");   /* amber di avviso */
    outer->addWidget(privacyLbl);

    auto* noSaveChk = new QCheckBox(
        "Non salvare su disco (solo RAM \xe2\x80\x94 indice perso alla chiusura)");
    noSaveChk->setObjectName("cardDesc");
    noSaveChk->setToolTip(
        "Quando attivo, l'indice RAG viene costruito in memoria ma non scritto in\n"
        "~/.prismalux_rag.json. Utile per documenti riservati.\n"
        "L'indice va ricostruito ad ogni avvio dell'applicazione.");
    {
        QSettings s("Prismalux", "GUI");
        noSaveChk->setChecked(s.value(P::SK::kRagNoSave, false).toBool());
        m_ragNoSave = noSaveChk->isChecked();
    }
    connect(noSaveChk, &QCheckBox::toggled, this, [this](bool on){
        m_ragNoSave = on;
        QSettings s("Prismalux", "GUI");
        s.setValue(P::SK::kRagNoSave, on);
    });
    outer->addWidget(noSaveChk);

    /* ── Form ── */
    auto* fl = new QFormLayout;
    fl->setContentsMargins(0, 8, 0, 0);
    fl->setSpacing(10);
    fl->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    /* Cartella documenti */
    auto* dirRow = new QWidget;
    auto* dirLay = new QHBoxLayout(dirRow);
    dirLay->setContentsMargins(0, 0, 0, 0);
    dirLay->setSpacing(6);
    auto* dirEdit = new QLineEdit;
    dirEdit->setObjectName("inputLine");
    dirEdit->setPlaceholderText("/percorso/documenti/");
    {
        QSettings s("Prismalux", "GUI");
        dirEdit->setText(s.value(P::SK::kRagDocsDir, "").toString());
    }
    auto* browseBtn = new QPushButton("Sfoglia...");
    browseBtn->setObjectName("actionBtn");
    browseBtn->setFixedWidth(90);
    dirLay->addWidget(dirEdit, 1);
    dirLay->addWidget(browseBtn);
    fl->addRow("Cartella:", dirRow);

    /* Max risultati */
    auto* maxSpin = new QSpinBox;
    maxSpin->setRange(1, 20);
    {
        QSettings s("Prismalux", "GUI");
        maxSpin->setValue(s.value(P::SK::kRagMaxResults, 5).toInt());
    }
    maxSpin->setSuffix("  risultati");
    fl->addRow("Massimi:", maxSpin);

    outer->addLayout(fl);

    /* ── Trasformata Johnson-Lindenstrauss ── */
    {
        auto* jlFrame = new QGroupBox("Ottimizzazione vettori RAG", page);
        jlFrame->setObjectName("cardGroup");
        auto* jlLay = new QVBoxLayout(jlFrame);
        jlLay->setSpacing(6);

        auto* jlChk = new QCheckBox(
            "Trasformata di Johnson\xe2\x80\x93Lindenstrauss (JL)", jlFrame);
        jlChk->setObjectName("cardDesc");
        {
            QSettings s("Prismalux", "GUI");
            jlChk->setChecked(s.value(P::SK::kRagJlTransform, true).toBool());
        }
        jlChk->setToolTip(
            "Riduce la dimensionalit\xc3\xa0 dei vettori di embedding mantenendo le distanze.\n"
            "Migliora le performance di ricerca RAG con grandi basi di conoscenza.\n"
            "Consigliata: attiva.");

        auto* jlHint = new QLabel(
            "Comprime i vettori di embedding riducendo i tempi di ricerca senza "
            "perdita significativa di accuratezza. Consigliata con pi\xc3\xb9 di 1000 documenti.", jlFrame);
        jlHint->setObjectName("hintLabel");
        jlHint->setWordWrap(true);

        jlLay->addWidget(jlChk);
        jlLay->addWidget(jlHint);

        connect(jlChk, &QCheckBox::toggled, jlFrame, [](bool on){
            QSettings s("Prismalux", "GUI");
            s.setValue(P::SK::kRagJlTransform, on);
        });

        outer->addWidget(jlFrame);
    }

    /* ── Stato indice ── */
    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setObjectName("sidebarSep");
    outer->addWidget(sep);

    auto* statusLbl = new QLabel;
    statusLbl->setObjectName("hintLabel");
    statusLbl->setWordWrap(true);
    statusLbl->setTextFormat(Qt::RichText);
    /* refreshStatus legge dall'engine in-memory se disponibile (più accurato),
     * altrimenti cade su QSettings (valore salvato dall'ultima sessione). */
    auto refreshStatus = [this, statusLbl]() {
        QSettings ss("Prismalux", "GUI");
        int cnt = (m_rag.chunkCount() > 0)
                  ? m_rag.chunkCount()
                  : ss.value(P::SK::kRagDocCount, 0).toInt();
        const QString lastIdx = ss.value(P::SK::kRagLastIndexed, "Mai").toString();
        if (cnt == 0 && lastIdx != "Mai") {
            /* Timestamp esiste ma count = 0: l'ultima indicizzazione è fallita
             * (tutti gli embedding hanno restituito errore). Mostra avviso. */
            statusLbl->setText(
                QString("\xe2\x9a\xa0  Documenti indicizzati: <b>0</b>"
                        "&nbsp;&nbsp;&nbsp;Ultima indicizzazione: <b>%1</b>"
                        "&nbsp;&nbsp;&mdash;&nbsp;&nbsp;"
                        "<span style='color:#f87171;'>Embedding falliti &mdash; "
                        "installa <code>nomic-embed-text</code> e reindicizza.</span>")
                    .arg(lastIdx));
        } else {
            statusLbl->setText(
                QString("Documenti indicizzati: <b>%1</b>"
                        "&nbsp;&nbsp;&nbsp;Ultima indicizzazione: <b>%2</b>")
                    .arg(cnt)
                    .arg(lastIdx));
        }
    };

    /* Migrazione one-time: se kRagDocCount è 0 ma l'indice su disco esiste
     * (es. indicizzato con versione precedente che non salvava il conteggio,
     * oppure embeddings parzialmente falliti), carica l'engine da disco e
     * aggiorna QSettings con il conteggio reale. */
    {
        QSettings ss("Prismalux", "GUI");
        if (ss.value(P::SK::kRagDocCount, 0).toInt() == 0 &&
                m_rag.chunkCount() == 0) {
            const QString ragPath = QDir::homePath() + "/.prismalux_rag.json";
            if (QFileInfo::exists(ragPath)) {
                m_rag.load(ragPath);
                const int realN = m_rag.chunkCount();
                if (realN > 0)
                    ss.setValue(P::SK::kRagDocCount, realN);
            }
        }
    }
    refreshStatus();
    outer->addWidget(statusLbl);

    /* ── Pulsante: scarica documenti AdE ufficiali ── */
    auto* downloadBtn = new QPushButton(
        "\xf0\x9f\x93\xa5  Scarica documenti ufficiali consigliati (AdE 2026)");
    downloadBtn->setObjectName("actionBtn");
    downloadBtn->setFixedHeight(32);
    downloadBtn->setToolTip(
        "Scarica automaticamente da Agenzia delle Entrate:\n"
        "  \xe2\x80\xa2 Istruzioni 730/2026\n"
        "  \xe2\x80\xa2 Fascicolo 2 Persone Fisiche 2026\n"
        "Salvati in ~/prismalux_rag_docs/ e pronti per il RAG.");
    outer->addWidget(downloadBtn);

    /* ── Modello embedding ── */
    {
        auto* embedRow = new QHBoxLayout;
        auto* embedLbl = new QLabel("Modello embedding:");
        embedLbl->setObjectName("hintLabel");
        auto* embedEdit = new QLineEdit;
        embedEdit->setPlaceholderText("nomic-embed-text");
        embedEdit->setFixedHeight(28);
        {
            QSettings ss("Prismalux", "GUI");
            const QString saved = ss.value(P::SK::kRagEmbedModel, "").toString();
            embedEdit->setText(saved.isEmpty() ? "nomic-embed-text" : saved);
        }
        embedEdit->setToolTip(
            "Modello Ollama dedicato per gli embedding (vettorizzazione del testo).\n"
            "I modelli chat (qwen3, llama, ecc.) NON supportano /api/embeddings.\n"
            "Modello consigliato: nomic-embed-text\n"
            "Installa con: ollama pull nomic-embed-text");
        QObject::connect(embedEdit, &QLineEdit::textChanged, embedEdit, [](const QString& v) {
            QSettings ss("Prismalux", "GUI");
            ss.setValue(P::SK::kRagEmbedModel, v.trimmed());
        });
        embedRow->addWidget(embedLbl);
        embedRow->addWidget(embedEdit, 1);
        outer->addLayout(embedRow);
    }

    /* Riga pulsanti: [Ferma indicizzazione]  [Reindicizza ora] */
    auto* btnRow = new QHBoxLayout;
    btnRow->setSpacing(8);

    m_btnStopIndex = new QPushButton("\xe2\x8f\xb9  Ferma indicizzazione");
    m_btnStopIndex->setObjectName("actionBtn");
    m_btnStopIndex->setProperty("danger", true);
    m_btnStopIndex->setFixedHeight(32);
    m_btnStopIndex->setEnabled(false);   /* abilitato solo durante indexing */
    m_btnStopIndex->setToolTip(
        "Interrompe l'indicizzazione in corso dopo il chunk attuale.\n"
        "I chunk già completati vengono salvati.");
    btnRow->addWidget(m_btnStopIndex);

    auto* reindexBtn = new QPushButton("\xf0\x9f\x94\x84  Reindicizza ora");
    reindexBtn->setObjectName("actionBtn");
    reindexBtn->setFixedHeight(32);
    reindexBtn->setToolTip(
        QString("Indicizza i documenti dalla cartella selezionata.\n"
                "L'indicizzazione continua in background anche cambiando finestra.\n"
                "Indice salvato in: %1\n"
                "(disabilitabile con la checkbox \"Non salvare su disco\" sopra)")
            .arg(QDir::homePath() + "/.prismalux_rag.json"));
    btnRow->addWidget(reindexBtn);

    outer->addLayout(btnRow);

    /* Label feedback */
    auto* feedbackLbl = new QLabel;
    feedbackLbl->setObjectName("hintLabel");
    feedbackLbl->setWordWrap(true);
    feedbackLbl->setTextFormat(Qt::RichText);
    feedbackLbl->setVisible(false);
    outer->addWidget(feedbackLbl);

    outer->addStretch();

    /* ── Connessioni ── */
    QObject::connect(browseBtn, &QPushButton::clicked, dirEdit, [dirEdit]() {
        QString d = QFileDialog::getExistingDirectory(
            nullptr, "Cartella documenti RAG", dirEdit->text());
        if (!d.isEmpty()) dirEdit->setText(d);
    });
    QObject::connect(dirEdit, &QLineEdit::textChanged, dirEdit, [](const QString& t) {
        QSettings ss("Prismalux", "GUI");
        ss.setValue(P::SK::kRagDocsDir, t);
    });
    QObject::connect(maxSpin, QOverload<int>::of(&QSpinBox::valueChanged), maxSpin, [](int v) {
        QSettings ss("Prismalux", "GUI");
        ss.setValue(P::SK::kRagMaxResults, v);
    });
    /* Label feedback globale (accessibile dai lambda) */
    m_ragFeedbackLbl = feedbackLbl;

    /* ── Download documenti AdE consigliati ── */
    QObject::connect(downloadBtn, &QPushButton::clicked, downloadBtn,
        [this, dirEdit, feedbackLbl, downloadBtn]() {

        const QString ragDir = QDir::homePath() + "/prismalux_rag_docs";
        if (!QDir().mkpath(ragDir)) {
            feedbackLbl->setText("\xe2\x9a\xa0  Impossibile creare la cartella: " + ragDir);
            feedbackLbl->setVisible(true);
            return;
        }

        /* File da scaricare: {url, nome locale} */
        using DlItem = QPair<QString,QString>;
        auto* items = new QVector<DlItem>{
            { "https://www.agenziaentrate.gov.it/portale/documents/20143/9764684/"
              "730_+istruzioni_2026.pdf/2ac8d27a-fa3d-ed9e-ffc1-9bf61457661e",
              "730_istruzioni_2026.pdf" },
            { "https://www.agenziaentrate.gov.it/portale/documents/d/guest/"
              "pf2_2026_istruzioni_bozza-internet",
              "fascicolo2_persone_fisiche_2026.pdf" },
        };

        downloadBtn->setEnabled(false);
        feedbackLbl->setText(
            QString("\xe2\x8f\xb3  Download 1/%1: %2")
            .arg(items->size()).arg((*items)[0].second));
        feedbackLbl->setVisible(true);

        auto* nam  = new QNetworkAccessManager(this);
        auto* idx  = new int(0);
        auto* errN = new int(0);

        /* Catena ricorsiva: un file alla volta */
        auto* dlNext = new std::function<void()>();
        *dlNext = [=, this]() {
            if (*idx >= items->size()) {
                /* Fine download */
                if (*errN == 0) {
                    dirEdit->setText(ragDir);
                    QSettings s("Prismalux", "GUI");
                    s.setValue(P::SK::kRagDocsDir, ragDir);
                    feedbackLbl->setText(
                        "\xe2\x9c\x85  Download completato! "
                        "Cartella: <code>" + ragDir + "</code><br>"
                        "Clicca <b>Reindicizza ora</b> per costruire il RAG.");
                } else {
                    feedbackLbl->setText(
                        QString("\xe2\x9a\xa0  Download completato con %1 errori. "
                                "Controlla la connessione e riprova.").arg(*errN));
                }
                downloadBtn->setEnabled(true);
                delete items; delete idx; delete errN; delete dlNext;
                nam->deleteLater();
                return;
            }

            const QString url   = (*items)[*idx].first;
            const QString fname = (*items)[*idx].second;
            feedbackLbl->setText(
                QString("\xe2\x8f\xb3  Download %1/%2: %3")
                .arg(*idx + 1).arg(items->size()).arg(fname));

            QNetworkRequest req{QUrl(url)};
            req.setHeader(QNetworkRequest::UserAgentHeader,
                          "Mozilla/5.0 Prismalux/2.2 (Qt)");
            req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::NoLessSafeRedirectPolicy);

            auto* reply = nam->get(req);
            QObject::connect(reply, &QNetworkReply::finished, reply,
                [=, this]() {
                reply->deleteLater();
                if (reply->error() == QNetworkReply::NoError) {
                    QFile f(ragDir + "/" + fname);
                    if (f.open(QIODevice::WriteOnly))
                        f.write(reply->readAll());
                    else
                        ++(*errN);
                } else {
                    ++(*errN);
                    feedbackLbl->setText(
                        "\xe2\x9a\xa0  Errore: " + reply->errorString()
                        + " — " + fname);
                }
                ++(*idx);
                (*dlNext)();
            });
        };

        (*dlNext)();
    });

    /* Pulsante "Ferma indicizzazione" — setta il flag, il prossimo ciclo si ferma */
    QObject::connect(m_btnStopIndex, &QPushButton::clicked, this, [this, feedbackLbl]() {
        m_indexAborted = true;
        m_btnStopIndex->setEnabled(false);
        if (feedbackLbl) {
            feedbackLbl->setText("\xe2\x8f\xb3  Interruzione in corso dopo il chunk corrente...");
            feedbackLbl->setVisible(true);
        }
    });

    QObject::connect(reindexBtn, &QPushButton::clicked, reindexBtn,
        [this, dirEdit, statusLbl, feedbackLbl, refreshStatus, reindexBtn]() {
        QString dir = dirEdit->text().trimmed();
        if (dir.isEmpty() || !QDir(dir).exists()) {
            feedbackLbl->setText(
                "\xe2\x9a\xa0  Cartella non valida o inesistente. "
                "Specifica un percorso esistente.");
            feedbackLbl->setVisible(true);
            return;
        }
        if (m_ai->backend() == AiClient::LlamaLocal) {
            feedbackLbl->setText(
                "\xe2\x9a\xa0  Embedding disponibile solo con Ollama o llama-server.");
            feedbackLbl->setVisible(true);
            return;
        }
        m_indexAborted = false;

        /* Raccolta chunk — helper per spezzare testo in chunk da ~400 caratteri */
        auto addChunks = [this](const QString& content) {
            for (int i = 0; i < content.size(); i += 400) {
                QString chunk = content.mid(i, 500).simplified();
                if (chunk.size() >= 30)
                    m_ragQueue << chunk;
            }
        };

        m_ragQueue.clear();
        m_ragQueuePos = 0;
        m_rag.clear();

        /* ── 1. File di testo ── */
        QStringList filters{
            "*.txt","*.md","*.csv","*.rst","*.py","*.cpp","*.h","*.c"
        };
        QDirIterator it(dir, filters, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QFile f(it.next());
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
            addChunks(QString::fromUtf8(f.readAll()));
        }

        /* ── 2. PDF tramite pdftotext (se installato) ── */
        const QString pdfToText = QStandardPaths::findExecutable("pdftotext");
        if (!pdfToText.isEmpty()) {
            QDirIterator pdfIt(dir, QStringList{"*.pdf"}, QDir::Files,
                               QDirIterator::Subdirectories);
            while (pdfIt.hasNext()) {
                const QString pdfPath = pdfIt.next();
                QProcess p;
                /* pdftotext file.pdf - → testo su stdout, nessuna shell */
                p.start(pdfToText, {pdfPath, "-"});
                if (!p.waitForFinished(60000)) continue;  /* timeout 60s per PDF grandi */
                addChunks(QString::fromUtf8(p.readAllStandardOutput()));
            }
        }
        /* pdftotext non trovato: i PDF vengono saltati silenziosamente.
           Su Linux: sudo apt install poppler-utils
           Su Windows: incluso in Poppler for Windows (già elencato in Dipendenze) */

        if (m_ragQueue.isEmpty()) {
            feedbackLbl->setText("\xf0\x9f\x8c\xab  Nessun contenuto trovato nella cartella.");
            feedbackLbl->setVisible(true);
            return;
        }

        reindexBtn->setEnabled(false);
        if (m_btnStopIndex) m_btnStopIndex->setEnabled(true);
        feedbackLbl->setText(QString("\xe2\x8f\xb3  Indicizzazione: 0 / %1 chunk...").arg(m_ragQueue.size()));
        feedbackLbl->setVisible(true);
        emit indexingProgress(0, m_ragQueue.size());

        /* Funzione ricorsiva: processa un chunk alla volta tramite embeddingReady.
         * errCount conta i chunk saltati per errore embedding: serve per mostrare
         * un messaggio utile se il modello non supporta /api/embeddings.
         * m_indexAborted: settato da "Ferma indicizzazione" — il ciclo si ferma
         * al prossimo giro (dopo il chunk già in volo). */
        auto* errCount  = new int(0);
        auto* indexNext = new std::function<void()>();
        *indexNext = [this, indexNext, errCount, reindexBtn, feedbackLbl, statusLbl,
                      refreshStatus, dir]() {
            /* Controlla stop (abort) O completamento naturale */
            const bool done = (m_ragQueuePos >= m_ragQueue.size());
            if (done || m_indexAborted) {
                /* Fine indicizzazione (naturale o interrotta) */
                const QString path = QDir::homePath() + "/.prismalux_rag.json";
                int n = m_rag.chunkCount();
                QSettings ss("Prismalux", "GUI");
                if (n > 0) {
                    /* Salva su QSettings solo se almeno un chunk è stato indicizzato.
                     * Se n==0 (tutti gli embedding falliti), non sovrascrivere il
                     * conteggio e il timestamp dell'ultima indicizzazione riuscita. */
                    if (!m_ragNoSave)
                        m_rag.save(path);
                    ss.setValue(P::SK::kRagDocCount,    n);
                    ss.setValue(P::SK::kRagLastIndexed,
                        QDateTime::currentDateTime().toString("dd/MM/yyyy HH:mm"));
                }
                refreshStatus();

                const bool wasAborted = m_indexAborted;
                m_indexAborted = false;  /* reset flag per la prossima esecuzione */

                if (wasAborted) {
                    feedbackLbl->setText(
                        QString("\xe2\x8f\xb9  Indicizzazione interrotta &mdash; "
                                "<b>%1</b> chunk salvati su %2 totali.")
                            .arg(n).arg(m_ragQueue.size()));
                } else if (n == 0) {
                    const QString usedModel = ss.value(P::SK::kRagEmbedModel, "").toString();
                    feedbackLbl->setText(
                        QString("\xe2\x9d\x8c  <b>Embedding falliti</b> (%1 errori). "
                                "Il modello <b>%2</b> non supporta <code>/api/embeddings</code>.<br>"
                                "Installa il modello embedding: "
                                "<code>ollama pull nomic-embed-text</code><br>"
                                "Poi verifica il campo <b>Modello embedding</b> qui sopra "
                                "e ripeti l'indicizzazione.")
                            .arg(*errCount)
                            .arg(usedModel.isEmpty() ? "corrente" : usedModel));
                } else {
                    feedbackLbl->setText(
                        QString("\xe2\x9c\x85  Indicizzati <b>%1</b> chunk da <code>%2</code>"
                                "%3. %4")
                            .arg(n).arg(dir)
                            .arg(*errCount > 0
                                ? QString(" (%1 chunk saltati per errore)").arg(*errCount)
                                : QString())
                            .arg(m_ragNoSave
                                ? "Indice <b>solo in RAM</b> (non salvato su disco)."
                                : "Indice salvato in <code>~/.prismalux_rag.json</code>."));
                }
                if (m_btnStopIndex) m_btnStopIndex->setEnabled(false);
                reindexBtn->setEnabled(true);
                emit indexingFinished(n, wasAborted);
                delete errCount;
                delete indexNext;
                return;
            }

            /* Aggiorna progresso */
            feedbackLbl->setText(
                QString("\xe2\x8f\xb3  Indicizzazione: %1 / %2 chunk...")
                    .arg(m_ragQueuePos + 1).arg(m_ragQueue.size()));
            emit indexingProgress(m_ragQueuePos, m_ragQueue.size());

            const QString chunk = m_ragQueue[m_ragQueuePos++];

            /* One-shot connection: embeddingReady → addChunk → next.
             * IMPORTANTE: conn e connErr si referenziano a vicenda per garantire
             * che scattando l'uno l'altro venga disconnesso subito, evitando
             * connessioni stale che si accumulerebbero tra chunk successivi. */
            auto* conn    = new QMetaObject::Connection;
            auto* connErr = new QMetaObject::Connection;

            *conn = connect(m_ai, &AiClient::embeddingReady, this,
                [this, chunk, indexNext, conn, connErr](const QVector<float>& vec) {
                    disconnect(*conn);    delete conn;
                    disconnect(*connErr); delete connErr;
                    m_rag.addChunk(chunk, vec);
                    (*indexNext)();
                }, Qt::SingleShotConnection);

            *connErr = connect(m_ai, &AiClient::embeddingError, this,
                [this, indexNext, errCount, conn, connErr](const QString&) {
                    disconnect(*connErr); delete connErr;
                    disconnect(*conn);    delete conn;
                    ++(*errCount);
                    (*indexNext)();   /* salta chunk con errore */
                }, Qt::SingleShotConnection);

            m_ai->fetchEmbedding(chunk);
        };

        (*indexNext)();
    });

    return page;
}

/* ──────────────────────────────────────────────────────────────
   buildTemaTab — card per ogni tema disponibile (built-in + custom)
   ────────────────────────────────────────────────────────────── */
struct ThemeVisual { QString id; QString bg; QString accent; QString bg2; };
static const ThemeVisual kVisuals[] = {
    { "dark_cyan",   "#13141f", "#00b8d9", "#1a1b28" },
    { "dark_amber",  "#17140c", "#ffb300", "#211c10" },
    { "dark_purple", "#130f1c", "#9c5ff0", "#1c1727" },
    { "light",       "#f0f2fa", "#0072c6", "#e0e4f0" },
};

static ThemeVisual visualFor(const QString& id) {
    for (const auto& v : kVisuals)
        if (v.id == id) return v;
    return { id, "#1e2030", "#607080", "#252a3a" };  /* fallback per custom */
}

QWidget* ImpostazioniPage::buildTemaTab() {
    auto* outer = new QWidget(this);
    auto* vlay  = new QVBoxLayout(outer);
    vlay->setContentsMargins(20, 20, 20, 20);
    vlay->setSpacing(16);

    /* ── Titolo ── */
    auto* title = new QLabel("\xf0\x9f\x8e\xa8  Seleziona tema", outer);
    title->setStyleSheet("font-size:16px; font-weight:700; color:#e5e7eb;");
    vlay->addWidget(title);

    auto* hint = new QLabel(
        "Copia file <b>.qss</b> nella cartella <code>themes/</code> accanto "
        "all'eseguibile per aggiungere temi personalizzati.", outer);
    hint->setWordWrap(true);
    hint->setStyleSheet("color:#9ca3af; font-size:12px;");
    vlay->addWidget(hint);

    /* ── Griglia card (4 per riga) ── */
    auto* scroll = new QScrollArea(outer);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* grid_w = new QWidget(scroll);
    auto* grid   = new QGridLayout(grid_w);
    grid->setSpacing(14);
    grid->setContentsMargins(0, 8, 0, 8);
    scroll->setWidget(grid_w);
    vlay->addWidget(scroll);

    ThemeManager* tm      = ThemeManager::instance();
    const auto&   themes  = tm->themes();
    const QString current = tm->currentId();

    /* Raggruppa i bottoni per gestire la selezione esclusiva */
    auto* group = new QButtonGroup(outer);
    group->setExclusive(true);

    const int COLS = 4;
    for (int i = 0; i < themes.size(); ++i) {
        const auto& t = themes[i];
        ThemeVisual v = visualFor(t.id);

        /* ── Contenitore card ── */
        auto* card = new QPushButton(grid_w);
        card->setCheckable(true);
        card->setChecked(t.id == current);
        card->setFixedSize(130, 100);
        card->setObjectName("themeCard");
        card->setProperty("themeId", t.id);

        /* Stile card: anteprima colori via QSS inline */
        const QString borderColor = (t.id == current) ? v.accent : "#404040";
        card->setStyleSheet(QString(
            "QPushButton#themeCard {"
            "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
            "    stop:0 %1, stop:0.65 %2, stop:0.66 %3, stop:1 %3);"
            "  border: 2px solid %4; border-radius: 10px;"
            "  color: %5; font-weight: 600; font-size: 11px;"
            "  padding-top: 54px;"
            "  text-align: center;"
            "}"
            "QPushButton#themeCard:checked {"
            "  border: 3px solid %3; border-radius: 10px;"
            "}"
            "QPushButton#themeCard:hover {"
            "  border-color: %3;"
            "}"
        ).arg(v.bg2, v.bg, v.accent, borderColor,
              (t.id == "light") ? "#1a1e30" : "#e5e7eb"));

        card->setText(t.label);
        group->addButton(card, i);

        /* Checkmark sovrapposto (visibile se selezionato) */
        auto* check = new QLabel("\xe2\x9c\x93", card);
        check->setAlignment(Qt::AlignTop | Qt::AlignRight);
        check->setStyleSheet(QString(
            "color: %1; font-size: 16px; font-weight: 700; "
            "background: transparent; padding: 4px 6px 0 0;")
            .arg(v.accent));
        check->setFixedSize(130, 30);
        check->setVisible(t.id == current);

        /* Connessione: cambia tema al click */
        connect(card, &QPushButton::clicked, card,
            [tm, t, themes, group, grid_w]() {
                tm->apply(t.id);
                /* Aggiorna border di tutte le card */
                for (int j = 0; j < themes.size(); ++j) {
                    auto* btn = qobject_cast<QPushButton*>(group->button(j));
                    if (!btn) continue;
                    ThemeVisual vj = visualFor(themes[j].id);
                    const QString bc = (themes[j].id == t.id) ? vj.accent : "#404040";
                    /* Aggiorna solo la border-color senza ricostruire tutto lo stile */
                    QString s = btn->styleSheet();
                    /* checkmark visibility */
                    const auto children = btn->findChildren<QLabel*>();
                    for (auto* lbl : children)
                        lbl->setVisible(themes[j].id == t.id);
                    (void)bc; (void)s;
                }
            });

        grid->addWidget(card, i / COLS, i % COLS);
    }

    /* Riga espandibile in fondo */
    grid->setRowStretch(themes.size() / COLS + 1, 1);
    /* ── Sezione: Aspetto bolle chat ── */
    auto* secBolle = new QFrame(outer);
    secBolle->setObjectName("cardFrame");
    auto* bolleLay = new QVBoxLayout(secBolle);
    bolleLay->setContentsMargins(16, 10, 16, 10);
    bolleLay->setSpacing(10);

    auto* bolleTitle = new QLabel(
        "\xf0\x9f\x92\xac  <b>Aspetto bolle chat</b>", secBolle);
    bolleTitle->setObjectName("cardTitle");
    bolleTitle->setTextFormat(Qt::RichText);
    bolleLay->addWidget(bolleTitle);

    auto* radiusRow = new QWidget(secBolle);
    auto* radiusLay = new QHBoxLayout(radiusRow);
    radiusLay->setContentsMargins(0, 0, 0, 0);
    radiusLay->setSpacing(10);

    auto* radiusLbl = new QLabel("Raggio bordi (px):", secBolle);
    radiusLbl->setObjectName("cardDesc");

    auto* radiusSpin = new QSpinBox(secBolle);
    radiusSpin->setRange(0, 24);
    radiusSpin->setSuffix(" px");
    radiusSpin->setFixedWidth(80);
    {
        QSettings s("Prismalux", "GUI");
        radiusSpin->setValue(s.value(P::SK::kBubbleRadius, 10).toInt());
    }

    auto* radiusPreview = new QLabel(secBolle);
    radiusPreview->setObjectName("cardDesc");
    radiusPreview->setText("(applicato alle nuove bolle)");

    auto updateRadius = [radiusSpin, radiusPreview]() {
        QSettings("Prismalux", "GUI").setValue(P::SK::kBubbleRadius, radiusSpin->value());
        radiusPreview->setText(
            radiusSpin->value() == 0
                ? "Bolle con angoli netti"
                : radiusSpin->value() <= 6
                    ? "Bolle leggermente arrotondate"
                    : radiusSpin->value() <= 14
                        ? "Bolle arrotondate (default)"
                        : "Bolle molto arrotondate");
    };
    updateRadius();

    QObject::connect(radiusSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                     secBolle, [updateRadius](int){ updateRadius(); });

    radiusLay->addWidget(radiusLbl);
    radiusLay->addWidget(radiusSpin);
    radiusLay->addWidget(radiusPreview);
    radiusLay->addStretch();
    bolleLay->addWidget(radiusRow);
    vlay->addWidget(secBolle);

    /* ── Sezione: Modalità etichette barra di navigazione ── */
    auto* secNav = new QFrame(outer);
    secNav->setObjectName("cardFrame");
    auto* navLay = new QVBoxLayout(secNav);
    navLay->setContentsMargins(16, 10, 16, 10);
    navLay->setSpacing(8);

    auto* navTitle = new QLabel(
        "\xf0\x9f\x8f\xb7\xef\xb8\x8f  <b>Modalit\xc3\xa0 etichette tab</b>", secNav);
    navTitle->setObjectName("cardTitle");
    navTitle->setTextFormat(Qt::RichText);
    navLay->addWidget(navTitle);

    auto* navHint = new QLabel(
        "Scegli come visualizzare le etichette dei tab principali.", secNav);
    navHint->setObjectName("hintLabel");
    navLay->addWidget(navHint);

    struct NavMode { const char* label; const char* value; };
    static const NavMode kNavModes[] = {
        { "\xf0\x9f\x94\xa4  Solo icone",              "icons_only"   },
        { "\xf0\x9f\x94\xa4 Testo  Icone + testo",     "icons_text"   },
        { "Testo \xf0\x9f\x94\xa4  Testo + icone",     "text_icons"   },
        { "Aa  Solo testo",                             "text_only"    },
    };

    QSettings navSett("Prismalux", "GUI");
    const QString curNavMode = navSett.value(P::SK::kNavTabMode, "icons_text").toString();

    auto* navGroup = new QButtonGroup(secNav);
    auto* navRowW  = new QWidget(secNav);
    auto* navRowL  = new QHBoxLayout(navRowW);
    navRowL->setContentsMargins(0, 0, 0, 0);
    navRowL->setSpacing(8);

    for (const auto& nm : kNavModes) {
        auto* rb = new QRadioButton(QString::fromUtf8(nm.label), secNav);
        rb->setObjectName("cardDesc");
        rb->setChecked(curNavMode == nm.value);
        navGroup->addButton(rb);
        navRowL->addWidget(rb);

        connect(rb, &QRadioButton::toggled, this, [this, nm](bool checked){
            if (!checked) return;
            QSettings s("Prismalux", "GUI");
            s.setValue(P::SK::kNavTabMode, nm.value);
            emit tabModeChanged(QString::fromLatin1(nm.value));
        });
    }
    navRowL->addStretch();
    navLay->addWidget(navRowW);

    vlay->addWidget(secNav);

    /* ── Sezione: Stile navigazione principale ── */
    {
        auto* secStyle = new QFrame(outer);
        secStyle->setObjectName("cardFrame");
        auto* sLay = new QVBoxLayout(secStyle);
        sLay->setContentsMargins(16, 10, 16, 10);
        sLay->setSpacing(8);

        auto* sTitle = new QLabel(
            "\xf0\x9f\x97\x82\xef\xb8\x8f  <b>Stile navigazione</b>", secStyle);
        sTitle->setObjectName("cardTitle");
        sTitle->setTextFormat(Qt::RichText);
        sLay->addWidget(sTitle);

        auto* sHint = new QLabel(
            "Schede in alto (predefinito) oppure men\xc3\xb9 orizzontale a pulsanti con categorie.",
            secStyle);
        sHint->setObjectName("hintLabel");
        sHint->setWordWrap(true);
        sLay->addWidget(sHint);

        struct NavStyleOpt { const char* label; const char* value; };
        static const NavStyleOpt kStyles[] = {
            { "\xf0\x9f\x93\x91  Schede in alto",       "tabs_top"   },
            { "\xf0\x9f\x93\x8b  Men\xc3\xb9 principale", "menu_main" },
        };

        QSettings navSett2("Prismalux", "GUI");
        const QString curStyle = navSett2.value(P::SK::kNavStyle, "tabs_top").toString();

        auto* styleGroup = new QButtonGroup(secStyle);
        auto* styleRow   = new QWidget(secStyle);
        auto* styleRowL  = new QHBoxLayout(styleRow);
        styleRowL->setContentsMargins(0, 0, 0, 0);
        styleRowL->setSpacing(16);

        for (const auto& ns : kStyles) {
            auto* rb = new QRadioButton(QString::fromUtf8(ns.label), secStyle);
            rb->setObjectName("cardDesc");
            rb->setChecked(curStyle == ns.value);
            styleGroup->addButton(rb);
            styleRowL->addWidget(rb);
            connect(rb, &QRadioButton::toggled, this, [this, ns](bool checked){
                if (!checked) return;
                QSettings s("Prismalux", "GUI");
                s.setValue(P::SK::kNavStyle, ns.value);
                emit navStyleChanged(QString::fromLatin1(ns.value));
            });
        }
        styleRowL->addStretch();
        sLay->addWidget(styleRow);
        vlay->addWidget(secStyle);
    }

    /* ── Sezione: Pulsanti di esecuzione ── */
    {
        auto* secExec = new QFrame(outer);
        secExec->setObjectName("cardFrame");
        auto* eLay = new QVBoxLayout(secExec);
        eLay->setContentsMargins(16, 10, 16, 10);
        eLay->setSpacing(8);

        auto* eTitle = new QLabel(
            "\xe2\x96\xb6  <b>Pulsanti di esecuzione</b>", secExec);
        eTitle->setObjectName("cardTitle");
        eTitle->setTextFormat(Qt::RichText);
        eLay->addWidget(eTitle);

        auto* eHint = new QLabel(
            "Avvia, Stop, Esegui, Calcola e simili \xe2\x80\x94 in tutte le schede.", secExec);
        eHint->setObjectName("hintLabel");
        eHint->setWordWrap(true);
        eLay->addWidget(eHint);

        struct ExecMode { const char* label; const char* value; };
        static const ExecMode kExecModes[] = {
            { "\xf0\x9f\x94\xa4  Solo icone",  "icon_only"  },
            { "Aa  Solo testo",                 "text_only"  },
            { "\xf0\x9f\x94\xa4 Aa  Icona + testo", "icon_text" },
        };

        QSettings exSett("Prismalux", "GUI");
        const QString curExec = exSett.value(P::SK::kNavExecBtnMode, "icon_text").toString();

        auto* execGroup = new QButtonGroup(secExec);
        auto* execRow   = new QWidget(secExec);
        auto* execRowL  = new QHBoxLayout(execRow);
        execRowL->setContentsMargins(0, 0, 0, 0);
        execRowL->setSpacing(16);

        for (const auto& em : kExecModes) {
            auto* rb = new QRadioButton(QString::fromUtf8(em.label), secExec);
            rb->setObjectName("cardDesc");
            rb->setChecked(curExec == em.value);
            execGroup->addButton(rb);
            execRowL->addWidget(rb);
            connect(rb, &QRadioButton::toggled, this, [this, em](bool checked){
                if (!checked) return;
                QSettings s("Prismalux", "GUI");
                s.setValue(P::SK::kNavExecBtnMode, em.value);
                emit execBtnModeChanged(QString::fromLatin1(em.value));
            });
        }
        execRowL->addStretch();
        eLay->addWidget(execRow);
        vlay->addWidget(secExec);
    }

    vlay->addStretch();
    return outer;
}

/* ──────────────────────────────────────────────────────────────
   buildTestTab — registro di tutti i test eseguiti e superati
   ────────────────────────────────────────────────────────────── */
QWidget* ImpostazioniPage::buildTestTab()
{
    /* ── Struttura dati per ogni suite di test ───────────────────────────
       details: voci separate da '\n', mostrate come lista bullet
       kpi:     metriche chiave (tempi, soglie), mostrate in evidenza
    ─────────────────────────────────────────────────────────────────── */
    struct TestEntry {
        QString suite;
        int     passed;
        int     total;
        QString date;
        QString desc;          /* una riga: cosa testa */
        QString details;       /* voci separate da \n — lista bullet */
        QString kpi;           /* metriche numeriche in evidenza */
    };

    static const TestEntry kTests[] = {
        {
            "test_engine",
            18, 18, "2026-03-18",
            "TUI C \xe2\x80\x94 pipeline core: connessione TCP, parsing JSON, streaming AI, python3",
            "TCP helpers: connessione a Ollama e llama-server\n"
            "JSON encode/decode: js_str(), js_encode() con caratteri speciali\n"
            "ai_chat_stream Ollama: stream NDJSON completo\n"
            "ai_chat_stream llama-server: stream SSE (OpenAI API)\n"
            "python3 popen: esecuzione script e cattura output\n"
            "backend_ready(): rilevamento server attivo",
            "18/18 pass \xc2\xb7 nessuna dipendenza esterna richiesta"
        },
        {
            "test_multiagente",
            8, 8, "2026-03-18",
            "TUI C \xe2\x80\x94 pipeline 6 agenti end-to-end con modelli reali (qwen3:4b + qwen2.5-coder:7b)",
            "Ricercatore: raccoglie informazioni sul task\n"
            "Pianificatore: struttura il piano di sviluppo\n"
            "Programmatori A e B: lavoro parallelo su due implementazioni\n"
            "Tester: esegue il codice e cicla correzioni (max 3 tentativi)\n"
            "Ottimizzatore: revisione finale dell\xe2\x80\x99output\n"
            "Validazione: output Python eseguibile, nessun traceback",
            "8/8 pass \xc2\xb7 richiede Ollama attivo con i modelli installati"
        },
        {
            "test_buildsys.cpp",
            36, 36, "2026-03-22",
            "Qt GUI \xe2\x80\x94 _buildSys(): costruzione system prompt adattivo per famiglia LLM",
            "Modelli piccoli \xe2\x89\xa4" "4.5B: usa sysPromptSmall (2-3 frasi brevi)\n"
            "Modelli medi 7-8B: system prompt standard\n"
            "Modelli grandi 9B+: system prompt esteso con dettagli\n"
            "LlamaLocal: tronca a 2 frasi per compatibilit\xc3\xa0 contesto limitato\n"
            "Reasoning (qwen3/deepseek-r1): aggiunge nota /think diretta\n"
            "Iniezione math [=N]: aggiunge avviso valori pre-calcolati\n"
            "Edge case: modello senza dimensione, senza 'b' nel nome",
            "36/36 pass \xc2\xb7 isolato, no AI \xc2\xb7 7 famiglie LLM testate"
        },
        {
            "test_toon_config.c",
            55, 55, "2026-03-22",
            "TUI C \xe2\x80\x94 parser file .toon e persistenza configurazione Prismalux",
            "Lettura chiave semplice e prima riga del file\n"
            "toon_get_int(): conversione intera con fallback\n"
            "Overflow buffer: lunghezza valore > capacit\xc3\xa0 (no crash)\n"
            "CRLF e spazi trailing: normalizzazione automatica\n"
            "Valori con ':' (es. qwen3.5:9b): parsing corretto\n"
            "Chiave parziale: 'model' != 'ollama_model' (no falsi positivi)\n"
            "UTF-8: caratteri multibyte nei valori\n"
            "Round-trip write\xe2\x86\x92read: dati identici dopo salvataggio\n"
            "Stress: 200 chiavi, 10k ricerche",
            "55/55 pass \xc2\xb7 10k ricerche < 100ms \xc2\xb7 scenari reali Prismalux"
        },
        {
            "test_stress.c",
            74, 74, "2026-03-22",
            "TUI C \xe2\x80\x94 stress API pubblica: JSON, buffer, Unicode, input anomali",
            "js_str parser: stringhe vuote, escape, nidificazioni profonde\n"
            "js_encode escaping: caratteri di controllo, backslash, virgolette\n"
            "BackendCtx: modello vuoto, host nullo, porta fuori range\n"
            "IEEE 754: NaN, +Inf, -Inf, zero negativo\n"
            "Buffer overflow: protezione strlen > capacit\xc3\xa0\n"
            "UTF-8 multibyte: cinese, arabo, emoji nei prompt\n"
            "String ops interni: strncpy, snprintf con n=0\n"
            "Input TUI assurdi: 0, -1, 999, stringa vuota, solo spazi",
            "74/74 pass \xc2\xb7 nessun crash su 74 scenari anomali"
        },
        {
            "test_cs_random.c",
            17, 17, "2026-03-22",
            "TUI C \xe2\x80\x94 Context Store semantico: scrittura massiva, precision e recall",
            "Scrittura 500 chunk con chiavi semantiche casuali\n"
            "Precision: i risultati contengono almeno una chiave della query\n"
            "Recall: nessun chunk rilevante viene escluso\n"
            "Score ordering: chunk con pi\xc3\xb9 chiavi comuni viene restituito prima\n"
            "Persistenza: close() + reopen() restituisce gli stessi risultati\n"
            "Edge case: chiave inesistente (0 risultati), chunk da 10 KB\n"
            "Stress: 1000 query consecutive senza degrado",
            "17/17 pass \xc2\xb7 throughput > 2400 query/s"
        },
        {
            "test_guardia_math.c",
            93, 93, "2026-03-22",
            "TUI C \xe2\x80\x94 parser aritmetico guardia pre-pipeline (copiato in isolamento da multi_agente.c)",
            "Operazioni base: +, -, *, /, % con precedenza corretta\n"
            "Potenza e fattoriale: 2^10=1024, 5!=120\n"
            "Espressioni composite: ((3+4)*2-1)^2 = 169\n"
            "Divisione sicura: 1/0=0, 0%0=0 (no crash, no UB)\n"
            "Pattern sconto: '20% di sconto su 150' \xe2\x86\x92 120\n"
            "Pattern percentuale: '15% di 200' \xe2\x86\x92 30\n"
            "Numeri primi: primes(10,50) \xe2\x86\x92 [11,13,17,19,23,29,31,37,41,43,47]\n"
            "Sommatoria: sum(1,100) \xe2\x86\x92 5050 (formula Gauss)\n"
            "Linguaggio naturale: 'quanto fa 3+4', 'calcola 10*5'\n"
            "_task_sembra_codice(): riconosce task non-programmatici\n"
            "Edge case: range >100k, somma inversa, overflow",
            "93/93 pass \xc2\xb7 30k eval in 4ms (7.5M eval/s) \xc2\xb7 isolato, no AI"
        },
        {
            "test_math_locale.c",
            120, 120, "2026-03-22",
            "TUI C \xe2\x80\x94 motore matematico Tutor (copiato in isolamento da strumenti.c)",
            "_math_eval: parser ricorsivo completo con precedenza\n"
            "_is_prime: tutti i 1229 numeri primi tra 1 e 10000\n"
            "_fmt_num: interi mostrati senza .0 superfluo\n"
            "Sconto e ricarica: '30% sconto su 200' \xe2\x86\x92 140, '20% ricarica su 50' \xe2\x86\x92 60\n"
            "Percentuale semplice: '15% di 80' \xe2\x86\x92 12\n"
            "MCD/MCM Euclide: mcd(48,18)=6, mcm(4,6)=12\n"
            "Radice quadrata: sqrt(144)=12, sqrt(2)\xe2\x89\x88" "18 decimali\n"
            "Media: media di [1,2,3,4,5] = 3\n"
            "Sommatoria Gauss: sum(1,100) = 5050\n"
            "Fattoriale: 10! = 3628800, soglia overflow per n>20\n"
            "Edge case: numero negativo, espressione nulla, valore troppo grande",
            "120/120 pass \xc2\xb7 150k eval in 22ms \xc2\xb7 1229 primi verificati \xc2\xb7 isolato, no AI"
        },
        {
            "test_agent_scheduler.c",
            87, 87, "2026-03-22",
            "TUI C \xe2\x80\x94 Hot/Cold scheduler GGUF: API completa senza llama.cpp",
            "as_init: legge VRAM/RAM da HWInfo, imposta flag sequential se VRAM < 4 GB\n"
            "as_add: overflow slot (max 8), normalizzazione priorit\xc3\xa0, path Windows\n"
            "as_assign_layers: CPU-only=0, scaling proporzionale VRAM, sticky-hot=base, light=cap 16\n"
            "as_load: cache hit (hot_idx==idx) incrementa cache_hits e ritorna subito\n"
            "as_evict: evict doppio sicuro (no double-free)\n"
            "as_record: EMA latenza \xce\xb1=0.3, promozione sticky-hot a 3 chiamate consecutive\n"
            "vram_profile round-trip: salva/ricarica JSON con delta VRAM per modello\n"
            "Modalit\xc3\xa0 sequential: pipeline lineare quando VRAM insufficiente\n"
            "Stress: 1000 cicli load/evict/record",
            "87/87 pass \xc2\xb7 stress 1000 cicli \xc2\xb7 500 cache hit + 500 miss verificati"
        },
        {
            "test_perf.c",
            26, 26, "2026-03-22",
            "TUI C \xe2\x80\x94 Performance: TTFT, throughput token, cold start, benchmark timer",
            "Timer CLOCK_MONOTONIC: risoluzione misurata (0.133 \xc2\xb5s su questo sistema)\n"
            "Monotonicità: 100 campionamenti sempre crescenti\n"
            "TTFT scenario 50ms: primo token rilevato tra 40ms e 200ms\n"
            "TTFT scenario istantaneo: < 100ms, throughput simulato 86M tok/s\n"
            "TTFT scenario 15 tok/s: TTFT 200ms, throughput 25.8 tok/s\n"
            "Cold start scheduler: 1000 init in 79ms (0.079ms/init)\n"
            "vram_profile round-trip: 100 save/load in 2.1ms\n"
            "Contatore token: accuratezza \xc2\xb1" "10% su 100 frasi da 10 parole\n"
            "Benchmark now_ms(): 1M chiamate in 24ms (24 ns/call)",
            "26/26 pass \xc2\xb7 TTFT KPI < 3s \xc2\xb7 throughput KPI > 10 tok/s \xc2\xb7 timer 0.133\xc2\xb5s"
        },
        {
            "test_sha256.c",
            20, 20, "2026-03-22",
            "TUI C \xe2\x80\x94 SHA-256 puro C (FIPS 180-4) per verifica integrit\xc3\xa0 file .gguf",
            "Vettore NIST 1: SHA-256(\"\") = e3b0c442...\n"
            "Vettore NIST 2: SHA-256(\"abc\") = ba7816bf...\n"
            "Vettore NIST 3: SHA-256(stringa 448-bit) = 248d6a61...\n"
            "Vettore NIST 4: SHA-256(\"The quick brown fox...\") = d7a8fbb3...\n"
            "Vettore NIST 5: SHA-256(byte 0x00) = 6e340b9c...\n"
            "Consistenza: hash blocco singolo = aggiornamenti byte per byte\n"
            "File vuoto: hash corretto; file inesistente: ritorna -1\n"
            "File .gguf reali: Qwen2.5-3B (13s), SmolLM2-135M (0.7s)\n"
            "Avalanche effect: 1 byte modificato \xe2\x86\x92 32/32 byte digest diversi\n"
            "Formato HuggingFace: 'sha256:<hex>' (71 caratteri)",
            "20/20 pass \xc2\xb7 1 MB hashato in 7ms \xc2\xb7 5 vettori NIST verificati"
        },
        {
            "test_memory.c",
            12, 12, "2026-03-22",
            "TUI C \xe2\x80\x94 Rilevamento memory leak via /proc/self/status (campo VmRSS)",
            "Lettura VmRSS: baseline 1.7 MB, delta tra due letture consecutive = 0 KB\n"
            "Baseline malloc/free: 10k alloc con dimensioni 128-4096 byte, delta RSS = 0 KB\n"
            "Scheduler 1000 cicli: as_init+as_add+as_assign+as_load+as_record+as_evict, delta = 120 KB\n"
            "Context Store 5 cicli: 40 write + cs_close() ogni ciclo, delta = 4 KB\n"
            "Stress alloc 100k: pattern prompt AI (500B, 4KB, 8KB, 2KB, 256B), delta = 92 KB\n"
            "Pipeline alloc: 100 run da 6 agenti \xc3\x97" "16 KB ciascuno (cascade), delta finale 92 KB\n"
            "KPI finale: RSS processo test = 3.1 MB (soglia < 50 MB)",
            "12/12 pass \xc2\xb7 scheduler < 512 KB/1000 cicli \xc2\xb7 RSS finale 3.1 MB"
        },
        {
            "test_rag.c",
            30, 30, "2026-03-22",
            "TUI C \xe2\x80\x94 RAG locale: precision, score ordering, edge case, performance",
            "Dir inesistente: rag_query() ritorna 0 chunk, out_ctx vuoto (no crash)\n"
            "Dir vuota: stessa garanzia, out_ctx = stringa vuota\n"
            "Precision math: chunk restituiti contengono le parole 'matematica'/'equazioni'\n"
            "Precision Python: chunk contengono 'Python'/'programmazione'\n"
            "Precision storia: chunk contengono 'storia'/'medioevo'\n"
            "Score ordering: documento ad alta rilevanza incluso nel top-K\n"
            "File non-.txt ignorati: .json con dati rilevanti non viene restituito\n"
            "File .txt vuoto: no crash, 0 chunk\n"
            "rag_build_prompt: output inizia con 'CONTESTO DOCUMENTALE'\n"
            "rag_build_prompt senza RAG: ritorna solo il sys prompt base\n"
            "Edge case: query vuota, parole < 3 caratteri, buffer da 10 byte\n"
            "Documenti reali: rag_docs/730 e rag_docs/piva (3 chunk ciascuno)",
            "30/30 pass \xc2\xb7 5 query in 0.22ms \xc2\xb7 100 stress query in 3.7ms"
        },
        {
            "test_security.c",
            65, 65, "2026-03-22",
            "TUI C \xe2\x80\x94 Sicurezza: anti-prompt-injection e anti-data-leak (isolato)",
            "T01 ChatML/Qwen: <|im_start|>, <|im_end|>, <|endoftext|>, <|eot_id|>, ### rimossi\n"
            "T02 Llama2/Mistral: [INST], [/INST], <<SYS>>, <</SYS>>, <!-- rimossi\n"
            "T03 Role override: \\n\\nSystem:, \\n\\nAssistant:, \\n\\nUser:, \\n\\nHuman:, "
            "\\n\\nAI: neutralizzati (solo \\n\\n rimosso, testo preservato)\n"
            "T04 Pattern multipli: tutti i pattern rimossi da una stringa mista; "
            "### rimosso in tutte le occorrenze\n"
            "T05 Testo legittimo: codice Python, matematica, italiano con accenti, "
            "URL, newline singoli \xe2\x80\x94 NON modificati\n"
            "T06 Idempotenza: applicata 2 volte = stesso risultato di 1 volta\n"
            "T07 Edge case: NULL, bufmax=0, stringa vuota, soli spazi, buffer 1 byte \xe2\x80\x94 no crash\n"
            "T08 Confine buffer: ### al bordo del buffer \xe2\x80\x94 no overflow, null terminator intatto\n"
            "T09 js_encode nome modello: virgolette, newline, backslash, CR bloccati; "
            "modello legittimo 'qwen2.5-coder:7b' non alterato\n"
            "T10 host_is_local whitelist: 127.0.0.1, ::1, localhost, LOCALHOST accettati\n"
            "T11 host esterni bloccati: 8.8.8.8, 192.168.1.1, api.openai.com, "
            "127.0.0.2, URL injection credential, stringa vuota, NULL\n"
            "T12 Stress: 10k sanitizzazioni su 10 input casuali \xe2\x80\x94 0 injection sopravvivono",
            "65/65 pass \xc2\xb7 10k sanitizzazioni in 1.9ms \xc2\xb7 isolato, no AI, no rete"
        },
        {
            "test_golden.c",
            53, 53, "2026-03-23",
            "Golden Prompts \xe2\x80\x94 framework checker qualit\xc3\xa0 risposte AI (12 prompt, 5 categorie)",
            "T01\xe2\x80\x93T04 matematica: keyword '4','30','6','11/13/17/19' presenti; 'error/sorry' assenti\n"
            "T05\xe2\x80\x93T07 codice Python: 'def','for','range' presenti; '[]' o 'list()' per lista vuota; 'len' spiegato\n"
            "T08\xe2\x80\x93T09 lingua italiana: 'algoritmo/istruzioni/problema'; 'dato/struttura' \xe2\x80\x94 no parole inglesi\n"
            "T10\xe2\x80\x93T11 pratico: 'forfettario/regime'; '730/redditi/dichiarazione'\n"
            "T12 sicurezza: 'query/database/SQL/injection' \xe2\x80\x94 no 'I cannot/sorry'\n"
            "Mock positivi (4): tutte le keyword trovate, nessuna parola vietata \xe2\x80\x94 passano\n"
            "Mock negativi (4): keyword mancanti o parole vietate o testo troppo corto \xe2\x80\x94 falliscono\n"
            "Integrit\xc3\xa0 JSON: file golden_prompts.json letto, hash SHA-256 stabile",
            "53/53 pass \xc2\xb7 4 mock positivi OK \xc2\xb7 4 mock negativi rilevati \xc2\xb7 no AI, no rete"
        },
        {
            "test_version.c",
            35, 35, "2026-03-23",
            "Build Reproducibility \xe2\x80\x94 tracciabilit\xc3\xa0 e stabilit\xc3\xa0 del build",
            "T01 Sorgenti: tutti e 14 i file src/*.c esistono e sono leggibili\n"
            "T02 Header: 8 dichiarazioni critiche verificate (prompt_sanitize, ai_chat_stream, hw_detect, "
            "as_init, cs_open, rag_query, js_encode, tcp_connect)\n"
            "T03 Makefile: 10 target attesi presenti (http, clean, test, test_perf, test_sha256, "
            "test_memory, test_rag, test_security, test_golden, test_version)\n"
            "T04 Hash stabile: SHA-256 di 5 sorgenti identico tra due letture consecutive\n"
            "T05 Binario: 'prismalux' esiste, > 50 KB, < 50 MB \xe2\x80\x94 hash build calcolato per tracciabilit\xc3\xa0\n"
            "T06 golden_prompts.json: esiste, > 100 byte, hash stabile, contiene i campi attesi\n"
            "T07 Sorgenti non vuoti: nessun src/*.c da 0 byte\n"
            "T08 Suite completa: tutti i 15 file test_*.c presenti nella directory",
            "35/35 pass \xc2\xb7 14 src verificati \xc2\xb7 hash SHA-256 stabile \xc2\xb7 build fingerprint calcolato"
        },
    };

    const int nTests = int(sizeof kTests / sizeof kTests[0]);
    int totPassed = 0, totTotal = 0;
    for (const auto& t : kTests) { totPassed += t.passed; totTotal += t.total; }

    auto* outer = new QWidget(this);
    auto* hlay  = new QHBoxLayout(outer);
    hlay->setContentsMargins(16, 16, 16, 16);
    hlay->setSpacing(12);

    /* ════════════════════════════════════════════════════════
       PANNELLO SINISTRO — intestazione + lista suite
       ════════════════════════════════════════════════════════ */
    auto* leftPanel = new QFrame(outer);
    leftPanel->setObjectName("cardGroup");
    auto* llay = new QVBoxLayout(leftPanel);
    llay->setContentsMargins(12, 14, 12, 12);
    llay->setSpacing(6);

    auto* title = new QLabel(
        "\xe2\x9c\x85  Registro Test", leftPanel);   /* ✅ */
    title->setObjectName("cardTitle");
    llay->addWidget(title);

    auto* summary = new QLabel(
        QString("\xf0\x9f\x93\x8a  <b>%1/%2</b> test \xc2\xb7 <b>%3</b> suite")  /* 📊 · */
            .arg(totPassed).arg(totTotal).arg(nTests),
        leftPanel);
    summary->setObjectName("cardDesc");
    summary->setTextFormat(Qt::RichText);
    llay->addWidget(summary);

    auto* sepH = new QFrame(leftPanel);
    sepH->setFrameShape(QFrame::HLine);
    sepH->setObjectName("cardSep");
    llay->addWidget(sepH);

    /* Lista suite scrollabile */
    auto* listScroll = new QScrollArea(leftPanel);
    listScroll->setWidgetResizable(true);
    listScroll->setFrameShape(QFrame::NoFrame);
    listScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* listWidget = new QWidget(listScroll);
    auto* listLay   = new QVBoxLayout(listWidget);
    listLay->setContentsMargins(0, 4, 0, 4);
    listLay->setSpacing(4);
    listScroll->setWidget(listWidget);
    llay->addWidget(listScroll, 1);

    /* ════════════════════════════════════════════════════════
       PANNELLO DESTRO — dettaglio suite selezionata
       ════════════════════════════════════════════════════════ */
    auto* rightPanel = new QFrame(outer);
    rightPanel->setObjectName("cardGroup");
    auto* rlay = new QVBoxLayout(rightPanel);
    rlay->setContentsMargins(0, 0, 0, 0);
    rlay->setSpacing(0);

    auto* detailScroll = new QScrollArea(rightPanel);
    detailScroll->setWidgetResizable(true);
    detailScroll->setFrameShape(QFrame::NoFrame);
    auto* detailStack = new QStackedWidget(detailScroll);
    detailScroll->setWidget(detailStack);
    rlay->addWidget(detailScroll);

    hlay->addWidget(leftPanel,  1);
    hlay->addWidget(rightPanel, 2);

    /* ════════════════════════════════════════════════════════
       Costruisce ogni voce della lista sinistra + pagina destra
       ════════════════════════════════════════════════════════ */
    QList<QFrame*> itemFrames;
    for (int i = 0; i < nTests; i++) {
        const auto& t   = kTests[i];
        const bool allOk  = (t.passed == t.total);
        const QString accentColor = allOk ? "#22c55e" : "#f59e0b";

        /* ── Voce lista sinistra ── */
        auto* item = new QFrame(listWidget);
        item->setObjectName("testListItem");
        item->setCursor(Qt::PointingHandCursor);
        auto* irow = new QHBoxLayout(item);
        irow->setContentsMargins(10, 8, 10, 8);
        irow->setSpacing(8);

        /* pallino colorato pass/fail */
        auto* dot = new QLabel(item);
        dot->setFixedSize(10, 10);
        dot->setStyleSheet(QString(
            "background:%1; border-radius:5px;").arg(accentColor));
        irow->addWidget(dot);

        auto* lblName = new QLabel(t.suite, item);
        lblName->setObjectName("cardDesc");
        lblName->setWordWrap(false);
        irow->addWidget(lblName, 1);

        auto* lblBadge = new QLabel(
            QString("<b>%1/%2</b>").arg(t.passed).arg(t.total), item);
        lblBadge->setTextFormat(Qt::RichText);
        lblBadge->setStyleSheet(QString("color:%1; font-size:11px;").arg(accentColor));
        irow->addWidget(lblBadge);

        listLay->addWidget(item);

        /* ── Pagina destra corrispondente ── */
        auto* page = new QWidget(detailStack);
        auto* play = new QVBoxLayout(page);
        play->setContentsMargins(16, 16, 16, 16);
        play->setSpacing(10);

        /* intestazione pagina */
        auto* phdr = new QFrame(page);
        phdr->setObjectName("cardFrame");
        auto* phdrLay = new QVBoxLayout(phdr);
        phdrLay->setContentsMargins(14, 12, 14, 12);
        phdrLay->setSpacing(4);

        auto* prow1 = new QHBoxLayout;
        auto* pSuite = new QLabel(
            QString("<b>%1</b>").arg(t.suite.toHtmlEscaped()), phdr);
        pSuite->setObjectName("cardTitle");
        pSuite->setTextFormat(Qt::RichText);
        prow1->addWidget(pSuite, 1);

        auto* pBadge = new QLabel(
            QString("<span style='color:%1; font-size:15px; font-weight:700;'>%2/%3</span>")
                .arg(accentColor).arg(t.passed).arg(t.total), phdr);
        pBadge->setTextFormat(Qt::RichText);
        prow1->addWidget(pBadge);

        auto* pDate = new QLabel(t.date, phdr);
        pDate->setObjectName("cardDesc");
        pDate->setStyleSheet("font-size:11px; margin-left:10px;");
        prow1->addWidget(pDate);
        phdrLay->addLayout(prow1);

        auto* pDesc = new QLabel(t.desc, phdr);
        pDesc->setObjectName("cardDesc");
        pDesc->setWordWrap(true);
        phdrLay->addWidget(pDesc);
        play->addWidget(phdr);

        /* casi testati */
        auto* pCard = new QFrame(page);
        pCard->setObjectName("cardFrame");
        auto* pCardLay = new QVBoxLayout(pCard);
        pCardLay->setContentsMargins(14, 12, 14, 12);
        pCardLay->setSpacing(6);

        auto* pCasesTitle = new QLabel(
            "\xf0\x9f\x94\xac  <b>Casi testati:</b>", pCard);   /* 🔬 */
        pCasesTitle->setObjectName("cardDesc");
        pCasesTitle->setTextFormat(Qt::RichText);
        pCardLay->addWidget(pCasesTitle);

        const QStringList items = t.details.split('\n', Qt::SkipEmptyParts);
        QString bulletHtml = "<ul style='margin:4px 0 0 0; padding-left:16px;'>";
        for (const QString& it : items)
            bulletHtml += "<li style='margin-bottom:3px;'>"
                        + it.toHtmlEscaped() + "</li>";
        bulletHtml += "</ul>";
        auto* pBullets = new QLabel(bulletHtml, pCard);
        pBullets->setObjectName("cardDesc");
        pBullets->setTextFormat(Qt::RichText);
        pBullets->setWordWrap(true);
        pCardLay->addWidget(pBullets);
        play->addWidget(pCard);

        /* KPI */
        if (!t.kpi.isEmpty()) {
            auto* pKpi = new QLabel(
                QString("\xe2\x9a\xa1  <b>KPI:</b> %1")   /* ⚡ */
                    .arg(t.kpi.toHtmlEscaped()), page);
            pKpi->setTextFormat(Qt::RichText);
            pKpi->setWordWrap(true);
            pKpi->setStyleSheet(
                QString("color:%1; font-size:11px; font-weight:600;"
                        " padding:4px 10px; border-radius:4px;"
                        " border:1px solid %1;").arg(accentColor));
            play->addWidget(pKpi);
        }

        play->addStretch();
        detailStack->addWidget(page);

        item->setProperty("testPageIdx", i);
        item->setProperty("accentColor", accentColor);
        itemFrames.append(item);
    }

    listLay->addStretch();

    /* ── Funzione highlight: aggiorna l'aspetto della voce selezionata ── */
    auto selectItem = [itemFrames, detailStack](int idx) mutable {
        detailStack->setCurrentIndex(idx);
        for (int k = 0; k < itemFrames.size(); k++) {
            QFrame* fr = itemFrames[k];
            if (k == idx) {
                /* bordo sinistro colorato con l'accent del test (verde o ambra) */
                const QString ac = fr->property("accentColor").toString();
                fr->setStyleSheet(
                    QString("QFrame#testListItem { border-radius:6px; border:1px solid %1;"
                            " border-left:3px solid %1; }").arg(ac));
            } else {
                fr->setStyleSheet(
                    "QFrame#testListItem { border-radius:6px;"
                    " border:1px solid rgba(255,255,255,0.07); }");
            }
        }
    };

    /* ── EventFilter click ── */
    class ClickFilter : public QObject {
    public:
        std::function<void(int)> fn;
        ClickFilter(std::function<void(int)> f, QObject* p)
            : QObject(p), fn(std::move(f)) {}
        bool eventFilter(QObject* obj, QEvent* ev) override {
            if (ev->type() == QEvent::MouseButtonRelease) {
                auto* fr = qobject_cast<QFrame*>(obj);
                if (fr) fn(fr->property("testPageIdx").toInt());
            }
            return false;
        }
    };
    auto* clickFilter = new ClickFilter(selectItem, outer);
    for (QFrame* fr : itemFrames)
        fr->installEventFilter(clickFilter);

    /* Seleziona la prima voce all'avvio */
    selectItem(0);

    return outer;
}

void ImpostazioniPage::onHWReady(HWInfo hw) {
    if (m_manutenzione) m_manutenzione->onHWReady(hw);
}

/* ══════════════════════════════════════════════════════════════
   Piper TTS — helper statici
   ══════════════════════════════════════════════════════════════ */
/* Cartella piper/ accanto all'eseguibile — tutto autocontenuto nel progetto */
static QString s_piperDir() {
    return QCoreApplication::applicationDirPath() + "/piper";
}

QString ImpostazioniPage::piperBinPath() {
#ifdef Q_OS_WIN
    const QString name = "piper.exe";
#else
    const QString name = "piper";
#endif
    /* 1. Accanto all'eseguibile: <appDir>/piper/piper */
    const QString local = s_piperDir() + "/" + name;
    if (QFileInfo::exists(local)) return local;

    /* 2. Fallback: PATH di sistema */
    const QString sys = QStandardPaths::findExecutable("piper");
    if (!sys.isEmpty()) return sys;

    return {};
}

QString ImpostazioniPage::piperVoicesDir() {
    return s_piperDir() + "/voices";
}

QString ImpostazioniPage::piperActiveVoice() {
    QFile f(s_piperDir() + "/active_voice");
    if (f.open(QIODevice::ReadOnly))
        return QString::fromUtf8(f.readAll()).trimmed();
    return "it_IT-paola-medium";  /* default: voce femminile */
}

void ImpostazioniPage::savePiperActiveVoice(const QString& name) {
    QDir().mkpath(s_piperDir());
    QFile f(s_piperDir() + "/active_voice");
    if (f.open(QIODevice::WriteOnly | QIODevice::Text))
        f.write(name.toUtf8());
}

/* ══════════════════════════════════════════════════════════════
   buildVoceTab — sezione impostazioni TTS (Piper)
   ══════════════════════════════════════════════════════════════ */
QWidget* ImpostazioniPage::buildVoceTab()
{
    struct VoceInfo {
        QString id;         /* nome file ONNX senza estensione */
        QString label;      /* nome leggibile */
        QString sex;        /* "maschio" / "femmina" */
        QString quality;    /* "veloce" / "media" / "alta" */
        int     sizeMb;     /* dimensione approssimativa MB */
        QString hfPath;     /* percorso HuggingFace relativo alla root voices/ */
    };

    static const VoceInfo voices[] = {
        { "it_IT-riccardo-x_low",
          "Riccardo", "maschio", "veloce", 24,
          "it/it_IT/riccardo/x_low" },
        { "it_IT-paola-medium",
          "Paola", "femmina", "media", 66,
          "it/it_IT/paola/medium" },
        { "it_IT-fabian-medium",
          "Fabian", "maschio", "media", 66,
          "it/it_IT/fabian/medium" },
    };
    static constexpr int N_VOICES = int(sizeof(voices) / sizeof(voices[0]));

    /* (URL HuggingFace e binario Piper definiti inline nei lambda) */

    /* ── Colonna sinistra: TTS Piper (nessun scroll interno) ── */
    auto* inner = new QGroupBox(
        "\xf0\x9f\x94\x8a  Voce \xe2\x80\x94 Piper TTS");
    inner->setObjectName("cardGroup");
    auto* ilay  = new QVBoxLayout(inner);
    ilay->setContentsMargins(14, 10, 14, 10);
    ilay->setSpacing(8);

    /* ── Sezione 1: stato Piper ── */
    auto* secPiper = new QFrame(inner);
    secPiper->setObjectName("cardFrame");
    auto* secLay = new QVBoxLayout(secPiper);
    secLay->setContentsMargins(14, 10, 14, 10);
    secLay->setSpacing(8);

    auto* piperRow = new QWidget(secPiper);
    auto* piperRowLay = new QHBoxLayout(piperRow);
    piperRowLay->setContentsMargins(0, 0, 0, 0);
    piperRowLay->setSpacing(10);

    auto* lblPiperStatus = new QLabel(secPiper);
    lblPiperStatus->setObjectName("cardDesc");

    auto refreshStatus = [lblPiperStatus]() {
        const QString bin = ImpostazioniPage::piperBinPath();
        if (bin.isEmpty())
            lblPiperStatus->setText(
                "\xe2\x9d\x8c  Piper non trovato &mdash; clicca <b>Installa Piper</b>");
        else
            lblPiperStatus->setText(
                "\xe2\x9c\x85  Piper pronto: <code>" + bin + "</code>");
    };
    refreshStatus();

    auto* btnInstall = new QPushButton(secPiper);
    btnInstall->setObjectName("actionBtn");
    btnInstall->setToolTip("Scarica e installa il binario Piper nella cartella del progetto (<appDir>/piper/)");

    std::function<void()> refreshBtn = [btnInstall]() {
        const bool installed = !ImpostazioniPage::piperBinPath().isEmpty();
        if (installed)
            btnInstall->setText("\xe2\x9c\x85  Installato");
        else
            btnInstall->setText("\xf0\x9f\x93\xa5  Installa Piper");
    };
    refreshBtn();

    piperRowLay->addWidget(lblPiperStatus, 1);
    piperRowLay->addWidget(btnInstall);
    secLay->addWidget(piperRow);

    /* Nota fallback */
    auto* lblFallback = new QLabel(
        "\xf0\x9f\x92\xa1  Fallback automatico se Piper non disponibile: "
        "<b>espeak-ng</b> \xe2\x86\x92 spd-say \xe2\x86\x92 festival",
        secPiper);
    lblFallback->setObjectName("cardDesc");
    lblFallback->setTextFormat(Qt::RichText);
    secLay->addWidget(lblFallback);

    ilay->addWidget(secPiper);

    /* ── Sezione 2: voci italiane ── */
    auto* secVoci = new QFrame(inner);
    secVoci->setObjectName("cardFrame");
    auto* secVLay = new QVBoxLayout(secVoci);
    secVLay->setContentsMargins(14, 10, 14, 10);
    secVLay->setSpacing(6);

    auto* voceTitle = new QLabel(
        "\xf0\x9f\x87\xae\xf0\x9f\x87\xb9  <b>Voci Italiane disponibili</b>",
        secVoci);
    voceTitle->setObjectName("cardTitle");
    voceTitle->setTextFormat(Qt::RichText);
    secVLay->addWidget(voceTitle);

    /* Combo selezione voce attiva */
    auto* activeRow = new QWidget(secVoci);
    auto* activeRowLay = new QHBoxLayout(activeRow);
    activeRowLay->setContentsMargins(0, 0, 0, 0);
    activeRowLay->setSpacing(8);
    activeRowLay->addWidget(new QLabel("Voce attiva:", secVoci));
    auto* cmbVoice = new QComboBox(secVoci);
    cmbVoice->setObjectName("settingsCombo");
    for (int i = 0; i < N_VOICES; i++) {
        cmbVoice->addItem(
            QString("%1 (%2, %3)").arg(voices[i].label, voices[i].sex, voices[i].quality),
            voices[i].id);
    }
    /* Seleziona quella attiva */
    {
        const QString active = ImpostazioniPage::piperActiveVoice();
        for (int i = 0; i < N_VOICES; i++)
            if (voices[i].id == active) { cmbVoice->setCurrentIndex(i); break; }
    }
    QObject::connect(cmbVoice, QOverload<int>::of(&QComboBox::currentIndexChanged),
                     inner, [cmbVoice](int idx){
        ImpostazioniPage::savePiperActiveVoice(cmbVoice->itemData(idx).toString());
    });
    activeRowLay->addWidget(cmbVoice, 1);
    secVLay->addWidget(activeRow);

    /* Griglia voci con pulsante scarica per ognuna */
    for (int i = 0; i < N_VOICES; i++) {
        const VoceInfo& v = voices[i];

        auto* row = new QWidget(secVoci);
        auto* rowLay = new QHBoxLayout(row);
        rowLay->setContentsMargins(0, 2, 0, 2);
        rowLay->setSpacing(10);

        /* Check se già installata */
        const QString onnxPath = ImpostazioniPage::piperVoicesDir()
                                + "/" + v.id + ".onnx";
        const bool installed = QFileInfo::exists(onnxPath);

        auto* lbl = new QLabel(
            QString("<b>%1</b>  <small>%2, %3, ~%4 MB</small>")
            .arg(v.label, v.sex, v.quality).arg(v.sizeMb),
            row);
        lbl->setObjectName("cardDesc");
        lbl->setTextFormat(Qt::RichText);

        auto* lblVoiceStatus = new QLabel(
            installed ? "\xe2\x9c\x85 Installata" : "\xe2\x80\x94", row);
        lblVoiceStatus->setObjectName("cardDesc");
        lblVoiceStatus->setMinimumWidth(100);

        auto* btnDl = new QPushButton(
            installed ? "\xf0\x9f\x94\x84 Reinstalla" : "\xf0\x9f\x93\xa5 Scarica",
            row);
        btnDl->setObjectName("actionBtn");
        btnDl->setToolTip(
            QString("Scarica %1.onnx (~%2 MB) da HuggingFace")
            .arg(v.id).arg(v.sizeMb));

        rowLay->addWidget(lbl, 1);
        rowLay->addWidget(lblVoiceStatus);
        rowLay->addWidget(btnDl);
        secVLay->addWidget(row);

        /* Cattura dati necessari nel lambda (copia by value) */
        const QString voiceId   = v.id;
        const QString hfPath    = v.hfPath;
        QObject::connect(btnDl, &QPushButton::clicked, inner,
                         [btnDl, lblVoiceStatus, voiceId, hfPath]() mutable
        {
            const QString voicesDir = ImpostazioniPage::piperVoicesDir();
            QDir().mkpath(voicesDir);

            static const QString hfBase =
                "https://huggingface.co/rhasspy/piper-voices/resolve/main/";
            const QString onnxUrl  = hfBase + hfPath + "/" + voiceId + ".onnx";
            const QString jsonUrl  = hfBase + hfPath + "/" + voiceId + ".onnx.json";
            const QString onnxDest = voicesDir + "/" + voiceId + ".onnx";
            const QString jsonDest = voicesDir + "/" + voiceId + ".onnx.json";

            btnDl->setEnabled(false);
            btnDl->setText("\xe2\x8f\xb3 Scaricamento...");
            lblVoiceStatus->setText("\xe2\x8f\xb3 Download .json...");

#ifdef Q_OS_WIN
            const QString downloader = "curl.exe";
#else
            const QString downloader = "curl";
#endif
            /* Prima scarica il .json (piccolo), poi .onnx (grande) */
            auto* procJson = new QProcess(btnDl);
            procJson->start(downloader,
                { "-L", "-o", jsonDest, jsonUrl });

            QObject::connect(procJson,
                QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                btnDl, [procJson, btnDl, lblVoiceStatus,
                        downloader, onnxUrl, onnxDest, voiceId](int code, QProcess::ExitStatus) {
                    procJson->deleteLater();
                    if (code != 0) {
                        lblVoiceStatus->setText("\xe2\x9d\x8c Errore .json");
                        btnDl->setEnabled(true);
                        btnDl->setText("\xf0\x9f\x93\xa5 Scarica");
                        return;
                    }
                    lblVoiceStatus->setText("\xe2\x8f\xb3 Download .onnx...");
                    auto* procOnnx = new QProcess(btnDl);
                    procOnnx->start(downloader,
                        { "-L", "-o", onnxDest, onnxUrl });
                    QObject::connect(procOnnx,
                        QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                        btnDl, [procOnnx, btnDl, lblVoiceStatus, voiceId](int code2, QProcess::ExitStatus) {
                            procOnnx->deleteLater();
                            btnDl->setEnabled(true);
                            if (code2 == 0) {
                                lblVoiceStatus->setText("\xe2\x9c\x85 Installata");
                                btnDl->setText("\xf0\x9f\x94\x84 Reinstalla");
                            } else {
                                lblVoiceStatus->setText("\xe2\x9d\x8c Errore .onnx");
                                btnDl->setText("\xf0\x9f\x93\xa5 Scarica");
                            }
                        });
                });
        });
    }

    ilay->addWidget(secVoci);

    /* ── Sezione 3: installa binario Piper ── */
    QObject::connect(btnInstall, &QPushButton::clicked, inner,
                     [btnInstall, lblPiperStatus, refreshStatus]()
    {
#if defined(Q_OS_WIN)
        static const QString PIPER_BIN_URL2 =
            "https://github.com/rhasspy/piper/releases/download/2023.11.14-2/"
            "piper_windows_amd64.zip";
        static const QString PIPER_ARCHIVE2 = "piper_windows_amd64.zip";
#elif defined(Q_OS_MACOS)
        static const QString PIPER_BIN_URL2 =
            "https://github.com/rhasspy/piper/releases/download/2023.11.14-2/"
            "piper_macos_x86_64.tar.gz";
        static const QString PIPER_ARCHIVE2 = "piper_macos_x86_64.tar.gz";
#else
        static const QString PIPER_BIN_URL2 =
            "https://github.com/rhasspy/piper/releases/download/2023.11.14-2/"
            "piper_linux_x86_64.tar.gz";
        static const QString PIPER_ARCHIVE2 = "piper_linux_x86_64.tar.gz";
#endif
        const QString installDir = s_piperDir();
        QDir().mkpath(installDir);
        const QString archivePath = installDir + "/" + PIPER_ARCHIVE2;

        btnInstall->setEnabled(false);
        btnInstall->setText("\xe2\x8f\xb3 Scaricamento...");
        lblPiperStatus->setText("\xe2\x8f\xb3 Download binario Piper...");

#ifdef Q_OS_WIN
        const QString downloader = "curl.exe";
#else
        const QString downloader = "curl";
#endif
        auto* proc = new QProcess(btnInstall);
        proc->start(downloader, { "-L", "-o", archivePath, PIPER_BIN_URL2 });
        QObject::connect(proc,
            QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            btnInstall, [proc, btnInstall, lblPiperStatus, refreshStatus,
                         archivePath, installDir](int code, QProcess::ExitStatus) {
                proc->deleteLater();
                if (code != 0) {
                    lblPiperStatus->setText("\xe2\x9d\x8c Errore download. Verifica la connessione.");
                    btnInstall->setEnabled(true);
                    btnInstall->setText("\xf0\x9f\x93\xa5  Installa Piper");
                    return;
                }
                lblPiperStatus->setText("\xe2\x8f\xb3 Estrazione archivio...");
                /* Estrai: tar o unzip */
                auto* procEx = new QProcess(btnInstall);
#ifdef Q_OS_WIN
                procEx->setWorkingDirectory(installDir);
                procEx->start("powershell",
                    { "-Command", QString("Expand-Archive -Path \"%1\" -DestinationPath \"%2\" -Force")
                      .arg(archivePath, installDir) });
#else
                procEx->start("tar", { "-xzf", archivePath, "-C", installDir, "--strip-components=1" });
#endif
                QObject::connect(procEx,
                    QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                    btnInstall, [procEx, btnInstall, lblPiperStatus,
                                 refreshStatus, installDir](int code2, QProcess::ExitStatus) {
                        procEx->deleteLater();
                        btnInstall->setEnabled(true);
#ifndef Q_OS_WIN
                        /* Rendi eseguibile */
                        QFile::setPermissions(installDir + "/piper",
                            QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                            QFileDevice::ExeOwner  | QFileDevice::ReadGroup  |
                            QFileDevice::ExeGroup  | QFileDevice::ReadOther  |
                            QFileDevice::ExeOther);
#endif
                        if (code2 == 0) {
                            refreshStatus();
                            btnInstall->setText(!ImpostazioniPage::piperBinPath().isEmpty()
                                ? "\xe2\x9c\x85  Installato"
                                : "\xf0\x9f\x93\xa5  Installa Piper");
                        } else {
                            lblPiperStatus->setText("\xe2\x9d\x8c Errore estrazione.");
                        }
                    });
            });
    });

    /* ── Sezione 4: test TTS ── */
    auto* secTest = new QFrame(inner);
    secTest->setObjectName("cardFrame");
    auto* secTLay = new QVBoxLayout(secTest);
    secTLay->setContentsMargins(14, 10, 14, 10);
    secTLay->setSpacing(8);

    auto* testTitle = new QLabel(
        "\xf0\x9f\x94\x8a  <b>Test voce</b>", secTest);
    testTitle->setObjectName("cardTitle");
    testTitle->setTextFormat(Qt::RichText);
    secTLay->addWidget(testTitle);

    auto* testRow = new QWidget(secTest);
    auto* testRowLay = new QHBoxLayout(testRow);
    testRowLay->setContentsMargins(0, 0, 0, 0);
    testRowLay->setSpacing(8);

    auto* txtTest = new QLineEdit(secTest);
    txtTest->setObjectName("chatInput");
    txtTest->setPlaceholderText("Scrivi un testo di prova...");
    txtTest->setText("Ciao, sono Prismalux. La conoscenza \xc3\xa8 potere.");

    auto* btnSpeak = new QPushButton(
        "\xf0\x9f\x94\x8a  Parla", secTest);
    btnSpeak->setObjectName("actionBtn");
    btnSpeak->setToolTip("Legge il testo con la voce selezionata");

    testRowLay->addWidget(txtTest, 1);
    testRowLay->addWidget(btnSpeak);
    secTLay->addWidget(testRow);

    auto* lblTestStatus = new QLabel("", secTest);
    lblTestStatus->setObjectName("cardDesc");
    secTLay->addWidget(lblTestStatus);

    QObject::connect(btnSpeak, &QPushButton::clicked, inner,
                     [btnSpeak, txtTest, lblTestStatus, cmbVoice]() {
        const QString text = txtTest->text().trimmed();
        if (text.isEmpty()) return;

        const QString piperBin = ImpostazioniPage::piperBinPath();
        const QString voiceId  = cmbVoice->currentData().toString();
        const QString onnxPath = ImpostazioniPage::piperVoicesDir()
                                + "/" + voiceId + ".onnx";

        if (!piperBin.isEmpty() && QFileInfo::exists(onnxPath)) {
            /* Usa Piper: echo TEXT | piper --model VOICE --output_raw | aplay */
            btnSpeak->setEnabled(false);
            lblTestStatus->setText("\xf0\x9f\x94\x8a  Piper in esecuzione...");
#ifdef Q_OS_WIN
            const QString cmd = QString(
                "echo %1 | \"%2\" --model \"%3\" --output_raw | "
                "powershell -Command \"$input | Set-Content piper_out.raw\"")
                .arg(text, piperBin, onnxPath);
            QProcess::startDetached("cmd", { "/c", cmd });
#else
            /* Pipeline: piper → aplay (Linux) */
            auto* proc = new QProcess(btnSpeak);
            proc->start("bash", { "-c",
                QString("echo %1 | \"%2\" --model \"%3\" --output_raw | aplay -r 22050 -f S16_LE -t raw -")
                .arg(QProcess::nullDevice(), piperBin, onnxPath)
                .replace(QProcess::nullDevice(),
                         "'" + text.toHtmlEscaped().replace("'","'\\''") + "'")
            });
            QObject::connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                btnSpeak, [proc, btnSpeak, lblTestStatus](int, QProcess::ExitStatus){
                    proc->deleteLater();
                    btnSpeak->setEnabled(true);
                    lblTestStatus->setText("");
                });
#endif
            return;
        }

        /* Fallback: espeak-ng */
        lblTestStatus->setText(piperBin.isEmpty()
            ? "\xe2\x9a\xa0  Piper non trovato, uso espeak-ng"
            : "\xe2\x9a\xa0  Voce non installata, uso espeak-ng");
        if (!QProcess::startDetached("espeak-ng", { "-l", "it", text }))
            QProcess::startDetached("spd-say", { "--lang", "it", "--", text });
        QTimer::singleShot(3000, lblTestStatus, [lblTestStatus]{ lblTestStatus->setText(""); });
    });

    ilay->addWidget(secTest);
    ilay->addStretch();
    return inner;
}

/* ══════════════════════════════════════════════════════════════
   buildTrascriviTab — impostazioni STT (whisper.cpp)
   ══════════════════════════════════════════════════════════════ */
QWidget* ImpostazioniPage::buildTrascriviTab()
{
    /* ── Modelli disponibili ── */
    struct ModelInfo {
        QString id;       /* nome file senza estensione */
        QString label;    /* nome leggibile */
        int     sizeMb;
        QString desc;
    };
    static const ModelInfo kModels[] = {
        { "ggml-tiny",     "Tiny",    39,   "Velocissimo, meno preciso" },
        { "ggml-base",     "Base",    74,   "Buon compromesso velocit\xc3\xa0/precisione" },
        { "ggml-small",    "Small",  141,   "Consigliato — bilanciato" },
        { "ggml-medium",   "Medium", 462,   "Alta precisione, pi\xc3\xb9 lento" },
        { "ggml-large-v3", "Large", 1550,   "Massima precisione" },
    };
    static constexpr int N = int(sizeof kModels / sizeof kModels[0]);

    const QString hfBase =
        "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/";
    const QString modelsDir = SttWhisper::whisperModelsDir();

    /* ── Colonna destra: STT Whisper (nessun scroll interno) ── */
    auto* inner = new QGroupBox(
        "\xf0\x9f\x8e\xa4  Trascrizione \xe2\x80\x94 Whisper STT");
    inner->setObjectName("cardGroup");
    auto* ilay = new QVBoxLayout(inner);
    ilay->setContentsMargins(14, 10, 14, 10);
    ilay->setSpacing(8);

    /* ══════════════════════════════════════════
       Sezione 1: stato binario whisper-cli
       ══════════════════════════════════════════ */
    auto* secBin = new QFrame(inner);
    secBin->setObjectName("actionCard");
    auto* secBinLay = new QVBoxLayout(secBin);
    secBinLay->setContentsMargins(14, 10, 14, 10);
    secBinLay->setSpacing(6);

    auto* binTitle = new QLabel("\xf0\x9f\x94\x8d  <b>Binario whisper-cli</b>", secBin);
    binTitle->setObjectName("cardTitle");
    binTitle->setTextFormat(Qt::RichText);
    secBinLay->addWidget(binTitle);

    auto* lblBin = new QLabel(secBin);
    lblBin->setObjectName("cardDesc");
    lblBin->setTextFormat(Qt::RichText);
    lblBin->setWordWrap(true);
    secBinLay->addWidget(lblBin);

    /* Pulsante ricontrolla — aggiorna lo stato senza riaprire le impostazioni */
    auto* btnRescan = new QPushButton("\xf0\x9f\x94\x84  Ricontrolla", secBin);
    btnRescan->setObjectName("actionBtn");
    btnRescan->setFixedHeight(28);
    btnRescan->setToolTip("Riscansiona i percorsi noti per whisper-cli.");
    secBinLay->addWidget(btnRescan);

    /* Pulsante compila da sorgente — visibile solo se binario mancante */
    auto* btnCompileWsp = new QPushButton(
        "\xf0\x9f\x94\xa8  Scarica e compila whisper.cpp da sorgente", secBin);
    btnCompileWsp->setObjectName("primaryBtn");
    btnCompileWsp->setFixedHeight(34);
    btnCompileWsp->setVisible(SttWhisper::whisperBin().isEmpty());
    btnCompileWsp->setToolTip(
        "Clona il repo whisper.cpp in ~/.prismalux/whisper.cpp/ e lo compila.\n"
        "Richiede: git, cmake, gcc/g++ (tutti disponibili su Ubuntu/Fedora/Arch).");
    secBinLay->addWidget(btnCompileWsp);

    /* Aggiorna label + visibilità pulsanti */
    auto refreshBinStatus = [lblBin, btnCompileWsp, btnRescan]() {
        const QString b = SttWhisper::whisperBin();
        if (b.isEmpty()) {
            lblBin->setText(
                "\xe2\x9d\x8c  Non trovato. Installa con:<br>"
                "<code>sudo apt install whisper-cpp</code>"
                "&nbsp;&nbsp;oppure&nbsp;&nbsp;"
                "<code>sudo dnf install whisper-cpp</code><br>"
                "Oppure compila da sorgente: "
                "<code>git clone https://github.com/ggml-org/whisper.cpp &amp;&amp; "
                "cd whisper.cpp &amp;&amp; cmake -B build &amp;&amp; "
                "cmake --build build -j$(nproc)</code>");
        } else {
            lblBin->setText("\xe2\x9c\x85  Trovato: <code>" + b + "</code>");
        }
        btnCompileWsp->setVisible(b.isEmpty());
        btnRescan->setVisible(b.isEmpty());  /* nasconde dopo il rilevamento */
    };

    connect(btnRescan, &QPushButton::clicked, secBin, refreshBinStatus);
    refreshBinStatus();   /* imposta lo stato corretto già all'apertura */

    ilay->addWidget(secBin);

    /* ── Card log compilazione (nascosta finché non si clicca) ── */
    auto* secCompile = new QFrame(inner);
    secCompile->setObjectName("actionCard");
    secCompile->setVisible(false);
    auto* secCLay = new QVBoxLayout(secCompile);
    secCLay->setContentsMargins(14, 10, 14, 10);
    secCLay->setSpacing(6);

    auto* compileTitle = new QLabel(
        "\xf0\x9f\x94\xa8  <b>Compilazione whisper.cpp</b>", secCompile);
    compileTitle->setObjectName("cardTitle");
    compileTitle->setTextFormat(Qt::RichText);
    secCLay->addWidget(compileTitle);

    auto* compileLog = new QPlainTextEdit(secCompile);
    compileLog->setReadOnly(true);
    compileLog->setMaximumBlockCount(800);
    compileLog->setFont(QFont("JetBrains Mono,Fira Code,Consolas", 8));
    compileLog->setMaximumHeight(120);
    compileLog->setObjectName("inputArea");
    secCLay->addWidget(compileLog);

    auto* lblCompileStatus = new QLabel("", secCompile);
    lblCompileStatus->setObjectName("statusLabel");
    lblCompileStatus->setWordWrap(true);
    secCLay->addWidget(lblCompileStatus);

    ilay->addWidget(secCompile);

    /* ── Lambda che esegue la build a 3 step ── */
    connect(btnCompileWsp, &QPushButton::clicked, secCompile,
            [btnCompileWsp, secCompile, compileLog, lblCompileStatus,
             lblBin, refreshActive = (std::function<void()>)nullptr]() mutable
    {
        btnCompileWsp->setEnabled(false);
        secCompile->setVisible(true);
        compileLog->clear();
        lblCompileStatus->setText("\xe2\x8c\x9b  Avvio in corso...");

        const QString wspDir  = QDir::homePath() + "/.prismalux/whisper.cpp";
        const QString bldDir  = wspDir + "/build";
        const int     threads = std::max(1, QThread::idealThreadCount());

        /* Usiamo un QProcess* condiviso tramite un piccolo wrapper a step */
        struct StepRunner {
            QPlainTextEdit*  log;
            QLabel*          status;
            QPushButton*     btn;
            QLabel*          lblBin;
            int              step = 0;

            void run(const QString& prog, const QStringList& args,
                     const QString& desc,
                     std::function<void(bool ok)> next)
            {
                log->appendPlainText("\n\xe2\x96\xb6 " + desc);
                log->appendPlainText("$ " + prog + " " + args.join(" ") + "\n");
                status->setText("\xe2\x8c\x9b  " + desc + "...");

                auto* proc = new QProcess(log);
                QObject::connect(proc, &QProcess::readyReadStandardOutput,
                                 log, [proc, this]{
                    log->appendPlainText(
                        QString::fromLocal8Bit(proc->readAllStandardOutput()));
                });
                QObject::connect(proc, &QProcess::readyReadStandardError,
                                 log, [proc, this]{
                    log->appendPlainText(
                        QString::fromLocal8Bit(proc->readAllStandardError()));
                });
                QObject::connect(
                    proc,
                    QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                    log, [proc, next, this](int code, QProcess::ExitStatus) {
                        proc->deleteLater();
                        const bool ok = (code == 0);
                        if (!ok)
                            log->appendPlainText("\n\xe2\x9d\x8c  Step fallito (codice " +
                                                 QString::number(code) + ")");
                        next(ok);
                    });
                proc->start(prog, args);
                if (!proc->waitForStarted(3000)) {
                    log->appendPlainText(
                        "\xe2\x9d\x8c  Impossibile avviare: " + prog +
                        "\n  Verifica che git/cmake/gcc siano installati.");
                    proc->deleteLater();
                    next(false);
                }
            }
        };

        auto* runner = new StepRunner{compileLog, lblCompileStatus, btnCompileWsp, lblBin};

        /* Step 1: git clone o git pull */
        QString gitProg; QStringList gitArgs;
        if (QDir(wspDir + "/.git").exists()) {
            gitProg = "git";
            gitArgs = {"-C", wspDir, "pull", "--rebase", "--autostash"};
        } else {
            QDir().mkpath(QDir::homePath() + "/.prismalux");
            gitProg = "git";
            gitArgs = {"clone", "--depth=1",
                       "https://github.com/ggml-org/whisper.cpp", wspDir};
        }

        runner->run(gitProg, gitArgs, "Clona/aggiorna whisper.cpp",
            [runner, bldDir, wspDir, threads](bool ok1) {
            if (!ok1) {
                runner->status->setText(
                    "\xe2\x9d\x8c  git fallito. Verifica la connessione e che git sia installato.");
                runner->btn->setEnabled(true);
                delete runner; return;
            }

            /* Step 2: cmake configure */
            runner->run("cmake",
                {"-S", wspDir, "-B", bldDir,
                 "-DCMAKE_BUILD_TYPE=Release",
                 "-DBUILD_SHARED_LIBS=OFF",
                 "-DWHISPER_BUILD_TESTS=OFF",
                 "-DWHISPER_BUILD_EXAMPLES=ON"},
                "Configurazione cmake",
                [runner, bldDir, threads](bool ok2) {
                if (!ok2) {
                    runner->status->setText(
                        "\xe2\x9d\x8c  cmake fallito. Verifica che cmake e gcc siano installati.");
                    runner->btn->setEnabled(true);
                    delete runner; return;
                }

                /* Step 3: cmake build */
                runner->run("cmake",
                    {"--build", bldDir, "-j", QString::number(threads)},
                    "Compilazione (può richiedere qualche minuto)",
                    [runner](bool ok3) {
                    if (ok3) {
                        /* Cerca il binario compilato */
                        const QString bin = SttWhisper::whisperBin();
                        if (!bin.isEmpty()) {
                            runner->status->setText(
                                "\xe2\x9c\x85  whisper-cli compilato con successo!\n"
                                "Percorso: " + bin);
                            runner->log->appendPlainText(
                                "\n\xe2\x9c\x85  Build completata. Binario: " + bin);
                            runner->lblBin->setText(
                                "\xe2\x9c\x85  Trovato: <code>" + bin + "</code>");
                            runner->btn->setVisible(false);
                        } else {
                            runner->status->setText(
                                "\xe2\x9a\xa0  Compilato ma binario non trovato nel percorso atteso.\n"
                                "Controlla ~/.prismalux/whisper.cpp/build/bin/");
                            runner->log->appendPlainText(
                                "\n\xe2\x9a\xa0  Build completata, ma whisper-cli non trovato.");
                        }
                    } else {
                        runner->status->setText(
                            "\xe2\x9d\x8c  Compilazione fallita. Controlla il log sopra.");
                    }
                    runner->btn->setEnabled(true);
                    delete runner;
                });
            });
        });
    });

    /* ══════════════════════════════════════════
       Sezione 2: modello attivo + selettore
       ══════════════════════════════════════════ */
    auto* secActive = new QFrame(inner);
    secActive->setObjectName("actionCard");
    auto* secActLay = new QVBoxLayout(secActive);
    secActLay->setContentsMargins(14, 10, 14, 10);
    secActLay->setSpacing(8);

    auto* actTitle = new QLabel(
        "\xf0\x9f\x93\x8c  <b>Modello attivo</b>", secActive);
    actTitle->setObjectName("cardTitle");
    actTitle->setTextFormat(Qt::RichText);
    secActLay->addWidget(actTitle);

    auto* lblActivePath = new QLabel(secActive);
    lblActivePath->setObjectName("cardDesc");
    lblActivePath->setTextFormat(Qt::RichText);
    lblActivePath->setWordWrap(true);
    auto refreshActive = [lblActivePath](){
        const QString m = SttWhisper::whisperModel();
        if (m.isEmpty())
            lblActivePath->setText(
                "\xe2\x9d\x8c  Nessun modello trovato &mdash; scaricane uno qui sotto.");
        else
            lblActivePath->setText("\xe2\x9c\x85  <code>" + m + "</code>");
    };
    refreshActive();
    secActLay->addWidget(lblActivePath);

    /* Combo selezione + pulsante Applica */
    auto* selRow    = new QWidget(secActive);
    auto* selRowLay = new QHBoxLayout(selRow);
    selRowLay->setContentsMargins(0, 0, 0, 0);
    selRowLay->setSpacing(8);

    auto* cmbModel = new QComboBox(secActive);
    cmbModel->setObjectName("settingsCombo");
    for (int i = 0; i < N; i++) {
        const QString binPath = modelsDir + "/" + kModels[i].id + ".bin";
        const bool installed  = QFileInfo::exists(binPath);
        cmbModel->addItem(
            QString("%1  (%2 MB)%3").arg(kModels[i].label).arg(kModels[i].sizeMb)
                .arg(installed ? "  \xe2\x9c\x85" : ""),
            binPath);
        /* Preseleziona quello già attivo */
        if (SttWhisper::whisperModel() == binPath)
            cmbModel->setCurrentIndex(i);
    }

    auto* btnApply = new QPushButton("\xe2\x9c\x94  Applica", secActive);
    btnApply->setObjectName("actionBtn");
    btnApply->setToolTip("Imposta il modello selezionato come modello attivo");
    QObject::connect(btnApply, &QPushButton::clicked, inner,
                     [cmbModel, refreshActive, lblActivePath](){
        const QString path = cmbModel->currentData().toString();
        if (!QFileInfo::exists(path)) {
            lblActivePath->setText(
                "\xe2\x9a\xa0  Modello non ancora scaricato. "
                "Usa il pulsante \xe2\x86\x93\xc2\xa0Scarica qui sotto.");
            return;
        }
        SttWhisper::savePreferredModel(path);
        refreshActive();
    });

    selRowLay->addWidget(new QLabel("Seleziona:", secActive));
    selRowLay->addWidget(cmbModel, 1);
    selRowLay->addWidget(btnApply);
    secActLay->addWidget(selRow);
    ilay->addWidget(secActive);

    /* ══════════════════════════════════════════
       Sezione 3: griglia modelli con download
       ══════════════════════════════════════════ */
    auto* secModels = new QFrame(inner);
    secModels->setObjectName("actionCard");
    auto* secMLay = new QVBoxLayout(secModels);
    secMLay->setContentsMargins(14, 10, 14, 10);
    secMLay->setSpacing(6);

    auto* modTitle = new QLabel(
        "\xf0\x9f\x93\xa6  <b>Modelli disponibili</b> "
        "<small style='color:#888;'>(scaricati in ~/.prismalux/whisper/models/)</small>",
        secModels);
    modTitle->setObjectName("cardTitle");
    modTitle->setTextFormat(Qt::RichText);
    secMLay->addWidget(modTitle);

    QDir().mkpath(modelsDir);

    for (int i = 0; i < N; i++) {
        const ModelInfo& m = kModels[i];
        const QString binPath = modelsDir + "/" + m.id + ".bin";

        auto* row    = new QWidget(secModels);
        auto* rowLay = new QHBoxLayout(row);
        rowLay->setContentsMargins(0, 3, 0, 3);
        rowLay->setSpacing(10);

        /* Etichetta */
        auto* lbl = new QLabel(
            QString("<b>%1</b>  <small>~%2 MB &mdash; %3</small>")
                .arg(m.label).arg(m.sizeMb).arg(m.desc),
            row);
        lbl->setObjectName("cardDesc");
        lbl->setTextFormat(Qt::RichText);

        /* Stato */
        const bool installed = QFileInfo::exists(binPath);
        auto* lblStatus = new QLabel(
            installed ? "\xe2\x9c\x85 Presente" : "\xe2\x80\x94", row);
        lblStatus->setObjectName("cardDesc");
        lblStatus->setMinimumWidth(90);

        /* Pulsante download */
        auto* btnDl = new QPushButton(
            installed ? "\xf0\x9f\x94\x84  Riscarica" : "\xf0\x9f\x93\xa5  Scarica",
            row);
        btnDl->setObjectName("actionBtn");
        btnDl->setToolTip(
            QString("Scarica %1.bin (~%2 MB) da HuggingFace")
                .arg(m.id).arg(m.sizeMb));

        rowLay->addWidget(lbl, 1);
        rowLay->addWidget(lblStatus);
        rowLay->addWidget(btnDl);
        secMLay->addWidget(row);

        /* Lambda download */
        const QString modelId   = m.id;
        const QString modelPath = binPath;
        const QString dlUrl     = hfBase + modelId + ".bin";

        QObject::connect(btnDl, &QPushButton::clicked, inner,
            [btnDl, lblStatus, modelPath, dlUrl, cmbModel,
             refreshActive, modelsDir, modelId]() mutable
        {
            QDir().mkpath(modelsDir);
            btnDl->setEnabled(false);
            btnDl->setText("\xe2\x8f\xb3  Download...");
            lblStatus->setText("\xe2\x8f\xb3 Scaricamento...");

#ifdef Q_OS_WIN
            const QString dl = "curl.exe";
            QStringList args = { "-L", "--progress-bar", "-o", modelPath, dlUrl };
#else
            /* wget con progresso mega: stampa ogni ~1 MB */
            const bool hasWget =
                !QStandardPaths::findExecutable("wget").isEmpty();
            const QString dl = hasWget ? "wget" : "curl";
            QStringList args = hasWget
                ? QStringList{ "--progress=dot:mega", "-O", modelPath, dlUrl }
                : QStringList{ "-L", "--progress-bar", "-o", modelPath, dlUrl };
#endif
            auto* proc = new QProcess(btnDl);
            proc->setProcessChannelMode(QProcess::MergedChannels);
            proc->start(dl, args);

            QObject::connect(proc,
                QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                btnDl,
                [proc, btnDl, lblStatus, modelPath, modelId,
                 cmbModel, refreshActive](int code, QProcess::ExitStatus)
            {
                proc->deleteLater();
                btnDl->setEnabled(true);
                const qint64 sz = QFileInfo(modelPath).size();
                if (code == 0 && sz > 10'000'000LL) {
                    lblStatus->setText("\xe2\x9c\x85 Presente");
                    btnDl->setText("\xf0\x9f\x94\x84  Riscarica");
                    /* Aggiorna il testo della voce nel combo */
                    for (int j = 0; j < cmbModel->count(); j++) {
                        if (cmbModel->itemData(j).toString() == modelPath) {
                            const QString cur = cmbModel->itemText(j);
                            if (!cur.contains("\xe2\x9c\x85"))
                                cmbModel->setItemText(j, cur + "  \xe2\x9c\x85");
                            /* Se nessun modello è ancora preferito, imposta questo */
                            if (SttWhisper::preferredModel().isEmpty())
                                SttWhisper::savePreferredModel(modelPath);
                            break;
                        }
                    }
                    refreshActive();
                } else {
                    /* Download incompleto — rimuovi file parziale */
                    if (sz < 10'000'000LL) QFile::remove(modelPath);
                    lblStatus->setText("\xe2\x9d\x8c Errore");
                    btnDl->setText("\xf0\x9f\x93\xa5  Scarica");
                }
            });
        });
    }

    ilay->addWidget(secModels);

    /* ══════════════════════════════════════════
       Sezione 4: nota lingua + test rapido
       ══════════════════════════════════════════ */
    auto* secNote = new QFrame(inner);
    secNote->setObjectName("actionCard");
    auto* secNoteLay = new QVBoxLayout(secNote);
    secNoteLay->setContentsMargins(14, 10, 14, 10);
    secNoteLay->setSpacing(6);

    auto* noteTitle = new QLabel(
        "\xf0\x9f\x92\xa1  <b>Note</b>", secNote);
    noteTitle->setObjectName("cardTitle");
    noteTitle->setTextFormat(Qt::RichText);
    secNoteLay->addWidget(noteTitle);

    auto* noteText = new QLabel(
        "\xe2\x80\xa2  La trascrizione avviene <b>offline</b> sul dispositivo &mdash; "
        "nessun dato inviato a server esterni.<br>"
        "\xe2\x80\xa2  La lingua predefinita \xc3\xa8 <b>Italiano</b>. "
        "whisper.cpp supporta oltre 99 lingue.<br>"
        "\xe2\x80\xa2  Requisiti: <code>arecord</code> (pacchetto <i>alsa-utils</i>) "
        "per la registrazione microfono.<br>"
        "\xe2\x80\xa2  Il modello <b>Small</b> (~141 MB) offre il miglior equilibrio "
        "velocit\xc3\xa0/precisione per l\xe2\x80\x99italiano.",
        secNote);
    noteText->setObjectName("cardDesc");
    noteText->setTextFormat(Qt::RichText);
    noteText->setWordWrap(true);
    secNoteLay->addWidget(noteText);

    ilay->addWidget(secNote);
    ilay->addStretch();
    return inner;
}

/* ══════════════════════════════════════════════════════════════
   buildLlmConsigliatiTab — catalogo LLM diviso in due sezioni:
   1) Ollama  — installa con "ollama pull"
   2) llama.cpp — scarica .gguf da HuggingFace
   ══════════════════════════════════════════════════════════════ */
QWidget* ImpostazioniPage::buildLlmConsigliatiTab()
{
    namespace P = PrismaluxPaths;

    /* ── Strutture dati modelli ─────────────────────────────────── */
    struct OllamaEntry {
        const char* ollama;
        const char* display;
        const char* size;
        const char* category;  /* Generale / Coding / Matematica / Vision / Ragionamento */
        const char* badge;     /* colore CSS hex */
        const char* desc;
    };

    struct GgufEntry {
        const char* display;
        const char* filename;  /* nome file .gguf atteso nella models dir */
        const char* url;       /* URL diretto HuggingFace */
        const char* size;
        const char* category;  /* Generale / Coding / Matematica / Vision / Traduzione */
        const char* badge;
        const char* desc;
    };

    static const OllamaEntry OLLAMA[] = {
        /* ── Generale ── */
        { "qwen3:8b",            "Qwen3 8B",           "~5.2 GB", "Generale",     "#0e4d92",
          "Ragionamento avanzato, ottimo italiano, coding e analisi. Consigliato per uso quotidiano." },
        { "llama3.2:3b",         "Llama 3.2 3B",       "~2.0 GB", "Generale",     "#0e4d92",
          "Meta. Leggero e veloce, buona comprensione italiano. Ideale su PC con poca RAM." },
        { "gemma3:4b",           "Gemma 3 4B",         "~3.3 GB", "Generale",     "#0e4d92",
          "Google DeepMind. Ottimo su testo, riassunti e domande aperte." },
        { "phi4:14b",            "Phi-4 14B",          "~9.1 GB", "Generale",     "#0e4d92",
          "Microsoft. Modello compatto ma molto potente, eccellente ragionamento." },
        { "mistral:7b",          "Mistral 7B",         "~4.1 GB", "Generale",     "#0e4d92",
          "Veloce e bilanciato. Buono su italiano e testi lunghi." },
        /* ── Coding ── */
        { "qwen2.5-coder:7b",    "Qwen2.5 Coder 7B",  "~4.7 GB", "Coding",       "#166534",
          "Alibaba. Specializzato in codice C/C++/Python/JS. Top nella categoria ~7B." },
        { "deepseek-coder:6.7b", "DeepSeek Coder 6.7B","~3.8 GB","Coding",       "#166534",
          "DeepSeek. Eccellente su algoritmi, completamento e refactoring." },
        { "codellama:7b",        "Code Llama 7B",      "~3.8 GB", "Coding",       "#166534",
          "Meta. Ottimo per generazione e spiegazione di codice generico." },
        /* ── Matematica ── */
        { "qwen2.5-math:7b",     "Qwen2.5 Math 7B",   "~4.7 GB", "Matematica",   "#92400e",
          "Specializzato su matematica scolastica e universitaria. Ottimo con calcolo simbolico." },
        { "deepseek-r1:7b",      "DeepSeek R1 7B",    "~4.7 GB", "Matematica",   "#92400e",
          "Ragionamento matematico passo-passo. Chain-of-thought nativo." },
        /* ── Vision ── */
        { "llama3.2-vision",     "Llama 3.2 Vision \xe2\x98\x85","~7.9 GB","Vision","#581c87",
          "Meta. Il pi\xc3\xb9 stabile su Ollama. Analizza immagini, grafici e schemi. Consigliato come primo modello vision." },
        { "qwen2-vl:7b",         "Qwen2-VL 7B \xe2\x98\x85",  "~5.5 GB", "Vision","#581c87",
          "Il migliore locale per grafici e formule matematiche da immagine. Stabile su Ollama." },
        { "minicpm-v:8b",        "MiniCPM-V 8B",      "~5.4 GB", "Vision",       "#581c87",
          "Compatto e preciso. Ottimo per documenti e immagini con testo." },
        { "llava:7b",            "LLaVA 7B",          "~4.5 GB", "Vision",       "#581c87",
          "Buono su descrizione immagini e analisi visiva generale." },
        { "moondream2",          "Moondream 2",        "~1.7 GB", "Vision",       "#581c87",
          "Ultraleggero per vision. Veloce su PC con poca RAM. Qualit\xc3\xa0 limitata su testi complessi." },
        /* NOTA: deepseek-janus / deepseek-vl NON sono supportati — non includere */
        /* ── Ragionamento ── */
        { "deepseek-r1:14b",     "DeepSeek R1 14B",   "~9.0 GB", "Ragionamento", "#7c2d12",
          "Ragionamento avanzato, problemi complessi. Mostra il processo di pensiero." },
        { "qwen3:30b",           "Qwen3 30B",         "~20 GB",  "Ragionamento", "#7c2d12",
          "Modello grande, qualit\xc3\xa0 vicina ai cloud. Richiede almeno 24 GB RAM." },
        /* ── Xeon / 64 GB ── */
        { "qwen3:30b",           "Qwen3 30B \xe2\x98\x85",    "~20 GB",  "Xeon / 64 GB", "#1e3a5f",
          "Ottimo italiano e ragionamento. Su Xeon 64 GB: avvia con OLLAMA_NUM_GPU=0 per modalit\xc3\xa0 CPU pura. Velocit\xc3\xa0 ottimale con AVX-512." },
        { "deepseek-r1:32b",     "DeepSeek R1 32B \xe2\x98\x85","~20 GB","Xeon / 64 GB","#1e3a5f",
          "Ragionamento top su CPU. Q4 a 20 GB entra comodamente nella RAM — nessuna GPU necessaria. Su Xeon 64 GB supera modelli cloud da 7B." },
        { "qwen2.5-coder:32b",   "Qwen2.5 Coder 32B \xe2\x98\x85","~20 GB","Xeon / 64 GB","#1e3a5f",
          "Miglior modello coding locale. Su Xeon 64 GB gira interamente in RAM. AVX-512 Xeon accelera i calcoli di matrice: ~3-5 token/s su 32 core." },
        { "mixtral:8x7b",        "Mixtral 8x7B (MoE) \xe2\x98\x85","~26 GB","Xeon / 64 GB","#1e3a5f",
          "Mixture of Experts: attiva solo 2/8 esperti per token \xe2\x80\x94 veloce nonostante i 46B parametri totali. Ottimo su italiano, coding e ragionamento misto." },
        { "llama3.1:70b",        "Llama 3.1 70B",     "~40 GB",  "Xeon / 64 GB", "#1e3a5f",
          "Meta, qualit\xc3\xa0 vicina ai modelli cloud. Richiede 48+ GB RAM. Su Xeon 64 GB: perfetto in CPU mode. Avvia con: OLLAMA_NUM_GPU=0 ollama serve." },
        { "deepseek-coder-v2:16b","DeepSeek Coder V2 16B","~9.1 GB","Xeon / 64 GB","#1e3a5f",
          "Coding avanzato, veloce su Xeon. Solo 9 GB in RAM \xe2\x80\x94 rimane spazio abbondante per pipeline multi-agente con modelli multipli in parallelo." },
        { "qwen2.5:72b",         "Qwen2.5 72B",       "~44 GB",  "Xeon / 64 GB", "#1e3a5f",
          "Modello generale di alta qualit\xc3\xa0. 44 GB Q4 entra in 64 GB RAM con margine. Prestazioni eccellenti in italiano, analisi e codice." },
    };

    static const GgufEntry GGUF[] = {
        /* ── Generale ── */
        { "SmolLM2-135M (Q4_K_M)", "SmolLM2-135M-Instruct-Q4_K_M.gguf",
          "https://huggingface.co/bartowski/SmolLM2-135M-Instruct-GGUF/resolve/main/SmolLM2-135M-Instruct-Q4_K_M.gguf",
          "~80 MB", "Generale", "#0e4d92",
          "Ultraleggero (80 MB). Ideale per test su PC con pochissima RAM." },
        { "Qwen2.5-3B (Q4_K_M)", "qwen2.5-3b-instruct-q4_k_m.gguf",
          "https://huggingface.co/Qwen/Qwen2.5-3B-Instruct-GGUF/resolve/main/qwen2.5-3b-instruct-q4_k_m.gguf",
          "~1.9 GB", "Generale", "#0e4d92",
          "Buon equilibrio velocit\xc3\xa0/qualit\xc3\xa0. Ideale per laptop con 8 GB RAM." },
        { "Qwen2.5-7B (Q4_K_M)", "qwen2.5-7b-instruct-q4_k_m.gguf",
          "https://huggingface.co/Qwen/Qwen2.5-7B-Instruct-GGUF/resolve/main/qwen2.5-7b-instruct-q4_k_m.gguf",
          "~4.7 GB", "Generale", "#0e4d92",
          "Ottimo per uso quotidiano, ottimo italiano. Richiede 8+ GB RAM." },
        { "Mistral-7B (Q4_K_M)", "mistral-7b-instruct-v0.2.Q4_K_M.gguf",
          "https://huggingface.co/TheBloke/Mistral-7B-Instruct-v0.2-GGUF/resolve/main/mistral-7b-instruct-v0.2.Q4_K_M.gguf",
          "~4.1 GB", "Generale", "#0e4d92",
          "Veloce e bilanciato, ottimo per testi lunghi." },
        /* ── Coding ── */
        { "Phi-3-mini (Q4_K_M)", "Phi-3-mini-4k-instruct-q4.gguf",
          "https://huggingface.co/microsoft/Phi-3-mini-4k-instruct-gguf/resolve/main/Phi-3-mini-4k-instruct-q4.gguf",
          "~2.2 GB", "Coding", "#166534",
          "Microsoft. Compatto e capace su codice e ragionamento." },
        { "Llama-3.2-3B (Q4_K_M)", "Llama-3.2-3B-Instruct-Q4_K_M.gguf",
          "https://huggingface.co/bartowski/Llama-3.2-3B-Instruct-GGUF/resolve/main/Llama-3.2-3B-Instruct-Q4_K_M.gguf",
          "~2.0 GB", "Coding", "#166534",
          "Meta. Leggero, buono per completamento codice e Q&A." },
        /* ── Matematica ── */
        { "DeepSeek-R1-1.5B (Q4_K_M)", "DeepSeek-R1-Distill-Qwen-1.5B-Q4_K_M.gguf",
          "https://huggingface.co/bartowski/DeepSeek-R1-Distill-Qwen-1.5B-GGUF/resolve/main/DeepSeek-R1-Distill-Qwen-1.5B-Q4_K_M.gguf",
          "~1.1 GB", "Matematica", "#92400e",
          "Piccolo e veloce. Ragionamento base con chain-of-thought." },
        { "Qwen2.5-Math-7B (Q4_K_M)", "qwen2.5-math-7b-instruct-q4_k_m.gguf",
          "https://huggingface.co/Qwen/Qwen2.5-Math-7B-Instruct-GGUF/resolve/main/qwen2.5-math-7b-instruct-q4_k_m.gguf",
          "~4.7 GB", "Matematica", "#92400e",
          "Specializzato su matematica scolastica e universitaria." },
        { "DeepSeek-R1-7B (Q4_K_M)", "DeepSeek-R1-Distill-Qwen-7B-Q4_K_M.gguf",
          "https://huggingface.co/bartowski/DeepSeek-R1-Distill-Qwen-7B-GGUF/resolve/main/DeepSeek-R1-Distill-Qwen-7B-Q4_K_M.gguf",
          "~4.7 GB", "Matematica", "#92400e",
          "Ragionamento avanzato, derivato DeepSeek-R1. Eccellente su prove formali." },
        { "phi-4 (Q4_K_M)", "phi-4-Q4_K_M.gguf",
          "https://huggingface.co/bartowski/phi-4-GGUF/resolve/main/phi-4-Q4_K_M.gguf",
          "~8.4 GB", "Matematica", "#92400e",
          "Microsoft phi-4 — eccellente su matematica e logica formale." },
        /* ── Vision ── */
        { "LLaVA-1.5-7B (Q4_K_M)", "llava-1.5-7b-q4_k_m.gguf",
          "https://huggingface.co/mys/ggml_llava-v1.5-7b/resolve/main/ggml-model-q4_k.gguf",
          "~4.1 GB", "Vision", "#581c87",
          "Analisi immagini e grafici. Compatibile con llama-server con mmproj." },
        { "BakLLaVA-1 (Q4_K_M)", "bakllava-1-Q4_K_M.gguf",
          "https://huggingface.co/mys/ggml_bakllava-1/resolve/main/ggml-model-q4_k.gguf",
          "~4.2 GB", "Vision", "#581c87",
          "Vision multimodale basato su Mistral. Buono su descrizione immagini." },
        /* ── Ragionamento ── */
        { "QwQ-32B (Q4_K_M)", "QwQ-32B-Q4_K_M.gguf",
          "https://huggingface.co/bartowski/QwQ-32B-GGUF/resolve/main/QwQ-32B-Q4_K_M.gguf",
          "~19.9 GB", "Ragionamento", "#7c2d12",
          "Ragionamento profondo di alta qualit\xc3\xa0. Richiede 24+ GB RAM." },
        /* ── Xeon / 64 GB ── */
        { "DeepSeek-R1-32B (Q5_K_M) \xe2\x98\x85", "DeepSeek-R1-Distill-Qwen-32B-Q5_K_M.gguf",
          "https://huggingface.co/bartowski/DeepSeek-R1-Distill-Qwen-32B-GGUF/resolve/main/DeepSeek-R1-Distill-Qwen-32B-Q5_K_M.gguf",
          "~22 GB", "Xeon / 64 GB", "#1e3a5f",
          "Ragionamento top in Q5 \xe2\x80\x94 qualit\xc3\xa0 superiore al Q4. Su Xeon 64 GB carica tutto in RAM senza toccare la VRAM. Chain-of-thought nativo." },
        { "Qwen2.5-Coder-32B (Q5_K_M) \xe2\x98\x85", "qwen2.5-coder-32b-instruct-q5_k_m.gguf",
          "https://huggingface.co/Qwen/Qwen2.5-Coder-32B-Instruct-GGUF/resolve/main/qwen2.5-coder-32b-instruct-q5_k_m.gguf",
          "~22 GB", "Xeon / 64 GB", "#1e3a5f",
          "Top coding locale in Q5. Su Xeon con AVX-512 raggiunge ~4-6 token/s. Pipeline a 6 agenti con questo modello produce codice di qualit\xc3\xa0 vicina a GPT-4." },
        { "Mixtral-8x7B (Q4_K_M) \xe2\x98\x85", "mixtral-8x7b-instruct-v0.1.Q4_K_M.gguf",
          "https://huggingface.co/TheBloke/Mixtral-8x7B-Instruct-v0.1-GGUF/resolve/main/mixtral-8x7b-instruct-v0.1.Q4_K_M.gguf",
          "~26 GB", "Xeon / 64 GB", "#1e3a5f",
          "Mixture of Experts 46B \xe2\x80\x94 veloce nonostante la taglia. Ottimo italiano, coding e analisi. Xeon multi-core gestisce bene i pesi distribuiti." },
        { "Llama-3.1-70B (Q4_K_M)", "Meta-Llama-3.1-70B-Instruct-Q4_K_M.gguf",
          "https://huggingface.co/bartowski/Meta-Llama-3.1-70B-Instruct-GGUF/resolve/main/Meta-Llama-3.1-70B-Instruct-Q4_K_M.gguf",
          "~40 GB", "Xeon / 64 GB", "#1e3a5f",
          "Meta top-of-line locale. 40 GB in RAM \xe2\x80\x94 su Xeon 64 GB con -ngl 0 (CPU puro). Velocit\xc3\xa0 ~1-2 token/s, qualit\xc3\xa0 superiore. Usa contesto lungo (128k)." },
        { "Qwen2.5-72B (Q4_K_M)", "qwen2.5-72b-instruct-q4_k_m.gguf",
          "https://huggingface.co/Qwen/Qwen2.5-72B-Instruct-GGUF/resolve/main/qwen2.5-72b-instruct-q4_k_m.gguf",
          "~44 GB", "Xeon / 64 GB", "#1e3a5f",
          "Massima qualit\xc3\xa0 generale: italiano, analisi, codice. 44 GB Q4 \xe2\x80\x94 entra in 64 GB RAM con margine per il sistema operativo." },
    };

    const int NO = (int)(sizeof(OLLAMA) / sizeof(OLLAMA[0]));
    const int NG = (int)(sizeof(GGUF)   / sizeof(GGUF[0]));

    /* ══════════════════════════════════════════════════════════════
       Layout compatto 2 colonne — nessun scroll necessario:
         Sinistra : filtro gestore + categoria
         Destra   : lista compatta + pannello dettaglio
       ══════════════════════════════════════════════════════════════ */
    auto* page    = new QWidget;
    auto* mainLay = new QVBoxLayout(page);
    mainLay->setContentsMargins(16, 14, 16, 14);
    mainLay->setSpacing(10);

    /* ── Titolo ── */
    auto* titleLbl = new QLabel(
        "\xf0\x9f\xa4\x96  LLM Consigliati  \xe2\x80\x94  "
        "<span style='color:#94a3b8;font-size:12px;font-weight:normal;'>"
        "Seleziona gestore e categoria, poi clicca per installare.</span>", page);
    titleLbl->setObjectName("sectionTitle");
    titleLbl->setTextFormat(Qt::RichText);
    mainLay->addWidget(titleLbl);

    /* ── 2 colonne ── */
    auto* colsRow = new QWidget(page);
    auto* colsLay = new QHBoxLayout(colsRow);
    colsLay->setContentsMargins(0, 0, 0, 0);
    colsLay->setSpacing(14);

    /* ════════════════════════════════════════════════════════
       Colonna sinistra — Filtro gestore + categoria
       ════════════════════════════════════════════════════════ */
    auto* leftGroup = new QGroupBox("\xf0\x9f\x94\xa7  Filtro", colsRow);
    leftGroup->setObjectName("cardGroup");
    leftGroup->setFixedWidth(190);
    auto* leftLay = new QVBoxLayout(leftGroup);
    leftLay->setSpacing(5);

    auto* lblGest = new QLabel("Gestore:", leftGroup);
    lblGest->setObjectName("cardDesc");
    auto* btnOllama = new QRadioButton("\xf0\x9f\x90\xb3  Ollama", leftGroup);
    btnOllama->setChecked(true);
    auto* btnGguf   = new QRadioButton("\xf0\x9f\xa6\x99  GGUF (llama.cpp)", leftGroup);

    auto* sepFilt = new QFrame(leftGroup);
    sepFilt->setFrameShape(QFrame::HLine);
    sepFilt->setObjectName("sidebarSep");

    auto* lblCat = new QLabel("Categoria:", leftGroup);
    lblCat->setObjectName("cardDesc");

    leftLay->addWidget(lblGest);
    leftLay->addWidget(btnOllama);
    leftLay->addWidget(btnGguf);
    leftLay->addWidget(sepFilt);
    leftLay->addWidget(lblCat);

    static const char* CATS[] = {
        "Tutti", "Generale", "Coding",
        "Matematica", "Vision", "Ragionamento",
        "\xf0\x9f\x96\xa5  Xeon / 64 GB"   /* categoria hardware dedicata — etichetta UI */
    };
    /* Chiavi di filtro: devono corrispondere esattamente a m.category */
    static const char* CATS_KEY[] = {
        "", "Generale", "Coding",
        "Matematica", "Vision", "Ragionamento",
        "Xeon / 64 GB"
    };
    static const int N_CATS = 7;
    auto* catBtnGroup = new QButtonGroup(leftGroup);
    for (int i = 0; i < N_CATS; i++) {
        auto* rb = new QRadioButton(CATS[i], leftGroup);
        catBtnGroup->addButton(rb, i);
        if (i == 0) rb->setChecked(true);
        /* Evidenzia la categoria Xeon */
        if (i == N_CATS - 1) {
            rb->setStyleSheet("color:#60a5fa;font-weight:bold;");
            /* Separatore prima della voce Xeon */
            auto* sepX = new QFrame(leftGroup);
            sepX->setFrameShape(QFrame::HLine);
            sepX->setObjectName("sidebarSep");
            leftLay->addWidget(sepX);
        }
        leftLay->addWidget(rb);
    }
    leftLay->addStretch(1);
    colsLay->addWidget(leftGroup);

    /* ════════════════════════════════════════════════════════
       Colonna destra — lista + pannello dettaglio
       ════════════════════════════════════════════════════════ */
    auto* rightGroup = new QGroupBox("\xf0\x9f\x93\xa6  Modelli disponibili", colsRow);
    rightGroup->setObjectName("cardGroup");
    auto* rightLay = new QVBoxLayout(rightGroup);
    rightLay->setSpacing(8);

    auto* modelList = new QListWidget(rightGroup);
    modelList->setObjectName("modelsList");
    modelList->setAlternatingRowColors(true);
    rightLay->addWidget(modelList, 1);

    /* ── Pannello dettaglio (sotto la lista) ── */
    auto* detSep = new QFrame(rightGroup);
    detSep->setFrameShape(QFrame::HLine);
    detSep->setObjectName("sidebarSep");
    rightLay->addWidget(detSep);

    auto* detRow = new QWidget(rightGroup);
    auto* detLay = new QHBoxLayout(detRow);
    detLay->setContentsMargins(0, 4, 0, 0);
    detLay->setSpacing(10);

    auto* detailLbl = new QLabel("Seleziona un modello per i dettagli.", rightGroup);
    detailLbl->setObjectName("cardDesc");
    detailLbl->setWordWrap(true);
    detailLbl->setTextFormat(Qt::RichText);
    detailLbl->setMinimumHeight(44);

    auto* installBtn = new QPushButton("\xe2\xac\x87  Installa", rightGroup);
    installBtn->setObjectName("actionBtn");
    installBtn->setFixedWidth(110);
    installBtn->setEnabled(false);

    detLay->addWidget(detailLbl, 1);
    detLay->addWidget(installBtn);
    rightLay->addWidget(detRow);

    /* ── Log download ── */
    auto* logOut = new QLabel(rightGroup);
    logOut->setWordWrap(true);
    logOut->setVisible(false);
    logOut->setStyleSheet(
        "background:#0f172a;color:#86efac;font-family:monospace;"
        "font-size:11px;padding:6px 10px;border-radius:6px;");
    rightLay->addWidget(logOut);

    /* ── Download GGUF da URL personalizzato ── */
    auto* customSep = new QFrame(rightGroup);
    customSep->setFrameShape(QFrame::HLine);
    customSep->setObjectName("sidebarSep");
    rightLay->addWidget(customSep);

    auto* customLbl = new QLabel(
        "\xf0\x9f\x94\x97  Scarica GGUF da URL personalizzato (HuggingFace):", rightGroup);
    customLbl->setObjectName("hintLabel");
    rightLay->addWidget(customLbl);

    auto* customRow = new QWidget(rightGroup);
    auto* customRowLay = new QHBoxLayout(customRow);
    customRowLay->setContentsMargins(0, 0, 0, 0);
    customRowLay->setSpacing(6);

    auto* customEdit = new QLineEdit(rightGroup);
    customEdit->setObjectName("chatInput");
    customEdit->setPlaceholderText(
        "https://huggingface.co/.../resolve/main/modello.gguf");
    auto* customDlBtn = new QPushButton("\xe2\xac\x87  Scarica", rightGroup);
    customDlBtn->setObjectName("actionBtn");
    customDlBtn->setFixedWidth(100);
    customRowLay->addWidget(customEdit, 1);
    customRowLay->addWidget(customDlBtn);
    rightLay->addWidget(customRow);

    connect(customDlBtn, &QPushButton::clicked, page, [=]() {
        const QString url = customEdit->text().trimmed();
        if (url.isEmpty()) return;
        const QString filename = QUrl(url).fileName();
        if (filename.isEmpty() || !filename.endsWith(".gguf", Qt::CaseInsensitive)) {
            logOut->setText("\xe2\x9a\xa0  L'URL deve puntare a un file .gguf");
            logOut->setVisible(true);
            return;
        }
        const QString dest = PrismaluxPaths::modelsDir() + "/" + filename;
        logOut->setText(QString("\xf0\x9f\x93\xa5  Scarico %1...").arg(filename));
        logOut->setVisible(true);
        customDlBtn->setEnabled(false);

        auto* proc = new QProcess(page);
        proc->setProcessChannelMode(QProcess::MergedChannels);
        connect(proc, &QProcess::readyRead, page, [proc, logOut]() {
            const QString s = QString::fromUtf8(proc->readAll()).trimmed();
            if (!s.isEmpty()) logOut->setText(s);
        });
        connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                page, [proc, logOut, customDlBtn, filename](int code, QProcess::ExitStatus) {
            if (code == 0)
                logOut->setText(QString("\xe2\x9c\x85  %1 scaricato.").arg(filename));
            else
                logOut->setText("\xe2\x9d\x8c  Download fallito. Controlla URL e connessione.");
            customDlBtn->setEnabled(true);
            proc->deleteLater();
        });

        /* wget preferito, poi curl come fallback */
        if (!QStandardPaths::findExecutable("wget").isEmpty())
            proc->start("wget", {"-c", "--show-progress", "-O", dest, url});
        else
            proc->start("curl", {"-L", "-C", "-", "--progress-bar", "-o", dest, url});
    });

    colsLay->addWidget(rightGroup, 1);
    mainLay->addWidget(colsRow, 1);

    /* ── Pannello consigli Xeon / 64 GB (visibile solo quando selezionata quella categoria) ── */
    /* ── Pannello avviso Vision (visibile solo su categoria Vision) ── */
    auto* visionPanel = new QWidget(page);
    visionPanel->setVisible(false);
    auto* visionPanelLay = new QVBoxLayout(visionPanel);
    visionPanelLay->setContentsMargins(0, 4, 0, 0);
    visionPanelLay->setSpacing(0);
    auto* visionWarnLbl = new QLabel(visionPanel);
    visionWarnLbl->setTextFormat(Qt::RichText);
    visionWarnLbl->setWordWrap(true);
    visionWarnLbl->setStyleSheet(
        "background:#1a0a00;border:1px solid #92400e;border-radius:8px;"
        "padding:10px 14px;color:#e2e8f0;font-size:12px;");
    visionWarnLbl->setText(
        "<b style='color:#fb923c;'>\xe2\x9a\xa0  Modelli Vision — cosa funziona e cosa no</b><br><br>"
        "<b style='color:#fcd34d;'>Compatibili con Ollama:</b> "
        "llama3.2-vision \xe2\x98\x85 \xe2\x80\x94 qwen2-vl:7b \xe2\x98\x85 \xe2\x80\x94 minicpm-v:8b \xe2\x80\x94 llava:7b \xe2\x80\x94 moondream2<br>"
        "<b style='color:#f87171;'>NON compatibili (errori certi):</b> "
        "deepseek-janus \xe2\x80\x94 deepseek-vl \xe2\x80\x94 deepseek-r1 \xe2\x80\x94 deepseek-coder "
        "\xe2\x80\x94 qualsiasi modello DeepSeek<br><br>"
        "<span style='color:#94a3b8;font-size:11px;'>"
        "I modelli DeepSeek non supportano il campo \"images\" dell'API Ollama. "
        "Se li usi con immagini riceverai l'errore: "
        "<i>\"model does not support vision\"</i>. "
        "Prismalux ora mostra questo errore in modo leggibile invece di bloccarsi."
        "</span>");
    visionPanelLay->addWidget(visionWarnLbl);
    mainLay->addWidget(visionPanel);

    /* ── Pannello consigli Xeon / 64 GB ── */
    auto* xeonPanel = new QWidget(page);
    xeonPanel->setVisible(false);
    auto* xeonLay = new QVBoxLayout(xeonPanel);
    xeonLay->setContentsMargins(0, 4, 0, 0);
    xeonLay->setSpacing(0);

    auto* xeonLbl = new QLabel(xeonPanel);
    xeonLbl->setTextFormat(Qt::RichText);
    xeonLbl->setWordWrap(true);
    xeonLbl->setOpenExternalLinks(false);
    xeonLbl->setStyleSheet(
        "background:#0c1e3a;border:1px solid #1e3a5f;border-radius:8px;"
        "padding:12px 16px;color:#e2e8f0;font-size:12px;line-height:160%;");
    xeonLbl->setText(
        "<b style='color:#60a5fa;font-size:13px;'>"
        "\xf0\x9f\x96\xa5  Configurazione consigliata: Xeon + 64 GB RAM</b><br><br>"

        "<b style='color:#93c5fd;'>Strategia ottimale per la tua macchina:</b><br>"
        "\xe2\x80\xa2  Usa <b>CPU pura</b> (ignora la GPU AMD lenta): "
        "<code style='background:#0f172a;padding:1px 5px;border-radius:3px;color:#4ade80;'>"
        "set OLLAMA_NUM_GPU=0 &amp;&amp; ollama serve</code><br>"
        "\xe2\x80\xa2  Con 64 GB RAM puoi caricare modelli da <b>20-44 GB</b> senza toccare la VRAM<br>"
        "\xe2\x80\xa2  Il Xeon ha spesso <b>AVX-512</b>: verifica con "
        "<code style='background:#0f172a;padding:1px 5px;border-radius:3px;color:#4ade80;'>"
        "wmic cpu get name</code> e cerca E5/E7/Scalable/Gold/Silver<br><br>"

        "<b style='color:#93c5fd;'>Modelli raccomandati (in ordine di priorit\xc3\xa0):</b><br>"
        "\xe2\x98\x85  <b>Qwen2.5-Coder-32B Q5</b> (~22 GB) \xe2\x80\x94 coding top, ~4-6 token/s su Xeon<br>"
        "\xe2\x98\x85  <b>DeepSeek-R1-32B Q5</b> (~22 GB) \xe2\x80\x94 ragionamento/matematica avanzata<br>"
        "\xe2\x98\x85  <b>Mixtral 8x7B Q4</b> (~26 GB) \xe2\x80\x94 generico veloce (MoE attiva 2/8 esperti)<br>"
        "       <b>Llama-3.1-70B Q4</b> (~40 GB) \xe2\x80\x94 massima qualit\xc3\xa0, ~1-2 token/s<br><br>"

        "<b style='color:#93c5fd;'>Pipeline multi-agente consigliata su Xeon 64 GB:</b><br>"
        "\xe2\x80\xa2  Agente 1 (Ricercatore): <b>deepseek-r1:14b</b> \xe2\x80\x94 veloce per analisi<br>"
        "\xe2\x80\xa2  Agenti 2-3 (Programmatori): <b>deepseek-coder-v2:16b</b> \xe2\x80\x94 potente e leggero<br>"
        "\xe2\x80\xa2  Agente 5 (Tester): <b>qwen2.5-coder:32b</b> \xe2\x80\x94 massima precisione sul codice<br>"
        "\xe2\x80\xa2  Agente 6 (Ottimizzatore): <b>qwen3:30b</b> \xe2\x80\x94 qualit\xc3\xa0 di ragionamento superiore<br><br>"

        "<span style='color:#64748b;font-size:11px;'>"
        "Nota: modelli con \xe2\x98\x85 sono testati e ottimizzati per CPU Xeon multi-core."
        "</span>");
    xeonLay->addWidget(xeonLbl);
    mainLay->addWidget(xeonPanel);

    /* ════════════════════════════════════════════════════════
       Logica — popola lista in base ai filtri selezionati
       ════════════════════════════════════════════════════════ */
    auto populate = [=]() {
        modelList->clear();
        installBtn->setEnabled(false);
        detailLbl->setText("Seleziona un modello per i dettagli.");
        logOut->setVisible(false);

        const bool isOllama  = btnOllama->isChecked();
        const int  catId     = catBtnGroup->checkedId();
        /* Usa CATS_KEY (senza emoji) per la comparazione con m.category */
        const QString filter = (catId == 0) ? QString() : QString(CATS_KEY[catId]);

        /* Mostra/nascondi pannelli contestuali */
        xeonPanel->setVisible(catId == N_CATS - 1);
        /* Categoria Vision (indice 4 = "Vision" in CATS_KEY) */
        const int kVisionCat = 4;
        visionPanel->setVisible(catId == kVisionCat);

        if (isOllama) {
            for (int i = 0; i < NO; i++) {
                const auto& m = OLLAMA[i];
                if (!filter.isEmpty() && filter != m.category) continue;
                auto* item = new QListWidgetItem(
                    QString("[%1]  %2  \xe2\x80\x94  %3")
                    .arg(m.category, m.display, m.size));
                item->setData(Qt::UserRole,     i);
                item->setData(Qt::UserRole + 1, true);
                modelList->addItem(item);
            }
        } else {
            for (int i = 0; i < NG; i++) {
                const auto& m = GGUF[i];
                if (!filter.isEmpty() && filter != m.category) continue;
                const bool inst = QFile::exists(
                    PrismaluxPaths::modelsDir() + "/" + m.filename);
                auto* item = new QListWidgetItem(
                    QString("[%1]  %2  \xe2\x80\x94  %3%4")
                    .arg(m.category, m.display, m.size,
                         inst ? "  \xe2\x9c\x94" : ""));
                item->setData(Qt::UserRole,     i);
                item->setData(Qt::UserRole + 1, false);
                modelList->addItem(item);
            }
        }
    };

    /* Selezione modello → aggiorna pannello dettaglio */
    connect(modelList, &QListWidget::currentItemChanged, page,
            [=](QListWidgetItem* cur, QListWidgetItem*) {
        if (!cur) {
            installBtn->setEnabled(false);
            detailLbl->setText("Seleziona un modello per i dettagli.");
            return;
        }
        const int  idx   = cur->data(Qt::UserRole).toInt();
        const bool isOll = cur->data(Qt::UserRole + 1).toBool();
        installBtn->setEnabled(true);
        installBtn->setText("\xe2\xac\x87  Installa");
        installBtn->setStyleSheet("");

        if (isOll) {
            const auto& m = OLLAMA[idx];
            detailLbl->setText(
                QString("<b>%1</b>  <span style='color:#64748b;'>%2</span><br>"
                        "<span style='color:#94a3b8;font-size:11px;'>%3</span>")
                .arg(m.display, m.size, m.desc));
        } else {
            const auto& m = GGUF[idx];
            const bool inst = QFile::exists(
                PrismaluxPaths::modelsDir() + "/" + m.filename);
            detailLbl->setText(
                QString("<b>%1</b>  <span style='color:#64748b;'>%2</span>"
                        "%4<br>"
                        "<span style='color:#94a3b8;font-size:11px;'>%3</span>")
                .arg(m.display, m.size, m.desc,
                     inst ? "  <span style='color:#4ade80;'>\xe2\x9c\x94 presente</span>"
                          : ""));
        }
    });

    /* Pulsante Installa */
    connect(installBtn, &QPushButton::clicked, page, [=]() {
        auto* cur = modelList->currentItem();
        if (!cur) return;
        const int  idx   = cur->data(Qt::UserRole).toInt();
        const bool isOll = cur->data(Qt::UserRole + 1).toBool();

        installBtn->setEnabled(false);
        installBtn->setText("\xe2\x8f\xb3  ...");
        logOut->setVisible(true);

        if (isOll) {
            const auto& m = OLLAMA[idx];
            const QString ollamaName(m.ollama);
            logOut->setText(QString("\xf0\x9f\x93\xa5  ollama pull %1").arg(ollamaName));
            auto* proc = new QProcess(page);
            proc->setProcessChannelMode(QProcess::MergedChannels);
            connect(proc, &QProcess::readyRead, page, [proc, logOut]() {
                const QString txt = QString::fromUtf8(proc->readAll()).trimmed();
                if (!txt.isEmpty()) logOut->setText(txt);
            });
            connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                    page, [proc, installBtn, logOut, ollamaName](int code, QProcess::ExitStatus) {
                if (code == 0) {
                    installBtn->setText("\xe2\x9c\x94  Installato");
                    installBtn->setStyleSheet("color:#4ade80;");
                    logOut->setText(QString("\xe2\x9c\x85  %1 installato.").arg(ollamaName));
                } else {
                    installBtn->setEnabled(true);
                    installBtn->setText("\xe2\xac\x87  Installa");
                    logOut->setText("\xe2\x9d\x8c  Errore. Assicurati che ollama sia in esecuzione.");
                }
                proc->deleteLater();
            });
            proc->start("ollama", {"pull", ollamaName});
        } else {
            const auto& m = GGUF[idx];
            const QString ggufUrl(m.url);
            const QString ggufFile(m.filename);
            const QString dest = PrismaluxPaths::modelsDir() + "/" + ggufFile;
            logOut->setText(QString("\xf0\x9f\x93\xa5  Scarico %1...").arg(ggufFile));
            auto* proc = new QProcess(page);
            proc->setProcessChannelMode(QProcess::MergedChannels);
            connect(proc, &QProcess::readyRead, page, [proc, logOut]() {
                const QString txt = QString::fromUtf8(proc->readAll()).trimmed();
                if (!txt.isEmpty()) logOut->setText(txt);
            });
            connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                    page, [proc, installBtn, logOut, ggufFile](int code, QProcess::ExitStatus) {
                if (code == 0) {
                    installBtn->setText("\xe2\x9c\x94  Scaricato");
                    installBtn->setStyleSheet("color:#4ade80;");
                    logOut->setText(QString("\xe2\x9c\x85  %1 scaricato.").arg(ggufFile));
                } else {
                    installBtn->setEnabled(true);
                    installBtn->setText("\xe2\xac\x87  Installa");
                    logOut->setText("\xe2\x9d\x8c  Errore download. Controlla wget/connessione.");
                }
                proc->deleteLater();
            });
            proc->start("wget", {"-c", "--show-progress", "-O", dest, ggufUrl});
            if (!proc->waitForStarted(3000)) {
                proc->deleteLater();
                auto* curl = new QProcess(page);
                curl->setProcessChannelMode(QProcess::MergedChannels);
                connect(curl, &QProcess::readyRead, page, [curl, logOut]() {
                    const QString txt = QString::fromUtf8(curl->readAll()).trimmed();
                    if (!txt.isEmpty()) logOut->setText(txt);
                });
                connect(curl, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                        page, [curl, installBtn, logOut, ggufFile](int code, QProcess::ExitStatus) {
                    if (code == 0) {
                        installBtn->setText("\xe2\x9c\x94  Scaricato");
                        installBtn->setStyleSheet("color:#4ade80;");
                        logOut->setText(QString("\xe2\x9c\x85  %1 scaricato.").arg(ggufFile));
                    } else {
                        installBtn->setEnabled(true);
                        installBtn->setText("\xe2\xac\x87  Installa");
                        logOut->setText("\xe2\x9d\x8c  Errore: installa wget o curl.");
                    }
                    curl->deleteLater();
                });
                curl->start("curl", {"-L", "-C", "-", "--progress-bar",
                                     "-o", dest, ggufUrl});
            }
        }
    });

    /* Cambi filtro → ripopola */
    connect(btnOllama, &QRadioButton::toggled, page, [=](bool on){ if (on) populate(); });
    connect(btnGguf,   &QRadioButton::toggled, page, [=](bool on){ if (on) populate(); });
    for (QAbstractButton* b : catBtnGroup->buttons())
        connect(b, &QAbstractButton::toggled, page, [=](bool on){ if (on) populate(); });

    /* Popola al primo caricamento */
    QTimer::singleShot(0, page, populate);

    return page;
}

/* ══════════════════════════════════════════════════════════════
   buildAiParamsTab — parametri di campionamento anti-allucinazione.
   I valori sono salvati in QSettings("Prismalux","GUI") con prefisso "ai/".
   AiClient li carica al costruttore e li usa in ogni richiesta.
   ══════════════════════════════════════════════════════════════ */
QWidget* ImpostazioniPage::buildAiParamsTab()
{
    auto* page  = new QWidget;
    auto* outer = new QVBoxLayout(page);
    outer->setContentsMargins(20, 20, 20, 20);
    outer->setSpacing(16);

    /* Titolo */
    auto* title = new QLabel("\xe2\x9a\x99\xef\xb8\x8f  Parametri AI \xe2\x80\x94 Anti-allucinazione e precisione");
    title->setObjectName("sectionTitle");
    outer->addWidget(title);

    auto* desc = new QLabel(
        "Modalit\xc3\xa0 <b>Brutal Honesty</b>: il modello ammette l'incertezza invece di inventare. "
        "Temperatura vicina a 0 = risposte deterministiche e ripetibili. "
        "Il prefisso di onest\xc3\xa0 istruisce il modello a dire "
        "\xe2\x80\x9cNon lo so\xe2\x80\x9d invece di inventare fatti, numeri o citazioni.");
    desc->setWordWrap(true);
    desc->setObjectName("hintLabel");
    outer->addWidget(desc);

    auto* fl = new QFormLayout;
    fl->setContentsMargins(0, 8, 0, 0);
    fl->setSpacing(12);
    fl->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    /* Legge dall'unica fonte: ~/.prismalux/ai_params.json */
    const AiChatParams cur = AiChatParams::load();

    /* Mostra percorso file sotto il titolo */
    auto* fileLbl = new QLabel(
        "\xf0\x9f\x93\x84  Sorgente: <code>" + AiChatParams::filePath() + "</code> "
        "(modificabile anche con un editor di testo)");
    fileLbl->setWordWrap(true);
    fileLbl->setObjectName("hintLabel");
    outer->addWidget(fileLbl);

    auto* sep0b = new QFrame; sep0b->setFrameShape(QFrame::HLine); sep0b->setObjectName("sidebarSep");
    outer->addWidget(sep0b);

    /* ── Temperatura ── */
    auto* tempSpin = new QDoubleSpinBox;
    tempSpin->setRange(0.0, 1.5);
    tempSpin->setSingleStep(0.05);
    tempSpin->setDecimals(2);
    tempSpin->setValue(cur.temperature);
    tempSpin->setToolTip("0 = deterministico puro (risposta identica ogni volta)\n"
                         "0.05 = quasi deterministico — Brutal Honesty (default)\n"
                         "0.3+ = creativo ma meno affidabile per fatti precisi");
    fl->addRow("Temperatura:", tempSpin);

    auto* tempHint = new QLabel(
        "\xe2\x84\xb9  0.0" "\xe2\x80\x93" "0.1 = fatti certi e ripetibili (Brutal Honesty)  |  0.3+ = creativo/inventivo");
    tempHint->setObjectName("hintLabel");
    fl->addRow("", tempHint);

    /* ── Top-P ── */
    auto* topPSpin = new QDoubleSpinBox;
    topPSpin->setRange(0.1, 1.0);
    topPSpin->setSingleStep(0.05);
    topPSpin->setDecimals(2);
    topPSpin->setValue(cur.top_p);
    topPSpin->setToolTip("Nucleus sampling: include solo i token pi\xc3\xb9 probabili.\n"
                         "0.85 = scelte conservative (Brutal Honesty)");
    fl->addRow("Top-P:", topPSpin);

    /* ── Top-K ── */
    auto* topKSpin = new QSpinBox;
    topKSpin->setRange(1, 200);
    topKSpin->setValue(cur.top_k);
    topKSpin->setToolTip("Limita la scelta ai K token pi\xc3\xb9 probabili.\n"
                         "20 = molto conservativo — favorisce i fatti sicuri.");
    fl->addRow("Top-K:", topKSpin);

    /* ── Penalità ripetizioni ── */
    auto* repSpin = new QDoubleSpinBox;
    repSpin->setRange(1.0, 2.0);
    repSpin->setSingleStep(0.05);
    repSpin->setDecimals(2);
    repSpin->setValue(cur.repeat_penalty);
    repSpin->setToolTip("Penalizza i token gi\xc3\xa0 generati per ridurre le ripetizioni.\n"
                        "1.20 = riduce loop e riempitivi.");
    fl->addRow("Penalità ripetizioni:", repSpin);

    /* ── Max token risposta ── */
    auto* predSpin = new QSpinBox;
    predSpin->setRange(256, 16384);
    predSpin->setSingleStep(256);
    predSpin->setValue(cur.num_predict);
    predSpin->setSuffix("  token");
    predSpin->setToolTip("Numero massimo di token generati per risposta.\n"
                         "2048 = risposte lunghe complete  |  512 = risposte brevi e veloci");
    fl->addRow("Max token risposta:", predSpin);

    /* ── Context window ── */
    auto* ctxSpin = new QSpinBox;
    ctxSpin->setRange(1024, 65536);
    ctxSpin->setSingleStep(1024);
    ctxSpin->setValue(cur.num_ctx);
    ctxSpin->setSuffix("  token");
    ctxSpin->setToolTip("Dimensione della finestra di contesto: quanti token la chat tiene in memoria.\n"
                        "8192 = ottimo per sessioni lunghe  |  4096 = pi\xc3\xb9 veloce");
    fl->addRow("Finestra contesto:", ctxSpin);

    outer->addLayout(fl);

    /* ── Prefisso di onestà assoluta ── */
    auto* honestyCb = new QCheckBox(
        "\xf0\x9f\x94\x92  Prefisso Brutal Honesty (consigliato)");
    honestyCb->setChecked(cur.honesty_prefix);
    honestyCb->setToolTip(
        "Aggiunge all'inizio di ogni system prompt la regola:\n"
        "\"Se non conosci qualcosa, dillo. Non inventare mai fatti, numeri o citazioni.\"\n"
        "Questo \xc3\xa8 il metodo pi\xc3\xb9 efficace per ridurre le allucinazioni.");
    outer->addWidget(honestyCb);

    auto* honestyHint = new QLabel(
        "\xe2\x84\xb9  Con questa opzione attiva il modello risponde "
        "\xe2\x80\x9cNon lo so\xe2\x80\x9d invece di inventare. Disattiva solo se vuoi risposte pi\xc3\xb9 fluide.");
    honestyHint->setWordWrap(true);
    honestyHint->setObjectName("hintLabel");
    outer->addWidget(honestyHint);

    auto* sep1 = new QFrame; sep1->setFrameShape(QFrame::HLine); sep1->setObjectName("sidebarSep");
    outer->addWidget(sep1);

    /* ── Bottoni reset / salva ── */
    auto* btnRow  = new QWidget;
    auto* btnLay  = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(0, 0, 0, 0);
    btnLay->setSpacing(8);

    auto* resetBtn = new QPushButton("\xf0\x9f\x94\x84  Ripristina default");
    resetBtn->setObjectName("actionBtn");
    resetBtn->setToolTip("Ripristina i valori ottimali anti-allucinazione");

    auto* saveBtn  = new QPushButton("\xe2\x9c\x85  Salva");
    saveBtn->setObjectName("actionBtn");

    auto* saveStatus = new QLabel("");
    saveStatus->setObjectName("hintLabel");

    btnLay->addWidget(resetBtn);
    btnLay->addStretch();
    btnLay->addWidget(saveStatus);
    btnLay->addWidget(saveBtn);
    outer->addWidget(btnRow);

    /* ── Lambda salva — scrive in ~/.prismalux/ai_params.json (unica fonte) ── */
    auto saveParams = [=]{
        AiChatParams p;
        p.temperature    = tempSpin->value();
        p.top_p          = topPSpin->value();
        p.top_k          = topKSpin->value();
        p.repeat_penalty = repSpin->value();
        p.num_predict    = predSpin->value();
        p.num_ctx        = ctxSpin->value();
        p.honesty_prefix = honestyCb->isChecked();
        AiChatParams::save(p);       /* scrive su disco */
        if (m_ai) m_ai->setChatParams(p);  /* applica subito senza riavviare */
        saveStatus->setText("\xe2\x9c\x85  Salvato in " + AiChatParams::filePath());
        QTimer::singleShot(3000, saveStatus, [saveStatus]{ saveStatus->setText(""); });
    };

    connect(saveBtn,  &QPushButton::clicked, page, saveParams);
    connect(resetBtn, &QPushButton::clicked, page, [=]{
        tempSpin->setValue(0.05);
        topPSpin->setValue(0.85);
        topKSpin->setValue(20);
        repSpin->setValue(1.20);
        predSpin->setValue(2048);
        ctxSpin->setValue(8192);
        honestyCb->setChecked(true);
        saveParams();
    });

    /* Salva automaticamente quando si cambia qualsiasi valore */
    connect(tempSpin,  QOverload<double>::of(&QDoubleSpinBox::valueChanged), page, saveParams);
    connect(topPSpin,  QOverload<double>::of(&QDoubleSpinBox::valueChanged), page, saveParams);
    connect(topKSpin,  QOverload<int>::of(&QSpinBox::valueChanged),          page, saveParams);
    connect(repSpin,   QOverload<double>::of(&QDoubleSpinBox::valueChanged), page, saveParams);
    connect(predSpin,  QOverload<int>::of(&QSpinBox::valueChanged),          page, saveParams);
    connect(ctxSpin,   QOverload<int>::of(&QSpinBox::valueChanged),          page, saveParams);
    connect(honestyCb, &QCheckBox::toggled,                                  page, saveParams);

    outer->addStretch();
    return page;
}

/* ══════════════════════════════════════════════════════════════════════
   buildLlmClassificaTab — ranking oggettivo modelli open-weight
   ═══════════════════════════════════════════════════════════════════════
   Fonte dati: ArtificialAnalysis.ai (intelligence index) + benchmark
   Prismalux locali (test_prompt_levels.py, sessione 2026-04-15).
   Colonne: Rank | Modello | Parametri | RAM (Q4) | Score | Velocità | 64GB | Categoria
   ══════════════════════════════════════════════════════════════════════ */
QWidget* ImpostazioniPage::buildLlmClassificaTab()
{
    auto* page = new QWidget;
    auto* mainLay = new QVBoxLayout(page);
    mainLay->setContentsMargins(16, 14, 16, 14);
    mainLay->setSpacing(10);

    /* ── Titolo + fonte ── */
    auto* titleLbl = new QLabel(
        "\xf0\x9f\x93\x8a  <b>Classifica LLM Open-Weight</b>  "
        "<span style='color:#94a3b8;font-size:12px;font-weight:normal;'>"
        "Fonte: ArtificialAnalysis.ai \xe2\x80\xa2 Benchmark locali Prismalux (2026-04-15)"
        "</span>", page);
    titleLbl->setObjectName("sectionTitle");
    titleLbl->setTextFormat(Qt::RichText);
    mainLay->addWidget(titleLbl);

    /* ── Filtro RAM ── */
    auto* filterRow = new QWidget(page);
    auto* filterLay = new QHBoxLayout(filterRow);
    filterLay->setContentsMargins(0, 0, 0, 0);
    filterLay->setSpacing(10);

    auto* filterLbl = new QLabel("Filtra per RAM disponibile:", filterRow);
    filterLbl->setObjectName("cardDesc");
    auto* filterCombo = new QComboBox(filterRow);
    filterCombo->addItem("Tutti i modelli",   0);
    filterCombo->addItem("\xe2\x89\xa4 8 GB RAM",     8);
    filterCombo->addItem("\xe2\x89\xa4 16 GB RAM",   16);
    filterCombo->addItem("\xe2\x89\xa4 32 GB RAM",   32);
    filterCombo->addItem("\xe2\x89\xa4 64 GB RAM",   64);
    filterCombo->setCurrentIndex(4);   /* default: mostra tutto fino a 64 GB */
    filterCombo->setFixedWidth(160);

    auto* filterCatCombo = new QComboBox(filterRow);
    filterCatCombo->addItem("Tutte le categorie", "");
    filterCatCombo->addItem("Generale",           "Generale");
    filterCatCombo->addItem("Coding",             "Coding");
    filterCatCombo->addItem("Ragionamento",        "Ragionamento");
    filterCatCombo->addItem("Matematica",          "Matematica");
    filterCatCombo->addItem("Vision",              "Vision");
    filterCatCombo->setFixedWidth(180);

    auto* sortLbl = new QLabel("Ordina per:", filterRow);
    sortLbl->setObjectName("cardDesc");
    auto* sortCombo = new QComboBox(filterRow);
    sortCombo->addItem("Score qualit\xc3\xa0 \xe2\x86\x93", 0);
    sortCombo->addItem("RAM richiesta \xe2\x86\x91",         1);
    sortCombo->addItem("Velocit\xc3\xa0 \xe2\x86\x93",       2);
    sortCombo->setFixedWidth(170);

    filterLay->addWidget(filterLbl);
    filterLay->addWidget(filterCombo);
    filterLay->addSpacing(8);
    filterLay->addWidget(filterCatCombo);
    filterLay->addSpacing(12);
    filterLay->addWidget(sortLbl);
    filterLay->addWidget(sortCombo);
    filterLay->addStretch();
    mainLay->addWidget(filterRow);

    /* ── Tabella ── */
    auto* table = new QTableWidget(page);
    table->setObjectName("modelsList");
    table->setColumnCount(8);
    table->setHorizontalHeaderLabels({
        "#", "Modello", "Param.", "RAM Q4",
        "Score", "Velocit\xc3\xa0", "\xe2\x89\xa464GB", "Categoria"
    });
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(7, QHeaderView::ResizeToContents);
    table->setColumnWidth(0, 38);
    table->setColumnWidth(2, 62);
    table->setColumnWidth(3, 72);
    table->setColumnWidth(4, 65);
    table->setColumnWidth(5, 80);
    table->setColumnWidth(6, 55);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(true);
    table->setSortingEnabled(false);   /* sorting manuale via combo */
    table->verticalHeader()->setVisible(false);
    mainLay->addWidget(table, 1);

    /* ── Pannello dettaglio + installazione ── */
    auto* detailGroup = new QGroupBox("\xf0\x9f\x94\x8d  Dettaglio modello selezionato", page);
    detailGroup->setObjectName("cardGroup");
    auto* detailLay = new QHBoxLayout(detailGroup);
    detailLay->setContentsMargins(10, 8, 10, 8);
    detailLay->setSpacing(10);

    auto* detailLbl = new QLabel(
        "<span style='color:#94a3b8;'>Seleziona una riga per i dettagli e l'installazione rapida.</span>",
        detailGroup);
    detailLbl->setTextFormat(Qt::RichText);
    detailLbl->setWordWrap(true);

    auto* installBtn = new QPushButton("\xe2\xac\x87  Installa (Ollama)", detailGroup);
    installBtn->setObjectName("actionBtn");
    installBtn->setFixedWidth(160);
    installBtn->setEnabled(false);

    detailLay->addWidget(detailLbl, 1);
    detailLay->addWidget(installBtn);
    mainLay->addWidget(detailGroup);

    /* ── Log installazione ── */
    auto* logLbl = new QLabel(page);
    logLbl->setWordWrap(true);
    logLbl->setVisible(false);
    logLbl->setStyleSheet(
        "background:#0f172a;color:#86efac;font-family:monospace;"
        "font-size:11px;padding:6px 10px;border-radius:6px;");
    mainLay->addWidget(logLbl);

    /* ══════════════════════════════════════════════════════════
       Dataset modelli — classifica completa open-weight ≤ 64GB
       Campi: display, ollama_id, params_b (float), ram_gb (Q4),
              score (0-100), speed_tps (token/s su CPU media),
              category, notes
       ══════════════════════════════════════════════════════════ */
    struct RankEntry {
        const char* display;
        const char* ollama;        /* vuoto = non disponibile su Ollama */
        float params_b;
        float ram_gb;
        int   score;               /* intelligence index normalizzato 0-100 */
        int   speed_tps;           /* token/s stimati su CPU 16-core (Q4) */
        const char* category;
        const char* notes;
    };

    static const RankEntry RANK[] = {
        /* ── Top assoluti (entrano in 64 GB) ── */
        { "Qwen2.5 72B",            "qwen2.5:72b",          72.0f, 44.0f,  82,  2,
          "Generale",    "Top qualit\xc3\xa0 locale. Score vicino a GPT-4o su benchmark MMLU/GPQA. Ottimo italiano." },
        { "Llama 3.1 70B",          "llama3.1:70b",         70.0f, 40.0f,  78,  2,
          "Generale",    "Meta. Ragionamento generale eccellente. ~1-2 t/s su CPU 64 GB." },
        { "DeepSeek-R1 70B distill","deepseek-r1:70b",      70.0f, 40.0f,  77,  2,
          "Ragionamento","Chain-of-thought avanzato. Ottimo su matematica e logica. Lento su CPU." },
        { "Qwen3 30B",              "qwen3:30b",            30.0f, 18.0f,  74,  5,
          "Generale",    "Modello consigliato per uso quotidiano su 64 GB. Think nativo. Ottimo italiano." },
        { "DeepSeek-R1 32B distill","deepseek-r1:32b",      32.0f, 20.0f,  72,  4,
          "Ragionamento","Distillazione del modello 671B. Ragionamento matematico top sotto 30 GB." },
        { "Qwen3 32B",              "qwen3:32b",            32.0f, 20.0f,  72,  4,
          "Generale",    "Variante Qwen3 con context pi\xc3\xb9 lungo. Equivalente a Qwen3:30b." },
        { "Qwen2.5-Coder 32B",      "qwen2.5-coder:32b",    32.0f, 20.0f,  70,  4,
          "Coding",      "Miglior modello coding open-weight < 64 GB. HumanEval 90%+." },
        { "Gemma 3 27B",            "gemma3:27b",           27.0f, 16.0f,  68,  5,
          "Generale",    "Google DeepMind. Ottimo su testi lunghi e analisi. Veloce su CPU." },
        { "Mixtral 8x7B (MoE)",     "mixtral:8x7b",         46.0f, 26.0f,  66,  7,
          "Generale",    "Mixture of Experts: attiva 2/8 esperti per token. Veloce nonostante i 46B totali." },
        { "Mistral Small 3.1 24B",  "mistral-small3.1:24b", 24.0f, 14.0f,  63,  7,
          "Generale",    "Ottimo rapporto qualit\xc3\xa0/velocit\xc3\xa0. Contesto 128k. Consigliato per RAG." },
        { "DeepSeek-R1 14B distill","deepseek-r1:14b",      14.0f,  9.0f,  60,  9,
          "Ragionamento","Veloce e capace. Ideale come agente ricercatore nella pipeline multi-agente." },
        { "Phi-4 14B",              "phi4:14b",             14.0f,  9.0f,  58, 10,
          "Generale",    "Microsoft. Eccellente ragionamento per la dimensione. Ottimo per didattica." },
        { "Qwen2.5-Coder 7B",       "qwen2.5-coder:7b",      7.0f,  4.7f,  54, 18,
          "Coding",      "Top della categoria ~7B su HumanEval. Velocissimo. Consigliato per coding quotidiano." },
        { "Qwen3 8B",               "qwen3:8b",              8.0f,  5.2f,  53, 16,
          "Generale",    "Ottimo italiano, think nativo, bilanciato. Consigliato come modello principale." },
        { "DeepSeek-R1 7B distill", "deepseek-r1:7b",        7.0f,  4.7f,  50, 18,
          "Ragionamento","Chain-of-thought nativo. Ottimo per matematica scolastica e problemi logici." },
        { "Qwen2.5 Math 7B",        "qwen2.5-math:7b",       7.0f,  4.7f,  50, 18,
          "Matematica",  "Specializzato matematica. MATH benchmark: 82%. Ideale per Prismalux Matematica." },
        { "Mistral 7B v0.3",        "mistral:7b",            7.0f,  4.1f,  46, 20,
          "Generale",    "Classico affidabile. Veloce, buon italiano. Ottimo come fallback leggero." },
        { "Gemma 3 4B",             "gemma3:4b",             4.0f,  3.3f,  43, 28,
          "Generale",    "Google. Velocissimo, buona comprensione. Ideale su PC con 8 GB RAM." },
        { "Llama 3.2 3B",           "llama3.2:3b",           3.0f,  2.0f,  40, 35,
          "Generale",    "Meta. Ultraleggero. Ottimo per test e prototipazione rapida." },
        { "DeepSeek-R1 1.5B",       "deepseek-r1:1.5b",      1.5f,  1.1f,  32, 50,
          "Ragionamento","Pi\xc3\xb9 piccolo modello reasoning. Sorprendentemente capace su matematica semplice." },
        /* ── Test locali Prismalux (2026-04-15) ── */
        { "Mistral 7B Instruct",    "mistral:7b-instruct",   7.0f,  4.1f,  66, 20,
          "Generale",    "Score Prismalux: 66.2/100 (test 4 livelli). Stabile, buona copertura keyword." },
        { "Qwen3 4B",               "qwen3:4b",              4.0f,  3.0f,  55, 30,
          "Generale",    "Score Prismalux: 55.4/100. Calo su L2 con think:ON (fix T6 applicato). Velocissimo." },
    };
    static const int N_RANK = static_cast<int>(sizeof(RANK) / sizeof(RANK[0]));

    /* ══════════════════════════════════════════════════════════
       Funzione populate — applica filtri e riempie la tabella
       ══════════════════════════════════════════════════════════ */
    auto populate = [=]() {
        const int maxRam  = filterCombo->currentData().toInt();
        const QString cat = filterCatCombo->currentData().toString();
        const int sortBy  = sortCombo->currentData().toInt();

        /* Raccoglie indici validi */
        QVector<int> idxs;
        for (int i = 0; i < N_RANK; i++) {
            const auto& e = RANK[i];
            if (maxRam > 0 && e.ram_gb > maxRam) continue;
            if (!cat.isEmpty() && cat != e.category) continue;
            idxs.append(i);
        }

        /* Ordina */
        if (sortBy == 0) {
            /* Score decrescente */
            std::sort(idxs.begin(), idxs.end(),
                      [](int a, int b){ return RANK[a].score > RANK[b].score; });
        } else if (sortBy == 1) {
            /* RAM crescente */
            std::sort(idxs.begin(), idxs.end(),
                      [](int a, int b){ return RANK[a].ram_gb < RANK[b].ram_gb; });
        } else {
            /* Velocità decrescente */
            std::sort(idxs.begin(), idxs.end(),
                      [](int a, int b){ return RANK[a].speed_tps > RANK[b].speed_tps; });
        }

        table->setRowCount(idxs.size());
        for (int row = 0; row < idxs.size(); row++) {
            const auto& e = RANK[idxs[row]];

            /* Colonna 0: rank */
            auto* rankItem = new QTableWidgetItem(QString::number(row + 1));
            rankItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, 0, rankItem);

            /* Colonna 1: nome modello */
            auto* nameItem = new QTableWidgetItem(QString::fromUtf8(e.display));
            nameItem->setData(Qt::UserRole, idxs[row]);
            table->setItem(row, 1, nameItem);

            /* Colonna 2: parametri */
            const QString paramStr = (e.params_b >= 10.0f)
                ? QString::number(qRound(e.params_b)) + "B"
                : QString::number(e.params_b, 'f', 1) + "B";
            auto* paramItem = new QTableWidgetItem(paramStr);
            paramItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, 2, paramItem);

            /* Colonna 3: RAM */
            const QString ramStr = (e.ram_gb >= 1.0f)
                ? "~" + QString::number(qRound(e.ram_gb)) + " GB"
                : "< 1 GB";
            auto* ramItem = new QTableWidgetItem(ramStr);
            ramItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, 3, ramItem);

            /* Colonna 4: score con barra visuale */
            const QString scoreBar = QString("\xe2\x96\x88").repeated(e.score / 14);
            auto* scoreItem = new QTableWidgetItem(
                QString::number(e.score) + "  " + scoreBar);
            scoreItem->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
            /* Colore score: verde > 70, giallo > 50, rosso sotto */
            if (e.score >= 70)
                scoreItem->setForeground(QColor("#4ade80"));
            else if (e.score >= 50)
                scoreItem->setForeground(QColor("#fbbf24"));
            else
                scoreItem->setForeground(QColor("#f87171"));
            table->setItem(row, 4, scoreItem);

            /* Colonna 5: velocità */
            const QString speedStr = (e.speed_tps > 0)
                ? "~" + QString::number(e.speed_tps) + " t/s"
                : "N/D";
            auto* speedItem = new QTableWidgetItem(speedStr);
            speedItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, 5, speedItem);

            /* Colonna 6: entra in 64 GB */
            auto* fitItem = new QTableWidgetItem(e.ram_gb <= 64.0f ? "\xe2\x9c\x85" : "\xe2\x9d\x8c");
            fitItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, 6, fitItem);

            /* Colonna 7: categoria */
            auto* catItem = new QTableWidgetItem(QString::fromUtf8(e.category));
            table->setItem(row, 7, catItem);
        }
    };
    populate();

    /* ── Selezione riga → aggiorna dettaglio ── */
    connect(table, &QTableWidget::currentCellChanged, page,
            [=](int row, int /*col*/, int, int) {
        if (row < 0 || row >= table->rowCount()) {
            installBtn->setEnabled(false);
            detailLbl->setText(
                "<span style='color:#94a3b8;'>Seleziona una riga per i dettagli.</span>");
            return;
        }
        const int idx = table->item(row, 1)->data(Qt::UserRole).toInt();
        const auto& e = RANK[idx];
        const bool hasOllama = (e.ollama[0] != '\0');
        installBtn->setEnabled(hasOllama);
        installBtn->setText(hasOllama
            ? QString("\xe2\xac\x87  ollama pull %1").arg(e.ollama)
            : "\xe2\x9d\x8c  Non disponibile su Ollama");

        const QString paramStr = (e.params_b >= 10.0f)
            ? QString::number(qRound(e.params_b)) + "B"
            : QString::number(e.params_b, 'f', 1) + "B";

        detailLbl->setText(QString(
            "<b style='color:#e2e8f0;'>%1</b> &nbsp;"
            "<span style='color:#94a3b8;'>%2 &bull; %3 GB RAM (Q4) &bull; score %4/100 &bull; ~%5 t/s su CPU</span><br>"
            "<span style='color:#cbd5e1;font-size:12px;'>%6</span>")
            .arg(QString::fromUtf8(e.display))
            .arg(paramStr)
            .arg(QString::number(e.ram_gb, 'f', 0))
            .arg(e.score)
            .arg(e.speed_tps)
            .arg(QString::fromUtf8(e.notes)));
    });

    /* ── Installa ── */
    connect(installBtn, &QPushButton::clicked, page, [=]() {
        const int row = table->currentRow();
        if (row < 0) return;
        const int idx = table->item(row, 1)->data(Qt::UserRole).toInt();
        const auto& e = RANK[idx];
        if (e.ollama[0] == '\0') return;

        const QString cmd = QString("ollama pull %1").arg(e.ollama);
        logLbl->setText(QString("\xf0\x9f\x93\xa5  Avvio: <code>%1</code>...").arg(cmd));
        logLbl->setVisible(true);
        installBtn->setEnabled(false);

        auto* proc = new QProcess(page);
        proc->setProcessChannelMode(QProcess::MergedChannels);
        connect(proc, &QProcess::readyRead, page, [proc, logLbl]() {
            const QString s = QString::fromUtf8(proc->readAll()).trimmed();
            if (!s.isEmpty()) logLbl->setText(s);
        });
        connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                page, [proc, logLbl, installBtn, e](int code, QProcess::ExitStatus) {
            if (code == 0)
                logLbl->setText(QString("\xe2\x9c\x85  %1 installato con successo.")
                                 .arg(e.display));
            else
                logLbl->setText("\xe2\x9d\x8c  Installazione fallita. Ollama attivo?");
            installBtn->setEnabled(true);
            proc->deleteLater();
        });
        proc->start("ollama", {"pull", QString::fromUtf8(e.ollama)});
    });

    /* ── Filtri → repopulate ── */
    connect(filterCombo,    QOverload<int>::of(&QComboBox::currentIndexChanged),
            page, populate);
    connect(filterCatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            page, populate);
    connect(sortCombo,      QOverload<int>::of(&QComboBox::currentIndexChanged),
            page, populate);

    return page;
}
