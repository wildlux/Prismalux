#pragma once
#include <QWidget>

class QLineEdit;
class QTextEdit;
class QSpinBox;
class QDoubleSpinBox;
class QComboBox;
class QPushButton;
class QLabel;
class QRadioButton;
#include <QProcess>
class QNetworkAccessManager;
class QNetworkReply;
class QScrollArea;
class QProgressBar;

/* ══════════════════════════════════════════════════════════════
   StableDiffusionWidget — Text-to-Image AI

   Modalità LOCALE  → Python diffusers (nessun server esterno)
   Modalità A1111   → AUTOMATIC1111 / Forge / SD.Next REST API
   ══════════════════════════════════════════════════════════════ */
class StableDiffusionWidget : public QWidget {
    Q_OBJECT
public:
    explicit StableDiffusionWidget(QWidget* parent = nullptr);
    ~StableDiffusionWidget() override;

private:
    /* backend check */
    void checkServer();
    void checkDiffusers();
    void checkA1111();

    /* generazione */
    void generate();
    void generateLocal();
    void generateA1111();

    /* helpers */
    void showImage(const QByteArray& pngData);
    void setStatus(const QString& msg, bool ok = true);
    bool isLocalMode() const;
    void onModeChanged();

    /* parametri */
    QRadioButton*     m_rbLocal    = nullptr;
    QRadioButton*     m_rbA1111   = nullptr;
    QWidget*          m_urlWidget  = nullptr;
    QLabel*           m_installHint = nullptr;
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
    QPushButton*      m_btnCopy    = nullptr;
    QLabel*           m_imgLabel   = nullptr;
    QLabel*           m_status     = nullptr;
    QProgressBar*     m_progress   = nullptr;
    QScrollArea*      m_imgScroll  = nullptr;

    QNetworkAccessManager* m_nam        = nullptr;
    QProcess*              m_sdProc     = nullptr;
    QNetworkReply*         m_checkReply = nullptr;
    QNetworkReply*         m_genReply   = nullptr;
    QByteArray             m_lastPng;
    QString                m_sdScriptPath;
    QString                m_pendingOutPath;

private slots:
    void onBtnSaveClicked();
    void onBtnCopyClicked();
    void onCopyFeedbackReset();
    void onImageReady(bool ok);
    void onCheckA1111ReplyFinished();
    void onCheckDiffusersFinished(int code, QProcess::ExitStatus status);
    void onLocalProcReadyRead();
    void onLocalProcFinished(int code, QProcess::ExitStatus status);
    void onA1111ReplyFinished();

signals:
    void _imageReady(bool ok);
};
