#include "lan_server.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>
#include <QSet>
#include <QFile>
#include <QTemporaryFile>
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
        return P::lanTokenPath();
    if (key == QLatin1String("cloud_api_key"))
        return QDir::homePath() + "/.prismalux/cloud_api.key";  /* percorso storico */
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
    /* TLS self-signed abilitato di default (m_tlsRequested=true). Fallback HTTP
     * se openssl non è disponibile. Il browser mostrerà un avviso "non sicuro"
     * che l'utente deve accettare una volta — normale per certificati self-signed. */
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

bool    LanServer::isRunning()   const {
    if (m_server->isListening()) return true;
#if QT_CONFIG(ssl)
    if (m_sslServer && m_sslServer->isListening()) return true;
#endif
    return false;
}
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
            } else if (key == "authorization") {
                s.authHeader = val;
            } else if (key == "cookie") {
                /* Estrai p_session dal Cookie header (può contenere più cookie) */
                for (const QStringView kv : QStringView(val).split(';')) {
                    const QStringView t = kv.trimmed();
                    if (t.startsWith(u"p_session="))
                        s.cookieSession = t.mid(10).toString();
                }
            }
        }
        s.headersDone = true;
    }

    /* Accumula body fino a contentLength */
    if ((int)s.buf.size() < s.contentLength) return;

    s.body = s.buf.left(s.contentLength);
    s.buf.remove(0, s.contentLength);

    processSession(s);

    /* Reset per eventuale richiesta successiva sulla stessa connessione.
     * processSession() può chiudere il socket (es. 401 + disconnectFromHost)
     * emettendo disconnected() in modo sincrono → onClientDisconnected() erasa
     * la sessione → s sarebbe dangling. Guard obbligatorio. */
    if (!m_sessions.contains(sock)) return;
    s.method.clear(); s.path.clear();
    s.contentLength = 0;
    s.headersDone   = false;
    s.body.clear();
    s.authHeader.clear();
    s.authHeaderFallback.clear();
    s.cookieSession.clear();
    s.queryString.clear();
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

    /* deny-by-default: solo le risorse statiche/web sono pubbliche;
       qualsiasi altro percorso (compreso ogni futuro /api/*) richiede token.
       Aggiungere qui nuovi percorsi pubblici, non in una lista protetti. */
    /* Risorse statiche pubbliche — non richiedono token.
       /web è protetta: i browser vi arrivano tramite QR code con ?token= */
    const bool isPublic = (s.path == "/"        ||
                           s.path == "/index"   ||
                           s.path == "/download"||
                           s.path.startsWith("/katex/")     ||
                           s.path.startsWith("/bootstrap/"));

    /* Scambio token→cookie per /web: il browser arriva da QR code con ?token=TOKEN.
       Se il token è valido emettiamo Set-Cookie e facciamo redirect 302 /web senza
       query string — così la cronologia e la barra indirizzi del browser non mostrano
       mai il token. L'app Android legge il token dal testo del QR direttamente, non
       via questo flusso browser, quindi non è impattata. */
    if (s.path == "/web" && s.method == "GET" &&
        !s.authHeaderFallback.isEmpty() && !m_accessToken.isEmpty()) {
        const QString expected  = "Bearer " + m_accessToken;
        const QString presented = "Bearer " + s.authHeaderFallback;
        if (timingSafeEqual(presented, expected)) {
            /* Token valido: emetti cookie HttpOnly + redirect a /web (no query) */
            QString safe = m_accessToken;
            /* RFC 6265 §4.1: cookie-value = VCHAR esclusi CTL, ';', '"', '\', spazio, virgola */
            static const QRegularExpression kCookieUnsafe(QStringLiteral("[^\\x21-\\x7E]|[;,\"\\\\]"));
            safe.remove(kCookieUnsafe);
            QByteArray resp = "HTTP/1.1 302 Found\r\n"
                              "Location: /web\r\n";
            resp += "Set-Cookie: p_session=" + safe.toUtf8()
                    + "; HttpOnly; SameSite=Strict; Path=/; Max-Age=86400\r\n";
            resp += "Cache-Control: no-store\r\n"
                    "Content-Length: 0\r\n\r\n";
            s.socket->write(resp);
            s.socket->flush();
            return;  /* non serve disconnettere: il browser fa GET /web subito dopo */
        }
        /* Token non valido: cade nel check auth standard sotto → 401 */
    }

    /* Auth check: tutto ciò che non è esplicitamente pubblico richiede
       header Authorization: Bearer TOKEN oppure cookie p_session.
       Il cookie p_session è accettato SOLO per /web (match esatto) — i sub-path
       futuri e tutte le API devono usare header Authorization. */
    if (!isPublic && !m_accessToken.isEmpty()) {
        const QString expected  = "Bearer " + m_accessToken;
        const QString effective = !s.authHeader.isEmpty()
                                  ? s.authHeader
                                  : (s.path == "/web" && !s.cookieSession.isEmpty()
                                     ? "Bearer " + s.cookieSession
                                     : QString());
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

    if (!isPublic && !s.isApiClient) {
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
    } else if (s.path == "/api/finanza/tfr" && s.method == "POST") {
        handleFinanzaTfr(s);
    } else if (s.path == "/api/git" && s.method == "POST") {
        if (checkHeavyRateLimit(s)) return;
        handleGitApi(s);
    } else if (s.path == "/api/wiki" && s.method == "GET") {
        if (checkHeavyRateLimit(s)) return;
        handleWikiApi(s);
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
    } else if (s.path == "/api/datetime" && s.method == "GET") {
        /* Data/ora locale + UTC/GMT + timezone + Unix timestamp.
           Accessibile da web app, Android, MCP esterni e qualsiasi client HTTP autenticato. */
        const QDateTime local      = QDateTime::currentDateTime();
        const QDateTime utc        = local.toUTC();
        const int       offsetSec  = local.timeZone().offsetFromUtc(local);
        const int       offsetH    = qAbs(offsetSec) / 3600;
        const int       offsetM    = (qAbs(offsetSec) % 3600) / 60;
        const QString   sign       = (offsetSec >= 0) ? "+" : "-";
        const QString   utcOffset  = QString("UTC%1%2:%3")
                                     .arg(sign)
                                     .arg(offsetH, 2, 10, QLatin1Char('0'))
                                     .arg(offsetM, 2, 10, QLatin1Char('0'));
        QJsonObject dtj;
        dtj["local"]              = local.toString("yyyy-MM-dd HH:mm:ss");
        dtj["local_iso"]          = local.toString(Qt::ISODate);
        dtj["utc"]                = utc.toString("yyyy-MM-dd HH:mm:ss");
        dtj["utc_iso"]            = utc.toString(Qt::ISODate);
        dtj["timezone"]           = local.timeZone().abbreviation(local);
        dtj["utc_offset"]         = utcOffset;
        dtj["utc_offset_seconds"] = offsetSec;
        dtj["unix"]               = local.toSecsSinceEpoch();
        dtj["day_of_week"]        = local.toString("dddd");
        dtj["week_number"]        = local.date().weekNumber();
        sendJson(s.socket, QJsonDocument(dtj).toJson(QJsonDocument::Compact));
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

        /* C-1: allowlist host — /api/mcp non deve fungere da proxy verso host arbitrari */
        static const QSet<QString> kAllowedMcpHosts = { "127.0.0.1", "localhost" };
        if (!kAllowedMcpHosts.contains(host)) {
            sendError(s.socket, 400, "MCP host non consentito — solo 127.0.0.1/localhost");
            return;
        }
        if (port < 1024 || port > 65535) {
            sendError(s.socket, 400, "MCP port fuori range");
            return;
        }

        const QJsonObject mcpReq = wrapper["request"].toObject();
        const QByteArray mcpBody = QJsonDocument(mcpReq).toJson(QJsonDocument::Compact);

        const QByteArray httpReq = QByteArray("POST / HTTP/1.1\r\n")
            + "Host: " + host.toLatin1() + ":" + QByteArray::number(port) + "\r\n"
            + "Content-Type: application/json\r\n"
            + "Content-Length: " + QByteArray::number(mcpBody.size()) + "\r\n"
            + "Connection: close\r\n\r\n"
            + mcpBody;

        /* Connessione asincrona al server MCP — non blocca l'event loop Qt.
         * mcp ha parent this: viene distrutto con LanServer se serve cleanup. */
        auto* mcp       = new QTcpSocket(this);
        auto* mcpTimer  = new QTimer(mcp);          /* distrutto insieme a mcp */
        auto clientSock = QPointer<QTcpSocket>(s.socket);
        auto buf        = QSharedPointer<QByteArray>::create();
        auto responded  = QSharedPointer<bool>::create(false);

        static constexpr int kMcpMaxResponseBytes = 10 * 1024 * 1024; /* H-1: cap 10 MB */

        /* Invia la request HTTP non appena il socket è connesso */
        connect(mcp, &QTcpSocket::connected, mcp, [mcp, httpReq]() {
            mcp->write(httpReq);
            mcp->flush();
        });

        /* Accumula chunk; se supera il cap, abort() per innescare disconnected */
        connect(mcp, &QTcpSocket::readyRead, mcp, [mcp, buf, responded]() mutable {
            *buf += mcp->readAll();
            if (!*responded && buf->size() > kMcpMaxResponseBytes)
                mcp->abort();
        });

        /* Invia la risposta al client LAN quando il server MCP chiude la connessione */
        auto finalize = [this, mcp, mcpTimer, buf, clientSock, responded]
                        (bool connErr) mutable {
            mcpTimer->stop();
            if (*responded) return;
            *responded = true;
            if (clientSock) {
                if (connErr && buf->isEmpty()) {
                    sendJson(clientSock, R"({"error":"MCP non raggiungibile","code":-1})");
                } else if (buf->size() > kMcpMaxResponseBytes) {
                    sendError(clientSock, 502, "Risposta MCP troppo grande");
                } else {
                    const int sep = buf->indexOf("\r\n\r\n");
                    const QByteArray body = sep >= 0 ? buf->mid(sep + 4) : *buf;
                    sendJson(clientSock, body.isEmpty()
                        ? QByteArray(R"({"error":"Nessuna risposta dal server MCP"})")
                        : body);
                }
            }
            mcp->deleteLater();
        };

        /* disconnected: fine normale della comunicazione HTTP/1.0 */
        connect(mcp, &QTcpSocket::disconnected, this,
            [finalize]() mutable { finalize(false); });

        /* errorOccurred: connect rifiutata o reset.
           Se lo stato è già UnconnectedState, disconnected non verrà emesso. */
        connect(mcp, &QAbstractSocket::errorOccurred, this,
            [finalize, mcp](QAbstractSocket::SocketError) mutable {
                if (mcp->state() == QAbstractSocket::UnconnectedState)
                    finalize(true);
                /* Altrimenti disconnected viene emesso subito dopo → finalize(false) lì */
            });

        /* Timeout globale: connect (3s) + read (5s) = 8s */
        connect(mcpTimer, &QTimer::timeout, this,
            [finalize, mcp]() mutable {
                mcp->abort();   /* → errorOccurred + eventuale disconnected */
                finalize(true); /* forza risposta se disconnected non arriva */
            });

        mcpTimer->setSingleShot(true);
        mcpTimer->start(8000);
        mcp->connectToHost(host, static_cast<quint16>(port));
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
        if (s.method == "POST" && checkHeavyRateLimit(s)) return; /* M-2 */
        handleSync(s);
    } else if (s.path == "/api/math" && s.method == "POST") {
        handleMath(s.socket, s);
    } else {
        sendJson(s.socket, R"({"status":"ok"})");
    }
}

