/*
 * modelli.h — Selezione backend, modello, gestione .gguf
 * ========================================================
 * Fornisce le funzioni di interfaccia utente per configurare il backend AI,
 * selezionare il modello e gestire i file .gguf locali (scarica, rimuovi).
 * load_gguf_model() include controlli preventivi su RAM e VRAM per evitare
 * di bloccare il sistema con modelli troppo grandi.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*
 * load_gguf_model — carica un file .gguf tramite llama.cpp statico.
 * Prima di caricare: verifica che il file non sia troppo piccolo (download incompleto),
 * stima la RAM necessaria per la quota CPU e la VRAM per la quota GPU,
 * e mostra avvisi o blocca il caricamento se le risorse sono insufficienti.
 * Se RAM/VRAM sono sotto soglia offre di scaricare il modello precedente o
 * di ridurre i layer GPU.
 * Parametri: filename = nome file .gguf relativo a g_models_dir (non path completo).
 */
void load_gguf_model(const char* filename);

/*
 * choose_backend — menu interattivo di selezione e configurazione backend.
 * Mostra le tre opzioni (llama/Ollama/llama-server), permette di personalizzare
 * host e porta per i backend HTTP, auto-carica il .gguf se ce n'è uno solo
 * in models/ quando si sceglie llama-statico. Salva la configurazione al termine.
 */
void choose_backend(void);

/*
 * choose_model — menu interattivo per selezionare il modello attivo.
 * Per llama-statico: elenca i file .gguf e chiama load_gguf_model().
 * Per HTTP backend: interroga il server (http_ollama_list / http_llamaserver_list)
 * e imposta g_ctx.model con il nome scelto. Permette anche inserimento manuale.
 */
void choose_model(void);

/*
 * gestisci_modelli — menu completo per i file .gguf (solo backend llama-statico).
 * Elenca i modelli con nome e dimensione, consente di:
 *   D — scaricare da una lista curata su Hugging Face o da URL personalizzato
 *   R — rimuovere un file .gguf (con conferma; scarica prima dalla memoria se attivo)
 */
void gestisci_modelli(void);

/*
 * menu_modelli — punto di ingresso unificato dalla Manutenzione.
 * Dispatcha a gestisci_modelli() per llama-statico,
 * oppure a choose_model() per i backend HTTP.
 */
void menu_modelli(void);

/*
 * scegli_modello_rapido — selettore compatto embedabile in qualsiasi schermata
 * di configurazione. Mostra la lista dei modelli disponibili, permette di
 * scegliere con un numero oppure premere INVIO per mantenere il corrente.
 * Non ha loop interno: si chiama una volta sola prima di avviare la chat.
 */
void scegli_modello_rapido(void);

#ifdef __cplusplus
}
#endif
