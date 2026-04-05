/*
 * sim_dp.c — Programmazione Dinamica (5)
 * ========================================
 * Zaino 0/1, LCS, Coin Change, Edit Distance, LIS
 */
#include "sim_common.h"

/* ── Zaino 0/1 (DP tabella) ─────────────────────────────────────────── */
int sim_zaino(void) {
    int W = 6;
    int wt[] = {1,2,3,5};
    int vl[] = {1,6,10,16};
    int n = 4;
    sim_log_reset();
    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x8e\x92  Zaino 0/1 — Programmazione Dinamica\n" RST);
    printf(GRY "  Capacit\xc3\xa0 W=%d   Oggetti: ", W);
    for (int i=0;i<n;i++) printf("(w=%d,v=%d) ",wt[i],vl[i]);
    printf("\n  dp[i][w] = valore massimo con i oggetti e peso \xe2\x89\xa4 w\n\n" RST);
    printf(GRY "  [INVIO per iniziare] " RST); fflush(stdout);
    if (sim_aspetta_passo()) return 0;
    int dp[5][7] = {{0}};
    char msg[128];
    for (int i=1;i<=n;i++) {
        for (int w=0;w<=W;w++) {
            if (wt[i-1] <= w)
                dp[i][w] = (dp[i-1][w] > dp[i-1][w-wt[i-1]]+vl[i-1])
                             ? dp[i-1][w] : dp[i-1][w-wt[i-1]]+vl[i-1];
            else
                dp[i][w] = dp[i-1][w];
            snprintf(msg, sizeof msg, "dp[%d][%d]=%-3d oggetto=%d w=%d v=%d",
                     i,w,dp[i][w],i,wt[i-1],vl[i-1]);
            sim_log_push(msg);
        }
        clear_screen(); print_header();
        printf(CYN BLD "\n  \xf0\x9f\x8e\x92  Zaino 0/1 — oggetto %d/%d (w=%d,v=%d)\n\n" RST,i,n,wt[i-1],vl[i-1]);
        printf("       ");
        for(int w=0;w<=W;w++) printf("w=%-3d",w);
        printf("\n");
        for (int ii=0;ii<=i;ii++){
            printf("  i=%d  ",ii);
            for(int w=0;w<=W;w++){
                if(ii==i&&w==W) printf(CYN BLD "%-5d" RST,dp[ii][w]);
                else printf(GRY "%-5d" RST,dp[ii][w]);
            }
            printf("\n");
        }
        printf(GRN "\n  Soluzione attuale: %d\n" RST, dp[i][W]);
        printf(GRY "\n  [INVIO=passo  ESC=esci] " RST); fflush(stdout);
        if (sim_aspetta_passo()) return 0;
    }
    printf(GRN BLD "\n  Valore massimo = %d\n\n" RST, dp[n][W]);
    printf(GRY "  [INVIO] " RST); fflush(stdout); sim_aspetta_passo(); return 1;
}

/* ── LCS — Longest Common Subsequence ───────────────────────────────── */
int sim_lcs(void) {
    const char *X = "ABCBDAB";
    const char *Y = "BDCAB";
    int m = (int)strlen(X), k = (int)strlen(Y);
    int dp[8][6] = {{0}};
    sim_log_reset();
    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x94\xa4  LCS — Longest Common Subsequence\n" RST);
    printf(GRY "  X = \"%s\"   Y = \"%s\"\n" RST, X, Y);
    printf(GRY "  dp[i][j] = LCS di X[0..i-1] e Y[0..j-1]\n\n" RST);
    printf(GRY "  [INVIO per iniziare] " RST); fflush(stdout);
    if (sim_aspetta_passo()) return 0;
    char msg[128];
    for (int i=1;i<=m;i++) {
        for (int j=1;j<=k;j++) {
            if (X[i-1]==Y[j-1]) dp[i][j]=dp[i-1][j-1]+1;
            else dp[i][j]=dp[i-1][j]>dp[i][j-1]?dp[i-1][j]:dp[i][j-1];
        }
        snprintf(msg, sizeof msg, "Riga i=%d ('%c'): dp[%d][%d]=%d", i, X[i-1], i, k, dp[i][k]);
        sim_log_push(msg);
        clear_screen(); print_header();
        printf(CYN BLD "\n  \xf0\x9f\x94\xa4  LCS — riga %d/%d ('%c')\n\n" RST, i, m, X[i-1]);
        printf("       ");
        for(int j=0;j<=k;j++) printf(j==0?"  ":GRY "%-3c" RST, j==0?' ':Y[j-1]);
        printf("\n");
        for (int ii=0;ii<=i;ii++){
            printf(ii==0?GRY "   " RST:GRY "  %c" RST, ii==0?' ':X[ii-1]);
            for(int j=0;j<=k;j++) printf(ii==i&&j==k?CYN BLD "%-3d" RST:GRY "%-3d" RST, dp[ii][j]);
            printf("\n");
        }
        printf(GRN "\n  LCS corrente: %d\n" RST, dp[i][k]);
        printf(GRY "\n  [INVIO=passo  ESC=esci] " RST); fflush(stdout);
        if (sim_aspetta_passo()) return 0;
    }
    return 1;
}

