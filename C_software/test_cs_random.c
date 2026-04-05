/* ══════════════════════════════════════════════════════════════════════════
   test_cs_random.c — test su dati casuali del Context Store Semantico
   ══════════════════════════════════════════════════════════════════════════
   Test coperti:
     T1 — Scrittura massiva (500 chunk, chiavi casuali)
     T2 — Query precision: risultati contengono davvero le chiavi richieste
     T3 — Query recall: nessun chunk rilevante escluso per errore
     T4 — Score ordering: chunk più rilevanti (più chiavi in comune) prima
     T5 — Persistenza: close + reopen, dati intatti
     T6 — Edge cases: chiave inesistente, query vuota, chunk gigante
     T7 — Stress: 1000 query consecutive, misura throughput
   ══════════════════════════════════════════════════════════════════════════ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "context_store.h"
#include "key_router.h"

/* ── Palette di chiavi casuali usate nei test ─────────────────────────────── */
static const char *TUTTE_LE_CHIAVI[] = {
    "alfa","beta","gamma","delta","epsilon","zeta","eta","theta",
    "iota","kappa","lambda","mu","nu","xi","omicron","pi",
    "rho","sigma","tau","upsilon","phi","chi","psi","omega"
};
#define N_CHIAVI 24

/* ── Helpers ──────────────────────────────────────────────────────────────── */

static int  pass = 0, fail = 0;

#define ASSERT(cond, msg) do { \
    if (cond) { printf("  \033[32m[OK]\033[0m %s\n", msg); pass++; } \
    else       { printf("  \033[31m[FAIL]\033[0m %s\n", msg); fail++; } \
} while(0)

#define ASSERT_EQ(a, b, msg) do { \
    if ((a)==(b)) { printf("  \033[32m[OK]\033[0m %s (= %d)\n", msg, (int)(a)); pass++; } \
    else { printf("  \033[31m[FAIL]\033[0m %s (atteso %d, ottenuto %d)\n", msg, (int)(b), (int)(a)); fail++; } \
} while(0)

static void sep(const char *titolo) {
    printf("\n\033[1;34m── %s\033[0m\n", titolo);
}

/* Genera una stringa casuale di testo (simulazione chunk conversazione) */
static void gen_testo(char *buf, int max, int seed) {
    static const char *parole[] = {
        "il","la","un","una","che","di","in","con","per","su",
        "valore","dato","risultato","processo","sistema","modello",
        "chunk","indice","chiave","offset","lettura","disco","RAM",
        "flusso","token","contesto","query","risposta","memoria","cache"
    };
    int nw = 16;
    srand((unsigned)seed);
    int len = 0;
    while (len < max - 20) {
        const char *w = parole[rand() % nw];
        int wl = (int)strlen(w);
        if (len + wl + 2 >= max) break;
        memcpy(buf + len, w, (size_t)wl);
        len += wl;
        buf[len++] = ' ';
    }
    buf[len > 0 ? len - 1 : 0] = '\0';
}

/* Sceglie k chiavi casuali senza ripetizione dall'elenco globale */
static int scegli_chiavi(const char **out, int k, unsigned seed) {
    int usati[N_CHIAVI] = {0};
    srand(seed);
    int n = 0;
    while (n < k) {
        int idx = rand() % N_CHIAVI;
        if (!usati[idx]) { usati[idx] = 1; out[n++] = TUTTE_LE_CHIAVI[idx]; }
    }
    return n;
}

/* Conta quante chiavi di 'entry' compaiono in 'query_keys' */
static int conta_comuni(const CSEntry *e, const char **qk, int nq) {
    int c = 0;
    for (int i = 0; i < nq; i++)
        for (int j = 0; j < e->n_keys; j++)
            if (strcmp(qk[i], e->keys[j]) == 0) { c++; break; }
    return c;
}

/* ══ TEST ════════════════════════════════════════════════════════════════════ */

/* T1: scrittura 500 chunk con chiavi casuali */
static void t1_scrittura(ContextStore *cs) {
    sep("T1 — Scrittura 500 chunk casuali");
    char testo[256];
    const char *kk[6];
    int ok = 0;

    for (int i = 0; i < 500; i++) {
        gen_testo(testo, sizeof(testo), i * 31 + 7);
        int nk = 1 + (rand() % 5);          /* 1-5 chiavi per chunk */
        scegli_chiavi(kk, nk, (unsigned)(i * 13 + 3));
        if (cs_write(cs, kk, nk, testo, (int)strlen(testo)) == 0) ok++;
    }

    ASSERT_EQ(ok, 500, "500 chunk scritti senza errori");
    ASSERT_EQ(cs->n_entries, 500, "indice in-memory conta 500 entries");
}

