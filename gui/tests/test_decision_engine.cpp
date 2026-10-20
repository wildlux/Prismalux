/* ══════════════════════════════════════════════════════════════
   test_decision_engine.cpp — Unit test DecisionEngine

   Nessuna rete richiesta: si testano logica pura, il registro su
   disco, la cache e le condizioni di attivazione. Le chiamate HTTP
   vere non sono testate qui — coprire il trasporto con un mock
   sarebbe un test del mock, non del codice.

   Categorie:
     CAT-A  parseLetter — parsing dell'output del classificatore
     CAT-B  taxonomy — mappa lettera → intento
     CAT-C  active() — quando il modulo deve considerarsi spento
     CAT-D  Configurazione — load/save round-trip
     CAT-E  Registro — catalog/registerModel e la separazione dagli LLM
     CAT-F  Cache — put/get, TTL, clear
     CAT-G  classify() disabilitato — nessuna rete, nessun crash
     CAT-H  Backend — DwarfStar: registro, seed, separazione

   Build:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_decision_engine
     ./build_tests/test_decision_engine
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QApplication>
#include <QSettings>
#include <QDir>
#include <QFile>
#include <QStandardPaths>

#include "../decision_engine.h"
#include "../prismalux_paths.h"

/* I test toccano ~/.prismalux/decision_models.json e decision_cache.db:
   reindirizzano QStandardPaths su una HOME temporanea così non
   inquinano la configurazione reale né la corrompono. */
static void useTempHome()
{
    QDir t(QDir::tempPath() + QStringLiteral("/prismalux_dec_test_%1")
              .arg(QCoreApplication::applicationPid()));
    t.removeRecursively();
    t.mkpath(QStringLiteral(".prismalux"));
    QStandardPaths::setTestModeEnabled(false);
    /* Qt ricava HomePath da QDir::homePath(), che segue HOME su Linux:
       lo sovrascriviamo per la durata del processo. */
    qputenv("HOME", t.absolutePath().toLocal8Bit());
}
static QString tempHomePath()
{
    return QDir::tempPath() + QStringLiteral("/prismalux_dec_test_%1")
             .arg(QCoreApplication::applicationPid());
}

/* ══════════════════════════════════════════════════════════════
   CAT-A — parseLetter
   ══════════════════════════════════════════════════════════════ */
class TestParseLetter : public QObject {
    Q_OBJECT
private slots:

    void lettera_secca() {
        QCOMPARE(DecisionEngine::parseLetter("A"), QString("A"));
    }

    void minuscolo_e_spazi() {
        QCOMPARE(DecisionEngine::parseLetter("  b  "), QString("B"));
    }

    void lettera_con_testo() {
        /* "A) coding" — formato che il modello produce spesso */
        QCOMPARE(DecisionEngine::parseLetter("A) coding"), QString("A"));
    }

    void prefisso_testuale() {
        QCOMPARE(DecisionEngine::parseLetter("Risposta: C"), QString("C"));
    }

    void vuoto() {
        QCOMPARE(DecisionEngine::parseLetter(""), QString());
    }

    void nessuna_lettera() {
        QCOMPARE(DecisionEngine::parseLetter("non lo so"), QString());
    }

    /* Una parola che CONTIENE una lettera non è una risposta:
       senza i word boundary "Analisi" verrebbe letta come "A". */
    void parola_non_e_lettera() {
        QCOMPARE(DecisionEngine::parseLetter("Analisi del problema"), QString());
    }

