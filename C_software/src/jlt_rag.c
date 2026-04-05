/*
 * jlt_rag.c — RAG accelerato con Johnson–Lindenstrauss Transform
 * ===============================================================
 * Applica SRHT (Subsampled Randomized Hadamard Transform, §1.4.2
 * di arXiv:2103.00564) agli embedding Ollama per ridurre la
 * dimensionalità prima della cosine similarity e della cache su disco.
 *
 * Garanzia matematica (Corollary 1.5 del paper):
 *   Se f è una JLT lineare e -y ∈ X, allora per ogni x,y ∈ X:
 *     |〈f(x), f(y)〉 - 〈x, y〉| ≤ ε ‖x‖₂ ‖y‖₂
 *   Applicato a vettori normalizzati (cosine similarity): l'ordine di
 *   ranking dei chunk è preservato con alta probabilità (1 - δ).
 *
 * Dipendenze: jlt.h, rag_embed.h, http.h, math.h
 */

#include "jlt_rag.h"
#include "rag_embed.h"
#include "http.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdint.h>
#include <time.h>

#ifdef _WIN32
#  include <direct.h>
#  define _jmk_dir(p) _mkdir(p)
#else
#  define _jmk_dir(p) mkdir((p), 0755)
#endif

/* ================================================================== */
/*  Costanti interne                                                   */
/* ================================================================== */

#define JLTRAG_CACHE_VERSION  1u
#define MAX_CHUNKS_PER_FILE   64

/* ── Header cache JLT-compressa ────────────────────────────────── */
typedef struct {
    uint32_t magic;        /* JLT_RAG_CACHE_MAGIC                    */
    uint32_t version;      /* JLTRAG_CACHE_VERSION                   */
    uint32_t d_orig;       /* dimensione embedding originale          */
    uint32_t m_dim;        /* dimensione compressa                    */
    uint64_t seed;         /* seme SRHT (riproducibile)               */
    uint32_t n;            /* numero chunk                            */
    uint32_t _pad;         /* allineamento a 8 byte                   */
} JltCacheHdr;             /* 32 byte totali                          */

/* ── Top-K entry ────────────────────────────────────────────────── */
typedef struct {
    char  text[RAG_EMBED_CHUNK_CHARS + 4];
    float score;
} TopKEntry;

/* ================================================================== */
/*  CRC32 (IEEE 802.3) — validazione cache                           */
/* ================================================================== */

static uint32_t _jtab[256];
static int      _jtab_ok = 0;

static void _jcrc_build(void) {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++)
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        _jtab[i] = c;
    }
    _jtab_ok = 1;
}