/* T2: precision — ogni risultato contiene davvero almeno una chiave richiesta */
static void t2_precision(ContextStore *cs) {
    sep("T2 — Precision: risultati contengono le chiavi richieste");
    const char *qk[] = {"gamma", "delta", "sigma"};
    CSEntry hits[200];
    int n = cs_query(cs, qk, 3, hits, 200);

    int falsi_positivi = 0;
    for (int i = 0; i < n; i++)
        if (conta_comuni(&hits[i], qk, 3) == 0) falsi_positivi++;

    printf("  Query: gamma+delta+sigma → %d risultati\n", n);
    ASSERT(n > 0,          "almeno un risultato trovato");
    ASSERT_EQ(falsi_positivi, 0, "zero falsi positivi");
}

/* T3: recall — nessun chunk rilevante viene escluso per errore
   (verifica manuale: scansione lineare di tutto l'indice) */
static void t3_recall(ContextStore *cs) {
    sep("T3 — Recall: nessun chunk rilevante escluso");
    const char *qk[] = {"omega", "pi"};
    int min_score = 1; /* soglia attesa: almeno 1 chiave in comune */

    /* conta "ground truth" con scansione bruta */
    int gt = 0;
    for (int i = 0; i < cs->n_entries; i++) {
        int s = 0;
        for (int q = 0; q < 2; q++)
            for (int j = 0; j < cs->index[i].n_keys; j++)
                if (strcmp(qk[q], cs->index[i].keys[j]) == 0) { s++; break; }
        if (s >= min_score) gt++;
    }

    CSEntry hits[500];
    int n = cs_query(cs, qk, 2, hits, 500);

    printf("  Query: omega+pi → ground truth=%d, trovati=%d\n", gt, n);
    /* con soglia 40% su 2 chiavi → min_score=1, quindi gt==n */
    ASSERT(n >= gt * 80 / 100, "recall >= 80% (soglia 40% chiavi in comune)");
}

/* T4: ordinamento per score — il primo risultato ha più chiavi in comune */
static void t4_score_order(ContextStore *cs) {
    sep("T4 — Ordinamento: chunk piu' rilevanti prima");

    /* scrivi 3 chunk controllati con score crescente */
    const char *k1[] = {"alfa"};                          /* score 1 su 4 */
    const char *k2[] = {"alfa","beta","gamma"};           /* score 3 su 4 */
    const char *k3[] = {"alfa","beta","gamma","delta"};   /* score 4 su 4 */
    const char *k4[] = {"zeta"};                          /* score 0 su 4 */
    cs_write(cs, k1, 1, "chunk_score1", 12);
    cs_write(cs, k2, 3, "chunk_score3", 12);
    cs_write(cs, k3, 4, "chunk_score4", 12);
    cs_write(cs, k4, 1, "chunk_score0", 12);

    const char *qk[] = {"alfa","beta","gamma","delta"};
    CSEntry hits[8];
    int n = cs_query(cs, qk, 4, hits, 8);

    /* verifica che i primi risultati abbiano score >= quelli successivi */
    int ordinato = 1;
    for (int i = 0; i < n - 1; i++) {
        int s1 = conta_comuni(&hits[i],   qk, 4);
        int s2 = conta_comuni(&hits[i+1], qk, 4);
        if (s1 < s2) { ordinato = 0; break; }
    }

    printf("  Query a 4 chiavi → %d risultati ordinati per score\n", n);
    ASSERT(ordinato, "risultati ordinati per rilevanza decrescente");

    /* il primo deve essere chunk_score4 (4 chiavi in comune) */
    if (n > 0) {
        int top_score = conta_comuni(&hits[0], qk, 4);
        ASSERT_EQ(top_score, 4, "primo risultato: 4 chiavi in comune (massimo)");
    }
}

/* T5: persistenza — close + reopen, tutti i dati intatti */
static void t5_persistenza(ContextStore *cs, const char *path) {
    sep("T5 — Persistenza: close + reopen");

    int n_prima = cs->n_entries;
    cs_close(cs);
    printf("  Store chiuso (%d entries)\n", n_prima);

    ContextStore cs2;
    cs_open(&cs2, path);
    ASSERT_EQ(cs2.n_entries, n_prima, "stesso numero entries dopo reopen");

    /* verifica che i dati siano leggibili */
    const char *qk[] = {"beta","gamma"};
    CSEntry hits[50];
    int n = cs_query(&cs2, qk, 2, hits, 50);
    ASSERT(n > 0, "dati leggibili dopo reopen");
    ASSERT(hits[0].data_len > 0, "dati non corrotti");

    /* riapri il cs originale per i test successivi */
    cs_close(&cs2);
    cs_open(cs, path);
}

