/*
 * main_graph_canvas_relational.cpp — GraficoCanvas: relazionali/avanzati
 * ===========================================================================
 * ParallelCoord, Sankey, Tree, Chord, Violin, WordCloud, RadialTree,
 * AnimatedLine, SmallMultiples. Split da main_graph_canvas.cpp (TODO D-8).
 */
#include "main_graph.h"
#include "main_graph_canvas_p.h"

#include <QPainter>
#include <QPainterPath>
#include <QFont>
#include <cmath>
#include <algorithm>


/* ══════════════════════════════════════════════════════════════
   ParallelCoord — coordinate parallele
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::drawParallelCoord(QPainter& p, const QRectF& a) {
    p.setRenderHint(QPainter::Antialiasing);
    if (m_lineSeries.isEmpty()) {
        p.setPen(QColor(0x77,0x77,0x77));
        p.drawText(a, Qt::AlignCenter, "Nessun dato\n# Asse1 Asse2 Asse3\n v1  v2  v3");
        return;
    }
    /* ogni serie = una riga/entità; ogni punto ha (x=asse, y=valore) */
    /* trova min/max per ogni asse */
    int nAxes = 0;
    for (auto& s : m_lineSeries) nAxes = std::max(nAxes, (int)s.size());
    if (nAxes < 2) {
        p.setPen(QColor(0x77,0x77,0x77));
        p.drawText(a, Qt::AlignCenter, "Servono almeno 2 assi");
        return;
    }
    QVector<double> axMn(nAxes, 1e18), axMx(nAxes, -1e18);
    for (auto& s : m_lineSeries)
        for (auto& pt : s) {
            int ax = (int)std::round(pt.x());
            if (ax >= 0 && ax < nAxes) {
                axMn[ax] = std::min(axMn[ax], pt.y());
                axMx[ax] = std::max(axMx[ax], pt.y());
            }
        }
    for (int ax = 0; ax < nAxes; ++ax) {
        if (axMn[ax] >= axMx[ax]) { axMn[ax] -= 1; axMx[ax] += 1; }
    }
    const double topM = 30.0, botM = 30.0;
    double axSpacing = a.width() / (nAxes - 1);

    /* assi verticali */
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 8));
    for (int ax = 0; ax < nAxes; ++ax) {
        double sx = a.left() + ax * axSpacing;
        p.setPen(QPen(QColor(0x60,0x60,0x60), 1.5));
        p.drawLine(QPointF(sx, a.top()+topM), QPointF(sx, a.bottom()-botM));
        /* etichetta asse */
        p.setPen(QColor(0xcc,0xcc,0xcc));
        QString axName = ax < m_lineNames.size() ? m_lineNames[ax] : QString("A%1").arg(ax+1);
        p.drawText(QRectF(sx-35, a.top()+2, 70, 20), Qt::AlignCenter, axName);
        /* min/max */
        p.setPen(QColor(0x77,0x77,0x77));
        p.drawText(QRectF(sx-35, a.bottom()-botM-14, 70, 14), Qt::AlignCenter, fmtNum(axMn[ax]));
        p.drawText(QRectF(sx-35, a.top()+topM, 70, 14), Qt::AlignCenter, fmtNum(axMx[ax]));
    }

    /* polilinee */
    p.setClipRect(a);
    for (int si = 0; si < m_lineSeries.size(); ++si) {
        auto& s = m_lineSeries[si];
        if (s.isEmpty()) continue;
        QColor c = paletteColor(si);
        p.setPen(QPen(QColor(c.red(), c.green(), c.blue(), 160), 1.5));
        QPainterPath path;
        bool first = true;
        for (auto& pt : s) {
            int ax = (int)std::round(pt.x());
            if (ax < 0 || ax >= nAxes) continue;
            double range = axMx[ax] - axMn[ax];
            double norm = range > 0 ? (pt.y() - axMn[ax]) / range : 0.5;
            double sx = a.left() + ax * axSpacing;
            double sy = a.bottom() - botM - norm * (a.height() - topM - botM);
            QPointF sp(sx, sy);
            if (first) { path.moveTo(sp); first=false; } else path.lineTo(sp);
        }
        p.drawPath(path);
    }
    p.setClipping(false);
}

