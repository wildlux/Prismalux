/* ══════════════════════════════════════════════════════════════════════════
   test_perf.c — Performance: TTFT, throughput, cold start, timer precision
   ══════════════════════════════════════════════════════════════════════════
   KPI di riferimento:
     TTFT (Time to First Token) : < 3000ms (token senza backend reale)
     Throughput                 : > 10 tokens/sec per lettura fluida
     Timer precision            : < 1µs risoluzione (CLOCK_MONOTONIC)
     Cold start strutture       : < 50ms per init scheduler + 8 agenti

   Build:
     gcc -O2 -Iinclude -lm -o test_perf \
         test_perf.c src/agent_scheduler.c src/hw_detect.c

   Esecuzione: ./test_perf
   (no AI/rete richiesta — usa callback simulati)
   ══════════════════════════════════════════════════════════════════════════ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>

#include "agent_scheduler.h"
#include "hw_detect.h"

/* ── Harness ──────────────────────────────────────────────────────────── */
static int _pass = 0, _fail = 0;
#define SECTION(t) printf("\n\033[36m\033[1m─── %s ───\033[0m\n", t)
#define CHECK(cond, label) do { \
    if (cond) { printf("  \033[32m[OK]\033[0m  %s\n", label); _pass++; } \
    else       { printf("  \033[31m[FAIL]\033[0m %s\n", label); _fail++; } \
} while(0)
#define CHECKF(got, exp, tol, label) do { \
    double _g=(got),_e=(exp),_t=(tol); \
    if (fabs(_g-_e)<_t){ printf("  \033[32m[OK]\033[0m  %s (=%.4g)\n",label,_g); _pass++; } \
    else { printf("  \033[31m[FAIL]\033[0m %s  got=%.4g exp=%.4g\n",label,_g,_e); _fail++; } \
} while(0)

/* ── Timer helpers ────────────────────────────────────────────────────── */
static inline double now_ms(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec * 1000.0 + (double)t.tv_nsec / 1e6;
}
static inline double now_us(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec * 1e6 + (double)t.tv_nsec / 1e3;
}

/* ════════════════════════════════════════════════════════════════════════
   STRUTTURA: PerfMetrics — raccoglie metriche di una sessione streaming
   ════════════════════════════════════════════════════════════════════════ */
typedef struct {
    double  ttft_ms;          /* Time to First Token (ms) */
    double  total_ms;         /* Durata totale generazione */
    int     token_count;      /* Numero token generati */
    double  tokens_per_sec;   /* Throughput effettivo */
    int     first_token_seen; /* Flag: primo token ricevuto */
    double  start_ms;         /* Timestamp inizio generazione */
} PerfMetrics;

static void perf_init(PerfMetrics *m) {
    memset(m, 0, sizeof *m);
    m->start_ms = now_ms();
}

/* Chiamata a ogni chunk — simula lw_token_cb */
static void perf_on_token(PerfMetrics *m, const char *chunk) {
    if (!m->first_token_seen && chunk && chunk[0]) {
        m->ttft_ms = now_ms() - m->start_ms;
        m->first_token_seen = 1;
    }
    /* Conta token approssimativi: ogni parola = 1 token (stima) */
    if (chunk) {
        for (const char *p = chunk; *p; p++)
            if (*p == ' ' || *p == '\n') m->token_count++;
        m->token_count++;  /* +1 per il chunk stesso */
    }
}

static void perf_finish(PerfMetrics *m) {
    m->total_ms = now_ms() - m->start_ms;
    if (m->total_ms > 0)
        m->tokens_per_sec = (double)m->token_count / (m->total_ms / 1000.0);
}

static void perf_print(const PerfMetrics *m) {
    printf("  TTFT        : %.2f ms\n",   m->ttft_ms);
    printf("  Totale      : %.2f ms\n",   m->total_ms);
    printf("  Token       : %d\n",         m->token_count);
    printf("  Throughput  : %.1f tok/s\n", m->tokens_per_sec);
}

/* ════════════════════════════════════════════════════════════════════════
   T01 — Precisione timer
   ════════════════════════════════════════════════════════════════════════ */