/* ── Coin Change ─────────────────────────────────────────────────────── */
int sim_coin_change(void) {
    int monete[] = {1, 5, 10, 25};
    int nm = 4, target = 30;
    sim_log_reset();
    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x92\xb0  Coin Change — target=%d, monete={1,5,10,25}\n" RST, target);
    printf(GRY "  dp[i] = minimo numero di monete per formare i\n\n" RST);
    printf(GRY "  [INVIO per iniziare] " RST); fflush(stdout);
    if (sim_aspetta_passo()) return 0;

    int dp[31]; for (int i = 0; i <= target; i++) dp[i] = 9999;
    dp[0] = 0;
    char msg[128];

    for (int i = 1; i <= target; i++) {
        for (int j = 0; j < nm; j++) {
            if (monete[j] <= i && dp[i - monete[j]] + 1 < dp[i]) {
                dp[i] = dp[i - monete[j]] + 1;
            }
        }
        if (i % 5 == 0 || i == target) {
            snprintf(msg, sizeof msg, "dp[%2d] = %d", i, dp[i]);
            sim_log_push(msg);
            clear_screen(); print_header();
            printf(CYN BLD "\n  \xf0\x9f\x92\xb0  Coin Change — riempimento dp\n\n" RST);
            printf("  Importo: ");
            for (int k = 0; k <= target; k += 5) printf("%3d ", k);
            printf("\n  Monete:  ");
            for (int k = 0; k <= target; k += 5) {
                if (dp[k] >= 9999) printf(GRY " \xe2\x88\x9e  " RST);
                else printf(GRN "%3d " RST, dp[k]);
            }
            printf("\n\n");
            for (int l = 0; l < _sim_log_n; l++) printf(GRY "  %s\n" RST, _sim_log[l]);
            printf("\n" GRY "  [INVIO=passo  ESC=esci] " RST); fflush(stdout);
            if (sim_aspetta_passo()) return 0;
        }
    }

    clear_screen(); print_header();
    printf(GRN BLD "\n  \xe2\x9c\x94  Risultato: dp[%d] = %d monete\n" RST, target, dp[target]);
    printf("  Soluzione: 25+5 = 30 (2 monete)\n\n");
    printf(GRY "  [INVIO] " RST); fflush(stdout); sim_aspetta_passo(); return 1;
}

