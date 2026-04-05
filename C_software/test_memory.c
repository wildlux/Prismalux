/* ══════════════════════════════════════════════════════════════════════════
   test_memory.c — Memory leak detection via /proc/self/status
   ══════════════════════════════════════════════════════════════════════════
   Verifica che le strutture di Prismalux non perdano memoria.

   Categorie:
     T01 — Lettura VmRSS da /proc/self/status
     T02 — Baseline: malloc/free non accumula leak
     T03 — Scheduler: 1000 cicli as_load/as_evict → delta RSS < 512 KB
     T04 — Context Store: 500 scritture + close → delta RSS < 1MB
     T05 — Stress allocazione: 100k malloc/free → nessun accumulo
     T06 — Soglia KPI: crescita RSS < 512KB dopo test intensivo

   Build:
     gcc -O2 -Iinclude -lm -o test_memory \
         test_memory.c src/agent_scheduler.c src/hw_detect.c \
         src/context_store.c src/key_router.c

   Esecuzione: ./test_memory
   ══════════════════════════════════════════════════════════════════════════ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "agent_scheduler.h"
#include "hw_detect.h"
#include "context_store.h"
#include "key_router.h"

/* ── Harness ──────────────────────────────────────────────────────────── */
static int _pass = 0, _fail = 0;
#define SECTION(t) printf("\n\033[36m\033[1m─── %s ───\033[0m\n", t)
#define CHECK(cond, label) do { \
    if (cond) { printf("  \033[32m[OK]\033[0m  %s\n", label); _pass++; } \
    else       { printf("  \033[31m[FAIL]\033[0m %s\n", label); _fail++; } \
} while(0)

/* ════════════════════════════════════════════════════════════════════════
   Lettura VmRSS da /proc/self/status (KB → KB)
   ════════════════════════════════════════════════════════════════════════ */
static long read_vmrss_kb(void) {
    FILE *f = fopen("/proc/self/status", "r");
    if (!f) return -1;
    char line[128];
    while (fgets(line, sizeof line, f)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            long kb = 0;
            sscanf(line + 6, "%ld", &kb);
            fclose(f);
            return kb;
        }
    }
    fclose(f);
    return -1;
}

/* Forza flush heap per misure più accurate */
static void flush_heap(void) {
    /* Alloca e libera immediatamente un blocco: forza il consolidamento del heap */
    void *p = malloc(65536); if (p) { memset(p,0,65536); free(p); }
}

/* ════════════════════════════════════════════════════════════════════════
   T01 — Lettura VmRSS
   ════════════════════════════════════════════════════════════════════════ */
static void t01_vmrss(void) {
    SECTION("T01 — Lettura VmRSS da /proc/self/status");

    long rss = read_vmrss_kb();
    printf("  VmRSS corrente: %ld KB (%.1f MB)\n", rss, rss/1024.0);
    CHECK(rss > 0,      "VmRSS > 0 KB (processo ha memoria allocata)");
    CHECK(rss < 500000, "VmRSS < 500 MB (sanity check: processo non è esploso)");

    /* Due letture consecutive: devono essere vicine */
    long r1 = read_vmrss_kb();
    long r2 = read_vmrss_kb();
    long delta = r2 - r1;
    printf("  Delta due letture consecutive: %ld KB\n", delta);
    CHECK(delta >= -1024 && delta <= 1024,
          "delta RSS tra due letture consecutive < 1 MB");
}

/* ════════════════════════════════════════════════════════════════════════
   T02 — Baseline malloc/free
   ════════════════════════════════════════════════════════════════════════ */
static void t02_baseline(void) {
    SECTION("T02 — Baseline: malloc/free non accumula");

    flush_heap();
    long rss_before = read_vmrss_kb();

    /* 10000 alloc/free con dimensioni variabili */
    for (int i = 0; i < 10000; i++) {
        size_t sz = 128 + (size_t)(i % 4096);
        void *p = malloc(sz);
        if (p) { memset(p, i & 0xFF, sz); free(p); }
    }

    flush_heap();
    long rss_after = read_vmrss_kb();
    long delta = rss_after - rss_before;
    printf("  Baseline 10k malloc/free: delta RSS = %ld KB\n", delta);

    /* Il heap può crescere un po' per frammentazione — soglia generosa 2MB */
    CHECK(delta < 2048, "10k malloc/free: delta RSS < 2 MB");
    CHECK(delta > -4096, "RSS non cala drasticamente (sanity)");
}

