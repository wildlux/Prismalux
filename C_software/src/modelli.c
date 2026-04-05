/*
 * modelli.c — SEZIONE 8: Selezione backend, modello, gestione .gguf
 * ================================================================
 * load_gguf_model, choose_backend, choose_model, gestisci_modelli,
 * menu_modelli, helper RAM/VRAM live, estimate_total_layers
 */
#include "common.h"
#include "backend.h"
#include "terminal.h"
#include "http.h"
#include "modelli.h"
#include "prismalux_ui.h"

/* Forward declaration (definita in main.c) */
void salva_config(void);

/* ══════════════════════════════════════════════════════════════
   SEZIONE 8 — Selezione backend e modello
   ══════════════════════════════════════════════════════════════ */

/* RAM disponibile in MB (lettura live, non dalla cache hw_detect) */
static long long ram_available_mb_live(void) {
#ifdef _WIN32
    MEMORYSTATUSEX ms; ms.dwLength = sizeof ms;
    GlobalMemoryStatusEx(&ms);
    return (long long)(ms.ullAvailPhys / (1024ULL * 1024ULL));
#else
    FILE* f = fopen("/proc/meminfo", "r");
    if (!f) return 0;
    char line[256]; long long avail = 0;
    while (fgets(line, sizeof line, f)) {
        unsigned long long v;
        if (sscanf(line, "MemAvailable: %llu", &v) == 1) { avail = (long long)(v / 1024); break; }
    }
    fclose(f);
    return avail;
#endif
}

/* Stima numero layer totali da dimensione file .gguf (euristica) */
static int estimate_total_layers(long long file_mb) {
    if (file_mb <  8192) return 32;   /* ~7B  (Q2-Q8) */
    if (file_mb < 14336) return 40;   /* ~13B         */
    if (file_mb < 28672) return 48;   /* ~34B         */
    if (file_mb < 49152) return 80;   /* ~70B         */
    return 120;                        /* >70B         */
}

/* ── Controlla file .gguf: dimensione minima e download in corso ────────
 * Ritorna 0=ok (file_sz aggiornato), -1=skip (messaggio già stampato).  */
static int _lgm_check_file(const char* path, long long* out_sz) {
    *out_sz = 0;
    FILE* tf = fopen(path, "rb");
    if (!tf) return 0;  /* file non esiste ancora: lw_init darà errore chiaro */
    fseek(tf, 0, SEEK_END);
    *out_sz = ftell(tf);
    fclose(tf);

    if (*out_sz < 1024 * 1024) {
        printf(RED "\n  \xe2\x9c\x97 File troppo piccolo (%lld KB) \xe2\x80\x94 download ancora in corso?\n" RST, *out_sz/1024);
        printf(DIM "  Attendi che wget/curl finisca prima di caricare il modello.\n\n" RST);
        fflush(stdout);
        return -1;
    }

    /* Doppia misura a 400 ms: se il file cresce è ancora in download */
#ifdef _WIN32
    Sleep(400);
#else
    usleep(400000);
#endif
    long long sz2 = 0;
    FILE* tf2 = fopen(path, "rb");
    if (tf2) { fseek(tf2, 0, SEEK_END); sz2 = ftell(tf2); fclose(tf2); }
    if (sz2 > *out_sz) {
        printf(YLW "\n  \xe2\x9a\xa0  Download ancora in corso! (%.1f GB scaricati finora)\n" RST,
               (double)sz2/(1024*1024*1024));
        printf("  Attendi che wget finisca prima di caricare il modello.\n");
        printf("  Monitora: watch -n3 'ls -lh models/*.gguf'\n\n");
        fflush(stdout);
        return -1;
    }
    return 0;
}

/* ── Verifica VRAM e offre downgrade layer ──────────────────────────────
 * *n_layers è in/out: può essere ridotto dall'utente.
 * Ritorna 0=ok, -1=utente ha annullato.                                 */
static int _lgm_check_vram(long long file_sz, int* n_layers) {
    if (*n_layers == 0 || file_sz == 0 || !g_hw_ready) return 0;
    const HWDevice* pd = &g_hw.dev[g_hw.primary];
    if (pd->type == DEV_CPU) return 0;

    long long file_mb    = file_sz / (1024LL * 1024LL);
    int       tot_layers = estimate_total_layers(file_mb);
    int       gpu_l      = (*n_layers >= 9999) ? tot_layers : *n_layers;
    if (gpu_l > tot_layers) gpu_l = tot_layers;

    long long vram_needed_mb = (file_mb * gpu_l / tot_layers) * 110 / 100;
    long long vu_ = 0, vt_ = 0;
    read_vram_mb(pd->gpu_index, pd->type, &vu_, &vt_);
    long long vram_free = (vt_ > vu_) ? vt_ - vu_ : 0;

    if (vram_free == 0 || vram_free >= vram_needed_mb) return 0;  /* VRAM ok */

    printf(YLW "\n  \xe2\x9a\xa0  VRAM insufficiente per %d layer GPU!\n" RST, gpu_l);
    printf("     VRAM necessaria stimata : ~%lld MB (%.1f GB)\n", vram_needed_mb, vram_needed_mb/1024.0);
    printf("     VRAM libera ora         :  %lld MB (%.1f GB)\n", vram_free,       vram_free/1024.0);
    printf("  [%s] %s\n\n", hw_dev_type_str(pd->type), pd->name);

    /* Offre di scaricare il modello già in VRAM */
    if (lw_is_loaded()) {
        char yn[8];
        printf("  " CYN "Modello gia caricato: %s" RST "\n", lw_model_name());
        printf("  Vuoi scaricarlo per liberare RAM+VRAM? [s/N] "); fflush(stdout);
        if (fgets(yn, sizeof yn, stdin) && (yn[0]=='s' || yn[0]=='S')) {
            lw_free();
            printf(GRN "  \xe2\x9c\x93 Modello precedente scaricato da RAM e VRAM.\n" RST);
            read_vram_mb(pd->gpu_index, pd->type, &vu_, &vt_);
            vram_free = (vt_ > vu_) ? vt_ - vu_ : 0;
            printf("  VRAM libera ora: %lld MB (%.1f GB)\n\n", vram_free, vram_free/1024.0);
        }
    }

    if (vram_free == 0 || vram_free >= vram_needed_mb) return 0;  /* ora ok */

    printf("  Opzioni:\n");
    printf("  " BLD "1" RST "  Ridurre layer GPU (carica solo quanto entra in VRAM)\n");
    printf("  " BLD "2" RST "  CPU-only \xe2\x80\x94 0 layer GPU (pi\xc3\xb9 lento, zero VRAM)\n");
    printf("  " BLD "3" RST "  Tentare comunque (rischio blocco sistema)\n");
    printf("  " BLD "0" RST "  Annulla\n\n");
    printf("  Scelta: "); fflush(stdout);
    char yn[8];
    if (!fgets(yn, sizeof yn, stdin)) yn[0] = '0';
    if (yn[0] == '1') {
        long long mb_per_layer = (file_mb / tot_layers) > 0 ? file_mb / tot_layers : 1;
        long long safe = (vram_free * 90 / 100) / mb_per_layer;
        if (safe < 1) safe = 1;
        *n_layers = (int)safe;
        printf(GRN "  \xe2\x9c\x93 Layer GPU ridotti a %d (~%lld MB VRAM stimata)\n\n" RST,
               *n_layers, mb_per_layer * *n_layers);
    } else if (yn[0] == '2') {
        *n_layers = 0;
        printf(GRN "  \xe2\x9c\x93 Modalita CPU-only (0 layer GPU)\n\n" RST);
    } else if (yn[0] == '3') {
        printf(YLW "  \xe2\x9a\xa0  Procedo. Se il sistema si blocca, usa CPU-only.\n\n" RST);
    } else {
        printf(DIM "  Caricamento annullato.\n\n" RST); fflush(stdout);
        return -1;
    }
    return 0;
}

