/* ══════════════════════════════════════════════════════════════════════════
   key_router.h — Key Router semantico (input → chiavi attive)
   ══════════════════════════════════════════════════════════════════════════
   Il key router analizza un testo in ingresso e restituisce l'insieme di
   chiavi semantiche attivate. Ogni chiave corrisponde a un "dominio" o
   "categoria" che guida il caricamento dal context store su disco.

   Architettura a regole:
     - ogni dominio (es. "animali") contiene N regole
     - ogni regola ha un trigger (parola chiave nell'input) e K chiavi da
       attivare quando il trigger viene trovato
     - i domini sono estensibili: kr_add_rule() aggiunge regole a runtime
     - kr_load_domain_animali() carica il dominio pre-definito animali

   Estensione ad altri argomenti:
     kr_load_domain_medicina(kr)
     kr_load_domain_informatica(kr)
     kr_load_domain_storia(kr)
     ... (implementare analogamente a kr_load_domain_animali)
   ══════════════════════════════════════════════════════════════════════════ */
#pragma once

/* ── Costanti ─────────────────────────────────────────────────────────────── */
#define KR_MAX_RULES    128     /* regole max per dominio                      */
#define KR_MAX_DOMAINS  16      /* domini max nel router                       */
#define KR_KEYWORD_LEN  64      /* lunghezza max del trigger                   */
#define KR_KEY_LEN      32      /* lunghezza max di una chiave (incluso \0)    */
#define KR_MAX_OUT_KEYS 16      /* chiavi max restituite da kr_route()         */
#define KR_MAX_RULE_KEYS 8      /* chiavi max attivabili da una singola regola */

/* ── Strutture ────────────────────────────────────────────────────────────── */

/**
 * KRRule — una singola regola di routing.
 * Se trigger appare nell'input → attiva keys[0..n_keys-1].
 */
typedef struct {
    char trigger[KR_KEYWORD_LEN];           /* parola che scatta la regola    */
    char keys[KR_MAX_RULE_KEYS][KR_KEY_LEN];/* chiavi semantiche attivate     */
    int  n_keys;                             /* numero chiavi valide           */
} KRRule;

/**
 * KRDomain — insieme di regole per un dominio tematico.
 */
typedef struct {
    char   name[32];                /* nome dominio ("animali","medicina",...) */
    KRRule rules[KR_MAX_RULES];
    int    n_rules;
} KRDomain;

/**
 * KeyRouter — handle del router.
 * Contiene tutti i domini registrati.
 */
typedef struct {
    KRDomain domains[KR_MAX_DOMAINS];
    int      n_domains;
} KeyRouter;

/* ── API pubblica ─────────────────────────────────────────────────────────── */

/**
 * kr_init() — azzera il router (da chiamare prima di qualsiasi altra funzione).
 */
void kr_init(KeyRouter *kr);

/**
 * kr_add_rule() — aggiunge una regola a un dominio.
 * Se il dominio non esiste viene creato automaticamente.
 * domain   = nome dominio (es. "animali")
 * trigger  = parola trigger nell'input (es. "aquila")
 * keys[]   = chiavi semantiche da attivare
 * n_keys   = numero elementi in keys[]
 */
void kr_add_rule(KeyRouter *kr, const char *domain,
                 const char *trigger, const char **keys, int n_keys);

/**
 * kr_route() — analizza input e ritorna le chiavi attive (unione di tutte
 * le regole che hanno il trigger nell'input, deduplicato).
 * out_keys[][KR_KEY_LEN] = buffer di output
 * max_keys               = dimensione massima buffer
 * Ritorna numero di chiavi trovate.
 */
int  kr_route(const KeyRouter *kr, const char *input,
              char out_keys[][KR_KEY_LEN], int max_keys);

/* ── Domini predefiniti ───────────────────────────────────────────────────── */

/**
 * kr_load_domain_animali() — carica il dominio animali con regole predefinite.
 * Copre: uccelli, mammiferi, rettili, pesci, anfibi +
 *        caratteristiche trasversali (alimentazione, habitat, riproduzione,
 *        dimensioni, classificazione).
 */
void kr_load_domain_animali(KeyRouter *kr);

/* Stub per futuri domini — implementare analogamente: */
/* void kr_load_domain_medicina(KeyRouter *kr);        */
/* void kr_load_domain_informatica(KeyRouter *kr);     */
/* void kr_load_domain_storia(KeyRouter *kr);          */
/* void kr_load_domain_chimica(KeyRouter *kr);         */
