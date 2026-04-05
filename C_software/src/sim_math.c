/*
 * sim_math.c — Algoritmi Matematici (10)
 * ======================================
 * GCD Euclide, Crivo Eratostene, Potenza Modulare, Triangolo Pascal,
 * Monte Carlo Pi, Fibonacci DP, Torre di Hanoi, Fibonacci Ricorsivo,
 * Miller-Rabin, Schema di Horner/Ruffini
 */
#include "sim_common.h"

/* ── GCD Euclide ─────────────────────────────────────────────────────── */
int sim_gcd_euclide(void) {
    srand((unsigned)time(NULL));
    int a = 24 + rand() % 120;
    int b = 12 + rand() % 80;
    sim_log_reset();

    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x94\xa2  GCD — Algoritmo di Euclide\n" RST);
    printf(GRY "  MCD(%d, %d) = ?\n\n" RST, a, b);
    printf("  Idea: MCD(a,b) = MCD(b, a mod b)\n");
    printf("  Quando b=0, MCD = a\n\n");
    printf(GRY "  [INVIO per iniziare] " RST); fflush(stdout);
    if (sim_aspetta_passo()) return 0;

    int passo = 0;
    int orig_a = a, orig_b = b;
    char msg[128];
    while (b != 0) {
        passo++;
        int r = a % b;
        snprintf(msg, sizeof msg, "Passo %d: MCD(%d, %d) = MCD(%d, %d mod %d) = MCD(%d, %d)",
                 passo, a, b, b, a, b, b, r);
        sim_log_push(msg);
        clear_screen(); print_header();
        printf(CYN BLD "\n  \xf0\x9f\x94\xa2  GCD — Algoritmo di Euclide\n\n" RST);
        for (int i = 0; i < _sim_log_n; i++) {
            if (i < _sim_log_n - 1) printf(GRY "  %s\n" RST, _sim_log[i]);
            else printf(CYN BLD "  %s\n" RST, _sim_log[i]);
        }
        printf("\n" GRY "  [INVIO=continua  ESC=esci] " RST); fflush(stdout);
        if (sim_aspetta_passo()) return 0;
        a = b; b = r;
    }
    clear_screen(); print_header();
    printf(GRN BLD "\n  \xe2\x9c\x94  MCD(%d, %d) = %d\n\n" RST, orig_a, orig_b, a);
    for (int i = 0; i < _sim_log_n; i++) printf(GRY "  %s\n" RST, _sim_log[i]);
    printf("\n  Verifica: %d/%d = %d   %d/%d = %d\n",
           orig_a, a, orig_a/a, orig_b, a, orig_b/a);
    printf("\n" GRY "  [INVIO per continuare] " RST); fflush(stdout);
    sim_aspetta_passo();
    return 1;
}

/* ── Crivo di Eratostene ─────────────────────────────────────────────── */
int sim_crivo(void) {
    int N = 50;
    int sieve[51];
    for (int i = 0; i <= N; i++) sieve[i] = 0;
    sieve[0] = sieve[1] = 1;
    sim_log_reset();

    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\xa7\xae  Crivo di Eratostene — primi fino a %d\n" RST, N);
    printf(GRY "  Per ogni primo p: segna tutti i multipli di p come composti\n\n" RST);
    printf(GRY "  [INVIO per iniziare] " RST); fflush(stdout);
    if (sim_aspetta_passo()) return 0;

    #define STAMPA_CRIVO(titolo, p_cur) do { \
        clear_screen(); print_header(); \
        printf(CYN BLD "\n  \xf0\x9f\xa7\xae  %s\n\n" RST, (titolo)); \
        for (int _r = 0; _r <= N; _r += 10) { \
            printf("  "); \
            for (int _c = _r; _c <= _r+9 && _c <= N; _c++) { \
                if (_c == (p_cur)) printf(YLW "[%2d]" RST, _c); \
                else if (sieve[_c]) printf(GRY " \xe2\x80\xa2\xe2\x80\xa2 " RST); \
                else printf(GRN "[%2d]" RST, _c); \
            } \
            printf("\n"); \
        } \
        printf("\n" GRY "  Verde=primo  Grigio=composto  Giallo=p corrente\n" RST); \
        for (int _i = 0; _i < _sim_log_n; _i++) { \
            if (_i < _sim_log_n-1) printf(GRY "  %s\n" RST, _sim_log[_i]); \
            else printf(CYN BLD "  %s\n" RST, _sim_log[_i]); \
        } \
        printf("\n" GRY "  [INVIO=passo  ESC=esci] " RST); fflush(stdout); \
    } while(0)

    char msg[128];
    for (int p = 2; (long long)p * p <= N; p++) {
        if (sieve[p]) continue;
        snprintf(msg, sizeof msg, "p=%d primo: segno multipli %d,%d,%d...",
                 p, p*2, p*3, p*4);
        sim_log_push(msg);
        STAMPA_CRIVO("Crivo di Eratostene", p);
        if (sim_aspetta_passo()) return 0;

        for (int m = p * p; m <= N; m += p) {
            sieve[m] = 1;
        }
    }
    STAMPA_CRIVO("Crivo di Eratostene — COMPLETATO", -1);
    printf(GRN "\n  Numeri primi trovati: " RST);
    for (int i = 2; i <= N; i++) if (!sieve[i]) printf("%d ", i);
    printf("\n\n" GRY "  [INVIO per continuare] " RST); fflush(stdout);
    sim_aspetta_passo();
    #undef STAMPA_CRIVO
    return 1;
}