/* ── Verifica RAM disponibile per la quota CPU del modello ──────────────
 * Ritorna 0=ok, -1=utente ha annullato.                                 */
static int _lgm_check_ram(long long file_sz, int n_layers) {
    if (file_sz == 0) return 0;
    long long file_mb    = file_sz / (1024LL * 1024LL);
    int       tot_layers = estimate_total_layers(file_mb);
    long long cpu_layers = (n_layers == 0) ? tot_layers :
                           (n_layers >= 9999) ? 0 :
                           tot_layers - n_layers;
    long long ram_needed_mb = (n_layers >= 9999)
        ? file_mb * 15 / 100
        : (file_mb * cpu_layers / tot_layers) * 120 / 100;
    if (ram_needed_mb < 256) ram_needed_mb = 256;

    long long ram_avail_mb = ram_available_mb_live();
    if (ram_avail_mb == 0 || ram_avail_mb >= ram_needed_mb) return 0;  /* RAM ok */

    printf(YLW "\n  \xe2\x9a\xa0  RAM insufficiente per la parte CPU del modello!\n" RST);
    printf("     RAM necessaria stimata : ~%lld MB (%.1f GB)\n", ram_needed_mb, ram_needed_mb/1024.0);
    printf("     RAM disponibile ora    :  %lld MB (%.1f GB)\n", ram_avail_mb,  ram_avail_mb/1024.0);

    char yn[8];
    if (lw_is_loaded()) {
        printf("\n  " CYN "Modello gia caricato: %s" RST "\n", lw_model_name());
        printf("  Vuoi scaricarlo per liberare RAM? [s/N] "); fflush(stdout);
        if (fgets(yn, sizeof yn, stdin) && (yn[0]=='s' || yn[0]=='S')) {
            lw_free();
            printf(GRN "  \xe2\x9c\x93 Modello precedente scaricato da RAM e VRAM.\n" RST);
            ram_avail_mb = ram_available_mb_live();
            printf("  RAM disponibile ora: %lld MB (%.1f GB)\n\n", ram_avail_mb, ram_avail_mb/1024.0);
        }
    }

    if (ram_avail_mb < ram_needed_mb) {
        printf(YLW "  \xe2\x9a\xa0  RAM ancora insufficiente (%lld MB < %lld MB stimati).\n" RST,
               ram_avail_mb, ram_needed_mb);
        printf("  Il sistema potrebbe bloccarsi o usare swap.\n");
        printf("  Vuoi tentare comunque? [s/N] "); fflush(stdout);
        if (!fgets(yn, sizeof yn, stdin) || (yn[0]!='s' && yn[0]!='S')) {
            printf(DIM "  Caricamento annullato.\n\n" RST); fflush(stdout);
            return -1;
        }
    }
    return 0;
}

/* ── Carica un modello .gguf — orchestra i tre check poi lw_init ─────── */
void load_gguf_model(const char* filename) {
    char path[1024];
    snprintf(path, sizeof path, "%s/%s", g_models_dir, filename);

    long long file_sz = 0;
    if (_lgm_check_file(path, &file_sz) != 0) return;

    int n_layers = (g_hw_ready && g_hw.dev[g_hw.primary].type != DEV_CPU)
                   ? g_hw.dev[g_hw.primary].n_gpu_layers : 0;

    if (_lgm_check_vram(file_sz, &n_layers) != 0) return;  /* VRAM prima */
    if (_lgm_check_ram(file_sz,  n_layers)  != 0) return;  /* RAM dopo   */

    printf("\n  " YLW "\xe2\x8f\xb3 Caricamento: %s" RST "\n", filename);
    printf("  " DIM "(llama.cpp \xe2\x80\x94 qualche secondo...)" RST "\n"); fflush(stdout);
    if (lw_init(path, n_layers, 4096) == 0) {
        salva_config();
        printf("  " GRN "\xe2\x9c\x93 Modello pronto! (GPU layers: %d)" RST "\n\n", n_layers);
    } else {
        printf("  " RED "\xe2\x9c\x97 Errore caricamento \xe2\x80\x94 il file potrebbe essere corrotto o incompleto.\n" RST
               "  Se il download era in corso, attendi che finisca e riprova.\n\n" RST);
    }
    fflush(stdout);
}

/* Selezione/configurazione backend */
void choose_backend(void) {
    _sync_to_profile(); /* salva stato corrente prima di cambiare */
    while(1){
        clear_screen(); print_header();
        printf("\n" BLD YLW "  \xe2\x9a\x99  Configurazione Backend AI\n\n" RST);
        printf("  " EM_1 "  \xf0\x9f\xa6\x99  llama-statico   " DIM "(libllama, zero server, richiede ./build.sh)\n" RST);
        printf("  " EM_2 "  \xf0\x9f\x90\xa6  Ollama          " DIM "(HTTP localhost:%d, avvia con 'ollama serve')\n" RST,
               g_ctx.backend==BACKEND_OLLAMA?g_ctx.port:11434);
        printf("  " EM_3 "  \xf0\x9f\x96\xa5\xef\xb8\x8f  llama-server    " DIM "(HTTP OpenAI API, avvia llama-server -m model.gguf)\n" RST);
        printf("  " EM_0 "  \xe2\x86\x90  Torna\n\n");
        /* Mostra modello corretto per ogni backend */
        if(g_ctx.backend==BACKEND_LLAMA)
            printf("  Corrente: " CYN "llama-statico" RST "  modello: " CYN "%s" RST "\n\n",
                   lw_is_loaded()?lw_model_name():RED "(nessuno \xe2\x80\x94 usa Opzione 2)" RST);
        else
            printf("  Corrente: " CYN "%s" RST " @ %s:%d  modello: " CYN "%s" RST "\n\n",
                   backend_name(g_ctx.backend), g_ctx.host, g_ctx.port,
                   g_ctx.model[0]?g_ctx.model:"(nessuno)");
        printf("  Scelta: "); fflush(stdout);
        int c=getch_single(); printf("%c\n",c);
        if(c=='0'||c==27)break;
        if(c=='1'){
            g_ctx.backend = BACKEND_LLAMA; g_ctx.port = 0;
            /* Ripristina ultimo modello llama usato */
            if(g_prof_llama_model[0])
                strncpy(g_ctx.model, g_prof_llama_model, sizeof g_ctx.model - 1);
            /* Auto-carica se c'e' un solo .gguf e non e' gia caricato */
            if(!lw_is_loaded()){
                char gguf_files[MAX_MODELS][256];
                int nm=scan_gguf_dir(g_models_dir,gguf_files,MAX_MODELS);
                if(nm==1){
                    printf(GRN "  Auto-caricamento: %s\n" RST, gguf_files[0]);
                    load_gguf_model(gguf_files[0]);
                } else if(nm>1){
                    printf(YLW "  %d modelli trovati \xe2\x80\x94 usa Gestione Modelli per scegliere.\n" RST, nm);
                } else {
                    printf(YLW "  Nessun .gguf in %s/ \xe2\x80\x94 aggiungi un modello.\n" RST, g_models_dir);
                }
            }
        }
        else if(c=='2'){
            g_ctx.backend = BACKEND_OLLAMA;
            /* Ripristina profilo Ollama */
            strncpy(g_ctx.host,  g_prof_ollama_host,  sizeof g_ctx.host  - 1);
            g_ctx.port = g_prof_ollama_port;
            strncpy(g_ctx.model, g_prof_ollama_model, sizeof g_ctx.model - 1);
            printf("  Ollama: " CYN "%s:%d" RST "  modello: " CYN "%s" RST
                   "  Personalizzare? [s/N] ",
                   g_ctx.host, g_ctx.port,
                   g_ctx.model[0] ? g_ctx.model : "(nessuno)"); fflush(stdout);
            char yn2[8]; if(fgets(yn2,sizeof yn2,stdin) && (yn2[0]=='s'||yn2[0]=='S')){
                char buf[128];
                printf("  Host [%s]: ", g_ctx.host); fflush(stdout);
                if(fgets(buf,sizeof buf,stdin)){
                    buf[strcspn(buf,"\n")]='\0';
                    if(buf[0]) strncpy(g_ctx.host,buf,sizeof g_ctx.host-1);
                }
                printf("  Porta [%d]: ", g_ctx.port); fflush(stdout);
                if(fgets(buf,sizeof buf,stdin)){
                    buf[strcspn(buf,"\n")]='\0';
                    if(buf[0]&&atoi(buf)>0) g_ctx.port=atoi(buf);
                }
            }
        }
        else if(c=='3'){
            g_ctx.backend = BACKEND_LLAMASERVER;
            /* Ripristina profilo llama-server */
            strncpy(g_ctx.host,  g_prof_lserver_host,  sizeof g_ctx.host  - 1);
            g_ctx.port = g_prof_lserver_port;
            strncpy(g_ctx.model, g_prof_lserver_model, sizeof g_ctx.model - 1);
            printf("  llama-server: " CYN "%s:%d" RST "  modello: " CYN "%s" RST
                   "  Personalizzare? [s/N] ",
                   g_ctx.host, g_ctx.port,
                   g_ctx.model[0] ? g_ctx.model : "(nessuno)"); fflush(stdout);
            char yn3[8]; if(fgets(yn3,sizeof yn3,stdin) && (yn3[0]=='s'||yn3[0]=='S')){
                char buf[128];
                printf("  Host [%s]: ", g_ctx.host); fflush(stdout);
                if(fgets(buf,sizeof buf,stdin)){
                    buf[strcspn(buf,"\n")]='\0';
                    if(buf[0]) strncpy(g_ctx.host,buf,sizeof g_ctx.host-1);
                }
                printf("  Porta [%d]: ", g_ctx.port); fflush(stdout);
                if(fgets(buf,sizeof buf,stdin)){
                    buf[strcspn(buf,"\n")]='\0';
                    if(buf[0]&&atoi(buf)>0) g_ctx.port=atoi(buf);
                }
            }
        }
        salva_config();
        printf(GRN "  \xe2\x9c\x93 Backend impostato: %s\n" RST, backend_name(g_ctx.backend));
        printf(GRY "  [INVIO per continuare] " RST); fflush(stdout);
        { char _b[4]; if(!fgets(_b,sizeof _b,stdin)){} }
        break;
    }
}

