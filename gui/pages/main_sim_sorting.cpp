/* main_sim_sorting.cpp — Sorting, Search, Array algorithms */
#include "main_simulator.h"
#include <QQueue>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <cmath>

/*
 * simulatore_algos.cpp — Implementazione dei 110 algoritmi
 * ===========================================================
 * randomArr + tutti i gen*() + helper interni (_qs, _heapify, _flip, ...).
 * Nessuna dipendenza da Qt UI — solo QVector<AlgoStep>.
 * La UI del simulatore è in simulatore_page.cpp.
 */
#include "main_simulator.h"
#include <QQueue>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <cmath>

/* ══════════════════════════════════════════════════════════════
   Helper array casuale
   ══════════════════════════════════════════════════════════════ */
QVector<int> SimulatorePage::randomArr(int n) {
    QVector<int> a(n);
    for (int& v : a) v = 5 + rand() % 91;
    return a;
}

/* ══════════════════════════════════════════════════════════════
   BUBBLE SORT
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genBubbleSort(QVector<int> arr) {
    QVector<AlgoStep> steps;
    const int n = arr.size();
    QVector<int> srt;

    { AlgoStep s; s.arr = arr;
      s.msg = QString("Bubble Sort — %1 elementi. Ogni passata porta il massimo in fondo.").arg(n);
      steps << s; }

    for (int i = 0; i < n - 1; i++) {
        bool swapped = false;
        for (int j = 0; j < n - 1 - i; j++) {
            { AlgoStep s; s.arr = arr; s.cmp << j << j+1; s.sorted = srt;
              s.msg = QString("Passata %1: confronto [%2]=%3 e [%4]=%5")
                      .arg(i+1).arg(j).arg(arr[j]).arg(j+1).arg(arr[j+1]);
              steps << s; }

            if (arr[j] > arr[j+1]) {
                std::swap(arr[j], arr[j+1]);
                swapped = true;
                AlgoStep s; s.arr = arr; s.swp << j << j+1; s.sorted = srt;
                s.msg = QString("Scambio! %1 > %2 → invertiti").arg(arr[j+1]).arg(arr[j]);
                steps << s;
            }
        }
        srt << (n - 1 - i);
        { AlgoStep s; s.arr = arr; s.sorted = srt;
          s.msg = QString("Fine passata %1: [%2]=%3 nella posizione finale")
                  .arg(i+1).arg(n-1-i).arg(arr[n-1-i]);
          steps << s; }

        if (!swapped) {
            for (int k = 0; k < n - 1 - i; k++) srt << k;
            AlgoStep s; s.arr = arr; s.sorted = srt;
            s.msg = "Nessuno scambio: array già ordinato!";
            steps << s;
            return steps;
        }
    }
    srt << 0;
    { AlgoStep s; s.arr = arr; s.sorted = srt; s.msg = "Ordinamento completato!"; steps << s; }
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   SELECTION SORT
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genSelectionSort(QVector<int> arr) {
    QVector<AlgoStep> steps;
    const int n = arr.size();
    QVector<int> srt;

    { AlgoStep s; s.arr = arr;
      s.msg = QString("Selection Sort — %1 elementi. Trova il minimo, portalo in testa.").arg(n);
      steps << s; }

    for (int i = 0; i < n - 1; i++) {
        int minIdx = i;
        for (int j = i + 1; j < n; j++) {
            AlgoStep s; s.arr = arr; s.cmp << minIdx << j; s.sorted = srt;
            s.msg = QString("Cerca minimo da [%1]: confronto [%2]=%3 e [%4]=%5")
                    .arg(i).arg(minIdx).arg(arr[minIdx]).arg(j).arg(arr[j]);
            steps << s;
            if (arr[j] < arr[minIdx]) minIdx = j;
        }
        if (minIdx != i) {
            AlgoStep s; s.arr = arr; s.swp << i << minIdx; s.sorted = srt;
            s.msg = QString("Minimo=%1 in [%2]: scambio con [%3]=%4")
                    .arg(arr[minIdx]).arg(minIdx).arg(i).arg(arr[i]);
            steps << s;
            std::swap(arr[i], arr[minIdx]);
        }
        srt << i;
        AlgoStep s; s.arr = arr; s.sorted = srt;
        s.msg = QString("[%1]=%2 nella posizione finale").arg(i).arg(arr[i]);
        steps << s;
    }
    srt << (n-1);
    { AlgoStep s; s.arr = arr; s.sorted = srt; s.msg = "Ordinamento completato!"; steps << s; }
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   INSERTION SORT
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genInsertionSort(QVector<int> arr) {
    QVector<AlgoStep> steps;
    const int n = arr.size();
    QVector<int> srt; srt << 0;

    { AlgoStep s; s.arr = arr; s.sorted = srt;
      s.msg = QString("Insertion Sort — %1 elementi. [0] già ordinato.").arg(n);
      steps << s; }

    for (int i = 1; i < n; i++) {
        int key = arr[i];
        { AlgoStep s; s.arr = arr; s.cmp << i; s.sorted = srt;
          s.msg = QString("Inserisci key=%1 da posizione [%2]").arg(key).arg(i);
          steps << s; }

        int j = i - 1;
        while (j >= 0 && arr[j] > key) {
            AlgoStep s; s.arr = arr; s.cmp << j << j+1; s.sorted = srt;
            s.msg = QString("[%1]=%2 > %3: sposta a destra").arg(j).arg(arr[j]).arg(key);
            steps << s;
            arr[j+1] = arr[j];
            { AlgoStep s2; s2.arr = arr; s2.swp << j+1; s2.sorted = srt;
              s2.msg = QString("[%1]=%2 spostato in [%3]").arg(j).arg(arr[j+1]).arg(j+1);
              steps << s2; }
            j--;
        }
        arr[j+1] = key;
        srt << i;
        AlgoStep s; s.arr = arr; s.sorted = srt;
        s.msg = QString("key=%1 inserita in posizione [%2]").arg(key).arg(j+1);
        steps << s;
    }
    { AlgoStep s; s.arr = arr; s.sorted = srt; s.msg = "Ordinamento completato!"; steps << s; }
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   QUICK SORT — helper ricorsivo Lomuto
   ══════════════════════════════════════════════════════════════ */
void SimulatorePage::_qs(QVector<int>& a, int lo, int hi,
                          QVector<AlgoStep>& st, QVector<int>& srt) {
    if (lo > hi) return;
    if (lo == hi) { if (!srt.contains(lo)) srt << lo; return; }

    int pivot = a[hi];
    { AlgoStep s; s.arr = a; s.cmp << hi; s.sorted = srt;
      s.msg = QString("Partizione [%1..%2]: pivot = [%3]=%4").arg(lo).arg(hi).arg(hi).arg(pivot);
      st << s; }

    int i = lo - 1;
    for (int j = lo; j < hi; j++) {
        { AlgoStep s; s.arr = a; s.cmp << j << hi; s.sorted = srt;
          s.msg = QString("[%1]=%2 vs pivot=%3").arg(j).arg(a[j]).arg(pivot);
          st << s; }
        if (a[j] <= pivot) {
            i++;
            if (i != j) {
                std::swap(a[i], a[j]);
                AlgoStep s; s.arr = a; s.swp << i << j; s.sorted = srt;
                s.msg = QString("%1 ≤ pivot: scambio [%2]↔[%3]").arg(a[j]).arg(i).arg(j);
                st << s;
            }
        }
    }
    std::swap(a[i+1], a[hi]);
    srt << (i+1);
    { AlgoStep s; s.arr = a; s.sorted = srt;
      s.msg = QString("Pivot %1 nella posizione finale [%2]").arg(pivot).arg(i+1);
      st << s; }

    _qs(a, lo, i, st, srt);
    _qs(a, i+2, hi, st, srt);
}

