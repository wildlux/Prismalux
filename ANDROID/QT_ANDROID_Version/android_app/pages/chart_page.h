#pragma once
#include <QWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QVector>
#include <QPointF>
#include <QSlider>
#include <cmath>

class AiClient;

/* ── FormulaEval — valutatore espressioni matematiche minimal ── */
class FormulaEval {
public:
    explicit FormulaEval(const QString& expr);
    double eval(double x);
    bool   hasError() const { return m_error; }
    QString errorMsg() const { return m_errMsg; }

private:
    QString m_expr;
    int     m_pos   = 0;
    bool    m_error = false;
    QString m_errMsg;
    double  m_x     = 0.0;

    double parseExpr();
    double parseTerm();
    double parsePow();
    double parsePrimary();
    void   skipWs();
    QChar  peek();
    QChar  consume();
    QString readName();
    double readNumber();
};

/* ── ChartCanvas — tela QPainter con pan/zoom ─────────────────── */
class ChartCanvas : public QWidget {
    Q_OBJECT
public:
    explicit ChartCanvas(QWidget* parent = nullptr);

    void setFormula(const QString& formula, bool autoScale = true);
    void setScatter(const QVector<QPointF>& pts);
    void clearScatter();
    void setXRange(double xMin, double xMax);
    void resetView();
    void zoom(double factor);

    QString lastError() const { return m_evalError; }

signals:
    void plotError(const QString& msg);

protected:
    void paintEvent(QPaintEvent*) override;
    void wheelEvent(QWheelEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    bool event(QEvent* e) override;

private:
    void   resampleCurve();
    QPoint worldToScreen(double wx, double wy) const;
    QPointF screenToWorld(const QPoint& p) const;
    void   drawGrid(QPainter& p);
    void   drawAxes(QPainter& p);
    void   drawCurve(QPainter& p);
    void   drawScatter(QPainter& p);

    QString           m_formula;
    QVector<QPointF>  m_curve;
    QVector<QPointF>  m_scatter;
    QString           m_evalError;

    double m_xMin = -8.0, m_xMax = 8.0;
    double m_yMin = -6.0, m_yMax = 6.0;

    /* pan */
    QPoint  m_dragOrigin;
    double  m_dragXMin = 0, m_dragXMax = 0;
    double  m_dragYMin = 0, m_dragYMax = 0;
    bool    m_dragging = false;

    /* pinch */
    double m_pinchScale0 = 1.0;
    double m_pinchXMin0  = 0, m_pinchXMax0 = 0;
    double m_pinchYMin0  = 0, m_pinchYMax0 = 0;
};

/* ── ChartPage — pagina completa con controlli ────────────────── */
class ChartPage : public QWidget {
    Q_OBJECT
public:
    explicit ChartPage(AiClient* ai = nullptr, QWidget* parent = nullptr);

public slots:
    void showFormula(const QString& formula);
    void showScatter(const QVector<QPointF>& pts);

private slots:
    void onPlotClicked();
    void onResetClicked();
    void onZoomIn();
    void onZoomOut();
    void onPlotError(const QString& msg);

private:
    ChartCanvas*  m_canvas    = nullptr;
    QLineEdit*    m_formulaEdit = nullptr;
    QLabel*       m_statusLbl   = nullptr;
    QPushButton*  m_plotBtn    = nullptr;
    QPushButton*  m_resetBtn   = nullptr;
    QPushButton*  m_zoomInBtn  = nullptr;
    QPushButton*  m_zoomOutBtn = nullptr;
};
