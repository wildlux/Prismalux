/*
 * sim_strutture.c — Strutture Dati (5)
 * =======================================
 * Stack e Queue, Linked List, BST, Hash Table, Min-Heap
 */
#include "sim_common.h"

/* ── Stack e Queue ───────────────────────────────────────────────────── */
int sim_stack_queue(void) {
    sim_log_reset();
    int stack[8], stk_top = 0;
    int queue[8], q_front = 0, q_back = 0;
    char msg[128];
    int vals[] = {10, 20, 30, 40, 50};

    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x93\xa6  Stack (LIFO) e Queue (FIFO)\n" RST);
    printf(GRY "  Stack: push/pop dalla cima — LIFO (Last In First Out)\n");
    printf("  Queue: enqueue in fondo, dequeue dalla testa — FIFO\n\n" RST);
    printf(GRY "  [INVIO per iniziare] " RST); fflush(stdout);
    if (sim_aspetta_passo()) return 0;

    for (int i = 0; i < 5; i++) {
        stack[stk_top++] = vals[i];
        snprintf(msg, sizeof msg, "Stack PUSH %d | Stack: ", vals[i]);
        sim_log_push(msg);
        clear_screen(); print_header();
        printf(CYN BLD "\n  \xf0\x9f\x93\xa6  Stack — PUSH %d\n\n" RST, vals[i]);
        printf("  Cima \xe2\x86\x93\n");
        for (int j = stk_top-1; j >= 0; j--)
            printf("  " GRN "[%d]" RST "\n", stack[j]);
        printf("  Fondo\n\n" GRY "  [INVIO=continua  ESC=esci] " RST); fflush(stdout);
        if (sim_aspetta_passo()) return 0;
    }

    for (int i = 0; i < 3; i++) {
        int popped = stack[--stk_top];
        snprintf(msg, sizeof msg, "Stack POP = %d", popped);
        sim_log_push(msg);
        clear_screen(); print_header();
        printf(CYN BLD "\n  \xf0\x9f\x93\xa6  Stack — POP = " YLW "%d" RST "\n\n", popped);
        for (int j = stk_top-1; j >= 0; j--)
            printf("  " GRN "[%d]" RST "\n", stack[j]);
        printf("\n" GRY "  [INVIO=continua  ESC=esci] " RST); fflush(stdout);
        if (sim_aspetta_passo()) return 0;
    }

    for (int i = 0; i < 4; i++) {
        queue[q_back++] = vals[i];
        snprintf(msg, sizeof msg, "Queue ENQUEUE %d", vals[i]);
        sim_log_push(msg);
        clear_screen(); print_header();
        printf(CYN BLD "\n  \xf0\x9f\x93\xa6  Queue — ENQUEUE %d\n\n" RST, vals[i]);
        printf("  Testa\xe2\x86\x92 ");
        for (int j = q_front; j < q_back; j++) printf(GRN "[%d]" RST " ", queue[j]);
        printf("\xe2\x86\x90" "Coda\n\n" GRY "  [INVIO=continua  ESC=esci] " RST); fflush(stdout);
        if (sim_aspetta_passo()) return 0;
    }

    for (int i = 0; i < 2; i++) {
        int deq = queue[q_front++];
        snprintf(msg, sizeof msg, "Queue DEQUEUE = %d", deq);
        sim_log_push(msg);
        clear_screen(); print_header();
        printf(CYN BLD "\n  \xf0\x9f\x93\xa6  Queue — DEQUEUE = " YLW "%d" RST "\n\n", deq);
        printf("  Testa\xe2\x86\x92 ");
        for (int j = q_front; j < q_back; j++) printf(GRN "[%d]" RST " ", queue[j]);
        printf("\xe2\x86\x90" "Coda\n\n" GRY "  [INVIO=continua  ESC=esci] " RST); fflush(stdout);
        if (sim_aspetta_passo()) return 0;
    }
    return 1;
}

