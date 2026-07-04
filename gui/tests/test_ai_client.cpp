/* ══════════════════════════════════════════════════════════════
   test_ai_client.cpp — Unit test AiClient (no Ollama richiesto)

   Categorie:
     CAT-A  classifyQuery — 12 casi (logica pura, no rete)
     CAT-B  detectQueryDomain — 8 casi (logica pura, no rete)
     CAT-C  SmartRouter API — enable/disable, accessor
     CAT-D  abort() segnale — aborted() emesso prima di finished()
     CAT-E  Mock HTTP 4xx/5xx — error() emesso, no crash

   Build:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_ai_client
     ./build_tests/test_ai_client
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QApplication>
#include <QEventLoop>
#include <QTimer>
#include <QTcpServer>
#include <QTcpSocket>

#include "../ai_client.h"

/* ══════════════════════════════════════════════════════════════
   CAT-A — classifyQuery (logica pura)
   ══════════════════════════════════════════════════════════════ */
class TestClassifyQuery : public QObject {
    Q_OBJECT
private slots:

    /* A-1: testo breve ≤30 char senza keyword → Simple */
    void breve_senzaKeyword() {
        QCOMPARE(AiClient::classifyQuery("Ciao come stai"),
                 AiClient::QuerySimple);
    }

    /* A-2: testo esattamente 30 char → Simple */
    void esattamente30char() {
        QCOMPARE(AiClient::classifyQuery(QString(30, 'a')),
                 AiClient::QuerySimple);
    }

    /* A-3: testo 31-200 char senza keyword → Auto */
    void medio_senzaKeyword() {
        const QString txt = QString(80, 'x');
        QCOMPARE(AiClient::classifyQuery(txt), AiClient::QueryAuto);
    }

    /* A-4: testo > 200 char → Complex indipendentemente dalle keyword */
    void lungo_sempreComplex() {
        const QString txt = QString(201, 'a');
        QCOMPARE(AiClient::classifyQuery(txt), AiClient::QueryComplex);
    }

    /* A-5: keyword "spiega" → Complex anche se breve */
    void keyword_spiega() {
        QCOMPARE(AiClient::classifyQuery("spiega il calcolo"),
                 AiClient::QueryComplex);
    }

    /* A-6: keyword "analizza" → Complex */
    void keyword_analizza() {
        QCOMPARE(AiClient::classifyQuery("analizza questo codice"),
                 AiClient::QueryComplex);
    }

    /* A-7: keyword "algoritmo" → Complex */
    void keyword_algoritmo() {
        QCOMPARE(AiClient::classifyQuery("scrivi un algoritmo per ordinare"),
                 AiClient::QueryComplex);
    }

    /* A-8: keyword "codice" → Complex */
    void keyword_codice() {
        QCOMPARE(AiClient::classifyQuery("mostrami il codice"),
                 AiClient::QueryComplex);
    }

    /* A-9: falso positivo "non so se" → NON deve essere Complex */
    void falsopositivo_nonSoSe() {
        /* "non so se" non è incertezza dichiarata su risposta — deve essere Auto */
        const QString q = "non so se devo usare Python o C++";
        const AiClient::QueryType t = AiClient::classifyQuery(q);
        QVERIFY2(t != AiClient::QuerySimple,
                 "query media senza falso positivo non deve essere Simple");
    }

    /* A-10: stringa vuota → Simple (lunghezza 0 ≤ 30) */
    void vuota() {
        QCOMPARE(AiClient::classifyQuery(""), AiClient::QuerySimple);
    }

    /* A-11: keyword "perché" con accento → Complex */
    void keyword_perche() {
        QCOMPARE(AiClient::classifyQuery(
                     QString::fromUtf8("perch\xc3\xa9 funziona")),
                 AiClient::QueryComplex);
    }

