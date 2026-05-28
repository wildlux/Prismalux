/* blhm_engine.c — Brain-Loop-Human-MultiContext inference engine
 *
 * Thread-pool architecture adapted from antirez/ds4 (MIT):
 *   github.com/antirez/ds4  —  ds4.c lines 830-1000  (ds4_thread_pool,
 *   ds4_worker_main, ds4_threads_init, ds4_parallel_for).
 *
 * Key adaptation: ds4 dispatches row-parallel matrix operations across N
 * threads.  BLHM dispatches exactly 3 fixed reading cycles (factory, link,
 * user) to 3 dedicated threads, one cycle per thread.  The generation-based
 * synchronisation mechanism is retained unchanged.
 *
 * wildlux 2026 — MIT License
 */

#include "blhm_engine.h"

#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ══════════════════════════════════════════════════════════════════════════
   BLHMGraph — internal definition
   ══════════════════════════════════════════════════════════════════════════ */
struct BLHMGraph {
    BLHMWeight *weights;
    int         n_weights;
    int         cap_weights;

    /* label registry: id → name */
    char        labels[BLHM_MAX_LABELS][BLHM_LABEL_LEN];
    int         n_labels;
};

/* ── Allocation helpers ──────────────────────────────────────────────────── */
static void *blhm_malloc(size_t n) {
    void *p = malloc(n);
    if (!p) { fprintf(stderr, "[BLHM] malloc(%zu) failed\n", n); exit(1); }
    return p;
}
static void *blhm_calloc(size_t n, size_t sz) {
    void *p = calloc(n, sz);
    if (!p) { fprintf(stderr, "[BLHM] calloc(%zu,%zu) failed\n", n, sz); exit(1); }
    return p;
}

/* ══════════════════════════════════════════════════════════════════════════
   Graph API
   ══════════════════════════════════════════════════════════════════════════ */
BLHMGraph* blhm_graph_create(int max_weights) {
    if (max_weights <= 0) max_weights = 4096;
    BLHMGraph *g = blhm_calloc(1, sizeof(BLHMGraph));
    g->weights     = blhm_malloc((size_t)max_weights * sizeof(BLHMWeight));
    g->cap_weights = max_weights;
    g->n_weights   = 0;
    g->n_labels    = 0;
    return g;
}

void blhm_graph_free(BLHMGraph *g) {
    if (!g) return;
    free(g->weights);
    free(g);
}

int blhm_graph_add(BLHMGraph *g, const BLHMWeight *w) {
    if (!g || !w) return -1;
    if (g->n_weights >= g->cap_weights) {
        int newcap = g->cap_weights * 2;
        BLHMWeight *nw = realloc(g->weights,
                                 (size_t)newcap * sizeof(BLHMWeight));
        if (!nw) return -1;
        g->weights     = nw;
        g->cap_weights = newcap;
    }
    g->weights[g->n_weights] = *w;
    return g->n_weights++;
}

int blhm_graph_count(const BLHMGraph *g) { return g ? g->n_weights : 0; }

const BLHMWeight* blhm_graph_get(const BLHMGraph *g, int idx) {
    if (!g || idx < 0 || idx >= g->n_weights) return NULL;
    return &g->weights[idx];
}

BLHMWeight* blhm_graph_get_mutable(BLHMGraph *g, int idx) {
    if (!g || idx < 0 || idx >= g->n_weights) return NULL;
    return &g->weights[idx];
}

void blhm_graph_clear(BLHMGraph *g) {
    if (g) g->n_weights = 0;
}

/* ── Label registry ──────────────────────────────────────────────────────── */
int blhm_label_register(BLHMGraph *g, const char *name) {
    if (!g || !name) return -1;
    int existing = blhm_label_find(g, name);
    if (existing >= 0) return existing;
    if (g->n_labels >= BLHM_MAX_LABELS) return -1;
    int id = g->n_labels++;
    strncpy(g->labels[id], name, BLHM_LABEL_LEN - 1);
    g->labels[id][BLHM_LABEL_LEN - 1] = '\0';
    return id;
}