/* ── Linked List ─────────────────────────────────────────────────────── */
int sim_linked_list(void) {
    sim_log_reset();
    char msg[128];
    int vals[] = {5, 10, 15, 20, 25};
    int n = 5;
    int data[8], next2[8];
    int head = -1, sz = 0;

    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x94\x97  Linked List — Lista Collegata Singola\n" RST);
    printf(GRY "  Ogni nodo contiene: valore + puntatore al prossimo\n\n" RST);
    printf(GRY "  [INVIO per iniziare] " RST); fflush(stdout);
    if (sim_aspetta_passo()) return 0;

    for (int i = 0; i < n; i++) {
        data[sz] = vals[i];
        next2[sz] = head;
        head = sz++;
        snprintf(msg, sizeof msg, "Inserisci %d in testa", vals[i]);
        sim_log_push(msg);
        clear_screen(); print_header();
        printf(CYN BLD "\n  \xf0\x9f\x94\x97  Linked List — inserimento in testa\n\n" RST);
        printf("  HEAD \xe2\x86\x92 ");
        int cur = head;
        while (cur != -1) {
            printf(GRN "[%d]" RST, data[cur]);
            if (next2[cur] != -1) printf(" \xe2\x86\x92 ");
            cur = next2[cur];
        }
        printf(" \xe2\x86\x92 NULL\n\n");
        for (int l = 0; l < _sim_log_n; l++) printf(GRY "  %s\n" RST, _sim_log[l]);
        printf("\n" GRY "  [INVIO=continua  ESC=esci] " RST); fflush(stdout);
        if (sim_aspetta_passo()) return 0;
    }

    sim_log_push("Elimina nodo con valore 15");
    int prev = -1, cur2 = head;
    while (cur2 != -1 && data[cur2] != 15) { prev = cur2; cur2 = next2[cur2]; }
    if (cur2 != -1) {
        if (prev == -1) head = next2[cur2];
        else next2[prev] = next2[cur2];
    }
    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x94\x97  Linked List — dopo eliminazione di 15\n\n" RST);
    printf("  HEAD \xe2\x86\x92 ");
    int cur3 = head;
    while (cur3 != -1) {
        printf(GRN "[%d]" RST, data[cur3]);
        if (next2[cur3] != -1) printf(" \xe2\x86\x92 ");
        cur3 = next2[cur3];
    }
    printf(" \xe2\x86\x92 NULL\n\n");
    printf(GRY "  [INVIO] " RST); fflush(stdout); sim_aspetta_passo(); return 1;
}

