/*
 * hw_detect.h — Rilevamento hardware cross-platform
 * Linux: /proc/meminfo, /sys/class/drm, nvidia-smi, rocm-smi
 * Windows: GlobalMemoryStatusEx, wmic, nvidia-smi
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* ── Tipo dispositivo ─────────────────────────────────────── */
typedef enum {
    DEV_CPU    = 0,
    DEV_NVIDIA = 1,
    DEV_AMD    = 2,
} DevType;

/* ── Dispositivo hardware ─────────────────────────────────── */
typedef struct {
    DevType   type;
    char      name[128];    /* nome CPU/GPU */
    long long mem_mb;       /* RAM totale (CPU) o VRAM totale (GPU) in MiB */
    long long avail_mb;     /* memoria disponibile / usabile (mem_mb * 0.85 per GPU) */
    int       gpu_index;    /* indice GPU (solo DEV_NVIDIA/DEV_AMD), -1 per CPU */
    int       n_gpu_layers; /* n_gpu_layers ottimale llama.cpp (0 per CPU) */
} HWDevice;

#define HW_MAX_DEV 8

/* ── Insieme dispositivi rilevati ─────────────────────────── */
typedef struct {
    HWDevice  dev[HW_MAX_DEV];
    int       count;
    int       primary;    /* indice dispositivo con più memoria (compiti pesanti) */
    int       secondary;  /* indice secondo dispositivo (-1 se non esiste) */
} HWInfo;

/* Rilevamento completo: CPU + NVIDIA + AMD */
void        hw_detect(HWInfo* out);

/* Stampa riepilogo hardware a schermo */
void        hw_print(const HWInfo* info);

/* Calcola n_gpu_layers ottimali dato VRAM disponibile (già al netto 15% OS) */
int         hw_gpu_layers(long long vram_mb);

/* Stringa tipo dispositivo */
const char* hw_dev_type_str(DevType t);

/* Thread cross-platform --------------------------------------------- */
#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
   typedef HANDLE hw_thread_t;
   typedef DWORD (*hw_thread_fn)(void*);
#else
#  include <pthread.h>
   typedef pthread_t hw_thread_t;
   typedef void* (*hw_thread_fn)(void*);
#endif

int  hw_thread_create(hw_thread_t* t, hw_thread_fn fn, void* arg);
void hw_thread_join(hw_thread_t t);

#ifdef __cplusplus
}
#endif
