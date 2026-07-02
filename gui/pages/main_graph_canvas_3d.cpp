/*
 * main_graph_canvas_3d.cpp — GraficoCanvas: rendering 3D
 * ==========================================================
 * Scatter3D (punti/wireframe/solido/superficie) e Graph3D.
 * Split da main_graph_canvas.cpp (TODO D-8).
 */
#include "main_graph.h"
#include "main_graph_canvas_p.h"

#include <QPainter>
#include <QFont>
#include <cmath>
#include <algorithm>
#include <limits>

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
    /* ─── colore basato su z (heatmap: blu→ciano→verde→giallo→rosso) ─── */
    /* calcola range z valido (esclude NaN) */
    double zMnC = std::numeric_limits<double>::max();
    double zMxC = std::numeric_limits<double>::lowest();
    for (const auto& pt : m_pts3d) {
        if (!std::isnan(pt.z)) { zMnC = std::min(zMnC, pt.z); zMxC = std::max(zMxC, pt.z); }
    }
    if (zMxC <= zMnC) { zMnC = zMn; zMxC = zMx; }

    auto heatColor = [&](double z) -> QColor {
        double t = (zMxC > zMnC) ? (z - zMnC) / (zMxC - zMnC) : 0.5;
        t = std::max(0.0, std::min(1.0, t));
        static const int rs[] = {  0,  0,  0,200,200};
        static const int gs[] = {  0,200,200,200,  0};
        static const int bs[] = {200,200,  0,  0,  0};
        const double s = t * 4.0;
        const int i = std::min(3, (int)s);
        const double f = s - i;
        return QColor(int(rs[i] + f*(rs[i+1]-rs[i])),
                      int(gs[i] + f*(gs[i+1]-gs[i])),
                      int(bs[i] + f*(bs[i+1]-bs[i])));
    };

    /* ─── proietta tutti i punti (inclusi NaN — per topologia griglia) ─── */
    struct PP { double sx, sy, depth; int idx; bool valid; };
    QVector<PP> proj(m_pts3d.size());
    for (int i = 0; i < m_pts3d.size(); i++) {
        const bool valid = !std::isnan(m_pts3d[i].z);
        double sx = 0, sy = 0, dz = 0;
        if (valid) {
            double nx = (m_pts3d[i].x - xMn) * sX - 0.5;
            double ny = (m_pts3d[i].y - yMn) * sY - 0.5;
            double nz = (m_pts3d[i].z - zMn) * sZ - 0.5;
            project(nx, ny, nz, sx, sy, dz);
        }
        proj[i] = {sx, sy, dz, i, valid};
    }

    const bool hasGrid = (m_grid3dCols > 1 &&
                          m_pts3d.size() >= m_grid3dCols * 2);
    const int cols = hasGrid ? m_grid3dCols : 0;
    const int rows = hasGrid ? (m_pts3d.size() / cols) : 0;

    if (hasGrid && m_renderMode3D == Surface3D) {
        /* ── Superficie riempita — painter's algorithm su quad ── */
        struct Quad { int r, c; double avgDepth; };
        QVector<Quad> quads;
        quads.reserve((rows-1)*(cols-1));
        for (int r = 0; r < rows-1; r++) {
            for (int c = 0; c < cols-1; c++) {
                const int i00=r*cols+c, i10=r*cols+c+1;
                const int i01=(r+1)*cols+c, i11=(r+1)*cols+c+1;
                if (!proj[i00].valid||!proj[i10].valid||
                    !proj[i01].valid||!proj[i11].valid) continue;
                quads.append({r, c,
                    (proj[i00].depth+proj[i10].depth+
                     proj[i01].depth+proj[i11].depth)*0.25});
            }
        }
        std::sort(quads.begin(), quads.end(),
                  [](const Quad& a, const Quad& b){ return a.avgDepth > b.avgDepth; });
        p.setRenderHint(QPainter::Antialiasing, false);
        for (const auto& q : quads) {
            const int i00=q.r*cols+q.c, i10=q.r*cols+q.c+1;
            const int i01=(q.r+1)*cols+q.c, i11=(q.r+1)*cols+q.c+1;
            const double zAvg = (m_pts3d[i00].z+m_pts3d[i10].z+
                                 m_pts3d[i01].z+m_pts3d[i11].z)*0.25;
            const QColor col = heatColor(zAvg);
            QPolygonF poly;
            poly << QPointF(proj[i00].sx,proj[i00].sy)
                 << QPointF(proj[i10].sx,proj[i10].sy)
                 << QPointF(proj[i11].sx,proj[i11].sy)
                 << QPointF(proj[i01].sx,proj[i01].sy);
            p.setBrush(col);
            p.setPen(QPen(col.darker(115), 0.4));
            p.drawPolygon(poly);
        }
        p.setRenderHint(QPainter::Antialiasing, true);

    } else if (hasGrid && m_renderMode3D == Solid3D) {
        /* ── Solido — quad colore piatto con depth shading ── */
        struct Quad { int r, c; double avgDepth; };
        QVector<Quad> quads;
        quads.reserve((rows-1)*(cols-1));
        for (int r = 0; r < rows-1; r++) {
            for (int c = 0; c < cols-1; c++) {
                const int i00=r*cols+c, i10=r*cols+c+1;
                const int i01=(r+1)*cols+c, i11=(r+1)*cols+c+1;
                if (!proj[i00].valid||!proj[i10].valid||
                    !proj[i01].valid||!proj[i11].valid) continue;
                quads.append({r, c,
                    (proj[i00].depth+proj[i10].depth+
                     proj[i01].depth+proj[i11].depth)*0.25});
            }
        }
        std::sort(quads.begin(), quads.end(),
                  [](const Quad& a, const Quad& b){ return a.avgDepth > b.avgDepth; });
        double dMin = 1e18, dMax = -1e18;
        for (const auto& q : quads) {
            dMin = std::min(dMin, q.avgDepth);
            dMax = std::max(dMax, q.avgDepth);
        }
        const double dRange = (dMax > dMin) ? (dMax - dMin) : 1.0;
        const QColor baseColor(0x3b, 0x82, 0xf6);
        p.setRenderHint(QPainter::Antialiasing, false);
        for (const auto& q : quads) {
            const int i00=q.r*cols+q.c, i10=q.r*cols+q.c+1;
            const int i01=(q.r+1)*cols+q.c, i11=(q.r+1)*cols+q.c+1;
            const double t = (q.avgDepth - dMin) / dRange;
            const int factor = 100 + static_cast<int>((1.0 - t) * 100);
            const QColor col = baseColor.darker(factor);
            QPolygonF poly;
            poly << QPointF(proj[i00].sx,proj[i00].sy)
                 << QPointF(proj[i10].sx,proj[i10].sy)
                 << QPointF(proj[i11].sx,proj[i11].sy)
                 << QPointF(proj[i01].sx,proj[i01].sy);
            p.setBrush(col);
            p.setPen(QPen(col.darker(130), 0.5));
            p.drawPolygon(poly);
        }
        p.setRenderHint(QPainter::Antialiasing, true);

    } else if (hasGrid && m_renderMode3D == Wireframe3D) {
        /* ── Wireframe — linee colorate per z ── */
        p.setRenderHint(QPainter::Antialiasing, true);
        auto drawSeg = [&](int i0, int i1) {
            if (!proj[i0].valid || !proj[i1].valid) return;
            const double zAvg = (m_pts3d[i0].z + m_pts3d[i1].z) * 0.5;
            p.setPen(QPen(heatColor(zAvg), 0.9));
            p.drawLine(QPointF(proj[i0].sx,proj[i0].sy),
                       QPointF(proj[i1].sx,proj[i1].sy));
        };
        for (int r = 0; r < rows; r++)
            for (int c = 0; c < cols-1; c++)
                drawSeg(r*cols+c, r*cols+c+1);
        for (int c = 0; c < cols; c++)
            for (int r = 0; r < rows-1; r++)
                drawSeg(r*cols+c, (r+1)*cols+c);

    } else {
        /* ── Punti (modalità default) ── */

        /* linee di collegamento — disegnate PRIMA dei punti così restano sotto */
        if (m_connectPts3D && proj.size() >= 2) {
            for (int i = 0; i < proj.size() - 1; i++) {
                if (!proj[i].valid || !proj[i+1].valid) continue;
                const double zAvg = (m_pts3d[i].z + m_pts3d[i+1].z) * 0.5;
                p.setPen(QPen(heatColor(zAvg).lighter(130), 1.5, Qt::SolidLine,
                              Qt::RoundCap, Qt::RoundJoin));
                p.drawLine(QPointF(proj[i].sx,   proj[i].sy),
                           QPointF(proj[i+1].sx, proj[i+1].sy));
            }
        }

        /* ordina per profondità (painter's algorithm) */
        QVector<const PP*> sortedPts;
        sortedPts.reserve(proj.size());
        for (const auto& pp : proj) if (pp.valid) sortedPts.append(&pp);
        std::sort(sortedPts.begin(), sortedPts.end(),
                  [](const PP* a, const PP* b){ return a->depth > b->depth; });
        for (const auto* pp : sortedPts) {
            const QColor c = paletteColor(pp->idx);
            p.setBrush(c);
            p.setPen(QPen(c.darker(150), 1));
            p.drawEllipse(QPointF(pp->sx, pp->sy), 5, 5);
        }
        p.setFont(QFont("Inter,Ubuntu,sans-serif", 8));
        for (const auto* pp : sortedPts) {
            const QColor  c   = paletteColor(pp->idx);
            const QString lbl = (!m_pts3d[pp->idx].label.isEmpty())
                                ? m_pts3d[pp->idx].label
                                : QString("P%1").arg(pp->idx + 1);
            p.setPen(c.lighter(140));
            p.drawText(QPointF(pp->sx + 8, pp->sy - 3), lbl);
        }
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
