/*
 * main_graph_canvas_bars.cpp — GraficoCanvas: bar/proporzioni
 * ================================================================
 * Column, HBar, GroupedBar, StackedBar, StackedBar100, Funnel, Donut,
 * Treemap, Sunburst. Split da main_graph_canvas.cpp (TODO D-8).
 */
#include "main_graph.h"
#include "main_graph_canvas_p.h"

#include <QPainter>
#include <QPainterPath>
#include <QFont>
#include <cmath>
#include <algorithm>


/* ══════════════════════════════════════════════════════════════
   Column — barre verticali per categorie
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::drawColumn(QPainter& p, const QRectF& a) {
    p.setRenderHint(QPainter::Antialiasing);
    if (m_values.isEmpty()) {
        p.setPen(QColor(0x77,0x77,0x77));
        p.drawText(a, Qt::AlignCenter, "Nessun dato\nFormato: etichetta:valore");
        return;
    }
    int n = m_values.size();
    double maxV = *std::max_element(m_values.begin(), m_values.end());
    if (maxV <= 0) maxV = 1;
    const double leftM = 70.0;
    QRectF pa(a.left()+leftM, a.top()+10, a.width()-leftM-10, a.height()-36);

    /* griglia orizzontale */
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 8));
    for (int i = 0; i <= 5; ++i) {
        double frac = (double)i / 5;
        double sy = pa.bottom() - frac * pa.height();
        p.setPen(QPen(QColor(0x35,0x35,0x35), 1));
        p.drawLine(QPointF(pa.left(), sy), QPointF(pa.right(), sy));
        p.setPen(QColor(0x70,0x70,0x70));
        double lv = maxV * frac;
        p.drawText(QRectF(a.left(), sy-8, leftM-4, 16),
                   Qt::AlignRight | Qt::AlignVCenter, fmtNum(lv));
    }
    double barW = pa.width() / n;
    double gap = barW * 0.15;
    double bw = barW - 2*gap;
    p.setClipRect(pa.adjusted(-1,-1,1,1));
    for (int i = 0; i < n; ++i) {
        double bh = (m_values[i] / maxV) * pa.height();
        double bx = pa.left() + i*barW + gap;
        double by = pa.bottom() - bh;
        QRectF r(bx, by, bw, bh);
        p.fillRect(r, paletteColor(i));
        p.setPen(QPen(QColor(0x18,0x18,0x18), 1));
        p.drawRect(r);
        /* valore sopra */
        p.setPen(QColor(0xcc,0xcc,0xcc));
        p.drawText(QRectF(bx-2, by-14, bw+4, 13), Qt::AlignCenter, fmtNum(m_values[i]));
        /* etichetta sotto — obliqua se >8 categorie */
        QString lbl = i < m_labels.size() ? m_labels[i] : QString::number(i+1);
        if (n > 8) {
            p.save();
            p.translate(bx + bw/2, pa.bottom()+4);
            p.rotate(35);
            p.drawText(QRectF(0, 0, 60, 14), Qt::AlignLeft, lbl);
            p.restore();
        } else {
            p.drawText(QRectF(bx, pa.bottom()+2, bw, 16), Qt::AlignCenter, lbl);
        }
    }
    p.setClipping(false);
    p.setPen(QPen(QColor(0x50,0x50,0x50), 1));
    p.drawRect(pa);
}

/* ══════════════════════════════════════════════════════════════
   HBar — barre orizzontali
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::drawHBar(QPainter& p, const QRectF& a) {
    p.setRenderHint(QPainter::Antialiasing);
    if (m_values.isEmpty()) {
        p.setPen(QColor(0x77,0x77,0x77));
        p.drawText(a, Qt::AlignCenter, "Nessun dato\nFormato: etichetta:valore");
        return;
    }
    int n = m_values.size();
    double maxV = *std::max_element(m_values.begin(), m_values.end());
    if (maxV <= 0) maxV = 1;
    const double leftM = 120.0;
    QRectF pa(a.left()+leftM, a.top()+10, a.width()-leftM-60, a.height()-20);
    double barH = pa.height() / n;
    double gap = barH * 0.15;
    double bh = barH - 2*gap;
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 8));
    p.setClipRect(pa.adjusted(-1,-1,60,1));
    for (int i = 0; i < n; ++i) {
        double bw = (m_values[i] / maxV) * pa.width();
        double by = pa.top() + i*barH + gap;
        QRectF r(pa.left(), by, bw, bh);
        p.fillRect(r, paletteColor(i));
        p.setPen(QPen(QColor(0x18,0x18,0x18), 1));
        p.drawRect(r);
        /* etichetta a sinistra */
        p.setPen(QColor(0xcc,0xcc,0xcc));
        QString lbl = i < m_labels.size() ? m_labels[i] : QString::number(i+1);
        p.drawText(QRectF(a.left(), by, leftM-4, bh),
                   Qt::AlignRight | Qt::AlignVCenter, lbl);
        /* valore a destra */
        p.drawText(QRectF(pa.left()+bw+4, by, 55, bh),
                   Qt::AlignLeft | Qt::AlignVCenter, fmtNum(m_values[i]));
    }
    p.setClipping(false);
    p.setPen(QPen(QColor(0x50,0x50,0x50), 1));
    p.drawRect(pa);
}

