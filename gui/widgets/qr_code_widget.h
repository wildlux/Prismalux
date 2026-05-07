#pragma once
#include <QWidget>
#include <QString>
#include <vector>

/* Widget che disegna un QR code a partire da una stringa URL. */
class QrCodeWidget : public QWidget {
    Q_OBJECT
public:
    explicit QrCodeWidget(const QString& text, QWidget* parent = nullptr);

    void setText(const QString& text);
    bool isValid() const { return m_size > 0; }
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent*) override;

private:
    void generate(const QString& text);

    int                  m_size   = 0;   /* lato QR in moduli */
    std::vector<uint8_t> m_qrcode;       /* bitmap qrcodegen */
};
