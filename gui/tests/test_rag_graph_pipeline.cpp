/* ══════════════════════════════════════════════════════════════
   test_rag_graph_pipeline.cpp
   ──────────────────────────────────────────────────────────────
   Suite di test per la pipeline RagGraph (rag_graph.h/cpp).
   Testa il flusso completo: file → LLM (mock) → parseAndStore → GraphMemory.

   Categorie:
     A  — Costruzione e configurazione
     B  — Parsing JSON LLM via pipeline completa (MockAiClient + emitFinished)
     C  — Ricerca (searchNodes, searchChunks)
     D  — Segnali (progressUpdated, finished, stopIngest)

   Esecuzione:
     cmake -B build_tests gui/ -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_rag_graph_pipeline
     ./build_tests/test_rag_graph_pipeline
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QApplication>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QSignalSpy>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>

#include "mock_ai_client.h"
#include "../rag_graph.h"
#include "../graph_memory.h"

/* ── JSON LLM valido usato in più test ───────────────────────── */
static const QString kValidJson = R"({
  "entities": [
    {"label":"Qt6","type":"framework","importance":0.9,"description":"Framework C++"},
    {"label":"C++","type":"tecnologia","importance":0.8,"description":"Linguaggio"}
  ],
  "relations": [
    {"from":"Qt6","to":"C++","type":"basato_su","weight":0.9}
  ]
})";

/* ── Helper: crea un file .txt temporaneo con testo ─────────── */
static QString makeTxtFile(const QTemporaryDir& dir, const QString& name,
                            const QString& content = "Qt6 e C++ sono collegati.")
{
    const QString path = dir.filePath(name);
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream ts(&f);
        ts << content;
    }
    return path;
}

/* ── Helper: apre un GraphMemory isolato ────────────────────── */
static GraphMemory* makeGm(const QTemporaryDir& dir, const QString& name = "rag.db")
{
    auto* gm = new GraphMemory(dir.filePath(name));
    const bool ok = gm->open();
    Q_ASSERT(ok);
    return gm;
}

/* ══════════════════════════════════════════════════════════════
   A — Costruzione e configurazione
   ══════════════════════════════════════════════════════════════ */
class TestCatA : public QObject {
    Q_OBJECT
private slots:

    /* A-1: costruito → running=false, totalFiles=0 */
    void a1_costruzione() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        MockAiClient ai;
        RagGraph rg(&ai, gm);
        QVERIFY(!rg.isRunning());
        QCOMPARE(rg.stats().totalFiles, 0);
        delete gm;
    }

    /* A-2: addFile() → totalFiles == 1 */
    void a2_addFile() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        MockAiClient ai;
        RagGraph rg(&ai, gm);

        const QString path = makeTxtFile(d, "doc.txt");
        rg.addFile(path);
        QCOMPARE(rg.stats().totalFiles, 1);
        delete gm;
    }

    /* A-3: addDirectory() su dir inesistente → totalFiles=0, nessun crash */
    void a3_addDirectoryInesistente() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        MockAiClient ai;
        RagGraph rg(&ai, gm);

        rg.addDirectory("/percorso/che/non/esiste_xyz_12345");
        QCOMPARE(rg.stats().totalFiles, 0);
        delete gm;
    }

    /* A-4: addDirectory() su dir con file .txt/.md → totalFiles > 0 */
    void a4_addDirectoryConFile() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        MockAiClient ai;
        RagGraph rg(&ai, gm);

        /* Crea una sotto-cartella con file .txt e .md */
        QTemporaryDir docDir;
        makeTxtFile(docDir, "a.txt", "contenuto A");
        makeTxtFile(docDir, "b.md",  "contenuto B");

        rg.addDirectory(docDir.path());
        QVERIFY(rg.stats().totalFiles > 0);
        delete gm;
    }

    /* A-5: setChunkSize/setMaxChunksPerFile/setMaxEntitiesPerChunk non crashano */
    void a5_settersNoCrash() {
        QTemporaryDir d;
        auto* gm = makeGm(d);
        MockAiClient ai;
        RagGraph rg(&ai, gm);

        rg.setChunkSize(300);
        rg.setMaxChunksPerFile(4);
        rg.setMaxEntitiesPerChunk(6);
        QVERIFY(true);   /* se siamo qui non c'e' crash */
        delete gm;
    }
};

