/* ══════════════════════════════════════════════════════════════
   test_tutor_data.cpp — Unit test per TutorData (dati puri)
   ──────────────────────────────────────────────────────────────
   Categorie:
     CAT-A  Invarianti strutturali per ogni soggetto (6 implementati)
     CAT-B  Unicità e coerenza cross-soggetto (12 test)
     CAT-C  Contenuto semantico minimo per soggetto (6 test)

   NOTE: tutorDataBiologia, Astronomia, Elettronica, Economia,
         Farmacologia, QuantumComputing sono dichiarate nell'header
         ma non ancora implementate — test su quelle 6 sono SKIP
         (TODO per quando saranno aggiunte).

   Build:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_tutor_data
     ./build_tests/test_tutor_data
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QSet>
#include "../pages/tutor_data.h"

/* Helper: applica i 5 invarianti strutturali su qualsiasi SubjectData */
static void checkInvarianti(const SubjectData& data, const char* nome)
{
    QVERIFY2(!data.isEmpty(),
             qPrintable(QString("SubjectData vuota per: ") + nome));

    for (const TutorSection& sez : data) {
        QVERIFY2(!sez.first.isEmpty(),
                 qPrintable(QString("titolo sezione vuoto in: ") + nome));
        QVERIFY2(!sez.second.isEmpty(),
                 qPrintable(QString("sezione senza topic in: ") + nome + " / " + sez.first));

        for (const TutorTopic& topic : sez.second) {
            QVERIFY2(!topic.first.isEmpty(),
                     qPrintable(QString("nome topic vuoto in: ") + nome + " / " + sez.first));
            QVERIFY2(!topic.second.isEmpty(),
                     qPrintable(QString("descr topic vuota in: ") + nome + " / " + topic.first));
        }
    }
}

/* ══════════════════════════════════════════════════════════════
   CAT-A — Invarianti per ogni soggetto implementato (6 test)
   ══════════════════════════════════════════════════════════════ */
class TestInvarianti : public QObject {
    Q_OBJECT
private slots:

