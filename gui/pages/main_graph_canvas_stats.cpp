/*
 * main_graph_canvas_stats.cpp — GraficoCanvas: statistici/business
 * =====================================================================
 * BoxPlot, DotPlot, Density, AreaStacked, OHLC, Gauge, Bullet, Gantt,
 * Pyramid. Split da main_graph_canvas.cpp (TODO D-8).
 */
#include "main_graph.h"
#include "main_graph_canvas_p.h"

#include <QPainter>
#include <QPainterPath>
#include <QFont>
#include <cmath>
#include <algorithm>


/* ══════════════════════════════════════════════════════════════
   BoxPlot — box-and-whisker
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::drawBoxPlot(QPainter& p, const QRectF& a) {
    p.setRenderHint(QPainter::Antialiasing);
    if (m_boxData.isEmpty()) {
        p.setPen(QColor(0x77,0x77,0x77));
        p.drawText(a, Qt::AlignCenter, "Nessun dato\nFormato: label: min,Q1,med,Q3,max");
        return;
    }
    int n = m_boxData.size();
    /* scala Y */
    double yMn = m_boxData[0].min, yMx = m_boxData[0].max;
    for (auto& b : m_boxData) {
        yMn = std::min(yMn, b.min); yMx = std::max(yMx, b.max);
    }
    double pad = (yMx - yMn) * 0.1 + 0.5;
    m_xVMin = 0; m_xVMax = n;
    m_yVMin = yMn - pad; m_yVMax = yMx + pad;

    const double leftM = 60.0;
    QRectF pa(a.left()+leftM, a.top()+10, a.width()-leftM-10, a.height()-36);

    /* griglia Y */
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 8));
    double yStep = niceStep(m_yVMax - m_yVMin, 6);
    double yFirst = std::ceil(m_yVMin / yStep) * yStep;
    for (double y = yFirst; y <= m_yVMax + yStep*0.01; y += yStep) {
        double sy = pa.bottom() - (y - m_yVMin) / (m_yVMax - m_yVMin) * pa.height();
        if (sy < pa.top() || sy > pa.bottom()) continue;
        p.setPen(QPen(QColor(0x35,0x35,0x35), 1));
        p.drawLine(QPointF(pa.left(), sy), QPointF(pa.right(), sy));
        p.setPen(QColor(0x70,0x70,0x70));
        p.drawText(QRectF(a.left(), sy-8, leftM-4, 16),
                   Qt::AlignRight|Qt::AlignVCenter, fmtNum(y));
    }

    double boxW = pa.width() / n * 0.5;
    double halfW = boxW / 2;
    auto sc = [&](double y) { return pa.bottom() - (y - m_yVMin) / (m_yVMax - m_yVMin) * pa.height(); };

    p.setClipRect(pa.adjusted(-1,-1,1,1));
    for (int i = 0; i < n; ++i) {
        auto& b = m_boxData[i];
        double cx = pa.left() + (i + 0.5) * pa.width() / n;
        double sMin = sc(b.min), sMax = sc(b.max);
        double sQ1  = sc(b.q1),  sQ3  = sc(b.q3);
        double sMed = sc(b.med);

        /* baffi */
        p.setPen(QPen(paletteColor(i), 1.5));
        p.drawLine(QPointF(cx, sMax), QPointF(cx, sQ3));
        p.drawLine(QPointF(cx, sQ1), QPointF(cx, sMin));
        p.drawLine(QPointF(cx-halfW*0.6, sMax), QPointF(cx+halfW*0.6, sMax));
        p.drawLine(QPointF(cx-halfW*0.6, sMin), QPointF(cx+halfW*0.6, sMin));

        /* box Q1-Q3 */
        QRectF box(cx-halfW, sQ3, boxW, sQ1-sQ3);
        QColor c = paletteColor(i);
        p.fillRect(box, QColor(c.red(), c.green(), c.blue(), 130));
        p.setPen(QPen(c, 1.5));
        p.drawRect(box);

        /* mediana */
        p.setPen(QPen(Qt::white, 2));
        p.drawLine(QPointF(cx-halfW, sMed), QPointF(cx+halfW, sMed));

        /* etichetta */
        p.setPen(QColor(0xaa,0xaa,0xaa));
        p.drawText(QRectF(cx-halfW*1.5, pa.bottom()+2, boxW*2, 14),
                   Qt::AlignCenter, b.label);
    }
    p.setClipping(false);
    p.setPen(QPen(QColor(0x50,0x50,0x50), 1));
    p.drawRect(pa);
}

