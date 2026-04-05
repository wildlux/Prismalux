#pragma once
/*
 * grafico_page.h — Pagina Grafico
 * ===============================
 * GraficoCanvas : area di disegno QPainter-pura con zoom (rotellina) e pan (drag).
 *   Tipi supportati: Cartesiano · Torta · Istogramma · Scatter · Grafo · Scatter 3D
 *                    Diagramma di Smith — Numeri Primi (SmithPrime)
 *                    Linea multi-serie · Polare · Radar · Bolle · Heatmap · Candele OHLC
 *                    Area · Cascata (Waterfall) · Scalini (Step)
 * GraficoPage   : layout completo con pannello parametri (sx) + canvas (dx).
 */
#include <QWidget>
#include <QVector>
#include <QPointF>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QComboBox>
#include <QStackedWidget>
#include <QLineEdit>
#include <QTextEdit>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QContextMenuEvent>
#include <QRectF>

class AiClient;

/* ══════════════════════════════════════════════════════════════
   GraficoCanvas
   ══════════════════════════════════════════════════════════════ */
class GraficoCanvas : public QWidget {
    Q_OBJECT
public:
    enum ChartType { Cartesian = 0, Pie, Histogram, ScatterXY, Graph, Scatter3D, Graph3D,
                     SmithPrime,     /* == 7 */
                     MathConst,      /* == 8  — π · e · Primi nel disco di Smith */
                     Line,           /* == 9  — linea multi-serie da dati discreti */
                     Polar,          /* == 10 — r = f(θ) in coordinate polari */
                     Radar,          /* == 11 — spider/ragno multi-categoria */
                     Bubble,         /* == 12 — scatter con dimensione (x, y, raggio) */
                     Heatmap,        /* == 13 — matrice di valori → colori */
                     Candlestick,    /* == 14 — OHLC finanziario */
                     Area,           /* == 15 — area riempita (stessa struttura di Line) */
                     Waterfall,      /* == 16 — cascata finanziaria (etichetta:valore) */
                     Step,           /* == 17 — scalini (stessa struttura di Line) */
                     Column,         /* == 18 — barre verticali per categorie */
                     HBar,           /* == 19 — barre orizzontali */
                     GroupedBar,     /* == 20 — barre raggruppate multi-serie */
                     StackedBar,     /* == 21 — barre impilate */
                     StackedBar100,  /* == 22 — barre impilate normalizzate 100% */
                     Funnel,         /* == 23 — imbuto a trapezi */
                     Donut,          /* == 24 — ciambella (pie con foro) */
                     Treemap,        /* == 25 — mappa ad albero rettangoli */
                     Sunburst,       /* == 26 — anelli concentrici gerarchici */
                     BoxPlot,        /* == 27 — box-and-whisker */
                     DotPlot,        /* == 28 — scatter 1D con stacking */
                     Density,        /* == 29 — KDE gaussiana */
                     AreaStacked,    /* == 30 — area cumulativa impilata */
                     OHLC,           /* == 31 — barre OHLC senza rettangolo */
                     Gauge,          /* == 32 — semicerchio gauge */
                     Bullet,         /* == 33 — bullet chart */
                     Gantt,          /* == 34 — diagramma di Gantt */
                     Pyramid,        /* == 35 — piramide demografica */
                     ParallelCoord,  /* == 36 — coordinate parallele */
                     Sankey,         /* == 37 — diagramma di Sankey */
                     Tree,           /* == 38 — albero gerarchico */
                     Chord,          /* == 39 — diagramma a corde */
                     Violin,         /* == 40 — Violin plot (KDE mirrored) */
                     WordCloud,      /* == 41 — Word cloud (parola:frequenza) */
                     RadialTree,     /* == 42 — Albero radiale (root al centro) */
                     AnimatedLine,   /* == 43 — Linea animata (QTimer) */
                     SmallMultiples };/* == 44 — Small multiples grid */

    /** Posizione del gizmo assi (2D e 3D). */
    enum AxisPos { AtData = 0, BottomLeft, BottomRight, TopLeft, TopRight };

    struct Pt3D    { double x, y, z; QString label; };
    struct Node3D  { QString name; double x, y, z; };
    struct OhlcPt  { double o, h, l, c; QString label; };

