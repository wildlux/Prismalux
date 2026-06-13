#include "chart_page.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QGestureEvent>
#include <QPinchGesture>
#include <QCoreApplication>
#include <QFont>
#include <QFontMetrics>
#include <cmath>
#include <limits>

/* ═══════════════════════════════════════════════════════════════
   FormulaEval — parser ricorsivo per espressioni con variabile x
   Supporta: +  -  *  /  ^  sin  cos  tan  sqrt  exp  log
             abs  floor  ceil  pi  e
   ═══════════════════════════════════════════════════════════════ */

FormulaEval::FormulaEval(const QString& expr)
    : m_expr(expr.toLower().simplified()), m_pos(0)
{
    m_expr.remove(' ');
}

void FormulaEval::skipWs()
{
    while (m_pos < m_expr.size() && m_expr[m_pos].isSpace())
        ++m_pos;
}

QChar FormulaEval::peek()
{
    if (m_pos >= m_expr.size()) return QChar(0);
    return m_expr[m_pos];
}

QChar FormulaEval::consume()
{
    if (m_pos >= m_expr.size()) { m_error = true; m_errMsg = "End of expression"; return QChar(0); }
    return m_expr[m_pos++];
}

QString FormulaEval::readName()
{
    QString name;
    while (m_pos < m_expr.size() && (m_expr[m_pos].isLetter() || m_expr[m_pos] == '_'))
        name += m_expr[m_pos++];
    return name;
}

double FormulaEval::readNumber()
{
    int start = m_pos;
    if (m_pos < m_expr.size() && (peek() == '+' || peek() == '-')) ++m_pos;
    while (m_pos < m_expr.size() && (m_expr[m_pos].isDigit() || m_expr[m_pos] == '.'))
        ++m_pos;
    if (m_pos < m_expr.size() && m_expr[m_pos] == 'e') {
        ++m_pos;
        if (m_pos < m_expr.size() && (m_expr[m_pos] == '+' || m_expr[m_pos] == '-'))
            ++m_pos;
        while (m_pos < m_expr.size() && m_expr[m_pos].isDigit())
            ++m_pos;
    }
    const QString sub = m_expr.mid(start, m_pos - start);
    bool ok;
    double v = sub.toDouble(&ok);
    if (!ok) { m_error = true; m_errMsg = "Numero non valido: " + sub; }
    return v;
}

double FormulaEval::parsePrimary()
{
    skipWs();
    if (m_error) return 0;

    if (peek() == '(') {
        consume();
        double v = parseExpr();
        if (peek() == ')') consume();
        else { m_error = true; m_errMsg = "Parentesi non chiusa"; }
        return v;
    }

    if (peek() == '-') { consume(); return -parsePrimary(); }
    if (peek() == '+') { consume(); return  parsePrimary(); }

    if (peek().isLetter()) {
        const QString name = readName();
        if (name == "x")     return m_x;
        if (name == "pi")    return M_PI;
        if (name == "e")     return M_E;

        /* funzione(arg) */
        if (peek() == '(') {
            consume();
            double arg = parseExpr();
            if (peek() == ')') consume();
            else { m_error = true; m_errMsg = "Parentesi non chiusa dopo " + name; return 0; }

            if (name == "sin")   return std::sin(arg);
            if (name == "cos")   return std::cos(arg);
            if (name == "tan")   return std::tan(arg);
            if (name == "sqrt")  return std::sqrt(arg);
            if (name == "exp")   return std::exp(arg);
            if (name == "log")   return std::log(arg);
            if (name == "log10") return std::log10(arg);
            if (name == "log2")  return std::log2(arg);
            if (name == "abs")   return std::abs(arg);
            if (name == "floor") return std::floor(arg);
            if (name == "ceil")  return std::ceil(arg);
            if (name == "round") return std::round(arg);
            if (name == "sign")  return (arg > 0) ? 1.0 : (arg < 0) ? -1.0 : 0.0;
            if (name == "sinh")  return std::sinh(arg);
            if (name == "cosh")  return std::cosh(arg);
            if (name == "tanh")  return std::tanh(arg);
            if (name == "asin")  return std::asin(arg);
            if (name == "acos")  return std::acos(arg);
            if (name == "atan")  return std::atan(arg);

            m_error = true;
            m_errMsg = "Funzione sconosciuta: " + name;
            return 0;
        }
        m_error = true;
        m_errMsg = "Token sconosciuto: " + name;
        return 0;
    }

    if (peek().isDigit() || peek() == '.') return readNumber();

    m_error = true;
    m_errMsg = QString("Token inatteso: '%1'").arg(peek());
    return 0;
}

