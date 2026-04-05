/*
 * sim_sort.c — Algoritmi di Ordinamento (15)
 * ===========================================
 * Bubble, Selection, Insertion, Merge, Quick, Heap, Shell,
 * Cocktail, Counting, Gnome, Comb, Pancake, Radix, Bucket, Tim
 */
#include "sim_common.h"

/* ── Bubble Sort ─────────────────────────────────────────────────────── */
int sim_bubble_sort(void) {
    int arr[8]; sim_genera_array(arr, 8, 5, 95);
    int n = 8;
    sim_log_reset();
    char msg[128];

    sim_mostra_array(arr, n, -1,-1,-1,-1,-1, 0,
        "Bubble Sort: confronto vicini, scambia se nell'ordine sbagliato — O(n\xc2\xb2)",
        "\xf0\x9f\x92\xa8 Bubble Sort");
    if (sim_aspetta_passo()) return 0;

    unsigned long long ordinati = 0;
    int passo = 0;
    for (int i = 0; i < n - 1; i++) {
        int scambiato = 0;
        for (int j = 0; j < n - 1 - i; j++) {
            passo++;
            snprintf(msg, sizeof msg, "Passo %d: confronto [%d]=%d e [%d]=%d",
                     passo, j, arr[j], j+1, arr[j+1]);
            sim_log_push(msg);
            sim_mostra_array(arr, n, j, j+1, -1,-1,-1, ordinati, msg, "\xf0\x9f\x92\xa8 Bubble Sort");
            if (sim_aspetta_passo()) return 0;

            if (arr[j] > arr[j+1]) {
                int tmp = arr[j]; arr[j] = arr[j+1]; arr[j+1] = tmp;
                scambiato = 1;
                snprintf(msg, sizeof msg, "  \xe2\x86\x92 %d > %d: SCAMBIO!", arr[j+1], arr[j]);
                sim_log_push(msg);
                sim_mostra_array(arr, n, -1,-1, j, j+1,-1, ordinati, msg, "\xf0\x9f\x92\xa8 Bubble Sort");
                if (sim_aspetta_passo()) return 0;
            } else {
                snprintf(msg, sizeof msg, "  \xe2\x86\x92 %d \xe2\x89\xa4 %d: gi\xc3\xa0 in ordine", arr[j], arr[j+1]);
                sim_log_push(msg);
            }
        }
        ordinati |= (1ULL << (n - 1 - i));
        if (!scambiato) break;
    }
    ordinati = (1ULL << n) - 1;
    sim_log_push("\xe2\x9c\x94 Lista ordinata!");
    sim_mostra_array(arr, n, -1,-1,-1,-1,-1, ordinati,
        "\xe2\x9c\x94 Lista ordinata! Nessun altro scambio necessario.",
        "\xf0\x9f\x92\xa8 Bubble Sort — COMPLETATO");
    sim_aspetta_passo();
    return 1;
}

/* ── Selection Sort ──────────────────────────────────────────────────── */
int sim_selection_sort(void) {
    int arr[8]; sim_genera_array(arr, 8, 5, 95);
    int n = 8;
    sim_log_reset();
    char msg[128];

    sim_mostra_array(arr, n, -1,-1,-1,-1,-1, 0,
        "Selection Sort: trova il minimo, mettilo in testa, ripeti — O(n\xc2\xb2)",
        "\xf0\x9f\x8e\xaf Selection Sort");
    if (sim_aspetta_passo()) return 0;

    unsigned long long ordinati = 0;
    for (int i = 0; i < n - 1; i++) {
        int min_idx = i;
        snprintf(msg, sizeof msg, "Passo %d: cerco il minimo da [%d] in poi", i+1, i);
        sim_log_push(msg);

        for (int j = i + 1; j < n; j++) {
            sim_mostra_array(arr, n, min_idx, j, -1,-1,-1, ordinati, msg, "\xf0\x9f\x8e\xaf Selection Sort");
            if (sim_aspetta_passo()) return 0;
            if (arr[j] < arr[min_idx]) {
                min_idx = j;
                snprintf(msg, sizeof msg, "  Nuovo minimo trovato: [%d]=%d", min_idx, arr[min_idx]);
                sim_log_push(msg);
            }
        }

        if (min_idx != i) {
            int tmp = arr[i]; arr[i] = arr[min_idx]; arr[min_idx] = tmp;
            snprintf(msg, sizeof msg, "Scambio [%d]=%d \xe2\x86\x94 [%d]=%d", i, arr[i], min_idx, arr[min_idx]);
            sim_log_push(msg);
            sim_mostra_array(arr, n, -1,-1, i, min_idx,-1, ordinati, msg, "\xf0\x9f\x8e\xaf Selection Sort");
            if (sim_aspetta_passo()) return 0;
        }
        ordinati |= (1ULL << i);
    }
    ordinati = (1ULL << n) - 1;
    sim_log_push("\xe2\x9c\x94 Lista ordinata!");
    sim_mostra_array(arr, n, -1,-1,-1,-1,-1, ordinati,
        "\xe2\x9c\x94 Lista ordinata!", "\xf0\x9f\x8e\xaf Selection Sort — COMPLETATO");
    sim_aspetta_passo();
    return 1;
}

