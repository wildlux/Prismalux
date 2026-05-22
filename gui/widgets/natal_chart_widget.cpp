#include "natal_chart_widget.h"
#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QToolButton>
#include <QLabel>
#include <QHBoxLayout>
#include <cmath>

/* ── Costanti zodiacali ───────────────────────────────────────── */
static constexpr int kNSigns = 12;

static const struct { const char* sym; int element; } kSigns[kNSigns] = {
    { "\xe2\x99\x88", 0 },  /* ♈ Ariete     — Fuoco  */
    { "\xe2\x99\x89", 1 },  /* ♉ Toro       — Terra  */
    { "\xe2\x99\x8a", 2 },  /* ♊ Gemelli    — Aria   */
    { "\xe2\x99\x8b", 3 },  /* ♋ Cancro     — Acqua  */
    { "\xe2\x99\x8c", 0 },  /* ♌ Leone      — Fuoco  */
    { "\xe2\x99\x8d", 1 },  /* ♍ Vergine    — Terra  */
    { "\xe2\x99\x8e", 2 },  /* ♎ Bilancia   — Aria   */
    { "\xe2\x99\x8f", 3 },  /* ♏ Scorpione  — Acqua  */
    { "\xe2\x99\x90", 0 },  /* ♐ Sagittario — Fuoco  */
    { "\xe2\x99\x91", 1 },  /* ♑ Capricorno — Terra  */
    { "\xe2\x99\x92", 2 },  /* ♒ Acquario   — Aria   */
    { "\xe2\x99\x93", 3 },  /* ♓ Pesci      — Acqua  */
};

/* Colori elemento: Fuoco, Terra, Aria, Acqua */
static const QColor kElemFill[4]   = {
    QColor(255,185,130,160), QColor(155,200,140,160),
    QColor(255,245,140,160), QColor(150,200,245,160),
};
static const QColor kElemPen[4]   = {
    QColor(190, 80, 30), QColor(70,130, 50),
    QColor(190,160, 20), QColor(50,110,170),
};

/* Colori aspetti: congiunzione, sestile, quadratura, trigono, opposizione */
static const QColor kAspColor[5] = {
    QColor(130,130,130,180),
    QColor(  0,160,  0,160),
    QColor(200,  0,  0,160),
    QColor(  0, 80,200,160),
    QColor(200, 50, 50,160),
};
static const double kAspAngles[5] = { 0, 60, 90, 120, 180 };
static const double kAspOrbs[5]   = { 8,  6,  8,   8,   8 };

/* ── Implementazione ────────────────────────────────────────────── */

NatalChartWidget::NatalChartWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(300, 300);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setCursor(Qt::OpenHandCursor);
    setMouseTracking(false);
    setToolTip("Rotella: zoom  |  Trascina: pan  |  Doppio clic: reset");

    const QString btnStyle =
        "QToolButton { border:none; background:transparent; font-size:14px; font-weight:bold; }"
        "QToolButton:hover { background:rgba(0,0,0,30); border-radius:3px; }";

    m_zoomBar = new QWidget(this);
    m_zoomBar->setStyleSheet(
        "background:rgba(255,255,255,210);"
        "border-radius:4px;"
        "border:1px solid rgba(0,0,0,40);");

    auto* lay = new QHBoxLayout(m_zoomBar);
    lay->setContentsMargins(4, 2, 4, 2);
    lay->setSpacing(2);

    m_btnZoomOut = new QToolButton(m_zoomBar);
    m_btnZoomOut->setText("\xe2\x88\x92");   /* − */
    m_btnZoomOut->setStyleSheet(btnStyle);
    m_btnZoomOut->setCursor(Qt::ArrowCursor);
    m_btnZoomOut->setToolTip("Zoom out");

    m_zoomLbl = new QLabel("100%", m_zoomBar);
    m_zoomLbl->setStyleSheet("font-size:11px; color:#333; min-width:36px;");
    m_zoomLbl->setAlignment(Qt::AlignCenter);

    m_btnZoomIn = new QToolButton(m_zoomBar);
    m_btnZoomIn->setText("+");
    m_btnZoomIn->setStyleSheet(btnStyle);
    m_btnZoomIn->setCursor(Qt::ArrowCursor);
    m_btnZoomIn->setToolTip("Zoom in");

    m_btnReset = new QToolButton(m_zoomBar);
    m_btnReset->setText("\xe2\x8c\x82");   /* ⌂ */
    m_btnReset->setStyleSheet(btnStyle);
    m_btnReset->setCursor(Qt::ArrowCursor);
    m_btnReset->setToolTip("Reimposta vista");

    lay->addWidget(m_btnZoomOut);
    lay->addWidget(m_zoomLbl);
    lay->addWidget(m_btnZoomIn);
    lay->addWidget(m_btnReset);

    connect(m_btnZoomOut, &QToolButton::clicked, this, &NatalChartWidget::onZoomOutClicked);
    connect(m_btnZoomIn,  &QToolButton::clicked, this, &NatalChartWidget::onZoomInClicked);
    connect(m_btnReset,   &QToolButton::clicked, this, &NatalChartWidget::onResetViewClicked);

    /* posiziona dopo che il widget ha dimensione iniziale */
    updateZoomLabel();
}

