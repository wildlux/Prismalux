/* test_toon_config.c — Test formato TOON e persistenza config Prismalux
 * =======================================================================
 * Controlli di regressione per il parser/serializer TOON e il ciclo
 * completo di salvataggio/lettura della configurazione.
 *
 * Eseguire con:
 *   make http && gcc -O2 -Iinclude -o test_toon_config test_toon_config.c src/config_toon.c && ./test_toon_config
 *
 * I test coprono scenari che potrebbero causare regressioni future:
 *   - Valori contenenti ':' (nomi modelli Ollama: "qwen3.5:9b")
 *   - Chiave alla prima riga (senza \n iniziale)
 *   - Commenti interlacciati
 *   - Buffer piccoli (overflow protection)
 *   - Chiavi inesistenti
 *   - Round-trip completo write→read della config Prismalux
 *   - Valori vuoti
 *   - Chiavi che sono prefisso di altre chiavi ("model" vs "ollama_model")
 *   - File con CRLF (Windows)
 *   - Valori con spazi finali (trim)
 *   - Stress: 500 chiavi
 */
#include "config_toon.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

/* ── ANSI ── */
#define GRN "\033[32m"
#define RED "\033[31m"
#define YEL "\033[33m"
#define CYN "\033[36m"
#define RST "\033[0m"
#define BLD "\033[1m"

static int passed = 0, failed = 0;

static void check(const char *test, int cond, const char *detail) {
    if (cond) {
        printf(GRN "  \xe2\x9c\x85 PASS" RST "  %s\n", test);
        passed++;
    } else {
        printf(RED "  \xe2\x9d\x8c FAIL" RST "  %s", test);
        if (detail) printf("\n         " YEL "%s" RST, detail);
        printf("\n");
        failed++;
    }
}

/* ── helper: scrive un file temporaneo ── */
static void write_tmp(const char *path, const char *content) {
    FILE *f = fopen(path, "w");
    if (!f) { perror(path); exit(1); }
    fputs(content, f);
    fclose(f);
}

/* ── helper: legge file in buffer ── */
static void read_file(const char *path, char *buf, int maxlen) {
    FILE *f = fopen(path, "r");
    if (!f) { buf[0] = '\0'; return; }
    int n = (int)fread(buf, 1, maxlen - 1, f);
    buf[n] = '\0';
    fclose(f);
}

/* ──────────────────────────────────────────────────────────────────────────
   SEZIONE 1 — toon_get: lettura da buffer in memoria
   ────────────────────────────────────────────────────────────────────────── */
static void test_get_basic(void) {
    printf("\n" CYN BLD "── toon_get: lettura da buffer ────────────────────────────\n" RST);

    /* Config tipica Prismalux */
    const char *cfg =
        "# Config Prismalux\n"
        "backend: ollama\n"
        "ollama_host: 127.0.0.1\n"
        "ollama_port: 11434\n"
        "ollama_model: qwen2.5-coder:7b\n"
        "lserver_host: 127.0.0.1\n"
        "lserver_port: 8081\n"
        "lserver_model: qwen3.5:9b\n"
        "llama_model: models/qwen3-4b.gguf\n";

    char out[256];

    /* Chiavi semplici */
    check("backend: ollama",
          toon_get(cfg, "backend", out, sizeof out) && strcmp(out, "ollama") == 0, out);

    check("ollama_host: 127.0.0.1",
          toon_get(cfg, "ollama_host", out, sizeof out) && strcmp(out, "127.0.0.1") == 0, out);

    /* Valore porta (intero come stringa) */
    check("ollama_port: 11434",
          toon_get(cfg, "ollama_port", out, sizeof out) && strcmp(out, "11434") == 0, out);

    /* CASO CRITICO: valore con ':' (nome modello Ollama) */
    check("ollama_model con ':' → qwen2.5-coder:7b",
          toon_get(cfg, "ollama_model", out, sizeof out) && strcmp(out, "qwen2.5-coder:7b") == 0, out);

    check("lserver_model con ':' → qwen3.5:9b",
          toon_get(cfg, "lserver_model", out, sizeof out) && strcmp(out, "qwen3.5:9b") == 0, out);

    /* Path con slash */
    check("llama_model path con '/' → models/qwen3-4b.gguf",
          toon_get(cfg, "llama_model", out, sizeof out) && strcmp(out, "models/qwen3-4b.gguf") == 0, out);

    /* Chiave inesistente */
    check("chiave inesistente → NULL",
          toon_get(cfg, "nonexistent_key", out, sizeof out) == NULL, NULL);

    /* CASO CRITICO: "model" non deve matchare "ollama_model" */
    check("'model' non matcha 'ollama_model' (falso positivo prevenuto)",
          toon_get(cfg, "model", out, sizeof out) == NULL, out);

    /* Commenti non matchano */
    check("riga commento '#' ignorata",
          toon_get(cfg, "# Config Prismalux", out, sizeof out) == NULL, out);
}