/* ══════════════════════════════════════════════════════════════
   GroupedBar — barre raggruppate multi-serie
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::drawGroupedBar(QPainter& p, const QRectF& a) {
    p.setRenderHint(QPainter::Antialiasing);
    if (m_lineSeries.isEmpty()) {
        p.setPen(QColor(0x77,0x77,0x77));
        p.drawText(a, Qt::AlignCenter, "Nessun dato\n# Serie1 Serie2\nCat1  v1  v2");
        return;
    }
    int nSeries = m_lineSeries.size();
    int nGroups = 0;
    for (auto& s : m_lineSeries) nGroups = std::max(nGroups, (int)s.size());
    if (nGroups == 0) return;
    double maxV = -1e18;
    for (auto& s : m_lineSeries) for (auto& pt : s) maxV = std::max(maxV, pt.y());
    if (maxV <= 0) maxV = 1;
    const double leftM = 60.0;
    QRectF pa(a.left()+leftM, a.top()+10, a.width()-leftM-10, a.height()-36);

    /* griglia */
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 8));
    for (int i = 0; i <= 5; ++i) {
        double frac = (double)i / 5;
        double sy = pa.bottom() - frac * pa.height();
        p.setPen(QPen(QColor(0x35,0x35,0x35), 1));
        p.drawLine(QPointF(pa.left(), sy), QPointF(pa.right(), sy));
        p.setPen(QColor(0x70,0x70,0x70));
        p.drawText(QRectF(a.left(), sy-8, leftM-4, 16),
                   Qt::AlignRight | Qt::AlignVCenter, fmtNum(maxV * frac));
    }
    double groupW = pa.width() / nGroups;
    double barW = groupW * 0.85 / nSeries;
    double groupGap = groupW * 0.075;
    p.setClipRect(pa.adjusted(-1,-1,1,1));
    for (int g = 0; g < nGroups; ++g) {
        for (int si = 0; si < nSeries; ++si) {
            if (si >= m_lineSeries.size() || g >= m_lineSeries[si].size()) continue;
            double val = m_lineSeries[si][g].y();
            double bh = (val / maxV) * pa.height();
            double bx = pa.left() + g*groupW + groupGap + si*barW;
            double by = pa.bottom() - bh;
            QRectF r(bx, by, barW, bh);
            p.fillRect(r, paletteColor(si));
            p.setPen(QPen(QColor(0x18,0x18,0x18), 1));
            p.drawRect(r);
        }
        /* etichetta gruppo */
        p.setPen(QColor(0x99,0x99,0x99));
        p.drawText(QRectF(pa.left()+g*groupW, pa.bottom()+2, groupW, 14),
                   Qt::AlignCenter, QString::number(g+1));
    }
    p.setClipping(false);
    /* legenda */
    double lx = pa.right() - 130, ly = pa.top() + 4;
    for (int si = 0; si < nSeries; ++si) {
        p.fillRect(QRectF(lx, ly+si*16, 10, 10), paletteColor(si));
        p.setPen(QColor(0xcc,0xcc,0xcc));
        QString name = si < m_lineNames.size() ? m_lineNames[si] : QString("Serie %1").arg(si+1);
        p.drawText(QRectF(lx+13, ly+si*16-2, 115, 14), Qt::AlignLeft|Qt::AlignVCenter, name);
    }
    p.setPen(QPen(QColor(0x50,0x50,0x50), 1));
    p.drawRect(pa);
}

