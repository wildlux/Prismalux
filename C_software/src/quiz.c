/*
 * quiz.c — Quiz Interattivi con utenti e punteggi
 * =================================================
 * Genera domande via AI, esegue quiz a scelta multipla,
 * gestisce profili utente con storico e classifica.
 *
 * Flusso:
 *   1. Seleziona o crea un utente
 *   2. Scegli categoria e numero domande
 *   3. Esegui il quiz (A/B/C/D)
 *   4. Risultato salvato nel profilo utente + log CSV
 *   5. Classifica globale ordinata per media %
 *
 * Generazione domande AI:
 *   - Usa ai_chat_stream() con prompt a formato strutturato (non JSON)
 *   - Formato: Q:/A:/B:/C:/D:/R:/S:/---  (facile da parsare in C)
 *   - Domande salvate in ../esportazioni/quiz_db_<categoria>.txt
 *   - Compatibile con il DB della versione Python (stesso path esportazioni/)
 */
#include "common.h"
#include "backend.h"
#include "terminal.h"
#include "ai.h"
#include "prismalux_ui.h"
#include "quiz.h"

#include <sys/stat.h>
#include <signal.h>

/* ── Timeout generazione AI ─────────────────────────────────────────────── */
#define QUIZ_GEN_TIMEOUT_SEC  30   /* secondi massimi per la generazione */
#define QUIZ_GEN_MAX_RETRY     3   /* tentativi prima di arrendersi */

#ifndef _WIN32
static volatile sig_atomic_t g_quiz_timeout = 0;
static void _quiz_alarm_handler(int sig) { (void)sig; g_quiz_timeout = 1; }
#else
static int g_quiz_timeout = 0;   /* placeholder: timeout non supportato su Windows */
#endif

/* ── Categorie (compatibili con Python_Software/quiz_interattivi.py) ──────── */
#define N_CAT 9

static const char *CAT_KEY[N_CAT] = {
    "python_base", "python_medio", "python_avanzato",
    "algoritmi",   "matematica",   "fisica",
    "chimica",     "sicurezza",    "reti"
};

static const char *CAT_LABEL[N_CAT] = {
    "\xf0\x9f\x90\x8d Python Base",
    "\xf0\x9f\x94\xb7 Python Intermedio",
    "\xe2\x9a\xa1 Python Avanzato",
    "\xf0\x9f\x94\xb5 Algoritmi",
    "\xf0\x9f\x93\x90 Matematica",
    "\xe2\x9a\x9b\xef\xb8\x8f Fisica",
    "\xf0\x9f\xa7\xaa Chimica",
    "\xf0\x9f\x94\x92 Sicurezza Informatica",
    "\xf0\x9f\x8c\x90 Protocolli di Rete"
};

static const char *CAT_DESC[N_CAT] = {
    "variabili, if/else, cicli, liste, dizionari, funzioni",
    "comprehension, lambda, map/filter, eccezioni, file I/O",
    "generatori, decoratori, classi, async/await, metaclassi",
    "sorting, ricerca binaria, grafi, BFS/DFS, programmazione dinamica",
    "algebra, calcolo, probabilita, statistica, algebra lineare",
    "meccanica, termodinamica, ottica, elettromagnetismo, cinematica",
    "legami chimici, reazioni, stechiometria, termodinamica chimica",
    "crittografia, vulnerabilita OWASP, reti, difese, protocolli",
    "TCP/IP, HTTP/HTTPS, DNS, TLS, OAuth, REST, WebSocket"
};

/* ── Limiti ─────────────────────────────────────────────────────────────── */
#define MAX_DOM_PER_CAT  300   /* domande caricabili in RAM per categoria */
#define MAX_UTENTI        50   /* utenti nel file JSON */
#define GEN_BUF_SZ     32768  /* buffer per raccogliere output AI */

/* ── Strutture ─────────────────────────────────────────────────────────── */

/* Domanda a scelta multipla */
typedef struct {
    char domanda[400];
    char opz[4][200];   /* A=0  B=1  C=2  D=3 */
    int  corretta;      /* indice 0–3 */
    char spieg[200];
} QuizDom;

/* Profilo utente (persistito in quiz_utenti.json) */
typedef struct {
    char nome[64];
    int  qt;   /* quiz totali giocati */
    int  dt;   /* domande totali risposte */
    int  ct;   /* corrette totali */
    int  mp;   /* miglior percentuale */
} QuizUtente;

/* ── Percorsi file ─────────────────────────────────────────────────────── */

static void _db_path(char *out, int sz, const char *cat_key) {
    snprintf(out, sz, "../esportazioni/quiz_db_%s.txt", cat_key);
}

static const char *_users_path(void)    { return "../esportazioni/quiz_utenti.json"; }
static const char *_sessions_path(void) { return "../esportazioni/quiz_sessioni.csv"; }

