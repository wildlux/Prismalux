/*
 * ai.c — SEZIONE 6: Astrazione AI, stream_cb, backend_ready
 */
#include "common.h"
#include "backend.h"
#include "http.h"
#include "ai.h"
#include "prismalux_ui.h"
#include "terminal.h"

/* Calcola keep_alive_secs in base alla RAM libera.
 * RAM libera > 50% del totale → -1 (default Ollama 5m, modello resta in RAM).
 * RAM libera 20-50%           →  60 (1 minuto, poi scarica).
 * RAM libera < 20%            →   0 (scarica subito dopo la risposta).
 * Questo evita che modelli pesanti blocchino il sistema quando l'utente
 * apre altri programmi dopo aver usato l'AI. */
static int _smart_keep_alive(void) {
    SysRes r = get_resources();
    if(r.ram_total <= 0) return -1;  /* non rilevabile: usa default */
    double free_pct = (r.ram_total - r.ram_used) / r.ram_total * 100.0;
    if(free_pct > 50.0) return -1;  /* RAM abbondante: mantieni in RAM */
    if(free_pct > 20.0) return 60;  /* RAM media: scarica dopo 1 minuto */
    return 0;                        /* RAM scarsa: scarica subito */
}

/* ══════════════════════════════════════════════════════════════
   SEZIONE 6 — Astrazione AI: ai_chat_stream / ai_chat
   ══════════════════════════════════════════════════════════════ */

void stream_cb(const char* piece, void* udata) {
    (void)udata; printf(GRN "%s" RST, piece); fflush(stdout);
}

/* Genera risposta (streaming) usando il BackendCtx fornito */
int ai_chat_stream(const BackendCtx* ctx,
                   const char* sys_p, const char* usr,
                   lw_token_cb cb, void* ud,
                   char* out, int outmax) {
    switch(ctx->backend) {
        case BACKEND_LLAMA:
#ifdef WITH_LLAMA_STATIC
            return lw_chat(sys_p, usr, cb, ud, out, outmax);
#else
            printf(RED "  [llama statico non disponibile — esegui ./build.sh]\n" RST);
            return -1;
#endif
        case BACKEND_OLLAMA:
            return http_ollama_stream(ctx->host, ctx->port, ctx->model,
                                      sys_p, usr, cb, ud, out, outmax,
                                      _smart_keep_alive());
        case BACKEND_LLAMASERVER:
            return http_llamaserver_stream(ctx->host, ctx->port, ctx->model,
                                           sys_p, usr, cb, ud, out, outmax);
    }
    return -1;
}

/* Backend ready? (model caricato per llama, model name impostato per HTTP) */
int backend_ready(const BackendCtx* ctx) {
    switch(ctx->backend) {
        case BACKEND_LLAMA:       return lw_is_loaded();
        case BACKEND_OLLAMA:
        case BACKEND_LLAMASERVER: return ctx->model[0] != '\0';
    }
    return 0;
}

static void ensure_ready_or_return_impl(const BackendCtx* ctx) {
    /* solo stampa avviso, il chiamante decide se tornare */
    if(!backend_ready(ctx)){
        printf(YLW "\n  \xe2\x9a\xa0  Backend non pronto.\n" RST);
        printf("  Usa Opzione 5 \xe2\x86\x92 Modello/Backend per configurarlo.\n");
        printf(GRY "  Premi un tasto...\n" RST); getch_single();
    }
}

void ensure_ready_or_return(const BackendCtx* ctx) {
    ensure_ready_or_return_impl(ctx);
}

/* ══════════════════════════════════════════════════════════════
   Controllo risorse prima di operazioni pesanti
   Ritorna 1 = ok procedere, 0 = utente ha annullato
   ══════════════════════════════════════════════════════════════ */
int check_risorse_ok(void) {
    SysRes r = get_resources();
    double rp = (r.ram_total>0) ? r.ram_used/r.ram_total*100 : 0;
    double vp = (r.vram_total>0.01) ? r.vram_used/r.vram_total*100 : 0;

    /* Nessun problema */
    if(rp < 80 && (vp < 80 || r.vram_total < 0.01)) return 1;

    clear_screen(); print_header();

    int crit = (rp >= 92) || (vp >= 92 && r.vram_total > 0.01);

    if(crit){
        printf(RED BLD "\n  \xe2\x9b\x94  ATTENZIONE: risorse critiche!\n\n" RST);
        if(rp >= 92)
            printf(RED "  RAM: %.1f/%.1f GB (%.0f%%) \xe2\x80\x94 quasi piena\n" RST,
                   r.ram_used, r.ram_total, rp);
        if(vp >= 92 && r.vram_total > 0.01)
            printf(RED "  VRAM: %.1f/%.1f GB (%.0f%%) \xe2\x80\x94 quasi piena\n" RST,
                   r.vram_used, r.vram_total, vp);
        printf(RED "\n  Avviare un modello AI ora potrebbe bloccare il sistema.\n" RST);
        printf(YLW "  Chiudi altri programmi prima di continuare.\n\n" RST);
        printf("  Vuoi procedere lo stesso? [s/N] " RST); fflush(stdout);
        char yn[8]; if(!fgets(yn,sizeof yn,stdin)) return 0;
        return (yn[0]=='s'||yn[0]=='S') ? 1 : 0;
    } else {
        /* Solo avviso giallo, non bloccante */
        printf(YLW "\n  \xe2\x9a\xa0  Risorse alte \xe2\x80\x94 il modello potrebbe caricarsi lentamente.\n\n" RST);
        if(rp >= 80)
            printf(YLW "  RAM: %.1f/%.1f GB (%.0f%%)\n" RST,
                   r.ram_used, r.ram_total, rp);
        if(vp >= 80 && r.vram_total > 0.01)
            printf(YLW "  VRAM: %.1f/%.1f GB (%.0f%%)\n" RST,
                   r.vram_used, r.vram_total, vp);
        printf(GRY "\n  Premi INVIO per continuare, " EM_0 " per annullare: " RST);
        fflush(stdout);
        char yn[8]; if(!fgets(yn,sizeof yn,stdin)) return 0;
        return (yn[0]=='0') ? 0 : 1;
    }
}
