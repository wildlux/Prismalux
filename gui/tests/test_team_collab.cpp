/* ══════════════════════════════════════════════════════════════
   test_team_collab.cpp — Test di collaborazione multi-agente
   ──────────────────────────────────────────────────────────────
   Simula un team di 4 agenti AI che lavorano in pipeline:
     Fase 1 — Architetto  (gpt-oss:120b)  : progetta il design
     Fase 2 — Sviluppatore(qwen2.5-coder) : implementa il codice
     Fase 3 — Revisore    (gpt-oss:120b)  : analizza il codice
     Fase 4 — Tester      (qwen2.5-coder) : scrive i test pytest

   L'output di ogni fase alimenta la fase successiva.

   Categorie:
     CAT-J  Sanity    — nessuna rete, verifica costanti e helpers
     CAT-K  Pipeline  — esecuzione sequenziale 4-fase (RETE REALE)
     CAT-L  Qualità   — verifica del contenuto degli output prodotti

   REQUISITI:
     - Ollama in esecuzione su 127.0.0.1:11434
     - Modello gpt-oss:120b disponibile  (override: PRISMALUX_TEST_MODEL_REASONING)
     - Modello qwen2.5-coder disponibile (override: PRISMALUX_TEST_MODEL_CODER)

   COME ESEGUIRE:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_team_collab
     ./build_tests/test_team_collab
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QStringList>

#include "../ai_client.h"

/* ── Timeout per modelli grandi ───────────────────────────────── */
static constexpr int kTimeoutReasoningMs = 600'000;  /* 10 min — 120b model  */
static constexpr int kTimeoutCoderMs     = 300'000;  /* 5 min  — coder model */
static constexpr int kConnTimeoutMs      =   5'000;  /* 5s    — check rete   */

/* ── Nomi modello (override da env) ───────────────────────────── */
static QString modelReasoning()
{
    const QString env = QProcessEnvironment::systemEnvironment()
                            .value("PRISMALUX_TEST_MODEL_REASONING");
    return env.isEmpty() ? QStringLiteral("gpt-oss:120b") : env;
}

static QString modelCoder()
{
    const QString env = QProcessEnvironment::systemEnvironment()
                            .value("PRISMALUX_TEST_MODEL_CODER");
    return env.isEmpty() ? QStringLiteral("qwen2.5-coder") : env;
}

/* ══════════════════════════════════════════════════════════════
   Helpers
   ══════════════════════════════════════════════════════════════ */

struct PhaseResult {
    QString  response;
    QString  errorMsg;
    bool     ok         = false;
    int      elapsed_ms = 0;
    int      tokenCount = 0;
    QString  model;
    QString  phase;
};

/* Esegue chat() con un modello specifico e blocca su QEventLoop. */
static PhaseResult runPhase(const QString& phaseName,
                             const QString& model,
                             const QString& system,
                             const QString& message,
                             int timeoutMs)
{
    PhaseResult res;
    res.phase = phaseName;
    res.model = model;

    AiClient ai;
    ai.setBackend(AiClient::Ollama, "127.0.0.1", 11434, model);

    QEventLoop  loop;
    QElapsedTimer timer;
    QTimer      watchdog;

    QObject::connect(&ai, &AiClient::token, &loop, [&](const QString&) {
        ++res.tokenCount;
    });

    QObject::connect(&ai, &AiClient::finished, &loop, [&](const QString& full) {
        res.response   = full;
        res.ok         = true;
        res.elapsed_ms = static_cast<int>(timer.elapsed());
        loop.quit();
    });

    QObject::connect(&ai, &AiClient::error, &loop, [&](const QString& err) {
        res.errorMsg   = err;
        res.ok         = false;
        res.elapsed_ms = static_cast<int>(timer.elapsed());
        loop.quit();
    });

    watchdog.setSingleShot(true);
    QObject::connect(&watchdog, &QTimer::timeout, &loop, [&]() {
        res.errorMsg = phaseName + ": TIMEOUT dopo " +
                       QString::number(timeoutMs / 1000) + "s";
        res.ok = false;
        loop.quit();
    });

    timer.start();
    watchdog.start(timeoutMs);
    ai.chat(system, message);
    loop.exec();

    return res;
}

