/*
 * prismalux_ui.h — Intestazione dinamica e primitive UI
 *
 * Includi questo file in qualsiasi .c per avere:
 *   - print_header()  : ridisegnata ogni volta (hardware aggiornato live)
 *   - box_*()         : box dinamico larghezza terminale
 *   - input_line()    : prompt ─── ❯ ─── a singola riga
 *
 * La funzione print_header() usa gli extern g_hw / g_hw_ready
 * definiti in prismalux.c — se viene incluso in altri file è sufficiente
 * che siano in fase di link.
 */
#pragma once

#include "hw_detect.h"
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#else
#  include <sys/ioctl.h>
#  include <unistd.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ── Versione ─────────────────────────────────────── */
#ifndef VERSION
#  define VERSION "2.1"
#endif

/* ── Colori ANSI ──────────────────────────────────── */
#ifndef RST
#  define RST  "\033[0m"
#  define BLD  "\033[1m"
#  define DIM  "\033[2m"
#  define CYN  "\033[96m"
#  define YLW  "\033[93m"
#  define GRN  "\033[92m"
#  define RED  "\033[91m"
#  define BLU  "\033[94m"
#  define MAG  "\033[95m"
#  define WHT  "\033[97m"
#  define GRY  "\033[90m"
#endif

/* ── Caratteri barra ──────────────────────────────── */
#ifndef BAR_FULL
#  define BAR_FULL  "\xe2\x96\x88"   /* █ */
#  define BAR_EMPTY "\xe2\x96\x91"   /* ░ */
#endif

/* ── Numeri keycap emoji ──────────────────────────── */
/* Formato: digit + U+FE0F + U+20E3 — display width 2 */
#ifndef EM_0
#  define EM_0 "\x30\xef\xb8\x8f\xe2\x83\xa3"
#  define EM_1 "\x31\xef\xb8\x8f\xe2\x83\xa3"
#  define EM_2 "\x32\xef\xb8\x8f\xe2\x83\xa3"
#  define EM_3 "\x33\xef\xb8\x8f\xe2\x83\xa3"
#  define EM_4 "\x34\xef\xb8\x8f\xe2\x83\xa3"
#  define EM_5 "\x35\xef\xb8\x8f\xe2\x83\xa3"
#  define EM_6 "\x36\xef\xb8\x8f\xe2\x83\xa3"
#  define EM_7 "\x37\xef\xb8\x8f\xe2\x83\xa3"
#  define EM_8 "\x38\xef\xb8\x8f\xe2\x83\xa3"
#  define EM_9 "\x39\xef\xb8\x8f\xe2\x83\xa3"
#endif

/* ── Risorse sistema ──────────────────────────────── */
typedef struct {
    double cpu_pct;
    double ram_used,  ram_total;
    double swp_used,  swp_total;   /* swap Linux / pagefile Windows */
    double dsk_used,  dsk_total;
    double vram_used, vram_total;  /* GPU primaria, GB (0 se N/D) */
    char   cpu_name[64];
    char   vram_name[64];          /* nome GPU breve */
} SysRes;

/* Globals definiti in prismalux.c */
extern HWInfo g_hw;
extern int    g_hw_ready;

/* Backend e modello corrente — aggiornati da prismalux.c prima di print_header() */
extern char g_hdr_backend[32];
extern char g_hdr_model[80];

/* Modalità headless: se != 0, print_header() salta la misurazione CPU (no nanosleep).
 * Usato dai test automatici per evitare il delay di 200ms per step. */
extern int g_sim_headless;

/* ── Prototipi ────────────────────────────────────── */

/* Larghezza terminale in colonne (live) */
int         term_width(void);

/* Lunghezza display di una stringa con ANSI e UTF-8 */
int         disp_len(const char *s);

/* Barra colorata pct% su len caratteri */
void        print_bar(double pct, int len);

/* Box drawing ─────────────────────────────────────── */
void        box_top(int W);
void        box_bot(int W);
void        box_sep(int W);
void        box_ctr(int W, const char *text);       /* centrato */
void        box_lft(int W, const char *text);       /* sinistra */
void        box_emp(int W);                          /* riga vuota */
void        box_res(int W, const char *lbl4,        /* riga risorsa */
                   const char *info, double pct, const char *warn);

/* Stringa avviso memoria (colore ANSI incluso) */
const char* mem_warn(double pct);

/* Lettura risorse hardware live */
SysRes      get_resources(void);

/* VRAM usata/totale in MB per la GPU indicata (lettura live) */
void        read_vram_mb(int gpu_index, DevType gpu_type,
                         long long *used_out, long long *total_out);

/* Intestazione dinamica — rilegge tutto ad ogni chiamata */
void        print_header(void);

/*
 * Prompt a singola riga stile:
 *   ──────────────────────────────────────
 *   ❯ [label]
 *   ──────────────────────────────────────
 *
 * label : testo facoltativo dopo ❯ (es. "Tu: ")
 * buf   : buffer output
 * sz    : dimensione buffer
 * Ritorna 1 = ok, 0 = EOF/errore
 */
int         input_line(const char *label, char *buf, int sz);

#ifdef __cplusplus
}
#endif
