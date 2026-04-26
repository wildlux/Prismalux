#pragma once
#include <QWidget>
#include <QLabel>
#include <QComboBox>
#include <QTextEdit>
#include <QTextBrowser>
#include <QLineEdit>
#include <QPushButton>
#include <QProcess>
#include <QStringList>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QTableWidget>
#include <QTimer>
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

private:
    AiClient*        m_ai;
    HardwareMonitor* m_hw;
    QLabel*          m_hwLabel = nullptr;
    QComboBox*       m_cmbModel;
    QComboBox*       m_cmbBackend;
    QLabel*          m_ramStatusLbl = nullptr;
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

    void    cronLoadJobs();
    void    cronSaveJobs();
    void    cronTick();
    void    cronRunJob(int idx);
    void    cronRefreshTable();
    void    cronAddOrEdit(int idx = -1);
    QString cronNextRun(const QString& schedule) const;
    bool    cronShouldRun(const QString& schedule, const QString& lastRun) const;
};