void NatalChartWidget::clear() {
    m_planets.clear();
    m_aspects.clear();
    m_hasData = false;
    update();
}

void NatalChartWidget::resetView() {
    m_zoom = 1.0;
    m_pan  = QPointF(0.0, 0.0);
    updateZoomLabel();
    update();
}

void NatalChartWidget::updateZoomLabel()
{
    m_zoomLbl->setText(QString("%1%").arg(qRound(m_zoom * 100)));
    m_btnZoomOut->setEnabled(m_zoom > 0.26);
    m_btnZoomIn ->setEnabled(m_zoom < 5.9);
}

void NatalChartWidget::positionZoomBar()
{
    const int margin = 6;
    const int zbH    = 28;
    const int zbW    = 124;   /* larghezza fissa: − | 100% | + | ⌂ */
    if (width() < zbW + margin * 2) return;
    m_zoomBar->setGeometry(width() - zbW - margin, margin, zbW, zbH);
    m_zoomBar->raise();
}

void NatalChartWidget::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    positionZoomBar();
}

void NatalChartWidget::onZoomInClicked()
{
    m_zoom = qBound(0.25, m_zoom * 1.25, 6.0);
    updateZoomLabel();
    update();
}

void NatalChartWidget::onZoomOutClicked()
{
    m_zoom = qBound(0.25, m_zoom / 1.25, 6.0);
    updateZoomLabel();
    update();
}

void NatalChartWidget::onResetViewClicked()
{
    resetView();
}

void NatalChartWidget::setData(const QVector<Planet>& planets, double ascLon, double mcLon,
                               const double* cusps)
{
    m_planets = planets;
    m_ascLon  = ascLon;
    m_mcLon   = (mcLon >= 0) ? mcLon : ascLon + 270.0;
    if (cusps) {
        for (int i = 0; i < 12; ++i) m_cusps[i] = cusps[i];
        m_hasRealCusps = true;
    } else {
        m_hasRealCusps = false;
    }
    m_hasData = !planets.isEmpty();
    computeAspects();
    update();
}

/* screenAngle in gradi: 0°=destra (DC), 90°=cima (MC), 180°=sinistra (AC), 270°=basso (IC) */
double NatalChartWidget::screenAngle(double lon) const
{
    return 180.0 + (lon - m_ascLon);
}

QPointF NatalChartWidget::toScreen(double lon, double r, double cx, double cy) const
{
    const double a = screenAngle(lon) * M_PI / 180.0;
    return QPointF(cx + r * std::cos(a), cy - r * std::sin(a));
}

void NatalChartWidget::computeAspects()
{
    m_aspects.clear();
    for (int i = 0; i < m_planets.size(); ++i) {
        for (int j = i + 1; j < m_planets.size(); ++j) {
            double diff = std::fmod(std::abs(m_planets[i].lon - m_planets[j].lon), 360.0);
            if (diff > 180.0) diff = 360.0 - diff;
            for (int t = 0; t < 5; ++t) {
                if (std::abs(diff - kAspAngles[t]) <= kAspOrbs[t]) {
                    m_aspects.append({i, j, t});
                    break;
                }
            }
        }
    }
}

