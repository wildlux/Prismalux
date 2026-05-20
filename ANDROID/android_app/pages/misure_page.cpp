#include "misure_page.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QLabel>
#include <QFont>
#include <QFrame>
#include <QTextCursor>
#include <QScrollBar>
#include <QPainter>
#include <QPen>
#include <QPolygonF>
#include <QScroller>
#include <QScrollerProperties>
#include <cmath>

/* ══════════════════════════════════════════════════════════════
   RoomPlanWidget — planimetria 2D touch-interattiva
   ══════════════════════════════════════════════════════════════ */
RoomPlanWidget::RoomPlanWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("RoomPlan");
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(300);
    setAttribute(Qt::WA_AcceptTouchEvents);
}

void RoomPlanWidget::setScaleMetersPerCell(double m)
{
    m_metersPerCell = qMax(0.1, m);
    update();
}

void RoomPlanWidget::reset()
{
    m_points.clear();
    update();
    emit planChanged(QString());
}

QPointF RoomPlanWidget::pixToM(QPointF px) const
{
    return { px.x() / m_gridPx * m_metersPerCell,
             px.y() / m_gridPx * m_metersPerCell };
}

double RoomPlanWidget::areaM2() const
{
    const int n = m_points.size();
    if (n < 3) return 0.0;
    double sum = 0.0;
    for (int i = 0; i < n; ++i) {
        const QPointF a = pixToM(m_points[i]);
        const QPointF b = pixToM(m_points[(i + 1) % n]);
        sum += a.x() * b.y() - b.x() * a.y();
    }
    return std::abs(sum) / 2.0;
}

double RoomPlanWidget::perimeterM() const
{
    const int n = m_points.size();
    if (n < 2) return 0.0;
    double perim = 0.0;
    for (int i = 0; i < n; ++i) {
        const QPointF a = pixToM(m_points[i]);
        const QPointF b = pixToM(m_points[(i + 1) % n]);
        const double dx = b.x() - a.x();
        const double dy = b.y() - a.y();
        perim += std::sqrt(dx * dx + dy * dy);
    }
    return perim;
}

QString RoomPlanWidget::calcResults() const
{
    const int n = m_points.size();
    if (n < 3) return QString();
    const double area  = areaM2();
    const double perim = perimeterM();
    return QString("Pavimento: <b>%1 m²</b>  —  Perimetro: <b>%2 m</b>  —  Punti: %3")
        .arg(area, 0, 'f', 2)
        .arg(perim, 0, 'f', 2)
        .arg(n);
}

void RoomPlanWidget::mousePressEvent(QMouseEvent* e)
{
    m_points.append(e->pos());
    update();
    emit planChanged(calcResults());
}

void RoomPlanWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    /* sfondo */
    p.fillRect(rect(), QColor(22, 30, 50));

    const int w = width(), h = height();

    /* griglia */
    QPen gridPen(QColor(50, 70, 110, 160), 1, Qt::DotLine);
    p.setPen(gridPen);
    for (int x = 0; x < w; x += m_gridPx)
        p.drawLine(x, 0, x, h);
    for (int y = 0; y < h; y += m_gridPx)
        p.drawLine(0, y, w, y);

    /* etichette scala (ogni 2 quadretti) */
    p.setPen(QColor(80, 110, 180, 200));
    QFont sf;
    sf.setPointSize(7);
    p.setFont(sf);
    for (int x = 0; x < w; x += m_gridPx * 2) {
        const double mVal = x / double(m_gridPx) * m_metersPerCell;
        p.drawText(QRect(x + 2, h - 16, m_gridPx * 2, 14),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   QString("%1m").arg(mVal, 0, 'f', 1));
    }

    if (m_points.isEmpty()) {
        p.setPen(QColor(150, 180, 255, 160));
        QFont hf; hf.setPointSize(11);
        p.setFont(hf);
        p.drawText(rect(), Qt::AlignCenter,
            QString::fromUtf8("\xf0\x9f\x91\x86 Tocca per aggiungere punti\ndella planimetria"));
        return;
    }

    /* riempimento poligono */
    if (m_points.size() >= 3) {
        QPolygonF poly;
        for (const auto& pt : m_points) poly << pt;
        p.setBrush(QColor(80, 140, 220, 90));
        p.setPen(Qt::NoPen);
        p.drawPolygon(poly);
    }

    /* lati */
    QPen edgePen(QColor(100, 180, 255), 2.0);
    p.setPen(edgePen);
    for (int i = 0; i < m_points.size() - 1; ++i)
        p.drawLine(m_points[i], m_points[i + 1]);
    /* lato chiusura */
    if (m_points.size() >= 3) {
        QPen closePen(QColor(100, 255, 180, 180), 2.0, Qt::DashLine);
        p.setPen(closePen);
        p.drawLine(m_points.last(), m_points.first());
    }

    /* punti numerati */
    p.setFont(sf);
    for (int i = 0; i < m_points.size(); ++i) {
        const QPointF& pt = m_points[i];
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 200, 80));
        p.drawEllipse(pt, 9, 9);
        p.setPen(QColor(20, 20, 20));
        p.drawText(QRectF(pt.x() - 9, pt.y() - 9, 18, 18),
                   Qt::AlignCenter, QString::number(i + 1));
    }

    /* misure sui lati */
    QPen dimPen(QColor(255, 220, 100), 1.0);
    p.setPen(dimPen);
    QFont df; df.setPointSize(8);
    p.setFont(df);
    const int n = m_points.size();
    for (int i = 0; i < n; ++i) {
        const QPointF a = m_points[i];
        const QPointF b = m_points[(i + 1) % n];
        if (i == n - 1 && n < 3) break;
        const QPointF mid((a.x() + b.x()) / 2, (a.y() + b.y()) / 2);
        const QPointF am = pixToM(a), bm = pixToM(b);
        const double d = std::sqrt(std::pow(bm.x() - am.x(), 2) +
                                   std::pow(bm.y() - am.y(), 2));
        p.drawText(QRectF(mid.x() - 24, mid.y() - 14, 48, 14),
                   Qt::AlignCenter, QString("%1m").arg(d, 0, 'f', 2));
    }
}

/* ══════════════════════════════════════════════════════════════
   RoomVisualWidget — proiezione isometrica della stanza
   ══════════════════════════════════════════════════════════════ */
RoomVisualWidget::RoomVisualWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("RoomVisual");
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(220);
}

void RoomVisualWidget::setDimensions(double L, double W, double H)
{
    m_L = qMax(0.1, L);
    m_W = qMax(0.1, W);
    m_H = qMax(0.1, H);
    update();
}

void RoomVisualWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const double W = width();
    const double H = height();

    /* sfondo scuro stile blueprint */
    p.fillRect(rect(), QColor(22, 30, 50));

    /* ── Proiezione isometrica standard ──
       Assi:
         X (larghezza stanza, m_W): verso destra-basso  (dx = cos30°, dy = sin30°)
         Y (profondità stanza, m_L): verso sinistra-basso (dx = -cos30°, dy = sin30°)
         Z (altezza stanza, m_H):   verso l'alto          (dx = 0, dy = -1)
    */
    const double cos30 = 0.86602540378;
    const double sin30 = 0.5;

    /* Spazio schermo necessario (in unità room):
       orizzontale: (m_W + m_L) * cos30
       verticale:   (m_W + m_L) * sin30 + m_H          */
    const double needW = (m_W + m_L) * cos30;
    const double needH = (m_W + m_L) * sin30 + m_H;

    const double margin = 28.0;
    const double scale  = qMin((W - margin * 2) / needW,
                               (H - margin * 2) / needH);

    /* Origine (angolo frontale-sinistro-basso della stanza) nel canvas */
    const double ox = W / 2.0 + (m_L - m_W) / 2.0 * cos30 * scale;
    const double oy = H / 2.0 + (m_W + m_L) / 4.0 * sin30 * scale;

    /* Proietta un punto 3D (x=largh., y=prof., z=alt.) → 2D screen */
    auto proj = [&](double x, double y, double z) -> QPointF {
        return {
            ox + (x * cos30 - y * cos30) * scale,
            oy + (x * sin30 + y * sin30 - z) * scale
        };
    };

    /* 8 vertici della stanza */
    QPointF p000 = proj(0,    0,    0);
    QPointF p100 = proj(m_W,  0,    0);
    QPointF p010 = proj(0,    m_L,  0);
    QPointF p110 = proj(m_W,  m_L,  0);
    QPointF p001 = proj(0,    0,    m_H);
    QPointF p101 = proj(m_W,  0,    m_H);
    QPointF p011 = proj(0,    m_L,  m_H);
    QPointF p111 = proj(m_W,  m_L,  m_H);

    QPen outline(QColor(180, 200, 255, 200), 1.2);

    /* ── Pavimento (z=0) — blu acciaio ── */
    QPolygonF floor_poly;
    floor_poly << p000 << p100 << p110 << p010;
    p.setBrush(QColor(50, 90, 160, 210));
    p.setPen(outline);
    p.drawPolygon(floor_poly);

    /* ── Parete destra (x=m_W) — azzurro medio ── */
    QPolygonF right_wall;
    right_wall << p100 << p110 << p111 << p101;
    p.setBrush(QColor(70, 120, 200, 210));
    p.drawPolygon(right_wall);

    /* ── Parete posteriore (y=m_L) — azzurro chiaro ── */
    QPolygonF back_wall;
    back_wall << p010 << p110 << p111 << p011;
    p.setBrush(QColor(100, 150, 220, 210));
    p.drawPolygon(back_wall);

    /* ── Spigoli visibili del soffitto ── */
    QPen edgePen(QColor(200, 220, 255, 160), 1.0, Qt::DashLine);
    p.setPen(edgePen);
    p.drawLine(p001, p101);
    p.drawLine(p001, p011);
    p.drawLine(p101, p111);
    p.drawLine(p011, p111);

    /* ── Spigoli verticali frontali ── */
    QPen solidEdge(QColor(200, 220, 255, 220), 1.4);
    p.setPen(solidEdge);
    p.drawLine(p000, p001);
    p.drawLine(p100, p101);
    p.drawLine(p010, p011);

    /* ── Etichette aree ── */
    QFont lblFont;
    lblFont.setPointSize(8);
    lblFont.setBold(true);
    p.setFont(lblFont);

    auto centroid4 = [](QPointF a, QPointF b, QPointF c, QPointF d) {
        return QPointF((a.x()+b.x()+c.x()+d.x())/4.0,
                       (a.y()+b.y()+c.y()+d.y())/4.0);
    };

    auto drawLabel = [&](QPointF center, const QString& line1, const QString& line2) {
        const int tw = 90, th = 32;
        QRectF r(center.x() - tw/2.0, center.y() - th/2.0, tw, th);
        p.setPen(QPen(QColor(20, 40, 80, 160)));
        p.setBrush(QColor(20, 40, 80, 160));
        p.drawRoundedRect(r, 4, 4);
        p.setPen(Qt::white);
        p.drawText(r.adjusted(0, 0, 0, -th/2), Qt::AlignCenter, line1);
        p.setPen(QColor(180, 220, 255));
        p.drawText(r.adjusted(0, th/2, 0, 0), Qt::AlignCenter, line2);
    };

    const double areaFloor     = m_W * m_L;
    const double areaPareRight = m_L * m_H;  /* parete destra: prof × alt */
    const double areaPareBack  = m_W * m_H;  /* parete poster.: larg × alt */

    drawLabel(centroid4(p000, p100, p110, p010),
              "Pavimento",
              QString("%1 m\xc2\xb2").arg(areaFloor, 0, 'f', 1));

    drawLabel(centroid4(p100, p110, p111, p101),
              "Parete lat.",
              QString("%1 m\xc2\xb2").arg(areaPareRight, 0, 'f', 1));

    drawLabel(centroid4(p010, p110, p111, p011),
              "Parete post.",
              QString("%1 m\xc2\xb2").arg(areaPareBack, 0, 'f', 1));

    /* ── Dimensioni sui bordi ── */
    QPen dimPen(QColor(255, 230, 100, 200), 1.0);
    p.setPen(dimPen);
    QFont dimFont;
    dimFont.setPointSize(7);
    p.setFont(dimFont);

    auto midpoint = [](QPointF a, QPointF b) { return QPointF((a.x()+b.x())/2, (a.y()+b.y())/2); };

    /* Lunghezza m_W (bordo frontale) */
    QPointF mFront = midpoint(p000, p100);
    p.drawText(QRectF(mFront.x() - 20, mFront.y() + 4, 40, 16),
               Qt::AlignCenter,
               QString("%1m").arg(m_W, 0, 'f', 1));

    /* Profondità m_L (bordo sinistro) */
    QPointF mLeft = midpoint(p000, p010);
    p.drawText(QRectF(mLeft.x() - 35, mLeft.y() - 8, 34, 16),
               Qt::AlignCenter,
               QString("%1m").arg(m_L, 0, 'f', 1));

    /* Altezza m_H (bordo verticale sinistro) */
    QPointF mVert = midpoint(p000, p001);
    p.drawText(QRectF(mVert.x() - 36, mVert.y() - 8, 34, 16),
               Qt::AlignRight | Qt::AlignVCenter,
               QString("%1m").arg(m_H, 0, 'f', 1));
}