/* ── Potenza Modulare ────────────────────────────────────────────────── */
int sim_potenza_modulare(void) {
    srand((unsigned)time(NULL));
    int base = 2 + rand() % 10;
    int exp  = 5 + rand() % 10;
    int mod  = 97 + rand() % 20;
    sim_log_reset();
    clear_screen(); print_header();
    printf(CYN BLD "\n  \xe2\x9a\xa1  Potenza Modulare: %d^%d mod %d\n" RST, base, exp, mod);
    printf(GRY "  Algoritmo: quadrato e moltiplica — O(log n)\n\n" RST);
    printf(GRY "  [INVIO per iniziare] " RST); fflush(stdout);
    if (sim_aspetta_passo()) return 0;
    long long result = 1, b = base % mod, e = exp;
    char msg[128];
    int passo = 0;
    while (e > 0) {
        passo++;
        if (e & 1) {
            snprintf(msg, sizeof msg, "Passo %d: e=%lld dispari \xe2\x86\x92 result = result*b = %lld*%lld mod %d = %lld",
                     passo, e, result, b, mod, (result*b)%mod);
            result = (result * b) % mod;
        } else {
            snprintf(msg, sizeof msg, "Passo %d: e=%lld pari", passo, e);
        }
        b = (b * b) % mod;
        e >>= 1;
        sim_log_push(msg);
        clear_screen(); print_header();
        printf(CYN BLD "\n  \xe2\x9a\xa1  %d^%d mod %d\n\n" RST, base, exp, mod);
        for (int i=0;i<_sim_log_n;i++) {
            if(i<_sim_log_n-1) printf(GRY "  %s\n" RST, _sim_log[i]);
            else printf(CYN BLD "  %s\n" RST, _sim_log[i]);
        }
        printf("\n" GRY "  [INVIO=continua  ESC=esci] " RST); fflush(stdout);
        if (sim_aspetta_passo()) return 0;
    }
    clear_screen(); print_header();
    printf(GRN BLD "\n  \xe2\x9c\x94  %d^%d mod %d = %lld\n\n" RST, base, exp, mod, result);
    printf(GRY "  [INVIO] " RST); fflush(stdout); sim_aspetta_passo(); return 1;
}

