#pragma once
#include <QWidget>
#include <QVector>
#include <QString>
#include <QColor>
#include <QPointF>

class QToolButton;
class QLabel;

/* ══════════════════════════════════════════════════════════════
   NatalChartWidget — ruota astrologica stile Astro.com
   Zoom: rotella mouse | Pan: trascina | Reset: doppio clic
   ══════════════════════════════════════════════════════════════ */
class NatalChartWidget : public QWidget {
    Q_OBJECT
public:
    struct Planet {
        QString name;
        QString symbol;   /* es. "☉", "☽", "AC" */
        double  lon;      /* longitudine eclittica 0-360 */
        QColor  color;
    };

    explicit NatalChartWidget(QWidget* parent = nullptr);

    /* Imposta i dati e ridisegna.
       mcLon = -1 => MC calcolato come ascLon+270 (case uguali).
       cusps = array 12 double con cuspidi case Placidus (nullptr = Equal House). */
    void setData(const QVector<Planet>& planets, double ascLon, double mcLon = -1,
                 const double* cusps = nullptr);
    void clear();
    void resetView();

    QSize sizeHint()        const override { return {460, 460}; }
    QSize minimumSizeHint() const override { return {300, 300}; }

protected:
    void paintEvent(QPaintEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void resizeEvent(QResizeEvent*) override;

private slots:
    void onZoomInClicked();
    void onZoomOutClicked();
    void onResetViewClicked();

private:
    struct Aspect { int i, j, type; };

    QPointF toScreen(double lon, double r, double cx, double cy) const;
    double  screenAngle(double lon) const;
    void    computeAspects();
    void    positionZoomBar();
    void    updateZoomLabel();

    void drawBackground(QPainter& p, double cx, double cy, double R);
    void drawZodiacRing(QPainter& p, double cx, double cy, double R);
    void drawHouseArea (QPainter& p, double cx, double cy, double R);
    void drawAxes      (QPainter& p, double cx, double cy, double R);
    void drawAspects   (QPainter& p, double cx, double cy, double R);
    void drawPlanets   (QPainter& p, double cx, double cy, double R);
    void drawPlaceholder(QPainter& p, double cx, double cy, double R);

    QVector<Planet> m_planets;
    QVector<Aspect> m_aspects;
    double  m_ascLon      = 0.0;
    double  m_mcLon       = -1.0;
    double  m_cusps[12]   = {};
    bool    m_hasRealCusps = false;
    bool    m_hasData     = false;

    /* Zoom / Pan */
    double  m_zoom     = 1.0;
    QPointF m_pan      {0.0, 0.0};
    QPointF m_dragStart{0.0, 0.0};
    bool    m_dragging = false;

    /* Zoom bar overlay (top-right) */
    QWidget*     m_zoomBar    = nullptr;
    QToolButton* m_btnZoomOut = nullptr;
    QLabel*      m_zoomLbl    = nullptr;
    QToolButton* m_btnZoomIn  = nullptr;
    QToolButton* m_btnReset   = nullptr;
};