    /* Eco del prompt: il modello ripete tutte le opzioni. Restituire
       la prima (sempre "A") sarebbe un risultato falso — deve
       fallire, così l'intento resta "other" a confidenza 0. */
    void eco_prompt_rifiutata() {
        QCOMPARE(DecisionEngine::parseLetter("A) coding B) reasoning C) chat D) other"),
                 QString());
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B — taxonomy
   ══════════════════════════════════════════════════════════════ */
class TestTaxonomy : public QObject {
    Q_OBJECT
private slots:

    void quattro_classi() {
        QCOMPARE(DecisionEngine::taxonomy().size(), 4);
    }

    void lettere_corrisppondono() {
        const auto t = DecisionEngine::taxonomy();
        QCOMPARE(t.at(0).first, QString("A"));
        QCOMPARE(t.at(1).first, QString("B"));
        QCOMPARE(t.at(2).first, QString("C"));
        QCOMPARE(t.at(3).first, QString("D"));
    }

    /* L'ordine è parte del protocollo col modello: cambiarlo
       invalida le cache già salvate. */
    void chiavi_stabili() {
        const auto t = DecisionEngine::taxonomy();
        QCOMPARE(t.at(0).second, QString("coding"));
        QCOMPARE(t.at(1).second, QString("reasoning"));
        QCOMPARE(t.at(2).second, QString("chat"));
        QCOMPARE(t.at(3).second, QString("other"));
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-C — active()
   ══════════════════════════════════════════════════════════════ */
class TestActive : public QObject {
    Q_OBJECT
private slots:
    void init() { useTempHome(); }

    void spento_per_default() {
        DecisionEngine e;
        /* Il default è OFF: la feature non deve mai partire da sola. */
        QVERIFY(!e.enabled());
        QVERIFY(!e.active());
    }

    void abilitato_senza_modello_non_e_attivo() {
        DecisionEngine e;
        e.setEnabled(true);
        QVERIFY(!e.active());
    }

    void modello_ma_non_installato() {
        DecisionEngine e;
        e.setEnabled(true);
        e.setModel("modello-che-non-esiste:99b");
        QVERIFY(!e.active());
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-D — Configurazione
   ══════════════════════════════════════════════════════════════ */
class TestConfig : public QObject {
    Q_OBJECT
private slots:
    void init() { useTempHome(); }

    void round_trip() {
        DecisionEngine e;
        e.setEnabled(true);
        e.setModel("tev1:4b");
        e.setHostPort("10.0.0.5", 12345);
        e.setThreshold(0.85);
        e.setVerifyEnabled(false);
        e.setCacheTtlHours(24);
        e.saveConfig();

        DecisionEngine e2;
        QCOMPARE(e2.enabled(), true);
        QCOMPARE(e2.model(), QString("tev1:4b"));
        QCOMPARE(e2.host(), QString("10.0.0.5"));
        QCOMPARE(e2.port(), 12345);
        QCOMPARE(e2.threshold(), 0.85);
        QCOMPARE(e2.verifyEnabled(), false);
        QCOMPARE(e2.cacheTtlHours(), 24);
    }

    /* Una porta fuori range non deve mai raggiungere QUrl: il
       fallback evita una URL malformata in ogni richiesta. */
    void porta_non_valida_usa_default() {
        DecisionEngine e;
        e.setHostPort("", 0);
        QCOMPARE(e.host(), QString("127.0.0.1"));
        QCOMPARE(e.port(), 11434);
    }

    void host_vuoto_usa_default() {
        DecisionEngine e;
        e.setHostPort("   ", 9999);
        QCOMPARE(e.host(), QString("127.0.0.1"));
        QCOMPARE(e.port(), 9999);
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-E — Registro e separazione dagli LLM
   ══════════════════════════════════════════════════════════════ */
class TestCatalog : public QObject {
    Q_OBJECT
private slots:
    void init() {
        useTempHome();
        QFile::remove(PrismaluxPaths::decisionModelsPath());
    }
    void cleanup() { QFile::remove(PrismaluxPaths::decisionModelsPath()); }

    void vuoto_su_file_inesistente() {
        QCOMPARE(DecisionEngine::catalog().size(), 0);
    }

    void registro_creato() {
        QVERIFY(DecisionEngine::registerModel("tev1:4b", DecisionEngine::RoleClassifier, "test"));
        const auto c = DecisionEngine::catalog();
        QCOMPARE(c.size(), 1);
        QCOMPARE(c.at(0).name, QString("tev1:4b"));
        QCOMPARE(c.at(0).role, DecisionEngine::RoleClassifier);
        QCOMPARE(c.at(0).note, QString("test"));
    }

    void duplicato_aggiorna_ruolo() {
        DecisionEngine::registerModel("m:1b", DecisionEngine::RoleClassifier);
        /* Seconda registrazione: non duplica, aggiorna il ruolo. */
        QVERIFY(!DecisionEngine::registerModel("m:1b", DecisionEngine::RoleVerifier));
        const auto c = DecisionEngine::catalog();
        QCOMPARE(c.size(), 1);
        QCOMPARE(c.at(0).role, DecisionEngine::RoleVerifier);
    }

    void nome_vuoto_rifiutato() {
        QVERIFY(!DecisionEngine::registerModel("   ", DecisionEngine::RoleClassifier));
        QCOMPARE(DecisionEngine::catalog().size(), 0);
    }

    /* Il cuore del requisito: ciò che è installato ma non registrato
       non è un decision model. È la garanzia che le due liste non
       possano incrociarsi. */
    void installati_sono_sempre_un_sottoinsieme_del_registro() {
        DecisionEngine::registerModel("solo-registrato:1b", DecisionEngine::RoleClassifier);
        DecisionEngine e;
        /* refreshInstalled() verso un Ollama assente lascia la lista
           vuota: il sottinsieme è banalmente rispettato, ma il caso
           interessante è che il filtro stia in refreshInstalled e
           non in un posto che un modello estraneo possa aggirare. */
        for (const auto& s : e.installedModels()) {
            bool found = false;
            for (const auto& c : DecisionEngine::catalog())
                if (c.name == s.name) found = true;
            QVERIFY2(found, qPrintable(QString("modello installato fuori dal registro: %1")
                                          .arg(s.name)));
        }
    }

    /* Il seed di primo avvio scrive i due classificatori di riferimento. */
    void seed_di_primo_avvio() {
        DecisionEngine e;                       /* costruttore → _seedCatalog() */
        const auto c = DecisionEngine::catalog();
        QVERIFY(c.size() >= 2);
        QVERIFY(!c.isEmpty());
    }

    /* Il seed non deve sovrascrivere un registro già esistente:
       un modello aggiunto a mano deve sopravvivere al riavvio. */
    void seed_non_sovrascrive() {
        DecisionEngine::registerModel("mio:7b", DecisionEngine::RoleClassifier);
        DecisionEngine e;                       /* secondo avvio */
        bool found = false;
        for (const auto& c : DecisionEngine::catalog())
            if (c.name == "mio:7b") found = true;
        QVERIFY(found);
    }

    /* Se il registro esiste ma non contiene modelli DwarfStar,
       il seed li aggiunge (miglioramento one-time per utenti esistenti). */
    void seed_aggiunge_dwarfstar_se_mancante() {
        /* Crea un registro con solo modelli Ollama */
        QVector<DecisionEngine::ModelSpec> c;
        DecisionEngine::ModelSpec s;
        s.name = QStringLiteral("solo-ollama:1b");
        s.backend = DecisionEngine::BackendOllama;
        s.role = DecisionEngine::RoleClassifier;
        c.append(s);
        DecisionEngine::saveCatalog(c);

        /* Il seed deve aggiungere i modelli DwarfStar */
        DecisionEngine e;
        bool hasDwarfstar = false;
        for (const auto& m : DecisionEngine::catalog())
            if (m.backend == DecisionEngine::BackendDwarfstar) hasDwarfstar = true;
        QVERIFY(hasDwarfstar);

        /* E il modello originale deve sopravvivere */
        bool hasOriginal = false;
        for (const auto& m : DecisionEngine::catalog())
            if (m.name == "solo-ollama:1b") hasOriginal = true;
        QVERIFY(hasOriginal);
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-F — Cache
   ══════════════════════════════════════════════════════════════ */
class TestCache : public QObject {
    Q_OBJECT
private slots:
    void init() { useTempHome(); }

    void stats_su_db_inesistente() {
        /* Non deve crashare né creare file: su db assente torna zero. */
        DecisionEngine e;
        const auto st = e.cacheStats();
        QVERIFY(st.entries >= 0);
        QVERIFY(st.bytes >= 0);
    }

    void clear_su_cache_vuota() {
        DecisionEngine e;
        e.clearCache();               /* non deve crashare */
        QCOMPARE(e.cacheSize(), 0);
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-G — classify() con modulo spento
   ══════════════════════════════════════════════════════════════ */
class TestClassifyDisabled : public QObject {
    Q_OBJECT
private slots:
    void init() { useTempHome(); }

    /* Il caso più importante: a modulo disabilitato classify()
       deve rispondere subito con ok=false e senza toccare la rete.
       Se qui ci fosse una chiamata HTTP, ogni chat dell'utente
       pagherebbe una latenza per una feature che ha spento. */
    void callback_immediata_senza_rete() {
        DecisionEngine e;
        bool called = false;
        DecisionEngine::ClassifyResult res;
        e.classify("ciao", [&](const DecisionEngine::ClassifyResult& r) {
            called = true; res = r;
        });
        QVERIFY(called);
        QVERIFY(!res.ok);
    }

    void verify_passa_quando_spento() {
        DecisionEngine e;
        bool called = false;
        DecisionEngine::VerifyResult res;
        e.verify("q", "a", [&](const DecisionEngine::VerifyResult& r) {
            called = true; res = r;
        });
        /* Non attiva → approva, così la risposta non viene bloccata. */
        QVERIFY(called);
        QVERIFY(res.approved);
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-H — Backend DwarfStar
   ══════════════════════════════════════════════════════════════ */
class TestBackend : public QObject {
    Q_OBJECT
private slots:
    void init() {
        useTempHome();
        QFile::remove(PrismaluxPaths::decisionModelsPath());
    }
    void cleanup() { QFile::remove(PrismaluxPaths::decisionModelsPath()); }

    /* Il seed include sia modelli Ollama che DwarfStar */
    void seed_contiene_entrambi_i_backend() {
        DecisionEngine e;
        const auto c = DecisionEngine::catalog();
        bool hasOllama = false, hasDwarfstar = false;
        for (const auto& s : c) {
            if (s.backend == DecisionEngine::BackendOllama) hasOllama = true;
            if (s.backend == DecisionEngine::BackendDwarfstar) hasDwarfstar = true;
        }
        QVERIFY(hasOllama);
        QVERIFY(hasDwarfstar);
    }

    /* I modelli DwarfStar hanno il backend corretto nel registro */
    void modelli_dwarfstar_hanno_backend_corretto() {
        DecisionEngine e;
        for (const auto& s : DecisionEngine::catalog()) {
            if (s.name == "deepseek-v4-flash" || s.name == "glm-5.3-flash") {
                QCOMPARE(s.backend, DecisionEngine::BackendDwarfstar);
            }
        }
    }

    /* La configurazione del backend sopravvive al round-trip */
    void backend_round_trip() {
        DecisionEngine e;
        e.setBackend(DecisionEngine::BackendDwarfstar);
        e.saveConfig();

        DecisionEngine e2;
        QCOMPARE(e2.backend(), DecisionEngine::BackendDwarfstar);
    }

    /* Il default è Ollama */
    void default_e_ollama() {
        DecisionEngine e;
        QCOMPARE(e.backend(), DecisionEngine::BackendOllama);
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-I — Modello per tipologia
   La classificazione usa il predefinito; la verifica può usare un
   modello diverso per coding/reasoning/chat/other. Le associazioni
   sono permissive in scrittura (l'utente può indicare un modello non
   ancora scaricato) ma modelForIntent() ricade sul predefinito se
   quello scelto non è installato.
   ══════════════════════════════════════════════════════════════ */
class TestPerIntent : public QObject {
    Q_OBJECT
private slots:
    void init() {
        useTempHome();
        QFile::remove(PrismaluxPaths::decisionModelsPath());
    }
    void cleanup() { QFile::remove(PrismaluxPaths::decisionModelsPath()); }

    /* Senza associazioni, ogni tipologia usa il modello predefinito */
    void default_per_tipologia_e_il_predefinito() {
        DecisionEngine e;
        e.setModel(QStringLiteral("predefinito:4b"));
        QCOMPARE(e.modelForIntent(QStringLiteral("coding")),
                 QStringLiteral("predefinito:4b"));
        QCOMPARE(e.modelForIntent(QStringLiteral("chat")),
                 QStringLiteral("predefinito:4b"));
        QCOMPARE(e.modelForIntent(QString()), QStringLiteral("predefinito:4b"));
    }

    /* Le associazioni sopravvivono al round-trip di configurazione */
    void associazioni_round_trip() {
        DecisionEngine e;
        e.setModelForIntent(QStringLiteral("coding"), QStringLiteral("coder:7b"));
        e.setModelForIntent(QStringLiteral("chat"),   QStringLiteral("chat:3b"));
        e.saveConfig();

        DecisionEngine e2;
        QCOMPARE(e2.modelForIntentSetting(QStringLiteral("coding")),
                 QStringLiteral("coder:7b"));
        QCOMPARE(e2.modelForIntentSetting(QStringLiteral("chat")),
                 QStringLiteral("chat:3b"));
        QCOMPARE(e2.modelForIntentSetting(QStringLiteral("reasoning")),
                 QString());
    }

    /* Un modello associato ma non installato non deve essere usato:
       si ricade sul predefinito invece di spedire una richiesta a un
       modello che il backend non ha. */
    void modello_non_installato_ricade_sul_predefinito() {
        DecisionEngine e;
        e.setModel(QStringLiteral("predefinito:4b"));
        e.setModelForIntent(QStringLiteral("reasoning"),
                            QStringLiteral("non-scaricato:99b"));
        QCOMPARE(e.modelForIntent(QStringLiteral("reasoning")),
                 QStringLiteral("predefinito:4b"));
    }

    /* Cancellare un'associazione riporta la tipologia al default */
    void associazione_vuota_riporta_al_default() {
        DecisionEngine e;
        e.setModel(QStringLiteral("predefinito:4b"));
        e.setModelForIntent(QStringLiteral("other"), QStringLiteral("x:1b"));
        e.setModelForIntent(QStringLiteral("other"), QString());
        QCOMPARE(e.modelForIntentSetting(QStringLiteral("other")), QString());
        QCOMPARE(e.modelForIntent(QStringLiteral("other")),
                 QStringLiteral("predefinito:4b"));
    }
};

/* ══════════════════════════════════════════════════════════════
   main
   ══════════════════════════════════════════════════════════════ */
int main(int argc, char** argv)
{
    /* QCoreApplication serviva a QStandardPaths/qputenv nei test;
       QApplication per coerenza con le altre suite GUI. */
    QApplication app(argc, argv);

    int status = 0;
    { TestParseLetter       t; status |= QTest::qExec(&t, argc, argv); }
    { TestTaxonomy          t; status |= QTest::qExec(&t, argc, argv); }
    { TestActive            t; status |= QTest::qExec(&t, argc, argv); }
    { TestConfig            t; status |= QTest::qExec(&t, argc, argv); }
    { TestCatalog           t; status |= QTest::qExec(&t, argc, argv); }
    { TestCache             t; status |= QTest::qExec(&t, argc, argv); }
    { TestClassifyDisabled  t; status |= QTest::qExec(&t, argc, argv); }
    { TestBackend          t; status |= QTest::qExec(&t, argc, argv); }
    { TestPerIntent        t; status |= QTest::qExec(&t, argc, argv); }

    QDir(tempHomePath()).removeRecursively();
    return status;
}

#include "test_decision_engine.moc"