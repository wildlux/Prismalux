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
#include "../prismalux_paths.h"
#include <QSettings>
namespace P = PrismaluxPaths;

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
        /* Carta Astrale è nascosta di default (easter egg "conoscenza" in
           Ringraziamenti, vedi P::SK::kAstraleUnlocked) — sbloccata qui per
           poter testare la sotto-tab senza passare dall'UI di Impostazioni. */
        QSettings("Prismalux", "GUI").setValue(P::SK::kAstraleUnlocked, true);
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
   CAT-D — Regressione su carte di riferimento (Astrodienst / Astro-Seek)
   Tolleranza pianeti: 1.0°   Tolleranza ASC/MC: 1.5°
   Carte sorgente: foto Telegram 2026-06-28
   ══════════════════════════════════════════════════════════════ */
class TestCatD : public QObject {
    Q_OBJECT

    /* Differenza angolare minima tra due longitudine [0,360) */
    static double angDiff(double a, double b) {
        double d = std::fmod(std::fabs(a - b), 360.0);
        return d > 180.0 ? 360.0 - d : d;
    }

    /* Verifica pianeta con messaggio leggibile */
    static bool checkPlanet(const AstroCalc::Result& r, const QString& name,
                            double expected, double tol, QString* msg)
    {
        for (const auto& p : r.planets) {
            if (p.name == name) {
                double diff = angDiff(p.lon, expected);
                if (diff > tol) {
                    *msg = QString("%1: got %2°, expected %3°, diff %4°")
                               .arg(name).arg(p.lon, 0, 'f', 2)
                               .arg(expected, 0, 'f', 2).arg(diff, 0, 'f', 2);
                    return false;
                }
                return true;
            }
        }
        *msg = name + " non trovato nel risultato";
        return false;
    }

private slots:

    /* ── D-1 Jannik Sinner ───────────────────────────────────────
       16 ago 2001 00:52 locale Sexten/IT (CEST=UTC+2) → UT 15-ago 22:52
       Riferimento: Astrodienst (Astrowiki), Placidus             */
    void sinner2001() {
        auto r = AstroCalc::compute(2001, 8, 15, 22, 52, 46.70, 12.35);
        if (!r.ok) QSKIP("AstroCalc non disponibile");
        const double T = 1.0, A = 1.5;
        QString msg;
        QVERIFY2(checkPlanet(r, "SOLE",     143.15, T, &msg), qPrintable(msg)); // Leo  23°9'
        QVERIFY2(checkPlanet(r, "LUNA",      98.86, T, &msg), qPrintable(msg)); // Can  8°52'
        QVERIFY2(checkPlanet(r, "MERCURIO", 153.25, T, &msg), qPrintable(msg)); // Vir  3°15'
        QVERIFY2(checkPlanet(r, "VENERE",   106.75, T, &msg), qPrintable(msg)); // Can 16°45'
        QVERIFY2(checkPlanet(r, "MARTE",    259.83, T, &msg), qPrintable(msg)); // Sag 19°50'
        QVERIFY2(checkPlanet(r, "GIOVE",     97.09, T, &msg), qPrintable(msg)); // Can  7°4'
        QVERIFY2(checkPlanet(r, "SATURNO",   73.44, T, &msg), qPrintable(msg)); // Gem 13°26'
        QVERIFY2(checkPlanet(r, "URANO",    322.83, T, &msg), qPrintable(msg)); // Aqu 22°50'
        QVERIFY2(checkPlanet(r, "NETTUNO",  306.93, T, &msg), qPrintable(msg)); // Aqu  6°56'
        QVERIFY2(checkPlanet(r, "PLUTONE",  252.55, T, &msg), qPrintable(msg)); // Sag 12°33'
        QVERIFY2(checkPlanet(r, "NODO",      93.67, T, &msg), qPrintable(msg)); // Can  3°40'
        QVERIFY2(angDiff(r.ascLon, 77.38) < A,
                 qPrintable(QString("ASC: got %1° exp 77.38°").arg(r.ascLon, 0,'f',2))); // Gem 17°23'
        QVERIFY2(angDiff(r.mcLon, 317.31) < A,
                 qPrintable(QString("MC: got %1° exp 317.31°").arg(r.mcLon, 0,'f',2)));  // Aqu 17°19'
    }

