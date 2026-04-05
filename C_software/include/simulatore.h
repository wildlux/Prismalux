/*
 * simulatore.h — Simulatore algoritmi step-by-step
 * ==================================================
 * Visualizza algoritmi di ordinamento, ricerca e matematici passo per passo
 * con barre ANSI colorate (identico concettualmente al simulatore.py Python).
 *
 * Legenda colori:
 *   GIALLO  = elementi sotto confronto
 *   ROSSO   = scambio in corso
 *   VERDE   = elemento nella posizione finale ordinata
 *   CIANO   = elemento trovato / target
 *   BIANCO  = elemento normale
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*
 * menu_simulatore — menu principale del simulatore algoritmi.
 * Presenta categorie (Base, Avanzati, Matematici) e permette di scegliere
 * un algoritmo da simulare step-by-step. Dopo la simulazione offre la
 * possibilità di fare domande all'AI sull'algoritmo appena visto.
 */
void menu_simulatore(void);

#ifdef __cplusplus
}
#endif