/* ──────────────────────────────────────────────────────────────────────────
   SEZIONE 2 — toon_get: chiave alla prima riga (no \n iniziale)
   ────────────────────────────────────────────────────────────────────────── */
static void test_first_line(void) {
    printf("\n" CYN BLD "── toon_get: chiave alla prima riga ───────────────────────\n" RST);

    const char *cfg = "backend: llamaserver\nport: 8081\n";
    char out[64];

    check("prima riga senza \\n iniziale → 'backend'",
          toon_get(cfg, "backend", out, sizeof out) && strcmp(out, "llamaserver") == 0, out);

    check("seconda riga normale → 'port'",
          toon_get(cfg, "port", out, sizeof out) && strcmp(out, "8081") == 0, out);
}

/* ──────────────────────────────────────────────────────────────────────────
   SEZIONE 3 — toon_get_int
   ────────────────────────────────────────────────────────────────────────── */
static void test_get_int(void) {
    printf("\n" CYN BLD "── toon_get_int ────────────────────────────────────────────\n" RST);

    const char *cfg =
        "port: 11434\n"
        "port2: 0\n"
        "port_neg: -1\n"
        "port_str: abc\n"
        "port_mixed: 123abc\n";

    char det[64];

    int v = toon_get_int(cfg, "port", 9999);
    snprintf(det, sizeof det, "got %d", v);
    check("port: 11434 → 11434", v == 11434, det);

    v = toon_get_int(cfg, "port2", 9999);
    snprintf(det, sizeof det, "got %d", v);
    check("port2: 0 → 0", v == 0, det);

    v = toon_get_int(cfg, "port_neg", 9999);
    snprintf(det, sizeof det, "got %d", v);
    check("port_neg: -1 → -1", v == -1, det);

    v = toon_get_int(cfg, "port_str", 9999);
    snprintf(det, sizeof det, "got %d", v);
    check("port_str: abc → default 9999", v == 9999, det);

    v = toon_get_int(cfg, "missing", 42);
    snprintf(det, sizeof det, "got %d", v);
    check("chiave mancante → default 42", v == 42, det);

    /* Porta fuori range (>65535): non è un errore TOON, ma annotazione */
    v = toon_get_int(cfg, "port_mixed", 9999);
    snprintf(det, sizeof det, "got %d (strtol ferma a '123')", v);
    check("port_mixed: 123abc → 123 (strtol ferma al non-digit)", v == 123, det);
}

/* ──────────────────────────────────────────────────────────────────────────
   SEZIONE 4 — toon_get: protezione buffer piccolo
   ────────────────────────────────────────────────────────────────────────── */