    struct BoxData  { double min, q1, med, q3, max; QString label; };
    struct BulletBar{ QString label; double value, target, rangeLow, rangeHigh; };
    struct GanttTask{ QString name, cat; double start, end; };
    struct SankeyLink{ QString from, to; double value; };
    struct TreeEdge  { QString parent, child; };
    struct ChordLink { QString from, to; double value; };

    /** Punto di una serie intera mappato nel disco di Smith via Γ = (z−1)/(z+1). */
    struct SmithPt { double re, im;    /* Γ coordinate */
                     bool gaussian;    /* true = primo di Gauss, false = serie reale */
                     int  ga, gb;      /* valore primo (ga=p, gb=0) o (a+b*i) */
                     int  value;       /* intero sorgente: il numero p, Fib, triangol., ecc. */
                     int  seriesId;    /* 0=primo, 1=Fibonacci, 2=triangolare, 3=quadrato, 4+=gaussiano */
                   };

    /** Punto di una serie di convergenza (π · e · primi) nel disco di Smith. */
    struct MathPt  { double re, im;    /* Γ coordinate (im=0 per serie reali) */
                     int seriesId;     /* 0=π Leibniz, 1=e Taylor, 2=primo reale */
                     int n;            /* indice del termine o valore del primo */
                   };

    explicit GraficoCanvas(QWidget* parent = nullptr);

    void setCartesian(const QString& formula, double xMin, double xMax);
    void setData(const QVector<double>& values, const QStringList& labels);
    void setScatter(const QVector<QPointF>& pts);
    void setEdges(const QVector<QPair<QString,QString>>& edges);
    void setScatter3D(const QVector<Pt3D>& pts);
    void setGraph3D(const QVector<Node3D>& nodes, const QVector<QPair<QString,QString>>& edges);

    /**
     * Popola il diagramma di Smith con i numeri primi fino a maxN.
     * @param maxN       limite superiore per il crivello (per i reali) e per la norma (per i gaussiani)
     * @param showReal   mostra i primi reali sull'asse reale (scala log₂)
     * @param showGauss  mostra i primi gaussiani nel piano complesso
     * @param expanded   se true, include anche i primi col denominatore fuori dal disco (|Γ|>1)
     * @param normScale  fattore di scala su z prima della trasformata di Möbius (default 1.0)
     */
    void setSmithPrime(int maxN, bool showReal, bool showGauss,
                       bool expanded, double normScale,
                       bool showFib    = false,
                       bool showTri    = false,
                       bool showSqr    = false,
                       bool showLabels = false);

    /**
     * Aggiunge una serie personalizzata (seriesId=5, colore magenta) al disco di Smith.
     * Va chiamata DOPO setSmithPrime(). I valori vengono mappati via Γ(log₂(N)*normScale).
     * @param vals  lista di interi > 1 fornita dall'utente
     */
    void addSmithCustom(const QVector<int>& vals);

    /** Grafico a linee multi-serie. Ogni QVector<QPointF> è una serie; names = etichette legenda. */
    void setLine(const QVector<QVector<QPointF>>& series, const QStringList& names);

    /** Grafico polare r = f(θ): formula con variabile x=θ, angolo in radianti. */
    void setPolar(const QString& formula, double tMin, double tMax);

    /** Heatmap da matrice 2D. rowLbls/colLbls possono essere vuoti. */
    void setHeatmap(const QVector<QVector<double>>& data,
                    const QStringList& rowLbls, const QStringList& colLbls);

    /** Grafico a candele OHLC. */
    void setCandlestick(const QVector<OhlcPt>& pts);

    /**
     * Popola il diagramma con le serie di convergenza verso π (Leibniz),
     * e (Taylor) e i numeri primi reali (scala log₂), tutti mappati via Γ=(z−1)/(z+1).
     * @param nTerms    numero di termini / punti per ogni serie attiva
     * @param showPi    mostra serie di Leibniz 4·Σ(-1)^k/(2k+1) → π
     * @param showE     mostra serie di Taylor Σ1/k! → e
     * @param showPrimes mostra primi reali sulla scala log₂ (come in SmithPrime)
     */
    void setMathConst(int nTerms, bool showPi, bool showE, bool showPrimes);

