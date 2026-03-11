/*
 * main.c — SEZIONE 12: Menu principale, config JSON, main()
 * ===========================================================
 * "Costruito per i mortali che aspirano alla saggezza."
 *
 * Supporta 3 backend AI selezionabili a runtime:
 *   --backend llama        → libllama statica (richiede ./build.sh)
 *   --backend ollama       → HTTP a Ollama    (default porta 11434)
 *   --backend llama-server → HTTP a llama-server OpenAI API (default porta 8080)
 */
#include "common.h"
#include "backend.h"
#include "terminal.h"
#include "http.h"
#include "ai.h"
#include "modelli.h"
#include "output.h"
#include "multi_agente.h"
#include "strumenti.h"
#include "prismalux_ui.h"
#include "hw_detect.h"

/* ══════════════════════════════════════════════════════════════
   Voce 1 — Agenti AI (sottomenu: Pipeline 6 agenti + Motore Byzantino)
   ══════════════════════════════════════════════════════════════ */
static void menu_agenti(void) {
    while(1){
        clear_screen(); print_header();
        printf(CYN BLD "\n  \xf0\x9f\xa4\x96  Agenti AI\n\n" RST);
        printf("  " EM_1 "  \xf0\x9f\x94\x84  Pipeline 6 Agenti     "
               GRY "ricercatore \xe2\x86\x92 pianificatore \xe2\x86\x92 2 prog \xe2\x86\x92 tester \xe2\x86\x92 ottimizzatore" RST "\n");
        printf("  " EM_2 "  \xf0\x9f\x94\xae  Motore Byzantino      "
               GRY "verifica a 4 agenti anti-allucinazione (A,B,C,D)" RST "\n");
        printf("  " EM_0 "  Torna\n\n  " GRN "Scelta: " RST); fflush(stdout);
        int c = getch_single(); printf("%c\n", c);
        if      (c == '1') menu_multi_agente();
        else if (c == '2') menu_byzantine_engine();
        else if (c == '0' || c == 'q' || c == 'Q' || c == 27) break;
    }
}

/* ══════════════════════════════════════════════════════════════
   Voce 3 — Impara con AI
   ══════════════════════════════════════════════════════════════ */
static void menu_impara(void) {
    while(1){
        clear_screen(); print_header();
        printf(CYN BLD "\n  \xf0\x9f\x93\x9a  Impara con AI\n\n" RST);
        printf("  " EM_1 "  \xf0\x9f\x8f\x9b\xef\xb8\x8f  Tutor AI \xe2\x80\x94 Oracolo di Prismalux\n");
        printf("  " EM_2 "  \xf0\x9f\x93\x9d  Quiz Interattivi          " GRY "(prossimamente)" RST "\n");
        printf("  " EM_3 "  \xf0\x9f\x93\x8a  Dashboard Statistica      " GRY "(prossimamente)" RST "\n");
        printf("  " EM_4 "  \xf0\x9f\x93\x88  Analisi Dati con AI       " GRY "(prossimamente)" RST "\n");
        printf("  " EM_0 "  Torna\n\n  " GRN "Scelta: " RST); fflush(stdout);
        int c=getch_single(); printf("%c\n",c);
        if(c=='1') menu_tutor();
        else if(c=='2'||c=='3'||c=='4'){
            clear_screen(); print_header();
            printf(YLW "\n  \xf0\x9f\x8c\xab\xef\xb8\x8f  La nebbia copre la verit\xc3\xa0. Riprova pi\xc3\xb9 tardi.\n\n" RST);
            printf(GRY "  (funzionalit\xc3\xa0 in sviluppo)\n\n" RST);
            printf(GRY "  [INVIO per continuare] " RST); fflush(stdout);
            { char _b[4]; if(!fgets(_b,sizeof _b,stdin)){} }
        }
        else if(c=='0'||c=='q'||c=='Q'||c==27) break;
    }
}

/* ══════════════════════════════════════════════════════════════
   Voce 4 — Personalizzazione Avanzata Estrema
   ══════════════════════════════════════════════════════════════ */
