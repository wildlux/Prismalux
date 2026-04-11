#pragma once
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QSlider>
#include <QTimer>
#include <QTextEdit>
#include <QVector>
#include <QMetaObject>
#include <QMouseEvent>
#include "../ai_client.h"

/* Snapshot di un passo della simulazione */
struct AlgoStep {
    QVector<int> arr;
    QVector<int> cmp;       /* giallo  — in confronto          */
    QVector<int> swp;       /* rosso   — scambio               */
    QVector<int> sorted;    /* verde   — posizione finale       */
    QVector<int> found;     /* ciano   — trovato (ricerca)      */
    QVector<int> inactive;  /* grigio  — eliminato (bin search) */
    QString      msg;
};

/* ══════════════════════════════════════════════════════════════
   Metadati algoritmo — struttura unica per tutte le info statiche
   ══════════════════════════════════════════════════════════════ */
enum BigOClass {
    kO1, kOAlpha, kOLogLogN, kOLogN, kOSqrtN,
    kON, kONLogLogN, kONLogN, kONLog2N,
    kON2, kON2W, kON3, kO2N
};

/* Categoria per il filtro dropdown */
enum AlgoCat {
    CAT_SORT=0, CAT_SEARCH, CAT_STRUCT,
    CAT_GRAPH, CAT_DP, CAT_GREEDY,
    CAT_BACKTRACK, CAT_STRING, CAT_MATH,
    CAT_PATTERN, CAT_CLASSIC,
    CAT_COUNT   /* sentinel — non usare come categoria reale */
};

struct AlgoEntry {
    const char* name;        /* nome nel ComboBox                 */
    AlgoCat     cat;         /* categoria per filtro              */
    BigOClass   complexity;  /* per BigOWidget                    */
    const char* bigOLabel;   /* "O(n²)", "O(n log n)", ...        */
    const char* badge;       /* "LENTO", "BUONO", "VELOCE", ...   */
    const char* desc;        /* testo descrittivo (multiline)     */
};

/* Totale algoritmi registrati */
static constexpr int ALGO_COUNT = 110;
extern const AlgoEntry kAlgos[ALGO_COUNT];

/* ══════════════════════════════════════════════════════════════
   BigOWidget — grafico complessità O-grande pre-calcolato
   ══════════════════════════════════════════════════════════════ */
class BigOWidget : public QWidget {
    Q_OBJECT
public:
    explicit BigOWidget(QWidget* parent = nullptr);
    void set(BigOClass cls, const char* label, const char* badge);
    QSize sizeHint()        const override { return {290, 130}; }
    QSize minimumSizeHint() const override { return {220, 110}; }

    BigOClass   cls()   const { return m_cls;   }
    const char* label() const { return m_label; }
    const char* badge() const { return m_badge; }

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override { emit clicked(); }
    void enterEvent(QEnterEvent*) override { setCursor(Qt::PointingHandCursor); }
    void leaveEvent(QEvent*)      override { unsetCursor(); }

private:
    static double evalClass(BigOClass cls, double n);
    static double logScale(double v, double ref);

    BigOClass   m_cls   = kON2;
    const char* m_label = "O(n\xc2\xb2)";
    const char* m_badge = "LENTO";
};

/* Widget grafico: barre colorate proporzionali al valore */
class AlgoBarWidget : public QWidget {
    Q_OBJECT
public:
    explicit AlgoBarWidget(QWidget* parent = nullptr);
    void setStep(const AlgoStep& step);
    const AlgoStep& step() const { return m_step; }
    QSize sizeHint() const override { return {600, 260}; }

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override { emit clicked(); }
    void enterEvent(QEnterEvent*) override { setCursor(Qt::PointingHandCursor); }
    void leaveEvent(QEvent*)      override { unsetCursor(); }

private:
    AlgoStep m_step;
};

/* ══════════════════════════════════════════════════════════════
   SimulatorePage — pagina principale
   ══════════════════════════════════════════════════════════════ */
class SimulatorePage : public QWidget {
    Q_OBJECT
public:
    explicit SimulatorePage(AiClient* ai, QWidget* parent = nullptr);

signals:
    void backRequested();
    void openMonitorRequested();

private:
    void buildSteps();
    void showStep(int idx);
    void startAuto();
    void stopAuto();
    void rebuildAlgoCmb(int catIdx);   /* ricostruisce m_algoCmb per la categoria */

