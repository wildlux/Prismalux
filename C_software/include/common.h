/*
 * common.h — Include di piattaforma, costanti globali e stub llama
 * ================================================================
 * Includi questo file in ogni .c del progetto.
 * Gestisce le differenze tra Windows e Linux/macOS in un unico punto,
 * definisce le costanti condivise e fornisce gli stub no-op per
 * llama.cpp quando il binario non viene compilato con -DWITH_LLAMA_STATIC.
 *
 * "Costruito per i mortali che aspirano alla saggezza."
 */
#pragma once

/* ── Platform includes ──────────────────────────────────────────── */
/*
 * Su Windows vengono incluse le API Winsock2 per i socket TCP,
 * conio.h per la lettura di un singolo tasto (_getch),
 * e sock_t è mappato su SOCKET (HANDLE intero a 64 bit su Windows).
 * Su Linux/macOS si usano i socket POSIX standard e sock_t è int.
 * SOCK_INV e sock_close() astraggono la differenza tra i due sistemi
 * evitando #ifdef sparsi nel codice.
 */
/* ── Pragma diagnostici globali ─────────────────────────────────────────────
   Tutti i pattern coinvolti sono intenzionali e sicuri in questo progetto:
   - strncpy/snprintf con troncamento deliberato (buffer fisso, null-term garantito)
   - \xNN > 0x7F in string literal (UTF-8 multibyte, non char singolo)
   - misleading-indentation in macro one-liner (STAMPA_BIT, STAMPA_GRIGLIA, ecc.)
   ─────────────────────────────────────────────────────────────────────────── */
#pragma GCC diagnostic ignored "-Wstringop-truncation"
#pragma GCC diagnostic ignored "-Wformat-truncation"
#pragma GCC diagnostic ignored "-Woverflow"
#pragma GCC diagnostic ignored "-Wmisleading-indentation"

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <conio.h>
#  include <io.h>             /* _access() per verifica file esistente */
   typedef SOCKET sock_t;        /* tipo socket su Windows */
#  define SOCK_INV INVALID_SOCKET /* valore sentinella "socket non valido" */
#  define sock_close(s) closesocket(s)
#else
#  include <unistd.h>
#  include <termios.h>           /* tcgetattr/tcsetattr per modalità raw */
#  include <sys/ioctl.h>         /* TIOCGWINSZ per larghezza terminale */
#  include <sys/statvfs.h>       /* statvfs per spazio disco */
#  include <sys/select.h>        /* select() per timeout su stdin */
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <netdb.h>             /* getaddrinfo/gethostbyname */
#  include <fcntl.h>            /* fcntl, O_NONBLOCK */
   typedef int sock_t;            /* tipo socket su Linux/macOS */
#  define SOCK_INV (-1)
#  define sock_close(s) close(s)
#endif

/* ── Standard C ─────────────────────────────────────────────────── */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <dirent.h>

/* ── Costanti globali ───────────────────────────────────────────── */
#define MAX_BUF      (512*1024)  /* dimensione massima buffer risposta AI (~512 KB) */
#define MAX_MODELS   64          /* numero massimo di modelli elencabili */
#define BAR_LEN      20          /* larghezza barre progresso nell'header */
#define MODELS_DIR   "models"    /* directory predefinita per i file .gguf */

/* ── llama.cpp statico — stub se non compilato con -DWITH_LLAMA_STATIC ── */
/*
 * Quando si compila senza ./build.sh (cioè senza -DWITH_LLAMA_STATIC),
 * le funzioni lw_* vengono sostituite da stub inline no-op che:
 *   - restituiscono sempre -1 o 0 (errore/assenza)
 *   - non chiamano librerie esterne
 * Questo permette di compilare il progetto con solo gcc e senza llama.cpp,
 * usando esclusivamente i backend HTTP (Ollama o llama-server).
 *
 * Con -DWITH_LLAMA_STATIC viene incluso llama_wrapper.h che espone
 * le vere implementazioni C++ wrappers della libreria llama.cpp.
 */
#ifdef WITH_LLAMA_STATIC
#  include "llama_wrapper.h"
#else
/* Stub: indica se un modello .gguf è attualmente in memoria */
static inline int         lw_is_loaded(void)                                        { return 0; }
/* Stub: restituisce il nome del modello caricato (nessuno in questo caso) */
static inline const char* lw_model_name(void)                                       { return "(llama non compilato)"; }
/* Stub: inizializza il modello .gguf (path, gpu_layers, ctx_size); restituisce -1 */
static inline int         lw_init(const char* p, int g, int c)                      { (void)p;(void)g;(void)c; return -1; }
/* Stub: scarica il modello dalla memoria */
static inline void        lw_free(void)                                              {}
/* Tipo callback per i token prodotti in streaming */
typedef void (*lw_token_cb)(const char*, void*);
/* Stub: esegue inferenza con sistema+utente, chiama cb per ogni token; restituisce -1 */
static inline int         lw_chat(const char* s, const char* u, lw_token_cb cb,
                                   void* ud, char* o, int m)
                           { (void)s;(void)u;(void)cb;(void)ud;(void)o;(void)m; return -1; }
/* Stub: elenca i file .gguf in una directory; restituisce 0 */
static inline int         lw_list_models(const char* d, char n[][256], int mx)
                           { (void)d;(void)n;(void)mx; return 0; }
/* Stub: KV-cache advisor JLT — senza llama.cpp statico ritorna default_ctx */
#include "jlt_index.h"
static inline int         lw_jlt_advised_ctx(const JltIndex* idx, int def,
                                              int s, int u, int h)
                           { (void)idx;(void)s;(void)u;(void)h; return def; }
#endif
