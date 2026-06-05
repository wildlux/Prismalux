#pragma once
#include "main_sci_protein.h"
/* ══════════════════════════════════════════════════════════════
   SciComputePage — Calcolo Scientifico Distribuito (BOINC-like)

   Architettura:
     - Coordinator (server TCP 11601): crea Work Unit, dispatch, raccoglie risultati
     - Worker (client): riceve WU, esegue tool scientifico, invia risultato + hash
     - Una stessa istanza può essere entrambi (usa questo PC come worker)

   Sicurezza:
     - Bearer token condiviso (config una volta, stesso per tutti i nodi)
     - Rete: Tailscale VPN consigliata (i nodi si vedono come LAN)
     - Path validation: blocca traversal e directory sensibili
     - Tool: solo nomi binari, mai shell string

   Persistenza: SQLite (~/.prismalux/sci_compute.db)
   Porta: P::kSciComputePort (11601)
   ══════════════════════════════════════════════════════════════ */
#include <QWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QMap>
#include <QVector>
#include <QJsonObject>
#include <QVariantMap>
#include <QStringList>

class QTableWidget;
class QTextEdit;
class QLabel;
class QLineEdit;
class QPushButton;
class QComboBox;
class QCheckBox;
class QStackedWidget;
class QTabWidget;
class QSplitter;

/* Tipo di task scientifico con template parametri */
struct SciTaskType {
    QString id;
    QString name;
    QString toolRequired;   /* binario richiesto, "" = nessuno */
    QString domain;         /* bioinformatica | fisica | chimica | generale */
    QString paramsTemplate; /* JSON di esempio mostrato nell'editor */
};

class SciComputePage : public QWidget {
    Q_OBJECT
public:
    explicit SciComputePage(QWidget* parent = nullptr);
    ~SciComputePage();

    static const QVector<SciTaskType>& taskTypes();
    static bool isSafeToolName(const QString& name);
    static bool isSafePath(const QString& path);

private slots:
    void onServerNewConn();
    void onWorkerReadyRead();
    void onWorkerDisconnected();
    void onSelfReadyRead();
    void onSelfDisconnected();
    void onDispatchTimer();
    void onHeartbeatTimer();
    void onStartStopClicked();
    void onConnectClicked();
    void onAddWuClicked();
    void onTypeComboChanged(int idx);
    void onWuTableRowClicked(int row);
    void onDeleteWuClicked();
    void onGenerateFromFileClicked();
    void onAggregateResultsClicked();

private:
    /* ── DB ── */
    void    openDb();
    void    initSchema();
    QString insertWu(const QString& type, const QString& label,
                     const QString& params, int priority, int replicas,
                     const QString& dependsOn = {}, const QString& pipelineId = {});
    void    createPipeline(const QString& tmplId,
                           const QJsonObject& userParams = {});
    void    setWuStatus(const QString& id, const QString& status,
                        const QString& nodeId = {}, const QString& err = {});
    void    saveResult(const QString& wuId, const QString& nodeId,
                       const QString& output, const QString& hash);
    bool    checkQuorum(const QString& wuId, int replicas);
    void    upsertNode(const QString& id, const QString& name,
                       const QString& addr, int port, int cpu, int ram,
                       const QString& gpu, const QStringList& tools);
    void    setNodeStatus(const QString& id, const QString& status);
    void    markOfflineNodes(qint64 thresholdMs = 60000);
    QVector<QVariantMap> queryWus(const QString& filter = {});
    QVector<QVariantMap> queryNodes();
    QVector<QVariantMap> queryResults(const QString& wuId);

    /* ── Rete — server (coordinator) ── */
    void startServer();
    void stopServer();
    void sendJson(QTcpSocket* s, const QJsonObject& obj);
    void handleWorkerMessage(QTcpSocket* s, const QJsonObject& msg);
    void dispatchTo(const QString& wuId, QTcpSocket* sock, const QString& nodeId);

    /* ── Rete — client (worker) ── */
    void connectToCoord(const QString& host, int port);
    void disconnectFromCoord();
    void handleCoordMessage(const QJsonObject& msg);

