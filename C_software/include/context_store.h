/* ══════════════════════════════════════════════════════════════════════════
   context_store.h — Context Store semantico su disco
   ══════════════════════════════════════════════════════════════════════════
   Idea: invece di tenere il contesto in RAM (KV cache cresce linearmente),
   ogni frammento di conversazione viene scritto su disco taggato con chiavi
   semantiche (es. "animale", "uccello", "alimentazione").

   Al momento del forward pass, un key router estrae le chiavi rilevanti
   dall'input corrente e carica DAL DISCO solo i chunk necessari.

   Formato su disco:
     base_path.dat  — dati grezzi (testo/token) concatenati
     base_path.idx  — indice binario in-memory, persistito a close()

   Lettura: fseek() diretto all'offset → O(1) per chunk, nessuna scansione.
   ══════════════════════════════════════════════════════════════════════════ */
#pragma once

#include <stdio.h>
#include <stdint.h>
#include <time.h>

/* ── Costanti ─────────────────────────────────────────────────────────────── */
#define CS_MAX_KEYS     8       /* chiavi semantiche max per entry             */
#define CS_KEY_LEN      32      /* lunghezza max di una chiave (incluso \0)    */
#define CS_MAX_ENTRIES  4096    /* entries max nell'indice in-memory           */
#define CS_DATA_MAX     2048    /* bytes max per singola entry (testo)         */

/* ── Strutture ────────────────────────────────────────────────────────────── */

/**
 * CSEntry — entry completa caricata in RAM dopo una query su disco.
 * Il campo data[] contiene il testo originale (null-terminated).
 */
typedef struct {
    char     keys[CS_MAX_KEYS][CS_KEY_LEN]; /* chiavi semantiche associate    */
    int      n_keys;                         /* numero chiavi valide           */
    char     data[CS_DATA_MAX];              /* testo/token (null-terminated)  */
    int      data_len;                       /* lunghezza dati in bytes        */
    float    importance;                     /* score per eviction (LRU)       */
    uint64_t timestamp;                      /* unix time di scrittura         */
} CSEntry;

/**
 * CSIndexEntry — record dell'indice in-memory.
 * Contiene solo metadati + offset per fseek(); i dati restano su disco.
 */
typedef struct {
    char     keys[CS_MAX_KEYS][CS_KEY_LEN];
    int      n_keys;
    uint64_t offset;    /* offset in .dat dove inizia il dato                 */
    uint32_t data_len;  /* lunghezza in bytes del dato                        */
    float    importance;
    uint64_t timestamp;
} CSIndexEntry;

/**
 * ContextStore — handle del context store.
 * Un'istanza per sessione di conversazione.
 */
typedef struct {
    FILE        *data_fp;                       /* file .dat aperto           */
    CSIndexEntry index[CS_MAX_ENTRIES];         /* indice in-memory           */
    int          n_entries;                     /* entries presenti           */
    char         path_dat[260];                 /* percorso file dati         */
    char         path_idx[260];                 /* percorso file indice       */
} ContextStore;

/* ── API pubblica ─────────────────────────────────────────────────────────── */

/**
 * cs_open() — apre (o crea) il context store.
 * Crea base_path.dat e base_path.idx se non esistono.
 * Se esistono, carica l'indice in-memory da .idx.
 * Ritorna 0 OK, -1 errore.
 */
int  cs_open(ContextStore *cs, const char *base_path);

/**
 * cs_close() — flush indice su .idx e chiude i file.
 * Da chiamare sempre prima di terminare.
 */
void cs_close(ContextStore *cs);

/**
 * cs_write() — scrive un frammento di testo con le sue chiavi semantiche.
 * keys[]    = array di stringhe (chiavi semantiche, es. "animale","uccello")
 * n_keys    = numero elementi in keys[]
 * data      = testo da persistere
 * data_len  = lunghezza in bytes (senza \0)
 * Ritorna 0 OK, -1 errore.
 */
int  cs_write(ContextStore *cs, const char **keys, int n_keys,
              const char *data, int data_len);

/**
 * cs_query() — cerca entries che hanno almeno UNA chiave in comune.
 * Lettura diretta (fseek) per ogni match trovato nell'indice.
 * out[]    = array di CSEntry da riempire
 * max_out  = dimensione massima di out[]
 * Ritorna il numero di entries trovate.
 */
int  cs_query(ContextStore *cs, const char **keys, int n_keys,
              CSEntry *out, int max_out);

/**
 * cs_print_index() — stampa l'indice corrente (debug).
 */
void cs_print_index(const ContextStore *cs);
