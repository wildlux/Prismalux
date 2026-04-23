/*
 * grafico_page.cpp — GraficoPage implementation
 * ===============================================
 * Layout: pannello parametri (sx) + GraficoCanvas (dx).
 * Il rendering del canvas è in grafico_canvas.cpp.
 */
#include "pages/grafico_page.h"
#include "widgets/formula_parser.h"
#include "ai_client.h"

#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFrame>
#include <QFileDialog>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QFont>
#include <QMessageBox>
#include <algorithm>

/* ══════════════════════════════════════════════════════════════
   GraficoPage — layout
   ══════════════════════════════════════════════════════════════ */
GraficoPage::GraficoPage(AiClient* ai, QWidget* parent) : QWidget(parent), m_ai(ai) {
    auto* mainLay = new QHBoxLayout(this);
    mainLay->setContentsMargins(0,0,0,0);
    mainLay->setSpacing(0);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setHandleWidth(4);
    splitter->addWidget(buildLeftPanel());

    m_canvas = new GraficoCanvas(this);
    connect(m_canvas, &GraficoCanvas::statusMessage,
            this, [this](const QString& msg){ m_statusLbl->setText(msg); });
    splitter->addWidget(m_canvas);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({278, 700});
    mainLay->addWidget(splitter);
}

/* ── Pannello parametri sinistro ─────────────────────────────── */
QWidget* GraficoPage::buildLeftPanel() {
    auto* panel = new QWidget(this);
    panel->setObjectName("leftPanel");
    panel->setMinimumWidth(220);
    panel->setMaximumWidth(310);

    auto* lay = new QVBoxLayout(panel);
    lay->setContentsMargins(10,10,10,10);
    lay->setSpacing(8);

    /* ── Titolo + tab 2D/3D sulla stessa riga ── */
    auto* titleRow = new QWidget(panel);
    auto* titleLay = new QHBoxLayout(titleRow);
    titleLay->setContentsMargins(0, 0, 0, 0);
    titleLay->setSpacing(8);

    auto* title = new QLabel("\xf0\x9f\x93\x88  Grafico", titleRow);
    title->setObjectName("pageTitle");
    titleLay->addWidget(title, 1);

    m_dimBar = new QTabBar(titleRow);
    m_dimBar->setObjectName("innerTabs");
    m_dimBar->addTab("2D");
    m_dimBar->addTab("3D");
    m_dimBar->setCurrentIndex(0);
    titleLay->addWidget(m_dimBar);

    lay->addWidget(titleRow);

    /* Tipo grafico */
    auto* lblType = new QLabel("Tipo:", panel);
    lblType->setObjectName("formLabel");
    lay->addWidget(lblType);

    /* Tabella completa dei tipi — usata da populateTypeCombo() per filtrare 2D/3D */
    struct ChartItem { const char* text; int typeId; bool is3d; };
    static const ChartItem kChartItems[] = {
        { "\xf0\x9f\x93\x88  Cartesiano  y = f(x)",                           0,  false },
        { "\xf0\x9f\xa5\xa7  Torta",                                           1,  false },
        { "\xf0\x9f\x93\x8a  Istogramma",                                      2,  false },
        { "\xf0\x9f\x94\xb9  Scatter 2D  (x, y)",                             3,  false },
        { "\xf0\x9f\x95\xb8  Grafo 2D",                                        4,  false },
        { "\xf0\x9f\x8c\x90  Scatter 3D",                                      5,  true  },
        { "\xf0\x9f\x94\xb7  Grafo 3D",                                        6,  true  },
        { "\xf0\x9f\x94\xb5  Smith \xe2\x80\x94 Numeri Primi",                 7,  false },
        { "\xf0\x9f\x93\x90  Smith \xe2\x80\x94 \xcf\x80\xc2\xb7""e\xc2\xb7Primi", 8, false },
        { "\xf0\x9f\x93\x89  Linea multi-serie",                               9,  false },
        { "\xf0\x9f\x8c\x80  Polare  r = f(\xce\xb8)",                        10,  false },
        { "\xf0\x9f\x95\xb7  Radar (Spider)",                                  11, false },
        { "\xf0\x9f\xab\xa7  Bolle  (x, y, raggio)",                           12, false },
        { "\xf0\x9f\x94\xa5  Heatmap (matrice colori)",                         13, false },
        { "\xf0\x9f\x95\xaf  Candele OHLC",                                    14, false },
        { "\xf0\x9f\x8f\x94  Area riempita",                                   15, false },
        { "\xf0\x9f\x8c\x8a  Cascata (Waterfall)",                             16, false },
        { "\xf0\x9f\x93\xb6  Scalini (Step)",                                  17, false },
        { "\xf0\x9f\x8f\x9b  Colonne (Column)",                                18, false },
        { "\xe2\x96\xac  Barre orizzontali (HBar)",                             19, false },
        { "\xf0\x9f\x97\x82  Barre raggruppate (Grouped)",                     20, false },
        { "\xf0\x9f\x93\x9a  Barre impilate (Stacked)",                        21, false },
        { "\xf0\x9f\x92\xaf  Barre impilate 100%",                             22, false },
        { "\xf0\x9f\xaa\xa3  Imbuto (Funnel)",                                 23, false },
        { "\xf0\x9f\x8d\xa9  Ciambella (Donut)",                               24, false },
        { "\xf0\x9f\x97\xba  Treemap",                                         25, false },
        { "\xe2\x98\x80  Sunburst",                                             26, false },
        { "\xf0\x9f\x93\xa6  Box Plot",                                        27, false },
        { "\xf0\x9f\x94\xb5  Dot Plot",                                        28, false },
        { "\xf0\x9f\x8c\x8a  Densit\xc3\xa0 (KDE)",                           29, false },
        { "\xf0\x9f\x8f\x94  Area impilata (Stacked Area)",                    30, false },
        { "\xf0\x9f\x93\x8a  OHLC (barre)",                                    31, false },
        { "\xf0\x9f\x8e\xaf  Gauge (semicerchio)",                             32, false },
        { "\xf0\x9f\x8e\xaf  Bullet Chart",                                    33, false },
        { "\xf0\x9f\x93\x85  Gantt",                                           34, false },
        { "\xf0\x9f\x94\xba  Piramide (Pyramid)",                              35, false },
        { "\xe2\x87\x94  Coordinate parallele",                                 36, false },
        { "\xf0\x9f\x8c\x8a  Sankey",                                          37, false },
        { "\xf0\x9f\x8c\xb3  Albero (Tree)",                                   38, false },
        { "\xf0\x9f\x94\x97  Chord",                                           39, false },
        { "\xf0\x9f\x8e\xbb  Violin Plot",                                     40, false },
        { "\xe2\x98\x81  Word Cloud",                                           41, false },
        { "\xf0\x9f\x8c\x9f  Albero Radiale",                                  42, false },
        { "\xe2\x96\xb6  Linea Animata",                                        43, false },
        { "\xe2\x8a\x9e  Small Multiples",                                      44, false },
    };

    m_typeCombo = new QComboBox(panel);
    m_typeCombo->setObjectName("settingCombo");
    /* Carica solo tipi 2D all'avvio — populateTypeCombo(0) */
    for (const auto& item : kChartItems)
        if (!item.is3d)
            m_typeCombo->addItem(QString::fromUtf8(item.text), item.typeId);
    lay->addWidget(m_typeCombo);

    /* ── Stack parametri ── */
    m_paramStack = new QStackedWidget(panel);

    /* [0] Cartesiano */
    {
        auto* w  = new QWidget;
        auto* fl = new QFormLayout(w);
        fl->setContentsMargins(0,0,0,0);
        fl->setSpacing(6);

        m_formulaEdit = new QLineEdit(w);
        m_formulaEdit->setObjectName("inputLine");
        m_formulaEdit->setPlaceholderText("sin(x) * 2 + 1");
        m_formulaEdit->setText("sin(x)");
        connect(m_formulaEdit, &QLineEdit::returnPressed, this, &GraficoPage::plot);
        fl->addRow("y = f(x):", m_formulaEdit);

        m_xMinSpin = new QDoubleSpinBox(w);
        m_xMinSpin->setRange(-1e6, 1e6);
        m_xMinSpin->setValue(-6.28);
        m_xMinSpin->setSingleStep(1.0);
        m_xMinSpin->setDecimals(3);
        fl->addRow("x min:", m_xMinSpin);

        m_xMaxSpin = new QDoubleSpinBox(w);
        m_xMaxSpin->setRange(-1e6, 1e6);
        m_xMaxSpin->setValue(6.28);
        m_xMaxSpin->setSingleStep(1.0);
        m_xMaxSpin->setDecimals(3);
        fl->addRow("x max:", m_xMaxSpin);

        auto* hint = new QLabel(
            "Funzioni:  sin(x)  cos(x)  tan(x)\n"
            "           sqrt(x)  abs(x)  exp(x)\n"
            "           log(x)  ln(x)  log2(x)\n"
            "           ceil(x)  floor(x)\n"
            "\n"
            "Costanti:  pi = 3.14159265\n"
            "           e  = 2.71828182\n"
            "\n"
            "Esempi:\n"
            "  sin(x)                (sinusoide)\n"
            "  x^2 - 3*x + 2        (parabola)\n"
            "  1 / (1 + exp(-x))    (sigmoide)\n"
            "  sqrt(1 - x^2)        (semicerchio)\n"
            "  sin(x) / x           (sinc)\n"
            "  2^x                  (esponenziale)\n"
            "  abs(sin(x)) * x      (inviluppo)", w);
        hint->setObjectName("hintLabel");
        hint->setFont(QFont("JetBrains Mono,Fira Code,Consolas", 8));
        hint->setWordWrap(false);
        fl->addRow(hint);

        m_paramStack->addWidget(w);   /* idx 0 */
    }

    /* [1] Dati generici — condiviso da Torta/Isto/Scatter/Grafo/3D */
    {
        auto* w  = new QWidget;
        auto* vl = new QVBoxLayout(w);
        vl->setContentsMargins(0,0,0,0);
        vl->setSpacing(4);

        /* ── Sezione "Genera da espressione" ─────────────────────── */
        m_genSection = new QWidget(w);
        {
            auto* gl = new QVBoxLayout(m_genSection);
            gl->setContentsMargins(0, 0, 0, 2);
            gl->setSpacing(4);

            auto* titleLbl = new QLabel(
                "\xf0\x9f\x93\x90  Genera da espressione", m_genSection);
            titleLbl->setObjectName("formLabel");
            gl->addWidget(titleLbl);

            /* Formula */
            m_genExpr = new QLineEdit(m_genSection);
            m_genExpr->setObjectName("inputLine");
            m_genExpr->setFont(QFont("JetBrains Mono,Fira Code,Consolas", 9));
            m_genExpr->setPlaceholderText("f(x)  es: x^2  sin(x)  x*(x+1)/2");
            gl->addWidget(m_genExpr);

            /* Range — da/a/passo su una riga */
            auto* rangeRow = new QWidget(m_genSection);
            auto* rl = new QHBoxLayout(rangeRow);
            rl->setContentsMargins(0, 0, 0, 0);
            rl->setSpacing(4);

            auto* lFrom = new QLabel("x da:", rangeRow);
            lFrom->setObjectName("formLabel");
            m_genFrom = new QDoubleSpinBox(rangeRow);
            m_genFrom->setObjectName("inputLine");
            m_genFrom->setRange(-1e6, 1e6);
            m_genFrom->setValue(1.0);
            m_genFrom->setDecimals(2);
            m_genFrom->setSingleStep(1.0);

            auto* lTo = new QLabel("a:", rangeRow);
            lTo->setObjectName("formLabel");
            m_genTo = new QDoubleSpinBox(rangeRow);
            m_genTo->setObjectName("inputLine");
            m_genTo->setRange(-1e6, 1e6);
            m_genTo->setValue(10.0);
            m_genTo->setDecimals(2);
            m_genTo->setSingleStep(1.0);

            auto* lStep = new QLabel("\xce\x94:", rangeRow);  /* Δ */
            lStep->setObjectName("formLabel");
            m_genStep = new QDoubleSpinBox(rangeRow);
            m_genStep->setObjectName("inputLine");
            m_genStep->setRange(1e-6, 1e6);
            m_genStep->setValue(1.0);
            m_genStep->setDecimals(3);
            m_genStep->setSingleStep(0.5);

            rl->addWidget(lFrom);
            rl->addWidget(m_genFrom, 2);
            rl->addWidget(lTo);
            rl->addWidget(m_genTo, 2);
            rl->addWidget(lStep);
            rl->addWidget(m_genStep, 2);
            gl->addWidget(rangeRow);

            auto* btnGen = new QPushButton(
                "\xe2\x96\xb6  Genera \xe2\x86\x92 inserisce i valori sotto", m_genSection);
            btnGen->setObjectName("actionBtn");
            btnGen->setFixedHeight(26);
            connect(btnGen, &QPushButton::clicked, this, [this]() {
                QString expr = m_genExpr->text().trimmed();
                if (expr.isEmpty()) return;
                FormulaParser fp(expr);
                if (!fp.ok()) {
                    m_statusLbl->setText("\xe2\x9a\xa0  " + fp.err());
                    return;
                }
                double x0   = m_genFrom->value();
                double x1   = m_genTo->value();
                double step = m_genStep->value();
                if (x0 > x1 || step <= 0) {
                    m_statusLbl->setText("\xe2\x9a\xa0  Range non valido");
                    return;
                }

                int type = m_typeCombo->currentData().toInt();
                QString out;
                int idx = 0;
                for (double x = x0; x <= x1 + step * 1e-9; x += step) {
                    double y = fp.eval(x);
                    if (!std::isfinite(y)) { ++idx; continue; }
                    QString sx = QString::number(x,  'g', 8);
                    QString sy = QString::number(y,  'g', 8);
                    if (type == 1 || type == 2) {
                        /* Torta / Istogramma: "etichetta: valore" */
                        out += QString("x=%1: %2\n").arg(sx, sy);
                    } else if (type == 3) {
                        /* Scatter XY: "x, y" */
                        out += QString("%1, %2\n").arg(sx, sy);
                    } else if (type == 5) {
                        /* Scatter 3D: "x, y, 0" — usa x come X, f(x) come Y, 0 come Z */
                        out += QString("%1, %2, 0\n").arg(sx, sy);
                    } else {
                        /* default: "x, y" */
                        out += QString("%1, %2\n").arg(sx, sy);
                    }
                    ++idx;
                }
                if (out.isEmpty()) {
                    m_statusLbl->setText("\xe2\x9a\xa0  Nessun valore finito generato");
                    return;
                }
                m_dataEdit->setPlainText(out.trimmed());
                m_statusLbl->setText(
                    QString("\xe2\x9c\x85  %1 valori generati da %2").arg(idx).arg(expr));
            });
            gl->addWidget(btnGen);

            auto* sepLine = new QFrame(m_genSection);
            sepLine->setFrameShape(QFrame::HLine);
            sepLine->setObjectName("separator");
            gl->addWidget(sepLine);
        }
        vl->addWidget(m_genSection);

        /* ── Hint formato manuale ───────────────────────────────── */
        m_dataHint = new QLabel("etichetta:valore (una per riga)", w);
        m_dataHint->setObjectName("hintLabel");
        m_dataHint->setFont(QFont("Inter,Ubuntu,sans-serif", 8));
        m_dataHint->setWordWrap(true);
        vl->addWidget(m_dataHint);

        m_dataEdit = new QTextEdit(w);
        m_dataEdit->setObjectName("inputArea");
        m_dataEdit->setFont(QFont("JetBrains Mono,Fira Code,Consolas", 9));
        m_dataEdit->setMinimumHeight(100);
        vl->addWidget(m_dataEdit, 1);

        m_paramStack->addWidget(w);   /* idx 1 */
    }

    /* [2] Smith Prime */
    {
        auto* w  = new QWidget;
        auto* fl = new QFormLayout(w);
        fl->setContentsMargins(0, 0, 0, 0);
        fl->setSpacing(7);

        m_smithMaxN = new QSpinBox(w);
        m_smithMaxN->setRange(10, 50000);
        m_smithMaxN->setValue(300);
        m_smithMaxN->setSingleStep(50);
        m_smithMaxN->setSuffix("  (n max)");
        m_smithMaxN->setObjectName("inputLine");
        fl->addRow("Fino a:", m_smithMaxN);

        auto* normRow = new QWidget(w);
        auto* normLay = new QHBoxLayout(normRow);
        normLay->setContentsMargins(0,0,0,0); normLay->setSpacing(4);
        m_smithNorm = new QDoubleSpinBox(normRow);
        m_smithNorm->setRange(0.1, 20.0);
        m_smithNorm->setValue(1.0);
        m_smithNorm->setSingleStep(0.1);
        m_smithNorm->setDecimals(2);
        m_smithNorm->setObjectName("inputLine");
        normLay->addWidget(m_smithNorm, 1);
        fl->addRow("Scala:", normRow);

        m_smithReal  = new QCheckBox("Primi reali (rosso)",           w);
        m_smithGauss = new QCheckBox("Primi gaussiani (blu/giallo)",  w);
        m_smithFib   = new QCheckBox("Fibonacci (giallo)",            w);
        m_smithTri   = new QCheckBox("Triangolari (ciano)",           w);
        m_smithSqr   = new QCheckBox("Quadrati perfetti (verde)",     w);
        m_smithExp   = new QCheckBox("Dominio espanso (|Γ|≤2)",       w);
        m_smithLabels= new QCheckBox("Mostra etichette numeriche",    w);
        m_smithReal ->setChecked(true);
        m_smithGauss->setChecked(true);
        m_smithFib  ->setChecked(false);
        m_smithTri  ->setChecked(false);
        m_smithSqr  ->setChecked(false);
        m_smithExp  ->setChecked(false);
        m_smithLabels->setChecked(false);
        fl->addRow(m_smithReal);
        fl->addRow(m_smithGauss);
        fl->addRow(m_smithFib);
        fl->addRow(m_smithTri);
        fl->addRow(m_smithSqr);
        fl->addRow(m_smithExp);
        fl->addRow(m_smithLabels);

        /* ── Separatore serie personalizzata ── */
        auto* sep = new QFrame(w);
        sep->setFrameShape(QFrame::HLine);
        sep->setObjectName("separator");
        fl->addRow(sep);

        auto* customLbl = new QLabel(
            "\xf0\x9f\x93\x9d  Serie personalizzata (magenta)", w);
        customLbl->setObjectName("formLabel");
        fl->addRow(customLbl);

        m_smithCustom = new QTextEdit(w);
        m_smithCustom->setObjectName("inputArea");
        m_smithCustom->setFont(QFont("JetBrains Mono,Fira Code,Consolas", 9));
        m_smithCustom->setFixedHeight(80);
        m_smithCustom->setPlaceholderText(
            "Inserisci numeri interi \xe2\x89\xa5 2\n"
            "Uno per riga, o separati da spazi/virgole.\n"
            "Es:  3  7  13  21  34  55  89");
        fl->addRow(m_smithCustom);

        m_btnSmithFile = new QPushButton(
            "\xf0\x9f\x93\x82  Carica sequenza da file (.txt)", w);
        m_btnSmithFile->setObjectName("actionBtn");
        m_btnSmithFile->setFixedHeight(28);
        m_btnSmithFile->setToolTip(
            "Apri un file di testo con numeri interi >= 2.\n"
            "Formati accettati: uno per riga, separati da spazi o virgole.");
        connect(m_btnSmithFile, &QPushButton::clicked, this, [this]() {
            QString path = QFileDialog::getOpenFileName(
                this, "Carica sequenza", QDir::homePath(),
                "File testo (*.txt *.csv *.dat);;Tutti i file (*)");
            if (path.isEmpty()) return;
            QFile f(path);
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
            m_smithCustom->setPlainText(
                QString::fromUtf8(f.readAll()));
        });
        fl->addRow(m_btnSmithFile);

        auto* hint = new QLabel(
            QString::fromUtf8(
                "\xce\x93 = (z\xe2\x88\x92""1)/(z+1)   z = log\xe2\x82\x82(N)*scala\n"
                "Stessa posizione = stesso N in pi\xc3\xb9 serie:\n"
                "punti impilati + linea tratteggiata.\n"
                "Gaussiani: a+bi con a\xc2\xb2+b\xc2\xb2 primo"),
            w);
        hint->setObjectName("hintLabel");
        hint->setFont(QFont("Inter,Ubuntu,sans-serif", 8));
        hint->setWordWrap(true);
        fl->addRow(hint);

        m_paramStack->addWidget(w);   /* idx 2 */
    }

    /* [3] Math Const — π · e · Primi */
    {
        auto* w  = new QWidget;
        auto* fl = new QFormLayout(w);
        fl->setContentsMargins(0, 0, 0, 0);
        fl->setSpacing(7);

        m_mathTerms = new QSpinBox(w);
        m_mathTerms->setRange(2, 300);
        m_mathTerms->setValue(40);
        m_mathTerms->setSingleStep(10);
        m_mathTerms->setSuffix("  termini");
        m_mathTerms->setObjectName("inputLine");
        fl->addRow("Termini:", m_mathTerms);

        m_mathPi     = new QCheckBox(QString::fromUtf8(
            "\xcf\x80  Serie di Leibniz"), w);       /* π */
        m_mathE      = new QCheckBox("e  Serie di Taylor", w);
        m_mathPrimes = new QCheckBox("Primi reali (log\xe2\x82\x82)", w);  /* log₂ */
        m_mathPi    ->setChecked(true);
        m_mathE     ->setChecked(true);
        m_mathPrimes->setChecked(true);
        fl->addRow(m_mathPi);
        fl->addRow(m_mathE);
        fl->addRow(m_mathPrimes);

        auto* hint = new QLabel(
            QString::fromUtf8(
                "Mappe di M\xc3\xb6""bius: \xce\x93=(z\xe2\x88\x92" "1)/(z+1)\n"
                "\n"
                "\xcf\x80 (arancio) — oscillazione\n"
                "  S\xe2\x82\x99 = 4\xc2\xb7\xce\xa3(-1)\xe2\x81\xb5/(2k+1)\n"
                "  \xe2\x86\x92 \xce\x93(\xcf\x80) \xe2\x89\x88 0.5168\n"
                "\n"
                "e (verde) — convergenza monotona\n"
                "  E\xe2\x82\x99 = \xce\xa3 1/k!\n"
                "  \xe2\x86\x92 \xce\x93(e) \xe2\x89\x88 0.4621\n"
                "\n"
                "Primi (rosso) — scala log\xe2\x82\x82\n"
                "  z = log\xe2\x82\x82(p)\n"
                "  sull'asse reale del disco"),
            w);
        hint->setObjectName("hintLabel");
        hint->setFont(QFont("Inter,Ubuntu,sans-serif", 8));
        hint->setWordWrap(true);
        fl->addRow(hint);

        m_paramStack->addWidget(w);   /* idx 3 */
    }

    /* [4] Polare r = f(θ) */
    {
        auto* w  = new QWidget;
        auto* fl = new QFormLayout(w);
        fl->setContentsMargins(0, 0, 0, 0);
        fl->setSpacing(7);

        m_polarFormula = new QLineEdit(w);
        m_polarFormula->setObjectName("inputLine");
        m_polarFormula->setPlaceholderText("cos(3*x)  oppure  sin(2*x)*2");
        m_polarFormula->setText("cos(3*x)");
        connect(m_polarFormula, &QLineEdit::returnPressed, this, &GraficoPage::plot);
        fl->addRow("r = f(\xce\xb8):", m_polarFormula);

        m_polarTMinSpin = new QDoubleSpinBox(w);
        m_polarTMinSpin->setRange(-100.0, 100.0);
        m_polarTMinSpin->setValue(0.0);
        m_polarTMinSpin->setSingleStep(0.1);
        m_polarTMinSpin->setDecimals(3);
        m_polarTMinSpin->setObjectName("inputLine");
        fl->addRow("\xce\xb8 min:", m_polarTMinSpin);

        m_polarTMaxSpin = new QDoubleSpinBox(w);
        m_polarTMaxSpin->setRange(-100.0, 100.0);
        m_polarTMaxSpin->setValue(6.283);
        m_polarTMaxSpin->setSingleStep(0.1);
        m_polarTMaxSpin->setDecimals(3);
        m_polarTMaxSpin->setObjectName("inputLine");
        fl->addRow("\xce\xb8 max:", m_polarTMaxSpin);

        m_polarSamples = new QSpinBox(w);
        m_polarSamples->setRange(50, 5000);
        m_polarSamples->setValue(800);
        m_polarSamples->setSingleStep(100);
        m_polarSamples->setObjectName("inputLine");
        fl->addRow("Campioni:", m_polarSamples);

        auto* hint = new QLabel(
            "Esempi:\n"
            "  cos(3*x)          (rosa a 3 petali)\n"
            "  sin(2*x)*2        (rosa a 4 petali)\n"
            "  1 - cos(x)        (cardioide)\n"
            "  1 + 2*cos(x)      (limacon)\n"
            "  x / (2*pi)        (spirale)\n"
            "  sqrt(cos(2*x))    (lemniscata)", w);
        hint->setObjectName("hintLabel");
        hint->setFont(QFont("JetBrains Mono,Fira Code,Consolas", 8));
        hint->setWordWrap(false);
        fl->addRow(hint);

        m_paramStack->addWidget(w);   /* idx 4 */
    }

    lay->addWidget(m_paramStack, 1);

    /* Esempi per ogni tipo non-cartesiano */
    static const char* kExamples[] = {
        /* 0 Cartesiano — non usa dataEdit */
        "",
        /* 1 Torta — etichetta:valore */
        "Nord:42\nSud:28\nEst:19\nOvest:11",
        /* 2 Istogramma — etichetta:valore */
        "Gen:82\nFeb:67\nMar:91\nApr:78\nMag:95\nGiu:88\nLug:76\nAgo:60\nSet:85\nOtt:93\nNov:71\nDic:55",
        /* 3 Scatter (x, y [, etichetta]) */
        "0.0, 0.0, Origine\n1.0, 2.5, A\n2.0, 3.8, B\n3.5, 6.1, C\n4.0, 7.2, D\n5.0, 9.8, E\n6.5, 11.4, F\n7.0, 13.0, G",
        /* 4 Grafo (nodo-nodo) */
        "A-B\nA-C\nB-D\nC-D\nD-E\nB-E",
        /* 5 Scatter 3D (x, y, z [, etichetta]) */
        "0, 0, 0, O\n1, 0, 0, X\n0, 1, 0, Y\n0, 0, 1, Z\n1, 1, 0, XY\n1, 0, 1, XZ\n0, 1, 1, YZ\n1, 1, 1, XYZ",
        /* 6 Grafo 3D — tetraedro regolare */
        "A, 0, 0, 0\nB, 2, 0, 0\nC, 1, 1.73, 0\nD, 1, 0.58, 1.63\nA-B\nA-C\nA-D\nB-C\nB-D\nC-D",
        /* 7 Smith — non usa dataEdit */  "",
        /* 8 MathConst — non usa dataEdit */ "",
        /* 9 Linea multi-serie: x, y1, y2, ... */
        "# Vendite  Costi\n0, 10, 8\n1, 14, 9\n2, 18, 11\n3, 22, 13\n4, 19, 15\n5, 25, 16",
        /* 10 Polare — non usa dataEdit */ "",
        /* 11 Radar */
        "Velocita:85\nResistenza:72\nAgilita:90\nForza:60\nPrecisione:78",
        /* 12 Bolle: x, y, raggio [, etichetta] */
        "1.0, 2.5, 30, Alfa\n2.5, 4.0, 50, Beta\n4.0, 1.5, 20, Gamma\n5.5, 3.8, 70, Delta\n7.0, 2.0, 45, Epsilon",
        /* 13 Heatmap: righe di valori separati da spazio/virgola */
        "1 2 3 4 5\n6 7 8 9 10\n11 12 13 14 15\n16 17 18 19 20\n21 22 23 24 25",
        /* 14 Candlestick: Etichetta, Open, High, Low, Close */
        "Gen, 100, 115, 98, 112\nFeb, 112, 120, 108, 105\nMar, 105, 125, 102, 118\nApr, 118, 130, 115, 125\nMag, 125, 128, 110, 108",
        /* 15 Area — stessa struttura di Linea */
        "# Entrate  Uscite\n0, 12, 8\n1, 16, 10\n2, 22, 12\n3, 28, 14\n4, 25, 18\n5, 32, 20",
        /* 16 Waterfall */
        "Inizio:1000\nVendite:+350\nResi:-80\nCosti:-200\nSussidi:+120\nFine:1190",
        /* 17 Step — stessa struttura di Linea */
        "# Soglia  Reale\n0, 10, 9\n1, 10, 12\n2, 15, 14\n3, 15, 17\n4, 20, 18\n5, 20, 22"
    };

    static const char* kHints[] = {
        "",
        "Formato: etichetta:valore  (una voce per riga)\n"
        "Accetta anche solo numeri (es. 42)",
        "Formato: etichetta:valore  (una voce per riga)\n"
        "Accetta anche solo numeri (es. 82)",
        "Formato: x, y [, Nome]  (una per riga)\n"
        "  2.5, 3.1, A\n"
        "  4.0, 7.2, B\n"
        "  6.5, 11.4\n"
        "Se il nome manca usa P1, P2, P3...",
        "Formato: Nodo1-Nodo2  (un arco per riga)\n"
        "Accetta anche: ->  \xe2\x80\x93  (trattino lungo)",
        "Formato: x, y, z [, Nome]  (una per riga)\n"
        "  0, 0, 0, Origine\n"
        "  1, 0, 0, X\n"
        "  0, 1, 0, Y\n"
        "Se il nome manca usa P1, P2, P3...",
        /* 6 Grafo 3D */
        "Nodi:   Nome, x, y, z\n"
        "Archi:  NodoA-NodoB\n"
        "(nodi e archi nello stesso testo)\n"
        "Esempio:\n"
        "  A, 0, 0, 0\n"
        "  B, 1, 0, 0\n"
        "  C, 0, 1, 0\n"
        "  A-B\n"
        "  A-C\n"
        "  B-C\n"
        "# Le righe con # sono ignorate",
        /* 7 SmithPrime */ "",
        /* 8 MathConst  */ "",
        /* 9 Linea multi-serie */
        "Formato: x, y1, y2, ... (colonne = serie)\n"
        "Riga #: nomi delle serie (opzionale)\n"
        "  # SerieA SerieB SerieC\n"
        "  0, 10, 8, 5\n"
        "  1, 14, 9, 7",
        /* 10 Polare — pannello dedicato */ "",
        /* 11 Radar */
        "Formato: Categoria:valore (una per riga)\n"
        "Minimo 3 categorie.\n"
        "  Velocita:85\n"
        "  Resistenza:72\n"
        "  Forza:60",
        /* 12 Bolle */
        "Formato: x, y, raggio [, Nome]\n"
        "  1.0, 2.5, 30, Alfa\n"
        "  2.5, 4.0, 50, Beta\n"
        "Il raggio e' in unita' pixel normalizzate.",
        /* 13 Heatmap */
        "Formato: valori separati da spazio o virgola,\n"
        "una riga per ogni riga della matrice.\n"
        "  1 2 3\n"
        "  4 5 6\n"
        "  7 8 9",
        /* 14 Candlestick */
        "Formato: Etichetta, Open, High, Low, Close\n"
        "  Gen, 100, 115, 98, 112\n"
        "  Feb, 112, 120, 108, 105",
        /* 15 Area — come Linea */
        "Formato: x, y1, y2, ... (colonne = serie)\n"
        "L'area sotto ogni curva viene riempita.",
        /* 16 Waterfall */
        "Formato: Etichetta:valore (una per riga)\n"
        "Usa +/- per indicare variazioni.\n"
        "L'ultima voce e' il totale cumulato.",
        /* 17 Step — come Linea */
        "Formato: x, y1, y2, ... (colonne = serie)\n"
        "Le linee vengono tracciate a scalini."
    };

    /* Connetti combo — usa currentData().toInt() per ottenere il tipo reale
       anche dopo il filtraggio 2D/3D (currentIndex() non coincide più con typeId) */
    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
        const int idx = m_typeCombo->currentData().toInt();
        m_canvas->setType(static_cast<GraficoCanvas::ChartType>(idx));
        if (idx == 0) {
            m_paramStack->setCurrentIndex(0);
        } else if (idx == 7) {
            /* Smith Prime — pannello dedicato, genera subito con i valori correnti */
            m_paramStack->setCurrentIndex(2);
            m_canvas->setSmithPrime(
                m_smithMaxN->value(),
                m_smithReal->isChecked(),
                m_smithGauss->isChecked(),
                m_smithExp->isChecked(),
                m_smithNorm->value(),
                m_smithFib->isChecked(),
                m_smithTri->isChecked(),
                m_smithSqr->isChecked(),
                m_smithLabels->isChecked()
            );
        } else if (idx == 8) {
            /* Math Const — π · e · Primi, genera subito */
            m_paramStack->setCurrentIndex(3);
            m_canvas->setMathConst(
                m_mathTerms->value(),
                m_mathPi    ->isChecked(),
                m_mathE     ->isChecked(),
                m_mathPrimes->isChecked()
            );
        } else if (idx == 10) {
            /* Polare — pannello dedicato */
            m_paramStack->setCurrentIndex(4);
        } else {
            /* Tutti gli altri tipi usano il pannello dati generico (idx 1) */
            m_paramStack->setCurrentIndex(1);
            /* Carica hint ed esempi per tutti i tipi che usano questo pannello */
            static const int kMaxHint = 17;
            if (idx >= 0 && idx <= kMaxHint &&
                idx != 7 && idx != 8 && idx != 10) {
                m_dataHint->setText(kHints[idx]);
                if (!QByteArray(kExamples[idx]).isEmpty())
                    m_dataEdit->setPlainText(kExamples[idx]);
            }
            /* hint per i nuovi tipi 40-44 */
            static const char* kNewHints[] = {
                /* 40 Violin */
                "Formato: NomeSerie: v1 v2 v3 ...\nUna serie per riga.\n"
                "Esempio:\n  Gruppo A: 12 18 22 25 30 35 40\n  Gruppo B: 8 14 20 28 35 42",
                /* 41 WordCloud */
                "Formato: parola:frequenza (una per riga)\n"
                "Esempio:\n  Python:95\n  C++:80\n  Java:70\n  Rust:60",
                /* 42 RadialTree */
                "Formato: padre: figlio1, figlio2\nUguale all'Albero lineare.\n"
                "Esempio:\n  Root: A, B\n  A: C, D\n  B: E",
                /* 43 AnimatedLine */
                "Formato: stessa struttura di Linea multi-serie\n"
                "x, y1, y2, ... (colonne = serie)\n"
                "La linea viene disegnata progressivamente.",
                /* 44 SmallMultiples */
                "Formato: stessa struttura di Linea multi-serie\n"
                "x, y1, y2, ... (colonne = serie)\n"
                "Ogni serie viene mostrata in un pannello separato."
            };
            static const char* kNewExamples[] = {
                /* 40 Violin */
                "Gruppo A: 12 18 22 25 30 35 40 28 32 26\n"
                "Gruppo B: 8 14 20 28 35 42 30 24 18 22\n"
                "Gruppo C: 5 10 15 22 30 38 45 35 28 20",
                /* 41 WordCloud */
                "Python:95\nC++:80\nJava:70\nRust:60\nGo:55\n"
                "JavaScript:50\nTypeScript:45\nSwift:40\nKotlin:35\n"
                "Ruby:30\nScala:25\nHaskell:20\nElixir:18\nErlang:15",
                /* 42 RadialTree — stesso esempio di Tree */
                "Root: A, B, C\nA: D, E\nB: F\nC: G, H\nD: I\nF: J, K",
                /* 43 AnimatedLine */
                "# Vendite  Costi  Profitto\n0, 10, 8, 2\n1, 14, 9, 5\n"
                "2, 18, 11, 7\n3, 22, 13, 9\n4, 19, 15, 4\n5, 25, 16, 9\n"
                "6, 28, 17, 11\n7, 30, 18, 12",
                /* 44 SmallMultiples */
                "# Nord  Sud  Est  Ovest\n"
                "0, 10, 8, 6, 4\n1, 14, 9, 7, 5\n2, 18, 11, 9, 6\n"
                "3, 22, 13, 11, 7\n4, 19, 15, 13, 8\n5, 25, 16, 14, 9"
            };
            /* ── Esempi e hint per indici 18-39 ── */
            if (idx >= 18 && idx <= 39) {
                static const char* kHints2[] = {
                    /* 18 Column */ "Formato: etichetta:valore  (una voce per riga)",
                    /* 19 HBar   */ "Formato: etichetta:valore  (una voce per riga)",
                    /* 20 Grouped*/ "Formato: x, y1, y2, ... (colonne = serie)\nRiga #: nomi serie",
                    /* 21 Stacked*/ "Formato: x, y1, y2, ... (colonne = serie)\nRiga #: nomi serie",
                    /* 22 Stk100 */ "Formato: x, y1, y2, ... (colonne = serie)\nRiga #: nomi serie",
                    /* 23 Funnel */ "Formato: etichetta:valore  (dalla voce pi\xc3\xb9 grande alla pi\xc3\xb9 piccola)",
                    /* 24 Donut  */ "Formato: etichetta:valore  (una voce per riga)",
                    /* 25 Treemap*/ "Formato: etichetta:valore  (una voce per riga)",
                    /* 26 Sunburst*/ "Formato: Categoria/Sottocategoria:valore\n"
                                    "  Frutta/Mele:40\n  Frutta/Pere:25\n  Verdura/Carote:35",
                    /* 27 BoxPlot*/ "Formato: etichetta: min, Q1, mediana, Q3, max\n"
                                    "  Gruppo A: 10, 20, 30, 40, 60",
                    /* 28 Dot    */ "Formato: etichetta:valore  (una voce per riga)",
                    /* 29 Density*/ "Valori numerici separati da spazi o virgole\n"
                                    "(dati grezzi — la curva KDE viene calcolata automaticamente)",
                    /* 30 AStk   */ "Formato: x, y1, y2, ... (colonne = serie)\nRiga #: nomi serie",
                    /* 31 OHLC   */ "Formato: Etichetta, Open, High, Low, Close\n"
                                    "  Gen, 100, 115, 98, 112",
                    /* 32 Gauge  */ "Riga 1: valore:massimo  (es. 72:100)\nRiga 2: etichetta (opzionale)",
                    /* 33 Bullet */ "Formato: etichetta: valore, target, minRange, maxRange\n"
                                    "  Fatturato: 85, 90, 0, 120",
                    /* 34 Gantt  */ "Formato: task: inizio, fine [, categoria]\n"
                                    "  Analisi: 0, 2, Fase1\n  Design: 1, 4, Fase1",
                    /* 35 Pyramid*/ "Formato: x, sinistra, destra  (colonne = due serie)\n"
                                    "# Maschi  Femmine\n0, 120, 115\n1, 140, 138",
                    /* 36 Parallel*/ "Formato: riga per entit\xc3\xa0, colonne = assi\n"
                                     "Riga #: nomi degli assi\n"
                                     "  # Velocita Forza Resistenza Agilita\n"
                                     "  85, 70, 60, 90",
                    /* 37 Sankey */ "Formato: Sorgente \xe2\x86\x92 Destinazione: valore\n"
                                    "  Energia -> Elettricita: 40\n  Energia -> Calore: 30",
                    /* 38 Tree   */ "Formato: padre: figlio1, figlio2\n"
                                    "  Root: A, B\n  A: C, D\n  B: E",
                    /* 39 Chord  */ "Formato: A \xe2\x86\x92 B: valore\n"
                                    "  Italia -> Francia: 15\n  Francia -> Germania: 20",
                };
                static const char* kExamples2[] = {
                    /* 18 Column */
                    "Gen:82\nFeb:67\nMar:91\nApr:78\nMag:95\nGiu:88",
                    /* 19 HBar */
                    "Python:85\nC++:72\nRust:68\nGo:61\nJava:55\nSwift:48",
                    /* 20 Grouped */
                    "# Nord Sud Est\n0, 42, 28, 19\n1, 55, 33, 22\n2, 48, 30, 25\n3, 60, 38, 28",
                    /* 21 Stacked */
                    "# Carbone Petrolio Gas Rinnovabili\n2019, 28, 33, 22, 17\n2020, 25, 30, 23, 22\n2021, 26, 31, 22, 21\n2022, 24, 29, 21, 26",
                    /* 22 Stacked 100% */
                    "# Desktop Mobile Tablet\n2020, 55, 35, 10\n2021, 50, 40, 10\n2022, 46, 44, 10\n2023, 43, 47, 10",
                    /* 23 Funnel */
                    "Visite:10000\nIscritti:4200\nContatti:1800\nOfferte:750\nClienti:320",
                    /* 24 Donut */
                    "Python:38\nJavaScript:26\nTypeScript:14\nJava:11\nC++:7\nAltri:4",
                    /* 25 Treemap */
                    "Europa:420\nAsia:580\nAmerica:390\nAfrica:180\nOceania:95",
                    /* 26 Sunburst */
                    "Frutta/Mele:40\nFrutta/Pere:25\nFrutta/Arance:20\n"
                    "Verdura/Carote:35\nVerdura/Spinaci:28\nVerdura/Pomodori:32\n"
                    "Cereali/Riso:55\nCereali/Frumento:45",
                    /* 27 BoxPlot */
                    "Gruppo A: 10, 22, 35, 48, 65\n"
                    "Gruppo B: 8, 18, 28, 42, 58\n"
                    "Gruppo C: 15, 28, 40, 55, 72\n"
                    "Gruppo D: 5, 15, 25, 38, 52",
                    /* 28 DotPlot */
                    "Lun:8.2\nMar:7.5\nMer:9.1\nGio:6.8\nVen:8.8\nSab:5.2\nDom:4.5",
                    /* 29 Density */
                    "2.1 3.4 2.8 4.1 3.7 2.5 3.9 4.5 3.2 2.7 "
                    "4.8 3.1 2.9 3.6 4.2 3.8 2.3 3.5 4.0 3.3 "
                    "2.6 4.4 3.0 2.4 3.8 4.1 2.8 3.7 4.3 3.2",
                    /* 30 AreaStacked */
                    "# Vendite Costi Profitto\n0, 120, 80, 40\n1, 145, 90, 55\n"
                    "2, 160, 95, 65\n3, 175, 100, 75\n4, 155, 105, 50\n5, 190, 110, 80",
                    /* 31 OHLC */
                    "Gen, 100, 115, 98, 112\nFeb, 112, 120, 108, 105\n"
                    "Mar, 105, 125, 102, 118\nApr, 118, 130, 115, 125\n"
                    "Mag, 125, 128, 110, 108\nGiu, 108, 122, 105, 118",
                    /* 32 Gauge */
                    "72:100\nPunteggio qualit\xc3\xa0",
                    /* 33 Bullet */
                    "Fatturato: 85, 90, 60, 120\n"
                    "Profitto: 42, 50, 30, 70\n"
                    "NPS: 68, 75, 40, 100",
                    /* 34 Gantt */
                    "Analisi: 0, 2, Fase 1\n"
                    "Design: 1, 4, Fase 1\n"
                    "Sviluppo: 3, 8, Fase 2\n"
                    "Testing: 7, 10, Fase 2\n"
                    "Deploy: 9, 11, Fase 3",
                    /* 35 Pyramid */
                    "# Maschi Femmine\n0, 850, 820\n1, 920, 900\n"
                    "2, 980, 960\n3, 870, 850\n4, 720, 710\n5, 540, 560",
                    /* 36 ParallelCoord */
                    "# Velocita Forza Resistenza Agilita Precisione\n"
                    "85, 70, 60, 90, 78\n"
                    "62, 88, 75, 55, 82\n"
                    "90, 55, 80, 72, 65\n"
                    "70, 80, 85, 68, 90",
                    /* 37 Sankey */
                    "Carbone -> Elettricita: 40\n"
                    "Gas -> Elettricita: 25\n"
                    "Elettricita -> Industria: 35\n"
                    "Elettricita -> Residenziale: 20\n"
                    "Gas -> Riscaldamento: 30\n"
                    "Rinnovabili -> Elettricita: 18",
                    /* 38 Tree */
                    "Azienda: HR, Tech, Vendite\n"
                    "Tech: Backend, Frontend, DevOps\n"
                    "Backend: API, Database\n"
                    "Vendite: Italia, Estero",
                    /* 39 Chord */
                    "Italia -> Francia: 15\n"
                    "Italia -> Germania: 22\n"
                    "Francia -> Germania: 18\n"
                    "Germania -> Spagna: 12\n"
                    "Spagna -> Italia: 20\n"
                    "Francia -> Spagna: 14",
                };
                m_dataHint->setText(kHints2[idx - 18]);
                if (kExamples2[idx - 18][0] != '\0')
                    m_dataEdit->setPlainText(kExamples2[idx - 18]);
            }

            if (idx >= 40 && idx <= 44) {
                m_dataHint->setText(kNewHints[idx - 40]);
                if (kNewExamples[idx-40][0] != '\0')
                    m_dataEdit->setPlainText(kNewExamples[idx-40]);
            }
            /* Generatore da espressione: nascosto per tipi complessi */
            const bool hasFormula = (idx != 4 && idx != 6 &&
                                     idx != 13 && idx != 14 && idx != 16 &&
                                     idx < 18 && idx < 40);
            m_genSection->setVisible(hasFormula);
        }
    });

    /* Pulsanti */
    auto* btnPlot = new QPushButton("\xf0\x9f\x93\x88  Traccia", panel);
    btnPlot->setObjectName("primaryBtn");
    btnPlot->setFixedHeight(34);
    connect(btnPlot, &QPushButton::clicked, this, &GraficoPage::plot);
    lay->addWidget(btnPlot);

    auto* btnReset = new QPushButton("\xf0\x9f\x94\x84  Reset vista", panel);
    btnReset->setObjectName("actionBtn");
    btnReset->setFixedHeight(28);
    connect(btnReset, &QPushButton::clicked, this, [this]{ m_canvas->resetView(); });
    lay->addWidget(btnReset);

    /* ── Sezione immagine → formula (solo Cartesiano) ─────────── */
    auto* imgSep = new QFrame(panel);
    imgSep->setFrameShape(QFrame::HLine);
    imgSep->setObjectName("separator");
    lay->addWidget(imgSep);

    m_imgSection = new QWidget(panel);
    auto* imgLay = new QVBoxLayout(m_imgSection);
    imgLay->setContentsMargins(0, 2, 0, 2);
    imgLay->setSpacing(4);

    auto* imgTitleLbl = new QLabel(
        "\xf0\x9f\x94\x8d  Grafico \xe2\x86\x92 Formula  [vision AI]", m_imgSection);
    imgTitleLbl->setObjectName("formLabel");
    imgLay->addWidget(imgTitleLbl);

    auto* imgHintLbl = new QLabel(
        "Carica un grafico cartesiano.\n"
        "Il modello vision estrae la formula y = f(x).", m_imgSection);
    imgHintLbl->setObjectName("hintLabel");
    imgHintLbl->setWordWrap(true);
    imgHintLbl->setFont(QFont("Inter,Ubuntu,sans-serif", 8));
    imgLay->addWidget(imgHintLbl);

    m_visionCombo = new QComboBox(m_imgSection);
    m_visionCombo->setObjectName("settingCombo");
    m_visionCombo->setToolTip(
        "Modello vision da usare (solo quelli con supporto immagini).\n"
        "Es: llama3.2-vision, llava, minicpm-v, moondream");
    m_visionCombo->setPlaceholderText("Seleziona modello vision...");
    imgLay->addWidget(m_visionCombo);

    /* Pulsante di installazione — visibile SOLO se non ci sono modelli vision */
    auto* btnInstallVision = new QPushButton(
        "\xf0\x9f\x93\xa5  Clicca qui per scaricare e installare un modello vision da internet",
        m_imgSection);
    btnInstallVision->setObjectName("actionBtn");
    btnInstallVision->setFixedHeight(36);
    btnInstallVision->setToolTip(
        "Apre Impostazioni \xe2\x86\x92 LLM Consigliati \xe2\x86\x92 sezione Vision\n"
        "per scaricare un modello adatto all'analisi di immagini e grafici.\n"
        "Consigliati: llama3.2-vision, qwen2-vl:7b, minicpm-v:8b");
    btnInstallVision->setStyleSheet(
        "QPushButton { background:#1e3a5f; border:1px dashed #38bdf8;"
        "border-radius:6px; color:#93c5fd; padding:4px 8px; text-align:left; }"
        "QPushButton:hover { background:#1e4a7f; border-color:#60a5fa; }");
    btnInstallVision->setVisible(false);
    connect(btnInstallVision, &QPushButton::clicked, this,
            [this]{ emit requestOpenSettings("LLM"); });
    imgLay->addWidget(btnInstallVision);

    m_btnImgFormula = new QPushButton(
        "\xf0\x9f\x93\xb7  Analizza immagine", m_imgSection);
    m_btnImgFormula->setObjectName("actionBtn");
    m_btnImgFormula->setFixedHeight(30);
    m_btnImgFormula->setToolTip("Apri un'immagine PNG/JPG e lascia che l'AI estragga la formula");
    connect(m_btnImgFormula, &QPushButton::clicked, this, &GraficoPage::analyzeImage);
    imgLay->addWidget(m_btnImgFormula);

    lay->addWidget(m_imgSection);

    /* Popola la combo vision la prima volta che i modelli sono disponibili */
    if (m_ai) {
        auto populateVision = [this, btnInstallVision]() {
            const QStringList all = m_ai->models();
            QStringList vision;
            for (const QString& m : all) {
                const QString ml = m.toLower();
                if (ml.contains("vision") || ml.contains("llava")  ||
                    ml.contains("minicpm") || ml.contains("moondream") ||
                    ml.contains("bakllava") || ml.contains("gemma3"))
                    vision << m;
            }
            m_visionCombo->clear();
            if (vision.isEmpty()) {
                /* Nessun modello vision: nascondi combo e mostra pulsante installazione */
                m_visionCombo->setVisible(false);
                btnInstallVision->setVisible(true);
                m_btnImgFormula->setEnabled(false);
            } else {
                m_visionCombo->setVisible(true);
                btnInstallVision->setVisible(false);
                m_visionCombo->addItems(vision);
                m_btnImgFormula->setEnabled(true);
            }
        };
        /* Popola subito se i modelli ci sono già, oppure alla prima ricezione */
        if (!m_ai->models().isEmpty())
            populateVision();
        connect(m_ai, &AiClient::modelsReady, m_imgSection,
                [populateVision](const QStringList&){ populateVision(); });
    }

    /* Visibilità: solo quando tipo = Cartesiano */
    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            m_imgSection, [imgSep, this](int idx){
        const bool cart = (idx == 0);   /* sezione vision solo per Cartesiano */
        imgSep->setVisible(cart);
        m_imgSection->setVisible(cart);
    });

    /* ── Sincronizzazione tab "2D" / "3D" ──
       I tipi 3D sono agli indici 5 (Scatter3D) e 6 (Grafo3D) nel combo.
       - Cliccare "3D" → seleziona Scatter3D nel combo
       - Cliccare "2D" → mostra solo tipi 2D nel combo (ripristina tipo compatibile)
       - Cliccare "3D" → mostra solo tipi 3D nel combo */
    connect(m_dimBar, &QTabBar::currentChanged, this, [this](int tab){
        populateTypeCombo(tab);
    });

    m_statusLbl = new QLabel("", panel);
    m_statusLbl->setObjectName("statusLabel");
    m_statusLbl->setWordWrap(true);
    m_statusLbl->setFont(QFont("Inter,Ubuntu,sans-serif", 8));
    lay->addWidget(m_statusLbl);

    return panel;
}

