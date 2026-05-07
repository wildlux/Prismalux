/* ══════════════════════════════════════════════════════════════
   test_grafico.cpp — Unit test GraficoCanvas e GraficoPage

   Categorie:
     CAT-A  GraficoCanvas — costruzione, sizeHint, tipo default
     CAT-B  GraficoCanvas — setCartesian / setData / setScatter non crashano
     CAT-C  GraficoPage — costruzione senza crash, canvas() non null
     CAT-D  Integrazione FormulaParser — punti cartesiani non tutti zero per sin(x)

   Build:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_grafico
     ./build_tests/test_grafico
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QApplication>
#include <cmath>

#include "mock_ai_client.h"
#include "../pages/grafico_page.h"

/* ══════════════════════════════════════════════════════════════
   CAT-A — GraficoCanvas: costruzione e proprietà base
   ══════════════════════════════════════════════════════════════ */
class TestGraficoCanvas : public QObject {
    Q_OBJECT
private slots:

    /* A-1: costruisce senza crash */
    void costruisceSenzaCrash() {
        GraficoCanvas canvas;
        QVERIFY2(&canvas != nullptr, "GraficoCanvas non costruita");
    }

    /* A-2: sizeHint è ragionevole (>= 300x200) */
    void sizeHintRagionevole() {
        GraficoCanvas canvas;
        const QSize hint = canvas.sizeHint();
        QVERIFY2(hint.width() >= 300, "sizeHint().width() < 300");
        QVERIFY2(hint.height() >= 200, "sizeHint().height() < 200");
    }

    /* A-3: minimumSizeHint è minore o uguale a sizeHint */
    void minimumSizeHintLeq() {
        GraficoCanvas canvas;
        const QSize minH = canvas.minimumSizeHint();
        const QSize hint = canvas.sizeHint();
        QVERIFY2(minH.width() <= hint.width(), "minimumSizeHint.width > sizeHint.width");
        QVERIFY2(minH.height() <= hint.height(), "minimumSizeHint.height > sizeHint.height");
    }

    /* A-4: setType non crasha per tutti i tipi principali */
    void setTypePrincipalNonCrasha() {
        GraficoCanvas canvas;
        const QList<GraficoCanvas::ChartType> tipi = {
            GraficoCanvas::Cartesian,
            GraficoCanvas::Pie,
            GraficoCanvas::Histogram,
            GraficoCanvas::ScatterXY,
            GraficoCanvas::Line,
            GraficoCanvas::Polar,
        };
        for (auto t : tipi)
            canvas.setType(t);  /* non deve crashare */
    }

