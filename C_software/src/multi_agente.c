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
#include "modelli.h"
#include "output.h"
#include "multi_agente.h"
#include "prismalux_ui.h"
#include "hw_detect.h"
#include <math.h>
#include <time.h>

/* ══════════════════════════════════════════════════════════════
   GUARDIA PRE-PIPELINE
   Rileva task non adatti alla pipeline (matematica, domande
   generali) e risponde direttamente senza consumare RAM/AI.
   ══════════════════════════════════════════════════════════════ */

/* ── Mini valutatore aritmetico — funzioni stile Wolfram/Derive/TI ───── */
static const char *_gp;
static int         _gp_err;  /* 1 se simbolo sconosciuto nel parser    */
static double _gp_expr(void);
static void   _gp_ws(void)  { while(*_gp==' '||*_gp=='\t') _gp++; }
static double _gp_prim(void) {
    _gp_ws();
    if(*_gp=='('){_gp++;double v=_gp_expr();_gp_ws();if(*_gp==')')_gp++;return v;}
    /* Costanti e funzioni matematiche (Wolfram/Derive/TI + sinonimi italiani) */
    if(isalpha((unsigned char)*_gp)){
        const char *s=_gp;
        while(isalpha((unsigned char)*_gp)||isdigit((unsigned char)*_gp)) _gp++;
        int len=(int)(_gp-s); char nm[16]={};
        for(int i=0;i<len&&i<15;i++) nm[i]=(char)tolower((unsigned char)s[i]);
        _gp_ws();
        /* ── Costanti ─ */
        if(!strcmp(nm,"pi"))   return 3.14159265358979323846;
        if(!strcmp(nm,"phi"))  return 1.6180339887498948;
        if(!strcmp(nm,"e") && *_gp!='(') return 2.71828182845904523536;
        /* ── Parole italiane di contesto (salta e prosegui) ─ */
        if(!strcmp(nm,"di")||!strcmp(nm,"del")||!strcmp(nm,"della"))
            { _gp_ws(); return _gp_prim(); }
        /* ── Special: radice [cubica|quadrata] [di] X (peek-ahead) ─ */
        if(!strcmp(nm,"radice")){
            int is_cub=(strncmp(_gp,"cubica",6)==0&&!isalnum((unsigned char)_gp[6]));
            if(is_cub){_gp+=6;_gp_ws();}
            else if(strncmp(_gp,"quadrata",8)==0&&!isalnum((unsigned char)_gp[8]))
                {_gp+=8;_gp_ws();}
            if(strncmp(_gp,"di",2)==0&&!isalnum((unsigned char)_gp[2]))
                {_gp+=2;_gp_ws();}
            double x;
            if(*_gp=='('){_gp++;x=_gp_expr();_gp_ws();if(*_gp==')')_gp++;}
            else x=_gp_prim();
            return is_cub?cbrt(x):(x>=0?sqrt(x):0.0);
        }
        /* ── Special: logaritmo [naturale] [di] X (peek-ahead) ─ */
        if(!strcmp(nm,"logaritmo")){
            int is_nat=(strncmp(_gp,"naturale",8)==0&&!isalnum((unsigned char)_gp[8]));
            if(is_nat){_gp+=8;_gp_ws();}
            if(strncmp(_gp,"di",2)==0&&!isalnum((unsigned char)_gp[2]))
                {_gp+=2;_gp_ws();}
            double x;
            if(*_gp=='('){_gp++;x=_gp_expr();_gp_ws();if(*_gp==')')_gp++;}
            else x=_gp_prim();
            return x>0?(is_nat?log(x):log10(x)):0.0;
        }
        /* ── Leggi 1 o 2 argomenti (con o senza parentesi) ─ */
        double a=0.0,b=0.0; int nargs=0;
        if(*_gp=='('){_gp++;a=_gp_expr();_gp_ws();nargs=1;b=a;
            if(*_gp==','){_gp++;b=_gp_expr();_gp_ws();nargs=2;}
            if(*_gp==')')_gp++;
        } else { a=_gp_prim(); b=a; nargs=1; }
        /* ── Radici ─ */
        if(!strcmp(nm,"sqrt")||!strcmp(nm,"radq")) return a>=0?sqrt(a):0.0;
        if(!strcmp(nm,"cbrt"))  return cbrt(a);
        /* ── Valore assoluto / arrotondamento ─ */
        if(!strcmp(nm,"abs"))   return fabs(a);
        if(!strcmp(nm,"floor")) return floor(a);
        if(!strcmp(nm,"ceil"))  return ceil(a);
        if(!strcmp(nm,"round")) return round(a);
        if(!strcmp(nm,"trunc")) return trunc(a);
        if(!strcmp(nm,"sign"))  return (double)((a>0)-(a<0));
        /* ── Esponenziale ─ */
        if(!strcmp(nm,"exp"))   return exp(a);
        /* ── Logaritmi (TI Derive: log=log10, ln=naturale) ─ */
        if(!strcmp(nm,"ln"))    return a>0?log(a):0.0;
        if(!strcmp(nm,"log2"))  return a>0?log2(a):0.0;
        if(!strcmp(nm,"log10")||!strcmp(nm,"lg")) return a>0?log10(a):0.0;
        if(!strcmp(nm,"log"))   return nargs==1?(a>0?log10(a):0.0)
                                              :(a>0&&b>0&&b!=1?log(a)/log(b):0.0);
        /* ── Trigonometria (argomento in radianti; sinonimi italiani) ─ */
        if(!strcmp(nm,"sin")||!strcmp(nm,"seno"))     return sin(a);
        if(!strcmp(nm,"cos")||!strcmp(nm,"coseno"))   return cos(a);
        if(!strcmp(nm,"tan")||!strcmp(nm,"tangente")) return tan(a);
        if(!strcmp(nm,"cot"))   return cos(a)/sin(a);
        if(!strcmp(nm,"sec"))   return 1.0/cos(a);
        if(!strcmp(nm,"csc"))   return 1.0/sin(a);
        /* ── Trigonometria inversa ─ */
        if(!strcmp(nm,"asin")||!strcmp(nm,"arcsin")||!strcmp(nm,"arcoseno"))   return asin(a);
        if(!strcmp(nm,"acos")||!strcmp(nm,"arccos")||!strcmp(nm,"arcocoseno")) return acos(a);
        if(!strcmp(nm,"atan")||!strcmp(nm,"arctan")||!strcmp(nm,"arcotangente"))
            return nargs==2?atan2(a,b):atan(a);
        /* ── Trigonometria iperbolica ─ */
        if(!strcmp(nm,"sinh")) return sinh(a);
        if(!strcmp(nm,"cosh")) return cosh(a);
        if(!strcmp(nm,"tanh")) return tanh(a);
        /* ── Funzioni a 2 argomenti ─ */
        if(!strcmp(nm,"pow"))   return pow(a,b);
        if(!strcmp(nm,"min"))   return a<b?a:b;
        if(!strcmp(nm,"max"))   return a>b?a:b;
        if(!strcmp(nm,"atan2")) return atan2(a,b);
        if(!strcmp(nm,"gcd")||!strcmp(nm,"mcd")){
            long long ia=(long long)fabs(a),ib=(long long)fabs(b);
            while(ib){long long t=ib;ib=ia%ib;ia=t;} return (double)ia;
        }
        if(!strcmp(nm,"lcm")||!strcmp(nm,"mcm")){
            long long ia=(long long)fabs(a),ib=(long long)fabs(b),g=ia,tb=ib;
            while(tb){long long t=tb;tb=g%tb;g=t;} return ia?(double)(ia/g*ib):0;
        }
        _gp_err=1; return 0.0; /* simbolo non riconosciuto — blocca l'espressione */
    }
    char *e; double v=strtod(_gp,&e);
    if(e!=_gp){_gp=e;return v;} return 0.0;
}
static double _gp_pow(void) {
    _gp_ws(); int neg=(*_gp=='-');if(neg||*_gp=='+')_gp++;
    double v=_gp_prim(); _gp_ws();
    if(*_gp=='^'){_gp++;v=pow(v,_gp_pow());}
    if(*_gp=='!'){_gp++;long long n=(long long)fabs(v),f=1;
        for(long long i=2;i<=n&&i<=20;i++)f*=i;v=(double)f;}
    return neg?-v:v;
}
static double _gp_term(void) {
    double v=_gp_pow();
    while(1){_gp_ws();char op=*_gp;
        if(op=='*'||op=='/'||op=='%'){_gp++;double r=_gp_pow();
            if(op=='*')v*=r;else if(op=='/')v=(r?v/r:0);else v=(r?fmod(v,r):0);}
        else break;}
    return v;
}
static double _gp_expr(void) {
    double v=_gp_term();
    while(1){_gp_ws();
        if(*_gp=='+'){_gp++;v+=_gp_term();}
        else if(*_gp=='-'){_gp++;v-=_gp_term();}
        else break;}
    return v;
}

