/*
 * jlt.c — Johnson–Lindenstrauss Transforms
 *
 * Implementazione delle varianti descritte in:
 *   "An Introduction to Johnson–Lindenstrauss Transforms"
 *   Casper Benjamin Freksen, arXiv:2103.00564v1 (2021)
 *
 *  §1.2  Lemma JL: m = Θ(ε⁻² log|X|)
 *  §1.4  Gaussiana e Rademacher dense
 *  §1.4.1 Achlioptas sparse, Feature Hashing
 *  §1.4.2 FJLT (f=P·H·D·x), SRHT (f=S·H·D·x)
 */

#include "jlt.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ================================================================== */
/*  PRNG interno — xorshift64 (veloce, sufficiente per costruire JLT) */
/* ================================================================== */

static uint64_t _rng_state;

static void rng_seed(uint64_t s) {
    _rng_state = s ? s : 0xDEADBEEF12345678ULL;
}

static uint64_t rng_u64(void) {
    uint64_t x = _rng_state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    _rng_state = x;
    return x;
}

/* Uniforme in [0,1) */
static double rng_double(void) {
    return (double)(rng_u64() >> 11) * (1.0 / (double)(1ULL << 53));
}

/*
 * Gaussiana N(0,1) via Box-Muller (Lemma §1.4, costruzione originale JL).
 * Usiamo il form polare per evitare log(0).
 */
static double rng_gaussian(void) {
    double u, v, s;
    do {
        u = rng_double() * 2.0 - 1.0;
        v = rng_double() * 2.0 - 1.0;
        s = u * u + v * v;
    } while (s >= 1.0 || s == 0.0);
    return u * sqrt(-2.0 * log(s) / s);
}

/* Rademacher: ±1 con prob 1/2 ciascuno */
static int rng_rademacher(void) {
    return (rng_u64() & 1) ? 1 : -1;
}

/*
 * Achlioptas §1.4.1:
 * Pr[a_ij = 0]    = 2/3
 * Pr[a_ij = +√3]  = 1/6
 * Pr[a_ij = -√3]  = 1/6
 * Il fattore finale 1/√m viene applicato dopo la moltiplicazione.
 */
static double rng_achlioptas(void) {
    uint64_t r = rng_u64() % 6;
    if (r < 4) return 0.0;          /* prob 4/6 = 2/3   */
    return (r == 4) ? 1.7320508075688772 : -1.7320508075688772; /* ±√3 */
}

/* ================================================================== */
/*  Formula dimensione target (Lemma 1.1 / 1.2)                       */
/*  m ≥ 8·ε⁻²·ln(n)  oppure  8·ε⁻²·ln(1/δ)                        */
/* ================================================================== */

int jlt_compute_m(int n_points, double epsilon, double delta) {
    if (epsilon <= 0.0 || epsilon >= 1.0) return -1;
    double inv_eps2 = 8.0 / (epsilon * epsilon);
    double m_n = (n_points > 1) ? inv_eps2 * log((double)n_points) : 1.0;
    double m_d = (delta > 0.0 && delta < 1.0) ? inv_eps2 * log(1.0 / delta) : 0.0;
    int m = (int)ceil(m_n > m_d ? m_n : m_d);
    return m < 1 ? 1 : m;
}

/* ================================================================== */
/*  Walsh–Hadamard Transform — O(n log n), n potenza di 2            */
/*  H_{ij} = (-1)^{<i,j>} (prodotto interno bit-a-bit)               */
/*  Versione non normalizzata: ‖Hx‖ = √n · ‖x‖                      */
/* ================================================================== */

void jlt_wht_inplace(double *x, int n) {
    for (int h = 1; h < n; h <<= 1) {
        for (int i = 0; i < n; i += h << 1) {
            for (int j = i; j < i + h; j++) {
                double a = x[j];
                double b = x[j + h];
                x[j]     = a + b;
                x[j + h] = a - b;
            }
        }
    }
}

/* Prossima potenza di 2 >= n */
static int next_pow2(int n) {
    int p = 1;
    while (p < n) p <<= 1;
    return p;
}

/* ================================================================== */
/*  Utility: distanza L2 al quadrato                                  */
/* ================================================================== */

double jlt_l2sq(const double *a, const double *b, int d) {
    double s = 0.0;
    for (int i = 0; i < d; i++) {
        double diff = a[i] - b[i];
        s += diff * diff;
    }
    return s;
}