    void setType(ChartType t);
    void resetView();

    /* setter per i nuovi tipi di dato */
    void setBoxData    (const QVector<BoxData>&   d) { m_boxData     = d; }
    void setBulletData (const QVector<BulletBar>&  d) { m_bulletData  = d; }
    void setGanttData  (const QVector<GanttTask>&  d) { m_ganttData   = d; }
    void setSankeyData (const QVector<SankeyLink>& d) { m_sankeyData  = d; }
    void setTreeData   (const QVector<TreeEdge>&   d) { m_treeData    = d; }
    void setChordData  (const QVector<ChordLink>&  d) { m_chordData   = d; }
    void setSunburstData(const QVector<QPair<QString,double>>& d) { m_sunburstData = d; }
    void setDensityData(const QVector<double>&     d) { m_densityData = d; }
    void setViolinData  (const QVector<QPair<QString,QVector<double>>>& d) { m_violinData = d; update(); }
    void setWordCloudData(const QVector<QPair<QString,double>>& d)         { m_wordData   = d; update(); }

    /** Numero di punti nell'ultimo setSmithPrime() — usato da GraficoPage::plot(). */
    int smithPointCount() const { return m_smithPts.size(); }
    /** Numero di punti nell'ultimo setMathConst() — usato da GraficoPage::plot(). */
    int mathPointCount()  const { return m_mathPts.size();  }
    void setAxes2dPos(int pos);   ///< AxisPos per grafici 2D (Cartesiano, Scatter)
    void setAxes3dPos(int pos);   ///< AxisPos per grafici 3D (Scatter3D, Graph3D)

    QSize sizeHint()        const override { return {600, 440}; }
    QSize minimumSizeHint() const override { return {300, 200}; }

signals:
    void statusMessage(const QString& msg);

protected:
    void paintEvent(QPaintEvent*)         override;
    void wheelEvent(QWheelEvent*)         override;
    void mousePressEvent(QMouseEvent*)    override;
    void mouseMoveEvent(QMouseEvent*)     override;
    void mouseReleaseEvent(QMouseEvent*)  override;
    void contextMenuEvent(QContextMenuEvent*) override;

private:
    void drawCartesian (QPainter& p, const QRectF& area);
    void drawPie       (QPainter& p, const QRectF& area);
    void drawHistogram (QPainter& p, const QRectF& area);
    void drawScatterXY (QPainter& p, const QRectF& area);
    void drawGraph     (QPainter& p, const QRectF& area);
    void drawScatter3D (QPainter& p, const QRectF& area);
    void drawGraph3D   (QPainter& p, const QRectF& area);
    void drawGrid       (QPainter& p, const QRectF& area);
    void drawSmithPrime (QPainter& p, const QRectF& area);
    void drawMathConst  (QPainter& p, const QRectF& area);
    void drawLine       (QPainter& p, const QRectF& area);
    void drawPolar      (QPainter& p, const QRectF& area);
    void drawRadar      (QPainter& p, const QRectF& area);
    void drawBubble     (QPainter& p, const QRectF& area);
    void drawHeatmap    (QPainter& p, const QRectF& area);
    void drawCandlestick(QPainter& p, const QRectF& area);
    void drawArea       (QPainter& p, const QRectF& area);
    void drawWaterfall  (QPainter& p, const QRectF& area);
    void drawStep       (QPainter& p, const QRectF& area);
    /* nuovi 22 tipi */
    void drawColumn      (QPainter& p, const QRectF& area);
    void drawHBar        (QPainter& p, const QRectF& area);
    void drawGroupedBar  (QPainter& p, const QRectF& area);
    void drawStackedBar  (QPainter& p, const QRectF& area);
    void drawStackedBar100(QPainter& p, const QRectF& area);
    void drawFunnel      (QPainter& p, const QRectF& area);
    void drawDonut       (QPainter& p, const QRectF& area);
    void drawTreemap     (QPainter& p, const QRectF& area);
    void drawSunburst    (QPainter& p, const QRectF& area);
    void drawBoxPlot     (QPainter& p, const QRectF& area);
    void drawDotPlot     (QPainter& p, const QRectF& area);
    void drawDensity     (QPainter& p, const QRectF& area);
    void drawAreaStacked (QPainter& p, const QRectF& area);
    void drawOHLC        (QPainter& p, const QRectF& area);
    void drawGauge       (QPainter& p, const QRectF& area);
    void drawBullet      (QPainter& p, const QRectF& area);
    void drawGantt       (QPainter& p, const QRectF& area);
    void drawPyramid     (QPainter& p, const QRectF& area);
    void drawParallelCoord(QPainter& p, const QRectF& area);
    void drawSankey      (QPainter& p, const QRectF& area);
    void drawTree        (QPainter& p, const QRectF& area);
    void drawChord       (QPainter& p, const QRectF& area);
    void drawViolin        (QPainter& p, const QRectF& area);
    void drawWordCloud     (QPainter& p, const QRectF& area);
    void drawRadialTree    (QPainter& p, const QRectF& area);
    void drawAnimatedLine  (QPainter& p, const QRectF& area);
    void drawSmallMultiples(QPainter& p, const QRectF& area);

