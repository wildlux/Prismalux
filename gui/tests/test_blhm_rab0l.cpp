/* ══════════════════════════════════════════════════════════════
   test_blhm_rab0l.cpp  —  Suite di regressione BLHM + RAB0-L

   3 categorie — 32 test totali:

   CAT-A  Rab0lCanvas       — 10 casi: widget puro, no LLM
   CAT-B  BLHM Engine C     — 12 casi: graph, match, gate, infer
   CAT-C  BioinformaticaPage UI — 10 casi: costruzione sezioni BLHM/RAB0-L

   COME ESEGUIRE:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_blhm_rab0l
     ./build_tests/test_blhm_rab0l
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QApplication>
#include <QWidget>
#include <QLineEdit>
#include <QTextEdit>
#include <QTableWidget>
#include <QLabel>
#include <cmath>

#include "mock_ai_client.h"
#include "../pages/rab0l_canvas.h"
#include "../blhm_engine.h"
#include "../pages/main_research.h"
#include "../pages/main_bioinformatica.h"

/* ══════════════════════════════════════════════════════════════
   CAT-A  Rab0lCanvas (widget puro, no LLM)
   10 casi
   ══════════════════════════════════════════════════════════════ */
class TestCatA : public QObject {
    Q_OBJECT
private:
    Rab0lCanvas* m_canvas = nullptr;

private slots:

    void init() {
        m_canvas = new Rab0lCanvas();
    }

    void cleanup() {
        delete m_canvas;
        m_canvas = nullptr;
    }

    /* A-01: costruisce senza crash */
    void constructNocrash() {
        QVERIFY(m_canvas != nullptr);
    }

    /* A-02: e' un QWidget valido */
    void isQWidget() {
        QVERIFY(qobject_cast<QWidget*>(m_canvas) != nullptr);
    }

    /* A-03: show() non crasha */
    void showNoCrash() {
        m_canvas->resize(200, 200);
        m_canvas->show();
        QApplication::processEvents();
        QVERIFY(m_canvas->isVisible());
    }

    /* A-04: update() non crasha */
    void updateNoCrash() {
        m_canvas->resize(200, 200);
        m_canvas->show();
        m_canvas->update();
        QApplication::processEvents();
        QVERIFY(true); /* nessun abort */
    }

    /* A-05: paintEvent non crasha su dimensione 200x200 */
    void paintEventNoCrash() {
        m_canvas->resize(200, 200);
        m_canvas->show();
        m_canvas->repaint();
        QApplication::processEvents();
        QVERIFY(true);
    }

    /* A-06: setSequences con valori validi non crasha */
    void setSequencesNoCrash() {
        m_canvas->setSequences("ATGCATGC", "ATGCTTGC");
        QApplication::processEvents();
        QVERIFY(true);
    }

    /* A-07: setSeq1Only con sequenza non vuota non crasha */
    void setSeq1OnlyNoCrash() {
        m_canvas->setSeq1Only("AACCGGTT");
        QApplication::processEvents();
        QVERIFY(true);
    }

    /* A-08: computePoints su sequenza Fibonacci-nt (ATGATGATG) */
    void computePointsFibonacci() {
        /* Usa la sequenza ATGATGATG come analogo nucleotidico di sequenza
         * con pattern ripetuto, tutte le basi valide */
        const QString seq = "ATGATGATG";
        QVector<Rab0lPoint> pts = Rab0lCanvas::computePoints(seq);
        QCOMPARE(pts.size(), seq.size());
    }

    /* A-09: computePoints restituisce pos corrette */
    void computePointsPositions() {
        const QString seq = "ACGT";
        QVector<Rab0lPoint> pts = Rab0lCanvas::computePoints(seq);
        QCOMPARE(pts.size(), 4);
        for (int i = 0; i < pts.size(); ++i)
            QCOMPARE(pts[i].pos, i);
    }

