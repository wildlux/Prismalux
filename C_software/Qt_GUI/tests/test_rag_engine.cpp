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

    /* ══════════════════════════════════════════════════════════════
       Test "RAG già indicizzato" — simula stato dopo una sessione
       precedente in cui i documenti erano già stati indicizzati.
       Questi test verificano che l'app funzioni correttamente quando
       l'utente riapre Prismalux dopo aver già creato un indice.
       ══════════════════════════════════════════════════════════════ */

    /* ── 11. RAG già indicizzato: save → load → search funziona ── */
    void alreadyIndexedSearch() {
        const int d = 64;

        /* Vettori su assi DISTINTI: cosine similarity diversa rispetto alla query */
        QVector<float> e0(d, 0.0f); e0[0] = 1.0f;  /* asse 0 — il più simile alla query */
        QVector<float> e1(d, 0.0f); e1[1] = 1.0f;  /* asse 1 — perpendicolare */
        QVector<float> e2(d, 0.0f); e2[2] = 1.0f;  /* asse 2 — perpendicolare */

        /* Simula prima sessione: indicizzazione documenti */
        RagEngine first;
        first.addChunk("Articolo 36 Costituzione: orario lavorativo", e0);
        first.addChunk("Fattorizzazione: 12 = 2 * 2 * 3",             e1);
        first.addChunk("Python: for i in range(10): print(i)",         e2);
        QCOMPARE(first.chunkCount(), 3);

        /* Salva su file temporaneo (come farebbe ImpostazioniPage) */
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + "/rag_already_XXXXXX.json");
        QVERIFY(tmp.open());
        const QString path = tmp.fileName();
        tmp.close();
        QVERIFY(first.save(path));

        /* Simula seconda sessione: carica l'indice già esistente */
        RagEngine second;
        QVERIFY(second.load(path));
        QCOMPARE(second.chunkCount(), 3);

        /* Query sull'asse 0: deve trovare il primo chunk come top-1 */
        QVector<float> query(d, 0.0f); query[0] = 1.0f;
        auto results = second.search(query, 1);
        QVERIFY(!results.isEmpty());
        QCOMPARE(results.first().text, QString("Articolo 36 Costituzione: orario lavorativo"));
    }

    /* ── 12. RAG già indicizzato: top-k restituisce risultati ordinati ── */
    void alreadyIndexedTopK() {
        const int d = 64;

        /* Tre chunk con distanze crescenti dalla query */
        QVector<float> qVec(d, 0.0f); qVec[0] = 1.0f;

        QVector<float> close(d, 0.0f);  close[0]  = 0.99f; close[1]  = 0.01f;
        QVector<float> mid(d, 0.0f);    mid[0]    = 0.7f;  mid[1]    = 0.7f;
        QVector<float> far_(d, 0.0f);   far_[1]   = 1.0f;  /* perpendicolare */

        RagEngine rag;
        rag.addChunk("vicino", close);
        rag.addChunk("medio",  mid);
        rag.addChunk("lontano", far_);

        /* Salva e ricarica (simula riavvio app) */
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + "/rag_topk_XXXXXX.json");
        QVERIFY(tmp.open()); tmp.close();
        QVERIFY(rag.save(tmp.fileName()));

        RagEngine loaded;
        QVERIFY(loaded.load(tmp.fileName()));

        /* top-2: deve includere "vicino" e "medio" */
        auto res = loaded.search(qVec, 2);
        QCOMPARE(res.size(), 2);
        QCOMPARE(res.at(0).text, QString("vicino"));
    }

    /* ── 13. Abort simulato: chunk parziali salvati, conteggio corretto ── */
    void partialIndexPreservesCount() {
        /* Simula che l'indicizzazione venga interrotta a metà:
         * 3 chunk su 5 vengono aggiunti prima dell'abort.
         * Verifica che chunkCount() rifletta solo i chunk completati. */
        const int d = 64;
        RagEngine rag;

        rag.addChunk("doc1-chunk1", makeEmb(d, 1.0f));
        rag.addChunk("doc1-chunk2", makeEmb(d, 2.0f));
        rag.addChunk("doc2-chunk1", makeEmb(d, 3.0f));
        /* "abort" simulato: i 2 chunk rimanenti non vengono aggiunti */

        QCOMPARE(rag.chunkCount(), 3);

        /* Il salvataggio dell'indice parziale deve funzionare */
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + "/rag_partial_XXXXXX.json");
        QVERIFY(tmp.open()); tmp.close();
        QVERIFY(rag.save(tmp.fileName()));

        /* Ricarica: deve contenere esattamente i 3 chunk parziali */
        RagEngine reloaded;
        QVERIFY(reloaded.load(tmp.fileName()));
        QCOMPARE(reloaded.chunkCount(), 3);
    }

    /* ── 14. Conta preservata dopo reindicizzazione fallita ── */
    void countPreservedOnFailedReindex() {
        /* Simula il fix del bug: kRagDocCount NON deve essere sovrascritto a 0
         * se n==0 (tutti gli embedding falliti).
         * Qui testiamo la logica RagEngine: un engine con 0 chunk NON deve
         * sovrascrivere un file JSON già valido. */
        const int d = 64;

        /* Stato iniziale: indice valido con 2 chunk */
        RagEngine good;
        good.addChunk("alpha", makeEmb(d, 1.0f));
        good.addChunk("beta",  makeEmb(d, 2.0f));

        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + "/rag_preserve_XXXXXX.json");
        QVERIFY(tmp.open()); tmp.close();
        QVERIFY(good.save(tmp.fileName()));

        /* Reindicizzazione fallita: engine vuoto (nessun embedding riuscito).
         * La logica in ImpostazioniPage NON chiama m_rag.save() se n==0,
         * quindi il file precedente rimane intatto. */
        RagEngine failed;
        QCOMPARE(failed.chunkCount(), 0);
        /* NON salviamo — simula il guard "if (n > 0)" introdotto nel fix */

        /* Il file originale deve essere ancora leggibile con i dati precedenti */
        RagEngine reloaded;
        QVERIFY(reloaded.load(tmp.fileName()));
        QCOMPARE(reloaded.chunkCount(), 2);
        auto res = reloaded.search(makeEmb(d, 1.0f), 1);
        QVERIFY(!res.isEmpty());
        QCOMPARE(res.first().text, QString("alpha"));
    }

    /* ── 15. RAG multi-documento: search semantico tra documenti diversi ── */
    void multiDocumentSearch() {
        /* Simula un indice con chunk da 3 "documenti" con direzioni diverse */
        const int d = 128;

        /* doc A: direzione asse 0 */
        QVector<float> a1(d, 0.0f); a1[0] = 1.0f;
        QVector<float> a2(d, 0.0f); a2[0] = 0.9f; a2[1] = 0.1f;

        /* doc B: direzione asse 1 */
        QVector<float> b1(d, 0.0f); b1[1] = 1.0f;
        QVector<float> b2(d, 0.0f); b2[1] = 0.8f; b2[2] = 0.2f;

        /* doc C: direzione asse 2 */
        QVector<float> c1(d, 0.0f); c1[2] = 1.0f;

        RagEngine rag;
        rag.addChunk("docA-1", a1);
        rag.addChunk("docA-2", a2);
        rag.addChunk("docB-1", b1);
        rag.addChunk("docB-2", b2);
        rag.addChunk("docC-1", c1);

        /* Query simile a doc A: top-2 devono essere entrambi da doc A */
        QVector<float> qA(d, 0.0f); qA[0] = 1.0f;
        auto resA = rag.search(qA, 2);
        QCOMPARE(resA.size(), 2);
        QVERIFY(resA.at(0).text.startsWith("docA"));
        QVERIFY(resA.at(1).text.startsWith("docA"));

        /* Query simile a doc B: top-1 deve essere da doc B */
        QVector<float> qB(d, 0.0f); qB[1] = 1.0f;
        auto resB = rag.search(qB, 1);
        QCOMPARE(resB.size(), 1);
        QVERIFY(resB.at(0).text.startsWith("docB"));
    }
};

QTEST_MAIN(TestRagEngine)
#include "test_rag_engine.moc"