    /* ── Esecuzione locale ── */
    void executeLocally(const QString& wuId, const QString& type,
                        const QJsonObject& params);
    void handleLocalResult(const QString& wuId, bool ok,
                            const QString& output, const QString& hash,
                            const QString& err);

    /* ── File broker ── */
    /* Coordinator: registra file locali accessibili ai worker */
    void   registerFile(const QString& logicalName, const QString& localPath);
    /* Coordinator: gestisce richiesta file da worker */
    void   handleFileRequest(QTcpSocket* s, const QString& name);
    /* Worker: richiede file al coordinator prima di eseguire */
    void   requestFile(const QString& name, const QString& saveTo);
    /* Worker: invia file risultato al coordinator */
    void   sendFileResult(const QString& wuId, const QString& filename,
                          const QString& localPath);
    QMap<QString, QString> m_fileRegistry;    /* name → localPath (coordinator) */
    QMap<QString, QString> m_pendingFileSaves; /* name → savePath (worker) */

    /* ── Progress streaming ── */
    void updateWuProgress(const QString& wuId, int pct, const QString& msg);

    /* ── Capability ── */
    static QJsonObject detectCapabilities();

    /* ── UI (in main_sci_compute_ui.cpp) ── */
    QWidget* buildUi();
    void refreshWuTable();
    void refreshNodeTable();
    void refreshResults(const QString& wuId = {});
    void appendLog(const QString& msg);
    void updateStatus();

    /* ── Network state ── */
    QTcpServer*                   m_server    = nullptr;
    QMap<QTcpSocket*, QByteArray> m_srvBufs;
    QMap<QTcpSocket*, QString>    m_sockNode;
    QMap<QString,     QTcpSocket*> m_nodeSock;

    QTcpSocket* m_selfSock = nullptr;   /* connessione verso il proprio server */
    QByteArray  m_selfBuf;

    /* ── Timer ── */
    QTimer*  m_dispatchTimer   = nullptr;   /* ogni 3s */
    QTimer*  m_heartbeatTimer  = nullptr;   /* ogni 30s */
    QMap<QString, qint64> m_nodeLastSeen;   /* node_id → ms epoch */

    /* ── Config ── */
    QString m_token;
    QString m_myNodeId;
    bool    m_isCoord    = true;
    bool    m_useLocal   = true;

    /* ── Progress streaming ── */
    QMap<QString, QPair<int,QString>> m_wuProgress;    /* wuId → {pct, msg} */
    QMap<QString, qint64>             m_lastProgressMs; /* throttle 2s per wuId */

    /* ── DB ── */
    QString m_connName;

    /* ── UI ── */
    QLabel*       m_statusLbl     = nullptr;
    QTableWidget* m_wuTable       = nullptr;
    QTableWidget* m_nodeTable     = nullptr;
    QTextEdit*    m_resultView    = nullptr;
    QTextEdit*    m_logView       = nullptr;
    QLineEdit*    m_coordHostEdit = nullptr;
    QLineEdit*    m_tokenEdit     = nullptr;
    QComboBox*    m_typeCombo     = nullptr;
    QLineEdit*    m_labelEdit     = nullptr;
    QTextEdit*    m_paramsEdit    = nullptr;
    QComboBox*    m_priorityCmb   = nullptr;
    QComboBox*    m_replicasCmb   = nullptr;
    QPushButton*  m_btnStartStop  = nullptr;
    QPushButton*  m_btnConnect    = nullptr;
    QPushButton*  m_btnAggregate  = nullptr;
    QCheckBox*    m_localChk      = nullptr;
    QStackedWidget* m_modeStack   = nullptr;
    QString           m_selectedWuId;
    SciProteinWidget* m_proteinWidget  = nullptr;
    QString           m_sciLlmModel    = "llama3.2:3b"; /* modello LLM per analisi sci */
    QLineEdit*        m_sciModelEdit   = nullptr;
    QLabel*           m_sciModelStatus = nullptr;
};
