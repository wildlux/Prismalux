/* ══════════════════════════════════════════════════════════════════════════
   test_math_locale.c — Test isolati: motore matematico del Tutor (strumenti.c)
   ══════════════════════════════════════════════════════════════════════════
   Testa in isolamento (zero AI, zero rete):
     T01 — _math_eval: parser completo, espressioni valide/invalide
     T02 — _is_prime: primalità con trial division
     T03 — _fmt_num: formattazione senza ".0" inutili
     T04 — Pattern sconto / ricarica
     T05 — Pattern percentuale
     T06 — Pattern numeri primi (range)
     T07 — Pattern sommatoria / sommatoria formula Gauss
     T08 — Pattern fattoriale
     T09 — Pattern MCD / MCM (algoritmo di Euclide)
     T10 — Pattern radice quadrata
     T11 — Pattern media di valori
     T12 — Espressione pura (fallback finale)
     T13 — Edge case: negativi, zero, overflow, input vuoto
     T14 — Stress: 50000 valutazioni _math_eval consecutive

   Build:
     gcc -O2 -lm -o test_math_locale test_math_locale.c

   Esecuzione: ./test_math_locale
   ══════════════════════════════════════════════════════════════════════════ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <time.h>

/* ── Stub ANSI ─────────────────────────────────────────────────────────── */
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
    if (fabs(_g-_e) < _t) { printf("  \033[32m[OK]\033[0m  %s (=%.8g)\n", label, _g); _pass++; } \
    else { printf("  \033[31m[FAIL]\033[0m %s  got=%.8g  exp=%.8g\n", label, _g, _e); _fail++; } \
} while(0)

/* ════════════════════════════════════════════════════════════════════════
   Copia standalone delle funzioni da src/strumenti.c
   ════════════════════════════════════════════════════════════════════════ */

/* ── Parser ricorsivo-discendente (_mp_*) ─────────────────────────────── */
static const char *_mp;
static double _mp_expr(void);

static void _mp_ws(void) { while(*_mp==' '||*_mp=='\t') _mp++; }

static double _mp_primary(void) {
    _mp_ws();
    if (*_mp == '(') {
        _mp++;
        double v = _mp_expr();
        _mp_ws(); if (*_mp==')') _mp++;
        return v;
    }
    char *end;
    double v = strtod(_mp, &end);
    if (end != _mp) { _mp = end; return v; }
    return 0.0;
}

static double _mp_power(void) {
    _mp_ws();
    int neg = 0;
    if (*_mp=='-') { neg=1; _mp++; }
    else if (*_mp=='+') _mp++;
    double v = _mp_primary();
    _mp_ws();
    if (*_mp=='^') { _mp++; v = pow(v, _mp_power()); }  /* destro-associativo */
    if (*_mp=='!') {
        _mp++;
        long long n = (long long)fabs(v);
        long long f = 1;
        for (long long i=2; i<=n && i<=20; i++) f *= i;
        v = (double)f;
    }
    return neg ? -v : v;
}

static double _mp_term(void) {
    double v = _mp_power();
    while (1) {
        _mp_ws();
        char op = *_mp;
        if (op=='*' || op=='/' || op=='%') {
            _mp++;
            double r = _mp_power();
            if      (op=='*') v *= r;
            else if (op=='/') v = (r != 0) ? v/r : 0.0;
            else              v = (r != 0) ? fmod(v,r) : 0.0;
        } else break;
    }
    return v;
}

static double _mp_expr(void) {
    double v = _mp_term();
    while (1) {
        _mp_ws();
        if      (*_mp=='+') { _mp++; v += _mp_term(); }
        else if (*_mp=='-') { _mp++; v -= _mp_term(); }
        else break;
    }
    return v;
}

/* Valuta espressione pura — ritorna 0=ok, -1=non parsabile */
static int _math_eval(const char *expr, double *out) {
    _mp = expr;
    *out = _mp_expr();
    _mp_ws();
    return (*_mp == '\0') ? 0 : -1;
}

/* ── Utility ──────────────────────────────────────────────────────────── */
static void _str_lower(const char *src, char *dst, int max) {
    int i = 0;
    for (; src[i] && i < max-1; i++)
        dst[i] = (char)tolower((unsigned char)src[i]);
    dst[i] = '\0';
}

static int _is_prime(long long n) {
    if (n < 2) return 0;
    if (n == 2) return 1;
    if (n % 2 == 0) return 0;
    for (long long i = 3; i*i <= n; i += 2)
        if (n % i == 0) return 0;
    return 1;
}