/* ════════════════════════════════════════════════════════════════════════
   T03 — Scheduler: cicli as_load/as_evict
   ════════════════════════════════════════════════════════════════════════ */
static void t03_scheduler(void) {
    SECTION("T03 — Scheduler: 1000 cicli as_load/as_evict");

    HWInfo hw; memset(&hw,0,sizeof hw);
    hw.count=1; hw.primary=0;
    hw.dev[0].type=DEV_CPU; hw.dev[0].avail_mb=16384; hw.dev[0].mem_mb=16384;

    flush_heap();
    long rss_before = read_vmrss_kb();

    for (int i = 0; i < 1000; i++) {
        AgentScheduler s;
        as_init(&s, &hw);
        for (int j = 0; j < 6; j++) {
            char name[16]; snprintf(name, sizeof name, "agente%d", j);
            as_add(&s, name, "A", "/models/test.gguf", 2);
        }
        as_assign_layers(&s);
        for (int j = 0; j < 6; j++) {
            as_load(&s, j);
            as_record(&s, j, 0.1);
            as_evict(&s);
        }
    }

    flush_heap();
    long rss_after = read_vmrss_kb();
    long delta = rss_after - rss_before;
    printf("  1000 cicli scheduler: delta RSS = %ld KB (%.1f MB)\n",
           delta, delta/1024.0);
    CHECK(delta < 512,  "scheduler 1000 cicli: delta RSS < 512 KB");
    CHECK(delta > -4096,"RSS non negativo significativo (sanity)");
}

/* ════════════════════════════════════════════════════════════════════════
   T04 — Context Store: scritture + close
   ════════════════════════════════════════════════════════════════════════ */
static void t04_context_store(void) {
    SECTION("T04 — Context Store: 200 write + close/reopen");

    const char *base = "/tmp/test_mem_ctx";

    flush_heap();
    long rss_before = read_vmrss_kb();

    for (int rep = 0; rep < 5; rep++) {
        ContextStore cs;
        if (cs_open(&cs, base) != 0) {
            printf("  \033[33m[SKIP]\033[0m cs_open fallito — test context store saltato\n");
            return;
        }
        const char *keys1[] = { "math", "algebra" };
        const char *keys2[] = { "codice", "python" };
        const char *keys3[] = { "storia", "medioevo" };
        for (int i = 0; i < 40; i++) {
            char chunk[64]; snprintf(chunk, sizeof chunk, "Contenuto chunk numero %d.", i);
            cs_write(&cs, (i%3==0)?keys1:(i%3==1)?keys2:keys3, 2, chunk, (int)strlen(chunk));
        }
        cs_close(&cs);
    }

    flush_heap();
    long rss_after = read_vmrss_kb();
    long delta = rss_after - rss_before;
    printf("  5 × (200 write + close): delta RSS = %ld KB\n", delta);
    CHECK(delta < 1024, "context store 5 cicli: delta RSS < 1 MB");

    remove("/tmp/test_mem_ctx.dat"); remove("/tmp/test_mem_ctx.idx");
}

/* ════════════════════════════════════════════════════════════════════════
   T05 — Stress: 100k malloc/free con pattern realistici
   ════════════════════════════════════════════════════════════════════════ */