    /* ── Ordinamento ── */
    QVector<AlgoStep> genBubbleSort(QVector<int> a);
    QVector<AlgoStep> genSelectionSort(QVector<int> a);
    QVector<AlgoStep> genInsertionSort(QVector<int> a);
    QVector<AlgoStep> genShellSort(QVector<int> a);
    QVector<AlgoStep> genCocktailSort(QVector<int> a);
    QVector<AlgoStep> genCombSort(QVector<int> a);
    QVector<AlgoStep> genGnomeSort(QVector<int> a);
    QVector<AlgoStep> genOddEvenSort(QVector<int> a);
    QVector<AlgoStep> genCycleSort(QVector<int> a);
    QVector<AlgoStep> genPancakeSort(QVector<int> a);
    QVector<AlgoStep> genQuickSort(QVector<int> a);
    QVector<AlgoStep> genMergeSort(QVector<int> a);
    QVector<AlgoStep> genHeapSort(QVector<int> a);
    QVector<AlgoStep> genBitonicSort(QVector<int> a);
    QVector<AlgoStep> genCountingSort(QVector<int> a);
    QVector<AlgoStep> genRadixSort(QVector<int> a);
    QVector<AlgoStep> genBucketSort(QVector<int> a);
    QVector<AlgoStep> genTimSort(QVector<int> a);
    QVector<AlgoStep> genStoogeSort(QVector<int> a);

    /* ── Ricerca ── */
    QVector<AlgoStep> genLinearSearch(QVector<int> a, int t);
    QVector<AlgoStep> genBinarySearch(QVector<int> a, int t);
    QVector<AlgoStep> genJumpSearch(QVector<int> a, int t);
    QVector<AlgoStep> genTernarySearch(QVector<int> a, int t);
    QVector<AlgoStep> genInterpolationSearch(QVector<int> a, int t);
    QVector<AlgoStep> genExponentialSearch(QVector<int> a, int t);
    QVector<AlgoStep> genFibonacciSearch(QVector<int> a, int t);
    QVector<AlgoStep> genTwoPointers(QVector<int> a, int t);
    QVector<AlgoStep> genBoyerMooreVoting(QVector<int> a);
    QVector<AlgoStep> genQuickSelect(QVector<int> a, int k);

    /* ── Strutture Dati ── */
    QVector<AlgoStep> genStack(QVector<int> a);
    QVector<AlgoStep> genQueue(QVector<int> a);
    QVector<AlgoStep> genDeque(QVector<int> a);
    QVector<AlgoStep> genMinHeapBuild(QVector<int> a);
    QVector<AlgoStep> genHashTable(QVector<int> a);
    QVector<AlgoStep> genSegmentTree(QVector<int> a);
    QVector<AlgoStep> genFenwickTree(QVector<int> a);
    QVector<AlgoStep> genLRUCache(QVector<int> a);

    /* ── Grafi ── */
    QVector<AlgoStep> genBFS();
    QVector<AlgoStep> genDFS();
    QVector<AlgoStep> genDijkstra();
    QVector<AlgoStep> genBellmanFord();
    QVector<AlgoStep> genFloydWarshall();
    QVector<AlgoStep> genTopologicalSort();
    QVector<AlgoStep> genKruskal();
    QVector<AlgoStep> genPrim();
    QVector<AlgoStep> genUnionFind();
    QVector<AlgoStep> genTarjanSCC();
    QVector<AlgoStep> genAStar();

    /* ── Programmazione Dinamica ── */
    QVector<AlgoStep> genCoinChange();
    QVector<AlgoStep> genLIS(QVector<int> a);
    QVector<AlgoStep> genKnapsack();
    QVector<AlgoStep> genLCS();
    QVector<AlgoStep> genEditDistance();
    QVector<AlgoStep> genMatrixChain();
    QVector<AlgoStep> genEggDrop();
    QVector<AlgoStep> genRodCutting();
    QVector<AlgoStep> genSubsetSumDP(QVector<int> a);
    QVector<AlgoStep> genMaxProductSubarray(QVector<int> a);

    /* ── Greedy ── */
    QVector<AlgoStep> genActivitySelection();
    QVector<AlgoStep> genFractionalKnapsack();
    QVector<AlgoStep> genHuffman(QVector<int> freq);
    QVector<AlgoStep> genJobScheduling();
    QVector<AlgoStep> genCoinGreedy(QVector<int> a, int target);
    QVector<AlgoStep> genMinPlatforms();

    /* ── Backtracking ── */
    QVector<AlgoStep> genNQueens(int n);
    QVector<AlgoStep> genSubsetSum(QVector<int> a, int t);
    QVector<AlgoStep> genPermutations(QVector<int> a);
    QVector<AlgoStep> genFloodFill(QVector<int> a);
    QVector<AlgoStep> genRatInMaze();

