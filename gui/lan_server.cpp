#include "lan_server.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QUuid>
#include <QRegularExpression>
#include <QFileInfo>
#include <QTextStream>
#include <QProcess>
#include <cmath>
#include "prismalux_paths.h"
#include "pages/main_jobs_data.h"
namespace P = PrismaluxPaths;

#ifdef HAVE_QKEYCHAIN
#  include <qt6keychain/keychain.h>
#  include <QEventLoop>
#endif

/* ══════════════════════════════════════════════════════════════
   saveLanToken / loadLanToken — helpers statici pubblici
   ──────────────────────────────────────────────────────────────
   Se QKeychain è disponibile usa il keyring di sistema
   (Secret Service su Linux, Keychain su macOS, Credential Manager
   su Windows). Fallback: file ~/.prismalux/lan_token.key (0600).
   ══════════════════════════════════════════════════════════════ */

void LanServer::saveLanToken(const QString& token)
{
#ifdef HAVE_QKEYCHAIN
    QKeychain::WritePasswordJob job(QStringLiteral("Prismalux"));
    job.setAutoDelete(false);
    job.setKey(QStringLiteral("lan_token"));
    job.setTextData(token);
    QEventLoop loop;
    QObject::connect(&job, &QKeychain::WritePasswordJob::finished,
                     &loop, &QEventLoop::quit);
    job.start();
    loop.exec();
    if (job.error() == QKeychain::NoError) {
        QFile::remove(P::lanTokenPath());   /* rimuove il vecchio file di fallback */
        return;
    }
    qWarning() << "LanServer: QKeychain write failed:" << job.errorString()
               << "— fallback a file 0600";
#endif
    /* Fallback file-based (0600) */
    QDir().mkpath(QDir::homePath() + "/.prismalux");
    QFile f(P::lanTokenPath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
    f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    f.write(token.toUtf8());
}

QString LanServer::loadLanToken()
{
#ifdef HAVE_QKEYCHAIN
    QKeychain::ReadPasswordJob job(QStringLiteral("Prismalux"));
    job.setAutoDelete(false);
    job.setKey(QStringLiteral("lan_token"));
    QEventLoop loop;
    QObject::connect(&job, &QKeychain::ReadPasswordJob::finished,
                     &loop, &QEventLoop::quit);
    job.start();
    loop.exec();
    if (job.error() == QKeychain::NoError && !job.textData().isEmpty())
        return job.textData();
    if (job.error() != QKeychain::EntryNotFound)
        qWarning() << "LanServer: QKeychain read failed:" << job.errorString()
                   << "— fallback a file 0600";
#endif
    /* Fallback: legge il file 0600 */
    QFile f(P::lanTokenPath());
    if (f.open(QIODevice::ReadOnly))
        return QString::fromUtf8(f.readAll()).trimmed();
    return {};
}

/* Confronto constant-time — evita timing attack sul token Bearer */
bool LanServer::timingSafeEqual(const QString& a, const QString& b)
{
    if (a.size() != b.size()) return false;
    volatile int diff = 0;
    for (int i = 0; i < a.size(); ++i)
        diff |= (a[i].unicode() ^ b[i].unicode());
    return diff == 0;
}

/* Appende una riga JSON al log di accesso ~/.prismalux/access.log.
 * Formato: {"t":"…","ip":"…","m":"…","p":"…"}
 * Ruota il file se supera 10 MB (mantiene access.log.1 come backup). */
void LanServer::appendAccessLog(const QString& addr, const QString& method, const QString& path)
{
    const QString logPath = QDir::homePath() + "/.prismalux/access.log";

    /* Rotazione log > 10 MB */
    {
        QFileInfo fi(logPath);
        if (fi.exists() && fi.size() > 10 * 1024 * 1024) {
            const QString bak = logPath + ".1";
            QFile::remove(bak);
            QFile::rename(logPath, bak);
        }
    }

    QFile f(logPath);
    if (!f.open(QIODevice::Append | QIODevice::Text)) return;
    QTextStream ts(&f);
    /* JSON a riga singola — parsabile con jq/journald */
    ts << "{\"t\":\""  << QDateTime::currentDateTime().toString(Qt::ISODate)
       << "\",\"ip\":\"" << addr.toHtmlEscaped()
       << "\",\"m\":\""  << method
       << "\",\"p\":\""  << QString(path).replace("\\","\\\\").replace("\"","\\\"")
       << "\"}\n";
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
#if QT_CONFIG(ssl)
    if (m_sslServer && m_sslServer->isListening()) return true;
#endif

    if (m_accessToken.isEmpty()) {
        m_accessToken = QUuid::createUuid().toString(QUuid::WithoutBraces).replace("-", "").left(32);
        saveLanToken(m_accessToken);
        emit tokenAutoGenerated(m_accessToken);
    }

#if QT_CONFIG(ssl)
    /* Tenta TLS self-signed se richiesto */
    if (m_tlsRequested) {
        QString certPath, keyPath;
        if (_ensureCert(certPath, keyPath)) {
            QSslCertificate cert;
            {
                QFile f(certPath);
                if (f.open(QIODevice::ReadOnly)) cert = QSslCertificate(&f, QSsl::Pem);
            }
            QSslKey key;
            {
                QFile f(keyPath);
                if (f.open(QIODevice::ReadOnly)) key = QSslKey(&f, QSsl::Rsa, QSsl::Pem);
            }
            if (!cert.isNull() && !key.isNull()) {
                if (!m_sslServer) {
                    m_sslServer = new QSslServer(this);
                    connect(m_sslServer, &QTcpServer::newConnection,
                            this, &LanServer::onNewConnection);
                }
                QSslConfiguration cfg = QSslConfiguration::defaultConfiguration();
                cfg.setLocalCertificate(cert);
                cfg.setPrivateKey(key);
                m_sslServer->setSslConfiguration(cfg);
                if (m_sslServer->listen(m_bindAddress, port)) {
                    m_useTls = true;
                    emit statusChanged(true);
                    return true;
                }
                qWarning() << "LanServer: TLS listen fallito —" << m_sslServer->errorString()
                           << "— fallback HTTP";
            }
        }
        qWarning() << "LanServer: TLS non disponibile — avvio in HTTP";
    }
#endif

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

    if (!m_server->listen(m_bindAddress, port)) return false;
    emit statusChanged(true);
    return true;
}

void LanServer::stop()
{
#if QT_CONFIG(ssl)
    const bool running = m_server->isListening()
                      || (m_sslServer && m_sslServer->isListening());
#else
    const bool running = m_server->isListening();
#endif
    if (!running) return;

    /* Raccoglie i socket PRIMA di modificare la mappa.
     * Se chiamassimo disconnectFromHost() dentro il loop, il segnale
     * disconnected() verrebbe emesso in modo sincrono → onClientDisconnected()
     * chiamerebbe m_sessions.erase() invalidando l'iteratore → SIGSEGV. */
    QList<QTcpSocket*> socks;
    socks.reserve(m_sessions.size());
    for (auto& s : m_sessions)
        if (s.socket) socks << s.socket;

    /* Svuota lo stato prima di chiudere i socket.
     * m_streamSock va a nullptr prima di abort() così i segnali LLM
     * in volo (onAiToken/onAiFinished/onAiError) trovano già guard==nullptr. */
    m_sessions.clear();
    m_appClientIps.clear();
    m_streamSock = nullptr;
    m_tagsSock   = nullptr;
    m_ragSock    = nullptr;

    /* Annulla l'eventuale richiesta LLM in corso dopo aver azzerato i socket:
     * evita che Ollama continui a rispondere su socket già chiusi. */
    if (m_ai && m_ai->busy())
        m_ai->abort();

    m_server->close();
#if QT_CONFIG(ssl)
    if (m_sslServer) m_sslServer->close();
#endif
    m_useTls = false;
    m_llmQueue.clear();
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
quint16 LanServer::port() const {
#if QT_CONFIG(ssl)
    if (m_sslServer && m_sslServer->isListening()) return m_sslServer->serverPort();
#endif
    return m_server->serverPort();
}
int     LanServer::clientCount() const { return m_appClientIps.size(); }

QStringList LanServer::connectedIPs() const
{
    QStringList list;
    for (auto it = m_sessions.cbegin(); it != m_sessions.cend(); ++it)
        list << it->addr;
    return list;
}

void LanServer::kickClient(const QString& addr)
{
    QTcpSocket* target = nullptr;
    for (auto it = m_sessions.cbegin(); it != m_sessions.cend(); ++it) {
        if (it->addr == addr) { target = it.key(); break; }
    }
    if (!target) return;

    const bool wasApi = m_sessions.value(target).isApiClient;
    m_sessions.remove(target);
    if (wasApi) m_appClientIps.remove(addr);

    target->disconnect(this);
    target->abort();
    target->deleteLater();

    emit clientDisconnected(addr);
}

/* ── nuova connessione ───────────────────────────────────────────────────── */

void LanServer::onNewConnection()
{
#if QT_CONFIG(ssl)
    QTcpServer* activeSrv = (m_sslServer && m_sslServer->isListening())
                            ? static_cast<QTcpServer*>(m_sslServer)
                            : m_server;
#else
    QTcpServer* activeSrv = m_server;
#endif
    while (activeSrv->hasPendingConnections()) {
        QTcpSocket* sock = activeSrv->nextPendingConnection();
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
        emit clientConnected(s.addr);
    }
}

/* ── rate limiter condiviso /api/chat + /api/generate ───────────────────── */

void LanServer::onChatRateTimeout()      { m_chatRateCount.clear(); }
void LanServer::onKnowledgeRateTimeout() { m_knowledgeReqCount.clear(); }
void LanServer::onHeavyRateTimeout()     { m_heavyRateCount.clear(); }

/* Max 6 richieste/minuto per IP su endpoint che lanciano sottoprocessi pesanti
 * (whisper, graphviz, mcp, launch) — evita DoS da spawn illimitato di processi. */
bool LanServer::checkHeavyRateLimit(Session& s)
{
    if (!m_heavyRateTimer) {
        m_heavyRateTimer = new QTimer(this);
        m_heavyRateTimer->setInterval(60 * 1000);
        connect(m_heavyRateTimer, &QTimer::timeout,
                this, &LanServer::onHeavyRateTimeout);
        m_heavyRateTimer->start();
    }
    if (++m_heavyRateCount[s.addr] > 6) {
        sendError(s.socket, 429, "Rate limit exceeded");
        return true;
    }
    return false;
}

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

        /* Request line — validazione robusta */
        const QList<QByteArray> reqLine = lines[0].trimmed().split(' ');
        if (reqLine.size() < 2) { sendError(sock, 400, "Bad Request"); sock->disconnectFromHost(); return; }
        s.method = QString::fromLatin1(reqLine[0]).toUpper();
        s.path   = QString::fromLatin1(reqLine[1]);

        /* Solo metodi noti — blocca request smuggling e parser confusion */
        static const QSet<QString> kAllowedMethods = {
            "GET","POST","PUT","DELETE","PATCH","HEAD","OPTIONS"
        };
        if (!kAllowedMethods.contains(s.method)) {
            sendError(sock, 405, "Method Not Allowed"); sock->disconnectFromHost(); return;
        }

        /* Path non deve contenere null byte o lunghezza assurda */
        if (s.path.contains('\0') || s.path.size() > 2048) {
            sendError(sock, 400, "Bad Request"); sock->disconnectFromHost(); return;
        }

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
            if (key == "content-length") {
                bool ok = false;
                const int cl = val.toInt(&ok);
                /* Rifiuta Content-Length negativo o assurdo (>25 MB) */
                if (!ok || cl < 0 || cl > 25 * 1024 * 1024) {
                    sendError(sock, 400, "Bad Content-Length");
                    sock->disconnectFromHost(); return;
                }
                s.contentLength = cl;
            } else if (key == "authorization")
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
        serveLlmQueue();   /* serve la prossima richiesta in coda */
    }
    /* Rimuovi eventuali richieste pendenti di questo socket dalla coda */
    for (auto& p : m_llmQueue) { if (p.sock == sock) p.sock = nullptr; }
    if (sock == m_tagsSock) {
        m_tagsSock = nullptr;
        disconnect(m_modelsConn);
    }
    if (sock == m_ragSock) {
        m_ragSock = nullptr;
        disconnect(m_ai, &AiClient::embeddingReady, this, &LanServer::onRagEmbeddingReady);
        disconnect(m_ai, &AiClient::embeddingError, this, &LanServer::onRagEmbeddingError);
    }

    if (it != m_sessions.end()) {
        if (it->isApiClient) m_appClientIps.remove(addr);
        m_sessions.erase(it);
    }
    sock->deleteLater();

    if (!addr.isEmpty()) emit clientDisconnected(addr);
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
                        s.path == "/api/mcp"       ||
                        s.path == "/api/lavoro"    ||
                        s.path == "/api/cv"        ||
                        s.path == "/api/rag"       ||
                        s.path == "/api/graphviz"  ||
                        s.path == "/api/whisper");

    /* Auth check: le route API richiedono header Authorization: Bearer TOKEN.
       Il fallback ?token= è rimosso per sicurezza — il token non deve
       comparire in URL, log proxy o cronologia browser. */
    if (isApi && !m_accessToken.isEmpty()) {
        const QString expected  = "Bearer " + m_accessToken;
        const QString effective = s.authHeader;
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
    } else if (s.path.startsWith("/katex/") && s.method == "GET") {
        handleKatex(s);
    } else if (s.path == "/api/launch" && s.method == "POST") {
        if (m_headless) { sendError(s.socket, 403, "Disabled in headless mode"); return; }
        if (checkHeavyRateLimit(s)) return;
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
        if (checkHeavyRateLimit(s)) return;
        const QJsonDocument wrapDoc = QJsonDocument::fromJson(s.body);
        if (wrapDoc.isNull() || !wrapDoc.isObject()) {
            sendError(s.socket, 400, "Invalid JSON");
            return;
        }
        const QJsonObject wrapper = wrapDoc.object();

        {
            const QJsonValue reqVal = wrapper["request"];
            if (!reqVal.isObject()) {
                sendError(s.socket, 400, "request must be a JSON object");
                return;
            }
            const QJsonObject mcpReqCheck = reqVal.toObject();
            const QJsonValue methodVal = mcpReqCheck["method"];
            if (!methodVal.isString()) {
                sendError(s.socket, 400, "request.method required");
                return;
            }
            const QString methodStr = methodVal.toString();
            static const QRegularExpression kMcpMethodRe(
                QStringLiteral("^[a-zA-Z0-9/_-]{1,64}$"));
            if (!kMcpMethodRe.match(methodStr).hasMatch()) {
                sendError(s.socket, 400, "request.method formato non valido");
                return;
            }
            if (mcpReqCheck.contains("params")) {
                const QJsonValue pv = mcpReqCheck["params"];
                if (!pv.isObject() && !pv.isNull() && !pv.isUndefined()) {
                    sendError(s.socket, 400, "request.params must be a JSON object");
                    return;
                }
            }
        }

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
    } else if (s.path == "/api/lavoro" && s.method == "POST") {
        const QJsonObject req = QJsonDocument::fromJson(s.body).object();
        const QString tipo    = req["tipo"].toString("tutti");
        const QString livello = req["livello"].toString("tutti");
        const QList<Offerta> filtrate = offerteFiltrate(tipo, livello);
        QJsonArray arr;
        for (const auto& o : filtrate) {
            QJsonObject obj;
            obj["azienda"]   = o.azienda;
            obj["ruolo"]     = o.ruolo;
            obj["sede"]      = o.sede;
            obj["tipo"]      = o.tipo;
            obj["livello"]   = o.livello;
            obj["email"]     = o.email;
            obj["requisiti"] = o.requisiti;
            arr.append(obj);
        }
        sendJson(s.socket, QJsonDocument(arr).toJson(QJsonDocument::Compact));
    } else if (s.path == "/api/cv" && s.method == "GET") {
        static const QString cvTxt =
            "Paolo Lo Bello, nato il 15/02/1989, Catania (36 anni).\n"
            "Email: wildlux@gmail.com | Tel: +39 340 96 25 057\n"
            "Patente B Europea. Dislessico (certificato ASL Catania).\n\n"
            "TITOLO DI STUDIO:\n"
            "- Perito Informatico — ITIS G. Marconi, Catania (2003-2010) — voto 67/100 — MQRF Level 4\n"
            "- CCNA Cisco Certificate Associate — ICE Malta (2023-2024)\n"
            "- Certificato E-Commerce (Joomla, PHP, HTML, Web Marketing) — CESIS (2010-2012)\n"
            "- Certificato Photoshop CS5 — ITIS Galileo Ferraris Acireale (2012)\n"
            "- Inglese A2 (ELA Malta, 2019)\n\n"
            "ESPERIENZE LAVORATIVE:\n"
            "- Lidl LTD Malta (lug 2024): cassa POS, muletto elettrico, controllo stock e date\n"
            "- Scott Supermarket Malta (apr 2020 - mag 2024): muletto manuale, facing product, ricollocamento\n"
            "- Playmobil/Poultons Ltd Malta (dic 2019 - feb 2020): operatore macchina, controllo qualita'\n"
            "- Convenience Shop Malta (giu-ago 2019): assistente negozio, cassa, scaffali\n"
            "- Karma Swim Catania (2014-2015): grafico freelance — Adobe Illustrator\n"
            "- Techno Work srl Catania (nov 2013): Python developer su Raspberry Pi 3, GNU/Linux\n"
            "- Almaviva Misterbianco (gen-feb 2012): call center Mediaset Premium\n"
            "- Mics SRL Misterbianco (giu-lug 2011): inbound call operator Enel Energia\n"
            "- Gio' Casa Misterbianco (ago 2005): assistenza e vendita condizionatori\n\n"
            "COMPETENZE TECNICHE:\n"
            "- Reti: CCNA Cisco, SSH, Kleopatra, PuTTY, FileZilla\n"
            "- Sviluppo: Python, C++, HTML, PHP, SQL, MySQL, JavaScript, Node.js, Assembler x86\n"
            "- OS: GNU/Linux, macOS, Windows\n"
            "- Web: WordPress, Joomla, Prestashop, Django\n"
            "- Grafica: Adobe Photoshop CS5, GIMP, Adobe Illustrator\n"
            "- 3D: Blender (2007-oggi) — mesh, rigging, rendering, video promozionali\n"
            "- Virtual: VirtualBox, Docker\n"
            "- Office: LibreOffice, Microsoft Office, gestione email";
        QJsonObject cvj;
        cvj["cv"] = cvTxt;
        sendJson(s.socket, QJsonDocument(cvj).toJson(QJsonDocument::Compact));
    } else if (s.path == "/api/rag" && s.method == "POST") {
        handleRag(s);
    } else if (s.path == "/api/graphviz" && s.method == "POST") {
        if (checkHeavyRateLimit(s)) return;
        handleGraphviz(s);
    } else if (s.path == "/api/whisper" && s.method == "POST") {
        if (checkHeavyRateLimit(s)) return;
        handleWhisper(s);
    } else if (s.path == "/api/sync") {
        handleSync(s);
    } else if (s.path == "/api/math" && s.method == "POST") {
        handleMath(s.socket, s);
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
            "#tab-chat{flex:1;min-height:0;display:flex;flex-direction:column;overflow:hidden}\n"
            "#tab-apps,#tab-sys{flex:1;min-height:0;overflow-y:auto;padding:16px;"
              "display:none;box-sizing:border-box}\n"
            ".tab-on-flex{display:flex!important}.tab-on-block{display:block!important}"
              ".tab-off{display:none!important}\n"
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
            "#tool-ind{padding:6px 14px;font-size:13px;font-weight:600;display:none;"
              "align-items:center;gap:6px;flex-shrink:0;"
              "border-left:3px solid transparent;border-bottom:1px solid var(--brd);"
              "transition:background .25s}\n"
            "#tool-extra{display:none;gap:6px;flex-wrap:wrap;align-items:center;"
              "padding:4px 0;flex-shrink:0}\n"
            ".xsel-lbl{font-size:12px;color:var(--dim);flex-shrink:0;white-space:nowrap}\n"
            ".xsel{background:var(--inp);border:1px solid var(--brd);color:var(--txt);"
              "border-radius:8px;padding:5px 8px;font-size:12px;flex:1;min-width:0}\n"
            ".sym-btn{background:var(--aim);border:1px solid var(--brd);color:var(--txt);"
              "border-radius:6px;padding:3px 8px;font-size:14px;cursor:pointer;flex-shrink:0}\n"
            ".sym-btn:hover{background:rgba(255,255,255,.1);border-color:var(--acc)}\n"
            "#lavoro-panel{display:none;flex-direction:column;gap:0;"
              "flex-shrink:0;max-height:46vh;border-bottom:1px solid var(--brd);"
              "background:var(--bg)}\n"
            "#lav-filters{display:flex;gap:6px;flex-wrap:wrap;align-items:center;"
              "padding:6px 10px;background:var(--hdr);flex-shrink:0;"
              "border-bottom:1px solid var(--brd)}\n"
            "#lav-filters select{background:var(--inp);border:1px solid var(--brd);"
              "color:var(--txt);border-radius:8px;padding:4px 8px;font-size:12px;"
              "flex:1;min-width:80px}\n"
            "#lav-search{background:var(--acc);color:#fff;border:none;border-radius:8px;"
              "padding:5px 12px;font-size:12px;cursor:pointer;white-space:nowrap;flex-shrink:0}\n"
            "#lav-cv-btn{background:rgba(45,212,191,.15);color:#2dd4bf;border:1px solid #2dd4bf;"
              "border-radius:8px;padding:5px 10px;font-size:12px;cursor:pointer;flex-shrink:0}\n"
            "#lav-list{flex:1;overflow-y:auto;min-height:0}\n"
            ".lav-item{padding:8px 12px;border-bottom:1px solid var(--brd);cursor:pointer;"
              "transition:background .15s}\n"
            ".lav-item:hover{background:rgba(255,255,255,.05)}\n"
            ".lav-item.sel{background:rgba(45,212,191,.12);border-left:3px solid #2dd4bf}\n"
            ".lav-row1{display:flex;gap:6px;align-items:baseline;flex-wrap:wrap}\n"
            ".lav-az{font-weight:600;font-size:13px;color:var(--txt)}\n"
            ".lav-ruolo{font-size:12px;color:var(--dim)}\n"
            ".lav-sede{font-size:11px;color:var(--dim);flex:1;text-align:right}\n"
            ".lav-req{font-size:11px;color:var(--dim);margin-top:2px;"
              "white-space:nowrap;overflow:hidden;text-overflow:ellipsis}\n"
            "#lav-empty{padding:18px;text-align:center;color:var(--dim);font-size:13px}\n"
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
            "#tab-rag{flex:1;min-height:0;overflow-y:auto;padding:16px;"
              "display:none;box-sizing:border-box}\n"
            ".rag-card{background:var(--aim);border:1px solid var(--brd);border-radius:10px;"
              "padding:12px 14px;margin-bottom:10px}\n"
            ".rag-card p{font-size:14px;line-height:1.55;white-space:pre-wrap;color:var(--txt);"
              "margin-bottom:6px}\n"
            ".rag-card small{font-size:11px;color:var(--dim)}\n"
            "#rag-query{width:100%;background:var(--inp);border:1px solid var(--brd);"
              "color:var(--txt);border-radius:8px;padding:10px;font-size:14px;"
              "resize:vertical;box-sizing:border-box;margin-bottom:10px}\n"
            "#rag-query:focus{outline:none;border-color:var(--acc)}\n"
            "#rag-srch{background:var(--acc);color:#fff;border:none;border-radius:8px;"
              "padding:10px 20px;font-size:14px;font-weight:700;cursor:pointer;width:100%;"
              "margin-bottom:12px}\n"
            "#rag-srch:hover{filter:brightness(1.1)}\n"
            /* ── Sessioni sidebar CSS ── */
            "#ses-list{display:flex;flex-direction:column;gap:4px;max-height:220px;"
              "overflow-y:auto;margin-bottom:8px}\n"
            ".ses-item{display:flex;align-items:center;gap:6px;padding:7px 10px;"
              "background:var(--aim);border:1px solid var(--brd);border-radius:8px;"
              "cursor:pointer;transition:background .15s}\n"
            ".ses-item:hover{background:rgba(255,255,255,.07)}\n"
            ".ses-item.active{border-color:var(--acc);background:rgba(108,99,255,.12)}\n"
            ".ses-name{flex:1;font-size:13px;overflow:hidden;text-overflow:ellipsis;"
              "white-space:nowrap;color:var(--txt)}\n"
            ".ses-del{background:transparent;border:none;color:var(--dim);"
              "cursor:pointer;padding:2px 5px;font-size:13px;border-radius:4px;flex-shrink:0}\n"
            ".ses-del:hover{background:rgba(255,80,80,.15);color:#ff5555}\n"
            "#ses-act{display:flex;gap:6px;flex-wrap:wrap}\n"
            ".ses-btn{flex:1;padding:7px 8px;background:var(--aim);"
              "border:1px solid var(--brd);color:var(--txt);border-radius:8px;"
              "font-size:12px;cursor:pointer;text-align:center;white-space:nowrap}\n"
            ".ses-btn:hover{background:rgba(255,255,255,.07);border-color:var(--acc)}\n"
            "#ses-empty{font-size:12px;color:var(--dim);padding:8px 0;text-align:center}\n"
            /* ── Matematica sub-tab CSS ── */
            ".mat-tab{background:none;border:none;padding:8px 14px;font-size:13px;"
              "color:var(--dim);cursor:pointer;border-bottom:2px solid transparent}\n"
            ".mat-tab.on{color:var(--acc);border-bottom-color:var(--acc);font-weight:600}\n"
            ".mtp{display:flex}\n"
            ".mat-run{background:var(--aim);border:1px solid var(--brd);color:var(--txt);"
              "border-radius:8px;padding:8px 14px;font-size:13px;cursor:pointer}\n"
            ".mat-run:hover{background:var(--brd)}\n"
            "#tab-cod{flex:1;min-height:0;overflow-y:auto;padding:16px;display:none}\n"
            "#tab-gvz{flex:1;min-height:0;overflow-y:auto;padding:16px;display:none}\n"
            "#tab-wsp{flex:1;min-height:0;overflow-y:auto;padding:16px;display:none}\n"
            ".tab-hd{font-size:18px;font-weight:700;margin-bottom:12px;color:var(--acc)}\n"
            "</style>\n"
            "<link rel=\"stylesheet\" href=\"/katex/katex.min.css\">\n"
            "<script defer src=\"/katex/katex.min.js\"></script>\n"
            "</head>\n<body>\n"
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
                  "<optgroup label=\"\xe2\x94\x80 Dark\">"
                    "<option value=\"dark\">&#x1F311; Dark</option>"
                    "<option value=\"dark_classic\">\xf0\x9f\x94\xb5 Classic Blue</option>"
                    "<option value=\"dark_amber\">\xf0\x9f\x9f\xa1 Amber</option>"
                    "<option value=\"dark_cyan\">\xf0\x9f\x94\xb7 Cyan</option>"
                    "<option value=\"dark_green\">\xf0\x9f\x9f\xa2 Green</option>"
                    "<option value=\"dark_lavender\">\xf0\x9f\x92\x9c Lavender</option>"
                    "<option value=\"dark_ocean\">\xf0\x9f\x8c\x8a Ocean</option>"
                    "<option value=\"dark_purple\">\xf0\x9f\x9f\xa3 Purple</option>"
                    "<option value=\"dark_sunset\">\xf0\x9f\x8c\x85 Sunset</option>"
                    "<option value=\"dark_rainbow\">\xf0\x9f\x8c\x88 Rainbow</option>"
                  "</optgroup>"
                  "<optgroup label=\"\xe2\x94\x80 Speciali\">"
                    "<option value=\"dracula\">\xf0\x9f\xa7\x9b Dracula</option>"
                    "<option value=\"nord\">\xe2\x9d\x84 Nord</option>"
                    "<option value=\"hacker\">\xf0\x9f\x92\x80 Hacker</option>"
                    "<option value=\"military\">\xf0\x9f\xaa\x96 Military</option>"
                    "<option value=\"neon\">\xe2\x9c\xa8 Neon</option>"
                    "<option value=\"pink\">\xf0\x9f\x8c\xb8 Pink</option>"
                    "<option value=\"solar\">\xe2\x98\x80 Solar</option>"
                  "</optgroup>"
                  "<optgroup label=\"\xe2\x94\x80 Venom\">"
                    "<option value=\"venom_blue\">\xe2\x9a\xa1 Venom Blue</option>"
                    "<option value=\"venom_red\">\xf0\x9f\x94\xb4 Venom Red</option>"
                    "<option value=\"venom_orange\">\xf0\x9f\x9f\xa0 Venom Orange</option>"
                    "<option value=\"venom_green\">\xf0\x9f\x9f\xa9 Venom Green</option>"
                  "</optgroup>"
                  "<optgroup label=\"\xe2\x94\x80 Light\">"
                    "<option value=\"light\">\xe2\x98\x80 Chiaro</option>"
                    "<option value=\"light_mint\">\xf0\x9f\x8c\xbf Mint</option>"
                    "<option value=\"light_rose\">\xf0\x9f\x8c\xb9 Rose</option>"
                    "<option value=\"light_sky\">\xf0\x9f\x8c\xa4 Sky</option>"
                    "<option value=\"light_sand\">\xf0\x9f\x8f\x96 Sand</option>"
                  "</optgroup>"
                "</select>"
              "</div>"
              "<div class=\"dsec\">"
                "<h3>&#128190; Sessioni Chat</h3>"
                "<div id=\"ses-list\"></div>"
                "<div id=\"ses-act\">"
                  "<button class=\"ses-btn\" id=\"ses-new\">&#x1F5D1; Nuova chat</button>"
                  "<button class=\"ses-btn\" id=\"ses-save\">" "\xe2\x9c\x8f" " Salva</button>"
                  "<button class=\"ses-btn\" id=\"ses-exp\">" "\xe2\xac\x87" " Esporta</button>"
                "</div>"
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
              "<button class=\"tab\" data-tab=\"mat\">\xcf\x80 Matematica</button>"
              "<button class=\"tab\" data-tab=\"age\">\xf0\x9f\xa4\x96 Agenti</button>"
              "<button class=\"tab\" data-tab=\"imp\">\xf0\x9f\x93\x9a Impara</button>"
              "<button class=\"tab\" data-tab=\"mul\">\xf0\x9f\x8e\xac Multimedia</button>"
              "<button class=\"tab\" data-tab=\"rag\">\xf0\x9f\x93\x9a RAG</button>"
              "<button class=\"tab\" data-tab=\"cod\">\xf0\x9f\x92\xbb Coding</button>"
              "<button class=\"tab\" data-tab=\"gvz\">\xf0\x9f\x97\xba Graphviz</button>"
              "<button class=\"tab\" data-tab=\"wsp\">\xf0\x9f\x8e\x99 Voce</button>"
            "</div>\n"
            "<div id=\"tab-chat\">"
              "<div id=\"tool-ind\"></div>"
              "<div id=\"lavoro-panel\">"
                "<div id=\"lav-filters\">"
                  "<select id=\"lav-tipo\">"
                    "<option value=\"tutti\">Tutti i settori</option>"
                    "<option value=\"IT\">IT / Informatica</option>"
                    "<option value=\"Retail\">Retail</option>"
                    "<option value=\"Ristorazione\">Ristorazione</option>"
                    "<option value=\"Edilizia\">Edilizia</option>"
                    "<option value=\"Logistica\">Logistica</option>"
                    "<option value=\"Finanza\">Finanza</option>"
                    "<option value=\"Sanitario\">Sanitario</option>"
                    "<option value=\"Produzione\">Produzione</option>"
                    "<option value=\"Tecnico\">Tecnico</option>"
                    "<option value=\"Turismo\">Turismo</option>"
                    "<option value=\"Admin\">Admin</option>"
                    "<option value=\"Commerciale\">Commerciale</option>"
                    "<option value=\"Altro\">Altro</option>"
                  "</select>"
                  "<select id=\"lav-liv\">"
                    "<option value=\"tutti\">Qualsiasi livello</option>"
                    "<option value=\"media\">Media</option>"
                    "<option value=\"diploma\" selected>Diploma</option>"
                    "<option value=\"laurea_t\">Laurea triennale</option>"
                    "<option value=\"laurea_m\">Laurea magistrale</option>"
                  "</select>"
                  "<button id=\"lav-search\">&#x1F50D; Cerca</button>"
                  "<button id=\"lav-cv-btn\">&#x1F4C4; CV</button>"
                "</div>"
                "<div id=\"lav-list\"><div id=\"lav-empty\">Premi Cerca per caricare le offerte</div></div>"
              "</div>"
              "<div id=\"log\"></div>"
              "<div id=\"bar\">"
                "<input type=\"file\" id=\"att-in\" style=\"display:none\">"
                "<div id=\"bar-mid\">"
                  "<div id=\"att-tag\">"
                    "<span id=\"att-name\"></span>"
                    "<button id=\"att-clr\">&#10005;</button>"
                  "</div>"
                  "<div id=\"tool-extra\"></div>"
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
            /* ── Tab Matematica ── */
            "<div id=\"tab-mat\">"
              "<div class=\"sec-hd\">\xcf\x80 Matematica</div>"
              /* Sub-tab bar */
              "<div id=\"mat-tabs\" style=\"display:flex;gap:0;border-bottom:1px solid var(--brd);padding:0 12px\">"
                "<button class=\"mat-tab on\" data-mt=\"seq\">\xf0\x9f\x94\xa2 Sequenza</button>"
                "<button class=\"mat-tab\" data-mt=\"const\">\xcf\x80 Costanti</button>"
                "<button class=\"mat-tab\" data-mt=\"nth\">#\xe2\x82\x99 N-esimo</button>"
                "<button class=\"mat-tab\" data-mt=\"expr\">\xf0\x9f\xa7\xae Espressione</button>"
              "</div>"
              /* Sub-tab: Sequenza */
              "<div id=\"mtp-seq\" class=\"mtp\" style=\"padding:12px;flex-direction:column;gap:10px\">"
                "<label style=\"font-size:13px;color:var(--dim)\">Sequenza (es. 1, 4, 9, 16, 25)</label>"
                "<input id=\"mat-seq\" type=\"text\""
                  " style=\"background:var(--inp);border:1px solid var(--brd);color:var(--txt);"
                  "border-radius:8px;padding:8px;font-size:13px\""
                  " placeholder=\"es. 1, 1, 2, 3, 5, 8, 13\">"
                "<div style=\"display:flex;gap:8px;align-items:center\">"
                  "<span style=\"font-size:13px;color:var(--dim)\">Prossimi</span>"
                  "<input id=\"mat-nxt\" type=\"number\" value=\"5\" min=\"1\" max=\"20\""
                    " style=\"width:60px;background:var(--inp);border:1px solid var(--brd);"
                    "color:var(--txt);border-radius:8px;padding:6px;font-size:13px\">"
                  "<span style=\"font-size:13px;color:var(--dim)\">termini</span>"
                "</div>"
                "<div id=\"mat-seq-res\" style=\"font-size:13px;color:var(--acc);min-height:20px\"></div>"
                "<div style=\"display:flex;gap:8px;flex-wrap:wrap\">"
                  "<button id=\"mat-local\" class=\"mat-run\">\xf0\x9f\x94\x8d Rileva (locale)</button>"
                  "<button id=\"mat-sympy\" class=\"mat-run\">\xcf\x83 Interpola sympy</button>"
                  "<button id=\"mat-ai\" class=\"mat-run\" style=\"background:var(--acc);color:#fff\">"
                    "\xf0\x9f\xa4\x96 Analizza AI</button>"
                "</div>"
              "</div>"
              /* Sub-tab: Costanti */
              "<div id=\"mtp-const\" class=\"mtp\" style=\"padding:12px;display:none;flex-direction:column;gap:10px\">"
                "<label style=\"font-size:13px;color:var(--dim)\">Costante</label>"
                "<select id=\"mat-const\""
                  " style=\"background:var(--inp);border:1px solid var(--brd);color:var(--txt);"
                  "border-radius:8px;padding:8px;font-size:13px\">"
                  "<option value=\"pi\">\xcf\x80  pi greco</option>"
                  "<option value=\"e\">e  numero di Eulero</option>"
                  "<option value=\"phi\">\xcf\x86  sezione aurea</option>"
                  "<option value=\"sqrt2\">\xe2\x88\x9a" "2  radice di 2</option>"
                  "<option value=\"sqrt3\">\xe2\x88\x9a" "3  radice di 3</option>"
                  "<option value=\"sqrt5\">\xe2\x88\x9a" "5  radice di 5</option>"
                  "<option value=\"euler_gamma\">\xce\xb3  Eulero-Mascheroni</option>"
                  "<option value=\"ln2\">ln(2)  logaritmo naturale di 2</option>"
                  "<option value=\"catalan\">C  costante di Catalan</option>"
                "</select>"
                "<div style=\"display:flex;gap:8px;align-items:center\">"
                  "<label style=\"font-size:13px;color:var(--dim)\">Cifre decimali:</label>"
                  "<input id=\"mat-prec\" type=\"number\" value=\"100\" min=\"10\" max=\"100000\""
                    " style=\"width:100px;background:var(--inp);border:1px solid var(--brd);"
                    "color:var(--txt);border-radius:8px;padding:6px;font-size:13px\">"
                "</div>"
                "<div style=\"display:flex;gap:8px\">"
                  "<button id=\"mat-calc-const\" class=\"mat-run\""
                    " style=\"flex:1;background:var(--acc);color:#fff\">\xcf\x80 Calcola</button>"
                  "<button id=\"mat-all-const\" class=\"mat-run\" style=\"flex:1\">Tutte (100 cifre)</button>"
                "</div>"
              "</div>"
              /* Sub-tab: N-esimo */
              "<div id=\"mtp-nth\" class=\"mtp\" style=\"padding:12px;display:none;flex-direction:column;gap:10px\">"
                "<label style=\"font-size:13px;color:var(--dim)\">Tipo</label>"
                "<select id=\"mat-ntype\""
                  " style=\"background:var(--inp);border:1px solid var(--brd);color:var(--txt);"
                  "border-radius:8px;padding:8px;font-size:13px\">"
                  "<option value=\"pi_digit\">\xcf\x80  N-esima cifra di \xcf\x80</option>"
                  "<option value=\"e_digit\">e  N-esima cifra di e</option>"
                  "<option value=\"prime\">p  N-esimo numero primo</option>"
                  "<option value=\"fib\">F  N-esimo Fibonacci</option>"
                  "<option value=\"fact\">n!  N-esimo fattoriale</option>"
                  "<option value=\"pow2\">2\xe1\xb5\x8f  N-esima potenza di 2</option>"
                  "<option value=\"pi_block\">\xcf\x80\xe1\xb5\x8f  Prime N cifre di \xcf\x80</option>"
                  "<option value=\"phi_block\">\xcf\x86\xe1\xb5\x8f  Prime N cifre di \xcf\x86</option>"
                "</select>"
                "<div id=\"mat-nth-desc\" style=\"font-size:12px;color:var(--dim);min-height:16px\"></div>"
                "<div style=\"display:flex;gap:8px;align-items:center\">"
                  "<label style=\"font-size:13px;color:var(--dim)\">N =</label>"
                  "<input id=\"mat-n\" type=\"text\" value=\"100\" placeholder=\"es. 1000000\""
                    " style=\"flex:1;background:var(--inp);border:1px solid var(--brd);"
                    "color:var(--txt);border-radius:8px;padding:8px;font-size:13px\">"
                "</div>"
                "<button id=\"mat-calc-nth\" class=\"mat-run\""
                  " style=\"background:var(--acc);color:#fff\">#\xe2\x82\x99 Calcola</button>"
              "</div>"
              /* Sub-tab: Espressione */
              "<div id=\"mtp-expr\" class=\"mtp\" style=\"padding:12px;display:none;flex-direction:column;gap:10px\">"
                "<label style=\"font-size:13px;color:var(--dim)\">Espressione (sympy + mpmath)</label>"
                "<input id=\"mat-expr\" type=\"text\""
                  " style=\"background:var(--inp);border:1px solid var(--brd);color:var(--txt);"
                  "border-radius:8px;padding:8px;font-size:13px\""
                  " placeholder=\"es. sqrt(2)+sin(pi/4) o integrate(x**2,(x,0,1))\">"
                "<div style=\"display:flex;gap:8px;align-items:center\">"
                  "<label style=\"font-size:13px;color:var(--dim)\">Precisione:</label>"
                  "<input id=\"mat-eprec\" type=\"number\" value=\"50\" min=\"10\" max=\"10000\""
                    " style=\"width:80px;background:var(--inp);border:1px solid var(--brd);"
                    "color:var(--txt);border-radius:8px;padding:6px;font-size:13px\">"
                  "<span style=\"font-size:12px;color:var(--dim)\">cifre</span>"
                "</div>"
                "<div style=\"display:flex;flex-wrap:wrap;gap:4px\">"
                  "<button class=\"mat-ex mat-run\" data-e=\"pi**2/6\">\xcf\x80\xc2\xb2/6</button>"
                  "<button class=\"mat-ex mat-run\" data-e=\"exp(pi)-pi**exp(1)\">e\xcf\x80\xe2\x88\x92\xcf\x80" "e</button>"
                  "<button class=\"mat-ex mat-run\" data-e=\"integrate(x**2,(x,0,1))\">\xe2\x88\xabx\xc2\xb2" "dx</button>"
                  "<button class=\"mat-ex mat-run\" data-e=\"sqrt(2+sqrt(3))\">\xe2\x88\x9a(2+\xe2\x88\x9a" "3)</button>"
                  "<button class=\"mat-ex mat-run\" data-e=\"factorial(1000)\">1000!</button>"
                  "<button class=\"mat-ex mat-run\" data-e=\"gcd(144,180)\">mcd(144,180)</button>"
                  "<button class=\"mat-ex mat-run\" data-e=\"log(factorial(100)).evalf()\">Stirling 100</button>"
                  "<button class=\"mat-ex mat-run\" data-e=\"phi**20\">\xcf\x86\xc2\xb2\xc2\xb0</button>"
                "</div>"
                "<div style=\"display:flex;gap:8px\">"
                  "<button id=\"mat-eval\" class=\"mat-run\""
                    " style=\"flex:1;background:var(--acc);color:#fff\">\xf0\x9f\xa7\xae Calcola</button>"
                  "<button id=\"mat-simp\" class=\"mat-run\" style=\"flex:1\">\xe2\x99\xbe Semplifica</button>"
                "</div>"
              "</div>"
              /* Output comune + stato */
              "<div style=\"padding:0 12px 12px\">"
                "<div style=\"display:flex;gap:8px;align-items:center;margin-bottom:4px\">"
                  "<span id=\"mat-status\" style=\"flex:1;font-size:12px;color:var(--dim)\">Pronto.</span>"
                  "<button id=\"mat-copy\" class=\"mat-run\" style=\"padding:4px 10px;font-size:12px\">"
                    "\xf0\x9f\x93\x8b Copia</button>"
                  "<button id=\"mat-clr\" class=\"mat-run\" style=\"padding:4px 10px;font-size:12px\">"
                    "\xf0\x9f\x97\x91 Cancella</button>"
                "</div>"
                "<div id=\"mat-out\" style=\"background:var(--aim);border:1px solid var(--brd);"
                  "border-radius:8px;padding:12px;min-height:80px;max-height:300px;overflow-y:auto;"
                  "font-family:monospace;font-size:13px;white-space:pre-wrap;line-height:1.5\"></div>"
              "</div>"
              /* Grafico */
              "<hr style=\"border-color:var(--brd);margin:0 12px\">"
              "<div style=\"padding:12px;display:flex;flex-direction:column;gap:8px\">"
                "<label style=\"font-size:13px;color:var(--dim)\">Grafico f(x) (JS Math)</label>"
                "<div style=\"display:flex;gap:8px\">"
                  "<input id=\"plot-fn\" type=\"text\" value=\"Math.sin(x)\" style=\"flex:1;"
                    "background:var(--inp);border:1px solid var(--brd);color:var(--txt);"
                    "border-radius:8px;padding:8px;font-size:13px\">"
                  "<button id=\"plot-btn\" class=\"mat-run\">Traccia</button>"
                "</div>"
                "<canvas id=\"plot-cv\" height=\"200\" style=\"width:100%;background:var(--aim);"
                  "border:1px solid var(--brd);border-radius:8px;display:none\"></canvas>"
              "</div>"
            "</div>\n"
            /* ── Tab Agenti ── */
            "<div id=\"tab-age\">"
              "<div class=\"sec-hd\">\xf0\x9f\xa4\x96 Pipeline Multi-Agente</div>"
              "<div style=\"padding:12px;display:flex;flex-direction:column;gap:10px\">"
                "<label style=\"font-size:13px;color:var(--dim)\">Task da eseguire</label>"
                "<textarea id=\"age-task\" rows=\"3\" style=\"background:var(--inp);"
                  "border:1px solid var(--brd);color:var(--txt);border-radius:8px;padding:8px;"
                  "font-size:13px;resize:vertical;box-sizing:border-box\""
                  " placeholder=\"Es: Scrivi un riassunto su x e revisiona il risultato\"></textarea>"
                "<div style=\"display:flex;gap:8px;flex-wrap:wrap\">"
                  "<div style=\"flex:1;min-width:140px\">"
                    "<label style=\"font-size:12px;color:var(--dim)\">Agente 1</label>"
                    "<select id=\"age-r1\" style=\"width:100%;background:var(--inp);border:1px solid var(--brd);"
                      "color:var(--txt);border-radius:8px;padding:6px;font-size:13px\">"
                      "<option>Ricercatore</option><option>Scrittore</option>"
                      "<option>Revisore</option><option>Programmatore</option><option>Analista</option>"
                    "</select>"
                  "</div>"
                  "<div style=\"flex:1;min-width:140px\">"
                    "<label style=\"font-size:12px;color:var(--dim)\">Agente 2</label>"
                    "<select id=\"age-r2\" style=\"width:100%;background:var(--inp);border:1px solid var(--brd);"
                      "color:var(--txt);border-radius:8px;padding:6px;font-size:13px\">"
                      "<option>Scrittore</option><option>Ricercatore</option>"
                      "<option>Revisore</option><option>Programmatore</option><option>Analista</option>"
                    "</select>"
                  "</div>"
                  "<div style=\"flex:1;min-width:140px\">"
                    "<label style=\"font-size:12px;color:var(--dim)\">Agente 3</label>"
                    "<select id=\"age-r3\" style=\"width:100%;background:var(--inp);border:1px solid var(--brd);"
                      "color:var(--txt);border-radius:8px;padding:6px;font-size:13px\">"
                      "<option>Revisore</option><option>Ricercatore</option>"
                      "<option>Scrittore</option><option>Programmatore</option><option>Analista</option>"
                    "</select>"
                  "</div>"
                "</div>"
                "<button id=\"age-run\" style=\"background:var(--acc);color:#fff;border:none;"
                  "border-radius:8px;padding:12px;font-size:15px;cursor:pointer;font-weight:600\">"
                  "\xe2\x96\xb6 Avvia Pipeline</button>"
                "<div id=\"age-log\" style=\"display:flex;flex-direction:column;gap:8px\"></div>"
              "</div>"
            "</div>\n"
            /* ── Tab Impara ── */
            "<div id=\"tab-imp\">"
              "<div class=\"sec-hd\">\xf0\x9f\x93\x9a Impara con AI</div>"
              "<div style=\"padding:12px;display:flex;flex-direction:column;gap:10px\">"
                "<div style=\"display:flex;gap:8px;flex-wrap:wrap\">"
                  "<select id=\"imp-arg\" style=\"flex:2;background:var(--inp);border:1px solid var(--brd);"
                    "color:var(--txt);border-radius:8px;padding:8px;font-size:13px\">"
                    "<option value=\"\">-- Scegli argomento --</option>"
                    "<option>Matematica</option><option>Fisica</option><option>Programmazione</option>"
                    "<option>Storia</option><option>Inglese</option><option>Economia</option>"
                    "<option>Chimica</option><option>Biologia</option>"
                  "</select>"
                  "<select id=\"imp-lv\" style=\"flex:1;background:var(--inp);border:1px solid var(--brd);"
                    "color:var(--txt);border-radius:8px;padding:8px;font-size:13px\">"
                    "<option>Principiante</option><option>Intermedio</option><option>Avanzato</option>"
                  "</select>"
                "</div>"
                "<input id=\"imp-custom\" type=\"text\" placeholder=\"Oppure scrivi un argomento libero...\""
                  " style=\"background:var(--inp);border:1px solid var(--brd);color:var(--txt);"
                  "border-radius:8px;padding:8px;font-size:13px\">"
                "<div style=\"display:flex;gap:8px\">"
                  "<button id=\"imp-spiega\" style=\"flex:1;background:var(--acc);color:#fff;border:none;"
                    "border-radius:8px;padding:10px;font-size:14px;cursor:pointer\">"
                    "\xf0\x9f\x93\x96 Spiega</button>"
                  "<button id=\"imp-quiz\" style=\"flex:1;background:var(--aim);border:1px solid var(--brd);"
                    "color:var(--txt);border-radius:8px;padding:10px;font-size:14px;cursor:pointer\">"
                    "\xe2\x9d\x93 Quiz</button>"
                  "<button id=\"imp-esem\" style=\"flex:1;background:var(--aim);border:1px solid var(--brd);"
                    "color:var(--txt);border-radius:8px;padding:10px;font-size:14px;cursor:pointer\">"
                    "\xf0\x9f\x92\xa1 Esempi</button>"
                "</div>"
                "<div id=\"imp-out\" style=\"background:var(--aim);border:1px solid var(--brd);"
                  "border-radius:8px;padding:12px;min-height:60px;font-size:14px;line-height:1.6;"
                  "white-space:pre-wrap;overflow-y:auto;max-height:300px\"></div>"
              "</div>"
            "</div>\n"
            /* ── Tab Multimedia ── */
            "<div id=\"tab-mul\">"
              "<div class=\"sec-hd\">\xf0\x9f\x8e\xac Multimedia \xe2\x80\x94 Sintetizzatore Toni</div>"
              "<div style=\"padding:12px;display:flex;flex-direction:column;gap:14px\">"
                /* Card 1: Programmatore */
                "<div style=\"background:var(--aim);border:1px solid var(--brd);"
                  "border-radius:10px;padding:12px\">"
                  "<div style=\"font-size:11px;color:var(--dim);margin-bottom:10px;"
                    "text-transform:uppercase;letter-spacing:.5px\">"
                    "\xf0\x9f\x8e\xb5 Programmatore Toni</div>"
                  "<div style=\"display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-bottom:8px\">"
                    "<div>"
                      "<label style=\"font-size:11px;color:var(--dim);display:block;margin-bottom:3px\">"
                        "Frequenza (Hz)</label>"
                      "<input id=\"mul-freq\" type=\"number\" value=\"440\" min=\"20\" max=\"20000\""
                        " style=\"width:100%;box-sizing:border-box;background:var(--inp);"
                        "border:1px solid var(--brd);color:var(--txt);border-radius:8px;"
                        "padding:7px;font-size:13px\">"
                    "</div>"
                    "<div>"
                      "<label style=\"font-size:11px;color:var(--dim);display:block;margin-bottom:3px\">"
                        "Durata (ms)</label>"
                      "<input id=\"mul-dur\" type=\"number\" value=\"500\" min=\"50\" max=\"5000\""
                        " style=\"width:100%;box-sizing:border-box;background:var(--inp);"
                        "border:1px solid var(--brd);color:var(--txt);border-radius:8px;"
                        "padding:7px;font-size:13px\">"
                    "</div>"
                    "<div>"
                      "<label style=\"font-size:11px;color:var(--dim);display:block;margin-bottom:3px\">"
                        "Forma d\xe2\x80\x99onda</label>"
                      "<select id=\"mul-wave\" style=\"width:100%;background:var(--inp);"
                        "border:1px solid var(--brd);color:var(--txt);border-radius:8px;"
                        "padding:7px;font-size:13px\">"
                        "<option value=\"sine\">Sinusoidale</option>"
                        "<option value=\"square\">Quadra</option>"
                        "<option value=\"triangle\">Triangolare</option>"
                        "<option value=\"sawtooth\">Dente di sega</option>"
                      "</select>"
                    "</div>"
                    "<div>"
                      "<label style=\"font-size:11px;color:var(--dim);display:block;margin-bottom:3px\">"
                        "Volume (<span id=\"mul-vol-lbl\">70</span>%)</label>"
                      "<input id=\"mul-vol\" type=\"range\" min=\"0\" max=\"100\" value=\"70\""
                        " style=\"width:100%;accent-color:var(--acc)\">"
                    "</div>"
                  "</div>"
                  "<div style=\"display:flex;gap:8px;margin-bottom:10px\">"
                    "<button id=\"mul-add\" style=\"flex:1;background:var(--acc);color:#fff;"
                      "border:none;border-radius:8px;padding:8px 12px;font-size:13px;"
                      "cursor:pointer;font-weight:600\">+ Aggiungi</button>"
                    "<button id=\"mul-prev\" style=\"flex:1;background:var(--aim);"
                      "border:1px solid var(--brd);color:var(--txt);border-radius:8px;"
                      "padding:8px 12px;font-size:13px;cursor:pointer\">"
                      "\xf0\x9f\x94\x8a Ascolta</button>"
                    "<button id=\"mul-clr\" style=\"background:var(--aim);"
                      "border:1px solid var(--brd);color:var(--txt);border-radius:8px;"
                      "padding:8px 12px;font-size:13px;cursor:pointer\">"
                      "\xf0\x9f\x97\x91</button>"
                  "</div>"
                  "<div id=\"mul-seq\" style=\"display:flex;flex-wrap:wrap;gap:6px;"
                    "min-height:28px\"></div>"
                "</div>"
                /* Card 2: Assemblatore Visuale */
                "<div style=\"background:var(--aim);border:1px solid var(--brd);"
                  "border-radius:10px;padding:12px\">"
                  "<div style=\"font-size:11px;color:var(--dim);margin-bottom:10px;"
                    "text-transform:uppercase;letter-spacing:.5px\">"
                    "\xf0\x9f\x93\x8a Assemblatore Visuale</div>"
                  "<div style=\"font-size:11px;color:var(--dim);margin-bottom:4px\">"
                    "Oscilloscopio</div>"
                  "<canvas id=\"mul-osc\" height=\"90\""
                    " style=\"width:100%;display:block;border-radius:6px;"
                    "background:var(--inp);border:1px solid var(--brd);margin-bottom:8px\">"
                  "</canvas>"
                  "<div style=\"font-size:11px;color:var(--dim);margin-bottom:4px\">"
                    "Spettro frequenze</div>"
                  "<canvas id=\"mul-fq\" height=\"70\""
                    " style=\"width:100%;display:block;border-radius:6px;"
                    "background:var(--inp);border:1px solid var(--brd);margin-bottom:10px\">"
                  "</canvas>"
                  "<div style=\"display:flex;align-items:center;gap:8px\">"
                    "<button id=\"mul-play\" style=\"flex:1;background:var(--acc);color:#fff;"
                      "border:none;border-radius:8px;padding:10px;font-size:14px;"
                      "cursor:pointer;font-weight:600\">\xe2\x96\xb6 Riproduci</button>"
                    "<button id=\"mul-stop\" style=\"flex:1;background:var(--aim);"
                      "border:1px solid var(--brd);color:var(--txt);border-radius:8px;"
                      "padding:10px;font-size:14px;cursor:pointer\">\xe2\x8f\xb9 Stop</button>"
                    "<span id=\"mul-status\" style=\"font-size:12px;color:var(--dim)\">"
                      "Aggiungi toni e premi Riproduci</span>"
                  "</div>"
                "</div>"
              "</div>"
            "</div>\n"
            /* ── Tab RAG ── */
            "<div id=\"tab-rag\">"
              "<div class=\"tab-hd\">\xf0\x9f\x93\x9a RAG \xe2\x80\x94 Ricerca Semantica</div>"
              "<div style=\"padding:12px;display:flex;flex-direction:column;gap:0\">"
                "<label style=\"font-size:13px;color:var(--dim);margin-bottom:6px;display:block\">"
                  "Fai una domanda — l'AI cerca i chunk pi\xc3\xb9 rilevanti nel tuo indice RAG</label>"
                "<textarea id=\"rag-query\" rows=\"3\""
                  " placeholder=\"Es: Come funziona il protocollo TCP?\"></textarea>"
                "<button id=\"rag-srch\">\xf0\x9f\x94\x8d Cerca nel RAG</button>"
                "<div id=\"rag-results\"></div>"
              "</div>"
            "</div>\n"
            /* ── Tab Coding ── */
            "<div id=\"tab-cod\">"
              "<div class=\"tab-hd\">\xf0\x9f\x92\xbb Coding \xe2\x80\x94 Editor + AI</div>"
              "<div style=\"display:flex;gap:8px;margin-bottom:8px;flex-wrap:wrap\">"
                "<select id=\"cod-lang\" style=\"background:var(--inp);color:var(--txt);border:1px solid var(--brd);border-radius:6px;padding:4px 8px;font-size:13px\">"
                  "<option value=\"python\">Python</option>"
                  "<option value=\"javascript\">JavaScript</option>"
                  "<option value=\"cpp\">C++</option>"
                  "<option value=\"bash\">Bash</option>"
                  "<option value=\"sql\">SQL</option>"
                "</select>"
                "<button id=\"cod-run\" style=\"background:var(--acc);color:#fff;border:none;border-radius:8px;padding:6px 14px;cursor:pointer;font-size:13px\">\xf0\x9f\xa4\x96 Analizza con AI</button>"
                "<button id=\"cod-clr\" style=\"background:var(--aim);color:var(--txt);border:1px solid var(--brd);border-radius:8px;padding:6px 14px;cursor:pointer;font-size:13px\">\xf0\x9f\x97\x91 Pulisci</button>"
              "</div>"
              "<textarea id=\"cod-editor\" spellcheck=\"false\" style=\"width:100%;height:220px;background:var(--inp);color:var(--txt);border:1px solid var(--brd);border-radius:8px;padding:10px;font-family:monospace;font-size:13px;resize:vertical;box-sizing:border-box\" placeholder=\"Incolla qui il tuo codice...\"></textarea>"
              "<div id=\"cod-out\" style=\"margin-top:10px;background:var(--aim);border:1px solid var(--brd);border-radius:8px;padding:10px;min-height:60px;font-family:monospace;font-size:13px;white-space:pre-wrap;color:var(--txt)\"></div>"
            "</div>\n"
            /* ── Tab Graphviz ── */
            "<div id=\"tab-gvz\">"
              "<div class=\"tab-hd\">\xf0\x9f\x97\xba Graphviz \xe2\x80\x94 Mappe Concettuali</div>"
              "<div style=\"display:flex;gap:8px;margin-bottom:8px;flex-wrap:wrap\">"
                "<button id=\"gvz-ai\" style=\"background:var(--acc);color:#fff;border:none;border-radius:8px;padding:6px 14px;cursor:pointer;font-size:13px\">\xf0\x9f\xa4\x96 Genera DOT con AI</button>"
                "<button id=\"gvz-run\" style=\"background:#059669;color:#fff;border:none;border-radius:8px;padding:6px 14px;cursor:pointer;font-size:13px\">\xf0\x9f\x96\xbc Renderizza</button>"
                "<button id=\"gvz-clr\" style=\"background:var(--aim);color:var(--txt);border:1px solid var(--brd);border-radius:8px;padding:6px 14px;cursor:pointer;font-size:13px\">\xf0\x9f\x97\x91 Pulisci</button>"
              "</div>"
              "<textarea id=\"gvz-desc\" style=\"width:100%;height:60px;background:var(--inp);color:var(--txt);border:1px solid var(--brd);border-radius:8px;padding:8px;font-size:13px;resize:vertical;box-sizing:border-box;margin-bottom:6px\" placeholder=\"Descrivi il grafo in linguaggio naturale (per AI)...\"></textarea>"
              "<textarea id=\"gvz-dot\" spellcheck=\"false\" style=\"width:100%;height:160px;background:var(--inp);color:var(--txt);border:1px solid var(--brd);border-radius:8px;padding:10px;font-family:monospace;font-size:13px;resize:vertical;box-sizing:border-box\" placeholder=\"digraph G {\\n  A -&gt; B -&gt; C\\n}\"></textarea>"
              "<div id=\"gvz-img\" style=\"margin-top:10px;text-align:center;min-height:60px\"></div>"
              "<div id=\"gvz-err\" style=\"color:#f87171;font-size:13px;margin-top:6px\"></div>"
            "</div>\n"
            /* ── Tab Whisper ── */
            "<div id=\"tab-wsp\">"

              /* ── 1. TTS ── */
              "<div class=\"tab-hd\">\xf0\x9f\x94\x8a Sintesi Vocale (TTS)</div>"
              "<p style=\"font-size:13px;color:var(--dim);margin-bottom:8px\">"
                "Scrivi il testo e premi <b>Parla</b> — usa le voci installate nel browser.</p>"
              "<textarea id=\"tts-txt\" style=\"width:100%;height:80px;background:var(--inp);"
                "color:var(--txt);border:1px solid var(--brd);border-radius:8px;padding:10px;"
                "font-size:13px;resize:vertical;box-sizing:border-box;margin-bottom:8px\""
                " placeholder=\"Scrivi qui il testo da leggere...\"></textarea>"
              "<div style=\"display:flex;gap:8px;align-items:center;flex-wrap:wrap;margin-bottom:6px\">"
                "<button id=\"tts-speak\" style=\"background:var(--acc);color:#fff;border:none;"
                  "border-radius:8px;padding:9px 20px;cursor:pointer;font-size:14px;font-weight:600\">"
                  "\xf0\x9f\x94\x8a Parla</button>"
                "<button id=\"tts-stop\" style=\"background:#e53e3e;color:#fff;border:none;"
                  "border-radius:8px;padding:9px 18px;cursor:pointer;font-size:13px\">"
                  "\xe2\x8f\xb9 Stop</button>"  /* ⏹ */
                "<select id=\"tts-voice\" style=\"background:var(--inp);color:var(--txt);"
                  "border:1px solid var(--brd);border-radius:8px;padding:6px 10px;"
                  "font-size:12px;flex:1;min-width:120px\"></select>"
                "<label style=\"font-size:12px;color:var(--dim)\">"
                  "Velocit\xc3\xa0:"  /* Velocità */
                  "<input type=\"range\" id=\"tts-rate\" min=\"0.5\" max=\"2\" step=\"0.1\" value=\"1\""
                    " style=\"margin:0 6px;vertical-align:middle;width:80px\">"
                  "<span id=\"tts-rate-lbl\" style=\"font-size:11px\">1.0x</span></label>"
              "</div>"
              "<div id=\"tts-status\" style=\"font-size:12px;color:var(--dim);margin-bottom:12px\"></div>"

              "<hr style=\"border:none;border-top:1px solid var(--brd);margin:10px 0\">"

              /* ── 2. STT via microfono → /api/whisper ── */
              "<div class=\"tab-hd\">\xf0\x9f\x8e\x99 Riconoscimento Vocale (STT)</div>"
              "<p style=\"font-size:13px;color:var(--dim);margin-bottom:8px\">"
                "Registra la voce dal microfono e trascrivila via Whisper (server Prismalux).</p>"
              "<div style=\"display:flex;gap:8px;align-items:center;margin-bottom:8px\">"
                "<button id=\"stt-rec\" style=\"background:#c53030;color:#fff;border:none;"
                  "border-radius:8px;padding:9px 20px;cursor:pointer;font-size:14px;font-weight:600\">"
                  "\xf0\x9f\x8e\x99 Registra</button>"
                "<span id=\"stt-timer\" style=\"font-size:13px;color:var(--acc);font-weight:600\"></span>"
                "<div id=\"stt-level\" style=\"flex:1;height:10px;background:var(--brd);"
                  "border-radius:5px;overflow:hidden\">"
                  "<div id=\"stt-level-bar\" style=\"height:100%;background:var(--acc);"
                    "width:0%;transition:width 0.1s\"></div>"
                "</div>"
              "</div>"
              "<div id=\"stt-status\" style=\"font-size:13px;color:var(--dim);margin-bottom:8px\"></div>"
              "<textarea id=\"stt-out\" style=\"width:100%;height:100px;background:var(--inp);"
                "color:var(--txt);border:1px solid var(--brd);border-radius:8px;padding:10px;"
                "font-size:13px;resize:vertical;box-sizing:border-box;margin-bottom:8px\""
                " placeholder=\"Testo riconosciuto apparer\xc3\xa0 qui...\"></textarea>"
              "<div style=\"display:flex;gap:8px\">"
                "<button id=\"stt-send\" style=\"background:var(--acc);color:#fff;border:none;"
                  "border-radius:8px;padding:8px 16px;cursor:pointer;font-size:13px\">"
                  "\xf0\x9f\x92\xac Invia in Chat</button>"  /* 💬 */
                "<button id=\"stt-copy\" style=\"background:transparent;color:var(--txt);"
                  "border:1px solid var(--brd);border-radius:8px;padding:8px 14px;"
                  "cursor:pointer;font-size:13px\">"
                  "\xf0\x9f\x93\x8b Copia</button>"  /* 📋 */
              "</div>"

              "<hr style=\"border:none;border-top:1px solid var(--brd);margin:12px 0\">"

              /* ── 3. Whisper file upload (legacy) ── */
              "<div class=\"tab-hd\">\xf0\x9f\x93\x82 Carica File Audio (Whisper)</div>"
              "<p style=\"font-size:13px;color:var(--dim);margin-bottom:10px\">"
                "Carica un file audio (WAV/MP3/OGG) per trascriverlo via Whisper.</p>"
              "<input type=\"file\" id=\"wsp-file\" accept=\".wav,.mp3,.ogg,.m4a,.flac\" style=\"display:none\">"
              "<button id=\"wsp-pick\" style=\"background:var(--acc);color:#fff;border:none;"
                "border-radius:8px;padding:8px 18px;cursor:pointer;font-size:13px;margin-bottom:10px\">"
                "\xf0\x9f\x93\x82 Scegli file audio</button>"
              "<div id=\"wsp-name\" style=\"font-size:12px;color:var(--dim);margin-bottom:8px\"></div>"
              "<button id=\"wsp-run\" style=\"background:#7c3aed;color:#fff;border:none;"
                "border-radius:8px;padding:8px 18px;cursor:pointer;font-size:13px;display:none\">"
                "\xf0\x9f\x8e\x99 Trascrivi</button>"
              "<div id=\"wsp-status\" style=\"font-size:13px;color:var(--acc);margin-top:8px\"></div>"
              "<textarea id=\"wsp-out\" readonly style=\"width:100%;height:180px;"
                "background:var(--inp);color:var(--txt);border:1px solid var(--brd);"
                "border-radius:8px;padding:10px;font-size:13px;resize:vertical;"
                "box-sizing:border-box;margin-top:10px;display:none\""
                " placeholder=\"Testo trascritto...\"></textarea>"
            "</div>\n"
            "<script>\n"
            "const THEMES={"
              /* ── Dark ── */
              "dark:        {bg:'#0f1117',hdr:'#1a1d2e',brd:'#2a2d4e',acc:'#6c63ff',usr:'#6c63ff',aim:'#1e2235',txt:'#e0e0f0',dim:'#888',inp:'#0f1117'},"
              "dark_classic:{bg:'#0c0e18',hdr:'#121420',brd:'#1e2438',acc:'#4a90e2',usr:'#4a90e2',aim:'#141622',txt:'#d8e4f8',dim:'#7a9cc8',inp:'#0c0e18'},"
              "dark_amber:  {bg:'#17140c',hdr:'#211c10',brd:'#3a3020',acc:'#ffb300',usr:'#ffb300',aim:'#261f12',txt:'#f5e8cc',dim:'#c0a870',inp:'#17140c'},"
              "dark_cyan:   {bg:'#13141f',hdr:'#1a1b28',brd:'#2a2b40',acc:'#00b8d9',usr:'#00b8d9',aim:'#1c1c26',txt:'#e8eaf6',dim:'#b0b4d0',inp:'#13141f'},"
              "dark_green:  {bg:'#0d1510',hdr:'#121e15',brd:'#1e3020',acc:'#2ecc71',usr:'#2ecc71',aim:'#152018',txt:'#d4f0da',dim:'#8ac890',inp:'#0d1510'},"
              "dark_lavender:{bg:'#0f0c18',hdr:'#161022',brd:'#261c38',acc:'#8b68e8',usr:'#8b68e8',aim:'#181228',txt:'#e0d8f8',dim:'#9878c8',inp:'#0f0c18'},"
              "dark_ocean:  {bg:'#0a1318',hdr:'#0e1c24',brd:'#1a2c38',acc:'#20c4da',usr:'#20c4da',aim:'#101e28',txt:'#d0eaf8',dim:'#7ab8d0',inp:'#0a1318'},"
              "dark_purple: {bg:'#130f1c',hdr:'#1c1727',brd:'#2e2442',acc:'#b47df5',usr:'#9c5ff0',aim:'#1f1a2c',txt:'#ede8f5',dim:'#a088c8',inp:'#130f1c'},"
              "dark_sunset: {bg:'#17100a',hdr:'#21160e',brd:'#382010',acc:'#fd7e14',usr:'#fd7e14',aim:'#261810',txt:'#f8e8d0',dim:'#c08050',inp:'#17100a'},"
              "dark_rainbow:{bg:'#111118',hdr:'#18181f',brd:'#282838',acc:'#ff6b6b',usr:'#ff6b6b',aim:'#1c1c24',txt:'#f0eefc',dim:'#b0b0d0',inp:'#111118'},"
              /* ── Speciali ── */
              "dracula:     {bg:'#282a36',hdr:'#21222c',brd:'#44475a',acc:'#bd93f9',usr:'#ff79c6',aim:'#383a59',txt:'#f8f8f2',dim:'#6272a4',inp:'#282a36'},"
              "nord:        {bg:'#2e3440',hdr:'#3b4252',brd:'#434c5e',acc:'#88c0d0',usr:'#5e81ac',aim:'#3b4252',txt:'#eceff4',dim:'#8fbcbb',inp:'#2e3440'},"
              "hacker:      {bg:'#000000',hdr:'#001a00',brd:'#003300',acc:'#00ff00',usr:'#00ff00',aim:'#002200',txt:'#00ff00',dim:'#00cc00',inp:'#000000'},"
              "military:    {bg:'#1a2e1a',hdr:'#2d4a2d',brd:'#3d5a3d',acc:'#9acd32',usr:'#9acd32',aim:'#344e34',txt:'#d4ed91',dim:'#8ab040',inp:'#1a2e1a'},"
              "neon:        {bg:'#0a0a0a',hdr:'#1a1a1a',brd:'#2a2a2a',acc:'#00ff9d',usr:'#00ff9d',aim:'#1e1e1e',txt:'#ffffff',dim:'#60c090',inp:'#0a0a0a'},"
              "pink:        {bg:'#2d1b2d',hdr:'#4a1a4a',brd:'#6b2a6b',acc:'#ec4899',usr:'#ec4899',aim:'#541e54',txt:'#fce7f3',dim:'#d07090',inp:'#2d1b2d'},"
              "solar:       {bg:'#002b36',hdr:'#073642',brd:'#0f4c5c',acc:'#268bd2',usr:'#268bd2',aim:'#093a4a',txt:'#839496',dim:'#657b83',inp:'#002b36'},"
              /* ── Venom ── */
              "venom_blue:  {bg:'#0a0a0a',hdr:'#1a1a1a',brd:'#2a2a2a',acc:'#00bfff',usr:'#00bfff',aim:'#1e1e1e',txt:'#ffffff',dim:'#808080',inp:'#0a0a0a'},"
              "venom_red:   {bg:'#0a0a0a',hdr:'#1a1a1a',brd:'#2a2a2a',acc:'#ff0000',usr:'#ff0000',aim:'#1e1e1e',txt:'#ffffff',dim:'#808080',inp:'#0a0a0a'},"
              "venom_orange:{bg:'#0a0a0a',hdr:'#1a1a1a',brd:'#2a2a2a',acc:'#ff4500',usr:'#ff4500',aim:'#1e1e1e',txt:'#ffffff',dim:'#808080',inp:'#0a0a0a'},"
              "venom_green: {bg:'#0a0a0a',hdr:'#1a1a1a',brd:'#2a2a2a',acc:'#00ff00',usr:'#00ff00',aim:'#1e1e1e',txt:'#ffffff',dim:'#808080',inp:'#0a0a0a'},"
              /* ── Light ── */
              "light:       {bg:'#f0f2f5',hdr:'#ffffff',brd:'#dce3f5',acc:'#0072c6',usr:'#0072c6',aim:'#edf0fa',txt:'#1a1e30',dim:'#7880a8',inp:'#f0f2f5'},"
              "light_mint:  {bg:'#f0f9f6',hdr:'#ffffff',brd:'#a8d8cc',acc:'#00897b',usr:'#00897b',aim:'#e0f5ee',txt:'#1a2e28',dim:'#6a9088',inp:'#f0f9f6'},"
              "light_rose:  {bg:'#fef0f4',hdr:'#ffffff',brd:'#f48fb1',acc:'#c2185b',usr:'#c2185b',aim:'#fde8ef',txt:'#2e0a18',dim:'#9a6070',inp:'#fef0f4'},"
              "light_sky:   {bg:'#f0f7ff',hdr:'#ffffff',brd:'#90c8f0',acc:'#0277bd',usr:'#0277bd',aim:'#e0f0ff',txt:'#0a1828',dim:'#6a90a8',inp:'#f0f7ff'},"
              "light_sand:  {bg:'#fdf5ea',hdr:'#ffffff',brd:'#ffcc80',acc:'#e65100',usr:'#e65100',aim:'#fdecd8',txt:'#2e1800',dim:'#9a7040',inp:'#fdf5ea'}"
            "};\n"
            "function applyTheme(n){"
              "if(n==='solarized')n='solar';"  /* backward compat */
              "const t=THEMES[n]||THEMES.dark,r=document.documentElement.style;"
              "r.setProperty('--bg',t.bg);r.setProperty('--hdr',t.hdr);"
              "r.setProperty('--brd',t.brd);r.setProperty('--acc',t.acc);"
              "r.setProperty('--usr',t.usr);r.setProperty('--aim',t.aim);"
              "r.setProperty('--txt',t.txt);r.setProperty('--dim',t.dim);"
              "r.setProperty('--inp',t.inp);localStorage.setItem('plx-theme',n);}\n"
            "const tSaved=localStorage.getItem('plx-theme')||'dark';"
            "applyTheme(tSaved);"
            "const tsel=document.getElementById('tsel');"
            "if(tsel)tsel.value=THEMES[tSaved]?tSaved:'dark';\n"
            "if(tsel)tsel.addEventListener('change',function(){applyTheme(this.value);});\n"
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
            "let sysTool='',toolCtxSuffix='';\n"
            "const TOOL_CFG={"
              "nessuno:{color:null,icon:'',name:'Chat',ph:'Scrivi un messaggio...',extra:null},"
              "scrittura:{color:'#a78bfa',icon:'✍️',name:'Scrittura',"
                "ph:'Descrivi il testo da scrivere...',"
                "extra:{type:'select',lbl:'Stile',"
                  "opts:['Formale','Informale','Creativo','Narrativo','Email','Report']}},"
              "programmazione:{color:'#34d399',icon:'💻',name:'Programmazione',"
                "ph:'Descrivi il codice da scrivere...',"
                "extra:{type:'select',lbl:'Linguaggio',"
                  "opts:['Python','JavaScript','TypeScript','C++','Java','Rust','Go','SQL','Bash','C#']}},"
              "matematica:{color:'#60a5fa',icon:'π',name:'Matematica',"
                "ph:'Inserisci il problema matematico...',"
                "extra:{type:'symbols',"
                  "syms:['π','√','²','³','⁴','⁵','⁶','⁷','⁸','⁹','⁰',"
                    "'Σ','∫','∮','∂','∇','∞','→','⇒','⇔','↔',"
                    "'±','×','÷','≤','≥','≠','≈','≡','∝',"
                    "'α','β','γ','δ','ε','θ','λ','μ','ξ','ρ','σ','τ','φ','ψ','ω',"
                    "'Α','Β','Γ','Δ','Ε','Θ','Λ','Π','Σ','Φ','Ψ','Ω',"
                    "'∀','∃','∈','∉','⊂','⊃','∪','∩','∅',"
                    "'½','⅓','¼','¾','∛','∜',"
                    "'ℝ','ℤ','ℕ','ℚ','ℂ','ℵ']}},"
              "ricerca:{color:'#22d3ee',icon:'🔬',name:'Ricerca',"
                "ph:'Cosa vuoi ricercare o analizzare?',"
                "extra:{type:'select',lbl:'Formato',"
                  "opts:['Accademico','Divulgativo','Executive Summary','Review sistematica']}},"
              "impara:{color:'#fb923c',icon:'📚',name:'Impara',"
                "ph:'Cosa vuoi imparare oggi?',"
                "extra:{type:'select',lbl:'Livello',"
                  "opts:['Principiante','Intermedio','Avanzato','Esperto']}},"
              "lavoro:{color:'#2dd4bf',icon:'💼',name:'Cerca Lavoro',"
                "ph:'Clicca un\\'offerta o scrivi una domanda sulla ricerca lavoro...',"
                "extra:null}"
            "};\n"
            "const TOOL_IND=document.getElementById('tool-ind'),"
              "TOOL_EX=document.getElementById('tool-extra'),"
              "LAV_PANEL=document.getElementById('lavoro-panel');\n"
            /* Dichiarate prima di applyTool — evita TDZ quando applyTool viene chiamata subito */
            "const L=document.getElementById('log'),"
              "T=document.getElementById('txt'),"
              "B=document.getElementById('snd'),"
              "H=[];\n"
            "const LAV_LIST=document.getElementById('lav-list'),"
              "LAV_TIPO=document.getElementById('lav-tipo'),"
              "LAV_LIV=document.getElementById('lav-liv'),"
              "LAV_SRCH=document.getElementById('lav-search'),"
              "LAV_CVB=document.getElementById('lav-cv-btn');\n"
            "function applyTool(key){"
              "sysTool='';toolCtxSuffix='';"
              "document.querySelectorAll('.tbtn').forEach(function(b){"
                "if(b.dataset.t===key){b.classList.add('on');sysTool=b.dataset.sys||'';}"
                "else b.classList.remove('on');});"
              "localStorage.setItem('plx-tool',key);\n"
              "const cfg=TOOL_CFG[key]||TOOL_CFG.nessuno;\n"
              /* tool indicator */
              "if(cfg.color){"
                "TOOL_IND.style.cssText='display:flex;background:'+cfg.color+'18;"
                  "border-left:3px solid '+cfg.color+';color:'+cfg.color"
                  "+';padding:6px 14px;font-size:13px;font-weight:600;"
                  "align-items:center;gap:6px;flex-shrink:0;"
                  "border-bottom:1px solid var(--brd);transition:background .25s';"
                "TOOL_IND.textContent=cfg.icon+(cfg.icon?' ':'')+cfg.name;"
              "}else{TOOL_IND.style.display='none';}\n"
              /* lavoro panel show/hide */
              "if(key==='lavoro'){"
                "LAV_PANEL.style.display='flex';"
                "if(!LAV_PANEL.dataset.loaded){lavFetch();}"
              "}else{LAV_PANEL.style.display='none';}\n"
              /* placeholder */
              "T.placeholder=cfg.ph;\n"
              /* extra widget */
              "TOOL_EX.innerHTML='';toolCtxSuffix='';\n"
              "if(cfg.extra){"
                "TOOL_EX.style.cssText='display:flex;gap:6px;flex-wrap:wrap;"
                  "align-items:center;padding:4px 0;flex-shrink:0';\n"
                "const lbl=document.createElement('span');"
                  "lbl.className='xsel-lbl';\n"
                "if(cfg.extra.type==='select'){"
                  "lbl.textContent=cfg.extra.lbl+':';"
                  "const sel=document.createElement('select');sel.className='xsel';\n"
                  "cfg.extra.opts.forEach(function(o){"
                    "const op=document.createElement('option');"
                    "op.value=o;op.textContent=o;sel.appendChild(op);});\n"
                  "toolCtxSuffix=cfg.extra.lbl+': '+cfg.extra.opts[0]+'.';\n"
                  "sel.addEventListener('change',function(){"
                    "toolCtxSuffix=cfg.extra.lbl+': '+this.value+'.';});\n"
                  "TOOL_EX.appendChild(lbl);TOOL_EX.appendChild(sel);"
                "}else if(cfg.extra.type==='symbols'){"
                  "lbl.textContent='Simboli:';TOOL_EX.appendChild(lbl);\n"
                  "cfg.extra.syms.forEach(function(sym){"
                    "const b=document.createElement('button');"
                    "b.className='sym-btn';b.textContent=sym;\n"
                    "b.addEventListener('click',function(){"
                      "const p=T.selectionStart;"
                      "T.value=T.value.slice(0,p)+sym+T.value.slice(T.selectionEnd);"
                      "T.selectionStart=T.selectionEnd=p+sym.length;T.focus();"
                      "T.style.height='';T.style.height=Math.min(T.scrollHeight,200)+'px';});\n"
                    "TOOL_EX.appendChild(b);});"
                "}"
              "}else{TOOL_EX.style.display='none';}"
            "}\n"
            "document.querySelectorAll('.tbtn').forEach(function(b){"
              "b.addEventListener('click',function(){applyTool(this.dataset.t);closeDrw();});});\n"
            "applyTool(localStorage.getItem('plx-tool')||'nessuno');\n"
            /* ── Lavoro panel JS ── */
            "let lavCV='';\n"
            "function lavRender(offerte){"
              "LAV_LIST.innerHTML='';"
              "if(!offerte.length){"
                "LAV_LIST.innerHTML='<div id=\"lav-empty\">Nessuna offerta trovata</div>';"
                "return;}"
              "offerte.forEach(function(o,i){"
                "const d=document.createElement('div');d.className='lav-item';"
                "const isLink=o.requisiti&&(o.requisiti.startsWith('http')||o.requisiti.startsWith('https'));\n"
                /* XSS-safe: usa createElement + textContent invece di innerHTML con dati esterni */
                "const r1=document.createElement('div');r1.className='lav-row1';"
                "const az=document.createElement('span');az.className='lav-az';az.textContent=o.azienda;"
                "const ru=document.createElement('span');ru.className='lav-ruolo';ru.textContent='\xe2\x80\x94 '+o.ruolo;"
                "const se=document.createElement('span');se.className='lav-sede';se.textContent=o.sede;"
                "r1.appendChild(az);r1.appendChild(ru);r1.appendChild(se);"
                "const rq=document.createElement('div');rq.className='lav-req';"
                "rq.textContent=isLink?'\xf0\x9f\x94\x97 Apri portale':o.requisiti;"
                "d.appendChild(r1);d.appendChild(rq);\n"
                "d.addEventListener('click',function(){"
                  "document.querySelectorAll('.lav-item').forEach(function(x){x.classList.remove('sel');});"
                  "d.classList.add('sel');\n"
                  "if(isLink){window.open(o.requisiti,'_blank');return;}\n"
                  "const cvPart=lavCV?'\\n\\nMIO CV:\\n'+lavCV:'';\n"
                  "T.value='Analizza questa offerta di lavoro per il mio profilo e suggerisci come candidarmi:\\n\\n'"
                    "+'Azienda: '+o.azienda+'\\nRuolo: '+o.ruolo+'\\nSede: '+o.sede"
                    "+'\\nRequisiti: '+o.requisiti+cvPart;\n"
                  "T.style.height='';T.style.height=Math.min(T.scrollHeight,200)+'px';"
                  "T.focus();});\n"
                "LAV_LIST.appendChild(d);});\n"
              "LAV_PANEL.dataset.loaded='1';"
            "}\n"
            "async function lavFetch(){"
              "LAV_LIST.innerHTML='<div id=\"lav-empty\">\xe2\x8f\xb3 Caricamento offerte...</div>';\n"
              "try{"
                "const r=await fetch('/api/lavoro',{method:'POST',"
                  "headers:{'Content-Type':'application/json'," + authHeadersJs + "},"
                  "body:JSON.stringify({tipo:LAV_TIPO.value,livello:LAV_LIV.value})});\n"
                "const data=await r.json();"
                "lavRender(Array.isArray(data)?data:[]);"
              "}catch(e){"
                "LAV_LIST.innerHTML='<div id=\"lav-empty\">\xe2\x9d\x8c Errore caricamento offerte</div>';}"
            "}\n"
            "LAV_SRCH.addEventListener('click',lavFetch);\n"
            "LAV_CVB.addEventListener('click',async function(){"
              "if(lavCV){"
                "T.value='Il mio CV:\\n\\n'+lavCV;"
                "T.style.height='';T.style.height=Math.min(T.scrollHeight,200)+'px';"
                "T.focus();return;}\n"
              "LAV_CVB.textContent='\xe2\x8f\xb3';"
              "try{"
                "const r=await fetch('/api/cv',{headers:{" + authHeadersJs + "}});"
                "const j=await r.json();lavCV=j.cv||'';"
                "LAV_CVB.textContent='\xf0\x9f\x93\x84 CV';"
                "if(lavCV){"
                  "T.value='Il mio CV:\\n\\n'+lavCV;"
                  "T.style.height='';T.style.height=Math.min(T.scrollHeight,200)+'px';"
                  "T.focus();}"
              "}catch(e){LAV_CVB.textContent='\xf0\x9f\x93\x84 CV';}"
            "});\n"
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
              "const fullTool=sysTool+(toolCtxSuffix?'\\n'+toolCtxSuffix:'');\n"
              "const sp=[fullTool,sysPers].filter(Boolean).join('\\n');\n"
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
                "var isDisconn=(e instanceof TypeError&&"
                  "(e.message.indexOf('fetch')>=0||e.message.indexOf('network')>=0"
                  "||e.message.indexOf('Failed')>=0||e.message.indexOf('NetworkError')>=0"
                  "||e.message.indexOf('aborted')>=0));"
                "if(!full){"
                  "aiD.style.color='#ff9944';"
                  "aiD.textContent=isDisconn?"
                    "'\xe2\x9a\xa0\xef\xb8\x8f Server disconnesso \xe2\x80\x94 il server Prismalux potrebbe essere stato fermato.':"
                    "'Errore: '+e.message;}"
                "else{"
                  "var warnD=document.createElement('div');"
                  "warnD.style.cssText='color:#ff9944;font-size:12px;padding:4px 0';"
                  "warnD.textContent='\xe2\x9a\xa0\xef\xb8\x8f Risposta interrotta (server disconnesso)';"
                  "L.appendChild(warnD);L.scrollTop=L.scrollHeight;}"
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
            "async function doGo(){await go();sesSaveCurrent();}\n"
            "B.addEventListener('click',doGo);\n"
            "T.addEventListener('keydown',function(e){\n"
              "if(e.key==='Enter'&&!e.shiftKey){e.preventDefault();doGo();}\n"
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
              "MCSL=document.getElementById('mcp-tool-sel'),"
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
                "MCSL.innerHTML='<option value=\"\">-- seleziona strumento --</option>';\n"
                "tls.forEach(function(t,i){"
                  "const o=document.createElement('option');"
                  "o.value=i;o.textContent=t.name;MCSL.appendChild(o);});\n"
                "MTLS.style.display='block';\n"
                "MCON.innerHTML='&#x2713; '+tls.length+' strumenti';\n"
              "}catch(e){"
                "MCON.textContent='&#x2717; '+e.message;"
                "setTimeout(function(){MCON.innerHTML='&#x1F517; Connetti';},3000);}});\n"
            "MCSL.addEventListener('change',function(){"
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
              "const idx=parseInt(MCSL.value);"
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
              "TS=document.getElementById('tab-sys'),"
              "TM=document.getElementById('tab-mat'),"
              "TG=document.getElementById('tab-age'),"
              "TI=document.getElementById('tab-imp'),"
              "TU=document.getElementById('tab-mul'),"
              "TR=document.getElementById('tab-rag'),"
              "TCOD=document.getElementById('tab-cod'),"
              "TGVZ=document.getElementById('tab-gvz'),"
              "TWSP=document.getElementById('tab-wsp');\n"
            "let sysItv=null;\n"
            "function switchTab(n){"
              "TABS.forEach(function(t){t.classList.toggle('on',t.dataset.tab===n);});"
              "TC.className=n==='chat'?'tab-on-flex':'tab-off';"
              "TA.className=n==='apps'?'tab-on-block':'tab-off';"
              "TS.className=n==='sys'?'tab-on-block':'tab-off';"
              "TM.className=n==='mat'?'tab-on-block':'tab-off';"
              "TG.className=n==='age'?'tab-on-block':'tab-off';"
              "TI.className=n==='imp'?'tab-on-block':'tab-off';"
              "TU.className=n==='mul'?'tab-on-block':'tab-off';"
              "TR.className=n==='rag'?'tab-on-block':'tab-off';"
              "TCOD.className=n==='cod'?'tab-on-block':'tab-off';"
              "TGVZ.className=n==='gvz'?'tab-on-block':'tab-off';"
              "TWSP.className=n==='wsp'?'tab-on-block':'tab-off';"
              "if(n==='sys'){"
                "fetchSys();"
                "if(!sysItv)sysItv=setInterval(fetchSys,3000);"
              "}else{if(sysItv){clearInterval(sysItv);sysItv=null;}}}\n"
            "TABS.forEach(function(t){"
              "t.addEventListener('click',function(){switchTab(this.dataset.tab);});});\n"
            "switchTab('chat');\n"
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
            /* ── Matematica tab JS ── */
            /* Sub-tab switching */
            "(function(){"
              "var tabs=document.querySelectorAll('.mat-tab');"
              "tabs.forEach(function(btn){"
                "btn.addEventListener('click',function(){"
                  "tabs.forEach(function(b){b.classList.remove('on');});"
                  "this.classList.add('on');"
                  "var mt=this.dataset.mt;"
                  "document.querySelectorAll('.mtp').forEach(function(p){p.style.display='none';});"
                  "var pnl=document.getElementById('mtp-'+mt);"
                  "if(pnl){pnl.style.display='flex';}"
                "});"
              "});"
            "})();\n"
            /* mathCall helper */
            "async function mathCall(params){\n"
              "const resp=await fetch('/api/math',{method:'POST',"
                "headers:{'Content-Type':'application/json'," + authHeadersJs + "},"
                "body:JSON.stringify(params)});\n"
              "const data=await resp.json();\n"
              "if(data.error)throw new Error(data.error);\n"
              "return data.result;\n"
            "}\n"
            /* mat-out helpers */
            "function matSetOut(txt){document.getElementById('mat-out').textContent=txt;}\n"
            "function matSetStatus(txt){document.getElementById('mat-status').textContent=txt;}\n"
            /* detectPatternLocal — rileva pattern in JS */
            "function detectPatternLocal(seq){\n"
              "var n=seq.length;\n"
              "if(n<2)return 'Troppo corta per rilevare un pattern.';\n"
              "var eps=1e-9;\n"
              "function eq(a,b){return Math.abs(a-b)<eps;}\n"
              /* Aritmetica */
              "var d=seq[1]-seq[0],arith=true;\n"
              "for(var i=2;i<n;i++){if(!eq(seq[i]-seq[i-1],d)){arith=false;break;}}\n"
              "if(arith){"
                "if(eq(d,0))return 'Sequenza costante: a(n) = '+seq[0];\n"
                "return 'Aritmetica: a(n) = '+seq[0]+' + (n-1)·'+d+'   [d = '+d+']';}\n"
              /* Geometrica */
              "if(!eq(seq[0],0)){"
                "var r=seq[1]/seq[0],geom=true;\n"
                "for(var i=2;i<n;i++){if(!eq(seq[i]/seq[i-1],r)){geom=false;break;}}\n"
                "if(geom)return 'Geometrica: a(n) = '+seq[0]+' · '+r+'^(n-1)   [r = '+r+']';}\n"
              /* Quadratica */
              "if(n>=3){"
                "var d1=[],d2=[];\n"
                "for(var i=0;i<n-1;i++)d1.push(seq[i+1]-seq[i]);\n"
                "for(var i=0;i<n-2;i++)d2.push(d1[i+1]-d1[i]);\n"
                "var quad=true;\n"
                "for(var i=1;i<d2.length;i++){if(!eq(d2[i],d2[0])){quad=false;break;}}\n"
                "if(quad&&!eq(d2[0],0)){"
                  "var a=d2[0]/2,b=d1[0]-a,c=seq[0]-a-b;\n"
                  "return 'Quadratica: a(n) = '+a+'·n² + '+(b-2*a)+'·n + '+(c+a-b+a);}}\n"
              /* Fibonacci-like */
              "if(n>=3){"
                "var fib=true;\n"
                "for(var i=2;i<n;i++){if(!eq(seq[i],seq[i-1]+seq[i-2])){fib=false;break;}}\n"
                "if(fib)return 'Fibonacci-like: a(n)=a(n-1)+a(n-2), a(1)='+seq[0]+', a(2)='+seq[1];}\n"
              /* Quadrati */
              "{var sq=true;for(var i=0;i<n;i++){if(!eq(seq[i],(i+1)*(i+1))){sq=false;break;}}if(sq)return 'Quadrati perfetti: a(n) = n²';}\n"
              /* Cubi */
              "{var cu=true;for(var i=0;i<n;i++){if(!eq(seq[i],(i+1)*(i+1)*(i+1))){cu=false;break;}}if(cu)return 'Cubi: a(n) = n³';}\n"
              /* Triangolari */
              "{var tri=true;for(var i=0;i<n;i++){if(!eq(seq[i],(i+1)*(i+2)/2)){tri=false;break;}}if(tri)return 'Numeri triangolari: a(n) = n·(n+1)/2';}\n"
              /* Fattoriali */
              "{var fact=true,f=1;for(var i=0;i<n;i++){f*=(i+1);if(!eq(seq[i],f)){fact=false;break;}}if(fact)return 'Fattoriali: a(n) = n!';}\n"
              /* Potenze di 2 */
              "{var p2=true;for(var i=0;i<n;i++){if(!eq(seq[i],Math.pow(2,i+1))){p2=false;break;}}if(p2)return 'Potenze di 2: a(n) = 2^n';}\n"
              /* Numeri primi */
              "{var prs=[2,3,5,7,11,13,17,19,23,29,31,37,41,43,47,53,59,61,67,71];"
                "var pr=true;for(var i=0;i<n&&i<20;i++){if(!eq(seq[i],prs[i])){pr=false;break;}}"
                "if(pr)return 'Numeri primi: p(1)=2, p(2)=3, p(3)=5, ...';}\n"
              "return 'Pattern non riconosciuto localmente — prova Interpola sympy o Analizza AI.';\n"
            "}\n"
            /* parseSeq: estrae numeri da stringa */
            "function parseSeq(s){\n"
              "var parts=s.split(/[,\\s]+/).filter(Boolean);\n"
              "var nums=parts.map(function(p){return parseFloat(p.replace(',','.'));});\n"
              "return nums.filter(function(v){return !isNaN(v);});\n"
            "}\n"
            /* Bottone: Rileva (locale) */
            "document.getElementById('mat-local').addEventListener('click',function(){"
              "var seq=parseSeq(document.getElementById('mat-seq').value);"
              "if(seq.length<2){matSetOut('Inserisci almeno 2 termini.');return;}"
              "var res=detectPatternLocal(seq);"
              "document.getElementById('mat-seq-res').textContent=res;"
              "matSetOut('Sequenza: '+seq.join(', ')+'\\n\\n'+res);"
              "matSetStatus('Pattern rilevato localmente.');"
            "});\n"
            /* Bottone: Interpola sympy */
            "document.getElementById('mat-sympy').addEventListener('click',async function(){"
              "var seq=parseSeq(document.getElementById('mat-seq').value);"
              "if(seq.length<2){matSetOut('Inserisci almeno 2 termini.');return;}"
              "var nxt=parseInt(document.getElementById('mat-nxt').value)||5;"
              "matSetStatus('\xcf\x83 Interpolazione sympy in corso...');"
              "matSetOut('\xcf\x83 Analisi sympy in corso...');"
              "try{"
                "var res=await mathCall({action:'sequence_sympy',seq:seq,next:nxt});"
                "matSetOut(res);"
                "matSetStatus('Interpolazione completata.');"
              "}catch(e){matSetOut('Errore: '+e.message);matSetStatus('Errore.');}"
            "});\n"
            /* Bottone: Analizza AI */
            "document.getElementById('mat-ai').addEventListener('click',async function(){"
              "var seq=parseSeq(document.getElementById('mat-seq').value);"
              "if(seq.length<2){matSetOut('Inserisci almeno 2 termini.');return;}"
              "var nxt=parseInt(document.getElementById('mat-nxt').value)||5;"
              "var prompt='Analizza questa sequenza numerica e trova la formula o pattern: '+"
                "seq.join(', ')+'. Poi fornisci i '+nxt+' termini successivi con spiegazione.';"
              "T.value=prompt;await go();"
            "});\n"
            /* Bottone: Calcola costante */
            "document.getElementById('mat-calc-const').addEventListener('click',async function(){"
              "var key=document.getElementById('mat-const').value;"
              "var digits=parseInt(document.getElementById('mat-prec').value)||100;"
              "matSetStatus('\xcf\x80 Calcolo in corso...');"
              "matSetOut('\xcf\x80 Calcolo '+key+' a '+digits+' cifre...');"
              "try{"
                "var res=await mathCall({action:'constant',key:key,digits:digits});"
                "matSetOut(res);"
                "matSetStatus('\xe2\x9c\x85 Fatto.');"
              "}catch(e){matSetOut('Errore: '+e.message);matSetStatus('Errore.');}"
            "});\n"
            /* Bottone: Tutte le costanti */
            "document.getElementById('mat-all-const').addEventListener('click',async function(){"
              "matSetStatus('\xcf\x80 Calcolo tutte le costanti...');"
              "matSetOut('\xcf\x80 Calcolo costanti a 100 cifre...');"
              "try{"
                "var res=await mathCall({action:'all_constants',digits:100});"
                "matSetOut(res);"
                "matSetStatus('\xe2\x9c\x85 Fatto.');"
              "}catch(e){matSetOut('Errore: '+e.message);matSetStatus('Errore.');}"
            "});\n"
            /* Select N-esimo: aggiorna descrizione */
            "(function(){"
              "var descMap={"
                "pi_digit:'N-esima cifra decimale di π (dopo il punto). Es. N=1 → 1, N=2 → 4, N=3 → 1...',"
                "e_digit:'N-esima cifra decimale di e. Es. N=1 → 7, N=2 → 1...',"
                "prime:'Il primo con indice N. p(1)=2, p(2)=3, p(3)=5... (sympy per N fino a ~10 000 000)',"
                "fib:'F(1)=1, F(2)=1, F(3)=2, F(4)=3, F(5)=5... Anche per N molto grandi (mpmath).',"
                "fact:'N! — fattoriale. 1!=1, 5!=120, 100!=93326... (precisione arbitraria).',"
                "pow2:'2^N. Anche per N molto grandi (migliaia di cifre).',"
                "pi_block:'Le prime N cifre di π come blocco continuo (3.14159...).',"
                "phi_block:'Le prime N cifre di φ (sezione aurea).'"
              "};\n"
              "var ntype=document.getElementById('mat-ntype');"
              "var ndesc=document.getElementById('mat-nth-desc');"
              "function updateDesc(){ndesc.textContent=descMap[ntype.value]||'';}\n"
              "ntype.addEventListener('change',updateDesc);\n"
              "updateDesc();"
            "})();\n"
            /* Bottone: Calcola N-esimo */
            "document.getElementById('mat-calc-nth').addEventListener('click',async function(){"
              "var type=document.getElementById('mat-ntype').value;"
              "var nval=document.getElementById('mat-n').value.trim();"
              "if(!nval){matSetOut('Inserisci N.');return;}"
              "matSetStatus('#\xe2\x82\x99 Calcolo in corso (N='+nval+')...');"
              "matSetOut('#\xe2\x82\x99 Calcolo in corso (N='+nval+')...');"
              "try{"
                "var res=await mathCall({action:'nth',type:type,n:parseInt(nval)||0});"
                "matSetOut(res);"
                "matSetStatus('\xe2\x9c\x85 Fatto.');"
              "}catch(e){matSetOut('Errore: '+e.message);matSetStatus('Errore.');}"
            "});\n"
            /* Bottoni esempi rapidi espressione */
            "document.querySelectorAll('.mat-ex').forEach(function(btn){"
              "btn.addEventListener('click',async function(){"
                "var expr=this.dataset.e;"
                "document.getElementById('mat-expr').value=expr;"
                "var prec=parseInt(document.getElementById('mat-eprec').value)||50;"
                "matSetStatus('\xf0\x9f\xa7\xae Calcolo...');"
                "matSetOut('\xf0\x9f\xa7\xae Calcolo: '+expr+'...');"
                "try{"
                  "var res=await mathCall({action:'expr',expr:expr,prec:prec});"
                  "matSetOut(res);"
                  "matSetStatus('\xe2\x9c\x85 Fatto.');"
                "}catch(e){matSetOut('Errore: '+e.message);matSetStatus('Errore.');}"
              "});"
            "});\n"
            /* Bottone: Calcola espressione */
            "document.getElementById('mat-eval').addEventListener('click',async function(){"
              "var expr=document.getElementById('mat-expr').value.trim();"
              "if(!expr){matSetOut('Inserisci un\\'espressione.');return;}"
              "var prec=parseInt(document.getElementById('mat-eprec').value)||50;"
              "matSetStatus('\xf0\x9f\xa7\xae Calcolo...');"
              "matSetOut('\xf0\x9f\xa7\xae Calcolo: '+expr+'...');"
              "try{"
                "var res=await mathCall({action:'expr',expr:expr,prec:prec});"
                "matSetOut(res);"
                "matSetStatus('\xe2\x9c\x85 Fatto.');"
              "}catch(e){matSetOut('Errore: '+e.message);matSetStatus('Errore.');}"
            "});\n"
            /* Bottone: Semplifica */
            "document.getElementById('mat-simp').addEventListener('click',async function(){"
              "var expr=document.getElementById('mat-expr').value.trim();"
              "if(!expr){matSetOut('Inserisci un\\'espressione.');return;}"
              "var prec=parseInt(document.getElementById('mat-eprec').value)||50;"
              "matSetStatus('\xe2\x99\xbe Semplificazione...');"
              "matSetOut('\xe2\x99\xbe Semplificazione: '+expr+'...');"
              "try{"
                "var res=await mathCall({action:'simplify',expr:expr,prec:prec});"
                "matSetOut(res);"
                "matSetStatus('\xe2\x9c\x85 Fatto.');"
              "}catch(e){matSetOut('Errore: '+e.message);matSetStatus('Errore.');}"
            "});\n"
            /* mat-copy */
            "document.getElementById('mat-copy').addEventListener('click',function(){"
              "var txt=document.getElementById('mat-out').textContent;"
              "if(!txt)return;"
              "navigator.clipboard.writeText(txt).then(function(){"
                "matSetStatus('\xf0\x9f\x93\x8b Copiato!');"
                "setTimeout(function(){matSetStatus('Pronto.');},2000);"
              "}).catch(function(){matSetStatus('Errore copia.');});"
            "});\n"
            /* mat-clr */
            "document.getElementById('mat-clr').addEventListener('click',function(){"
              "matSetOut('');"
              "document.getElementById('mat-seq-res').textContent='';"
              "matSetStatus('Pronto.');"
            "});\n"
            /* plot-btn (grafico JS — identico a prima) */
            "document.getElementById('plot-btn').addEventListener('click',function(){"
              "const fn=document.getElementById('plot-fn').value.trim();"
              "const cv=document.getElementById('plot-cv');"
              "cv.style.display='block';"
              "const ctx=cv.getContext('2d');"
              "const W=cv.offsetWidth||cv.width;cv.width=W;cv.height=200;"
              "ctx.clearRect(0,0,W,200);"
              "ctx.strokeStyle=getComputedStyle(document.documentElement).getPropertyValue('--acc').trim()||'#6c63ff';"
              "ctx.lineWidth=2;"
              "const xs=[];const ys=[];"
              "for(let i=0;i<W;i++){"
                "const x=(i/W)*8-4;"
                "let y;try{y=eval(fn);}catch(e){ctx.fillStyle='#f44';ctx.fillText('Errore: '+e.message,10,20);return;}"
                "xs.push(x);ys.push(y);}"
              "const mn=Math.min(...ys),mx=Math.max(...ys);"
              "const rng=mx-mn||1;"
              "ctx.beginPath();"
              "xs.forEach(function(x,i){"
                "const px=i;"
                "const py=200-((ys[i]-mn)/rng)*180-10;"
                "i===0?ctx.moveTo(px,py):ctx.lineTo(px,py);});"
              "ctx.stroke();"
            "});\n"
            /* ── Pipeline Agenti JS ── */
            "const AGE_ROLES={"
              "Ricercatore:'Sei un ricercatore esperto. Analizza il task e raccoglie le informazioni chiave.',"
              "Scrittore:'Sei uno scrittore esperto. Prendi il contesto e produci un testo chiaro e coinvolgente.',"
              "Revisore:'Sei un revisore critico. Esamina il testo e proponi miglioramenti concreti.',"
              "Programmatore:'Sei un programmatore esperto. Implementa o migliora il codice relativo al task.',"
              "Analista:'Sei un analista. Sintetizza i risultati e fornisci conclusioni actionable.'"
            "};\n"
            "const AGE_RUN=document.getElementById('age-run'),"
              "AGE_LOG=document.getElementById('age-log'),"
              "AGE_TASK=document.getElementById('age-task');\n"
            "AGE_RUN.addEventListener('click',async function(){"
              "const task=AGE_TASK.value.trim();"
              "if(!task)return;"
              "AGE_RUN.disabled=true;AGE_RUN.textContent='\xe2\x8f\xb3 Pipeline in corso...';"
              "AGE_LOG.innerHTML='';"
              "const roles=["
                "document.getElementById('age-r1').value,"
                "document.getElementById('age-r2').value,"
                "document.getElementById('age-r3').value];\n"
              "let agectx=task;"
              "for(let i=0;i<roles.length;i++){"
                "const role=roles[i];"
                "const card=document.createElement('div');"
                "card.style.cssText='background:var(--aim);border:1px solid var(--brd);border-radius:8px;padding:10px;';"
                "card.innerHTML='<b style=\"color:var(--acc)\">\xf0\x9f\xa4\x96 '+role+'</b><p style=\"margin:6px 0 0;white-space:pre-wrap;font-size:13px\">...</p>';"
                "AGE_LOG.appendChild(card);AGE_LOG.scrollTop=AGE_LOG.scrollHeight;\n"
                "const p=card.querySelector('p');"
                "try{"
                  "const r=await fetch('/api/chat',{method:'POST',"
                    "headers:{'Content-Type':'application/json'," + authHeadersJs + "},"
                    "body:JSON.stringify({model:curModel,"
                      "messages:["
                        "{role:'system',content:AGE_ROLES[role]||role},"
                        "{role:'user',content:agectx}],"
                      "stream:false})});\n"
                  "const j=await r.json();"
                  "const out=(j.message&&j.message.content)||j.response||'Nessuna risposta';"
                  "p.textContent=out;agectx=out;"
                "}catch(e){p.textContent='Errore: '+e.message;break;}"
              "}"
              "AGE_RUN.disabled=false;AGE_RUN.textContent='\xe2\x96\xb6 Avvia Pipeline';"
            "});\n"
            /* ── Impara tab JS ── */
            "const IMP_OUT=document.getElementById('imp-out');\n"
            "function getImpArg(){"
              "const c=document.getElementById('imp-custom').value.trim();"
              "const s=document.getElementById('imp-arg').value;"
              "return c||s||'';}\n"
            "async function impChat(prompt){"
              "const arg=getImpArg();"
              "if(!arg){IMP_OUT.textContent='Seleziona o scrivi un argomento.';return;}"
              "IMP_OUT.textContent='...';"
              "try{"
                "const r=await fetch('/api/chat',{method:'POST',"
                  "headers:{'Content-Type':'application/json'," + authHeadersJs + "},"
                  "body:JSON.stringify({model:curModel,"
                    "messages:[{role:'system',content:'Sei un tutor esperto e paziente.'},"
                      "{role:'user',content:prompt+' Argomento: '+arg}],"
                    "stream:false})});\n"
                "const j=await r.json();"
                "IMP_OUT.textContent=(j.message&&j.message.content)||j.response||'Nessuna risposta';"
              "}catch(e){IMP_OUT.textContent='Errore: '+e.message;}"
            "}\n"
            "document.getElementById('imp-spiega').addEventListener('click',function(){"
              "const lv=document.getElementById('imp-lv').value;"
              "impChat('Spiega in modo chiaro per livello '+lv+'.');});\n"
            "document.getElementById('imp-quiz').addEventListener('click',function(){"
              "impChat('Crea 3 domande quiz con risposta corretta su');});\n"
            "document.getElementById('imp-esem').addEventListener('click',function(){"
              "impChat('Dai 3 esempi pratici e concreti di');});\n"
            /* ── RAG tab JS ── */
            "async function ragSearch(){"
              "const q=document.getElementById('rag-query').value.trim();"
              "if(!q)return;"
              "const res=document.getElementById('rag-results');"
              "res.innerHTML='<p style=\"color:var(--dim)\">Ricerca in corso...</p>';\n"
              "try{"
                "const r=await fetch('/api/rag',{method:'POST',"
                  "headers:{'Content-Type':'application/json'," + authHeadersJs + "},"
                  "body:JSON.stringify({query:q,k:5})});\n"
                "const d=await r.json();\n"
                "if(d.error){res.innerHTML='<p style=\"color:#f44\">Errore: '+d.error+'</p>';return;}\n"
                "if(d.info){res.innerHTML='<p style=\"color:var(--dim)\">'+d.info+'</p>';return;}\n"
                "const items=d.results||[];\n"
                "if(!items.length){res.innerHTML='<p style=\"color:var(--dim)\">Nessun risultato trovato.</p>';return;}\n"
                "let html='';\n"
                "items.forEach(function(x,i){"
                  "const score=typeof x.score==='number'?x.score.toFixed(3):'?';\n"
                  "const txt=(x.text||'').replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');\n"
                  "html+='<div class=\"rag-card\">'+"
                    "'<p>'+txt+'</p>'+"
                    "'<small>\xf0\x9f\x93\x8a Score: '+score+' \xe2\x80\x94 Risultato '+(i+1)+'</small>'+"
                    "'</div>';});\n"
                "res.innerHTML=html;"
              "}catch(e){"
                "res.innerHTML='<p style=\"color:#f44\">Errore di rete: '+e.message+'</p>';}}\n"
            "document.getElementById('rag-srch').addEventListener('click',ragSearch);\n"
            "document.getElementById('rag-query').addEventListener('keydown',function(e){"
              "if(e.key==='Enter'&&!e.shiftKey){e.preventDefault();ragSearch();}});\n"
            /* ── Coding tab JS ── */
            "document.getElementById('cod-run').addEventListener('click',function(){"
              "var code=document.getElementById('cod-editor').value.trim();"
              "var lang=document.getElementById('cod-lang').value;"
              "if(!code){return;}"
              "var out=document.getElementById('cod-out');"
              "out.textContent='\xe2\x8f\xb3 Analisi AI in corso...';"
              "var sys='Sei un esperto di '+lang+'. Analizza il seguente codice, spiega cosa fa, segnala bug e suggerisci miglioramenti in italiano.';"
              "var body=JSON.stringify({model:curModel,messages:[{role:'system',content:sys},{role:'user',content:'```'+lang+'\\n'+code+'\\n```'}],stream:false});"
              "fetch('/api/chat',{method:'POST',headers:{'Content-Type':'application/json','Authorization':'Bearer '+TOKEN},body:body})"
                ".then(function(r){return r.json();})"
                ".then(function(d){out.textContent=d.message&&d.message.content?d.message.content:JSON.stringify(d);})"
                ".catch(function(e){out.textContent='Errore: '+e;});"
            "});\n"
            "document.getElementById('cod-clr').addEventListener('click',function(){"
              "document.getElementById('cod-editor').value='';"
              "document.getElementById('cod-out').textContent='';"
            "});\n"
            /* ── Graphviz tab JS ── */
            "document.getElementById('gvz-ai').addEventListener('click',function(){"
              "var desc=document.getElementById('gvz-desc').value.trim();"
              "if(!desc)return;"
              "document.getElementById('gvz-err').textContent='\xe2\x8f\xb3 Generazione DOT...';"
              "var sys='Genera codice DOT Graphviz per il seguente grafo. Rispondi SOLO con il codice DOT tra ``` e ```, nient\\'altro.';"
              "var body=JSON.stringify({model:curModel,messages:[{role:'system',content:sys},{role:'user',content:desc}],stream:false});"
              "fetch('/api/chat',{method:'POST',headers:{'Content-Type':'application/json','Authorization':'Bearer '+TOKEN},body:body})"
                ".then(function(r){return r.json();})"
                ".then(function(d){"
                  "var txt=d.message&&d.message.content?d.message.content:'';"
                  "var m=txt.match(/```[^\\n]*\\n([\\s\\S]*?)```/);"
                  "if(m){document.getElementById('gvz-dot').value=m[1].trim();}"
                  "else{document.getElementById('gvz-dot').value=txt.trim();}"
                  "document.getElementById('gvz-err').textContent='';"
                "})"
                ".catch(function(e){document.getElementById('gvz-err').textContent='Errore AI: '+e;});"
            "});\n"
            "document.getElementById('gvz-run').addEventListener('click',function(){"
              "var dot=document.getElementById('gvz-dot').value.trim();"
              "if(!dot)return;"
              "document.getElementById('gvz-err').textContent='\xe2\x8f\xb3 Rendering...';"
              "fetch('/api/graphviz',{method:'POST',headers:{'Content-Type':'application/json','Authorization':'Bearer '+TOKEN},body:JSON.stringify({dot:dot})})"
                ".then(function(r){return r.json();})"
                ".then(function(d){"
                  "if(d.png){"
                    "document.getElementById('gvz-img').innerHTML='<img src=\"data:image/png;base64,'+d.png+'\" style=\"max-width:100%;border-radius:8px\">';"
                    "document.getElementById('gvz-err').textContent='';"
                  "}else{"
                    "document.getElementById('gvz-err').textContent=d.error||'Errore rendering';"
                  "}"
                "})"
                ".catch(function(e){document.getElementById('gvz-err').textContent='Errore: '+e;});"
            "});\n"
            "document.getElementById('gvz-clr').addEventListener('click',function(){"
              "document.getElementById('gvz-dot').value='';"
              "document.getElementById('gvz-desc').value='';"
              "document.getElementById('gvz-img').innerHTML='';"
              "document.getElementById('gvz-err').textContent='';"
            "});\n"
            /* ── TTS (SpeechSynthesis) JS ── */
            "(function(){\n"
            "var synth=window.speechSynthesis;\n"
            "var ttsSpeak=document.getElementById('tts-speak'),"
              "ttsStop=document.getElementById('tts-stop'),"
              "ttsVoice=document.getElementById('tts-voice'),"
              "ttsTxt=document.getElementById('tts-txt'),"
              "ttsRate=document.getElementById('tts-rate'),"
              "ttsRateLbl=document.getElementById('tts-rate-lbl'),"
              "ttsStatus=document.getElementById('tts-status');\n"
            "function populateVoices(){"
              "if(!synth)return;"
              "var voices=synth.getVoices();"
              "ttsVoice.innerHTML='';"
              "voices.forEach(function(v,i){"
                "var o=document.createElement('option');"
                "o.value=i;o.textContent=v.name+' ('+v.lang+')';"
                "if(v.lang&&v.lang.startsWith('it'))o.selected=true;"
                "ttsVoice.appendChild(o);});}\n"
            "if(synth){"
              "populateVoices();"
              "if(synth.onvoiceschanged!==undefined)synth.onvoiceschanged=populateVoices;"
              "ttsRate.addEventListener('input',function(){"
                "ttsRateLbl.textContent=parseFloat(this.value).toFixed(1)+'x';});\n"
              "ttsSpeak.addEventListener('click',function(){"
                "var txt=ttsTxt.value.trim();"
                "if(!txt){ttsStatus.textContent='\xe2\x9a\xa0\xef\xb8\x8f Inserisci testo prima';return;}"
                "synth.cancel();"
                "var u=new SpeechSynthesisUtterance(txt);"
                "var voices=synth.getVoices();"
                "var vi=parseInt(ttsVoice.value);"
                "if(!isNaN(vi)&&voices[vi])u.voice=voices[vi];"
                "u.rate=parseFloat(ttsRate.value)||1;"
                "u.lang='it-IT';"
                "u.onstart=function(){ttsStatus.textContent='\xf0\x9f\x94\x8a Riproduzione...';};"  /* 🔊 */
                "u.onend=function(){ttsStatus.textContent='\xe2\x9c\x85 Fine.';};"
                "u.onerror=function(e){ttsStatus.textContent='\xe2\x9d\x8c Errore: '+e.error;};"
                "synth.speak(u);});\n"
              "ttsStop.addEventListener('click',function(){"
                "synth.cancel();ttsStatus.textContent='\xe2\x8f\xb9 Fermato.';});}"  /* ⏹ */
            "else{ttsStatus.textContent='\xe2\x9d\x8c SpeechSynthesis non supportato da questo browser.';}\n"
            "})();\n"

            /* ── STT via MediaRecorder → /api/whisper ── */
            "(function(){\n"
            "var sttRec=document.getElementById('stt-rec'),"
              "sttStatus=document.getElementById('stt-status'),"
              "sttOut=document.getElementById('stt-out'),"
              "sttSend=document.getElementById('stt-send'),"
              "sttCopy=document.getElementById('stt-copy'),"
              "sttTimer=document.getElementById('stt-timer'),"
              "sttLevelBar=document.getElementById('stt-level-bar');\n"
            "var mr=null,isRec=false,sttSecs=0,sttItv=null,audioCtx=null,analyser=null,lvlItv=null;\n"

            /* Livello microfono */
            "function startLevel(stream){"
              "try{"
                "audioCtx=new(window.AudioContext||window.webkitAudioContext)();"
                "analyser=audioCtx.createAnalyser();"
                "analyser.fftSize=256;"
                "var src=audioCtx.createMediaStreamSource(stream);"
                "src.connect(analyser);"
                "lvlItv=setInterval(function(){"
                  "var buf=new Uint8Array(analyser.frequencyBinCount);"
                  "analyser.getByteFrequencyData(buf);"
                  "var avg=buf.reduce(function(a,b){return a+b;},0)/buf.length;"
                  "sttLevelBar.style.width=Math.min(100,avg*1.5)+'%';"
                "},80);"
              "}catch(e){}}\n"
            "function stopLevel(){"
              "if(lvlItv){clearInterval(lvlItv);lvlItv=null;}"
              "if(audioCtx){audioCtx.close();audioCtx=null;}"
              "sttLevelBar.style.width='0%';}\n"

            "if(!navigator.mediaDevices||!window.MediaRecorder){"
              "sttStatus.textContent='\xe2\x9d\x8c MediaRecorder non supportato da questo browser.';"
              "sttRec.disabled=true;"
            "}else{"
              "sttRec.addEventListener('click',function(){"
                "if(isRec){"
                  /* stop */
                  "mr.stop();isRec=false;"
                  "sttRec.textContent='\xf0\x9f\x8e\x99 Registra';"  /* 🎙️ */
                  "sttRec.style.background='#c53030';"
                  "clearInterval(sttItv);sttTimer.textContent='';"
                  "stopLevel();"
                "}else{"
                  /* start */
                  "navigator.mediaDevices.getUserMedia({audio:true}).then(function(stream){"
                    "var chunks=[];"
                    "mr=new MediaRecorder(stream);"
                    "mr.ondataavailable=function(e){if(e.data.size>0)chunks.push(e.data);};"
                    "mr.onstop=function(){"
                      "stream.getTracks().forEach(function(t){t.stop();});"
                      "var blob=new Blob(chunks,{type:mr.mimeType||'audio/webm'});"
                      "var fd=new FormData();"
                      "fd.append('audio',blob,'registrazione.webm');"
                      "sttStatus.textContent='\xe2\x8f\xb3 Trascrizione in corso...';"
                      "fetch('/api/whisper',{method:'POST',"
                        "headers:{'Authorization':'Bearer '+TOKEN},body:fd})"
                        ".then(function(r){return r.json();})"
                        ".then(function(d){"
                          "if(d.text){"
                            "sttOut.value=d.text;"
                            "sttStatus.textContent='\xe2\x9c\x85 Trascrizione completata.';"  /* ✅ */
                          "}else{"
                            "sttStatus.textContent='\xe2\x9d\x8c '+(d.error||'Nessun testo riconosciuto');"
                          "}"
                        "})"
                        ".catch(function(e){sttStatus.textContent='\xe2\x9d\x8c Errore: '+e;});"
                    "};\n"
                    "mr.start();"
                    "isRec=true;"
                    "sttRec.textContent='\xe2\x8f\xb9 Ferma';"  /* ⏹ */
                    "sttRec.style.background='#22543d';"
                    "sttSecs=0;sttTimer.textContent='00:00';"
                    "sttItv=setInterval(function(){"
                      "sttSecs++;var m=Math.floor(sttSecs/60),s=sttSecs%60;"
                      "sttTimer.textContent=(m<10?'0':'')+m+':'+(s<10?'0':'')+s;"
                    "},1000);"
                    "startLevel(stream);"
                    "sttStatus.textContent='\xf0\x9f\x94\xb4 Registrazione in corso...';"  /* 🔴 */
                  "}).catch(function(e){"
                    "sttStatus.textContent='\xe2\x9d\x8c Accesso microfono negato: '+e.message;"
                  "});}"
              "});\n"
              "sttSend.addEventListener('click',function(){"
                "var txt=sttOut.value.trim();if(!txt)return;"
                /* switch to chat tab and fill input */
                "switchTab('chat');"
                "var inp=document.getElementById('inp');"
                "if(inp){inp.value+=(inp.value?'\\n':'')+txt;inp.focus();}"
              "});\n"
              "sttCopy.addEventListener('click',function(){"
                "var txt=sttOut.value.trim();if(!txt)return;"
                "navigator.clipboard.writeText(txt).then(function(){"
                  "sttCopy.textContent='\xe2\x9c\x85 Copiato!';"
                  "setTimeout(function(){sttCopy.textContent='\xf0\x9f\x93\x8b Copia';},1800);});});"
            "}\n"
            "})();\n"

            /* ── Whisper tab JS ── */
            "var wspFile=null;\n"
            "document.getElementById('wsp-pick').addEventListener('click',function(){"
              "document.getElementById('wsp-file').click();"
            "});\n"
            "document.getElementById('wsp-file').addEventListener('change',function(e){"
              "wspFile=e.target.files[0];"
              "if(wspFile){"
                "document.getElementById('wsp-name').textContent='\xf0\x9f\x93\x84 '+wspFile.name+' ('+Math.round(wspFile.size/1024)+' KB)';"
                "document.getElementById('wsp-run').style.display='inline-block';"
              "}"
            "});\n"
            "document.getElementById('wsp-run').addEventListener('click',function(){"
              "if(!wspFile)return;"
              "var status=document.getElementById('wsp-status');"
              "status.textContent='\xe2\x8f\xb3 Caricamento e trascrizione in corso...';"
              "var fd=new FormData();"
              "fd.append('audio',wspFile,wspFile.name);"
              "fetch('/api/whisper',{method:'POST',headers:{'Authorization':'Bearer '+TOKEN},body:fd})"
                ".then(function(r){return r.json();})"
                ".then(function(d){"
                  "if(d.text){"
                    "var out=document.getElementById('wsp-out');"
                    "out.style.display='block';"
                    "out.value=d.text;"
                    "status.textContent='\xe2\x9c\x85 Trascrizione completata';"
                  "}else{"
                    "status.textContent='Errore: '+(d.error||'risposta non valida');"
                  "}"
                "})"
                ".catch(function(e){status.textContent='Errore: '+e;});"
            "});\n"
            /* ── Multimedia: Sintetizzatore + Visualizzatore ── */
            "let mulCtx=null,mulAnalyser=null,mulAnimId=null,mulPlaying=false;\n"
            "let mulSeq=[];\n"
            "function getMulCtx(){"
              "if(!mulCtx){"
                "mulCtx=new(window.AudioContext||window.webkitAudioContext)();"
                "mulAnalyser=mulCtx.createAnalyser();"
                "mulAnalyser.fftSize=1024;"
                "mulAnalyser.connect(mulCtx.destination);}"
              "return mulCtx;}\n"
            "function mulRenderSeq(){"
              "const list=document.getElementById('mul-seq');"
              "list.innerHTML='';"
              "mulSeq.forEach(function(t,i){"
                "const chip=document.createElement('div');"
                "chip.style.cssText='display:inline-flex;align-items:center;gap:5px;"
                  "background:var(--aim);border:1px solid var(--brd);border-radius:20px;"
                  "padding:3px 10px;font-size:12px;color:var(--txt)';"
                "chip.innerHTML='<span>'+t.freq+'Hz \xc2\xb7 '+t.dur+'ms \xc2\xb7 '+t.wave"
                  "+'</span><button style=\"background:none;border:none;color:var(--dim);"
                  "cursor:pointer;font-size:14px;padding:0 2px\" data-i=\"'+i+'\">\xc3\x97</button>';"
                "chip.querySelector('button').addEventListener('click',function(){"
                  "mulSeq.splice(parseInt(this.dataset.i),1);mulRenderSeq();});"
                "list.appendChild(chip);});}\n"
            "document.getElementById('mul-vol').addEventListener('input',function(){"
              "document.getElementById('mul-vol-lbl').textContent=this.value;});\n"
            "document.getElementById('mul-add').addEventListener('click',function(){"
              "const freq=parseFloat(document.getElementById('mul-freq').value)||440;"
              "const dur=parseInt(document.getElementById('mul-dur').value)||500;"
              "const wave=document.getElementById('mul-wave').value;"
              "const vol=parseFloat(document.getElementById('mul-vol').value)/100;"
              "mulSeq.push({freq:freq,dur:dur,wave:wave,vol:vol});"
              "mulRenderSeq();});\n"
            "document.getElementById('mul-clr').addEventListener('click',function(){"
              "mulSeq=[];mulRenderSeq();"
              "document.getElementById('mul-status').textContent='Sequenza svuotata';});\n"
            /* Anteprima tono singolo */
            "document.getElementById('mul-prev').addEventListener('click',function(){"
              "const ctx=getMulCtx();"
              "const freq=parseFloat(document.getElementById('mul-freq').value)||440;"
              "const dur=parseInt(document.getElementById('mul-dur').value)||500;"
              "const wave=document.getElementById('mul-wave').value;"
              "const vol=parseFloat(document.getElementById('mul-vol').value)/100;"
              "const osc=ctx.createOscillator();"
              "const gain=ctx.createGain();"
              "osc.type=wave;osc.frequency.value=freq;gain.gain.value=vol;"
              "osc.connect(gain);gain.connect(mulAnalyser);"
              "mulStartDraw();"
              "osc.start();"
              "setTimeout(function(){"
                "gain.gain.setTargetAtTime(0,ctx.currentTime,0.01);"
                "setTimeout(function(){osc.stop();mulStopDraw();},40);"
              "},dur);});\n"
            /* Riproduci sequenza */
            "document.getElementById('mul-play').addEventListener('click',async function(){"
              "if(mulPlaying||!mulSeq.length){"
                "document.getElementById('mul-status').textContent="
                  "mulPlaying?'Gi\xc3\xa0 in riproduzione':'Aggiungi almeno un tono';"
                "return;}"
              "mulPlaying=true;"
              "document.getElementById('mul-play').disabled=true;"
              "const ctx=getMulCtx();"
              "mulStartDraw();\n"
              "for(let i=0;i<mulSeq.length;i++){"
                "if(!mulPlaying)break;"
                "const t=mulSeq[i];"
                "document.getElementById('mul-status').textContent="
                  "'Tono '+(i+1)+'/'+mulSeq.length+' \xe2\x80\x94 '+t.freq+' Hz \xc2\xb7 '+t.wave;"
                "const osc=ctx.createOscillator();"
                "const gain=ctx.createGain();"
                "osc.type=t.wave;osc.frequency.value=t.freq;gain.gain.value=t.vol;"
                "osc.connect(gain);gain.connect(mulAnalyser);"
                "osc.start();"
                "await new Promise(function(r){setTimeout(r,t.dur);});"
                "gain.gain.setTargetAtTime(0,ctx.currentTime,0.008);"
                "await new Promise(function(r){setTimeout(r,30);});"
                "osc.stop();}\n"
              "mulPlaying=false;"
              "document.getElementById('mul-play').disabled=false;"
              "document.getElementById('mul-status').textContent='\xe2\x9c\x94 Fine sequenza';"
              "mulStopDraw();});\n"
            "document.getElementById('mul-stop').addEventListener('click',function(){"
              "mulPlaying=false;"
              "document.getElementById('mul-status').textContent='Fermato';"
              "mulStopDraw();});\n"
            /* Oscilloscopio + spettro */
            "function mulStartDraw(){"
              "if(mulAnimId)return;"
              "const cvO=document.getElementById('mul-osc');"
              "const cvF=document.getElementById('mul-fq');"
              "const ctxO=cvO.getContext('2d');"
              "const ctxF=cvF.getContext('2d');"
              "const bufLen=mulAnalyser.frequencyBinCount;"
              "const dTime=new Uint8Array(bufLen);"
              "const dFreq=new Uint8Array(bufLen);"
              "function draw(){"
                "mulAnimId=requestAnimationFrame(draw);"
                "const W=cvO.offsetWidth||cvO.width||300;"
                "cvO.width=W;cvO.height=90;"
                "cvF.width=W;cvF.height=70;"
                "const acc=getComputedStyle(document.documentElement)"
                  ".getPropertyValue('--acc').trim()||'#6c63ff';"
                "const bg=getComputedStyle(document.documentElement)"
                  ".getPropertyValue('--inp').trim()||'#0f1117';"
                "mulAnalyser.getByteTimeDomainData(dTime);"
                "mulAnalyser.getByteFrequencyData(dFreq);"
                /* oscilloscopio */
                "ctxO.fillStyle=bg;ctxO.fillRect(0,0,W,90);"
                "ctxO.strokeStyle=acc;ctxO.lineWidth=2;ctxO.beginPath();"
                "for(let i=0;i<bufLen;i++){"
                  "const x=(i/bufLen)*W;"
                  "const y=((dTime[i]/128)-1)*40+45;"
                  "i===0?ctxO.moveTo(x,y):ctxO.lineTo(x,y);}"
                "ctxO.stroke();"
                /* linea centrale di riferimento */
                "ctxO.strokeStyle='rgba(255,255,255,0.06)';ctxO.lineWidth=1;"
                "ctxO.beginPath();ctxO.moveTo(0,45);ctxO.lineTo(W,45);ctxO.stroke();"
                /* spettro frequenze */
                "ctxF.fillStyle=bg;ctxF.fillRect(0,0,W,70);"
                "const bw=W/bufLen*2;"
                "for(let i=0;i<bufLen/2;i++){"
                  "const h=(dFreq[i]/255)*70;"
                  "ctxF.fillStyle=acc;"
                  "ctxF.globalAlpha=0.85;"
                  "ctxF.fillRect(i*bw,70-h,Math.max(bw-1,1),h);}"
                "ctxF.globalAlpha=1;}"
              "draw();}\n"
            "function mulStopDraw(){"
              "if(mulAnimId){cancelAnimationFrame(mulAnimId);mulAnimId=null;}"
              /* svuota canvas */
              "const cvO=document.getElementById('mul-osc');"
              "const cvF=document.getElementById('mul-fq');"
              "const ctxO=cvO.getContext('2d');"
              "const ctxF=cvF.getContext('2d');"
              "const bg=getComputedStyle(document.documentElement)"
                ".getPropertyValue('--inp').trim()||'#0f1117';"
              "ctxO.fillStyle=bg;ctxO.fillRect(0,0,cvO.width,cvO.height);"
              "ctxF.fillStyle=bg;ctxF.fillRect(0,0,cvF.width,cvF.height);}\n"
            /* ── Sessioni Chat: auto-restore + storico permanente ── */
            "var SES_SESS_KEY='plx-sess-current';"
            "var SES_TTL=600000;\n"  /* 10 minuti in ms */
            /* raccoglie le bolle UI visibili */
            "function sesCollectUI(){"
              "var msgs=L.querySelectorAll('.msg');"
              "var ui=[];"
              "msgs.forEach(function(d){"
                "ui.push({role:d.classList.contains('user')?'user':'ai',"
                  "content:d.textContent});});"
              "return ui;}\n"
            /* Salva la sessione corrente in sessionStorage con timestamp */
            "function sesSaveCurrent(){"
              "if(!H.length)return;"
              "sessionStorage.setItem(SES_SESS_KEY,"
                "JSON.stringify({ts:Date.now(),messages:sesCollectUI(),history:H}));}\n"
            /* Ripristina la sessione dal sessionStorage se entro TTL */
            "function sesRestoreCurrent(){"
              "var raw=sessionStorage.getItem(SES_SESS_KEY);"
              "if(!raw)return;"
              "try{"
                "var obj=JSON.parse(raw);"
                "if(!obj||!obj.ts||!obj.messages)return;"
                "if(Date.now()-obj.ts>SES_TTL){"
                  "sessionStorage.removeItem(SES_SESS_KEY);return;}"
                "L.innerHTML='';"
                "obj.messages.forEach(function(m){"
                  "var d=document.createElement('div');"
                  "d.className='msg '+(m.role==='user'?'user':'ai');"
                  "d.textContent=m.content;"
                  "L.appendChild(d);});"
                "L.scrollTop=L.scrollHeight;"
                "if(Array.isArray(obj.history)){"
                  "H.length=0;"
                  "obj.history.forEach(function(x){H.push(x);});}"
              "}catch(e){}}\n"
            /* Rende la lista sessioni nel drawer */
            "function sesRenderList(){"
              "var list=document.getElementById('ses-list');"
              "list.innerHTML='';"
              "var keys=[];"
              "for(var i=0;i<localStorage.length;i++){"
                "var k=localStorage.key(i);"
                "if(k&&k.indexOf('plx-session-')===0)keys.push(k);}"
              "keys.sort().reverse();"
              "if(!keys.length){"
                "list.innerHTML='<div id=\"ses-empty\">Nessuna sessione salvata</div>';"
                "return;}"
              "keys.forEach(function(k){"
                "var raw2=localStorage.getItem(k);"
                "if(!raw2)return;"
                "try{"
                  "var obj=JSON.parse(raw2);"
                  "var item=document.createElement('div');item.className='ses-item';"
                  "var nameSpan=document.createElement('span');nameSpan.className='ses-name';"
                  "nameSpan.textContent=obj.name||k;nameSpan.title=obj.name||k;"
                  "var capturedK=k;"
                  "var delBtn=document.createElement('button');delBtn.className='ses-del';"
                  "delBtn.textContent='\xf0\x9f\x97\x91';delBtn.title='Elimina';"
                  "delBtn.addEventListener('click',function(e){"
                    "e.stopPropagation();localStorage.removeItem(capturedK);sesRenderList();});"
                  "item.appendChild(nameSpan);item.appendChild(delBtn);"
                  "item.addEventListener('click',function(){"
                    "sesLoadSession(capturedK);closeDrw();});"
                  "list.appendChild(item);"
                "}catch(e){}});}\n"
            /* Carica una sessione dal localStorage */
            "function sesLoadSession(k){"
              "var raw3=localStorage.getItem(k);"
              "if(!raw3)return;"
              "try{"
                "var obj=JSON.parse(raw3);"
                "if(!obj||!obj.messages)return;"
                "L.innerHTML='';"
                "obj.messages.forEach(function(m){"
                  "var d=document.createElement('div');"
                  "d.className='msg '+(m.role==='user'?'user':'ai');"
                  "d.textContent=m.content;"
                  "L.appendChild(d);});"
                "L.scrollTop=L.scrollHeight;"
                "H.length=0;"
                "if(Array.isArray(obj.history)){"
                  "obj.history.forEach(function(x){H.push(x);});}"
                "sesSaveCurrent();"
              "}catch(e){}}\n"
            /* Bottone Salva: chiede nome, salva in localStorage */
            "document.getElementById('ses-save').addEventListener('click',function(){"
              "if(!H.length){alert('Nessun messaggio da salvare.');return;}"
              "var nome=prompt('Nome sessione:','Chat '+new Date().toLocaleString('it-IT'));"
              "if(!nome||!nome.trim())return;"
              "nome=nome.trim();"
              "var key='plx-session-'+Date.now();"
              "localStorage.setItem(key,JSON.stringify({name:nome,messages:sesCollectUI(),history:H}));"
              "sesRenderList();"
              "closeDrw();});\n"
            /* Bottone Nuova chat */
            "document.getElementById('ses-new').addEventListener('click',function(){"
              "if(H.length&&!confirm('Iniziare una nuova chat? I messaggi non salvati andranno persi.'))return;"
              "L.innerHTML='';"
              "H.length=0;"
              "sessionStorage.removeItem(SES_SESS_KEY);"
              "closeDrw();});\n"
            /* Bottone Esporta JSON */
            "document.getElementById('ses-exp').addEventListener('click',function(){"
              "if(!H.length){alert('Nessun messaggio da esportare.');return;}"
              "var data={exported:new Date().toISOString(),model:curModel,"
                "messages:sesCollectUI(),history:H};"
              "var bl=new Blob([JSON.stringify(data,null,2)],{type:'application/json'});"
              "var a=document.createElement('a');"
              "a.href=URL.createObjectURL(bl);"
              "a.download='prismalux-chat-'+Date.now()+'.json';"
              "a.click();URL.revokeObjectURL(a.href);"
              "closeDrw();});\n"
            /* Ripristina all'avvio (entro 10 min) */
            "sesRestoreCurrent();\n"
            /* Aggiorna la lista ogni volta che si apre il drawer */
            "document.getElementById('ham').removeEventListener('click',openDrw);\n"
            "document.getElementById('ham').addEventListener('click',function(){"
              "sesRenderList();openDrw();});\n"
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

/* ── /api/math — calcoli matematici con Python/sympy/mpmath ─────────────── */

QString LanServer::buildMathPythonCode(const QString& action, const QJsonObject& req)
{
    if (action == "constant") {
        const QString key    = req["key"].toString();
        const int     digits = qBound(10, req["digits"].toInt(100), 100000);

        struct Map { const char* k; const char* expr; const char* label; };
        static const Map table[] = {
            { "pi",          "mp.pi",       "pi greco" },
            { "e",           "mp.e",        "e (numero di Eulero)" },
            { "phi",         "mp.phi",      "phi (sezione aurea)" },
            { "sqrt2",       "mp.sqrt(2)",  "sqrt(2)" },
            { "sqrt3",       "mp.sqrt(3)",  "sqrt(3)" },
            { "sqrt5",       "mp.sqrt(5)",  "sqrt(5)" },
            { "euler_gamma", "mp.euler",    "gamma (Eulero-Mascheroni)" },
            { "ln2",         "mp.log(2)",   "ln(2)" },
            { "catalan",     "mp.catalan",  "C (costante di Catalan)" },
        };
        QString mpExpr, label;
        for (const auto& m : table) {
            if (key == m.k) { mpExpr = m.expr; label = m.label; break; }
        }
        if (mpExpr.isEmpty()) return {};

        return QString(
            "from mpmath import mp\n"
            "mp.dps = %1 + 10\n"
            "val = %2\n"
            "s = mp.nstr(val, %1, strip_zeros=False)\n"
            "print('%3')\n"
            "print(s)\n"
            "print()\n"
            "print('Cifre richieste: %1')\n"
        ).arg(digits).arg(mpExpr).arg(label);
    }

    if (action == "all_constants") {
        const int digits = qBound(10, req["digits"].toInt(100), 1000);
        return QString(
            "from mpmath import mp\n"
            "mp.dps = %1 + 10\n"
            "consts = [\n"
            "    ('pi greco', mp.pi),\n"
            "    ('e (numero di Eulero)', mp.e),\n"
            "    ('phi (sezione aurea)', mp.phi),\n"
            "    ('sqrt(2)', mp.sqrt(2)),\n"
            "    ('sqrt(3)', mp.sqrt(3)),\n"
            "    ('gamma (Eulero-Mascheroni)', mp.euler),\n"
            "    ('ln(2)', mp.log(2)),\n"
            "    ('C (costante di Catalan)', mp.catalan),\n"
            "]\n"
            "for nome, val in consts:\n"
            "    s = mp.nstr(val, %1, strip_zeros=False)\n"
            "    print(f'{nome}')\n"
            "    print(f'  {s}')\n"
            "    print()\n"
        ).arg(digits);
    }

    if (action == "nth") {
        const QString type = req["type"].toString();
        bool ok = false;
        const long long N  = req["n"].toVariant().toLongLong(&ok);
        if (!ok || N < 1) return {};

        if (type == "pi_digit") {
            return QString(
                "from mpmath import mp\n"
                "mp.dps = %1 + 20\n"
                "s = mp.nstr(mp.pi, %1 + 10)\n"
                "digits = s.replace('3.', '').replace('.', '')\n"
                "if %1 <= len(digits):\n"
                "    d = digits[%1 - 1]\n"
                "    print(f'La {%1}-esima cifra decimale di \\u03c0 \\xe8: {d}')\n"
                "    ctx = digits[max(0,%1-6):%1+5]\n"
                "    pos_in_ctx = min(%1-1, 5)\n"
                "    print(f'Contesto: ...{ctx[:pos_in_ctx]}[{d}]{ctx[pos_in_ctx+1:]}...')\n"
                "else:\n"
                "    print('N troppo grande.')\n"
            ).arg(N);
        }
        if (type == "e_digit") {
            return QString(
                "from mpmath import mp\n"
                "mp.dps = %1 + 20\n"
                "s = mp.nstr(mp.e, %1 + 10)\n"
                "digits = s.replace('2.', '').replace('.', '')\n"
                "if %1 <= len(digits):\n"
                "    d = digits[%1 - 1]\n"
                "    print(f'La {%1}-esima cifra decimale di e \\xe8: {d}')\n"
                "    ctx = digits[max(0,%1-6):%1+5]\n"
                "    pos_in_ctx = min(%1-1, 5)\n"
                "    print(f'Contesto: ...{ctx[:pos_in_ctx]}[{d}]{ctx[pos_in_ctx+1:]}...')\n"
                "else:\n"
                "    print('N troppo grande.')\n"
            ).arg(N);
        }
        if (type == "prime") {
            return QString(
                "from sympy import prime\n"
                "import time\n"
                "t = time.time()\n"
                "p = prime(%1)\n"
                "elapsed = time.time() - t\n"
                "print(f'Il {%1}-esimo numero primo \\xe8:')\n"
                "print(f'  p({%1}) = {p}')\n"
                "print(f'  ({len(str(p))} cifre, calcolato in {elapsed:.3f}s)')\n"
            ).arg(N);
        }
        if (type == "fib") {
            return QString(
                "from mpmath import mp, fib\n"
                "mp.dps = 50\n"
                "import time\n"
                "t = time.time()\n"
                "f = int(fib(%1))\n"
                "elapsed = time.time() - t\n"
                "s = str(f)\n"
                "print(f'Il {%1}-esimo numero di Fibonacci:')\n"
                "if len(s) <= 200:\n"
                "    print(f'  F({%1}) = {s}')\n"
                "else:\n"
                "    print(f'  F({%1}) = {s[:80]}...')\n"
                "    print(f'  ...{s[-20:]}')\n"
                "print(f'  ({len(s)} cifre, calcolato in {elapsed:.3f}s)')\n"
            ).arg(N);
        }
        if (type == "fact") {
            return QString(
                "from sympy import factorial\n"
                "import time\n"
                "t = time.time()\n"
                "f = factorial(%1)\n"
                "elapsed = time.time() - t\n"
                "s = str(f)\n"
                "print(f'{%1}! =')\n"
                "if len(s) <= 300:\n"
                "    print(f'  {s}')\n"
                "else:\n"
                "    print(f'  {s[:100]}...')\n"
                "    print(f'  ...{s[-30:]}')\n"
                "print(f'  ({len(s)} cifre, calcolato in {elapsed:.3f}s)')\n"
            ).arg(N);
        }
        if (type == "pow2") {
            return QString(
                "import time\n"
                "t = time.time()\n"
                "v = 2 ** %1\n"
                "elapsed = time.time() - t\n"
                "s = str(v)\n"
                "print(f'2^{%1} =')\n"
                "if len(s) <= 300:\n"
                "    print(f'  {s}')\n"
                "else:\n"
                "    print(f'  {s[:100]}...')\n"
                "    print(f'  ...{s[-30:]}')\n"
                "print(f'  ({len(s)} cifre, calcolato in {elapsed:.3f}s)')\n"
            ).arg(N);
        }
        if (type == "pi_block") {
            return QString(
                "from mpmath import mp\n"
                "mp.dps = %1 + 10\n"
                "s = mp.nstr(mp.pi, %1 + 5)\n"
                "print(f'Le prime {%1} cifre di \\u03c0:')\n"
                "print(s[:%1+2])\n"
            ).arg(N);
        }
        if (type == "phi_block") {
            return QString(
                "from mpmath import mp\n"
                "mp.dps = %1 + 10\n"
                "s = mp.nstr(mp.phi, %1 + 5)\n"
                "print(f'Le prime {%1} cifre di \\u03c6 (sezione aurea):')\n"
                "print(s[:%1+2])\n"
            ).arg(N);
        }
        return {};
    }

    if (action == "expr") {
        const QString expr = req["expr"].toString().trimmed();
        if (expr.isEmpty()) return {};
        const int prec = qBound(10, req["prec"].toInt(50), 10000);
        /* L'espressione viene passata via sys.stdin — nessuna interpolazione nel codice */
        return QString(
            "import sys\n"
            "from sympy import *\n"
            "from sympy import N as Neval\n"
            "from mpmath import mp\n"
            "mp.dps = %1\n"
            "x, y, z, t = symbols('x y z t')\n"
            "n = symbols('n', positive=True, integer=True)\n"
            "_expr_str = sys.stdin.read().strip()\n"
            "try:\n"
            "    result = sympify(_expr_str)\n"
            "except Exception as ex:\n"
            "    print('Errore parsing espressione:', ex); sys.exit(1)\n"
            "print('Espressione:   ', result)\n"
            "try:\n"
            "    simp = simplify(result)\n"
            "    if simp != result: print('Semplificata:  ', simp)\n"
            "except: pass\n"
            "try:\n"
            "    num = Neval(result, %1)\n"
            "    print(f'Valore numerico ({%1} cifre):')\n"
            "    print(f'  {num}')\n"
            "except Exception as ex:\n"
            "    print(f'  (valore numerico non disponibile: {ex})')\n"
        ).arg(prec);
    }

    if (action == "simplify") {
        const QString expr = req["expr"].toString().trimmed();
        if (expr.isEmpty()) return {};
        const int prec = qBound(10, req["prec"].toInt(50), 10000);
        /* L'espressione viene passata via sys.stdin — nessuna interpolazione nel codice */
        return QString(
            "import sys\n"
            "from sympy import *\n"
            "from mpmath import mp\n"
            "mp.dps = %1\n"
            "x = symbols('x')\n"
            "_expr_str = sys.stdin.read().strip()\n"
            "try:\n"
            "    expr = sympify(_expr_str)\n"
            "except Exception as ex:\n"
            "    print('Errore parsing espressione:', ex); sys.exit(1)\n"
            "print('Espressione:  ', expr)\n"
            "print('Semplificata: ', simplify(expr))\n"
            "print('Fattorizzata: ', factor(expr))\n"
            "try:\n"
            "    print('Valore numerico:', N(expr, %1))\n"
            "except: pass\n"
        ).arg(prec);
    }

    if (action == "sequence_sympy") {
        const QJsonArray seqArr = req["seq"].toArray();
        if (seqArr.isEmpty()) return {};
        const int nxt = qBound(1, req["next"].toInt(5), 50);

        QString listStr = "[";
        for (int i = 0; i < seqArr.size(); ++i) {
            const double v = seqArr[i].toDouble();
            const long long iv = static_cast<long long>(v);
            if (static_cast<double>(iv) == v)
                listStr += QString::number(iv);
            else
                listStr += QString::number(v, 'g', 17);
            if (i < seqArr.size() - 1) listStr += ", ";
        }
        listStr += "]";

        return QString(
            "from sympy import symbols, interpolating_poly, factor, simplify, Integer, nsimplify\n"
            "from sympy import factorint, isprime, fibonacci as fib\n"
            "import sys\n"
            "seq = %1\n"
            "n = symbols('n')\n"
            "N = len(seq)\n"
            "print('Sequenza:', seq)\n"
            "print(f'Termini: {N}')\n"
            "print()\n"
            "try:\n"
            "    pts = list(enumerate(seq, 1))\n"
            "    poly = interpolating_poly(N, n, pts)\n"
            "    fpoly = factor(simplify(poly))\n"
            "    print('Formula polinomiale (interpolazione):')\n"
            "    print(f'  a(n) = {fpoly}')\n"
            "    print()\n"
            "    print('Termini successivi:')\n"
            "    for i in range(N+1, N+%2+1):\n"
            "        print(f'  a({i}) = {fpoly.subs(n, i)}')\n"
            "except Exception as e:\n"
            "    print(f'Interpolazione fallita: {e}')\n"
            "print()\n"
            "diffs = [seq[i+1]-seq[i] for i in range(len(seq)-1)]\n"
            "diffs2 = [diffs[i+1]-diffs[i] for i in range(len(diffs)-1)] if len(diffs)>1 else []\n"
            "print(f'Prime differenze:  {diffs}')\n"
            "if diffs2: print(f'Seconde differenze: {diffs2}')\n"
        ).arg(listStr).arg(nxt);
    }

    return {};
}

void LanServer::handleMath(QTcpSocket* sock, const Session& s)
{
    const QJsonObject req    = QJsonDocument::fromJson(s.body).object();
    const QString     action = req["action"].toString();

    const QString pyCode = buildMathPythonCode(action, req);
    if (pyCode.isEmpty()) {
        const QByteArray err = R"({"error":"action non riconosciuta o parametri mancanti"})";
        sendJson(sock, err);
        return;
    }

    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start("python3", QStringList{"-c", pyCode});
    if (!proc.waitForStarted(3000)) {
        sendJson(sock, R"({"error":"python3 non trovato o non avviato"})");
        return;
    }
    /* Per le action "expr"/"simplify" l'espressione viene passata via stdin
       (il codice Python usa sys.stdin.read()) invece di essere interpolata nel codice. */
    if (action == "expr" || action == "simplify") {
        const QString expr = req["expr"].toString().trimmed();
        proc.write(expr.toUtf8());
    }
    proc.closeWriteChannel();
    proc.waitForFinished(30000);
    const QString out  = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    const int     code = proc.exitCode();

    QJsonObject resp;
    if (code != 0)
        resp["error"] = out.isEmpty() ? "Errore Python (exit code non zero)" : out;
    else
        resp["result"] = out;
    sendJson(sock, QJsonDocument(resp).toJson(QJsonDocument::Compact));
}

/* ── /api/graphviz — renderizza DOT con graphviz dot -Tpng ──────────────── */

void LanServer::handleGraphviz(const Session& s)
{
    const QJsonObject req = QJsonDocument::fromJson(s.body).object();
    const QString dot = req["dot"].toString().trimmed();
    if (dot.isEmpty()) {
        sendError(s.socket, 400, "dot field required");
        return;
    }

    QProcess proc;
    proc.setProcessChannelMode(QProcess::SeparateChannels);
    proc.start("dot", QStringList{"-Tpng"});
    if (!proc.waitForStarted(3000)) {
        QJsonObject err;
        err["error"] = "graphviz (dot) non trovato. Installa graphviz sul server desktop.";
        sendJson(s.socket, QJsonDocument(err).toJson(QJsonDocument::Compact));
        return;
    }
    proc.write(dot.toUtf8());
    proc.closeWriteChannel();
    proc.waitForFinished(15000);

    if (proc.exitCode() != 0) {
        const QString errMsg = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        QJsonObject err;
        err["error"] = errMsg.isEmpty() ? "Errore rendering Graphviz" : errMsg;
        sendJson(s.socket, QJsonDocument(err).toJson(QJsonDocument::Compact));
        return;
    }

    const QByteArray png = proc.readAllStandardOutput();
    QJsonObject resp;
    resp["png"] = QString::fromLatin1(png.toBase64());
    sendJson(s.socket, QJsonDocument(resp).toJson(QJsonDocument::Compact));
}

/* ── /api/whisper — trascrizione audio via whisper.cpp o whisper CLI ────── */

void LanServer::handleWhisper(const Session& s)
{
    static constexpr int kMaxWhisperBytes = 25 * 1024 * 1024;
    if (s.body.size() > kMaxWhisperBytes) {
        sendError(s.socket, 413, "File audio troppo grande (max 25 MB)");
        return;
    }

    {
        const int headerCt = [&]() -> int {
            const QByteArray lower = s.body.left(512).toLower();
            return lower.indexOf("content-type:");
        }();
        if (headerCt >= 0) {
            const int lf = s.body.indexOf('\n', headerCt);
            const QByteArray partCt = s.body.mid(headerCt + 13, lf - headerCt - 13).trimmed().toLower();
            const bool isAudio = partCt.startsWith("audio/")
                              || partCt.startsWith("video/")
                              || partCt.contains("octet-stream");
            if (!isAudio) {
                sendError(s.socket, 415, "Tipo file non supportato — richiesto audio/*");
                return;
            }
        }
    }

    /* Cerca il file audio nel multipart body.
       Per semplicità: il client invia multipart/form-data con campo "audio".
       Estraiamo i byte grezzi dal body cercando la sequenza dopo il doppio CRLF
       dell'header part e prima del boundary finale. */
    const QByteArray ct = [&]() -> QByteArray {
        /* cerca Content-Type nella sessione */
        const int cti = s.buf.indexOf("Content-Type:");
        if (cti < 0) return {};
        const int lf = s.buf.indexOf('\n', cti);
        return s.buf.mid(cti + 13, lf - cti - 13).trimmed();
    }();

    /* Estrai boundary */
    const int bi = ct.indexOf("boundary=");
    if (bi < 0) {
        sendError(s.socket, 400, "multipart boundary non trovato");
        return;
    }
    const QByteArray boundary = "--" + ct.mid(bi + 9).trimmed();

    /* Trova inizio dati audio (dopo doppio CRLF dell'header part) */
    const int partStart = s.body.indexOf(boundary);
    if (partStart < 0) {
        sendError(s.socket, 400, "multipart body non valido");
        return;
    }
    const int hdrEnd = s.body.indexOf("\r\n\r\n", partStart);
    if (hdrEnd < 0) {
        sendError(s.socket, 400, "header part non trovato");
        return;
    }
    const int dataStart = hdrEnd + 4;
    const QByteArray endBoundary = "\r\n" + boundary + "--";
    int dataEnd = s.body.indexOf(endBoundary, dataStart);
    if (dataEnd < 0) dataEnd = s.body.size();
    const QByteArray audioData = s.body.mid(dataStart, dataEnd - dataStart);

    if (audioData.isEmpty()) {
        sendError(s.socket, 400, "file audio vuoto");
        return;
    }

    /* Salva in file temporaneo */
    const QString tmpPath = QDir::tempPath() + "/prismalux_wsp_" +
                            QString::number(QDateTime::currentMSecsSinceEpoch()) + ".wav";
    QFile tmpFile(tmpPath);
    if (!tmpFile.open(QIODevice::WriteOnly)) {
        sendError(s.socket, 500, "impossibile scrivere file temporaneo");
        return;
    }
    tmpFile.write(audioData);
    tmpFile.close();

    /* Prova prima whisper-cpp (whisper), poi whisper CLI Python */
    QStringList candidates = {"whisper-cpp", "whisper"};
    QString whisperBin;
    for (const QString& c : candidates) {
        QProcess test;
        test.start(c, {"--help"});
        if (test.waitForStarted(1000)) { whisperBin = c; test.kill(); break; }
    }

    if (whisperBin.isEmpty()) {
        QFile::remove(tmpPath);
        QJsonObject err;
        err["error"] = "whisper non trovato. Installa whisper.cpp o openai-whisper sul server desktop.";
        sendJson(s.socket, QJsonDocument(err).toJson(QJsonDocument::Compact));
        return;
    }

    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    if (whisperBin == "whisper") {
        /* openai-whisper: whisper file.wav --model tiny --output_format txt --output_dir /tmp */
        proc.start(whisperBin, QStringList{tmpPath, "--model", "tiny",
                                           "--output_format", "txt",
                                           "--output_dir", QDir::tempPath()});
    } else {
        /* whisper.cpp: whisper-cpp -m model.bin -f file.wav */
        proc.start(whisperBin, QStringList{"-f", tmpPath});
    }

    if (!proc.waitForStarted(5000)) {
        QFile::remove(tmpPath);
        sendError(s.socket, 500, "impossibile avviare whisper");
        return;
    }
    proc.waitForFinished(120000); /* max 2 minuti */
    QFile::remove(tmpPath);

    QString text = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();

    /* Se openai-whisper ha scritto un file .txt separato, leggiamolo */
    if (text.isEmpty() || whisperBin == "whisper") {
        const QString txtPath = QDir::tempPath() + "/" +
            QFileInfo(tmpPath).completeBaseName() + ".txt";
        QFile txtFile(txtPath);
        if (txtFile.open(QIODevice::ReadOnly)) {
            text = QString::fromUtf8(txtFile.readAll()).trimmed();
            txtFile.close();
            QFile::remove(txtPath);
        }
    }

    QJsonObject resp;
    if (text.isEmpty())
        resp["error"] = "Trascrizione vuota o fallita";
    else
        resp["text"] = text;
    sendJson(s.socket, QJsonDocument(resp).toJson(QJsonDocument::Compact));
}

/* ── /katex/ — serve KaTeX da disco locale ───────────────────────────────── */

void LanServer::handleKatex(Session& s)
{
    const QString relPath = s.path.mid(7);  /* strip "/katex/" */

    static const QRegularExpression kTraversal(QStringLiteral("\\.\\."));
    if (relPath.isEmpty() || kTraversal.match(relPath).hasMatch()) {
        sendError(s.socket, 400, "Bad Request");
        return;
    }

    static const QStringList kSearchDirs = {
        QStringLiteral("/usr/share/javascript/katex"),
        QStringLiteral("/usr/share/katex"),
    };

    QString filePath;
    for (const QString& base : kSearchDirs) {
        const QString candidate = base + "/" + relPath;
        if (QFileInfo::exists(candidate)) {
            filePath = candidate;
            break;
        }
    }

    if (filePath.isEmpty()) {
        sendError(s.socket, 404, "KaTeX file not found");
        return;
    }

    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        sendError(s.socket, 500, "Cannot read KaTeX file");
        return;
    }
    const QByteArray data = f.readAll();
    f.close();

    const QString ext = QFileInfo(filePath).suffix().toLower();
    const char* mime  = "application/octet-stream";
    if      (ext == "css")  mime = "text/css; charset=utf-8";
    else if (ext == "js")   mime = "application/javascript; charset=utf-8";
    else if (ext == "woff") mime = "font/woff";
    else if (ext == "woff2")mime = "font/woff2";
    else if (ext == "ttf")  mime = "font/ttf";

    QByteArray resp = httpOkHeader(mime);
    resp += "Cache-Control: max-age=86400\r\n";
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
                          : (code == 413) ? "413 Request Entity Too Large"
                          : (code == 415) ? "415 Unsupported Media Type"
                          : (code == 401) ? "401 Unauthorized"
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
        m_streamSock->disconnectFromHost();
    }
    m_streamSock = nullptr;
    m_genMode    = false;
    serveLlmQueue();
}

