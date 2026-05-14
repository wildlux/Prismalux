#include "lan_server.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QFile>
#include <QRegularExpression>
#include <QFileInfo>
#include <QTextStream>
#include "prismalux_paths.h"
namespace P = PrismaluxPaths;

/* SO_REUSEADDR per evitare "Address already in use" al riavvio rapido */
#ifndef Q_OS_WIN
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <unistd.h>
#else
#  include <winsock2.h>
#endif

/* ── costruttore / distruttore ───────────────────────────────────────────── */

LanServer::LanServer(AiClient* ai, QObject* parent)
    : QObject(parent), m_ai(ai)
{
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &LanServer::onNewConnection);
}

LanServer::~LanServer()
{
    blockSignals(true);   /* evita statusChanged(false) su widget già in distruzione */
    stop();
}

/* ── start / stop ────────────────────────────────────────────────────────── */

bool LanServer::start(quint16 port)
{
    if (m_server->isListening()) return true;

    /* Crea il socket con SO_REUSEADDR prima del bind: evita "Address already
     * in use" quando il software viene riavviato rapidamente (TIME_WAIT ~60s). */
#ifndef Q_OS_WIN
    {
        int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd >= 0) {
            int yes = 1;
            ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
            struct sockaddr_in sa{};
            sa.sin_family      = AF_INET;
            sa.sin_addr.s_addr = htonl(INADDR_ANY);
            sa.sin_port        = htons(port);
            if (::bind(fd, reinterpret_cast<struct sockaddr*>(&sa), sizeof(sa)) == 0
                && ::listen(fd, 128) == 0
                && m_server->setSocketDescriptor(fd)) {
                emit statusChanged(true);
                return true;
            }
            ::close(fd);
        }
    }
    /* Fallback se il percorso manuale fallisce */
#else
    /* Windows: SO_EXCLUSIVEADDRUSE è il default di Qt, SO_REUSEADDR è pericoloso
     * su Windows; lasciamo fare a Qt che usa già le opzioni corrette. */
#endif

    if (!m_server->listen(QHostAddress::AnyIPv4, port)) return false;
    emit statusChanged(true);
    return true;
}

void LanServer::stop()
{
    if (!m_server->isListening()) return;

    /* Raccoglie i socket PRIMA di modificare la mappa.
     * Se chiamassimo disconnectFromHost() dentro il loop, il segnale
     * disconnected() verrebbe emesso in modo sincrono → onClientDisconnected()
     * chiamerebbe m_sessions.erase() invalidando l'iteratore → SIGSEGV. */
    QList<QTcpSocket*> socks;
    socks.reserve(m_sessions.size());
    for (auto& s : m_sessions)
        if (s.socket) socks << s.socket;

    /* Svuota lo stato prima di chiudere i socket */
    m_sessions.clear();
    m_appClientIps.clear();
    m_streamSock = nullptr;
    m_tagsSock   = nullptr;

    m_server->close();
    emit statusChanged(false);

    /* Chiude i socket dopo aver svuotato m_sessions: onClientDisconnected()
     * non troverà nulla da cancellare e uscirà senza crash. */
    for (auto* sock : socks) {
        sock->disconnect(this);   /* scollega tutti i segnali verso LanServer */
        sock->abort();
        sock->deleteLater();
    }
}

bool    LanServer::isRunning()   const { return m_server->isListening(); }
quint16 LanServer::port()        const { return m_server->serverPort(); }
int     LanServer::clientCount() const { return m_appClientIps.size(); }

QStringList LanServer::connectedIPs() const
{
    QStringList list;
    for (auto it = m_sessions.cbegin(); it != m_sessions.cend(); ++it)
        list << it->addr;
    return list;
}

/* ── nuova connessione ───────────────────────────────────────────────────── */