/* ══════════════════════════════════════════════════════════════
   MisurePage — costruttore
   ══════════════════════════════════════════════════════════════ */
MisurePage::MisurePage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    QScroller::grabGesture(scroll->viewport(), QScroller::TouchGesture);
    QScroller* qs = QScroller::scroller(scroll->viewport());
    QScrollerProperties sp = qs->scrollerProperties();
    sp.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy,
        QVariant::fromValue<QScrollerProperties::OvershootPolicy>(
            QScrollerProperties::OvershootAlwaysOff));
    sp.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy,
        QVariant::fromValue<QScrollerProperties::OvershootPolicy>(
            QScrollerProperties::OvershootAlwaysOff));
    qs->setScrollerProperties(sp);

    auto* inner = new QWidget;
    auto* vbox  = new QVBoxLayout(inner);
    vbox->setContentsMargins(8, 8, 8, 8);
    vbox->setSpacing(10);

    auto* title = new QLabel(
        QString::fromUtf8("\xf0\x9f\x93\x90 Misure & Fotogrammetria"), inner);
    QFont tf = title->font();
    tf.setPointSize(14);
    tf.setBold(true);
    title->setFont(tf);
    title->setAlignment(Qt::AlignCenter);
    vbox->addWidget(title);

    /* ─────────────────────────────────────────────────────
       Sezione 1 — Calcolatore dimensioni stanza
       ───────────────────────────────────────────────────── */
    auto* calcGroup = new QGroupBox(
        QString::fromUtf8("\xf0\x9f\x93\x8f Dimensioni Stanza"), inner);
    auto* calcForm = new QFormLayout(calcGroup);
    calcForm->setSpacing(8);
    calcForm->setRowWrapPolicy(QFormLayout::WrapAllRows);

    m_lungSpin = new QDoubleSpinBox(inner);
    m_lungSpin->setRange(0.1, 999.0);
    m_lungSpin->setValue(5.0);
    m_lungSpin->setSuffix(" m");
    m_lungSpin->setDecimals(2);
    calcForm->addRow("Lunghezza (prof.):", m_lungSpin);

    m_largSpin = new QDoubleSpinBox(inner);
    m_largSpin->setRange(0.1, 999.0);
    m_largSpin->setValue(4.0);
    m_largSpin->setSuffix(" m");
    m_largSpin->setDecimals(2);
    calcForm->addRow("Larghezza:", m_largSpin);

    m_altSpin = new QDoubleSpinBox(inner);
    m_altSpin->setRange(0.1, 30.0);
    m_altSpin->setValue(2.7);
    m_altSpin->setSuffix(" m");
    m_altSpin->setDecimals(2);
    calcForm->addRow("Altezza soffitto:", m_altSpin);

    m_sovrasSpin = new QDoubleSpinBox(inner);
    m_sovrasSpin->setRange(0.0, 30.0);
    m_sovrasSpin->setValue(10.0);
    m_sovrasSpin->setSuffix(" %");
    m_sovrasSpin->setDecimals(0);
    calcForm->addRow("Scarto piastrelle:", m_sovrasSpin);

    auto* btnCalc = new QPushButton(
        QString::fromUtf8("\xe2\x9a\x99\xef\xb8\x8f Calcola"), inner);
    btnCalc->setMinimumHeight(44);
    btnCalc->setObjectName("PrimaryBtn");
    calcForm->addRow(btnCalc);

    m_resultLbl = new QLabel("", inner);
    m_resultLbl->setWordWrap(true);
    m_resultLbl->setTextInteractionFlags(
        Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    calcForm->addRow(m_resultLbl);

    vbox->addWidget(calcGroup);

    /* ─────────────────────────────────────────────────────
       Sezione 2 — Vista 3D isometrica della stanza
       ───────────────────────────────────────────────────── */
    auto* view3dGroup = new QGroupBox(
        QString::fromUtf8("\xf0\x9f\x8f\xa0 Vista 3D (proiezione isometrica)"), inner);
    auto* view3dLay = new QVBoxLayout(view3dGroup);
    view3dLay->setContentsMargins(4, 4, 4, 4);

    m_roomView = new RoomVisualWidget(view3dGroup);
    view3dLay->addWidget(m_roomView);

    auto* view3dHint = new QLabel(
        QString::fromUtf8("<small style='color:#8890a8'>"
        "\xf0\x9f\x92\xa1 La vista si aggiorna dopo aver premuto <b>Calcola</b>.<br>"
        "Blu scuro = pavimento &nbsp; Blu medio = parete laterale &nbsp; Blu chiaro = parete posteriore"
        "</small>"),
        view3dGroup);
    view3dHint->setTextFormat(Qt::RichText);
    view3dHint->setWordWrap(true);
    view3dLay->addWidget(view3dHint);

    vbox->addWidget(view3dGroup);

    /* ─────────────────────────────────────────────────────
       Sezione 3 — Analisi AI
       ───────────────────────────────────────────────────── */
    auto* aiGroup = new QGroupBox(
        QString::fromUtf8("\xf0\x9f\xa4\x96 Analisi AI"), inner);
    auto* aiVbox = new QVBoxLayout(aiGroup);
    aiVbox->setSpacing(6);

    m_aiAction = new QComboBox(inner);
    m_aiAction->addItems({
        "Stima dimensioni da descrizione",
        "Consigli materiali / ristrutturazione",
        "Calcola costo ristrutturazione",
        "Fotogrammetria / misure da foto (descrivi)",
        "Confronto standard edilizi italiani",
    });
    aiVbox->addWidget(m_aiAction);

    m_aiInput = new QTextEdit(inner);
    m_aiInput->setPlaceholderText(
        "Descrivi la stanza o incolla i risultati del calcolatore.\n"
        "Es.: «Stanza 5x4x2.7m con 2 finestre e 1 porta. "
        "Devo tinteggiare le pareti. Che quantita' di vernice?»\n\n"
        "Per fotogrammetria: descrivi la foto (oggetti di riferimento, distanza).");
    m_aiInput->setFixedHeight(100);
    aiVbox->addWidget(m_aiInput);

    auto* btnRow = new QHBoxLayout;
    m_btnAi = new QPushButton(
        QString::fromUtf8("\xf0\x9f\xa4\x96 Analizza con AI"), inner);
    m_btnAi->setMinimumHeight(44);
    m_btnStop = new QPushButton(
        QString::fromUtf8("\xe2\x8f\xb9 Stop"), inner);
    m_btnStop->setVisible(false);
    m_btnStop->setFixedWidth(72);
    btnRow->addWidget(m_btnAi, 1);
    btnRow->addWidget(m_btnStop);
    aiVbox->addLayout(btnRow);

    m_statusLbl = new QLabel("", inner);
    m_statusLbl->setVisible(false);
    aiVbox->addWidget(m_statusLbl);

    m_progress = new QProgressBar(inner);
    m_progress->setRange(0, 0);
    m_progress->setVisible(false);
    m_progress->setFixedHeight(4);
    aiVbox->addWidget(m_progress);

    m_aiOutput = new QTextEdit(inner);
    m_aiOutput->setReadOnly(true);
    m_aiOutput->setPlaceholderText("La risposta AI apparira' qui...");
    m_aiOutput->setMinimumHeight(160);
    aiVbox->addWidget(m_aiOutput);

    vbox->addWidget(aiGroup);

    /* ─────────────────────────────────────────────────────
       Sezione 4 — Planimetria touch-interattiva
       ───────────────────────────────────────────────────── */
    auto* planGroup = new QGroupBox(
        QString::fromUtf8("\xf0\x9f\x96\x8a\xef\xb8\x8f Planimetria Interattiva"), inner);
    auto* planVbox = new QVBoxLayout(planGroup);
    planVbox->setSpacing(6);

    /* Istruzioni */
    auto* planHint = new QLabel(
        QString::fromUtf8(
            "<small style='color:#8890a8'>"
            "\xf0\x9f\x91\x86 <b>Tocca</b> i punti del perimetro della stanza nell'ordine.<br>"
            "Il sistema calcola automaticamente l'area e il perimetro.<br>"
            "Ogni quadretto della griglia = la scala impostata.</small>"),
        planGroup);
    planHint->setTextFormat(Qt::RichText);
    planHint->setWordWrap(true);
    planVbox->addWidget(planHint);

    /* Scala */
    auto* scaleRow = new QHBoxLayout;
    scaleRow->addWidget(new QLabel(
        QString::fromUtf8("\xf0\x9f\x93\x8f Scala (m/quadretto):"), planGroup));
    m_planScaleSpin = new QDoubleSpinBox(planGroup);
    m_planScaleSpin->setRange(0.1, 5.0);
    m_planScaleSpin->setSingleStep(0.1);
    m_planScaleSpin->setValue(0.5);
    m_planScaleSpin->setDecimals(1);
    m_planScaleSpin->setSuffix(" m");
    scaleRow->addWidget(m_planScaleSpin);
    planVbox->addLayout(scaleRow);

    /* Canvas */
    m_planWidget = new RoomPlanWidget(planGroup);
    planVbox->addWidget(m_planWidget);

    /* Risultati */
    m_planResults = new QLabel(
        QString::fromUtf8("<small style='color:#8890a8'>Aggiungi almeno 3 punti per calcolare l'area.</small>"),
        planGroup);
    m_planResults->setTextFormat(Qt::RichText);
    m_planResults->setWordWrap(true);
    planVbox->addWidget(m_planResults);

    /* Pulsante reset */
    auto* planResetBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x97\x91 Cancella punti"), planGroup);
    planResetBtn->setObjectName("SecondaryBtn");
    planResetBtn->setMinimumHeight(44);
    planVbox->addWidget(planResetBtn);

    vbox->addWidget(planGroup);
    vbox->addStretch();

    scroll->setWidget(inner);

    auto* outerVbox = new QVBoxLayout(this);
    outerVbox->setContentsMargins(0, 0, 0, 0);
    outerVbox->addWidget(scroll);

    /* ── Connessioni ── */
    connect(btnCalc,   &QPushButton::clicked, this, &MisurePage::onCalcolaClicked);
    connect(m_btnAi,   &QPushButton::clicked, this, &MisurePage::onAiClicked);
    connect(m_btnStop, &QPushButton::clicked, m_ai, &AiClient::abort);
    connect(m_ai, &AiClient::token,    this, &MisurePage::onToken);
    connect(m_ai, &AiClient::finished, this, &MisurePage::onFinished);
    connect(m_ai, &AiClient::error,    this, &MisurePage::onError);
    connect(m_ai, &AiClient::aborted,  this, &MisurePage::onAborted);

    connect(m_planWidget, &RoomPlanWidget::planChanged,
            this, &MisurePage::onPlanChanged);
    connect(planResetBtn, &QPushButton::clicked,
            this, &MisurePage::onPlanResetClicked);
    connect(m_planScaleSpin,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MisurePage::onPlanScaleChanged);
}

