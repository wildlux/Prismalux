/*
 * rag_embed.h — RAG con embedding semantici via Ollama
 * =====================================================
 * Motore alternativo al TF-overlap di rag.h: genera vettori semantici
 * tramite un modello embedding Ollama e trova i chunk piu' pertinenti
 * con cosine similarity.
 *
 * Richiede un modello embedding installato in Ollama:
 *   ollama pull nomic-embed-text   (consigliato, 274 MB)
 *   ollama pull all-minilm          (piu' leggero)
 *   ollama pull mxbai-embed-large   (alta qualita')
 *
 * Cache su disco: <docs_dir>/.rag_cache/<model>/<file>.emb
 *   Gli embedding vengono ricalcolati solo se il file sorgente cambia
 *   (confronto mtime). Il formato e' binario per lettura rapida.
 */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif

/* Dimensione chunk (piu' grande del TF-overlap per contesto semantico migliore) */
#define RAG_EMBED_CHUNK_CHARS  512

/* Chunk da restituire nel contesto */
#define RAG_EMBED_TOP_K        3

/* Dimensione massima vettore embedding (copre tutti i modelli comuni:
 *   nomic-embed-text:768, all-minilm:384, mxbai-embed-large:1024,
 *   bge-large:1024, text-embedding-3-large:3072, e1:4096) */
#define RAG_EMBED_MAX_DIM      4096

/* Magic number del file cache binario: "REMB" */
#define RAG_EMBED_CACHE_MAGIC  0x52454D42u

/*
 * rag_embed_detect_model — rileva il primo modello embedding disponibile.
 *
 * Interroga GET /api/tags e cerca modelli con "embed" nel nome.
 * Ritorna 1 se trovato (out_model riempito, max 64 byte), 0 altrimenti.
 */
int rag_embed_detect_model(const char *host, int port, char out_model[64]);

/*
 * rag_embed_query — recupera i chunk piu' rilevanti tramite cosine similarity.
 *
 * Stessa interfaccia di rag_query() ma usa embeddings semantici.
 * Se Ollama non e' disponibile o l'embedding fallisce, ritorna 0 senza crash.
 * La cache e' in <docs_dir>/.rag_cache/<model>/<file>.emb.
 *
 * Parametri:
 *   host/port     — Ollama server (di solito localhost:11434)
 *   embed_model   — modello embedding (da rag_embed_detect_model)
 *   docs_dir      — cartella con i file .txt da indicizzare
 *   query         — domanda dell'utente
 *   out_ctx       — buffer di output con i chunk selezionati
 *   out_max       — dimensione massima di out_ctx (usare RAG_CTX_MAX)
 *
 * Ritorna: numero di chunk trovati (0 se fallisce o nessun risultato).
 */
int rag_embed_query(const char *host, int port, const char *embed_model,
                    const char *docs_dir, const char *query,
                    char *out_ctx, int out_max);

#ifdef __cplusplus
}
#endif
