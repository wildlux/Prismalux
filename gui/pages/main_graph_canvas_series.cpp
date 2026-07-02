/*
 * main_graph_canvas_series.cpp — GraficoCanvas: serie classiche
 * ==================================================================
 * Line, Polar, Radar, Bubble, Heatmap, Candlestick, Area, Waterfall, Step.
 * Split da main_graph_canvas.cpp (TODO D-8).
 */
#include "main_graph.h"
#include "main_graph_canvas_p.h"
#include "widgets/formula_parser.h"

#include <QPainter>
#include <QPainterPath>
#include <QFont>
#include <cmath>
#include <algorithm>

/* ══════════════════════════════════════════════════════════════
   Linea multi-serie — rendering
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::drawLine(QPainter& p, const QRectF& area) {
    p.setRenderHint(QPainter::Antialiasing);
    if (m_lineSeries.isEmpty()) {
        p.setPen(QColor(0x66,0x88,0xaa));
        p.drawText(area, Qt::AlignCenter, "Nessun dato");
        return;
    }
    drawGrid(p, area);

    /* Clipping alla plot area */
    p.setClipRect(area.adjusted(-1,-1,1,1));

    for (int si = 0; si < m_lineSeries.size(); ++si) {
        const auto& s   = m_lineSeries[si];
        QColor col = paletteColor(si);
        QPainterPath path;
        bool first = true;
        for (auto& pt : s) {
            QPointF sp = dataToScreen(pt.x(), pt.y(), area);
            if (first) { path.moveTo(sp); first = false; }
            else        path.lineTo(sp);
        }
        p.setPen(QPen(col, 2.0));
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
        /* dot su ogni punto */
        p.setPen(Qt::NoPen);
        p.setBrush(col);
        for (auto& pt : s)
            p.drawEllipse(dataToScreen(pt.x(), pt.y(), area), 3.0, 3.0);
    }
    p.setClipping(false);

    /* Legenda */
    {
        double lx = area.right() - 140;
        double ly = area.top() + 8;
        p.setFont(QFont("Inter,Ubuntu,sans-serif", 8));
        for (int si = 0; si < m_lineSeries.size(); ++si) {
            QColor col = paletteColor(si);
            p.setPen(QPen(col, 2)); p.drawLine(QPointF(lx,ly+5), QPointF(lx+16,ly+5));
            p.setPen(QColor(0xcc,0xcc,0xcc));
            QString name = (si < m_lineNames.size()) ? m_lineNames[si]
                                                      : QString("Serie %1").arg(si+1);
            p.drawText(QPointF(lx+20, ly+9), name);
            ly += 16;
        }
    }
    p.setPen(QColor(0xbb,0xdd,0xff,190));
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 9, QFont::Bold));
    p.drawText(QRectF(area.left(), area.top()-18, area.width(), 18),
               Qt::AlignHCenter|Qt::AlignVCenter, "Grafico a Linee");
}

