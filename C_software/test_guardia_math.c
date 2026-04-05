/* ══════════════════════════════════════════════════════════════════════════
   test_guardia_math.c — Test isolati: parser aritmetico + guardie pipeline
   ══════════════════════════════════════════════════════════════════════════
   Testa in isolamento (zero dipendenze AI/rete):
     PARTE 1 — Parser _gp_expr: espressioni aritmetiche, precedenza, potenza, fattoriale
     PARTE 2 — _gp_fmt: formattazione senza ".0" inutili
     PARTE 3 — Pattern _guardia_math: sconto, %, primi, sommatoria, linguaggio naturale
     PARTE 4 — _task_sembra_codice: rilevamento task programmazione
     PARTE 5 — Edge case: divisione per zero, espressioni vuote, overflow pattern
     PARTE 6 — Stress: 10000 valutazioni consecutive

   Build:
     gcc -O2 -lm -o test_guardia_math test_guardia_math.c

   Esecuzione: ./test_guardia_math
   ══════════════════════════════════════════════════════════════════════════ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <time.h>

/* ── Stub ANSI (silenzio colori nei test) ─────────────────────────────── */
#define GRN ""
#define RED ""
#define RST ""
#define GRY ""
#define BLD ""

/* ── Harness ──────────────────────────────────────────────────────────── */
static int _pass = 0, _fail = 0;
#define SECTION(t) printf("\n\033[36m\033[1m─── %s ───\033[0m\n", t)
#define CHECK(cond, label) do { \
    if (cond) { printf("  \033[32m[OK]\033[0m  %s\n", label); _pass++; } \
    else       { printf("  \033[31m[FAIL]\033[0m %s\n", label); _fail++; } \
} while(0)
#define CHECKF(got, exp, tol, label) do { \
    double _g=(got), _e=(exp), _t=(tol); \
    int _ok = fabs(_g-_e) < _t; \
    if (_ok) { printf("  \033[32m[OK]\033[0m  %s  (= %.6g)\n", label, _g); _pass++; } \
    else      { printf("  \033[31m[FAIL]\033[0m %s  got=%.6g exp=%.6g\n", label, _g, _e); _fail++; } \
} while(0)

/* ════════════════════════════════════════════════════════════════════════
   PARTE 1 — Copia standalone del parser _gp_* (da src/multi_agente.c)
             Wolfram/Derive/TI style: ln, log, sin, cos, tan, exp, cbrt,
             abs, floor, ceil, pi, e, phi, gcd, lcm + sinonimi italiani
   ════════════════════════════════════════════════════════════════════════ */
static const char *_gp;
static int         _gp_err;  /* 1 se simbolo non riconosciuto */
static void   _gp_ws(void)  { while(*_gp==' '||*_gp=='\t') _gp++; }
static double _gp_expr(void);