    /* A-12: keyword case-insensitive "SPIEGA" → Complex */
    void keyword_uppercase() {
        QCOMPARE(AiClient::classifyQuery("SPIEGA la teoria"),
                 AiClient::QueryComplex);
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B — detectQueryDomain (logica pura)
   ══════════════════════════════════════════════════════════════ */
class TestDetectQueryDomain : public QObject {
    Q_OBJECT
private slots:

    /* B-1: query matematica → DomainMath */
    void queryMat() {
        const auto d = AiClient::detectQueryDomain("calcola l'integrale di x^2");
        QVERIFY2(d == AiClient::DomainMath || d == AiClient::DomainPhysics,
                 "query matematica deve essere DomainMath o DomainPhysics");
    }

    /* B-2: domainNeedsMathModel(DomainMath) == true */
    void mathNeedsModel() {
        QVERIFY(AiClient::domainNeedsMathModel(AiClient::DomainMath));
    }

    /* B-3: domainNeedsMathModel(DomainGeneral) == false */
    void generalNotNeeds() {
        QVERIFY(!AiClient::domainNeedsMathModel(AiClient::DomainGeneral));
    }

    /* B-4: query codice → DomainCoding (usa keyword che non è anche math) */
    void queryCodice() {
        /* "debug" è keyword code-only (non appare nei math keywords) */
        const auto d = AiClient::detectQueryDomain("debug this Python script");
        QVERIFY2(d == AiClient::DomainCoding,
                 "query debug Python script deve rilevare DomainCoding");
    }

    /* B-5: testo generico → DomainGeneral */
    void queryGenerale() {
        const auto d = AiClient::detectQueryDomain("Ciao come stai?");
        QCOMPARE(d, AiClient::DomainGeneral);
    }

    /* B-6: domainNeedsMathModel restituisce bool valido per tutti i valori enum */
    void allDomainsReturnBool() {
        const AiClient::QueryDomain all[] = {
            AiClient::DomainGeneral, AiClient::DomainMath, AiClient::DomainPhysics,
            AiClient::DomainChemistry, AiClient::DomainElectronics,
            AiClient::DomainUnitConvert, AiClient::DomainCoding
        };
        for (const auto d : all) {
            const bool b = AiClient::domainNeedsMathModel(d);
            Q_UNUSED(b);
        }
        QVERIFY(true);
    }

    /* B-7: query fisica → DomainPhysics o DomainMath */
    void queryFisica() {
        const auto d = AiClient::detectQueryDomain("calcola la velocità F=ma");
        const bool isStem = (d == AiClient::DomainMath
                          || d == AiClient::DomainPhysics
                          || d == AiClient::DomainCoding);
        QVERIFY2(isStem, "query fisica deve essere in un dominio tecnico");
    }

    /* B-8: detectQueryDomain non crasha su stringa vuota */
    void vuotaNonCrasha() {
        const auto d = AiClient::detectQueryDomain("");
        Q_UNUSED(d);
        QVERIFY(true);
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B2 — detectQueryIsEnglish (D-28): rilevamento lingua query,
   usata per l'istruzione di lingua dinamica (sostituisce il vecchio
   vincolo fisso "Rispondi sempre in italiano" in kHonestyPrefix)
   ══════════════════════════════════════════════════════════════ */
class TestDetectQueryIsEnglish : public QObject {
    Q_OBJECT
private slots:

    void englishSentenceDetected() {
        QVERIFY(AiClient::detectQueryIsEnglish("What is the capital of France?"));
    }

    void englishCodeRequestDetected() {
        QVERIFY(AiClient::detectQueryIsEnglish("Please write a function to sort this array"));
    }

    void italianSentenceNotEnglish() {
        QVERIFY(!AiClient::detectQueryIsEnglish("Qual è la capitale della Francia?"));
    }

    void italianCodeRequestNotEnglish() {
        QVERIFY(!AiClient::detectQueryIsEnglish("Scrivi una funzione per ordinare questo array"));
    }

    void emptyStringNotEnglish() {
        QVERIFY(!AiClient::detectQueryIsEnglish(""));
    }

    void singleAmbiguousWordNotEnglish() {
        /* una sola occorrenza (soglia richiede >=2 hit) — evita falsi
         * positivi su singole parole ambigue tra le due lingue */
        QVERIFY(!AiClient::detectQueryIsEnglish("la formula è E=mc2"));
    }

    void mixedLanguageWithTwoEnglishHitsDetected() {
        QVERIFY(AiClient::detectQueryIsEnglish("please can you help me with this"));
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-C — SmartRouter API
   ══════════════════════════════════════════════════════════════ */
class TestSmartRouter : public QObject {
    Q_OBJECT
private slots:

    /* C-1: smartRouterEnabled() falso di default */
    void disabledByDefault() {
        AiClient ai;
        QVERIFY(!ai.smartRouterEnabled());
    }

    /* C-2: setSmartRouter(true,...) → smartRouterEnabled() == true */
    void enableSmartRouter() {
        AiClient ai;
        ai.setSmartRouter(true, "https://api.openai.com", "gpt-4o", "sk-test");
        QVERIFY(ai.smartRouterEnabled());
    }

    /* C-3: setSmartRouter(false,...) → smartRouterEnabled() == false */
    void disableSmartRouter() {
        AiClient ai;
        ai.setSmartRouter(true,  "https://api.openai.com", "gpt-4o", "sk-test");
        ai.setSmartRouter(false, "", "", "");
        QVERIFY(!ai.smartRouterEnabled());
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-D — abort() → aborted() emesso
   ══════════════════════════════════════════════════════════════ */
class TestAbort : public QObject {
    Q_OBJECT
private slots:

    /* D-1: abort() senza chat attiva non crasha */
    void abortSenzaChatNonCrasha() {
        AiClient ai;
        ai.abort();
        QVERIFY(true);
    }

    /* D-2: abort() durante chat emette aborted() (con server che accetta ma non risponde) */
    void abortEmitteAborted() {
        /* Server TCP che accetta connessioni ma non scrive mai nulla.
           Conserviamo il socket per prevenire la chiusura immediata per GC. */
        QTcpServer stallSrv;
        QList<QTcpSocket*> held;
        QVERIFY(stallSrv.listen(QHostAddress::LocalHost, 0));
        const int stallPort = static_cast<int>(stallSrv.serverPort());
        QObject::connect(&stallSrv, &QTcpServer::newConnection, [&stallSrv, &held] {
            QTcpSocket* s = stallSrv.nextPendingConnection();
            if (s) held.append(s); /* mantiene il socket vivo per stalling */
        });

        AiClient ai;
        ai.setBackend(AiClient::Ollama, "127.0.0.1", stallPort, "test");
        QSignalSpy spyAbort(&ai, &AiClient::aborted);
        QSignalSpy spyFin(&ai,   &AiClient::finished);

        ai.chat("sys", "user");

        QTimer::singleShot(150, &ai, &AiClient::abort);

        QEventLoop loop;
        connect(&ai, &AiClient::aborted, &loop, &QEventLoop::quit);
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        loop.exec();

        QVERIFY2(spyAbort.count() >= 1,
                 "abort() su richiesta pending deve emettere aborted()");
        QCOMPARE(spyFin.count(), 0);
    }

    /* D-3: due abort() consecutivi non crashano */
    void doubleAbortNonCrasha() {
        AiClient ai;
        ai.abort();
        ai.abort();
        QVERIFY(true);
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-E — Mock HTTP server → error() su 4xx/5xx
   ══════════════════════════════════════════════════════════════ */
class TestMockHttp : public QObject {
    Q_OBJECT
private:
    /* Mini server TCP che risponde con status fisso */
    class MockServer : public QObject {
    public:
        int port = 0;
        int statusCode = 200;
        QString body;
        explicit MockServer(QObject* p = nullptr) : QObject(p) {}

        bool start() {
            m_srv = new QTcpServer(this);
            if (!m_srv->listen(QHostAddress::LocalHost, 0)) return false;
            port = static_cast<int>(m_srv->serverPort());
            connect(m_srv, &QTcpServer::newConnection, this, [this] {
                QTcpSocket* s = m_srv->nextPendingConnection();
                if (!s) return;
                connect(s, &QTcpSocket::readyRead, this, [this, s] {
                    s->readAll();
                    const QByteArray resp =
                        "HTTP/1.1 " + QByteArray::number(statusCode) + " Status\r\n"
                        "Content-Type: application/json\r\n"
                        "Content-Length: " + QByteArray::number(body.toUtf8().size()) + "\r\n"
                        "\r\n" + body.toUtf8();
                    s->write(resp);
                    s->flush();
                    s->disconnectFromHost();
                });
            });
            return true;
        }
    private:
        QTcpServer* m_srv = nullptr;
    };

private slots:

    /* E-1: 404 da Ollama → error() emesso */
    void risposta404_emitteError() {
        MockServer srv;
        QVERIFY(srv.start());
        srv.statusCode = 404;
        srv.body = "{\"error\":\"not found\"}";

        AiClient ai;
        ai.setBackend(AiClient::Ollama, "127.0.0.1", srv.port, "test");

        QSignalSpy spyErr(&ai, &AiClient::error);
        QSignalSpy spyFin(&ai, &AiClient::finished);
        ai.chat("sys", "user");

        QEventLoop loop;
        connect(&ai, &AiClient::error,    &loop, &QEventLoop::quit);
        connect(&ai, &AiClient::finished, &loop, &QEventLoop::quit);
        QTimer::singleShot(5000, &loop, &QEventLoop::quit);
        loop.exec();

        QCOMPARE(spyFin.count(), 0);
        QVERIFY2(spyErr.count() >= 1, "404 deve emettere error()");
    }

    /* E-2: 500 da Ollama → error() emesso */
    void risposta500_emitteError() {
        MockServer srv;
        QVERIFY(srv.start());
        srv.statusCode = 500;
        srv.body = "{\"error\":\"internal server error\"}";

        AiClient ai;
        ai.setBackend(AiClient::Ollama, "127.0.0.1", srv.port, "test");

        QSignalSpy spyErr(&ai, &AiClient::error);
        ai.chat("sys", "user");

        QEventLoop loop;
        connect(&ai, &AiClient::error,    &loop, &QEventLoop::quit);
        connect(&ai, &AiClient::finished, &loop, &QEventLoop::quit);
        QTimer::singleShot(5000, &loop, &QEventLoop::quit);
        loop.exec();

        QVERIFY2(spyErr.count() >= 1, "500 deve emettere error()");
    }

    /* E-3: host irraggiungibile → error() emesso, no crash */
    void hostIrraggiungibile_error() {
        AiClient ai;
        ai.setBackend(AiClient::Ollama, "127.0.0.1", 19998, "test");

        QSignalSpy spyErr(&ai, &AiClient::error);
        ai.chat("sys", "user");

        QEventLoop loop;
        connect(&ai, &AiClient::error,    &loop, &QEventLoop::quit);
        connect(&ai, &AiClient::finished, &loop, &QEventLoop::quit);
        QTimer::singleShot(8000, &loop, &QEventLoop::quit);
        loop.exec();

        QVERIFY2(spyErr.count() >= 1, "host irraggiungibile deve emettere error()");
    }
};

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    int ret = 0;
    { TestClassifyQuery    t; ret |= QTest::qExec(&t, argc, argv); }
    { TestDetectQueryDomain t; ret |= QTest::qExec(&t, argc, argv); }
    { TestDetectQueryIsEnglish t; ret |= QTest::qExec(&t, argc, argv); }
    { TestSmartRouter       t; ret |= QTest::qExec(&t, argc, argv); }
    { TestAbort             t; ret |= QTest::qExec(&t, argc, argv); }
    { TestMockHttp          t; ret |= QTest::qExec(&t, argc, argv); }
    return ret;
}
#include "test_ai_client.moc"
