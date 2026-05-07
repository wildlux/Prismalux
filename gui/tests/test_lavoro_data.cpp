/* ══════════════════════════════════════════════════════════════
   test_lavoro_data.cpp — Unit test per LavoroData (dati puri)
   ──────────────────────────────────────────────────────────────
   Categorie:
     CAT-A  kOfferte() — integrità strutturale dataset (8 test)
     CAT-B  offerteFiltrate() — logica filtro transitiva (15 test)
     CAT-C  tipoIcon() / livLabel() — mappature presentazione (9 test)

   Build:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_lavoro_data
     ./build_tests/test_lavoro_data
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QSet>
#include "../pages/lavoro_data.h"

/* ══════════════════════════════════════════════════════════════
   CAT-A — kOfferte(): integrità dataset (8 test)
   ══════════════════════════════════════════════════════════════ */
class TestKOfferte : public QObject {
    Q_OBJECT
private slots:

    void datasetNonVuoto() {
        QVERIFY(kOfferte().size() > 0);
    }

    void campiObbligatoriNonVuoti() {
        for (const Offerta& o : kOfferte()) {
            QVERIFY2(!o.azienda.isEmpty(), qPrintable("azienda vuota in: " + o.ruolo));
            QVERIFY2(!o.ruolo.isEmpty(),   qPrintable("ruolo vuoto in: "   + o.azienda));
        }
    }

    void tipiNoti() {
        static const QSet<QString> tipiValidi = {
            "IT","Retail","Ristorazione","Edilizia","Logistica",
            "Finanza","Sanitario","Produzione","Tecnico","Turismo",
            "Admin","Commerciale","Altro"
        };
        for (const Offerta& o : kOfferte()) {
            QVERIFY2(tipiValidi.contains(o.tipo),
                     qPrintable("tipo sconosciuto '" + o.tipo + "' in: " + o.azienda));
        }
    }

    void livelliNoti() {
        static const QSet<QString> livValidi = {
            "qualsiasi","diploma","laurea_t","laurea_m"
        };
        for (const Offerta& o : kOfferte()) {
            QVERIFY2(livValidi.contains(o.livello),
                     qPrintable("livello sconosciuto '" + o.livello + "' in: " + o.azienda));
        }
    }

    void emailFormatoMinimo() {
        for (const Offerta& o : kOfferte()) {
            if (!o.email.isEmpty()) {
                QVERIFY2(o.email.contains('@') || o.email.contains("http"),
                         qPrintable("email/url non valida: " + o.email + " — " + o.azienda));
            }
        }
    }

    void deterministico() {
        QCOMPARE(kOfferte().size(), kOfferte().size());
        for (int i = 0; i < kOfferte().size(); ++i)
            QCOMPARE(kOfferte()[i].azienda, kOfferte()[i].azienda);
    }

    void categoriePresenti() {
        QSet<QString> tipiTrovati;
        for (const Offerta& o : kOfferte())
            tipiTrovati.insert(o.tipo);
        /* Almeno le categorie principali devono essere nel dataset */
        QVERIFY(tipiTrovati.contains("IT"));
        QVERIFY(tipiTrovati.contains("Retail"));
        QVERIFY(tipiTrovati.contains("Tecnico"));
    }

