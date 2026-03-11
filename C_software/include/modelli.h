/*
 * modelli.h — Selezione backend, modello, gestione .gguf
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void load_gguf_model(const char* filename);
void choose_backend(void);
void choose_model(void);
void gestisci_modelli(void);
void menu_modelli(void);

#ifdef __cplusplus
}
#endif
