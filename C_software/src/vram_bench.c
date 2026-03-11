/*
 * vram_bench.c — Benchmark VRAM per modelli .gguf / Ollama
 * =========================================================
 * Misura quanta VRAM (e RAM) consuma ogni modello AI, e salva
 * i risultati in vram_profile.json per calibrare l'AgentScheduler.
 *
 * Due modalità:
 *   --backend ollama   : usa Ollama API HTTP (GET /api/ps dopo load)
 *   --backend llama    : usa llama_wrapper statico + nvidia-smi delta
 *                        (richiede build con -DWITH_LLAMA_STATIC)
 *
 * Uso:
 *   ./vram_bench                            # Ollama, tutti i modelli
 *   ./vram_bench --backend ollama           # esplicito
 *   ./vram_bench --backend llama            # richiede ./build.sh prima
 *   ./vram_bench --model mistral-7b.gguf    # solo un modello
 *   ./vram_bench --layers 32               # forza n_gpu_layers
 *   ./vram_bench --port 11434 --host 127.0.0.1
 *   ./vram_bench --output risultati.json
 *
 * Build HTTP-only (nessuna dipendenza da llama.cpp):
 *   gcc -O2 -Wall -o vram_bench vram_bench.c agent_scheduler.c hw_detect.c -lpthread -lm
 *
 * Build con llama statico (dopo ./build.sh):
 *   make vram_bench_static
 *
 * "Costruito per i mortali che aspirano alla saggezza."
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include "hw_detect.h"
#include "agent_scheduler.h"

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <winsock2.h>
#  include <ws2tcpip.h>
   typedef SOCKET sock_t;
#  define SOCK_INV   INVALID_SOCKET
#  define sock_close(s) closesocket(s)
#  define snprintf _snprintf
#else
#  include <unistd.h>
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <netdb.h>
#  include <dirent.h>
   typedef int sock_t;
#  define SOCK_INV (-1)
#  define sock_close(s) close(s)
#endif

/* llama_wrapper disponibile solo in build statica */
#ifdef WITH_LLAMA_STATIC
#  include "llama_wrapper.h"
#  define HAS_LW 1
#else
#  define HAS_LW 0
static int         lw_init(const char* p, int g, int c) { (void)p;(void)g;(void)c; return -1; }
static void        lw_free(void) {}
static int         lw_list_models(const char* d, char n[][256], int m) { (void)d;(void)n;(void)m; return 0; }
#endif

/* ═══════════════════════════════════════════════════════════════════
   SEZIONE 1 — Strutture risultati
   ═══════════════════════════════════════════════════════════════════ */

#define MAX_PROFILES 64

typedef struct {
    char   model[256];       /* nome file o nome Ollama                */
    int    n_gpu_layers;     /* layers usati durante il test           */
    int    vram_before_mb;   /* VRAM usata prima del load              */
    int    vram_after_mb;    /* VRAM usata dopo il load                */
    int    vram_delta_mb;    /* differenza = consumo netto             */
    int    ram_before_mb;    /* RAM disponibile prima (CPU)            */
    int    ram_after_mb;     /* RAM disponibile dopo                   */
    int    ram_delta_mb;     /* RAM consumata aggiuntiva               */
    double load_ms;          /* millisecondi per caricare il modello   */
    int    success;          /* 1=ok, 0=errore                         */
    char   error_msg[128];
} VramProfile;

static VramProfile g_profiles[MAX_PROFILES];
static int         g_n_profiles = 0;

/* ═══════════════════════════════════════════════════════════════════
   SEZIONE 2 — Lettura VRAM / RAM di sistema
   ═══════════════════════════════════════════════════════════════════ */

/* Legge VRAM usata correntemente via nvidia-smi (MB).
 * Ritorna -1 se nvidia-smi non disponibile. */
static int read_vram_used_mb(void) {
#ifdef _WIN32
    FILE* f = _popen("nvidia-smi --query-gpu=memory.used --format=csv,noheader,nounits 2>nul", "r");
#else
    FILE* f = popen("nvidia-smi --query-gpu=memory.used --format=csv,noheader,nounits 2>/dev/null", "r");
#endif
    if (!f) return -1;
    char line[64] = {0};
    int mb = -1;
    if (fgets(line, sizeof(line), f)) mb = atoi(line);
#ifdef _WIN32
    _pclose(f);
#else
    pclose(f);
#endif
    return mb;
}