/* Controlla che Ollama sia raggiungibile con un modello specifico. */
static bool modelAvailable(const QString& model)
{
    AiClient ai;
    ai.setBackend(AiClient::Ollama, "127.0.0.1", 11434, model);
    bool found = false;

    QEventLoop loop;
    QTimer     watchdog;

    QObject::connect(&ai, &AiClient::modelsReady, &loop,
                     [&](const QStringList& list) {
        found = list.contains(model) ||
                list.contains(model + ":latest") ||
                std::any_of(list.begin(), list.end(),
                            [&](const QString& m) {
                                return m.startsWith(model.section(':', 0, 0));
                            });
        loop.quit();
    });

    watchdog.setSingleShot(true);
    QObject::connect(&watchdog, &QTimer::timeout, &loop, &QEventLoop::quit);

    watchdog.start(kConnTimeoutMs);
    ai.fetchModels();
    loop.exec();

    return found;
}

/* ══════════════════════════════════════════════════════════════
   CAT-J — Sanity (nessuna rete richiesta)
   Verifica costanti, nomi modello, struttura helper.
   ══════════════════════════════════════════════════════════════ */
class TestTeamSanity : public QObject {
    Q_OBJECT
private slots:

    /* I nomi modello non sono vuoti */
    void modelNamesNotEmpty()
    {
        QVERIFY(!modelReasoning().isEmpty());
        QVERIFY(!modelCoder().isEmpty());
    }

    /* I due modelli sono distinti */
    void modelNamesDistinct()
    {
        QVERIFY(modelReasoning() != modelCoder());
    }

