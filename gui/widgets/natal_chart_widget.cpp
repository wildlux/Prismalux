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
#include <algorithm>
#include <cmath>

/* ── Radii proporzionali a R (raggio pieno) ──────────────────────
   Astrodienst layout:
   [outer_band | zodiac_ring | planet_ring | house_area | hub]
   R=1.00       0.880         0.720  0.640   0.620        0.190
   ──────────────────────────────────────────────────────────────── */
static constexpr double kRZodOut  = 0.880;   /* bordo esterno anello zodiaco  */
static constexpr double kRZodIn   = 0.720;   /* bordo interno anello zodiaco  */
static constexpr double kRSep     = 0.640;   /* separatore anello pianeti / case */
static constexpr double kRPlanet  = 0.678;   /* centro glifi pianeti (nel ring)  */
static constexpr double kRAspEnd  = 0.620;   /* endpoint linee aspetto           */
static constexpr double kRHouseN  = 0.430;   /* numeri case                      */
static constexpr double kRHub     = 0.190;   /* cerchio hub centrale             */

static constexpr int kNSigns  = 12;
static constexpr int kNAspDef = 9;

/* ── Segni zodiacali ─────────────────────────────────────────── */
static const struct { const char* sym; int element; } kSigns[kNSigns] = {
    { "\xe2\x99\x88", 0 }, /* ♈ Ariete     — Fuoco  */
    { "\xe2\x99\x89", 1 }, /* ♉ Toro       — Terra  */
    { "\xe2\x99\x8a", 2 }, /* ♊ Gemelli    — Aria   */
    { "\xe2\x99\x8b", 3 }, /* ♋ Cancro     — Acqua  */
    { "\xe2\x99\x8c", 0 }, /* ♌ Leone      — Fuoco  */
    { "\xe2\x99\x8d", 1 }, /* ♍ Vergine    — Terra  */
    { "\xe2\x99\x8e", 2 }, /* ♎ Bilancia   — Aria   */
    { "\xe2\x99\x8f", 3 }, /* ♏ Scorpione  — Acqua  */
    { "\xe2\x99\x90", 0 }, /* ♐ Sagittario — Fuoco  */
    { "\xe2\x99\x91", 1 }, /* ♑ Capricorno — Terra  */
    { "\xe2\x99\x92", 2 }, /* ♒ Acquario   — Aria   */
    { "\xe2\x99\x93", 3 }, /* ♓ Pesci      — Acqua  */
};

/* Colori elemento — palette Astrodienst */
static const QColor kElemFill[4] = {
    QColor(255, 195, 135, 195), /* Fuoco — arancio caldo  */
    QColor(160, 210, 140, 195), /* Terra — verde salvia   */
    QColor(255, 248, 148, 195), /* Aria  — giallo chiaro  */
    QColor(145, 200, 255, 195), /* Acqua — azzurro cielo  */
};
static const QColor kElemPen[4] = {
    QColor(165,  60,  15),
    QColor( 50, 110,  30),
    QColor(145, 125,   0),
    QColor( 30,  90, 165),
};

/* ── Definizioni aspetti (9 tipi) ────────────────────────────── */
struct AspectDef {
    double       angle;
    double       orb;
    QColor       color;
    Qt::PenStyle style;
    double       width;
};
/* Colori dal progetto zodiac (atten/zodiac): rosso duro (207,41,33),
   blu armonioso (15,114,248), verde minore (14,162,98) */
