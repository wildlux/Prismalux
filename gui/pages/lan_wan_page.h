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
#include <QTableWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QPointer>
#include <QTimer>
#include <QStackedWidget>
#include <QDateTime>
#include <QRadioButton>
#include "../ai_client.h"
#include "../widgets/ai_error_widget.h"
#include "../widgets/qr_code_widget.h"
#include "agenti_multi_page.h"

class LanServer;

class LanWanPage : public QWidget {
    Q_OBJECT
public:
    explicit LanWanPage(AiClient* ai, QWidget* parent = nullptr);
    ~LanWanPage() override;
private:
    AiClient*    m_ai           = nullptr;
    LanServer*   m_lanServer    = nullptr;
    QPushButton* m_lanToggleBtn = nullptr;
    QSpinBox*    m_lanPortSpin  = nullptr;
    QLabel*      m_lanStatusLbl = nullptr;
    QPushButton* m_lanWebBtn    = nullptr;
    QLineEdit*   m_lanTokenEdit = nullptr;
    QPushButton* m_qrApkBtn        = nullptr;
    QPushButton* m_qrPageBtn       = nullptr;
    QrCodeWidget* m_qrInlineWidget = nullptr;
    QLabel*       m_urlDisplayLbl  = nullptr;
    QString       m_lanConnectIp;

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
        QString              status;   ///< "idle" | "working"
        QStringList          caps;
        QPointer<QTcpSocket> sock;
    };
    struct WanTask {
        QString   id;
        QString   kind;      ///< "ai_query" | "shell_cmd" | "eval_script"
        QString   payload;
        QString   status;    ///< "pending" | "running" | "done" | "error"
        QString   node;
        QString   result;
        QDateTime created;
    };

    /* --- Server --- */
    QTcpServer*   m_wanServer       = nullptr;
    QSpinBox*     m_wanPortSpin     = nullptr;
    QPushButton*  m_wanStartBtn     = nullptr;
    QLabel*       m_wanSrvStatusLbl = nullptr;
    QTableWidget* m_wanNodeTable    = nullptr;
    QTableWidget* m_wanTaskTable    = nullptr;
    QComboBox*    m_wanTaskKind     = nullptr;
    QTextEdit*    m_wanTaskPayload  = nullptr;
    QPushButton*  m_wanAddTaskBtn   = nullptr;
    QVector<WanNode> m_wanNodes;
    QVector<WanTask> m_wanTasks;

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
    QPushButton*  m_wanCliConBtn       = nullptr;
    QPushButton*  m_wanCliDisconBtn    = nullptr;
    QLabel*       m_wanCliStatusLbl    = nullptr;
    QTextEdit*    m_wanCliLog          = nullptr;
    QTcpSocket*   m_wanCliSock         = nullptr;
    QTimer*       m_wanCliPollTimer    = nullptr;
    QString       m_wanCliNodeId;
    QString       m_wanCliCurrentTask;  ///< id del task in esecuzione

    /* --- AI lato client --- */
    bool          m_wanCliAiActive  = false;
    QString       m_wanCliAiBuf;
    QMetaObject::Connection m_wanCliTokenConn;
    QMetaObject::Connection m_wanCliFinishedConn;
    QMetaObject::Connection m_wanCliErrorConn;
    /* --- llm_agent state --- */
    bool          m_wanCliIsAgentTask = false; ///< true durante un task llm_agent
    int           m_wanCliAgentDepth  = 0;     ///< profondità spawn corrente
    QString       m_wanCliAgentChain;          ///< chain_id per tracciamento

    /* --- Mode stack --- */
    QStackedWidget* m_wanModeStack  = nullptr;

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
    AgentiMultiPage* m_multiAgentTab = nullptr;

public:
    AgentiMultiPage* multiAgentTab() const { return m_multiAgentTab; }
private:

    QString  wanNextId() const;
    void     wanDispatch();
    void     wanRefreshTables();
    void     wanSendJson(QTcpSocket* sock, const QJsonObject& obj);
    void     wanLogCron(const QString& msg);
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
    void     gns3PopulateModels(QComboBox* combo);

private slots:
    void onModelsReady(const QStringList& models);
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
    void onWanNewConnection();
    void onWanNodeReadyRead();
    void onWanNodeDisconnected();
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
    /* llm_agent form */
    void onAgentSaveBtnClicked();
    void onAgentLoadBtnClicked();
    /* WAN decomposizione */
    void onWanDecomposeBtnClicked();
    /* WAN simulazione locale */
    void onWanSimBtnClicked();
};
