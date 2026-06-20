#pragma once
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QComboBox>
#include <QProcess>
#include <QProgressBar>
#include <QAbstractSocket>
#include <QCheckBox>
#include <QTableWidget>
#include <QTcpServer>
#include <QTcpSocket>
#if QT_CONFIG(ssl)
#  include <QSslServer>
#  include <QSslSocket>
#  include <QSslConfiguration>
#  include <QSslCertificate>
#  include <QSslKey>
#endif
#include <QPointer>
#include <QTimer>
#include <QStackedWidget>
#include <QDateTime>
#include <QRadioButton>
#include "../ai_client.h"
#include "../widgets/ai_error_widget.h"
#include "../widgets/qr_code_widget.h"
#include "main_multi_agent.h"
#include "main_sci_compute.h"

class LanServer;

class LanWanPage : public QWidget {
    Q_OBJECT
public:
    explicit LanWanPage(AiClient* ai, QWidget* parent = nullptr);
    ~LanWanPage() override;

    void addExtraTab(QWidget* w, const QString& label);

private:
    QTabWidget* m_tabs = nullptr;
    AiClient*    m_ai           = nullptr;
    LanServer*   m_lanServer    = nullptr;
    QPushButton* m_lanToggleBtn = nullptr;
    QSpinBox*    m_lanPortSpin  = nullptr;
    QLabel*      m_lanStatusLbl = nullptr;
    QPushButton* m_lanWebBtn    = nullptr;
    QLineEdit*   m_lanTokenEdit = nullptr;
    QPushButton* m_qrApkBtn        = nullptr;
    QPushButton* m_qrPageBtn       = nullptr;
    QrCodeWidget* m_qrInlineWidget  = nullptr;  ///< QR generico (web browser)
    QrCodeWidget* m_qrAndroidWidget = nullptr;  ///< QR app Flutter Android
    QLabel*       m_urlDisplayLbl  = nullptr;
    QString       m_lanConnectIp;
    QTimer*       m_ipWatchTimer   = nullptr;  ///< polling IP LAN ogni 30s
    QString       m_lastKnownIp;               ///< IP precedente per rilevare cambi

    /* ── Installazione ADB via USB ── */
    QPushButton* m_adbInstallBtn  = nullptr;
    QLabel*      m_adbStatusLbl   = nullptr;
    QTextEdit*   m_adbLog         = nullptr;
    QProcess*    m_adbProc        = nullptr;

    static QString findAdb();

    /* ── Tabella client connessi ── */
    QTableWidget* m_clientTable  = nullptr;
    QPushButton*  m_kickBtn      = nullptr;
    QPushButton*  m_kickAllBtn   = nullptr;

    /* ── GNS3 MCP ── */
    QLineEdit*     m_gns3HostEdit    = nullptr;
    QLabel*        m_gns3StatusLbl   = nullptr;
    QPushButton*   m_gns3ExecBtn     = nullptr;
    QComboBox*     m_gns3Action      = nullptr;
    QComboBox*     m_gns3Model       = nullptr;
    QTextEdit*     m_gns3Input       = nullptr;
    QTextEdit*     m_gns3Output      = nullptr;
    QPushButton*   m_gns3RunBtn      = nullptr;
    QPushButton*   m_gns3StopBtn     = nullptr;
    QString        m_gns3Code;
    QObject*       m_gns3TokenHolder = nullptr;
    bool           m_gns3AiActive    = false;
    AiErrorWidget* m_gns3ErrorPanel  = nullptr;
    QProcess*      m_gns3ExecProc    = nullptr;
    QProgressBar*  m_gns3Progress    = nullptr;

    /* ══════════════════════════════════════════════════════════════
       WAN — Calcolo Distribuito (BOINC-like)
       ══════════════════════════════════════════════════════════════ */

    /* --- Strutture dati --- */
    struct WanNode {
        QString              id;
        QString              name;
        QString              ip;
        QString              status;      ///< "idle" | "working"
        QStringList          caps;
        QPointer<QTcpSocket> sock;
        QDateTime            lastSeen;   ///< ultimo pong ricevuto
        int                  missedPings = 0; ///< ping consecutivi senza pong
    };
    struct WanTask {
        QString   id;
        QString   kind;      ///< "ai_query" | "shell_cmd" | "eval_script"
        QString   payload;
        QString   status;    ///< "pending" | "running" | "done" | "error" | "failed"
        QString   node;
        QString   result;
        QDateTime created;
        QDateTime startedAt; ///< quando il task è partito su un nodo
        int       retryCount = 0; ///< tentativi (max 3 prima di "failed")
        int       priority   = 1; ///< 0=bassa  1=normale  2=alta
    };