static const AspectDef kAspDefs[kNAspDef] = {
    /*0 Congiunzione (0°)    */ {   0.0, 8.0, QColor(100,100,100,200), Qt::SolidLine,    1.5 },
    /*1 Sestile (60°)        */ {  60.0, 5.0, QColor( 15,114,248,220), Qt::SolidLine,    1.8 },
    /*2 Quadratura (90°)     */ {  90.0, 7.0, QColor(207, 41, 33,235), Qt::SolidLine,    2.0 },
    /*3 Trigono (120°)       */ { 120.0, 7.0, QColor( 15,114,248,220), Qt::SolidLine,    1.8 },
    /*4 Opposizione (180°)   */ { 180.0, 7.0, QColor(207, 41, 33,235), Qt::SolidLine,    2.0 },
    /*5 Semi-sestile (30°)   */ {  30.0, 1.5, QColor(120,120,120,130), Qt::DotLine,      0.9 },
    /*6 Semi-quadratura (45°)*/ {  45.0, 1.5, QColor( 14,162, 98,175), Qt::DashDotLine,  1.1 },
    /*7 Quinconce (150°)     */ { 150.0, 2.5, QColor( 14,162, 98,175), Qt::DashDotLine,  1.1 },
    /*8 Sesquiquadrato (135°)*/ { 135.0, 1.5, QColor( 14,162, 98,175), Qt::DashDotLine,  1.1 },
};

/* ══════════════════════════════════════════════════════════════════
   Costruttore e infrastruttura zoom/pan (invariati)
   ══════════════════════════════════════════════════════════════════ */

NatalChartWidget::NatalChartWidget(QWidget* parent) : QWidget(parent)
{
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
    m_btnZoomOut->setText("\xe2\x88\x92");
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
    m_btnReset->setText("\xe2\x8c\x82");
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

    updateZoomLabel();
}

void NatalChartWidget::clear()
{
    m_planets.clear();
    m_aspects.clear();
    m_hasData = false;
    update();
}

