#include "chat_history.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QFileInfoList>
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
    ChatSession s;
    s.id        = QString::number(QDateTime::currentMSecsSinceEpoch());
    s.title     = firstTask.left(40).trimmed();
    s.createdAt = QDateTime::currentDateTime();

    QJsonObject obj;
    obj["id"]      = s.id;
    obj["title"]   = s.title;
    obj["created"] = s.createdAt.toString(Qt::ISODate);
    obj["log"]     = "";

    QFile f(sessionPath(s.id));
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(obj).toJson());
    return s.id;
}

void ChatHistory::saveLog(const QString& sessionId, const QString& logHtml) {
    const QString path = sessionPath(sessionId);
    QFile fr(path);
    QJsonObject obj;
    if (fr.open(QIODevice::ReadOnly)) {
        auto doc = QJsonDocument::fromJson(fr.readAll());
        if (doc.isObject()) obj = doc.object();
        fr.close();
    }
    obj["log"] = logHtml;
    QFile fw(path);
    if (fw.open(QIODevice::WriteOnly | QIODevice::Truncate))
        fw.write(QJsonDocument(obj).toJson());
}

QString ChatHistory::loadLog(const QString& sessionId) const {
    QFile f(sessionPath(sessionId));
    if (!f.open(QIODevice::ReadOnly)) return {};
    auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return {};
    return doc.object()["log"].toString();
}

void ChatHistory::remove(const QString& sessionId) {
    QFile::remove(sessionPath(sessionId));
}