/* ══════════════════════════════════════════════════════════════
   Polare r = f(θ) — rendering
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::drawPolar(QPainter& p, const QRectF& area) {
    p.setRenderHint(QPainter::Antialiasing);
    const double cx = area.center().x() + m_panNC.x();
    const double cy = area.center().y() + m_panNC.y();
    const double baseR = std::min(area.width(), area.height()) * 0.40;
    const double R = baseR * m_zoomNC;   /* 1 unità = R pixel */

    if (m_formula.isEmpty()) {
        p.setPen(QColor(0x66,0x88,0xaa));
        p.drawText(area, Qt::AlignCenter, "Inserisci r = f(\xce\xb8) e clicca Traccia");
        return;
    }

    /* Valuta la formula e calcola r_max per la griglia */
    FormulaParser fp(m_formula);
    if (!fp.ok()) {
        p.setPen(QColor(0xff,0x66,0x44));
        p.drawText(area, Qt::AlignCenter, "Errore: " + fp.err());
        return;
    }
    int N = 800;
    double dt = (m_polarTMax - m_polarTMin) / N;
    double rMax = 0.0;
    for (int i = 0; i <= N; ++i) {
        double t = m_polarTMin + i * dt;
        double r = std::abs(fp.eval(t));
        if (std::isfinite(r)) rMax = std::max(rMax, r);
    }
    if (rMax < 1e-12) rMax = 1.0;

    /* Griglia polare */
    {
        QPainterPath clip; clip.addRect(area);
        p.setClipPath(clip);
        p.setBrush(Qt::NoBrush);

        /* Cerchi concentrici */
        int nRings = 4;
        double rStep = niceStep(rMax, nRings);
        for (double rv = rStep; rv <= rMax * 1.01; rv += rStep) {
            double sr = rv / rMax * R;
            p.setPen(QPen(QColor(0x35,0x35,0x35), 0.8));
            p.drawEllipse(QPointF(cx, cy), sr, sr);
            p.setPen(QColor(0x55,0x55,0x55));
            p.setFont(QFont("Inter,Ubuntu,sans-serif", 7));
            p.drawText(QPointF(cx + sr + 2, cy - 2), fmtNum(rv));
        }
        /* Raggi angolari ogni 30° */
        p.setPen(QPen(QColor(0x30,0x30,0x30), 0.6));
        for (int deg = 0; deg < 360; deg += 30) {
            double a = deg * M_PI / 180.0;
            p.drawLine(QPointF(cx, cy),
                       QPointF(cx + std::cos(a)*R*1.05, cy - std::sin(a)*R*1.05));
        }
        /* Etichette angoli cardinali */
        p.setPen(QColor(0x66,0x88,0xaa));
        p.setFont(QFont("Inter,Ubuntu,sans-serif", 8));
        static const char* kAngLbl[] = {"0","90\xc2\xb0","180\xc2\xb0","270\xc2\xb0"};
        static const int   kAngDeg[] = { 0,  90,  180,  270 };
        for (int i = 0; i < 4; ++i) {
            double a = kAngDeg[i] * M_PI / 180.0;
            double lx = cx + std::cos(a)*(R + 12);
            double ly = cy - std::sin(a)*(R + 12);
            p.drawText(QRectF(lx-16, ly-8, 32, 16),
                       Qt::AlignCenter, kAngLbl[i]);
        }
        p.setClipping(false);
    }

    /* Curva polare */
    QPainterPath path;
    bool first = true;
    for (int i = 0; i <= N; ++i) {
        double t  = m_polarTMin + i * dt;
        double r  = fp.eval(t);
        if (!std::isfinite(r)) { first = true; continue; }
        double sx = cx + (r / rMax) * R * std::cos(t);
        double sy = cy - (r / rMax) * R * std::sin(t);
        if (first) { path.moveTo(sx, sy); first = false; }
        else        path.lineTo(sx, sy);
    }
    p.setPen(QPen(QColor(0x00,0xcc,0xff), 1.8));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);

    /* Titolo e formula */
    p.setPen(QColor(0xbb,0xdd,0xff,190));
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 9, QFont::Bold));
    p.drawText(QRectF(area.left(), area.top(), area.width(), 22),
               Qt::AlignHCenter|Qt::AlignVCenter,
               QString("Polare  r = %1").arg(m_formula));
    p.setPen(QColor(0x44,0x55,0x55));
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 8));
    p.drawText(QRectF(area.left(), area.bottom()-16, area.width(), 14),
               Qt::AlignRight,
               "\xce\xb8\xe2\x88\x88 [" + fmtNum(m_polarTMin) + ", " +
               fmtNum(m_polarTMax) + "]  \xe2\x80\x94  Rotella zoom \xc2\xb7 Trascina");
}

