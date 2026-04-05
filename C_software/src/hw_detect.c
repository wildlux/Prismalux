/*
 * hw_detect.c — Rilevamento hardware cross-platform (CPU + GPU + RAM/VRAM)
 * =========================================================================
 * Strategia di rilevamento per piattaforma:
 *
 *   CPU / RAM
 *     Linux   → /proc/meminfo (MemTotal, MemAvailable) + /proc/cpuinfo
 *     Windows → GlobalMemoryStatusEx() + wmic cpu get Name
 *
 *   NVIDIA GPU (tutte le piattaforme)
 *     nvidia-smi --query-gpu=name,memory.total --format=csv,noheader,nounits
 *     Riserva 15% VRAM per driver/contesto OS.
 *
 *   AMD GPU
 *     Linux   → /sys/class/drm/card[N]/device/ (vendor=0x1002, mem_info_vram_total)
 *               Fallback nome GPU: product_name oppure rocm-smi --showproductname
 *     Windows → wmic path Win32_VideoController (filtra AMD/Radeon/ATI)
 *
 * La funzione hw_detect() riempie HWInfo con tutti i device trovati,
 * ordina le GPU per VRAM decrescente e assegna primary/secondary.
 * primary  = GPU con più VRAM disponibile (preferita per llama.cpp)
 * secondary = seconda GPU o CPU (per split layer o fallback)
 *
 * hw_gpu_layers(vram_mb) restituisce n_gpu_layers ottimale per llama.cpp
 * in base alla VRAM libera stimata.
 */
#include "hw_detect.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  define popen  _popen
#  define pclose _pclose
#  define snprintf _snprintf
#else
#  include <unistd.h>
#  include <dirent.h>
#endif

/* ── utilità ────────────────────────────────────────────────────────── */
static void trim_str(char* s) {
    if (!s || !*s) return;
    char* p = s + strlen(s) - 1;
    while (p >= s && (*p == '\n' || *p == '\r' || *p == ' ' || *p == '\t')) *p-- = '\0';
    char* q = s;
    while (*q == ' ' || *q == '\t') q++;
    if (q != s) memmove(s, q, strlen(q) + 1);
}

static int cmd_exists(const char* cmd) {
#ifdef _WIN32
    char buf[256];
    snprintf(buf, sizeof(buf), "where %s >nul 2>&1", cmd);
#else
    char buf[256];
    snprintf(buf, sizeof(buf), "command -v %s >/dev/null 2>&1", cmd);
#endif
    return system(buf) == 0;
}

/* ── n_gpu_layers ottimali ─────────────────────────────────────────── */
int hw_gpu_layers(long long vram_mb) {
    /* 15% già riservato per driver/OS nel campo avail_mb */
    if (vram_mb >= 22000) return 9999; /* tutto su GPU (qualsiasi modello) */
    if (vram_mb >= 12000) return 80;   /* 70B parziale / 34B completo      */
    if (vram_mb >= 6000)  return 40;   /* 13B completo                     */
    if (vram_mb >= 3500)  return 32;   /* 7B completo                      */
    if (vram_mb >= 2000)  return 20;   /* 7B parziale                      */
    if (vram_mb >= 1000)  return 10;   /* piccolo boost, modelli tiny       */
    return 0;                          /* CPU-only                          */
}

const char* hw_dev_type_str(DevType t) {
    switch (t) {
        case DEV_CPU:    return "CPU";
        case DEV_NVIDIA: return "NVIDIA";
        case DEV_AMD:    return "AMD";
        case DEV_INTEL:  return "INTL";
    }
    return "?";
}

/* ── CPU + RAM ─────────────────────────────────────────────────────── */
static void detect_cpu(HWDevice* dev) {
    memset(dev, 0, sizeof(*dev));
    dev->type      = DEV_CPU;
    dev->gpu_index = -1;

#ifdef _WIN32
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatusEx(&ms);
    dev->mem_mb   = (long long)(ms.ullTotalPhys / (1024ULL * 1024ULL));
    dev->avail_mb = (long long)(ms.ullAvailPhys / (1024ULL * 1024ULL));
    /* riserva 10% per OS */
    dev->avail_mb = dev->avail_mb * 90 / 100;

    /* nome CPU via wmic */
    FILE* f = popen("wmic cpu get Name /format:value 2>nul", "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "Name=", 5) == 0) {
                snprintf(dev->name, sizeof(dev->name), "%s", line + 5);
                trim_str(dev->name);
                break;
            }
        }
        pclose(f);
    }
    if (!dev->name[0]) strncpy(dev->name, "CPU (Windows)", sizeof(dev->name) - 1);
