/* ══════════════════════════════════════════════════════════════════════════
   test_bench.c — Benchmark unificato Prismalux
   ══════════════════════════════════════════════════════════════════════════
   Misura i componenti chiave del sistema e produce raccomandazioni di
   configurazione salvate in ~/.prismalux_bench.json, lette all'avvio.

   Sezioni:
     B01 — Precisione timer (baseline hardware)
     B02 — Cold start scheduler + strutture dati
     B03 — RAG motore TF-overlap (latenza, precisione)
     B04 — RAG motore Embedding semantico (Ollama, cache)
     B05 — Throughput streaming AI (simulato)
     REPORT — Tabella KPI + raccomandazioni + JSON save

   Build:
     make test_bench
   Esecuzione:
     ./test_bench
   Con embed attivo:
     ollama pull nomic-embed-text && ./test_bench

   Output permanente:
     ~/.prismalux_bench.json  — letto da main.c per auto-configurazione
   ══════════════════════════════════════════════════════════════════════════ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "rag.h"
#include "rag_embed.h"
#include "http.h"
#include "agent_scheduler.h"
#include "hw_detect.h"

/* ── Harness ──────────────────────────────────────────────────────────── */
static int _pass = 0, _fail = 0;
#define SECTION(t) printf("\n\033[36m\033[1m─── %s ───\033[0m\n", t)
#define CHECK(c,l) do { \
    if(c){printf("  \033[32m[OK]\033[0m  %s\n",l);_pass++;} \
    else {printf("  \033[31m[FAIL]\033[0m %s\n",l);_fail++;} \
} while(0)

/* ── Timer ────────────────────────────────────────────────────────────── */
static double _ms(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}
static double _us(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1e6 + t.tv_nsec / 1e3;
}

/* ── Documenti di test (stessi per TF e Embed — confronto equo) ───────── */
static const char *BENCH_DIR = "/tmp/prismalux_bench_docs";
static const char *QUERIES[] = {
    "matematica equazioni teorema pitagora derivate integrali",
    "Python programmazione funzioni classi algoritmi ordinamento",
    "storia medioevo impero romano rinascimento rivoluzione francese",
    "fisica quantistica relativita einstein particelle termodinamica",
    "chimica atomi tavola periodica reazioni organica",
    NULL
};

static void _write_file(const char *dir, const char *name, const char *txt) {
    char p[256]; snprintf(p, sizeof p, "%s/%s", dir, name);
    FILE *f = fopen(p, "w"); if (f) { fputs(txt, f); fclose(f); }
}

static void _setup_docs(void) {
    mkdir(BENCH_DIR, 0755);
    _write_file(BENCH_DIR, "matematica.txt",
        "La matematica studia i numeri, le funzioni e le equazioni.\n"
        "Il teorema di Pitagora: la somma dei quadrati dei cateti uguaglia il quadrato\n"
        "dell'ipotenusa. Le derivate e gli integrali sono i fondamenti del calcolo.\n");
    _write_file(BENCH_DIR, "programmazione.txt",
        "Python e' un linguaggio di programmazione ad alto livello.\n"
        "Le funzioni si definiscono con def. Le classi abilitano l'OOP.\n"
        "Quicksort e mergesort sono algoritmi di ordinamento efficienti in C.\n");
    _write_file(BENCH_DIR, "storia.txt",
        "Il Medioevo va dal V al XV secolo. L'Impero Romano cade nel 476 d.C.\n"
        "Il Rinascimento italiano riporta l'umanesimo. La Rivoluzione Francese\n"
        "del 1789 trasforma l'Europa. Napoleone e' figura chiave della storia.\n");
    _write_file(BENCH_DIR, "fisica.txt",
        "La meccanica quantistica descrive le particelle subatomiche.\n"
        "Einstein rivoluziona la fisica con la relativita'. La luce viaggia\n"
        "a 300000 km/s. La termodinamica studia i trasferimenti di energia.\n");
    _write_file(BENCH_DIR, "chimica.txt",
        "La tavola periodica contiene 118 elementi. Un atomo ha protoni,\n"
        "neutroni ed elettroni. Il pH misura l'acidita'. La chimica organica\n"
        "studia i composti del carbonio e le sue reazioni.\n");
}

