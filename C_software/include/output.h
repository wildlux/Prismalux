/*
 * output.h — Salvataggio output su file
 * =======================================
 * Espone salva_output() usata dalla pipeline multi-agente e dal Motore Byzantino
 * per esportare i risultati in un file di testo nella cartella ../esportazioni/.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*
 * salva_output — salva il risultato di una sessione AI su file di testo.
 * Tenta di scrivere in ../esportazioni/, poi in esportazioni/, poi in ./ .
 * Il nome file è: multiagente_YYYYMMDD_HHMMSS.txt
 * Il file include: timestamp, task, backend, modello, e il testo output.
 * Parametri:
 *   task   — descrizione del task eseguito (usata come titolo nel file)
 *   output — testo da salvare (es. codice finale ottimizzato)
 */
void salva_output(const char* task, const char* output);

#ifdef __cplusplus
}
#endif