static void menu_personalizzazione(void) {
    while(1){
        clear_screen(); print_header();
        printf(CYN BLD "\n  \xf0\x9f\x94\xa9  Personalizzazione Avanzata Estrema\n\n" RST);
        printf("  " EM_1 "  \xf0\x9f\x94\xac  VRAM Benchmark            " GRY "(vram_bench)" RST "\n");
        printf("  " EM_2 "  \xf0\x9f\xa6\x99  llama.cpp Studio          " GRY "(compila, gestisci, avvia)" RST "\n");
        printf("  " EM_3 "  \xe2\x9a\xa1  Cython Studio             " GRY "(ottimizza codice Python)" RST "\n");
        printf("  " EM_0 "  Torna\n\n  " GRN "Scelta: " RST); fflush(stdout);
        int c=getch_single(); printf("%c\n",c);
        if(c=='1'){
            /* vram_bench e' un eseguibile separato */
            clear_screen(); print_header();
            printf(CYN BLD "\n  \xf0\x9f\x94\xac  VRAM Benchmark\n\n" RST);
            printf("  Esegui dall'esterno:\n\n");
            printf(YLW "    ./vram_bench" RST "                         # profila tutti i modelli Ollama\n");
            printf(YLW "    ./vram_bench --model mistral:7b" RST "      # solo un modello\n");
            printf(YLW "    ./vram_bench --output profilo.json" RST "\n\n");
            printf(GRY "  Compila con: " YLW "make vram_bench\n\n" RST);
            printf(GRY "  [INVIO per continuare] " RST); fflush(stdout);
            { char _b[4]; if(!fgets(_b,sizeof _b,stdin)){} }
        }
        else if(c=='2'||c=='3'){
            clear_screen(); print_header();
            printf(YLW "\n  \xf0\x9f\x8c\xab\xef\xb8\x8f  La nebbia copre la verit\xc3\xa0. Riprova pi\xc3\xb9 tardi.\n\n" RST);
            printf(GRY "  (funzionalit\xc3\xa0 in sviluppo)\n\n" RST);
            printf(GRY "  [INVIO per continuare] " RST); fflush(stdout);
            { char _b[4]; if(!fgets(_b,sizeof _b,stdin)){} }
        }
        else if(c=='0'||c=='q'||c=='Q'||c==27) break;
    }
}

/* ══════════════════════════════════════════════════════════════
   Voce 5 — Manutenzione
   ══════════════════════════════════════════════════════════════ */
static void menu_manutenzione(void) {
    while(1){
        clear_screen(); print_header();
        printf(CYN BLD "\n  \xf0\x9f\x94\xa7  Manutenzione\n\n" RST);
        {
            const char *bkname  = backend_name(g_ctx.backend);
            const char *modname = (g_ctx.backend==BACKEND_LLAMA)
                ? (lw_is_loaded()?lw_model_name():"nessun modello")
                : (g_ctx.model[0]?g_ctx.model:"nessun modello");
            printf("  " EM_1 "  Modelli                 " GRY "attuale: %.40s" RST "\n", modname);
            printf("  " EM_2 "  Backend & Rete          " GRY "attuale: %s @ %s:%d" RST "\n",
                   bkname, g_ctx.host, g_ctx.port);
        }
        printf("  " EM_3 "  Info Hardware           " GRY "CPU, GPU, RAM, VRAM" RST "\n");
        printf("  " EM_0 "  Torna\n\n  " GRN "Scelta: " RST); fflush(stdout);
        int c=getch_single(); printf("%c\n",c);
        if     (c=='1') menu_modelli();
        else if(c=='2') choose_backend();
        else if(c=='3'){
            clear_screen(); print_header();
            printf(CYN BLD "\n  \xf0\x9f\x96\xa5\xef\xb8\x8f  Info Hardware\n\n" RST);
            hw_print(&g_hw);
            printf(GRY "\n  [INVIO per continuare] " RST); fflush(stdout);
            { char _b[4]; if(!fgets(_b,sizeof _b,stdin)){} }
        }
        else if(c=='0'||c=='q'||c=='Q'||c==27) break;
    }
}

/* ══════════════════════════════════════════════════════════════
   SEZIONE CONFIG — Salvataggio/caricamento impostazioni
   ══════════════════════════════════════════════════════════════ */

static char g_config_path[512] = "";

