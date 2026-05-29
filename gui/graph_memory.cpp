#include "graph_memory.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QTextStream>
#include <QUuid>
#include <QSet>
#include <QQueue>
#include <QDebug>

namespace {
constexpr const char* kConnPrefix = "graph_memory_";
}

/* ══════════════════════════════════════════════════════════════
   Costruttore / Distruttore
   ══════════════════════════════════════════════════════════════ */
GraphMemory::GraphMemory(const QString& dbPath, QObject* parent)
    : QObject(parent)
    , m_dbPath(dbPath)
    , m_connName(kConnPrefix + QString::number(reinterpret_cast<quintptr>(this)))
{}

GraphMemory::~GraphMemory()
{
    close();
}

/* ══════════════════════════════════════════════════════════════
   open / close
   ══════════════════════════════════════════════════════════════ */
bool GraphMemory::open()
{
    if (m_open) return true;

#ifndef HAVE_QT_SQL
    qWarning() << "GraphMemory: Qt::Sql non disponibile — funzionamento degradato.";
    m_open = true;   /* no-op mode */
    return true;
#else
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", m_connName);
    db.setDatabaseName(m_dbPath);
    if (!db.open()) {
        qWarning() << "GraphMemory: impossibile aprire" << m_dbPath
                   << db.lastError().text();
        return false;
    }
    initSchema();
    m_open = true;
    return true;
#endif
}

void GraphMemory::close()
{
    if (!m_open) return;
    m_open = false;
#ifdef HAVE_QT_SQL
    {
        QSqlDatabase db = QSqlDatabase::database(m_connName);
        if (db.isOpen()) db.close();
    }
    QSqlDatabase::removeDatabase(m_connName);
#endif
}

/* ══════════════════════════════════════════════════════════════
   initSchema
   ══════════════════════════════════════════════════════════════ */
void GraphMemory::initSchema()
{
#ifdef HAVE_QT_SQL
    QSqlDatabase db = QSqlDatabase::database(m_connName);
    QSqlQuery q(db);

    q.exec("PRAGMA journal_mode=WAL");
    q.exec("PRAGMA foreign_keys=ON");

    q.exec(
        "CREATE TABLE IF NOT EXISTS gm_nodes ("
        "  id         TEXT PRIMARY KEY,"
        "  type       TEXT NOT NULL DEFAULT 'entity',"
        "  label      TEXT NOT NULL,"
        "  content    TEXT DEFAULT '',"
        "  meta       TEXT DEFAULT '{}',"   /* JSON */
        "  importance REAL DEFAULT 1.0,"
        "  created_at TEXT NOT NULL"
        ")"
    );
    q.exec(
        "CREATE TABLE IF NOT EXISTS gm_edges ("
        "  id        TEXT PRIMARY KEY,"
        "  from_id   TEXT NOT NULL,"
        "  to_id     TEXT NOT NULL,"
        "  relation  TEXT NOT NULL DEFAULT 'relates_to',"
        "  weight    REAL DEFAULT 1.0,"
        "  meta      TEXT DEFAULT '{}',"
        "  FOREIGN KEY(from_id) REFERENCES gm_nodes(id) ON DELETE CASCADE,"
        "  FOREIGN KEY(to_id)   REFERENCES gm_nodes(id) ON DELETE CASCADE"
        ")"
    );
    q.exec("CREATE INDEX IF NOT EXISTS idx_edges_from ON gm_edges(from_id)");
    q.exec("CREATE INDEX IF NOT EXISTS idx_edges_to   ON gm_edges(to_id)");
    q.exec("CREATE INDEX IF NOT EXISTS idx_nodes_label ON gm_nodes(label COLLATE NOCASE)");
    q.exec("CREATE INDEX IF NOT EXISTS idx_nodes_type  ON gm_nodes(type)");
#endif
}

/* ══════════════════════════════════════════════════════════════
   Helpers
   ══════════════════════════════════════════════════════════════ */
QString GraphMemory::newUuid()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

GmNode GraphMemory::rowToNode(const QVariantMap& row)
{
    GmNode n;
    n.id         = row.value("id").toString();
    n.type       = row.value("type").toString();
    n.label      = row.value("label").toString();
    n.content    = row.value("content").toString();
    n.importance = row.value("importance", 1.0).toFloat();
    n.createdAt  = QDateTime::fromString(row.value("created_at").toString(), Qt::ISODate);

    const QByteArray metaJson = row.value("meta").toString().toUtf8();
    if (!metaJson.isEmpty()) {
        const auto doc = QJsonDocument::fromJson(metaJson);
        if (doc.isObject())
            n.meta = doc.object().toVariantMap();
    }
    return n;
}