/* ── Insertion Sort ──────────────────────────────────────────────────── */
int sim_insertion_sort(void) {
    int arr[8]; sim_genera_array(arr, 8, 5, 95);
    int n = 8;
    sim_log_reset();
    char msg[128];

    sim_mostra_array(arr, n, -1,-1,-1,-1,-1, 0,
        "Insertion Sort: prendi elemento, inseriscilo nel posto giusto — O(n\xc2\xb2), O(n) best",
        "\xf0\x9f\x83\x8f Insertion Sort");
    if (sim_aspetta_passo()) return 0;

    unsigned long long ordinati = 1ULL; /* il primo è già "ordinato" */
    for (int i = 1; i < n; i++) {
        int chiave = arr[i];
        int j = i - 1;
        snprintf(msg, sizeof msg, "Inserisco [%d]=%d nella parte ordinata", i, chiave);
        sim_log_push(msg);
        sim_mostra_array(arr, n, i, -1,-1,-1,-1, ordinati, msg, "\xf0\x9f\x83\x8f Insertion Sort");
        if (sim_aspetta_passo()) return 0;

        while (j >= 0 && arr[j] > chiave) {
            arr[j+1] = arr[j];
            snprintf(msg, sizeof msg, "  Sposto [%d]=%d a destra", j, arr[j]);
            sim_log_push(msg);
            sim_mostra_array(arr, n, j, j+1,-1,-1,-1, ordinati, msg, "\xf0\x9f\x83\x8f Insertion Sort");
            if (sim_aspetta_passo()) return 0;
            j--;
        }
        arr[j+1] = chiave;
        ordinati |= (1ULL << i);
        snprintf(msg, sizeof msg, "  Posiziono %d in [%d]", chiave, j+1);
        sim_log_push(msg);
        sim_mostra_array(arr, n, -1,-1,-1,-1, j+1, ordinati, msg, "\xf0\x9f\x83\x8f Insertion Sort");
        if (sim_aspetta_passo()) return 0;
    }
    unsigned long long tutti = (1ULL << n) - 1;
    sim_log_push("\xe2\x9c\x94 Lista ordinata!");
    sim_mostra_array(arr, n, -1,-1,-1,-1,-1, tutti,
        "\xe2\x9c\x94 Lista ordinata!", "\xf0\x9f\x83\x8f Insertion Sort — COMPLETATO");
    sim_aspetta_passo();
    return 1;
}

/* ── helper Merge Sort ───────────────────────────────────────────────── */
static void _merge(int *arr, int *tmp, int l, int m, int r,
                   unsigned long long *ord) {
    for (int i = l; i <= r; i++) tmp[i] = arr[i];
    int i = l, j = m + 1, k = l;
    char msg[128];
    while (i <= m && j <= r) {
        snprintf(msg, sizeof msg, "Confronto sinistra[%d]=%d con destra[%d]=%d", i, tmp[i], j, tmp[j]);
        sim_log_push(msg);
        sim_mostra_array(arr, r+1 > 8 ? 8 : r+1, i, j,-1,-1,-1, *ord, msg, "\xf0\x9f\x94\x80 Merge Sort");
        if (sim_aspetta_passo()) {
            for (int x = l; x <= r; x++) arr[x] = tmp[x];
            return;
        }
        if (tmp[i] <= tmp[j]) { arr[k++] = tmp[i++]; }
        else                  { arr[k++] = tmp[j++]; }
    }
    while (i <= m) arr[k++] = tmp[i++];
    while (j <= r) arr[k++] = tmp[j++];
    for (int x = l; x <= r; x++) *ord |= (1ULL << x);
}

