#include "camera_page.h"
#include "../ai_client.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QImageCapture>
#include <QMediaDevices>
#include <QCameraDevice>
#include <QBuffer>
#include <QByteArray>

/* ══════════════════════════════════════════════════════════════
   Costruttore
   ══════════════════════════════════════════════════════════════ */
CameraPage::CameraPage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    setObjectName("CameraPage");

    /* ── Viewfinder ── */
    m_viewfinder = new QVideoWidget(this);
    m_viewfinder->setObjectName("Viewfinder");
    m_viewfinder->setMinimumHeight(280);

    /* ── Status ── */
    m_statusLbl = new QLabel("Premi Scansiona per acquisire", this);
    m_statusLbl->setObjectName("StatusLabel");
    m_statusLbl->setWordWrap(true);
    m_statusLbl->setAlignment(Qt::AlignCenter);

    /* ── Anteprima cattura ── */
    m_previewLbl = new QLabel(this);
    m_previewLbl->setObjectName("PreviewLabel");
    m_previewLbl->setFixedHeight(120);
    m_previewLbl->setAlignment(Qt::AlignCenter);
    m_previewLbl->setVisible(false);

    /* ── Risultato OCR/Vision ── */
    auto* scroll = new QScrollArea(this);
    m_resultLbl  = new QLabel(this);
    m_resultLbl->setObjectName("ResultLabel");
    m_resultLbl->setWordWrap(true);
    m_resultLbl->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    scroll->setWidget(m_resultLbl);
    scroll->setWidgetResizable(true);
    scroll->setFixedHeight(120);
    scroll->setObjectName("ResultScroll");

    /* ── Pulsanti ── */
    m_captureBtn = new QPushButton("\xf0\x9f\x93\xb7  Scansiona", this);  // 📷
    m_captureBtn->setObjectName("PrimaryBtn");
    m_sendBtn = new QPushButton("\xe2\x9e\xa4  Invia a Chat", this);       // ➤
    m_sendBtn->setObjectName("SecondaryBtn");
    m_sendBtn->setEnabled(false);

    auto* btnRow = new QHBoxLayout;
    btnRow->addWidget(m_captureBtn);
    btnRow->addWidget(m_sendBtn);

    /* ── Layout principale ── */
    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(8, 8, 8, 8);
    vbox->setSpacing(8);
    vbox->addWidget(m_viewfinder, 1);
    vbox->addWidget(m_statusLbl);
    vbox->addWidget(m_previewLbl);
    vbox->addWidget(scroll);
    vbox->addLayout(btnRow);

    /* ── Camera Qt6 Multimedia ── */
    const QList<QCameraDevice> cameras = QMediaDevices::videoInputs();
    if (!cameras.isEmpty()) {
        m_camera        = new QCamera(cameras.first(), this);
        m_session       = new QMediaCaptureSession(this);
        m_imageCapture  = new QImageCapture(this);
        m_session->setCamera(m_camera);
        m_session->setVideoOutput(m_viewfinder);
        m_session->setImageCapture(m_imageCapture);
    } else {
        m_statusLbl->setText(
            "\xe2\x9a\xa0  Nessuna camera disponibile su questo dispositivo.");  // ⚠
        m_captureBtn->setEnabled(false);
    }

    /* ── Connessioni ── */
    connect(m_captureBtn, &QPushButton::clicked,
            this, &CameraPage::onCapture);
    connect(m_sendBtn, &QPushButton::clicked, this, [this] {
        if (!m_extractedText.isEmpty())
            emit textExtracted(m_extractedText);
    });
    if (m_imageCapture) {
        connect(m_imageCapture, &QImageCapture::imageCaptured,
                this, &CameraPage::onImageCaptured);
    }
    connect(m_ai, &AiClient::token,    this, &CameraPage::onAiToken);
    connect(m_ai, &AiClient::finished, this, &CameraPage::onAiFinished);
    connect(m_ai, &AiClient::error,    this, &CameraPage::onAiError);
}

CameraPage::~CameraPage()
{
    stopCamera();
}

/* ── startCamera / stopCamera ───────────────────────────────── */
void CameraPage::startCamera()
{
    if (m_camera && !m_camera->isActive())
        m_camera->start();
}

void CameraPage::stopCamera()
{
    if (m_camera && m_camera->isActive())
        m_camera->stop();
}

/* ── onCapture ───────────────────────────────────────────────── */
void CameraPage::onCapture()
{
    if (!m_imageCapture || m_processing) return;
    m_captureBtn->setEnabled(false);
    m_statusLbl->setText("Acquisizione in corso…");
    m_resultLbl->clear();
    m_extractedText.clear();
    m_sendBtn->setEnabled(false);
    m_imageCapture->capture();
}

/* ── onImageCaptured ─────────────────────────────────────────── */
void CameraPage::onImageCaptured(int /*id*/, const QImage& img)
{
    /* Anteprima ridotta */
    m_previewLbl->setPixmap(
        QPixmap::fromImage(img).scaledToHeight(120, Qt::SmoothTransformation));
    m_previewLbl->setVisible(true);

    /* Converti in JPEG base64 per invio al modello vision */
    QByteArray ba;
    QBuffer buf(&ba);
    buf.open(QIODevice::WriteOnly);
    img.scaled(800, 800, Qt::KeepAspectRatio, Qt::SmoothTransformation)
       .save(&buf, "JPEG", 80);
    const QString b64 = ba.toBase64();

    m_processing = true;
    m_statusLbl->setText("\xf0\x9f\xa4\x96  Analisi in corso…");  // 🤖

    /* System prompt per estrazione testo */
    const QString sys =
        "Sei un assistente OCR. Estrai tutto il testo visibile nell'immagine "
        "mantenendo la struttura originale. Se non c'è testo, descrivi "
        "brevemente cosa vedi. Rispondi SOLO con il testo estratto, "
        "senza commenti aggiuntivi.";

    m_ai->chatWithImage(sys,
        "Estrai tutto il testo da questa immagine.",
        ba,
        "image/jpeg");
}

/* ── onAiToken ───────────────────────────────────────────────── */
void CameraPage::onAiToken(const QString& chunk)
{
    m_extractedText += chunk;
    m_resultLbl->setText(m_extractedText);
}

/* ── onAiFinished ────────────────────────────────────────────── */
void CameraPage::onAiFinished(const QString& /*full*/)
{
    m_processing = false;
    m_captureBtn->setEnabled(true);
    m_sendBtn->setEnabled(!m_extractedText.isEmpty());
    m_statusLbl->setText(
        m_extractedText.isEmpty()
            ? "\xe2\x9a\xa0  Nessun testo estratto"
            : "\xe2\x9c\x85  Testo pronto — premi Invia a Chat");  // ✅
}

/* ── onAiError ───────────────────────────────────────────────── */
void CameraPage::onAiError(const QString& msg)
{
    m_processing = false;
    m_captureBtn->setEnabled(true);
    /* Il modello vision potrebbe non essere disponibile */
    m_statusLbl->setText(
        "\xe2\x9d\x8c  " + msg.left(120) +   // ❌
        "\n\xf0\x9f\x92\xa1  Prova: llama3.2-vision o qwen2-vl:7b");  // 💡
}