/* ══════════════════════════════════════════════════════════════
   Sankey — diagramma di Sankey semplificato
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::drawSankey(QPainter& p, const QRectF& a) {
    p.setRenderHint(QPainter::Antialiasing);
    if (m_sankeyData.isEmpty()) {
        p.setPen(QColor(0x77,0x77,0x77));
        p.drawText(a, Qt::AlignCenter, "Nessun dato\nFormato: A \xe2\x86\x92 B: valore");
        return;
    }
    /* raccoglie nodi sorgente e destinazione */
    QStringList fromNodes, toNodes;
    for (auto& lk : m_sankeyData) {
        if (!fromNodes.contains(lk.from)) fromNodes.append(lk.from);
        if (!toNodes.contains(lk.to))     toNodes.append(lk.to);
    }
    /* calcola altezze nodi */
    QMap<QString, double> fromH, toH;
    for (auto& lk : m_sankeyData) {
        fromH[lk.from] += lk.value;
        toH[lk.to]     += lk.value;
    }
    double totalH = 0; for (auto v : fromH) totalH = std::max(totalH, v);
    for (auto v : toH) totalH = std::max(totalH, v);
    if (totalH <= 0) return;

    const double nodeW = 18.0;
    double lx = a.left() + 60, rx = a.right() - 60;
    double padV = 8.0;
    double usableH = a.height() - padV * (fromNodes.size() - 1);
    double scale = usableH / totalH;

    /* posizione Y nodi sinistra */
    QMap<QString, double> fromY, toY;
    {
        double y = a.top();
        for (const QString& n : fromNodes) {
            fromY[n] = y;
            y += fromH[n] * scale + padV;
        }
    }
    {
        double y = a.top();
        for (const QString& n : toNodes) {
            toY[n] = y;
            y += toH[n] * scale + padV;
        }
    }
    /* offsetter per flussi */
    QMap<QString, double> fromOff, toOff;

    p.setFont(QFont("Inter,Ubuntu,sans-serif", 9));
    int li = 0;
    for (auto& lk : m_sankeyData) {
        double fh = lk.value * scale;
        double fy0 = fromY[lk.from] + fromOff[lk.from];
        double ty0 = toY[lk.to]     + toOff[lk.to];
        fromOff[lk.from] += fh;
        toOff[lk.to]     += fh;

        QColor c = paletteColor(li++ % 8);
        QPainterPath path;
        path.moveTo(lx + nodeW, fy0);
        path.cubicTo(QPointF((lx+rx)/2, fy0),
                     QPointF((lx+rx)/2, ty0),
                     QPointF(rx, ty0));
        path.lineTo(rx, ty0 + fh);
        path.cubicTo(QPointF((lx+rx)/2, ty0+fh),
                     QPointF((lx+rx)/2, fy0+fh),
                     QPointF(lx+nodeW, fy0+fh));
        path.closeSubpath();
        p.fillPath(path, QColor(c.red(), c.green(), c.blue(), 80));
        p.setPen(QPen(QColor(c.red(), c.green(), c.blue(), 160), 1));
        p.drawPath(path);
    }
    /* rettangoli nodi */
    int ni = 0;
    for (const QString& n : fromNodes) {
        double h = fromH[n] * scale;
        p.fillRect(QRectF(lx, fromY[n], nodeW, h), paletteColor(ni));
        p.setPen(QColor(0xcc,0xcc,0xcc));
        p.drawText(QRectF(a.left(), fromY[n], lx-4, std::max(h,14.0)),
                   Qt::AlignRight|Qt::AlignVCenter, n);
        ++ni;
    }
    ni = 0;
    for (const QString& n : toNodes) {
        double h = toH[n] * scale;
        p.fillRect(QRectF(rx, toY[n], nodeW, h), paletteColor(ni+4));
        p.setPen(QColor(0xcc,0xcc,0xcc));
        p.drawText(QRectF(rx+nodeW+4, toY[n], 80, std::max(h,14.0)),
                   Qt::AlignLeft|Qt::AlignVCenter, n);
        ++ni;
    }
}