/* Selezione modello (dipende dal backend corrente) */
void choose_model(void) {
    while(1) {
    clear_screen(); print_header();

    int tw=term_width(), W=tw-2;
    if(W<60) W=60;
    if(W>130) W=130;
    box_top(W);
    box_ctr(W, BLD WHT "  Selezione Modello  " RST);
    box_sep(W);
    {
        char bline[128];
        snprintf(bline,sizeof bline," Backend: " CYN "%s" RST, backend_name(g_ctx.backend));
        box_lft(W,bline);
        if(g_ctx.backend!=BACKEND_LLAMA){
            char aline[128];
            snprintf(aline,sizeof aline," Indirizzo: " WHT "%s:%d" RST, g_ctx.host,g_ctx.port);
            box_lft(W,aline);
        }
        if(g_ctx.model[0]||lw_is_loaded()){
            const char *cm=(g_ctx.backend==BACKEND_LLAMA)
                           ? (lw_is_loaded()?lw_model_name():"(nessuno)")
                           : (g_ctx.model[0]?g_ctx.model:"(nessuno)");
            char mline[256];
            snprintf(mline,sizeof mline," Modello attuale: " CYN "%s" RST, cm);
            box_lft(W,mline);
        }
    }
    box_sep(W);

    if(g_ctx.backend==BACKEND_LLAMA){
        /* ── llama-statico: lista .gguf ─────────────────────── */
        char gguf_files[MAX_MODELS][256];
        int nm=scan_gguf_dir(g_models_dir,gguf_files,MAX_MODELS);

        if(nm==0){
            box_lft(W, YLW " \xe2\x9a\xa0  Nessun file .gguf trovato in models/" RST);
            box_lft(W, GRY "    (per usare Ollama o llama-server cambia backend)" RST);
            box_sep(W);
            box_lft(W, " " BLD "S" RST "  Passa subito a " CYN "Ollama" RST " (localhost:11434)");
            box_lft(W, " " BLD "M" RST "  Imposta modello manualmente (nome libero)");
            box_lft(W, " " EM_0 "  Torna");
        } else {
            char hdr[64]; snprintf(hdr,sizeof hdr," File .gguf in models/  (%d trovati):",nm);
            box_lft(W,hdr);
            box_emp(W);
            for(int i=0;i<nm;i++){
                int sel=(lw_is_loaded()&&strcmp(gguf_files[i],lw_model_name())==0);
                if(sel){ char tmp[512]; snprintf(tmp,sizeof tmp," " BLD "%d." RST "  " CYN "%s  \xe2\x86\x90 attivo" RST,i+1,gguf_files[i]); box_lft(W,tmp); }
                else  { char row[320]; snprintf(row,sizeof row," " BLD "%d." RST "  %s",i+1,gguf_files[i]); box_lft(W,row); }
            }
            box_sep(W);
            box_lft(W, " " BLD "M" RST "  Imposta modello manualmente");
            box_lft(W, " " EM_0 "  Torna");
        }
        box_bot(W);
        printf("\n  Scelta: "); fflush(stdout);
        char buf[64]; if(!fgets(buf,sizeof buf,stdin))return;
        buf[strcspn(buf,"\n")]='\0';

        if(buf[0]=='0') return;
        if(buf[0]=='s'||buf[0]=='S'){
            /* Passa a Ollama */
            g_ctx.backend=BACKEND_OLLAMA;
            strncpy(g_ctx.host,"127.0.0.1",sizeof g_ctx.host-1);
            g_ctx.port=11434;
            printf(GRN "  \xe2\x9c\x93 Backend cambiato a Ollama @ 127.0.0.1:11434\n" RST);
            /* continua il loop per mostrare lista Ollama */
            continue;
        }
        if(buf[0]=='m'||buf[0]=='M'){
            printf("  Nome modello: "); fflush(stdout);
            char mn[128]; if(!fgets(mn,sizeof mn,stdin))return;
            mn[strcspn(mn,"\n")]='\0';
            if(mn[0]){
                strncpy(g_ctx.model,mn,sizeof g_ctx.model-1);
                /* Se llama, prova a caricare come .gguf */
                if(g_ctx.backend==BACKEND_LLAMA){ lw_free(); load_gguf_model(mn); }
                else printf(GRN "  \xe2\x9c\x93 Modello impostato: %s\n" RST,g_ctx.model);
            }
        } else {
            int ch=atoi(buf);
            if(nm>0 && ch>=1 && ch<=nm){ lw_free(); load_gguf_model(gguf_files[ch-1]); }
        }

    } else {
        /* ── HTTP backend: lista da server ─────────────────── */
        char http_models[MAX_MODELS][128];
        int nm=0;
        box_lft(W, GRY " Connessione al server..." RST);
        box_bot(W);
        fflush(stdout);

        if(g_ctx.backend==BACKEND_OLLAMA)
            nm=http_ollama_list(g_ctx.host,g_ctx.port,http_models,MAX_MODELS);
        else
            nm=http_llamaserver_list(g_ctx.host,g_ctx.port,http_models,MAX_MODELS);

        /* Ridisegna la box con i risultati */
        clear_screen(); print_header();
        box_top(W);
        box_ctr(W, BLD WHT "  Selezione Modello  " RST);
        box_sep(W);

        if(nm==0){
            box_lft(W, RED " \xe2\x9c\x97  Nessun modello trovato \xe2\x80\x94 server non raggiungibile" RST);
            char dl[128];
            snprintf(dl,sizeof dl," Indirizzo: " CYN "%s:%d" RST,g_ctx.host,g_ctx.port);
            box_lft(W,dl);
            box_sep(W);
            if(g_ctx.backend==BACKEND_OLLAMA){
                box_lft(W, YLW " Avvia Ollama con:  " RST BLD "ollama serve" RST);
                box_lft(W, GRY " Poi scegli un modello con:  ollama pull <nome>" RST);
            } else {
                box_lft(W, YLW " Avvia llama-server con:" RST);
                box_lft(W, BLD "   llama-server -m /percorso/modello.gguf --port 8080 --host 127.0.0.1" RST);
                box_lft(W, GRY " Oppure usa " BLD "Impostazioni \xe2\x86\x92 2 (llama.cpp)" RST GRY " per caricare un .gguf senza server" RST);
            }
            box_sep(W);
        } else {
            char hdr[64]; snprintf(hdr,sizeof hdr," Modelli disponibili  (%d):",nm);
            box_lft(W,hdr);
            box_emp(W);
            for(int i=0;i<nm;i++){
                int sel=strcmp(http_models[i],g_ctx.model)==0;
                if(sel){
                    char row[256]; snprintf(row,sizeof row," [%d]  " CYN "%s  \xe2\x86\x90 attivo" RST,i+1,http_models[i]);
                    box_lft(W,row);
                } else {
                    char row[256]; snprintf(row,sizeof row," [%d]  %s",i+1,http_models[i]);
                    box_lft(W,row);
                }
            }
            box_sep(W);
        }
        box_lft(W, " " BLD "M" RST "  Imposta nome manualmente (es: llama3.2, qwen3:30b)");
        box_lft(W, " " EM_0 "  Torna");
        box_bot(W);

        printf("\n  Scelta (1-%d / M / 0): ",nm); fflush(stdout);
        char buf[64]; if(!fgets(buf,sizeof buf,stdin))return;
        buf[strcspn(buf,"\n")]='\0';

        if(buf[0]=='0') return;
        if(buf[0]=='m'||buf[0]=='M'){
            printf("  Nome modello: "); fflush(stdout);
            char mn[128]; if(!fgets(mn,sizeof mn,stdin))return;
            mn[strcspn(mn,"\n")]='\0';
            if(mn[0]){
                strncpy(g_ctx.model,mn,sizeof g_ctx.model-1);
                printf(GRN "  \xe2\x9c\x93 Modello impostato: %s\n" RST,g_ctx.model);
            }
        } else {
            int ch=atoi(buf);
            if(nm>0 && ch>=1 && ch<=nm){
                strncpy(g_ctx.model,http_models[ch-1],sizeof g_ctx.model-1);
                printf(GRN "  \xe2\x9c\x93 Modello: %s\n" RST,g_ctx.model);
            }
        }
    }

    printf(GRY "\n  [INVIO per continuare] " RST); fflush(stdout);
    { char _b[4]; if(!fgets(_b,sizeof _b,stdin)){} }
    return;
    } /* fine while */
}