/* ── Triangolo di Pascal ─────────────────────────────────────────────── */
int sim_triangolo_pascal(void) {
    sim_log_reset();
    int tri[10][10] = {{0}};
    for (int i=0;i<10;i++){tri[i][0]=1;for(int j=1;j<=i;j++) tri[i][j]=tri[i-1][j-1]+tri[i-1][j];}
    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x94\xba  Triangolo di Pascal\n" RST);
    printf(GRY "  Ogni cella = somma dei due sopra. C(n,k) = n!/(k!(n-k)!)\n\n" RST);
    printf(GRY "  [INVIO per procedere riga per riga] " RST); fflush(stdout);
    if (sim_aspetta_passo()) return 0;
    for (int r=0;r<10;r++) {
        clear_screen(); print_header();
        printf(CYN BLD "\n  \xf0\x9f\x94\xba  Triangolo di Pascal — riga %d\n\n" RST, r);
        for (int i=0;i<=r;i++) {
            printf("  ");
            for (int sp=0;sp<(9-i)*2;sp++) printf(" ");
            for (int j=0;j<=i;j++) {
                if(j==i) printf(CYN BLD "%4d" RST, tri[i][j]);
                else printf(YLW "%4d" RST, tri[i][j]);
            }
            printf("\n");
        }
        printf("\n" GRY "  [INVIO=prossima riga  ESC=esci] " RST); fflush(stdout);
        if (sim_aspetta_passo()) return 0;
    }
    return 1;
}

/* ── Monte Carlo Pi ─────────────────────────────────────────────────── */
int sim_monte_carlo_pi(void) {
    sim_log_reset();
    srand((unsigned)time(NULL));
    int dentro=0, totale=0;
    char msg[128];
    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x8e\xb2  Monte Carlo \xe2\x80\x94 Stima di \xcf\x80\n" RST);
    printf(GRY "  Punti casuali in [0,1)x[0,1): dentro cerchio = x\xc2\xb2+y\xc2\xb2 < 1\n" RST);
    printf(GRY "  \xcf\x80 \xe2\x89\x88 4 * (dentro/totale)\n\n" RST);
    printf(GRY "  [INVIO per iniziare] " RST); fflush(stdout);
    if (sim_aspetta_passo()) return 0;
    int steps[] = {10,50,100,500,1000,5000,10000,50000,100000,0};
    for (int s=0; steps[s]; s++) {
        while (totale < steps[s]) {
            double x = (double)rand()/RAND_MAX;
            double y = (double)rand()/RAND_MAX;
            if (x*x+y*y < 1.0) dentro++;
            totale++;
        }
        double pi_est = 4.0*dentro/totale;
        snprintf(msg, sizeof msg, "n=%6d  dentro=%6d  \xcf\x80\xe2\x89\x88%.6f  errore=%.6f",
                 totale, dentro, pi_est, pi_est - SIM_PI);
        sim_log_push(msg);
        clear_screen(); print_header();
        printf(CYN BLD "\n  \xf0\x9f\x8e\xb2  Monte Carlo \xe2\x80\x94 Stima di \xcf\x80\n\n" RST);
        printf(GRY "  Pi reale: " RST YLW "%.10f\n\n" RST, SIM_PI);
        for (int i=0;i<_sim_log_n;i++) {
            double pi_v = 3.0; sscanf(_sim_log[i]+40, "%lf", &pi_v);
            const char *col = fabs(pi_v-SIM_PI)<0.01 ? GRN : fabs(pi_v-SIM_PI)<0.1 ? YLW : MAG;
            printf("  %s%s" RST "\n", col, _sim_log[i]);
        }
        printf("\n" GRY "  [INVIO=prosegui  ESC=esci] " RST); fflush(stdout);
        if (sim_aspetta_passo()) return 0;
    }
    return 1;
}

/* ── Fibonacci DP con memoization ───────────────────────────────────── */
int sim_fibonacci_dp(void) {
    sim_log_reset();
    long long memo[21];
    for (int i=0;i<21;i++) memo[i]=-1;
    memo[0]=0; memo[1]=1;
    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x90\x87  Fibonacci con Memoization\n" RST);
    printf(GRY "  F(n) = F(n-1) + F(n-2),  F(0)=0, F(1)=1\n" RST);
    printf(GRY "  Salviamo i risultati gi\xc3\xa0 calcolati (tabella memo)\n\n" RST);
    printf(GRY "  [INVIO per procedere] " RST); fflush(stdout);
    if (sim_aspetta_passo()) return 0;
    char msg[128];
    for (int i=2;i<=20;i++) {
        memo[i] = memo[i-1]+memo[i-2];
        snprintf(msg, sizeof msg, "F(%2d) = F(%2d)+F(%2d) = %lld+%lld = %lld",
                 i, i-1, i-2, memo[i-1], memo[i-2], memo[i]);
        sim_log_push(msg);
        clear_screen(); print_header();
        printf(CYN BLD "\n  \xf0\x9f\x90\x87  Fibonacci — n=%d\n\n" RST, i);
        printf(GRY "  Tabella memo:\n  " RST);
        for (int j=0;j<=i;j++) printf(j==i?CYN BLD "[%lld]" RST GRY " " RST:GRY "[%lld] " RST, memo[j]);
        printf("\n\n");
        for (int k=0;k<_sim_log_n;k++) {
            if(k<_sim_log_n-1) printf(GRY "  %s\n" RST, _sim_log[k]);
            else printf(CYN BLD "  %s\n" RST, _sim_log[k]);
        }
        printf("\n" GRY "  [INVIO=passo  ESC=esci] " RST); fflush(stdout);
        if (sim_aspetta_passo()) return 0;
    }
    return 1;
}