/* ══════════════════════════════════════════════════════════════
   Calcolatore locale
   ══════════════════════════════════════════════════════════════ */
void MisurePage::onCalcolaClicked()
{
    calcola();
}

void MisurePage::calcola()
{
    const double L  = m_lungSpin->value();
    const double W  = m_largSpin->value();
    const double H  = m_altSpin->value();
    const double sc = m_sovrasSpin->value() / 100.0;

    const double areaFloor  = L * W;
    const double areaWalls  = 2.0 * (L + W) * H;
    const double areaCeil   = areaFloor;
    const double areaTotal  = areaWalls + areaCeil + areaFloor;
    const double volume     = L * W * H;

    /* Vernice: ~10 m² per litro (1 mano), 2 mani standard */
    const double litriPareti   = std::ceil(areaWalls  / 10.0 * 2.0);
    const double litriSoffitto = std::ceil(areaCeil   / 10.0 * 2.0);

    /* Piastrelle: area pavimento + scarto */
    const double mq_piastrelle = areaFloor * (1.0 + sc);

    /* Riscaldamento: volume × 40 W/m³ (stima) */
    const double wattRisc = volume * 40.0;

    QString res;
    res += QString::fromUtf8("\xf0\x9f\x93\x90 <b>Risultati</b><br>");
    res += QString("Pavimento: <b>%1 m\xc2\xb2</b><br>").arg(areaFloor, 0, 'f', 2);
    res += QString("Pareti tot.: <b>%1 m\xc2\xb2</b><br>").arg(areaWalls, 0, 'f', 2);
    res += QString("Soffitto: <b>%1 m\xc2\xb2</b><br>").arg(areaCeil, 0, 'f', 2);
    res += QString("Totale superfici: <b>%1 m\xc2\xb2</b><br>").arg(areaTotal, 0, 'f', 2);
    res += QString("Volume: <b>%1 m\xc2\xb3</b><br>").arg(volume, 0, 'f', 2);
    res += "<br>";
    res += QString::fromUtf8(
        "\xf0\x9f\x96\x8c\xef\xb8\x8f <b>Vernice pareti</b> (2 mani): <b>%1 L</b><br>")
           .arg(litriPareti, 0, 'f', 0);
    res += QString::fromUtf8(
        "\xf0\x9f\x96\x8c\xef\xb8\x8f <b>Vernice soffitto</b> (2 mani): <b>%1 L</b><br>")
           .arg(litriSoffitto, 0, 'f', 0);
    res += QString::fromUtf8(
        "\xf0\x9f\x9f\xa7 <b>Piastrelle pavimento</b> (+%1%% scarto): <b>%2 m\xc2\xb2</b><br>")
           .arg(int(m_sovrasSpin->value()))
           .arg(mq_piastrelle, 0, 'f', 2);
    res += QString::fromUtf8(
        "\xf0\x9f\x94\xa5 <b>Potenza riscaldamento</b> (stima): <b>%1 W</b><br>")
           .arg(wattRisc, 0, 'f', 0);

    m_resultLbl->setTextFormat(Qt::RichText);
    m_resultLbl->setText(res);

    /* Aggiorna la vista 3D */
    if (m_roomView)
        m_roomView->setDimensions(L, W, H);

    /* Pre-compila il campo AI con un riassunto utile */
    if (m_aiInput->toPlainText().trimmed().isEmpty()) {
        m_aiInput->setPlainText(
            QString("Stanza %1x%2x%3 m.\n"
                    "Pavimento: %4 m\xc2\xb2, Pareti: %5 m\xc2\xb2, Volume: %6 m\xc2\xb3.\n"
                    "Vernice pareti (2 mani): %7 L. Piastrelle: %8 m\xc2\xb2.")
            .arg(L, 0, 'f', 2).arg(W, 0, 'f', 2).arg(H, 0, 'f', 2)
            .arg(areaFloor, 0, 'f', 2).arg(areaWalls, 0, 'f', 2).arg(volume, 0, 'f', 2)
            .arg(litriPareti, 0, 'f', 0).arg(mq_piastrelle, 0, 'f', 2));
    }
}