/* ══════════════════════════════════════════════════════════════
   StackedBar — barre impilate
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::drawStackedBar(QPainter& p, const QRectF& a) {
    p.setRenderHint(QPainter::Antialiasing);
    if (m_lineSeries.isEmpty()) {
        p.setPen(QColor(0x77,0x77,0x77));
        p.drawText(a, Qt::AlignCenter, "Nessun dato\n# Serie1 Serie2\nCat1  v1  v2");
        return;
    }
    int nSeries = m_lineSeries.size();
    int nGroups = 0;
    for (auto& s : m_lineSeries) nGroups = std::max(nGroups, (int)s.size());
    if (nGroups == 0) return;
    /* somme per ogni gruppo */
    QVector<double> totals(nGroups, 0.0);
    for (auto& s : m_lineSeries)
        for (int g = 0; g < s.size(); ++g)
            totals[g] += std::max(0.0, s[g].y());
    double maxV = *std::max_element(totals.begin(), totals.end());
    if (maxV <= 0) maxV = 1;
    const double leftM = 60.0;
    QRectF pa(a.left()+leftM, a.top()+10, a.width()-leftM-10, a.height()-36);

    /* griglia */
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 8));
    for (int i = 0; i <= 5; ++i) {
        double frac = (double)i / 5;
        double sy = pa.bottom() - frac * pa.height();
        p.setPen(QPen(QColor(0x35,0x35,0x35), 1));
        p.drawLine(QPointF(pa.left(), sy), QPointF(pa.right(), sy));
        p.setPen(QColor(0x70,0x70,0x70));
        p.drawText(QRectF(a.left(), sy-8, leftM-4, 16),
                   Qt::AlignRight | Qt::AlignVCenter, fmtNum(maxV * frac));
    }
    double barW = pa.width() / nGroups;
    double gap = barW * 0.15;
    double bw = barW - 2*gap;
    p.setClipRect(pa.adjusted(-1,-1,1,1));
    for (int g = 0; g < nGroups; ++g) {
        double cumY = pa.bottom();
        for (int si = 0; si < nSeries; ++si) {
            if (si >= m_lineSeries.size() || g >= m_lineSeries[si].size()) continue;
            double val = std::max(0.0, m_lineSeries[si][g].y());
            double segH = (val / maxV) * pa.height();
            double bx = pa.left() + g*barW + gap;
            double by = cumY - segH;
            QRectF r(bx, by, bw, segH);
            p.fillRect(r, paletteColor(si));
            p.setPen(QPen(QColor(0x18,0x18,0x18), 1));
            p.drawRect(r);
            cumY = by;
        }
        p.setPen(QColor(0x99,0x99,0x99));
        p.drawText(QRectF(pa.left()+g*barW, pa.bottom()+2, barW, 14),
                   Qt::AlignCenter, QString::number(g+1));
    }
    p.setClipping(false);
    /* legenda */
    double lx = pa.right() - 130, ly = pa.top() + 4;
    for (int si = 0; si < nSeries; ++si) {
        p.fillRect(QRectF(lx, ly+si*16, 10, 10), paletteColor(si));
        p.setPen(QColor(0xcc,0xcc,0xcc));
        QString name = si < m_lineNames.size() ? m_lineNames[si] : QString("Serie %1").arg(si+1);
        p.drawText(QRectF(lx+13, ly+si*16-2, 115, 14), Qt::AlignLeft|Qt::AlignVCenter, name);
    }
    p.setPen(QPen(QColor(0x50,0x50,0x50), 1));
    p.drawRect(pa);
}