/* ── Formatta numero senza decimali inutili ─────────────────────────── */
static void _gp_fmt(double v, char *buf, int sz) {
    if(v==(long long)v && fabs(v)<1e15) snprintf(buf,sz,"%lld",(long long)v);
    else snprintf(buf,sz,"%.6g",v);
}

/* ── Rileva e risolve matematica semplice — ritorna 1 se gestita ─────── */
static int _guardia_math(const char *input) {
    struct timespec _t0, _t1;
    clock_gettime(CLOCK_MONOTONIC, &_t0);

    char low[512]; int i=0;
    for(;input[i]&&i<511;i++) low[i]=(char)tolower((unsigned char)input[i]);
    low[i]='\0';
    /* Rifiuta stringa vuota o solo spazi */
    { int _hc=0; for(int j=0;low[j];j++) if(!isspace((unsigned char)low[j])){_hc=1;break;}
      if(!_hc) return 0; }

    /* ── Normalizza: forme alternative → operatori canonici ─────────────
       2**3        → 2^3   (stile Python)
       "X elevato a Y"  → X^Y
       "X elevato Y"    → X^Y
       "X alla Y"       → X^Y  (es. "2 alla 3" = 2³)
       ──────────────────────────────────────────────────────────────── */
    for(int j=0; low[j]; j++)
        if(low[j]=='*'&&low[j+1]=='*')
            { low[j]='^'; memmove(low+j+1,low+j+2,strlen(low+j+2)+1); }
    {
        static const struct { const char *s; int len; } kPow[]={
            {" elevato a ",11},{" elevato ",9},{" alla ",6},{NULL,0}
        };
        for(int k=0;kPow[k].s;k++){
            char *p;
            while((p=strstr(low,kPow[k].s))!=NULL){
                *p='^';
                memmove(p+1,p+kPow[k].len,strlen(p+kPow[k].len)+1);
            }
        }
    }

    /* Macro interna: stampa risultato + tempo trascorso */
    #define _PRINT_RESULT(expr, rv) do { \
        clock_gettime(CLOCK_MONOTONIC, &_t1); \
        double _ms = (_t1.tv_sec-_t0.tv_sec)*1000.0 + (_t1.tv_nsec-_t0.tv_nsec)/1e6; \
        printf(GRN "\n  \xe2\x9c\x94  %s = %s\n" RST, expr, rv); \
        printf(GRY "  Tempo: %.3f ms (%.6f s)\n\n" RST, _ms, _ms/1000.0); \
    } while(0)

    /* Sconto: "sconto X% su Y" */
    double pct=0,base=0;
    if(sscanf(low,"sconto %lf%% su %lf",&pct,&base)==2) {
        double risp=base*pct/100.0,fin=base-risp;
        char bv[32],rv[32],fv[32];
        _gp_fmt(base,bv,sizeof bv);_gp_fmt(risp,rv,sizeof rv);_gp_fmt(fin,fv,sizeof fv);
        clock_gettime(CLOCK_MONOTONIC,&_t1);
        double _ms=(_t1.tv_sec-_t0.tv_sec)*1000.0+(_t1.tv_nsec-_t0.tv_nsec)/1e6;
        printf(GRN "\n  \xe2\x9c\x94  Sconto %.4g%% su %s\n" RST, pct, bv);
        printf(GRY "  Risparmio: %s   Prezzo finale: %s\n" RST, rv, fv);
        printf(GRY "  Tempo: %.3f ms (%.6f s)\n\n" RST, _ms, _ms/1000.0);
        return 1;
    }
    /* Percentuale: "X% di Y" */
    if(sscanf(low,"%lf%% di %lf",&pct,&base)==2) {
        char lbl[64],rv[32];
        snprintf(lbl,sizeof lbl,"%.4g%% di %.4g",pct,base);
        _gp_fmt(base*pct/100.0,rv,sizeof rv);
        _PRINT_RESULT(lbl,rv); return 1;
    }
    /* Numeri primi in range */
    long long da=0,a=0;
    if((sscanf(low,"primi da %lld a %lld",&da,&a)==2||
        sscanf(low,"primi tra %lld e %lld",&da,&a)==2) && a-da<100000) {
        printf(GRN "\n  \xe2\x9c\x94  Numeri primi tra %lld e %lld:\n  " RST, da, a);
        int cnt=0;
        for(long long n=da;n<=a;n++){
            int primo=n>1;
            for(long long j=2;j*j<=n&&primo;j++) if(n%j==0)primo=0;
            if(primo){printf(GRY "%lld " RST,n);cnt++;}
        }
        clock_gettime(CLOCK_MONOTONIC,&_t1);
        double _ms=(_t1.tv_sec-_t0.tv_sec)*1000.0+(_t1.tv_nsec-_t0.tv_nsec)/1e6;
        printf(GRN "\n  Totale: %d\n" RST, cnt);
        printf(GRY "  Tempo: %.3f ms (%.6f s)\n\n" RST, _ms, _ms/1000.0);
        return 1;
    }
    /* Sommatoria */
    if((sscanf(low,"somma da %lld a %lld",&da,&a)==2||
        sscanf(low,"somma %lld a %lld",&da,&a)==2) && a>=da) {
        long long s=(da+a)*(a-da+1)/2;
        char lbl[64],sv[32];
        snprintf(lbl,sizeof lbl,"\xce\xa3(%lld..%lld)",da,a);
        _gp_fmt((double)s,sv,sizeof sv);
        _PRINT_RESULT(lbl,sv); return 1;
    }
    /* Linguaggio naturale: "quanto fa 4+4?", "calcola 3*7", ecc. */
    {
        const char *prefissi[]={
            "quanto fa ","quanto vale ","quanto e' ","quanto e ",
            "quanto fa'","calcola ","risultato di ","dimmi ",
            "quanto risulta ","quant'e' ","quante fa ",
            NULL
        };
        for(int k=0;prefissi[k];k++){
            size_t pl=strlen(prefissi[k]);
            if(strncmp(low,prefissi[k],pl)==0){
                /* estrai espressione dopo il prefisso (usa low: già normalizzato) */
                char expr[128];
                strncpy(expr, low+pl, sizeof expr-1);
                expr[sizeof expr-1]='\0';
                /* rimuovi '?' e spazi finali */
                int el=(int)strlen(expr);
                while(el>0&&(expr[el-1]=='?'||expr[el-1]==' '||expr[el-1]=='\n'))
                    expr[--el]='\0';
                if(!el) break;
                _gp_err=0; _gp=expr; double v=_gp_expr(); _gp_ws();
                if(!_gp_err && !*_gp){
                    char rv[32];_gp_fmt(v,rv,sizeof rv);
                    _PRINT_RESULT(expr,rv); return 1;
                }
                break; /* espressione non parsabile, lascia passare */
            }
        }
    }

    /* sqrt / radq / radice [quadrata] [di]: input diretto senza espressione */
    {
        const char *rest=NULL;
        if((strncmp(low,"sqrt",4)==0||strncmp(low,"radq",4)==0)&&
           !isalnum((unsigned char)low[4])&&low[4]!='_')
            rest=low+4;
        else if(strncmp(low,"radice",6)==0&&!isalnum((unsigned char)low[6])&&low[6]!='_'){
            rest=low+6; while(*rest==' ')rest++;
            if(strncmp(rest,"quadrata",8)==0&&!isalnum((unsigned char)rest[8]))
                { rest+=8; }
            while(*rest==' ')rest++;
            if(strncmp(rest,"di",2)==0&&!isalnum((unsigned char)rest[2]))
                { rest+=2; }
        }
        if(rest){
            while(*rest==' ')rest++;
            if(*rest=='(')rest++;
            char *ep; double n=strtod(rest,&ep);
            if(ep!=rest){
                char lbl[32],rv[32];
                snprintf(lbl,sizeof lbl,"sqrt(%.6g)",n);
                _gp_fmt(n>=0.0?sqrt(n):0.0,rv,sizeof rv);
                _PRINT_RESULT(lbl,rv); return 1;
            }
        }
    }

    /* Radice cubica: "radice cubica [di] X" */
    {
        const char *rest=NULL;
        if(strncmp(low,"radice cubica",13)==0&&(low[13]==' '||low[13]=='('))
            rest=low+13;
        if(rest){
            while(*rest==' ')rest++;
            if(strncmp(rest,"di",2)==0&&!isalnum((unsigned char)rest[2]))
                { rest+=2; while(*rest==' ')rest++; }
            if(*rest=='(')rest++;
            char *ep; double n=strtod(rest,&ep);
            if(ep!=rest){
                char lbl[32],rv[32];
                snprintf(lbl,sizeof lbl,"cbrt(%.6g)",n);
                _gp_fmt(cbrt(n),rv,sizeof rv);
                _PRINT_RESULT(lbl,rv); return 1;
            }
        }
    }
    /* Logaritmo naturale: "logaritmo naturale [di] X" / "log naturale [di] X" / "ln [di] X" */
    {
        const char *rest=NULL;
        if(strncmp(low,"logaritmo naturale",18)==0&&(low[18]==' '||low[18]=='('))
            rest=low+18;
        else if(strncmp(low,"log naturale",12)==0&&(low[12]==' '||low[12]=='('))
            rest=low+12;
        else if(strncmp(low,"ln",2)==0&&!isalnum((unsigned char)low[2])&&low[2]!='_')
            rest=low+2;
        if(rest){
            while(*rest==' ')rest++;
            if(strncmp(rest,"di",2)==0&&!isalnum((unsigned char)rest[2]))
                { rest+=2; while(*rest==' ')rest++; }
            if(*rest=='(')rest++;
            char *ep; double n=strtod(rest,&ep);
            if(ep!=rest&&n>0){
                char lbl[32],rv[32];
                snprintf(lbl,sizeof lbl,"ln(%.6g)",n);
                _gp_fmt(log(n),rv,sizeof rv);
                _PRINT_RESULT(lbl,rv); return 1;
            }
        }
    }
    /* Logaritmo base 10: "logaritmo [di] X" (convenzione italiana) */
    {
        const char *rest=NULL;
        if(strncmp(low,"logaritmo",9)==0&&!isalnum((unsigned char)low[9])&&
           strstr(low,"naturale")==NULL)
            rest=low+9;
        if(rest){
            while(*rest==' ')rest++;
            if(strncmp(rest,"di",2)==0&&!isalnum((unsigned char)rest[2]))
                { rest+=2; while(*rest==' ')rest++; }
            if(*rest=='(')rest++;
            char *ep; double n=strtod(rest,&ep);
            if(ep!=rest&&n>0){
                char lbl[32],rv[32];
                snprintf(lbl,sizeof lbl,"log10(%.6g)",n);
                _gp_fmt(log10(n),rv,sizeof rv);
                _PRINT_RESULT(lbl,rv); return 1;
            }
        }
    }
    /* Trig in linguaggio naturale: "seno/coseno/tangente [di] X [gradi/radianti]"
       Supporta sinonimi italiani e conversione gradi→radianti */
    {
        static const struct { const char *nm; double(*fn)(double); } trigs[]={
            {"seno",sin},{"coseno",cos},{"tangente",tan},
            {"sin",sin},{"cos",cos},{"tan",tan},
            {"arcoseno",asin},{"arcocoseno",acos},{"arcotangente",atan},
            {"asin",asin},{"acos",acos},{"atan",atan},{NULL,NULL}
        };
        for(int k=0;trigs[k].nm;k++){
            size_t nl=strlen(trigs[k].nm);
            if(strncmp(low,trigs[k].nm,nl)==0&&
               !isalnum((unsigned char)low[nl])&&low[nl]!='_'){
                const char *rest=low+nl;
                while(*rest==' ')rest++;
                if(strncmp(rest,"di",2)==0&&!isalnum((unsigned char)rest[2]))
                    { rest+=2; while(*rest==' ')rest++; }
                if(*rest=='(')rest++;
                char *ep; double n=strtod(rest,&ep);
                if(ep!=rest){
                    int gradi=strstr(low,"grad")!=NULL; /* "gradi" / "degrees" */
                    double arg=gradi?n*(3.14159265358979323846/180.0):n;
                    double res=trigs[k].fn(arg);
                    char lbl[48],rv[32];
                    snprintf(lbl,sizeof lbl,"%s(%.6g%s)",trigs[k].nm,n,gradi?"\xc2\xb0":"");
                    _gp_fmt(res,rv,sizeof rv);
                    _PRINT_RESULT(lbl,rv); return 1;
                }
                break;
            }
        }
    }
    /* Espressione con funzioni, costanti e operatori (stile Wolfram/Derive/TI)
       _gp_err blocca automaticamente le stringhe con simboli non riconosciuti */
    {
        _gp_err=0; _gp=low; double v=_gp_expr(); _gp_ws();
        if(!_gp_err && !*_gp){char rv[32];_gp_fmt(v,rv,sizeof rv);
            _PRINT_RESULT(low,rv); return 1;}
    }
    #undef _PRINT_RESULT
    return 0;
}

