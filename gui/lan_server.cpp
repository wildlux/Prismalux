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
#include <QProcess>
#include "prismalux_paths.h"
namespace P = PrismaluxPaths;

/* Confronto constant-time — evita timing attack sul token Bearer */
bool LanServer::timingSafeEqual(const QString& a, const QString& b)
{
    if (a.size() != b.size()) return false;
    volatile int diff = 0;
    for (int i = 0; i < a.size(); ++i)
        diff |= (a[i].unicode() ^ b[i].unicode());
    return diff == 0;
}

/* Appende una riga al log di accesso ~/.prismalux/access.log */
void LanServer::appendAccessLog(const QString& addr, const QString& method, const QString& path)
{
    const QString logPath = QDir::homePath() + "/.prismalux/access.log";
    QFile f(logPath);
    if (!f.open(QIODevice::Append | QIODevice::Text)) return;
    QTextStream ts(&f);
    ts << QDateTime::currentDateTime().toString(Qt::ISODate)
       << " " << addr << " " << method << " " << path << "\n";
}

/* Genera certificato self-signed in ~/.prismalux/ se non già presente.
 * Usa `openssl req -x509`; se openssl non è disponibile ritorna false
 * e il chiamante cade in fallback HTTP. */
bool LanServer::_ensureCert(QString& certPath, QString& keyPath)
{
#if !QT_CONFIG(ssl)
    return false;
#else
    const QString dir = QDir::homePath() + "/.prismalux";
    QDir().mkpath(dir);
    certPath = dir + "/server.crt";
    keyPath  = dir + "/server.key";

    if (QFileInfo::exists(certPath) && QFileInfo::exists(keyPath))
        return true;

    QProcess proc;
    proc.start("openssl", {
        "req", "-x509", "-newkey", "rsa:2048", "-nodes",
        "-days", "3650",
        "-keyout", keyPath,
        "-out",    certPath,
        "-subj",   "/CN=Prismalux-LAN"
    });
    if (!proc.waitForFinished(10000) || proc.exitCode() != 0) {
        qWarning() << "LanServer: openssl non disponibile — TLS disabilitato";
        return false;
    }
    return QFileInfo::exists(certPath) && QFileInfo::exists(keyPath);
#endif
}

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
    /* HTTP semplice sulla LAN: il TLS auto-firmato causava handshake lenti,
     * avvisi "sito non sicuro" nel browser e problemi con i QR code.
     * La sicurezza è garantita dal Bearer token. */
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

        if (static_cast<int>(m_sessions.size()) >= kMaxSessions) {
            sock->write("HTTP/1.1 503 Service Unavailable\r\n"
                        "Content-Type: application/json\r\n"
                        "X-Content-Type-Options: nosniff\r\n"
                        "Content-Length: 21\r\n\r\n"
                        "{\"error\":\"overload\"}");
            sock->flush();
            sock->deleteLater();
            continue;
        }

        /* Entra subito in m_sessions */
        Session s;
        s.socket = sock;
        s.addr   = sock->peerAddress().toString();
        m_sessions.insert(sock, s);

        connect(sock, &QTcpSocket::readyRead,    this, &LanServer::onClientReadyRead);
        connect(sock, &QTcpSocket::disconnected, this, &LanServer::onClientDisconnected);
        /* clientConnected viene emesso in processSession solo per percorsi API */
    }
}

/* ── rate limiter condiviso /api/chat + /api/generate ───────────────────── */

void LanServer::onChatRateTimeout()      { m_chatRateCount.clear(); }
void LanServer::onKnowledgeRateTimeout() { m_knowledgeReqCount.clear(); }