/* ══════════════════════════════════════════════════════════════
   DotPlot — scatter 1D, stacking verticale
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::drawDotPlot(QPainter& p, const QRectF& a) {
    p.setRenderHint(QPainter::Antialiasing);
    if (m_values.isEmpty()) {
        p.setPen(QColor(0x77,0x77,0x77));
        p.drawText(a, Qt::AlignCenter, "Nessun dato\nFormato: etichetta:valore");
        return;
    }
    int n = m_values.size();
    double maxV = *std::max_element(m_values.begin(), m_values.end());
    double minV = *std::min_element(m_values.begin(), m_values.end());
    if (maxV <= minV) maxV = minV + 1;
    const double leftM = 60.0;
    QRectF pa(a.left()+leftM, a.top()+10, a.width()-leftM-10, a.height()-36);
    m_xVMin = 0; m_xVMax = n + 1;
    m_yVMin = minV - (maxV-minV)*0.1; m_yVMax = maxV + (maxV-minV)*0.1;

    /* griglia */
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 8));
    double yStep = niceStep(m_yVMax - m_yVMin, 6);
    double yFirst = std::ceil(m_yVMin / yStep) * yStep;
    for (double y = yFirst; y <= m_yVMax + yStep*0.01; y += yStep) {
        double sy = pa.bottom() - (y - m_yVMin) / (m_yVMax - m_yVMin) * pa.height();
        if (sy < pa.top() || sy > pa.bottom()) continue;
        p.setPen(QPen(QColor(0x35,0x35,0x35), 1));
        p.drawLine(QPointF(pa.left(), sy), QPointF(pa.right(), sy));
        p.setPen(QColor(0x70,0x70,0x70));
        p.drawText(QRectF(a.left(), sy-8, leftM-4, 16),
                   Qt::AlignRight|Qt::AlignVCenter, fmtNum(y));
    }
    p.setClipRect(pa.adjusted(-1,-1,1,1));
    double colW = pa.width() / (n + 1);
    for (int i = 0; i < n; ++i) {
        double sx = pa.left() + (i + 0.5) * colW + colW * 0.5;
        double sy = pa.bottom() - (m_values[i] - m_yVMin) / (m_yVMax - m_yVMin) * pa.height();
        QColor c = paletteColor(i);
        p.setBrush(c);
        p.setPen(QPen(c.darker(150), 1));
        p.drawEllipse(QPointF(sx, sy), 5.5, 5.5);
        /* etichetta */
        QString lbl = i < m_labels.size() ? m_labels[i] : QString::number(i+1);
        p.setPen(QColor(0x99,0x99,0x99));
        p.drawText(QRectF(sx-30, pa.bottom()+2, 60, 14), Qt::AlignCenter, lbl);
    }
    p.setClipping(false);
    p.setPen(QPen(QColor(0x50,0x50,0x50), 1));
    p.drawRect(pa);
}