    /* --- Server --- */
    struct WanBadToken { int count = 0; QDateTime blockedUntil; };
    QHash<QString, WanBadToken> m_wanBadTokens; ///< IP → tentativi falliti (rate limiting)

    QTcpServer*   m_wanServer          = nullptr;
#if QT_CONFIG(ssl)
    QSslServer*   m_wanSslServer       = nullptr;
#endif
    bool          m_wanUseTls          = false;
    QCheckBox*    m_wanTlsCheck        = nullptr;
    QSpinBox*     m_wanPortSpin        = nullptr;
    QPushButton*  m_wanStartBtn        = nullptr;
    QLabel*       m_wanSrvStatusLbl    = nullptr;
    QCheckBox*    m_wanExposeAllCheck  = nullptr; ///< bind su 0.0.0.0 se ON, 127.0.0.1 se OFF
    QLineEdit*    m_wanTokenEdit       = nullptr;   ///< token Bearer server WAN (auth nodi)
    QLabel*       m_wanVpnNoteLbl      = nullptr; ///< suggerimento WireGuard visibile quando exposeAll=ON
    QTableWidget* m_wanNodeTable    = nullptr;
    QTableWidget* m_wanTaskTable    = nullptr;
    QComboBox*    m_wanTaskKind     = nullptr;
    QTextEdit*    m_wanTaskPayload  = nullptr;
    QPushButton*  m_wanAddTaskBtn   = nullptr;
    QVector<WanNode> m_wanNodes;
    QVector<WanTask> m_wanTasks;
    /* Persistenza coda WU su SQLite (~/.prismalux/wan_tasks.db): la coda sopravvive
       a chiusura/crash del coordinatore. wanLoadTasks() rimette "running"→"pending". */
    void             wanLoadTasks();
    void             wanPersistTasks();
    void             wanSchedulePersist();   ///< salvataggio con debounce
    QTimer*          m_wanPersistTimer    = nullptr;
    QString          m_wanDbConn;            ///< nome connessione SQLite univoco
    QTimer*          m_wanHeartbeatTimer  = nullptr; ///< 30s ping nodi
    QLabel*          m_wanStatsLbl        = nullptr; ///< stats BOINC-style
    QTimer*          m_wanDashTimer       = nullptr; ///< 5s aggiornamento throughput
    QLabel*          m_wanThroughputLbl   = nullptr; ///< task/ora
    QLabel*          m_wanChartWidget     = nullptr; ///< istogramma throughput (QPainter su QPixmap)
    QPushButton*     m_wanExportBtn       = nullptr; ///< esporta CSV
    QVector<QDateTime> m_wanCompletedTs;             ///< timestamp task completati (ultimi 60min)

    /* --- Cron --- */
    QSpinBox*    m_wanCronInterval  = nullptr;
    QComboBox*   m_wanCronKind      = nullptr;
    QTextEdit*   m_wanCronPayload   = nullptr;
    QPushButton* m_wanCronStartBtn  = nullptr;
    QPushButton* m_wanCronStopBtn   = nullptr;
    QTextEdit*   m_wanCronLog       = nullptr;
    QTimer*      m_wanCronTimer     = nullptr;

    /* --- Client --- */
    QLineEdit*    m_wanCliHost         = nullptr;
    QSpinBox*     m_wanCliPort         = nullptr;
    QLineEdit*    m_wanCliName         = nullptr;
    QLineEdit*    m_wanCliTokenEdit    = nullptr;  ///< token per autenticarsi al server
    QCheckBox*    m_wanCliTlsCheck     = nullptr;  ///< connetti con TLS (auto se server usa TLS)
    QCheckBox*    m_wanCliShellCheck   = nullptr;  ///< abilita esecuzione shell (opt-in esplicito)
    QPushButton*  m_wanCliConBtn       = nullptr;
    QPushButton*  m_wanCliDisconBtn    = nullptr;
    QLabel*       m_wanCliStatusLbl    = nullptr;
    QTextEdit*    m_wanCliLog          = nullptr;
    QTcpSocket*   m_wanCliSock         = nullptr;  ///< legacy — alias a worker[0].sock
    QTimer*       m_wanCliPollTimer    = nullptr;  ///< legacy — alias a worker[0].pollTimer
    QString       m_wanCliNodeId;
    QString       m_wanCliCurrentTask;  ///< legacy — alias a worker[0].currentTask

