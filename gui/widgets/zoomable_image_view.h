#pragma once
#include <QWidget>
#include <QPixmap>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QCursor>
#ifdef HAVE_SVG
#  include <QtSvg/QSvgRenderer>
#endif

/* Visualizzatore immagine con zoom (rotella) e pan (drag).
   Supporta PNG (setPixmap) e SVG vettoriale (setSvgFile, richiede HAVE_SVG).
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
    }

    void setPixmap(const QPixmap& px)
    {
#ifdef HAVE_SVG
        m_svgRenderer.reset();
#endif
        m_px = px;
        resetView();
        update();
    }

    void setSvgFile(const QString& path)
    {
#ifdef HAVE_SVG
        m_svgRenderer = std::make_unique<QSvgRenderer>(path);
        m_px = QPixmap();
        resetView();
        update();
#else
        Q_UNUSED(path)
#endif
    }

    bool hasContent() const
    {
#ifdef HAVE_SVG
        if (m_svgRenderer && m_svgRenderer->isValid()) return true;
#endif
        return !m_px.isNull();
    }

    void resetView()
    {
        QSizeF sz = naturalSize();
        if (sz.isEmpty() || width() == 0 || height() == 0) return;
        double sw = width()  / sz.width();
        double sh = height() / sz.height();
        m_scale  = qMin(sw, sh) * 0.98;
        m_origin = QPointF((width()  - sz.width()  * m_scale) / 2.0,
                           (height() - sz.height() * m_scale) / 2.0);
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
        p.fillRect(rect(), QColor("#0f172a"));

#ifdef HAVE_SVG
        if (m_svgRenderer && m_svgRenderer->isValid()) {
            p.translate(m_origin);
            p.scale(m_scale, m_scale);
            QSizeF sz = m_svgRenderer->defaultSize();
            m_svgRenderer->render(&p, QRectF(QPointF(0,0), sz));
            return;
        }
#endif
        if (!m_px.isNull()) {
            p.translate(m_origin);
            p.scale(m_scale, m_scale);
            p.drawPixmap(0, 0, m_px);
            return;
        }
        p.setPen(QColor("#475569"));
        p.drawText(rect(), Qt::AlignCenter,
                   "Grafico non disponibile — esegui il benchmark.");
    }

    void wheelEvent(QWheelEvent* e) override
    {
        if (!hasContent()) return;
        const double factor = (e->angleDelta().y() > 0) ? 1.15 : (1.0 / 1.15);
        QPointF cursor = e->position();
        m_origin = cursor - factor * (cursor - m_origin);
        m_scale  = qBound(0.05, m_scale * factor, 40.0);
        update();
        e->accept();
    }

    void mousePressEvent(QMouseEvent* e) override
    {
        if (e->button() == Qt::LeftButton) {
            m_dragging = true;
            m_lastPos  = e->pos();
            setCursor(Qt::ClosedHandCursor);
        }
    }

    void mouseMoveEvent(QMouseEvent* e) override
    {
        if (m_dragging) {
            m_origin += e->pos() - m_lastPos;
            m_lastPos  = e->pos();
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

    void mouseDoubleClickEvent(QMouseEvent*) override { resetView(); }

    void resizeEvent(QResizeEvent*) override { resetView(); }

private:
    QSizeF naturalSize() const
    {
#ifdef HAVE_SVG
        if (m_svgRenderer && m_svgRenderer->isValid())
            return m_svgRenderer->defaultSize();
#endif
        return m_px.size();
    }

    QPixmap  m_px;
    double   m_scale    = 1.0;
    QPointF  m_origin;
    bool     m_dragging = false;
    QPoint   m_lastPos;
#ifdef HAVE_SVG
    std::unique_ptr<QSvgRenderer> m_svgRenderer;
#endif
};