/* ══════════════════════════════════════════════════════════════
   Tree — albero gerarchico
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::drawTree(QPainter& p, const QRectF& a) {
    p.setRenderHint(QPainter::Antialiasing);
    if (m_treeData.isEmpty()) {
        p.setPen(QColor(0x77,0x77,0x77));
        p.drawText(a, Qt::AlignCenter, "Nessun dato\nFormato: padre: figlio1, figlio2");
        return;
    }
    /* raccoglie tutti i nodi e trova la root */
    QStringList allNodes;
    QMap<QString, QStringList> children;
    QSet<QString> hasParent;
    for (auto& e : m_treeData) {
        if (!allNodes.contains(e.parent)) allNodes.append(e.parent);
        if (!allNodes.contains(e.child))  allNodes.append(e.child);
        children[e.parent].append(e.child);
        hasParent.insert(e.child);
    }
    QStringList roots;
    for (const QString& n : allNodes) if (!hasParent.contains(n)) roots.append(n);
    if (roots.isEmpty()) roots.append(allNodes[0]);

    /* BFS per calcolare posizioni livello per livello */
    struct TNode { QString name; int level; double posX; };
    QVector<TNode> layout;
    QMap<int, int> levelCount;
    QMap<QString, int> nodeLevel;
    QVector<QPair<QString,QString>> edges;

    /* BFS */
    QVector<QString> queue = roots;
    for (auto& r : roots) nodeLevel[r] = 0;
    int levelCountMax = 0;
    while (!queue.isEmpty()) {
        QString cur = queue.takeFirst();
        int lv = nodeLevel[cur];
        levelCountMax = std::max(levelCountMax, lv);
        levelCount[lv]++;
        layout.append({cur, lv, 0.0});
        for (auto& child : children[cur]) {
            if (!nodeLevel.contains(child)) {
                nodeLevel[child] = lv + 1;
                queue.append(child);
                edges.append({cur, child});
            }
        }
    }
    int nLevels = levelCountMax + 1;
    /* assegna posizione X per ogni livello */
    QMap<int, int> lvIdx;
    for (auto& tn : layout) {
        int cnt = levelCount[tn.level];
        tn.posX = (double)(lvIdx[tn.level]++) / std::max(1, cnt-1);
        if (cnt == 1) tn.posX = 0.5;
    }
    /* posizioni pixel */
    double vPad = 30.0, hPad = 30.0;
    auto nodePos = [&](const QString& name) -> QPointF {
        for (auto& tn : layout) {
            if (tn.name == name) {
                double sx = hPad + tn.posX * (a.width() - 2*hPad) + a.left();
                double sy = vPad + ((double)tn.level / std::max(1, nLevels-1)) * (a.height() - 2*vPad) + a.top();
                return {sx, sy};
            }
        }
        return a.center();
    };

    /* archi */
    p.setPen(QPen(QColor(0x60,0x60,0x60), 1.5));
    for (auto& e : edges) {
        QPointF p0 = nodePos(e.first), p1 = nodePos(e.second);
        p.drawLine(p0, p1);
    }
    /* nodi */
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 8));
    int ni = 0;
    for (auto& tn : layout) {
        QPointF pos = nodePos(tn.name);
        QColor c = paletteColor(ni++);
        p.setBrush(QColor(0x1e,0x1e,0x1e));
        p.setPen(QPen(c, 2));
        p.drawEllipse(pos, 10, 10);
        p.setPen(c.lighter(140));
        p.drawText(QRectF(pos.x()-30, pos.y()+12, 60, 14), Qt::AlignCenter, tn.name);
    }
}