static void test_buffer_overflow(void) {
    printf("\n" CYN BLD "── Buffer overflow protection ──────────────────────────────\n" RST);

    const char *cfg = "key: ThisIsAVeryLongValueThatExceedsSmallBufferSize1234567890\n";
    char small[16];
    char large[128];

    /* Buffer piccolo: non deve scrivere oltre small[] */
    const char *r = toon_get(cfg, "key", small, sizeof small);
    check("buffer 16: ritorna non-NULL", r != NULL, NULL);
    check("buffer 16: lunghezza ≤ 15", (int)strlen(small) <= 15, small);
    check("buffer 16: null-terminato", small[15] == '\0', NULL);

    /* Buffer grande: valore completo */
    toon_get(cfg, "key", large, sizeof large);
    check("buffer 128: valore completo",
          strcmp(large, "ThisIsAVeryLongValueThatExceedsSmallBufferSize1234567890") == 0, large);

    /* outmax = 1: deve scrivere solo '\0' e non crashare */
    char one[1];
    one[0] = 0xFF;
    toon_get(cfg, "key", one, 1);
    check("outmax=1: null-terminato senza crash", one[0] == '\0', NULL);

    /* Input NULL → NULL ritornato */
    check("buf=NULL → NULL", toon_get(NULL, "key", large, sizeof large) == NULL, NULL);
    check("key=NULL → NULL", toon_get(cfg, NULL, large, sizeof large) == NULL, NULL);
    check("out=NULL → NULL", toon_get(cfg, "key", NULL, sizeof large) == NULL, NULL);
    check("outmax=0 → NULL", toon_get(cfg, "key", large, 0) == NULL, NULL);
}

/* ──────────────────────────────────────────────────────────────────────────
   SEZIONE 5 — Valori edge case
   ────────────────────────────────────────────────────────────────────────── */
static void test_edge_values(void) {
    printf("\n" CYN BLD "── Edge case valori ────────────────────────────────────────\n" RST);

    /* Valore vuoto */
    const char *cfg_empty = "key1: \nkey2: normale\n";
    char out[128];
    const char *r = toon_get(cfg_empty, "key1", out, sizeof out);
    check("valore vuoto → stringa vuota (trim spazio)",
          r != NULL && out[0] == '\0', out);

    /* Spazi finali nel valore devono essere trimmati */
    const char *cfg_trail = "key: valore   \nkey2: ok\n";
    toon_get(cfg_trail, "key", out, sizeof out);
    check("trailing spaces trimmati → 'valore' senza spazi",
          strcmp(out, "valore") == 0, out);

    /* CRLF (Windows) — il \r deve essere ignorato nel valore */
    const char *cfg_crlf = "backend: ollama\r\nport: 11434\r\n";
    toon_get(cfg_crlf, "backend", out, sizeof out);
    /* La specifica attuale legge fino a \n o \r, quindi "ollama" senza \r */
    check("CRLF: 'backend' senza \\r",
          strcmp(out, "ollama") == 0, out);

    /* Doppi punti nel valore (URL stile) */
    const char *cfg_colons = "url: http://127.0.0.1:11434/api\n";
    toon_get(cfg_colons, "url", out, sizeof out);
    check("url con '://' → valore completo con tutti i ':'",
          strcmp(out, "http://127.0.0.1:11434/api") == 0, out);

    /* Unicode in valore (path con caratteri italiani) */
    const char *cfg_utf8 = "path: /home/utente/Desktop/Prismalux\n";
    toon_get(cfg_utf8, "path", out, sizeof out);
    check("path UTF-8 senza spazi",
          strcmp(out, "/home/utente/Desktop/Prismalux") == 0, out);

    /* Chiave che inizia come un'altra */
    const char *cfg_prefix =
        "port: 1234\n"
        "port_ext: 5678\n";
    toon_get(cfg_prefix, "port", out, sizeof out);
    check("'port' non matcha 'port_ext' (falso positivo con _ext)",
          strcmp(out, "1234") == 0, out);
    toon_get(cfg_prefix, "port_ext", out, sizeof out);
    check("'port_ext' trovato correttamente",
          strcmp(out, "5678") == 0, out);
}

/* ──────────────────────────────────────────────────────────────────────────
   SEZIONE 6 — Round-trip completo: write → file → read
   ────────────────────────────────────────────────────────────────────────── */
