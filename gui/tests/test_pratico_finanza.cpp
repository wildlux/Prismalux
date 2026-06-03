/* ══════════════════════════════════════════════════════════════
   test_pratico_finanza.cpp
   ──────────────────────────────────────────────────────────────
   Suite di test per PraticoCalcs (pratico_calcs.h).
   Funzioni pure header-only: nessun DB, nessuna rete.

   Categorie:
     A  — normalizzaComune: strip accenti, toLower, trim
     B  — cercaBelfiore: comuni IT, accenti, paesi esteri, alias
     C  — calcolaCodiceFiscale: algoritmo D.M. 23/12/1976

   Esecuzione:
     cmake -B build_tests gui/ -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
     cmake --build build_tests -j$(nproc) --target test_pratico_finanza
     ./build_tests/test_pratico_finanza
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QApplication>
#include <QDate>

#include "../pages/pratico_calcs.h"

namespace PC = PraticoCalcs;

/* ══════════════════════════════════════════════════════════════
   A — normalizzaComune
   ══════════════════════════════════════════════════════════════ */
class TestNormalizzaComune : public QObject {
    Q_OBJECT
private slots:

    /* A-1: stringa gia' normalizzata rimane uguale */
    void giaNomalizzata() {
        QCOMPARE(PC::normalizzaComune("roma"),    QString("roma"));
        QCOMPARE(PC::normalizzaComune("milano"),  QString("milano"));
        QCOMPARE(PC::normalizzaComune("torino"),  QString("torino"));
    }

    /* A-2: accenti italiani rimossi */
    void accentiRimossi() {
        /* a con accento grave */
        QCOMPARE(PC::normalizzaComune(QString::fromUtf8("citt\xc3\xa0")),
                 QString("citta"));
        /* e con accento grave */
        QCOMPARE(PC::normalizzaComune(QString::fromUtf8("caf\xc3\xa8")),
                 QString("cafe"));
        /* i con accento grave */
        QCOMPARE(PC::normalizzaComune(QString::fromUtf8("qua\xc3\xac")),
                 QString("quai"));
        /* o con accento grave */
        QCOMPARE(PC::normalizzaComune(QString::fromUtf8("giol\xc3\xb2")),
                 QString("giolo"));
        /* u con accento grave */
        QCOMPARE(PC::normalizzaComune(QString::fromUtf8("virt\xc3\xb9")),
                 QString("virtu"));
    }

    /* A-3: toLower applicato */
    void toLowerApplicato() {
        QCOMPARE(PC::normalizzaComune("ROMA"),    QString("roma"));
        QCOMPARE(PC::normalizzaComune("Milano"),  QString("milano"));
        QCOMPARE(PC::normalizzaComune("NAPOLI"),  QString("napoli"));
    }

    /* A-4: trim (spazi iniziali/finali rimossi) */
    void trimApplicato() {
        QCOMPARE(PC::normalizzaComune("  roma  "),   QString("roma"));
        QCOMPARE(PC::normalizzaComune("\tmilano\t"),  QString("milano"));
        QCOMPARE(PC::normalizzaComune(" torino"),     QString("torino"));
    }

    /* A-5: stringa vuota -> stringa vuota */
    void stringaVuota() {
        QCOMPARE(PC::normalizzaComune(""), QString(""));
        QCOMPARE(PC::normalizzaComune("   "), QString(""));
    }
};

/* ══════════════════════════════════════════════════════════════
   B — cercaBelfiore
   ══════════════════════════════════════════════════════════════ */
class TestCercaBelfiore : public QObject {
    Q_OBJECT
private slots:

    /* B-1: comuni capoluogo di regione */
    void capoluoghiRegione() {
        QCOMPARE(PC::cercaBelfiore("Roma"),     QString("H501"));
        QCOMPARE(PC::cercaBelfiore("Milano"),   QString("F205"));
        QCOMPARE(PC::cercaBelfiore("Napoli"),   QString("F839"));
        QCOMPARE(PC::cercaBelfiore("Torino"),   QString("L219"));
        QCOMPARE(PC::cercaBelfiore("Palermo"),  QString("G273"));
    }

    /* B-2: comune con apostrofo/accento (L'Aquila) */
    void comuneConApostrofo() {
        QCOMPARE(PC::cercaBelfiore("L'Aquila"), QString("A345"));
        QCOMPARE(PC::cercaBelfiore("l'aquila"), QString("A345"));
        /* alias senza apostrofo */
        QCOMPARE(PC::cercaBelfiore("Aquila"),   QString("A345"));
    }

