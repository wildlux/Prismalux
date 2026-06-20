#pragma once
#include <QObject>
#include <QString>
#include <QVector>
#include <QVariantMap>
#include <QDateTime>
#include <QTimer>
#include <optional>

/* ══════════════════════════════════════════════════════════════
   GraphMemory — Memoria a grafo con persistenza SQLite

   Nodo: qualsiasi entità (concetto, fatto, task, risultato)
   Arco: relazione tipizzata tra due nodi

   Tipi di nodo:
     entity      — entità nominale estratta da testo (RAG)
     concept     — concetto astratto
     fact        — affermazione verificata
     task        — sotto-compito assegnato a un agente
     result      — output di un sotto-compito
     rag_chunk   — chunk sorgente collegato a entità

   Tipi di arco:
     relates_to  — relazione generica
     elaborates  — un nodo espande un altro
     contradicts — nodi in contraddizione
     causes      — causa-effetto
     part_of     — composizione
     task_of     — subtask → task principale
     derived_from— risultato derivato da sorgente
   ══════════════════════════════════════════════════════════════ */

struct GmNode {
    QString     id;
    QString     type;         ///< entity | concept | fact | task | result | rag_chunk
    QString     label;        ///< nome breve (chiave di lookup)
    QString     content;      ///< testo completo del nodo
    QVariantMap meta;         ///< coppie chiave-valore arbitrarie (agente, source, ecc.)
    QDateTime   createdAt;
    float       importance = 1.0f;  ///< 0.0–1.0 per pruning
};

struct GmEdge {
    QString     id;
    QString     fromId;
    QString     toId;
    QString     relation;     ///< relates_to | elaborates | contradicts | ecc.
    float       weight = 1.0f;
    QVariantMap meta;
};

class GraphMemory : public QObject {
    Q_OBJECT
public:
    explicit GraphMemory(const QString& dbPath, QObject* parent = nullptr);
    ~GraphMemory() override;

    /* ── Apertura / chiusura ────────────────────────────────── */
    bool open();
    void close();
    bool isOpen() const { return m_open; }

    /* ── Scrittura ──────────────────────────────────────────── */

    /** Aggiunge un nodo e restituisce il suo id (UUID). */
    QString addNode(const QString& type,
                    const QString& label,
                    const QString& content   = {},
                    float          importance = 1.0f,
                    const QVariantMap& meta  = {});

    /** Aggiunge un arco tra due nodi già presenti.
     *  Ignora silenziosamente se uno dei due id non esiste. */
    bool addEdge(const QString& fromId,
                 const QString& toId,
                 const QString& relation,
                 float          weight = 1.0f,
                 const QVariantMap& meta = {});

    /** Aggiorna content e importance di un nodo esistente. */
    bool updateNode(const QString& id,
                    const QString& content,
                    float          importance = -1.0f);   // -1 = non toccare

    bool removeNode(const QString& id);   ///< rimuove nodo e tutti i suoi archi

    /* ── Lettura ────────────────────────────────────────────── */

    QVector<GmNode> allNodes(const QString& typeFilter = {}) const;
    QVector<GmEdge> allEdges() const;

    /** Restituisce nodi adiacenti entro `depth` hop. */
    QVector<GmNode> neighbours(const QString& nodeId, int depth = 1) const;

    /** Ricerca testuale (substring) su label + content. */
    QVector<GmNode> searchNodes(const QString& query, int limit = 20) const;

    /** Nodo con quel label (primo trovato, case-insensitive). */
    std::optional<GmNode> nodeByLabel(const QString& label) const;

    std::optional<GmNode> nodeById(const QString& id) const;

    /* ── Statistiche ────────────────────────────────────────── */
    int nodeCount() const;
    int edgeCount() const;

    /* ── Export ─────────────────────────────────────────────── */

    /** Genera Graphviz DOT per visualizzazione. */
    QString toDot(const QString& title = "KnowledgeGraph",
                  int   maxNodes = 150) const;

    /** Dump JSON per interop con agenti / MCP. */
    QString toJson(int maxNodes = 200) const;

    /** Snapshot .txt leggibile dall'uomo — usato come memoria condivisa
     *  fallback quando il contesto è troppo grande. */
    bool exportTxt(const QString& path, int maxNodes = 100) const;

    /* ── Transazioni batch ─────────────────────────────────── */

    /** Inizia una transazione esplicita per inserimenti multipli veloci.
     *  Tutti gli addNode/addEdge successivi fanno parte della transazione.
     *  Chiamare endBatch() per committare o abortBatch() per annullare. */
    void beginBatch();

    /** Committa la transazione aperta con beginBatch(). */
    void endBatch();

    /** Annulla la transazione aperta con beginBatch(). */
    void abortBatch();

    /* ── Manutenzione ───────────────────────────────────────── */

    /** Mantiene solo i `keepTopN` nodi con importanza più alta;
     *  rimuove gli archi orfani. */
    void pruneByImportance(int keepTopN = 500);

    /** Cancella tutto il grafo (irreversibile). */
    void clearAll();

    /**
     * enableAutoBackup() — avvia un timer che ogni intervalHours ore:
     *   1. chiama pruneByImportance(pruneKeepN) per limitare la dimensione
     *   2. copia il DB in <backupDir>/graph_memory_YYYYMMDD_HHmmss.db
     *   3. mantiene solo gli ultimi keepMaxBackups file (cancella i più vecchi)
     * Sicuro: la copia avviene tramite QFile::copy() (WAL flushed da SQLite).
     * Chiamare dopo open().
     */
    void enableAutoBackup(int intervalHours = 6,
                          int pruneKeepN     = 500,
                          int keepMaxBackups = 7);

    /** Esegue subito un backup manuale (stesso comportamento del timer). */
    bool runBackupNow();

signals:
    /** Emesso dopo ogni modifica — per aggiornare la vista. */
    void changed();

    /** Emesso al termine di un backup automatico. path = file creato; ok = esito. */
    void backupDone(const QString& path, bool ok);

private slots:
    void onBackupTimer();

private:
    void        initSchema();
    static QString newUuid();
    static GmNode  rowToNode(const QVariantMap& row);
    static GmEdge  rowToEdge(const QVariantMap& row);

    QString  m_dbPath;
    bool     m_open         = false;
    bool     m_inBatch      = false;  ///< true tra beginBatch() e endBatch()
    QString  m_connName;
    QTimer*  m_backupTimer  = nullptr;
    int      m_pruneKeepN   = 500;
    int      m_keepMaxBackups = 7;
};
