#pragma once
#include <QWidget>
#include <QVector>
#include <QPointF>
#include <QString>
#include <QColor>

/* ── Punto RAB₀-L: coordinate display (polar web) + valori formula ── */
struct Rab0lPoint {
    double dispAngle;   // angolo display basato su nucleotide [0, 2π)
    double dispR;       // raggio display normalizzato per posizione [0,1]
    double theta;       // theta reale formula: (rid/80)*2π + 2π*i
    double logR;        // log(r_i) reale per distanza composita
    char   base;        // 'A','T','G','C'
    int    pos;         // posizione nella sequenza
};

struct Rab0lResult {
    double          sim  = 0.0;
    int             len  = 0;
    int             snp  = 0;
    QVector<double> contrib;   // exp(-α·d_i) per posizione
};

/* ══════════════════════════════════════════════════════════════════════
   Rab0lCanvas — widget QPainter "spider web" logaritmica per RAB₀-L

   Display polar: angolo = identità nucleotide, raggio = posizione seq.
   Zoom con rotella · Pan con drag · Seq1 pieno, Seq2 tratteggiato.
   ══════════════════════════════════════════════════════════════════════ */
class Rab0lCanvas : public QWidget {
    Q_OBJECT
public:
    explicit Rab0lCanvas(QWidget* parent = nullptr);

    void setSequences(const QString& seq1, const QString& seq2);
    void setSeq1Only(const QString& seq1);
    void clearAll();
    const Rab0lResult& result() const { return m_result; }

    static int                 ridFromBase(char c);
    static QColor              colorForBase(char c);
    static QVector<Rab0lPoint> computePoints(const QString& seq);
    static Rab0lResult         computeSim(const QVector<Rab0lPoint>& pts1,
                                          const QVector<Rab0lPoint>& pts2);
protected:
    void paintEvent(QPaintEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;

private:
    QString             m_seq1, m_seq2;
    QVector<Rab0lPoint> m_pts1, m_pts2;
    Rab0lResult         m_result;
    double              m_zoom = 1.0;
    QPointF             m_pan;
    QPointF             m_dragStart;
    bool                m_dragging = false;

    QRectF  canvasArea() const;
    double  outerR(const QRectF& area) const;
    QPointF toScreen(const Rab0lPoint& p, const QRectF& area) const;
    void    drawGrid(QPainter& p, const QRectF& area) const;
    void    drawSequence(QPainter& p, const QVector<Rab0lPoint>& pts,
                         bool primary, const QRectF& area) const;
    void    drawConnections(QPainter& p, const QRectF& area) const;
    void    drawLegend(QPainter& p) const;
};