/* Legge RAM disponibile correntemente (MB). */
static long long read_ram_avail_mb(void) {
#ifdef _WIN32
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatusEx(&ms);
    return (long long)(ms.ullAvailPhys / (1024ULL * 1024ULL));
#else
    FILE* f = fopen("/proc/meminfo", "r");
    if (!f) return -1;
    char line[256];
    long long avail = -1;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "MemAvailable:", 13) == 0) {
            sscanf(line + 13, "%lld", &avail);
            avail /= 1024;  /* kB → MB */
            break;
        }
    }
    fclose(f);
    return avail;
#endif
}

/* ═══════════════════════════════════════════════════════════════════
   SEZIONE 3 — TCP helpers per Ollama API
   ═══════════════════════════════════════════════════════════════════ */

#ifdef _WIN32
static int _wsa_ok = 0;
static void wsa_init(void) {
    if (!_wsa_ok) { WSADATA w; WSAStartup(MAKEWORD(2,2),&w); _wsa_ok=1; }
}
#else
#define wsa_init() (void)0
#endif

static sock_t tcp_connect(const char* host, int port) {
    wsa_init();
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((unsigned short)port);
    struct hostent* he = gethostbyname(host);
    if (!he) return SOCK_INV;
    memcpy(&addr.sin_addr, he->h_addr_list[0], (size_t)he->h_length);
    sock_t s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == SOCK_INV) return SOCK_INV;
    if (connect(s, (struct sockaddr*)&addr, sizeof addr) < 0) {
        sock_close(s); return SOCK_INV;
    }
    return s;
}

static void send_all(sock_t s, const char* buf, int len) {
    int sent = 0;
    while (sent < len) {
        int n = send(s, buf + sent, (size_t)(len - sent), 0);
        if (n <= 0) return;
        sent += n;
    }
}

/* Riceve la risposta HTTP completa in buf. Ritorna byte letti. */
static int http_recv_all(sock_t s, char* buf, int maxlen) {
    int total = 0;
    char tmp[4096];
    for (;;) {
        int n = recv(s, tmp, sizeof(tmp) - 1, 0);
        if (n <= 0) break;
        if (total + n < maxlen - 1) {
            memcpy(buf + total, tmp, (size_t)n);
            total += n;
        }
    }
    buf[total] = '\0';
    return total;
}

/* Esegui una richiesta HTTP POST/GET a Ollama, ritorna il body.
 * body_out deve essere pre-allocato, body_max è la sua dimensione.
 * Ritorna 0=ok, -1=errore connessione. */
static int ollama_request(const char* host, int port,
                           const char* method, const char* path,
                           const char* json_body,
                           char* body_out, int body_max)
{
    sock_t s = tcp_connect(host, port);
    if (s == SOCK_INV) return -1;

    char req[4096];
    int  body_len = json_body ? (int)strlen(json_body) : 0;

    int req_len = snprintf(req, sizeof(req),
        "%s %s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n",
        method, path, host, port, body_len);

    send_all(s, req, req_len);
    if (json_body && body_len > 0)
        send_all(s, json_body, body_len);

    /* Ricevi risposta */
    char* raw = (char*)malloc((size_t)body_max);
    if (!raw) { sock_close(s); return -1; }
    http_recv_all(s, raw, body_max);
    sock_close(s);

    /* Salta headers HTTP → trova corpo dopo \r\n\r\n */
    const char* body_start = strstr(raw, "\r\n\r\n");
    if (body_start) {
        body_start += 4;
        strncpy(body_out, body_start, (size_t)(body_max - 1));
        body_out[body_max - 1] = '\0';
    } else {
        body_out[0] = '\0';
    }
    free(raw);
    return 0;
}