#else
    /* RAM da /proc/meminfo */
    FILE* f = fopen("/proc/meminfo", "r");
    if (f) {
        char line[256];
        long long tot = -1, avail = -1;
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "MemTotal:",    9)  == 0) { sscanf(line + 9,  "%lld", &tot);   tot   /= 1024; }
            if (strncmp(line, "MemAvailable:", 13) == 0) { sscanf(line + 13, "%lld", &avail); avail /= 1024; }
        }
        fclose(f);
        dev->mem_mb   = (tot   > 0) ? tot   : 0;
        dev->avail_mb = (avail > 0) ? avail * 90 / 100 : 0; /* riserva 10% */
    }
    /* nome CPU da /proc/cpuinfo */
    FILE* cp = fopen("/proc/cpuinfo", "r");
    if (cp) {
        char line[256];
        while (fgets(line, sizeof(line), cp)) {
            if (strncmp(line, "model name", 10) == 0) {
                char* c = strchr(line, ':');
                if (c) { c++; while (*c == ' ') c++; strncpy(dev->name, c, sizeof(dev->name) - 1); trim_str(dev->name); }
                break;
            }
        }
        fclose(cp);
    }
    if (!dev->name[0]) strncpy(dev->name, "CPU", sizeof(dev->name) - 1);
#endif
}

/* ── NVIDIA (cross-platform via nvidia-smi) ────────────────────────── */
static int detect_nvidia(HWDevice* out, int max) {
    if (!cmd_exists("nvidia-smi")) return 0;
    int count = 0;
#ifdef _WIN32
    FILE* f = popen("nvidia-smi --query-gpu=name,memory.total --format=csv,noheader,nounits 2>nul", "r");
#else
    FILE* f = popen("nvidia-smi --query-gpu=name,memory.total --format=csv,noheader,nounits 2>/dev/null", "r");
#endif
    if (!f) return 0;
    char line[256];
    while (fgets(line, sizeof(line), f) && count < max) {
        trim_str(line);
        /* formato: "GPU Name, VRAM_MB" — cerco l'ultima virgola */
        char* comma = strrchr(line, ',');
        if (!comma) continue;
        *comma = '\0';
        HWDevice* d = &out[count];
        memset(d, 0, sizeof(*d));
        d->type      = DEV_NVIDIA;
        d->gpu_index = count;
        snprintf(d->name, sizeof(d->name), "%s", line);
        trim_str(d->name);
        d->mem_mb   = strtoll(comma + 1, NULL, 10);
        d->avail_mb = d->mem_mb * 85 / 100; /* riserva 15% per driver */
        d->n_gpu_layers = hw_gpu_layers(d->avail_mb);
        count++;
    }
    pclose(f);
    return count;
}