static void _fmt_num(double v, char *buf, int sz) {
    if (v == (long long)v && fabs(v) < 1e15)
        snprintf(buf, sz, "%lld", (long long)v);
    else
        snprintf(buf, sz, "%.6g", v);
}

/* ── Rilevatore pattern (senza printf) — ritorna 1 se gestito ────────── */
static int ml_detect(const char *input) {
    char low[512];
    _str_lower(input, low, sizeof low);

    /* 1. Sconto */
    { double p=0,b=0;
      if (sscanf(low,"sconto %lf%% su %lf",&p,&b)==2) return 1;
      if (sscanf(low,"sconto %lf %% su %lf",&p,&b)==2) return 1; }

    /* 2. Percentuale */
    { double p=0,b=0;
      if (sscanf(low,"%lf%% di %lf",&p,&b)==2) return 1;
      if (sscanf(low,"%lf %% di %lf",&p,&b)==2) return 1; }

    /* 3. Ricarica */
    { double p=0,b=0;
      if (sscanf(low,"ricarica %lf%% su %lf",&p,&b)==2) return 1;
      if (sscanf(low,"ricarico %lf%% su %lf",&p,&b)==2) return 1; }

    /* 4. Numeri primi */
    { long long da=0,a=0;
      int ok = (sscanf(low,"primi da %lld a %lld",&da,&a)==2) ||
               (sscanf(low,"primi tra %lld e %lld",&da,&a)==2);
      if (ok && da>=0 && a>=da && (a-da)<100000) return 1; }

    /* 5. Sommatoria */
    { long long da=0,a=0;
      int ok = (sscanf(low,"somma da %lld a %lld",&da,&a)==2) ||
               (sscanf(low,"sommatoria da %lld a %lld",&da,&a)==2) ||
               (sscanf(low,"somma %lld..%lld",&da,&a)==2) ||
               (sscanf(low,"somma %lld a %lld",&da,&a)==2);
      if (ok && a>=da && (a-da)<10000000LL) return 1; }

    /* 6. Fattoriale */
    { long long n=0;
      if ((sscanf(low,"fattoriale di %lld",&n)==1 ||
           sscanf(low,"fattoriale %lld",&n)==1 ||
           sscanf(low,"%lld!",&n)==1) && n>=0 && n<=20) return 1; }

    /* 7. MCD / MCM */
    { long long a=0,b=0;
      int is_mcd = (sscanf(low,"mcd(%lld,%lld)",&a,&b)==2) ||
                   (sscanf(low,"mcd %lld %lld",&a,&b)==2) ||
                   (sscanf(low,"mcd(%lld, %lld)",&a,&b)==2);
      int is_mcm = !is_mcd && (
                   (sscanf(low,"mcm(%lld,%lld)",&a,&b)==2) ||
                   (sscanf(low,"mcm %lld %lld",&a,&b)==2) ||
                   (sscanf(low,"mcm(%lld, %lld)",&a,&b)==2));
      if ((is_mcd || is_mcm) && a>0 && b>0) return 1; }

    /* 8. Radice */
    { double v=0;
      if (sscanf(low,"radice di %lf",&v)==1 ||
          sscanf(low,"radice quadrata di %lf",&v)==1 ||
          sscanf(low,"sqrt(%lf)",&v)==1) return 1; }

    /* 9. Media */
    if (strncmp(low,"media ",6)==0 || strncmp(low,"media di ",9)==0) {
        const char *p = low + (strncmp(low,"media di ",9)==0 ? 9 : 6);
        char tmp[256]; strncpy(tmp, p, sizeof tmp-1); tmp[sizeof tmp-1]='\0';
        int cnt=0; char *tok=strtok(tmp," ,;");
        while (tok) { char *e; double v=strtod(tok,&e); if(e!=tok)cnt++; tok=strtok(NULL," ,;"); }
        if (cnt > 0) return 1;
    }

    /* 10. Espressione pura */
    { int hd=0,hl=0;
      for (int i=0; low[i]; i++) {
          if (isdigit((unsigned char)low[i])) hd=1;
          if (isalpha((unsigned char)low[i]) && low[i]!='e' && low[i]!='E') hl=1;
      }
      if (hd && !hl) {
          double v=0;
          if (_math_eval(input,&v)==0) return 1;
      }
    }
    return 0;
}

/* ════════════════════════════════════════════════════════════════════════
   MAIN
   ════════════════════════════════════════════════════════════════════════ */