/* Estrae un valore numerico da JSON grezzo: {"key": 12345, ...} */
static long long json_get_ll(const char* json, const char* key) {
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char* p = strstr(json, pattern);
    if (!p) return -1;
    p += strlen(pattern);
    while (*p == ':' || *p == ' ') p++;
    if (*p == '"') p++;   /* valore stringa → salta virgolette */
    return strtoll(p, NULL, 10);
}

/* Estrae una stringa da JSON: {"key": "valore"} → "valore" */
static void json_get_str(const char* json, const char* key,
                          char* out, int out_max) {
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char* p = strstr(json, pattern);
    if (!p) { out[0] = '\0'; return; }
    p += strlen(pattern);
    while (*p == ':' || *p == ' ') p++;
    if (*p != '"') { out[0] = '\0'; return; }
    p++;
    int i = 0;
    while (*p && *p != '"' && i < out_max - 1)
        out[i++] = *p++;
    out[i] = '\0';
}

/* ═══════════════════════════════════════════════════════════════════
   SEZIONE 4 — Benchmark in modalità Ollama
   ═══════════════════════════════════════════════════════════════════ */

/* Carica un modello Ollama (via /api/generate num_predict=1) e legge
 * la VRAM consumata da /api/ps. Poi scarica il modello (keep_alive=0).
 * Ritorna 0=ok, -1=errore. */
static int bench_ollama_model(const char* host, int port,
                               const char* model_name,
                               VramProfile* out)
{
    strncpy(out->model, model_name, sizeof(out->model) - 1);
    out->n_gpu_layers = -1;  /* Ollama gestisce autonomamente */
    out->success = 0;

    char body[2048];
    char resp[65536];

    /* ── STEP 1: pre-caricamento VRAM ─────────────────────────── */
    out->vram_before_mb = read_vram_used_mb();
    out->ram_before_mb  = (int)read_ram_avail_mb();

    /* ── STEP 2: carica il modello ────────────────────────────── */
    printf("    Caricamento %-40s ", model_name); fflush(stdout);
    struct timespec t0, t1;
#ifdef _WIN32
    LARGE_INTEGER freq, cnt0, cnt1;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&cnt0);
#else
    clock_gettime(CLOCK_MONOTONIC, &t0);
#endif

    snprintf(body, sizeof(body),
             "{\"model\":\"%s\",\"prompt\":\"ping\","
             "\"stream\":false,\"num_predict\":1}",
             model_name);
    if (ollama_request(host, port, "POST", "/api/generate",
                       body, resp, sizeof(resp)) != 0) {
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "Connessione Ollama fallita su %s:%d", host, port);
        printf("❌ ERRORE (connessione)\n");
        return -1;
    }

#ifdef _WIN32
    QueryPerformanceCounter(&cnt1);
    out->load_ms = (double)(cnt1.QuadPart - cnt0.QuadPart) /
                   (double)freq.QuadPart * 1000.0;
#else
    clock_gettime(CLOCK_MONOTONIC, &t1);
    out->load_ms = (double)(t1.tv_sec  - t0.tv_sec)  * 1000.0
                 + (double)(t1.tv_nsec - t0.tv_nsec) / 1e6;
#endif

    /* Verifica errore nel JSON di risposta */
    if (strstr(resp, "\"error\"")) {
        char errbuf[200] = {0};
        json_get_str(resp, "error", errbuf, sizeof(errbuf));
        snprintf(out->error_msg, sizeof(out->error_msg), "%s", errbuf);
        printf("❌ ERRORE (%s)\n", errbuf);
        return -1;
    }

    /* ── STEP 3: leggi VRAM da /api/ps ───────────────────────── */
    if (ollama_request(host, port, "GET", "/api/ps",
                       NULL, resp, sizeof(resp)) == 0)
    {
        /* Cerca il modello nella lista caricata */
        const char* entry = strstr(resp, model_name);
        if (entry) {
            long long size_vram = json_get_ll(entry, "size_vram");
            if (size_vram > 0)
                out->vram_delta_mb = (int)(size_vram / (1024LL * 1024LL));
        }
    }

    /* Fallback: misura delta nvidia-smi */
    out->vram_after_mb = read_vram_used_mb();
    if (out->vram_delta_mb <= 0 &&
        out->vram_before_mb >= 0 && out->vram_after_mb >= 0) {
        out->vram_delta_mb = out->vram_after_mb - out->vram_before_mb;
        if (out->vram_delta_mb < 0) out->vram_delta_mb = 0;
    }

    out->ram_after_mb  = (int)read_ram_avail_mb();
    out->ram_delta_mb  = (out->ram_before_mb >= 0 && out->ram_after_mb >= 0)
                         ? (out->ram_before_mb - out->ram_after_mb)
                         : 0;
    if (out->ram_delta_mb < 0) out->ram_delta_mb = 0;

    /* ── STEP 4: scarica il modello (keep_alive=0) ────────────── */
    snprintf(body, sizeof(body),
             "{\"model\":\"%s\",\"keep_alive\":0}", model_name);
    ollama_request(host, port, "POST", "/api/generate", body, resp, sizeof(resp));

    out->success = 1;
    printf("✅  VRAM: %4d MB   RAM: %4d MB   %.0f ms\n",
           out->vram_delta_mb, out->ram_delta_mb, out->load_ms);
    return 0;
}

