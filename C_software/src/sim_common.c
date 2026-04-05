/*
 * sim_common.c — Implementazione helper condivisi Simulatore
 * ===========================================================
 * Funzioni usate da tutti i moduli sim_xxx.c:
 *   sim_log_reset/push  — ring buffer messaggi
 *   sim_aspetta_passo   — attende tasto, ritorna 1 se ESC/q
 *   sim_mostra_array    — visualizzazione ANSI array con barre
 *   sim_genera_array    — generatore array casuale
 *   sim_chiedi_ai       — chat AI post-simulazione
 *   sim_fine            — schermata fine simulazione
 */
#include "sim_common.h"

/* ── Log ring buffer (condiviso tra tutti i moduli) ───────────────────── */
char _sim_log[LOG_MAX][128];
int  _sim_log_n = 0;

/* Accesso interno tramite puntatori extern (usati da sim_mostra_array) */
char (*sim_log_buf)[128] = _sim_log;
int  *sim_log_n_ptr      = &_sim_log_n;

void sim_log_reset(void) {
    _sim_log_n = 0;
}

void sim_log_push(const char *msg) {
    if (_sim_log_n < LOG_MAX)
        snprintf(_sim_log[_sim_log_n++], 128, "%s", msg);
    else {
        /* scorre in avanti (drop oldest) */
        for (int i = 1; i < LOG_MAX; i++)
            memcpy(_sim_log[i-1], _sim_log[i], 128);
        snprintf(_sim_log[LOG_MAX-1], 128, "%s", msg);
    }
}

/* ── aspetta_passo — ritorna 1 se l'utente vuole uscire ──────────────── */
int sim_aspetta_passo(void) {
    int c = getch_single();
    return (c == 27 || c == 'q' || c == 'Q') ? 1 : 0;
}

/* ── mostra_array — cuore della visualizzazione ──────────────────────── */
void sim_mostra_array(const int *arr, int n,
                      int c1, int c2, int s1, int s2, int trovato,
                      unsigned long long ordinati,
                      const char *msg, const char *titolo) {
    int max_v = 1;
    for (int i = 0; i < n; i++) if (arr[i] > max_v) max_v = arr[i];

    clear_screen(); print_header();

    /* Titolo pannello */
    int W = term_width() - 2;
    if (W < 60)  W = 60;
    if (W > 130) W = 130;
    printf(CYN BLD "\n  %s\n" RST, titolo);
    printf(GRY "  \xe2\x80\x94\xe2\x80\x94 Verde=ordinato  " YLW "\xe2\x96\x88=confronto  " RST RED "\xe2\x96\x88=scambio  " RST CYN "\xe2\x96\x88=trovato\n\n" RST);

    /* Barre */
    for (int i = 0; i < n; i++) {
        int bar = (int)((double)arr[i] / max_v * BAR_MAX);
        if (bar < 1) bar = 1;

        const char *col;
        const char *sym = "  ";
        if      (i == s1 || i == s2)                    { col = RED; sym = " \xe2\x86\x95"; }
        else if (i == c1 || i == c2)                    { col = YLW; sym = " ?"; }
        else if (i == trovato)                          { col = CYN; sym = " \xe2\x9c\x93"; }
        else if ((ordinati >> i) & 1ULL)                { col = GRN; sym = "  "; }
        else                                            { col = WHT; sym = "  "; }

        printf("  " GRY "[%2d]" RST " " GRY "%3d" RST " %s", i, arr[i], col);
        for (int b = 0; b < bar; b++) printf("\xe2\x96\x88");
        printf("%s" RST "\n", sym);
    }

    /* Log ultimi passi */
    printf("\n");
    int start = (_sim_log_n > LOG_MAX) ? _sim_log_n - LOG_MAX : 0;
    for (int i = 0; i < _sim_log_n && i < LOG_MAX; i++) {
        int abs_n = start + i + 1;
        if (i < _sim_log_n - 1)
            printf(GRY "  %3d. %s\n" RST, abs_n, _sim_log[i]);
        else
            printf(CYN BLD "  %3d. %s\n" RST, abs_n, _sim_log[i]);
    }

    /* Messaggio corrente */
    if (msg && msg[0]) {
        printf("\n  " YLW "\xe2\x80\xba " RST "%s\n", msg);
    }

    printf("\n" GRY "  [INVIO=passo successivo  ESC/q=interrompi] " RST);
    fflush(stdout);
}

/* ── genera_array — array casuale di n elementi tra min e max ─────────── */
void sim_genera_array(int *arr, int n, int vmin, int vmax) {
    srand((unsigned)time(NULL));
    for (int i = 0; i < n; i++)
        arr[i] = vmin + rand() % (vmax - vmin + 1);
}

/* ── chiedi_ai — chat AI dopo la simulazione ─────────────────────────── */
void sim_chiedi_ai(const char *nome_algo) {
    if (!backend_ready(&g_ctx)) { ensure_ready_or_return(&g_ctx); return; }
    if (!check_risorse_ok()) return;

    char sys[512];
    snprintf(sys, sizeof sys,
        "Sei un esperto di algoritmi e strutture dati. Hai appena mostrato una "
        "simulazione step-by-step di '%s' all'utente. Rispondi alle domande "
        "sull'algoritmo: complessit\xc3\xa0, casi limite, varianti, applicazioni reali. "
        "Rispondi SEMPRE e SOLO in italiano.", nome_algo);

    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\xa4\x96  AI — %s\n" RST, nome_algo);
    printf(GRY "  Fai domande sull'algoritmo. " EM_0 " per tornare.\n\n" RST);

    char q[2048];
    while (1) {
        printf(YLW "Tu: " RST);
        if (!fgets(q, sizeof q, stdin)) break;
        q[strcspn(q, "\n")] = '\0';
        if (!q[0] || q[0] == '0' || strcmp(q,"q")==0 || strcmp(q,"esci")==0) break;
        printf(MAG "AI: " RST);
        ai_chat_stream(&g_ctx, sys, q, stream_cb, NULL, NULL, 0);
        printf("\n");
    }
}

/* ── schermata fine simulazione ──────────────────────────────────────── */
void sim_fine(const char *nome) {
    clear_screen(); print_header();
    printf(GRN BLD "\n  \xe2\x9c\x94  Simulazione completata: %s\n\n" RST, nome);
    printf("  " EM_1 "  Chiedi all'AI sull'algoritmo\n");
    printf("  " EM_2 "  Nuova simulazione (numeri diversi)\n");
    printf("  " EM_0 "  Torna\n\n  " GRN "Scelta: " RST); fflush(stdout);
}