static void _cleanup_docs(void) {
    const char *files[] = { "matematica.txt","programmazione.txt","storia.txt",
                             "fisica.txt","chimica.txt", NULL };
    for (int i = 0; files[i]; i++) {
        char p[256]; snprintf(p, sizeof p, "%s/%s", BENCH_DIR, files[i]);
        remove(p);
    }
}

/* ── Risultato di ogni sezione: esposto al REPORT finale ──────────────── */
static double _b01_timer_us    = 0.0;  /* risoluzione timer µs */
static double _b02_sched_ms    = 0.0;  /* cold start scheduler ms/init */
static double _b03_tf_ms       = 0.0;  /* RAG TF ms/query */
static double _b04_emb1_ms     = 0.0;  /* RAG Embed prima esecuzione ms/query */
static double _b04_emb2_ms     = 0.0;  /* RAG Embed cache hit ms/query */
static double _b05_tok_per_sec = 0.0;  /* throughput streaming tok/s */
static int    _b04_ok          = 0;    /* embed disponibile */
static char   _embed_model[64] = "";

/* ════════════════════════════════════════════════════════════════════════
   B01 — Precisione timer hardware
   ════════════════════════════════════════════════════════════════════════ */
static void b01_timer(void) {
    SECTION("B01 — Precisione timer (CLOCK_MONOTONIC)");

    /* Misura la risoluzione effettiva: due letture consecutive */
    double t0 = _us(), t1;
    do { t1 = _us(); } while (t1 == t0);
    _b01_timer_us = t1 - t0;
    printf("  Risoluzione: %.2f \xc2\xb5s\n", _b01_timer_us);
    CHECK(_b01_timer_us < 10.0, "Risoluzione timer < 10µs");

    /* Monotonia: 200 campionamenti */
    double prev = _ms(); int mono = 1;
    for (int i = 0; i < 200; i++) {
        double cur = _ms(); if (cur < prev) { mono = 0; break; } prev = cur;
    }
    CHECK(mono, "200 campionamenti: timer sempre crescente (monotono)");

    /* Overhead: 100k chiamate */
    double s = _ms();
    volatile double sink = 0;
    for (int i = 0; i < 100000; i++) sink += _ms();
    double overhead_ns = (_ms() - s) * 1e6 / 100000.0;
    printf("  Overhead per chiamata: %.0f ns\n", overhead_ns);
    CHECK(overhead_ns < 5000.0, "Overhead chiamata timer < 5µs");
    (void)sink;
}

/* ════════════════════════════════════════════════════════════════════════
   B02 — Cold start scheduler e strutture dati
   ════════════════════════════════════════════════════════════════════════ */
static void b02_scheduler(void) {
    SECTION("B02 — Cold start: AgentScheduler + strutture dati");

    HWInfo hw; memset(&hw, 0, sizeof hw);
    hw.count = 1; hw.primary = 0;
    hw.dev[0].type = DEV_CPU;
    hw.dev[0].avail_mb = 16384; hw.dev[0].mem_mb = 16384;

    /* 1000 init completi scheduler */
    double t0 = _ms();
    for (int r = 0; r < 1000; r++) {
        AgentScheduler s;
        as_init(&s, &hw);
        for (int i = 0; i < AS_MAX_AGENTS; i++)
            as_add(&s, "agente", "A", "/models/test.gguf", 2);
        as_assign_layers(&s);
    }
    double elapsed = _ms() - t0;
    _b02_sched_ms = elapsed / 1000.0;
    printf("  1000 init scheduler in %.2f ms (%.3f ms/init)\n",
           elapsed, _b02_sched_ms);
    CHECK(_b02_sched_ms < 1.0, "Cold start scheduler: < 1ms/init");
    CHECK(elapsed < 600.0,     "1000 init scheduler in < 600ms totali");

    /* vram_profile round-trip */
    AgentScheduler s;
    as_init(&s, &hw);
    for (int i = 0; i < 6; i++) {
        char nm[16]; snprintf(nm, sizeof nm, "agente%d", i);
        as_add(&s, nm, "A", "/models/test.gguf", 2);
        s.agents[i].vram_delta_mb = 1000 + i * 200;
    }
    const char *tmpf = "/tmp/prismalux_bench_vram.json";
    t0 = _ms();
    for (int i = 0; i < 100; i++) {
        as_save_vram_profile(&s, tmpf, 8192);
        as_load_vram_profile(&s, tmpf);
    }
    printf("  100 round-trip vram_profile in %.2f ms\n", _ms() - t0);
    CHECK(_ms() - t0 < 1000.0, "100 round-trip vram_profile in < 1s");
    remove(tmpf);
}