/* ── Rileva se il task sembra programmazione ────────────────────────── */
static int _task_sembra_codice(const char *input) {
    char low[512]; int i=0;
    for(;input[i]&&i<511;i++) low[i]=(char)tolower((unsigned char)input[i]);
    low[i]='\0';
    const char *kw[]={
        "scrivi","crea","implementa","programma","codice","script",
        "funzione","classe","algoritmo","calcola con","automatizza",
        "parser","leggi file","json","csv","api","web","bot",
        "ordina","cerca","trova tutti","genera lista",
        NULL
    };
    for(int k=0;kw[k];k++) if(strstr(low,kw[k])) return 1;
    return 0;
}

/* ── Controllo RAM pre-pipeline con suggerimento concreto ───────────── */
static int _check_ram_pipeline(void) {
    SysRes r = get_resources();
    double rp = (r.ram_total>0) ? r.ram_used/r.ram_total*100.0 : 0;
    if(rp < 75.0) return 1;  /* OK */

    clear_screen(); print_header();
    printf(RED BLD "\n  \xe2\x9a\xa0  RAM alta prima della pipeline: %.0f%%\n\n" RST, rp);
    printf("  La pipeline usa 6 agenti AI in sequenza — richiede RAM libera.\n\n");

    if(rp >= 90.0) {
        printf(RED "  Con %.0f%% di RAM occupata il rischio di crash e' alto.\n\n" RST, rp);
    }

    printf(YLW "  Suggerimenti per liberare RAM:\n" RST);
    printf(GRY "  1. Chiudi app non necessarie (browser, IDE, ecc.)\n");
    printf(GRY "  2. Libera il modello Ollama:\n");
    printf(CYN "       curl -X POST http://localhost:11434/api/generate \\\n");
    printf(CYN "            -d '{\"model\":\"%s\",\"keep_alive\":0}'\n" RST,
           g_ctx.model[0] ? g_ctx.model : "NOME_MODELLO");
    printf(GRY "  3. Usa un modello piu piccolo (es. qwen2.5:3b)\n\n");

    if(rp >= 92.0) {
        printf(RED "  Avvio fortemente sconsigliato. " RST);
        printf("Vuoi procedere lo stesso? [s/N] "); fflush(stdout);
        char yn[4]; if(!fgets(yn,sizeof yn,stdin)) return 0;
        return (yn[0]=='s'||yn[0]=='S') ? 1 : 0;
    }
    printf(YLW "  Vuoi procedere comunque? [s/N] " RST); fflush(stdout);
    char yn[4]; if(!fgets(yn,sizeof yn,stdin)) return 0;
    return (yn[0]=='s'||yn[0]=='S') ? 1 : 0;
}

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