/* ══════════════════════════════════════════════════════════════
   B — Parsing JSON LLM via pipeline completa
   ══════════════════════════════════════════════════════════════ */
class TestCatB : public QObject {
    Q_OBJECT

    /* Helper: crea RagGraph, carica 1 file, avvia ingest, emette risposta LLM.
     * Restituisce il numero di nodi in GraphMemory al termine. */
    struct Env {
        QTemporaryDir   dir;
        GraphMemory*    gm  = nullptr;
        MockAiClient*   ai  = nullptr;
        RagGraph*       rg  = nullptr;

        Env() {
            gm = makeGm(dir, "b.db");
            ai = new MockAiClient();
            rg = new RagGraph(ai, gm);
        }
        ~Env() {
            delete rg;
            delete ai;
            delete gm;
        }
    };

private slots:

    /* B-1: JSON valido → 2 entità + 1 documento = 3 nodi totali in GraphMemory */
    void b1_jsonValido_2entita() {
        Env e;
        const QString path = makeTxtFile(e.dir, "doc.txt", "Qt6 e C++");
        e.rg->addFile(path);

        QSignalSpy spy(e.rg, &RagGraph::finished);
        e.rg->startIngest();
        /* Simula risposta LLM */
        e.ai->emitFinished(kValidJson);

        /* Aspetta finished() */
        if (spy.isEmpty()) spy.wait(1000);
        QVERIFY(!spy.isEmpty());

        /* 1 documento RAG + 2 entita' */
        QCOMPARE(e.gm->nodeCount(), 3);
    }

    /* B-2: JSON valido → 1 arco relazione basato_su */
    void b2_jsonValido_1arco() {
        Env e;
        const QString path = makeTxtFile(e.dir, "doc.txt");
        e.rg->addFile(path);

        QSignalSpy spy(e.rg, &RagGraph::finished);
        e.rg->startIngest();
        e.ai->emitFinished(kValidJson);
        if (spy.isEmpty()) spy.wait(1000);
        QVERIFY(!spy.isEmpty());

        /* arco "basato_su" + 2 archi "contiene" (doc→Qt6, doc→C++) = 3 totali */
        QVERIFY(e.gm->edgeCount() >= 1);
        /* Verifica specificamente che almeno un arco "basato_su" esista */
        const auto edges = e.gm->allEdges();
        bool hasBased = false;
        for (const auto& edge : edges) {
            if (edge.relation == "basato_su") { hasBased = true; break; }
        }
        QVERIFY(hasBased);
    }

    /* B-3: <think>...</think> strippati prima del parse */
    void b3_thinkStrippato() {
        Env e;
        const QString path = makeTxtFile(e.dir, "doc.txt");
        e.rg->addFile(path);

        QSignalSpy spy(e.rg, &RagGraph::finished);
        e.rg->startIngest();

        /* Il codice reale usa regex \{[\s\S]*\} che isola il JSON anche
         * se c'e' del testo prima o dopo — i <think> non cambiano nulla.
         * Emettiamo il JSON preceduto da un blocco <think> */
        const QString withThink = "<think>Analizzo il testo...</think>\n" + kValidJson;
        e.ai->emitFinished(withThink);
        if (spy.isEmpty()) spy.wait(1000);
        QVERIFY(!spy.isEmpty());

        /* Entita' devono essere state estratte correttamente */
        const auto nodes = e.gm->searchNodes("Qt6", 10);
        QVERIFY(!nodes.isEmpty());
    }