/* ══════════════════════════════════════════════════════════════
   Chord — diagramma a corde
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::drawChord(QPainter& p, const QRectF& a) {
    p.setRenderHint(QPainter::Antialiasing);
    if (m_chordData.isEmpty()) {
        p.setPen(QColor(0x77,0x77,0x77));
        p.drawText(a, Qt::AlignCenter, "Nessun dato\nFormato: A \xe2\x86\x92 B: valore");
        return;
    }
    /* raccoglie nodi */
    QStringList nodes;
    for (auto& lk : m_chordData) {
        if (!nodes.contains(lk.from)) nodes.append(lk.from);
        if (!nodes.contains(lk.to))   nodes.append(lk.to);
    }
    int n = nodes.size();
    if (n < 2) {
        p.setPen(QColor(0x77,0x77,0x77));
        p.drawText(a, Qt::AlignCenter, "Servono almeno 2 nodi");
        return;
    }
    /* somma flussi per nodo */
    QVector<double> totals(n, 0.0);
    for (auto& lk : m_chordData) {
        totals[nodes.indexOf(lk.from)] += lk.value;
        totals[nodes.indexOf(lk.to)]   += lk.value;
    }
    double grandTotal = 0; for (double v : totals) grandTotal += v;
    if (grandTotal <= 0) return;

    double cx = a.center().x() + m_panNC.x();
    double cy = a.center().y() + m_panNC.y();
    double r  = std::min(a.width(), a.height()) * 0.36 * m_zoomNC;
    double arcThick = r * 0.12;

    /* angoli per ogni nodo (proporzionali alla somma) */
    QVector<double> startAng(n), spanAng(n);
    double gap = 0.02; /* gap in radianti tra archi */
    double totalGap = gap * n;
    double available = 2*M_PI - totalGap;
    double cur = -M_PI / 2;
    for (int i = 0; i < n; ++i) {
        startAng[i] = cur;
        spanAng[i]  = (totals[i] / grandTotal) * available;
        cur += spanAng[i] + gap;
    }

    /* disegna archi nodo */
    for (int i = 0; i < n; ++i) {
        QColor c = paletteColor(i);
        p.setPen(QPen(c, arcThick, Qt::SolidLine, Qt::FlatCap));
        int startDeg = (int)(startAng[i] * 180.0 / M_PI * 16);
        int spanDeg  = (int)(spanAng[i]  * 180.0 / M_PI * 16);
        p.drawArc(QRectF(cx-r, cy-r, 2*r, 2*r), -startDeg, -spanDeg);
        /* etichetta */
        double midA = startAng[i] + spanAng[i] / 2;
        double lx = cx + (r + arcThick + 10) * std::cos(midA);
        double ly = cy + (r + arcThick + 10) * std::sin(midA);
        p.setPen(c.lighter(150));
        p.setFont(QFont("Inter,Ubuntu,sans-serif", 8));
        p.drawText(QRectF(lx-30, ly-8, 60, 16), Qt::AlignCenter, nodes[i]);
    }
    /* disegna corde */
    QVector<double> fromOff(n, 0.0), toOff(n, 0.0);
    int li = 0;
    for (auto& lk : m_chordData) {
        int fi = nodes.indexOf(lk.from);
        int ti = nodes.indexOf(lk.to);
        if (fi < 0 || ti < 0) continue;
        double frac = lk.value / grandTotal;
        double fSpan = frac * available;
        double tSpan = frac * available;
        double fa0 = startAng[fi] + fromOff[fi];
        double fa1 = fa0 + fSpan;
        double ta0 = startAng[ti] + toOff[ti];
        double ta1 = ta0 + tSpan;
        fromOff[fi] += fSpan;
        toOff[ti]   += tSpan;
        double fMid = (fa0 + fa1) / 2;
        double tMid = (ta0 + ta1) / 2;
        QColor c = paletteColor(li++ % 8);
        QPainterPath path;
        path.moveTo(cx + r * std::cos(fMid), cy + r * std::sin(fMid));
        path.quadTo(QPointF(cx, cy),
                    QPointF(cx + r * std::cos(tMid), cy + r * std::sin(tMid)));
        p.setPen(QPen(QColor(c.red(), c.green(), c.blue(), 100), std::max(1.0, fSpan*r*0.5)));
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
    }
}

