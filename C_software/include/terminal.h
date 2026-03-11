/*
 * terminal.h — Utilities terminale: getch, read_key, clear_screen
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define KEY_F2  0xF002

int  getch_single(void);
int  read_key_ext(void);
void clear_screen(void);

#ifndef _WIN32
void term_restore(void);
#endif

#ifdef __cplusplus
}
#endif