static void config_path_init(void) {
    if(g_config_path[0]) return;
    const char *home = getenv("HOME");
#ifdef _WIN32
    if(!home) home = getenv("USERPROFILE");
#endif
    if(home)
        snprintf(g_config_path, sizeof g_config_path,
                 "%s/.prismalux_config.json", home);
    else
        strncpy(g_config_path, ".prismalux_config.json",
                sizeof g_config_path - 1);
}

void salva_config(void) {
    config_path_init();
    FILE *f = fopen(g_config_path, "w"); if(!f) return;
    const char *bk =
        (g_ctx.backend == BACKEND_OLLAMA)      ? "ollama"       :
        (g_ctx.backend == BACKEND_LLAMASERVER) ? "llama-server" : "llama";
    /* Aggiorna profilo corrente prima di scrivere */
    _sync_to_profile();
    /* Profilo llama: preferisce il modello caricato */
    const char *llm = lw_is_loaded() ? lw_model_name()
                    : (g_prof_llama_model[0] ? g_prof_llama_model : "");
    char e_om[256]="", e_lsm[256]="", e_llm[256]="";
    js_encode(g_prof_ollama_model,  e_om,  sizeof e_om);
    js_encode(g_prof_lserver_model, e_lsm, sizeof e_lsm);
    js_encode(llm,                  e_llm, sizeof e_llm);
    fprintf(f,
        "{\n"
        "  \"backend\": \"%s\",\n"
        "  \"ollama_host\": \"%s\",\n"
        "  \"ollama_port\": %d,\n"
        "  \"ollama_model\": \"%s\",\n"
        "  \"lserver_host\": \"%s\",\n"
        "  \"lserver_port\": %d,\n"
        "  \"lserver_model\": \"%s\",\n"
        "  \"llama_model\": \"%s\"\n"
        "}\n",
        bk,
        g_prof_ollama_host, g_prof_ollama_port, e_om,
        g_prof_lserver_host, g_prof_lserver_port, e_lsm,
        e_llm);
    fclose(f);
}

static void carica_config(void) {
    config_path_init();
    FILE *f = fopen(g_config_path, "r"); if(!f) return;
    char buf[4096]; int n=(int)fread(buf,1,sizeof buf-1,f); fclose(f);
    if(n<=0) return; buf[n]='\0';

    const char *v;
    char vbk[32]="";
    v=js_str(buf,"backend"); if(v) strncpy(vbk, v, sizeof vbk-1);

    /* ── Profilo Ollama ── */
    v=js_str(buf,"ollama_host");  if(v&&v[0]) strncpy(g_prof_ollama_host, v, sizeof g_prof_ollama_host-1);
    v=js_str(buf,"ollama_model"); if(v&&v[0]) strncpy(g_prof_ollama_model,v, sizeof g_prof_ollama_model-1);
    { const char *pp=strstr(buf,"\"ollama_port\":");
      if(pp){ pp+=14; while(*pp==' ')pp++; int p=atoi(pp); if(p>0) g_prof_ollama_port=p; } }

    /* ── Profilo llama-server ── */
    v=js_str(buf,"lserver_host");  if(v&&v[0]) strncpy(g_prof_lserver_host, v, sizeof g_prof_lserver_host-1);
    v=js_str(buf,"lserver_model"); if(v&&v[0]) strncpy(g_prof_lserver_model,v, sizeof g_prof_lserver_model-1);
    { const char *pp=strstr(buf,"\"lserver_port\":");
      if(pp){ pp+=15; while(*pp==' ')pp++; int p=atoi(pp); if(p>0) g_prof_lserver_port=p; } }

    /* ── Profilo llama-statico ── */
    v=js_str(buf,"llama_model"); if(v&&v[0]) strncpy(g_prof_llama_model, v, sizeof g_prof_llama_model-1);

    /* ── Ripristina g_ctx dal backend attivo ── */
    if(vbk[0]){
        if     (strcmp(vbk,"ollama"      )==0) g_ctx.backend=BACKEND_OLLAMA;
        else if(strcmp(vbk,"llama-server")==0) g_ctx.backend=BACKEND_LLAMASERVER;
        else                                    g_ctx.backend=BACKEND_LLAMA;
    }
    if(g_ctx.backend==BACKEND_OLLAMA){
        strncpy(g_ctx.host,  g_prof_ollama_host,  sizeof g_ctx.host-1);
        g_ctx.port = g_prof_ollama_port;
        strncpy(g_ctx.model, g_prof_ollama_model, sizeof g_ctx.model-1);
    } else if(g_ctx.backend==BACKEND_LLAMASERVER){
        strncpy(g_ctx.host,  g_prof_lserver_host,  sizeof g_ctx.host-1);
        g_ctx.port = g_prof_lserver_port;
        strncpy(g_ctx.model, g_prof_lserver_model, sizeof g_ctx.model-1);
    } else {
        g_ctx.port = 0;
        strncpy(g_ctx.model, g_prof_llama_model, sizeof g_ctx.model-1);
    }
}

