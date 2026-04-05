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
/* Salva lo stato originale del terminale per il ripristino (solo Linux/macOS) */
static struct termios g_orig_termios;
static int g_raw_active = 0; /* flag: 1 se siamo in modalità raw */

/* Ripristina il terminale allo stato originale; chiamata anche da atexit() */
void term_restore(void) {
    if (g_raw_active) { tcsetattr(STDIN_FILENO, TCSANOW, &g_orig_termios); g_raw_active=0; }
}

/* Attiva la modalità raw: disabilita ECHO e ICANON (no buffering di riga, no eco).
 * VMIN=1 VTIME=0: getchar() blocca fino a che arriva almeno 1 byte.
 * Registra term_restore() con atexit() la prima volta, così il terminale viene
 * sempre ripristinato anche in caso di uscita imprevista. */
static void term_raw_on(void) {
    if (!g_raw_active) { tcgetattr(STDIN_FILENO, &g_orig_termios); atexit(term_restore); g_raw_active=1; }
    struct termios raw = g_orig_termios;
    raw.c_lflag &= (tcflag_t)~(ECHO|ICANON);
    raw.c_cc[VMIN]=1; raw.c_cc[VTIME]=0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

/* Ripristina immediatamente la modalità normale (cooked) dopo la lettura */
static void term_raw_off(void) {
    if (g_raw_active) tcsetattr(STDIN_FILENO, TCSANOW, &g_orig_termios);
}
#endif

/* Legge un singolo tasto senza eco e senza aspettare INVIO */
int getch_single(void) {
#ifdef _WIN32
    return _getch();  /* conio.h — gestisce tutto nativamente su Windows */
#else
    term_raw_on();
    int c = getchar();
    term_raw_off();
    return c;
#endif
}

/* Come getch_single ma rileva anche F2 tramite sequenza di escape multi-byte.
 * I terminali Linux inviano F2 come tre o quattro byte: ESC O Q oppure ESC [ 1 2 ~.
 * Il timeout di 50ms su select() distingue un ESC solitario (premuto dall'utente)
 * da un ESC che inizia una sequenza di escape (emesso dal terminale come prefisso). */
int read_key_ext(void) {
#ifdef _WIN32
    int c = _getch();
    /* Su Windows i tasti speciali inviano prima un byte 0 o 0xE0 come prefisso */
    if (c == 0 || c == 0xE0) {
        int c2 = _getch();
        if (c2 == 60) return KEY_F2;  /* F2 su Windows = prefisso + 60 */
        return 0;
    }
    return c;
#else
    term_raw_on();
    int c = getchar();
    if (c == 0x1b) {
        /* Aspetta al massimo 50ms: se arrivano altri byte è una sequenza escape */
        struct timeval tv = {0, 50000};
        fd_set fds; FD_ZERO(&fds); FD_SET(STDIN_FILENO, &fds);
        if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0) {
            int c2 = getchar();
            if (c2 == 'O') {
                /* Sequenza xterm: ESC O Q = F2 */
                int c3 = getchar();
                if (c3 == 'Q') { term_raw_off(); return KEY_F2; }
            } else if (c2 == '[') {
                /* Sequenza VT100/Linux: ESC [ 1 2 ~ = F2 */
                int c3 = getchar();
                if (c3 == '1') {
                    int c4 = getchar();
                    if (c4 == '2') { getchar(); /* byte '~' finale */ term_raw_off(); return KEY_F2; }
                }
                /* Altre sequenze CSI (frecce, Ins, Del, ecc.) vengono scartate */
            }
        }
        /* ESC solitario (es. il tasto ESC del menu): lo restituiamo com'è (valore 27) */
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