/* ══════════════════════════════════════════════════════════════
   Radar / Spider — rendering
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::drawRadar(QPainter& p, const QRectF& area) {
    p.setRenderHint(QPainter::Antialiasing);
    const int n = m_values.size();
    if (n < 3) {
        p.setPen(QColor(0x66,0x88,0xaa));
        p.drawText(area, Qt::AlignCenter,
                   "Servono almeno 3 categorie.\nFormato: Categoria: valore");
        return;
    }

    const double cx = area.center().x() + m_panNC.x();
    const double cy = area.center().y() + m_panNC.y();
    const double R  = std::min(area.width(), area.height()) * 0.38 * m_zoomNC;

    double vMin = *std::min_element(m_values.begin(), m_values.end());
    double vMax = *std::max_element(m_values.begin(), m_values.end());
    /* Baseline a 0 se tutti positivi, altrimenti a vMin per supportare negativi */
    if (vMin >= 0.0) vMin = 0.0;
    if (vMax <= vMin) vMax = vMin + 1.0;
    const double vRange = vMax - vMin;

    /* Griglia poligonale */
    p.setBrush(Qt::NoBrush);
    static const int kRings = 4;
    for (int ring = 1; ring <= kRings; ++ring) {
        double frac = (double)ring / kRings;
        QPainterPath poly;
        for (int k = 0; k < n; ++k) {
            double angle = -M_PI/2.0 + 2*M_PI*k/n;
            double px = cx + std::cos(angle)*R*frac;
            double py = cy + std::sin(angle)*R*frac;
            if (k == 0) poly.moveTo(px, py); else poly.lineTo(px, py);
        }
        poly.closeSubpath();
        p.setPen(QPen(QColor(0x33,0x33,0x33), 0.8));
        p.drawPath(poly);
        /* etichetta valore */
        p.setPen(QColor(0x55,0x55,0x55));
        p.setFont(QFont("Inter,Ubuntu,sans-serif", 7));
        p.drawText(QPointF(cx + 3, cy - R*frac + 2), fmtNum(vMin + vRange*frac));
    }
    /* Assi radiali */
    p.setPen(QPen(QColor(0x44,0x44,0x44), 0.8));
    for (int k = 0; k < n; ++k) {
        double angle = -M_PI/2.0 + 2*M_PI*k/n;
        p.drawLine(QPointF(cx, cy),
                   QPointF(cx + std::cos(angle)*R, cy + std::sin(angle)*R));
    }

    /* Poligono dati */
    QPainterPath poly;
    for (int k = 0; k < n; ++k) {
        double angle = -M_PI/2.0 + 2*M_PI*k/n;
        double frac  = std::max(0.0, std::min(1.0, (m_values[k] - vMin) / vRange));
        double px = cx + std::cos(angle)*R*frac;
        double py = cy + std::sin(angle)*R*frac;
        if (k == 0) poly.moveTo(px, py); else poly.lineTo(px, py);
    }
    poly.closeSubpath();
    QColor fillCol(0x00,0xcc,0xff,60);
    QColor lineCol(0x00,0xcc,0xff,220);
    p.setBrush(fillCol);
    p.setPen(QPen(lineCol, 1.8));
    p.drawPath(poly);
    /* Punti */
    p.setBrush(lineCol); p.setPen(Qt::NoPen);
    for (int k = 0; k < n; ++k) {
        double angle = -M_PI/2.0 + 2*M_PI*k/n;
        double frac  = std::max(0.0, std::min(1.0, (m_values[k] - vMin) / vRange));
        p.drawEllipse(QPointF(cx + std::cos(angle)*R*frac,
                              cy + std::sin(angle)*R*frac), 3.5, 3.5);
    }

    /* Etichette assi */
    p.setPen(QColor(0xcc,0xcc,0xcc));
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 9));
    for (int k = 0; k < n; ++k) {
        double angle = -M_PI/2.0 + 2*M_PI*k/n;
        double lx = cx + std::cos(angle)*(R + 18);
        double ly = cy + std::sin(angle)*(R + 18);
        QString lbl = (k < m_labels.size()) ? m_labels[k] : QString::number(k+1);
        p.drawText(QRectF(lx-40, ly-10, 80, 20), Qt::AlignCenter, lbl);
    }

    p.setPen(QColor(0xbb,0xdd,0xff,190));
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 9, QFont::Bold));
    p.drawText(QRectF(area.left(), area.top(), area.width(), 22),
               Qt::AlignHCenter|Qt::AlignVCenter, "Grafico Radar");
}

