/*
 * sim_llm.c — Algoritmi LLM: Entropia Pesi, GPTQ, SparseGPT
 * =============================================================
 * Simulazioni didattiche su matrici di pesi 4x8 (layer fittizio).
 * Nessun modello reale: i valori sono scelti per mostrare chiaramente
 * il funzionamento degli algoritmi.
 *
 * sim_entropia_pesi  — calcola entropia di Shannon per layer, bar chart
 * sim_gptq_sparsegpt — visualizza GPTQ (compensazione errore) +
 *                      SparseGPT (pruning per importanza) + confronto errori
 */
#include "sim_common.h"

/* ── Costanti condivise ───────────────────────────────────────────────── */
#define LLM_ROWS  4     /* layer simulati (righe della matrice) */
#define LLM_COLS  8     /* pesi per layer */
#define LLM_BINS  4     /* bucket per calcolo entropia */
#define LLM_QLEVELS 4   /* livelli quantizzazione Q2 (per visualita') */

/* Matrice di pesi fissa (valori gaussiani realistici, range ~[-1, 1]) */
static const double _W[LLM_ROWS][LLM_COLS] = {
    {  0.82, -0.14,  0.67, -0.91,  0.03,  0.55, -0.38,  0.71 },  /* layer A: spread alto */
    {  0.12, -0.09,  0.08, -0.11,  0.10, -0.07,  0.09, -0.08 },  /* layer B: valori piccoli */
    {  0.95, -0.88,  0.91, -0.93,  0.87, -0.90,  0.92, -0.86 },  /* layer C: bimodale */
    {  0.44,  0.41,  0.47,  0.43,  0.45,  0.42,  0.46,  0.44 },  /* layer D: quasi uniforme */
};
static const char *_LAYER_NOME[LLM_ROWS] = {
    "Layer A (attention Q)", "Layer B (feed-forward 1)",
    "Layer C (attention K)", "Layer D (embedding out)"
};

/* ── Quantizzazione Q4 semplificata (4 livelli simmetrici) ───────────── */
static double _quantizza(double w) {
    /* livelli: -1.0, -0.33, +0.33, +1.0 */
    double livelli[LLM_QLEVELS] = { -1.0, -0.33, 0.33, 1.0 };
    double best = livelli[0];
    double best_d = fabs(w - best);
    for (int i = 1; i < LLM_QLEVELS; i++) {
        double d = fabs(w - livelli[i]);
        if (d < best_d) { best_d = d; best = livelli[i]; }
    }
    return best;
}

/* ── Barra ASCII proporzionale ───────────────────────────────────────── */
static void _stampa_barra(double v, double max_v, int max_w, const char *col) {
    int n = (max_v > 0) ? (int)(v / max_v * max_w) : 0;
    if (n < 0) n = 0;
    if (n > max_w) n = max_w;
    printf("%s", col);
    for (int i = 0; i < n; i++) printf("\xe2\x96\x88");   /* █ */
    printf(RST);
    for (int i = n; i < max_w; i++) printf("\xe2\x96\x91"); /* ░ */
}

/* ══════════════════════════════════════════════════════════════════════
   sim_entropia_pesi — entropia di Shannon per layer
   ══════════════════════════════════════════════════════════════════════ */

/*
 * Calcola entropia H = -sum(p * log2(p)) dividendo i pesi in LLM_BINS bucket.
 * Entropia alta  → distribuzione uniforme → pesi importanti, incomprimibili
 * Entropia bassa → distribuzione concentrata → layer ridondante, eliminabile
 */
