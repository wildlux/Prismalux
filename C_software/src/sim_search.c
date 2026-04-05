/*
 * sim_search.c — Algoritmi di Ricerca (4)
 * =========================================
 * Ricerca Binaria, Ricerca Lineare, Jump Search, Interpolation Search
 */
#include "sim_common.h"

/* ── Ricerca Binaria ─────────────────────────────────────────────────── */
int sim_binary_search(void) {
    int base[] = {5,12,18,25,34,47,56,63,71,82};
    int n = 10;
    sim_log_reset();

    srand((unsigned)time(NULL));
    int target_idx = rand() % n;
    int target = base[target_idx];

    char intro[128];
    snprintf(intro, sizeof intro, "Ricerca Binaria: cerca %d nella lista ordinata — O(log n)", target);
    sim_mostra_array(base, n, -1,-1,-1,-1,-1, 0, intro, "\xf0\x9f\x94\x8d Ricerca Binaria");
    if (sim_aspetta_passo()) return 0;

    int lo = 0, hi = n - 1, trovato = -1;
    char msg[128];
    int passo = 0;
    while (lo <= hi) {
        passo++;
        int mid = (lo + hi) / 2;
        snprintf(msg, sizeof msg, "Passo %d: lo=%d hi=%d mid=%d (val=%d)", passo, lo, hi, mid, base[mid]);
        sim_log_push(msg);
        sim_mostra_array(base, n, lo, hi,-1,-1, mid, 0, msg, "\xf0\x9f\x94\x8d Ricerca Binaria");
        if (sim_aspetta_passo()) return 0;

        if (base[mid] == target) {
            trovato = mid;
            snprintf(msg, sizeof msg, "\xe2\x9c\x94 Trovato! %d \xe2\x86\x92 posizione [%d]", target, mid);
            sim_log_push(msg);
            sim_mostra_array(base, n, -1,-1,-1,-1, mid, 0, msg, "\xf0\x9f\x94\x8d Ricerca Binaria — TROVATO");
            sim_aspetta_passo();
            break;
        } else if (base[mid] < target) {
            snprintf(msg, sizeof msg, "  %d < %d: cerco a destra (lo=%d)", base[mid], target, mid+1);
            sim_log_push(msg);
            lo = mid + 1;
        } else {
            snprintf(msg, sizeof msg, "  %d > %d: cerco a sinistra (hi=%d)", base[mid], target, mid-1);
            sim_log_push(msg);
            hi = mid - 1;
        }
    }
    if (trovato < 0) {
        sim_log_push("Elemento non trovato.");
        sim_mostra_array(base, n, -1,-1,-1,-1,-1, 0, "Elemento non trovato.", "\xf0\x9f\x94\x8d Ricerca Binaria");
        sim_aspetta_passo();
    }
    return 1;
}

/* ── Ricerca Lineare ─────────────────────────────────────────────────── */
int sim_ricerca_lineare(void) {
    int arr[10]; sim_genera_array(arr, 10, 1, 50);
    int n = 10;
    sim_log_reset();
    srand((unsigned)time(NULL));
    int target = arr[rand() % n];
    char intro[128];
    snprintf(intro, sizeof intro, "Ricerca Lineare: cerca %d scansionando ogni elemento — O(n)", target);
    sim_mostra_array(arr, n, -1,-1,-1,-1,-1, 0, intro, "\xf0\x9f\x94\x8e Ricerca Lineare");
    if (sim_aspetta_passo()) return 0;
    char msg[128];
    for (int i = 0; i < n; i++) {
        snprintf(msg, sizeof msg, "Passo %d: arr[%d]=%d %s %d", i+1, i, arr[i],
                 arr[i]==target ? "==" : "!=", target);
        sim_log_push(msg);
        sim_mostra_array(arr, n, i,-1,-1,-1, arr[i]==target ? i : -1, 0, msg, "\xf0\x9f\x94\x8e Ricerca Lineare");
        if (sim_aspetta_passo()) return 0;
        if (arr[i] == target) {
            snprintf(msg, sizeof msg, "\xe2\x9c\x94 Trovato %d in posizione [%d]!", target, i);
            sim_log_push(msg);
            sim_mostra_array(arr, n, -1,-1,-1,-1, i, 0, msg, "\xf0\x9f\x94\x8e Ricerca Lineare — TROVATO");
            sim_aspetta_passo();
            return 1;
        }
    }
    sim_log_push("Elemento non trovato.");
    sim_mostra_array(arr, n, -1,-1,-1,-1,-1, 0, "Elemento non trovato.", "\xf0\x9f\x94\x8e Ricerca Lineare");
    sim_aspetta_passo();
    return 1;
}