/* ── Torre di Hanoi (helper e stato globale) ──────────────────────────── */
static int _hanoi_step;
static int _hanoi_interrotto;
static int _hanoi_n;
static int _paletti[3][8];
static int _paletti_sz[3];

static void _hanoi_stampa(const char *msg) {
    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x97\xbc  Torre di Hanoi (n=%d) — Passo %d\n\n" RST, _hanoi_n, _hanoi_step);
    const char *nomi[] = {"A (sorgente)", "B (ausiliario)", "C (destinazione)"};
    for (int p = 0; p < 3; p++) {
        printf("  %s: ", nomi[p]);
        if (_paletti_sz[p] == 0) { printf(GRY "[vuoto]" RST); }
        else {
            for (int i = 0; i < _paletti_sz[p]; i++) {
                printf(GRN "[%d]" RST " ", _paletti[p][i]);
            }
        }
        printf("\n");
    }
    printf("\n  " YLW "\xe2\x80\xba " RST "%s\n", msg);
    printf("\n" GRY "  [INVIO=passo  ESC=esci] " RST); fflush(stdout);
}

static void _hanoi_move(int from, int to, char msg_buf[], int msz) {
    if (_hanoi_interrotto) return;
    int disco = _paletti[from][--_paletti_sz[from]];
    _paletti[to][_paletti_sz[to]++] = disco;
    _hanoi_step++;
    snprintf(msg_buf, msz, "Sposta disco %d da %c a %c", disco,
             'A'+from, 'A'+to);
    _hanoi_stampa(msg_buf);
    if (sim_aspetta_passo()) _hanoi_interrotto = 1;
}

static void _hanoi(int n, int from, int to, int aux, char msg_buf[], int msz) {
    if (n == 0 || _hanoi_interrotto) return;
    _hanoi(n-1, from, aux, to, msg_buf, msz);
    _hanoi_move(from, to, msg_buf, msz);
    _hanoi(n-1, aux, to, from, msg_buf, msz);
}

int sim_torre_hanoi(void) {
    _hanoi_n = 4;
    _hanoi_step = 0;
    _hanoi_interrotto = 0;
    memset(_paletti, 0, sizeof _paletti);
    memset(_paletti_sz, 0, sizeof _paletti_sz);
    for (int i = _hanoi_n; i >= 1; i--) _paletti[0][_paletti_sz[0]++] = i;
    sim_log_reset();
    char msg[128];
    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x97\xbc  Torre di Hanoi (n=4)\n" RST);
    printf(GRY "  Sposta tutti i dischi da A a C usando B come appoggio.\n");
    printf("  Regola: mai disco grande su disco piccolo. Mosse totali: 2^n - 1 = 15\n\n" RST);
    _hanoi_stampa("Configurazione iniziale");
    if (sim_aspetta_passo()) return 0;
    _hanoi(_hanoi_n, 0, 2, 1, msg, sizeof msg);
    if (_hanoi_interrotto) return 0;
    clear_screen(); print_header();
    printf(GRN BLD "\n  \xe2\x9c\x94  Hanoi completata in %d mosse!\n\n" RST, _hanoi_step);
    printf(GRY "  [INVIO] " RST); fflush(stdout); sim_aspetta_passo(); return 1;
}

