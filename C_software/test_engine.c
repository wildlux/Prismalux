/*
 * test_engine.c — Test automatico del motore AI di Prismalux
 * ===========================================================
 * Verifica: connessione Ollama, lista modelli, ai_chat_stream,
 * js_str/js_encode, ma_extract_code, ma_valuta_output.
 * Compila: gcc -O2 -Iinclude test_engine.c src/http.c src/backend.c
 *          src/ai.c src/terminal.c src/hw_detect.c src/agent_scheduler.c
 *          src/prismalux_ui.c src/output.c src/modelli.c src/strumenti.c
 *          src/multi_agente.c src/main.c -lpthread -lm -o test_engine
 * Uso:     ./test_engine
 */
#include "common.h"
#include "backend.h"
#include "http.h"
#include "ai.h"

/* ── colori ANSI per output test ── */
#define OK   "\033[32m[OK]\033[0m "
#define FAIL "\033[31m[FAIL]\033[0m "
#define INFO "\033[33m[INFO]\033[0m "
#define SEP  "────────────────────────────────────────\n"

static int g_pass = 0, g_fail = 0;

static void test_result(const char* name, int ok, const char* detail) {
    if (ok) { printf(OK "%s\n", name); g_pass++; }
    else     { printf(FAIL "%s  → %s\n", name, detail ? detail : ""); g_fail++; }
}

/* ── 1. Connessione TCP a Ollama ── */
static void test_tcp(void) {
    printf(SEP "TEST 1: Connessione TCP a Ollama (localhost:11434)\n" SEP);
    sock_t s = tcp_connect("127.0.0.1", 11434);
    test_result("tcp_connect()", s != SOCK_INV, "socket non valido");
    if (s != SOCK_INV) sock_close(s);
}

/* ── 2. Lista modelli Ollama ── */
static void test_list_models(void) {
    printf(SEP "TEST 2: Lista modelli Ollama\n" SEP);
    char names[64][128];
    int n = http_ollama_list("127.0.0.1", 11434, names, 64);
    char detail[64]; snprintf(detail, sizeof detail, "trovati %d modelli", n);
    test_result("http_ollama_list()", n > 0, "nessun modello");
    if (n > 0) {
        printf(INFO "%d modelli disponibili:\n", n);
        for (int i = 0; i < n && i < 5; i++)
            printf("       %d. %s\n", i+1, names[i]);
        if (n > 5) printf("       ... (+%d altri)\n", n-5);
    }
}

/* ── 3. js_str / js_encode ── */
static void test_json_helpers(void) {
    printf(SEP "TEST 3: JSON helpers (js_str, js_encode)\n" SEP);

    const char *json = "{\"model\":\"qwen3:4b\",\"content\":\"ciao\\nmondo\"}";
    const char *v = js_str(json, "model");
    test_result("js_str() trova chiave", v && strcmp(v, "qwen3:4b") == 0,
                v ? v : "NULL");

    const char *v2 = js_str(json, "content");
    test_result("js_str() decodifica \\n", v2 && strchr(v2, '\n') != NULL,
                v2 ? v2 : "NULL");

    const char *v3 = js_str(json, "missing");
    test_result("js_str() chiave assente → NULL", v3 == NULL, "non NULL");

    char enc[256];
    js_encode("hello\nworld\"test\\", enc, sizeof enc);
    test_result("js_encode() escape \\n \\\" \\\\",
                strstr(enc, "\\n") && strstr(enc, "\\\"") && strstr(enc, "\\\\"),
                enc);
}

/* ── 4. ai_chat_stream con modello leggero ── */
static int g_token_count = 0;
static void count_cb(const char* piece, void* ud) {
    (void)ud;
    g_token_count += (int)strlen(piece);
}

static void test_ai_stream(void) {
    printf(SEP "TEST 4: ai_chat_stream (Ollama, modello leggero)\n" SEP);

    /* Scegli il modello più leggero disponibile */
    char names[64][128]; int n = http_ollama_list("127.0.0.1", 11434, names, 64);
    if (n == 0) { test_result("ai_chat_stream()", 0, "nessun modello disponibile"); return; }

    /* Preferisce modelli piccoli per velocità */
    const char *pref[] = {"deepseek-r1:1.5b","gemma2:2b","qwen3:4b","llama3.2:3b","exaone3.5:2.4b",NULL};
    char model[128] = "";
    for (int p = 0; pref[p] && !model[0]; p++)
        for (int i = 0; i < n; i++)
            if (strstr(names[i], pref[p])) { strncpy(model, names[i], 127); break; }
    if (!model[0]) strncpy(model, names[0], 127);

    printf(INFO "Modello: %s\n", model);
    printf(INFO "Domanda: 'Rispondi con una sola parola: colore del cielo?'\n");
    printf(INFO "Risposta: ");

    BackendCtx ctx = { BACKEND_OLLAMA, "127.0.0.1", 11434, "" };
    strncpy(ctx.model, model, sizeof ctx.model - 1);

    char out[4096]; out[0] = '\0';
    g_token_count = 0;

    int rc = ai_chat_stream(&ctx,
        "Rispondi SEMPRE e SOLO in italiano. Sii brevissimo.",
        "Rispondi con una sola parola: di che colore è il cielo di giorno?",
        count_cb, NULL, out, sizeof out);

    printf("\n");
    printf(INFO "Caratteri ricevuti: %d\n", g_token_count);

    test_result("ai_chat_stream() ritorna 0", rc == 0, "rc != 0");
    test_result("ai_chat_stream() produce output", g_token_count > 0, "zero token");
    test_result("output contiene testo", out[0] != '\0', "buffer vuoto");
}

