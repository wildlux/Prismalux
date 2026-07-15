/* ══════════════════════════════════════════════════════════════
   test_lan_wan_core.cpp — Test LanServer core (no Ollama)

   Categorie:
     CAT-A  timingSafeEqual — constant-time compare
     CAT-B  Token LAN — generazione, save/load, confronto
     CAT-C  Rate limiting — chat e heavy endpoint
     CAT-D  Costruzione LanServer, start/stop, clientCount
     CAT-E  LanWanPage — rubrica persone autorizzate (accessList)
     CAT-F  Fallback TLS→HTTP visibile (D-19/T-D19) — stato server + label ambra

   Tecnica CAT-E: #define private public per accedere a m_accessListTable /
   loadAccessList() / saveAccessList() (privati in LanWanPage).

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
#include <QSettings>
#include <thread>
#include <atomic>
#ifdef Q_OS_UNIX
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <unistd.h>
#endif

#include "../lan_server.h"
#include "../ai_client.h"
#include "mock_ai_client.h"

/* Rende accessibili i metodi private di LanWanPage in questo TU. */
#define private public
#define protected public
#include "../pages/main_lan_wan.h"
#undef protected
#undef private

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

    /* Qt 6.10.2: readyRead non emesso su loopback con data+FIN simultanei.
       Fix: thread POSIX con recv() bloccante, main thread = Qt event loop server. */
    static QByteArray sendSync(int port, const QByteArray& req, int timeoutMs = 2000)
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
            addr.sin_port        = htons(static_cast<quint16>(port));
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

        const QByteArray resp = sendSync(p,
            "GET / HTTP/1.0\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n");
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

        const QByteArray resp = sendSync(p,
            "GET /api/tags HTTP/1.0\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n", 3000);
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

/* ══════════════════════════════════════════════════════════════
   CAT-E — LanWanPage: rubrica persone autorizzate (accessList)
   ══════════════════════════════════════════════════════════════ */
class TestAccessList : public QObject {
    Q_OBJECT
private:
    MockAiClient* m_ai = nullptr;
    QByteArray    m_savedRaw;   ///< backup del valore reale dell'utente

    static void insertRow(LanWanPage* page, const QString& name, const QString& added) {
        const int row = page->m_accessListTable->rowCount();
        page->m_accessListTable->insertRow(row);
        page->m_accessListTable->setItem(row, 0, new QTableWidgetItem(name));
        page->m_accessListTable->setItem(row, 1, new QTableWidgetItem(added));
    }

private slots:
    void initTestCase() {
        m_ai = new MockAiClient(this);
        /* Backup del valore reale prima di sporcare QSettings nei test */
        m_savedRaw = QSettings("Prismalux", "GUI").value("lan/accessList").toByteArray();
    }

    void cleanupTestCase() {
        QSettings("Prismalux", "GUI").setValue("lan/accessList", m_savedRaw);
    }

    void init() {
        /* Ogni test parte da rubrica vuota */
        QSettings("Prismalux", "GUI").setValue("lan/accessList", QByteArray());
    }

    /* E-1: saveAccessList() poi loadAccessList() sulla stessa istanza — round-trip */
    void roundTripStessaIstanza() {
        auto* page = new LanWanPage(m_ai);
        insertRow(page, "Mario - telefono", "01/07/2026 10:00");
        page->saveAccessList();

        page->m_accessListTable->setRowCount(0);
        page->loadAccessList();

        QCOMPARE(page->m_accessListTable->rowCount(), 1);
        QCOMPARE(page->m_accessListTable->item(0, 0)->text(), QString("Mario - telefono"));
        QCOMPARE(page->m_accessListTable->item(0, 1)->text(), QString("01/07/2026 10:00"));
        delete page;
    }

    /* E-2: loadAccessList() su QSettings vuota → tabella vuota, no crash */
    void loadVuotoNoCrash() {
        auto* page = new LanWanPage(m_ai);
        page->loadAccessList();
        QCOMPARE(page->m_accessListTable->rowCount(), 0);
        delete page;
    }