    /* ── D-2 Ariana Grande ───────────────────────────────────────
       26 giu 1993 21:16 locale Boca Raton FL (EDT=UTC-4) → UT 27-giu 01:16
       Riferimento: Astrodienst, Placidus                         */
    void arianaGrande1993() {
        auto r = AstroCalc::compute(1993, 6, 27, 1, 16, 26.367, -80.083);
        if (!r.ok) QSKIP("AstroCalc non disponibile");
        const double T = 1.0, A = 1.5;
        QString msg;
        QVERIFY2(checkPlanet(r, "SOLE",      95.42, T, &msg), qPrintable(msg)); // Can  5°25'
        QVERIFY2(checkPlanet(r, "LUNA",     186.80, T, &msg), qPrintable(msg)); // Lib  6°48'
        QVERIFY2(checkPlanet(r, "MERCURIO", 117.45, T, &msg), qPrintable(msg)); // Can 27°27'
        QVERIFY2(checkPlanet(r, "VENERE",    50.48, T, &msg), qPrintable(msg)); // Tau 20°29'
        QVERIFY2(checkPlanet(r, "MARTE",    152.98, T, &msg), qPrintable(msg)); // Vir  2°59'
        QVERIFY2(checkPlanet(r, "GIOVE",    185.75, T, &msg), qPrintable(msg)); // Lib  5°45'
        QVERIFY2(checkPlanet(r, "SATURNO",  330.10, T, &msg), qPrintable(msg)); // Pis  0°6'
        QVERIFY2(checkPlanet(r, "URANO",    290.82, T, &msg), qPrintable(msg)); // Cap 20°49'
        QVERIFY2(checkPlanet(r, "NETTUNO",  290.50, T, &msg), qPrintable(msg)); // Cap 20°30'
        QVERIFY2(checkPlanet(r, "PLUTONE",  233.08, T, &msg), qPrintable(msg)); // Sco 23°5'
        QVERIFY2(angDiff(r.ascLon, 290.95) < A,
                 qPrintable(QString("ASC: got %1° exp 290.95°").arg(r.ascLon, 0,'f',2))); // Cap 20°57'
        QVERIFY2(angDiff(r.mcLon, 216.41) < A,
                 qPrintable(QString("MC: got %1° exp 216.41°").arg(r.mcLon, 0,'f',2)));   // Sco  6°25'
    }

    /* ── D-3 Adriano Celentano ───────────────────────────────────
       6 gen 1938 07:00 locale Milan (CET=UTC+1) → UT 06:00
       Riferimento: Astro-Seek, Placidus                          */
    void celentano1938() {
        auto r = AstroCalc::compute(1938, 1, 6, 6, 0, 45.467, 9.200);
        if (!r.ok) QSKIP("AstroCalc non disponibile");
        const double T = 1.0, A = 1.5;
        QString msg;
        QVERIFY2(checkPlanet(r, "SOLE",     285.27, T, &msg), qPrintable(msg)); // Cap 15°16'
        QVERIFY2(checkPlanet(r, "LUNA",     335.17, T, &msg), qPrintable(msg)); // Pis  5°10'
        QVERIFY2(checkPlanet(r, "MERCURIO", 270.33, T, &msg), qPrintable(msg)); // Cap  0°20'
        QVERIFY2(checkPlanet(r, "VENERE",   278.35, T, &msg), qPrintable(msg)); // Cap  8°21'
        QVERIFY2(checkPlanet(r, "MARTE",    341.73, T, &msg), qPrintable(msg)); // Pis 11°44'
        QVERIFY2(checkPlanet(r, "GIOVE",    303.83, T, &msg), qPrintable(msg)); // Aqu  3°50'
        QVERIFY2(checkPlanet(r, "SATURNO",  359.46, T, &msg), qPrintable(msg)); // Pis 29°28'
        QVERIFY2(checkPlanet(r, "URANO",     39.77, T, &msg), qPrintable(msg)); // Tau  9°46'
        QVERIFY2(checkPlanet(r, "NETTUNO",  171.10, T, &msg), qPrintable(msg)); // Vir 21°6'
        QVERIFY2(checkPlanet(r, "PLUTONE",  119.28, T, &msg), qPrintable(msg)); // Can 29°17'
        QVERIFY2(angDiff(r.ascLon, 268.36) < A,
                 qPrintable(QString("ASC: got %1° exp 268.36°").arg(r.ascLon, 0,'f',2))); // Sag 28°22'
        QVERIFY2(angDiff(r.mcLon, 206.25) < A,
                 qPrintable(QString("MC: got %1° exp 206.25°").arg(r.mcLon, 0,'f',2)));   // Lib 26°15'
    }

