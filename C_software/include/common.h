/*
 * common.h — Include di piattaforma, costanti globali e stub llama
 * ================================================================
 * Includi questo file in ogni .c del progetto.
 *
 * "Costruito per i mortali che aspirano alla saggezza."
 */
#pragma once

/* ── Platform includes ──────────────────────────────────────────── */
#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <conio.h>
   typedef SOCKET sock_t;
#  define SOCK_INV INVALID_SOCKET
#  define sock_close(s) closesocket(s)
#else
#  include <unistd.h>
#  include <termios.h>
#  include <sys/ioctl.h>
#  include <sys/statvfs.h>
#  include <sys/select.h>
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <netdb.h>
   typedef int sock_t;
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
#define MAX_BUF      (512*1024)
#define MAX_MODELS   64
#define BAR_LEN      20
#define MODELS_DIR   "models"

/* ── llama.cpp statico — stub se non compilato con -DWITH_LLAMA_STATIC ── */
#ifdef WITH_LLAMA_STATIC
#  include "llama_wrapper.h"
#else
static inline int         lw_is_loaded(void)                                        { return 0; }
static inline const char* lw_model_name(void)                                       { return "(llama non compilato)"; }
static inline int         lw_init(const char* p, int g, int c)                      { (void)p;(void)g;(void)c; return -1; }
static inline void        lw_free(void)                                              {}
typedef void (*lw_token_cb)(const char*, void*);
static inline int         lw_chat(const char* s, const char* u, lw_token_cb cb,
                                   void* ud, char* o, int m)
                           { (void)s;(void)u;(void)cb;(void)ud;(void)o;(void)m; return -1; }
static inline int         lw_list_models(const char* d, char n[][256], int mx)
                           { (void)d;(void)n;(void)mx; return 0; }
#endif