int blhm_label_find(const BLHMGraph *g, const char *name) {
    if (!g || !name) return -1;
    for (int i = 0; i < g->n_labels; i++)
        if (strncmp(g->labels[i], name, BLHM_LABEL_LEN) == 0) return i;
    return -1;
}

const char* blhm_label_name(const BLHMGraph *g, int id) {
    if (!g || id < 0 || id >= g->n_labels) return "";
    return g->labels[id];
}

/* ══════════════════════════════════════════════════════════════════════════
   Core math — match + gate
   ══════════════════════════════════════════════════════════════════════════ */
float blhm_match(const int32_t *w_path, int w_len,
                 const int32_t *q_path, int q_len) {
    if (!w_path || !q_path || w_len <= 0 || q_len <= 0) return 0.0f;
    int common  = 0;
    int min_len = w_len < q_len ? w_len : q_len;
    for (int i = 0; i < min_len; i++) {
        if (w_path[i] == q_path[i]) common++;
        else break;
    }
    int denom = w_len > q_len ? w_len : q_len;
    return denom > 0 ? (float)common / (float)denom : 0.0f;
}

bool blhm_gate(const BLHMWeight *w, const int32_t *creds, int n_creds) {
    if (!w || w->n_req_creds == 0) return true;  /* no gate: always open */
    if (!creds || n_creds == 0)    return false;  /* credentials required but none provided */
    /* AND-logic: every required credential must appear in creds */
    for (int r = 0; r < w->n_req_creds; r++) {
        bool found = false;
        for (int c = 0; c < n_creds; c++) {
            if (creds[c] == w->req_creds[r]) { found = true; break; }
        }
        if (!found) return false;
    }
    return true;
}

/* ══════════════════════════════════════════════════════════════════════════
   Thread pool — adapted from antirez/ds4 ds4_thread_pool
   ══════════════════════════════════════════════════════════════════════════ */

/* Context shared by all 3 cycle threads during one inference call */
typedef struct {
    /* inputs (set by main thread before dispatch) */
    const BLHMGraph  *graph;
    const int32_t    *query_path;
    int               query_len;
    const int32_t    *creds;
    int               n_creds;
    /* outputs (written by each dedicated thread) */
    BLHMCycleResult   factory;
    BLHMCycleResult   link;
    BLHMCycleResult   user;
} BLHMInferCtx;

/* ── Persistent 3-thread pool (ds4 pattern, fixed cycle count) ───────────── */
typedef struct {
    pthread_t       threads[BLHM_N_CYCLES]; /* thread[i] runs cycle i         */
    pthread_mutex_t mutex;
    pthread_cond_t  work_cond;
    pthread_cond_t  done_cond;
    uint32_t        generation;             /* incremented to dispatch work    */
    uint32_t        done;                   /* workers that finished this gen  */
    bool            shutdown;
    bool            initialized;
    BLHMInferCtx   *ctx;                    /* current inference context       */
} BLHMPool;

static BLHMPool g_pool;

/* ── Cycle implementations ───────────────────────────────────────────────── */

/* top-8 insertion helper */
static void cycle_push_top(BLHMCycleResult *res, int idx, float s) {
    if (res->n_top < BLHM_TOP_K) {
        res->top_idx[res->n_top]   = idx;
        res->top_score[res->n_top] = s;
        res->n_top++;
        return;
    }
    /* replace minimum if s is larger */
    int min_i = 0;
    for (int i = 1; i < BLHM_TOP_K; i++)
        if (res->top_score[i] < res->top_score[min_i]) min_i = i;
    if (s > res->top_score[min_i]) {
        res->top_idx[min_i]   = idx;
        res->top_score[min_i] = s;
    }
}

