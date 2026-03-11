/*
 * agent_scheduler.c — Implementazione Hot/Cold scheduler
 * =======================================================
 * Gestione ciclo di vita modelli llama.cpp per pipeline multi-agente.
 *
 * Poiché llama_wrapper ha stato globale (un solo modello alla volta),
 * la strategia è:
 *   as_load(idx) → evict se necessario → lw_init → agente HOT
 *   as_evict()   → lw_free → agente COLD
 *
 * I modelli ripetuti frequentemente (call_count ≥ AS_STICKY_THRESHOLD)
 * vengono promossi a priority=1 e n_gpu_layers massimo: rimangono
 * HOT il più a lungo possibile tra invocazioni consecutive.
 *
 * "Costruito per i mortali che aspirano alla saggezza."
 */

#include "agent_scheduler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <unistd.h>   /* _SC_NPROCESSORS_ONLN non esiste su Win, usiamo GetSystemInfo */
#else
#  include <unistd.h>
#endif

/* llama_wrapper disponibile solo in build statica */
#ifdef WITH_LLAMA_STATIC
#  include "llama_wrapper.h"
#  define HAS_LW 1
#else
/* Stub silenziosi: lo scheduler funziona anche senza llama statico
 * (es. quando si usa il backend Ollama HTTP). */
static int         lw_init(const char* p, int g, int c)
                   { (void)p;(void)g;(void)c; return -1; }
static void        lw_free(void) {}
static int         lw_is_loaded(void) { return 0; }
#  define HAS_LW 0
#endif

/* ── Utility: tempo corrente in secondi (double) ─────────────────── */
static double _now_s(void) {
#ifdef _WIN32
    LARGE_INTEGER freq, cnt;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&cnt);
    return (double)cnt.QuadPart / (double)freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
#endif
}

/* ── Utility: numero thread CPU ottimali (metà core fisici) ───────── */
static int _optimal_threads(void) {
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    int n = (int)si.dwNumberOfProcessors;
#else
    long n = sysconf(_SC_NPROCESSORS_ONLN);
#endif
    if (n <= 0) n = 4;
    return (n >= 2) ? n / 2 : 1;
}

/* ── Utility: n_ctx di default in base al ruolo ───────────────────── */
static int _default_ctx(const char* name) {
    if (strstr(name, "programm") || strstr(name, "ottimiz")) return 4096;
    if (strstr(name, "pian"))                                 return 3072;
    return 2048;   /* ricercatore, tester */
}

/* ─────────────────────────────────────────────────────────────────── */
/*  API PUBBLICA                                                       */
/* ─────────────────────────────────────────────────────────────────── */

void as_init(AgentScheduler* s, const HWInfo* hw) {
    memset(s, 0, sizeof(*s));
    s->hot_idx = -1;

    /* Budget VRAM dalla GPU primaria (se presente) */
    if (hw->primary >= 0 && hw->dev[hw->primary].type != DEV_CPU)
        s->vram_avail_mb = hw->dev[hw->primary].avail_mb;

    /* Budget RAM dalla CPU */
    for (int i = 0; i < hw->count; i++) {
        if (hw->dev[i].type == DEV_CPU) {
            s->ram_avail_mb = hw->dev[i].avail_mb;
            break;
        }
    }

    /* Sequenziale se VRAM scarsa o assente */
    s->sequential = (s->vram_avail_mb < AS_SEQUENTIAL_VRAM_MB) ? 1 : 0;
}

/* ── as_add ──────────────────────────────────────────────────────── */
int as_add(AgentScheduler* s,
           const char* name, const char* icon,
           const char* model_path, int priority)
{
    if (s->n >= AS_MAX_AGENTS) return -1;

    AgentCfg* a = &s->agents[s->n];
    memset(a, 0, sizeof(*a));

    strncpy(a->name, name, AS_NAME_MAX - 1);
    strncpy(a->icon, icon, AS_ICON_MAX - 1);
    strncpy(a->model_path, model_path ? model_path : "", AS_PATH_MAX - 1);

    /* Estrae solo il nome file dal path */
    const char* sl = strrchr(a->model_path, '/');
    if (!sl) sl = strrchr(a->model_path, '\\');
    strncpy(a->model_file, sl ? sl + 1 : a->model_path, AS_MODEL_MAX - 1);

    a->priority  = (priority >= 1 && priority <= 3) ? priority : 2;
    a->state     = AS_COLD;
    a->n_ctx     = _default_ctx(name);
    a->n_threads = _optimal_threads();

    return s->n++;
}