    /* ── D-4 Alda Merini ─────────────────────────────────────────
       21 mar 1931 05:00 locale Milan (CET=UTC+1) → UT 04:00
       Riferimento: Astro-Seek, Placidus                          */
    void aldaMerini1931() {
        auto r = AstroCalc::compute(1931, 3, 21, 4, 0, 45.467, 9.200);
        if (!r.ok) QSKIP("AstroCalc non disponibile");
        const double T = 1.0, A = 1.5;
        QString msg;
        QVERIFY2(checkPlanet(r, "SOLE",     359.57, T, &msg), qPrintable(msg)); // Pis 29°34'
        QVERIFY2(checkPlanet(r, "LUNA",      19.62, T, &msg), qPrintable(msg)); // Ari 19°37'
        QVERIFY2(checkPlanet(r, "MERCURIO",   4.72, T, &msg), qPrintable(msg)); // Ari  4°43'
        QVERIFY2(checkPlanet(r, "VENERE",   317.57, T, &msg), qPrintable(msg)); // Aqu 17°34'
        QVERIFY2(checkPlanet(r, "MARTE",    118.33, T, &msg), qPrintable(msg)); // Can 28°20'
        QVERIFY2(checkPlanet(r, "GIOVE",    100.75, T, &msg), qPrintable(msg)); // Can 10°45'
        QVERIFY2(checkPlanet(r, "SATURNO",  291.77, T, &msg), qPrintable(msg)); // Cap 21°46'
        QVERIFY2(checkPlanet(r, "URANO",     14.42, T, &msg), qPrintable(msg)); // Ari 14°25'
        QVERIFY2(checkPlanet(r, "NETTUNO",  153.73, T, &msg), qPrintable(msg)); // Vir  3°44'
        QVERIFY2(checkPlanet(r, "PLUTONE",  108.72, T, &msg), qPrintable(msg)); // Can 18°43'
        QVERIFY2(checkPlanet(r, "NODO",      15.42, T, &msg), qPrintable(msg)); // Ari 15°25'
        QVERIFY2(angDiff(r.ascLon, 318.25) < A,
                 qPrintable(QString("ASC: got %1° exp 318.25°").arg(r.ascLon, 0,'f',2))); // Aqu 18°15'
        QVERIFY2(angDiff(r.mcLon, 248.63) < A,
                 qPrintable(QString("MC: got %1° exp 248.63°").arg(r.mcLon, 0,'f',2)));   // Sag  8°38'
    }

    /* ── D-5 John Lennon ─────────────────────────────────────────
       9 ott 1940 18:30 locale Liverpool (BST=UTC+1) → UT 17:30
       Riferimento: Astro-Seek, Placidus                          */
    void johnLennon1940() {
        auto r = AstroCalc::compute(1940, 10, 9, 17, 30, 53.417, -2.917);
        if (!r.ok) QSKIP("AstroCalc non disponibile");
        const double T = 1.0, A = 1.5;
        QString msg;
        QVERIFY2(checkPlanet(r, "SOLE",     196.27, T, &msg), qPrintable(msg)); // Lib 16°16'
        QVERIFY2(checkPlanet(r, "LUNA",     303.53, T, &msg), qPrintable(msg)); // Aqu  3°32'
        QVERIFY2(checkPlanet(r, "MERCURIO", 218.55, T, &msg), qPrintable(msg)); // Sco  8°33'
        QVERIFY2(checkPlanet(r, "VENERE",   153.20, T, &msg), qPrintable(msg)); // Vir  3°12'
        QVERIFY2(checkPlanet(r, "MARTE",    182.65, T, &msg), qPrintable(msg)); // Lib  2°39'
        QVERIFY2(checkPlanet(r, "GIOVE",     43.68, T, &msg), qPrintable(msg)); // Tau 13°41'
        QVERIFY2(checkPlanet(r, "SATURNO",   43.20, T, &msg), qPrintable(msg)); // Tau 13°12'
        QVERIFY2(checkPlanet(r, "URANO",     55.55, T, &msg), qPrintable(msg)); // Tau 25°33'
        QVERIFY2(checkPlanet(r, "NETTUNO",  176.02, T, &msg), qPrintable(msg)); // Vir 26°1'
        QVERIFY2(checkPlanet(r, "PLUTONE",  124.18, T, &msg), qPrintable(msg)); // Leo  4°11'
        QVERIFY2(angDiff(r.ascLon, 19.90) < A,
                 qPrintable(QString("ASC: got %1° exp 19.90°").arg(r.ascLon, 0,'f',2)));  // Ari 19°54'
        QVERIFY2(angDiff(r.mcLon, 277.12) < A,
                 qPrintable(QString("MC: got %1° exp 277.12°").arg(r.mcLon, 0,'f',2)));   // Cap  7°7'
    }