void LanServer::serveLlmQueue()
{
    /* Rimuovi richieste con socket già chiuso */
    while (!m_llmQueue.isEmpty() && !m_llmQueue.head().sock)
        m_llmQueue.dequeue();

    if (m_llmQueue.isEmpty() || m_streamSock || m_ai->busy()) return;

    PendingLlmRequest req = m_llmQueue.dequeue();
    if (!req.sock) { serveLlmQueue(); return; } /* socket chiuso nel frattempo */

    if (!req.model.isEmpty() && req.model != m_ai->model())
        m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), req.model);

    m_streamSock = req.sock;

    disconnect(m_ai, &AiClient::token,    this, &LanServer::onAiToken);
    disconnect(m_ai, &AiClient::finished, this, &LanServer::onAiFinished);
    disconnect(m_ai, &AiClient::error,    this, &LanServer::onAiError);
    connect(m_ai, &AiClient::token,    this, &LanServer::onAiToken);
    connect(m_ai, &AiClient::finished, this, &LanServer::onAiFinished);
    connect(m_ai, &AiClient::error,    this, &LanServer::onAiError);

    req.sock->write(httpStreamHeader());
    req.sock->flush();

    if (req.isGenerate) {
        m_genMode = true;
        m_ai->generate(req.genSystem, req.genPrompt, AiClient::QueryAuto);
    } else {
        m_genMode = false;
        m_ai->chat(req.systemPrompt, req.userMsg, req.history, AiClient::QueryAuto);
    }
}

