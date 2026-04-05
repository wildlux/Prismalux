/*
 * jlt.h — Johnson–Lindenstrauss Transforms
 *
 * Implementazione basata su:
 *   "An Introduction to Johnson–Lindenstrauss Transforms"
 *   Casper Benjamin Freksen, arXiv:2103.00564v1 (2021)
 *
 * Varianti implementate:
 *   JLT_GAUSSIAN    Dense Gaussiana i.i.d. N(0,1) / sqrt(m)         [§1.4]
 *   JLT_RADEMACHER  Dense Rademacher ±1 / sqrt(m)                   [§1.4]
 *   JLT_ACHLIOPTAS  Sparse Achlioptas (2/3 zeri, ±sqrt(3/m))        [§1.4.1]
 *   JLT_FEAT_HASH   Feature Hashing / Count Sketch (s=1)            [§1.3.2]
 *   JLT_FJLT        Fast JLT: f(x) = P·H·D·x                       [§1.4.2]
 *   JLT_SRHT        Subsampled Randomized Hadamard Transform         [§1.4.2]
 *
 * Uso rapido:
 *   JLTContext *ctx = jlt_create(JLT_GAUSSIAN, d, n, 0.1, 0.05, 0, 42);
 *   jlt_embed(ctx, x, out);   // R^d -> R^m
 *   jlt_free(ctx);
 */

#ifndef JLT_H
#define JLT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  Tipi                                                               */
/* ------------------------------------------------------------------ */

typedef enum {
    JLT_GAUSSIAN   = 0,
    JLT_RADEMACHER = 1,
    JLT_ACHLIOPTAS = 2,
    JLT_FEAT_HASH  = 3,
    JLT_FJLT       = 4,
    JLT_SRHT       = 5,
} JLT_Method;

/*
 * JLTContext — contiene tutto lo stato della trasformazione.
 * Non accedere ai campi interni direttamente: usa le funzioni pubbliche.
 */
typedef struct {
    JLT_Method method;
    int        d;           /* dimensione sorgente                    */
    int        m;           /* dimensione target                      */
    double     epsilon;     /* distorsione massima consentita         */
    double     delta;       /* probabilità di fallimento              */

    /* --- Metodi densi (Gaussian, Rademacher, Achlioptas) --- */
    double    *A;           /* matrice m×d row-major                  */

    /* --- Feature Hashing --- */
    int       *fh_h;        /* h : [d] → [m]   (indice bucket)       */
    int       *fh_sigma;    /* σ : [d] → {-1,+1}  (segno)            */

    /* --- FJLT e SRHT: preprocessing WHT --- */
    int       *D_signs;     /* vettore diagonale ±1, lunghezza d_pad  */
    int        d_pad;       /* prossima potenza di 2 >= d             */

    /* --- FJLT: matrice sparsa P (m × d_pad) in formato COO --- */
    int       *fjlt_row;
    int       *fjlt_col;
    double    *fjlt_val;
    int        fjlt_nnz;

    /* --- SRHT: m indici di riga campionati senza ripetizione --- */
    int       *srht_rows;
} JLTContext;

/* ------------------------------------------------------------------ */
/*  API pubblica                                                       */
/* ------------------------------------------------------------------ */

/*
 * jlt_compute_m — formula dimensione target (Lemma 1.1 / 1.2 del paper)
 *   m = ceil(8 * ln(n_points) / epsilon^2)
 *   Se delta > 0 usa: m = ceil(8 * ln(1/delta) / epsilon^2)
 *   e prende il massimo dei due.
 */
int jlt_compute_m(int n_points, double epsilon, double delta);

/*
 * jlt_create — alloca e inizializza un JLTContext.
 *
 *   method      : variante da usare (JLT_GAUSSIAN, ...)
 *   d           : dimensione sorgente
 *   n_points    : |X|, usato per calcolare m automaticamente
 *   epsilon     : distorsione (0 < ε < 1), tipicamente 0.1 – 0.3
 *   delta       : prob. fallimento (0 < δ < 1), tipicamente 0.01 – 0.1
 *   m_override  : se > 0, usa questo valore di m invece della formula
 *   seed        : seme per il generatore casuale (0 = usa time)
 *
 *   Ritorna NULL in caso di errore.
 */
JLTContext *jlt_create(JLT_Method method, int d, int n_points,
                       double epsilon, double delta,
                       int m_override, uint64_t seed);

/*
 * jlt_embed — proietta un singolo vettore x ∈ R^d → out ∈ R^m.
 *   out deve essere pre-allocato con almeno ctx->m double.
 */
void jlt_embed(const JLTContext *ctx, const double *x, double *out);

/*
 * jlt_embed_batch — proietta n vettori.
 *   X   : matrice n×d row-major (X[i*d + j] = j-esima componente del vettore i)
 *   out : matrice n×m row-major (pre-allocata)
 */
void jlt_embed_batch(const JLTContext *ctx, const double *X, int n,
                     double *out);

/*
 * jlt_verify — verifica la proprietà JL su un insieme di n vettori.
 *   Controlla max_pairs coppie casuali e ritorna la frazione di coppie
 *   che soddisfano |(‖f(x)-f(y)‖² / ‖x-y‖²) - 1| ≤ epsilon.
 *   Ritorna 1.0 se tutti OK, < 1.0 se alcune coppie violano il bound.
 *   Ritorna -1.0 se n < 2 o max_pairs < 1.
 */
double jlt_verify(const JLTContext *ctx, const double *X, int n,
                  int max_pairs);

/*
 * jlt_print_info — stampa un riepilogo del contesto su stdout.
 */
void jlt_print_info(const JLTContext *ctx);

/*
 * jlt_free — libera tutta la memoria.
 */
void jlt_free(JLTContext *ctx);

/* ------------------------------------------------------------------ */
/*  Utility esposte (usabili anche standalone)                         */
/* ------------------------------------------------------------------ */

/*
 * jlt_wht_inplace — Walsh–Hadamard Transform in-place (non normalizzata).
 *   n deve essere una potenza di 2.
 *   Complessità: O(n log n).
 */
void jlt_wht_inplace(double *x, int n);

/*
 * jlt_l2sq — distanza L2 al quadrato tra due vettori di lunghezza d.
 */
double jlt_l2sq(const double *a, const double *b, int d);

#ifdef __cplusplus
}
#endif

#endif /* JLT_H */
