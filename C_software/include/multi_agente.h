/*
 * multi_agente.h — Pipeline 6 agenti e Motore Byzantino
 * ======================================================
 * Espone i due punti di ingresso principali della funzionalità "Agenti AI":
 *
 * 1. menu_multi_agente(): pipeline sequenziale/parallela a 6 agenti
 *    che a partire da un task testuale produce codice Python ottimizzato.
 *
 * 2. menu_byzantine_engine(): sistema a 4 agenti per la verifica
 *    anti-allucinazione basata sulla logica booleana T = (A∧C) ∧ ¬B_valido.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*
 * menu_multi_agente — avvia la pipeline a 6 agenti per la generazione di codice.
 * Sequenza:
 *   [1] Ricercatore    — analisi tecnica del task
 *   [2] Pianificatore  — genera due approcci architetturali (A e B)
 *   [3] Programmatore A+B — implementano i due approcci in parallelo (thread)
 *   [4] Tester         — sceglie il codice migliore e avvia un loop di correzione
 *                         (max MA_MAX_TENTATIVI=3 tentativi)
 *   [5] Ottimizzatore  — applica type hints, list comprehension, docstring, ecc.
 * Al termine propone di salvare il codice ottimizzato in ../esportazioni/.
 */
void menu_multi_agente(void);

/*
 * menu_byzantine_engine — sistema di verifica a 4 agenti anti-allucinazione.
 * Pipeline:
 *   [A] Esperto Originale    — risponde alla query
 *   [B] Avvocato del Diavolo — cerca errori nella risposta di A
 *   [C] Gemello Indipendente — risponde alla stessa query senza vedere A
 *   [D] Giudice Quantico     — emette verdetto con logica T = (A∧C) ∧ ¬B_valido
 *
 * Logica del verdetto:
 *   T = 1.0 → VERIFICATO   (A e C concordano E B non ha trovato errori)
 *   T = 0.5 → INCERTO      (discordanza A/C oppure B ha sollevato obiezioni)
 */
void menu_byzantine_engine(void);

#ifdef __cplusplus
}
#endif