/* R_factory = Σ [ factory_w · match · gate ] */
static void blhm_factory_cycle(BLHMInferCtx *ctx) {
    BLHMCycleResult *res = &ctx->factory;
    memset(res, 0, sizeof(*res));
    const BLHMGraph *g = ctx->graph;
    for (int i = 0; i < g->n_weights; i++) {
        const BLHMWeight *w = &g->weights[i];
        if (!blhm_gate(w, ctx->creds, ctx->n_creds)) continue;
        float m = blhm_match(w->path, w->path_len,
                             ctx->query_path, ctx->query_len);
        if (m == 0.0f) continue;
        float contrib = w->factory_w * m;
        res->score += contrib;
        res->n_active++;
        cycle_push_top(res, i, contrib);
    }
}

/* R_link = Σ [ link_w · match² · gate ] */
static void blhm_link_cycle(BLHMInferCtx *ctx) {
    BLHMCycleResult *res = &ctx->link;
    memset(res, 0, sizeof(*res));
    const BLHMGraph *g = ctx->graph;
    for (int i = 0; i < g->n_weights; i++) {
        const BLHMWeight *w = &g->weights[i];
        if (!blhm_gate(w, ctx->creds, ctx->n_creds)) continue;
        float m = blhm_match(w->path, w->path_len,
                             ctx->query_path, ctx->query_len);
        if (m == 0.0f) continue;
        float contrib = w->link_w * m * m;
        res->score += contrib;
        res->n_active++;
        cycle_push_top(res, i, contrib);
    }
}

/* R_user = Σ [ user_w · gate ] */
static void blhm_user_cycle(BLHMInferCtx *ctx) {
    BLHMCycleResult *res = &ctx->user;
    memset(res, 0, sizeof(*res));
    const BLHMGraph *g = ctx->graph;
    for (int i = 0; i < g->n_weights; i++) {
        const BLHMWeight *w = &g->weights[i];
        if (!blhm_gate(w, ctx->creds, ctx->n_creds)) continue;
        if (w->user_w == 0.0f) continue;
        float contrib = w->user_w;
        res->score += contrib;
        res->n_active++;
        cycle_push_top(res, i, contrib);
    }
}

/* Cycle dispatch table — thread[i] calls kCycleFns[i] */
typedef void (*BLHMCycleFn)(BLHMInferCtx *);
static const BLHMCycleFn kCycleFns[BLHM_N_CYCLES] = {
    blhm_factory_cycle,
    blhm_link_cycle,
    blhm_user_cycle
};

/* ── Worker thread (adapted from ds4_worker_main) ────────────────────────── */
static void *blhm_worker_main(void *arg) {
    const uint32_t tid   = (uint32_t)(uintptr_t)arg;
    uint32_t seen_gen    = 0;

    for (;;) {
        pthread_mutex_lock(&g_pool.mutex);
        while (seen_gen == g_pool.generation && !g_pool.shutdown)
            pthread_cond_wait(&g_pool.work_cond, &g_pool.mutex);
        if (g_pool.shutdown) {
            pthread_mutex_unlock(&g_pool.mutex);
            return NULL;
        }
        seen_gen        = g_pool.generation;
        BLHMInferCtx *c = g_pool.ctx;
        pthread_mutex_unlock(&g_pool.mutex);

        /* each thread runs its assigned cycle */
        kCycleFns[tid](c);

        pthread_mutex_lock(&g_pool.mutex);
        g_pool.done++;
        if (g_pool.done == BLHM_N_CYCLES)
            pthread_cond_signal(&g_pool.done_cond);
        pthread_mutex_unlock(&g_pool.mutex);
    }
}