/* ══════════════════════════════════════════════════════════════
   Bolle (Bubble) — rendering
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::drawBubble(QPainter& p, const QRectF& area) {
    p.setRenderHint(QPainter::Antialiasing);
    if (m_pts3d.isEmpty()) {
        p.setPen(QColor(0x66,0x88,0xaa));
        p.drawText(area, Qt::AlignCenter,
                   "Formato: x, y, raggio [, Nome]");
        return;
    }
    /* Calcola bounds da x,y */
    double xMn=1e18,xMx=-1e18,yMn=1e18,yMx=-1e18,rMx=0;
    for (auto& pt : m_pts3d) {
        xMn=std::min(xMn,pt.x); xMx=std::max(xMx,pt.x);
        yMn=std::min(yMn,pt.y); yMx=std::max(yMx,pt.y);
        rMx=std::max(rMx,std::abs(pt.z));
    }
    if (xMn>=xMx){xMn-=1;xMx+=1;} if(yMn>=yMx){yMn-=1;yMx+=1;}
    double px=(xMx-xMn)*0.1,py=(yMx-yMn)*0.1;
    m_xVMin=xMn-px; m_xVMax=xMx+px;
    m_yVMin=yMn-py; m_yVMax=yMx+py;
    drawGrid(p, area);

    const double rPxMax = std::min(area.width(), area.height()) * 0.08;
    const double rPxMin = 4.0;

    p.setClipRect(area.adjusted(-1,-1,1,1));
    for (int i = 0; i < m_pts3d.size(); ++i) {
        auto& pt = m_pts3d[i];
        double rNorm = (rMx > 0) ? std::abs(pt.z) / rMx : 0.5;
        double rPx   = rPxMin + rNorm * (rPxMax - rPxMin);
        QColor col   = paletteColor(i).lighter(110);
        col.setAlpha(160);
        QPointF sp = dataToScreen(pt.x, pt.y, area);
        p.setBrush(col);
        p.setPen(QPen(col.lighter(140), 1.0));
        p.drawEllipse(sp, rPx, rPx);
        if (!pt.label.isEmpty()) {
            p.setPen(QColor(0xdd,0xdd,0xdd));
            p.setFont(QFont("Inter,Ubuntu,sans-serif", 7));
            p.drawText(QPointF(sp.x()+rPx+2, sp.y()+4), pt.label);
        }
    }
    p.setClipping(false);

    p.setPen(QColor(0xbb,0xdd,0xff,190));
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 9, QFont::Bold));
    p.drawText(QRectF(area.left(), area.top()-18, area.width(), 18),
               Qt::AlignHCenter|Qt::AlignVCenter, "Grafico a Bolle");
}

/* ══════════════════════════════════════════════════════════════
   Heatmap — rendering
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::drawHeatmap(QPainter& p, const QRectF& area) {
    if (m_heatData.isEmpty()) {
        p.setPen(QColor(0x66,0x88,0xaa));
        p.drawText(area, Qt::AlignCenter,
                   "Formato: valori separati da spazi/virgole, una riga per row\n"
                   "Es:\n  1 2 3\n  4 5 6\n  7 8 9");
        return;
    }
    int rows = m_heatData.size();
    int cols = 0;
    for (auto& r : m_heatData) cols = std::max(cols, (int)r.size());
    if (cols == 0) return;

    /* min/max globale */
    double vMn=1e18, vMx=-1e18;
    for (auto& row : m_heatData)
        for (double v : row) { vMn=std::min(vMn,v); vMx=std::max(vMx,v); }
    if (vMn >= vMx) { vMn -= 1; vMx += 1; }

    /* Colore: cold→hot */
    auto heatColor = [&](double v) -> QColor {
        double t = (v - vMn) / (vMx - vMn);
        t = std::max(0.0, std::min(1.0, t));
        static const QColor stops[] = {
            {0x08,0x08,0x80}, {0x00,0x88,0xff}, {0x00,0xcc,0x88},
            {0xff,0xcc,0x00}, {0xff,0x22,0x00}
        };
        double seg = t * 4.0;
        int    idx = (int)seg; idx = std::min(idx, 3);
        double f   = seg - idx;
        auto lerp = [](int a, int b, double f){ return (int)(a + (b-a)*f); };
        return QColor(lerp(stops[idx].red(),  stops[idx+1].red(),  f),
                      lerp(stops[idx].green(),stops[idx+1].green(),f),
                      lerp(stops[idx].blue(), stops[idx+1].blue(), f));
    };

    /* Area di disegno con margine per etichette */
    double lblW = m_heatRowLbls.isEmpty() ? 0 : 60;
    double lblH = m_heatColLbls.isEmpty() ? 0 : 20;
    QRectF grid(area.left() + lblW, area.top() + 20,
                area.width() - lblW - 44, area.height() - 20 - lblH - 8);
    double cw = grid.width()  / cols;
    double ch = grid.height() / rows;

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < m_heatData[r].size(); ++c) {
            double v  = m_heatData[r][c];
            QRectF cell(grid.left() + c*cw, grid.top() + r*ch, cw, ch);
            p.fillRect(cell, heatColor(v));
            /* valore dentro la cella se abbastanza grande */
            if (cw > 30 && ch > 14) {
                p.setPen(QColor(0,0,0,160));
                p.setFont(QFont("Inter,Ubuntu,sans-serif", 7));
                p.drawText(cell, Qt::AlignCenter, fmtNum(v));
            }
        }
    }
    /* Etichette righe */
    if (!m_heatRowLbls.isEmpty()) {
        p.setPen(QColor(0xcc,0xcc,0xcc));
        p.setFont(QFont("Inter,Ubuntu,sans-serif", 8));
        for (int r = 0; r < std::min(rows, (int)m_heatRowLbls.size()); ++r)
            p.drawText(QRectF(area.left(), grid.top()+r*ch, lblW-4, ch),
                       Qt::AlignRight|Qt::AlignVCenter, m_heatRowLbls[r]);
    }
    /* Etichette colonne */
    if (!m_heatColLbls.isEmpty()) {
        p.setPen(QColor(0xcc,0xcc,0xcc));
        p.setFont(QFont("Inter,Ubuntu,sans-serif", 8));
        for (int c = 0; c < std::min(cols, (int)m_heatColLbls.size()); ++c)
            p.drawText(QRectF(grid.left()+c*cw, grid.bottom()+2, cw, lblH),
                       Qt::AlignCenter, m_heatColLbls[c]);
    }
    /* Barra colori */
    {
        QRectF bar(area.right()-36, grid.top(), 16, grid.height());
        int steps = 100;
        double sh = bar.height() / steps;
        for (int i = 0; i < steps; ++i) {
            double t = 1.0 - (double)i / steps;
            p.fillRect(QRectF(bar.left(), bar.top()+i*sh, bar.width(), sh+1),
                       heatColor(vMn + t*(vMx-vMn)));
        }
        p.setPen(QColor(0x88,0x88,0x88));
        p.drawRect(bar);
        p.setPen(QColor(0xcc,0xcc,0xcc));
        p.setFont(QFont("Inter,Ubuntu,sans-serif", 7));
        p.drawText(QPointF(bar.right()+2, bar.top()+8),    fmtNum(vMx));
        p.drawText(QPointF(bar.right()+2, bar.bottom()+4), fmtNum(vMn));
    }

    p.setPen(QColor(0xbb,0xdd,0xff,190));
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 9, QFont::Bold));
    p.drawText(QRectF(area.left(), area.top(), area.width(), 20),
               Qt::AlignHCenter|Qt::AlignVCenter, "Heatmap");
}