/* ══════════════════════════════════════════════════════════════
   Density — KDE gaussiana (Silverman bandwidth)
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::drawDensity(QPainter& p, const QRectF& a) {
    p.setRenderHint(QPainter::Antialiasing);
    if (m_densityData.isEmpty()) {
        p.setPen(QColor(0x77,0x77,0x77));
        p.drawText(a, Qt::AlignCenter, "Nessun dato\nInserisci valori numerici");
        return;
    }
    const auto& d = m_densityData;
    int n = d.size();
    double mean = 0; for (double v : d) mean += v; mean /= n;
    double var  = 0; for (double v : d) var += (v-mean)*(v-mean); var /= n;
    double sigma = std::sqrt(var);
    if (sigma < 1e-10) sigma = 1.0;
    /* Silverman bandwidth */
    double h = 1.06 * sigma * std::pow((double)n, -0.2);

    double xMn = *std::min_element(d.begin(), d.end());
    double xMx = *std::max_element(d.begin(), d.end());
    double pad = (xMx - xMn) * 0.15 + 3*h;
    xMn -= pad; xMx += pad;
    m_xVMin = xMn; m_xVMax = xMx;

    /* valuta KDE in 200 punti */
    const int NP = 200;
    QVector<double> xs(NP), ys(NP);
    double yMx = 0;
    const double inv_h = 1.0 / h;
    const double norm = 1.0 / (n * h * std::sqrt(2.0 * M_PI));
    for (int k = 0; k < NP; ++k) {
        xs[k] = xMn + k * (xMx - xMn) / (NP - 1);
        double f = 0;
        for (double xi : d) {
            double u = (xs[k] - xi) * inv_h;
            f += std::exp(-0.5 * u * u);
        }
        ys[k] = f * norm;
        yMx = std::max(yMx, ys[k]);
    }
    m_yVMin = 0; m_yVMax = yMx * 1.15;

    drawGrid(p, a);
    p.setClipRect(a.adjusted(-1,-1,1,1));

    /* area riempita */
    QColor fc = paletteColor(0);
    QPainterPath areaPath;
    QPointF sp0(dataToScreen(xs[0], 0, a));
    areaPath.moveTo(sp0);
    for (int k = 0; k < NP; ++k) {
        QPointF sp = dataToScreen(xs[k], ys[k], a);
        areaPath.lineTo(sp);
    }
    areaPath.lineTo(dataToScreen(xs[NP-1], 0, a));
    areaPath.closeSubpath();
    p.fillPath(areaPath, QColor(fc.red(), fc.green(), fc.blue(), 60));

    /* curva */
    QPainterPath curvePath;
    curvePath.moveTo(dataToScreen(xs[0], ys[0], a));
    for (int k = 1; k < NP; ++k)
        curvePath.lineTo(dataToScreen(xs[k], ys[k], a));
    p.setPen(QPen(fc, 2));
    p.drawPath(curvePath);

    /* punti grezzi */
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(fc.red(), fc.green(), fc.blue(), 120));
    for (double v : d) {
        QPointF sp = dataToScreen(v, m_yVMin, a);
        p.drawEllipse(QPointF(sp.x(), a.bottom() - 6), 3.0, 3.0);
    }
    p.setClipping(false);
    p.setPen(QColor(0xcc,0xcc,0xcc));
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 8));
    p.drawText(QRectF(a.left(), a.top(), a.width(), 18),
               Qt::AlignRight|Qt::AlignVCenter,
               QString("n=%1  h=%2").arg(n).arg(fmtNum(h)));
}