/* Crea la directory ../esportazioni/ se non esiste */
static void _ensure_dir(void) {
    struct stat st;
    if (stat("../esportazioni", &st) != 0)
#ifdef _WIN32
        mkdir("../esportazioni");
#else
        mkdir("../esportazioni", 0755);
#endif
}

/* ── DB domande: carica ─────────────────────────────────────────────────── */
/*
 * Formato file (una domanda per blocco ---):
 *   Q:Testo della domanda?
 *   A:Prima opzione
 *   B:Seconda opzione
 *   C:Terza opzione
 *   D:Quarta opzione
 *   R:A          <- risposta corretta (A-D)
 *   S:Spiegazione breve
 *   ---
 */
static int _carica_domande(const char *cat_key, QuizDom *out, int max_out) {
    char path[256];
    _db_path(path, sizeof path, cat_key);
    FILE *f = fopen(path, "r");
    if (!f) return 0;

    int n = 0;
    QuizDom cur;
    memset(&cur, 0, sizeof cur);
    int has_q = 0, has_r = 0, has_ab = 0;
    char line[512];

    while (n < max_out && fgets(line, sizeof line, f)) {
        int len = (int)strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = '\0';

        if (strncmp(line, "Q:", 2) == 0) {
            memset(&cur, 0, sizeof cur);
            strncpy(cur.domanda, line+2, sizeof cur.domanda - 1);
            has_q = 1; has_r = 0; has_ab = 0;
        } else if (len >= 2 && line[1] == ':' && line[0] >= 'A' && line[0] <= 'D') {
            int idx = line[0] - 'A';
            strncpy(cur.opz[idx], line+2, sizeof cur.opz[0] - 1);
            has_ab++;
        } else if (strncmp(line, "R:", 2) == 0 && len >= 3) {
            char r = line[2];
            if (r >= 'A' && r <= 'D') { cur.corretta = r - 'A'; has_r = 1; }
        } else if (strncmp(line, "S:", 2) == 0) {
            strncpy(cur.spieg, line+2, sizeof cur.spieg - 1);
        } else if (strncmp(line, "---", 3) == 0) {
            if (has_q && has_r && has_ab >= 4)
                out[n++] = cur;
            has_q = 0; has_r = 0; has_ab = 0;
        }
    }
    fclose(f);
    return n;
}

/* ── DB domande: conta (senza caricare tutto in RAM) ───────────────────── */
static int _conta_domande(const char *cat_key) {
    char path[256];
    _db_path(path, sizeof path, cat_key);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    int n = 0;
    char line[16];
    while (fgets(line, sizeof line, f))
        if (strncmp(line, "---", 3) == 0) n++;
    fclose(f);
    return n;
}

/* ── DB domande: salva (append) ─────────────────────────────────────────── */
static void _salva_domande(const char *cat_key, const QuizDom *dom, int n) {
    char path[256];
    _db_path(path, sizeof path, cat_key);
    FILE *f = fopen(path, "a");
    if (!f) return;
    const char lettere[] = "ABCD";
    for (int i = 0; i < n; i++) {
        fprintf(f, "Q:%s\n", dom[i].domanda);
        for (int j = 0; j < 4; j++)
            fprintf(f, "%c:%s\n", lettere[j], dom[i].opz[j]);
        fprintf(f, "R:%c\n", lettere[dom[i].corretta]);
        fprintf(f, "S:%s\n", dom[i].spieg);
        fprintf(f, "---\n");
    }
    fclose(f);
}

/* ── Parser output AI ───────────────────────────────────────────────────── */
/*
 * Parsa il testo generato dall'AI cercando il formato strutturato.
 * Tollerante a testo extra prima/dopo i blocchi: cerca Q: come inizio.
 */
static int _parse_ai_output(const char *testo, QuizDom *out, int max_out) {
    int n = 0;
    QuizDom cur;
    memset(&cur, 0, sizeof cur);
    int has_q = 0, has_r = 0, has_ab = 0;
    const char *p = testo;
    char line[512];

    while (*p && n < max_out) {
        const char *eol = strchr(p, '\n');
        int len = eol ? (int)(eol - p) : (int)strlen(p);
        if (len >= (int)sizeof(line)) len = (int)sizeof(line) - 1;
        strncpy(line, p, len);
        line[len] = '\0';
        if (len > 0 && line[len-1] == '\r') line[--len] = '\0';
        p = eol ? eol + 1 : p + len;

        if (strncmp(line, "Q:", 2) == 0) {
            memset(&cur, 0, sizeof cur);
            strncpy(cur.domanda, line+2, sizeof cur.domanda - 1);
            has_q = 1; has_r = 0; has_ab = 0;
        } else if (len >= 2 && line[1] == ':' && line[0] >= 'A' && line[0] <= 'D') {
            int idx = line[0] - 'A';
            strncpy(cur.opz[idx], line+2, sizeof cur.opz[0] - 1);
            has_ab++;
        } else if (strncmp(line, "R:", 2) == 0 && len >= 3) {
            char r = line[2];
            if (r >= 'A' && r <= 'D') { cur.corretta = r - 'A'; has_r = 1; }
        } else if (strncmp(line, "S:", 2) == 0) {
            strncpy(cur.spieg, line+2, sizeof cur.spieg - 1);
        } else if (strncmp(line, "---", 3) == 0) {
            if (has_q && has_r && has_ab >= 4)
                out[n++] = cur;
            has_q = 0; has_r = 0; has_ab = 0;
        }
    }
    /* ultima domanda se non seguita da --- */
    if (n < max_out && has_q && has_r && has_ab >= 4)
        out[n++] = cur;
    return n;
}