/* ── as_assign_layers ─────────────────────────────────────────────
 * Assegna n_gpu_layers a ogni agente.
 *
 * Poiché solo UN modello è HOT alla volta, ogni agente può usare
 * l'intera VRAM disponibile quando è il suo turno.
 * Se il vram_delta_mb misurato > vram_avail_mb, si scalano i layer
 * proporzionalmente (stima lineare: VRAM ∝ n_layers).
 * ────────────────────────────────────────────────────────────────── */
void as_assign_layers(AgentScheduler* s) {
    int base_layers = hw_gpu_layers(s->vram_avail_mb);

    for (int i = 0; i < s->n; i++) {
        AgentCfg* a = &s->agents[i];

        if (s->vram_avail_mb <= 0) {
            /* Nessuna GPU → CPU-only */
            a->n_gpu_layers = 0;
            continue;
        }

        if (a->vram_delta_mb <= 0 || a->vram_delta_mb <= (int)s->vram_avail_mb) {
            /* Misura assente o modello ci sta tutto: layer massimi */
            a->n_gpu_layers = base_layers;
        } else {
            /* Modello più grande della VRAM disponibile:
             * stima quanti layer ci entrano con la VRAM che abbiamo */
            double ratio = (double)s->vram_avail_mb / (double)a->vram_delta_mb;
            a->n_gpu_layers = (int)(base_layers * ratio);
            if (a->n_gpu_layers < 1) a->n_gpu_layers = 0;  /* forza CPU */
        }

        /* Agenti sticky-hot (priority=1) → preferenza GPU massima */
        if (a->priority == 1 && a->n_gpu_layers > 0)
            a->n_gpu_layers = base_layers;

        /* Agenti light (priority=3) → CPU a meno che la VRAM non sia abbondante */
        if (a->priority == 3 && s->vram_avail_mb < 8000)
            a->n_gpu_layers = (base_layers > 16) ? 16 : base_layers;
    }
}

/* ── as_load_vram_profile ─────────────────────────────────────────
 * Parser JSON minimalista per vram_profile.json.
 * Formato atteso (scritto da vram_bench):
 *   {"profiles":[{"model":"foo.gguf","vram_delta_mb":4200,...},...]}
 * ────────────────────────────────────────────────────────────────── */
int as_load_vram_profile(AgentScheduler* s, const char* json_path) {
    FILE* f = fopen(json_path, "r");
    if (!f) return -1;

    /* Legge tutto il file in memoria */
    fseek(f, 0, SEEK_END);
    long fsz = ftell(f);
    rewind(f);
    if (fsz <= 0 || fsz > 1024*1024) { fclose(f); return -1; }

    char* buf = (char*)malloc((size_t)fsz + 1);
    if (!buf) { fclose(f); return -1; }
    fread(buf, 1, (size_t)fsz, f);
    buf[fsz] = '\0';
    fclose(f);

    int found = 0;

    /* Itera su ogni oggetto dentro "profiles":[...] */
    const char* p = buf;
    while ((p = strstr(p, "\"model\"")) != NULL) {
        /* Estrae valore "model" */
        const char* q = strchr(p, ':');
        if (!q) { p++; continue; }
        q++;
        while (*q == ' ' || *q == '"') q++;
        char model_name[AS_MODEL_MAX] = {0};
        int mi = 0;
        while (*q && *q != '"' && mi < AS_MODEL_MAX - 1)
            model_name[mi++] = *q++;
        model_name[mi] = '\0';

        /* Estrae valore "vram_delta_mb" nella stessa voce dell'array */
        const char* next_entry = strstr(q, "\"model\"");  /* inizio prossima voce */
        const char* vram_key   = strstr(q, "\"vram_delta_mb\"");
        if (!vram_key || (next_entry && vram_key > next_entry)) {
            p = q; continue;
        }
        const char* vp = strchr(vram_key, ':');
        if (!vp) { p = q; continue; }
        int vram_mb = atoi(vp + 1);

        /* Cerca l'agente con questo model_file */
        for (int i = 0; i < s->n; i++) {
            if (strcmp(s->agents[i].model_file, model_name) == 0) {
                s->agents[i].vram_delta_mb = vram_mb;
                found++;
                break;
            }
        }
        p = q;
    }

    free(buf);
    return found;
}