/* ══════════════════════════════════════════════════════════════
   AreaStacked — area cumulativa impilata
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::drawAreaStacked(QPainter& p, const QRectF& a) {
    p.setRenderHint(QPainter::Antialiasing);
    if (m_lineSeries.isEmpty()) {
        p.setPen(QColor(0x66,0x88,0xaa));
        p.drawText(a, Qt::AlignCenter, "Nessun dato");
        return;
    }
    int nSeries = m_lineSeries.size();
    int nPts = 0;
    for (auto& s : m_lineSeries) nPts = std::max(nPts, (int)s.size());
    if (nPts == 0) return;
    /* calcola somme cumulative per ogni punto */
    QVector<QVector<double>> cumY(nSeries, QVector<double>(nPts, 0.0));
    for (int k = 0; k < nPts; ++k) {
        double acc = 0;
        for (int si = 0; si < nSeries; ++si) {
            if (si < m_lineSeries.size() && k < m_lineSeries[si].size())
                acc += std::max(0.0, m_lineSeries[si][k].y());
            cumY[si][k] = acc;
        }
    }
    double xMn = 1e18, xMx = -1e18, yMx = -1e18;
    for (auto& s : m_lineSeries) for (auto& pt : s) { xMn=std::min(xMn,pt.x()); xMx=std::max(xMx,pt.x()); }
    for (int si = 0; si < nSeries; ++si) for (double v : cumY[si]) yMx=std::max(yMx,v);
    if (xMn>=xMx){xMn-=1;xMx+=1;} if(yMx<=0) yMx=1;
    m_xVMin = xMn; m_xVMax = xMx;
    m_yVMin = 0;   m_yVMax = yMx * 1.1;

    drawGrid(p, a);
    p.setClipRect(a.adjusted(-1,-1,1,1));

    /* disegna dal top verso il basso per evitare overlap visivo */
    for (int si = nSeries-1; si >= 0; --si) {
        auto& s = m_lineSeries[si];
        QColor c = paletteColor(si);
        QPainterPath path;
        /* bordo superiore */
        for (int k = 0; k < (int)cumY[si].size() && k < s.size(); ++k) {
            QPointF sp = dataToScreen(s[k].x(), cumY[si][k], a);
            k == 0 ? path.moveTo(sp) : path.lineTo(sp);
        }
        /* bordo inferiore */
        double prevCumBase = si > 0 ? 1.0 : 0.0; (void)prevCumBase;
        for (int k = (int)cumY[si].size()-1; k >= 0; --k) {
            if (k >= s.size()) continue;
            double baseY = (si > 0 && k < (int)cumY[si-1].size()) ? cumY[si-1][k] : 0.0;
            path.lineTo(dataToScreen(s[k].x(), baseY, a));
        }
        path.closeSubpath();
        p.fillPath(path, QColor(c.red(), c.green(), c.blue(), 160));
        p.setPen(QPen(c.lighter(140), 1));
        p.drawPath(path);
    }
    p.setClipping(false);
    /* legenda */
    double lx = a.right() - 140, ly = a.top() + 8;
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 8));
    for (int si = 0; si < nSeries; ++si) {
        QColor c = paletteColor(si);
        p.fillRect(QRectF(lx, ly+si*16, 12, 10), QColor(c.red(), c.green(), c.blue(), 160));
        p.setPen(QColor(0xcc,0xcc,0xcc));
        QString name = si < m_lineNames.size() ? m_lineNames[si] : QString("Serie %1").arg(si+1);
        p.drawText(QRectF(lx+14, ly+si*16-2, 120, 14), Qt::AlignLeft|Qt::AlignVCenter, name);
    }
}

