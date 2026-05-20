#include "pages/rab0l_canvas.h"

#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>
#include <QMouseEvent>
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static constexpr double kA     = 0.08;
static constexpr double kB     = 0.18;
static constexpr double kAlpha = 1.5;
static constexpr double kPi    = M_PI;

/* ── Codifica nucleotide → indice base-80 (A=0, C=20, G=40, T=60) ── */
int Rab0lCanvas::ridFromBase(char c) {
    switch (c) {
    case 'A': return  0;
    case 'C': return 20;
    case 'G': return 40;
    case 'T': return 60;
    default:  return  0;
    }
}

QColor Rab0lCanvas::colorForBase(char c) {
    switch (c) {
    case 'A': return {0x4C, 0xAF, 0x50};   // verde
    case 'C': return {0xFF, 0xC1, 0x07};   // ambra
    case 'G': return {0x21, 0x96, 0xF3};   // blu
    case 'T': return {0xF4, 0x43, 0x36};   // rosso
    default:  return {0x99, 0x99, 0x99};
    }
}

/* ── Calcolo punti: display polar + coordinate reali per SIM ── */
QVector<Rab0lPoint> Rab0lCanvas::computePoints(const QString& seq) {
    QVector<Rab0lPoint> pts;
    int N = seq.size();
    if (!N) return pts;
    pts.reserve(N);
    for (int i = 0; i < N; ++i) {
        char   b   = seq[i].toLatin1();
        int    rid = ridFromBase(b);
        double theta = (rid / 80.0) * 2.0 * kPi + 2.0 * kPi * i;
        double r     = kA * std::exp(kB * theta);
        pts.push_back({
            (rid / 80.0) * 2.0 * kPi,   // dispAngle: settore nucleotide
            (i + 0.5) / N,              // dispR: posizione normalizzata
            theta,
            std::log(r),
            b,
            i
        });
    }
    return pts;
}

/* ── SIM formula RAB₀-L: d_i = sqrt(Δθ² + Δlog(r)²), SIM = mean(exp(-α·d_i)) ── */
Rab0lResult Rab0lCanvas::computeSim(const QVector<Rab0lPoint>& pts1,
                                     const QVector<Rab0lPoint>& pts2) {
    Rab0lResult res;
    int N = std::min(pts1.size(), pts2.size());
    if (!N) return res;
    res.len = N;
    res.contrib.reserve(N);
    double sum = 0.0;
    for (int i = 0; i < N; ++i) {
        double dTheta = pts1[i].theta - pts2[i].theta;
        double dLogR  = pts1[i].logR  - pts2[i].logR;
        double d      = std::sqrt(dTheta * dTheta + dLogR * dLogR);
        double c      = std::exp(-kAlpha * d);
        res.contrib.append(c);
        sum += c;
        if (pts1[i].base != pts2[i].base) ++res.snp;
    }
    res.sim = sum / N;
    return res;
}

/* ── Costruttore ── */
Rab0lCanvas::Rab0lCanvas(QWidget* parent) : QWidget(parent) {
    setMinimumSize(280, 280);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void Rab0lCanvas::setSequences(const QString& s1, const QString& s2) {
    m_seq1 = s1; m_seq2 = s2;
    m_pts1 = computePoints(s1);
    m_pts2 = computePoints(s2);
    m_result = (!m_pts1.isEmpty() && !m_pts2.isEmpty())
               ? computeSim(m_pts1, m_pts2) : Rab0lResult{};
    update();
}

void Rab0lCanvas::setSeq1Only(const QString& s1) {
    m_seq1 = s1; m_seq2.clear();
    m_pts1 = computePoints(s1); m_pts2.clear();
    m_result = {};
    update();
}

void Rab0lCanvas::clearAll() {
    m_seq1.clear(); m_seq2.clear();
    m_pts1.clear(); m_pts2.clear();
    m_result = {};
    m_zoom = 1.0; m_pan = {};
    update();
}

/* ── Helpers geometria ── */
QRectF Rab0lCanvas::canvasArea() const {
    return QRectF(8, 8, width() - 16, height() - 32);
}

double Rab0lCanvas::outerR(const QRectF& area) const {
    return std::min(area.width(), area.height()) * 0.42 * m_zoom;
}

QPointF Rab0lCanvas::toScreen(const Rab0lPoint& p, const QRectF& area) const {
    double cx = area.center().x() + m_pan.x();
    double cy = area.center().y() + m_pan.y();
    double R  = outerR(area);
    double a  = p.dispAngle - kPi / 2.0;   // A in cima (12 o'clock)
    return { cx + R * p.dispR * std::cos(a),
             cy + R * p.dispR * std::sin(a) };
}

/* ── Griglia ragnatela ── */
void Rab0lCanvas::drawGrid(QPainter& p, const QRectF& area) const {
    double cx = area.center().x() + m_pan.x();
    double cy = area.center().y() + m_pan.y();
    double R  = outerR(area);

    p.save();
    // cerchi concentrici
    QPen circPen(QColor(70, 80, 110, 70), 1, Qt::DotLine);
    p.setPen(circPen);
    for (int k = 1; k <= 4; ++k)
        p.drawEllipse(QPointF(cx, cy), R * k / 4.0, R * k / 4.0);

    // 4 raggi direzionali + etichette nucleotide
    static const char kBases[4] = {'A','C','G','T'};
    QFont lf = p.font(); lf.setBold(true); lf.setPointSize(10); p.setFont(lf);
    for (int i = 0; i < 4; ++i) {
        char   b     = kBases[i];
        int    rid   = i * 20;
        double angle = (rid / 80.0) * 2.0 * kPi - kPi / 2.0;
        double ex    = cx + (R + 18.0) * std::cos(angle);
        double ey    = cy + (R + 18.0) * std::sin(angle);

        QColor col = colorForBase(b);
        p.setPen(QPen(col.darker(140), 1, Qt::DashLine));
        p.drawLine(QPointF(cx, cy),
                   QPointF(cx + R * std::cos(angle), cy + R * std::sin(angle)));
        p.setPen(col);
        p.drawText(QRectF(ex - 11, ey - 11, 22, 22),
                   Qt::AlignCenter, QString(QChar(b)));
    }
    p.restore();
}

/* ── Sequenza: polilinea + dot colorati ── */
void Rab0lCanvas::drawSequence(QPainter& p, const QVector<Rab0lPoint>& pts,
                                bool primary, const QRectF& area) const {
    if (pts.isEmpty()) return;
    p.save();

    // traccia connettiva
    QPainterPath path;
    path.moveTo(toScreen(pts[0], area));
    for (int i = 1; i < pts.size(); ++i)
        path.lineTo(toScreen(pts[i], area));
    p.setPen(QPen(primary ? QColor(120, 190, 255, 140) : QColor(255, 170, 80, 140),
                  1.5, primary ? Qt::SolidLine : Qt::DashLine));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);

    // dot nucleotide
    bool showLabels = (pts.size() <= 40 && m_zoom >= 0.85);
    QFont lf = p.font(); lf.setPointSize(7); lf.setBold(false); p.setFont(lf);
    for (const auto& pt : pts) {
        QPointF sc  = toScreen(pt, area);
        QColor  col = colorForBase(pt.base);
        p.setPen(QPen(col.darker(170), 1));
        p.setBrush(primary ? col : col.lighter(170));
        double r = primary ? 5.5 : 4.0;
        p.drawEllipse(sc, r, r);
        if (showLabels) {
            p.setPen(primary ? Qt::black : col.darker(200));
            p.drawText(QRectF(sc.x() - 5, sc.y() - 5, 10, 10),
                       Qt::AlignCenter, QString(QChar(pt.base)));
        }
    }
    p.restore();
}