/* ── Elenca modelli disponibili in Ollama ───────────────────────── */
static int list_ollama_models(const char* host, int port,
                               char names[][256], int max_names)
{
    char resp[65536];
    if (ollama_request(host, port, "GET", "/api/tags",
                       NULL, resp, sizeof(resp)) != 0)
        return -1;

    int count = 0;
    const char* p = resp;
    while ((p = strstr(p, "\"name\"")) != NULL && count < max_names) {
        const char* q = strchr(p, ':');
        if (!q) { p++; continue; }
        q++;
        while (*q == ' ' || *q == '"') q++;
        int i = 0;
        while (*q && *q != '"' && i < 255) names[count][i++] = *q++;
        names[count][i] = '\0';
        count++;
        p = q;
    }
    return count;
}

/* ═══════════════════════════════════════════════════════════════════
   SEZIONE 5 — Benchmark in modalità llama-statico
   ═══════════════════════════════════════════════════════════════════ */

static int bench_llama_model(const char* models_dir,
                              const char* model_file,
                              int n_gpu_layers,
                              VramProfile* out)
{
    strncpy(out->model, model_file, sizeof(out->model) - 1);
    out->n_gpu_layers = n_gpu_layers;
    out->success      = 0;

    char full_path[768];
    snprintf(full_path, sizeof(full_path), "%s/%s", models_dir, model_file);

    /* Pre-lettura VRAM/RAM */
    out->vram_before_mb = read_vram_used_mb();
    out->ram_before_mb  = (int)read_ram_avail_mb();

    printf("    %-40s layers=%-3d  ", model_file, n_gpu_layers); fflush(stdout);

#if !HAS_LW
    snprintf(out->error_msg, sizeof(out->error_msg),
             "Build senza llama statico (ricompila con make vram_bench_static)");
    printf("❌ Non disponibile (HTTP-only build)\n");
    return -1;
#else
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    if (lw_init(full_path, n_gpu_layers, 512) != 0) {
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "lw_init fallito per %s", model_file);
        printf("❌ ERRORE caricamento\n");
        return -1;
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);
    out->load_ms = (double)(t1.tv_sec  - t0.tv_sec)  * 1000.0
                 + (double)(t1.tv_nsec - t0.tv_nsec) / 1e6;

    out->vram_after_mb = read_vram_used_mb();
    out->ram_after_mb  = (int)read_ram_avail_mb();

    out->vram_delta_mb = (out->vram_before_mb >= 0 && out->vram_after_mb >= 0)
                         ? (out->vram_after_mb - out->vram_before_mb)
                         : 0;
    if (out->vram_delta_mb < 0) out->vram_delta_mb = 0;

    out->ram_delta_mb  = (out->ram_before_mb >= 0 && out->ram_after_mb >= 0)
                         ? (out->ram_before_mb - out->ram_after_mb)
                         : 0;
    if (out->ram_delta_mb < 0) out->ram_delta_mb = 0;

    lw_free();

    out->success = 1;
    printf("✅  VRAM: %4d MB   RAM: %4d MB   %.0f ms\n",
           out->vram_delta_mb, out->ram_delta_mb, out->load_ms);
    return 0;