void NatalChartWidget::resetView()
{
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
    const int zbW    = 124;
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

/* screenAngle: 0°=destra(DC), 90°=alto(MC), 180°=sinistra(AC), 270°=basso(IC) */
double NatalChartWidget::screenAngle(double lon) const
{
    return 180.0 + (lon - m_ascLon);
}

QPointF NatalChartWidget::toScreen(double lon, double r, double cx, double cy) const
{
    const double a = screenAngle(lon) * M_PI / 180.0;
    return QPointF(cx + r * std::cos(a), cy - r * std::sin(a));
}

/* ── Calcola aspetti — 9 tipi ───────────────────────────────────── */
void NatalChartWidget::computeAspects()
{
    m_aspects.clear();
    for (int i = 0; i < m_planets.size(); ++i) {
        for (int j = i + 1; j < m_planets.size(); ++j) {
            double diff = std::fmod(std::abs(m_planets[i].lon - m_planets[j].lon), 360.0);
            if (diff > 180.0) diff = 360.0 - diff;
            for (int t = 0; t < kNAspDef; ++t) {
                if (std::abs(diff - kAspDefs[t].angle) <= kAspDefs[t].orb) {
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

/* ══════════════════════════════════════════════════════════════════
   paintEvent — composizione layer
   ══════════════════════════════════════════════════════════════════ */

void NatalChartWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setClipRect(rect());
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    const double W  = width();
    const double H  = height();
    const double sz = qMin(W, H);
    const double cx = W / 2.0 + m_pan.x();
    const double cy = H / 2.0 + m_pan.y();
    const double R  = sz / 2.0 * 0.88 * m_zoom;   /* raggio pieno (include outer band) */

    drawBackground  (p, cx, cy, R);
    drawZodiacRing  (p, cx, cy, R);
    drawHouseArea   (p, cx, cy, R);
    drawAxes        (p, cx, cy, R);

    if (m_hasData) {
        drawAspects(p, cx, cy, R);
        drawPlanets(p, cx, cy, R);
    } else {
        drawPlaceholder(p, cx, cy, R);
    }
}

/* ══════════════════════════════════════════════════════════════════
   Layer 1 — Sfondo
   Outer band: azzurro Astrodienst
   Inner cream: area case
   ══════════════════════════════════════════════════════════════════ */
void NatalChartWidget::drawBackground(QPainter& p, double cx, double cy, double R)
{
    /* Fascia esterna azzurra (da R a R*kRZodOut) */
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(185, 213, 244));
    p.drawEllipse(QPointF(cx, cy), R, R);

    /* Area interna crema (da R*kRZodOut verso il centro: ridisegnata in drawHouseArea) */
    /* Qui disegniamo solo lo sfondo crema sotto l'anello zodiacale */
    const double rZI = R * kRZodIn;
    p.setBrush(QColor(252, 250, 246));
    p.drawEllipse(QPointF(cx, cy), rZI, rZI);
}

/* ══════════════════════════════════════════════════════════════════
   Layer 2 — Anello zodiacale
   12 settori colorati per elemento, glifi, tacche 5° e 1°
   ══════════════════════════════════════════════════════════════════ */
void NatalChartWidget::drawZodiacRing(QPainter& p, double cx, double cy, double R)
{
    const double rOut = R * kRZodOut;
    const double rIn  = R * kRZodIn;
    const QRectF rOuter(cx - rOut, cy - rOut, rOut * 2, rOut * 2);
    const QRectF rInner(cx - rIn,  cy - rIn,  rIn  * 2, rIn  * 2);

    /* Settori colorati per elemento */
    for (int i = 0; i < kNSigns; ++i) {
        const double lon      = i * 30.0;
        const double startAng = screenAngle(lon);
        const double spanAng  = 30.0;

        QPainterPath path;
        path.moveTo(toScreen(lon, rOut, cx, cy));
        path.arcTo(rOuter, startAng, spanAng);
        path.arcTo(rInner, startAng + spanAng, -spanAng);
        path.closeSubpath();

        p.setBrush(kElemFill[kSigns[i].element]);
        p.setPen(QPen(kElemPen[kSigns[i].element].darker(110), 0.5));
        p.drawPath(path);
    }

    /* Glifi zodiacali al centro di ogni settore */
    QFont sf = font();
    sf.setPixelSize(qMax(11, static_cast<int>(R * 0.092)));
    p.setFont(sf);
    const double symR    = (rOut + rIn) / 2.0;
    const double symHalf = qMax(10.0, R * 0.078);
    for (int i = 0; i < kNSigns; ++i) {
        const double midLon = i * 30.0 + 15.0;
        const QPointF sp    = toScreen(midLon, symR, cx, cy);
        p.setPen(kElemPen[kSigns[i].element]);
        p.drawText(QRectF(sp.x() - symHalf, sp.y() - symHalf, symHalf * 2, symHalf * 2),
                   Qt::AlignCenter, QString::fromUtf8(kSigns[i].sym));
    }

    /* Bordi anello */
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(95, 85, 78), 1.1));
    p.drawEllipse(QPointF(cx, cy), rOut, rOut);
    p.drawEllipse(QPointF(cx, cy), rIn,  rIn);

    /* Linee divisorie segni (30°) */
    p.setPen(QPen(QColor(95, 85, 78), 1.0));
    for (int i = 0; i < kNSigns; ++i)
        p.drawLine(toScreen(i * 30.0, rOut, cx, cy),
                   toScreen(i * 30.0, rIn,  cx, cy));

    /* Tacche 5° sul bordo esterno */
    p.setPen(QPen(QColor(130, 120, 110), 0.7));
    for (int d = 0; d < 360; d += 5) {
        if (d % 30 == 0) continue;
        p.drawLine(toScreen(d, rOut,        cx, cy),
                   toScreen(d, rOut * 0.940, cx, cy));
    }

    /* Tacche 1° (ancora più corte) */
    p.setPen(QPen(QColor(160, 150, 140), 0.4));
    for (int d = 0; d < 360; ++d) {
        if (d % 5 == 0) continue;
        p.drawLine(toScreen(d, rOut,        cx, cy),
                   toScreen(d, rOut * 0.965, cx, cy));
    }

    /* Tacche 5° sul bordo INTERNO dell'anello — visibili dall'area pianeti */
    p.setPen(QPen(QColor(130, 120, 110), 0.6));
    for (int d = 0; d < 360; d += 5) {
        if (d % 30 == 0) continue;
        p.drawLine(toScreen(d, rIn,        cx, cy),
                   toScreen(d, rIn * 1.028, cx, cy));
    }

    /* Numeri di grado 0/10/20 all'interno dell'anello zodiacale —
       stile Astrodienst: piccoli numeri vicino al bordo interno */
    QFont gf = font();
    gf.setPixelSize(qMax(5, static_cast<int>(R * 0.038)));
    p.setFont(gf);
    const double numGradR = rIn * 1.060;
    const double ngHalf   = qMax(5.0, R * 0.030);
    for (int s = 0; s < 12; ++s) {
        const double baseLon = s * 30.0;
        for (int g = 0; g <= 20; g += 10) {
            const double lon    = baseLon + g;
            const QPointF np    = toScreen(lon, numGradR, cx, cy);
            const int signIdx   = kSigns[s].element;
            p.setPen(kElemPen[signIdx].darker(120));
            p.drawText(QRectF(np.x() - ngHalf, np.y() - ngHalf, ngHalf * 2, ngHalf * 2),
                       Qt::AlignCenter, QString::number(g));
        }
    }
}

/* ══════════════════════════════════════════════════════════════════
   Layer 3 — Area case
   Sfondo crema, separatore pianeti/numeri, linee case, numeri
   ══════════════════════════════════════════════════════════════════ */
void NatalChartWidget::drawHouseArea(QPainter& p, double cx, double cy, double R)
{
    const double rHO  = R * kRZodIn;  /* bordo esterno area case  */
    const double rSep = R * kRSep;    /* anello separatore        */
    const double rHI  = R * kRHub;    /* hub centrale             */

    /* Sfondo crema uniforme per tutta l'area interna */
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(252, 250, 246));
    p.drawEllipse(QPointF(cx, cy), rHO, rHO);

    /* Linee cuspidi case — dal bordo zodiaco fino all'hub */
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(140, 130, 120), 0.8));
    for (int h = 0; h < 12; ++h) {
        const double lon = m_hasRealCusps ? m_cusps[h] : (m_ascLon + h * 30.0);
        p.drawLine(toScreen(lon, rHO, cx, cy),
                   toScreen(lon, rHI, cx, cy));
    }

    /* Anello separatore anello pianeti / area case (più visibile) */
    p.setPen(QPen(QColor(150, 140, 130), 1.0, Qt::SolidLine));
    p.drawEllipse(QPointF(cx, cy), rSep, rSep);

    /* Numeri case — font leggermente più grande */
    QFont nf = font();
    nf.setPixelSize(qMax(9, static_cast<int>(R * 0.062)));
    p.setFont(nf);
    p.setPen(QColor(75, 75, 105));
    const double nHalf = qMax(10.0, R * 0.064);
    const double numR  = R * kRHouseN;
    for (int h = 0; h < 12; ++h) {
        const double cA = m_hasRealCusps ? m_cusps[h]            : (m_ascLon + h * 30.0);
        const double cB = m_hasRealCusps ? m_cusps[(h + 1) % 12] : (m_ascLon + (h + 1) * 30.0);
        double span = std::fmod(cB - cA + 360.0, 360.0);
        const double midLon = std::fmod(cA + span / 2.0, 360.0);
        const QPointF pt = toScreen(midLon, numR, cx, cy);
        p.drawText(QRectF(pt.x() - nHalf, pt.y() - nHalf, nHalf * 2, nHalf * 2),
                   Qt::AlignCenter, QString::number(h + 1));
    }

    /* Hub centrale — bordo leggermente più spesso */
    p.setPen(QPen(QColor(95, 85, 78), 1.2));
    p.setBrush(QColor(255, 253, 250));
    p.drawEllipse(QPointF(cx, cy), rHI, rHI);
}

