/*
 * multi_agente.c — SEZIONE 10: Pipeline 6 agenti
 * ===============================================
 * Ricercatore → Pianificatore → 2×Programmatore (parallelo)
 * → Tester (loop correzione) → Ottimizzatore
 */
#include "common.h"
#include "backend.h"
#include "terminal.h"
#include "http.h"
#include "ai.h"
#include "output.h"
#include "multi_agente.h"
#include "prismalux_ui.h"
#include "hw_detect.h"

/* Forward declarations (definite in main.c e modelli.c) */
void choose_backend(void);
void choose_model(void);
void salva_config(void);

/* ══════════════════════════════════════════════════════════════
   SEZIONE 10 — Multi-Agente 6 agenti
   Pipeline: Ricercatore → Pianificatore → 2×Programmatore (parallelo)
             → Tester (loop correzione) → Ottimizzatore
   ══════════════════════════════════════════════════════════════ */

#define MA_MAX_TENTATIVI 3
#define MA_N_AGENTS      6

/* Indici ruoli nell'array agents[] */
#define MA_I_RICER  0
#define MA_I_PIAN   1
#define MA_I_PROGA  2
#define MA_I_PROGB  3
#define MA_I_TESTER 4
#define MA_I_OTTIM  5

typedef struct {
    const char* nome;
    const char* icona;
    const char* sistema;
    BackendCtx  ctx;   /* ogni agente ha il proprio backend/modello */
} Agent;

static void agent_init_from_global(Agent* a) { a->ctx = g_ctx; }

/* Preferenze modello per ruolo (primo match vince) */
static const char* _MA_PREF[MA_N_AGENTS][9] = {
    /* Ricercatore   */ {"llama3.2","qwen3:4b","qwen3","mistral","gemma2","deepseek-r1",NULL},
    /* Pianificatore */ {"deepseek-r1:7b","deepseek-r1","qwen3:8b","qwen3","mistral",NULL},
    /* Programmatore */ {"qwen2.5-coder","deepseek-coder","codellama","qwen3:8b","mistral","qwen3",NULL},
    /* Prog B        */ {"qwen2.5-coder","deepseek-coder","codellama","qwen3:8b","mistral","qwen3",NULL},
    /* Tester        */ {"gemma2:2b","gemma2","llama3.2","deepseek-r1:1.5b","qwen3:4b","qwen3",NULL},
    /* Ottimizzatore */ {"qwen2.5-coder","deepseek-coder","qwen3:8b","mistral","qwen3",NULL},
};

/* ── Trova modello default per un ruolo tra quelli disponibili ─── */
static void ma_trova_default(char modelli[][128], int nm, int ruolo,
                              char* out, int outsz) {
    for (int p=0; p<8 && _MA_PREF[ruolo][p]; p++) {
        for (int d=0; d<nm; d++) {
            if (strstr(modelli[d], _MA_PREF[ruolo][p])) {
                strncpy(out, modelli[d], outsz-1); out[outsz-1]='\0'; return;
            }
        }
    }
    if (nm>0) { strncpy(out, modelli[0], outsz-1); out[outsz-1]='\0'; }
    else out[0]='\0';
}

/* ── Estrae codice Python da un blocco ```python...``` ─────────── */
static int ma_extract_code(const char* resp, char* out, int maxlen) {
    const char* s = strstr(resp, "```python");
    if (!s) s = strstr(resp, "```");
    if (s) {
        s = strchr(s, '\n'); if (!s) goto fallback; s++;
        const char* e = strstr(s, "```");
        if (!e) e = s + strlen(s);
        int n = (int)(e - s); if (n >= maxlen) n = maxlen-1;
        memcpy(out, s, n); out[n]='\0'; return 1;
    }
fallback:
    strncpy(out, resp, maxlen-1); out[maxlen-1]='\0'; return 0;
}

