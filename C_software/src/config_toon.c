/*
 * config_toon.c — Parser/serializer TOON flat per la config di Prismalux
 * ========================================================================
 * Implementa il sottoinsieme flat di TOON (Token-Oriented Object Notation).
 * Nessuna dipendenza esterna: solo stdlib C standard.
 *
 * Algoritmo di parsing: strstr() con pattern "\nkey: " — stesso approccio
 * del parser JSON (js_str), sfrutta l'accelerazione SIMD di glibc.
 * Risultato: velocita' comparabile a JSON per file flat.
 *
 * Riferimento spec: https://github.com/toon-format/toon
 */
#include "config_toon.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* ── toon_get — strstr SIMD come js_str ──────────────────────────────── */
/*
 * Costruisce il pattern "\nkey: " e usa strstr() per trovarlo nel buffer.
 * Gestisce anche il caso in cui la chiave sia alla prima riga (no \n iniziale).
 *
 * Sicurezza contro falsi positivi:
 *   - Il \n prima della chiave impedisce match parziali (es. "ollama_model: "
 *     non matcha mai "\nmodel: ").
 *   - I commenti iniziano con '#': "\n# key: " non matcha "\nkey: ". ✓
 *   - I valori non contengono \n (formato flat): nessun match spurio. ✓
 */
const char *toon_get(const char *buf, const char *key,
                     char *out, int outmax) {
    if (!buf || !key || !out || outmax < 1) return NULL;
    out[0] = '\0';

    /* Pattern con newline: "\nkey: " — ancora di riga, blocca match parziali */
    char pat[132];
    snprintf(pat, sizeof pat, "\n%s: ", key);

    const char *p = strstr(buf, pat);

    /* Fallback: chiave alla prima riga (buffer non inizia con \n) */
    if (!p) {
        int klen = (int)strlen(key);
        /* controlla se il buffer inizia esattamente con "key: " */
        if (strncmp(buf, key, klen) == 0 && buf[klen] == ':' && buf[klen+1] == ' ')
            p = buf - 1; /* simuliamo \n prima per uniformare il calcolo offset */
        else
            return NULL;
    }

    /* Salta "\nkey: " per arrivare al valore */
    const char *vstart = p + strlen(pat);

    /* Leggi fino a fine riga */
    int i = 0;
    while (vstart[i] && vstart[i] != '\n' && vstart[i] != '\r' && i < outmax - 1) {
        out[i] = vstart[i]; i++;   /* separato per evitare UB sequence-point */
    }
    out[i] = '\0';

    /* Trim destro (spazi finali) */
    while (i > 0 && (out[i-1] == ' ' || out[i-1] == '\t')) out[--i] = '\0';

    return out;
}

/* ── toon_get_int ────────────────────────────────────────────────────── */
int toon_get_int(const char *buf, const char *key, int def_val) {
    char tmp[32];
    if (!toon_get(buf, key, tmp, sizeof tmp)) return def_val;
    if (!tmp[0]) return def_val;
    char *end;
    long v = strtol(tmp, &end, 10);
    if (end == tmp) return def_val;
    return (int)v;
}

/* ── toon_write_str ──────────────────────────────────────────────────── */
void toon_write_str(FILE *f, const char *key, const char *value) {
    fprintf(f, "%s: %s\n", key, value ? value : "");
}

/* ── toon_write_int ──────────────────────────────────────────────────── */
void toon_write_int(FILE *f, const char *key, int value) {
    fprintf(f, "%s: %d\n", key, value);
}

/* ── toon_write_comment ──────────────────────────────────────────────── */
void toon_write_comment(FILE *f, const char *comment) {
    fprintf(f, "# %s\n", comment ? comment : "");
}
