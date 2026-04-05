/*
 * jlt_rag.h — RAG accelerato con Johnson–Lindenstrauss Transform
 * ===============================================================
 * Applica la JLT (variante SRHT, §1.4.2 di arXiv:2103.00564) agli
 * embedding Ollama prima di memorizzarli e prima della cosine similarity.
 *
 * Vantaggio concreto (Corollary 1.5 del paper):
 *   La JLT preserva i prodotti scalari (e quindi la cosine similarity)
 *   con errore ≤ ε con alta probabilità — se usiamo la STESSA matrice
 *   per tutti i vettori (documenti e query).
 *
 * Riduzione dimensionale tipica:
 *   nomic-embed-text  768D  →  192D   (4× più veloce, 4× meno memoria)
 *   mxbai-embed-large 1024D →  256D   (4× più veloce)
 *   text-embedding-3  3072D →  512D   (6× più veloce)
 *
 * Il seme della SRHT è derivato dal nome del modello embedding:
 * stesso modello → stessa matrice → similarità coerenti tra sessioni.
 *
 * Formato cache compressa (magic diverso da rag_embed.c):
 *   JltCacheHdr (24 byte) + per ogni chunk:
 *     uint32_t crc32 | uint32_t text_len | char text[text_len]
 *     float vec[m_dim]   ← dimensione COMPRESSA (non originale)
 */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "jlt.h"

/* ── Parametri ottimizzazione ─────────────────────────────────── */

/* Distorsione ε: 0.20 = buon compromesso qualità/velocità (§1.2) */
#define JLT_RAG_EPSILON     0.20

/* Probabilità fallimento δ: 0.05 = 95% garanzia per singola query */
#define JLT_RAG_DELTA       0.05

/* Dimensione minima dopo compressione (sotto non ha senso JLT) */
#define JLT_RAG_MIN_DIM     64

/* Dimensione massima target anche se il modello è gigante */
#define JLT_RAG_MAX_DIM     512

/* Magic number cache JLT: "JEMB" */
#define JLT_RAG_CACHE_MAGIC 0x4A454D42u

/* ── Struttura di stato JLT per il RAG ───────────────────────── */
typedef struct {
    JLTContext *ctx;       /* SRHT inizializzata con d_orig e m_dim  */
    int         d_orig;    /* dimensione embedding originale          */
    int         m_dim;     /* dimensione compressa                    */
    uint64_t    seed;      /* seme derivato dal modello               */
    double      ratio;     /* m_dim / d_orig (< 1.0 = compressione)  */
} JltRagState;

/* ── API pubblica ─────────────────────────────────────────────── */

/*
 * jlt_rag_init — crea lo stato JLT per un embedding di dimensione d_orig.
 *
 *   model_name : nome del modello Ollama (usato per derivare il seme)
 *   d_orig     : dimensione vettore embedding (rilevata dal primo embed)
 *   n_chunks   : numero totale di chunk nel corpus (usato per la formula m)
 *
 * Ritorna un JltRagState allocato, oppure NULL in caso di errore.
 * Liberare con jlt_rag_free().
 */
JltRagState *jlt_rag_init(const char *model_name, int d_orig, int n_chunks);

/*
 * jlt_rag_compress — applica la SRHT a un vettore float d_orig → m_dim.
 *
 *   in  : vettore sorgente di lunghezza d_orig
 *   out : vettore destinazione di lunghezza m_dim (pre-allocato)
 */
void jlt_rag_compress(const JltRagState *state,
                      const float *in, float *out);

/*
 * jlt_rag_cosine — cosine similarity tra due vettori compressi (lunghezza m_dim).
 */
float jlt_rag_cosine(const float *a, const float *b, int m_dim);

/*
 * jlt_rag_free — libera lo stato JLT.
 */
void jlt_rag_free(JltRagState *state);

/*
 * jlt_rag_query — versione JLT-accelerata di rag_embed_query().
 *
 * Drop-in replacement: stessa firma di rag_embed_query(), più un parametro
 * opzionale stats_out (può essere NULL) per ricevere statistiche.
 *
 * Cache: usa un magic diverso da rag_embed.c — se trova cache non-JLT
 * la ignora e ricalcola con compressione.
 */
typedef struct {
    int   d_orig;          /* dimensione embedding originale          */
    int   m_compressed;    /* dimensione dopo JLT                     */
    int   n_chunks_total;  /* chunk indicizzati totali                */
    int   cache_hits;      /* chunk caricati dalla cache JLT          */
    double compress_ratio; /* m/d (< 1.0)                             */
    double speedup_est;    /* stima speedup similarity (d/m)          */
} JltRagStats;

int jlt_rag_query(const char *host, int port, const char *embed_model,
                  const char *docs_dir, const char *query,
                  char *out_ctx, int out_max,
                  JltRagStats *stats_out);   /* NULL = non stampare */

/*
 * jlt_rag_print_stats — stampa le statistiche su stdout.
 */
void jlt_rag_print_stats(const JltRagStats *s);

#ifdef __cplusplus
}
#endif