/* ── populateTypeCombo ───────────────────────────────────────── */
void GraficoPage::populateTypeCombo(int tab) {
    struct ChartItem { const char* text; int typeId; bool is3d; };
    static const ChartItem kItems[] = {
        { "\xf0\x9f\x93\x88  Cartesiano  y = f(x)",                           0,  false },
        { "\xf0\x9f\xa5\xa7  Torta",                                           1,  false },
        { "\xf0\x9f\x93\x8a  Istogramma",                                      2,  false },
        { "\xf0\x9f\x94\xb9  Scatter 2D  (x, y)",                             3,  false },
        { "\xf0\x9f\x95\xb8  Grafo 2D",                                        4,  false },
        { "\xf0\x9f\x8c\x90  Scatter 3D",                                      5,  true  },
        { "\xf0\x9f\x94\xb7  Grafo 3D",                                        6,  true  },
        { "\xf0\x9f\x94\xb5  Smith \xe2\x80\x94 Numeri Primi",                 7,  false },
        { "\xf0\x9f\x93\x90  Smith \xe2\x80\x94 \xcf\x80\xc2\xb7""e\xc2\xb7Primi", 8, false },
        { "\xf0\x9f\x93\x89  Linea multi-serie",                               9,  false },
        { "\xf0\x9f\x8c\x80  Polare  r = f(\xce\xb8)",                        10,  false },
        { "\xf0\x9f\x95\xb7  Radar (Spider)",                                  11, false },
        { "\xf0\x9f\xab\xa7  Bolle  (x, y, raggio)",                           12, false },
        { "\xf0\x9f\x94\xa5  Heatmap (matrice colori)",                         13, false },
        { "\xf0\x9f\x95\xaf  Candele OHLC",                                    14, false },
        { "\xf0\x9f\x8f\x94  Area riempita",                                   15, false },
        { "\xf0\x9f\x8c\x8a  Cascata (Waterfall)",                             16, false },
        { "\xf0\x9f\x93\xb6  Scalini (Step)",                                  17, false },
        { "\xf0\x9f\x8f\x9b  Colonne (Column)",                                18, false },
        { "\xe2\x96\xac  Barre orizzontali (HBar)",                             19, false },
        { "\xf0\x9f\x97\x82  Barre raggruppate (Grouped)",                     20, false },
        { "\xf0\x9f\x93\x9a  Barre impilate (Stacked)",                        21, false },
        { "\xf0\x9f\x92\xaf  Barre impilate 100%",                             22, false },
        { "\xf0\x9f\xaa\xa3  Imbuto (Funnel)",                                 23, false },
        { "\xf0\x9f\x8d\xa9  Ciambella (Donut)",                               24, false },
        { "\xf0\x9f\x97\xba  Treemap",                                         25, false },
        { "\xe2\x98\x80  Sunburst",                                             26, false },
        { "\xf0\x9f\x93\xa6  Box Plot",                                        27, false },
        { "\xf0\x9f\x94\xb5  Dot Plot",                                        28, false },
        { "\xf0\x9f\x8c\x8a  Densit\xc3\xa0 (KDE)",                           29, false },
        { "\xf0\x9f\x8f\x94  Area impilata (Stacked Area)",                    30, false },
        { "\xf0\x9f\x93\x8a  OHLC (barre)",                                    31, false },
        { "\xf0\x9f\x8e\xaf  Gauge (semicerchio)",                             32, false },
        { "\xf0\x9f\x8e\xaf  Bullet Chart",                                    33, false },
        { "\xf0\x9f\x93\x85  Gantt",                                           34, false },
        { "\xf0\x9f\x94\xba  Piramide (Pyramid)",                              35, false },
        { "\xe2\x87\x94  Coordinate parallele",                                 36, false },
        { "\xf0\x9f\x8c\x8a  Sankey",                                          37, false },
        { "\xf0\x9f\x8c\xb3  Albero (Tree)",                                   38, false },
        { "\xf0\x9f\x94\x97  Chord",                                           39, false },
        { "\xf0\x9f\x8e\xbb  Violin Plot",                                     40, false },
        { "\xe2\x98\x81  Word Cloud",                                           41, false },
        { "\xf0\x9f\x8c\x9f  Albero Radiale",                                  42, false },
        { "\xe2\x96\xb6  Linea Animata",                                        43, false },
        { "\xe2\x8a\x9e  Small Multiples",                                      44, false },
    };

    const int prevType = m_typeCombo->count() > 0
                       ? m_typeCombo->currentData().toInt() : 0;

    m_typeCombo->blockSignals(true);
    m_typeCombo->clear();
    for (const auto& item : kItems)
        if (item.is3d == (tab == 1))
            m_typeCombo->addItem(QString::fromUtf8(item.text), item.typeId);
    /* Ripristina il tipo precedente se compatibile, altrimenti primo della lista */
    int idx = m_typeCombo->findData(prevType);
    if (idx < 0) idx = 0;
    m_typeCombo->blockSignals(false);
    m_typeCombo->setCurrentIndex(idx);  /* fires currentIndexChanged */
}