int sim_entropia_pesi(void) {
    sim_log_reset();
    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x4e\x8b  Entropia di Shannon — Analisi Pesi LLM\n" RST);
    printf(GRY "  H = -\xce\xa3 p(x) * log\xe2\x82\x82(p(x))   [bit per simbolo]\n\n" RST);
    printf("  Matrice simulata: %d layer \xc3\x97 %d pesi  (es. strato attention trasformer)\n\n",
           LLM_ROWS, LLM_COLS);

    /* mostra matrice grezza */
    for (int r = 0; r < LLM_ROWS; r++) {
        printf("  %s\n  " GRY, _LAYER_NOME[r]);
        for (int c = 0; c < LLM_COLS; c++)
            printf("%+.2f  ", _W[r][c]);
        printf(RST "\n\n");
    }
    printf(GRY "  [INVIO per calcolare entropia per layer] " RST); fflush(stdout);
    if (sim_aspetta_passo()) return 0;

    /* calcola entropia per ogni layer */
    double entropie[LLM_ROWS];
    double max_H = 0;

    for (int r = 0; r < LLM_ROWS; r++) {
        /* conta frequenze nei bucket [-1,-0.5), [-0.5,0), [0,0.5), [0.5,1] */
        int cnt[LLM_BINS] = {0};
        for (int c = 0; c < LLM_COLS; c++) {
            double w = _W[r][c];
            int b = (int)((w + 1.0) / 2.0 * LLM_BINS);
            if (b < 0) b = 0;
            if (b >= LLM_BINS) b = LLM_BINS - 1;
            cnt[b]++;
        }
        double H = 0.0;
        for (int b = 0; b < LLM_BINS; b++) {
            if (cnt[b] == 0) continue;
            double p = (double)cnt[b] / LLM_COLS;
            H -= p * log2(p);
        }
        entropie[r] = H;
        if (H > max_H) max_H = H;
    }

    /* visualizza */
    double H_max_teorica = log2(LLM_BINS); /* es. log2(4) = 2.0 bit */
    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x4e\x8b  Entropia per Layer  " GRY "(max teorico = %.2f bit)\n\n" RST,
           H_max_teorica);

    const char *consigli[] = {
        "TENERE — pesi distribuiti, alta informazione",
        "PRUNING AGGRESSIVO — pesi concentrati vicino a zero",
        "TENERE — distribuzione bimodale, struttura forte",
        "PRUNING MODERATO — pesi quasi identici, ridondanza alta"
    };
    const char *colori[] = { GRN, YLW, GRN, MAG };

    for (int r = 0; r < LLM_ROWS; r++) {
        double H = entropie[r];
        double perc = H / H_max_teorica * 100.0;
        printf("  %s\n", _LAYER_NOME[r]);
        printf("  H = %.3f bit  (%.0f%% del max)  ", H, perc);
        _stampa_barra(H, H_max_teorica, 20, colori[r]);
        printf("\n");
        printf("  " GRY "\xe2\x86\x92 %s\n\n" RST, consigli[r]);
        printf(GRY "  [INVIO] " RST); fflush(stdout);
        if (sim_aspetta_passo()) return 0;
    }

    clear_screen(); print_header();
    printf(GRN BLD "\n  \xe2\x9c\x94  Riepilogo Entropia\n\n" RST);
    printf("  %-30s  %s  %s\n", "Layer", "Entropia", "Decisione");
    printf("  %s\n", GRY "──────────────────────────────────────────────────" RST);
    for (int r = 0; r < LLM_ROWS; r++) {
        const char *dec = (entropie[r] > 1.5) ? GRN "TENERE    " RST :
                          (entropie[r] > 0.8) ? MAG "RIDURRE   " RST :
                                                YLW "ELIMINARE " RST;
        printf("  %-30s  %.3f bit  %s\n", _LAYER_NOME[r], entropie[r], dec);
    }
    printf("\n" GRY "  Entropia guida quali layer quantizzare/potare prima — zero token AI\n\n" RST);
    printf(GRY "  [INVIO] " RST); fflush(stdout); sim_aspetta_passo(); return 1;
}

/* ══════════════════════════════════════════════════════════════════════
   sim_gptq_sparsegpt — quantizzazione GPTQ + pruning SparseGPT
   ══════════════════════════════════════════════════════════════════════ */

/*
 * GPTQ: quantizza peso per peso; dopo ogni quantizzazione redistribuisce
 * l'errore ai pesi rimanenti usando la diagonale dell'Hessiano (semplificata
 * qui come w_i^2, che e' l'approssimazione OBD — Optimal Brain Damage).
 *
 * SparseGPT: score di importanza = w_i^2 / H_ii (semplificato: w_i^2).
 * Prune il 50% dei pesi con importanza minore (li azzera).
 *
 * Confronto errore quadratico medio: Naive Q4 vs GPTQ vs SparseGPT.
 */
