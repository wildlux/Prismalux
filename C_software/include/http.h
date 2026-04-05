/*
 * http.h — TCP/HTTP helpers, JSON helpers, Ollama/llama-server stream
 * ====================================================================
 * Tutto il codice di rete usa socket TCP raw senza librerie esterne.
 * Il parsing JSON è minimale: solo estrazione di stringhe (js_str) e
 * escaping per embedding in JSON (js_encode).
 * I due stream HTTP implementano rispettivamente:
 *   - Ollama: formato NDJSON (una riga JSON per token) via POST /api/chat
 *   - llama-server: formato SSE OpenAI (righe "data: {...}") via POST /v1/chat/completions
 */
#pragma once

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── TCP helpers ────────────────────────────────────────────────── */

/*
 * tcp_connect — apre una connessione TCP a host:port.
 * Su Windows inizializza Winsock (WSAStartup) alla prima chiamata.
 * Usa gethostbyname per risolvere il nome host (supporta sia IP che hostname).
 * Ritorna: socket valido, oppure SOCK_INV se la connessione fallisce.
 */
sock_t tcp_connect(const char* host, int port);

/*
 * send_all — invia tutti i byte di buf (lunghezza len) sul socket s,
 * ripetendo send() fino a che tutti i dati sono stati trasmessi.
 */
void   send_all(sock_t s, const char* buf, int len);

/*
 * sock_getbyte — legge un singolo byte dal socket s.
 * Ritorna: il byte letto (0-255), oppure -1 se la connessione è chiusa.
 */
int    sock_getbyte(sock_t s);

/*
 * sock_readline — legge caratteri dal socket finché trova '\n' o EOF.
 * Scarta '\r', termina la stringa con '\0'.
 * Parametri: s=socket, buf=buffer output, maxlen=dimensione buffer.
 * Ritorna: numero di caratteri letti (senza '\n').
 */
int    sock_readline(sock_t s, char* buf, int maxlen);

/*
 * sock_skip_headers — scarta tutte le intestazioni HTTP/1.x
 * leggendo righe fino alla riga vuota che separa header dal body.
 */
void   sock_skip_headers(sock_t s);

/* ── JSON helpers ───────────────────────────────────────────────── */

/*
 * js_str — estrae il valore stringa di una chiave JSON.
 * Cerca "key":"..." nel testo JSON e restituisce il contenuto decodificato
 * (gestisce \n, \t, \", \\). Usa un buffer interno statico (non thread-safe).
 * Parametri: json=testo JSON, key=nome chiave da cercare.
 * Ritorna: puntatore al buffer interno con il valore, oppure NULL se non trovato.
 */
const char* js_str(const char* json, const char* key);

/*
 * js_encode — codifica una stringa per inclusione sicura in un valore JSON.
 * Esegue l'escaping di ", \, \n, \r.
 * Parametri: src=stringa sorgente, dst=buffer destinazione, dmax=dimensione dst.
 */
void        js_encode(const char* src, char* dst, int dmax);

/*
 * prompt_sanitize — rimuove pattern di prompt injection dal testo in-place.
 * Neutralizza format-token ChatML/Llama2, separatori di sezione, role-override.
 * Da chiamare DOPO js_encode, prima di inviare il testo al modello.
 * Parametri: buf=buffer da sanificare (modificato sul posto), bufmax=dimensione.
 * Sicuro: O(N), no regex, no allocazioni.
 */
void        prompt_sanitize(char* buf, int bufmax);

/* ── Backend HTTP stream ─────────────────────────────────────────── */

/*
 * http_ollama_stream — invia una richiesta chat a Ollama (POST /api/chat)
 * con stream:true e legge la risposta NDJSON riga per riga.
 * Per ogni token nel campo "message".content chiama cb(token, ud).
 * Accumula la risposta completa in out (fino a outmax byte).
 * Parametri: host/port=indirizzo server, model=nome modello,
 *            sys_p=system prompt, usr=messaggio utente,
 *            cb=callback token (può essere NULL), ud=userdata per cb,
 *            out=buffer output (può essere NULL), outmax=dimensione out.
 *            keep_alive_secs: secondi di keep-alive del modello in RAM dopo la risposta.
 *              -1 = lascia decidere a Ollama (default 5 min)
 *               0 = scarica subito il modello dopo la risposta (risparmia RAM)
 *              >0 = mantieni caricato per N secondi
 * Ritorna: 0 se OK, -1 se errore di connessione o memoria.
 */
int http_ollama_stream(const char* host, int port, const char* model,
                       const char* sys_p, const char* usr,
                       lw_token_cb cb, void* ud, char* out, int outmax,
                       int keep_alive_secs);

/*
 * http_llamaserver_stream — invia una richiesta chat a llama-server
 * (POST /v1/chat/completions, API OpenAI) con stream:true e legge
 * la risposta nel formato SSE ("data: {...}" righe, "data: [DONE]" finale).
 * Per ogni token in choices[0].delta.content chiama cb(token, ud).
 * Parametri e valore di ritorno identici a http_ollama_stream.
 */
int http_llamaserver_stream(const char* host, int port, const char* model,
                            const char* sys_p, const char* usr,
                            lw_token_cb cb, void* ud, char* out, int outmax);

/* ── Embedding Ollama ─────────────────────────────────────────────── */

/*
 * http_ollama_embed — genera un vettore embedding tramite Ollama.
 *
 * Invia POST /api/embeddings {"model":"<m>","prompt":"<text>"} e parsa
 * l'array float restituito nel campo "embedding":[...].
 * Non usa streaming: legge la risposta completa in un buffer.
 *
 * Parametri:
 *   host/port    — indirizzo Ollama (localhost:11434)
 *   embed_model  — modello embedding (es. "nomic-embed-text")
 *   text         — testo da embeddare
 *   vec          — array float di output (allocato dal chiamante)
 *   out_dim      — dimensione effettiva del vettore prodotto
 *   max_dim      — capacita' massima di vec
 *
 * Ritorna: 0 se OK, -1 se errore (connessione, JSON malformato,
 *          modello non embedding-capable).
 */
int http_ollama_embed(const char *host, int port, const char *embed_model,
                      const char *text, float *vec, int *out_dim, int max_dim);

/* ── Lista modelli dal server ────────────────────────────────────── */

/*
 * http_ollama_list — interroga GET /api/tags e restituisce i nomi modelli.
 * Parametri: host/port=server, names=array output (max righe), max=dimensione array.
 * Ritorna: numero di modelli trovati (0 se server offline o nessun modello).
 */
int http_ollama_list(const char* host, int port, char names[][128], int max);

/*
 * http_llamaserver_list — interroga GET /v1/models e restituisce gli id modelli.
 * Parametri e valore di ritorno identici a http_ollama_list.
 */
int http_llamaserver_list(const char* host, int port, char names[][128], int max);

#ifdef __cplusplus
}
#endif