GmEdge GraphMemory::rowToEdge(const QVariantMap& row)
{
    GmEdge e;
    e.id       = row.value("id").toString();
    e.fromId   = row.value("from_id").toString();
    e.toId     = row.value("to_id").toString();
    e.relation = row.value("relation").toString();
    e.weight   = row.value("weight", 1.0).toFloat();

    const QByteArray metaJson = row.value("meta").toString().toUtf8();
    if (!metaJson.isEmpty()) {
        const auto doc = QJsonDocument::fromJson(metaJson);
        if (doc.isObject())
            e.meta = doc.object().toVariantMap();
    }
    return e;
}

/* ══════════════════════════════════════════════════════════════
   addNode
   ══════════════════════════════════════════════════════════════ */
QString GraphMemory::addNode(const QString& type,
                              const QString& label,
                              const QString& content,
                              float          importance,
                              const QVariantMap& meta)
{
    const QString id = newUuid();
    const QString ts = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    const QString metaJson = QString::fromUtf8(
        QJsonDocument(QJsonObject::fromVariantMap(meta)).toJson(QJsonDocument::Compact));

#ifdef HAVE_QT_SQL
    if (m_open) {
        QSqlDatabase db = QSqlDatabase::database(m_connName);
        QSqlQuery q(db);
        q.prepare(
            "INSERT INTO gm_nodes(id,type,label,content,meta,importance,created_at)"
            " VALUES(:id,:type,:label,:content,:meta,:imp,:ts)"
        );
        q.bindValue(":id",      id);
        q.bindValue(":type",    type.isEmpty()    ? "entity" : type);
        q.bindValue(":label",   label);
        q.bindValue(":content", content);
        q.bindValue(":meta",    metaJson);
        q.bindValue(":imp",     importance);
        q.bindValue(":ts",      ts);
        if (!q.exec())
            qWarning() << "GraphMemory::addNode:" << q.lastError().text();
    }
#else
    Q_UNUSED(type) Q_UNUSED(label) Q_UNUSED(content) Q_UNUSED(importance)
    Q_UNUSED(meta) Q_UNUSED(ts) Q_UNUSED(metaJson)
#endif

    emit changed();
    return id;
}

/* ══════════════════════════════════════════════════════════════
   addEdge
   ══════════════════════════════════════════════════════════════ */
bool GraphMemory::addEdge(const QString& fromId,
                           const QString& toId,
                           const QString& relation,
                           float          weight,
                           const QVariantMap& meta)
{
    if (fromId.isEmpty() || toId.isEmpty()) return false;

    const QString id = newUuid();
    const QString metaJson = QString::fromUtf8(
        QJsonDocument(QJsonObject::fromVariantMap(meta)).toJson(QJsonDocument::Compact));

#ifdef HAVE_QT_SQL
    if (m_open) {
        QSqlDatabase db = QSqlDatabase::database(m_connName);
        QSqlQuery q(db);
        q.prepare(
            "INSERT OR IGNORE INTO gm_edges(id,from_id,to_id,relation,weight,meta)"
            " VALUES(:id,:from,:to,:rel,:w,:meta)"
        );
        q.bindValue(":id",   id);
        q.bindValue(":from", fromId);
        q.bindValue(":to",   toId);
        q.bindValue(":rel",  relation.isEmpty() ? "relates_to" : relation);
        q.bindValue(":w",    weight);
        q.bindValue(":meta", metaJson);
        if (!q.exec()) {
            qWarning() << "GraphMemory::addEdge:" << q.lastError().text();
            return false;
        }
    }
#else
    Q_UNUSED(fromId) Q_UNUSED(toId) Q_UNUSED(relation) Q_UNUSED(weight)
    Q_UNUSED(meta) Q_UNUSED(id) Q_UNUSED(metaJson)
#endif

    emit changed();
    return true;
}

/* ══════════════════════════════════════════════════════════════
   updateNode
   ══════════════════════════════════════════════════════════════ */