int sim_gptq_sparsegpt(void) {
    /* lavora sulla prima riga (Layer A) come esempio */
    int R = 0;
    double w_orig[LLM_COLS];
    double w_naive[LLM_COLS];
    double w_gptq[LLM_COLS];
    double w_sparse[LLM_COLS];
    double importanza[LLM_COLS];
    char msg[128];

    for (int c = 0; c < LLM_COLS; c++) w_orig[c] = _W[R][c];

    sim_log_reset();
    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\xa4\x96  GPTQ + SparseGPT — Compressione LLM\n" RST);
    printf(GRY "  Layer analizzato: %s\n\n" RST, _LAYER_NOME[R]);
    printf("  Pesi originali (float32 simulato):\n  ");
    for (int c = 0; c < LLM_COLS; c++) printf(CYN "%+.3f  " RST, w_orig[c]);
    printf("\n\n");
    printf(GRY "  Passi:\n");
    printf("  1. Quantizzazione NAIVE Q4  — arrotonda ogni peso, ignora errore\n");
    printf("  2. GPTQ                     — dopo ogni peso, compensa i rimanenti\n");
    printf("  3. SparseGPT                — pota 50%% per importanza (w^2/H_ii)\n");
    printf("  4. Confronto errori MSE\n\n" RST);
    printf(GRY "  [INVIO per iniziare] " RST); fflush(stdout);
    if (sim_aspetta_passo()) return 0;

    /* ── PASSO 1: Quantizzazione NAIVE ─────────────────────────────── */
    clear_screen(); print_header();
    printf(CYN BLD "\n  [1/4]  Quantizzazione NAIVE — Q4 senza compensazione\n\n" RST);
    printf(GRY "  Ogni peso arrotondato al livello piu' vicino. Errore ignorato.\n" RST);
    printf(GRY "  Livelli Q4: -1.00  -0.33  +0.33  +1.00\n\n" RST);

    double mse_naive = 0;
    for (int c = 0; c < LLM_COLS; c++) {
        w_naive[c] = _quantizza(w_orig[c]);
        double err = w_orig[c] - w_naive[c];
        mse_naive += err * err;
        printf("  w[%d] %+.3f  \xe2\x86\x92  q=%+.3f  err=%+.4f\n",
               c, w_orig[c], w_naive[c], err);
    }
    mse_naive /= LLM_COLS;
    printf(GRY "\n  MSE naive: " RST YLW "%.6f\n" RST, mse_naive);
    printf("\n" GRY "  [INVIO] " RST); fflush(stdout);
    if (sim_aspetta_passo()) return 0;

    /* ── PASSO 2: GPTQ ─────────────────────────────────────────────── */
    clear_screen(); print_header();
    printf(CYN BLD "\n  [2/4]  GPTQ — compensazione errore sui pesi rimanenti\n\n" RST);
    printf(GRY "  Idea: quantizza w[i], calcola errore e,\n");
    printf("  distribuisci e ai pesi successivi w[j] -= e * (w[j]^2 / sum_w^2)\n");
    printf("  (Hessiano semplificato: H_ii proporzionale a w_i^2)\n\n" RST);

    for (int c = 0; c < LLM_COLS; c++) w_gptq[c] = w_orig[c];

    double mse_gptq = 0;
    sim_log_reset();
    for (int c = 0; c < LLM_COLS; c++) {
        double q     = _quantizza(w_gptq[c]);
        double err   = w_gptq[c] - q;

        /* calcola somma H dei rimanenti per normalizzare */
        double sum_h = 0;
        for (int j = c + 1; j < LLM_COLS; j++)
            sum_h += w_gptq[j] * w_gptq[j] + 1e-6;

        snprintf(msg, sizeof msg,
                 "w[%d] %+.3f\xe2\x86\x92q=%+.3f  err=%+.4f  compensa %d pesi",
                 c, w_gptq[c], q, err, LLM_COLS - c - 1);
        sim_log_push(msg);
        w_gptq[c] = q;

        /* redistribuisci errore proporzionalmente all'importanza H */
        if (sum_h > 0) {
            for (int j = c + 1; j < LLM_COLS; j++) {
                double h_jj = w_gptq[j] * w_gptq[j] + 1e-6;
                w_gptq[j] -= err * (h_jj / sum_h);
            }
        }

        clear_screen(); print_header();
        printf(CYN BLD "\n  [2/4]  GPTQ — passo %d/%d\n\n" RST, c+1, LLM_COLS);
        for (int i = 0; i < _sim_log_n; i++) {
            if (i < _sim_log_n - 1) printf(GRY "  %s\n" RST, _sim_log[i]);
            else                    printf(CYN BLD "  %s\n" RST, _sim_log[i]);
        }
        printf("\n  Pesi correnti dopo compensazione:\n  ");
        for (int j = 0; j < LLM_COLS; j++) {
            if (j <= c) printf(GRN "%+.3f  " RST, w_gptq[j]);
            else        printf(YLW "%+.3f  " RST, w_gptq[j]);
        }
        printf(GRY "\n  Verde=quantizzato  Giallo=aggiornato dalla compensazione\n" RST);
        printf("\n" GRY "  [INVIO=passo  ESC=esci] " RST); fflush(stdout);
        if (sim_aspetta_passo()) return 0;

        mse_gptq += (w_gptq[c] - w_orig[c]) * (w_gptq[c] - w_orig[c]);
    }
    mse_gptq /= LLM_COLS;
    printf(GRY "\n  MSE GPTQ: " RST GRN "%.6f\n\n" RST, mse_gptq);
    printf(GRY "  [INVIO] " RST); fflush(stdout);
    if (sim_aspetta_passo()) return 0;

    /* ── PASSO 3: SparseGPT ────────────────────────────────────────── */
    clear_screen(); print_header();
    printf(CYN BLD "\n  [3/4]  SparseGPT — pruning per importanza\n\n" RST);
    printf(GRY "  Score importanza: s_i = w_i^2 / H_ii  (semplif.: s_i = w_i^2)\n");
    printf("  Pota il 50%% dei pesi con score piu' basso (li azzera)\n\n" RST);

    for (int c = 0; c < LLM_COLS; c++) {
        w_sparse[c]    = w_orig[c];
        importanza[c]  = w_orig[c] * w_orig[c];
        printf("  w[%d] %+.3f  score=%.4f\n", c, w_orig[c], importanza[c]);
    }

    /* trova soglia al 50° percentile */
    double imp_sorted[LLM_COLS];
    for (int c = 0; c < LLM_COLS; c++) imp_sorted[c] = importanza[c];
    /* bubble sort su LLM_COLS=8 elementi */
    for (int i = 0; i < LLM_COLS - 1; i++)
        for (int j = i+1; j < LLM_COLS; j++)
            if (imp_sorted[j] < imp_sorted[i]) {
                double t = imp_sorted[i]; imp_sorted[i] = imp_sorted[j]; imp_sorted[j] = t;
            }
    double soglia = imp_sorted[LLM_COLS / 2 - 1];

    printf(GRY "\n  Soglia 50%% percentile: %.4f\n\n" RST, soglia);
    printf("  Risultato pruning:\n");
    double mse_sparse = 0;
    int pruned = 0;
    for (int c = 0; c < LLM_COLS; c++) {
        if (importanza[c] <= soglia) {
            w_sparse[c] = 0.0;
            pruned++;
            printf("  w[%d] %+.3f  " YLW "\xe2\x86\x92 ZERO (potato, score basso)\n" RST,
                   c, w_orig[c]);
        } else {
            printf("  w[%d] %+.3f  " GRN "\xe2\x86\x92 TENUTO (score alto)\n" RST,
                   c, w_orig[c]);
        }
        double err = w_sparse[c] - w_orig[c];
        mse_sparse += err * err;
    }
    mse_sparse /= LLM_COLS;
    printf(GRY "\n  Pesi annullati: %d/%d  (%.0f%% sparsita')\n" RST,
           pruned, LLM_COLS, (double)pruned/LLM_COLS*100.0);
    printf(GRY "  MSE SparseGPT: " RST MAG "%.6f\n\n" RST, mse_sparse);
    printf(GRY "  [INVIO] " RST); fflush(stdout);
    if (sim_aspetta_passo()) return 0;

    /* ── PASSO 4: Confronto ────────────────────────────────────────── */
    clear_screen(); print_header();
    printf(GRN BLD "\n  [4/4]  Confronto MSE — errore quadratico medio\n\n" RST);

    double mse_max = mse_naive > mse_sparse ? mse_naive : mse_sparse;
    if (mse_gptq > mse_max) mse_max = mse_gptq;

    printf("  %-18s  MSE        Barra errore\n", "Metodo");
    printf("  %s\n", GRY "─────────────────────────────────────────────────" RST);

    printf("  %-18s  " YLW "%.6f  " RST, "Naive Q4", mse_naive);
    _stampa_barra(mse_naive, mse_max, 20, YLW); printf("\n");

    printf("  %-18s  " GRN "%.6f  " RST, "GPTQ", mse_gptq);
    _stampa_barra(mse_gptq, mse_max, 20, GRN); printf("\n");

    printf("  %-18s  " MAG "%.6f  " RST, "SparseGPT 50%%", mse_sparse);
    _stampa_barra(mse_sparse, mse_max, 20, MAG); printf("\n\n");

    double miglioramento = (mse_naive - mse_gptq) / mse_naive * 100.0;
    printf(GRN "  GPTQ riduce l'errore del " BLD "%.1f%%\n" RST, miglioramento);
    printf(GRY "  SparseGPT azzera il 50%% dei pesi — utile per formato sparse\n\n" RST);

    printf("  Dimensioni dopo compressione:\n");
    printf("  %-18s  %d x float32 = %d byte\n", "Originale", LLM_COLS, LLM_COLS * 4);
    printf("  %-18s  %d x Q4     = %d byte  (-75%%)\n", "GPTQ Q4", LLM_COLS, LLM_COLS / 2);
    printf("  %-18s  %d x float32 = %d byte  (-50%%)\n",
           "SparseGPT 50%%", LLM_COLS - pruned, (LLM_COLS - pruned) * 4);
    printf("  %-18s  %d x Q4     ~ %d byte  (-87%%)\n",
           "GPTQ+SparseGPT", LLM_COLS - pruned, (LLM_COLS - pruned) / 2);

    printf("\n" GRY "  [INVIO] " RST); fflush(stdout); sim_aspetta_passo(); return 1;
}