static double _gp_prim(void) {
    _gp_ws();
    if(*_gp=='('){_gp++;double v=_gp_expr();_gp_ws();if(*_gp==')')_gp++;return v;}
    if(isalpha((unsigned char)*_gp)){
        const char *s=_gp;
        while(isalpha((unsigned char)*_gp)||isdigit((unsigned char)*_gp)) _gp++;
        int len=(int)(_gp-s); char nm[16]={};
        for(int i=0;i<len&&i<15;i++) nm[i]=(char)tolower((unsigned char)s[i]);
        _gp_ws();
        /* costanti */
        if(!strcmp(nm,"pi"))   return 3.14159265358979323846;
        if(!strcmp(nm,"phi"))  return 1.6180339887498948;
        if(!strcmp(nm,"e") && *_gp!='(') return 2.71828182845904523536;
        /* parole italiane di contesto */
        if(!strcmp(nm,"di")||!strcmp(nm,"del")||!strcmp(nm,"della"))
            { _gp_ws(); return _gp_prim(); }
        /* special: radice [cubica|quadrata] [di] X */
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
        /* special: logaritmo [naturale] [di] X */
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
        double a=0.0,b=0.0; int nargs=0;
        if(*_gp=='('){_gp++;a=_gp_expr();_gp_ws();nargs=1;b=a;
            if(*_gp==','){_gp++;b=_gp_expr();_gp_ws();nargs=2;}
            if(*_gp==')')_gp++;
        } else { a=_gp_prim(); b=a; nargs=1; }
        if(!strcmp(nm,"sqrt")||!strcmp(nm,"radq")) return a>=0?sqrt(a):0.0;
        if(!strcmp(nm,"cbrt"))  return cbrt(a);
        if(!strcmp(nm,"abs"))   return fabs(a);
        if(!strcmp(nm,"floor")) return floor(a);
        if(!strcmp(nm,"ceil"))  return ceil(a);
        if(!strcmp(nm,"round")) return round(a);
        if(!strcmp(nm,"trunc")) return trunc(a);
        if(!strcmp(nm,"sign"))  return (double)((a>0)-(a<0));
        if(!strcmp(nm,"exp"))   return exp(a);
        if(!strcmp(nm,"ln"))    return a>0?log(a):0.0;
        if(!strcmp(nm,"log2"))  return a>0?log2(a):0.0;
        if(!strcmp(nm,"log10")||!strcmp(nm,"lg")) return a>0?log10(a):0.0;
        if(!strcmp(nm,"log"))   return nargs==1?(a>0?log10(a):0.0)
                                              :(a>0&&b>0&&b!=1?log(a)/log(b):0.0);
        if(!strcmp(nm,"logaritmo")) return a>0?log10(a):0.0;
        if(!strcmp(nm,"sin")||!strcmp(nm,"seno"))     return sin(a);
        if(!strcmp(nm,"cos")||!strcmp(nm,"coseno"))   return cos(a);
        if(!strcmp(nm,"tan")||!strcmp(nm,"tangente")) return tan(a);
        if(!strcmp(nm,"cot"))   return cos(a)/sin(a);
        if(!strcmp(nm,"sec"))   return 1.0/cos(a);
        if(!strcmp(nm,"csc"))   return 1.0/sin(a);
        if(!strcmp(nm,"asin")||!strcmp(nm,"arcsin")||!strcmp(nm,"arcoseno"))   return asin(a);
        if(!strcmp(nm,"acos")||!strcmp(nm,"arccos")||!strcmp(nm,"arcocoseno")) return acos(a);
        if(!strcmp(nm,"atan")||!strcmp(nm,"arctan")||!strcmp(nm,"arcotangente"))
            return nargs==2?atan2(a,b):atan(a);
        if(!strcmp(nm,"sinh")) return sinh(a);
        if(!strcmp(nm,"cosh")) return cosh(a);
        if(!strcmp(nm,"tanh")) return tanh(a);
        if(!strcmp(nm,"pow"))  return pow(a,b);
        if(!strcmp(nm,"min"))  return a<b?a:b;
        if(!strcmp(nm,"max"))  return a>b?a:b;
        if(!strcmp(nm,"atan2"))return atan2(a,b);
        if(!strcmp(nm,"gcd")||!strcmp(nm,"mcd")){
            long long ia=(long long)fabs(a),ib=(long long)fabs(b);
            while(ib){long long t=ib;ib=ia%ib;ia=t;} return (double)ia;
        }
        if(!strcmp(nm,"lcm")||!strcmp(nm,"mcm")){
            long long ia=(long long)fabs(a),ib=(long long)fabs(b),g=ia,tb=ib;
            while(tb){long long t=tb;tb=g%tb;g=t;} return ia?(double)(ia/g*ib):0;
        }
        _gp_err=1; return 0.0;
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

/* ── Wrapper testabile ────────────────────────────────────────────────── */
static double gp_eval(const char *expr) {
    _gp_err=0; _gp = expr;
    double v = _gp_expr();
    _gp_ws();
    return v;
}
/* Ritorna 1 se l'espressione è stata consumata completamente senza errori */
static int gp_complete(const char *expr) {
    _gp_err=0; _gp = expr;
    _gp_expr();
    _gp_ws();
    return (!_gp_err && *_gp == '\0');
}

/* ── _gp_fmt: formatta senza ".0" inutili ─────────────────────────────── */
static void _gp_fmt(double v, char *buf, int sz) {
    if(v==(long long)v && fabs(v)<1e15) snprintf(buf,sz,"%lld",(long long)v);
    else snprintf(buf,sz,"%.6g",v);
}

/* ════════════════════════════════════════════════════════════════════════
   PARTE 3 — Pattern recognition: copia standalone di _guardia_math
   (versione semplificata: ritorna 1 se pattern riconosciuto, 0 altrimenti)
   ════════════════════════════════════════════════════════════════════════ */
static int gm_detect(const char *input) {
    char low[512]; int i=0;
    for(;input[i]&&i<511;i++) low[i]=(char)tolower((unsigned char)input[i]);
    low[i]='\0';
    /* Rifiuta stringa vuota o solo spazi */
    { int has_c=0; for(int j=0;low[j];j++) if(!isspace((unsigned char)low[j])){has_c=1;break;}
      if(!has_c) return 0; }

    /* Normalizza ** → ^ (Python style) */
    for(int j=0;low[j];j++){
        if(low[j]=='*'&&low[j+1]=='*'){
            low[j]='^';
            memmove(low+j+1,low+j+2,strlen(low+j+2)+1);
        }
    }
    /* Normalizza "X elevato a Y" / "X elevato Y" → "X^Y" (semplificato) */
    {
        char *p;
        if((p=strstr(low," elevato a "))!=NULL){
            *p='^'; memmove(p+1,p+11,strlen(p+11)+1);
        } else if((p=strstr(low," elevato "))!=NULL){
            *p='^'; memmove(p+1,p+9,strlen(p+9)+1);
        }
    }

    double pct=0,base=0;
    if(sscanf(low,"sconto %lf%% su %lf",&pct,&base)==2) return 1;
    if(sscanf(low,"%lf%% di %lf",&pct,&base)==2)         return 1;

    long long da=0,a=0;
    if((sscanf(low,"primi da %lld a %lld",&da,&a)==2||
        sscanf(low,"primi tra %lld e %lld",&da,&a)==2) && a-da<100000) return 1;
    if((sscanf(low,"somma da %lld a %lld",&da,&a)==2||
        sscanf(low,"somma %lld a %lld",&da,&a)==2) && a>=da) return 1;

    const char *prefissi[]={
        "quanto fa ","quanto vale ","quanto e' ","quanto e ",
        "quanto fa'","calcola ","risultato di ","dimmi ",
        "quanto risulta ","quant'e' ","quante fa ",NULL};
    for(int k=0;prefissi[k];k++){
        size_t pl=strlen(prefissi[k]);
        if(strncmp(low,prefissi[k],pl)==0){
            char expr[128];
            strncpy(expr,low+pl,sizeof expr-1); expr[sizeof expr-1]='\0';
            int el=(int)strlen(expr);
            while(el>0&&(expr[el-1]=='?'||expr[el-1]==' '||expr[el-1]=='\n'))
                expr[--el]='\0';
            if(!el) break;
            _gp_err=0; _gp=expr; _gp_expr(); _gp_ws();
            if(!_gp_err && !*_gp) return 1;
            break;
        }
    }

    /* Radice cubica */
    if(strncmp(low,"radice cubica",13)==0&&(low[13]==' '||low[13]=='(')) return 1;
    /* Logaritmo naturale / ln */
    if(strncmp(low,"logaritmo naturale",18)==0) return 1;
    if(strncmp(low,"log naturale",12)==0) return 1;
    if(strncmp(low,"ln",2)==0&&!isalnum((unsigned char)low[2])&&low[2]!='_') return 1;
    /* Logaritmo base 10 */
    if(strncmp(low,"logaritmo",9)==0&&!isalnum((unsigned char)low[9])&&
       strstr(low,"naturale")==NULL) return 1;
    /* Trig in linguaggio naturale */
    { static const char *t[]={"seno","coseno","tangente","arcoseno","arcocoseno","arcotangente",NULL};
      for(int k=0;t[k];k++){size_t nl=strlen(t[k]);
        if(strncmp(low,t[k],nl)==0&&!isalnum((unsigned char)low[nl]))return 1;} }

    /* Espressione con funzioni Wolfram/Derive — _gp_err blocca simboli sconosciuti */
    {
        _gp_err=0; _gp=low; _gp_expr(); _gp_ws();
        if(!_gp_err && !*_gp) return 1;
    }
    return 0;
}

/* ════════════════════════════════════════════════════════════════════════
   PARTE 4 — Copia standalone di _task_sembra_codice
   ════════════════════════════════════════════════════════════════════════ */
static int task_sembra_codice(const char *input) {
    char low[512]; int i=0;
    for(;input[i]&&i<511;i++) low[i]=(char)tolower((unsigned char)input[i]);
    low[i]='\0';
    const char *kw[]={
        "scrivi","crea","implementa","programma","codice","script",
        "funzione","classe","algoritmo","calcola con","automatizza",
        "parser","leggi file","json","csv","api","web","bot",
        "ordina","cerca","trova tutti","genera lista",NULL};
    for(int k=0;kw[k];k++) if(strstr(low,kw[k])) return 1;
    return 0;
}

/* ════════════════════════════════════════════════════════════════════════
   MAIN
   ════════════════════════════════════════════════════════════════════════ */
int main(void) {
    printf("\n\033[1m══ test_guardia_math.c — Parser aritmetico + Guardie pipeline ══\033[0m\n");

    /* ── PARTE 1: Parser — operazioni base ── */
    SECTION("Parser — operazioni base");
    CHECKF(gp_eval("2+3"),         5.0,   1e-9, "2+3 = 5");
    CHECKF(gp_eval("10-4"),        6.0,   1e-9, "10-4 = 6");
    CHECKF(gp_eval("3*7"),        21.0,   1e-9, "3*7 = 21");
    CHECKF(gp_eval("15/4"),        3.75,  1e-9, "15/4 = 3.75");
    CHECKF(gp_eval("17%5"),        2.0,   1e-9, "17%5 = 2 (modulo)");
    CHECKF(gp_eval("100"),       100.0,   1e-9, "100 = 100 (numero solo)");
    CHECKF(gp_eval("-5"),         -5.0,   1e-9, "-5 = -5 (negativo)");
    CHECKF(gp_eval("+3"),          3.0,   1e-9, "+3 = 3 (positivo esplicito)");

    SECTION("Parser — precedenza operatori");
    CHECKF(gp_eval("2+3*4"),      14.0,   1e-9, "2+3*4 = 14 (precedenza *)");
    CHECKF(gp_eval("(2+3)*4"),    20.0,   1e-9, "(2+3)*4 = 20 (parentesi)");
    CHECKF(gp_eval("10-2-3"),      5.0,   1e-9, "10-2-3 = 5 (left-assoc)");
    CHECKF(gp_eval("2*3+4*5"),    26.0,   1e-9, "2*3+4*5 = 26");
    CHECKF(gp_eval("(1+2)*(3+4)"),21.0,   1e-9, "(1+2)*(3+4) = 21");
    CHECKF(gp_eval("100/5/4"),     5.0,   1e-9, "100/5/4 = 5 (div left-assoc)");

    SECTION("Parser — potenza e fattoriale");
    CHECKF(gp_eval("2^10"),     1024.0,   1e-9, "2^10 = 1024");
    CHECKF(gp_eval("3^3"),        27.0,   1e-9, "3^3 = 27");
    CHECKF(gp_eval("2^2^3"),     256.0,   1e-9, "2^2^3 = 256 (right-assoc)");
    CHECKF(gp_eval("5!"),        120.0,   1e-9, "5! = 120 (fattoriale)");
    CHECKF(gp_eval("3!"),          6.0,   1e-9, "3! = 6");
    CHECKF(gp_eval("0!"),          1.0,   1e-9, "0! = 1 (per convenzione f=1)");
    CHECKF(gp_eval("2+3!"),        8.0,   1e-9, "2+3! = 8 (fattoriale > somma)");

    SECTION("Parser — espressioni composite");
    CHECKF(gp_eval("1+2+3+4+5"),  15.0,   1e-9, "1+2+3+4+5 = 15");
    CHECKF(gp_eval("2^3*5"),      40.0,   1e-9, "2^3*5 = 40 (potenza > prodotto)");
    CHECKF(gp_eval("(2+3)^2"),    25.0,   1e-9, "(2+3)^2 = 25");
    CHECKF(gp_eval("100-5*3+2"),  87.0,   1e-9, "100-5*3+2 = 87");
    CHECKF(gp_eval("((4))"),       4.0,   1e-9, "((4)) = 4 (doppie parentesi)");
    CHECKF(gp_eval(" 3 + 4 "),     7.0,   1e-9, "3 + 4 con spazi = 7");

    SECTION("Parser — edge case numerici");
    CHECKF(gp_eval("0"),           0.0,   1e-9, "0 = 0");
    CHECKF(gp_eval("1/0"),         0.0,   1e-9, "1/0 = 0 (sicuro: no crash)");
    CHECKF(gp_eval("0%0"),         0.0,   1e-9, "0%0 = 0 (sicuro: no crash)");
    CHECKF(gp_eval("1000000*1000000"), 1e12, 1.0, "1e6*1e6 = 1e12");
    CHECKF(gp_eval("0.5+0.5"),     1.0,   1e-9, "0.5+0.5 = 1");
    CHECKF(gp_eval("3.14*2"),      6.28,  1e-9, "3.14*2 = 6.28");

    SECTION("Parser — completezza espressione");
    CHECK(gp_complete("2+3"),        "2+3 consumata completamente");
    CHECK(gp_complete("(2+3)*4"),    "(2+3)*4 consumata completamente");
    CHECK(!gp_complete("2+3 ciao"),  "2+3 ciao: rifiutata (testo extra)");
    CHECK(gp_complete(" 7 "),        "7 con spazi: consumata");

    /* ── PARTE 2: _gp_fmt ── */
    SECTION("_gp_fmt — formattazione numeri");
    {
        char buf[32];
        _gp_fmt(42.0, buf, sizeof buf);
        CHECK(strcmp(buf,"42")==0,     "_gp_fmt(42.0) → \"42\" (no .0)");
        _gp_fmt(-7.0, buf, sizeof buf);
        CHECK(strcmp(buf,"-7")==0,     "_gp_fmt(-7.0) → \"-7\"");
        _gp_fmt(0.0, buf, sizeof buf);
        CHECK(strcmp(buf,"0")==0,      "_gp_fmt(0.0) → \"0\"");
        _gp_fmt(3.14, buf, sizeof buf);
        CHECK(strncmp(buf,"3.14",4)==0,"_gp_fmt(3.14) → \"3.14...\"");
        _gp_fmt(1e16, buf, sizeof buf);
        CHECK(strstr(buf,"e")!=NULL||strlen(buf)>5,
              "_gp_fmt(1e16) → notazione scientifica (>1e15)");
    }

    /* ── PARTE 3: Pattern _guardia_math ── */
    SECTION("Guardia — sconto");
    CHECK(gm_detect("sconto 20% su 150"),    "sconto 20% su 150 → riconosciuto");
    CHECK(gm_detect("sconto 10% su 50"),     "sconto 10% su 50 → riconosciuto");
    CHECK(gm_detect("sconto 0% su 100"),     "sconto 0% su 100 → riconosciuto");
    CHECK(gm_detect("sconto 100% su 200"),   "sconto 100% su 200 → riconosciuto");
    CHECK(!gm_detect("sconto su 100"),       "sconto su 100 (no %) → non riconosciuto");

    SECTION("Guardia — percentuale");
    CHECK(gm_detect("20% di 500"),           "20% di 500 → riconosciuto");
    CHECK(gm_detect("7.5% di 1000"),         "7.5% di 1000 → riconosciuto");
    CHECK(gm_detect("100% di 42"),           "100% di 42 → riconosciuto");
    CHECK(!gm_detect("percentuale di venti"),"percentuale di venti → non riconosciuto");

    SECTION("Guardia — numeri primi");
    CHECK(gm_detect("primi da 1 a 20"),      "primi da 1 a 20 → riconosciuto");
    CHECK(gm_detect("primi tra 10 e 50"),    "primi tra 10 e 50 → riconosciuto");
    CHECK(gm_detect("primi da 0 a 100"),     "primi da 0 a 100 → riconosciuto");
    CHECK(!gm_detect("i numeri primi"),      "i numeri primi (no range) → non riconosciuto");

    SECTION("Guardia — sommatoria");
    CHECK(gm_detect("somma da 1 a 100"),     "somma da 1 a 100 → riconosciuto");
    CHECK(gm_detect("somma 1 a 50"),         "somma 1 a 50 → riconosciuto");
    CHECK(gm_detect("somma da 0 a 0"),       "somma da 0 a 0 → riconosciuto");
    CHECK(!gm_detect("somma i numeri"),      "somma i numeri → non riconosciuto");

    SECTION("Guardia — linguaggio naturale");
    CHECK(gm_detect("quanto fa 4+4"),        "quanto fa 4+4 → riconosciuto");
    CHECK(gm_detect("calcola 3*7"),          "calcola 3*7 → riconosciuto");
    CHECK(gm_detect("quanto vale 2^8"),      "quanto vale 2^8 → riconosciuto");
    CHECK(gm_detect("risultato di 100/4"),   "risultato di 100/4 → riconosciuto");
    CHECK(gm_detect("dimmi 1+1"),            "dimmi 1+1 → riconosciuto");
    CHECK(gm_detect("quanto fa 5!"),         "quanto fa 5! → riconosciuto");
    CHECK(gm_detect("quant'e' 3+2"),         "quant'e' 3+2 → riconosciuto");
    CHECK(!gm_detect("quanto fa la pasta"),  "quanto fa la pasta → non riconosciuto");
    CHECK(!gm_detect("calcola l'IVA del 22% in modo teorico"),
                                             "calcola l'IVA... testo misto → non riconosciuto");

    SECTION("Guardia — espressione pura");
    CHECK(gm_detect("2+3"),                  "2+3 espressione pura → riconosciuto");
    CHECK(gm_detect("100*3+50"),             "100*3+50 → riconosciuto");
    CHECK(!gm_detect("scrivi un programma"), "scrivi un programma → non riconosciuto");
    CHECK(!gm_detect("calcola l'algoritmo"), "calcola l'algoritmo (lettera extra) → non riconosciuto");

    SECTION("Guardia — sqrt / radq / radice quadrata");
    /* sqrt e radq come keyword della guardia */
    CHECK(gm_detect("sqrt 16"),              "sqrt 16 → riconosciuto");
    CHECK(gm_detect("sqrt(25)"),             "sqrt(25) → riconosciuto");
    CHECK(gm_detect("radq 9"),               "radq 9 → riconosciuto");
    CHECK(gm_detect("radq(49)"),             "radq(49) → riconosciuto");
    CHECK(gm_detect("radice di 4"),          "radice di 4 → riconosciuto");
    CHECK(gm_detect("radice quadrata di 64"),"radice quadrata di 64 → riconosciuto");
    /* come linguaggio naturale */
    CHECK(gm_detect("quanto fa sqrt(9)"),    "quanto fa sqrt(9) → riconosciuto");
    CHECK(gm_detect("calcola radq 100"),     "calcola radq 100 → riconosciuto");
    /* in espressioni composite */
    CHECK(gm_detect("sqrt(16)+4"),           "sqrt(16)+4 espressione → riconosciuto");

    SECTION("Guardia — potenza: ^ / ** / elevato a");
    /* stile standard: ^ */
    CHECK(gm_detect("2^10"),                 "2^10 → riconosciuto");
    CHECK(gm_detect("3^3"),                  "3^3 → riconosciuto");
    /* stile Python: ** */
    CHECK(gm_detect("2**3"),                 "2**3 (Python) → riconosciuto");
    CHECK(gm_detect("5**2"),                 "5**2 (Python) → riconosciuto");
    /* linguaggio naturale: elevato a */
    CHECK(gm_detect("2 elevato a 10"),       "2 elevato a 10 → riconosciuto");
    CHECK(gm_detect("3 elevato a 3"),        "3 elevato a 3 → riconosciuto");
    /* in linguaggio naturale composto */
    CHECK(gm_detect("quanto fa 2**8"),       "quanto fa 2**8 → riconosciuto");
    CHECK(gm_detect("calcola 2 elevato a 3"),"calcola 2 elevato a 3 → riconosciuto");

    SECTION("Guardia — parser: sqrt/radq/** nelle espressioni");
    /* valori del parser (gp_eval usa _gp_prim con sqrt/radq/radice) */
    CHECKF(gp_eval("sqrt(16)"),        4.0,  1e-9, "sqrt(16) = 4");
    CHECKF(gp_eval("sqrt 25"),         5.0,  1e-9, "sqrt 25 = 5");
    CHECKF(gp_eval("radq 9"),          3.0,  1e-9, "radq 9 = 3");
    CHECKF(gp_eval("radq(49)"),        7.0,  1e-9, "radq(49) = 7");
    CHECKF(gp_eval("radice di 4"),     2.0,  1e-9, "radice di 4 = 2");
    CHECKF(gp_eval("radice quadrata di 100"), 10.0, 1e-9, "radice quadrata di 100 = 10");
    CHECKF(gp_eval("sqrt(16)+4"),      8.0,  1e-9, "sqrt(16)+4 = 8");
    CHECKF(gp_eval("sqrt(9)*sqrt(4)"), 6.0,  1e-9, "sqrt(9)*sqrt(4) = 6");
    /* ** viene normalizzato a ^ dalla guardia — il parser usa ^ */
    CHECKF(gp_eval("2^8"),           256.0,  1e-9, "2^8 = 256 (verifica parser)");
    CHECKF(gp_eval("2^3*5"),          40.0,  1e-9, "2^3*5 = 40 (potenza > prodotto)");
    /* sqrt negativo → 0 (sicuro, no NaN) */
    CHECKF(gp_eval("sqrt(-9)"),        0.0,  1e-9, "sqrt(-9) = 0 (sicuro)");

    /* ── Nuove SEZIONI: funzioni Wolfram/Derive/TI ── */
    SECTION("Parser — costanti pi / e / phi");
    CHECKF(gp_eval("pi"),           3.14159265358979, 1e-12, "pi = 3.14159...");
    CHECKF(gp_eval("e"),            2.71828182845905, 1e-12, "e = 2.71828...");
    CHECKF(gp_eval("phi"),          1.61803398874989, 1e-12, "phi = 1.61803...");
    CHECKF(gp_eval("2*pi"),         6.28318530717959, 1e-12, "2*pi = 6.2831...");
    CHECKF(gp_eval("e^2"),          7.38905609893065, 1e-10, "e^2 = 7.389...");
    CHECK( gp_complete("pi+e"),     "pi+e consumata completamente");
    CHECK(!gp_complete("pippo+3"),  "pippo+3: simbolo sconosciuto rifiutato");

    SECTION("Parser — logaritmi (TI Derive: log=log10, ln=naturale)");
    CHECKF(gp_eval("ln(1)"),        0.0,              1e-12, "ln(1) = 0");
    CHECKF(gp_eval("ln(e)"),        1.0,              1e-10, "ln(e) = 1");
    CHECKF(gp_eval("log(100)"),     2.0,              1e-12, "log(100) = 2 (log10)");
    CHECKF(gp_eval("log(1000)"),    3.0,              1e-12, "log(1000) = 3");
    CHECKF(gp_eval("log10(10)"),    1.0,              1e-12, "log10(10) = 1");
    CHECKF(gp_eval("log2(8)"),      3.0,              1e-12, "log2(8) = 3");
    CHECKF(gp_eval("log(8,2)"),     3.0,              1e-12, "log(8,2) = log_2(8) = 3");
    CHECKF(gp_eval("ln(100)/ln(10)"),2.0,             1e-10, "ln(100)/ln(10) = 2");
    CHECKF(gp_eval("exp(0)"),       1.0,              1e-12, "exp(0) = 1");
    CHECKF(gp_eval("exp(1)"),       2.71828182845905, 1e-10, "exp(1) = e");

    SECTION("Parser — trigonometria (radianti + sinonimi italiani)");
    CHECKF(gp_eval("sin(0)"),       0.0,              1e-12, "sin(0) = 0");
    CHECKF(gp_eval("cos(0)"),       1.0,              1e-12, "cos(0) = 1");
    CHECKF(gp_eval("tan(0)"),       0.0,              1e-12, "tan(0) = 0");
    CHECKF(gp_eval("seno(0)"),      0.0,              1e-12, "seno(0) = 0 (sinonimo IT)");
    CHECKF(gp_eval("coseno(0)"),    1.0,              1e-12, "coseno(0) = 1 (sinonimo IT)");
    CHECKF(gp_eval("sin(pi/6)"),    0.5,              1e-12, "sin(pi/6) = 0.5");
    CHECKF(gp_eval("cos(pi/3)"),    0.5,              1e-12, "cos(pi/3) = 0.5");
    CHECKF(gp_eval("sin(pi/2)"),    1.0,              1e-12, "sin(pi/2) = 1");
    CHECKF(gp_eval("asin(1)"),      3.14159265358979/2, 1e-12, "asin(1) = pi/2");
    CHECKF(gp_eval("atan(1)*4"),    3.14159265358979, 1e-12, "atan(1)*4 = pi");
    CHECKF(gp_eval("sin(pi/6)^2+cos(pi/6)^2"), 1.0, 1e-12, "sin²+cos²=1 (identità)");

    SECTION("Parser — radici e valore assoluto");
    CHECKF(gp_eval("cbrt(8)"),      2.0,              1e-12, "cbrt(8) = 2");
    CHECKF(gp_eval("cbrt(27)"),     3.0,              1e-12, "cbrt(27) = 3");
    CHECKF(gp_eval("cbrt(-8)"),    -2.0,              1e-12, "cbrt(-8) = -2");
    CHECKF(gp_eval("abs(-5)"),      5.0,              1e-12, "abs(-5) = 5");
    CHECKF(gp_eval("abs(3)"),       3.0,              1e-12, "abs(3) = 3");
    CHECKF(gp_eval("floor(3.7)"),   3.0,              1e-12, "floor(3.7) = 3");
    CHECKF(gp_eval("ceil(3.2)"),    4.0,              1e-12, "ceil(3.2) = 4");
    CHECKF(gp_eval("round(3.5)"),   4.0,              1e-12, "round(3.5) = 4");

    SECTION("Parser — funzioni composite stile Derive");
    CHECKF(gp_eval("sqrt(sin(pi/2)^2+cos(pi/2)^2)"), 1.0, 1e-12, "sqrt(sin²+cos²) = 1");
    CHECKF(gp_eval("ln(e^3)"),      3.0,              1e-10, "ln(e^3) = 3");
    CHECKF(gp_eval("log(10^3)"),    3.0,              1e-12, "log(10^3) = 3");
    CHECKF(gp_eval("2^log2(16)"),  16.0,              1e-10, "2^log2(16) = 16");
    CHECKF(gp_eval("exp(ln(5))"),   5.0,              1e-10, "exp(ln(5)) = 5");
    CHECKF(gp_eval("max(3,7)"),     7.0,              1e-12, "max(3,7) = 7");
    CHECKF(gp_eval("min(3,7)"),     3.0,              1e-12, "min(3,7) = 3");
    CHECKF(gp_eval("gcd(12,8)"),    4.0,              1e-12, "gcd(12,8) = 4");
    CHECKF(gp_eval("mcd(12,8)"),    4.0,              1e-12, "mcd(12,8) = 4 (IT alias)");
    CHECKF(gp_eval("lcm(4,6)"),    12.0,              1e-12, "lcm(4,6) = 12");

    SECTION("Guardia — parser rifiuta testo libero (regressione)");
    CHECK(!gm_detect("scrivi una funzione"),   "scrivi una funzione → rifiutato");
    CHECK(!gm_detect("quanto fa la pasta"),    "quanto fa la pasta → rifiutato");
    CHECK(!gm_detect("calcola l'algoritmo"),   "calcola l'algoritmo → rifiutato");
    CHECK(!gm_detect("chi e' pitagora"),       "chi e' pitagora → rifiutato");
    CHECK(!gm_detect("spiegami il calcolo"),   "spiegami il calcolo → rifiutato");

    SECTION("Guardia — nuove funzioni: ln/log/trig/cbrt");
    /* rilevamento diretto */
    CHECK(gm_detect("ln 2"),                   "ln 2 → riconosciuto");
    CHECK(gm_detect("ln(2.718)"),              "ln(2.718) → riconosciuto");
    CHECK(gm_detect("logaritmo naturale di 100"), "logaritmo naturale di 100 → riconosciuto");
    CHECK(gm_detect("logaritmo di 1000"),      "logaritmo di 1000 → riconosciuto");
    CHECK(gm_detect("radice cubica di 27"),    "radice cubica di 27 → riconosciuto");
    CHECK(gm_detect("seno di 0"),              "seno di 0 → riconosciuto");
    CHECK(gm_detect("coseno di 0"),            "coseno di 0 → riconosciuto");
    CHECK(gm_detect("tangente di 0"),          "tangente di 0 → riconosciuto");
    CHECK(gm_detect("arcoseno di 1"),          "arcoseno di 1 → riconosciuto");
    /* espressioni con funzioni (parser Wolfram/Derive) */
    CHECK(gm_detect("ln(e)"),                  "ln(e) espressione → riconosciuto");
    CHECK(gm_detect("log(100)"),               "log(100) espressione → riconosciuto");
    CHECK(gm_detect("sin(pi/6)"),              "sin(pi/6) espressione → riconosciuto");
    CHECK(gm_detect("seno(pi/6)"),             "seno(pi/6) IT alias → riconosciuto");
    CHECK(gm_detect("cbrt(27)"),               "cbrt(27) → riconosciuto");
    CHECK(gm_detect("exp(1)"),                 "exp(1) → riconosciuto");
    CHECK(gm_detect("2*pi"),                   "2*pi costante → riconosciuto");
    CHECK(gm_detect("e^2"),                    "e^2 costante → riconosciuto");

    SECTION("Guardia — valori corretti funzioni (CHECKF)");
    CHECKF(gp_eval("ln di 1"),      0.0,   1e-12, "ln di 1 = 0 (con skip 'di')");
    CHECKF(gp_eval("log di 100"),   2.0,   1e-12, "log di 100 = 2 (log10 + skip 'di')");
    CHECKF(gp_eval("seno di 0"),    0.0,   1e-12, "seno di 0 = 0 (trig IT)");
    CHECKF(gp_eval("coseno di 0"),  1.0,   1e-12, "coseno di 0 = 1 (trig IT)");
    CHECKF(gp_eval("cbrt di 8"),    2.0,   1e-12, "cbrt di 8 = 2 (con skip 'di')");
    CHECKF(gp_eval("sqrt di 9"),    3.0,   1e-12, "sqrt di 9 = 3 (con skip 'di')");
    CHECKF(gp_eval("abs(-42)"),    42.0,   1e-12, "abs(-42) = 42");
    CHECKF(gp_eval("floor(3.9)"),   3.0,   1e-12, "floor(3.9) = 3");
    CHECKF(gp_eval("ceil(3.1)"),    4.0,   1e-12, "ceil(3.1) = 4");
    CHECKF(gp_eval("log(8,2)"),     3.0,   1e-12, "log(8,2) = log_2(8) = 3");
    CHECKF(gp_eval("log(1000000,10)"), 6.0,1e-10, "log(1e6,10) = 6");

    /* ── PARTE 4: _task_sembra_codice ── */
    SECTION("_task_sembra_codice — task riconosciuto come codice");
    CHECK(task_sembra_codice("scrivi una funzione Python"),     "scrivi una funzione Python");
    CHECK(task_sembra_codice("crea un algoritmo di ordinamento"),"crea un algoritmo");
    CHECK(task_sembra_codice("implementa un parser JSON"),       "implementa un parser JSON");
    CHECK(task_sembra_codice("programma un bot Telegram"),       "programma un bot");
    CHECK(task_sembra_codice("leggi file CSV e analizza"),       "leggi file CSV");
    CHECK(task_sembra_codice("crea un'API REST"),                "crea un'API REST");
    CHECK(task_sembra_codice("trova tutti i duplicati"),         "trova tutti i duplicati");
    CHECK(task_sembra_codice("genera lista di numeri"),          "genera lista");

    SECTION("_task_sembra_codice — task NON di programmazione");
    CHECK(!task_sembra_codice("quanto fa 2+2"),       "quanto fa 2+2 → non codice");
    CHECK(!task_sembra_codice("spiegami la relatività"),"spiegami la relatività → non codice");
    CHECK(!task_sembra_codice("dimmi il meteo di Roma"),"meteo Roma → non codice");
    CHECK(!task_sembra_codice("riassumi questo testo"),"riassumi testo → non codice");

    /* ── PARTE 5: edge case ── */
    SECTION("Edge case — pattern limite");
    CHECK(!gm_detect(""),                    "stringa vuota → non riconosciuta");
    CHECK(!gm_detect("   "),                 "solo spazi → non riconosciuta");
    CHECK(!gm_detect("abc"),                 "solo lettere → non riconosciuta");
    /* Grandi range primi: >100000 → deve essere rifiutato per sicurezza */
    CHECK(!gm_detect("primi da 1 a 200000"), "range primi >100000 → rifiutato (protezione)");
    /* Somma a<da */
    CHECK(!gm_detect("somma da 100 a 1"),    "somma inversa (100 a 1) → non riconosciuta");

    SECTION("Edge case — sommatoria Gauss check");
    {
        /* somma da 1 a 100 = 5050 (formula di Gauss) */
        long long da=1,a=100;
        long long s=(da+a)*(a-da+1)/2;
        CHECK(s==5050, "formula Gauss: somma 1..100 = 5050");
        da=1; a=10; s=(da+a)*(a-da+1)/2;
        CHECK(s==55,   "formula Gauss: somma 1..10 = 55");
    }

    /* ── PARTE 6: stress ── */
    SECTION("Stress — 10000 valutazioni consecutive");
    {
        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        double acc = 0;
        for(int i=0;i<10000;i++){
            acc += gp_eval("2+3*4");
            acc += gp_eval("(100-50)*2");
            acc += gp_eval("2^10");
        }
        clock_gettime(CLOCK_MONOTONIC, &t1);
        double ms = (t1.tv_sec-t0.tv_sec)*1000.0+(t1.tv_nsec-t0.tv_nsec)/1e6;
        CHECK(ms < 500.0, "30000 eval in < 500ms");
        CHECK(fabs(acc - (14.0+100.0+1024.0)*10000) < 0.1, "risultati coerenti nello stress");
        printf("  Tempo: %.2f ms per 30000 valutazioni\n", ms);
    }

    /* ── Risultato finale ── */
    printf("\n\033[1m══════════════════════════════════════════════════════\033[0m\n");
    if(_fail==0)
        printf("\033[1m\033[32m  RISULTATI: %d/%d PASSATI — PARSER A PROVA DI BOMBA\033[0m\n",
               _pass, _pass+_fail);
    else
        printf("\033[1m\033[31m  RISULTATI: %d/%d PASSATI — %d FALLITI\033[0m\n",
               _pass, _pass+_fail, _fail);
    printf("\033[1m══════════════════════════════════════════════════════\033[0m\n\n");
    return _fail > 0 ? 1 : 0;
}
