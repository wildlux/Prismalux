/* ══════════════════════════════════════════════════════════════
   test_ai_routing.cpp — _pickRoutedModel() + isCoderModel() (D-27)
   ──────────────────────────────────────────────────────────────
   CAT-A  Routing disattivato → mai cambia il fallback (3 test)
   CAT-B  Routing attivo — immagine allegata → modello vision (4 test)
   CAT-C  Routing attivo — dominio codice → modello coder (4 test)
   CAT-D  Routing attivo — dominio generale/STEM → nessun cambio (3 test)
   CAT-E  isCoderModel() — riconoscimento nomi modello (6 test)

   Build:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_ai_routing
     ./build_tests/test_ai_routing
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QString>
#include <QStringList>
#include "../ai_client.h"
#include "../prismalux_paths.h"

/* _pickRoutedModel() è definita in main_ai_pipeline.cpp e dichiarata in
 * main_ai_p.h — header interno (commento: "Non fare mai #include
 * main_ai_p.h da file esterni"). Forward-declaration diretta della stessa
 * firma (stessa tecnica di test_ai_math.cpp / test_ai_knowledge_lookup.cpp). */
QString _pickRoutedModel(bool autoRoutingEnabled, bool hasImage,
                          AiClient::QueryDomain domain,
                          const QStringList& installedModels,
                          const QString& fallback);

namespace P = PrismaluxPaths;

static const QStringList kInstalled = {
    "llama3.1:8b", "deepseek-coder:6.7b", "llama3.2-vision:11b", "qwen2.5:7b"
};

/* ══════════════════════════════════════════════════════════════
   CAT-A — Routing disattivato → sempre fallback invariato (3 test)
   ══════════════════════════════════════════════════════════════ */
class TestRoutingDisabled : public QObject {
    Q_OBJECT
private slots:

    void codingDomainIgnoredWhenDisabled() {
        const QString r = _pickRoutedModel(false, false, AiClient::DomainCoding,
                                            kInstalled, "llama3.1:8b");
        QCOMPARE(r, QString("llama3.1:8b"));
    }

    void imageIgnoredWhenDisabled() {
        const QString r = _pickRoutedModel(false, true, AiClient::DomainGeneral,
                                            kInstalled, "llama3.1:8b");
        QCOMPARE(r, QString("llama3.1:8b"));
    }

