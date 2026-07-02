/*
 * grafico_canvas.cpp — GraficoCanvas implementation
 * ===================================================
 * Rendering QPainter puro, zoom + pan con mouse.
 * 45 tipi di grafico: Cartesian → SmallMultiples.
 * GraficoPage e la sua UI sono in grafico_page.cpp.
 */
#include "main_graph.h"
#include "main_graph_canvas_p.h"
#include "widgets/formula_parser.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QDir>
#include <QFont>
#include <cmath>
#include <algorithm>

QColor GraficoCanvas::paletteColor(int i) const {
    if (!m_style.palette.isEmpty())
        return m_style.palette[i % m_style.palette.size()];
    return kPal[i % 8];
}

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

void GraficoCanvas::setScatter3D(const QVector<Pt3D>& pts, int gridCols) {
    m_pts3d = pts;
    m_grid3dCols = gridCols;
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
            connect(m_animTimer, &QTimer::timeout, this, &GraficoCanvas::onAnimTimerTick);
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
   Slot modalità rendering 3D (connesso a QComboBox::currentIndexChanged)
   ══════════════════════════════════════════════════════════════ */
void GraficoCanvas::setRenderMode(int mode)
{
    m_renderMode3D = static_cast<RenderMode3D>(
        qBound(0, mode, static_cast<int>(Solid3D)));
    update();
}