/* ── Merge Sort ──────────────────────────────────────────────────────── */
int sim_merge_sort(void) {
    int arr[8]; sim_genera_array(arr, 8, 5, 95);
    int n = 8;
    int tmp[8];
    sim_log_reset();

    sim_mostra_array(arr, n, -1,-1,-1,-1,-1, 0,
        "Merge Sort: dividi a met\xc3\xa0, ordina le met\xc3\xa0, fondi — O(n log n), stabile",
        "\xf0\x9f\x94\x80 Merge Sort");
    if (sim_aspetta_passo()) return 0;

    unsigned long long ordinati = 0;
    for (int size = 1; size < n; size *= 2) {
        for (int l = 0; l < n - size; l += 2 * size) {
            int m = l + size - 1;
            int r = l + 2 * size - 1;
            if (r >= n) r = n - 1;
            char msg[128];
            snprintf(msg, sizeof msg, "Fondi blocco [%d..%d] con [%d..%d]", l, m, m+1, r);
            sim_log_push(msg);
            _merge(arr, tmp, l, m, r, &ordinati);
        }
    }
    unsigned long long tutti = (1ULL << n) - 1;
    sim_log_push("\xe2\x9c\x94 Lista ordinata!");
    sim_mostra_array(arr, n, -1,-1,-1,-1,-1, tutti,
        "\xe2\x9c\x94 Lista ordinata!", "\xf0\x9f\x94\x80 Merge Sort — COMPLETATO");
    sim_aspetta_passo();
    return 1;
}

/* ── helper Quick Sort: partition ────────────────────────────────────── */
static int _partition(int *arr, int l, int r,
                      unsigned long long *ord, int n) {
    int pivot = arr[r];
    int i = l - 1;
    char msg[128];
    snprintf(msg, sizeof msg, "Pivot = [%d]=%d", r, pivot);
    sim_log_push(msg);
    sim_mostra_array(arr, n, -1, r,-1,-1,-1, *ord, msg, "\xe2\x9a\xa1 Quick Sort");
    sim_aspetta_passo();

    for (int j = l; j < r; j++) {
        snprintf(msg, sizeof msg, "Confronto [%d]=%d con pivot %d", j, arr[j], pivot);
        sim_mostra_array(arr, n, j, r,-1,-1,-1, *ord, msg, "\xe2\x9a\xa1 Quick Sort");
        if (sim_aspetta_passo()) return i + 1;
        if (arr[j] <= pivot) {
            i++;
            int tmp = arr[i]; arr[i] = arr[j]; arr[j] = tmp;
            snprintf(msg, sizeof msg, "  %d \xe2\x89\xa4 pivot: scambio [%d]\xe2\x86\x94[%d]", arr[i], i, j);
            sim_log_push(msg);
            sim_mostra_array(arr, n, -1,-1, i, j,-1, *ord, msg, "\xe2\x9a\xa1 Quick Sort");
            if (sim_aspetta_passo()) return i + 1;
        }
    }
    int tmp = arr[i+1]; arr[i+1] = arr[r]; arr[r] = tmp;
    *ord |= (1ULL << (i+1));
    snprintf(msg, sizeof msg, "Pivot %d in posizione finale [%d]", arr[i+1], i+1);
    sim_log_push(msg);
    sim_mostra_array(arr, n, -1,-1,-1,-1, i+1, *ord, msg, "\xe2\x9a\xa1 Quick Sort");
    sim_aspetta_passo();
    return i + 1;
}

/* ── Quick Sort ──────────────────────────────────────────────────────── */
int sim_quick_sort(void) {
    int arr[8]; sim_genera_array(arr, 8, 5, 95);
    int n = 8;
    sim_log_reset();

    sim_mostra_array(arr, n, -1,-1,-1,-1,-1, 0,
        "Quick Sort: scegli pivot, partiziona, ricorsione — O(n log n) medio",
        "\xe2\x9a\xa1 Quick Sort");
    if (sim_aspetta_passo()) return 0;

    int stack[16]; int top = -1;
    stack[++top] = 0; stack[++top] = n - 1;
    unsigned long long ordinati = 0;

    while (top >= 0) {
        int r = stack[top--];
        int l = stack[top--];
        if (l < r) {
            int p = _partition(arr, l, r, &ordinati, n);
            if (p - 1 > l) { stack[++top] = l; stack[++top] = p - 1; }
            if (p + 1 < r) { stack[++top] = p + 1; stack[++top] = r; }
        }
    }
    unsigned long long tutti = (1ULL << n) - 1;
    sim_log_push("\xe2\x9c\x94 Lista ordinata!");
    sim_mostra_array(arr, n, -1,-1,-1,-1,-1, tutti,
        "\xe2\x9c\x94 Lista ordinata!", "\xe2\x9a\xa1 Quick Sort — COMPLETATO");
    sim_aspetta_passo();
    return 1;
}