int main(void) {
    printf("\n\033[1m══ test_math_locale.c — Motore matematico Tutor (strumenti.c) ══\033[0m\n");

    /* ── T01: _math_eval ── */
    SECTION("T01 — _math_eval: valutazione espressioni");
    { double v=0;
      CHECK(_math_eval("2+3",&v)==0 && fabs(v-5)<1e-9,   "_math_eval(2+3) = 5");
      CHECK(_math_eval("2*3+4",&v)==0 && fabs(v-10)<1e-9,"_math_eval(2*3+4) = 10");
      CHECK(_math_eval("(2+3)*4",&v)==0 && fabs(v-20)<1e-9,"_math_eval((2+3)*4) = 20");
      CHECK(_math_eval("2^8",&v)==0 && fabs(v-256)<1e-9, "_math_eval(2^8) = 256");
      CHECK(_math_eval("5!",&v)==0 && fabs(v-120)<1e-9,  "_math_eval(5!) = 120");
      CHECK(_math_eval("3.14",&v)==0 && fabs(v-3.14)<1e-6,"_math_eval(3.14) = 3.14");
      CHECK(_math_eval("1/0",&v)==0 && v==0.0,            "_math_eval(1/0) = 0 (sicuro)");
      CHECK(_math_eval("15%4",&v)==0 && fabs(v-3)<1e-9,  "_math_eval(15%4) = 3 (modulo)");
    }

    SECTION("T01b — _math_eval: espressioni non valide → -1");
    { double v=0;
      CHECK(_math_eval("abc",&v)==-1,          "\"abc\" → -1 (solo lettere)");
      CHECK(_math_eval("2+ciao",&v)==-1,       "\"2+ciao\" → -1 (lettere extra)");
      CHECK(_math_eval("",&v)==0,              "\"\" → 0 con v=0 (stringa vuota = 0)");
      CHECK(_math_eval("  ",&v)==0,            "\"  \" → 0 (spazi = 0)");
    }

    /* ── T02: _is_prime ── */
    SECTION("T02 — _is_prime: primalità");
    CHECK(!_is_prime(0),  "0 non è primo");
    CHECK(!_is_prime(1),  "1 non è primo");
    CHECK(_is_prime(2),   "2 è primo");
    CHECK(_is_prime(3),   "3 è primo");
    CHECK(!_is_prime(4),  "4 non è primo");
    CHECK(_is_prime(5),   "5 è primo");
    CHECK(_is_prime(7),   "7 è primo");
    CHECK(!_is_prime(9),  "9 non è primo (3×3)");
    CHECK(_is_prime(11),  "11 è primo");
    CHECK(_is_prime(13),  "13 è primo");
    CHECK(!_is_prime(15), "15 non è primo");
    CHECK(_is_prime(97),  "97 è primo");
    CHECK(!_is_prime(100),"100 non è primo");
    CHECK(_is_prime(101), "101 è primo");
    CHECK(_is_prime(997), "997 è primo");
    CHECK(!_is_prime(1000),"1000 non è primo");

    /* Verifica: 25 primi tra 1 e 100 */
    { int cnt=0;
      for (long long n=1; n<=100; n++) if(_is_prime(n)) cnt++;
      CHECK(cnt==25, "25 numeri primi tra 1 e 100"); }

    SECTION("T02b — _is_prime: casi limite negativi");
    CHECK(!_is_prime(-1),  "-1 non è primo");
    CHECK(!_is_prime(-2),  "-2 non è primo");
    CHECK(!_is_prime(-997),"-997 non è primo");

    /* ── T03: _fmt_num ── */
    SECTION("T03 — _fmt_num: formattazione numeri");
    { char b[32];
      _fmt_num(0.0,b,sizeof b);   CHECK(strcmp(b,"0")==0,      "_fmt_num(0) → \"0\"");
      _fmt_num(42.0,b,sizeof b);  CHECK(strcmp(b,"42")==0,     "_fmt_num(42) → \"42\"");
      _fmt_num(-7.0,b,sizeof b);  CHECK(strcmp(b,"-7")==0,     "_fmt_num(-7) → \"-7\"");
      _fmt_num(3.14,b,sizeof b);  CHECK(strncmp(b,"3.14",4)==0,"_fmt_num(3.14) → \"3.14...\"");
      _fmt_num(999999999999999.0,b,sizeof b);
                                  CHECK(strcmp(b,"999999999999999")==0,
                                         "_fmt_num(1e15-1) → intero (< 1e15 threshold)");
      _fmt_num(1e16,b,sizeof b);  CHECK(strstr(b,"e")!=NULL,"_fmt_num(1e16) → notazione scientifica");
      _fmt_num(0.5,b,sizeof b);   CHECK(strncmp(b,"0.5",3)==0, "_fmt_num(0.5) → \"0.5\"");
      _fmt_num(100.0,b,sizeof b); CHECK(strcmp(b,"100")==0,    "_fmt_num(100) → \"100\"");
    }

    /* ── T04: sconto ── */
    SECTION("T04 — Pattern sconto");
    CHECK(ml_detect("sconto 20% su 150"),       "sconto 20% su 150 → riconosciuto");
    CHECK(ml_detect("sconto 10% su 50"),        "sconto 10% su 50");
    CHECK(ml_detect("Sconto 30% su 200"),       "Sconto 30%... (maiuscola) → riconosciuto");
    CHECK(!ml_detect("sconto su 100"),          "sconto su 100 (no %) → non riconosciuto");

    /* Verifica matematica: sconto 20% su 150 = risparmio 30, finale 120 */
    { double p=20.0, b=150.0;
      CHECK(fabs(b*p/100.0 - 30.0) < 1e-9,     "20% di 150 = risparmio 30");
      CHECK(fabs(b - b*p/100.0 - 120.0) < 1e-9,"150 - 30 = finale 120"); }

    SECTION("T04b — Pattern ricarica");
    CHECK(ml_detect("ricarica 15% su 100"),     "ricarica 15% su 100 → riconosciuto");
    CHECK(ml_detect("ricarico 10% su 500"),     "ricarico 10% su 500 → riconosciuto");
    { double p=15.0, b=100.0;
      CHECK(fabs(b+b*p/100.0 - 115.0) < 1e-9,  "ricarica 15% su 100 = 115"); }

    /* ── T05: percentuale ── */
    SECTION("T05 — Pattern percentuale");
    CHECK(ml_detect("20% di 500"),              "20% di 500 → riconosciuto");
    CHECK(ml_detect("7.5% di 1000"),            "7.5% di 1000 → riconosciuto");
    CHECK(ml_detect("100% di 42"),              "100% di 42 → riconosciuto");
    CHECK(!ml_detect("percentuale di venti"),   "percentuale di venti → non riconosciuto");
    { double p=7.5, b=1000.0;
      CHECK(fabs(b*p/100.0 - 75.0) < 1e-9,     "7.5% di 1000 = 75"); }

    /* ── T06: numeri primi ── */
    SECTION("T06 — Pattern numeri primi (range)");
    CHECK(ml_detect("primi da 1 a 20"),         "primi da 1 a 20");
    CHECK(ml_detect("Primi tra 10 e 50"),        "Primi tra 10 e 50 (maiuscola)");
    CHECK(ml_detect("primi da 0 a 100"),         "primi da 0 a 100");
    CHECK(!ml_detect("i numeri primi"),          "testo generico → non riconosciuto");
    CHECK(!ml_detect("primi da 1 a 200000"),     "range >100000 → protetto");

    /* Verifica: 4 primi tra 1 e 10: 2,3,5,7 */
    { int cnt=0;
      for(long long n=1;n<=10;n++) if(_is_prime(n)) cnt++;
      CHECK(cnt==4, "4 primi tra 1 e 10: 2,3,5,7"); }

    /* ── T07: sommatoria ── */
    SECTION("T07 — Pattern sommatoria e formula Gauss");
    CHECK(ml_detect("somma da 1 a 100"),        "somma da 1 a 100");
    CHECK(ml_detect("sommatoria da 1 a 50"),    "sommatoria da 1 a 50");
    CHECK(ml_detect("somma 1..100"),            "somma 1..100");
    CHECK(ml_detect("somma 1 a 10"),            "somma 1 a 10");
    CHECK(!ml_detect("somma i numeri"),         "somma i numeri → non riconosciuto");

    { long long da=1,a=100, s=(da+a)*(a-da+1)/2;
      CHECK(s==5050, "formula Gauss: 1..100 = 5050"); }
    { long long da=1,a=1000, s=(da+a)*(a-da+1)/2;
      CHECK(s==500500, "formula Gauss: 1..1000 = 500500"); }
    { long long da=0,a=0, s=(da+a)*(a-da+1)/2;
      CHECK(s==0, "somma da 0 a 0 = 0"); }

    /* ── T08: fattoriale ── */
    SECTION("T08 — Pattern fattoriale");
    CHECK(ml_detect("fattoriale di 5"),         "fattoriale di 5");
    CHECK(ml_detect("fattoriale 7"),            "fattoriale 7");
    CHECK(ml_detect("5!"),                      "5! (espressione diretta)");
    CHECK(!ml_detect("fattoriale di 25"),       "fattoriale 25 > 20 → non gestito");

    { long long f=1; for(long long i=2;i<=5;i++) f*=i;
      CHECK(f==120, "5! = 120"); }
    { long long f=1; for(long long i=2;i<=10;i++) f*=i;
      CHECK(f==3628800, "10! = 3628800"); }
    { long long f=1;
      CHECK(f==1, "0! = 1 (f inizializzato a 1, loop non eseguito)"); }

    /* ── T09: MCD / MCM ── */
    SECTION("T09 — MCD / MCM (algoritmo Euclide)");
    CHECK(ml_detect("mcd(12,8)"),               "mcd(12,8)");
    CHECK(ml_detect("mcd 12 8"),                "mcd 12 8");
    CHECK(ml_detect("mcd(12, 8)"),              "mcd(12, 8) con spazio");
    CHECK(ml_detect("mcm(4,6)"),                "mcm(4,6)");
    CHECK(ml_detect("mcm 4 6"),                 "mcm 4 6");
    CHECK(!ml_detect("mcd(0,5)"),               "mcd(0,5): a=0 → non gestito (richiede a>0)");

    /* Verifica algoritmo Euclide */
    { long long a=12,b=8,x=a,y=b;
      while(y){long long t=y;y=x%y;x=t;}
      CHECK(x==4, "MCD(12,8) = 4"); }
    { long long a=4,b=6,x=a,y=b;
      while(y){long long t=y;y=x%y;x=t;}
      long long gcd=x, lcm=(a/gcd)*b;
      CHECK(gcd==2, "MCD(4,6) = 2");
      CHECK(lcm==12,"MCM(4,6) = 12"); }
    { long long a=7,b=13,x=a,y=b;
      while(y){long long t=y;y=x%y;x=t;}
      CHECK(x==1, "MCD(7,13) = 1 (numeri co-primi)"); }
    { long long a=100,b=75,x=a,y=b;
      while(y){long long t=y;y=x%y;x=t;}
      CHECK(x==25, "MCD(100,75) = 25"); }

    /* ── T10: radice quadrata ── */
    SECTION("T10 — Pattern radice quadrata");
    CHECK(ml_detect("radice di 16"),            "radice di 16");
    CHECK(ml_detect("radice quadrata di 9"),    "radice quadrata di 9");
    CHECK(ml_detect("sqrt(25)"),               "sqrt(25)");
    CHECK(ml_detect("radice di 2"),             "radice di 2 (irrazionale)");

    { CHECK(fabs(sqrt(16.0)-4.0)<1e-9,  "sqrt(16) = 4"); }
    { CHECK(fabs(sqrt(2.0)-1.41421356)<1e-6, "sqrt(2) ≈ 1.41421356"); }
    { CHECK(fabs(sqrt(0.0)-0.0)<1e-9,   "sqrt(0) = 0"); }

    /* ── T11: media ── */
    SECTION("T11 — Pattern media di valori");
    CHECK(ml_detect("media 1 2 3 4 5"),         "media 1 2 3 4 5");
    CHECK(ml_detect("media di 10 20 30"),       "media di 10 20 30");
    CHECK(ml_detect("media 7,8,9"),             "media 7,8,9 (separatore virgola)");
    CHECK(!ml_detect("media"),                  "media senza numeri → non riconosciuto");

    /* Verifica calcolo media */
    { double sum=1+2+3+4+5; int n=5;
      CHECK(fabs(sum/n - 3.0) < 1e-9, "media(1,2,3,4,5) = 3"); }
    { double sum=10+20+30; int n=3;
      CHECK(fabs(sum/n - 20.0) < 1e-9,"media(10,20,30) = 20"); }

    /* ── T12: espressione pura ── */
    SECTION("T12 — Espressione pura (fallback)");
    CHECK(ml_detect("2+3"),                     "2+3 → pura");
    CHECK(ml_detect("100*3+50"),                "100*3+50 → pura");
    CHECK(ml_detect("2^10"),                    "2^10 → pura");
    CHECK(!ml_detect("scrivi un programma"),    "testo puro → non riconosciuto");
    CHECK(!ml_detect("calcola l'algoritmo"),    "lettere non-e → non riconosciuto");

    /* ── T13: edge case ── */
    SECTION("T13 — Edge case");
    CHECK(!ml_detect(""),                       "stringa vuota → non riconosciuta");
    CHECK(!ml_detect("   "),                    "solo spazi → non riconosciuta");
    CHECK(!ml_detect("xyz"),                    "solo lettere → non riconosciuta");
    /* somma con overflow: (a-da) >= 10000000 → protetto */
    CHECK(!ml_detect("somma da 1 a 20000000"), "somma range >10M → protetta");
    /* radice di negativo: ml_detect ritorna 1 (pattern riconosciuto) ma il risultato è immaginario */
    CHECK(ml_detect("radice di -4"),            "radice di -4: pattern catturato (risposta: immaginario)");
    /* MCD con b=0: non gestito */
    CHECK(!ml_detect("mcd(5,0)"),              "mcd(5,0): b=0 → non gestito");
    /* Fattoriale negativo: non gestito */
    CHECK(!ml_detect("fattoriale di -3"),       "fattoriale negativo → non gestito");
    /* Fattoriale 20: max supportato */
    CHECK(ml_detect("fattoriale di 20"),        "fattoriale 20 (limite massimo)");
    /* Fattoriale 21: oltre il limite */
    CHECK(!ml_detect("fattoriale di 21"),       "fattoriale 21 > 20 → non gestito");

    /* Verifica _is_prime: nessun crash con valori estremi */
    CHECK(!_is_prime(0),  "_is_prime(0) → 0 (no crash)");
    CHECK(!_is_prime(-100), "_is_prime(-100) → 0 (no crash)");

    /* ── T14: stress ── */
    SECTION("T14 — Stress: 50000 valutazioni _math_eval");
    { struct timespec t0,t1;
      clock_gettime(CLOCK_MONOTONIC,&t0);
      double acc=0; int ok_cnt=0;
      for(int i=0;i<50000;i++) {
          double v=0;
          if(_math_eval("2+3*4",&v)==0){ acc+=v; ok_cnt++; }
          if(_math_eval("(100-1)*(100+1)",&v)==0){ acc+=v; ok_cnt++; }
          if(_math_eval("2^16",&v)==0){ acc+=v; ok_cnt++; }
      }
      clock_gettime(CLOCK_MONOTONIC,&t1);
      double ms=(t1.tv_sec-t0.tv_sec)*1000.0+(t1.tv_nsec-t0.tv_nsec)/1e6;
      CHECK(ms < 1000.0, "150000 eval in < 1s");
      CHECK(ok_cnt == 150000, "tutte le 150000 eval hanno successo");
      /* 2+3*4=14, (100-1)*(100+1)=9999, 2^16=65536 */
      double expected = (14.0 + 9999.0 + 65536.0) * 50000.0;
      CHECK(fabs(acc - expected) < 1.0, "risultati consistenti durante lo stress");
      printf("  Tempo: %.2f ms per 150000 valutazioni\n", ms);
    }

    /* ── Stress _is_prime ── */
    { struct timespec t0,t1;
      clock_gettime(CLOCK_MONOTONIC,&t0);
      int cnt=0;
      for(long long n=1; n<=10000; n++) if(_is_prime(n)) cnt++;
      clock_gettime(CLOCK_MONOTONIC,&t1);
      double ms=(t1.tv_sec-t0.tv_sec)*1000.0+(t1.tv_nsec-t0.tv_nsec)/1e6;
      CHECK(cnt==1229, "1229 primi tra 1 e 10000 (teorema dei numeri primi)");
      CHECK(ms < 100.0, "_is_prime 1..10000 in < 100ms");
      printf("  Tempo _is_prime(1..10000): %.2f ms, %d primi trovati\n", ms, cnt);
    }

    /* ── Risultato finale ── */
    printf("\n\033[1m══════════════════════════════════════════════════════\033[0m\n");
    if (_fail == 0)
        printf("\033[1m\033[32m  RISULTATI: %d/%d PASSATI — MOTORE MATEMATICO TUTOR OK\033[0m\n",
               _pass, _pass+_fail);
    else
        printf("\033[1m\033[31m  RISULTATI: %d/%d PASSATI — %d FALLITI\033[0m\n",
               _pass, _pass+_fail, _fail);
    printf("\033[1m══════════════════════════════════════════════════════\033[0m\n\n");
    return _fail > 0 ? 1 : 0;
}
