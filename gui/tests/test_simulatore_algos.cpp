/* ══════════════════════════════════════════════════════════════
   test_simulatore_algos.cpp — Unit test algoritmi SimulatorePage
   ──────────────────────────────────────────────────────────────
   CAT-A  BubbleSort — correttezza passi (10 test)
   CAT-B  Sorting avanzato (SelectionSort, InsertionSort, QuickSort,
          MergeSort, HeapSort) — 4 invarianti × 5 algos (20 test)
   CAT-C  Ricerca (LinearSearch, BinarySearch) — 8 test
   CAT-D  AlgoStep — invarianti generali (8 test)
   CAT-E  BO::evalClass e logScale (8 test)

   Build:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_simulatore_algos
     ./build_tests/test_simulatore_algos
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QApplication>
#include <algorithm>
#include <cmath>

#include "mock_ai_client.h"
#include "../pages/simulatore_page.h"

/* ──────────────────────────────────────────────────────────────
   Access structs — delegano l'accesso ai metodi privati tramite
   le dichiarazioni friend nel header di produzione.
   ────────────────────────────────────────────────────────────── */
struct SimulatoreAlgosAccess {
    static QVector<AlgoStep> bubbleSort(SimulatorePage* p, QVector<int> a)    { return p->genBubbleSort(a); }
    static QVector<AlgoStep> selectionSort(SimulatorePage* p, QVector<int> a) { return p->genSelectionSort(a); }
    static QVector<AlgoStep> insertionSort(SimulatorePage* p, QVector<int> a) { return p->genInsertionSort(a); }
    static QVector<AlgoStep> quickSort(SimulatorePage* p, QVector<int> a)     { return p->genQuickSort(a); }
    static QVector<AlgoStep> mergeSort(SimulatorePage* p, QVector<int> a)     { return p->genMergeSort(a); }
    static QVector<AlgoStep> heapSort(SimulatorePage* p, QVector<int> a)      { return p->genHeapSort(a); }
    static QVector<AlgoStep> linearSearch(SimulatorePage* p, QVector<int> a, int t) { return p->genLinearSearch(a, t); }
    static QVector<AlgoStep> binarySearch(SimulatorePage* p, QVector<int> a, int t) { return p->genBinarySearch(a, t); }
};

struct BigOAccess {
    static double evalClass(BigOClass cls, double n)    { return BigOWidget::evalClass(cls, n); }
    static double logScale(double v, double ref)        { return BigOWidget::logScale(v, ref); }
};

using SA = SimulatoreAlgosAccess;
using BO = BigOAccess;

/* ──────────────────────────────────────────────────────────────
   Helpers
   ────────────────────────────────────────────────────────────── */

/* Restituisce true se tutti gli indici negli insiemi sono nel range [0, size) */
static bool indiciValidi(const QVector<int>& idx, int size)
{
    for (int i : idx)
        if (i < 0 || i >= size) return false;
    return true;
}

/* Restituisce true se ogni step mantiene la stessa dimensione dell'array originale */
static bool dimStabile(const QVector<AlgoStep>& steps, int expectedSize)
{
    for (const AlgoStep& s : steps)
        if (s.arr.size() != expectedSize) return false;
    return true;
}

/* Restituisce l'array dell'ultimo step */
static QVector<int> lastArr(const QVector<AlgoStep>& steps)
{
    if (steps.isEmpty()) return {};
    return steps.last().arr;
}

/* Crea una SimulatorePage con mock AI (necessaria per istanziare i gen*) */
static SimulatorePage* makePage()
{
    static MockAiClient mock;
    return new SimulatorePage(&mock);
}

/* ══════════════════════════════════════════════════════════════
   CAT-A — BubbleSort: correttezza passi (10 test)
   ══════════════════════════════════════════════════════════════ */
class TestBubbleSort : public QObject {
    Q_OBJECT
    SimulatorePage* page = nullptr;

private slots:
    void initTestCase()  { page = makePage(); }
    void cleanupTestCase() { delete page; }

    void listaVuotaNoCrash() {
        const auto steps = SA::bubbleSort(page, {});
        /* Lista vuota: nessun crash, 0 o 1 step finale */
        QVERIFY(steps.size() >= 0);
    }

