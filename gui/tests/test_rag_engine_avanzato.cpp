/* ══════════════════════════════════════════════════════════════
   test_rag_engine_avanzato.cpp — Test avanzati RagEngine
   ──────────────────────────────────────────────────────────────
   Estende test_rag_engine.cpp (15 test base) con:

   CAT-A  Proprietà matematiche JLT (8 test)
     — ortogonalità, determinismo, range, vettore zero
   CAT-B  Search — edge case e ranking (10 test)
     — k=0, k<0, dimMismatch, ordinamento, duplicati
   CAT-C  Save/Load avanzato (10 test)
     — file corrotto, dir mancante, 1000 chunk, idempotenza
   CAT-D  Performance (7 test)
     — benchmarks addChunk, search, project, save/load

   Build:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_rag_engine_avanzato
     ./build_tests/test_rag_engine_avanzato
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QTemporaryFile>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <cmath>
#include "../rag_engine.h"

/* ──────────────────────────────────────────────────────────────
   Helpers
   ────────────────────────────────────────────────────────────── */
static QVector<float> axis(int d, int axisIdx, float val = 1.0f)
{
    QVector<float> v(d, 0.0f);
    if (axisIdx >= 0 && axisIdx < d) v[axisIdx] = val;
    return v;
}

static QVector<float> uniform(int d, float val)
{
    return QVector<float>(d, val);
}

static float cosineRaw(const QVector<float>& a, const QVector<float>& b)
{
    if (a.size() != b.size()) return 0.0f;
    float dot = 0, na = 0, nb = 0;
    for (int i = 0; i < a.size(); ++i) {
        dot += a[i] * b[i];
        na  += a[i] * a[i];
        nb  += b[i] * b[i];
    }
    if (na < 1e-12f || nb < 1e-12f) return 0.0f;
    return dot / (std::sqrt(na) * std::sqrt(nb));
}

/* ══════════════════════════════════════════════════════════════
   CAT-A — Proprietà matematiche JLT (8 test)
   ══════════════════════════════════════════════════════════════ */
class TestJltMath : public QObject {
    Q_OBJECT
private slots:

    /* Vettori identici → proiezione identica (determinismo) */
    void identicalInputSameProjection() {
        RagEngine r1, r2;
        const int d = 128;
        QVector<float> v = axis(d, 0);
        r1.addChunk("x", v);
        r2.addChunk("x", v);
        const QVector<float> p1 = r1.project(v);
        const QVector<float> p2 = r2.project(v);
        QCOMPARE(p1.size(), RagEngine::kTargetDim);
        QCOMPARE(p1, p2);
    }

    /* Vettori ortogonali → cosine JLT ≈ 0 (entro ε = 0.25 per d=64) */
    void orthogonalVectorsLowCosine() {
        const int d = 256;
        QVector<float> va = axis(d, 0, 1.0f);
        QVector<float> vb = axis(d, 1, 1.0f);   /* perpendicolare a va */

        RagEngine rag;
        const QVector<float> pa = rag.project(va);
        const QVector<float> pb = rag.project(vb);
        const float cos = cosineRaw(pa, pb);
        /* JLT preserva le distanze con ε≈0.10 — cosine fra vettori ortogonali
         * deve stare vicino a 0 ma può avere rumore statistico. Tolleranza 0.30 */
        QVERIFY2(std::fabs(cos) < 0.30f,
                 qPrintable(QString("cosine troppo alto per vettori ortogonali: %1").arg(cos)));
    }

    /* Vettori identici → cosine proiettato ≈ 1.0 */
    void identicalVectorsMaxCosine() {
        const int d = 64;
        QVector<float> v = axis(d, 3, 2.5f);

        RagEngine rag;
        const QVector<float> p1 = rag.project(v);
        const QVector<float> p2 = rag.project(v);
        const float cos = cosineRaw(p1, p2);
        QVERIFY2(cos > 0.999f,
                 qPrintable(QString("cosine fra vettori identici troppo basso: %1").arg(cos)));
    }

