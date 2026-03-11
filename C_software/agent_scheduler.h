/*
 * agent_scheduler.h — Scheduler Hot/Cold per agenti multi-modello
 * ================================================================
 * Gestisce il caricamento/scaricamento dei modelli llama.cpp in base
 * alla VRAM disponibile e alla frequenza d'uso degli agenti.
 *
 * Strategia Hot/Cold:
 *   HOT  = modello attualmente caricato in VRAM (risposta immediata)
 *   COLD = modello non caricato (richiede lw_init prima dell'uso)
 *
 * Poiché llama_wrapper supporta UN solo modello alla volta, gli agenti
 * vengono eseguiti in sequenza:
 *   Agente 1 (HOT) → risposta → as_evict → as_load Agente 2 → ...
 *
 * I modelli più chiamati vengono promossi a priority=1 (sticky-hot):
 * vengono caricati per primi e, se la VRAM lo consente, rimangono HOT
 * tra una chiamata e l'altra.
 *
 * "Costruito per i mortali che aspirano alla saggezza."
 */
#pragma once

#include "hw_detect.h"
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Limiti ──────────────────────────────────────────────────────── */
#define AS_MAX_AGENTS    8
#define AS_MODEL_MAX   256   /* lunghezza max nome file modello */
#define AS_PATH_MAX    512   /* lunghezza max path completo     */
#define AS_NAME_MAX     64   /* lunghezza max nome ruolo         */
#define AS_ICON_MAX     16   /* emoji UTF-8                      */

/* Soglia call_count oltre la quale un agente diventa sticky-hot */
#define AS_STICKY_THRESHOLD  3

/* VRAM minima (MB) sotto la quale si forza modalità sequenziale */
#define AS_SEQUENTIAL_VRAM_MB 4000LL

/* ── Stato agente ────────────────────────────────────────────────── */
typedef enum {
    AS_COLD = 0,   /* non caricato                          */
    AS_HOT  = 1,   /* caricato e pronto in VRAM/RAM         */
} AgentState;

/* ── Configurazione singolo agente ───────────────────────────────── */
typedef struct {
    char       name[AS_NAME_MAX];       /* "ricercatore", "programmatore" ... */
    char       icon[AS_ICON_MAX];       /* emoji breve (UTF-8)               */
    char       model_file[AS_MODEL_MAX]; /* solo nome file .gguf             */
    char       model_path[AS_PATH_MAX]; /* path completo al .gguf            */

    int        n_gpu_layers;  /* 0=CPU-only, 99=tutto su GPU          */
    int        n_ctx;         /* dimensione KV context window          */
    int        n_threads;     /* thread CPU (usati se n_gpu_layers=0)  */

    int        vram_delta_mb; /* VRAM consumata misurata (0=sconosciuta) */
    int        ram_delta_mb;  /* RAM consumata in fallback CPU            */
    int        priority;      /* 1=sticky-hot, 2=normale, 3=light        */

    AgentState state;
    time_t     last_used;     /* epoch dell'ultimo uso */
    int        call_count;    /* quante volte è stato chiamato           */
    double     avg_latency_s; /* latenza media generazione (secondi)     */
} AgentCfg;

/* ── Scheduler ────────────────────────────────────────────────────── */
typedef struct {
    AgentCfg    agents[AS_MAX_AGENTS];
    int         n;              /* numero agenti registrati */
    int         hot_idx;        /* indice agente HOT (-1 = nessuno)     */

    long long   vram_avail_mb;  /* VRAM usabile dalla GPU primaria       */
    long long   ram_avail_mb;   /* RAM usabile dalla CPU                 */
    int         sequential;     /* 1 = esegui in sequenza (VRAM bassa)   */

    /* Statistiche sessione */
    int         cache_hits;     /* quante volte il modello era già HOT   */
    int         cache_misses;   /* quante volte ha dovuto fare swap       */
    double      total_swap_s;   /* secondi totali spesi in evict+load    */
} AgentScheduler;

/* ════════════════════════════════════════════════════════════════════
   API PUBBLICA
   ════════════════════════════════════════════════════════════════════ */

/* Inizializza scheduler dall'hardware rilevato.
 * Imposta vram_avail_mb, ram_avail_mb e il flag sequential. */
void as_init(AgentScheduler* s, const HWInfo* hw);

/* Registra un agente nello scheduler.
 * name:       nome ruolo (es. "programmatore")
 * icon:       emoji (es. "💻")
 * model_path: path completo al .gguf (può essere "" se non ancora scelto)
 * priority:   1=sticky-hot, 2=normale, 3=light
 * Ritorna l'indice dell'agente, o -1 se scheduler pieno. */
int  as_add(AgentScheduler* s,
            const char* name, const char* icon,
            const char* model_path, int priority);

/* Assegna automaticamente n_gpu_layers a ogni agente in base al budget
 * VRAM e alle misurazioni vram_delta_mb (se disponibili).
 * Agenti HOT pesanti → più layer su GPU; agenti COLD leggeri → CPU. */
void as_assign_layers(AgentScheduler* s);

/* Carica profilo VRAM da JSON (scritto da vram_bench).
 * Aggiorna vram_delta_mb per ogni agente il cui model_file è nel profilo.
 * Ritorna il numero di voci trovate, -1 su errore di lettura file. */
int  as_load_vram_profile(AgentScheduler* s, const char* json_path);

/* Scrive profilo VRAM su JSON (usato da vram_bench per salvare i risultati).
 * hw_vram_mb: VRAM totale GPU (per documentazione nel file).
 * Ritorna 0=ok, -1=errore scrittura. */
int  as_save_vram_profile(const AgentScheduler* s,
                           const char* json_path,
                           long long hw_vram_mb);

/* Evict dell'agente HOT corrente (chiama lw_free se WITH_LLAMA_STATIC).
 * Dopo la chiamata hot_idx == -1 e l'agente è nello stato COLD. */
void as_evict(AgentScheduler* s);

/* Commuta verso l'agente idx:
 *   - se già HOT → ritorna 0 immediatamente (cache hit)
 *   - se c'è un HOT diverso → as_evict(), poi carica idx (lw_init)
 *   - se idx ha model_path vuoto → ritorna -1
 * Ritorna 0=ok, -1=errore. */
int  as_load(AgentScheduler* s, int idx);

/* Aggiorna statistiche dopo ogni chiamata riuscita (call_count, last_used,
 * promozione a sticky-hot se supera AS_STICKY_THRESHOLD). */
void as_record(AgentScheduler* s, int idx, double latency_s);

/* Ritorna 1 se si consiglia esecuzione sequenziale degli agenti,
 * 0 se si può parallelizzare (solo per backend Ollama multi-porta). */
int  as_sequential(const AgentScheduler* s);

/* Stampa riepilogo stato scheduler su stdout. */
void as_print(const AgentScheduler* s);

/* Stampa suggerimento sui next_layers ottimali per ogni agente. */
void as_print_layer_plan(const AgentScheduler* s);

#ifdef __cplusplus
}
#endif