/* ── Genera domande con AI ──────────────────────────────────────────────── */
static void _genera_domande(int cat_idx, int n_dom) {
    if (!backend_ready(&g_ctx)) { ensure_ready_or_return(&g_ctx); return; }
    if (!check_risorse_ok()) return;

    printf(CYN "\n  Generazione in corso: %d domande su %s...\n\n" RST,
           n_dom, CAT_LABEL[cat_idx]);
    printf(GRY "  Il modello sta costruendo le domande, attendere...\n\n" RST);

    const char *sys =
        "Sei un docente esperto. Genera domande a scelta multipla didatticamente corrette. "
        "Rispondi SEMPRE e SOLO in italiano. "
        "Usa ESATTAMENTE il formato richiesto, senza testo aggiuntivo prima o dopo.";

    char usr[1024];
    snprintf(usr, sizeof usr,
        "Genera %d domande a scelta multipla su: %s\n\n"
        "Usa ESATTAMENTE questo formato per ogni domanda:\n"
        "Q:Testo della domanda?\n"
        "A:Prima opzione\n"
        "B:Seconda opzione\n"
        "C:Terza opzione\n"
        "D:Quarta opzione\n"
        "R:A\n"
        "S:Spiegazione breve (max 80 caratteri)\n"
        "---\n\n"
        "Regole: una sola risposta corretta, difficolta progressiva, "
        "opzioni plausibili e non banali, lingua italiana.",
        n_dom, CAT_DESC[cat_idx]);

    static char buf[GEN_BUF_SZ];
    static QuizDom dom[50];
    int n = 0;

    /* Installa handler SIGALRM per il timeout (POSIX only — non disponibile su Windows) */
#ifndef _WIN32
    struct sigaction sa_new = {0}, sa_old = {0};
    sa_new.sa_handler = _quiz_alarm_handler;
    sigemptyset(&sa_new.sa_mask);
    sigaction(SIGALRM, &sa_new, &sa_old);
#endif

    for (int attempt = 1; attempt <= QUIZ_GEN_MAX_RETRY; attempt++) {
        if (attempt > 1)
            printf(YLW "  Tentativo %d/%d...\n\n" RST, attempt, QUIZ_GEN_MAX_RETRY);

        buf[0] = '\0';
        g_quiz_timeout = 0;

#ifndef _WIN32
        /* Avvia il timer: se scade, SIGALRM interrompe recv() in http.c */
        alarm(QUIZ_GEN_TIMEOUT_SEC);
#endif

        ai_chat_stream(&g_ctx, sys, usr, stream_cb, NULL, buf, GEN_BUF_SZ);

#ifndef _WIN32
        alarm(0);   /* annulla il timer se la generazione è finita in tempo */
#endif
        printf("\n\n");

        if (g_quiz_timeout) {
            printf(YLW "  \xe2\x9a\xa0  Timeout (%ds) — il modello ha impiegato troppo.\n" RST,
                   QUIZ_GEN_TIMEOUT_SEC);
            if (attempt < QUIZ_GEN_MAX_RETRY)
                printf(GRY "  Riprovo...\n\n" RST);
            continue;
        }

        n = _parse_ai_output(buf, dom, 50);
        if (n > 0) break;   /* successo: uscita dal loop */

        printf(YLW "  Formato non riconosciuto nella risposta del modello.\n" RST);
        if (attempt < QUIZ_GEN_MAX_RETRY)
            printf(GRY "  Riprovo con lo stesso prompt...\n\n" RST);
    }

#ifndef _WIN32
    /* Ripristina il vecchio handler SIGALRM */
    sigaction(SIGALRM, &sa_old, NULL);
#endif

    if (n == 0) {
        printf(YLW "  \xe2\x9d\x8c  Impossibile generare domande dopo %d tentativi.\n" RST,
               QUIZ_GEN_MAX_RETRY);
        printf(GRY "  Suggerimenti: usa un modello pi\xc3\xb9 grande, riduci il numero\n"
               GRY "  di domande richieste, o cambia categoria.\n" RST);
    } else {
        _salva_domande(CAT_KEY[cat_idx], dom, n);
        printf(GRN "  \xe2\x9c\x94  %d domande aggiunte al database.\n" RST, n);
        if (n < n_dom)
            printf(YLW "  (richieste %d, generate %d)\n" RST, n_dom, n);
    }
    printf(GRY "\n  [INVIO] " RST); fflush(stdout);
    { char _b[4]; if (!fgets(_b, sizeof _b, stdin)) {} }
}

