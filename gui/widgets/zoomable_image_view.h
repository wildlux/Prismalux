#pragma once
#include <QWidget>
#include <QPixmap>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QCursor>

/* Visualizzatore immagine con zoom (rotella) e pan (drag).
   Doppio click → reset vista. */
class ZoomableImageView : public QWidget
{
public:
    explicit ZoomableImageView(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setMinimumHeight(180);
        setStyleSheet("background:#0f172a;border-radius:8px;");
        setCursor(Qt::OpenHandCursor);
        setMouseTracking(true);
    }

    void setPixmap(const QPixmap& px)
    {
        m_px = px;
        resetView();
        update();
    }

    bool hasPixmap() const { return !m_px.isNull(); }

    void resetView()
    {
        if (m_px.isNull()) return;
        /* Scala l'immagine per adattarla al widget mantenendo le proporzioni */
        double sw = double(width())  / m_px.width();
        double sh = double(height()) / m_px.height();
        m_scale = qMin(sw, sh) * 0.98;
        m_origin = QPointF(
            (width()  - m_px.width()  * m_scale) / 2.0,
            (height() - m_px.height() * m_scale) / 2.0);
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        p.fillRect(rect(), QColor("#0f172a"));
        if (m_px.isNull()) {
            p.setPen(QColor("#475569"));
            p.drawText(rect(), Qt::AlignCenter,
                       "Grafico non disponibile — esegui il benchmark.");
            return;
        }
        p.translate(m_origin);
        p.scale(m_scale, m_scale);
        p.drawPixmap(0, 0, m_px);
    }

    void wheelEvent(QWheelEvent* e) override
    {
        if (m_px.isNull()) return;
        const double factor = (e->angleDelta().y() > 0) ? 1.15 : (1.0 / 1.15);
        /* Zoom centrato sul cursore */
        QPointF cursor = e->position();
        m_origin = cursor - factor * (cursor - m_origin);
        m_scale *= factor;
        m_scale = qBound(0.05, m_scale, 20.0);
        update();
        e->accept();
    }

    void mousePressEvent(QMouseEvent* e) override
    {
        if (e->button() == Qt::LeftButton) {
            m_dragging = true;
            m_lastPos = e->pos();
            setCursor(Qt::ClosedHandCursor);
        }
    }

    void mouseMoveEvent(QMouseEvent* e) override
    {
        if (m_dragging) {
            m_origin += e->pos() - m_lastPos;
            m_lastPos = e->pos();
            update();
        }
    }

    void mouseReleaseEvent(QMouseEvent* e) override
    {
        if (e->button() == Qt::LeftButton) {
            m_dragging = false;
            setCursor(Qt::OpenHandCursor);
        }
    }

    void mouseDoubleClickEvent(QMouseEvent*) override
    {
        resetView();
    }

    void resizeEvent(QResizeEvent*) override
    {
        resetView();
    }

private:
    QPixmap  m_px;
    double   m_scale  = 1.0;
    QPointF  m_origin;
    bool     m_dragging = false;
    QPoint   m_lastPos;
};