/* ══════════════════════════════════════════════════════════════
   Violin Plot (KDE mirrored)
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::drawViolin(QPainter& p, const QRectF& area) {
    p.setRenderHint(QPainter::Antialiasing);
    if (m_violinData.isEmpty()) {
        p.setPen(QColor(0x77,0x77,0x77));
        p.drawText(area, Qt::AlignCenter,
                   "Formato: NomeSerie: v1 v2 v3 ... (una serie per riga)\n"
                   "Esempio:\n  Gruppo A: 12 18 22 25 30 35 40\n  Gruppo B: 8 14 20 28 35 42");
        return;
    }
    /* range Y globale */
    double vMin = 1e18, vMax = -1e18;
    for (auto& sp : m_violinData)
        for (double v : sp.second) { vMin = std::min(vMin,v); vMax = std::max(vMax,v); }
    if (vMin >= vMax) { vMin -= 1; vMax += 1; }
    double yPad = (vMax-vMin)*0.10;
    vMin -= yPad; vMax += yPad;
    double yRange = vMax - vMin;

    int n = m_violinData.size();
    double slotW = area.width() / n;

    /* griglia Y */
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 7));
    double yStep = niceStep(yRange, 5);
    for (double yv = std::ceil(vMin/yStep)*yStep; yv <= vMax+yStep*0.5; yv += yStep) {
        double sy = area.bottom() - (yv-vMin)/yRange * area.height();
        if (sy < area.top()-2 || sy > area.bottom()+2) continue;
        p.setPen(QPen(QColor(0x2a,0x2a,0x2a), 0.8));
        p.drawLine(QPointF(area.left(), sy), QPointF(area.right(), sy));
        p.setPen(QColor(0x66,0x66,0x66));
        p.drawText(QRectF(area.left()-52, sy-8, 48, 16),
                   Qt::AlignRight|Qt::AlignVCenter, fmtNum(yv));
    }

    for (int si = 0; si < n; ++si) {
        const QVector<double>& vals = m_violinData[si].second;
        const QString&         nm   = m_violinData[si].first;
        if (vals.size() < 2) continue;
        QColor col = paletteColor(si);
        double cx  = area.left() + slotW*(si+0.5);
        double maxHW = slotW * 0.38;

        /* banda di Silverman */
        double mean = 0;
        for (double v : vals) mean += v;
        mean /= vals.size();
        double var = 0;
        for (double v : vals) var += (v-mean)*(v-mean);
        double bw = 1.06 * std::sqrt(var / std::max((int)vals.size()-1,1))
                    * std::pow((double)vals.size(), -0.2);
        if (bw < 1e-10) bw = yRange * 0.10;

        /* campiona KDE */
        const int kS = 100;
        QVector<double> kde(kS);
        double kdeMax = 0;
        for (int k = 0; k < kS; ++k) {
            double y = vMin + (k+0.5)/kS * yRange;
            double d = 0;
            for (double xi : vals) { double z=(y-xi)/bw; d += std::exp(-0.5*z*z); }
            kde[k] = d / (vals.size()*bw*std::sqrt(2*M_PI));
            kdeMax = std::max(kdeMax, kde[k]);
        }
        if (kdeMax < 1e-15) continue;

        /* forma a violin */
        QPainterPath path;
        for (int k = kS-1; k >= 0; --k) {
            double sy = area.bottom() - (vMin+(k+0.5)/kS*yRange - vMin)/yRange * area.height();
            double hw = kde[k]/kdeMax * maxHW;
            QPointF pt(cx+hw, sy);
            if (k == kS-1) path.moveTo(pt); else path.lineTo(pt);
        }
        for (int k = 0; k < kS; ++k) {
            double sy = area.bottom() - (vMin+(k+0.5)/kS*yRange - vMin)/yRange * area.height();
            double hw = kde[k]/kdeMax * maxHW;
            path.lineTo(QPointF(cx-hw, sy));
        }
        path.closeSubpath();
        QColor fc = col; fc.setAlpha(100);
        p.setBrush(fc); p.setPen(QPen(col, 1.5));
        p.drawPath(path);

        /* IQR + mediana */
        QVector<double> sorted = vals;
        std::sort(sorted.begin(), sorted.end());
        int sz = sorted.size();
        auto toY = [&](double v){ return area.bottom()-(v-vMin)/yRange*area.height(); };
        p.setRenderHint(QPainter::Antialiasing, false);
        p.setPen(QPen(Qt::white, 3));
        p.drawLine(QPointF(cx, toY(sorted[sz/4])), QPointF(cx, toY(sorted[3*sz/4])));
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen); p.setBrush(Qt::white);
        p.drawEllipse(QPointF(cx, toY(sorted[sz/2])), 5, 5);

        /* etichetta */
        p.setPen(QColor(0xcc,0xcc,0xcc));
        p.setFont(QFont("Inter,Ubuntu,sans-serif", 8));
        p.drawText(QRectF(cx-slotW*0.45, area.bottom()+4, slotW*0.9, 16), Qt::AlignCenter, nm);
    }
    p.setPen(QColor(0xbb,0xdd,0xff,190));
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 9, QFont::Bold));
    p.drawText(QRectF(area.left(), area.top()-18, area.width(), 18),
               Qt::AlignHCenter|Qt::AlignVCenter, "Violin Plot");
}