/* ── as_save_vram_profile ─────────────────────────────────────────
 * Scrive profilo VRAM su JSON.
 * Formato compatibile con as_load_vram_profile.
 * ────────────────────────────────────────────────────────────────── */
int as_save_vram_profile(const AgentScheduler* s,
                          const char* json_path,
                          long long hw_vram_mb)
{
    FILE* f = fopen(json_path, "w");
    if (!f) return -1;

    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", tm_info);

    fprintf(f, "{\n");
    fprintf(f, "  \"generated\": \"%s\",\n", ts);
    fprintf(f, "  \"hw_vram_mb\": %lld,\n", hw_vram_mb);
    fprintf(f, "  \"profiles\": [\n");

    int first = 1;
    for (int i = 0; i < s->n; i++) {
        const AgentCfg* a = &s->agents[i];
        if (a->vram_delta_mb <= 0 && a->ram_delta_mb <= 0) continue;

        if (!first) fprintf(f, ",\n");
        first = 0;

        fprintf(f, "    {\n");
        fprintf(f, "      \"agent\": \"%s\",\n",        a->name);
        fprintf(f, "      \"model\": \"%s\",\n",        a->model_file);
        fprintf(f, "      \"n_gpu_layers\": %d,\n",    a->n_gpu_layers);
        fprintf(f, "      \"vram_delta_mb\": %d,\n",   a->vram_delta_mb);
        fprintf(f, "      \"ram_delta_mb\": %d,\n",    a->ram_delta_mb);
        fprintf(f, "      \"call_count\": %d,\n",      a->call_count);
        fprintf(f, "      \"priority\": %d\n",         a->priority);
        fprintf(f, "    }");
    }

    fprintf(f, "\n  ]\n}\n");
    fclose(f);
    return 0;
}

/* ── as_evict ─────────────────────────────────────────────────────
 * Scarica l'agente HOT corrente dalla memoria (lw_free).
 * ────────────────────────────────────────────────────────────────── */
void as_evict(AgentScheduler* s) {
    if (s->hot_idx < 0) return;

#if HAS_LW
    lw_free();
#endif

    s->agents[s->hot_idx].state = AS_COLD;
    s->hot_idx = -1;
}

/* ── as_load ──────────────────────────────────────────────────────
 * Commuta verso l'agente idx.
 * Ritorna 0=ok (incluso cache hit), -1=errore caricamento.
 * ────────────────────────────────────────────────────────────────── */
int as_load(AgentScheduler* s, int idx) {
    if (idx < 0 || idx >= s->n) return -1;
    AgentCfg* target = &s->agents[idx];

    /* ── Cache hit: modello già HOT ───────────────────────────── */
    if (s->hot_idx == idx) {
        s->cache_hits++;
        return 0;
    }

    /* ── Path non impostato → non possiamo caricare ───────────── */
    if (!target->model_path[0]) return -1;

    double t0 = _now_s();

    /* ── Evict agente HOT corrente ────────────────────────────── */
    if (s->hot_idx >= 0) {
        as_evict(s);
    }

    /* ── Carica il nuovo agente ───────────────────────────────── */
#if HAS_LW
    if (lw_init(target->model_path, target->n_gpu_layers, target->n_ctx) != 0) {
        s->cache_misses++;
        return -1;
    }
#else
    /* Senza backend statico: segna comunque HOT (backend Ollama gestisce da solo) */
#endif

    target->state = AS_HOT;
    s->hot_idx    = idx;
    s->cache_misses++;
    s->total_swap_s += _now_s() - t0;

    return 0;
}

/* ── as_record ────────────────────────────────────────────────────
 * Aggiorna statistiche dopo ogni chiamata riuscita.
 * ────────────────────────────────────────────────────────────────── */