    /* B-4: JSON con code fence ```json...``` — deve essere parsato lo stesso */
    void b4_codeFence() {
        Env e;
        const QString path = makeTxtFile(e.dir, "doc.txt");
        e.rg->addFile(path);

        QSignalSpy spy(e.rg, &RagGraph::finished);
        e.rg->startIngest();

        const QString withFence = "```json\n" + kValidJson + "\n```";
        e.ai->emitFinished(withFence);
        if (spy.isEmpty()) spy.wait(1000);
        QVERIFY(!spy.isEmpty());

        const auto nodes = e.gm->searchNodes("Qt6", 10);
        QVERIFY(!nodes.isEmpty());
    }

    /* B-5: JSON malformato → fileError emesso */
    void b5_jsonMalformato() {
        Env e;
        const QString path = makeTxtFile(e.dir, "doc.txt");
        e.rg->addFile(path);

        QSignalSpy spyErr(e.rg,    &RagGraph::fileError);
        QSignalSpy spyFin(e.rg,    &RagGraph::finished);

        e.rg->startIngest();
        e.ai->emitFinished("questa non e' JSON valida {{{");
        if (spyFin.isEmpty()) spyFin.wait(1000);
        QVERIFY(!spyFin.isEmpty());
        QVERIFY(!spyErr.isEmpty());
    }

    /* B-6: entita' gia' esistente → updateNode (importance +0.1), nodeCount invariato */
    void b6_entitaEsistente_updateNode() {
        Env e;
        /* Pre-inserisci Qt6 con importance 0.5 */
        const QString existingId = e.gm->addNode("framework", "Qt6", "gia esistente", 0.5f);
        QVERIFY(!existingId.isEmpty());

        const int nodesBefore = e.gm->nodeCount();   /* 1 */

        const QString path = makeTxtFile(e.dir, "doc.txt");
        e.rg->addFile(path);

        QSignalSpy spy(e.rg, &RagGraph::finished);
        e.rg->startIngest();
        e.ai->emitFinished(kValidJson);
        if (spy.isEmpty()) spy.wait(1000);
        QVERIFY(!spy.isEmpty());

        /* Qt6 gia' esiste → updateNode, NON addNode.
         * Nodi attesi: 1 (Qt6 preesistente) + 1 documento + 1 C++ (nuovo) = 3 */
        const int nodesAfter = e.gm->nodeCount();
        QCOMPARE(nodesAfter, nodesBefore + 2);  /* doc + C++ */

        /* Importance di Qt6 deve essere aumentata */
        const auto opt = e.gm->nodeByLabel("Qt6");
        QVERIFY(opt.has_value());
        QVERIFY(opt->importance > 0.5f);
    }
};

/* ══════════════════════════════════════════════════════════════
   C — Ricerca
   ══════════════════════════════════════════════════════════════ */
class TestCatC : public QObject {
    Q_OBJECT
private slots:

    /* C-1: searchNodes("Qt6") dopo ingest → almeno 1 nodo trovato */
    void c1_searchNodesHit() {
        QTemporaryDir d;
        auto* gm = makeGm(d, "c1.db");
        MockAiClient ai;
        RagGraph rg(&ai, gm);

        const QString path = makeTxtFile(d, "doc.txt");
        rg.addFile(path);

        QSignalSpy spy(&rg, &RagGraph::finished);
        rg.startIngest();
        ai.emitFinished(kValidJson);
        if (spy.isEmpty()) spy.wait(1000);
        QVERIFY(!spy.isEmpty());

        const auto nodes = rg.searchNodes("Qt6", 20);
        QVERIFY(!nodes.isEmpty());

        delete gm;
    }

    /* C-2: searchNodes("inesistente_xyz") → lista vuota */
    void c2_searchNodesMiss() {
        QTemporaryDir d;
        auto* gm = makeGm(d, "c2.db");
        MockAiClient ai;
        RagGraph rg(&ai, gm);

        const QString path = makeTxtFile(d, "doc.txt");
        rg.addFile(path);

        QSignalSpy spy(&rg, &RagGraph::finished);
        rg.startIngest();
        ai.emitFinished(kValidJson);
        if (spy.isEmpty()) spy.wait(1000);
        QVERIFY(!spy.isEmpty());

        const auto nodes = rg.searchNodes("inesistente_xyz_99999", 20);
        QVERIFY(nodes.isEmpty());

        delete gm;
    }

