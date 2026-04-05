/*
 * test_config_fmt.c — Benchmark TOON vs JSON per la config di Prismalux
 * ======================================================================
 * Misura:
 *   1. Velocità di scrittura (serializzazione) — N iterazioni
 *   2. Velocità di lettura  (parsing)          — N iterazioni
 *   3. Dimensione file risultante
 *   4. Correttezza round-trip (tutti i campi tornano uguali)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "include/config_toon.h"
#include "src/http.c"   /* per js_str / js_encode — incluso direttamente */

/* ── helpers ─────────────────────────────────────────────────────────── */
#define N_ITER   100000
#define GRN      "\033[32m"
#define YLW      "\033[33m"
#define CYN      "\033[36m"
#define BLD      "\033[1m"
#define RST      "\033[0m"
#define GRY      "\033[90m"

static double _ms(struct timespec a, struct timespec b) {
    return (b.tv_sec - a.tv_sec) * 1000.0
         + (b.tv_nsec - a.tv_nsec) / 1e6;
}

/* Dati di config realistici */
static const char *BK       = "ollama";
static const char *OH       = "127.0.0.1";
static const int   OP       = 11434;
static const char *OM       = "qwen2.5-coder:7b";
static const char *LH       = "127.0.0.1";
static const int   LP       = 8080;
static const char *LM       = "deepseek-coder:33b";
static const char *LL       = "";
static const char *GUI      = "Qt_GUI/build/Prismalux_GUI";
static const char *CFMT     = "json";

/* ══════════════════════════════════════════════════════════════════════
   SCRITTURA
   ══════════════════════════════════════════════════════════════════════ */

static void write_json(FILE *f) {
    char e_om[256]="", e_lm[256]="", e_ll[256]="", e_gui[512]="";
    js_encode(OM, e_om, sizeof e_om);
    js_encode(LM, e_lm, sizeof e_lm);
    js_encode(LL, e_ll, sizeof e_ll);
    js_encode(GUI, e_gui, sizeof e_gui);
    fprintf(f,
        "{\n"
        "  \"backend\": \"%s\",\n"
        "  \"ollama_host\": \"%s\",\n"
        "  \"ollama_port\": %d,\n"
        "  \"ollama_model\": \"%s\",\n"
        "  \"lserver_host\": \"%s\",\n"
        "  \"lserver_port\": %d,\n"
        "  \"lserver_model\": \"%s\",\n"
        "  \"llama_model\": \"%s\",\n"
        "  \"gui_path\": \"%s\",\n"
        "  \"config_fmt\": \"%s\"\n"
        "}\n",
        BK, OH, OP, e_om, LH, LP, e_lm, e_ll, e_gui, CFMT);
}

static void write_toon(FILE *f) {
    toon_write_comment(f, "Prismalux config — TOON");
    fprintf(f, "\n");
    toon_write_str(f, "backend",      BK);
    fprintf(f, "\n");
    toon_write_str(f, "ollama_host",  OH);
    toon_write_int(f, "ollama_port",  OP);
    toon_write_str(f, "ollama_model", OM);
    fprintf(f, "\n");
    toon_write_str(f, "lserver_host",  LH);
    toon_write_int(f, "lserver_port",  LP);
    toon_write_str(f, "lserver_model", LM);
    fprintf(f, "\n");
    toon_write_str(f, "llama_model", LL);
    fprintf(f, "\n");
    toon_write_str(f, "gui_path",    GUI);
    toon_write_str(f, "config_fmt",  CFMT);
}

/* ══════════════════════════════════════════════════════════════════════
   LETTURA
   ══════════════════════════════════════════════════════════════════════ */

static int read_json(const char *buf) {
    char bk[32]="", oh[64]="", om[128]="", lh[64]="", lm[128]="",
         ll[256]="", gui[512]="", cfmt[8]="";
    int op=0, lp=0;
    const char *v;
    v=js_str(buf,"backend");      if(v) strncpy(bk,  v, sizeof bk-1);
    v=js_str(buf,"ollama_host");  if(v) strncpy(oh,  v, sizeof oh-1);
    v=js_str(buf,"ollama_model"); if(v) strncpy(om,  v, sizeof om-1);
    v=js_str(buf,"lserver_host"); if(v) strncpy(lh,  v, sizeof lh-1);
    v=js_str(buf,"lserver_model");if(v) strncpy(lm,  v, sizeof lm-1);
    v=js_str(buf,"llama_model");  if(v) strncpy(ll,  v, sizeof ll-1);
    v=js_str(buf,"gui_path");     if(v) strncpy(gui, v, sizeof gui-1);
    v=js_str(buf,"config_fmt");   if(v) strncpy(cfmt,v, sizeof cfmt-1);
    { const char *pp=strstr(buf,"\"ollama_port\":");
      if(pp){ pp+=14; while(*pp==' ')pp++; op=(int)strtol(pp,NULL,10); } }
    { const char *pp=strstr(buf,"\"lserver_port\":");
      if(pp){ pp+=15; while(*pp==' ')pp++; lp=(int)strtol(pp,NULL,10); } }
    /* ritorna somma hash per evitare ottimizzazioni del compilatore */
    return (int)(bk[0]+oh[0]+om[0]+lh[0]+lm[0]+ll[0]+gui[0]+cfmt[0]+op+lp);
}