/* ── Jump Search ─────────────────────────────────────────────────────── */
int sim_jump_search(void) {
    int base[] = {3,8,14,20,27,35,44,54,65,77};
    int n = 10;
    sim_log_reset();
    srand((unsigned)time(NULL));
    int target = base[rand() % n];
    char intro[128];
    snprintf(intro, sizeof intro, "Jump Search: salta di \xe2\x88\x9an=3 poi scan lineare — O(\xe2\x88\x9an)  Target=%d", target);
    sim_mostra_array(base, n, -1,-1,-1,-1,-1, 0, intro, "\xf0\x9f\xa6\x98 Jump Search");
    if (sim_aspetta_passo()) return 0;
    int step = 3;
    int prev = 0, cur = 0;
    char msg[128];
    while (cur < n && base[cur] < target) {
        snprintf(msg, sizeof msg, "Salto: [%d]=%d < %d, avanzo a [%d]", cur, base[cur], target, cur+step);
        sim_log_push(msg);
        sim_mostra_array(base, n, cur,-1,-1,-1,-1, 0, msg, "\xf0\x9f\xa6\x98 Jump Search");
        if (sim_aspetta_passo()) return 0;
        prev = cur; cur += step;
        if (cur >= n) cur = n - 1;
    }
    for (int i = prev; i <= cur && i < n; i++) {
        snprintf(msg, sizeof msg, "Scan lineare: [%d]=%d %s %d", i, base[i], base[i]==target?"==":"!=", target);
        sim_log_push(msg);
        sim_mostra_array(base, n, i,-1,-1,-1, base[i]==target ? i : -1, 0, msg, "\xf0\x9f\xa6\x98 Jump Search");
        if (sim_aspetta_passo()) return 0;
        if (base[i] == target) {
            snprintf(msg, sizeof msg, "\xe2\x9c\x94 Trovato %d in [%d]!", target, i);
            sim_log_push(msg);
            sim_mostra_array(base, n,-1,-1,-1,-1,i,0,msg,"\xf0\x9f\xa6\x98 Jump Search — TROVATO");
            sim_aspetta_passo(); return 1;
        }
    }
    sim_log_push("Non trovato."); sim_aspetta_passo(); return 1;
}

/* ── Interpolation Search ────────────────────────────────────────────── */
int sim_interpolation_search(void) {
    int arr[10] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
    int n = 10;
    srand((unsigned)time(NULL));
    int target = arr[rand() % n];
    sim_log_reset();
    char msg[128];

    snprintf(msg, sizeof msg,
        "Interpolation Search: target=%d — stima posizione con formula proporzionale — O(log log n)",
        target);
    sim_mostra_array(arr, n,-1,-1,-1,-1,-1,0, msg, "\xf0\x9f\x8e\xaf Interpolation Search");
    if (sim_aspetta_passo()) return 0;

    int lo = 0, hi = n-1;
    while (lo <= hi && target >= arr[lo] && target <= arr[hi]) {
        int pos = lo + (int)(((double)(target - arr[lo]) / (arr[hi] - arr[lo])) * (hi - lo));
        snprintf(msg, sizeof msg, "pos = %d + (%d-%d)/(%d-%d)*(%d-%d) = %d",
                 lo, target, arr[lo], arr[hi], arr[lo], hi, lo, pos);
        sim_log_push(msg);
        sim_mostra_array(arr, n, pos,-1,-1,-1,-1,0, msg, "\xf0\x9f\x8e\xaf Interpolation Search");
        if (sim_aspetta_passo()) return 0;

        if (arr[pos] == target) {
            sim_mostra_array(arr, n,-1,-1,-1,-1,pos,0,"\xe2\x9c\x94 Trovato!","\xf0\x9f\x8e\xaf Interpolation — TROVATO");
            sim_aspetta_passo(); return 1;
        }
        if (arr[pos] < target) lo = pos + 1;
        else hi = pos - 1;
    }
    sim_log_push("Non trovato");
    sim_mostra_array(arr, n,-1,-1,-1,-1,-1,0,"Non trovato","\xf0\x9f\x8e\xaf Interpolation Search");
    sim_aspetta_passo(); return 1;
}