/* ── Esegue codice Python e cattura output (max 2000 char) ─────── */
static void ma_run_python(const char* code, char* output, int maxlen) {
    output[0]='\0';
    if (!code || !code[0]) { strncpy(output,"[nessun codice da eseguire]",maxlen-1); return; }
    char tmpfile[256];
#ifdef _WIN32
    snprintf(tmpfile, sizeof tmpfile, "prismalux_ma_%d.py", (int)GetCurrentProcessId());
#else
    snprintf(tmpfile, sizeof tmpfile, "/tmp/prismalux_ma_%d.py", (int)getpid());
#endif
    FILE* f = fopen(tmpfile, "w");
    if (!f) { strncpy(output, "[errore creazione file temp]", maxlen-1); return; }
    fputs(code, f); fclose(f);
    char cmd[640];
#ifdef _WIN32
    snprintf(cmd, sizeof cmd, "python \"%s\" 2>&1", tmpfile);
#else
    snprintf(cmd, sizeof cmd, "python3 \"%s\" 2>&1", tmpfile);
#endif
    FILE* p = popen(cmd, "r");
    if (!p) { strncpy(output,"[python3 non trovato]",maxlen-1); remove(tmpfile); return; }
    int len=0;
    while (len < maxlen-1) { int c=fgetc(p); if(c==EOF)break; output[len++]=(char)c; }
    output[len]='\0'; pclose(p);
    remove(tmpfile);
    if (len > 2000) { output[2000]='\0'; strncat(output,"\n[... output troncato ...]",50); }
}

/* ── Valuta output: 1=PASSATO, 0=FALLITO ──────────────────────── */
static int ma_valuta_output(const char* out) {
    char low[4096]; int n=(int)strlen(out); if(n>4095)n=4095;
    for(int i=0;i<n;i++) low[i]=(char)tolower((unsigned char)out[i]); low[n]='\0';
    return !(strstr(low,"traceback")||strstr(low,"  file \"")||
             strstr(low,"error:")||strstr(low,"exception")||
             strstr(low,"syntaxerror")||strstr(low,"nameerror")||
             strstr(low,"typeerror")||strstr(low,"indentationerror"));
}

/* ── Thread: programmatore senza stream (output in background) ─── */
typedef struct { const Agent* agent; char input[8192]; char* output; int status; } ProgArg;