/* ══════════════════════════════════════════════════════════════════
   Layer 4 — Assi AC/DC/MC/IC
   Linee nere spesse attraverso tutto il cerchio interno.
   Etichette con grado (dentro segno) posizionate fuori dalla ruota.
   ══════════════════════════════════════════════════════════════════ */
void NatalChartWidget::drawAxes(QPainter& p, double cx, double cy, double R)
{
    const double rAxis  = R * kRZodIn;   /* fino al bordo interno zodiaco */
    const double mcLon  = (m_mcLon >= 0) ? m_mcLon : m_ascLon + 270.0;

    /* Linee assi — nere e spesse */
    QPen axisPen(QColor(10, 10, 10), 2.2);
    p.setPen(axisPen);
    p.drawLine(toScreen(m_ascLon,        rAxis, cx, cy),
               toScreen(m_ascLon + 180.0, rAxis, cx, cy));
    p.drawLine(toScreen(mcLon,           rAxis, cx, cy),
               toScreen(mcLon + 180.0,   rAxis, cx, cy));

    /* Helper: grado dentro il segno zodiacale */
    auto degInSign = [](double lon) -> int {
        return static_cast<int>(std::fmod(lon < 0 ? lon + 360.0 : lon, 30.0));
    };

    /* Etichette nella fascia esterna azzurra (tra R e kRZodOut*R) */
    QFont af = font();
    af.setPixelSize(qMax(8, static_cast<int>(R * 0.056)));
    af.setBold(true);
    p.setFont(af);

    const double lr   = R * 0.940;         /* raggio etichetta (nella fascia azzurra) */
    const double lw   = qMax(24.0, R * 0.105);
    const double lh   = qMax(13.0, R * 0.066);

    auto axisLabel = [&](double lon, const QString& tag) {
        const double la = screenAngle(lon) * M_PI / 180.0;
        const double lx = cx + lr * std::cos(la);
        const double ly = cy - lr * std::sin(la);
        const QString txt = tag + " " + QString::number(degInSign(lon)) + "\xc2\xb0";
        const QRectF rc(lx - lw / 2, ly - lh / 2, lw, lh);

        /* Sfondo bianco nella fascia azzurra per leggibilità */
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 215));
        p.drawRoundedRect(rc.adjusted(-2, -1, 2, 1), 3, 3);

        p.setBrush(Qt::NoBrush);
        p.setPen(QColor(10, 10, 10));
        p.drawText(rc, Qt::AlignCenter, txt);
    };

    axisLabel(m_ascLon,         "AC");
    axisLabel(m_ascLon + 180.0, "DC");
    axisLabel(mcLon,            "MC");
    axisLabel(mcLon + 180.0,    "IC");
}

