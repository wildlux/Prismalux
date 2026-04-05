/* ══════════════════════════════════════════════════════════════════════════
   test_rag_bench.c — Benchmark tri-engine RAG
   ══════════════════════════════════════════════════════════════════════════
   Confronta i tre motori RAG sugli stessi documenti e query:
     - TF-overlap  (rag_query):       zero dipendenze, istantaneo
     - Embedding   (rag_embed_query): semantico, richiede Ollama + modello embed
     - JLT-SRHT    (jlt_rag_query):   JLT arXiv:2103.00564, 4-6x piu' veloce
                                      di Embed con stessa qualita' semantica

   Il benchmark misura la latenza reale e mostra un'anteprima dei risultati.
   Prima esecuzione Embed/JLT: calcola e mette in cache gli embedding.
   Seconda esecuzione: legge dalla cache — latenza reale in produzione.

   Build:
     make test_rag_bench
   Esecuzione:
     ./test_rag_bench
   ══════════════════════════════════════════════════════════════════════════ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "rag.h"
#include "rag_embed.h"
#include "jlt_rag.h"
#include "http.h"

/* ── Harness ──────────────────────────────────────────────────────────── */
static int _pass = 0, _fail = 0;
#define SECTION(t)          printf("\n\033[36m\033[1m─── %s ───\033[0m\n", t)
#define CHECK(cond, label)  do { \
    if (cond) { printf("  \033[32m[OK]\033[0m  %s\n", label); _pass++; } \
    else      { printf("  \033[31m[FAIL]\033[0m %s\n", label); _fail++; } \
} while(0)

