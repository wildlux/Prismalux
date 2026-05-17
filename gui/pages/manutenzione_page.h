#pragma once
class LanServer;
#include <QWidget>
#include <QLabel>
#include <QComboBox>
#include <QTextEdit>
#include <QTextBrowser>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QRadioButton>
#include <QTimeEdit>
#include <QDateTimeEdit>
#include <QDialog>
#include <QCheckBox>
#include <QProcess>
#include <QStringList>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QTableWidget>
#include <QTimer>
#include <QModelIndex>
#include <QGroupBox>
#include <QTableWidgetItem>
#include "../hardware_monitor.h"
#include "../ai_client.h"

class ManutenzioneePage : public QWidget {
    Q_OBJECT
public:
    explicit ManutenzioneePage(AiClient* ai, HardwareMonitor* hw, QWidget* parent = nullptr);
    void onHWReady(const HWInfo& hw);

    /** Costruiscono i widget di sezione — chiamati da ImpostazioniPage per tab piatte */
    QWidget* buildBackend();
    QWidget* buildConfigFmt();
    QWidget* buildHardware();
    QWidget* buildBugTracker();
    QWidget* buildLanServer();

private:
    AiClient*        m_ai;
    HardwareMonitor* m_hw;
    QLabel*          m_hwLabel = nullptr;
    QComboBox*       m_cmbModel;
    QComboBox*       m_cmbBackend;
    QLabel*          m_ramStatusLbl = nullptr;
    QLabel*          m_zramWarnLbl  = nullptr;
    QTextEdit*       m_ramLog       = nullptr;
    QComboBox*       m_cmbFmt       = nullptr;
    QLabel*          m_fmtStatus    = nullptr;

    /* ── Avvia llama-server (scheda Backend) ────────────────────────── */
    QLineEdit*   m_srvModelPath = nullptr;
    QLineEdit*   m_srvPort      = nullptr;
    QPushButton* m_srvStartBtn  = nullptr;
    QPushButton* m_srvStopBtn   = nullptr;
    QTextEdit*   m_srvLog       = nullptr;
    QProcess*    m_srvProc      = nullptr;

    /* ── Backend connection/model widgets (promossi da locali) ─────── */
    QLineEdit*   m_hostEdit     = nullptr;
    QLineEdit*   m_portEdit     = nullptr;
    QGroupBox*   m_grpServ      = nullptr;

    /* ── Ollama version + update widgets ────────────────────────────── */
    QLabel*      m_verLbl       = nullptr;
    QTextEdit*   m_updLog       = nullptr;
    QLabel*      m_updStatusLbl = nullptr;
    QPushButton* m_updAllBtn    = nullptr;
    QPushButton* m_updLlamaBtn  = nullptr;

    /* ── Processi per update/verifica ───────────────────────────────── */
    QProcess*    m_ollamaVerProc = nullptr;
    QProcess*    m_listProc      = nullptr;
    QProcess*    m_gitProc       = nullptr;
    QProcess*    m_ramCmdProc    = nullptr;

    /* helper per i bottoni RAM/zRAM */
    void runRamCmd(const QString& prog, const QStringList& args, const QString& label);

    /* ── Modalità Calcolo LLM ───────────────────────────────────────── */
    QPushButton* m_btnGpu      = nullptr;
    QPushButton* m_btnCpu      = nullptr;
    QPushButton* m_btnMisto    = nullptr;
    QPushButton* m_btnDoppia   = nullptr;   ///< NVIDIA + Intel iGPU (llama-server SYCL)
    QPushButton* m_btnSaveMode = nullptr;
    QLabel*      m_computeInfo = nullptr;
    int          m_gpuLayersFull = 0;       ///< layer stimati per GPU dedicata (avail_mb/80)
    long         m_nvidiaVramMb  = 0;      ///< VRAM disponibile GPU dedicata (MB)
    long         m_igpuVramMb    = 0;      ///< Apertura VRAM Intel iGPU (MB, 0 se non leggibile)
    QString      m_selectedMode;            ///< selezione in sospeso (non ancora salvata)

