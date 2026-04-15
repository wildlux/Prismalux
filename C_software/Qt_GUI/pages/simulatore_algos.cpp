/*
 * simulatore_algos.cpp — Implementazione dei 110 algoritmi
 * ===========================================================
 * randomArr + tutti i gen*() + helper interni (_qs, _heapify, _flip, ...).
 * Nessuna dipendenza da Qt UI — solo QVector<AlgoStep>.
 * La UI del simulatore è in simulatore_page.cpp.
 */
#include "pages/simulatore_page.h"
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
static const int GRAPH_N = 8;
/* adj[u] = lista di (v, peso) */
static const int GRAPH_EDGES[][3] = {
    {0,1,4},{0,2,1},{1,3,2},{1,4,5},{2,3,8},{2,5,2},{3,6,3},{4,7,6},{5,6,1},{6,7,4}
};
static const int GRAPH_NE = 10;

/* Costruisce lista di adiacenza non pesata */
static QVector<QVector<int>> makeAdj() {
    QVector<QVector<int>> adj(GRAPH_N);
    for (int i = 0; i < GRAPH_NE; i++) {
        adj[GRAPH_EDGES[i][0]] << GRAPH_EDGES[i][1];
        adj[GRAPH_EDGES[i][1]] << GRAPH_EDGES[i][0];
    }
    return adj;
}

QVector<AlgoStep> SimulatorePage::genBFS() {
    QVector<AlgoStep> steps;
    QVector<int> dist(GRAPH_N, 99);
    dist[0] = 0;
    QVector<int> visited;
    auto adj = makeAdj();

    { AlgoStep s; s.arr = dist;
      s.msg = "BFS — 8 nodi. Barre = distanza dal nodo 0. Esplora per livelli.";
      steps << s; }

    QVector<int> queue = {0};
    while (!queue.isEmpty()) {
        int u = queue.takeFirst(); visited << u;
        AlgoStep s; s.arr = dist; s.cmp << u; s.sorted = visited;
        s.msg = QString("Visito nodo %1 (dist=%2)").arg(u).arg(dist[u]);
        steps << s;
        for (int v : adj[u]) {
            if (dist[v] == 99) {
                dist[v] = dist[u] + 1; queue << v;
                AlgoStep sv; sv.arr = dist; sv.found << v; sv.sorted = visited;
                sv.msg = QString("Scoperto nodo %1 (dist=%2) da %3").arg(v).arg(dist[v]).arg(u);
                steps << sv;
            }
        }
    }
    { AlgoStep sf; sf.arr = dist; sf.sorted = visited;
      sf.msg = "BFS completata! Distanze minime da nodo 0 calcolate.";
      steps << sf; }
    return steps;
}

QVector<AlgoStep> SimulatorePage::genDFS() {
    QVector<AlgoStep> steps;
    QVector<int> time_(GRAPH_N, 0);
    QVector<bool> vis(GRAPH_N, false);
    auto adj = makeAdj();
    QVector<int> visited;
    int t = 0;

    { AlgoStep s; s.arr = time_;
      s.msg = "DFS — 8 nodi. Barre = timestamp di scoperta. Esplora in profondità.";
      steps << s; }

    std::function<void(int)> dfs = [&](int u) {
        vis[u] = true; time_[u] = ++t; visited << u;
        AlgoStep s; s.arr = time_; s.cmp << u; s.sorted = visited;
        s.msg = QString("Scopro nodo %1 (time=%2)").arg(u).arg(t);
        steps << s;
        for (int v : adj[u]) {
            if (!vis[v]) {
                AlgoStep se; se.arr = time_; se.cmp << u << v; se.sorted = visited;
                se.msg = QString("Arco albero %1→%2").arg(u).arg(v);
                steps << se;
                dfs(v);
                AlgoStep sr; sr.arr = time_; sr.found << u; sr.sorted = visited;
                sr.msg = QString("Ritorno a %1 da %2").arg(u).arg(v);
                steps << sr;
            }
        }
    };
    dfs(0);
    { AlgoStep sf; sf.arr = time_; sf.sorted = visited;
      sf.msg = "DFS completata! Tutti i nodi raggiunti.";
      steps << sf; }
    return steps;
}

QVector<AlgoStep> SimulatorePage::genDijkstra() {
    QVector<AlgoStep> steps;
    const int INF = 99;
    QVector<int> dist(GRAPH_N, INF); dist[0] = 0;
    QVector<bool> vis(GRAPH_N, false);
    QVector<int> settled;

    { AlgoStep s; s.arr = dist;
      s.msg = "Dijkstra — 8 nodi pesati. Barre = distanza minima dal nodo 0.";
      steps << s; }

    for (int iter = 0; iter < GRAPH_N; iter++) {
        /* trova nodo non visitato con dist minima */
        int u = -1;
        for (int i = 0; i < GRAPH_N; i++)
            if (!vis[i] && (u == -1 || dist[i] < dist[u])) u = i;
        if (u == -1 || dist[u] == INF) break;
        vis[u] = true; settled << u;
        AlgoStep s; s.arr = dist; s.cmp << u; s.sorted = settled;
        s.msg = QString("Estrai nodo %1 (dist=%2) — definitivo").arg(u).arg(dist[u]);
        steps << s;

        for (int e = 0; e < GRAPH_NE; e++) {
            int a = GRAPH_EDGES[e][0], b = GRAPH_EDGES[e][1], w = GRAPH_EDGES[e][2];
            int v = (a == u) ? b : (b == u) ? a : -1;
            if (v == -1 || vis[v]) continue;
            if (dist[u] + w < dist[v]) {
                dist[v] = dist[u] + w;
                AlgoStep sv; sv.arr = dist; sv.found << v; sv.sorted = settled;
                sv.msg = QString("Rilassa %1→%2 (peso=%3): dist[%2]=%4").arg(u).arg(v).arg(w).arg(dist[v]);
                steps << sv;
            }
        }
    }
    { AlgoStep sf; sf.arr = dist; sf.sorted = settled;
      sf.msg = "Dijkstra completato! Tutti i cammini minimi da nodo 0.";
      steps << sf; }
    return steps;
}

QVector<AlgoStep> SimulatorePage::genBellmanFord() {
    QVector<AlgoStep> steps;
    const int INF = 99;
    QVector<int> dist(GRAPH_N, INF); dist[0] = 0;

    { AlgoStep s; s.arr = dist;
      s.msg = "Bellman-Ford — 8 nodi. Rilassa tutti gli archi V-1 = 7 volte.";
      steps << s; }

    for (int iter = 1; iter < GRAPH_N; iter++) {
        bool changed = false;
        for (int e = 0; e < GRAPH_NE; e++) {
            int u = GRAPH_EDGES[e][0], v = GRAPH_EDGES[e][1], w = GRAPH_EDGES[e][2];
            for (int dir = 0; dir < 2; dir++) {
                if (dir == 1) { std::swap(u, v); }
                if (dist[u] != INF && dist[u] + w < dist[v]) {
                    dist[v] = dist[u] + w; changed = true;
                    AlgoStep s; s.arr = dist; s.swp << v; s.cmp << u;
                    s.msg = QString("Iter %1: rilassa %2→%3 (w=%4) dist[%3]=%5")
                            .arg(iter).arg(u).arg(v).arg(w).arg(dist[v]);
                    steps << s;
                }
            }
        }
        if (!changed) {
            AlgoStep s; s.arr = dist;
            s.msg = QString("Iter %1: nessuna modifica — terminato in anticipo!").arg(iter);
            steps << s; break;
        }
    }
    QVector<int> all; for (int i = 0; i < GRAPH_N; i++) all << i;
    { AlgoStep sf; sf.arr = dist; sf.sorted = all;
      sf.msg = "Bellman-Ford completato! Nessun ciclo negativo rilevato.";
      steps << sf; }
    return steps;
}

QVector<AlgoStep> SimulatorePage::genTopologicalSort() {
    QVector<AlgoStep> steps;
    /* DAG: 0→1, 0→2, 1→3, 1→4, 2→4, 2→5, 3→6, 4→6, 5→7, 6→7 */
    QVector<QVector<int>> dag(GRAPH_N);
    int dagEdges[][2] = {{0,1},{0,2},{1,3},{1,4},{2,4},{2,5},{3,6},{4,6},{5,7},{6,7}};
    QVector<int> inDeg(GRAPH_N, 0);
    for (auto& e : dagEdges) { dag[e[0]] << e[1]; inDeg[e[1]]++; }

    QVector<int> topoOrder(GRAPH_N, 0);
    int pos = 0;
    QVector<int> queue, settled;
    for (int i = 0; i < GRAPH_N; i++) if (inDeg[i] == 0) queue << i;

    { AlgoStep s; s.arr = inDeg;
      s.msg = "Topological Sort (Kahn) — barre = in-degree. DAG: 0→1→3→6→7...";
      steps << s; }

    while (!queue.isEmpty()) {
        int u = queue.takeFirst(); topoOrder[u] = pos++;
        settled << u;
        AlgoStep s; s.arr = inDeg; s.found << u; s.sorted = settled;
        s.msg = QString("Elaboro nodo %1 (in-degree=0, pos=%2)").arg(u).arg(pos-1);
        steps << s;
        for (int v : dag[u]) {
            inDeg[v]--;
            AlgoStep sv; sv.arr = inDeg; sv.cmp << v; sv.sorted = settled;
            sv.msg = QString("Rimuovi arco %1→%2: in-degree[%2]=%3").arg(u).arg(v).arg(inDeg[v]);
            steps << sv;
            if (inDeg[v] == 0) queue << v;
        }
    }
    { AlgoStep sf; sf.arr = inDeg; sf.sorted = settled;
      sf.msg = "Topological Sort completato! Ordine: 0,1,2,3,4,5,6,7";
      steps << sf; }
    return steps;
}

