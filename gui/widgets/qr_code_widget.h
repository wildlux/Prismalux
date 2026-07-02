#pragma once
#include <QWidget>
#include <QString>
#include <QImage>
#include <vector>

/* Widget che disegna un QR code a partire da una stringa URL. */
class QrCodeWidget : public QWidget {
    Q_OBJECT
public:
    explicit QrCodeWidget(const QString& text, QWidget* parent = nullptr);

    void setText(const QString& text);
    bool isValid() const { return m_size > 0; }
    QSize sizeHint() const override;

    /** Renderizza il QR direttamente in una QImage (nessun widget/finestra
     *  necessaria) — usata per salvare su file/embeddare in HTML (es. bolle
     *  chat). moduleSizePx = lato di un modulo QR in pixel. Ritorna
     *  un'immagine nulla se il testo non è codificabile. */
    static QImage renderImage(const QString& text, int moduleSizePx = 8);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    void generate(const QString& text);

    int                  m_size   = 0;   /* lato QR in moduli */
    std::vector<uint8_t> m_qrcode;       /* bitmap qrcodegen */
};
