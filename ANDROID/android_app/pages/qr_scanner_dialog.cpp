/* ══════════════════════════════════════════════════════════════
   qr_scanner_dialog.cpp — scanner QR con fotocamera (Qt Multimedia)

   Flusso:
   1. Apre la fotocamera posteriore (o la prima disponibile)
   2. QVideoSink emette videoFrameChanged() ad ogni frame
   3. onVideoFrameChanged() converte il frame in QImage (scala a 640px)
      e chiama tryDecode() che usa ZXing-cpp
   4. Quando un QR viene trovato: emette urlFound(), chiude il dialog

   Compilato solo se HAVE_MULTIMEDIA è definito (Qt Multimedia disponibile)
   e HAVE_ZXING è definito (zxing-cpp disponibile).
   ══════════════════════════════════════════════════════════════ */
#include "qr_scanner_dialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QImage>
#include <QApplication>
#include <QCoreApplication>
#include <QPermission>

#ifdef HAVE_MULTIMEDIA
#  include <QVideoFrame>
#  include <QMediaDevices>
#  include <QCameraDevice>
#endif

#ifdef HAVE_ZXING
/* FetchContent: headers in core/src/ (no ZXing/ prefix).
   System-installed ZXing uses ZXing/ReadBarcode.h — handled by CMake include path. */
#  include <ReadBarcode.h>
#  include <ReaderOptions.h>
#  include <BarcodeFormat.h>
#endif

/* ── Costruttore ─────────────────────────────────────────────── */
QrScannerDialog::QrScannerDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("\xf0\x9f\x93\xb7  Scansiona QR Prismalux");  /* 📷 */
    setMinimumSize(320, 420);
    setAttribute(Qt::WA_DeleteOnClose);

    auto* vl = new QVBoxLayout(this);
    vl->setSpacing(8);
    vl->setContentsMargins(8, 8, 8, 8);

    auto* titleLbl = new QLabel(
        "<b>\xf0\x9f\x94\x91  Scansiona il QR sul PC</b><br>"
        "<small>Apri Prismalux Desktop \xe2\x86\x92 LAN & WAN \xe2\x86\x92 QR Connetti</small>",
        this);
    titleLbl->setTextFormat(Qt::RichText);
    titleLbl->setAlignment(Qt::AlignCenter);
    titleLbl->setWordWrap(true);
    vl->addWidget(titleLbl);

#ifdef HAVE_MULTIMEDIA
    m_view = new QVideoWidget(this);
    m_view->setMinimumHeight(260);
    m_view->setObjectName("QrViewfinder");
    vl->addWidget(m_view, 1);
#else
    auto* noHwLbl = new QLabel(
        "\xe2\x9a\xa0\xef\xb8\x8f  Fotocamera non disponibile in questa build.",
        this);
    noHwLbl->setAlignment(Qt::AlignCenter);
    noHwLbl->setWordWrap(true);
    vl->addWidget(noHwLbl, 1);
#endif

    m_statusLbl = new QLabel(
        "\xf0\x9f\x94\x84  Avvio fotocamera\xe2\x80\xa6", this);  /* 🔄 */
    m_statusLbl->setAlignment(Qt::AlignCenter);
    m_statusLbl->setWordWrap(true);
    vl->addWidget(m_statusLbl);

    m_cancelBtn = new QPushButton("Annulla", this);
    m_cancelBtn->setObjectName("SecondaryBtn");
    vl->addWidget(m_cancelBtn);

    connect(m_cancelBtn, &QPushButton::clicked, this, &QrScannerDialog::onCancelClicked);

#ifdef HAVE_MULTIMEDIA
    /* Richiesta permesso fotocamera a runtime (richiesto da Android 6+) */
    QCameraPermission camPerm;
    qApp->requestPermission(camPerm, this, [this](const QPermission& p) {
        if (p.status() == Qt::PermissionStatus::Granted) {
            startCamera();
        } else {
            m_statusLbl->setText(
                "\xe2\x9d\x8c  Permesso fotocamera negato.\n"
                "Vai in Impostazioni \xe2\x86\x92 App \xe2\x86\x92 PrismaluxMobile \xe2\x86\x92 Permessi.");
        }
    });
#endif
}

/* ── Distruttore ─────────────────────────────────────────────── */
QrScannerDialog::~QrScannerDialog()
{
#ifdef HAVE_MULTIMEDIA
    if (m_camera) m_camera->stop();
#endif
}

/* ── startCamera ─────────────────────────────────────────────── */
void QrScannerDialog::startCamera()
{
#ifdef HAVE_MULTIMEDIA
    QCameraDevice chosenDev;
    for (const QCameraDevice& dev : QMediaDevices::videoInputs()) {
        if (dev.position() == QCameraDevice::BackFace) { chosenDev = dev; break; }
    }
    if (chosenDev.isNull() && !QMediaDevices::videoInputs().isEmpty())
        chosenDev = QMediaDevices::videoInputs().first();

    if (chosenDev.isNull()) {
        m_statusLbl->setText("\xe2\x9d\x8c  Nessuna fotocamera trovata.");
        return;
    }

    m_camera = new QCamera(chosenDev, this);
    m_sink   = new QVideoSink(this);

    m_session.setCamera(m_camera);
    m_session.setVideoSink(m_sink);
    m_session.setVideoOutput(m_view);

    connect(m_sink, &QVideoSink::videoFrameChanged,
            this, &QrScannerDialog::onVideoFrameChanged);

    m_camera->start();
    m_statusLbl->setText(
        "\xf0\x9f\x94\x8d  Punta la fotocamera al QR sul PC\xe2\x80\xa6");  /* 🔍 */
#endif
}

/* ── onCancelClicked ─────────────────────────────────────────── */
void QrScannerDialog::onCancelClicked()
{
    reject();
}

/* ── onVideoFrameChanged ─────────────────────────────────────── */
void QrScannerDialog::onVideoFrameChanged()
{
    if (m_found) return;

#ifdef HAVE_MULTIMEDIA
    const QVideoFrame frame = m_sink->videoFrame();
    if (!frame.isValid()) return;

    /* Converte il frame in QImage (scala a max 640px per performance) */
    QImage img = frame.toImage();
    if (img.isNull()) return;
    if (img.width() > 640)
        img = img.scaledToWidth(640, Qt::FastTransformation);
    img = img.convertToFormat(QImage::Format_Grayscale8);

    const QString decoded = tryDecode(img);
    if (decoded.isEmpty()) return;

    m_found = true;
    m_camera->stop();
    m_statusLbl->setText("\xe2\x9c\x85  QR riconosciuto!");

    emit urlFound(decoded);
    QMetaObject::invokeMethod(this, &QrScannerDialog::accept, Qt::QueuedConnection);
#endif
}

/* ── tryDecode ───────────────────────────────────────────────── */
QString QrScannerDialog::tryDecode(const QImage& img)
{
#ifdef HAVE_ZXING
    using namespace ZXing;
    ReaderOptions opts;
    opts.setFormats(BarcodeFormat::QRCode);
    opts.setTryHarder(false);

    ImageView iv{
        img.bits(),
        img.width(),
        img.height(),
        ImageFormat::Lum
    };

    const auto result = ReadBarcode(iv, opts);
    if (result.isValid())
        return QString::fromStdString(result.text());
#else
    Q_UNUSED(img)
#endif
    return {};
}