/* ── helper Heap Sort: sift_down ─────────────────────────────────────── */
static void _sift_down(int *arr, int n, int i,
                        unsigned long long *ord) {
    char msg[128];
    while (1) {
        int largest = i, l = 2*i+1, r = 2*i+2;
        if (l < n && arr[l] > arr[largest]) largest = l;
        if (r < n && arr[r] > arr[largest]) largest = r;
        if (largest == i) break;
        snprintf(msg, sizeof msg, "sift-down: scambio [%d]=%d \xe2\x86\x94 [%d]=%d", i, arr[i], largest, arr[largest]);
        sim_log_push(msg);
        sim_mostra_array(arr, n, i, largest,-1,-1,-1, *ord, msg, "\xf0\x9f\x94\xba Heap Sort");
        sim_aspetta_passo();
        int tmp = arr[i]; arr[i] = arr[largest]; arr[largest] = tmp;
        i = largest;
    }
}

/* ── Heap Sort ───────────────────────────────────────────────────────── */
int sim_heap_sort(void) {
    int arr[8]; sim_genera_array(arr, 8, 5, 95);
    int n = 8;
    sim_log_reset();
    sim_mostra_array(arr, n,-1,-1,-1,-1,-1,0,
        "Heap Sort: costruisci max-heap, estrai radice — O(n log n)",
        "\xf0\x9f\x94\xba Heap Sort");
    if (sim_aspetta_passo()) return 0;
    unsigned long long ordinati = 0;
    for (int i = n/2-1; i >= 0; i--) _sift_down(arr, n, i, &ordinati);
    char msg[128];
    for (int i = n-1; i > 0; i--) {
        snprintf(msg, sizeof msg, "Estraggo radice %d in posizione [%d]", arr[0], i);
        sim_log_push(msg);
        sim_mostra_array(arr, n,-1,-1,0,i,-1,ordinati,msg,"\xf0\x9f\x94\xba Heap Sort");
        if (sim_aspetta_passo()) return 0;
        int tmp = arr[0]; arr[0] = arr[i]; arr[i] = tmp;
        ordinati |= (1ULL << i);
        _sift_down(arr, i, 0, &ordinati);
    }
    ordinati = (1ULL << n) - 1;
    sim_log_push("\xe2\x9c\x94 Lista ordinata!");
    sim_mostra_array(arr, n,-1,-1,-1,-1,-1,ordinati,"\xe2\x9c\x94 Lista ordinata!","\xf0\x9f\x94\xba Heap Sort — COMPLETATO");
    sim_aspetta_passo(); return 1;
}

/* ── Shell Sort ──────────────────────────────────────────────────────── */
int sim_shell_sort(void) {
    int arr[8]; sim_genera_array(arr, 8, 5, 95);
    int n = 8;
    sim_log_reset();
    sim_mostra_array(arr, n,-1,-1,-1,-1,-1,0,
        "Shell Sort: Insertion Sort con gap decrescente — O(n log\xc2\xb2 n)",
        "\xf0\x9f\x90\x9a Shell Sort");
    if (sim_aspetta_passo()) return 0;
    char msg[128];
    for (int gap = n/2; gap > 0; gap /= 2) {
        snprintf(msg, sizeof msg, "Gap = %d: confronto elementi distanti %d", gap, gap);
        sim_log_push(msg);
        for (int i = gap; i < n; i++) {
            int tmp = arr[i], j = i;
            while (j >= gap && arr[j-gap] > tmp) {
                snprintf(msg, sizeof msg, "  Sposto [%d]=%d a [%d]", j-gap, arr[j-gap], j);
                sim_log_push(msg);
                sim_mostra_array(arr, n, j-gap, j,-1,-1,-1, 0, msg, "\xf0\x9f\x90\x9a Shell Sort");
                if (sim_aspetta_passo()) return 0;
                arr[j] = arr[j-gap]; j -= gap;
            }
            arr[j] = tmp;
        }
    }
    unsigned long long tutti = (1ULL << n) - 1;
    sim_log_push("\xe2\x9c\x94 Lista ordinata!");
    sim_mostra_array(arr, n,-1,-1,-1,-1,-1,tutti,"\xe2\x9c\x94 Lista ordinata!","\xf0\x9f\x90\x9a Shell Sort — COMPLETATO");
    sim_aspetta_passo(); return 1;
}