/* ================================================================== */
/*  Costruzione Feature Hashing (§1.3.2, Count Sketch con s=1)       */
/*                                                                    */
/*  f(x)_j = Σ_{i: h(i)=j} σ(i) · x_i                              */
/*  dove h: [d]→[m] bucket hash, σ: [d]→{-1,+1} sign hash           */
/*                                                                    */
/*  Usiamo multiply-shift hashing con costanti 64-bit dispari.        */
/* ================================================================== */

static void _build_feat_hash(JLTContext *ctx) {
    uint64_t ah = rng_u64() | 1ULL;   /* costante hash per h, dispari */
    uint64_t bh = rng_u64() | 1ULL;   /* costante hash per σ, dispari */
    int bits = 0;
    for (int tmp = ctx->m - 1; tmp > 0; tmp >>= 1) bits++;
    if (bits == 0) bits = 1;
    uint64_t mask_m = (uint64_t)(ctx->m - 1);

    /* Se m è potenza di 2 usa maschera, altrimenti usa modulo */
    int m_is_pow2 = (ctx->m & (ctx->m - 1)) == 0;

    ctx->fh_h     = (int *)malloc(ctx->d * sizeof(int));
    ctx->fh_sigma = (int *)malloc(ctx->d * sizeof(int));

    for (int i = 0; i < ctx->d; i++) {
        uint64_t hi = ah * (uint64_t)(i + 1);
        uint64_t si = bh * (uint64_t)(i + 1);
        ctx->fh_h[i]     = m_is_pow2 ? (int)(hi & mask_m)
                                      : (int)((hi >> 32) % (uint64_t)ctx->m);
        ctx->fh_sigma[i] = (si >> 63) ? 1 : -1;
    }
}

/* ================================================================== */
/*  Costruzione FJLT — f(x) = P · H · D · x   (§1.4.2, AC09)        */
/*                                                                    */
/*  D ∈ {-1,+1}^{d×d}  diagonale Rademacher                         */
/*  H ∈ R^{d×d}        Walsh–Hadamard (poi normalizzata /√d_pad)     */
/*  P ∈ R^{m×d}        sparsa Achlioptas con q = m/d_pad per colonna */
/*                                                                    */
/*  Complessità embedding: O(d log d + m·q) ≈ O(d log d)             */
/* ================================================================== */

static void _build_fjlt(JLTContext *ctx) {
    int d_pad = ctx->d_pad;
    int m     = ctx->m;

    /* D: vettore diagonale ±1 */
    ctx->D_signs = (int *)malloc(d_pad * sizeof(int));
    for (int i = 0; i < d_pad; i++)
        ctx->D_signs[i] = rng_rademacher();

    /*
     * P sparsa: per ogni riga i e colonna j, l'entry è:
     *   0        con prob 1 - q
     *  ±scale    con prob q/2 ciascuno
     * dove q = min(1, c·log²(1/δ)/d_pad) ≈ m/d_pad per semplicità,
     * e scale = sqrt(d_pad / (q·m)) per preservare la norma.
     *
     * Usiamo q = m/d_pad (Achlioptas-style con densità proporzionale).
     * Garantisce ~m nonzeri per colonna in media.
     */
    double q     = (double)m / (double)d_pad;
    if (q > 1.0) q = 1.0;
    double scale = sqrt(1.0 / (q * (double)m));

    /* Stima nnz massimi: m * d_pad * q * 2 (con margine) */
    int nnz_max = (int)(m * d_pad * q * 3) + m + d_pad;
    ctx->fjlt_row = (int    *)malloc(nnz_max * sizeof(int));
    ctx->fjlt_col = (int    *)malloc(nnz_max * sizeof(int));
    ctx->fjlt_val = (double *)malloc(nnz_max * sizeof(double));
    ctx->fjlt_nnz = 0;

    for (int i = 0; i < m; i++) {
        for (int j = 0; j < d_pad; j++) {
            double r = rng_double();
            if (r < q / 2.0) {
                ctx->fjlt_row[ctx->fjlt_nnz] = i;
                ctx->fjlt_col[ctx->fjlt_nnz] = j;
                ctx->fjlt_val[ctx->fjlt_nnz] = scale;
                ctx->fjlt_nnz++;
            } else if (r < q) {
                ctx->fjlt_row[ctx->fjlt_nnz] = i;
                ctx->fjlt_col[ctx->fjlt_nnz] = j;
                ctx->fjlt_val[ctx->fjlt_nnz] = -scale;
                ctx->fjlt_nnz++;
            }
            if (ctx->fjlt_nnz >= nnz_max - 1) goto done_fjlt;
        }
    }
done_fjlt:;
}