double FormulaEval::parsePow()
{
    double base = parsePrimary();
    if (m_error) return 0;
    skipWs();
    if (peek() == '^') { consume(); return std::pow(base, parsePow()); }
    return base;
}

double FormulaEval::parseTerm()
{
    double left = parsePow();
    if (m_error) return 0;
    skipWs();
    while (!m_error && (peek() == '*' || peek() == '/')) {
        const QChar op = consume();
        const double right = parsePow();
        if (op == '*') left *= right;
        else {
            if (std::abs(right) < 1e-300) return std::numeric_limits<double>::quiet_NaN();
            left /= right;
        }
        skipWs();
    }
    return left;
}

double FormulaEval::parseExpr()
{
    double left = parseTerm();
    if (m_error) return 0;
    skipWs();
    while (!m_error && (peek() == '+' || peek() == '-')) {
        const QChar op = consume();
        const double right = parseTerm();
        left = (op == '+') ? left + right : left - right;
        skipWs();
    }
    return left;
}

double FormulaEval::eval(double x)
{
    m_x   = x;
    m_pos = 0;
    m_error = false;
    m_errMsg.clear();
    const double v = parseExpr();
    if (!m_error && m_pos < m_expr.size()) {
        m_error = true;
        m_errMsg = QString("Caratteri extra: '%1'").arg(m_expr.mid(m_pos));
    }
    return v;
}

/* ═══════════════════════════════════════════════════════════════
   ChartCanvas — implementazione
   ═══════════════════════════════════════════════════════════════ */

ChartCanvas::ChartCanvas(QWidget* parent) : QWidget(parent)
{
    setMinimumSize(200, 200);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAutoFillBackground(false);
    grabGesture(Qt::PinchGesture);
    setMouseTracking(false);
}

void ChartCanvas::zoom(double factor)
{
    const double cx = (m_xMin + m_xMax) / 2.0;
    const double cy = (m_yMin + m_yMax) / 2.0;
    const double hw = (m_xMax - m_xMin) / 2.0 * factor;
    const double hh = (m_yMax - m_yMin) / 2.0 * factor;
    m_xMin = cx - hw; m_xMax = cx + hw;
    m_yMin = cy - hh; m_yMax = cy + hh;
    resampleCurve();
    update();
}

void ChartCanvas::resetView()
{
    m_xMin = -8.0; m_xMax = 8.0;
    m_yMin = -6.0; m_yMax = 6.0;
    resampleCurve();
    update();
}

void ChartCanvas::setXRange(double xMin, double xMax)
{
    m_xMin = xMin; m_xMax = xMax;
    resampleCurve();
    update();
}

void ChartCanvas::setFormula(const QString& formula, bool autoScale)
{
    m_formula    = formula;
    m_evalError.clear();
    resampleCurve();

    if (autoScale && !m_curve.isEmpty()) {
        double yMin = std::numeric_limits<double>::max();
        double yMax = std::numeric_limits<double>::lowest();
        for (const QPointF& p : m_curve) {
            if (std::isfinite(p.y())) {
                yMin = std::min(yMin, p.y());
                yMax = std::max(yMax, p.y());
            }
        }
        if (yMax > yMin) {
            const double margin = (yMax - yMin) * 0.1;
            m_yMin = yMin - margin;
            m_yMax = yMax + margin;
        }
    }
    update();
}

void ChartCanvas::setScatter(const QVector<QPointF>& pts)
{
    m_scatter = pts;
    update();
}

void ChartCanvas::clearScatter()
{
    m_scatter.clear();
    update();
}

void ChartCanvas::resampleCurve()
{
    m_curve.clear();
    if (m_formula.trimmed().isEmpty()) return;

    const int N = 600;
    FormulaEval ev(m_formula);
    bool firstError = true;
    for (int i = 0; i <= N; ++i) {
        const double x = m_xMin + (m_xMax - m_xMin) * i / N;
        const double y = ev.eval(x);
        if (ev.hasError()) {
            if (firstError) {
                m_evalError = ev.errorMsg();
                emit plotError(m_evalError);
                firstError = false;
            }
            break;
        }
        if (std::isfinite(y))
            m_curve.append(QPointF(x, y));
        else
            m_curve.append(QPointF(x, std::numeric_limits<double>::quiet_NaN()));
    }
}