/* ── Cocktail Sort ───────────────────────────────────────────────────── */
int sim_cocktail_sort(void) {
    int arr[8]; sim_genera_array(arr, 8, 5, 95);
    int n = 8;
    sim_log_reset();
    sim_mostra_array(arr, n,-1,-1,-1,-1,-1,0,
        "Cocktail Sort: Bubble bidirezionale — O(n\xc2\xb2)",
        "\xf0\x9f\x8d\xb9 Cocktail Sort");
    if (sim_aspetta_passo()) return 0;
    unsigned long long ordinati = 0;
    int lo = 0, hi = n-1;
    char msg[128];
    while (lo < hi) {
        for (int j = lo; j < hi; j++) {
            snprintf(msg, sizeof msg, "\xe2\x86\x92 Confronto [%d]=%d e [%d]=%d", j, arr[j], j+1, arr[j+1]);
            sim_mostra_array(arr, n, j, j+1,-1,-1,-1, ordinati, msg, "\xf0\x9f\x8d\xb9 Cocktail");
            if (sim_aspetta_passo()) return 0;
            if (arr[j] > arr[j+1]) { int t=arr[j];arr[j]=arr[j+1];arr[j+1]=t; }
        }
        ordinati |= (1ULL << hi); hi--;
        for (int j = hi-1; j >= lo; j--) {
            snprintf(msg, sizeof msg, "\xe2\x86\x90 Confronto [%d]=%d e [%d]=%d", j, arr[j], j+1, arr[j+1]);
            sim_mostra_array(arr, n, j, j+1,-1,-1,-1, ordinati, msg, "\xf0\x9f\x8d\xb9 Cocktail");
            if (sim_aspetta_passo()) return 0;
            if (arr[j] > arr[j+1]) { int t=arr[j];arr[j]=arr[j+1];arr[j+1]=t; }
        }
        ordinati |= (1ULL << lo); lo++;
    }
    unsigned long long tutti = (1ULL << n) - 1;
    sim_log_push("\xe2\x9c\x94 Ordinato!");
    sim_mostra_array(arr, n,-1,-1,-1,-1,-1,tutti,"\xe2\x9c\x94 Ordinato!","\xf0\x9f\x8d\xb9 Cocktail — COMPLETATO");
    sim_aspetta_passo(); return 1;
}

/* ── Counting Sort ───────────────────────────────────────────────────── */
int sim_counting_sort(void) {
    int arr[10];
    srand((unsigned)time(NULL));
    for (int i = 0; i < 10; i++) arr[i] = 1 + rand() % 9;
    int n = 10;
    sim_log_reset();
    sim_mostra_array(arr, n,-1,-1,-1,-1,-1,0,
        "Counting Sort: conta occorrenze, ricostruisce — O(n+k)",
        "\xf0\x9f\x93\x8a Counting Sort");
    if (sim_aspetta_passo()) return 0;
    int cnt[10] = {0};
    char msg[128];
    for (int i = 0; i < n; i++) {
        cnt[arr[i]-1]++;
        snprintf(msg, sizeof msg, "Conto arr[%d]=%d  (conteggio[%d]=%d)", i, arr[i], arr[i], cnt[arr[i]-1]);
        sim_log_push(msg);
        sim_mostra_array(arr, n, i,-1,-1,-1,-1, 0, msg, "\xf0\x9f\x93\x8a Counting — conta");
        if (sim_aspetta_passo()) return 0;
    }
    int k = 0;
    for (int v = 1; v <= 9; v++) {
        for (int j = 0; j < cnt[v-1]; j++) {
            arr[k++] = v;
            snprintf(msg, sizeof msg, "Scrivo %d in posizione [%d]", v, k-1);
            sim_log_push(msg);
            sim_mostra_array(arr, n,-1,-1,-1,-1, k-1, (1ULL<<k)-1, msg, "\xf0\x9f\x93\x8a Counting — ricostruisce");
            if (sim_aspetta_passo()) return 0;
        }
    }
    unsigned long long tutti = (1ULL << n) - 1;
    sim_log_push("\xe2\x9c\x94 Ordinato!");
    sim_mostra_array(arr, n,-1,-1,-1,-1,-1,tutti,"\xe2\x9c\x94 Ordinato!","\xf0\x9f\x93\x8a Counting — COMPLETATO");
    sim_aspetta_passo(); return 1;
}