    QPointF dataToScreen(double dx, double dy, const QRectF& area) const;
    QPointF screenToData(const QPointF& sp,  const QRectF& area) const;

    static QColor  paletteColor(int idx);
    static QString fmtNum(double v);
    static double  niceStep(double range, int ticks);

    /* ── tipo corrente ── */
    ChartType m_type = Cartesian;

    /* ── cartesiano ── */
    QString          m_formula;
    QVector<QPointF> m_cartPts;

    /* ── vista cartesiana / scatter ── */
    double m_xVMin = -5, m_xVMax = 5;
    double m_yVMin = -3, m_yVMax = 3;

    /* ── dati generici (Torta · Istogramma) ── */
    QVector<double> m_values;
    QStringList     m_labels;

    /* ── scatter 2D ── */
    QVector<QPointF> m_scatterPts;

    /* ── grafo ── */
    QVector<QPair<QString,QString>> m_edges;

    /* ── scatter 3D ── */
    QVector<Pt3D> m_pts3d;

    /* ── grafo 3D ── */
    QVector<Node3D>                 m_nodes3d;
    QVector<QPair<QString,QString>> m_edges3d;

    /* ── Smith Prime ── */
    QVector<SmithPt> m_smithPts;
    bool             m_smithExpanded    = false;
    bool             m_smithShowLabels  = false;
    double           m_smithNormScale   = 1.0;   ///< scala corrente — usata da addSmithCustom

    /* ── Math Const (π · e · Primi) ── */
    QVector<MathPt>  m_mathPts;

    /* ── Linea multi-serie ── */
    QVector<QVector<QPointF>> m_lineSeries;
    QStringList               m_lineNames;

    /* ── Polare ── */
    double m_polarTMin = 0.0;
    double m_polarTMax = 6.28318530718;

    /* ── Heatmap ── */
    QVector<QVector<double>> m_heatData;
    QStringList              m_heatRowLbls;
    QStringList              m_heatColLbls;

    /* ── Candlestick OHLC ── */
    QVector<OhlcPt>  m_ohlcPts;

    /* ── nuovi tipi ── */
    QVector<BoxData>   m_boxData;
    QVector<BulletBar> m_bulletData;
    QVector<GanttTask> m_ganttData;
    QVector<SankeyLink>m_sankeyData;
    QVector<TreeEdge>  m_treeData;
    QVector<ChordLink> m_chordData;
    QVector<QPair<QString,double>> m_sunburstData;
    QVector<double>    m_densityData;

    /* ── Violin ── */
    QVector<QPair<QString,QVector<double>>> m_violinData;

    /* ── Word Cloud ── */
    QVector<QPair<QString,double>>          m_wordData;

    /* ── Animazione ── */
    QTimer* m_animTimer = nullptr;
    int     m_animFrame = 0;

    double m_rotY = 0.65;   /* yaw   (radianti) — condiviso da Scatter3D e Graph3D */
    double m_rotX = 0.35;   /* pitch (radianti) */

    /* ── posizione gizmo assi ── */
    int m_axes2dPos = AtData;       /* default: origine dati */
    int m_axes3dPos = BottomRight;  /* default: angolo in basso a destra */