    /* --- AI lato client --- */
    bool          m_wanCliAiActive  = false;       ///< legacy — alias a worker[0].aiActive
    QString       m_wanCliAiBuf;                   ///< legacy — alias a worker[0].aiBuf
    QMetaObject::Connection m_wanCliTokenConn;
    QMetaObject::Connection m_wanCliFinishedConn;
    QMetaObject::Connection m_wanCliErrorConn;
    /* --- llm_agent state --- */
    bool          m_wanCliIsAgentTask = false; ///< true durante un task llm_agent
    int           m_wanCliAgentDepth  = 0;     ///< profondità spawn corrente
    QString       m_wanCliAgentChain;          ///< chain_id per tracciamento

    /* --- Multi-worker pool --- */
    struct WanWorker {
        QPointer<QTcpSocket>    sock;
        QPointer<AiClient>      ai;
        QPointer<QTimer>        pollTimer;
        QString                 name;           ///< nome nodo inviato nell'hello
        QString                 token;          ///< token auth (copiato all'avvio)
        bool                    shellAllowed   = false;
        QString                 nodeId;
        QString                 currentTask;
        bool                    aiActive       = false;
        QString                 aiBuf;
        QMetaObject::Connection tokConn;
        QMetaObject::Connection finConn;
        QMetaObject::Connection errConn;
        /* llm_agent per-worker state */
        bool                    isAgentTask    = false;
        int                     agentDepth     = 0;
        QString                 agentChain;
    };
    QVector<WanWorker>  m_wanWorkers;              ///< pool worker attivi (1-4)
    QSpinBox*           m_wanCliWorkerSpin = nullptr; ///< UI spinbox 1-4 worker

    /* --- Worker helpers --- */
    int     wanWorkerIndex(QObject* sender) const; ///< -1 se non trovato
    void    wanWorkerSendJson(int idx, const QJsonObject& obj);
    void    wanWorkerHandleTask(int idx, const QString& id,
                                const QString& kind, const QString& payload);
    void    wanWorkerAppendLog(int idx, const QString& msg);

    /* --- Mode stack --- */
    QStackedWidget* m_wanModeStack  = nullptr;

    /* --- Exec mode stack: 0=Solo PC, 1=Rete LAN --- */
    QStackedWidget* m_execModeStack = nullptr;

    /* --- Helpers --- */
    QString  localLanIp() const;
    QString  serverScheme() const;
    void     openQrDialog(QPushButton* parent, const QString& url,
                          const QString& title, const QString& subtitle,
                          const QString& note);
    void     clientTableAddRow(const QString& ip);
    void     clientTableRemoveRow(const QString& ip);
    static QString readMacForIp(const QString& ip);

    /* WAN — decomposizione automatica con MasterAgent */
    QTextEdit*   m_wanDecomposeInput     = nullptr;
    QPushButton* m_wanDecomposeBtn       = nullptr;
    QLabel*      m_wanDecomposeStatusLbl = nullptr;
    QObject*     m_wanDecompHolder       = nullptr;
    void         wanApplyDecomposedPlan(const QString& jsonPlan);

    /* WAN — simulazione locale (server + client sullo stesso PC) */
    QPushButton*    m_wanSimBtn      = nullptr;
    bool            m_wanSimActive   = false;

    /* Multi-Agente embed (tab interno) */
    AgentiMultiPage*  m_multiAgentTab  = nullptr;
    SciComputePage*   m_sciComputeTab  = nullptr;

public:
    AgentiMultiPage* multiAgentTab() const { return m_multiAgentTab; }
private:

    QString  wanNextId() const;
    void     wanDispatch();
    void     wanRefreshTables();
    void     wanSendJson(QTcpSocket* sock, const QJsonObject& obj);
    void     wanLogCron(const QString& msg);
    void     updateWanStats();
    void     wanReassignNodeTasks(const QString& nodeName);
    void     updateWanThroughput();
    void     exportWanCsv();
    void     wanCliAppendLog(const QString& msg);
    void     wanCliSendJson(const QJsonObject& obj);
    void     wanPopulateKindCombo(QComboBox* combo);
    QString  wanKindTemplate(const QString& kind) const;
    void     wanCliHandleTask(const QString& id, const QString& kind, const QString& payload);