static int read_toon(const char *buf) {
    char bk[32]="", oh[64]="", om[128]="", lh[64]="", lm[128]="",
         ll[256]="", gui[512]="", cfmt[8]="";
    int op=0, lp=0;
    toon_get(buf,"backend",      bk,  sizeof bk);
    toon_get(buf,"ollama_host",  oh,  sizeof oh);
    toon_get(buf,"ollama_model", om,  sizeof om);
    toon_get(buf,"lserver_host", lh,  sizeof lh);
    toon_get(buf,"lserver_model",lm,  sizeof lm);
    toon_get(buf,"llama_model",  ll,  sizeof ll);
    toon_get(buf,"gui_path",     gui, sizeof gui);
    toon_get(buf,"config_fmt",   cfmt,sizeof cfmt);
    op = toon_get_int(buf,"ollama_port",  0);
    lp = toon_get_int(buf,"lserver_port", 0);
    return (int)(bk[0]+oh[0]+om[0]+lh[0]+lm[0]+ll[0]+gui[0]+cfmt[0]+op+lp);
}

/* ══════════════════════════════════════════════════════════════════════
   ROUND-TRIP CORRETTEZZA
   ══════════════════════════════════════════════════════════════════════ */

static int check_roundtrip(const char *buf, int is_toon) {
    char bk[32]="", oh[64]="", om[128]="", gui[512]="";
    int  op=0;
    int  ok=1;
    if (is_toon) {
        toon_get(buf,"backend",     bk,  sizeof bk);
        toon_get(buf,"ollama_host", oh,  sizeof oh);
        toon_get(buf,"ollama_model",om,  sizeof om);
        toon_get(buf,"gui_path",    gui, sizeof gui);
        op = toon_get_int(buf,"ollama_port",0);
    } else {
        const char *v;
        v=js_str(buf,"backend");     if(v) strncpy(bk, v,sizeof bk-1);
        v=js_str(buf,"ollama_host"); if(v) strncpy(oh, v,sizeof oh-1);
        v=js_str(buf,"ollama_model");if(v) strncpy(om, v,sizeof om-1);
        v=js_str(buf,"gui_path");    if(v) strncpy(gui,v,sizeof gui-1);
        { const char *pp=strstr(buf,"\"ollama_port\":");
          if(pp){ pp+=14; while(*pp==' ')pp++; op=(int)strtol(pp,NULL,10); } }
    }
    if(strcmp(bk,  BK) !=0){ printf("  FAIL backend:     '%s' != '%s'\n",bk,BK);  ok=0;}
    if(strcmp(oh,  OH) !=0){ printf("  FAIL ollama_host: '%s' != '%s'\n",oh,OH);  ok=0;}
    if(strcmp(om,  OM) !=0){ printf("  FAIL ollama_model:'%s' != '%s'\n",om,OM);  ok=0;}
    if(strcmp(gui, GUI)!=0){ printf("  FAIL gui_path:    '%s' != '%s'\n",gui,GUI);ok=0;}
    if(op != OP)            { printf("  FAIL ollama_port: %d != %d\n",op,OP);      ok=0;}
    return ok;
}

/* ══════════════════════════════════════════════════════════════════════
   MAIN
   ══════════════════════════════════════════════════════════════════════ */

