/* ══════════════════════════════════════════════════════════════
   test_gns3_mcp.cpp  —  Suite di regressione GNS3 in LanWanPage
   ──────────────────────────────────────────────────────────────
   3 categorie:

   CAT-A  Costruzione LanWanPage     — widget GNS3 presenti e validi
   CAT-B  GNS3 mock server           — connessione 127.0.0.1:3080 (QSKIP se assente)
   CAT-C  Topologia / API / defaults — combo azioni, host default, stato pulsante

   COME ESEGUIRE:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_gns3_mcp
     ./build_tests/test_gns3_mcp
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QApplication>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QComboBox>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QTimer>

#include "mock_ai_client.h"
#include "../pages/main_lan_wan.h"

/* ══════════════════════════════════════════════════════════════
   CAT-A — Costruzione LanWanPage (widget GNS3 presenti)
   ══════════════════════════════════════════════════════════════ */
class TestCatA : public QObject {
    Q_OBJECT

private:
    MockAiClient* m_ai   = nullptr;
    LanWanPage*   m_page = nullptr;

private slots:

    void init()
    {
        m_ai   = new MockAiClient;
        m_page = new LanWanPage(m_ai);
    }

    void cleanup()
    {
        delete m_page;
        delete m_ai;
        m_page = nullptr;
        m_ai   = nullptr;
    }

    /* LanWanPage si costruisce senza crash */
    void constructsWithoutCrash()
    {
        QVERIFY(m_page != nullptr);
    }

    /* Eredita QWidget */
    void isQWidget()
    {
        QVERIFY(qobject_cast<QWidget*>(m_page) != nullptr);
    }

    /* m_gns3HostEdit presente */
    void hasGns3HostEdit()
    {
        QLineEdit* w = m_page->findChild<QLineEdit*>("", Qt::FindChildrenRecursively);
        /* findChild generico: cerchiamo via accesso diretto al membro tramite
           l'objectName oppure verifichiamo che almeno uno QLineEdit contenga
           il testo di default "localhost:3080" */
        bool found = false;
        const auto edits = m_page->findChildren<QLineEdit*>();
        for (QLineEdit* e : edits) {
            if (e->text().contains("localhost") || e->text().contains("127.0.0.1")) {
                found = true;
                break;
            }
        }
        QVERIFY2(found, "m_gns3HostEdit non trovato (nessun QLineEdit con host GNS3)");
        Q_UNUSED(w)
    }

    /* m_gns3RunBtn presente */
    void hasGns3RunBtn()
    {
        const auto btns = m_page->findChildren<QPushButton*>();
        bool found = false;
        for (QPushButton* b : btns) {
            /* Il testo contiene "GNS3" (il pulsante "Genera script GNS3") */
            if (b->text().contains("GNS3", Qt::CaseInsensitive)) {
                found = true;
                break;
            }
        }
        QVERIFY2(found, "m_gns3RunBtn non trovato");
    }

    /* m_gns3Output presente */
    void hasGns3Output()
    {
        const auto outputs = m_page->findChildren<QTextEdit*>();
        QVERIFY2(!outputs.isEmpty(), "Nessun QTextEdit trovato in LanWanPage");
    }

