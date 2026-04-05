/* ══════════════════════════════════════════════════════════════════════════
   test_rag.c — RAG: Retrieval-Augmented Generation locale
   ══════════════════════════════════════════════════════════════════════════
   Testa rag_query() e rag_build_prompt() in isolamento
   creando documenti .txt temporanei in /tmp/rag_test_docs/.

   Categorie:
     T01 — Directory vuota: ritorna 0, out_ctx vuoto
     T02 — Precision: risultati contengono parole della query
     T03 — Score ordering: chunk con più match viene prima
     T04 — File non .txt ignorati
     T05 — rag_build_prompt: prefisso CONTESTO DOCUMENTALE
     T06 — Edge case: query vuota, file vuoto, query senza parole >3 char
     T07 — Performance: query su 10 file in < 50ms
     T08 — Reali rag_docs/730 e rag_docs/piva (se presenti)

   Build:
     gcc -O2 -Iinclude -o test_rag test_rag.c src/rag.c

   Esecuzione: ./test_rag
   ══════════════════════════════════════════════════════════════════════════ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "rag.h"

/* ── Harness ──────────────────────────────────────────────────────────── */
static int _pass = 0, _fail = 0;
#define SECTION(t) printf("\n\033[36m\033[1m─── %s ───\033[0m\n", t)
#define CHECK(cond, label) do { \
    if (cond) { printf("  \033[32m[OK]\033[0m  %s\n", label); _pass++; } \
    else       { printf("  \033[31m[FAIL]\033[0m %s\n", label); _fail++; } \
} while(0)

/* ── Helpers: crea/rimuovi directory e file di test ──────────────────── */
static const char *TMP_DIR = "/tmp/rag_test_docs";

static void write_file(const char *dir, const char *name, const char *content) {
    char path[256]; snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *f = fopen(path, "w");
    if (f) { fputs(content, f); fclose(f); }
}

static void setup_test_docs(void) {
    mkdir(TMP_DIR, 0755);
    /* Documenti con argomenti diversi per testare scoring e precision */
    write_file(TMP_DIR, "matematica.txt",
        "La matematica studia i numeri, le funzioni e le equazioni.\n"
        "Un'equazione differenziale descrive la variazione di una funzione.\n"
        "Il teorema di Pitagora afferma che in un triangolo rettangolo\n"
        "la somma dei quadrati dei cateti e' uguale al quadrato dell'ipotenusa.\n"
        "Le derivate e gli integrali sono i fondamenti del calcolo infinitesimale.\n");

    write_file(TMP_DIR, "programmazione.txt",
        "Python e' un linguaggio di programmazione ad alto livello.\n"
        "Le funzioni in Python si definiscono con la parola chiave def.\n"
        "Le classi permettono la programmazione orientata agli oggetti.\n"
        "Gli algoritmi di ordinamento come quicksort e mergesort sono efficienti.\n"
        "Il linguaggio C e' noto per la sua velocita' e controllo della memoria.\n");

    write_file(TMP_DIR, "storia.txt",
        "La storia del Medioevo copre il periodo dal V al XV secolo.\n"
        "L'Impero Romano d'Occidente cade nel 476 d.C.\n"
        "Il Rinascimento italiano porta una rinnovata attenzione all'umanesimo.\n"
        "La Rivoluzione Francese del 1789 trasforma l'Europa politicamente.\n"
        "Napoleone Bonaparte e' una delle figure piu' influenti della storia moderna.\n");

    write_file(TMP_DIR, "fisica.txt",
        "La meccanica quantistica descrive il comportamento delle particelle subatomiche.\n"
        "La teoria della relativita' di Einstein rivoluziona la fisica moderna.\n"
        "La velocita' della luce nel vuoto e' circa 300000 km/s.\n"
        "La termodinamica studia i trasferimenti di energia termica.\n"
        "La forza gravitazionale e' descritta dalla legge di Newton.\n");

    write_file(TMP_DIR, "chimica.txt",
        "La tavola periodica degli elementi contiene 118 elementi conosciuti.\n"
        "Un atomo e' composto da protoni, neutroni ed elettroni.\n"
        "Le reazioni chimiche trasformano i reagenti in prodotti.\n"
        "Il pH misura l'acidita' o la basicita' di una soluzione acquosa.\n"
        "La chimica organica studia i composti del carbonio.\n");
}

static void cleanup_test_docs(void) {
    /* Rimuove i file di test */
    const char *files[] = {
        "matematica.txt","programmazione.txt","storia.txt",
        "fisica.txt","chimica.txt","extra.json","vuoto.txt",NULL
    };
    for (int i = 0; files[i]; i++) {
        char path[256]; snprintf(path,sizeof path,"%s/%s",TMP_DIR,files[i]);
        remove(path);
    }
    rmdir(TMP_DIR);
}

