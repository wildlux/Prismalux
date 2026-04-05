/*
 * rag_embed.c — RAG con embedding semantici via Ollama
 * =====================================================
 * Motore alternativo al TF-overlap: usa cosine similarity su vettori
 * generati da un modello embedding Ollama per trovare i chunk piu'
 * pertinenti alla query, indipendentemente dalla sovrapposizione lessicale.
 *
 * Cache binaria: <docs_dir>/.rag_cache/<model>/<file>.emb
 *   Ricalcolata solo se il file sorgente e' piu' recente della cache.
 *
 * Dipendenze: http.h (http_ollama_embed, http_ollama_list), math.h (-lm)
 */
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

#ifdef _WIN32
#  include <direct.h>
#  define _mk_dir(p) _mkdir(p)
#else
#  define _mk_dir(p) mkdir((p), 0755)
#endif

/* ── Costanti interne ─────────────────────────────────────────── */
#define EMBED_CACHE_VERSION  1u
#define MAX_CHUNKS_PER_FILE  64   /* chunk massimi per singolo .txt */

/* ── Struttura top-K: mantiene i migliori RAG_EMBED_TOP_K chunk ── */
typedef struct {
    char  text[RAG_EMBED_CHUNK_CHARS + 4];
    float score; /* cosine similarity con la query */
} TopK;

/* ── Header file di cache binario ─────────────────────────────── */
typedef struct {
    uint32_t magic;    /* RAG_EMBED_CACHE_MAGIC */
    uint32_t version;  /* EMBED_CACHE_VERSION   */
    uint32_t dim;      /* dimensione vettore    */
    uint32_t n;        /* numero chunk          */
} CacheHdr;

/* ══════════════════════════════════════════════════════════════
   CRC32 IEEE 802.3 — validazione contenuto chunk in cache
   ══════════════════════════════════════════════════════════════ */
static uint32_t _ctab[256];
static int      _ctab_ok = 0;

static void _crc_build(void) {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++)
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        _ctab[i] = c;
    }
    _ctab_ok = 1;
}