/* ── plot() ──────────────────────────────────────────────────── */
void GraficoPage::plot() {
    int type = m_typeCombo->currentData().toInt();
    m_statusLbl->clear();

    switch (type) {
        case 0: {   /* Cartesiano */
            QString f = m_formulaEdit->text().trimmed();
            if (f.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Inserisci una formula"); return; }
            double xMin = m_xMinSpin->value(), xMax = m_xMaxSpin->value();
            if (xMin >= xMax)  { m_statusLbl->setText("\xe2\x9a\xa0  x min deve essere < x max"); return; }
            m_canvas->setCartesian(f, xMin, xMax);
            FormulaParser fp(f);
            if (!fp.ok()) m_statusLbl->setText("\xe2\x9a\xa0  " + fp.err());
            else          m_statusLbl->setText("\xe2\x9c\x85  Grafico aggiornato");
            break;
        }
        case 1:
        case 2: {   /* Torta / Istogramma */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<double> vals; QStringList lbls;
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                QString l = line.trimmed(); if (l.isEmpty()) continue;
                QChar sep = l.contains(':') ? ':' : ',';
                int pos = l.indexOf(sep);
                if (pos > 0) {
                    bool ok; double v = l.mid(pos+1).trimmed().toDouble(&ok);
                    if (ok) { lbls.append(l.left(pos).trimmed()); vals.append(v); }
                } else {
                    bool ok; double v = l.toDouble(&ok);
                    if (ok) { lbls.append(QString::number(vals.size()+1)); vals.append(v); }
                }
            }
            if (vals.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun valore valido"); return; }
            m_canvas->setData(vals, lbls);
            m_canvas->setType(type == 1 ? GraficoCanvas::Pie : GraficoCanvas::Histogram);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(vals.size()) + " voci");
            break;
        }
        case 3: {   /* Scatter XY */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<QPointF> pts; QStringList lbls;
            static QRegularExpression sep("[,;\\s]+");
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                /* formato: x, y [, etichetta]
                   L'etichetta può essere un testo qualsiasi (anche con spazi) */
                QString trimmed = line.trimmed();
                QStringList p   = trimmed.split(sep, Qt::SkipEmptyParts);
                if (p.size() >= 2) {
                    bool ox, oy;
                    double x = p[0].toDouble(&ox), y = p[1].toDouble(&oy);
                    if (ox && oy) {
                        pts.append({x, y});
                        /* la terza colonna (e successive) è l'etichetta */
                        lbls.append(p.size() > 2 ? p.mid(2).join(" ") : "");
                    }
                }
            }
            if (pts.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessuna coppia x,y valida"); return; }
            m_canvas->setScatter(pts);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(pts.size()) + " punti");
            break;
        }
        case 4: {   /* Grafo */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun arco"); return; }
            QVector<QPair<QString,QString>> edges;
            static QRegularExpression edgeSep("\\s*[-\xe2\x80\x93>]+\\s*");
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                QStringList p = line.trimmed().split(edgeSep, Qt::SkipEmptyParts);
                if (p.size() >= 2) edges.append({p[0].trimmed(), p[1].trimmed()});
            }
            if (edges.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun arco valido (A-B)"); return; }
            m_canvas->setEdges(edges);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(edges.size()) + " archi");
            break;
        }
        case 5: {   /* 3D Scatter */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<GraficoCanvas::Pt3D> pts;
            static QRegularExpression sep3("[,;\\s]+");
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                /* formato: x, y, z [, etichetta] */
                QStringList p = line.trimmed().split(sep3, Qt::SkipEmptyParts);
                if (p.size() >= 3) {
                    bool ox, oy, oz;
                    double x = p[0].toDouble(&ox), y = p[1].toDouble(&oy), z = p[2].toDouble(&oz);
                    if (ox && oy && oz) {
                        QString lbl = p.size() > 3 ? p.mid(3).join(" ") : "";
                        pts.append({x, y, z, lbl});
                    }
                }
            }
            if (pts.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessuna tripla x,y,z valida"); return; }
            m_canvas->setScatter3D(pts);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(pts.size()) + " punti 3D");
            break;
        }
        case 7: {   /* Smith Prime */
            m_canvas->setSmithPrime(
                m_smithMaxN->value(),
                m_smithReal->isChecked(),
                m_smithGauss->isChecked(),
                m_smithExp->isChecked(),
                m_smithNorm->value(),
                m_smithFib->isChecked(),
                m_smithTri->isChecked(),
                m_smithSqr->isChecked(),
                m_smithLabels->isChecked()
            );
            /* ── Serie personalizzata: parsa i numeri dal text edit ── */
            {
                QString raw = m_smithCustom->toPlainText();
                if (!raw.trimmed().isEmpty()) {
                    QVector<int> customVals;
                    static QRegularExpression sepC("[,;\\s]+");
                    for (const QString& tok : raw.split(sepC, Qt::SkipEmptyParts)) {
                        bool ok;
                        int v = tok.toInt(&ok);
                        if (ok && v >= 2) customVals.append(v);
                    }
                    if (!customVals.isEmpty())
                        m_canvas->addSmithCustom(customVals);
                }
            }
            int nPts = m_canvas->smithPointCount();
            m_statusLbl->setText(
                QString("\xe2\x9c\x85  %1 punti nel disco di Smith").arg(nPts));
            break;
        }
        case 8: {   /* Math Const — π · e · Primi */
            if (!m_mathPi->isChecked() && !m_mathE->isChecked() && !m_mathPrimes->isChecked()) {
                m_statusLbl->setText("\xe2\x9a\xa0  Seleziona almeno una serie");
                return;
            }
            m_canvas->setMathConst(
                m_mathTerms ->value(),
                m_mathPi    ->isChecked(),
                m_mathE     ->isChecked(),
                m_mathPrimes->isChecked()
            );
            int nPts = m_canvas->mathPointCount();
            m_statusLbl->setText(
                QString("\xe2\x9c\x85  %1 punti  |  "
                        "\xce\x93(\xcf\x80)\xe2\x89\x88" "0.517  \xce\x93(e)\xe2\x89\x88" "0.462").arg(nPts));
            break;
        }
        case 6: {   /* Grafo 3D — righe "Nome, x, y, z" + "A-B" mescolate */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<GraficoCanvas::Node3D>          nodes;
            QVector<QPair<QString,QString>>          edges;
            static QRegularExpression sepN("[,;\\s]+");
            static QRegularExpression sepE("\\s*[-\xe2\x80\x93>]+\\s*");
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                QString l = line.trimmed();
                if (l.startsWith('#')) continue;           /* commento */
                QStringList tok = l.split(sepN, Qt::SkipEmptyParts);
                /* nodo: almeno 4 token, token[1..3] sono numeri */
                if (tok.size() >= 4) {
                    bool ox, oy, oz;
                    double x = tok[1].toDouble(&ox);
                    double y = tok[2].toDouble(&oy);
                    double z = tok[3].toDouble(&oz);
                    if (ox && oy && oz) {
                        nodes.append({tok[0], x, y, z});
                        continue;
                    }
                }
                /* arco: A-B */
                QStringList parts = l.split(sepE, Qt::SkipEmptyParts);
                if (parts.size() >= 2)
                    edges.append({parts[0].trimmed(), parts[1].trimmed()});
            }
            if (nodes.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun nodo (formato: Nome, x, y, z)"); return; }
            m_canvas->setGraph3D(nodes, edges);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(nodes.size()) +
                                  " nodi, " + QString::number(edges.size()) + " archi");
            break;
        }
        case 9:   /* Linea multi-serie */
        case 15:  /* Area riempita  */
        case 17: { /* Scalini (Step) */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<QVector<QPointF>> series;
            QStringList names;
            static QRegularExpression sepMS("[,;\\s]+");
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                QString l = line.trimmed();
                if (l.startsWith('#')) {
                    /* riga header: # NomeSerie1 NomeSerie2 ... */
                    names = l.mid(1).trimmed().split(sepMS, Qt::SkipEmptyParts);
                    continue;
                }
                QStringList tok = l.split(sepMS, Qt::SkipEmptyParts);
                if (tok.size() < 2) continue;
                bool ox; double x = tok[0].toDouble(&ox);
                if (!ox) continue;
                /* assicura che ci siano abbastanza serie */
                while (series.size() < tok.size() - 1) series.append(QVector<QPointF>());
                for (int k = 1; k < tok.size(); ++k) {
                    bool ok; double y = tok[k].toDouble(&ok);
                    if (ok) series[k-1].append({x, y});
                }
            }
            if (series.isEmpty()) {
                m_statusLbl->setText("\xe2\x9a\xa0  Nessuna colonna x,y valida");
                return;
            }
            m_canvas->setLine(series, names);
            if (type == 15) m_canvas->setType(GraficoCanvas::Area);
            else if (type == 17) m_canvas->setType(GraficoCanvas::Step);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(series.size()) + " serie, " +
                                  QString::number(series[0].size()) + " punti");
            break;
        }
        case 10: { /* Polare r = f(θ) */
            if (!m_polarFormula) { m_statusLbl->setText("\xe2\x9a\xa0  Pannello non pronto"); return; }
            QString f = m_polarFormula->text().trimmed();
            if (f.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Inserisci r = f(\xce\xb8)"); return; }
            double tMin = m_polarTMinSpin->value();
            double tMax = m_polarTMaxSpin->value();
            if (tMin >= tMax) { m_statusLbl->setText("\xe2\x9a\xa0  \xce\xb8 min deve essere < \xce\xb8 max"); return; }
            m_canvas->setPolar(f, tMin, tMax);
            m_statusLbl->setText("\xe2\x9c\x85  Grafico polare aggiornato");
            break;
        }
        case 11: { /* Radar (Spider) — riusa parsing Torta/Istogramma */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<double> vals; QStringList lbls;
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                QString l = line.trimmed(); if (l.isEmpty()) continue;
                QChar sep = l.contains(':') ? ':' : ',';
                int pos = l.indexOf(sep);
                if (pos > 0) {
                    bool ok; double v = l.mid(pos+1).trimmed().toDouble(&ok);
                    if (ok) { lbls.append(l.left(pos).trimmed()); vals.append(v); }
                } else {
                    bool ok; double v = l.toDouble(&ok);
                    if (ok) { lbls.append(QString::number(vals.size()+1)); vals.append(v); }
                }
            }
            if (vals.size() < 3) {
                m_statusLbl->setText("\xe2\x9a\xa0  Servono almeno 3 categorie");
                return;
            }
            m_canvas->setData(vals, lbls);
            m_canvas->setType(GraficoCanvas::Radar);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(vals.size()) + " assi");
            break;
        }
        case 12: { /* Bolle: x, y, raggio [, etichetta] */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<GraficoCanvas::Pt3D> pts;
            static QRegularExpression sepB("[,;\\s]+");
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                QStringList tok = line.trimmed().split(sepB, Qt::SkipEmptyParts);
                if (tok.size() < 3) continue;
                bool ox, oy, oz;
                double x = tok[0].toDouble(&ox);
                double y = tok[1].toDouble(&oy);
                double r = tok[2].toDouble(&oz);
                if (ox && oy && oz)
                    pts.append({x, y, r, tok.size() > 3 ? tok.mid(3).join(" ") : ""});
            }
            if (pts.isEmpty()) {
                m_statusLbl->setText("\xe2\x9a\xa0  Nessuna terna x,y,raggio valida");
                return;
            }
            m_canvas->setScatter3D(pts);
            m_canvas->setType(GraficoCanvas::Bubble);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(pts.size()) + " bolle");
            break;
        }
        case 13: { /* Heatmap */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<QVector<double>> data;
            QStringList rowLbls, colLbls;
            static QRegularExpression sepH("[,;\\s]+");
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                QString l = line.trimmed();
                if (l.startsWith('#')) {
                    colLbls = l.mid(1).trimmed().split(sepH, Qt::SkipEmptyParts);
                    continue;
                }
                /* supporto opzionale "etichetta: v1 v2 v3" */
                QStringList tok;
                int colon = l.indexOf(':');
                if (colon > 0 && colon < 20) {
                    rowLbls.append(l.left(colon).trimmed());
                    tok = l.mid(colon+1).trimmed().split(sepH, Qt::SkipEmptyParts);
                } else {
                    tok = l.split(sepH, Qt::SkipEmptyParts);
                }
                QVector<double> row;
                for (auto& t : tok) { bool ok; double v = t.toDouble(&ok); if (ok) row.append(v); }
                if (!row.isEmpty()) data.append(row);
            }
            if (data.isEmpty()) {
                m_statusLbl->setText("\xe2\x9a\xa0  Nessun valore numerico trovato");
                return;
            }
            m_canvas->setHeatmap(data, rowLbls, colLbls);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(data.size()) + " \xc3\x97 " +
                                  QString::number(data[0].size()) + " matrice");
            break;
        }
        case 14: { /* Candlestick OHLC */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<GraficoCanvas::OhlcPt> pts;
            static QRegularExpression sepC("[,;]+");
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                QStringList tok = line.trimmed().split(sepC);
                if (tok.size() < 5) continue;
                bool oo, oh, ol, oc;
                double o = tok[1].trimmed().toDouble(&oo);
                double h = tok[2].trimmed().toDouble(&oh);
                double l = tok[3].trimmed().toDouble(&ol);
                double c = tok[4].trimmed().toDouble(&oc);
                if (oo && oh && ol && oc)
                    pts.append({o, h, l, c, tok[0].trimmed()});
            }
            if (pts.isEmpty()) {
                m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato OHLC valido (formato: Label,O,H,L,C)");
                return;
            }
            m_canvas->setCandlestick(pts);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(pts.size()) + " candele");
            break;
        }
        case 16: { /* Waterfall */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<double> vals; QStringList lbls;
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                QString l = line.trimmed(); if (l.isEmpty()) continue;
                QChar sep = l.contains(':') ? ':' : ',';
                int pos = l.indexOf(sep);
                if (pos > 0) {
                    /* rimuovi eventuale '+' dal valore */
                    QString vStr = l.mid(pos+1).trimmed();
                    if (vStr.startsWith('+')) vStr = vStr.mid(1);
                    bool ok; double v = vStr.toDouble(&ok);
                    if (ok) { lbls.append(l.left(pos).trimmed()); vals.append(v); }
                } else {
                    bool ok; double v = l.toDouble(&ok);
                    if (ok) { lbls.append(QString::number(vals.size()+1)); vals.append(v); }
                }
            }
            if (vals.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun valore valido"); return; }
            m_canvas->setData(vals, lbls);
            m_canvas->setType(GraficoCanvas::Waterfall);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(vals.size()) + " voci");
            break;
        }
        /* ── Column / HBar / Funnel / Donut / Treemap / DotPlot / Gauge ── */
        case 18: /* Column    */
        case 19: /* HBar      */
        case 23: /* Funnel    */
        case 24: /* Donut     */
        case 25: /* Treemap   */
        case 28: /* DotPlot   */
        {
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<double> vals; QStringList lbls;
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                QString l = line.trimmed(); if (l.startsWith('#') || l.isEmpty()) continue;
                QChar sep = l.contains(':') ? ':' : ',';
                int pos = l.indexOf(sep);
                if (pos > 0) {
                    bool ok; double v = l.mid(pos+1).trimmed().toDouble(&ok);
                    if (ok) { lbls.append(l.left(pos).trimmed()); vals.append(v); }
                } else {
                    bool ok; double v = l.toDouble(&ok);
                    if (ok) { lbls.append(QString::number(vals.size()+1)); vals.append(v); }
                }
            }
            if (vals.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun valore valido"); return; }
            m_canvas->setData(vals, lbls);
            static const GraficoCanvas::ChartType kT[] = {
                GraficoCanvas::Column, GraficoCanvas::HBar,
                GraficoCanvas::Funnel, GraficoCanvas::Donut,
                GraficoCanvas::Treemap, GraficoCanvas::DotPlot
            };
            const int kMap[] = {18, 19, 23, 24, 25, 28};
            GraficoCanvas::ChartType ct = GraficoCanvas::Column;
            for (int i = 0; i < 6; ++i) if (kMap[i] == type) { ct = kT[i]; break; }
            m_canvas->setType(ct);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(vals.size()) + " voci");
            break;
        }
        /* ── GroupedBar / StackedBar / StackedBar100 / Pyramid / AreaStacked ── */
        case 20: /* GroupedBar    */
        case 21: /* StackedBar    */
        case 22: /* StackedBar100 */
        case 30: /* AreaStacked   */
        case 35: /* Pyramid       */
        {
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<QVector<QPointF>> series;
            QStringList names;
            static QRegularExpression sepGr("[,;\\s]+");
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                QString l = line.trimmed();
                if (l.startsWith('#')) {
                    names = l.mid(1).trimmed().split(sepGr, Qt::SkipEmptyParts);
                    continue;
                }
                QStringList tok = l.split(sepGr, Qt::SkipEmptyParts);
                if (tok.size() < 2) continue;
                bool ox; double x = tok[0].toDouble(&ox);
                if (!ox) {
                    /* etichetta testuale: usa indice */
                    x = series.isEmpty() ? 0 : (series[0].isEmpty() ? 0 : series[0].size());
                    /* riprova dal primo token come label, skip */
                }
                while (series.size() < tok.size() - 1) series.append(QVector<QPointF>());
                for (int k = 1; k < tok.size(); ++k) {
                    bool ok; double y = tok[k].toDouble(&ok);
                    if (ok) series[k-1].append({x, y});
                }
            }
            if (series.isEmpty()) {
                m_statusLbl->setText("\xe2\x9a\xa0  Nessuna serie valida");
                return;
            }
            m_canvas->setLine(series, names);
            static const int kGrMap[]  = {20, 21, 22, 30, 35};
            static const GraficoCanvas::ChartType kGrT[] = {
                GraficoCanvas::GroupedBar, GraficoCanvas::StackedBar,
                GraficoCanvas::StackedBar100, GraficoCanvas::AreaStacked,
                GraficoCanvas::Pyramid
            };
            GraficoCanvas::ChartType ct = GraficoCanvas::GroupedBar;
            for (int i = 0; i < 5; ++i) if (kGrMap[i] == type) { ct = kGrT[i]; break; }
            m_canvas->setType(ct);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(series.size()) + " serie");
            break;
        }
        case 26: { /* Sunburst — Categoria/Subcategoria: valore */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<QPair<QString,double>> data;
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                QString l = line.trimmed();
                if (l.startsWith('#') || l.isEmpty()) continue;
                int pos = l.lastIndexOf(':');
                if (pos > 0) {
                    bool ok; double v = l.mid(pos+1).trimmed().toDouble(&ok);
                    if (ok) data.append({l.left(pos).trimmed(), v});
                }
            }
            if (data.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato valido"); return; }
            m_canvas->setSunburstData(data);
            m_canvas->setType(GraficoCanvas::Sunburst);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(data.size()) + " voci");
            break;
        }
        case 27: { /* BoxPlot */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<GraficoCanvas::BoxData> data;
            static QRegularExpression sepBx("[,;\\s]+");
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                QString l = line.trimmed();
                if (l.startsWith('#') || l.isEmpty()) continue;
                QString lbl; QString valPart = l;
                int colon = l.indexOf(':');
                if (colon > 0) { lbl = l.left(colon).trimmed(); valPart = l.mid(colon+1); }
                QStringList tok = valPart.split(sepBx, Qt::SkipEmptyParts);
                if (tok.size() < 5) continue;
                bool o1,o2,o3,o4,o5;
                double mn = tok[0].toDouble(&o1), q1 = tok[1].toDouble(&o2),
                       md = tok[2].toDouble(&o3), q3 = tok[3].toDouble(&o4),
                       mx = tok[4].toDouble(&o5);
                if (o1&&o2&&o3&&o4&&o5)
                    data.append({mn, q1, md, q3, mx, lbl.isEmpty() ? QString::number(data.size()+1) : lbl});
            }
            if (data.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Formato: label: min,Q1,med,Q3,max"); return; }
            m_canvas->setBoxData(data);
            m_canvas->setType(GraficoCanvas::BoxPlot);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(data.size()) + " box");
            break;
        }
        case 29: { /* Density — valori grezzi */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<double> data;
            static QRegularExpression sepDs("[,;\\s\n]+");
            for (auto& tok : raw.split(sepDs, Qt::SkipEmptyParts)) {
                bool ok; double v = tok.trimmed().toDouble(&ok);
                if (ok) data.append(v);
            }
            if (data.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun valore numerico"); return; }
            m_canvas->setDensityData(data);
            m_canvas->setType(GraficoCanvas::Density);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(data.size()) + " campioni");
            break;
        }
        case 31: { /* OHLC — stessa struttura Candlestick */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<GraficoCanvas::OhlcPt> pts;
            static QRegularExpression sepOh("[,;]+");
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                QStringList tok = line.trimmed().split(sepOh);
                if (tok.size() < 5) continue;
                bool oo,oh,ol,oc;
                double o = tok[1].trimmed().toDouble(&oo), h = tok[2].trimmed().toDouble(&oh),
                       l = tok[3].trimmed().toDouble(&ol), c = tok[4].trimmed().toDouble(&oc);
                if (oo&&oh&&ol&&oc) pts.append({o, h, l, c, tok[0].trimmed()});
            }
            if (pts.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Formato: Label,O,H,L,C"); return; }
            m_canvas->setCandlestick(pts);
            m_canvas->setType(GraficoCanvas::OHLC);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(pts.size()) + " barre OHLC");
            break;
        }
        case 32: { /* Gauge — valore:massimo su riga 1, etichetta opzionale riga 2 */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QStringList lines = raw.split('\n', Qt::SkipEmptyParts);
            QVector<double> vals; QStringList lbls;
            QString line0 = lines[0].trimmed();
            QChar sep0 = line0.contains(':') ? ':' : ',';
            QStringList p0 = line0.split(sep0);
            bool ok0, ok1;
            double v0 = p0[0].trimmed().toDouble(&ok0);
            double v1 = p0.size() > 1 ? p0[1].trimmed().toDouble(&ok1) : (ok1=true, 100.0);
            if (!ok0 || !ok1) { m_statusLbl->setText("\xe2\x9a\xa0  Formato: valore:massimo"); return; }
            vals.append(v0); vals.append(v1);
            lbls.append(lines.size() > 1 ? lines[1].trimmed() : "");
            m_canvas->setData(vals, lbls);
            m_canvas->setType(GraficoCanvas::Gauge);
            m_statusLbl->setText("\xe2\x9c\x85  Gauge: " + QString::number(v0) + " / " + QString::number(v1));
            break;
        }
        case 33: { /* Bullet Chart */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<GraficoCanvas::BulletBar> data;
            static QRegularExpression sepBu("[,;]+");
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                QString l = line.trimmed();
                if (l.startsWith('#') || l.isEmpty()) continue;
                int colon = l.indexOf(':');
                QString lbl = colon > 0 ? l.left(colon).trimmed() : "";
                QString rest = colon > 0 ? l.mid(colon+1) : l;
                QStringList tok = rest.split(sepBu, Qt::SkipEmptyParts);
                if (tok.size() < 4) continue;
                bool o1,o2,o3,o4;
                double val = tok[0].trimmed().toDouble(&o1), tgt = tok[1].trimmed().toDouble(&o2),
                       rlo = tok[2].trimmed().toDouble(&o3), rhi = tok[3].trimmed().toDouble(&o4);
                if (o1&&o2&&o3&&o4)
                    data.append({lbl.isEmpty() ? QString::number(data.size()+1) : lbl, val, tgt, rlo, rhi});
            }
            if (data.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Formato: label: valore, target, min, max"); return; }
            m_canvas->setBulletData(data);
            m_canvas->setType(GraficoCanvas::Bullet);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(data.size()) + " barre bullet");
            break;
        }
        case 34: { /* Gantt */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<GraficoCanvas::GanttTask> data;
            static QRegularExpression sepGa("[,;]+");
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                QString l = line.trimmed();
                if (l.startsWith('#') || l.isEmpty()) continue;
                int colon = l.indexOf(':');
                QString name = colon > 0 ? l.left(colon).trimmed() : l;
                QString rest = colon > 0 ? l.mid(colon+1) : "";
                QStringList tok = rest.split(sepGa, Qt::SkipEmptyParts);
                if (tok.size() < 2) continue;
                bool os, oe;
                double s = tok[0].trimmed().toDouble(&os), e = tok[1].trimmed().toDouble(&oe);
                if (os && oe) {
                    QString cat = tok.size() > 2 ? tok[2].trimmed() : "";
                    data.append({name, cat, s, e});
                }
            }
            if (data.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Formato: task: inizio, fine [, categoria]"); return; }
            m_canvas->setGanttData(data);
            m_canvas->setType(GraficoCanvas::Gantt);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(data.size()) + " task");
            break;
        }
        case 36: { /* ParallelCoord — come multi-serie */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<QVector<QPointF>> series;
            QStringList names;
            static QRegularExpression sepPC("[,;\\s]+");
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                QString l = line.trimmed();
                if (l.startsWith('#')) {
                    names = l.mid(1).trimmed().split(sepPC, Qt::SkipEmptyParts);
                    continue;
                }
                QStringList tok = l.split(sepPC, Qt::SkipEmptyParts);
                if (tok.isEmpty()) continue;
                /* ogni riga = una "serie" (entità), colonne = assi */
                QVector<QPointF> row;
                for (int k = 0; k < tok.size(); ++k) {
                    bool ok; double v = tok[k].toDouble(&ok);
                    if (ok) row.append({(double)k, v});
                }
                if (!row.isEmpty()) series.append(row);
            }
            if (series.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessuna riga valida"); return; }
            m_canvas->setLine(series, names);
            m_canvas->setType(GraficoCanvas::ParallelCoord);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(series.size()) + " entit\xc3\xa0");
            break;
        }
        case 37: { /* Sankey */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<GraficoCanvas::SankeyLink> data;
            static QRegularExpression arrowSep("\\s*[\xe2\x86\x92>]+\\s*|\\s*->\\s*");
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                QString l = line.trimmed();
                if (l.startsWith('#') || l.isEmpty()) continue;
                /* formato: "sorgente → destinazione: valore" */
                int cpos = l.lastIndexOf(':');
                QString linkPart = cpos > 0 ? l.left(cpos) : l;
                double val = 1.0;
                if (cpos > 0) { bool ok; double v = l.mid(cpos+1).trimmed().toDouble(&ok); if (ok) val = v; }
                QStringList parts = linkPart.split(arrowSep, Qt::SkipEmptyParts);
                if (parts.size() < 2) {
                    /* prova con spazio + parola chiave */
                    parts = linkPart.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
                }
                if (parts.size() >= 2)
                    data.append({parts[0].trimmed(), parts[parts.size()-1].trimmed(), val});
            }
            if (data.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Formato: A \xe2\x86\x92 B: valore"); return; }
            m_canvas->setSankeyData(data);
            m_canvas->setType(GraficoCanvas::Sankey);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(data.size()) + " flussi");
            break;
        }
        case 38: { /* Tree */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<GraficoCanvas::TreeEdge> data;
            static QRegularExpression sepTr("[,;]+");
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                QString l = line.trimmed();
                if (l.startsWith('#') || l.isEmpty()) continue;
                int colon = l.indexOf(':');
                if (colon <= 0) continue;
                QString parent = l.left(colon).trimmed();
                QStringList children = l.mid(colon+1).split(sepTr, Qt::SkipEmptyParts);
                for (auto& ch : children) {
                    QString c = ch.trimmed();
                    if (!c.isEmpty()) data.append({parent, c});
                }
            }
            if (data.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Formato: padre: figlio1, figlio2"); return; }
            m_canvas->setTreeData(data);
            m_canvas->setType(GraficoCanvas::Tree);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(data.size()) + " archi");
            break;
        }
        case 39: { /* Chord */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<GraficoCanvas::ChordLink> data;
            static QRegularExpression arrowCh("\\s*[\xe2\x86\x92>]+\\s*|\\s*->\\s*");
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                QString l = line.trimmed();
                if (l.startsWith('#') || l.isEmpty()) continue;
                int cpos = l.lastIndexOf(':');
                double val = 1.0;
                QString linkPart = cpos > 0 ? l.left(cpos) : l;
                if (cpos > 0) { bool ok; double v = l.mid(cpos+1).trimmed().toDouble(&ok); if (ok) val = v; }
                QStringList parts = linkPart.split(arrowCh, Qt::SkipEmptyParts);
                if (parts.size() >= 2)
                    data.append({parts[0].trimmed(), parts[parts.size()-1].trimmed(), val});
            }
            if (data.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Formato: A \xe2\x86\x92 B: valore"); return; }
            m_canvas->setChordData(data);
            m_canvas->setType(GraficoCanvas::Chord);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(data.size()) + " corde");
            break;
        }
        case 40: { /* Violin Plot */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<QPair<QString,QVector<double>>> data;
            static QRegularExpression sepV("[,;\\s]+");
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                QString l = line.trimmed();
                if (l.startsWith('#') || l.isEmpty()) continue;
                int colon = l.indexOf(':');
                if (colon <= 0) continue;
                QString name = l.left(colon).trimmed();
                QVector<double> vals;
                for (auto& tok : l.mid(colon+1).split(sepV, Qt::SkipEmptyParts)) {
                    bool ok; double v = tok.toDouble(&ok); if (ok) vals.append(v);
                }
                if (!vals.isEmpty()) data.append({name, vals});
            }
            if (data.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Formato: NomeSerie: v1 v2 v3"); return; }
            m_canvas->setViolinData(data);
            m_canvas->setType(GraficoCanvas::Violin);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(data.size()) + " serie");
            break;
        }
        case 41: { /* Word Cloud */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<QPair<QString,double>> data;
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                QString l = line.trimmed();
                if (l.startsWith('#') || l.isEmpty()) continue;
                int colon = l.indexOf(':');
                if (colon <= 0) continue;
                bool ok; double v = l.mid(colon+1).trimmed().toDouble(&ok);
                if (ok) data.append({l.left(colon).trimmed(), v});
            }
            if (data.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Formato: parola:frequenza"); return; }
            m_canvas->setWordCloudData(data);
            m_canvas->setType(GraficoCanvas::WordCloud);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(data.size()) + " parole");
            break;
        }
        case 42: { /* Albero Radiale — stessa struttura di Tree */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<GraficoCanvas::TreeEdge> data;
            static QRegularExpression sepRT("[,;]+");
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                QString l = line.trimmed();
                if (l.startsWith('#') || l.isEmpty()) continue;
                int colon = l.indexOf(':');
                if (colon <= 0) continue;
                QString parent = l.left(colon).trimmed();
                QStringList children = l.mid(colon+1).split(sepRT, Qt::SkipEmptyParts);
                for (auto& ch : children) {
                    QString c = ch.trimmed();
                    if (!c.isEmpty()) data.append({parent, c});
                }
            }
            if (data.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Formato: padre: figlio1, figlio2"); return; }
            m_canvas->setTreeData(data);
            m_canvas->setType(GraficoCanvas::RadialTree);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(data.size()) + " archi");
            break;
        }
        case 43: { /* Linea Animata — stessa struttura di Linea */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<QVector<QPointF>> series;
            QStringList names;
            static QRegularExpression sepAN("[,;\\s]+");
            bool namesLoaded = false;
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                QString l = line.trimmed();
                if (l.startsWith('#')) {
                    if (!namesLoaded) {
                        names = l.mid(1).split(sepAN, Qt::SkipEmptyParts);
                        namesLoaded = true;
                    }
                    continue;
                }
                QStringList tok = l.split(sepAN, Qt::SkipEmptyParts);
                if (tok.size() < 2) continue;
                bool ok; double x = tok[0].toDouble(&ok); if (!ok) continue;
                for (int c = 1; c < tok.size(); ++c) {
                    double y = tok[c].toDouble(&ok); if (!ok) continue;
                    while (series.size() < c) series.append(QVector<QPointF>());
                    series[c-1].append({x, y});
                }
            }
            if (series.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessuna serie"); return; }
            m_canvas->setLine(series, names);
            m_canvas->setType(GraficoCanvas::AnimatedLine);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(series.size()) + " serie — animazione avviata");
            break;
        }
        case 44: { /* Small Multiples — stessa struttura di Linea */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<QVector<QPointF>> series;
            QStringList names;
            static QRegularExpression sepSM("[,;\\s]+");
            bool namesLoaded = false;
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                QString l = line.trimmed();
                if (l.startsWith('#')) {
                    if (!namesLoaded) {
                        names = l.mid(1).split(sepSM, Qt::SkipEmptyParts);
                        namesLoaded = true;
                    }
                    continue;
                }
                QStringList tok = l.split(sepSM, Qt::SkipEmptyParts);
                if (tok.size() < 2) continue;
                bool ok; double x = tok[0].toDouble(&ok); if (!ok) continue;
                for (int c = 1; c < tok.size(); ++c) {
                    double y = tok[c].toDouble(&ok); if (!ok) continue;
                    while (series.size() < c) series.append(QVector<QPointF>());
                    series[c-1].append({x, y});
                }
            }
            if (series.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessuna serie"); return; }
            m_canvas->setLine(series, names);
            m_canvas->setType(GraficoCanvas::SmallMultiples);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(series.size()) + " pannelli");
            break;
        }
    }
}

