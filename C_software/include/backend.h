/*
 * backend.h — Tipi BackendCtx, profili, globals condivisi
 */
#pragma once

#include "common.h"
#include "hw_detect.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Tipo backend ───────────────────────────────────────────────── */
typedef enum { BACKEND_LLAMA = 0, BACKEND_OLLAMA = 1, BACKEND_LLAMASERVER = 2 } Backend;

typedef struct {
    Backend backend;
    char    host[64];
    int     port;
    char    model[128];  /* nome modello per HTTP backends; filename .gguf per llama */
} BackendCtx;

/* ── Globals definiti in backend.c ─────────────────────────────── */
extern BackendCtx g_ctx;
extern HWInfo     g_hw;
extern int        g_hw_ready;
extern char       g_hdr_backend[32];
extern char       g_hdr_model[80];
extern char       g_models_dir[512];

/* Profili per backend */
extern char g_prof_ollama_host[64];
extern int  g_prof_ollama_port;
extern char g_prof_ollama_model[128];
extern char g_prof_lserver_host[64];
extern int  g_prof_lserver_port;
extern char g_prof_lserver_model[128];
extern char g_prof_llama_model[256];

/* ── Funzioni ────────────────────────────────────────────────────── */
const char* backend_name(Backend b);
void        _sync_to_profile(void);
void        _sync_hdr_ctx(void);
int         scan_gguf_dir(const char* dir, char names[][256], int max);

#ifdef __cplusplus
}
#endif
