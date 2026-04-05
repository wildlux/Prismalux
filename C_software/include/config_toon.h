/*
 * config_toon.h — Parser/serializer TOON (Token-Oriented Object Notation)
 * =========================================================================
 * Implementa il sottoinsieme flat di TOON usato per la config di Prismalux.
 *
 * Formato TOON flat (chiave: valore, una per riga):
 *
 *   # Commento
 *   backend: ollama
 *   ollama_host: 127.0.0.1
 *   ollama_port: 11434
 *   ollama_model: qwen2.5-coder:7b
 *   gui_path: Qt_GUI/build/Prismalux_GUI
 *
 * Regole:
 *   - Righe che iniziano con '#' sono commenti (ignorate)
 *   - Split sul PRIMO ':' della riga → chiave : valore
 *   - Spazi prima/dopo chiave e valore vengono rimossi (trim)
 *   - I valori possono contenere ':' (es. host:porta, modelli Ollama)
 *   - Righe vuote ignorate
 *
 * Riferimento spec: https://github.com/toon-format/toon
 */
#pragma once
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * toon_get — cerca 'key' nel buffer TOON e copia il valore in 'out'.
 *
 * Ritorna out se trovato, NULL se la chiave non esiste.
 * Thread-safe: usa solo stack (nessun buffer statico).
 */
const char *toon_get(const char *buf, const char *key, char *out, int outmax);

/*
 * toon_get_int — come toon_get ma converte il valore in intero.
 * Ritorna def_val se la chiave non esiste o non e' un numero valido.
 */
int toon_get_int(const char *buf, const char *key, int def_val);

/*
 * toon_write_str — scrive "key: value\n" su file.
 */
void toon_write_str(FILE *f, const char *key, const char *value);

/*
 * toon_write_int — scrive "key: <numero>\n" su file.
 */
void toon_write_int(FILE *f, const char *key, int value);

/*
 * toon_write_comment — scrive "# commento\n" su file.
 */
void toon_write_comment(FILE *f, const char *comment);

#ifdef __cplusplus
}
#endif
