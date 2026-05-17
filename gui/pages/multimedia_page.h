#pragma once
#include <QWidget>
#include <QLabel>
#include <QTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QProcess>
#include "../ai_client.h"
#include "../widgets/ai_error_widget.h"

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
    void onAnalyzeBtnClicked();
    void onGraphvizBtnClicked();
    void onGraphvizProcFinished(int code, QProcess::ExitStatus status);
    void onGraphvizAiFinished(const QString& full);
    void onGraphvizAiError(const QString& msg);
    void onAudioToken(const QString& t);
    void onAudioAnalyzeFinished(const QString& full);
    void onAudioAnalyzeError(const QString& msg);
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
    QWidget* buildAudioTab();
    QWidget* buildSDTab();
    QWidget* buildGraphvizTab();
    void runGraphvizAi();
    void _renderDotCode(const QString& dot);
    void _doTranscribe(const QString& wav);
};
