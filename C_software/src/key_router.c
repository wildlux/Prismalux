/* ══════════════════════════════════════════════════════════════════════════
   key_router.c — routing semantico input → chiavi attive
   ══════════════════════════════════════════════════════════════════════════
   Il router analizza il testo in ingresso e restituisce l'insieme di chiavi
   semantiche rilevanti da usare per interrogare il context store su disco.

   Algoritmo:
     per ogni dominio → per ogni regola → se trigger ⊆ input (case-insensitive)
       → aggiungi le chiavi della regola all'output (deduplicato)

   Estendibilità: aggiungere una funzione kr_load_domain_XXX() sul modello
   di kr_load_domain_animali(), senza toccare il resto del codice.
   ══════════════════════════════════════════════════════════════════════════ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "key_router.h"

/* ══ Helper privati ══════════════════════════════════════════════════════════ */

/**
 * _str_lower() — copia src in dst convertendo in minuscolo.
 * Usata per il match case-insensitive senza modificare l'originale.
 */
static void _str_lower(char *dst, const char *src, int max) {
    int i;
    for (i = 0; src[i] && i < max - 1; i++)
        dst[i] = (char)tolower((unsigned char)src[i]);
    dst[i] = '\0';
}

/**
 * _contains_ci() — ricerca case-insensitive di needle in haystack.
 * Ritorna 1 se trovato, 0 altrimenti.
 */
static int _contains_ci(const char *haystack, const char *needle) {
    char h[1024], n[KR_KEYWORD_LEN];
    _str_lower(h, haystack, (int)sizeof(h));
    _str_lower(n, needle,   (int)sizeof(n));
    return strstr(h, n) != NULL;
}

/**
 * _find_or_create_domain() — cerca un dominio per nome o lo crea se assente.
 * Ritorna puntatore al dominio, NULL se il router è pieno.
 */
static KRDomain *_find_or_create_domain(KeyRouter *kr, const char *name) {
    for (int i = 0; i < kr->n_domains; i++)
        if (strcmp(kr->domains[i].name, name) == 0)
            return &kr->domains[i];
    if (kr->n_domains >= KR_MAX_DOMAINS) return NULL;
    KRDomain *d = &kr->domains[kr->n_domains++];
    memset(d, 0, sizeof(*d));
    strncpy(d->name, name, 31);
    return d;
}

/**
 * _key_present() — verifica se una chiave è già nell'output (dedup).
 */
static int _key_present(char out[][KR_KEY_LEN], int n, const char *key) {
    for (int i = 0; i < n; i++)
        if (strcmp(out[i], key) == 0) return 1;
    return 0;
}

/* ══ API pubblica ════════════════════════════════════════════════════════════ */

void kr_init(KeyRouter *kr) {
    memset(kr, 0, sizeof(*kr));
}

void kr_add_rule(KeyRouter *kr, const char *domain,
                 const char *trigger, const char **keys, int n_keys) {
    KRDomain *d = _find_or_create_domain(kr, domain);
    if (!d || d->n_rules >= KR_MAX_RULES) return;
    if (n_keys > KR_MAX_RULE_KEYS) n_keys = KR_MAX_RULE_KEYS;

    KRRule *r = &d->rules[d->n_rules++];
    memset(r, 0, sizeof(*r));
    strncpy(r->trigger, trigger, KR_KEYWORD_LEN - 1);
    for (int i = 0; i < n_keys; i++)
        strncpy(r->keys[i], keys[i], KR_KEY_LEN - 1);
    r->n_keys = n_keys;
}

int kr_route(const KeyRouter *kr, const char *input,
             char out_keys[][KR_KEY_LEN], int max_keys) {
    int n = 0;
    for (int d = 0; d < kr->n_domains; d++) {
        const KRDomain *dom = &kr->domains[d];
        for (int r = 0; r < dom->n_rules; r++) {
            const KRRule *rule = &dom->rules[r];
            if (!_contains_ci(input, rule->trigger)) continue;
            /* trigger trovato: aggiungi tutte le chiavi (dedup) */
            for (int k = 0; k < rule->n_keys && n < max_keys; k++) {
                if (!_key_present(out_keys, n, rule->keys[k]))
                    strncpy(out_keys[n++], rule->keys[k], KR_KEY_LEN - 1);
            }
        }
    }
    return n;
}

