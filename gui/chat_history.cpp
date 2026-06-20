#include "chat_history.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QSaveFile>
#include <QFileInfoList>
#include <QAtomicInt>
#include <algorithm>

ChatHistory::ChatHistory()
{
    m_dir = QDir(QDir::homePath() + "/.prismalux_chats");
    if (!m_dir.exists())
        m_dir.mkpath(".");
}

QString ChatHistory::sessionPath(const QString& id) const {
    return m_dir.filePath(id + ".json");
}

QVector<ChatSession> ChatHistory::list() const {
    QVector<ChatSession> result;
    const auto entries = m_dir.entryInfoList({"*.json"}, QDir::Files, QDir::Time);
    for (const auto& fi : entries) {
        QFile f(fi.absoluteFilePath());
        if (!f.open(QIODevice::ReadOnly)) continue;
        const auto doc = QJsonDocument::fromJson(f.readAll());
        if (!doc.isObject()) continue;
        const auto obj = doc.object();
        ChatSession s;
        s.id        = obj["id"].toString();
        s.title     = obj["title"].toString();
        s.createdAt = QDateTime::fromString(obj["created"].toString(), Qt::ISODate);
        if (s.id.isEmpty()) continue;
        result << s;
    }
    return result;
}

QString ChatHistory::newSession(const QString& firstTask) {
    static QAtomicInt s_seq(0);
    ChatSession s;
    s.id = QString::number(QDateTime::currentMSecsSinceEpoch())
         + "_" + QString::number(s_seq.fetchAndAddRelaxed(1));
    s.title     = firstTask.left(40).trimmed();
    s.createdAt = QDateTime::currentDateTime();

    QJsonObject obj;
    obj["id"]      = s.id;
    obj["title"]   = s.title;
    obj["created"] = s.createdAt.toString(Qt::ISODate);
    obj["log"]     = "";

    QSaveFile f(sessionPath(s.id));
    if (f.open(QIODevice::WriteOnly))
        if (f.write(QJsonDocument(obj).toJson()) >= 0)
            f.commit();

    /* Mantieni al massimo kMaxSessions sessioni — elimina le più vecchie */
    pruneOldSessions();
    return s.id;
}

void ChatHistory::pruneOldSessions(int maxSessions) {
    const auto entries = m_dir.entryInfoList({"*.json"}, QDir::Files, QDir::Time);
    if (entries.size() <= maxSessions) return;
    /* entryInfoList(QDir::Time) ordina dalla più recente — elimina le ultime */
    for (int i = maxSessions; i < entries.size(); ++i)
        QFile::remove(entries[i].absoluteFilePath());
}

void ChatHistory::saveSession(const QString& sessionId,
                              const QString& logHtml,
                              const QMap<int,QString>& bubbleTexts,
                              const QMap<int,QPair<QString,QString>>& codeBlocks,
                              const QJsonArray& autoHistory)
{
    const QString path = sessionPath(sessionId);

    /* Una sola lettura per preservare i metadati (id/title/created) */
    QFile fr(path);
    QJsonObject obj;
    if (fr.open(QIODevice::ReadOnly)) {
        auto doc = QJsonDocument::fromJson(fr.readAll());
        if (doc.isObject()) obj = doc.object();
        fr.close();
    }

    obj["log"] = logHtml;

    if (!autoHistory.isEmpty())
        obj["auto_history"] = autoHistory;

    QJsonObject btObj;
    for (auto it = bubbleTexts.cbegin(); it != bubbleTexts.cend(); ++it)
        btObj[QString::number(it.key())] = it.value();
    obj["bubble_texts"] = btObj;

    QJsonObject cbObj;
    for (auto it = codeBlocks.cbegin(); it != codeBlocks.cend(); ++it) {
        QJsonObject entry;
        entry["lang"] = it.value().first;
        entry["code"] = it.value().second;
        cbObj[QString::number(it.key())] = entry;
    }
    obj["code_blocks"] = cbObj;

    /* Una sola scrittura atomica */
    QSaveFile fw(path);
    if (fw.open(QIODevice::WriteOnly))
        if (fw.write(QJsonDocument(obj).toJson()) >= 0)
            fw.commit();
}

void ChatHistory::saveLog(const QString& sessionId, const QString& logHtml) {
    saveSession(sessionId, logHtml, {}, {}, {});
}

QString ChatHistory::loadLog(const QString& sessionId) const {
    QFile f(sessionPath(sessionId));
    if (!f.open(QIODevice::ReadOnly)) return {};
    auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return {};
    return doc.object()["log"].toString();
}

QJsonArray ChatHistory::loadAutoHistory(const QString& sessionId) const
{
    QFile f(sessionPath(sessionId));
    if (!f.open(QIODevice::ReadOnly)) return {};
    auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return {};
    return doc.object()["auto_history"].toArray();
}

void ChatHistory::loadMaps(const QString& sessionId,
                           QMap<int,QString>& bubbleTexts,
                           QMap<int,QPair<QString,QString>>& codeBlocks) const
{
    bubbleTexts.clear();
    codeBlocks.clear();

    QFile f(sessionPath(sessionId));
    if (!f.open(QIODevice::ReadOnly)) return;
    const auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return;
    const QJsonObject obj = doc.object();

    const QJsonObject btObj = obj["bubble_texts"].toObject();
    for (auto it = btObj.constBegin(); it != btObj.constEnd(); ++it)
        bubbleTexts[it.key().toInt()] = it.value().toString();

    const QJsonObject cbObj = obj["code_blocks"].toObject();
    for (auto it = cbObj.constBegin(); it != cbObj.constEnd(); ++it) {
        const QJsonObject e = it.value().toObject();
        codeBlocks[it.key().toInt()] = { e["lang"].toString(), e["code"].toString() };
    }
}

void ChatHistory::remove(const QString& sessionId) {
    QFile::remove(sessionPath(sessionId));
}
