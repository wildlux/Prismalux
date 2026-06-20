/* ======================================================================
   lan_server_ai.cpp — Handler AI di LanServer
   /api/tags · /api/chat · /api/generate · /api/rag · /knowledge
   /web · / (index) · /apk
   Estratto da lan_server.cpp per ridurne le dimensioni.
   ====================================================================== */
#include "lan_server.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QFileInfo>
#include <QTextStream>
#include <QUrlQuery>
#include <QUrl>
#include "prismalux_paths.h"
namespace P = PrismaluxPaths;

/* ── /api/tags ───────────────────────────────────────────────────────────── */

void LanServer::handleTags(Session& s)
{
    /* Un solo fetch in corso alla volta */
    if (m_tagsSock) {
        sendError(s.socket, 503, "Tags fetch in progress");
        return;
    }
    m_tagsSock = s.socket;

    /* One-shot: disconnette dopo il primo fire */
    m_modelsConn = connect(m_ai, &AiClient::modelsReady,
                           this, &LanServer::onAiModelsReady,
                           Qt::SingleShotConnection);
    m_ai->fetchModels();
}

void LanServer::onAiModelsReady(const QStringList& models)
{
    QTcpSocket* sock = m_tagsSock;
    m_tagsSock = nullptr;
    if (!sock || !m_sessions.contains(sock)) return;

    QJsonArray arr;
    for (const QString& m : models) {
        QJsonObject obj;
        obj["name"]       = m;
        obj["model"]      = m;
        obj["modified_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        obj["size"]        = (qint64)0;
        QJsonObject details;
        details["family"] = "unknown";
        obj["details"]    = details;
        arr.append(obj);
    }
    QJsonObject root;
    root["models"] = arr;
    sendJson(sock, QJsonDocument(root).toJson(QJsonDocument::Compact));
}

/* ── /api/chat ───────────────────────────────────────────────────────────── */

void LanServer::handleChat(Session& s)
{
    if (checkChatRateLimit(s)) return;

    const QJsonDocument chatDoc = QJsonDocument::fromJson(s.body);
    if (chatDoc.isNull() || !chatDoc.isObject()) {
        sendError(s.socket, 400, "Invalid JSON"); return;
    }

    /* Se l'AI è occupata → accoda (max kMaxLlmQueue) */
    if (m_streamSock || m_ai->busy()) {
        if (m_llmQueue.size() >= kMaxLlmQueue) {
            sendError(s.socket, 429, "Queue full — retry later"); return;
        }
        const QJsonObject req  = chatDoc.object();
        const QJsonArray  msgs = req["messages"].toArray();
        PendingLlmRequest p;
        p.sock  = s.socket;
        p.model = req["model"].toString();
        for (int i = 0; i < msgs.size(); ++i) {
            const QJsonObject m  = msgs[i].toObject();
            const QString role   = m["role"].toString();
            const QString cont   = m["content"].toString();
            if (role == "system") { p.systemPrompt = cont; }
            else if (role == "user" && i == msgs.size() - 1) { p.userMsg = cont; }
            else { p.history.append(m); }
        }
        m_llmQueue.enqueue(p);
        return;   /* risposta arriverà quando tocca a questa richiesta */
    }

    /* chatDoc già parsato sopra */
    const QJsonObject req  = chatDoc.object();
    const QJsonArray  msgs = req["messages"].toArray();

    /* Se il client specifica un modello diverso, lo impostiamo prima di chattare */
    const QString reqModel = req["model"].toString();
    if (!reqModel.isEmpty() && reqModel != m_ai->model())
        m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), reqModel);

    /* Adotta num_ctx e num_predict dalla richiesta mobile */
    const QJsonObject opts = req["options"].toObject();
    if (!opts.isEmpty()) {
        AiChatParams p = m_ai->chatParams();
        if (opts.contains("num_ctx"))     p.num_ctx     = opts["num_ctx"].toInt(p.num_ctx);
        if (opts.contains("num_predict")) p.num_predict = opts["num_predict"].toInt(p.num_predict);
        m_ai->setChatParams(p);
    }

    /* Estrai system prompt, history intermedia, ultimo messaggio utente */
    QString    systemPrompt;
    QJsonArray history;
    QString    userMsg;

    for (int i = 0; i < msgs.size(); ++i) {
        const QJsonObject m    = msgs[i].toObject();
        const QString     role = m["role"].toString();
        const QString     cont = m["content"].toString();
        if (role == "system") { systemPrompt = cont; }
        else if (role == "user" && i == msgs.size() - 1) { userMsg = cont; }
        else { history.append(m); }
    }

    m_streamSock = s.socket;
    m_genMode    = false;

    disconnect(m_ai, &AiClient::token,    this, &LanServer::onAiToken);
    disconnect(m_ai, &AiClient::finished, this, &LanServer::onAiFinished);
    disconnect(m_ai, &AiClient::error,    this, &LanServer::onAiError);
    connect(m_ai, &AiClient::token,    this, &LanServer::onAiToken);
    connect(m_ai, &AiClient::finished, this, &LanServer::onAiFinished);
    connect(m_ai, &AiClient::error,    this, &LanServer::onAiError);

    s.socket->write(httpStreamHeader());
    s.socket->flush();

    m_ai->chat(systemPrompt, userMsg, history, AiClient::QueryAuto);
}