/* ── Gnome Sort ──────────────────────────────────────────────────────── */
int sim_gnome_sort(void) {
    int arr[8]; sim_genera_array(arr, 8, 5, 95);
    int n = 8;
    sim_log_reset();
    sim_mostra_array(arr, n,-1,-1,-1,-1,-1,0,
        "Gnome Sort: avanza se OK, scambia e torna indietro — O(n\xc2\xb2)",
        "\xf0\x9f\x8c\xbf Gnome Sort");
    if (sim_aspetta_passo()) return 0;
    int pos = 0;
    char msg[128];
    while (pos < n) {
        if (pos == 0 || arr[pos] >= arr[pos-1]) {
            snprintf(msg, sizeof msg, "Gnome avanza: [%d]=%d \xe2\x89\xa5 [%d]=%d  pos\xe2\x86\x92%d",
                     pos, arr[pos], pos-1, pos>0?arr[pos-1]:0, pos+1);
            sim_log_push(msg);
            sim_mostra_array(arr, n, pos,-1,-1,-1,-1, 0, msg, "\xf0\x9f\x8c\xbf Gnome Sort");
            if (sim_aspetta_passo()) return 0;
            pos++;
        } else {
            snprintf(msg, sizeof msg, "Gnome indietro: scambio [%d]=%d \xe2\x86\x94 [%d]=%d",
                     pos, arr[pos], pos-1, arr[pos-1]);
            sim_log_push(msg);
            sim_mostra_array(arr, n,-1,-1, pos-1, pos,-1, 0, msg, "\xf0\x9f\x8c\xbf Gnome Sort");
            if (sim_aspetta_passo()) return 0;
            int t=arr[pos];arr[pos]=arr[pos-1];arr[pos-1]=t;
            pos--;
        }
    }
    unsigned long long tutti = (1ULL << n) - 1;
    sim_log_push("\xe2\x9c\x94 Ordinato!");
    sim_mostra_array(arr, n,-1,-1,-1,-1,-1,tutti,"\xe2\x9c\x94 Ordinato!","\xf0\x9f\x8c\xbf Gnome — COMPLETATO");
    sim_aspetta_passo(); return 1;
}

/* ── Comb Sort ───────────────────────────────────────────────────────── */
int sim_comb_sort(void) {
    int arr[8]; sim_genera_array(arr, 8, 5, 95);
    int n = 8;
    sim_log_reset();
    sim_mostra_array(arr, n,-1,-1,-1,-1,-1,0,
        "Comb Sort: gap=n*0.77 decrescente, migliora Bubble — O(n\xc2\xb2) worst",
        "\xf0\x9f\x94\xa7 Comb Sort");
    if (sim_aspetta_passo()) return 0;
    int gap = n;
    char msg[128];
    while (gap > 1) {
        gap = (int)(gap / 1.3);
        if (gap < 1) gap = 1;
        snprintf(msg, sizeof msg, "Gap = %d: confronto a distanza %d", gap, gap);
        sim_log_push(msg);
        for (int i = 0; i + gap < n; i++) {
            sim_mostra_array(arr, n, i, i+gap,-1,-1,-1, 0, msg, "\xf0\x9f\x94\xa7 Comb Sort");
            if (sim_aspetta_passo()) return 0;
            if (arr[i] > arr[i+gap]) {
                int t=arr[i];arr[i]=arr[i+gap];arr[i+gap]=t;
                snprintf(msg, sizeof msg, "  Scambio [%d]\xe2\x86\x94[%d]", i, i+gap);
                sim_log_push(msg);
            }
        }
    }
    unsigned long long tutti = (1ULL << n) - 1;
    sim_log_push("\xe2\x9c\x94 Ordinato!");
    sim_mostra_array(arr, n,-1,-1,-1,-1,-1,tutti,"\xe2\x9c\x94 Ordinato!","\xf0\x9f\x94\xa7 Comb — COMPLETATO");
    sim_aspetta_passo(); return 1;
}