/* ── Cross-platform sleep ────────────────────────────────────────── */
#ifdef _WIN32
#  define MA_SLEEP_MS(ms) Sleep((DWORD)(ms))
#else
#  define MA_SLEEP_MS(ms) usleep((useconds_t)(ms) * 1000u)
#endif

#define MA_BAR_LEN 36  /* larghezza barra progresso in blocchi */

/* ma_progress_bar — anima la barra di avanzamento pipeline.
 * step_da/step_a: range agente corrente (es. 3,4 per i programmatori paralleli).
 * nome: etichetta testuale da mostrare a destra della barra.
 * La barra si colora linearmente da (step_da-1)/6 a step_a/6 con ~55 ms per blocco,
 * usando \r per sovrascrivere la stessa riga ad ogni frame. */
static void ma_progress_bar(int step_da, int step_a, const char *nome) {
    int prev = (step_da - 1) * MA_BAR_LEN / MA_N_AGENTS;
    int targ = step_a       * MA_BAR_LEN / MA_N_AGENTS;
    char label[32];
    if (step_da == step_a)
        snprintf(label, sizeof label, "Agente %d/%d", step_da, MA_N_AGENTS);
    else
        snprintf(label, sizeof label, "Agente %d-%d/%d", step_da, step_a, MA_N_AGENTS);

    for (int f = prev; f <= targ; f++) {
        printf("\r  " CYN BLD "%-14s" RST "  [", label);
        for (int i = 0; i < MA_BAR_LEN; i++) {
            if (i < f) printf(GRN "\xe2\x96\x88" RST);  /* █ pieno */
            else        printf(DIM "\xe2\x96\x91" RST);  /* ░ vuoto */
        }
        printf("]  " YLW "%s" RST "   ", nome);
        fflush(stdout);
        MA_SLEEP_MS(55);
    }
    printf("\n");
}

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
                snprintf(out, outsz, "%s", modelli[d]); return;
            }
        }
    }
    if (nm>0) { snprintf(out, outsz, "%s", modelli[0]); }
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
    /* GetTempPath: directory temp utente (es. C:\Users\user\AppData\Local\Temp\) */
    { char tmp_dir[MAX_PATH];
      GetTempPath(sizeof tmp_dir, tmp_dir);
      /* XOR pid + tick count per nome non predicibile */
      snprintf(tmpfile, sizeof tmpfile, "%sprismalux_%08X.py",
               tmp_dir, (unsigned)(GetCurrentProcessId() ^ GetTickCount())); }