    /* E-3: saveAccessList() con tabella vuota → array JSON vuoto persistito */
    void saveVuotoProduceArrayVuoto() {
        auto* page = new LanWanPage(m_ai);
        page->saveAccessList();
        const QJsonDocument doc = QJsonDocument::fromJson(
            QSettings("Prismalux", "GUI").value("lan/accessList").toByteArray());
        QVERIFY(doc.isArray());
        QCOMPARE(doc.array().size(), 0);
        delete page;
    }

    /* E-4: più righe (simula addRow di onAddPersonClicked senza il dialog modale) */
    void multiRigaRoundTrip() {
        auto* page = new LanWanPage(m_ai);
        insertRow(page, "Alice", "01/07/2026 09:00");
        insertRow(page, "Bob",   "01/07/2026 09:05");
        insertRow(page, "Carla", "01/07/2026 09:10");
        page->saveAccessList();

        page->m_accessListTable->setRowCount(0);
        page->loadAccessList();
        QCOMPARE(page->m_accessListTable->rowCount(), 3);
        QCOMPARE(page->m_accessListTable->item(1, 0)->text(), QString("Bob"));
        delete page;
    }

    /* E-5: onRemovePersonClicked() senza selezione → no crash, nessuna riga rimossa */
    void removeSenzaSelezioneNoCrash() {
        auto* page = new LanWanPage(m_ai);
        insertRow(page, "Solo", "01/07/2026 09:00");
        page->m_accessListTable->clearSelection();
        page->m_accessListTable->setCurrentCell(-1, -1);
        page->onRemovePersonClicked();
        QCOMPARE(page->m_accessListTable->rowCount(), 1);
        delete page;
    }

    /* E-6: onRemovePersonClicked() con riga selezionata → rimuove e persiste */
    void removeConSelezionePersiste() {
        auto* page = new LanWanPage(m_ai);
        insertRow(page, "DaTenere", "01/07/2026 09:00");
        insertRow(page, "DaRimuovere", "01/07/2026 09:05");
        page->saveAccessList();

        page->m_accessListTable->setCurrentCell(1, 0);
        page->onRemovePersonClicked();
        QCOMPARE(page->m_accessListTable->rowCount(), 1);
        QCOMPARE(page->m_accessListTable->item(0, 0)->text(), QString("DaTenere"));

        /* Verifica che la rimozione sia stata persistita, non solo in memoria */
        page->m_accessListTable->setRowCount(0);
        page->loadAccessList();
        QCOMPARE(page->m_accessListTable->rowCount(), 1);
        QCOMPARE(page->m_accessListTable->item(0, 0)->text(), QString("DaTenere"));
        delete page;
    }

    /* E-7: persistenza tra "sessioni" — due istanze separate condividono QSettings */
    void persistenzaTraIstanze() {
        auto* page1 = new LanWanPage(m_ai);
        insertRow(page1, "Persistente", "01/07/2026 09:00");
        page1->saveAccessList();
        delete page1;

        auto* page2 = new LanWanPage(m_ai);
        page2->loadAccessList();
        QCOMPARE(page2->m_accessListTable->rowCount(), 1);
        QCOMPARE(page2->m_accessListTable->item(0, 0)->text(), QString("Persistente"));
        delete page2;
    }