#endif
}

/* ═══════════════════════════════════════════════════════════════════
   SEZIONE 6 — Salvataggio profilo JSON
   ═══════════════════════════════════════════════════════════════════ */

static int save_profile_json(const char* out_path,
                              VramProfile* profiles, int n,
                              long long hw_vram_mb,
                              const char* backend_used)
{
    FILE* f = fopen(out_path, "w");
    if (!f) {
        fprintf(stderr, "  ❌ Impossibile scrivere: %s\n", out_path);
        return -1;
    }

    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", tm_info);

    fprintf(f, "{\n");
    fprintf(f, "  \"generated\": \"%s\",\n", ts);
    fprintf(f, "  \"hw_vram_mb\": %lld,\n", hw_vram_mb);
    fprintf(f, "  \"backend\": \"%s\",\n", backend_used);
    fprintf(f, "  \"profiles\": [\n");

    int first = 1;
    for (int i = 0; i < n; i++) {
        VramProfile* p = &profiles[i];
        if (!p->success) continue;

        if (!first) fprintf(f, ",\n");
        first = 0;

        fprintf(f, "    {\n");
        fprintf(f, "      \"model\": \"%s\",\n",         p->model);
        fprintf(f, "      \"n_gpu_layers\": %d,\n",     p->n_gpu_layers);
        fprintf(f, "      \"vram_delta_mb\": %d,\n",    p->vram_delta_mb);
        fprintf(f, "      \"ram_delta_mb\": %d,\n",     p->ram_delta_mb);
        fprintf(f, "      \"load_ms\": %.1f\n",         p->load_ms);
        fprintf(f, "    }");
    }

    fprintf(f, "\n  ]\n}\n");
    fclose(f);
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════
   SEZIONE 7 — Stampa riepilogo + suggerimenti scheduler
   ═══════════════════════════════════════════════════════════════════ */

static void print_summary(VramProfile* profiles, int n,
                           long long vram_avail_mb)
{
    const char* sep = "════════════════════════════════════════════════════════";
    printf("\n  %s\n", sep);
    printf("  RIEPILOGO BENCHMARK VRAM\n");
    printf("  VRAM disponibile: %lld MB\n", vram_avail_mb);
    printf("  %s\n", sep);
    printf("  %-38s  %-8s  %-8s  %-10s  %s\n",
           "Modello", "VRAM_MB", "RAM_MB", "Load_ms", "Suggerimento");
    printf("  %s\n", sep);

    int total_ok = 0, total_err = 0;

    for (int i = 0; i < n; i++) {
        VramProfile* p = &profiles[i];
        if (!p->success) {
            printf("  %-38s  %-8s  %-8s  %-10s  ❌ %s\n",
                   p->model, "—", "—", "—", p->error_msg);
            total_err++;
            continue;
        }
        total_ok++;

        const char* suggestion;
        if (vram_avail_mb <= 0)
            suggestion = "CPU-only";
        else if (p->vram_delta_mb <= 0)
            suggestion = "GPU (VRAM N/D)";
        else if (p->vram_delta_mb <= (int)vram_avail_mb)
            suggestion = "GPU completa ✅";
        else if (p->vram_delta_mb <= (int)(vram_avail_mb * 2))
            suggestion = "GPU parziale ⚠️";
        else
            suggestion = "CPU (troppo grande) 🔴";

        char name_short[39];
        strncpy(name_short, p->model, 38);
        name_short[38] = '\0';

        printf("  %-38s  %-8d  %-8d  %-10.0f  %s\n",
               name_short,
               p->vram_delta_mb,
               p->ram_delta_mb,
               p->load_ms,
               suggestion);
    }

    printf("  %s\n", sep);
    printf("  Riusciti: %d   Errori: %d\n\n", total_ok, total_err);

    /* ── Suggerimenti scheduler ────────────────────────────────── */
    if (vram_avail_mb > 0 && total_ok > 0) {
        printf("  💡 Suggerimenti per AgentScheduler:\n");

        /* Trova il modello più pesante e quello più leggero */
        int max_vram = -1, min_vram = 999999;
        const char *heavy_model = NULL, *light_model = NULL;
        for (int i = 0; i < n; i++) {
            if (!profiles[i].success) continue;
            if (profiles[i].vram_delta_mb > max_vram) {
                max_vram    = profiles[i].vram_delta_mb;
                heavy_model = profiles[i].model;
            }
            if (profiles[i].vram_delta_mb > 0 &&
                profiles[i].vram_delta_mb < min_vram) {
                min_vram    = profiles[i].vram_delta_mb;
                light_model = profiles[i].model;
            }
        }

        if (heavy_model)
            printf("    🏋️  Modello più pesante: %s (%d MB)\n",
                   heavy_model, max_vram);
        if (light_model && light_model != heavy_model)
            printf("    🪶  Modello più leggero: %s (%d MB)\n",
                   light_model, min_vram);

        if (max_vram > 0 && max_vram <= (int)vram_avail_mb)
            printf("    ✅  Tutti i modelli entrano nella VRAM — hot/cold switching efficiente\n");
        else if (max_vram > (int)vram_avail_mb)
            printf("    ⚠️  Alcuni modelli > VRAM: usa n_gpu_layers parziale + as_assign_layers()\n");

        printf("    📄  Profilo salvato — caricalo con: as_load_vram_profile(&sched, \"vram_profile.json\")\n");
    }
    printf("\n");
}

/* ═══════════════════════════════════════════════════════════════════
   SEZIONE 8 — main
   ═══════════════════════════════════════════════════════════════════ */

static void usage(const char* prog) {
    fprintf(stderr,
        "Uso: %s [OPZIONI]\n\n"
        "Opzioni:\n"
        "  --backend ollama|llama   backend AI (default: ollama)\n"
        "  --host    HOST           host Ollama (default: 127.0.0.1)\n"
        "  --port    PORT           porta Ollama (default: 11434)\n"
        "  --models-dir DIR         cartella .gguf (default: models/)\n"
        "  --model   NOME           profila solo questo modello\n"
        "  --layers  N              n_gpu_layers (default: auto da hw_detect)\n"
        "  --output  FILE           file JSON output (default: vram_profile.json)\n"
        "  --help                   mostra questo aiuto\n\n"
        "Esempi:\n"
        "  %s                                # Ollama, tutti i modelli\n"
        "  %s --backend llama                # llama statico (richiede make vram_bench_static)\n"
        "  %s --model mistral:7b             # solo un modello Ollama\n"
        "  %s --output mio_profilo.json      # salva in file personalizzato\n\n",
        prog, prog, prog, prog, prog);
}

int main(int argc, char** argv) {
    /* ── Argomenti ─────────────────────────────────────────────── */
    const char* backend    = "ollama";
    const char* host       = "127.0.0.1";
    int         port       = 11434;
    const char* models_dir = "models";
    const char* only_model = NULL;
    int         force_layers = -1;
    const char* out_path   = "vram_profile.json";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage(argv[0]); return 0;
        }
#define ARG1(flag, var) \
        if (strcmp(argv[i], flag) == 0 && i+1 < argc) { var = argv[++i]; continue; }
        ARG1("--backend",    backend)
        ARG1("--host",       host)
        ARG1("--models-dir", models_dir)
        ARG1("--model",      only_model)
        ARG1("--output",     out_path)
        if (strcmp(argv[i], "--port") == 0 && i+1 < argc)
            { port = atoi(argv[++i]); continue; }
        if (strcmp(argv[i], "--layers") == 0 && i+1 < argc)
            { force_layers = atoi(argv[++i]); continue; }
#undef ARG1
        fprintf(stderr, "Opzione sconosciuta: %s\n", argv[i]);
        usage(argv[0]); return 1;
    }

    /* ── Banner ────────────────────────────────────────────────── */
    printf("\n");
    printf("  ╔══════════════════════════════════════════════╗\n");
    printf("  ║   Prismalux VRAM Benchmark — vram_bench      ║\n");
    printf("  ║   \"Misura prima, ottimizza dopo.\"             ║\n");
    printf("  ╚══════════════════════════════════════════════╝\n\n");

    /* ── Hardware detection ────────────────────────────────────── */
    HWInfo hw;
    hw_detect(&hw);
    hw_print(&hw);

    long long vram_avail = (hw.primary >= 0 &&
                            hw.dev[hw.primary].type != DEV_CPU)
                           ? hw.dev[hw.primary].avail_mb : 0;

    int auto_layers = hw_gpu_layers(vram_avail);
    int use_layers  = (force_layers >= 0) ? force_layers : auto_layers;

    printf("\n  Backend: %s   Layers: %d   Output: %s\n\n",
           backend, use_layers, out_path);

    /* ── Benchmark ─────────────────────────────────────────────── */
    if (strcmp(backend, "ollama") == 0) {
        /* ─── Modalità Ollama ───────────────────────────────────── */
        char model_names[MAX_PROFILES][256];
        int n_models = 0;

        if (only_model) {
            strncpy(model_names[0], only_model, 255);
            n_models = 1;
        } else {
            printf("  Recupero lista modelli da Ollama (%s:%d)...\n", host, port);
            n_models = list_ollama_models(host, port, model_names, MAX_PROFILES);
            if (n_models <= 0) {
                fprintf(stderr,
                    "  ❌ Nessun modello trovato o Ollama non raggiungibile.\n"
                    "     Assicurati che Ollama sia avviato: ollama serve\n\n");
                return 1;
            }
        }

        printf("  Trovati %d modelli. Inizio benchmark...\n\n", n_models);
        printf("  %-40s  %-8s  %-8s  %-10s\n",
               "Modello", "VRAM_MB", "RAM_MB", "Load_ms");
        printf("  %s\n", "──────────────────────────────────────────────────────────────────────");

        for (int i = 0; i < n_models && g_n_profiles < MAX_PROFILES; i++) {
            bench_ollama_model(host, port,
                               model_names[i],
                               &g_profiles[g_n_profiles]);
            g_n_profiles++;

            /* Pausa breve tra i test per lasciare stabilizzare Ollama */
#ifdef _WIN32
            Sleep(500);
#else
            usleep(500000);
#endif
        }

    } else if (strcmp(backend, "llama") == 0) {
        /* ─── Modalità llama-statico ────────────────────────────── */
#if !HAS_LW
        fprintf(stderr,
            "  ❌ Build senza llama.cpp statico.\n"
            "     Esegui prima: ./build.sh\n"
            "     Poi: make vram_bench_static\n\n");
        return 1;
#endif
        char model_names[MAX_PROFILES][256];
        int n_models = 0;

        if (only_model) {
            strncpy(model_names[0], only_model, 255);
            n_models = 1;
        } else {
            n_models = lw_list_models(models_dir, model_names, MAX_PROFILES);
            if (n_models <= 0) {
                fprintf(stderr,
                    "  ❌ Nessun file .gguf trovato in: %s\n"
                    "     Usa: ./vram_bench --models-dir /percorso/modelli\n\n",
                    models_dir);
                return 1;
            }
        }

        printf("  Trovati %d modelli in '%s'. Inizio benchmark...\n\n",
               n_models, models_dir);
        printf("  %-40s  %-6s  %-8s  %-8s  %-10s\n",
               "Modello", "Layers", "VRAM_MB", "RAM_MB", "Load_ms");
        printf("  %s\n", "──────────────────────────────────────────────────────────────────────────");

        for (int i = 0; i < n_models && g_n_profiles < MAX_PROFILES; i++) {
            bench_llama_model(models_dir, model_names[i],
                              use_layers,
                              &g_profiles[g_n_profiles]);
            g_n_profiles++;
        }

    } else {
        fprintf(stderr, "  ❌ Backend non riconosciuto: %s\n", backend);
        usage(argv[0]); return 1;
    }

    /* ── Salva JSON ────────────────────────────────────────────── */
    if (save_profile_json(out_path, g_profiles, g_n_profiles,
                          vram_avail, backend) == 0)
        printf("  💾  Profilo salvato: %s\n", out_path);

    /* ── Riepilogo ─────────────────────────────────────────────── */
    print_summary(g_profiles, g_n_profiles, vram_avail);

    return 0;
}
