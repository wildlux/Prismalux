/*
 * sim_stringhe.c — Algoritmi su Stringhe (3)
 * =============================================
 * KMP Search, Rabin-Karp, Z-Algorithm
 */
#include "sim_common.h"

/* ── KMP Search ──────────────────────────────────────────────────────── */
int sim_kmp_search(void) {
    const char *testo = "ABABCABABCABAB";
    const char *pattern = "ABABCABAB";
    int n = (int)strlen(testo), m = (int)strlen(pattern);
    sim_log_reset();
    char msg[128];

    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x94\x8d  KMP Search — Knuth-Morris-Pratt\n" RST);
    printf(GRY "  Testo:   \"%s\"\n" RST, testo);
    printf(GRY "  Pattern: \"%s\"\n" RST, pattern);
    printf(GRY "  O(n+m) — usa failure function per evitare backtrack\n\n" RST);
    printf(GRY "  [INVIO per iniziare] " RST); fflush(stdout);
    if (sim_aspetta_passo()) return 0;

    int fail[32] = {0};
    fail[0] = 0;
    for (int k = 0, i2 = 1; i2 < m; i2++) {
        while (k > 0 && pattern[k] != pattern[i2]) k = fail[k-1];
        if (pattern[k] == pattern[i2]) k++;
        fail[i2] = k;
    }

    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x94\x8d  KMP — Failure Function\n\n" RST);
    printf("  Pattern: ");
    for (int i2 = 0; i2 < m; i2++) printf("  %c", pattern[i2]);
    printf("\n  fail[]:  ");
    for (int i2 = 0; i2 < m; i2++) printf("  %d", fail[i2]);
    printf("\n\n" GRY "  [INVIO=cerca  ESC=esci] " RST); fflush(stdout);
    if (sim_aspetta_passo()) return 0;

    int trovati = 0, q = 0;
    for (int i2 = 0; i2 < n; i2++) {
        while (q > 0 && pattern[q] != testo[i2]) q = fail[q-1];
        if (pattern[q] == testo[i2]) q++;
        if (q == m) {
            trovati++;
            snprintf(msg, sizeof msg, "Pattern trovato a posizione %d!", i2-m+1);
            sim_log_push(msg);
            q = fail[q-1];
        }
        if (i2 % 3 == 0 || q == m) {
            clear_screen(); print_header();
            printf(CYN BLD "\n  \xf0\x9f\x94\x8d  KMP — pos testo=%d  match=%d\n\n" RST, i2, q);
            printf("  Testo:   ");
            for (int k = 0; k < n; k++) {
                if (k == i2) printf(YLW "%c" RST, testo[k]);
                else printf("%c", testo[k]);
            }
            printf("\n  Pattern: ");
            for (int k = 0; k < q; k++) printf(GRN "%c" RST, pattern[k]);
            printf("\n\n");
            for (int l = 0; l < _sim_log_n; l++) printf(GRY "  %s\n" RST, _sim_log[l]);
            printf("\n" GRY "  [INVIO=passo  ESC=esci] " RST); fflush(stdout);
            if (sim_aspetta_passo()) return 0;
        }
    }
    clear_screen(); print_header();
    printf(GRN BLD "\n  \xe2\x9c\x94  KMP: trovate %d occorrenze\n\n" RST, trovati);
    printf(GRY "  [INVIO] " RST); fflush(stdout); sim_aspetta_passo(); return 1;
}

