/* ══════════════════════════════════════════════════════════════════════════
   test_context_store.c — demo del Context Store Semantico su disco
   ══════════════════════════════════════════════════════════════════════════
   Simula una conversazione sugli animali in tre fasi:

   FASE 1 — Scrittura:
     Frasi "dette nel corso della conversazione" vengono salvate su disco
     con le loro chiavi semantiche (es. "aquila" → ["animale","uccello","rapace"])

   FASE 2 — Query semantica:
     Nuove domande vengono analizzate dal key router → chiavi attive →
     lettura diretta dal disco solo dei chunk rilevanti.

   FASE 3 — Verifica persistenza:
     Chiude e riapre il context store: l'indice viene riletto da .idx
     dimostrando che il contesto sopravvive tra sessioni (nessuna RAM).

   Build: make test_cs
   Esecuzione: ./test_cs
   File generati: /tmp/prismalux_ctx.dat  /tmp/prismalux_ctx.idx
   ══════════════════════════════════════════════════════════════════════════ */
#include <stdio.h>
#include <string.h>
#include "context_store.h"
#include "key_router.h"

/* ── Dati di conversazione di esempio ──────────────────────────────────────
   Ogni frase simula un "chunk" che durante una conversazione reale verrebbe
   estratto dall'output del modello e salvato sul disco con le chiavi giuste.   */
typedef struct { const char *testo; const char *keys[5]; int n_keys; } Chunk;

static const Chunk conversazione[] = {
    { "L'aquila reale (Aquila chrysaetos) e' il piu' grande rapace europeo, "
      "con un'apertura alare fino a 2,2 metri.",
      {"animale","uccello","rapace","dimensioni"}, 4 },

    { "L'aquila si nutre principalmente di lepri, conigli, marmotte e "
      "occasionalmente di carogne.",
      {"animale","uccello","alimentazione","rapace"}, 4 },

    { "L'aquila costruisce il nido (eyrie) su pareti rocciose ad alta quota; "
      "abita montagne e altipiani aperti.",
      {"animale","uccello","habitat","rapace"}, 4 },

    { "Il leone (Panthera leo) e' il piu' grande felino africano. "
      "Il maschio pesa tra 150 e 250 kg.",
      {"animale","mammifero","felino","dimensioni"}, 4 },

    { "Il leone vive nelle savane e nelle boscaglie dell'Africa subsahariana "
      "e di una piccola riserva in India (Gir).",
      {"animale","mammifero","felino","habitat"}, 4 },

    { "Il leone caccia in branco: le femmine coordinano l'attacco a gnu, "
      "zebre e bufali.",
      {"animale","mammifero","felino","alimentazione","comportamento"}, 5 },

    { "Il delfino comune (Delphinus delphis) e' un mammifero cetaceo. "
      "Respira aria attraverso lo sfiatatoio.",
      {"animale","mammifero","cetaceo","marino"}, 4 },

    { "I delfini comunicano con ultrasuoni (ecolocalizzazione) e vivono "
      "in gruppi sociali chiamati baccelli.",
      {"animale","mammifero","cetaceo","comportamento"}, 4 },

    { "Il serpente cobra reale (Ophiophagus hannah) e' il piu' lungo serpente "
      "velenoso del mondo, fino a 5,5 metri.",
      {"animale","rettile","serpente","difesa","dimensioni"}, 5 },

    { "Il cobra sputa il veleno con precisione fino a 2,5 metri di distanza "
      "per difendersi dai predatori.",
      {"animale","rettile","serpente","difesa","comportamento"}, 5 },

    { "L'aquila delle steppe (Aquila nipalensis) migra ogni anno dall'Asia "
      "centrale all'Africa orientale.",
      {"animale","uccello","rapace","comportamento"}, 4 },

    { "I rapaci notturni come il gufo reale (Bubo bubo) cacciano di notte "
      "grazie a un udito eccezionale.",
      {"animale","uccello","rapace","comportamento","alimentazione"}, 5 },
};

static void sep(void) {
    printf("\n\033[90m%s\033[0m\n",
           "──────────────────────────────────────────────────────────────");
}