void LanServer::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket* sock = m_server->nextPendingConnection();
        if (!sock) continue;

        Session s;
        s.socket = sock;
        s.addr   = sock->peerAddress().toString();
        m_sessions.insert(sock, s);

        connect(sock, &QTcpSocket::readyRead,    this, &LanServer::onClientReadyRead);
        connect(sock, &QTcpSocket::disconnected, this, &LanServer::onClientDisconnected);
        /* clientConnected viene emesso in processSession solo per percorsi API */
    }
}

/* ── lettura dati dal client ─────────────────────────────────────────────── */

void LanServer::onClientReadyRead()
{
    auto* sock = qobject_cast<QTcpSocket*>(sender());
    if (!sock) return;

    auto it = m_sessions.find(sock);
    if (it == m_sessions.end()) return;
    Session& s = it.value();

    s.buf += sock->readAll();

    /* DoS guard: scarta connessioni con richieste abnormi */
    if (s.buf.size() > 4 * 1024 * 1024) {
        sendError(sock, 400, "Request too large");
        sock->disconnectFromHost();
        return;
    }

    /* Parsing incrementale: prima header, poi body */
    if (!s.headersDone) {
        const int sep = s.buf.indexOf("\r\n\r\n");
        if (sep == -1) return;  // header non ancora completi

        const QByteArray headerPart = s.buf.left(sep);
        s.buf.remove(0, sep + 4);

        const QList<QByteArray> lines = headerPart.split('\n');
        if (lines.isEmpty()) { sendError(sock, 400, "Bad Request"); return; }

        /* Request line */
        const QList<QByteArray> reqLine = lines[0].trimmed().split(' ');
        if (reqLine.size() < 2) { sendError(sock, 400, "Bad Request"); return; }
        s.method = QString::fromLatin1(reqLine[0]).toUpper();
        s.path   = QString::fromLatin1(reqLine[1]);

        /* Headers */
        for (int i = 1; i < lines.size(); ++i) {
            const QByteArray line = lines[i].trimmed();
            const int colon = line.indexOf(':');
            if (colon < 0) continue;
            const QString key = QString::fromLatin1(line.left(colon)).trimmed().toLower();
            const QString val = QString::fromLatin1(line.mid(colon + 1)).trimmed();
            if (key == "content-length")
                s.contentLength = val.toInt();
        }
        s.headersDone = true;
    }

    /* Accumula body fino a contentLength */
    if ((int)s.buf.size() < s.contentLength) return;

    s.body = s.buf.left(s.contentLength);
    s.buf.remove(0, s.contentLength);

    processSession(s);

    /* Reset per eventuale richiesta successiva sulla stessa connessione */
    s.method.clear(); s.path.clear();
    s.contentLength = 0;
    s.headersDone   = false;
    s.body.clear();
}

/* ── disconnessione client ───────────────────────────────────────────────── */

void LanServer::onClientDisconnected()
{
    auto* sock = qobject_cast<QTcpSocket*>(sender());
    if (!sock) return;

    auto it = m_sessions.find(sock);
    const QString addr = (it != m_sessions.end()) ? it->addr : QString();

    if (sock == m_streamSock) {
        m_streamSock = nullptr;
        disconnect(m_ai, nullptr, this, nullptr);
    }
    if (sock == m_tagsSock) {
        m_tagsSock = nullptr;
        disconnect(m_modelsConn);
    }

    const bool wasApi = (it != m_sessions.end()) && it->isApiClient;
    if (it != m_sessions.end()) {
        if (wasApi) m_appClientIps.remove(addr);
        m_sessions.erase(it);
    }
    sock->deleteLater();

    if (wasApi && !addr.isEmpty()) emit clientDisconnected(addr);
}

/* ── dispatch richiesta ──────────────────────────────────────────────────── */

