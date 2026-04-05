/* ══════════════════════════════════════════════════════════════════════════
   test_agent_scheduler.c — Test isolati: Hot/Cold scheduler
   ══════════════════════════════════════════════════════════════════════════
   Testa l'intera API pubblica senza AI/rete (backend HTTP/llama-static
   non necessari: HAS_LW=0, le operazioni di caricamento reale sono stub).

   Categorie:
     T01 — as_init: valori default, flag sequential
     T02 — as_add: registrazione agenti, overflow, priorità fuori range
     T03 — as_assign_layers: CPU-only, VRAM piena, scaling proporzionale
     T04 — as_load: cache hit, path vuoto, evict automatico
     T05 — as_evict: scaricamento HOT, hot_idx=-1 dopo
     T06 — as_record: call_count, EMA latenza, promozione sticky-hot
     T07 — as_sequential: flag coerente con VRAM
     T08 — as_save_vram_profile + as_load_vram_profile: round-trip JSON
     T09 — Edge case: idx invalidi, scheduler vuoto
     T10 — Stress: 1000 load+record consecutivi

   Build:
     gcc -O2 -Iinclude -lm -o test_agent_scheduler \
         test_agent_scheduler.c src/agent_scheduler.c src/hw_detect.c

   Esecuzione: ./test_agent_scheduler
   ══════════════════════════════════════════════════════════════════════════ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "agent_scheduler.h"
#include "hw_detect.h"

/* ── Harness ──────────────────────────────────────────────────────────── */
static int _pass = 0, _fail = 0;
#define SECTION(t) printf("\n\033[36m\033[1m─── %s ───\033[0m\n", t)
#define CHECK(cond, label) do { \
    if (cond) { printf("  \033[32m[OK]\033[0m  %s\n", label); _pass++; } \
    else       { printf("  \033[31m[FAIL]\033[0m %s\n", label); _fail++; } \
} while(0)

/* ── Helper: HWInfo con VRAM fissa (no hw_detect() reale) ──────────────── */
static HWInfo make_hw(long long vram_mb, long long ram_mb) {
    HWInfo hw;
    memset(&hw, 0, sizeof hw);
    hw.count     = 2;
    hw.primary   = 0;   /* GPU primaria */
    hw.secondary = 1;

    /* dispositivo 0: GPU NVIDIA */
    hw.dev[0].type     = DEV_NVIDIA;
    hw.dev[0].avail_mb = vram_mb;
    hw.dev[0].mem_mb   = vram_mb;
    hw.dev[0].gpu_index = 0;
    strncpy(hw.dev[0].name, "TestGPU", sizeof hw.dev[0].name - 1);

    /* dispositivo 1: CPU */
    hw.dev[1].type     = DEV_CPU;
    hw.dev[1].avail_mb = ram_mb;
    hw.dev[1].mem_mb   = ram_mb;
    hw.dev[1].gpu_index = -1;
    strncpy(hw.dev[1].name, "TestCPU", sizeof hw.dev[1].name - 1);

    return hw;
}

static HWInfo make_hw_no_gpu(long long ram_mb) {
    HWInfo hw;
    memset(&hw, 0, sizeof hw);
    hw.count     = 1;
    hw.primary   = 0;   /* solo CPU */
    hw.secondary = -1;
    hw.dev[0].type     = DEV_CPU;
    hw.dev[0].avail_mb = ram_mb;
    hw.dev[0].mem_mb   = ram_mb;
    hw.dev[0].gpu_index = -1;
    strncpy(hw.dev[0].name, "TestCPU", sizeof hw.dev[0].name - 1);
    return hw;
}

/* ════════════════════════════════════════════════════════════════════════
   T01 — as_init
   ════════════════════════════════════════════════════════════════════════ */