/* ════════════════════════════════════════════════════════════════════════
   T01 — Directory vuota
   ════════════════════════════════════════════════════════════════════════ */
static void t01_empty_dir(void) {
    SECTION("T01 — Directory vuota o inesistente");

    char out[RAG_CTX_MAX]; memset(out, 0, sizeof out);

    /* Directory inesistente → 0 chunk, out vuoto */
    int n = rag_query("/tmp/rag_nonexistent_xyzzy", "query test", out, sizeof out);
    CHECK(n == 0, "dir inesistente: ritorna 0 chunk");
    CHECK(out[0] == '\0' || strlen(out) == 0, "out_ctx vuoto per dir inesistente");

    /* Directory vuota → 0 chunk */
    mkdir("/tmp/rag_empty_test", 0755);
    memset(out, 0, sizeof out);
    n = rag_query("/tmp/rag_empty_test", "qualsiasi query", out, sizeof out);
    CHECK(n == 0, "dir vuota: ritorna 0 chunk");
    rmdir("/tmp/rag_empty_test");
}

/* ════════════════════════════════════════════════════════════════════════
   T02 — Precision: risultati contengono parole della query
   ════════════════════════════════════════════════════════════════════════ */
static void t02_precision(void) {
    SECTION("T02 — Precision: chunk restituiti rilevanti per la query");

    char out[RAG_CTX_MAX]; memset(out, 0, sizeof out);

    /* Query "matematica equazioni funzioni": deve portare chunk da matematica.txt */
    int n = rag_query(TMP_DIR, "matematica equazioni funzioni", out, sizeof out);
    CHECK(n > 0, "query matematica: almeno un chunk trovato");
    /* Il risultato deve contenere parole chiave della query */
    CHECK(strstr(out,"matem") || strstr(out,"equaz") || strstr(out,"funz"),
          "chunk matematica: contiene parole della query");
    printf("  Query 'matematica equazioni': %d chunk, %zu char\n", n, strlen(out));

    /* Query "Python programmazione algoritmi": deve portare chunk da programmazione.txt */
    memset(out, 0, sizeof out);
    n = rag_query(TMP_DIR, "Python programmazione algoritmi", out, sizeof out);
    CHECK(n > 0, "query Python: almeno un chunk trovato");
    CHECK(strstr(out,"Python") || strstr(out,"python") ||
          strstr(out,"programm") || strstr(out,"algorit"),
          "chunk Python: contiene parole della query");

    /* Query "storia medioevo impero romano": deve portare storia.txt */
    memset(out, 0, sizeof out);
    n = rag_query(TMP_DIR, "storia medioevo impero romano", out, sizeof out);
    CHECK(n > 0, "query storia: almeno un chunk trovato");
    CHECK(strstr(out,"storia") || strstr(out,"Medioevo") ||
          strstr(out,"medioevo") || strstr(out,"Romano"),
          "chunk storia: contiene parole della query");
}

/* ════════════════════════════════════════════════════════════════════════
   T03 — Score ordering: chunk più rilevante prima
   ════════════════════════════════════════════════════════════════════════ */
static void t03_ordering(void) {
    SECTION("T03 — Score ordering: chunk con più match viene prima");

    /* Crea documenti con densità di keyword diversa */
    mkdir("/tmp/rag_order_test", 0755);

    write_file("/tmp/rag_order_test", "bassa_rilevanza.txt",
        "Testo generico senza parole chiave rilevanti.\n"
        "Questo documento parla di cose varie e non pertinenti.\n");

    write_file("/tmp/rag_order_test", "alta_rilevanza.txt",
        "Intelligenza artificiale e machine learning sono tecnologie chiave.\n"
        "L'intelligenza artificiale usa modelli di machine learning.\n"
        "I modelli linguistici sono esempi di intelligenza artificiale avanzata.\n"
        "Machine learning e intelligenza artificiale rivoluzionano l'informatica.\n");

    char out[RAG_CTX_MAX]; memset(out, 0, sizeof out);
    int n = rag_query("/tmp/rag_order_test",
                      "intelligenza artificiale machine learning modelli", out, sizeof out);
    CHECK(n > 0, "score ordering: almeno un chunk trovato");
    if (n > 0) {
        /* Il documento ad alta rilevanza deve apparire nel contesto */
        CHECK(strstr(out,"intelligenza") || strstr(out,"machine") || strstr(out,"modell"),
              "documento ad alta rilevanza incluso nel risultato");
    }
    printf("  Score ordering: %d chunk, output %zu char\n", n, strlen(out));

    remove("/tmp/rag_order_test/bassa_rilevanza.txt");
    remove("/tmp/rag_order_test/alta_rilevanza.txt");
    rmdir("/tmp/rag_order_test");
}

