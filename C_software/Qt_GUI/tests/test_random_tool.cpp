/* ══════════════════════════════════════════════════════════════
   test_random_tool.cpp — Unit test per _inject_random (agenti_page_tools.cpp)

   Build:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc)
     ./build_tests/test_random_tool

   Casi coperti:
     1.  Task senza keyword random → passthrough invariato
     2.  "numeri casuali" → header iniettato, task originale preservato
     3.  Conteggio esplicito "10 numeri casuali" → esattamente 10 valori
     4.  Range "tra 1 e 50" → tutti i valori ∈ [1, 50]
     5.  Decimali "numeri decimali casuali" → formato float con virgola
     6.  Grafico + random → default 50 punti (densità visiva)
     7.  Header iniettato prima del task originale
     8.  Due chiamate → sequenze diverse (non deterministico)
     9.  Range negativo "tra -100 e -1" → valori negativi
     10. Conteggio limite alto "500 numeri" → esattamente 500 valori
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QRegularExpression>

/* Dichiarazione esterna: definita in agenti_page_tools.cpp */
extern QString _inject_random(const QString& task);

/* ── Helper: estrae i numeri dal blocco iniettato ── */
static QStringList extractNumbers(const QString& injected) {
    /* Formato: [DATI RANDOM ... (N tipo in [min, max]):\nV1, V2, ...]\n\n
     * Usiamo [^\n]* per matchare la prima riga dell'header (può contenere
     * ] da "[1, 100]" → non possiamo usare [^\]]*). */
    static const QRegularExpression reBlock(
        R"(\[DATI RANDOM[^\n]*:\n([^\]]+)\])");
    auto m = reBlock.match(injected);
    if (!m.hasMatch()) return {};
    return m.captured(1).split(", ", Qt::SkipEmptyParts);
}

class TestRandomTool : public QObject {
    Q_OBJECT

private slots:

    /* ── 1. Task non-random → invariato ── */
    void noKeyword_passthrough() {
        const QString task = "Spiega il teorema di Pitagora";
        const QString result = _inject_random(task);
        QCOMPARE(result, task);
    }

    /* ── 2. "numeri casuali" → header iniettato ── */
    void keyword_injectsHeader() {
        const QString result = _inject_random("fammi dei numeri casuali");
        QVERIFY(result.contains("[DATI RANDOM"));
        QVERIFY(result.contains("mt19937"));
    }

    /* ── 3. Conteggio esplicito ── */
    void explicitCount_10() {
        const QString result = _inject_random("dammi 10 numeri casuali tra 1 e 100");
        const QStringList nums = extractNumbers(result);
        QCOMPARE(nums.size(), 10);
    }

    /* ── 4. Range "tra 1 e 50" → tutti i valori nel range ── */
    void range_inBounds() {
        const QString result = _inject_random("genera 30 numeri casuali tra 1 e 50");
        const QStringList nums = extractNumbers(result);
        QVERIFY(!nums.isEmpty());
        for (const QString& s : nums) {
            const int v = s.trimmed().toInt();
            QVERIFY2(v >= 1 && v <= 50,
                     qPrintable(QString("Valore fuori range: %1").arg(v)));
        }
    }

    /* ── 5. "decimali" → output float con punto decimale ── */
    void floatMode_hasDecimalPoint() {
        const QString result = _inject_random(
            "genera 5 numeri decimali casuali tra 0 e 1");
        const QStringList nums = extractNumbers(result);
        QVERIFY(!nums.isEmpty());
        /* Almeno un valore deve contenere il punto decimale */
        bool foundDot = false;
        for (const QString& s : nums)
            if (s.contains('.')) { foundDot = true; break; }
        QVERIFY(foundDot);
    }

    /* ── 6. "grafico" + "random" → default 50 punti ── */
    void chartMode_default50() {
        const QString result = _inject_random(
            "fai un grafico con numeri random");
        const QStringList nums = extractNumbers(result);
        /* Con count=50 e range default [1,100] deve avere 50 valori */
        QCOMPARE(nums.size(), 50);
    }

    /* ── 7. Header precede il task originale ── */
    void headerBeforeTask() {
        const QString task = "usa questi numeri per un istogramma: numeri casuali";
        const QString result = _inject_random(task);
        QVERIFY(result.startsWith("[DATI RANDOM"));
        /* Il task originale compare dopo il blocco */
        QVERIFY(result.contains(task));
        QVERIFY(result.indexOf("[DATI RANDOM") < result.indexOf(task));
    }

    /* ── 8. Due chiamate → sequenze diverse ── */
    void twoCallsDiffer() {
        const QString r1 = _inject_random("dammi 20 numeri casuali tra 1 e 1000000");
        const QString r2 = _inject_random("dammi 20 numeri casuali tra 1 e 1000000");
        const QStringList n1 = extractNumbers(r1);
        const QStringList n2 = extractNumbers(r2);
        QVERIFY(!n1.isEmpty() && !n2.isEmpty());
        /* Statisticamente impossibile che due sequenze da 20 su [1,10^6] siano uguali */
        QVERIFY(n1.join(",") != n2.join(","));
    }

    /* ── 9. Range negativo ── */
    void negativeRange() {
        const QString result = _inject_random(
            "genera 10 numeri casuali tra -100 e -1");
        const QStringList nums = extractNumbers(result);
        QVERIFY(!nums.isEmpty());
        for (const QString& s : nums) {
            const int v = s.trimmed().toInt();
            QVERIFY2(v >= -100 && v <= -1,
                     qPrintable(QString("Valore fuori range negativo: %1").arg(v)));
        }
    }

    /* ── 10. Conteggio alto → 500 valori ── */
    void highCount_500() {
        const QString result = _inject_random("genera 500 numeri casuali");
        const QStringList nums = extractNumbers(result);
        QCOMPARE(nums.size(), 500);
    }
};

QTEST_MAIN(TestRandomTool)
#include "test_random_tool.moc"
