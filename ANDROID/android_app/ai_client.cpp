#include "ai_client.h"
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSettings>
#include <QUrl>

AiClient::AiClient(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{}

/* ── setServer ───────────────────────────────────────────────── */
void AiClient::setServer(const QString& host, int port, const QString& model)
{
    m_host = host;
    m_port = port;
    if (!model.isEmpty() && model != m_model) {
        m_model = model;
        emit modelChanged(model);
    } else {
        m_model = model;
    }
    /* Salva subito in QSettings per riapertura app */
    QSettings s("Prismalux", "Mobile");
    s.setValue("server/host",  m_host);
    s.setValue("server/port",  m_port);
    s.setValue("server/model", m_model);
}

/* ── abort ───────────────────────────────────────────────────── */
void AiClient::abort()
{
    m_generateMode = false;
    if (!m_reply) return;
    QNetworkReply* r = m_reply;
    m_reply = nullptr;
    r->abort();
    r->deleteLater();
    m_accum.clear();
    emit aborted();
}

/* ── chat ─────────────────────────────────────────────────────── */
quint64 AiClient::chat(const QString& sys, const QString& msg,
                       const QJsonArray& history)
{
    if (busy()) return m_reqId;
    if (m_throttle.isValid() && m_throttle.elapsed() < 300) return m_reqId;
    m_throttle.restart();
    ++m_reqId;

    m_generateMode = false;
    m_accum.clear();

    QJsonArray messages;
    if (!sys.isEmpty()) {
        QJsonObject s; s["role"] = "system"; s["content"] = sys;
        messages.append(s);
    }
    for (const QJsonValue& v : history) messages.append(v);
    QJsonObject u; u["role"] = "user"; u["content"] = msg;
    messages.append(u);

    QJsonObject opts;
    opts["temperature"] = m_temperature;
    opts["num_predict"] = m_numPredict;
    opts["num_ctx"]     = m_numCtx;

    QJsonObject body;
    body["model"]    = m_model;
    body["stream"]   = true;
    body["messages"] = messages;
    body["options"]  = opts;

    const QString url = QString("http://%1:%2/api/chat").arg(m_host).arg(m_port);
    QNetworkRequest req(QUrl(url));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    m_reply = m_nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(m_reply, &QNetworkReply::readyRead, this, &AiClient::onReadyRead);
    connect(m_reply, &QNetworkReply::finished,  this, &AiClient::onFinished);
    return m_reqId;
}

/* ── generate ───────────────────────────────────────────────────
   /api/generate è più leggero di /api/chat per query RAG
   single-shot senza history. */
quint64 AiClient::generate(const QString& sys, const QString& prompt)
{
    if (busy()) return m_reqId;
    if (m_throttle.isValid() && m_throttle.elapsed() < 300) return m_reqId;
    m_throttle.restart();
    ++m_reqId;

    m_generateMode = true;
    m_accum.clear();

    QJsonObject opts;
    opts["temperature"] = m_temperature;
    opts["num_predict"] = m_numPredict;
    opts["num_ctx"]     = m_numCtx;

    QJsonObject body;
    body["model"]   = m_model;
    body["stream"]  = true;
    body["prompt"]  = prompt;
    body["options"] = opts;
    if (!sys.isEmpty()) body["system"] = sys;

    const QString url = QString("http://%1:%2/api/generate").arg(m_host).arg(m_port);
    QNetworkRequest req(QUrl(url));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    m_reply = m_nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(m_reply, &QNetworkReply::readyRead, this, &AiClient::onReadyRead);
    connect(m_reply, &QNetworkReply::finished,  this, &AiClient::onFinished);
    return m_reqId;
}

/* ── fetchModels ─────────────────────────────────────────────── */
void AiClient::fetchModels()
{
    const QString url = QString("http://%1:%2/api/tags").arg(m_host).arg(m_port);
    QNetworkReply* r = m_nam->get(QNetworkRequest(QUrl(url)));
    connect(r, &QNetworkReply::finished, this, &AiClient::onModelsReply);
}

void AiClient::onModelsReply()
{
    auto* r = qobject_cast<QNetworkReply*>(sender());
    if (!r) return;
    r->deleteLater();
    if (r->error() != QNetworkReply::NoError) { emit modelsReady({}); return; }
    QStringList list;
    const QJsonObject obj = QJsonDocument::fromJson(r->readAll()).object();
    for (auto v : obj["models"].toArray())
        list << v.toObject()["name"].toString();
    emit modelsReady(list);
}

/* ── onReadyRead — parsing NDJSON streaming ──────────────────── */
void AiClient::onReadyRead()
{
    if (!m_reply) return;
    for (const QByteArray& raw : m_reply->readAll().split('\n')) {
        const QByteArray line = raw.trimmed();
        if (line.isEmpty()) continue;

        /* Errore inline Ollama {"error":"..."} */
        if (line.startsWith('{') && line.contains("\"error\"")) {
            const QString msg =
                QJsonDocument::fromJson(line).object()["error"].toString();
            if (!msg.isEmpty()) { emit error(msg); return; }
        }

        const QJsonObject obj = QJsonDocument::fromJson(line).object();
        if (obj.isEmpty()) continue;

        QString chunk;
        if (m_generateMode)
            chunk = obj["response"].toString();
        else
            chunk = obj["message"].toObject()["content"].toString();

        if (!chunk.isEmpty()) { m_accum += chunk; emit token(chunk); }
    }
}

/* ── onFinished ──────────────────────────────────────────────── */
void AiClient::onFinished()
{
    if (!m_reply) return;
    QNetworkReply* r = m_reply;
    m_reply = nullptr;
    m_generateMode = false;

    if (r->error() != QNetworkReply::NoError &&
        r->error() != QNetworkReply::OperationCanceledError)
        emit error(r->errorString());

    r->deleteLater();
    emit finished(m_accum);
    m_accum.clear();
}