/* ══════════════════════════════════════════════════════════════
   Candele OHLC — rendering
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::drawCandlestick(QPainter& p, const QRectF& area) {
    p.setRenderHint(QPainter::Antialiasing, false);
    if (m_ohlcPts.isEmpty()) {
        p.setPen(QColor(0x66,0x88,0xaa));
        p.drawText(area, Qt::AlignCenter,
                   "Formato: Etichetta, Open, High, Low, Close\n"
                   "Es:  Gen, 100, 115, 98, 112");
        return;
    }
    int n = m_ohlcPts.size();
    /* Y range */
    double yMn=1e18, yMx=-1e18;
    for (auto& c : m_ohlcPts) { yMn=std::min({yMn,c.l,c.o,c.c}); yMx=std::max({yMx,c.h,c.o,c.c}); }
    double pad = (yMx-yMn)*0.06; yMn-=pad; yMx+=pad;
    if (yMn>=yMx){yMn-=1;yMx+=1;}

    /* Griglia Y */
    double yStep = niceStep(yMx-yMn, 6);
    double yFirst = std::ceil(yMn/yStep)*yStep;
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 8));
    for (double y=yFirst; y<=yMx+yStep*0.01; y+=yStep) {
        double sy = area.bottom() - (y-yMn)/(yMx-yMn)*area.height();
        p.setPen(QPen(QColor(0x33,0x33,0x33),0.8));
        p.drawLine(QPointF(area.left(),sy), QPointF(area.right(),sy));
        p.setPen(QColor(0x77,0x77,0x77));
        p.drawText(QRectF(0, sy-8, area.left()-4, 16),
                   Qt::AlignRight|Qt::AlignVCenter, fmtNum(y));
    }

    auto toY = [&](double v) {
        return area.bottom() - (v-yMn)/(yMx-yMn)*area.height();
    };

    double totalW = area.width();
    double slotW  = totalW / n;
    double bodyW  = std::max(2.0, slotW * 0.55);

    for (int i = 0; i < n; ++i) {
        auto& c  = m_ohlcPts[i];
        double cx = area.left() + (i + 0.5) * slotW;
        bool bull = (c.c >= c.o);
        QColor col = bull ? QColor(0x26,0xbb,0x74) : QColor(0xee,0x44,0x44);

        double yH = toY(c.h), yL = toY(c.l);
        double yO = toY(c.o), yC = toY(c.c);
        double yBodyTop = std::min(yO, yC), yBodyBot = std::max(yO, yC);
        if (yBodyBot - yBodyTop < 1.0) yBodyBot = yBodyTop + 1.0;

        /* Stoppino */
        p.setPen(QPen(col, 1.2));
        p.drawLine(QPointF(cx, yH), QPointF(cx, yBodyTop));
        p.drawLine(QPointF(cx, yBodyBot), QPointF(cx, yL));

        /* Corpo */
        QRectF body(cx - bodyW/2, yBodyTop, bodyW, yBodyBot - yBodyTop);
        if (bull) { p.setBrush(col); p.setPen(QPen(col.darker(130), 0.8)); }
        else      { p.setBrush(QColor(0x22,0x22,0x22)); p.setPen(QPen(col, 0.8)); }
        p.drawRect(body);

        /* Etichetta X */
        if (slotW > 18) {
            p.setPen(QColor(0x77,0x77,0x77));
            p.setFont(QFont("Inter,Ubuntu,sans-serif", 7));
            p.drawText(QRectF(cx-slotW/2, area.bottom()+2, slotW, 14),
                       Qt::AlignCenter, c.label);
        }
    }
    p.setPen(QColor(0xbb,0xdd,0xff,190));
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 9, QFont::Bold));
    p.drawText(QRectF(area.left(), area.top()-18, area.width(), 18),
               Qt::AlignHCenter|Qt::AlignVCenter, "Candele OHLC");
}