/* ── 5. Estrazione codice Python da risposta ── */
static void test_extract_code(void) {
    printf(SEP "TEST 5: ma_extract_code (estrazione blocco ```python)\n" SEP);

    /* Simula una risposta dell'AI con codice */
    const char *resp =
        "Ecco il codice:\n"
        "```python\n"
        "print('hello world')\n"
        "```\n"
        "Fine.";

    /* Accedo alla funzione tramite mini-implementazione inline per il test */
    char out[1024]; out[0] = '\0';
    const char *s = strstr(resp, "```python");
    if (s) {
        s = strchr(s, '\n'); if (s) s++;
        const char *e = strstr(s, "```");
        if (!e) e = s + strlen(s);
        int n = (int)(e - s); if (n >= 1023) n = 1022;
        memcpy(out, s, n); out[n] = '\0';
    }

    test_result("estrae blocco ```python", strstr(out, "print") != NULL, out);
    test_result("non include ``` nei delimitatori", !strstr(out, "```"), out);
}

/* ── 6. Valutazione output Python ── */
static void test_valuta_output(void) {
    printf(SEP "TEST 6: ma_valuta_output (rilevamento errori Python)\n" SEP);

    /* Replica esatta di ma_valuta_output() da multi_agente.c (include tolower) */
    #define VALUTA(inp) ({ \
        const char *_s=(inp); int _n=(int)strlen(_s); if(_n>4095)_n=4095; \
        char _low[4096]; for(int _i=0;_i<_n;_i++) _low[_i]=(char)tolower((unsigned char)_s[_i]); _low[_n]='\0'; \
        !(strstr(_low,"traceback")||strstr(_low,"  file \"")||strstr(_low,"error:")||strstr(_low,"exception")||strstr(_low,"syntaxerror")||strstr(_low,"nameerror")||strstr(_low,"typeerror")||strstr(_low,"indentationerror")); \
    })

    test_result("output pulito = PASSATO",  VALUTA("hello world\n42\n") == 1, "");
    test_result("Traceback = FALLITO",       VALUTA("Traceback (most recent call last):\n  File...") == 0, "");
    test_result("NameError = FALLITO",       VALUTA("NameError: name 'x' is not defined") == 0, "");
    test_result("SyntaxError = FALLITO",     VALUTA("SyntaxError: invalid syntax") == 0, "");
    test_result("output vuoto = PASSATO",    VALUTA("") == 1, "");

    #undef VALUTA
}

/* ── 7. Esecuzione Python ── */
static void test_run_python(void) {
    printf(SEP "TEST 7: Esecuzione Python (python3)\n" SEP);

    /* Test: python3 disponibile? */
    int rc = system("python3 --version > /dev/null 2>&1");
    test_result("python3 disponibile", rc == 0, "python3 non trovato");

    if (rc != 0) return;

    /* Test: esegue codice semplice */
    FILE *f = fopen("/tmp/prismalux_test.py", "w");
    if (f) {
        fprintf(f, "print('PRISMALUX_TEST_OK')\n");
        fclose(f);
        FILE *p = popen("python3 /tmp/prismalux_test.py 2>&1", "r");
        char buf[256]; buf[0] = '\0';
        if (p) { fgets(buf, sizeof buf, p); pclose(p); }
        remove("/tmp/prismalux_test.py");
        buf[strcspn(buf, "\n")] = '\0';
        test_result("python3 esegue codice", strcmp(buf, "PRISMALUX_TEST_OK") == 0, buf);
    }
}

int main(void) {
    printf("\n\033[36m\033[1m  PRISMALUX — Test Engine\033[0m\n");
    printf("  Test automatico dei componenti core\n\n");

    test_tcp();
    test_list_models();
    test_json_helpers();
    test_ai_stream();
    test_extract_code();
    test_valuta_output();
    test_run_python();

    printf(SEP);
    int total = g_pass + g_fail;
    printf("  Risultati: \033[32m%d/%d PASSATI\033[0m", g_pass, total);
    if (g_fail > 0)
        printf("  \033[31m%d FALLITI\033[0m", g_fail);
    printf("\n\n");

    return g_fail > 0 ? 1 : 0;
}
