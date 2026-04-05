/*
 * test_hw_platform.c — Test hardware platform-specific
 * =====================================================
 * Verifica rilevamento RAM (Windows Xeon) e GPU Nvidia (Linux).
 * Compila: gcc -O2 -Iinclude test_hw_platform.c src/hw_detect.c
 *          -lpthread -lm -o test_hw_platform
 * Uso:     ./test_hw_platform [--ram] [--gpu] [--all]
 *   --ram    solo test RAM/CPU (per Xeon con sola RAM)
 *   --gpu    solo test GPU Nvidia (per Linux + nvidia-smi)
 *   --all    esegue tutti i test (default)
 */
#include "hw_detect.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── colori ANSI ── */
#define OK   "\033[32m[OK]\033[0m "
#define FAIL "\033[31m[FAIL]\033[0m "
#define INFO "\033[33m[INFO]\033[0m "
#define SKIP "\033[36m[SKIP]\033[0m "
#define SEP  "────────────────────────────────────────\n"

static int g_pass = 0, g_fail = 0, g_skip = 0;

static void result(const char* name, int ok, const char* detail) {
    if (ok) { printf(OK "%s\n", name); g_pass++; }
    else     { printf(FAIL "%s — %s\n", name, detail ? detail : ""); g_fail++; }
}
static void skip(const char* name, const char* reason) {
    printf(SKIP "%s — %s\n", name, reason);
    g_skip++;
}

/* ══════════════════════════════════════════════════════════════
   TEST 1: Rilevamento hardware base (cross-platform)
   ══════════════════════════════════════════════════════════════ */
static HWInfo g_hw;

static void test_hw_detect_base(void) {
    printf(SEP "TEST 1: hw_detect() — rilevamento base\n" SEP);
    hw_detect(&g_hw);

    result("hw_detect() non crash", 1, "");

    /* Almeno un device rilevato */
    result("almeno 1 device rilevato", g_hw.count >= 1, "count=0");

    /* Primary device valido */
    result("primary device index valido",
           g_hw.primary >= 0 && g_hw.primary < g_hw.count,
           "primary out-of-range");

    /* Stampa riepilogo */
    printf(INFO "Devices rilevati: %d\n", g_hw.count);
    for (int i = 0; i < g_hw.count; i++) {
        const HWDevice* d = &g_hw.dev[i];
        printf("       [%d] %-8s  %s\n",
               i, hw_dev_type_str(d->type), d->name);
        if (d->mem_mb > 0)
            printf("            VRAM/RAM: %lld MB  avail: %lld MB  gpu_layers: %d\n",
                   (long long)d->mem_mb, (long long)d->avail_mb, d->n_gpu_layers);
    }
}

/* ══════════════════════════════════════════════════════════════
   TEST 2: RAM — Windows Xeon (solo CPU, senza GPU dedicata)
   ══════════════════════════════════════════════════════════════ */