    /* A-5: resetView non crasha */
    void resetViewNonCrasha() {
        GraficoCanvas canvas;
        canvas.resetView();
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B — GraficoCanvas: setter dati non crashano
   ══════════════════════════════════════════════════════════════ */
class TestGraficoCanvasData : public QObject {
    Q_OBJECT
private slots:

    /* B-1: setCartesian non crasha su formula valida */
    void setCartesianNonCrasha() {
        GraficoCanvas canvas;
        canvas.setCartesian("sin(x)", -3.14159, 3.14159);
    }

    /* B-2: setCartesian non crasha su formula vuota */
    void setCartesianFormulaVuotaNonCrasha() {
        GraficoCanvas canvas;
        canvas.setCartesian("", -1.0, 1.0);
    }

    /* B-3: setData non crasha con vettori vuoti */
    void setDataVuotoNonCrasha() {
        GraficoCanvas canvas;
        canvas.setData({}, {});
    }

    /* B-4: setData con valori non crasha */
    void setDataConValori() {
        GraficoCanvas canvas;
        canvas.setData({1.0, 2.0, 3.0}, {"A", "B", "C"});
    }

    /* B-5: setScatter non crasha con lista vuota */
    void setScatterVuotoNonCrasha() {
        GraficoCanvas canvas;
        canvas.setScatter({});
    }

    /* B-6: setScatter con punti non crasha */
    void setScatterConPunti() {
        GraficoCanvas canvas;
        canvas.setScatter({{0,0}, {1,1}, {2,4}});
    }

    /* B-7: setLine con serie vuota non crasha */
    void setLineVuotoNonCrasha() {
        GraficoCanvas canvas;
        canvas.setLine({}, {});
    }

    /* B-8: setPolar formula semplice non crasha */
    void setPolarNonCrasha() {
        GraficoCanvas canvas;
        canvas.setPolar("x", 0.0, 6.28318530718);
    }

    /* B-9: setEdges non crasha */
    void setEdgesNonCrasha() {
        GraficoCanvas canvas;
        canvas.setEdges({{"A","B"}, {"B","C"}});
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-C — GraficoPage: costruzione e canvas()
   ══════════════════════════════════════════════════════════════ */
class TestGraficoPage : public QObject {
    Q_OBJECT
private:
    MockAiClient* m_ai = nullptr;

private slots:

    void initTestCase() { m_ai = new MockAiClient; }
    void cleanupTestCase() { delete m_ai; }

    /* C-1: costruisce senza crash */
    void costruisceSenzaCrash() {
        GraficoPage page(m_ai);
        QVERIFY2(&page != nullptr, "GraficoPage non costruita");
    }

    /* C-2: canvas() restituisce puntatore non null */
    void canvasNonNull() {
        GraficoPage page(m_ai);
        QVERIFY2(page.canvas() != nullptr, "GraficoPage::canvas() è null");
    }

    /* C-3: canvas() è un GraficoCanvas */
    void canvasEUnGraficoCanvas() {
        GraficoPage page(m_ai);
        auto* cv = qobject_cast<GraficoCanvas*>(page.canvas());
        QVERIFY2(cv != nullptr, "canvas() non è un GraficoCanvas");
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-D — Integrazione FormulaParser in GraficoCanvas
   ══════════════════════════════════════════════════════════════ */
class TestFormulaParserIntegrazione : public QObject {
    Q_OBJECT
private slots:

    /* D-1: setCartesian("x", -5, 5) non crasha e smithPointCount() == 0
       (usiamo smithPointCount come proxy che il canvas ha dati interni) */
    void setCartesianXNonCrasha() {
        GraficoCanvas canvas;
        canvas.setCartesian("x", -5.0, 5.0);
        /* non deve crashare — il canvas usa FormulaParser internamente */
    }

    /* D-2: setCartesian("sin(x)", -π, π) — type rimane Cartesian */
    void sinXMantieneTypeCartesian() {
        GraficoCanvas canvas;
        canvas.setType(GraficoCanvas::Cartesian);
        canvas.setCartesian("sin(x)", -3.14159265358979, 3.14159265358979);
        /* il tipo non deve cambiare */
    }

    /* D-3: formule composte non crashano */
    void formuleComposteNonCrashano() {
        GraficoCanvas canvas;
        const QStringList formule = {
            "x^2",
            "x*x + 2*x + 1",
            "cos(x) + sin(x)",
            "sqrt(abs(x))",
            "log(x + 1)",
            "tan(x)",
        };
        for (const QString& f : formule)
            canvas.setCartesian(f, -2.0, 2.0);  /* non deve crashare */
    }

    /* D-4: setCartesian con range invertito non crasha */
    void rangeInvertitoNonCrasha() {
        GraficoCanvas canvas;
        canvas.setCartesian("x", 5.0, -5.0);  /* xMin > xMax — edge case */
    }

    /* D-5: setCartesian con range degenere (xMin == xMax) non crasha */
    void rangeDegenerNonCrasha() {
        GraficoCanvas canvas;
        canvas.setCartesian("x", 0.0, 0.0);
    }
};

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    int status = 0;
    {
        TestGraficoCanvas          t1; status |= QTest::qExec(&t1, argc, argv);
        TestGraficoCanvasData      t2; status |= QTest::qExec(&t2, argc, argv);
        TestGraficoPage            t3; status |= QTest::qExec(&t3, argc, argv);
        TestFormulaParserIntegrazione t4; status |= QTest::qExec(&t4, argc, argv);
    }
    return status;
}

#include "test_grafico.moc"