/* ── AMD Linux (via /sys/class/drm) ────────────────────────────────── */
#ifndef _WIN32
static int detect_amd_linux(HWDevice* out, int max) {
    int count = 0;
    DIR* dr = opendir("/sys/class/drm");
    if (!dr) return 0;
    struct dirent* e;
    int gpu_idx = 0;
    while ((e = readdir(dr)) != NULL && count < max) {
        /* solo card0, card1 ... (no card0-HDMI-A-1 ecc.) */
        if (strncmp(e->d_name, "card", 4) != 0) continue;
        if (strchr(e->d_name + 4, '-')) continue;

        /* verifica vendor AMD (0x1002) */
        char path[512];
        snprintf(path, sizeof(path), "/sys/class/drm/%s/device/vendor", e->d_name);
        FILE* vf = fopen(path, "r");
        if (!vf) continue;
        unsigned vendor = 0;
        { int _r = fscanf(vf, "0x%x", &vendor); (void)_r; }
        fclose(vf);
        if (vendor != 0x1002) { continue; } /* non AMD */

        /* VRAM totale */
        snprintf(path, sizeof(path), "/sys/class/drm/%s/device/mem_info_vram_total", e->d_name);
        FILE* mf = fopen(path, "r");
        if (!mf) { gpu_idx++; continue; }
        unsigned long long vram_bytes = 0;
        { int _r = fscanf(mf, "%llu", &vram_bytes); (void)_r; }
        fclose(mf);
        if (vram_bytes < 64ULL * 1024 * 1024) { gpu_idx++; continue; } /* < 64MB → skip (iGPU tiny) */

        HWDevice* d = &out[count];
        memset(d, 0, sizeof(*d));
        d->type      = DEV_AMD;
        d->gpu_index = gpu_idx++;
        d->mem_mb    = (long long)(vram_bytes / (1024ULL * 1024ULL));
        d->avail_mb  = d->mem_mb * 85 / 100;
        d->n_gpu_layers = hw_gpu_layers(d->avail_mb);

        /* nome GPU */
        snprintf(path, sizeof(path), "/sys/class/drm/%s/device/product_name", e->d_name);
        FILE* nf = fopen(path, "r");
        if (nf) { char* _r = fgets(d->name, sizeof(d->name), nf); (void)_r; fclose(nf); trim_str(d->name); }
        if (!d->name[0]) {
            /* fallback: prova via rocm-smi */
            char cmd[128];
            snprintf(cmd, sizeof(cmd), "rocm-smi --showproductname 2>/dev/null | grep -m1 'Card series'");
            FILE* rf = popen(cmd, "r");
            if (rf) {
                char tmp[256] = {0};
                if (fgets(tmp, sizeof(tmp), rf)) {
                    char* col = strchr(tmp, ':');
                    if (col) { col++; while(*col==' ')col++; strncpy(d->name, col, sizeof(d->name)-1); trim_str(d->name); }
                }
                pclose(rf);
            }
        }
        if (!d->name[0]) snprintf(d->name, sizeof(d->name), "AMD GPU card%s", e->d_name + 4);
        count++;
    }
    closedir(dr);
    return count;
}
#endif

/* ── Intel GPU Linux (via /sys/class/drm, vendor 0x8086) ───────────── */
#ifndef _WIN32
static int detect_intel_linux(HWDevice* out, int max) {
    int count = 0;
    DIR* dr = opendir("/sys/class/drm");
    if (!dr) return 0;
    struct dirent* e;
    while ((e = readdir(dr)) != NULL && count < max) {
        if (strncmp(e->d_name, "card", 4) != 0) continue;
        if (strchr(e->d_name + 4, '-')) continue; /* salta card0-HDMI-A-1 ecc. */

        /* verifica vendor Intel (0x8086) */
        char path[512];
        snprintf(path, sizeof(path), "/sys/class/drm/%s/device/vendor", e->d_name);
        FILE* vf = fopen(path, "r");
        if (!vf) continue;
        unsigned vendor = 0;
        { int _r = fscanf(vf, "0x%x", &vendor); (void)_r; }
        fclose(vf);
        if (vendor != 0x8086) continue; /* non Intel */

        HWDevice* d = &out[count];
        memset(d, 0, sizeof(*d));
        d->type      = DEV_INTEL;
        /* usa il numero DRM direttamente dal nome directory (card0→0, card1→1) */
        d->gpu_index = atoi(e->d_name + 4);

        /* VRAM dedicata — esiste su Intel Arc (xe driver), non su iGPU (i915) */
        snprintf(path, sizeof(path),
                 "/sys/class/drm/%s/device/mem_info_vram_total", e->d_name);
        FILE* mf = fopen(path, "r");
        if (mf) {
            unsigned long long vram_bytes = 0;
            { int _r = fscanf(mf, "%llu", &vram_bytes); (void)_r; }
            fclose(mf);
            d->mem_mb = (long long)(vram_bytes / (1024ULL * 1024ULL));
        }
        /* iGPU senza VRAM dedicata: mem_mb rimane 0 */
        d->avail_mb     = d->mem_mb * 85 / 100;
        d->n_gpu_layers = hw_gpu_layers(d->avail_mb);

        /* nome GPU */
        snprintf(path, sizeof(path),
                 "/sys/class/drm/%s/device/product_name", e->d_name);
        FILE* nf = fopen(path, "r");
        if (nf) {
            char* _r = fgets(d->name, sizeof(d->name), nf);
            (void)_r;
            fclose(nf);
            trim_str(d->name);
        }
        if (!d->name[0])
            snprintf(d->name, sizeof(d->name), "Intel GPU (card%s)", e->d_name + 4);

        count++;
    }
    closedir(dr);
    return count;
}
#endif