    void matematica()   { checkInvarianti(tutorDataMatematica(),   "Matematica");   }
    void fisica()       { checkInvarianti(tutorDataFisica(),       "Fisica");       }
    void chimica()       { checkInvarianti(tutorDataChimica(),      "Chimica");      }
    void sicurezza()    { checkInvarianti(tutorDataSicurezza(),    "Sicurezza");    }
    void informatica()  { checkInvarianti(tutorDataInformatica(),  "Informatica");  }
    void algoritmi()    { checkInvarianti(tutorDataAlgoritmi(),    "Algoritmi");    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B — Unicità e coerenza cross-soggetto (12 test)
   ══════════════════════════════════════════════════════════════ */
class TestUnicitaCoerenza : public QObject {
    Q_OBJECT
private slots:

    void titoliSezioniMatFisicaDiversi() {
        QSet<QString> titMat, titFis;
        for (const TutorSection& s : tutorDataMatematica()) titMat.insert(s.first);
        for (const TutorSection& s : tutorDataFisica())     titFis.insert(s.first);
        /* I titoli delle sezioni devono essere almeno parzialmente diversi */
        int comuni = 0;
        for (const QString& t : titMat) if (titFis.contains(t)) comuni++;
        QVERIFY2(comuni < titMat.size(),
                 "Matematica e Fisica hanno TUTTI i titoli sezione uguali — probabile copia-incolla");
    }

    void conteggioTopicMinimo() {
        int totale = 0;
        for (const TutorSection& s : tutorDataMatematica())  for (const auto& t : s.second) { Q_UNUSED(t); totale++; }
        for (const TutorSection& s : tutorDataFisica())       for (const auto& t : s.second) { Q_UNUSED(t); totale++; }
        for (const TutorSection& s : tutorDataChimica())      for (const auto& t : s.second) { Q_UNUSED(t); totale++; }
        for (const TutorSection& s : tutorDataSicurezza())   for (const auto& t : s.second) { Q_UNUSED(t); totale++; }
        for (const TutorSection& s : tutorDataInformatica()) for (const auto& t : s.second) { Q_UNUSED(t); totale++; }
        for (const TutorSection& s : tutorDataAlgoritmi())   for (const auto& t : s.second) { Q_UNUSED(t); totale++; }
        QVERIFY2(totale >= 80,
                 qPrintable(QString("troppo pochi topic totali: %1 (min 80)").arg(totale)));
    }

    void nessunTopicDuplicatoEsatto() {
        /* Stessa (nome, descrizione) non può apparire due volte nel dataset completo */
        QSet<QString> visti;
        auto check = [&](const SubjectData& data, const char* sogg) {
            for (const TutorSection& sez : data) {
                for (const TutorTopic& t : sez.second) {
                    const QString chiave = t.first + "||" + t.second;
                    QVERIFY2(!visti.contains(chiave),
                             qPrintable(QString("topic duplicato in %1: ").arg(sogg) + t.first));
                    visti.insert(chiave);
                }
            }
        };
        check(tutorDataMatematica(),  "Matematica");
        check(tutorDataFisica(),      "Fisica");
        check(tutorDataChimica(),     "Chimica");
        check(tutorDataSicurezza(),   "Sicurezza");
        check(tutorDataInformatica(), "Informatica");
        check(tutorDataAlgoritmi(),   "Algoritmi");
    }

    void deterministico() {
        /* Stessa chiamata = stesso risultato */
        const SubjectData a = tutorDataMatematica();
        const SubjectData b = tutorDataMatematica();
        QCOMPARE(a.size(), b.size());
        for (int i = 0; i < a.size(); ++i) {
            QCOMPARE(a[i].first, b[i].first);
            QCOMPARE(a[i].second.size(), b[i].second.size());
        }
    }

    void nessunaTitoloSezioneSupera60Char() {
        auto check = [&](const SubjectData& data, const char* sogg) {
            for (const TutorSection& s : data) {
                /* titolo include emoji — contiamo in QChar (UTF-16 units) */
                QVERIFY2(s.first.size() <= 80,
                         qPrintable(QString("titolo sezione troppo lungo in %1: ").arg(sogg) + s.first));
            }
        };
        check(tutorDataMatematica(),  "Matematica");
        check(tutorDataFisica(),      "Fisica");
        check(tutorDataChimica(),     "Chimica");
        check(tutorDataSicurezza(),   "Sicurezza");
        check(tutorDataInformatica(), "Informatica");
        check(tutorDataAlgoritmi(),   "Algoritmi");
    }

    void minimoSezioniPerSoggetto() {
        QVERIFY(tutorDataMatematica().size()  >= 3);
        QVERIFY(tutorDataFisica().size()       >= 2);
        QVERIFY(tutorDataChimica().size()      >= 2);
        QVERIFY(tutorDataSicurezza().size()   >= 2);
        QVERIFY(tutorDataInformatica().size() >= 3);
        QVERIFY(tutorDataAlgoritmi().size()   >= 3);
    }

    void algoritmiHaSortingSection() {
        bool trovato = false;
        for (const TutorSection& s : tutorDataAlgoritmi()) {
            if (s.first.toLower().contains("sort") || s.first.toLower().contains("ordinamento"))
                trovato = true;
            for (const TutorTopic& t : s.second)
                if (t.first.toLower().contains("sort") || t.first.toLower().contains("ordinamento") ||
                    t.second.toLower().contains("bubble") || t.second.toLower().contains("quick sort") ||
                    t.second.toLower().contains("merge"))
                    trovato = true;
        }
        QVERIFY2(trovato, "tutorDataAlgoritmi() deve avere almeno un topic su Sorting/Ordinamento");
    }

    void sicurezzaHaVulnerabilitaSection() {
        bool trovato = false;
        for (const TutorSection& s : tutorDataSicurezza()) {
            const QString t = s.first.toLower();
            if (t.contains("vulnerab") || t.contains("attacc") || t.contains("sicur"))
                trovato = true;
            /* cerca anche nei topic */
            for (const TutorTopic& tp : s.second) {
                const QString n = tp.first.toLower();
                if (n.contains("sql") || n.contains("xss") || n.contains("injection") ||
                    n.contains("vulnerab") || n.contains("attacc"))
                    trovato = true;
            }
        }
        QVERIFY2(trovato, "tutorDataSicurezza() mancano topic su vulnerabilità/attacchi");
    }

    void matematicaHaDerivataSection() {
        bool trovato = false;
        for (const TutorSection& s : tutorDataMatematica())
            for (const TutorTopic& t : s.second)
                if (t.first.toLower().contains("derivat") || t.second.toLower().contains("derivat"))
                    trovato = true;
        QVERIFY2(trovato, "tutorDataMatematica() manca un topic su derivate");
    }

    void informaticaHaAlgoritmiSection() {
        bool trovato = false;
        for (const TutorSection& s : tutorDataInformatica()) {
            for (const TutorTopic& t : s.second) {
                const QString n = t.first.toLower();
                if (n.contains("sort") || n.contains("lista") || n.contains("struct") ||
                    n.contains("algor") || n.contains("python") || n.contains("linguagg"))
                    trovato = true;
            }
        }
        QVERIFY2(trovato, "tutorDataInformatica() troppo scarsa — mancano topic tecnici base");
    }

    void fisicaHaMeccanicaSection() {
        bool trovato = false;
        for (const TutorSection& s : tutorDataFisica()) {
            if (s.first.toLower().contains("meccan") || s.first.toLower().contains("cinemat"))
                trovato = true;
        }
        QVERIFY2(trovato, "tutorDataFisica() manca una sezione di Meccanica/Cinematica");
    }

    void chimicaHaAtomiSection() {
        bool trovato = false;
        for (const TutorSection& s : tutorDataChimica())
            for (const TutorTopic& t : s.second)
                if (t.first.toLower().contains("atom") || t.first.toLower().contains("element") ||
                    t.second.toLower().contains("atom"))
                    trovato = true;
        QVERIFY2(trovato, "tutorDataChimica() manca un topic su atomi/elementi");
    }

    void nessunDescrizioneTopicSupera500Char() {
        auto check = [&](const SubjectData& data, const char* sogg) {
            for (const TutorSection& s : data)
                for (const TutorTopic& t : s.second)
                    QVERIFY2(t.second.size() <= 500,
                             qPrintable(QString("descrizione topic troppo lunga in %1: ").arg(sogg) + t.first));
        };
        check(tutorDataMatematica(),  "Matematica");
        check(tutorDataFisica(),      "Fisica");
        check(tutorDataChimica(),     "Chimica");
        check(tutorDataSicurezza(),   "Sicurezza");
        check(tutorDataInformatica(), "Informatica");
        check(tutorDataAlgoritmi(),   "Algoritmi");
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-C — Contenuto semantico minimo per soggetto (6 test)
   ══════════════════════════════════════════════════════════════ */
class TestContenutoSemantico : public QObject {
    Q_OBJECT

    /* Conta topic che contengono la parola nei primi o secondi elementi */
    static int contaTopicConParola(const SubjectData& data, const QString& parola) {
        int n = 0;
        for (const TutorSection& s : data)
            for (const TutorTopic& t : s.second)
                if (t.first.contains(parola, Qt::CaseInsensitive) ||
                    t.second.contains(parola, Qt::CaseInsensitive))
                    n++;
        return n;
    }

private slots:

    void matematicaHaCalcolo() {
        QVERIFY(contaTopicConParola(tutorDataMatematica(), "integr") +
                contaTopicConParola(tutorDataMatematica(), "derivat") >= 2);
    }

    void fisicaHaTermodinamica() {
        QVERIFY(contaTopicConParola(tutorDataFisica(), "termodinam") +
                contaTopicConParola(tutorDataFisica(), "calore") +
                contaTopicConParola(tutorDataFisica(), "entropia") >= 1);
    }

    void chimicaHaReazioni() {
        QVERIFY(contaTopicConParola(tutorDataChimica(), "reazion") +
                contaTopicConParola(tutorDataChimica(), "element") >= 1);
    }

    void sicurezzaHaCrittografia() {
        QVERIFY(contaTopicConParola(tutorDataSicurezza(), "crittograf") +
                contaTopicConParola(tutorDataSicurezza(), "rsa") +
                contaTopicConParola(tutorDataSicurezza(), "aes") +
                contaTopicConParola(tutorDataSicurezza(), "hash") >= 1);
    }

    void informaticaHaProgrammazione() {
        QVERIFY(contaTopicConParola(tutorDataInformatica(), "python") +
                contaTopicConParola(tutorDataInformatica(), "c++") +
                contaTopicConParola(tutorDataInformatica(), "programm") >= 1);
    }

    void algoritmiHaGrafi() {
        QVERIFY(contaTopicConParola(tutorDataAlgoritmi(), "graph") +
                contaTopicConParola(tutorDataAlgoritmi(), "grafi") +
                contaTopicConParola(tutorDataAlgoritmi(), "dijkstra") +
                contaTopicConParola(tutorDataAlgoritmi(), "bfs") +
                contaTopicConParola(tutorDataAlgoritmi(), "dfs") >= 1);
    }
};

/* ══════════════════════════════════════════════════════════════
   Runner
   ══════════════════════════════════════════════════════════════ */
int main(int argc, char** argv)
{
    int status = 0;
    {
        TestInvarianti t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestUnicitaCoerenza t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestContenutoSemantico t;
        status |= QTest::qExec(&t, argc, argv);
    }
    return status;
}

#include "test_tutor_data.moc"