QByteArray LanServer::httpOkHeader(const char* contentType) const
{
    QByteArray h = "HTTP/1.1 200 OK\r\n";
    h += "Content-Type: ";
    h += contentType;
    h += "\r\nX-Content-Type-Options: nosniff\r\n";
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
    h += "X-Content-Type-Options: nosniff\r\n";
    h += "X-Frame-Options: DENY\r\n";
    h += "Referrer-Policy: no-referrer\r\n";
    h += "X-Accel-Buffering: no\r\n";
    h += "Connection: close\r\n\r\n";
    return h;
}

/* ── /api/sync ────────────────────────────────────────────────────────────────
   GET  → esporta knowledge + lista ultime sessioni chat (JSON)
   POST → riceve dati mobile (quiz stats, note) e li salva in mobile_sync.json
   ─────────────────────────────────────────────────────────────────────────── */
void LanServer::handleSync(const Session& s)
{
    if (s.method == "GET") {
        /* Leggi knowledge utente */
        QString knowledge;
        {
            QFile f(P::userKnowledgePath());
            if (f.open(QIODevice::ReadOnly | QIODevice::Text))
                knowledge = QString::fromUtf8(f.readAll());
        }

        /* Lista ultime 5 sessioni chat (files in ~/.prismalux_chats/) */
        QJsonArray sessions;
        const QString chatsDir = QDir::homePath() + "/.prismalux_chats";
        QDir dir(chatsDir);
        if (dir.exists()) {
            QFileInfoList files = dir.entryInfoList(
                QStringList() << "*.json",
                QDir::Files, QDir::Time);
            int count = 0;
            for (const QFileInfo& fi : std::as_const(files)) {
                if (count++ >= 5) break;
                QFile f(fi.absoluteFilePath());
                if (!f.open(QIODevice::ReadOnly)) continue;
                const QJsonObject sess =
                    QJsonDocument::fromJson(f.readAll()).object();
                QJsonObject item;
                item["id"]    = sess.value("id").toString();
                item["title"] = sess.value("title").toString();
                item["date"]  = fi.lastModified().toString(Qt::ISODate);
                sessions.append(item);
            }
        }

        QJsonObject resp;
        resp["version"]     = "2.x";
        resp["knowledge"]   = knowledge;
        resp["sessions"]    = sessions;
        resp["synced_at"]   = QDateTime::currentDateTime().toString(Qt::ISODate);
        sendJson(s.socket, QJsonDocument(resp).toJson(QJsonDocument::Compact));

    } else if (s.method == "POST") {
        /* Ricevi dati mobile e salva in mobile_sync.json */
        const QJsonObject body = QJsonDocument::fromJson(s.body).object();
        if (body.isEmpty()) {
            sendError(s.socket, 400, "body JSON vuoto");
            return;
        }

        const QString syncPath = P::root() + "/KNOWLEDGE_USER/mobile_sync.json";
        QDir().mkpath(QFileInfo(syncPath).absolutePath());

        /* Leggi sync esistente e mergia */
        QJsonObject existing;
        {
            QFile f(syncPath);
            if (f.open(QIODevice::ReadOnly))
                existing = QJsonDocument::fromJson(f.readAll()).object();
        }

        /* Mergia quiz_stats: somma correct/total per materia */
        if (body.contains("quiz_stats")) {
            QJsonObject inStats = body["quiz_stats"].toObject();
            QJsonObject curStats = existing["quiz_stats"].toObject();
            for (const QString& key : inStats.keys()) {
                QJsonObject cur = curStats[key].toObject();
                QJsonObject inc = inStats[key].toObject();
                cur["total"]   = cur["total"].toInt() + inc["total"].toInt();
                cur["correct"] = cur["correct"].toInt() + inc["correct"].toInt();
                curStats[key]  = cur;
            }
            existing["quiz_stats"] = curStats;
        }

        /* Salva note e altri campi opzionali */
        if (body.contains("notes"))
            existing["notes"] = body["notes"];

        existing["last_sync"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        existing["device"]    = body.value("device").toString("mobile");

        QFile out(syncPath);
        if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            sendError(s.socket, 500, "impossibile scrivere mobile_sync.json");
            return;
        }
        out.write(QJsonDocument(existing).toJson());

        QJsonObject resp;
        resp["status"]    = "ok";
        resp["synced_at"] = existing["last_sync"].toString();
        sendJson(s.socket, QJsonDocument(resp).toJson(QJsonDocument::Compact));

    } else {
        sendError(s.socket, 405, "Method Not Allowed");
    }
}