static void t01_init(void) {
    SECTION("T01 — as_init: inizializzazione");

    /* GPU con 8 GB VRAM → non sequenziale */
    HWInfo hw = make_hw(8192, 16384);
    AgentScheduler s;
    as_init(&s, &hw);
    CHECK(s.n == 0,             "n agenti = 0 dopo init");
    CHECK(s.hot_idx == -1,      "hot_idx = -1 dopo init");
    CHECK(s.vram_avail_mb == 8192, "vram_avail_mb corretta");
    CHECK(s.ram_avail_mb  == 16384,"ram_avail_mb corretta");
    CHECK(s.sequential == 0,    "non sequenziale con VRAM 8GB");
    CHECK(s.cache_hits == 0,    "cache_hits = 0");
    CHECK(s.cache_misses == 0,  "cache_misses = 0");

    /* GPU con solo 2 GB VRAM → sequenziale (< AS_SEQUENTIAL_VRAM_MB=4000) */
    HWInfo hw2 = make_hw(2048, 8192);
    AgentScheduler s2;
    as_init(&s2, &hw2);
    CHECK(s2.sequential == 1,   "sequenziale con VRAM 2GB");

    /* Nessuna GPU → CPU only, VRAM=0, sequenziale */
    HWInfo hw3 = make_hw_no_gpu(32768);
    AgentScheduler s3;
    as_init(&s3, &hw3);
    CHECK(s3.vram_avail_mb == 0,"vram_avail_mb = 0 senza GPU");
    CHECK(s3.sequential == 1,   "sequenziale senza GPU");
}

/* ════════════════════════════════════════════════════════════════════════
   T02 — as_add
   ════════════════════════════════════════════════════════════════════════ */
static void t02_add(void) {
    SECTION("T02 — as_add: registrazione agenti");
    HWInfo hw = make_hw(8192, 16384);
    AgentScheduler s;
    as_init(&s, &hw);

    int i0 = as_add(&s, "ricercatore", "\xf0\x9f\x94\x8d", "/models/qwen.gguf", 2);
    CHECK(i0 == 0, "primo agente: indice 0");
    CHECK(s.n == 1, "n=1 dopo primo add");
    CHECK(strcmp(s.agents[0].name, "ricercatore")==0, "nome agente corretto");
    CHECK(strcmp(s.agents[0].model_file, "qwen.gguf")==0, "model_file estratto dal path");
    CHECK(s.agents[0].priority == 2,  "priorità 2");
    CHECK(s.agents[0].state == AS_COLD, "stato iniziale COLD");
    CHECK(s.agents[0].call_count == 0, "call_count = 0");
    CHECK(s.agents[0].n_ctx > 0,       "n_ctx > 0 (da _default_ctx)");
    CHECK(s.agents[0].n_threads > 0,   "n_threads > 0");

    /* Agente con priorità 1 (sticky-hot) */
    int i1 = as_add(&s, "programmatore", "\xf0\x9f\x92\xbb", "/models/coder.gguf", 1);
    CHECK(i1 == 1, "secondo agente: indice 1");
    CHECK(s.agents[1].priority == 1, "priorità 1 (sticky-hot)");

    /* Agente con path con backslash (Windows) */
    int i2 = as_add(&s, "tester", "\xf0\x9f\xa7\xaa", "C:\\models\\test.gguf", 3);
    CHECK(i2 == 2, "terzo agente: indice 2");
    CHECK(strcmp(s.agents[2].model_file, "test.gguf")==0, "model_file da path Windows");
    CHECK(s.agents[2].priority == 3, "priorità 3 (light)");

    /* Priorità fuori range → normalizzata a 2 */
    int i3 = as_add(&s, "x", "X", "/m.gguf", 0);
    CHECK(i3 == 3, "agente priorità 0: indice 3");
    CHECK(s.agents[3].priority == 2, "priorità 0 → normalizzata a 2");
    int i4 = as_add(&s, "y", "Y", "/m.gguf", 5);
    CHECK(s.agents[4].priority == 2, "priorità 5 → normalizzata a 2");

    /* Agente con path vuoto */
    as_add(&s, "vuoto", "V", "", 2);
    CHECK(s.agents[5].model_path[0] == '\0', "model_path vuoto mantenuto");

    /* Overflow: AS_MAX_AGENTS=8 */
    as_add(&s, "a7", "7", "/m.gguf", 2);
    as_add(&s, "a8", "8", "/m.gguf", 2);
    int iover = as_add(&s, "overflow", "!", "/m.gguf", 2);
    CHECK(iover == -1, "overflow: ritorna -1");
    CHECK(s.n == AS_MAX_AGENTS, "n non supera AS_MAX_AGENTS");

    /* n_ctx diverso per ruolo "programmatore" vs "ricercatore" */
    AgentScheduler s2; as_init(&s2, &hw);
    as_add(&s2, "programmatore", "", "/m.gguf", 2);
    as_add(&s2, "ricercatore",   "", "/m.gguf", 2);
    CHECK(s2.agents[0].n_ctx >= s2.agents[1].n_ctx,
          "programmatore: n_ctx >= ricercatore (più contesto per codice)");
}