    /* I timeout sono ragionevoli (> 1 min per reasoning, > 30s per coder) */
    void timeoutsReasonable()
    {
        QVERIFY(kTimeoutReasoningMs >= 60'000);
        QVERIFY(kTimeoutCoderMs     >= 30'000);
        QVERIFY(kTimeoutReasoningMs >  kTimeoutCoderMs);
    }

    /* PhaseResult parte in stato non-ok */
    void phaseResultInitialState()
    {
        PhaseResult r;
        QVERIFY(!r.ok);
        QVERIFY(r.response.isEmpty());
        QCOMPARE(r.tokenCount, 0);
    }

    /* Env override funziona (simula override rilevando QProcessEnvironment) */
    void envOverrideReadable()
    {
        /* Verifica solo che la lettura non produca crash */
        const QString mr = modelReasoning();
        const QString mc = modelCoder();
        QVERIFY(mr.length() > 0);
        QVERIFY(mc.length() > 0);
    }

    /* I nomi fase (usati nel report) non sono vuoti */
    void phaseNamesValid()
    {
        const QStringList fasi = {
            "Architetto", "Sviluppatore", "Revisore", "Tester"
        };
        for (const QString& f : fasi)
            QVERIFY(!f.isEmpty());
        QCOMPARE(fasi.size(), 4);
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-K — Pipeline sequenziale (RETE REALE)
   I 4 agenti lavorano in sequenza su un task comune:
   progettare e implementare un TaskManager Python.
   ══════════════════════════════════════════════════════════════ */
class TestTeamPipeline : public QObject {
    Q_OBJECT

    /* Task condiviso del team */
    static constexpr const char* kTaskDesc =
        "una semplice classe Python `TaskManager` che gestisce una lista di "
        "task (aggiungi, rimuovi, segna come completato, elenca). "
        "Il progetto è piccolo e autonomo, senza dipendenze esterne.";

    /* Output accumulato da ogni fase */
    QString m_outputArchitetto;
    QString m_outputSviluppatore;
    QString m_outputRevisore;
    QString m_outputTester;

private slots:

    /* Salta l'intera categoria se Ollama non è raggiungibile */
    void initTestCase()
    {
        if (!modelAvailable(modelReasoning())) {
            QSKIP(qPrintable("CAT-K: modello reasoning '" +
                              modelReasoning() + "' non disponibile — skip"));
        }
        if (!modelAvailable(modelCoder())) {
            QSKIP(qPrintable("CAT-K: modello coder '" +
                              modelCoder() + "' non disponibile — skip"));
        }
    }

    /* ─── Fase 1: Architetto progetta il design ─────────────────── */
    void fase1_Architetto()
    {
        const QString sys =
            "Sei un architetto software esperto. "
            "Rispondi in italiano. "
            "Produci design concisi e strutturati.";

        const QString msg =
            "Progetta " + QString(kTaskDesc) + "\n\n"
            "Fornisci:\n"
            "1. Descrizione del design con le firme dei metodi pubblici (Python).\n"
            "2. Schema del data model interno (dict o list).\n"
            "Sii conciso: massimo 300 parole.";

        PhaseResult r = runPhase("Architetto", modelReasoning(),
                                  sys, msg, kTimeoutReasoningMs);

        qDebug() << "[Fase 1 Architetto]" << r.elapsed_ms << "ms,"
                 << r.tokenCount << "token";
        if (!r.ok) qWarning() << "Errore:" << r.errorMsg;

        QVERIFY2(r.ok, qPrintable("Architetto fallito: " + r.errorMsg));
        QVERIFY2(!r.response.isEmpty(), "Risposta architetto vuota");
        QVERIFY2(r.tokenCount > 0, "Nessun token ricevuto da architetto");

        m_outputArchitetto = r.response;
    }

    /* ─── Fase 2: Sviluppatore implementa il codice ─────────────── */
    void fase2_Sviluppatore()
    {
        if (m_outputArchitetto.isEmpty())
            QSKIP("Fase 1 non completata — skip Fase 2");

        const QString sys =
            "Sei uno sviluppatore Python esperto. "
            "Scrivi codice pulito, completo e funzionante. "
            "Rispondi SOLO con il codice Python, senza spiegazioni extra.";

        const QString msg =
            "Implementa " + QString(kTaskDesc) + "\n\n"
            "Segui questa architettura prodotta dall'architetto:\n"
            "──────────────────────────────\n" +
            m_outputArchitetto +
            "\n──────────────────────────────\n\n"
            "Scrivi il codice Python completo in un unico blocco.";

        PhaseResult r = runPhase("Sviluppatore", modelCoder(),
                                  sys, msg, kTimeoutCoderMs);

        qDebug() << "[Fase 2 Sviluppatore]" << r.elapsed_ms << "ms,"
                 << r.tokenCount << "token";
        if (!r.ok) qWarning() << "Errore:" << r.errorMsg;

        QVERIFY2(r.ok, qPrintable("Sviluppatore fallito: " + r.errorMsg));
        QVERIFY2(!r.response.isEmpty(), "Risposta sviluppatore vuota");
        QVERIFY2(r.tokenCount > 0, "Nessun token ricevuto da sviluppatore");

        m_outputSviluppatore = r.response;
    }

    /* ─── Fase 3: Revisore analizza il codice ───────────────────── */
    void fase3_Revisore()
    {
        if (m_outputSviluppatore.isEmpty())
            QSKIP("Fase 2 non completata — skip Fase 3");

        const QString sys =
            "Sei un senior software reviewer. "
            "Rispondi in italiano. "
            "Analizza il codice in modo critico e strutturato.";

        const QString msg =
            "Revisiona questo codice Python:\n"
            "──────────────────────────────\n" +
            m_outputSviluppatore +
            "\n──────────────────────────────\n\n"
            "Fornisci:\n"
            "1. Bug o problemi logici trovati (se presenti).\n"
            "2. Suggerimenti di miglioramento (max 3).\n"
            "3. Verdetto: APPROVATO o RICHIEDE MODIFICHE.\n"
            "Sii conciso: massimo 250 parole.";

        PhaseResult r = runPhase("Revisore", modelReasoning(),
                                  sys, msg, kTimeoutReasoningMs);

        qDebug() << "[Fase 3 Revisore]" << r.elapsed_ms << "ms,"
                 << r.tokenCount << "token";
        if (!r.ok) qWarning() << "Errore:" << r.errorMsg;

        QVERIFY2(r.ok, qPrintable("Revisore fallito: " + r.errorMsg));
        QVERIFY2(!r.response.isEmpty(), "Risposta revisore vuota");
        QVERIFY2(r.tokenCount > 0, "Nessun token ricevuto da revisore");

        m_outputRevisore = r.response;
    }

    /* ─── Fase 4: Tester scrive i test pytest ───────────────────── */
    void fase4_Tester()
    {
        if (m_outputSviluppatore.isEmpty())
            QSKIP("Fase 2 non completata — skip Fase 4");

        const QString sys =
            "Sei un esperto di testing Python con pytest. "
            "Scrivi test completi e funzionanti. "
            "Rispondi SOLO con il codice dei test, senza spiegazioni extra.";

        const QString msg =
            "Scrivi i test pytest per questo codice:\n"
            "──────────────────────────────\n" +
            m_outputSviluppatore +
            "\n──────────────────────────────\n\n" +
            (m_outputRevisore.isEmpty()
                 ? QString()
                 : "Tieni conto della seguente review:\n" +
                   m_outputRevisore + "\n\n") +
            "Scrivi test per: aggiunta, rimozione, completamento e listaggio task. "
            "Usa pytest (no unittest). Codice completo in un unico blocco.";

        PhaseResult r = runPhase("Tester", modelCoder(),
                                  sys, msg, kTimeoutCoderMs);

        qDebug() << "[Fase 4 Tester]" << r.elapsed_ms << "ms,"
                 << r.tokenCount << "token";
        if (!r.ok) qWarning() << "Errore:" << r.errorMsg;

        QVERIFY2(r.ok, qPrintable("Tester fallito: " + r.errorMsg));
        QVERIFY2(!r.response.isEmpty(), "Risposta tester vuota");
        QVERIFY2(r.tokenCount > 0, "Nessun token ricevuto da tester");

        m_outputTester = r.response;
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-L — Qualità output (RETE REALE, dipende da CAT-K)
   Verifica che i contenuti prodotti rispettino i minimi attesi:
   codice Python nella Fase 2, test pytest nella Fase 4.
   ══════════════════════════════════════════════════════════════ */
class TestTeamQuality : public QObject {
    Q_OBJECT

    /* Condivide i risultati con CAT-K attraverso variabili globali.
       Alternativa più pulita: eseguire le fasi anche qui (ridondante).
       Per semplicità il test assume che CAT-K venga eseguito prima. */

    QString m_outputSviluppatore;
    QString m_outputRevisore;
    QString m_outputTester;

private slots:

    void initTestCase()
    {
        if (!modelAvailable(modelReasoning()) || !modelAvailable(modelCoder())) {
            QSKIP("CAT-L: modelli non disponibili — skip");
        }

        /* Ri-esegue solo le fasi necessarie per la verifica qualità */
        const QString kTaskDesc =
            "una semplice classe Python `TaskManager` che gestisce una lista di "
            "task (aggiungi, rimuovi, segna come completato, elenca).";

        /* Fase architettura rapida per il coder */
        const QString archDesign =
            "class TaskManager:\n"
            "    def __init__(self): self.tasks = []\n"
            "    def add(self, title: str) -> dict\n"
            "    def remove(self, task_id: int) -> bool\n"
            "    def complete(self, task_id: int) -> bool\n"
            "    def list_all(self) -> list[dict]\n"
            "Data model: list di dict con chiavi: id, title, done";

        /* Fase 2 — Sviluppatore */
        PhaseResult devRes = runPhase(
            "Sviluppatore-L", modelCoder(),
            "Sei uno sviluppatore Python esperto. "
            "Scrivi codice pulito e completo. "
            "Rispondi SOLO con il codice Python.",
            "Implementa " + QString(kTaskDesc) +
            "\nArchitettura:\n" + archDesign +
            "\nCodice Python completo in un unico blocco.",
            kTimeoutCoderMs);

        if (!devRes.ok) {
            QSKIP(qPrintable("CAT-L: Sviluppatore fallito — " + devRes.errorMsg));
        }
        m_outputSviluppatore = devRes.response;

        /* Fase 3 — Revisore */
        PhaseResult revRes = runPhase(
            "Revisore-L", modelReasoning(),
            "Sei un senior reviewer. Analisi concisa in italiano.",
            "Revisiona:\n" + m_outputSviluppatore +
            "\nVerdetto: APPROVATO o RICHIEDE MODIFICHE (max 150 parole).",
            kTimeoutReasoningMs);

        m_outputRevisore = revRes.ok ? revRes.response : QString();

        /* Fase 4 — Tester */
        PhaseResult testRes = runPhase(
            "Tester-L", modelCoder(),
            "Sei un esperto pytest. Scrivi SOLO codice test.",
            "Test pytest per:\n" + m_outputSviluppatore +
            "\nUsa pytest, copri add/remove/complete/list.",
            kTimeoutCoderMs);

        if (!testRes.ok) {
            QSKIP(qPrintable("CAT-L: Tester fallito — " + testRes.errorMsg));
        }
        m_outputTester = testRes.response;
    }

    /* Fase 2 contiene definizione di una classe Python */
    void codiceContieneClasse()
    {
        QVERIFY2(m_outputSviluppatore.contains("class "),
                 "Output sviluppatore non contiene 'class'");
    }

    /* Fase 2 contiene almeno una definizione di metodo */
    void codiceContieneMetodi()
    {
        QVERIFY2(m_outputSviluppatore.contains("def "),
                 "Output sviluppatore non contiene definizioni di metodi");
    }

    /* Fase 2 fa riferimento a TaskManager */
    void codiceContieneTaskManager()
    {
        QVERIFY2(m_outputSviluppatore.contains("TaskManager"),
                 "Output sviluppatore non menziona 'TaskManager'");
    }

    /* Fase 3 contiene un verdetto (APPROVATO o RICHIEDE) */
    void reviewContieneVerdetto()
    {
        if (m_outputRevisore.isEmpty())
            QSKIP("Review non disponibile");

        const bool verdetto =
            m_outputRevisore.contains("APPROVATO", Qt::CaseInsensitive) ||
            m_outputRevisore.contains("RICHIEDE",  Qt::CaseInsensitive) ||
            m_outputRevisore.contains("approvato", Qt::CaseInsensitive) ||
            m_outputRevisore.contains("modifiche", Qt::CaseInsensitive);

        QVERIFY2(verdetto, "Review non contiene un verdetto chiaro");
    }

    /* Fase 4 contiene funzioni di test pytest (def test_) */
    void testContieneTestFunctions()
    {
        QVERIFY2(m_outputTester.contains("def test_"),
                 "Output tester non contiene funzioni pytest 'def test_'");
    }

    /* Fase 4 contiene import o from (codice Python valido) */
    void testContieneImport()
    {
        const bool hasImport =
            m_outputTester.contains("import ") ||
            m_outputTester.contains("from ");

        QVERIFY2(hasImport,
                 "Output tester non contiene alcuna import — potrebbe non essere codice Python");
    }

    /* Fase 4 almeno 2 funzioni di test (copertura minima) */
    void testCoperturaMinimaRaggiunta()
    {
        int count = 0;
        int idx   = 0;
        while ((idx = m_outputTester.indexOf("def test_", idx)) != -1) {
            ++count;
            ++idx;
        }
        QVERIFY2(count >= 2,
                 qPrintable(QString("Test insufficienti: %1 trovati, attesi >= 2")
                                .arg(count)));
    }

    /* Il codice sviluppatore è più lungo di 100 caratteri (non troncato) */
    void codiceNonTroncato()
    {
        QVERIFY2(m_outputSviluppatore.length() > 100,
                 "Output sviluppatore troppo corto — potrebbe essere troncato");
    }
};

/* ── Entry point ──────────────────────────────────────────────── */
int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    int failures = 0;
    failures += QTest::qExec(new TestTeamSanity,   argc, argv);
    failures += QTest::qExec(new TestTeamPipeline, argc, argv);
    failures += QTest::qExec(new TestTeamQuality,  argc, argv);

    return failures;
}

#include "test_team_collab.moc"
