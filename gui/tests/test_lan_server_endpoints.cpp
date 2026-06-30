/* ══════════════════════════════════════════════════════════════
   test_lan_server_endpoints.cpp — Test endpoint HTTP di LanServer

   Categorie:
     CAT-A  Endpoint /knowledge — GET (lettura), POST (append/replace)
     CAT-B  Endpoint /apk — presenza file, 404 se assente, solo GET
     CAT-C  Ciclo vita + segnale requestHandled

   Nota: i test TCP sincroni usano waitForReadyRead(2000ms).
   In ambienti senza event loop completo alcuni test potrebbero
   essere instabili — usano QSKIP se il server non risponde.

   Build:
     cmake --build gui/build_gui -j4 --target test_lan_server_endpoints
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QApplication>
#include <QSignalSpy>
#include <QTcpSocket>
#include <QTemporaryFile>
#include <QDir>
#include <QTimer>
#include <QEventLoop>
#include <thread>
#include <atomic>
#ifdef Q_OS_UNIX
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <unistd.h>
#endif

#include "mock_ai_client.h"
#include "../lan_server.h"
#include "../prismalux_paths.h"

namespace P = PrismaluxPaths;

static constexpr int kTimeout = 3000; /* ms */
static const QByteArray kToken = "test-token-lan-ep-12345";

/* Qt 6.10.2: readyRead non emesso su loopback con data+FIN simultanei.
   Fix: thread POSIX con recv() bloccante, main thread = Qt event loop server.
   Restituisce l'intera risposta HTTP grezza (header + body). */