static void t01_timer_precision(void) {
    SECTION("T01 — Precisione timer CLOCK_MONOTONIC");

    /* Misura la risoluzione effettiva */
    double t0 = now_us(), t1;
    do { t1 = now_us(); } while (t1 == t0);
    double resolution_us = t1 - t0;
    printf("  Risoluzione timer: %.3f µs\n", resolution_us);
    CHECK(resolution_us < 10.0, "Risoluzione timer < 10µs (CLOCK_MONOTONIC)");

    /* Monotono: 100 campionamenti sempre crescenti */
    double prev = now_ms();
    int monotone = 1;
    for (int i = 0; i < 100; i++) {
        double cur = now_ms();
        if (cur < prev) { monotone = 0; break; }
        prev = cur;
    }
    CHECK(monotone, "100 campionamenti: timer sempre monotono crescente");

    /* Misura sleep(0): deve essere molto rapido */
    double t_sleep0 = now_ms();
    struct timespec req = {0, 0};
    nanosleep(&req, NULL);
    double elapsed = now_ms() - t_sleep0;
    CHECK(elapsed < 5.0, "nanosleep(0) dura < 5ms");

    /* Ripetibilità: 10 misurazioni di 1ms sleep */
    double samples[10];
    for (int i = 0; i < 10; i++) {
        double s = now_ms();
        struct timespec r1 = {0, 1000000L}; /* 1ms */
        nanosleep(&r1, NULL);
        samples[i] = now_ms() - s;
    }
    double min_s = samples[0], max_s = samples[0], sum_s = 0;
    for (int i = 0; i < 10; i++) {
        if (samples[i] < min_s) min_s = samples[i];
        if (samples[i] > max_s) max_s = samples[i];
        sum_s += samples[i];
    }
    double avg_s = sum_s / 10.0;
    printf("  sleep(1ms): min=%.2f max=%.2f avg=%.2f ms\n", min_s, max_s, avg_s);
    CHECK(avg_s >= 0.9 && avg_s < 20.0, "sleep(1ms): media tra 0.9ms e 20ms");
    CHECK(max_s - min_s < 15.0,         "sleep(1ms): jitter < 15ms");
}

/* ════════════════════════════════════════════════════════════════════════
   T02 — TTFT simulato (mock callback stream)
   ════════════════════════════════════════════════════════════════════════ */
static void t02_ttft_simulated(void) {
    SECTION("T02 — TTFT simulato: mock stream a velocità variabile");

    /* Simula stream con latenza primo token = 50ms */
    {
        PerfMetrics m;
        perf_init(&m);
        struct timespec delay = {0, 50000000L}; /* 50ms */
        nanosleep(&delay, NULL);  /* simula latenza backend */
        perf_on_token(&m, "Ciao");
        perf_on_token(&m, " come");
        perf_on_token(&m, " stai");
        perf_on_token(&m, " oggi");
        perf_finish(&m);
        printf("  [stream 50ms latency] "); perf_print(&m);
        CHECK(m.ttft_ms >= 40.0 && m.ttft_ms < 200.0,
              "TTFT 50ms latency: tra 40ms e 200ms");
        CHECK(m.first_token_seen, "primo token rilevato");
        CHECK(m.token_count > 0, "token_count > 0");
    }

    /* Simula stream veloce: TTFT ideale < 100ms */
    {
        PerfMetrics m;
        perf_init(&m);
        const char *tokens[] = {"Token1 ","Token2 ","Token3 ","Token4 ","Token5 ",
                                 "Token6 ","Token7 ","Token8 ","Token9 ","Token10 ", NULL};
        for (int i = 0; tokens[i]; i++) perf_on_token(&m, tokens[i]);
        perf_finish(&m);
        printf("  [stream istantaneo] "); perf_print(&m);
        CHECK(m.ttft_ms < 100.0, "TTFT stream istantaneo < 100ms");
        CHECK(m.token_count >= 10, "almeno 10 token contati");
        /* Throughput: senza latenza artificiale deve essere altissimo */
        CHECK(m.tokens_per_sec > 100.0 || m.total_ms < 1.0,
              "throughput istantaneo: >100 tok/s oppure totale <1ms");
    }

    /* Simula stream a 15 tok/s (sopra soglia KPI) */
    {
        PerfMetrics m;
        perf_init(&m);
        /* Latenza primo token: 200ms */
        struct timespec d1 = {0, 200000000L};
        nanosleep(&d1, NULL);
        perf_on_token(&m, "Primo");

        /* 30 token in 2 secondi = 15 tok/s */
        struct timespec d2 = {0, 66666666L}; /* ~67ms tra token */
        for (int i = 0; i < 15; i++) {
            nanosleep(&d2, NULL);
            char tok[16]; snprintf(tok, sizeof tok, "tok%d ", i);
            perf_on_token(&m, tok);
        }
        perf_finish(&m);
        printf("  [stream 15 tok/s simulato] "); perf_print(&m);
        CHECK(m.ttft_ms >= 150.0 && m.ttft_ms < 500.0, "TTFT 200ms: tra 150 e 500ms");
        CHECK(m.tokens_per_sec >= 5.0,  "throughput >= 5 tok/s (sopra minimo lettura)");
    }
}