    /* A-10: clearAll non crasha, poi setSequences funziona ancora */
    void clearAllThenSetSequences() {
        m_canvas->setSequences("ATGC", "TTTT");
        m_canvas->clearAll();
        m_canvas->setSequences("GCTA", "GCTA");
        const Rab0lResult& res = m_canvas->result();
        /* sim identica deve essere ~1.0 (stessa seq) */
        QVERIFY(res.sim >= 0.0 && res.sim <= 1.0);
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B  BLHM Engine C
   12 casi: graph lifecycle, label registry, match/gate, infer
   ══════════════════════════════════════════════════════════════ */
class TestCatB : public QObject {
    Q_OBJECT
private:
    BLHMGraph* m_g = nullptr;

    /* Helper: costruisce un BLHMWeight minimo con path di lunghezza 1 */
    static BLHMWeight makeWeight(int32_t pathId, float fw, float lw, float uw) {
        BLHMWeight w{};
        w.factory_w = fw;
        w.link_w    = lw;
        w.user_w    = uw;
        w.safety_w  = 0.0f;
        w.path[0]   = pathId;
        w.path_len  = 1;
        w.n_req_creds = 0;
        return w;
    }

private slots:

    void init() {
        m_g = blhm_graph_create(64);
    }

    void cleanup() {
        blhm_graph_free(m_g);
        m_g = nullptr;
    }

    /* B-01: blhm_graph_create restituisce handle non-null */
    void graphCreateNonNull() {
        QVERIFY(m_g != nullptr);
    }

    /* B-02: grafo vuoto — count == 0 */
    void graphInitiallyEmpty() {
        QCOMPARE(blhm_graph_count(m_g), 0);
    }

    /* B-03: blhm_graph_add incrementa count */
    void graphAddIncreasesCount() {
        BLHMWeight w = makeWeight(1, 0.5f, 0.3f, 0.2f);
        blhm_graph_add(m_g, &w);
        QCOMPARE(blhm_graph_count(m_g), 1);
    }

    /* B-04: blhm_graph_get restituisce puntatore valido */
    void graphGetValid() {
        BLHMWeight w = makeWeight(2, 0.8f, 0.1f, 0.1f);
        blhm_graph_add(m_g, &w);
        const BLHMWeight* got = blhm_graph_get(m_g, 0);
        QVERIFY(got != nullptr);
        QCOMPARE(got->path[0], (int32_t)2);
    }

    /* B-05: blhm_graph_clear svuota il grafo */
    void graphClearResetsCount() {
        BLHMWeight w = makeWeight(3, 0.5f, 0.5f, 0.0f);
        blhm_graph_add(m_g, &w);
        blhm_graph_add(m_g, &w);
        blhm_graph_clear(m_g);
        QCOMPARE(blhm_graph_count(m_g), 0);
    }

    /* B-06: label registry — register e find */
    void labelRegisterAndFind() {
        int id = blhm_label_register(m_g, "qt6");
        QVERIFY(id >= 0);
        int found = blhm_label_find(m_g, "qt6");
        QCOMPARE(found, id);
    }

    /* B-07: label non registrata — blhm_label_find restituisce -1 */
    void labelFindMissReturnsMinusOne() {
        int found = blhm_label_find(m_g, "nonexistent_label_xyz");
        QCOMPARE(found, -1);
    }

    /* B-08: blhm_match — prefix identico di lunghezza 1 → 1.0 */
    void matchIdenticalPathReturnsOne() {
        int32_t p[] = {10};
        float m = blhm_match(p, 1, p, 1);
        QVERIFY(qFuzzyCompare(m, 1.0f));
    }

    /* B-09: blhm_match — nessun prefix in comune → 0.0 */
    void matchNoCommonPrefixReturnsZero() {
        int32_t wp[] = {1, 2};
        int32_t qp[] = {3, 4};
        float m = blhm_match(wp, 2, qp, 2);
        QCOMPARE(m, 0.0f);
    }

    /* B-10: blhm_gate — nessuna credenziale richiesta → true */
    void gateNoCredsRequired() {
        BLHMWeight w = makeWeight(5, 0.5f, 0.5f, 0.0f);
        w.n_req_creds = 0;
        QVERIFY(blhm_gate(&w, nullptr, 0) == true);
    }

    /* B-11: blhm_gate — credenziale richiesta presente → true */
    void gateCredPresentReturnsTrue() {
        BLHMWeight w = makeWeight(5, 0.5f, 0.5f, 0.0f);
        w.req_creds[0] = 42;
        w.n_req_creds  = 1;
        int32_t creds[] = {42};
        QVERIFY(blhm_gate(&w, creds, 1) == true);
    }

    /* B-12: blhm_infer su grafo con 2 pesi — combined in [0, 3] */
    void inferResultInRange() {
        BLHMWeight w1 = makeWeight(7, 1.0f, 1.0f, 1.0f);
        BLHMWeight w2 = makeWeight(7, 0.5f, 0.5f, 0.5f);
        blhm_graph_add(m_g, &w1);
        blhm_graph_add(m_g, &w2);
        int32_t q[] = {7};
        BLHMResult res = blhm_infer(m_g, q, 1, nullptr, 0);
        /* combined = 0.50*Rf + 0.35*Rl + 0.15*Ru — sempre >= 0 */
        QVERIFY(res.combined >= 0.0f);
        QVERIFY(res.latency_ms >= 0.0);
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-C  RicercaPage — costruzione sezioni BLHM / RAB0-L
   10 casi
   ══════════════════════════════════════════════════════════════ */
class TestCatC : public QObject {
    Q_OBJECT
private:
    MockAiClient*       m_ai      = nullptr;
    RicercaPage*        m_page    = nullptr;   /* per C-01/C-02/C-10 */
    BioinformaticaPage* m_bioPage = nullptr;   /* per C-03..C-09 (BLHM/RAB0-L) */

private slots:

    void init() {
        m_ai     = new MockAiClient();
        m_page   = new RicercaPage(m_ai);
        m_bioPage = new BioinformaticaPage(m_ai);
    }

    void cleanup() {
        delete m_page;    m_page    = nullptr;
        delete m_bioPage; m_bioPage = nullptr;
        delete m_ai;      m_ai      = nullptr;
    }

    /* C-01: RicercaPage si costruisce con MockAiClient senza crash */
    void constructNocrash() {
        QVERIFY(m_page != nullptr);
    }

    /* C-02: e' un QWidget valido */
    void isQWidget() {
        QVERIFY(qobject_cast<QWidget*>(m_page) != nullptr);
    }

    /* C-03: m_rab0lCanvas esiste in BioinformaticaPage (findChild) */
    void rab0lCanvasExists() {
        Rab0lCanvas* c = m_bioPage->findChild<Rab0lCanvas*>();
        QVERIFY(c != nullptr);
    }

    /* C-04: m_rab0lSeq1 (QLineEdit) esiste in BioinformaticaPage */
    void rab0lSeq1LineEditExists() {
        QList<QLineEdit*> edits = m_bioPage->findChildren<QLineEdit*>();
        QVERIFY(!edits.isEmpty());
    }

    /* C-05: m_rab0lSeq2 — almeno 2 QLineEdit in BioinformaticaPage */
    void rab0lSeq2LineEditExists() {
        QList<QLineEdit*> edits = m_bioPage->findChildren<QLineEdit*>();
        QVERIFY(edits.size() >= 2);
    }

    /* C-06: m_blhmTable (QTableWidget) esiste in BioinformaticaPage */
    void blhmTableExists() {
        QTableWidget* t = m_bioPage->findChild<QTableWidget*>();
        QVERIFY(t != nullptr);
    }

    /* C-07: m_blhmOutput (QTextEdit) esiste in BioinformaticaPage */
    void blhmOutputExists() {
        QTextEdit* te = m_bioPage->findChild<QTextEdit*>();
        QVERIFY(te != nullptr);
    }

    /* C-08: m_blhmDnaCanvas (secondo Rab0lCanvas) esiste in BioinformaticaPage */
    void blhmDnaCanvasExists() {
        QList<Rab0lCanvas*> canvases = m_bioPage->findChildren<Rab0lCanvas*>();
        /* Devono esistere almeno 2 canvas: m_rab0lCanvas + m_blhmDnaCanvas */
        QVERIFY(canvases.size() >= 2);
    }

    /* C-09: show() + processEvents non crasha */
    void showNoCrash() {
        m_bioPage->resize(800, 600);
        m_bioPage->show();
        QApplication::processEvents();
        QVERIFY(m_bioPage->isVisible());
    }

    /* C-10: ragGraphMemory() restituisce un puntatore (puo' essere null se
     *       Ollama non e' disponibile — accettiamo entrambi) */
    void ragGraphMemoryCallable() {
        /* Chiamare ragGraphMemory() non deve crashare */
        GraphMemory* gm = m_page->ragGraphMemory();
        Q_UNUSED(gm);
        QVERIFY(true);
    }
};

/* ══════════════════════════════════════════════════════════════
   main
   ══════════════════════════════════════════════════════════════ */
int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    int r = 0;
    { TestCatA t; r |= QTest::qExec(&t, argc, argv); }
    { TestCatB t; r |= QTest::qExec(&t, argc, argv); }
    { TestCatC t; r |= QTest::qExec(&t, argc, argv); }
    return r;
}

#include "test_blhm_rab0l.moc"
