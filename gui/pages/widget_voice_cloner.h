#pragma once
#include <QWidget>
#include <QLabel>
#include <QTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QProcess>
#include <QProgressBar>
#include <QGroupBox>
#include <QSlider>
#include <QTimer>

// Sintetizzatore vocale da campione — multi-backend: Coqui XTTS-v2 / chatterbox-tts / edge-tts
class VoiceClonerWidget : public QWidget {
    Q_OBJECT
public:
    enum TtsBackend { BackendNone, BackendCoqui, BackendChatterbox, BackendEdge };

    explicit VoiceClonerWidget(QWidget* parent = nullptr);
    ~VoiceClonerWidget() override;

private slots:
    void onLoadSampleClicked();
    void onRecordToggled(bool on);
    void onRecProcFinished(int code, QProcess::ExitStatus);
    void onGenerateClicked();
    void onStopClicked();
    void onCloneProcReadyRead();
    void onCloneProcFinished(int code, QProcess::ExitStatus);
    void onPlayOutputClicked();
    void onSaveWavClicked();
    void onInstallClicked();
    void onInstallProcReadyRead();
    void onInstallProcFinished(int code, QProcess::ExitStatus);

private:
    /* --- UI --- */
    QLabel*       m_sampleLbl    = nullptr;
    QPushButton*  m_loadBtn      = nullptr;
    QPushButton*  m_recBtn       = nullptr;
    QGroupBox*    m_sampleGroup  = nullptr;
    QTextEdit*    m_textEdit     = nullptr;
    QComboBox*    m_langCombo    = nullptr;
    QProgressBar* m_progress     = nullptr;
    QLabel*       m_statusLbl    = nullptr;
    QPushButton*  m_genBtn       = nullptr;
    QPushButton*  m_stopBtn      = nullptr;
    QPushButton*  m_playBtn      = nullptr;
    QPushButton*  m_saveBtn      = nullptr;
    QPushButton*  m_installBtn   = nullptr;
    QLabel*       m_backendLbl   = nullptr;   ///< mostra il backend attivo
    QTextEdit*    m_logView      = nullptr;

    /* --- stato --- */
    QString    m_samplePath;
    QString    m_outputWav;
    bool       m_ttsInstalled = false;
    TtsBackend m_backend      = BackendNone;

    /* --- processi --- */
    QProcess* m_recProc   = nullptr;
    QProcess* m_cloneProc = nullptr;
    QProcess* m_playProc  = nullptr;
    QProcess* m_instProc  = nullptr;

    /* --- costruzione UI (step-down) --- */
    QGroupBox* buildSampleGroup();
    QGroupBox* buildTextGroup();
    QGroupBox* buildSettingsGroup();
    QWidget*   buildActionRow();
    QGroupBox* buildOutputGroup();
    QGroupBox* buildLogGroup();

    void checkTtsInstalled();
    void applyBackend();
    QString buildScript(const QString& testo, const QString& lang) const;
    static QString edgeVoice(const QString& lang);
    void appendLog(const QString& line);
    void setStatus(const QString& msg);
    bool hasSample() const;
};