/* ── Utenti: carica da JSON ─────────────────────────────────────────────── */
/*
 * Formato quiz_utenti.json:
 *   {"utenti":[{"nome":"Mario","qt":5,"dt":50,"ct":38,"mp":85},...]}
 * Parser manuale: cerca "nome": e i campi numerici nel segmento tra { e }.
 */
static int _carica_utenti(QuizUtente *u, int max_u) {
    FILE *f = fopen(_users_path(), "r");
    if (!f) return 0;
    char buf[65536];
    int sz = (int)fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[sz] = '\0';

    int n = 0;
    const char *p = buf;
    while (n < max_u) {
        const char *q = strstr(p, "\"nome\":");
        if (!q) break;
        q += 7;
        while (*q == ' ' || *q == '"') q++;
        if (!*q) break;
        int i = 0;
        while (*q && *q != '"' && i < 63)
            u[n].nome[i++] = *q++;
        u[n].nome[i] = '\0';
        if (!u[n].nome[0]) { p = q; continue; }

        /* Estrai interi dal segmento fino al prossimo } */
        const char *end = strchr(q, '}');
        if (!end) end = buf + sz;
        char seg[256];
        int seglen = (int)(end - q);
        if (seglen >= (int)sizeof seg) seglen = (int)sizeof seg - 1;
        strncpy(seg, q, seglen); seg[seglen] = '\0';

        #define EXT(key, field) { \
            const char *kp = strstr(seg, "\"" key "\":"); \
            if (kp) { kp += sizeof("\"" key "\":") - 1; \
                      while(*kp==' ') kp++; field = atoi(kp); } \
        }
        u[n].qt = 0; u[n].dt = 0; u[n].ct = 0; u[n].mp = 0;
        EXT("qt", u[n].qt)
        EXT("dt", u[n].dt)
        EXT("ct", u[n].ct)
        EXT("mp", u[n].mp)
        #undef EXT

        n++;
        p = end + 1;
    }
    return n;
}

/* ── Utenti: salva JSON ─────────────────────────────────────────────────── */
static void _salva_utenti(const QuizUtente *u, int nu) {
    FILE *f = fopen(_users_path(), "w");
    if (!f) return;
    fprintf(f, "{\n  \"utenti\": [\n");
    for (int i = 0; i < nu; i++) {
        fprintf(f, "    {\"nome\":\"%s\",\"qt\":%d,\"dt\":%d,\"ct\":%d,\"mp\":%d}%s\n",
                u[i].nome, u[i].qt, u[i].dt, u[i].ct, u[i].mp,
                i < nu - 1 ? "," : "");
    }
    fprintf(f, "  ]\n}\n");
    fclose(f);
}

/* Crea o aggiorna il profilo utente dopo una sessione */
static void _aggiorna_utente(const char *nome, int domande, int corrette, int perc) {
    static QuizUtente utenti[MAX_UTENTI];
    int nu = _carica_utenti(utenti, MAX_UTENTI);

    int found = -1;
    for (int i = 0; i < nu; i++)
        if (strcmp(utenti[i].nome, nome) == 0) { found = i; break; }

    if (found < 0 && nu < MAX_UTENTI) {
        found = nu++;
        memset(&utenti[found], 0, sizeof utenti[0]);
        strncpy(utenti[found].nome, nome, 63);
    }
    if (found >= 0) {
        utenti[found].qt++;
        utenti[found].dt += domande;
        utenti[found].ct += corrette;
        if (perc > utenti[found].mp) utenti[found].mp = perc;
        _salva_utenti(utenti, nu);
    }
}

/* ── Log sessione (CSV append) ──────────────────────────────────────────── */
static void _log_sessione(const char *utente, const char *cat_key,
                           int domande, int corrette, int perc) {
    FILE *f = fopen(_sessions_path(), "a");
    if (!f) return;
    /* header se file vuoto */
    fseek(f, 0, SEEK_END);
    if (ftell(f) == 0)
        fprintf(f, "utente,categoria,domande,corrette,perc,data\n");
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    fprintf(f, "%s,%s,%d,%d,%d,%04d-%02d-%02d %02d:%02d\n",
            utente, cat_key, domande, corrette, perc,
            tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
            tm->tm_hour, tm->tm_min);
    fclose(f);
}