/* ══════════════════════════════════════════════════════════════
   Analisi AI
   ══════════════════════════════════════════════════════════════ */
void MisurePage::onAiClicked()
{
    if (m_busy || m_ai->busy()) {
        m_aiOutput->setPlainText(
            QString::fromUtf8("\xe2\x9a\xa0\xef\xb8\x8f")
            + " L'AI sta elaborando. Attendi oppure premi Stop.");
        return;
    }
    const QString userMsg = m_aiInput->toPlainText().trimmed();
    if (userMsg.isEmpty()) {
        m_aiOutput->setPlainText(
            QString::fromUtf8("\xe2\x9a\xa0\xef\xb8\x8f")
            + " Inserisci una descrizione o usa prima il Calcolatore.");
        return;
    }

    static const char* kSysPrompts[] = {
        "Sei un esperto geometra italiano. Stima le dimensioni di stanza/ambiente "
        "dalla descrizione fornita. Fornisci una stima numerica dettagliata con range min-max. "
        "Rispondi in italiano.",

        "Sei un esperto di edilizia e ristrutturazioni italiane. In base alle misure/descrizione "
        "fornite, consiglia i materiali migliori per pavimenti, pareti e soffitto, con prezzi "
        "indicativi al m\xc2\xb2 per il mercato italiano. Rispondi in italiano.",

        "Sei un geometra/perito esperto di costi edilizi italiani. Calcola una stima del costo "
        "totale di ristrutturazione in base alle misure e al tipo di intervento descritto. "
        "Considera materiali, manodopera e IVA. Rispondi in italiano.",

        "Sei un esperto di fotogrammetria e misurazione da immagini. L'utente descrive una foto "
        "e vuole stimare le dimensioni degli oggetti o dello spazio. Guida l'utente nel processo "
        "di stima, identificando gli oggetti di riferimento e spiegando come calcolare le misure. "
        "Rispondi in italiano.",

        "Sei un esperto di normativa edilizia italiana (D.M. 5/7/1975, NTC 2018). Confronta le "
        "misure fornite con i requisiti minimi italiani per i vari ambienti (camera, cucina, "
        "bagno, etc.) e segnala eventuali non conformita'. Rispondi in italiano.",
    };

    const int idx = m_aiAction->currentIndex();
    const QString sys = (idx >= 0 && idx < 5) ? kSysPrompts[idx] : kSysPrompts[0];

    m_busy = true;
    m_aiOutput->clear();
    m_statusLbl->setText(QString::fromUtf8("\xe2\x8f\xb3") + " Analisi in corso...");
    m_statusLbl->setVisible(true);
    m_progress->setVisible(true);
    m_btnStop->setVisible(true);
    m_btnAi->setEnabled(false);
    m_ai->chat(sys, userMsg);
}

