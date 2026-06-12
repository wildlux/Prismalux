#include "lan_server.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QDir>
#include <QSet>
#include <QFile>
#include <QUuid>
#include <QRegularExpression>
#include <QFileInfo>
#include <QTextStream>
#include <QProcess>
#include <QUrlQuery>
#include <QUrl>
#include <cmath>
#include "prismalux_paths.h"
#include "pages/main_jobs_data.h"
#include "widgets/proc_helper.h"
#include "pages/pratico_calcs.h"
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

/* Percorso del file di fallback 0600 per una chiave generica. */
static QString secretFallbackPath(const QString& key)
{
    if (key == QLatin1String("lan_token"))
        return P::lanTokenPath();   /* compatibilità col percorso storico */
    return QDir::homePath() + "/.prismalux/" + key + ".key";
}

void LanServer::saveSecret(const QString& key, const QString& value)
{
#ifdef HAVE_QKEYCHAIN
    QKeychain::WritePasswordJob job(QStringLiteral("Prismalux"));
    job.setAutoDelete(false);
    job.setKey(key);
    job.setTextData(value);
    QEventLoop loop;
    QObject::connect(&job, &QKeychain::WritePasswordJob::finished,
                     &loop, &QEventLoop::quit);
    job.start();
    loop.exec();
    if (job.error() == QKeychain::NoError) {
        QFile::remove(secretFallbackPath(key));   /* rimuove il vecchio file di fallback */
        return;
    }
    qWarning() << "LanServer: QKeychain write failed:" << job.errorString()
               << "— fallback a file 0600";
#endif
    /* Fallback file-based (0600) */
    QDir().mkpath(QDir::homePath() + "/.prismalux");
    QFile f(secretFallbackPath(key));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
    f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    f.write(value.toUtf8());
}

QString LanServer::loadSecret(const QString& key)
{
#ifdef HAVE_QKEYCHAIN
    QKeychain::ReadPasswordJob job(QStringLiteral("Prismalux"));
    job.setAutoDelete(false);
    job.setKey(key);
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
    QFile f(secretFallbackPath(key));
    if (f.open(QIODevice::ReadOnly))
        return QString::fromUtf8(f.readAll()).trimmed();
    return {};
}

void LanServer::deleteSecret(const QString& key)
{
#ifdef HAVE_QKEYCHAIN
    QKeychain::DeletePasswordJob job(QStringLiteral("Prismalux"));
    job.setAutoDelete(false);
    job.setKey(key);
    QEventLoop loop;
    QObject::connect(&job, &QKeychain::DeletePasswordJob::finished,
                     &loop, &QEventLoop::quit);
    job.start();
    loop.exec();
#endif
    QFile::remove(secretFallbackPath(key));
}