/* ── Linee di connessione tra stessa posizione (verde=match, rosso=SNP) ── */
void Rab0lCanvas::drawConnections(QPainter& p, const QRectF& area) const {
    int N = std::min(m_pts1.size(), m_pts2.size());
    p.save();
    for (int i = 0; i < N; ++i) {
        bool same = (m_pts1[i].base == m_pts2[i].base);
        p.setPen(QPen(same ? QColor(0, 210, 0, 45) : QColor(255, 60, 60, 110), 1));
        p.drawLine(toScreen(m_pts1[i], area), toScreen(m_pts2[i], area));
    }
    p.restore();
}

/* ── Legenda nucleotide ── */
void Rab0lCanvas::drawLegend(QPainter& p) const {
    p.save();
    QFont f = p.font(); f.setPointSize(8); f.setBold(false); p.setFont(f);
    int x = 10, y = height() - 20;
    for (char b : {'A','C','G','T'}) {
        QColor col = colorForBase(b);
        p.setBrush(col); p.setPen(QPen(col.darker(160), 1));
        p.drawEllipse(QRectF(x, y + 1, 10, 10));
        p.setPen(QColor(210, 210, 210));
        p.drawText(QRectF(x + 13, y, 18, 13), Qt::AlignVCenter, QString(QChar(b)));
        x += 36;
    }
    p.setPen(QColor(80, 90, 110));
    p.drawText(QRectF(width() - 175, y, 165, 13),
               Qt::AlignRight | Qt::AlignVCenter,
               "scroll=zoom \xe2\x80\x94 drag=pan");
    p.restore();
}

/* ── paintEvent ── */
void Rab0lCanvas::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor(17, 19, 27));

    if (m_pts1.isEmpty()) {
        p.setPen(QColor(85, 95, 115));
        QFont f = p.font(); f.setPointSize(11); p.setFont(f);
        p.drawText(rect(), Qt::AlignCenter,
                   "Inserisci una sequenza DNA\ne premi \xe2\x96\xb6 Analizza");
        return;
    }

    QRectF area = canvasArea();
    drawGrid(p, area);
    if (!m_pts2.isEmpty()) {
        drawConnections(p, area);
        drawSequence(p, m_pts2, false, area);
    }
    drawSequence(p, m_pts1, true, area);
    drawLegend(p);
}

/* ── Interazione mouse ── */
void Rab0lCanvas::wheelEvent(QWheelEvent* e) {
    m_zoom = std::clamp(m_zoom * (e->angleDelta().y() > 0 ? 1.15 : 0.87), 0.15, 10.0);
    e->accept();
    update();
}

void Rab0lCanvas::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        m_dragging  = true;
        m_dragStart = QPointF(e->pos());
    }
}

void Rab0lCanvas::mouseMoveEvent(QMouseEvent* e) {
    if (!m_dragging) return;
    QPointF cur = QPointF(e->pos());
    m_pan      += cur - m_dragStart;
    m_dragStart = cur;
    update();
}

void Rab0lCanvas::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) m_dragging = false;
}