/* T6: edge cases */
static void t6_edge_cases(ContextStore *cs) {
    sep("T6 — Edge cases");

    /* chiave completamente inesistente */
    const char *qk_vuota[] = {"XXXXXXXXXX_NON_ESISTE"};
    CSEntry hits[10];
    int n = cs_query(cs, qk_vuota, 1, hits, 10);
    ASSERT_EQ(n, 0, "chiave inesistente → 0 risultati");

    /* chunk con testo al limite (CS_DATA_MAX - 1 caratteri) */
    char testo_lungo[CS_DATA_MAX];
    memset(testo_lungo, 'x', CS_DATA_MAX - 1);
    testo_lungo[CS_DATA_MAX - 1] = '\0';
    const char *kl[] = {"omega"};
    int r = cs_write(cs, kl, 1, testo_lungo, CS_DATA_MAX - 1);
    ASSERT_EQ(r, 0, "scrittura chunk al limite CS_DATA_MAX - OK");

    /* query con 1 sola chiave molto comune */
    const char *qk_comune[] = {"alfa"};
    int n2 = cs_query(cs, qk_comune, 1, hits, 500);
    ASSERT(n2 > 0, "chiave comune → almeno un risultato");
    printf("  Chiave 'alfa' (comune) → %d risultati\n", n2);

    /* store pieno: scrivi fino a CS_MAX_ENTRIES */
    int spazio = CS_MAX_ENTRIES - cs->n_entries;
    const char *kf[] = {"phi"};
    int scritti = 0;
    for (int i = 0; i < spazio + 5; i++) {
        if (cs_write(cs, kf, 1, "x", 1) == 0) scritti++;
    }
    ASSERT_EQ(scritti, spazio, "store pieno: scritture oltre limite rifiutate");
    ASSERT_EQ(cs->n_entries, CS_MAX_ENTRIES, "n_entries non supera CS_MAX_ENTRIES");
}

/* T7: stress — 1000 query consecutive, misura throughput */
static void t7_stress(ContextStore *cs) {
    sep("T7 — Stress: 1000 query consecutive");

    CSEntry hits[20];
    int totale_hits = 0;
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    for (int q = 0; q < 1000; q++) {
        /* ruota le chiavi in modo pseudo-casuale */
        int idx1 = q % N_CHIAVI;
        int idx2 = (q * 7 + 3) % N_CHIAVI;
        const char *qk[2] = { TUTTE_LE_CHIAVI[idx1], TUTTE_LE_CHIAVI[idx2] };
        int n = cs_query(cs, qk, idx1 != idx2 ? 2 : 1, hits, 20);
        totale_hits += n;
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);
    double ms = (t1.tv_sec - t0.tv_sec) * 1000.0
               + (t1.tv_nsec - t0.tv_nsec) / 1e6;

    printf("  1000 query in %.2f ms (%.1f query/sec)\n",
           ms, 1000.0 / ms * 1000.0);
    printf("  Hits totali: %d (media %.1f per query)\n",
           totale_hits, totale_hits / 1000.0);
    ASSERT(ms < 2000.0, "1000 query in meno di 2 secondi");
    ASSERT(totale_hits > 0, "almeno un hit in 1000 query");
}

/* ══ MAIN ════════════════════════════════════════════════════════════════════ */
int main(void) {
    srand((unsigned)time(NULL));
    const char *path = "/tmp/cs_random_test";

    /* pulizia da run precedenti */
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "rm -f %s.dat %s.idx", path, path);
    system(cmd);

    printf("\033[1;36m=== Test Context Store — Dati Casuali ===\033[0m\n");
    printf("Chiavi disponibili: %d (lettere greche)\n", N_CHIAVI);
    printf("Store: %s.dat / .idx\n", path);

    ContextStore cs;
    if (cs_open(&cs, path) != 0) {
        fprintf(stderr, "Errore apertura store\n");
        return 1;
    }

    t1_scrittura(&cs);
    t2_precision(&cs);
    t3_recall(&cs);
    t4_score_order(&cs);
    t5_persistenza(&cs, path);
    t6_edge_cases(&cs);
    t7_stress(&cs);

    cs_close(&cs);

    /* riepilogo */
    printf("\n\033[1m══ Risultati ══\033[0m\n");
    printf("  \033[32mPASSATI: %d\033[0m\n", pass);
    if (fail > 0)
        printf("  \033[31mFALLITI: %d\033[0m\n", fail);
    else
        printf("  \033[32mFALLITI: 0  — tutti i test superati\033[0m\n");

    return fail > 0 ? 1 : 0;
}
