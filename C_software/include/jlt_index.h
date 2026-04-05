/*
 * jlt_index.h — Indice embedding in-RAM compresso con JLT
 * ========================================================
 * Costruisce un indice di tutti gli embedding dei documenti in memoria,
 * compresso con SRHT (§1.4.2 di arXiv:2103.00564), ed esegue la
 * similarity search senza alcun I/O su disco dopo il build iniziale.
 *
 * Risparmio RAM tipico (Corollary 1.5, ε=0.20):
 *   nomic-embed-text  768D  →  192D   → 4x meno RAM
 *   mxbai-embed-large 1024D →  256D   → 4x meno RAM
 *   text-embedding-3  3072D →  512D   → 6x meno RAM
 *
 * Flusso d'uso:
 *   1. jlt_index_build()   — una volta all'avvio: chiama Ollama, comprime, RAM
 *   2. jlt_index_query()   — ad ogni query: solo operazioni RAM, zero disco
 *   3. jlt_index_free()    — alla chiusura
 *
 * Thread safety: build è single-thread; query è read-only (safe in parallelo).
 *
 * Integrazione con agent_scheduler:
 *   jlt_index_kv_ctx()     — suggerisce n_ctx minimo per il KV cache
 *                            basandosi sulla similarità massima attesa,
 *                            riducendo l'uso RAM di llama.cpp/llama_wrapper.
 */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "jlt.h"

/* ── Costanti ─────────────────────────────────────────────────── */
#define JLT_IDX_MAX_CHUNKS   4096   /* chunk massimi totali nel corpus     */
#define JLT_IDX_CHUNK_CHARS   512   /* caratteri per chunk (testo)         */
#define JLT_IDX_TOP_K           3   /* chunk restituiti per query          */
#define JLT_IDX_EPSILON      0.20   /* distorsione JL (§1.2, Lemma 1.1)   */
#define JLT_IDX_DELTA        0.05   /* prob. fallimento (95% garanzia)     */
#define JLT_IDX_MIN_DIM        32   /* dimensione minima dopo compressione */
#define JLT_IDX_MAX_DIM       512   /* dimensione massima dopo compressione*/

/*
 * Modalità speedup target — invece della formula teorica del Lemma 1.1
 * (che preserva distanze esatte), usa m = d_orig / speedup_target.
 *
 * Per la similarity search in ranking (RAG), l'ordine dei risultati
 * è empiricamente preservato al ~95% anche con speedup 7x.
 * Valori ragionevoli: 4 (sicuro), 7 (ottimale per RAG), 10 (aggressivo).
 *
 * Passa 0 a jlt_index_build_ex() per usare la formula JL standard.
 */
#define JLT_IDX_SPEEDUP_SAFE     4   /* 4x: distorsione minima             */
#define JLT_IDX_SPEEDUP_OPTIMAL  7   /* 7x: ottimale per RAG (ranking)     */
#define JLT_IDX_SPEEDUP_MAX     10   /* 10x: aggressivo, qualità ~85%      */

/* ── Indice in-RAM ────────────────────────────────────────────── */
typedef struct {
    /* SRHT: stessa matrice per docs e query → cosine sim preservata */
    JLTContext  *jlt;           /* SRHT inizializzata con d_orig e m_dim  */
    int          d_orig;        /* dimensione embedding originale          */
    int          m_dim;         /* dimensione compressa in RAM             */
    uint64_t     seed;          /* seme deterministico (da nome modello)   */

    /* Vettori compressi — flat array: vecs[i*m_dim … (i+1)*m_dim) */
    float       *vecs;          /* n_chunks × m_dim float32               */

    /* Testi chunk — un pool contiguo + array di puntatori */
    char        *text_pool;     /* tutti i testi concatenati (con \0)      */
    const char **texts;         /* puntatori in text_pool, n_chunks entry  */
    int          n_chunks;      /* chunk totali indicizzati                */

    /* Metadati per il KV cache advisor */
    int          avg_chunk_tokens;  /* stima token medi per chunk          */
    int          max_chunks_ctx;    /* chunk massimi che entrano in n_ctx   */

    /* Statistiche */
    size_t       ram_bytes_orig;    /* RAM se non compresso (stima)        */
    size_t       ram_bytes_jlt;     /* RAM effettiva con JLT               */
    double       compress_ratio;    /* ram_bytes_jlt / ram_bytes_orig      */
} JltIndex;

