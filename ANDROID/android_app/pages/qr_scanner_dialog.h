#pragma once
#include <QDialog>
#include <QString>

#ifdef HAVE_MULTIMEDIA
#  include <QCamera>
#  include <QVideoSink>
#  include <QVideoWidget>
#  include <QMediaCaptureSession>
#endif

class QLabel;
class QPushButton;

/* --------------------------------------------------------------
   QrScannerDialog -- apre la fotocamera e scansiona un QR code.

   Quando un QR viene riconosciuto emette urlFound(url) e si chiude.
   Disponibile solo se Qt Multimedia ? presente (HAVE_MULTIMEDIA).
   -------------------------------------------------------------- */
class QrScannerDialog : public QDialog {
    Q_OBJECT
public:
    explicit QrScannerDialog(QWidget* parent = nullptr);
    ~QrScannerDialog() override;

signals:
    /** Emesso con il testo grezzo del QR code quando trovato. */
    void urlFound(const QString& url);

private slots:
    void onVideoFrameChanged();
    void onCancelClicked();

private:
#ifdef HAVE_MULTIMEDIA
    QCamera*              m_camera  = nullptr;
    QVideoSink*           m_sink    = nullptr;
    QVideoWidget*         m_view    = nullptr;
    QMediaCaptureSession  m_session;
#endif
    QLabel*      m_statusLbl = nullptr;
    QPushButton* m_cancelBtn = nullptr;

    bool m_found = false;

    /* Tenta di decodificare il frame corrente; ritorna stringa vuota se non trovato. */
    QString tryDecode(const QImage& img);

    /* Avvia la fotocamera dopo che il permesso ? stato concesso. */
    void startCamera();
};