int main(void) {
    ContextStore cs;
    KeyRouter    kr;

    printf("\033[1;36m=== Prototipo: Context Store Semantico su Disco ===\033[0m\n");
    printf("Dominio di test: Animali\n");
    printf("Store:           /tmp/prismalux_ctx.dat / .idx\n");

    /* ── Init ────────────────────────────────────────────────────────────── */
    if (cs_open(&cs, "/tmp/prismalux_ctx") != 0) {
        fprintf(stderr, "\033[31mErrore: impossibile aprire context store\033[0m\n");
        return 1;
    }
    kr_init(&kr);
    kr_load_domain_animali(&kr);

    /* ══ FASE 1: Scrittura su disco ══════════════════════════════════════════ */
    sep();
    printf("\033[1;33mFASE 1 — Scrittura contesto su disco\033[0m\n\n");

    int n_chunk = (int)(sizeof(conversazione) / sizeof(conversazione[0]));
    for (int i = 0; i < n_chunk; i++) {
        const Chunk *c = &conversazione[i];
        cs_write(&cs, c->keys, c->n_keys,
                 c->testo, (int)strlen(c->testo));

        printf("\033[32m[+]\033[0m \"%.*s%s\"\n",
               60, c->testo, strlen(c->testo) > 60 ? "..." : "");
        printf("    \033[90mchiavi:\033[0m");
        for (int k = 0; k < c->n_keys; k++)
            printf(" \033[33m\"%s\"\033[0m", c->keys[k]);
        printf("\n");
    }

    cs_print_index(&cs);

    /* ══ FASE 2: Query semantica ═════════════════════════════════════════════ */
    sep();
    printf("\033[1;33mFASE 2 — Query semantica (simulazione domande utente)\033[0m\n");
    printf("\033[90mIl key router analizza la domanda → chiavi attive → fseek disco\033[0m\n");

    const char *domande[] = {
        "Cosa mangia l'aquila?",
        "Dove vive il leone e come caccia?",
        "Il delfino e' un pesce o un mammifero?",
        "Dimmi qualcosa sui serpenti velenosi",
        "Quali animali migrano e sono notturni?",
        "Caratteristiche dei rapaci",
    };

    int n_domande = (int)(sizeof(domande) / sizeof(domande[0]));
    CSEntry  risultati[16];
    char     route_keys[KR_MAX_OUT_KEYS][KR_KEY_LEN];

    for (int q = 0; q < n_domande; q++) {
        sep();
        printf("\033[1mDOMANDA:\033[0m \"%s\"\n", domande[q]);

        /* routing: input → chiavi semantiche */
        memset(route_keys, 0, sizeof(route_keys));
        int nk = kr_route(&kr, domande[q], route_keys, KR_MAX_OUT_KEYS);

        printf("\033[36mKEY ROUTER\033[0m → %d chiavi attivate:", nk);
        for (int k = 0; k < nk; k++)
            printf(" \033[33m\"%s\"\033[0m", route_keys[k]);
        printf("\n");

        if (nk == 0) {
            printf("\033[31mNessuna chiave → domanda fuori dominio\033[0m\n");
            continue;
        }

        /* query disco: fseek diretto per ogni match */
        const char *qk[KR_MAX_OUT_KEYS];
        for (int k = 0; k < nk; k++) qk[k] = route_keys[k];
        int found = cs_query(&cs, qk, nk, risultati, 16);

        printf("\033[36mDISCO\033[0m → %d chunk rilevanti caricati in RAM:\n", found);
        for (int r = 0; r < found; r++) {
            printf("  \033[90m[%d]\033[0m %s\n", r + 1, risultati[r].data);
        }

        /* stima RAM risparmiata: tutti i chunk NON caricati */
        int non_caricati = cs.n_entries - found;
        printf("\033[90m    (%d/%d chunk lasciati su disco — non in RAM)\033[0m\n",
               non_caricati, cs.n_entries);
    }

    /* ══ FASE 3: Persistenza tra sessioni ═══════════════════════════════════ */
    sep();
    printf("\033[1;33mFASE 3 — Verifica persistenza (close + reopen)\033[0m\n\n");

    int entries_prima = cs.n_entries;
    cs_close(&cs);
    printf("Context store chiuso. File su disco:\n");
    printf("  \033[33m/tmp/prismalux_ctx.dat\033[0m  (dati grezzi)\n");
    printf("  \033[33m/tmp/prismalux_ctx.idx\033[0m  (indice binario)\n\n");

    /* riapre — simula una NUOVA sessione che riusa il contesto precedente */
    ContextStore cs2;
    cs_open(&cs2, "/tmp/prismalux_ctx");
    printf("Nuova sessione aperta. Entries caricate dall'indice: \033[1;32m%d/%d\033[0m\n",
           cs2.n_entries, entries_prima);

    /* query di verifica nella nuova sessione */
    const char *kv[] = {"mammifero","cetaceo"};
    int found2 = cs_query(&cs2, kv, 2, risultati, 4);
    printf("Query \"mammifero+cetaceo\" nella nuova sessione → %d chunk:\n", found2);
    for (int r = 0; r < found2; r++)
        printf("  \033[90m[%d]\033[0m %s\n", r + 1, risultati[r].data);

    cs_close(&cs2);

    /* ══ Riepilogo architettura ══════════════════════════════════════════════ */
    sep();
    printf("\033[1;36mRiepilogo architettura\033[0m\n\n");
    printf("  Contesto totale:    %d chunk\n", entries_prima);
    printf("  RAM usata (indice): ~%zu bytes  (solo metadati + offset)\n",
           (size_t)entries_prima * sizeof(CSIndexEntry));
    printf("  Dati su disco:      vedi /tmp/prismalux_ctx.dat\n");
    printf("  Lettura:            fseek() diretto — O(1) per chunk\n");
    printf("  Chiavi:             domain 'animali' — %d regole\n", 0);
    printf("\n");
    printf("  \033[90mProssimi passi:\033[0m\n");
    printf("  - Integrare in ai_chat_stream(): prima di ogni chiamata AI,\n");
    printf("    cs_query() carica solo i chunk rilevanti come system prompt\n");
    printf("  - Aggiungere domini: medicina, informatica, storia, chimica\n");
    printf("  - Eviction LRU per importance score\n");
    printf("  - Quantizzazione embeddings nel .dat (int8 invece di testo)\n\n");

    return 0;
}