#else
    /* mkstemp: crea file con nome casuale — nessun TOCTOU su /tmp condiviso */
    strncpy(tmpfile, "/tmp/prismalux_ma_XXXXXX", sizeof tmpfile - 1);
    tmpfile[sizeof tmpfile - 1] = '\0';
    { int fd = mkstemp(tmpfile);
      if (fd < 0) { strncpy(output, "[errore creazione file temp]", maxlen-1); return; }
      close(fd); /* chiudiamo il fd; riapriremo con fopen("w") sotto */ }
#endif
    FILE* f = fopen(tmpfile, "w");
    if (!f) { strncpy(output, "[errore creazione file temp]", maxlen-1); return; }
    fputs(code, f); fclose(f);
    char cmd[640];
#ifdef _WIN32
    snprintf(cmd, sizeof cmd, "python \"%s\" 2>&1", tmpfile);
#else
    /* python3 esegue il file per nome: l'estensione .py non è obbligatoria */
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
/*
 * Cerca pattern di errore Python nell'output del subprocess.
 * Tutti i confronti sono case-insensitive (lowercase preventivo).
 *
 * Keyword Python che indicano fallimento:
 *   "traceback"        → stack trace (qualsiasi eccezione non catturata)
 *   "  file \""        → riga del traceback con percorso file
 *   "error:"           → prefisso standard errori Python (SyntaxError:, ecc.)
 *   "exception"        → eccezione generica (anche custom)
 *   "syntaxerror"      → errore di sintassi (indentazione, token errati)
 *   "nameerror"        → variabile/funzione non definita
 *   "typeerror"        → operazione su tipo errato
 *   "indentationerror" → blocco indentato in modo non valido
 *
 * NOTA: un output vuoto ("") passa questo test — è responsabilità
 * del codice generato produrre almeno una riga di output.
 */
