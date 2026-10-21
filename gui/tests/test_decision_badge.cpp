/* ══════════════════════════════════════════════════════════════
   test_decision_badge.cpp — D-72: badge Decision Model nella chat

   Verifica il lato UI dei segnali emessi da AiClient quando il gate
   decisionale è attivo:
     decisionClassified → intento + confidenza (o "sotto soglia")
     decisionRetry      → primo tentativo scartato
     decisionVerified   → esito approvato / non approvato

   Non usa la rete: MockAiClient emette i segnali a mano. Il badge è
   il QLabel con objectName "decisionBadge" nella toolbar di AgentiPage.

   Eseguire offscreen:
     QT_QPA_PLATFORM=offscreen ./build_tests/test_decision_badge
   Screenshot opzionale:
     PRISMALUX_D72_SCREENSHOT=/tmp/d72_badge.png \
       QT_QPA_PLATFORM=offscreen ./build_tests/test_decision_badge
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QApplication>
#include <QLabel>

#include "mock_ai_client.h"
#include "../pages/main_ai.h"

class TestDecisionBadge : public QObject {
    Q_OBJECT

    MockAiClient* m_ai   = nullptr;
    AgentiPage*   m_page = nullptr;
    QLabel*       m_badge = nullptr;

    void flush() { QCoreApplication::processEvents(); }

private slots:

    void initTestCase()
    {
        m_ai   = new MockAiClient;
        m_page = new AgentiPage(m_ai);
        m_page->resize(1200, 800);
        m_page->show();
        QTest::qWaitForWindowExposed(m_page);
        m_badge = m_page->findChild<QLabel*>("decisionBadge");
        QVERIFY2(m_badge, "QLabel 'decisionBadge' non trovata nella toolbar");
    }

    void cleanupTestCase()
    {
        delete m_page; m_page = nullptr;
        delete m_ai;   m_ai   = nullptr;
    }

    /* B-1: senza segnali il badge non occupa spazio. */
    void badgeNascostoAllAvvio()
    {
        QVERIFY(!m_badge->isVisible());
    }

    /* B-2: intento sopra soglia con confidenza e cache. */
    void badgeIntentoClassificato()
    {
        emit m_ai->decisionClassified(QStringLiteral("coding"), 0.87, false);
        flush();
        QVERIFY(m_badge->isVisible());
        QVERIFY2(m_badge->text().contains(QStringLiteral("coding")),
                 qPrintable(m_badge->text()));
        QVERIFY2(m_badge->text().contains(QStringLiteral("87%")),
                 qPrintable(m_badge->text()));
        QVERIFY2(!m_badge->text().contains(QStringLiteral("cache")),
                 qPrintable(m_badge->text()));

        emit m_ai->decisionClassified(QStringLiteral("reasoning"), 0.72, true);
        flush();
        QVERIFY2(m_badge->text().contains(QStringLiteral("cache")),
                 qPrintable(m_badge->text()));
    }

    /* B-3: intento vuoto = sotto soglia, non un'etichetta inventata. */
    void badgeSottoSoglia()
    {
        emit m_ai->decisionClassified(QString(), 0.31, false);
        flush();
        QVERIFY(m_badge->isVisible());
        QVERIFY2(m_badge->text().contains(QStringLiteral("sotto soglia")),
                 qPrintable(m_badge->text()));
        QVERIFY2(!m_badge->text().contains(QStringLiteral("coding")),
                 "sotto soglia non deve comparire un intento");
    }

    /* B-4: retry → badge arancione con il motivo nel tooltip. */
    void badgeRetry()
    {
        emit m_ai->decisionRetry(QStringLiteral("risposta generica"));
        flush();
        QVERIFY(m_badge->isVisible());
        QVERIFY2(m_badge->text().contains(QStringLiteral("riverifica")),
                 qPrintable(m_badge->text()));
        QVERIFY2(m_badge->toolTip().contains(QStringLiteral("risposta generica")),
                 qPrintable(m_badge->toolTip()));
    }

    /* B-5: verdetto negativo → la risposta è mostrata comunque. */
    void badgeVerificaNegativa()
    {
        emit m_ai->decisionVerified(false, QStringLiteral("nessuna capitale"));
        flush();
        QVERIFY(m_badge->isVisible());
        QVERIFY2(m_badge->text().contains(QStringLiteral("non verificata")),
                 qPrintable(m_badge->text()));
        QVERIFY2(m_badge->toolTip().contains(QStringLiteral("nessuna capitale")),
                 qPrintable(m_badge->toolTip()));
    }

    /* B-6: verdetto positivo → badge verde. */
    void badgeVerificaPositiva()
    {
        emit m_ai->decisionVerified(true, QString());
        flush();
        QVERIFY(m_badge->isVisible());
        QVERIFY2(m_badge->text().contains(QStringLiteral("verificata")),
                 qPrintable(m_badge->text()));
        QVERIFY2(!m_badge->text().contains(QStringLiteral("non")),
                 qPrintable(m_badge->text()));
    }

    /* B-7: abort della richiesta → il badge sparisce. */
    void badgeResetSuAbort()
    {
        emit m_ai->decisionClassified(QStringLiteral("chat"), 0.9, false);
        flush();
        QVERIFY(m_badge->isVisible());

        m_ai->emitAborted();
        flush();
        QVERIFY2(!m_badge->isVisible(), "il badge deve sparire dopo l'abort");
    }

    /* B-8 (opzionale): screenshot del badge per la verifica visiva D-72. */
    void screenshotBadge()
    {
        const QString shot = qEnvironmentVariable("PRISMALUX_D72_SCREENSHOT");
        if (shot.isEmpty())
            QSKIP("PRISMALUX_D72_SCREENSHOT non impostata");

        m_page->show();
        emit m_ai->decisionClassified(QStringLiteral("coding"), 0.91, false);
        QTest::qWait(300);
        QVERIFY2(m_page->grab().save(shot),
                 qPrintable(QStringLiteral("salvataggio screenshot fallito: ") + shot));
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    TestDecisionBadge t;
    return QTest::qExec(&t, argc, argv);
}

#include "test_decision_badge.moc"