void LanServer::saveLanToken(const QString& token) { saveSecret(QStringLiteral("lan_token"), token); }
QString LanServer::loadLanToken()                  { return loadSecret(QStringLiteral("lan_token")); }

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

    const auto r = ProcHelper::run("openssl", {
        "req", "-x509", "-newkey", "rsa:2048", "-nodes",
        "-days", "3650",
        "-keyout", keyPath,
        "-out",    certPath,
        "-subj",   "/CN=Prismalux-LAN"
    }, 10000);
    if (!r.ok) {
        qWarning() << "LanServer: openssl non disponibile — TLS disabilitato";
        return false;
    }
    /* Chiave privata non cifrata (-nodes): permessi 0600 per impedire lettura ad altri utenti */
    QFile::setPermissions(keyPath, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
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
            s.path        = s.path.left(qmark);
            s.queryString = qs;   /* salva query string grezza per handler GET */
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
                        s.path == "/api/whisper"   ||
                        s.path == "/api/file"      ||
                        s.path == "/api/repl"      ||
                        s.path == "/api/finanza/cf"||
                        s.path.startsWith("/api/graph"));

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
    } else if (s.path.startsWith("/bootstrap/") && s.method == "GET") {
        handleBootstrap(s);
    } else if (s.path == "/api/file" && s.method == "POST") {
        if (checkHeavyRateLimit(s)) return;
        handleFileApi(s);
    } else if (s.path == "/api/repl" && s.method == "POST") {
        if (m_headless) { sendError(s.socket, 403, "Disabled in headless mode"); return; }
        if (checkHeavyRateLimit(s)) return;
        handleReplApi(s);
    } else if (s.path == "/api/finanza/cf" && s.method == "POST") {
        handleFinanzaCf(s);
    } else if (s.path.startsWith("/api/graph") && s.method == "GET") {
        handleGraphApi(s);
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
    } else if (s.path == "/api/lavoro" && s.method == "GET") {
        /* Ricerca testuale: GET /api/lavoro?q=QUERY */
        QString q;
        for (const QStringView part : QStringView(s.queryString).split('&')) {
            if (part.startsWith(u"q=")) {
                q = QUrl::fromPercentEncoding(part.mid(2).toString().replace('+', ' ').toLatin1());
                break;
            }
        }
        const QList<Offerta> tutte = offerteFiltrate("tutti", "tutti");
        QJsonArray arr;
        for (const auto& o : tutte) {
            if (!q.isEmpty()) {
                const QString ql = q.toLower();
                const bool match = o.azienda.toLower().contains(ql)
                                || o.ruolo.toLower().contains(ql)
                                || o.sede.toLower().contains(ql)
                                || o.requisiti.toLower().contains(ql);
                if (!match) continue;
            }
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
        /* CV caricato da file locale (PII non versionato) */
        const QString cvFile = QDir::homePath() + "/.prismalux/cv.txt";
        QFile f(cvFile);
        QString cvTxt;
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            cvTxt = QTextStream(&f).readAll().trimmed();
        } else {
            cvTxt = "[CV non configurato — crea il file ~/.prismalux/cv.txt con i tuoi dati]";
        }
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

    QFile f(":/lan/webchat.html");
    if (!f.open(QIODevice::ReadOnly)) {
        sendError(s.socket, 500, "webchat.html not found in resources");
        return;
    }
    QByteArray html = f.readAll();
    html.replace("{{MODEL}}", model);
    html.replace("{{AUTH_HEADERS_JS}}", authHeadersJs);

    QByteArray resp = httpOkHeader("text/html; charset=utf-8");
    resp += "Cache-Control: no-cache, no-store, must-revalidate\r\n";
    resp += "Pragma: no-cache\r\n";
    /* CSP: blocca script/connessioni verso domini esterni (anti-esfiltrazione e
       supply-chain). 'unsafe-inline' resta necessario perché il JS della web chat è
       inline; rimuoverlo richiederebbe estrarre tutto lo script in un file servito. */
    resp += "Content-Security-Policy: default-src 'self'; "
            "script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; "
            "img-src 'self' data:; connect-src 'self'; frame-ancestors 'none'\r\n";
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

    /* Blocca attributi Graphviz che referenziano file locali (disclosure) */
    static const QRegularExpression kDotDangerousAttr(
        R"(\b(image|shapefile|fontpath|imagepath)\s*=)",
        QRegularExpression::CaseInsensitiveOption);
    if (kDotDangerousAttr.match(dot).hasMatch()) {
        sendError(s.socket, 400, "DOT contiene attributi file non consentiti (image/shapefile/fontpath)");
        return;
    }

    QProcess proc;
    proc.setProcessChannelMode(QProcess::SeparateChannels);
    /* -Gimagepath= vuoto impedisce il caricamento di immagini da disco */
    proc.start("dot", QStringList{"-Tpng", "-Gimagepath="});
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

/* ── /api/file — estrai testo da PDF/DOCX/TXT/CSV/codice ────────────────── */

void LanServer::handleFileApi(Session& s)
{
    /* Estrai Content-Type dal buffer grezzo della richiesta (stesso pattern di handleWhisper) */
    const QByteArray ct = [&]() -> QByteArray {
        const int cti = s.buf.indexOf("Content-Type:");
        if (cti < 0) return {};
        const int lf = s.buf.indexOf('\n', cti);
        return s.buf.mid(cti + 13, lf - cti - 13).trimmed();
    }();

    const int boundPos = ct.indexOf("boundary=");
    if (boundPos < 0) { sendError(s.socket, 400, "Missing boundary"); return; }
    const QByteArray boundary = "--" + ct.mid(boundPos + 9).trimmed();

    /* Estrai la parte "file" */
    const int partStart = s.body.indexOf(boundary);
    if (partStart < 0) { sendError(s.socket, 400, "No part found"); return; }
    const int headerEnd = s.body.indexOf("\r\n\r\n", partStart);
    if (headerEnd < 0) { sendError(s.socket, 400, "Malformed part"); return; }
    const QByteArray partHeader = s.body.mid(partStart, headerEnd - partStart);
    const int dataStart = headerEnd + 4;
    const int nextBound = s.body.indexOf("\r\n" + boundary, dataStart);
    const QByteArray fileData = (nextBound > 0)
        ? s.body.mid(dataStart, nextBound - dataStart)
        : s.body.mid(dataStart);

    /* Ricava il filename dall'header della parte */
    QString filename;
    {
        const QString ph = QString::fromUtf8(partHeader);
        const QRegularExpression re(R"re(filename="([^"]+)")re",
                                    QRegularExpression::CaseInsensitiveOption);
        const auto m = re.match(ph);
        if (m.hasMatch()) filename = m.captured(1);
    }

    const QString ext = QFileInfo(filename).suffix().toLower();

    /* Limite dimensione: evita di passare upload enormi ai processi esterni. */
    if (fileData.size() > 25 * 1024 * 1024) {
        sendError(s.socket, 413, "File too large (max 25 MB)");
        return;
    }
    /* Whitelist estensioni: solo formati che gli estrattori sanno gestire. */
    static const QSet<QString> kAllowedExt = {
        "pdf", "docx", "doc", "txt", "csv", "md", "rtf", "odt", "json", "xml", "html", "htm"
    };
    if (!kAllowedExt.contains(ext)) {
        sendError(s.socket, 415, "Unsupported file type: " + ext.toUtf8());
        return;
    }

    /* Salva su file temporaneo */
    const QString tmp = QDir::tempPath() + "/plx_upload_" +
                        QString::number(QDateTime::currentMSecsSinceEpoch()) + "." + ext;
    {
        QFile f(tmp);
        if (!f.open(QIODevice::WriteOnly)) { sendError(s.socket, 500, "Cannot write tmp"); return; }
        f.write(fileData);
    }

    QString text;
    if (ext == "pdf") {
        const auto r = ProcHelper::run("pdftotext", {tmp, "-"}, 15'000);
        text = r.ok ? r.out : r.err;
    } else if (ext == "docx") {
        const QString pyCode =
            "import sys, docx\n"
            "d = docx.Document(sys.argv[1])\n"
            "print('\\n'.join(p.text for p in d.paragraphs))\n";
        const auto r = ProcHelper::runWithInput(
            "python3", QStringList{"-c", pyCode, tmp}, QByteArray{}, 10'000);
        text = r.ok ? r.out : ("Errore: " + r.err);
    } else {
        QFile f(tmp);
        if (f.open(QIODevice::ReadOnly))
            text = QString::fromUtf8(f.readAll());
    }
    QFile::remove(tmp);

    if (text.length() > 60'000)
        text = text.left(60'000) + "\n[... troncato a 60 000 caratteri ...]";

    QJsonObject obj;
    obj["text"]     = text;
    obj["filename"] = filename;
    obj["chars"]    = text.length();
    sendJson(s.socket, QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

/* ── /api/repl — esegui Python/Bash con timeout ──────────────────────────── */

void LanServer::handleReplApi(Session& s)
{
    const QJsonObject body = QJsonDocument::fromJson(s.body).object();
    const QString code = body["code"].toString().trimmed();
    const QString lang = body["lang"].toString().toLower().trimmed();

    if (code.isEmpty()) { sendError(s.socket, 400, "Empty code"); return; }
    if (code.length() > 16'000) { sendError(s.socket, 400, "Code too large"); return; }

    ProcResult r;
    if (lang == "python" || lang == "python3" || lang.isEmpty()) {
        r = ProcHelper::runWithInput("python3", {"-"}, code.toUtf8(), 15'000);
    } else if (lang == "javascript" || lang == "node") {
        r = ProcHelper::runWithInput("node", QStringList{"-e", code}, QByteArray{}, 10'000);
    } else if (lang == "bash") {
        r = ProcHelper::runWithInput("bash", {"-s"}, code.toUtf8(), 10'000);
    } else {
        sendError(s.socket, 400, "Unsupported lang: " + lang.toUtf8()); return;
    }

    QJsonObject obj;
    obj["output"]   = r.out.left(20'000);
    obj["error"]    = r.err.left(4'000);
    obj["exit_code"] = r.code;
    obj["ok"]       = r.ok;
    sendJson(s.socket, QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

/* ── /api/finanza/cf — calcola Codice Fiscale (D.M. 1976) ───────────────── */

void LanServer::handleFinanzaCf(Session& s)
{
    const QJsonObject body = QJsonDocument::fromJson(s.body).object();
    const QString cognome = body["cognome"].toString().trimmed();
    const QString nome    = body["nome"].toString().trimmed();
    const QString data    = body["data"].toString().trimmed();   /* YYYY-MM-DD */
    const QString sesso   = body["sesso"].toString().trimmed().toUpper();
    const QString comune  = body["comune"].toString().trimmed();

    if (cognome.isEmpty() || nome.isEmpty() || data.isEmpty()) {
        sendError(s.socket, 400, "Campi obbligatori: cognome, nome, data, sesso, comune");
        return;
    }

    const QDate nascita = QDate::fromString(data, Qt::ISODate);
    if (!nascita.isValid()) { sendError(s.socket, 400, "Data non valida (usa YYYY-MM-DD)"); return; }

    const bool maschio = (sesso != "F");
    const QString belfiore = PraticoCalcs::cercaBelfiore(comune);

    QJsonObject obj;
    if (belfiore.isEmpty()) {
        obj["error"] = "Comune non trovato nel database Belfiore. Specifica il codice manualmente.";
        obj["belfiore_hint"] = QString();
    } else {
        const QString cf = PraticoCalcs::calcolaCodiceFiscale(cognome, nome, nascita, maschio, belfiore);
        obj["cf"]       = cf;
        obj["belfiore"] = belfiore;
    }
    sendJson(s.socket, QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

/* ── /api/graph — query GraphMemory ─────────────────────────────────────── */

void LanServer::handleGraphApi(Session& s)
{
    if (!m_graphMemory) {
        sendError(s.socket, 503, "GraphMemory non disponibile (avvia Multi-Agente nella GUI)");
        return;
    }

    /* /api/graph/dot → esporta DOT */
    if (s.path.endsWith("/dot")) {
        const QString dot = m_graphMemory->toDot("Prismalux Memory", 150);
        QJsonObject obj;
        obj["dot"] = dot;
        sendJson(s.socket, QJsonDocument(obj).toJson(QJsonDocument::Compact));
        return;
    }

    /* /api/graph/nodes?q=...&limit=... → cerca nodi */
    const QUrlQuery q(s.queryString);
    const QString query = q.queryItemValue("q", QUrl::FullyDecoded).trimmed();
    const int limit = qBound(1, q.queryItemValue("limit").toInt(), 200);
    const int lim   = (limit > 0) ? limit : 50;

    const QVector<GmNode> nodes = query.isEmpty()
        ? m_graphMemory->allNodes().mid(0, lim)
        : m_graphMemory->searchNodes(query, lim);

    QJsonArray arr;
    for (const GmNode& n : nodes) {
        QJsonObject o;
        o["id"]         = n.id;
        o["type"]       = n.type;
        o["label"]      = n.label;
        o["content"]    = n.content.left(400);
        o["importance"] = static_cast<double>(n.importance);
        arr.append(o);
    }
    QJsonObject obj;
    obj["nodes"] = arr;
    obj["total"] = arr.size();
    sendJson(s.socket, QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

/* ── /bootstrap/ — serve Bootstrap da gui/lan_web/ ───────────────────────── */

void LanServer::handleBootstrap(Session& s)
{
    const QString relPath = s.path.mid(11);  /* strip "/bootstrap/" */

    static const QRegularExpression kTraversal(QStringLiteral("\\.\\."));
    if (relPath.isEmpty() || kTraversal.match(relPath).hasMatch()) {
        sendError(s.socket, 400, "Bad Request");
        return;
    }

    const QString filePath = P::root() + "/gui/lan_web/" + relPath;
    if (!QFileInfo::exists(filePath)) {
        sendError(s.socket, 404, "Bootstrap file not found");
        return;
    }

    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        sendError(s.socket, 500, "Cannot read Bootstrap file");
        return;
    }
    const QByteArray data = f.readAll();
    f.close();

    const QString ext = QFileInfo(filePath).suffix().toLower();
    const char* mime  = "application/octet-stream";
    if      (ext == "css") mime = "text/css; charset=utf-8";
    else if (ext == "js")  mime = "application/javascript; charset=utf-8";

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

        const QString syncPath = P::root() + "/TOOL_TIP/KNOWLEDGE_USER/mobile_sync.json";
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
