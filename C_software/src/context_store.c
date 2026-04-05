/* ══════════════════════════════════════════════════════════════════════════
   context_store.c — implementazione del context store semantico su disco
   ══════════════════════════════════════════════════════════════════════════
   Formato file .dat:  dati grezzi concatenati (no separatori, l'indice
                       tiene gli offset e le lunghezze)
   Formato file .idx:  int n_entries + array di CSIndexEntry (binario)

   Lettura: fseek(offset) + fread(data_len) → O(1) per singolo chunk.
   Scrittura: append a fine file + aggiornamento indice in-memory.
   Persistenza indice: _save_index() chiamata da cs_close().
   ══════════════════════════════════════════════════════════════════════════ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "context_store.h"

/* ══ Helper privati ══════════════════════════════════════════════════════════ */

/**
 * _score_match() — calcola quante chiavi l'entry condivide con la query.
 * Usato per ordinare i risultati per rilevanza (più chiavi in comune = più rilevante).
 * Ritorna 0 se nessuna chiave in comune (no match).
 */
static int _score_match(const CSIndexEntry *e, const char **keys, int n_keys) {
    int score = 0;
    for (int i = 0; i < n_keys; i++)
        for (int j = 0; j < e->n_keys; j++)
            if (strcmp(keys[i], e->keys[j]) == 0) { score++; break; }
    return score;
}

/**
 * _save_index() — scrive l'indice in-memory su .idx (formato binario).
 * Chiamata da cs_close().
 */
static void _save_index(const ContextStore *cs) {
    FILE *f = fopen(cs->path_idx, "wb");
    if (!f) return;
    fwrite(&cs->n_entries, sizeof(int), 1, f);
    fwrite(cs->index, sizeof(CSIndexEntry), (size_t)cs->n_entries, f);
    fclose(f);
}

/**
 * _load_index() — carica l'indice da .idx se esiste.
 * Se il file non esiste, parte con indice vuoto (nuova sessione).
 */
static void _load_index(ContextStore *cs) {
    FILE *f = fopen(cs->path_idx, "rb");
    if (!f) { cs->n_entries = 0; return; }
    if (fread(&cs->n_entries, sizeof(int), 1, f) != 1)
        cs->n_entries = 0;
    if (cs->n_entries < 0 || cs->n_entries > CS_MAX_ENTRIES)
        cs->n_entries = 0;
    if (cs->n_entries > 0 &&
        fread(cs->index, sizeof(CSIndexEntry), (size_t)cs->n_entries, f)
            != (size_t)cs->n_entries)
        cs->n_entries = 0;  /* file corrotto: resetta l'indice */
    fclose(f);
}

/* ══ API pubblica ════════════════════════════════════════════════════════════ */

int cs_open(ContextStore *cs, const char *base_path) {
    memset(cs, 0, sizeof(*cs));
    snprintf(cs->path_dat, sizeof(cs->path_dat), "%s.dat", base_path);
    snprintf(cs->path_idx, sizeof(cs->path_idx), "%s.idx", base_path);

    /* carica indice se sessione precedente esiste */
    _load_index(cs);

    /* apre il file dati in append+read binario */
    cs->data_fp = fopen(cs->path_dat, "a+b");
    if (!cs->data_fp) return -1;
    return 0;
}

void cs_close(ContextStore *cs) {
    if (cs->data_fp) {
        fclose(cs->data_fp);
        cs->data_fp = NULL;
    }
    _save_index(cs);
}

int cs_write(ContextStore *cs, const char **keys, int n_keys,
             const char *data, int data_len) {
    if (!cs->data_fp || cs->n_entries >= CS_MAX_ENTRIES) return -1;
    if (n_keys   > CS_MAX_KEYS) n_keys   = CS_MAX_KEYS;
    if (data_len > CS_DATA_MAX) data_len = CS_DATA_MAX;

    /* offset corrente = fine del file dati */
    fseek(cs->data_fp, 0, SEEK_END);
    uint64_t offset = (uint64_t)ftell(cs->data_fp);

    /* scrivi dati grezzi */
    fwrite(data, 1, (size_t)data_len, cs->data_fp);
    fflush(cs->data_fp);

    /* aggiorna indice in-memory */
    CSIndexEntry *e = &cs->index[cs->n_entries++];
    memset(e, 0, sizeof(*e));
    for (int i = 0; i < n_keys; i++)
        strncpy(e->keys[i], keys[i], CS_KEY_LEN - 1);
    e->n_keys    = n_keys;
    e->offset    = offset;
    e->data_len  = (uint32_t)data_len;
    e->importance = 1.0f;
    e->timestamp  = (uint64_t)time(NULL);

    return 0;
}

/* Soglia minima: almeno ceil(n_keys * 0.4) chiavi in comune, minimo 1.
   Con 5 chiavi serve almeno 2 match; con 2 chiavi basta 1 match. */
static int _min_score(int n_keys) {
    int t = (n_keys * 40 + 99) / 100;  /* ceil(n_keys * 0.4) */
    return t > 1 ? t : 1;
}

int cs_query(ContextStore *cs, const char **keys, int n_keys,
             CSEntry *out, int max_out) {
    /* raccoglie tutti i match con score ≥ soglia */
    int scores[CS_MAX_ENTRIES];
    int indices[CS_MAX_ENTRIES];
    int found = 0;
    int min_s = _min_score(n_keys);

    for (int i = 0; i < cs->n_entries; i++) {
        int s = _score_match(&cs->index[i], keys, n_keys);
        if (s < min_s) continue;
        scores[found]  = s;
        indices[found] = i;
        found++;
    }

    /* ordina per score decrescente (bubble sort — found è piccolo) */
    for (int a = 0; a < found - 1; a++)
        for (int b = a + 1; b < found; b++)
            if (scores[b] > scores[a]) {
                int ts = scores[a]; scores[a] = scores[b]; scores[b] = ts;
                int ti = indices[a]; indices[a] = indices[b]; indices[b] = ti;
            }

    if (found > max_out) found = max_out;
    int actual = found;
    found = 0;

    for (int fi = 0; fi < actual; fi++) {
        int i = indices[fi];

        CSEntry *o = &out[found];
        memset(o, 0, sizeof(*o));

        /* copia metadati dall'indice */
        memcpy(o->keys, cs->index[i].keys, sizeof(o->keys));
        o->n_keys    = cs->index[i].n_keys;
        o->importance = cs->index[i].importance;
        o->timestamp  = cs->index[i].timestamp;

        /* lettura diretta dal disco: fseek → fread */
        uint32_t len = cs->index[i].data_len;
        if (len >= CS_DATA_MAX) len = CS_DATA_MAX - 1;
        fseek(cs->data_fp, (long)cs->index[i].offset, SEEK_SET);
        o->data_len = (int)fread(o->data, 1, len, cs->data_fp);
        o->data[o->data_len] = '\0';

        found++;
    }
    return found; /* numero effettivo di entries caricate */
}

void cs_print_index(const ContextStore *cs) {
    printf("\n=== Context Store Index (%d entries) ===\n", cs->n_entries);
    for (int i = 0; i < cs->n_entries; i++) {
        const CSIndexEntry *e = &cs->index[i];
        printf("[%2d] chiavi:", i);
        for (int k = 0; k < e->n_keys; k++)
            printf(" \"%s\"", e->keys[k]);
        printf("  | %u bytes @ offset %llu\n",
               e->data_len, (unsigned long long)e->offset);
    }
}