bool GraphMemory::updateNode(const QString& id,
                              const QString& content,
                              float          importance)
{
#ifdef HAVE_QT_SQL
    if (!m_open) return false;
    QSqlDatabase db = QSqlDatabase::database(m_connName);
    QSqlQuery q(db);
    if (importance < 0.0f) {
        q.prepare("UPDATE gm_nodes SET content=:c WHERE id=:id");
        q.bindValue(":c",  content);
        q.bindValue(":id", id);
    } else {
        q.prepare("UPDATE gm_nodes SET content=:c, importance=:i WHERE id=:id");
        q.bindValue(":c",  content);
        q.bindValue(":i",  importance);
        q.bindValue(":id", id);
    }
    /* exec() ritorna true anche se WHERE non matcha — usiamo numRowsAffected
       per distinguere "query ok" da "nodo trovato e modificato". */
    if (!q.exec()) return false;
    const bool found = q.numRowsAffected() > 0;
    if (found) emit changed();
    return found;
#else
    Q_UNUSED(id) Q_UNUSED(content) Q_UNUSED(importance)
    return false;
#endif
}

/* ══════════════════════════════════════════════════════════════
   removeNode
   ══════════════════════════════════════════════════════════════ */
bool GraphMemory::removeNode(const QString& id)
{
#ifdef HAVE_QT_SQL
    if (!m_open) return false;
    QSqlDatabase db = QSqlDatabase::database(m_connName);
    QSqlQuery q(db);
    q.prepare("DELETE FROM gm_nodes WHERE id=:id");
    q.bindValue(":id", id);
    if (!q.exec()) return false;
    const bool found = q.numRowsAffected() > 0;
    if (found) emit changed();
    return found;
#else
    Q_UNUSED(id)
    return false;
#endif
}

/* ══════════════════════════════════════════════════════════════
   allNodes / allEdges
   ══════════════════════════════════════════════════════════════ */
QVector<GmNode> GraphMemory::allNodes(const QString& typeFilter) const
{
    QVector<GmNode> result;
#ifdef HAVE_QT_SQL
    if (!m_open) return result;
    QSqlDatabase db = QSqlDatabase::database(m_connName);
    QSqlQuery q(db);
    if (typeFilter.isEmpty())
        q.exec("SELECT * FROM gm_nodes ORDER BY importance DESC");
    else {
        q.prepare("SELECT * FROM gm_nodes WHERE type=:t ORDER BY importance DESC");
        q.bindValue(":t", typeFilter);
        q.exec();
    }
    while (q.next()) {
        QVariantMap row;
        for (int i = 0; i < q.record().count(); ++i)
            row[q.record().fieldName(i)] = q.value(i);
        result.append(rowToNode(row));
    }
#endif
    return result;
}

QVector<GmEdge> GraphMemory::allEdges() const
{
    QVector<GmEdge> result;
#ifdef HAVE_QT_SQL
    if (!m_open) return result;
    QSqlDatabase db = QSqlDatabase::database(m_connName);
    QSqlQuery q(db);
    q.exec("SELECT * FROM gm_edges");
    while (q.next()) {
        QVariantMap row;
        for (int i = 0; i < q.record().count(); ++i)
            row[q.record().fieldName(i)] = q.value(i);
        result.append(rowToEdge(row));
    }
#endif
    return result;
}

/* ══════════════════════════════════════════════════════════════
   neighbours — BFS entro depth hop
   ══════════════════════════════════════════════════════════════ */
QVector<GmNode> GraphMemory::neighbours(const QString& nodeId, int depth) const
{
    QVector<GmNode> result;
#ifdef HAVE_QT_SQL
    if (!m_open) return result;
    QSqlDatabase db = QSqlDatabase::database(m_connName);

    QSet<QString>    visited;
    QQueue<QPair<QString,int>> queue;
    queue.enqueue({nodeId, 0});
    visited.insert(nodeId);

    while (!queue.isEmpty()) {
        auto [currId, d] = queue.dequeue();
        if (d >= depth) continue;

        QSqlQuery q(db);
        q.prepare(
            "SELECT DISTINCT n.* FROM gm_nodes n"
            " JOIN gm_edges e ON (e.from_id=:id AND e.to_id=n.id)"
            "              OR   (e.to_id=:id2 AND e.from_id=n.id)"
        );
        q.bindValue(":id",  currId);
        q.bindValue(":id2", currId);
        q.exec();

        while (q.next()) {
            const QString nid = q.value("id").toString();
            if (visited.contains(nid)) continue;
            visited.insert(nid);
            QVariantMap row;
            for (int i = 0; i < q.record().count(); ++i)
                row[q.record().fieldName(i)] = q.value(i);
            result.append(rowToNode(row));
            queue.enqueue({nid, d + 1});
        }
    }
#else
    Q_UNUSED(nodeId) Q_UNUSED(depth)
#endif
    return result;
}

/* ══════════════════════════════════════════════════════════════
   searchNodes — ricerca testuale
   ══════════════════════════════════════════════════════════════ */
