#pragma once
#include <QWidget>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QTextCursor>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QProgressBar>
#include <QSpinBox>
#include <QGroupBox>
#include <QSplitter>
#include <QShortcut>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QKeyEvent>
#include <QVector>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFrame>
#include "../ai_client.h"
#include "../chat_history.h"

/* ══════════════════════════════════════════════════════════════
   RagDropWidget — area drag-and-drop per contesto RAG per agente
   ══════════════════════════════════════════════════════════════ */
class RagDropWidget : public QFrame {
    Q_OBJECT
public:
    explicit RagDropWidget(QWidget* parent = nullptr);

    /* Testo aggregato da tutti i file caricati (max 16 KB totali) */
    QString ragContext() const;
    bool    hasContext() const { return !m_files.isEmpty(); }

protected:
    void dragEnterEvent(QDragEnterEvent*) override;
    void dragMoveEvent(QDragMoveEvent*)   override;
    void dropEvent(QDropEvent*)           override;
    void mousePressEvent(QMouseEvent*)    override;

private:
    void addPath(const QString& path);
    void addFile(const QString& path);
    void updateLabel();

    struct FileEntry { QString name; QString content; };
    QVector<FileEntry> m_files;
    QLabel*      m_lbl      = nullptr;
    QPushButton* m_clearBtn = nullptr;
    int          m_totalBytes = 0;

    static constexpr int MAX_PER_FILE  =  4096;   /* 4 KB per file  */
    static constexpr int MAX_TOTAL     = 16384;   /* 16 KB totale   */
};

/* ══════════════════════════════════════════════════════════════
   AgentiPage — Pipeline configurabile + Motore Byzantino
   ══════════════════════════════════════════════════════════════ */
class AgentiPage : public QWidget {
    Q_OBJECT
public:
    explicit AgentiPage(AiClient* ai, QWidget* parent = nullptr);

    /** Espone la barra progresso per incorporarla nella status bar di MainWindow. */
    QProgressBar* progressBar() const { return m_progress; }

    /** Richiama una chat salvata: riempie m_input e m_log. */
    void recallChat(const ChatEntry& entry);

signals:
    /** Emessa quando una pipeline/byzantino/maththeory completa (per salvare in DB). */
    void chatCompleted(const QString& mode, const QString& model,
                       const QString& task,  const QString& response);
    /** Emessa quando un singolo agente completa (snapshot parziale per il DB). */
    void agentStepCompleted(const QString& mode, const QString& model,
                            const QString& task,  const QString& html);
    /** Emessa all'avvio di qualsiasi modalità (per liberare la status bar). */
    void pipelineStarted();

private:
    AiClient*       m_ai;
    QTextEdit*      m_log       = nullptr;
    QPlainTextEdit* m_input     = nullptr;
    QPushButton*    m_btnRun    = nullptr;  ///< "▶ Avvia" quando idle, "⏹ Stop" quando in esecuzione
    QPushButton*    m_btnAuto   = nullptr;
    QComboBox*      m_cmbMode   = nullptr;
    QLabel*         m_autoLbl   = nullptr;
    QLabel*         m_waitLbl   = nullptr;
    QProgressBar*   m_progress  = nullptr;
    QSpinBox*       m_spinShots = nullptr;
    QDialog*        m_cfgDlg    = nullptr;   // dialog configurazione agenti
    QPushButton*    m_btnCfg    = nullptr;   // apre m_cfgDlg
    int             m_maxShots   = MAX_AGENTS;
    int             m_tokenCount = 0;

    /* Configurazione agenti pipeline */
    static constexpr int MAX_AGENTS = 6;
    QComboBox*    m_roleCombo [MAX_AGENTS] = {};
    QComboBox*    m_modelCombo[MAX_AGENTS] = {};
    QCheckBox*    m_enabledChk[MAX_AGENTS] = {};
    RagDropWidget* m_ragWidget[MAX_AGENTS] = {};

    /* Modelli disponibili con dimensione (per trovare il più grande) */
    struct ModelInfo { QString name; qint64 size; };
    QVector<ModelInfo> m_modelInfos;
    QNetworkAccessManager* m_namAuto = nullptr;   // solo per auto-assegna

    /* Stato pipeline sequenziale */
    int              m_currentAgent = 0;
    QVector<QString> m_agentOutputs;
    QString          m_taskOriginal;

    /* Stato motore byzantino */
    int     m_byzStep = 0;
    QString m_byzA, m_byzC;

    /* Stato auto-assegnazione */
    enum class OpMode { Idle, Pipeline, Byzantine, AutoAssign, MathTheory };
    OpMode  m_opMode = OpMode::Idle;
    QString m_autoBuffer;   // accumula risposta JSON dell'orchestratore

    bool eventFilter(QObject* obj, QEvent* ev) override;
    void setupUI();
    void runPipeline();
    void runByzantine();
    void runMathTheory();
    void runAgent(int idx);
    void advancePipeline();
    void autoAssignRoles();            // avvia fetch modelli → chiama orchestratore
    void onAutoModelsReady();          // dopo fetch, chiama il modello più grande
    void parseAutoAssign(const QString& json);  // interpreta JSON ruoli

    /* Dati ruoli */
    struct AgentRole {
        QString icon;
        QString name;
        QString sysPrompt;
    };
    static QVector<AgentRole> roles();
    /* Guardia matematica locale — ritorna stringa risultato se gestita, "" altrimenti */
    static QString guardiaMath(const QString& input);

private slots:
    void onToken(const QString& t);
    void onFinished(const QString& full);
    void onError(const QString& msg);
    void onModelsReady(const QStringList& list);
};
