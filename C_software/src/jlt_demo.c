/*
 * jlt_demo.c — Programma dimostrativo per le JLT
 *
 * Compila con:
 *   gcc -O2 -Wall -o jlt_demo jlt_demo.c jlt.c -lm
 *
 * Dal paper arXiv:2103.00564 (Freksen 2021):
 *   - Lemma 1.1: per ogni X ⊂ R^d, esiste f: R^d→R^m con m=Θ(ε⁻²log|X|)
 *     tale che per ogni x,y ∈ X:  |‖f(x)-f(y)‖² - ‖x-y‖²| ≤ ε‖x-y‖²
 */

#include "jlt.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ------------------------------------------------------------------ */
/*  Generatori di dataset di test                                     */
/* ------------------------------------------------------------------ */

/* Vettori casuali uniformi in [-1,1]^d */
static double *gen_random(int n, int d, uint64_t seed) {
    uint64_t s = seed;
    double *X = (double *)malloc((size_t)n * d * sizeof(double));
    for (int i = 0; i < n * d; i++) {
        s ^= s << 13; s ^= s >> 7; s ^= s << 17;
        X[i] = (double)(s >> 11) / (double)(1ULL << 52) * 2.0 - 1.0;
    }
    return X;
}

/* Vettori sparse: k entry non-zero casuali (worst-case per alcune JLT) */
static double *gen_sparse(int n, int d, int k, uint64_t seed) {
    uint64_t s = seed;
    double *X = (double *)calloc(n * d, sizeof(double));
    for (int i = 0; i < n; i++) {
        for (int t = 0; t < k; t++) {
            s ^= s << 13; s ^= s >> 7; s ^= s << 17;
            int j = (int)((s >> 11) % (uint64_t)d);
            s ^= s << 13; s ^= s >> 7; s ^= s << 17;
            double v = (double)(s >> 11) / (double)(1ULL << 52) * 2.0 - 1.0;
            X[(size_t)i * d + j] = v;
        }
    }
    return X;
}

/* ------------------------------------------------------------------ */
/*  Sezione 1: Info formule e dimensioni target                       */
/* ------------------------------------------------------------------ */

static void demo_formule(void) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════╗\n");
    printf("║     Lemma JL — Formula dimensione target m            ║\n");
    printf("║     m = ceil( 8 * ln(n) / ε² )                       ║\n");
    printf("╚═══════════════════════════════════════════════════════╝\n\n");

    int ns[]   = { 100, 1000, 10000, 100000 };
    double epsilons[] = { 0.1, 0.2, 0.3 };

    printf("  %-8s", "|X|");
    for (int e = 0; e < 3; e++)
        printf("  ε=%.1f ", epsilons[e]);
    printf("\n");
    printf("  %-8s", "--------");
    for (int e = 0; e < 3; e++)
        printf("  ------");
    printf("\n");

    for (int ni = 0; ni < 4; ni++) {
        printf("  %-8d", ns[ni]);
        for (int e = 0; e < 3; e++) {
            int m = jlt_compute_m(ns[ni], epsilons[e], 0.05);
            printf("  m=%-4d", m);
        }
        printf("\n");
    }
}

/* ------------------------------------------------------------------ */
/*  Sezione 2: Verifica proprietà JL per ogni variante                */
/* ------------------------------------------------------------------ */