#ifdef _WIN32
static DWORD prog_thread_fn(void* arg) {
#else
static void* prog_thread_fn(void* arg) {
#endif
    ProgArg* a = (ProgArg*)arg;
    a->output = (char*)malloc(MAX_BUF);
    if (!a->output) { a->status=-1;
#ifdef _WIN32
        return 0;
#else
        return NULL;
#endif
    }
    a->output[0]='\0';
    a->status = ai_chat_stream(&a->agent->ctx, a->agent->sistema,
                               a->input, NULL, NULL, a->output, MAX_BUF);
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

/* ══ Selezione modelli per ruolo (5 visibili, Prog A e B condividono) ═══ */
static void ma_scegli_modelli(Agent* agents) {
    static const char* NOMI[5]  = {"Ricercatore","Pianificatore","Programmatore","Tester","Ottimizzatore"};
    static const char* ICONE[5] = {"\xf0\x9f\x94\x8d","\xf0\x9f\x93\x90",
                                    "\xf0\x9f\x92\xbb","\xf0\x9f\xa7\xaa","\xe2\x9c\xa8"};
    static const int   IDXMAP[5] = {MA_I_RICER, MA_I_PIAN, MA_I_PROGA, MA_I_TESTER, MA_I_OTTIM};

    /* Lista modelli Ollama (usa backend del primo agente) */
    char modelli[32][128]; int nm=0;
    if (agents[0].ctx.backend == BACKEND_OLLAMA)
        nm = http_ollama_list(agents[0].ctx.host, agents[0].ctx.port, modelli, 32);
    else if (agents[0].ctx.backend == BACKEND_LLAMASERVER)
        nm = http_llamaserver_list(agents[0].ctx.host, agents[0].ctx.port, modelli, 32);

    if (nm <= 0) {
        printf(YLW "\n  \xe2\x9a\xa0  Nessun modello recuperato. Configura prima il backend.\n" RST);
        printf(GRY "  Premi un tasto...\n" RST); getch_single(); return;
    }

    /* Default per ogni ruolo */
    char defaults[5][128];
    for (int r=0; r<5; r++) ma_trova_default(modelli, nm, IDXMAP[r], defaults[r], 128);

    const char* sep = "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
                      "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
                      "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
                      "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
                      "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
                      "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
                      "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
                      "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
                      "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
                      "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
                      "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80";

    while (1) {
        clear_screen(); print_header();
        printf(CYN BLD "\n  \xf0\x9f\xa4\x96 Selezione Modelli per Agente\n\n" RST);

        printf("  Modelli Ollama disponibili:\n  %s\n", sep);
        for (int i=0; i<nm; i++) printf("  %2d.  %s\n", i+1, modelli[i]);
        printf("  %s\n\n", sep);

        printf("  Selezione automatica:\n  %s\n", sep);
        for (int r=0; r<5; r++) {
            const char* tag = (strstr(defaults[r],"coder")||strstr(defaults[r],"code"))
                              ? "  \xe2\x86\x90 coder \xe2\x9c\x93" : "";
            printf("  " BLD "[%d]" RST "  %s  %-16s \xe2\x86\x92  " CYN "%-30s" RST "%s\n",
                   r+1, ICONE[r], NOMI[r], defaults[r], tag);
        }
        printf("  %s\n\n", sep);
        printf("  " BLD "INVIO" RST " = conferma tutto   |   " BLD "1\xe2\x80\x935" RST " = cambia un agente\n");
        printf("  \xe2\x9d\xaf "); fflush(stdout);

        char buf[16]; if (!fgets(buf, sizeof buf, stdin)) break;
        buf[strcspn(buf,"\n")]='\0';
        if (!buf[0]) break;   /* INVIO = conferma */

        int r = atoi(buf);
        if (r >= 1 && r <= 5) {
            r--;
            printf("\n  %s %s \xe2\x86\x92 inserisci numero (1\xe2\x80\x93%d): ",
                   ICONE[r], NOMI[r], nm);
            fflush(stdout);
            char mbuf[8]; if (fgets(mbuf, sizeof mbuf, stdin)) {
                int mc = atoi(mbuf)-1;
                if (mc >= 0 && mc < nm) {
                    strncpy(defaults[r], modelli[mc], 127);
                    printf(GRN "  \xe2\x9c\x93 %s \xe2\x86\x92 %s\n" RST, NOMI[r], defaults[r]);
                } else { printf(YLW "  Numero non valido.\n" RST); }
            }
        }
    }

    /* Applica ai 5 ruoli; Prog B copia Prog A */
    for (int r=0; r<5; r++) {
        if (!defaults[r][0]) continue;
        strncpy(agents[IDXMAP[r]].ctx.model, defaults[r],
                sizeof(agents[0].ctx.model)-1);
    }
    agents[MA_I_PROGB].ctx = agents[MA_I_PROGA].ctx;
    printf(GRN "\n  \xe2\x9c\x93 Modelli configurati.\n" RST);
    printf(GRY "  Premi un tasto...\n" RST); getch_single();
}

/* ══ Pipeline principale 6-agenti ════════════════════════════════ */
static void multi_agent_run(const char* task, Agent* agents) {

    /* Buffer allocati dinamicamente */
    char *ricer_out = malloc(MAX_BUF);   /* output Ricercatore           */
    char *piano_raw = malloc(MAX_BUF);   /* output Pianificatore grezzo  */
    char *piano_a   = malloc(MAX_BUF);   /* piano A estratto             */
    char *piano_b   = malloc(MAX_BUF);   /* piano B estratto             */
    char *codice_fin = malloc(MAX_BUF);  /* codice vincitore             */
    char *run_out   = malloc(MAX_BUF);   /* output esecuzione            */
    char *tester_out = malloc(MAX_BUF);  /* output Tester                */
    char *ottim_out = malloc(MAX_BUF);   /* output Ottimizzatore         */
    char *input_buf = malloc(MAX_BUF);   /* buffer costruzione prompt    */

    if (!ricer_out||!piano_raw||!piano_a||!piano_b||!codice_fin||
        !run_out||!tester_out||!ottim_out||!input_buf) {
        printf(RED "  Memoria insufficiente.\n" RST);
        free(ricer_out); free(piano_raw); free(piano_a); free(piano_b);
        free(codice_fin); free(run_out); free(tester_out);
        free(ottim_out); free(input_buf); return;
    }

    ricer_out[0]=piano_raw[0]=piano_a[0]=piano_b[0]=codice_fin[0]='\0';
    run_out[0]=tester_out[0]=ottim_out[0]=input_buf[0]='\0';

    const char* riga = "  \xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
                       "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
                       "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
                       "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
                       "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
                       "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
                       "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
                       "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
                       "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
                       "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80";

    /* ────────────────────────────────────────────────────────────
       STEP 1 — RICERCATORE
       ──────────────────────────────────────────────────────────── */
    printf("\n" CYN "  \xf0\x9f\x93\x9c Consultando gli archivi divini...\n\n" RST);
    printf(YLW BLD "  [1/5]  \xf0\x9f\x94\x8d Ricercatore" RST);
    if (agents[MA_I_RICER].ctx.model[0])
        printf(DIM "  %s" RST, agents[MA_I_RICER].ctx.model);
    printf("\n%s\n", riga);

    snprintf(input_buf, MAX_BUF,
        "Analizza questo task Python in 3-4 frasi tecniche:\n"
        "Task: %s\n\n"
        "Descrivi: 1) Approccio generale, 2) Librerie utili, 3) Difficolta principali.",
        task);

    printf("  "); fflush(stdout);
    ai_chat_stream(&agents[MA_I_RICER].ctx, agents[MA_I_RICER].sistema,
                   input_buf, stream_cb, NULL, ricer_out, MAX_BUF);
    printf("\n\n");

    /* ────────────────────────────────────────────────────────────
       STEP 2 — PIANIFICATORE → 2 piani
       ──────────────────────────────────────────────────────────── */
    printf(YLW BLD "  [2/5]  \xf0\x9f\x93\x90 Pianificatore" RST);
    if (agents[MA_I_PIAN].ctx.model[0])
        printf(DIM "  %s" RST, agents[MA_I_PIAN].ctx.model);
    printf("\n%s\n", riga);
    printf(DIM "  Creo 2 approcci alternativi...\n" RST); fflush(stdout);

    snprintf(input_buf, MAX_BUF,
        "Crea DUE approcci architetturali per questo programma Python.\n"
        "Task: %s\n\nContesto: %s\n\n"
        "Formato OBBLIGATORIO:\n"
        "APPROCCIO A\n[4 passi max, niente codice]\n\n"
        "APPROCCIO B\n[4 passi max, alternativa diversa, niente codice]",
        task, ricer_out);

    printf("  "); fflush(stdout);
    ai_chat_stream(&agents[MA_I_PIAN].ctx, agents[MA_I_PIAN].sistema,
                   input_buf, stream_cb, NULL, piano_raw, MAX_BUF);
    printf("\n\n");

    /* Split piano A e piano B */
    const char* sep_b = strstr(piano_raw, "APPROCCIO B");
    if (!sep_b) sep_b = strstr(piano_raw, "Approccio B");
    if (!sep_b) sep_b = strstr(piano_raw, "approccio b");
    if (sep_b) {
        int la = (int)(sep_b - piano_raw);
        strncpy(piano_a, piano_raw, (la < MAX_BUF-1) ? la : MAX_BUF-1);
        piano_a[(la < MAX_BUF-1) ? la : MAX_BUF-1]='\0';
        snprintf(piano_b, MAX_BUF, "APPROCCIO B%s", sep_b+11);
    } else {
        strncpy(piano_a, piano_raw, MAX_BUF-1);
        strncpy(piano_b, piano_raw, MAX_BUF-1);
    }

    /* ────────────────────────────────────────────────────────────
       STEP 3 — 2 PROGRAMMATORI in parallelo
       ──────────────────────────────────────────────────────────── */
    printf(YLW BLD "  [3/5]  \xf0\x9f\x92\xbb 2 Programmatori in parallelo" RST);
    if (agents[MA_I_PROGA].ctx.model[0])
        printf(DIM "  %s" RST, agents[MA_I_PROGA].ctx.model);
    printf("\n%s\n", riga);
    printf(DIM "  \xe2\x9a\x99  Agente A e B in esecuzione... (attendere)\n" RST);
    fflush(stdout);

    ProgArg arg_a, arg_b;
    arg_a.agent = &agents[MA_I_PROGA]; arg_a.output = NULL; arg_a.status = -1;
    arg_b.agent = &agents[MA_I_PROGB]; arg_b.output = NULL; arg_b.status = -1;

    snprintf(arg_a.input, 8192,
        "Task: %s\n\nImplementa il seguente piano in Python.\n%s\n\n"
        "REGOLE: Scrivi SOLO codice Python con ```python ... ```.\n"
        "NON usare input(). Usa valori di esempio hardcodati.",
        task, piano_a);
    snprintf(arg_b.input, 8192,
        "Task: %s\n\nImplementa il seguente piano in Python.\n%s\n\n"
        "REGOLE: Scrivi SOLO codice Python con ```python ... ```.\n"
        "NON usare input(). Usa valori di esempio hardcodati.",
        task, piano_b);

    hw_thread_t tid_a, tid_b;
    int ta = hw_thread_create(&tid_a, (hw_thread_fn)prog_thread_fn, &arg_a);
    int tb = hw_thread_create(&tid_b, (hw_thread_fn)prog_thread_fn, &arg_b);
    if (ta == 0) hw_thread_join(tid_a);
    if (tb == 0) hw_thread_join(tid_b);

    const char* raw_a = arg_a.output ? arg_a.output : "";
    const char* raw_b = arg_b.output ? arg_b.output : "";

    /* Estrae codice dai due output */
    char code_a[MAX_BUF/2], code_b[MAX_BUF/2];
    code_a[0]=code_b[0]='\0';
    ma_extract_code(raw_a, code_a, MAX_BUF/2);
    ma_extract_code(raw_b, code_b, MAX_BUF/2);

    /* Mostra i due codici */
    printf(MAG BLD "\n  \xf0\x9f\x92\xbb CODICE A:\n" RST);
    printf(GRY "%.1200s\n" RST, code_a[0] ? code_a : "(nessun codice estratto)");
    printf(MAG BLD "\n  \xf0\x9f\x92\xbb CODICE B:\n" RST);
    printf(GRY "%.1200s\n" RST, code_b[0] ? code_b : "(nessun codice estratto)");
    printf("\n");

    /* Esegue entrambi i codici */
    char out_a[8192], out_b[8192];
    out_a[0]=out_b[0]='\0';
    printf(DIM "  \xe2\x96\xb6 Esecuzione Codice A... " RST); fflush(stdout);
    ma_run_python(code_a, out_a, 8192);
    printf(ma_valuta_output(out_a) ? GRN "PASSATO\n" RST : RED "FALLITO\n" RST);
    printf(DIM "  \xe2\x96\xb6 Esecuzione Codice B... " RST); fflush(stdout);
    ma_run_python(code_b, out_b, 8192);
    printf(ma_valuta_output(out_b) ? GRN "PASSATO\n" RST : RED "FALLITO\n" RST);

    /* ────────────────────────────────────────────────────────────
       STEP 4 — TESTER: sceglie il migliore + loop correzione
       ──────────────────────────────────────────────────────────── */
    printf("\n" YLW BLD "  [4/5]  \xf0\x9f\xa7\xaa Tester" RST);
    if (agents[MA_I_TESTER].ctx.model[0])
        printf(DIM "  %s" RST, agents[MA_I_TESTER].ctx.model);
    printf("\n%s\n", riga);

    /* Il tester sceglie il codice migliore */
    snprintf(input_buf, MAX_BUF,
        "Task: %s\n\n"
        "CODICE A:\n```python\n%.1500s\n```\nOutput A:\n%.500s\n\n"
        "CODICE B:\n```python\n%.1500s\n```\nOutput B:\n%.500s\n\n"
        "Scegli il codice migliore. Scrivi SOLO 'APPROCCIO A' oppure 'APPROCCIO B' "
        "nella prima riga, poi il verdetto PASSATO o FALLITO, poi 2 righe di analisi.",
        task, code_a, out_a, code_b, out_b);

    printf("  "); fflush(stdout);
    ai_chat_stream(&agents[MA_I_TESTER].ctx, agents[MA_I_TESTER].sistema,
                   input_buf, stream_cb, NULL, tester_out, MAX_BUF);
    printf("\n");

    /* Determina codice scelto dal tester */
    int usa_a = strstr(tester_out, "APPROCCIO A") != NULL ||
                strstr(tester_out, "Approccio A") != NULL;
    strncpy(codice_fin, usa_a ? code_a : code_b, MAX_BUF-1);
    strncpy(run_out,    usa_a ? out_a   : out_b,  MAX_BUF-1);
    printf(DIM "\n  Scelta tester: %s\n" RST, usa_a ? "Approccio A" : "Approccio B");

    /* ── Loop di correzione (max MA_MAX_TENTATIVI) ── */
    int verdetto = strstr(tester_out,"PASSATO") != NULL;
    int tentativo = 0;
    while (!verdetto && tentativo < MA_MAX_TENTATIVI) {
        tentativo++;
        printf(YLW "\n  \xf0\x9f\x94\xa7 Correttore (tentativo %d/%d)\n" RST,
               tentativo, MA_MAX_TENTATIVI);
        printf("%s\n", riga);

        snprintf(input_buf, MAX_BUF,
            "Il seguente codice Python produce un errore. Correggilo.\n\n"
            "Task: %s\n\nCodice con errore:\n```python\n%.1800s\n```\n\nErrore:\n%.600s\n\n"
            "Scrivi SOLO il codice corretto in ```python ... ```. Nessuna spiegazione.",
            task, codice_fin, run_out);

        char* correz = (char*)malloc(MAX_BUF);
        if (!correz) break;
        correz[0]='\0';

        printf("  "); fflush(stdout);
        int rc = ai_chat_stream(&agents[MA_I_PROGA].ctx, agents[MA_I_PROGA].sistema,
                                input_buf, stream_cb, NULL, correz, MAX_BUF);
        printf("\n");

        if (rc == 0) {
            ma_extract_code(correz, codice_fin, MAX_BUF-1);
            printf(DIM "\n  \xe2\x96\xb6 Riesecuzione... " RST); fflush(stdout);
            ma_run_python(codice_fin, run_out, MAX_BUF-1);
            verdetto = ma_valuta_output(run_out);
            printf(verdetto ? GRN "PASSATO\n" RST : RED "FALLITO\n" RST);

            /* Tester ri-valuta */
            snprintf(input_buf, MAX_BUF,
                "Codice:\n```python\n%.2000s\n```\nOutput:\n%.600s\n\n"
                "Verdetto: scrivi SOLO 'PASSATO' o 'FALLITO' e 1 riga di motivazione.",
                codice_fin, run_out);
            tester_out[0]='\0';
            ai_chat_stream(&agents[MA_I_TESTER].ctx, agents[MA_I_TESTER].sistema,
                           input_buf, stream_cb, NULL, tester_out, MAX_BUF);
            printf("\n");
            verdetto = strstr(tester_out,"PASSATO") != NULL;
        }
        free(correz);
    }

    /* ────────────────────────────────────────────────────────────
       STEP 5 — OTTIMIZZATORE
       ──────────────────────────────────────────────────────────── */
    printf("\n" YLW BLD "  [5/5]  \xe2\x9c\xa8 Ottimizzatore" RST);
    if (agents[MA_I_OTTIM].ctx.model[0])
        printf(DIM "  %s" RST, agents[MA_I_OTTIM].ctx.model);
    printf("\n%s\n", riga);
    printf(DIM "  Rendo il codice pi\xc3\xb9 pythonic...\n" RST); fflush(stdout);

    snprintf(input_buf, MAX_BUF,
        "Ottimizza questo codice Python applicando nell'ordine:\n"
        "1) Type hints, 2) List/dict comprehension, 3) Nomi variabili chiari,\n"
        "4) Costanti in UPPER_CASE, 5) Docstring breve, 6) Rimuovi codice morto.\n\n"
        "Codice da ottimizzare:\n```python\n%.2000s\n```\n\n"
        "Scrivi SOLO il codice ottimizzato in ```python ... ```. Nessuna spiegazione.",
        codice_fin);

    printf("  "); fflush(stdout);
    ai_chat_stream(&agents[MA_I_OTTIM].ctx, agents[MA_I_OTTIM].sistema,
                   input_buf, stream_cb, NULL, ottim_out, MAX_BUF);
    printf("\n");

    /* Estrae codice finale ottimizzato */
    char codice_ottim[MAX_BUF/2];
    codice_ottim[0]='\0';
    ma_extract_code(ottim_out, codice_ottim, MAX_BUF/2);
    if (codice_ottim[0]) strncpy(codice_fin, codice_ottim, MAX_BUF-1);

    /* ────────────────────────────────────────────────────────────
       EPILOGO — Salvataggio
       ──────────────────────────────────────────────────────────── */
    printf(CYN BLD "\n  \xe2\x9c\xa8 Verit\xc3\xa0 rivelata. Bevi la conoscenza.\n" RST);
    printf(DIM "  Verdetto finale: %s\n\n" RST, verdetto ? "PASSATO" : "NON VERIFICATO");

    printf(GRY "  \xf0\x9f\x92\xbe Vuoi salvare il risultato? [S/n] " RST); fflush(stdout);
    char _yn[8];
    if (fgets(_yn, sizeof _yn, stdin) &&
        (_yn[0]=='S'||_yn[0]=='s'||_yn[0]=='\n'||_yn[0]=='\r'))
        salva_output(task, codice_fin);
    else
        printf(GRY "  Risultato non salvato.\n" RST);

    free(arg_a.output); free(arg_b.output);
    free(ricer_out); free(piano_raw); free(piano_a); free(piano_b);
    free(codice_fin); free(run_out); free(tester_out);
    free(ottim_out); free(input_buf);
}

/* ══ Setup rapido + menu Multi-Agente ════════════════════════════ */
static int setup_rapido_multiagente(Agent* agents) {
    while (1) {
        clear_screen(); print_header();
        printf(CYN BLD "\n  \xf0\x9f\xa4\x96 Multi-Agente AI \xe2\x80\x94 Configurazione\n" RST);
        printf(GRY "  Pipeline: Ricercatore \xe2\x86\x92 Pianificatore \xe2\x86\x92 "
                   "2\xc3\x97Programmatore \xe2\x86\x92 Tester \xe2\x86\x92 Ottimizzatore\n\n" RST);

        /* Stato backend */
        printf("  Backend : " CYN "%s" RST, backend_name(agents[0].ctx.backend));
        if (agents[0].ctx.backend == BACKEND_LLAMA)
            printf("  (modello: %s)\n", lw_is_loaded() ? lw_model_name() : RED "nessuno" RST);
        else
            printf("  \xe2\x86\x92  %s:%d  modello: " CYN "%s" RST "\n",
                   agents[0].ctx.host, agents[0].ctx.port,
                   agents[0].ctx.model[0] ? agents[0].ctx.model : RED "(nessuno)" RST);

        if (agents[0].ctx.backend == BACKEND_LLAMA && g_hw_ready) {
            const HWDevice* pd = &g_hw.dev[g_hw.primary];
            printf("  Device  : " CYN "%s \xe2\x80\x94 %s" RST "\n",
                   hw_dev_type_str(pd->type), pd->name);
        }

        printf("\n");
        printf("  " EM_1 "  Cambia backend\n");
        printf("  " EM_2 "  Cambia modello globale\n");
        printf("  " EM_3 "  Selezione modelli per ruolo\n");
        printf("  " BLD "F2" RST "  Alterna Ollama \xe2\x86\x94 llama\n");
        printf("  " BLD "INVIO" RST "  Avvia pipeline\n");
        printf("  " EM_0 "  Torna al menu\n");
        printf("\n  Scelta: "); fflush(stdout);

        int c = read_key_ext();

        if (c == KEY_F2) {
            if (agents[0].ctx.backend == BACKEND_OLLAMA ||
                agents[0].ctx.backend == BACKEND_LLAMASERVER) {
                for (int i=0; i<MA_N_AGENTS; i++) agents[i].ctx.backend = BACKEND_LLAMA;
                printf(GRN "\n  \xe2\x86\x92 Backend: llama statico\n" RST);
            } else {
                for (int i=0; i<MA_N_AGENTS; i++) {
                    agents[i].ctx.backend = BACKEND_OLLAMA;
                    if (!agents[i].ctx.port) agents[i].ctx.port = 11434;
                }
                printf(GRN "\n  \xe2\x86\x92 Backend: Ollama\n" RST);
            }
            salva_config(); continue;
        }
        if (c == '\n' || c == '\r') {
            /* Verifica che almeno un agente sia configurato */
            if (!backend_ready(&agents[0].ctx)) {
                printf(YLW "\n  \xe2\x9a\xa0  Backend non pronto \xe2\x80\x94 configura prima il modello.\n" RST);
                printf(GRY "  Premi un tasto...\n" RST); getch_single();
                continue;
            }
            return 1;
        }
        if (c == '0' || c == 27) return 0;
        if (c == '1') { choose_backend();
                        for (int i=1; i<MA_N_AGENTS; i++) agents[i].ctx = g_ctx;
                        continue; }
        if (c == '2') { choose_model();
                        for (int i=0; i<MA_N_AGENTS; i++) agents[i].ctx = g_ctx;
                        continue; }
        if (c == '3') { ma_scegli_modelli(agents); continue; }
    }
}

void menu_multi_agente(void) {
    Agent agents[MA_N_AGENTS] = {
        {"Ricercatore",   "\xf0\x9f\x94\x8d",
         "Sei un ricercatore Python esperto. Rispondi SEMPRE e SOLO in italiano. "
         "Analizza il task in modo conciso e tecnico. Non scrivere codice, solo analisi in 3-4 frasi.", {0}},
        {"Pianificatore", "\xf0\x9f\x93\x90",
         "Sei un architetto software Python. Rispondi SEMPRE e SOLO in italiano. "
         "Crea piani di implementazione chiari e concisi. "
         "Usa il formato richiesto: APPROCCIO A e APPROCCIO B.", {0}},
        {"Programmatore A", "\xf0\x9f\x92\xbb",
         "Sei un programmatore Python esperto. I commenti nel codice sono in italiano. "
         "Scrivi SOLO codice Python corretto e funzionante "
         "in blocchi ```python ... ```. NON usare input(), usa valori hardcodati.", {0}},
        {"Programmatore B", "\xf0\x9f\x92\xbb",
         "Sei un programmatore Python esperto. I commenti nel codice sono in italiano. "
         "Scrivi SOLO codice Python corretto e funzionante "
         "in blocchi ```python ... ```. NON usare input(), usa valori hardcodati.", {0}},
        {"Tester",  "\xf0\x9f\xa7\xaa",
         "Sei un tester Python. Rispondi SEMPRE e SOLO in italiano. "
         "Valuta il codice analizzando l'output. "
         "Scrivi PASSATO se funziona, FALLITO se ci sono errori. Sii conciso.", {0}},
        {"Ottimizzatore", "\xe2\x9c\xa8",
         "Sei un senior Python developer. I commenti nel codice ottimizzato sono in italiano. "
         "Ottimizza il codice applicando type hints, list comprehension, nomi chiari e docstring. "
         "Scrivi SOLO il codice in ```python...```.", {0}},
    };
    for (int i=0; i<MA_N_AGENTS; i++) agent_init_from_global(&agents[i]);

    clear_screen(); print_header();

    /* Verifica backend pronto */
    if (!backend_ready(&agents[0].ctx)) {
        printf(YLW "\n  \xe2\x9a\xa0  Backend non configurato.\n" RST);
        printf(GRY "  Vai in " CYN "Manutenzione \xe2\x86\x92 Backend & Rete" GRY " per configurarlo.\n\n" RST);
        printf(GRY "  [INVIO per tornare] " RST); fflush(stdout);
        { char _b[4]; if(!fgets(_b,sizeof _b,stdin)){} }
        return;
    }

    /* Info rapida */
    printf(CYN BLD "\n  \xf0\x9f\xa4\x96 Multi-Agente AI\n" RST);
    printf(GRY "  \xf0\x9f\x94\x8d Ricercatore \xe2\x86\x92 \xf0\x9f\x93\x90 Pianificatore \xe2\x86\x92 "
               "\xf0\x9f\x92\xbb\xf0\x9f\x92\xbb 2 Programmatori \xe2\x86\x92 "
               "\xf0\x9f\xa7\xaa Tester \xe2\x86\x92 \xe2\x9c\xa8 Ottimizzatore\n" RST);
    printf(GRY "  Backend: " CYN "%s" GRY "  Modello: " CYN "%s\n\n" RST,
           backend_name(agents[0].ctx.backend),
           agents[0].ctx.model[0] ? agents[0].ctx.model : "auto");

    printf("  Descrivi cosa vuoi programmare " GRY "(" EM_0 " o vuoto = torna)" RST ":\n");
    char task[1024];
    if (!input_line("  \xe2\x9d\xaf ", task, sizeof task) || !task[0]) return;
    if (task[0]=='0' && !task[1]) return;
    if (!check_risorse_ok()) return;

    multi_agent_run(task, agents);
    printf(GRN "\n  \xf0\x9f\x8d\xba \xc3\x80 prossima libagione di sapere.\n" RST);
    printf(GRY "\n  [INVIO per tornare] " RST); fflush(stdout);
    { char _b[4]; if(!fgets(_b,sizeof _b,stdin)){} }
}