/* ══════════════════════════════════════════════════════════════════
   Layer 5 — Linee di aspetto
   Colori Astrodienst: rosso=duro, blu=armonioso, verde=minore
   Endpoint: R*kRAspEnd (pianeti proiettati sull'anello interno)
   ══════════════════════════════════════════════════════════════════ */
void NatalChartWidget::drawAspects(QPainter& p, double cx, double cy, double R)
{
    const double rEnd = R * kRAspEnd;
    for (const Aspect& a : m_aspects) {
        if (a.type < 0 || a.type >= kNAspDef) continue;
        const AspectDef& def = kAspDefs[a.type];
        QPen pen(def.color, def.width, def.style);
        pen.setCapStyle(Qt::RoundCap);
        p.setPen(pen);
        p.drawLine(toScreen(m_planets[a.i].lon, rEnd, cx, cy),
                   toScreen(m_planets[a.j].lon, rEnd, cx, cy));
    }
}

/* ══════════════════════════════════════════════════════════════════
   Layer 6 — Pianeti
   Glifo + grado°min' dentro il segno.
   Linea radiale sottile dal glifo al bordo interno dello zodiaco.
   Anti-collisione: pianeti entro 6° vengono alternati a radii diversi.
   ══════════════════════════════════════════════════════════════════ */
