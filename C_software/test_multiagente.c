/*
 * test_multiagente.c — Test della pipeline multi-agente (non interattivo)
 * ========================================================================
 * Esegue la pipeline a 6 agenti su un task semplice e verifica che:
 * - ogni agente produca output non vuoto
 * - il codice Python venga estratto correttamente
 * - python3 riesca ad eseguire il codice generato
 * Compila: gcc -O2 -Iinclude test_multiagente.c src/http.c src/backend.c
 *          src/ai.c src/terminal.c src/hw_detect.c src/agent_scheduler.c
 *          src/prismalux_ui.c -lpthread -lm -o test_multiagente
 */
#include "common.h"
#include "backend.h"
#include "http.h"
#include "ai.h"

#define OK   "\033[32m[OK]\033[0m "
#define FAIL "\033[31m[FAIL]\033[0m "
#define INFO "\033[33m[INFO]\033[0m "
#define SEP  "  ────────────────────────────────────────────────────────\n"

static int g_pass = 0, g_fail = 0;
static void chk(const char* name, int ok, const char* d) {
    if (ok) { printf("  " OK "%s\n", name); g_pass++; }
    else     { printf("  " FAIL "%s  %s\n", name, d?d:""); g_fail++; }
}

/* Callback silenziosa: accumula ma non stampa */
static void silent_cb(const char* p, void* ud) {
    char *buf=(char*)ud; int l=strlen(buf); int pl=strlen(p);
    if(l+pl<MAX_BUF-1){ memcpy(buf+l,p,pl); buf[l+pl]='\0'; }
}

/* Stampa progressivo dei token */
static void stream_print(const char* p, void* ud) {
    (void)ud; printf("%s",p); fflush(stdout);
}

/* Estrazione blocco ```python */
static int extract_code(const char* resp, char* out, int maxlen) {
    const char* s = strstr(resp,"```python");
    if(!s) s = strstr(resp,"```");
    if(s){ s=strchr(s,'\n'); if(!s) goto fb; s++;
        const char* e=strstr(s,"```"); if(!e) e=s+strlen(s);
        int n=(int)(e-s); if(n>=maxlen) n=maxlen-1;
        memcpy(out,s,n); out[n]='\0'; return 1; }
fb: strncpy(out,resp,maxlen-1); out[maxlen-1]='\0'; return 0;
}

/* Esegue codice python e restituisce output */
static void run_python(const char* code, char* out, int maxlen) {
    out[0]='\0';
    FILE* f=fopen("/tmp/prismalux_ma_test.py","w");
    if(!f){ strncpy(out,"[errore file temp]",maxlen-1); return; }
    fputs(code,f); fclose(f);
    FILE* p=popen("python3 /tmp/prismalux_ma_test.py 2>&1","r");
    if(!p){ strncpy(out,"[python3 non trovato]",maxlen-1); return; }
    int l=0; while(l<maxlen-1){ int c=fgetc(p); if(c==EOF)break; out[l++]=(char)c; }
    out[l]='\0'; pclose(p);
    remove("/tmp/prismalux_ma_test.py");
}