    void emptyInstalledListNoOp() {
        const QString r = _pickRoutedModel(false, true, AiClient::DomainCoding,
                                            {}, "fallback-model");
        QCOMPARE(r, QString("fallback-model"));
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B — Routing attivo, immagine allegata → modello vision,
   priorità assoluta anche su dominio codice (4 test)
   ══════════════════════════════════════════════════════════════ */
class TestImageRouting : public QObject {
    Q_OBJECT
private slots:

    void picksVisionModelWhenImageAttached() {
        const QString r = _pickRoutedModel(true, true, AiClient::DomainGeneral,
                                            kInstalled, "llama3.1:8b");
        QCOMPARE(r, QString("llama3.2-vision:11b"));
    }

    void imagePriorityOverCodingDomain() {
        /* anche con dominio codice, l'immagine allegata vince: senza un
         * modello vision la richiesta fallirebbe comunque */
        const QString r = _pickRoutedModel(true, true, AiClient::DomainCoding,
                                            kInstalled, "llama3.1:8b");
        QCOMPARE(r, QString("llama3.2-vision:11b"));
    }

    void noVisionModelInstalledFallsBack() {
        const QStringList noVision = { "llama3.1:8b", "deepseek-coder:6.7b" };
        const QString r = _pickRoutedModel(true, true, AiClient::DomainGeneral,
                                            noVision, "llama3.1:8b");
        QCOMPARE(r, QString("llama3.1:8b"));
    }

    void emptyFallbackStaysEmptyWithoutVision() {
        const QString r = _pickRoutedModel(true, true, AiClient::DomainGeneral, {}, "");
        QVERIFY(r.isEmpty());
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-C — Routing attivo, dominio codice → modello coder (4 test)
   ══════════════════════════════════════════════════════════════ */
class TestCodingRouting : public QObject {
    Q_OBJECT
private slots:

    void picksCoderModelForCodingDomain() {
        const QString r = _pickRoutedModel(true, false, AiClient::DomainCoding,
                                            kInstalled, "llama3.1:8b");
        QCOMPARE(r, QString("deepseek-coder:6.7b"));
    }

    void noCoderModelInstalledFallsBack() {
        const QStringList noCoder = { "llama3.1:8b", "llama3.2-vision:11b" };
        const QString r = _pickRoutedModel(true, false, AiClient::DomainCoding,
                                            noCoder, "llama3.1:8b");
        QCOMPARE(r, QString("llama3.1:8b"));
    }

    void firstMatchingCoderModelWins() {
        const QStringList twoCoders = { "codellama:13b", "deepseek-coder:6.7b" };
        const QString r = _pickRoutedModel(true, false, AiClient::DomainCoding,
                                            twoCoders, "fallback");
        QCOMPARE(r, QString("codellama:13b"));
    }

    void emptyModelNamesSkippedSafely() {
        const QStringList withEmpty = { "", "deepseek-coder:6.7b" };
        const QString r = _pickRoutedModel(true, false, AiClient::DomainCoding,
                                            withEmpty, "fallback");
        QCOMPARE(r, QString("deepseek-coder:6.7b"));
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-D — Routing attivo, dominio generale/STEM → nessun cambio:
   il "modello leggero" è già la scelta manuale (3 test)
   ══════════════════════════════════════════════════════════════ */
class TestGeneralDomainNoRouting : public QObject {
    Q_OBJECT
private slots:

    void generalDomainKeepsFallback() {
        const QString r = _pickRoutedModel(true, false, AiClient::DomainGeneral,
                                            kInstalled, "llama3.1:8b");
        QCOMPARE(r, QString("llama3.1:8b"));
    }

    void mathDomainKeepsFallback() {
        const QString r = _pickRoutedModel(true, false, AiClient::DomainMath,
                                            kInstalled, "qwen2.5:7b");
        QCOMPARE(r, QString("qwen2.5:7b"));
    }

    void physicsDomainKeepsFallback() {
        const QString r = _pickRoutedModel(true, false, AiClient::DomainPhysics,
                                            kInstalled, "qwen2.5:7b");
        QCOMPARE(r, QString("qwen2.5:7b"));
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-E — isCoderModel(): riconoscimento nomi modello (6 test)
   ══════════════════════════════════════════════════════════════ */
class TestIsCoderModel : public QObject {
    Q_OBJECT
private slots:

    void deepseekCoderRecognized() {
        QVERIFY(P::isCoderModel("deepseek-coder:6.7b"));
    }

    void codellamaRecognized() {
        QVERIFY(P::isCoderModel("codellama:13b"));
    }

    void starcoderRecognized() {
        QVERIFY(P::isCoderModel("starcoder2:15b"));
    }

    void genericChatModelNotCoder() {
        QVERIFY(!P::isCoderModel("llama3.1:8b"));
    }

    void visionModelNotCoder() {
        QVERIFY(!P::isCoderModel("llama3.2-vision:11b"));
    }

    void caseInsensitive() {
        QVERIFY(P::isCoderModel("DeepSeek-Coder:6.7B"));
    }
};

int main(int argc, char** argv)
{
    int status = 0;
    { TestRoutingDisabled       t; status |= QTest::qExec(&t, argc, argv); }
    { TestImageRouting          t; status |= QTest::qExec(&t, argc, argv); }
    { TestCodingRouting         t; status |= QTest::qExec(&t, argc, argv); }
    { TestGeneralDomainNoRouting t; status |= QTest::qExec(&t, argc, argv); }
    { TestIsCoderModel          t; status |= QTest::qExec(&t, argc, argv); }
    return status;
}

#include "test_ai_routing.moc"