/* ── AMD Windows (via wmic) ────────────────────────────────────────── */
#ifdef _WIN32
static int detect_amd_windows(HWDevice* out, int max) {
    int count = 0;
    FILE* f = popen("wmic path Win32_VideoController get Name,AdapterRAM /format:csv 2>nul", "r");
    if (!f) return 0;
    char line[512];
    while (fgets(line, sizeof(line), f) && count < max) {
        trim_str(line);
        if (!line[0]) continue;
        /* CSV: Node,AdapterRAM,Name */
        char* p1 = strchr(line, ',');  if (!p1) continue; p1++;
        char* p2 = strchr(p1,   ',');  if (!p2) continue; *p2 = '\0'; p2++;
        trim_str(p2);
        /* solo schede AMD/ATI/Radeon */
        if (!strstr(p2,"AMD") && !strstr(p2,"Radeon") && !strstr(p2,"ATI")) continue;
        /* salta se già trovata da NVIDIA (non dovrebbe, ma per sicurezza) */
        long long ram_bytes = strtoll(p1, NULL, 10);
        HWDevice* d = &out[count];
        memset(d, 0, sizeof(*d));
        d->type      = DEV_AMD;
        d->gpu_index = count;
        strncpy(d->name, p2, sizeof(d->name)-1);
        d->mem_mb    = ram_bytes / (1024LL * 1024LL);
        /* wmic AdapterRAM su Windows spesso sottostima → prova anche DirectX */
        if (d->mem_mb < 128) d->mem_mb = 512; /* minimo stimato */
        d->avail_mb  = d->mem_mb * 85 / 100;
        d->n_gpu_layers = hw_gpu_layers(d->avail_mb);
        count++;
    }
    pclose(f);
    return count;
}
#endif

/* ── Intel GPU Windows (via wmic VideoController) ──────────────────── */
#ifdef _WIN32
static int detect_intel_windows(HWDevice* out, int max) {
    int count = 0;
    FILE* f = popen("wmic path Win32_VideoController get Name,AdapterRAM /format:csv 2>nul", "r");
    if (!f) return 0;
    char line[512];
    while (fgets(line, sizeof(line), f) && count < max) {
        trim_str(line);
        if (!line[0]) continue;
        /* CSV: Node,AdapterRAM,Name */
        char* p1 = strchr(line, ',');  if (!p1) continue; p1++;
        char* p2 = strchr(p1,   ',');  if (!p2) continue; *p2 = '\0'; p2++;
        trim_str(p2);
        /* solo schede Intel (Arc, UHD, HD Graphics, Iris Xe) */
        if (!strstr(p2, "Intel")) continue;
        /* esclude eventuali match spurii con altri vendor */
        if (strstr(p2, "NVIDIA") || strstr(p2, "AMD") ||
            strstr(p2, "Radeon") || strstr(p2, "ATI")) continue;

        long long ram_bytes = strtoll(p1, NULL, 10);
        HWDevice* d = &out[count];
        memset(d, 0, sizeof(*d));
        d->type      = DEV_INTEL;
        d->gpu_index = count;
        strncpy(d->name, p2, sizeof(d->name) - 1);
        d->mem_mb    = ram_bytes / (1024LL * 1024LL);
        /* iGPU su Windows: AdapterRAM spesso 0 o sottostimato (usa RAM di sistema) */
        if (d->mem_mb < 64) d->mem_mb = 0; /* shared memory, non dedicata */
        d->avail_mb     = d->mem_mb * 85 / 100;
        d->n_gpu_layers = hw_gpu_layers(d->avail_mb);
        count++;
    }
    pclose(f);
    return count;
}
#endif

