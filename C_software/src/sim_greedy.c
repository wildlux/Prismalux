/*
 * sim_greedy.c — Algoritmi Greedy (3)
 * =====================================
 * Activity Selection, Zaino Frazionario, Huffman Coding
 */
#include "sim_common.h"

/* ── Activity Selection ──────────────────────────────────────────────── */
int sim_activity_selection(void) {
    struct { int start, end; const char *nome; } acts[] = {
        {1,4,"A1"},{3,5,"A2"},{0,6,"A3"},{5,7,"A4"},
        {3,9,"A5"},{5,9,"A6"},{6,10,"A7"},{8,11,"A8"},{8,12,"A9"},{2,14,"A10"}
    };
    int na = 10;
    sim_log_reset();
    char msg[128];

    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x93\x85  Activity Selection — Greedy\n" RST);
    printf(GRY "  Seleziona il massimo numero di attivit\xc3\xa0 non sovrapposte\n");
    printf("  Strategia: scegli sempre quella che finisce prima\n\n" RST);
    printf(GRY "  Attivit\xc3\xa0 (ordinate per fine):\n");
    for (int i = 0; i < na; i++)
        printf("  %s: [%2d, %2d)\n", acts[i].nome, acts[i].start, acts[i].end);
    printf("\n  [INVIO per iniziare] " RST); fflush(stdout);
    if (sim_aspetta_passo()) return 0;

    int last_end = 0, count = 0;
    int selected[10]; for (int i = 0; i < 10; i++) selected[i] = 0;

    for (int i = 0; i < na; i++) {
        if (acts[i].start >= last_end) {
            selected[i] = 1;
            count++;
            last_end = acts[i].end;
            snprintf(msg, sizeof msg, "Seleziona %s [%d,%d) — fine aggiornata a %d", acts[i].nome, acts[i].start, acts[i].end, last_end);
        } else {
            snprintf(msg, sizeof msg, "Salta %s [%d,%d) — sovrapposta (fine corrente=%d)", acts[i].nome, acts[i].start, acts[i].end, last_end);
        }
        sim_log_push(msg);

        clear_screen(); print_header();
        printf(CYN BLD "\n  \xf0\x9f\x93\x85  Activity Selection — attivit\xc3\xa0 %d/%d\n\n" RST, i+1, na);
        for (int j = 0; j <= i; j++) {
            const char *col = selected[j] ? GRN : GRY;
            printf("  %s%s [%2d,%2d)%s %s\n", col, acts[j].nome, acts[j].start, acts[j].end, RST, selected[j] ? "\xe2\x9c\x94" : "");
        }
        printf("\n  Selezionate finora: %d\n", count);
        for (int l = 0; l < _sim_log_n; l++) printf(GRY "  %s\n" RST, _sim_log[l]);
        printf("\n" GRY "  [INVIO=passo  ESC=esci] " RST); fflush(stdout);
        if (sim_aspetta_passo()) return 0;
    }
    clear_screen(); print_header();
    printf(GRN BLD "\n  \xe2\x9c\x94  Selezionate %d attivit\xc3\xa0\n\n" RST, count);
    printf(GRY "  [INVIO] " RST); fflush(stdout); sim_aspetta_passo(); return 1;
}

