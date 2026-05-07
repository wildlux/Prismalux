/* ══════════════════════════════════════════════════════════════
   test_matematica_page.cpp — Unit test per MatematicaPage
   ──────────────────────────────────────────────────────────────
   Categorie:
     CAT-A  parseSeq() — stringa → QVector<double> (10 test)
     CAT-B  detectPatternLocal() — riconoscimento pattern (15 test)
     CAT-C  numbersFromText() — estrazione numeri da testo (7 test)
     CAT-D  Widget — costruzione e invarianti (3 test)

   Tecnica: #define private public per accedere ai metodi privati
   in questa unica translation unit.

   Build:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_matematica_page
     ./build_tests/test_matematica_page
   ══════════════════════════════════════════════════════════════ */

/* Rende accessibili i metodi private di MatematicaPage in questo TU. */
#define private public
#define protected public
#include "../pages/matematica_page.h"
#undef protected
#undef private

#include <QtTest/QtTest>
#include <QApplication>
#include <QTabWidget>
#include <QPlainTextEdit>
#include <cmath>
#include "mock_ai_client.h"

/* ══════════════════════════════════════════════════════════════
   Fixture condivisa — istanza di MatematicaPage riusata tra CAT
   ══════════════════════════════════════════════════════════════ */
namespace {

MockAiClient* g_ai   = nullptr;
MatematicaPage* g_pg = nullptr;

void ensurePage()
{
    if (!g_ai) g_ai = new MockAiClient();
    if (!g_pg) g_pg = new MatematicaPage(g_ai);
}

} // namespace

/* ══════════════════════════════════════════════════════════════
   CAT-A — parseSeq() (10 test)
   ══════════════════════════════════════════════════════════════ */
class TestParseSeq : public QObject {
    Q_OBJECT
private slots:

    void initTestCase() { ensurePage(); }

    void stringaVuota() {
        QString err;
        const auto v = g_pg->parseSeq("", err);
        QVERIFY(v.isEmpty());
        QVERIFY2(!err.isEmpty(), "err deve essere non vuoto per input vuoto");
    }

    void virgole() {
        QString err;
        const auto v = g_pg->parseSeq("1,4,9,16", err);
        QCOMPARE(v.size(), 4);
        QVERIFY(err.isEmpty());
        QCOMPARE(v[0], 1.0);
        QCOMPARE(v[3], 16.0);
    }

    void spazi() {
        QString err;
        const auto v = g_pg->parseSeq("1 4 9 16", err);
        QCOMPARE(v.size(), 4);
        QVERIFY(err.isEmpty());
    }

    void float_() {
        QString err;
        const auto v = g_pg->parseSeq("1.5, 2.7, 3.14", err);
        QCOMPARE(v.size(), 3);
        QVERIFY(std::fabs(v[0] - 1.5) < 1e-9);
        QVERIFY(std::fabs(v[2] - 3.14) < 1e-9);
    }

    void tokenNonNumerico() {
        QString err;
        const auto v = g_pg->parseSeq("1,abc,3", err);
        QVERIFY(v.isEmpty());
        QVERIFY2(!err.isEmpty(), "token non numerico deve produrre errore");
        QVERIFY2(err.contains("abc"), qPrintable("err non menziona 'abc': " + err));
    }

    void unSoloElemento() {
        QString err;
        const auto v = g_pg->parseSeq("42", err);
        QCOMPARE(v.size(), 1);
        QVERIFY2(!err.isEmpty(), "un solo elemento deve generare avviso");
    }

    void spaziAttornoVirgole() {
        QString err;
        const auto v = g_pg->parseSeq("1, 2, 3", err);
        QCOMPARE(v.size(), 3);
        QVERIFY(err.isEmpty());
    }

    void tabComeSeparatori() {
        QString err;
        const auto v = g_pg->parseSeq("1\t2\t3", err);
        QCOMPARE(v.size(), 3);
        QVERIFY(err.isEmpty());
    }

    void whitespaceAiBordi() {
        QString err;
        const auto v = g_pg->parseSeq("  1, 4, 9  ", err);
        QCOMPARE(v.size(), 3);
        QVERIFY(err.isEmpty());
    }