/* ================================================================== */
/*  Costruzione SRHT — f(x) = sqrt(d/m) · S · H · D · x  (§1.4.2)  */
/*                                                                    */
/*  S ∈ {0,1}^{m×d} campiona m righe senza ripetizione               */
/*  D e H come FJLT                                                   */
/* ================================================================== */

static void _build_srht(JLTContext *ctx) {
    int d_pad = ctx->d_pad;
    int m     = ctx->m;

    /* D: vettore diagonale ±1 */
    ctx->D_signs = (int *)malloc(d_pad * sizeof(int));
    for (int i = 0; i < d_pad; i++)
        ctx->D_signs[i] = rng_rademacher();

    /* Campionamento senza ripetizione: Fisher-Yates su [0, d_pad) */
    int *perm = (int *)malloc(d_pad * sizeof(int));
    for (int i = 0; i < d_pad; i++) perm[i] = i;
    for (int i = d_pad - 1; i > 0; i--) {
        int j = (int)(rng_u64() % (uint64_t)(i + 1));
        int tmp = perm[i]; perm[i] = perm[j]; perm[j] = tmp;
    }
    int take = m < d_pad ? m : d_pad;
    ctx->srht_rows = (int *)malloc(take * sizeof(int));
    memcpy(ctx->srht_rows, perm, take * sizeof(int));
    free(perm);
}

/* ================================================================== */
/*  jlt_create                                                        */
/* ================================================================== */

JLTContext *jlt_create(JLT_Method method, int d, int n_points,
                       double epsilon, double delta,
                       int m_override, uint64_t seed)
{
    if (d <= 0 || epsilon <= 0.0 || epsilon >= 1.0) return NULL;

    rng_seed(seed ? seed : (uint64_t)time(NULL));

    JLTContext *ctx = (JLTContext *)calloc(1, sizeof(JLTContext));
    if (!ctx) return NULL;

    ctx->method  = method;
    ctx->d       = d;
    ctx->epsilon = epsilon;
    ctx->delta   = delta;
    ctx->m       = m_override > 0 ? m_override
                                  : jlt_compute_m(n_points, epsilon, delta);
    if (ctx->m <= 0) { free(ctx); return NULL; }

    /* Clamp m <= d (proiettare verso l'alto non ha senso) */
    if (ctx->m > d) ctx->m = d;

    ctx->d_pad = next_pow2(d);

    switch (method) {

    /* -------------------------------------------------------- */
    case JLT_GAUSSIAN: {
        double scale = 1.0 / sqrt((double)ctx->m);
        ctx->A = (double *)malloc((size_t)ctx->m * ctx->d * sizeof(double));
        if (!ctx->A) goto oom;
        for (int i = 0; i < ctx->m * ctx->d; i++)
            ctx->A[i] = rng_gaussian() * scale;
        break;
    }

    /* -------------------------------------------------------- */
    case JLT_RADEMACHER: {
        double scale = 1.0 / sqrt((double)ctx->m);
        ctx->A = (double *)malloc((size_t)ctx->m * ctx->d * sizeof(double));
        if (!ctx->A) goto oom;
        for (int i = 0; i < ctx->m * ctx->d; i++)
            ctx->A[i] = rng_rademacher() * scale;
        break;
    }

    /* -------------------------------------------------------- */
    case JLT_ACHLIOPTAS: {
        /*
         * Achlioptas §1.4.1: entry ~ {0 w.p. 2/3, ±√3 w.p. 1/6}
         * Dopo la proiezione si scala per 1/√m.
         * La varianza di ogni entry è (1/6·3 + 1/6·3) = 1, uguale
         * alla Gaussiana N(0,1) — quindi stesso bound JL.
         */
        double scale = 1.0 / sqrt((double)ctx->m);
        ctx->A = (double *)malloc((size_t)ctx->m * ctx->d * sizeof(double));
        if (!ctx->A) goto oom;
        for (int i = 0; i < ctx->m * ctx->d; i++)
            ctx->A[i] = rng_achlioptas() * scale;
        break;
    }

    /* -------------------------------------------------------- */
    case JLT_FEAT_HASH:
        _build_feat_hash(ctx);
        if (!ctx->fh_h || !ctx->fh_sigma) goto oom;
        break;

    /* -------------------------------------------------------- */
    case JLT_FJLT:
        _build_fjlt(ctx);
        if (!ctx->D_signs || !ctx->fjlt_row) goto oom;
        break;

    /* -------------------------------------------------------- */
    case JLT_SRHT:
        _build_srht(ctx);
        if (!ctx->D_signs || !ctx->srht_rows) goto oom;
        break;

    default:
        free(ctx);
        return NULL;
    }

    return ctx;

oom:
    jlt_free(ctx);
    return NULL;
}

