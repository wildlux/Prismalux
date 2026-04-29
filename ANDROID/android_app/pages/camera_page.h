#pragma once
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QCamera>
#include <QMediaCaptureSession>
#include <QImageCapture>
#include <QVideoWidget>

class AiClient;

/* ══════════════════════════════════════════════════════════════
   CameraPage — scansione documento + invio testo a ChatPage.

   Flusso:
     1. Avvia preview camera (startCamera)
     2. L'utente inquadra il documento e preme "Scansiona"
     3. L'immagine viene catturata e inviata al modello vision
        (se disponibile: llama3.2-vision, qwen2-vl:7b)
        oppure mostrata come allegato testo nel prompt
     4. Il testo estratto viene emesso via textExtracted()
        e ChatPage lo inietta nel prossimo messaggio

   Se il modello attivo non supporta vision, mostra un avviso
   e permette di digitare manualmente il testo del documento.
   ══════════════════════════════════════════════════════════════ */
class CameraPage : public QWidget {
    Q_OBJECT
public:
    explicit CameraPage(AiClient* ai, QWidget* parent = nullptr);
    ~CameraPage() override;

    void startCamera();
    void stopCamera();

signals:
    /** Testo estratto dall'immagine pronto per ChatPage. */
    void textExtracted(const QString& text);

private slots:
    void onCapture();
    void onImageCaptured(int id, const QImage& img);
    void onAiToken(const QString& chunk);
    void onAiFinished(const QString& full);
    void onAiError(const QString& msg);

private:
    AiClient*              m_ai;
    QCamera*               m_camera         = nullptr;
    QMediaCaptureSession*  m_session         = nullptr;
    QImageCapture*         m_imageCapture    = nullptr;
    QVideoWidget*          m_viewfinder      = nullptr;

    QLabel*      m_statusLbl  = nullptr;
    QPushButton* m_captureBtn = nullptr;
    QPushButton* m_sendBtn    = nullptr;  ///< invia testo estratto a Chat
    QLabel*      m_previewLbl = nullptr;  ///< anteprima immagine catturata
    QLabel*      m_resultLbl  = nullptr;  ///< testo estratto (scroll)

    QString      m_extractedText;
    bool         m_processing = false;
};