/* ══════════════════════════════════════════════════════════════
   SEZIONE 12 — Menu principale
   ══════════════════════════════════════════════════════════════ */

static void menu_principale(void) {
    while(1){
        _sync_hdr_ctx();
        clear_screen();
        print_header();

        int tw = term_width();
        int W  = tw - 2;
        if(W < 60)  W = 60;
        if(W > 130) W = 130;

        /* ── helpers locali ──────────────────────────────────── */
#define MROW(key,icon,nome,desc) do { \
    char _b[320]; \
    snprintf(_b,sizeof _b, " %s  %s  " BLD "%-30s" RST "  " GRY "%s" RST, \
             key,icon,nome,desc); \
    box_lft(W,_b); \
} while(0)

        box_top(W);

        /* ── Voci menu ───────────────────────────────────────── */
        box_emp(W);
        MROW(EM_1, "\xf0\x9f\xa4\x96", "Agenti AI",
             "Pipeline 6 agenti | Motore Byzantino anti-allucinazione");
        MROW(EM_2, "\xf0\x9f\x9b\xa0\xef\xb8\x8f", "Strumento Pratico",
             "730, Partita IVA, Cerca Lavoro, CV Reader");
        MROW(EM_3, "\xf0\x9f\x93\x9a", "Impara con AI",
             "Tutor Oracolo, Quiz interattivi, Dashboard, Analisi dati");
        MROW(EM_4, "\xf0\x9f\x94\xa9", "Personalizzazione Avanzata Estrema",
             "VRAM Benchmark, llama.cpp Studio, Cython Studio");
        MROW(EM_5, "\xf0\x9f\x94\xa7", "Manutenzione",
             "Modelli, Backend, HW info");
        box_emp(W);

        /* ── avviso RAM critica (se <512 MB liberi) ──────────── */
        {
#ifdef _WIN32
            MEMORYSTATUSEX ms; ms.dwLength = sizeof ms;
            GlobalMemoryStatusEx(&ms);
            long long ra = (long long)(ms.ullAvailPhys / (1024ULL * 1024ULL));
#else
            long long ra = 0;
            FILE* mf = fopen("/proc/meminfo", "r");
            if(mf) {
                char line[256];
                while(fgets(line, sizeof line, mf)) {
                    unsigned long long v;
                    if(sscanf(line, "MemAvailable: %llu", &v) == 1) { ra = (long long)(v/1024); break; }
                }
                fclose(mf);
            }
#endif
            if(ra>0 && ra<512){
                char w[160];
                snprintf(w,sizeof w,
                    " " RED "\xe2\x9a\xa0" RST "  RAM libera: %lld MB  \xe2\x80\x94  "
                    "scarica il modello corrente!", ra);
                box_lft(W,w);
            }
        }

        /* ── barra stato in basso ────────────────────────────── */
        box_sep(W);
        {
            const char *footer =
                "  " YLW "[Q]" RST " \xf0\x9f\x9a\xaa Esci  "
                "\xe2\x94\x82  " YLW "[ESC]" RST " esci subito  "
                "\xe2\x94\x82  Premi " EM_1 "\xe2\x80\x93" EM_5 " per selezionare  ";
            int dl=disp_len(footer);
            int pad=W-dl; if(pad<0)pad=0;
            printf(CYN "\xe2\x95\x91" RST "\033[7m%s",footer);
            for(int i=0;i<pad;i++) putchar(' ');
            printf("\033[27m" CYN "\xe2\x95\x91\n" RST);
        }
        box_bot(W);

        printf("\n"); fflush(stdout);

#undef MROW
        int c=getch_single();

        if(c==27){ /* ESC = esci subito */
            salva_config();
            clear_screen();
            printf(CYN BLD "\n  \xf0\x9f\x8d\xba Alla prossima libagione di sapere.\n\n" RST);
            return;
        }
        switch(c){
            case '1': menu_agenti();             break;
            case '2': menu_pratico();            break;
            case '3': menu_impara();             break;
            case '4': menu_personalizzazione();  break;
            case '5': menu_manutenzione();       break;
            case 'q': case 'Q':
                salva_config();
                clear_screen();
                printf(CYN BLD "\n  \xf0\x9f\x8d\xba Alla prossima libagione di sapere.\n\n" RST);
                return;
        }
    }
}