static uint32_t _jcrc32(const char *d, int n) {
    if (!_jtab_ok) _jcrc_build();
    uint32_t c = 0xFFFFFFFFu;
    for (int i = 0; i < n; i++)
        c = _jtab[(c ^ (unsigned char)d[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

/* ================================================================== */
/*  Seme deterministico dal nome del modello                          */
/*  Stessa funzione di hash → stessa SRHT tra sessioni diverse.      */
/* ================================================================== */

static uint64_t _seed_from_model(const char *model_name) {
    /* FNV-1a 64-bit */
    uint64_t h = 14695981039346656037ULL;
    for (const char *p = model_name; *p; p++) {
        h ^= (uint8_t)*p;
        h *= 1099511628211ULL;
    }
    return h ? h : 0xDEAD5EED00000001ULL;
}

/* ================================================================== */
/*  jlt_rag_init                                                      */
/* ================================================================== */

JltRagState *jlt_rag_init(const char *model_name, int d_orig, int n_chunks) {
    if (d_orig <= 0) return NULL;

    JltRagState *s = (JltRagState *)calloc(1, sizeof(JltRagState));
    if (!s) return NULL;

    s->d_orig = d_orig;
    s->seed   = _seed_from_model(model_name ? model_name : "default");

    /* ── Calcola m_dim ottimale con la formula del Lemma 1.2 ── */
    int m_formula = jlt_compute_m(n_chunks > 0 ? n_chunks : 100,
                                  JLT_RAG_EPSILON, JLT_RAG_DELTA);
    /* Clamp: [JLT_RAG_MIN_DIM, min(JLT_RAG_MAX_DIM, d_orig)] */
    if (m_formula < JLT_RAG_MIN_DIM) m_formula = JLT_RAG_MIN_DIM;
    if (m_formula > JLT_RAG_MAX_DIM) m_formula = JLT_RAG_MAX_DIM;
    if (m_formula > d_orig)          m_formula = d_orig; /* no upsampling */
    s->m_dim = m_formula;

    /* Se m == d, nessuna compressione utile → SRHT identità */
    if (s->m_dim == d_orig) {
        s->ratio     = 1.0;
        s->ctx       = NULL; /* segnale: nessuna compressione */
        return s;
    }

    /* ── Inizializza SRHT (§1.4.2): rapida e preserva cosine sim ── */
    s->ctx = jlt_create(JLT_SRHT, d_orig, n_chunks,
                        JLT_RAG_EPSILON, JLT_RAG_DELTA,
                        s->m_dim, s->seed);
    if (!s->ctx) {
        free(s);
        return NULL;
    }

    s->ratio = (double)s->m_dim / (double)d_orig;
    return s;
}

/* ================================================================== */
/*  jlt_rag_compress — float32 in/out (interfaccia RAG usa float)    */
/* ================================================================== */

void jlt_rag_compress(const JltRagState *state,
                      const float *in, float *out)
{
    if (!state) return;

    /* Nessuna compressione configurata (m == d) */
    if (!state->ctx) {
        memcpy(out, in, state->m_dim * sizeof(float));
        return;
    }

    /* Converte float → double, applica SRHT, riconverte double → float */
    double *tmp_in  = (double *)malloc(state->d_orig * sizeof(double));
    double *tmp_out = (double *)malloc(state->m_dim  * sizeof(double));
    if (!tmp_in || !tmp_out) {
        free(tmp_in); free(tmp_out);
        /* Fallback: copia i primi m_dim elementi */
        for (int i = 0; i < state->m_dim; i++)
            out[i] = (i < state->d_orig) ? in[i] : 0.0f;
        return;
    }

    for (int i = 0; i < state->d_orig; i++) tmp_in[i] = (double)in[i];
    jlt_embed(state->ctx, tmp_in, tmp_out);
    for (int i = 0; i < state->m_dim; i++) out[i] = (float)tmp_out[i];

    free(tmp_in);
    free(tmp_out);
}

/* ================================================================== */
/*  jlt_rag_cosine — cosine similarity in spazio compresso           */
/* ================================================================== */

float jlt_rag_cosine(const float *a, const float *b, int m_dim) {
    double dot = 0.0, na = 0.0, nb = 0.0;
    for (int i = 0; i < m_dim; i++) {
        dot += (double)a[i] * (double)b[i];
        na  += (double)a[i] * (double)a[i];
        nb  += (double)b[i] * (double)b[i];
    }
    double d = sqrt(na) * sqrt(nb);
    return (d < 1e-12) ? 0.0f : (float)(dot / d);
}

/* ================================================================== */
/*  jlt_rag_free                                                      */
/* ================================================================== */

void jlt_rag_free(JltRagState *state) {
    if (!state) return;
    jlt_free(state->ctx);
    free(state);
}

/* ================================================================== */
/*  Helpers filesystem (analoghi a rag_embed.c, namespace _jr_)      */
/* ================================================================== */

static void _jr_safe_model(const char *m, char *out, int max) {
    int i = 0;
    for (; m[i] && i < max - 1; i++) {
        char c = m[i];
        out[i] = (c == ':' || c == '/' || c == '\\' || c == ' ') ? '_' : c;
    }
    out[i] = '\0';
}

static void _jr_cache_path(const char *docs_dir, const char *model,
                            const char *fname, char *out, int max) {
    char ms[128];
    _jr_safe_model(model, ms, sizeof ms);
    /* Directory separata da rag_embed.c per evitare collisioni */
    snprintf(out, max, "%s/.rag_cache_jlt/%s/%s.jemb", docs_dir, ms, fname);
}

static void _jr_ensure_dirs(const char *docs_dir, const char *model) {
    char ms[128], path[512];
    _jr_safe_model(model, ms, sizeof ms);
    snprintf(path, sizeof path, "%s/.rag_cache_jlt", docs_dir);
    _jmk_dir(path);
    snprintf(path, sizeof path, "%s/.rag_cache_jlt/%s", docs_dir, ms);
    _jmk_dir(path);
}

static int _jr_cache_fresh(const char *src, const char *cache) {
    struct stat ss, sc;
    if (stat(src, &ss) != 0) return 0;
    if (stat(cache, &sc) != 0) return 0;
    return (sc.st_mtime >= ss.st_mtime) ? 1 : 0;
}

/* ================================================================== */
/*  Cache I/O — formato JLT                                          */
/*  JltCacheHdr (32 byte) + N × (crc32 + text_len + text + vec[m])  */
/* ================================================================== */

static int _jr_cache_load(const char *path,
                          char texts[][RAG_EMBED_CHUNK_CHARS + 4],
                          float *vecs, int max,
                          int *m_out, uint64_t expect_seed)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;

    JltCacheHdr h;
    if (fread(&h, sizeof h, 1, f) != 1             ||
        h.magic   != JLT_RAG_CACHE_MAGIC           ||
        h.version != JLTRAG_CACHE_VERSION          ||
        h.seed    != expect_seed                   ||
        h.m_dim   == 0 || h.m_dim > 4096          ||
        h.n == 0  || h.n > (uint32_t)max) {
        fclose(f); return 0;
    }

    int n = (int)h.n, m = (int)h.m_dim;
    for (int i = 0; i < n; i++) {
        uint32_t crc_skip, tlen;
        if (fread(&crc_skip, 4, 1, f) != 1) { fclose(f); return 0; }
        if (fread(&tlen, 4, 1, f) != 1 || tlen >= RAG_EMBED_CHUNK_CHARS + 4) {
            fclose(f); return 0;
        }
        if ((int)fread(texts[i], 1, tlen, f) != (int)tlen) { fclose(f); return 0; }
        texts[i][tlen] = '\0';
        if ((int)fread(vecs + (size_t)i * m, sizeof(float), m, f) != m) {
            fclose(f); return 0;
        }
    }

    fclose(f);
    *m_out = m;
    return n;
}

static void _jr_cache_save(const char *path, int d_orig,
                           const JltRagState *state,
                           char texts[][RAG_EMBED_CHUNK_CHARS + 4],
                           float *vecs, int n)
{
    FILE *f = fopen(path, "wb");
    if (!f) return;

    JltCacheHdr h;
    memset(&h, 0, sizeof h);
    h.magic   = JLT_RAG_CACHE_MAGIC;
    h.version = JLTRAG_CACHE_VERSION;
    h.d_orig  = (uint32_t)d_orig;
    h.m_dim   = (uint32_t)state->m_dim;
    h.seed    = state->seed;
    h.n       = (uint32_t)n;
    h._pad    = 0;
    fwrite(&h, sizeof h, 1, f);

    for (int i = 0; i < n; i++) {
        uint32_t tlen = (uint32_t)strlen(texts[i]);
        uint32_t crc  = _jcrc32(texts[i], (int)tlen);
        fwrite(&crc,          4,            1,  f);
        fwrite(&tlen,         4,            1,  f);
        fwrite(texts[i],      1,         tlen,  f);
        fwrite(vecs + (size_t)i * state->m_dim,
               sizeof(float), state->m_dim, f);
    }
    fclose(f);
}

/* ================================================================== */
/*  Chunking word-boundary (identico a rag_embed.c)                  */
/* ================================================================== */

static int _jr_next_chunk(const char *buf, int len, int *pos,
                           char *out, int out_max)
{
    if (*pos >= len) return 0;
    int end = *pos + RAG_EMBED_CHUNK_CHARS;
    if (end >= len) {
        end = len;
    } else {
        while (end > *pos && !isspace((unsigned char)buf[end])) end--;
        if (end == *pos) end = *pos + RAG_EMBED_CHUNK_CHARS;
    }
    int cl = end - *pos;
    if (cl <= 0) { (*pos)++; return 0; }
    if (cl >= out_max) cl = out_max - 1;
    memcpy(out, buf + *pos, cl);
    out[cl] = '\0';
    *pos = end;
    return cl;
}

/* ================================================================== */
/*  jlt_rag_query — versione JLT-accelerata di rag_embed_query()     */
/* ================================================================== */

int jlt_rag_query(const char *host, int port, const char *embed_model,
                  const char *docs_dir, const char *query,
                  char *out_ctx, int out_max,
                  JltRagStats *stats_out)
{
    out_ctx[0] = '\0';
    if (!host || !embed_model || !embed_model[0] ||
        !docs_dir || !query || !out_ctx || out_max < 8) return 0;

    if (stats_out) memset(stats_out, 0, sizeof *stats_out);

    /* ── 1. Embedding della query (dimensione completa d_orig) ── */
    float *qvec_full = (float *)malloc(RAG_EMBED_MAX_DIM * sizeof(float));
    if (!qvec_full) return 0;
    int d_orig = 0;

    if (http_ollama_embed(host, port, embed_model, query,
                          qvec_full, &d_orig, RAG_EMBED_MAX_DIM) != 0) {
        fprintf(stderr, "[JLT-RAG] Embedding query fallito\n");
        free(qvec_full);
        return 0;
    }

    /* ── 2. Stima n_chunks totali (scansione rapida directory) ── */
    int n_est = 0;
    {
        DIR *d2 = opendir(docs_dir);
        struct dirent *e2;
        if (d2) {
            while ((e2 = readdir(d2)) != NULL) {
                int nl = (int)strlen(e2->d_name);
                if (nl >= 5 && strcmp(e2->d_name + nl - 4, ".txt") == 0)
                    n_est += 20; /* stima conservativa per file */
            }
            closedir(d2);
        }
    }
    if (n_est < 10) n_est = 10;

    /* ── 3. Inizializza SRHT con d_orig e n_est ── */
    JltRagState *state = jlt_rag_init(embed_model, d_orig, n_est);
    if (!state) {
        free(qvec_full);
        return 0;
    }

    /* ── 4. Comprimi il vettore query con la SRHT ── */
    float *qvec = (float *)malloc(state->m_dim * sizeof(float));
    if (!qvec) { jlt_rag_free(state); free(qvec_full); return 0; }
    jlt_rag_compress(state, qvec_full, qvec);
    free(qvec_full);

    /* ── 5. Setup directory cache JLT ── */
    _jr_ensure_dirs(docs_dir, embed_model);

    /* ── 6. Top-K heap ── */
    TopKEntry top[RAG_EMBED_TOP_K];
    int top_n = 0;
    char ftexts[MAX_CHUNKS_PER_FILE][RAG_EMBED_CHUNK_CHARS + 4];

    int n_total = 0, cache_hits = 0;

    /* ── 7. Scansione documenti ── */
    DIR *dir = opendir(docs_dir);
    if (!dir) { jlt_rag_free(state); free(qvec); return 0; }

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        const char *name = ent->d_name;
        int nlen = (int)strlen(name);
        if (nlen < 5 || strcmp(name + nlen - 4, ".txt") != 0) continue;

        char filepath[512], cpath[512];
        snprintf(filepath, sizeof filepath, "%s/%s", docs_dir, name);
        _jr_cache_path(docs_dir, embed_model, name, cpath, sizeof cpath);

        int nc = 0, m_cached = 0;
        float *fvecs = NULL;

        /* ── Prova a caricare dalla cache JLT ── */
        if (_jr_cache_fresh(filepath, cpath)) {
            /* Leggi header per conoscere n prima di allocare */
            FILE *cf = fopen(cpath, "rb");
            if (cf) {
                JltCacheHdr h;
                int hok = (fread(&h, sizeof h, 1, cf) == 1        &&
                           h.magic   == JLT_RAG_CACHE_MAGIC        &&
                           h.version == JLTRAG_CACHE_VERSION       &&
                           h.seed    == state->seed                &&
                           h.m_dim   == (uint32_t)state->m_dim     &&
                           h.d_orig  == (uint32_t)d_orig           &&
                           h.n > 0   && h.n <= MAX_CHUNKS_PER_FILE);
                fclose(cf);
                if (hok) {
                    int exp_n = (int)h.n;
                    fvecs = (float *)malloc((size_t)exp_n * state->m_dim
                                           * sizeof(float));
                    if (fvecs) {
                        nc = _jr_cache_load(cpath, ftexts, fvecs,
                                            MAX_CHUNKS_PER_FILE,
                                            &m_cached, state->seed);
                        if (nc != exp_n || m_cached != state->m_dim) {
                            free(fvecs); fvecs = NULL; nc = 0;
                        } else {
                            cache_hits += nc;
                        }
                    }
                }
            }
        }

        /* ── Ricalcola se cache mancante o invalidata ── */
        if (nc == 0) {
            FILE *tf = fopen(filepath, "r");
            if (!tf) continue;
            char *buf = (char *)malloc(32768);
            if (!buf) { fclose(tf); continue; }
            int blen = (int)fread(buf, 1, 32767, tf);
            fclose(tf);
            if (blen <= 0) { free(buf); continue; }
            buf[blen] = '\0';

            /* Chunking */
            int pos2 = 0, n_chunks = 0;
            while (n_chunks < MAX_CHUNKS_PER_FILE) {
                int cl = _jr_next_chunk(buf, blen, &pos2,
                                        ftexts[n_chunks],
                                        RAG_EMBED_CHUNK_CHARS + 4);
                if (cl == 0) break;
                n_chunks++;
            }
            free(buf);
            if (n_chunks == 0) continue;

            /* Buffer per embedding COMPRESSI */
            fvecs = (float *)malloc((size_t)n_chunks * state->m_dim
                                    * sizeof(float));
            if (!fvecs) continue;

            /* Buffer temporaneo per embedding COMPLETI (d_orig) */
            float *tmp_full = (float *)malloc(d_orig * sizeof(float));
            if (!tmp_full) { free(fvecs); continue; }

            int ok = 1;
            for (int i = 0; i < n_chunks && ok; i++) {
                int di = 0;
                if (http_ollama_embed(host, port, embed_model,
                                      ftexts[i], tmp_full,
                                      &di, d_orig) != 0) {
                    fprintf(stderr,
                        "[JLT-RAG] Errore embed chunk %d in '%s'\n", i, name);
                    /* Se Ollama cade: abort */
                    ok = 0;
                    break;
                }
                /* Comprimi con SRHT: d_orig → m_dim */
                jlt_rag_compress(state,
                                 tmp_full,
                                 fvecs + (size_t)i * state->m_dim);
            }
            free(tmp_full);

            if (!ok) {
                free(fvecs);
                closedir(dir);
                jlt_rag_free(state);
                free(qvec);
                return 0;
            }
            nc = n_chunks;

            /* Salva cache JLT compressa */
            _jr_cache_save(cpath, d_orig, state, ftexts, fvecs, nc);
        }

        n_total += nc;

        /* ── Aggiorna top-K: cosine similarity in spazio m_dim ── */
        for (int i = 0; i < nc; i++) {
            float score = jlt_rag_cosine(qvec,
                                         fvecs + (size_t)i * state->m_dim,
                                         state->m_dim);
            if (top_n < RAG_EMBED_TOP_K) {
                memcpy(top[top_n].text, ftexts[i], RAG_EMBED_CHUNK_CHARS + 4);
                top[top_n].score = score;
                top_n++;
            } else {
                int mi = 0;
                for (int k = 1; k < RAG_EMBED_TOP_K; k++)
                    if (top[k].score < top[mi].score) mi = k;
                if (score > top[mi].score) {
                    memcpy(top[mi].text, ftexts[i], RAG_EMBED_CHUNK_CHARS + 4);
                    top[mi].score = score;
                }
            }
        }
        free(fvecs);
    }
    closedir(dir);

    /* ── Statistiche ── */
    if (stats_out) {
        stats_out->d_orig          = d_orig;
        stats_out->m_compressed    = state->m_dim;
        stats_out->n_chunks_total  = n_total;
        stats_out->cache_hits      = cache_hits;
        stats_out->compress_ratio  = state->ratio;
        stats_out->speedup_est     = (state->ratio > 0.0)
                                     ? 1.0 / state->ratio : 1.0;
    }

    jlt_rag_free(state);
    free(qvec);

    if (top_n == 0) return 0;

    /* ── Ordina top-K per score decrescente ── */
    for (int i = 0; i < top_n - 1; i++)
        for (int j = i + 1; j < top_n; j++)
            if (top[j].score > top[i].score) {
                TopKEntry tmp = top[i]; top[i] = top[j]; top[j] = tmp;
            }

    /* ── Concatena in out_ctx ── */
    int pos = 0, written = 0;
    for (int i = 0; i < top_n && pos < out_max - 4; i++) {
        if (top[i].score <= 0.0f) continue;
        int rem  = out_max - pos - 2;
        int tlen = (int)strlen(top[i].text);
        if (tlen > rem) tlen = rem;
        memcpy(out_ctx + pos, top[i].text, tlen);
        pos += tlen;
        out_ctx[pos++] = '\n';
        written++;
    }
    out_ctx[pos] = '\0';
    return written;
}

/* ================================================================== */
/*  jlt_rag_print_stats                                               */
/* ================================================================== */

void jlt_rag_print_stats(const JltRagStats *s) {
    if (!s) return;
    printf("[JLT-RAG] d_orig=%d  m=%d  ratio=%.2f  speedup=~%.1fx"
           "  chunk=%d  cache_hit=%d\n",
           s->d_orig, s->m_compressed,
           s->compress_ratio, s->speedup_est,
           s->n_chunks_total, s->cache_hits);
}
