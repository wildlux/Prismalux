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

void load_gguf_model(const char* filename) {
    char path[768];
    snprintf(path,sizeof path,"%s/%s",g_models_dir,filename);

    /* Controlla che il file non sia troppo piccolo (download incompleto) */
    long long file_sz = 0;
    FILE* tf = fopen(path, "rb");
    if(tf) {
        fseek(tf, 0, SEEK_END);
        file_sz = ftell(tf);
        fclose(tf);
        if(file_sz < 1024 * 1024) {
            printf(RED "\n  \xe2\x9c\x97 File troppo piccolo (%lld KB) \xe2\x80\x94 download ancora in corso?\n" RST, file_sz/1024);
            printf(DIM "  Attendi che wget/curl finisca prima di caricare il modello.\n\n" RST);
            fflush(stdout);
            return;
        }
        /* Controlla se wget sta ancora scrivendo questo file */
        {
            char check_cmd[640];
            snprintf(check_cmd, sizeof check_cmd,
                     "lsof \"%s\" 2>/dev/null | grep -q wget", path);
            if(system(check_cmd) == 0) {
                printf(YLW "\n  \xe2\x9a\xa0  Download ancora in corso! (%.1f GB scaricati finora)\n" RST,
                       (double)file_sz/(1024*1024*1024));
                printf("  Attendi che wget finisca prima di caricare il modello.\n");
                printf("  Monitora: watch -n3 'ls -lh models/*.gguf'\n\n");
                fflush(stdout);
                return;
            }
        }
    }

    /* Calcolo n_layers anticipato (serve sia per check VRAM che per lw_init) */
    int n_layers = (g_hw_ready && g_hw.dev[g_hw.primary].type != DEV_CPU)
                   ? g_hw.dev[g_hw.primary].n_gpu_layers : 0;

    /* ── Controllo RAM prima del caricamento ───────────────────────── */
    if (file_sz > 0) {
        long long file_mb    = file_sz / (1024LL * 1024LL);
        int       tot_layers = estimate_total_layers(file_mb);

        /* Quota CPU: se n_layers>=9999 tutto su GPU → RAM solo per overhead.
         * Altrimenti la parte CPU = (tot_layers - gpu_layers) / tot_layers × file */
        long long cpu_layers = (n_layers == 0) ? tot_layers :
                               (n_layers >= 9999) ? 0 :
                               tot_layers - n_layers;
        long long ram_needed_mb = (n_layers >= 9999)
            ? file_mb * 15 / 100           /* solo overhead: ~15% per contesto/KV */
            : (file_mb * cpu_layers / tot_layers) * 120 / 100;
        if (ram_needed_mb < 256) ram_needed_mb = 256; /* minimo OS overhead */

        long long ram_avail_mb = ram_available_mb_live();

        if (ram_avail_mb > 0 && ram_avail_mb < ram_needed_mb) {
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
                    return;
                }
            }
        }
    }

    /* ── Controllo VRAM (solo se usiamo layer GPU) ─────────────────── */
    if (n_layers != 0 && file_sz > 0 && g_hw_ready) {
        const HWDevice* pd = &g_hw.dev[g_hw.primary];
        if (pd->type != DEV_CPU) {
            long long file_mb    = file_sz / (1024LL * 1024LL);
            int       tot_layers = estimate_total_layers(file_mb);
            int       gpu_l      = (n_layers >= 9999) ? tot_layers : n_layers;
            if (gpu_l > tot_layers) gpu_l = tot_layers;

            /* VRAM stimata = quota GPU del file + 10% overhead */
            long long vram_needed_mb = (file_mb * gpu_l / tot_layers) * 110 / 100;
            long long vu_=0, vt_=0;
            read_vram_mb(pd->gpu_index, pd->type, &vu_, &vt_);
            long long vram_free = (vt_ > vu_) ? vt_ - vu_ : 0;

            if (vram_free > 0 && vram_free < vram_needed_mb) {
                printf(YLW "\n  \xe2\x9a\xa0  VRAM insufficiente per %d layer GPU!\n" RST, gpu_l);
                printf("     VRAM necessaria stimata : ~%lld MB (%.1f GB)\n", vram_needed_mb, vram_needed_mb/1024.0);
                printf("     VRAM libera ora         :  %lld MB (%.1f GB)\n", vram_free,       vram_free/1024.0);
                printf("  [%s] %s\n\n", hw_dev_type_str(pd->type), pd->name);

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

                /* Ri-controlla dopo eventuale scaricamento */
                if (vram_free > 0 && vram_free < vram_needed_mb) {
                    printf("  Opzioni:\n");
                    printf("  " BLD "1" RST "  Ridurre layer GPU (carica solo quanto entra in VRAM)\n");
                    printf("  " BLD "2" RST "  CPU-only \xe2\x80\x94 0 layer GPU (pi\xc3\xb9 lento, zero VRAM)\n");
                    printf("  " BLD "3" RST "  Tentare comunque (rischio blocco sistema)\n");
                    printf("  " BLD "0" RST "  Annulla\n\n");
                    printf("  Scelta: "); fflush(stdout);
                    char yn[8];
                    if (!fgets(yn, sizeof yn, stdin)) yn[0] = '0';
                    if (yn[0] == '1') {
                        /* Layer sicuri = VRAM_libera*90% / MB_per_layer */
                        long long mb_per_layer = (file_mb / tot_layers) > 0
                                                 ? file_mb / tot_layers : 1;
                        long long safe = (vram_free * 90 / 100) / mb_per_layer;
                        if (safe < 1) safe = 1;
                        n_layers = (int)safe;
                        printf(GRN "  \xe2\x9c\x93 Layer GPU ridotti a %d (~%lld MB VRAM stimata)\n\n" RST,
                               n_layers, mb_per_layer * n_layers);
                    } else if (yn[0] == '2') {
                        n_layers = 0;
                        printf(GRN "  \xe2\x9c\x93 Modalita CPU-only (0 layer GPU)\n\n" RST);
                    } else if (yn[0] == '3') {
                        printf(YLW "  \xe2\x9a\xa0  Procedo. Se il sistema si blocca, usa CPU-only.\n\n" RST);
                    } else {
                        printf(DIM "  Caricamento annullato.\n\n" RST); fflush(stdout);
                        return;
                    }
                }
            }
        }
    }

    printf("\n  " YLW "\xe2\x8f\xb3 Caricamento: %s" RST "\n",filename);
    printf("  " DIM "(llama.cpp \xe2\x80\x94 qualche secondo...)" RST "\n"); fflush(stdout);
    if(lw_init(path, n_layers, 4096)==0) {
        salva_config();
        printf("  " GRN "\xe2\x9c\x93 Modello pronto! (GPU layers: %d)" RST "\n\n", n_layers);
    } else { printf("  " RED "\xe2\x9c\x97 Errore caricamento \xe2\x80\x94 il file potrebbe essere corrotto o incompleto.\n" RST
                  "  Se il download era in corso, attendi che finisca e riprova.\n\n" RST); }
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
    if(W<60)W=60; if(W>130)W=130;
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
            box_lft(W, RED " \xe2\x9c\x97  Nessun modello trovato (server offline?)" RST);
            char dl[128];
            snprintf(dl,sizeof dl," Indirizzo: %s @ %s:%d",
                     backend_name(g_ctx.backend),g_ctx.host,g_ctx.port);
            box_lft(W,dl);
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