    /* ── zoom/pan per tipi non-cartesiani ── */
    double  m_zoomNC = 1.0;
    QPointF m_panNC;

    /* ── stato mouse ── */
    bool    m_dragging = false;
    QPointF m_dragStart;
    /* snapshot al drag-start */
    double  m_xVMinD, m_xVMaxD, m_yVMinD, m_yVMaxD;
    double  m_zoomD;
    QPointF m_panD;
    double  m_rotYD, m_rotXD;
};

/* ══════════════════════════════════════════════════════════════
   GraficoPage
   ══════════════════════════════════════════════════════════════ */
class GraficoPage : public QWidget {
    Q_OBJECT
public:
    explicit GraficoPage(AiClient* ai, QWidget* parent = nullptr);

    /** Accesso diretto al canvas (usato da ImpostazioniPage per i controlli assi). */
    GraficoCanvas* canvas() const { return m_canvas; }

signals:
    /** Emesso quando l'utente clicca "Installa modello Vision" — apre Impostazioni */
    void requestOpenSettings(const QString& tabName);

private:
    QWidget* buildLeftPanel();
    void     plot();
    void     analyzeImage();   ///< Invia immagine a modello vision → riempie formula

    AiClient*       m_ai         = nullptr;

    GraficoCanvas*  m_canvas     = nullptr;
    QComboBox*      m_typeCombo  = nullptr;
    QStackedWidget* m_paramStack = nullptr;
    QLabel*         m_statusLbl  = nullptr;

    /* Parametri cartesiano */
    QLineEdit*      m_formulaEdit = nullptr;
    QDoubleSpinBox* m_xMinSpin   = nullptr;
    QDoubleSpinBox* m_xMaxSpin   = nullptr;

    /* Dati per tutti gli altri tipi */
    QTextEdit* m_dataEdit  = nullptr;
    QLabel*    m_dataHint  = nullptr;

    /* Generatore da espressione (panel [1]) */
    QWidget*        m_genSection = nullptr;  ///< nascosto per Grafo/Grafo3D
    QLineEdit*      m_genExpr    = nullptr;  ///< formula f(x), usa x come indice
    QDoubleSpinBox* m_genFrom    = nullptr;
    QDoubleSpinBox* m_genTo      = nullptr;
    QDoubleSpinBox* m_genStep    = nullptr;

    /* Sezione immagine → formula (solo tipo Cartesiano) */
    QWidget*    m_imgSection     = nullptr;
    QComboBox*  m_visionCombo    = nullptr;
    QPushButton* m_btnImgFormula = nullptr;

    /* Parametri Smith Prime (pannello idx 2) */
    QSpinBox*       m_smithMaxN    = nullptr;
    QCheckBox*      m_smithReal    = nullptr;
    QCheckBox*      m_smithGauss   = nullptr;
    QCheckBox*      m_smithExp     = nullptr;
    QDoubleSpinBox* m_smithNorm    = nullptr;
    QCheckBox*      m_smithFib     = nullptr;  ///< serie Fibonacci
    QCheckBox*      m_smithTri     = nullptr;  ///< numeri triangolari
    QCheckBox*      m_smithSqr     = nullptr;  ///< quadrati perfetti
    QCheckBox*      m_smithLabels  = nullptr;  ///< etichette numeriche sui punti
    QTextEdit*      m_smithCustom  = nullptr;  ///< valori personalizzati dell'utente
    QPushButton*    m_btnSmithFile = nullptr;  ///< carica sequenza da file

    /* Parametri Math Const — π · e · Primi (pannello idx 3) */
    QSpinBox*       m_mathTerms  = nullptr;
    QCheckBox*      m_mathPi     = nullptr;
    QCheckBox*      m_mathE      = nullptr;
    QCheckBox*      m_mathPrimes = nullptr;

    /* Parametri Polare (pannello idx 4) */
    QLineEdit*      m_polarFormula = nullptr;
    QDoubleSpinBox* m_polarTMinSpin = nullptr;
    QDoubleSpinBox* m_polarTMaxSpin = nullptr;
    QSpinBox*       m_polarSamples  = nullptr;
};