/* ── scegli_modello_rapido — selettore compatto embeddabile ──────────────
 * Mostra la lista dei modelli disponibili (Ollama/llama-server) o .gguf in
 * una singola schermata inline, senza loop. L'utente sceglie con un numero
 * oppure preme INVIO per mantenere il modello corrente. Pensato per essere
 * chiamato da schermate di configurazione (agenti specializzati, byzantino).
 * ─────────────────────────────────────────────────────────────────────── */
void scegli_modello_rapido(void) {
    char http_models[MAX_MODELS][128];
    int  nm = 0;

    printf(CYN BLD "\n  \xf0\x9f\xa4\x96  Selezione Modello LLM\n" RST);

    if (g_ctx.backend == BACKEND_LLAMA) {
        /* ── llama-statico: lista .gguf ── */
        char gguf_files[MAX_MODELS][256];
        nm = scan_gguf_dir(g_models_dir, gguf_files, MAX_MODELS);
        if (nm == 0) {
            printf(GRY "  (nessun .gguf in models/ — modello corrente: %s)\n" RST,
                   lw_is_loaded() ? lw_model_name() : "nessuno");
            printf(GRY "  [INVIO per mantenere] " RST); fflush(stdout);
            { char _b[4]; if (!fgets(_b, sizeof _b, stdin)) {} }
            return;
        }
        const char *cur = lw_is_loaded() ? lw_model_name() : "";
        for (int i = 0; i < nm; i++) {
            int sel = strcmp(gguf_files[i], cur) == 0;
            if (sel) printf("  " YLW "[%2d]" GRN "  %s  \xe2\x86\x90 attivo\n" RST, i+1, gguf_files[i]);
            else     printf("  " YLW "[%2d]" RST "  %s\n", i+1, gguf_files[i]);
        }
        printf(GRY "\n  [1-%d] Scegli   [INVIO] Mantieni corrente: " CYN "%s\n" RST,
               nm, cur[0] ? cur : "nessuno");
        printf(GRN "  Scelta: " RST); fflush(stdout);
        char buf[16]; if (!fgets(buf, sizeof buf, stdin)) return;
        buf[strcspn(buf, "\n")] = '\0';
        int ch = atoi(buf);
        if (ch >= 1 && ch <= nm) {
            char path[512];
            snprintf(path, sizeof path, "%s/%s", g_models_dir, gguf_files[ch-1]);
            /* riusa load_gguf_model per avere i check RAM/VRAM */
            load_gguf_model(gguf_files[ch-1]);
        }
        return;
    }

    /* ── HTTP backend: lista da server ── */
    printf(GRY "  Recupero modelli da %s:%d..." RST, g_ctx.host, g_ctx.port);
    fflush(stdout);
    if (g_ctx.backend == BACKEND_OLLAMA)
        nm = http_ollama_list(g_ctx.host, g_ctx.port, http_models, MAX_MODELS);
    else
        nm = http_llamaserver_list(g_ctx.host, g_ctx.port, http_models, MAX_MODELS);

    printf("\r" "  " "                                        " "\r"); /* cancella riga */

    if (nm == 0) {
        printf(YLW "  \xe2\x9a\xa0  Nessun modello trovato (server offline?)\n" RST);
        printf(GRY "  Modello corrente: " CYN "%s\n" RST, g_ctx.model[0] ? g_ctx.model : "nessuno");
        printf(GRY "  [INVIO per mantenere] " RST); fflush(stdout);
        { char _b[4]; if (!fgets(_b, sizeof _b, stdin)) {} }
        return;
    }

    for (int i = 0; i < nm; i++) {
        int sel = strcmp(http_models[i], g_ctx.model) == 0;
        if (sel) printf("  " YLW "[%2d]" GRN "  %s  \xe2\x86\x90 attivo\n" RST, i+1, http_models[i]);
        else     printf("  " YLW "[%2d]" RST "  %s\n", i+1, http_models[i]);
    }
    printf(GRY "\n  [1-%d] Scegli   [INVIO] Mantieni corrente: " CYN "%s\n" RST,
           nm, g_ctx.model[0] ? g_ctx.model : "nessuno");
    printf(GRN "  Scelta: " RST); fflush(stdout);
    char buf[16]; if (!fgets(buf, sizeof buf, stdin)) return;
    buf[strcspn(buf, "\n")] = '\0';
    if (!buf[0]) return; /* INVIO = mantieni */
    int ch = atoi(buf);
    if (ch >= 1 && ch <= nm) {
        strncpy(g_ctx.model, http_models[ch-1], sizeof g_ctx.model - 1);
        printf(GRN "  \xe2\x9c\x93 Modello: " CYN "%s\n" RST, g_ctx.model);
    }
}