/* ── Scegli / crea utente ───────────────────────────────────────────────── */
static void _scegli_utente(char *out_nome, int out_sz) {
    static QuizUtente utenti[MAX_UTENTI];
    char buf[128];

    while (1) {
        int nu = _carica_utenti(utenti, MAX_UTENTI);
        clear_screen(); print_header();
        printf(CYN BLD "\n  \xf0\x9f\x91\xa4  Seleziona Utente\n\n" RST);

        if (nu > 0) {
            printf(GRY "  Utenti registrati:\n\n" RST);
            for (int i = 0; i < nu; i++) {
                int avg = utenti[i].dt > 0
                    ? (int)(utenti[i].ct * 100 / utenti[i].dt) : 0;
                printf("  " YLW "[%d]" RST "  %-24s  "
                       GRY "Quiz: %d  Media: %d%%  Record: %d%%\n" RST,
                       i+1, utenti[i].nome, utenti[i].qt, avg, utenti[i].mp);
            }
            printf("\n");
        }

        printf("  " GRN "[N]" RST "  Nuovo utente\n");
        printf("  " GRY "[0]" RST "  Torna\n\n");
        printf("  " GRN "Scelta: " RST); fflush(stdout);

        if (!fgets(buf, sizeof buf, stdin)) break;
        buf[strcspn(buf, "\n")] = '\0';

        if (buf[0] == '0') { out_nome[0] = '\0'; return; }

        if (buf[0] == 'n' || buf[0] == 'N') {
            printf("  Nome utente: "); fflush(stdout);
            if (!fgets(buf, sizeof buf, stdin)) break;
            buf[strcspn(buf, "\n")] = '\0';
            if (!buf[0]) continue;
            strncpy(out_nome, buf, out_sz - 1);
            out_nome[out_sz - 1] = '\0';
            _aggiorna_utente(out_nome, 0, 0, 0);
            return;
        }

        int scelta = atoi(buf);
        if (scelta >= 1 && scelta <= nu) {
            strncpy(out_nome, utenti[scelta-1].nome, out_sz - 1);
            out_nome[out_sz - 1] = '\0';
            return;
        }
    }
    out_nome[0] = '\0';
}

/* ── Classifica globale ─────────────────────────────────────────────────── */
static void _classifica(void) {
    static QuizUtente utenti[MAX_UTENTI];
    int nu = _carica_utenti(utenti, MAX_UTENTI);

    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x8f\x86  Classifica Utenti\n\n" RST);

    if (nu == 0) {
        printf(GRY "  Nessun utente ancora. Gioca un quiz per apparire in classifica.\n" RST);
        printf(GRY "\n  [INVIO] " RST); fflush(stdout);
        { char _b[4]; if (!fgets(_b, sizeof _b, stdin)) {} }
        return;
    }

    /* Ordina per percentuale media (bubble sort, n piccolo) */
    for (int i = 0; i < nu - 1; i++) {
        for (int j = 0; j < nu - 1 - i; j++) {
            int avgA = utenti[j].dt   > 0 ? utenti[j].ct   * 100 / utenti[j].dt   : 0;
            int avgB = utenti[j+1].dt > 0 ? utenti[j+1].ct * 100 / utenti[j+1].dt : 0;
            if (avgA < avgB) {
                QuizUtente tmp = utenti[j]; utenti[j] = utenti[j+1]; utenti[j+1] = tmp;
            }
        }
    }

    printf(GRY "  %-3s  %-22s  %5s  %5s  %8s  %6s\n" RST,
           "#", "Utente", "Quiz", "Dom.", "Media %", "Record");
    printf(GRY "  ");
    for (int i = 0; i < 58; i++) putchar('-');
    printf("\n" RST);

    const char *medaglie[] = {
        "\xf0\x9f\xa5\x87",  /* oro */
        "\xf0\x9f\xa5\x88",  /* argento */
        "\xf0\x9f\xa5\x89"   /* bronzo */
    };

    for (int i = 0; i < nu; i++) {
        int avg = utenti[i].dt > 0 ? (int)(utenti[i].ct * 100 / utenti[i].dt) : 0;
        const char *col = avg >= 80 ? GRN : avg >= 60 ? YLW : GRY;
        const char *pos = i < 3 ? medaglie[i] : "   ";
        printf("  %s " YLW "%2d" RST "  %-22s  "
               GRY "%4d   %4d    " RST
               "%s%5d%%   " RST YLW "%4d%%\n" RST,
               pos, i+1,
               utenti[i].nome,
               utenti[i].qt,
               utenti[i].dt,
               col, avg,
               utenti[i].mp);
    }

    printf(GRY "\n  [INVIO] " RST); fflush(stdout);
    { char _b[4]; if (!fgets(_b, sizeof _b, stdin)) {} }
}

