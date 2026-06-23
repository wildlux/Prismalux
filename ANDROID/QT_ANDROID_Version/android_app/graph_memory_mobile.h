#pragma once
#include <QObject>
#include <QString>
#include <QVector>
#include <QVariantMap>
#include <QDateTime>
#include <optional>

/* ══════════════════════════════════════════════════════════════
   GraphMemory mobile — memoria a grafo con persistenza SQLite
   Port di gui/graph_memory.h adattato per Android.

   DB: QStandardPaths::AppDataLocation + "/graph_memory.db"
   Guard: HAVE_SQL (da CMakeLists.txt se Qt6Sql trovato)
   ══════════════════════════════════════════════════════════════ */

struct GmNode {
    QString     id;
    QString     type;
    QString     label;
    QString     content;
    QVariantMap meta;
    QDateTime   createdAt;
    float       importance = 1.0f;
};

struct GmEdge {
    QString     id;
    QString     fromId;
    QString     toId;
    QString     relation;
    float       weight = 1.0f;
    QVariantMap meta;
};

class GraphMemoryMobile : public QObject {
    Q_OBJECT
public:
    explicit GraphMemoryMobile(const QString& dbPath, QObject* parent = nullptr);
    ~GraphMemoryMobile() override;

    bool open();
    void close();
    bool isOpen() const { return m_open; }

    /* ── Scrittura ── */
    QString addNode(const QString& type,
                    const QString& label,
                    const QString& content   = {},
                    float          importance = 1.0f,
                    const QVariantMap& meta  = {});

    bool addEdge(const QString& fromId,
                 const QString& toId,
                 const QString& relation,
                 float          weight = 1.0f,
                 const QVariantMap& meta = {});

    bool updateNode(const QString& id,
                    const QString& content,
                    float          importance = -1.0f);

    bool removeNode(const QString& id);

    /* ── Lettura ── */
    QVector<GmNode> allNodes(const QString& typeFilter = {}) const;
    QVector<GmEdge> allEdges() const;
    QVector<GmNode> neighbours(const QString& nodeId, int depth = 1) const;
    QVector<GmNode> searchNodes(const QString& query, int limit = 20) const;
    std::optional<GmNode> nodeByLabel(const QString& label) const;
    std::optional<GmNode> nodeById(const QString& id) const;

    /* ── Statistiche ── */
    int nodeCount() const;
    int edgeCount() const;

    /* ── Export ── */
    QString toDot(const QString& title = "KnowledgeGraph", int maxNodes = 100) const;
    QString toJson(int maxNodes = 200) const;
    bool    exportTxt(const QString& path, int maxNodes = 100) const;

    /* ── Transazioni batch ── */
    void beginBatch();
    void endBatch();
    void abortBatch();

    /* ── Manutenzione ── */
    void pruneByImportance(int keepTopN = 500);
    void clearAll();

    /** Percorso DB di default per Android. */
    static QString defaultDbPath();

signals:
    void changed();

private:
    void           initSchema();
    static QString newUuid();
    static GmNode  rowToNode(const QVariantMap& row);
    static GmEdge  rowToEdge(const QVariantMap& row);

    QString m_dbPath;
    bool    m_open    = false;
    bool    m_inBatch = false;
    QString m_connName;
};