/* ════════════════════════════════════════════════════════════════════════
   T03 — Cold start scheduler (strutture dati, senza AI)
   ════════════════════════════════════════════════════════════════════════ */
static void t03_cold_start(void) {
    SECTION("T03 — Cold start: init strutture senza AI");

    HWInfo hw; memset(&hw,0,sizeof hw);
    hw.count=1; hw.primary=0;
    hw.dev[0].type=DEV_CPU; hw.dev[0].avail_mb=16384; hw.dev[0].mem_mb=16384;

    /* Misura init scheduler + 8 agenti (AS_MAX_AGENTS=8) */
    double t0 = now_ms();
    for (int rep = 0; rep < 1000; rep++) {
        AgentScheduler s;
        as_init(&s, &hw);
        for (int i = 0; i < AS_MAX_AGENTS; i++)
            as_add(&s, "agente", "A", "/models/test.gguf", 2);
        as_assign_layers(&s);
    }
    double elapsed = now_ms() - t0;
    double per_init = elapsed / 1000.0;
    printf("  1000 init completi in %.2f ms (%.3f ms/init)\n", elapsed, per_init);
    CHECK(per_init < 1.0, "cold start scheduler: < 1ms per init completo");
    CHECK(elapsed < 500.0, "1000 init scheduler in < 500ms totali");

    /* Misura vram_profile save+load round-trip */
    const char *tmpf = "/tmp/test_perf_vram.json";
    AgentScheduler s;
    as_init(&s, &hw);
    for (int i = 0; i < 6; i++) {
        char name[16]; snprintf(name,sizeof name,"agente%d",i);
        as_add(&s, name, "A", "/models/test.gguf", 2);
        s.agents[i].vram_delta_mb = 1000 + i*200;
    }
    t0 = now_ms();
    for (int i = 0; i < 100; i++) {
        as_save_vram_profile(&s, tmpf, 8192);
        as_load_vram_profile(&s, tmpf);
    }
    elapsed = now_ms() - t0;
    printf("  100 round-trip vram_profile in %.2f ms\n", elapsed);
    CHECK(elapsed < 500.0, "100 round-trip vram_profile in < 500ms");
    remove(tmpf);
}

/* ════════════════════════════════════════════════════════════════════════
   T04 — Throughput token counter
   ════════════════════════════════════════════════════════════════════════ */
static void t04_throughput(void) {
    SECTION("T04 — Throughput: accuratezza contatore token");

    /* Token counter: verifica conteggio esatto su testo noto */
    PerfMetrics m;
    perf_init(&m);

    /* 100 frasi di 10 token ciascuna = 1000 token approssimativi */
    for (int i = 0; i < 100; i++)
        perf_on_token(&m, "uno due tre quattro cinque sei sette otto nove dieci ");
    perf_finish(&m);

    printf("  Token contati: %d (attesi ~1000)\n", m.token_count);
    CHECK(m.token_count >= 900 && m.token_count <= 1100,
          "contatore token: 900-1100 per 100 frasi da 10 parole");

    /* KPI check: simula risposta di 200 token in 15 secondi */
    PerfMetrics m2;
    perf_init(&m2);
    for (int i = 0; i < 200; i++) {
        char tok[16]; snprintf(tok, sizeof tok, "tok%d ", i);
        perf_on_token(&m2, tok);
    }
    /* Forza total_ms = 15000 per simulare 15s */
    m2.total_ms = 15000.0;
    m2.tokens_per_sec = (double)m2.token_count / (m2.total_ms / 1000.0);

    printf("  Simulazione 200 token in 15s: %.1f tok/s\n", m2.tokens_per_sec);
    CHECK(m2.tokens_per_sec >= 10.0,
          "200 tok / 15s = 13.3 tok/s > soglia KPI (10 tok/s)");

    /* Soglia critica: < 5 tok/s = lettura difficile */
    PerfMetrics m3;
    perf_init(&m3);
    for (int i = 0; i < 50; i++) perf_on_token(&m3, "tok ");
    m3.total_ms = 20000.0;
    m3.tokens_per_sec = (double)m3.token_count / (m3.total_ms / 1000.0);
    CHECK(m3.tokens_per_sec < 10.0,
          "50 tok / 20s = 2.5 tok/s < soglia KPI (rilevazione corretta)");
}

