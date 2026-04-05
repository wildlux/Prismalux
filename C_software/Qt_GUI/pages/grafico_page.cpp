/*
 * grafico_page.cpp — Implementazione pagina Grafico
 * ==================================================
 * GraficoCanvas: rendering QPainter puro, zoom + pan con mouse.
 * GraficoPage  : pannello parametri (sx) + GraficoCanvas (dx).
 */
#include "pages/grafico_page.h"
#include "widgets/formula_parser.h"
#include "ai_client.h"

#include <QPainter>
#include <QPainterPath>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFrame>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QFont>
#include <QMessageBox>
#include <cmath>
#include <algorithm>

/* ── Palette colori ──────────────────────────────────────────── */
static const QColor kPal[] = {
    {0x00,0xbf,0xd8}, {0xff,0x79,0x5a}, {0x73,0xe2,0x73},
    {0xff,0xd7,0x6e}, {0xb0,0x90,0xff}, {0xff,0x82,0xc2},
    {0x5a,0xd7,0xff}, {0xff,0xa0,0x50}
};
QColor GraficoCanvas::paletteColor(int i) { return kPal[i % 8]; }

/* ── Numero formattato ───────────────────────────────────────── */
QString GraficoCanvas::fmtNum(double v) {
    if (!std::isfinite(v)) return "?";
    if (std::abs(v) >= 1e4 || (std::abs(v) > 0 && std::abs(v) < 1e-3))
        return QString::number(v, 'g', 4);
    QString s = QString::number(v, 'f', 4);
    while (s.endsWith('0')) s.chop(1);
    if (s.endsWith('.'))    s.chop(1);
    return s;
}

/* ── Passo griglia "carino" ──────────────────────────────────── */
double GraficoCanvas::niceStep(double range, int ticks) {
    double r = range / ticks;
    double m = std::pow(10.0, std::floor(std::log10(r)));
    double n = r / m;
    if (n < 1.5) return 1 * m;
    if (n < 3.5) return 2 * m;
    if (n < 7.5) return 5 * m;
    return 10 * m;
}

/* ── Area di disegno (margini fissi) ─────────────────────────── */
static QRectF plotArea(const QWidget* w) {
    return QRectF(60, 18, w->width() - 80, w->height() - 52);
}

/* ── Origine gizmo assi in base alla posizione scelta ─────────── */
static QPointF gizmoOrigin(int pos, const QRectF& a) {
    constexpr double M = 52;  /* margine dal bordo */
    switch (pos) {
        case GraficoCanvas::BottomLeft:  return { a.left()  + M, a.bottom() - M };
        case GraficoCanvas::TopLeft:     return { a.left()  + M, a.top()    + M };
        case GraficoCanvas::TopRight:    return { a.right() - M, a.top()    + M };
        default: /* BottomRight */       return { a.right() - M, a.bottom() - M };
    }
}

/* ══════════════════════════════════════════════════════════════
   Costruttore
   ══════════════════════════════════════════════════════════════ */
GraficoCanvas::GraficoCanvas(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent);
}

/* ══════════════════════════════════════════════════════════════
   Setter dati
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::setCartesian(const QString& formula, double xMin, double xMax) {
    m_formula = formula;
    m_xVMin = xMin; m_xVMax = xMax;
    FormulaParser fp(formula);
    if (fp.ok()) {
        m_cartPts = fp.sample(xMin, xMax, 600);
        if (!m_cartPts.isEmpty()) {
            double yMn =  1e18, yMx = -1e18;
            for (auto& pt : m_cartPts) {
                if (!std::isfinite(pt.y())) continue;
                if (pt.y() < yMn) yMn = pt.y();
                if (pt.y() > yMx) yMx = pt.y();
            }
            if (yMn <= yMx) {
                double pad = (yMx - yMn) * 0.12 + 0.5;
                m_yVMin = yMn - pad;
                m_yVMax = yMx + pad;
            }
        }
    } else {
        m_cartPts.clear();
    }
    m_type = Cartesian;
    update();
}

void GraficoCanvas::setData(const QVector<double>& v, const QStringList& l) {
    m_values = v; m_labels = l;
    m_zoomNC = 1.0; m_panNC = {};
    update();
}

void GraficoCanvas::setScatter(const QVector<QPointF>& pts) {
    m_scatterPts = pts;
    if (!pts.isEmpty()) {
        double xMn = pts[0].x(), xMx = xMn, yMn = pts[0].y(), yMx = yMn;
        for (auto& p : pts) {
            if (p.x() < xMn) xMn = p.x(); if (p.x() > xMx) xMx = p.x();
            if (p.y() < yMn) yMn = p.y(); if (p.y() > yMx) yMx = p.y();
        }
        double px = (xMx - xMn) * 0.12 + 0.5;
        double py = (yMx - yMn) * 0.12 + 0.5;
        m_xVMin = xMn - px; m_xVMax = xMx + px;
        m_yVMin = yMn - py; m_yVMax = yMx + py;
    }
    m_type = ScatterXY;
    update();
}

void GraficoCanvas::setEdges(const QVector<QPair<QString,QString>>& e) {
    m_edges = e; m_zoomNC = 1.0; m_panNC = {};
    m_type = Graph; update();
}

void GraficoCanvas::setScatter3D(const QVector<Pt3D>& pts) {
    m_pts3d = pts;
    m_rotY = 0.65; m_rotX = 0.35;
    m_zoomNC = 1.0; m_panNC = {};
    m_type = Scatter3D; update();
}

void GraficoCanvas::setGraph3D(const QVector<Node3D>& nodes,
                                const QVector<QPair<QString,QString>>& edges) {
    m_nodes3d = nodes;
    m_edges3d = edges;
    m_rotY = 0.65; m_rotX = 0.35;
    m_zoomNC = 1.0; m_panNC = {};
    m_type = Graph3D; update();
}

void GraficoCanvas::setType(ChartType t) {
    if (m_type == AnimatedLine && t != AnimatedLine && m_animTimer)
        m_animTimer->stop();
    m_type = t;
    if (t == AnimatedLine) {
        m_animFrame = 0;
        if (!m_animTimer) {
            m_animTimer = new QTimer(this);
            connect(m_animTimer, &QTimer::timeout, this, [this]{
                int maxP = 0;
                for (auto& s : m_lineSeries) maxP = std::max(maxP, (int)s.size());
                if (maxP > 0) {
                    ++m_animFrame;
                    if (m_animFrame >= maxP + 8) m_animFrame = 0;
                }
                update();
            });
        }
        m_animTimer->start(50);
    }
    update();
}

void GraficoCanvas::setAxes2dPos(int pos) { m_axes2dPos = pos; repaint(); }
void GraficoCanvas::setAxes3dPos(int pos) { m_axes3dPos = pos; repaint(); }

void GraficoCanvas::resetView() {
    m_zoomNC = 1.0; m_panNC = {};
    m_rotY = 0.65;  m_rotX = 0.35;
    if (m_type == Cartesian && !m_formula.isEmpty())
        setCartesian(m_formula, m_xVMin, m_xVMax);
    else
        update();
}

/* ── Trasformazioni data ↔ screen ───────────────────────────── */
QPointF GraficoCanvas::dataToScreen(double dx, double dy, const QRectF& a) const {
    double rX = m_xVMax - m_xVMin; if (rX == 0) rX = 1;
    double rY = m_yVMax - m_yVMin; if (rY == 0) rY = 1;
    return { a.left()   + (dx - m_xVMin) / rX * a.width(),
             a.bottom() - (dy - m_yVMin) / rY * a.height() };
}

QPointF GraficoCanvas::screenToData(const QPointF& sp, const QRectF& a) const {
    double rX = m_xVMax - m_xVMin; if (rX == 0) rX = 1;
    double rY = m_yVMax - m_yVMin; if (rY == 0) rY = 1;
    return { m_xVMin + (sp.x() - a.left())    / a.width()  * rX,
             m_yVMin + (a.bottom() - sp.y())   / a.height() * rY };
}

/* ══════════════════════════════════════════════════════════════
   Griglia assi (usata da Cartesian + ScatterXY)
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::drawGrid(QPainter& p, const QRectF& a) {
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 8));

    double xStep = niceStep(m_xVMax - m_xVMin, 8);
    double yStep = niceStep(m_yVMax - m_yVMin, 6);
    double xFirst = std::ceil(m_xVMin / xStep) * xStep;
    double yFirst = std::ceil(m_yVMin / yStep) * yStep;

    /* linee verticali + etichette X */
    for (double x = xFirst; x <= m_xVMax + xStep * 0.01; x += xStep) {
        double sx = a.left() + (x - m_xVMin) / (m_xVMax - m_xVMin) * a.width();
        p.setPen(QPen(QColor(0x35,0x35,0x35), 1));
        p.drawLine(QPointF(sx, a.top()), QPointF(sx, a.bottom()));
        p.setPen(QColor(0x77,0x77,0x77));
        p.drawText(QRectF(sx - 26, a.bottom() + 3, 52, 14), Qt::AlignCenter, fmtNum(x));
    }

    /* linee orizzontali + etichette Y */
    for (double y = yFirst; y <= m_yVMax + yStep * 0.01; y += yStep) {
        double sy = a.bottom() - (y - m_yVMin) / (m_yVMax - m_yVMin) * a.height();
        p.setPen(QPen(QColor(0x35,0x35,0x35), 1));
        p.drawLine(QPointF(a.left(), sy), QPointF(a.right(), sy));
        p.setPen(QColor(0x77,0x77,0x77));
        p.drawText(QRectF(0, sy - 8, a.left() - 4, 16),
                   Qt::AlignRight | Qt::AlignVCenter, fmtNum(y));
    }

    if (m_axes2dPos == AtData) {
        /* assi X=0 e Y=0 attraverso tutto il plot */
        p.setPen(QPen(QColor(0x55,0x55,0x55), 1));
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
        p.setFont(QFont("Inter,Ubuntu,sans-serif", 8, QFont::Bold));
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
    p.setPen(QPen(QColor(0x50,0x50,0x50), 1));
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
    /* etichetta formula */
    p.setPen(QColor(0x00,0xbf,0xd8,170));
    p.setFont(QFont("JetBrains Mono,Fira Code,Consolas,monospace", 10));
    p.drawText(QRectF(a.left()+6, a.top()+4, a.width()-8, 18),
               Qt::AlignLeft, "y = " + m_formula);
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

/* ══════════════════════════════════════════════════════════════
   Scatter 3D
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::drawScatter3D(QPainter& p, const QRectF& a) {
    if (m_pts3d.isEmpty()) {
        p.setPen(QColor(0x77,0x77,0x77));
        p.drawText(a, Qt::AlignCenter, "Nessun dato\nFormato: x,y,z (uno per riga)");
        return;
    }
    /* normalizzazione in [-0.5, 0.5] */
    double xMn = m_pts3d[0].x, xMx = xMn;
    double yMn = m_pts3d[0].y, yMx = yMn;
    double zMn = m_pts3d[0].z, zMx = zMn;
    for (auto& pt : m_pts3d) {
        if (pt.x < xMn) xMn = pt.x; if (pt.x > xMx) xMx = pt.x;
        if (pt.y < yMn) yMn = pt.y; if (pt.y > yMx) yMx = pt.y;
        if (pt.z < zMn) zMn = pt.z; if (pt.z > zMx) zMx = pt.z;
    }
    double sX = (xMx > xMn) ? 1.0/(xMx-xMn) : 1;
    double sY = (yMx > yMn) ? 1.0/(yMx-yMn) : 1;
    double sZ = (zMx > zMn) ? 1.0/(zMx-zMn) : 1;

    double cosY = std::cos(m_rotY), sinY = std::sin(m_rotY);
    double cosX = std::cos(m_rotX), sinX = std::sin(m_rotX);
    double ccx = a.center().x() + m_panNC.x();
    double ccy = a.center().y() + m_panNC.y();
    double sc  = std::min(a.width(), a.height()) * 0.40 * m_zoomNC;

    auto project = [&](double nx, double ny, double nz, double& outX, double& outY, double& outZ) {
        double rx =  nx * cosY - nz * sinY;
        double ry =  ny;
        double rz =  nx * sinY + nz * cosY;
        double fx =  rx;
        double fy =  ry * cosX - rz * sinX;
        double fz =  ry * sinX + rz * cosX;
        double d  = 3.0, w = d / (d + fz * 0.5 + 0.1);
        outX = ccx + fx * w * sc;
        outY = ccy - fy * w * sc;
        outZ = fz;
    };

    p.setRenderHint(QPainter::Antialiasing);
    /* gizmo assi — angolo configurabile, proiezione ortografica */
    {
        const QPointF _go = gizmoOrigin(m_axes3dPos, a);
        const double gx = _go.x();
        const double gy = _go.y();
        const double gs = 38.0;               /* lunghezza asse in pixel */
        const struct { double x,y,z; const char* l; } axG[] = {
            {1,0,0,"X"}, {0,1,0,"Y"}, {0,0,1,"Z"}
        };
        const QColor axC[] = { {0xff,0x60,0x60}, {0x60,0xff,0x60}, {0x60,0x80,0xff} };
        p.setFont(QFont("Inter,Ubuntu,sans-serif", 8, QFont::Bold));
        for (int ai = 0; ai < 3; ai++) {
            /* rotazione pura (no prospettiva, no scala dati) */
            double rx =  axG[ai].x * cosY - axG[ai].z * sinY;
            double ry =  axG[ai].y;
            double rz =  axG[ai].x * sinY + axG[ai].z * cosY;
            double fx =  rx;
            double fy =  ry * cosX - rz * sinX;
            double ex  = gx + fx * gs;
            double ey  = gy - fy * gs;
            p.setPen(QPen(axC[ai], 1.5));
            p.drawLine(QPointF(gx, gy), QPointF(ex, ey));
            p.setPen(axC[ai]);
            p.drawText(QPointF(ex + 3, ey + 3), axG[ai].l);
        }
    }
    /* punti ordinati per profondità (painter's algorithm) */
    struct PP { double sx, sy, depth; int idx; };
    QVector<PP> proj;
    proj.reserve(m_pts3d.size());
    for (int i = 0; i < m_pts3d.size(); i++) {
        double nx = (m_pts3d[i].x - xMn) * sX - 0.5;
        double ny = (m_pts3d[i].y - yMn) * sY - 0.5;
        double nz = (m_pts3d[i].z - zMn) * sZ - 0.5;
        double sx, sy, dz;
        project(nx, ny, nz, sx, sy, dz);
        proj.append({sx, sy, dz, i});
    }
    std::sort(proj.begin(), proj.end(), [](const PP& a, const PP& b){ return a.depth > b.depth; });
    /* pass 1: disegna sfere */
    for (auto& pp : proj) {
        QColor c = paletteColor(pp.idx);
        p.setBrush(c);
        p.setPen(QPen(c.darker(150), 1));
        p.drawEllipse(QPointF(pp.sx, pp.sy), 5, 5);
    }
    /* pass 2: etichette (sopra le sfere, ordine identico) */
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 8));
    for (auto& pp : proj) {
        QColor  c   = paletteColor(pp.idx);
        QString lbl = (!m_pts3d[pp.idx].label.isEmpty())
                      ? m_pts3d[pp.idx].label
                      : QString("P%1").arg(pp.idx + 1);
        p.setPen(c.lighter(140));
        p.drawText(QPointF(pp.sx + 8, pp.sy - 3), lbl);
    }
    /* suggerimento rotazione */
    p.setPen(QColor(0x55,0x55,0x55));
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 8));
    p.drawText(QRectF(a.left(), a.bottom()-16, a.width(), 14),
               Qt::AlignRight, "Trascina per ruotare  |  Rotella per zoom");
}