QVector<GmNode> GraphMemory::searchNodes(const QString& query, int limit) const
{
    QVector<GmNode> result;
#ifdef HAVE_QT_SQL
    if (!m_open || query.isEmpty()) return result;
    QSqlDatabase db = QSqlDatabase::database(m_connName);
    QSqlQuery q(db);
    const QString like = "%" + query + "%";
    q.prepare(
        "SELECT * FROM gm_nodes"
        " WHERE label LIKE :q COLLATE NOCASE OR content LIKE :q2 COLLATE NOCASE"
        " ORDER BY importance DESC LIMIT :lim"
    );
    q.bindValue(":q",   like);
    q.bindValue(":q2",  like);
    q.bindValue(":lim", limit);
    q.exec();
    while (q.next()) {
        QVariantMap row;
        for (int i = 0; i < q.record().count(); ++i)
            row[q.record().fieldName(i)] = q.value(i);
        result.append(rowToNode(row));
    }
#else
    Q_UNUSED(query) Q_UNUSED(limit)
#endif
    return result;
}

/* ══════════════════════════════════════════════════════════════
   nodeByLabel / nodeById
   ══════════════════════════════════════════════════════════════ */
std::optional<GmNode> GraphMemory::nodeByLabel(const QString& label) const
{
#ifdef HAVE_QT_SQL
    if (!m_open) return std::nullopt;
    QSqlDatabase db = QSqlDatabase::database(m_connName);
    QSqlQuery q(db);
    q.prepare("SELECT * FROM gm_nodes WHERE label=:l COLLATE NOCASE LIMIT 1");
    q.bindValue(":l", label);
    if (q.exec() && q.next()) {
        QVariantMap row;
        for (int i = 0; i < q.record().count(); ++i)
            row[q.record().fieldName(i)] = q.value(i);
        return rowToNode(row);
    }
#else
    Q_UNUSED(label)
#endif
    return std::nullopt;
}

std::optional<GmNode> GraphMemory::nodeById(const QString& id) const
{
#ifdef HAVE_QT_SQL
    if (!m_open) return std::nullopt;
    QSqlDatabase db = QSqlDatabase::database(m_connName);
    QSqlQuery q(db);
    q.prepare("SELECT * FROM gm_nodes WHERE id=:id LIMIT 1");
    q.bindValue(":id", id);
    if (q.exec() && q.next()) {
        QVariantMap row;
        for (int i = 0; i < q.record().count(); ++i)
            row[q.record().fieldName(i)] = q.value(i);
        return rowToNode(row);
    }
#else
    Q_UNUSED(id)
#endif
    return std::nullopt;
}

/* ══════════════════════════════════════════════════════════════
   Statistiche
   ══════════════════════════════════════════════════════════════ */
int GraphMemory::nodeCount() const
{
#ifdef HAVE_QT_SQL
    if (!m_open) return 0;
    QSqlQuery q(QSqlDatabase::database(m_connName));
    q.exec("SELECT COUNT(*) FROM gm_nodes");
    return q.next() ? q.value(0).toInt() : 0;
#else
    return 0;
#endif
}

int GraphMemory::edgeCount() const
{
#ifdef HAVE_QT_SQL
    if (!m_open) return 0;
    QSqlQuery q(QSqlDatabase::database(m_connName));
    q.exec("SELECT COUNT(*) FROM gm_edges");
    return q.next() ? q.value(0).toInt() : 0;
#else
    return 0;
#endif
}

/* ══════════════════════════════════════════════════════════════
   toDot — export Graphviz
   ══════════════════════════════════════════════════════════════ */