static const QByteArray kSecHeaders =
    "X-Content-Type-Options: nosniff\r\n"
    "X-Frame-Options: DENY\r\n"
    "X-XSS-Protection: 1; mode=block\r\n"
    "Referrer-Policy: no-referrer\r\n"
    "Permissions-Policy: camera=(), microphone=(), geolocation=(), "
        "payment=(), usb=(), magnetometer=(), gyroscope=()\r\n";

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
    resp += kSecHeaders;
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
    h += "\r\n";
    h += kSecHeaders;
    if (m_useTls)
        h += "Strict-Transport-Security: max-age=31536000\r\n";
    return h;
}

QByteArray LanServer::httpStreamHeader() const
{
    /* Nessun Content-Length né Transfer-Encoding chunked:
       il client legge via readyRead fino alla chiusura del socket. */
    QByteArray h = "HTTP/1.1 200 OK\r\n";
    h += "Content-Type: application/x-ndjson\r\n";
    h += kSecHeaders;
    if (m_useTls)
        h += "Strict-Transport-Security: max-age=31536000\r\n";
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
        /* M-2: limita dimensione notes per evitare scritture unbounded su disco */
        if (body["notes"].toString().size() > 64 * 1024) {
            sendError(s.socket, 413, "notes troppo grandi (max 64 KB)");
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

/* ── /api/finanza/tfr — calcolo TFR (art. 2120 c.c.) ──────────────────────
   POST  { stipendio_annuo: float, anni: int, inflazione: float,
           fondo_pensione: bool }
   ─────────────────────────────────────────────────────────────────────────── */
void LanServer::handleFinanzaTfr(const Session& s)
{
    const QJsonObject req = QJsonDocument::fromJson(s.body).object();

    const double stipendio    = req["stipendio_annuo"].toDouble();
    const int    anni         = qMax(1, req["anni"].toInt());
    const double inflazione   = req["inflazione"].toDouble(2.0);
    const bool   fondoPension = req["fondo_pensione"].toBool(false);

    if (stipendio <= 0) {
        sendError(s.socket, 400, "stipendio_annuo deve essere positivo");
        return;
    }
    if (anni > 50) {
        sendError(s.socket, 400, "anni troppo elevati (max 50)");
        return;
    }

    const double quotaAnnua  = stipendio / 13.5;
    const double tassoRival  = 0.015 + 0.75 * (inflazione / 100.0);
    double fondo = 0.0;

    for (int yr = 1; yr <= anni; ++yr) {
        fondo += quotaAnnua;
        fondo *= (1.0 + tassoRival);
    }

    const double tfrSemplice = quotaAnnua * anni;
    const double plusvalenza = fondo - tfrSemplice;
    const double netto       = fondoPension ? fondo * 0.88 : fondo * 0.77;
    const QString fiscalita  = fondoPension
        ? "Fondo Pensione: imposta sostitutiva 15% (riduzione 0,3%/anno oltre il 15°, min 9%)"
        : "In azienda: tassazione separata art. 17 TUIR, aliquota stimata 23%";

    QJsonObject resp;
    resp["quota_annua"]     = qRound(quotaAnnua * 100.0) / 100.0;
    resp["tasso_rival_pct"] = qRound(tassoRival * 10000.0) / 100.0;
    resp["tfr_semplice"]    = qRound(tfrSemplice * 100.0) / 100.0;
    resp["tfr_rivalutato"]  = qRound(fondo * 100.0) / 100.0;
    resp["plusvalenza"]     = qRound(plusvalenza * 100.0) / 100.0;
    resp["netto_stimato"]   = qRound(netto * 100.0) / 100.0;
    resp["fiscalita"]       = fiscalita;
    resp["anni"]            = anni;
    resp["stipendio_annuo"] = stipendio;
    sendJson(s.socket, QJsonDocument(resp).toJson(QJsonDocument::Compact));
}

/* ── /api/git — esegui sottocomandi git nella cartella progetto ─────────────
   POST  { cmd: "status"|"log"|"diff"|"diff-staged"|"branch" }
   ─────────────────────────────────────────────────────────────────────────── */
void LanServer::handleGitApi(const Session& s)
{
    const QJsonObject req = QJsonDocument::fromJson(s.body).object();
    const QString cmd = req["cmd"].toString().trimmed().toLower();

    /* Allowlist di sottocomandi sicuri (sola lettura) */
    static const QMap<QString, QStringList> kAllowed = {
        {"status",      {"status", "--short"}},
        {"log",         {"log", "--oneline", "-30", "--decorate"}},
        {"diff",        {"diff"}},
        {"diff-staged", {"diff", "--cached"}},
        {"branch",      {"branch", "-a"}},
    };

    if (!kAllowed.contains(cmd)) {
        sendError(s.socket, 400, "cmd non consentito. Usa: status, log, diff, diff-staged, branch");
        return;
    }

    const QStringList args = kAllowed[cmd];
    QProcess proc;
    proc.setWorkingDirectory(P::root());
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start("git", args);
    if (!proc.waitForStarted(3000)) {
        sendError(s.socket, 500, "git non trovato sul server");
        return;
    }
    proc.waitForFinished(10000);

    const QString output = QString::fromUtf8(proc.readAll()).trimmed();
    QJsonObject resp;
    resp["cmd"]    = "git " + args.join(" ");
    resp["output"] = output.isEmpty() ? "(nessun output)" : output;
    resp["exit"]   = proc.exitCode();
    sendJson(s.socket, QJsonDocument(resp).toJson(QJsonDocument::Compact));
}

/* ── /api/wiki — riepilogo Wikipedia via REST API ───────────────────────────
   GET   /api/wiki?q=QUERY&lang=it
   ─────────────────────────────────────────────────────────────────────────── */
void LanServer::handleWikiApi(const Session& s)
{
    /* Estrai q e lang dalla querystring */
    QString q, lang = "it";
    for (const QString& part : s.queryString.split('&')) {
        if (part.startsWith("q="))
            q = QUrl::fromPercentEncoding(part.mid(2).toUtf8()).replace('+', ' ');
        else if (part.startsWith("lang="))
            lang = part.mid(5).toLower();
    }

    q = q.trimmed();
    if (q.isEmpty()) {
        sendError(s.socket, 400, "parametro q mancante");
        return;
    }

    /* Sanifica: solo it/en/fr/de/es/pt/ja/zh */
    static const QStringList kLangs = {"it","en","fr","de","es","pt","ja","zh","ar","ru"};
    if (!kLangs.contains(lang)) lang = "it";

    /* Sanitizza titolo per URL: sostituisce spazi con _ */
    const QString title = QString(q).replace(' ', '_');
    const QString url   = QString("https://%1.wikipedia.org/api/rest_v1/page/summary/%2")
                          .arg(lang, QString(QUrl::toPercentEncoding(title)));

    /* Chiama curl con timeout 8s */
    QProcess proc;
    proc.setProcessChannelMode(QProcess::SeparateChannels);
    proc.start("curl", QStringList{
        "-s", "--max-time", "8",
        "-H", "User-Agent: Prismalux/2.9 (Qt6; Linux)",
        url
    });
    if (!proc.waitForStarted(3000)) {
        sendError(s.socket, 500, "curl non disponibile");
        return;
    }
    proc.waitForFinished(10000);

    const QByteArray raw = proc.readAllStandardOutput();
    const QJsonObject wiki = QJsonDocument::fromJson(raw).object();

    if (wiki.contains("type") && wiki["type"].toString() == "https://mediawiki.org/wiki/HyperSwitch/errors/not_found") {
        QJsonObject err;
        err["error"] = "Voce non trovata su Wikipedia (" + lang + "): " + q;
        sendJson(s.socket, QJsonDocument(err).toJson(QJsonDocument::Compact));
        return;
    }

    QJsonObject resp;
    resp["title"]       = wiki["title"].toString();
    resp["extract"]     = wiki["extract"].toString();
    resp["description"] = wiki["description"].toString();
    resp["url"]         = wiki.value("content_urls").toObject()
                              .value("desktop").toObject()
                              .value("page").toString();
    resp["lang"]        = lang;
    sendJson(s.socket, QJsonDocument(resp).toJson(QJsonDocument::Compact));
}
