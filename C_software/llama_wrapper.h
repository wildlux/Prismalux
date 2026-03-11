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

#ifdef __cplusplus
}
#endif
