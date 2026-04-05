/*
 * llama_wrapper.h — Interfaccia C per il wrapper C++ su llama.cpp
 * ================================================================
 * Disponibile SOLO quando si compila con -DWITH_LLAMA_STATIC
 * (richiede ./build.sh per clonare e compilare llama.cpp).
 *
 * Senza -DWITH_LLAMA_STATIC, tutti i simboli lw_* sono sostituiti
 * da stub no-op inline definiti in include/common.h — in questo modo
 * il codice compila e gira con i backend HTTP (Ollama / llama-server)
 * senza richiedere la presenza di llama.cpp.
 *
 * Parametri interni del wrapper (src/llama_wrapper.cpp):
 *   - flash_attn  = true          → riduce VRAM, accelera l'inferenza
 *   - type_k/v   = GGML_TYPE_Q8_0 → cache KV compressa
 *   - n_threads  = nCPU / 2       → solo core fisici (no hyper-threading)
 *   - Sampler    : top_k=40, top_p=0.90, temp=0.30, repeat_penalty=1.2
 *   - llama_memory_clear() prima di ogni lw_chat() → contesto azzerato
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Callback chiamata per ogni token generato */
typedef void (*lw_token_cb)(const char* piece, void* udata);

/* Inizializza con un modello .gguf
 * n_gpu_layers: 0=solo CPU, 99=tutto GPU
 * n_ctx: dimensione contesto (es. 4096)
 * Ritorna 0=ok, -1=errore */
int  lw_init(const char* model_path, int n_gpu_layers, int n_ctx);

/* Libera risorse */
void lw_free(void);

/* Genera risposta (streaming via callback)
 * out_buf: buffer per la risposta completa (può essere NULL)
 * out_max: dimensione out_buf
 * Ritorna 0=ok, -1=errore */
int lw_chat(const char* system_prompt,
            const char* user_message,
            lw_token_cb callback,
            void*       udata,
            char*       out_buf,
            int         out_max);

/* Elenca file .gguf nella directory
 * Riempie names[][256] con i nomi dei file
 * Ritorna il numero di file trovati */
int lw_list_models(const char* dir, char names[][256], int max_names);

/* Ritorna 1 se il modello è caricato */
int lw_is_loaded(void);

/* Nome breve del modello caricato (es. "mistral-7b-q4.gguf") */
const char* lw_model_name(void);

/* ── JLT KV-cache Advisor ────────────────────────────────────────
 * Suggerisce il valore ottimale di n_ctx per ridurre la RAM del KV
 * cache di llama.cpp, basandosi sulla dimensione del corpus RAG.
 *
 * Usa jlt_index_kv_ctx() se un indice JLT è disponibile,
 * altrimenti ritorna il default_ctx passato.
 *
 * Esempio:
 *   int kv = lw_jlt_advised_ctx(idx, 4096, 200, 150, 20);
 *   lw_init(model_path, n_gpu_layers, kv);
 *
 * idx             : JltIndex costruito con jlt_index_build()
 *                   (può essere NULL → ritorna default_ctx)
 * default_ctx     : valore di fallback se idx == NULL
 * sys_toks        : token stimati del system prompt
 * user_toks       : token stimati della domanda utente
 * headroom_pct    : margine % extra (es. 20)
 */
#include "jlt_index.h"
int lw_jlt_advised_ctx(const JltIndex *idx,
                        int default_ctx,
                        int sys_toks,
                        int user_toks,
                        int headroom_pct);

#ifdef __cplusplus
}
#endif
