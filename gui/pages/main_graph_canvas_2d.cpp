/*
 * main_graph_canvas_2d.cpp — GraficoCanvas: grafici 2D base
 * ============================================================
 * Griglia, Cartesiano, Torta, Istogramma, Scatter XY, Grafo.
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

void GraficoCanvas::drawGrid(QPainter& p, const QRectF& a) {
    p.setFont(QFont(m_style.fontFamily, m_style.fontSize));

    double xStep = niceStep(m_xVMax - m_xVMin, 8);
    double yStep = niceStep(m_yVMax - m_yVMin, 6);
    double xFirst = std::ceil(m_xVMin / xStep) * xStep;
    double yFirst = std::ceil(m_yVMin / yStep) * yStep;

    /* linee verticali + etichette X */
    for (double x = xFirst; x <= m_xVMax + xStep * 0.01; x += xStep) {
        double sx = a.left() + (x - m_xVMin) / (m_xVMax - m_xVMin) * a.width();
        if (m_style.showGrid) {
            p.setPen(QPen(m_style.gridColor, 1));
            p.drawLine(QPointF(sx, a.top()), QPointF(sx, a.bottom()));
        }
        p.setPen(m_style.textColor);
        p.drawText(QRectF(sx - 26, a.bottom() + 3, 52, 14), Qt::AlignCenter, fmtNum(x));
    }

    /* linee orizzontali + etichette Y */
    for (double y = yFirst; y <= m_yVMax + yStep * 0.01; y += yStep) {
        double sy = a.bottom() - (y - m_yVMin) / (m_yVMax - m_yVMin) * a.height();
        if (m_style.showGrid) {
            p.setPen(QPen(m_style.gridColor, 1));
            p.drawLine(QPointF(a.left(), sy), QPointF(a.right(), sy));
        }
        p.setPen(m_style.textColor);
        p.drawText(QRectF(0, sy - 8, a.left() - 4, 16),
                   Qt::AlignRight | Qt::AlignVCenter, fmtNum(y));
    }

    if (m_axes2dPos == AtData) {
        /* assi X=0 e Y=0 attraverso tutto il plot */
        p.setPen(QPen(m_style.axisColor, 1));
        if (m_xVMin <= 0 && m_xVMax >= 0) {
            double sx = a.left() + (-m_xVMin) / (m_xVMax - m_xVMin) * a.width();
            p.drawLine(QPointF(sx, a.top()), QPointF(sx, a.bottom()));
        }
        if (m_yVMin <= 0 && m_yVMax >= 0) {
            double sy = a.bottom() - (-m_yVMin) / (m_yVMax - m_yVMin) * a.height();
            p.drawLine(QPointF(a.left(), sy), QPointF(a.right(), sy));
        }
    } else {
        /* gizmo L in angolo — X→ rosso, Y↑ verde */
        QPointF org = gizmoOrigin(m_axes2dPos, a);
        const double gs = 32.0;
        p.setFont(QFont(m_style.fontFamily, m_style.fontSize, QFont::Bold));
        p.setPen(QPen(QColor(0xff,0x60,0x60), 1.5));
        p.drawLine(org, org + QPointF(gs, 0));
        p.setPen(QColor(0xff,0x60,0x60));
        p.drawText(org + QPointF(gs + 3, 4), "X");
        p.setPen(QPen(QColor(0x60,0xff,0x60), 1.5));
        p.drawLine(org, org + QPointF(0, -gs));
        p.setPen(QColor(0x60,0xff,0x60));
        p.drawText(org + QPointF(-10, -gs - 2), "Y");
    }

    /* bordo */
    p.setPen(QPen(m_style.axisColor, 1));
    p.drawRect(a);
}