/* ── Zoom / Pan ─────────────────────────────────────────────────── */

void NatalChartWidget::wheelEvent(QWheelEvent* e)
{
    const double factor = (e->angleDelta().y() > 0) ? 1.12 : (1.0 / 1.12);
    m_zoom = qBound(0.25, m_zoom * factor, 6.0);
    updateZoomLabel();
    update();
    e->accept();
}

void NatalChartWidget::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton) {
        m_dragging  = true;
        m_dragStart = QPointF(e->pos()) - m_pan;
        setCursor(Qt::ClosedHandCursor);
    }
}

void NatalChartWidget::mouseMoveEvent(QMouseEvent* e)
{
    if (m_dragging) {
        m_pan = QPointF(e->pos()) - m_dragStart;
        update();
    }
}

void NatalChartWidget::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton) {
        m_dragging = false;
        setCursor(Qt::OpenHandCursor);
    }
}

void NatalChartWidget::mouseDoubleClickEvent(QMouseEvent*)
{
    onResetViewClicked();
}

/* ── paintEvent — Step-Down ─────────────────────────────────────── */

void NatalChartWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setClipRect(rect());   /* evita overflow fuori dai bordi del widget */
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    const double W    = width();
    const double H    = height();
    const double size = qMin(W, H);
    /* Centro spostato da pan */
    const double cx   = W / 2.0 + m_pan.x();
    const double cy   = H / 2.0 + m_pan.y();
    /* Raggio scalato dallo zoom */
    const double R    = size / 2.0 * 0.92 * m_zoom;

    drawBackground(p, cx, cy, R);
    drawZodiacRing(p, cx, cy, R);
    drawHouseArea (p, cx, cy, R);
    drawAxes      (p, cx, cy, R);

    if (m_hasData) {
        drawAspects(p, cx, cy, R);
        drawPlanets(p, cx, cy, R);
    } else {
        drawPlaceholder(p, cx, cy, R);
    }

}

/* ── Layer 1: cerchio azzurro di sfondo ─────────────────────────── */
void NatalChartWidget::drawBackground(QPainter& p, double cx, double cy, double R)
{
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(215, 235, 255));
    p.drawEllipse(QPointF(cx, cy), R, R);

    p.setBrush(QColor(252, 250, 245));
    p.drawEllipse(QPointF(cx, cy), R * 0.87, R * 0.87);
}

/* ── Layer 2: anello zodiacale (12 segmenti colorati) ─────────── */
void NatalChartWidget::drawZodiacRing(QPainter& p, double cx, double cy, double R)
{
    const double rOut = R * 0.87;
    const double rIn  = R * 0.73;
    const QRectF rOuter(cx - rOut, cy - rOut, rOut * 2, rOut * 2);
    const QRectF rInner(cx - rIn,  cy - rIn,  rIn  * 2, rIn  * 2);

    for (int i = 0; i < kNSigns; ++i) {
        const double lon      = i * 30.0;
        const double startAng = screenAngle(lon);
        const double spanAng  = 30.0;  /* CCW */

        QPainterPath path;
        path.moveTo(toScreen(lon, rOut, cx, cy));
        path.arcTo(rOuter, startAng, spanAng);
        path.arcTo(rInner, startAng + spanAng, -spanAng);
        path.closeSubpath();

        p.setBrush(kElemFill[kSigns[i].element]);
        p.setPen(QPen(kElemPen[kSigns[i].element].darker(120), 0.6));
        p.drawPath(path);

        /* Simbolo segno al centro del settore */
        const double midLon  = lon + 15.0;
        const double symR    = (rOut + rIn) / 2.0;
        const QPointF symPt  = toScreen(midLon, symR, cx, cy);
        const double symHalf = qMax(10.0, R * 0.075);
        QFont sf = font();
        sf.setPixelSize(qMax(10, static_cast<int>(R * 0.095)));
        p.setFont(sf);
        p.setPen(kElemPen[kSigns[i].element]);
        p.drawText(QRectF(symPt.x() - symHalf, symPt.y() - symHalf,
                          symHalf * 2, symHalf * 2),
                   Qt::AlignCenter, QString::fromUtf8(kSigns[i].sym));
    }

    /* Bordi anello */
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(110, 100, 90), 1.0));
    p.drawEllipse(QPointF(cx, cy), rOut, rOut);
    p.drawEllipse(QPointF(cx, cy), rIn,  rIn);

    /* Tacche di divisione + tacche minori ogni 10° */
    for (int i = 0; i < kNSigns; ++i) {
        const double lon = i * 30.0;
        p.setPen(QPen(QColor(110, 100, 90), 1.0));
        p.drawLine(toScreen(lon, rOut, cx, cy), toScreen(lon, rIn, cx, cy));

        p.setPen(QPen(QColor(150, 140, 130), 0.5));
        for (int t = 1; t < 3; ++t) {
            p.drawLine(toScreen(lon + t * 10.0, rOut,       cx, cy),
                       toScreen(lon + t * 10.0, rOut * 0.93, cx, cy));
        }
    }
}