/* ════════════════════════════════════════════════════════════════════════
   T03 — as_assign_layers
   ════════════════════════════════════════════════════════════════════════ */
static void t03_assign_layers(void) {
    SECTION("T03 — as_assign_layers: calcolo GPU layers");

    /* Caso: CPU-only (no VRAM) */
    {
        HWInfo hw = make_hw_no_gpu(16384);
        AgentScheduler s; as_init(&s, &hw);
        as_add(&s, "a", "A", "/m.gguf", 2);
        as_assign_layers(&s);
        CHECK(s.agents[0].n_gpu_layers == 0, "CPU-only: n_gpu_layers=0");
    }

    /* Caso: VRAM abbondante → layers massimi */
    {
        HWInfo hw = make_hw(16384, 32768);
        AgentScheduler s; as_init(&s, &hw);
        as_add(&s, "programmatore", "P", "/m.gguf", 2);
        as_assign_layers(&s);
        CHECK(s.agents[0].n_gpu_layers > 0, "VRAM 16GB: n_gpu_layers > 0");
    }

    /* Caso: modello più grande della VRAM → scaling proporzionale */
    {
        HWInfo hw = make_hw(4000, 16384);
        AgentScheduler s; as_init(&s, &hw);
        as_add(&s, "a", "A", "/m.gguf", 2);
        s.agents[0].vram_delta_mb = 8000;  /* modello da 8GB, VRAM 4GB */
        as_assign_layers(&s);
        int layers_full = hw_gpu_layers(4000);  /* base senza scaling */
        CHECK(s.agents[0].n_gpu_layers < layers_full,
              "modello >VRAM: layers scalati proporzionalmente");
        CHECK(s.agents[0].n_gpu_layers >= 0,
              "scaling non produce layers negativi");
    }

    /* Caso: agente sticky-hot (priority=1) → layers massimi anche se grande */
    {
        HWInfo hw = make_hw(4000, 16384);
        AgentScheduler s; as_init(&s, &hw);
        as_add(&s, "prog", "P", "/m.gguf", 1);  /* sticky-hot */
        s.agents[0].vram_delta_mb = 8000;
        as_assign_layers(&s);
        int base = hw_gpu_layers(4000);
        CHECK(s.agents[0].n_gpu_layers == base,
              "sticky-hot: layers = base (ignora scaling)");
    }

    /* Caso: agente light (priority=3) con VRAM bassa → max 16 layer */
    {
        HWInfo hw = make_hw(4000, 16384);
        AgentScheduler s; as_init(&s, &hw);
        as_add(&s, "light", "L", "/m.gguf", 3);
        as_assign_layers(&s);
        int base = hw_gpu_layers(4000);
        if (base > 16)
            CHECK(s.agents[0].n_gpu_layers == 16,
                  "light con VRAM<8GB: cappato a 16 layers");
        else
            CHECK(s.agents[0].n_gpu_layers == base,
                  "light con VRAM<8GB: base già ≤16");
    }
}

/* ════════════════════════════════════════════════════════════════════════
   T04 — as_load
   ════════════════════════════════════════════════════════════════════════ */
static void t04_load(void) {
    SECTION("T04 — as_load: cache hit, evict, path vuoto");
    HWInfo hw = make_hw(8192, 16384);
    AgentScheduler s; as_init(&s, &hw);
    as_add(&s, "a0", "A", "/models/a.gguf", 2);
    as_add(&s, "a1", "B", "/models/b.gguf", 2);
    as_add(&s, "vuoto", "V", "", 2);

    /* idx invalido */
    CHECK(as_load(&s, -1) == -1, "as_load(-1) → -1 (idx invalido)");
    CHECK(as_load(&s, 99) == -1, "as_load(99) → -1 (idx fuori range)");

    /* path vuoto → non può caricare */
    CHECK(as_load(&s, 2) == -1, "as_load(vuoto) → -1 (path vuoto)");

    /* prima load agente 0 → miss */
    int r0 = as_load(&s, 0);
    CHECK(r0 == 0,               "as_load(0) prima volta → 0 (ok)");
    CHECK(s.hot_idx == 0,        "hot_idx = 0 dopo load");
    CHECK(s.agents[0].state == AS_HOT, "agente 0: stato HOT");
    CHECK(s.cache_misses == 1,   "cache_misses = 1");

    /* seconda load stesso agente → cache hit */
    int r0b = as_load(&s, 0);
    CHECK(r0b == 0,              "as_load(0) seconda volta → 0 (cache hit)");
    CHECK(s.cache_hits == 1,     "cache_hits = 1");
    CHECK(s.cache_misses == 1,   "cache_misses ancora 1 (nessun nuovo miss)");

    /* load agente 1 → evict 0, load 1 */
    int r1 = as_load(&s, 1);
    CHECK(r1 == 0,               "as_load(1) → 0 (evict+load)");
    CHECK(s.hot_idx == 1,        "hot_idx = 1");
    CHECK(s.agents[0].state == AS_COLD, "agente 0 tornato COLD");
    CHECK(s.agents[1].state == AS_HOT,  "agente 1 diventato HOT");
    CHECK(s.cache_misses == 2,   "cache_misses = 2");

    /* cache hit agente 1 */
    as_load(&s, 1);
    CHECK(s.cache_hits == 2,     "cache_hits = 2 (hit su agente 1)");
}