/* ══════════════════════════════════════════════════════════════
   Grafo 3D — nodi posizionati in 3D, archi come linee proiettate
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::drawGraph3D(QPainter& p, const QRectF& a) {
    if (m_nodes3d.isEmpty()) {
        p.setPen(QColor(0x77,0x77,0x77));
        p.drawText(a, Qt::AlignCenter,
                   "Nessun nodo\nFormato:\n  A, 0, 0, 0\n  B, 1, 0, 0\n  A-B");
        return;
    }
    /* normalizzazione in [-0.5, 0.5] */
    double xMn = m_nodes3d[0].x, xMx = xMn;
    double yMn = m_nodes3d[0].y, yMx = yMn;
    double zMn = m_nodes3d[0].z, zMx = zMn;
    for (auto& n : m_nodes3d) {
        if (n.x < xMn) xMn = n.x; if (n.x > xMx) xMx = n.x;
        if (n.y < yMn) yMn = n.y; if (n.y > yMx) yMx = n.y;
        if (n.z < zMn) zMn = n.z; if (n.z > zMx) zMx = n.z;
    }
    double sX = (xMx > xMn) ? 1.0/(xMx-xMn) : 1;
    double sY = (yMx > yMn) ? 1.0/(yMx-yMn) : 1;
    double sZ = (zMx > zMn) ? 1.0/(zMx-zMn) : 1;

    double cosY = std::cos(m_rotY), sinY = std::sin(m_rotY);
    double cosX = std::cos(m_rotX), sinX = std::sin(m_rotX);
    double ccx = a.center().x() + m_panNC.x();
    double ccy = a.center().y() + m_panNC.y();
    double sc  = std::min(a.width(), a.height()) * 0.40 * m_zoomNC;

    auto project = [&](double nx, double ny, double nz,
                       double& outX, double& outY, double& outZ) {
        double rx = nx * cosY - nz * sinY, ry = ny, rz = nx * sinY + nz * cosY;
        double fx = rx, fy = ry * cosX - rz * sinX, fz = ry * sinX + rz * cosX;
        double w = 3.0 / (3.0 + fz * 0.5 + 0.1);
        outX = ccx + fx * w * sc;
        outY = ccy - fy * w * sc;
        outZ = fz;
    };

    /* proietta tutti i nodi */
    int nn = m_nodes3d.size();
    struct PN { double sx, sy, depth; int idx; };
    QVector<PN> proj(nn);
    for (int i = 0; i < nn; i++) {
        double nx = (m_nodes3d[i].x - xMn) * sX - 0.5;
        double ny = (m_nodes3d[i].y - yMn) * sY - 0.5;
        double nz = (m_nodes3d[i].z - zMn) * sZ - 0.5;
        project(nx, ny, nz, proj[i].sx, proj[i].sy, proj[i].depth);
        proj[i].idx = i;
    }

    /* lookup nome → indice */
    auto findNode = [&](const QString& name) -> int {
        for (int i = 0; i < nn; i++) if (m_nodes3d[i].name == name) return i;
        return -1;
    };

    p.setRenderHint(QPainter::Antialiasing);

    /* gizmo assi — angolo configurabile, proiezione ortografica */
    {
        const double gx = a.right()  - 52;
        const double gy = a.bottom() - 52;
        const double gs = 38.0;
        const struct { double x,y,z; const char* l; } axG[] = {
            {1,0,0,"X"}, {0,1,0,"Y"}, {0,0,1,"Z"}
        };
        const QColor axC[] = { {0xff,0x55,0x55}, {0x55,0xff,0x55}, {0x55,0x88,0xff} };
        p.setFont(QFont("Inter,Ubuntu,sans-serif", 8, QFont::Bold));
        for (int ai = 0; ai < 3; ai++) {
            double rx =  axG[ai].x * cosY - axG[ai].z * sinY;
            double ry =  axG[ai].y;
            double rz =  axG[ai].x * sinY + axG[ai].z * cosY;
            double fx  = rx;
            double fy  = ry * cosX - rz * sinX;
            double ex  = gx + fx * gs;
            double ey  = gy - fy * gs;
            p.setPen(QPen(axC[ai], 1.5));
            p.drawLine(QPointF(gx, gy), QPointF(ex, ey));
            p.setPen(axC[ai]);
            p.drawText(QPointF(ex + 3, ey + 3), axG[ai].l);
        }
    }

    /* archi (disegno prima, sotto i nodi) */
    for (auto& e : m_edges3d) {
        int ai = findNode(e.first), bi = findNode(e.second);
        if (ai < 0 || bi < 0) continue;
        /* colore interpolato tra i due nodi, proporzionale alla profondità */
        double depthMid = (proj[ai].depth + proj[bi].depth) * 0.5;
        int alpha = qBound(80, 180 - (int)(depthMid * 40), 220);
        p.setPen(QPen(QColor(0x88,0x88,0x88, alpha), 1.8));
        p.drawLine(QPointF(proj[ai].sx, proj[ai].sy),
                   QPointF(proj[bi].sx, proj[bi].sy));
    }

    /* nodi ordinati back-to-front (painter's algorithm) */
    QVector<PN> sorted = proj;
    std::sort(sorted.begin(), sorted.end(),
              [](const PN& a, const PN& b){ return a.depth > b.depth; });

    double nr = std::max(10.0, std::min(20.0, sc * 0.09));
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 9, QFont::Bold));
    for (auto& pn : sorted) {
        QColor c = paletteColor(pn.idx);
        /* cerchio con bordo colorato */
        p.setBrush(QColor(0x1a,0x1a,0x2a));
        p.setPen(QPen(c, 2));
        p.drawEllipse(QPointF(pn.sx, pn.sy), nr, nr);
        /* nome del nodo dentro il cerchio */
        p.setPen(c);
        p.drawText(QRectF(pn.sx - nr, pn.sy - nr, 2*nr, 2*nr),
                   Qt::AlignCenter, m_nodes3d[pn.idx].name);
    }

    /* coordinate (x,y,z) come tooltip sotto al nodo */
    p.setFont(QFont("JetBrains Mono,Fira Code,Consolas", 7));
    for (auto& pn : sorted) {
        const Node3D& nd = m_nodes3d[pn.idx];
        QString coords = QString("(%1,%2,%3)")
                         .arg(fmtNum(nd.x)).arg(fmtNum(nd.y)).arg(fmtNum(nd.z));
        p.setPen(QColor(0x66,0x66,0x66));
        p.drawText(QPointF(pn.sx - 30, pn.sy + nr + 10), coords);
    }

    /* suggerimento */
    p.setPen(QColor(0x55,0x55,0x55));
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 8));
    p.drawText(QRectF(a.left(), a.bottom()-16, a.width(), 14),
               Qt::AlignRight, "Trascina per ruotare  |  Rotella per zoom");
}

/* ══════════════════════════════════════════════════════════════
   Smith Prime — generazione punti (crivello + trasformata di Möbius)
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::setSmithPrime(int maxN, bool showReal, bool showGauss,
                                   bool expanded, double normScale,
                                   bool showFib, bool showTri, bool showSqr,
                                   bool showLabels) {
    m_smithPts.clear();
    m_smithExpanded   = expanded;
    m_smithShowLabels = showLabels;
    if (maxN < 2)     maxN = 2;
    if (maxN > 50000) maxN = 50000;
    if (normScale <= 0.0) normScale = 1.0;
    m_smithNormScale  = normScale;   /* memorizzato per addSmithCustom() */

    /* ── Crivello di Eratostene fino a maxN ── */
    QVector<bool> sieve(maxN + 1, true);
    sieve[0] = sieve[1] = false;
    for (int i = 2; (long long)i * i <= maxN; ++i)
        if (sieve[i])
            for (int j = i * i; j <= maxN; j += i)
                sieve[j] = false;

    /* ── Trasformata di Möbius: Γ = (z−1)/(z+1) ── */
    auto mob = [](double zRe, double zIm, double& gRe, double& gIm) {
        double denom = (zRe + 1.0) * (zRe + 1.0) + zIm * zIm;
        if (denom < 1e-15) { gRe = 1.0; gIm = 0.0; return; }
        gRe = ((zRe - 1.0) * (zRe + 1.0) + zIm * zIm) / denom;
        gIm =  2.0 * zIm / denom;
    };

    /* Lambda: mappa l'intero N > 1 via Γ(log2(N)*normScale) */
    auto mapInt = [&](int N, double& gRe, double& gIm) {
        double z = std::log2(static_cast<double>(N)) * normScale;
        mob(z, 0.0, gRe, gIm);
    };

    /* ── Primi reali — sull'asse reale (seriesId=0) ── */
    if (showReal) {
        for (int p = 2; p <= maxN; ++p) {
            if (!sieve[p]) continue;
            double gRe, gIm;
            mapInt(p, gRe, gIm);
            m_smithPts.append({gRe, 0.0, false, p, 0, p, 0});
        }
    }

    /* ── Fibonacci (seriesId=1): 1,1,2,3,5,8,13,21,… ── */
    if (showFib) {
        long long fa = 1, fb = 2;
        while (fa <= maxN) {
            if (fa >= 2) {
                double gRe, gIm;
                mapInt(static_cast<int>(fa), gRe, gIm);
                m_smithPts.append({gRe, 0.0, false, static_cast<int>(fa), 0,
                                   static_cast<int>(fa), 1});
            }
            long long fc = fa + fb; fa = fb; fb = fc;
        }
    }

    /* ── Triangolari (seriesId=2): T_n = n*(n+1)/2 ── */
    if (showTri) {
        for (int n = 2; ; ++n) {
            int t = n * (n + 1) / 2;
            if (t > maxN) break;
            double gRe, gIm;
            mapInt(t, gRe, gIm);
            m_smithPts.append({gRe, 0.0, false, t, 0, t, 2});
        }
    }

    /* ── Quadrati perfetti (seriesId=3): 4,9,16,25,… ── */
    if (showSqr) {
        for (int n = 2; ; ++n) {
            int sq = n * n;
            if (sq > maxN) break;
            double gRe, gIm;
            mapInt(sq, gRe, gIm);
            m_smithPts.append({gRe, 0.0, false, sq, 0, sq, 3});
        }
    }

    /* ── Primi gaussiani — quadrante a ≥ 0, b ≥ 0 (seriesId=4) ──
     *
     * Un intero gaussiano α = a + b·i è primo di Gauss se:
     *   - b = 0: |a| primo reale ≡ 3 (mod 4)   [NON mostrati qui per non sovrapporsi]
     *   - a = 0: |b| primo reale ≡ 3 (mod 4)   [mappati sul bordo |Γ|=1]
     *   - a,b > 0: a²+b² primo reale            [mappati all'interno del disco]
     */
    if (showGauss) {
        int sq = static_cast<int>(std::sqrt(static_cast<double>(maxN))) + 1;
        for (int a = 0; a <= sq; ++a) {
            for (int b = 1; b <= sq; ++b) {
                bool gp = false;
                if (a == 0) {
                    gp = (b <= maxN) && sieve[b] && (b % 4 == 3);
                } else {
                    long long n2 = (long long)a * a + (long long)b * b;
                    gp = (n2 <= (long long)maxN) && sieve[static_cast<int>(n2)];
                }
                if (!gp) continue;

                double zRe = static_cast<double>(a) * normScale;
                double zIm = static_cast<double>(b) * normScale;
                double gRe, gIm;
                mob(zRe, zIm, gRe, gIm);
                int norm2 = a * a + b * b;
                m_smithPts.append({gRe, gIm, true, a, b, norm2, 4});

                double gRec, gImc;
                mob(zRe, -zIm, gRec, gImc);
                m_smithPts.append({gRec, gImc, true, a, -b, norm2, 4});
            }
        }
    }

    m_zoomNC = 1.0;
    m_panNC  = {};
    m_type   = SmithPrime;
    update();
}

/* ══════════════════════════════════════════════════════════════
   Smith Prime — serie personalizzata (seriesId = 5)
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::addSmithCustom(const QVector<int>& vals) {
    if (vals.isEmpty()) return;

    auto mob = [](double zRe, double zIm, double& gRe, double& gIm) {
        double denom = (zRe + 1.0) * (zRe + 1.0) + zIm * zIm;
        if (denom < 1e-15) { gRe = 1.0; gIm = 0.0; return; }
        gRe = ((zRe - 1.0) * (zRe + 1.0) + zIm * zIm) / denom;
        gIm =  2.0 * zIm / denom;
    };

    for (int v : vals) {
        if (v < 2) continue;
        double z = std::log2(static_cast<double>(v)) * m_smithNormScale;
        double gRe, gIm;
        mob(z, 0.0, gRe, gIm);
        m_smithPts.append({gRe, 0.0, false, v, 0, v, 5});
    }
    m_type = SmithPrime;   /* assicura il rendering corretto anche senza setSmithPrime */
    update();
}

/* ══════════════════════════════════════════════════════════════
   Linea multi-serie — setter
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::setLine(const QVector<QVector<QPointF>>& series,
                             const QStringList& names) {
    m_lineSeries = series;
    m_lineNames  = names;
    /* calcola bounding box da tutti i punti */
    double xMn=1e18, xMx=-1e18, yMn=1e18, yMx=-1e18;
    for (auto& s : series)
        for (auto& pt : s) {
            xMn = std::min(xMn, pt.x()); xMx = std::max(xMx, pt.x());
            yMn = std::min(yMn, pt.y()); yMx = std::max(yMx, pt.y());
        }
    if (m_lineSeries.isEmpty() || xMn >= xMx) { xMn = 0; xMx = 1; }
    if (yMn >= yMx) { yMn -= 1; yMx += 1; }
    double px = (xMx - xMn) * 0.05, py = (yMx - yMn) * 0.08;
    m_xVMin = xMn - px; m_xVMax = xMx + px;
    m_yVMin = yMn - py; m_yVMax = yMx + py;
    m_type = Line; update();
}

/* ══════════════════════════════════════════════════════════════
   Polare — setter
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::setPolar(const QString& formula, double tMin, double tMax) {
    m_formula     = formula;
    m_polarTMin   = tMin;
    m_polarTMax   = tMax;
    m_zoomNC = 1.0; m_panNC = {};
    m_type = Polar; update();
}

/* ══════════════════════════════════════════════════════════════
   Heatmap — setter
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::setHeatmap(const QVector<QVector<double>>& data,
                                const QStringList& rowLbls,
                                const QStringList& colLbls) {
    m_heatData    = data;
    m_heatRowLbls = rowLbls;
    m_heatColLbls = colLbls;
    m_type = Heatmap; update();
}

/* ══════════════════════════════════════════════════════════════
   Candlestick OHLC — setter
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::setCandlestick(const QVector<OhlcPt>& pts) {
    m_ohlcPts = pts;
    m_type = Candlestick; update();
}

/* ══════════════════════════════════════════════════════════════
   Math Const — setter (π · e · Primi)
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::setMathConst(int nTerms, bool showPi, bool showE, bool showPrimes) {
    m_mathPts.clear();
    if (nTerms < 2)   nTerms = 2;
    if (nTerms > 300) nTerms = 300;

    /* Γ = (z−1)/(z+1) per z reale positivo */
    auto gammaR = [](double z) -> double {
        if (z + 1.0 < 1e-15) return 1.0;
        return (z - 1.0) / (z + 1.0);
    };

    /* ── Serie di Leibniz per π: 4 · Σ_{k=0}^{n−1} (−1)^k / (2k+1) ── */
    if (showPi) {
        double sum = 0.0;
        for (int k = 0; k < nTerms; ++k) {
            sum += (k % 2 == 0 ? 1.0 : -1.0) / (2.0 * k + 1.0);
            m_mathPts.append({gammaR(4.0 * sum), 0.0, 0, k});
        }
    }

    /* ── Serie di Taylor per e: Σ_{k=0}^{n−1} 1/k! ── */
    if (showE) {
        double sum = 0.0, fact = 1.0;
        for (int k = 0; k < nTerms; ++k) {
            if (k > 0) fact *= static_cast<double>(k);
            sum += 1.0 / fact;
            m_mathPts.append({gammaR(sum), 0.0, 1, k});
        }
    }

    /* ── Primi reali sulla scala log₂ (come SmithPrime, normScale=1) ── */
    if (showPrimes) {
        int maxP = nTerms * 8 + 20;
        if (maxP > 10000) maxP = 10000;
        QVector<bool> sieve(maxP + 1, true);
        sieve[0] = sieve[1] = false;
        for (int i = 2; (long long)i * i <= maxP; ++i)
            if (sieve[i]) for (int j = i * i; j <= maxP; j += i) sieve[j] = false;
        int cnt = 0;
        for (int p = 2; p <= maxP && cnt < nTerms; ++p) {
            if (!sieve[p]) continue;
            double z = std::log2(static_cast<double>(p));
            m_mathPts.append({gammaR(z), 0.0, 2, p});
            ++cnt;
        }
    }

    m_zoomNC = 1.0;
    m_panNC  = {};
    m_type   = MathConst;
    update();
}

