#pragma once
#include <QWidget>
#include <QVector>
#include <QPointF>
#include <QString>
#include <QColor>

/*
 * ChartWidget — grafico a linea con QPainter puro (zero dipendenze esterne)
 *
 * Caratteristiche:
 *  - Serie multiple con palette colori automatica
 *  - Griglia adattiva, assi con etichette, titolo
 *  - Auto-scale dell'intervallo sui dati
 *  - Tasto destro → salva PNG
 *  - Doppio click → copia formula negli appunti
 */
class ChartWidget : public QWidget {
    Q_OBJECT
public:
    enum Mode { Line, Scatter };

    struct Series {
        QString          name;
        QVector<QPointF> pts;
        QColor           color;   /* QColor() = palette automatica */
        Mode             mode  = Line;
        bool             showLabels = false; /* mostra "(x, y)" su ogni punto */
    };

    explicit ChartWidget(QWidget* parent = nullptr);

    void setTitle(const QString& t);
    void setAxisLabels(const QString& xLabel, const QString& yLabel);
    void addSeries(const Series& s);
    void clearAll();
    /** Forza un intervallo fisso (ignora auto-scale sui dati). */
    void setRange(double xMin, double xMax, double yMin, double yMax);

    QSize sizeHint()        const override { return {560, 280}; }
    QSize minimumSizeHint() const override { return {300, 180}; }

protected:
    void paintEvent(QPaintEvent*) override;
    void contextMenuEvent(QContextMenuEvent*) override;

private:
    void   updateRange();
    QPointF toScreen(QPointF data, const QRectF& area) const;
    static QColor paletteColor(int idx);

    QString         m_title;
    QString         m_xLabel;
    QString         m_yLabel;
    QVector<Series> m_series;
    double          m_xMin = 0, m_xMax = 1;
    double          m_yMin = -1, m_yMax = 1;
    bool            m_fixedRange = false;
};