/* ════════════════════════════════════════════════════════════════════════
   T05 — as_evict
   ════════════════════════════════════════════════════════════════════════ */
static void t05_evict(void) {
    SECTION("T05 — as_evict: scaricamento HOT");
    HWInfo hw = make_hw(8192, 16384);
    AgentScheduler s; as_init(&s, &hw);
    as_add(&s, "a", "A", "/models/a.gguf", 2);

    /* evict su scheduler vuoto → nessun crash */
    as_evict(&s);
    CHECK(s.hot_idx == -1, "evict su hot_idx=-1 → nessun crash");

    /* carica e poi evict */
    as_load(&s, 0);
    CHECK(s.hot_idx == 0, "hot_idx=0 dopo load");
    as_evict(&s);
    CHECK(s.hot_idx == -1,           "hot_idx=-1 dopo evict");
    CHECK(s.agents[0].state == AS_COLD, "agente torna COLD dopo evict");

    /* doppio evict → nessun crash */
    as_evict(&s);
    CHECK(s.hot_idx == -1, "doppio evict → nessun crash, hot_idx=-1");
}

/* ════════════════════════════════════════════════════════════════════════
   T06 — as_record: call_count, EMA latenza, promozione sticky-hot
   ════════════════════════════════════════════════════════════════════════ */
static void t06_record(void) {
    SECTION("T06 — as_record: statistiche e promozione");
    HWInfo hw = make_hw(8192, 16384);
    AgentScheduler s; as_init(&s, &hw);
    as_add(&s, "prog", "\xf0\x9f\x92\xbb", "/m.gguf", 2);
    as_add(&s, "vuoto", "V", "", 2);

    /* idx invalido → no crash */
    as_record(&s, -1, 1.0);
    as_record(&s, 99, 1.0);
    CHECK(s.agents[0].call_count == 0, "idx invalido: call_count invariato");

    /* prima chiamata: avg_latency = latenza stessa */
    as_record(&s, 0, 2.0);
    CHECK(s.agents[0].call_count == 1,       "call_count = 1");
    CHECK(fabs(s.agents[0].avg_latency_s - 2.0) < 1e-9,
          "prima latenza: avg = latenza (init diretto)");
    CHECK(s.agents[0].priority == 2,         "priority ancora 2 (1 chiamata)");

    /* seconda chiamata: EMA α=0.3 */
    as_record(&s, 0, 1.0);
    double expected = 0.7*2.0 + 0.3*1.0;  /* = 1.7 */
    CHECK(fabs(s.agents[0].avg_latency_s - expected) < 1e-9,
          "EMA latenza corretta: 0.7*2.0 + 0.3*1.0 = 1.7");
    CHECK(s.agents[0].call_count == 2, "call_count = 2");

    /* terza chiamata → promozione a sticky-hot (AS_STICKY_THRESHOLD=3) */
    as_record(&s, 0, 1.0);
    CHECK(s.agents[0].call_count == 3, "call_count = 3");
    CHECK(s.agents[0].priority == 1,   "promosso a sticky-hot (priority=1) a 3 chiamate");

    /* quarta: già sticky-hot → priority resta 1 */
    as_record(&s, 0, 1.0);
    CHECK(s.agents[0].priority == 1, "priority resta 1 dopo la promozione");
    CHECK(s.agents[0].call_count == 4, "call_count = 4");

    /* last_used aggiornato */
    time_t before = time(NULL);
    as_record(&s, 0, 0.5);
    CHECK(s.agents[0].last_used >= before, "last_used aggiornato");
}

