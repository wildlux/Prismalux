#include "https_redirect_server.h"
#if QT_CONFIG(ssl)
#include <QHostAddress>
#include <QSocketNotifier>
#include <QTcpSocket>
#include <QTimer>
#include <cerrno>
#ifdef Q_OS_WIN
#  include <winsock2.h>
#else
#  include <sys/socket.h>
#  include <unistd.h>
#endif

namespace {

const char kFdProperty[]         = "plxProbeFd";     ///< fd grezzo sul QSocketNotifier
const char kRedirectedProperty[] = "plxRedirected";  ///< 301 già inviato su questo socket

/* Sbircia il primo byte senza consumarlo: >0 ok, 0 = peer ha chiuso,
 * <0 = niente dati (EAGAIN) o errore vero — distinti dal chiamante. */
int peekFirstByte(qintptr fd, char* out)
{
#ifdef Q_OS_WIN
    return ::recv(static_cast<SOCKET>(fd), out, 1, MSG_PEEK);
#else
    return static_cast<int>(::recv(static_cast<int>(fd), out, 1,
                                   MSG_PEEK | MSG_DONTWAIT));
#endif
}

bool peekWouldBlock()
{
#ifdef Q_OS_WIN
    return WSAGetLastError() == WSAEWOULDBLOCK;
#else
    return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR;
#endif
}

void closeFd(qintptr fd)
{
#ifdef Q_OS_WIN
    ::closesocket(static_cast<SOCKET>(fd));
#else
    ::close(static_cast<int>(fd));
#endif
}

} // namespace

HttpsRedirectServer::HttpsRedirectServer(QObject* parent)
    : QSslServer(parent)
{
}

void HttpsRedirectServer::incomingConnection(qintptr socketDescriptor)
{
    /* Niente socket Qt finché non si sa se è TLS o HTTP in chiaro: il peek
     * OS-level lascia i byte nel kernel, così QSslServer riceve poi il
     * ClientHello integro. */
    auto* notifier = new QSocketNotifier(socketDescriptor,
                                         QSocketNotifier::Read, this);
    notifier->setProperty(kFdProperty,
                          static_cast<qlonglong>(socketDescriptor));
    connect(notifier, &QSocketNotifier::activated,
            this, &HttpsRedirectServer::onProbeActivated);

    auto* timer = new QTimer(notifier);   /* muore col notifier */
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout,
            this, &HttpsRedirectServer::onProbeTimeout);
    timer->start(kProbeTimeoutMs);
}

void HttpsRedirectServer::onProbeActivated()
{
    auto* notifier = qobject_cast<QSocketNotifier*>(sender());
    if (!notifier) return;
    const qintptr fd =
        static_cast<qintptr>(notifier->property(kFdProperty).toLongLong());

    char first = 0;
    const int n = peekFirstByte(fd, &first);
    if (n < 0) {
        if (peekWouldBlock()) return;     /* risveglio spurio — si riprova */
        notifier->setEnabled(false);
        notifier->deleteLater();
        closeFd(fd);
        return;
    }
    notifier->setEnabled(false);
    notifier->deleteLater();
    if (n == 0) { closeFd(fd); return; }  /* chiuso senza mandare un byte */

    if (static_cast<quint8>(first) == 0x16) {
        QSslServer::incomingConnection(fd);   /* handshake TLS standard */
        return;
    }
    adoptPlainSocket(fd);
}

void HttpsRedirectServer::adoptPlainSocket(qintptr fd)
{
    auto* sock = new QTcpSocket(this);
    if (!sock->setSocketDescriptor(fd)) {
        delete sock;
        closeFd(fd);
        return;
    }
    connect(sock, &QTcpSocket::readyRead,
            this, &HttpsRedirectServer::onRedirectReadyRead);
    connect(sock, &QTcpSocket::disconnected,
            this, &HttpsRedirectServer::onRedirectDisconnected);

    auto* timer = new QTimer(sock);       /* muore col socket */
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout,
            this, &HttpsRedirectServer::onProbeTimeout);
    timer->start(kProbeTimeoutMs);

    processRedirect(sock);   /* i primi byte sono quasi sempre già arrivati */
}

void HttpsRedirectServer::onRedirectReadyRead()
{
    auto* sock = qobject_cast<QTcpSocket*>(sender());
    if (!sock) return;
    if (sock->property(kRedirectedProperty).toBool()) {
        sock->readAll();   /* scarta un body in ritardo: chiudere con dati
                              non letti manderebbe RST e perderebbe il 301 */
        return;
    }
    processRedirect(sock);
}

void HttpsRedirectServer::onRedirectDisconnected()
{
    if (auto* sock = qobject_cast<QTcpSocket*>(sender()))
        sock->deleteLater();
}

void HttpsRedirectServer::onProbeTimeout()
{
    auto* timer = qobject_cast<QTimer*>(sender());
    if (!timer) return;
    if (auto* notifier = qobject_cast<QSocketNotifier*>(timer->parent())) {
        const qintptr fd = static_cast<qintptr>(
            notifier->property(kFdProperty).toLongLong());
        notifier->setEnabled(false);
        notifier->deleteLater();
        closeFd(fd);
        return;
    }
    if (auto* sock = qobject_cast<QTcpSocket*>(timer->parent())) {
        sock->abort();
        sock->deleteLater();
    }
}

void HttpsRedirectServer::processRedirect(QTcpSocket* sock)
{
    const QByteArray req = sock->peek(kMaxHeaderBytes);
    const int hdrEnd = req.indexOf("\r\n\r\n");
    if (hdrEnd < 0 && req.size() < kMaxHeaderBytes)
        return;                               /* header incompleti: aspetta */

    sock->setProperty(kRedirectedProperty, true);
    sock->readAll();                          /* consuma la richiesta */

    /* Request line: "GET /path HTTP/1.1" */
    const int lineEnd = req.indexOf("\r\n");
    const QList<QByteArray> parts =
        (lineEnd > 0) ? req.left(lineEnd).split(' ') : QList<QByteArray>();
    if (parts.size() < 3 || !parts.at(2).startsWith("HTTP/")) {
        sock->abort();                        /* non è HTTP: chiudi e basta */
        sock->deleteLater();
        return;
    }
    QByteArray target = parts.at(1);
    if (!target.startsWith('/')) target = "/";

    /* Host: (contiene già la porta); fallback: indirizzo locale del socket */
    QByteArray host;
    const QByteArray hdrBlock = (hdrEnd > 0) ? req.left(hdrEnd) : req;
    const QList<QByteArray> lines = hdrBlock.split('\n');
    for (const QByteArray& raw : lines) {
        const QByteArray l = raw.trimmed();
        if (l.left(5).toLower() == "host:") { host = l.mid(5).trimmed(); break; }
    }
    if (host.isEmpty()) {
        const QHostAddress la = sock->localAddress();
        bool isV4 = false;
        const quint32 v4 = la.toIPv4Address(&isV4);
        host = isV4 ? QHostAddress(v4).toString().toUtf8()
                    : ("[" + la.toString().toUtf8() + "]");
        host += ":" + QByteArray::number(sock->localPort());
    }
    /* Anti header-injection nel Location */
    host.replace('\r', QByteArray()).replace('\n', QByteArray())
        .replace(' ', QByteArray());

    sock->write("HTTP/1.1 301 Moved Permanently\r\n"
                "Location: https://" + host + target + "\r\n"
                "Connection: close\r\n"
                "Content-Length: 0\r\n\r\n");
    sock->flush();
    sock->disconnectFromHost();
}

#endif // QT_CONFIG(ssl)