static int ma_valuta_output(const char* out) {
    char low[4096]; int n=(int)strlen(out); if(n>4095)n=4095;
    for(int i=0;i<n;i++) low[i]=(char)tolower((unsigned char)out[i]);
    low[n]='\0';
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
        printf("  " BLD "INVIO" RST " = conferma tutto   |   " BLD "1\xe2\x80\x93" "5" RST " = cambia un agente\n");
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
        snprintf(agents[IDXMAP[r]].ctx.model, sizeof(agents[0].ctx.model),
                 "%s", defaults[r]);
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
    ma_progress_bar(1, 1, "Ricercatore");
    printf(YLW BLD "  [1/6]  \xf0\x9f\x94\x8d Ricercatore" RST);
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
    ma_progress_bar(2, 2, "Pianificatore");
    printf(YLW BLD "  [2/6]  \xf0\x9f\x93\x90 Pianificatore" RST);
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
    ma_progress_bar(3, 4, "Programmatori A+B");
    printf(YLW BLD "  [3-4/6]  \xf0\x9f\x92\xbb 2 Programmatori in parallelo" RST);
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

    /* Estrae codice dai due output.
     * Heap: MAX_BUF/2 = 262144 byte × 2 = 524 KB su stack causerebbe crash
     * su Windows (stack default 1 MB) e stack overflow su Linux con guard page. */
    char *code_a = malloc(MAX_BUF/2);
    char *code_b = malloc(MAX_BUF/2);
    if (!code_a || !code_b) {
        free(code_a); free(code_b);
        free(arg_a.output); free(arg_b.output);
        free(ricer_out); free(piano_raw); free(piano_a); free(piano_b);
        free(codice_fin); free(run_out); free(tester_out);
        free(ottim_out); free(input_buf);
        printf(RED "  Memoria insufficiente (code_a/b).\n" RST); return;
    }
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
    printf("\n");
    ma_progress_bar(5, 5, "Tester");
    printf(YLW BLD "  [5/6]  \xf0\x9f\xa7\xaa Tester" RST);
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
    printf("\n");
    ma_progress_bar(6, 6, "Ottimizzatore");
    printf(YLW BLD "  [6/6]  \xe2\x9c\xa8 Ottimizzatore" RST);
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

    /* Estrae codice finale ottimizzato (heap: 256 KB evita stack overflow) */
    char *codice_ottim = malloc(MAX_BUF/2);
    if (codice_ottim) {
        codice_ottim[0]='\0';
        ma_extract_code(ottim_out, codice_ottim, MAX_BUF/2);
        if (codice_ottim[0]) strncpy(codice_fin, codice_ottim, MAX_BUF-1);
        free(codice_ottim);
    }

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

    free(code_a); free(code_b);
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

    /* Menu configurazione: backend, modello globale, modelli per ruolo.
     * Ritorna 0 se l'utente ha scelto di tornare. */
    if (!setup_rapido_multiagente(agents)) return;

    printf("  Descrivi cosa vuoi programmare " GRY "(" EM_0 " o vuoto = torna)" RST ":\n");
    char task[1024];
    if (!input_line("  \xe2\x9d\xaf ", task, sizeof task) || !task[0]) return;
    if (task[0]=='0' && !task[1]) return;

    /* ── Guardia 1: matematica semplice → risposta istantanea ─────── */
    if (_guardia_math(task)) {
        printf(GRY "  [INVIO per tornare] " RST); fflush(stdout);
        { char _b[4]; if(!fgets(_b,sizeof _b,stdin)){} }
        return;
    }

    /* ── Guardia 2: task non sembra programmazione → avviso ───────── */
    if (!_task_sembra_codice(task)) {
        printf(YLW "\n  \xe2\x9a\xa0  Il task non sembra un problema di programmazione.\n" RST);
        printf(GRY "  La pipeline genera codice Python — per domande generali\n"
                   "  usa il Tutor AI (menu Impara con AI).\n\n" RST);
        printf("  Vuoi avviare la pipeline lo stesso? [s/N] "); fflush(stdout);
        char yn[4]; if(!fgets(yn,sizeof yn,stdin)){return;}
        if(yn[0]!='s' && yn[0]!='S') return;
    }

    /* ── Guardia 3: controllo RAM dedicato pre-pipeline ───────────── */
    if (!_check_ram_pipeline()) return;

    multi_agent_run(task, agents);
    printf(GRN "\n  \xf0\x9f\x8d\xba \xc3\x80 prossima libagione di sapere.\n" RST);
    printf(GRY "\n  [INVIO per tornare] " RST); fflush(stdout);
    { char _b[4]; if(!fgets(_b,sizeof _b,stdin)){} }
}