void LanServer::processSession(Session& s)
{
    emit requestHandled(s.method, s.path, s.addr);

    /* Percorsi API Ollama — contano come "client APK" */
    const bool isApi = (s.path == "/api/tags"     ||
                        s.path == "/api/chat"      ||
                        s.path == "/api/generate"  ||
                        s.path == "/knowledge");

    if (isApi && !s.isApiClient) {
        s.isApiClient = true;
        if (!m_appClientIps.contains(s.addr)) {
            m_appClientIps.insert(s.addr);
            emit clientConnected(s.addr);
        }
    }

    if (s.path == "/api/tags" && s.method == "GET") {
        handleTags(s);
    } else if (s.path == "/api/chat") {
        handleChat(s);
    } else if (s.path == "/api/generate") {
        handleGenerate(s);
    } else if (s.path == "/knowledge") {
        handleKnowledge(s);
    } else if (s.path == "/apk" && s.method == "GET") {
        handleApk(s);
    } else if ((s.path == "/" || s.path == "/index" || s.path == "/download")
               && s.method == "GET") {
        handleIndex(s);
    } else if (s.path == "/web" && s.method == "GET") {
        handleWebChat(s);
    } else {
        sendJson(s.socket, R"({"status":"ok"})");
    }
}

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
    if (m_ai->busy()) { sendError(s.socket, 503, "AI busy"); return; }
    if (m_streamSock) { sendError(s.socket, 503, "Stream in progress"); return; }

    const QJsonDocument chatDoc = QJsonDocument::fromJson(s.body);
    if (chatDoc.isNull() || !chatDoc.isObject()) {
        sendError(s.socket, 400, "Invalid JSON"); return;
    }
    const QJsonObject req = chatDoc.object();
    const QJsonArray  msgs = req["messages"].toArray();

    /* Se il client specifica un modello diverso, lo impostiamo prima di chattare */
    const QString reqModel = req["model"].toString();
    if (!reqModel.isEmpty() && reqModel != m_ai->model())
        m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), reqModel);

    /* Adotta num_ctx e num_predict dalla richiesta mobile (salva e ripristina) */
    const QJsonObject opts = req["options"].toObject();
    if (!opts.isEmpty()) {
        AiChatParams p = m_ai->chatParams();
        if (opts.contains("num_ctx"))     p.num_ctx     = opts["num_ctx"].toInt(p.num_ctx);
        if (opts.contains("num_predict")) p.num_predict = opts["num_predict"].toInt(p.num_predict);
        m_ai->setChatParams(p);
    }

    /* Estrai system prompt, history intermedia, ultimo messaggio utente.
       Il client Android invia [system?, user, assistant, ..., user_corrente]. */
    QString    systemPrompt;
    QJsonArray history;
    QString    userMsg;

    for (int i = 0; i < msgs.size(); ++i) {
        const QJsonObject m  = msgs[i].toObject();
        const QString     role = m["role"].toString();
        const QString     cont = m["content"].toString();

        if (role == "system") {
            systemPrompt = cont;
        } else if (role == "user" && i == msgs.size() - 1) {
            /* Ultimo elemento = messaggio corrente */
            userMsg = cont;
        } else {
            /* Turni precedenti (user+assistant) = history */
            history.append(m);
        }
    }

    m_streamSock = s.socket;
    m_genMode    = false;

    disconnect(m_ai, nullptr, this, nullptr);
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
    if (m_ai->busy()) { sendError(s.socket, 503, "AI busy"); return; }
    if (m_streamSock) { sendError(s.socket, 503, "Stream in progress"); return; }

    const QJsonDocument genDoc = QJsonDocument::fromJson(s.body);
    if (genDoc.isNull() || !genDoc.isObject()) {
        sendError(s.socket, 400, "Invalid JSON"); return;
    }
    const QJsonObject req  = genDoc.object();
    const QString model    = req["model"].toString(m_ai->model());
    const QString prompt   = req["prompt"].toString();
    const QString system   = req["system"].toString();

    if (!model.isEmpty() && model != m_ai->model())
        m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), model);

    m_streamSock = s.socket;
    m_genMode    = true;

    disconnect(m_ai, nullptr, this, nullptr);
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
            connect(m_knowledgeRateTimer, &QTimer::timeout, this,
                    [this]{ m_knowledgeReqCount.clear(); });
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