/* ── Zaino Frazionario ───────────────────────────────────────────────── */
int sim_zaino_frazionario(void) {
    struct { const char *nome; double peso, valore, rapporto; } items[] = {
        {"Oro",     10, 60, 6.0},
        {"Argento", 20, 100, 5.0},
        {"Bronzo",  30, 120, 4.0},
        {"Rame",    25, 80,  3.2},
    };
    int ni = 4;
    double capacita = 50.0;
    sim_log_reset();
    char msg[128];

    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x8e\x92  Zaino Frazionario — Greedy\n" RST);
    printf(GRY "  Capacit\xc3\xa0: %.0f kg. Prendi frazioni di oggetti.\n" RST, capacita);
    printf(GRY "  Strategia: ordina per valore/peso decrescente\n\n" RST);
    for (int i = 0; i < ni; i++)
        printf("  %s: peso=%.0f val=%.0f  v/p=%.1f\n", items[i].nome, items[i].peso, items[i].valore, items[i].rapporto);
    printf("\n" GRY "  [INVIO per iniziare] " RST); fflush(stdout);
    if (sim_aspetta_passo()) return 0;

    double rimasta = capacita, valore_tot = 0;
    for (int i = 0; i < ni && rimasta > 0; i++) {
        double preso = items[i].peso < rimasta ? items[i].peso : rimasta;
        double val_preso = preso * items[i].rapporto;
        valore_tot += val_preso;
        rimasta -= preso;

        snprintf(msg, sizeof msg, "Prendo %.0f/%.0f kg di %s (+%.1f valore)", preso, items[i].peso, items[i].nome, val_preso);
        sim_log_push(msg);
        clear_screen(); print_header();
        printf(CYN BLD "\n  \xf0\x9f\x8e\x92  Zaino Frazionario — %s\n\n" RST, items[i].nome);
        printf("  Capacit\xc3\xa0 rimasta: " GRN "%.1f kg" RST "\n", rimasta);
        printf("  Valore totale:    " GRN "%.1f" RST "\n\n", valore_tot);
        for (int l = 0; l < _sim_log_n; l++) printf(GRY "  %s\n" RST, _sim_log[l]);
        printf("\n" GRY "  [INVIO=passo  ESC=esci] " RST); fflush(stdout);
        if (sim_aspetta_passo()) return 0;
    }
    clear_screen(); print_header();
    printf(GRN BLD "\n  \xe2\x9c\x94  Valore ottimale = %.1f\n\n" RST, valore_tot);
    printf(GRY "  [INVIO] " RST); fflush(stdout); sim_aspetta_passo(); return 1;
}

/* ── Huffman (semplificato, 6 simboli) ───────────────────────────────── */
int sim_huffman(void) {
    struct { char sym; int freq; char code[8]; } syms[] = {
        {'A', 45, "0"},
        {'B', 13, "101"},
        {'C', 12, "100"},
        {'D', 16, "111"},
        {'E', 9,  "1101"},
        {'F', 5,  "1100"},
    };
    int ns = 6;
    sim_log_reset();
    char msg[128];

    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x93\x9d  Huffman Coding — Greedy\n" RST);
    printf(GRY "  Comprimi simboli frequenti con codici brevi\n\n" RST);
    printf(GRY "  Simbolo  Freq  Codice\n" RST);
    for (int i = 0; i < ns; i++)
        printf("     %c     %3d   \n", syms[i].sym, syms[i].freq);
    printf("\n" GRY "  [INVIO per procedere] " RST); fflush(stdout);
    if (sim_aspetta_passo()) return 0;

    const char *fasi[] = {
        "Inizia: {A:45} {B:13} {C:12} {D:16} {E:9} {F:5}",
        "Unisci E(9)+F(5) = EF(14): {A:45} {B:13} {C:12} {D:16} {EF:14}",
        "Unisci C(12)+B(13) = CB(25): {A:45} {D:16} {EF:14} {CB:25}",
        "Unisci EF(14)+D(16) = DEF(30): {A:45} {CB:25} {DEF:30}",
        "Unisci CB(25)+DEF(30) = BCDEF(55): {A:45} {BCDEF:55}",
        "Unisci A(45)+BCDEF(55) = radice(100): albero completo!"
    };
    for (int i = 0; i < 6; i++) {
        snprintf(msg, sizeof msg, "Passo %d: %s", i+1, fasi[i]);
        sim_log_push(msg);
        clear_screen(); print_header();
        printf(CYN BLD "\n  \xf0\x9f\x93\x9d  Huffman — passo %d\n\n" RST, i+1);
        for (int l = 0; l < _sim_log_n; l++) {
            if (l == _sim_log_n-1) printf(CYN BLD "  %s\n" RST, _sim_log[l]);
            else printf(GRY "  %s\n" RST, _sim_log[l]);
        }
        printf("\n" GRY "  [INVIO=passo  ESC=esci] " RST); fflush(stdout);
        if (sim_aspetta_passo()) return 0;
    }

    clear_screen(); print_header();
    printf(GRN BLD "\n  \xe2\x9c\x94  Codici Huffman (ottimali):\n\n" RST);
    for (int i = 0; i < ns; i++)
        printf("  %c: %s  (%d bit)\n", syms[i].sym, syms[i].code, (int)strlen(syms[i].code));
    printf("\n  Risparmio vs ASCII (8 bit): %.1f%%\n", (1.0 - 2.3/8.0) * 100);
    printf("\n" GRY "  [INVIO] " RST); fflush(stdout); sim_aspetta_passo(); return 1;
}