/* ══════════════════════════════════════════════════════════════
   math_model_check — verifica presenza modello matematico e
   consiglia quantizzazione ottimale per il sistema corrente.
   Restituisce 1 se trovato almeno un modello matematico, 0 altrimenti.
   ══════════════════════════════════════════════════════════════ */
static int _is_math_model(const char* name) {
    /* Nomi in minuscolo cercati nel filename (case-insensitive) */
    static const char* KWORDS[] = { "math","numina","mathstral","minerva","deepseek-math",NULL };
    char lower[256]; int i=0;
    while(name[i] && i < 255) { lower[i] = (char)tolower((unsigned char)name[i]); i++; }
    lower[i] = '\0';
    for(int k=0; KWORDS[k]; k++)
        if(strstr(lower, KWORDS[k])) return 1;
    return 0;
}

static void _math_model_download_info(int W) {
    /* Lista curata HF: 2 modelli × 2 quantizzazioni */
    static const struct {
        const char* nome; const char* desc;
        const char* lbl_q4; const char* dim_q4;
        const char* lbl_q8; const char* dim_q8;
        const char* url_q4; const char* file_q4;
        const char* url_q8; const char* file_q8;
    } MATH_HF[] = {
        {
            "Qwen2.5-Math-72B-Instruct",
            "Matematica universitaria, analisi, algebra, calcolo — OTTIMALE per Xeon 64 GB",
            "[g] Q4_K_M", "~40 GB (entra in RAM, usa --no-mmap)",
            "[h] Q8_0  ", "~74 GB (mmap + NVMe SSD, qualita superiore)",
            "https://huggingface.co/bartowski/Qwen2.5-Math-72B-Instruct-GGUF/resolve/main/Qwen2.5-Math-72B-Instruct-Q4_K_M.gguf",
            "Qwen2.5-Math-72B-Instruct-Q4_K_M.gguf",
            "https://huggingface.co/bartowski/Qwen2.5-Math-72B-Instruct-GGUF/resolve/main/Qwen2.5-Math-72B-Instruct-Q8_0.gguf",
            "Qwen2.5-Math-72B-Instruct-Q8_0.gguf",
        },
        {
            "Qwen2.5-Math-7B-Instruct",
            "Matematica avanzata — leggero (test rapidi o RAM < 16 GB)",
            "[i] Q4_K_M", "~4.7 GB",
            "[j] Q8_0  ", "~7.7 GB",
            "https://huggingface.co/bartowski/Qwen2.5-Math-7B-Instruct-GGUF/resolve/main/Qwen2.5-Math-7B-Instruct-Q4_K_M.gguf",
            "Qwen2.5-Math-7B-Instruct-Q4_K_M.gguf",
            "https://huggingface.co/bartowski/Qwen2.5-Math-7B-Instruct-GGUF/resolve/main/Qwen2.5-Math-7B-Instruct-Q8_0.gguf",
            "Qwen2.5-Math-7B-Instruct-Q8_0.gguf",
        },
    };
    int NM = (int)(sizeof(MATH_HF)/sizeof(MATH_HF[0]));
    (void)W;

    printf(YLW "\n  \xf0\x9f\x93\x90  Modelli matematici disponibili su Hugging Face\n\n" RST);
    for(int m=0; m<NM; m++){
        printf(BLD "  %s\n" RST, MATH_HF[m].nome);
        printf(GRY "  %s\n\n" RST, MATH_HF[m].desc);
        printf("    " CYN "%s" RST "  %s\n", MATH_HF[m].lbl_q4, MATH_HF[m].dim_q4);
        printf("    " CYN "%s" RST "  %s\n\n", MATH_HF[m].lbl_q8, MATH_HF[m].dim_q8);
    }
    printf("  " BLD "[0]" RST "  Annulla\n");
    printf("\n  Scelta: "); fflush(stdout);

    char sc[8]; if(!fgets(sc, sizeof sc, stdin)) return;
    sc[strcspn(sc,"\n")] = '\0';

    /* Mappa tasti → url/file */
    struct { char k; const char* url; const char* fname; } MAP[] = {
        {'g', MATH_HF[0].url_q4, MATH_HF[0].file_q4},
        {'h', MATH_HF[0].url_q8, MATH_HF[0].file_q8},
        {'i', MATH_HF[1].url_q4, MATH_HF[1].file_q4},
        {'j', MATH_HF[1].url_q8, MATH_HF[1].file_q8},
    };
    const char* url=NULL; const char* fname=NULL;
    for(int m=0;m<4;m++){
        if(sc[0]==MAP[m].k){ url=MAP[m].url; fname=MAP[m].fname; break; }
    }
    if(!url || sc[0]=='0') return;

    char dest[1600];
    snprintf(dest, sizeof dest, "%s/%s", g_models_dir, fname);
    printf(CYN "\n  \xe2\x8f\xb3 Download: %s\n" RST, fname);
    printf(DIM "  Destinazione: %s\n" RST, dest);

    /* Note specifiche per quantizzazione */
    if(sc[0]=='g'){
        printf(YLW "  Flag consigliati dopo il download:\n" RST);
        printf(BLD "    llama-server -m %s --port 8081 --host 127.0.0.1 --no-mmap --ctx-size 8192 --threads 24\n\n" RST, dest);
    } else if(sc[0]=='h'){
        printf(YLW "  Flag consigliati dopo il download:\n" RST);
        printf(BLD "    llama-server -m %s --port 8081 --host 127.0.0.1 --ctx-size 8192 --threads 24\n" RST, dest);
        printf(GRY "    (mmap attivo: NVMe SSD richiesto per prestazioni accettabili)\n\n" RST);
    }
    fflush(stdout);

    char cmd[2750];
    snprintf(cmd, sizeof cmd,
             "wget -c --progress=bar:force \"%s\" -O \"%s\" 2>&1 &\necho \"  PID download: $!\"",
             url, dest);
    { int _rc = system(cmd); (void)_rc; }
    printf(GRY "\n  [INVIO per continuare] " RST); fflush(stdout);
    { char _b[4]; if(!fgets(_b,sizeof _b,stdin)){} }
}