/* ── helper Pancake Sort ─────────────────────────────────────────────── */
static void _flip(int *arr, int k) {
    for (int i=0,j=k;i<j;i++,j--){int t=arr[i];arr[i]=arr[j];arr[j]=t;}
}
static int _max_idx(int *arr, int n) {
    int m=0; for(int i=1;i<n;i++) if(arr[i]>arr[m]) m=i; return m;
}

/* ── Pancake Sort ────────────────────────────────────────────────────── */
int sim_pancake_sort(void) {
    int arr[7]; sim_genera_array(arr, 7, 5, 95);
    int n = 7;
    sim_log_reset();
    sim_mostra_array(arr, n,-1,-1,-1,-1,-1,0,
        "Pancake Sort: flip dei prefissi — O(n\xc2\xb2)",
        "\xf0\x9f\xa5\x9e Pancake Sort");
    if (sim_aspetta_passo()) return 0;
    char msg[128];
    unsigned long long ordinati = 0;
    for (int sz = n; sz > 1; sz--) {
        int mi = _max_idx(arr, sz);
        if (mi != sz-1) {
            if (mi > 0) {
                snprintf(msg, sizeof msg, "Flip [0..%d]: porta massimo %d in testa", mi, arr[mi]);
                sim_log_push(msg);
                sim_mostra_array(arr, n,-1,-1,0,mi,-1,ordinati,msg,"\xf0\x9f\xa5\x9e Pancake");
                if (sim_aspetta_passo()) return 0;
                _flip(arr, mi);
            }
            snprintf(msg, sizeof msg, "Flip [0..%d]: porta %d in posizione finale [%d]", sz-1, arr[0], sz-1);
            sim_log_push(msg);
            sim_mostra_array(arr, n,-1,-1,0,sz-1,-1,ordinati,msg,"\xf0\x9f\xa5\x9e Pancake");
            if (sim_aspetta_passo()) return 0;
            _flip(arr, sz-1);
        }
        ordinati |= (1ULL << (sz-1));
    }
    ordinati = (1ULL << n) - 1;
    sim_log_push("\xe2\x9c\x94 Ordinato!");
    sim_mostra_array(arr, n,-1,-1,-1,-1,-1,ordinati,"\xe2\x9c\x94 Ordinato!","\xf0\x9f\xa5\x9e Pancake — COMPLETATO");
    sim_aspetta_passo(); return 1;
}

/* ── Radix Sort (LSD, base 10) ───────────────────────────────────────── */
int sim_radix_sort(void) {
    int arr[8]; sim_genera_array(arr, 8, 5, 999);
    int n = 8;
    sim_log_reset();
    char msg[128];

    sim_mostra_array(arr, n, -1,-1,-1,-1,-1, 0,
        "Radix Sort LSD: ordina cifra per cifra dalla meno significativa — O(d*n)",
        "\xf0\x9f\x94\xa2 Radix Sort");
    if (sim_aspetta_passo()) return 0;

    int max_v = arr[0];
    for (int i = 1; i < n; i++) if (arr[i] > max_v) max_v = arr[i];

    for (int exp = 1; max_v / exp > 0; exp *= 10) {
        int output[8], count[10] = {0};

        for (int i = 0; i < n; i++) count[(arr[i] / exp) % 10]++;
        for (int i = 1; i < 10; i++) count[i] += count[i-1];
        for (int i = n-1; i >= 0; i--) {
            output[--count[(arr[i] / exp) % 10]] = arr[i];
        }
        for (int i = 0; i < n; i++) arr[i] = output[i];

        snprintf(msg, sizeof msg, "Passata cifra %d: bucket e riposiziona", exp);
        sim_log_push(msg);
        sim_mostra_array(arr, n, -1,-1,-1,-1,-1, 0, msg, "\xf0\x9f\x94\xa2 Radix Sort");
        if (sim_aspetta_passo()) return 0;
    }
    unsigned long long tutti = (1ULL << n) - 1;
    sim_log_push("\xe2\x9c\x94 Ordinato!");
    sim_mostra_array(arr, n,-1,-1,-1,-1,-1,tutti,"\xe2\x9c\x94 Ordinato!","\xf0\x9f\x94\xa2 Radix Sort — COMPLETATO");
    sim_aspetta_passo(); return 1;
}

