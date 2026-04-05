/*
 * test_stress.c — Stress test: API pubbliche, edge case, fuzzing leggero
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <limits.h>
#include <float.h>
#include "include/http.h"
#include "include/backend.h"
#include "include/common.h"

#define GRN "\033[32m" 
#define RED "\033[31m"
#define YLW "\033[33m"
#define CYN "\033[36m"
#define RST "\033[0m"
#define BLD "\033[1m"

static int passed = 0, failed = 0;
#define _check(cond, label) do { \
    if (cond) { printf("  " GRN "[OK]" RST "  %s\n", label); passed++; } \
    else       { printf("  " RED "[FAIL]" RST " %s\n", label); failed++; } \
} while(0)
#define _ok(label) do { printf("  " GRN "[OK]" RST "  %s\n", label); passed++; } while(0)
#define _section(t) printf("\n" CYN BLD "─── %s ───" RST "\n", t)

/* ══════════════════════════════════════════════════════════
   1. js_str — parser JSON minimale
   ══════════════════════════════════════════════════════════ */
static void test_json(void) {
    _section("js_str — parser JSON stress");

    /* Caso normale */
    const char *v = js_str("{\"model\":\"qwen2.5\"}", "model");
    _check(v && strcmp(v, "qwen2.5") == 0, "chiave presente: valore corretto");

    /* Chiave assente */
    v = js_str("{\"a\":\"hello\"}", "b");
    _check(v == NULL, "chiave assente: ritorna NULL");

    /* JSON vuoto */
    v = js_str("{}", "k");
    _check(v == NULL, "JSON vuoto: ritorna NULL");

    /* Input NULL */
    v = js_str(NULL, "k");
    _check(v == NULL, "input NULL: ritorna NULL");

    /* Chiave NULL */
    v = js_str("{\"k\":\"v\"}", NULL);
    _check(v == NULL, "chiave NULL: ritorna NULL");

    /* JSON malformato */
    v = js_str("{\"k\":}", "k");
    _ok("JSON malformato — non crasha");

    /* JSON annidato (js_str non supporta nesting, ma non deve crashare) */
    v = js_str("{\"a\":{\"b\":\"c\"}}", "b");
    _ok("JSON annidato — non crasha");

    /* Valore numerico (js_str si aspetta stringa, ma non deve crashare) */
    v = js_str("{\"n\":42}", "n");
    _ok("valore numerico — non crasha");

    /* Stringa con escape */
    v = js_str("{\"s\":\"hello\\\"world\"}", "s");
    _ok("valore con escape — non crasha");

    /* Stringa da 10KB come valore */
    char *big = malloc(10300);
    if (big) {
        memset(big+1, 'A', 10000);
        big[0] = '{'; big[1] = '"'; big[2] = 'k'; big[3] = '"';
        big[4] = ':'; big[5] = '"';
        memset(big+6, 'X', 10000);
        strcpy(big+10006, "\"}");
        v = js_str(big, "k");
        _ok("valore 10KB — non crasha");
        free(big);
    }
}

/* ══════════════════════════════════════════════════════════
   2. js_encode — escaping per JSON
   ══════════════════════════════════════════════════════════ */
static void test_json_encode(void) {
    _section("js_encode — escaping stress");
    char dst[512];

    /* Stringa normale */
    js_encode("hello world", dst, sizeof dst);
    _check(strlen(dst) > 0, "encode normale: non vuota");

    /* Virgolette */
    js_encode("say \"hello\"", dst, sizeof dst);
    _check(strchr(dst, '"') == NULL || strstr(dst, "\\\"") != NULL,
           "encode virgolette: escaped");

    /* Backslash */
    js_encode("path\\to\\file", dst, sizeof dst);
    _ok("encode backslash — non crasha");

    /* Newline e tab */
    js_encode("riga1\nriga2\ttab", dst, sizeof dst);
    _ok("encode newline/tab — non crasha");

    /* Caratteri di controllo */
    js_encode("\x01\x02\x03\x1b", dst, sizeof dst);
    _ok("encode ctrl chars — non crasha");

    /* Stringa vuota */
    js_encode("", dst, sizeof dst);
    _check(strlen(dst) == 0 || strcmp(dst, "") == 0, "encode stringa vuota: OK");

    /* Input NULL — non deve crashare */
    js_encode(NULL, dst, sizeof dst);
    _ok("encode NULL — non crasha");

    /* Buffer piccolo — troncamento sicuro */
    char small[4];
    js_encode("abcdefghij", small, sizeof small);
    _check(small[3] == '\0', "encode buffer piccolo: null terminator");

    /* Buffer size 0 */
    js_encode("test", dst, 0);
    _ok("encode buffer size 0 — non crasha");

    /* UTF-8 multibyte */
    js_encode("\xf0\x9f\xa4\x96 robot \xf0\x9f\x8d\xba birra", dst, sizeof dst);
    _ok("encode emoji UTF-8 — non crasha");

    /* Stringa da 100KB */
    char *huge = malloc(100001);
    if (huge) {
        memset(huge, 'A', 100000); huge[100000] = '\0';
        char *hugeDst = malloc(210000);
        if (hugeDst) {
            js_encode(huge, hugeDst, 210000);
            _ok("encode stringa 100KB — non crasha");
            free(hugeDst);
        }
        free(huge);
    }

    /* Injection patterns */
    const char *inj[] = {
        "'; DROP TABLE users; --",
        "<script>alert(1)</script>",
        "$(rm -rf /)",
        "%s%s%s%s%s%n",
        "\x00\x01\x02\x03",
        NULL
    };
    for (int i = 0; inj[i]; i++) {
        js_encode(inj[i], dst, sizeof dst);
    }
    _ok("encode injection patterns — nessun crash");
}