    /* llm_agent form */
    QStackedWidget* m_wanPayloadStack   = nullptr; ///< 0=raw textarea 1=agent form
    QFrame*         m_agentFormFrame    = nullptr;
    QLineEdit*      m_agentRoleEdit     = nullptr;
    QTextEdit*      m_agentPromptEdit   = nullptr;
    QTextEdit*      m_agentContextEdit  = nullptr;
    QPushButton*    m_agentSaveBtn      = nullptr;
    QPushButton*    m_agentLoadBtn      = nullptr;

    bool eventFilter(QObject* obj, QEvent* e) override;

    QWidget* buildLanAndroidTab();
    QWidget* buildGNS3Tab();
    QWidget* buildWanComputeTab();
    void     gns3RunAi(const QString& sys, const QString& userMsg);

private slots:
    void onTokenTextChanged(const QString& t);
    void onEyeBtnToggled(bool show);
    void onRegenBtnClicked();
    void onCopyTokenBtnClicked();
    void onQrConnectBtnClicked();
    void onLanPortChanged(int v);
    void onUpdateQrInline();
    void onQrApkBtnClicked();
    void onQrPageBtnClicked();
    void onLanToggleBtnToggled(bool on);
    void onLanServerStatusChanged(bool running);
    void onLanClientConnected(const QString& addr);
    void onLanClientDisconnected(const QString& addr);
    void onTokenAutoGenerated(const QString& token);
    void onKickBtnClicked();
    void onKickAllBtnClicked();
    void onLanWebBtnClicked();
    void onAdbInstallBtnClicked();
    void onAdbProcReadyRead();
    void onAdbProcFinished(int code, QProcess::ExitStatus status);
    void onGns3AiToken(const QString& t);
    void onGns3AiFinished(const QString& full);
    void onGns3AiError(const QString& msg);
    void onPingBtnClicked();
    void onGns3SockConnected();
    void onGns3SockError(QAbstractSocket::SocketError err);
    void onGns3ExecBtnClicked();
    void onGns3ProcReadyRead();
    void onGns3ProcFinished(int code, QProcess::ExitStatus status);
    void onGns3RunBtnClicked();
    void onGns3StopBtnClicked();

    /* WAN Compute — server */
    void onWanStartBtnClicked();
    void onCheckOllamaExposed();
    void onOllamaCheckDone(QProcess* proc);
    void onWanNewConnection();
    void onWanNodeReadyRead();
    void onWanNodeDisconnected();
    void onWanHeartbeatTick();
    void onWanDashTick();
    void onWanExportCsvClicked();
    void onWanAddTaskBtnClicked();
    /* WAN Compute — cron */
    void onWanCronStartBtnClicked();
    void onWanCronStopBtnClicked();
    void onWanCronFired();
    /* WAN Compute — client */
    void onWanCliConBtnClicked();
    void onWanCliDisconBtnClicked();
    void onWanCliPoll();
    void onWanCliSockReadyRead();
    void onWanCliSockDisconnected();
    void onWanCliSockError(QAbstractSocket::SocketError err);
    /* WAN Compute — AI execution lato client */
    void onWanCliAiToken(const QString& t);
    void onWanCliAiFinished(const QString& full);
    void onWanCliAiError(const QString& msg);
    /* WAN Compute — multi-worker slots (usa sender() per identificare il worker) */
    void onWanWorkerConnected();
    void onWanWorkerPoll();
    void onWanWorkerReadyRead();
    void onWanWorkerDisconnected();
    void onWanWorkerError(QAbstractSocket::SocketError err);
    void onWanWorkerAiToken(const QString& t);
    void onWanWorkerAiFinished(const QString& full);
    void onWanWorkerAiError(const QString& msg);
    /* llm_agent form */
    void onAgentSaveBtnClicked();
    void onAgentLoadBtnClicked();
    /* WAN decomposizione */
    void onWanDecomposeBtnClicked();
    /* WAN simulazione locale */
    void onWanSimBtnClicked();
    void onIpWatchTick();  ///< rileva cambio IP e aggiorna QR
};
