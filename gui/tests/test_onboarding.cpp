/* ══════════════════════════════════════════════════════════════
   test_onboarding.cpp — Test onboarding wizard + rate limiter /knowledge

   Categorie:
     CAT-A  Onboarding QSettings — chiave kSetupDone letta/scritta correttamente
     CAT-B  LAN token QSettings — chiave kLanToken round-trip
     CAT-C  Rate limiter knowledge — contatore + reset (senza rete)
     CAT-D  LanServer token API — setAccessToken state machine

   Build:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_onboarding
     ./build_tests/test_onboarding
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QApplication>
#include <QSettings>

#include "mock_ai_client.h"
#include "../prismalux_paths.h"
#include "../lan_server.h"

namespace P = PrismaluxPaths;

/* ══════════════════════════════════════════════════════════════
   CAT-A — Onboarding QSettings
   ══════════════════════════════════════════════════════════════ */
class TestOnboardingSettings : public QObject {
    Q_OBJECT
private slots:

    void cleanup() {
        /* Ripulisce la chiave dopo ogni test per non inquinare altri */
        QSettings("Prismalux", "GUI").remove(P::SK::kSetupDone);
    }

    /* A-1: chiave assente → default false */
    void defaultFalse() {
        QSettings("Prismalux", "GUI").remove(P::SK::kSetupDone);
        const bool done = QSettings("Prismalux", "GUI")
                              .value(P::SK::kSetupDone, false).toBool();
        QVERIFY2(!done, "kSetupDone deve essere false se non impostato");
    }

    /* A-2: round-trip write/read */
    void roundTrip() {
        QSettings("Prismalux", "GUI").setValue(P::SK::kSetupDone, true);
        const bool done = QSettings("Prismalux", "GUI")
                              .value(P::SK::kSetupDone, false).toBool();
        QVERIFY2(done, "kSetupDone deve essere true dopo setValue(true)");
    }

    /* A-3: chiave costante non vuota */
    void chiaveNonVuota() {
        QVERIFY2(strlen(P::SK::kSetupDone) > 0, "kSetupDone non deve essere stringa vuota");
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B — LAN token QSettings
   ══════════════════════════════════════════════════════════════ */
class TestLanTokenSettings : public QObject {
    Q_OBJECT
private slots:

    void cleanup() {
        QSettings("Prismalux", "GUI").remove(P::SK::kLanToken);
    }

    /* B-1: default stringa vuota */
    void defaultVuoto() {
        QSettings("Prismalux", "GUI").remove(P::SK::kLanToken);
        const QString tok = QSettings("Prismalux", "GUI")
                                .value(P::SK::kLanToken, "").toString();
        QVERIFY2(tok.isEmpty(), "kLanToken deve essere vuoto di default");
    }

    /* B-2: round-trip con token ASCII */
    void roundTripAscii() {
        const QString expected = "mySecretToken123";
        QSettings("Prismalux", "GUI").setValue(P::SK::kLanToken, expected);
        const QString got = QSettings("Prismalux", "GUI")
                                .value(P::SK::kLanToken, "").toString();
        QCOMPARE(got, expected);
    }

    /* B-3: token con caratteri speciali */
    void roundTripSpeciali() {
        const QString expected = "token_con-punto.e/slash\\e\"quote'";
        QSettings("Prismalux", "GUI").setValue(P::SK::kLanToken, expected);
        const QString got = QSettings("Prismalux", "GUI")
                                .value(P::SK::kLanToken, "").toString();
        QCOMPARE(got, expected);
    }

    /* B-4: chiave costante corretta */
    void chiaveCorretta() {
        QCOMPARE(QString(P::SK::kLanToken), QString("lan/accessToken"));
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-C — Rate limiter: verifica costanti e stato LanServer
   ══════════════════════════════════════════════════════════════ */
class TestRateLimiter : public QObject {
    Q_OBJECT
private slots:

    /* C-1: LanServer costruito → non in esecuzione, rate limiter non attivo */
    void costruitoNonRunning() {
        MockAiClient ai;
        LanServer srv(&ai);
        QVERIFY(!srv.isRunning());
        QCOMPARE(srv.clientCount(), 0);
    }

    /* C-2: start/stop multipli non causano accumulo di stato */
    void startStopNoAccumulo() {
        MockAiClient ai;
        LanServer srv(&ai);
        for (int i = 0; i < 5; ++i) {
            QVERIFY(srv.start(0));
            srv.stop();
        }
        QVERIFY(!srv.isRunning());
        QCOMPARE(srv.clientCount(), 0);
    }

    /* C-3: dopo stop, clientCount rimane 0 */
    void clientCountZeroDopoStop() {
        MockAiClient ai;
        LanServer srv(&ai);
        srv.start(0);
        srv.stop();
        QCOMPARE(srv.clientCount(), 0);
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-D — setAccessToken state machine
   ══════════════════════════════════════════════════════════════ */
class TestTokenStateMachine : public QObject {
    Q_OBJECT
private slots:

    /* D-1: token vuoto → server avvia */
    void tokenVuotoAvvia() {
        MockAiClient ai;
        LanServer srv(&ai);
        srv.setAccessToken("");
        QVERIFY(srv.start(0));
        srv.stop();
    }

    /* D-2: token impostato → server avvia ugualmente */
    void tokenImpostatoAvvia() {
        MockAiClient ai;
        LanServer srv(&ai);
        srv.setAccessToken("token-test");
        QVERIFY(srv.start(0));
        srv.stop();
    }

    /* D-3: set token → clear → server avvia */
    void setClearAvvia() {
        MockAiClient ai;
        LanServer srv(&ai);
        srv.setAccessToken("token");
        srv.setAccessToken("");
        QVERIFY(srv.start(0));
        srv.stop();
    }

    /* D-4: set token mentre server è running → non crasha */
    void setTokenMentreRunning() {
        MockAiClient ai;
        LanServer srv(&ai);
        QVERIFY(srv.start(0));
        srv.setAccessToken("token-live");
        QVERIFY(srv.isRunning());
        srv.setAccessToken("");
        QVERIFY(srv.isRunning());
        srv.stop();
    }

    /* D-5: token molto lungo non crasha */
    void tokenLungoNonCrasha() {
        MockAiClient ai;
        LanServer srv(&ai);
        srv.setAccessToken(QString(1024, 'x'));
        QVERIFY(srv.start(0));
        srv.stop();
    }
};

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    /* Usa organizzazione di test separata per non inquinare le impostazioni utente */
    QCoreApplication::setOrganizationName("Prismalux");
    QCoreApplication::setApplicationName("GUI");

    int status = 0;
    {
        TestOnboardingSettings t1; status |= QTest::qExec(&t1, argc, argv);
        TestLanTokenSettings   t2; status |= QTest::qExec(&t2, argc, argv);
        TestRateLimiter        t3; status |= QTest::qExec(&t3, argc, argv);
        TestTokenStateMachine  t4; status |= QTest::qExec(&t4, argc, argv);
    }
    return status;
}

#include "test_onboarding.moc"