    /* m_gns3Action presente e con almeno 1 item */
    void hasGns3ActionWithItems()
    {
        const auto combos = m_page->findChildren<QComboBox*>();
        bool found = false;
        for (QComboBox* c : combos) {
            if (c->count() > 0) {
                /* Cerchiamo il combo azioni GNS3: contiene item con "topologia"
                   o "Router" o "Firewall" */
                for (int i = 0; i < c->count(); ++i) {
                    const QString txt = c->itemText(i);
                    if (txt.contains("topologia",  Qt::CaseInsensitive) ||
                        txt.contains("Router",     Qt::CaseInsensitive) ||
                        txt.contains("Firewall",   Qt::CaseInsensitive) ||
                        txt.contains("Script",     Qt::CaseInsensitive))
                    {
                        found = true;
                        break;
                    }
                }
            }
            if (found) break;
        }
        QVERIFY2(found, "m_gns3Action non trovato o senza item GNS3 attesi");
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B — GNS3 mock server (QSKIP se non raggiungibile)
   ══════════════════════════════════════════════════════════════ */
class TestCatB : public QObject {
    Q_OBJECT

private:
    MockAiClient* m_ai   = nullptr;
    LanWanPage*   m_page = nullptr;

    /* Ritorna true se GNS3 risponde entro timeout_ms */
    static bool gns3Reachable(int timeout_ms = 500)
    {
        QNetworkAccessManager nam;
        QNetworkRequest req(QUrl("http://127.0.0.1:3080/v3/version"));
        req.setTransferTimeout(timeout_ms);
        QNetworkReply* reply = nam.get(req);

        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        QObject::connect(reply, &QNetworkReply::finished,   &loop, &QEventLoop::quit);
        QObject::connect(&timer, &QTimer::timeout,          &loop, &QEventLoop::quit);
        timer.start(timeout_ms);
        loop.exec();

        const bool ok = (reply->error() == QNetworkReply::NoError);
        reply->deleteLater();
        return ok;
    }

private slots:

    void init()
    {
        m_ai   = new MockAiClient;
        m_page = new LanWanPage(m_ai);
    }

    void cleanup()
    {
        delete m_page;
        delete m_ai;
        m_page = nullptr;
        m_ai   = nullptr;
    }

    /* Tentativo connessione: verifica che il pulsante Run sia presente quando il server e' up */
    void gns3ConnectOrSkip()
    {
        if (!gns3Reachable()) {
            QSKIP("GNS3 non disponibile su 127.0.0.1:3080 — skip CAT-B");
        }

        /* Se GNS3 e' up, m_gns3RunBtn deve essere abilitato */
        const auto btns = m_page->findChildren<QPushButton*>();
        bool runBtnEnabled = false;
        for (QPushButton* b : btns) {
            if (b->text().contains("GNS3", Qt::CaseInsensitive) && b->isEnabled()) {
                runBtnEnabled = true;
                break;
            }
        }
        QVERIFY2(runBtnEnabled, "m_gns3RunBtn non abilitato con GNS3 disponibile");
    }

    /* La versione API risponde con JSON valido contenente "version" */
    void gns3VersionApiResponds()
    {
        if (!gns3Reachable()) {
            QSKIP("GNS3 non disponibile su 127.0.0.1:3080 — skip CAT-B");
        }

        QNetworkAccessManager nam;
        QNetworkRequest req(QUrl("http://127.0.0.1:3080/v3/version"));
        req.setTransferTimeout(500);
        QNetworkReply* reply = nam.get(req);

        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        QCOMPARE(reply->error(), QNetworkReply::NoError);
        const QByteArray body = reply->readAll();
        QVERIFY2(body.contains("version"), "Risposta GNS3 /v3/version non contiene 'version'");
        reply->deleteLater();
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-C — Topologia / API / defaults
   ══════════════════════════════════════════════════════════════ */
class TestCatC : public QObject {
    Q_OBJECT

private:
    MockAiClient* m_ai   = nullptr;
    LanWanPage*   m_page = nullptr;

private slots:

    void init()
    {
        m_ai   = new MockAiClient;
        m_page = new LanWanPage(m_ai);
    }

    void cleanup()
    {
        delete m_page;
        delete m_ai;
        m_page = nullptr;
        m_ai   = nullptr;
    }

    /* Il combo GNS3 contiene le azioni attese */
    void gns3ActionComboContainsExpectedItems()
    {
        const auto combos = m_page->findChildren<QComboBox*>();
        QComboBox* actionCombo = nullptr;
        for (QComboBox* c : combos) {
            for (int i = 0; i < c->count(); ++i) {
                if (c->itemText(i).contains("topologia", Qt::CaseInsensitive)) {
                    actionCombo = c;
                    break;
                }
            }
            if (actionCombo) break;
        }
        QVERIFY2(actionCombo != nullptr, "m_gns3Action non trovato");
        QVERIFY2(actionCombo->count() >= 5,
                 "m_gns3Action contiene meno di 5 azioni GNS3 attese");

        /* Verifica presenza azioni fondamentali */
        QStringList found;
        for (int i = 0; i < actionCombo->count(); ++i) {
            found << actionCombo->itemText(i);
        }
        const QString joined = found.join("|");
        QVERIFY2(joined.contains("topologia",  Qt::CaseInsensitive), "Manca 'Nuova topologia'");
        QVERIFY2(joined.contains("Router",     Qt::CaseInsensitive), "Manca 'Router & Switch'");
        QVERIFY2(joined.contains("Firewall",   Qt::CaseInsensitive), "Manca 'Configura Firewall'");
    }

    /* Il campo host di default e' localhost:3080 */
    void gns3HostDefaultIsLocalhost()
    {
        const auto edits = m_page->findChildren<QLineEdit*>();
        bool found = false;
        for (QLineEdit* e : edits) {
            const QString txt = e->text();
            if (txt == "localhost:3080" || txt.startsWith("127.0.0.1")) {
                found = true;
                break;
            }
        }
        QVERIFY2(found, "Host default GNS3 non e' 'localhost:3080' o '127.0.0.1'");
    }

    /* Il combo azioni e' abilitato di default */
    void gns3ActionComboEnabledByDefault()
    {
        const auto combos = m_page->findChildren<QComboBox*>();
        QComboBox* actionCombo = nullptr;
        for (QComboBox* c : combos) {
            for (int i = 0; i < c->count(); ++i) {
                if (c->itemText(i).contains("topologia", Qt::CaseInsensitive)) {
                    actionCombo = c;
                    break;
                }
            }
            if (actionCombo) break;
        }
        QVERIFY2(actionCombo != nullptr, "m_gns3Action non trovato");
        QVERIFY2(actionCombo->isEnabled(), "m_gns3Action deve essere abilitato di default");
    }

    /* Il pulsante "Esegui su GNS3" (m_gns3ExecBtn) e' disabilitato finche'
       non e' presente codice generato dall'AI */
    void gns3ExecBtnDisabledByDefault()
    {
        const auto btns = m_page->findChildren<QPushButton*>();
        bool execBtnFound    = false;
        bool execBtnDisabled = false;
        for (QPushButton* b : btns) {
            if (b->text().contains("Esegui", Qt::CaseInsensitive) &&
                b->text().contains("GNS3",   Qt::CaseInsensitive))
            {
                execBtnFound    = true;
                execBtnDisabled = !b->isEnabled();
                break;
            }
        }
        QVERIFY2(execBtnFound,    "m_gns3ExecBtn non trovato");
        QVERIFY2(execBtnDisabled, "m_gns3ExecBtn deve essere disabilitato prima di generare codice");
    }

    /* Il campo host non e' vuoto di default */
    void gns3HostNotEmptyByDefault()
    {
        const auto edits = m_page->findChildren<QLineEdit*>();
        bool hostEditsNonEmpty = false;
        for (QLineEdit* e : edits) {
            const QString txt = e->text();
            if (txt.contains("localhost") || txt.contains("127.0.0.1") || txt.contains(":3080")) {
                hostEditsNonEmpty = true;
                break;
            }
        }
        QVERIFY2(hostEditsNonEmpty, "Il campo host GNS3 e' vuoto al lancio");
    }

    /* Il numero di azioni GNS3 corrisponde a kGNS3Actions (5 voci + nullptr) */
    void gns3ActionComboExactCount()
    {
        const auto combos = m_page->findChildren<QComboBox*>();
        QComboBox* actionCombo = nullptr;
        for (QComboBox* c : combos) {
            for (int i = 0; i < c->count(); ++i) {
                if (c->itemText(i).contains("topologia", Qt::CaseInsensitive)) {
                    actionCombo = c;
                    break;
                }
            }
            if (actionCombo) break;
        }
        QVERIFY2(actionCombo != nullptr, "m_gns3Action non trovato");
        QCOMPARE(actionCombo->count(), 5);
    }
};

/* ══════════════════════════════════════════════════════════════
   main
   ══════════════════════════════════════════════════════════════ */
int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    int r = 0;
    r |= QTest::qExec(new TestCatA, argc, argv);
    r |= QTest::qExec(new TestCatB, argc, argv);
    r |= QTest::qExec(new TestCatC, argc, argv);
    return r;
}

#include "test_gns3_mcp.moc"
