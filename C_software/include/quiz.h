/*
 * quiz.h — Quiz Interattivi con utenti e punteggi
 * =================================================
 * Genera domande via AI, gestisce utenti con storico e classifica.
 *
 * DB domande:  ../esportazioni/quiz_db_<categoria>.txt
 * DB utenti:   ../esportazioni/quiz_utenti.json
 * Log sessioni:../esportazioni/quiz_sessioni.csv
 *
 * Categorie disponibili (compatibili con Python_Software):
 *   python_base, python_medio, python_avanzato, algoritmi,
 *   matematica, fisica, chimica, sicurezza, reti
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*
 * menu_quiz — entry point del sistema Quiz Interattivi.
 * Gestisce la selezione utente, l'esecuzione dei quiz,
 * la generazione domande via AI e la visualizzazione della classifica.
 */
void menu_quiz(void);

#ifdef __cplusplus
}
#endif