/* ── Fibonacci ricorsivo visuale ─────────────────────────────────────── */
int sim_fib_ricorsivo(void) {
    sim_log_reset();
    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x8c\xb3  Fibonacci Ricorsivo — Albero delle chiamate (n=6)\n" RST);
    printf(GRY "  F(n) = F(n-1) + F(n-2)   F(0)=0  F(1)=1\n");
    printf("  Complessit\xc3\xa0: O(2^n) — molte chiamate ridondanti!\n\n" RST);
    printf(GRY "  [INVIO per procedere] " RST); fflush(stdout);
    if (sim_aspetta_passo()) return 0;

    const char *albero[] = {
        "                F(5)",
        "              /      \\",
        "           F(4)       F(3)",
        "          /    \\      /   \\",
        "       F(3)   F(2)  F(2) F(1)",
        "       / \\    / \\   / \\",
        "    F(2) F(1) F(1) F(0) F(1) F(0)",
        "    / \\",
        " F(1) F(0)",
    };
    int livelli = 9;
    for (int i = 0; i < livelli; i++) {
        clear_screen(); print_header();
        printf(CYN BLD "\n  \xf0\x9f\x8c\xb3  Fibonacci Ricorsivo — Albero (n=5)\n\n" RST);
        for (int j = 0; j <= i; j++) printf(GRN "  %s\n" RST, albero[j]);
        printf("\n" GRY "  [INVIO=espandi  ESC=esci] " RST); fflush(stdout);
        if (sim_aspetta_passo()) return 0;
    }

    int fib[7] = {0,1,1,2,3,5,8};
    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x8c\xb3  Fibonacci — Valori\n\n" RST);
    for (int i = 0; i <= 6; i++)
        printf("  F(%d) = %d\n", i, fib[i]);
    printf("\n" GRY "  Totale chiamate per F(5): 15   per F(6): 25\n" RST);
    printf("\n" GRY "  [INVIO] " RST); fflush(stdout); sim_aspetta_passo(); return 1;
}

/* ── Miller-Rabin ────────────────────────────────────────────────────── */
static long long _mr_powmod(long long base, long long exp, long long mod) {
    long long result = 1;
    base %= mod;
    while (exp > 0) {
        if (exp & 1) result = result * base % mod;
        base = base * base % mod;
        exp >>= 1;
    }
    return result;
}

int sim_miller_rabin(void) {
    int n = 97;
    sim_log_reset();
    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\xa7\xae  Miller-Rabin — Test di Primalit\xc3\xa0 per n=%d\n" RST, n);
    printf(GRY "  Scrivi n-1 = 2^r * d, poi testa basi a={2,3,5,7}\n\n" RST);
    printf(GRY "  [INVIO per procedere] " RST); fflush(stdout);
    if (sim_aspetta_passo()) return 0;

    int r = 0, d = n - 1;
    while (d % 2 == 0) { d /= 2; r++; }

    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\xa7\xae  Miller-Rabin — n=%d\n\n" RST, n);
    printf("  n-1 = %d = 2^%d * %d\n\n", n-1, r, d);
    printf(GRY "  [INVIO] " RST); fflush(stdout);
    if (sim_aspetta_passo()) return 0;

    int basi[] = {2, 3, 5, 7};
    int composto = 0;
    char msg[128];
    for (int bi = 0; bi < 4 && !composto; bi++) {
        int a = basi[bi];
        long long x = _mr_powmod(a, d, n);
        snprintf(msg, sizeof msg, "Base a=%d: %d^%d mod %d = %lld", a, a, d, n, x);
        sim_log_push(msg);
        clear_screen(); print_header();
        printf(CYN BLD "\n  \xf0\x9f\xa7\xae  Miller-Rabin — base %d\n\n" RST, a);
        for (int i = 0; i < _sim_log_n; i++) printf(GRY "  %s\n" RST, _sim_log[i]);
        printf("\n" GRY "  [INVIO] " RST); fflush(stdout);
        if (sim_aspetta_passo()) return 0;

        if (x == 1 || x == n-1) continue;
        int continua = 1;
        for (int i2 = 0; i2 < r-1 && continua; i2++) {
            x = x * x % n;
            if (x == n-1) continua = 0;
        }
        if (continua) composto = 1;
    }

    clear_screen(); print_header();
    printf(GRN BLD "\n  \xe2\x9c\x94  n=%d e' %s\n\n" RST, n, composto ? "COMPOSTO" : "PRIMO (con alta probabilita')");
    printf(GRY "  Miller-Rabin con basi {2,3,5,7} e' deterministico per n < 3.2 miliardi\n\n" RST);
    printf(GRY "  [INVIO] " RST); fflush(stdout); sim_aspetta_passo(); return 1;
}