    /* C-3: searchChunks("Qt6") → chunk non vuoto */
    void c3_searchChunks() {
        QTemporaryDir d;
        auto* gm = makeGm(d, "c3.db");
        MockAiClient ai;
        RagGraph rg(&ai, gm);

        const QString path = makeTxtFile(d, "doc.txt");
        rg.addFile(path);

        QSignalSpy spy(&rg, &RagGraph::finished);
        rg.startIngest();
        ai.emitFinished(kValidJson);
        if (spy.isEmpty()) spy.wait(1000);
        QVERIFY(!spy.isEmpty());

        const auto chunks = rg.searchChunks("Qt6", 10);
        /* searchChunks() crea chunk sintetici dal nodo quando non c'e' un chunk
         * mappa esplicito: deve comunque restituire almeno un elemento */
        QVERIFY(!chunks.isEmpty());
        /* Il testo del chunk non e' vuoto */
        QVERIFY(!chunks.first().text.isEmpty());

        delete gm;
    }
};

/* ══════════════════════════════════════════════════════════════
   D — Segnali
   ══════════════════════════════════════════════════════════════ */
class TestCatD : public QObject {
    Q_OBJECT
private slots:

    /* D-1: progressUpdated emesso durante startIngest */
    void d1_progressUpdated() {
        QTemporaryDir d;
        auto* gm = makeGm(d, "d1.db");
        MockAiClient ai;
        RagGraph rg(&ai, gm);

        const QString path = makeTxtFile(d, "doc.txt");
        rg.addFile(path);

        QSignalSpy spyProg(&rg, &RagGraph::progressUpdated);
        QSignalSpy spyFin (&rg, &RagGraph::finished);

        rg.startIngest();
        /* progressUpdated deve essere emesso immediatamente (sincrono) */
        QVERIFY(!spyProg.isEmpty());

        /* Completa l'ingest per lasciare il sistema pulito */
        ai.emitFinished(kValidJson);
        if (spyFin.isEmpty()) spyFin.wait(500);

        delete gm;
    }

    /* D-2: finished emesso al termine con processedFiles == 1 */
    void d2_finishedStats() {
        QTemporaryDir d;
        auto* gm = makeGm(d, "d2.db");
        MockAiClient ai;
        RagGraph rg(&ai, gm);

        const QString path = makeTxtFile(d, "doc.txt");
        rg.addFile(path);

        QSignalSpy spy(&rg, &RagGraph::finished);
        rg.startIngest();
        ai.emitFinished(kValidJson);
        if (spy.isEmpty()) spy.wait(1000);

        QVERIFY(!spy.isEmpty());
        /* Dopo finished(), le stats sono accessibili tramite rg.stats() */
        const RagGraphStats& stats = rg.stats();
        QCOMPARE(stats.processedFiles, 1);
        QVERIFY(!stats.running);

        delete gm;
    }

    /* D-3: stopIngest() → isRunning() == false */
    void d3_stopIngest() {
        QTemporaryDir d;
        auto* gm = makeGm(d, "d3.db");
        MockAiClient ai;
        RagGraph rg(&ai, gm);

        const QString path = makeTxtFile(d, "doc.txt");
        rg.addFile(path);

        rg.startIngest();
        QVERIFY(rg.isRunning());

        rg.stopIngest();
        QVERIFY(!rg.isRunning());

        delete gm;
    }
};

/* ══════════════════════════════════════════════════════════════
   main
   ══════════════════════════════════════════════════════════════ */
int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    int r = 0;
    { TestCatA t; r |= QTest::qExec(&t, argc, argv); }
    { TestCatB t; r |= QTest::qExec(&t, argc, argv); }
    { TestCatC t; r |= QTest::qExec(&t, argc, argv); }
    { TestCatD t; r |= QTest::qExec(&t, argc, argv); }
    return r;
}

#include "test_rag_graph_pipeline.moc"