/* ── Bucket Sort ─────────────────────────────────────────────────────── */
int sim_bucket_sort(void) {
    int arr[8]; sim_genera_array(arr, 8, 1, 100);
    int n = 8;
    sim_log_reset();
    char msg[128];

    sim_mostra_array(arr, n,-1,-1,-1,-1,-1,0,
        "Bucket Sort: distribuisce in secchielli [0-24][25-49][50-74][75-100] — O(n+k)",
        "\xf0\x9f\xaa\xa3 Bucket Sort");
    if (sim_aspetta_passo()) return 0;

    int buckets[4][8], bsz[4] = {0};
    for (int i = 0; i < n; i++) {
        int b = (arr[i]-1) / 25;
        if (b > 3) b = 3;
        buckets[b][bsz[b]++] = arr[i];
        snprintf(msg, sizeof msg, "arr[%d]=%d \xe2\x86\x92 bucket[%d] (%d-%d)", i, arr[i], b, b*25+1, b*25+25);
        sim_log_push(msg);
        sim_mostra_array(arr, n, i,-1,-1,-1,-1,0, msg, "\xf0\x9f\xaa\xa3 Bucket Sort");
        if (sim_aspetta_passo()) return 0;
    }

    for (int b = 0; b < 4; b++) {
        for (int i = 1; i < bsz[b]; i++) {
            int key = buckets[b][i], j = i-1;
            while (j >= 0 && buckets[b][j] > key) { buckets[b][j+1] = buckets[b][j]; j--; }
            buckets[b][j+1] = key;
        }
    }

    int k = 0;
    for (int b = 0; b < 4; b++) for (int i = 0; i < bsz[b]; i++) arr[k++] = buckets[b][i];
    unsigned long long tutti = (1ULL << n) - 1;
    sim_log_push("\xe2\x9c\x94 Bucket fusi e ordinati!");
    sim_mostra_array(arr, n,-1,-1,-1,-1,-1,tutti,"\xe2\x9c\x94 Ordinato!","\xf0\x9f\xaa\xa3 Bucket Sort — COMPLETATO");
    sim_aspetta_passo(); return 1;
}

/* ── Tim Sort (semplificato: insertion su runs + merge) ─────────────── */
int sim_tim_sort(void) {
    int arr[8]; sim_genera_array(arr, 8, 5, 95);
    int n = 8;
    sim_log_reset();
    char msg[128];

    sim_mostra_array(arr, n,-1,-1,-1,-1,-1,0,
        "Tim Sort: insertion su run da 4, poi merge — ibrido O(n log n), Python/Java",
        "\xe2\x8f\xb1 Tim Sort");
    if (sim_aspetta_passo()) return 0;

    int RUN = 4;
    for (int i = 0; i < n; i += RUN) {
        int end = i + RUN < n ? i + RUN : n;
        for (int j = i+1; j < end; j++) {
            int key = arr[j]; int k = j-1;
            while (k >= i && arr[k] > key) { arr[k+1] = arr[k]; k--; }
            arr[k+1] = key;
            snprintf(msg, sizeof msg, "Run [%d..%d]: inserisci %d in [%d]", i, end-1, key, k+1);
            sim_log_push(msg);
            sim_mostra_array(arr, n, j,-1,-1,-1,-1,0, msg, "\xe2\x8f\xb1 Tim Sort fase 1");
            if (sim_aspetta_passo()) return 0;
        }
    }

    int tmp[8];
    int l = 0, m = RUN-1, r = n-1;
    if (m < r) {
        for (int i2 = l; i2 <= r; i2++) tmp[i2] = arr[i2];
        int ia = l, ib = m+1, ic = l;
        while (ia <= m && ib <= r) {
            if (tmp[ia] <= tmp[ib]) arr[ic++] = tmp[ia++];
            else arr[ic++] = tmp[ib++];
        }
        while (ia <= m) arr[ic++] = tmp[ia++];
        while (ib <= r) arr[ic++] = tmp[ib++];
        sim_log_push("Merge run [0..3] con run [4..7]");
        unsigned long long tutti = (1ULL << n) - 1;
        sim_mostra_array(arr, n,-1,-1,-1,-1,-1,tutti,"Merge completato","\xe2\x8f\xb1 Tim Sort — COMPLETATO");
        if (sim_aspetta_passo()) return 0;
    }
    return 1;
}