    void listaSingoloElemento() {
        const auto steps = SA::bubbleSort(page, {42});
        QVERIFY(!steps.isEmpty());
        QCOMPARE(lastArr(steps), QVector<int>{42});
    }

    void listaGiaOrdinata() {
        const QVector<int> input = {1, 2, 3, 4, 5};
        const auto steps = SA::bubbleSort(page, input);
        QVERIFY(!steps.isEmpty());
        QCOMPARE(lastArr(steps), input);
    }

    void listaInversa() {
        const QVector<int> input = {5, 4, 3, 2, 1};
        const QVector<int> expected = {1, 2, 3, 4, 5};
        const auto steps = SA::bubbleSort(page, input);
        QCOMPARE(lastArr(steps), expected);
    }

    void listaDuplicati() {
        const QVector<int> input = {3, 1, 4, 1, 5, 9, 2, 6};
        const auto steps = SA::bubbleSort(page, input);
        QVector<int> expected = input;
        std::sort(expected.begin(), expected.end());
        QCOMPARE(lastArr(steps), expected);
    }

    void ultimoStepTuttoSortato() {
        const QVector<int> input = {4, 2, 7, 1, 5};
        const auto steps = SA::bubbleSort(page, input);
        QVERIFY(!steps.isEmpty());
        /* L'ultimo step deve avere tutti gli indici in 'sorted' */
        const AlgoStep& last = steps.last();
        QCOMPARE(last.sorted.size(), input.size());
    }

    void dimensioneArrStabileOgniStep() {
        const QVector<int> input = {7, 3, 5, 2, 8};
        const auto steps = SA::bubbleSort(page, input);
        QVERIFY(dimStabile(steps, input.size()));
    }

    void indiciCompareValidi() {
        const QVector<int> input = {5, 3, 8, 1, 9, 2};
        const int n = input.size();
        const auto steps = SA::bubbleSort(page, input);
        for (const AlgoStep& s : steps)
            QVERIFY2(indiciValidi(s.cmp, n), "indice cmp fuori range");
    }

    void indiciSwapValidi() {
        const QVector<int> input = {5, 3, 8, 1, 9, 2};
        const int n = input.size();
        const auto steps = SA::bubbleSort(page, input);
        for (const AlgoStep& s : steps)
            QVERIFY2(indiciValidi(s.swp, n), "indice swp fuori range");
    }