bool LanServer::checkChatRateLimit(Session& s)
{
    if (!m_chatRateTimer) {
        m_chatRateTimer = new QTimer(this);
        m_chatRateTimer->setInterval(60 * 1000);
        connect(m_chatRateTimer, &QTimer::timeout,
                this, &LanServer::onChatRateTimeout);
        m_chatRateTimer->start();
    }
    if (++m_chatRateCount[s.addr] > 30) {
        sendError(s.socket, 429, "Rate limit exceeded");
        return true;
    }
    return false;
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

        /* Separa il query string dal path e usa ?token= come auth fallback.
         * Priorità: header Authorization (già parsato) > query ?token=.
         * Utile per aprire /web con link diretto da browser o condivisione URL. */
        const int qmark = s.path.indexOf('?');
        if (qmark >= 0) {
            const QString qs = s.path.mid(qmark + 1);
            s.path = s.path.left(qmark);
            /* Cerca token=XXX nella query string */
            for (const QStringView part : QStringView(qs).split('&')) {
                if (part.startsWith(u"token=")) {
                    const QString raw = part.mid(6).toString();
                    /* URL-decode minimale: solo %XX sequenze basilari */
                    s.authHeaderFallback = QUrl::fromPercentEncoding(raw.toLatin1());
                    break;
                }
            }
        }

        /* Headers */
        for (int i = 1; i < lines.size(); ++i) {
            const QByteArray line = lines[i].trimmed();
            const int colon = line.indexOf(':');
            if (colon < 0) continue;
            const QString key = QString::fromLatin1(line.left(colon)).trimmed().toLower();
            const QString val = QString::fromLatin1(line.mid(colon + 1)).trimmed();
            if (key == "content-length")
                s.contentLength = val.toInt();
            else if (key == "authorization")
                s.authHeader = val;
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
    appendAccessLog(s.addr, s.method, s.path);

    /* Percorsi API Ollama — contano come "client APK" */
    const bool isApi = (s.path == "/api/tags"     ||
                        s.path == "/api/chat"      ||
                        s.path == "/api/generate"  ||
                        s.path == "/knowledge"     ||
                        s.path == "/apk"           ||
                        s.path == "/api/launch"    ||
                        s.path == "/api/sysinfo"   ||
                        s.path == "/api/mcp");

    /* Auth check: se il token è impostato, le route protette richiedono
       Authorization: Bearer TOKEN, oppure ?token=TOKEN nella URL (fallback).
       Confronto constant-time per evitare timing attack. */
    if (isApi && !m_accessToken.isEmpty()) {
        const QString expected = "Bearer " + m_accessToken;
        /* Usa l'header se presente, altrimenti il token dalla query string */
        const QString effective = s.authHeader.isEmpty()
            ? "Bearer " + s.authHeaderFallback
            : s.authHeader;
        if (!timingSafeEqual(effective, expected)) {
            QByteArray resp = "HTTP/1.1 401 Unauthorized\r\n"
                              "Content-Type: application/json\r\n"
                              "WWW-Authenticate: Bearer realm=\"Prismalux\"\r\n"
                              "Connection: close\r\n"
                              "Content-Length: 26\r\n\r\n"
                              "{\"error\":\"Unauthorized\"}";
            s.socket->write(resp);
            s.socket->flush();
            s.socket->disconnectFromHost();
            return;
        }
    }

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
    } else if (s.path == "/api/launch" && s.method == "POST") {
        const QJsonObject body = QJsonDocument::fromJson(s.body).object();
        const QString app = body["app"].toString().toLower().trimmed();
        static const QMap<QString, QString> kAppMap = {
            {"blender",      "blender"},
            {"freecad",      "freecad"},
            {"libreoffice",  "libreoffice"},
            {"cloudcompare", "cloudcompare"},
            {"anki",         "anki"},
            {"kicad",        "kicad"},
            {"obs",          "obs"},
            {"opencode",     "opencode"},
            {"godot",        "godot4"},
            {"tinymcp",      "tinymcp"},
            {"vscode",       "code"},
            {"gimp",         "gimp"},
            {"inkscape",     "inkscape"},
            {"vlc",          "vlc"},
            {"firefox",      "firefox"},
            {"chromium",     "chromium"},
            {"nautilus",     "nautilus"},
            {"thunar",       "thunar"},
            {"konsole",      "konsole"},
            {"xterm",        "xterm"},
        };
        if (!kAppMap.contains(app)) {
            sendError(s.socket, 400, "App non trovata");
        } else {
            const bool ok = QProcess::startDetached(kAppMap[app], {});
            sendJson(s.socket, ok
                ? R"({"status":"launched"})"
                : R"({"status":"error","msg":"eseguibile non trovato"})");
        }
    } else if (s.path == "/api/sysinfo" && s.method == "GET") {
        int ncpu = 1;
        {
            QFile f("/proc/cpuinfo");
            if (f.open(QIODevice::ReadOnly)) {
                while (!f.atEnd())
                    if (QString::fromLatin1(f.readLine()).startsWith("processor")) ncpu++;
                f.close();
            }
        }
        double load1 = 0.0;
        {
            QFile f("/proc/loadavg");
            if (f.open(QIODevice::ReadOnly)) {
                load1 = QString::fromLatin1(f.readLine()).split(' ').value(0).toDouble();
                f.close();
            }
        }
        const int cpuPct = qMin(100, int(load1 * 100.0 / ncpu));
        qint64 ramTotal = 0, ramFree = 0, ramBuf = 0, ramCached = 0;
        {
            QFile f("/proc/meminfo");
            if (f.open(QIODevice::ReadOnly)) {
                while (!f.atEnd()) {
                    const QString ln = QString::fromLatin1(f.readLine()).trimmed();
                    const auto p = ln.split(QRegularExpression("\\s+"));
                    if      (ln.startsWith("MemTotal:"))                           ramTotal  = p.value(1).toLongLong();
                    else if (ln.startsWith("MemFree:"))                            ramFree   = p.value(1).toLongLong();
                    else if (ln.startsWith("Buffers:"))                            ramBuf    = p.value(1).toLongLong();
                    else if (ln.startsWith("Cached:") && !ln.startsWith("SwapCached:")) ramCached = p.value(1).toLongLong();
                }
                f.close();
            }
        }
        const qint64 ramUsed = ramTotal - ramFree - ramBuf - ramCached;
        const int ramPct = ramTotal > 0 ? int(ramUsed * 100 / ramTotal) : 0;
        QString uptime = "--";
        {
            QFile f("/proc/uptime");
            if (f.open(QIODevice::ReadOnly)) {
                const double secs = QString::fromLatin1(f.readLine()).split(' ').value(0).toDouble();
                uptime = QString("%1h %2m").arg(int(secs / 3600)).arg(int(secs / 60) % 60, 2, 10, QChar('0'));
                f.close();
            }
        }
        QString hostname = "unknown";
        {
            QFile f("/proc/sys/kernel/hostname");
            if (f.open(QIODevice::ReadOnly)) { hostname = QString::fromLatin1(f.readLine()).trimmed(); f.close(); }
        }
        QJsonObject sj;
        sj["cpu_pct"]      = cpuPct;
        sj["ram_used_gb"]  = QString::number(ramUsed  / (1024.0 * 1024.0), 'f', 1);
        sj["ram_total_gb"] = QString::number(ramTotal / (1024.0 * 1024.0), 'f', 1);
        sj["ram_pct"]      = ramPct;
        sj["uptime"]       = uptime;
        sj["hostname"]     = hostname;
        sendJson(s.socket, QJsonDocument(sj).toJson(QJsonDocument::Compact));
    } else if (s.path == "/api/mcp" && s.method == "POST") {
        const QJsonObject wrapper = QJsonDocument::fromJson(s.body).object();
        const int       port    = wrapper["port"].toInt(8765);
        const QString   host    = wrapper["host"].toString("127.0.0.1");
        const QJsonObject mcpReq = wrapper["request"].toObject();
        const QByteArray mcpBody = QJsonDocument(mcpReq).toJson(QJsonDocument::Compact);

        QTcpSocket mcp;
        mcp.connectToHost(host, static_cast<quint16>(port));
        if (!mcp.waitForConnected(3000)) {
            sendJson(s.socket, R"({"error":"MCP non raggiungibile","code":-1})");
            return;
        }
        const QByteArray httpReq = QByteArray("POST / HTTP/1.1\r\n")
            + "Host: " + host.toLatin1() + ":" + QByteArray::number(port) + "\r\n"
            + "Content-Type: application/json\r\n"
            + "Content-Length: " + QByteArray::number(mcpBody.size()) + "\r\n"
            + "Connection: close\r\n\r\n"
            + mcpBody;
        mcp.write(httpReq);
        mcp.flush();

        QByteArray response;
        while (mcp.state() == QAbstractSocket::ConnectedState
               || mcp.waitForReadyRead(5000)) {
            const QByteArray chunk = mcp.readAll();
            if (chunk.isEmpty() && mcp.state() != QAbstractSocket::ConnectedState) break;
            response += chunk;
        }
        response += mcp.readAll();

        const int sep = response.indexOf("\r\n\r\n");
        const QByteArray mcpResp = sep >= 0 ? response.mid(sep + 4) : response;
        sendJson(s.socket, mcpResp.isEmpty()
            ? QByteArray(R"({"error":"Nessuna risposta dal server MCP"})")
            : mcpResp);
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
    if (checkChatRateLimit(s)) return;

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

    QByteArray html;
    html += "<!DOCTYPE html>\n<html lang=\"it\">\n<head>\n"
            "<meta charset=\"utf-8\">\n"
            "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
            "<title>Prismalux \xe2\x80\x94 Chat AI</title>\n"
            "<style>\n"
            ":root{--bg:#0f1117;--hdr:#1a1d2e;--brd:#2a2d4e;"
              "--acc:#6c63ff;--usr:#6c63ff;--aim:#1e2235;"
              "--txt:#e0e0f0;--dim:#888;--inp:#0f1117}\n"
            "*{box-sizing:border-box;margin:0;padding:0}\n"
            "body{font-family:-apple-system,BlinkMacSystemFont,sans-serif;"
              "background:var(--bg);color:var(--txt);height:100vh;height:100dvh;"
              "display:flex;flex-direction:column;overflow:hidden}\n"
            /* ── Overlay + Drawer ── */
            "#ov{display:none;position:fixed;inset:0;background:rgba(0,0,0,.5);z-index:10}\n"
            "#ov.open{display:block}\n"
            "#drw{position:fixed;top:0;left:0;bottom:0;width:280px;max-width:85vw;"
              "background:var(--hdr);border-right:1px solid var(--brd);"
              "z-index:11;display:flex;flex-direction:column;"
              "transform:translateX(-100%);transition:transform .25s ease;overflow-y:auto}\n"
            "#drw.open{transform:translateX(0)}\n"
            "#drw-hdr{display:flex;align-items:center;justify-content:space-between;"
              "padding:14px 16px;border-bottom:1px solid var(--brd);flex-shrink:0}\n"
            "#drw-hdr h2{font-size:15px;font-weight:700;color:var(--txt)}\n"
            "#cls{background:transparent;border:none;color:var(--dim);"
              "font-size:18px;cursor:pointer;padding:2px 8px;border-radius:4px}\n"
            "#cls:hover{background:rgba(255,255,255,.1);color:var(--txt)}\n"
            ".dsec{padding:14px 16px;border-bottom:1px solid var(--brd)}\n"
            ".dsec h3{font-size:11px;color:var(--dim);text-transform:uppercase;"
              "letter-spacing:.8px;margin-bottom:10px}\n"
            "#mdl-sel{width:100%;background:var(--inp);border:1px solid var(--brd);"
              "color:var(--txt);border-radius:8px;padding:8px 10px;font-size:14px}\n"
            "#mdl-sel:focus{outline:none;border-color:var(--acc)}\n"
            ".pbtn{display:flex;align-items:center;gap:8px;width:100%;"
              "padding:9px 12px;margin-bottom:4px;background:transparent;"
              "border:1px solid transparent;border-radius:8px;"
              "color:var(--txt);font-size:14px;cursor:pointer;text-align:left}\n"
            ".pbtn:hover{background:rgba(255,255,255,.07)}\n"
            ".pbtn.on{background:rgba(128,128,200,.2);border-color:var(--acc);"
              "color:var(--acc);font-weight:600}\n"
            ".tbtn{display:flex;align-items:center;gap:8px;width:100%;"
              "padding:9px 12px;margin-bottom:4px;background:transparent;"
              "border:1px solid transparent;border-radius:8px;"
              "color:var(--txt);font-size:14px;cursor:pointer;text-align:left}\n"
            ".tbtn:hover{background:rgba(255,255,255,.07)}\n"
            ".tbtn.on{background:rgba(80,200,120,.15);border-color:#50c878;"
              "color:#50c878;font-weight:600}\n"
            "#mic-btn{background:transparent;border:1px solid var(--brd);"
              "color:var(--dim);border-radius:8px;padding:8px 10px;"
              "font-size:16px;cursor:pointer;flex-shrink:0;line-height:1}\n"
            "#mic-btn:hover{background:rgba(255,255,255,.07);color:var(--txt)}\n"
            "#mic-btn.on{color:#ff4444;background:rgba(255,68,68,.12);"
              "border-color:#ff4444}\n"
            /* ── Header ── */
            "#ham{background:transparent;border:none;color:var(--txt);"
              "font-size:22px;cursor:pointer;padding:4px 8px;border-radius:6px;flex-shrink:0}\n"
            "#ham:hover{background:rgba(255,255,255,.1)}\n"
            "#hdr{background:var(--hdr);border-bottom:1px solid var(--brd);"
              "padding:10px 16px;display:flex;align-items:center;gap:8px;flex-shrink:0}\n"
            "#hdr h1{font-size:16px;font-weight:700;color:var(--txt);flex-shrink:0}\n"
            ".mdl{font-size:11px;color:var(--acc);border:1px solid var(--brd);"
              "border-radius:20px;padding:2px 10px;flex-shrink:1;"
              "overflow:hidden;text-overflow:ellipsis;white-space:nowrap;max-width:110px}\n"
            /* ── #tsel ora nel drawer, stile uguale a #mdl-sel ── */
            "#tsel{width:100%;background:var(--inp);border:1px solid var(--brd);"
              "color:var(--txt);border-radius:8px;padding:8px 10px;font-size:14px}\n"
            "#tsel:focus{outline:none;border-color:var(--acc)}\n"
            "#log{flex:1;overflow-y:auto;padding:16px;"
              "display:flex;flex-direction:column;gap:10px}\n"
            ".msg{max-width:82%;padding:10px 14px;border-radius:14px;"
              "line-height:1.55;font-size:14px;white-space:pre-wrap}\n"
            ".user{align-self:flex-end;background:var(--usr);color:#fff;"
              "border-bottom-right-radius:4px}\n"
            ".ai{align-self:flex-start;background:var(--aim);color:var(--txt);"
              "border-bottom-left-radius:4px;border:1px solid var(--brd)}\n"
            "#bar{display:flex;gap:8px;padding:10px 16px;background:var(--hdr);"
              "border-top:1px solid var(--brd);flex-shrink:0;align-items:flex-end}\n"
            "#bar-mid{flex:1;display:flex;flex-direction:column;gap:4px}\n"
            "#att-tag{display:none;align-items:center;gap:6px;"
              "background:rgba(255,255,255,.08);border-radius:6px;padding:4px 8px}\n"
            "#att-name{flex:1;overflow:hidden;text-overflow:ellipsis;"
              "white-space:nowrap;font-size:12px;color:var(--dim)}\n"
            "#att-clr{background:transparent;border:none;color:var(--dim);"
              "cursor:pointer;padding:0 4px;font-size:14px;flex-shrink:0;line-height:1}\n"
            "#att-btn{background:transparent;border:1px solid var(--brd);"
              "color:var(--dim);border-radius:8px;padding:8px 10px;"
              "font-size:16px;cursor:pointer;flex-shrink:0;line-height:1}\n"
            "#att-btn:hover{background:rgba(255,255,255,.07);color:var(--txt)}\n"
            "#txt{width:100%;background:var(--inp);border:1px solid var(--brd);"
              "border-radius:10px;color:var(--txt);padding:10px 14px;"
              "font-size:14px;resize:none;max-height:200px}\n"
            "#txt:focus{outline:none;border-color:var(--acc)}\n"
            "#snd{background:var(--acc);color:#fff;border:none;border-radius:10px;"
              "padding:10px 20px;font-size:14px;font-weight:700;cursor:pointer;flex-shrink:0}\n"
            "#snd:hover{filter:brightness(1.1)}\n"
            "#snd:disabled{background:#333;color:#666;cursor:default}\n"
            "#bar-right{display:flex;flex-direction:column;align-items:stretch;gap:4px;flex-shrink:0}\n"
            "#bar-tools{display:flex;gap:4px;justify-content:flex-end}\n"
            ".ai-actions{display:flex;align-items:center;gap:2px;padding:2px 4px;"
              "max-width:82%;align-self:flex-start}\n"
            ".aact{background:transparent;border:none;color:var(--dim);"
              "border-radius:6px;padding:5px 7px;font-size:15px;cursor:pointer;line-height:1}\n"
            ".aact:hover{background:rgba(255,255,255,.08);color:var(--txt)}\n"
            ".aact.liked{color:#4caf50}\n"
            ".aact.disliked{color:#f44336}\n"
            ".aact-wrap{position:relative}\n"
            ".aact-menu{display:none;position:absolute;bottom:calc(100% + 4px);left:0;"
              "background:var(--hdr);border:1px solid var(--brd);border-radius:10px;"
              "box-shadow:0 4px 20px rgba(0,0,0,.4);padding:4px;z-index:200;min-width:190px}\n"
            ".aact-menu.open{display:block}\n"
            ".amnu-item{display:flex;align-items:center;gap:8px;width:100%;"
              "padding:9px 12px;background:transparent;border:none;color:var(--txt);"
              "font-size:14px;cursor:pointer;border-radius:7px;text-align:left;white-space:nowrap}\n"
            ".amnu-item:hover{background:rgba(255,255,255,.07)}\n"
            ".ai-details{font-size:11px;color:var(--dim);padding:2px 14px;"
              "align-self:flex-start;max-width:82%}\n"
            "#tabbar{display:flex;background:var(--hdr);border-bottom:1px solid var(--brd);"
              "flex-shrink:0}\n"
            ".tab{flex:1;padding:10px 4px;background:transparent;border:none;"
              "border-bottom:2px solid transparent;color:var(--dim);font-size:13px;cursor:pointer}\n"
            ".tab.on{color:var(--acc);border-bottom-color:var(--acc);font-weight:600}\n"
            "#tab-chat{flex:1;display:flex;flex-direction:column;overflow:hidden}\n"
            "#tab-apps,#tab-sys{flex:1;overflow-y:auto;padding:16px;"
              "display:none;box-sizing:border-box}\n"
            ".sec-hd{font-size:11px;color:var(--dim);text-transform:uppercase;"
              "letter-spacing:.8px;margin:14px 0 8px;padding-bottom:5px;"
              "border-bottom:1px solid var(--brd)}\n"
            ".app-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(80px,1fr));"
              "gap:10px;margin-bottom:4px}\n"
            ".app-btn{display:flex;flex-direction:column;align-items:center;gap:4px;"
              "padding:12px 6px;background:var(--aim);border:1px solid var(--brd);"
              "border-radius:12px;color:var(--txt);font-size:11px;cursor:pointer;"
              "text-align:center;line-height:1.3;width:100%}\n"
            ".app-btn:hover{border-color:var(--acc);background:rgba(128,128,255,.08)}\n"
            ".app-btn>span:first-child{font-size:24px}\n"
            ".app-st{font-size:10px;min-height:13px;border-radius:8px;"
              "padding:1px 5px;display:block;font-weight:600}\n"
            ".app-ok{background:rgba(80,200,120,.2);color:#50c878}\n"
            ".app-err{background:rgba(255,80,80,.2);color:#ff5050}\n"
            ".stat-card{background:var(--aim);border:1px solid var(--brd);border-radius:12px;"
              "padding:14px 16px;margin-bottom:10px}\n"
            ".stat-lbl{font-size:11px;color:var(--dim);text-transform:uppercase;letter-spacing:.8px}\n"
            ".stat-val{font-size:24px;font-weight:700;color:var(--txt);margin:4px 0}\n"
            ".stat-bar{height:5px;background:rgba(255,255,255,.1);border-radius:3px;overflow:hidden}\n"
            ".stat-fill{height:100%;background:var(--acc);border-radius:3px;transition:width .5s ease}\n"
            "#mcp-panel{margin-top:8px}\n"
            "#mcp-top{display:flex;gap:8px;flex-wrap:wrap;margin-bottom:10px}\n"
            "#mcp-preset{flex:1;background:var(--inp);border:1px solid var(--brd);"
              "color:var(--txt);border-radius:8px;padding:8px 10px;font-size:13px}\n"
            "#mcp-port{width:90px;background:var(--inp);border:1px solid var(--brd);"
              "color:var(--txt);border-radius:8px;padding:8px 10px;font-size:13px;display:none}\n"
            "#mcp-conn{background:var(--acc);color:#fff;border:none;border-radius:8px;"
              "padding:8px 14px;font-size:13px;cursor:pointer;flex-shrink:0}\n"
            "#mcp-tools{margin-top:4px}\n"
            "#mcp-tool-sel{width:100%;background:var(--inp);border:1px solid var(--brd);"
              "color:var(--txt);border-radius:8px;padding:8px 10px;font-size:13px;"
              "margin-bottom:8px;box-sizing:border-box}\n"
            ".mcp-desc{font-size:12px;color:var(--dim);margin-bottom:8px;min-height:14px}\n"
            "#mcp-params{width:100%;background:var(--inp);border:1px solid var(--brd);"
              "color:var(--txt);border-radius:8px;padding:8px 10px;font-size:12px;"
              "font-family:monospace;resize:vertical;box-sizing:border-box;margin-bottom:8px}\n"
            "#mcp-run{width:100%;background:var(--acc);color:#fff;border:none;"
              "border-radius:8px;padding:10px;font-size:14px;cursor:pointer;margin-bottom:8px}\n"
            "#mcp-result{background:var(--aim);border:1px solid var(--brd);border-radius:8px;"
              "padding:10px;font-size:12px;font-family:monospace;white-space:pre-wrap;"
              "word-break:break-all;max-height:220px;overflow-y:auto;display:none}\n"
            "</style>\n</head>\n<body>\n"
            "<div id=\"ov\"></div>\n"
            "<div id=\"drw\">"
              "<div id=\"drw-hdr\">"
                "<h2>&#9776; Menu</h2>"
                "<button id=\"cls\">&#10005;</button>"
              "</div>"
              "<div class=\"dsec\">"
                "<h3>Modello</h3>"
                "<select id=\"mdl-sel\"><option>Caricamento...</option></select>"
              "</div>"
              "<div class=\"dsec\">"
                "<h3>Personalit&#224;</h3>"
                "<button class=\"pbtn\" data-p=\"nessuna\" data-sys=\"\">"
                  "&#x1F6AB; Nessuna</button>"
                "<button class=\"pbtn\" data-p=\"jarvis\""
                  " data-sys=\"Rispondi come JARVIS, l'AI di Tony Stark: professionale,"
                  " preciso, con sottile ironia britannica. Chiama l'utente 'signore'.\">"
                  "&#x1F916; Jarvis</button>"
                "<button class=\"pbtn\" data-p=\"kitt\""
                  " data-sys=\"Rispondi come KITT, il sistema di bordo di Knight Rider:"
                  " sofisticato, calmo, formale, con occasionali riferimenti alla guida"
                  " e alla sicurezza stradale.\">"
                  "&#x1F697; KITT</button>"
                "<button class=\"pbtn\" data-p=\"yoda\""
                  " data-sys=\"Rispondi come Yoda di Star Wars. Sintassi invertita"
                  " (complemento-soggetto-verbo). Breve, saggio, sereno.\">"
                  "&#x1F33F; Yoda</button>"
                "<button class=\"pbtn\" data-p=\"snake\""
                  " data-sys=\"Rispondi come Solid Snake di Metal Gear Solid: diretto,"
                  " tattico, cinismo controllato, frasi brevi e incisive.\">"
                  "&#x1F3AE; Snake</button>"
                "<button class=\"pbtn\" data-p=\"sonic\""
                  " data-sys=\"Rispondi come Sonic the Hedgehog: rapido, energico,"
                  " spiritoso, leggermente impaziente con le cose lente.\">"
                  "&#x1F4A8; Sonic</button>"
                "<button class=\"pbtn\" data-p=\"mario\""
                  " data-sys=\"Rispondi come Super Mario: entusiasta, positivo, usa"
                  " esclamazioni come 'Wahoo!' e 'Mamma mia!', sempre incoraggiante.\">"
                  "&#x1F344; Mario</button>"
              "</div>"
              "<div class=\"dsec\">"
                "<h3>Strumenti</h3>"
                "<button class=\"tbtn\" data-t=\"nessuno\" data-sys=\"\">"
                  "&#x1F4AC; Nessuno</button>"
                "<button class=\"tbtn\" data-t=\"scrittura\""
                  " data-sys=\"Sei un esperto di scrittura e comunicazione. Aiuta l'utente"
                  " a scrivere testi chiari, coinvolgenti e grammaticalmente corretti."
                  " Proponi miglioramenti stilistici quando utile.\">"
                  "&#x270D; Scrittura</button>"
                "<button class=\"tbtn\" data-t=\"programmazione\""
                  " data-sys=\"Sei un esperto programmatore. Fornisci codice corretto,"
                  " sicuro e idiomatico. Spiega le scelte tecniche, segnala bug e"
                  " vulnerabilit&#224;, suggerisci refactoring quando opportuno.\">"
                  "&#x1F4BB; Programmazione</button>"
                "<button class=\"tbtn\" data-t=\"matematica\""
                  " data-sys=\"Sei un esperto di matematica e statistica. Risolvi problemi"
                  " passo per passo, mostra i calcoli intermedi e spiega i concetti"
                  " in modo rigoroso ma accessibile.\">"
                  "&#x03C0; Matematica</button>"
                "<button class=\"tbtn\" data-t=\"ricerca\""
                  " data-sys=\"Sei un assistente di ricerca accademica. Analizza fonti,"
                  " individua metodologie, sintetizza concetti complessi e aiuta a"
                  " strutturare argomenti in modo rigoroso e citabile.\">"
                  "&#x1F52C; Ricerca</button>"
                "<button class=\"tbtn\" data-t=\"impara\""
                  " data-sys=\"Sei un tutor paziente e motivante. Adatta il livello di"
                  " spiegazione all'utente, usa esempi concreti, poni domande di verifica"
                  " e incoraggia la curiosit&#224; intellettuale.\">"
                  "&#x1F4DA; Impara</button>"
                "<button class=\"tbtn\" data-t=\"lavoro\""
                  " data-sys=\"Sei un career coach esperto. Aiuta con CV, lettere di"
                  " presentazione, preparazione colloqui, analisi offerte di lavoro"
                  " e strategie di ricerca dell'impiego.\">"
                  "&#x1F4BC; Cerca Lavoro</button>"
              "</div>"
              "<div class=\"dsec\">"
                "<h3>Tema</h3>"
                "<select id=\"tsel\">"
                  "<option value=\"dark\">&#x1F311; Dark</option>"
                  "<option value=\"light\">&#9728; Chiaro</option>"
                  "<option value=\"dracula\">&#x1F9DB; Dracula</option>"
                  "<option value=\"nord\">&#10052; Nord</option>"
                  "<option value=\"solarized\">&#x1F30A; Solarized</option>"
                "</select>"
              "</div>"
            "</div>\n"
            "<div id=\"hdr\">"
              "<button id=\"ham\">&#9776;</button>"
              "<span style=\"font-size:22px\">&#127866;</span>"
              "<h1>Prismalux</h1>"
              "<span id=\"mdlBadge\" class=\"mdl\">" + model + "</span>"
            "</div>\n"
            "<div id=\"tabbar\">"
              "<button class=\"tab on\" data-tab=\"chat\">&#x1F4AC; Chat</button>"
              "<button class=\"tab\" data-tab=\"apps\">&#x1F680; App</button>"
              "<button class=\"tab\" data-tab=\"sys\">&#x1F4CA; Sistema</button>"
            "</div>\n"
            "<div id=\"tab-chat\">"
              "<div id=\"log\"></div>"
              "<div id=\"bar\">"
                "<input type=\"file\" id=\"att-in\" style=\"display:none\">"
                "<div id=\"bar-mid\">"
                  "<div id=\"att-tag\">"
                    "<span id=\"att-name\"></span>"
                    "<button id=\"att-clr\">&#10005;</button>"
                  "</div>"
                  "<textarea id=\"txt\" rows=\"3\""
                    " placeholder=\"Scrivi un messaggio...\"></textarea>"
                "</div>"
                "<div id=\"bar-right\">"
                  "<div id=\"bar-tools\">"
                    "<button id=\"att-btn\">&#x1F4CE;</button>"
                    "<button id=\"mic-btn\">&#x1F3A4;</button>"
                  "</div>"
                  "<button id=\"snd\">Invia</button>"
                "</div>"
              "</div>"
            "</div>\n"
            "<div id=\"tab-apps\">"
              "<div class=\"sec-hd\">&#x1F579; App Controller</div>"
              "<div class=\"app-grid\">"
                "<button class=\"app-btn\" data-cmd=\"blender\"><span>&#x1F3A8;</span><span>Blender</span><span class=\"app-st\"></span></button>"
                "<button class=\"app-btn\" data-cmd=\"freecad\"><span>&#x2699;&#xFE0F;</span><span>FreeCAD</span><span class=\"app-st\"></span></button>"
                "<button class=\"app-btn\" data-cmd=\"libreoffice\"><span>&#x1F4C4;</span><span>LibreOffice</span><span class=\"app-st\"></span></button>"
                "<button class=\"app-btn\" data-cmd=\"cloudcompare\"><span>&#x2601;&#xFE0F;</span><span>CloudCmp</span><span class=\"app-st\"></span></button>"
                "<button class=\"app-btn\" data-cmd=\"anki\"><span>&#x1F0CF;</span><span>Anki</span><span class=\"app-st\"></span></button>"
                "<button class=\"app-btn\" data-cmd=\"kicad\"><span>&#x1F50C;</span><span>KiCAD</span><span class=\"app-st\"></span></button>"
                "<button class=\"app-btn\" data-cmd=\"obs\"><span>&#x1F3AC;</span><span>OBS</span><span class=\"app-st\"></span></button>"
                "<button class=\"app-btn\" data-cmd=\"opencode\"><span>&#x1F916;</span><span>OpenCode</span><span class=\"app-st\"></span></button>"
                "<button class=\"app-btn\" data-cmd=\"godot\"><span>&#x1F3AE;</span><span>Godot</span><span class=\"app-st\"></span></button>"
                "<button class=\"app-btn\" data-cmd=\"tinymcp\"><span>&#x1F9E9;</span><span>TinyMCP</span><span class=\"app-st\"></span></button>"
              "</div>"
              "<div class=\"sec-hd\">&#x1F58C;&#xFE0F; Grafica &amp; Media</div>"
              "<div class=\"app-grid\">"
                "<button class=\"app-btn\" data-cmd=\"gimp\"><span>&#x1F5BC;&#xFE0F;</span><span>GIMP</span><span class=\"app-st\"></span></button>"
                "<button class=\"app-btn\" data-cmd=\"inkscape\"><span>&#x270F;&#xFE0F;</span><span>Inkscape</span><span class=\"app-st\"></span></button>"
                "<button class=\"app-btn\" data-cmd=\"vlc\"><span>&#x1F3B5;</span><span>VLC</span><span class=\"app-st\"></span></button>"
                "<button class=\"app-btn\" data-cmd=\"vscode\"><span>&#x1F4BB;</span><span>VS Code</span><span class=\"app-st\"></span></button>"
              "</div>"
              "<div class=\"sec-hd\">&#x1F5A5;&#xFE0F; Sistema</div>"
              "<div class=\"app-grid\">"
                "<button class=\"app-btn\" data-cmd=\"firefox\"><span>&#x1F98A;</span><span>Firefox</span><span class=\"app-st\"></span></button>"
                "<button class=\"app-btn\" data-cmd=\"chromium\"><span>&#x1F310;</span><span>Chromium</span><span class=\"app-st\"></span></button>"
                "<button class=\"app-btn\" data-cmd=\"nautilus\"><span>&#x1F4C1;</span><span>File</span><span class=\"app-st\"></span></button>"
                "<button class=\"app-btn\" data-cmd=\"konsole\"><span>&#x1F5A5;&#xFE0F;</span><span>Terminale</span><span class=\"app-st\"></span></button>"
              "</div>"
              "<div class=\"sec-hd\">&#x1F50C; MCP &#x2014; Controllo Diretto</div>"
              "<div id=\"mcp-panel\">"
                "<div id=\"mcp-top\">"
                  "<select id=\"mcp-preset\">"
                    "<option value=\"8765\">&#x1F3A8; Blender (8765)</option>"
                    "<option value=\"8766\">&#x2699;&#xFE0F; FreeCAD (8766)</option>"
                    "<option value=\"8765\">&#x1F50C; KiCAD (8765)</option>"
                    "<option value=\"9001\">&#x1F3AE; Godot (9001)</option>"
                    "<option value=\"8765\">&#x1F916; OpenCode (8765)</option>"
                    "<option value=\"custom\">&#x270F;&#xFE0F; Personalizzato...</option>"
                  "</select>"
                  "<input type=\"number\" id=\"mcp-port\" placeholder=\"Porta\" min=\"1\" max=\"65535\">"
                  "<button id=\"mcp-conn\">&#x1F517; Connetti</button>"
                "</div>"
                "<div id=\"mcp-tools\" style=\"display:none\">"
                  "<select id=\"mcp-tool-sel\"><option value=\"\">-- seleziona strumento --</option></select>"
                  "<div class=\"mcp-desc\" id=\"mcp-desc\"></div>"
                  "<textarea id=\"mcp-params\" rows=\"3\" placeholder=\"{}\"></textarea>"
                  "<button id=\"mcp-run\">&#x25B6; Esegui</button>"
                  "<div id=\"mcp-result\"></div>"
                "</div>"
              "</div>"
            "</div>\n"
            "<div id=\"tab-sys\">"
              "<div class=\"stat-card\">"
                "<div class=\"stat-lbl\">CPU (carico)</div>"
                "<div class=\"stat-val\" id=\"s-cpu\">--%</div>"
                "<div class=\"stat-bar\"><div class=\"stat-fill\" id=\"s-cpu-b\" style=\"width:0\"></div></div>"
              "</div>"
              "<div class=\"stat-card\">"
                "<div class=\"stat-lbl\">RAM</div>"
                "<div class=\"stat-val\" id=\"s-ram\">-- / -- GB</div>"
                "<div class=\"stat-bar\"><div class=\"stat-fill\" id=\"s-ram-b\" style=\"width:0\"></div></div>"
              "</div>"
              "<div class=\"stat-card\">"
                "<div class=\"stat-lbl\">Uptime</div>"
                "<div class=\"stat-val\" id=\"s-up\" style=\"font-size:20px\">--</div>"
              "</div>"
              "<div class=\"stat-card\">"
                "<div class=\"stat-lbl\">Host</div>"
                "<div class=\"stat-val\" id=\"s-host\" style=\"font-size:18px\">--</div>"
              "</div>"
              "<p style=\"text-align:center;color:var(--dim);font-size:11px;margin-top:8px\">"
                "Aggiornamento ogni 3s</p>"
            "</div>\n"
            "<script>\n"
            "const THEMES={"
              "dark:{bg:'#0f1117',hdr:'#1a1d2e',brd:'#2a2d4e',acc:'#6c63ff',"
                "usr:'#6c63ff',aim:'#1e2235',txt:'#e0e0f0',dim:'#888',inp:'#0f1117'},"
              "light:{bg:'#f0f2f5',hdr:'#ffffff',brd:'#dde1ec',acc:'#6c63ff',"
                "usr:'#6c63ff',aim:'#ffffff',txt:'#1a1a2e',dim:'#666',inp:'#f0f2f5'},"
              "dracula:{bg:'#282a36',hdr:'#21222c',brd:'#44475a',acc:'#bd93f9',"
                "usr:'#ff79c6',aim:'#383a59',txt:'#f8f8f2',dim:'#6272a4',inp:'#282a36'},"
              "nord:{bg:'#2e3440',hdr:'#3b4252',brd:'#434c5e',acc:'#88c0d0',"
                "usr:'#5e81ac',aim:'#3b4252',txt:'#eceff4',dim:'#8fbcbb',inp:'#2e3440'},"
              "solarized:{bg:'#002b36',hdr:'#073642',brd:'#586e75',acc:'#2aa198',"
                "usr:'#268bd2',aim:'#073642',txt:'#93a1a1',dim:'#657b83',inp:'#002b36'}"
            "};\n"
            "function applyTheme(n){"
              "const t=THEMES[n]||THEMES.dark,r=document.documentElement.style;"
              "r.setProperty('--bg',t.bg);r.setProperty('--hdr',t.hdr);"
              "r.setProperty('--brd',t.brd);r.setProperty('--acc',t.acc);"
              "r.setProperty('--usr',t.usr);r.setProperty('--aim',t.aim);"
              "r.setProperty('--txt',t.txt);r.setProperty('--dim',t.dim);"
              "r.setProperty('--inp',t.inp);localStorage.setItem('plx-theme',n);}\n"
            "const tSaved=localStorage.getItem('plx-theme')||'dark';"
            "applyTheme(tSaved);"
            "document.getElementById('tsel').value=tSaved;\n"
            "document.getElementById('tsel').addEventListener('change',"
              "function(){applyTheme(this.value);});\n"
            "const DRW=document.getElementById('drw'),OV=document.getElementById('ov');\n"
            "function openDrw(){DRW.classList.add('open');OV.classList.add('open');}\n"
            "function closeDrw(){DRW.classList.remove('open');OV.classList.remove('open');}\n"
            "document.getElementById('ham').addEventListener('click',openDrw);\n"
            "document.getElementById('cls').addEventListener('click',closeDrw);\n"
            "OV.addEventListener('click',closeDrw);\n"
            "let sysPers='';\n"
            "function applyPersona(key){"
              "sysPers='';"
              "document.querySelectorAll('.pbtn').forEach(function(b){"
                "if(b.dataset.p===key){b.classList.add('on');sysPers=b.dataset.sys||'';}"
                "else b.classList.remove('on');});"
              "localStorage.setItem('plx-persona',key);}\n"
            "document.querySelectorAll('.pbtn').forEach(function(b){"
              "b.addEventListener('click',function(){"
                "applyPersona(this.dataset.p);closeDrw();});});\n"
            "applyPersona(localStorage.getItem('plx-persona')||'nessuna');\n"
            "let sysTool='';\n"
            "function applyTool(key){"
              "sysTool='';"
              "document.querySelectorAll('.tbtn').forEach(function(b){"
                "if(b.dataset.t===key){b.classList.add('on');sysTool=b.dataset.sys||'';}"
                "else b.classList.remove('on');});"
              "localStorage.setItem('plx-tool',key);}\n"
            "document.querySelectorAll('.tbtn').forEach(function(b){"
              "b.addEventListener('click',function(){applyTool(this.dataset.t);closeDrw();});});\n"
            "applyTool(localStorage.getItem('plx-tool')||'nessuno');\n"
            "let recog=null,isListening=false,baseText='';\n"
            "const micBtn=document.getElementById('mic-btn');\n"
            "const SR=window.SpeechRecognition||window.webkitSpeechRecognition;\n"
            "if(SR){"
              "recog=new SR();recog.lang='it-IT';"
              "recog.continuous=false;recog.interimResults=true;\n"
              "function resetMic(){"
                "isListening=false;"
                "micBtn.classList.remove('on');"
                "micBtn.textContent='\xf0\x9f\x8e\xa4';}\n"
              "recog.onresult=function(e){"
                "let t=baseText;"
                "for(let i=e.resultIndex;i<e.results.length;i++)"
                  "t+=e.results[i][0].transcript;"
                "T.value=t;"
                "T.style.height='';T.style.height=Math.min(T.scrollHeight,200)+'px';};\n"
              "recog.onend=function(){baseText=T.value;resetMic();};\n"
              "recog.onerror=function(){resetMic();};\n"
              "micBtn.addEventListener('click',function(){"
                "if(isListening){recog.abort();resetMic();}"
                "else{"
                  "baseText=T.value;"
                  "try{recog.start();}catch(e){return;}"
                  "isListening=true;"
                  "micBtn.classList.add('on');"
                  "micBtn.textContent='\xe2\x8f\xb9';}});"
            "}else{micBtn.style.display='none';}\n"
            "let curModel='" + model + "';\n"
            "const MSEL=document.getElementById('mdl-sel'),"
              "BADGE=document.getElementById('mdlBadge');\n"
            "async function fetchModels(){try{"
              "const r=await fetch('/api/tags',{headers:{" + authHeadersJs + "}});"
              "const j=await r.json(),models=j.models||[];"
              "if(!models.length)return;"
              "MSEL.innerHTML='';"
              "models.forEach(function(m){const o=document.createElement('option');"
              "o.value=m.name;o.textContent=m.name;MSEL.appendChild(o);});"
              "const sv=localStorage.getItem('plx-model');"
              "MSEL.value=(sv&&[...MSEL.options].some(function(o){return o.value===sv;}))"
                "?sv:curModel;"
              "curModel=MSEL.value;BADGE.textContent=curModel;"
            "}catch(e){}}\n"
            "MSEL.addEventListener('change',function(){"
              "curModel=this.value;BADGE.textContent=curModel;"
              "localStorage.setItem('plx-model',curModel);});\n"
            "fetchModels();\n"
            /* ── Allegati ── */
            "let attFile=null;\n"
            "const ATB=document.getElementById('att-btn'),"
              "AIN=document.getElementById('att-in'),"
              "ATAG=document.getElementById('att-tag'),"
              "ANME=document.getElementById('att-name');\n"
            "ATB.addEventListener('click',function(){AIN.click();});\n"
            "AIN.addEventListener('change',function(){"
              "if(this.files[0]){"
                "attFile=this.files[0];"
                "ANME.textContent='\xf0\x9f\x93\x8e ' + attFile.name;"
                "ATAG.style.display='flex';}});\n"
            "document.getElementById('att-clr').addEventListener('click',function(){"
              "attFile=null;AIN.value='';ATAG.style.display='none';});\n"
            "function readTxt(f){return new Promise(function(res){"
              "const r=new FileReader();"
              "r.onload=function(e){res(e.target.result);};"
              "r.onerror=function(){res('');};"
              "r.readAsText(f);});}\n"
            /* ── Chat ── */
            "const L=document.getElementById('log'),"
              "T=document.getElementById('txt'),"
              "B=document.getElementById('snd'),"
              "H=[];\n"
            "function add(r,t){const d=document.createElement('div');"
              "d.className='msg '+r;d.textContent=t;"
              "L.appendChild(d);L.scrollTop=L.scrollHeight;return d;}\n"
            "function addActions(aiD,txt){"
              "const w=document.createElement('div');w.className='ai-actions';\n"
              "function mk(lbl,ttl){const b=document.createElement('button');"
                "b.className='aact';b.title=ttl;b.innerHTML=lbl;return b;}\n"
              "const lk=mk('&#x1F44D;','Utile'),dk=mk('&#x1F44E;','Non utile');\n"
              "lk.addEventListener('click',function(){"
                "lk.classList.toggle('liked');dk.classList.remove('disliked');});\n"
              "dk.addEventListener('click',function(){"
                "dk.classList.toggle('disliked');lk.classList.remove('liked');});\n"
              "const rg=mk('&#x1F504;','Rigenera');\n"
              "rg.addEventListener('click',function(){"
                "w.remove();aiD.remove();"
                "if(H.length&&H[H.length-1].role==='assistant')H.pop();"
                "B.disabled=true;streamResponse();});\n"
              "const cp=mk('&#x1F4CB;','Copia');\n"
              "cp.addEventListener('click',function(){"
                "navigator.clipboard.writeText(txt).then(function(){"
                  "cp.innerHTML='&#x2713;';"
                  "setTimeout(function(){cp.innerHTML='&#x1F4CB;';},1500);});});\n"
              "const mw=document.createElement('div');mw.className='aact-wrap';\n"
              "const mb=mk('&bull;&bull;&bull;','Altro');\n"
              "const mn=document.createElement('div');mn.className='aact-menu';\n"
              "const ti=document.createElement('button');"
                "ti.className='amnu-item';ti.innerHTML='&#x1F50A; Ascolta';"
                "let spk=false;\n"
              "ti.addEventListener('click',function(){"
                "if(spk){speechSynthesis.cancel();spk=false;"
                  "ti.innerHTML='&#x1F50A; Ascolta';}"
                "else{const u=new SpeechSynthesisUtterance(txt);u.lang='it-IT';"
                  "u.onend=function(){spk=false;ti.innerHTML='&#x1F50A; Ascolta';};"
                  "speechSynthesis.speak(u);spk=true;ti.innerHTML='&#x23F9; Stop';}"
                "mn.classList.remove('open');});\n"
              "const ei=document.createElement('button');"
                "ei.className='amnu-item';ei.innerHTML='&#x1F4C4; Esporta';\n"
              "ei.addEventListener('click',function(){"
                "const bl=new Blob([txt],{type:'text/plain'}),"
                  "a=document.createElement('a');"
                "a.href=URL.createObjectURL(bl);a.download='risposta.txt';a.click();"
                "URL.revokeObjectURL(a.href);mn.classList.remove('open');});\n"
              "const di=document.createElement('button');"
                "di.className='amnu-item';di.innerHTML='&#x1F4CA; Dettagli risposta';\n"
              "di.addEventListener('click',function(){"
                "const wds=txt.trim().split(/\\s+/).length,"
                  "tkn=Math.round(txt.length/4);"
                "const inf=document.createElement('div');inf.className='ai-details';"
                "inf.textContent='Modello: '+curModel+' \\u2022 ~'+tkn"
                  "+' token \\u2022 '+wds+' parole';"
                "w.after(inf);setTimeout(function(){inf.remove();},5000);"
                "mn.classList.remove('open');});\n"
              "mn.appendChild(ti);mn.appendChild(ei);mn.appendChild(di);\n"
              "mb.addEventListener('click',function(e){"
                "e.stopPropagation();mn.classList.toggle('open');});\n"
              "document.addEventListener('click',function(){mn.classList.remove('open');});\n"
              "mw.appendChild(mb);mw.appendChild(mn);\n"
              "w.appendChild(lk);w.appendChild(dk);w.appendChild(rg);"
                "w.appendChild(cp);w.appendChild(mw);\n"
              "L.appendChild(w);L.scrollTop=L.scrollHeight;}\n"
            "async function streamResponse(){\n"
              "const msgs=[];\n"
              "const sp=[sysTool,sysPers].filter(Boolean).join('\\n');\n"
              "if(sp)msgs.push({role:'system',content:sp});\n"
              "msgs.push(...H);\n"
              "const aiD=add('ai','...');let full='';\n"
              "const dF=['...','..','.']; let dI=0;\n"
              "const dT=setInterval(function(){"
                "if(!full){aiD.textContent=dF[dI%3];dI++;}},400);\n"
              "try{\n"
                "const r=await fetch('/api/chat',{method:'POST',"
                  "headers:{" + authHeadersJs + "},"
                  "body:JSON.stringify({model:curModel,messages:msgs,stream:true})});\n"
                "const rd=r.body.getReader(),dc=new TextDecoder();\n"
                "while(true){\n"
                  "const {done,value}=await rd.read();if(done)break;\n"
                  "for(const ln of dc.decode(value).split('\\n')){\n"
                    "if(!ln.trim())continue;\n"
                    "try{const o=JSON.parse(ln);\n"
                      "const tk=(o.message&&o.message.content)||o.response||'';\n"
                      "if(tk){if(!full)clearInterval(dT);"
                        "full+=tk;aiD.textContent=full;"
                        "L.scrollTop=L.scrollHeight;}\n"
                    "}catch(x){}\n"
                  "}\n"
                "}\n"
              "}catch(e){"
                "clearInterval(dT);"
                "if(!full)aiD.textContent='Errore: '+e.message;"
              "}\n"
              "clearInterval(dT);\n"
              "if(full){H.push({role:'assistant',content:full});addActions(aiD,full);}\n"
              "B.disabled=false;T.focus();\n"
            "}\n"
            "async function go(){\n"
              "const m=T.value.trim();"
              "if(!m&&!attFile)return;\n"
              "T.value='';T.style.height='';B.disabled=true;\n"
              "let sendTxt=m,dispTxt=m;\n"
              "if(attFile){\n"
                "dispTxt=(m?m+'\\n':'')+'\xf0\x9f\x93\x8e '+attFile.name;\n"
                "const ext=attFile.name.split('.').pop().toLowerCase();\n"
                "const txts=['txt','md','py','js','ts','cpp','c','h',"
                  "'java','json','xml','csv','log','html','css'];\n"
                "if(attFile.type.startsWith('text/')||txts.indexOf(ext)>=0){\n"
                  "const fc=await readTxt(attFile);\n"
                  "sendTxt=(m?m+'\\n\\n':'')+'['+attFile.name+']:\\n'+fc;\n"
                "}else{\n"
                  "sendTxt=(m?m+' ':'')+'[Allegato: '+attFile.name+']';\n"
                "}\n"
                "attFile=null;AIN.value='';ATAG.style.display='none';\n"
              "}\n"
              "add('user',dispTxt);H.push({role:'user',content:sendTxt});\n"
              "await streamResponse();\n"
            "}\n"
            "B.addEventListener('click',go);\n"
            "T.addEventListener('keydown',function(e){\n"
              "if(e.key==='Enter'&&!e.shiftKey){e.preventDefault();go();}\n"
              "setTimeout(function(){"
                "T.style.height='';"
                "T.style.height=Math.min(T.scrollHeight,200)+'px';},0);\n"
            "});\n"
            /* ── MCP panel ── */
            "let mcpTools=[];\n"
            "const MPRE=document.getElementById('mcp-preset'),"
              "MPRT=document.getElementById('mcp-port'),"
              "MCON=document.getElementById('mcp-conn'),"
              "MTLS=document.getElementById('mcp-tools'),"
              "MSEL=document.getElementById('mcp-tool-sel'),"
              "MDSC=document.getElementById('mcp-desc'),"
              "MPAR=document.getElementById('mcp-params'),"
              "MRUN=document.getElementById('mcp-run'),"
              "MRES=document.getElementById('mcp-result');\n"
            "MPRE.addEventListener('change',function(){"
              "MPRT.style.display=this.value==='custom'?'block':'none';});\n"
            "function getMcpPort(){"
              "return MPRE.value==='custom'?parseInt(MPRT.value)||8765:parseInt(MPRE.value);}\n"
            "MCON.addEventListener('click',async function(){"
              "MCON.textContent='&#x23F3;...';MTLS.style.display='none';MRES.style.display='none';\n"
              "try{"
                "const r=await fetch('/api/mcp',{method:'POST',"
                  "headers:{'Content-Type':'application/json'," + authHeadersJs + "},"
                  "body:JSON.stringify({port:getMcpPort(),"
                    "request:{jsonrpc:'2.0',id:1,method:'tools/list',params:{}}})});\n"
                "const j=await r.json();\n"
                "if(j.error)throw new Error(j.error);\n"
                "const tls=(j.result&&j.result.tools)||j.tools||[];\n"
                "if(!tls.length)throw new Error('Nessuno strumento trovato');\n"
                "mcpTools=tls;\n"
                "MSEL.innerHTML='<option value=\"\">-- seleziona strumento --</option>';\n"
                "tls.forEach(function(t,i){"
                  "const o=document.createElement('option');"
                  "o.value=i;o.textContent=t.name;MSEL.appendChild(o);});\n"
                "MTLS.style.display='block';\n"
                "MCON.innerHTML='&#x2713; '+tls.length+' strumenti';\n"
              "}catch(e){"
                "MCON.textContent='&#x2717; '+e.message;"
                "setTimeout(function(){MCON.innerHTML='&#x1F517; Connetti';},3000);}});\n"
            "MSEL.addEventListener('change',function(){"
              "const idx=parseInt(this.value);"
              "if(isNaN(idx)||idx<0){MDSC.textContent='';MPAR.value='';return;}\n"
              "const tool=mcpTools[idx];"
              "MDSC.textContent=tool.description||'';\n"
              "const schema=tool.inputSchema||tool.input_schema||{};"
              "const props=schema.properties||{};"
              "if(Object.keys(props).length){"
                "const tmpl={};"
                "Object.keys(props).forEach(function(k){"
                  "const p=props[k];"
                  "tmpl[k]=p['default']!==undefined?p['default']:"
                    "(p.type==='string'?'':p.type==='number'||p.type==='integer'?0:null);});"
                "MPAR.value=JSON.stringify(tmpl,null,2);"
              "}else{MPAR.value='{}';}\n"
            "});\n"
            "MRUN.addEventListener('click',async function(){"
              "const idx=parseInt(MSEL.value);"
              "if(isNaN(idx)||idx<0)return;\n"
              "const tool=mcpTools[idx];\n"
              "let args={};\n"
              "try{args=JSON.parse(MPAR.value||'{}');}"
              "catch(e){MRES.textContent='JSON non valido: '+e.message;"
                "MRES.style.display='block';return;}\n"
              "MRES.textContent='Esecuzione...';MRES.style.display='block';\n"
              "try{"
                "const r=await fetch('/api/mcp',{method:'POST',"
                  "headers:{'Content-Type':'application/json'," + authHeadersJs + "},"
                  "body:JSON.stringify({port:getMcpPort(),"
                    "request:{jsonrpc:'2.0',id:2,method:'tools/call',"
                      "params:{name:tool.name,arguments:args}}})});\n"
                "const j=await r.json();\n"
                "if(j.error){MRES.textContent='Errore MCP: '+JSON.stringify(j.error);return;}\n"
                "const content=(j.result&&j.result.content)||[];\n"
                "if(content.length){"
                  "MRES.textContent=content.map(function(c){return c.text||JSON.stringify(c);}).join('\\n');"
                "}else{"
                  "MRES.textContent=JSON.stringify(j.result||j,null,2);}"
              "}catch(e){MRES.textContent='Errore: '+e.message;}});\n"
            /* ── Tab switching ── */
            "const TABS=document.querySelectorAll('.tab'),"
              "TC=document.getElementById('tab-chat'),"
              "TA=document.getElementById('tab-apps'),"
              "TS=document.getElementById('tab-sys');\n"
            "let sysItv=null;\n"
            "function switchTab(n){"
              "TABS.forEach(function(t){t.classList.toggle('on',t.dataset.tab===n);});"
              "TC.style.display=n==='chat'?'flex':'none';"
              "TA.style.display=n==='apps'?'block':'none';"
              "TS.style.display=n==='sys'?'block':'none';"
              "if(n==='sys'){"
                "fetchSys();"
                "if(!sysItv)sysItv=setInterval(fetchSys,3000);"
              "}else{if(sysItv){clearInterval(sysItv);sysItv=null;}}}\n"
            "TABS.forEach(function(t){"
              "t.addEventListener('click',function(){switchTab(this.dataset.tab);});});\n"
            /* ── App launcher ── */
            "document.querySelectorAll('.app-btn').forEach(function(btn){"
              "btn.addEventListener('click',async function(){"
                "const cmd=this.dataset.cmd;"
                "const st=this.querySelector('.app-st');"
                "if(st){st.textContent='...';st.className='app-st';}\n"
                "try{"
                  "const r=await fetch('/api/launch',{method:'POST',"
                    "headers:{'Content-Type':'application/json'," + authHeadersJs + "},"
                    "body:JSON.stringify({app:cmd})});\n"
                  "const j=await r.json();"
                  "if(st){"
                    "st.textContent=j.status==='launched'?'&#x2713; OK':'&#x2717; Errore';"
                    "st.className='app-st '+(j.status==='launched'?'app-ok':'app-err');"
                    "setTimeout(function(){st.textContent='';st.className='app-st';},3000);}"
                "}catch(e){"
                  "if(st){st.textContent='&#x2717;';st.className='app-st app-err';"
                    "setTimeout(function(){st.textContent='';st.className='app-st';},3000);}}"
              "});});\n"
            /* ── Sysinfo ── */
            "async function fetchSys(){\n"
              "try{"
                "const r=await fetch('/api/sysinfo',{headers:{" + authHeadersJs + "}});\n"
                "const j=await r.json();\n"
                "document.getElementById('s-cpu').textContent=j.cpu_pct+'%';\n"
                "document.getElementById('s-cpu-b').style.width=j.cpu_pct+'%';\n"
                "document.getElementById('s-ram').textContent="
                  "j.ram_used_gb+' / '+j.ram_total_gb+' GB';\n"
                "document.getElementById('s-ram-b').style.width=j.ram_pct+'%';\n"
                "document.getElementById('s-up').textContent=j.uptime;\n"
                "document.getElementById('s-host').textContent=j.hostname;\n"
              "}catch(e){}}\n"
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
                          : (code == 429) ? "429 Too Many Requests"
                          : (code == 400) ? "400 Bad Request"
                          :                 "500 Internal Server Error";
    QByteArray resp = "HTTP/1.1 " + status.toLatin1() + "\r\n";
    resp += "Content-Type: application/json\r\n";
    resp += "X-Content-Type-Options: nosniff\r\n";
    resp += "X-Frame-Options: DENY\r\n";
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

QByteArray LanServer::httpOkHeader(const char* contentType) const
{
    QByteArray h = "HTTP/1.1 200 OK\r\n";
    h += "Content-Type: ";
    h += contentType;
    h += "\r\nAccess-Control-Allow-Origin: *\r\n";
    h += "X-Content-Type-Options: nosniff\r\n";
    h += "X-Frame-Options: DENY\r\n";
    h += "Referrer-Policy: no-referrer\r\n";
    return h;
}

QByteArray LanServer::httpStreamHeader() const
{
    /* Nessun Content-Length né Transfer-Encoding chunked:
       il client legge via readyRead fino alla chiusura del socket. */
    QByteArray h = "HTTP/1.1 200 OK\r\n";
    h += "Content-Type: application/x-ndjson\r\n";
    h += "Access-Control-Allow-Origin: *\r\n";
    h += "X-Content-Type-Options: nosniff\r\n";
    h += "X-Frame-Options: DENY\r\n";
    h += "Referrer-Policy: no-referrer\r\n";
    h += "X-Accel-Buffering: no\r\n";
    h += "Connection: close\r\n\r\n";
    return h;
}