/* ── Storico sessioni (CSV) ─────────────────────────────────────────────── */
static void _storico(const char *filtro_utente) {
    clear_screen(); print_header();
    if (filtro_utente && filtro_utente[0])
        printf(CYN BLD "\n  \xf0\x9f\x93\x8a  Storico — %s\n\n" RST, filtro_utente);
    else
        printf(CYN BLD "\n  \xf0\x9f\x93\x8a  Storico Sessioni\n\n" RST);

    FILE *f = fopen(_sessions_path(), "r");
    if (!f) {
        printf(GRY "  Nessuna sessione ancora.\n" RST);
        printf(GRY "\n  [INVIO] " RST); fflush(stdout);
        { char _b[4]; if (!fgets(_b, sizeof _b, stdin)) {} }
        return;
    }

    /* Ring buffer: teniamo le ultime 20 righe filtrate */
    char righe[20][256];
    int nr = 0;
    char line[256];
    int skip = 1; /* salta header CSV */

    while (fgets(line, sizeof line, f)) {
        if (skip) { skip = 0; continue; }
        line[strcspn(line, "\n")] = '\0';
        /* filtra per utente se richiesto */
        if (filtro_utente && filtro_utente[0]) {
            if (strncmp(line, filtro_utente, strlen(filtro_utente)) != 0) continue;
        }
        strncpy(righe[nr % 20], line, 255);
        nr++;
    }
    fclose(f);

    if (nr == 0) {
        printf(GRY "  Nessuna sessione registrata.\n" RST);
        printf(GRY "\n  [INVIO] " RST); fflush(stdout);
        { char _b[4]; if (!fgets(_b, sizeof _b, stdin)) {} }
        return;
    }

    printf(GRY "  %-20s  %-24s  %4s  %5s  %8s  %s\n" RST,
           "Utente", "Categoria", "Dom.", "Corr.", "Punteggio", "Data");
    printf(GRY "  ");
    for (int i = 0; i < 78; i++) putchar('-');
    printf("\n" RST);

    /* Mostra ultime 20 in ordine inverso (più recenti prima) */
    int shown = nr > 20 ? 20 : nr;
    int start = nr > 20 ? nr % 20 : 0;
    for (int k = shown - 1; k >= 0; k--) {
        char *r = righe[(start + k) % 20];
        char u[64]="", c[32]="", d[8]="", ok[8]="", p[8]="", dt[20]="";
        sscanf(r, "%63[^,],%31[^,],%7[^,],%7[^,],%7[^,],%19[^\n]",
               u, c, d, ok, p, dt);
        int perc = atoi(p);
        const char *col = perc >= 80 ? GRN : perc >= 60 ? YLW : MAG;
        printf("  %-20s  %-24s  %-4s  %-5s  %s%5d%%\t" RST "  %s\n",
               u, c, d, ok, col, perc, dt);
    }

    printf(GRY "\n  [INVIO] " RST); fflush(stdout);
    { char _b[4]; if (!fgets(_b, sizeof _b, stdin)) {} }
}

/* ── Stato database ─────────────────────────────────────────────────────── */
static void _stato_db(void) {
    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x97\x83\xef\xb8\x8f  Database Domande\n\n" RST);

    int totale = 0;
    for (int i = 0; i < N_CAT; i++) {
        int n = _conta_domande(CAT_KEY[i]);
        totale += n;
        const char *stato, *col;
        if      (n == 0)   { stato = "vuota \xe2\x80\x94 genera!"; col = MAG; }
        else if (n < 10)   { stato = "poche";                      col = YLW; }
        else               { stato = "OK";                         col = GRN; }
        printf("  %-36s  %s%3d domande" RST "  %s(%s)" RST "\n",
               CAT_LABEL[i], col, n, col, stato);
    }
    printf("\n" GRY "  Totale: " RST YLW "%d domande\n" RST, totale);
    printf(GRY "  Percorso: ../esportazioni/quiz_db_*.txt\n" RST);
    printf(GRY "\n  [INVIO] " RST); fflush(stdout);
    { char _b[4]; if (!fgets(_b, sizeof _b, stdin)) {} }
}

/* ── Esecuzione quiz ─────────────────────────────────────────────────────── */
static void _shuffle(int *arr, int n) {
    srand((unsigned)time(NULL));
    for (int i = n - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int tmp = arr[i]; arr[i] = arr[j]; arr[j] = tmp;
    }
}

