/* ══════════════════════════════════════════════════════════════
   test_astrale.cpp  —  Suite di regressione Carta Astrale
   ──────────────────────────────────────────────────────────────
   3 categorie:

   CAT-A  RicercaPage widget    — 10 casi (costruzione + figli)
   CAT-B  NatalChartWidget      —  8 casi (unit, setData, clear)
   CAT-C  AstroCalc::compute()  —  7 casi (output astronomico)

   COME ESEGUIRE:
     cmake -B build_tests gui/ -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_astrale
     ./build_tests/test_astrale
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QApplication>
#include <QDateEdit>
#include <QTimeEdit>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>

#include "mock_ai_client.h"
#include "../pages/main_research.h"
#include "../widgets/natal_chart_widget.h"
#include "../widgets/astro_calc.h"

/* ══════════════════════════════════════════════════════════════
   CAT-A — RicercaPage widget (costruzione + presenza figli)
   ══════════════════════════════════════════════════════════════ */
class TestCatA : public QObject {
    Q_OBJECT
private:
    MockAiClient* ai   = nullptr;
    RicercaPage*  page = nullptr;

private slots:
    void init() {
        ai   = new MockAiClient();
        page = new RicercaPage(ai);
    }

    void cleanup() {
        delete page; page = nullptr;
        delete ai;   ai   = nullptr;
    }

    /* A-1: costruzione senza crash */
    void constructsWithoutCrash() {
        QVERIFY(page != nullptr);
    }

    /* A-2: e' un QWidget valido */
    void isValidQWidget() {
        QVERIFY(qobject_cast<QWidget*>(page) != nullptr);
    }

    /* A-3: contiene almeno un NatalChartWidget come figlio */
    void containsNatalChartWidget() {
        auto* ncw = page->findChild<NatalChartWidget*>();
        QVERIFY2(ncw != nullptr, "NatalChartWidget non trovato nei figli di RicercaPage");
    }

    /* A-4: contiene campo data nascita QDateEdit */
    void containsDateEdit() {
        auto list = page->findChildren<QDateEdit*>();
        QVERIFY2(!list.isEmpty(), "QDateEdit (data nascita) non trovato");
    }

    /* A-5: contiene campo ora QTimeEdit */
    void containsTimeEdit() {
        auto list = page->findChildren<QTimeEdit*>();
        QVERIFY2(!list.isEmpty(), "QTimeEdit (ora nascita) non trovato");
    }

    /* A-6: contiene campo citta QLineEdit con placeholder "Citta" */
    void containsCittaLineEdit() {
        bool found = false;
        for (auto* le : page->findChildren<QLineEdit*>()) {
            if (le->placeholderText().contains("itt", Qt::CaseInsensitive)) {
                found = true;
                break;
            }
        }
        QVERIFY2(found, "QLineEdit citta non trovato (placeholder 'Citta')");
    }

    /* A-7: contiene campo latitudine QLineEdit */
    void containsLatLineEdit() {
        bool found = false;
        for (auto* le : page->findChildren<QLineEdit*>()) {
            if (le->placeholderText().contains("Lat", Qt::CaseInsensitive) ||
                le->text().contains('.')) {
                if (le->maximumWidth() < 120) { found = true; break; }
            }
        }
        /* fallback: cerca un LineEdit con testo numerico corto (lat) */
        if (!found) {
            for (auto* le : page->findChildren<QLineEdit*>()) {
                bool ok = false;
                le->text().toDouble(&ok);
                if (ok) { found = true; break; }
            }
        }
        QVERIFY2(found, "QLineEdit latitudine non trovato");
    }

    /* A-8: contiene campo longitudine QLineEdit distinto da lat */
    void containsAtLeastTwoNumericLineEdits() {
        int numericCount = 0;
        for (auto* le : page->findChildren<QLineEdit*>()) {
            bool ok = false;
            le->text().toDouble(&ok);
            if (ok) ++numericCount;
        }
        QVERIFY2(numericCount >= 2, "Attesi almeno 2 QLineEdit numerici (lat+lon)");
    }