QPoint ChartCanvas::worldToScreen(double wx, double wy) const
{
    const int W = width(), H = height();
    const int margin = 40;
    const int pw = W - 2 * margin, ph = H - 2 * margin;
    if (pw <= 0 || ph <= 0) return {};
    const int sx = margin + int((wx - m_xMin) / (m_xMax - m_xMin) * pw);
    const int sy = margin + int((m_yMax - wy) / (m_yMax - m_yMin) * ph);
    return {sx, sy};
}

QPointF ChartCanvas::screenToWorld(const QPoint& p) const
{
    const int W = width(), H = height();
    const int margin = 40;
    const int pw = W - 2 * margin, ph = H - 2 * margin;
    if (pw <= 0 || ph <= 0) return {};
    const double wx = m_xMin + (p.x() - margin) * (m_xMax - m_xMin) / pw;
    const double wy = m_yMax - (p.y() - margin) * (m_yMax - m_yMin) / ph;
    return {wx, wy};
}

void ChartCanvas::drawGrid(QPainter& p)
{
    p.setPen(QPen(QColor(55, 65, 80), 1, Qt::DotLine));

    /* griglia automatica: ~8 linee per asse */
    const double xStep = std::pow(10, std::floor(std::log10((m_xMax - m_xMin) / 8)));
    const double yStep = std::pow(10, std::floor(std::log10((m_yMax - m_yMin) / 8)));

    for (double x = std::ceil(m_xMin / xStep) * xStep; x <= m_xMax; x += xStep) {
        const QPoint a = worldToScreen(x, m_yMin);
        const QPoint b = worldToScreen(x, m_yMax);
        p.drawLine(a, b);
    }
    for (double y = std::ceil(m_yMin / yStep) * yStep; y <= m_yMax; y += yStep) {
        const QPoint a = worldToScreen(m_xMin, y);
        const QPoint b = worldToScreen(m_xMax, y);
        p.drawLine(a, b);
    }
}

void ChartCanvas::drawAxes(QPainter& p)
{
    /* assi X e Y in bianco */
    p.setPen(QPen(QColor(160, 174, 192), 1));

    if (m_yMin <= 0 && m_yMax >= 0) {
        const QPoint l = worldToScreen(m_xMin, 0);
        const QPoint r = worldToScreen(m_xMax, 0);
        p.drawLine(l, r);
    }
    if (m_xMin <= 0 && m_xMax >= 0) {
        const QPoint t = worldToScreen(0, m_yMax);
        const QPoint b = worldToScreen(0, m_yMin);
        p.drawLine(t, b);
    }

    /* etichette asse X (ogni passo griglia) */
    p.setPen(QColor(100, 116, 139));
    QFont fnt = p.font();
    fnt.setPointSize(8);
    p.setFont(fnt);

    const double xStep = std::pow(10, std::floor(std::log10((m_xMax - m_xMin) / 8)));
    for (double x = std::ceil(m_xMin / xStep) * xStep; x <= m_xMax; x += xStep) {
        const QPoint pt = worldToScreen(x, m_yMin);
        const QString lbl = QString::number(x, 'g', 3);
        p.drawText(pt.x() - 15, pt.y() + 18, 30, 16, Qt::AlignCenter, lbl);
    }
    const double yStep = std::pow(10, std::floor(std::log10((m_yMax - m_yMin) / 8)));
    for (double y = std::ceil(m_yMin / yStep) * yStep; y <= m_yMax; y += yStep) {
        const QPoint pt = worldToScreen(m_xMin, y);
        const QString lbl = QString::number(y, 'g', 3);
        p.drawText(2, pt.y() - 8, 34, 16, Qt::AlignRight, lbl);
    }
}

void ChartCanvas::drawCurve(QPainter& p)
{
    if (m_curve.isEmpty()) return;

    const QPen curvePen(QColor(99, 179, 237), 2.5);
    p.setPen(curvePen);
    p.setRenderHint(QPainter::Antialiasing);

    QPainterPath path;
    bool penDown = false;
    for (const QPointF& pt : m_curve) {
        if (!std::isfinite(pt.y())) { penDown = false; continue; }
        const QPoint sp = worldToScreen(pt.x(), pt.y());
        if (!penDown) { path.moveTo(sp); penDown = true; }
        else path.lineTo(sp);
    }
    p.drawPath(path);
}