/* ── BST (Binary Search Tree) ────────────────────────────────────────── */
int sim_bst(void) {
    int vals[] = {50, 30, 70, 20, 40, 60, 80};
    int n = 7;
    int left2[8], right2[8], key[8];
    int sz = 0, root = -1;
    for (int i = 0; i < 8; i++) { left2[i] = -1; right2[i] = -1; }
    sim_log_reset();
    char msg[128];

    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x8c\xb2  BST — Binary Search Tree\n" RST);
    printf(GRY "  Inserisci: sinistra < nodo < destra\n\n" RST);
    printf(GRY "  [INVIO per iniziare] " RST); fflush(stdout);
    if (sim_aspetta_passo()) return 0;

    for (int i = 0; i < n; i++) {
        int v = vals[i];
        key[sz] = v;
        int ni = sz++;
        if (root == -1) { root = ni; }
        else {
            int cur4 = root;
            while (1) {
                if (v < key[cur4]) {
                    if (left2[cur4] == -1) { left2[cur4] = ni; break; }
                    else cur4 = left2[cur4];
                } else {
                    if (right2[cur4] == -1) { right2[cur4] = ni; break; }
                    else cur4 = right2[cur4];
                }
            }
        }
        snprintf(msg, sizeof msg, "Inserisci %d", v);
        sim_log_push(msg);
        clear_screen(); print_header();
        printf(CYN BLD "\n  \xf0\x9f\x8c\xb2  BST — inserimento %d\n\n" RST, v);
        printf("  Livello 0:                 " GRN "[%d]" RST "\n", key[root]);
        if (left2[root] != -1 || right2[root] != -1) {
            printf("  Livello 1:      ");
            if (left2[root] != -1) printf(GRN "[%d]" RST "           ", key[left2[root]]);
            else printf(GRY "[--]" RST "           ");
            if (right2[root] != -1) printf(GRN "[%d]" RST, key[right2[root]]);
            else printf(GRY "[--]" RST);
            printf("\n");
        }
        if (sz > 3) {
            printf("  Livello 2: ");
            int nodes2[] = {left2[root], right2[root]};
            for (int p = 0; p < 2; p++) {
                if (nodes2[p] != -1) {
                    if (left2[nodes2[p]] != -1) printf(GRN "[%d]" RST " ", key[left2[nodes2[p]]]);
                    else printf(GRY "[ ] " RST);
                    if (right2[nodes2[p]] != -1) printf(GRN "[%d]" RST " ", key[right2[nodes2[p]]]);
                    else printf(GRY "[ ] " RST);
                } else printf(GRY "[ ] [ ] " RST);
            }
            printf("\n");
        }
        printf("\n");
        for (int l = 0; l < _sim_log_n; l++) printf(GRY "  %s\n" RST, _sim_log[l]);
        printf("\n" GRY "  [INVIO=continua  ESC=esci] " RST); fflush(stdout);
        if (sim_aspetta_passo()) return 0;
    }
    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x8c\xb2  BST — InOrder traversal (risultato ordinato)\n\n" RST);
    int stk2[8], stk2_top = 0;
    int cur5 = root;
    printf("  InOrder: ");
    while (cur5 != -1 || stk2_top > 0) {
        while (cur5 != -1) { stk2[stk2_top++] = cur5; cur5 = left2[cur5]; }
        cur5 = stk2[--stk2_top];
        printf(GRN "%d " RST, key[cur5]);
        cur5 = right2[cur5];
    }
    printf("\n\n" GRY "  [INVIO] " RST); fflush(stdout); sim_aspetta_passo(); return 1;
}

/* ── Hash Table (chaining) ───────────────────────────────────────────── */
int sim_hash_table(void) {
    int SIZE = 7;
    int buckets[7][4], bsz2[7] = {0};
    int vals[] = {14, 17, 11, 16, 19, 33, 2, 8};
    int n = 8;
    sim_log_reset();
    char msg[128];

    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x97\x82  Hash Table — chaining, dimensione %d\n" RST, SIZE);
    printf(GRY "  h(k) = k mod %d — collisioni risolte con liste\n\n" RST, SIZE);
    printf(GRY "  [INVIO per iniziare] " RST); fflush(stdout);
    if (sim_aspetta_passo()) return 0;

    for (int i = 0; i < n; i++) {
        int v = vals[i];
        int h = v % SIZE;
        if (bsz2[h] < 4) buckets[h][bsz2[h]++] = v;
        snprintf(msg, sizeof msg, "Inserisci %d: h(%d)=%d mod %d = %d", v, v, v, SIZE, h);
        sim_log_push(msg);

        clear_screen(); print_header();
        printf(CYN BLD "\n  \xf0\x9f\x97\x82  Hash Table — inserimento %d\n\n" RST, v);
        for (int bi = 0; bi < SIZE; bi++) {
            printf("  [%d]: ", bi);
            for (int j = 0; j < bsz2[bi]; j++) {
                if (buckets[bi][j] == v && bi == h) printf(YLW "[%d]" RST " \xe2\x86\x92 ", buckets[bi][j]);
                else printf(GRN "[%d]" RST " \xe2\x86\x92 ", buckets[bi][j]);
            }
            printf("NULL\n");
        }
        printf("\n");
        for (int l = 0; l < _sim_log_n; l++) printf(GRY "  %s\n" RST, _sim_log[l]);
        printf("\n" GRY "  [INVIO=continua  ESC=esci] " RST); fflush(stdout);
        if (sim_aspetta_passo()) return 0;
    }
    return 1;
}