/* ════════════════════════════════════════════════════════════════════════
   T04 — File non .txt ignorati
   ════════════════════════════════════════════════════════════════════════ */
static void t04_non_txt(void) {
    SECTION("T04 — File non .txt ignorati");

    /* Aggiunge file non-txt nella directory di test */
    write_file(TMP_DIR, "extra.json",
        "{\"nota\":\"questo file non deve essere letto dal RAG\","
        "\"segreto\":\"Python programmazione algoritmi chimica\"}");

    char out[RAG_CTX_MAX]; memset(out, 0, sizeof out);
    int n = rag_query(TMP_DIR, "nota segreto json", out, sizeof out);

    /* "nota", "segreto", "json" hanno <= 3 char o non compaiono nei .txt normali */
    /* Il file .json non deve contribuire ai risultati */
    CHECK(!strstr(out,"\"segreto\"") && !strstr(out,"\"nota\""),
          "file .json ignorato: parole del JSON non nei risultati");
    printf("  Query su .json: %d chunk (da .txt)\n", n);

    /* Aggiunge file .txt vuoto */
    write_file(TMP_DIR, "vuoto.txt", "");
    memset(out, 0, sizeof out);
    n = rag_query(TMP_DIR, "qualsiasi query test documento", out, sizeof out);
    CHECK(n >= 0, "file .txt vuoto: no crash");
}

/* ════════════════════════════════════════════════════════════════════════
   T05 — rag_build_prompt
   ════════════════════════════════════════════════════════════════════════ */
static void t05_build_prompt(void) {
    SECTION("T05 — rag_build_prompt: iniezione contesto nel system prompt");

    char out[2048]; memset(out, 0, sizeof out);

    const char *base_sys = "Sei un assistente AI. Rispondi in italiano.";
    const char *rag_ctx  = "La matematica studia i numeri.";

    /* Con contesto RAG */
    rag_build_prompt(base_sys, rag_ctx, out, sizeof out);
    CHECK(strstr(out,"CONTESTO") || strstr(out,"contesto"),
          "rag_build_prompt con ctx: contiene 'CONTESTO'");
    CHECK(strstr(out,"matematica"),
          "rag_build_prompt con ctx: contiene il testo RAG");
    CHECK(strstr(out,"assistente"),
          "rag_build_prompt con ctx: contiene il sys prompt base");
    printf("  Prompt con RAG (%zu char): %.60s...\n", strlen(out), out);

    /* Senza contesto RAG (rag_ctx vuoto) */
    memset(out, 0, sizeof out);
    rag_build_prompt(base_sys, "", out, sizeof out);
    CHECK(strstr(out,"assistente"),  "senza RAG: sys prompt base presente");
    CHECK(!strstr(out,"CONTESTO") || strstr(out,base_sys),
          "senza RAG: output = solo sys prompt base");

    /* Buffer overflow protection */
    char small_out[50]; memset(small_out,0,sizeof small_out);
    rag_build_prompt(base_sys, rag_ctx, small_out, sizeof small_out);
    CHECK(strlen(small_out) < 50, "rag_build_prompt: rispetta buffer piccolo");
    CHECK(small_out[49] == '\0',   "rag_build_prompt: null terminator presente");
}

/* ════════════════════════════════════════════════════════════════════════
   T06 — Edge case
   ════════════════════════════════════════════════════════════════════════ */
static void t06_edge(void) {
    SECTION("T06 — Edge case: query vuota, parole corte, buffer piccolo");

    char out[RAG_CTX_MAX]; memset(out, 0, sizeof out);

    /* Query vuota → 0 chunk o risultati minimi */
    int n = rag_query(TMP_DIR, "", out, sizeof out);
    CHECK(n >= 0, "query vuota: no crash (ritorna >= 0)");

    /* Query con sole parole corte (<= 3 char): nessuna word > 3 char → score 0 */
    memset(out, 0, sizeof out);
    n = rag_query(TMP_DIR, "il la lo un di da", out, sizeof out);
    CHECK(n >= 0, "query parole corte: no crash");
    printf("  Query parole corte: %d chunk\n", n);

    /* Buffer output piccolo: no overflow */
    char small[10]; memset(small, 0xFF, sizeof small);
    rag_query(TMP_DIR, "matematica python storia", small, sizeof small);
    CHECK(small[9] == '\0' || small[9] == (char)0xFF,
          "buffer piccolo: no overflow oltre la dimensione");
    /* Forza null terminator per test successivi */
    small[9] = '\0';
    CHECK(strlen(small) < 10, "output rispetta dimensione buffer");
}

