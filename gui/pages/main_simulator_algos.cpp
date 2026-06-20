/* main_simulator_algos.cpp — Control: rebuildAlgoCmb, buildSteps, showStep, auto */
#include "main_simulator.h"
#include <QQueue>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <cmath>


static const char* algoSpeedEmoji(const char* badge) {
    if (!badge) return "";
    if (strcmp(badge, "OTTIMO")   == 0) return "\xe2\x9a\xa1 ";
    if (strcmp(badge, "VELOCE")   == 0) return "\xe2\x9a\xa1 ";
    if (strcmp(badge, "LINEARE")  == 0) return "\xf0\x9f\x90\x87 ";
    if (strcmp(badge, "BUONO")    == 0) return "\xf0\x9f\x90\x87 ";
    if (strcmp(badge, "MEDIO")    == 0) return "\xf0\x9f\x90\x95 ";
    if (strcmp(badge, "LENTO")    == 0) return "\xf0\x9f\x90\x8c ";
    if (strcmp(badge, "???")      == 0) return "\xf0\x9f\xa4\x94 ";
    return "";
}

void SimulatorePage::rebuildAlgoCmb(int catIdx) {
    m_filteredIdx.clear();
    for (int i = 0; i < ALGO_COUNT; i++) {
        if (catIdx == 0 || (int)kAlgos[i].cat == catIdx - 1)
            m_filteredIdx.append(i);
    }
    m_algoCmb->blockSignals(true);
    m_algoCmb->clear();
    for (int fi = 0; fi < m_filteredIdx.size(); fi++)
        m_algoCmb->addItem(
            QString::fromUtf8(algoSpeedEmoji(kAlgos[m_filteredIdx[fi]].badge))
            + QString::fromUtf8(kAlgos[m_filteredIdx[fi]].name));
    /* tenta di conservare l'algoritmo corrente */
    int restore = 0;
    for (int fi = 0; fi < m_filteredIdx.size(); fi++) {
        if (m_filteredIdx[fi] == m_globalIdx) { restore = fi; break; }
    }
    m_algoCmb->setCurrentIndex(restore);
    m_algoCmb->blockSignals(false);
    if (!m_filteredIdx.isEmpty()) {
        m_globalIdx = m_filteredIdx[restore];
        m_descLbl->setText(QString::fromUtf8(kAlgos[m_globalIdx].desc));
        m_bigO->set(kAlgos[m_globalIdx].complexity,
                    kAlgos[m_globalIdx].bigOLabel,
                    kAlgos[m_globalIdx].badge);
    }
}

/* ══════════════════════════════════════════════════════════════
   buildSteps — genera passi per l'algoritmo selezionato
   ══════════════════════════════════════════════════════════════ */