void MisurePage::onToken(const QString& t)
{
    if (!m_busy) return;
    m_aiOutput->moveCursor(QTextCursor::End);
    m_aiOutput->insertPlainText(t);
    m_aiOutput->verticalScrollBar()->setValue(m_aiOutput->verticalScrollBar()->maximum());
}

void MisurePage::onFinished(const QString&)
{
    if (!m_busy) return;
    m_busy = false;
    m_statusLbl->setVisible(false);
    m_progress->setVisible(false);
    m_btnStop->setVisible(false);
    m_btnAi->setEnabled(true);
}

void MisurePage::onError(const QString& e)
{
    if (!m_busy) return;
    m_busy = false;
    m_statusLbl->setVisible(false);
    m_progress->setVisible(false);
    m_btnStop->setVisible(false);
    m_btnAi->setEnabled(true);
    m_aiOutput->setPlainText(QString::fromUtf8("\xe2\x9d\x8c") + " Errore: " + e);
}

void MisurePage::onAborted()
{
    onFinished("");
}

/* ── Planimetria interattiva ─────────────────────────────────── */
void MisurePage::onPlanChanged(const QString& results)
{
    if (!m_planResults) return;
    if (results.isEmpty()) {
        m_planResults->setText(
            QString::fromUtf8("<small style='color:#8890a8'>Aggiungi almeno 3 punti per calcolare l'area.</small>"));
    } else {
        m_planResults->setText(results);
    }
}

void MisurePage::onPlanResetClicked()
{
    if (m_planWidget) m_planWidget->reset();
}

void MisurePage::onPlanScaleChanged(double v)
{
    if (m_planWidget) m_planWidget->setScaleMetersPerCell(v);
}
