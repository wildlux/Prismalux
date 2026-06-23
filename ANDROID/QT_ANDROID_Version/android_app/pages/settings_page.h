#pragma once
#include <QWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QSlider>
#include <QProgressBar>
#include <QCheckBox>
#include <QScrollArea>

class AiClient;
class RagEngineSimple;
class LocalLlmClient;

class SettingsPage : public QWidget {
    Q_OBJECT
public:
    explicit SettingsPage(AiClient* ai, RagEngineSimple* rag,
                          LocalLlmClient* localLlm = nullptr,
                          QWidget* parent = nullptr);

signals:
    void serverChanged(const QString& host, int port, const QString& model);
    void ragIndexChanged(const QString& path);

private slots:
    void onApplyServer();
    void onTestConnection();
    void onRefreshModels();
    void onModelsReady(const QStringList& models);
    void onLoadRag();
    void onUpdateWebLink();
    void onQrScanClicked();
    void onLocalModeToggled(bool on);
    void onAiLocalModeChanged(bool on);
    void onDlModelSelected(int idx);
    void onDownloadModelClicked();
    void onDlProgress(qint64 received, qint64 total);
    void onDlFinished(const QString& path);
    void onDlError(const QString& msg);
    void onLocalTempUpdated(float celsius);
    void onLocalThermalPause(float celsius);
    void onLocalThermalResume();
    void onLocalModelLoaded(const QString& path);
    void onLocalLoadError(const QString& msg);
    void onThemeApply();
    void onModelComboChanged(int idx);
    void onTempChanged(int v);
    void onDlCancelClicked();
    void onQuickApply1b();
    void onQuickApply15b();
    void onModelHelpClicked();
    void onTestConnectionFinished();
    void onRagWatchdogTimeout();
    void onRagLoaded(int n);
    void onQrUrlFound(const QString& raw);
    void onSyncClicked();
    void onSyncFinished();

private:
    void applyQuickModel(const QString& model);

    AiClient*        m_ai;
    RagEngineSimple* m_rag;
    LocalLlmClient*  m_localLlm = nullptr;

    /* Tema */
    QComboBox*    m_themeCombo       = nullptr;

    /* Server */
    QLineEdit*   m_hostEdit      = nullptr;
    QSpinBox*    m_portSpin      = nullptr;
    QLineEdit*   m_tokenEdit     = nullptr;
    QLabel*      m_webLinkLbl    = nullptr;
    QPushButton* m_applyBtn      = nullptr;
    QPushButton* m_testBtn       = nullptr;
    QLabel*      m_connStatus    = nullptr;

    /* Modello */
    QComboBox*   m_modelCombo      = nullptr;
    QPushButton* m_refreshBtn      = nullptr;
    QLabel*      m_modelStatus     = nullptr;
    QWidget*     m_customModelRow  = nullptr;
    QLineEdit*   m_customModelEdit = nullptr;

    /* Parametri AI */
    QSlider*     m_tempSlider  = nullptr;
    QLabel*      m_tempLbl     = nullptr;
    QSpinBox*    m_predictSpin = nullptr;

    /* RAG */
    QLineEdit*   m_ragPathEdit = nullptr;
    QPushButton* m_ragLoadBtn  = nullptr;
    QLabel*      m_ragStatus   = nullptr;

    QScrollArea* m_scrollArea = nullptr;

    /* Test connessione temporaneo */
    QNetworkAccessManager* m_testNam   = nullptr;
    QNetworkReply*         m_testReply = nullptr;

    /* Sync desktop↔mobile */
    QNetworkAccessManager* m_syncNam       = nullptr;
    QNetworkReply*         m_syncReply     = nullptr;
    QLabel*                m_syncStatusLbl = nullptr;

    /* RAG watchdog */
    QTimer* m_ragWatchdog = nullptr;

    /* LLM Locale */
    QCheckBox*    m_localModeChk   = nullptr;
    QLabel*       m_localStatusLbl = nullptr;
    QComboBox*    m_dlModelCombo   = nullptr;
    QPushButton*  m_dlModelBtn     = nullptr;
    QPushButton*  m_dlCancelBtn    = nullptr;
    QProgressBar* m_dlProgress     = nullptr;
    QLabel*       m_tempLiveLbl    = nullptr;
    QLabel*       m_thermalMsgLbl  = nullptr;
};