static uint32_t _crc32(const char *d, int n) {
    if (!_ctab_ok) _crc_build();
    uint32_t c = 0xFFFFFFFFu;
    for (int i = 0; i < n; i++)
        c = _ctab[(c ^ (unsigned char)d[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

/* ══════════════════════════════════════════════════════════════
   Cosine similarity — usa double internamente per precisione
   ══════════════════════════════════════════════════════════════ */
static float _cos_sim(const float *a, const float *b, int dim) {
    double dot = 0.0, na = 0.0, nb = 0.0;
    for (int i = 0; i < dim; i++) {
        dot += (double)a[i] * (double)b[i];
        na  += (double)a[i] * (double)a[i];
        nb  += (double)b[i] * (double)b[i];
    }
    double d = sqrt(na) * sqrt(nb);
    return (d < 1e-12) ? 0.0f : (float)(dot / d);
}

/* ══════════════════════════════════════════════════════════════
   Chunking word-boundary: estrae il prossimo chunk da buf[*pos].
   Aggiorna *pos. Ritorna lunghezza del chunk (0 = fine).
   ══════════════════════════════════════════════════════════════ */
static int _next_chunk(const char *buf, int len, int *pos,
                        char *out, int out_max) {
    if (*pos >= len) return 0;
    int end = *pos + RAG_EMBED_CHUNK_CHARS;
    if (end >= len) {
        end = len;
    } else {
        /* arretra fino a un confine di parola */
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

/* ── Nome modello sicuro per filesystem (':' e '/' → '_') ──────── */
static void _safe_model(const char *m, char *out, int max) {
    int i = 0;
    for (; m[i] && i < max - 1; i++) {
        char c = m[i];
        out[i] = (c == ':' || c == '/' || c == '\\' || c == ' ') ? '_' : c;
    }
    out[i] = '\0';
}

/* ── Costruisce il percorso cache per un file ─────────────────── */
static void _cache_path(const char *docs_dir, const char *model,
                         const char *fname, char *out, int max) {
    char ms[128];
    _safe_model(model, ms, sizeof ms);
    snprintf(out, max, "%s/.rag_cache/%s/%s.emb", docs_dir, ms, fname);
}

/* ── Crea directory cache (due livelli) ───────────────────────── */
static void _ensure_cache_dirs(const char *docs_dir, const char *model) {
    char ms[128], path[512];
    _safe_model(model, ms, sizeof ms);
    snprintf(path, sizeof path, "%s/.rag_cache", docs_dir);
    _mk_dir(path);
    snprintf(path, sizeof path, "%s/.rag_cache/%s", docs_dir, ms);
    _mk_dir(path);
}

/* ── Confronto mtime: ritorna 1 se la cache e' fresca ────────── */
static int _cache_fresh(const char *src_path, const char *cache_path_val) {
    struct stat st_src, st_cache;
    if (stat(src_path, &st_src) != 0) return 0;
    if (stat(cache_path_val, &st_cache) != 0) return 0;
    return (st_cache.st_mtime >= st_src.st_mtime) ? 1 : 0;
}

/* ══════════════════════════════════════════════════════════════
   Cache I/O binario
   Formato: CacheHdr (16 byte) + per ogni chunk:
     uint32_t crc32  | uint32_t text_len | char text[text_len]
     float vec[dim]  (dim float a 32 bit, little-endian nativo)
   ══════════════════════════════════════════════════════════════ */

/* Carica embedding dalla cache. Ritorna n_chunks se OK, 0 se fallisce.
   vecs: buffer pre-allocato [max * dim] float (stride = dim). */
static int _cache_load(const char *path,
                        char texts[][RAG_EMBED_CHUNK_CHARS + 4],
                        float *vecs, int max, int *dim_out) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;

    CacheHdr h;
    if (fread(&h, sizeof h, 1, f) != 1                    ||
        h.magic   != RAG_EMBED_CACHE_MAGIC                ||
        h.version != EMBED_CACHE_VERSION                  ||
        h.dim == 0 || h.dim > RAG_EMBED_MAX_DIM           ||
        h.n == 0   || h.n > (uint32_t)max) {
        fclose(f); return 0;
    }

    int n = (int)h.n, dim = (int)h.dim;
    for (int i = 0; i < n; i++) {
        uint32_t crc_skip, tlen;
        if (fread(&crc_skip, 4, 1, f) != 1) { fclose(f); return 0; }
        if (fread(&tlen, 4, 1, f) != 1 || tlen >= RAG_EMBED_CHUNK_CHARS + 4) {
            fclose(f); return 0;
        }
        if ((int)fread(texts[i], 1, tlen, f) != (int)tlen) { fclose(f); return 0; }
        texts[i][tlen] = '\0';
        if ((int)fread(vecs + (size_t)i * dim, sizeof(float), dim, f) != dim) {
            fclose(f); return 0;
        }
    }

    fclose(f);
    *dim_out = dim;
    return n;
}

/* Salva embedding su disco (stride = dim). Ignora errori I/O. */
static void _cache_save(const char *path, int dim,
                         char texts[][RAG_EMBED_CHUNK_CHARS + 4],
                         float *vecs, int n) {
    FILE *f = fopen(path, "wb");
    if (!f) return;

    CacheHdr h = { RAG_EMBED_CACHE_MAGIC, EMBED_CACHE_VERSION,
                   (uint32_t)dim, (uint32_t)n };
    fwrite(&h, sizeof h, 1, f);

    for (int i = 0; i < n; i++) {
        uint32_t tlen = (uint32_t)strlen(texts[i]);
        uint32_t crc  = _crc32(texts[i], (int)tlen);
        fwrite(&crc,      4,                  1, f);
        fwrite(&tlen,     4,                  1, f);
        fwrite(texts[i],  1,              tlen,  f);
        fwrite(vecs + (size_t)i * dim, sizeof(float), dim, f);
    }

    fclose(f);
}

/* ══════════════════════════════════════════════════════════════
   API pubblica
   ══════════════════════════════════════════════════════════════ */

int rag_embed_detect_model(const char *host, int port, char out_model[64]) {
    char names[32][128];
    int n = http_ollama_list(host, port, names, 32);
    out_model[0] = '\0';

    for (int i = 0; i < n; i++) {
        /* Cerca "embed" case-insensitive */
        char low[128];
        int j = 0;
        for (; names[i][j] && j < 127; j++)
            low[j] = (char)tolower((unsigned char)names[i][j]);
        low[j] = '\0';
        if (strstr(low, "embed")) {
            strncpy(out_model, names[i], 63);
            out_model[63] = '\0';
            return 1;
        }
    }
    return 0;
}

int rag_embed_query(const char *host, int port, const char *embed_model,
                    const char *docs_dir, const char *query,
                    char *out_ctx, int out_max) {
    out_ctx[0] = '\0';
    if (!host || !embed_model || !embed_model[0] || !docs_dir ||
        !query || !out_ctx || out_max < 8) return 0;

    /* ── Embedding della query ── */
    float *qvec = malloc(RAG_EMBED_MAX_DIM * sizeof(float));
    if (!qvec) return 0;
    int qdim = 0;
    if (http_ollama_embed(host, port, embed_model, query,
                          qvec, &qdim, RAG_EMBED_MAX_DIM) != 0) {
        fprintf(stderr, "[RAG-EMBED] Embedding query fallito\n");
        free(qvec);
        return 0;
    }

    /* ── Setup directory cache ── */
    _ensure_cache_dirs(docs_dir, embed_model);

    /* ── Top-K heap (K = RAG_EMBED_TOP_K) ── */
    TopK top[RAG_EMBED_TOP_K];
    int  top_n = 0;

    /* Buffer testi per un file alla volta: stack (64 * 516 ~ 32 KB) */
    char ftexts[MAX_CHUNKS_PER_FILE][RAG_EMBED_CHUNK_CHARS + 4];

    /* ── Scansione directory ── */
    DIR *dir = opendir(docs_dir);
    if (!dir) { free(qvec); return 0; }

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        const char *name = ent->d_name;
        int nlen = (int)strlen(name);
        if (nlen < 5 || strcmp(name + nlen - 4, ".txt") != 0) continue;

        char filepath[512], cpath[512];
        snprintf(filepath, sizeof filepath, "%s/%s", docs_dir, name);
        _cache_path(docs_dir, embed_model, name, cpath, sizeof cpath);

        /* ── Prova a caricare dalla cache ── */
        int nc = 0, dim = 0;
        float *fvecs = NULL;

        if (_cache_fresh(filepath, cpath)) {
            /* Legge header per conoscere dim e n prima di allocare */
            FILE *cf = fopen(cpath, "rb");
            if (cf) {
                CacheHdr h;
                int hok = (fread(&h, sizeof h, 1, cf) == 1      &&
                           h.magic   == RAG_EMBED_CACHE_MAGIC    &&
                           h.version == EMBED_CACHE_VERSION      &&
                           h.dim     == (uint32_t)qdim           &&
                           h.n > 0   && h.n <= MAX_CHUNKS_PER_FILE);
                fclose(cf);
                if (hok) {
                    int exp_n = (int)h.n;
                    fvecs = malloc((size_t)exp_n * qdim * sizeof(float));
                    if (fvecs) {
                        nc = _cache_load(cpath, ftexts, fvecs,
                                         MAX_CHUNKS_PER_FILE, &dim);
                        if (nc != exp_n) {
                            free(fvecs); fvecs = NULL; nc = 0; dim = 0;
                        }
                    }
                }
            }
        }

        /* ── Calcola embedding se la cache manca o e' invalida ── */
        if (nc == 0) {
            /* Leggi il file sorgente */
            FILE *tf = fopen(filepath, "r");
            if (!tf) continue;
            char *buf = malloc(32768);
            if (!buf) { fclose(tf); continue; }
            int blen = (int)fread(buf, 1, 32767, tf);
            fclose(tf);
            if (blen <= 0) { free(buf); continue; }
            buf[blen] = '\0';

            /* Suddividi in chunk */
            int pos = 0, n_chunks = 0;
            while (n_chunks < MAX_CHUNKS_PER_FILE) {
                int cl = _next_chunk(buf, blen, &pos,
                                     ftexts[n_chunks],
                                     RAG_EMBED_CHUNK_CHARS + 4);
                if (cl == 0) break;
                n_chunks++;
            }
            free(buf);
            if (n_chunks == 0) continue;

            /* Embedding del primo chunk: determina la dimensione del modello */
            float *tmp = malloc(RAG_EMBED_MAX_DIM * sizeof(float));
            if (!tmp) continue;
            int first_dim = 0;
            if (http_ollama_embed(host, port, embed_model,
                                  ftexts[0], tmp, &first_dim,
                                  RAG_EMBED_MAX_DIM) != 0) {
                fprintf(stderr,
                    "[RAG-EMBED] Errore embed chunk 0 in '%s' — Ollama disponibile?\n",
                    name);
                free(tmp);
                /* Abort dell'intera query: Ollama non risponde */
                closedir(dir);
                free(qvec);
                return 0;
            }
            dim = first_dim;
            free(tmp);

            /* Alloca buffer vecs con dimensione reale (non MAX_DIM) */
            fvecs = malloc((size_t)n_chunks * dim * sizeof(float));
            if (!fvecs) continue;

            /* Primo chunk: ricalcolato con buffer a dimensione corretta */
            int d0 = 0;
            if (http_ollama_embed(host, port, embed_model,
                                  ftexts[0], fvecs, &d0, dim) != 0) {
                free(fvecs);
                closedir(dir);
                free(qvec);
                return 0;
            }

            /* Chunk rimanenti */
            int ok = 1;
            for (int i = 1; i < n_chunks && ok; i++) {
                int di = 0;
                if (http_ollama_embed(host, port, embed_model,
                                      ftexts[i],
                                      fvecs + (size_t)i * dim,
                                      &di, dim) != 0) {
                    fprintf(stderr,
                        "[RAG-EMBED] Errore embed chunk %d in '%s'\n", i, name);
                    ok = 0;
                }
            }
            if (!ok) {
                free(fvecs);
                closedir(dir);
                free(qvec);
                return 0;
            }
            nc = n_chunks;

            /* Salva la cache (ignora errori di scrittura) */
            _cache_save(cpath, dim, ftexts, fvecs, nc);
        }

        /* ── Aggiorna top-K con i chunk di questo file ── */
        int sim_dim = (qdim < dim) ? qdim : dim; /* usa il minimo in caso di mismatch */
        for (int i = 0; i < nc; i++) {
            float score = _cos_sim(qvec, fvecs + (size_t)i * dim, sim_dim);
            if (top_n < RAG_EMBED_TOP_K) {
                memcpy(top[top_n].text, ftexts[i], RAG_EMBED_CHUNK_CHARS + 4);
                top[top_n].score = score;
                top_n++;
            } else {
                /* Sostituisce il chunk con score minimo se questo e' migliore */
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
    free(qvec);

    if (top_n == 0) return 0;

    /* ── Ordina top-K per score decrescente (bubble su K <= 3) ── */
    for (int i = 0; i < top_n - 1; i++)
        for (int j = i + 1; j < top_n; j++)
            if (top[j].score > top[i].score) {
                TopK tmp = top[i]; top[i] = top[j]; top[j] = tmp;
            }

    /* ── Concatena in out_ctx, scarta chunk con score nullo ── */
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
