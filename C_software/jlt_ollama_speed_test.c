/*
 * jlt_ollama_speed_test.c — Test di velocità reale: Ollama standard vs JLT
 * =========================================================================
 * Confronta con chiamate Ollama REALI:
 *
 *   Motore A — rag_embed_query   : embedding 768D completo + cache disco
 *   Motore B — jlt_rag_query     : JLT SRHT 109D + cache disco compressa
 *   Motore C — jlt_index_query   : JLT in-RAM (zero disco dopo build)
 *
 * Mostra:
 *   - ms per query (media su N run)
 *   - % di velocizzazione rispetto al baseline
 *   - prima esecuzione (embed Ollama live) vs seconda (cache hit)
 *   - RAM stimata: originale vs compressa
 *
 * Build:
 *   gcc -O2 -Wall -Iinclude -o jlt_ollama_speed_test \
 *       jlt_ollama_speed_test.c \
 *       src/rag_embed.c src/jlt.c src/jlt_rag.c src/jlt_index.c src/http.c \
 *       -lpthread -lm
 * Esecuzione:
 *   ./jlt_ollama_speed_test
 *
 * Riferimenti:
 *   arXiv:2103.00564 — An Introduction to Johnson–Lindenstrauss Transforms
 *   Dasgupta & Gupta 2003, Ji et al. 2012
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

#include "rag_embed.h"
#include "jlt_rag.h"
#include "jlt_index.h"
#include "http.h"

/* ── Configurazione ──────────────────────────────────────────────────────── */
#define OLLAMA_HOST  "127.0.0.1"
#define OLLAMA_PORT  11434
#define DOCS_DIR     "/tmp/jlt_speed_docs"
#define N_WARMUP     1    /* query di warmup (scartate) */
#define N_RUNS       5    /* query per misura (media) */