/* ══════════════════════════════════════════════════════════════
   Math Const — rendering
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::drawMathConst(QPainter& p, const QRectF& area) {
    p.setRenderHint(QPainter::Antialiasing);

    const double cx = area.center().x() + m_panNC.x();
    const double cy = area.center().y() + m_panNC.y();
    const double R  = std::min(area.width(), area.height()) * 0.44 * m_zoomNC;

    auto toSc = [&](double re, double im) -> QPointF {
        return QPointF(cx + re * R, cy - im * R);
    };

    /* ── Sfondo disco ── */
    p.setBrush(QColor(0x0d, 0x1a, 0x26));
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPointF(cx, cy), R, R);

    /* ── Griglia Smith (clip al disco) ── */
    {
        QPainterPath clip;
        clip.addEllipse(QPointF(cx, cy), R + 1.5, R + 1.5);
        p.setClipPath(clip);

        QPen gridPen(QColor(0x1a, 0x42, 0x58, 160), 0.7);
        p.setPen(gridPen);
        p.setBrush(Qt::NoBrush);

        static const double kR[] = {0.2, 0.5, 1.0, 2.0, 5.0};
        for (double r : kR) {
            double cn = r / (r + 1.0), rn = 1.0 / (r + 1.0);
            p.drawEllipse(QPointF(cx + cn * R, cy), rn * R, rn * R);
        }
        static const double kX[] = {0.5, 1.0, 2.0, 5.0};
        for (double x : kX) {
            for (int sg : {1, -1}) {
                double xs = x * sg, cyG = 1.0 / xs, rxG = std::abs(cyG);
                p.drawEllipse(QPointF(cx + R, cy - cyG * R), rxG * R, rxG * R);
            }
        }
        p.setPen(QPen(QColor(0x22, 0x66, 0x55, 160), 0.7));
        p.drawLine(toSc(-1.0, 0.0), toSc(1.0, 0.0));
        p.setClipping(false);
    }

    /* ── Cerchio unitario ── */
    p.setPen(QPen(QColor(0x33, 0x99, 0xcc), 1.5));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QPointF(cx, cy), R, R);

    /* ── Titolo ── */
    p.setPen(QColor(0xbb, 0xdd, 0xff, 190));
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 9, QFont::Bold));
    p.drawText(QRectF(area.left(), area.top(), area.width(), 22),
               Qt::AlignHCenter | Qt::AlignVCenter,
               QString::fromUtf8("Diagramma di Smith \xe2\x80\x94 \xcf\x80 \xc2\xb7 e \xc2\xb7 Primi"));

    if (m_mathPts.isEmpty()) {
        p.setPen(QColor(0x66, 0x88, 0xaa));
        p.setFont(QFont("Inter,Ubuntu,sans-serif", 10));
        p.drawText(QRectF(cx - 140, cy - 14, 280, 28),
                   Qt::AlignCenter,
                   "Clicca \xe2\x80\x9cTraccia\xe2\x80\x9d per generare");
        return;
    }

    /* ── Linee verticali target Γ(π) e Γ(e) ── */
    const double kGammaPi = (M_PI - 1.0) / (M_PI + 1.0);   /* ≈ 0.5168 */
    const double kGammaE  = (M_E  - 1.0) / (M_E  + 1.0);   /* ≈ 0.4621 */

    p.setPen(QPen(QColor(0xff, 0xa5, 0x00, 90), 1.0, Qt::DashLine));
    p.drawLine(toSc(kGammaPi, -0.22), toSc(kGammaPi,  0.22));
    p.setPen(QPen(QColor(0x40, 0xe0, 0x70, 90), 1.0, Qt::DashLine));
    p.drawLine(toSc(kGammaE,  -0.22), toSc(kGammaE,   0.22));

    /* Etichette target */
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 8, QFont::Bold));
    p.setPen(QColor(0xff, 0xa5, 0x00, 200));
    p.drawText(toSc(kGammaPi + 0.02,  0.24), QString::fromUtf8("\xcf\x80"));
    p.setPen(QColor(0x40, 0xe0, 0x70, 200));
    p.drawText(toSc(kGammaE  + 0.02, -0.26), "e");

    /* ── Separa per serie ── */
    QVector<MathPt> piPts, ePts, primePts;
    for (const MathPt& pt : m_mathPts) {
        if      (pt.seriesId == 0) piPts    << pt;
        else if (pt.seriesId == 1) ePts     << pt;
        else                       primePts << pt;
    }

    /* Offset verticale fisso (coordinate Γ) per separare visivamente le tre serie */
    const double yPi    = +0.10;   /* π sopra l'asse reale */
    const double yE     = -0.10;   /* e sotto l'asse reale */

    /* ── Serie π (arancio) ── */
    if (!piPts.isEmpty()) {
        const QColor col(0xff, 0xa5, 0x00, 220);
        /* Linea di convergenza */
        p.setPen(QPen(col, 1.2));
        p.setBrush(Qt::NoBrush);
        QPainterPath path;
        path.moveTo(toSc(piPts[0].re, yPi));
        for (int i = 1; i < piPts.size(); ++i) path.lineTo(toSc(piPts[i].re, yPi));
        p.drawPath(path);
        /* Punti */
        p.setPen(Qt::NoPen);
        p.setBrush(col);
        for (int i = 0; i < piPts.size(); ++i) {
            double r = (i == piPts.size() - 1) ? 4.5 : 2.4;
            p.drawEllipse(toSc(piPts[i].re, yPi), r, r);
        }
        /* Freccia tratteggiata verso target */
        p.setPen(QPen(col.darker(150), 0.8, Qt::DotLine));
        p.drawLine(toSc(piPts.back().re, yPi), toSc(kGammaPi, yPi));
    }

    /* ── Serie e (verde smeraldo) ── */
    if (!ePts.isEmpty()) {
        const QColor col(0x40, 0xe0, 0x70, 220);
        p.setPen(QPen(col, 1.2));
        p.setBrush(Qt::NoBrush);
        QPainterPath path;
        path.moveTo(toSc(ePts[0].re, yE));
        for (int i = 1; i < ePts.size(); ++i) path.lineTo(toSc(ePts[i].re, yE));
        p.drawPath(path);
        p.setPen(Qt::NoPen);
        p.setBrush(col);
        for (int i = 0; i < ePts.size(); ++i) {
            double r = (i == ePts.size() - 1) ? 4.5 : 2.4;
            p.drawEllipse(toSc(ePts[i].re, yE), r, r);
        }
        p.setPen(QPen(col.darker(150), 0.8, Qt::DotLine));
        p.drawLine(toSc(ePts.back().re, yE), toSc(kGammaE, yE));
    }

    /* ── Primi (rosso, sull'asse reale) ── */
    if (!primePts.isEmpty()) {
        const QColor col(0xff, 0x55, 0x44, 220);
        p.setPen(Qt::NoPen);
        p.setBrush(col);
        for (const MathPt& pt : primePts)
            p.drawEllipse(toSc(pt.re, 0.0), 2.8, 2.8);
    }

    /* ── Punti notevoli ── */
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 7, QFont::Bold));
    p.setPen(QColor(0x77, 0xbb, 0xdd));
    p.drawText(toSc(-1.02, 0.04), "SC");
    p.drawText(toSc( 0.86, 0.04), "OC");
    p.drawText(toSc( 0.02, 0.06), "Z\xe2\x82\x80");

    /* ── Legenda ── */
    {
        double lx = area.right() - 168;
        double ly = area.top() + 28;
        p.setFont(QFont("Inter,Ubuntu,sans-serif", 8));

        auto drawItem = [&](QColor col, const QString& text) {
            p.setBrush(col); p.setPen(Qt::NoPen);
            p.drawEllipse(QPointF(lx + 4, ly + 4), 3.5, 3.5);
            p.setPen(QColor(0xcc, 0xcc, 0xcc));
            p.drawText(QPointF(lx + 14, ly + 8), text);
            ly += 17;
        };

        if (!piPts.isEmpty())
            drawItem(QColor(0xff,0xa5,0x00),
                     QString::fromUtf8("\xcf\x80 Leibniz (%1 termini)").arg(piPts.size()));
        if (!ePts.isEmpty())
            drawItem(QColor(0x40,0xe0,0x70),
                     QString("e Taylor (%1 termini)").arg(ePts.size()));
        if (!primePts.isEmpty())
            drawItem(QColor(0xff,0x55,0x44),
                     QString("Primi (%1)").arg(primePts.size()));
    }

    /* ── Hint ── */
    p.setPen(QColor(0x44, 0x55, 0x55));
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 8));
    p.drawText(QRectF(area.left(), area.bottom() - 16, area.width(), 14),
               Qt::AlignRight,
               "\xce\x93 = (z\xe2\x88\x92" "1)/(z+1)  \xe2\x80\x94  Rotella per zoom \xc2\xb7 Trascina");
}

/* ══════════════════════════════════════════════════════════════
   Smith Prime — rendering
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::drawSmithPrime(QPainter& p, const QRectF& area) {
    p.setRenderHint(QPainter::Antialiasing);

    /* ── Centro e raggio del disco unitario (con zoom/pan) ── */
    const double cx = area.center().x() + m_panNC.x();
    const double cy = area.center().y() + m_panNC.y();
    const double R  = std::min(area.width(), area.height()) * 0.44 * m_zoomNC;

    /* Conversione da coordinate Γ (re,im) a schermo */
    auto toSc = [&](double re, double im) -> QPointF {
        return QPointF(cx + re * R, cy - im * R);
    };

    /* ── Sfondo del disco ── */
    p.setBrush(QColor(0x0d, 0x1a, 0x26));
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPointF(cx, cy), R, R);

    /* ── Griglia Smith (clip al disco) ── */
    {
        QPainterPath clip;
        clip.addEllipse(QPointF(cx, cy), R + 1.5, R + 1.5);
        p.setClipPath(clip);

        QPen gridPen(QColor(0x1a, 0x42, 0x58, 160), 0.7);
        p.setPen(gridPen);
        p.setBrush(Qt::NoBrush);

        /* Cerchi a resistenza normalizzata costante r:
         * centro Γ = (r/(r+1), 0), raggio = 1/(r+1) */
        static const double kR[] = {0.2, 0.5, 1.0, 2.0, 5.0};
        for (double r : kR) {
            double cn = r / (r + 1.0);
            double rn = 1.0 / (r + 1.0);
            p.drawEllipse(QPointF(cx + cn * R, cy), rn * R, rn * R);
        }

        /* Archi a reattanza normalizzata costante x:
         * centro Γ = (1, 1/x), raggio = 1/|x|  — clippati al disco */
        static const double kX[] = {0.5, 1.0, 2.0, 5.0};
        for (double x : kX) {
            for (int sg : {1, -1}) {
                double xs   = x * sg;
                double cyG  = 1.0 / xs;
                double rxG  = std::abs(cyG);
                double scx  = cx + 1.0 * R;
                double scy  = cy - cyG * R;
                p.drawEllipse(QPointF(scx, scy), rxG * R, rxG * R);
            }
        }

        /* Asse reale (x = 0) */
        p.setPen(QPen(QColor(0x22, 0x66, 0x55, 160), 0.7));
        p.drawLine(toSc(-1.0, 0.0), toSc(1.0, 0.0));

        p.setClipping(false);
    }

    /* ── Cerchio unitario (bordo) ── */
    p.setPen(QPen(QColor(0x33, 0x99, 0xcc), 1.5));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QPointF(cx, cy), R, R);

    /* ── Etichette resistenza ── */
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 7));
    p.setPen(QColor(0x44, 0x88, 0xaa, 190));
    {
        static const double kR[] = {0.2, 0.5, 1.0, 2.0, 5.0};
        static const char*  kL[] = {"0.2", "0.5", "1", "2", "5"};
        for (int i = 0; i < 5; i++) {
            double cn = kR[i] / (kR[i] + 1.0);
            double rn = 1.0 / (kR[i] + 1.0);
            /* Etichetta sul punto dell'asse reale più a sinistra del cerchio */
            QPointF lp = toSc(cn - rn + 0.01, 0.06);
            p.drawText(lp, kL[i]);
        }
    }
    /* Etichetta r=0 vicino al bordo sinistro */
    p.drawText(toSc(-0.97, 0.05), "0");

    /* ── Punti: nessun dato disponibile ── */
    if (m_smithPts.isEmpty()) {
        p.setPen(QColor(0x66, 0x88, 0xaa));
        p.setFont(QFont("Inter,Ubuntu,sans-serif", 10));
        p.drawText(QRectF(cx - 140, cy - 14, 280, 28),
                   Qt::AlignCenter,
                   "Clicca \xe2\x80\x9cTraccia\xe2\x80\x9d per generare i punti");
        /* Titolo e return */
        p.setPen(QColor(0xaa, 0xcc, 0xee, 160));
        p.setFont(QFont("Inter,Ubuntu,sans-serif", 9, QFont::Bold));
        p.drawText(QRectF(area.left(), area.top(), area.width(), 24),
                   Qt::AlignHCenter | Qt::AlignVCenter,
                   "Diagramma di Smith \xe2\x80\x94 Numeri Primi");
        return;
    }

    /* ── Colore per seriesId ── */
    auto seriesColor = [](int id, bool gaussBorder) -> QColor {
        if (id == 4) return gaussBorder
                         ? QColor(0xff, 0xdd, 0x33, 230)   /* gaussiano bordo — giallo */
                         : QColor(0x44, 0x88, 0xff, 200);  /* gaussiano complesso — blu */
        if (id == 5) return QColor(0xff, 0x44, 0xdd, 230);  /* personalizzato — magenta */
        static const QColor kC[] = {
            QColor(0xff, 0x66, 0x44, 230),   /* 0 = primi  — rosso-arancio */
            QColor(0xff, 0xdd, 0x00, 220),   /* 1 = Fib    — giallo */
            QColor(0x00, 0xe0, 0xcc, 220),   /* 2 = tri    — ciano */
            QColor(0x44, 0xdd, 0x44, 220),   /* 3 = sqr    — verde */
        };
        return (id >= 0 && id < 4) ? kC[id] : QColor(0xaa, 0xaa, 0xaa);
    };

    /* ── Raggruppa punti reali per valore intero (collision detection) ── */
    /* I gaussiani vengono tenuti separati (hanno coordinate im≠0) */
    const double limSq = m_smithExpanded ? 4.0 : 1.04;

    /* offset verticale (in coord Γ) per ogni seriesId (0..5) */
    static const double kYOff[] = { 0.0, 0.06, 0.12, -0.06, 0.0, 0.18 };

    /* Mappa value → lista (seriesId, re, im_base) per serie reali */
    QMap<int, QVector<QPair<int,double>>> groups;  /* value → [(sid, re)] */
    for (const SmithPt& pt : m_smithPts) {
        if (pt.gaussian) continue;
        double mod2 = pt.re * pt.re + pt.im * pt.im;
        if (mod2 > limSq) continue;
        groups[pt.value].append({pt.seriesId, pt.re});
    }

    /* ── Disegna linee di collisione (dashed) ── */
    {
        QPen dashPen(QColor(0x88, 0x88, 0x88, 130), 0.8, Qt::DashLine);
        p.setPen(dashPen);
        p.setBrush(Qt::NoBrush);
        for (auto it = groups.cbegin(); it != groups.cend(); ++it) {
            const auto& items = it.value();
            if (items.size() < 2) continue;
            double re = items[0].second;
            /* y range: da min offset a max offset */
            double yMin = 1e9, yMax = -1e9;
            for (auto& [sid, r2] : items) {
                double yo = (sid >= 0 && sid < 6) ? kYOff[sid] : 0.0;
                yMin = std::min(yMin, yo);
                yMax = std::max(yMax, yo);
            }
            if (yMax > yMin)
                p.drawLine(toSc(re, yMin), toSc(re, yMax));
        }
    }

    /* ── Disegna punti reali (con offset per collisioni) ── */
    p.setPen(Qt::NoPen);
    for (auto it = groups.cbegin(); it != groups.cend(); ++it) {
        for (auto& [sid, re] : it.value()) {
            double yo = (sid >= 0 && sid < 6) ? kYOff[sid] : 0.0;
            QPointF sp = toSc(re, yo);
            QColor col = seriesColor(sid, false);
            p.setBrush(col);
            p.drawEllipse(sp, 2.5, 2.5);
        }
    }

    /* ── Disegna punti gaussiani ── */
    p.setPen(Qt::NoPen);
    for (const SmithPt& pt : m_smithPts) {
        if (!pt.gaussian) continue;
        double mod2 = pt.re * pt.re + pt.im * pt.im;
        if (mod2 > limSq) continue;
        bool border = (pt.ga == 0);
        QColor col = seriesColor(4, border);
        p.setBrush(col);
        p.drawEllipse(toSc(pt.re, pt.im), border ? 3.0 : 2.8, border ? 3.0 : 2.8);
    }

    /* ── Etichette numeriche sui gruppi (se abilitate) ── */
    if (m_smithShowLabels) {
        p.setFont(QFont("Inter,Ubuntu,sans-serif", 6));
        for (auto it = groups.cbegin(); it != groups.cend(); ++it) {
            int val = it.key();
            if (val > 9999) continue;  /* evita sovraffollamento */
            const auto& items = it.value();
            double re = items[0].second;
            /* posiziona l'etichetta sopra il gruppo */
            double yTop = -1e9;
            for (auto& [sid, r2] : items) {
                double yo = (sid >= 0 && sid < 6) ? kYOff[sid] : 0.0;
                yTop = std::max(yTop, yo);
            }
            QPointF lp = toSc(re, yTop + 0.04);
            p.setPen(QColor(0xee, 0xee, 0xee, 200));
            p.drawText(lp, QString::number(val));
        }
    }

    /* ── Punti notevoli ── */
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 7, QFont::Bold));
    p.setPen(QColor(0x77, 0xbb, 0xdd));
    p.drawText(toSc(-0.97, 0.05), "SC");
    p.drawText(toSc( 0.92, 0.05), "OC");
    p.drawText(toSc(-0.02, 0.06), "Z\xe2\x82\x80");

    /* ── Titolo ── */
    p.setPen(QColor(0xbb, 0xdd, 0xff, 190));
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 9, QFont::Bold));
    p.drawText(QRectF(area.left(), area.top(), area.width(), 22),
               Qt::AlignHCenter | Qt::AlignVCenter,
               "Diagramma di Smith \xe2\x80\x94 Numeri Primi");

    /* ── Legenda ── */
    {
        /* Conta punti per serie */
        int nSeries[6] = {0, 0, 0, 0, 0, 0};
        for (const SmithPt& sp : m_smithPts) {
            if (sp.gaussian) { ++nSeries[4]; continue; }
            int sid = sp.seriesId;
            if (sid >= 0 && sid < 6) ++nSeries[sid];
        }

        static const char* kNames[] = {
            "Primi reali", "Fibonacci", "Triangolari",
            "Quadrati perfetti", "Primi gaussiani", "Personalizzata"
        };

        double lx = area.right() - 162;
        double ly = area.top() + 28;
        p.setFont(QFont("Inter,Ubuntu,sans-serif", 8));

        for (int sid = 0; sid < 6; ++sid) {
            if (nSeries[sid] == 0) continue;
            QColor col = seriesColor(sid, sid == 4);
            p.setBrush(col);
            p.setPen(Qt::NoPen);
            p.drawEllipse(QPointF(lx + 4, ly + 4), 3.0, 3.0);
            p.setPen(QColor(0xcc, 0xcc, 0xcc));
            p.drawText(QPointF(lx + 12, ly + 8),
                       QString("%1 (%2)").arg(kNames[sid]).arg(nSeries[sid]));
            ly += 16;
        }
    }

    /* ── Hint in basso ── */
    p.setPen(QColor(0x44, 0x55, 0x55));
    p.setFont(QFont("Inter,Ubuntu,sans-serif", 8));
    p.drawText(QRectF(area.left(), area.bottom() - 16, area.width(), 14),
               Qt::AlignRight,
               "\xce\x93 = (z\xe2\x88\x92""1)/(z+1)  \xe2\x80\x94  Rotella per zoom \xc2\xb7 Trascina per spostare");
}

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

