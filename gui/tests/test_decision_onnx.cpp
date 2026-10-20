/* ══════════════════════════════════════════════════════════════
   test_decision_onnx.cpp — backend ONNX locale

   Categorie:
     CAT-A  parità feature C++ ↔ generatore Python (fixture JSON)
     CAT-B  caricamento modelli inclusi
     CAT-C  classificazione A/B/C/D
     CAT-D  verifica N/Y (pertinente / rifiuto / risposta vuota)
     CAT-E  integrazione DecisionEngine (refresh, active, cache)
     CAT-F  modelli mancanti → errore esplicito, verifica fail-open

   I test che richiedono ONNX Runtime e i file .onnx si saltano
   (QSKIP) su build senza runtime: il codice deve comunque compilare.
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>
#include <cmath>

#include "../decision_engine.h"
#include "../decision_onnx.h"

class TestDecisionOnnx : public QObject
{
    Q_OBJECT

private:
    QString dataPath() const
    {
        QString p = QFINDTESTDATA("data/decision_onnx_features.json");
        if (p.isEmpty())
            p = QStringLiteral(PRISMALUX_ROOT)
                + QStringLiteral("/gui/tests/data/decision_onnx_features.json");
        return p;
    }

    void compareVec(const QVector<float>& got, const QJsonArray& expected,
                    const QString& what)
    {
        QCOMPARE(got.size(), expected.size());
        for (int i = 0; i < expected.size(); ++i) {
            const double exp = expected.at(i).toDouble();
            const double act = double(got.at(i));
            QVERIFY2(std::abs(act - exp) < 1e-5,
                     qPrintable(QStringLiteral("%1 bucket %2: C++=%3 Python=%4")
                                    .arg(what).arg(i)
                                    .arg(act, 0, 'g', 10)
                                    .arg(exp, 0, 'g', 10)));
        }
    }

private slots:

    void initTestCase()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("Prismalux"));
        QCoreApplication::setApplicationName(QStringLiteral("GUI"));
    }

    /* ── CAT-A ──────────────────────────────────────────────────
       Se questa parità salta, i modelli .onnx sono disallineati
       rispetto alle feature C++: vanno rigenerati. */
    void parita_feature_testo()
    {
        QFile f(dataPath());
        QVERIFY2(f.open(QIODevice::ReadOnly),
                 qPrintable(QStringLiteral("fixture non trovata: %1").arg(dataPath())));
        const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
        QCOMPARE(root.value("feature_dim").toInt(), DecisionOnnx::kFeatureDim);

        const QJsonObject texts = root.value("texts").toObject();
        QVERIFY(!texts.isEmpty());
        for (auto it = texts.begin(); it != texts.end(); ++it) {
            const QJsonObject o = it.value().toObject();
            const auto got = DecisionOnnx::featuresText(o.value("text").toString());
            compareVec(got, o.value("features").toArray(), it.key());
        }
    }

    void parita_feature_coppia()
    {
        QFile f(dataPath());
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
        const QJsonObject pairs = root.value("pairs").toObject();
        QVERIFY(!pairs.isEmpty());

        for (auto it = pairs.begin(); it != pairs.end(); ++it) {
            const QJsonObject o = it.value().toObject();
            const auto got = DecisionOnnx::featuresPair(
                o.value("question").toString(),
                o.value("answer").toString());
            compareVec(got, o.value("features").toArray(), it.key());
        }
    }

    /* ── CAT-B ────────────────────────────────────────────────── */
    void carica_modelli_inclusi()
    {
        if (!DecisionOnnx::runtimeAvailable())
            QSKIP("ONNX Runtime non disponibile in questa build");
        if (!DecisionOnnx::modelsPresent())
            QSKIP("modelli ONNX non presenti in Frameworks/onnx/models");

        DecisionOnnx onnx;
        QVERIFY2(onnx.load(), qPrintable(onnx.lastError()));
        QVERIFY(onnx.isReady());
        QVERIFY(onnx.lastError().isEmpty());
    }

    /* ── CAT-C ────────────────────────────────────────────────── */
    void classificazione_quattro_tipologie()
    {
        if (!DecisionOnnx::runtimeAvailable() || !DecisionOnnx::modelsPresent())
            QSKIP("ONNX Runtime o modelli non disponibili");

        DecisionOnnx onnx;
        QVERIFY2(onnx.load(), qPrintable(onnx.lastError()));

        const QVector<QPair<QString, int>> cases = {
            { QStringLiteral("scrivi una funzione python che legge un file json"), 0 },
            { QStringLiteral("dimostra che la radice di 2 è irrazionale"),         1 },
            { QStringLiteral("ciao, raccontami una barzelletta"),                  2 },
            { QStringLiteral("qual è la capitale del Giappone"),                   3 },
        };
        for (const auto& c : cases) {
            const auto r = onnx.classify(c.first);
            QVERIFY2(r.ok, qPrintable(r.error));
            QCOMPARE(r.label, c.second);
            QVERIFY2(r.confidence > 0.40,
                     qPrintable(QStringLiteral("confidenza bassa: %1")
                                    .arg(r.confidence)));
        }
    }

    /* ── CAT-D ────────────────────────────────────────────────── */
    void verifica_pertinenza()
    {
        if (!DecisionOnnx::runtimeAvailable() || !DecisionOnnx::modelsPresent())
            QSKIP("ONNX Runtime o modelli non disponibili");

        DecisionOnnx onnx;
        QVERIFY2(onnx.load(), qPrintable(onnx.lastError()));

        const auto good = onnx.verify(
            QStringLiteral("come si ordina una lista in python"),
            QStringLiteral("Per ordinare una lista in python usa sorted() "
                           "oppure il metodo sort()."));
        QVERIFY2(good.ok, qPrintable(good.error));
        QCOMPARE(good.label, 1);   /* Y */
        QVERIFY(good.confidence > 0.40);

        const auto refusal = onnx.verify(
            QStringLiteral("come si ordina una lista in python"),
            QStringLiteral("Non lo so."));
        QVERIFY2(refusal.ok, qPrintable(refusal.error));
        QCOMPARE(refusal.label, 0);   /* N */
        QVERIFY(refusal.confidence > 0.40);

        const auto empty = onnx.verify(
            QStringLiteral("spiegami il teorema di pitagora"),
            QStringLiteral("Ok."));
        QVERIFY(empty.ok);
        QCOMPARE(empty.label, 0);
    }

    /* ── CAT-E ────────────────────────────────────────────────── */
    void integrazione_decision_engine()
    {
        if (!DecisionOnnx::runtimeAvailable() || !DecisionOnnx::modelsPresent())
            QSKIP("ONNX Runtime o modelli non disponibili");

        DecisionEngine dec;
        dec.setBackend(DecisionEngine::BackendOnnx);
        dec.setEnabled(true);
        dec.setModel(QStringLiteral("onnx-decision"));
        dec.setVerifyEnabled(true);
        dec.setThreshold(0.5);
        dec.refreshInstalled();

        QVERIFY2(!dec.installedModels().isEmpty(),
                 qPrintable(dec.onnxError()));
        QVERIFY(dec.active());

        bool done = false;
        DecisionEngine::ClassifyResult cr;
        dec.classify(QStringLiteral("scrivi una funzione python che legge un file json"),
                     [&done, &cr](const DecisionEngine::ClassifyResult& r) {
                         cr = r; done = true;
                     });
        QVERIFY(done);
        QVERIFY2(cr.ok, qPrintable(cr.error));
        QCOMPARE(cr.intent, QStringLiteral("coding"));
        QVERIFY(cr.confidence > 0.40);

        bool vdone = false;
        DecisionEngine::VerifyResult vr;
        dec.verify(QStringLiteral("qual è la capitale della Francia"),
                   QStringLiteral("La capitale della Francia è Parigi."),
                   QStringLiteral("other"),
                   [&vdone, &vr](const DecisionEngine::VerifyResult& r) {
                       vr = r; vdone = true;
                   });
        QVERIFY(vdone);
        QVERIFY2(vr.ok, qPrintable(vr.error));
        QVERIFY(vr.approved);

        bool ndone = false;
        DecisionEngine::VerifyResult nr;
        dec.verify(QStringLiteral("qual è la capitale della Francia"),
                   QStringLiteral("Non lo so."),
                   QStringLiteral("other"),
                   [&ndone, &nr](const DecisionEngine::VerifyResult& r) {
                       nr = r; ndone = true;
                   });
        QVERIFY(ndone);
        QVERIFY(nr.ok);
        QVERIFY(!nr.approved);
    }

    /* ── CAT-F ────────────────────────────────────────────────── */
    void modelli_mancanti_errore_esplicito()
    {
        const QString old = qEnvironmentVariable("PRISMALUX_DECISION_ONNX_DIR");
        const QString emptyDir = QDir::tempPath()
            + QStringLiteral("/prismalux_onnx_missing_%1")
                  .arg(QCoreApplication::applicationPid());
        QDir(emptyDir).removeRecursively();
        qputenv("PRISMALUX_DECISION_ONNX_DIR", emptyDir.toLocal8Bit());

        {
            DecisionOnnx onnx;
            QVERIFY(!onnx.load());
            QVERIFY(!onnx.isReady());
            QVERIFY2(onnx.lastError().contains(QStringLiteral("ONNX")),
                     qPrintable(onnx.lastError()));

            const auto r = onnx.classify(QStringLiteral("ciao"));
            QVERIFY(!r.ok);
            QVERIFY(!r.error.isEmpty());
        }

        {
            DecisionEngine dec;
            dec.setBackend(DecisionEngine::BackendOnnx);
            dec.setEnabled(true);
            dec.setModel(QStringLiteral("onnx-decision"));
            dec.setVerifyEnabled(true);
            dec.refreshInstalled();

            QVERIFY(dec.installedModels().isEmpty());
            QVERIFY(dec.onnxError().contains(QStringLiteral("ONNX")));

            bool done = false;
            DecisionEngine::VerifyResult vr;
            dec.verify(QStringLiteral("q"), QStringLiteral("a"),
                       QStringLiteral("coding"),
                       [&done, &vr](const DecisionEngine::VerifyResult& r) {
                           vr = r; done = true;
                       });
            QVERIFY(done);
            QVERIFY(!vr.ok);
            QVERIFY(vr.approved);   /* fail-open */
        }

        if (old.isEmpty()) qunsetenv("PRISMALUX_DECISION_ONNX_DIR");
        else               qputenv("PRISMALUX_DECISION_ONNX_DIR", old.toLocal8Bit());
    }
};

int main(int argc, char** argv)
{
    const QString home = QDir::tempPath()
        + QStringLiteral("/prismalux_onnx_test_%1")
              .arg(QCoreApplication::applicationPid());
    QDir(home).removeRecursively();
    QDir().mkpath(home + QStringLiteral("/.prismalux"));
    QDir().mkpath(home + QStringLiteral("/.config"));
    qputenv("HOME", home.toLocal8Bit());
    qputenv("XDG_CONFIG_HOME", (home + QStringLiteral("/.config")).toLocal8Bit());

    QApplication app(argc, argv);
    TestDecisionOnnx t;
    const int status = QTest::qExec(&t, argc, argv);

    QDir(home).removeRecursively();
    return status;
}

#include "test_decision_onnx.moc"