/* ── analyzeImage() ──────────────────────────────────────────────
   Carica un'immagine PNG/JPG, la invia al modello vision selezionato
   tramite AiClient::chatWithImage() e, alla risposta, inserisce la
   formula estratta in m_formulaEdit e traccia il grafico.
   ──────────────────────────────────────────────────────────────── */
void GraficoPage::analyzeImage() {
    if (!m_ai) {
        m_statusLbl->setText("\xe2\x9a\xa0  AiClient non disponibile");
        return;
    }
    if (m_ai->busy()) {
        m_statusLbl->setText("\xe2\x9a\xa0  AI occupata — attendi il termine dell'operazione");
        return;
    }
    const QString modello = m_visionCombo->currentText().trimmed();
    if (modello.isEmpty() || modello.startsWith("(")) {
        m_statusLbl->setText("\xe2\x9a\xa0  Seleziona un modello vision dalla lista");
        return;
    }

    /* Dialogo selezione immagine */
    const QString path = QFileDialog::getOpenFileName(
        this,
        "Seleziona immagine del grafico",
        QDir::homePath(),
        "Immagini (*.png *.jpg *.jpeg *.bmp *.webp)"
    );
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        m_statusLbl->setText("\xe2\x9d\x8c  Impossibile aprire il file");
        return;
    }
    const QByteArray raw   = f.readAll();
    const QByteArray b64   = raw.toBase64();
    const QString    mime  = path.endsWith(".png", Qt::CaseInsensitive) ? "image/png" : "image/jpeg";

    /* Cambia temporaneamente modello → vision → poi ripristina */
    const QString modeloPrecedente = m_ai->model();
    m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), modello);

    m_statusLbl->setText("\xf0\x9f\x94\x8d  Analisi in corso...");
    m_btnImgFormula->setEnabled(false);

    static const char* kSys =
        "Sei un analizzatore matematico preciso. "
        "Rispondi SEMPRE e SOLO in italiano. "
        "Il tuo UNICO compito e' identificare la formula matematica "
        "rappresentata nel grafico, nella forma y = f(x).";

    static const char* kUser =
        "Analizza questo grafico cartesiano e identifica la formula matematica "
        "nella forma y = f(x). "
        "Rispondi con UNA SOLA RIGA contenente solo la formula, "
        "senza testo aggiuntivo, senza 'y =', senza spiegazioni. "
        "Esempi di risposta corretta:\n"
        "  sin(x)\n"
        "  x^2 - 3*x + 2\n"
        "  1 / (1 + exp(-x))\n"
        "Se non riesci a identificare la formula scrivi: SCONOSCIUTO";

    /* Connessione one-shot per ricevere la risposta */
    auto* connHolder = new QObject(this);
    connect(m_ai, &AiClient::finished, connHolder,
            [this, connHolder, modeloPrecedente](const QString& risposta) {
        connHolder->deleteLater();
        m_btnImgFormula->setEnabled(true);

        /* Ripristina modello precedente */
        m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), modeloPrecedente);

        /* Estrai formula: prendi la prima riga non vuota */
        const QStringList righe = risposta.split('\n', Qt::SkipEmptyParts);
        QString formula;
        for (const QString& r : righe) {
            QString c = r.trimmed();
            /* Rimuovi eventuali prefissi "y = " o "f(x) = " */
            if (c.startsWith("y =", Qt::CaseInsensitive))  c = c.mid(3).trimmed();
            if (c.startsWith("y=",  Qt::CaseInsensitive))  c = c.mid(2).trimmed();
            if (c.startsWith("f(x) =", Qt::CaseInsensitive)) c = c.mid(6).trimmed();
            if (!c.isEmpty() && !c.toUpper().contains("SCONOSCIUTO")) {
                formula = c;
                break;
            }
        }

        if (formula.isEmpty()) {
            m_statusLbl->setText(
                "\xf0\x9f\x8c\xab  Il modello non ha riconosciuto la formula.\n"
                "Prova con un modello vision migliore o un'immagine pi\xc3\xb9 chiara.");
            return;
        }

        m_formulaEdit->setText(formula);
        m_statusLbl->setText("\xe2\x9c\x85  Formula estratta: " + formula);
        plot();
    });

    connect(m_ai, &AiClient::error, connHolder,
            [this, connHolder, modeloPrecedente](const QString& err) {
        connHolder->deleteLater();
        m_btnImgFormula->setEnabled(true);
        m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), modeloPrecedente);
        m_statusLbl->setText("\xe2\x9d\x8c  Errore: " + err);
    });

    m_ai->chatWithImage(kSys, kUser, b64, mime);
}
