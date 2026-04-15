/*
 * hw_detect.c — Rilevamento hardware cross-platform
 *
 * Funziona su:  Linux, Windows (MinGW/MSVC), macOS
 * Dipendenze:   nessuna (stdlib + popen/wmic)
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
#  ifndef popen
#    define popen  _popen
#    define pclose _pclose
#  endif
#endif

/* ── utilità interne ──────────────────────────────────────────── */

/* Esegue cmd, legge la prima riga nell'output, tronca a max_len. */
static void run_first_line(const char* cmd, char* out, int max_len)
{
    out[0] = '\0';
    FILE* f = popen(cmd, "r");
    if (!f) return;
    if (fgets(out, max_len, f)) {
        int n = (int)strlen(out);
        while (n > 0 && (out[n-1] == '\n' || out[n-1] == '\r' || out[n-1] == ' '))
            out[--n] = '\0';
    }
    pclose(f);
}

/* Trim leading/trailing whitespace in-place */
static void trim(char* s)
{
    if (!s || !*s) return;
    char* end = s + strlen(s) - 1;
    while (end > s && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n'))
        *end-- = '\0';
    char* start = s;
    while (*start == ' ' || *start == '\t') start++;
    if (start != s) memmove(s, start, strlen(start) + 1);
}

/*
 * Stima n_gpu_layers a partire dalla VRAM disponibile.
 * Regola empirica: ~80 MB per layer (LLM 7B Q4).
 * Cap a 99 (valore massimo usato da llama.cpp con -ngl).
 */
static int estimate_gpu_layers(long avail_mb)
{
    if (avail_mb <= 0) return 0;
    int layers = (int)(avail_mb / 80);
    return (layers > 99) ? 99 : layers;
}

/* ── CPU ──────────────────────────────────────────────────────── */

static void detect_cpu(HWDevice* d)
{
    d->type = DEV_CPU;
    d->gpu_index = -1;
    d->n_gpu_layers = 0;

#if defined(_WIN32)
    char buf[HW_NAME_LEN] = {0};
    run_first_line(
        "wmic cpu get Name /value 2>nul | findstr \"Name=\"",
        buf, sizeof buf);
    /* "Name=Intel(R) Xeon(R)..." → salta "Name=" */
    const char* p = strstr(buf, "=");
    strncpy(d->name, p ? p + 1 : buf, HW_NAME_LEN - 1);
    trim(d->name);
    if (!d->name[0]) strncpy(d->name, "CPU (sconosciuta)", HW_NAME_LEN - 1);

    MEMORYSTATUSEX ms; ms.dwLength = sizeof ms;
    GlobalMemoryStatusEx(&ms);
    d->mem_mb   = (long)(ms.ullTotalPhys   / (1024*1024));
    d->avail_mb = (long)(ms.ullAvailPhys   / (1024*1024));

#elif defined(__APPLE__)
    run_first_line("sysctl -n machdep.cpu.brand_string 2>/dev/null",
                   d->name, HW_NAME_LEN);
    trim(d->name);
    if (!d->name[0]) strncpy(d->name, "CPU (Apple)", HW_NAME_LEN - 1);

    {
        char buf[64] = {0};
        unsigned long long total = 0, page_free = 0, page_size = 4096;
        run_first_line("sysctl -n hw.memsize 2>/dev/null", buf, sizeof buf);
        total = strtoull(buf, NULL, 10);
        d->mem_mb = (long)(total / (1024*1024));
        run_first_line("vm_stat 2>/dev/null | awk '/Pages free/ {print $3}'",
                       buf, sizeof buf);
        page_free = strtoull(buf, NULL, 10);
        d->avail_mb = (long)((page_free * page_size) / (1024*1024));
    }

#else /* Linux */
    {
        FILE* f = fopen("/proc/cpuinfo", "r");
        if (f) {
            char line[256];
            while (fgets(line, sizeof line, f)) {
                if (strncmp(line, "model name", 10) == 0) {
                    char* p = strchr(line, ':');
                    if (p) {
                        strncpy(d->name, p + 2, HW_NAME_LEN - 1);
                        trim(d->name);
                    }
                    break;
                }
            }
            fclose(f);
        }
        if (!d->name[0]) strncpy(d->name, "CPU (Linux)", HW_NAME_LEN - 1);
    }
    {
        FILE* f = fopen("/proc/meminfo", "r");
        if (f) {
            char line[256];
            unsigned long long total = 0, avail = 0;
            while (fgets(line, sizeof line, f)) {
                unsigned long long v;
                if (sscanf(line, "MemTotal: %llu", &v) == 1)     total = v;
                else if (sscanf(line, "MemAvailable: %llu", &v) == 1) avail = v;
            }
            fclose(f);
            d->mem_mb   = (long)(total / 1024);
            d->avail_mb = (long)(avail / 1024);
        }
    }
#endif
}

/* ── NVIDIA (nvidia-smi) ──────────────────────────────────────── */

/*
 * Ritorna il numero di GPU NVIDIA trovate e popola dev[] a partire da start_idx.
 * Formato nvidia-smi: "nome,mem_total_MB,mem_free_MB"
 */
static int detect_nvidia(HWDevice* devs, int start_idx, int max_count)
{
    int found = 0;

#ifdef _WIN32
    const char* cmd =
        "nvidia-smi --query-gpu=name,memory.total,memory.free"
        " --format=csv,noheader,nounits 2>nul";
#else
    const char* cmd =
        "nvidia-smi --query-gpu=name,memory.total,memory.free"
        " --format=csv,noheader,nounits 2>/dev/null";
#endif

    FILE* f = popen(cmd, "r");
    if (!f) return 0;

    char line[256];
    while (fgets(line, sizeof line, f) && found < max_count) {
        /* Formato: "NVIDIA RTX 3080, 10240, 9800" */
        char nm[HW_NAME_LEN] = {0};
        long total = 0, free_mb = 0;

        char* p1 = strrchr(line, ',');
        if (!p1) continue;
        *p1 = '\0';
        free_mb = atol(p1 + 1);

        char* p2 = strrchr(line, ',');
        if (!p2) continue;
        *p2 = '\0';
        total = atol(p2 + 1);

        strncpy(nm, line, HW_NAME_LEN - 1);
        trim(nm);

        HWDevice* d = &devs[start_idx + found];
        strncpy(d->name, nm, HW_NAME_LEN - 1);
        d->type      = DEV_NVIDIA;
        d->gpu_index = found;
        d->mem_mb    = total;
        d->avail_mb  = free_mb;
        d->n_gpu_layers = estimate_gpu_layers(free_mb);
        found++;
    }
    pclose(f);
    return found;
}

/* ── AMD (rocm-smi su Linux) ──────────────────────────────────── */

static int detect_amd(HWDevice* devs, int start_idx, int max_count)
{
#if defined(_WIN32) || defined(__APPLE__)
    (void)devs; (void)start_idx; (void)max_count;
    return 0;
#else
    /* rocm-smi --showproductname --showmeminfo vram */
    char buf[256] = {0};
    run_first_line("rocm-smi --showproductname 2>/dev/null | grep 'Card series' | head -1",
                   buf, sizeof buf);
    if (!buf[0]) return 0;

    char* p = strrchr(buf, ':');
    const char* nm = p ? p + 2 : buf;

    if (start_idx >= HW_MAX_DEVICES || start_idx >= start_idx + max_count) return 0;
    HWDevice* d = &devs[start_idx];

    strncpy(d->name, nm, HW_NAME_LEN - 1);
    trim(d->name);
    d->type      = DEV_AMD;
    d->gpu_index = 0;

    /* VRAM */
    char vbuf[64] = {0};
    run_first_line("rocm-smi --showmeminfo vram 2>/dev/null | grep 'Total Memory' | awk '{print $NF}' | head -1",
                   vbuf, sizeof vbuf);
    d->mem_mb = atol(vbuf) / (1024*1024);

    char fbuf[64] = {0};
    run_first_line("rocm-smi --showmeminfo vram 2>/dev/null | grep 'Free Memory' | awk '{print $NF}' | head -1",
                   fbuf, sizeof fbuf);
    d->avail_mb = atol(fbuf) / (1024*1024);
    d->n_gpu_layers = estimate_gpu_layers(d->avail_mb);

    return 1;
#endif
}

/* ── Apple Silicon (Metal) ────────────────────────────────────── */

static int detect_apple_gpu(HWDevice* devs, int start_idx)
{
#ifndef __APPLE__
    (void)devs; (void)start_idx;
    return 0;
#else
    /* Su Apple Silicon la GPU condivide la RAM unificata */
    if (start_idx >= HW_MAX_DEVICES) return 0;
    HWDevice* d = &devs[start_idx];

    strncpy(d->name, "Apple GPU (Metal)", HW_NAME_LEN - 1);
    d->type      = DEV_APPLE;
    d->gpu_index = 0;

    /* Stima VRAM ≈ 65-75% della RAM fisica */
    char buf[64] = {0};
    run_first_line("sysctl -n hw.memsize 2>/dev/null", buf, sizeof buf);
    unsigned long long total = strtoull(buf, NULL, 10);
    d->mem_mb   = (long)(total * 70 / 100 / (1024*1024));
    d->avail_mb = d->mem_mb;  /* non misurabile con precisione senza Metal API */
    d->n_gpu_layers = estimate_gpu_layers(d->avail_mb);

    return 1;
#endif
}

/* ── hw_detect ────────────────────────────────────────────────── */

void hw_detect(HWInfo* hw)
{
    memset(hw, 0, sizeof *hw);
    hw->secondary = -1;

    /* 0 = CPU */
    detect_cpu(&hw->dev[0]);
    hw->count   = 1;
    hw->primary = 0;

    /* GPU NVIDIA */
    int nvidia = detect_nvidia(hw->dev, hw->count, HW_MAX_DEVICES - hw->count);
    if (nvidia > 0) {
        if (hw->secondary < 0) hw->secondary = hw->count;
        hw->count += nvidia;
    }

    /* GPU AMD (solo se nessuna NVIDIA trovata) */
    if (nvidia == 0) {
        int amd = detect_amd(hw->dev, hw->count, HW_MAX_DEVICES - hw->count);
        if (amd > 0) {
            if (hw->secondary < 0) hw->secondary = hw->count;
            hw->count += amd;
        }
    }

    /* Apple Silicon */
#ifdef __APPLE__
    {
        /* Rileva se è Silicon (arm64) */
        char arch[32] = {0};
        run_first_line("uname -m 2>/dev/null", arch, sizeof arch);
        if (strcmp(arch, "arm64") == 0) {
            int apple = detect_apple_gpu(hw->dev, hw->count);
            if (apple > 0) {
                if (hw->secondary < 0) hw->secondary = hw->count;
                hw->count += apple;
            }
        }
    }
#endif
}

/* ── hw_dev_type_str ──────────────────────────────────────────── */

const char* hw_dev_type_str(HWDevType type)
{
    switch (type) {
    case DEV_CPU:     return "CPU";
    case DEV_NVIDIA:  return "NVIDIA";
    case DEV_AMD:     return "AMD";
    case DEV_INTEL:   return "Intel GPU";
    case DEV_APPLE:   return "Apple GPU";
    default:          return "???";
    }
}