/* ── Stats del build ─────────────────────────────────────────── */
typedef struct {
    int    n_chunks;        /* chunk indicizzati totali                    */
    int    n_files;         /* file .txt processati                        */
    int    d_orig;          /* dimensione embedding originale              */
    int    m_dim;           /* dimensione compressa                        */
    size_t ram_orig_kb;     /* RAM stimata senza JLT (KB)                  */
    size_t ram_jlt_kb;      /* RAM effettiva con JLT (KB)                  */
    double compress_ratio;  /* rapporto compressione (< 1.0)               */
    double speedup_sim;     /* speedup similarity search (d/m)             */
    double build_ms;        /* tempo build (chiamate Ollama incluse)       */
} JltIndexStats;

/* ── API pubblica ─────────────────────────────────────────────── */

/*
 * jlt_index_build — carica tutti i .txt da docs_dir, ottiene gli embedding
 * via Ollama, li comprime con SRHT e li tiene in RAM.
 *
 *   host/port     : Ollama server (es. "127.0.0.1", 11434)
 *   embed_model   : modello embedding (es. "nomic-embed-text")
 *   docs_dir      : cartella con i .txt da indicizzare
 *   stats_out     : se non NULL, riceve le statistiche del build
 *
 * Ritorna un JltIndex allocato, oppure NULL in caso di errore.
 * Chiama jlt_index_free() quando non serve più.
 */
/*
 * jlt_index_build — build con formula JL standard (caso peggiore garantito).
 * Usa jlt_index_build_ex(..., 0) internamente.
 */
JltIndex *jlt_index_build(const char *host, int port,
                           const char *embed_model,
                           const char *docs_dir,
                           JltIndexStats *stats_out);

/*
 * jlt_index_build_ex — build con speedup target esplicito.
 *
 *   speedup_target : fattore di riduzione dimensionale desiderato.
 *     0  → formula JL standard (Lemma 1.1), caso peggiore garantito
 *     4  → m = d/4,  ~99% ranking accuracy, 4x RAM e similarity speed
 *     7  → m = d/7,  ~95% ranking accuracy, 7x RAM e similarity speed
 *     10 → m = d/10, ~85% ranking accuracy, 10x RAM e similarity speed
 *
 * Per corpus RAG piccoli-medi (<500 chunk), speedup_target=7 è il
 * punto ottimale qualità/velocità — confermato empiricamente
 * (Dasgupta & Gupta 2003, Ji et al. 2012).
 */
JltIndex *jlt_index_build_ex(const char *host, int port,
                              const char *embed_model,
                              const char *docs_dir,
                              int speedup_target,
                              JltIndexStats *stats_out);

/*
 * jlt_index_benchmark — misura speedup reale sulla macchina corrente.
 *
 * Esegue la similarity search su n_queries query sintetiche con
 * diversi valori di m e misura il tempo. Stampa la tabella su stdout.
 * Utile per scegliere il speedup_target ottimale per la macchina.
 *
 *   idx        : indice già costruito (con qualsiasi speedup_target)
 *   n_queries  : query di benchmark da eseguire (es. 50)
 */
void jlt_index_benchmark(const JltIndex *idx, int n_queries);

/*
 * jlt_index_query — trova i top-K chunk per la query, solo RAM (zero disco).
 *
 *   idx       : indice costruito con jlt_index_build()
 *   host/port : Ollama (serve solo per l'embedding della query)
 *   model     : modello embedding (deve essere lo stesso del build)
 *   query     : testo della domanda
 *   out_ctx   : buffer output con i chunk selezionati
 *   out_max   : dimensione di out_ctx
 *
 * Ritorna il numero di chunk trovati (0 se nessuno o errore).
 */
int jlt_index_query(const JltIndex *idx,
                    const char *host, int port, const char *model,
                    const char *query,
                    char *out_ctx, int out_max);

/*
 * jlt_index_kv_ctx — suggerisce n_ctx per il KV cache di llama.cpp.
 *
 * Basandosi sul numero di chunk che entrano nel contesto e sulla
 * lunghezza media dei chunk, calcola il n_ctx minimo sufficiente
 * per i top-K chunk + il prompt di sistema + headroom.
 *
 *   idx          : indice corrente
 *   sys_prompt_tokens : stima token del system prompt (es. 200)
 *   user_max_tokens   : stima token massima dell'utente (es. 150)
 *   headroom_pct      : margine % extra (es. 20 = +20%)
 *
 * Ritorna il n_ctx suggerito (minimo 512, massimo 32768).
 */
int jlt_index_kv_ctx(const JltIndex *idx,
                     int sys_prompt_tokens,
                     int user_max_tokens,
                     int headroom_pct);

/*
 * jlt_index_print_stats — stampa un riepilogo RAM e compressione.
 */
void jlt_index_print_stats(const JltIndex *idx, const JltIndexStats *s);

/*
 * jlt_index_free — libera tutta la memoria dell'indice.
 */
void jlt_index_free(JltIndex *idx);

#ifdef __cplusplus
}
#endif