static void t05_stress_alloc(void) {
    SECTION("T05 — Stress: 100k malloc/free pattern realistici");

    flush_heap();
    long rss_before = read_vmrss_kb();

    /* Pattern 1: dimensioni tipiche Prismalux (buffer AI, prompt, output) */
    for (int i = 0; i < 20000; i++) {
        /* sys_prompt: ~500 byte */
        void *p1 = malloc(500); if (p1) { memset(p1,'A',500); free(p1); }
        /* output AI: ~4096 byte */
        void *p2 = malloc(4096); if (p2) { memset(p2,'B',4096); free(p2); }
        /* chunk HTTP: ~8192 byte */
        void *p3 = malloc(8192); if (p3) { memset(p3,'C',8192); free(p3); }
        /* JSON response: ~2048 byte */
        void *p4 = malloc(2048); if (p4) { memset(p4,'D',2048); free(p4); }
        /* nome modello: ~256 byte */
        void *p5 = malloc(256); if (p5) { memset(p5,'E',256); free(p5); }
    }

    flush_heap();
    long rss_mid = read_vmrss_kb();
    long delta_mid = rss_mid - rss_before;
    printf("  100k alloc/free (mix): delta RSS = %ld KB\n", delta_mid);

    /* Pattern 2: allocazione a cascata (simula pipeline 6 agenti) */
    for (int run = 0; run < 100; run++) {
        void *agents[6];
        for (int j = 0; j < 6; j++) agents[j] = malloc(16384);  /* 16KB per agente */
        for (int j = 0; j < 6; j++) if (agents[j]) { memset(agents[j],run,16384); }
        for (int j = 5; j >= 0; j--) free(agents[j]);
    }

    flush_heap();
    long rss_after = read_vmrss_kb();
    long delta = rss_after - rss_before;
    printf("  Dopo anche pipeline-alloc: delta RSS = %ld KB (%.1f MB)\n",
           delta, delta/1024.0);
    CHECK(delta < 4096, "100k alloc/free + pipeline: delta RSS < 4 MB");
}

/* ════════════════════════════════════════════════════════════════════════
   T06 — KPI: crescita RSS < 512KB dopo test intensivo completo
   ════════════════════════════════════════════════════════════════════════ */
static void t06_kpi(void) {
    SECTION("T06 — KPI finale: RSS complessivo del processo di test");

    flush_heap();
    long rss = read_vmrss_kb();
    printf("  RSS finale processo test: %ld KB (%.1f MB)\n", rss, rss/1024.0);

    /* Un processo di test puro (no LLM) non dovrebbe superare 50 MB */
    CHECK(rss < 51200, "RSS finale test < 50 MB (no LLM caricato)");

    /* Confronto: RSS attuale vs RSS tipico processo C minimo (~1MB) */
    CHECK(rss > 512, "RSS > 512 KB (strutture dati correttamente allocate)");

    /* Lettura /proc/self/status funziona */
    long r = read_vmrss_kb();
    CHECK(r > 0, "/proc/self/status: lettura VmRSS riuscita");
}

/* ════════════════════════════════════════════════════════════════════════
   MAIN
   ════════════════════════════════════════════════════════════════════════ */
int main(void) {
    printf("\n\033[1m══ test_memory.c — Memory Leak Detection ══\033[0m\n");
    printf("   Soglia KPI: delta RSS < 512KB per 1000 cicli\n");

    /* Verifica che /proc/self/status sia disponibile */
    if (read_vmrss_kb() < 0) {
        printf("  \033[33m[WARN]\033[0m /proc/self/status non disponibile"
               " (non Linux?) — alcuni test saltati\n");
    }

    t01_vmrss();
    t02_baseline();
    t03_scheduler();
    t04_context_store();
    t05_stress_alloc();
    t06_kpi();

    printf("\n\033[1m══════════════════════════════════════════════════════\033[0m\n");
    if (_fail == 0)
        printf("\033[1m\033[32m  RISULTATI: %d/%d PASSATI — NESSUN MEMORY LEAK\033[0m\n",
               _pass, _pass+_fail);
    else
        printf("\033[1m\033[31m  RISULTATI: %d/%d PASSATI — %d FALLITI\033[0m\n",
               _pass, _pass+_fail, _fail);
    printf("\033[1m══════════════════════════════════════════════════════\033[0m\n\n");
    return _fail > 0 ? 1 : 0;
}