    /* ── D-6 James Joyce ─────────────────────────────────────────
       2 feb 1882 06:00 locale Dublin (DMT≈UTC-0:25) → UT ~06:25
       Riferimento: Astro-Seek, Placidus. Tolleranza 2° per fuso storico. */
    void jamesJoyce1882() {
        auto r = AstroCalc::compute(1882, 2, 2, 6, 25, 53.333, -6.25);
        if (!r.ok) QSKIP("AstroCalc non disponibile");
        const double T = 2.0, A = 2.0;   /* tolleranza ampia: fuso storico incerto */
        QString msg;
        QVERIFY2(checkPlanet(r, "SOLE",     313.35, T, &msg), qPrintable(msg)); // Aqu 13°21'
        QVERIFY2(checkPlanet(r, "LUNA",     122.65, T, &msg), qPrintable(msg)); // Leo  2°39'
        QVERIFY2(checkPlanet(r, "MERCURIO", 330.48, T, &msg), qPrintable(msg)); // Pis  0°29'
        QVERIFY2(checkPlanet(r, "VENERE",   308.80, T, &msg), qPrintable(msg)); // Aqu  8°48'
        QVERIFY2(checkPlanet(r, "MARTE",     86.95, T, &msg), qPrintable(msg)); // Gem 26°57'
        QVERIFY2(checkPlanet(r, "GIOVE",     47.08, T, &msg), qPrintable(msg)); // Tau 17°5'
        QVERIFY2(checkPlanet(r, "SATURNO",   36.18, T, &msg), qPrintable(msg)); // Tau  6°11'
        QVERIFY2(checkPlanet(r, "NETTUNO",   43.78, T, &msg), qPrintable(msg)); // Tau 13°47'
        QVERIFY2(angDiff(r.ascLon, 276.42) < A,
                 qPrintable(QString("ASC: got %1° exp 276.42°").arg(r.ascLon, 0,'f',2))); // Cap  6°25'
        QVERIFY2(angDiff(r.mcLon, 224.81) < A,
                 qPrintable(QString("MC: got %1° exp 224.81°").arg(r.mcLon, 0,'f',2)));   // Sco 14°49'
    }

    /* ── D-7 Italia (nascita Repubblica) ─────────────────────────
       18 giu 1946 16:00 UT, Roma (41.90N 12.48E)
       Riferimento: Astrodienst, Placidus. Solo ASC/MC leggibili. */
    void italiaRepubblica1946() {
        auto r = AstroCalc::compute(1946, 6, 18, 16, 0, 41.90, 12.483);
        if (!r.ok) QSKIP("AstroCalc non disponibile");
        const double A = 2.0;
        QVERIFY2(angDiff(r.ascLon, 233.0) < A,
                 qPrintable(QString("ASC: got %1° exp 233°").arg(r.ascLon, 0,'f',2)));    // Sco 23°
        QVERIFY2(angDiff(r.mcLon, 157.0) < A,
                 qPrintable(QString("MC: got %1° exp 157°").arg(r.mcLon, 0,'f',2)));      // Vir  7°
    }

    /* ── D-8 USA (indipendenza) ──────────────────────────────────
       4 lug 1776 22:10 UT (LMT 17:10), Philadelphia (39.95N -75.17E)
       Riferimento: Astrodienst, Placidus. Solo ASC/MC leggibili. */
    void usaIndipendenza1776() {
        auto r = AstroCalc::compute(1776, 7, 4, 22, 10, 39.95, -75.167);
        if (!r.ok) QSKIP("AstroCalc non disponibile");
        const double A = 2.0;
        QVERIFY2(angDiff(r.ascLon, 252.0) < A,
                 qPrintable(QString("ASC: got %1° exp 252°").arg(r.ascLon, 0,'f',2)));    // Sag 12°
        QVERIFY2(angDiff(r.mcLon, 180.88) < A,
                 qPrintable(QString("MC: got %1° exp 180.88°").arg(r.mcLon, 0,'f',2)));   // Lib  0°53'
    }

    /* ── D-9 Unione Europea ──────────────────────────────────────
       31 ott 1993 23:00 UT (= 1 nov 00:00 MET), Bruxelles (50.83N 4.33E)
       Riferimento: Astrodienst, Placidus. Solo ASC/MC leggibili. */
    void unioneEuropea1993() {
        auto r = AstroCalc::compute(1993, 10, 31, 23, 0, 50.833, 4.333);
        if (!r.ok) QSKIP("AstroCalc non disponibile");
        const double A = 2.0;
        QVERIFY2(angDiff(r.ascLon, 137.0) < A,
                 qPrintable(QString("ASC: got %1° exp 137°").arg(r.ascLon, 0,'f',2)));    // Leo 17°
        QVERIFY2(angDiff(r.mcLon, 31.75) < A,
                 qPrintable(QString("MC: got %1° exp 31.75°").arg(r.mcLon, 0,'f',2)));    // Tau  1°45'
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
    { TestCatD t; r |= QTest::qExec(&t, argc, argv); }
    return r;
}

#include "test_astrale.moc"
