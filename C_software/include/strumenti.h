/*
 * strumenti.h — Tutor AI e Strumento Pratico
 * ============================================
 * Espone le due voci del menu "Impara con AI" e "Strumento Pratico".
 * Tutti gli strumenti sono loop interattivi che usano ai_chat_stream()
 * con system prompt specializzati per rispondere in italiano.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*
 * menu_tutor — tutor AI interattivo in modalità "Oracolo di Prismalux".
 * Loop domanda/risposta: l'utente digita una domanda, l'AI risponde
 * con system prompt da tutor esperto e paziente. Esce con 0/vuoto/esci/q.
 */
void menu_tutor(void);

/*
 * menu_agenti_ruoli — selezione di 50 agenti AI specializzati con system prompt dedicati.
 * Categorie: pipeline base, matematica, informatica, sicurezza, fisica, chimica,
 * informatica estrema, sicurezza estrema, fisica estrema, chimica estrema, ingegneria estrema.
 * Ogni agente apre un loop di chat con il proprio system prompt.
 * Navigazione con [N]/[P] per le pagine (10 ruoli per pagina) e numero per selezionare.
 */
void menu_agenti_ruoli(void);

/*
 * menu_modalita_agenti — menu categorie: scegli prima il dominio (Matematica,
 * Informatica, Sicurezza, Fisica, Chimica, Trasversale, Pipeline), poi il ruolo
 * specifico all'interno di quella categoria. L'LLM riceve il system prompt già
 * ottimizzato per quel dominio senza dover scorrere tutti i 50 ruoli.
 */
void menu_modalita_agenti(void);

/*
 * menu_pratico — sottomenu dello Strumento Pratico con due assistenti AI:
 *   1 — Dichiarazione 730: fiscalista specializzato su deduzioni/detrazioni
 *   2 — Partita IVA & Forfettario: consulente su regime forfettario, INPS, fatturazione
 * Entrambi sono loop interattivi con system prompt specifici.
 */
void menu_pratico(void);

#ifdef __cplusplus
}
#endif
