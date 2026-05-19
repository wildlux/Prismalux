#include "ai_client.h"
#include "local_llm_client.h"
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSettings>
#include <QUrl>

AiClient::AiClient(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    /* Ripristina token salvato — così funziona anche senza passare da Settings */
    QSettings s("Prismalux", "Mobile");
    m_token = s.value("server/token", "").toString();
}

/* ── Helper: aggiunge Authorization se c'è un token ── */
static void addAuthHeader(QNetworkRequest& req, const QString& token)
{
    if (!token.isEmpty())
        req.setRawHeader("Authorization", ("Bearer " + token).toUtf8());
}

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

/* ── setLocalLlm — collega LocalLlmClient e ne ritrasmette i segnali ── */
void AiClient::setLocalLlm(LocalLlmClient* llm)
{
    m_localLlm = llm;
    if (!llm) return;
    /* Ritrasmette i segnali locali come segnali di AiClient:
       tutte le pagine (Chat, Studio, Lavoro…) funzionano senza modifiche. */
    connect(llm, &LocalLlmClient::token,    this, &AiClient::token);
    connect(llm, &LocalLlmClient::finished, this, &AiClient::finished);
    connect(llm, &LocalLlmClient::error,    this, &AiClient::error);
    connect(llm, &LocalLlmClient::aborted,  this, &AiClient::aborted);
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
    /* Delega al LLM locale se attivato e modello caricato */
    if (m_localMode && m_localLlm && m_localLlm->isLoaded()) {
        if (!m_localLlm->busy()) {
            ++m_reqId;
            m_localLlm->chat(sys, msg);
        }
        return m_reqId;
    }

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
    /* Disabilita think mode per modelli qwen3/deepseek-r1 — evita attese di 10s+ su mobile */
    body["think"]    = false;

    QNetworkRequest req{QUrl{QString("http://%1:%2/api/chat").arg(m_host).arg(m_port)}};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    addAuthHeader(req, m_token);
    m_reply = m_nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(m_reply, &QNetworkReply::readyRead, this, &AiClient::onReadyRead);
    connect(m_reply, &QNetworkReply::finished,  this, &AiClient::onFinished);
    return m_reqId;
}

/* ── chatWithImage ──────────────────────────────────────────────
   Ollama multimodal: invia l'immagine come base64 nel campo "images"
   del messaggio utente. Richiede un modello vision (es. llama3.2-vision). */
quint64 AiClient::chatWithImage(const QString& sys, const QString& msg,
                                const QByteArray& imgData, const QString& mimeType)
{
    Q_UNUSED(mimeType)  // Ollama accetta qualsiasi immagine come base64
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
    QJsonObject u;
    u["role"]    = "user";
    u["content"] = msg;
    u["images"]  = QJsonArray{ QString::fromLatin1(imgData.toBase64()) };
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

    QNetworkRequest req{QUrl{QString("http://%1:%2/api/chat").arg(m_host).arg(m_port)}};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    addAuthHeader(req, m_token);
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

    QNetworkRequest req{QUrl{QString("http://%1:%2/api/generate").arg(m_host).arg(m_port)}};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    addAuthHeader(req, m_token);
    m_reply = m_nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(m_reply, &QNetworkReply::readyRead, this, &AiClient::onReadyRead);
    connect(m_reply, &QNetworkReply::finished,  this, &AiClient::onFinished);
    return m_reqId;
}

/* ── fetchModels ─────────────────────────────────────────────── */
void AiClient::fetchModels()
{
    QNetworkRequest req{QUrl{QString("http://%1:%2/api/tags").arg(m_host).arg(m_port)}};
    addAuthHeader(req, m_token);
    QNetworkReply* r = m_nam->get(req);
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
