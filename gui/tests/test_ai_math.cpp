/* ══════════════════════════════════════════════════════════════
   test_ai_math.cpp — normalizeItFormats() (D-24)
   ──────────────────────────────────────────────────────────────
   CAT-A  Virgola decimale (5 test)
   CAT-B  Liste non toccate (3 test)
   CAT-C  Punto migliaia (4 test)
   CAT-D  Guardia IPv4 (4 test)
   CAT-E  Date con mese in lettere (5 test)
   CAT-F  Nessuna regressione su testo senza numeri (2 test)

   Build:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_ai_math
     ./build_tests/test_ai_math
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QString>

/* normalizeItFormats() è definita in main_ai_math.cpp e dichiarata in
 * main_ai_p.h — header interno (commento: "Non fare mai #include
 * main_ai_p.h da file esterni"). Forward-declaration diretta della stessa
 * firma invece di includere l'header: linka contro il simbolo compilato
 * in test_shared_objs senza violare quella regola. */
QString normalizeItFormats(const QString& text);

/* ══════════════════════════════════════════════════════════════
   CAT-A — Virgola decimale (5 test)
   ══════════════════════════════════════════════════════════════ */
class TestDecimalComma : public QObject {
    Q_OBJECT
private slots:

    void simplePair() {
        QCOMPARE(normalizeItFormats("3,5"), QString("3.5"));
    }

    void arithmeticExpression() {
        QCOMPARE(normalizeItFormats("quanto fa 3,5 + 2,1"),
                  QString("quanto fa 3.5 + 2.1"));
    }

    void singleNumberInSentence() {
        QCOMPARE(normalizeItFormats("il totale è 12,75 euro"),
                  QString("il totale è 12.75 euro"));
    }

    void multipleIndependentPairs() {
        QCOMPARE(normalizeItFormats("prima 1,5 poi 2,5"),
                  QString("prima 1.5 poi 2.5"));
    }

    void noCommaUnchanged() {
        QCOMPARE(normalizeItFormats("quanto fa 3 + 2"), QString("quanto fa 3 + 2"));
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B — Liste separate da virgola NON toccate (3 test)
   Motivazione: "3,5,7,9,2" è una lista di numeri (vedi D-16, statistica
   su lista — non ancora implementata), non un decimale. L'euristica deve
   escludere questo caso per non romperlo in futuro.
   ══════════════════════════════════════════════════════════════ */
class TestListNotComma : public QObject {
    Q_OBJECT
private slots:

    void fiveNumberList() {
        QCOMPARE(normalizeItFormats("media di 3,5,7,9,2"),
                  QString("media di 3,5,7,9,2"));
    }

    void threeNumberList() {
        QCOMPARE(normalizeItFormats("3,5,7"), QString("3,5,7"));
    }

    void twoNumberListStillAmbiguousButIsolatedPairConverts() {
        /* Un pair isolato (senza virgole adiacenti) è trattato come
         * decimale — comportamento intenzionale, vedi commento D-24. */
        QCOMPARE(normalizeItFormats("valore 7,2"), QString("valore 7.2"));
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-C — Punto migliaia (4 test)
   ══════════════════════════════════════════════════════════════ */
class TestThousandsSeparator : public QObject {
    Q_OBJECT
private slots:

    void simpleThousand() {
        QCOMPARE(normalizeItFormats("100.000 euro"), QString("100000 euro"));
    }

    void million() {
        QCOMPARE(normalizeItFormats("1.234.567"), QString("1234567"));
    }

    void thousandWithDecimal() {
        QCOMPARE(normalizeItFormats("1.234.567,89"), QString("1234567.89"));
    }

    void smallNumberUnaffected() {
        /* "12.5" non ha gruppi da 3 cifre dopo il punto — non è formato
         * migliaia (sarebbe semmai un decimale con punto invece di virgola,
         * caso non coperto qui: fuori scope D-24, resta invariato). */
        QCOMPARE(normalizeItFormats("12.5"), QString("12.5"));
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-D — Guardia IPv4: mai mutilare un indirizzo IP LAN (4 test)
   ══════════════════════════════════════════════════════════════ */
class TestIpv4Guard : public QObject {
    Q_OBJECT
private slots:

    void cameraIpUnchanged() {
        /* IP reale della telecamera WIBY (memoria progetto) */
        QCOMPARE(normalizeItFormats("la telecamera è su 192.168.1.222"),
                  QString("la telecamera è su 192.168.1.222"));
    }

    void allThreeDigitOctetsUnchanged() {
        /* Caso più a rischio: tutti gli ottetti da 3 cifre — indistinguibile
         * da un numero a migliaia se non protetto esplicitamente */
        QCOMPARE(normalizeItFormats("connetti a 192.168.100.200"),
                  QString("connetti a 192.168.100.200"));
    }

    void ipWithPortUnchanged() {
        QCOMPARE(normalizeItFormats("192.168.1.100:8080"),
                  QString("192.168.1.100:8080"));
    }

    void ipAndThousandsCoexist() {
        QCOMPARE(normalizeItFormats("prezzo 100.000, ip 192.168.1.222"),
                  QString("prezzo 100000, ip 192.168.1.222"));
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-E — Date con mese in lettere → numerico dd/mm/yyyy (5 test)
   ══════════════════════════════════════════════════════════════ */
class TestMonthNameDates : public QObject {
    Q_OBJECT
private slots:

    void birthDateFullYear() {
        QCOMPARE(normalizeItFormats("nato il 15 marzo 1990"),
                  QString("nato il 15/03/1990"));
    }

    void twoDigitYear() {
        QCOMPARE(normalizeItFormats("il 3 gennaio 90"),
                  QString("il 03/01/90"));
    }

    void december() {
        QCOMPARE(normalizeItFormats("25 dicembre 2024"),
                  QString("25/12/2024"));
    }

    void invalidDayLeftIntact() {
        /* giorno 40 non esiste — la guardia non deve inventare una data */
        QCOMPARE(normalizeItFormats("40 marzo 1990"), QString("40 marzo 1990"));
    }

    void caseInsensitiveMonth() {
        QCOMPARE(normalizeItFormats("1 Marzo 2000"), QString("01/03/2000"));
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-F — Nessuna regressione su testo senza numeri (2 test)
   ══════════════════════════════════════════════════════════════ */
class TestNoRegression : public QObject {
    Q_OBJECT
private slots:

    void plainSentenceUnchanged() {
        QCOMPARE(normalizeItFormats("ciao, come stai oggi?"),
                  QString("ciao, come stai oggi?"));
    }

    void emptyStringUnchanged() {
        QCOMPARE(normalizeItFormats(""), QString(""));
    }
};

int main(int argc, char** argv)
{
    int status = 0;
    { TestDecimalComma       t; status |= QTest::qExec(&t, argc, argv); }
    { TestListNotComma       t; status |= QTest::qExec(&t, argc, argv); }
    { TestThousandsSeparator t; status |= QTest::qExec(&t, argc, argv); }
    { TestIpv4Guard          t; status |= QTest::qExec(&t, argc, argv); }
    { TestMonthNameDates     t; status |= QTest::qExec(&t, argc, argv); }
    { TestNoRegression       t; status |= QTest::qExec(&t, argc, argv); }
    return status;
}

#include "test_ai_math.moc"