void ChartCanvas::drawScatter(QPainter& p)
{
    if (m_scatter.isEmpty()) return;
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(252, 129, 74));
    p.setRenderHint(QPainter::Antialiasing);
    for (const QPointF& pt : m_scatter) {
        const QPoint sp = worldToScreen(pt.x(), pt.y());
        p.drawEllipse(sp, 4, 4);
    }
}

void ChartCanvas::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(15, 23, 42));   /* sfondo scuro */

    drawGrid(p);
    drawAxes(p);
    drawCurve(p);
    drawScatter(p);

    /* formula e errore */
    p.setPen(QColor(148, 163, 184));
    QFont fnt = p.font();
    fnt.setPointSize(9);
    p.setFont(fnt);
    if (!m_evalError.isEmpty())
        p.drawText(rect().adjusted(8, 8, -8, -8),
                   Qt::AlignTop | Qt::AlignLeft,
                   "\xe2\x9d\x8c " + m_evalError);
    else if (!m_formula.isEmpty())
        p.drawText(rect().adjusted(8, 8, -8, -8),
                   Qt::AlignTop | Qt::AlignLeft,
                   "y = " + m_formula);
}

void ChartCanvas::wheelEvent(QWheelEvent* e)
{
    const double factor = (e->angleDelta().y() > 0) ? 0.85 : 1.0 / 0.85;
    const QPointF center = screenToWorld(e->position().toPoint());
    m_xMin = center.x() + (m_xMin - center.x()) * factor;
    m_xMax = center.x() + (m_xMax - center.x()) * factor;
    m_yMin = center.y() + (m_yMin - center.y()) * factor;
    m_yMax = center.y() + (m_yMax - center.y()) * factor;
    resampleCurve();
    update();
    e->accept();
}

void ChartCanvas::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton) {
        m_dragging   = true;
        m_dragOrigin = e->pos();
        m_dragXMin   = m_xMin; m_dragXMax = m_xMax;
        m_dragYMin   = m_yMin; m_dragYMax = m_yMax;
    }
}

void ChartCanvas::mouseMoveEvent(QMouseEvent* e)
{
    if (!m_dragging) return;
    const QPoint delta = e->pos() - m_dragOrigin;
    const int margin = 40;
    const int pw = width()  - 2 * margin;
    const int ph = height() - 2 * margin;
    if (pw <= 0 || ph <= 0) return;
    const double dx = -delta.x() * (m_dragXMax - m_dragXMin) / pw;
    const double dy =  delta.y() * (m_dragYMax - m_dragYMin) / ph;
    m_xMin = m_dragXMin + dx; m_xMax = m_dragXMax + dx;
    m_yMin = m_dragYMin + dy; m_yMax = m_dragYMax + dy;
    resampleCurve();
    update();
}

void ChartCanvas::mouseReleaseEvent(QMouseEvent*)
{
    m_dragging = false;
}

bool ChartCanvas::event(QEvent* e)
{
    if (e->type() == QEvent::Gesture) {
        auto* ge = static_cast<QGestureEvent*>(e);
        if (auto* pg = static_cast<QPinchGesture*>(ge->gesture(Qt::PinchGesture))) {
            if (pg->state() == Qt::GestureStarted) {
                m_pinchScale0 = 1.0;
                m_pinchXMin0  = m_xMin; m_pinchXMax0 = m_xMax;
                m_pinchYMin0  = m_yMin; m_pinchYMax0 = m_yMax;
            }
            const double scale   = pg->totalScaleFactor();
            if (scale > 0.01) {
                const double xCtr = (m_pinchXMin0 + m_pinchXMax0) / 2.0;
                const double yCtr = (m_pinchYMin0 + m_pinchYMax0) / 2.0;
                const double hx   = (m_pinchXMax0 - m_pinchXMin0) / 2.0 / scale;
                const double hy   = (m_pinchYMax0 - m_pinchYMin0) / 2.0 / scale;
                m_xMin = xCtr - hx; m_xMax = xCtr + hx;
                m_yMin = yCtr - hy; m_yMax = yCtr + hy;
                resampleCurve();
                update();
            }
            ge->accept();
            return true;
        }
    }
    return QWidget::event(e);
}