/* ── Min-Heap ────────────────────────────────────────────────────────── */
int sim_min_heap(void) {
    int heap[10], hsz = 0;
    int vals[] = {9, 4, 7, 1, 8, 3, 6};
    int n = 7;
    sim_log_reset();
    char msg[128];

    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x94\xba  Min-Heap — inserimento e extract-min\n" RST);
    printf(GRY "  Propriet\xc3\xa0: parent <= figli. Radice = elemento minimo\n\n" RST);
    printf(GRY "  [INVIO per iniziare] " RST); fflush(stdout);
    if (sim_aspetta_passo()) return 0;

    for (int i = 0; i < n; i++) {
        heap[hsz] = vals[i]; int pos = hsz++;
        while (pos > 0) {
            int par = (pos-1)/2;
            if (heap[par] > heap[pos]) { int t=heap[par];heap[par]=heap[pos];heap[pos]=t; pos=par; }
            else break;
        }
        snprintf(msg, sizeof msg, "Inserisci %d (sift-up)", vals[i]);
        sim_log_push(msg);

        clear_screen(); print_header();
        printf(CYN BLD "\n  \xf0\x9f\x94\xba  Min-Heap — inserimento %d\n\n" RST, vals[i]);
        printf("  Heap array: ");
        for (int j = 0; j < hsz; j++) printf(GRN "[%d]" RST " ", heap[j]);
        printf("\n\n  Struttura:\n");
        if (hsz > 0) printf("            " GRN "[%d]" RST "\n", heap[0]);
        if (hsz > 1) {
            printf("        ");
            if (hsz > 1) printf(GRN "[%d]" RST "       ", heap[1]);
            if (hsz > 2) printf(GRN "[%d]" RST, heap[2]);
            printf("\n");
        }
        if (hsz > 3) {
            printf("      ");
            for (int j = 3; j < hsz && j < 7; j++) printf(GRN "[%d]" RST "  ", heap[j]);
            printf("\n");
        }
        printf("\n");
        for (int l = 0; l < _sim_log_n; l++) printf(GRY "  %s\n" RST, _sim_log[l]);
        printf("\n" GRY "  [INVIO=continua  ESC=esci] " RST); fflush(stdout);
        if (sim_aspetta_passo()) return 0;
    }

    for (int ex = 0; ex < 2; ex++) {
        int minv = heap[0];
        heap[0] = heap[--hsz];
        int pos2 = 0;
        while (1) {
            int smallest = pos2, l2 = 2*pos2+1, r2 = 2*pos2+2;
            if (l2 < hsz && heap[l2] < heap[smallest]) smallest = l2;
            if (r2 < hsz && heap[r2] < heap[smallest]) smallest = r2;
            if (smallest == pos2) break;
            int t=heap[pos2];heap[pos2]=heap[smallest];heap[smallest]=t;
            pos2 = smallest;
        }
        snprintf(msg, sizeof msg, "Extract-min = %d (sift-down)", minv);
        sim_log_push(msg);
        clear_screen(); print_header();
        printf(CYN BLD "\n  \xf0\x9f\x94\xba  Min-Heap — extract-min = " YLW "%d" RST "\n\n", minv);
        printf("  Heap: ");
        for (int j = 0; j < hsz; j++) printf(GRN "[%d]" RST " ", heap[j]);
        printf("\n\n");
        for (int l = 0; l < _sim_log_n; l++) printf(GRY "  %s\n" RST, _sim_log[l]);
        printf("\n" GRY "  [INVIO=continua  ESC=esci] " RST); fflush(stdout);
        if (sim_aspetta_passo()) return 0;
    }
    return 1;
}
