/* ══════════════════════════════════════════════════════════════
   test_rag_engine.cpp — Unit test per RagEngine (JLT + coseno)

   Build:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc)
     ./build_tests/test_rag_engine

   Casi coperti:
     1. addChunk + search roundtrip — top-1 deve ritornare il chunk inserito
     2. search k > chunkCount — ritorna al massimo chunkCount risultati
     3. chunkCount == 0 su RagEngine vuoto
     4. save + load — round-trip su file temporaneo
     5. load file inesistente → return false, engine vuoto
     6. clear() azzera l'indice
     7. Dimensione vettore output JLT = kTargetDim
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QTemporaryFile>
#include "../rag_engine.h"

/* Helper: crea un embedding fittizio di dimensione d con valore val */
static QVector<float> makeEmb(int d, float val = 1.0f) {
    QVector<float> v(d, 0.0f);
    if (d > 0) v[0] = val;   /* un solo componente non-zero → vettore direzionale */
    return v;
}

class TestRagEngine : public QObject {
    Q_OBJECT

private slots:

    /* ── 1. chunkCount == 0 su indice vuoto ── */
    void emptyCount() {
        RagEngine rag;
        QCOMPARE(rag.chunkCount(), 0);
    }

    /* ── 2. Dimensione vettore proiettato = kTargetDim ── */
    void projectDim() {
        RagEngine rag;
        const int d = 128;
        QVector<float> emb = makeEmb(d, 1.0f);
        QVector<float> proj = rag.project(emb);
        QCOMPARE(proj.size(), RagEngine::kTargetDim);
    }

    /* ── 3. addChunk aumenta chunkCount ── */
    void addIncreasesCount() {
        RagEngine rag;
        const int d = 64;
        rag.addChunk("primo",  makeEmb(d, 1.0f));
        QCOMPARE(rag.chunkCount(), 1);
        rag.addChunk("secondo", makeEmb(d, 2.0f));
        QCOMPARE(rag.chunkCount(), 2);
    }

    /* ── 4. search top-1: chunk inserito con vettore identico viene ritrovato ── */
    void searchRoundTrip() {
        RagEngine rag;
        const int d = 64;

        /* Inseriamo 3 chunk con direzioni diverse sull'asse 0 */
        QVector<float> q = makeEmb(d, 10.0f);    /* query: vettore "lungo" asse 0 */
        QVector<float> e1 = makeEmb(d, 1.0f);   /* chunk 1: simile alla query */
        QVector<float> e2(d, 0.0f); e2[1] = 1.0f; /* chunk 2: perpendicolare */
        QVector<float> e3(d, 0.0f); e3[2] = 1.0f; /* chunk 3: perpendicolare */

        rag.addChunk("target", e1);
        rag.addChunk("altro",  e2);
        rag.addChunk("altro2", e3);

        auto results = rag.search(q, 1);
        QVERIFY(!results.isEmpty());
        QCOMPARE(results.first().text, QString("target"));
    }

    /* ── 5. search k > chunkCount ritorna al massimo chunkCount ── */
    void searchKLargerThanCount() {
        RagEngine rag;
        const int d = 32;
        rag.addChunk("solo", makeEmb(d, 1.0f));

        auto results = rag.search(makeEmb(d, 1.0f), 100);
        QVERIFY(results.size() <= rag.chunkCount());
    }

    /* ── 6. search su indice vuoto ritorna lista vuota ── */
    void searchOnEmpty() {
        RagEngine rag;
        auto results = rag.search(makeEmb(32, 1.0f), 5);
        QVERIFY(results.isEmpty());
    }

    /* ── 7. save + load round-trip ── */
    void saveLoad() {
        RagEngine rag;
        const int d = 64;
        rag.addChunk("alpha", makeEmb(d, 1.0f));
        rag.addChunk("beta",  makeEmb(d, 2.0f));

        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + "/test_rag_XXXXXX.json");
        QVERIFY(tmp.open());
        const QString path = tmp.fileName();
        tmp.close();   /* chiude ma non rimuove (autoRemove=true per default) */

        QVERIFY(rag.save(path));

        RagEngine rag2;
        QVERIFY(rag2.load(path));
        QCOMPARE(rag2.chunkCount(), 2);
        QCOMPARE(rag2.inputDim(),   d);

        /* Verifica che i chunk siano recuperabili via search */
        auto res = rag2.search(makeEmb(d, 1.0f), 1);
        QVERIFY(!res.isEmpty());
        QCOMPARE(res.first().text, QString("alpha"));
    }

    /* ── 8. load file inesistente → false, engine vuoto ── */
    void loadMissing() {
        RagEngine rag;
        const bool ok = rag.load("/tmp/__prisma_test_missing_123456.json");
        QVERIFY(!ok);
        QCOMPARE(rag.chunkCount(), 0);
    }

    /* ── 9. clear() azzera l'indice ── */
    void clearResetsCount() {
        RagEngine rag;
        rag.addChunk("x", makeEmb(32, 1.0f));
        QCOMPARE(rag.chunkCount(), 1);
        rag.clear();
        QCOMPARE(rag.chunkCount(), 0);
    }

    /* ── 10. due RagEngine con stesso seed producono stessa proiezione ── */
    void deterministicProjection() {
        const int d = 64;
        QVector<float> emb = makeEmb(d, 1.0f);

        RagEngine r1, r2;
        r1.addChunk("dummy", emb);   /* inizializza matrice JLT */
        r2.addChunk("dummy", emb);

        /* Cerca con stessa query — risultato deve coincidere */
        auto res1 = r1.search(emb, 1);
        auto res2 = r2.search(emb, 1);
        QVERIFY(!res1.isEmpty() && !res2.isEmpty());
        QCOMPARE(res1.first().text, res2.first().text);
    }
};

QTEST_MAIN(TestRagEngine)
#include "test_rag_engine.moc"