/* ── /api/generate ───────────────────────────────────────────────────────── */

void LanServer::handleGenerate(Session& s)
{
    if (checkChatRateLimit(s)) return;

    const QJsonDocument genDoc = QJsonDocument::fromJson(s.body);
    if (genDoc.isNull() || !genDoc.isObject()) {
        sendError(s.socket, 400, "Invalid JSON"); return;
    }
    const QJsonObject req = genDoc.object();

    /* Se l'AI è occupata → accoda */
    if (m_streamSock || m_ai->busy()) {
        if (m_llmQueue.size() >= kMaxLlmQueue) {
            sendError(s.socket, 429, "Queue full — retry later"); return;
        }
        PendingLlmRequest p;
        p.sock       = s.socket;
        p.isGenerate = true;
        p.genPrompt  = req["prompt"].toString();
        p.genSystem  = req["system"].toString();
        p.model      = req["model"].toString(m_ai->model());
        m_llmQueue.enqueue(p);
        return;
    }

    const QString model  = req["model"].toString(m_ai->model());
    const QString prompt = req["prompt"].toString();
    const QString system = req["system"].toString();

    if (!model.isEmpty() && model != m_ai->model())
        m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), model);

    m_streamSock = s.socket;
    m_genMode    = true;

    disconnect(m_ai, &AiClient::token,    this, &LanServer::onAiToken);
    disconnect(m_ai, &AiClient::finished, this, &LanServer::onAiFinished);
    disconnect(m_ai, &AiClient::error,    this, &LanServer::onAiError);
    connect(m_ai, &AiClient::token,    this, &LanServer::onAiToken);
    connect(m_ai, &AiClient::finished, this, &LanServer::onAiFinished);
    connect(m_ai, &AiClient::error,    this, &LanServer::onAiError);

    s.socket->write(httpStreamHeader());
    s.socket->flush();

    m_ai->generate(system, prompt, AiClient::QueryAuto);
}

/* ── callback AiClient ───────────────────────────────────────────────────── */

