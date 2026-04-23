#include "chart_widget.h"
#include <QPainter>
#include <QPainterPath>
#include <QMenu>
#include <QContextMenuEvent>
#include <QFileDialog>
#include <cmath>
#include <limits>
#include <algorithm>

/* ══════════════════════════════════════════════════════════════
   Palette colori per serie multiple
   ══════════════════════════════════════════════════════════════ */
QColor ChartWidget::paletteColor(int idx) {
    static const QColor pal[] = {
        QColor("#00a37f"), QColor("#0ea5e9"), QColor("#eab308"),
        QColor("#ef4444"), QColor("#a855f7"), QColor("#f97316"),
        QColor("#06b6d4"), QColor("#84cc16"),
    };
    return pal[idx % 8];
}

/* ══════════════════════════════════════════════════════════════
   Costruttore
   ══════════════════════════════════════════════════════════════ */
ChartWidget::ChartWidget(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(200);
    setMaximumHeight(340);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void ChartWidget::setTitle(const QString& t) { m_title = t; update(); }

void ChartWidget::setAxisLabels(const QString& xLabel, const QString& yLabel) {
    m_xLabel = xLabel; m_yLabel = yLabel; update();
}

void ChartWidget::addSeries(const Series& s) {
    m_series.append(s);
    updateRange();
    update();
}

void ChartWidget::clearAll() {
    m_series.clear(); m_title.clear();
    m_xLabel.clear(); m_yLabel.clear();
    m_fixedRange = false;
    update();
}

void ChartWidget::setRange(double xMin, double xMax, double yMin, double yMax) {
    m_xMin = xMin; m_xMax = xMax;
    m_yMin = yMin; m_yMax = yMax;
    m_fixedRange = true;
    update();
}

/* ══════════════════════════════════════════════════════════════
   updateRange — calcola bounds dai dati con 5% margine
   ══════════════════════════════════════════════════════════════ */
void ChartWidget::updateRange() {
    if (m_series.isEmpty() || m_fixedRange) return;
    double xMin =  std::numeric_limits<double>::max();
    double xMax = -std::numeric_limits<double>::max();
    double yMin =  std::numeric_limits<double>::max();
    double yMax = -std::numeric_limits<double>::max();
    for (const auto& s : m_series)
        for (const auto& pt : s.pts) {
            xMin = std::min(xMin, pt.x()); xMax = std::max(xMax, pt.x());
            yMin = std::min(yMin, pt.y()); yMax = std::max(yMax, pt.y());
        }
    if (!std::isfinite(xMin)) { xMin = 0; xMax = 1; }
    if (!std::isfinite(yMin)) { yMin = 0; yMax = 1; }
    const double dx = (xMax - xMin) * 0.06;
    const double dy = (yMax - yMin) * 0.06;
    m_xMin = xMin - dx; m_xMax = xMax + dx;
    m_yMin = yMin - dy; m_yMax = yMax + dy;
    if (m_xMin >= m_xMax) { m_xMin -= 1; m_xMax += 1; }
    if (m_yMin >= m_yMax) { m_yMin -= 1; m_yMax += 1; }
}

/* ══════════════════════════════════════════════════════════════
   toScreen — converte coordinate dati → pixel
   ══════════════════════════════════════════════════════════════ */
QPointF ChartWidget::toScreen(QPointF data, const QRectF& area) const {
    double sx = area.left() + (data.x() - m_xMin) / (m_xMax - m_xMin) * area.width();
    double sy = area.bottom() - (data.y() - m_yMin) / (m_yMax - m_yMin) * area.height();
    return {sx, sy};
}

/* ══════════════════════════════════════════════════════════════
   paintEvent — disegna tutto con QPainter
   ══════════════════════════════════════════════════════════════ */
void ChartWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRect r = rect();
    const int marginTop    = m_title.isEmpty() ? 10 : 30;
    const int marginBottom = m_xLabel.isEmpty() ? 36 : 52;
    const int marginLeft   = m_yLabel.isEmpty() ? 54 : 72;
    const int marginRight  = 16;

    const QRectF plot(
        r.left()  + marginLeft,
        r.top()   + marginTop,
        r.width() - marginLeft - marginRight,
        r.height()- marginTop  - marginBottom);

    /* ── Sfondo ── */
    p.fillRect(r, QColor("#1e1e1e"));
    p.fillRect(plot.toRect(), QColor("#181818"));

    /* ── Titolo ── */
    if (!m_title.isEmpty()) {
        p.setPen(QColor("#e5e7eb"));
        p.setFont(QFont("Inter", 11, QFont::Bold));
        p.drawText(QRect(r.left(), r.top(), r.width(), 26),
                   Qt::AlignHCenter | Qt::AlignVCenter, m_title);
    }

    /* ── Etichette assi ── */
    p.setPen(QColor("#6b7280"));
    p.setFont(QFont("Inter", 9));
    if (!m_xLabel.isEmpty())
        p.drawText(QRect(r.left(), r.bottom() - 18, r.width(), 18),
                   Qt::AlignHCenter | Qt::AlignVCenter, m_xLabel);
    if (!m_yLabel.isEmpty()) {
        p.save();
        p.translate(r.left() + 12, plot.center().y());
        p.rotate(-90);
        p.drawText(QRect(-50, -9, 100, 18), Qt::AlignCenter, m_yLabel);
        p.restore();
    }

    /* ── Griglia ── */
    const int nGrid = 5;
    p.setPen(QPen(QColor("#2a2a2a"), 1, Qt::DotLine));
    for (int i = 1; i < nGrid; ++i) {
        double t  = double(i) / nGrid;
        double gx = plot.left() + t * plot.width();
        double gy = plot.top()  + t * plot.height();
        p.drawLine(QPointF(gx, plot.top()),    QPointF(gx, plot.bottom()));
        p.drawLine(QPointF(plot.left(), gy),   QPointF(plot.right(), gy));
    }

    /* ── Bordo area grafico ── */
    p.setPen(QPen(QColor("#383838"), 1));
    p.drawRect(plot.adjusted(-0.5,-0.5,0.5,0.5).toRect());

    /* ── Etichette asse X ── */
    p.setPen(QColor("#6b7280"));
    p.setFont(QFont("Inter", 8));
    for (int i = 0; i <= nGrid; ++i) {
        double t   = double(i) / nGrid;
        double val = m_xMin + t * (m_xMax - m_xMin);
        double gx  = plot.left() + t * plot.width();
        QString lbl = QString::number(val, 'g', 4);
        p.drawText(QRectF(gx - 26, plot.bottom() + 4, 52, 14),
                   Qt::AlignHCenter, lbl);
    }

    /* ── Etichette asse Y ── */
    for (int i = 0; i <= nGrid; ++i) {
        double t   = double(i) / nGrid;
        double val = m_yMax - t * (m_yMax - m_yMin);
        double gy  = plot.top() + t * plot.height();
        QString lbl = QString::number(val, 'g', 4);
        p.drawText(QRectF(r.left(), gy - 8, marginLeft - 5, 16),
                   Qt::AlignRight | Qt::AlignVCenter, lbl);
    }

    if (m_series.isEmpty()) {
        p.setPen(QColor("#4b5563"));
        p.setFont(QFont("Inter", 11));
        p.drawText(plot.toRect(), Qt::AlignCenter, "Nessun dato");
        return;
    }

    /* ── Assi dello zero (se l'origine è nel range) ── */
    if (m_xMin <= 0.0 && m_xMax >= 0.0) {
        QPointF a = toScreen({0.0, m_yMin}, plot);
        QPointF b = toScreen({0.0, m_yMax}, plot);
        p.setPen(QPen(QColor("#44556688"), 1, Qt::DashLine));
        p.drawLine(a, b);
    }
    if (m_yMin <= 0.0 && m_yMax >= 0.0) {
        QPointF a = toScreen({m_xMin, 0.0}, plot);
        QPointF b = toScreen({m_xMax, 0.0}, plot);
        p.setPen(QPen(QColor("#44556688"), 1, Qt::DashLine));
        p.drawLine(a, b);
    }

    /* ── Serie ── */
    p.setClipRect(plot.adjusted(-1,-1,1,1));
    for (int si = 0; si < m_series.size(); ++si) {
        const auto& s = m_series[si];
        QColor c = s.color.isValid() ? s.color : paletteColor(si);

        if (s.mode == Scatter) {
            /* Scatter: cerchi pieni con etichetta opzionale */
            for (const auto& pt : s.pts) {
                QPointF sp = toScreen(pt, plot);
                p.setPen(Qt::NoPen);
                p.setBrush(c);
                p.drawEllipse(sp, 6.0, 6.0);
                p.setPen(QPen(c.lighter(130), 1));
                p.setBrush(Qt::NoBrush);
                p.drawEllipse(sp, 7.5, 7.5);

                if (s.showLabels) {
                    p.setClipping(false);
                    p.setPen(QColor("#e5e7eb"));
                    p.setFont(QFont("Inter", 8));
                    const QString lbl = QString("(%1, %2)")
                        .arg(pt.x(), 0, 'g', 4)
                        .arg(pt.y(), 0, 'g', 4);
                    p.drawText(QPointF(sp.x() + 9, sp.y() - 5), lbl);
                    p.setClipRect(plot.adjusted(-1,-1,1,1));
                }
            }
        } else {
            /* Line: path continuo */
            p.setPen(QPen(c, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.setBrush(Qt::NoBrush);
            QPainterPath path;
            bool first = true;
            for (const auto& pt : s.pts) {
                QPointF sp = toScreen(pt, plot);
                if (first) { path.moveTo(sp); first = false; }
                else        path.lineTo(sp);
            }
            p.drawPath(path);
        }
    }
    p.setClipping(false);

    /* ── Legenda (solo se più di una serie) ── */
    if (m_series.size() > 1) {
        const int lx = int(plot.right()) - 130;
        const int ly = int(plot.top()) + 6;
        for (int i = 0; i < m_series.size() && i < 6; ++i) {
            QColor c = paletteColor(i);
            p.fillRect(lx, ly + i * 16 + 5, 14, 3, c);
            p.setPen(QColor("#e5e7eb"));
            p.setFont(QFont("Inter", 9));
            p.drawText(QRect(lx + 18, ly + i * 16, 110, 14),
                       Qt::AlignVCenter, m_series[i].name);
        }
    }
}

/* ══════════════════════════════════════════════════════════════
   contextMenuEvent — salva PNG
   ══════════════════════════════════════════════════════════════ */
void ChartWidget::contextMenuEvent(QContextMenuEvent* ev) {
    QMenu menu(this);
    QAction* actSave = menu.addAction(
        "\xf0\x9f\x96\xbc  Salva come PNG...");
    QAction* chosen = menu.exec(ev->globalPos());
    if (chosen == actSave) {
        QString path = QFileDialog::getSaveFileName(
            this, "Salva grafico", "grafico.png",
            "Immagini PNG (*.png);;Tutti i file (*)");
        if (!path.isEmpty()) grab().save(path, "PNG");
    }
}