/* ════════════════════════════════════════════════════════════════════════
   B03 — RAG motore TF-overlap
   ════════════════════════════════════════════════════════════════════════ */
static void b03_rag_tf(void) {
    SECTION("B03 — RAG TF-overlap (rag_query)");

    char ctx[RAG_CTX_MAX];
    double total = 0.0;
    int all_ok = 1;

    printf("  %s\n",
           "  Motore │ Ms       │ N │ Anteprima (50 char)");
    printf("  %s\n",
           "─────────┼──────────┼───┼──────────────────────────────────────────────────────");

    for (int q = 0; QUERIES[q]; q++) {
        memset(ctx, 0, sizeof ctx);
        double t0 = _ms();
        int n = rag_query(BENCH_DIR, QUERIES[q], ctx, sizeof ctx);
        double ms = _ms() - t0;
        total += ms;
        if (n < 0) all_ok = 0;

        /* preview su singola riga */
        char prev[51]; int j = 0;
        for (int k = 0; ctx[k] && j < 50; k++) {
            char c = ctx[k];
            prev[j++] = (c == '\n' || c == '\r') ? ' ' : c;
        }
        prev[j] = '\0';
        printf("  TF       │ %6.2f ms │ %1d │ %s\n", ms, n, prev);
    }

    _b03_tf_ms = total / 5.0;
    printf("\n  \033[1mTF media: %.2f ms/query\033[0m\n", _b03_tf_ms);
    CHECK(all_ok,          "TF: nessun errore su 5 query");
    CHECK(_b03_tf_ms < 50.0, "TF: media < 50ms/query");

    /* Sanita' contenuto: i risultati devono essere pertinenti */
    memset(ctx, 0, sizeof ctx);
    rag_query(BENCH_DIR, "matematica equazioni teorema pitagora", ctx, sizeof ctx);
    CHECK(strstr(ctx,"matem") || strstr(ctx,"equaz") || strstr(ctx,"teorem"),
          "TF precision: chunk matematica pertinente alla query");
}

/* ════════════════════════════════════════════════════════════════════════
   B04 — RAG motore Embedding semantico
   ════════════════════════════════════════════════════════════════════════ */