/* ══════════════════════════════════════════════════════════════
   StackedBar100 — barre impilate normalizzate 100%
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::drawStackedBar100(QPainter& p, const QRectF& a) {
    p.setRenderHint(QPainter::Antialiasing);
    if (m_lineSeries.isEmpty()) {
        p.setPen(QColor(0x77,0x77,0x77));
        p.drawText(a, Qt::AlignCenter, "Nessun dato\n# Serie1 Serie2\nCat1  v1  v2");
        return;
    }
    int nSeries = m_lineSeries.size();
    int nGroups = 0;
    for (auto& s : m_lineSeries) nGroups = std::max(nGroups, (int)s.size());
    if (nGroups == 0) return;
    QVector<double> totals(nGroups, 0.0);
    for (auto& s : m_lineSeries)
        for (int g = 0; g < s.size(); ++g)
            totals[g] += std::max(0.0, s[g].y());
    const double leftM = 60.0;
    QRectF pa(a.left()+leftM, a.top()+10, a.width()-leftM-10, a.height()-36);

    /* griglia 0-100% */
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 8));
    for (int i = 0; i <= 5; ++i) {
        double frac = (double)i / 5;
        double sy = pa.bottom() - frac * pa.height();
        p.setPen(QPen(QColor(0x35,0x35,0x35), 1));
        p.drawLine(QPointF(pa.left(), sy), QPointF(pa.right(), sy));
        p.setPen(QColor(0x70,0x70,0x70));
        p.drawText(QRectF(a.left(), sy-8, leftM-4, 16),
                   Qt::AlignRight | Qt::AlignVCenter, QString::number((int)(frac*100)) + "%");
    }
    double barW = pa.width() / nGroups;
    double gap = barW * 0.15;
    double bw = barW - 2*gap;
    p.setClipRect(pa.adjusted(-1,-1,1,1));
    for (int g = 0; g < nGroups; ++g) {
        double total = totals[g] > 0 ? totals[g] : 1.0;
        double cumY = pa.bottom();
        for (int si = 0; si < nSeries; ++si) {
            if (si >= m_lineSeries.size() || g >= m_lineSeries[si].size()) continue;
            double val = std::max(0.0, m_lineSeries[si][g].y());
            double segH = (val / total) * pa.height();
            double bx = pa.left() + g*barW + gap;
            double by = cumY - segH;
            QRectF r(bx, by, bw, segH);
            p.fillRect(r, paletteColor(si));
            p.setPen(QPen(QColor(0x18,0x18,0x18), 1));
            p.drawRect(r);
            if (segH > 14) {
                p.setPen(Qt::white);
                p.drawText(r, Qt::AlignCenter,
                           QString::number((int)(val/total*100)) + "%");
            }
            cumY = by;
        }
        p.setPen(QColor(0x99,0x99,0x99));
        p.drawText(QRectF(pa.left()+g*barW, pa.bottom()+2, barW, 14),
                   Qt::AlignCenter, QString::number(g+1));
    }
    p.setClipping(false);
    /* legenda */
    double lx = pa.right() - 130, ly = pa.top() + 4;
    for (int si = 0; si < nSeries; ++si) {
        p.fillRect(QRectF(lx, ly+si*16, 10, 10), paletteColor(si));
        p.setPen(QColor(0xcc,0xcc,0xcc));
        QString name = si < m_lineNames.size() ? m_lineNames[si] : QString("Serie %1").arg(si+1);
        p.drawText(QRectF(lx+13, ly+si*16-2, 115, 14), Qt::AlignLeft|Qt::AlignVCenter, name);
    }
    p.setPen(QPen(QColor(0x50,0x50,0x50), 1));
    p.drawRect(pa);
}

/* ══════════════════════════════════════════════════════════════
   Funnel — imbuto a trapezi orizzontali
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::drawFunnel(QPainter& p, const QRectF& a) {
    p.setRenderHint(QPainter::Antialiasing);
    if (m_values.isEmpty()) {
        p.setPen(QColor(0x77,0x77,0x77));
        p.drawText(a, Qt::AlignCenter, "Nessun dato\nFormato: etichetta:valore");
        return;
    }
    int n = m_values.size();
    double maxV = *std::max_element(m_values.begin(), m_values.end());
    if (maxV <= 0) maxV = 1;
    double total = 0; for (double v : m_values) total += v;
    double segH = a.height() / n - 2;
    double cx = a.center().x();
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 9));
    for (int i = 0; i < n; ++i) {
        double halfW = (m_values[i] / maxV) * a.width() * 0.45;
        double y0 = a.top() + i * (segH + 2);
        double y1 = y0 + segH;
        QPolygonF trap;
        trap << QPointF(cx - halfW, y0) << QPointF(cx + halfW, y0)
             << QPointF(cx + halfW, y1) << QPointF(cx - halfW, y1);
        QColor c = paletteColor(i);
        p.setBrush(QColor(c.red(), c.green(), c.blue(), 200));
        p.setPen(QPen(QColor(0x18,0x18,0x18), 1));
        p.drawPolygon(trap);
        /* etichetta + % */
        QString lbl = (i < m_labels.size() ? m_labels[i] : QString::number(i+1));
        QString pct = total > 0 ? " (" + QString::number(m_values[i]/total*100,'f',1) + "%)" : "";
        p.setPen(Qt::white);
        p.drawText(QRectF(cx-halfW, y0, 2*halfW, segH),
                   Qt::AlignCenter, lbl + pct);
    }
}