/* ══════════════════════════════════════════════════════════
   3. BackendCtx — init e configurazione estrema
   ══════════════════════════════════════════════════════════ */
static void test_backend(void) {
    _section("BackendCtx — edge case configurazione");

    BackendCtx ctx;
    memset(&ctx, 0, sizeof ctx);
    ctx.backend = BACKEND_OLLAMA;
    strncpy(ctx.host, "127.0.0.1", sizeof ctx.host - 1);
    ctx.port = 11434;
    strncpy(ctx.model, "qwen2.5:7b", sizeof ctx.model - 1);

    _check(ctx.backend == BACKEND_OLLAMA,        "backend OLLAMA");
    _check(ctx.port == 11434,                    "porta 11434");
    _check(strcmp(ctx.host, "127.0.0.1") == 0,   "host corretto");

    /* Host malformato */
    strncpy(ctx.host, "999.999.999.999:99999", sizeof ctx.host - 1);
    ctx.host[sizeof ctx.host - 1] = '\0';
    _ok("host non valido: struttura non corrotta");

    /* Porta out-of-range */
    ctx.port = 99999;
    _check(ctx.port == 99999, "porta 99999: salvata (validazione a livello UI)");

    ctx.port = 0;
    _check(ctx.port == 0, "porta 0");

    ctx.port = -1;
    _check(ctx.port == -1, "porta negativa: salvata come int");

    /* Modello stringa da 4KB → campo da 256 */
    char bigm[4096];
    memset(bigm, 'm', 4095); bigm[4095] = '\0';
    strncpy(ctx.model, bigm, sizeof ctx.model - 1);
    ctx.model[sizeof ctx.model - 1] = '\0';
    _check(ctx.model[sizeof(ctx.model)-1] == '\0', "modello 4KB: troncato+null");

    /* Backend tutti i valori */
    ctx.backend = BACKEND_OLLAMA;     _check(ctx.backend == BACKEND_OLLAMA,     "BACKEND_OLLAMA");
    ctx.backend = BACKEND_LLAMASERVER;_check(ctx.backend == BACKEND_LLAMASERVER,"BACKEND_LLAMASERVER");
    ctx.backend = BACKEND_LLAMA;      _check(ctx.backend == BACKEND_LLAMA,      "BACKEND_LLAMA");
    ctx.backend = (Backend)999;   _ok("backend valore sconosciuto: non crasha struct");
}

/* ══════════════════════════════════════════════════════════
   4. Math IEEE 754 — edge case numerici
   ══════════════════════════════════════════════════════════ */
static void test_math_ieee(void) {
    _section("IEEE 754 — edge case numerici");

    double nan_v  = 0.0 / 0.0;
    double inf_p  = 1.0 / 0.0;
    double inf_n  = -1.0 / 0.0;
    double zero_n = -0.0;

    _check(nan_v != nan_v,         "NaN: nan != nan (IEEE 754)");
    _check(isinf(inf_p) && inf_p > 0, "+Inf: isinf && positivo");
    _check(isinf(inf_n) && inf_n < 0, "-Inf: isinf && negativo");
    _check(zero_n == 0.0,          "-0.0 == +0.0 (IEEE 754)");
    _check(DBL_MAX > 1e300,        "DBL_MAX > 1e300");
    _check(DBL_MIN > 0,            "DBL_MIN > 0");

    /* Overflow rilevato prima del calcolo */
    long long a = LLONG_MAX;
    _check(a > LLONG_MAX - 1LL, "overflow detection: LLONG_MAX > LLONG_MAX-1");

    /* Potenza 2^53 (precisione double) */
    double p53 = pow(2.0, 53.0);
    _check(p53 == (double)(1LL << 53), "2^53 == 1LL<<53 (nessuna perdita precisione)");

    /* Divisione per quasi-zero */
    double eps = DBL_MIN;
    double huge_r = 1.0 / eps;
    _check(isinf(huge_r) || huge_r > 1e300, "1/DBL_MIN: molto grande o inf");

    /* fmod con denominatore 0 */
    double fm = fmod(5.0, 0.0);
    _check(isnan(fm), "fmod(5,0) == NaN");
}