/* ── Schema di Horner / Regola di Ruffini ────────────────────────────── */
/*
 * Valutazione polinomiale naive vs schema di Horner.
 * Polinomio: p(x) = x^3 - 6x^2 + 11x - 6  (radici: 1, 2, 3)
 * Valutazione in x=2, poi divisione sintetica (regola di Ruffini) per (x-2).
 *
 * Naive:  calcola ogni termine x^k separatamente  — O(n^2) moltiplicazioni
 * Horner: p(x) = ((1*x - 6)*x + 11)*x - 6       — O(n)   moltiplicazioni
 */
int sim_horner_ruffini(void) {
    /* coefficienti di p(x) = 1*x^3 - 6*x^2 + 11*x - 6, grado alto->basso */
    int coef[] = {1, -6, 11, -6};
    int grado  = 3;
    int x_val  = 2;   /* punto di valutazione / radice per Ruffini */
    sim_log_reset();

    /* ── Schermata introduttiva ─────────────────────────────────────── */
    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x93\x90  Schema di Horner \xe2\x80\x94 Regola di Ruffini\n" RST);
    printf(GRY "  Polinomio: p(x) = x\xc2\xb3 - 6x\xc2\xb2 + 11x - 6\n" RST);
    printf(GRY "  Radici reali: x=1, x=2, x=3\n\n" RST);
    printf("  Problema: calcolare p(%d) nel modo piu' efficiente\n\n", x_val);
    printf("  " YLW "Metodo Naive" RST ":  calcola ogni x^k separatamente \xe2\x86\x92 O(n\xc2\xb2)\n");
    printf("  " GRN "Metodo Horner" RST ": parentesizzazione annidata      \xe2\x86\x92 O(n)\n\n");
    printf("  " CYN "Ruffini" RST ":      divisione sintetica per (x-%d)  \xe2\x86\x92 stessa idea\n\n", x_val);
    printf(GRY "  [INVIO per iniziare] " RST); fflush(stdout);
    if (sim_aspetta_passo()) return 0;

    /* ── METODO NAIVE ─────────────────────────────────────────────── */
    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x93\x90  Metodo Naive — p(%d)\n\n" RST, x_val);
    printf(GRY "  p(x) = 1*x^3 + (-6)*x^2 + 11*x + (-6)\n\n" RST);

    long long naive_muls = 0;
    long long risultato_naive = 0;
    for (int i = 0; i <= grado; i++) {
        /* calcola x^(grado-i) con moltiplicazioni esplicite */
        long long potenza = 1;
        int esp = grado - i;
        for (int j = 0; j < esp; j++) { potenza *= x_val; naive_muls++; }
        long long termine = coef[i] * potenza;
        risultato_naive += termine;
        printf("  termine %d: %+d * %d^%d = %+lld\n",
               i+1, coef[i], x_val, esp, termine);
    }
    printf(GRY "\n  Moltiplicazioni totali: %lld  (O(n\xc2\xb2) con n=%d)\n" RST, naive_muls, grado);
    printf(YLW "  p(%d) = %lld\n\n" RST, x_val, risultato_naive);
    printf(GRY "  [INVIO=continua  ESC=esci] " RST); fflush(stdout);
    if (sim_aspetta_passo()) return 0;

    /* ── METODO HORNER passo-passo ─────────────────────────────────── */
    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x93\x90  Schema di Horner — p(%d)\n\n" RST, x_val);
    printf(GRY "  Riscrittura:\n" RST);
    printf("  p(x) = x\xc2\xb3 - 6x\xc2\xb2 + 11x - 6\n");
    printf("       = ((1*x - 6)*x + 11)*x - 6\n\n");
    printf(GRY "  [INVIO per procedere passo per passo] " RST); fflush(stdout);
    if (sim_aspetta_passo()) return 0;

    long long acc = coef[0];   /* parte dall'ultimo coefficiente di grado piu' alto */
    char msg[128];
    snprintf(msg, sizeof msg, "Inizializza: acc = %d  (coeff. grado %d)", coef[0], grado);
    sim_log_push(msg);

    for (int i = 1; i <= grado; i++) {
        long long acc_prec = acc;
        acc = acc * x_val + coef[i];
        snprintf(msg, sizeof msg,
                 "Passo %d: acc = acc*x + c[%d] = %lld*%d + (%d) = %lld",
                 i, i, acc_prec, x_val, coef[i], acc);
        sim_log_push(msg);
        clear_screen(); print_header();
        printf(CYN BLD "\n  \xf0\x9f\x93\x90  Schema di Horner — passo %d/%d\n\n" RST, i, grado);
        for (int k = 0; k < _sim_log_n; k++) {
            if (k < _sim_log_n-1) printf(GRY "  %s\n" RST, _sim_log[k]);
            else                  printf(CYN BLD "  %s\n" RST, _sim_log[k]);
        }
        printf(GRY "\n  Moltiplicazioni: %d di %d (O(n))\n" RST, i, grado);
        printf("\n" GRY "  [INVIO=passo  ESC=esci] " RST); fflush(stdout);
        if (sim_aspetta_passo()) return 0;
    }

    /* ── TABLEAU DI RUFFINI ────────────────────────────────────────── */
    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x93\x90  Regola di Ruffini \xe2\x80\x94 divisione per (x-%d)\n\n" RST, x_val);
    printf(GRY "  Stessa aritmetica di Horner, presentata come tableau:\n\n" RST);

    /* riga coefficienti */
    printf("  Coefficienti:  ");
    for (int i = 0; i <= grado; i++) printf("%5d", coef[i]);
    printf("\n");
    printf("  c=%d           ", x_val);
    for (int i = 0; i <= grado; i++) printf("     ");
    printf("\n");

    /* calcola righe tableau */
    long long tab[4] = {0};
    tab[0] = coef[0];
    printf("                 " GRN "%5lld" RST, tab[0]);
    for (int i = 1; i <= grado; i++) {
        tab[i] = tab[i-1] * x_val + coef[i];
        printf(i < grado ? GRN "%5lld" RST : YLW "%5lld" RST, tab[i]);
    }
    printf("\n\n");

    printf("  Quoziente: ");
    for (int i = 0; i < grado; i++) {
        int esp = grado - 1 - i;
        if (esp == 0)      printf(GRN "  %+lld" RST, tab[i]);
        else if (esp == 1) printf(GRN "  %+lldx" RST, tab[i]);
        else               printf(GRN "  %+lldx^%d" RST, tab[i], esp);
    }
    printf("\n");
    printf("  Resto:     " YLW "%lld" RST "  (= p(%d))\n\n", tab[grado], x_val);

    if (tab[grado] == 0)
        printf(GRN "  \xe2\x9c\x94 Resto=0  \xe2\x86\x92  %d e' una radice di p(x)!\n\n" RST, x_val);
    else
        printf(GRY "  Resto=%lld  \xe2\x86\x92  %d non e' una radice\n\n" RST, tab[grado], x_val);

    printf(GRY "  [INVIO] " RST); fflush(stdout);
    if (sim_aspetta_passo()) return 0;

    /* ── Confronto finale ─────────────────────────────────────────── */
    clear_screen(); print_header();
    printf(GRN BLD "\n  \xe2\x9c\x94  Confronto: p(%d) = %lld\n\n" RST, x_val, acc);
    printf("  %-22s  moltiplic. naive O(n\xc2\xb2) : %lld\n", "Metodo Naive",   naive_muls);
    printf("  %-22s  moltiplic. Horner O(n)  : %d\n\n", "Schema di Horner", grado);
    printf(GRY "  Paolo Ruffini (1765-1822): tableau = Horner scritto in colonna\n");
    printf("  Usato ancora oggi in algebra, compilatori, DSP, NF4 log-quant.\n\n" RST);
    printf(GRY "  [INVIO] " RST); fflush(stdout); sim_aspetta_passo(); return 1;
}
