#pragma once
#include <QWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QComboBox>
#include <QTimer>
#include <QPermission>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QHttpMultiPart>
#include "../ai_client.h"

#ifdef HAVE_MULTIMEDIA
#include <QMediaRecorder>
#include <QMediaCaptureSession>
#include <QAudioInput>
#endif

/* --------------------------------------------------------------
   AudioPage -- registrazione audio e trascrizione AI.

   Modalità:
   1. Registra voce con il microfono (Qt Multimedia)
   2. Invia al server Whisper (compatibile OpenAI /v1/audio/transcriptions)
      oppure chiede all'AI di analizzare una descrizione testuale.
   3. Mostra il testo trascritto, modificabile e copiabile.
   -------------------------------------------------------------- */
class AudioPage : public QWidget {
    Q_OBJECT
public:
    explicit AudioPage(AiClient* ai, QWidget* parent = nullptr);

private slots:
    void onRecordToggle();
    void onMicPermissionResult(const QPermission& permission);
    void onTranscribeClicked();
    void onCopyResult();
    void onSendToChat();
    void onToken(const QString& t);
    void onFinished(const QString& f);
    void onError(const QString& e);
    void onAborted();
    void onRecordTick();
    void onAnalyzeText();
    void onCopyBtnRestore();
    void onChatBtnRestore();
    void onWhisperReply();

signals:
    void transcriptionReady(const QString& text);

private:
    void setRecordingState(bool recording);
    QString savedAudioPath() const;
    void uploadWhisper(const QString& filePath);

    AiClient* m_ai = nullptr;

    QPushButton*  m_recBtn       = nullptr;
    QLabel*       m_recStatus    = nullptr;
    QLabel*       m_recTimeLbl   = nullptr;
    QProgressBar* m_levelBar     = nullptr;
    QPushButton*  m_transcribeBtn = nullptr;
    QComboBox*    m_modeCombo    = nullptr;
    QTextEdit*    m_resultEdit   = nullptr;
    QPushButton*  m_copyBtn      = nullptr;
    QPushButton*  m_chatBtn      = nullptr;
    QLabel*       m_aiStatus     = nullptr;
    QProgressBar* m_aiProgress   = nullptr;

    QTimer* m_recTimer   = nullptr;
    int     m_recSecs    = 0;
    bool    m_recording  = false;
    bool    m_busy       = false;

    /* Whisper upload */
    QNetworkAccessManager* m_whisperNam   = nullptr;
    QNetworkReply*         m_whisperReply = nullptr;

#ifdef HAVE_MULTIMEDIA
    QMediaCaptureSession* m_captureSession = nullptr;
    QAudioInput*          m_audioInput     = nullptr;
    QMediaRecorder*       m_recorder       = nullptr;
#endif
};