/* ══════════════════════════════════════════════════════════════
   Word Cloud
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::drawWordCloud(QPainter& p, const QRectF& area) {
    p.setRenderHint(QPainter::Antialiasing);
    if (m_wordData.isEmpty()) {
        p.setPen(QColor(0x77,0x77,0x77));
        p.drawText(area, Qt::AlignCenter,
                   "Formato: parola:frequenza (una per riga)\n"
                   "Esempio:\n  Python:95\n  C++:80\n  Java:70\n  Rust:60\n  Go:55");
        return;
    }
    QVector<QPair<QString,double>> sorted = m_wordData;
    std::sort(sorted.begin(), sorted.end(), [](auto& a, auto& b){ return a.second > b.second; });
    double maxF = sorted[0].second;
    double minF = sorted.last().second;
    if (maxF <= minF) maxF = minF + 1;
    const double kMaxPt = 54.0, kMinPt = 9.0;
    double cx = area.center().x() + m_panNC.x();
    double cy = area.center().y() + m_panNC.y();
    static const QColor pal[] = {
        {0x00,0xbf,0xd8},{0xff,0x79,0x5a},{0x73,0xe2,0x73},
        {0xff,0xd7,0x6e},{0xb0,0x90,0xff},{0xff,0x82,0xc2},
        {0x5a,0xd7,0xff},{0xff,0xa0,0x50}
    };
    QVector<QRectF> placed;
    for (int wi = 0; wi < std::min((int)sorted.size(), 80); ++wi) {
        double t = (sorted[wi].second - minF) / (maxF - minF);
        int ptSize = (int)((kMinPt + t*(kMaxPt-kMinPt)) * m_zoomNC);
        if (ptSize < 6) ptSize = 6;
        QFont f("Inter,Ubuntu,sans-serif", ptSize, t > 0.75 ? QFont::Bold : QFont::Normal);
        QFontMetricsF fm(f);
        double ww = fm.horizontalAdvance(sorted[wi].first) + 6;
        double wh = fm.height() + 2;
        double bx = cx - ww/2, by = cy - wh/2;
        bool ok = false;
        double angle = 0, r = 0;
        for (int tries = 0; tries < 600 && !ok; ++tries) {
            double px = cx + r*std::cos(angle) - ww/2;
            double py = cy + r*std::sin(angle) - wh/2;
            QRectF rect(px, py, ww, wh);
            if (rect.left()>=area.left()+4 && rect.right()<=area.right()-4 &&
                rect.top()>=area.top()+4 && rect.bottom()<=area.bottom()-4) {
                bool ov = false;
                for (auto& pr : placed) if (rect.adjusted(-2,-2,2,2).intersects(pr)){ov=true;break;}
                if (!ov) { bx=px; by=py; ok=true; }
            }
            angle += 0.35; r += 0.8;
        }
        if (!ok) continue;
        QRectF rect(bx, by, ww, wh);
        placed.append(rect);
        QColor col = pal[wi%8]; col.setAlpha(130+(int)(t*125));
        p.setFont(f); p.setPen(col);
        p.drawText(rect, Qt::AlignCenter, sorted[wi].first);
    }
    p.setPen(QColor(0xbb,0xdd,0xff,190));
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 9, QFont::Bold));
    p.drawText(QRectF(area.left(), area.top()-18, area.width(), 18),
               Qt::AlignHCenter|Qt::AlignVCenter, "Word Cloud");
}

/* ══════════════════════════════════════════════════════════════
   Albero Radiale (root al centro)
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::drawRadialTree(QPainter& p, const QRectF& area) {
    p.setRenderHint(QPainter::Antialiasing);
    if (m_treeData.isEmpty()) {
        p.setPen(QColor(0x77,0x77,0x77));
        p.drawText(area, Qt::AlignCenter, "Nessun dato\nFormato: padre: figlio1, figlio2");
        return;
    }
    QStringList allNodes;
    QMap<QString,QStringList> children;
    QSet<QString> hasParent;
    for (auto& e : m_treeData) {
        if (!allNodes.contains(e.parent)) allNodes.append(e.parent);
        if (!allNodes.contains(e.child))  allNodes.append(e.child);
        children[e.parent].append(e.child);
        hasParent.insert(e.child);
    }
    QStringList roots;
    for (const QString& nd : allNodes) if (!hasParent.contains(nd)) roots.append(nd);
    if (roots.isEmpty() && !allNodes.isEmpty()) roots.append(allNodes[0]);
    if (roots.isEmpty()) return;
    QString root = roots[0];

    double cx = area.center().x()+m_panNC.x();
    double cy = area.center().y()+m_panNC.y();
    double layR = std::min(area.width(),area.height()) * 0.17 * m_zoomNC;

    QMap<QString,QPointF> pos;
    QMap<QString,int>     dep;

    /* conta sottofoglie ricorsivamente */
    std::function<int(const QString&)> subSize = [&](const QString& nd)->int {
        int s=1; for(auto& c:children.value(nd)) s+=subSize(c); return s;
    };

    std::function<void(const QString&, double, double, int)> layout =
        [&](const QString& nd, double aFrom, double aTo, int d) {
        double angle=(aFrom+aTo)*0.5, r=d*layR;
        pos[nd] = (d==0) ? QPointF(cx,cy) : QPointF(cx+r*std::cos(angle),cy+r*std::sin(angle));
        dep[nd] = d;
        QStringList& ch = children[nd];
        if (ch.isEmpty()) return;
        int tot=0; QVector<int> sizes;
        for(auto& c:ch){int s=subSize(c);sizes.append(s);tot+=s;}
        double span=aTo-aFrom, cur=aFrom;
        for(int i=0;i<ch.size();++i){
            double frac=(double)sizes[i]/tot;
            layout(ch[i],cur,cur+span*frac,d+1);
            cur+=span*frac;
        }
    };
    layout(root, 0, 2*M_PI, 0);

    /* archi bezier */
    for (auto& e : m_treeData) {
        if (!pos.contains(e.parent)||!pos.contains(e.child)) continue;
        QPointF from=pos[e.parent], to=pos[e.child];
        QPointF ctrl={(from.x()+to.x()+cx)*0.333,(from.y()+to.y()+cy)*0.333};
        QPainterPath ep; ep.moveTo(from); ep.quadTo(ctrl,to);
        p.setPen(QPen(QColor(0x44,0x66,0x88),1.2)); p.setBrush(Qt::NoBrush);
        p.drawPath(ep);
    }
    /* nodi */
    p.setFont(QFont("Inter,Ubuntu,sans-serif",8));
    for (const QString& nd : allNodes) {
        if (!pos.contains(nd)) continue;
        int d=dep[nd];
        QColor col=(d==0)?paletteColor(0):paletteColor(1+d%7);
        double nr=(d==0)?10:7;
        p.setPen(QPen(col.darker(140),1.5)); p.setBrush(QColor(0x1e,0x1e,0x1e));
        p.drawEllipse(pos[nd],nr,nr);
        p.setPen(col.lighter(140));
        p.drawText(QRectF(pos[nd].x()+nr+3,pos[nd].y()-7,80,14),
                   Qt::AlignLeft|Qt::AlignVCenter, nd);
    }
    p.setPen(QColor(0xbb,0xdd,0xff,190));
    p.setFont(QFont("Inter,Ubuntu,sans-serif",9,QFont::Bold));
    p.drawText(QRectF(area.left(),area.top()-18,area.width(),18),
               Qt::AlignHCenter|Qt::AlignVCenter,"Albero Radiale");
}