void as_record(AgentScheduler* s, int idx, double latency_s) {
    if (idx < 0 || idx >= s->n) return;
    AgentCfg* a = &s->agents[idx];

    a->call_count++;
    a->last_used = time(NULL);

    /* Media mobile esponenziale della latenza (α=0.3) */
    if (a->avg_latency_s <= 0.0)
        a->avg_latency_s = latency_s;
    else
        a->avg_latency_s = 0.7 * a->avg_latency_s + 0.3 * latency_s;

    /* Promozione a sticky-hot se chiamato frequentemente */
    if (a->call_count >= AS_STICKY_THRESHOLD && a->priority > 1) {
        a->priority = 1;
        printf("  ⭐  %s %s promosso a sticky-hot (chiamate: %d)\n",
               a->icon, a->name, a->call_count);
    }
}

/* ── as_sequential ────────────────────────────────────────────── */
int as_sequential(const AgentScheduler* s) {
    return s->sequential;
}

/* ── as_print ─────────────────────────────────────────────────── */
void as_print(const AgentScheduler* s) {
    const char* sep = "─────────────────────────────────────────────────────";
    printf("\n  %s\n", sep);
    printf("  Scheduler Hot/Cold — %d agenti registrati\n", s->n);
    printf("  VRAM usabile: %lld MB   RAM usabile: %lld MB\n",
           s->vram_avail_mb, s->ram_avail_mb);
    printf("  Modalità: %s\n", s->sequential ? "SEQUENZIALE (VRAM limitata)" : "libera");
    printf("  %s\n", sep);
    printf("  %-3s %-14s %-6s  %-34s %-8s  %-10s\n",
           "Idx", "Agente", "Stato", "Modello", "VRAM_MB", "Chiamate");
    printf("  %s\n", sep);

    for (int i = 0; i < s->n; i++) {
        const AgentCfg* a = &s->agents[i];
        const char* stato = (a->state == AS_HOT) ? "🔥 HOT"  : "❄️  cold";
        const char* prio  = (a->priority == 1)    ? " ⭐"     :
                            (a->priority == 3)    ? " 🪶"     : "   ";

        char model_short[35];
        strncpy(model_short, a->model_file, 34);
        model_short[34] = '\0';
        if (strlen(a->model_file) > 34) {
            model_short[31] = '.'; model_short[32] = '.'; model_short[33] = '.';
        }

        char vram_s[16];
        if (a->vram_delta_mb > 0)
            snprintf(vram_s, sizeof(vram_s), "%d", a->vram_delta_mb);
        else
            snprintf(vram_s, sizeof(vram_s), "?");

        printf("  [%d] %-14s %-9s  %-34s %-8s  %d%s\n",
               i, a->name, stato, model_short, vram_s, a->call_count, prio);
    }
    printf("  %s\n", sep);
    printf("  Cache hits: %d   Swap: %d   Swap time: %.1fs\n",
           s->cache_hits, s->cache_misses, s->total_swap_s);
    printf("  %s\n\n", sep);
}

/* ── as_print_layer_plan ──────────────────────────────────────── */
void as_print_layer_plan(const AgentScheduler* s) {
    printf("\n  Piano layer GPU (VRAM disponibile: %lld MB)\n", s->vram_avail_mb);
    printf("  %-14s  %-6s  %-10s  %s\n",
           "Agente", "Layers", "VRAM_est", "Strategia");
    printf("  %s\n", "────────────────────────────────────────────────");

    for (int i = 0; i < s->n; i++) {
        const AgentCfg* a = &s->agents[i];
        const char* strategia;

        if (a->n_gpu_layers == 0)
            strategia = "CPU-only (nessuna GPU)";
        else if (a->n_gpu_layers >= 9999)
            strategia = "GPU completa (tutti i layer)";
        else if (a->priority == 1)
            strategia = "GPU max (sticky-hot)";
        else if (a->vram_delta_mb > 0 &&
                 a->vram_delta_mb > (int)s->vram_avail_mb)
            strategia = "GPU parziale (modello > VRAM)";
        else
            strategia = "GPU completa";

        char vram_s[16];
        if (a->vram_delta_mb > 0)
            snprintf(vram_s, sizeof(vram_s), "%d MB", a->vram_delta_mb);
        else
            snprintf(vram_s, sizeof(vram_s), "?");

        printf("  %-14s  %-6d  %-10s  %s\n",
               a->name, a->n_gpu_layers, vram_s, strategia);
    }
    printf("\n");
}