static void demo_verifica(int d, int n, double eps, double delta,
                          uint64_t seed)
{
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════╗\n");
    printf("║     Verifica proprietà JL — Lemma 1.1                ║\n");
    printf("║     d=%4d  n=%4d  ε=%.2f  δ=%.2f               ║\n",
           d, n, eps, delta);
    printf("╚═══════════════════════════════════════════════════════╝\n\n");

    double *X = gen_random(n, d, seed);

    struct {
        JLT_Method method;
        const char *nome;
    } variants[] = {
        { JLT_GAUSSIAN,   "Gaussiana       " },
        { JLT_RADEMACHER, "Rademacher      " },
        { JLT_ACHLIOPTAS, "Achlioptas Sparse"},
        { JLT_FEAT_HASH,  "Feature Hashing " },
        { JLT_FJLT,       "FJLT (P·H·D·x)  " },
        { JLT_SRHT,       "SRHT (S·H·D·x)  " },
    };

    printf("  %-20s  %5s  %8s  %8s\n",
           "Variante", "m", "OK%", "μs/vec");
    printf("  %-20s  %5s  %8s  %8s\n",
           "--------------------", "-----", "--------", "--------");

    for (int v = 0; v < 6; v++) {
        JLTContext *ctx = jlt_create(variants[v].method, d, n,
                                     eps, delta, 0, seed + v);
        if (!ctx) {
            printf("  %-20s  ERRORE allocazione\n", variants[v].nome);
            continue;
        }

        double pct = jlt_verify(ctx, X, n, 500) * 100.0;

        /* Benchmark */
        clock_t t0 = clock();
        double *tmp_out = (double *)malloc((size_t)n * ctx->m * sizeof(double));
        jlt_embed_batch(ctx, X, n, tmp_out);
        clock_t t1 = clock();
        double us = (double)(t1 - t0) / CLOCKS_PER_SEC * 1e6 / n;
        free(tmp_out);

        printf("  %-20s  %5d  %7.1f%%  %6.2fμs\n",
               variants[v].nome, ctx->m, pct, us);

        jlt_free(ctx);
    }

    free(X);
}

/* ------------------------------------------------------------------ */
/*  Sezione 3: Demo visuale — proietta 5 vettori 8D → 3D             */
/* ------------------------------------------------------------------ */

static void demo_visuale(void) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════╗\n");
    printf("║     Demo visuale: 5 vettori R^8 → R^3                ║\n");
    printf("╚═══════════════════════════════════════════════════════╝\n\n");

    int d = 8, n = 5, m_target = 3;
    double X[5][8] = {
        { 1.0,  0.0,  0.0,  0.5,  0.0,  0.0,  0.0,  0.0 },
        { 0.0,  1.0,  0.0,  0.0,  0.5,  0.0,  0.0,  0.0 },
        { 0.0,  0.0,  1.0,  0.0,  0.0,  0.5,  0.0,  0.0 },
        { 0.5,  0.5,  0.0,  0.0,  0.0,  0.0,  0.5,  0.0 },
        {-0.5,  0.0,  0.5,  0.0,  0.0,  0.0,  0.0,  0.5 },
    };

    JLT_Method methods[] = { JLT_GAUSSIAN, JLT_FJLT, JLT_SRHT };
    const char *mnames[] = { "Gaussiana", "FJLT", "SRHT" };

    /* Distanze originali */
    printf("  Distanze originali (R^8):\n");
    for (int i = 0; i < n; i++) {
        for (int j = i+1; j < n; j++) {
            double d_orig = sqrt(jlt_l2sq(X[i], X[j], d));
            printf("    d(v%d,v%d) = %.4f\n", i, j, d_orig);
        }
    }

    for (int mv = 0; mv < 3; mv++) {
        JLTContext *ctx = jlt_create(methods[mv], d, n, 0.3, 0.1,
                                     m_target, 42 + mv);
        if (!ctx) continue;
        printf("\n  [ %s → R^%d ]\n", mnames[mv], ctx->m);

        double out[5][3];
        for (int i = 0; i < n; i++)
            jlt_embed(ctx, X[i], out[i]);

        for (int i = 0; i < n; i++)
            printf("    v%d → [%+.3f, %+.3f, %+.3f]\n",
                   i, out[i][0], out[i][1], out[i][2]);

        printf("  Distanze proiettate vs originali:\n");
        for (int i = 0; i < n; i++) {
            for (int j = i+1; j < n; j++) {
                double d_orig = sqrt(jlt_l2sq(X[i], X[j], d));
                double d_emb  = sqrt(jlt_l2sq(out[i], out[j], ctx->m));
                double ratio  = (d_orig > 1e-10) ? d_emb / d_orig : 0.0;
                printf("    d(v%d,v%d): orig=%.3f  emb=%.3f  ratio=%.3f %s\n",
                       i, j, d_orig, d_emb, ratio,
                       (ratio >= 1.0 - 0.3 && ratio <= 1.0 + 0.3) ? "OK" : "!!");
            }
        }
        jlt_free(ctx);
    }
}