/* ══════════════════════════════════════════════════════════════
   Area riempita — rendering
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::drawArea(QPainter& p, const QRectF& area) {
    p.setRenderHint(QPainter::Antialiasing);
    if (m_lineSeries.isEmpty()) {
        p.setPen(QColor(0x66,0x88,0xaa));
        p.drawText(area, Qt::AlignCenter, "Nessun dato");
        return;
    }
    /* aggiorna bounds da tutti i punti */
    double xMn=1e18,xMx=-1e18,yMn=1e18,yMx=-1e18;
    for (auto& s : m_lineSeries)
        for (auto& pt : s) {
            xMn=std::min(xMn,pt.x()); xMx=std::max(xMx,pt.x());
            yMn=std::min(yMn,pt.y()); yMx=std::max(yMx,pt.y());
        }
    if (xMn>=xMx){xMn-=1;xMx+=1;} if(yMn>=yMx){yMn-=1;yMx+=1;}
    double px=(xMx-xMn)*0.08, py=(yMx-yMn)*0.12+0.5;
    m_xVMin=xMn-px; m_xVMax=xMx+px;
    m_yVMin=std::min(yMn-py, 0.0); m_yVMax=yMx+py;

    drawGrid(p, area);
    p.setClipRect(area.adjusted(-1,-1,1,1));

    /* linea di base y=0 in coordinate schermo */
    QPointF baseScreen = dataToScreen(0.0, 0.0, area);
    double  baseY = std::max(area.top(), std::min(area.bottom(), baseScreen.y()));

    for (int si = 0; si < m_lineSeries.size(); ++si) {
        const auto& s  = m_lineSeries[si];
        if (s.isEmpty()) continue;
        QColor col = paletteColor(si);

        /* riempi area */
        QPainterPath fill;
        QPointF fp0 = dataToScreen(s[0].x(), s[0].y(), area);
        fill.moveTo(fp0.x(), baseY);
        fill.lineTo(fp0);
        for (int j = 1; j < s.size(); ++j)
            fill.lineTo(dataToScreen(s[j].x(), s[j].y(), area));
        QPointF fpL = dataToScreen(s.last().x(), s.last().y(), area);
        fill.lineTo(fpL.x(), baseY);
        fill.closeSubpath();
        QColor fillCol = col;
        fillCol.setAlpha(std::max(20, 70 - si * 15));
        p.setBrush(fillCol);
        p.setPen(Qt::NoPen);
        p.drawPath(fill);

        /* linea superiore */
        QPainterPath line;
        line.moveTo(dataToScreen(s[0].x(), s[0].y(), area));
        for (int j = 1; j < s.size(); ++j)
            line.lineTo(dataToScreen(s[j].x(), s[j].y(), area));
        p.setPen(QPen(col, 2.0));
        p.setBrush(Qt::NoBrush);
        p.drawPath(line);
    }
    p.setClipping(false);

    /* legenda */
    {
        double lx = area.right() - 140;
        double ly = area.top() + 8;
        p.setFont(QFont("Inter,Ubuntu,sans-serif", 8));
        for (int si = 0; si < m_lineSeries.size(); ++si) {
            QColor col = paletteColor(si);
            QColor fill = col; fill.setAlpha(80);
            p.setBrush(fill); p.setPen(Qt::NoPen);
            p.fillRect(QRectF(lx, ly+1, 16, 8), fill);
            p.setPen(QPen(col, 2)); p.drawLine(QPointF(lx,ly+5), QPointF(lx+16,ly+5));
            p.setPen(QColor(0xcc,0xcc,0xcc));
            QString name = (si < m_lineNames.size()) ? m_lineNames[si]
                                                      : QString("Serie %1").arg(si+1);
            p.drawText(QPointF(lx+20, ly+9), name);
            ly += 16;
        }
    }
    p.setPen(QColor(0xbb,0xdd,0xff,190));
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 9, QFont::Bold));
    p.drawText(QRectF(area.left(), area.top()-18, area.width(), 18),
               Qt::AlignHCenter|Qt::AlignVCenter, "Grafico ad Area");
}

