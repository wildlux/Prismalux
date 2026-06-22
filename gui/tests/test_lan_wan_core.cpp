/* ══════════════════════════════════════════════════════════════
   test_lan_wan_core.cpp — Test LanServer core (no Ollama)

   Categorie:
     CAT-A  timingSafeEqual — constant-time compare
     CAT-B  Token LAN — generazione, save/load, confronto
     CAT-C  Rate limiting — chat e heavy endpoint
     CAT-D  Costruzione LanServer, start/stop, clientCount

   Build:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_lan_wan_core
     ./build_tests/test_lan_wan_core
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QApplication>
#include <QEventLoop>
#include <QTimer>
#include <QTcpSocket>
#include <QElapsedTimer>

#include "../lan_server.h"
#include "../ai_client.h"

/* ══════════════════════════════════════════════════════════════
   CAT-A — timingSafeEqual
   ══════════════════════════════════════════════════════════════ */
class TestTimingSafeEqual : public QObject {
    Q_OBJECT
private slots:

    /* A-1: stringhe identiche → true */
    void uguale() {
        QVERIFY(LanServer::timingSafeEqual("abc123", "abc123"));
    }

    /* A-2: stringhe diverse (stesso len) → false */
    void diversaSamelen() {
        QVERIFY(!LanServer::timingSafeEqual("abc123", "XYZ999"));
    }

    /* A-3: stringhe diverse (len diversa) → false */
    void diversaLen() {
        QVERIFY(!LanServer::timingSafeEqual("short", "longer_string"));
    }

    /* A-4: entrambe vuote → true */
    void entrambeVuote() {
        QVERIFY(LanServer::timingSafeEqual("", ""));
    }

    /* A-5: una vuota, una non → false */
    void unaVuota() {
        QVERIFY(!LanServer::timingSafeEqual("", "x"));
        QVERIFY(!LanServer::timingSafeEqual("x", ""));
    }

    /* A-6: tempo di confronto non cresce linearmente con lunghezza
       (verifica solo che non esploda — non è un benchmark di timing reale) */
    void tempoStabile() {
        const QString a(1024, 'a');
        const QString b(1024, 'b');
        QElapsedTimer t; t.start();
        for (int i = 0; i < 1000; ++i)
            LanServer::timingSafeEqual(a, b);
        const qint64 ms = t.elapsed();
        QVERIFY2(ms < 2000, "timingSafeEqual 1000× non deve richiedere >2s");
    }

    /* A-7: token con caratteri speciali */
    void caratteriSpeciali() {
        const QString tok = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.test==+/";
        QVERIFY(LanServer::timingSafeEqual(tok, tok));
        QVERIFY(!LanServer::timingSafeEqual(tok, tok + "x"));
    }