    /* B-3: comune non esistente -> stringa vuota */
    void comuneNonEsiste() {
        QVERIFY(PC::cercaBelfiore("Paperopoli").isEmpty());
        QVERIFY(PC::cercaBelfiore("CittaFantasma").isEmpty());
    }

    /* B-4: case insensitive */
    void caseInsensitive() {
        QCOMPARE(PC::cercaBelfiore("ROMA"),  QString("H501"));
        QCOMPARE(PC::cercaBelfiore("roma"),  QString("H501"));
        QCOMPARE(PC::cercaBelfiore("Roma"),  QString("H501"));
        QCOMPARE(PC::cercaBelfiore("rOmA"),  QString("H501"));
    }

    /* B-5: paesi esteri (codici Z) */
    void paesiEsteri() {
        QCOMPARE(PC::cercaBelfiore("Francia"),  QString("Z110"));
        QCOMPARE(PC::cercaBelfiore("Germania"), QString("Z112"));
        QCOMPARE(PC::cercaBelfiore("USA"),      QString("Z404"));
    }

    /* B-6: alias (Olanda == Paesi Bassi) */
    void aliasComuni() {
        QCOMPARE(PC::cercaBelfiore("Olanda"),      QString("Z121"));
        QCOMPARE(PC::cercaBelfiore("Paesi Bassi"), QString("Z121"));
        QCOMPARE(PC::cercaBelfiore("olanda"),      QString("Z121"));
    }
};

/* ══════════════════════════════════════════════════════════════
   C — calcolaCodiceFiscale (algoritmo D.M. 23/12/1976)
   ══════════════════════════════════════════════════════════════ */
class TestCalcolaCodiceFiscale : public QObject {
    Q_OBJECT
private slots:

    /* C-1: caso reale verificabile: Rossi Mario, 01/01/1990, M, Roma */
    void casoRealeRossiMario() {
        const QString cf = PC::calcolaCodiceFiscale(
            "Rossi", "Mario",
            QDate(1990, 1, 1), true, "H501");
        QCOMPARE(cf, QString("RSSMRA90A01H501W")); /* D.M. 1976 corretto: somma=100, 100%26=22, 'A'+22='W' */
    }

    /* C-2: cognome con < 3 consonanti -> padding con X (es. "Oe" -> OE, voc: OE -> OEX) */
    void cognomePocheConsonanti() {
        /* "Oe": nessuna consonante, vocali O+E -> "OEX" */
        const QString cf = PC::calcolaCodiceFiscale(
            "Oe", "Mario",
            QDate(1990, 1, 1), true, "H501");
        QVERIFY(!cf.isEmpty());
        QVERIFY2(cf.left(3) == "OEX",
                 qPrintable(QString("Atteso OEX, ottenuto: %1").arg(cf.left(3))));
    }

    /* C-3: nome con >= 4 consonanti -> prende 1a+3a+4a (es. "Roberto": R,B,R,T -> 1a=R,3a=R,4a=T -> RRT) */
    void nomeQuattroConsonanti() {
        /* "Roberto": consonanti = R,B,R,T -> 1a=R, 3a=R, 4a=T -> "RRT" */
        const QString cf = PC::calcolaCodiceFiscale(
            "Rossi", "Roberto",
            QDate(1990, 1, 1), true, "H501");
        QVERIFY(!cf.isEmpty());
        QVERIFY2(cf.mid(3, 3) == "RRT",
                 qPrintable(QString("Atteso RRT, ottenuto: %1").arg(cf.mid(3, 3))));
    }

    /* C-4: femmina -> giorno + 40 (giorno 15 femmina = "55") */
    void femminaGiorno() {
        const QString cf = PC::calcolaCodiceFiscale(
            "Rossi", "Maria",
            QDate(1990, 1, 15), false, "H501");
        QVERIFY(!cf.isEmpty());
        /* posizione 9-10 (0-based) = giorno: atteso "55" */
        QVERIFY2(cf.mid(9, 2) == "55",
                 qPrintable(QString("Atteso giorno=55 (femmina 15+40), ottenuto: %1").arg(cf.mid(9, 2))));
    }

    /* C-5: data di nascita non valida -> stringa vuota */
    void dataNonValida() {
        QVERIFY(PC::calcolaCodiceFiscale("Rossi", "Mario",
                QDate(), true, "H501").isEmpty());
        QVERIFY(PC::calcolaCodiceFiscale("Rossi", "Mario",
                QDate(1990, 13, 1), true, "H501").isEmpty());
    }