    void selectComputeMode(const QString& mode);  ///< evidenzia + anteprima, non salva
    void applyComputeMode(const QString& mode);   ///< salva + applica ad AiClient

    /* ── Bug Tracker ────────────────────────────────────────────────── */
    QComboBox*             m_bugModelCombo    = nullptr;
    QPushButton*           m_btnSearchBug     = nullptr;
    QPushButton*           m_btnApplyFix      = nullptr;
    QLabel*                m_bugStatusLbl     = nullptr;
    QTextBrowser*          m_bugLog           = nullptr;
    QNetworkAccessManager* m_namBug           = nullptr;
    QStringList            m_bugSnippets;
    int                    m_bugSearchPending = 0;
    QString                m_bugCurrentModel;
    QJsonObject            m_bugFixJson;

    void searchBugs(const QString& model);
    void analyzeBugResults();
    void parseBugFix(const QString& response);
    void applyBugFix();

    void updateHWLabel(const HWInfo& hw);

    /* ── helpers formato config ─────────────────────────────────────── */
    static QString detectConfigFmt();
    static QString convertConfig(const QString& newFmt);

    /* ── Cron ───────────────────────────────────────────────────────── */
public:
    QWidget* buildCronTab();

private:
    struct CronJob {
        QString id, name, prompt, model, schedule, lastRun, lastResult;
        bool    enabled = true;
    };

    QList<CronJob>  m_cronJobs;
    QTimer*         m_cronTimer     = nullptr;
    QTableWidget*   m_cronTable     = nullptr;
    QTextBrowser*   m_cronLog       = nullptr;
    QPushButton*    m_btnCronPause  = nullptr;
    bool            m_cronPaused    = false;

    /* Stato corrente del job cron in esecuzione (per slot onCronAi*) */
    int     m_cronJobIdx    = -1;
    QString m_cronJobName;
    QString m_cronPrevModel;
    QString m_cronJobModel;
    QMetaObject::Connection m_cronAiFinConn;
    QMetaObject::Connection m_cronAiErrConn;

    /* ── Cron dialog (widget temporanei del dialogo add/edit) ── */
    QLineEdit*      m_cronDlgEdName      = nullptr;
    QTextEdit*      m_cronDlgEdPrompt    = nullptr;
    QComboBox*      m_cronDlgCmbModel    = nullptr;
    QRadioButton*   m_cronDlgRdDaily     = nullptr;
    QRadioButton*   m_cronDlgRdHourly    = nullptr;
    QRadioButton*   m_cronDlgRdInterval  = nullptr;
    QRadioButton*   m_cronDlgRdOnce      = nullptr;
    QTimeEdit*      m_cronDlgTeDaily     = nullptr;
    QSpinBox*       m_cronDlgSpHourly    = nullptr;
    QSpinBox*       m_cronDlgSpInterval  = nullptr;
    QDateTimeEdit*  m_cronDlgDtOnce      = nullptr;
    bool            m_cronDlgIsEdit      = false;
    int             m_cronDlgIdx         = -1;
    QString         m_cronDlgSrcId;
    bool            m_cronDlgSrcEnabled  = true;
    QString         m_cronDlgSrcLastRun;
    QString         m_cronDlgSrcLastResult;
    QDialog*        m_cronDlgPtr         = nullptr;

    void    cronLoadJobs();
    void    cronSaveJobs();
    void    cronTick();
    void    cronRunJob(int idx);
    void    cronRefreshTable();
    void    cronAddOrEdit(int idx = -1);
    QString cronNextRun(const QString& schedule) const;
    bool    cronShouldRun(const QString& schedule, const QString& lastRun) const;