QVector<AlgoStep> SimulatorePage::genUnionFind() {
    QVector<AlgoStep> steps;
    /* parent[i] = i all'inizio */
    QVector<int> parent(GRAPH_N);
    QVector<int> rank_(GRAPH_N, 0);
    for (int i = 0; i < GRAPH_N; i++) parent[i] = i;

    { AlgoStep s; s.arr = parent;
      s.msg = "Union-Find — barre = parent[i]. Inizialmente ogni nodo è radice di sé stesso.";
      steps << s; }

    std::function<int(int)> find = [&](int x) -> int {
        while (parent[x] != x) x = parent[x] = parent[parent[x]]; /* path compression */
        return x;
    };

    for (int e = 0; e < GRAPH_NE && e < 6; e++) {
        int u = GRAPH_EDGES[e][0], v = GRAPH_EDGES[e][1];
        int ru = find(u), rv = find(v);
        AlgoStep s; s.arr = parent; s.cmp << u << v;
        s.msg = QString("Union(%1,%2): radici %3 e %4").arg(u).arg(v).arg(ru).arg(rv);
        steps << s;
        if (ru != rv) {
            if (rank_[ru] < rank_[rv]) std::swap(ru, rv);
            parent[rv] = ru;
            if (rank_[ru] == rank_[rv]) rank_[ru]++;
            AlgoStep su; su.arr = parent; su.swp << rv;
            su.msg = QString("parent[%1] = %2 (union by rank)").arg(rv).arg(ru);
            steps << su;
        } else {
            AlgoStep sc; sc.arr = parent; sc.found << ru;
            sc.msg = QString("Stesso componente! %1 e %2 già connessi.").arg(u).arg(v);
            steps << sc;
        }
    }
    { AlgoStep sf; sf.arr = parent;
      sf.msg = "Union-Find completato! parent[i] = rappresentante del componente.";
      steps << sf; }
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   COIN CHANGE DP
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genCoinChange() {
    QVector<AlgoStep> steps;
    QVector<int> coins = {1, 3, 4};
    const int amount = 12;
    QVector<int> dp(amount + 1, 99);
    dp[0] = 0;

    { AlgoStep s; s.arr = dp.mid(0, qMin(13, dp.size()));
      s.msg = QString("Coin Change — monete {1,3,4}, importo=%1. dp[i]=min monete per i.").arg(amount);
      steps << s; }

    QVector<int> srt = {0};
    for (int i = 1; i <= amount; i++) {
        for (int c : coins) {
            if (c <= i && dp[i-c] + 1 < dp[i]) {
                dp[i] = dp[i-c] + 1;
                AlgoStep s; s.arr = dp.mid(0,13); s.cmp << i; s.sorted = srt;
                s.msg = QString("dp[%1] = dp[%2]+1 = %3 (moneta %4)").arg(i).arg(i-c).arg(dp[i]).arg(c);
                steps << s;
            }
        }
        srt << i;
        AlgoStep s; s.arr = dp.mid(0,13); s.sorted = srt;
        s.msg = QString("dp[%1] = %2 monete").arg(i).arg(dp[i] == 99 ? -1 : dp[i]);
        steps << s;
    }
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   LIS — Longest Increasing Subsequence (DP + BinarySearch)
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genLIS(QVector<int> arr) {
    QVector<AlgoStep> steps;
    const int n = arr.size();
    { AlgoStep s; s.arr = arr;
      s.msg = "LIS — Longest Increasing Subsequence. Patience sorting O(n log n).";
      steps << s; }

    QVector<int> piles; /* piles[i] = top della pila i */
    QVector<int> lisIdx; /* per visualizzazione */
    for (int i = 0; i < n; i++) {
        /* Binary search per trovare la pila giusta */
        int lo = 0, hi = (int)piles.size();
        while (lo < hi) {
            int mid = (lo + hi) / 2;
            if (piles[mid] < arr[i]) lo = mid + 1; else hi = mid;
        }
        if (lo == (int)piles.size()) piles << arr[i];
        else piles[lo] = arr[i];

        QVector<int> pilesVis = piles;
        while ((int)pilesVis.size() < n) pilesVis << 0;
        AlgoStep s; s.arr = pilesVis; s.cmp << lo;
        s.msg = QString("[%1]=%2 → pila %3 (LIS lunghezza=%4)").arg(i).arg(arr[i]).arg(lo).arg(piles.size());
        steps << s;
    }
    QVector<int> finalSrt; for (int i = 0; i < (int)piles.size(); i++) finalSrt << i;
    QVector<int> pilesVis = piles; while ((int)pilesVis.size() < n) pilesVis << 0;
    { AlgoStep sf; sf.arr = pilesVis; sf.sorted = finalSrt;
      sf.msg = QString("LIS completato! Lunghezza = %1").arg(piles.size());
      steps << sf; }
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   0/1 KNAPSACK
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genKnapsack() {
    QVector<AlgoStep> steps;
    QVector<int> weights = {2, 3, 4, 5};
    QVector<int> values  = {3, 4, 5, 8};
    const int W = 8, n = 4;
    /* dp[w] = max valore con capacità w */
    QVector<int> dp(W+1, 0);

    { AlgoStep s; s.arr = dp;
      s.msg = "0/1 Knapsack — 4 oggetti, capacità=8. dp[w]=valore massimo.";
      steps << s; }

    QVector<int> srt;
    for (int i = 0; i < n; i++) {
        /* Scorri a ritroso per evitare doppio uso */
        for (int w = W; w >= weights[i]; w--) {
            int newVal = dp[w - weights[i]] + values[i];
            if (newVal > dp[w]) {
                dp[w] = newVal;
                AlgoStep s; s.arr = dp; s.cmp << w; s.sorted = srt;
                s.msg = QString("Oggetto %1 (w=%2,v=%3): dp[%4]=%5")
                        .arg(i+1).arg(weights[i]).arg(values[i]).arg(w).arg(newVal);
                steps << s;
            }
        }
        srt << i+1;
        AlgoStep s; s.arr = dp;
        s.msg = QString("Dopo oggetto %1: valore massimo = %2").arg(i+1).arg(*std::max_element(dp.begin(), dp.end()));
        steps << s;
    }
    int maxV = *std::max_element(dp.begin(), dp.end());
    QVector<int> all; for (int i = 0; i <= W; i++) all << i;
    { AlgoStep sf; sf.arr = dp; sf.sorted = all;
      sf.msg = QString("Knapsack completato! Valore massimo = %1").arg(maxV);
      steps << sf; }
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   LCS — Longest Common Subsequence
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genLCS() {
    QVector<AlgoStep> steps;
    QString s1 = "ABCBDAB", s2 = "BDCAB";
    const int m = s1.size(), n = s2.size();
    /* Mostra la riga corrente della tabella DP */
    QVector<int> prev(n+1, 0), curr(n+1, 0);
    QVector<int> srt;

    { QVector<int> arr(n+1, 0); AlgoStep s; s.arr = arr;
      s.msg = QString("LCS(\"%1\", \"%2\"). Tabella DP riga per riga.").arg(s1).arg(s2);
      steps << s; }

    for (int i = 1; i <= m; i++) {
        for (int j = 1; j <= n; j++) {
            if (s1[i-1] == s2[j-1]) {
                curr[j] = prev[j-1] + 1;
                AlgoStep s; s.arr = curr; s.found << j;
                s.msg = QString("s1[%1]='%2'=s2[%3]='%4': dp[%5][%6]=%7 ✓")
                        .arg(i-1).arg(s1[i-1]).arg(j-1).arg(s2[j-1]).arg(i).arg(j).arg(curr[j]);
                steps << s;
            } else {
                curr[j] = qMax(prev[j], curr[j-1]);
                AlgoStep s; s.arr = curr; s.cmp << j;
                s.msg = QString("s1[%1]='%2'≠s2[%3]='%4': dp=%5")
                        .arg(i-1).arg(s1[i-1]).arg(j-1).arg(s2[j-1]).arg(curr[j]);
                steps << s;
            }
        }
        prev = curr; std::fill(curr.begin(), curr.end(), 0); curr[0] = 0;
        AlgoStep s; s.arr = prev;
        s.msg = QString("Riga %1 ('%2') completata. LCS parziale=%3").arg(i).arg(s1[i-1]).arg(prev[n]);
        steps << s;
    }
    QVector<int> all; for (int j = 0; j <= n; j++) all << j;
    { AlgoStep sf; sf.arr = prev; sf.sorted = all;
      sf.msg = QString("LCS(\"%1\",\"%2\") = %3 caratteri").arg(s1).arg(s2).arg(prev[n]);
      steps << sf; }
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   EDIT DISTANCE (Levenshtein)
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genEditDistance() {
    QVector<AlgoStep> steps;
    QString s1 = "KITTEN", s2 = "SITTING";
    const int m = s1.size(), n = s2.size();
    QVector<int> prev(n+1), curr(n+1);
    for (int j = 0; j <= n; j++) prev[j] = j;

    { AlgoStep s; s.arr = prev;
      s.msg = QString("Edit Distance(\"%1\"→\"%2\"). Operazioni: ins, del, sost.").arg(s1).arg(s2);
      steps << s; }

    for (int i = 1; i <= m; i++) {
        curr[0] = i;
        for (int j = 1; j <= n; j++) {
            int cost = (s1[i-1] == s2[j-1]) ? 0 : 1;
            curr[j] = qMin(prev[j]+1, qMin(curr[j-1]+1, prev[j-1]+cost));
            AlgoStep s; s.arr = curr; s.cmp << j;
            s.msg = QString("s1[%1]='%2' vs s2[%3]='%4': cost=%5, dp=%6")
                    .arg(i-1).arg(s1[i-1]).arg(j-1).arg(s2[j-1]).arg(cost).arg(curr[j]);
            steps << s;
        }
        prev = curr;
        AlgoStep s; s.arr = prev;
        s.msg = QString("Riga '%1' completata — dist parziale=%2").arg(s1[i-1]).arg(prev[n]);
        steps << s;
    }
    QVector<int> all; for (int j = 0; j <= n; j++) all << j;
    { AlgoStep sf; sf.arr = prev; sf.sorted = all;
      sf.msg = QString("Edit Distance(\"%1\",\"%2\") = %3 operazioni").arg(s1).arg(s2).arg(prev[n]);
      steps << sf; }
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   RESERVOIR SAMPLING
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genReservoirSampling(QVector<int> arr) {
    QVector<AlgoStep> steps;
    const int n = arr.size(), k = 3;
    { AlgoStep s; s.arr = arr;
      s.msg = QString("Reservoir Sampling — stream di %1 elem, mantieni k=%2 campioni uniformi.").arg(n).arg(k);
      steps << s; }

    QVector<int> reservoir;
    for (int i = 0; i < k && i < n; i++) { reservoir << arr[i]; }
    QVector<int> resIdx; for (int i = 0; i < k; i++) resIdx << i;
    { AlgoStep s; s.arr = arr; s.sorted = resIdx;
      s.msg = QString("Reservoir iniziale: elementi 0..%1").arg(k-1);
      steps << s; }

    for (int i = k; i < n; i++) {
        int j = rand() % (i + 1);
        AlgoStep s; s.arr = arr; s.cmp << i;
        s.msg = QString("Elemento [%1]=%2: j=%3 (casuale in [0..%4])").arg(i).arg(arr[i]).arg(j).arg(i);
        steps << s;
        if (j < k) {
            resIdx[j] = i;
            AlgoStep sw; sw.arr = arr; sw.swp << i; sw.sorted = resIdx;
            sw.msg = QString("j=%1 < k=%2: sostituisce reservoir[%3] con [%4]=%5").arg(j).arg(k).arg(j).arg(i).arg(arr[i]);
            steps << sw;
        } else {
            AlgoStep sk; sk.arr = arr; sk.inactive << i; sk.sorted = resIdx;
            sk.msg = QString("j=%1 >= k=%2: scartato").arg(j).arg(k);
            steps << sk;
        }
    }
    { AlgoStep sf; sf.arr = arr; sf.found = resIdx;
      sf.msg = QString("Campionamento completato! %1 elementi con prob. esatta k/n = %2/%3")
               .arg(k).arg(k).arg(n);
      steps << sf; }
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   FLOYD'S CYCLE DETECTION (Tortoise and Hare)
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genFloydCycle() {
    QVector<AlgoStep> steps;
    /* Lista come array: next[i] = prossimo nodo. Ciclo in posizione 3 */
    QVector<int> next_ = {1, 2, 3, 4, 5, 3, 3, 0}; /* ciclo: 3→4→5→3 */
    const int n = next_.size();
    { AlgoStep s; s.arr = next_;
      s.msg = "Floyd Cycle Detection — barre = next[i]. Ciclo: 3→4→5→3.";
      steps << s; }

    int tortoise = next_[0], hare = next_[next_[0]];
    int step = 1;
    while (tortoise != hare) {
        AlgoStep s; s.arr = next_; s.cmp << tortoise << hare;
        s.msg = QString("Step %1: tartaruga=%2, lepre=%3").arg(step++).arg(tortoise).arg(hare);
        steps << s;
        tortoise = next_[tortoise];
        hare     = next_[next_[hare]];
    }
    { AlgoStep s; s.arr = next_; s.found << tortoise;
      s.msg = QString("Incontro nel nodo %1 — ciclo rilevato!").arg(tortoise);
      steps << s; }

    /* Trova inizio ciclo */
    int mu = 0; tortoise = 0;
    while (tortoise != hare) {
        tortoise = next_[tortoise]; hare = next_[hare]; mu++;
        AlgoStep s; s.arr = next_; s.cmp << tortoise << hare;
        s.msg = QString("Cerca inizio ciclo: t=%1 h=%2").arg(tortoise).arg(hare);
        steps << s;
    }
    { AlgoStep sf; sf.arr = next_; sf.sorted << tortoise;
      sf.msg = QString("Inizio ciclo: nodo %1, lunghezza ciclo calcolata.").arg(tortoise);
      steps << sf; }
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   TORRE DI HANOI (ricorsiva, n dischi, 3 pioli come barre)
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genTowerOfHanoi(int n) {
    QVector<AlgoStep> steps;
    /* Codifica stato: barre = [top piolo A, top piolo B, top piolo C, mosse, dischi totali, ...]*/
    QVector<int> peg(3, 0);
    /* Conta dischi per piolo (come altezza barra) */
    QVector<int> pegH = {n, 0, 0};
    { QVector<int> arr = {n*10, 0, 0}; AlgoStep s; s.arr = arr;
      s.msg = QString("Torri di Hanoi — %1 dischi. Barre = dischi su ogni piolo.").arg(n);
      steps << s; }

    int moves = 0;
    std::function<void(int,int,int,int)> hanoi = [&](int discs, int from, int to, int aux) {
        if (discs == 0) return;
        hanoi(discs-1, from, aux, to);
        pegH[from]--; pegH[to]++;
        moves++;
        QVector<int> arr = {pegH[0]*10, pegH[1]*10, pegH[2]*10};
        AlgoStep s; s.arr = arr; s.swp << from << to;
        s.msg = QString("Mossa %1: disco da piolo %2 → piolo %3 (rimangono: A=%4 B=%5 C=%6)")
                .arg(moves).arg(from).arg(to).arg(pegH[0]).arg(pegH[1]).arg(pegH[2]);
        steps << s;
        hanoi(discs-1, aux, to, from);
    };
    hanoi(n, 0, 2, 1);
    { QVector<int> arr = {0, 0, pegH[2]*10}; AlgoStep sf; sf.arr = arr; sf.found << 2;
      sf.msg = QString("Completato! %1 mosse totali (= 2^%2 - 1).").arg(moves).arg(n);
      steps << sf; }
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   NUOVI ALGORITMI — implementazioni gen*
   ══════════════════════════════════════════════════════════════ */

/* ── BucketSort ── */
QVector<AlgoStep> SimulatorePage::genBucketSort(QVector<int> arr) {
    QVector<AlgoStep> st;
    int n = arr.size();
    int mx = *std::max_element(arr.begin(), arr.end()) + 1;
    int nb = qMax(2, n/2);
    auto snap = [&](QVector<int> cmp, QVector<int> swp, QVector<int> srt, QString msg){
        AlgoStep s; s.arr=arr; s.cmp=cmp; s.swp=swp; s.sorted=srt; s.msg=msg; st<<s;
    };
    /* distribuzione nei bucket */
    QVector<QVector<int>> buckets(nb);
    for(int i=0;i<n;i++){
        int bi = arr[i]*nb/mx;
        if(bi>=nb) bi=nb-1;
        snap({i},{},{}, QString("Elemento arr[%1]=%2 → bucket %3").arg(i).arg(arr[i]).arg(bi));
        buckets[bi] << arr[i];
    }
    /* insertion sort su ogni bucket + raccolta */
    QVector<int> res;
    for(int b=0;b<nb;b++){
        std::sort(buckets[b].begin(), buckets[b].end());
        for(int v : buckets[b]) res<<v;
        if(!buckets[b].isEmpty()){
            QVector<int> srt; for(int i=0;i<res.size();i++) srt<<i;
            snap({},{},srt, QString("Bucket %1 ordinato e raccolto (%2 elementi)").arg(b).arg(buckets[b].size()));
        }
    }
    arr = res;
    QVector<int> all; for(int i=0;i<n;i++) all<<i;
    AlgoStep sf; sf.arr=arr; sf.sorted=all; sf.msg="BucketSort completato!"; st<<sf;
    return st;
}

/* ── TimSort ── */
QVector<AlgoStep> SimulatorePage::genTimSort(QVector<int> arr) {
    QVector<AlgoStep> st;
    int n = arr.size();
    const int RUN = 4;
    auto snap = [&](QVector<int> cmp, QVector<int> srt, QString msg){
        AlgoStep s; s.arr=arr; s.cmp=cmp; s.sorted=srt; s.msg=msg; st<<s;
    };
    /* insertion sort su ogni run */
    for(int i=0;i<n;i+=RUN){
        int r = qMin(i+RUN-1, n-1);
        for(int j=i+1;j<=r;j++){
            int key=arr[j], k=j-1;
            snap({j},{}, QString("Insertion Sort: run [%1..%2], inserisco %3").arg(i).arg(r).arg(key));
            while(k>=i && arr[k]>key){ arr[k+1]=arr[k]; k--; }
            arr[k+1]=key;
        }
        QVector<int> srt; for(int x=i;x<=r;x++) srt<<x;
        snap({},srt, QString("Run [%1..%2] ordinata").arg(i).arg(r));
    }
    /* merge dei run */
    for(int sz=RUN;sz<n;sz*=2){
        for(int lo=0;lo<n;lo+=2*sz){
            int mid=qMin(lo+sz-1, n-1);
            int hi=qMin(lo+2*sz-1, n-1);
            if(mid>=hi) continue;
            QVector<int> cmp; for(int x=lo;x<=hi;x++) cmp<<x;
            snap(cmp,{}, QString("Merge [%1..%2] con [%3..%4]").arg(lo).arg(mid).arg(mid+1).arg(hi));
            QVector<int> tmp;
            int a=lo, b=mid+1;
            while(a<=mid&&b<=hi){ if(arr[a]<=arr[b]) tmp<<arr[a++]; else tmp<<arr[b++]; }
            while(a<=mid) tmp<<arr[a++];
            while(b<=hi)  tmp<<arr[b++];
            for(int x=lo;x<=hi;x++) arr[x]=tmp[x-lo];
            QVector<int> srt; for(int x=lo;x<=hi;x++) srt<<x;
            snap({},srt, QString("Merge [%1..%2] completato").arg(lo).arg(hi));
        }
    }
    QVector<int> all; for(int i=0;i<n;i++) all<<i;
    AlgoStep sf; sf.arr=arr; sf.sorted=all; sf.msg="TimSort completato (ibrido Insertion+Merge)!"; st<<sf;
    return st;
}

/* ── StoogeSort ── */
void SimulatorePage::_stoogeRec(QVector<int>& a, int l, int r, QVector<AlgoStep>& st) {
    if(a[l]>a[r]){ std::swap(a[l],a[r]);
        AlgoStep s; s.arr=a; s.swp<<l<<r;
        s.msg=QString("Scambio a[%1]=%2 ↔ a[%3]=%4").arg(l).arg(a[r]).arg(r).arg(a[l]); st<<s; }
    if(r-l+1 > 2){
        int t=(r-l+1)/3;
        { AlgoStep s; s.arr=a; s.cmp<<l<<r;
          s.msg=QString("Stooge: ordina 2/3 iniziali [%1..%2]").arg(l).arg(r-t); st<<s; }
        _stoogeRec(a,l,r-t,st);
        { AlgoStep s; s.arr=a; s.cmp<<l<<r;
          s.msg=QString("Stooge: ordina 2/3 finali [%1..%2]").arg(l+t).arg(r); st<<s; }
        _stoogeRec(a,l+t,r,st);
        { AlgoStep s; s.arr=a; s.cmp<<l<<r;
          s.msg=QString("Stooge: ordina di nuovo 2/3 iniziali [%1..%2]").arg(l).arg(r-t); st<<s; }
        _stoogeRec(a,l,r-t,st);
    }
}
QVector<AlgoStep> SimulatorePage::genStoogeSort(QVector<int> arr) {
    QVector<AlgoStep> st;
    { AlgoStep s; s.arr=arr; s.msg="StoogeSort: ricorsione su 2/3 dell'array (3 chiamate per livello)"; st<<s; }
    int n = qMin((int)arr.size(), 7); /* limita per evitare troppi passi */
    arr.resize(n);
    _stoogeRec(arr,0,n-1,st);
    QVector<int> all; for(int i=0;i<n;i++) all<<i;
    AlgoStep sf; sf.arr=arr; sf.sorted=all; sf.msg="StoogeSort completato! O(n^2.7) — MAI usare in produzione."; st<<sf;
    return st;
}

/* ── BoyerMooreVoting ── */
QVector<AlgoStep> SimulatorePage::genBoyerMooreVoting(QVector<int> arr) {
    QVector<AlgoStep> st;
    int n=arr.size();
    /* crea array con un elemento maggioritario garantito */
    int maj = arr[rand()%n];
    for(int i=0;i<n*2/3;i++) arr[i]=maj;
    arr.resize(n);
    for(int i=n-1;i>0;i--){ int j=rand()%(i+1); std::swap(arr[i],arr[j]); }
    int candidate=arr[0], count=1;
    { AlgoStep s; s.arr=arr; s.cmp<<0; s.found<<0;
      s.msg=QString("Inizio: candidato=%1, count=1").arg(candidate); st<<s; }
    for(int i=1;i<n;i++){
        if(count==0){ candidate=arr[i]; count=1;
            AlgoStep s; s.arr=arr; s.cmp<<i;
            s.msg=QString("count=0 → nuovo candidato=%1").arg(candidate); st<<s; }
        else if(arr[i]==candidate){ count++;
            QVector<int> fi; for(int j=0;j<=i;j++) if(arr[j]==candidate) fi<<j;
            AlgoStep s; s.arr=arr; s.cmp<<i; s.found=fi;
            s.msg=QString("arr[%1]=%2 = candidato → count=%3").arg(i).arg(arr[i]).arg(count); st<<s; }
        else { count--;
            AlgoStep s; s.arr=arr; s.cmp<<i; s.swp<<i;
            s.msg=QString("arr[%1]=%2 ≠ candidato → count=%3").arg(i).arg(arr[i]).arg(count); st<<s; }
    }
    QVector<int> fi; for(int i=0;i<n;i++) if(arr[i]==candidate) fi<<i;
    AlgoStep sf; sf.arr=arr; sf.found=fi;
    sf.msg=QString("Maggioritario trovato: %1 (%2 volte, >n/2=%3)").arg(candidate).arg(fi.size()).arg(n/2); st<<sf;
    return st;
}

/* ── Stack ── */
QVector<AlgoStep> SimulatorePage::genStack(QVector<int> arr) {
    QVector<AlgoStep> st;
    QVector<int> stk;
    int n=qMin((int)arr.size(),8);
    { AlgoStep s; s.arr=arr; s.msg="Stack LIFO: visualizzazione push/pop (cima a destra)"; st<<s; }
    for(int i=0;i<n;i++){
        stk<<arr[i];
        AlgoStep s; s.arr=stk;
        if(!stk.isEmpty()) s.cmp<<(stk.size()-1);
        s.msg=QString("PUSH %1 → top=%1 (size=%2)").arg(arr[i]).arg(stk.size()); st<<s;
    }
    for(int i=0;i<n/2;i++){
        int top=stk.last(); stk.pop_back();
        AlgoStep s; s.arr=stk;
        if(!stk.isEmpty()) s.found<<(stk.size()-1);
        s.msg=QString("POP → rimosso %1, nuovo top=%2").arg(top).arg(stk.isEmpty()?-1:stk.last()); st<<s;
    }
    AlgoStep sf; sf.arr=stk; sf.msg=QString("Stack finale: %1 elementi rimasti.").arg(stk.size()); st<<sf;
    return st;
}

/* ── Queue ── */
QVector<AlgoStep> SimulatorePage::genQueue(QVector<int> arr) {
    QVector<AlgoStep> st;
    QVector<int> q;
    int n=qMin((int)arr.size(),8);
    { AlgoStep s; s.arr=arr; s.msg="Queue FIFO: testa a sinistra, coda a destra"; st<<s; }
    for(int i=0;i<n;i++){
        q<<arr[i];
        AlgoStep s; s.arr=q; s.cmp<<(q.size()-1);
        s.msg=QString("ENQUEUE %1 in coda (size=%2, front=%3)").arg(arr[i]).arg(q.size()).arg(q.front()); st<<s;
    }
    for(int i=0;i<n/2;i++){
        int front=q.front(); q.pop_front();
        AlgoStep s; s.arr=q;
        if(!q.isEmpty()) s.found<<0;
        s.msg=QString("DEQUEUE %1 dalla testa, nuovo front=%2").arg(front).arg(q.isEmpty()?-1:q.front()); st<<s;
    }
    AlgoStep sf; sf.arr=q; sf.msg=QString("Queue finale: %1 elementi rimasti.").arg(q.size()); st<<sf;
    return st;
}

/* ── Deque ── */
QVector<AlgoStep> SimulatorePage::genDeque(QVector<int> arr) {
    QVector<AlgoStep> st;
    QVector<int> dq;
    int n=qMin((int)arr.size(),8);
    { AlgoStep s; s.arr=arr; s.msg="Deque: push/pop da entrambe le estremità"; st<<s; }
    for(int i=0;i<n;i++){
        if(i%2==0){ dq.push_back(arr[i]);
            AlgoStep s; s.arr=dq; if(!dq.isEmpty()) s.cmp<<(dq.size()-1);
            s.msg=QString("push_back(%1) → [%2 ... %3]").arg(arr[i]).arg(dq.front()).arg(dq.back()); st<<s; }
        else { dq.push_front(arr[i]);
            AlgoStep s; s.arr=dq; s.cmp<<0;
            s.msg=QString("push_front(%1) → [%2 ... %3]").arg(arr[i]).arg(dq.front()).arg(dq.back()); st<<s; }
    }
    for(int i=0;i<n/2;i++){
        if(i%2==0 && !dq.isEmpty()){ int v=dq.back(); dq.pop_back();
            AlgoStep s; s.arr=dq; s.msg=QString("pop_back → rimosso %1").arg(v); st<<s; }
        else if(!dq.isEmpty()){ int v=dq.front(); dq.pop_front();
            AlgoStep s; s.arr=dq; s.msg=QString("pop_front → rimosso %1").arg(v); st<<s; }
    }
    AlgoStep sf; sf.arr=dq; sf.msg="Deque: operazioni O(1) su entrambi i lati."; st<<sf;
    return st;
}

/* ── MinHeapBuild (Floyd) ── */
QVector<AlgoStep> SimulatorePage::genMinHeapBuild(QVector<int> arr) {
    QVector<AlgoStep> st;
    int n=arr.size();
    { AlgoStep s; s.arr=arr; s.msg="Min-Heap Build (Floyd): heapify dal basso — O(n)"; st<<s; }
    std::function<void(int)> heapify=[&](int i){
        int smallest=i, l=2*i+1, r=2*i+2;
        if(l<n && arr[l]<arr[smallest]) smallest=l;
        if(r<n && arr[r]<arr[smallest]) smallest=r;
        if(smallest!=i){
            AlgoStep s; s.arr=arr; s.cmp<<i<<smallest;
            s.msg=QString("Heapify: arr[%1]=%2 > arr[%3]=%4 → scambio").arg(i).arg(arr[i]).arg(smallest).arg(arr[smallest]); st<<s;
            std::swap(arr[i],arr[smallest]);
            AlgoStep s2; s2.arr=arr; s2.swp<<i<<smallest; s2.msg="Scambio eseguito"; st<<s2;
            heapify(smallest);
        } else {
            AlgoStep s; s.arr=arr; s.found<<i;
            s.msg=QString("Heapify nodo %1=%2: soddisfatto (min-heap property OK)").arg(i).arg(arr[i]); st<<s;
        }
    };
    for(int i=n/2-1;i>=0;i--) heapify(i);
    QVector<int> all; for(int i=0;i<n;i++) all<<i;
    AlgoStep sf; sf.arr=arr; sf.sorted=all;
    sf.msg=QString("Min-Heap costruito! Radice (min) = %1").arg(arr[0]); st<<sf;
    return st;
}

/* ── HashTable ── */
QVector<AlgoStep> SimulatorePage::genHashTable(QVector<int> arr) {
    QVector<AlgoStep> st;
    int n=arr.size();
    int SZ=n+3;
    QVector<int> table(SZ, -1);
    { AlgoStep s; s.arr=table; s.msg=QString("Hash Table size=%1: inserimento con h(k)=k%%SIZE, chaining").arg(SZ); st<<s; }
    QVector<QVector<int>> chains(SZ);
    for(int i=0;i<n;i++){
        int h=arr[i]%SZ;
        { AlgoStep s; s.arr=table; s.cmp<<h;
          s.msg=QString("Insert %1: h(%1)=%1%%(%2)=%3").arg(arr[i]).arg(SZ).arg(h); st<<s; }
        chains[h]<<arr[i];
        table[h]=chains[h].size();
        AlgoStep s2; s2.arr=table; s2.swp<<h;
        s2.msg=QString("Bucket[%1] ora ha %2 elemento/i (chaining)").arg(h).arg(chains[h].size()); st<<s2;
    }
    /* ricerca */
    int key=arr[rand()%n], hk=key%SZ;
    AlgoStep sf; sf.arr=table; sf.found<<hk;
    sf.msg=QString("Ricerca %1: h=%2, trovato in bucket[%3] — O(1) avg").arg(key).arg(hk).arg(hk); st<<sf;
    return st;
}

/* ── SegmentTree ── */
QVector<AlgoStep> SimulatorePage::genSegmentTree(QVector<int> arr) {
    QVector<AlgoStep> st;
    int n=arr.size();
    QVector<int> seg(4*n, 0);
    std::function<void(int,int,int)> build=[&](int node,int l,int r){
        if(l==r){ seg[node]=arr[l];
            AlgoStep s; s.arr=seg; s.cmp<<node;
            s.msg=QString("Foglia seg[%1] = arr[%2] = %3").arg(node).arg(l).arg(arr[l]); st<<s;
            return; }
        int mid=(l+r)/2;
        build(2*node,l,mid);
        build(2*node+1,mid+1,r);
        seg[node]=seg[2*node]+seg[2*node+1];
        AlgoStep s; s.arr=seg; s.swp<<node<<2*node<<2*node+1;
        s.msg=QString("seg[%1] = seg[%2]+seg[%3] = %4 (range [%5..%6])")
              .arg(node).arg(2*node).arg(2*node+1).arg(seg[node]).arg(l).arg(r); st<<s;
    };
    build(1,0,n-1);
    /* range query */
    int ql=1, qr=n-2;
    AlgoStep sf; sf.arr=seg; sf.found<<1;
    sf.msg=QString("Segment Tree costruito! Query range [%1..%2] in O(log n)").arg(ql).arg(qr); st<<sf;
    return st;
}

/* ── FenwickTree ── */
QVector<AlgoStep> SimulatorePage::genFenwickTree(QVector<int> arr) {
    QVector<AlgoStep> st;
    int n=arr.size();
    QVector<int> bit(n+1, 0);
    { AlgoStep s; s.arr=bit; s.msg="Fenwick Tree (BIT): build tramite n update — O(n log n)"; st<<s; }
    for(int i=0;i<n;i++){
        int v=arr[i], pos=i+1;
        { AlgoStep s; s.arr=bit; s.cmp<<(pos-1);
          s.msg=QString("Update pos=%1, val=%2").arg(pos).arg(v); st<<s; }
        for(int j=pos;j<=n;j+=j&(-j)){
            bit[j]+=v;
            AlgoStep s; s.arr=bit; s.swp<<(j-1);
            s.msg=QString("bit[%1] += %2 → %3  (lowbit=%4)").arg(j).arg(v).arg(bit[j]).arg(j&(-j)); st<<s;
        }
    }
    /* prefix query */
    int qpos=n/2; int sum=0;
    for(int j=qpos;j>0;j-=j&(-j)) sum+=bit[j];
    AlgoStep sf; sf.arr=bit; sf.found<<(qpos-1);
    sf.msg=QString("Prefix sum [1..%1] = %2  (in O(log n))").arg(qpos).arg(sum); st<<sf;
    return st;
}

/* ── LRUCache ── */
QVector<AlgoStep> SimulatorePage::genLRUCache(QVector<int> arr) {
    QVector<AlgoStep> st;
    int cap=4, n=qMin((int)arr.size()+2, 10);
    QVector<int> cache; /* sinistra=LRU, destra=MRU */
    { AlgoStep s; s.arr=arr; s.msg=QString("LRU Cache capacity=%1: sinistra=LRU, destra=MRU").arg(cap); st<<s; }
    QVector<int> accesses;
    for(int i=0;i<n;i++) accesses<<arr[i%arr.size()];
    accesses[n/2]=accesses[0]; /* forza un hit */
    for(int i=0;i<n;i++){
        int key=accesses[i]%10+1;
        int pos=cache.indexOf(key);
        if(pos>=0){ /* hit */
            cache.remove(pos); cache.append(key);
            AlgoStep s; s.arr=cache; s.found<<(cache.size()-1);
            s.msg=QString("HIT key=%1 → sposta in MRU (destra)").arg(key); st<<s;
        } else { /* miss */
            if(cache.size()>=cap){
                int evicted=cache.front(); cache.pop_front();
                AlgoStep s; s.arr=cache; s.swp<<0;
                s.msg=QString("MISS key=%1: evict LRU=%2").arg(key).arg(evicted); st<<s;
            }
            cache.append(key);
            AlgoStep s; s.arr=cache; s.cmp<<(cache.size()-1);
            s.msg=QString("MISS key=%1: inserito (cache=%2/%3)").arg(key).arg(cache.size()).arg(cap); st<<s;
        }
    }
    QVector<int> all; for(int i=0;i<cache.size();i++) all<<i;
    AlgoStep sf; sf.arr=cache; sf.sorted=all; sf.msg="LRU Cache finale (O(1) get/put con HashMap+DList)"; st<<sf;
    return st;
}

/* ── FloydWarshall ── */
QVector<AlgoStep> SimulatorePage::genFloydWarshall() {
    QVector<AlgoStep> st;
    const int V=5;
    const int INF=999;
    int g[V][V]={
        {0,  3,INF,  7,INF},
        {8,  0,  2,INF,INF},
        {5,INF,  0,  1,INF},
        {2,INF,INF,  0,  3},
        {INF,INF,INF, 6,  0}
    };
    QVector<int> dist(V*V);
    for(int i=0;i<V;i++) for(int j=0;j<V;j++) dist[i*V+j]=g[i][j];
    { AlgoStep s; s.arr=dist; s.msg=QString("Floyd-Warshall: matrice distanze %1x%1 (INF=%2)").arg(V).arg(INF); st<<s; }
    for(int k=0;k<V;k++){
        for(int i=0;i<V;i++) for(int j=0;j<V;j++){
            if(dist[i*V+k]<INF && dist[k*V+j]<INF){
                int newD=dist[i*V+k]+dist[k*V+j];
                if(newD<dist[i*V+j]){
                    AlgoStep s; s.arr=dist; s.cmp<<i*V+j<<i*V+k<<k*V+j;
                    s.msg=QString("k=%1: dist[%2][%3]=%4 → via %5 = %6 (miglioramento!)")
                          .arg(k).arg(i).arg(j).arg(dist[i*V+j]).arg(k).arg(newD); st<<s;
                    dist[i*V+j]=newD;
                    AlgoStep s2; s2.arr=dist; s2.swp<<i*V+j; s2.msg="Distanza aggiornata"; st<<s2;
                }
            }
        }
        AlgoStep s; s.arr=dist;
        s.msg=QString("Iterazione k=%1 completata: nodo %1 come intermedio").arg(k); st<<s;
    }
    AlgoStep sf; sf.arr=dist; sf.msg="Floyd-Warshall: tutti i cammini minimi calcolati! O(V³)"; st<<sf;
    return st;
}

/* ── Kruskal MST ── */
QVector<AlgoStep> SimulatorePage::genKruskal() {
    QVector<AlgoStep> st;
    /* grafi come archi pesati: {u,v,w} */
    struct Edge { int u,v,w; };
    QVector<Edge> edges={{0,1,4},{0,2,3},{1,2,1},{1,3,2},{2,3,4},{3,4,2},{2,4,5}};
    int V=5;
    QVector<int> parent(V); for(int i=0;i<V;i++) parent[i]=i;
    std::function<int(int)> find=[&](int x)->int{
        return parent[x]==x?x:parent[x]=find(parent[x]);
    };
    std::sort(edges.begin(),edges.end(),[](const Edge& a,const Edge& b){return a.w<b.w;});
    QVector<int> display(edges.size()); for(int i=0;i<edges.size();i++) display[i]=edges[i].w;
    { AlgoStep s; s.arr=display; s.msg="Kruskal: archi ordinati per peso. MST crescerà aggiungendo il minimo senza cicli."; st<<s; }
    int mstW=0; int added=0;
    QVector<int> inMST;
    for(int i=0;i<(int)edges.size();i++){
        int pu=find(edges[i].u), pv=find(edges[i].v);
        if(pu!=pv){ parent[pu]=pv; mstW+=edges[i].w; inMST<<i; added++;
            AlgoStep s; s.arr=display; s.found=inMST; s.cmp<<i;
            s.msg=QString("Aggiungo arco (%1-%2) w=%3 al MST (ciclo: NO) — totale=%4")
                  .arg(edges[i].u).arg(edges[i].v).arg(edges[i].w).arg(mstW); st<<s;
            if(added==V-1) break;
        } else {
            AlgoStep s; s.arr=display; s.swp<<i;
            s.msg=QString("Salto arco (%1-%2) w=%3: formerebbe un ciclo")
                  .arg(edges[i].u).arg(edges[i].v).arg(edges[i].w); st<<s;
        }
    }
    AlgoStep sf; sf.arr=display; sf.sorted=inMST;
    sf.msg=QString("Kruskal MST completato! Peso totale=%1 (%2 archi)").arg(mstW).arg(added); st<<sf;
    return st;
}

/* ── Prim MST ── */
QVector<AlgoStep> SimulatorePage::genPrim() {
    QVector<AlgoStep> st;
    const int V=6;
    int adj[V][V]={
        {0,2,0,6,0,0},{2,0,3,8,5,0},{0,3,0,0,7,0},
        {6,8,0,0,9,0},{0,5,7,9,0,1},{0,0,0,0,1,0}
    };
    QVector<int> key(V,999), parent(V,-1); QVector<bool> inMST(V,false);
    key[0]=0;
    QVector<int> display(V,0);
    { AlgoStep s; s.arr=display; s.msg=QString("Prim MST: partenza dal nodo 0. Cresce aggiungendo l'arco minimo."); st<<s; }
    for(int cnt=0;cnt<V;cnt++){
        int u=-1;
        for(int v=0;v<V;v++) if(!inMST[v]&&(u==-1||key[v]<key[u])) u=v;
        inMST[u]=true; display[u]=key[u];
        QVector<int> srt; for(int i=0;i<V;i++) if(inMST[i]) srt<<i;
        AlgoStep s; s.arr=display; s.sorted=srt; s.cmp<<u;
        s.msg=QString("Aggiungo nodo %1 (peso arco=%2, padre=%3) al MST")
              .arg(u).arg(key[u]).arg(parent[u]); st<<s;
        for(int v=0;v<V;v++) if(adj[u][v]&&!inMST[v]&&adj[u][v]<key[v]){
            key[v]=adj[u][v]; parent[v]=u;
            AlgoStep s2; s2.arr=display; s2.swp<<v;
            s2.msg=QString("Aggiorna key[%1]=%2 via nodo %3").arg(v).arg(key[v]).arg(u); st<<s2;
        }
    }
    int tot=0; for(int i=1;i<V;i++) tot+=key[i];
    QVector<int> all; for(int i=0;i<V;i++) all<<i;
    AlgoStep sf; sf.arr=display; sf.sorted=all;
    sf.msg=QString("Prim MST completato! Peso totale=%1").arg(tot); st<<sf;
    return st;
}

/* ── TarjanSCC ── */
QVector<AlgoStep> SimulatorePage::genTarjanSCC() {
    QVector<AlgoStep> st;
    const int V=7;
    QVector<QVector<int>> adj={{1},{2},{0,3},{4},{3,5},{6},{4}};
    QVector<int> disc(V,-1), low(V,-1), stkArr(V,0);
    QVector<bool> onStk(V,false);
    int timer=0;
    QVector<QVector<int>> sccs;
    QVector<int> stkV;
    std::function<void(int)> dfs=[&](int u){
        disc[u]=low[u]=timer++;
        stkV<<u; onStk[u]=true;
        QVector<int> disp(V,0); for(int i=0;i<V;i++) disp[i]=disc[i]<0?0:disc[i];
        AlgoStep s; s.arr=disp; s.cmp<<u;
        s.msg=QString("DFS nodo %1: disc=%2, low=%3").arg(u).arg(disc[u]).arg(low[u]); st<<s;
        for(int v : adj[u]){
            if(disc[v]<0){ dfs(v); low[u]=qMin(low[u],low[v]); }
            else if(onStk[v]) low[u]=qMin(low[u],disc[v]);
        }
        if(low[u]==disc[u]){
            QVector<int> scc;
            while(true){ int w=stkV.last(); stkV.pop_back(); onStk[w]=false; scc<<w; if(w==u) break; }
            sccs<<scc;
            QVector<int> disp2(V,0); for(auto& sc:sccs) for(int x:sc) disp2[x]=(int)sccs.size();
            AlgoStep s2; s2.arr=disp2; s2.found=scc;
            s2.msg=QString("SCC trovata: {%1}").arg([&](){QString r; for(int x:scc) r+=QString::number(x)+","; r.chop(1); return r;}()); st<<s2;
        }
    };
    for(int i=0;i<V;i++) if(disc[i]<0) dfs(i);
    QVector<int> final(V,0); for(int s=0;s<(int)sccs.size();s++) for(int x:sccs[s]) final[x]=s+1;
    QVector<int> all; for(int i=0;i<V;i++) all<<i;
    AlgoStep sf; sf.arr=final; sf.sorted=all;
    sf.msg=QString("Tarjan SCC: trovate %1 componenti fortemente connesse!").arg(sccs.size()); st<<sf;
    return st;
}

/* ── A* Search ── */
QVector<AlgoStep> SimulatorePage::genAStar() {
    QVector<AlgoStep> st;
    /* griglia 4x4, 0=libero 1=muro */
    const int W=5, H=4;
    int grid[H][W]={{0,0,1,0,0},{0,0,1,0,0},{0,0,0,0,0},{0,1,1,0,0}};
    int sx=0,sy=0, gx=4,gy=3;
    auto h=[&](int x,int y){ return qAbs(x-gx)+qAbs(y-gy); };
    QVector<int> display(W*H,0);
    display[sy*W+sx]=1; display[gy*W+gx]=2;
    { AlgoStep s; s.arr=display; s.cmp<<sy*W+sx; s.found<<gy*W+gx;
      s.msg=QString("A*: start=(%1,%2) goal=(%3,%4). Euristica=Manhattan.").arg(sx).arg(sy).arg(gx).arg(gy); st<<s; }
    struct Node{ int x,y,g,f; };
    QVector<Node> open={{sx,sy,0,h(sx,sy)}};
    QVector<int> visited(W*H,0);
    int itr=0;
    while(!open.isEmpty() && itr<20){
        auto it=std::min_element(open.begin(),open.end(),[](const Node&a,const Node&b){return a.f<b.f;});
        Node cur=*it; open.erase(it);
        if(cur.x==gx&&cur.y==gy){ break; }
        visited[cur.y*W+cur.x]=1;
        display[cur.y*W+cur.x]=3;
        QVector<int> cmp; cmp<<cur.y*W+cur.x;
        AlgoStep s; s.arr=display; s.cmp=cmp;
        s.msg=QString("Espando (%1,%2): g=%3, h=%4, f=%5").arg(cur.x).arg(cur.y).arg(cur.g).arg(h(cur.x,cur.y)).arg(cur.f); st<<s;
        int dx[]={1,-1,0,0}, dy[]={0,0,1,-1};
        for(int d=0;d<4;d++){
            int nx=cur.x+dx[d], ny=cur.y+dy[d];
            if(nx<0||nx>=W||ny<0||ny>=H||grid[ny][nx]||visited[ny*W+nx]) continue;
            int ng=cur.g+1, nf=ng+h(nx,ny);
            open.push_back({nx,ny,ng,nf});
            display[ny*W+nx]=4;
            AlgoStep s2; s2.arr=display; s2.swp<<ny*W+nx;
            s2.msg=QString("Aggiungo (%1,%2) all'open list: g=%3 h=%4 f=%5").arg(nx).arg(ny).arg(ng).arg(h(nx,ny)).arg(nf); st<<s2;
        }
        itr++;
    }
    display[gy*W+gx]=5;
    QVector<int> all; for(int i=0;i<W*H;i++) all<<i;
    AlgoStep sf; sf.arr=display; sf.found<<gy*W+gx;
    sf.msg=QString("A* completato! Percorso trovato da (%1,%2) a (%3,%4)").arg(sx).arg(sy).arg(gx).arg(gy); st<<sf;
    return st;
}

/* ── MatrixChain ── */
QVector<AlgoStep> SimulatorePage::genMatrixChain() {
    QVector<AlgoStep> st;
    /* dimensioni catena: p[i] x p[i+1] */
    QVector<int> p={10,30,5,60,10};
    int n=p.size()-1;
    QVector<int> dp(n*n,0);
    { AlgoStep s; s.arr=p; s.msg=QString("Matrix Chain: %1 matrici. p=%2x%3, %3x%4, %4x%5, %5x%6")
        .arg(n).arg(p[0]).arg(p[1]).arg(p[2]).arg(p[3]).arg(p[4]); st<<s; }
    for(int len=2;len<=n;len++){
        for(int i=0;i<n-len+1;i++){
            int j=i+len-1;
            dp[i*n+j]=INT_MAX;
            for(int k=i;k<j;k++){
                int cost=dp[i*n+k]+dp[(k+1)*n+j]+p[i]*p[k+1]*p[j+1];
                if(cost<dp[i*n+j]){
                    dp[i*n+j]=cost;
                    QVector<int> disp; for(int x=0;x<n;x++) disp<<(dp[x*n+qMin(x+len-1,n-1)]/100+1);
                    AlgoStep s; s.arr=disp; s.cmp<<i<<j;
                    s.msg=QString("dp[%1][%2]=min via k=%3: costo=%4").arg(i).arg(j).arg(k).arg(cost); st<<s;
                }
            }
        }
    }
    QVector<int> disp; for(int x=0;x<n;x++) disp<<(dp[x*n+qMin(x+n-1,n-1)]/100+1);
    AlgoStep sf; sf.arr=disp; sf.msg=QString("Costo ottimale Matrix Chain = %1 moltiplicazioni").arg(dp[0*n+n-1]); st<<sf;
    return st;
}

/* ── EggDrop ── */
QVector<AlgoStep> SimulatorePage::genEggDrop() {
    QVector<AlgoStep> st;
    int eggs=3, floors=8;
    QVector<int> dp((eggs+1)*(floors+1), 0);
    { AlgoStep s; s.arr=dp; s.msg=QString("Egg Drop: %1 uova, %2 piani. dp[e][f]=tentativi minimi").arg(eggs).arg(floors); st<<s; }
    for(int e=1;e<=eggs;e++){
        for(int f=1;f<=floors;f++){
            dp[e*(floors+1)+f]=f; /* worst case con 1 uovo */
            for(int x=1;x<=f;x++){
                int worst=1+qMax(dp[(e-1)*(floors+1)+(x-1)], dp[e*(floors+1)+(f-x)]);
                if(worst<dp[e*(floors+1)+f]){
                    dp[e*(floors+1)+f]=worst;
                    QVector<int> disp; for(int i=0;i<=eggs;i++) disp<<dp[i*(floors+1)+f];
                    AlgoStep s; s.arr=disp; s.cmp<<e;
                    s.msg=QString("dp[%1 uova][%2 piani]=%3 (provo piano x=%4)").arg(e).arg(f).arg(worst).arg(x); st<<s;
                }
            }
        }
    }
    QVector<int> disp; for(int e=0;e<=eggs;e++) disp<<dp[e*(floors+1)+floors];
    AlgoStep sf; sf.arr=disp; sf.found<<eggs;
    sf.msg=QString("Con %1 uova e %2 piani: servono min %3 tentativi").arg(eggs).arg(floors).arg(dp[eggs*(floors+1)+floors]); st<<sf;
    return st;
}

/* ── RodCutting ── */
QVector<AlgoStep> SimulatorePage::genRodCutting() {
    QVector<AlgoStep> st;
    QVector<int> price={0,1,5,8,9,10,17,17,20};
    int n=8;
    QVector<int> dp(n+1,0);
    { AlgoStep s; s.arr=price; s.msg="Rod Cutting: profitto massimo tagliando la sbarra in pezzi"; st<<s; }
    for(int i=1;i<=n;i++){
        for(int j=1;j<=i;j++){
            if(price[j]+dp[i-j]>dp[i]){
                dp[i]=price[j]+dp[i-j];
                AlgoStep s; s.arr=dp; s.cmp<<i; s.swp<<j;
                s.msg=QString("dp[%1]=max via taglio j=%2: prezzo[%3]=%4 + dp[%5]=%6 = %7")
                      .arg(i).arg(j).arg(j).arg(price[j]).arg(i-j).arg(dp[i-j]).arg(dp[i]); st<<s;
            }
        }
        QVector<int> srt; for(int k=0;k<=i;k++) srt<<k;
        AlgoStep s; s.arr=dp; s.sorted=srt; s.msg=QString("Sbarra lunghezza %1: profitto ottimale=%2").arg(i).arg(dp[i]); st<<s;
    }
    QVector<int> all; for(int i=0;i<=n;i++) all<<i;
    AlgoStep sf; sf.arr=dp; sf.sorted=all; sf.msg=QString("Rod Cutting: profitto max per sbarra n=%1 è %2").arg(n).arg(dp[n]); st<<sf;
    return st;
}

/* ── SubsetSumDP ── */
QVector<AlgoStep> SimulatorePage::genSubsetSumDP(QVector<int> arr) {
    QVector<AlgoStep> st;
    int n=qMin((int)arr.size(),6);
    arr.resize(n);
    int target=0; for(int v:arr) target+=v; target/=2;
    QVector<int> dp(target+2,0); dp[0]=1;
    { AlgoStep s; s.arr=arr; s.msg=QString("Subset Sum DP: target=%1, tabella booleana di size %2").arg(target).arg(target+1); st<<s; }
    for(int i=0;i<n;i++){
        QVector<int> ndp=dp;
        for(int j=target;j>=arr[i];j--){
            if(dp[j-arr[i]]){ ndp[j]=1;
                AlgoStep s; s.arr=ndp; s.cmp<<j; s.swp<<(j-arr[i]);
                s.msg=QString("arr[%1]=%2: dp[%3]=true (da dp[%4])").arg(i).arg(arr[i]).arg(j).arg(j-arr[i]); st<<s; }
        }
        dp=ndp;
    }
    QVector<int> found_; for(int j=0;j<=target;j++) if(dp[j]) found_<<j;
    AlgoStep sf; sf.arr=dp; sf.found=found_;
    sf.msg=QString("Subset Sum DP: somma target=%1 → %2").arg(target).arg(dp[target]?"TROVATA":"non trovata"); st<<sf;
    return st;
}

/* ── MaxProductSubarray ── */
QVector<AlgoStep> SimulatorePage::genMaxProductSubarray(QVector<int> arr) {
    QVector<AlgoStep> st;
    int n=arr.size();
    /* mix di positivi e negativi */
    for(int i=0;i<n;i++) if(i%3==1) arr[i]=-arr[i];
    int maxP=arr[0], minP=arr[0], best=arr[0];
    { AlgoStep s; s.arr=arr; s.msg="Max Product Subarray: traccia max E min (negativi si invertono)"; st<<s; }
    for(int i=1;i<n;i++){
        int tmax=std::max({arr[i], maxP*arr[i], minP*arr[i]});
        int tmin=std::min({arr[i], maxP*arr[i], minP*arr[i]});
        maxP=tmax; minP=tmin;
        if(maxP>best) best=maxP;
        AlgoStep s; s.arr=arr; s.cmp<<i;
        if(maxP==best) s.found<<i;
        s.msg=QString("i=%1 arr[i]=%2: maxP=%3, minP=%4, best=%5")
              .arg(i).arg(arr[i]).arg(maxP).arg(minP).arg(best); st<<s;
    }
    AlgoStep sf; sf.arr=arr; sf.msg=QString("Max Product Subarray = %1").arg(best); st<<sf;
    return st;
}

/* ── ActivitySelection ── */
QVector<AlgoStep> SimulatorePage::genActivitySelection() {
    QVector<AlgoStep> st;
    struct Act { int s,e; };
    QVector<Act> acts={{1,3},{2,5},{4,7},{6,9},{5,8},{8,10},{9,11}};
    std::sort(acts.begin(),acts.end(),[](const Act& a,const Act& b){return a.e<b.e;});
    QVector<int> ends; for(auto& a:acts) ends<<a.e;
    { AlgoStep s; s.arr=ends; s.msg="Activity Selection: attività ordinate per fine. Greedy: scegli sempre quella che finisce prima."; st<<s; }
    QVector<int> sel;
    int lastEnd=-1;
    for(int i=0;i<(int)acts.size();i++){
        if(acts[i].s>=lastEnd){ sel<<i; lastEnd=acts[i].e;
            AlgoStep s; s.arr=ends; s.found=sel; s.cmp<<i;
            s.msg=QString("Seleziono att. %1 [%2..%3] (inizia dopo fine=%4)").arg(i).arg(acts[i].s).arg(acts[i].e).arg(lastEnd); st<<s;
        } else {
            AlgoStep s; s.arr=ends; s.swp<<i;
            s.msg=QString("Salto att. %1 [%2..%3]: sovrapposta (last end=%4)").arg(i).arg(acts[i].s).arg(acts[i].e).arg(lastEnd); st<<s;
        }
    }
    AlgoStep sf; sf.arr=ends; sf.sorted=sel;
    sf.msg=QString("Activity Selection: %1 attività non sovrapposte selezionate (ottimale)").arg(sel.size()); st<<sf;
    return st;
}

/* ── FractionalKnapsack ── */
QVector<AlgoStep> SimulatorePage::genFractionalKnapsack() {
    QVector<AlgoStep> st;
    struct Item { int v,w; };
    QVector<Item> items={{60,10},{100,20},{120,30},{80,25},{50,15}};
    int cap=50;
    std::sort(items.begin(),items.end(),[](const Item& a,const Item& b){
        return (double)a.v/a.w > (double)b.v/b.w;
    });
    QVector<int> vals; for(auto& it:items) vals<<it.v;
    { AlgoStep s; s.arr=vals; s.msg=QString("Fractional Knapsack: capacity=%1. Ordina per valore/peso decrescente.").arg(cap); st<<s; }
    double totalVal=0; int rem=cap;
    QVector<int> taken;
    for(int i=0;i<(int)items.size();i++){
        if(rem>=items[i].w){ rem-=items[i].w; totalVal+=items[i].v; taken<<i;
            AlgoStep s; s.arr=vals; s.found=taken; s.cmp<<i;
            s.msg=QString("Prendo intero item %1 (v=%2,w=%3) ratio=%.1f → tot=%.1f rem=%4")
                  .arg(i).arg(items[i].v).arg(items[i].w).arg((double)items[i].v/items[i].w).arg(totalVal).arg(rem); st<<s;
        } else if(rem>0){
            double frac=(double)rem/items[i].w;
            totalVal+=frac*items[i].v;
            AlgoStep s; s.arr=vals; s.cmp<<i; s.swp<<i;
            s.msg=QString("Prendo fraz. %1%% di item %2 (v=%3,w=%4) → val+=%.1f")
                  .arg((int)(frac*100)).arg(i).arg(items[i].v).arg(items[i].w).arg(frac*items[i].v); st<<s;
            rem=0; taken<<i; break;
        }
    }
    AlgoStep sf; sf.arr=vals; sf.sorted=taken;
    sf.msg=QString("Fractional Knapsack: valore massimo = %.1f (capacity=%2)").arg(totalVal).arg(cap); st<<sf;
    return st;
}

/* ── Huffman ── */
QVector<AlgoStep> SimulatorePage::genHuffman(QVector<int> freq) {
    QVector<AlgoStep> st;
    int n=qMin((int)freq.size(),8);
    freq.resize(n);
    using P=QPair<int,int>; /* peso, id */
    QVector<P> pq;
    for(int i=0;i<n;i++) pq.push_back({freq[i],i});
    std::sort(pq.begin(),pq.end());
    { AlgoStep s; s.arr=freq; s.msg="Huffman: min-heap di frequenze. Unisce i 2 minimi a ogni passo."; st<<s; }
    int nextId=n;
    while(pq.size()>1){
        std::sort(pq.begin(),pq.end());
        P a=pq.front(); pq.pop_front();
        P b=pq.front(); pq.pop_front();
        int merged=a.first+b.first;
        AlgoStep s; s.arr=freq;
        if(a.second<n) s.cmp<<a.second;
        if(b.second<n) s.swp<<b.second;
        s.msg=QString("Unisci freq=%1 + freq=%2 → nodo interno %3 (peso=%4)")
              .arg(a.first).arg(b.first).arg(nextId).arg(merged); st<<s;
        freq.append(merged);
        pq.push_back({merged,nextId++});
    }
    QVector<int> all; for(int i=0;i<freq.size();i++) all<<i;
    AlgoStep sf; sf.arr=freq; sf.found=all;
    sf.msg=QString("Huffman completato! Simboli frequenti = codici corti. Radice=%1").arg(pq.front().first); st<<sf;
    return st;
}

/* ── JobScheduling ── */
QVector<AlgoStep> SimulatorePage::genJobScheduling() {
    QVector<AlgoStep> st;
    struct Job { int p,d; }; /* profit, deadline */
    QVector<Job> jobs={{20,2},{15,2},{10,1},{5,3},{1,3}};
    std::sort(jobs.begin(),jobs.end(),[](const Job& a,const Job& b){return a.p>b.p;});
    int maxD=0; for(auto& j:jobs) maxD=qMax(maxD,j.d);
    QVector<int> jobSlots(maxD, -1);
    QVector<int> profits; for(auto& j:jobs) profits<<j.p;
    { AlgoStep s; s.arr=profits; s.msg=QString("Job Scheduling EDF: %1 job, ordina per profitto desc. %2 slot.").arg(jobs.size()).arg(maxD); st<<s; }
    int total=0;
    QVector<int> sel;
    for(int i=0;i<(int)jobs.size();i++){
        for(int k=qMin(jobs[i].d-1,maxD-1);k>=0;k--){
            if(jobSlots[k]<0){ jobSlots[k]=i; total+=jobs[i].p; sel<<i;
                QVector<int> disp(maxD,0); for(int m=0;m<maxD;m++) if(jobSlots[m]>=0) disp[m]=jobs[jobSlots[m]].p;
                AlgoStep as; as.arr=disp; as.found=sel; as.cmp<<k;
                as.msg=QString("Job %1 (p=%2,d=%3) \xe2\x86\x92 slot %4").arg(i).arg(jobs[i].p).arg(jobs[i].d).arg(k); st<<as;
                break; }
        }
    }
    QVector<int> disp(maxD,0); for(int k=0;k<maxD;k++) if(jobSlots[k]>=0) disp[k]=jobs[jobSlots[k]].p;
    AlgoStep sf; sf.arr=disp; sf.msg=QString("Job Scheduling: profitto massimo=%1 (%2 job)").arg(total).arg(sel.size()); st<<sf;
    return st;
}

/* ── CoinGreedy ── */
QVector<AlgoStep> SimulatorePage::genCoinGreedy(QVector<int> coins, int target) {
    QVector<AlgoStep> st;
    std::sort(coins.begin(),coins.end(),std::greater<int>());
    QVector<int> used(coins.size(),0);
    int rem=target;
    { AlgoStep s; s.arr=coins; s.msg=QString("Coin Change Greedy: target=%1, monete decrescenti").arg(target); st<<s; }
    for(int i=0;i<(int)coins.size()&&rem>0;i++){
        if(coins[i]<=rem){ int cnt=rem/coins[i]; used[i]=cnt; rem-=cnt*coins[i];
            AlgoStep s; s.arr=used; s.found<<i;
            s.msg=QString("Uso %1x moneta %2 (resto=%3)").arg(cnt).arg(coins[i]).arg(rem); st<<s; }
    }
    QVector<int> all; for(int i=0;i<(int)coins.size();i++) all<<i;
    AlgoStep sf; sf.arr=used; sf.sorted=all;
    sf.msg=QString("Greedy: target=%1, resto=%2 (resto≠0 = greedy non ottimale su questo insieme)").arg(target).arg(rem); st<<sf;
    return st;
}

/* ── MinPlatforms ── */
QVector<AlgoStep> SimulatorePage::genMinPlatforms() {
    QVector<AlgoStep> st;
    QVector<int> arr={900,940,950,1100,1500,1800};
    QVector<int> dep={910,1200,1120,1130,1900,2000};
    int n=arr.size();
    std::sort(arr.begin(),arr.end()); std::sort(dep.begin(),dep.end());
    { AlgoStep s; s.arr=arr; s.msg="Min Platforms: orari di arrivo e partenza (in centinaia). Massimo treni contemporanei."; st<<s; }
    int plat=1, maxPlat=1, i=1, j=0;
    QVector<int> display(n,0);
    while(i<n&&j<n){
        if(arr[i]<=dep[j]){ plat++; if(plat>maxPlat) maxPlat=plat;
            display[i]=plat;
            AlgoStep s; s.arr=display; s.cmp<<i;
            s.msg=QString("Arrivo %1 ≤ partenza %2 → +1 piattaforma = %3").arg(arr[i]).arg(dep[j]).arg(plat); st<<s;
            i++;
        } else { plat--;
            display[j]=0;
            AlgoStep s; s.arr=display; s.swp<<j;
            s.msg=QString("Partenza %1 < arrivo %2 → -1 piattaforma = %3").arg(dep[j]).arg(arr[i<n?i:n-1]).arg(plat); st<<s;
            j++;
        }
    }
    AlgoStep sf; sf.arr=display;
    sf.msg=QString("Min Platforms = %1 (massimo treni contemporanei)").arg(maxPlat); st<<sf;
    return st;
}

/* ── NQueens ── */
void SimulatorePage::_nqSolve(int col, QVector<int>& board, QVector<AlgoStep>& st, int& found_) {
    int n=board.size();
    if(col==n){ found_++;
        AlgoStep s; s.arr=board; s.found=board;
        s.msg=QString("SOLUZIONE #%1 trovata! Regina in ogni colonna senza attacchi.").arg(found_); st<<s;
        return; }
    for(int row=0;row<n;row++){
        bool ok=true;
        for(int c=0;c<col;c++){
            if(board[c]==row||qAbs(board[c]-row)==qAbs(c-col)){ ok=false; break; }
        }
        if(ok){ board[col]=row;
            AlgoStep s; s.arr=board; s.cmp<<col;
            s.msg=QString("Piazzo regina col=%1, riga=%2").arg(col).arg(row); st<<s;
            if(found_<3) _nqSolve(col+1,board,st,found_);
            if(found_>=3) return;
            board[col]=-1;
            AlgoStep sb; sb.arr=board; sb.swp<<col;
            sb.msg=QString("Backtrack: colonna %1 libera").arg(col); st<<sb;
        }
    }
}
QVector<AlgoStep> SimulatorePage::genNQueens(int n) {
    QVector<AlgoStep> st;
    n=qMin(n,5);
    QVector<int> board(n,-1);
    { AlgoStep s; s.arr=board; s.msg=QString("N-Queens %1x%1: backtracking. -1=libero, val=riga della regina.").arg(n).arg(n); st<<s; }
    int found_=0;
    _nqSolve(0,board,st,found_);
    AlgoStep sf; sf.arr=board; sf.msg=QString("N-Queens %1: trovate ≥%2 soluzioni (totale=%3)").arg(n).arg(found_).arg(n<=6?QStringList({"1","0","0","2","10","4"})[n-1]:"?"); st<<sf;
    return st;
}

/* ── SubsetSum backtracking ── */
QVector<AlgoStep> SimulatorePage::genSubsetSum(QVector<int> arr, int target) {
    QVector<AlgoStep> st;
    int n=qMin((int)arr.size(),7); arr.resize(n);
    if(target<=0||target>100) target=arr[0]+arr[1]+arr[2];
    { AlgoStep s; s.arr=arr; s.msg=QString("Subset Sum BT: target=%1. Include/escludi ogni elemento.").arg(target); st<<s; }
    int found_=0;
    std::function<void(int,int,QVector<int>)> bt=[&](int i,int cur,QVector<int> chosen){
        if(found_>=4) return;
        if(cur==target){ found_++;
            AlgoStep s; s.arr=arr; s.found=chosen;
            s.msg=QString("TROVATO subset #%1 con somma=%2").arg(found_).arg(target); st<<s; return; }
        if(i>=n||cur>target) return;
        /* includi */
        QVector<int> ch2=chosen; ch2<<i;
        AlgoStep s1; s1.arr=arr; s1.cmp<<i; s1.found=ch2;
        s1.msg=QString("Includo arr[%1]=%2 → somma=%3").arg(i).arg(arr[i]).arg(cur+arr[i]); st<<s1;
        bt(i+1,cur+arr[i],ch2);
        /* escludi */
        AlgoStep s2; s2.arr=arr; s2.swp<<i; s2.found=chosen;
        s2.msg=QString("Escludo arr[%1]=%2 → somma=%3").arg(i).arg(arr[i]).arg(cur); st<<s2;
        bt(i+1,cur,chosen);
    };
    bt(0,0,{});
    AlgoStep sf; sf.arr=arr;
    sf.msg=QString("Subset Sum BT: trovati %1 sottoinsiemi con somma=%2").arg(found_).arg(target); st<<sf;
    return st;
}

/* ── Permutations ── */
QVector<AlgoStep> SimulatorePage::genPermutations(QVector<int> arr) {
    QVector<AlgoStep> st;
    int n=qMin((int)arr.size(),5); arr.resize(n);
    int count=0;
    { AlgoStep s; s.arr=arr; s.msg=QString("Permutazioni di %1 elementi: genera tutte le %2! = %3 permutazioni").arg(n).arg(n).arg([](int k){int r=1;for(int i=2;i<=k;i++)r*=i;return r;}(n)); st<<s; }
    std::function<void(int)> perm=[&](int start){
        if(count>=12) return;
        if(start==n){ count++;
            AlgoStep s; s.arr=arr; s.found=QVector<int>(n); for(int i=0;i<n;i++) s.found[i]=i;
            s.msg=QString("Permutazione #%1: [%2]").arg(count).arg([&](){QString r; for(int v:arr) r+=QString::number(v)+" "; return r.trimmed();}()); st<<s; return; }
        for(int i=start;i<n;i++){
            std::swap(arr[start],arr[i]);
            AlgoStep s; s.arr=arr; s.swp<<start<<i;
            s.msg=QString("Scambio pos %1↔%2").arg(start).arg(i); st<<s;
            perm(start+1);
            std::swap(arr[start],arr[i]);
        }
    };
    perm(0);
    AlgoStep sf; sf.arr=arr; sf.msg=QString("Permutazioni: generate %1 permutazioni (O(n!))").arg(count); st<<sf;
    return st;
}

/* ── FloodFill ── */
QVector<AlgoStep> SimulatorePage::genFloodFill(QVector<int> arr) {
    QVector<AlgoStep> st;
    /* griglia 4x4 con valori 0..2 */
    const int W=4, H=4;
    QVector<int> grid(W*H);
    for(int i=0;i<W*H;i++) grid[i]=arr[i%arr.size()]%3;
    int startColor=grid[0], fillColor=9;
    if(startColor==fillColor) fillColor=8;
    { AlgoStep s; s.arr=grid; s.cmp<<0; s.msg=QString("Flood Fill: riempi da (0,0) colore=%1 → nuovo=%2 (BFS)").arg(startColor).arg(fillColor); st<<s; }
    QQueue<int> q; q.enqueue(0); grid[0]=fillColor;
    QVector<int> filled; filled<<0;
    int dx[]={1,-1,0,0}, dy[]={0,0,1,-1};
    while(!q.isEmpty()){
        int pos=q.dequeue(); int x=pos%W, y=pos/W;
        AlgoStep s; s.arr=grid; s.found=filled; s.cmp<<pos;
        s.msg=QString("Visita (%1,%2): colora con %3").arg(x).arg(y).arg(fillColor); st<<s;
        for(int d=0;d<4;d++){
            int nx=x+dx[d], ny=y+dy[d];
            if(nx<0||nx>=W||ny<0||ny>=H) continue;
            int np=ny*W+nx;
            if(grid[np]==startColor){ grid[np]=fillColor; q.enqueue(np); filled<<np; }
        }
    }
    AlgoStep sf; sf.arr=grid; sf.found=filled;
    sf.msg=QString("Flood Fill completato: %1 celle colorate").arg(filled.size()); st<<sf;
    return st;
}

/* ── RatInMaze ── */
QVector<AlgoStep> SimulatorePage::genRatInMaze() {
    QVector<AlgoStep> st;
    const int N=4;
    int maze[N][N]={{1,0,0,0},{1,1,0,1},{0,1,0,0},{0,1,1,1}};
    QVector<int> display(N*N,0);
    for(int i=0;i<N;i++) for(int j=0;j<N;j++) display[i*N+j]=maze[i][j];
    { AlgoStep s; s.arr=display; s.cmp<<0; s.msg="Rat in a Maze: da (0,0) a (3,3). 1=libero, 0=muro."; st<<s; }
    QVector<int> sol(N*N,0);
    std::function<bool(int,int)> solve=[&](int r,int c)->bool{
        if(r==N-1&&c==N-1){ sol[r*N+c]=2;
            AlgoStep s; s.arr=sol; QVector<int> path; for(int i=0;i<N*N;i++) if(sol[i]) path<<i; s.found=path;
            s.msg="PERCORSO TROVATO! Il topo ha raggiunto (3,3)."; st<<s; return true; }
        if(r<0||r>=N||c<0||c>=N||!maze[r][c]) return false;
        sol[r*N+c]=1;
        AlgoStep s; s.arr=sol; s.cmp<<r*N+c;
        s.msg=QString("Provo (%1,%2)").arg(r).arg(c); st<<s;
        int dr[]={1,0,0,-1}, dc[]={0,1,-1,0};
        for(int d=0;d<4;d++) if(solve(r+dr[d],c+dc[d])) return true;
        sol[r*N+c]=0;
        AlgoStep sb; sb.arr=sol; sb.swp<<r*N+c;
        sb.msg=QString("Backtrack da (%1,%2)").arg(r).arg(c); st<<sb;
        return false;
    };
    solve(0,0);
    return st;
}

/* ── KMP ── */
QVector<AlgoStep> SimulatorePage::genKMP(const QString& pattern, const QString& text) {
    QVector<AlgoStep> st;
    int m=pattern.size(), n=text.size();
    QVector<int> fail(m,0);
    /* costruisci failure function */
    for(int i=1;i<m;i++){
        int j=fail[i-1];
        while(j>0&&pattern[i]!=pattern[j]) j=fail[j-1];
        if(pattern[i]==pattern[j]) j++;
        fail[i]=j;
    }
    QVector<int> fdisp(m); for(int i=0;i<m;i++) fdisp[i]=fail[i];
    { AlgoStep s; s.arr=fdisp; s.msg=QString("KMP failure function per pattern='%1'. Evita backtracking.").arg(pattern); st<<s; }
    /* ricerca */
    QVector<int> textV(n), patV(m);
    for(int i=0;i<n;i++) textV[i]=text[i].unicode()%30+1;
    for(int i=0;i<m;i++) patV[i]=pattern[i].unicode()%30+1;
    int j=0;
    QVector<int> found_;
    for(int i=0;i<n;){
        AlgoStep s; s.arr=textV; s.cmp<<i; if(j>0&&j-1<textV.size()) s.swp<<j-1;
        s.msg=QString("text[%1]='%2' vs pattern[%3]='%4'").arg(i).arg(text[i]).arg(j).arg(j<m?pattern[j]:QChar('?')); st<<s;
        if(text[i]==pattern[j]){ i++; j++;
            if(j==m){ found_<<(i-m);
                AlgoStep sf; sf.arr=textV; sf.found=found_;
                sf.msg=QString("TROVATO pattern a pos %1!").arg(i-m); st<<sf; j=fail[j-1]; }
        } else if(j>0) j=fail[j-1];
        else i++;
    }
    AlgoStep sfin; sfin.arr=textV; sfin.found=found_;
    sfin.msg=QString("KMP completato: pattern '%1' trovato %2 volta/e").arg(pattern).arg(found_.size()); st<<sfin;
    return st;
}

/* ── RabinKarp ── */
QVector<AlgoStep> SimulatorePage::genRabinKarp(const QString& pattern, const QString& text) {
    QVector<AlgoStep> st;
    int m=pattern.size(), n=text.size();
    if(m>n){ AlgoStep s; s.arr={0}; s.msg="Pattern più lungo del testo!"; st<<s; return st; }
    const int BASE=31, MOD=101;
    int pH=0, tH=0, pw=1;
    for(int i=0;i<m-1;i++) pw=pw*BASE%MOD;
    for(int i=0;i<m;i++){
        pH=(pH*BASE+pattern[i].unicode())%MOD;
        tH=(tH*BASE+text[i].unicode())%MOD;
    }
    QVector<int> textV(n); for(int i=0;i<n;i++) textV[i]=text[i].unicode()%30+1;
    { AlgoStep s; s.arr=textV; s.msg=QString("Rabin-Karp: hash pattern='%1' =%2. Rolling hash su testo.").arg(pattern).arg(pH); st<<s; }
    QVector<int> found_;
    for(int i=0;i<=n-m;i++){
        AlgoStep s; s.arr=textV; QVector<int> window; for(int k=i;k<i+m;k++) window<<k; s.cmp=window;
        s.msg=QString("Finestra [%1..%2]: hash=%3 (pattern hash=%4) → %5").arg(i).arg(i+m-1).arg(tH).arg(pH).arg(tH==pH?"HIT":"miss"); st<<s;
        if(tH==pH){ /* verifica */
            bool ok=true; for(int k=0;k<m;k++) if(text[i+k]!=pattern[k]){ok=false;break;}
            if(ok){ found_<<i;
                AlgoStep sf; sf.arr=textV; sf.found=found_;
                sf.msg=QString("TROVATO a pos %1 (hash match + verifica char OK)").arg(i); st<<sf; }
        }
        if(i<n-m) tH=(BASE*(tH-text[i].unicode()*pw%MOD+MOD)+text[i+m].unicode())%MOD;
    }
    AlgoStep sfin; sfin.arr=textV; sfin.found=found_;
    sfin.msg=QString("Rabin-Karp: trovato %1 volta/e").arg(found_.size()); st<<sfin;
    return st;
}

/* ── ZAlgorithm ── */
QVector<AlgoStep> SimulatorePage::genZAlgorithm(const QString& s) {
    QVector<AlgoStep> st;
    int n=s.size();
    QVector<int> Z(n,0); Z[0]=n;
    int l=0, r=0;
    { QVector<int> disp(n,0); disp[0]=n;
      AlgoStep s0; s0.arr=disp; s0.msg=QString("Z-Algorithm su '%1': Z[i]=lunghezza prefisso comune con s[i..]").arg(s); st<<s0; }
    for(int i=1;i<n;i++){
        if(i<r) Z[i]=qMin(r-i, Z[i-l]);
        while(i+Z[i]<n && s[Z[i]]==s[i+Z[i]]) Z[i]++;
        if(i+Z[i]>r){ l=i; r=i+Z[i]; }
        AlgoStep snap; snap.arr=Z; snap.cmp<<i;
        snap.msg=QString("Z[%1]=%2 (s[%1..]='%3' vs prefisso '%4')")
                 .arg(i).arg(Z[i]).arg(s.mid(i,qMin(Z[i],4))).arg(s.left(qMin(Z[i],4))); st<<snap;
    }
    QVector<int> all; for(int i=0;i<n;i++) all<<i;
    AlgoStep sf; sf.arr=Z; sf.sorted=all; sf.msg=QString("Z-Array completato per '%1'").arg(s); st<<sf;
    return st;
}

/* ── Manacher ── */
QVector<AlgoStep> SimulatorePage::genManacher(const QString& s) {
    QVector<AlgoStep> st;
    /* trasforma in #a#b#a# */
    QString t="#";
    for(QChar c:s){ t+=c; t+="#"; }
    int n=t.size();
    QVector<int> P(n,0);
    int C=0, R=0;
    { AlgoStep s0; s0.arr=QVector<int>(s.size(),0); s0.msg=QString("Manacher su '%1' → trasformato in '%2'").arg(s).arg(t); st<<s0; }
    for(int i=0;i<n;i++){
        if(i<R) P[i]=qMin(R-i, P[2*C-i]);
        while(i-P[i]-1>=0 && i+P[i]+1<n && t[i-P[i]-1]==t[i+P[i]+1]) P[i]++;
        if(i+P[i]>R){ C=i; R=i+P[i]; }
        if(P[i]>0){
            QVector<int> disp(s.size(),0);
            int center=(i-1)/2, half=P[i]/2;
            if(center>=0&&center<s.size()) disp[qMax(0,center-half)]=P[i];
            AlgoStep snap; snap.arr=disp; snap.cmp<<qMax(0,(center-half>0?center-half:0));
            snap.msg=QString("i=%1 ('%2'): P[i]=%3, palindromo di raggio %4").arg(i).arg(t[i]).arg(P[i]).arg(P[i]); st<<snap;
        }
    }
    int bestR=*std::max_element(P.begin(),P.end());
    int bestC=P.indexOf(bestR);
    int lo=(bestC-bestR)/2, len=bestR;
    AlgoStep sf; sf.arr=P; sf.found<<bestC;
    sf.msg=QString("Palindromo più lungo: '%1' (len=%2, posizione=%3)")
           .arg(s.mid(lo,len)).arg(len).arg(lo); st<<sf;
    return st;
}

/* ── LongestCommonPrefix ── */
QVector<AlgoStep> SimulatorePage::genLongestCommonPrefix(QVector<int> arr) {
    QVector<AlgoStep> st;
    int n=arr.size();
    /* usa l'array come suffissi di un vettore numerico */
    QVector<QVector<int>> suffixes;
    for(int i=0;i<n;i++){
        QVector<int> s;
        for(int j=i;j<n;j++) s<<arr[j];
        suffixes<<s;
    }
    /* ordina lessicograficamente */
    std::sort(suffixes.begin(),suffixes.end());
    QVector<int> lcp(n,0);
    for(int i=1;i<n;i++){
        int k=0;
        while(k<(int)suffixes[i-1].size()&&k<(int)suffixes[i].size()&&suffixes[i-1][k]==suffixes[i][k]) k++;
        lcp[i]=k;
    }
    { AlgoStep s; s.arr=lcp; s.msg="LCP Array su suffix array. lcp[i]=prefisso comune tra suffisso i e i-1."; st<<s; }
    for(int i=1;i<n;i++){
        AlgoStep s; s.arr=lcp; s.cmp<<i;
        s.msg=QString("lcp[%1]=%2: i primi %3 elementi in comune tra suff%1 e suff%4").arg(i).arg(lcp[i]).arg(lcp[i]).arg(i-1); st<<s;
    }
    int maxLcp=*std::max_element(lcp.begin(),lcp.end());
    QVector<int> all; for(int i=0;i<n;i++) all<<i;
    AlgoStep sf; sf.arr=lcp; sf.sorted=all; sf.msg=QString("LCP Array completato. Prefisso comune massimo = %1").arg(maxLcp); st<<sf;
    return st;
}

/* ── SieveSundaram ── */
QVector<AlgoStep> SimulatorePage::genSieveSundaram(int limit) {
    QVector<AlgoStep> st;
    QVector<int> a(limit+1, 1);
    { AlgoStep s; s.arr=a; s.msg=QString("Crivello di Sundaram: elimina i+j+2ij per 1≤i≤j, produce primi dispari ≤ %1").arg(2*limit+1); st<<s; }
    for(int i=1;i<=limit;i++){
        for(int j=i;i+j+2*i*j<=limit;j++){
            int idx=i+j+2*i*j;
            if(a[idx]){ a[idx]=0;
                AlgoStep s; s.arr=a; s.swp<<idx;
                s.msg=QString("Elimina %1 (i=%2,j=%3): 2*%1+1=%4 non è primo").arg(idx).arg(i).arg(j).arg(2*idx+1); st<<s; }
        }
    }
    QVector<int> primes={2};
    for(int i=1;i<=limit;i++) if(a[i]) primes<<2*i+1;
    AlgoStep sf; sf.arr=a;
    for(int i=1;i<=limit;i++) if(a[i]) sf.sorted<<i;
    sf.msg=QString("Sundaram: trovati %1 primi (incluso 2)").arg(primes.size()); st<<sf;
    return st;
}

/* ── MillerRabin ── */
QVector<AlgoStep> SimulatorePage::genMillerRabin(int n) {
    QVector<AlgoStep> st;
    if(n%2==0) n++;
    auto mulmod=[](long long a,long long b,long long m)->long long{
        long long r=0; a%=m;
        while(b>0){ if(b&1) r=(r+a)%m; a=a*2%m; b>>=1; }
        return r;
    };
    auto powmod=[&](long long a,long long b,long long m)->long long{
        long long r=1; a%=m;
        while(b>0){ if(b&1) r=mulmod(r,a,m); a=mulmod(a,a,m); b>>=1; }
        return r;
    };
    /* scrivi n-1 = 2^s * d */
    int s=0; long long d=n-1;
    while(d%2==0){ d/=2; s++; }
    QVector<int> witnesses={2,3,5,7};
    QVector<int> display(witnesses.size(),0);
    { AlgoStep s0; s0.arr=display; s0.msg=QString("Miller-Rabin test su n=%1: n-1=2^%2 * %3").arg(n).arg(s).arg(d); st<<s0; }
    bool composite=false;
    for(int wi=0;wi<(int)witnesses.size();wi++){
        long long a=witnesses[wi]; if(a>=n) continue;
        long long x=powmod(a,d,n);
        display[wi]=(int)(x%50+1);
        bool ok=(x==1||x==n-1);
        if(!ok){
            for(int r=0;r<s-1;r++){ x=mulmod(x,x,n); if(x==n-1){ok=true;break;} }
        }
        AlgoStep snap; snap.arr=display; snap.cmp<<wi;
        snap.msg=QString("Testimone a=%1: x=%2 → %3").arg(a).arg(x%100).arg(ok?"PASS (probabilmente primo)":"COMPOSITE!"); st<<snap;
        if(!ok){ composite=true; display[wi]=0; break; }
    }
    AlgoStep sf; sf.arr=display;
    sf.msg=QString("Miller-Rabin: n=%1 è %2 (k=%3 testimoni, errore<4^(-k))").arg(n).arg(composite?"COMPOSTO":"probabilmente PRIMO").arg(witnesses.size()); st<<sf;
    return st;
}

/* ── PascalTriangle ── */
QVector<AlgoStep> SimulatorePage::genPascalTriangle(int n) {
    QVector<AlgoStep> st;
    n=qMin(n,8);
    QVector<int> prev={1};
    { AlgoStep s; s.arr=prev; s.msg=QString("Triangolo di Pascal: %1 righe. C(n,k) = C(n-1,k-1) + C(n-1,k)").arg(n); st<<s; }
    for(int row=1;row<n;row++){
        QVector<int> cur={1};
        for(int k=1;k<row;k++) cur<<prev[k-1]+prev[k];
        cur<<1;
        AlgoStep s; s.arr=cur; for(int i=0;i<(int)cur.size();i++) s.cmp<<i;
        s.msg=QString("Riga %1: coefficienti binomiali C(%1,0)..C(%1,%1)").arg(row); st<<s;
        prev=cur;
    }
    QVector<int> all; for(int i=0;i<(int)prev.size();i++) all<<i;
    AlgoStep sf; sf.arr=prev; sf.sorted=all; sf.msg=QString("Pascal riga %1 completata").arg(n-1); st<<sf;
    return st;
}

/* ── Catalan ── */
QVector<AlgoStep> SimulatorePage::genCatalan(int n) {
    QVector<AlgoStep> st;
    n=qMin(n,10);
    QVector<int> cat(n+1,0); cat[0]=cat[1]=1;
    { AlgoStep s; s.arr=cat; s.msg="Numeri di Catalan: C(n) = sum C(i)*C(n-1-i) per i=0..n-1"; st<<s; }
    for(int i=2;i<=n;i++){
        for(int j=0;j<i;j++) cat[i]+=cat[j]*cat[i-1-j];
        AlgoStep s; s.arr=cat; s.cmp<<i;
        s.msg=QString("C(%1) = %2 (alberi BST con %1 nodi, parentesizzazioni di %2 fattori)").arg(i).arg(cat[i]); st<<s;
    }
    QVector<int> all; for(int i=0;i<=n;i++) all<<i;
    AlgoStep sf; sf.arr=cat; sf.sorted=all; sf.msg=QString("Catalan(%1) = %2. Sequenza: 1,1,2,5,14,42,132...").arg(n).arg(cat[n]); st<<sf;
    return st;
}

/* ── MonteCarloPi ── */
QVector<AlgoStep> SimulatorePage::genMonteCarloPi(int n) {
    QVector<AlgoStep> st;
    n=qMin(n,60);
    int inside=0;
    QVector<int> display(10,0); /* barre: % dentro cerchio per 10 gruppi */
    { AlgoStep s; s.arr=display; s.msg=QString("Monte Carlo Pi: %1 punti casuali. Punti dentro cerchio / totale ≈ π/4").arg(n); st<<s; }
    for(int i=1;i<=n;i++){
        double x=(double)rand()/RAND_MAX*2-1;
        double y=(double)rand()/RAND_MAX*2-1;
        if(x*x+y*y<=1.0) inside++;
        if(i%(n/10+1)==0||i==n){
            int gi=qMin(9,(i-1)*10/n);
            display[gi]=(int)(4.0*inside/i*10);
            double pi=4.0*inside/i;
            AlgoStep s; s.arr=display; s.cmp<<gi;
            s.msg=QString("Dopo %1 punti: dentro=%2, stima π=%3").arg(i).arg(inside).arg(QString::number(pi,'f',4)); st<<s;
        }
    }
    double pi=4.0*inside/n;
    AlgoStep sf; sf.arr=display; sf.msg=QString("Monte Carlo Pi ≈ %1 (valore reale 3.14159...)").arg(QString::number(pi,'f',5)); st<<sf;
    return st;
}

/* ── Collatz ── */
QVector<AlgoStep> SimulatorePage::genCollatz(int n) {
    QVector<AlgoStep> st;
    QVector<int> seq; seq<<n;
    { AlgoStep s; s.arr=seq; s.msg=QString("Congettura di Collatz: n=%1. Pari→n/2, dispari→3n+1. Raggiunge 1?").arg(n); st<<s; }
    int steps=0;
    while(n!=1&&steps<100){
        if(n%2==0){ n/=2;
            AlgoStep s; seq<<n; s.arr=seq.mid(qMax(0,(int)seq.size()-10)); s.found<<((int)seq.size()-1)%10;
            s.msg=QString("Pari → n/2 = %1 (passo %2)").arg(n).arg(++steps); st<<s; }
        else { n=3*n+1;
            AlgoStep s; seq<<n; s.arr=seq.mid(qMax(0,(int)seq.size()-10)); s.swp<<((int)seq.size()-1)%10;
            s.msg=QString("Dispari → 3n+1 = %1 (passo %2)").arg(n).arg(++steps); st<<s; }
    }
    QVector<int> disp=seq.mid(qMax(0,(int)seq.size()-10));
    AlgoStep sf; sf.arr=disp; sf.msg=QString("Collatz: %1 passi per raggiungere 1 (congettura non dimostrata!)").arg(steps); st<<sf;
    return st;
}

/* ── Karatsuba ── */
QVector<AlgoStep> SimulatorePage::genKaratsuba(int a, int b) {
    QVector<AlgoStep> st;
    { QVector<int> disp={a/100,a%100,b/100,b%100};
      AlgoStep s; s.arr=disp; s.msg=QString("Karatsuba: %1 × %2. Divide ogni numero in 2 metà.").arg(a).arg(b); st<<s; }
    /* a = a1*100 + a0, b = b1*100 + b0 */
    int a1=a/100, a0=a%100, b1=b/100, b0=b%100;
    int z0=a0*b0, z2=a1*b1;
    { QVector<int> disp={z0,z2,a0+a1,b0+b1};
      AlgoStep s; s.arr=disp; s.cmp<<0<<1;
      s.msg=QString("z0=a0*b0=%1*%2=%3, z2=a1*b1=%4*%5=%6").arg(a0).arg(b0).arg(z0).arg(a1).arg(b1).arg(z2); st<<s; }
    int z1=(a0+a1)*(b0+b1)-z0-z2;
    { QVector<int> disp={z0,z1,z2,0};
      AlgoStep s; s.arr=disp; s.swp<<1;
      s.msg=QString("z1=(a0+a1)(b0+b1)-z0-z2=%1*%2-%3-%4=%5").arg(a0+a1).arg(b0+b1).arg(z0).arg(z2).arg(z1); st<<s; }
    long long result=(long long)z2*10000 + (long long)z1*100 + z0;
    { QVector<int> disp={(int)(result/10000),(int)((result/100)%100),(int)(result%100),(int)(a*b==result?1:0)};
      AlgoStep sf; sf.arr=disp; sf.found<<0<<1<<2;
      sf.msg=QString("Risultato: %1*10000 + %2*100 + %3 = %4 (check=%5*%6=%7)")
             .arg(z2).arg(z1).arg(z0).arg(result).arg(a).arg(b).arg(a*b); st<<sf; }
    return st;
}

/* ── MaxCircularSubarray ── */
QVector<AlgoStep> SimulatorePage::genMaxCircularSubarray(QVector<int> arr) {
    QVector<AlgoStep> st;
    int n=arr.size();
    /* mix neg/pos */
    for(int i=0;i<n;i++) if(i%2==0&&arr[i]>3) arr[i]-=arr[i]/2;
    { AlgoStep s; s.arr=arr; s.msg="Max Circular Subarray: Kadane normale E Kadane sul min subarray (caso wrap-around)"; st<<s; }
    /* kadane normale */
    int maxS=arr[0], cur=arr[0], total=arr[0];
    for(int i=1;i<n;i++){
        cur=qMax(arr[i], cur+arr[i]);
        maxS=qMax(maxS,cur); total+=arr[i];
        AlgoStep s; s.arr=arr; s.cmp<<i;
        s.msg=QString("Kadane normale: i=%1 arr[i]=%2 maxS=%3").arg(i).arg(arr[i]).arg(maxS); st<<s;
    }
    /* kadane sul minimo */
    int minS=arr[0]; cur=arr[0];
    for(int i=1;i<n;i++){
        cur=qMin(arr[i], cur+arr[i]);
        minS=qMin(minS,cur);
        AlgoStep s; s.arr=arr; s.swp<<i;
        s.msg=QString("Kadane minimo: i=%1 arr[i]=%2 minS=%3").arg(i).arg(arr[i]).arg(minS); st<<s;
    }
    int maxCircular=total-minS;
    int best=qMax(maxS, maxCircular);
    AlgoStep sf; sf.arr=arr;
    sf.msg=QString("Max Circular Subarray = max(lineare=%1, circolare=%2) = %3").arg(maxS).arg(maxCircular).arg(best); st<<sf;
    return st;
}

/* ── CountInversions ── */
QVector<AlgoStep> SimulatorePage::genCountInversions(QVector<int> arr) {
    QVector<AlgoStep> st;
    int n=arr.size(); int totalInv=0;
    { AlgoStep s; s.arr=arr; s.msg="Count Inversions con Merge Sort: conta coppie (i,j) con i<j ma a[i]>a[j]"; st<<s; }
    std::function<int(QVector<int>&,int,int)> mergeCount=[&](QVector<int>& a,int lo,int hi)->int{
        if(hi<=lo) return 0;
        int mid=(lo+hi)/2;
        int inv=mergeCount(a,lo,mid)+mergeCount(a,mid+1,hi);
        QVector<int> tmp;
        int i=lo, j=mid+1;
        while(i<=mid&&j<=hi){
            if(a[i]<=a[j]) tmp<<a[i++];
            else { inv+=mid-i+1; tmp<<a[j++];
                QVector<int> cmp_; for(int k=i;k<=mid;k++) cmp_<<k;
                AlgoStep s; s.arr=a; s.cmp=cmp_; s.swp<<j-1;
                s.msg=QString("a[%1]=%2 > a[%3]=%4: %5 inversioni (da pos %6..%7)")
                      .arg(j-1).arg(a[j-1]).arg(i).arg(a[i]).arg(mid-i+1).arg(i).arg(mid); st<<s; }
        }
        while(i<=mid) tmp<<a[i++];
        while(j<=hi) tmp<<a[j++];
        for(int k=lo;k<=hi;k++) a[k]=tmp[k-lo];
        totalInv+=inv;
        return inv;
    };
    mergeCount(arr,0,n-1);
    QVector<int> all; for(int i=0;i<n;i++) all<<i;
    AlgoStep sf; sf.arr=arr; sf.sorted=all; sf.msg=QString("Count Inversions: %1 inversioni totali").arg(totalInv); st<<sf;
    return st;
}

/* ── GameOfLife1D ── */
QVector<AlgoStep> SimulatorePage::genGameOfLife1D(QVector<int> arr) {
    QVector<AlgoStep> st;
    int n=qMin((int)arr.size(),12);
    QVector<int> cells(n);
    for(int i=0;i<n;i++) cells[i]=(arr[i]>arr.size()/2)?1:0;
    cells[n/2]=1; /* assicura almeno una cella viva */
    { AlgoStep s; s.arr=cells; s.msg=QString("Game of Life 1D: %1 celle. 1=viva 0=morta. Regola: nasce se esattamente 1 vicino vivo.").arg(n); st<<s; }
    for(int gen=0;gen<10;gen++){
        QVector<int> next(n,0);
        for(int i=1;i<n-1;i++){
            int alive=(cells[i-1]+cells[i+1]);
            next[i]=(alive==1)?1:0;
        }
        QVector<int> cmp_,srt_;
        for(int i=0;i<n;i++){
            if(next[i]!=cells[i]) cmp_<<i;
            if(next[i]) srt_<<i;
        }
        cells=next;
        AlgoStep s; s.arr=cells; s.cmp=cmp_; s.sorted=srt_;
        s.msg=QString("Generazione %1: %2 cellule vive").arg(gen+1).arg(srt_.size()); st<<s;
    }
    return st;
}

/* ── Rule30 ── */
QVector<AlgoStep> SimulatorePage::genRule30(QVector<int> arr) {
    QVector<AlgoStep> st;
    int n=qMin((int)arr.size(),12);
    QVector<int> cells(n,0); cells[n/2]=1;
    { AlgoStep s; s.arr=cells; s.msg="Rule 30 (Wolfram): automa cellulare deterministico che genera pseudo-casualità. Centro=1."; st<<s; }
    for(int gen=0;gen<12;gen++){
        QVector<int> next(n,0);
        for(int i=1;i<n-1;i++){
            int pattern=(cells[i-1]<<2)|(cells[i]<<1)|cells[i+1];
            /* Rule 30: 00011110 */
            next[i]=(30>>pattern)&1;
        }
        cells=next;
        QVector<int> srt_; for(int i=0;i<n;i++) if(cells[i]) srt_<<i;
        AlgoStep s; s.arr=cells; s.sorted=srt_;
        s.msg=QString("Gen %1: %2 celle attive (Rule 30 = pseudo-random)").arg(gen+1).arg(srt_.size()); st<<s;
    }
    return st;
}

/* ── SpiralMatrix ── */
QVector<AlgoStep> SimulatorePage::genSpiralMatrix(int n) {
    QVector<AlgoStep> st;
    n=qMin(n,4);
    QVector<int> mat(n*n,0);
    { AlgoStep s; s.arr=mat; s.msg=QString("Spiral Matrix %1x%1: riempi a spirale. Barre = ordine di visita.").arg(n); st<<s; }
    int top=0,bot=n-1,left=0,right=n-1,val=1;
    QVector<int> order;
    while(top<=bot&&left<=right){
        for(int c=left;c<=right;c++){ mat[top*n+c]=val++; order<<top*n+c;
            AlgoStep s; s.arr=mat; s.cmp<<top*n+c;
            s.msg=QString("Spirale: mat[%1][%2]=%3 (→)").arg(top).arg(c).arg(val-1); st<<s; }
        top++;
        for(int r=top;r<=bot;r++){ mat[r*n+right]=val++; order<<r*n+right;
            AlgoStep s; s.arr=mat; s.cmp<<r*n+right;
            s.msg=QString("Spirale: mat[%1][%2]=%3 (↓)").arg(r).arg(right).arg(val-1); st<<s; }
        right--;
        if(top<=bot){ for(int c=right;c>=left;c--){ mat[bot*n+c]=val++; order<<bot*n+c;
            AlgoStep s; s.arr=mat; s.cmp<<bot*n+c;
            s.msg=QString("Spirale: mat[%1][%2]=%3 (←)").arg(bot).arg(c).arg(val-1); st<<s; } bot--; }
        if(left<=right){ for(int r=bot;r>=top;r--){ mat[r*n+left]=val++; order<<r*n+left;
            AlgoStep s; s.arr=mat; s.cmp<<r*n+left;
            s.msg=QString("Spirale: mat[%1][%2]=%3 (↑)").arg(r).arg(left).arg(val-1); st<<s; } left++; }
    }
    AlgoStep sf; sf.arr=mat; sf.sorted=order; sf.msg=QString("Spiral Matrix %1x%1 completata! %2 celle.").arg(n).arg(n*n); st<<sf;
    return st;
}

/* ── SierpinskiRow ── */
QVector<AlgoStep> SimulatorePage::genSierpinskiRow(int n) {
    QVector<AlgoStep> st;
    n=qMin(n,10);
    { QVector<int> row={1};
      AlgoStep s; s.arr=row; s.msg="Triangolo di Sierpinski: riga n del triangolo di Pascal mod 2. Frattale binario."; st<<s; }
    for(int r=1;r<n;r++){
        /* calcola riga r del triangolo di Pascal mod 2 */
        QVector<int> row(r+1,0); row[0]=row[r]=1;
        for(int k=1;k<r;k++){
            /* C(r,k) mod 2 via regola di Lucas */
            int rr=r,kk=k,ok=1;
            while(rr>0||kk>0){ if((kk%2)>(rr%2)){ok=0;break;} rr/=2; kk/=2; }
            row[k]=ok;
        }
        QVector<int> srt; for(int k=0;k<=r;k++) if(row[k]) srt<<k;
        AlgoStep s; s.arr=row; s.sorted=srt;
        s.msg=QString("Riga %1: %2 bit = 1 (Sierpinski: 1 se C(%1,k) mod 2 ≠ 0)").arg(r).arg(srt.size()); st<<s;
    }
    return st;
}

/* ══════════════════════════════════════════════════════════════
   rebuildAlgoCmb — ricostruisce m_algoCmb per la categoria scelta
   catIdx: 0 = Tutti, 1..CAT_COUNT = categoria specifica
   ══════════════════════════════════════════════════════════════ */
void SimulatorePage::rebuildAlgoCmb(int catIdx) {
    m_filteredIdx.clear();
    for (int i = 0; i < ALGO_COUNT; i++) {
        if (catIdx == 0 || (int)kAlgos[i].cat == catIdx - 1)
            m_filteredIdx.append(i);
    }
    m_algoCmb->blockSignals(true);
    m_algoCmb->clear();
    for (int fi = 0; fi < m_filteredIdx.size(); fi++)
        m_algoCmb->addItem(QString::fromUtf8(kAlgos[m_filteredIdx[fi]].name));
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
    m_autoBtn->setText("\u23f8 Pausa");
    m_timer->start(1600 - m_speedSlider->value()); /* slider alto = veloce */
}

void SimulatorePage::stopAuto() {
    m_timer->stop();
    m_autoBtn->setText("\u25b6\u25b6 Auto");
}