/* ══════════════════════════════════════════════════════════════
   Linea Animata (QTimer)
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::drawAnimatedLine(QPainter& p, const QRectF& area) {
    p.setRenderHint(QPainter::Antialiasing);
    if (m_lineSeries.isEmpty()) {
        p.setPen(QColor(0x66,0x88,0xaa));
        p.drawText(area, Qt::AlignCenter,
                   "Formato: stessa struttura di Linea multi-serie\n"
                   "x, y1, y2, ... (colonne = serie)");
        return;
    }
    int maxPts = 0;
    for (auto& s : m_lineSeries) maxPts = std::max(maxPts, (int)s.size());
    if (maxPts == 0) return;
    int frame = std::min(m_animFrame, maxPts-1);

    /* calcola range sull'intera serie */
    double xMn=1e18,xMx=-1e18,yMn=1e18,yMx=-1e18;
    for (auto& s:m_lineSeries) for (auto& pt:s){
        xMn=std::min(xMn,pt.x());xMx=std::max(xMx,pt.x());
        yMn=std::min(yMn,pt.y());yMx=std::max(yMx,pt.y());
    }
    if (xMn>=xMx){xMn-=1;xMx+=1;} if (yMn>=yMx){yMn-=1;yMx+=1;}
    double xP=(xMx-xMn)*0.08,yP=(yMx-yMn)*0.12;
    m_xVMin=xMn-xP; m_xVMax=xMx+xP; m_yVMin=yMn-yP; m_yVMax=yMx+yP;

    drawGrid(p, area);
    p.setClipRect(area.adjusted(-1,-1,1,1));
    for (int si=0; si<m_lineSeries.size(); ++si) {
        auto& s = m_lineSeries[si];
        QColor col = paletteColor(si);
        int vis = std::min(frame+1, (int)s.size());
        if (vis < 1) continue;
        QPainterPath path;
        path.moveTo(dataToScreen(s[0].x(),s[0].y(),area));
        for (int k=1;k<vis;++k) path.lineTo(dataToScreen(s[k].x(),s[k].y(),area));
        p.setPen(QPen(col,2.0)); p.setBrush(Qt::NoBrush); p.drawPath(path);
        /* dot animato all'estremita' */
        QPointF tip = dataToScreen(s[vis-1].x(),s[vis-1].y(),area);
        p.setPen(Qt::NoPen); p.setBrush(col); p.drawEllipse(tip,5.0,5.0);
    }
    p.setClipping(false);
    /* legenda */
    { double lx=area.right()-140,ly=area.top()+8;
      p.setFont(QFont("Inter,Ubuntu,sans-serif",8));
      for (int si=0;si<m_lineSeries.size();++si){
        QColor col=paletteColor(si);
        p.setPen(QPen(col,2)); p.drawLine(QPointF(lx,ly+5),QPointF(lx+16,ly+5));
        p.setPen(QColor(0xcc,0xcc,0xcc));
        QString nm=(si<m_lineNames.size())?m_lineNames[si]:QString("Serie %1").arg(si+1);
        p.drawText(QPointF(lx+20,ly+9),nm); ly+=16;
      }
    }
    /* barra progresso */
    double pct=(double)(frame+1)/maxPts;
    double bw2=area.width()*0.5, bx2=area.left()+(area.width()-bw2)*0.5, by2=area.bottom()+6;
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x30,0x30,0x30)); p.drawRoundedRect(QRectF(bx2,by2,bw2,4),2,2);
    p.setBrush(paletteColor(0));        p.drawRoundedRect(QRectF(bx2,by2,bw2*pct,4),2,2);
    p.setPen(QColor(0xbb,0xdd,0xff,190));
    p.setFont(QFont("Inter,Ubuntu,sans-serif",9,QFont::Bold));
    p.drawText(QRectF(area.left(),area.top()-18,area.width(),18),
               Qt::AlignHCenter|Qt::AlignVCenter,"Linea Animata");
}