/* ── Pool init (lazy, adapted from ds4_threads_init) ────────────────────── */
static void blhm_pool_init(void) {
    if (g_pool.initialized) return;

    pthread_mutex_init(&g_pool.mutex,     NULL);
    pthread_cond_init(&g_pool.work_cond,  NULL);
    pthread_cond_init(&g_pool.done_cond,  NULL);
    g_pool.generation   = 0;
    g_pool.done         = 0;
    g_pool.shutdown     = false;
    g_pool.initialized  = true;
    g_pool.ctx          = NULL;

    /* threads are 1-indexed in ds4; here we use 0-indexed (all are workers) */
    for (uint32_t i = 0; i < BLHM_N_CYCLES; i++) {
        if (pthread_create(&g_pool.threads[i], NULL,
                           blhm_worker_main, (void *)(uintptr_t)i) != 0) {
            fprintf(stderr, "[BLHM] failed to create cycle thread %u\n", i);
            exit(1);
        }
    }
}

void blhm_pool_shutdown(void) {
    if (!g_pool.initialized) return;

    pthread_mutex_lock(&g_pool.mutex);
    g_pool.shutdown = true;
    g_pool.generation++;
    pthread_cond_broadcast(&g_pool.work_cond);
    pthread_mutex_unlock(&g_pool.mutex);

    for (uint32_t i = 0; i < BLHM_N_CYCLES; i++)
        pthread_join(g_pool.threads[i], NULL);

    pthread_cond_destroy(&g_pool.done_cond);
    pthread_cond_destroy(&g_pool.work_cond);
    pthread_mutex_destroy(&g_pool.mutex);
    memset(&g_pool, 0, sizeof(g_pool));
}

/* ══════════════════════════════════════════════════════════════════════════
   blhm_infer — dispatch 3 cycle threads and collect results
   Adapted from ds4_parallel_for dispatch pattern.
   ══════════════════════════════════════════════════════════════════════════ */
BLHMResult blhm_infer(BLHMGraph      *g,
                      const int32_t  *query_path, int query_len,
                      const int32_t  *creds,      int n_creds) {
    BLHMResult res;
    memset(&res, 0, sizeof(res));
    if (!g || g->n_weights == 0) return res;

    blhm_pool_init();  /* no-op after first call */

    /* wall-clock start */
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    /* fill shared context */
    BLHMInferCtx ctx = {
        .graph      = g,
        .query_path = query_path,
        .query_len  = query_len,
        .creds      = creds,
        .n_creds    = n_creds,
    };

    /* dispatch all 3 threads simultaneously (ds4 broadcast pattern) */
    pthread_mutex_lock(&g_pool.mutex);
    g_pool.ctx  = &ctx;
    g_pool.done = 0;
    g_pool.generation++;
    pthread_cond_broadcast(&g_pool.work_cond);

    /* wait for all cycles to complete */
    while (g_pool.done < BLHM_N_CYCLES)
        pthread_cond_wait(&g_pool.done_cond, &g_pool.mutex);
    pthread_mutex_unlock(&g_pool.mutex);

    /* collect results */
    res.factory  = ctx.factory;
    res.link     = ctx.link;
    res.user     = ctx.user;
    res.combined = 0.50f * res.factory.score
                 + 0.35f * res.link.score
                 + 0.15f * res.user.score;

    clock_gettime(CLOCK_MONOTONIC, &t1);
    res.latency_ms = (t1.tv_sec  - t0.tv_sec)  * 1000.0
                   + (t1.tv_nsec - t0.tv_nsec) / 1.0e6;

    /* update activation statistics */
    for (int i = 0; i < g->n_weights; i++) {
        BLHMWeight *w = &g->weights[i];
        float m = blhm_match(w->path, w->path_len, query_path, query_len);
        if (m > 0.0f && blhm_gate(w, creds, n_creds)) {
            w->activation_sum += m;
            w->use_count++;
        }
    }

    return res;
}

/* ══════════════════════════════════════════════════════════════════════════
   Auto-finetuning — Hebbian reinforcement on link_w
   link_w += lr · match  for every weight that activates
   factory_w is NEVER modified (frozen as per paper §3.1)
   ══════════════════════════════════════════════════════════════════════════ */
