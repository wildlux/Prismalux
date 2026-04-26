/*
 * hw_detect.h — Rilevamento hardware cross-platform (CPU, GPU NVIDIA/AMD/Intel)
 *
 * Unico include necessario:  #include "hw_detect.h"
 * Compilare insieme a:       hw_detect.c
 */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#define HW_MAX_DEVICES  8
#define HW_NAME_LEN     128

typedef enum {
    DEV_UNKNOWN = 0,
    DEV_CPU     = 1,
    DEV_NVIDIA  = 2,
    DEV_AMD     = 3,
    DEV_INTEL   = 4,
    DEV_APPLE   = 5,
} HWDevType;

typedef struct {
    char        name[HW_NAME_LEN];   /* nome dispositivo */
    HWDevType   type;
    int         gpu_index;           /* indice per nvidia-smi --id=N */
    long        mem_mb;              /* VRAM totale o RAM totale (MB) */
    long        avail_mb;            /* VRAM/RAM disponibile (MB) */
    int         n_gpu_layers;        /* layer GPU ottimali (stima) */
} HWDevice;

typedef struct {
    HWDevice    dev[HW_MAX_DEVICES];
    int         count;
    int         primary;   /* indice del device CPU */
    int         secondary; /* GPU dedicata preferita per inferenza: NVIDIA > AMD > Intel iGPU */
    int         igpu;      /* indice Intel iGPU (-1 se assente) */
} HWInfo;

/* Rileva hardware: chiama popen/wmic — eseguire in un thread separato */
void        hw_detect(HWInfo* hw);

/* Restituisce stringa leggibile per il tipo device */
const char* hw_dev_type_str(HWDevType type);

#ifdef __cplusplus
}
#endif