    /* A-8: token vuoto vs token reale → false */
    void tokenVuotoVsReale() {
        QVERIFY(!LanServer::timingSafeEqual("", "some_real_token_32chars"));
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B — Token LAN (save/load round-trip)
   ══════════════════════════════════════════════════════════════ */
class TestTokenLan : public QObject {
    Q_OBJECT
private slots:

    /* B-1: saveLanToken / loadLanToken round-trip */
    void saveLoadRoundtrip() {
        const QString tok = "test_token_12345_abcdef";
        LanServer::saveLanToken(tok);
        const QString loaded = LanServer::loadLanToken();
        QCOMPARE(loaded, tok);
    }

    /* B-2: loadLanToken restituisce stringa (anche vuota) senza crash */
    void loadNonCrasha() {
        const QString t = LanServer::loadLanToken();
        Q_UNUSED(t);
        QVERIFY(true);
    }

    /* B-3: setAccessToken non crasha */
    void setTokenNonCrasha() {
        AiClient ai;
        LanServer srv(&ai);
        srv.setAccessToken("my_secure_token");
        QVERIFY(true);
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-C — Rate limiting (via avvio server + richieste rapide)
   ══════════════════════════════════════════════════════════════ */
class TestRateLimit : public QObject {
    Q_OBJECT
private:

    static QByteArray makeHttpPost(const QString& path, const QByteArray& body,
                                   const QString& token = "")
    {
        QByteArray req;
        req += "POST " + path.toLatin1() + " HTTP/1.0\r\n";
        req += "Host: 127.0.0.1\r\n";
        req += "Content-Type: application/json\r\n";
        if (!token.isEmpty())
            req += "Authorization: Bearer " + token.toLatin1() + "\r\n";
        req += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
        req += "Connection: close\r\n\r\n";
        req += body;
        return req;
    }

    /* invia una richiesta sincrona e restituisce la risposta HTTP */
    static QByteArray sendSync(int port, const QByteArray& req, int timeoutMs = 2000)
    {
        QTcpSocket sock;
        sock.connectToHost("127.0.0.1", static_cast<quint16>(port));
        if (!sock.waitForConnected(timeoutMs)) return {};
        sock.write(req);
        sock.waitForBytesWritten(timeoutMs);
        QByteArray resp;
        while (sock.waitForReadyRead(timeoutMs))
            resp += sock.readAll();
        return resp;
    }

private slots:

    /* C-1: server si avvia su porta nel range test (49200-49299) */
    void serverAvvio() {
        AiClient ai;
        LanServer srv(&ai);
        /* Porta 0 non è supportata nel path manuale Linux SO_REUSEADDR;
           usiamo una porta nel range test (non-privilegiato, raramente usata) */
        const quint16 testPort = 49200;
        const bool ok = srv.start(testPort);
        if (!ok) QSKIP("Porta 49200 già in uso — skip");
        QVERIFY(srv.isRunning());
        QVERIFY(srv.port() > 0);
        srv.stop();
        QVERIFY(!srv.isRunning());
    }

    /* C-2: richiesta senza token restituisce 401 quando token configurato */
    void senzaToken401() {
        AiClient ai;
        LanServer srv(&ai);
        srv.setAccessToken("secret123");
        const quint16 testPort = 49201;
        if (!srv.start(testPort)) QSKIP("Porta 49201 già in uso — skip");
        const int p = srv.port();

        const QByteArray req = makeHttpPost("/api/chat",
                                            R"({"message":"hi","model":"x"})");
        const QByteArray resp = sendSync(p, req);
        QVERIFY2(resp.contains("401") || resp.contains("403"),
                 "richiesta senza token deve ricevere 401/403");
        srv.stop();
    }

    /* C-3: clientCount() == 0 senza connessioni */
    void clientCountZero() {
        AiClient ai;
        LanServer srv(&ai);
        const quint16 testPort = 49202;
        if (!srv.start(testPort)) QSKIP("Porta 49202 già in uso — skip");
        QCOMPARE(srv.clientCount(), 0);
        srv.stop();
    }

    /* C-4: richiesta su / restituisce 200 (pagina web) */
    void rootReturn200() {
        AiClient ai;
        LanServer srv(&ai);
        const quint16 testPort = 49203;
        if (!srv.start(testPort)) QSKIP("Porta 49203 già in uso — skip");
        const int p = srv.port();

        QTcpSocket sock;
        sock.connectToHost("127.0.0.1", static_cast<quint16>(p));
        QVERIFY(sock.waitForConnected(2000));
        sock.write("GET / HTTP/1.0\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n");
        sock.waitForBytesWritten(1000);
        QByteArray resp;
        while (sock.waitForReadyRead(2000)) resp += sock.readAll();
        QVERIFY2(resp.contains("200") || resp.contains("301"),
                 "GET / deve dare 200 o redirect");
        srv.stop();
    }

    /* C-5: richiesta /api/tags senza token (se nessun token configurato) → JSON */
    void apiTagsSenzaToken() {
        AiClient ai;
        LanServer srv(&ai);
        const quint16 testPort = 49204;
        if (!srv.start(testPort)) QSKIP("Porta 49204 già in uso — skip");
        const int p = srv.port();

        QTcpSocket sock;
        sock.connectToHost("127.0.0.1", static_cast<quint16>(p));
        QVERIFY(sock.waitForConnected(2000));
        sock.write("GET /api/tags HTTP/1.0\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n");
        sock.waitForBytesWritten(1000);
        QByteArray resp;
        while (sock.waitForReadyRead(3000)) resp += sock.readAll();
        QVERIFY2(!resp.isEmpty(), "/api/tags deve rispondere");
        srv.stop();
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-D — Costruzione, start/stop multipli
   ══════════════════════════════════════════════════════════════ */
class TestLanServerLifecycle : public QObject {
    Q_OBJECT
private slots:

    /* D-1: start su porta già usata → non crasha (può fallire l'avvio) */
    void doppioStartSamPort() {
        AiClient ai;
        LanServer srv1(&ai), srv2(&ai);
        const quint16 testPort = 49210;
        if (!srv1.start(testPort)) QSKIP("Porta 49210 già in uso — skip");
        const int p = srv1.port();
        /* Non ci interessa se srv2 riesce — deve solo non crashare */
        srv2.start(static_cast<quint16>(p));
        srv1.stop();
        srv2.stop();
        QVERIFY(true);
    }

    /* D-2: stop senza start non crasha */
    void stopSenzaStart() {
        AiClient ai;
        LanServer srv(&ai);
        srv.stop();
        QVERIFY(true);
    }

    /* D-3: start/stop/start riavvia correttamente */
    void restartCorretto() {
        AiClient ai;
        LanServer srv(&ai);
        const quint16 testPort = 49211;
        if (!srv.start(testPort)) QSKIP("Porta 49211 già in uso — skip");
        srv.stop();
        QVERIFY(!srv.isRunning());
        if (!srv.start(testPort)) QSKIP("Porta 49211 non disponibile per restart — skip");
        QVERIFY(srv.isRunning());
        srv.stop();
    }

    /* D-4: segnale statusChanged emesso a start e stop */
    void statusChangedEmesso() {
        AiClient ai;
        LanServer srv(&ai);
        QSignalSpy spy(&srv, &LanServer::statusChanged);
        const quint16 testPort = 49212;
        srv.start(testPort);
        srv.stop();
        QVERIFY2(spy.count() >= 2,
                 "statusChanged deve essere emesso almeno 2 volte (start + stop)");
    }
};

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    int ret = 0;
    { TestTimingSafeEqual   t; ret |= QTest::qExec(&t, argc, argv); }
    { TestTokenLan          t; ret |= QTest::qExec(&t, argc, argv); }
    { TestRateLimit         t; ret |= QTest::qExec(&t, argc, argv); }
    { TestLanServerLifecycle t; ret |= QTest::qExec(&t, argc, argv); }
    return ret;
}
#include "test_lan_wan_core.moc"