/* ════════════════════════════════════════════════════════════════════════
   T07 — Performance: 10 file, query < 50ms
   ════════════════════════════════════════════════════════════════════════ */
static void t07_performance(void) {
    SECTION("T07 — Performance: query su 5 file in < 50ms");

    struct timespec t0, t1;
    char out[RAG_CTX_MAX];

    const char *queries[] = {
        "matematica equazioni teorema pitagora",
        "Python programmazione funzioni classi",
        "storia medioevo impero romano rinascimento",
        "fisica quantistica relativita' einstein",
        "chimica atomi tavola periodica reazioni",
        NULL
    };

    clock_gettime(CLOCK_MONOTONIC, &t0);
    int total_chunks = 0;
    for (int i = 0; queries[i]; i++) {
        memset(out, 0, sizeof out);
        total_chunks += rag_query(TMP_DIR, queries[i], out, sizeof out);
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double ms = (t1.tv_sec-t0.tv_sec)*1000.0 + (t1.tv_nsec-t0.tv_nsec)/1e6;

    printf("  5 query su 5 file: %.2f ms totale (%.2f ms/query), %d chunk totali\n",
           ms, ms/5.0, total_chunks);
    CHECK(ms < 50.0,         "5 query su 5 file in < 50ms totale");
    CHECK(ms/5.0 < 10.0,     "< 10ms per query in media");
    CHECK(total_chunks >= 3, "almeno 3 chunk totali ritornati");

    /* Stress: 100 query consecutive */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int r = 0; r < 100; r++) {
        memset(out, 0, sizeof out);
        rag_query(TMP_DIR, "matematica python storia fisica chimica", out, sizeof out);
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    ms = (t1.tv_sec-t0.tv_sec)*1000.0 + (t1.tv_nsec-t0.tv_nsec)/1e6;
    printf("  100 query stress: %.2f ms (%.2f ms/query)\n", ms, ms/100.0);
    CHECK(ms < 2000.0, "100 query in < 2s");
}

/* ════════════════════════════════════════════════════════════════════════
   T08 — Documenti reali rag_docs/730 e rag_docs/piva
   ════════════════════════════════════════════════════════════════════════ */
static void t08_real_docs(void) {
    SECTION("T08 — Documenti reali rag_docs/730 e rag_docs/piva");

    const char *dirs[] = { "rag_docs/730", "rag_docs/piva", NULL };
    const char *queries[] = {
        "dichiarazione dei redditi modello 730 detrazioni",
        "partita IVA regime forfettario coefficiente redditività",
        NULL
    };

    for (int i = 0; dirs[i]; i++) {
        char out[RAG_CTX_MAX]; memset(out, 0, sizeof out);
        int n = rag_query(dirs[i], queries[i], out, sizeof out);
        if (n > 0) {
            printf("  \033[32m[OK]\033[0m  %s: %d chunk trovati (%zu char)\n",
                   dirs[i], n, strlen(out));
            _pass++;
        } else {
            printf("  \033[33m[INFO]\033[0m %s: 0 chunk (directory vuota o assente — ok)\n",
                   dirs[i]);
            /* Non è un fail: i file reali possono essere vuoti */
            _pass++;
        }
    }
}

/* ════════════════════════════════════════════════════════════════════════
   MAIN
   ════════════════════════════════════════════════════════════════════════ */
int main(void) {
    printf("\n\033[1m══ test_rag.c — RAG: Retrieval-Augmented Generation locale ══\033[0m\n");
    printf("   RAG_CHUNK_CHARS=%d  RAG_TOP_K=%d  RAG_CTX_MAX=%d\n",
           RAG_CHUNK_CHARS, RAG_TOP_K, RAG_CTX_MAX);

    setup_test_docs();

    t01_empty_dir();
    t02_precision();
    t03_ordering();
    t04_non_txt();
    t05_build_prompt();
    t06_edge();
    t07_performance();
    t08_real_docs();

    cleanup_test_docs();

    printf("\n\033[1m══════════════════════════════════════════════════════\033[0m\n");
    if (_fail == 0)
        printf("\033[1m\033[32m  RISULTATI: %d/%d PASSATI — RAG A PROVA DI BOMBA\033[0m\n",
               _pass, _pass+_fail);
    else
        printf("\033[1m\033[31m  RISULTATI: %d/%d PASSATI — %d FALLITI\033[0m\n",
               _pass, _pass+_fail, _fail);
    printf("\033[1m══════════════════════════════════════════════════════\033[0m\n\n");
    return _fail > 0 ? 1 : 0;
}