/* ══════════════════════════════════════════════════════════════
   paintEvent
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), QColor(0x18,0x18,0x18));
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
    if (e->button() == Qt::LeftButton) {
        m_dragging = true;
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
        m_rotY = m_rotYD + d.x() * 0.008;
        m_rotX = std::max(-M_PI/2 + 0.05,
                 std::min( M_PI/2 - 0.05, m_rotXD + d.y() * 0.008));
    } else {
        m_panNC = m_panD + d;
    }
    update();
}

void GraficoCanvas::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        m_dragging = false;
        setCursor(Qt::ArrowCursor);
    }
}

void GraficoCanvas::contextMenuEvent(QContextMenuEvent* e) {
    auto* menu = new QMenu(this);
    auto* actSave = menu->addAction("\xf0\x9f\x96\xbc  Salva come PNG...");
    connect(actSave, &QAction::triggered, this, [this]{
        QString path = QFileDialog::getSaveFileName(
            this, "Salva grafico", QDir::homePath() + "/grafico.png", "PNG (*.png)");
        if (path.isEmpty()) return;
        QPixmap px(size());
        render(&px);
        px.save(path);
        emit statusMessage("\xe2\x9c\x85  PNG salvato: " + path);
    });
    menu->addSeparator();
    auto* actReset = menu->addAction("\xf0\x9f\x94\x84  Reset vista");
    connect(actReset, &QAction::triggered, this, [this]{ resetView(); });
    menu->exec(e->globalPos());
    menu->deleteLater();
}

/* ══════════════════════════════════════════════════════════════
   GraficoPage — layout
   ══════════════════════════════════════════════════════════════ */
GraficoPage::GraficoPage(AiClient* ai, QWidget* parent) : QWidget(parent), m_ai(ai) {
    auto* mainLay = new QHBoxLayout(this);
    mainLay->setContentsMargins(0,0,0,0);
    mainLay->setSpacing(0);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setHandleWidth(4);
    splitter->addWidget(buildLeftPanel());

    m_canvas = new GraficoCanvas(this);
    connect(m_canvas, &GraficoCanvas::statusMessage,
            this, [this](const QString& msg){ m_statusLbl->setText(msg); });
    splitter->addWidget(m_canvas);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({278, 700});
    mainLay->addWidget(splitter);
}