    /* C-6: Belfiore di lunghezza sbagliata -> stringa vuota */
    void belfioreNonValido() {
        QVERIFY(PC::calcolaCodiceFiscale("Rossi", "Mario",
                QDate(1990, 1, 1), true, "H50").isEmpty());
        QVERIFY(PC::calcolaCodiceFiscale("Rossi", "Mario",
                QDate(1990, 1, 1), true, "H5012").isEmpty());
        QVERIFY(PC::calcolaCodiceFiscale("Rossi", "Mario",
                QDate(1990, 1, 1), true, "").isEmpty());
    }

    /* C-7: risultato ha esattamente 16 caratteri */
    void lunghezza16() {
        const QString cf = PC::calcolaCodiceFiscale(
            "Bianchi", "Luigi",
            QDate(1985, 6, 20), true, "F205");
        QVERIFY(!cf.isEmpty());
        QVERIFY2(cf.length() == 16,
                 qPrintable(QString("Lunghezza attesa 16, ottenuta: %1 (%2)")
                            .arg(cf.length()).arg(cf)));
    }

    /* C-8: risultato uppercase */
    void risultatoUppercase() {
        const QString cf = PC::calcolaCodiceFiscale(
            "verdi", "giuseppe",
            QDate(1980, 3, 10), true, "H501");
        QVERIFY(!cf.isEmpty());
        QCOMPARE(cf, cf.toUpper());
    }

    /* C-9: check digit corretto (ultima lettera = 'A' + somma%26) */
    void checkDigitCorretto() {
        /* CF corretto D.M. 1976: RSSMRA90A01H501W (somma=100, 100%26=22, 'A'+22='W') */
        const QString cf = PC::calcolaCodiceFiscale(
            "Rossi", "Mario",
            QDate(1990, 1, 1), true, "H501");
        QVERIFY(!cf.isEmpty());

        /* Ricalcola la somma con l'algoritmo corretto D.M. 1976:
           posizioni dispari → kDisp; posizioni pari → valore diretto (lettera A=0..Z=25) */
        static const int kDisp[36] = {
             1, 0, 5, 7, 9,13,15,17,19,21,
             1, 0, 5, 7, 9,13,15,17,19,21,
             2, 4,18,20,11, 3, 6, 8,12,14,
            16,10,22,25,24,23
        };
        int somma = 0;
        for (int i = 0; i < 15; ++i) {
            const QChar c = cf[i];
            if (i % 2 == 0) {
                const int idx = c.isDigit() ? c.digitValue() : 10 + (c.unicode() - 'A');
                somma += kDisp[idx];
            } else {
                somma += c.isDigit() ? c.digitValue() : (c.unicode() - 'A');
            }
        }
        const QChar atteso = QChar('A' + (somma % 26));
        QVERIFY2(cf[15] == atteso,
                 qPrintable(QString("Check digit atteso '%1', ottenuto '%2'")
                            .arg(atteso).arg(cf[15])));
    }

    /* C-10: codifica mese corretta ("ABCDEHLMPRST") */
    void codificaMese() {
        /* Tabella mesi: A=gen, B=feb, C=mar, D=apr, E=mag, H=giu,
                         L=lug, M=ago, P=set, R=ott, S=nov, T=dic */
        const QString mesi = "ABCDEHLMPRST";
        for (int m = 1; m <= 12; ++m) {
            const QString cf = PC::calcolaCodiceFiscale(
                "Rossi", "Mario",
                QDate(1990, m, 1), true, "H501");
            QVERIFY2(!cf.isEmpty(), qPrintable(QString("CF vuoto per mese %1").arg(m)));
            const QChar meseLetter = cf[7];  /* posizione 7 (0-based): yy=6-7, mese=8 -> pos 8 (0-based) */
            /* posizione: COG(3)+NOM(3)+YY(2) = index 8 */
            const QChar meseAtteso = mesi[m - 1];
            QVERIFY2(cf[8] == meseAtteso,
                     qPrintable(QString("Mese %1: atteso '%2', ottenuto '%3' in '%4'")
                                .arg(m).arg(meseAtteso).arg(cf[8]).arg(cf)));
        }
    }
};

/* ══════════════════════════════════════════════════════════════
   main
   ══════════════════════════════════════════════════════════════ */
int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    int result = 0;
    { TestNormalizzaComune t;      result |= QTest::qExec(&t, argc, argv); }
    { TestCercaBelfiore    t;      result |= QTest::qExec(&t, argc, argv); }
    { TestCalcolaCodiceFiscale t;  result |= QTest::qExec(&t, argc, argv); }
    return result;
}

#include "test_pratico_finanza.moc"