    /* ── LAN Server per Android ────────────────────────────────────── */
    LanServer*   m_lanServer    = nullptr;
    QPushButton* m_lanToggleBtn = nullptr;
    QLabel*      m_lanStatusLbl = nullptr;
    QLabel*      m_lanClientsLbl= nullptr;
    QSpinBox*    m_lanPortSpin  = nullptr;
    QPushButton* m_qrBtn        = nullptr;   ///< promosso da locale buildLanServer()
    QPushButton* m_copyBtn      = nullptr;   ///< "Copia URL" nel dialog QR
    QString      m_apkUrl;                   ///< URL APK corrente (per onCopyApkUrlClicked)

    friend struct CronAccess;   ///< accesso test suite a cronShouldRun/cronNextRun
    friend struct ManutAccess;  ///< accesso test suite a detectConfigFmt/convertConfig

    /* ── Bug Tracker AI (connessioni salvate per disconnessione one-shot) ── */
    QMetaObject::Connection m_bugAiTokenConn;
    QMetaObject::Connection m_bugAiFinishedConn;
    QMetaObject::Connection m_bugAiErrorConn;

    QString currentBugModel() const;

private slots:
    /* ── Backend / Server ── */
    void onSrvBrowseClicked();
    void onSrvStartClicked();
    void onSrvProcReadyRead();
    void onSrvProcFinished(int code, QProcess::ExitStatus status);
    void onSrvProcErrorOccurred(QProcess::ProcessError err);
    void onSrvStopClicked();
    void onFmtApplyClicked();
    void onApplyBtnClicked();
    void onBackendModelsReady(const QStringList& list);
    void onBackendModelsFetchError(const QString& msg);
    void onSetModelBtnClicked();
    void onBackendCmbChanged(int idx);

    /* ── Verifica / Aggiornamento Ollama & llama.cpp ── */
    void onVerifyOllamaVersion();
    void onOllamaVerProcFinished(int code, QProcess::ExitStatus status);
    void onOllamaVerProcError(QProcess::ProcessError err);
    void onUpdAllBtnClicked();
    void onListProcFinished(int code, QProcess::ExitStatus status);
    void onListProcError(QProcess::ProcessError err);
    void onUpdLlamaBtnClicked();
    void onGitProcFinished(int code, QProcess::ExitStatus status);
    void onGitProcError(QProcess::ProcessError err);

    /* ── Hardware / RAM / zRAM / NPU ── */
    void onAutoZramCbToggled(bool on);
    void onBtnGpuClicked();
    void onBtnCpuClicked();
    void onBtnMistoClicked();
    void onBtnDoppiaClicked();
    void onBtnSaveModeClicked();
    void onAiModelChangedApplyMode(const QString& model);
    void onRamCmdReadyRead();
    void onRamCmdFinished(int code, QProcess::ExitStatus status);
    void onDetectBtnClicked();
    void onCompBtnClicked();
    void onDisableRamBtnClicked();
    void onSingBtnClicked();
    void onDoppiaBtnClicked();
    void onBtnIntelNpuClicked();

    /* ── Cron ── */
    void onCronTableItemChanged(QTableWidgetItem* item);
    void onCronTableDoubleClicked(const QModelIndex& idx);
    void onCronBtnAddClicked();
    void onCronBtnEditClicked();
    void onCronBtnDelClicked();
    void onCronBtnRunClicked();
    void onCronPauseToggled(bool on);
    void onCronAiFinished(const QString& resp);
    void onCronAiError(const QString& err);
    void onCronDialogAccepted();

    /* ── LAN Server ── */
    void onLanToggleBtnToggled(bool on);
    void onLanQrBtnEnableUpdate(bool on);
    void onLanServerStatusChanged(bool running);
    void onLanClientConnected(const QString& addr);
    void onLanClientDisconnected(const QString& addr);
    void onQrBtnClicked();
    void onCopyApkUrlClicked();

    /* ── Bug Tracker ── */
    void onBugModelsReady(const QStringList& list);
    void onSearchBugClicked();
    void onClearBugFixClicked();
    void onBugGithubReply();  ///< usa sender() + property("bugRepo")
    void onBugRedditReply();  ///< usa sender()
    void onBugAiToken(const QString& tok);
    void onBugAiFinished(const QString& full);
    void onBugAiError(const QString& err);
};