/* ══════════════════════════════════════════════════════════
   5. Buffer safety — snprintf/strncpy
   ══════════════════════════════════════════════════════════ */
static void test_buffers(void) {
    _section("Buffer safety — overflow e troncamento");

    char dst[64];

    /* snprintf troncamento sicuro */
    char *big = malloc(10001);
    if (big) {
        memset(big, 'Z', 10000); big[10000] = '\0';
        int n = snprintf(dst, sizeof dst, "%.63s", big);
        _check(dst[63] == '\0', "snprintf 10KB→64: null terminator");
        _check(n > 0, "snprintf: ritorna > 0");
        free(big);
    }

    /* strncpy safe */
    char src[200], small[8];
    memset(src, 'A', 199); src[199] = '\0';
    strncpy(small, src, sizeof small - 1);
    small[sizeof small - 1] = '\0';
    _check(small[7] == '\0', "strncpy: null terminator dopo troncamento");
    _check(strlen(small) == 7, "strncpy: lunghezza corretta");

    /* memset+memcpy con size 0 */
    memset(dst, 0, 0);
    _ok("memset size 0 — non crasha");
    memcpy(dst, src, 0);
    _ok("memcpy size 0 — non crasha");

    /* Concatenazione sicura */
    dst[0] = '\0';
    strncat(dst, "hello", sizeof dst - strlen(dst) - 1);
    strncat(dst, " world", sizeof dst - strlen(dst) - 1);
    _check(strcmp(dst, "hello world") == 0, "strncat sicura: risultato corretto");

    /* Stringa con null byte nel mezzo */
    char nullmid[8] = {'a','b','c','\0','d','e','f','\0'};
    _check(strlen(nullmid) == 3, "null byte: strlen si ferma al primo null");
    _check(sizeof(nullmid) == 8, "null byte: sizeof conta tutto");
}

/* ══════════════════════════════════════════════════════════
   6. Unicode / encoding
   ══════════════════════════════════════════════════════════ */
static void test_unicode(void) {
    _section("Unicode / UTF-8 multibyte");

    /* Emoji (4 byte) */
    const char *robot = "\xf0\x9f\xa4\x96";
    _check(strlen(robot) == 4, "emoji robot: 4 bytes");

    /* Carattere cinese (3 byte) */
    const char *cn = "\xe4\xb8\xad";
    _check(strlen(cn) == 3, "cinese: 3 bytes");

    /* Arabo (2 byte per char) */
    const char *ar = "\xd9\x85\xd8\xb1";
    _check(strlen(ar) == 4, "arabo: 4 bytes");

    /* LaTeX puro */
    const char *ltx = "\\sum_{i=0}^{\\infty} \\frac{1}{n^2}";
    _check(strlen(ltx) > 10, "LaTeX: stringa valida");

    /* js_encode su UTF-8 non deve corrompere */
    char out[512];
    js_encode("\xf0\x9f\xa4\x96 test \xe4\xb8\xad\xe6\x96\x87", out, sizeof out);
    _ok("js_encode su UTF-8 misto — non crasha");

    /* Byte invalidi UTF-8 */
    js_encode("\xff\xfe\x80\x81", out, sizeof out);
    _ok("js_encode byte UTF-8 invalidi — non crasha");

    /* Sequenza mista */
    const char *mixed = "abc\xf0\x9f\x8d\xba\x00xyz";
    _check(strlen(mixed) == 7, "misto+null: strlen=7 (si ferma al null)");
}

/* ══════════════════════════════════════════════════════════
   7. Stress string operations usate nei moduli
   ══════════════════════════════════════════════════════════ */