/* ══════════════════════════════════════════════════════════════
   Donut — torta con foro centrale
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::drawDonut(QPainter& p, const QRectF& a) {
    if (m_values.isEmpty()) {
        p.setPen(QColor(0x77,0x77,0x77));
        p.drawText(a, Qt::AlignCenter, "Nessun dato\nFormato: etichetta:valore");
        return;
    }
    double total = 0;
    for (double v : m_values) if (v > 0) total += v;
    if (total <= 0) return;
    double cx = a.center().x() + m_panNC.x();
    double cy = a.center().y() + m_panNC.y();
    double r  = std::min(a.width(), a.height()) * 0.36 * m_zoomNC;
    QRectF pr(cx-r, cy-r, 2*r, 2*r);

    p.setRenderHint(QPainter::Antialiasing);
    int startA = 90 * 16;
    for (int i = 0; i < m_values.size(); i++) {
        if (m_values[i] <= 0) continue;
        int spanA = -qRound(m_values[i] / total * 5760.0);
        p.setBrush(paletteColor(i));
        p.setPen(QPen(QColor(0x18,0x18,0x18), 2));
        p.drawPie(pr, startA, spanA);
        double midDeg = (startA + spanA / 2.0) / 16.0;
        double midRad = midDeg * M_PI / 180.0;
        double lx = cx + r * 0.72 * std::cos(midRad);
        double ly = cy - r * 0.72 * std::sin(midRad);
        QString lbl = i < m_labels.size() ? m_labels[i] : QString::number(i+1);
        p.setPen(Qt::white);
        p.setFont(QFont("Inter,Ubuntu,sans-serif", 8));
        p.drawText(QRectF(lx-34, ly-8, 68, 16), Qt::AlignCenter, lbl);
        startA += spanA;
    }
    /* foro centrale */
    double ri = r * 0.40;
    p.setBrush(QColor(0x18,0x18,0x18));
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPointF(cx, cy), ri, ri);
    /* totale al centro */
    p.setPen(QColor(0xcc,0xcc,0xcc));
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 10, QFont::Bold));
    p.drawText(QRectF(cx-ri+4, cy-12, 2*ri-8, 24), Qt::AlignCenter, fmtNum(total));
    /* legenda */
    double lx2 = a.right() - 120, ly2 = a.top() + 10;
    for (int i = 0; i < m_values.size() && i < 14; i++) {
        p.fillRect(QRectF(lx2, ly2+i*18, 11, 11), paletteColor(i));
        p.setPen(QColor(0xcc,0xcc,0xcc));
        p.setFont(QFont("Inter,Ubuntu,sans-serif", 8));
        QString lbl = i < m_labels.size() ? m_labels[i] : QString::number(i+1);
        p.drawText(QRectF(lx2+14, ly2+i*18-2, 105, 16),
                   Qt::AlignLeft|Qt::AlignVCenter, lbl);
    }
}

/* ══════════════════════════════════════════════════════════════
   Treemap — mappa ad albero (slice-and-dice)
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::drawTreemap(QPainter& p, const QRectF& a) {
    p.setRenderHint(QPainter::Antialiasing);
    if (m_values.isEmpty()) {
        p.setPen(QColor(0x77,0x77,0x77));
        p.drawText(a, Qt::AlignCenter, "Nessun dato\nFormato: etichetta:valore");
        return;
    }
    double total = 0; for (double v : m_values) if (v > 0) total += v;
    if (total <= 0) return;
    /* slice-and-dice: divide orizzontalmente */
    double y = a.top();
    for (int i = 0; i < m_values.size(); ++i) {
        if (m_values[i] <= 0) continue;
        double h = (m_values[i] / total) * a.height();
        QRectF r(a.left(), y, a.width(), h);
        QColor c = paletteColor(i);
        p.fillRect(r, QColor(c.red(), c.green(), c.blue(), 180));
        p.setPen(QPen(QColor(0x18,0x18,0x18), 1));
        p.drawRect(r);
        QString lbl = (i < m_labels.size() ? m_labels[i] : QString::number(i+1))
                      + "\n" + fmtNum(m_values[i]);
        p.setPen(Qt::white);
        p.setFont(QFont("Inter,Ubuntu,sans-serif", 9, QFont::Bold));
        p.drawText(r.adjusted(4,4,-4,-4), Qt::AlignCenter | Qt::TextWordWrap, lbl);
        y += h;
    }
}