static void _esegui_quiz(const char *utente, int cat_idx, int n_dom) {
    static QuizDom pool[MAX_DOM_PER_CAT];
    int n_pool = _carica_domande(CAT_KEY[cat_idx], pool, MAX_DOM_PER_CAT);

    if (n_pool == 0) {
        printf(YLW "\n  Nessuna domanda per questa categoria.\n" RST);
        printf(GRY "  Usa [2] Genera domande per popolare il database.\n" RST);
        printf(GRY "\n  [INVIO] " RST); fflush(stdout);
        { char _b[4]; if (!fgets(_b, sizeof _b, stdin)) {} }
        return;
    }

    /* Indici randomizzati */
    int idx[MAX_DOM_PER_CAT];
    for (int i = 0; i < n_pool; i++) idx[i] = i;
    _shuffle(idx, n_pool);
    int n = (n_dom < n_pool) ? n_dom : n_pool;

    int punteggio = 0;
    int risposte  = 0;
    const char lettere[] = "ABCD";
    char buf[8];

    for (int q = 0; q < n; q++) {
        const QuizDom *d = &pool[idx[q]];

        clear_screen(); print_header();

        /* Barra progresso */
        int W = 36;
        int fill = n > 0 ? (q * W) / n : 0;
        printf("\n  " GRN);
        for (int i = 0; i < fill;      i++) printf("\xe2\x96\x88");
        printf(GRY);
        for (int i = fill; i < W; i++) printf("\xe2\x96\x91");
        printf(RST "  %d/%d   " YLW "[%s]" RST "  " GRN "%d corrette\n\n" RST,
               q, n, utente, punteggio);

        /* Domanda */
        printf(CYN BLD "  %s  Domanda %d/%d\n\n" RST, CAT_LABEL[cat_idx], q+1, n);
        printf("  %s\n\n", d->domanda);

        for (int j = 0; j < 4; j++)
            printf("  " YLW "[%c]" RST "  %s\n", lettere[j], d->opz[j]);

        printf("\n  " GRN "Risposta [A/B/C/D, 0=abbandona]: " RST); fflush(stdout);
        if (!fgets(buf, sizeof buf, stdin)) break;
        char ris = (char)toupper((unsigned char)buf[0]);
        if (ris == '0') break;

        risposte++;
        int giusta = (ris >= 'A' && ris <= 'D') && (ris - 'A' == d->corretta);
        if (giusta) {
            punteggio++;
            printf("\n  " GRN BLD "\xe2\x9c\x85  CORRETTO!\n" RST);
        } else {
            printf("\n  " MAG BLD "\xe2\x9d\x8c  SBAGLIATO!" RST
                   "  Risposta: " GRN "[%c] %s\n" RST,
                   lettere[d->corretta], d->opz[d->corretta]);
        }
        if (d->spieg[0])
            printf("  " GRY "%s\n" RST, d->spieg);

        printf(GRY "\n  [INVIO] " RST); fflush(stdout);
        { char _b[8]; if (!fgets(_b, sizeof _b, stdin)) {} }
    }

    if (risposte == 0) return;

    /* Riepilogo finale */
    int perc = punteggio * 100 / risposte;
    const char *voto, *col_v;
    if      (perc >= 80) { voto = "OTTIMO";      col_v = GRN; }
    else if (perc >= 60) { voto = "BUONO";        col_v = YLW; }
    else if (perc >= 40) { voto = "SUFFICIENTE";  col_v = MAG; }
    else                 { voto = "DA RIVEDERE";  col_v = MAG; }

    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x8f\x86  RISULTATO FINALE\n\n" RST);
    printf("  Utente:     " YLW "%s\n" RST, utente);
    printf("  Categoria:  %s\n", CAT_LABEL[cat_idx]);
    printf("  Domande:    %d\n", risposte);
    printf("  Corrette:   " GRN "%d\n" RST, punteggio);
    printf("  Sbagliate:  " MAG "%d\n" RST, risposte - punteggio);
    printf("  Punteggio:  %s%d%%  %s\n\n" RST, col_v, perc, voto);

    _aggiorna_utente(utente, risposte, punteggio, perc);
    _log_sessione(utente, CAT_KEY[cat_idx], risposte, punteggio, perc);

    printf(GRY "  [INVIO] " RST); fflush(stdout);
    { char _b[4]; if (!fgets(_b, sizeof _b, stdin)) {} }
}

