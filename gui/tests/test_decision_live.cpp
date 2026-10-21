/* ══════════════════════════════════════════════════════════════
   test_decision_live.cpp — D-72: pipeline Decision Model con Ollama
   reale (decision model + modello conversazionale).

   Non è un test CI: richiede Ollama attivo e i modelli installati.
   Verifica ciò che i mock non possono dimostrare:
     · il decision model reale classifica la richiesta;
     · con riverifica attiva i token arrivano DOPO il verdetto
       (nessuno streaming della risposta scartata);
     · il badge della chat mostra intento ed esito;
     · la risposta finale arriva comunque.

   Modelli (override con env):
     PRISMALUX_DECISION_MODEL  default tev1:4b
     PRISMALUX_TEST_MODEL      default qwen2.5-coder:1.5b / qwen3:1.7b

   Eseguire:
     cmake --build build_tests --target test_decision_live
     QT_QPA_PLATFORM=offscreen ./build_tests/test_decision_live
   Screenshot:
     PRISMALUX_D72_LIVE_SCREENSHOT=/tmp/d72_live.png
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QJsonDocument>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSettings>
#include <QTextBrowser>
#include <QTextEdit>
#include <QTimer>

#include "../ai_client.h"
#include "../decision_engine.h"
#include "../pages/main_ai.h"
#include "../prismalux_paths.h"

namespace P = PrismaluxPaths;

static QStringList ollamaModels()
{
    QNetworkAccessManager nam;
    QNetworkRequest req(QUrl(QStringLiteral("http://127.0.0.1:%1/api/tags")
                                 .arg(P::kOllamaPort)));
    req.setTransferTimeout(2000);
    auto* r = nam.get(req);
    QEventLoop loop;
    QObject::connect(r, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    QStringList out;
    if (r->error() == QNetworkReply::NoError) {
        const auto arr = QJsonDocument::fromJson(r->readAll())
                             .object().value("models").toArray();
        for (const auto& v : arr)
            out << v.toObject().value("name").toString();
    }
    r->deleteLater();
    return out;
}

static QString envOr(const char* key, const QString& fallback)
{
    const QString v = qEnvironmentVariable(key);
    return v.isEmpty() ? fallback : v;
}

class TestDecisionLive : public QObject {
    Q_OBJECT

    QString m_decModel;
    QString m_chatModel;
    QString m_catalogPath;
    QByteArray m_catalogBackup;
    bool m_catalogExisted = false;

    void writeCatalog()
    {
        DecisionEngine::ModelSpec s;
        s.name = m_decModel;
        s.backend = DecisionEngine::BackendOllama;
        DecisionEngine::saveCatalog({ s });
    }

    bool waitInstalled(DecisionEngine& dec)
    {
        QEventLoop loop;
        QObject::connect(&dec, &DecisionEngine::installedReady,
                         &loop, &QEventLoop::quit);
        QTimer::singleShot(5000, &loop, &QEventLoop::quit);
        dec.refreshInstalled();
        loop.exec();
        return !dec.installedModels().isEmpty();
    }

    void configure(AiClient& ai, DecisionEngine& dec, bool verify)
    {
        dec.setHostPort(QStringLiteral("127.0.0.1"), P::kOllamaPort);
        dec.setBackend(DecisionEngine::BackendOllama);
        dec.setEnabled(true);
        dec.setVerifyEnabled(verify);
        dec.setModel(m_decModel);
        dec.setThreshold(0.0);   /* osserviamo anche i casi sotto soglia */
        ai.setDecisionEngine(&dec);
        ai.setBackend(AiClient::Ollama, QStringLiteral("127.0.0.1"),
                      P::kOllamaPort, m_chatModel);
    }