/* ══════════════════════════════════════════════════════════════
   Cartesiano
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::drawCartesian(QPainter& p, const QRectF& a) {
    drawGrid(p, a);
    if (m_cartPts.size() < 2) {
        p.setPen(QColor(0xff,0x70,0x70));
        p.setFont(QFont("Inter", 11));
        p.drawText(a, Qt::AlignCenter,
                   m_formula.isEmpty() ? "Inserisci una formula e premi Traccia"
                                       : "Formula non valida o nessun punto");
        return;
    }

    /* ── LAYER 0: banda di esclusione dominio (zona limite) ─────────── */
    if (m_limitActive) {
        p.setRenderHint(QPainter::Antialiasing);
        p.setClipRect(a);

        const double sx = dataToScreen(m_limitX, 0.0, a).x();
        const double pxPerUnit = (m_xVMax > m_xVMin)
            ? a.width() / (m_xVMax - m_xVMin) : 1.0;
        const double bHalf = qMax(8.0, pxPerUnit * 0.4); /* larghezza banda ≈ 0.4 unità */

        /* Gradiente arancio (sinistra, x < a) */
        QLinearGradient lgL(sx - bHalf, 0, sx, 0);
        lgL.setColorAt(0.0, QColor(0xff, 0x99, 0x00,  0));
        lgL.setColorAt(1.0, QColor(0xff, 0x99, 0x00, 60));
        p.setBrush(lgL); p.setPen(Qt::NoPen);
        p.drawRect(QRectF(sx - bHalf, a.top(), bHalf, a.height()));

        /* Gradiente azzurro (destra, x > a) */
        QLinearGradient lgR(sx, 0, sx + bHalf, 0);
        lgR.setColorAt(0.0, QColor(0x00, 0xaa, 0xff, 60));
        lgR.setColorAt(1.0, QColor(0x00, 0xaa, 0xff,  0));
        p.setBrush(lgR); p.setPen(Qt::NoPen);
        p.drawRect(QRectF(sx, a.top(), bHalf, a.height()));

        /* Linea verticale tratteggiata a x = a */
        p.setPen(QPen(QColor(0xff, 0xcc, 0x00, 200), 1.5, Qt::DashLine));
        p.drawLine(QPointF(sx, a.top()), QPointF(sx, a.bottom()));

        /* Etichetta "x → a" sopra la banda */
        QFont lf("Inter,Ubuntu,sans-serif", 8); lf.setBold(true);
        p.setFont(lf);
        p.setPen(QColor(0xff, 0xcc, 0x44, 230));
        QString xLbl = QString("x \xe2\x86\x92 %1").arg(m_limitX, 0, 'g', 5);
        p.drawText(QRectF(sx - 46, a.top() + 5, 92, 16), Qt::AlignCenter, xLbl);

        /* Riga orizzontale tratteggiata a y = L (se il valore è disponibile) */
        if (m_limitHasVal) {
            const double sy = dataToScreen(0.0, m_limitVal, a).y();
            p.setPen(QPen(QColor(0x00, 0xdd, 0x88, 160), 1.0, Qt::DotLine));
            p.drawLine(QPointF(a.left(), sy), QPointF(a.right(), sy));
        }

        p.setClipping(false);
    }

    /* ── LAYER 1: curva ──────────────────────────────────────────────── */
    p.setRenderHint(QPainter::Antialiasing);
    p.setClipRect(a);
    p.setPen(QPen(kPal[0], 2));
    QPainterPath path;
    bool first = true;
    for (auto& pt : m_cartPts) {
        if (!std::isfinite(pt.y())) { first = true; continue; }
        QPointF sp = dataToScreen(pt.x(), pt.y(), a);
        if (first) { path.moveTo(sp); first = false; }
        else         path.lineTo(sp);
    }
    p.drawPath(path);
    p.setClipping(false);

    /* ── LAYER 2: marcatori del limite (sopra la curva) ─────────────── */
    if (m_limitActive) {
        p.setRenderHint(QPainter::Antialiasing);
        p.setClipRect(a);
        const double sx = dataToScreen(m_limitX, 0.0, a).x();

        /* Trova il valore della funzione in x=a (cerchio aperto = "buco") */
        FormulaParser fp(m_formula);
        if (fp.ok()) {
            const double ya = fp.eval(m_limitX);
            if (std::isfinite(ya)) {
                QPointF holeSc = dataToScreen(m_limitX, ya, a);
                if (a.contains(holeSc)) {
                    /* cerchio aperto rosso: punto escluso dal dominio */
                    p.setPen(QPen(QColor(0xff, 0x55, 0x55, 230), 2));
                    p.setBrush(m_style.bgColor);
                    p.drawEllipse(holeSc, 5.0, 5.0);
                }
            }
        }

        /* Marcatore valore limite L (cerchio verde pieno) */
        if (m_limitHasVal) {
            QPointF limSc = dataToScreen(m_limitX, m_limitVal, a);
            if (a.contains(limSc)) {
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(0x00, 0xdd, 0x88, 230));
                p.drawEllipse(limSc, 5.5, 5.5);

                QFont lf("Inter,Ubuntu,sans-serif", 8); lf.setBold(true);
                p.setFont(lf);
                p.setPen(QColor(0x00, 0xee, 0x99));
                const QString lLbl = QString("L = %1").arg(m_limitVal, 0, 'g', 5);
                p.drawText(QRectF(limSc.x() + 9, limSc.y() - 9, 80, 16),
                           Qt::AlignLeft | Qt::AlignVCenter, lLbl);
            }
        }
        (void)sx;
        p.setClipping(false);
    }

    /* etichetta formula */
    p.setPen(QColor(0x00,0xbf,0xd8,170));
    p.setFont(QFont("JetBrains Mono,Fira Code,Consolas,monospace", 10));
    p.drawText(QRectF(a.left()+6, a.top()+4, a.width()-8, 18),
               Qt::AlignLeft, "y = " + m_formula);
}

