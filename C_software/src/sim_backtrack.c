/*
 * sim_backtrack.c — Algoritmi di Backtracking (2)
 * =================================================
 * N-Queens (N=5), Sudoku 4x4
 */
#include "sim_common.h"

/* ── N-Queens (N=5) ──────────────────────────────────────────────────── */
static int _nq_board[5];
static int _nq_steps;
static int _nq_interrotto;

static int _nq_safe(int row, int col, int n) {
    for (int i = 0; i < row; i++) {
        if (_nq_board[i] == col) return 0;
        if (abs(_nq_board[i] - col) == abs(i - row)) return 0;
    }
    return 1;
}

static void _nq_stampa(int row, int n, const char *msg) {
    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x91\x91  N-Queens (N=%d) — Passo %d\n\n" RST, n, _nq_steps);
    for (int r = 0; r < n; r++) {
        printf("  ");
        for (int c = 0; c < n; c++) {
            if (r < row && _nq_board[r] == c) printf(GRN " Q" RST);
            else if (r == row && c == _nq_board[row] && row < n) printf(YLW " Q" RST);
            else printf(GRY " ." RST);
        }
        printf("\n");
    }
    printf("\n  " YLW "\xe2\x80\xba " RST "%s\n", msg);
    printf("\n" GRY "  [INVIO=passo  ESC=esci] " RST); fflush(stdout);
}

static int _nq_solve(int row, int n) {
    if (_nq_interrotto) return 0;
    if (row == n) return 1;
    for (int col = 0; col < n; col++) {
        _nq_board[row] = col;
        _nq_steps++;
        char msg[64];
        if (_nq_safe(row, col, n)) {
            snprintf(msg, sizeof msg, "Riga %d col %d: sicuro! Provo riga %d", row, col, row+1);
            _nq_stampa(row, n, msg);
            if (sim_aspetta_passo()) { _nq_interrotto = 1; return 0; }
            if (_nq_solve(row+1, n)) return 1;
        } else {
            snprintf(msg, sizeof msg, "Riga %d col %d: conflitto! backtrack", row, col);
            _nq_stampa(row, n, msg);
            if (sim_aspetta_passo()) { _nq_interrotto = 1; return 0; }
        }
    }
    return 0;
}

int sim_n_queens(void) {
    int N = 5;
    memset(_nq_board, 0, sizeof _nq_board);
    _nq_steps = 0; _nq_interrotto = 0;
    sim_log_reset();
    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x91\x91  N-Queens (N=%d) — Backtracking\n" RST, N);
    printf(GRY "  Posiziona %d regine in una scacchiera %dx%d\n" RST, N, N, N);
    printf(GRY "  senza che si attacchino a vicenda.\n\n" RST);
    printf(GRY "  [INVIO per iniziare] " RST); fflush(stdout);
    if (sim_aspetta_passo()) return 0;
    int ok = _nq_solve(0, N);
    if (_nq_interrotto) return 0;
    clear_screen(); print_header();
    if (ok) {
        printf(GRN BLD "\n  \xe2\x9c\x94  Soluzione trovata in %d passi!\n\n" RST, _nq_steps);
        for (int r = 0; r < N; r++) {
            printf("  ");
            for (int c = 0; c < N; c++)
                printf(_nq_board[r] == c ? GRN " Q" RST : GRY " ." RST);
            printf("\n");
        }
    } else { printf(RED "\n  Nessuna soluzione trovata\n" RST); }
    printf("\n" GRY "  [INVIO] " RST); fflush(stdout); sim_aspetta_passo(); return 1;
}

/* ── Sudoku Solver 4x4 ───────────────────────────────────────────────── */
static int _sdk_steps;
static int _sdk_interrotto;
static int _sdk_board[4][4];

static void _sdk_stampa(int r, int c, const char *msg) {
    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x9f\xa8  Sudoku 4x4 — Passo %d\n\n" RST, _sdk_steps);
    for (int ri = 0; ri < 4; ri++) {
        printf("  ");
        if (ri == 2) printf(GRY "  ----+----\n  " RST);
        for (int ci = 0; ci < 4; ci++) {
            if (ci == 2) printf(GRY "|" RST);
            if (_sdk_board[ri][ci] == 0) printf(GRY " ." RST);
            else if (ri == r && ci == c) printf(YLW " %d" RST, _sdk_board[ri][ci]);
            else printf(GRN " %d" RST, _sdk_board[ri][ci]);
        }
        printf("\n");
    }
    printf("\n  " YLW "\xe2\x80\xba " RST "%s\n", msg);
    printf("\n" GRY "  [INVIO=passo  ESC=esci] " RST); fflush(stdout);
}

static int _sdk_valid(int r, int c, int v) {
    for (int i = 0; i < 4; i++) {
        if (_sdk_board[r][i] == v) return 0;
        if (_sdk_board[i][c] == v) return 0;
    }
    int br = (r/2)*2, bc = (c/2)*2;
    for (int i = 0; i < 2; i++) for (int j = 0; j < 2; j++)
        if (_sdk_board[br+i][bc+j] == v) return 0;
    return 1;
}

static int _sdk_solve(void) {
    if (_sdk_interrotto) return 0;
    for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) {
        if (_sdk_board[r][c] == 0) {
            for (int v = 1; v <= 4; v++) {
                _sdk_steps++;
                char msg[64];
                if (_sdk_valid(r, c, v)) {
                    _sdk_board[r][c] = v;
                    snprintf(msg, sizeof msg, "Prova [%d][%d]=%d (valido)", r, c, v);
                    _sdk_stampa(r, c, msg);
                    if (sim_aspetta_passo()) { _sdk_interrotto = 1; return 0; }
                    if (_sdk_solve()) return 1;
                    _sdk_board[r][c] = 0;
                    snprintf(msg, sizeof msg, "Backtrack [%d][%d]=%d", r, c, v);
                    _sdk_stampa(r, c, msg);
                    if (sim_aspetta_passo()) { _sdk_interrotto = 1; return 0; }
                }
            }
            return 0;
        }
    }
    return 1;
}

int sim_sudoku(void) {
    int init[4][4] = {
        {1, 0, 0, 4},
        {0, 4, 1, 0},
        {4, 0, 0, 1},
        {0, 1, 4, 0},
    };
    for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) _sdk_board[r][c] = init[r][c];
    _sdk_steps = 0; _sdk_interrotto = 0;
    sim_log_reset();
    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x9f\xa8  Sudoku 4x4 — Backtracking\n" RST);
    printf(GRY "  Griglia 4x4, cifre 1-4. Nessuna ripetizione in righe, colonne, box 2x2.\n\n" RST);
    printf(GRY "  [INVIO per iniziare] " RST); fflush(stdout);
    if (sim_aspetta_passo()) return 0;
    int ok = _sdk_solve();
    if (_sdk_interrotto) return 0;
    clear_screen(); print_header();
    if (ok) {
        printf(GRN BLD "\n  \xe2\x9c\x94  Sudoku risolto in %d passi!\n\n" RST, _sdk_steps);
        for (int r = 0; r < 4; r++) {
            printf("  ");
            if (r == 2) printf(GRY "  ----+----\n  " RST);
            for (int c = 0; c < 4; c++) {
                if (c == 2) printf(GRY "|" RST);
                printf(GRN " %d" RST, _sdk_board[r][c]);
            }
            printf("\n");
        }
    } else { printf(RED "\n  Nessuna soluzione\n" RST); }
    printf("\n" GRY "  [INVIO] " RST); fflush(stdout); sim_aspetta_passo(); return 1;
}
