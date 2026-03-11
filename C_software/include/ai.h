/*
 * ai.h — Astrazione AI: ai_chat_stream, backend_ready
 */
#pragma once

#include "common.h"
#include "backend.h"

#ifdef __cplusplus
extern "C" {
#endif

void stream_cb(const char* piece, void* udata);

int  ai_chat_stream(const BackendCtx* ctx,
                    const char* sys_p, const char* usr,
                    lw_token_cb cb, void* ud,
                    char* out, int outmax);

int  backend_ready(const BackendCtx* ctx);
void ensure_ready_or_return(const BackendCtx* ctx);
int  check_risorse_ok(void);

#ifdef __cplusplus
}
#endif