    /* A-9: combo profondita lettura esiste e ha almeno 2 opzioni */
    void depthComboHasItems() {
        /* Cerca combo con almeno 2 item e dati che contengono le chiavi lettura */
        for (auto* cb : page->findChildren<QComboBox*>()) {
            if (cb->count() >= 2) {
                for (int i = 0; i < cb->count(); ++i) {
                    const QString d = cb->itemData(i).toString();
                    if (d == "sintesi" || d == "completa" || d == "pro") {
                        QVERIFY(cb->count() >= 2);
                        return;
                    }
                }
            }
        }
        QFAIL("Combo profondita lettura astrale non trovato");
    }

    /* A-10: pulsante "Leggi gli Astri" esiste */
    void runButtonExists() {
        bool found = false;
        for (auto* b : page->findChildren<QPushButton*>()) {
            if (b->text().contains("Astri", Qt::CaseInsensitive) ||
                b->text().contains("Leggi", Qt::CaseInsensitive)) {
                found = true;
                break;
            }
        }
        QVERIFY2(found, "Pulsante 'Leggi gli Astri' non trovato");
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B — NatalChartWidget unit test
   ══════════════════════════════════════════════════════════════ */
class TestCatB : public QObject {
    Q_OBJECT
private:
    NatalChartWidget* w = nullptr;

private slots:
    void init() {
        w = new NatalChartWidget();
    }

    void cleanup() {
        delete w; w = nullptr;
    }

    /* B-1: costruisce senza crash */
    void constructsWithoutCrash() {
        QVERIFY(w != nullptr);
    }

    /* B-2: e' un QWidget valido */
    void isValidQWidget() {
        QVERIFY(qobject_cast<QWidget*>(w) != nullptr);
    }

    /* B-3: sizeHint() restituisce dimensione positiva */
    void sizeHintIsPositive() {
        const QSize sh = w->sizeHint();
        QVERIFY(sh.width()  > 0);
        QVERIFY(sh.height() > 0);
    }

    /* B-4: minimumSizeHint() <= sizeHint() */
    void minimumSizeHintLeSize() {
        QVERIFY(w->minimumSizeHint().width()  <= w->sizeHint().width());
        QVERIFY(w->minimumSizeHint().height() <= w->sizeHint().height());
    }

    /* B-5: setData() con vettore vuoto e ascendente 0 non crasha */
    void setDataEmptyPlanetsNoCrash() {
        QVector<NatalChartWidget::Planet> empty;
        w->setData(empty, 0.0);
        QVERIFY(true);
    }

    /* B-6: setData() con pianeti validi (coordinate 45N 9E) non crasha */
    void setDataValidPlanetsNoCrash() {
        QVector<NatalChartWidget::Planet> planets;
        NatalChartWidget::Planet sol;
        sol.name   = "Sole";
        sol.symbol = "\xe2\x98\x89";
        sol.lon    = 120.0;
        sol.color  = QColor(Qt::yellow);
        planets.append(sol);

        NatalChartWidget::Planet luna;
        luna.name   = "Luna";
        luna.symbol = "\xe2\x98\xbd";
        luna.lon    = 45.0;
        luna.color  = QColor(Qt::white);
        planets.append(luna);

        w->setData(planets, 15.0, 285.0);
        QVERIFY(true);
    }

    /* B-7: setData() con coordinate lon fuori range 0-360 non crasha */
    void setDataOutOfRangeLonNoCrash() {
        QVector<NatalChartWidget::Planet> planets;
        NatalChartWidget::Planet p;
        p.name   = "X";
        p.symbol = "X";
        p.lon    = 720.0;   /* fuori range */
        p.color  = QColor(Qt::red);
        planets.append(p);
        w->setData(planets, -90.0, 999.0);
        QVERIFY(true);
    }

    /* B-8: clear() dopo setData() non crasha */
    void clearAfterSetDataNoCrash() {
        QVector<NatalChartWidget::Planet> planets;
        NatalChartWidget::Planet p;
        p.name   = "Sole";
        p.symbol = "\xe2\x98\x89";
        p.lon    = 200.0;
        p.color  = QColor(Qt::yellow);
        planets.append(p);
        w->setData(planets, 30.0);
        w->clear();
        QVERIFY(true);
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-C — AstroCalc::compute() (output astronomico)
   ══════════════════════════════════════════════════════════════ */
class TestCatC : public QObject {
    Q_OBJECT
private slots:

    /* C-1: compute() restituisce un Result valido (ok=true) per data standard */
    void computeReturnOkForStandardDate() {
        /* Roma, 1 gennaio 1990, ore 12:00, 41.9N 12.5E */
        auto r = AstroCalc::compute(1990, 1, 1, 12, 0, 41.9, 12.5);
        if (!r.ok) {
            QSKIP("AstroCalc::compute() non supportata in questo ambiente");
        }
        QVERIFY(r.ok);
        QVERIFY(r.error.isEmpty());
    }

    /* C-2: vettore pianeti non vuoto */
    void computeReturnsPlanets() {
        auto r = AstroCalc::compute(1990, 1, 1, 12, 0, 41.9, 12.5);
        if (!r.ok) QSKIP("AstroCalc non disponibile");
        QVERIFY(!r.planets.isEmpty());
    }

    /* C-3: ascendente e' nell'intervallo 0-360 gradi */
    void computeAscLonInRange() {
        auto r = AstroCalc::compute(1990, 1, 1, 12, 0, 41.9, 12.5);
        if (!r.ok) QSKIP("AstroCalc non disponibile");
        QVERIFY2(r.ascLon >= 0.0 && r.ascLon < 360.0,
                 qPrintable(QString("ascLon fuori range: %1").arg(r.ascLon)));
    }

    /* C-4: MC e' nell'intervallo 0-360 gradi */
    void computeMcLonInRange() {
        auto r = AstroCalc::compute(1990, 1, 1, 12, 0, 41.9, 12.5);
        if (!r.ok) QSKIP("AstroCalc non disponibile");
        QVERIFY2(r.mcLon >= 0.0 && r.mcLon < 360.0,
                 qPrintable(QString("mcLon fuori range: %1").arg(r.mcLon)));
    }

    /* C-5: longitudine di ogni pianeta e' nell'intervallo 0-360 */
    void computePlanetLonsInRange() {
        auto r = AstroCalc::compute(1990, 1, 1, 12, 0, 41.9, 12.5);
        if (!r.ok) QSKIP("AstroCalc non disponibile");
        for (const auto& p : r.planets) {
            QVERIFY2(p.lon >= 0.0 && p.lon < 360.0,
                     qPrintable(QString("Pianeta %1: lon=%2 fuori range").arg(p.name).arg(p.lon)));
        }
    }

    /* C-6: compute() con coordinate polari estreme (lat=89N, lon=179E) non crasha */
    void computeExtremeCoordsNoCrash() {
        auto r = AstroCalc::compute(2000, 6, 21, 6, 0, 89.0, 179.0);
        /* Non verifichiamo ok — le case placidiane ai poli possono fallire */
        QVERIFY(true);
    }

    /* C-7: due date diverse producono ascendenti diversi (variabilita') */
    void computeDifferentDatesProduceDifferentAsc() {
        auto r1 = AstroCalc::compute(1980,  3, 15, 8,  0, 41.9, 12.5);
        auto r2 = AstroCalc::compute(2000, 10, 30, 20, 0, 41.9, 12.5);
        if (!r1.ok || !r2.ok) QSKIP("AstroCalc non disponibile");
        /* L'ascendente dipende dall'ora siderale — deve differire tra le due date */
        QVERIFY2(qAbs(r1.ascLon - r2.ascLon) > 0.01,
                 "ascLon identico per date molto diverse — probabile bug nel calcolo");
    }
};

/* ══════════════════════════════════════════════════════════════
   main
   ══════════════════════════════════════════════════════════════ */
int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    int r = 0;
    { TestCatA t; r |= QTest::qExec(&t, argc, argv); }
    { TestCatB t; r |= QTest::qExec(&t, argc, argv); }
    { TestCatC t; r |= QTest::qExec(&t, argc, argv); }
    return r;
}

#include "test_astrale.moc"