void LanServer::onAiToken(const QString& chunk)
{
    if (!m_streamSock) return;

    QJsonObject obj;
    obj["model"] = m_ai->model();
    obj["done"]  = false;

    if (m_genMode) {
        obj["response"] = chunk;
    } else {
        QJsonObject msg;
        msg["role"]    = "assistant";
        msg["content"] = chunk;
        obj["message"] = msg;
    }
    sendStreamLine(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void LanServer::onAiFinished(const QString& /*full*/)
{
    if (!m_streamSock) return;

    QJsonObject obj;
    obj["model"]      = m_ai->model();
    obj["done"]       = true;
    obj["done_reason"] = "stop";
    sendStreamLine(QJsonDocument(obj).toJson(QJsonDocument::Compact));

    closeStreamSession();
}

void LanServer::onAiError(const QString& msg)
{
    /* Errore durante fetchModels → risponde con lista vuota invece di lasciare il socket appeso */
    if (m_tagsSock && m_sessions.contains(m_tagsSock)) {
        QJsonObject root;
        root["models"] = QJsonArray();
        root["error"]  = msg;
        sendJson(m_tagsSock, QJsonDocument(root).toJson(QJsonDocument::Compact));
        m_tagsSock = nullptr;
    }

    if (!m_streamSock) return;

    /* Invia riga di errore nel flusso NDJSON */
    QJsonObject obj;
    obj["model"] = m_ai->model();
    obj["done"]  = true;
    obj["error"] = msg;
    sendStreamLine(QJsonDocument(obj).toJson(QJsonDocument::Compact));

    closeStreamSession();
}

/* ── /knowledge ──────────────────────────────────────────────────────────── */

void LanServer::handleKnowledge(Session& s)
{
    if (s.method == "GET") {
        const QString content = P::readUserKnowledge();
        QJsonObject obj;
        obj["content"]   = content;
        obj["available"] = !content.trimmed().isEmpty();
        sendJson(s.socket, QJsonDocument(obj).toJson(QJsonDocument::Compact));

    } else if (s.method == "POST") {
        /* Rate limiting: max 10 richieste/minuto per IP */
        if (!m_knowledgeRateTimer) {
            m_knowledgeRateTimer = new QTimer(this);
            m_knowledgeRateTimer->setInterval(60 * 1000);
            connect(m_knowledgeRateTimer, &QTimer::timeout,
                    this, &LanServer::onKnowledgeRateTimeout);
            m_knowledgeRateTimer->start();
        }
        const int reqCount = ++m_knowledgeReqCount[s.addr];
        if (reqCount > 10) {
            sendError(s.socket, 429, "Rate limit exceeded");
            return;
        }

        /* Cap payload a 32 KB */
        if (s.body.size() > 32 * 1024) {
            sendError(s.socket, 413, "Payload too large");
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(s.body);
        if (doc.isNull() || !doc.isObject()) {
            sendError(s.socket, 400, "Invalid JSON");
            return;
        }
        const QJsonObject req = doc.object();
        const QString text    = req["content"].toString().trimmed();
        const QString mode    = req["mode"].toString("append"); /* "append" | "replace" */

        if (text.isEmpty()) {
            sendError(s.socket, 400, "content field required");
            return;
        }

        QFile f(P::userKnowledgePath());
        const QIODevice::OpenMode flags = (mode == "replace")
            ? (QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)
            : (QIODevice::Append   | QIODevice::Text);

        if (!f.open(flags)) {
            sendError(s.socket, 500, "Cannot write knowledge file");
            return;
        }
        QTextStream ts(&f);
        if (mode != "replace") ts << "\n\n";
        ts << text;
        f.close();
        P::invalidateKnowledgeCache();

        sendJson(s.socket, R"({"ok":true})");

    } else {
        sendError(s.socket, 400, "Method not allowed");
    }
}

/* ── /api/rag — ricerca semantica nel RagEngine condiviso ───────────────── */

void LanServer::handleRag(Session& s)
{
    if (!m_rag) {
        sendError(s.socket, 503, "RAG non disponibile");
        return;
    }
    if (m_rag->chunkCount() == 0) {
        sendJson(s.socket, R"({"results":[],"info":"Indice RAG vuoto"})");
        return;
    }
    if (m_ragSock) {
        sendError(s.socket, 503, "RAG query in progress");
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(s.body);
    if (doc.isNull() || !doc.isObject()) {
        sendError(s.socket, 400, "Invalid JSON");
        return;
    }
    const QJsonObject req = doc.object();
    const QString query   = req["query"].toString().trimmed();
    if (query.isEmpty()) {
        sendError(s.socket, 400, "query field required");
        return;
    }

    m_ragSock = s.socket;

    /* Salva k per lo slot (letto prima che il socket venga consumato) */
    m_ragK = qBound(1, req["k"].toInt(5), 20);

    disconnect(m_ai, &AiClient::embeddingReady, this, &LanServer::onRagEmbeddingReady);
    disconnect(m_ai, &AiClient::embeddingError, this, &LanServer::onRagEmbeddingError);
    connect(m_ai, &AiClient::embeddingReady, this, &LanServer::onRagEmbeddingReady,
            Qt::SingleShotConnection);
    connect(m_ai, &AiClient::embeddingError, this, &LanServer::onRagEmbeddingError,
            Qt::SingleShotConnection);

    m_ai->fetchEmbedding(query);
}

void LanServer::onRagEmbeddingReady(const QVector<float>& vec)
{
    QTcpSocket* sock = m_ragSock;
    m_ragSock = nullptr;
    if (!sock || !m_sessions.contains(sock)) return;

    const QVector<RagChunk> results = m_rag->search(vec, m_ragK);

    /* Proietta il vettore query nello spazio JLT (256-dim) per calcolare i coseni */
    const QVector<float> qProj = m_rag->project(vec);

    QJsonArray arr;
    for (const RagChunk& c : results) {
        QJsonObject obj;
        obj["text"] = c.text;
        /* Calcola coseno tra qProj e c.vec (entrambi 256-dim) */
        float dot = 0.0f, na = 0.0f, nb = 0.0f;
        const int dim = qMin(qProj.size(), c.vec.size());
        for (int i = 0; i < dim; ++i) {
            dot += qProj[i] * c.vec[i];
            na  += qProj[i] * qProj[i];
            nb  += c.vec[i] * c.vec[i];
        }
        const float score = (na > 0.0f && nb > 0.0f)
                            ? dot / (std::sqrt(na) * std::sqrt(nb))
                            : 0.0f;
        obj["score"] = static_cast<double>(score);
        arr.append(obj);
    }
    QJsonObject root;
    root["results"] = arr;
    sendJson(sock, QJsonDocument(root).toJson(QJsonDocument::Compact));
}

void LanServer::onRagEmbeddingError(const QString& msg)
{
    QTcpSocket* sock = m_ragSock;
    m_ragSock = nullptr;
    if (!sock || !m_sessions.contains(sock)) return;

    QString safe = msg;
    safe.remove(QRegularExpression(R"([\r\n"\\])"));
    safe = safe.left(200);
    const QByteArray body = ("{\"error\":\"" + safe + "\"}").toUtf8();
    QByteArray resp = httpOkHeader("application/json");
    resp += "Content-Length: " + QByteArray::number(body.size()) + "\r\n\r\n";
    resp += body;
    sock->write(resp);
    sock->flush();
}

/* ── /web — interfaccia chat web per PC nella rete locale ───────────────── */
void LanServer::handleWebChat(Session& s)
{
    const QByteArray model = m_ai->model().isEmpty()
                             ? QByteArray("ollama")
                             : m_ai->model().toUtf8();

    const QByteArray authHeadersJs = [this]() -> QByteArray {
        if (m_accessToken.isEmpty())
            return "'Content-Type':'application/json'";
        QString safe = m_accessToken;
        safe.replace("\\", "\\\\").replace("'", "\\'");
        return QByteArray("'Content-Type':'application/json','Authorization':'Bearer ")
               + safe.toUtf8() + QByteArray("'");
    }();

    QFile f(":/lan/webchat.html");
    if (!f.open(QIODevice::ReadOnly)) {
        sendError(s.socket, 500, "webchat.html not found in resources");
        return;
    }
    QByteArray html = f.readAll();
    html.replace("{{MODEL}}", model);
    html.replace("{{AUTH_HEADERS_JS}}", authHeadersJs);
    html.replace("{{TOKEN_VAL}}", m_accessToken.toUtf8());

    QByteArray resp = httpOkHeader("text/html; charset=utf-8");
    resp += "Cache-Control: no-cache, no-store, must-revalidate\r\n";
    resp += "Pragma: no-cache\r\n";
    /* CSP: blocca script/connessioni verso domini esterni (anti-esfiltrazione e
       supply-chain). 'unsafe-inline' resta necessario perché il JS della web chat è
       inline; rimuoverlo richiederebbe estrarre tutto lo script in un file servito. */
    resp += "Content-Security-Policy: default-src 'self'; "
            "script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; "
            "img-src 'self' data: https://tile.openstreetmap.org https://*.tile.openstreetmap.org; "
            "connect-src 'self'; frame-ancestors 'none'\r\n";
    resp += "Connection: close\r\n";
    resp += "\r\n";
    s.socket->write(resp);
    s.socket->write(html);
    while (s.socket->bytesToWrite() > 0
           && s.socket->state() == QAbstractSocket::ConnectedState)
        s.socket->waitForBytesWritten(5000);
    s.socket->disconnectFromHost();
    s.socket->waitForDisconnected(5000);
}


/* ── / — pagina HTML benvenuto + download APK ───────────────────────────── */

void LanServer::handleIndex(Session& s)
{
    const QString apkPath = P::root() + "/ANDROID/PrismaluxMobile.apk";
    const bool apkExists  = QFile::exists(apkPath);
    const QString apkSize = [&]() -> QString {
        if (!apkExists) return "";
        const qint64 bytes = QFileInfo(apkPath).size();
        return QString(" (%1 MB)").arg(QString::number(bytes / (1024.0 * 1024.0), 'f', 1));
    }();

    const QByteArray apkSection = apkExists
        ? (QByteArray(R"(<a class="btn-download" href="/apk" download="PrismaluxMobile.apk">
        &#x2B07; Scarica APK Android
      </a>
      <p class="apk-info">PrismaluxMobile.apk)")
           + apkSize.toUtf8()
           + QByteArray("</p>"))
        : QByteArray(R"(<div class="unavailable">
        &#x26A0; APK non disponibile sul server.<br>
        Compila l'app Android prima di scaricarla.
      </div>)");

    QFile f(":/lan/index.html");
    if (!f.open(QIODevice::ReadOnly)) {
        sendError(s.socket, 500, "index.html not found in resources");
        return;
    }
    QByteArray html = f.readAll();
    html.replace("{{APK_SECTION}}", apkSection);

    QByteArray resp = httpOkHeader("text/html; charset=utf-8");
    resp += "Connection: close\r\n";
    resp += "\r\n";
    s.socket->write(resp);
    s.socket->write(html);
    while (s.socket->bytesToWrite() > 0
           && s.socket->state() == QAbstractSocket::ConnectedState)
        s.socket->waitForBytesWritten(5000);
    s.socket->disconnectFromHost();
    s.socket->waitForDisconnected(5000);
}

/* ── /apk — serve il file APK Android ───────────────────────────────────── */

void LanServer::handleApk(Session& s)
{
    const QString apkPath = P::root() + "/ANDROID/PrismaluxMobile.apk";
    QFile f(apkPath);
    if (!f.exists()) {
        sendError(s.socket, 404, "APK not found at " + apkPath);
        return;
    }
    if (!f.open(QIODevice::ReadOnly)) {
        sendError(s.socket, 500, "Cannot open APK file");
        return;
    }
    const QByteArray data = f.readAll();
    f.close();

    QByteArray resp = "HTTP/1.1 200 OK\r\n";
    resp += "Content-Type: application/vnd.android.package-archive\r\n";
    resp += "Content-Disposition: attachment; filename=\"PrismaluxMobile.apk\"\r\n";
    resp += "Content-Length: " + QByteArray::number(data.size()) + "\r\n\r\n";
    resp += data;
    s.socket->write(resp);
    s.socket->flush();
}