/* ================================================================== */
/*  Embed — metodi densi (Gaussian, Rademacher, Achlioptas)           */
/* ================================================================== */

static void _embed_dense(const JLTContext *ctx,
                         const double *x, double *out)
{
    /* out = A · x     (A è m×d row-major) */
    int m = ctx->m, d = ctx->d;
    for (int i = 0; i < m; i++) {
        double s = 0.0;
        const double *row = ctx->A + (size_t)i * d;
        for (int j = 0; j < d; j++)
            s += row[j] * x[j];
        out[i] = s;
    }
}

/* ================================================================== */
/*  Embed — Feature Hashing                                           */
/* ================================================================== */

static void _embed_feat_hash(const JLTContext *ctx,
                             const double *x, double *out)
{
    memset(out, 0, ctx->m * sizeof(double));
    for (int i = 0; i < ctx->d; i++)
        out[ctx->fh_h[i]] += ctx->fh_sigma[i] * x[i];
}

/* ================================================================== */
/*  Embed — FJLT: f(x) = P · normalize(H · (D · x))                 */
/* ================================================================== */

static void _embed_fjlt(const JLTContext *ctx,
                        const double *x, double *out)
{
    int d_pad = ctx->d_pad;
    double *tmp = (double *)calloc(d_pad, sizeof(double));
    if (!tmp) return;

    /* 1. Copia x e applica D (moltiplicazione per segni diagonali) */
    for (int i = 0; i < ctx->d; i++)
        tmp[i] = x[i] * ctx->D_signs[i];
    /* Pad già zero (calloc) */

    /* 2. WHT in-place: O(d_pad · log d_pad) */
    jlt_wht_inplace(tmp, d_pad);

    /* 3. Normalizza: dividi per √d_pad così ‖H·D·x‖ ≈ ‖x‖ */
    double norm_wht = 1.0 / sqrt((double)d_pad);
    for (int i = 0; i < d_pad; i++)
        tmp[i] *= norm_wht;

    /* 4. Applica P sparsa (COO): out = P · tmp */
    memset(out, 0, ctx->m * sizeof(double));
    for (int k = 0; k < ctx->fjlt_nnz; k++)
        out[ctx->fjlt_row[k]] += ctx->fjlt_val[k] * tmp[ctx->fjlt_col[k]];

    free(tmp);
}

/* ================================================================== */
/*  Embed — SRHT: f(x) = √(d_pad/m) · (H·D·x)[srht_rows]           */
/* ================================================================== */

static void _embed_srht(const JLTContext *ctx,
                        const double *x, double *out)
{
    int d_pad = ctx->d_pad;
    int m     = ctx->m;
    double *tmp = (double *)calloc(d_pad, sizeof(double));
    if (!tmp) return;

    /* 1. D · x */
    for (int i = 0; i < ctx->d; i++)
        tmp[i] = x[i] * ctx->D_signs[i];

    /* 2. WHT in-place */
    jlt_wht_inplace(tmp, d_pad);

    /* 3. Campiona m righe e scala: fattore √(d_pad/m) / √d_pad = 1/√m
     *    (il WHT non normalizzato scala di √d_pad, dobbiamo compensare) */
    double scale = 1.0 / sqrt((double)m);
    for (int i = 0; i < m; i++)
        out[i] = tmp[ctx->srht_rows[i]] * scale;

    free(tmp);
}

/* ================================================================== */
/*  jlt_embed — dispatcher pubblico                                   */
/* ================================================================== */