/* ══════════════════════════════════════════════════════════════
   OHLC — barre OHLC senza rettangolo pieno
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::drawOHLC(QPainter& p, const QRectF& a) {
    p.setRenderHint(QPainter::Antialiasing);
    if (m_ohlcPts.isEmpty()) {
        p.setPen(QColor(0x77,0x77,0x77));
        p.drawText(a, Qt::AlignCenter, "Nessun dato\nFormato: Label,O,H,L,C");
        return;
    }
    int n = m_ohlcPts.size();
    double yMn = m_ohlcPts[0].l, yMx = m_ohlcPts[0].h;
    for (auto& pt : m_ohlcPts) {
        yMn = std::min(yMn, pt.l); yMx = std::max(yMx, pt.h);
    }
    double pad = (yMx - yMn) * 0.12 + 0.5;
    m_xVMin = 0; m_xVMax = n;
    m_yVMin = yMn - pad; m_yVMax = yMx + pad;

    const double leftM = 70.0;
    QRectF pa(a.left()+leftM, a.top()+10, a.width()-leftM-10, a.height()-36);

    /* griglia */
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 8));
    double yStep = niceStep(m_yVMax - m_yVMin, 6);
    double yFirst = std::ceil(m_yVMin / yStep) * yStep;
    for (double y = yFirst; y <= m_yVMax + yStep*0.01; y += yStep) {
        double sy = pa.bottom() - (y - m_yVMin) / (m_yVMax - m_yVMin) * pa.height();
        if (sy < pa.top() || sy > pa.bottom()) continue;
        p.setPen(QPen(QColor(0x35,0x35,0x35), 1));
        p.drawLine(QPointF(pa.left(), sy), QPointF(pa.right(), sy));
        p.setPen(QColor(0x70,0x70,0x70));
        p.drawText(QRectF(a.left(), sy-8, leftM-4, 16),
                   Qt::AlignRight|Qt::AlignVCenter, fmtNum(y));
    }

    double barW = pa.width() / n;
    double tickW = barW * 0.28;
    auto sc = [&](double y) { return pa.bottom() - (y - m_yVMin) / (m_yVMax - m_yVMin) * pa.height(); };

    p.setClipRect(pa.adjusted(-1,-1,1,1));
    for (int i = 0; i < n; ++i) {
        auto& pt = m_ohlcPts[i];
        double cx = pa.left() + (i + 0.5) * barW;
        QColor c = pt.c >= pt.o ? QColor(0x73,0xe2,0x73) : QColor(0xff,0x79,0x5a);
        p.setPen(QPen(c, 1.5));
        /* linea H-L */
        p.drawLine(QPointF(cx, sc(pt.h)), QPointF(cx, sc(pt.l)));
        /* tick Open (sinistra) */
        p.drawLine(QPointF(cx-tickW, sc(pt.o)), QPointF(cx, sc(pt.o)));
        /* tick Close (destra) */
        p.drawLine(QPointF(cx, sc(pt.c)), QPointF(cx+tickW, sc(pt.c)));
        /* etichetta */
        p.setPen(QColor(0x88,0x88,0x88));
        p.drawText(QRectF(cx-barW*0.5, pa.bottom()+2, barW, 14),
                   Qt::AlignCenter, pt.label);
    }
    p.setClipping(false);
    p.setPen(QPen(QColor(0x50,0x50,0x50), 1));
    p.drawRect(pa);
}