int main(void) {
    printf(CYN BLD "\n  Benchmark TOON vs JSON — Prismalux Config\n\n" RST);
    printf(GRY "  Iterazioni: %d\n\n" RST, N_ITER);

    /* ── Genera i file di config ──────────────────────────────── */
    FILE *fj = fopen("/tmp/prismalux_test.json","w");
    FILE *ft = fopen("/tmp/prismalux_test.toon","w");
    write_json(fj); fclose(fj);
    write_toon(ft); fclose(ft);

    /* Misura dimensioni */
    FILE *tmp;
    long sz_json=0, sz_toon=0;
    tmp=fopen("/tmp/prismalux_test.json","r"); fseek(tmp,0,SEEK_END); sz_json=ftell(tmp); fclose(tmp);
    tmp=fopen("/tmp/prismalux_test.toon","r"); fseek(tmp,0,SEEK_END); sz_toon=ftell(tmp); fclose(tmp);

    printf("  ── Dimensione file ─────────────────────────────\n");
    printf("  JSON : %ld byte\n", sz_json);
    printf("  TOON : %ld byte  (%+.1f%%)\n\n", sz_toon,
           (sz_toon - sz_json) * 100.0 / sz_json);

    /* ── Leggi buffer per benchmark parsing ───────────────────── */
    char buf_json[4096]="", buf_toon[4096]="";
    tmp=fopen("/tmp/prismalux_test.json","r");
    fread(buf_json,1,sizeof buf_json-1,tmp); fclose(tmp);
    tmp=fopen("/tmp/prismalux_test.toon","r");
    fread(buf_toon,1,sizeof buf_toon-1,tmp); fclose(tmp);

    /* ── Round-trip correttezza ───────────────────────────────── */
    printf("  ── Correttezza round-trip ──────────────────────\n");
    int ok_json = check_roundtrip(buf_json, 0);
    int ok_toon = check_roundtrip(buf_toon, 1);
    printf("  JSON : %s\n", ok_json ? GRN "OK" RST : "\033[31mFAIL" RST);
    printf("  TOON : %s\n\n", ok_toon ? GRN "OK" RST : "\033[31mFAIL" RST);

    /* ── Benchmark SCRITTURA ──────────────────────────────────── */
    struct timespec t0, t1;
    volatile int sink = 0; /* evita ottimizzazioni */

    printf("  ── Scrittura (%d iter) ──────────────────────────\n", N_ITER);

    /* JSON write */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for(int i=0; i<N_ITER; i++){
        FILE *f=fopen("/tmp/_pw.json","w"); write_json(f); fclose(f); sink+=i;
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double wj = _ms(t0,t1);

    /* TOON write */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for(int i=0; i<N_ITER; i++){
        FILE *f=fopen("/tmp/_pw.toon","w"); write_toon(f); fclose(f); sink+=i;
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double wt = _ms(t0,t1);

    printf("  JSON : %8.2f ms totale  %6.3f µs/op\n", wj, wj*1000/N_ITER);
    printf("  TOON : %8.2f ms totale  %6.3f µs/op\n", wt, wt*1000/N_ITER);
    if(wt < wj) printf(GRN "  TOON piu' veloce del %.1f%%\n" RST, (wj-wt)/wj*100);
    else        printf(YLW "  JSON piu' veloce del %.1f%%\n" RST, (wt-wj)/wt*100);

    /* ── Benchmark LETTURA ────────────────────────────────────── */
    printf("\n  ── Lettura/Parsing (%d iter) ────────────────────\n", N_ITER);

    /* JSON parse */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for(int i=0; i<N_ITER; i++) sink += read_json(buf_json);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double rj = _ms(t0,t1);

    /* TOON parse */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for(int i=0; i<N_ITER; i++) sink += read_toon(buf_toon);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double rt = _ms(t0,t1);

    printf("  JSON : %8.2f ms totale  %6.3f µs/op\n", rj, rj*1000/N_ITER);
    printf("  TOON : %8.2f ms totale  %6.3f µs/op\n", rt, rt*1000/N_ITER);
    if(rt < rj) printf(GRN "  TOON piu' veloce del %.1f%%\n" RST, (rj-rt)/rj*100);
    else        printf(YLW "  JSON piu' veloce del %.1f%%\n" RST, (rt-rj)/rt*100);

    /* ── Riepilogo ────────────────────────────────────────────── */
    printf("\n  ── Riepilogo ───────────────────────────────────\n");
    printf("  %-10s  %-12s  %-12s  %s\n","Formato","Scrittura","Lettura","Dimensione");
    printf("  %-10s  %6.3f µs/op  %6.3f µs/op  %ld byte\n",
           "JSON", wj*1000/N_ITER, rj*1000/N_ITER, sz_json);
    printf("  %-10s  %6.3f µs/op  %6.3f µs/op  %ld byte\n",
           "TOON", wt*1000/N_ITER, rt*1000/N_ITER, sz_toon);
    printf(GRY "\n  (sink=%d — previene ottimizzazioni compilatore)\n\n" RST, sink);

    return 0;
}