private slots:

    void initTestCase()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("Prismalux"));
        QCoreApplication::setApplicationName(QStringLiteral("GUI"));

        const QStringList installed = ollamaModels();
        if (installed.isEmpty())
            QSKIP("Ollama non raggiungibile su 127.0.0.1:11434");

        m_decModel = envOr("PRISMALUX_DECISION_MODEL",
                           QStringLiteral("tev1:4b"));
        if (!installed.contains(m_decModel))
            QSKIP(qPrintable(QStringLiteral("decision model '%1' non installato")
                                 .arg(m_decModel)));

        if (qEnvironmentVariableIsSet("PRISMALUX_TEST_MODEL")) {
            m_chatModel = qEnvironmentVariable("PRISMALUX_TEST_MODEL");
        } else {
            const QStringList pref = {
                QStringLiteral("qwen2.5-coder:1.5b"),
                QStringLiteral("qwen3:1.7b"),
                QStringLiteral("qwen2.5-coder:3b"),
                m_decModel,
            };
            for (const QString& p : pref)
                if (installed.contains(p)) { m_chatModel = p; break; }
        }
        if (m_chatModel.isEmpty())
            QSKIP("nessun modello conversazionale di test installato");

        /* Il registro decisionale è un file utente: backup + restore. */
        m_catalogPath = P::decisionModelsPath();
        QFile f(m_catalogPath);
        m_catalogExisted = f.exists();
        if (m_catalogExisted && f.open(QIODevice::ReadOnly))
            m_catalogBackup = f.readAll();
        f.close();

        qInfo() << "Decision model live:" << m_decModel
                << "| chat model:" << m_chatModel;
    }

    void cleanupTestCase()
    {
        QFile f(m_catalogPath);
        if (m_catalogExisted) {
            if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
                f.write(m_catalogBackup);
            f.close();
        } else if (f.exists()) {
            f.remove();
        }
    }

    void init()
    {
        QDir().mkpath(QDir::homePath() + QStringLiteral("/.prismalux"));
        writeCatalog();
    }

    /* L-1: pipeline completa con AiClient reale — classificazione,
       riverifica e ordine verdetto→token. */
    void pipelineRealeOrdineVerdettoToken()
    {
        DecisionEngine dec;
        AiClient ai;
        configure(ai, dec, /*verify=*/true);
        QVERIFY2(waitInstalled(dec), "decision model non risulta installato");
        QVERIFY(dec.active());

        QStringList events;
        QString intent, verifyReason;
        bool verified = false, approved = false;
        connect(&ai, &AiClient::token, this,
                [&events](const QString&) { events << QStringLiteral("token"); });
        connect(&ai, &AiClient::decisionClassified, this,
                [&events, &intent](const QString& i, double, bool) {
                    events << QStringLiteral("classified");
                    intent = i;
                });
        connect(&ai, &AiClient::decisionVerified, this,
                [&events, &verified, &approved, &verifyReason]
                (bool ok, const QString& why) {
                    events << QStringLiteral("verified");
                    verified = true; approved = ok; verifyReason = why;
                });

        QSignalSpy finished(&ai, &AiClient::finished);
        QSignalSpy errors(&ai, &AiClient::error);

        ai.chat(QStringLiteral("Sei un assistente conciso. Rispondi in italiano."),
                QStringLiteral("Qual \xc3\xa8 la capitale della Francia?"));
        QTRY_VERIFY_WITH_TIMEOUT(finished.count() + errors.count() > 0, 300000);

        qInfo() << "L-1 intento:" << intent
                << "| approvata:" << approved
                << "| motivo:" << verifyReason
                << "| risposta:" << finished.at(0).at(0).toString().left(160);

        QCOMPARE(errors.count(), 0);
        QCOMPARE(finished.count(), 1);
        QVERIFY2(!intent.isEmpty(), "classificazione assente");
        QVERIFY2(verified, "riverifica non eseguita");

        /* Il verdetto deve precedere l'unico token (risposta in blocco). */
        const int iv = events.indexOf(QStringLiteral("verified"));
        const int it = events.indexOf(QStringLiteral("token"));
        QVERIFY2(iv >= 0 && it > iv,
                 qPrintable(QStringLiteral("ordine errato: %1").arg(events.join(','))));
    }

    /* L-2: la stessa pipeline dietro AgentiPage — il badge mostra
       l'intento e poi l'esito, senza streaming anticipato. */
    void badgeConPipelineReale()
    {
        auto* ai = new AiClient;
        auto* dec = new DecisionEngine;
        configure(*ai, *dec, /*verify=*/true);
        QVERIFY2(waitInstalled(*dec), "decision model non risulta installato");

        auto* page = new AgentiPage(ai);
        page->resize(1280, 800);
        page->show();
        QVERIFY(QTest::qWaitForWindowExposed(page));

        auto* badge = page->findChild<QLabel*>(QStringLiteral("decisionBadge"));
        auto* log   = page->findChild<QTextBrowser*>(QStringLiteral("chatLog"));
        QVERIFY2(badge, "badge decisionale non trovato");
        QVERIFY2(log, "chatLog non trovato");

        QTextEdit* input = nullptr;
        QPushButton* run = nullptr;
        for (auto* e : page->findChildren<QTextEdit*>())
            if (!qobject_cast<QTextBrowser*>(e)) { input = e; break; }
        for (auto* b : page->findChildren<QPushButton*>())
            if (!b->isCheckable() && b->text().contains(QStringLiteral("Invia"))) {
                run = b; break;
            }
        QVERIFY2(input && run, "input o pulsante Invia non trovati");

        /* Il routing automatico può sostituire il modello scelto con un
           coder/vision installato (es. qwen3.5:4b, che qui va in thinking
           loop): per il test vogliamo il modello configurato. */
        for (auto* c : page->findChildren<QCheckBox*>())
            if (c->text().contains(QStringLiteral("Auto")))
                c->setChecked(false);

        /* La pagina popola il combo modelli in modo asincrono: cliccare
           prima che la lista arrivi invierebbe "(caricamento...)" a Ollama.
           Aspetta il modello reale e selezionalo. */
        QComboBox* modelCombo = nullptr;
        auto findModelCombo = [&]() -> QComboBox* {
            for (auto* c : page->findChildren<QComboBox*>())
                if (c->findData(m_chatModel, Qt::UserRole) >= 0)
                    return c;
            return nullptr;
        };
        QTRY_VERIFY_WITH_TIMEOUT((modelCombo = findModelCombo()) != nullptr, 15000);
        modelCombo->setCurrentIndex(
            modelCombo->findData(m_chatModel, Qt::UserRole));
        QCOMPARE(ai->model(), m_chatModel);

        input->setPlainText(QStringLiteral("Qual \xc3\xa8 la capitale della Francia?"));
        connect(ai, &AiClient::error, this, [](const QString& e) {
            qWarning() << "L-2 errore AiClient:" << e;
        });
        connect(ai, &AiClient::decisionClassified, this,
                [](const QString& i, double c, bool cached) {
                    qInfo() << "L-2 classified:" << i << c << cached;
                });
        connect(ai, &AiClient::decisionRetry, this, [](const QString& r) {
            qInfo() << "L-2 retry:" << r;
        });
        connect(ai, &AiClient::decisionVerified, this,
                [](bool ok, const QString& r) {
                    qInfo() << "L-2 verified:" << ok << r;
                });
        connect(ai, &AiClient::finished, this, [](const QString& f) {
            qInfo() << "L-2 finished:" << f.left(120);
        });
        QSignalSpy finishedSpy(ai, &AiClient::finished);
        QSignalSpy errorSpy(ai, &AiClient::error);
        run->click();

        /* Badge intento: appare appena la classificazione reale termina. */
        QTRY_VERIFY_WITH_TIMEOUT(
            badge->isVisible() || errorSpy.count() > 0 || finishedSpy.count() > 0,
            300000);
        QVERIFY2(badge->isVisible(),
                 qPrintable(QStringLiteral("badge mai mostrato (stage=%1) log=%2")
                                .arg(ai->decisionStage(),
                                     log->toPlainText().right(400))));
        QVERIFY2(badge->text().contains(QStringLiteral("%")),
                 qPrintable(badge->text()));

        const QString shot = qEnvironmentVariable("PRISMALUX_D72_LIVE_SCREENSHOT");
        if (!shot.isEmpty())
            QVERIFY(page->grab().save(shot));

        /* Attesa risposta completa (o errore esplicito). Il modello
           conversazionale può impiegare minuti su CPU (cold load). */
        QTRY_VERIFY_WITH_TIMEOUT(finishedSpy.count() + errorSpy.count() > 0, 300000);
        QVERIFY2(errorSpy.isEmpty(),
                 qPrintable(QStringLiteral("errore live: %1")
                                .arg(errorSpy.isEmpty()
                                         ? QString()
                                         : errorSpy.at(0).at(0).toString())));
        QTRY_VERIFY_WITH_TIMEOUT(
            badge->text().contains(QStringLiteral("risposta verificata"))
                || badge->text().contains(QStringLiteral("non verificata")),
            15000);

        qInfo() << "L-2 badge:" << badge->text()
                << "| log:" << log->toPlainText().right(200);

        delete page;   /* la pagina non possiede il client */
        ai->abort();
        delete ai;
        delete dec;
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    /* La generazione su CPU può superare il watchdog di 5 minuti di QtTest. */
    if (!qEnvironmentVariableIsSet("QTEST_FUNCTION_TIMEOUT"))
        qputenv("QTEST_FUNCTION_TIMEOUT", "600000");
    TestDecisionLive t;
    return QTest::qExec(&t, argc, argv);
}

#include "test_decision_live.moc"