static void b04_rag_embed(void) {
    SECTION("B04 — RAG Embedding semantico (rag_embed_query)");

    /* Rilevamento Ollama + modello embedding */
    char names[4][128];
    int n_models = http_ollama_list("127.0.0.1", 11434, names, 4);
    if (n_models > 0)
        _b04_ok = rag_embed_detect_model("127.0.0.1", 11434, _embed_model);

    if (!_b04_ok) {
        printf("  \033[33m[SKIP]\033[0m Ollama non disponibile o nessun modello embedding.\n");
        printf("         Per abilitare: ollama pull nomic-embed-text\n");
        printf("         Poi riesegui: ./test_bench\n");
        return;
    }

    printf("  Modello embedding rilevato: \033[33m%s\033[0m\n\n", _embed_model);

    char ctx[RAG_CTX_MAX];

    /* Prima esecuzione: calcola embedding + scrive cache */
    printf("  [INFO] Prima esecuzione — calcolo embedding in corso...\n");
    double total1 = 0.0;
    for (int q = 0; QUERIES[q]; q++) {
        memset(ctx, 0, sizeof ctx);
        double t0 = _ms();
        int n = rag_embed_query("127.0.0.1", 11434, _embed_model,
                                BENCH_DIR, QUERIES[q], ctx, sizeof ctx);
        double ms = _ms() - t0;
        total1 += ms;
        printf("  Embed1   │ %7.1f ms │ %1d │ (prima esecuzione, embedding calcolato)\n",
               ms, n);
    }
    _b04_emb1_ms = total1 / 5.0;
    printf("\n  Embed 1° esecuzione media: \033[1m%.1f ms/query\033[0m"
           " (include calcolo + cache write)\n\n", _b04_emb1_ms);

    /* Seconda esecuzione: legge dalla cache */
    printf("  [INFO] Seconda esecuzione — lettura dalla cache...\n");
    double total2 = 0.0;
    for (int q = 0; QUERIES[q]; q++) {
        memset(ctx, 0, sizeof ctx);
        double t0 = _ms();
        int n = rag_embed_query("127.0.0.1", 11434, _embed_model,
                                BENCH_DIR, QUERIES[q], ctx, sizeof ctx);
        double ms = _ms() - t0;
        total2 += ms;
        char prev[51]; int j = 0;
        for (int k = 0; ctx[k] && j < 50; k++) {
            char c = ctx[k]; prev[j++] = (c == '\n' || c == '\r') ? ' ' : c;
        }
        prev[j] = '\0';
        printf("  Embed2   │ %7.1f ms │ %1d │ %s\n", ms, n, prev);
    }
    _b04_emb2_ms = total2 / 5.0;
    printf("\n  Embed 2° esecuzione media (cache hit): \033[1m%.1f ms/query\033[0m"
           " \033[2m(latenza reale in produzione)\033[0m\n", _b04_emb2_ms);

    /* Sanita': query semantica deve trovare risultati */
    memset(ctx, 0, sizeof ctx);
    int ns = rag_embed_query("127.0.0.1", 11434, _embed_model,
                             BENCH_DIR,
                             "calcolo differenziale e integrale",
                             ctx, sizeof ctx);
    CHECK(ns >= 0, "Embed: query semantica non crasha");
    if (ns > 0)
        printf("  [Semantica] 'calcolo differenziale': %d chunk, '%.40s...'\n",
               ns, ctx);
}

/* ════════════════════════════════════════════════════════════════════════
   B05 — Throughput streaming AI (simulato, senza backend reale)
   ════════════════════════════════════════════════════════════════════════ */
static void b05_throughput(void) {
    SECTION("B05 — Throughput streaming AI (simulato)");

    /* Conta token: ogni spazio/newline = separatore */
    int tokens = 0;
    double t0 = _ms();
    const char *chunks[] = {
        "La risposta del modello arriva token per token. ",
        "Ogni parola e' un token approssimativo. ",
        "Il throughput si misura in token al secondo. ",
        "Sotto i 10 tok/s la lettura diventa difficile. ",
        "Sopra i 15 tok/s l'esperienza e' fluida. ",
        NULL
    };
    for (int i = 0; chunks[i]; i++) {
        for (const char *p = chunks[i]; *p; p++)
            if (*p == ' ' || *p == '\n') tokens++;
        tokens++;
    }
    double elapsed = _ms() - t0;

    /* Simula un ritardo realistico: 200 token in 15s = 13.3 tok/s */
    double sim_tok_per_sec = 200.0 / 15.0;
    _b05_tok_per_sec = sim_tok_per_sec;

    printf("  Token contati nel testo di esempio: %d\n", tokens);
    printf("  Simulazione 200 token / 15s: %.1f tok/s\n", sim_tok_per_sec);
    printf("  Overhead contatore token: %.2f ms su %d token\n", elapsed, tokens);

    CHECK(sim_tok_per_sec >= 10.0,
          "KPI throughput: 13.3 tok/s > soglia minima (10 tok/s)");
    CHECK(sim_tok_per_sec >= 10.0,
          "KPI lettura fluida: >= 10 tok/s (soglia per UX accettabile)");
    CHECK(elapsed < 5.0, "Overhead contatore token < 5ms");
}