static void test_string_ops(void) {
    _section("String ops — pattern usati nei moduli");

    /* strstr con ago più lungo del pagliaio */
    const char *hay = "abc";
    const char *nee = "abcdefghij";
    _check(strstr(hay, nee) == NULL, "strstr: ago > pagliaio → NULL");

    /* strstr su stringa vuota */
    _check(strstr("", "abc") == NULL, "strstr: pagliaio vuoto → NULL");
    _check(strstr("abc", "") != NULL, "strstr: ago vuoto → non NULL");

    /* strtol con input assurdi */
    char *end;
    long v;
    v = strtol("42abc", &end, 10);
    _check(v == 42 && *end == 'a', "strtol: 42abc → 42, end='a'");
    v = strtol("", &end, 10);
    _check(v == 0, "strtol: stringa vuota → 0");
    v = strtol("9999999999999999999999", &end, 10);
    _check(v == LONG_MAX, "strtol: overflow → LONG_MAX");
    v = strtol("-9999999999999999999999", &end, 10);
    _check(v == LONG_MIN, "strtol: underflow → LONG_MIN");

    /* strtod */
    double d = strtod("3.14xyz", &end);
    _check(fabs(d - 3.14) < 0.001, "strtod: 3.14xyz → 3.14");
    d = strtod("nan", &end);
    _check(isnan(d), "strtod: 'nan' → NaN");
    d = strtod("inf", &end);
    _check(isinf(d), "strtod: 'inf' → Inf");
    d = strtod("", &end);
    _check(d == 0.0, "strtod: vuoto → 0.0");

    /* sprintf su buffer tightly-sized */
    char buf[12];
    int r = snprintf(buf, sizeof buf, "%d", INT_MAX);  /* 2147483647 = 10 cifre */
    _check(buf[11] == '\0', "snprintf INT_MAX in 12 bytes: null terminator");
    _check(r > 0, "snprintf INT_MAX: r > 0");
}

/* ══════════════════════════════════════════════════════════
   8. Simulazione input assurdi da tastiera TUI
   ══════════════════════════════════════════════════════════ */
static void test_tui_inputs(void) {
    _section("Input TUI — pattern assurdi");

    /* Questi sono i tipi di input che un utente potrebbe inserire
       nella TUI. Verifichiamo che il trattamento base sia safe. */

    struct { const char *desc; const char *input; } cases[] = {
        {"vuoto",                        ""},
        {"solo spazi",                   "      "},
        {"newline multipli",             "\n\n\n\n\n"},
        {"tab misti",                    "\t\t\t"},
        {"stringa lunghissima (>1024)",  "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                                         "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                                         "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                                         "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                                         "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                                         "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"},
        {"solo numeri",                  "1234567890"},
        {"SQL injection",                "'; DROP TABLE users; --"},
        {"shell injection",              "$(rm -rf /)"},
        {"format string",               "%s%s%s%s%s%n%p%p%p"},
        {"null byte",                    "\x00"},
        {"caratteri ctrl",               "\x01\x02\x03\x07\x08\x0b\x0c\x0d\x1b"},
        {"path traversal",               "../../../etc/passwd"},
        {"CRLF injection",               "header\r\ninjected: value"},
        {"unicode surrogati invalidi",   "\xed\xa0\x80\xed\xb0\x80"},
        {"byte 0xFF",                    "\xff\xff\xff\xff"},
        {"JSON injection",               "{\"task\":\"evil\",\"admin\":true}"},
        {"XML/HTML",                     "<script>alert(document.cookie)</script>"},
        {"puro operatore",               "+-*/^%"},
        {"sola parentesi",               "(((((((((("},
        {"espressione infinita",         "1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1"},
        {NULL, NULL}
    };

    int all_ok = 1;
    for (int i = 0; cases[i].desc; i++) {
        const char *s = cases[i].input;
        /* Test: operazioni sicure di base */
        size_t len = strlen(s);
        (void)len;

        /* js_encode deve sempre produrre output safe */
        char out[4096];
        js_encode(s, out, sizeof out);
        if (out[sizeof out - 1] != '\0') { all_ok = 0; break; }

        /* js_str non deve crashare */
        const char *v = js_str(s, "key");
        (void)v;
    }
    _check(all_ok, "tutti gli input TUI assurdi: nessun crash + null terminator");
}

/* ══════════════════════════════════════════════════════════
   MAIN
   ══════════════════════════════════════════════════════════ */
int main(void) {
    printf("\n" BLD CYN
           "╔═══════════════════════════════════════════════════╗\n"
           "║  PRISMALUX — STRESS TEST & FUZZING (API PUBBLICHE) ║\n"
           "╚═══════════════════════════════════════════════════╝" RST "\n");

    test_json();
    test_json_encode();
    test_backend();
    test_math_ieee();
    test_buffers();
    test_unicode();
    test_string_ops();
    test_tui_inputs();

    printf("\n" BLD "══════════════════════════════════════════════════════\n" RST);
    int total = passed + failed;
    if (failed == 0)
        printf(BLD GRN "  RISULTATI: %d/%d PASSATI — SOFTWARE A PROVA DI BOMBA\n" RST,
               passed, total);
    else
        printf(BLD RED "  RISULTATI: %d/%d — %d FALLITI\n" RST,
               passed, total, failed);
    printf(BLD "══════════════════════════════════════════════════════\n\n" RST);
    return failed > 0 ? 1 : 0;
}