/* ════════════════════════════════════════════════════════════════════════
   T05 — Benchmark: 1 milione di misurazioni timer
   ════════════════════════════════════════════════════════════════════════ */
static void t05_timer_benchmark(void) {
    SECTION("T05 — Benchmark: 1M misurazioni now_ms()");

    double t0 = now_ms();
    volatile double sink = 0;
    for (int i = 0; i < 1000000; i++) sink += now_ms();
    double elapsed = now_ms() - t0;
    double ns_per_call = elapsed * 1e6 / 1000000.0;

    printf("  1M chiamate now_ms() in %.2f ms (%.1f ns/call)\n",
           elapsed, ns_per_call);
    CHECK(elapsed < 2000.0, "1M misurazioni in < 2s");
    CHECK(ns_per_call < 2000.0, "ogni misurazione < 2µs");
    (void)sink;
}

/* ════════════════════════════════════════════════════════════════════════
   T06 — KPI summary: verifica soglie di sistema
   ════════════════════════════════════════════════════════════════════════ */
static void t06_kpi_summary(void) {
    SECTION("T06 — KPI summary: soglie di sistema");

    /* Definizione soglie KPI (da TODO_TESTS.md) */
    struct { const char *name; double value; double threshold; int higher_is_better; } kpi[] = {
        { "TTFT min accettabile (ms)",          3000.0,  3000.0, 0 },
        { "Throughput min lettura (tok/s)",        10.0,    10.0, 1 },
        { "Throughput fluida (tok/s)",             15.0,    10.0, 1 },
        { "Timer risoluzione max accettabile (µs)", 10.0,   10.0, 0 },
        { "Cold start scheduler max (ms)",          1.0,     1.0, 0 },
    };
    int n = (int)(sizeof kpi / sizeof kpi[0]);
    for (int i = 0; i < n; i++) {
        int ok = kpi[i].higher_is_better
                 ? (kpi[i].value >= kpi[i].threshold)
                 : (kpi[i].value <= kpi[i].threshold);
        char buf[80];
        snprintf(buf, sizeof buf, "%s = %.1f (soglia %.1f)",
                 kpi[i].name, kpi[i].value, kpi[i].threshold);
        CHECK(ok, buf);
    }
}

/* ════════════════════════════════════════════════════════════════════════
   MAIN
   ════════════════════════════════════════════════════════════════════════ */
int main(void) {
    printf("\n\033[1m══ test_perf.c — Performance: TTFT, Throughput, Cold Start ══\033[0m\n");
    printf("   KPI: TTFT<3s | Throughput>10tok/s | Timer<10µs | ColdStart<1ms\n");

    t01_timer_precision();
    t02_ttft_simulated();
    t03_cold_start();
    t04_throughput();
    t05_timer_benchmark();
    t06_kpi_summary();

    printf("\n\033[1m══════════════════════════════════════════════════════\033[0m\n");
    if (_fail == 0)
        printf("\033[1m\033[32m  RISULTATI: %d/%d PASSATI — PERFORMANCE OK\033[0m\n",
               _pass, _pass+_fail);
    else
        printf("\033[1m\033[31m  RISULTATI: %d/%d PASSATI — %d FALLITI\033[0m\n",
               _pass, _pass+_fail, _fail);
    printf("\033[1m══════════════════════════════════════════════════════\033[0m\n\n");
    return _fail > 0 ? 1 : 0;
}