/* ══════════════════════════════════════════════════════════════
   Small Multiples (una serie = un pannello)
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::drawSmallMultiples(QPainter& p, const QRectF& area) {
    p.setRenderHint(QPainter::Antialiasing);
    if (m_lineSeries.isEmpty()) {
        p.setPen(QColor(0x66,0x88,0xaa));
        p.drawText(area, Qt::AlignCenter,
                   "Formato: stessa struttura di Linea multi-serie\n"
                   "x, y1, y2, ... ogni colonna diventa un pannello");
        return;
    }
    int n = m_lineSeries.size();
    int cols = (int)std::ceil(std::sqrt((double)n));
    int rows = (int)std::ceil((double)n / cols);
    double panW = area.width()/cols;
    double panH = area.height()/rows;
    const double pad = 8.0;
    for (int si=0; si<n; ++si) {
        int row=si/cols, col=si%cols;
        QRectF pa(area.left()+col*panW+pad, area.top()+row*panH+pad,
                  panW-2*pad, panH-2*pad-14);
        if (pa.width()<40||pa.height()<40) continue;
        auto& s = m_lineSeries[si];
        QColor col_c = paletteColor(si);
        /* border */
        p.setPen(QPen(QColor(0x33,0x33,0x33),1));
        p.setBrush(QColor(0x1a,0x1a,0x1a));
        p.drawRect(pa);
        if (!s.isEmpty()) {
            double xMn=s[0].x(),xMx=xMn,yMn=s[0].y(),yMx=yMn;
            for(auto& pt:s){xMn=std::min(xMn,pt.x());xMx=std::max(xMx,pt.x());
                            yMn=std::min(yMn,pt.y());yMx=std::max(yMx,pt.y());}
            if(xMn>=xMx){xMn-=1;xMx+=1;} if(yMn>=yMx){yMn-=1;yMx+=1;}
            double xP=(xMx-xMn)*0.08,yP=(yMx-yMn)*0.12;
            xMn-=xP;xMx+=xP;yMn-=yP;yMx+=yP;
            /* grid lines */
            p.setPen(QPen(QColor(0x28,0x28,0x28),0.5));
            for(int g=1;g<=3;++g){double gy=pa.top()+pa.height()*g/4.0;
                p.drawLine(QPointF(pa.left(),gy),QPointF(pa.right(),gy));}
            auto toSc=[&](const QPointF& pt)->QPointF{
                return{pa.left()+(pt.x()-xMn)/(xMx-xMn)*pa.width(),
                       pa.bottom()-(pt.y()-yMn)/(yMx-yMn)*pa.height()};
            };
            p.setClipRect(pa);
            /* area riempita */
            QPainterPath fill;
            fill.moveTo(toSc({s[0].x(),yMn}));
            fill.lineTo(toSc(s[0]));
            for(int k=1;k<s.size();++k) fill.lineTo(toSc(s[k]));
            fill.lineTo(toSc({s.last().x(),yMn}));
            fill.closeSubpath();
            QColor fc=col_c;fc.setAlpha(30);p.setBrush(fc);p.setPen(Qt::NoPen);p.drawPath(fill);
            /* linea */
            QPainterPath lp;
            lp.moveTo(toSc(s[0]));
            for(int k=1;k<s.size();++k) lp.lineTo(toSc(s[k]));
            p.setPen(QPen(col_c,1.5));p.setBrush(Qt::NoBrush);p.drawPath(lp);
            p.setClipping(false);
        }
        QString nm=(si<m_lineNames.size())?m_lineNames[si]:QString("Serie %1").arg(si+1);
        p.setPen(col_c.lighter(140));
        p.setFont(QFont("Inter,Ubuntu,sans-serif",8,QFont::Bold));
        p.drawText(QRectF(pa.left(),pa.bottom()+2,pa.width(),14),Qt::AlignCenter,nm);
    }
    p.setPen(QColor(0xbb,0xdd,0xff,190));
    p.setFont(QFont("Inter,Ubuntu,sans-serif",9,QFont::Bold));
    p.drawText(QRectF(area.left(),area.top()-18,area.width(),18),
               Qt::AlignHCenter|Qt::AlignVCenter,"Small Multiples");
}