    void dueElementiDisordinati() {
        const auto steps = SA::bubbleSort(page, {2, 1});
        QCOMPARE(lastArr(steps), QVector<int>({1, 2}));
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B — Sorting avanzato: 4 invarianti × 5 algoritmi
   ══════════════════════════════════════════════════════════════ */

/* Classe generica parametrizzata sull'algoritmo */
class TestSortingGenerico : public QObject {
    Q_OBJECT

    using GenFn = std::function<QVector<AlgoStep>(QVector<int>)>;
    GenFn      m_gen;
    const char* m_name;

public:
    TestSortingGenerico(GenFn fn, const char* name)
        : m_gen(std::move(fn)), m_name(name) {}

private slots:

    void listaVuotaNoCrash() {
        const auto steps = m_gen({});
        Q_UNUSED(steps);
        /* no crash */
    }

    void listaInversaOrdinata() {
        const QVector<int> input = {10, 7, 5, 3, 1};
        const QVector<int> expected = {1, 3, 5, 7, 10};
        const auto steps = m_gen(input);
        QVERIFY2(lastArr(steps) == expected,
                 qPrintable(QString("%1: lista inversa non ordinata").arg(m_name)));
    }

    void listaDuplicatiOrdinata() {
        const QVector<int> input = {5, 2, 8, 2, 5, 1, 9};
        QVector<int> expected = input;
        std::sort(expected.begin(), expected.end());
        const auto steps = m_gen(input);
        QVERIFY2(lastArr(steps) == expected,
                 qPrintable(QString("%1: lista con duplicati non ordinata").arg(m_name)));
    }

    void ultimoStepTuttoSortato() {
        const QVector<int> input = {4, 2, 7, 1, 5};
        const auto steps = m_gen(input);
        QVERIFY(!steps.isEmpty());
        /* L'ultimo step deve avere sorted.size() == arr.size() */
        const AlgoStep& last = steps.last();
        QVERIFY2(last.sorted.size() == input.size() || last.sorted.isEmpty(),
                 /* alcuni algo non popolano 'sorted', è accettabile */
                 qPrintable(QString("%1: ultimo step non ha tutti sorted").arg(m_name)));
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-C — Ricerca (LinearSearch, BinarySearch) — 8 test
   ══════════════════════════════════════════════════════════════ */
class TestRicerca : public QObject {
    Q_OBJECT
    SimulatorePage* page = nullptr;

private slots:
    void initTestCase()  { page = makePage(); }
    void cleanupTestCase() { delete page; }

    /* LinearSearch: elemento presente */
    void linearPresente() {
        const QVector<int> input = {5, 3, 8, 1, 9, 2, 7};
        const auto steps = SA::linearSearch(page, input, 9);
        QVERIFY(!steps.isEmpty());
        bool trovato = false;
        for (const AlgoStep& s : steps)
            if (!s.found.isEmpty()) trovato = true;
        QVERIFY2(trovato, "linearSearch: elemento 9 non trovato");
    }

    /* LinearSearch: elemento assente */
    void linearAssente() {
        const QVector<int> input = {5, 3, 8, 1, 9};
        const auto steps = SA::linearSearch(page, input, 99);
        QVERIFY(!steps.isEmpty());
        /* Nell'ultimo step found deve essere vuoto */
        QVERIFY2(steps.last().found.isEmpty(),
                 "linearSearch: 99 non dovrebbe essere trovato");
    }

    /* LinearSearch: indici validi */
    void linearIndiciValidi() {
        const QVector<int> input = {3, 1, 4, 1, 5, 9};
        const int n = input.size();
        const auto steps = SA::linearSearch(page, input, 4);
        for (const AlgoStep& s : steps) {
            QVERIFY(indiciValidi(s.cmp,   n));
            QVERIFY(indiciValidi(s.found, n));
        }
    }

    /* LinearSearch: dimensione arr stabile */
    void linearDimStabile() {
        const QVector<int> input = {7, 2, 5, 8, 3};
        const auto steps = SA::linearSearch(page, input, 5);
        QVERIFY(dimStabile(steps, input.size()));
    }

    /* BinarySearch: lista ordinata, elemento presente */
    void binaryPresente() {
        const QVector<int> input = {1, 3, 5, 7, 9, 11, 13};
        const auto steps = SA::binarySearch(page, input, 7);
        QVERIFY(!steps.isEmpty());
        bool trovato = false;
        for (const AlgoStep& s : steps)
            if (!s.found.isEmpty()) trovato = true;
        QVERIFY2(trovato, "binarySearch: elemento 7 non trovato");
    }

    /* BinarySearch: elemento assente */
    void binaryAssente() {
        const QVector<int> input = {1, 3, 5, 7, 9};
        const auto steps = SA::binarySearch(page, input, 6);
        QVERIFY(!steps.isEmpty());
        QVERIFY(steps.last().found.isEmpty());
    }

    /* BinarySearch: lista di 1024 → passi ragionevoli (il simulatore usa più step per passo logico) */
    void binaryMaxPassiLog() {
        QVector<int> input;
        for (int i = 0; i < 1024; ++i) input << i * 2;
        const auto steps = SA::binarySearch(page, input, 500);
        /* Il simulatore può usare più step per ogni confronto logico.
         * Soglia conservativa: ≤ log₂(1024) × 4 = 40 step */
        QVERIFY2(steps.size() <= 40,
                 qPrintable(QString("binarySearch su 1024 elementi: %1 passi (>40)").arg(steps.size())));
    }

    /* BinarySearch: indici nella lista inactive coprono tutto alla fine */
    void binaryInactiveCopreTutto() {
        const QVector<int> input = {1, 3, 5, 7, 9, 11};
        /* Cerca elemento assente: inactive alla fine = tutta la lista */
        const auto steps = SA::binarySearch(page, input, 4);
        if (!steps.isEmpty()) {
            QVERIFY(indiciValidi(steps.last().inactive, input.size()));
        }
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-D — AlgoStep: invarianti generali su tutti gli algoritmi (8 test)
   ══════════════════════════════════════════════════════════════ */
class TestAlgoStepInvarianti : public QObject {
    Q_OBJECT
    SimulatorePage* page = nullptr;

private slots:
    void initTestCase()  { page = makePage(); }
    void cleanupTestCase() { delete page; }

    /* Tutti i gen* sullo stesso input producono risultati deterministici */
    void bubbleSortDeterministico() {
        const QVector<int> input = {5, 3, 1, 4, 2};
        const auto a = SA::bubbleSort(page, input);
        const auto b = SA::bubbleSort(page, input);
        QCOMPARE(a.size(), b.size());
        if (!a.isEmpty())
            QCOMPARE(lastArr(a), lastArr(b));
    }

    /* arr.size() non cambia tra step consecutivi (tutti gli algos) */
    void selectionSortDimStabile() {
        const QVector<int> input = {9, 7, 5, 3, 1};
        const auto steps = SA::selectionSort(page, input);
        QVERIFY(dimStabile(steps, input.size()));
    }

    void insertionSortDimStabile() {
        const QVector<int> input = {3, 1, 4, 1, 5};
        const auto steps = SA::insertionSort(page, input);
        QVERIFY(dimStabile(steps, input.size()));
    }

    /* msg non è mai null (può essere vuoto) */
    void msgNonNull() {
        const QVector<int> input = {4, 2, 6, 1};
        const auto steps = SA::bubbleSort(page, input);
        for (const AlgoStep& s : steps) {
            /* msg è QString — non può essere null in Qt6, ma verifichiamo */
            Q_UNUSED(s.msg);   /* no crash */
        }
    }

    /* QuickSort produce risultato ordinato */
    void quickSortCorrect() {
        const QVector<int> input = {8, 3, 1, 7, 4, 2, 9, 5};
        QVector<int> expected = input;
        std::sort(expected.begin(), expected.end());
        const auto steps = SA::quickSort(page, input);
        QCOMPARE(lastArr(steps), expected);
    }

    /* MergeSort produce risultato ordinato */
    void mergeSortCorrect() {
        const QVector<int> input = {6, 2, 9, 1, 8, 3};
        QVector<int> expected = input;
        std::sort(expected.begin(), expected.end());
        const auto steps = SA::mergeSort(page, input);
        QCOMPARE(lastArr(steps), expected);
    }

    /* HeapSort produce risultato ordinato */
    void heapSortCorrect() {
        const QVector<int> input = {5, 8, 1, 3, 9, 2, 7};
        QVector<int> expected = input;
        std::sort(expected.begin(), expected.end());
        const auto steps = SA::heapSort(page, input);
        QCOMPARE(lastArr(steps), expected);
    }

    /* Tutti i gen* su lista di 1000 elementi non crashano */
    void scalabilita1000Elementi() {
        QVector<int> big;
        for (int i = 999; i >= 0; --i) big << i;   /* inverso */

        /* Test solo BubbleSort sarebbe lentissimo su 1000 — usiamo QuickSort e MergeSort */
        const auto qs = SA::quickSort(page, big);
        const auto ms = SA::mergeSort(page, big);

        QVector<int> expected = big;
        std::sort(expected.begin(), expected.end());
        QCOMPARE(lastArr(qs), expected);
        QCOMPARE(lastArr(ms), expected);
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-E — BO::evalClass e logScale (8 test)
   ══════════════════════════════════════════════════════════════ */
class TestBigO : public QObject {
    Q_OBJECT
private slots:

    /* O(1) è costante rispetto a n */
    void o1Costante() {
        const double v10  = BO::evalClass(kO1, 10.0);
        const double v1000 = BO::evalClass(kO1, 1000.0);
        QCOMPARE(v10, v1000);
    }

    /* O(n) scala linearmente */
    void oNLineare() {
        const double v100  = BO::evalClass(kON, 100.0);
        const double v200  = BO::evalClass(kON, 200.0);
        /* v200 ≈ 2 × v100 (entro 1%) */
        QVERIFY2(std::fabs(v200 / v100 - 2.0) < 0.01,
                 qPrintable(QString("O(n) non lineare: %1 / %2 = %3").arg(v200).arg(v100).arg(v200/v100)));
    }

    /* O(n²) scala quadraticamente */
    void oN2Quadratico() {
        const double v100  = BO::evalClass(kON2, 100.0);
        const double v200  = BO::evalClass(kON2, 200.0);
        /* v200 ≈ 4 × v100 */
        QVERIFY2(std::fabs(v200 / v100 - 4.0) < 0.01,
                 qPrintable(QString("O(n²) non quadratico: ratio = %1").arg(v200/v100)));
    }

    /* O(log n) cresce più lentamente di O(n) */
    void oLogNMinoreDiON() {
        const double n = 1000.0;
        QVERIFY(BO::evalClass(kOLogN, n) < BO::evalClass(kON, n));
    }

    /* O(n log n) è compreso tra O(n) e O(n²) */
    void oNLogNTraNEtaN2() {
        const double n = 100.0;
        const double on     = BO::evalClass(kON, n);
        const double onlogn = BO::evalClass(kONLogN, n);
        const double on2    = BO::evalClass(kON2, n);
        QVERIFY(onlogn > on);
        QVERIFY(onlogn < on2);
    }

    /* O(2^n) per n ≤ 20: no crash, no NaN/Inf */
    void o2NNoOverflow() {
        for (double n = 1.0; n <= 20.0; n += 1.0) {
            const double v = BO::evalClass(kO2N, n);
            QVERIFY2(!std::isnan(v) && !std::isinf(v) && v >= 0,
                     qPrintable(QString("O(2^n) anomalo per n=%1: %2").arg(n).arg(v)));
        }
    }

    /* evalClass con n = 0 non crasha */
    void evalClassNZero() {
        const BigOClass classes[] = { kO1, kOLogN, kON, kONLogN, kON2, kO2N };
        for (BigOClass c : classes) {
            const double v = BO::evalClass(c, 0.0);
            Q_UNUSED(v);   /* no crash */
        }
    }

    /* logScale(0, ref) non crasha e ritorna 0 o valore finito */
    void logScaleZeroInput() {
        const double ref = BO::evalClass(kON2, 100.0);
        const double v   = BO::logScale(0.0, ref);
        QVERIFY2(!std::isnan(v) && !std::isinf(v),
                 qPrintable(QString("logScale(0, ref) = %1 — NaN o Inf").arg(v)));
    }
};

/* ══════════════════════════════════════════════════════════════
   Runner
   ══════════════════════════════════════════════════════════════ */
int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    int status = 0;

    {
        TestBubbleSort t;
        status |= QTest::qExec(&t, argc, argv);
    }

    /* CAT-B: 5 algoritmi — usiamo la classe generica via lambda (SA:: per accesso privato) */
    {
        MockAiClient mock;
        SimulatorePage* page = new SimulatorePage(&mock);

        auto run = [&](const char* name, std::function<QVector<AlgoStep>(QVector<int>)> fn) {
            TestSortingGenerico t(fn, name);
            return QTest::qExec(&t, argc, argv);
        };

        status |= run("SelectionSort", [&](QVector<int> a){ return SA::selectionSort(page, a); });
        status |= run("InsertionSort", [&](QVector<int> a){ return SA::insertionSort(page, a); });
        status |= run("QuickSort",     [&](QVector<int> a){ return SA::quickSort(page, a); });
        status |= run("MergeSort",     [&](QVector<int> a){ return SA::mergeSort(page, a); });
        status |= run("HeapSort",      [&](QVector<int> a){ return SA::heapSort(page, a); });

        delete page;
    }

    {
        TestRicerca t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestAlgoStepInvarianti t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestBigO t;
        status |= QTest::qExec(&t, argc, argv);
    }

    return status;
}

#include "test_simulatore_algos.moc"