QString GraphMemory::toDot(const QString& title, int maxNodes) const
{
    const auto nodes = allNodes();
    const auto edges = allEdges();

    /* Palette colori per tipo nodo */
    auto nodeColor = [](const QString& type) -> QString {
        if (type == "entity")    return "#4fc3f7";
        if (type == "concept")   return "#a5d6a7";
        if (type == "fact")      return "#fff176";
        if (type == "task")      return "#ffab91";
        if (type == "result")    return "#ce93d8";
        if (type == "rag_chunk") return "#80cbc4";
        return "#b0bec5";
    };

    QString dot;
    dot += "digraph \"" + title + "\" {\n";
    dot += "  rankdir=LR;\n";
    dot += "  bgcolor=\"#0f1117\";\n";
    dot += "  node [style=filled,fontcolor=\"#e0e0e0\",fontname=\"Arial\",fontsize=10];\n";
    dot += "  edge [color=\"#555577\",fontcolor=\"#aaaacc\",fontsize=8];\n\n";

    const int n = std::min(maxNodes, (int)nodes.size());
    for (int i = 0; i < n; ++i) {
        const auto& nd = nodes[i];
        const QString lbl = nd.label.left(28).replace("\"","'");
        const QString col = nodeColor(nd.type);
        dot += QString("  \"%1\" [label=\"%2\\n(%3)\",fillcolor=\"%4\"];\n")
               .arg(nd.id, lbl, nd.type, col);
    }

    /* Raccogli id nodi inclusi */
    QSet<QString> included;
    for (int i = 0; i < n; ++i) included.insert(nodes[i].id);

    dot += "\n";
    for (const auto& e : edges) {
        if (!included.contains(e.fromId) || !included.contains(e.toId)) continue;
        dot += QString("  \"%1\" -> \"%2\" [label=\"%3\",weight=%4];\n")
               .arg(e.fromId, e.toId, e.relation).arg(e.weight, 0, 'f', 1);
    }

    dot += "}\n";
    return dot;
}

/* ══════════════════════════════════════════════════════════════
   toJson
   ══════════════════════════════════════════════════════════════ */
QString GraphMemory::toJson(int maxNodes) const
{
    QJsonArray nodesArr, edgesArr;

    const auto nodes = allNodes();
    const int n = std::min(maxNodes, (int)nodes.size());
    QSet<QString> included;
    for (int i = 0; i < n; ++i) {
        const auto& nd = nodes[i];
        included.insert(nd.id);
        QJsonObject o;
        o["id"]         = nd.id;
        o["type"]       = nd.type;
        o["label"]      = nd.label;
        o["content"]    = nd.content.left(300);
        o["importance"] = nd.importance;
        nodesArr.append(o);
    }

    for (const auto& e : allEdges()) {
        if (!included.contains(e.fromId) || !included.contains(e.toId)) continue;
        QJsonObject o;
        o["id"]       = e.id;
        o["from"]     = e.fromId;
        o["to"]       = e.toId;
        o["relation"] = e.relation;
        o["weight"]   = e.weight;
        edgesArr.append(o);
    }

    QJsonObject root;
    root["nodes"] = nodesArr;
    root["edges"] = edgesArr;
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

/* ══════════════════════════════════════════════════════════════
   exportTxt — snapshot testuale per memoria fallback
   ══════════════════════════════════════════════════════════════ */
bool GraphMemory::exportTxt(const QString& path, int maxNodes) const
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;

    QTextStream out(&f);
    out << "# GraphMemory snapshot — " << QDateTime::currentDateTime().toString() << "\n";
    out << "# Nodi: " << nodeCount() << "  Archi: " << edgeCount() << "\n\n";

    const auto nodes = allNodes();
    const int n = std::min(maxNodes, (int)nodes.size());
    for (int i = 0; i < n; ++i) {
        const auto& nd = nodes[i];
        out << "## [" << nd.type.toUpper() << "] " << nd.label << "\n";
        if (!nd.content.isEmpty())
            out << nd.content.left(500) << "\n";
        out << "\n";
    }

    out << "\n# Relazioni\n";
    for (const auto& e : allEdges())
        out << e.fromId.left(8) << " --[" << e.relation << "]--> " << e.toId.left(8) << "\n";

    return true;
}

/* ══════════════════════════════════════════════════════════════
   pruneByImportance
   ══════════════════════════════════════════════════════════════ */
void GraphMemory::pruneByImportance(int keepTopN)
{
#ifdef HAVE_QT_SQL
    if (!m_open || keepTopN <= 0) return;
    QSqlDatabase db = QSqlDatabase::database(m_connName);
    QSqlQuery q(db);

    /* Trova id da eliminare (quelli con rank > keepTopN) */
    q.prepare(
        "DELETE FROM gm_nodes WHERE id NOT IN ("
        "  SELECT id FROM gm_nodes ORDER BY importance DESC LIMIT :n"
        ")"
    );
    q.bindValue(":n", keepTopN);
    if (q.exec()) emit changed();
#else
    Q_UNUSED(keepTopN)
#endif
}

/* ══════════════════════════════════════════════════════════════
   clearAll
   ══════════════════════════════════════════════════════════════ */
void GraphMemory::clearAll()
{
#ifdef HAVE_QT_SQL
    if (!m_open) return;
    QSqlDatabase db = QSqlDatabase::database(m_connName);
    QSqlQuery q(db);
    q.exec("DELETE FROM gm_edges");
    q.exec("DELETE FROM gm_nodes");
    emit changed();
#endif
}