static void math_model_check(int W) {
    char gguf_files[MAX_MODELS][256];
    int nm = scan_gguf_dir(g_models_dir, gguf_files, MAX_MODELS);

    /* Cerca modelli matematici tra quelli presenti */
    int math_idx[MAX_MODELS]; int nm_math = 0;
    for(int i=0; i<nm; i++)
        if(_is_math_model(gguf_files[i])) math_idx[nm_math++] = i;

    clear_screen(); print_header();
    printf(BLD YLW "\n  \xf0\x9f\x93\x90  Profilo Matematico — Verifica Modello\n" RST);
    printf(DIM "  Cartella: %s/\n\n" RST, g_models_dir);

    if(nm_math > 0) {
        printf(GRN "  \xe2\x9c\x93 Trovati %d modello/i matematico/i:\n\n" RST, nm_math);
        printf(GRY "  %-4s  %-48s  %-10s  %s\n" RST, "N.", "Nome file", "Dim.", "Quantiz.");
        printf(GRY "  \xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80  \xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80  \xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80  \xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\n" RST);

        for(int k=0; k<nm_math; k++) {
            const char* fn = gguf_files[math_idx[k]];
            char path[1024];
            snprintf(path, sizeof path, "%s/%s", g_models_dir, fn);
            long long sz = 0;
            FILE* tf = fopen(path,"rb");
            if(tf){ fseek(tf,0,SEEK_END); sz=ftell(tf); fclose(tf); }
            char sz_str[24];
            if(sz >= 1024LL*1024*1024)
                snprintf(sz_str, sizeof sz_str, "%.1f GB", (double)sz/(1024*1024*1024));
            else
                snprintf(sz_str, sizeof sz_str, "%.0f MB", (double)sz/(1024*1024));

            /* Rileva quantizzazione dal nome file */
            char lower[256]; int li=0;
            while(fn[li] && li<255){ lower[li]=(char)tolower((unsigned char)fn[li]); li++; }
            lower[li]='\0';
            const char* quant = "?";
            if(strstr(lower,"q8_0"))       quant = "Q8_0   \xe2\x98\x85\xe2\x98\x85\xe2\x98\x85 qualita max";
            else if(strstr(lower,"q4_k_m")) quant = "Q4_K_M \xe2\x98\x85\xe2\x98\x85\xe2\x98\x86 bilanciato";
            else if(strstr(lower,"q4_k_s")) quant = "Q4_K_S \xe2\x98\x85\xe2\x98\x86\xe2\x98\x86 compresso";

            /* Consiglio specifico per dimensione */
            int is_big = (sz > 10LL*1024*1024*1024); /* >10 GB */
            printf("  " BLD "%d." RST "  %-48s  %-10s  %s\n", k+1, fn, sz_str, quant);
            if(is_big && strstr(lower,"q8_0"))
                printf(GRY "        \xe2\x86\x92 Xeon 64 GB: usa --no-mmap se entra in RAM, altrimenti mmap+NVMe\n" RST);
            else if(is_big && strstr(lower,"q4_k_m"))
                printf(GRN "        \xe2\x86\x92 Xeon 64 GB: entra in RAM \xe2\x9c\x93 usa --no-mmap per velocita massima\n" RST);
        }

        /* Mostra il comando di avvio ottimale per il primo modello matematico trovato */
        printf("\n");
        const char* best = gguf_files[math_idx[0]];
        char lower_best[256]; int li=0;
        while(best[li]&&li<255){ lower_best[li]=(char)tolower((unsigned char)best[li]); li++; }
        lower_best[li]='\0';
        int is_q4 = (strstr(lower_best,"q4_k_m") != NULL);

        printf(CYN "  Comando consigliato per " BLD "%s" RST CYN ":\n" RST, best);
        printf(BLD "    llama-server -m %s/%s\\\n" RST, g_models_dir, best);
        printf(BLD "      --port 8081 --host 127.0.0.1 --ctx-size 8192 --threads 24\n" RST);
        if(is_q4)
            printf(BLD "      --no-mmap\n" RST);
        printf(GRY "    (adatta --threads ai core fisici reali dello Xeon)\n\n" RST);

        printf("  " BLD "D" RST "  Scarica un altro modello matematico\n");
        printf("  " EM_0 "  Torna\n");
    } else {
        printf(YLW "  \xe2\x9a\xa0  Nessun modello matematico trovato in models/\n\n" RST);
        printf(GRY "  Parole chiave cercate: math, numina, mathstral, minerva\n\n" RST);
        _math_model_download_info(W);
        return;
    }

    printf("\n  Scelta: "); fflush(stdout);
    int c = getch_single(); printf("%c\n", c);
    if(c == 'd' || c == 'D') {
        _math_model_download_info(W);
    }
}