/* ══════════════════════════════════════════════════════════════
   Gauge — semicerchio
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::drawGauge(QPainter& p, const QRectF& a) {
    p.setRenderHint(QPainter::Antialiasing);
    if (m_values.size() < 2) {
        p.setPen(QColor(0x77,0x77,0x77));
        p.drawText(a, Qt::AlignCenter, "Nessun dato\nFormato: valore:massimo\netichetta (opzionale)");
        return;
    }
    double val = m_values[0], maxV = m_values[1];
    if (maxV <= 0) maxV = 100;
    double frac = std::max(0.0, std::min(1.0, val / maxV));

    double cx = a.center().x() + m_panNC.x();
    double cy = a.center().y() * 0.7 + a.bottom() * 0.3 + m_panNC.y();
    double r  = std::min(a.width(), a.height()) * 0.38 * m_zoomNC;

    /* arco sfondo (da 180° a 0° — semicerchio) */
    p.setPen(QPen(QColor(0x40,0x40,0x40), r*0.22, Qt::SolidLine, Qt::FlatCap));
    p.drawArc(QRectF(cx-r, cy-r, 2*r, 2*r), 0 * 16, 180 * 16);

    /* arco colorato */
    QColor arcCol = frac < 0.5 ? QColor(0x73,0xe2,0x73)
                  : frac < 0.8 ? QColor(0xff,0xd7,0x6e)
                  :               QColor(0xff,0x79,0x5a);
    p.setPen(QPen(arcCol, r*0.20, Qt::SolidLine, Qt::FlatCap));
    int spanDeg = (int)(180.0 * frac);
    p.drawArc(QRectF(cx-r, cy-r, 2*r, 2*r), 0 * 16, spanDeg * 16);

    /* tacche di scala */
    p.setPen(QPen(QColor(0x88,0x88,0x88), 1.5));
    for (int i = 0; i <= 10; ++i) {
        double ang = M_PI - (M_PI * i / 10.0); /* 180° → 0° */
        double r1 = r * 0.88, r2 = r * (i % 5 == 0 ? 0.72 : 0.80);
        p.drawLine(QPointF(cx + r1*std::cos(ang), cy - r1*std::sin(ang)),
                   QPointF(cx + r2*std::cos(ang), cy - r2*std::sin(ang)));
    }

    /* valore al centro */
    p.setPen(QColor(0xee,0xee,0xee));
    p.setFont(QFont("Inter,Ubuntu,sans-serif", (int)(r*0.22), QFont::Bold));
    p.drawText(QRectF(cx-r, cy-r*0.3, 2*r, r*0.6), Qt::AlignCenter, fmtNum(val));

    /* etichetta */
    QString lbl = m_labels.isEmpty() ? "" : m_labels[0];
    if (!lbl.isEmpty()) {
        p.setPen(QColor(0x99,0x99,0x99));
        p.setFont(QFont("Inter,Ubuntu,sans-serif", 10));
        p.drawText(QRectF(cx-r, cy+r*0.15, 2*r, r*0.4), Qt::AlignCenter, lbl);
    }
    /* min/max */
    p.setPen(QColor(0x77,0x77,0x77));
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 9));
    p.drawText(QRectF(cx-r-10, cy+4, r*0.5, 18), Qt::AlignCenter, "0");
    p.drawText(QRectF(cx+r*0.6, cy+4, r*0.5, 18), Qt::AlignCenter, fmtNum(maxV));
}

/* ══════════════════════════════════════════════════════════════
   Bullet Chart
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::drawBullet(QPainter& p, const QRectF& a) {
    p.setRenderHint(QPainter::Antialiasing);
    if (m_bulletData.isEmpty()) {
        p.setPen(QColor(0x77,0x77,0x77));
        p.drawText(a, Qt::AlignCenter, "Nessun dato\nFormato: label: valore, target, min, max");
        return;
    }
    int n = m_bulletData.size();
    const double leftM = 110.0;
    double rowH = a.height() / n;
    double barH = rowH * 0.35;
    double gap = rowH * 0.32;
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 9));
    for (int i = 0; i < n; ++i) {
        auto& bb = m_bulletData[i];
        double maxW = a.width() - leftM - 10;
        double scale = bb.rangeHigh > bb.rangeLow ? maxW / (bb.rangeHigh - bb.rangeLow) : maxW;
        double barY = a.top() + i*rowH + gap;
        double barX = a.left() + leftM;
        /* zona cattivo (rosso scuro) */
        double midX = barX + (bb.rangeLow + (bb.rangeHigh - bb.rangeLow) * 0.4) * scale / bb.rangeHigh;
        (void)midX;
        /* sfondo 3 zone */
        double z1 = bb.rangeHigh > 0 ? (bb.rangeHigh - bb.rangeLow) * 0.33 * scale / bb.rangeHigh : maxW*0.33;
        double z2 = bb.rangeHigh > 0 ? (bb.rangeHigh - bb.rangeLow) * 0.66 * scale / bb.rangeHigh : maxW*0.66;
        (void)z1; (void)z2;
        double total = bb.rangeHigh > bb.rangeLow ? bb.rangeHigh - bb.rangeLow : 1;
        double w1 = total * 0.33 * scale / total;
        double w2 = total * 0.33 * scale / total;
        double w3 = (total - total * 0.66) * scale / total;
        p.fillRect(QRectF(barX,       barY, w1, barH), QColor(0x50,0x20,0x20));
        p.fillRect(QRectF(barX+w1,    barY, w2, barH), QColor(0x50,0x40,0x20));
        p.fillRect(QRectF(barX+w1+w2, barY, w3, barH), QColor(0x20,0x40,0x20));
        /* barra valore */
        double valW = bb.rangeHigh > bb.rangeLow
                      ? (bb.value - bb.rangeLow) / total * maxW
                      : bb.value / (bb.rangeHigh > 0 ? bb.rangeHigh : 1) * maxW;
        valW = std::max(0.0, std::min(valW, maxW));
        double bh2 = barH * 0.5;
        double by2 = barY + (barH - bh2) / 2;
        p.fillRect(QRectF(barX, by2, valW, bh2), paletteColor(0));
        /* linea target */
        double tgtX = bb.rangeHigh > bb.rangeLow
                      ? barX + (bb.target - bb.rangeLow) / total * maxW
                      : barX + bb.target / (bb.rangeHigh > 0 ? bb.rangeHigh : 1) * maxW;
        tgtX = std::max(barX, std::min(tgtX, barX + maxW));
        p.setPen(QPen(Qt::white, 2));
        p.drawLine(QPointF(tgtX, barY+1), QPointF(tgtX, barY+barH-1));
        /* etichetta */
        p.setPen(QColor(0xcc,0xcc,0xcc));
        p.drawText(QRectF(a.left(), barY, leftM-4, barH),
                   Qt::AlignRight|Qt::AlignVCenter, bb.label);
    }
}