/* ══════════════════════════════════════════════════════════════
   Cascata (Waterfall) — rendering
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::drawWaterfall(QPainter& p, const QRectF& area) {
    p.setRenderHint(QPainter::Antialiasing, false);
    if (m_values.isEmpty()) {
        p.setPen(QColor(0x66,0x88,0xaa));
        p.drawText(area, Qt::AlignCenter,
                   "Formato: Etichetta:valore  (una per riga)\n"
                   "Es:\n  Inizio:1000\n  Vendite:+350\n  Costi:-200\n  Fine:1150");
        return;
    }

    int n = m_values.size();
    /* calcola avvio/fine cumulata per ogni barra */
    QVector<double> starts(n), ends(n);
    double cum = 0.0;
    double vMin = 0.0, vMax = 0.0;
    for (int i = 0; i < n; ++i) {
        starts[i] = cum;
        ends[i]   = cum + m_values[i];
        cum += m_values[i];
        vMin = std::min({vMin, starts[i], ends[i]});
        vMax = std::max({vMax, starts[i], ends[i]});
    }
    if (vMin >= vMax) { vMin -= 1; vMax += 1; }
    double pad = (vMax - vMin) * 0.12;
    vMin -= pad; vMax += pad;

    /* area plot con margini per etichette */
    QRectF plot(area.left() + 52, area.top() + 24,
                area.width() - 60, area.height() - 48);

    /* griglie orizzontali */
    double yStep = niceStep(vMax - vMin, 5);
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 7));
    for (double yv = std::ceil(vMin/yStep)*yStep; yv <= vMax + yStep*0.5; yv += yStep) {
        double sy = plot.bottom() - (yv - vMin) / (vMax - vMin) * plot.height();
        if (sy < plot.top() || sy > plot.bottom()) continue;
        p.setPen(QPen(QColor(0x2a,0x2a,0x2a), 0.8));
        p.drawLine(QPointF(plot.left(), sy), QPointF(plot.right(), sy));
        p.setPen(QColor(0x66,0x66,0x66));
        p.drawText(QRectF(area.left(), sy-8, plot.left()-area.left()-3, 16),
                   Qt::AlignRight|Qt::AlignVCenter, fmtNum(yv));
    }

    /* barre */
    double bw  = plot.width() / n * 0.68;
    double gap = plot.width() / n * 0.16;
    for (int i = 0; i < n; ++i) {
        double xc = plot.left() + (i + 0.5) / n * plot.width();
        double y1 = plot.bottom() - (starts[i] - vMin) / (vMax - vMin) * plot.height();
        double y2 = plot.bottom() - (ends[i]   - vMin) / (vMax - vMin) * plot.height();
        if (y1 > y2) std::swap(y1, y2);
        double barH = std::max(2.0, y2 - y1);
        QRectF bar(xc - bw/2, y1, bw, barH);

        /* colore: positivo=verde, negativo=rosso, ultima=cyan */
        QColor col;
        if (i == n-1)            col = QColor(0x00,0xbf,0xd8);
        else if (m_values[i] >= 0) col = QColor(0x50,0xc8,0x78);
        else                     col = QColor(0xff,0x66,0x55);

        p.setBrush(col.lighter(115));
        p.setPen(QPen(col.darker(140), 1));
        p.drawRect(bar);

        /* connettore tratteggiato verso la prossima barra */
        if (i < n-1) {
            double ny = plot.bottom() - (ends[i] - vMin) / (vMax - vMin) * plot.height();
            double nextX = plot.left() + (i + 1.0) / n * plot.width() - bw/2;
            p.setPen(QPen(QColor(0x44,0x44,0x44), 0.8, Qt::DashLine));
            p.drawLine(QPointF(xc + bw/2, ny), QPointF(nextX - gap*0.5, ny));
        }

        /* valore sopra/sotto la barra */
        p.setPen(QColor(0xee,0xee,0xee));
        p.setFont(QFont("Inter,Ubuntu,sans-serif", 8));
        double valY = (m_values[i] >= 0) ? y1 - 14 : y2 + 2;
        valY = std::max(plot.top(), std::min(plot.bottom() - 12, valY));
        p.drawText(QRectF(xc - bw, valY, bw*2, 12), Qt::AlignCenter, fmtNum(ends[i]));

        /* etichetta asse X */
        if (i < m_labels.size()) {
            p.setPen(QColor(0xaa,0xaa,0xaa));
            p.setFont(QFont("Inter,Ubuntu,sans-serif", 8));
            p.drawText(QRectF(xc - bw, plot.bottom() + 3, bw*2, 14),
                       Qt::AlignCenter, m_labels[i]);
        }
    }

    p.setPen(QColor(0xbb,0xdd,0xff,190));
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 9, QFont::Bold));
    p.drawText(QRectF(area.left(), area.top(), area.width(), 22),
               Qt::AlignHCenter|Qt::AlignVCenter, "Grafico a Cascata (Waterfall)");
}

