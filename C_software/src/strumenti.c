/*
 * strumenti.c — SEZIONE 11: Tutor AI, Agenti Specializzati e Strumento Pratico
 * ==============================================================================
 * menu_tutor(), menu_agenti_ruoli(), pratico_730(), pratico_piva(), menu_pratico()
 */
#include "common.h"
#include <time.h>
#include "backend.h"
#include "terminal.h"
#include "ai.h"
#include "modelli.h"
#include "strumenti.h"
#include "prismalux_ui.h"
#include "rag.h"
#include <math.h>

/* ══════════════════════════════════════════════════════════════
   MOTORE MATEMATICO LOCALE — zero latenza, zero token AI
   Gestisce: aritmetica, %, sconto, primi, sommatoria,
             fattoriale, potenza, MCD, MCM, radice, media
   ══════════════════════════════════════════════════════════════ */

/* ── Parser aritmetico ricorsivo-discendente ─────────────────────────── */
static const char *_mp;   /* cursore nell'input */

static double _mp_expr(void);   /* forward */

static void _mp_ws(void) { while(*_mp==' '||*_mp=='\t') _mp++; }

static double _mp_primary(void) {
    _mp_ws();
    if (*_mp == '(') {
        _mp++;
        double v = _mp_expr();
        _mp_ws(); if (*_mp==')') _mp++;
        return v;
    }
    /* sqrt / radq / radice [quadrata] [di] — funzioni radice nel parser */
    if((strncmp(_mp,"sqrt",4)==0||strncmp(_mp,"radq",4)==0)&&
       !isalnum((unsigned char)_mp[4])&&_mp[4]!='_'){
        _mp+=4; _mp_ws();
        double inner;
        if(*_mp=='('){_mp++;inner=_mp_expr();_mp_ws();if(*_mp==')')_mp++;}
        else inner=_mp_primary();
        return inner>=0.0 ? sqrt(inner) : 0.0;
    }
    if(strncmp(_mp,"radice",6)==0&&!isalnum((unsigned char)_mp[6])&&_mp[6]!='_'){
        _mp+=6; _mp_ws();
        if(strncmp(_mp,"quadrata",8)==0&&!isalnum((unsigned char)_mp[8]))
            { _mp+=8; _mp_ws(); }
        if(strncmp(_mp,"di",2)==0&&!isalnum((unsigned char)_mp[2]))
            { _mp+=2; _mp_ws(); }
        double inner;
        if(*_mp=='('){_mp++;inner=_mp_expr();_mp_ws();if(*_mp==')')_mp++;}
        else inner=_mp_primary();
        return inner>=0.0 ? sqrt(inner) : 0.0;
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
    if (*_mp=='!') {   /* fattoriale inline: 5! */
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

/* Valuta un'espressione matematica pura — ritorna 0=ok, -1=non parsabile */
static int _math_eval(const char *expr, double *out) {
    _mp = expr;
    *out = _mp_expr();
    _mp_ws();
    return (*_mp == '\0') ? 0 : -1;
}

/* ── Utility: converti stringa in lowercase ──────────────────────────── */
static void _str_lower(const char *src, char *dst, int max) {
    int i = 0;
    for (; src[i] && i < max-1; i++)
        dst[i] = (char)tolower((unsigned char)src[i]);
    dst[i] = '\0';
}

/* ── Primalità (trial division) ──────────────────────────────────────── */
static int _is_prime(long long n) {
    if (n < 2) return 0;
    if (n == 2) return 1;
    if (n % 2 == 0) return 0;
    for (long long i = 3; i*i <= n; i += 2)
        if (n % i == 0) return 0;
    return 1;
}

/* ── Formatta numero: evita ".000000" per interi ─────────────────────── */
static void _fmt_num(double v, char *buf, int sz) {
    if (v == (long long)v && fabs(v) < 1e15)
        snprintf(buf, sz, "%lld", (long long)v);
    else
        snprintf(buf, sz, "%.6g", v);
}

/* ── Motore matematico principale ────────────────────────────────────── */
/*
 * Ritorna 1 se la domanda e stata gestita localmente (stampa il risultato),
 *         0 se va passata all'AI.
 */
static int _math_locale(const char *input) {
    struct timespec _ml_t0, _ml_t1;
    clock_gettime(CLOCK_MONOTONIC, &_ml_t0);

    char low[512];
    _str_lower(input, low, sizeof low);

    /* ── Normalizza: forme alternative → operatori canonici ─────────────
       2**3            → 2^3  (stile Python)
       "X elevato a Y" → X^Y
       "X elevato Y"   → X^Y
       "X alla Y"      → X^Y
       ──────────────────────────────────────────────────────────────── */
    for(int _j=0; low[_j]; _j++)
        if(low[_j]=='*'&&low[_j+1]=='*')
            { low[_j]='^'; memmove(low+_j+1,low+_j+2,strlen(low+_j+2)+1); }
    {
        static const struct { const char *s; int len; } kPow[]={
            {" elevato a ",11},{" elevato ",9},{" alla ",6},{NULL,0}
        };
        for(int _k=0;kPow[_k].s;_k++){
            char *_p;
            while((_p=strstr(low,kPow[_k].s))!=NULL){
                *_p='^';
                memmove(_p+1,_p+kPow[_k].len,strlen(_p+kPow[_k].len)+1);
            }
        }
    }

    char res[256] = "";
    char extra[1024] = "";

    /* ── 1. Sconto: "sconto X% su Y" o "X% di sconto su Y" ─────────── */
    {
        double pct=0, base=0;
        if (sscanf(low, "sconto %lf%% su %lf", &pct, &base) == 2 ||
            sscanf(low, "sconto %lf %% su %lf", &pct, &base) == 2) {
            double risparmio = base * pct / 100.0;
            double finale    = base - risparmio;
            char b[32], r[32], f[32];
            _fmt_num(base,     b, sizeof b);
            _fmt_num(risparmio,r, sizeof r);
            _fmt_num(finale,   f, sizeof f);
            snprintf(res,   sizeof res,   "%s \xe2\x86\x92 risparmio: %s  prezzo finale: %s", b, r, f);
            snprintf(extra, sizeof extra, "  Prezzo originale : %s\n  Sconto (%.4g%%)  : -%s\n  Prezzo finale    : %s", b, pct, r, f);
            goto stampa;
        }
    }

    /* ── 2. Percentuale: "X% di Y" ───────────────────────────────────── */
    {
        double pct=0, base=0;
        if (sscanf(low, "%lf%% di %lf", &pct, &base) == 2 ||
            sscanf(low, "%lf %% di %lf", &pct, &base) == 2) {
            double v = base * pct / 100.0;
            char bv[32], rv[32];
            _fmt_num(base, bv, sizeof bv);
            _fmt_num(v,    rv, sizeof rv);
            snprintf(res,   sizeof res,   "%.4g%% di %s = %s", pct, bv, rv);
            snprintf(extra, sizeof extra, "  %s * %.4g / 100 = %s", bv, pct, rv);
            goto stampa;
        }
    }

    /* ── 3. Margine/ricarico: "ricarica X% su Y" ─────────────────────── */
    {
        double pct=0, base=0;
        if (sscanf(low, "ricarica %lf%% su %lf", &pct, &base) == 2 ||
            sscanf(low, "ricarico %lf%% su %lf", &pct, &base) == 2) {
            double aggiunta = base * pct / 100.0;
            double finale   = base + aggiunta;
            char bv[32], av[32], fv[32];
            _fmt_num(base,    bv, sizeof bv);
            _fmt_num(aggiunta,av, sizeof av);
            _fmt_num(finale,  fv, sizeof fv);
            snprintf(res,   sizeof res,   "%s + %.4g%% = %s", bv, pct, fv);
            snprintf(extra, sizeof extra, "  Base: %s  +  %.4g%%(%s)  =  %s", bv, pct, av, fv);
            goto stampa;
        }
    }

    /* ── 4. Numeri primi in un intervallo ─────────────────────────────── */
    {
        long long da=0, a=0;
        int ok = (sscanf(low, "primi da %lld a %lld",   &da, &a)==2) ||
                 (sscanf(low, "primi tra %lld e %lld",  &da, &a)==2) ||
                 (sscanf(low, "primi tra %lld a %lld",  &da, &a)==2) ||
                 (strstr(low,"primi") && sscanf(low, "%*[^0-9]%lld%*[^0-9]%lld", &da, &a)==2);
        if (ok && da>=0 && a>=da && (a-da)<100000) {
            char lista[4096] = "";
            int cnt = 0;
            long long somma = 0;
            for (long long n=da; n<=a; n++) {
                if (_is_prime(n)) {
                    char nb[24]; snprintf(nb, sizeof nb, "%lld ", n);
                    if (strlen(lista)+strlen(nb) < sizeof lista-4)
                        strcat(lista, nb);
                    else if (cnt < 99999) { strcat(lista, "..."); }
                    cnt++; somma += n;
                }
            }
            char sv[32]; _fmt_num((double)somma, sv, sizeof sv);
            snprintf(res,   sizeof res,   "%d numeri primi tra %lld e %lld", cnt, da, a);
            snprintf(extra, sizeof extra, "  %s\n  Totale: %d   Somma: %s", lista[0]?lista:"(nessuno)", cnt, sv);
            goto stampa;
        }
    }

    /* ── 5. Sommatoria: "somma da X a Y" o "somma 1..100" ───────────── */
    {
        long long da=0, a=0;
        int ok = (sscanf(low, "somma da %lld a %lld",  &da, &a)==2) ||
                 (sscanf(low, "sommatoria da %lld a %lld", &da, &a)==2) ||
                 (sscanf(low, "somma %lld..%lld",       &da, &a)==2) ||
                 (sscanf(low, "somma %lld a %lld",      &da, &a)==2);
        if (ok && a >= da && (a-da) < 10000000LL) {
            long long s = (da + a) * (a - da + 1) / 2;
            char sv[32]; _fmt_num((double)s, sv, sizeof sv);
            snprintf(res,   sizeof res,   "Σ(%lld..%lld) = %s", da, a, sv);
            snprintf(extra, sizeof extra, "  Formula: (da+a) * n / 2 = (%lld+%lld) * %lld / 2 = %s",
                     da, a, a-da+1, sv);
            goto stampa;
        }
    }

    /* ── 6. Fattoriale: "fattoriale di N" o "N!" ─────────────────────── */
    {
        long long n = 0;
        if (sscanf(low, "fattoriale di %lld", &n)==1 ||
            sscanf(low, "fattoriale %lld",    &n)==1 ||
            (sscanf(low, "%lld!", &n)==1 && n>=0)) {
            if (n >= 0 && n <= 20) {
                long long f = 1;
                char passi[256] = "1";
                for (long long i=2; i<=n; i++) { f *= i; snprintf(passi+strlen(passi), 20, "*%lld", i); }
                char fv[32]; _fmt_num((double)f, fv, sizeof fv);
                snprintf(res,   sizeof res,   "%lld! = %s", n, fv);
                snprintf(extra, sizeof extra, "  %s = %s", passi, fv);
            } else {
                snprintf(res, sizeof res, "Fattoriale disponibile solo per n \xe2\x89\xa4 20");
            }
            goto stampa;
        }
    }

    /* ── 7. MCD / MCM ────────────────────────────────────────────────── */
    {
        long long a=0, b=0;
        int is_mcd = (sscanf(low, "mcd(%lld,%lld)", &a, &b)==2) ||
                     (sscanf(low, "mcd %lld %lld",  &a, &b)==2) ||
                     (sscanf(low, "mcd(%lld, %lld)",&a, &b)==2);
        int is_mcm = !is_mcd && (
                     (sscanf(low, "mcm(%lld,%lld)", &a, &b)==2) ||
                     (sscanf(low, "mcm %lld %lld",  &a, &b)==2) ||
                     (sscanf(low, "mcm(%lld, %lld)",&a, &b)==2));
        if ((is_mcd || is_mcm) && a>0 && b>0) {
            long long x=a, y=b;
            while(y) { long long t=y; y=x%y; x=t; }  /* GCD */
            long long gcd=x, lcm=(a/gcd)*b;
            char gv[32], lv[32];
            _fmt_num((double)gcd, gv, sizeof gv);
            _fmt_num((double)lcm, lv, sizeof lv);
            if (is_mcd)
                snprintf(res, sizeof res, "MCD(%lld, %lld) = %s", a, b, gv);
            else
                snprintf(res, sizeof res, "MCM(%lld, %lld) = %s", a, b, lv);
            snprintf(extra, sizeof extra, "  MCD = %s    MCM = %s", gv, lv);
            goto stampa;
        }
    }

    /* ── 8. Radice quadrata: "radice di X" o "sqrt(X)" ──────────────── */
    {
        double v = 0;
        if (sscanf(low, "radice di %lf",  &v)==1 ||
            sscanf(low, "radice quadrata di %lf", &v)==1 ||
            sscanf(low, "sqrt(%lf)",      &v)==1 ||
            sscanf(low, "sqrt %lf",       &v)==1 ||
            sscanf(low, "radq(%lf)",      &v)==1 ||
            sscanf(low, "radq %lf",       &v)==1) {
            if (v >= 0) {
                char rv[32]; _fmt_num(sqrt(v), rv, sizeof rv);
                snprintf(res,   sizeof res,   "\xe2\x88\x9a%.4g = %s", v, rv);
                snprintf(extra, sizeof extra, "  sqrt(%.4g) = %s", v, rv);
            } else {
                snprintf(res, sizeof res, "Radice di numero negativo = numero immaginario");
            }
            goto stampa;
        }
    }

    /* ── 9. Media: "media di X Y Z ..." ──────────────────────────────── */
    if (strncmp(low, "media ", 6)==0 || strncmp(low, "media di ", 9)==0) {
        const char *p = low + (strncmp(low,"media di ",9)==0 ? 9 : 6);
        double sum = 0; int cnt = 0;
        char tmp[256]; strncpy(tmp, p, sizeof tmp-1); tmp[sizeof tmp-1]='\0';
        char *tok = strtok(tmp, " ,;");
        while (tok) {
            char *end; double v = strtod(tok, &end);
            if (end != tok) { sum += v; cnt++; }
            tok = strtok(NULL, " ,;");
        }
        if (cnt > 0) {
            char mv[32]; _fmt_num(sum/cnt, mv, sizeof mv);
            char sv[32]; _fmt_num(sum,     sv, sizeof sv);
            snprintf(res,   sizeof res,   "Media di %d valori = %s", cnt, mv);
            snprintf(extra, sizeof extra, "  Somma: %s   n: %d   Media: %s", sv, cnt, mv);
            goto stampa;
        }
    }

    /* ── 10. Espressione aritmetica pura (ultima risorsa) ─────────────── */
    {
        /* Accetta solo se contiene cifre e operatori, niente lettere strane */
        int has_digit = 0, has_letter = 0;
        for (int i=0; low[i]; i++) {
            if (isdigit((unsigned char)low[i])) has_digit = 1;
            if (isalpha((unsigned char)low[i]) &&
                low[i]!='e' && low[i]!='E')    /* 'e' e' notazione scientifica */
                has_letter = 1;
        }
        /* sqrt/radq/radice sono lettere ma fanno parte del parser */
        int is_fn_expr = (strncmp(low,"sqrt",4)==0||strncmp(low,"radq",4)==0||strncmp(low,"radice",6)==0);
        if (has_digit && (!has_letter || is_fn_expr)) {
            double v = 0;
            if (_math_eval(low, &v) == 0) {  /* usa low: già normalizzato */
                char rv[32]; _fmt_num(v, rv, sizeof rv);
                snprintf(res, sizeof res, "%s = %s", low, rv);
                goto stampa;
            }
        }
    }

    return 0;  /* non gestito: passa all'AI */

stampa:
    clock_gettime(CLOCK_MONOTONIC, &_ml_t1);
    {
        double _ms = (_ml_t1.tv_sec-_ml_t0.tv_sec)*1000.0
                   + (_ml_t1.tv_nsec-_ml_t0.tv_nsec)/1e6;
        printf(GRN BLD "\n  \xe2\x9c\x94  %s\n" RST, res);
        if (extra[0]) printf(GRY "%s\n" RST, extra);
        printf(GRY "  Tempo: %.3f ms (%.6f s)\n\n" RST, _ms, _ms/1000.0);
    }
    return 1;
}

/* ══════════════════════════════════════════════════════════════
   SEZIONE 11 — Tutor e Strumento Pratico
   ══════════════════════════════════════════════════════════════ */

/* ── System prompt tutor: breve, non spreca token ──────────────────────── */
static const char _SYS_TUTOR[] =
    "Sei il Tutor AI di Prismalux. "
    "Rispondi SEMPRE e SOLO in italiano, in modo chiaro, didattico e incoraggiante. "
    "Usa esempi pratici. Adatta il livello alla domanda.";

/* ── Lista argomenti: stampata LOCALMENTE, zero token AI ───────────────── */
static void _stampa_argomenti(void) {
    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x93\x9a  Prismalux \xe2\x80\x94 Argomenti disponibili\n\n" RST);

    printf(YLW "  SIMULATORE ALGORITMI" RST " (35 simulazioni, 9 categorie)\n");
    printf(GRY "  \xe2\x94\x9c\xe2\x94\x80 Ordinamento (12): " RST
           "Bubble, Selection, Insertion, Merge, Quick, Heap,\n"
           "  \xe2\x94\x82               Shell, Counting, Cocktail, Gnome, Pancake, Comb\n");
    printf(GRY "  \xe2\x94\x9c\xe2\x94\x80 Ricerca (3):     " RST "Binaria, Lineare, Jump Search\n");
    printf(GRY "  \xe2\x94\x9c\xe2\x94\x80 Matematica (6):  " RST
           "GCD Euclide, Crivo, Potenza Modulare, Pascal, \xcf\x80 Monte Carlo, Fibonacci DP\n");
    printf(GRY "  \xe2\x94\x9c\xe2\x94\x80 Prog.Din. (2):   " RST "Zaino 0/1, LCS\n");
    printf(GRY "  \xe2\x94\x9c\xe2\x94\x80 Grafi (2):       " RST "BFS su griglia, Dijkstra\n");
    printf(GRY "  \xe2\x94\x9c\xe2\x94\x80 Tecniche (3):    " RST "Two Pointers, Sliding Window, Bit Manipulation\n");
    printf(GRY "  \xe2\x94\x9c\xe2\x94\x80 Visuale (3):     " RST "Game of Life, Sierpinski, Rule 30 Wolfram\n");
    printf(GRY "  \xe2\x94\x9c\xe2\x94\x80 Fisica (1):      " RST "Caduta Libera\n");
    printf(GRY "  \xe2\x94\x94\xe2\x94\x80 Chimica (3):     " RST "pH, Gas Ideali PV=nRT, Stechiometria\n\n");

    printf(YLW "  QUIZ INTERATTIVI" RST " (9 categorie, domande generate da AI)\n");
    printf(GRY "    " RST "Python Base/Intermedio/Avanzato \xe2\x80\x94 Algoritmi \xe2\x80\x94 Matematica\n");
    printf(GRY "    " RST "Fisica \xe2\x80\x94 Chimica \xe2\x80\x94 Sicurezza Informatica \xe2\x80\x94 Protocolli di Rete\n\n");

    printf(YLW "  AGENTI AI SPECIALIZZATI" RST " (50 ruoli)\n");
    printf(GRY "    " RST "Matematico, Fisico, Chimico, Biologo, Storico, Filosofo, Economista...\n\n");

    printf(YLW "  PIPELINE MULTI-AGENTE" RST " (6 agenti)\n");
    printf(GRY "    " RST "Ricercatore\xe2\x86\x92Pianificatore\xe2\x86\x922 Programmatori(par)\xe2\x86\x92Tester\xe2\x86\x92Ottimizzatore\n\n");

    printf(YLW "  STRUMENTI PRATICI\n" RST);
    printf(GRY "    " RST "Assistente 730 \xe2\x80\x94 Assistente Partita IVA\n\n");

    printf(YLW "  MOTORE ANTI-ALLUCINAZIONE" RST " (Byzantino, 4 agenti)\n");
    printf(GRY "    " RST "A(originale) + B(avvocato) + C(gemello) + D(giudice)\n\n");

    printf(GRN "  Totale: " YLW "35 sim + 9 quiz + 50 ruoli\n\n" RST);
    printf(GRY "  [INVIO per tornare al tutor] " RST); fflush(stdout);
    { char _b[4]; if(!fgets(_b,sizeof _b,stdin)){} }
}

/* ── Rileva se la domanda riguarda le capacita di Prismalux ────────────── */
static int _e_domanda_su_prismalux(const char *s) {
    const char *kw[] = {
        "cosa puoi", "cosa sai", "quanti argomenti", "quanti temi",
        "cosa hai", "hai in memoria", "argomenti disponibili",
        "cosa conosci", "funzionalit", "categorie hai",
        NULL
    };
    char low[256]; int i=0;
    for(; s[i] && i<255; i++) low[i]=(char)tolower((unsigned char)s[i]);
    low[i]='\0';
    for(int k=0; kw[k]; k++)
        if(strstr(low, kw[k])) return 1;
    return 0;
}

void menu_tutor(void) {
    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x8f\x9b\xef\xb8\x8f  Modalit\xc3\xa0 Oracolo \xe2\x80\x94 Tutor AI\n" RST);
    printf(GRY "  Scrivi la tua domanda. " EM_0 " o lascia vuoto per tornare.\n" RST);
    printf(GRY "  Digita " CYN "?" RST GRY " per argomenti.  Calcoli istantanei: " RST
           CYN "3+4*2" RST GRY ", " RST CYN "sconto 20%% su 150" RST GRY ", " RST
           CYN "primi da 1 a 50" RST GRY ", " RST CYN "somma da 1 a 100" RST GRY ", "
           RST CYN "5!" RST GRY ", " RST CYN "mcd(12,8)" RST "\n\n" RST);
    if(!backend_ready(&g_ctx)){ ensure_ready_or_return(&g_ctx); return; }
    if(!check_risorse_ok()) return;

    char domanda[2048];
    while(1){
        printf(YLW "Tu: " RST);
        if(!fgets(domanda, sizeof domanda, stdin)) break;
        domanda[strcspn(domanda,"\n")] = '\0';
        if(!domanda[0] || domanda[0]=='0' || strcmp(domanda,"esci")==0 || strcmp(domanda,"q")==0) break;

        /* Comando diretto: lista argomenti senza toccare l'AI */
        if(strcmp(domanda,"?") == 0 || strcmp(domanda,"help") == 0) {
            _stampa_argomenti();
            clear_screen(); print_header();
            printf(CYN BLD "\n  \xf0\x9f\x8f\x9b\xef\xb8\x8f  Modalit\xc3\xa0 Oracolo \xe2\x80\x94 Tutor AI\n\n" RST);
            printf(GRY "  Digita " CYN "?" RST GRY " per argomenti, " EM_0 " per tornare.\n\n" RST);
            continue;
        }

        /* Domanda su capacita Prismalux: stampa lista + chiedi all'AI se vuole aggiungere */
        if(_e_domanda_su_prismalux(domanda)) {
            _stampa_argomenti();
            clear_screen(); print_header();
            printf(CYN BLD "\n  \xf0\x9f\x8f\x9b\xef\xb8\x8f  Modalit\xc3\xa0 Oracolo \xe2\x80\x94 Tutor AI\n\n" RST);
            printf(GRY "  Digita " CYN "?" RST GRY " per argomenti, " EM_0 " per tornare.\n\n" RST);
            continue;
        }

        /* Matematica locale: istantaneo, zero token AI */
        if (_math_locale(domanda)) continue;

        /* Domanda normale: invia all'AI con system prompt minimo */
        printf(MAG "Oracolo: " RST);
        ai_chat_stream(&g_ctx, _SYS_TUTOR, domanda, stream_cb, NULL, NULL, 0);
        printf("\n");
    }
}

/* ══════════════════════════════════════════════════════════════
   Agenti AI Specializzati — 50 ruoli con system prompt dedicati
   Ogni ruolo ha: icona UTF-8, nome, system prompt, intro statica.
   L'intro viene mostrata all'apertura senza chiamate AI.
   ══════════════════════════════════════════════════════════════ */

typedef struct {
    const char *icona;
    const char *nome;
    const char *prompt;
    const char *intro;   /* testo statico mostrato all'apertura — zero latenza */
} RuoloAI;

static const RuoloAI _RUOLI[] = {

/* ── Ruoli base pipeline ── */

{ "\xf0\x9f\x94\x8d", "Analizzatore",
  "Sei l'Analizzatore nella pipeline AI di Prismalux. "
  "Analizza il task: identifica requisiti, componenti chiave, dipendenze e casi limite. "
  "Produci un'analisi strutturata. Rispondi SEMPRE e SOLO in italiano.",
  "Scompongo problemi complessi in requisiti, componenti, dipendenze e casi limite.\n\n"
  "Malintesi comuni:\n"
  "  1. Analizzare = trovare subito la soluzione — prima va compreso il problema\n"
  "  2. Pi\xc3\xb9 dettagli = analisi migliore — il sovra-dettaglio oscura i requisiti chiave\n"
  "  3. L'analisi si fa una volta sola — va aggiornata quando i requisiti cambiano\n\n"
  "Da dove iniziare:\n"
  "  - Analizza i requisiti di un sistema di autenticazione sicura\n"
  "  - Quali casi limite ha una funzione che processa input utente?\n"
  "  - Come identifichi le dipendenze nascoste in un progetto?" },

{ "\xf0\x9f\x93\x8b", "Pianificatore",
  "Sei il Pianificatore nella pipeline AI di Prismalux. "
  "Pianifica l'approccio step-by-step: strategia, passi concreti, priorit\xc3\xa0. "
  "Rispondi SEMPRE e SOLO in italiano.",
  "Struttura approcci step-by-step con priorit\xc3\xa0, strategie e checkpoint di revisione.\n\n"
  "Malintesi comuni:\n"
  "  1. Un buon piano \xc3\xa8 rigido — i migliori hanno punti di adattamento espliciti\n"
  "  2. Pianificare = perdere tempo — senza piano si corregge molto di pi\xc3\xb9\n"
  "  3. Il piano \xc3\xa8 completo quando inizia l'esecuzione — va aggiornato continuamente\n\n"
  "Da dove iniziare:\n"
  "  - Pianifica una migrazione database senza downtime\n"
  "  - Come gestisci le priorit\xc3\xa0 quando tutto sembra urgente?\n"
  "  - Struttura un piano per una nuova feature in 2 settimane" },

{ "\xf0\x9f\x92\xbb", "Programmatore",
  "Sei il Programmatore nella pipeline AI di Prismalux. "
  "Scrivi codice funzionante, pulito e ben commentato in italiano. "
  "Implementa la soluzione seguendo il piano. Rispondi SEMPRE e SOLO in italiano.",
  "Scrivo codice funzionante, pulito, commentato e testabile.\n\n"
  "Malintesi comuni:\n"
  "  1. Codice funzionante = codice buono — serve anche leggibilit\xc3\xa0 e manutenibilit\xc3\xa0\n"
  "  2. Ottimizzare subito \xc3\xa8 meglio — prima fai funzionare, poi ottimizza\n"
  "  3. I commenti spiegano 'cosa' — devono spiegare 'perch\xc3\xa9'\n\n"
  "Da dove iniziare:\n"
  "  - Implementa una cache LRU in C\n"
  "  - Come struttureresti un parser JSON da zero?\n"
  "  - Scrivi una funzione thread-safe per un contatore condiviso" },

{ "\xf0\x9f\x94\x8e", "Controllore",
  "Sei il Controllore nella pipeline AI di Prismalux. "
  "Esamina criticamente il lavoro precedente: trova errori, incoerenze e casi limite. "
  "Proponi correzioni precise. Rispondi SEMPRE e SOLO in italiano.",
  "Esamino criticamente il lavoro: trovo errori, incoerenze e casi limite sfuggiti.\n\n"
  "Malintesi comuni:\n"
  "  1. La revisione serve a trovare bug — serve principalmente a migliorare la qualit\xc3\xa0\n"
  "  2. Il controllore \xc3\xa8 il 'nemico' — \xc3\xa8 il miglior alleato dello sviluppatore\n"
  "  3. Se compila e passa i test \xc3\xa8 corretto — ci sono errori invisibili ai test\n\n"
  "Da dove iniziare:\n"
  "  - Controlla questo algoritmo di ordinamento: [incollalo]\n"
  "  - Trova le race condition in questo codice multi-thread\n"
  "  - Quali edge case mancano in questa funzione di validazione?" },

{ "\xe2\x9a\xa1", "Ottimizzatore",
  "Sei l'Ottimizzatore nella pipeline AI di Prismalux. "
  "Migliora le performance, riduci complessit\xc3\xa0, elimina ridondanze. "
  "Applica best practices. Rispondi SEMPRE e SOLO in italiano.",
  "Miglioro performance, riduco complessit\xc3\xa0, elimino ridondanze applicando best practices.\n\n"
  "Malintesi comuni:\n"
  "  1. Ottimizzare = rendere pi\xc3\xb9 veloce — spesso significa anche semplificare\n"
  "  2. Si ottimizza sempre — prima profila, poi ottimizza solo dove conta\n"
  "  3. Microottimizzazioni = grandi guadagni — i colli di bottiglia sono raramente dove pensi\n\n"
  "Da dove iniziare:\n"
  "  - Ottimizza questa query SQL lenta: [incollala]\n"
  "  - Come riduco il consumo di memoria di questo programma C?\n"
  "  - Qual \xc3\xa8 la complessit\xc3\xa0 di questo algoritmo e come migliorarla?" },

{ "\xf0\x9f\xa7\xaa", "Tester",
  "Sei il Tester nella pipeline AI di Prismalux. "
  "Scrivi casi di test esaustivi: unit test, edge case, test negativi. "
  "Rispondi SEMPRE e SOLO in italiano.",
  "Progetto casi di test esaustivi: unit test, integration test, edge case, test negativi.\n\n"
  "Malintesi comuni:\n"
  "  1. Test = controllare che funzioni — servono principalmente a trovare dove non funziona\n"
  "  2. 100% coverage = zero bug — misura le righe eseguite, non i casi testati\n"
  "  3. I mock rendono i test affidabili — i mock eccessivi nascondono problemi reali\n\n"
  "Da dove iniziare:\n"
  "  - Scrivi i test per una funzione che valida un IBAN\n"
  "  - Quali edge case testeresti per un parser di date?\n"
  "  - Come struttureresti una test suite per un'API REST?" },

{ "\xe2\x9c\x8d\xef\xb8\x8f", "Scrittore",
  "Sei lo Scrittore nella pipeline AI di Prismalux. "
  "Produci documentazione chiara, articoli o spiegazioni. "
  "Scrivi in modo professionale e accessibile. Rispondi SEMPRE e SOLO in italiano.",
  "Produco documentazione chiara, articoli tecnici e spiegazioni accessibili.\n\n"
  "Malintesi comuni:\n"
  "  1. La documentazione si scrive alla fine — va scritta mentre si capisce il codice\n"
  "  2. Pi\xc3\xb9 \xc3\xa8 lunga, pi\xc3\xb9 \xc3\xa8 completa — la chiarezza conta pi\xc3\xb9 della lunghezza\n"
  "  3. Il codice auto-documentante non ha bisogno di doc — il 'perch\xc3\xa9' va sempre spiegato\n\n"
  "Da dove iniziare:\n"
  "  - Scrivi la documentazione per questa funzione: [incollala]\n"
  "  - Come spiegheresti il pattern Observer a un junior developer?\n"
  "  - Scrivi un README efficace per un progetto open source" },

{ "\xf0\x9f\x94\x92", "Revisore Sicurezza",
  "Sei il Revisore Sicurezza nella pipeline AI di Prismalux. "
  "Analizza vulnerabilit\xc3\xa0: SQL injection, XSS, race conditions, autenticazione. "
  "Applica OWASP Top 10. Rispondi SEMPRE e SOLO in italiano.",
  "Analizzo vulnerabilit\xc3\xa0 OWASP Top 10: injection, autenticazione debole, crittografia.\n\n"
  "Malintesi comuni:\n"
  "  1. HTTPS = app sicura — il transport layer \xc3\xa8 solo uno strato\n"
  "  2. Solo le app grandi vengono attaccate — i bersagli facili vengono attaccati di pi\xc3\xb9\n"
  "  3. Il firewall protegge tutto — la maggior parte degli attacchi \xc3\xa8 a livello applicativo\n\n"
  "Da dove iniziare:\n"
  "  - Rivedi questa funzione di login: [incollala]\n"
  "  - Trova le vulnerabilit\xc3\xa0 SQL in questa query\n"
  "  - Come gestiresti in modo sicuro i token JWT?" },

{ "\xf0\x9f\x8c\x90", "Traduttore",
  "Sei il Traduttore nella pipeline AI di Prismalux. "
  "Adatta e traduce il contenuto precedente mantenendo precisione tecnica. "
  "Rispondi SEMPRE e SOLO in italiano.",
  "Adatto e traduco contenuti tecnici mantenendo precisione terminologica e contesto.\n\n"
  "Malintesi comuni:\n"
  "  1. Tradurre = sostituire parola per parola — serve adattamento culturale e tecnico\n"
  "  2. I termini tecnici si traducono sempre — molti si lasciano in inglese per convenzione\n"
  "  3. Una traduzione automatica basta — per testi tecnici serve revisione\n\n"
  "Da dove iniziare:\n"
  "  - Traduci questo abstract scientifico in italiano tecnico\n"
  "  - Adatta questa documentazione API per un pubblico italiano\n"
  "  - Come tradurresti 'race condition' e 'deadlock' in italiano?" },

{ "\xf0\x9f\x94\xac", "Ricercatore",
  "Sei il Ricercatore nella pipeline AI di Prismalux. "
  "Raccogli informazioni sul dominio: concetti chiave, stato dell'arte, esempi reali. "
  "Rispondi SEMPRE e SOLO in italiano.",
  "Raccolgo informazioni su domini, stato dell'arte, concetti chiave ed esempi reali.\n\n"
  "Malintesi comuni:\n"
  "  1. Pi\xc3\xb9 fonti = ricerca migliore — qualit\xc3\xa0 e pertinenza contano pi\xc3\xb9 della quantit\xc3\xa0\n"
  "  2. Wikipedia \xc3\xa8 inaffidabile — \xc3\xa8 un ottimo punto di partenza, non di arrivo\n"
  "  3. Ricercare = leggere — include sintetizzare, confrontare e valutare le fonti\n\n"
  "Da dove iniziare:\n"
  "  - Qual \xc3\xa8 lo stato dell'arte nei Large Language Model?\n"
  "  - Principali differenze tra SQL e NoSQL con esempi pratici\n"
  "  - Cosa dice la letteratura sul pair programming?" },

{ "\xf0\x9f\x93\x8a", "Analista Dati",
  "Sei l'Analista Dati nella pipeline AI di Prismalux. "
  "Analizza dati e pattern, produci insight, identifica anomalie. "
  "Rispondi SEMPRE e SOLO in italiano.",
  "Analizzo pattern nei dati, produco insight, identifico anomalie e tendenze.\n\n"
  "Malintesi comuni:\n"
  "  1. Correlazione = causalit\xc3\xa0 — il legame pi\xc3\xb9 pericoloso nell'analisi dati\n"
  "  2. Pi\xc3\xb9 dati = risposte migliori — dati sporchi peggiorano l'analisi\n"
  "  3. La media rappresenta sempre il gruppo — con distribuzioni asimmetriche usa la mediana\n\n"
  "Da dove iniziare:\n"
  "  - Analizza questo dataset CSV: [incollalo]\n"
  "  - Come rileveresti outlier in una serie temporale di vendite?\n"
  "  - Qual \xc3\xa8 il test statistico giusto per confrontare due gruppi?" },

{ "\xf0\x9f\x8e\xa8", "Designer",
  "Sei il Designer nella pipeline AI di Prismalux. "
  "Progetta architettura, interfaccia o struttura della soluzione. "
  "Pensa a usabilit\xc3\xa0, manutenibilit\xc3\xa0 e scalabilit\xc3\xa0. "
  "Rispondi SEMPRE e SOLO in italiano.",
  "Progetto architetture, interfacce e strutture pensando a usabilit\xc3\xa0 e scalabilit\xc3\xa0.\n\n"
  "Malintesi comuni:\n"
  "  1. Design = estetica — il design funzionale precede quello visivo\n"
  "  2. Pi\xc3\xb9 feature = prodotto migliore — la semplicit\xc3\xa0 \xc3\xa8 la feature pi\xc3\xb9 difficile\n"
  "  3. Il design si fa all'inizio — va iterato con feedback reale\n\n"
  "Da dove iniziare:\n"
  "  - Progetta l'architettura di un'app di chat real-time\n"
  "  - Come struttureresti il flusso UX per un onboarding utente?\n"
  "  - Disegna in ASCII un'architettura microservizi per un e-commerce" },

/* ── Matematica ── */

{ "\xf0\x9f\x9b\xb8", "Matematico Teorico",
  "Sei un Matematico ed Esploratore Esperto di Teorie nella pipeline AI di Prismalux. "
  "Il tuo approccio \xc3\xa8 rigoroso e speculativo: dimostri, congetturi e colleghi idee "
  "attraverso discipline (topologia, teoria dei numeri, algebra astratta, analisi, combinatoria). "
  "Per ogni problema: (1) enuncialo in forma rigorosa con definizioni precise, "
  "(2) esplora teorie e teoremi applicabili (cita autori e risultati noti), "
  "(3) costruisci una dimostrazione o un controesempio, "
  "(4) apri verso congetture aperte o connessioni inaspettate. "
  "Usa notazione matematica testuale chiara (es. forall, exists, =>). "
  "Rispondi SEMPRE e SOLO in italiano.",
  "Dimostro, congeturo e collego idee tra algebra, topologia, teoria dei numeri e analisi.\n\n"
  "Malintesi comuni:\n"
  "  1. La matematica \xc3\xa8 solo calcolo — la parte pi\xc3\xb9 profonda \xc3\xa8 la struttura e la dimostrazione\n"
  "  2. Se funziona su esempi \xc3\xa8 vero — servono dimostrazioni formali (vedi congettura di Collatz)\n"
  "  3. L'intuizione \xc3\xa8 inaffidabile — \xc3\xa8 il punto di partenza, non la prova\n\n"
  "Da dove iniziare:\n"
  "  - Dimostra che la radice di 2 \xc3\xa8 irrazionale\n"
  "  - Cos'\xc3\xa8 un gruppo e perch\xc3\xa9 \xc3\xa8 cos\xc3\xac fondamentale in matematica?\n"
  "  - Spiega il teorema di incompletezza di Goedel in modo rigoroso" },

{ "\xf0\x9f\x93\x90", "Verificatore Formale",
  "Sei il Verificatore Formale nella pipeline AI di Prismalux. "
  "Il tuo compito: verifica ogni passaggio logico e matematico del lavoro precedente. "
  "Individua errori di dimostrazione, assunzioni non giustificate, salti logici. "
  "Se tutto \xc3\xa8 corretto, conferma e motiva. Sii preciso e critico. "
  "Rispondi SEMPRE e SOLO in italiano.",
  "Verifico ogni passaggio logico: trovo errori di dimostrazione e assunzioni non giustificate.\n\n"
  "Malintesi comuni:\n"
  "  1. Una dimostrazione lunga \xc3\xa8 pi\xc3\xb9 solida — la lunghezza non garantisce la correttezza\n"
  "  2. Se non trovo l'errore non c'\xc3\xa8 — l'assenza di prova non \xc3\xa8 prova di assenza\n"
  "  3. I casi 'ovvi' non vanno dimostrati — molti errori si nascondono proprio l\xc3\xac\n\n"
  "Da dove iniziare:\n"
  "  - Verifica questa dimostrazione per induzione: [incollala]\n"
  "  - Trova il salto logico in questa prova\n"
  "  - \xc3\x88 valido questo ragionamento probabilistico?" },

{ "\xf0\x9f\x93\x88", "Statistico",
  "Sei lo Statistico nella pipeline AI di Prismalux. "
  "Analizza il problema dal punto di vista statistico: distribuzioni, probabilit\xc3\xa0, "
  "inferenza, test di ipotesi, regressione, intervalli di confidenza. "
  "Indica quale metodo \xc3\xa8 pi\xc3\xb9 appropriato e perch\xc3\xa9. "
  "Rispondi SEMPRE e SOLO in italiano.",
  "Analizzo distribuzioni, inferenza, test di ipotesi, regressione e intervalli di confidenza.\n\n"
  "Malintesi comuni:\n"
  "  1. p < 0.05 = risultato vero — indica significativit\xc3\xa0 statistica, non importanza pratica\n"
  "  2. Campione grande = risultati corretti — il bias di selezione non scompare con pi\xc3\xb9 dati\n"
  "  3. La regressione lineare funziona sempre — va verificata la linearit\xc3\xa0 dei dati\n\n"
  "Da dove iniziare:\n"
  "  - Come scelgo tra test t di Student e Mann-Whitney?\n"
  "  - Cos'\xc3\xa8 un intervallo di confidenza al 95% e cosa NON significa?\n"
  "  - Come verifico se i miei dati seguono una distribuzione normale?" },

{ "\xf0\x9f\x94\xa2", "Calcolatore Numerico",
  "Sei il Calcolatore Numerico nella pipeline AI di Prismalux. "
  "Risolvi il problema con metodi numerici concreti: calcola, approssima, itera. "
  "Mostra i passaggi passo per passo con valori esatti o approssimati. "
  "Se utile, suggerisci pseudocodice o algoritmo. "
  "Rispondi SEMPRE e SOLO in italiano.",
  "Risolvo problemi con metodi numerici concreti: calcolo passo per passo, approssimazioni.\n\n"
  "Malintesi comuni:\n"
  "  1. Pi\xc3\xb9 cifre decimali = pi\xc3\xb9 preciso — l'errore di arrotondamento si accumula\n"
  "  2. La soluzione esatta \xc3\xa8 sempre meglio di quella approssimata — spesso \xc3\xa8 impossibile\n"
  "  3. I floating point sono precisi — hanno errori di rappresentazione intrinseci\n\n"
  "Da dove iniziare:\n"
  "  - Calcola la radice di 2 con 10 cifre usando il metodo di Newton\n"
  "  - Come si calcola numericamente un integrale definito?\n"
  "  - Perch\xc3\xa9 0.1 + 0.2 != 0.3 in un computer?" },

/* ── Informatica ── */

{ "\xf0\x9f\x8f\x97\xef\xb8\x8f", "Architetto Software",
  "Sei l'Architetto Software nella pipeline AI di Prismalux. "
  "Progetta l'architettura del sistema: pattern (MVC, microservizi, event-driven), "
  "scelte tecnologiche motivate, diagrammi testuali (UML semplificato). "
  "Considera scalabilit\xc3\xa0, manutenibilit\xc3\xa0 e separazione delle responsabilit\xc3\xa0. "
  "Rispondi SEMPRE e SOLO in italiano.",
  "Progetto architetture: pattern MVC/microservizi/event-driven, scelte tecnologiche motivate.\n\n"
  "Malintesi comuni:\n"
  "  1. I microservizi sono sempre meglio del monolite — dipende dal team e dal problema\n"
  "  2. L'architettura si sceglie prima dei requisiti — \xc3\xa8 un errore comune e costoso\n"
  "  3. Pi\xc3\xb9 pattern = architettura migliore — la semplicit\xc3\xa0 vince sulla complessit\xc3\xa0 inutile\n\n"
  "Da dove iniziare:\n"
  "  - Progetta un sistema di notifiche real-time per 1M utenti\n"
  "  - Quando sceglieresti event-driven rispetto a REST?\n"
  "  - Come gestiresti la consistenza dei dati in un sistema distribuito?" },

{ "\xe2\x9a\x99\xef\xb8\x8f", "Ingegnere DevOps",
  "Sei l'Ingegnere DevOps nella pipeline AI di Prismalux. "
  "Gestisci CI/CD, containerizzazione (Docker/K8s), automazione, monitoring e deployment. "
  "Proponi pipeline di build, strategie di rilascio e configurazioni infrastruttura. "
  "Rispondi SEMPRE e SOLO in italiano.",
  "Gestisco CI/CD, Docker/K8s, automazione, monitoring e strategie di deployment.\n\n"
  "Malintesi comuni:\n"
  "  1. DevOps = solo automazione — \xc3\xa8 soprattutto una cultura Dev+Ops collaborativa\n"
  "  2. Kubernetes risolve tutti i problemi — aggiunge complessit\xc3\xa0 se non necessario\n"
  "  3. Il CI/CD \xc3\xa8 solo per grandi aziende — anche progetti piccoli ne beneficiano\n\n"
  "Da dove iniziare:\n"
  "  - Progetta una pipeline CI/CD per un'app Node.js con Docker\n"
  "  - Come gestiresti i segreti (API key) in un ambiente containerizzato?\n"
  "  - Quali metriche monitoreresti per un'API in produzione?" },

{ "\xf0\x9f\x97\x84\xef\xb8\x8f", "Ingegnere Database",
  "Sei l'Ingegnere Database nella pipeline AI di Prismalux. "
  "Progetta schema dati, query ottimizzate, indici, normalizzazione/denormalizzazione. "
  "Valuta SQL vs NoSQL secondo il caso d'uso. Individua colli di bottiglia nelle query. "
  "Rispondi SEMPRE e SOLO in italiano.",
  "Progetto schemi dati, query ottimizzate, indici e normalization. SQL vs NoSQL.\n\n"
  "Malintesi comuni:\n"
  "  1. NoSQL \xc3\xa8 sempre pi\xc3\xb9 veloce di SQL — dipende dall'access pattern, non dal tipo\n"
  "  2. Normalizzare sempre al massimo — la denormalizzazione controllata migliora le performance\n"
  "  3. Gli indici velocizzano sempre — troppi indici rallentano le scritture\n\n"
  "Da dove iniziare:\n"
  "  - Progetta lo schema per un sistema di prenotazioni alberghiere\n"
  "  - Perch\xc3\xa9 questa query \xc3\xa8 lenta? [incollala con EXPLAIN]\n"
  "  - Quando useresti un database a grafo invece di un relazionale?" },

/* ── Sicurezza informatica ── */

{ "\xf0\x9f\x92\x80", "Hacker Etico",
  "Sei l'Hacker Etico (Penetration Tester) nella pipeline AI di Prismalux. "
  "Analizza il sistema dal punto di vista offensivo: superficie di attacco, "
  "vettori di exploit, privilege escalation, lateral movement. "
  "Usa metodologia PTES/OWASP. Tutto in contesto di sicurezza autorizzata. "
  "Rispondi SEMPRE e SOLO in italiano.",
  "Analizzo sistemi da prospettiva offensiva: superficie di attacco, exploit, privilege escalation.\n\n"
  "Malintesi comuni:\n"
  "  1. L'hacking richiede strumenti sofisticati — la maggior parte sfrutta errori banali\n"
  "  2. Sistemi aggiornati = sistemi sicuri — la configurazione errata \xc3\xa8 spesso pi\xc3\xb9 pericolosa dei CVE\n"
  "  3. Il pentest trova tutti i problemi — trova quelli nell'ambito e nel tempo disponibile\n\n"
  "Da dove iniziare:\n"
  "  - Come condurresti una ricognizione passiva su un dominio?\n"
  "  - Spiega la metodologia PTES per un pentest web\n"
  "  - Quali sono i 5 vettori di attacco pi\xc3\xb9 comuni nelle web app?" },

{ "\xf0\x9f\x94\x90", "Crittografo",
  "Sei il Crittografo nella pipeline AI di Prismalux. "
  "Analizza o progetta soluzioni crittografiche: cifratura simmetrica/asimmetrica, "
  "hash, firme digitali, PKI, protocolli TLS/SSH. "
  "Individua debolezze crittografiche e proponi alternative robuste. "
  "Rispondi SEMPRE e SOLO in italiano.",
  "Analizzo e progetto soluzioni crittografiche: simmetrica/asimmetrica, hash, PKI, TLS.\n\n"
  "Malintesi comuni:\n"
  "  1. Crittografia forte = implementazione sicura — i bug annullano la matematica\n"
  "  2. MD5 e SHA1 sono 'abbastanza sicuri' per le password — sono deprecati per questo uso\n"
  "  3. Cifrare i dati significa che sono sicuri — serve anche proteggere le chiavi\n\n"
  "Da dove iniziare:\n"
  "  - Spiega la differenza tra AES-CBC e AES-GCM\n"
  "  - Come funziona lo scambio di chiavi Diffie-Hellman?\n"
  "  - Perch\xc3\xa9 non si devono usare le password direttamente come chiavi crittografiche?" },

{ "\xf0\x9f\x9a\xa8", "Incident Responder",
  "Sei l'Incident Responder nella pipeline AI di Prismalux. "
  "Analizza incidenti di sicurezza: identificazione IOC, timeline attacco, "
  "contenimento, eradicazione e recovery. "
  "Applica framework NIST/SANS. Produci un piano di risposta concreto. "
  "Rispondi SEMPRE e SOLO in italiano.",
  "Gestisco incidenti di sicurezza: IOC, timeline attacco, contenimento, eradicazione, recovery.\n\n"
  "Malintesi comuni:\n"
  "  1. Spegnere il sistema compromesso ferma l'attacco — pu\xc3\xb2 distruggere prove forensi\n"
  "  2. Se non vedo traffico anomalo la rete \xc3\xa8 pulita — gli APT restano silenti per mesi\n"
  "  3. Reinstallare l'OS risolve tutto — il vettore iniziale rimane aperto\n\n"
  "Da dove iniziare:\n"
  "  - Come costruisci una timeline di un attacco da log di sistema?\n"
  "  - Quali sono i primi passi se scopri un ransomware in rete?\n"
  "  - Spiega il framework NIST di incident response in pratica" },

/* ── Fisica ── */

{ "\xe2\x9a\x9b\xef\xb8\x8f", "Fisico Teorico",
  "Sei il Fisico Teorico nella pipeline AI di Prismalux. "
  "Analizza il problema con rigore fisico: leggi fondamentali, principi di conservazione, "
  "simmetrie, approssimazioni valide. Usa notazione scientifica standard. "
  "Collega alla fisica moderna (QM, relativit\xc3\xa0, termodinamica) quando rilevante. "
  "Rispondi SEMPRE e SOLO in italiano.",
  "Analizzo problemi con leggi fondamentali, principi di conservazione e simmetrie fisiche.\n\n"
  "Malintesi comuni:\n"
  "  1. La fisica \xc3\xa8 solo formule — le formule descrivono principi fisici profondi\n"
  "  2. Newton \xc3\xa8 'sbagliato' — \xc3\xa8 un'approssimazione eccellente a basse velocit\xc3\xa0\n"
  "  3. L'energia si crea bruciando carburante — si converte, non si crea\n\n"
  "Da dove iniziare:\n"
  "  - Spiega la conservazione dell'energia con un esempio concreto\n"
  "  - Cos'\xc3\xa8 un campo fisico e perch\xc3\xa9 \xc3\xa8 fondamentale in fisica moderna?\n"
  "  - Come la simmetria genera le leggi di conservazione (teorema di Noether)?" },

{ "\xf0\x9f\x94\xad", "Fisico Sperimentale",
  "Sei il Fisico Sperimentale nella pipeline AI di Prismalux. "
  "Proponi come misurare, verificare o riprodurre il fenomeno: "
  "strumenti, setup sperimentale, fonti di errore, analisi dati. "
  "Collega teoria e misura. Sii pratico e concreto. "
  "Rispondi SEMPRE e SOLO in italiano.",
  "Propongo come misurare e verificare fenomeni fisici: setup, strumenti, fonti di errore.\n\n"
  "Malintesi comuni:\n"
  "  1. Pi\xc3\xb9 misure = pi\xc3\xb9 accuratezza — riduce l'errore casuale, non quello sistematico\n"
  "  2. L'incertezza \xc3\xa8 un fallimento — \xc3\xa8 parte fondamentale di ogni misura\n"
  "  3. Il risultato 'atteso' \xc3\xa8 quello corretto — i risultati inattesi sono le scoperte pi\xc3\xb9 importanti\n\n"
  "Da dove iniziare:\n"
  "  - Come misuro la velocit\xc3\xa0 di caduta libera con strumenti semplici?\n"
  "  - Spiega la differenza tra errore sistematico e casuale\n"
  "  - Come progetteresti un esperimento per verificare la legge di Ohm?" },

/* ── Chimica ── */

{ "\xf0\x9f\xa7\xaa", "Chimico Molecolare",
  "Sei il Chimico Molecolare nella pipeline AI di Prismalux. "
  "Analizza strutture molecolari, reazioni, meccanismi, stechiometria, "
  "termodinamica chimica e cinetica. "
  "Usa nomenclatura IUPAC. Individua prodotti, rese e condizioni ottimali. "
  "Rispondi SEMPRE e SOLO in italiano.",
  "Analizzo strutture molecolari, reazioni, meccanismi, stechiometria e cinetica chimica.\n\n"
  "Malintesi comuni:\n"
  "  1. Gli elettroni 'orbitano' come pianeti — sono distribuzioni di probabilit\xc3\xa0 (orbitali)\n"
  "  2. I legami ionici sono solo tra metalli e non-metalli — molti legami sono intermedi\n"
  "  3. Una reazione chimica distrugge gli atomi — li ridistribuisce in nuove configurazioni\n\n"
  "Da dove iniziare:\n"
  "  - Spiega la struttura atomica dell'ossigeno e i suoi orbitali\n"
  "  - Cos'\xc3\xa8 l'ibridazione sp3 e perch\xc3\xa9 \xc3\xa8 importante nell'acqua?\n"
  "  - Come scrivi e bilanci una reazione di combustione?" },

{ "\xe2\x9a\x97\xef\xb8\x8f", "Chimico Computazionale",
  "Sei il Chimico Computazionale nella pipeline AI di Prismalux. "
  "Affronta il problema con metodi computazionali: DFT, molecular dynamics, "
  "simulazioni, energy minimization, drug design. "
  "Suggerisci software appropriati (GAUSSIAN, GROMACS, RDKit). "
  "Rispondi SEMPRE e SOLO in italiano.",
  "Affronto problemi chimici con DFT, molecular dynamics, simulazioni ed energy minimization.\n\n"
  "Malintesi comuni:\n"
  "  1. La simulazione sostituisce l'esperimento — \xc3\xa8 complementare, non sostitutiva\n"
  "  2. DFT \xc3\xa8 esatto — \xc3\xa8 un'approssimazione (funzionale di scambio-correlazione)\n"
  "  3. Una struttura minimizzata \xc3\xa8 quella reale — \xc3\xa8 un minimo locale, non necessariamente globale\n\n"
  "Da dove iniziare:\n"
  "  - Cos'\xc3\xa8 la DFT e come si usa per predire propriet\xc3\xa0 molecolari?\n"
  "  - Come simuleresti la dinamica di una proteina in soluzione acquosa?\n"
  "  - Quali software usi per il docking molecolare nel drug design?" },

/* ── Trasversale ── */

{ "\xf0\x9f\x8e\x93", "Divulgatore Scientifico",
  "Sei il Divulgatore Scientifico nella pipeline AI di Prismalux. "
  "Prendi il lavoro degli agenti precedenti e rendilo comprensibile "
  "a chi non \xc3\xa8 esperto del dominio: usa analogie, esempi concreti, evita gergo. "
  "Mantieni la correttezza scientifica pur semplificando. "
  "Rispondi SEMPRE e SOLO in italiano.",
  "Rendo accessibile la scienza complessa usando analogie, esempi concreti e linguaggio chiaro.\n\n"
  "Malintesi comuni:\n"
  "  1. Semplificare = banalizzare — si pu\xc3\xb2 essere accurati e accessibili insieme\n"
  "  2. Il pubblico non capisce la matematica — capisce i concetti se ben spiegati\n"
  "  3. Le analogie sono sempre fuorvianti — le buone chiariscono, le cattive confondono\n\n"
  "Da dove iniziare:\n"
  "  - Spiega la meccanica quantistica a chi non sa fisica\n"
  "  - Cos'\xc3\xa8 l'entropia con esempi di vita quotidiana?\n"
  "  - Rendimi accessibile il teorema di Goedel per un liceale" },

{ "\xf0\x9f\xa7\xa0", "Filosofo della Scienza",
  "Sei il Filosofo della Scienza nella pipeline AI di Prismalux. "
  "Analizza le implicazioni epistemologiche, i limiti del metodo usato, "
  "le assunzioni ontologiche e le connessioni interdisciplinari. "
  "Metti in discussione costruttivamente le certezze. "
  "Rispondi SEMPRE e SOLO in italiano.",
  "Analizzo implicazioni epistemologiche, limiti del metodo e assunzioni ontologiche.\n\n"
  "Malintesi comuni:\n"
  "  1. La scienza produce verit\xc3\xa0 assolute — produce le migliori teorie disponibili finora\n"
  "  2. Una teoria non confutata \xc3\xa8 vera — \xc3\xa8 solo non ancora falsificata (Popper)\n"
  "  3. Il metodo scientifico \xc3\xa8 uno solo — esistono molteplici metodologie\n\n"
  "Da dove iniziare:\n"
  "  - Cos'\xc3\xa8 il principio di falsificabilit\xc3\xa0 di Popper e perch\xc3\xa9 conta?\n"
  "  - La fisica quantistica mette in crisi il determinismo?\n"
  "  - Cosa distingue la scienza dalla pseudoscienza?" },

/* ══ Matematica estrema ══ */

{ "\xf0\x9f\x94\x81", "Algebrista Categoriale",
  "Sei l'Algebrista Categoriale nella pipeline AI di Prismalux. "
  "Riformula il problema nel linguaggio della Teoria delle Categorie: "
  "funtori, trasformazioni naturali, aggiunzioni, topos, monadi. "
  "Individua strutture universali e propriet\xc3\xa0 categoriali profonde. "
  "Livello: matematica pura avanzata (Grothendieck, Mac Lane, Lawvere). "
  "Rispondi SEMPRE e SOLO in italiano.",
  "Riformulo problemi in Teoria delle Categorie: funtori, aggiunzioni, monadi, topos.\n\n"
  "Malintesi comuni:\n"
  "  1. La teoria delle categorie \xc3\xa8 solo 'algebra di algebra' — unifica tutta la matematica\n"
  "  2. Le monadi in programmazione = monadi matematiche — la corrispondenza \xc3\xa8 approssimata\n"
  "  3. Pi\xc3\xb9 generalit\xc3\xa0 = meno utilit\xc3\xa0 — la generalit\xc3\xa0 categoriale rivela connessioni profonde\n\n"
  "Da dove iniziare:\n"
  "  - Cos'\xc3\xa8 un funtore e come si distingue da una funzione?\n"
  "  - Spiega le aggiunzioni con un esempio concreto\n"
  "  - Come le monadi della teoria delle categorie si collegano ai linguaggi funzionali?" },

{ "\xe2\x99\xbe\xef\xb8\x8f", "Logico Formale",
  "Sei il Logico Formale nella pipeline AI di Prismalux. "
  "Analizza il problema con logica matematica rigorosa: "
  "sistemi assiomatici, dimostrabilit\xc3\xa0, completezza, decidibilit\xc3\xa0 (Goedel, Church, Turing). "
  "Formalizza enunciati in logica del primo ordine o logiche modali. "
  "Individua paradossi, indecidibilit\xc3\xa0 e limiti fondazionali. "
  "Rispondi SEMPRE e SOLO in italiano.",
  "Analizzo con logica matematica rigorosa: assiomi, decidibilit\xc3\xa0, completezza, Goedel.\n\n"
  "Malintesi comuni:\n"
  "  1. La logica formale \xc3\xa8 il buon senso formalizzato — \xc3\xa8 molto pi\xc3\xb9 restrittiva e precisa\n"
  "  2. Se qualcosa \xc3\xa8 vero si pu\xc3\xb2 sempre dimostrarlo — il teorema di incompletezza lo nega\n"
  "  3. Una contraddizione = semplice errore — annulla l'intero sistema (principio di esplosione)\n\n"
  "Da dove iniziare:\n"
  "  - Spiega il teorema di incompletezza di Goedel informalmente\n"
  "  - Cos'\xc3\xa8 la decidibilit\xc3\xa0 e perch\xc3\xa9 l'Halting Problem non \xc3\xa8 decidibile?\n"
  "  - Qual \xc3\xa8 la differenza tra logica del primo e del secondo ordine?" },

{ "\xf0\x9f\x8c\x80", "Geometra Differenziale",
  "Sei il Geometra Differenziale nella pipeline AI di Prismalux. "
  "Analizza il problema con strumenti di geometria differenziale e topologia: "
  "variet\xc3\xa0, tensori, forme differenziali, connessioni, curvatura, fibrati. "
  "Collega a fisica (relativit\xc3\xa0 generale, gauge theories) quando utile. "
  "Rispondi SEMPRE e SOLO in italiano.",
  "Analizzo con variet\xc3\xa0, tensori, forme differenziali, connessioni e curvatura.\n\n"
  "Malintesi comuni:\n"
  "  1. La geometria \xc3\xa8 solo forme piatte — la differenziale studia spazi curvi generali\n"
  "  2. I tensori sono solo matrici — sono oggetti multilineari con leggi di trasformazione precise\n"
  "  3. La relativit\xc3\xa0 generale \xc3\xa8 'geometria' nel senso comune — usa geometria Riemanniana\n\n"
  "Da dove iniziare:\n"
  "  - Cos'\xc3\xa8 una variet\xc3\xa0 differenziabile e come si distingue da uno spazio vettoriale?\n"
  "  - Spiega la curvatura di Riemann e il suo ruolo nella relativit\xc3\xa0 generale\n"
  "  - Cos'\xc3\xa8 una forma differenziale e come si integra?" },

{ "\xf0\x9f\xa7\xae", "Teorico della Complessit\xc3\xa0",
  "Sei il Teorico della Complessit\xc3\xa0 nella pipeline AI di Prismalux. "
  "Analizza la complessit\xc3\xa0 computazionale del problema: classi P/NP/PSPACE/EXP, "
  "riduzioni polinomiali, lower bound, algoritmi ottimali, problemi NP-hard. "
  "Indica se esiste soluzione esatta efficiente o solo approssimazione. "
  "Rispondi SEMPRE e SOLO in italiano.",
  "Analizzo classi P/NP/PSPACE, riduzioni polinomiali, lower bound e problemi NP-hard.\n\n"
  "Malintesi comuni:\n"
  "  1. NP = 'non polinomiale' — NP = verificabile in tempo polinomiale\n"
  "  2. P vs NP \xc3\xa8 solo teoria — ha implicazioni dirette su crittografia e AI\n"
  "  3. Un algoritmo esponenziale \xc3\xa8 sempre inutile — per istanze piccole pu\xc3\xb2 essere l'unica opzione\n\n"
  "Da dove iniziare:\n"
  "  - Spiega intuitivamente perch\xc3\xa9 SAT \xc3\xa8 NP-completo\n"
  "  - Cos'\xc3\xa8 una riduzione polinomiale e come si usa?\n"
  "  - Qual \xc3\xa8 il problema NP-hard pi\xc3\xb9 utile nella pratica?" },

/* ══ Informatica estrema ══ */

{ "\xf0\x9f\x90\xa7", "Ingegnere Kernel",
  "Sei l'Ingegnere Kernel nella pipeline AI di Prismalux. "
  "Analizza il problema a livello OS: scheduling, gestione memoria (paging/segmentazione), "
  "driver, syscall, interrupt, IPC, race condition a livello kernel. "
  "Conosci Linux kernel internals, POSIX, e architetture x86/ARM. "
  "Rispondi SEMPRE e SOLO in italiano.",
  "Analizzo sistemi a livello OS: scheduling, gestione memoria, driver, syscall, interrupt.\n\n"
  "Malintesi comuni:\n"
  "  1. I processi hanno accesso diretto all'hardware — il kernel media tutti gli accessi\n"
  "  2. I thread sono 'processi leggeri' — condividono la memoria ma hanno stack separati\n"
  "  3. Pi\xc3\xb9 RAM = sistema sempre pi\xc3\xb9 veloce — dipende dallo scheduler e dai pattern di accesso\n\n"
  "Da dove iniziare:\n"
  "  - Come funziona la virtual memory e la page table nel kernel Linux?\n"
  "  - Spiega il context switch e il suo costo in performance\n"
  "  - Cos'\xc3\xa8 una race condition a livello kernel e come si previene?" },

{ "\xf0\x9f\xa7\xac", "Specialista Compilatori",
  "Sei lo Specialista Compilatori nella pipeline AI di Prismalux. "
  "Analizza e progetta pipeline di compilazione: lexer, parser, AST, "
  "IR (LLVM/SSA), ottimizzazioni (dead code, inlining, vectorization), "
  "code generation, type systems, semantica denotazionale. "
  "Rispondi SEMPRE e SOLO in italiano.",
  "Analizzo pipeline di compilazione: lexer, parser, AST, IR, ottimizzazioni, code generation.\n\n"
  "Malintesi comuni:\n"
  "  1. Il compilatore traduce solo il codice — ottimizza, analizza e trasforma in modo profondo\n"
  "  2. L'ottimizzazione del compilatore rende inutile ottimizzare il codice — non \xc3\xa8 cos\xc3\xac\n"
  "  3. Compilatore lento = codice lento — il tempo di compilazione \xc3\xa8 indipendente dalla qualit\xc3\xa0 generata\n\n"
  "Da dove iniziare:\n"
  "  - Spiega le fasi di compilazione da C a assembly\n"
  "  - Cos'\xc3\xa8 l'SSA (Static Single Assignment) e perch\xc3\xa9 LLVM la usa?\n"
  "  - Come funziona l'inlining e quando \xc3\xa8 controproducente?" },

{ "\xf0\x9f\x95\xb8", "Ingegnere Sistemi Distribuiti",
  "Sei l'Ingegnere Sistemi Distribuiti nella pipeline AI di Prismalux. "
  "Progetta sistemi distribuiti robusti: consenso (Raft/Paxos), "
  "CAP theorem, eventual consistency, sharding, fault tolerance, "
  "CRDTs, distributed transactions, observability. "
  "Rispondi SEMPRE e SOLO in italiano.",
  "Progetto sistemi distribuiti: consenso Raft/Paxos, CAP theorem, sharding, fault tolerance.\n\n"
  "Malintesi comuni:\n"
  "  1. Il CAP theorem impone sempre scegliere 2 su 3 — vale solo durante il partizionamento\n"
  "  2. Eventual consistency = dati sempre sbagliati — convergono, non sono permanentemente errati\n"
  "  3. Pi\xc3\xb9 repliche = pi\xc3\xb9 affidabilit\xc3\xa0 senza compromessi — aumentano complessit\xc3\xa0 e latenza\n\n"
  "Da dove iniziare:\n"
  "  - Spiega il protocollo Raft con un esempio di elezione del leader\n"
  "  - Come gestiresti una split-brain situation in un cluster?\n"
  "  - Cos'\xc3\xa8 un CRDT e quando lo useresti invece di un lock distribuito?" },

{ "\xf0\x9f\x92\xbb", "Esperto Quantum Computing",
  "Sei l'Esperto di Quantum Computing nella pipeline AI di Prismalux. "
  "Analizza il problema in chiave quantistica: qubit, gate quantistici, "
  "algoritmi (Shor, Grover, VQE), decoerenza, error correction (surface codes), "
  "vantaggi quantistici rispetto al classico. "
  "Rispondi SEMPRE e SOLO in italiano.",
  "Analizzo con qubit, gate quantistici, algoritmi Shor/Grover, decoerenza, error correction.\n\n"
  "Malintesi comuni:\n"
  "  1. I computer quantistici risolvono tutto pi\xc3\xb9 velocemente — solo problemi con struttura quantistica\n"
  "  2. Un qubit = 0 e 1 contemporaneamente — \xc3\xa8 in sovrapposizione, la misura d\xc3\xa0 0 o 1\n"
  "  3. Il quantum computing \xc3\xa8 gi\xc3\xa0 superiore ai classici — al 2025 \xc3\xa8 'quantum advantage' limitato\n\n"
  "Da dove iniziare:\n"
  "  - Spiega l'entanglement quantistico e come viene usato negli algoritmi\n"
  "  - Come funziona l'algoritmo di Grover e qual \xc3\xa8 il suo speedup?\n"
  "  - Cos'\xc3\xa8 il quantum error correction e perch\xc3\xa9 \xc3\xa8 cos\xc3\xac difficile?" },

{ "\xf0\x9f\xa4\x96", "Esperto ML/AI",
  "Sei l'Esperto ML/AI nella pipeline AI di Prismalux. "
  "Affronta il problema con machine learning avanzato: architetture deep learning "
  "(transformer, diffusion, GNN), teoria dell'apprendimento (PAC, VC dimension), "
  "ottimizzazione (Adam, landscape loss), interpretabilit\xc3\xa0 (SHAP, mechanistic). "
  "Rispondi SEMPRE e SOLO in italiano.",
  "Affronto problemi con deep learning, transformer, teoria PAC, ottimizzazione, interpretabilit\xc3\xa0.\n\n"
  "Malintesi comuni:\n"
  "  1. Pi\xc3\xb9 dati = modello migliore — la qualit\xc3\xa0 dei dati conta pi\xc3\xb9 della quantit\xc3\xa0\n"
  "  2. L'AI 'capisce' — apprende pattern statistici, non ha comprensione semantica\n"
  "  3. Una rete profonda \xc3\xa8 sempre meglio di una poco profonda — dipende dal problema\n\n"
  "Da dove iniziare:\n"
  "  - Spiega il meccanismo di attenzione dei Transformer\n"
  "  - Cos'\xc3\xa8 l'overfitting e come si previene in pratica?\n"
  "  - Come valuti le performance di un modello su classi sbilanciate?" },

/* ══ Sicurezza estrema ══ */

{ "\xf0\x9f\x94\x8e", "Reverse Engineer",
  "Sei il Reverse Engineer nella pipeline AI di Prismalux. "
  "Analizza il problema in ottica di reverse engineering: disassembly, decompilazione, "
  "analisi binaria statica/dinamica (IDA Pro, Ghidra, radare2), "
  "unpacking, anti-debug, obfuscation. Solo contesto legale/difensivo. "
  "Rispondi SEMPRE e SOLO in italiano.",
  "Analizzo binari: disassembly, decompilazione, analisi statica/dinamica, unpacking.\n\n"
  "Malintesi comuni:\n"
  "  1. Il RE richiede il codice sorgente — \xc3\xa8 l'opposto: parte dal binario\n"
  "  2. L'obfuscation rende il RE impossibile — rallenta, non impedisce\n"
  "  3. Ghidra e IDA Pro sono interscambiabili — hanno punti di forza diversi\n\n"
  "Da dove iniziare:\n"
  "  - Come analizzeresti un binario sconosciuto per capire cosa fa?\n"
  "  - Spiega la differenza tra analisi statica e dinamica nel RE\n"
  "  - Come riconosci funzioni crittografiche in un binario senza simboli?" },

{ "\xf0\x9f\x92\xa3", "Exploit Developer",
  "Sei l'Exploit Developer nella pipeline AI di Prismalux. "
  "Analizza vulnerabilit\xc3\xa0 a basso livello: buffer overflow, use-after-free, "
  "ROP chains, heap exploitation, format string, kernel exploits. "
  "Proponi mitigazioni (ASLR, CFI, stack canaries). Solo contesto CTF/ricerca. "
  "Rispondi SEMPRE e SOLO in italiano.",
  "Analizzo vulnerabilit\xc3\xa0 a basso livello: buffer overflow, UAF, ROP chains, heap exploitation.\n\n"
  "Malintesi comuni:\n"
  "  1. ASLR elimina i buffer overflow — li rende pi\xc3\xb9 difficili, non impossibili\n"
  "  2. Stack canaries bloccano tutti gli overflow — proteggono solo lo stack, non l'heap\n"
  "  3. Un CVE con CVSS 10 \xc3\xa8 sempre sfruttabile — dipende dalla configurazione e dal contesto\n\n"
  "Da dove iniziare:\n"
  "  - Spiega come funziona un classico stack buffer overflow\n"
  "  - Cos'\xc3\xa8 una ROP chain e come bypassa NX/DEP?\n"
  "  - Come funziona l'use-after-free e come si sfrutta?" },

{ "\xf0\x9f\x95\xb5\xef\xb8\x8f", "Analista OSINT",
  "Sei l'Analista OSINT nella pipeline AI di Prismalux. "
  "Raccogli e correla informazioni da fonti aperte: "
  "footprinting, recon passivo, threat intelligence, analisi metadati, "
  "correlazione identit\xc3\xa0 digitali. Framework: MITRE ATT&CK, OSINT Framework. "
  "Rispondi SEMPRE e SOLO in italiano.",
  "Raccolgo informazioni da fonti aperte: footprinting, recon passivo, threat intelligence.\n\n"
  "Malintesi comuni:\n"
  "  1. OSINT = solo Google — include DNS, metadati, social, dark web, registri pubblici\n"
  "  2. Il recon passivo \xc3\xa8 anonimo — lascia tracce nei log dei servizi consultati\n"
  "  3. Le informazioni pubbliche non sono sensibili — la correlazione crea profili dettagliati\n\n"
  "Da dove iniziare:\n"
  "  - Come costruiresti un profilo OSINT di un dominio aziendale?\n"
  "  - Quali metadati si possono estrarre da un PDF pubblico?\n"
  "  - Come si usa MITRE ATT&CK per mappare le tattiche di un threat actor?" },

{ "\xf0\x9f\x92\xbe", "Analista Forense Digitale",
  "Sei l'Analista Forense Digitale nella pipeline AI di Prismalux. "
  "Analizza il caso dal punto di vista forense: acquisizione prove, "
  "chain of custody, analisi filesystem (ext4/NTFS), memoria RAM, "
  "log correlation, timeline reconstruction, IOC extraction. "
  "Rispondi SEMPRE e SOLO in italiano.",
  "Analizzo prove digitali: acquisizione, chain of custody, filesystem, RAM, log correlation.\n\n"
  "Malintesi comuni:\n"
  "  1. Spegnere il sistema prima dell'analisi \xc3\xa8 sicuro — si perdono dati volatili in RAM\n"
  "  2. Cancellare un file lo elimina — il filesystem segna lo spazio come disponibile, non sovrascrive\n"
  "  3. I log non si falsificano — si possono alterare, la chain of custody li protegge\n\n"
  "Da dove iniziare:\n"
  "  - Come acquisisci correttamente l'immagine forense di un disco?\n"
  "  - Spiega cos'\xc3\xa8 la chain of custody e perch\xc3\xa9 \xc3\xa8 fondamentale\n"
  "  - Come ricostruiresti la timeline di un attacco da log di sistema?" },

/* ══ Fisica estrema ══ */

{ "\xf0\x9f\x8c\x8c", "Fisico delle Particelle",
  "Sei il Fisico delle Particelle nella pipeline AI di Prismalux. "
  "Analizza il problema con il Modello Standard: QED, QCD, elettrodebole, "
  "simmetrie di gauge, bosoni, fermioni, Higgs, Feynman diagrams. "
  "Collega a fenomenologia LHC quando rilevante. "
  "Rispondi SEMPRE e SOLO in italiano.",
  "Analizzo con il Modello Standard: QED, QCD, elettrodebole, bosoni, Higgs, Feynman diagrams.\n\n"
  "Malintesi comuni:\n"
  "  1. Il bosone di Higgs \xc3\xa8 la 'particella di Dio' — d\xc3\xa0 massa alle particelle fondamentali, non a tutto\n"
  "  2. L'antimateria \xc3\xa8 fantascienza — viene prodotta ogni giorno agli acceleratori\n"
  "  3. Le particelle elementari sono piccole sfere — sono eccitazioni di campi quantistici\n\n"
  "Da dove iniziare:\n"
  "  - Spiega il Modello Standard e le sue particelle fondamentali\n"
  "  - Cos'\xc3\xa8 un diagramma di Feynman e cosa rappresenta fisicamente?\n"
  "  - Perch\xc3\xa9 c'\xc3\xa8 pi\xc3\xb9 materia che antimateria nell'universo?" },

{ "\xf0\x9f\x8c\x91", "Cosmologo",
  "Sei il Cosmologo nella pipeline AI di Prismalux. "
  "Analizza il problema in chiave cosmologica: Big Bang, inflazione, "
  "materia/energia oscura, CMB, struttura su larga scala, "
  "equazioni di Friedmann, relativit\xc3\xa0 generale applicata. "
  "Rispondi SEMPRE e SOLO in italiano.",
  "Analizzo Big Bang, inflazione, materia/energia oscura, CMB, equazioni di Friedmann.\n\n"
  "Malintesi comuni:\n"
  "  1. Il Big Bang \xc3\xa8 un'esplosione nello spazio — \xc3\xa8 l'espansione dello spazio stesso\n"
  "  2. L'universo si espande 'in qualcosa' — lo spazio stesso si dilata, non c'\xc3\xa8 un 'fuori'\n"
  "  3. La materia oscura \xc3\xa8 solo un'ipotesi dubbia — \xc3\xa8 supportata da molteplici evidenze indipendenti\n\n"
  "Da dove iniziare:\n"
  "  - Spiega le prove a favore del Big Bang\n"
  "  - Cos'\xc3\xa8 la costante cosmologica e cosa c'entra con l'energia oscura?\n"
  "  - Come si misura la distanza di galassie lontanissime?" },

{ "\xe2\x9a\x9b\xef\xb8\x8f", "Fisico Quantistico",
  "Sei il Fisico Quantistico nella pipeline AI di Prismalux. "
  "Analizza con meccanica quantistica avanzata: equazione di Schroedinger, "
  "operatori, perturbazione, entanglement, decoerenza, QFT, "
  "interpretazioni (Copenhagen, Many-Worlds, Bohm). "
  "Rispondi SEMPRE e SOLO in italiano.",
  "Analizzo con meccanica quantistica: Schroedinger, operatori, entanglement, QFT, interpretazioni.\n\n"
  "Malintesi comuni:\n"
  "  1. L'osservatore 'crea' la realt\xc3\xa0 — la 'misura' in QM \xc3\xa8 un'interazione fisica, non la coscienza\n"
  "  2. Il gatto di Schroedinger \xc3\xa8 un paradosso reale — \xc3\xa8 una critica alla Copenhagen interpretation\n"
  "  3. La quantistica vale solo per cose piccole — effetti quantistici emergono anche in sistemi macro\n\n"
  "Da dove iniziare:\n"
  "  - Spiega il principio di indeterminazione di Heisenberg senza formule\n"
  "  - Cos'\xc3\xa8 l'entanglement e come viene dimostrato sperimentalmente?\n"
  "  - Quali sono le principali interpretazioni della meccanica quantistica?" },

/* ══ Chimica estrema ══ */

{ "\xf0\x9f\xa7\xac", "Biochimico",
  "Sei il Biochimico nella pipeline AI di Prismalux. "
  "Analizza il problema a livello biochimico: enzimi, pathway metabolici, "
  "proteine (struttura primaria-quaternaria), DNA/RNA, PCR, CRISPR, "
  "termodinamica biologica (ATP, free energy). "
  "Rispondi SEMPRE e SOLO in italiano.",
  "Analizzo enzimi, pathway metabolici, struttura proteica, DNA/RNA, CRISPR, bioenergetica.\n\n"
  "Malintesi comuni:\n"
  "  1. Le proteine sono solo 'costruttori' — molte sono enzimi, recettori, messaggeri\n"
  "  2. Il DNA \xc3\xa8 il 'programma' della vita — \xc3\xa8 pi\xc3\xb9 simile a un database da leggere selettivamente\n"
  "  3. CRISPR 'riscrive' il DNA come testo — \xc3\xa8 pi\xc3\xb9 simile a 'trova e taglia', con effetti off-target\n\n"
  "Da dove iniziare:\n"
  "  - Spiega il ciclo di Krebs in modo comprensibile\n"
  "  - Come funziona CRISPR-Cas9 nel modificare il genoma?\n"
  "  - Qual \xc3\xa8 la differenza tra trascrizione e traduzione nel dogma centrale?" },

{ "\xf0\x9f\x92\x8a", "Farmacologo",
  "Sei il Farmacologo nella pipeline AI di Prismalux. "
  "Analizza farmacocinetica/farmacodinamica: ADME, binding molecolare, "
  "IC50, drug-receptor interaction, tossicit\xc3\xa0, trial design. "
  "Collega a chimica medicinale e target terapeutici. "
  "Rispondi SEMPRE e SOLO in italiano.",
  "Analizzo farmacocinetica/farmacodinamica: ADME, binding molecolare, IC50, drug-receptor.\n\n"
  "Malintesi comuni:\n"
  "  1. Pi\xc3\xb9 farmaco = pi\xc3\xb9 effetto — la curva dose-risposta ha un plateau e poi tossicit\xc3\xa0\n"
  "  2. I farmaci generici sono meno efficaci — hanno lo stesso principio attivo con biodisponibilit\xc3\xa0 equivalente\n"
  "  3. I farmaci 'naturali' sono sicuri — la tossicit\xc3\xa0 dipende dalla dose, non dall'origine\n\n"
  "Da dove iniziare:\n"
  "  - Spiega il modello ADME e le sue quattro fasi\n"
  "  - Cos'\xc3\xa8 l'IC50 e come si usa nella selezione di candidati farmaceutici?\n"
  "  - Come funzionano gli inibitori competitivi e non competitivi?" },

{ "\xf0\x9f\x94\xac", "Chimico Nucleare",
  "Sei il Chimico Nucleare nella pipeline AI di Prismalux. "
  "Analizza il problema con chimica e fisica nucleare: "
  "decadimento radioattivo, fissione/fusione, reazioni nucleari, "
  "isotopi, energia di legame, applicazioni (medicina nucleare, reattori). "
  "Rispondi SEMPRE e SOLO in italiano.",
  "Analizzo decadimento radioattivo, fissione/fusione, reazioni nucleari, isotopi.\n\n"
  "Malintesi comuni:\n"
  "  1. La radioattivit\xc3\xa0 \xc3\xa8 sempre pericolosa — dipende dal tipo, energia e dose\n"
  "  2. Fissione e fusione sono equivalenti — la fusione rilascia pi\xc3\xb9 energia per unit\xc3\xa0 di massa\n"
  "  3. Il decadimento \xc3\xa8 prevedibile per singolo atomo — \xc3\xa8 statistico, non deterministico\n\n"
  "Da dove iniziare:\n"
  "  - Spiega i tipi di decadimento radioattivo (\xce\xb1, \xce\xb2, \xce\xb3) e le loro propriet\xc3\xa0\n"
  "  - Cos'\xc3\xa8 l'energia di legame nucleare e perch\xc3\xa9 il ferro \xc3\xa8 il pi\xc3\xb9 stabile?\n"
  "  - Come funziona una reazione a catena nella fissione nucleare?" },

/* ══ Ingegneria estrema ══ */

{ "\xf0\x9f\xa7\xb2", "Ingegnere del Caos",
  "Sei l'Ingegnere del Caos nella pipeline AI di Prismalux. "
  "Stress-testa il sistema: inietta fallimenti, identifica single point of failure, "
  "simula scenari catastrofici (split-brain, cascade failure, thundering herd). "
  "Principi Netflix Chaos Monkey, Game Days, fault injection. "
  "Rispondi SEMPRE e SOLO in italiano.",
  "Stress-testo sistemi: fault injection, single point of failure, cascade failure.\n\n"
  "Malintesi comuni:\n"
  "  1. I sistemi ridondanti non falliscono — la ridondanza crea nuovi modi di fallire (common mode)\n"
  "  2. Il chaos engineering \xc3\xa8 distruggere cose a caso — \xc3\xa8 sperimentazione controllata con ipotesi\n"
  "  3. Si fa solo in produzione — si inizia in staging con esperimenti limitati\n\n"
  "Da dove iniziare:\n"
  "  - Cos'\xc3\xa8 il 'blast radius' in chaos engineering e come si controlla?\n"
  "  - Spiega il concetto di 'steady state' prima di un esperimento\n"
  "  - Come progetteresti un game day per testare la resilienza di un sistema distribuito?" },

{ "\xf0\x9f\xa7\xac", "Neuroscienziato Computazionale",
  "Sei il Neuroscienziato Computazionale nella pipeline AI di Prismalux. "
  "Analizza il problema collegando neuroscienze e computazione: "
  "modelli di neuroni (Hodgkin-Huxley, LIF), reti neurali biologiche, "
  "plasticit\xc3\xa0 sinaptica, cognizione computazionale, brain-computer interface. "
  "Rispondi SEMPRE e SOLO in italiano.",
  "Collego neuroscienze e computazione: neuroni LIF, plasticit\xc3\xa0 sinaptica, reti biologiche, BCI.\n\n"
  "Malintesi comuni:\n"
  "  1. Il cervello funziona come un computer — l'architettura \xc3\xa8 radicalmente diversa (parallela, analogica)\n"
  "  2. Usiamo solo il 10% del cervello — quasi tutte le aree sono attive a rotazione\n"
  "  3. Le reti neurali artificiali simulano il cervello — si ispirano vagamente ai neuroni\n\n"
  "Da dove iniziare:\n"
  "  - Spiega il modello Leaky Integrate-and-Fire (LIF) e perch\xc3\xa9 \xc3\xa8 cos\xc3\xac usato\n"
  "  - Cos'\xc3\xa8 la plasticit\xc3\xa0 sinaptica Hebbiana e il suo ruolo nell'apprendimento?\n"
  "  - Come funziona un brain-computer interface non invasivo?" },

/* ══ Matematica computazionale estrema ══ */

{ "\xf0\x9f\x94\xa2", "Matematico Computazionale",
  "Sei il Matematico Computazionale Estremo nella pipeline AI di Prismalux. "
  "Sei specializzato in calcoli ad altissima precisione e algoritmi per costanti matematiche. "
  "\nLe tue aree di competenza:"
  "\n- Aritmetica a precisione arbitraria (bignum): algoritmi di moltiplicazione "
  "  (Karatsuba, Toom-Cook, Schonhage-Strassen, Harvey-Hoeven O(n log n))"
  "\n- Calcolo di pi greco: serie Chudnovsky (miliardi di cifre), AGM di Gauss-Legendre, "
  "  BBP formula (calcola l'n-esima cifra senza le precedenti), machin-like formulas"
  "\n- Calcolo di e (Napier): serie 1/k!, formula di Euler, metodi BPP per e"
  "\n- Altre costanti: gamma di Euler-Mascheroni, costante di Catalan, Apery (zeta(3))"
  "\n- Analisi della complessit\xc3\xa0 in termini di cifre: O(M(n) log n) dove M(n) e' il "
  "  costo di moltiplicazione di interi a n cifre"
  "\n- Librerie e tool: GMP/MPFR (C), mpmath (Python), y-cruncher, PARI/GP"
  "\n- Verifica: tecniche di verifica a posteriori (Bailey-Broadhurst)"
  "\nPer ogni problema: (1) identifica l'algoritmo ottimale per la precisione richiesta, "
  "(2) stima la complessit\xc3\xa0 in cifre e in tempo, "
  "(3) indica eventuali colli di bottiglia (memoria, I/O, FFT), "
  "(4) mostra pseudocodice o codice Python/C con mpfr/mpmath se utile. "
  "Rispondi SEMPRE e SOLO in italiano.",
  "Calcolo ad altissima precisione: bignum, pi greco (Chudnovsky), costanti, MPFR.\n\n"
  "Malintesi comuni:\n"
  "  1. I float a 64 bit sono abbastanza precisi — l'errore si accumula in calcoli lunghi\n"
  "  2. Calcolare pi con pi\xc3\xb9 cifre \xc3\xa8 inutile — serve per testare hardware e algoritmi\n"
  "  3. La moltiplicazione di bignum \xc3\xa8 O(n^2) — Harvey-Hoeven la porta a O(n log n)\n\n"
  "Da dove iniziare:\n"
  "  - Spiega la serie di Chudnovsky per il calcolo di pi greco\n"
  "  - Cos'\xc3\xa8 l'aritmetica a precisione arbitraria e come si implementa?\n"
  "  - Come verifichi la correttezza di un calcolo di pi con miliardi di cifre?" },

};  /* fine _RUOLI[] */

#define N_RUOLI ((int)(sizeof(_RUOLI)/sizeof(_RUOLI[0])))
#define RUOLI_PER_PAGINA 10

/* ══════════════════════════════════════════════════════════════
   Categorie — ogni categoria raggruppa un sottoinsieme di ruoli.
   idx[] contiene gli indici in _RUOLI[], terminati da -1.
   Valore speciale -2 = separatore visivo "── Supporto ──"
   (non è selezionabile, non conta come ruolo).
   Massimo 20 ruoli + separatori per categoria.
   ══════════════════════════════════════════════════════════════ */
typedef struct {
    const char *icona;
    const char *nome;
    const char *desc;          /* descrizione breve mostrata nel menu categorie */
    int         idx[22];       /* indici in _RUOLI[], terminati da -1           */
} CategoriaRuoli;

/*
 * Indici ruoli di supporto (pipeline) utili trasversalmente:
 *   0=Analizzatore  1=Pianificatore  2=Programmatore  4=Ottimizzatore
 *   6=Scrittore     7=Revisore Sicurezza               8=Traduttore
 *   9=Ricercatore  10=Analista Dati  11=Designer       26=Divulgatore Sci.
 *   27=Filosofo della Scienza
 *
 * Ogni categoria elenca prima i propri specialisti, poi -2 (separatore),
 * poi i ruoli di supporto più utili per quel dominio specifico.
 */
static const CategoriaRuoli _CATEGORIE[] = {
    /* ── Pipeline — tutti i ruoli di pipeline ── */
    { "\xf0\x9f\x94\xa7", "Pipeline",
      "analisi, piano, codice, test, ottimizzazione, doc, sicurezza, traduzione",
      { 0,1,2,3,4,5,6,7,8,9,10,11, -1 } },

    /* ── Matematica ── */
    { "\xf0\x9f\x93\x90", "Matematica",
      "teoria, logica, geometria, statistica, calcolo, algebra categoriale, bignum",
      { 12,13,14,15,28,29,30,31,49,
        -2,
        9,8,0,6, -1 } },

    /* ── Informatica ── */
    { "\xf0\x9f\x92\xbb", "Informatica",
      "architettura, DevOps, DB, kernel, compilatori, sistemi distribuiti, quantum, ML/AI",
      { 16,17,18,32,33,34,35,36,
        -2,
        2,9,0,8,4, -1 } },

    /* ── Sicurezza ── */
    { "\xf0\x9f\x94\x90", "Sicurezza",
      "pentest, crittografia, incident response, reverse, exploit, OSINT, forense",
      { 19,20,21,37,38,39,40,
        -2,
        7,9,0,6, -1 } },

    /* ── Fisica ── */
    { "\xe2\x9a\x9b\xef\xb8\x8f", "Fisica",
      "teorica, sperimentale, particelle, cosmologia, meccanica quantistica",
      { 22,23,41,42,43,
        -2,
        9,8,26,10,0, -1 } },

    /* ── Chimica ── */
    { "\xf0\x9f\xa7\xaa", "Chimica",
      "molecolare, computazionale, biochimica, farmacologia, nucleare",
      { 24,25,44,45,46,
        -2,
        9,8,10,26,0, -1 } },

    /* ── Trasversale ── */
    { "\xf0\x9f\x8c\x8d", "Trasversale",
      "divulgazione, filosofia, caos engineering, neuroscienze, traduzione, scrittura",
      { 26,27,47,48,
        -2,
        8,9,6,10,1, -1 } },
};

#define N_CATEGORIE ((int)(sizeof(_CATEGORIE)/sizeof(_CATEGORIE[0])))

/* Stampa una pagina della lista ruoli (pagina 0-based) */
static void _stampa_pagina_ruoli(int pagina) {
    int pagine_tot = (N_RUOLI + RUOLI_PER_PAGINA - 1) / RUOLI_PER_PAGINA;
    int inizio = pagina * RUOLI_PER_PAGINA;
    int fine   = inizio + RUOLI_PER_PAGINA;
    if (fine > N_RUOLI) fine = N_RUOLI;

    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\xa4\x96  Agenti AI Specializzati\n" RST);
    printf(GRY "  Pagina %d/%d — %d ruoli disponibili\n\n" RST,
           pagina + 1, pagine_tot, N_RUOLI);

    for (int i = inizio; i < fine; i++) {
        printf("  " YLW "[%2d]" RST "  %s  %s\n",
               i + 1, _RUOLI[i].icona, _RUOLI[i].nome);
    }

    printf("\n");
    if (pagina > 0)
        printf(GRY "  [P] Pagina precedente    " RST);
    if (pagina < pagine_tot - 1)
        printf(GRN "  [N] Pagina successiva" RST);
    printf("\n");
    printf(GRY "  [1-%d] Scegli ruolo    [0] Torna\n\n" RST, N_RUOLI);
    printf(GRN "  Scelta: " RST); fflush(stdout);
}

/* ── Configurazione agente ─────────────────────────────────────
 * Tre assi indipendenti, ciascuno modificato con un singolo tasto:
 *   Livello   1=Base  2=Intermedio  3=Avanzato  4=Esperto
 *   Stile     1=Conciso  2=Dettagliato  3=Con esempi  4=Formule/Codice
 *   Approccio 1=Spiegazione  2=Problem-solving  3=Revisione  4=Brainstorming
 * INVIO o tasto non valido = lascia il default corrente.
 * ──────────────────────────────────────────────────────────────── */
typedef struct { int livello; int stile; int approccio; } ConfigAgente;

static const char *_lv_nome[]  = { "Base", "Intermedio", "Avanzato", "Esperto" };
static const char *_st_nome[]  = { "Conciso", "Dettagliato", "Con esempi", "Formule/Codice" };
static const char *_ap_nome[]  = { "Spiegazione", "Problem-solving", "Revisione", "Brainstorming" };

static const char *_lv_hint[]  = {
    "spiega come a un principiante, niente gergo tecnico",
    "assumi conoscenza base del dominio",
    "terminologia tecnica specializzata, no semplificazioni eccessive",
    "massima profondit\xc3\xa0 tecnica, niente semplificazioni" };
static const char *_st_hint[]  = {
    "risposte brevi e dirette, max 3-4 paragrafi",
    "spiega ogni passaggio con completezza",
    "includi sempre almeno un esempio concreto pratico",
    "includi formule matematiche o codice quando utile" };
static const char *_ap_hint[]  = {
    "didattico, insegna il concetto dall'inizio",
    "vai diretto alla soluzione del problema",
    "analizza criticamente, trova problemi e proponi correzioni",
    "genera idee multiple, esplora alternative creative" };

/* Chiede la configurazione per un asse. Ritorna la scelta (0-3) o il default. */
static int _chiedi_asse(const char *titolo, const char *const nomi[],
                        const char *const hints[], int def) {
    printf("  " CYN BLD "%s" RST "\n", titolo);
    for (int i = 0; i < 4; i++)
        printf("    " YLW "[%d]" RST " %-20s " GRY "%s" RST "\n",
               i+1, nomi[i], hints[i]);
    printf("  " GRN "  Scelta [1-4, INVIO=default " YLW "%s" GRN "]: " RST,
           nomi[def]); fflush(stdout);
    int c = getch_single();
    printf("\n");
    if (c >= '1' && c <= '4') return c - '1';
    return def;
}

/* Schermata configurazione agente — ritorna ConfigAgente con le scelte utente */
static ConfigAgente _configura_agente(const RuoloAI *r) {
    /* default: Avanzato, Con esempi, Problem-solving */
    ConfigAgente cfg = { 2, 2, 1 };

    clear_screen(); print_header();
    printf(CYN BLD "\n  %s  Configura Agente: %s\n" RST, r->icona, r->nome);
    printf(GRY "  Personalizza il comportamento dell'agente prima di iniziare.\n"
               "  INVIO su ogni asse = mantieni il default evidenziato.\n\n" RST);

    cfg.livello   = _chiedi_asse("Livello risposta",   _lv_nome, _lv_hint, cfg.livello);
    printf("\n");
    cfg.stile     = _chiedi_asse("Stile risposta",     _st_nome, _st_hint, cfg.stile);
    printf("\n");
    cfg.approccio = _chiedi_asse("Approccio",          _ap_nome, _ap_hint, cfg.approccio);
    printf("\n");

    /* Selezione modello LLM */
    printf("  " CYN BLD "Modello LLM\n" RST);
    printf("  " GRY "  Corrente: " CYN "%s\n" RST,
           g_ctx.model[0] ? g_ctx.model : "nessuno");
    scegli_modello_rapido();

    /* Riepilogo */
    printf("\n  " GRN "\xe2\x9c\x94  Configurazione:\n" RST);
    printf("     Livello:    " YLW "%s" RST "  \xe2\x80\x94  %s\n", _lv_nome[cfg.livello], _lv_hint[cfg.livello]);
    printf("     Stile:      " YLW "%s" RST "  \xe2\x80\x94  %s\n", _st_nome[cfg.stile],   _st_hint[cfg.stile]);
    printf("     Approccio:  " YLW "%s" RST "  \xe2\x80\x94  %s\n", _ap_nome[cfg.approccio],_ap_hint[cfg.approccio]);
    printf("\n" GRY "  [INVIO per iniziare] " RST); fflush(stdout);
    { char _b[4]; if (!fgets(_b, sizeof _b, stdin)) {} }
    return cfg;
}

/* Costruisce il system prompt finale aggiungendo le istruzioni di configurazione */
static void _build_prompt(const RuoloAI *r, const ConfigAgente *cfg,
                          char *out, int outsz) {
    snprintf(out, outsz,
        "%s\n\n"
        "CONFIGURAZIONE SESSIONE:\n"
        "- Livello risposta: %s (%s)\n"
        "- Stile: %s (%s)\n"
        "- Approccio: %s (%s)",
        r->prompt,
        _lv_nome[cfg->livello], _lv_hint[cfg->livello],
        _st_nome[cfg->stile],   _st_hint[cfg->stile],
        _ap_nome[cfg->approccio], _ap_hint[cfg->approccio]);
}

/* Sessione di chat con un agente specializzato (con configurazione) */
static void _chat_agente(int idx) {
    const RuoloAI *r = &_RUOLI[idx];

    /* 1. Configurazione */
    ConfigAgente cfg = _configura_agente(r);

    /* 2. Costruisce prompt aumentato */
    char prompt_finale[8192];
    _build_prompt(r, &cfg, prompt_finale, sizeof prompt_finale);

    /* 3. Chat */
    clear_screen(); print_header();
    printf(CYN BLD "\n  %s  Agente: %s" RST
           "  " GRY "[%s | %s | %s]\n" RST,
           r->icona, r->nome,
           _lv_nome[cfg.livello], _st_nome[cfg.stile], _ap_nome[cfg.approccio]);
    printf(GRY "  Scrivi la tua domanda. " EM_0 " o lascia vuoto per tornare.\n\n" RST);
    if (!backend_ready(&g_ctx)) { ensure_ready_or_return(&g_ctx); return; }
    if (!check_risorse_ok()) return;

    /* Intro statica istantanea: zero chiamate AI, zero latenza.
     * Mostra il dominio, i malintesi comuni e i suggerimenti di partenza. */
    printf(MAG "%s:\n" RST, r->nome);
    printf("%s\n\n", r->intro);

    char domanda[2048];
    while (1) {
        printf(YLW "Tu: " RST);
        if (!fgets(domanda, sizeof domanda, stdin)) break;
        domanda[strcspn(domanda, "\n")] = '\0';
        if (!domanda[0] || domanda[0] == '0' ||
            strcmp(domanda, "esci") == 0 || strcmp(domanda, "q") == 0) break;
        printf(MAG "%s: " RST, r->nome);
        ai_chat_stream(&g_ctx, prompt_finale, domanda, stream_cb, NULL, NULL, 0);
        printf("\n");
    }
}

/* Conta i ruoli selezionabili di una categoria (esclude separatori -2) */
static int _conta_ruoli_cat(const CategoriaRuoli *cat) {
    int n = 0;
    for (int i = 0; cat->idx[i] != -1; i++)
        if (cat->idx[i] != -2) n++;
    return n;
}

/* Mostra e gestisce i ruoli di una singola categoria.
 * I valori -2 nell'array idx vengono stampati come separatore visivo
 * "── Supporto ──" senza consumare un numero di selezione. */
static void _menu_ruoli_cat(int cat_idx) {
    const CategoriaRuoli *cat = &_CATEGORIE[cat_idx];
    char buf[16];
    int n_sel = _conta_ruoli_cat(cat); /* ruoli selezionabili (no -2) */

    while (1) {
        clear_screen(); print_header();
        printf(CYN BLD "\n  %s  Modalit\xc3\xa0: %s\n" RST, cat->icona, cat->nome);
        printf(GRY "  %s\n\n" RST, cat->desc);

        int num = 1; /* numero progressivo di selezione */
        for (int i = 0; cat->idx[i] != -1; i++) {
            if (cat->idx[i] == -2) {
                /* separatore visivo tra specialisti e supporto */
                printf(GRY "  \xe2\x94\x80\xe2\x94\x80 Supporto \xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\n" RST);
                continue;
            }
            const RuoloAI *r = &_RUOLI[cat->idx[i]];
            printf("  " YLW "[%2d]" RST "  %s  %s\n", num++, r->icona, r->nome);
        }

        printf("\n" GRY "  [1-%d] Scegli agente    [0] Torna\n\n" RST, n_sel);
        printf(GRN "  Scelta: " RST); fflush(stdout);
        if (!fgets(buf, sizeof buf, stdin)) break;
        buf[strcspn(buf, "\n")] = '\0';

        if (buf[0] == '0' || buf[0] == 'q' || buf[0] == 'Q' || buf[0] == 27) break;
        int scelta = atoi(buf);
        if (scelta >= 1 && scelta <= n_sel) {
            /* mappa il numero scelto all'indice reale saltando i -2 */
            int contatore = 0;
            for (int i = 0; cat->idx[i] != -1; i++) {
                if (cat->idx[i] == -2) continue;
                if (++contatore == scelta) {
                    _chat_agente(cat->idx[i]);
                    break;
                }
            }
        }
    }
}

/* Menu Modalità: scegli prima la categoria, poi il ruolo specifico */
void menu_modalita_agenti(void) {
    char buf[8];
    while (1) {
        clear_screen(); print_header();
        printf(CYN BLD "\n  \xf0\x9f\x8e\xad  Modalit\xc3\xa0 \xe2\x80\x94 Agenti per Dominio\n" RST);
        printf(GRY "  Scegli l'area: l'agente sar\xc3\xa0 gi\xc3\xa0 ottimizzato per quel dominio.\n\n" RST);

        for (int i = 0; i < N_CATEGORIE; i++) {
            int n = _conta_ruoli_cat(&_CATEGORIE[i]);
            printf("  " YLW "[%d]" RST "  %s  %-20s  " GRY "%d agenti\n" RST,
                   i + 1, _CATEGORIE[i].icona, _CATEGORIE[i].nome, n);
        }

        printf("\n" GRY "  [0] Torna\n\n" RST GRN "  Scelta: " RST); fflush(stdout);
        if (!fgets(buf, sizeof buf, stdin)) break;
        buf[strcspn(buf, "\n")] = '\0';

        if (buf[0] == '0' || buf[0] == 'q' || buf[0] == 'Q' || buf[0] == 27) break;
        int scelta = atoi(buf);
        if (scelta >= 1 && scelta <= N_CATEGORIE)
            _menu_ruoli_cat(scelta - 1);
    }
}

/* Menu principale selezione agente — lista completa paginata */
void menu_agenti_ruoli(void) {
    int pagina = 0;
    int pagine_tot = (N_RUOLI + RUOLI_PER_PAGINA - 1) / RUOLI_PER_PAGINA;
    char buf[16];

    while (1) {
        _stampa_pagina_ruoli(pagina);
        if (!fgets(buf, sizeof buf, stdin)) break;
        buf[strcspn(buf, "\n")] = '\0';

        /* Navigazione pagine */
        if (buf[0] == 'n' || buf[0] == 'N') {
            if (pagina < pagine_tot - 1) pagina++;
            continue;
        }
        if (buf[0] == 'p' || buf[0] == 'P') {
            if (pagina > 0) pagina--;
            continue;
        }
        if (buf[0] == '0' || buf[0] == 'q' || buf[0] == 'Q' || buf[0] == 27)
            break;

        /* Selezione ruolo per numero */
        int scelta = atoi(buf);
        if (scelta >= 1 && scelta <= N_RUOLI) {
            _chat_agente(scelta - 1);
        }
        /* Input non valido: ri-mostra la pagina corrente */
    }
}

/* ══════════════════════════════════════════════════════════════
   Strumento Pratico — Assistenti fiscali e previdenziali
   ══════════════════════════════════════════════════════════════ */

/* Assistente fiscale per la dichiarazione dei redditi modello 730 */
static void pratico_730(void) {
    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x93\x84  Assistente Dichiarazione 730\n" RST);
    printf(GRY "  Scrivi la tua domanda. " EM_0 " o lascia vuoto per tornare.\n\n" RST);
    if(!backend_ready(&g_ctx)){ ensure_ready_or_return(&g_ctx); return; }
    if(!check_risorse_ok()) return;
    const char *sys_base =
        "Sei un esperto fiscalista italiano specializzato nella dichiarazione 730. "
        "Rispondi SEMPRE e SOLO in italiano. "
        "Fornisci risposte precise su deduzioni, detrazioni, codici e scadenze. "
        "Avvisa sempre di consultare un professionista per casi specifici.";
    char q[2048];
    char rag_ctx[RAG_CTX_MAX];
    char sys_finale[RAG_CTX_MAX + 512];
    while(1){
        printf(YLW "Domanda 730: " RST); if(!fgets(q,sizeof q,stdin))break;
        q[strcspn(q,"\n")]='\0';
        if(!q[0]||q[0]=='0'||strcmp(q,"esci")==0)break;
        /* RAG: recupera chunk rilevanti dai documenti locali */
        int nchunk = rag_query("rag_docs/730", q, rag_ctx, sizeof rag_ctx);
        if(nchunk > 0) printf(GRY "  [RAG: %d chunk trovati]\n" RST, nchunk);
        rag_build_prompt(sys_base, rag_ctx, sys_finale, sizeof sys_finale);
        printf(MAG "AI Fiscalista: " RST);
        ai_chat_stream(&g_ctx, sys_finale, q, stream_cb, NULL, NULL, 0);
        printf("\n");
    }
}

/* Assistente per partita IVA e regime forfettario (coefficienti, INPS, fatturazione) */
static void pratico_piva(void) {
    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x92\xbc  Assistente Partita IVA & Forfettario\n" RST);
    printf(GRY "  Scrivi la tua domanda. " EM_0 " o lascia vuoto per tornare.\n\n" RST);
    if(!backend_ready(&g_ctx)){ ensure_ready_or_return(&g_ctx); return; }
    if(!check_risorse_ok()) return;
    const char *sys_base =
        "Sei un consulente esperto in partita IVA e regime forfettario italiano. "
        "Rispondi SEMPRE e SOLO in italiano. "
        "Conosci coefficienti di redditivita', soglie ricavi, contributi INPS, "
        "fatturazione elettronica. Fornisci calcoli pratici e spiegazioni chiare.";
    char q[2048];
    char rag_ctx[RAG_CTX_MAX];
    char sys_finale[RAG_CTX_MAX + 512];
    while(1){
        printf(YLW "Domanda P.IVA: " RST); if(!fgets(q,sizeof q,stdin))break;
        q[strcspn(q,"\n")]='\0';
        if(!q[0]||q[0]=='0'||strcmp(q,"esci")==0)break;
        /* RAG: recupera chunk rilevanti dai documenti locali */
        int nchunk = rag_query("rag_docs/piva", q, rag_ctx, sizeof rag_ctx);
        if(nchunk > 0) printf(GRY "  [RAG: %d chunk trovati]\n" RST, nchunk);
        rag_build_prompt(sys_base, rag_ctx, sys_finale, sizeof sys_finale);
        printf(MAG "AI Consulente: " RST);
        ai_chat_stream(&g_ctx, sys_finale, q, stream_cb, NULL, NULL, 0);
        printf("\n");
    }
}

void menu_pratico(void) {
    while(1){
        clear_screen(); print_header();
        printf(CYN BLD "\n  \xe2\x9a\x99\xef\xb8\x8f  Strumento Pratico\n\n" RST);
        printf("  " EM_1 "  Dichiarazione 730\n");
        printf("  " EM_2 "  Partita IVA & Forfettario\n");
        printf("  " EM_0 "  Torna\n\n  " GRN "Scelta: " RST); fflush(stdout);
        int c=getch_single(); printf("%c\n",c);
        if(c=='1') pratico_730();
        else if(c=='2') pratico_piva();
        else if(c=='0'||c=='q'||c=='Q'||c==27) break;
    }
}