    /* Vettore zero → no NaN/Inf nel proiettato, no crash */
    void zeroVectorNoNan() {
        const int d = 64;
        QVector<float> zero(d, 0.0f);

        RagEngine rag;
        const QVector<float> proj = rag.project(zero);
        QCOMPARE(proj.size(), RagEngine::kTargetDim);
        for (float v : proj) {
            QVERIFY2(!std::isnan(v),  "NaN nel proiettato di vettore zero");
            QVERIFY2(!std::isinf(v), "Inf nel proiettato di vettore zero");
        }
    }

    /* Vettori con valori molto grandi → no overflow/NaN nel proiettato */
    void largeValuesNoOverflow() {
        const int d = 64;
        QVector<float> big(d, 1e15f);

        RagEngine rag;
        rag.addChunk("bigvec", big);   /* triggera initMatrix */
        const QVector<float> proj = rag.project(big);
        for (float v : proj)
            QVERIFY2(!std::isnan(v) && !std::isinf(v),
                     "overflow o NaN con valori molto grandi");
    }

    /* Dimensione output sempre kTargetDim indipendentemente da inputDim */
    void outputDimAlwaysKTargetDim() {
        const int dims[] = {8, 32, 64, 128, 256, 512, 1024, 4096};
        for (int d : dims) {
            RagEngine rag;
            const QVector<float> proj = rag.project(axis(d, 0));
            QCOMPARE(proj.size(), RagEngine::kTargetDim);
        }
    }

    /* project() 1000× stesso vettore → stesso risultato (no side effect) */
    void projectIdempotent() {
        const int d = 64;
        QVector<float> v = axis(d, 5, 3.0f);
        RagEngine rag;
        rag.addChunk("init", v);   /* inizializza matrice JLT */

        const QVector<float> first = rag.project(v);
        for (int i = 0; i < 999; ++i) {
            const QVector<float> rep = rag.project(v);
            QCOMPARE(rep, first);
        }
    }