/* ══════════════════════════════════════════════════════════════
   Gantt
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::drawGantt(QPainter& p, const QRectF& a) {
    p.setRenderHint(QPainter::Antialiasing);
    if (m_ganttData.isEmpty()) {
        p.setPen(QColor(0x77,0x77,0x77));
        p.drawText(a, Qt::AlignCenter, "Nessun dato\nFormato: task: inizio, fine [, cat]");
        return;
    }
    int n = m_ganttData.size();
    double xMn = m_ganttData[0].start, xMx = m_ganttData[0].end;
    for (auto& t : m_ganttData) {
        xMn = std::min(xMn, t.start); xMx = std::max(xMx, t.end);
    }
    if (xMn >= xMx) xMx = xMn + 1;
    const double leftM = 110.0;
    QRectF pa(a.left()+leftM, a.top()+10, a.width()-leftM-10, a.height()-30);
    double rowH = pa.height() / n;
    double barH = rowH * 0.55;
    double gap  = (rowH - barH) / 2;

    /* griglia verticale */
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 8));
    double xStep = niceStep(xMx - xMn, 8);
    double xFirst = std::ceil(xMn / xStep) * xStep;
    for (double x = xFirst; x <= xMx + xStep*0.01; x += xStep) {
        double sx = pa.left() + (x - xMn) / (xMx - xMn) * pa.width();
        p.setPen(QPen(QColor(0x35,0x35,0x35), 1));
        p.drawLine(QPointF(sx, pa.top()), QPointF(sx, pa.bottom()));
        p.setPen(QColor(0x70,0x70,0x70));
        p.drawText(QRectF(sx-26, pa.bottom()+2, 52, 14), Qt::AlignCenter, fmtNum(x));
    }
    /* raccoglie categorie per colori */
    QStringList cats;
    for (auto& t : m_ganttData) if (!t.cat.isEmpty() && !cats.contains(t.cat)) cats.append(t.cat);

    p.setClipRect(pa.adjusted(-1,-1,1,1));
    for (int i = 0; i < n; ++i) {
        auto& t = m_ganttData[i];
        double bx1 = pa.left() + (t.start - xMn) / (xMx - xMn) * pa.width();
        double bx2 = pa.left() + (t.end   - xMn) / (xMx - xMn) * pa.width();
        double by  = pa.top() + i*rowH + gap;
        int ci = cats.indexOf(t.cat);
        QRectF r(bx1, by, bx2-bx1, barH);
        p.fillRect(r, paletteColor(ci >= 0 ? ci : i));
        p.setPen(QPen(QColor(0x18,0x18,0x18), 1));
        p.drawRect(r);
        /* etichetta sinistra */
        p.setPen(QColor(0xcc,0xcc,0xcc));
        p.drawText(QRectF(a.left(), by, leftM-4, barH),
                   Qt::AlignRight|Qt::AlignVCenter, t.name);
    }
    p.setClipping(false);
    p.setPen(QPen(QColor(0x50,0x50,0x50), 1));
    p.drawRect(pa);
}

