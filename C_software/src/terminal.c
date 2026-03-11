/*
 * terminal.c — SEZIONE 2: Terminal utilities
 * getch_single, read_key_ext, clear_screen
 */
#include "common.h"
#include "terminal.h"

/* ══════════════════════════════════════════════════════════════
   SEZIONE 2 — Terminal utilities
   ══════════════════════════════════════════════════════════════ */

#ifndef _WIN32
static struct termios g_orig_termios;
static int g_raw_active = 0;

void term_restore(void) {
    if (g_raw_active) { tcsetattr(STDIN_FILENO, TCSANOW, &g_orig_termios); g_raw_active=0; }
}
static void term_raw_on(void) {
    if (!g_raw_active) { tcgetattr(STDIN_FILENO, &g_orig_termios); atexit(term_restore); g_raw_active=1; }
    struct termios raw = g_orig_termios;
    raw.c_lflag &= (tcflag_t)~(ECHO|ICANON);
    raw.c_cc[VMIN]=1; raw.c_cc[VTIME]=0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}
static void term_raw_off(void) {
    if (g_raw_active) tcsetattr(STDIN_FILENO, TCSANOW, &g_orig_termios);
}
#endif

int getch_single(void) {
#ifdef _WIN32
    return _getch();
#else
    term_raw_on();
    int c = getchar();
    term_raw_off();
    return c;
#endif
}

/* Come getch_single ma rileva anche F2 (sequenza ESC) */
int read_key_ext(void) {
#ifdef _WIN32
    int c = _getch();
    if (c == 0 || c == 0xE0) {
        int c2 = _getch();
        if (c2 == 60) return KEY_F2;  /* F2 su Windows */
        return 0;
    }
    return c;
#else
    term_raw_on();
    int c = getchar();
    if (c == 0x1b) {
        /* Verifica se seguono byte (sequenza escape per tasti funzione) */
        struct timeval tv = {0, 50000};  /* 50 ms */
        fd_set fds; FD_ZERO(&fds); FD_SET(STDIN_FILENO, &fds);
        if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0) {
            int c2 = getchar();
            if (c2 == 'O') {
                int c3 = getchar();
                if (c3 == 'Q') { term_raw_off(); return KEY_F2; }  /* ESC O Q */
            } else if (c2 == '[') {
                int c3 = getchar();
                if (c3 == '1') {
                    int c4 = getchar();
                    if (c4 == '2') { getchar(); /* ~ */ term_raw_off(); return KEY_F2; }
                }
                /* Consuma altri byte della sequenza */
                if (c3 != '1') { /* sequenza non-F2, scartiamo */ }
            }
        }
        /* ESC solitario o sequenza non riconosciuta → passa come ESC */
    }
    term_raw_off();
    return c;
#endif
}

void clear_screen(void) {
#ifdef _WIN32
    system("cls");
#else
    printf("\033[2J\033[H"); fflush(stdout);
#endif
}