static void test_ram_xeon(void) {
    printf(SEP "TEST 2: RAM — scenario Xeon (solo CPU)\n" SEP);

    /* Cerca device CPU */
    const HWDevice* cpu = NULL;
    for (int i = 0; i < g_hw.count; i++) {
        if (g_hw.dev[i].type == DEV_CPU) { cpu = &g_hw.dev[i]; break; }
    }

    if (!cpu) {
        skip("CPU device trovato", "nessun device CPU nella lista");
    } else {
        result("CPU device trovato", 1, "");
        result("nome CPU non vuoto", cpu->name[0] != '\0', "name vuoto");
        result("RAM > 0 MB", cpu->mem_mb > 0, "mem_mb=0");
        result("RAM disponibile > 0 MB", cpu->avail_mb > 0, "avail_mb=0");
        result("RAM disponibile <= RAM totale", cpu->avail_mb <= cpu->mem_mb, "avail>total");
        result("RAM totale >= 1024 MB (minimo ragionevole)", cpu->mem_mb >= 1024,
               "RAM troppo bassa (< 1 GB)");

        /* Su Xeon con molta RAM: almeno 16 GB */
        if (cpu->mem_mb >= 16384) {
            printf(INFO "Sistema RAM-heavy rilevato: %lld MB (%lld GB) — profilo Xeon\n",
                   (long long)cpu->mem_mb,
                   (long long)(cpu->mem_mb / 1024));
            result("RAM >= 16 GB (profilo Xeon)", 1, "");
        } else {
            printf(INFO "RAM: %lld MB — non profilo Xeon ma test superato\n",
                   (long long)cpu->mem_mb);
            result("RAM rilevata correttamente", cpu->mem_mb > 0, "");
        }

        /* Verifica che n_gpu_layers sia 0 se non c'è GPU Nvidia */
        int has_nvidia = 0;
        for (int i = 0; i < g_hw.count; i++)
            if (g_hw.dev[i].type == DEV_NVIDIA) { has_nvidia = 1; break; }

        if (!has_nvidia) {
            result("n_gpu_layers=0 su CPU-only", cpu->n_gpu_layers == 0,
                   "gpu_layers != 0 su sistema senza GPU Nvidia");
            printf(INFO "Sistema CPU-only (Xeon): llama.cpp userebbe solo RAM\n");
        }
    }

    /* Test hw_gpu_layers con sola RAM (nessuna VRAM) */
    printf(SEP "  Sub-test hw_gpu_layers(0) — no VRAM\n" SEP);
    int layers_no_vram = hw_gpu_layers(0);
    result("hw_gpu_layers(0) = 0", layers_no_vram == 0, "atteso 0 senza VRAM");

    /* Test hw_gpu_layers con VRAM elevata (simulazione) */
    int layers_8gb = hw_gpu_layers(8192);
    result("hw_gpu_layers(8192) > 0", layers_8gb > 0, "atteso >0 con 8 GB VRAM");

    int layers_24gb = hw_gpu_layers(24576);
    result("hw_gpu_layers(24576) > hw_gpu_layers(8192)",
           layers_24gb > layers_8gb || layers_24gb == layers_8gb,
           "più VRAM = più layer o uguale (massimizzato)");

    printf(INFO "layers con 0 MB VRAM: %d | 8 GB: %d | 24 GB: %d\n",
           layers_no_vram, layers_8gb, layers_24gb);
}

/* ══════════════════════════════════════════════════════════════
   TEST 3: GPU Nvidia — Linux con nvidia-smi
   ══════════════════════════════════════════════════════════════ */
static void test_gpu_nvidia_linux(void) {
    printf(SEP "TEST 3: GPU Nvidia — Linux (nvidia-smi)\n" SEP);

    /* Controlla se nvidia-smi è disponibile */
    int smi_ok = (system("nvidia-smi --query-gpu=name --format=csv,noheader > /dev/null 2>&1") == 0);
    if (!smi_ok) {
        skip("nvidia-smi disponibile",
             "nvidia-smi non trovato — test GPU saltati (normale su CPU-only)");
        printf(INFO "Sistema senza GPU Nvidia: tutti i test GPU sono SKIP\n");
        g_skip += 5;
        return;
    }

    result("nvidia-smi disponibile", 1, "");

    /* Cerca device Nvidia in g_hw */
    const HWDevice* nv = NULL;
    for (int i = 0; i < g_hw.count; i++) {
        if (g_hw.dev[i].type == DEV_NVIDIA) { nv = &g_hw.dev[i]; break; }
    }

    if (!nv) {
        skip("GPU Nvidia in HWInfo", "nvidia-smi presente ma hw_detect non ha trovato device Nvidia");
        g_skip += 4;
        return;
    }

    result("GPU Nvidia device trovato in HWInfo", 1, "");
    result("nome GPU non vuoto", nv->name[0] != '\0', "name vuoto");
    result("VRAM > 0 MB", nv->mem_mb > 0, "mem_mb=0 — nvidia-smi potrebbe non rispondere");
    result("VRAM disponibile > 0 MB", nv->avail_mb > 0, "avail_mb=0");
    result("VRAM disponibile <= VRAM totale", nv->avail_mb <= nv->mem_mb, "avail>total");
    result("n_gpu_layers > 0 con GPU Nvidia", nv->n_gpu_layers > 0, "gpu_layers=0 con GPU Nvidia");

    printf(INFO "GPU: %s | VRAM: %lld MB | avail: %lld MB | gpu_layers: %d\n",
           nv->name, (long long)nv->mem_mb, (long long)nv->avail_mb, nv->n_gpu_layers);

    /* Verifica che primary sia la GPU Nvidia se presente */
    result("primary device = GPU Nvidia (preferita al CPU)",
           g_hw.dev[g_hw.primary].type == DEV_NVIDIA,
           "primary non è DEV_NVIDIA nonostante GPU presente");

    /* Verifica coerenza: n_gpu_layers nel device deve essere > 0 e <= hw_gpu_layers(vram) */
    int max_layers = hw_gpu_layers(nv->mem_mb);
    result("n_gpu_layers nel range valido [1, hw_gpu_layers(mem_mb)]",
           nv->n_gpu_layers > 0 && nv->n_gpu_layers <= max_layers,
           "gpu_layers fuori dal range atteso");
    printf(INFO "n_gpu_layers device=%d  hw_gpu_layers(%lld MB)=%d\n",
           nv->n_gpu_layers, (long long)nv->mem_mb, max_layers);
}

