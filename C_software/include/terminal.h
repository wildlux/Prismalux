/*
 * terminal.h — Utilities terminale: getch, read_key, clear_screen
 * =================================================================
 * Astrae le differenze Windows/Linux per lettura singolo tasto,
 * rilevamento tasti funzione (F2) e pulizia schermo.
 * Su Linux gestisce la modalità raw del terminale per leggere
 * un tasto senza attendere INVIO.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Valore sentinella restituito da read_key_ext() quando viene premuto F2 */
#define KEY_F2  0xF002

/*
 * getch_single — legge un singolo carattere senza eco e senza attendere INVIO.
 * Su Windows usa _getch() da conio.h.
 * Su Linux attiva temporaneamente la modalità raw (no echo, no canonical),
 * legge un byte da stdin, poi ripristina il terminale.
 * Ritorna: il codice ASCII del tasto premuto.
 */
int  getch_single(void);

/*
 * read_key_ext — come getch_single ma riconosce anche i tasti funzione.
 * Su Linux usa select() con timeout di 50ms per rilevare le sequenze
 * di escape multicarattere emesse dai tasti F1-F12 (es. ESC O Q per F2).
 * Su Windows usa il secondo byte restituito da _getch() per i tasti speciali.
 * Ritorna: il codice ASCII del tasto, oppure KEY_F2 (0xF002) se viene premuto F2.
 */
int  read_key_ext(void);

/*
 * clear_screen — pulisce il terminale.
 * Su Windows esegue il comando di sistema "cls".
 * Su Linux/macOS invia la sequenza ANSI ESC[2J ESC[H (più rapido di system("clear")).
 */
void clear_screen(void);

/*
 * term_restore — ripristina il terminale allo stato originale (solo Linux/macOS).
 * Viene registrata con atexit() la prima volta che si attiva la modalità raw,
 * così viene chiamata automaticamente all'uscita del programma.
 */
#ifndef _WIN32
void term_restore(void);
#endif

#ifdef __cplusplus
}
#endif