/* ═══════════════════════════════════════════════════════════════
   ChartPage — implementazione
   ═══════════════════════════════════════════════════════════════ */

ChartPage::ChartPage(AiClient* /*ai*/, QWidget* parent) : QWidget(parent)
{
    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(8, 8, 8, 8);
    vbox->setSpacing(6);

    /* ── Header ─────────────────────────────────── */
    auto* hdr = new QWidget(this);
    auto* hdrLay = new QHBoxLayout(hdr);
    hdrLay->setContentsMargins(0, 0, 0, 0);
    hdrLay->setSpacing(6);

    m_formulaEdit = new QLineEdit(this);
    m_formulaEdit->setObjectName("chatInput");
    m_formulaEdit->setPlaceholderText("y = f(x)  es: sin(x)/x   x^2   cos(x)*exp(-x/4)");
    m_formulaEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    {
        QFont f = m_formulaEdit->font();
        f.setPointSize(13);
        m_formulaEdit->setFont(f);
    }

    m_plotBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x93\x88") + " Disegna", this);   /* 📈 */
    m_plotBtn->setObjectName("primaryBtn");
    m_plotBtn->setFixedHeight(40);

    m_resetBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x94\x84"), this);   /* 🔄 */
    m_resetBtn->setObjectName("actionBtn");
    m_resetBtn->setFixedSize(40, 40);
    m_resetBtn->setToolTip("Ripristina vista");

    m_zoomInBtn = new QPushButton("+", this);
    m_zoomInBtn->setObjectName("actionBtn");
    m_zoomInBtn->setFixedSize(40, 40);

    m_zoomOutBtn = new QPushButton("\xe2\x80\x93", this);  /* – */
    m_zoomOutBtn->setObjectName("actionBtn");
    m_zoomOutBtn->setFixedSize(40, 40);

    hdrLay->addWidget(m_formulaEdit, 1);
    hdrLay->addWidget(m_plotBtn);
    hdrLay->addWidget(m_zoomInBtn);
    hdrLay->addWidget(m_zoomOutBtn);
    hdrLay->addWidget(m_resetBtn);
    vbox->addWidget(hdr);

    /* ── Canvas ──────────────────────────────────── */
    m_canvas = new ChartCanvas(this);
    m_canvas->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    vbox->addWidget(m_canvas, 1);

    /* ── Status ──────────────────────────────────── */
    m_statusLbl = new QLabel(
        QString::fromUtf8("\xf0\x9f\x91\x88")
        + "  Trascina per spostarti \xc2\xb7 Pizzica/rotella per zoom",
        this);
    m_statusLbl->setObjectName("cardDesc");
    m_statusLbl->setAlignment(Qt::AlignCenter);
    vbox->addWidget(m_statusLbl);

    /* connessioni */
    connect(m_plotBtn,    &QPushButton::clicked,  this, &ChartPage::onPlotClicked);
    connect(m_resetBtn,   &QPushButton::clicked,  this, &ChartPage::onResetClicked);
    connect(m_zoomInBtn,  &QPushButton::clicked,  this, &ChartPage::onZoomIn);
    connect(m_zoomOutBtn, &QPushButton::clicked,  this, &ChartPage::onZoomOut);
    connect(m_formulaEdit, &QLineEdit::returnPressed, this, &ChartPage::onPlotClicked);
    connect(m_canvas, &ChartCanvas::plotError, this, &ChartPage::onPlotError);
}

void ChartPage::onPlotClicked()
{
    const QString formula = m_formulaEdit->text().trimmed();
    if (formula.isEmpty()) return;
    m_statusLbl->setText(
        QString::fromUtf8("\xf0\x9f\x93\x88")
        + "  y = " + formula);
    m_canvas->setFormula(formula);
}

void ChartPage::onResetClicked()
{
    m_canvas->resetView();
}

void ChartPage::onZoomIn()
{
    m_canvas->zoom(0.75);
}

void ChartPage::onZoomOut()
{
    m_canvas->zoom(1.0 / 0.75);
}

void ChartPage::onPlotError(const QString& msg)
{
    m_statusLbl->setText(
        QString::fromUtf8("\xe2\x9d\x8c  Errore formula: ") + msg);
}

void ChartPage::showFormula(const QString& formula)
{
    m_formulaEdit->setText(formula);
    onPlotClicked();
}

void ChartPage::showScatter(const QVector<QPointF>& pts)
{
    m_canvas->setScatter(pts);
}