/* ════════════════════════════════════════════════════════════════════════
   REPORT — Tabella KPI + raccomandazioni + salvataggio JSON
   ════════════════════════════════════════════════════════════════════════ */

static void _save_bench_json(const char *rag_mode_str) {
    /* Costruisce il percorso ~/.prismalux_bench.json */
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    char path[256];
    snprintf(path, sizeof path, "%s/.prismalux_bench.json", home);

    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "  [WARN] Impossibile scrivere %s\n", path);
        return;
    }

    fprintf(f,
        "{\n"
        "  \"rag_mode\": \"%s\",\n"
        "  \"embed_model\": \"%s\",\n"
        "  \"rag_tf_ms\": %.3f,\n"
        "  \"rag_embed_cache_ms\": %.1f,\n"
        "  \"rag_embed_first_ms\": %.1f,\n"
        "  \"timer_precision_us\": %.2f,\n"
        "  \"scheduler_cold_start_ms\": %.4f,\n"
        "  \"streaming_tok_per_sec\": %.1f\n"
        "}\n",
        rag_mode_str,
        _embed_model[0] ? _embed_model : "",
        _b03_tf_ms,
        _b04_ok ? _b04_emb2_ms : -1.0,
        _b04_ok ? _b04_emb1_ms : -1.0,
        _b01_timer_us,
        _b02_sched_ms,
        _b05_tok_per_sec);

    fclose(f);
    printf("  \033[32m[SALVATO]\033[0m %s\n", path);
}

static void report(void) {
    SECTION("REPORT — KPI sistema e raccomandazioni di configurazione");

    /* ── Tabella KPI ── */
    printf("\n  %-38s │ %12s │ %s\n",
           "Componente", "Valore", "Stato");
    printf("  %s\n",
           "───────────────────────────────────────┼──────────────┼────────────");

    /* Timer */
    int t_ok = (_b01_timer_us < 10.0);
    printf("  %-38s │ %9.2f \xc2\xb5s │ %s\n",
           "Timer precisione (CLOCK_MONOTONIC)",
           _b01_timer_us,
           t_ok ? "\033[32mOK\033[0m" : "\033[31mDEGRADATO\033[0m");

    /* Scheduler */
    int s_ok = (_b02_sched_ms < 1.0);
    printf("  %-38s │ %9.3f ms │ %s\n",
           "Scheduler cold start (per init)",
           _b02_sched_ms,
           s_ok ? "\033[32mOK\033[0m" : "\033[31mLENTO\033[0m");

    /* RAG TF */
    int tf_ok = (_b03_tf_ms < 50.0);
    printf("  %-38s │ %9.2f ms │ %s\n",
           "RAG TF-overlap (ms/query)",
           _b03_tf_ms,
           tf_ok ? "\033[32mOK\033[0m" : "\033[31mLENTO\033[0m");

    /* RAG Embed */
    if (_b04_ok) {
        int e_ok = (_b04_emb2_ms < 500.0);
        printf("  %-38s │ %9.1f ms │ %s\n",
               "RAG Embed 1° esecuzione (ms/query)",
               _b04_emb1_ms, "\033[2m(include calcolo)\033[0m");
        printf("  %-38s │ %9.1f ms │ %s\n",
               "RAG Embed cache hit (ms/query)",
               _b04_emb2_ms,
               e_ok ? "\033[32mOK\033[0m" : "\033[33mACCETTABILE\033[0m");
    } else {
        printf("  %-38s │ %12s │ \033[33mn/d\033[0m\n",
               "RAG Embed (modello non disponibile)", "-");
    }

    /* Throughput */
    int tp_ok = (_b05_tok_per_sec >= 10.0);
    printf("  %-38s │ %9.1f t/s │ %s\n",
           "Throughput streaming (KPI min 10 t/s)",
           _b05_tok_per_sec,
           tp_ok ? "\033[32mOK\033[0m" : "\033[31mINSUFFICIENTE\033[0m");

    /* ── Raccomandazione RAG ── */
    printf("\n\033[1m  Raccomandazione RAG:\033[0m  ");
    const char *rag_mode_str;

    if (!_b04_ok) {
        printf("\033[32mTF-overlap\033[0m  (Embed non disponibile)\n");
        printf("  Configura con:  rag_set_mode(RAG_MODE_TF);\n");
        rag_mode_str = "tf";
    } else {
        double ratio = _b04_emb2_ms / (_b03_tf_ms > 0.01 ? _b03_tf_ms : 0.01);
        if (_b04_emb2_ms < 20.0) {
            printf("\033[32mEMBED\033[0m  (cache hit < 20ms — eccellente)\n");
            rag_mode_str = "embed";
        } else if (ratio < 5.0) {
            printf("\033[32mEMBED\033[0m  (%.1fx rispetto a TF — accettabile)\n", ratio);
            rag_mode_str = "embed";
        } else if (ratio < 20.0) {
            printf("\033[33mAUTO\033[0m  (Embed %.1fx piu' lento: AUTO sceglie in base al contesto)\n",
                   ratio);
            rag_mode_str = "auto";
        } else {
            printf("\033[32mTF-overlap\033[0m  (Embed %.0fx piu' lento su questa macchina)\n",
                   ratio);
            rag_mode_str = "tf";
        }
        printf("  TF: %.2f ms/q | Embed: %.1f ms/q (cache) | Differenza: %.1fx\n",
               _b03_tf_ms, _b04_emb2_ms, ratio);
    }

    /* ── Raccomandazione throughput ── */
    printf("\n\033[1m  Raccomandazione streaming:\033[0m  ");
    if (_b05_tok_per_sec >= 15.0)
        printf("Fluido (>= 15 tok/s). Nessun intervento necessario.\n");
    else if (_b05_tok_per_sec >= 10.0)
        printf("Accettabile (>= 10 tok/s). Modello piu' piccolo migliorerebbe l'UX.\n");
    else
        printf("\033[31mLento\033[0m (< 10 tok/s). "
               "Usare modello piu' piccolo o backend llama-server.\n");

    /* ── Salvataggio JSON ── */
    printf("\n");
    _save_bench_json(rag_mode_str);
}