/* ══════════════════════════════════════════════════════════════
   Scalini (Step) — rendering
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::drawStep(QPainter& p, const QRectF& area) {
    p.setRenderHint(QPainter::Antialiasing);
    if (m_lineSeries.isEmpty()) {
        p.setPen(QColor(0x66,0x88,0xaa));
        p.drawText(area, Qt::AlignCenter, "Nessun dato");
        return;
    }
    /* aggiorna bounds */
    double xMn=1e18,xMx=-1e18,yMn=1e18,yMx=-1e18;
    for (auto& s : m_lineSeries)
        for (auto& pt : s) {
            xMn=std::min(xMn,pt.x()); xMx=std::max(xMx,pt.x());
            yMn=std::min(yMn,pt.y()); yMx=std::max(yMx,pt.y());
        }
    if (xMn>=xMx){xMn-=1;xMx+=1;} if(yMn>=yMx){yMn-=1;yMx+=1;}
    double px=(xMx-xMn)*0.08, py=(yMx-yMn)*0.12+0.5;
    m_xVMin=xMn-px; m_xVMax=xMx+px;
    m_yVMin=yMn-py; m_yVMax=yMx+py;

    drawGrid(p, area);
    p.setClipRect(area.adjusted(-1,-1,1,1));

    for (int si = 0; si < m_lineSeries.size(); ++si) {
        const auto& s  = m_lineSeries[si];
        QColor col = paletteColor(si);

        QPainterPath path;
        for (int j = 0; j < s.size(); ++j) {
            QPointF sp = dataToScreen(s[j].x(), s[j].y(), area);
            if (j == 0) {
                path.moveTo(sp);
            } else {
                /* scalino: prima orizzontale (fino a x corrente), poi verticale */
                QPointF prev = dataToScreen(s[j-1].x(), s[j-1].y(), area);
                path.lineTo(QPointF(sp.x(), prev.y()));
                path.lineTo(sp);
            }
        }
        p.setPen(QPen(col, 2.0));
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);

        /* dot su ogni punto dato */
        p.setPen(Qt::NoPen);
        p.setBrush(col);
        for (auto& pt : s)
            p.drawEllipse(dataToScreen(pt.x(), pt.y(), area), 3.5, 3.5);
    }
    p.setClipping(false);

    /* legenda */
    {
        double lx = area.right() - 140;
        double ly = area.top() + 8;
        p.setFont(QFont("Inter,Ubuntu,sans-serif", 8));
        for (int si = 0; si < m_lineSeries.size(); ++si) {
            QColor col = paletteColor(si);
            p.setPen(QPen(col, 2)); p.drawLine(QPointF(lx,ly+5), QPointF(lx+16,ly+5));
            p.setPen(QColor(0xcc,0xcc,0xcc));
            QString name = (si < m_lineNames.size()) ? m_lineNames[si]
                                                      : QString("Serie %1").arg(si+1);
            p.drawText(QPointF(lx+20, ly+9), name);
            ly += 16;
        }
    }

    p.setPen(QColor(0xbb,0xdd,0xff,190));
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 9, QFont::Bold));
    p.drawText(QRectF(area.left(), area.top()-18, area.width(), 18),
               Qt::AlignHCenter|Qt::AlignVCenter, "Grafico a Scalini (Step)");
}
