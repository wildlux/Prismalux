#pragma once
#include <QWidget>

class QLineEdit;
class QTextEdit;
class QSpinBox;
class QDoubleSpinBox;
class QComboBox;
class QPushButton;
class QLabel;
class QNetworkAccessManager;
class QScrollArea;

/* ══════════════════════════════════════════════════════════════
   StableDiffusionWidget — Text-to-Image via REST API locale

   Compatibile con:  AUTOMATIC1111, Forge, SD.Next, Stability Matrix
   Endpoint default: http://localhost:7860

   Flusso:
   1. "⚡ Controlla" → GET /sdapi/v1/sd-models → lista modelli
   2. "🎨 Genera"   → POST /sdapi/v1/txt2img   → base64 PNG
   3. Immagine mostrata inline + salvata in ~/.prismalux/generated/
   ══════════════════════════════════════════════════════════════ */
class StableDiffusionWidget : public QWidget {
    Q_OBJECT
public:
    explicit StableDiffusionWidget(QWidget* parent = nullptr);

private:
    void checkServer();
    void generate();
    void showImage(const QByteArray& pngBase64);
    void setStatus(const QString& msg, bool ok = true);

    QLineEdit*        m_urlEdit    = nullptr;
    QTextEdit*        m_prompt     = nullptr;
    QTextEdit*        m_negPrompt  = nullptr;
    QComboBox*        m_modelCombo = nullptr;
    QSpinBox*         m_steps      = nullptr;
    QDoubleSpinBox*   m_cfg        = nullptr;
    QComboBox*        m_size       = nullptr;
    QLineEdit*        m_seed       = nullptr;
    QPushButton*      m_btnCheck   = nullptr;
    QPushButton*      m_btnGen     = nullptr;
    QPushButton*      m_btnSave    = nullptr;
    QLabel*           m_imgLabel   = nullptr;
    QLabel*           m_status     = nullptr;
    QScrollArea*      m_imgScroll  = nullptr;

    QNetworkAccessManager* m_nam   = nullptr;
    QByteArray             m_lastPng;

signals:
    void _imageReady(bool ok);   ///< segnale interno per abilitare pulsanti Salva/Copia
};