/* ── Timing ad alta risoluzione ──────────────────────────────────────────── */
static double ms_now(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

/* ── Scrittura file documenti ────────────────────────────────────────────── */
static void write_doc(const char *name, const char *text) {
    char path[256];
    snprintf(path, sizeof path, "%s/%s", DOCS_DIR, name);
    FILE *f = fopen(path, "w");
    if (f) { fputs(text, f); fclose(f); }
}

static void setup_docs(void) {
    mkdir(DOCS_DIR, 0755);

    write_doc("matematica.txt",
        "La matematica studia i numeri, le strutture e le trasformazioni.\n"
        "Il teorema di Pitagora: a^2 + b^2 = c^2 per triangoli rettangoli.\n"
        "Le derivate misurano la variazione istantanea di una funzione.\n"
        "Gli integrali calcolano aree e volumi sotto curve.\n"
        "I numeri primi hanno infiniti elementi (Euclide, ~300 a.C.).\n"
        "Il teorema fondamentale del calcolo unisce derivate e integrali.\n"
        "La trasformata di Fourier decompone segnali in frequenze.\n");

    write_doc("programmazione.txt",
        "Python e' un linguaggio interpretato ad alto livello.\n"
        "C e' noto per velocita' e controllo diretto della memoria.\n"
        "Gli algoritmi di ordinamento: quicksort O(n log n) in media.\n"
        "La ricerca binaria trova elementi in O(log n) in array ordinati.\n"
        "La programmazione dinamica risolve problemi con sottostruttura ottimale.\n"
        "I design pattern descrivono soluzioni riusabili a problemi comuni.\n"
        "Il garbage collector gestisce automaticamente la memoria in Java/Python.\n");

    write_doc("intelligenza_artificiale.txt",
        "Le reti neurali artificiali si ispirano al cervello umano.\n"
        "Il machine learning permette ai computer di imparare dai dati.\n"
        "Il deep learning usa reti con molti strati (layer) profondi.\n"
        "I transformer sono l'architettura alla base dei moderni LLM.\n"
        "L'attenzione (attention mechanism) permette il contesto a lungo raggio.\n"
        "GPT e LLaMA sono Large Language Models addestrati su enormi corpora.\n"
        "Il fine-tuning adatta un modello pre-addestrato a un compito specifico.\n");

    write_doc("fisica.txt",
        "La meccanica quantistica descrive le particelle subatomiche.\n"
        "La relativita' speciale di Einstein: E = mc^2.\n"
        "La velocita' della luce nel vuoto e' circa 299792 km/s.\n"
        "La termodinamica studia l'energia termica e il calore.\n"
        "Il principio di indeterminazione di Heisenberg limita la precisione.\n"
        "I quark sono le particelle fondamentali dei protoni e neutroni.\n"
        "La forza gravitazionale e' descritta dalla relatività generale.\n");

    write_doc("storia.txt",
        "L'Impero Romano e' caduto nel 476 d.C.\n"
        "Il Rinascimento italiano (XIV-XVI sec.) porta umanesimo e arte.\n"
        "La Rivoluzione Francese del 1789 cambia l'Europa.\n"
        "Napoleone Bonaparte conquista gran parte dell'Europa.\n"
        "La Prima Guerra Mondiale inizia nel 1914 con l'assassinio di Sarajevo.\n"
        "La Seconda Guerra Mondiale termina nel 1945 con la resa dell'Asse.\n"
        "La Guerra Fredda divide il mondo in blocchi USA-URSS fino al 1991.\n");

    write_doc("biologia.txt",
        "Il DNA contiene le istruzioni genetiche di tutti gli organismi viventi.\n"
        "Le cellule sono le unita' fondamentali della vita.\n"
        "La fotosintesi converte luce solare in energia chimica nelle piante.\n"
        "Charles Darwin propone la teoria dell'evoluzione per selezione naturale.\n"
        "Il codice genetico traduce sequenze di basi in proteine.\n"
        "I virus non hanno cellule proprie e si replicano all'interno di cellule ospite.\n"
        "Il sistema immunitario protegge l'organismo da patogeni esterni.\n");

    write_doc("economia.txt",
        "Il PIL misura il valore totale dei beni e servizi prodotti in un paese.\n"
        "L'inflazione riduce il potere d'acquisto della moneta nel tempo.\n"
        "La legge della domanda e offerta regola i prezzi di mercato.\n"
        "Il mercato azionario permette alle aziende di raccogliere capitali.\n"
        "La politica monetaria e' gestita dalle banche centrali.\n"
        "Il libero scambio promuove la specializzazione e l'efficienza.\n"
        "La recessione e' un periodo di contrazione economica prolungata.\n");

    write_doc("astronomia.txt",
        "Il Sistema Solare e' composto da 8 pianeti intorno al Sole.\n"
        "Una stella e' una sfera di plasma che genera energia per fusione nucleare.\n"
        "La Via Lattea e' la galassia che contiene il nostro Sistema Solare.\n"
        "I buchi neri hanno una gravita' talmente forte che nulla sfugge.\n"
        "Il Big Bang e' la teoria sull'origine dell'universo circa 13.8 miliardi di anni fa.\n"
        "Le onde gravitazionali sono perturbazioni nello spaziotempo (Einstein 1916).\n"
        "Marte e' il pianeta piu' simile alla Terra per periodo di rotazione.\n");
}

static void cleanup_docs(void) {
    const char *files[] = {
        "matematica.txt", "programmazione.txt", "intelligenza_artificiale.txt",
        "fisica.txt", "storia.txt", "biologia.txt", "economia.txt",
        "astronomia.txt", NULL
    };
    for (int i = 0; files[i]; i++) {
        char path[256];
        snprintf(path, sizeof path, "%s/%s", DOCS_DIR, files[i]);
        remove(path);
    }
    rmdir(DOCS_DIR);
}

/* ── Query rappresentative (una per argomento) ───────────────────────────── */
static const char *QUERIES[] = {
    "teorema pitagora calcolo derivate integrali matematica",
    "python algoritmo ordinamento quicksort programmazione",
    "reti neurali deep learning transformer LLM intelligenza artificiale",
    "meccanica quantistica relativita einstein fisica particelle",
    "storia medioevo impero romano rinascimento guerre mondiali",
    NULL
};
static const int N_QUERIES = 5;

/* ── Rilevamento Ollama ──────────────────────────────────────────────────── */
static int   g_ollama_ok   = 0;
static char  g_embed_model[64] = "";

static int detect_ollama(void) {
    char names[8][128];
    int n = http_ollama_list(OLLAMA_HOST, OLLAMA_PORT, names, 8);
    if (n <= 0) return 0;
    return rag_embed_detect_model(OLLAMA_HOST, OLLAMA_PORT, g_embed_model);
}

/* ── Barra di progresso semplice ─────────────────────────────────────────── */
static void progress(const char *label, int cur, int tot) {
    int pct = (tot > 0) ? cur * 100 / tot : 0;
    int filled = pct / 5;
    printf("\r  %s [", label);
    for (int i = 0; i < 20; i++) printf(i < filled ? "\xe2\x96\x88" : "\xe2\x96\x91");
    printf("] %d/%d  ", cur, tot);
    fflush(stdout);
}

/* ── Formatta ms → stringa leggibile ─────────────────────────────────────── */
static void fmt_ms(double ms, char *buf, int sz) {
    if (ms < 1000.0)
        snprintf(buf, sz, "%.1f ms", ms);
    else
        snprintf(buf, sz, "%.2f s ", ms / 1000.0);
}

/* ════════════════════════════════════════════════════════════════════════════
   MAIN
   ════════════════════════════════════════════════════════════════════════════ */
int main(void) {
    printf("\n");
    printf("\033[1m\033[36m╔══════════════════════════════════════════════════════════════════╗\033[0m\n");
    printf("\033[1m\033[36m║   JLT Ollama Speed Test — Benchmark con Chiamate Ollama Reali   ║\033[0m\n");
    printf("\033[1m\033[36m║   arXiv:2103.00564 — Johnson–Lindenstrauss Transform for RAG     ║\033[0m\n");
    printf("\033[1m\033[36m╚══════════════════════════════════════════════════════════════════╝\033[0m\n\n");

    /* ── Verifica Ollama ── */
    printf("  Rilevamento Ollama...\n");
    g_ollama_ok = detect_ollama();
    if (!g_ollama_ok) {
        printf("\n  \033[31m[ERRORE]\033[0m Ollama non raggiungibile o nessun modello embedding trovato.\n");
        printf("  Assicurati che Ollama sia avviato e che sia installato un modello embedding:\n");
        printf("    ollama pull nomic-embed-text\n\n");
        return 1;
    }
    printf("  \033[32m[OK]\033[0m Ollama attivo — modello embedding: \033[1m%s\033[0m\n\n", g_embed_model);

    /* ── Crea documenti di test ── */
    setup_docs();
    printf("  Documenti creati in %s (8 file tematici)\n\n", DOCS_DIR);

    /* ── FASE 1: Prima esecuzione (embedding live da Ollama) ── */
    printf("\033[1m┌─ FASE 1: Prima esecuzione (embedding generati da Ollama)\033[0m\n");
    printf("\033[1m│\033[0m  I motori A e B chiamano Ollama per ogni documento.\n");
    printf("\033[1m│\033[0m  Il motore C (JLT-RAM) indicizza tutto in RAM prima.\n");
    printf("\033[1m└────────────────────────────────────────────────────────\033[0m\n\n");

    double total_A_first = 0, total_B_first = 0, total_C_first = 0;
    char ctx_buf[32768];

    /* ── Motore A: rag_embed (primo run, genera embedding su disco) ── */
    printf("  [A] rag_embed   — calcolando embedding completi (768D)...\n");
    for (int qi = 0; qi < N_WARMUP; qi++) {
        rag_embed_query(OLLAMA_HOST, OLLAMA_PORT, g_embed_model,
                        DOCS_DIR, QUERIES[0], ctx_buf, sizeof ctx_buf);
    }
    for (int qi = 0; qi < N_QUERIES; qi++) {
        progress("A", qi + 1, N_QUERIES);
        double t0 = ms_now();
        rag_embed_query(OLLAMA_HOST, OLLAMA_PORT, g_embed_model,
                        DOCS_DIR, QUERIES[qi], ctx_buf, sizeof ctx_buf);
        total_A_first += ms_now() - t0;
    }
    printf("\n");

    /* ── Motore B: jlt_rag (primo run, genera embedding JLT su disco) ── */
    printf("  [B] jlt_rag     — calcolando embedding JLT compressi...\n");
    for (int qi = 0; qi < N_WARMUP; qi++) {
        jlt_rag_query(OLLAMA_HOST, OLLAMA_PORT, g_embed_model,
                      DOCS_DIR, QUERIES[0], ctx_buf, sizeof ctx_buf, NULL);
    }
    for (int qi = 0; qi < N_QUERIES; qi++) {
        progress("B", qi + 1, N_QUERIES);
        double t0 = ms_now();
        jlt_rag_query(OLLAMA_HOST, OLLAMA_PORT, g_embed_model,
                      DOCS_DIR, QUERIES[qi], ctx_buf, sizeof ctx_buf, NULL);
        total_B_first += ms_now() - t0;
    }
    printf("\n");

    /* ── Motore C: jlt_index (build in-RAM, include il build) ── */
    printf("  [C] jlt_index   — indicizzando tutto in RAM (speedup 7x)...\n");
    JltIndexStats stats;
    double t_build0 = ms_now();
    JltIndex *idx = jlt_index_build_ex(OLLAMA_HOST, OLLAMA_PORT, g_embed_model,
                                        DOCS_DIR, JLT_IDX_SPEEDUP_OPTIMAL, &stats);
    double ms_build = ms_now() - t_build0;

    if (!idx) {
        printf("\n  \033[31m[ERRORE]\033[0m jlt_index_build_ex fallito.\n");
        cleanup_docs();
        return 1;
    }
    printf("  Indice costruito in %.0f ms  (%d chunk, %zuKB → %zuKB RAM)\n",
           ms_build, stats.n_chunks, stats.ram_orig_kb, stats.ram_jlt_kb);

    /* Prima run inclusa nel build time — misura query pure */
    for (int qi = 0; qi < N_WARMUP; qi++) {
        jlt_index_query(idx, OLLAMA_HOST, OLLAMA_PORT, g_embed_model,
                        QUERIES[0], ctx_buf, sizeof ctx_buf);
    }
    for (int qi = 0; qi < N_QUERIES; qi++) {
        progress("C", qi + 1, N_QUERIES);
        double t0 = ms_now();
        jlt_index_query(idx, OLLAMA_HOST, OLLAMA_PORT, g_embed_model,
                        QUERIES[qi], ctx_buf, sizeof ctx_buf);
        total_C_first += ms_now() - t0;
    }
    printf("\n\n");

    double avg_A1 = total_A_first / N_QUERIES;
    double avg_B1 = total_B_first / N_QUERIES;
    double avg_C1 = total_C_first / N_QUERIES;

    /* ── FASE 2: Seconda esecuzione (tutti dalla cache) ── */
    printf("\033[1m┌─ FASE 2: Seconda esecuzione (cache calda — latenza produzione)\033[0m\n");
    printf("\033[1m│\033[0m  Embedding già su disco (A,B) o in RAM (C). Zero chiamate Ollama.\n");
    printf("\033[1m└────────────────────────────────────────────────────────────────\033[0m\n\n");

    double total_A2 = 0, total_B2 = 0, total_C2 = 0;

    printf("  [A] rag_embed   — lettura cache disco...\n");
    for (int qi = 0; qi < N_QUERIES; qi++) {
        progress("A", qi + 1, N_QUERIES);
        double t0 = ms_now();
        rag_embed_query(OLLAMA_HOST, OLLAMA_PORT, g_embed_model,
                        DOCS_DIR, QUERIES[qi], ctx_buf, sizeof ctx_buf);
        total_A2 += ms_now() - t0;
    }
    printf("\n");

    printf("  [B] jlt_rag     — lettura cache JLT disco...\n");
    for (int qi = 0; qi < N_QUERIES; qi++) {
        progress("B", qi + 1, N_QUERIES);
        double t0 = ms_now();
        jlt_rag_query(OLLAMA_HOST, OLLAMA_PORT, g_embed_model,
                      DOCS_DIR, QUERIES[qi], ctx_buf, sizeof ctx_buf, NULL);
        total_B2 += ms_now() - t0;
    }
    printf("\n");

    printf("  [C] jlt_index   — ricerca pura in RAM (zero disco)...\n");
    for (int qi = 0; qi < N_QUERIES; qi++) {
        progress("C", qi + 1, N_QUERIES);
        double t0 = ms_now();
        jlt_index_query(idx, OLLAMA_HOST, OLLAMA_PORT, g_embed_model,
                        QUERIES[qi], ctx_buf, sizeof ctx_buf);
        total_C2 += ms_now() - t0;
    }
    printf("\n\n");

    double avg_A2 = total_A2 / N_QUERIES;
    double avg_B2 = total_B2 / N_QUERIES;
    double avg_C2 = total_C2 / N_QUERIES;

    /* ── Tabella risultati ── */
    printf("\033[1m╔══════════════════════════════════════════════════════════════════════╗\033[0m\n");
    printf("\033[1m║                    RISULTATI BENCHMARK                              ║\033[0m\n");
    printf("\033[1m╠══════════════════════════════════════════════════════════════════════╣\033[0m\n");

    /* Header */
    printf("  %-22s  %10s  %10s  %10s  %10s\n",
           "Motore", "1° run", "2° run", "vs A (2°)", "RAM embed");
    printf("  %-22s  %10s  %10s  %10s  %10s\n",
           "──────────────────────",
           "──────────", "──────────", "──────────", "──────────");

    /* Calcola dimensioni RAM stimate */
    int d_orig = stats.d_orig;
    int m_jlt  = stats.m_dim;
    int n_ch   = stats.n_chunks;
    size_t ram_full = (size_t)n_ch * d_orig * sizeof(float);
    size_t ram_jlt  = (size_t)n_ch * m_jlt  * sizeof(float);

    /* Riga A */
    char a1[32], a2[32]; fmt_ms(avg_A1, a1, 32); fmt_ms(avg_A2, a2, 32);
    char ram_a[32]; snprintf(ram_a, 32, "%zu KB", ram_full / 1024);
    printf("  %-22s  %10s  %10s  %10s  %10s  \033[90m(baseline)\033[0m\n",
           "A: rag_embed (768D)", a1, a2, "—", ram_a);

    /* Riga B */
    char b1[32], b2[32]; fmt_ms(avg_B1, b1, 32); fmt_ms(avg_B2, b2, 32);
    double sp_B2 = (avg_B2 > 0.001 && avg_A2 > 0.001) ? avg_A2 / avg_B2 : 0;
    double pct_B = (avg_A2 > 0) ? (avg_A2 - avg_B2) / avg_A2 * 100.0 : 0;
    char ram_b[32]; snprintf(ram_b, 32, "%zu KB", ram_jlt / 1024);
    char spB[32];
    if (sp_B2 >= 1.0)
        snprintf(spB, 32, "%.1fx \033[32m(+%.0f%%)\033[0m", sp_B2, pct_B);
    else
        snprintf(spB, 32, "%.1fx \033[33m(%.0f%%)\033[0m", sp_B2, pct_B);
    printf("  %-22s  %10s  %10s  %-20s  %10s\n",
           "B: jlt_rag (SRHT)", b1, b2, spB, ram_b);

    /* Riga C */
    char c1[32], c2[32]; fmt_ms(avg_C1, c1, 32); fmt_ms(avg_C2, c2, 32);
    double sp_C2 = (avg_C2 > 0.001 && avg_A2 > 0.001) ? avg_A2 / avg_C2 : 0;
    double pct_C = (avg_A2 > 0) ? (avg_A2 - avg_C2) / avg_A2 * 100.0 : 0;
    char spC[32];
    if (sp_C2 >= 1.0)
        snprintf(spC, 32, "%.1fx \033[32m(+%.0f%%)\033[0m", sp_C2, pct_C);
    else
        snprintf(spC, 32, "%.1fx \033[33m(%.0f%%)\033[0m", sp_C2, pct_C);
    printf("  %-22s  %10s  %10s  %-20s  %10s  \033[33m\xe2\x86\x90 ottimale\033[0m\n",
           "C: jlt_index (RAM)", c1, c2, spC, ram_b);

    printf("\n");

    /* ── Note aggiuntive ── */
    printf("  \033[1mDettagli JLT:\033[0m\n");
    printf("    Dimensione originale  : %d D  (modello %s)\n", d_orig, g_embed_model);
    printf("    Dimensione compressa  : %d D  (speedup_target=7, m=d/7)\n", m_jlt);
    printf("    Chunk indicizzati     : %d\n", n_ch);
    printf("    RAM originale stimata : %zu KB  (%d chunk × %dD × 4B)\n",
           ram_full / 1024, n_ch, d_orig);
    printf("    RAM con JLT           : %zu KB  (%d chunk × %dD × 4B)  → %.1fx meno\033[0m\n",
           ram_jlt / 1024, n_ch, m_jlt, (double)ram_full / ram_jlt);
    printf("    Build indice RAM      : %.0f ms  (include tutte le chiamate Ollama)\n", ms_build);
    printf("    KV cache suggerito    : %d token\n",
           jlt_index_kv_ctx(idx, 200, 150, 20));
    printf("\n");

    printf("  \033[1mInterpretazione:\033[0m\n");
    printf("    1° run: include il costo Ollama di generare embedding per ogni documento.\n");
    printf("    2° run: misura la latenza reale in produzione (cache calda).\n");
    printf("    Il motore C (jlt_index) tiene tutto in RAM: latenza minima per query ripetute.\n");
    printf("    La compressione JLT preserva l'ordine dei risultati al ~95%% (Ji et al. 2012).\n");
    printf("\n");
    printf("  \033[90mRiferimento: arXiv:2103.00564, Dasgupta & Gupta 2003, Ji et al. 2012\033[0m\n\n");

    /* ── Cleanup ── */
    jlt_index_free(idx);
    cleanup_docs();

    return 0;
}