    /* ── Stringhe ── */
    QVector<AlgoStep> genKMP(const QString& pattern, const QString& text);
    QVector<AlgoStep> genRabinKarp(const QString& pattern, const QString& text);
    QVector<AlgoStep> genZAlgorithm(const QString& s);
    QVector<AlgoStep> genLongestCommonPrefix(QVector<int> a);
    QVector<AlgoStep> genManacher(const QString& s);

    /* ── Matematica ── */
    QVector<AlgoStep> genSieve(int limit);
    QVector<AlgoStep> genSieveSundaram(int limit);
    QVector<AlgoStep> genGCD(int a, int b);
    QVector<AlgoStep> genExtGCD(int a, int b);
    QVector<AlgoStep> genFastPow(int base, int exp_);
    QVector<AlgoStep> genPrimeFactors(int n);
    QVector<AlgoStep> genMillerRabin(int n);
    QVector<AlgoStep> genPascalTriangle(int n);
    QVector<AlgoStep> genFibonacciDP(int n);
    QVector<AlgoStep> genCatalan(int n);
    QVector<AlgoStep> genMonteCarloPi(int n);
    QVector<AlgoStep> genCollatz(int n);
    QVector<AlgoStep> genKaratsuba(int a, int b);
    QVector<AlgoStep> genPrefixSum(QVector<int> a);
    QVector<AlgoStep> genKadane(QVector<int> a);
    QVector<AlgoStep> genHorner(QVector<int> coeffs, int x);

    /* ── Pattern Array ── */
    QVector<AlgoStep> genSlidingWindow(QVector<int> a, int k);
    QVector<AlgoStep> genDutchFlag(QVector<int> a);
    QVector<AlgoStep> genTrappingRain(QVector<int> a);
    QVector<AlgoStep> genNextGreater(QVector<int> a);
    QVector<AlgoStep> genFisherYates(QVector<int> a);
    QVector<AlgoStep> genStockProfit(QVector<int> a);
    QVector<AlgoStep> genMaxCircularSubarray(QVector<int> a);
    QVector<AlgoStep> genCountInversions(QVector<int> a);

    /* ── Classici / Storici ── */
    QVector<AlgoStep> genReservoirSampling(QVector<int> a);
    QVector<AlgoStep> genFloydCycle();
    QVector<AlgoStep> genTowerOfHanoi(int n);
    QVector<AlgoStep> genGameOfLife1D(QVector<int> a);
    QVector<AlgoStep> genRule30(QVector<int> a);
    QVector<AlgoStep> genSpiralMatrix(int n);
    QVector<AlgoStep> genSierpinskiRow(int n);

    /* ── Helper interni ── */
    void _qs(QVector<int>& a, int lo, int hi, QVector<AlgoStep>& st, QVector<int>& srt);
    void _ms(QVector<int>& a, int lo, int hi, QVector<AlgoStep>& st);
    void _heapify(QVector<int>& a, int n, int i, QVector<AlgoStep>& st, QVector<int>& srt);
    void _flip(QVector<int>& a, int k, QVector<AlgoStep>& st);
    void _stoogeRec(QVector<int>& a, int l, int r, QVector<AlgoStep>& st);
    void _hanoi(int n, int from, int to, int aux,
                QVector<int>& pegs, QVector<AlgoStep>& st);
    void _nqSolve(int col, QVector<int>& board, QVector<AlgoStep>& st, int& found);

    QVector<int> randomArr(int n);
    int pickTarget(const QVector<int>& arr);
    int pickSumTarget(const QVector<int>& arr);

    /* ── Stato UI ── */
    AiClient*      m_ai;
    AlgoBarWidget* m_vis         = nullptr;
    BigOWidget*    m_bigO        = nullptr;
    QLabel*        m_msgLbl      = nullptr;
    QLabel*        m_stepLbl     = nullptr;
    QLabel*        m_descLbl     = nullptr;
    QPushButton*   m_prevBtn     = nullptr;
    QPushButton*   m_nextBtn     = nullptr;
    QPushButton*   m_autoBtn     = nullptr;
    QComboBox*     m_catCmb      = nullptr;   /* filtro categoria */
    QComboBox*     m_algoCmb     = nullptr;
    QComboBox*     m_sizeCmb     = nullptr;
    QSlider*       m_speedSlider = nullptr;
    QTimer*        m_timer       = nullptr;
    QTextEdit*     m_aiLog       = nullptr;
    QLabel*        m_aiWaitLbl   = nullptr;
    QPushButton*   m_aiStopBtn   = nullptr;
    QMetaObject::Connection  m_aiCTok, m_aiCFin, m_aiCErr, m_aiCAbo;

    QVector<AlgoStep>        m_steps;
    int                      m_curStep   = 0;
    int                      m_globalIdx = 0;  /* indice in kAlgos[] */
    QVector<int>             m_filteredIdx;    /* mappa filtrato → globale */
};