/* ── Layer 3: area case (numeri + cerchio interno) ───────────────── */
void NatalChartWidget::drawHouseArea(QPainter& p, double cx, double cy, double R)
{
    const double rHouseOut = R * 0.73;  /* = rIn dello zodiaco */
    const double rHouseIn  = R * 0.22;  /* cerchio centrale     */

    /* Sfondo area case */
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(253, 251, 247));
    p.drawEllipse(QPointF(cx, cy), rHouseOut, rHouseOut);

    /* Linee di divisione case */
    p.setPen(QPen(QColor(150, 140, 130), 0.6, Qt::DotLine));
    for (int h = 0; h < 12; ++h) {
        const double lon = m_hasRealCusps ? m_cusps[h] : (m_ascLon + h * 30.0);
        p.drawLine(toScreen(lon, rHouseOut, cx, cy),
                   toScreen(lon, rHouseIn,  cx, cy));
    }

    /* Numeri case */
    QFont nf = font();
    nf.setPixelSize(qMax(8, static_cast<int>(R * 0.060)));
    p.setFont(nf);
    p.setPen(QColor(80, 80, 110));
    const double nHalf = qMax(10.0, R * 0.065);
    const double numR  = (rHouseOut + rHouseIn) / 2.0;
    for (int h = 0; h < 12; ++h) {
        const double cuspA = m_hasRealCusps ? m_cusps[h]             : (m_ascLon + h * 30.0);
        const double cuspB = m_hasRealCusps ? m_cusps[(h + 1) % 12]  : (m_ascLon + (h + 1) * 30.0);
        double span = cuspB - cuspA;
        if (span < 0) span += 360.0;
        const double midLon = std::fmod(cuspA + span / 2.0, 360.0);
        const QPointF pt    = toScreen(midLon, numR, cx, cy);
        p.drawText(QRectF(pt.x() - nHalf, pt.y() - nHalf, nHalf * 2, nHalf * 2),
                   Qt::AlignCenter, QString::number(h + 1));
    }

    /* Cerchio centrale (hub) */
    p.setPen(QPen(QColor(110, 100, 90), 1.0));
    p.setBrush(QColor(255, 253, 249));
    p.drawEllipse(QPointF(cx, cy), rHouseIn, rHouseIn);
}