/* ════════════════════════════════════════════════════════════════════════
   T07 — as_sequential
   ════════════════════════════════════════════════════════════════════════ */
static void t07_sequential(void) {
    SECTION("T07 — as_sequential: flag coerente");
    {
        HWInfo hw = make_hw(8192, 16384);
        AgentScheduler s; as_init(&s, &hw);
        CHECK(as_sequential(&s) == 0, "VRAM 8GB: non sequenziale");
    }
    {
        HWInfo hw = make_hw(2048, 8192);
        AgentScheduler s; as_init(&s, &hw);
        CHECK(as_sequential(&s) == 1, "VRAM 2GB: sequenziale");
    }
    {
        HWInfo hw = make_hw_no_gpu(16384);
        AgentScheduler s; as_init(&s, &hw);
        CHECK(as_sequential(&s) == 1, "No GPU: sequenziale");
    }
}

/* ════════════════════════════════════════════════════════════════════════
   T08 — as_save_vram_profile + as_load_vram_profile
   ════════════════════════════════════════════════════════════════════════ */
static void t08_vram_profile(void) {
    SECTION("T08 — vram_profile: round-trip save/load JSON");
    const char *tmpfile = "/tmp/test_vram_profile.json";

    HWInfo hw = make_hw(8192, 16384);
    AgentScheduler s; as_init(&s, &hw);
    as_add(&s, "ricercatore", "R", "/models/qwen3.gguf",   2);
    as_add(&s, "programmatore","P","/models/coder.gguf",   1);
    as_add(&s, "tester",      "T", "/models/tester.gguf",  3);

    /* Imposta misure VRAM simulate */
    s.agents[0].vram_delta_mb = 3500;
    s.agents[1].vram_delta_mb = 4200;
    s.agents[2].vram_delta_mb = 2800;

    int ok = as_save_vram_profile(&s, tmpfile, 8192);
    CHECK(ok == 0, "as_save_vram_profile → 0 (ok)");

    /* Ricarica in nuovo scheduler */
    AgentScheduler s2; as_init(&s2, &hw);
    as_add(&s2, "ricercatore", "R", "/models/qwen3.gguf",   2);
    as_add(&s2, "programmatore","P","/models/coder.gguf",   1);
    as_add(&s2, "tester",      "T", "/models/tester.gguf",  3);

    int found = as_load_vram_profile(&s2, tmpfile);
    CHECK(found == 3, "as_load_vram_profile: trovate 3 voci");
    CHECK(s2.agents[0].vram_delta_mb == 3500, "qwen3.gguf: vram_delta_mb = 3500");
    CHECK(s2.agents[1].vram_delta_mb == 4200, "coder.gguf: vram_delta_mb = 4200");
    CHECK(s2.agents[2].vram_delta_mb == 2800, "tester.gguf: vram_delta_mb = 2800");

    /* File inesistente → ritorna -1 */
    int err = as_load_vram_profile(&s2, "/tmp/nonexistent_xyzzy.json");
    CHECK(err == -1, "file inesistente → -1");

    remove(tmpfile);
}

/* ════════════════════════════════════════════════════════════════════════
   T09 — Edge case
   ════════════════════════════════════════════════════════════════════════ */
static void t09_edge(void) {
    SECTION("T09 — Edge case: scheduler vuoto, idx invalidi");
    HWInfo hw = make_hw(8192, 16384);
    AgentScheduler s; as_init(&s, &hw);

    /* Operazioni su scheduler vuoto */
    CHECK(as_load(&s, 0) == -1,   "load(0) su scheduler vuoto → -1");
    as_evict(&s);                  /* no crash */
    CHECK(s.hot_idx == -1,         "evict su scheduler vuoto → hot_idx=-1");
    as_record(&s, 0, 1.0);         /* no crash */
    CHECK(s.n == 0,                "n=0 dopo operazioni su vuoto");
    as_assign_layers(&s);          /* no crash */

    /* Agente con nome lungo (truncato a AS_NAME_MAX-1) */
    char long_name[200];
    memset(long_name, 'X', sizeof long_name - 1);
    long_name[sizeof long_name - 1] = '\0';
    as_add(&s, long_name, "", "/m.gguf", 2);
    CHECK(strlen(s.agents[0].name) == AS_NAME_MAX - 1,
          "nome lungo: truncato a AS_NAME_MAX-1");

    /* Path lungo (truncato a AS_PATH_MAX-1) */
    AgentScheduler s2; as_init(&s2, &hw);
    char long_path[600];
    memset(long_path, 'P', sizeof long_path - 1);
    long_path[sizeof long_path - 1] = '\0';
    as_add(&s2, "a", "", long_path, 2);
    CHECK(strlen(s2.agents[0].model_path) == AS_PATH_MAX - 1,
          "path lungo: truncato a AS_PATH_MAX-1");
}