/* ── Menu principale ─────────────────────────────────────────────────────── */
void menu_quiz(void) {
    _ensure_dir();
    char utente[64] = "";
    char buf[8];

    while (1) {
        /* Conta domande totali nel DB */
        int tot_dom = 0;
        for (int i = 0; i < N_CAT; i++)
            tot_dom += _conta_domande(CAT_KEY[i]);

        clear_screen(); print_header();
        printf(CYN BLD "\n  \xf0\x9f\x93\x9d  Quiz Interattivi\n\n" RST);
        printf("  Utente: %s%s\n" RST,
               utente[0] ? YLW : GRY,
               utente[0] ? utente : "(nessuno selezionato)");
        printf(GRY "  Domande nel database: " RST YLW "%d\n\n" RST, tot_dom);

        printf("  " EM_1 "  \xf0\x9f\x8e\xaf  Inizia Quiz\n");
        printf("  " EM_2 "  \xf0\x9f\xa4\x96  Genera domande con AI\n");
        printf("  " EM_3 "  \xf0\x9f\x91\xa4  Seleziona / Crea utente\n");
        printf("  " EM_4 "  \xf0\x9f\x8f\x86  Classifica\n");
        printf("  " EM_5 "  \xf0\x9f\x93\x8a  Storico sessioni\n");
        printf("  " EM_6 "  \xf0\x9f\x97\x83\xef\xb8\x8f  Stato database\n");
        printf("  " EM_0 "  Torna\n\n  " GRN "Scelta: " RST); fflush(stdout);

        int c = getch_single(); printf("%c\n", c);
        if (c == '0' || c == 'q' || c == 'Q' || c == 27) break;

        /* ── 1: Inizia Quiz */
        if (c == '1') {
            if (!utente[0]) {
                printf(YLW "\n  Seleziona prima un utente ([3]).\n" RST);
                printf(GRY "  [INVIO] " RST); fflush(stdout);
                { char _b[4]; if (!fgets(_b, sizeof _b, stdin)) {} }
                continue;
            }
            clear_screen(); print_header();
            printf(CYN BLD "\n  Scegli categoria:\n\n" RST);
            for (int i = 0; i < N_CAT; i++) {
                int nd = _conta_domande(CAT_KEY[i]);
                printf("  " YLW "[%d]" RST "  %-36s  ", i+1, CAT_LABEL[i]);
                if (nd > 0) printf(GRN "%d domande\n" RST, nd);
                else        printf(MAG "vuota\n" RST);
            }
            printf("\n  " GRY "[0] Torna\n\n  " GRN "Scelta: " RST); fflush(stdout);
            if (!fgets(buf, sizeof buf, stdin)) continue;
            buf[strcspn(buf, "\n")] = '\0';
            if (buf[0] == '0') continue;
            int cat = atoi(buf) - 1;
            if (cat < 0 || cat >= N_CAT) continue;
            if (_conta_domande(CAT_KEY[cat]) == 0) {
                printf(YLW "\n  Categoria vuota. Usa [2] Genera domande prima.\n" RST);
                printf(GRY "  [INVIO] " RST); fflush(stdout);
                { char _b[4]; if (!fgets(_b, sizeof _b, stdin)) {} }
                continue;
            }
            printf("  Quante domande? [10]: "); fflush(stdout);
            if (!fgets(buf, sizeof buf, stdin)) continue;
            int n_dom = atoi(buf);
            if (n_dom <= 0) n_dom = 10;
            _esegui_quiz(utente, cat, n_dom);
        }

        /* ── 2: Genera domande */
        else if (c == '2') {
            clear_screen(); print_header();
            printf(CYN BLD "\n  Genera domande \xe2\x80\x94 Scegli categoria:\n\n" RST);
            printf("  " YLW "[0]" RST "  Tutte le categorie\n");
            for (int i = 0; i < N_CAT; i++) {
                int nd = _conta_domande(CAT_KEY[i]);
                printf("  " YLW "[%d]" RST "  %-36s  " GRY "%d dom.\n" RST,
                       i+1, CAT_LABEL[i], nd);
            }
            printf("\n  " GRN "Scelta: " RST); fflush(stdout);
            if (!fgets(buf, sizeof buf, stdin)) continue;
            buf[strcspn(buf, "\n")] = '\0';

            printf("  Quante domande per categoria? [5]: "); fflush(stdout);
            char nbuf[8];
            if (!fgets(nbuf, sizeof nbuf, stdin)) continue;
            int n_dom = atoi(nbuf);
            if (n_dom <= 0) n_dom = 5;
            if (n_dom > 20) n_dom = 20;

            if (buf[0] == '0') {
                for (int i = 0; i < N_CAT; i++)
                    _genera_domande(i, n_dom);
            } else {
                int cat = atoi(buf) - 1;
                if (cat >= 0 && cat < N_CAT)
                    _genera_domande(cat, n_dom);
            }
        }

        /* ── 3: Seleziona utente */
        else if (c == '3') {
            _scegli_utente(utente, sizeof utente);
        }

        /* ── 4: Classifica */
        else if (c == '4') {
            _classifica();
        }

        /* ── 5: Storico */
        else if (c == '5') {
            _storico(utente);
        }

        /* ── 6: Stato DB */
        else if (c == '6') {
            _stato_db();
        }
    }
}