/* ── /web — interfaccia chat web per PC nella rete locale ───────────────── */
void LanServer::handleWebChat(Session& s)
{
    const QByteArray model = m_ai->model().isEmpty()
                             ? QByteArray("ollama")
                             : m_ai->model().toUtf8();

    /* L'HTML è spezzato in blocchi per evitare problemi con raw-string e char
       literal nel preprocessore C++ (apici singoli JS, template literal, ecc.) */
    QByteArray html;
    html += "<!DOCTYPE html>\n<html lang=\"it\">\n<head>\n"
            "<meta charset=\"utf-8\">\n"
            "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
            "<title>Prismalux \xe2\x80\x94 Chat AI</title>\n"
            "<style>\n"
            "*{box-sizing:border-box;margin:0;padding:0}\n"
            "body{font-family:-apple-system,BlinkMacSystemFont,sans-serif;"
              "background:#0f1117;color:#e0e0f0;height:100vh;"
              "display:flex;flex-direction:column}\n"
            "#hdr{background:#1a1d2e;border-bottom:1px solid #2a2d4e;"
              "padding:10px 16px;display:flex;align-items:center;gap:10px;flex-shrink:0}\n"
            "#hdr h1{font-size:16px;font-weight:700;color:#fff}\n"
            "#hdr .mdl{font-size:11px;color:#6c63ff;border:1px solid #6c63ff40;"
              "border-radius:20px;padding:2px 10px}\n"
            "#log{flex:1;overflow-y:auto;padding:16px;"
              "display:flex;flex-direction:column;gap:10px}\n"
            ".msg{max-width:82%;padding:10px 14px;border-radius:14px;"
              "line-height:1.55;font-size:14px;white-space:pre-wrap}\n"
            ".user{align-self:flex-end;background:#6c63ff;color:#fff;"
              "border-bottom-right-radius:4px}\n"
            ".ai{align-self:flex-start;background:#1e2235;color:#e0e0f0;"
              "border-bottom-left-radius:4px;border:1px solid #2a2d4e}\n"
            "#bar{display:flex;gap:8px;padding:12px 16px;background:#1a1d2e;"
              "border-top:1px solid #2a2d4e;flex-shrink:0}\n"
            "#sys{flex:0 0 auto;width:200px;background:#0f1117;"
              "border:1px solid #2a2d4e;border-radius:8px;"
              "color:#888;padding:6px 10px;font-size:12px}\n"
            "#txt{flex:1;background:#0f1117;border:1px solid #2a2d4e;"
              "border-radius:10px;color:#e0e0f0;padding:10px 14px;"
              "font-size:14px;resize:none;max-height:120px}\n"
            "#txt:focus,#sys:focus{outline:none;border-color:#6c63ff}\n"
            "#snd{background:#6c63ff;color:#fff;border:none;border-radius:10px;"
              "padding:10px 20px;font-size:14px;font-weight:700;cursor:pointer}\n"
            "#snd:hover{background:#7c74ff}\n"
            "#snd:disabled{background:#333;color:#666;cursor:default}\n"
            "</style>\n</head>\n<body>\n"
            "<div id=\"hdr\"><span style=\"font-size:24px\">&#127866;</span>"
            "<h1>Prismalux</h1>"
            "<span class=\"mdl\">" + model + "</span></div>\n"
            "<div id=\"log\"></div>\n"
            "<div id=\"bar\">"
            "<input id=\"sys\" placeholder=\"System prompt (opz.)\">"
            "<textarea id=\"txt\" rows=\"1\""
              " placeholder=\"Scrivi un messaggio... (Invio=invia)\"></textarea>"
            "<button id=\"snd\">Invia</button>"
            "</div>\n"
            "<script>\n"
            "const L=document.getElementById('log'),"
              "T=document.getElementById('txt'),"
              "B=document.getElementById('snd'),"
              "S=document.getElementById('sys'),"
              "H=[];\n"
            "function add(r,t){"
              "const d=document.createElement('div');"
              "d.className='msg '+r;d.textContent=t;"
              "L.appendChild(d);L.scrollTop=L.scrollHeight;return d;}\n"
            "async function go(){\n"
              "const m=T.value.trim();if(!m)return;\n"
              "T.value='';T.style.height='';B.disabled=true;\n"
              "add('user',m);H.push({role:'user',content:m});\n"
              "const msgs=[];\n"
              "const sv=S.value.trim();if(sv)msgs.push({role:'system',content:sv});\n"
              "msgs.push(...H);\n"
              "const aiD=add('ai','');let full='';\n"
              "try{\n"
                "const r=await fetch('/api/chat',{method:'POST',"
                  "headers:{'Content-Type':'application/json'},"
                  "body:JSON.stringify({model:'" + model + "',messages:msgs,stream:true})});\n"
                "const rd=r.body.getReader(),dc=new TextDecoder();\n"
                "while(true){\n"
                  "const {done,value}=await rd.read();if(done)break;\n"
                  "for(const ln of dc.decode(value).split('\\n')){\n"
                    "if(!ln.trim())continue;\n"
                    "try{const o=JSON.parse(ln);\n"
                      "const tk=(o.message&&o.message.content)||o.response||'';\n"
                      "if(tk){full+=tk;aiD.textContent=full;"
                        "L.scrollTop=L.scrollHeight;}\n"
                    "}catch(x){}\n"
                  "}\n"
                "}\n"
              "}catch(e){aiD.textContent='Errore: '+e.message;}\n"
              "if(full)H.push({role:'assistant',content:full});\n"
              "B.disabled=false;T.focus();\n"
            "}\n"
            "B.addEventListener('click',go);\n"
            "T.addEventListener('keydown',function(e){\n"
              "if(e.key==='Enter'&&!e.shiftKey){e.preventDefault();go();}\n"
              "setTimeout(function(){"
                "T.style.height='';"
                "T.style.height=Math.min(T.scrollHeight,120)+'px';},0);\n"
            "});\n"
            "T.focus();\n"
            "</script>\n</body>\n</html>\n";

    QByteArray resp = httpOkHeader("text/html; charset=utf-8");
    resp += "Content-Length: " + QByteArray::number(html.size()) + "\r\n\r\n";
    resp += html;
    s.socket->write(resp);
    s.socket->flush();
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

    const QByteArray html = QStringLiteral(R"(<!DOCTYPE html>
<html lang="it">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Prismalux — Scarica App</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
      background: linear-gradient(135deg, #0f1117 0%, #1a1d2e 100%);
      min-height: 100vh;
      display: flex;
      align-items: center;
      justify-content: center;
      color: #e0e0f0;
    }
    .card {
      background: rgba(255,255,255,0.06);
      border: 1px solid rgba(255,255,255,0.12);
      border-radius: 20px;
      padding: 44px 36px;
      max-width: 420px;
      width: 92%;
      text-align: center;
      box-shadow: 0 8px 40px rgba(0,0,0,0.5);
    }
    .logo { font-size: 56px; margin-bottom: 12px; }
    h1 { font-size: 26px; font-weight: 700; margin-bottom: 6px; color: #fff; }
    .subtitle {
      font-size: 14px;
      color: #8a8fb0;
      margin-bottom: 32px;
      line-height: 1.5;
    }
    .btn-download {
      display: inline-block;
      background: linear-gradient(135deg, #6c63ff, #48b4e0);
      color: #fff;
      text-decoration: none;
      font-size: 18px;
      font-weight: 700;
      padding: 16px 36px;
      border-radius: 50px;
      letter-spacing: 0.3px;
      box-shadow: 0 4px 20px rgba(108,99,255,0.4);
      transition: transform .15s, box-shadow .15s;
    }
    .btn-download:active { transform: scale(0.97); }
    .apk-info {
      margin-top: 14px;
      font-size: 12px;
      color: #6a6f90;
    }
    .unavailable {
      background: rgba(244,67,54,0.15);
      border: 1px solid rgba(244,67,54,0.3);
      border-radius: 10px;
      padding: 14px;
      color: #ef9a9a;
      font-size: 14px;
      margin-top: 8px;
    }
    .footer {
      margin-top: 28px;
      font-size: 11px;
      color: #4a4f68;
      line-height: 1.6;
    }
  </style>
</head>
<body>
  <div class="card">
    <div class="logo">🍺</div>
    <h1>Prismalux</h1>
    <p class="subtitle">
      Benvenuto!<br>
      Installa l'app Android per usare l'AI locale<br>
      direttamente dal tuo telefono via Wi-Fi.
    </p>
    %1
    <div class="footer">
      Assicurati di avere il Wi-Fi abilitato<br>
      e di consentire l'installazione<br>
      da sorgenti sconosciute nelle Impostazioni Android.
    </div>
  </div>
</body>
</html>
)").arg(
        apkExists
        ? QString(R"(<a class="btn-download" href="/apk" download="PrismaluxMobile.apk">
        ⬇ Scarica APK Android
      </a>
      <p class="apk-info">PrismaluxMobile.apk%1</p>)").arg(apkSize)
        : R"(<div class="unavailable">
        ⚠ APK non disponibile sul server.<br>
        Compila l'app Android prima di scaricarla.
      </div>)"
    ).toUtf8();

    QByteArray resp = httpOkHeader("text/html; charset=utf-8");
    resp += "Content-Length: " + QByteArray::number(html.size()) + "\r\n\r\n";
    resp += html;
    s.socket->write(resp);
    s.socket->flush();
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

/* ── helpers ─────────────────────────────────────────────────────────────── */

void LanServer::sendJson(QTcpSocket* sock, const QByteArray& json)
{
    if (!sock) return;
    QByteArray resp = httpOkHeader("application/json");
    resp += "Content-Length: " + QByteArray::number(json.size()) + "\r\n\r\n";
    resp += json;
    sock->write(resp);
    sock->flush();
}

void LanServer::sendStreamLine(const QByteArray& json)
{
    if (!m_streamSock) return;
    m_streamSock->write(json + "\n");
    m_streamSock->flush();
}

void LanServer::sendError(QTcpSocket* sock, int code, const QString& msg)
{
    if (!sock) return;
    /* Sanitizza: rimuove caratteri che permetterebbero header injection o
       JSON injection (backslash, virgolette, CR/LF). */
    QString safe = msg;
    safe.remove(QRegularExpression(R"([\r\n"\\])"));
    safe = safe.left(200);
    const QByteArray body = ("{\"error\":\"" + safe + "\"}").toUtf8();
    const QString status  = (code == 503) ? "503 Service Unavailable"
                          : (code == 400) ? "400 Bad Request"
                          :                 "500 Internal Server Error";
    QByteArray resp = "HTTP/1.1 " + status.toLatin1() + "\r\n";
    resp += "Content-Type: application/json\r\n";
    resp += "Content-Length: " + QByteArray::number(body.size()) + "\r\n\r\n";
    resp += body;
    sock->write(resp);
    sock->flush();
}

void LanServer::closeStreamSession()
{
    disconnect(m_ai, nullptr, this, nullptr);
    if (m_streamSock) {
        m_streamSock->flush();
        m_streamSock->disconnectFromHost();   // segnala EOF al client Android
    }
    m_streamSock = nullptr;
    m_genMode    = false;
}

QByteArray LanServer::httpOkHeader(const char* contentType)
{
    QByteArray h = "HTTP/1.1 200 OK\r\n";
    h += "Content-Type: ";
    h += contentType;
    h += "\r\nAccess-Control-Allow-Origin: *\r\n";
    return h;
}

QByteArray LanServer::httpStreamHeader()
{
    /* Nessun Content-Length né Transfer-Encoding chunked:
       il client legge via readyRead fino alla chiusura del socket. */
    QByteArray h = "HTTP/1.1 200 OK\r\n";
    h += "Content-Type: application/x-ndjson\r\n";
    h += "Access-Control-Allow-Origin: *\r\n";
    h += "X-Accel-Buffering: no\r\n";
    h += "Connection: close\r\n\r\n";
    return h;
}
