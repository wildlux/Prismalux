#include "chat_history.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <QUuid>

/* ══════════════════════════════════════════════════════════════
   Costruttore — apre (o crea) il database SQLite
   Il file viene salvato in:
     Linux/macOS: ~/.local/share/Prismalux/chat_history.db
     Windows:     %APPDATA%\Prismalux\chat_history.db
   ══════════════════════════════════════════════════════════════ */
ChatHistoryDb::ChatHistoryDb(QObject* parent)
    : QObject(parent)
    , m_connName(QUuid::createUuid().toString(QUuid::Id128))
{
    const QString dataDir = QStandardPaths::writableLocation(
                                QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(dataDir);
    const QString dbPath = dataDir + "/chat_history.db";

    m_db = QSqlDatabase::addDatabase("QSQLITE", m_connName);
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        qWarning() << "[ChatHistoryDb] impossibile aprire DB:" << m_db.lastError().text();
        return;
    }
    initSchema();
    m_ready = true;
}

ChatHistoryDb::~ChatHistoryDb() {
    m_db.close();
    QSqlDatabase::removeDatabase(m_connName);
}

/* ══════════════════════════════════════════════════════════════
   initSchema — crea la tabella se non esiste
   ══════════════════════════════════════════════════════════════ */
void ChatHistoryDb::initSchema() {
    QSqlQuery q(m_db);
    q.exec(
        "CREATE TABLE IF NOT EXISTS chats ("
        "  id        INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  mode      TEXT NOT NULL,"
        "  model     TEXT NOT NULL,"
        "  task      TEXT NOT NULL,"
        "  response  TEXT NOT NULL,"
        "  timestamp TEXT NOT NULL,"
        "  images    TEXT NOT NULL DEFAULT ''"
        ")"
    );
    /* Indice per ordinamento rapido */
    q.exec("CREATE INDEX IF NOT EXISTS idx_ts ON chats(timestamp DESC)");
}

/* ══════════════════════════════════════════════════════════════
   saveChat — inserisce una nuova riga
   ══════════════════════════════════════════════════════════════ */
int ChatHistoryDb::saveChat(const QString& mode, const QString& model,
                             const QString& task, const QString& response,
                             const QString& images)
{
    if (!m_ready) return -1;
    QSqlQuery q(m_db);
    q.prepare(
        "INSERT INTO chats(mode, model, task, response, timestamp, images) "
        "VALUES(?, ?, ?, ?, ?, ?)"
    );
    q.addBindValue(mode);
    q.addBindValue(model);
    q.addBindValue(task);
    q.addBindValue(response);
    q.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    q.addBindValue(images);
    if (!q.exec()) {
        qWarning() << "[ChatHistoryDb] saveChat error:" << q.lastError().text();
        return -1;
    }
    return q.lastInsertId().toInt();
}

/* ══════════════════════════════════════════════════════════════
   loadAll — tutte le chat, più recenti prima (max 200)
   ══════════════════════════════════════════════════════════════ */
QVector<ChatEntry> ChatHistoryDb::loadAll() const {
    QVector<ChatEntry> result;
    if (!m_ready) return result;
    QSqlQuery q(m_db);
    q.exec("SELECT id,mode,model,task,response,timestamp,images "
           "FROM chats ORDER BY timestamp DESC LIMIT 200");
    while (q.next()) {
        ChatEntry e;
        e.id        = q.value(0).toInt();
        e.mode      = q.value(1).toString();
        e.model     = q.value(2).toString();
        e.task      = q.value(3).toString();
        e.response  = q.value(4).toString();
        e.timestamp = q.value(5).toString();
        e.images    = q.value(6).toString();
        result.append(e);
    }
    return result;
}

/* ══════════════════════════════════════════════════════════════
   loadChat — singola chat per id
   ══════════════════════════════════════════════════════════════ */
ChatEntry ChatHistoryDb::loadChat(int id) const {
    ChatEntry e;
    if (!m_ready) return e;
    QSqlQuery q(m_db);
    q.prepare("SELECT id,mode,model,task,response,timestamp,images FROM chats WHERE id=?");
    q.addBindValue(id);
    if (q.exec() && q.next()) {
        e.id        = q.value(0).toInt();
        e.mode      = q.value(1).toString();
        e.model     = q.value(2).toString();
        e.task      = q.value(3).toString();
        e.response  = q.value(4).toString();
        e.timestamp = q.value(5).toString();
        e.images    = q.value(6).toString();
    }
    return e;
}

/* ══════════════════════════════════════════════════════════════
   deleteChat — elimina per id
   ══════════════════════════════════════════════════════════════ */
bool ChatHistoryDb::deleteChat(int id) {
    if (!m_ready) return false;
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM chats WHERE id=?");
    q.addBindValue(id);
    return q.exec() && q.numRowsAffected() > 0;
}