/* ════════════════════════════════════════════════════════════════════════
   T10 — Stress: 1000 load+record consecutivi
   ════════════════════════════════════════════════════════════════════════ */
static void t10_stress(void) {
    SECTION("T10 — Stress: 1000 cicli load+record");
    HWInfo hw = make_hw(8192, 16384);
    AgentScheduler s; as_init(&s, &hw);

    /* 6 agenti (come la pipeline reale) */
    as_add(&s, "ricercatore",  "R", "/m/a.gguf", 2);
    as_add(&s, "pianificatore","P", "/m/b.gguf", 2);
    as_add(&s, "programmA",    "A", "/m/c.gguf", 2);
    as_add(&s, "programmB",    "B", "/m/d.gguf", 2);
    as_add(&s, "tester",       "T", "/m/e.gguf", 2);
    as_add(&s, "ottimizzatore","O", "/m/f.gguf", 2);

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    /* Ogni agente viene chiamato DUE volte consecutivamente:
     * prima load = miss, seconda load = cache hit (stesso hot_idx).
     * Pattern: 0,0,1,1,2,2,3,3,4,4,5,5,0,0,... → 500 miss + 500 hit */
    for (int iter = 0; iter < 1000; iter++) {
        int idx = (iter / 2) % s.n;
        as_load(&s, idx);
        as_record(&s, idx, 0.1 + (double)(iter%10)*0.05);
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);
    double ms = (t1.tv_sec-t0.tv_sec)*1000.0 + (t1.tv_nsec-t0.tv_nsec)/1e6;

    CHECK(ms < 200.0, "1000 cicli load+record in < 200ms");
    CHECK(s.cache_hits == 500, "500 cache hit (ogni agente 2 volte di fila)");

    /* Verifica che tutti gli agenti abbiano latenza registrata */
    int all_have_latency = 1;
    for (int i = 0; i < s.n; i++)
        if (s.agents[i].avg_latency_s <= 0.0) all_have_latency = 0;
    CHECK(all_have_latency, "tutti gli agenti hanno avg_latency_s > 0 dopo stress");

    /* Promozione sticky-hot: ogni agente viene chiamato ≥3 volte (1000/6 = 166) */
    int all_promoted = 1;
    for (int i = 0; i < s.n; i++)
        if (s.agents[i].priority != 1) all_promoted = 0;
    CHECK(all_promoted, "tutti gli agenti promossi a sticky-hot dopo 166+ chiamate ciascuno");

    printf("  Tempo: %.2f ms per 1000 cicli load+record\n", ms);
    printf("  Cache hits: %d  Cache misses: %d\n", s.cache_hits, s.cache_misses);
}

/* ════════════════════════════════════════════════════════════════════════
   MAIN
   ════════════════════════════════════════════════════════════════════════ */
int main(void) {
    printf("\n\033[1m══ test_agent_scheduler.c — Hot/Cold Scheduler ══\033[0m\n");
    printf("   AS_MAX_AGENTS=%d  AS_STICKY_THRESHOLD=%d  AS_SEQUENTIAL_VRAM_MB=%lld MB\n",
           AS_MAX_AGENTS, AS_STICKY_THRESHOLD, AS_SEQUENTIAL_VRAM_MB);

    t01_init();
    t02_add();
    t03_assign_layers();
    t04_load();
    t05_evict();
    t06_record();
    t07_sequential();
    t08_vram_profile();
    t09_edge();
    t10_stress();

    printf("\n\033[1m══════════════════════════════════════════════════════\033[0m\n");
    if (_fail == 0)
        printf("\033[1m\033[32m  RISULTATI: %d/%d PASSATI — SCHEDULER A PROVA DI BOMBA\033[0m\n",
               _pass, _pass+_fail);
    else
        printf("\033[1m\033[31m  RISULTATI: %d/%d PASSATI — %d FALLITI\033[0m\n",
               _pass, _pass+_fail, _fail);
    printf("\033[1m══════════════════════════════════════════════════════\033[0m\n\n");
    return _fail > 0 ? 1 : 0;
}