/* ── Pannello parametri sinistro ─────────────────────────────── */
QWidget* GraficoPage::buildLeftPanel() {
    auto* panel = new QWidget(this);
    panel->setObjectName("leftPanel");
    panel->setMinimumWidth(220);
    panel->setMaximumWidth(310);

    auto* lay = new QVBoxLayout(panel);
    lay->setContentsMargins(10,10,10,10);
    lay->setSpacing(8);

    /* Titolo */
    auto* title = new QLabel("\xf0\x9f\x93\x88  Grafico", panel);  /* 📈 */
    title->setObjectName("pageTitle");
    title->setFont(QFont("Inter,Ubuntu,sans-serif", 14, QFont::Bold));
    lay->addWidget(title);

    /* Tipo grafico */
    auto* lblType = new QLabel("Tipo:", panel);
    lblType->setObjectName("formLabel");
    lay->addWidget(lblType);

    m_typeCombo = new QComboBox(panel);
    m_typeCombo->setObjectName("settingCombo");
    m_typeCombo->addItem("Cartesiano  y = f(x)",      0);
    m_typeCombo->addItem("Torta",                     1);
    m_typeCombo->addItem("Istogramma",                2);
    m_typeCombo->addItem("Scatter 2D  (x, y)",       3);
    m_typeCombo->addItem("Grafo 2D",                  4);
    m_typeCombo->addItem("Scatter 3D",                5);
    m_typeCombo->addItem("Grafo 3D",                  6);
    m_typeCombo->addItem("\xf0\x9f\x94\xb5  Smith — Numeri Primi", 7);  /* 🔵 */
    m_typeCombo->addItem(QString::fromUtf8("\xf0\x9f\x93\x90  Smith \xe2\x80\x94 \xcf\x80 \xc2\xb7 e \xc2\xb7 Primi"), 8);  /* 📐 */
    m_typeCombo->addItem("Linea multi-serie",              9);
    m_typeCombo->addItem("Polare  r = f(\xce\xb8)",       10);
    m_typeCombo->addItem("Radar (Spider)",                 11);
    m_typeCombo->addItem("Bolle  (x, y, raggio)",         12);
    m_typeCombo->addItem("Heatmap (matrice colori)",       13);
    m_typeCombo->addItem("Candele OHLC",                   14);
    m_typeCombo->addItem("Area riempita",                  15);
    m_typeCombo->addItem("Cascata (Waterfall)",            16);
    m_typeCombo->addItem("Scalini (Step)",                 17);
    m_typeCombo->addItem("Colonne (Column)",               18);
    m_typeCombo->addItem("Barre orizzontali (HBar)",       19);
    m_typeCombo->addItem("Barre raggruppate (Grouped)",    20);
    m_typeCombo->addItem("Barre impilate (Stacked)",       21);
    m_typeCombo->addItem("Barre impilate 100%",            22);
    m_typeCombo->addItem("Imbuto (Funnel)",                23);
    m_typeCombo->addItem("Ciambella (Donut)",              24);
    m_typeCombo->addItem("Treemap",                        25);
    m_typeCombo->addItem("Sunburst",                       26);
    m_typeCombo->addItem("Box Plot",                       27);
    m_typeCombo->addItem("Dot Plot",                       28);
    m_typeCombo->addItem("Densit\xc3\xa0 (KDE)",          29);
    m_typeCombo->addItem("Area impilata (Stacked Area)",   30);
    m_typeCombo->addItem("OHLC (barre)",                   31);
    m_typeCombo->addItem("Gauge (semicerchio)",            32);
    m_typeCombo->addItem("Bullet Chart",                   33);
    m_typeCombo->addItem("Gantt",                          34);
    m_typeCombo->addItem("Piramide (Pyramid)",             35);
    m_typeCombo->addItem("Coordinate parallele",           36);
    m_typeCombo->addItem("Sankey",                         37);
    m_typeCombo->addItem("Albero (Tree)",                  38);
    m_typeCombo->addItem("Chord",                          39);
    m_typeCombo->addItem("Violin Plot",                    40);
    m_typeCombo->addItem("Word Cloud",                     41);
    m_typeCombo->addItem("Albero Radiale",                 42);
    m_typeCombo->addItem("Linea Animata",                  43);
    m_typeCombo->addItem("Small Multiples",                44);
    lay->addWidget(m_typeCombo);

    /* ── Stack parametri ── */
    m_paramStack = new QStackedWidget(panel);

    /* [0] Cartesiano */
    {
        auto* w  = new QWidget;
        auto* fl = new QFormLayout(w);
        fl->setContentsMargins(0,0,0,0);
        fl->setSpacing(6);

        m_formulaEdit = new QLineEdit(w);
        m_formulaEdit->setObjectName("inputLine");
        m_formulaEdit->setPlaceholderText("sin(x) * 2 + 1");
        m_formulaEdit->setText("sin(x)");
        connect(m_formulaEdit, &QLineEdit::returnPressed, this, &GraficoPage::plot);
        fl->addRow("y = f(x):", m_formulaEdit);

        m_xMinSpin = new QDoubleSpinBox(w);
        m_xMinSpin->setRange(-1e6, 1e6);
        m_xMinSpin->setValue(-6.28);
        m_xMinSpin->setSingleStep(1.0);
        m_xMinSpin->setDecimals(3);
        fl->addRow("x min:", m_xMinSpin);

        m_xMaxSpin = new QDoubleSpinBox(w);
        m_xMaxSpin->setRange(-1e6, 1e6);
        m_xMaxSpin->setValue(6.28);
        m_xMaxSpin->setSingleStep(1.0);
        m_xMaxSpin->setDecimals(3);
        fl->addRow("x max:", m_xMaxSpin);

        auto* hint = new QLabel(
            "Funzioni:  sin(x)  cos(x)  tan(x)\n"
            "           sqrt(x)  abs(x)  exp(x)\n"
            "           log(x)  ln(x)  log2(x)\n"
            "           ceil(x)  floor(x)\n"
            "\n"
            "Costanti:  pi = 3.14159265\n"
            "           e  = 2.71828182\n"
            "\n"
            "Esempi:\n"
            "  sin(x)                (sinusoide)\n"
            "  x^2 - 3*x + 2        (parabola)\n"
            "  1 / (1 + exp(-x))    (sigmoide)\n"
            "  sqrt(1 - x^2)        (semicerchio)\n"
            "  sin(x) / x           (sinc)\n"
            "  2^x                  (esponenziale)\n"
            "  abs(sin(x)) * x      (inviluppo)", w);
        hint->setObjectName("hintLabel");
        hint->setFont(QFont("JetBrains Mono,Fira Code,Consolas", 8));
        hint->setWordWrap(false);
        fl->addRow(hint);

        m_paramStack->addWidget(w);   /* idx 0 */
    }

    /* [1] Dati generici — condiviso da Torta/Isto/Scatter/Grafo/3D */
    {
        auto* w  = new QWidget;
        auto* vl = new QVBoxLayout(w);
        vl->setContentsMargins(0,0,0,0);
        vl->setSpacing(4);

        /* ── Sezione "Genera da espressione" ─────────────────────── */
        m_genSection = new QWidget(w);
        {
            auto* gl = new QVBoxLayout(m_genSection);
            gl->setContentsMargins(0, 0, 0, 2);
            gl->setSpacing(4);

            auto* titleLbl = new QLabel(
                "\xf0\x9f\x93\x90  Genera da espressione", m_genSection);
            titleLbl->setObjectName("formLabel");
            gl->addWidget(titleLbl);

            /* Formula */
            m_genExpr = new QLineEdit(m_genSection);
            m_genExpr->setObjectName("inputLine");
            m_genExpr->setFont(QFont("JetBrains Mono,Fira Code,Consolas", 9));
            m_genExpr->setPlaceholderText("f(x)  es: x^2  sin(x)  x*(x+1)/2");
            gl->addWidget(m_genExpr);

            /* Range — da/a/passo su una riga */
            auto* rangeRow = new QWidget(m_genSection);
            auto* rl = new QHBoxLayout(rangeRow);
            rl->setContentsMargins(0, 0, 0, 0);
            rl->setSpacing(4);

            auto* lFrom = new QLabel("x da:", rangeRow);
            lFrom->setObjectName("formLabel");
            m_genFrom = new QDoubleSpinBox(rangeRow);
            m_genFrom->setObjectName("inputLine");
            m_genFrom->setRange(-1e6, 1e6);
            m_genFrom->setValue(1.0);
            m_genFrom->setDecimals(2);
            m_genFrom->setSingleStep(1.0);

            auto* lTo = new QLabel("a:", rangeRow);
            lTo->setObjectName("formLabel");
            m_genTo = new QDoubleSpinBox(rangeRow);
            m_genTo->setObjectName("inputLine");
            m_genTo->setRange(-1e6, 1e6);
            m_genTo->setValue(10.0);
            m_genTo->setDecimals(2);
            m_genTo->setSingleStep(1.0);

            auto* lStep = new QLabel("\xce\x94:", rangeRow);  /* Δ */
            lStep->setObjectName("formLabel");
            m_genStep = new QDoubleSpinBox(rangeRow);
            m_genStep->setObjectName("inputLine");
            m_genStep->setRange(1e-6, 1e6);
            m_genStep->setValue(1.0);
            m_genStep->setDecimals(3);
            m_genStep->setSingleStep(0.5);

            rl->addWidget(lFrom);
            rl->addWidget(m_genFrom, 2);
            rl->addWidget(lTo);
            rl->addWidget(m_genTo, 2);
            rl->addWidget(lStep);
            rl->addWidget(m_genStep, 2);
            gl->addWidget(rangeRow);

            auto* btnGen = new QPushButton(
                "\xe2\x96\xb6  Genera \xe2\x86\x92 inserisce i valori sotto", m_genSection);
            btnGen->setObjectName("actionBtn");
            btnGen->setFixedHeight(26);
            connect(btnGen, &QPushButton::clicked, this, [this]() {
                QString expr = m_genExpr->text().trimmed();
                if (expr.isEmpty()) return;
                FormulaParser fp(expr);
                if (!fp.ok()) {
                    m_statusLbl->setText("\xe2\x9a\xa0  " + fp.err());
                    return;
                }
                double x0   = m_genFrom->value();
                double x1   = m_genTo->value();
                double step = m_genStep->value();
                if (x0 > x1 || step <= 0) {
                    m_statusLbl->setText("\xe2\x9a\xa0  Range non valido");
                    return;
                }

                int type = m_typeCombo->currentIndex();
                QString out;
                int idx = 0;
                for (double x = x0; x <= x1 + step * 1e-9; x += step) {
                    double y = fp.eval(x);
                    if (!std::isfinite(y)) { ++idx; continue; }
                    QString sx = QString::number(x,  'g', 8);
                    QString sy = QString::number(y,  'g', 8);
                    if (type == 1 || type == 2) {
                        /* Torta / Istogramma: "etichetta: valore" */
                        out += QString("x=%1: %2\n").arg(sx, sy);
                    } else if (type == 3) {
                        /* Scatter XY: "x, y" */
                        out += QString("%1, %2\n").arg(sx, sy);
                    } else if (type == 5) {
                        /* Scatter 3D: "x, y, 0" — usa x come X, f(x) come Y, 0 come Z */
                        out += QString("%1, %2, 0\n").arg(sx, sy);
                    } else {
                        /* default: "x, y" */
                        out += QString("%1, %2\n").arg(sx, sy);
                    }
                    ++idx;
                }
                if (out.isEmpty()) {
                    m_statusLbl->setText("\xe2\x9a\xa0  Nessun valore finito generato");
                    return;
                }
                m_dataEdit->setPlainText(out.trimmed());
                m_statusLbl->setText(
                    QString("\xe2\x9c\x85  %1 valori generati da %2").arg(idx).arg(expr));
            });
            gl->addWidget(btnGen);

            auto* sepLine = new QFrame(m_genSection);
            sepLine->setFrameShape(QFrame::HLine);
            sepLine->setObjectName("separator");
            gl->addWidget(sepLine);
        }
        vl->addWidget(m_genSection);

        /* ── Hint formato manuale ───────────────────────────────── */
        m_dataHint = new QLabel("etichetta:valore (una per riga)", w);
        m_dataHint->setObjectName("hintLabel");
        m_dataHint->setFont(QFont("Inter,Ubuntu,sans-serif", 8));
        m_dataHint->setWordWrap(true);
        vl->addWidget(m_dataHint);

        m_dataEdit = new QTextEdit(w);
        m_dataEdit->setObjectName("inputArea");
        m_dataEdit->setFont(QFont("JetBrains Mono,Fira Code,Consolas", 9));
        m_dataEdit->setMinimumHeight(100);
        vl->addWidget(m_dataEdit, 1);

        m_paramStack->addWidget(w);   /* idx 1 */
    }

    /* [2] Smith Prime */
    {
        auto* w  = new QWidget;
        auto* fl = new QFormLayout(w);
        fl->setContentsMargins(0, 0, 0, 0);
        fl->setSpacing(7);

        m_smithMaxN = new QSpinBox(w);
        m_smithMaxN->setRange(10, 50000);
        m_smithMaxN->setValue(300);
        m_smithMaxN->setSingleStep(50);
        m_smithMaxN->setSuffix("  (n max)");
        m_smithMaxN->setObjectName("inputLine");
        fl->addRow("Fino a:", m_smithMaxN);

        auto* normRow = new QWidget(w);
        auto* normLay = new QHBoxLayout(normRow);
        normLay->setContentsMargins(0,0,0,0); normLay->setSpacing(4);
        m_smithNorm = new QDoubleSpinBox(normRow);
        m_smithNorm->setRange(0.1, 20.0);
        m_smithNorm->setValue(1.0);
        m_smithNorm->setSingleStep(0.1);
        m_smithNorm->setDecimals(2);
        m_smithNorm->setObjectName("inputLine");
        normLay->addWidget(m_smithNorm, 1);
        fl->addRow("Scala:", normRow);

        m_smithReal  = new QCheckBox("Primi reali (rosso)",           w);
        m_smithGauss = new QCheckBox("Primi gaussiani (blu/giallo)",  w);
        m_smithFib   = new QCheckBox("Fibonacci (giallo)",            w);
        m_smithTri   = new QCheckBox("Triangolari (ciano)",           w);
        m_smithSqr   = new QCheckBox("Quadrati perfetti (verde)",     w);
        m_smithExp   = new QCheckBox("Dominio espanso (|Γ|≤2)",       w);
        m_smithLabels= new QCheckBox("Mostra etichette numeriche",    w);
        m_smithReal ->setChecked(true);
        m_smithGauss->setChecked(true);
        m_smithFib  ->setChecked(false);
        m_smithTri  ->setChecked(false);
        m_smithSqr  ->setChecked(false);
        m_smithExp  ->setChecked(false);
        m_smithLabels->setChecked(false);
        fl->addRow(m_smithReal);
        fl->addRow(m_smithGauss);
        fl->addRow(m_smithFib);
        fl->addRow(m_smithTri);
        fl->addRow(m_smithSqr);
        fl->addRow(m_smithExp);
        fl->addRow(m_smithLabels);

        /* ── Separatore serie personalizzata ── */
        auto* sep = new QFrame(w);
        sep->setFrameShape(QFrame::HLine);
        sep->setObjectName("separator");
        fl->addRow(sep);

        auto* customLbl = new QLabel(
            "\xf0\x9f\x93\x9d  Serie personalizzata (magenta)", w);
        customLbl->setObjectName("formLabel");
        fl->addRow(customLbl);

        m_smithCustom = new QTextEdit(w);
        m_smithCustom->setObjectName("inputArea");
        m_smithCustom->setFont(QFont("JetBrains Mono,Fira Code,Consolas", 9));
        m_smithCustom->setFixedHeight(80);
        m_smithCustom->setPlaceholderText(
            "Inserisci numeri interi \xe2\x89\xa5 2\n"
            "Uno per riga, o separati da spazi/virgole.\n"
            "Es:  3  7  13  21  34  55  89");
        fl->addRow(m_smithCustom);

        m_btnSmithFile = new QPushButton(
            "\xf0\x9f\x93\x82  Carica sequenza da file (.txt)", w);
        m_btnSmithFile->setObjectName("actionBtn");
        m_btnSmithFile->setFixedHeight(28);
        m_btnSmithFile->setToolTip(
            "Apri un file di testo con numeri interi >= 2.\n"
            "Formati accettati: uno per riga, separati da spazi o virgole.");
        connect(m_btnSmithFile, &QPushButton::clicked, this, [this]() {
            QString path = QFileDialog::getOpenFileName(
                this, "Carica sequenza", QDir::homePath(),
                "File testo (*.txt *.csv *.dat);;Tutti i file (*)");
            if (path.isEmpty()) return;
            QFile f(path);
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
            m_smithCustom->setPlainText(
                QString::fromUtf8(f.readAll()));
        });
        fl->addRow(m_btnSmithFile);

        auto* hint = new QLabel(
            QString::fromUtf8(
                "\xce\x93 = (z\xe2\x88\x92""1)/(z+1)   z = log\xe2\x82\x82(N)*scala\n"
                "Stessa posizione = stesso N in pi\xc3\xb9 serie:\n"
                "punti impilati + linea tratteggiata.\n"
                "Gaussiani: a+bi con a\xc2\xb2+b\xc2\xb2 primo"),
            w);
        hint->setObjectName("hintLabel");
        hint->setFont(QFont("Inter,Ubuntu,sans-serif", 8));
        hint->setWordWrap(true);
        fl->addRow(hint);

        m_paramStack->addWidget(w);   /* idx 2 */
    }

    /* [3] Math Const — π · e · Primi */
    {
        auto* w  = new QWidget;
        auto* fl = new QFormLayout(w);
        fl->setContentsMargins(0, 0, 0, 0);
        fl->setSpacing(7);

        m_mathTerms = new QSpinBox(w);
        m_mathTerms->setRange(2, 300);
        m_mathTerms->setValue(40);
        m_mathTerms->setSingleStep(10);
        m_mathTerms->setSuffix("  termini");
        m_mathTerms->setObjectName("inputLine");
        fl->addRow("Termini:", m_mathTerms);

        m_mathPi     = new QCheckBox(QString::fromUtf8(
            "\xcf\x80  Serie di Leibniz"), w);       /* π */
        m_mathE      = new QCheckBox("e  Serie di Taylor", w);
        m_mathPrimes = new QCheckBox("Primi reali (log\xe2\x82\x82)", w);  /* log₂ */
        m_mathPi    ->setChecked(true);
        m_mathE     ->setChecked(true);
        m_mathPrimes->setChecked(true);
        fl->addRow(m_mathPi);
        fl->addRow(m_mathE);
        fl->addRow(m_mathPrimes);

        auto* hint = new QLabel(
            QString::fromUtf8(
                "Mappe di M\xc3\xb6""bius: \xce\x93=(z\xe2\x88\x92" "1)/(z+1)\n"
                "\n"
                "\xcf\x80 (arancio) — oscillazione\n"
                "  S\xe2\x82\x99 = 4\xc2\xb7\xce\xa3(-1)\xe2\x81\xb5/(2k+1)\n"
                "  \xe2\x86\x92 \xce\x93(\xcf\x80) \xe2\x89\x88 0.5168\n"
                "\n"
                "e (verde) — convergenza monotona\n"
                "  E\xe2\x82\x99 = \xce\xa3 1/k!\n"
                "  \xe2\x86\x92 \xce\x93(e) \xe2\x89\x88 0.4621\n"
                "\n"
                "Primi (rosso) — scala log\xe2\x82\x82\n"
                "  z = log\xe2\x82\x82(p)\n"
                "  sull'asse reale del disco"),
            w);
        hint->setObjectName("hintLabel");
        hint->setFont(QFont("Inter,Ubuntu,sans-serif", 8));
        hint->setWordWrap(true);
        fl->addRow(hint);

        m_paramStack->addWidget(w);   /* idx 3 */
    }

    /* [4] Polare r = f(θ) */
    {
        auto* w  = new QWidget;
        auto* fl = new QFormLayout(w);
        fl->setContentsMargins(0, 0, 0, 0);
        fl->setSpacing(7);

        m_polarFormula = new QLineEdit(w);
        m_polarFormula->setObjectName("inputLine");
        m_polarFormula->setPlaceholderText("cos(3*x)  oppure  sin(2*x)*2");
        m_polarFormula->setText("cos(3*x)");
        connect(m_polarFormula, &QLineEdit::returnPressed, this, &GraficoPage::plot);
        fl->addRow("r = f(\xce\xb8):", m_polarFormula);

        m_polarTMinSpin = new QDoubleSpinBox(w);
        m_polarTMinSpin->setRange(-100.0, 100.0);
        m_polarTMinSpin->setValue(0.0);
        m_polarTMinSpin->setSingleStep(0.1);
        m_polarTMinSpin->setDecimals(3);
        m_polarTMinSpin->setObjectName("inputLine");
        fl->addRow("\xce\xb8 min:", m_polarTMinSpin);

        m_polarTMaxSpin = new QDoubleSpinBox(w);
        m_polarTMaxSpin->setRange(-100.0, 100.0);
        m_polarTMaxSpin->setValue(6.283);
        m_polarTMaxSpin->setSingleStep(0.1);
        m_polarTMaxSpin->setDecimals(3);
        m_polarTMaxSpin->setObjectName("inputLine");
        fl->addRow("\xce\xb8 max:", m_polarTMaxSpin);

        m_polarSamples = new QSpinBox(w);
        m_polarSamples->setRange(50, 5000);
        m_polarSamples->setValue(800);
        m_polarSamples->setSingleStep(100);
        m_polarSamples->setObjectName("inputLine");
        fl->addRow("Campioni:", m_polarSamples);

        auto* hint = new QLabel(
            "Esempi:\n"
            "  cos(3*x)          (rosa a 3 petali)\n"
            "  sin(2*x)*2        (rosa a 4 petali)\n"
            "  1 - cos(x)        (cardioide)\n"
            "  1 + 2*cos(x)      (limacon)\n"
            "  x / (2*pi)        (spirale)\n"
            "  sqrt(cos(2*x))    (lemniscata)", w);
        hint->setObjectName("hintLabel");
        hint->setFont(QFont("JetBrains Mono,Fira Code,Consolas", 8));
        hint->setWordWrap(false);
        fl->addRow(hint);

        m_paramStack->addWidget(w);   /* idx 4 */
    }

    lay->addWidget(m_paramStack, 1);

    /* Esempi per ogni tipo non-cartesiano */
    static const char* kExamples[] = {
        /* 0 Cartesiano — non usa dataEdit */
        "",
        /* 1 Torta — etichetta:valore */
        "Nord:42\nSud:28\nEst:19\nOvest:11",
        /* 2 Istogramma — etichetta:valore */
        "Gen:82\nFeb:67\nMar:91\nApr:78\nMag:95\nGiu:88\nLug:76\nAgo:60\nSet:85\nOtt:93\nNov:71\nDic:55",
        /* 3 Scatter (x, y [, etichetta]) */
        "0.0, 0.0, Origine\n1.0, 2.5, A\n2.0, 3.8, B\n3.5, 6.1, C\n4.0, 7.2, D\n5.0, 9.8, E\n6.5, 11.4, F\n7.0, 13.0, G",
        /* 4 Grafo (nodo-nodo) */
        "A-B\nA-C\nB-D\nC-D\nD-E\nB-E",
        /* 5 Scatter 3D (x, y, z [, etichetta]) */
        "0, 0, 0, O\n1, 0, 0, X\n0, 1, 0, Y\n0, 0, 1, Z\n1, 1, 0, XY\n1, 0, 1, XZ\n0, 1, 1, YZ\n1, 1, 1, XYZ",
        /* 6 Grafo 3D — tetraedro regolare */
        "A, 0, 0, 0\nB, 2, 0, 0\nC, 1, 1.73, 0\nD, 1, 0.58, 1.63\nA-B\nA-C\nA-D\nB-C\nB-D\nC-D",
        /* 7 Smith — non usa dataEdit */  "",
        /* 8 MathConst — non usa dataEdit */ "",
        /* 9 Linea multi-serie: x, y1, y2, ... */
        "# Vendite  Costi\n0, 10, 8\n1, 14, 9\n2, 18, 11\n3, 22, 13\n4, 19, 15\n5, 25, 16",
        /* 10 Polare — non usa dataEdit */ "",
        /* 11 Radar */
        "Velocita:85\nResistenza:72\nAgilita:90\nForza:60\nPrecisione:78",
        /* 12 Bolle: x, y, raggio [, etichetta] */
        "1.0, 2.5, 30, Alfa\n2.5, 4.0, 50, Beta\n4.0, 1.5, 20, Gamma\n5.5, 3.8, 70, Delta\n7.0, 2.0, 45, Epsilon",
        /* 13 Heatmap: righe di valori separati da spazio/virgola */
        "1 2 3 4 5\n6 7 8 9 10\n11 12 13 14 15\n16 17 18 19 20\n21 22 23 24 25",
        /* 14 Candlestick: Etichetta, Open, High, Low, Close */
        "Gen, 100, 115, 98, 112\nFeb, 112, 120, 108, 105\nMar, 105, 125, 102, 118\nApr, 118, 130, 115, 125\nMag, 125, 128, 110, 108",
        /* 15 Area — stessa struttura di Linea */
        "# Entrate  Uscite\n0, 12, 8\n1, 16, 10\n2, 22, 12\n3, 28, 14\n4, 25, 18\n5, 32, 20",
        /* 16 Waterfall */
        "Inizio:1000\nVendite:+350\nResi:-80\nCosti:-200\nSussidi:+120\nFine:1190",
        /* 17 Step — stessa struttura di Linea */
        "# Soglia  Reale\n0, 10, 9\n1, 10, 12\n2, 15, 14\n3, 15, 17\n4, 20, 18\n5, 20, 22"
    };

    static const char* kHints[] = {
        "",
        "Formato: etichetta:valore  (una voce per riga)\n"
        "Accetta anche solo numeri (es. 42)",
        "Formato: etichetta:valore  (una voce per riga)\n"
        "Accetta anche solo numeri (es. 82)",
        "Formato: x, y [, Nome]  (una per riga)\n"
        "  2.5, 3.1, A\n"
        "  4.0, 7.2, B\n"
        "  6.5, 11.4\n"
        "Se il nome manca usa P1, P2, P3...",
        "Formato: Nodo1-Nodo2  (un arco per riga)\n"
        "Accetta anche: ->  \xe2\x80\x93  (trattino lungo)",
        "Formato: x, y, z [, Nome]  (una per riga)\n"
        "  0, 0, 0, Origine\n"
        "  1, 0, 0, X\n"
        "  0, 1, 0, Y\n"
        "Se il nome manca usa P1, P2, P3...",
        /* 6 Grafo 3D */
        "Nodi:   Nome, x, y, z\n"
        "Archi:  NodoA-NodoB\n"
        "(nodi e archi nello stesso testo)\n"
        "Esempio:\n"
        "  A, 0, 0, 0\n"
        "  B, 1, 0, 0\n"
        "  C, 0, 1, 0\n"
        "  A-B\n"
        "  A-C\n"
        "  B-C\n"
        "# Le righe con # sono ignorate",
        /* 7 SmithPrime */ "",
        /* 8 MathConst  */ "",
        /* 9 Linea multi-serie */
        "Formato: x, y1, y2, ... (colonne = serie)\n"
        "Riga #: nomi delle serie (opzionale)\n"
        "  # SerieA SerieB SerieC\n"
        "  0, 10, 8, 5\n"
        "  1, 14, 9, 7",
        /* 10 Polare — pannello dedicato */ "",
        /* 11 Radar */
        "Formato: Categoria:valore (una per riga)\n"
        "Minimo 3 categorie.\n"
        "  Velocita:85\n"
        "  Resistenza:72\n"
        "  Forza:60",
        /* 12 Bolle */
        "Formato: x, y, raggio [, Nome]\n"
        "  1.0, 2.5, 30, Alfa\n"
        "  2.5, 4.0, 50, Beta\n"
        "Il raggio e' in unita' pixel normalizzate.",
        /* 13 Heatmap */
        "Formato: valori separati da spazio o virgola,\n"
        "una riga per ogni riga della matrice.\n"
        "  1 2 3\n"
        "  4 5 6\n"
        "  7 8 9",
        /* 14 Candlestick */
        "Formato: Etichetta, Open, High, Low, Close\n"
        "  Gen, 100, 115, 98, 112\n"
        "  Feb, 112, 120, 108, 105",
        /* 15 Area — come Linea */
        "Formato: x, y1, y2, ... (colonne = serie)\n"
        "L'area sotto ogni curva viene riempita.",
        /* 16 Waterfall */
        "Formato: Etichetta:valore (una per riga)\n"
        "Usa +/- per indicare variazioni.\n"
        "L'ultima voce e' il totale cumulato.",
        /* 17 Step — come Linea */
        "Formato: x, y1, y2, ... (colonne = serie)\n"
        "Le linee vengono tracciate a scalini."
    };

    /* Connetti combo */
    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        m_canvas->setType(static_cast<GraficoCanvas::ChartType>(idx));
        if (idx == 0) {
            m_paramStack->setCurrentIndex(0);
        } else if (idx == 7) {
            /* Smith Prime — pannello dedicato, genera subito con i valori correnti */
            m_paramStack->setCurrentIndex(2);
            m_canvas->setSmithPrime(
                m_smithMaxN->value(),
                m_smithReal->isChecked(),
                m_smithGauss->isChecked(),
                m_smithExp->isChecked(),
                m_smithNorm->value(),
                m_smithFib->isChecked(),
                m_smithTri->isChecked(),
                m_smithSqr->isChecked(),
                m_smithLabels->isChecked()
            );
        } else if (idx == 8) {
            /* Math Const — π · e · Primi, genera subito */
            m_paramStack->setCurrentIndex(3);
            m_canvas->setMathConst(
                m_mathTerms->value(),
                m_mathPi    ->isChecked(),
                m_mathE     ->isChecked(),
                m_mathPrimes->isChecked()
            );
        } else if (idx == 10) {
            /* Polare — pannello dedicato */
            m_paramStack->setCurrentIndex(4);
        } else {
            /* Tutti gli altri tipi usano il pannello dati generico (idx 1) */
            m_paramStack->setCurrentIndex(1);
            /* Carica hint ed esempi per tutti i tipi che usano questo pannello */
            static const int kMaxHint = 17;
            if (idx >= 0 && idx <= kMaxHint &&
                idx != 7 && idx != 8 && idx != 10) {
                m_dataHint->setText(kHints[idx]);
                if (!QByteArray(kExamples[idx]).isEmpty())
                    m_dataEdit->setPlainText(kExamples[idx]);
            }
            /* hint per i nuovi tipi 40-44 */
            static const char* kNewHints[] = {
                /* 40 Violin */
                "Formato: NomeSerie: v1 v2 v3 ...\nUna serie per riga.\n"
                "Esempio:\n  Gruppo A: 12 18 22 25 30 35 40\n  Gruppo B: 8 14 20 28 35 42",
                /* 41 WordCloud */
                "Formato: parola:frequenza (una per riga)\n"
                "Esempio:\n  Python:95\n  C++:80\n  Java:70\n  Rust:60",
                /* 42 RadialTree */
                "Formato: padre: figlio1, figlio2\nUguale all'Albero lineare.\n"
                "Esempio:\n  Root: A, B\n  A: C, D\n  B: E",
                /* 43 AnimatedLine */
                "Formato: stessa struttura di Linea multi-serie\n"
                "x, y1, y2, ... (colonne = serie)\n"
                "La linea viene disegnata progressivamente.",
                /* 44 SmallMultiples */
                "Formato: stessa struttura di Linea multi-serie\n"
                "x, y1, y2, ... (colonne = serie)\n"
                "Ogni serie viene mostrata in un pannello separato."
            };
            static const char* kNewExamples[] = {
                /* 40 Violin */
                "Gruppo A: 12 18 22 25 30 35 40 28 32 26\n"
                "Gruppo B: 8 14 20 28 35 42 30 24 18 22\n"
                "Gruppo C: 5 10 15 22 30 38 45 35 28 20",
                /* 41 WordCloud */
                "Python:95\nC++:80\nJava:70\nRust:60\nGo:55\n"
                "JavaScript:50\nTypeScript:45\nSwift:40\nKotlin:35\n"
                "Ruby:30\nScala:25\nHaskell:20\nElixir:18\nErlang:15",
                /* 42 RadialTree — stesso esempio di Tree */
                "Root: A, B, C\nA: D, E\nB: F\nC: G, H\nD: I\nF: J, K",
                /* 43 AnimatedLine */
                "# Vendite  Costi  Profitto\n0, 10, 8, 2\n1, 14, 9, 5\n"
                "2, 18, 11, 7\n3, 22, 13, 9\n4, 19, 15, 4\n5, 25, 16, 9\n"
                "6, 28, 17, 11\n7, 30, 18, 12",
                /* 44 SmallMultiples */
                "# Nord  Sud  Est  Ovest\n"
                "0, 10, 8, 6, 4\n1, 14, 9, 7, 5\n2, 18, 11, 9, 6\n"
                "3, 22, 13, 11, 7\n4, 19, 15, 13, 8\n5, 25, 16, 14, 9"
            };
            /* ── Esempi e hint per indici 18-39 ── */
            if (idx >= 18 && idx <= 39) {
                static const char* kHints2[] = {
                    /* 18 Column */ "Formato: etichetta:valore  (una voce per riga)",
                    /* 19 HBar   */ "Formato: etichetta:valore  (una voce per riga)",
                    /* 20 Grouped*/ "Formato: x, y1, y2, ... (colonne = serie)\nRiga #: nomi serie",
                    /* 21 Stacked*/ "Formato: x, y1, y2, ... (colonne = serie)\nRiga #: nomi serie",
                    /* 22 Stk100 */ "Formato: x, y1, y2, ... (colonne = serie)\nRiga #: nomi serie",
                    /* 23 Funnel */ "Formato: etichetta:valore  (dalla voce pi\xc3\xb9 grande alla pi\xc3\xb9 piccola)",
                    /* 24 Donut  */ "Formato: etichetta:valore  (una voce per riga)",
                    /* 25 Treemap*/ "Formato: etichetta:valore  (una voce per riga)",
                    /* 26 Sunburst*/ "Formato: Categoria/Sottocategoria:valore\n"
                                    "  Frutta/Mele:40\n  Frutta/Pere:25\n  Verdura/Carote:35",
                    /* 27 BoxPlot*/ "Formato: etichetta: min, Q1, mediana, Q3, max\n"
                                    "  Gruppo A: 10, 20, 30, 40, 60",
                    /* 28 Dot    */ "Formato: etichetta:valore  (una voce per riga)",
                    /* 29 Density*/ "Valori numerici separati da spazi o virgole\n"
                                    "(dati grezzi — la curva KDE viene calcolata automaticamente)",
                    /* 30 AStk   */ "Formato: x, y1, y2, ... (colonne = serie)\nRiga #: nomi serie",
                    /* 31 OHLC   */ "Formato: Etichetta, Open, High, Low, Close\n"
                                    "  Gen, 100, 115, 98, 112",
                    /* 32 Gauge  */ "Riga 1: valore:massimo  (es. 72:100)\nRiga 2: etichetta (opzionale)",
                    /* 33 Bullet */ "Formato: etichetta: valore, target, minRange, maxRange\n"
                                    "  Fatturato: 85, 90, 0, 120",
                    /* 34 Gantt  */ "Formato: task: inizio, fine [, categoria]\n"
                                    "  Analisi: 0, 2, Fase1\n  Design: 1, 4, Fase1",
                    /* 35 Pyramid*/ "Formato: x, sinistra, destra  (colonne = due serie)\n"
                                    "# Maschi  Femmine\n0, 120, 115\n1, 140, 138",
                    /* 36 Parallel*/ "Formato: riga per entit\xc3\xa0, colonne = assi\n"
                                     "Riga #: nomi degli assi\n"
                                     "  # Velocita Forza Resistenza Agilita\n"
                                     "  85, 70, 60, 90",
                    /* 37 Sankey */ "Formato: Sorgente \xe2\x86\x92 Destinazione: valore\n"
                                    "  Energia -> Elettricita: 40\n  Energia -> Calore: 30",
                    /* 38 Tree   */ "Formato: padre: figlio1, figlio2\n"
                                    "  Root: A, B\n  A: C, D\n  B: E",
                    /* 39 Chord  */ "Formato: A \xe2\x86\x92 B: valore\n"
                                    "  Italia -> Francia: 15\n  Francia -> Germania: 20",
                };
                static const char* kExamples2[] = {
                    /* 18 Column */
                    "Gen:82\nFeb:67\nMar:91\nApr:78\nMag:95\nGiu:88",
                    /* 19 HBar */
                    "Python:85\nC++:72\nRust:68\nGo:61\nJava:55\nSwift:48",
                    /* 20 Grouped */
                    "# Nord Sud Est\n0, 42, 28, 19\n1, 55, 33, 22\n2, 48, 30, 25\n3, 60, 38, 28",
                    /* 21 Stacked */
                    "# Carbone Petrolio Gas Rinnovabili\n2019, 28, 33, 22, 17\n2020, 25, 30, 23, 22\n2021, 26, 31, 22, 21\n2022, 24, 29, 21, 26",
                    /* 22 Stacked 100% */
                    "# Desktop Mobile Tablet\n2020, 55, 35, 10\n2021, 50, 40, 10\n2022, 46, 44, 10\n2023, 43, 47, 10",
                    /* 23 Funnel */
                    "Visite:10000\nIscritti:4200\nContatti:1800\nOfferte:750\nClienti:320",
                    /* 24 Donut */
                    "Python:38\nJavaScript:26\nTypeScript:14\nJava:11\nC++:7\nAltri:4",
                    /* 25 Treemap */
                    "Europa:420\nAsia:580\nAmerica:390\nAfrica:180\nOceania:95",
                    /* 26 Sunburst */
                    "Frutta/Mele:40\nFrutta/Pere:25\nFrutta/Arance:20\n"
                    "Verdura/Carote:35\nVerdura/Spinaci:28\nVerdura/Pomodori:32\n"
                    "Cereali/Riso:55\nCereali/Frumento:45",
                    /* 27 BoxPlot */
                    "Gruppo A: 10, 22, 35, 48, 65\n"
                    "Gruppo B: 8, 18, 28, 42, 58\n"
                    "Gruppo C: 15, 28, 40, 55, 72\n"
                    "Gruppo D: 5, 15, 25, 38, 52",
                    /* 28 DotPlot */
                    "Lun:8.2\nMar:7.5\nMer:9.1\nGio:6.8\nVen:8.8\nSab:5.2\nDom:4.5",
                    /* 29 Density */
                    "2.1 3.4 2.8 4.1 3.7 2.5 3.9 4.5 3.2 2.7 "
                    "4.8 3.1 2.9 3.6 4.2 3.8 2.3 3.5 4.0 3.3 "
                    "2.6 4.4 3.0 2.4 3.8 4.1 2.8 3.7 4.3 3.2",
                    /* 30 AreaStacked */
                    "# Vendite Costi Profitto\n0, 120, 80, 40\n1, 145, 90, 55\n"
                    "2, 160, 95, 65\n3, 175, 100, 75\n4, 155, 105, 50\n5, 190, 110, 80",
                    /* 31 OHLC */
                    "Gen, 100, 115, 98, 112\nFeb, 112, 120, 108, 105\n"
                    "Mar, 105, 125, 102, 118\nApr, 118, 130, 115, 125\n"
                    "Mag, 125, 128, 110, 108\nGiu, 108, 122, 105, 118",
                    /* 32 Gauge */
                    "72:100\nPunteggio qualit\xc3\xa0",
                    /* 33 Bullet */
                    "Fatturato: 85, 90, 60, 120\n"
                    "Profitto: 42, 50, 30, 70\n"
                    "NPS: 68, 75, 40, 100",
                    /* 34 Gantt */
                    "Analisi: 0, 2, Fase 1\n"
                    "Design: 1, 4, Fase 1\n"
                    "Sviluppo: 3, 8, Fase 2\n"
                    "Testing: 7, 10, Fase 2\n"
                    "Deploy: 9, 11, Fase 3",
                    /* 35 Pyramid */
                    "# Maschi Femmine\n0, 850, 820\n1, 920, 900\n"
                    "2, 980, 960\n3, 870, 850\n4, 720, 710\n5, 540, 560",
                    /* 36 ParallelCoord */
                    "# Velocita Forza Resistenza Agilita Precisione\n"
                    "85, 70, 60, 90, 78\n"
                    "62, 88, 75, 55, 82\n"
                    "90, 55, 80, 72, 65\n"
                    "70, 80, 85, 68, 90",
                    /* 37 Sankey */
                    "Carbone -> Elettricita: 40\n"
                    "Gas -> Elettricita: 25\n"
                    "Elettricita -> Industria: 35\n"
                    "Elettricita -> Residenziale: 20\n"
                    "Gas -> Riscaldamento: 30\n"
                    "Rinnovabili -> Elettricita: 18",
                    /* 38 Tree */
                    "Azienda: HR, Tech, Vendite\n"
                    "Tech: Backend, Frontend, DevOps\n"
                    "Backend: API, Database\n"
                    "Vendite: Italia, Estero",
                    /* 39 Chord */
                    "Italia -> Francia: 15\n"
                    "Italia -> Germania: 22\n"
                    "Francia -> Germania: 18\n"
                    "Germania -> Spagna: 12\n"
                    "Spagna -> Italia: 20\n"
                    "Francia -> Spagna: 14",
                };
                m_dataHint->setText(kHints2[idx - 18]);
                if (kExamples2[idx - 18][0] != '\0')
                    m_dataEdit->setPlainText(kExamples2[idx - 18]);
            }

            if (idx >= 40 && idx <= 44) {
                m_dataHint->setText(kNewHints[idx - 40]);
                if (kNewExamples[idx-40][0] != '\0')
                    m_dataEdit->setPlainText(kNewExamples[idx-40]);
            }
            /* Generatore da espressione: nascosto per tipi complessi */
            const bool hasFormula = (idx != 4 && idx != 6 &&
                                     idx != 13 && idx != 14 && idx != 16 &&
                                     idx < 18 && idx < 40);
            m_genSection->setVisible(hasFormula);
        }
    });

    /* Pulsanti */
    auto* btnPlot = new QPushButton("\xf0\x9f\x93\x88  Traccia", panel);
    btnPlot->setObjectName("primaryBtn");
    btnPlot->setFixedHeight(34);
    connect(btnPlot, &QPushButton::clicked, this, &GraficoPage::plot);
    lay->addWidget(btnPlot);

    auto* btnReset = new QPushButton("\xf0\x9f\x94\x84  Reset vista", panel);
    btnReset->setObjectName("actionBtn");
    btnReset->setFixedHeight(28);
    connect(btnReset, &QPushButton::clicked, this, [this]{ m_canvas->resetView(); });
    lay->addWidget(btnReset);

    /* ── Sezione immagine → formula (solo Cartesiano) ─────────── */
    auto* imgSep = new QFrame(panel);
    imgSep->setFrameShape(QFrame::HLine);
    imgSep->setObjectName("separator");
    lay->addWidget(imgSep);

    m_imgSection = new QWidget(panel);
    auto* imgLay = new QVBoxLayout(m_imgSection);
    imgLay->setContentsMargins(0, 2, 0, 2);
    imgLay->setSpacing(4);

    auto* imgTitleLbl = new QLabel(
        "\xf0\x9f\x94\x8d  Grafico \xe2\x86\x92 Formula  [vision AI]", m_imgSection);
    imgTitleLbl->setObjectName("formLabel");
    imgLay->addWidget(imgTitleLbl);

    auto* imgHintLbl = new QLabel(
        "Carica un grafico cartesiano.\n"
        "Il modello vision estrae la formula y = f(x).", m_imgSection);
    imgHintLbl->setObjectName("hintLabel");
    imgHintLbl->setWordWrap(true);
    imgHintLbl->setFont(QFont("Inter,Ubuntu,sans-serif", 8));
    imgLay->addWidget(imgHintLbl);

    m_visionCombo = new QComboBox(m_imgSection);
    m_visionCombo->setObjectName("settingCombo");
    m_visionCombo->setToolTip(
        "Modello vision da usare (solo quelli con supporto immagini).\n"
        "Es: llama3.2-vision, llava, minicpm-v, moondream");
    m_visionCombo->setPlaceholderText("Seleziona modello vision...");
    imgLay->addWidget(m_visionCombo);

    /* Pulsante di installazione — visibile SOLO se non ci sono modelli vision */
    auto* btnInstallVision = new QPushButton(
        "\xf0\x9f\x93\xa5  Clicca qui per scaricare e installare un modello vision da internet",
        m_imgSection);
    btnInstallVision->setObjectName("actionBtn");
    btnInstallVision->setFixedHeight(36);
    btnInstallVision->setToolTip(
        "Apre Impostazioni \xe2\x86\x92 LLM Consigliati \xe2\x86\x92 sezione Vision\n"
        "per scaricare un modello adatto all'analisi di immagini e grafici.\n"
        "Consigliati: llama3.2-vision, qwen2-vl:7b, minicpm-v:8b");
    btnInstallVision->setStyleSheet(
        "QPushButton { background:#1e3a5f; border:1px dashed #38bdf8;"
        "border-radius:6px; color:#93c5fd; padding:4px 8px; text-align:left; }"
        "QPushButton:hover { background:#1e4a7f; border-color:#60a5fa; }");
    btnInstallVision->setVisible(false);
    connect(btnInstallVision, &QPushButton::clicked, this,
            [this]{ emit requestOpenSettings("LLM"); });
    imgLay->addWidget(btnInstallVision);

    m_btnImgFormula = new QPushButton(
        "\xf0\x9f\x93\xb7  Analizza immagine", m_imgSection);
    m_btnImgFormula->setObjectName("actionBtn");
    m_btnImgFormula->setFixedHeight(30);
    m_btnImgFormula->setToolTip("Apri un'immagine PNG/JPG e lascia che l'AI estragga la formula");
    connect(m_btnImgFormula, &QPushButton::clicked, this, &GraficoPage::analyzeImage);
    imgLay->addWidget(m_btnImgFormula);

    lay->addWidget(m_imgSection);

    /* Popola la combo vision la prima volta che i modelli sono disponibili */
    if (m_ai) {
        auto populateVision = [this, btnInstallVision]() {
            const QStringList all = m_ai->models();
            QStringList vision;
            for (const QString& m : all) {
                const QString ml = m.toLower();
                if (ml.contains("vision") || ml.contains("llava")  ||
                    ml.contains("minicpm") || ml.contains("moondream") ||
                    ml.contains("bakllava") || ml.contains("gemma3"))
                    vision << m;
            }
            m_visionCombo->clear();
            if (vision.isEmpty()) {
                /* Nessun modello vision: nascondi combo e mostra pulsante installazione */
                m_visionCombo->setVisible(false);
                btnInstallVision->setVisible(true);
                m_btnImgFormula->setEnabled(false);
            } else {
                m_visionCombo->setVisible(true);
                btnInstallVision->setVisible(false);
                m_visionCombo->addItems(vision);
                m_btnImgFormula->setEnabled(true);
            }
        };
        /* Popola subito se i modelli ci sono già, oppure alla prima ricezione */
        if (!m_ai->models().isEmpty())
            populateVision();
        connect(m_ai, &AiClient::modelsReady, m_imgSection,
                [populateVision](const QStringList&){ populateVision(); });
    }

    /* Visibilità: solo quando tipo = Cartesiano */
    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            m_imgSection, [imgSep, this](int idx){
        const bool cart = (idx == 0);   /* sezione vision solo per Cartesiano */
        imgSep->setVisible(cart);
        m_imgSection->setVisible(cart);
    });

    m_statusLbl = new QLabel("", panel);
    m_statusLbl->setObjectName("statusLabel");
    m_statusLbl->setWordWrap(true);
    m_statusLbl->setFont(QFont("Inter,Ubuntu,sans-serif", 8));
    lay->addWidget(m_statusLbl);

    return panel;
}

