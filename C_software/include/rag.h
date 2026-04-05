/*
 * rag.h — Retrieval-Augmented Generation (RAG) locale
 * =====================================================
 * Recupero di testo rilevante da file .txt locali prima di chiamare
 * ai_chat_stream(). Nessuna dipendenza esterna: usa term-frequency
 * overlap (TF matching) per trovare i chunk piu' pertinenti alla query.
 *
 * Flusso:
 *   1. rag_query() carica tutti i .txt in docs_dir
 *   2. Divide ogni file in chunk da ~RAG_CHUNK_CHARS caratteri
 *   3. Assegna punteggio: quante parole della query compaiono nel chunk
 *   4. Ritorna i top-RAG_TOP_K chunk concatenati in out_ctx
 *   5. Il chiamante inietta out_ctx nel system prompt prima di ai_chat_stream()
 *
 * Convenzione directory:
 *   rag_docs/730/   <- documenti per l'assistente 730
 *   rag_docs/piva/  <- documenti per l'assistente P.IVA
 */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif

/* ── Modalita' RAG: tre motori a confronto ─────────────────────── */
#define RAG_MODE_TF    0   /* TF-overlap: veloce, zero dipendenze esterne  */
#define RAG_MODE_EMBED 1   /* Embedding semantico: richiede modello Ollama  */
#define RAG_MODE_AUTO  2   /* Preferisce EMBED se disponibile, fallback TF  */
#define RAG_MODE_JLT   3   /* JLT-SRHT compressa (arXiv:2103.00564): 4-6x  */
                           /* più veloce di EMBED, stessa qualità semantica  */

/*
 * rag_get_mode / rag_set_mode — legge/imposta la modalita' corrente.
 * Usati dal benchmark e dal menu Impostazioni per salvare la scelta.
 */
int  rag_get_mode(void);
void rag_set_mode(int mode);

/* Dimensione massima di un chunk (in caratteri) */
#define RAG_CHUNK_CHARS 350

/* Numero massimo di chunk da restituire */
#define RAG_TOP_K 3

/* Lunghezza massima del contesto RAG iniettato nel prompt */
#define RAG_CTX_MAX 1600

/*
 * rag_query — recupera i chunk piu' rilevanti per query da docs_dir.
 *
 * Parametri:
 *   docs_dir  — percorso della cartella con i file .txt
 *   query     — domanda dell'utente (usata per il matching)
 *   out_ctx   — buffer di output con i chunk selezionati
 *   out_max   — dimensione massima di out_ctx (usare RAG_CTX_MAX)
 *
 * Ritorna:
 *   Numero di chunk trovati (0 se docs_dir vuota o assente).
 *   out_ctx e' vuoto se ritorna 0.
 */
int rag_query(const char *docs_dir, const char *query,
              char *out_ctx, int out_max);

/*
 * rag_build_prompt — costruisce il system prompt con contesto RAG iniettato.
 *
 * Se rag_ctx e' non vuoto, lo antepone a base_sys come:
 *   "CONTESTO DOCUMENTALE:\n<rag_ctx>\n---\n<base_sys>"
 * Se rag_ctx e' vuoto, copia solo base_sys in out.
 */
void rag_build_prompt(const char *base_sys, const char *rag_ctx,
                      char *out, int out_max);

#ifdef __cplusplus
}
#endif
