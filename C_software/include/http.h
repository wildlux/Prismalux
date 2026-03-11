/*
 * http.h — TCP/HTTP helpers, JSON helpers, Ollama/llama-server stream
 */
#pragma once

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── TCP helpers ────────────────────────────────────────────────── */
sock_t tcp_connect(const char* host, int port);
void   send_all(sock_t s, const char* buf, int len);
int    sock_getbyte(sock_t s);
int    sock_readline(sock_t s, char* buf, int maxlen);
void   sock_skip_headers(sock_t s);

/* ── JSON helpers ───────────────────────────────────────────────── */
const char* js_str(const char* json, const char* key);
void        js_encode(const char* src, char* dst, int dmax);

/* ── Backend HTTP stream ─────────────────────────────────────────── */
int http_ollama_stream(const char* host, int port, const char* model,
                       const char* sys_p, const char* usr,
                       lw_token_cb cb, void* ud, char* out, int outmax);

int http_llamaserver_stream(const char* host, int port, const char* model,
                            const char* sys_p, const char* usr,
                            lw_token_cb cb, void* ud, char* out, int outmax);

/* ── Lista modelli dal server ────────────────────────────────────── */
int http_ollama_list(const char* host, int port, char names[][128], int max);
int http_llamaserver_list(const char* host, int port, char names[][128], int max);

#ifdef __cplusplus
}
#endif