/* ── plot() ──────────────────────────────────────────────────── */
void GraficoPage::plot() {
    int type = m_typeCombo->currentIndex();
    m_statusLbl->clear();

    switch (type) {
        case 0: {   /* Cartesiano */
            QString f = m_formulaEdit->text().trimmed();
            if (f.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Inserisci una formula"); return; }
            double xMin = m_xMinSpin->value(), xMax = m_xMaxSpin->value();
            if (xMin >= xMax)  { m_statusLbl->setText("\xe2\x9a\xa0  x min deve essere < x max"); return; }
            m_canvas->setCartesian(f, xMin, xMax);
            FormulaParser fp(f);
            if (!fp.ok()) m_statusLbl->setText("\xe2\x9a\xa0  " + fp.err());
            else          m_statusLbl->setText("\xe2\x9c\x85  Grafico aggiornato");
            break;
        }
        case 1:
        case 2: {   /* Torta / Istogramma */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<double> vals; QStringList lbls;
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                QString l = line.trimmed(); if (l.isEmpty()) continue;
                QChar sep = l.contains(':') ? ':' : ',';
                int pos = l.indexOf(sep);
                if (pos > 0) {
                    bool ok; double v = l.mid(pos+1).trimmed().toDouble(&ok);
                    if (ok) { lbls.append(l.left(pos).trimmed()); vals.append(v); }
                } else {
                    bool ok; double v = l.toDouble(&ok);
                    if (ok) { lbls.append(QString::number(vals.size()+1)); vals.append(v); }
                }
            }
            if (vals.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun valore valido"); return; }
            m_canvas->setData(vals, lbls);
            m_canvas->setType(type == 1 ? GraficoCanvas::Pie : GraficoCanvas::Histogram);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(vals.size()) + " voci");
            break;
        }
        case 3: {   /* Scatter XY */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<QPointF> pts; QStringList lbls;
            static QRegularExpression sep("[,;\\s]+");
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                /* formato: x, y [, etichetta]
                   L'etichetta può essere un testo qualsiasi (anche con spazi) */
                QString trimmed = line.trimmed();
                QStringList p   = trimmed.split(sep, Qt::SkipEmptyParts);
                if (p.size() >= 2) {
                    bool ox, oy;
                    double x = p[0].toDouble(&ox), y = p[1].toDouble(&oy);
                    if (ox && oy) {
                        pts.append({x, y});
                        /* la terza colonna (e successive) è l'etichetta */
                        lbls.append(p.size() > 2 ? p.mid(2).join(" ") : "");
                    }
                }
            }
            if (pts.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessuna coppia x,y valida"); return; }
            m_canvas->setScatter(pts);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(pts.size()) + " punti");
            break;
        }
        case 4: {   /* Grafo */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun arco"); return; }
            QVector<QPair<QString,QString>> edges;
            static QRegularExpression edgeSep("\\s*[-\xe2\x80\x93>]+\\s*");
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                QStringList p = line.trimmed().split(edgeSep, Qt::SkipEmptyParts);
                if (p.size() >= 2) edges.append({p[0].trimmed(), p[1].trimmed()});
            }
            if (edges.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun arco valido (A-B)"); return; }
            m_canvas->setEdges(edges);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(edges.size()) + " archi");
            break;
        }
        case 5: {   /* 3D Scatter */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<GraficoCanvas::Pt3D> pts;
            static QRegularExpression sep3("[,;\\s]+");
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                /* formato: x, y, z [, etichetta] */
                QStringList p = line.trimmed().split(sep3, Qt::SkipEmptyParts);
                if (p.size() >= 3) {
                    bool ox, oy, oz;
                    double x = p[0].toDouble(&ox), y = p[1].toDouble(&oy), z = p[2].toDouble(&oz);
                    if (ox && oy && oz) {
                        QString lbl = p.size() > 3 ? p.mid(3).join(" ") : "";
                        pts.append({x, y, z, lbl});
                    }
                }
            }
            if (pts.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessuna tripla x,y,z valida"); return; }
            m_canvas->setScatter3D(pts);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(pts.size()) + " punti 3D");
            break;
        }
        case 7: {   /* Smith Prime */
            m_canvas->setSmithPrime(
                m_smithMaxN->value(),
                m_smithReal->isChecked(),
                m_smithGauss->isChecked(),
                m_smithExp->isChecked(),
                m_smithNorm->value(),
                m_smithFib->isChecked(),
                m_smithTri->isChecked(),
                m_smithSqr->isChecked(),
                m_smithLabels->isChecked()
            );
            /* ── Serie personalizzata: parsa i numeri dal text edit ── */
            {
                QString raw = m_smithCustom->toPlainText();
                if (!raw.trimmed().isEmpty()) {
                    QVector<int> customVals;
                    static QRegularExpression sepC("[,;\\s]+");
                    for (const QString& tok : raw.split(sepC, Qt::SkipEmptyParts)) {
                        bool ok;
                        int v = tok.toInt(&ok);
                        if (ok && v >= 2) customVals.append(v);
                    }
                    if (!customVals.isEmpty())
                        m_canvas->addSmithCustom(customVals);
                }
            }
            int nPts = m_canvas->smithPointCount();
            m_statusLbl->setText(
                QString("\xe2\x9c\x85  %1 punti nel disco di Smith").arg(nPts));
            break;
        }
        case 8: {   /* Math Const — π · e · Primi */
            if (!m_mathPi->isChecked() && !m_mathE->isChecked() && !m_mathPrimes->isChecked()) {
                m_statusLbl->setText("\xe2\x9a\xa0  Seleziona almeno una serie");
                return;
            }
            m_canvas->setMathConst(
                m_mathTerms ->value(),
                m_mathPi    ->isChecked(),
                m_mathE     ->isChecked(),
                m_mathPrimes->isChecked()
            );
            int nPts = m_canvas->mathPointCount();
            m_statusLbl->setText(
                QString("\xe2\x9c\x85  %1 punti  |  "
                        "\xce\x93(\xcf\x80)\xe2\x89\x88" "0.517  \xce\x93(e)\xe2\x89\x88" "0.462").arg(nPts));
            break;
        }
        case 6: {   /* Grafo 3D — righe "Nome, x, y, z" + "A-B" mescolate */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<GraficoCanvas::Node3D>          nodes;
            QVector<QPair<QString,QString>>          edges;
            static QRegularExpression sepN("[,;\\s]+");
            static QRegularExpression sepE("\\s*[-\xe2\x80\x93>]+\\s*");
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                QString l = line.trimmed();
                if (l.startsWith('#')) continue;           /* commento */
                QStringList tok = l.split(sepN, Qt::SkipEmptyParts);
                /* nodo: almeno 4 token, token[1..3] sono numeri */
                if (tok.size() >= 4) {
                    bool ox, oy, oz;
                    double x = tok[1].toDouble(&ox);
                    double y = tok[2].toDouble(&oy);
                    double z = tok[3].toDouble(&oz);
                    if (ox && oy && oz) {
                        nodes.append({tok[0], x, y, z});
                        continue;
                    }
                }
                /* arco: A-B */
                QStringList parts = l.split(sepE, Qt::SkipEmptyParts);
                if (parts.size() >= 2)
                    edges.append({parts[0].trimmed(), parts[1].trimmed()});
            }
            if (nodes.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun nodo (formato: Nome, x, y, z)"); return; }
            m_canvas->setGraph3D(nodes, edges);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(nodes.size()) +
                                  " nodi, " + QString::number(edges.size()) + " archi");
            break;
        }
        case 9:   /* Linea multi-serie */
        case 15:  /* Area riempita  */
        case 17: { /* Scalini (Step) */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<QVector<QPointF>> series;
            QStringList names;
            static QRegularExpression sepMS("[,;\\s]+");
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                QString l = line.trimmed();
                if (l.startsWith('#')) {
                    /* riga header: # NomeSerie1 NomeSerie2 ... */
                    names = l.mid(1).trimmed().split(sepMS, Qt::SkipEmptyParts);
                    continue;
                }
                QStringList tok = l.split(sepMS, Qt::SkipEmptyParts);
                if (tok.size() < 2) continue;
                bool ox; double x = tok[0].toDouble(&ox);
                if (!ox) continue;
                /* assicura che ci siano abbastanza serie */
                while (series.size() < tok.size() - 1) series.append(QVector<QPointF>());
                for (int k = 1; k < tok.size(); ++k) {
                    bool ok; double y = tok[k].toDouble(&ok);
                    if (ok) series[k-1].append({x, y});
                }
            }
            if (series.isEmpty()) {
                m_statusLbl->setText("\xe2\x9a\xa0  Nessuna colonna x,y valida");
                return;
            }
            m_canvas->setLine(series, names);
            if (type == 15) m_canvas->setType(GraficoCanvas::Area);
            else if (type == 17) m_canvas->setType(GraficoCanvas::Step);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(series.size()) + " serie, " +
                                  QString::number(series[0].size()) + " punti");
            break;
        }
        case 10: { /* Polare r = f(θ) */
            if (!m_polarFormula) { m_statusLbl->setText("\xe2\x9a\xa0  Pannello non pronto"); return; }
            QString f = m_polarFormula->text().trimmed();
            if (f.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Inserisci r = f(\xce\xb8)"); return; }
            double tMin = m_polarTMinSpin->value();
            double tMax = m_polarTMaxSpin->value();
            if (tMin >= tMax) { m_statusLbl->setText("\xe2\x9a\xa0  \xce\xb8 min deve essere < \xce\xb8 max"); return; }
            m_canvas->setPolar(f, tMin, tMax);
            m_statusLbl->setText("\xe2\x9c\x85  Grafico polare aggiornato");
            break;
        }
        case 11: { /* Radar (Spider) — riusa parsing Torta/Istogramma */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<double> vals; QStringList lbls;
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                QString l = line.trimmed(); if (l.isEmpty()) continue;
                QChar sep = l.contains(':') ? ':' : ',';
                int pos = l.indexOf(sep);
                if (pos > 0) {
                    bool ok; double v = l.mid(pos+1).trimmed().toDouble(&ok);
                    if (ok) { lbls.append(l.left(pos).trimmed()); vals.append(v); }
                } else {
                    bool ok; double v = l.toDouble(&ok);
                    if (ok) { lbls.append(QString::number(vals.size()+1)); vals.append(v); }
                }
            }
            if (vals.size() < 3) {
                m_statusLbl->setText("\xe2\x9a\xa0  Servono almeno 3 categorie");
                return;
            }
            m_canvas->setData(vals, lbls);
            m_canvas->setType(GraficoCanvas::Radar);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(vals.size()) + " assi");
            break;
        }
        case 12: { /* Bolle: x, y, raggio [, etichetta] */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<GraficoCanvas::Pt3D> pts;
            static QRegularExpression sepB("[,;\\s]+");
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                QStringList tok = line.trimmed().split(sepB, Qt::SkipEmptyParts);
                if (tok.size() < 3) continue;
                bool ox, oy, oz;
                double x = tok[0].toDouble(&ox);
                double y = tok[1].toDouble(&oy);
                double r = tok[2].toDouble(&oz);
                if (ox && oy && oz)
                    pts.append({x, y, r, tok.size() > 3 ? tok.mid(3).join(" ") : ""});
            }
            if (pts.isEmpty()) {
                m_statusLbl->setText("\xe2\x9a\xa0  Nessuna terna x,y,raggio valida");
                return;
            }
            m_canvas->setScatter3D(pts);
            m_canvas->setType(GraficoCanvas::Bubble);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(pts.size()) + " bolle");
            break;
        }
        case 13: { /* Heatmap */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<QVector<double>> data;
            QStringList rowLbls, colLbls;
            static QRegularExpression sepH("[,;\\s]+");
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                QString l = line.trimmed();
                if (l.startsWith('#')) {
                    colLbls = l.mid(1).trimmed().split(sepH, Qt::SkipEmptyParts);
                    continue;
                }
                /* supporto opzionale "etichetta: v1 v2 v3" */
                QStringList tok;
                int colon = l.indexOf(':');
                if (colon > 0 && colon < 20) {
                    rowLbls.append(l.left(colon).trimmed());
                    tok = l.mid(colon+1).trimmed().split(sepH, Qt::SkipEmptyParts);
                } else {
                    tok = l.split(sepH, Qt::SkipEmptyParts);
                }
                QVector<double> row;
                for (auto& t : tok) { bool ok; double v = t.toDouble(&ok); if (ok) row.append(v); }
                if (!row.isEmpty()) data.append(row);
            }
            if (data.isEmpty()) {
                m_statusLbl->setText("\xe2\x9a\xa0  Nessun valore numerico trovato");
                return;
            }
            m_canvas->setHeatmap(data, rowLbls, colLbls);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(data.size()) + " \xc3\x97 " +
                                  QString::number(data[0].size()) + " matrice");
            break;
        }
        case 14: { /* Candlestick OHLC */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<GraficoCanvas::OhlcPt> pts;
            static QRegularExpression sepC("[,;]+");
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                QStringList tok = line.trimmed().split(sepC);
                if (tok.size() < 5) continue;
                bool oo, oh, ol, oc;
                double o = tok[1].trimmed().toDouble(&oo);
                double h = tok[2].trimmed().toDouble(&oh);
                double l = tok[3].trimmed().toDouble(&ol);
                double c = tok[4].trimmed().toDouble(&oc);
                if (oo && oh && ol && oc)
                    pts.append({o, h, l, c, tok[0].trimmed()});
            }
            if (pts.isEmpty()) {
                m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato OHLC valido (formato: Label,O,H,L,C)");
                return;
            }
            m_canvas->setCandlestick(pts);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(pts.size()) + " candele");
            break;
        }
        case 16: { /* Waterfall */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<double> vals; QStringList lbls;
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                QString l = line.trimmed(); if (l.isEmpty()) continue;
                QChar sep = l.contains(':') ? ':' : ',';
                int pos = l.indexOf(sep);
                if (pos > 0) {
                    /* rimuovi eventuale '+' dal valore */
                    QString vStr = l.mid(pos+1).trimmed();
                    if (vStr.startsWith('+')) vStr = vStr.mid(1);
                    bool ok; double v = vStr.toDouble(&ok);
                    if (ok) { lbls.append(l.left(pos).trimmed()); vals.append(v); }
                } else {
                    bool ok; double v = l.toDouble(&ok);
                    if (ok) { lbls.append(QString::number(vals.size()+1)); vals.append(v); }
                }
            }
            if (vals.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun valore valido"); return; }
            m_canvas->setData(vals, lbls);
            m_canvas->setType(GraficoCanvas::Waterfall);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(vals.size()) + " voci");
            break;
        }
        /* ── Column / HBar / Funnel / Donut / Treemap / DotPlot / Gauge ── */
        case 18: /* Column    */
        case 19: /* HBar      */
        case 23: /* Funnel    */
        case 24: /* Donut     */
        case 25: /* Treemap   */
        case 28: /* DotPlot   */
        {
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<double> vals; QStringList lbls;
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                QString l = line.trimmed(); if (l.startsWith('#') || l.isEmpty()) continue;
                QChar sep = l.contains(':') ? ':' : ',';
                int pos = l.indexOf(sep);
                if (pos > 0) {
                    bool ok; double v = l.mid(pos+1).trimmed().toDouble(&ok);
                    if (ok) { lbls.append(l.left(pos).trimmed()); vals.append(v); }
                } else {
                    bool ok; double v = l.toDouble(&ok);
                    if (ok) { lbls.append(QString::number(vals.size()+1)); vals.append(v); }
                }
            }
            if (vals.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun valore valido"); return; }
            m_canvas->setData(vals, lbls);
            static const GraficoCanvas::ChartType kT[] = {
                GraficoCanvas::Column, GraficoCanvas::HBar,
                GraficoCanvas::Funnel, GraficoCanvas::Donut,
                GraficoCanvas::Treemap, GraficoCanvas::DotPlot
            };
            const int kMap[] = {18, 19, 23, 24, 25, 28};
            GraficoCanvas::ChartType ct = GraficoCanvas::Column;
            for (int i = 0; i < 6; ++i) if (kMap[i] == type) { ct = kT[i]; break; }
            m_canvas->setType(ct);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(vals.size()) + " voci");
            break;
        }
        /* ── GroupedBar / StackedBar / StackedBar100 / Pyramid / AreaStacked ── */
        case 20: /* GroupedBar    */
        case 21: /* StackedBar    */
        case 22: /* StackedBar100 */
        case 30: /* AreaStacked   */
        case 35: /* Pyramid       */
        {
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<QVector<QPointF>> series;
            QStringList names;
            static QRegularExpression sepGr("[,;\\s]+");
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                QString l = line.trimmed();
                if (l.startsWith('#')) {
                    names = l.mid(1).trimmed().split(sepGr, Qt::SkipEmptyParts);
                    continue;
                }
                QStringList tok = l.split(sepGr, Qt::SkipEmptyParts);
                if (tok.size() < 2) continue;
                bool ox; double x = tok[0].toDouble(&ox);
                if (!ox) {
                    /* etichetta testuale: usa indice */
                    x = series.isEmpty() ? 0 : (series[0].isEmpty() ? 0 : series[0].size());
                    /* riprova dal primo token come label, skip */
                }
                while (series.size() < tok.size() - 1) series.append(QVector<QPointF>());
                for (int k = 1; k < tok.size(); ++k) {
                    bool ok; double y = tok[k].toDouble(&ok);
                    if (ok) series[k-1].append({x, y});
                }
            }
            if (series.isEmpty()) {
                m_statusLbl->setText("\xe2\x9a\xa0  Nessuna serie valida");
                return;
            }
            m_canvas->setLine(series, names);
            static const int kGrMap[]  = {20, 21, 22, 30, 35};
            static const GraficoCanvas::ChartType kGrT[] = {
                GraficoCanvas::GroupedBar, GraficoCanvas::StackedBar,
                GraficoCanvas::StackedBar100, GraficoCanvas::AreaStacked,
                GraficoCanvas::Pyramid
            };
            GraficoCanvas::ChartType ct = GraficoCanvas::GroupedBar;
            for (int i = 0; i < 5; ++i) if (kGrMap[i] == type) { ct = kGrT[i]; break; }
            m_canvas->setType(ct);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(series.size()) + " serie");
            break;
        }
        case 26: { /* Sunburst — Categoria/Subcategoria: valore */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<QPair<QString,double>> data;
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                QString l = line.trimmed();
                if (l.startsWith('#') || l.isEmpty()) continue;
                int pos = l.lastIndexOf(':');
                if (pos > 0) {
                    bool ok; double v = l.mid(pos+1).trimmed().toDouble(&ok);
                    if (ok) data.append({l.left(pos).trimmed(), v});
                }
            }
            if (data.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato valido"); return; }
            m_canvas->setSunburstData(data);
            m_canvas->setType(GraficoCanvas::Sunburst);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(data.size()) + " voci");
            break;
        }
        case 27: { /* BoxPlot */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<GraficoCanvas::BoxData> data;
            static QRegularExpression sepBx("[,;\\s]+");
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                QString l = line.trimmed();
                if (l.startsWith('#') || l.isEmpty()) continue;
                QString lbl; QString valPart = l;
                int colon = l.indexOf(':');
                if (colon > 0) { lbl = l.left(colon).trimmed(); valPart = l.mid(colon+1); }
                QStringList tok = valPart.split(sepBx, Qt::SkipEmptyParts);
                if (tok.size() < 5) continue;
                bool o1,o2,o3,o4,o5;
                double mn = tok[0].toDouble(&o1), q1 = tok[1].toDouble(&o2),
                       md = tok[2].toDouble(&o3), q3 = tok[3].toDouble(&o4),
                       mx = tok[4].toDouble(&o5);
                if (o1&&o2&&o3&&o4&&o5)
                    data.append({mn, q1, md, q3, mx, lbl.isEmpty() ? QString::number(data.size()+1) : lbl});
            }
            if (data.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Formato: label: min,Q1,med,Q3,max"); return; }
            m_canvas->setBoxData(data);
            m_canvas->setType(GraficoCanvas::BoxPlot);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(data.size()) + " box");
            break;
        }
        case 29: { /* Density — valori grezzi */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<double> data;
            static QRegularExpression sepDs("[,;\\s\n]+");
            for (auto& tok : raw.split(sepDs, Qt::SkipEmptyParts)) {
                bool ok; double v = tok.trimmed().toDouble(&ok);
                if (ok) data.append(v);
            }
            if (data.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun valore numerico"); return; }
            m_canvas->setDensityData(data);
            m_canvas->setType(GraficoCanvas::Density);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(data.size()) + " campioni");
            break;
        }
        case 31: { /* OHLC — stessa struttura Candlestick */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<GraficoCanvas::OhlcPt> pts;
            static QRegularExpression sepOh("[,;]+");
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                QStringList tok = line.trimmed().split(sepOh);
                if (tok.size() < 5) continue;
                bool oo,oh,ol,oc;
                double o = tok[1].trimmed().toDouble(&oo), h = tok[2].trimmed().toDouble(&oh),
                       l = tok[3].trimmed().toDouble(&ol), c = tok[4].trimmed().toDouble(&oc);
                if (oo&&oh&&ol&&oc) pts.append({o, h, l, c, tok[0].trimmed()});
            }
            if (pts.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Formato: Label,O,H,L,C"); return; }
            m_canvas->setCandlestick(pts);
            m_canvas->setType(GraficoCanvas::OHLC);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(pts.size()) + " barre OHLC");
            break;
        }
        case 32: { /* Gauge — valore:massimo su riga 1, etichetta opzionale riga 2 */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QStringList lines = raw.split('\n', Qt::SkipEmptyParts);
            QVector<double> vals; QStringList lbls;
            QString line0 = lines[0].trimmed();
            QChar sep0 = line0.contains(':') ? ':' : ',';
            QStringList p0 = line0.split(sep0);
            bool ok0, ok1;
            double v0 = p0[0].trimmed().toDouble(&ok0);
            double v1 = p0.size() > 1 ? p0[1].trimmed().toDouble(&ok1) : (ok1=true, 100.0);
            if (!ok0 || !ok1) { m_statusLbl->setText("\xe2\x9a\xa0  Formato: valore:massimo"); return; }
            vals.append(v0); vals.append(v1);
            lbls.append(lines.size() > 1 ? lines[1].trimmed() : "");
            m_canvas->setData(vals, lbls);
            m_canvas->setType(GraficoCanvas::Gauge);
            m_statusLbl->setText("\xe2\x9c\x85  Gauge: " + QString::number(v0) + " / " + QString::number(v1));
            break;
        }
        case 33: { /* Bullet Chart */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<GraficoCanvas::BulletBar> data;
            static QRegularExpression sepBu("[,;]+");
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                QString l = line.trimmed();
                if (l.startsWith('#') || l.isEmpty()) continue;
                int colon = l.indexOf(':');
                QString lbl = colon > 0 ? l.left(colon).trimmed() : "";
                QString rest = colon > 0 ? l.mid(colon+1) : l;
                QStringList tok = rest.split(sepBu, Qt::SkipEmptyParts);
                if (tok.size() < 4) continue;
                bool o1,o2,o3,o4;
                double val = tok[0].trimmed().toDouble(&o1), tgt = tok[1].trimmed().toDouble(&o2),
                       rlo = tok[2].trimmed().toDouble(&o3), rhi = tok[3].trimmed().toDouble(&o4);
                if (o1&&o2&&o3&&o4)
                    data.append({lbl.isEmpty() ? QString::number(data.size()+1) : lbl, val, tgt, rlo, rhi});
            }
            if (data.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Formato: label: valore, target, min, max"); return; }
            m_canvas->setBulletData(data);
            m_canvas->setType(GraficoCanvas::Bullet);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(data.size()) + " barre bullet");
            break;
        }
        case 34: { /* Gantt */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<GraficoCanvas::GanttTask> data;
            static QRegularExpression sepGa("[,;]+");
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                QString l = line.trimmed();
                if (l.startsWith('#') || l.isEmpty()) continue;
                int colon = l.indexOf(':');
                QString name = colon > 0 ? l.left(colon).trimmed() : l;
                QString rest = colon > 0 ? l.mid(colon+1) : "";
                QStringList tok = rest.split(sepGa, Qt::SkipEmptyParts);
                if (tok.size() < 2) continue;
                bool os, oe;
                double s = tok[0].trimmed().toDouble(&os), e = tok[1].trimmed().toDouble(&oe);
                if (os && oe) {
                    QString cat = tok.size() > 2 ? tok[2].trimmed() : "";
                    data.append({name, cat, s, e});
                }
            }
            if (data.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Formato: task: inizio, fine [, categoria]"); return; }
            m_canvas->setGanttData(data);
            m_canvas->setType(GraficoCanvas::Gantt);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(data.size()) + " task");
            break;
        }
        case 36: { /* ParallelCoord — come multi-serie */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<QVector<QPointF>> series;
            QStringList names;
            static QRegularExpression sepPC("[,;\\s]+");
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                QString l = line.trimmed();
                if (l.startsWith('#')) {
                    names = l.mid(1).trimmed().split(sepPC, Qt::SkipEmptyParts);
                    continue;
                }
                QStringList tok = l.split(sepPC, Qt::SkipEmptyParts);
                if (tok.isEmpty()) continue;
                /* ogni riga = una "serie" (entità), colonne = assi */
                QVector<QPointF> row;
                for (int k = 0; k < tok.size(); ++k) {
                    bool ok; double v = tok[k].toDouble(&ok);
                    if (ok) row.append({(double)k, v});
                }
                if (!row.isEmpty()) series.append(row);
            }
            if (series.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessuna riga valida"); return; }
            m_canvas->setLine(series, names);
            m_canvas->setType(GraficoCanvas::ParallelCoord);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(series.size()) + " entit\xc3\xa0");
            break;
        }
        case 37: { /* Sankey */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<GraficoCanvas::SankeyLink> data;
            static QRegularExpression arrowSep("\\s*[\xe2\x86\x92>]+\\s*|\\s*->\\s*");
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                QString l = line.trimmed();
                if (l.startsWith('#') || l.isEmpty()) continue;
                /* formato: "sorgente → destinazione: valore" */
                int cpos = l.lastIndexOf(':');
                QString linkPart = cpos > 0 ? l.left(cpos) : l;
                double val = 1.0;
                if (cpos > 0) { bool ok; double v = l.mid(cpos+1).trimmed().toDouble(&ok); if (ok) val = v; }
                QStringList parts = linkPart.split(arrowSep, Qt::SkipEmptyParts);
                if (parts.size() < 2) {
                    /* prova con spazio + parola chiave */
                    parts = linkPart.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
                }
                if (parts.size() >= 2)
                    data.append({parts[0].trimmed(), parts[parts.size()-1].trimmed(), val});
            }
            if (data.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Formato: A \xe2\x86\x92 B: valore"); return; }
            m_canvas->setSankeyData(data);
            m_canvas->setType(GraficoCanvas::Sankey);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(data.size()) + " flussi");
            break;
        }
        case 38: { /* Tree */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<GraficoCanvas::TreeEdge> data;
            static QRegularExpression sepTr("[,;]+");
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                QString l = line.trimmed();
                if (l.startsWith('#') || l.isEmpty()) continue;
                int colon = l.indexOf(':');
                if (colon <= 0) continue;
                QString parent = l.left(colon).trimmed();
                QStringList children = l.mid(colon+1).split(sepTr, Qt::SkipEmptyParts);
                for (auto& ch : children) {
                    QString c = ch.trimmed();
                    if (!c.isEmpty()) data.append({parent, c});
                }
            }
            if (data.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Formato: padre: figlio1, figlio2"); return; }
            m_canvas->setTreeData(data);
            m_canvas->setType(GraficoCanvas::Tree);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(data.size()) + " archi");
            break;
        }
        case 39: { /* Chord */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<GraficoCanvas::ChordLink> data;
            static QRegularExpression arrowCh("\\s*[\xe2\x86\x92>]+\\s*|\\s*->\\s*");
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                QString l = line.trimmed();
                if (l.startsWith('#') || l.isEmpty()) continue;
                int cpos = l.lastIndexOf(':');
                double val = 1.0;
                QString linkPart = cpos > 0 ? l.left(cpos) : l;
                if (cpos > 0) { bool ok; double v = l.mid(cpos+1).trimmed().toDouble(&ok); if (ok) val = v; }
                QStringList parts = linkPart.split(arrowCh, Qt::SkipEmptyParts);
                if (parts.size() >= 2)
                    data.append({parts[0].trimmed(), parts[parts.size()-1].trimmed(), val});
            }
            if (data.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Formato: A \xe2\x86\x92 B: valore"); return; }
            m_canvas->setChordData(data);
            m_canvas->setType(GraficoCanvas::Chord);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(data.size()) + " corde");
            break;
        }
        case 40: { /* Violin Plot */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<QPair<QString,QVector<double>>> data;
            static QRegularExpression sepV("[,;\\s]+");
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                QString l = line.trimmed();
                if (l.startsWith('#') || l.isEmpty()) continue;
                int colon = l.indexOf(':');
                if (colon <= 0) continue;
                QString name = l.left(colon).trimmed();
                QVector<double> vals;
                for (auto& tok : l.mid(colon+1).split(sepV, Qt::SkipEmptyParts)) {
                    bool ok; double v = tok.toDouble(&ok); if (ok) vals.append(v);
                }
                if (!vals.isEmpty()) data.append({name, vals});
            }
            if (data.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Formato: NomeSerie: v1 v2 v3"); return; }
            m_canvas->setViolinData(data);
            m_canvas->setType(GraficoCanvas::Violin);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(data.size()) + " serie");
            break;
        }
        case 41: { /* Word Cloud */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<QPair<QString,double>> data;
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                QString l = line.trimmed();
                if (l.startsWith('#') || l.isEmpty()) continue;
                int colon = l.indexOf(':');
                if (colon <= 0) continue;
                bool ok; double v = l.mid(colon+1).trimmed().toDouble(&ok);
                if (ok) data.append({l.left(colon).trimmed(), v});
            }
            if (data.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Formato: parola:frequenza"); return; }
            m_canvas->setWordCloudData(data);
            m_canvas->setType(GraficoCanvas::WordCloud);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(data.size()) + " parole");
            break;
        }
        case 42: { /* Albero Radiale — stessa struttura di Tree */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<GraficoCanvas::TreeEdge> data;
            static QRegularExpression sepRT("[,;]+");
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                QString l = line.trimmed();
                if (l.startsWith('#') || l.isEmpty()) continue;
                int colon = l.indexOf(':');
                if (colon <= 0) continue;
                QString parent = l.left(colon).trimmed();
                QStringList children = l.mid(colon+1).split(sepRT, Qt::SkipEmptyParts);
                for (auto& ch : children) {
                    QString c = ch.trimmed();
                    if (!c.isEmpty()) data.append({parent, c});
                }
            }
            if (data.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Formato: padre: figlio1, figlio2"); return; }
            m_canvas->setTreeData(data);
            m_canvas->setType(GraficoCanvas::RadialTree);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(data.size()) + " archi");
            break;
        }
        case 43: { /* Linea Animata — stessa struttura di Linea */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<QVector<QPointF>> series;
            QStringList names;
            static QRegularExpression sepAN("[,;\\s]+");
            bool namesLoaded = false;
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                QString l = line.trimmed();
                if (l.startsWith('#')) {
                    if (!namesLoaded) {
                        names = l.mid(1).split(sepAN, Qt::SkipEmptyParts);
                        namesLoaded = true;
                    }
                    continue;
                }
                QStringList tok = l.split(sepAN, Qt::SkipEmptyParts);
                if (tok.size() < 2) continue;
                bool ok; double x = tok[0].toDouble(&ok); if (!ok) continue;
                for (int c = 1; c < tok.size(); ++c) {
                    double y = tok[c].toDouble(&ok); if (!ok) continue;
                    while (series.size() < c) series.append(QVector<QPointF>());
                    series[c-1].append({x, y});
                }
            }
            if (series.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessuna serie"); return; }
            m_canvas->setLine(series, names);
            m_canvas->setType(GraficoCanvas::AnimatedLine);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(series.size()) + " serie — animazione avviata");
            break;
        }
        case 44: { /* Small Multiples — stessa struttura di Linea */
            QString raw = m_dataEdit->toPlainText().trimmed();
            if (raw.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessun dato"); return; }
            QVector<QVector<QPointF>> series;
            QStringList names;
            static QRegularExpression sepSM("[,;\\s]+");
            bool namesLoaded = false;
            for (auto& line : raw.split('\n', Qt::SkipEmptyParts)) {
                QString l = line.trimmed();
                if (l.startsWith('#')) {
                    if (!namesLoaded) {
                        names = l.mid(1).split(sepSM, Qt::SkipEmptyParts);
                        namesLoaded = true;
                    }
                    continue;
                }
                QStringList tok = l.split(sepSM, Qt::SkipEmptyParts);
                if (tok.size() < 2) continue;
                bool ok; double x = tok[0].toDouble(&ok); if (!ok) continue;
                for (int c = 1; c < tok.size(); ++c) {
                    double y = tok[c].toDouble(&ok); if (!ok) continue;
                    while (series.size() < c) series.append(QVector<QPointF>());
                    series[c-1].append({x, y});
                }
            }
            if (series.isEmpty()) { m_statusLbl->setText("\xe2\x9a\xa0  Nessuna serie"); return; }
            m_canvas->setLine(series, names);
            m_canvas->setType(GraficoCanvas::SmallMultiples);
            m_statusLbl->setText("\xe2\x9c\x85  " + QString::number(series.size()) + " pannelli");
            break;
        }
    }
}