void jlt_embed(const JLTContext *ctx, const double *x, double *out)
{
    switch (ctx->method) {
    case JLT_GAUSSIAN:
    case JLT_RADEMACHER:
    case JLT_ACHLIOPTAS:
        _embed_dense(ctx, x, out);
        break;
    case JLT_FEAT_HASH:
        _embed_feat_hash(ctx, x, out);
        break;
    case JLT_FJLT:
        _embed_fjlt(ctx, x, out);
        break;
    case JLT_SRHT:
        _embed_srht(ctx, x, out);
        break;
    }
}

/* ================================================================== */
/*  jlt_embed_batch                                                   */
/* ================================================================== */

void jlt_embed_batch(const JLTContext *ctx, const double *X, int n,
                     double *out)
{
    for (int i = 0; i < n; i++)
        jlt_embed(ctx, X + (size_t)i * ctx->d, out + (size_t)i * ctx->m);
}

/* ================================================================== */
/*  jlt_verify — verifica proprietà JL (Lemma 1.1)                   */
/*                                                                    */
/*  Per ogni coppia (x,y): | ‖f(x)-f(y)‖² / ‖x-y‖² - 1 | ≤ ε      */
/* ================================================================== */

double jlt_verify(const JLTContext *ctx, const double *X, int n,
                  int max_pairs)
{
    if (n < 2 || max_pairs < 1) return -1.0;

    double *Xemb = (double *)malloc((size_t)n * ctx->m * sizeof(double));
    if (!Xemb) return -1.0;
    jlt_embed_batch(ctx, X, n, Xemb);

    int ok = 0, total = 0;
    double eps = ctx->epsilon;

    /* Campionamento casuale di coppie (evita O(n²) per n grande) */
    for (int iter = 0; iter < max_pairs; iter++) {
        int i = (int)(rng_u64() % (uint64_t)n);
        int j = (int)(rng_u64() % (uint64_t)n);
        if (i == j) { j = (j + 1) % n; }

        double dist_orig = jlt_l2sq(X    + (size_t)i * ctx->d,
                                    X    + (size_t)j * ctx->d, ctx->d);
        double dist_emb  = jlt_l2sq(Xemb + (size_t)i * ctx->m,
                                    Xemb + (size_t)j * ctx->m, ctx->m);

        if (dist_orig < 1e-15) { /* vettori identici, salta */
            continue;
        }

        double ratio = dist_emb / dist_orig;
        double err   = fabs(ratio - 1.0);
        if (err <= eps) ok++;
        total++;
    }

    free(Xemb);
    return total > 0 ? (double)ok / total : 1.0;
}

/* ================================================================== */
/*  jlt_print_info                                                    */
/* ================================================================== */

void jlt_print_info(const JLTContext *ctx)
{
    static const char *names[] = {
        "Gaussiana", "Rademacher", "Achlioptas Sparse",
        "Feature Hashing", "FJLT (P·H·D)", "SRHT (S·H·D)"
    };
    printf("=== Johnson-Lindenstrauss Transform ===\n");
    printf("  Variante  : %s\n", names[ctx->method]);
    printf("  d sorgente: %d\n", ctx->d);
    printf("  m target  : %d  (riduzione: %.1f%%)\n",
           ctx->m, 100.0 * (1.0 - (double)ctx->m / ctx->d));
    printf("  epsilon   : %.4f\n", ctx->epsilon);
    printf("  delta     : %.4f\n", ctx->delta);
    if (ctx->method == JLT_FJLT)
        printf("  nnz(P)    : %d  (densita': %.3f%%)\n",
               ctx->fjlt_nnz,
               100.0 * ctx->fjlt_nnz / ((double)ctx->m * ctx->d_pad));
    if (ctx->method == JLT_FJLT || ctx->method == JLT_SRHT)
        printf("  d_pad     : %d\n", ctx->d_pad);
    printf("=======================================\n");
}

/* ================================================================== */
/*  jlt_free                                                          */
/* ================================================================== */

void jlt_free(JLTContext *ctx)
{
    if (!ctx) return;
    free(ctx->A);
    free(ctx->fh_h);
    free(ctx->fh_sigma);
    free(ctx->D_signs);
    free(ctx->fjlt_row);
    free(ctx->fjlt_col);
    free(ctx->fjlt_val);
    free(ctx->srht_rows);
    free(ctx);
}
