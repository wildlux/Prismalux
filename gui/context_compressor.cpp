#include "context_compressor.h"
#include "ai_client.h"
#include "prismalux_paths.h"
namespace P = PrismaluxPaths;
#include <QSettings>
#include <QJsonDocument>
#include <QNetworkRequest>

/* ── default: 2 × kChatMaxTurns (es. 3 turni → 6 messaggi) ── */
ContextCompressor::ContextCompressor(AiClient* ai, QObject* parent)
    : QObject(parent), m_ai(ai)
{
    const int turns = QSettings("Prismalux", "GUI")
                          .value(P::SK::kChatMaxTurns, 3).toInt();
    m_maxMsgs = qBound(2, turns, 20) * 2;
}

void ContextCompressor::setMaxRecentMessages(int n)
{
    m_maxMsgs = qBound(2, n, 100);
}

/* ── appendPair ─────────────────────────────────────────────── */
void ContextCompressor::appendPair(const QString& user, const QString& assistant)
{
    QJsonObject u; u["role"] = "user";      u["content"] = user;
    QJsonObject a; a["role"] = "assistant"; a["content"] = assistant.left(800);
    m_recent.append(u);
    m_recent.append(a);
    compressIfNeeded();
}

/* ── appendMessage ──────────────────────────────────────────── */
void ContextCompressor::appendMessage(const QString& role, const QString& content)
{
    QJsonObject m; m["role"] = role; m["content"] = content;
    m_recent.append(m);
    compressIfNeeded();
}

/* ── buildContext ───────────────────────────────────────────── */
QJsonArray ContextCompressor::buildContext() const
{
    if (m_summary.isEmpty())
        return m_recent;

    QJsonArray out;
    QJsonObject su; su["role"] = "user";
    su["content"] = "[Riepilogo dei messaggi precedenti della conversazione]";
    QJsonObject sa; sa["role"] = "assistant";
    sa["content"] = m_summary;
    out.append(su);
    out.append(sa);
    for (const auto& v : m_recent)
        out.append(v);
    return out;
}

/* ── clear ──────────────────────────────────────────────────── */
void ContextCompressor::clear()
{
    m_recent  = QJsonArray();
    m_summary.clear();
}

/* ── toJson / fromJson ──────────────────────────────────────── */
QJsonObject ContextCompressor::toJson() const
{
    QJsonObject o;
    o["recent"]  = m_recent;
    o["summary"] = m_summary;
    return o;
}

void ContextCompressor::fromJson(const QJsonObject& o)
{
    m_recent  = o["recent"].toArray();
    m_summary = o["summary"].toString();
}

void ContextCompressor::fromJsonArray(const QJsonArray& arr)
{
    m_recent  = arr;
    m_summary.clear();
}

/* ── compressIfNeeded ───────────────────────────────────────── */
void ContextCompressor::compressIfNeeded()
{
    if (m_recent.size() <= m_maxMsgs) return;

    const int toCompress = m_recent.size() - m_maxMsgs;
    QJsonArray overflow;
    for (int i = 0; i < toCompress; ++i)
        overflow.append(m_recent.at(i));

    QJsonArray remaining;
    for (int i = toCompress; i < m_recent.size(); ++i)
        remaining.append(m_recent.at(i));
    m_recent = remaining;

    summarizeAsync(overflow);
}

/* ── summarizeAsync ─────────────────────────────────────────── */
void ContextCompressor::summarizeAsync(const QJsonArray& overflow)
{
    if (overflow.isEmpty()) return;

    QString conv;
    for (const auto& v : overflow) {
        const QJsonObject msg = v.toObject();
        const QString role = msg["role"].toString();
        const QString content = msg["content"].toString();
        if (role == "user")
            conv += "Utente: " + content + "\n";
        else if (role == "assistant")
            conv += "AI: " + content.left(500) + "\n";
        else
            conv += role + ": " + content.left(300) + "\n";
        conv += "\n";
    }

    /* fallback testuale se il LLM non risponde */
    m_pendingFallback.clear();
    for (const auto& v : overflow) {
        const QJsonObject msg = v.toObject();
        m_pendingFallback += msg["role"].toString().left(1).toUpper() + ": "
                             + msg["content"].toString().left(200) + "\n";
    }

    const QString prompt =
        "Riassumi in italiano la seguente conversazione in modo SINTETICO ma "
        "COMPLETO, preservando TUTTI i fatti concreti menzionati "
        "(nomi, cifre, date, eventi, risultati). Massimo 400 parole:\n\n" + conv;

    QJsonObject body;
    QString endpoint;
    if (m_ai->backend() == AiClient::LlamaServer) {
        endpoint = QString("http://%1:%2/completion")
                       .arg(m_ai->host()).arg(m_ai->port());
        body["prompt"]    = prompt;
        body["n_predict"] = 600;
        body["stream"]    = false;
    } else {
        endpoint = QString("http://%1:%2/api/generate")
                       .arg(m_ai->host()).arg(m_ai->port());
        body["model"]  = m_ai->model();
        body["prompt"] = prompt;
        body["stream"] = false;
    }

    if (!m_nam)
        m_nam = new QNetworkAccessManager(this);

    QNetworkRequest req;
    req.setUrl(QUrl(endpoint));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    auto* reply = m_nam->post(req, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, &ContextCompressor::onSummaryFinished);
}

/* ── onSummaryFinished ──────────────────────────────────────── */
void ContextCompressor::onSummaryFinished()
{
    auto* r = qobject_cast<QNetworkReply*>(sender());
    if (!r) return;
    r->deleteLater();

    QString summary;
    if (r->error() == QNetworkReply::NoError) {
        const QJsonObject obj = QJsonDocument::fromJson(r->readAll()).object();
        summary = obj.contains("response")
                      ? obj["response"].toString().trimmed()
                      : obj["content"].toString().trimmed();
    }
    if (summary.isEmpty())
        summary = m_pendingFallback;

    if (!m_summary.isEmpty())
        m_summary += "\n\n";
    m_summary += summary;

    emit summaryUpdated();
}
