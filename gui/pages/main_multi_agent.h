#pragma once
#include <QWidget>
#include <QVector>
#include <QMap>
#include <QSet>
#include <QPointer>
#include <QFuture>
#include <QFutureWatcher>
#include <QPromise>
#include "../ai_client.h"
#include "../graph_memory.h"
#include "../widgets/model_combo_box.h"

class QTextEdit;
class QPlainTextEdit;
class QLabel;
class QPushButton;
class QListWidget;
class QListWidgetItem;
class QProgressBar;
class QScrollArea;
class QSplitter;
class QFrame;
class QComboBox;
class QTabWidget;

/* ══════════════════════════════════════════════════════════════
   SubTask — rappresenta un sotto-compito assegnato a un sub-agente

   Stato: pending → running → done | error
   ══════════════════════════════════════════════════════════════ */
struct SubTask {
    int        id          = 0;
    QString    role;           ///< "Ricercatore", "Analista", ecc.
    QString    prompt;         ///< istruzioni per il sub-agente
    QVector<int> dependsOn;    ///< ids dei task precedenti da attendere
    QString    result;         ///< risposta accumulata
    QString    resultNodeId;   ///< id nodo GraphMemory del risultato
    enum class State { Pending, Running, Done, Error } state = State::Pending;
};

/* ══════════════════════════════════════════════════════════════
   AgentiMultiPage — Orchestratore multi-agente con memoria a grafo

   Layout:
   ┌─────────────────────────────────────────────────────────────┐
   │  [Prompt utente]           [Scegli modello]  [Decomponsi]   │
   ├──────────────────┬──────────────────────────────────────────┤
   │  Pannello        │  Output complessivo / grafo memoria       │
   │  sub-task        │                                          │
   │  (lista items    │  [Tab: Risultato | Grafo | JSON memoria] │
   │   con stato)     │                                          │
   └──────────────────┴──────────────────────────────────────────┘
   │  Memoria condivisa: [Nodi] [Archi] [Export TXT] [Clear]     │
   └─────────────────────────────────────────────────────────────┘
   ══════════════════════════════════════════════════════════════ */
class AgentiMultiPage : public QWidget {
    Q_OBJECT
public:
    explicit AgentiMultiPage(AiClient* ai, QWidget* parent = nullptr);
    ~AgentiMultiPage() override;

    GraphMemory* graphMemory() const { return m_gm; }

    /** Cross-pollination: inietta la GraphMemory del RagGraph esterno.
     *  QPointer garantisce che un eventuale destroy di RicercaPage
     *  non lasci un dangling pointer (diventa nullptr automaticamente). */
    void setExtRagMemory(GraphMemory* gm) { m_extRagGm = gm; }

private:
    /* ── Layout helpers ── */
    QWidget* buildInputBar();
    QWidget* buildTaskPanel();
    QWidget* buildOutputPanel();
    QWidget* buildMemoryBar();

    /* ── Orchestrazione ── */
    void decompose(const QString& userPrompt);
    void parsePlan(const QString& jsonPlan);
    void runNextPendingTask();
    void runTask(int taskIdx, AiClient* client);
    void onTaskResultToken(int taskIdx, const QString& tok);
    void onTaskResultDone(int taskIdx, const QString& full);
    void onTaskResultError(int taskIdx, const QString& msg);
    void synthesizeFinal();

    /* ── Pool parallelo ── */
    static constexpr int kMaxParallel = 3;  ///< max sub-agenti simultanei
    void      initPool();                   ///< crea/aggiorna il pool con le impostazioni di m_ai
    AiClient* takePoolClient();             ///< prende un client libero (nullptr se pool esaurito)
    void      returnPoolClient(AiClient* c);///< restituisce al pool

    /* ── UI helpers ── */
    void addTaskItem(const SubTask& t);
    void updateTaskItem(int taskIdx);
    void appendOutput(const QString& html);
    void clearOutput();
    void setStatus(const QString& msg);
    void refreshMemoryStats();
    void refreshDot();

    /* ── Membri ── */
    AiClient*    m_ai   = nullptr;
    GraphMemory* m_gm   = nullptr;     ///< memoria a grafo condivisa (owned)
    QPointer<GraphMemory> m_extRagGm;  ///< GraphMemory RAG esterna (borrowed, cross-pollination)

    QVector<SubTask> m_tasks;
    QSet<int>    m_runningTasks;        ///< indici task attualmente in esecuzione (paralleli)
    bool m_decomposeBusy = false;
    bool m_synthBusy     = false;

    /* Pool AiClient per esecuzione parallela */
    QVector<AiClient*>  m_aiPool;       ///< figli di this → distrutti automaticamente
    QSet<AiClient*>     m_busyClients;  ///< client attualmente occupati
    QMap<int,AiClient*> m_taskClients;  ///< taskIdx → client assegnato

    /* holder one-shot per decomposizione */
    QObject* m_decompHolder = nullptr;
    /* holder one-shot per sintesi finale */
    QObject* m_synthHolder  = nullptr;
    /* holders one-shot per ogni sub-agente */
    QMap<int, QObject*> m_taskHolders;

    /* QFuture/QPromise per tracciamento parallelo sub-agenti */
    QMap<int, QSharedPointer<QPromise<QString>>> m_taskPromises;
    QMap<int, QFutureWatcher<QString>*>          m_taskWatchers;

    /* UI */
    ModelComboBox*  m_modelCombo   = nullptr;
    QTextEdit*      m_promptInput  = nullptr;
    QPushButton*    m_btnDecompose = nullptr;
    QPushButton*    m_btnStop      = nullptr;
    QPushButton*    m_btnClear     = nullptr;
    QLabel*         m_status       = nullptr;
    QListWidget*    m_taskList     = nullptr;
    QTabWidget*     m_outputTabs   = nullptr;
    QPlainTextEdit* m_outputLog    = nullptr;   ///< tab "Risultato"
    QPlainTextEdit* m_dotView      = nullptr;   ///< tab "Grafo DOT"
    QPlainTextEdit* m_jsonView     = nullptr;   ///< tab "JSON"
    QLabel*         m_memStats     = nullptr;
    QPushButton*    m_btnExportTxt = nullptr;
    QPushButton*    m_btnClearMem  = nullptr;

private slots:
    void onDecomposeClicked();
    void onStopClicked();
    void onClearClicked();
    void onExportTxtClicked();
    void onClearMemClicked();
    void onGraphMemoryChanged();
    void onTaskItemClicked(QListWidgetItem* item);
};