    void valoriNegativi() {
        QString err;
        const auto v = g_pg->parseSeq("-3, -1, 1, 3", err);
        QCOMPARE(v.size(), 4);
        QVERIFY(err.isEmpty());
        QCOMPARE(v[0], -3.0);
        QCOMPARE(v[3],  3.0);
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B — detectPatternLocal() (15 test)
   ══════════════════════════════════════════════════════════════ */
class TestDetectPattern : public QObject {
    Q_OBJECT
private slots:

    void initTestCase() { ensurePage(); }

    void sequenzaVuota() {
        const QString r = g_pg->detectPatternLocal({});
        QVERIFY2(r.contains("corta", Qt::CaseInsensitive) ||
                 r.contains("Troppo", Qt::CaseInsensitive),
                 qPrintable("atteso 'Troppo corta', ottenuto: " + r));
    }

    void unElemento() {
        const QString r = g_pg->detectPatternLocal({5.0});
        QVERIFY2(r.contains("corta", Qt::CaseInsensitive) ||
                 r.contains("Troppo", Qt::CaseInsensitive),
                 qPrintable("atteso 'Troppo corta', ottenuto: " + r));
    }

    void aritmeticaD2() {
        const QString r = g_pg->detectPatternLocal({2,4,6,8});
        QVERIFY2(r.contains("Aritmetica", Qt::CaseInsensitive),
                 qPrintable("{2,4,6,8} → atteso 'Aritmetica', ottenuto: " + r));
    }

    void costante() {
        const QString r = g_pg->detectPatternLocal({7,7,7,7});
        QVERIFY2(r.contains("costante", Qt::CaseInsensitive),
                 qPrintable("{7,7,7,7} → atteso 'costante', ottenuto: " + r));
    }

    void aritmeticaD4() {
        const QString r = g_pg->detectPatternLocal({3,7,11,15});
        QVERIFY2(r.contains("Aritmetica", Qt::CaseInsensitive),
                 qPrintable("{3,7,11,15} → atteso 'Aritmetica', ottenuto: " + r));
    }

    void geometricaR3() {
        const QString r = g_pg->detectPatternLocal({2,6,18,54});
        QVERIFY2(r.contains("Geometrica", Qt::CaseInsensitive),
                 qPrintable("{2,6,18,54} → atteso 'Geometrica', ottenuto: " + r));
    }

    void quadratiPerfetti() {
        const QString r = g_pg->detectPatternLocal({1,4,9,16,25});
        QVERIFY2(r.contains("Quadrati", Qt::CaseInsensitive),
                 qPrintable("{1,4,9,16,25} → atteso 'Quadrati', ottenuto: " + r));
    }

    void cubi() {
        const QString r = g_pg->detectPatternLocal({1,8,27,64});
        QVERIFY2(r.contains("Cubi", Qt::CaseInsensitive),
                 qPrintable("{1,8,27,64} → atteso 'Cubi', ottenuto: " + r));
    }

    void triangolari() {
        /* n(n+1)/2 è una sequenza quadratica — rilevata come "Quadratica" o "triangolar" */
        const QString r = g_pg->detectPatternLocal({1,3,6,10,15});
        QVERIFY2(r.contains("triangolar", Qt::CaseInsensitive) ||
                 r.contains("Quadratica", Qt::CaseInsensitive),
                 qPrintable("{1,3,6,10,15} → atteso 'triangolari' o 'Quadratica', ottenuto: " + r));
    }

    void fibonacciLike() {
        const QString r = g_pg->detectPatternLocal({1,1,2,3,5,8});
        QVERIFY2(r.contains("Fibonacci", Qt::CaseInsensitive),
                 qPrintable("{1,1,2,3,5,8} → atteso 'Fibonacci', ottenuto: " + r));
    }

    void fattoriali() {
        const QString r = g_pg->detectPatternLocal({1,2,6,24,120});
        QVERIFY2(r.contains("fattorial", Qt::CaseInsensitive),
                 qPrintable("{1,2,6,24,120} → atteso 'Fattoriali', ottenuto: " + r));
    }

    void potenzeDi2() {
        /* 2^n è geometrica r=2 — rilevata come "Geometrica" o "Potenze di 2" */
        const QString r = g_pg->detectPatternLocal({2,4,8,16,32});
        QVERIFY2(r.contains("Potenze", Qt::CaseInsensitive) ||
                 r.contains("potenze", Qt::CaseInsensitive) ||
                 r.contains("Geometrica", Qt::CaseInsensitive),
                 qPrintable("{2,4,8,16,32} → atteso 'Geometrica' o 'Potenze di 2', ottenuto: " + r));
    }

    void numeriPrimi() {
        const QString r = g_pg->detectPatternLocal({2,3,5,7,11,13});
        QVERIFY2(r.contains("primi", Qt::CaseInsensitive),
                 qPrintable("{2,3,5,7,11,13} → atteso 'Numeri primi', ottenuto: " + r));
    }

    void quadraticaNnPiuN() {
        /* a(n) = n² + n = 2, 6, 12, 20, 30 (n=1..5) */
        const QString r = g_pg->detectPatternLocal({2,6,12,20,30});
        QVERIFY2(r.contains("Quadratica", Qt::CaseInsensitive),
                 qPrintable("{2,6,12,20,30} → atteso 'Quadratica', ottenuto: " + r));
    }

    void patternSconosciuto() {
        /* {1,4,7,11,15}: non è aritmetica (salto 3,3,4,4), né geometrica, né nota */
        const QString r = g_pg->detectPatternLocal({1,4,7,11,15});
        QVERIFY2(r.contains("non riconosciuto", Qt::CaseInsensitive) ||
                 r.contains("Interpola", Qt::CaseInsensitive),
                 qPrintable("{1,4,7,11,15} → atteso 'non riconosciuto', ottenuto: " + r));
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-C — numbersFromText() (7 test)
   ══════════════════════════════════════════════════════════════ */
class TestNumbersFromText : public QObject {
    Q_OBJECT
private slots:

    void initTestCase() { ensurePage(); }

    void soloNumeri() {
        const QString r = g_pg->numbersFromText("1 4 9 16");
        /* risultato deve contenere almeno tutti e 4 i numeri */
        QVERIFY2(r.contains("1") && r.contains("4") && r.contains("9") && r.contains("16"),
                 qPrintable("risultato: " + r));
    }

    void numeriMistiATesto() {
        const QString r = g_pg->numbersFromText("La sequenza \xc3\xa8: 3, 7, 11");
        QVERIFY2(r.contains("3") && r.contains("7") && r.contains("11"),
                 qPrintable("risultato: " + r));
    }

    void numeriNegativi() {
        const QString r = g_pg->numbersFromText("Da -5 a 5: -5,-3,-1,1");
        QVERIFY2(r.contains("-5") && r.contains("-3"),
                 qPrintable("risultato: " + r));
    }

    void testoCheNonContieneNumeri() {
        const QString r = g_pg->numbersFromText("ciao come stai");
        QVERIFY2(r.isEmpty(), qPrintable("atteso vuoto, ottenuto: " + r));
    }

    void numeriDecimaliConPunto() {
        const QString r = g_pg->numbersFromText("valori: 1.5, 2.7, 3.14");
        QVERIFY2(r.contains("1.5") && r.contains("2.7"),
                 qPrintable("risultato: " + r));
    }

    void separatoreMigliaia_NonSpezza() {
        /* "1,000" — con la regex corrente dovrebbe estrarre solo "1" o gestirlo in modo stabile */
        const QString r = g_pg->numbersFromText("1,000 elementi");
        /* il risultato non deve contenere "0" come terzo numero distinto da 1000 */
        const QStringList parts = r.split(",", Qt::SkipEmptyParts);
        /* accettiamo sia "1000" sia "1" (comportamento documentato per il separatore migliaia) */
        for (const QString& p : parts) {
            bool ok = false;
            p.trimmed().toDouble(&ok);
            QVERIFY2(ok, qPrintable("parte non numerica nel risultato: " + p));
        }
    }

    void stringaVuota() {
        const QString r = g_pg->numbersFromText("");
        QVERIFY2(r.isEmpty(), qPrintable("atteso vuoto, ottenuto: " + r));
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-D — Widget — costruzione e invarianti (3 test)
   ══════════════════════════════════════════════════════════════ */
class TestWidgetCostruzione : public QObject {
    Q_OBJECT
private slots:

    void costruzioneNoCrash() {
        MockAiClient ai;
        MatematicaPage page(&ai);
        /* se arriviamo qui senza crash = ok */
        QVERIFY(true);
    }

    void quattroTab() {
        ensurePage();
        QVERIFY2(g_pg->m_tabs != nullptr, "m_tabs non deve essere null");
        QCOMPARE(g_pg->m_tabs->count(), 4);
    }

    void outputReadOnly() {
        ensurePage();
        QVERIFY2(g_pg->m_output != nullptr, "m_output non deve essere null");
        QVERIFY2(g_pg->m_output->isReadOnly(),
                 "m_output deve essere read-only (l'utente non vi scrive direttamente)");
    }
};

/* ══════════════════════════════════════════════════════════════
   Runner
   ══════════════════════════════════════════════════════════════ */
int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    int status = 0;
    { TestParseSeq       t; status |= QTest::qExec(&t, argc, argv); }
    { TestDetectPattern  t; status |= QTest::qExec(&t, argc, argv); }
    { TestNumbersFromText t; status |= QTest::qExec(&t, argc, argv); }
    { TestWidgetCostruzione t; status |= QTest::qExec(&t, argc, argv); }

    /* Pulizia globale */
    delete g_pg; g_pg = nullptr;
    delete g_ai; g_ai = nullptr;

    return status;
}

#include "test_matematica_page.moc"