void GraficoCanvas::setLimitHighlight(double xPoint)
{
    m_limitActive = true;
    m_limitX      = xPoint;
    m_limitHasVal = false;
    update();
}

void GraficoCanvas::updateLimitValue(double limitVal)
{
    m_limitVal    = limitVal;
    m_limitHasVal = true;
    update();
}

/* ══════════════════════════════════════════════════════════════
   Torta
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::drawPie(QPainter& p, const QRectF& a) {
    if (m_values.isEmpty()) {
        p.setPen(QColor(0x77,0x77,0x77));
        p.drawText(a, Qt::AlignCenter, "Nessun dato\nFormato: etichetta:valore (una per riga)");
        return;
    }
    double total = 0;
    for (double v : m_values) if (v > 0) total += v;
    if (total <= 0) return;

    double cx = a.center().x() + m_panNC.x();
    double cy = a.center().y() + m_panNC.y();
    double r  = std::min(a.width(), a.height()) * 0.36 * m_zoomNC;
    QRectF pr(cx - r, cy - r, 2*r, 2*r);

    p.setRenderHint(QPainter::Antialiasing);
    int startA = 90 * 16;   /* 12 o'clock */
    for (int i = 0; i < m_values.size(); i++) {
        if (m_values[i] <= 0) continue;
        int spanA = -qRound(m_values[i] / total * 5760.0);  /* 5760 = 360*16, CW */
        p.setBrush(paletteColor(i));
        p.setPen(QPen(QColor(0x18,0x18,0x18), 2));
        p.drawPie(pr, startA, spanA);
        /* etichetta al centro dell'arco */
        double midDeg = (startA + spanA / 2.0) / 16.0;
        double midRad = midDeg * M_PI / 180.0;
        double lx = cx + r * 0.62 * std::cos(midRad);
        double ly = cy - r * 0.62 * std::sin(midRad);
        QString lbl = i < m_labels.size() ? m_labels[i] : QString::number(i+1);
        QString pct = QString::number(m_values[i]/total*100, 'f', 1) + "%";
        p.setPen(Qt::white);
        p.setFont(QFont("Inter,Ubuntu,sans-serif", 8));
        p.drawText(QRectF(lx-34, ly-11, 68, 22), Qt::AlignCenter,
                   lbl + "\n" + pct);
        startA += spanA;
    }
    /* legenda */
    double lx = a.right() - 120;
    double ly = a.top()   + 10;
    for (int i = 0; i < m_values.size() && i < 14; i++) {
        p.fillRect(QRectF(lx, ly + i*18, 11, 11), paletteColor(i));
        p.setPen(QColor(0xcc,0xcc,0xcc));
        p.setFont(QFont("Inter,Ubuntu,sans-serif", 8));
        QString lbl = i < m_labels.size() ? m_labels[i] : QString::number(i+1);
        p.drawText(QRectF(lx+14, ly + i*18 - 2, 105, 16),
                   Qt::AlignLeft | Qt::AlignVCenter, lbl);
    }
}

