#pragma once
#include <QWidget>
#include <QLabel>
#include <QTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QProcess>
#include <QSpinBox>
#include <QCheckBox>
#include <QSet>
#include <QTimer>
#include "../ai_client.h"
#include "../widgets/ai_error_widget.h"
#include "widget_synthesizer.h"

class MultimediaPage : public QWidget {
    Q_OBJECT
public:
    explicit MultimediaPage(AiClient* ai, QWidget* parent = nullptr);
private slots:
    void onFileBtnClicked();
    void onRecBtnToggled(bool on);
    void onRecProcFinished(int code, QProcess::ExitStatus status);
    void onTranscribeBtnClicked();
    void onFfmpegFinished(int code, QProcess::ExitStatus status);
    void onHttpTranscriptionReady(const QString& text);
    void onHttpTranscriptionError(const QString& msg);
    void onAnalyzeBtnClicked();
    void onGraphvizBtnClicked();
    void onGraphvizProcFinished(int code, QProcess::ExitStatus status);
    void onGraphvizAiFinished(const QString& full);
    void onGraphvizAiError(const QString& msg);
    void onAudioToken(const QString& t);
    void onAudioAnalyzeFinished(const QString& full);
    void onAudioAnalyzeError(const QString& msg);
    // OCR
    void onOcrStartStopClicked(bool on);
    void onOcrTimerTick();
    void onOcrDaemonReadyRead();
    void onOcrDaemonFinished(int code, QProcess::ExitStatus status);
    void onOcrLoadVideoClicked();
    void onOcrTranscribeAudioClicked();
    void onOcrFfmpegFinished(int code, QProcess::ExitStatus status);
    void onOcrAnalyzeClicked();
    void onOcrAiToken(const QString& t);
    void onOcrAiFinished(const QString& full);
    void onOcrAiError(const QString& msg);
private:
    AiClient* m_ai = nullptr;
    // Audio AI
    QLabel*      m_audioFileLbl     = nullptr;
    QComboBox*   m_audioActionCombo = nullptr;
    QTextEdit*   m_audioTranscript  = nullptr;
    QTextEdit*   m_audioOutput      = nullptr;
    QProcess*    m_audioProc        = nullptr;
    QString      m_audioFilePath;
    QPushButton* m_recBtn           = nullptr;
    QProcess*    m_recProc          = nullptr;
    QString      m_recPath;
    QString      m_ffmpegWavTmp;
    QProcess*    m_ffmpegProc       = nullptr;
    // Graphviz
    QTextEdit*   m_graphvizInput    = nullptr;
    QLabel*      m_graphvizImg      = nullptr;
    QLabel*      m_graphvizStatus   = nullptr;
    QProcess*    m_graphvizProc     = nullptr;
    QString      m_graphvizTmpPng;
    AiErrorWidget* m_graphvizErr   = nullptr;
    AiErrorWidget* m_audioErr      = nullptr;
    QMetaObject::Connection m_graphvizFinishedConn;
    QMetaObject::Connection m_graphvizErrorConn;
    QMetaObject::Connection m_audioTokenConn;
    QMetaObject::Connection m_audioFinishedConn;
    QMetaObject::Connection m_audioErrorConn;
    QMetaObject::Connection m_transcriptionReadyConn;
    QMetaObject::Connection m_transcriptionErrorConn;
    // OCR continua
    QLabel*      m_ocrPreview    = nullptr;
    QTextEdit*   m_ocrText       = nullptr;
    QLabel*      m_ocrStatus     = nullptr;
    QTextEdit*   m_ocrAiOut      = nullptr;
    QPushButton* m_ocrStartBtn   = nullptr;
    QProcess*    m_ocrDaemon     = nullptr;   ///< processo Python persistente (no respawn)
    QByteArray   m_ocrLineBuf;               ///< buffer linea parziale dal daemon
    QTimer*      m_ocrTimer      = nullptr;
    QSpinBox*    m_ocrInterval   = nullptr;
    bool         m_ocrPending    = false;     ///< evita richieste sovrapposte
    // filtri testo OCR
    QCheckBox*   m_ocrChkDedup   = nullptr;
    QCheckBox*   m_ocrChkAlpha   = nullptr;
    QCheckBox*   m_ocrChkMinLen  = nullptr;
    QSet<QString> m_ocrSeenLines;
    // video OCR
    QString      m_ocrVideoPath;
    QLabel*      m_ocrVideoLbl      = nullptr;
    QSpinBox*    m_ocrVideoStep     = nullptr;
    // trascrizione audio da video
    QPushButton* m_ocrTranscribeBtn = nullptr;
    QProcess*    m_ocrFfmpegProc    = nullptr;
    QProcess*    m_ocrWhisperProc   = nullptr;
    QString      m_ocrAudioWav;
    QMetaObject::Connection m_ocrAiTokenConn;
    QMetaObject::Connection m_ocrAiFinishedConn;
    QMetaObject::Connection m_ocrAiErrorConn;
    QWidget* buildAudioTab();
    QWidget* buildSDTab();
    QWidget* buildGraphvizTab();
    QWidget* buildSintetizzatoreTab();
    QWidget* buildOcrTab();
    QWidget* buildOsmMapTab();

    /* ── Mappa OSM ── */
    class WorldMapWidget* m_osmMap         = nullptr;
    QListWidget*          m_osmWpList      = nullptr;
    QLabel*               m_osmRouteInfo   = nullptr;
    QComboBox*            m_osmProfileCmb  = nullptr;
    QNetworkAccessManager* m_osmNam        = nullptr;
    void runGraphvizAi();
    void _renderDotCode(const QString& dot);
    void _doTranscribe(const QString& wav);
    void startOcrDaemon();
    void stopOcrDaemon();
    void requestOcrCapture();
};