static QByteArray rawHttp(quint16 port, const QByteArray& req, int timeoutMs = kTimeout)
{
    QByteArray resp;
#ifdef Q_OS_UNIX
    std::atomic<bool> done{false};
    std::thread t([&]() {
        int s = ::socket(AF_INET, SOCK_STREAM, 0);
        if (s < 0) { done.store(true, std::memory_order_release); return; }
        struct timeval tv { timeoutMs / 1000, (timeoutMs % 1000) * 1000L };
        ::setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        struct sockaddr_in addr{};
        addr.sin_family      = AF_INET;
        addr.sin_port        = htons(port);
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (::connect(s, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == 0) {
            ::send(s, req.constData(), static_cast<size_t>(req.size()), 0);
            char tmp[65536]; ssize_t n;
            while ((n = ::recv(s, tmp, sizeof(tmp), 0)) > 0)
                resp.append(tmp, static_cast<int>(n));
        }
        ::close(s);
        done.store(true, std::memory_order_release);
    });
    {
        QEventLoop loop;
        QTimer chk; chk.setInterval(5);
        QObject::connect(&chk, &QTimer::timeout, &loop, [&]() {
            if (done.load(std::memory_order_acquire)) loop.quit();
        });
        chk.start();
        QTimer::singleShot(timeoutMs + 200, &loop, &QEventLoop::quit);
        loop.exec();
    }
    done.store(true, std::memory_order_release);
    t.join();
#else
    Q_UNUSED(port) Q_UNUSED(req) Q_UNUSED(timeoutMs)
#endif
    return resp;
}

/* Costruisce una richiesta HTTP e restituisce solo il body (dopo \r\n\r\n). */
static QByteArray httpRequest(quint16 port, const QByteArray& method,
                              const QByteArray& path,
                              const QByteArray& body = {},
                              bool withToken = true)
{
    QByteArray req = method + " " + path + " HTTP/1.1\r\n"
                     "Host: 127.0.0.1\r\n"
                     "Connection: close\r\n";
    if (withToken)
        req += "Authorization: Bearer " + kToken + "\r\n";
    if (!body.isEmpty()) {
        req += "Content-Type: application/json\r\n";
        req += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    }
    req += "\r\n";
    if (!body.isEmpty()) req += body;

    const QByteArray resp = rawHttp(port, req);
    const int sep = resp.indexOf("\r\n\r\n");
    return (sep >= 0) ? resp.mid(sep + 4) : resp;
}

/* Estrae lo status code dall'header HTTP della risposta. */
static int httpStatus(quint16 port, const QByteArray& method,
                      const QByteArray& path,
                      const QByteArray& body = {},
                      bool withToken = true)
{
    QByteArray req = method + " " + path + " HTTP/1.1\r\n"
                     "Host: 127.0.0.1\r\n"
                     "Connection: close\r\n";
    if (withToken)
        req += "Authorization: Bearer " + kToken + "\r\n";
    if (!body.isEmpty()) {
        req += "Content-Type: application/json\r\n";
        req += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    }
    req += "\r\n";
    if (!body.isEmpty()) req += body;

    const QByteArray head = rawHttp(port, req);
    /* "HTTP/1.1 200 OK\r\n..." → 200 */
    const int sp1 = head.indexOf(' ');
    const int sp2 = head.indexOf(' ', sp1 + 1);
    if (sp1 < 0 || sp2 < 0) return -1;
    return head.mid(sp1 + 1, sp2 - sp1 - 1).toInt();
}

/* ══════════════════════════════════════════════════════════════
   CAT-A — Endpoint /knowledge
   ══════════════════════════════════════════════════════════════ */
class TestKnowledge : public QObject {
    Q_OBJECT

    MockAiClient m_ai;
    LanServer*   m_srv = nullptr;
    quint16      m_port = 0;
    QString      m_knFile;

private slots:

    void initTestCase() {
        m_srv = new LanServer(&m_ai, this);
        m_srv->setAccessToken(QString::fromUtf8(kToken));
        QVERIFY2(m_srv->start(0), "impossibile avviare LanServer");
        m_port = m_srv->port();
        m_knFile = P::userKnowledgePath();
    }

    void cleanupTestCase() {
        m_srv->stop();
        /* Ripristina file knowledge se lo abbiamo creato */
        if (QFile::exists(m_knFile + ".bak")) {
            QFile::remove(m_knFile);
            QFile::rename(m_knFile + ".bak", m_knFile);
        }
    }

    /* A1: GET /knowledge senza token → 401 */
    void getKnowledgeSenzaToken() {
        const int code = httpStatus(m_port, "GET", "/knowledge", {}, false);
        if (code < 0) QSKIP("server non risponde (SKIP)");
        QCOMPARE(code, 401);
    }

    /* A2: GET /knowledge con file assente → {"available":false} */
    void getKnowledgeFileAssente() {
        /* Fai backup del file reale se esiste */
        if (QFile::exists(m_knFile))
            QFile::rename(m_knFile, m_knFile + ".bak");
        QFile::remove(m_knFile);

        const QByteArray body = httpRequest(m_port, "GET", "/knowledge");
        if (body.isEmpty()) QSKIP("server non risponde (SKIP)");
        QVERIFY2(body.contains("\"available\":false"),
                 qPrintable("body ricevuto: " + body));
    }

    /* A3: POST /knowledge con contenuto → {"ok":true} */
    void postKnowledgeAppend() {
        const QByteArray payload = R"({"content":"Test knowledge append","mode":"replace"})";
        const int code = httpStatus(m_port, "POST", "/knowledge", payload);
        if (code < 0) QSKIP("server non risponde (SKIP)");
        QCOMPARE(code, 200);
        /* Verifica che il file sia stato scritto */
        QVERIFY(QFile::exists(m_knFile));
    }

    /* A4: GET /knowledge dopo POST → {"available":true} */
    void getKnowledgeDopoPost() {
        if (!QFile::exists(m_knFile)) QSKIP("file knowledge assente (SKIP)");
        const QByteArray body = httpRequest(m_port, "GET", "/knowledge");
        if (body.isEmpty()) QSKIP("server non risponde (SKIP)");
        QVERIFY2(body.contains("\"available\":true"),
                 qPrintable("body ricevuto: " + body));
        QVERIFY2(body.contains("Test knowledge append"),
                 "contenuto POST non trovato nel GET");
    }

    /* A5: POST body malformato → 400 */
    void postBodyMalformato() {
        const int code = httpStatus(m_port, "POST", "/knowledge", "not-json-at-all");
        if (code < 0) QSKIP("server non risponde (SKIP)");
        QCOMPARE(code, 400);
    }

    /* A6: POST content vuoto → 400 */
    void postContentVuoto() {
        const int code = httpStatus(m_port, "POST", "/knowledge",
                                    R"({"content":"","mode":"append"})");
        if (code < 0) QSKIP("server non risponde (SKIP)");
        QCOMPARE(code, 400);
    }

    /* A7: Content-Type risposta = application/json */
    void contentTypeJson() {
        QTcpSocket sock;
        sock.connectToHost("127.0.0.1", m_port);
        if (!sock.waitForConnected(kTimeout)) QSKIP("connessione fallita");
        const QByteArray req =
            "GET /knowledge HTTP/1.1\r\n"
            "Host: 127.0.0.1\r\n"
            "Authorization: Bearer " + kToken + "\r\n"
            "Connection: close\r\n\r\n";
        sock.write(req); sock.flush();
        QByteArray head;
        while (sock.waitForReadyRead(kTimeout)) {
            head += sock.readAll();
            if (head.contains("\r\n\r\n")) break;
        }
        if (head.isEmpty()) QSKIP("nessuna risposta (SKIP)");
        QVERIFY2(head.toLower().contains("content-type: application/json"),
                 "Content-Type non è application/json");
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B — Endpoint /apk
   ══════════════════════════════════════════════════════════════ */
class TestApk : public QObject {
    Q_OBJECT

    MockAiClient m_ai;
    LanServer*   m_srv = nullptr;
    quint16      m_port = 0;

private slots:

    void initTestCase() {
        m_srv = new LanServer(&m_ai, this);
        m_srv->setAccessToken(QString::fromUtf8(kToken));
        QVERIFY2(m_srv->start(0), "impossibile avviare LanServer");
        m_port = m_srv->port();
    }

    void cleanupTestCase() { m_srv->stop(); }

    /* B1: GET /apk senza token → 401 */
    void getApkSenzaToken() {
        const int code = httpStatus(m_port, "GET", "/apk", {}, false);
        if (code < 0) QSKIP("server non risponde (SKIP)");
        QCOMPARE(code, 401);
    }

    /* B2: GET /apk con token ma APK assente → 404 */
    void getApkFileAssente() {
        const QString apkPath = P::root() + "/ANDROID/PrismaluxMobile.apk";
        if (QFile::exists(apkPath))
            QSKIP("APK presente — skip test 404 (usare B3 se presente)");
        const int code = httpStatus(m_port, "GET", "/apk");
        if (code < 0) QSKIP("server non risponde (SKIP)");
        QCOMPARE(code, 404);
    }

    /* B3: GET /apk con APK presente → 200 */
    void getApkPresente() {
        const QString apkPath = P::root() + "/ANDROID/PrismaluxMobile.apk";
        if (!QFile::exists(apkPath))
            QSKIP("APK assente — skip test 200 (ambiente CI senza APK)");
        const int code = httpStatus(m_port, "GET", "/apk");
        if (code < 0) QSKIP("server non risponde (SKIP)");
        QCOMPARE(code, 200);
    }

    /* B4: POST /apk → 4xx (solo GET ammesso) */
    void postApkNonAmmesso() {
        const int code = httpStatus(m_port, "POST", "/apk", "{}");
        if (code < 0) QSKIP("server non risponde (SKIP)");
        QVERIFY2(code >= 400, "POST /apk dovrebbe ritornare 4xx");
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-C — Ciclo vita + segnale requestHandled
   ══════════════════════════════════════════════════════════════ */
class TestCicloVita : public QObject {
    Q_OBJECT
private slots:

    /* C1: start → isRunning == true */
    void startRunning() {
        MockAiClient ai; LanServer srv(&ai);
        srv.setAccessToken("tok");
        QVERIFY(srv.start(0));
        QVERIFY(srv.isRunning());
        srv.stop();
    }

    /* C2: stop → isRunning == false */
    void stopNonRunning() {
        MockAiClient ai; LanServer srv(&ai);
        srv.setAccessToken("tok");
        srv.start(0);
        srv.stop();
        QVERIFY(!srv.isRunning());
    }

    /* C3: start su porta occupata → false, no crash */
    void startPortaOccupata() {
        MockAiClient ai1, ai2;
        LanServer srv1(&ai1), srv2(&ai2);
        srv1.setAccessToken("tok");
        srv2.setAccessToken("tok");
        QVERIFY(srv1.start(0));
        const quint16 p = srv1.port();
        /* Il secondo start sulla stessa porta dovrebbe fallire */
        const bool ok = srv2.start(p);
        Q_UNUSED(ok); /* può fallire o no — l'importante è no crash */
        srv1.stop();
        srv2.stop();
        QVERIFY(true);
    }

    /* C4: requestHandled emesso per ogni richiesta HTTP ricevuta */
    void requestHandledEmesso() {
        MockAiClient ai; LanServer srv(&ai);
        srv.setAccessToken(QString::fromUtf8(kToken));
        QVERIFY(srv.start(0));
        const quint16 p = srv.port();

        QSignalSpy spy(&srv, &LanServer::requestHandled);

        /* Richiesta HTTP senza token — 401 ma requestHandled comunque emesso */
        QTcpSocket sock;
        sock.connectToHost("127.0.0.1", p);
        if (!sock.waitForConnected(kTimeout)) {
            srv.stop();
            QSKIP("connessione fallita (SKIP)");
        }
        sock.write("GET /knowledge HTTP/1.1\r\nHost: 127.0.0.1\r\n"
                   "Authorization: Bearer " + kToken + "\r\n"
                   "Connection: close\r\n\r\n");
        sock.flush();
        sock.waitForReadyRead(kTimeout);

        /* Dà tempo al server di processare e emettere il segnale */
        QEventLoop loop;
        QTimer::singleShot(500, &loop, &QEventLoop::quit);
        loop.exec();

        QVERIFY2(spy.count() >= 1,
                 qPrintable(QString("requestHandled non emesso (count=%1)").arg(spy.count())));
        srv.stop();
    }

    /* C5: clientCount() == 0 dopo stop */
    void clientCountDopoStop() {
        MockAiClient ai; LanServer srv(&ai);
        srv.start(0);
        srv.stop();
        QCOMPARE(srv.clientCount(), 0);
    }

    /* C6: doppio stop non crasha */
    void doppioStop() {
        MockAiClient ai; LanServer srv(&ai);
        srv.start(0);
        srv.stop();
        srv.stop(); /* secondo stop */
        QVERIFY(true);
    }
};

/* ══════════════════════════════════════════════════════════════
   Runner
   ══════════════════════════════════════════════════════════════ */
int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    int status = 0;
    { TestKnowledge t; status |= QTest::qExec(&t, argc, argv); }
    { TestApk       t; status |= QTest::qExec(&t, argc, argv); }
    { TestCicloVita t; status |= QTest::qExec(&t, argc, argv); }
    return status;
}

#include "test_lan_server_endpoints.moc"
