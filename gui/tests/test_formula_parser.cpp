/* ══════════════════════════════════════════════════════════════
   test_formula_parser.cpp — Unit test per FormulaParser
   ──────────────────────────────────────────────────────────────
   Categorie:
     CAT-A  Costruttore + validità (6 test)
     CAT-B  eval() — calcolo numerico (8 test)
     CAT-C  sample() — campionamento (5 test)
     CAT-D  tryExtract() — estrazione da testo AI (7 test)
     CAT-E  tryExtractXRange() — estrazione intervallo (4 test)
     CAT-F  tryExtractPoints() — estrazione punti (4 test)

   Build:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_formula_parser
     ./build_tests/test_formula_parser
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <cmath>
#include "../widgets/formula_parser.h"

/* ══════════════════════════════════════════════════════════════
   CAT-A — Costruttore + validità (6 test)
   ══════════════════════════════════════════════════════════════ */
class TestCostruttore : public QObject {
    Q_OBJECT
private slots:
    void variabileSemplice()  { QVERIFY(FormulaParser("x").ok()); }
    void polinomio()          { QVERIFY(FormulaParser("x^2 + 1").ok()); }
    void trigonoSin()         { QVERIFY(FormulaParser("sin(x)").ok()); }
    void stringaVuota()       { QVERIFY(!FormulaParser("").ok()); }
    void parentesiAperta()    { QVERIFY(!FormulaParser("(").ok()); }
    void erroreNonVuotoSeInvalido() {
        FormulaParser fp("");
        QVERIFY(!fp.err().isEmpty());
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B — eval() (8 test)
   ══════════════════════════════════════════════════════════════ */
class TestEval : public QObject {
    Q_OBJECT
private slots:
    void evalX()       { QCOMPARE(FormulaParser("x").eval(3.0),   3.0); }
    void eval2X()      { QCOMPARE(FormulaParser("2*x").eval(4.0), 8.0); }
    void evalXquadro() { QCOMPARE(FormulaParser("x^2").eval(3.0), 9.0); }
    void evalSinZero() { QVERIFY(std::fabs(FormulaParser("sin(x)").eval(0.0)) < 1e-9); }
    void evalCosZero() { QVERIFY(std::fabs(FormulaParser("cos(x)").eval(0.0) - 1.0) < 1e-9); }

    void evalDivZero() {
        /* 1/0 → NaN o Inf — non deve crashare */
        const double r = FormulaParser("1/x").eval(0.0);
        QVERIFY(std::isnan(r) || std::isinf(r));
    }

    void evalSqrt4() {
        QVERIFY(std::fabs(FormulaParser("sqrt(x)").eval(4.0) - 2.0) < 1e-9);
    }

    void evalPi() {
        const double r = FormulaParser("pi").eval(0.0);
        QVERIFY(std::fabs(r - M_PI) < 1e-9);
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-C — sample() (5 test)
   ══════════════════════════════════════════════════════════════ */
class TestSample : public QObject {
    Q_OBJECT
private slots:
    void nMenoDi2ReturnsEmpty() {
        QVERIFY(FormulaParser("x").sample(0, 1, 1).isEmpty());
    }

    void xMaxMenoDiXMin() {
        QVERIFY(FormulaParser("x").sample(5, 0, 10).isEmpty());
    }

    void campionamentoPuntiLineare() {
        const auto pts = FormulaParser("x").sample(-1, 1, 100);
        QCOMPARE(pts.size(), 100);
        QVERIFY(std::fabs(pts.first().x() - (-1.0)) < 1e-9);
        QVERIFY(std::fabs(pts.last().x()  -   1.0)  < 1e-9);
    }

    void nanEsclusiDaCampionamento() {
        /* log(x) per x≤0 → NaN → deve essere escluso */
        const auto pts = FormulaParser("log(x)").sample(-5, 5, 200);
        for (const QPointF& p : pts)
            QVERIFY2(std::isfinite(p.y()),
                     "sample() non deve includere punti NaN/Inf");
    }

    void numeroPuntiEsatto() {
        const auto pts = FormulaParser("x^2").sample(0, 10, 10);
        QCOMPARE(pts.size(), 10);
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-D — tryExtract() (7 test)
   ══════════════════════════════════════════════════════════════ */
class TestTryExtract : public QObject {
    Q_OBJECT
private slots:
    void patternYuguale() {
        QCOMPARE(FormulaParser::tryExtract("y = 2*x+1"), QString("2*x+1"));
    }

    void patternFdix() {
        QCOMPARE(FormulaParser::tryExtract("f(x) = sin(x)"), QString("sin(x)"));
    }

    void patternPlotta() {
        const QString r = FormulaParser::tryExtract("plotta sin(x)");
        QVERIFY2(!r.isEmpty(), "plotta sin(x) deve estrarre una formula");
        QVERIFY(r.contains("sin"));
    }

    void patternGrafico() {
        const QString r = FormulaParser::tryExtract("grafico di x^2");
        QVERIFY2(!r.isEmpty(), "grafico di x^2 deve estrarre una formula");
        QVERIFY(r.contains("x"));
    }

    void nessunaFormula() {
        QCOMPARE(FormulaParser::tryExtract("ciao come stai"), QString(""));
    }

    void testMaiuscolo() {
        const QString r = FormulaParser::tryExtract("F(x) = cos(x)");
        QVERIFY2(!r.isEmpty(), "F(x)=... maiuscolo deve essere riconosciuto");
        QVERIFY(r.contains("cos"));
    }

    void textConNewline() {
        /* I newline devono essere normalizzati a spazio prima della ricerca */
        const QString r = FormulaParser::tryExtract("y =\nx^2");
        QVERIFY(!r.isEmpty());
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-E — tryExtractXRange() (4 test)
   ══════════════════════════════════════════════════════════════ */
class TestTryExtractXRange : public QObject {
    Q_OBJECT
private slots:
    void daxAx() {
        double xMin, xMax;
        QVERIFY(FormulaParser::tryExtractXRange("da x=-5 a x=5", xMin, xMax));
        QVERIFY(std::fabs(xMin - (-5.0)) < 1e-9);
        QVERIFY(std::fabs(xMax -   5.0)  < 1e-9);
    }

    void xIn() {
        double xMin, xMax;
        QVERIFY(FormulaParser::tryExtractXRange("x in [-10, 10]", xMin, xMax));
        QVERIFY(std::fabs(xMin - (-10.0)) < 1e-9);
        QVERIFY(std::fabs(xMax -  10.0)   < 1e-9);
    }

    void xDa() {
        double xMin, xMax;
        QVERIFY(FormulaParser::tryExtractXRange("x da -3 a 3", xMin, xMax));
        QVERIFY(std::fabs(xMin - (-3.0)) < 1e-9);
        QVERIFY(std::fabs(xMax -   3.0)  < 1e-9);
    }

    void nessunoIntervallo() {
        double xMin, xMax;
        QVERIFY(!FormulaParser::tryExtractXRange("nessun intervallo qui", xMin, xMax));
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-F — tryExtractPoints() (4 test)
   Note: tryExtractPoints richiede una keyword "cartesiana" nel testo
   (grafico/punto/punti/piano/traccia/disegna/plotta) — false positive guard.
   ══════════════════════════════════════════════════════════════ */
class TestTryExtractPoints : public QObject {
    Q_OBJECT
private slots:
    void coppiaxEy() {
        const auto pts = FormulaParser::tryExtractPoints("punti: x=3, y=5");
        QVERIFY2(pts.size() >= 1, "x=3,y=5 deve essere estratto come punto");
        bool found = false;
        for (const QPointF& p : pts)
            if (std::fabs(p.x() - 3.0) < 1e-9 && std::fabs(p.y() - 5.0) < 1e-9)
                found = true;
        QVERIFY(found);
    }

    void parentesi() {
        const auto pts = FormulaParser::tryExtractPoints("punto (1, 2)");
        QVERIFY2(pts.size() >= 1, "(1,2) dopo 'punto' deve essere estratto");
    }

    void nessunaKeyword_ReturnsEmpty() {
        /* Senza keyword cartesiana (grafico/punto/...) → lista vuota */
        QVERIFY(FormulaParser::tryExtractPoints("x=1, y=2").isEmpty());
    }

    void puntiMultipli() {
        const auto pts = FormulaParser::tryExtractPoints("plotta i punti (1,2) e (3,4)");
        QVERIFY2(pts.size() >= 2, "due coppie di coordinate devono generare due punti");
    }
};

/* ══════════════════════════════════════════════════════════════
   Runner
   ══════════════════════════════════════════════════════════════ */
int main(int argc, char** argv)
{
    int status = 0;
    { TestCostruttore t;      status |= QTest::qExec(&t, argc, argv); }
    { TestEval t;             status |= QTest::qExec(&t, argc, argv); }
    { TestSample t;           status |= QTest::qExec(&t, argc, argv); }
    { TestTryExtract t;       status |= QTest::qExec(&t, argc, argv); }
    { TestTryExtractXRange t; status |= QTest::qExec(&t, argc, argv); }
    { TestTryExtractPoints t; status |= QTest::qExec(&t, argc, argv); }
    return status;
}

#include "test_formula_parser.moc"