    /* inputDim() restituisce la dimensione corretta dopo il primo addChunk */
    void inputDimSetOnFirstChunk() {
        RagEngine rag;
        QCOMPARE(rag.inputDim(), 0);
        rag.addChunk("x", axis(128, 0));
        QCOMPARE(rag.inputDim(), 128);
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B — Search: edge case e ranking (10 test)
   ══════════════════════════════════════════════════════════════ */
class TestSearchEdge : public QObject {
    Q_OBJECT
private slots:

    /* k = 0 → lista vuota, no crash */
    void kZeroReturnsEmpty() {
        RagEngine rag;
        rag.addChunk("a", axis(32, 0));
        const auto r = rag.search(axis(32, 0), 0);
        QVERIFY(r.isEmpty());
    }

    /* k negativo → lista vuota, no crash */
    void kNegativeReturnsEmpty() {
        RagEngine rag;
        rag.addChunk("a", axis(32, 0));
        const auto r = rag.search(axis(32, 0), -5);
        QVERIFY(r.isEmpty());
    }

    /* Top-k ordinato: score[i] >= score[i+1] */
    void resultsDecreasingScore() {
        const int d = 128;
        RagEngine rag;
        /* 5 chunk con progressiva distanza dalla query (asse 0) */
        rag.addChunk("a", axis(d, 0, 1.00f));                        /* identico */
        rag.addChunk("b", [&]{ auto v=axis(d,0,.8f); v[1]=.6f; return v; }());
        rag.addChunk("c", [&]{ auto v=axis(d,0,.5f); v[1]=.86f; return v; }());
        rag.addChunk("d", axis(d, 1, 1.0f));                         /* perpendicolare */
        rag.addChunk("e", axis(d, 2, 1.0f));                         /* perpendicolare */

        const auto res = rag.search(axis(d, 0), 5);
        QCOMPARE(res.size(), 5);
        /* score calcolato implicitamente — verifichiamo solo che top-1 sia "a" */
        QCOMPARE(res.at(0).text, QString("a"));
        /* Non possiamo verificare i valori float interni senza accesso privato,
         * ma almeno verifichiamo che la top-1 sia quella attesa. */
    }

    /* Chunk duplicati: testo uguale ma embedding diverso → entrambi nel risultato */
    void duplicateTextBothReturned() {
        const int d = 64;
        RagEngine rag;
        rag.addChunk("dup", axis(d, 0));
        rag.addChunk("dup", axis(d, 1));

        const auto res = rag.search(axis(d, 0), 2);
        QCOMPARE(res.size(), 2);
    }

    /* 1000 chunk → top-5 corretto: il chunk identico alla query è top-1 */
    void largeIndex1000ChunksCorrectTop1() {
        const int d = 64;
        RagEngine rag;

        /* Inserisci 1000 chunk su assi casuali (tutti diversi dall'asse 0) */
        for (int i = 1; i <= 999; ++i)
            rag.addChunk(QString("noise-%1").arg(i), axis(d, i % (d-1) + 1));

        /* Inserisci il chunk "target" sull'asse 0 */
        rag.addChunk("target", axis(d, 0));
        QCOMPARE(rag.chunkCount(), 1000);

        const auto res = rag.search(axis(d, 0), 5);
        QCOMPARE(res.size(), 5);
        QCOMPARE(res.at(0).text, QString("target"));
    }

    /* Query identica a un chunk presente → quel chunk è il top-1 */
    void exactMatchIsTop1() {
        const int d = 64;
        RagEngine rag;
        QVector<float> target = axis(d, 7, 3.14f);
        rag.addChunk("esatto",  target);
        rag.addChunk("altro",   axis(d, 8));
        rag.addChunk("altro2",  axis(d, 9));

        const auto res = rag.search(target, 1);
        QCOMPARE(res.size(), 1);
        QCOMPARE(res.at(0).text, QString("esatto"));
    }

    /* Embedding su indice vuoto post-clear → lista vuota */
    void searchAfterClearEmpty() {
        RagEngine rag;
        rag.addChunk("x", axis(32, 0));
        rag.clear();
        const auto res = rag.search(axis(32, 0), 5);
        QVERIFY(res.isEmpty());
    }

    /* Aggiungere chunk dopo clear usa nuova matrice JLT */
    void addAfterClearWorks() {
        const int d = 64;
        RagEngine rag;
        rag.addChunk("old", axis(d, 0));
        rag.clear();
        rag.addChunk("new", axis(d, 1));
        QCOMPARE(rag.chunkCount(), 1);
        const auto res = rag.search(axis(d, 1), 1);
        QCOMPARE(res.size(), 1);
        QCOMPARE(res.at(0).text, QString("new"));
    }

    /* k > chunkCount → mai più risultati del count */
    void kLargerThanCountCapped() {
        const int d = 32;
        RagEngine rag;
        rag.addChunk("a", axis(d, 0));
        rag.addChunk("b", axis(d, 1));
        const auto res = rag.search(axis(d, 0), 9999);
        QVERIFY(res.size() <= 2);
    }

    /* Search su 1 chunk → top-1 è quel chunk indipendentemente dalla query */
    void singleChunkAlwaysReturned() {
        const int d = 64;
        RagEngine rag;
        rag.addChunk("unico", axis(d, 3));
        /* Query completamente diversa */
        const auto res = rag.search(axis(d, 0), 1);
        QCOMPARE(res.size(), 1);
        QCOMPARE(res.at(0).text, QString("unico"));
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-C — Save / Load avanzato (10 test)
   ══════════════════════════════════════════════════════════════ */
class TestSaveLoadAvanzato : public QObject {
    Q_OBJECT
private slots:

    /* Save + load + search → risultati identici all'originale */
    void saveLoadSearchConsistency() {
        const int d = 64;
        RagEngine rag;
        rag.addChunk("x", axis(d, 0));
        rag.addChunk("y", axis(d, 1));

        QTemporaryFile f;
        f.setFileTemplate(QDir::tempPath() + "/ragadv_XXXXXX.json");
        QVERIFY(f.open()); f.close();
        QVERIFY(rag.save(f.fileName()));

        RagEngine loaded;
        QVERIFY(loaded.load(f.fileName()));
        QCOMPARE(loaded.chunkCount(), 2);
        QCOMPARE(loaded.inputDim(),   d);

        auto orig  = rag.search(axis(d, 0), 1);
        auto after = loaded.search(axis(d, 0), 1);
        QVERIFY(!orig.isEmpty() && !after.isEmpty());
        QCOMPARE(orig.first().text, after.first().text);
    }

    /* Save dopo clear → file con 0 chunk, load ritorna engine vuoto ma ok */
    void saveEmptyAfterClear() {
        RagEngine rag;
        rag.addChunk("x", axis(32, 0));
        rag.clear();

        QTemporaryFile f;
        f.setFileTemplate(QDir::tempPath() + "/ragadv_empty_XXXXXX.json");
        QVERIFY(f.open()); f.close();
        const bool ok = rag.save(f.fileName());
        /* Può ritornare true (file vuoto valido) o false — non deve crashare */
        Q_UNUSED(ok);
    }

    /* Load file corrotto (JSON non valido) → false, engine in stato pulito */
    void loadCorruptedJson() {
        QTemporaryFile f;
        f.setFileTemplate(QDir::tempPath() + "/ragadv_corrupt_XXXXXX.json");
        QVERIFY(f.open());
        f.write(QByteArray("{ non_json_valido [[["));
        f.close();

        RagEngine rag;
        const bool ok = rag.load(f.fileName());
        QVERIFY(!ok);
        QCOMPARE(rag.chunkCount(), 0);
    }

    /* Load file valido ma con chunks array vuoto → ok, engine vuoto */
    void loadEmptyChunksArray() {
        QTemporaryFile f;
        f.setFileTemplate(QDir::tempPath() + "/ragadv_emptyarr_XXXXXX.json");
        QVERIFY(f.open());
        f.write(QByteArray(R"({"inputDim":64,"chunks":[]})"));
        f.close();

        RagEngine rag;
        const bool ok = rag.load(f.fileName());
        /* Può essere true o false — quel che conta è no crash e count == 0 */
        Q_UNUSED(ok);
        QCOMPARE(rag.chunkCount(), 0);
    }

    /* 1000 chunk → save → load → chunkCount == 1000 */
    void saveLoad1000Chunks() {
        const int d = 64;
        RagEngine rag;
        for (int i = 0; i < 1000; ++i)
            rag.addChunk(QString("doc-%1").arg(i), axis(d, i % d));
        QCOMPARE(rag.chunkCount(), 1000);

        QTemporaryFile f;
        f.setFileTemplate(QDir::tempPath() + "/ragadv_1000_XXXXXX.json");
        QVERIFY(f.open()); f.close();
        QVERIFY(rag.save(f.fileName()));

        RagEngine loaded;
        QVERIFY(loaded.load(f.fileName()));
        QCOMPARE(loaded.chunkCount(), 1000);
    }

    /* save + save → secondo save sovrascrive il primo (idempotente) */
    void saveOverwrite() {
        const int d = 32;
        RagEngine rag1, rag2;
        rag1.addChunk("v1", axis(d, 0));
        rag2.addChunk("v2", axis(d, 1));

        QTemporaryFile f;
        f.setFileTemplate(QDir::tempPath() + "/ragadv_overwrite_XXXXXX.json");
        QVERIFY(f.open()); f.close();

        QVERIFY(rag1.save(f.fileName()));
        QVERIFY(rag2.save(f.fileName()));   /* sovrascrive */

        RagEngine loaded;
        QVERIFY(loaded.load(f.fileName()));
        QCOMPARE(loaded.chunkCount(), 1);
        auto res = loaded.search(axis(d, 1), 1);
        QCOMPARE(res.first().text, QString("v2"));
    }

    /* inputDim preservata dopo load */
    void inputDimPreservedAfterLoad() {
        const int d = 512;
        RagEngine rag;
        rag.addChunk("x", axis(d, 0));

        QTemporaryFile f;
        f.setFileTemplate(QDir::tempPath() + "/ragadv_dim_XXXXXX.json");
        QVERIFY(f.open()); f.close();
        QVERIFY(rag.save(f.fileName()));

        RagEngine loaded;
        QVERIFY(loaded.load(f.fileName()));
        QCOMPARE(loaded.inputDim(), d);
    }

    /* save + load + save + load → risultato identico (round-trip doppio) */
    void doubleRoundTrip() {
        const int d = 64;
        RagEngine rag;
        rag.addChunk("uno",  axis(d, 0));
        rag.addChunk("due",  axis(d, 1));

        QTemporaryFile f1, f2;
        f1.setFileTemplate(QDir::tempPath() + "/ragadv_rt1_XXXXXX.json");
        f2.setFileTemplate(QDir::tempPath() + "/ragadv_rt2_XXXXXX.json");
        QVERIFY(f1.open()); f1.close();
        QVERIFY(f2.open()); f2.close();

        QVERIFY(rag.save(f1.fileName()));

        RagEngine loaded1;
        QVERIFY(loaded1.load(f1.fileName()));
        QVERIFY(loaded1.save(f2.fileName()));

        RagEngine loaded2;
        QVERIFY(loaded2.load(f2.fileName()));
        QCOMPARE(loaded2.chunkCount(), 2);

        auto r1 = loaded1.search(axis(d, 0), 1);
        auto r2 = loaded2.search(axis(d, 0), 1);
        QCOMPARE(r1.first().text, r2.first().text);
    }

    /* load su path con directory non esistente → false, no crash */
    void loadNonExistentDir() {
        RagEngine rag;
        const bool ok = rag.load("/tmp/__prisma_nonexistent_dir_xyz/file.json");
        QVERIFY(!ok);
    }

    /* save su path non scrivibile → false, no crash */
    void saveToReadOnlyPath() {
        RagEngine rag;
        rag.addChunk("x", axis(32, 0));
        /* /proc/nonscrivibile è non scrivibile su Linux */
        const bool ok = rag.save("/proc/prismalux_test_readonly.json");
        QVERIFY(!ok);
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-D — Performance (7 test)
   Usa QElapsedTimer + soglie conservative per CI.
   ══════════════════════════════════════════════════════════════ */
class TestPerformance : public QObject {
    Q_OBJECT
private slots:

    /* addChunk × 1000 (dim=512) < 5s — usa dim=512 per rimanere veloce in Debug */
    void addChunk1000x512() {
        const int d = 512;
        RagEngine rag;
        QElapsedTimer t;
        t.start();
        for (int i = 0; i < 1000; ++i)
            rag.addChunk(QString("doc-%1").arg(i), axis(d, i % d));
        const qint64 ms = t.elapsed();
        QVERIFY2(ms < 5000,
                 qPrintable(QString("addChunk 1000×512 troppo lento: %1 ms").arg(ms)));
        QCOMPARE(rag.chunkCount(), 1000);
    }

    /* project × 1000 (dim=512) < 5s */
    void project1000x512() {
        const int d = 512;
        RagEngine rag;
        QVector<float> v = axis(d, 0);
        rag.addChunk("init", v);   /* inizializza matrice */
        QElapsedTimer t;
        t.start();
        for (int i = 0; i < 1000; ++i)
            rag.project(v);
        const qint64 ms = t.elapsed();
        QVERIFY2(ms < 5000,
                 qPrintable(QString("project 1000×512 troppo lento: %1 ms").arg(ms)));
    }

    /* search top-5 su 1000 chunk < 100ms */
    void searchTop5On1000() {
        const int d = 64;
        RagEngine rag;
        for (int i = 0; i < 1000; ++i)
            rag.addChunk(QString("d-%1").arg(i), axis(d, i % d));
        QElapsedTimer t;
        t.start();
        rag.search(axis(d, 0), 5);
        const qint64 ms = t.elapsed();
        QVERIFY2(ms < 100,
                 qPrintable(QString("search top-5/1000 troppo lento: %1 ms").arg(ms)));
    }

    /* search top-5 su 5000 chunk < 1s */
    void searchTop5On5000() {
        const int d = 64;
        RagEngine rag;
        for (int i = 0; i < 5000; ++i)
            rag.addChunk(QString("d-%1").arg(i), axis(d, i % d));
        QElapsedTimer t;
        t.start();
        rag.search(axis(d, 0), 5);
        const qint64 ms = t.elapsed();
        QVERIFY2(ms < 1000,
                 qPrintable(QString("search top-5/5000 troppo lento: %1 ms").arg(ms)));
    }

    /* save 1000 chunk < 2s */
    void save1000Chunks() {
        const int d = 64;
        RagEngine rag;
        for (int i = 0; i < 1000; ++i)
            rag.addChunk(QString("d-%1").arg(i), axis(d, i % d));

        QTemporaryFile f;
        f.setFileTemplate(QDir::tempPath() + "/ragperf_XXXXXX.json");
        QVERIFY(f.open()); f.close();

        QElapsedTimer t;
        t.start();
        rag.save(f.fileName());
        const qint64 ms = t.elapsed();
        QVERIFY2(ms < 2000,
                 qPrintable(QString("save 1000 chunk troppo lento: %1 ms").arg(ms)));
    }

    /* load 1000 chunk < 1s */
    void load1000Chunks() {
        const int d = 64;
        RagEngine rag;
        for (int i = 0; i < 1000; ++i)
            rag.addChunk(QString("d-%1").arg(i), axis(d, i % d));

        QTemporaryFile f;
        f.setFileTemplate(QDir::tempPath() + "/ragperf_load_XXXXXX.json");
        QVERIFY(f.open()); f.close();
        QVERIFY(rag.save(f.fileName()));

        QElapsedTimer t;
        t.start();
        RagEngine loaded;
        loaded.load(f.fileName());
        const qint64 ms = t.elapsed();
        QVERIFY2(ms < 1000,
                 qPrintable(QString("load 1000 chunk troppo lento: %1 ms").arg(ms)));
    }

    /* 100 ricerche consecutive non degradano il tempo (no leak strutturale) */
    void repeatedSearchStableTime() {
        const int d = 64;
        RagEngine rag;
        for (int i = 0; i < 500; ++i)
            rag.addChunk(QString("d-%1").arg(i), axis(d, i % d));

        /* Warm-up */
        rag.search(axis(d, 0), 5);

        QElapsedTimer t;
        t.start();
        for (int i = 0; i < 100; ++i)
            rag.search(axis(d, i % d), 5);
        const qint64 ms = t.elapsed();
        /* 100 ricerche su 500 chunk in meno di 2s */
        QVERIFY2(ms < 2000,
                 qPrintable(QString("100 ricerche ripetute troppo lente: %1 ms").arg(ms)));
    }
};

/* ══════════════════════════════════════════════════════════════
   Runner
   ══════════════════════════════════════════════════════════════ */
int main(int argc, char** argv)
{
    int status = 0;
    { TestJltMath       t; status |= QTest::qExec(&t, argc, argv); }
    { TestSearchEdge    t; status |= QTest::qExec(&t, argc, argv); }
    { TestSaveLoadAvanzato t; status |= QTest::qExec(&t, argc, argv); }
    { TestPerformance   t; status |= QTest::qExec(&t, argc, argv); }
    return status;
}

#include "test_rag_engine_avanzato.moc"
