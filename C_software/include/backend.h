/*
 * backend.h — Tipi BackendCtx, profili, globals condivisi
 * =========================================================
 * Definisce il tipo BackendCtx (backend attivo + host/porta/modello),
 * la enum Backend con i tre backend supportati, e le variabili globali
 * condivise da tutti i moduli. I profili conservano le ultime impostazioni
 * usate per ogni backend, così da ripristinarle quando si torna a quel backend.
 */
#pragma once

#include "common.h"
#include "hw_detect.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Tipo backend ───────────────────────────────────────────────── */
/* I tre backend AI supportati a runtime */
typedef enum {
    BACKEND_LLAMA       = 0,  /* llama.cpp statico (libllama, zero server) */
    BACKEND_OLLAMA      = 1,  /* Ollama via HTTP NDJSON (default porta 11434) */
    BACKEND_LLAMASERVER = 2   /* llama-server via HTTP OpenAI SSE (default porta 8080) */
} Backend;

/*
 * BackendCtx — contesto del backend attivo.
 * Viene passato a ai_chat_stream() per ogni chiamata AI.
 */
typedef struct {
    Backend backend;       /* quale backend usare */
    char    host[64];      /* hostname o IP del server HTTP */
    int     port;          /* porta del server HTTP */
    char    model[128];    /* nome modello per HTTP backends; filename .gguf per llama */
} BackendCtx;

/* ── Globals definiti in backend.c ─────────────────────────────── */
extern BackendCtx g_ctx;        /* contesto backend attivo (modificato da menu e --backend) */
extern HWInfo     g_hw;         /* snapshot hardware rilevato all'avvio da hw_detect() */
extern int        g_hw_ready;   /* 1 dopo che hw_detect() ha completato, 0 prima */
extern char       g_hdr_backend[32]; /* nome backend per print_header() — aggiornato da _sync_hdr_ctx() */
extern char       g_hdr_model[80];   /* nome modello per print_header() — aggiornato da _sync_hdr_ctx() */
extern char       g_models_dir[512]; /* directory .gguf (default "models", override con --models-dir) */
extern char       g_gui_path[512];   /* percorso GUI Qt6 (default "Qt_GUI/build/Prismalux_GUI", override con --gui-path o config) */
extern char       g_config_fmt[8];   /* formato config: "json" (default) o "toon" */

/*
 * Profili per backend — conservano host/porta/modello dell'ultima
 * sessione per ogni backend. Quando l'utente cambia backend, il profilo
 * precedente viene salvato da _sync_to_profile() e ripristinato al ritorno.
 */
extern char g_prof_ollama_host[64];    /* ultimo host Ollama usato */
extern int  g_prof_ollama_port;        /* ultima porta Ollama usata */
extern char g_prof_ollama_model[128];  /* ultimo modello Ollama selezionato */
extern char g_prof_lserver_host[64];   /* ultimo host llama-server usato */
extern int  g_prof_lserver_port;       /* ultima porta llama-server usata */
extern char g_prof_lserver_model[128]; /* ultimo modello llama-server selezionato */
extern char g_prof_llama_model[256];   /* ultimo file .gguf caricato con llama statico */

/* ── Funzioni ────────────────────────────────────────────────────── */
/* Restituisce la stringa leggibile del backend (es. "Ollama", "llama-server") */
const char* backend_name(Backend b);
/* Copia g_ctx nel profilo del backend corrente (da chiamare prima di ogni cambio) */
void        _sync_to_profile(void);
/* Aggiorna g_hdr_backend e g_hdr_model usati da print_header() */
void        _sync_hdr_ctx(void);
/* Scansiona la directory dir cercando file .gguf; riempie names[] e ritorna il conteggio */
int         scan_gguf_dir(const char* dir, char names[][256], int max);

#ifdef __cplusplus
}
#endif
