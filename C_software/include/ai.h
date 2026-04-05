/*
 * ai.h — Astrazione AI: ai_chat_stream, backend_ready
 * =====================================================
 * Questo modulo espone l'unico punto di ingresso per l'inferenza AI:
 * ai_chat_stream() che dispatcha automaticamente al backend configurato.
 * Contiene inoltre le funzioni di controllo pre-invocazione (backend pronto,
 * risorse RAM/VRAM sufficienti).
 */
#pragma once

#include "common.h"
#include "backend.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * stream_cb — callback di default per la stampa dei token in streaming.
 * Stampa ogni token ricevuto in verde (colore GRN) su stdout con fflush.
 * Usata come parametro cb quando non serve una callback personalizzata.
 * Parametri: piece=token di testo, udata=non usato (ignorato).
 */
void stream_cb(const char* piece, void* udata);

/*
 * ai_chat_stream — punto unico di dispatch per l'inferenza AI.
 * In base a ctx->backend chiama:
 *   BACKEND_LLAMA       → lw_chat() (solo se compilato con -DWITH_LLAMA_STATIC)
 *   BACKEND_OLLAMA      → http_ollama_stream()
 *   BACKEND_LLAMASERVER → http_llamaserver_stream()
 * Parametri:
 *   ctx    — backend e modello da usare
 *   sys_p  — system prompt (istruzioni al modello)
 *   usr    — messaggio dell'utente
 *   cb     — callback chiamata per ogni token (può essere NULL)
 *   ud     — userdata passato alla callback
 *   out    — buffer per accumulare la risposta completa (può essere NULL)
 *   outmax — dimensione di out
 * Ritorna: 0 se OK, -1 se errore o backend non disponibile.
 */
int  ai_chat_stream(const BackendCtx* ctx,
                    const char* sys_p, const char* usr,
                    lw_token_cb cb, void* ud,
                    char* out, int outmax);

/*
 * backend_ready — verifica se il backend è pronto per l'inferenza.
 * Per BACKEND_LLAMA: controlla che lw_is_loaded() sia 1 (modello caricato).
 * Per gli HTTP backend: controlla che ctx->model non sia vuoto.
 * Ritorna: 1 se pronto, 0 se non configurato.
 */
int  backend_ready(const BackendCtx* ctx);

/*
 * ensure_ready_or_return — mostra un avviso se il backend non è pronto.
 * Stampa un messaggio giallo con istruzioni e aspetta un tasto.
 * Non modifica il flusso di controllo (il chiamante deve decidere se tornare).
 */
void ensure_ready_or_return(const BackendCtx* ctx);

/*
 * check_risorse_ok — controlla RAM e VRAM prima di avviare operazioni pesanti.
 * Legge l'utilizzo corrente con get_resources() e applica due soglie:
 *   >= 92%: avviso critico con richiesta esplicita di conferma [s/N]
 *   >= 80%: avviso giallo non bloccante, INVIO continua oppure 0 annulla
 * Ritorna: 1 se l'utente ha confermato o le risorse sono ok, 0 se ha annullato.
 */
int  check_risorse_ok(void);

#ifdef __cplusplus
}
#endif