/* ══════════════════════════════════════════════════════════════
   Istogramma
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::drawHistogram(QPainter& p, const QRectF& a) {
    if (m_values.isEmpty()) {
        p.setPen(QColor(0x77,0x77,0x77));
        p.drawText(a, Qt::AlignCenter, "Nessun dato\nFormato: etichetta:valore (una per riga)");
        return;
    }
    double maxV = *std::max_element(m_values.begin(), m_values.end());
    if (maxV <= 0) maxV = 1;

    int n = m_values.size();
    double barW = a.width() / n;

    /* griglia orizzontale */
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 8));
    for (int i = 0; i <= 5; i++) {
        double frac = (double)i / 5;
        double sy = a.bottom() - frac * a.height() * m_zoomNC + m_panNC.y();
        if (sy < a.top() - 2 || sy > a.bottom() + 2) continue;
        p.setPen(QPen(QColor(0x35,0x35,0x35), 1));
        p.drawLine(QPointF(a.left(), sy), QPointF(a.right(), sy));
        p.setPen(QColor(0x70,0x70,0x70));
        p.drawText(QRectF(0, sy-8, a.left()-2, 16),
                   Qt::AlignRight | Qt::AlignVCenter, fmtNum(maxV * frac));
    }

    p.setRenderHint(QPainter::Antialiasing);
    p.setClipRect(a);
    double gapFrac = 0.15;
    double effW = barW * m_zoomNC * (1 - 2*gapFrac);
    double gapW = barW * m_zoomNC * gapFrac;

    for (int i = 0; i < n; i++) {
        double barH = (m_values[i] / maxV) * a.height() * m_zoomNC;
        double bx   = a.left() + i * barW * m_zoomNC + gapW + m_panNC.x();
        double by   = a.bottom() - barH + m_panNC.y();
        if (bx + effW < a.left() || bx > a.right()) continue;
        QRectF r(bx, by, effW, barH);
        p.fillRect(r, paletteColor(i));
        p.setPen(QPen(QColor(0x18,0x18,0x18), 1));
        p.drawRect(r);
        /* valore sopra la barra */
        p.setPen(QColor(0xcc,0xcc,0xcc));
        p.setFont(QFont("Inter,Ubuntu,sans-serif", 8));
        p.drawText(QRectF(bx-4, by-15, effW+8, 13), Qt::AlignCenter, fmtNum(m_values[i]));
        /* etichetta sotto */
        QString lbl = i < m_labels.size() ? m_labels[i] : QString::number(i+1);
        p.drawText(QRectF(bx, a.bottom()+2, effW, 16), Qt::AlignCenter, lbl);
    }
    p.setClipping(false);
    p.setPen(QPen(QColor(0x50,0x50,0x50), 1));
    p.drawRect(a);
}

/* ══════════════════════════════════════════════════════════════
   Scatter XY
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::drawScatterXY(QPainter& p, const QRectF& a) {
    drawGrid(p, a);
    p.setRenderHint(QPainter::Antialiasing);
    p.setClipRect(a);
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 8));
    for (int i = 0; i < m_scatterPts.size(); i++) {
        QPointF sp = dataToScreen(m_scatterPts[i].x(), m_scatterPts[i].y(), a);
        QColor  c  = paletteColor(i);
        p.setBrush(c);
        p.setPen(Qt::NoPen);
        p.drawEllipse(sp, 5.0, 5.0);
        /* etichetta: testo fornito dall'utente, altrimenti "P1", "P2"... */
        QString lbl = (i < m_labels.size() && !m_labels[i].isEmpty())
                      ? m_labels[i]
                      : QString("P%1").arg(i + 1);
        p.setPen(c.lighter(140));
        p.drawText(sp + QPointF(8, -2), lbl);
    }
    p.setClipping(false);
}

/* ══════════════════════════════════════════════════════════════
   Grafo
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::drawGraph(QPainter& p, const QRectF& a) {
    if (m_edges.isEmpty()) {
        p.setPen(QColor(0x77,0x77,0x77));
        p.drawText(a, Qt::AlignCenter, "Nessun arco\nFormato: A-B (uno per riga)");
        return;
    }
    QStringList nodes;
    for (auto& e : m_edges) {
        if (!nodes.contains(e.first))  nodes.append(e.first);
        if (!nodes.contains(e.second)) nodes.append(e.second);
    }
    int nn = nodes.size();
    double cx = a.center().x() + m_panNC.x();
    double cy = a.center().y() + m_panNC.y();
    double r  = std::min(a.width(), a.height()) * 0.36 * m_zoomNC;

    QVector<QPointF> pos(nn);
    for (int i = 0; i < nn; i++) {
        double angle = 2 * M_PI * i / nn - M_PI / 2;
        pos[i] = { cx + r * std::cos(angle), cy + r * std::sin(angle) };
    }

    p.setRenderHint(QPainter::Antialiasing);
    /* archi */
    p.setPen(QPen(QColor(0x60,0x60,0x60), 1.5));
    for (auto& e : m_edges) {
        int ai = nodes.indexOf(e.first), bi = nodes.indexOf(e.second);
        if (ai < 0 || bi < 0) continue;
        p.drawLine(pos[ai], pos[bi]);
    }
    /* nodi */
    double nr = std::max(12.0, std::min(22.0, r * 0.18));
    for (int i = 0; i < nn; i++) {
        p.setBrush(QColor(0x1e,0x1e,0x1e));
        p.setPen(QPen(paletteColor(i), 2));
        p.drawEllipse(pos[i], nr, nr);
        p.setPen(paletteColor(i));
        p.setFont(QFont("Inter,Ubuntu,sans-serif", 9, QFont::Bold));
        p.drawText(QRectF(pos[i].x()-nr, pos[i].y()-nr, 2*nr, 2*nr),
                   Qt::AlignCenter, nodes[i]);
    }
}