/* ── Edit Distance (Levenshtein) ─────────────────────────────────────── */
int sim_edit_distance(void) {
    const char *s1 = "GATTO";
    const char *s2 = "GRASSO";
    int m = (int)strlen(s1), n2 = (int)strlen(s2);
    sim_log_reset();
    clear_screen(); print_header();
    printf(CYN BLD "\n  \xe2\x9c\x8f  Edit Distance: \"%s\" \xe2\x86\x92 \"%s\"\n" RST, s1, s2);
    printf(GRY "  dp[i][j] = min(insert, delete, replace)\n\n" RST);
    printf(GRY "  [INVIO per iniziare] " RST); fflush(stdout);
    if (sim_aspetta_passo()) return 0;

    int dp[6][7];
    for (int i = 0; i <= m; i++) dp[i][0] = i;
    for (int j = 0; j <= n2; j++) dp[0][j] = j;

    char msg[128];
    for (int i = 1; i <= m; i++) {
        for (int j = 1; j <= n2; j++) {
            if (s1[i-1] == s2[j-1]) dp[i][j] = dp[i-1][j-1];
            else dp[i][j] = 1 + (dp[i-1][j] < dp[i][j-1] ? (dp[i-1][j] < dp[i-1][j-1] ? dp[i-1][j] : dp[i-1][j-1]) : (dp[i][j-1] < dp[i-1][j-1] ? dp[i][j-1] : dp[i-1][j-1]));
        }
        snprintf(msg, sizeof msg, "Riga %d ('%c'): calcolata", i, s1[i-1]);
        sim_log_push(msg);
        clear_screen(); print_header();
        printf(CYN BLD "\n  \xe2\x9c\x8f  Edit Distance \"%s\" \xe2\x86\x92 \"%s\"\n\n" RST, s1, s2);
        printf("      ");
        printf("   \"\"");
        for (int j2 = 0; j2 < n2; j2++) printf("  %c ", s2[j2]);
        printf("\n");
        for (int i2 = 0; i2 <= i; i2++) {
            if (i2 == 0) printf("  \"\" "); else printf("   %c  ", s1[i2-1]);
            for (int j2 = 0; j2 <= n2; j2++) {
                if (i2 == i && j2 == n2) printf(GRN "%3d " RST, dp[i2][j2]);
                else printf("%3d ", dp[i2][j2]);
            }
            printf("\n");
        }
        printf("\n");
        for (int l = 0; l < _sim_log_n; l++) printf(GRY "  %s\n" RST, _sim_log[l]);
        printf("\n" GRY "  [INVIO=passo  ESC=esci] " RST); fflush(stdout);
        if (sim_aspetta_passo()) return 0;
    }
    clear_screen(); print_header();
    printf(GRN BLD "\n  \xe2\x9c\x94  Edit Distance(\"%s\",\"%s\") = %d\n\n" RST, s1, s2, dp[m][n2]);
    printf("  Operazioni: inserisci R, inserisci S, aggiungi O\n\n");
    printf(GRY "  [INVIO] " RST); fflush(stdout); sim_aspetta_passo(); return 1;
}

/* ── LIS (Longest Increasing Subsequence) ────────────────────────────── */
int sim_lis(void) {
    int arr[] = {3, 10, 2, 1, 20, 4, 15, 9};
    int n = 8;
    sim_log_reset();
    char msg[128];

    sim_mostra_array(arr, n,-1,-1,-1,-1,-1,0,
        "LIS: Longest Increasing Subsequence — O(n^2) con DP",
        "\xf0\x9f\x93\x88 LIS");
    if (sim_aspetta_passo()) return 0;

    int dp[8], parent[8];
    for (int i = 0; i < n; i++) { dp[i] = 1; parent[i] = -1; }

    for (int i = 1; i < n; i++) {
        for (int j = 0; j < i; j++) {
            if (arr[j] < arr[i] && dp[j] + 1 > dp[i]) {
                dp[i] = dp[j] + 1;
                parent[i] = j;
            }
        }
        snprintf(msg, sizeof msg, "dp[%d]=%d (%d: LIS che termina qui ha lungh. %d)", i, dp[i], arr[i], dp[i]);
        sim_log_push(msg);
        sim_mostra_array(dp, n, i,-1,-1,-1,-1,0, msg, "\xf0\x9f\x93\x88 LIS — tabella dp");
        if (sim_aspetta_passo()) return 0;
    }

    int best = 0;
    for (int i = 1; i < n; i++) if (dp[i] > dp[best]) best = i;

    int lis_seq[8], lis_len = 0, cur = best;
    while (cur != -1) { lis_seq[lis_len++] = arr[cur]; cur = parent[cur]; }

    clear_screen(); print_header();
    printf(GRN BLD "\n  \xe2\x9c\x94  LIS di lunghezza %d: ", dp[best]);
    for (int i = lis_len-1; i >= 0; i--) printf("%d ", lis_seq[i]);
    printf("\n\n" RST);
    printf(GRY "  [INVIO] " RST); fflush(stdout); sim_aspetta_passo(); return 1;
}