/* ── Gestione modelli .gguf (scarica / rimuovi / info) ── */
void gestisci_modelli(void) {
    while(1) {
        clear_screen(); print_header();
        printf(BLD YLW "\n  \xf0\x9f\x93\xa6 Gestione Modelli llama.cpp\n" RST);
        printf(DIM "  Cartella: %s/\n\n" RST, g_models_dir);

        char gguf_files[MAX_MODELS][256];
        int nm = scan_gguf_dir(g_models_dir, gguf_files, MAX_MODELS);

        if(nm == 0) {
            printf(YLW "  \xe2\x9a\xa0  Nessun modello .gguf presente nella cartella models/\n\n" RST);
            printf(GRY "  Premi " BLD "D" RST GRY " per scaricare un modello dalla lista curata qui sotto.\n\n" RST);
        } else {
            printf(GRY "  %-4s  %-48s  %s\n" RST, "N.", "Nome file", "Dimensione");
            printf(GRY "  %-4s  %-48s  %s\n" RST, "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80", "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80", "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80");
            for(int i = 0; i < nm; i++) {
                /* Calcola dimensione file */
                char path[1024];
                snprintf(path, sizeof path, "%s/%s", g_models_dir, gguf_files[i]);
                long long sz = 0;
                FILE* tf = fopen(path, "rb");
                if(tf) { fseek(tf, 0, SEEK_END); sz = ftell(tf); fclose(tf); }
                char sz_str[32];
                if(sz >= 1024*1024*1024)
                    snprintf(sz_str, sizeof sz_str, "%.1f GB", (double)sz/(1024*1024*1024));
                else
                    snprintf(sz_str, sizeof sz_str, "%.0f MB", (double)sz/(1024*1024));

                int attivo = lw_is_loaded() && strcmp(gguf_files[i], lw_model_name()) == 0;
                printf("  " BLD "%d." RST "  %s%-48s" RST "  %s%s\n",
                       i+1,
                       attivo ? CYN : "",
                       gguf_files[i],
                       sz_str,
                       attivo ? "  " GRN "\xe2\x86\x90 attivo" RST : "");
            }
            printf("\n");
        }

        printf("  " BLD "D" RST "  Scarica modello da Hugging Face\n");
        printf("  " BLD "M" RST "  " YLW "\xf0\x9f\x93\x90 Profilo Matematico" RST " — verifica/scarica modello math\n");
        if(nm > 0)
            printf("  " BLD "R" RST "  Rimuovi modello\n");
        printf("  " EM_0 "  Torna\n");

        printf("\n  Scelta: "); fflush(stdout);

        int c = getch_single(); printf("%c\n", c);
        if(c == '0' || c == 27) break;

        /* ── Profilo Matematico ── */
        if(c == 'm' || c == 'M') {
            int W = term_width(); if(W<60)W=60; if(W>130)W=130;
            math_model_check(W);
            continue;
        }

        /* ── Scarica ── */
        if(c == 'd' || c == 'D') {
            /* Tabella modelli {chiave, desc_breve, url, filename}
             * Chiavi: '1'-'9' + 'a'-'z' per avere più di 9 modelli senza emoji */
            static const struct {
                char k;
                const char* cat;   /* categoria (usata per stampare separatori) */
                const char* desc;
                const char* url;
                const char* fname;
            } MODS[] = {
              /* ── Microscopici < 500 MB ───────────────────────────── */
              {'1',"micro",
               "SmolLM2-135M        ~145 MB   testo semplice, ultra-veloce (CPU)",
               "https://huggingface.co/bartowski/SmolLM2-135M-Instruct-GGUF/resolve/main/SmolLM2-135M-Instruct-Q8_0.gguf",
               "SmolLM2-135M-Instruct-Q8_0.gguf"},
              {'2',"micro",
               "SmolLM2-360M        ~385 MB   testo base, CPU-friendly",
               "https://huggingface.co/bartowski/SmolLM2-360M-Instruct-GGUF/resolve/main/SmolLM2-360M-Instruct-Q8_0.gguf",
               "SmolLM2-360M-Instruct-Q8_0.gguf"},
              /* ── Piccoli 500 MB – 2 GB ───────────────────────────── */
              {'3',"small",
               "TinyLlama-1.1B      ~670 MB   chat generale, leggerissimo",
               "https://huggingface.co/TheBloke/TinyLlama-1.1B-Chat-v1.0-GGUF/resolve/main/tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf",
               "tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf"},
              {'4',"small",
               "Qwen2.5-1.5B        ~1.0 GB   ottimo testo, italiano decente",
               "https://huggingface.co/bartowski/Qwen2.5-1.5B-Instruct-GGUF/resolve/main/Qwen2.5-1.5B-Instruct-Q4_K_M.gguf",
               "Qwen2.5-1.5B-Instruct-Q4_K_M.gguf"},
              {'5',"small",
               "Llama-3.2-3B        ~1.9 GB   Meta, buona comprensione",
               "https://huggingface.co/bartowski/Llama-3.2-3B-Instruct-GGUF/resolve/main/Llama-3.2-3B-Instruct-Q4_K_M.gguf",
               "Llama-3.2-3B-Instruct-Q4_K_M.gguf"},
              /* ── Medi 2–5 GB ─────────────────────────────────────── */
              {'6',"med",
               "Qwen2.5-3B          ~2.0 GB   uso generale, bilingue ita/eng",
               "https://huggingface.co/bartowski/Qwen2.5-3B-Instruct-GGUF/resolve/main/Qwen2.5-3B-Instruct-Q4_K_M.gguf",
               "Qwen2.5-3B-Instruct-Q4_K_M.gguf"},
              {'7',"med",
               "Phi-3.5-mini        ~2.2 GB   Microsoft, ragionamento",
               "https://huggingface.co/bartowski/Phi-3.5-mini-instruct-GGUF/resolve/main/Phi-3.5-mini-instruct-Q4_K_M.gguf",
               "Phi-3.5-mini-instruct-Q4_K_M.gguf"},
              {'8',"med",
               "Gemma-3-4B          ~2.5 GB   Google, italiano buono",
               "https://huggingface.co/bartowski/gemma-3-4b-it-GGUF/resolve/main/gemma-3-4b-it-Q4_K_M.gguf",
               "gemma-3-4b-it-Q4_K_M.gguf"},
              {'9',"med",
               "Qwen2.5-Math-7B     ~4.7 GB   matematica avanzata",
               "https://huggingface.co/bartowski/Qwen2.5-Math-7B-Instruct-GGUF/resolve/main/Qwen2.5-Math-7B-Instruct-Q4_K_M.gguf",
               "Qwen2.5-Math-7B-Instruct-Q4_K_M.gguf"},
              {'a',"med",
               "Qwen2.5-Coder-7B    ~4.7 GB   programmazione Python/C",
               "https://huggingface.co/bartowski/Qwen2.5-Coder-7B-Instruct-GGUF/resolve/main/Qwen2.5-Coder-7B-Instruct-Q4_K_M.gguf",
               "Qwen2.5-Coder-7B-Instruct-Q4_K_M.gguf"},
              /* ── Grandi 5+ GB ─────────────────────────────────────── */
              {'b',"large",
               "Mistral-7B          ~4.1 GB   classico, affidabile",
               "https://huggingface.co/TheBloke/Mistral-7B-Instruct-v0.2-GGUF/resolve/main/mistral-7b-instruct-v0.2.Q4_K_M.gguf",
               "mistral-7b-instruct-v0.2.Q4_K_M.gguf"},
              {'c',"large",
               "Llama-3.1-8B        ~4.7 GB   Meta, versatile",
               "https://huggingface.co/bartowski/Meta-Llama-3.1-8B-Instruct-GGUF/resolve/main/Meta-Llama-3.1-8B-Instruct-Q4_K_M.gguf",
               "Meta-Llama-3.1-8B-Instruct-Q4_K_M.gguf"},
              {'d',"large",
               "Llama-3.1-8B IT     ~5.0 GB   italiano nativo (Lexi)",
               "https://huggingface.co/mradermacher/Llama-3.1-8B-Lexi-Italian-GGUF/resolve/main/Llama-3.1-8B-Lexi-Italian.Q4_K_M.gguf",
               "Llama-3.1-8B-Lexi-Italian.Q4_K_M.gguf"},
              {'e',"large",
               "DeepSeek-Coder-V2   ~9.7 GB   codice avanzato (MoE 16B)",
               "https://huggingface.co/bartowski/DeepSeek-Coder-V2-Lite-Instruct-GGUF/resolve/main/DeepSeek-Coder-V2-Lite-Instruct-Q4_K_M.gguf",
               "DeepSeek-Coder-V2-Lite-Instruct-Q4_K_M.gguf"},
              {'f',"large",
               "Mistral-Nemo-12B    ~7.1 GB   uso generale avanzato",
               "https://huggingface.co/bartowski/Mistral-Nemo-Instruct-2407-GGUF/resolve/main/Mistral-Nemo-Instruct-2407-Q4_K_M.gguf",
               "Mistral-Nemo-Instruct-2407-Q4_K_M.gguf"},
              /* ── Matematica 70B — Xeon 64 GB RAM ─────────────────── */
              {'g',"math70b",
               "Qwen2.5-Math-72B  Q4_K_M  ~40 GB  \xe2\x96\xb6 entra in 64 GB RAM  [OPZIONE 2]",
               "https://huggingface.co/bartowski/Qwen2.5-Math-72B-Instruct-GGUF/resolve/main/Qwen2.5-Math-72B-Instruct-Q4_K_M.gguf",
               "Qwen2.5-Math-72B-Instruct-Q4_K_M.gguf"},
              {'h',"math70b",
               "Qwen2.5-Math-72B  Q8_0    ~74 GB  \xe2\x96\xb6 mmap + NVMe SSD     [OPZIONE 3]",
               "https://huggingface.co/bartowski/Qwen2.5-Math-72B-Instruct-GGUF/resolve/main/Qwen2.5-Math-72B-Instruct-Q8_0.gguf",
               "Qwen2.5-Math-72B-Instruct-Q8_0.gguf"},
            };
            int NMODS = (int)(sizeof(MODS)/sizeof(MODS[0]));

            clear_screen(); print_header();
            printf(BLD YLW "\n  \xf0\x9f\x93\xa5  Scarica Modello GGUF\n\n" RST);
            printf(GRY "  Tutti i link puntano a bartowski/TheBloke su Hugging Face (Q4_K_M).\n"
                       "  Il download avviene in background con wget \xe2\x80\x94 puoi continuare a usare il programma.\n\n" RST);

            const char* last_cat = "";
            for(int mi = 0; mi < NMODS; mi++){
                if(strcmp(MODS[mi].cat, last_cat) != 0){
                    last_cat = MODS[mi].cat;
                    if(strcmp(last_cat,"micro")==0)
                        printf(CYN "  \xe2\x94\x80\xe2\x94\x80 Microscopici  < 500 MB \xe2\x80\x94 RAM minima, CPU pura"
                               "  \xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\n" RST);
                    else if(strcmp(last_cat,"small")==0)
                        printf(CYN "  \xe2\x94\x80\xe2\x94\x80 Piccoli  500 MB\xe2\x80\x931.9 GB \xe2\x80\x94 qualit\xc3\xa0/peso ottimo"
                               "  \xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\n" RST);
                    else if(strcmp(last_cat,"med")==0)
                        printf(CYN "  \xe2\x94\x80\xe2\x94\x80 Medi  2\xe2\x80\x935 GB \xe2\x80\x94 qualit\xc3\xa0 alta"
                               "  \xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\n" RST);
                    else if(strcmp(last_cat,"large")==0)
                        printf(CYN "  \xe2\x94\x80\xe2\x94\x80 Grandi  5+ GB \xe2\x80\x94 GPU o molta RAM"
                               "  \xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\n" RST);
                    else if(strcmp(last_cat,"math70b")==0) {
                        printf(YLW "\n  \xe2\x94\x80\xe2\x94\x80 Matematica 70B \xe2\x80\x94 Xeon 64 GB RAM (senza GPU)"
                               "  \xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\n" RST);
                        printf(GRY "  Specializzato in matematica avanzata, analisi, calcolo simbolico e numerico.\n");
                        printf("  [g] Q4_K_M: carica direttamente in RAM (40 GB), usa --no-mmap per velocita massima.\n");
                        printf("  [h] Q8_0:   qualita superiore (74 GB), richiede NVMe SSD, usa mmap automaticamente.\n");
                        printf("  Flag consigliati: --ctx-size 8192 --threads <n_core_fisici>\n\n" RST);
                    }
                }
                printf("  " BLD "[%c]" RST "  %s\n", MODS[mi].k, MODS[mi].desc);
            }
            printf("\n  " BLD "[U]" RST "  URL personalizzato\n");
            printf("  " BLD "[0]" RST "  Annulla\n");
            printf("\n  Scelta: "); fflush(stdout);
            char sc_buf[8]; if(!fgets(sc_buf, sizeof sc_buf, stdin)) continue;
            sc_buf[strcspn(sc_buf,"\n")]='\0';
            int sc = (unsigned char)sc_buf[0];

            const char* url = NULL;
            const char* fname = NULL;
            for(int mi=0;mi<NMODS;mi++){
                if(sc_buf[0]==MODS[mi].k){ url=MODS[mi].url; fname=MODS[mi].fname; break; }
            }
            if(sc == 'u' || sc == 'U') {
                static char custom_url[1024];
                static char custom_fname[1024]; /* pari a custom_url: URL senza '/' diventa il nome */
                printf("  URL: "); fflush(stdout);
                if(!fgets(custom_url, sizeof custom_url, stdin)) continue;
                custom_url[strcspn(custom_url, "\n")] = '\0';
                if(!custom_url[0]) continue;
                /* Estrai nome file dall'URL */
                const char* slash = strrchr(custom_url, '/');
                snprintf(custom_fname, sizeof custom_fname, "%s", slash ? slash+1 : custom_url);
                url = custom_url; fname = custom_fname;
            }

            if(url && fname) {
                char dest[1600]; /* g_models_dir(511) + '/'(1) + fname (fino a 1023) + null */
                snprintf(dest, sizeof dest, "%s/%s", g_models_dir, fname);
                printf(CYN "\n  \xe2\x8f\xb3 Download: %s\n" RST, fname);
                printf(DIM "  Destinazione: %s\n" RST, dest);
                printf(DIM "  (il download avviene in background \xe2\x80\x94 non chiudere il terminale)\n\n" RST);
                fflush(stdout);

                /* Nota speciale per i modelli matematici 70B */
                int sel_mi = -1;
                for(int mi=0;mi<NMODS;mi++){
                    if(sc_buf[0]==MODS[mi].k){ sel_mi=mi; break; }
                }
                if(sel_mi >= 0 && strcmp(MODS[sel_mi].cat,"math70b")==0){
                    if(sc_buf[0]=='g'){
                        printf(YLW "  \xe2\x96\xb6 OPZIONE 2 \xe2\x80\x94 Q4_K_M (40 GB): entra in 64 GB RAM.\n");
                        printf("  Avvia llama-server con:\n");
                        printf(BLD "    llama-server -m %s/%s\\\n" RST, g_models_dir, fname);
                        printf(BLD "      --port 8081 --host 127.0.0.1\\\n" RST);
                        printf(BLD "      --no-mmap --ctx-size 8192 --threads 24\n\n" RST);
                        printf(GRY "  --no-mmap: carica tutto in RAM (piu veloce, ~40 GB usati).\n" RST);
                    } else if(sc_buf[0]=='h'){
                        printf(YLW "  \xe2\x96\xb6 OPZIONE 3 \xe2\x80\x94 Q8_0 (74 GB): richiede NVMe SSD per il paging.\n");
                        printf("  Avvia llama-server con:\n");
                        printf(BLD "    llama-server -m %s/%s\\\n" RST, g_models_dir, fname);
                        printf(BLD "      --port 8081 --host 127.0.0.1\\\n" RST);
                        printf(BLD "      --ctx-size 8192 --threads 24\n\n" RST);
                        printf(GRY "  mmap attivo (default): llama.cpp legge i layer dal SSD on-demand.\n");
                        printf("  Con NVMe PCIe 4: ~5-10 tok/s. Con SATA SSD: ~1-3 tok/s.\n" RST);
                    }
                    printf(GRY "  Il numero di --threads va adattato ai core fisici dello Xeon.\n\n" RST);
                }

                char cmd[2750]; /* url(1023) + dest(1600) + formato(~80) + margine */
                snprintf(cmd, sizeof cmd,
                         "wget -c --progress=bar:force \"%s\" -O \"%s\" 2>&1 &\n"
                         "echo \"  PID download: $!\"", url, dest);
                { int _rc = system(cmd); (void)_rc; }
                printf(GRY "\n  [INVIO per continuare] " RST); fflush(stdout);
                { char _b[4]; if(!fgets(_b,sizeof _b,stdin)){} }
            }
            continue;
        }

        /* ── Rimuovi ── */
        if((c == 'r' || c == 'R') && nm > 0) {
            printf("\n  Quale modello rimuovere? (1-%d, 0=annulla): ", nm);
            fflush(stdout);
            char buf[16]; if(!fgets(buf, sizeof buf, stdin)) continue;
            int ch = atoi(buf);
            if(ch < 1 || ch > nm) continue;

            /* Conferma */
            printf(RED "  Rimuovere '%s'? [s/N] " RST, gguf_files[ch-1]);
            fflush(stdout);
            char yn[8]; if(!fgets(yn, sizeof yn, stdin)) continue;
            if(yn[0] != 's' && yn[0] != 'S') continue;

            /* Scarica dalla memoria se e' il modello attivo */
            if(lw_is_loaded() && strcmp(gguf_files[ch-1], lw_model_name()) == 0) {
                lw_free();
                printf(YLW "  Modello scaricato dalla memoria.\n" RST);
            }

            char path[1024];
            snprintf(path, sizeof path, "%s/%s", g_models_dir, gguf_files[ch-1]);
            if(remove(path) == 0)
                printf(GRN "  \xe2\x9c\x93 '%s' rimosso.\n" RST, gguf_files[ch-1]);
            else
                printf(RED "  \xe2\x9c\x97 Errore rimozione: %s\n" RST, strerror(errno));

            printf(GRY "  [INVIO per continuare] " RST); fflush(stdout);
            { char _b[4]; if(!fgets(_b,sizeof _b,stdin)){} }
        }
    }
}

/* Voce unificata: seleziona + scarica + rimuovi modelli */
void menu_modelli(void) {
    if (g_ctx.backend == BACKEND_LLAMA) {
        /* ── llama-statico: .gguf con dimensione + D/R ── */
        gestisci_modelli(); /* include gia selezione + download + rimozione */
    } else {
        /* ── HTTP: seleziona dal server ── */
        choose_model();
    }
}