static void test_roundtrip(void) {
    printf("\n" CYN BLD "── Round-trip write → read ─────────────────────────────────\n" RST);

    const char *tmppath = "/tmp/prismalux_test_config.toon";

    /* Scrittura */
    FILE *f = fopen(tmppath, "w");
    if (!f) { printf(RED "  SKIP: impossibile aprire /tmp\n" RST); return; }

    toon_write_comment(f, "Prismalux test config");
    toon_write_str(f, "backend", "ollama");
    toon_write_str(f, "ollama_host", "127.0.0.1");
    toon_write_int(f, "ollama_port", 11434);
    toon_write_str(f, "ollama_model", "qwen3.5:9b");
    toon_write_str(f, "lserver_host", "127.0.0.1");
    toon_write_int(f, "lserver_port", 8081);
    toon_write_str(f, "lserver_model", "deepseek-r1:7b");
    toon_write_str(f, "llama_model", "/home/utente/models/qwen3-4b-q8.gguf");
    fclose(f);

    /* Lettura */
    char buf[4096];
    read_file(tmppath, buf, sizeof buf);

    char out[512];
    check("backend: ollama",
          toon_get(buf, "backend", out, sizeof out) && strcmp(out, "ollama") == 0, out);

    check("ollama_port: 11434",
          toon_get_int(buf, "ollama_port", 0) == 11434, NULL);

    check("ollama_model con ':' → qwen3.5:9b",
          toon_get(buf, "ollama_model", out, sizeof out) && strcmp(out, "qwen3.5:9b") == 0, out);

    check("lserver_port: 8081",
          toon_get_int(buf, "lserver_port", 0) == 8081, NULL);

    check("lserver_model con ':' → deepseek-r1:7b",
          toon_get(buf, "lserver_model", out, sizeof out) && strcmp(out, "deepseek-r1:7b") == 0, out);

    check("llama_model path lungo",
          toon_get(buf, "llama_model", out, sizeof out) &&
          strcmp(out, "/home/utente/models/qwen3-4b-q8.gguf") == 0, out);

    /* Commento non deve essere una chiave leggibile */
    check("commento non leggibile come chiave",
          toon_get(buf, "# Prismalux test config", out, sizeof out) == NULL, NULL);

    remove(tmppath);
}

/* ──────────────────────────────────────────────────────────────────────────
   SEZIONE 7 — Stress: buffer con molte chiavi
   ────────────────────────────────────────────────────────────────────────── */
static void test_stress(void) {
    printf("\n" CYN BLD "── Stress: 200 chiavi, ricerca ultima ──────────────────────\n" RST);

    /* Costruisce un buffer con 200 chiavi key_000...key_199 */
    static char big[32768];
    int pos = 0;
    for (int i = 0; i < 200 && pos < (int)sizeof(big) - 64; i++) {
        pos += snprintf(big + pos, sizeof(big) - pos,
                        "key_%03d: value_%03d\n", i, i * 7);
    }

    char out[64];
    /* Cerca l'ultima chiave */
    const char *r = toon_get(big, "key_199", out, sizeof out);
    check("key_199 trovata in buffer con 200 chiavi",
          r != NULL && strcmp(out, "value_1393") == 0, out);

    /* Cerca la prima */
    r = toon_get(big, "key_000", out, sizeof out);
    check("key_000 trovata (prima riga)",
          r != NULL && strcmp(out, "value_000") == 0, out);

    /* Chiave che non esiste */
    check("key_999 non trovata",
          toon_get(big, "key_999", out, sizeof out) == NULL, NULL);

    /* Timing */
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < 10000; i++)
        toon_get(big, "key_199", out, sizeof out);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    long ns = (t1.tv_sec - t0.tv_sec) * 1000000000L + (t1.tv_nsec - t0.tv_nsec);
    char det[64];
    snprintf(det, sizeof det, "10k ricerche in %ld ms", ns / 1000000L);
    check("10k ricerche su buffer 200 chiavi < 100ms", ns < 100000000L, det);
}

/* ──────────────────────────────────────────────────────────────────────────
   SEZIONE 8 — Scenari config Prismalux reali
   ────────────────────────────────────────────────────────────────────────── */