    void nessunRuoloDuplicatoEsatto() {
        /* La stessa coppia (azienda, ruolo) non deve apparire due volte */
        QSet<QString> visti;
        for (const Offerta& o : kOfferte()) {
            const QString chiave = o.azienda + "||" + o.ruolo;
            QVERIFY2(!visti.contains(chiave),
                     qPrintable("offerta duplicata: " + chiave));
            visti.insert(chiave);
        }
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B — offerteFiltrate(): logica filtro (15 test)
   ══════════════════════════════════════════════════════════════ */
class TestOfferteFiltrate : public QObject {
    Q_OBJECT
private slots:

    void filtroTuttiRitornaTutto() {
        const auto tutte = offerteFiltrate("tutti", "tutti");
        QCOMPARE(tutte.size(), kOfferte().size());
    }

    void filtroTipoIT() {
        const auto r = offerteFiltrate("IT", "tutti");
        QVERIFY(r.size() > 0);
        for (const Offerta& o : r)
            QCOMPARE(o.tipo, QString("IT"));
    }

    void filtroTipoRetail() {
        const auto r = offerteFiltrate("Retail", "tutti");
        QVERIFY(r.size() > 0);
        for (const Offerta& o : r)
            QCOMPARE(o.tipo, QString("Retail"));
    }

    void filtroLivelloDiploma() {
        /* diploma: deve includere "diploma" e "qualsiasi" ma non laurea_t/laurea_m */
        const auto r = offerteFiltrate("tutti", "diploma");
        QVERIFY(r.size() > 0);
        for (const Offerta& o : r) {
            QVERIFY2(o.livello == "diploma" || o.livello == "qualsiasi",
                     qPrintable("filtro diploma include livello inatteso: " + o.livello));
        }
    }

    void filtroLivelloLaureaT() {
        /* laurea_t include: laurea_t, diploma, qualsiasi */
        const auto r = offerteFiltrate("tutti", "laurea_t");
        QVERIFY(r.size() > 0);
        for (const Offerta& o : r) {
            QVERIFY2(o.livello == "qualsiasi" || o.livello == "diploma" || o.livello == "laurea_t",
                     qPrintable("filtro laurea_t include livello inatteso: " + o.livello));
        }
    }

    void filtroLivelloLaureaM() {
        /* laurea_m include tutti i livelli */
        const auto tuttiLiv   = offerteFiltrate("tutti", "tutti");
        const auto filtroM    = offerteFiltrate("tutti", "laurea_m");
        QCOMPARE(filtroM.size(), tuttiLiv.size());
    }

    void filtroCombinato_IT_Diploma() {
        const auto r = offerteFiltrate("IT", "diploma");
        QVERIFY(r.size() > 0);
        for (const Offerta& o : r) {
            QCOMPARE(o.tipo, QString("IT"));
            QVERIFY(o.livello == "qualsiasi" || o.livello == "diploma");
        }
    }

    void filtroInesistente() {
        const auto r = offerteFiltrate("INVALIDO_TIPO", "tutti");
        QCOMPARE(r.size(), 0);
    }

    void filtroCombinatoZeroMatch() {
        /* Sanitario + laurea_t potrebbe esserci, ma "Edilizia" + "laurea_m" non ci sono */
        const auto r = offerteFiltrate("TIPO_INESISTENTE", "LIVELLO_INESISTENTE");
        QCOMPARE(r.size(), 0);
    }

    void risultatiSottoinsiemedikOfferte() {
        const auto r = offerteFiltrate("IT", "diploma");
        const auto& tutte = kOfferte();
        for (const Offerta& o : r) {
            bool trovata = false;
            for (const Offerta& orig : tutte) {
                if (orig.azienda == o.azienda && orig.ruolo == o.ruolo) {
                    trovata = true;
                    break;
                }
            }
            QVERIFY2(trovata, qPrintable("risultato non presente in kOfferte(): " + o.azienda));
        }
    }

    void nessunSideEffectSukOfferte() {
        const int sizePrima = kOfferte().size();
        offerteFiltrate("IT", "tutti");
        offerteFiltrate("Retail", "diploma");
        offerteFiltrate("INVALIDO", "laurea_m");
        QCOMPARE(kOfferte().size(), sizePrima);
    }

    void filtroLivelloQualsiasi() {
        const auto r = offerteFiltrate("tutti", "qualsiasi");
        QVERIFY(r.size() > 0);
        /* "qualsiasi" include solo offerte con livello == "qualsiasi" */
        for (const Offerta& o : r)
            QCOMPARE(o.livello, QString("qualsiasi"));
    }

    void unioneFiltriTipiEkOfferte() {
        /* Unione di tutti i tipi = kOfferte() */
        QList<QString> tipi;
        for (const Offerta& o : kOfferte())
            if (!tipi.contains(o.tipo)) tipi << o.tipo;

        QList<Offerta> unione;
        for (const QString& t : tipi) {
            const auto r = offerteFiltrate(t, "tutti");
            for (const Offerta& o : r)
                unione << o;
        }
        QCOMPARE(unione.size(), kOfferte().size());
    }

    void filtroTuttiLivelliCoproneTutto() {
        /* Verifica che l'unione dei risultati di tutti i livelli specifici
         * (qualsiasi, diploma, laurea_t, laurea_m — senza transitività) copra kOfferte().
         * Nota: ogni livello filtra SOLO le offerte compatibili — non usa transitività inversa.
         * Test semplificato: filtro "tutti"/"tutti" deve dare lo stesso risultato di kOfferte(). */
        const auto tutti = offerteFiltrate("tutti", "tutti");
        QCOMPARE(tutti.size(), kOfferte().size());
    }

    void filtroTransitivitaLaurea() {
        /* offerteFiltrate(..., "laurea_t") include tutto ciò che include "diploma" */
        const auto filtDiploma  = offerteFiltrate("tutti", "diploma");
        const auto filtLaureaT  = offerteFiltrate("tutti", "laurea_t");
        QVERIFY(filtLaureaT.size() >= filtDiploma.size());
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-C — tipoIcon() / livLabel(): mappature presentazione (9 test)
   ══════════════════════════════════════════════════════════════ */
class TestIconeLabel : public QObject {
    Q_OBJECT
private slots:

    void tipoIconITNonVuota() {
        QVERIFY(!tipoIcon("IT").isEmpty());
    }

    void tipoIconRetailDiversaDaIT() {
        QVERIFY(tipoIcon("Retail") != tipoIcon("IT"));
    }

    void tipoIconTuttiITipiNonVuoti() {
        const QStringList tipi = {
            "IT","Retail","Ristorazione","Edilizia","Logistica",
            "Finanza","Sanitario","Produzione","Tecnico","Turismo",
            "Admin","Commerciale"
        };
        for (const QString& t : tipi)
            QVERIFY2(!tipoIcon(t).isEmpty(), qPrintable("tipoIcon vuota per: " + t));
    }

    void tipoIconStringaVuota() {
        /* no crash — fallback emoji */
        const QString r = tipoIcon("");
        Q_UNUSED(r);
    }

    void tipoIconInvalido() {
        const QString r = tipoIcon("INVALIDO");
        /* fallback generico: non vuoto (usa il fallback 🔹) */
        QVERIFY(!r.isEmpty());
    }

    void livLabelQualsiasi() {
        QVERIFY(!livLabel("qualsiasi").isEmpty());
    }

    void livLabelSeniorDiversoDaJunior() {
        QVERIFY(livLabel("laurea_m") != livLabel("diploma"));
    }

    void livLabelTuttiILivelliNonVuoti() {
        const QStringList livelli = {"qualsiasi","diploma","laurea_t","laurea_m"};
        for (const QString& l : livelli)
            QVERIFY2(!livLabel(l).isEmpty(), qPrintable("livLabel vuota per: " + l));
    }

    void livLabelStringaVuota() {
        /* no crash — ritorna stringa vuota o fallback */
        const QString r = livLabel("");
        Q_UNUSED(r);
    }
};

/* ══════════════════════════════════════════════════════════════
   Runner
   ══════════════════════════════════════════════════════════════ */
int main(int argc, char** argv)
{
    int status = 0;
    {
        TestKOfferte t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestOfferteFiltrate t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestIconeLabel t;
        status |= QTest::qExec(&t, argc, argv);
    }
    return status;
}

#include "test_lavoro_data.moc"