QVector<AlgoStep> SimulatorePage::genQuickSort(QVector<int> arr) {
    QVector<AlgoStep> steps;
    QVector<int> srt;
    { AlgoStep s; s.arr = arr;
      s.msg = QString("Quick Sort — %1 elementi. Pivot=ultimo elemento (Lomuto).").arg(arr.size());
      steps << s; }
    _qs(arr, 0, arr.size()-1, steps, srt);
    /* assicura che tutte le posizioni siano marcate ordinate */
    for (int i = 0; i < arr.size(); i++) if (!srt.contains(i)) srt << i;
    { AlgoStep s; s.arr = arr; s.sorted = srt; s.msg = "Quick Sort completato!"; steps << s; }
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   MERGE SORT — bottom-up iterativo
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genMergeSort(QVector<int> arr) {
    QVector<AlgoStep> steps;
    const int n = arr.size();
    QVector<int> globalSorted;

    { AlgoStep s; s.arr = arr;
      s.msg = QString("Merge Sort — %1 elementi. Fonde blocchi di dimensione crescente.").arg(n);
      steps << s; }

    for (int size = 1; size < n; size *= 2) {
        for (int lo = 0; lo < n - size; lo += 2 * size) {
            int mid = lo + size - 1;
            int hi  = qMin(lo + 2 * size - 1, n - 1);

            /* mostra range che verrà fuso */
            { AlgoStep s; s.arr = arr; s.sorted = globalSorted;
              for (int k = lo; k <= hi; k++) s.cmp << k;
              s.msg = QString("Fusione [%1..%2] + [%3..%4]").arg(lo).arg(mid).arg(mid+1).arg(hi);
              steps << s; }

            /* merge */
            QVector<int> left(arr.begin()+lo, arr.begin()+mid+1);
            QVector<int> right(arr.begin()+mid+1, arr.begin()+hi+1);
            int ii = 0, jj = 0, k = lo;
            while (ii < left.size() && jj < right.size()) {
                if (left[ii] <= right[jj]) {
                    arr[k++] = left[ii++];
                } else {
                    arr[k++] = right[jj++];
                    AlgoStep s; s.arr = arr; s.swp << k-1; s.sorted = globalSorted;
                    s.msg = QString("Da destra: %1 → posizione [%2]").arg(arr[k-1]).arg(k-1);
                    steps << s;
                }
            }
            while (ii < left.size())  arr[k++] = left[ii++];
            while (jj < right.size()) arr[k++] = right[jj++];

            /* marca range come ordinato */
            for (int x = lo; x <= hi; x++)
                if (!globalSorted.contains(x)) globalSorted << x;
            AlgoStep s; s.arr = arr; s.sorted = globalSorted;
            s.msg = QString("Segmento [%1..%2] ordinato").arg(lo).arg(hi);
            steps << s;
        }
    }
    { AlgoStep s; s.arr = arr; s.sorted = globalSorted; s.msg = "Merge Sort completato!"; steps << s; }
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   LINEAR SEARCH
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genLinearSearch(QVector<int> arr, int target) {
    QVector<AlgoStep> steps;
    { AlgoStep s; s.arr = arr;
      s.msg = QString("Linear Search — target: %1 in %2 elementi").arg(target).arg(arr.size());
      steps << s; }

    for (int i = 0; i < arr.size(); i++) {
        AlgoStep s; s.arr = arr; s.cmp << i;
        s.msg = QString("Controllo [%1]=%2  —  %3 target %4?")
                .arg(i).arg(arr[i])
                .arg(arr[i] == target ? "==" : (arr[i] < target ? "<" : ">"))
                .arg(target);
        steps << s;
        if (arr[i] == target) {
            AlgoStep sf; sf.arr = arr; sf.found << i;
            sf.msg = QString("✓ Trovato %1 in posizione [%2]! (%3 confronti)").arg(target).arg(i).arg(i+1);
            steps << sf;
            return steps;
        }
    }
    AlgoStep s; s.arr = arr;
    s.msg = QString("✗ Elemento %1 non trovato (%2 confronti eseguiti)").arg(target).arg(arr.size());
    steps << s;
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   BINARY SEARCH
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genBinarySearch(QVector<int> arr, int target) {
    std::sort(arr.begin(), arr.end());
    QVector<AlgoStep> steps;
    QVector<int> inactive;

    { AlgoStep s; s.arr = arr;
      s.msg = QString("Binary Search — array ordinato, target: %1").arg(target);
      steps << s; }

    int lo = 0, hi = arr.size() - 1, iter = 0;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        iter++;
        { AlgoStep s; s.arr = arr; s.cmp << mid; s.inactive = inactive;
          s.msg = QString("Iter %1: centro [%2]=%3, intervallo [%4..%5]")
                  .arg(iter).arg(mid).arg(arr[mid]).arg(lo).arg(hi);
          steps << s; }

        if (arr[mid] == target) {
            AlgoStep sf; sf.arr = arr; sf.found << mid; sf.inactive = inactive;
            sf.msg = QString("✓ Trovato %1 in [%2] dopo %3 confronti!").arg(target).arg(mid).arg(iter);
            steps << sf;
            return steps;
        } else if (arr[mid] < target) {
            for (int x = lo; x <= mid; x++) if (!inactive.contains(x)) inactive << x;
            lo = mid + 1;
            AlgoStep s; s.arr = arr; s.inactive = inactive;
            s.msg = QString("%1 < %2: elimina sinistra, cerca in [%3..%4]")
                    .arg(arr[mid]).arg(target).arg(lo).arg(hi);
            steps << s;
        } else {
            for (int x = mid; x <= hi; x++) if (!inactive.contains(x)) inactive << x;
            hi = mid - 1;
            AlgoStep s; s.arr = arr; s.inactive = inactive;
            s.msg = QString("%1 > %2: elimina destra, cerca in [%3..%4]")
                    .arg(arr[mid]).arg(target).arg(lo).arg(hi);
            steps << s;
        }
    }
    AlgoStep s; s.arr = arr; s.inactive = inactive;
    s.msg = QString("✗ Elemento %1 non trovato (%2 confronti)").arg(target).arg(iter);
    steps << s;
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   HEAP SORT
   ══════════════════════════════════════════════════════════════ */
void SimulatorePage::_heapify(QVector<int>& a, int n, int i,
                               QVector<AlgoStep>& st, QVector<int>& srt) {
    int largest = i, l = 2*i+1, r = 2*i+2;
    if (l < n && a[l] > a[largest]) largest = l;
    if (r < n && a[r] > a[largest]) largest = r;

    { AlgoStep s; s.arr = a; s.cmp << i;
      if (l < n) s.cmp << l;
      if (r < n) s.cmp << r;
      s.sorted = srt;
      s.msg = QString("Heapify [%1]: figlio-max=%2").arg(i).arg(a[largest]);
      st << s; }

    if (largest != i) {
        std::swap(a[i], a[largest]);
        AlgoStep s; s.arr = a; s.swp << i << largest; s.sorted = srt;
        s.msg = QString("Scambio radice [%1]=%2 ↔ [%3]=%4")
                .arg(i).arg(a[largest]).arg(largest).arg(a[i]);
        st << s;
        _heapify(a, n, largest, st, srt);
    }
}

QVector<AlgoStep> SimulatorePage::genHeapSort(QVector<int> arr) {
    QVector<AlgoStep> steps;
    const int n = arr.size();
    QVector<int> srt;

    { AlgoStep s; s.arr = arr;
      s.msg = QString("Heap Sort — %1 elementi. Step 1: costruisce max-heap.").arg(n);
      steps << s; }

    /* Build max-heap */
    for (int i = n/2 - 1; i >= 0; i--)
        _heapify(arr, n, i, steps, srt);

    { AlgoStep s; s.arr = arr;
      s.msg = "Max-heap costruito. Step 2: estrae radice ripetutamente.";
      steps << s; }

    for (int i = n - 1; i > 0; i--) {
        std::swap(arr[0], arr[i]);
        srt << i;
        AlgoStep s; s.arr = arr; s.swp << 0 << i; s.sorted = srt;
        s.msg = QString("Estrae massimo %1 → posizione [%2]").arg(arr[i]).arg(i);
        steps << s;
        _heapify(arr, i, 0, steps, srt);
    }
    srt << 0;
    { AlgoStep s; s.arr = arr; s.sorted = srt; s.msg = "Heap Sort completato!"; steps << s; }
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   SHELL SORT (sequenza di Knuth: 1, 4, 13, 40, ...)
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genShellSort(QVector<int> arr) {
    QVector<AlgoStep> steps;
    const int n = arr.size();

    { AlgoStep s; s.arr = arr;
      s.msg = QString("Shell Sort — %1 elementi. Usa gap di Knuth (3h+1).").arg(n);
      steps << s; }

    /* Calcola il gap iniziale (sequenza Knuth) */
    int gap = 1;
    while (gap < n / 3) gap = 3 * gap + 1;

    while (gap >= 1) {
        { AlgoStep s; s.arr = arr;
          s.msg = QString("Nuova passata con gap = %1").arg(gap);
          steps << s; }

        for (int i = gap; i < n; i++) {
            int key = arr[i];
            int j   = i;
            { AlgoStep s; s.arr = arr; s.cmp << i;
              s.msg = QString("gap=%1: inserisci key=%2 da [%3]").arg(gap).arg(key).arg(i);
              steps << s; }

            while (j >= gap && arr[j - gap] > key) {
                AlgoStep s; s.arr = arr; s.cmp << j << j-gap;
                s.msg = QString("[%1]=%2 > %3: sposta a destra (gap=%4)")
                        .arg(j-gap).arg(arr[j-gap]).arg(key).arg(gap);
                steps << s;
                arr[j] = arr[j - gap];
                { AlgoStep s2; s2.arr = arr; s2.swp << j;
                  s2.msg = QString("[%1] spostato in [%2]").arg(j-gap).arg(j);
                  steps << s2; }
                j -= gap;
            }
            arr[j] = key;
            { AlgoStep s; s.arr = arr; s.sorted << j;
              s.msg = QString("key=%1 inserita in [%2]").arg(key).arg(j);
              steps << s; }
        }
        gap /= 3;
    }
    QVector<int> allSorted;
    for (int i = 0; i < n; i++) allSorted << i;
    { AlgoStep s; s.arr = arr; s.sorted = allSorted; s.msg = "Shell Sort completato!"; steps << s; }
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   COUNTING SORT (valori 5-95, range 91)
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genCountingSort(QVector<int> arr) {
    QVector<AlgoStep> steps;
    const int n = arr.size();
    const int minV = *std::min_element(arr.begin(), arr.end());
    const int maxV = *std::max_element(arr.begin(), arr.end());
    const int range = maxV - minV + 1;

    { AlgoStep s; s.arr = arr;
      s.msg = QString("Counting Sort — %1 elementi, range [%2..%3]. "
                      "Conta occorrenze senza confronti.").arg(n).arg(minV).arg(maxV);
      steps << s; }

    /* Step 1: conta occorrenze */
    QVector<int> count(range, 0);
    for (int i = 0; i < n; i++) {
        count[arr[i] - minV]++;
        AlgoStep s; s.arr = arr; s.cmp << i;
        s.msg = QString("Conta [%1]=%2 → count[%3]=%4")
                .arg(i).arg(arr[i]).arg(arr[i]-minV).arg(count[arr[i]-minV]);
        steps << s;
    }

    /* Step 2: ricostruisce l'array ordinato */
    QVector<int> out;
    QVector<int> sorted;
    for (int v = 0; v < range; v++) {
        for (int k = 0; k < count[v]; k++) {
            out << (v + minV);
            int pos = out.size() - 1;
            QVector<int> vis = arr;
            for (int p = 0; p < (int)out.size(); p++) vis[p] = out[p];
            sorted << pos;
            AlgoStep s; s.arr = vis; s.sorted = sorted;
            s.msg = QString("Posiziona valore %1 in [%2]").arg(v + minV).arg(pos);
            steps << s;
        }
    }
    { AlgoStep s; s.arr = out; s.sorted = sorted; s.msg = "Counting Sort completato!"; steps << s; }
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   JUMP SEARCH
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genJumpSearch(QVector<int> arr, int target) {
    std::sort(arr.begin(), arr.end());
    QVector<AlgoStep> steps;
    const int n = arr.size();
    const int step = (int)std::sqrt((double)n);

    { AlgoStep s; s.arr = arr;
      s.msg = QString("Jump Search — target: %1, array ordinato, blocco=%2").arg(target).arg(step);
      steps << s; }

    int prev = 0, cur = step;
    while (cur < n && arr[cur] < target) {
        AlgoStep s; s.arr = arr; s.cmp << cur;
        s.msg = QString("arr[%1]=%2 < %3: salta al blocco successivo").arg(cur).arg(arr[cur]).arg(target);
        steps << s;
        prev = cur;
        cur  = qMin(cur + step, n);
    }

    /* Ricerca lineare nel blocco */
    for (int i = prev; i < qMin(cur, n); i++) {
        AlgoStep s; s.arr = arr; s.cmp << i;
        s.msg = QString("Ricerca lineare [%1..%2]: controllo [%3]=%4")
                .arg(prev).arg(qMin(cur, n)-1).arg(i).arg(arr[i]);
        steps << s;
        if (arr[i] == target) {
            AlgoStep sf; sf.arr = arr; sf.found << i;
            sf.msg = QString("\xe2\x9c\x93 Trovato %1 in [%2]!").arg(target).arg(i);
            steps << sf;
            return steps;
        }
    }
    AlgoStep s; s.arr = arr;
    s.msg = QString("\xe2\x9c\x97 Elemento %1 non trovato.").arg(target);
    steps << s;
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   TERNARY SEARCH
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genTernarySearch(QVector<int> arr, int target) {
    std::sort(arr.begin(), arr.end());
    QVector<AlgoStep> steps;
    QVector<int> inactive;

    { AlgoStep s; s.arr = arr;
      s.msg = QString("Ternary Search — target: %1. Divide in 3 parti ad ogni passo.").arg(target);
      steps << s; }

    int lo = 0, hi = arr.size() - 1, iter = 0;
    while (lo <= hi) {
        int m1 = lo + (hi - lo) / 3;
        int m2 = hi - (hi - lo) / 3;
        iter++;
        AlgoStep s; s.arr = arr; s.cmp << m1 << m2; s.inactive = inactive;
        s.msg = QString("Iter %1: m1=[%2]=%3, m2=[%4]=%5, range [%6..%7]")
                .arg(iter).arg(m1).arg(arr[m1]).arg(m2).arg(arr[m2]).arg(lo).arg(hi);
        steps << s;

        if (arr[m1] == target) {
            AlgoStep sf; sf.arr = arr; sf.found << m1; sf.inactive = inactive;
            sf.msg = QString("\xe2\x9c\x93 Trovato in m1=[%1]!").arg(m1);
            steps << sf; return steps;
        }
        if (arr[m2] == target) {
            AlgoStep sf; sf.arr = arr; sf.found << m2; sf.inactive = inactive;
            sf.msg = QString("\xe2\x9c\x93 Trovato in m2=[%1]!").arg(m2);
            steps << sf; return steps;
        }
        if (target < arr[m1]) {
            for (int x = m1; x <= hi; x++) if (!inactive.contains(x)) inactive << x;
            hi = m1 - 1;
            AlgoStep si; si.arr = arr; si.inactive = inactive;
            si.msg = QString("target < arr[m1]: cerca in [%1..%2]").arg(lo).arg(hi);
            steps << si;
        } else if (target > arr[m2]) {
            for (int x = lo; x <= m2; x++) if (!inactive.contains(x)) inactive << x;
            lo = m2 + 1;
            AlgoStep si; si.arr = arr; si.inactive = inactive;
            si.msg = QString("target > arr[m2]: cerca in [%1..%2]").arg(lo).arg(hi);
            steps << si;
        } else {
            for (int x = lo; x < m1; x++) if (!inactive.contains(x)) inactive << x;
            for (int x = m2+1; x <= hi; x++) if (!inactive.contains(x)) inactive << x;
            lo = m1 + 1; hi = m2 - 1;
            AlgoStep si; si.arr = arr; si.inactive = inactive;
            si.msg = QString("arr[m1] < target < arr[m2]: cerca in [%1..%2]").arg(lo).arg(hi);
            steps << si;
        }
    }
    AlgoStep sf; sf.arr = arr; sf.inactive = inactive;
    sf.msg = QString("\xe2\x9c\x97 Elemento %1 non trovato.").arg(target);
    steps << sf;
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   TWO POINTERS — trova coppia con somma target
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genTwoPointers(QVector<int> arr, int target) {
    std::sort(arr.begin(), arr.end());
    QVector<AlgoStep> steps;
    const int n = arr.size();

    { AlgoStep s; s.arr = arr;
      s.msg = QString("Two Pointers — array ordinato. Trova coppia con somma=%1.").arg(target);
      steps << s; }

    int l = 0, r = n - 1;
    while (l < r) {
        int sum = arr[l] + arr[r];
        AlgoStep s; s.arr = arr; s.cmp << l << r;
        s.msg = QString("L=[%1]=%2  R=[%3]=%4  somma=%5 (target=%6)")
                .arg(l).arg(arr[l]).arg(r).arg(arr[r]).arg(sum).arg(target);
        steps << s;

        if (sum == target) {
            AlgoStep sf; sf.arr = arr; sf.found << l << r;
            sf.msg = QString("\xe2\x9c\x93 Coppia trovata: [%1]=%2 + [%3]=%4 = %5!")
                     .arg(l).arg(arr[l]).arg(r).arg(arr[r]).arg(target);
            steps << sf;
            return steps;
        } else if (sum < target) {
            AlgoStep si; si.arr = arr; si.swp << l;
            si.msg = QString("Somma %1 < %2: sposta L a destra →").arg(sum).arg(target);
            steps << si;
            l++;
        } else {
            AlgoStep si; si.arr = arr; si.swp << r;
            si.msg = QString("Somma %1 > %2: sposta R a sinistra ←").arg(sum).arg(target);
            steps << si;
            r--;
        }
    }
    AlgoStep sf; sf.arr = arr;
    sf.msg = QString("\xe2\x9c\x97 Nessuna coppia trovata con somma=%1.").arg(target);
    steps << sf;
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   RADIX SORT LSD
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genRadixSort(QVector<int> arr) {
    QVector<AlgoStep> steps;
    const int n = arr.size();
    int maxV = *std::max_element(arr.begin(), arr.end());
    { AlgoStep s; s.arr = arr;
      s.msg = QString("Radix Sort LSD — %1 elementi. Ordina cifra per cifra.").arg(n);
      steps << s; }

    for (int exp = 1; maxV / exp > 0; exp *= 10) {
        QVector<int> cnt(10, 0);
        for (int i = 0; i < n; i++) { cnt[(arr[i]/exp)%10]++; }
        for (int i = 1; i < 10; i++) cnt[i] += cnt[i-1];
        QVector<int> out(n);
        for (int i = n-1; i >= 0; i--) out[--cnt[(arr[i]/exp)%10]] = arr[i];
        for (int i = 0; i < n; i++) arr[i] = out[i];
        QVector<int> srt; for (int i = 0; i < n; i++) srt << i;
        AlgoStep s; s.arr = arr; s.sorted = srt;
        s.msg = QString("Passata exp=%1: cifra '%2' estratta per ogni elemento").arg(exp)
                .arg(exp==1?"unità":exp==10?"decine":"centinaia");
        steps << s;
    }
    QVector<int> all; for (int i = 0; i < n; i++) all << i;
    { AlgoStep s; s.arr = arr; s.sorted = all; s.msg = "Radix Sort completato!"; steps << s; }
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   COCKTAIL SHAKER SORT
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genCocktailSort(QVector<int> arr) {
    QVector<AlgoStep> steps;
    const int n = arr.size();
    QVector<int> srt;
    { AlgoStep s; s.arr = arr;
      s.msg = QString("Cocktail Shaker — %1 elem. Bolla in entrambe le direzioni.").arg(n);
      steps << s; }

    int lo = 0, hi = n - 1;
    bool swapped = true;
    while (swapped && lo < hi) {
        swapped = false;
        for (int i = lo; i < hi; i++) {
            AlgoStep s; s.arr = arr; s.cmp << i << i+1; s.sorted = srt;
            s.msg = QString("→ [%1]=%2 vs [%3]=%4").arg(i).arg(arr[i]).arg(i+1).arg(arr[i+1]);
            steps << s;
            if (arr[i] > arr[i+1]) { std::swap(arr[i], arr[i+1]); swapped = true;
                AlgoStep sw; sw.arr = arr; sw.swp << i << i+1; sw.sorted = srt;
                sw.msg = "Scambio →"; steps << sw; }
        }
        srt << hi--; { AlgoStep s; s.arr = arr; s.sorted = srt; steps << s; }
        if (!swapped) break;
        swapped = false;
        for (int i = hi; i > lo; i--) {
            AlgoStep s; s.arr = arr; s.cmp << i << i-1; s.sorted = srt;
            s.msg = QString("← [%1]=%2 vs [%3]=%4").arg(i).arg(arr[i]).arg(i-1).arg(arr[i-1]);
            steps << s;
            if (arr[i] < arr[i-1]) { std::swap(arr[i], arr[i-1]); swapped = true;
                AlgoStep sw; sw.arr = arr; sw.swp << i << i-1; sw.sorted = srt;
                sw.msg = "Scambio ←"; steps << sw; }
        }
        srt << lo++; { AlgoStep s; s.arr = arr; s.sorted = srt; steps << s; }
    }
    QVector<int> all; for (int i = 0; i < arr.size(); i++) all << i;
    { AlgoStep s; s.arr = arr; s.sorted = all; s.msg = "Completato!"; steps << s; }
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   COMB SORT
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genCombSort(QVector<int> arr) {
    QVector<AlgoStep> steps;
    const int n = arr.size();
    { AlgoStep s; s.arr = arr;
      s.msg = QString("Comb Sort — %1 elem. Gap iniziale = %2 (÷1.3 ogni passata).").arg(n).arg(n);
      steps << s; }

    int gap = n; bool sorted_ = false;
    while (!sorted_) {
        gap = qMax(1, (int)(gap / 1.3));
        sorted_ = (gap == 1);
        for (int i = 0; i + gap < n; i++) {
            AlgoStep s; s.arr = arr; s.cmp << i << i+gap;
            s.msg = QString("gap=%1: [%2]=%3 vs [%4]=%5").arg(gap).arg(i).arg(arr[i]).arg(i+gap).arg(arr[i+gap]);
            steps << s;
            if (arr[i] > arr[i+gap]) {
                std::swap(arr[i], arr[i+gap]); sorted_ = false;
                AlgoStep sw; sw.arr = arr; sw.swp << i << i+gap;
                sw.msg = QString("Scambio gap=%1").arg(gap); steps << sw;
            }
        }
    }
    QVector<int> all; for (int i = 0; i < n; i++) all << i;
    { AlgoStep s; s.arr = arr; s.sorted = all; s.msg = "Comb Sort completato!"; steps << s; }
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   GNOME SORT
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genGnomeSort(QVector<int> arr) {
    QVector<AlgoStep> steps;
    const int n = arr.size();
    { AlgoStep s; s.arr = arr;
      s.msg = QString("Gnome Sort — %1 elem. Avanza se ok, scambia e torna indietro se no.").arg(n);
      steps << s; }

    int i = 0;
    while (i < n) {
        if (i == 0 || arr[i] >= arr[i-1]) {
            AlgoStep s; s.arr = arr; s.found << i;
            s.msg = QString("[%1]=%2 ≥ [%3]=%4: avanza").arg(i).arg(arr[i])
                    .arg(i>0?i-1:0).arg(i>0?arr[i-1]:0);
            steps << s; i++;
        } else {
            AlgoStep s; s.arr = arr; s.cmp << i << i-1;
            s.msg = QString("[%1]=%2 < [%3]=%4: scambio e torna").arg(i).arg(arr[i]).arg(i-1).arg(arr[i-1]);
            steps << s;
            std::swap(arr[i], arr[i-1]);
            AlgoStep sw; sw.arr = arr; sw.swp << i << i-1; steps << sw;
            i--;
        }
    }
    QVector<int> all; for (int i = 0; i < n; i++) all << i;
    { AlgoStep s; s.arr = arr; s.sorted = all; s.msg = "Gnome Sort completato!"; steps << s; }
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   CYCLE SORT
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genCycleSort(QVector<int> arr) {
    QVector<AlgoStep> steps;
    const int n = arr.size();
    QVector<int> srt;
    { AlgoStep s; s.arr = arr;
      s.msg = QString("Cycle Sort — %1 elem. Minimizza le scritture (≤ n-1 scambi).").arg(n);
      steps << s; }

    int writes = 0;
    for (int cycleStart = 0; cycleStart < n - 1; cycleStart++) {
        int item = arr[cycleStart];
        int pos  = cycleStart;
        for (int i = cycleStart+1; i < n; i++)
            if (arr[i] < item) pos++;
        if (pos == cycleStart) { srt << cycleStart; continue; }
        while (item == arr[pos]) pos++;
        std::swap(arr[pos], item); writes++;
        AlgoStep s; s.arr = arr; s.swp << pos; s.sorted = srt;
        s.msg = QString("Ciclo da [%1]: scrivi %2 in pos %3 (write #%4)")
                .arg(cycleStart).arg(item).arg(pos).arg(writes);
        steps << s;

        while (pos != cycleStart) {
            pos = cycleStart;
            for (int i = cycleStart+1; i < n; i++)
                if (arr[i] < item) pos++;
            while (item == arr[pos]) pos++;
            std::swap(arr[pos], item); writes++;
            AlgoStep s2; s2.arr = arr; s2.swp << pos; s2.sorted = srt;
            s2.msg = QString("Continua ciclo: scrivi %1 in pos %2").arg(item).arg(pos);
            steps << s2;
        }
        srt << cycleStart;
    }
    srt << (n-1);
    { AlgoStep s; s.arr = arr; s.sorted = srt;
      s.msg = QString("Cycle Sort completato! Scritture totali: %1").arg(writes);
      steps << s; }
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   PANCAKE SORT
   ══════════════════════════════════════════════════════════════ */
void SimulatorePage::_flip(QVector<int>& a, int k, QVector<AlgoStep>& st) {
    QVector<int> cmp; for (int i = 0; i <= k; i++) cmp << i;
    std::reverse(a.begin(), a.begin() + k + 1);
    AlgoStep s; s.arr = a; s.swp = cmp;
    s.msg = QString("flip(%1): inverti i primi %2 elementi").arg(k).arg(k+1);
    st << s;
}

QVector<AlgoStep> SimulatorePage::genPancakeSort(QVector<int> arr) {
    QVector<AlgoStep> steps;
    const int n = arr.size();
    QVector<int> srt;
    { AlgoStep s; s.arr = arr;
      s.msg = QString("Pancake Sort — %1 elem. Unica op: flip(k) inverte i primi k.").arg(n);
      steps << s; }

    for (int size = n; size > 1; size--) {
        int mi = 0;
        for (int i = 1; i < size; i++) if (arr[i] > arr[mi]) mi = i;
        if (mi != size - 1) {
            if (mi != 0) _flip(arr, mi, steps);
            _flip(arr, size - 1, steps);
        }
        srt << (size - 1);
        AlgoStep s; s.arr = arr; s.sorted = srt;
        s.msg = QString("[%1]=%2 nella posizione finale").arg(size-1).arg(arr[size-1]);
        steps << s;
    }
    srt << 0;
    { AlgoStep s; s.arr = arr; s.sorted = srt; s.msg = "Pancake Sort completato!"; steps << s; }
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   ODD-EVEN SORT
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genOddEvenSort(QVector<int> arr) {
    QVector<AlgoStep> steps;
    const int n = arr.size();
    { AlgoStep s; s.arr = arr;
      s.msg = QString("Odd-Even Sort — %1 elem. Alterna fase pari/dispari.").arg(n);
      steps << s; }

    bool sorted_ = false;
    while (!sorted_) {
        sorted_ = true;
        for (int phase = 0; phase < 2; phase++) {
            for (int i = phase; i < n-1; i += 2) {
                AlgoStep s; s.arr = arr; s.cmp << i << i+1;
                s.msg = QString("Fase %1 — [%2]=%3 vs [%4]=%5")
                        .arg(phase==0?"pari":"dispari").arg(i).arg(arr[i]).arg(i+1).arg(arr[i+1]);
                steps << s;
                if (arr[i] > arr[i+1]) {
                    std::swap(arr[i], arr[i+1]); sorted_ = false;
                    AlgoStep sw; sw.arr = arr; sw.swp << i << i+1;
                    sw.msg = "Scambio"; steps << sw;
                }
            }
        }
    }
    QVector<int> all; for (int i = 0; i < n; i++) all << i;
    { AlgoStep s; s.arr = arr; s.sorted = all; s.msg = "Odd-Even Sort completato!"; steps << s; }
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   BITONIC SORT (n deve essere potenza di 2 — usiamo 8)
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genBitonicSort(QVector<int> arr) {
    QVector<AlgoStep> steps;
    /* Forza n = 8 per visualizzazione corretta */
    while (arr.size() < 8) arr << 5 + rand()%91;
    arr.resize(8);
    const int n = 8;
    { AlgoStep s; s.arr = arr;
      s.msg = "Bitonic Sort — 8 elementi (n=2^k). Costruisce sequenze bitoniche e le fonde.";
      steps << s; }

    for (int k = 2; k <= n; k *= 2) {
        for (int j = k/2; j >= 1; j /= 2) {
            for (int i = 0; i < n; i++) {
                int l = i ^ j;
                if (l > i) {
                    bool asc = ((i & k) == 0);
                    AlgoStep s; s.arr = arr; s.cmp << i << l;
                    s.msg = QString("k=%1 j=%2: confronto [%3]=%4 e [%5]=%6 (%7)")
                            .arg(k).arg(j).arg(i).arg(arr[i]).arg(l).arg(arr[l])
                            .arg(asc?"asc":"desc");
                    steps << s;
                    if ((arr[i] > arr[l]) == asc) {
                        std::swap(arr[i], arr[l]);
                        AlgoStep sw; sw.arr = arr; sw.swp << i << l;
                        sw.msg = "Scambio"; steps << sw;
                    }
                }
            }
        }
    }
    QVector<int> all; for (int i = 0; i < n; i++) all << i;
    { AlgoStep s; s.arr = arr; s.sorted = all; s.msg = "Bitonic Sort completato!"; steps << s; }
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   INTERPOLATION SEARCH
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genInterpolationSearch(QVector<int> arr, int target) {
    std::sort(arr.begin(), arr.end());
    QVector<AlgoStep> steps;
    QVector<int> inactive;
    { AlgoStep s; s.arr = arr;
      s.msg = QString("Interpolation Search — target: %1. Stima posizione con formula lineare.").arg(target);
      steps << s; }

    int lo = 0, hi = arr.size()-1, iter = 0;
    while (lo <= hi && target >= arr[lo] && target <= arr[hi]) {
        iter++;
        if (arr[hi] == arr[lo]) { break; }
        int pos = lo + (int)((double)(target - arr[lo]) / (arr[hi] - arr[lo]) * (hi - lo));
        pos = qMax(lo, qMin(hi, pos));
        AlgoStep s; s.arr = arr; s.cmp << pos; s.inactive = inactive;
        s.msg = QString("Iter %1: pos=%2 (formula), [%3]=%4 — range [%5..%6]")
                .arg(iter).arg(pos).arg(pos).arg(arr[pos]).arg(lo).arg(hi);
        steps << s;
        if (arr[pos] == target) {
            AlgoStep sf; sf.arr = arr; sf.found << pos; sf.inactive = inactive;
            sf.msg = QString("\xe2\x9c\x93 Trovato %1 in [%2]!").arg(target).arg(pos);
            steps << sf; return steps;
        }
        if (arr[pos] < target) {
            for (int x = lo; x <= pos; x++) if (!inactive.contains(x)) inactive << x;
            lo = pos + 1;
        } else {
            for (int x = pos; x <= hi; x++) if (!inactive.contains(x)) inactive << x;
            hi = pos - 1;
        }
    }
    AlgoStep s; s.arr = arr; s.inactive = inactive;
    s.msg = QString("\xe2\x9c\x97 Elemento %1 non trovato.").arg(target);
    steps << s;
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   EXPONENTIAL SEARCH
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genExponentialSearch(QVector<int> arr, int target) {
    std::sort(arr.begin(), arr.end());
    QVector<AlgoStep> steps;
    const int n = arr.size();
    QVector<int> inactive;
    { AlgoStep s; s.arr = arr;
      s.msg = QString("Exponential Search — target: %1. Raddoppia il range, poi Binary Search.").arg(target);
      steps << s; }

    if (arr[0] == target) {
        AlgoStep s; s.arr = arr; s.found << 0; s.msg = "Trovato in [0]!"; steps << s; return steps;
    }
    int i = 1;
    while (i < n && arr[i] <= target) {
        AlgoStep s; s.arr = arr; s.cmp << i;
        s.msg = QString("arr[%1]=%2 ≤ %3: raddoppio range → %4").arg(i).arg(arr[i]).arg(target).arg(i*2);
        steps << s;
        i *= 2;
    }
    /* Binary search in [i/2, min(i, n-1)] */
    int lo = i/2, hi = qMin(i, n-1);
    for (int x = 0; x < lo; x++) inactive << x;
    { AlgoStep s; s.arr = arr; s.inactive = inactive;
      s.msg = QString("Binary Search in [%1..%2]").arg(lo).arg(hi);
      steps << s; }

    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        AlgoStep s; s.arr = arr; s.cmp << mid; s.inactive = inactive;
        s.msg = QString("Binary: mid=[%1]=%2").arg(mid).arg(arr[mid]);
        steps << s;
        if (arr[mid] == target) {
            AlgoStep sf; sf.arr = arr; sf.found << mid; sf.inactive = inactive;
            sf.msg = QString("\xe2\x9c\x93 Trovato %1 in [%2]!").arg(target).arg(mid);
            steps << sf; return steps;
        }
        if (arr[mid] < target) lo = mid + 1; else hi = mid - 1;
    }
    AlgoStep s; s.arr = arr; s.inactive = inactive;
    s.msg = QString("\xe2\x9c\x97 Elemento %1 non trovato.").arg(target);
    steps << s;
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   FIBONACCI SEARCH
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genFibonacciSearch(QVector<int> arr, int target) {
    std::sort(arr.begin(), arr.end());
    QVector<AlgoStep> steps;
    const int n = arr.size();
    QVector<int> inactive;
    { AlgoStep s; s.arr = arr;
      s.msg = QString("Fibonacci Search — target: %1. Usa indici Fibonacci per dividere.").arg(target);
      steps << s; }

    int fibM2 = 0, fibM1 = 1, fibM = 1;
    while (fibM < n) { fibM2 = fibM1; fibM1 = fibM; fibM = fibM1 + fibM2; }
    int offset = -1, iter = 0;
    while (fibM > 1) {
        int i = qMin(offset + fibM2, n-1); iter++;
        AlgoStep s; s.arr = arr; s.cmp << i; s.inactive = inactive;
        s.msg = QString("Iter %1: fib=(%2,%3,%4), controllo [%5]=%6")
                .arg(iter).arg(fibM2).arg(fibM1).arg(fibM).arg(i).arg(arr[i]);
        steps << s;
        if (arr[i] == target) {
            AlgoStep sf; sf.arr = arr; sf.found << i;
            sf.msg = QString("\xe2\x9c\x93 Trovato %1 in [%2]!").arg(target).arg(i);
            steps << sf; return steps;
        }
        if (arr[i] < target) {
            for (int x = 0; x <= i; x++) if (!inactive.contains(x)) inactive << x;
            fibM = fibM1; fibM1 = fibM2; fibM2 = fibM - fibM1;
            offset = i;
        } else {
            for (int x = i; x < n; x++) if (!inactive.contains(x)) inactive << x;
            fibM = fibM2; fibM1 -= fibM2; fibM2 = fibM - fibM1;
        }
    }
    if (fibM1 && offset+1 < n && arr[offset+1] == target) {
        AlgoStep sf; sf.arr = arr; sf.found << offset+1;
        sf.msg = QString("\xe2\x9c\x93 Trovato %1 in [%2]!").arg(target).arg(offset+1);
        steps << sf; return steps;
    }
    AlgoStep s; s.arr = arr; s.inactive = inactive;
    s.msg = QString("\xe2\x9c\x97 Elemento %1 non trovato.").arg(target);
    steps << s;
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   SIEVE OF ERATOSTHENES
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genSieve(int limit) {
    QVector<AlgoStep> steps;
    /* arr[i] = i+2 (valori 2..limit+1), barre proporzionali al valore */
    QVector<int> arr(limit); for (int i = 0; i < limit; i++) arr[i] = i + 2;
    QVector<bool> composite(limit + 2, false);
    QVector<int> primes;

    { AlgoStep s; s.arr = arr;
      s.msg = QString("Crivello di Eratostene — numeri 2..%1. Segna i multipli.").arg(limit+1);
      steps << s; }

    for (int p = 0; p < limit; p++) {
        if (composite[p]) continue;
        int prime = p + 2;
        { AlgoStep s; s.arr = arr; s.cmp << p;
          s.msg = QString("Primo trovato: %1. Segno i suoi multipli.").arg(prime);
          steps << s; }
        for (int m = prime*prime; m <= limit+1; m += prime) {
            if (m - 2 >= 0 && m - 2 < limit) {
                composite[m-2] = true;
                AlgoStep s; s.arr = arr; s.inactive << (m-2);
                for (int x : primes) if (!s.found.contains(x)) s.found << x;
                s.found << p;
                s.msg = QString("%1 = %2×%3 → composto").arg(m).arg(prime).arg(m/prime);
                steps << s;
            }
        }
        primes << p;
    }
    /* Step finale: mostra tutti i primi */
    AlgoStep sf; sf.arr = arr; sf.found = primes;
    for (int i = 0; i < limit; i++) if (!primes.contains(i)) sf.inactive << i;
    sf.msg = QString("Completato! Primi trovati: %1").arg(primes.size());
    steps << sf;
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   GCD EUCLIDEO
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genGCD(int a, int b) {
    QVector<AlgoStep> steps;
    /* arr = [a, b, r] come 3 barre */
    QVector<int> arr = {a, b, 0};
    { AlgoStep s; s.arr = arr;
      s.msg = QString("GCD(%1, %2) — Algoritmo di Euclide (~300 a.C.)").arg(a).arg(b);
      steps << s; }

    while (b != 0) {
        int r = a % b;
        arr = {a, b, r};
        AlgoStep s; s.arr = arr; s.cmp << 0 << 1;
        s.msg = QString("%1 = %2 × %3 + %4  →  GCD(%2,%4)")
                .arg(a).arg(b).arg(a/b).arg(r);
        steps << s;
        a = b; b = r;
    }
    arr = {a, 0, 0};
    AlgoStep s; s.arr = arr; s.found << 0;
    s.msg = QString("GCD = %1  (resto = 0, ci fermiamo)").arg(a);
    steps << s;
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   GCD ESTESO
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genExtGCD(int a, int b) {
    QVector<AlgoStep> steps;
    int orig_a = a, orig_b = b;
    int x0 = 1, x1 = 0, y0 = 0, y1 = 1;
    /* arr = [a, b, |x|, |y|] */
    QVector<int> arr = {a, b, abs(x0), abs(y0)};
    { AlgoStep s; s.arr = arr;
      s.msg = QString("GCD Esteso(%1, %2) — trova x,y: ax+by=GCD").arg(a).arg(b);
      steps << s; }

    while (b != 0) {
        int q = a / b;
        int r = a % b;
        int nx = x0 - q*x1, ny = y0 - q*y1;
        arr = {r, b, abs(nx), abs(ny)};
        AlgoStep s; s.arr = arr; s.cmp << 0 << 1;
        s.msg = QString("q=%1: r=%2, x=%3, y=%4  (verifica: %5×%6 + %7×%8 = %9)")
                .arg(q).arg(r).arg(nx).arg(ny)
                .arg(orig_a).arg(nx).arg(orig_b).arg(ny).arg(orig_a*nx + orig_b*ny);
        steps << s;
        a = b; b = r; x0 = x1; x1 = nx; y0 = y1; y1 = ny;
    }
    arr = {a, 0, abs(x0), abs(y0)};
    AlgoStep sf; sf.arr = arr; sf.found << 0;
    sf.msg = QString("GCD=%1, x=%2, y=%3  →  %4×%5 + %6×%7 = %1")
             .arg(a).arg(x0).arg(y0).arg(orig_a).arg(x0).arg(orig_b).arg(y0);
    steps << sf;
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   FAST EXPONENTIATION (Binary Exponentiation)
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genFastPow(int base_, int exp_) {
    QVector<AlgoStep> steps;
    long long result = 1, b = base_;
    int e = exp_;
    /* arr = [base, exp, result] scalati per visualizzazione */
    auto arr = [&]() -> QVector<int> {
        return { (int)qMin((long long)95, b % 100),
                 qMin(95, e),
                 (int)qMin((long long)95, result % 100 + 1) };
    };
    { AlgoStep s; s.arr = arr();
      s.msg = QString("Esponenziazione Veloce — %1^%2. Usa bit di exp.").arg(base_).arg(exp_);
      steps << s; }

    while (e > 0) {
        if (e & 1) {
            result *= b;
            AlgoStep s; s.arr = arr(); s.swp << 2;
            s.msg = QString("Bit=1: result×=%1 → result=%2 (mod 100)").arg(b%100).arg(result%100);
            steps << s;
        }
        b *= b; e >>= 1;
        AlgoStep s; s.arr = arr(); s.cmp << 0 << 1;
        s.msg = QString("base²=%1, exp>>=1 → exp=%2").arg(b%100).arg(e);
        steps << s;
    }
    AlgoStep sf; sf.arr = arr(); sf.found << 2;
    sf.msg = QString("%1^%2 = %3").arg(base_).arg(exp_).arg(result);
    steps << sf;
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   PRIME FACTORIZATION
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genPrimeFactors(int n) {
    QVector<AlgoStep> steps;
    QVector<int> factors;
    int orig = n;
    /* arr = fattori trovati + n rimanente */
    { QVector<int> arr = {n}; AlgoStep s; s.arr = arr;
      s.msg = QString("Fattorizzazione Prima di %1. Trial division fino a √n.").arg(n);
      steps << s; }

    for (int p = 2; (long long)p*p <= n; p++) {
        while (n % p == 0) {
            factors << p; n /= p;
            QVector<int> arr = factors; arr << n;
            for (int i = arr.size(); i < 8; i++) arr << 1;
            AlgoStep s; s.arr = arr; s.sorted << (int)factors.size()-1;
            s.msg = QString("%1 ÷ %2 = %3 (fattore trovato!)").arg(n*p).arg(p).arg(n);
            steps << s;
        }
    }
    if (n > 1) {
        factors << n;
        QVector<int> arr = factors;
        for (int i = arr.size(); i < 8; i++) arr << 1;
        AlgoStep s; s.arr = arr; s.found << (int)factors.size()-1;
        s.msg = QString("%1 è primo (fattore rimanente)").arg(n);
        steps << s;
    }
    QVector<int> arr = factors; for (int i = arr.size(); i < 8; i++) arr << 1;
    QVector<int> all; for (int i = 0; i < (int)factors.size(); i++) all << i;
    { AlgoStep s; s.arr = arr; s.sorted = all;
      QString f; for (int x : factors) f += QString::number(x) + "×";
      s.msg = QString("%1 = %2").arg(orig).arg(f.left(f.size()-1));
      steps << s; }
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   FIBONACCI DP (Bottom-Up Tabulation)
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genFibonacciDP(int n) {
    QVector<AlgoStep> steps;
    QVector<int> dp(n, 0);
    if (n >= 1) dp[0] = 1;
    if (n >= 2) dp[1] = 1;
    QVector<int> srt; if (n >= 1) srt << 0; if (n >= 2) srt << 1;
    { AlgoStep s; s.arr = dp; s.sorted = srt;
      s.msg = QString("Fibonacci DP — calcola F(0)..F(%1). F(0)=F(1)=1.").arg(n-1);
      steps << s; }

    for (int i = 2; i < n; i++) {
        int v = dp[i-1] + dp[i-2];
        dp[i] = qMin(95, v);  /* scala per visualizzazione */
        srt << i;
        AlgoStep s; s.arr = dp; s.sorted = srt; s.cmp << i-1 << i-2;
        s.msg = QString("F(%1) = F(%2)+F(%3) = %4+%5 = %6")
                .arg(i).arg(i-1).arg(i-2).arg(dp[i-1]).arg(dp[i-2]).arg(v);
        steps << s;
    }
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   PREFIX SUM
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genPrefixSum(QVector<int> arr) {
    QVector<AlgoStep> steps;
    const int n = arr.size();
    QVector<int> orig = arr;
    { AlgoStep s; s.arr = arr;
      s.msg = QString("Prefix Sum — %1 elem. Trasforma arr[i] in sum(0..i).").arg(n);
      steps << s; }

    QVector<int> srt;
    for (int i = 1; i < n; i++) {
        int old = arr[i];
        arr[i] += arr[i-1];
        srt << i-1;
        AlgoStep s; s.arr = arr; s.sorted = srt; s.cmp << i << i-1;
        s.msg = QString("prefix[%1] = prefix[%2](%3) + orig[%1](%4) = %5")
                .arg(i).arg(i-1).arg(arr[i-1]).arg(old).arg(arr[i]);
        steps << s;
    }
    srt << n-1;
    { AlgoStep s; s.arr = arr; s.sorted = srt;
      s.msg = "Prefix sum completa! Range query [l,r] = arr[r] - arr[l-1] in O(1).";
      steps << s; }
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   KADANE'S ALGORITHM
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genKadane(QVector<int> arr) {
    QVector<AlgoStep> steps;
    const int n = arr.size();
    { AlgoStep s; s.arr = arr;
      s.msg = QString("Kadane — %1 elem. Trova il sottovettore contiguo di somma massima.").arg(n);
      steps << s; }

    int maxSum = arr[0], curSum = arr[0];
    int maxL = 0, maxR = 0, curL = 0;
    QVector<int> bestRange, curRange;
    bestRange << 0; curRange << 0;

    for (int i = 1; i < n; i++) {
        if (curSum + arr[i] < arr[i]) {
            curL = i; curSum = arr[i]; curRange = {i};
        } else {
            curSum += arr[i]; curRange << i;
        }
        AlgoStep s; s.arr = arr; s.cmp = curRange;
        s.msg = QString("[%1]=%2: curSum=%3, maxSum=%4")
                .arg(i).arg(arr[i]).arg(curSum).arg(maxSum);
        steps << s;
        if (curSum > maxSum) {
            maxSum = curSum; maxL = curL; maxR = i;
            bestRange = curRange;
            AlgoStep sf; sf.arr = arr; sf.found = bestRange;
            sf.msg = QString("Nuovo massimo! somma=%1 in [%2..%3]").arg(maxSum).arg(maxL).arg(maxR);
            steps << sf;
        }
    }
    { AlgoStep sf; sf.arr = arr; sf.sorted = bestRange;
      sf.msg = QString("Max subarray [%1..%2] con somma %3").arg(maxL).arg(maxR).arg(maxSum);
      steps << sf; }
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   HORNER'S METHOD
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genHorner(QVector<int> coeffs, int x) {
    QVector<AlgoStep> steps;
    const int n = coeffs.size();
    /* arr = coefficienti, result come ultima barra */
    QVector<int> arr = coeffs;
    { AlgoStep s; s.arr = arr;
      s.msg = QString("Horner — p(x=%1) con coefficienti a[n]..a[0]. Valuta in O(n).").arg(x);
      steps << s; }

    long long result = coeffs[0];
    for (int i = 1; i < n; i++) {
        AlgoStep s; s.arr = arr; s.cmp << i;
        s.msg = QString("p = p×%1 + %2 = %3×%4 + %5 = %6")
                .arg(x).arg(coeffs[i]).arg(result).arg(x).arg(coeffs[i]).arg(result*x + coeffs[i]);
        result = result * x + coeffs[i];
        arr[i] = (int)qMin((long long)95, qAbs(result) % 100 + 1);
        s.arr = arr; s.sorted << i;
        steps << s;
    }
    AlgoStep sf; sf.arr = arr; sf.found << n-1;
    sf.msg = QString("p(%1) = %2").arg(x).arg(result);
    steps << sf;
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   SLIDING WINDOW (Max Sum di K elementi)
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genSlidingWindow(QVector<int> arr, int k) {
    QVector<AlgoStep> steps;
    const int n = arr.size();
    { AlgoStep s; s.arr = arr;
      s.msg = QString("Sliding Window — max somma di %1 elementi contigui.").arg(k);
      steps << s; }

    int winSum = 0;
    for (int i = 0; i < k; i++) winSum += arr[i];
    int maxSum = winSum;
    QVector<int> win; for (int i = 0; i < k; i++) win << i;
    QVector<int> bestWin = win;
    { AlgoStep s; s.arr = arr; s.cmp = win;
      s.msg = QString("Finestra [0..%1] somma=%2").arg(k-1).arg(winSum);
      steps << s; }

    for (int i = k; i < n; i++) {
        winSum += arr[i] - arr[i-k];
        win.clear(); for (int j = i-k+1; j <= i; j++) win << j;
        AlgoStep s; s.arr = arr; s.cmp = win;
        s.msg = QString("Scorri: aggiungi [%1]=%2 rimuovi [%3]=%4 → somma=%5")
                .arg(i).arg(arr[i]).arg(i-k).arg(arr[i-k]).arg(winSum);
        steps << s;
        if (winSum > maxSum) {
            maxSum = winSum; bestWin = win;
            AlgoStep sf; sf.arr = arr; sf.found = bestWin;
            sf.msg = QString("Nuovo massimo! somma=%1 in [%2..%3]").arg(maxSum).arg(i-k+1).arg(i);
            steps << sf;
        }
    }
    { AlgoStep sf; sf.arr = arr; sf.sorted = bestWin;
      sf.msg = QString("Max sum window: [%1..%2] = %3").arg(bestWin.first()).arg(bestWin.last()).arg(maxSum);
      steps << sf; }
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   DUTCH NATIONAL FLAG (3-way partition)
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genDutchFlag(QVector<int> arr) {
    QVector<AlgoStep> steps;
    /* Forza valori 0,1,2 per la dimostrazione */
    for (int& v : arr) v = rand() % 3;
    const int n = arr.size();
    { AlgoStep s; s.arr = arr;
      s.msg = QString("Dutch National Flag (Dijkstra) — %1 elem con valori 0,1,2. 1 passata.").arg(n);
      steps << s; }

    int lo = 0, mid = 0, hi = n - 1;
    while (mid <= hi) {
        AlgoStep s; s.arr = arr; s.cmp << mid;
        s.msg = QString("mid=%1 val=%2 | lo=%3 mid=%4 hi=%5")
                .arg(mid).arg(arr[mid]).arg(lo).arg(mid).arg(hi);
        steps << s;
        if (arr[mid] == 0) {
            std::swap(arr[lo], arr[mid]);
            AlgoStep sw; sw.arr = arr; sw.swp << lo << mid;
            sw.msg = QString("0 trovato: scambia [%1]↔[%2]").arg(lo).arg(mid);
            steps << sw; lo++; mid++;
        } else if (arr[mid] == 1) {
            AlgoStep si; si.arr = arr; si.found << mid;
            si.msg = "1 già nella posizione giusta.";
            steps << si; mid++;
        } else {
            std::swap(arr[mid], arr[hi]);
            AlgoStep sw; sw.arr = arr; sw.swp << mid << hi;
            sw.msg = QString("2 trovato: scambia [%1]↔[%2]").arg(mid).arg(hi);
            steps << sw; hi--;
        }
    }
    QVector<int> all; for (int i = 0; i < n; i++) all << i;
    { AlgoStep s; s.arr = arr; s.sorted = all; s.msg = "Dutch National Flag completato!"; steps << s; }
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   TRAPPING RAIN WATER
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genTrappingRain(QVector<int> arr) {
    QVector<AlgoStep> steps;
    /* Crea profilo simile a un paesaggio */
    for (int& v : arr) v = 5 + rand() % 45;
    const int n = arr.size();
    { AlgoStep s; s.arr = arr;
      s.msg = QString("Trapping Rain Water — %1 barre. Two Pointers O(n).").arg(n);
      steps << s; }

    int l = 0, r = n-1, lMax = 0, rMax = 0, water = 0;
    QVector<int> trapped;
    while (l < r) {
        AlgoStep s; s.arr = arr; s.cmp << l << r;
        s.msg = QString("L=%1(%2) R=%3(%4) | lMax=%5 rMax=%6 | acqua=%7")
                .arg(l).arg(arr[l]).arg(r).arg(arr[r]).arg(lMax).arg(rMax).arg(water);
        steps << s;
        if (arr[l] <= arr[r]) {
            if (arr[l] >= lMax) lMax = arr[l];
            else { water += lMax - arr[l]; trapped << l;
                AlgoStep sw; sw.arr = arr; sw.found = trapped;
                sw.msg = QString("[%1]: +%2 litri (lMax=%3 - h=%4)")
                         .arg(l).arg(lMax-arr[l]).arg(lMax).arg(arr[l]);
                steps << sw; }
            l++;
        } else {
            if (arr[r] >= rMax) rMax = arr[r];
            else { water += rMax - arr[r]; trapped << r;
                AlgoStep sw; sw.arr = arr; sw.found = trapped;
                sw.msg = QString("[%1]: +%2 litri (rMax=%3 - h=%4)")
                         .arg(r).arg(rMax-arr[r]).arg(rMax).arg(arr[r]);
                steps << sw; }
            r--;
        }
    }
    { AlgoStep sf; sf.arr = arr; sf.sorted = trapped;
      sf.msg = QString("Acqua totale intrappolata: %1 unit\xc3\xa0").arg(water);
      steps << sf; }
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   NEXT GREATER ELEMENT (Monotonic Stack)
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genNextGreater(QVector<int> arr) {
    QVector<AlgoStep> steps;
    const int n = arr.size();
    QVector<int> result(n, -1);
    QVector<int> stk; /* stack di indici */
    { AlgoStep s; s.arr = arr;
      s.msg = "Next Greater Element — stack monotono decrescente. O(n).";
      steps << s; }

    for (int i = 0; i < n; i++) {
        while (!stk.isEmpty() && arr[stk.last()] < arr[i]) {
            int idx = stk.takeLast();
            result[idx] = arr[i];
            AlgoStep s; s.arr = arr; s.found << idx; s.cmp << i;
            s.msg = QString("NGE([%1]=%2) = [%3]=%4 ✓").arg(idx).arg(arr[idx]).arg(i).arg(arr[i]);
            steps << s;
        }
        stk << i;
        AlgoStep s; s.arr = arr; s.cmp << i;
        s.msg = QString("Push [%1]=%2 nello stack").arg(i).arg(arr[i]);
        steps << s;
    }
    /* Mostra risultato finale */
    QVector<int> found_all;
    for (int i = 0; i < n; i++) if (result[i] != -1) found_all << i;
    { AlgoStep sf; sf.arr = arr; sf.found = found_all;
      sf.msg = "NGE completato! Elementi verdi hanno un NGE; gli altri (-1) non ce l'hanno.";
      steps << sf; }
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   FISHER-YATES SHUFFLE
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genFisherYates(QVector<int> arr) {
    QVector<AlgoStep> steps;
    const int n = arr.size();
    { AlgoStep s; s.arr = arr;
      s.msg = "Fisher-Yates Shuffle. Ogni permutazione ha probabilità 1/n! esatta.";
      steps << s; }

    QVector<int> srt;
    for (int i = n - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        AlgoStep s; s.arr = arr; s.cmp << i << j; s.sorted = srt;
        s.msg = QString("i=%1: scelgo j=%2 casuale in [0..%3]").arg(i).arg(j).arg(i);
        steps << s;
        if (i != j) {
            std::swap(arr[i], arr[j]);
            AlgoStep sw; sw.arr = arr; sw.swp << i << j; sw.sorted = srt;
            sw.msg = QString("Scambio [%1]↔[%2]").arg(i).arg(j);
            steps << sw;
        }
        srt << i;
    }
    srt << 0;
    { AlgoStep sf; sf.arr = arr; sf.found = srt;
      sf.msg = "Shuffle completato! Array completamente rimescolato.";
      steps << sf; }
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   QUICKSELECT (K-esimo elemento)
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genQuickSelect(QVector<int> arr, int k) {
    QVector<AlgoStep> steps;
    const int n = arr.size();
    { AlgoStep s; s.arr = arr;
      s.msg = QString("Quickselect — trova il %1\xc2\xb0 elemento più piccolo in O(n) avg.").arg(k+1);
      steps << s; }

    int lo = 0, hi = n - 1;
    while (lo <= hi) {
        int pivot = arr[hi];
        int i = lo - 1;
        { AlgoStep s; s.arr = arr; s.cmp << hi;
          s.msg = QString("Partizione [%1..%2] pivot=%3").arg(lo).arg(hi).arg(pivot);
          steps << s; }
        for (int j = lo; j < hi; j++) {
            if (arr[j] <= pivot) {
                i++;
                if (i != j) { std::swap(arr[i], arr[j]);
                    AlgoStep sw; sw.arr = arr; sw.swp << i << j;
                    sw.msg = QString("Scambio [%1]↔[%2]").arg(i).arg(j);
                    steps << sw; }
            }
        }
        std::swap(arr[i+1], arr[hi]);
        int p = i + 1;
        { AlgoStep s; s.arr = arr; s.sorted << p;
          s.msg = QString("Pivot=%1 in posizione [%2]").arg(pivot).arg(p);
          steps << s; }
        if (p == k) {
            AlgoStep sf; sf.arr = arr; sf.found << p;
            sf.msg = QString("Trovato! Il %1\xc2\xb0 elemento è %2").arg(k+1).arg(arr[p]);
            steps << sf; return steps;
        }
        if (p < k) lo = p + 1; else hi = p - 1;
    }
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   STOCK MAX PROFIT
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genStockProfit(QVector<int> arr) {
    QVector<AlgoStep> steps;
    const int n = arr.size();
    { AlgoStep s; s.arr = arr;
      s.msg = "Stock Max Profit — trova il giorno migliore di acquisto e vendita.";
      steps << s; }

    int minIdx = 0, bestBuy = 0, bestSell = 0, maxProfit = 0;
    for (int i = 1; i < n; i++) {
        if (arr[i] < arr[minIdx]) {
            minIdx = i;
            AlgoStep s; s.arr = arr; s.cmp << i;
            s.msg = QString("Nuovo minimo: compra a [%1]=%2").arg(i).arg(arr[i]);
            steps << s;
        } else {
            int profit = arr[i] - arr[minIdx];
            AlgoStep s; s.arr = arr; s.cmp << i << minIdx;
            s.msg = QString("Vendi a [%1]=%2, compra a [%3]=%4 → profitto=%5")
                    .arg(i).arg(arr[i]).arg(minIdx).arg(arr[minIdx]).arg(profit);
            steps << s;
            if (profit > maxProfit) {
                maxProfit = profit; bestBuy = minIdx; bestSell = i;
                AlgoStep sf; sf.arr = arr; sf.sorted << bestBuy; sf.found << bestSell;
                sf.msg = QString("Nuovo massimo! Compra [%1]=%2, vendi [%3]=%4, profitto=%5")
                         .arg(bestBuy).arg(arr[bestBuy]).arg(bestSell).arg(arr[bestSell]).arg(maxProfit);
                steps << sf;
            }
        }
    }
    { AlgoStep sf; sf.arr = arr; sf.sorted << bestBuy; sf.found << bestSell;
      sf.msg = QString("Soluzione: compra giorno %1 (%2), vendi giorno %3 (%4) → +%5")
               .arg(bestBuy).arg(arr[bestBuy]).arg(bestSell).arg(arr[bestSell]).arg(maxProfit);
      steps << sf; }
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   GRAFI: grafo fisso 8 nodi
   Adiacenza (pesata): 0-1(4), 0-2(1), 1-3(2), 1-4(5),
                       2-3(8), 2-5(2), 3-6(3), 4-7(6),
                       5-6(1), 6-7(4)
   Rappresentazione barre: arr[i] = distanza/stato nodo i
   ══════════════════════════════════════════════════════════════ */

