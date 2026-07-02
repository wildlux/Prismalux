/*
 * main_graph_canvas_events.cpp — GraficoCanvas: eventi Qt + animazione/export
 * ============================================================================
 * paintEvent (dispatch ai draw*), input mouse/wheel, context menu, timer
 * animazione, export PNG. Split da main_graph_canvas.cpp (TODO D-8).
 */
#include "main_graph.h"
#include "main_graph_canvas_p.h"
#include "widgets/formula_parser.h"

#include <QPainter>
#include <QPixmap>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QDir>
#include <cmath>
#include <algorithm>

void GraficoCanvas::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), m_style.bgColor);
    QRectF a = plotArea(this);
    switch (m_type) {
        case Cartesian:  drawCartesian(p, a);  break;
        case Pie:        drawPie(p, a);        break;
        case Histogram:  drawHistogram(p, a);  break;
        case ScatterXY:  drawScatterXY(p, a);  break;
        case Graph:      drawGraph(p, a);      break;
        case Scatter3D:  drawScatter3D(p, a);  break;
        case Graph3D:    drawGraph3D(p, a);    break;
        case SmithPrime:  drawSmithPrime (p, a); break;
        case MathConst:   drawMathConst  (p, a); break;
        case Line:        drawLine       (p, a); break;
        case Polar:       drawPolar      (p, a); break;
        case Radar:       drawRadar      (p, a); break;
        case Bubble:      drawBubble     (p, a); break;
        case Heatmap:     drawHeatmap    (p, a); break;
        case Candlestick:  drawCandlestick (p, a); break;
        case Area:         drawArea        (p, a); break;
        case Waterfall:    drawWaterfall   (p, a); break;
        case Step:         drawStep        (p, a); break;
        case Column:       drawColumn      (p, a); break;
        case HBar:         drawHBar        (p, a); break;
        case GroupedBar:   drawGroupedBar  (p, a); break;
        case StackedBar:   drawStackedBar  (p, a); break;
        case StackedBar100:drawStackedBar100(p, a); break;
        case Funnel:       drawFunnel      (p, a); break;
        case Donut:        drawDonut       (p, a); break;
        case Treemap:      drawTreemap     (p, a); break;
        case Sunburst:     drawSunburst    (p, a); break;
        case BoxPlot:      drawBoxPlot     (p, a); break;
        case DotPlot:      drawDotPlot     (p, a); break;
        case Density:      drawDensity     (p, a); break;
        case AreaStacked:  drawAreaStacked (p, a); break;
        case OHLC:         drawOHLC        (p, a); break;
        case Gauge:        drawGauge       (p, a); break;
        case Bullet:       drawBullet      (p, a); break;
        case Gantt:        drawGantt       (p, a); break;
        case Pyramid:      drawPyramid     (p, a); break;
        case ParallelCoord:drawParallelCoord(p, a); break;
        case Sankey:       drawSankey      (p, a); break;
        case Tree:         drawTree        (p, a); break;
        case Chord:         drawChord        (p, a); break;
        case Violin:        drawViolin       (p, a); break;
        case WordCloud:     drawWordCloud    (p, a); break;
        case RadialTree:    drawRadialTree   (p, a); break;
        case AnimatedLine:  drawAnimatedLine (p, a); break;
        case SmallMultiples:drawSmallMultiples(p,a); break;
    }
}

