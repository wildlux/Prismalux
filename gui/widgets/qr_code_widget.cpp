#include "qr_code_widget.h"
#include "../qrcodegen.h"
#include <QPainter>
#include <QPaintEvent>
#include <cstring>

QrCodeWidget::QrCodeWidget(const QString& text, QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(200, 200);
    generate(text);
}

void QrCodeWidget::setText(const QString& text)
{
    generate(text);
    update();
}

void QrCodeWidget::generate(const QString& text)
{
    m_size   = 0;
    m_qrcode.clear();

    if (text.trimmed().isEmpty()) return;

    const QByteArray utf8 = text.toUtf8();
    const size_t bufLen   = qrcodegen_BUFFER_LEN_MAX;

    m_qrcode.resize(bufLen * 2);
    uint8_t* tempBuf = m_qrcode.data();
    uint8_t* qrcode  = m_qrcode.data() + bufLen;

    const bool ok = qrcodegen_encodeText(
        utf8.constData(), tempBuf, qrcode,
        qrcodegen_Ecc_MEDIUM,
        qrcodegen_VERSION_MIN, qrcodegen_VERSION_MAX,
        qrcodegen_Mask_AUTO, true);

    if (ok)
        m_size = qrcodegen_getSize(qrcode);
    else
        m_qrcode.clear();
}

void QrCodeWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.fillRect(rect(), Qt::white);

    if (m_size <= 0 || m_qrcode.empty()) {
        p.setPen(Qt::red);
        p.drawText(rect(), Qt::AlignCenter, "QR error");
        return;
    }

    const uint8_t* qrcode = m_qrcode.data() + qrcodegen_BUFFER_LEN_MAX;

    /* Margine silenzioso: 4 moduli su ogni lato (spec ISO 18004) */
    const int border  = 4;
    const int total   = m_size + border * 2;
    const int side    = qMin(width(), height());
    const double cell = static_cast<double>(side) / total;

    const double offX = (width()  - total * cell) / 2.0;
    const double offY = (height() - total * cell) / 2.0;

    p.fillRect(QRectF(offX, offY, total * cell, total * cell), Qt::white);

    p.setBrush(Qt::black);
    p.setPen(Qt::NoPen);
    for (int y = 0; y < m_size; y++) {
        for (int x = 0; x < m_size; x++) {
            if (qrcodegen_getModule(qrcode, x, y)) {
                const double px = offX + (border + x) * cell;
                const double py = offY + (border + y) * cell;
                p.drawRect(QRectF(px, py, cell, cell));
            }
        }
    }
}

QSize QrCodeWidget::sizeHint() const
{
    return QSize(280, 280);
}