/* ══════════════════════════════════════════════════════════════
   MOTORE BYZANTINO — Verifica a 4 agenti anti-allucinazione
   Pipeline: A (Originale) → B (Avvocato del Diavolo)
           → C (Gemello Indipendente) → D (Giudice Quantico)
   Logica booleana: T = (A ∧ C) ∧ ¬B_valido
   ══════════════════════════════════════════════════════════════ */

#define BQ_BUF 32768

static void bq_stream_print(const char* piece, void* ud) {
    (void)ud; printf(GRN "%s" RST, piece); fflush(stdout);
}

void menu_byzantine_engine(void) {
    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x94\xae  Motore Byzantino \xe2\x80\x94 Verifica a 4 Agenti\n" RST);
    printf(GRY "  Pipeline: "
               CYN "A" GRY " (Esperto) \xe2\x86\x92 "
               RED "B" GRY " (Avvocato del Diavolo) \xe2\x86\x92 "
               YLW "C" GRY " (Gemello) \xe2\x86\x92 "
               MAG "D" GRY " (Giudice Quantico)\n" RST);
    printf(GRY "  Logica: T = (A \xe2\x88\xa7 C) \xe2\x88\xa7 \xc2\xac" "B_valido\n\n" RST);

    if (!backend_ready(&g_ctx)) { ensure_ready_or_return(&g_ctx); return; }
    if (!check_risorse_ok()) return;

    /* Selezione modello LLM per i 4 agenti */
    printf(GRY "  Modello corrente: " CYN "%s\n\n" RST,
           g_ctx.model[0] ? g_ctx.model : "nessuno");
    scegli_modello_rapido();
    printf("\n");

    printf("  Inserisci la query da verificare " GRY "(" EM_0 " o vuoto = torna)" RST ":\n");
    char query[1024];
    if (!input_line("  \xe2\x9d\xaf ", query, sizeof query) || !query[0]) return;
    if (query[0] == '0' && !query[1]) return;

    /* Alloca buffer output per i 4 agenti */
    char *out_a = malloc(BQ_BUF);
    char *out_b = malloc(BQ_BUF);
    char *out_c = malloc(BQ_BUF);
    char *out_d = malloc(BQ_BUF);
    char *prompt_b = malloc(BQ_BUF + 512);
    /* BQ_BUF*4: query(1024) + 3×out(3×32768) + testo fisso(~350) — margine ampio */
    char *prompt_d = malloc(BQ_BUF * 4 + 2048);
    if (!out_a || !out_b || !out_c || !out_d || !prompt_b || !prompt_d) {
        printf(RED "  Errore: memoria insufficiente.\n" RST);
        free(out_a); free(out_b); free(out_c); free(out_d);
        free(prompt_b); free(prompt_d);
        return;
    }

    /* ── [A] Agente Originale / Esperto ──────────────────────── */
    printf(CYN BLD "\n  [A] \xf0\x9f\xa7\xa0 Agente Originale (Esperto):\n" RST);
    ai_chat_stream(&g_ctx,
        "Sei un esperto altamente qualificato. Rispondi SEMPRE e SOLO in italiano "
        "in modo preciso e dettagliato. Fornisci solo fatti verificabili. "
        "Se non sei sicuro di qualcosa, dillo esplicitamente.",
        query, bq_stream_print, NULL, out_a, BQ_BUF);
    printf("\n");

    /* ── [B] Avvocato del Diavolo ─────────────────────────────── */
    printf(RED BLD "\n  [B] \xe2\x9a\x94\xef\xb8\x8f  Avvocato del Diavolo (cerca errori in A):\n" RST);
    snprintf(prompt_b, BQ_BUF + 512,
        "Hai ricevuto questa risposta da un esperto:\n\n%s\n\n"
        "Il tuo UNICO scopo e' trovare errori, contraddizioni, imprecisioni o punti deboli. "
        "Sii critico e severo. "
        "Se la risposta e' completamente corretta, scrivi esattamente: 'NESSUN ERRORE RILEVATO'. "
        "Altrimenti elenca gli errori trovati.", out_a);
    ai_chat_stream(&g_ctx,
        "Sei l'Avvocato del Diavolo. Rispondi SEMPRE e SOLO in italiano. "
        "Il tuo ruolo e' dimostrare che l'affermazione ricevuta e' sbagliata o incompleta. "
        "Cerca contraddizioni, errori logici, storici o tecnici.",
        prompt_b, bq_stream_print, NULL, out_b, BQ_BUF);
    printf("\n");

    /* ── [C] Gemello Indipendente ─────────────────────────────── */
    printf(YLW BLD "\n  [C] \xf0\x9f\x94\x84 Gemello Indipendente (risponde senza vedere A):\n" RST);
    ai_chat_stream(&g_ctx,
        "Sei un validatore indipendente. Rispondi SEMPRE e SOLO in italiano. "
        "NON hai visto la risposta di altri agenti. Rispondi in modo autonomo e obiettivo. "
        "Sii preciso e fornisci solo fatti verificabili.",
        query, bq_stream_print, NULL, out_c, BQ_BUF);
    printf("\n");

    /* ── [D] Giudice Quantico ─────────────────────────────────── */
    printf(MAG BLD "\n  [D] \xe2\x9a\x96\xef\xb8\x8f  Giudice Quantico (verdetto finale):\n" RST);
    snprintf(prompt_d, BQ_BUF * 4 + 2048,
        "Query originale: %s\n\n"
        "RISPOSTA A (Esperto Originale):\n%s\n\n"
        "RISPOSTA B (Avvocato del Diavolo):\n%s\n\n"
        "RISPOSTA C (Gemello Indipendente, non ha visto A):\n%s\n\n"
        "Applica la logica booleana: T = (A concordante con C) AND (B non ha trovato errori validi).\n"
        "Emetti il verdetto:\n"
        "- Se T=1.0: scrivi 'VERDETTO: VERIFICATO' e una sintesi della risposta corretta.\n"
        "- Se T<1.0: scrivi 'VERDETTO: INCERTO' e spiega le discordanze rilevate.",
        query, out_a, out_b, out_c);
    ai_chat_stream(&g_ctx,
        "Sei il Giudice Quantico Super-Osservatore. Rispondi SEMPRE e SOLO in italiano. "
        "Analizza il dibattito tra i tre agenti e emetti un verdetto finale "
        "basato sulla logica booleana. Sii imparziale, preciso e conciso.",
        prompt_d, bq_stream_print, NULL, out_d, BQ_BUF);
    printf("\n");

    /* ── Collision score e verdetto visivo ───────────────────── */
    int b_senza_errori = (strstr(out_b, "NESSUN ERRORE")   != NULL ||
                          strstr(out_b, "nessun errore")   != NULL);
    int d_verificato   = (strstr(out_d, "VERIFICATO")      != NULL);
    double score = (b_senza_errori && d_verificato) ? 1.0 : 0.5;

    printf("\n");
    if (score >= 1.0) {
        printf(GRN BLD "  \xe2\x9c\xa8 T = 1.0 \xe2\x80\x94 VERIFICATO. La verit\xc3\xa0 \xc3\xa8 confermata.\n" RST);
    } else {
        printf(YLW BLD "  \xe2\x9a\xa0  T = 0.5 \xe2\x80\x94 INCERTO. " RST);
        if (!b_senza_errori)
            printf(YLW "L'Avvocato ha sollevato obiezioni valide.\n" RST);
        else
            printf(YLW "Discordanza tra A e C rilevata.\n" RST);
    }

    /* Salvataggio opzionale */
    printf(GRY "\n  \xf0\x9f\x92\xbe Vuoi salvare il risultato? [S/n] " RST); fflush(stdout);
    char yn[8];
    if (fgets(yn, sizeof yn, stdin) &&
        (yn[0]=='S' || yn[0]=='s' || yn[0]=='\n' || yn[0]=='\r')) {
        char *full = malloc(BQ_BUF * 4 + 2048);
        if (full) {
            snprintf(full, BQ_BUF * 4 + 2048,
                "[A - Esperto Originale]\n%s\n\n"
                "[B - Avvocato del Diavolo]\n%s\n\n"
                "[C - Gemello Indipendente]\n%s\n\n"
                "[D - Giudice Quantico]\n%s\n\n"
                "Collision score: %.1f  |  Verdetto: %s",
                out_a, out_b, out_c, out_d,
                score, d_verificato ? "VERIFICATO" : "INCERTO");
            salva_output(query, full);
            free(full);
        }
    }

    free(out_a); free(out_b); free(out_c); free(out_d);
    free(prompt_b); free(prompt_d);
    printf(GRY "\n  [INVIO per tornare] " RST); fflush(stdout);
    { char _b[4]; if(!fgets(_b, sizeof _b, stdin)){} }
}
