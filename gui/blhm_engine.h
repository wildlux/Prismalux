/* blhm_engine.h — Brain-Loop-Human-MultiContext C engine
 *
 * Thread pool pattern adapted from antirez/ds4 (MIT License).
 * Three parallel reading cycles (factory · link · user) via POSIX threads.
 * wildlux 2026 — MIT License
 */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* ══════════════════════════════════════════════════════════════════════════
   Constants
   ══════════════════════════════════════════════════════════════════════════ */
#define BLHM_MAX_DEPTH    8     /* max ontological depth                       */
#define BLHM_MAX_CREDS    8     /* max credential IDs per weight               */
#define BLHM_LABEL_LEN    64    /* max label string length                     */
#define BLHM_MAX_LABELS   2048  /* label registry capacity                     */
#define BLHM_N_CYCLES     3     /* factory · link · user — always 3            */
#define BLHM_VERSION      1     /* .blhm file format version                   */

/* ══════════════════════════════════════════════════════════════════════════
   BLHMWeight — 96 bytes (24× a standard float scalar)
   Layout matches paper §3.1: factory/link/user + ontological path + stats
   ══════════════════════════════════════════════════════════════════════════ */
typedef struct {
    float   factory_w;                   /* pre-training component — FROZEN    */
    float   link_w;                      /* ontological link — auto-finetuning */
    float   user_w;                      /* session context — runtime          */
    float   safety_w;                    /* sensitivity [0.0 public–1.0 priv.] */
    int32_t path[BLHM_MAX_DEPTH];        /* ontological path IDs               */
    int32_t path_len;                    /* depth of ontological path          */
    float   activation_sum;             /* cumulative activation (autoft stat) */
    int32_t use_count;                   /* total activations (autoft stat)    */
    int32_t req_creds[BLHM_MAX_CREDS];   /* credential IDs — AND-gate          */
    int32_t n_req_creds;                 /* number of required credentials     */
} BLHMWeight;  /* 4+4+4+4 + 32+4+4+4 + 32+4 = 96 bytes                       */

/* ══════════════════════════════════════════════════════════════════════════
   BLHMCycleResult — output of one reading cycle
   ══════════════════════════════════════════════════════════════════════════ */
#define BLHM_TOP_K  8

typedef struct {
    float  score;                /* accumulated cycle score                    */
    int    n_active;             /* weights that passed gate + match > 0       */
    int    top_idx[BLHM_TOP_K];  /* indices of top-scoring weights             */
    float  top_score[BLHM_TOP_K];
    int    n_top;
} BLHMCycleResult;

/* ══════════════════════════════════════════════════════════════════════════
   BLHMResult — full inference result (all three cycles + combined)
   ══════════════════════════════════════════════════════════════════════════ */
typedef struct {
    BLHMCycleResult factory;   /* R_f = Σ factory_w · match · gate            */
    BLHMCycleResult link;      /* R_l = Σ link_w · match² · gate             */
    BLHMCycleResult user;      /* R_u = Σ user_w · gate                       */
    float  combined;           /* 0.50·R_f + 0.35·R_l + 0.15·R_u             */
    double latency_ms;         /* wall-clock time of the whole inference       */
} BLHMResult;

/* ══════════════════════════════════════════════════════════════════════════
   BLHMGraph — opaque handle to the ontological weight graph
   ══════════════════════════════════════════════════════════════════════════ */
typedef struct BLHMGraph BLHMGraph;

BLHMGraph*        blhm_graph_create(int max_weights);
void              blhm_graph_free(BLHMGraph *g);
int               blhm_graph_add(BLHMGraph *g, const BLHMWeight *w);
int               blhm_graph_count(const BLHMGraph *g);
const BLHMWeight* blhm_graph_get(const BLHMGraph *g, int idx);
BLHMWeight*       blhm_graph_get_mutable(BLHMGraph *g, int idx);
void              blhm_graph_clear(BLHMGraph *g);

/* ── Label registry ────────────────────────────────────────────────────── */
int         blhm_label_register(BLHMGraph *g, const char *name); /* returns ID */
int         blhm_label_find(const BLHMGraph *g, const char *name); /* -1 = miss */
const char* blhm_label_name(const BLHMGraph *g, int id);

/* ══════════════════════════════════════════════════════════════════════════
   Core math
   ══════════════════════════════════════════════════════════════════════════ */
/* match(w,q) = |common_prefix| / max(|w|,|q|)  — paper eq. (2) */
float blhm_match(const int32_t *w_path, int w_len,
                 const int32_t *q_path, int q_len);

/* gate = 1 if all req_creds are present in creds array (AND-logic) */
bool  blhm_gate(const BLHMWeight *w, const int32_t *creds, int n_creds);

/* ══════════════════════════════════════════════════════════════════════════
   Inference — 3 POSIX threads in parallel (adapted from ds4 pool pattern)
   ══════════════════════════════════════════════════════════════════════════ */
BLHMResult blhm_infer(BLHMGraph      *g,
                      const int32_t  *query_path, int query_len,
                      const int32_t  *creds,      int n_creds);

/* ══════════════════════════════════════════════════════════════════════════
   Auto-finetuning — Hebbian reinforcement on link_w
   link_w[i] += lr · match(w_i, q)  for all weights that activated
   ══════════════════════════════════════════════════════════════════════════ */
void blhm_autoft(BLHMGraph      *g,
                 const int32_t  *query_path, int query_len,
                 float           lr);

/* ── User context update ─────────────────────────────────────────────── */
void blhm_update_user(BLHMGraph      *g,
                      const int32_t  *path, int path_len, float delta);

/* ══════════════════════════════════════════════════════════════════════════
   Persistence — simple binary .blhm format
   ══════════════════════════════════════════════════════════════════════════ */
bool       blhm_save(const BLHMGraph *g, const char *path);
BLHMGraph* blhm_load(const char *path);

/* ── Thread pool lifecycle (called implicitly, but exposed for cleanup) ── */
void blhm_pool_shutdown(void);

#ifdef __cplusplus
}
#endif