void NatalChartWidget::drawPlanets(QPainter& p, double cx, double cy, double R)
{
    const int n = m_planets.size();
    if (n == 0) return;

    /* Ordina per longitudine per rilevare collisioni */
    QVector<int> idx(n);
    for (int i = 0; i < n; ++i) idx[i] = i;
    std::sort(idx.begin(), idx.end(), [&](int a, int b) {
        return m_planets[a].lon < m_planets[b].lon;
    });

    /* Assegna raggio glifo — alterna se due pianeti sono entro 7°.
       rDelta piccolo: i pianeti restano nel ring (kRZodIn .. kRSep). */
    const double rBase  = R * kRPlanet;
    const double rDelta = R * 0.018;
    QVector<double> rGlyph(n, rBase);
    {
        int level = 0;
        for (int k = 1; k < n; ++k) {
            double diff = std::fmod(std::abs(m_planets[idx[k]].lon - m_planets[idx[k-1]].lon), 360.0);
            if (diff > 180.0) diff = 360.0 - diff;
            if (diff < 7.0) {
                level = (level % 2) + 1;
            } else {
                level = 0;
            }
            rGlyph[idx[k]] = rBase - level * rDelta;
        }
    }

    const double rZodIn = R * kRZodIn;
    const int    pxGly  = qMax(9,  static_cast<int>(R * 0.072));
    const int    pxDeg  = qMax(6,  static_cast<int>(R * 0.052));
    const double prad   = qMax(7.0, R * 0.036);

    QFont pf = font(); pf.setPixelSize(pxGly);
    QFont df = font(); df.setPixelSize(pxDeg);

    for (int i = 0; i < n; ++i) {
        const Planet& pl = m_planets[i];
        const double  rG = rGlyph[i];
        const QPointF pt = toScreen(pl.lon, rG, cx, cy);

        /* Linea radiale dal glifo al bordo interno dello zodiaco */
        p.setPen(QPen(pl.color.darker(160), 0.7));
        p.drawLine(pt, toScreen(pl.lon, rZodIn - 1.0, cx, cy));

        /* Cerchio bianco di sfondo glifo */
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 215));
        p.drawEllipse(pt, prad, prad);

        /* Glifo pianeta */
        p.setFont(pf);
        p.setPen(pl.color);
        p.drawText(QRectF(pt.x() - prad, pt.y() - prad, prad * 2, prad * 2),
                   Qt::AlignCenter, pl.symbol);

        /* Etichetta grado°min' — verso l'anello zodiacale (Astrodienst style) */
        const double degInS  = std::fmod(pl.lon < 0 ? pl.lon + 360.0 : pl.lon, 30.0);
        const int    d       = static_cast<int>(degInS);
        const int    m_val   = static_cast<int>((degInS - d) * 60.0);
        const QString degLbl = QString("%1\xc2\xb0%2'")
                               .arg(d)
                               .arg(m_val, 2, 10, QChar('0'));

        /* Posizione etichetta: outward (verso lo zodiaco) */
        const double la = screenAngle(pl.lon) * M_PI / 180.0;
        const double ex = pt.x() + (prad + 2.0) * std::cos(la);
        const double ey = pt.y() - (prad + 2.0) * std::sin(la);
        const double dw = qMax(20.0, R * 0.082);
        const double dh = qMax(9.0,  R * 0.044);
        /* sfondo bianco semitrasparente per leggibilità nell'anello */
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 190));
        p.drawRoundedRect(QRectF(ex - dw / 2 - 1, ey - dh / 2 - 1, dw + 2, dh + 2), 2, 2);
        p.setFont(df);
        p.setPen(QColor(45, 45, 65));
        p.drawText(QRectF(ex - dw / 2, ey - dh / 2, dw, dh),
                   Qt::AlignCenter, degLbl);
    }
}

/* ── Placeholder quando non ci sono dati ─────────────────────────── */
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