static void sigint_handler(int sig) {
    (void)sig;
    salva_config();
#ifndef _WIN32
    term_restore();
#endif
    printf(RST "\n  " GRY "[Ctrl+C] Configurazione salvata. Uscita.\n" RST);
    exit(0);
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    HANDLE hOut=GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode=0; GetConsoleMode(hOut,&mode);
    SetConsoleMode(hOut,mode|ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif

    signal(SIGINT, sigint_handler);

    /* ── Carica impostazioni salvate (le args la sovrascrivo) ─── */
    carica_config();

    /* ── Parsing argomenti ────────────────────────────────────── */
    for(int i=1;i<argc;i++){
        if(strcmp(argv[i],"--backend")==0&&i+1<argc){
            i++;
            if(strcmp(argv[i],"ollama")==0){
                g_ctx.backend=BACKEND_OLLAMA;
                if(!g_ctx.port)g_ctx.port=11434;
            }else if(strcmp(argv[i],"llama-server")==0){
                g_ctx.backend=BACKEND_LLAMASERVER;
                if(!g_ctx.port)g_ctx.port=8080;
            }else if(strcmp(argv[i],"llama")==0){
                g_ctx.backend=BACKEND_LLAMA;
            }else{
                fprintf(stderr,"Backend non valido: %s\n(usa: llama | ollama | llama-server)\n",argv[i]);
                return 1;
            }
        }
        else if(strcmp(argv[i],"--host")==0&&i+1<argc){
            strncpy(g_ctx.host,argv[++i],sizeof g_ctx.host-1);
        }
        else if(strcmp(argv[i],"--port")==0&&i+1<argc){
            g_ctx.port=atoi(argv[++i]);
        }
        else if(strcmp(argv[i],"--model")==0&&i+1<argc){
            strncpy(g_ctx.model,argv[++i],sizeof g_ctx.model-1);
        }
        else if(strcmp(argv[i],"--models-dir")==0&&i+1<argc){
            strncpy(g_models_dir,argv[++i],sizeof g_models_dir-1);
        }
        else if(strcmp(argv[i],"--help")==0||strcmp(argv[i],"-h")==0){
            printf("Prismalux v" VERSION "\n\n"
                   "Uso: %s [opzioni]\n\n"
                   "  --backend  llama|ollama|llama-server\n"
                   "  --host     HOST  (default: 127.0.0.1)\n"
                   "  --port     PORT  (ollama: 11434, llama-server: 8080)\n"
                   "  --model    NOME  (es: llama3.2, mistral)\n"
                   "  --models-dir DIR (default: ./models)\n\n"
                   "Esempi:\n"
                   "  %s --backend ollama\n"
                   "  %s --backend llama-server --port 8080\n"
                   "  %s --backend ollama --model deepseek-coder:33b\n",
                   argv[0],argv[0],argv[0],argv[0]);
            return 0;
        }
    }

    /* ── Porta di default se non specificata ─────────────────── */
    if(g_ctx.port==0){
        g_ctx.port=(g_ctx.backend==BACKEND_LLAMASERVER)?8080:11434;
    }

    clear_screen();
    printf(CYN BLD "\n  \xf0\x9f\x8d\xba Invocazione riuscita. Gli dei ascoltano.\n\n" RST);
    printf(DIM "  Backend: %s", backend_name(g_ctx.backend));
    if(g_ctx.backend!=BACKEND_LLAMA) printf("  \xe2\x86\x92  %s:%d",g_ctx.host,g_ctx.port);
    printf("\n" RST);

    /* ── Init backend ────────────────────────────────────────── */
    if(g_ctx.backend==BACKEND_LLAMA){
        char gguf_files[MAX_MODELS][256];
        int nm=scan_gguf_dir(g_models_dir,gguf_files,MAX_MODELS);
        if(nm>0){
            printf(GRY "  Trovati %d modello/i in %s/\n" RST,nm,g_models_dir);
            if(nm==1){
                printf(GRY "  Auto-caricamento: %s\n" RST,gguf_files[0]);
                load_gguf_model(gguf_files[0]);
            }else{
                printf(GRY "  Usa Opzione 4 per scegliere il modello.\n" RST);
            }
        }else{
            printf(YLW "  Nessun .gguf in %s/\n" RST,g_models_dir);
            printf(DIM "  Usa --backend ollama per avvio senza compilazione.\n" RST);
        }
    }else{
        /* HTTP: testa connettivita' */
        sock_t test=tcp_connect(g_ctx.host,g_ctx.port);
        if(test!=SOCK_INV){
            printf(GRN "  \xe2\x9c\x93 Connesso a %s:%d\n" RST,g_ctx.host,g_ctx.port);
            sock_close(test);
            /* Auto-popola lista modelli */
            char http_models[MAX_MODELS][128]; int nm=0;
            if(g_ctx.backend==BACKEND_OLLAMA)
                nm=http_ollama_list(g_ctx.host,g_ctx.port,http_models,MAX_MODELS);
            else
                nm=http_llamaserver_list(g_ctx.host,g_ctx.port,http_models,MAX_MODELS);
            if(nm>0){
                printf(GRY "  Modelli disponibili: %d\n" RST,nm);
                if(!g_ctx.model[0]){
                    /* Preferenza: qwen2.5-coder > qwen > deepseek-coder > primo disponibile */
                    const char* pref[] = {"qwen2.5-coder","qwen2.5","qwen","deepseek-coder","codellama",NULL};
                    int found=0;
                    for(int pi=0; pref[pi] && !found; pi++){
                        for(int mi=0; mi<nm && !found; mi++){
                            if(strstr(http_models[mi], pref[pi])){
                                strncpy(g_ctx.model, http_models[mi], sizeof g_ctx.model-1);
                                found=1;
                            }
                        }
                    }
                    if(!found)
                        strncpy(g_ctx.model, http_models[0], sizeof g_ctx.model-1);
                }
                printf(GRY "  Modello attivo: %s\n" RST,g_ctx.model);
            }
        }else{
            printf(YLW "  \xe2\x9a\xa0  Impossibile connettersi a %s:%d\n" RST,g_ctx.host,g_ctx.port);
            if(g_ctx.backend==BACKEND_OLLAMA)
                printf(DIM "  Avvia Ollama con: ollama serve\n" RST);
            else
                printf(DIM "  Avvia llama-server con: llama-server -m model.gguf\n" RST);
        }
    }

    /* ── Rilevamento hardware ── */
    printf(CYN "  \xe2\x9a\x99  Rilevamento hardware...\n" RST); fflush(stdout);
    hw_detect(&g_hw);
    g_hw_ready = 1;
    hw_print(&g_hw);
    printf("\n");
    /* Auto-configura n_gpu_layers per llama backend */
    if(g_hw.primary >= 0 && g_hw.dev[g_hw.primary].type != DEV_CPU)
        printf(GRN "  \xe2\x9c\x93 GPU primaria: %s (%lld MB VRAM usabile, layers=%d)\n" RST,
               g_hw.dev[g_hw.primary].name,
               g_hw.dev[g_hw.primary].avail_mb,
               g_hw.dev[g_hw.primary].n_gpu_layers);
    else
        printf(YLW "  \xe2\x9a\xa0  Nessuna GPU dedicata \xe2\x80\x94 elaborazione su CPU\n" RST);
    /* Pausa breve per leggere il riepilogo hardware, poi continua da solo */
    sleep(2);

    menu_principale();
    lw_free();
    return 0;
}