static void test_prismalux_scenarios(void) {
    printf("\n" CYN BLD "── Scenari config Prismalux reali ──────────────────────────\n" RST);

    /* Scenario 1: config minimale (solo backend) */
    {
        const char *minimal = "backend: ollama\n";
        char out[128];
        check("config minimale: backend=ollama",
              toon_get(minimal, "backend", out, sizeof out) && strcmp(out, "ollama") == 0, out);
        check("config minimale: ollama_port mancante → default 11434",
              toon_get_int(minimal, "ollama_port", 11434) == 11434, NULL);
    }

    /* Scenario 2: backend llama-server (porte custom) */
    {
        const char *srv_cfg =
            "backend: llamaserver\n"
            "lserver_host: 192.168.1.100\n"
            "lserver_port: 8082\n"
            "lserver_model: mistral:7b-instruct\n";
        char out[128];
        check("llama-server config: host remoto",
              toon_get(srv_cfg, "lserver_host", out, sizeof out) && strcmp(out, "192.168.1.100") == 0, out);
        check("llama-server config: porta 8082",
              toon_get_int(srv_cfg, "lserver_port", 0) == 8082, NULL);
        check("llama-server model con ':' → mistral:7b-instruct",
              toon_get(srv_cfg, "lserver_model", out, sizeof out) && strcmp(out, "mistral:7b-instruct") == 0, out);
    }

    /* Scenario 3: config corrotta (duplicati, usa il primo) */
    {
        const char *dup_cfg =
            "backend: ollama\n"
            "ollama_port: 11434\n"
            "ollama_port: 9999\n";  /* duplicato: toon_get trova il primo \n */
        char out[64];
        int port = toon_get_int(dup_cfg, "ollama_port", 0);
        char det[64]; snprintf(det, sizeof det, "got %d", port);
        check("chiave duplicata: vince il primo valore (11434)",
              port == 11434, det);
    }

    /* Scenario 4: path Windows nel toon (backslash) */
    {
        const char *win_cfg = "llama_model: C:\\Users\\Paolo\\models\\qwen3.gguf\n";
        char out[256];
        toon_get(win_cfg, "llama_model", out, sizeof out);
        check("path Windows con backslash preservato",
              strcmp(out, "C:\\Users\\Paolo\\models\\qwen3.gguf") == 0, out);
    }

    /* Scenario 5: valore con spazi INTERNI (es. percorso con spazio) */
    {
        const char *space_cfg = "llama_model: /home/wild lux/models/qwen3.gguf\n";
        char out[256];
        toon_get(space_cfg, "llama_model", out, sizeof out);
        /* Il parser legge fino a \n, quindi gli spazi interni sono preservati */
        check("spazi interni nel valore preservati",
              strcmp(out, "/home/wild lux/models/qwen3.gguf") == 0, out);
    }

    /* Scenario 6: config con tutti i backend (migrazione JSON→TOON) */
    {
        const char *full_cfg =
            "# Prismalux config — formato TOON\n"
            "backend: llama\n"
            "ollama_host: 127.0.0.1\n"
            "ollama_port: 11434\n"
            "ollama_model: qwen3.5:latest\n"
            "lserver_host: 127.0.0.1\n"
            "lserver_port: 8081\n"
            "lserver_model: qwen3:8b\n"
            "llama_model: models/deepseek-r1-4b-q8.gguf\n";
        char out[256];

        check("full config: backend=llama",
              toon_get(full_cfg, "backend", out, sizeof out) && strcmp(out, "llama") == 0, out);
        check("full config: ollama_model con tag ':latest'",
              toon_get(full_cfg, "ollama_model", out, sizeof out) && strcmp(out, "qwen3.5:latest") == 0, out);
        check("full config: llama_model path relativo",
              toon_get(full_cfg, "llama_model", out, sizeof out) && strcmp(out, "models/deepseek-r1-4b-q8.gguf") == 0, out);
    }
}

/* ── Main ── */
int main(void) {
    printf(BLD "\n\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n"
           "  test_toon_config — parser TOON + persistenza config\n"
           "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n" RST);

    test_get_basic();
    test_first_line();
    test_get_int();
    test_buffer_overflow();
    test_edge_values();
    test_roundtrip();
    test_stress();
    test_prismalux_scenarios();

    int total = passed + failed;
    printf("\n" BLD
           "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n"
           RST);
    if (failed == 0) {
        printf(GRN BLD "  \xe2\x9c\x85 %d/%d test PASSATI\n" RST, passed, total);
    } else {
        printf(RED BLD "  \xe2\x9d\x8c %d FALLITI  |  " GRN "%d passati  |  %d totale\n" RST,
               failed, passed, total);
    }
    printf(BLD "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n\n" RST);

    return failed > 0 ? 1 : 0;
}