/* ── Gestione modelli .gguf (scarica / rimuovi / info) ── */
void gestisci_modelli(void) {
    while(1) {
        clear_screen(); print_header();
        printf(BLD YLW "\n  \xf0\x9f\x93\xa6 Gestione Modelli llama.cpp\n" RST);
        printf(DIM "  Cartella: %s/\n\n" RST, g_models_dir);

        char gguf_files[MAX_MODELS][256];
        int nm = scan_gguf_dir(g_models_dir, gguf_files, MAX_MODELS);

        if(nm == 0) {
            printf(YLW "  Nessun modello .gguf presente.\n\n" RST);
        } else {
            printf(GRY "  %-4s  %-48s  %s\n" RST, "N.", "Nome file", "Dimensione");
            printf(GRY "  %-4s  %-48s  %s\n" RST, "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80", "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80", "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80");
            for(int i = 0; i < nm; i++) {
                /* Calcola dimensione file */
                char path[512];
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
        if(nm > 0)
            printf("  " BLD "R" RST "  Rimuovi modello\n");
        printf("  " EM_0 "  Torna\n");

        printf("\n  Scelta: "); fflush(stdout);

        int c = getch_single(); printf("%c\n", c);
        if(c == '0' || c == 27) break;

        /* ── Scarica ── */
        if(c == 'd' || c == 'D') {
            /* Tabella modelli {chiave, descrizione, url, filename} */
            static const struct { const char* k; const char* desc; const char* url; const char* fname; } MODS[] = {
              {"1","Qwen2.5-Math-7B         ~4.7GB  matematica avanzata",
               "https://huggingface.co/bartowski/Qwen2.5-Math-7B-Instruct-GGUF/resolve/main/Qwen2.5-Math-7B-Instruct-Q4_K_M.gguf",
               "Qwen2.5-Math-7B-Instruct-Q4_K_M.gguf"},
              {"2","DeepSeek-Math-7B        ~4.7GB  matematica (alternativa)",
               "https://huggingface.co/bartowski/DeepSeek-Math-7B-Instruct-GGUF/resolve/main/DeepSeek-Math-7B-Instruct-Q4_K_M.gguf",
               "DeepSeek-Math-7B-Instruct-Q4_K_M.gguf"},
              {"3","NuminaMath-7B           ~4.7GB  matematica olimpica",
               "https://huggingface.co/bartowski/NuminaMath-7B-v1.1-GGUF/resolve/main/NuminaMath-7B-v1.1-Q4_K_M.gguf",
               "NuminaMath-7B-v1.1-Q4_K_M.gguf"},
              {"4","Qwen2.5-Coder-7B       ~4.7GB  programmazione",
               "https://huggingface.co/bartowski/Qwen2.5-Coder-7B-Instruct-GGUF/resolve/main/Qwen2.5-Coder-7B-Instruct-Q4_K_M.gguf",
               "Qwen2.5-Coder-7B-Instruct-Q4_K_M.gguf"},
              {"5","Llama-3.1-8B-Lexi-IT   ~5.0GB  italiano nativo",
               "https://huggingface.co/mradermacher/Llama-3.1-8B-Lexi-Italian-GGUF/resolve/main/Llama-3.1-8B-Lexi-Italian.Q4_K_M.gguf",
               "Llama-3.1-8B-Lexi-Italian-Q4_K_M.gguf"},
              {"6","DeepSeek-Coder-V2-16B  ~9.7GB  codice avanzato (MoE)",
               "https://huggingface.co/bartowski/DeepSeek-Coder-V2-Lite-Instruct-GGUF/resolve/main/DeepSeek-Coder-V2-Lite-Instruct-Q4_K_M.gguf",
               "DeepSeek-Coder-V2-Lite-Instruct-Q4_K_M.gguf"},
              {"7","Mistral-Nemo-12B        ~7.1GB  uso generale",
               "https://huggingface.co/bartowski/Mistral-Nemo-Instruct-2407-GGUF/resolve/main/Mistral-Nemo-Instruct-2407-Q4_K_M.gguf",
               "Mistral-Nemo-Instruct-2407-Q4_K_M.gguf"},
              {"8","Qwen2.5-3B             ~2.0GB  leggero, uso generale",
               "https://huggingface.co/bartowski/Qwen2.5-3B-Instruct-GGUF/resolve/main/Qwen2.5-3B-Instruct-Q4_K_M.gguf",
               "Qwen2.5-3B-Instruct-Q4_K_M.gguf"},
            };
            int NMODS = (int)(sizeof(MODS)/sizeof(MODS[0]));
            static const char* _EM[] = {EM_0,EM_1,EM_2,EM_3,EM_4,EM_5,EM_6,EM_7,EM_8,EM_9};
            printf("\n  Modelli disponibili:\n");
            for(int mi=0;mi<NMODS;mi++)
                printf("  %s  %s\n", _EM[mi+1], MODS[mi].desc);
            printf("  " BLD "U" RST "  URL personalizzato\n");
            printf("  " EM_0 "  Annulla\n");
            printf("\n  Scelta: "); fflush(stdout);
            char sc_buf[8]; if(!fgets(sc_buf, sizeof sc_buf, stdin)) continue;
            sc_buf[strcspn(sc_buf,"\n")]='\0';
            int sc = sc_buf[0];

            const char* url = NULL;
            const char* fname = NULL;
            for(int mi=0;mi<NMODS;mi++){
                if(sc_buf[0]==MODS[mi].k[0]){ url=MODS[mi].url; fname=MODS[mi].fname; break; }
            }
            if(sc == 'u' || sc == 'U') {
                static char custom_url[1024];
                static char custom_fname[256];
                printf("  URL: "); fflush(stdout);
                if(!fgets(custom_url, sizeof custom_url, stdin)) continue;
                custom_url[strcspn(custom_url, "\n")] = '\0';
                if(!custom_url[0]) continue;
                /* Estrai nome file dall'URL */
                const char* slash = strrchr(custom_url, '/');
                strncpy(custom_fname, slash ? slash+1 : custom_url, 255);
                url = custom_url; fname = custom_fname;
            }

            if(url && fname) {
                char dest[512];
                snprintf(dest, sizeof dest, "%s/%s", g_models_dir, fname);
                printf(CYN "\n  \xe2\x8f\xb3 Download: %s\n" RST, fname);
                printf(DIM "  Destinazione: %s\n" RST, dest);
                printf(DIM "  (il download avviene in background \xe2\x80\x94 non chiudere il terminale)\n\n" RST);
                fflush(stdout);
                char cmd[2048];
                snprintf(cmd, sizeof cmd,
                         "wget -c --progress=bar:force \"%s\" -O \"%s\" 2>&1 &\n"
                         "echo \"  PID download: $!\"", url, dest);
                (void)system(cmd);
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

            char path[512];
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