/* ── Layer 4: assi AC/DC/MC/IC ───────────────────────────────────── */
void NatalChartWidget::drawAxes(QPainter& p, double cx, double cy, double R)
{
    const double rAxis = R * 0.73;   /* = bordo interno anello zodiacale */
    const double mcLon = (m_mcLon >= 0) ? m_mcLon : m_ascLon + 270.0;

    QPen axisPen(QColor(15, 15, 15), 2.0);
    p.setPen(axisPen);
    p.drawLine(toScreen(m_ascLon,         rAxis, cx, cy),
               toScreen(m_ascLon + 180.0, rAxis, cx, cy));
    p.drawLine(toScreen(mcLon,         rAxis, cx, cy),
               toScreen(mcLon + 180.0, rAxis, cx, cy));

    /* Etichette nell'anello zodiacale, con box bianco per leggibilità */
    QFont af = font();
    af.setPixelSize(qMax(9, static_cast<int>(R * 0.065)));
    af.setBold(true);
    p.setFont(af);

    const double lr = rAxis + (R * 0.87 - rAxis) * 0.42;

    const double lw = qMax(22.0, R * 0.115);
    const double lh = qMax(14.0, R * 0.075);
    auto label = [&](double lon, const QString& txt) {
        const double la = screenAngle(lon) * M_PI / 180.0;
        const double lx = cx + lr * std::cos(la);
        const double ly = cy - lr * std::sin(la);
        const QRectF bg(lx - lw / 2, ly - lh / 2, lw, lh);
        p.fillRect(bg, QColor(255, 255, 255, 230));
        p.setPen(QPen(QColor(10, 10, 10), 0.7));
        p.drawRect(bg);
        p.setPen(QColor(10, 10, 10));
        p.drawText(bg, Qt::AlignCenter, txt);
    };
    label(m_ascLon,           "AC");
    label(m_ascLon + 180.0,   "DC");
    label(mcLon,              "MC");
    label(mcLon + 180.0,      "IC");
}

/* ── Layer 5: linee aspetto nel cerchio interno ──────────────────── */
void NatalChartWidget::drawAspects(QPainter& p, double cx, double cy, double R)
{
    const double rAsp = R * 0.20;
    for (const Aspect& a : m_aspects) {
        p.setPen(QPen(kAspColor[a.type], 1.0));
        p.drawLine(toScreen(m_planets[a.i].lon, rAsp, cx, cy),
                   toScreen(m_planets[a.j].lon, rAsp, cx, cy));
    }
}

/* ── Layer 6: simboli pianeti ────────────────────────────────────── */
void NatalChartWidget::drawPlanets(QPainter& p, double cx, double cy, double R)
{
    const double rPl = R * 0.60;
    const int    px  = qMax(10, static_cast<int>(R * 0.100));
    const int    dx  = qMax(7,  static_cast<int>(R * 0.060));

    QFont pf = font();
    pf.setPixelSize(px);
    QFont df = font();
    df.setPixelSize(dx);

    const double prad = qMax(8.0, R * 0.045);   /* raggio cerchio pianeta */
    const double dw   = qMax(14.0, R * 0.08);   /* larghezza label grado  */
    const double dh   = qMax(10.0, R * 0.055);  /* altezza  label grado   */

    for (const Planet& pl : m_planets) {
        const QPointF pt = toScreen(pl.lon, rPl, cx, cy);

        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 210));
        p.drawEllipse(pt, prad, prad);

        p.setFont(pf);
        p.setPen(pl.color);
        p.drawText(QRectF(pt.x() - prad, pt.y() - prad, prad * 2, prad * 2),
                   Qt::AlignCenter, pl.symbol);

        const double la = screenAngle(pl.lon) * M_PI / 180.0;
        const double ex = pt.x() + (prad + 5.0) * std::cos(la);
        const double ey = pt.y() - (prad + 5.0) * std::sin(la);
        const int    deg = static_cast<int>(pl.lon) % 30;
        p.setFont(df);
        p.setPen(QColor(50, 50, 70));
        p.drawText(QRectF(ex - dw / 2, ey - dh / 2, dw, dh),
                   Qt::AlignCenter, QString::number(deg) + "\xc2\xb0");
    }
}

/* ── Placeholder quando non ci sono dati ────────────────────────── */
void NatalChartWidget::drawPlaceholder(QPainter& p, double cx, double cy, double R)
{
    QFont pf = font();
    pf.setPixelSize(qMax(12, static_cast<int>(R * 0.07)));
    p.setFont(pf);
    p.setPen(QColor(140, 130, 120));
    const double ph = qMax(40.0, R * 0.18);
    p.drawText(QRectF(cx - R * 0.55, cy - ph / 2, R * 1.1, ph),
               Qt::AlignCenter,
               "\xe2\xad\x90  Il tema natale\napparirà qui");
}