/* ══════════════════════════════════════════════════════════════
   Sunburst — anelli concentrici a due livelli
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::drawSunburst(QPainter& p, const QRectF& a) {
    p.setRenderHint(QPainter::Antialiasing);
    if (m_sunburstData.isEmpty()) {
        p.setPen(QColor(0x77,0x77,0x77));
        p.drawText(a, Qt::AlignCenter, "Nessun dato\nFormato: Cat/Sub: valore");
        return;
    }
    /* raggruppa per prefisso prima di '/' */
    QMap<QString, double> catTotals;
    QMap<QString, QVector<QPair<QString,double>>> catSubs;
    for (auto& kv : m_sunburstData) {
        int sl = kv.first.indexOf('/');
        QString cat = sl > 0 ? kv.first.left(sl) : kv.first;
        QString sub = sl > 0 ? kv.first.mid(sl+1) : kv.first;
        catTotals[cat] += kv.second;
        catSubs[cat].append({sub, kv.second});
    }
    double total = 0; for (auto v : catTotals) total += v;
    if (total <= 0) return;
    double cx = a.center().x() + m_panNC.x();
    double cy = a.center().y() + m_panNC.y();
    double maxR = std::min(a.width(), a.height()) * 0.42 * m_zoomNC;
    double innerR = maxR * 0.42;
    double midR   = maxR * 0.68;

    /* anello interno: categorie */
    int catIdx = 0;
    double catStart = 90.0 * 16;
    QStringList catKeys = catTotals.keys();
    for (const QString& cat : catKeys) {
        double span = -(catTotals[cat] / total) * 5760.0;
        QRectF rIn(cx-innerR, cy-innerR, 2*innerR, 2*innerR);
        p.setBrush(paletteColor(catIdx));
        p.setPen(QPen(QColor(0x18,0x18,0x18), 1));
        p.drawPie(rIn, (int)catStart, (int)span);
        catStart += span;
        ++catIdx;
    }
    /* foro */
    p.setBrush(QColor(0x18,0x18,0x18));
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPointF(cx, cy), innerR * 0.5, innerR * 0.5);

    /* anello esterno: sottocategorie */
    catIdx = 0;
    catStart = 90.0 * 16;
    for (const QString& cat : catKeys) {
        double catFrac = catTotals[cat] / total;
        double catSpanTotal = -catFrac * 5760.0;
        double subStart = catStart;
        const auto& subs = catSubs[cat];
        double subSum = 0; for (auto& kv : subs) subSum += kv.second;
        int subIdx = 0;
        for (auto& kv : subs) {
            double subSpan = -( kv.second / (subSum > 0 ? subSum : 1) ) * catSpanTotal;
            QRectF rOut(cx-maxR, cy-maxR, 2*maxR, 2*maxR);
            QColor c = paletteColor(catIdx).lighter(120 + subIdx * 20);
            p.setBrush(c);
            p.setPen(QPen(QColor(0x18,0x18,0x18), 1));
            p.drawPie(rOut, (int)subStart, (int)subSpan);
            /* etichetta */
            double midDeg = (subStart + subSpan / 2.0) / 16.0;
            double midRad = midDeg * M_PI / 180.0;
            double lx = cx + (midR + (maxR - midR)*0.5) * std::cos(midRad);
            double ly = cy - (midR + (maxR - midR)*0.5) * std::sin(midRad);
            p.setPen(Qt::white);
            p.setFont(QFont("Inter,Ubuntu,sans-serif", 7));
            p.drawText(QRectF(lx-28, ly-8, 56, 16), Qt::AlignCenter, kv.first);
            subStart += subSpan;
            ++subIdx;
        }
        /* etichetta categoria (anello interno) */
        double midDeg = (catStart + catSpanTotal / 2.0) / 16.0;
        double midRad = midDeg * M_PI / 180.0;
        double lx = cx + innerR * 0.75 * std::cos(midRad);
        double ly = cy - innerR * 0.75 * std::sin(midRad);
        p.setPen(Qt::white);
        p.setFont(QFont("Inter,Ubuntu,sans-serif", 8, QFont::Bold));
        p.drawText(QRectF(lx-30, ly-8, 60, 16), Qt::AlignCenter, cat);
        catStart += catSpanTotal;
        ++catIdx;
    }
    /* maschera foro */
    p.setBrush(QColor(0x18,0x18,0x18));
    p.setPen(Qt::NoPen);
    double hole = innerR * 0.5;
    p.drawEllipse(QPointF(cx, cy), hole, hole);
}