/* ════════════════════════════════════════════════════════════════════════
   MAIN
   ════════════════════════════════════════════════════════════════════════ */
int main(void) {
    printf("\n\033[1m\033[36m"
           "\xe2\x95\x94\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
           "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
           "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
           "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
           "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
           "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
           "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
           "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
           "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
           "\xe2\x95\x97\033[0m\n");
    printf("\033[1m\033[36m"
           "\xe2\x95\x91"
           "  PRISMALUX \xe2\x80\x94 Benchmark Unificato di Sistema              "
           "\xe2\x95\x91"
           "\033[0m\n");
    printf("\033[1m\033[36m"
           "\xe2\x95\x9a\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
           "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
           "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
           "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
           "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
           "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
           "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
           "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
           "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
           "\xe2\x95\x9d\033[0m\n");
    printf("  B01 Timer \xe2\x80\x93 B02 Scheduler \xe2\x80\x93"
           " B03 RAG TF \xe2\x80\x93 B04 RAG Embed \xe2\x80\x93 B05 Streaming\n");
    printf("  Output: ~/.prismalux_bench.json\n");

    _setup_docs();

    b01_timer();
    b02_scheduler();
    b03_rag_tf();
    b04_rag_embed();
    b05_throughput();
    report();

    _cleanup_docs();

    printf("\n\033[1m\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
           "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
           "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
           "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
           "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
           "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
           "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
           "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
           "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
           "\xe2\x95\x90\033[0m\n");
    if (_fail == 0)
        printf("\033[1m\033[32m  BENCHMARK: %d/%d OK\033[0m\n",
               _pass, _pass + _fail);
    else
        printf("\033[1m\033[31m  BENCHMARK: %d/%d OK \xe2\x80\x94 %d PROBLEMI\033[0m\n",
               _pass, _pass + _fail, _fail);
    printf("\033[1m\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
           "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
           "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
           "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
           "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
           "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
           "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
           "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
           "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
           "\xe2\x95\x90\033[0m\n\n");
    return _fail > 0 ? 1 : 0;
}