void blhm_autoft(BLHMGraph      *g,
                 const int32_t  *query_path, int query_len,
                 float           lr) {
    if (!g || g->n_weights == 0 || lr == 0.0f) return;
    for (int i = 0; i < g->n_weights; i++) {
        BLHMWeight *w = &g->weights[i];
        float m = blhm_match(w->path, w->path_len, query_path, query_len);
        if (m > 0.0f) {
            w->link_w += lr * m;
            /* clamp to [0.0, 2.0] */
            if (w->link_w < 0.0f) w->link_w = 0.0f;
            if (w->link_w > 2.0f) w->link_w = 2.0f;
        }
    }
}

/* ── User context ──────────────────────────────────────────────────────── */
void blhm_update_user(BLHMGraph      *g,
                      const int32_t  *path, int path_len, float delta) {
    if (!g) return;
    for (int i = 0; i < g->n_weights; i++) {
        BLHMWeight *w = &g->weights[i];
        float m = blhm_match(w->path, w->path_len, path, path_len);
        if (m > 0.0f) {
            w->user_w += delta * m;
            if (w->user_w < -1.0f) w->user_w = -1.0f;
            if (w->user_w >  1.0f) w->user_w =  1.0f;
        }
    }
}

/* ══════════════════════════════════════════════════════════════════════════
   Persistence — .blhm binary format
   Header: "BLHM" magic (4) + version (4) + n_labels (4) + n_weights (4)
   Labels: n_labels * BLHM_LABEL_LEN bytes
   Weights: n_weights * sizeof(BLHMWeight) bytes
   ══════════════════════════════════════════════════════════════════════════ */
#define BLHM_MAGIC 0x4D484C42u  /* "BLHM" LE */

bool blhm_save(const BLHMGraph *g, const char *path) {
    if (!g || !path) return false;
    FILE *f = fopen(path, "wb");
    if (!f) return false;

    uint32_t hdr[4] = {
        BLHM_MAGIC,
        BLHM_VERSION,
        (uint32_t)g->n_labels,
        (uint32_t)g->n_weights
    };
    if (fwrite(hdr, sizeof(hdr), 1, f) != 1)           goto fail;
    if (fwrite(g->labels, BLHM_LABEL_LEN, (size_t)g->n_labels, f) != (size_t)g->n_labels) goto fail;
    if (fwrite(g->weights, sizeof(BLHMWeight), (size_t)g->n_weights, f) != (size_t)g->n_weights) goto fail;

    fclose(f);
    return true;
fail:
    fclose(f);
    return false;
}

BLHMGraph* blhm_load(const char *path) {
    if (!path) return NULL;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    uint32_t hdr[4];
    if (fread(hdr, sizeof(hdr), 1, f) != 1)         goto fail;
    if (hdr[0] != BLHM_MAGIC || hdr[1] != BLHM_VERSION) goto fail;

    int n_labels  = (int)hdr[2];
    int n_weights = (int)hdr[3];
    if (n_labels < 0 || n_labels > BLHM_MAX_LABELS) goto fail;
    if (n_weights < 0)                               goto fail;

    BLHMGraph *g = blhm_graph_create(n_weights > 0 ? n_weights : 1);
    g->n_labels  = n_labels;

    if (n_labels > 0 &&
        fread(g->labels, BLHM_LABEL_LEN, (size_t)n_labels, f) != (size_t)n_labels)
        goto fail2;

    for (int i = 0; i < n_weights; i++) {
        BLHMWeight w;
        if (fread(&w, sizeof(BLHMWeight), 1, f) != 1) goto fail2;
        blhm_graph_add(g, &w);
    }

    fclose(f);
    return g;

fail2:
    blhm_graph_free(g);
fail:
    fclose(f);
    return NULL;
}