    /* E-8: caratteri speciali/unicode nel nome sopravvivono al round-trip JSON */
    void caratteriSpecialiRoundTrip() {
        auto* page = new LanWanPage(m_ai);
        const QString nome = QString::fromUtf8("Mario \"il telefono\" — 日本語 🍺");
        insertRow(page, nome, "01/07/2026 09:00");
        page->saveAccessList();

        page->m_accessListTable->setRowCount(0);
        page->loadAccessList();
        QCOMPARE(page->m_accessListTable->item(0, 0)->text(), nome);
        delete page;
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-F — Fallback TLS→HTTP visibile (D-19/T-D19)

   D-19 ha reso il fallback silenzioso TLS→HTTP visibile in UI
   (stato ambra "TLS non disponibile: token in chiaro"). T-D19
   chiedeva la verifica manuale (rinominare il cert / disinstallare
   openssl): qui la stessa condizione è riprodotta da riga di
   comando — HOME → dir temporanea senza ~/.prismalux/server.crt,
   PATH senza openssl → _ensureCert() fallisce → start() ripiega
   su HTTP con isTlsRequested()==true e isTlsEnabled()==false,
   esattamente la condizione che onLanServerStatusChanged() mappa
   sullo stato ambra. La label è verificata chiamando lo slot reale.
   ══════════════════════════════════════════════════════════════ */
class TestTlsFallback : public QObject {
    Q_OBJECT
private:
    QByteArray m_oldHome, m_oldPath;

private slots:
    void init() {
        m_oldHome = qgetenv("HOME");
        m_oldPath = qgetenv("PATH");
    }
    void cleanup() {
        qputenv("HOME", m_oldHome);
        qputenv("PATH", m_oldPath);
    }

    /* F-1: senza cert né openssl, TLS richiesto → fallback HTTP rilevabile */
    void fallbackHttpSenzaCertNeOpenssl() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        qputenv("HOME", tmp.path().toUtf8());   /* niente ~/.prismalux/server.crt */
        qputenv("PATH", "/nonexistent");         /* openssl irraggiungibile */

        AiClient ai;
        LanServer srv(&ai);
        srv.setTlsEnabled(true);
        const quint16 testPort = 49215;
        if (!srv.start(testPort)) QSKIP("Porta 49215 già in uso — skip");
        QVERIFY(srv.isRunning());
        QVERIFY(srv.isTlsRequested());
        QVERIFY2(!srv.isTlsEnabled(),
                 "senza cert e senza openssl il server deve ripiegare su HTTP "
                 "(condizione ambra di onLanServerStatusChanged)");
        srv.blockSignals(true);
        srv.stop();
    }

    /* F-2: TLS disattivato esplicitamente → nessuna condizione ambra
       (richiesto==attivo==false, l'avviso non deve scattare) */
    void tlsNonRichiestoNessunAllarme() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        qputenv("HOME", tmp.path().toUtf8());
        qputenv("PATH", "/nonexistent");

        AiClient ai;
        LanServer srv(&ai);
        srv.setTlsEnabled(false);
        const quint16 testPort = 49216;
        if (!srv.start(testPort)) QSKIP("Porta 49216 già in uso — skip");
        QVERIFY(srv.isRunning());
        QVERIFY(!srv.isTlsRequested());
        QVERIFY(!srv.isTlsEnabled());
        srv.blockSignals(true);
        srv.stop();
    }

    /* F-3: label ambra quando TLS richiesto ma non attivo — slot UI reale.
       Il server della pagina NON viene avviato: setTlsEnabled(true) basta a
       creare lo stato richiesto≠attivo che onLanServerStatusChanged() legge. */
    void labelAmbraSuFallback() {
        MockAiClient ai;
        auto* page = new LanWanPage(&ai);
        /* m_lanServer è creato lazy in onLanToggleBtnToggled(): qui lo si
           crea allo stesso modo, senza avviarlo — richiesto≠attivo basta */
        page->m_lanServer = new LanServer(&ai, page);
        page->m_lanServer->setTlsEnabled(true);
        QVERIFY(!page->m_lanServer->isTlsEnabled());
        page->onLanServerStatusChanged(true);
        QVERIFY2(page->m_lanStatusLbl->text().contains("TLS non disponibile"),
                 "con TLS richiesto ma non attivo la label deve avvisare del fallback");
        QVERIFY2(page->m_lanStatusLbl->styleSheet().contains("#f59e0b"),
                 "lo stato fallback deve essere ambra, non verde");
        delete page;
    }

    /* F-4: nessun avviso quando TLS non è richiesto (HTTP scelto, non subito) */
    void labelSenzaAvvisoSeTlsNonRichiesto() {
        MockAiClient ai;
        auto* page = new LanWanPage(&ai);
        page->m_lanServer = new LanServer(&ai, page);
        page->m_lanServer->setTlsEnabled(false);
        page->onLanServerStatusChanged(true);
        QVERIFY(!page->m_lanStatusLbl->text().contains("TLS non disponibile"));
        QVERIFY(!page->m_lanStatusLbl->styleSheet().contains("#f59e0b"));
        delete page;
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
    { TestAccessList        t; ret |= QTest::qExec(&t, argc, argv); }
    { TestTlsFallback       t; ret |= QTest::qExec(&t, argc, argv); }
    return ret;
}
#include "test_lan_wan_core.moc"