/* ── Ordina per mem_mb decrescente ─────────────────────────────────── */
static void sort_by_mem(HWDevice* devs, int n) {
    for (int i = 0; i < n - 1; i++)
        for (int j = i + 1; j < n; j++)
            if (devs[j].mem_mb > devs[i].mem_mb) {
                HWDevice tmp = devs[i]; devs[i] = devs[j]; devs[j] = tmp;
            }
}

/* ── API pubblica ───────────────────────────────────────────────────── */
void hw_detect(HWInfo* out) {
    memset(out, 0, sizeof(*out));
    out->primary   =  0;
    out->secondary = -1;
    int n = 0;

    /* CPU — sempre presente */
    detect_cpu(&out->dev[n++]);

    /* GPU — cerca NVIDIA + AMD + Intel */
    HWDevice gpu_tmp[HW_MAX_DEV];
    int ng = 0;
    ng += detect_nvidia(gpu_tmp + ng, HW_MAX_DEV - ng);
#ifdef _WIN32
    ng += detect_amd_windows(gpu_tmp + ng, HW_MAX_DEV - ng);
    ng += detect_intel_windows(gpu_tmp + ng, HW_MAX_DEV - ng);
#else
    ng += detect_amd_linux(gpu_tmp + ng, HW_MAX_DEV - ng);
    ng += detect_intel_linux(gpu_tmp + ng, HW_MAX_DEV - ng);
#endif
    sort_by_mem(gpu_tmp, ng);
    for (int i = 0; i < ng && n < HW_MAX_DEV; i++)
        out->dev[n++] = gpu_tmp[i];
    out->count = n;

    /* primary = GPU con più VRAM (se esiste), altrimenti CPU.
     * La GPU ha sempre priorità sulla CPU per llama.cpp. */
    int best_gpu = -1;
    for (int i = 0; i < n; i++) {
        if (out->dev[i].type != DEV_CPU) {
            if (best_gpu < 0 || out->dev[i].avail_mb > out->dev[best_gpu].avail_mb)
                best_gpu = i;
        }
    }
    out->primary = (best_gpu >= 0) ? best_gpu : 0;

    /* secondary = seconda GPU o CPU (diversa dal primary) */
    if (n > 1) {
        int sec = -1;
        /* Cerca un'altra GPU prima */
        for (int i = 0; i < n; i++) {
            if (i == out->primary) continue;
            if (out->dev[i].type != DEV_CPU) {
                if (sec < 0 || out->dev[i].avail_mb > out->dev[sec].avail_mb)
                    sec = i;
            }
        }
        /* Se nessuna altra GPU, usa la CPU come secondary */
        if (sec < 0) {
            for (int i = 0; i < n; i++) {
                if (i == out->primary) continue;
                sec = i; break;
            }
        }
        out->secondary = sec;
    }
}

void hw_print(const HWInfo* info) {
    for (int i = 0; i < info->count; i++) {
        const HWDevice* d = &info->dev[i];
        const char* tag = (i == info->primary)   ? " [PRIMARIO]"   :
                          (i == info->secondary)  ? " [secondario]" : "";
        if (d->type == DEV_CPU)
            printf("  [%s] %s — RAM %lld MB (disponibile %lld MB)%s\n",
                   hw_dev_type_str(d->type), d->name, d->mem_mb, d->avail_mb, tag);
        else
            printf("  [%s] %s — VRAM %lld MB (usabile %lld MB, layers=%d)%s\n",
                   hw_dev_type_str(d->type), d->name, d->mem_mb, d->avail_mb, d->n_gpu_layers, tag);
    }
}

/* ── Thread cross-platform ─────────────────────────────────────────── */
int hw_thread_create(hw_thread_t* t, hw_thread_fn fn, void* arg) {
#ifdef _WIN32
    *t = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)fn, arg, 0, NULL);
    return (*t == NULL) ? -1 : 0;
#else
    return pthread_create(t, NULL, (void*(*)(void*))fn, arg);
#endif
}

void hw_thread_join(hw_thread_t t) {
#ifdef _WIN32
    WaitForSingleObject(t, INFINITE);
    CloseHandle(t);
#else
    pthread_join(t, NULL);
#endif
}