/* ══════════════════════════════════════════════════════════════
   Pyramid — piramide demografica (due serie speculari)
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::drawPyramid(QPainter& p, const QRectF& a) {
    p.setRenderHint(QPainter::Antialiasing);
    if (m_lineSeries.isEmpty()) {
        p.setPen(QColor(0x77,0x77,0x77));
        p.drawText(a, Qt::AlignCenter, "Nessun dato\n# Sinistra Destra\nCat1  v1  v2");
        return;
    }
    int nSeries = std::min((int)m_lineSeries.size(), 2);
    int nRows = 0;
    for (auto& s : m_lineSeries) nRows = std::max(nRows, (int)s.size());
    if (nRows == 0) return;
    double maxV = 0;
    for (int si = 0; si < nSeries; ++si) for (auto& pt : m_lineSeries[si]) maxV = std::max(maxV, pt.y());
    if (maxV <= 0) maxV = 1;
    const double labelW = 60.0;
    double halfW = (a.width() - labelW) / 2;
    double cx = a.left() + halfW;
    double rowH = a.height() / nRows;
    double barH = rowH * 0.75;
    double gap  = (rowH - barH) / 2;

    p.setFont(QFont("Inter,Ubuntu,sans-serif", 8));
    p.setClipRect(a.adjusted(-1,-1,1,1));
    for (int i = 0; i < nRows; ++i) {
        double by = a.top() + i*rowH + gap;
        /* sinistra */
        if (nSeries > 0 && i < m_lineSeries[0].size()) {
            double bw = (m_lineSeries[0][i].y() / maxV) * halfW * 0.9;
            QRectF r(cx - bw, by, bw, barH);
            p.fillRect(r, paletteColor(0));
            p.setPen(QPen(QColor(0x18,0x18,0x18), 1));
            p.drawRect(r);
        }
        /* destra */
        if (nSeries > 1 && i < m_lineSeries[1].size()) {
            double bw = (m_lineSeries[1][i].y() / maxV) * halfW * 0.9;
            QRectF r(cx, by, bw, barH);
            p.fillRect(r, paletteColor(1));
            p.setPen(QPen(QColor(0x18,0x18,0x18), 1));
            p.drawRect(r);
        }
        /* etichetta centrale */
        p.setPen(QColor(0xcc,0xcc,0xcc));
        /* usa y come indice di riga (label da m_lineNames o numero) */
        QString lbl = (i < m_lineNames.size()) ? m_lineNames[i] : QString::number(i+1);
        p.drawText(QRectF(cx - labelW/2, by, labelW, barH),
                   Qt::AlignCenter, lbl);
    }
    p.setClipping(false);
    /* legenda */
    double lx = a.right() - 130, ly = a.top() + 4;
    for (int si = 0; si < nSeries; ++si) {
        p.fillRect(QRectF(lx, ly+si*16, 10, 10), paletteColor(si));
        p.setPen(QColor(0xcc,0xcc,0xcc));
        QString name = (si < (int)m_lineNames.size() && si < 2)
                       ? (si == 0 ? "Sinistra" : "Destra")
                       : QString("Serie %1").arg(si+1);
        p.drawText(QRectF(lx+13, ly+si*16-2, 115, 14), Qt::AlignLeft|Qt::AlignVCenter, name);
    }
}