int main(void) {
    printf("\n\033[36m\033[1m  PRISMALUX — Test Pipeline Multi-Agente\033[0m\n");
    printf("  Task: 'stampa i numeri da 1 a 5 con il loro quadrato'\n\n");

    /* Trova modelli disponibili */
    char models[32][128]; int nm=0;
    nm=http_ollama_list("127.0.0.1",11434,models,32);
    if(nm==0){ printf("  " FAIL "Ollama non raggiungibile\n"); return 1; }

    /* Seleziona modelli: leggeri per velocità del test */
    BackendCtx ctx = { BACKEND_OLLAMA, "127.0.0.1", 11434, "" };
    const char *pref_small[]  = {"qwen3:4b","qwen3.5:4b","deepseek-r1:1.5b","gemma2:2b","llama3.2:3b",NULL};
    const char *pref_coder[]  = {"qwen2.5-coder","deepseek-coder","qwen3:8b","qwen3:4b","qwen3.5",NULL};

    char m_small[128]="", m_coder[128]="";
    for(int p=0;pref_small[p]&&!m_small[0];p++)
        for(int i=0;i<nm;i++) if(strstr(models[i],pref_small[p])){strncpy(m_small,models[i],127);break;}
    for(int p=0;pref_coder[p]&&!m_coder[0];p++)
        for(int i=0;i<nm;i++) if(strstr(models[i],pref_coder[p])){strncpy(m_coder,models[i],127);break;}
    if(!m_small[0]) strncpy(m_small,models[0],127);
    if(!m_coder[0]) strncpy(m_coder,models[0],127);

    printf("  " INFO "Modello analisi : %s\n", m_small);
    printf("  " INFO "Modello codice  : %s\n\n", m_coder);

    const char* TASK = "stampa i numeri da 1 a 5 con il loro quadrato";

    char *r_out = calloc(MAX_BUF,1);
    char *p_out = calloc(MAX_BUF,1);
    char *c_out = calloc(MAX_BUF,1);
    char *run   = calloc(16384,1);
    char *prompt= calloc(MAX_BUF,1);
    if(!r_out||!p_out||!c_out||!run||!prompt){printf("  [FAIL] memoria\n");return 1;}

    /* ── [1] RICERCATORE ─────────────────────────────────────────── */
    printf(SEP "  [1/5] Ricercatore  (%s)\n" SEP, m_small);
    strncpy(ctx.model, m_small, sizeof ctx.model-1);
    snprintf(prompt, MAX_BUF,
        "Analizza questo task Python in 2-3 frasi tecniche:\n"
        "Task: %s\nDescrivi: approccio generale e librerie utili.", TASK);
    printf("  ");
    int rc1 = ai_chat_stream(&ctx,
        "Sei un ricercatore Python. Rispondi SEMPRE e SOLO in italiano. Sii conciso.",
        prompt, stream_print, NULL, r_out, MAX_BUF);
    printf("\n\n");
    chk("Ricercatore produce output", rc1==0 && r_out[0], "nessun output");

    /* ── [2] PIANIFICATORE ───────────────────────────────────────── */
    printf(SEP "  [2/5] Pianificatore  (%s)\n" SEP, m_small);
    snprintf(prompt, MAX_BUF,
        "Crea un piano in 3 passi per questo programma Python.\n"
        "Task: %s\nContesto: %s", TASK, r_out);
    printf("  ");
    int rc2 = ai_chat_stream(&ctx,
        "Sei un architetto software. Rispondi SEMPRE e SOLO in italiano. Sii conciso.",
        prompt, stream_print, NULL, p_out, MAX_BUF);
    printf("\n\n");
    chk("Pianificatore produce output", rc2==0 && p_out[0], "nessun output");

    /* ── [3] PROGRAMMATORE ───────────────────────────────────────── */
    printf(SEP "  [3/5] Programmatore  (%s)\n" SEP, m_coder);
    strncpy(ctx.model, m_coder, sizeof ctx.model-1);
    snprintf(prompt, MAX_BUF,
        "Task: %s\nPiano: %s\n\n"
        "Scrivi SOLO codice Python in ```python ... ```. NON usare input().",
        TASK, p_out);
    printf("  ");
    int rc3 = ai_chat_stream(&ctx,
        "Sei un programmatore Python. I commenti nel codice sono in italiano. "
        "Scrivi SOLO codice in ```python...```.",
        prompt, stream_print, NULL, c_out, MAX_BUF);
    printf("\n\n");
    chk("Programmatore produce output", rc3==0 && c_out[0], "nessun output");

    char code[32768]; code[0]='\0';
    int estratto = extract_code(c_out, code, sizeof code);
    chk("Estrazione blocco ```python", estratto && code[0], "nessun blocco Python trovato");

    /* ── [4] ESECUZIONE CODICE ───────────────────────────────────── */
    printf(SEP "  [4/5] Esecuzione codice generato\n" SEP);
    printf("  Codice:\n\033[90m");
    /* Stampa max 20 righe */
    int lines=0; const char *cp=code;
    while(*cp && lines<20){ const char *nl=strchr(cp,'\n');
        int len=nl?(int)(nl-cp):(int)strlen(cp);
        printf("    %.*s\n",len,cp); cp+=len+(nl?1:0); lines++; }
    if(*cp) printf("    ...\n");
    printf("\033[0m\n");

    run_python(code, run, 16384);
    printf("  Output: \033[33m%s\033[0m\n", run[0]?run:"(vuoto)");

    /* Valuta: nessun traceback/error */
    char low[4096]; int ln=(int)strlen(run); if(ln>4095)ln=4095;
    for(int i=0;i<ln;i++) low[i]=(char)tolower((unsigned char)run[i]); low[ln]='\0';
    int ok_run = !(strstr(low,"traceback")||strstr(low,"error:")||
                   strstr(low,"exception")||strstr(low,"syntaxerror"));
    chk("Codice eseguito senza errori", ok_run, low);

    /* Verifica che stampi numeri da 1 a 5 */
    int ha_numeri = strstr(run,"1")&&strstr(run,"2")&&strstr(run,"3")&&
                    strstr(run,"4")&&strstr(run,"5");
    chk("Output contiene numeri 1-5", ha_numeri, run);

    /* ── [5] OTTIMIZZATORE ───────────────────────────────────────── */
    printf(SEP "  [5/5] Ottimizzatore  (%s)\n" SEP, m_coder);
    char ottim[MAX_BUF]; ottim[0]='\0';
    snprintf(prompt, MAX_BUF,
        "Ottimizza questo codice Python (type hints, docstring, nomi chiari):\n"
        "```python\n%.2000s\n```\nScrivi SOLO il codice ottimizzato in ```python...```.",
        code);
    printf("  ");
    int rc5 = ai_chat_stream(&ctx,
        "Sei un senior Python developer. I commenti sono in italiano. "
        "Scrivi SOLO il codice in ```python...```.",
        prompt, stream_print, NULL, ottim, MAX_BUF);
    printf("\n\n");
    chk("Ottimizzatore produce output", rc5==0 && ottim[0], "nessun output");

    char code_ottim[32768]; code_ottim[0]='\0';
    if(extract_code(ottim, code_ottim, sizeof code_ottim) && code_ottim[0]) {
        char run2[16384]; run_python(code_ottim, run2, sizeof run2);
        char low2[4096]; int l2=(int)strlen(run2); if(l2>4095)l2=4095;
        for(int i=0;i<l2;i++) low2[i]=(char)tolower((unsigned char)run2[i]); low2[l2]='\0';
        int ok2 = !(strstr(low2,"traceback")||strstr(low2,"error:")||strstr(low2,"exception"));
        chk("Codice ottimizzato eseguibile", ok2, low2);
    } else {
        chk("Estrazione codice ottimizzato", 0, "nessun blocco Python");
    }

    /* ── Riepilogo ───────────────────────────────────────────────── */
    printf(SEP);
    int total = g_pass + g_fail;
    printf("  Risultati pipeline: \033[32m%d/%d PASSATI\033[0m", g_pass, total);
    if(g_fail>0) printf("  \033[31m%d FALLITI\033[0m", g_fail);
    printf("\n\n");

    free(r_out); free(p_out); free(c_out); free(run); free(prompt);
    return g_fail>0 ? 1 : 0;
}