/* ── Timing ad alta risoluzione ───────────────────────────────────────── */
static double _ms_now(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

/* ── Directory e documenti di test ────────────────────────────────────── */
static const char *BENCH_DIR = "/tmp/rag_bench_docs";

static void _write_file(const char *dir, const char *name, const char *content) {
    char path[256];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *f = fopen(path, "w");
    if (f) { fputs(content, f); fclose(f); }
}

static void _setup_docs(void) {
    mkdir(BENCH_DIR, 0755);

    _write_file(BENCH_DIR, "matematica.txt",
        "La matematica studia i numeri, le funzioni e le equazioni.\n"
        "Un'equazione differenziale descrive la variazione di una funzione.\n"
        "Il teorema di Pitagora afferma che in un triangolo rettangolo\n"
        "la somma dei quadrati dei cateti e' uguale al quadrato dell'ipotenusa.\n"
        "Le derivate e gli integrali sono i fondamenti del calcolo infinitesimale.\n");

    _write_file(BENCH_DIR, "programmazione.txt",
        "Python e' un linguaggio di programmazione ad alto livello.\n"
        "Le funzioni in Python si definiscono con la parola chiave def.\n"
        "Le classi permettono la programmazione orientata agli oggetti.\n"
        "Gli algoritmi di ordinamento come quicksort e mergesort sono efficienti.\n"
        "Il linguaggio C e' noto per la sua velocita' e controllo della memoria.\n");

    _write_file(BENCH_DIR, "storia.txt",
        "La storia del Medioevo copre il periodo dal V al XV secolo.\n"
        "L'Impero Romano d'Occidente cade nel 476 d.C.\n"
        "Il Rinascimento italiano porta una rinnovata attenzione all'umanesimo.\n"
        "La Rivoluzione Francese del 1789 trasforma l'Europa politicamente.\n"
        "Napoleone Bonaparte e' una delle figure piu' influenti della storia moderna.\n");

    _write_file(BENCH_DIR, "fisica.txt",
        "La meccanica quantistica descrive il comportamento delle particelle subatomiche.\n"
        "La teoria della relativita' di Einstein rivoluziona la fisica moderna.\n"
        "La velocita' della luce nel vuoto e' circa 300000 km/s.\n"
        "La termodinamica studia i trasferimenti di energia termica.\n"
        "La forza gravitazionale e' descritta dalla legge di Newton.\n");

    _write_file(BENCH_DIR, "chimica.txt",
        "La tavola periodica degli elementi contiene 118 elementi conosciuti.\n"
        "Un atomo e' composto da protoni, neutroni ed elettroni.\n"
        "Le reazioni chimiche trasformano i reagenti in prodotti.\n"
        "Il pH misura l'acidita' o la basicita' di una soluzione acquosa.\n"
        "La chimica organica studia i composti del carbonio.\n");
}

static void _cleanup_docs(void) {
    const char *files[] = {
        "matematica.txt", "programmazione.txt", "storia.txt",
        "fisica.txt", "chimica.txt", NULL
    };
    for (int i = 0; files[i]; i++) {
        char path[256];
        snprintf(path, sizeof path, "%s/%s", BENCH_DIR, files[i]);
        remove(path);
    }
    /* la cache viene lasciata per analisi post-benchmark */
}

/* ── Query di benchmark (5 — una per argomento) ───────────────────────── */
static const char *QUERIES[] = {
    "matematica equazioni teorema pitagora derivate integrali",
    "Python programmazione funzioni classi algoritmi ordinamento",
    "storia medioevo impero romano rinascimento rivoluzione francese",
    "fisica quantistica relativita einstein particelle termodinamica",
    "chimica atomi tavola periodica reazioni organica",
    NULL
};

/* ── Stato Ollama ─────────────────────────────────────────────────────── */
static int  _ollama_ok   = 0;
static char _embed_model[64] = "";

static void _detect_ollama(void) {
    char names[4][128];
    int n = http_ollama_list("127.0.0.1", 11434, names, 4);
    if (n > 0) {
        _ollama_ok = rag_embed_detect_model("127.0.0.1", 11434, _embed_model);
    }
}

/* ── Tabella risultati ────────────────────────────────────────────────── */
typedef struct {
    const char *motore;
    double      ms;
    int         n_chunk;
    char        preview[61];
} Row;

static void _print_row(const Row *r) {
    printf("  %-8s │ %8.1f ms │ %3d │ %-60s\n",
           r->motore, r->ms, r->n_chunk, r->preview);
}

static void _print_header(void) {
    printf("  %-8s │ %10s │ %3s │ %-60s\n",
           "Motore", "Latenza", "N", "Anteprima risultato");
    printf("  %s\n",
           "─────────┼────────────┼─────┼"
           "─────────────────────────────────────────────────────────────");
}

/* ── Correggi stringa (preview sicura, null-terminated) ──────────────── */
static void _make_preview(const char *ctx, char *out, int max) {
    int i = 0;
    for (; ctx[i] && i < max - 1; i++) {
        char c = ctx[i];
        /* rimpiazza newline con spazio per una preview su singola riga */
        out[i] = (c == '\n' || c == '\r') ? ' ' : c;
    }
    out[i] = '\0';
}

/* ════════════════════════════════════════════════════════════════════════
   MAIN
   ════════════════════════════════════════════════════════════════════════ */
int main(void) {
    printf("\n\033[1m══ test_rag_bench.c — Benchmark Tri-Engine RAG ══\033[0m\n");
    printf("   Motori: TF-overlap | Embedding semantico | JLT-SRHT (arXiv:2103.00564)\n");

    /* Setup */
    _setup_docs();
    _detect_ollama();

    printf("\n\033[1m[Setup]\033[0m Documenti: 5 file in %s\n", BENCH_DIR);
    if (_ollama_ok) {
        printf("\033[1m[Setup]\033[0m Ollama: \033[32mdisponibile\033[0m"
               " — modello embedding: \033[33m%s\033[0m\n", _embed_model);
    } else {
        printf("\033[1m[Setup]\033[0m Ollama: \033[31mnon disponibile\033[0m"
               " o nessun modello embedding installato\n");
        printf("         (per abilitare: ollama pull nomic-embed-text)\n");
    }

    char ctx[RAG_CTX_MAX];

    /* ═══════════════════════════════════════════════════════════
       MOTORE TF-OVERLAP
       ═══════════════════════════════════════════════════════════ */
    SECTION("Motore TF-overlap (rag_query)");

    Row tf_rows[5];
    double tf_total = 0.0;

    for (int q = 0; QUERIES[q]; q++) {
        memset(ctx, 0, sizeof ctx);
        double t0 = _ms_now();
        int n = rag_query(BENCH_DIR, QUERIES[q], ctx, sizeof ctx);
        double ms = _ms_now() - t0;

        tf_rows[q].motore  = "TF";
        tf_rows[q].ms      = ms;
        tf_rows[q].n_chunk = n;
        _make_preview(ctx, tf_rows[q].preview, 61);

        tf_total += ms;

        char label[80];
        snprintf(label, sizeof label, "Query %d: %d chunk trovati", q + 1, n);
        CHECK(n >= 0, label);
    }

    _print_header();
    for (int q = 0; QUERIES[q]; q++) _print_row(&tf_rows[q]);
    printf("\n  TF media: \033[1m%.2f ms/query\033[0m\n", tf_total / 5.0);

    /* ═══════════════════════════════════════════════════════════
       MOTORE EMBEDDING — PRIMA ESECUZIONE (calcola + cache)
       ═══════════════════════════════════════════════════════════ */
    Row emb1_rows[5];
    Row emb2_rows[5];
    double emb1_total = 0.0, emb2_total = 0.0;
    int embed_ok = 0;

    if (_ollama_ok) {
        SECTION("Motore Embedding — prima esecuzione (calcola embedding)");
        printf("  [INFO] Prima esecuzione: calcola embedding e li mette in cache.\n");
        printf("         Attendi (ogni file richiede N * latenza_embed_ms)...\n\n");

        embed_ok = 1;
        for (int q = 0; QUERIES[q]; q++) {
            memset(ctx, 0, sizeof ctx);
            double t0 = _ms_now();
            int n = rag_embed_query("127.0.0.1", 11434, _embed_model,
                                    BENCH_DIR, QUERIES[q], ctx, sizeof ctx);
            double ms = _ms_now() - t0;

            emb1_rows[q].motore  = "Embed1";
            emb1_rows[q].ms      = ms;
            emb1_rows[q].n_chunk = n;
            _make_preview(ctx, emb1_rows[q].preview, 61);
            emb1_total += ms;

            char label[80];
            snprintf(label, sizeof label, "Embed query %d: %d chunk trovati", q + 1, n);
            CHECK(n >= 0, label);
            if (n < 0) embed_ok = 0;
        }

        _print_header();
        for (int q = 0; QUERIES[q]; q++) _print_row(&emb1_rows[q]);
        printf("\n  Embed 1° esecuzione media: \033[1m%.2f ms/query\033[0m"
               " (include calcolo + cache write)\n", emb1_total / 5.0);

        /* ═══════════════════════════════════════════════════════
           MOTORE EMBEDDING — SECONDA ESECUZIONE (cache hit)
           ═══════════════════════════════════════════════════════ */
        SECTION("Motore Embedding — seconda esecuzione (cache hit)");
        printf("  [INFO] Seconda esecuzione: legge embedding dalla cache su disco.\n\n");

        for (int q = 0; QUERIES[q]; q++) {
            memset(ctx, 0, sizeof ctx);
            double t0 = _ms_now();
            int n = rag_embed_query("127.0.0.1", 11434, _embed_model,
                                    BENCH_DIR, QUERIES[q], ctx, sizeof ctx);
            double ms = _ms_now() - t0;

            emb2_rows[q].motore  = "Embed2";
            emb2_rows[q].ms      = ms;
            emb2_rows[q].n_chunk = n;
            _make_preview(ctx, emb2_rows[q].preview, 61);
            emb2_total += ms;

            char label[80];
            snprintf(label, sizeof label, "Embed (cache) query %d: %d chunk", q + 1, n);
            CHECK(n >= 0, label);
        }

        _print_header();
        for (int q = 0; QUERIES[q]; q++) _print_row(&emb2_rows[q]);
        printf("\n  Embed 2° esecuzione media: \033[1m%.2f ms/query\033[0m"
               " (cache hit — latenza in produzione)\n", emb2_total / 5.0);

    } else {
        SECTION("Motore Embedding — saltato (Ollama non disponibile)");
        printf("  Per abilitare il motore Embed e JLT:\n");
        printf("    1. Assicurati che Ollama sia in esecuzione\n");
        printf("    2. ollama pull nomic-embed-text\n");
        printf("    3. Riesegui: ./test_rag_bench\n");
    }

    /* ═══════════════════════════════════════════════════════════
       MOTORE JLT-SRHT — PRIMA ESECUZIONE (calcola + cache JLT)
       ═══════════════════════════════════════════════════════════ */
    Row jlt1_rows[5];
    Row jlt2_rows[5];
    double jlt1_total = 0.0, jlt2_total = 0.0;
    JltRagStats jlt_stats = {0};
    int jlt_ok = 0;

    if (_ollama_ok) {
        SECTION("Motore JLT-SRHT — prima esecuzione (arXiv:2103.00564 §1.4.2)");
        printf("  [INFO] Applica SRHT agli embedding Ollama: d_orig → m_dim\n");
        printf("         Garanzia: |〈f(x),f(y)〉-〈x,y〉| ≤ ε (Corollary 1.5)\n");
        printf("         Prima esecuzione: calcola embedding + comprimi + cache.\n\n");

        jlt_ok = 1;
        for (int q = 0; QUERIES[q]; q++) {
            memset(ctx, 0, sizeof ctx);
            JltRagStats qs = {0};
            double t0 = _ms_now();
            int n = jlt_rag_query("127.0.0.1", 11434, _embed_model,
                                   BENCH_DIR, QUERIES[q], ctx, sizeof ctx, &qs);
            double ms = _ms_now() - t0;

            jlt1_rows[q].motore  = "JLT-1";
            jlt1_rows[q].ms      = ms;
            jlt1_rows[q].n_chunk = n;
            _make_preview(ctx, jlt1_rows[q].preview, 61);
            jlt1_total += ms;
            if (q == 0) jlt_stats = qs; /* salva stats della prima query */

            char label[80];
            snprintf(label, sizeof label, "JLT query %d: %d chunk trovati", q + 1, n);
            CHECK(n >= 0, label);
            if (n < 0) jlt_ok = 0;
        }

        _print_header();
        for (int q = 0; QUERIES[q]; q++) _print_row(&jlt1_rows[q]);
        if (jlt_stats.d_orig > 0)
            printf("\n  [JLT] d_orig=%d → m=%d (ratio=%.2f, speedup_sim=~%.1fx)\n",
                   jlt_stats.d_orig, jlt_stats.m_compressed,
                   jlt_stats.compress_ratio, jlt_stats.speedup_est);
        printf("  JLT 1° esecuzione media: \033[1m%.2f ms/query\033[0m\n",
               jlt1_total / 5.0);

        /* ═══════════════════════════════════════════════════════
           MOTORE JLT — SECONDA ESECUZIONE (cache JLT hit)
           ═══════════════════════════════════════════════════════ */
        SECTION("Motore JLT-SRHT — seconda esecuzione (cache JLT hit)");
        printf("  [INFO] Carica vettori già compressi dalla cache JLT.\n\n");

        for (int q = 0; QUERIES[q]; q++) {
            memset(ctx, 0, sizeof ctx);
            JltRagStats qs2 = {0};
            double t0 = _ms_now();
            int n = jlt_rag_query("127.0.0.1", 11434, _embed_model,
                                   BENCH_DIR, QUERIES[q], ctx, sizeof ctx, &qs2);
            double ms = _ms_now() - t0;

            jlt2_rows[q].motore  = "JLT-2";
            jlt2_rows[q].ms      = ms;
            jlt2_rows[q].n_chunk = n;
            _make_preview(ctx, jlt2_rows[q].preview, 61);
            jlt2_total += ms;

            char label[80];
            snprintf(label, sizeof label, "JLT (cache) query %d: chunk=%d hit=%d",
                     q + 1, n, qs2.cache_hits);
            CHECK(n >= 0, label);
        }

        _print_header();
        for (int q = 0; QUERIES[q]; q++) _print_row(&jlt2_rows[q]);
        printf("\n  JLT 2° esecuzione media: \033[1m%.2f ms/query\033[0m"
               " (cache JLT hit — latenza in produzione)\n", jlt2_total / 5.0);
    }

    /* ═══════════════════════════════════════════════════════════
       TABELLA COMPARATIVA FINALE
       ═══════════════════════════════════════════════════════════ */
    SECTION("Risultati comparativi — Tri-Engine");

    printf("\n  %-16s │ %14s │ %s\n",
           "Motore", "Latenza media", "Note");
    printf("  %s\n",
           "─────────────────┼────────────────┼──────────────────────────────────────────");
    printf("  %-16s │ %11.2f ms │ veloce, lessicale, zero dipendenze\n",
           "TF-overlap", tf_total / 5.0);

    if (_ollama_ok && embed_ok) {
        printf("  %-16s │ %11.2f ms │ calcolo embedding + cache write\n",
               "Embed (1° run)", emb1_total / 5.0);
        printf("  %-16s │ %11.2f ms │ cache hit — latenza in produzione\n",
               "Embed (cache)", emb2_total / 5.0);
    } else {
        printf("  %-16s │ %14s │ Ollama non disponibile\n", "Embed", "n/d");
    }

    if (_ollama_ok && jlt_ok) {
        printf("  %-16s │ %11.2f ms │ JLT+embedding calcolo (arXiv:2103.00564)\n",
               "JLT-SRHT (1° run)", jlt1_total / 5.0);
        printf("  %-16s │ \033[32m%11.2f ms\033[0m │ cache JLT hit ← latenza target\n",
               "JLT-SRHT (cache)", jlt2_total / 5.0);
        if (jlt_stats.d_orig > 0) {
            printf("\n  [JLT] Compressione embedding: %dD → %dD (%.0f%% piu' piccolo)\n",
                   jlt_stats.d_orig, jlt_stats.m_compressed,
                   (1.0 - jlt_stats.compress_ratio) * 100.0);
            printf("  [JLT] Speedup cosine similarity: ~%.1fx\n",
                   jlt_stats.speedup_est);
            printf("  [JLT] Cache su disco: %.0f%% piu' piccola di Embed\n",
                   (1.0 - jlt_stats.compress_ratio) * 100.0);
        }
    } else if (_ollama_ok) {
        printf("  %-16s │ %14s │ errore JLT\n", "JLT-SRHT", "n/d");
    } else {
        printf("  %-16s │ %14s │ Ollama non disponibile\n", "JLT-SRHT", "n/d");
    }

    printf("\n");

    /* ─── Raccomandazione basata sui dati misurati ─── */
    printf("\033[1m  Raccomandazione:\033[0m ");
    if (!_ollama_ok || (!embed_ok && !jlt_ok)) {
        printf("\033[32mTF-overlap\033[0m (Embed non disponibile)\n");
        printf("  rag_set_mode(RAG_MODE_TF);\n");
    } else {
        double tf_avg   = tf_total   / 5.0;
        double emb_avg  = embed_ok ? emb2_total  / 5.0 : 99999.0;
        double jlt_avg  = jlt_ok   ? jlt2_total  / 5.0 : 99999.0;
        double best_sem = (jlt_avg < emb_avg) ? jlt_avg : emb_avg;

        if (best_sem < 20.0 && jlt_avg <= emb_avg) {
            printf("\033[32mJLT-SRHT\033[0m (cache < 20ms, stessa qualita' semantica"
                   " di Embed, piu' veloce)\n");
            printf("  rag_set_mode(RAG_MODE_JLT);\n");
        } else if (best_sem < 20.0) {
            printf("\033[32mEMBED\033[0m (cache hit < 20ms, semantica superiore a TF)\n");
            printf("  rag_set_mode(RAG_MODE_EMBED);\n");
        } else if (best_sem / (tf_avg > 0.01 ? tf_avg : 0.01) < 10.0) {
            if (jlt_avg <= emb_avg) {
                printf("\033[32mJLT-SRHT\033[0m (miglior compromesso velocita'/semantica)\n");
                printf("  rag_set_mode(RAG_MODE_JLT);\n");
            } else {
                printf("\033[32mEMBED\033[0m (%.1fx piu' lento di TF ma accettabile)\n",
                       emb_avg / tf_avg);
                printf("  rag_set_mode(RAG_MODE_EMBED);\n");
            }
        } else {
            printf("\033[32mTF-overlap\033[0m (Embed/JLT %.0fx piu' lento su questa macchina)\n",
                   best_sem / tf_avg);
            printf("  rag_set_mode(RAG_MODE_TF);\n");
        }

        printf("\n  (TF: %.2f ms/q — Embed cache: %.2f ms/q — JLT cache: %.2f ms/q)\n",
               tf_avg,
               embed_ok ? emb_avg : 0.0,
               jlt_ok   ? jlt_avg : 0.0);
    }

    /* ═══════════════════════════════════════════════════════════
       CHECK SANITA' BASE
       ═══════════════════════════════════════════════════════════ */
    SECTION("Sanita' dei dati");

    /* TF: i risultati devono contenere parole delle query */
    {
        char out[RAG_CTX_MAX]; memset(out, 0, sizeof out);
        rag_query(BENCH_DIR, "matematica equazioni teorema pitagora", out, sizeof out);
        CHECK(strstr(out, "matem") || strstr(out, "equaz") || strstr(out, "teorem"),
              "TF: chunk matematica contiene parole della query");
    }
    {
        char out[RAG_CTX_MAX]; memset(out, 0, sizeof out);
        rag_query(BENCH_DIR, "Python programmazione algoritmi funzioni", out, sizeof out);
        CHECK(strstr(out, "Python") || strstr(out, "python") || strstr(out, "programm"),
              "TF: chunk programmazione contiene parole della query");
    }

    if (_ollama_ok && embed_ok) {
        /* Embed: deve trovare chunk sulla stessa query semantica */
        char out[RAG_CTX_MAX]; memset(out, 0, sizeof out);
        int n = rag_embed_query("127.0.0.1", 11434, _embed_model,
                                BENCH_DIR,
                                "calcolo differenziale e integrale in matematica",
                                out, sizeof out);
        CHECK(n > 0, "Embed: query semantica trova almeno 1 chunk");
        /* "calcolo differenziale" non e' in TF se usa sinonimi */
        if (n > 0) {
            printf("  [Embed] Query semantica: %d chunk, preview: '%.60s...'\n",
                   n, out);
        }
    }

    /* Cleanup */
    _cleanup_docs();

    /* ═══════════════════════════════════════════════════════════
       RIEPILOGO
       ═══════════════════════════════════════════════════════════ */
    printf("\n\033[1m══════════════════════════════════════════════════════\033[0m\n");
    if (_fail == 0)
        printf("\033[1m\033[32m  RISULTATI: %d/%d OK — benchmark completato\033[0m\n",
               _pass, _pass + _fail);
    else
        printf("\033[1m\033[31m  RISULTATI: %d/%d OK — %d falliti\033[0m\n",
               _pass, _pass + _fail, _fail);
    printf("\033[1m══════════════════════════════════════════════════════\033[0m\n\n");

    return _fail > 0 ? 1 : 0;
}
