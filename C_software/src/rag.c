/*
 * rag.c — Retrieval-Augmented Generation locale (senza dipendenze esterne)
 * =========================================================================
 * Algoritmo: TF-overlap scoring sui chunk dei file .txt in docs_dir.
 *
 * Passi:
 *   1. Apri la directory con opendir/readdir (POSIX)
 *   2. Per ogni .txt: leggi contenuto, spezza in chunk da RAG_CHUNK_CHARS
 *   3. Ogni chunk riceve un punteggio = parole query (len>3) trovate nel chunk
 *   4. Mantieni un heap minimo dei top-RAG_TOP_K chunk per punteggio
 *   5. Concatena i chunk selezionati in out_ctx
 *
 * Nessun indice persistente: i file sono piccoli (< 10 KB ciascuno),
 * la ricerca e' istantanea e i documenti possono essere modificati senza
 * dover ricostruire un indice.
 */
#include "rag.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>

/* ── Modalita' corrente (default AUTO) ──────────────────────────────── */
static int _rag_mode = RAG_MODE_AUTO;
int  rag_get_mode(void)     { return _rag_mode; }
void rag_set_mode(int mode)  { _rag_mode = mode; }

/* ── Struttura chunk candidato ───────────────────────────────────────── */
typedef struct {
    char  text[RAG_CHUNK_CHARS + 4];
    int   score;
} Chunk;

/* ── Helper: lowercopy ───────────────────────────────────────────────── */
static void _str_lower(const char *src, char *dst, int max) {
    int i = 0;
    for (; src[i] && i < max - 1; i++)
        dst[i] = (char)tolower((unsigned char)src[i]);
    dst[i] = '\0';
}

/* ── Tokenizza query: ritorna array di parole (len>3), n via puntatore ─ */
#define MAX_QWORDS 32
static int _tokenize_query(const char *query,
                            char words[MAX_QWORDS][64]) {
    int n = 0;
    char low[1024];
    _str_lower(query, low, sizeof low);
    char *tok = strtok(low, " \t\n\r,.;:!?()[]");
    while (tok && n < MAX_QWORDS) {
        if ((int)strlen(tok) > 3) {
            strncpy(words[n], tok, 63);
            words[n][63] = '\0';
            n++;
        }
        tok = strtok(NULL, " \t\n\r,.;:!?()[]");
    }
    return n;
}

/* ── Score di un chunk: conta parole query presenti ─────────────────── */
static int _score_chunk(const char *chunk_text,
                         char words[MAX_QWORDS][64], int nw) {
    char low[RAG_CHUNK_CHARS + 4];
    _str_lower(chunk_text, low, sizeof low);
    int score = 0;
    for (int i = 0; i < nw; i++)
        if (strstr(low, words[i])) score++;
    return score;
}

/* ── Spezza testo in chunk, aggiorna heap top-K ──────────────────────── */
static void _process_file(const char *path,
                           char words[MAX_QWORDS][64], int nw,
                           Chunk *heap, int *heap_n) {
    FILE *f = fopen(path, "r");
    if (!f) return;

    /* leggi intero file (max 16 KB per sicurezza) */
    char buf[16384];
    int  len = (int)fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    if (len <= 0) return;
    buf[len] = '\0';

    /* spezza in chunk rispettando i confini di parola */
    int pos = 0;
    while (pos < len) {
        int end = pos + RAG_CHUNK_CHARS;
        if (end >= len) {
            end = len;
        } else {
            /* arretra fino a spazio/newline per non troncare parole */
            while (end > pos && !isspace((unsigned char)buf[end])) end--;
            if (end == pos) end = pos + RAG_CHUNK_CHARS; /* parola lunghissima */
        }

        /* crea chunk */
        Chunk c;
        int clen = end - pos;
        if (clen <= 0) { pos++; continue; }
        if (clen > RAG_CHUNK_CHARS) clen = RAG_CHUNK_CHARS;
        memcpy(c.text, buf + pos, clen);
        c.text[clen] = '\0';
        c.score = _score_chunk(c.text, words, nw);

        /* aggiorna heap minimo top-K (inserzione semplice per K=3) */
        if (*heap_n < RAG_TOP_K) {
            heap[(*heap_n)++] = c;
        } else {
            /* trova il minimo nello heap */
            int min_i = 0;
            for (int i = 1; i < RAG_TOP_K; i++)
                if (heap[i].score < heap[min_i].score) min_i = i;
            if (c.score > heap[min_i].score)
                heap[min_i] = c;
        }
        pos = end;
    }
}

/* ── Ordinamento heap per punteggio decrescente (bubble su K=3) ───── */
static void _sort_heap(Chunk *heap, int n) {
    for (int i = 0; i < n - 1; i++)
        for (int j = i + 1; j < n; j++)
            if (heap[j].score > heap[i].score) {
                Chunk tmp = heap[i]; heap[i] = heap[j]; heap[j] = tmp;
            }
}

/* ══════════════════════════════════════════════════════════════════════
   API pubblica
   ══════════════════════════════════════════════════════════════════════ */

int rag_query(const char *docs_dir, const char *query,
              char *out_ctx, int out_max) {
    out_ctx[0] = '\0';
    if (!docs_dir || !query || !out_ctx || out_max < 8) return 0;

    /* tokenizza query */
    char words[MAX_QWORDS][64];
    int nw = _tokenize_query(query, words);
    if (nw == 0) return 0;

    /* apri directory */
    DIR *dir = opendir(docs_dir);
    if (!dir) return 0;

    Chunk heap[RAG_TOP_K];
    int   heap_n = 0;

    struct dirent *ent;
    char path[512];
    while ((ent = readdir(dir)) != NULL) {
        /* solo file .txt */
        const char *name = ent->d_name;
        int nlen = (int)strlen(name);
        if (nlen < 5) continue;
        if (strcmp(name + nlen - 4, ".txt") != 0) continue;

        snprintf(path, sizeof path, "%s/%s", docs_dir, name);
        _process_file(path, words, nw, heap, &heap_n);
    }
    closedir(dir);

    if (heap_n == 0) return 0;

    /* ordina per punteggio decrescente, scarta punteggio zero */
    _sort_heap(heap, heap_n);
    int written = 0;
    int pos = 0;
    for (int i = 0; i < heap_n && pos < out_max - 4; i++) {
        if (heap[i].score == 0) continue;
        int rem = out_max - pos - 2;
        int clen = (int)strlen(heap[i].text);
        if (clen > rem) clen = rem;
        memcpy(out_ctx + pos, heap[i].text, clen);
        pos += clen;
        out_ctx[pos++] = '\n';
        written++;
    }
    out_ctx[pos] = '\0';
    return written;
}

void rag_build_prompt(const char *base_sys, const char *rag_ctx,
                      char *out, int out_max) {
    if (!rag_ctx || rag_ctx[0] == '\0') {
        strncpy(out, base_sys, out_max - 1);
        out[out_max - 1] = '\0';
        return;
    }
    snprintf(out, out_max,
             "CONTESTO DOCUMENTALE (usa queste informazioni per rispondere):\n"
             "%s\n"
             "---\n"
             "%s",
             rag_ctx, base_sys);
}