/* ── Rabin-Karp ──────────────────────────────────────────────────────── */
int sim_rabin_karp(void) {
    const char *testo = "ABCADABCAB";
    const char *pattern = "ABCAB";
    int n = (int)strlen(testo), m = (int)strlen(pattern);
    int base = 31, mod = 101;
    sim_log_reset();
    char msg[128];

    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x94\x91  Rabin-Karp — rolling hash\n" RST);
    printf(GRY "  Testo:   \"%s\"   Pattern: \"%s\"\n" RST, testo, pattern);
    printf(GRY "  Hash pattern confrontato con hash finestre di testo — O(n+m) medio\n\n" RST);
    printf(GRY "  [INVIO per iniziare] " RST); fflush(stdout);
    if (sim_aspetta_passo()) return 0;

    int hp = 0, ht = 0, h = 1;
    for (int i2 = 0; i2 < m-1; i2++) h = (h * base) % mod;
    for (int i2 = 0; i2 < m; i2++) {
        hp = (hp * base + pattern[i2]) % mod;
        ht = (ht * base + testo[i2]) % mod;
    }

    int trovati = 0;
    for (int i2 = 0; i2 <= n-m; i2++) {
        int match = (hp == ht);
        if (match) {
            for (int j = 0; j < m; j++) if (testo[i2+j] != pattern[j]) { match = 0; break; }
            if (match) { trovati++; snprintf(msg, sizeof msg, "Trovato a pos %d! (hash=%d)", i2, ht); sim_log_push(msg); }
            else { snprintf(msg, sizeof msg, "Falso positivo a pos %d (hash=%d)", i2, ht); sim_log_push(msg); }
        } else {
            snprintf(msg, sizeof msg, "Pos %d: hash_testo=%d != hash_pattern=%d, salta", i2, ht, hp);
            sim_log_push(msg);
        }

        clear_screen(); print_header();
        printf(CYN BLD "\n  \xf0\x9f\x94\x91  Rabin-Karp — finestra pos %d\n\n" RST, i2);
        printf("  Testo: ");
        for (int k = 0; k < n; k++) {
            if (k >= i2 && k < i2+m) printf(match ? GRN "%c" RST : YLW "%c" RST, testo[k]);
            else printf("%c", testo[k]);
        }
        printf("\n  hash_pattern=%d  hash_finestra=%d\n\n", hp, ht);
        for (int l = 0; l < _sim_log_n; l++) printf(GRY "  %s\n" RST, _sim_log[l]);
        printf("\n" GRY "  [INVIO=passo  ESC=esci] " RST); fflush(stdout);
        if (sim_aspetta_passo()) return 0;

        if (i2 < n-m) {
            ht = (base * (ht - testo[i2] * h) + testo[i2+m]) % mod;
            if (ht < 0) ht += mod;
        }
    }
    clear_screen(); print_header();
    printf(GRN BLD "\n  \xe2\x9c\x94  Rabin-Karp: trovate %d occorrenze\n\n" RST, trovati);
    printf(GRY "  [INVIO] " RST); fflush(stdout); sim_aspetta_passo(); return 1;
}

/* ── Z-Algorithm ─────────────────────────────────────────────────────── */
int sim_z_algorithm(void) {
    const char *pattern = "AAB";
    const char *testo = "AABXAABXAAB";
    char s[64];
    snprintf(s, sizeof s, "%s$%s", pattern, testo);
    int slen = (int)strlen(s);
    int pm = (int)strlen(pattern);
    int Z[64] = {0};
    sim_log_reset();
    char msg[128];

    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x94\xa4  Z-Algorithm — pattern in testo\n" RST);
    printf(GRY "  Stringa: \"%s\"\n" RST, s);
    printf(GRY "  Z[i] = lunghezza del prefisso pi\xc3\xb9 lungo che inizia in i\n\n" RST);
    printf(GRY "  [INVIO per iniziare] " RST); fflush(stdout);
    if (sim_aspetta_passo()) return 0;

    int l = 0, r = 0;
    for (int i2 = 1; i2 < slen; i2++) {
        if (i2 < r) Z[i2] = (Z[i2 - l] < r - i2) ? Z[i2 - l] : r - i2;
        while (i2 + Z[i2] < slen && s[Z[i2]] == s[i2 + Z[i2]]) Z[i2]++;
        if (i2 + Z[i2] > r) { l = i2; r = i2 + Z[i2]; }

        snprintf(msg, sizeof msg, "Z[%d]=%d (s[%d..%d] matcha prefisso)", i2, Z[i2], i2, i2+Z[i2]-1);
        sim_log_push(msg);
        if (Z[i2] == pm) {
            snprintf(msg, sizeof msg, "  -> Pattern trovato a pos %d nel testo!", i2 - pm - 1);
            sim_log_push(msg);
        }

        clear_screen(); print_header();
        printf(CYN BLD "\n  \xf0\x9f\x94\xa4  Z-Algorithm — i=%d\n\n" RST, i2);
        printf("  Stringa: %s\n  Z[]:     ", s);
        for (int k = 0; k < i2+1; k++) printf("%d", Z[k]);
        printf("\n\n");
        for (int l2 = 0; l2 < _sim_log_n; l2++) printf(GRY "  %s\n" RST, _sim_log[l2]);
        printf("\n" GRY "  [INVIO=passo  ESC=esci] " RST); fflush(stdout);
        if (sim_aspetta_passo()) return 0;
    }
    clear_screen(); print_header();
    printf(GRN BLD "\n  \xe2\x9c\x94  Z-Algorithm completato\n\n" RST);
    printf(GRY "  [INVIO] " RST); fflush(stdout); sim_aspetta_passo(); return 1;
}