/* ══ Dominio ANIMALI ═════════════════════════════════════════════════════════
   Schema chiavi usate:
     Tassonomia:     "animale", "vertebrato", "invertebrato"
     Classi:         "uccello", "mammifero", "rettile", "pesce", "anfibio",
                     "insetto", "aracnide"
     Sottoclassi:    "rapace", "felino", "canide", "primate", "cetaceo",
                     "serpente", "coccodrillo", "squalo", "marino"
     Caratteristiche:"alimentazione", "habitat", "riproduzione",
                     "dimensioni", "comportamento", "difesa"
     Classificazione:"specie", "regno", "famiglia", "ordine"
   ══════════════════════════════════════════════════════════════════════════ */
void kr_load_domain_animali(KeyRouter *kr) {
    const char *d = "animali";

    /* ── Vertebrati (classe generica) ────────────────────────────────────── */
    { const char *k[] = {"animale","vertebrato"};
      kr_add_rule(kr, d, "vertebrato", k, 2); }
    { const char *k[] = {"animale","invertebrato"};
      kr_add_rule(kr, d, "invertebrato", k, 2); }

    /* ── Uccelli ─────────────────────────────────────────────────────────── */
    { const char *k[] = {"animale","uccello","vertebrato"};
      kr_add_rule(kr, d, "uccello", k, 3); }
    { const char *k[] = {"animale","uccello","rapace"};
      kr_add_rule(kr, d, "rapace", k, 3); }
    { const char *k[] = {"animale","uccello","rapace"};
      kr_add_rule(kr, d, "aquila", k, 3); }
    { const char *k[] = {"animale","uccello","rapace"};
      kr_add_rule(kr, d, "falco", k, 3); }
    { const char *k[] = {"animale","uccello","rapace"};
      kr_add_rule(kr, d, "gufo", k, 3); }
    { const char *k[] = {"animale","uccello","rapace"};
      kr_add_rule(kr, d, "nibbio", k, 3); }
    { const char *k[] = {"animale","uccello","marino"};
      kr_add_rule(kr, d, "gabbiano", k, 3); }
    { const char *k[] = {"animale","uccello","marino"};
      kr_add_rule(kr, d, "fenicottero", k, 3); }
    { const char *k[] = {"animale","uccello"};
      kr_add_rule(kr, d, "piccione", k, 2); }
    { const char *k[] = {"animale","uccello"};
      kr_add_rule(kr, d, "passero", k, 2); }
    { const char *k[] = {"animale","uccello"};
      kr_add_rule(kr, d, "rondine", k, 2); }

    /* ── Mammiferi ───────────────────────────────────────────────────────── */
    { const char *k[] = {"animale","mammifero","vertebrato"};
      kr_add_rule(kr, d, "mammifero", k, 3); }
    { const char *k[] = {"animale","mammifero","felino"};
      kr_add_rule(kr, d, "leone", k, 3); }
    { const char *k[] = {"animale","mammifero","felino"};
      kr_add_rule(kr, d, "tigre", k, 3); }
    { const char *k[] = {"animale","mammifero","felino"};
      kr_add_rule(kr, d, "leopardo", k, 3); }
    { const char *k[] = {"animale","mammifero","felino"};
      kr_add_rule(kr, d, "ghepardo", k, 3); }
    { const char *k[] = {"animale","mammifero","felino"};
      kr_add_rule(kr, d, "gatto", k, 3); }
    { const char *k[] = {"animale","mammifero","canide"};
      kr_add_rule(kr, d, "lupo", k, 3); }
    { const char *k[] = {"animale","mammifero","canide"};
      kr_add_rule(kr, d, "cane", k, 3); }
    { const char *k[] = {"animale","mammifero","canide"};
      kr_add_rule(kr, d, "volpe", k, 3); }
    { const char *k[] = {"animale","mammifero","primate"};
      kr_add_rule(kr, d, "scimmia", k, 3); }
    { const char *k[] = {"animale","mammifero","primate"};
      kr_add_rule(kr, d, "gorilla", k, 3); }
    { const char *k[] = {"animale","mammifero","primate"};
      kr_add_rule(kr, d, "scimpanze", k, 3); }
    { const char *k[] = {"animale","mammifero","cetaceo","marino"};
      kr_add_rule(kr, d, "balena", k, 4); }
    { const char *k[] = {"animale","mammifero","cetaceo","marino"};
      kr_add_rule(kr, d, "delfino", k, 4); }
    { const char *k[] = {"animale","mammifero"};
      kr_add_rule(kr, d, "elefante", k, 2); }
    { const char *k[] = {"animale","mammifero"};
      kr_add_rule(kr, d, "orso", k, 2); }

    /* ── Rettili ─────────────────────────────────────────────────────────── */
    { const char *k[] = {"animale","rettile","vertebrato"};
      kr_add_rule(kr, d, "rettile", k, 3); }
    { const char *k[] = {"animale","rettile","serpente"};
      kr_add_rule(kr, d, "serpente", k, 3); }
    { const char *k[] = {"animale","rettile","serpente"};
      kr_add_rule(kr, d, "cobra", k, 3); }
    { const char *k[] = {"animale","rettile","serpente"};
      kr_add_rule(kr, d, "pitone", k, 3); }
    { const char *k[] = {"animale","rettile","coccodrillo"};
      kr_add_rule(kr, d, "coccodrillo", k, 3); }
    { const char *k[] = {"animale","rettile","coccodrillo"};
      kr_add_rule(kr, d, "alligatore", k, 3); }
    { const char *k[] = {"animale","rettile"};
      kr_add_rule(kr, d, "lucertola", k, 2); }
    { const char *k[] = {"animale","rettile"};
      kr_add_rule(kr, d, "tartaruga", k, 2); }
    { const char *k[] = {"animale","rettile"};
      kr_add_rule(kr, d, "iguana", k, 2); }

    /* ── Pesci ───────────────────────────────────────────────────────────── */
    { const char *k[] = {"animale","pesce","vertebrato","marino"};
      kr_add_rule(kr, d, "squalo", k, 4); }
    { const char *k[] = {"animale","pesce","vertebrato"};
      kr_add_rule(kr, d, "salmone", k, 3); }
    { const char *k[] = {"animale","pesce","vertebrato"};
      kr_add_rule(kr, d, "tonno", k, 3); }
    { const char *k[] = {"animale","pesce","vertebrato"};
      kr_add_rule(kr, d, "pesce", k, 3); }

    /* ── Anfibi ──────────────────────────────────────────────────────────── */
    { const char *k[] = {"animale","anfibio","vertebrato"};
      kr_add_rule(kr, d, "rana", k, 3); }
    { const char *k[] = {"animale","anfibio","vertebrato"};
      kr_add_rule(kr, d, "rospo", k, 3); }
    { const char *k[] = {"animale","anfibio","vertebrato"};
      kr_add_rule(kr, d, "salamandra", k, 3); }

    /* ── Invertebrati ────────────────────────────────────────────────────── */
    { const char *k[] = {"animale","invertebrato","insetto"};
      kr_add_rule(kr, d, "farfalla", k, 3); }
    { const char *k[] = {"animale","invertebrato","insetto"};
      kr_add_rule(kr, d, "ape", k, 3); }
    { const char *k[] = {"animale","invertebrato","aracnide"};
      kr_add_rule(kr, d, "ragno", k, 3); }
    { const char *k[] = {"animale","invertebrato","aracnide"};
      kr_add_rule(kr, d, "scorpione", k, 3); }

    /* ── Trigger generici ────────────────────────────────────────────────── */
    { const char *k[] = {"animale"};
      kr_add_rule(kr, d, "animale", k, 1); }
    { const char *k[] = {"caratteristiche"};
      kr_add_rule(kr, d, "caratteristic", k, 1); }
    { const char *k[] = {"classificazione"};
      kr_add_rule(kr, d, "classif", k, 1); }

    /* ── Caratteristiche trasversali ─────────────────────────────────────── */
    { const char *k[] = {"caratteristiche","alimentazione"};
      kr_add_rule(kr, d, "mangia", k, 2); }
    { const char *k[] = {"caratteristiche","alimentazione"};
      kr_add_rule(kr, d, "nutre", k, 2); }
    { const char *k[] = {"caratteristiche","alimentazione"};
      kr_add_rule(kr, d, "cibo", k, 2); }
    { const char *k[] = {"caratteristiche","alimentazione"};
      kr_add_rule(kr, d, "preda", k, 2); }
    { const char *k[] = {"caratteristiche","alimentazione"};
      kr_add_rule(kr, d, "caccia", k, 2); }
    { const char *k[] = {"caratteristiche","habitat"};
      kr_add_rule(kr, d, "vive", k, 2); }
    { const char *k[] = {"caratteristiche","habitat"};
      kr_add_rule(kr, d, "abita", k, 2); }
    { const char *k[] = {"caratteristiche","habitat"};
      kr_add_rule(kr, d, "foresta", k, 2); }
    { const char *k[] = {"caratteristiche","habitat"};
      kr_add_rule(kr, d, "savana", k, 2); }
    { const char *k[] = {"caratteristiche","habitat"};
      kr_add_rule(kr, d, "oceano", k, 2); }
    { const char *k[] = {"caratteristiche","habitat"};
      kr_add_rule(kr, d, "montagna", k, 2); }
    { const char *k[] = {"caratteristiche","riproduzione"};
      kr_add_rule(kr, d, "uova", k, 2); }
    { const char *k[] = {"caratteristiche","riproduzione"};
      kr_add_rule(kr, d, "cuccioli", k, 2); }
    { const char *k[] = {"caratteristiche","riproduzione"};
      kr_add_rule(kr, d, "depone", k, 2); }
    { const char *k[] = {"caratteristiche","dimensioni"};
      kr_add_rule(kr, d, "grande", k, 2); }
    { const char *k[] = {"caratteristiche","dimensioni"};
      kr_add_rule(kr, d, "piccolo", k, 2); }
    { const char *k[] = {"caratteristiche","dimensioni"};
      kr_add_rule(kr, d, "peso", k, 2); }
    { const char *k[] = {"caratteristiche","comportamento"};
      kr_add_rule(kr, d, "migra", k, 2); }
    { const char *k[] = {"caratteristiche","comportamento"};
      kr_add_rule(kr, d, "notturno", k, 2); }
    { const char *k[] = {"caratteristiche","comportamento"};
      kr_add_rule(kr, d, "branco", k, 2); }
    { const char *k[] = {"caratteristiche","difesa"};
      kr_add_rule(kr, d, "velenoso", k, 2); }
    { const char *k[] = {"caratteristiche","difesa"};
      kr_add_rule(kr, d, "veleno", k, 2); }
    { const char *k[] = {"caratteristiche","difesa"};
      kr_add_rule(kr, d, "mimetismo", k, 2); }

    /* ── Classificazione tassonomica ─────────────────────────────────────── */
    { const char *k[] = {"classificazione","regno"};
      kr_add_rule(kr, d, "regno", k, 2); }
    { const char *k[] = {"classificazione","specie"};
      kr_add_rule(kr, d, "specie", k, 2); }
    { const char *k[] = {"classificazione","famiglia"};
      kr_add_rule(kr, d, "famiglia", k, 2); }
    { const char *k[] = {"classificazione","ordine"};
      kr_add_rule(kr, d, "ordine", k, 2); }
    { const char *k[] = {"classificazione","classe"};
      kr_add_rule(kr, d, "classe", k, 2); }
}