/* ── analyzeImage() ──────────────────────────────────────────────
   Carica un'immagine PNG/JPG, la invia al modello vision selezionato
   tramite AiClient::chatWithImage() e, alla risposta, inserisce la
   formula estratta in m_formulaEdit e traccia il grafico.
   ──────────────────────────────────────────────────────────────── */
void GraficoPage::analyzeImage() {
    if (!m_ai) {
        m_statusLbl->setText("\xe2\x9a\xa0  AiClient non disponibile");
        return;
    }
    if (m_ai->busy()) {
        m_statusLbl->setText("\xe2\x9a\xa0  AI occupata — attendi il termine dell'operazione");
        return;
    }
    const QString modello = m_visionCombo->currentText().trimmed();
    if (modello.isEmpty() || modello.startsWith("(")) {
        m_statusLbl->setText("\xe2\x9a\xa0  Seleziona un modello vision dalla lista");
        return;
    }

    /* Dialogo selezione immagine */
    const QString path = QFileDialog::getOpenFileName(
        this,
        "Seleziona immagine del grafico",
        QDir::homePath(),
        "Immagini (*.png *.jpg *.jpeg *.bmp *.webp)"
    );
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        m_statusLbl->setText("\xe2\x9d\x8c  Impossibile aprire il file");
        return;
    }
    const QByteArray raw   = f.readAll();
    const QByteArray b64   = raw.toBase64();
    const QString    mime  = path.endsWith(".png", Qt::CaseInsensitive) ? "image/png" : "image/jpeg";

    /* Cambia temporaneamente modello → vision → poi ripristina */
    const QString modeloPrecedente = m_ai->model();
    m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), modello);

    m_statusLbl->setText("\xf0\x9f\x94\x8d  Analisi in corso...");
    m_btnImgFormula->setEnabled(false);

    static const char* kSys =
        "Sei un analizzatore matematico preciso. "
        "Rispondi SEMPRE e SOLO in italiano. "
        "Il tuo UNICO compito e' identificare la formula matematica "
        "rappresentata nel grafico, nella forma y = f(x).";

    static const char* kUser =
        "Analizza questo grafico cartesiano e identifica la formula matematica "
        "nella forma y = f(x). "
        "Rispondi con UNA SOLA RIGA contenente solo la formula, "
        "senza testo aggiuntivo, senza 'y =', senza spiegazioni. "
        "Esempi di risposta corretta:\n"
        "  sin(x)\n"
        "  x^2 - 3*x + 2\n"
        "  1 / (1 + exp(-x))\n"
        "Se non riesci a identificare la formula scrivi: SCONOSCIUTO";

    /* Connessione one-shot per ricevere la risposta */
    auto* connHolder = new QObject(this);
    connect(m_ai, &AiClient::finished, connHolder,
            [this, connHolder, modeloPrecedente](const QString& risposta) {
        connHolder->deleteLater();
        m_btnImgFormula->setEnabled(true);

        /* Ripristina modello precedente */
        m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), modeloPrecedente);

        /* Estrai formula: prendi la prima riga non vuota */
        const QStringList righe = risposta.split('\n', Qt::SkipEmptyParts);
        QString formula;
        for (const QString& r : righe) {
            QString c = r.trimmed();
            /* Rimuovi eventuali prefissi "y = " o "f(x) = " */
            if (c.startsWith("y =", Qt::CaseInsensitive))  c = c.mid(3).trimmed();
            if (c.startsWith("y=",  Qt::CaseInsensitive))  c = c.mid(2).trimmed();
            if (c.startsWith("f(x) =", Qt::CaseInsensitive)) c = c.mid(6).trimmed();
            if (!c.isEmpty() && !c.toUpper().contains("SCONOSCIUTO")) {
                formula = c;
                break;
            }
        }

        if (formula.isEmpty()) {
            m_statusLbl->setText(
                "\xf0\x9f\x8c\xab  Il modello non ha riconosciuto la formula.\n"
                "Prova con un modello vision migliore o un'immagine pi\xc3\xb9 chiara.");
            return;
        }

        m_formulaEdit->setText(formula);
        m_statusLbl->setText("\xe2\x9c\x85  Formula estratta: " + formula);
        plot();
    });

    connect(m_ai, &AiClient::error, connHolder,
            [this, connHolder, modeloPrecedente](const QString& err) {
        connHolder->deleteLater();
        m_btnImgFormula->setEnabled(true);
        m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), modeloPrecedente);
        m_statusLbl->setText("\xe2\x9d\x8c  Errore: " + err);
    });

    m_ai->chatWithImage(kSys, kUser, b64, mime);
}