/* ══════════════════════════════════════════════════════════════
   Input mouse
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::wheelEvent(QWheelEvent* e) {
    double dy = e->angleDelta().y();
    double factor = (dy > 0) ? (1.0/1.15) : 1.15;

    const bool isCartCoord = (m_type == Cartesian || m_type == ScatterXY ||
                               m_type == Line || m_type == Area || m_type == Step);
    if (isCartCoord) {
        QRectF a = plotArea(this);
        QPointF mp = screenToData(e->position(), a);
        double tX = (m_xVMax - m_xVMin == 0) ? 0.5
                   : (mp.x() - m_xVMin) / (m_xVMax - m_xVMin);
        double tY = (m_yVMax - m_yVMin == 0) ? 0.5
                   : (mp.y() - m_yVMin) / (m_yVMax - m_yVMin);
        double dX = (m_xVMax - m_xVMin) * factor;
        double dY = (m_yVMax - m_yVMin) * factor;
        m_xVMin = mp.x() - tX * dX; m_xVMax = m_xVMin + dX;
        m_yVMin = mp.y() - tY * dY; m_yVMax = m_yVMin + dY;
        if (m_type == Cartesian && !m_formula.isEmpty()) {
            FormulaParser fp(m_formula);
            if (fp.ok()) m_cartPts = fp.sample(m_xVMin, m_xVMax, 600);
        }
    } else if (m_type == Scatter3D || m_type == Graph3D) {
        /* 3D — zoom semplice al centro (nessun pan, la vista è una rotazione) */
        m_zoomNC *= factor;
        m_zoomNC  = std::max(0.001, std::min(m_zoomNC, 1000.0));
    } else {
        /* 2D non-cartesiano (Smith, Torta, Isto, Grafo, MathConst):
         * zoom verso il cursore — la stessa logica di Blender.
         *
         * Sistema di coordinate:
         *   cx0 = area.center().x() + m_panNC.x()   (centro del grafico a schermo)
         *   R0  = base * m_zoomNC                    (raggio / scala corrente)
         *
         * Il punto contenuto sotto il cursore (coord normalizzate Γ o equivalenti):
         *   qx = (mx - cx0) / R0
         *   qy = (cy0 - my) / R0   (y schermo invertito)
         *
         * Dopo lo zoom (R1 = R0 * factor) vogliamo qx/qy invariati,
         * quindi il nuovo centro deve soddisfare:
         *   cx1 = mx - qx * R1
         *   cy1 = my + qy * R1
         * e il nuovo pan:
         *   panNew = (cx1 - area.center().x(), cy1 - area.center().y())
         */
        QRectF  a    = plotArea(this);
        double  base = std::min(a.width(), a.height()) * 0.44;
        double  R0   = base * m_zoomNC;
        double  cx0  = a.center().x() + m_panNC.x();
        double  cy0  = a.center().y() + m_panNC.y();
        double  mx   = e->position().x();
        double  my   = e->position().y();

        double  qx   = (R0 > 1e-9) ? (mx - cx0) / R0 : 0.0;
        double  qy   = (R0 > 1e-9) ? (cy0 - my) / R0 : 0.0;

        m_zoomNC *= factor;
        m_zoomNC  = std::max(0.001, std::min(m_zoomNC, 1000.0));

        double  R1   = base * m_zoomNC;
        double  cx1  = mx - qx * R1;
        double  cy1  = my + qy * R1;
        m_panNC = QPointF(cx1 - a.center().x(), cy1 - a.center().y());
    }
    update();
    e->accept();
}

void GraficoCanvas::mousePressEvent(QMouseEvent* e) {
    const bool is3D = (m_type == Scatter3D || m_type == Graph3D);

    /* ── Navigazione 3D stile Blender ──────────────────────────────
     * Tasto centrale (MMB)         → orbita
     * Shift + tasto centrale (MMB) → pan
     * Alt + tasto sinistro (LMB)   → orbita (alternativa)
     * Shift + Alt + LMB            → pan
     * ─────────────────────────────────────────────────────────────── */
    if (is3D) {
        const bool isMid   = (e->button() == Qt::MiddleButton);
        const bool isAltLMB= (e->button() == Qt::LeftButton &&
                              (e->modifiers() & Qt::AltModifier));
        if (isMid || isAltLMB) {
            m_dragging  = true;
            m_dragStart = e->pos();
            m_rotYD     = m_rotY;
            m_rotXD     = m_rotX;
            m_panD      = m_panNC;
            m_zoomD     = m_zoomNC;

            const bool wantPan = (e->modifiers() & Qt::ShiftModifier);
            m_drag3DMode = wantPan ? Drag3DPan : Drag3DOrbit;
            setCursor(wantPan ? Qt::SizeAllCursor : Qt::ClosedHandCursor);
            e->accept();
            return;
        }
    }

    /* ── Comportamento esistente per LMB ───────────────────────── */
    if (e->button() == Qt::LeftButton) {
        m_dragging = true;
        m_drag3DMode = Drag3DNone;   /* usa percorso classico in mouseMoveEvent */
        m_dragStart = e->pos();
        m_xVMinD = m_xVMin; m_xVMaxD = m_xVMax;
        m_yVMinD = m_yVMin; m_yVMaxD = m_yVMax;
        m_zoomD  = m_zoomNC;
        m_panD   = m_panNC;
        m_rotYD  = m_rotY;
        m_rotXD  = m_rotX;
        setCursor(Qt::ClosedHandCursor);
    }
}