void SimulatorePage::buildSteps() {
    stopAuto();
    const int n   = m_sizeCmb->currentText().toInt();
    const int idx = m_globalIdx;
    QVector<int> arr = randomArr(n);

    /* target casuale riusato da algoritmi di ricerca */
    auto pickT = [&]() -> int {
        return (rand()%4==0) ? arr[rand()%n]+rand()%12+1 : arr[rand()%n];
    };
    /* target garantito nella somma per Two Pointers */
    auto pickST = [&]() -> int {
        QVector<int> s = arr; std::sort(s.begin(), s.end());
        int l=0, r=n-1;
        while (l<r){ if(rand()%3==0) return s[l]+s[r]; l++; r--; }
        return s[0]+s[n-1];
    };

    switch (idx) {
    /* ── ORDINAMENTO 0-18 ── */
    case  0: m_steps = genBubbleSort(arr);                         break;
    case  1: m_steps = genSelectionSort(arr);                      break;
    case  2: m_steps = genInsertionSort(arr);                      break;
    case  3: m_steps = genShellSort(arr);                          break;
    case  4: m_steps = genCocktailSort(arr);                       break;
    case  5: m_steps = genCombSort(arr);                           break;
    case  6: m_steps = genGnomeSort(arr);                          break;
    case  7: m_steps = genOddEvenSort(arr);                        break;
    case  8: m_steps = genCycleSort(arr);                          break;
    case  9: m_steps = genPancakeSort(arr);                        break;
    case 10: m_steps = genQuickSort(arr);                          break;
    case 11: m_steps = genMergeSort(arr);                          break;
    case 12: m_steps = genHeapSort(arr);                           break;
    case 13: { QVector<int> a2 = randomArr(8);
               m_steps = genBitonicSort(a2); }                     break;
    case 14: m_steps = genCountingSort(arr);                       break;
    case 15: m_steps = genRadixSort(arr);                          break;
    case 16: m_steps = genBucketSort(arr);                         break;
    case 17: m_steps = genTimSort(arr);                            break;
    case 18: m_steps = genStoogeSort(arr);                         break;
    /* ── RICERCA 19-28 ── */
    case 19: m_steps = genLinearSearch(arr, pickT());              break;
    case 20: m_steps = genBinarySearch(arr, pickT());              break;
    case 21: m_steps = genJumpSearch(arr, pickT());                break;
    case 22: m_steps = genTernarySearch(arr, pickT());             break;
    case 23: m_steps = genInterpolationSearch(arr, pickT());       break;
    case 24: m_steps = genExponentialSearch(arr, pickT());         break;
    case 25: m_steps = genFibonacciSearch(arr, pickT());           break;
    case 26: m_steps = genTwoPointers(arr, pickST());              break;
    case 27: m_steps = genBoyerMooreVoting(arr);                   break;
    case 28: m_steps = genQuickSelect(arr, n/2);                   break;
    /* ── STRUTTURE DATI 29-36 ── */
    case 29: m_steps = genStack(arr);                              break;
    case 30: m_steps = genQueue(arr);                              break;
    case 31: m_steps = genDeque(arr);                              break;
    case 32: m_steps = genMinHeapBuild(arr);                       break;
    case 33: m_steps = genHashTable(arr);                          break;
    case 34: m_steps = genSegmentTree(arr);                        break;
    case 35: m_steps = genFenwickTree(arr);                        break;
    case 36: m_steps = genLRUCache(arr);                           break;
    /* ── GRAFI 37-47 ── */
    case 37: m_steps = genBFS();                                   break;
    case 38: m_steps = genDFS();                                   break;
    case 39: m_steps = genDijkstra();                              break;
    case 40: m_steps = genBellmanFord();                           break;
    case 41: m_steps = genFloydWarshall();                         break;
    case 42: m_steps = genTopologicalSort();                       break;
    case 43: m_steps = genKruskal();                               break;
    case 44: m_steps = genPrim();                                  break;
    case 45: m_steps = genUnionFind();                             break;
    case 46: m_steps = genTarjanSCC();                             break;
    case 47: m_steps = genAStar();                                 break;
    /* ── PROG. DINAMICA 48-57 ── */
    case 48: m_steps = genCoinChange();                            break;
    case 49: m_steps = genLIS(arr);                                break;
    case 50: m_steps = genKnapsack();                              break;
    case 51: m_steps = genLCS();                                   break;
    case 52: m_steps = genEditDistance();                          break;
    case 53: m_steps = genMatrixChain();                           break;
    case 54: m_steps = genEggDrop();                               break;
    case 55: m_steps = genRodCutting();                            break;
    case 56: m_steps = genSubsetSumDP(arr);                        break;
    case 57: m_steps = genMaxProductSubarray(arr);                 break;
    /* ── GREEDY 58-63 ── */
    case 58: m_steps = genActivitySelection();                     break;
    case 59: m_steps = genFractionalKnapsack();                    break;
    case 60: { QVector<int> fr; for(int i=0;i<8;i++) fr<<(1+rand()%20);
               m_steps = genHuffman(fr); }                         break;
    case 61: m_steps = genJobScheduling();                         break;
    case 62: { QVector<int> coins={1,5,10,25};
               m_steps = genCoinGreedy(coins, 30+rand()%71); }    break;
    case 63: m_steps = genMinPlatforms();                          break;
    /* ── BACKTRACKING 64-68 ── */
    case 64: m_steps = genNQueens(qMin(n, 6));                     break;
    case 65: m_steps = genSubsetSum(arr, pickST());                break;
    case 66: m_steps = genPermutations(arr);                       break;
    case 67: m_steps = genFloodFill(arr);                          break;
    case 68: m_steps = genRatInMaze();                             break;
    /* ── STRINGHE 69-73 ── */
    case 69: m_steps = genKMP("ABA", "ABABCABAB");                 break;
    case 70: m_steps = genRabinKarp("AB", "ABABCABAB");            break;
    case 71: m_steps = genZAlgorithm("AABXAA");                    break;
    case 72: m_steps = genManacher("RACECAR");                     break;
    case 73: m_steps = genLongestCommonPrefix(arr);                break;
    /* ── MATEMATICA 74-89 ── */
    case 74: m_steps = genSieve(50);                               break;
    case 75: m_steps = genSieveSundaram(25);                       break;
    case 76: { int a=12+rand()%80, b=8+rand()%60;
               m_steps = genGCD(a,b); }                            break;
    case 77: { int a=12+rand()%80, b=8+rand()%60;
               m_steps = genExtGCD(a,b); }                         break;
    case 78: m_steps = genFastPow(2+rand()%4, 6+rand()%7);        break;
    case 79: m_steps = genPrimeFactors(30+rand()%200);             break;
    case 80: m_steps = genMillerRabin(97+rand()%100);              break;
    case 81: m_steps = genPascalTriangle(qMin(n,8));               break;
    case 82: m_steps = genFibonacciDP(qMin(n,12));                 break;
    case 83: m_steps = genCatalan(qMin(n,8));                      break;
    case 84: m_steps = genMonteCarloPi(qMin(n*5,40));              break;
    case 85: m_steps = genCollatz(6+rand()%15);                    break;
    case 86: { int a=100+rand()%900, b=100+rand()%900;
               m_steps = genKaratsuba(a,b); }                      break;
    case 87: m_steps = genPrefixSum(arr);                          break;
    case 88: m_steps = genKadane(arr);                             break;
    case 89: { QVector<int> c; for(int i=0;i<5;i++) c<<(1+rand()%9);
               m_steps = genHorner(c, 2+rand()%4); }              break;
    /* ── PATTERN ARRAY 90-97 ── */
    case 90: m_steps = genSlidingWindow(arr, 3+rand()%2);          break;
    case 91: { QVector<int> df; for(int i=0;i<n;i++) df<<(rand()%3);
               m_steps = genDutchFlag(df); }                       break;
    case 92: m_steps = genTrappingRain(arr);                       break;
    case 93: m_steps = genNextGreater(arr);                        break;
    case 94: m_steps = genFisherYates(arr);                        break;
    case 95: m_steps = genStockProfit(arr);                        break;
    case 96: m_steps = genMaxCircularSubarray(arr);                break;
    case 97: m_steps = genCountInversions(arr);                    break;
    /* ── CLASSICI 98-104 ── */
    case  98: m_steps = genReservoirSampling(arr);                 break;
    case  99: m_steps = genFloydCycle();                           break;
    case 100: m_steps = genTowerOfHanoi(qMin(n,4));                break;
    case 101: m_steps = genGameOfLife1D(arr);                      break;
    case 102: m_steps = genRule30(arr);                            break;
    case 103: m_steps = genSpiralMatrix(qMin((int)std::sqrt((double)n)+1,4)); break;
    case 104: m_steps = genSierpinskiRow(qMin(n,8));               break;
    default:  m_steps = genBubbleSort(arr);                        break;
    }
    showStep(0);
}

/* ══════════════════════════════════════════════════════════════
   Navigazione
   ══════════════════════════════════════════════════════════════ */
void SimulatorePage::showStep(int idx) {
    if (m_steps.isEmpty() || idx < 0 || idx >= m_steps.size()) return;
    m_curStep = idx;
    m_vis->setStep(m_steps[idx]);
    m_msgLbl->setText(m_steps[idx].msg);
    m_stepLbl->setText(QString("Passo %1 / %2").arg(idx+1).arg(m_steps.size()));
    m_prevBtn->setEnabled(idx > 0);
    m_nextBtn->setEnabled(idx < m_steps.size() - 1);
}

void SimulatorePage::startAuto() {
    m_autoBtn->setText(tr("\u23f8 Pausa"));
    m_timer->start(1600 - m_speedSlider->value()); /* slider alto = veloce */
}

void SimulatorePage::stopAuto() {
    m_timer->stop();
    m_autoBtn->setText(tr("\u25b6\u25b6 Auto"));
}

