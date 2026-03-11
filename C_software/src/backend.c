/*
 * backend.c — SEZIONE 1: BackendCtx, profili globali, sync, scan_gguf_dir
 */
#include "common.h"
#include "backend.h"

/* ══════════════════════════════════════════════════════════════
   SEZIONE 1 — Backend types + globals
   ══════════════════════════════════════════════════════════════ */

/* Contesto globale (modificato da --backend / menu) */
BackendCtx g_ctx = { BACKEND_LLAMA, "127.0.0.1", 11434, "" };

/* ── Profili per backend (conservano le ultime impostazioni) ─── */
char g_prof_ollama_host[64]    = "127.0.0.1";
int  g_prof_ollama_port        = 11434;
char g_prof_ollama_model[128]  = "";
char g_prof_lserver_host[64]   = "127.0.0.1";
int  g_prof_lserver_port       = 8080;
char g_prof_lserver_model[128] = "";
char g_prof_llama_model[256]   = "";

/* Directory modelli .gguf */
char g_models_dir[512] = MODELS_DIR;

/* Hardware rilevato all'avvio (extern in prismalux_ui.h per print_header) */
HWInfo g_hw;
int    g_hw_ready = 0;

/* Backend+modello per print_header() — aggiornati da _sync_hdr_ctx() */
char g_hdr_backend[32] = "";
char g_hdr_model[80]   = "";

/* ── backend_name ────────────────────────────────────────────── */
const char* backend_name(Backend b) {
    switch(b) {
        case BACKEND_LLAMA:       return "llama-statico";
        case BACKEND_OLLAMA:      return "Ollama";
        case BACKEND_LLAMASERVER: return "llama-server";
    }
    return "?";
}

/* Salva g_ctx nel profilo del backend corrente */
void _sync_to_profile(void) {
    if (g_ctx.backend == BACKEND_OLLAMA) {
        strncpy(g_prof_ollama_host,  g_ctx.host,  sizeof g_prof_ollama_host  - 1);
        g_prof_ollama_port = g_ctx.port;
        strncpy(g_prof_ollama_model, g_ctx.model, sizeof g_prof_ollama_model - 1);
    } else if (g_ctx.backend == BACKEND_LLAMASERVER) {
        strncpy(g_prof_lserver_host,  g_ctx.host,  sizeof g_prof_lserver_host  - 1);
        g_prof_lserver_port = g_ctx.port;
        strncpy(g_prof_lserver_model, g_ctx.model, sizeof g_prof_lserver_model - 1);
    } else {
        const char *m = lw_is_loaded() ? lw_model_name() : g_ctx.model;
        if (m && m[0]) strncpy(g_prof_llama_model, m, sizeof g_prof_llama_model - 1);
    }
}

/* Aggiorna g_hdr_backend / g_hdr_model per il nuovo header compatto */
void _sync_hdr_ctx(void) {
    strncpy(g_hdr_backend, backend_name(g_ctx.backend), sizeof g_hdr_backend - 1);
    g_hdr_backend[sizeof g_hdr_backend - 1] = '\0';
    const char *m = (g_ctx.backend == BACKEND_LLAMA)
        ? (lw_is_loaded() ? lw_model_name() : "nessun modello")
        : (g_ctx.model[0] ? g_ctx.model : "nessun modello");
    strncpy(g_hdr_model, m, sizeof g_hdr_model - 1);
    g_hdr_model[sizeof g_hdr_model - 1] = '\0';
}

/* ── Scanner .gguf (non dipende da llama stub) ──────────────── */
int scan_gguf_dir(const char* dir, char names[][256], int max) {
    int count = 0;
#ifdef _WIN32
    char pattern[600];
    snprintf(pattern, sizeof pattern, "%s\\*.gguf", dir);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        strncpy(names[count], fd.cFileName, 255);
        names[count][255] = '\0';
        count++;
    } while (FindNextFileA(h, &fd) && count < max);
    FindClose(h);
#else
    DIR* d = opendir(dir);
    if (!d) return 0;
    struct dirent* e;
    while ((e = readdir(d)) != NULL && count < max) {
        size_t len = strlen(e->d_name);
        if (len > 5 && strcmp(e->d_name + len - 5, ".gguf") == 0) {
            strncpy(names[count], e->d_name, 255);
            names[count][255] = '\0';
            count++;
        }
    }
    closedir(d);
#endif
    return count;
}