void GraficoCanvas::mouseMoveEvent(QMouseEvent* e) {
    if (!m_dragging) return;
    QPointF d  = e->pos() - m_dragStart;
    QRectF  a  = plotArea(this);

    /* ── Navigazione 3D stile Blender (MMB / Alt+LMB) ─────────── */
    if (m_drag3DMode == Drag3DOrbit) {
        m_rotY = m_rotYD + d.x() * 0.008;
        m_rotX = std::max(-M_PI/2 + 0.05,
                 std::min( M_PI/2 - 0.05, m_rotXD + d.y() * 0.008));
        update();
        return;
    }
    if (m_drag3DMode == Drag3DPan) {
        m_panNC = m_panD + d;
        update();
        return;
    }

    /* ── Percorso classico (LMB senza Alt) ─────────────────────── */
    const bool isCartCoord2 = (m_type == Cartesian || m_type == ScatterXY ||
                                m_type == Line || m_type == Area || m_type == Step);
    if (isCartCoord2) {
        double ddx = d.x() / (a.width()  > 0 ? a.width()  : 1) * (m_xVMaxD - m_xVMinD);
        double ddy = d.y() / (a.height() > 0 ? a.height() : 1) * (m_yVMaxD - m_yVMinD);
        m_xVMin = m_xVMinD - ddx; m_xVMax = m_xVMaxD - ddx;
        m_yVMin = m_yVMinD + ddy; m_yVMax = m_yVMaxD + ddy;
        if (m_type == Cartesian && !m_formula.isEmpty()) {
            FormulaParser fp(m_formula);
            if (fp.ok()) m_cartPts = fp.sample(m_xVMin, m_xVMax, 600);
        }
    } else if (m_type == Scatter3D || m_type == Graph3D) {
        /* LMB classico su 3D = orbita (comportamento precedente) */
        m_rotY = m_rotYD + d.x() * 0.008;
        m_rotX = std::max(-M_PI/2 + 0.05,
                 std::min( M_PI/2 - 0.05, m_rotXD + d.y() * 0.008));
    } else {
        m_panNC = m_panD + d;
    }
    update();
}

void GraficoCanvas::mouseReleaseEvent(QMouseEvent* e) {
    const bool isReleasedBtn =
        (e->button() == Qt::LeftButton || e->button() == Qt::MiddleButton);
    if (isReleasedBtn && m_dragging) {
        m_dragging   = false;
        m_drag3DMode = Drag3DNone;
        setCursor(Qt::ArrowCursor);
    }
}

void GraficoCanvas::contextMenuEvent(QContextMenuEvent* e) {
    auto* menu = new QMenu(this);
    auto* actSave = menu->addAction("\xf0\x9f\x96\xbc  Salva come PNG...");
    connect(actSave, &QAction::triggered, this, &GraficoCanvas::onContextSavePng);
    menu->addSeparator();
    auto* actReset = menu->addAction("\xf0\x9f\x94\x84  Reset vista");
    connect(actReset, &QAction::triggered, this, &GraficoCanvas::resetView);
    menu->exec(e->globalPos());
    menu->deleteLater();
}

void GraficoCanvas::onAnimTimerTick()
{
    int maxP = 0;
    for (auto& s : m_lineSeries) maxP = std::max(maxP, (int)s.size());
    if (maxP > 0) {
        ++m_animFrame;
        if (m_animFrame >= maxP + 8) m_animFrame = 0;
    }
    update();
}

void GraficoCanvas::onContextSavePng()
{
    QString path = QFileDialog::getSaveFileName(
        this, "Salva grafico", QDir::homePath() + "/grafico.png", "PNG (*.png)");
    if (path.isEmpty()) return;
    QPixmap px(size());
    render(&px);
    px.save(path);
    emit statusMessage("\xe2\x9c\x85  PNG salvato: " + path);
}