/* ------------------------------------------------------------------ */
/*  Sezione 4: Confronto Sparse vs Dense — velocità su vettori sparsi */
/* ------------------------------------------------------------------ */

static void demo_sparse_vs_dense(int d, int n, double eps) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════╗\n");
    printf("║     Sparse vs Dense — vettori sparsi (nnz = d/10)    ║\n");
    printf("╚═══════════════════════════════════════════════════════╝\n\n");

    int k = d / 10;
    double *X = gen_sparse(n, d, k, 0xC0FFEE);
    double *out_d = NULL, *out_s = NULL;

    printf("  d=%d  n=%d  k=%d  ε=%.1f\n\n", d, n, k, eps);

    JLTContext *gaus = jlt_create(JLT_GAUSSIAN,   d, n, eps, 0.05, 0, 1);
    JLTContext *ach  = jlt_create(JLT_ACHLIOPTAS, d, n, eps, 0.05, 0, 2);
    JLTContext *fh   = jlt_create(JLT_FEAT_HASH,  d, n, eps, 0.05, 0, 3);

    if (!gaus || !ach || !fh) { goto cleanup_sparse; }

    out_d = (double *)malloc((size_t)n * gaus->m * sizeof(double));
    out_s = (double *)malloc((size_t)n * ach->m  * sizeof(double));

    clock_t t0, t1;

    t0 = clock();
    jlt_embed_batch(gaus, X, n, out_d);
    t1 = clock();
    printf("  Gaussiana     m=%4d  tempo: %.2f ms\n",
           gaus->m, (double)(t1-t0)/CLOCKS_PER_SEC*1000.0);

    t0 = clock();
    jlt_embed_batch(ach, X, n, out_s);
    t1 = clock();
    printf("  Achlioptas    m=%4d  tempo: %.2f ms\n",
           ach->m, (double)(t1-t0)/CLOCKS_PER_SEC*1000.0);

    t0 = clock();
    jlt_embed_batch(fh, X, n, out_s);
    t1 = clock();
    printf("  Feat. Hash    m=%4d  tempo: %.2f ms  (O(nnz) per vettore)\n",
           fh->m, (double)(t1-t0)/CLOCKS_PER_SEC*1000.0);

cleanup_sparse:
    jlt_free(gaus); jlt_free(ach); jlt_free(fh);
    free(X); free(out_d); free(out_s);
}

/* ================================================================== */
/*  main                                                              */
/* ================================================================== */

int main(int argc, char **argv) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════╗\n");
    printf("║   Johnson–Lindenstrauss Transforms — Prismalux        ║\n");
    printf("║   arXiv:2103.00564 (Freksen, 2021)                    ║\n");
    printf("╚═══════════════════════════════════════════════════════╝\n");

    uint64_t seed = (argc > 1) ? (uint64_t)atoll(argv[1]) : 12345ULL;

    /* 1. Tabella formule */
    demo_formule();

    /* 2. Verifica JL property su vettori medi */
    demo_verifica(256, 200, 0.2, 0.05, seed);

    /* 3. Demo visuale piccola */
    demo_visuale();

    /* 4. Sparse vs dense */
    demo_sparse_vs_dense(1024, 300, 0.2);

    printf("\n=== Fine demo JLT ===\n\n");
    return 0;
}