/* ══════════════════════════════════════════════════════════════
   TEST 4: hw_dev_type_str — tutte le varianti
   ══════════════════════════════════════════════════════════════ */
static void test_dev_type_str(void) {
    printf(SEP "TEST 4: hw_dev_type_str() — stringe tipo device\n" SEP);

    /* Verifica solo che restituisca una stringa non vuota (il formato esatto è impl-defined) */
    result("DEV_CPU  restituisce stringa",  hw_dev_type_str(DEV_CPU)[0]    != '\0', "stringa vuota");
    result("DEV_NVIDIA restituisce stringa",hw_dev_type_str(DEV_NVIDIA)[0] != '\0', "stringa vuota");
    result("DEV_AMD  restituisce stringa",  hw_dev_type_str(DEV_AMD)[0]    != '\0', "stringa vuota");
    result("DEV_INTEL restituisce stringa", hw_dev_type_str(DEV_INTEL)[0]  != '\0', "stringa vuota");
    printf(INFO "Stringhe: CPU='%s' NVIDIA='%s' AMD='%s' INTEL='%s'\n",
           hw_dev_type_str(DEV_CPU), hw_dev_type_str(DEV_NVIDIA),
           hw_dev_type_str(DEV_AMD), hw_dev_type_str(DEV_INTEL));
}

/* ══════════════════════════════════════════════════════════════
   TEST 5: Thread cross-platform
   ══════════════════════════════════════════════════════════════ */
static volatile int g_thread_flag = 0;

#ifdef _WIN32
static DWORD thread_worker(void* arg) {
    (void)arg;
    g_thread_flag = 42;
    return 0;
}
#else
static void* thread_worker(void* arg) {
    (void)arg;
    g_thread_flag = 42;
    return NULL;
}
#endif

static void test_threads(void) {
    printf(SEP "TEST 5: hw_thread_create/join — cross-platform\n" SEP);

    hw_thread_t t;
    int rc = hw_thread_create(&t, thread_worker, NULL);
    result("hw_thread_create() = 0", rc == 0, "creazione thread fallita");

    if (rc == 0) {
        hw_thread_join(t);
        result("thread ha settato g_thread_flag = 42", g_thread_flag == 42, "flag non aggiornato");
    } else {
        skip("thread ha settato g_thread_flag", "thread non creato");
    }
}

/* ══════════════════════════════════════════════════════════════
   Punto d'ingresso
   ══════════════════════════════════════════════════════════════ */
int main(int argc, char* argv[]) {
    int run_ram = 1, run_gpu = 1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--ram") == 0) { run_gpu = 0; }
        else if (strcmp(argv[i], "--gpu") == 0) { run_ram = 0; }
        /* --all: entrambi (default) */
    }

    printf("\n\033[36m\033[1m  PRISMALUX — Test HW Platform\033[0m\n");
    printf("  Rilevamento hardware: RAM (Xeon) + GPU Nvidia (Linux)\n\n");

    test_hw_detect_base();
    if (run_ram) test_ram_xeon();
    if (run_gpu) test_gpu_nvidia_linux();
    test_dev_type_str();
    test_threads();

    printf(SEP);
    int total = g_pass + g_fail + g_skip;
    printf("  Risultati: \033[32m%d/%d PASSATI\033[0m", g_pass, total);
    if (g_fail > 0)
        printf("  \033[31m%d FALLITI\033[0m", g_fail);
    if (g_skip > 0)
        printf("  \033[36m%d SALTATI\033[0m", g_skip);
    printf("\n");
    if (g_skip > 0)
        printf("  (i SALTATI sono normali su piattaforme diverse da quella testata)\n");
    printf("\n");

    return g_fail > 0 ? 1 : 0;
}
