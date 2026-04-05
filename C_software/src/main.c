/*
 * main.c — SEZIONE 12: Menu principale, config JSON, main()
 * ===========================================================
 * "Costruito per i mortali che aspirano alla saggezza."
 *
 * Supporta 3 backend AI selezionabili a runtime:
 *   --backend llama        → libllama statica (richiede ./build.sh)
 *   --backend ollama       → HTTP a Ollama    (default porta 11434)
 *   --backend llama-server → HTTP a llama-server OpenAI API (default porta 8080)
 */
#include "common.h"
#ifdef _WIN32
#include <shellapi.h>
#endif
#include "backend.h"
#include "terminal.h"
#include "http.h"
#include "ai.h"
#include "modelli.h"
#include "output.h"
#include "multi_agente.h"
#include "strumenti.h"
#include "simulatore.h"
#include "quiz.h"
#include "prismalux_ui.h"
#include "hw_detect.h"
#include "config_toon.h"

/* ══════════════════════════════════════════════════════════════
   Voce 1 — Agenti AI (sottomenu: Pipeline 6 agenti + Motore Byzantino)
   ══════════════════════════════════════════════════════════════ */
static void menu_agenti(void) {
    while(1){
        clear_screen(); print_header();
        printf(CYN BLD "\n  \xf0\x9f\xa4\x96  Agenti AI\n\n" RST);
        printf("  " EM_1 "  \xf0\x9f\x94\x84  Pipeline 6 Agenti     "
               GRY "ricercatore \xe2\x86\x92 pianificatore \xe2\x86\x92 2 prog \xe2\x86\x92 tester \xe2\x86\x92 ottimizzatore" RST "\n");
        printf("  " EM_2 "  \xf0\x9f\x94\xae  Motore Byzantino      "
               GRY "verifica a 4 agenti anti-allucinazione (A,B,C,D)" RST "\n");
        printf("  " EM_3 "  \xf0\x9f\x8e\xad  Modalit\xc3\xa0              "
               GRY "matematica, informatica, sicurezza, fisica, chimica..." RST "\n");
        printf("  " EM_4 "  \xf0\x9f\x93\x8b  Lista completa        "
               GRY "tutti i 50 ruoli" RST "\n");
        printf("  " EM_0 "  Torna\n\n  " GRN "Scelta: " RST); fflush(stdout);
        int c = getch_single(); printf("%c\n", c);
        if      (c == '1') menu_multi_agente();
        else if (c == '2') menu_byzantine_engine();
        else if (c == '3') menu_modalita_agenti();
        else if (c == '4') menu_agenti_ruoli();
        else if (c == '0' || c == 'q' || c == 'Q' || c == 27) break;
    }
}

/* ══════════════════════════════════════════════════════════════
   Voce 3 — Impara con AI
   ══════════════════════════════════════════════════════════════ */
static void menu_impara(void) {
    while(1){
        clear_screen(); print_header();
        printf(CYN BLD "\n  \xf0\x9f\x93\x9a  Impara con AI\n\n" RST);
        printf("  " EM_1 "  \xf0\x9f\x8f\x9b\xef\xb8\x8f  Tutor AI \xe2\x80\x94 Oracolo di Prismalux\n");
        printf("  " EM_2 "  \xf0\x9f\xa4\x96  Agenti AI Specializzati   " CYN "(50 ruoli)" RST "\n");
        printf("  " EM_3 "  \xf0\x9f\x8e\xae  Simulatore Algoritmi      " CYN "(Bubble, Merge, Quick, GCD...)" RST "\n");
        printf("  " EM_4 "  \xf0\x9f\x93\x9d  Quiz Interattivi\n");
        printf("  " EM_5 "  \xf0\x9f\x93\x8a  Dashboard Statistica      " GRY "(prossimamente)" RST "\n");
        printf("  " EM_0 "  Torna\n\n  " GRN "Scelta: " RST); fflush(stdout);
        int c=getch_single(); printf("%c\n",c);
        if(c=='1') menu_tutor();
        else if(c=='2') menu_agenti_ruoli();
        else if(c=='3') menu_simulatore();
        else if(c=='4') menu_quiz();
        else if(c=='5'){
            clear_screen(); print_header();
            printf(YLW "\n  \xf0\x9f\x8c\xab\xef\xb8\x8f  La nebbia copre la verit\xc3\xa0. Riprova pi\xc3\xb9 tardi.\n\n" RST);
            printf(GRY "  (funzionalit\xc3\xa0 in sviluppo)\n\n" RST);
            printf(GRY "  [INVIO per continuare] " RST); fflush(stdout);
            { char _b[4]; if(!fgets(_b,sizeof _b,stdin)){} }
        }
        else if(c=='0'||c=='q'||c=='Q'||c==27) break;
    }
}

/* ══════════════════════════════════════════════════════════════
   Voce 4 — Compila per Windows (.exe)
   ══════════════════════════════════════════════════════════════ */
/* ══════════════════════════════════════════════════════════════
   Helper: esegue un comando con output live, ritorna exit code
   ══════════════════════════════════════════════════════════════ */
static int _run_cmd(const char* cmd) {
    FILE* f = popen(cmd, "r");
    if (!f) return -1;
    char line[512]; int had = 0;
    while (fgets(line, sizeof line, f)) { printf("  %s", line); had=1; fflush(stdout); }
    int rc = pclose(f);
    if (!had && rc == 0) printf(GRN "  (nessun avviso \xe2\x80\x94 compilazione pulita)\n" RST);
    return rc;
}

/* ── Stampa separatore orizzontale ────────────────────────────── */
static void _sep(void) {
    int tw = term_width(); if(tw < 4) tw = 80;
    printf(DIM "  "); for(int i=2;i<tw-2;i++) printf("\xe2\x94\x80"); printf("\n" RST);
    fflush(stdout);
}

/* ══════════════════════════════════════════════════════════════
   Compila TUI (for_windows=1 → .exe MinGW, for_windows=0 → Linux gcc)
   ══════════════════════════════════════════════════════════════ */
static void _compila_tui(int for_windows) {
    clear_screen(); print_header();
    const char* srcs =
        "src/main.c src/backend.c src/terminal.c src/http.c src/ai.c "
        "src/modelli.c src/output.c src/multi_agente.c src/strumenti.c "
        "src/hw_detect.c src/agent_scheduler.c src/prismalux_ui.c";

    if (for_windows) {
        printf(CYN BLD "\n  \xf0\x9f\xaa\x9f  Compila TUI — Windows (.exe)\n\n" RST);
        const char* cc = NULL;
#ifdef _WIN32
        if (system("where gcc >nul 2>&1") == 0) cc = "gcc";
#else
        if (system("command -v x86_64-w64-mingw32-gcc >/dev/null 2>&1") == 0)
            cc = "x86_64-w64-mingw32-gcc";
#endif
        if (!cc) {
#ifdef _WIN32
            printf(YLW "  \xe2\x9a\xa0  gcc non trovato nel PATH.\n\n" RST);
            printf(DIM "  Su Windows installa MSYS2 (https://www.msys2.org) poi:\n\n");
            printf("    " YLW "pacman -S --needed git base-devel mingw-w64-x86_64-gcc" RST "\n");
            printf("    " GRY "# Apri MSYS2 MinGW64 e aggiungi C:\\msys64\\mingw64\\bin al PATH\n\n" RST);
#else
            printf(YLW "  \xe2\x9a\xa0  Cross-compilatore MinGW non trovato.\n\n" RST);
            printf(DIM "  Installa il cross-compilatore su Linux:\n\n");
            printf("    " YLW "sudo apt install gcc-mingw-w64-x86-64" RST "   # Debian/Ubuntu\n");
            printf("    " YLW "sudo pacman -S mingw-w64-gcc" RST "            # Arch\n");
            printf("    " YLW "sudo dnf install mingw64-gcc" RST "            # Fedora\n\n");
#endif
            printf(GRY "  [INVIO per continuare] " RST); fflush(stdout);
            { char _b[4]; if(!fgets(_b,sizeof _b,stdin)){} }
            return;
        }
        printf(GRN "  \xe2\x9c\x93 Compilatore: " YLW "%s" RST "\n", cc);
        printf(GRY "  Compilazione in corso...\n\n" RST); fflush(stdout);
        _sep();
        char cmd[2048];
        snprintf(cmd, sizeof cmd,
                 "%s -O2 -Wall -Iinclude -o prismalux.exe %s -lws2_32 -lm 2>&1",
                 cc, srcs);
        int ok = (_run_cmd(cmd) == 0);
        _sep();
        if (ok) {
            printf(GRN BLD "  \xf0\x9f\x8e\x89  Completato!\n\n" RST);
            printf("  Eseguibile: " YLW BLD "prismalux.exe" RST "\n\n");
            printf(DIM "  Copia su Windows e avvia: " YLW "prismalux.exe --backend ollama\n\n" RST);
        } else {
            printf(RED BLD "  \xe2\x9c\x97  Compilazione fallita. Controlla sopra.\n\n" RST);
        }

    } else {
        printf(CYN BLD "\n  \xf0\x9f\x90\xa7  Compila TUI — Linux (nativo)\n\n" RST);
        const char* cc = "gcc";
        printf(GRN "  \xe2\x9c\x93 Compilatore: " YLW "%s" RST "\n", cc);
        printf(GRY "  Compilazione in corso...\n\n" RST); fflush(stdout);
        _sep();
        char cmd[2048];
        snprintf(cmd, sizeof cmd,
                 "%s -O2 -Wall -Iinclude -o prismalux %s -lpthread -lm 2>&1",
                 cc, srcs);
        int ok = (_run_cmd(cmd) == 0);
        _sep();
        if (ok) {
            printf(GRN BLD "  \xf0\x9f\x8e\x89  Completato!\n\n" RST);
            printf("  Eseguibile: " YLW BLD "prismalux" RST "\n\n");
            printf(DIM "  Avvia con: " YLW "./prismalux --backend ollama\n\n" RST);
        } else {
            printf(RED BLD "  \xe2\x9c\x97  Compilazione fallita. Controlla sopra.\n\n" RST);
        }
    }

    printf(GRY "  [INVIO per continuare] " RST); fflush(stdout);
    { char _b[4]; if(!fgets(_b,sizeof _b,stdin)){} }
}

/* ══════════════════════════════════════════════════════════════
   Compila GUI Qt (for_windows=1 → MinGW cmake, for_windows=0 → Unix)
   ══════════════════════════════════════════════════════════════ */
static void _compila_qt(int for_windows) {
    clear_screen(); print_header();

    if (for_windows) {
        printf(CYN BLD "\n  \xf0\x9f\xaa\x9f\xf0\x9f\x96\xbc\xef\xb8\x8f  Compila GUI Qt — Windows (.exe)\n\n" RST);
    } else {
        printf(CYN BLD "\n  \xf0\x9f\x90\xa7\xf0\x9f\x96\xbc\xef\xb8\x8f  Compila GUI Qt — Linux (nativo)\n\n" RST);
    }

    /* Verifica cmake */
#ifdef _WIN32
    int has_cmake = (system("where cmake >nul 2>&1") == 0);
#else
    int has_cmake = (system("command -v cmake >/dev/null 2>&1") == 0);
#endif
    if (!has_cmake) {
        printf(RED "  \xe2\x9c\x97  cmake non trovato.\n\n" RST);
        printf(DIM "  Installa cmake:\n");
        printf("    " YLW "sudo apt install cmake" RST "   # Debian/Ubuntu\n");
        printf("    " YLW "sudo pacman -S cmake" RST "     # Arch\n");
        printf("    " YLW "winget install Kitware.CMake" RST " # Windows\n\n");
        printf(GRY "  [INVIO per continuare] " RST); fflush(stdout);
        { char _b[4]; if(!fgets(_b,sizeof _b,stdin)){} }
        return;
    }

    const char* qt_src = "../Qt_GUI";
    const char* qt_bld = for_windows ? "../Qt_GUI/build_win" : "../Qt_GUI/build_lin";
    const char* generator = for_windows ? "MinGW Makefiles" : "Unix Makefiles";
    const char* exe_name  = for_windows ? "Prismalux_GUI.exe" : "Prismalux_GUI";

    printf(GRY "  Sorgenti : " RST YLW "%s\n" RST, qt_src);
    printf(GRY "  Build dir: " RST YLW "%s\n" RST, qt_bld);
    printf(GRY "  Generator: " RST YLW "%s\n\n" RST, generator);

    /* Fase 1: configure */
    printf(GRY "  \xe2\x96\xb6 Fase 1/2: cmake configure...\n\n" RST); fflush(stdout);
    _sep();
    char cmd[1024];
    snprintf(cmd, sizeof cmd,
             "cmake -B \"%s\" -S \"%s\" -G \"%s\" -DCMAKE_BUILD_TYPE=Release 2>&1",
             qt_bld, qt_src, generator);
    int cfg_ok = (_run_cmd(cmd) == 0);
    _sep();

    if (!cfg_ok) {
        printf(RED BLD "  \xe2\x9c\x97  Configurazione cmake fallita.\n\n" RST);
        if (for_windows) {
            printf(DIM "  Prerequisiti Windows:\n");
            printf("    " YLW "Qt6 MinGW nel PATH" RST " (es. C:\\Qt\\6.x.x\\mingw_64\\bin)\n");
            printf("    " YLW "cmake + mingw32-make" RST "\n\n");
        } else {
            printf(DIM "  Prerequisiti Linux:\n");
            printf("    " YLW "sudo apt install qt6-base-dev qt6-base-dev-tools libgl-dev\n\n" RST);
        }
        printf(GRY "  [INVIO per continuare] " RST); fflush(stdout);
        { char _b[4]; if(!fgets(_b,sizeof _b,stdin)){} }
        return;
    }

    /* Fase 2: build */
    printf(GRY "\n  \xe2\x96\xb6 Fase 2/2: cmake build (1-2 min)...\n\n" RST); fflush(stdout);
    _sep();
#ifdef _WIN32
    snprintf(cmd, sizeof cmd,
             "cmake --build \"%s\" --config Release 2>&1", qt_bld);
#else
    snprintf(cmd, sizeof cmd,
             "cmake --build \"%s\" --config Release -j$(nproc) 2>&1", qt_bld);
#endif
    int bld_ok = (_run_cmd(cmd) == 0);
    _sep();

    if (bld_ok) {
        printf(GRN BLD "  \xf0\x9f\x8e\x89  Compilazione completata!\n\n" RST);
        printf("  Eseguibile: " YLW BLD "%s/%s" RST "\n\n", qt_bld, exe_name);
        if (for_windows)
            printf(DIM "  Copia la cartella su Windows con le DLL Qt e avvia.\n\n" RST);
        else
            printf(DIM "  Avvia con: " YLW "%s/%s\n\n" RST, qt_bld, exe_name);
    } else {
        printf(RED BLD "  \xe2\x9c\x97  Compilazione fallita.\n\n" RST);
        printf(DIM "  Controlla gli errori sopra.\n\n" RST);
    }

    printf(GRY "  [INVIO per continuare] " RST); fflush(stdout);
    { char _b[4]; if(!fgets(_b,sizeof _b,stdin)){} }
}

/* ══════════════════════════════════════════════════════════════
   Sotto-menu 4: Compila TUI (Windows + Linux)
   ══════════════════════════════════════════════════════════════ */
static void menu_compila_tui(void) {
    while(1){
        clear_screen(); print_header();
        printf(CYN BLD "\n  \xf0\x9f\x93\xa6  Compila TUI\n\n" RST);
        printf("  " EM_1 "  \xf0\x9f\xaa\x9f  Windows (.exe)   " GRY "cross-compila con MinGW (su Linux) o gcc (su Windows)" RST "\n");
        printf("  " EM_2 "  \xf0\x9f\x90\xa7  Linux (nativo)   " GRY "gcc ottimizzato per la CPU locale" RST "\n");
        printf("  " EM_0 "  Torna\n\n  " GRN "Scelta: " RST); fflush(stdout);
        int c = getch_single(); printf("%c\n", c);
        if      (c=='1') _compila_tui(1);
        else if (c=='2') _compila_tui(0);
        else if (c=='0'||c=='q'||c=='Q'||c==27) break;
    }
}

/* ══════════════════════════════════════════════════════════════
   Sotto-menu 5: Compila GUI Qt (Windows + Linux)
   ══════════════════════════════════════════════════════════════ */
static void menu_compila_qt(void) {
    while(1){
        clear_screen(); print_header();
        printf(CYN BLD "\n  \xf0\x9f\x96\xbc\xef\xb8\x8f  Compila GUI Qt\n\n" RST);
        printf("  " EM_1 "  \xf0\x9f\xaa\x9f  Windows (.exe)   " GRY "cmake + Qt6 MinGW" RST "\n");
        printf("  " EM_2 "  \xf0\x9f\x90\xa7  Linux (nativo)   " GRY "cmake + Qt6 Unix Makefiles" RST "\n");
        printf("  " EM_0 "  Torna\n\n  " GRN "Scelta: " RST); fflush(stdout);
        int c = getch_single(); printf("%c\n", c);
        if      (c=='1') _compila_qt(1);
        else if (c=='2') _compila_qt(0);
        else if (c=='0'||c=='q'||c=='Q'||c==27) break;
    }
}

/* Forward declaration: salva_config() e' definita piu' in basso in questo file */
void salva_config(void);

/* ══════════════════════════════════════════════════════════════
   Voce 4 — Impostazioni (backend AI, modello, strumenti avanzati)
   ══════════════════════════════════════════════════════════════ */
static void menu_impostazioni(void) {
    while (1) {
        _sync_to_profile();
        clear_screen(); print_header();

        printf(CYN BLD "\n  \xe2\x9a\x99\xef\xb8\x8f  Impostazioni\n" RST);

        /* ─── Backend AI ─────────────────────────────────────────────────── */
        printf(GRY "\n  \xe2\x94\x80\xe2\x94\x80\xe2\x94\x80 Backend AI"
               "  \xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
               "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
               "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
               "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
               "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
               "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
               "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\n" RST);

        int ao = (g_ctx.backend == BACKEND_OLLAMA);
        int al = (g_ctx.backend == BACKEND_LLAMA);
        int as = (g_ctx.backend == BACKEND_LLAMASERVER);

        /* ── 1. Ollama ──────────────────────────────────────────────────── */
        printf("\n  " EM_1 "  \xf0\x9f\x90\xa6  Ollama");
        if (ao) printf("  " GRN BLD "[\xe2\x9c\x93 ATTIVO]" RST);
        printf("\n");
        printf(GRY "       Parla via HTTP con il demone Ollama (deve girare in background)\n"
                   "       Avvia con: " YLW "ollama serve\n" RST);
        if (ao)
            printf(GRY "       " CYN "%s:%d" GRY "  \xe2\x80\x94  modello: " CYN "%s\n" RST,
                   g_ctx.host, g_ctx.port,
                   g_ctx.model[0] ? g_ctx.model : "(nessuno \xe2\x80\x94 premi M)");

        /* ── 2. llama.cpp statico ──────────────────────────────────────── */
        printf("\n  " EM_2 "  \xf0\x9f\xa6\x99  llama.cpp");
        if (al) printf("  " GRN BLD "[\xe2\x9c\x93 ATTIVO]" RST);
        printf("\n");
        printf(GRY "       Incorporato \xe2\x80\x94 nessun server esterno, nessuna porta di rete\n"
                   "       Carica il .gguf direttamente in RAM/VRAM del processo\n"
                   "       Richiede: " YLW "./build.sh" GRY " (una-tantum, ~10 min, compila llama.cpp)\n" RST);
        if (al) {
            const char *mn = lw_is_loaded() ? lw_model_name()
                           : (g_prof_llama_model[0] ? g_prof_llama_model
                                                     : "(nessuno \xe2\x80\x94 premi M)");
            printf(GRY "       Modello: " CYN "%s\n" RST, mn);
        }

        /* ── 3. llama-server ──────────────────────────────────────────── */
        printf("\n  " EM_3 "  \xf0\x9f\x96\xa5\xef\xb8\x8f  llama-server");
        if (as) printf("  " GRN BLD "[\xe2\x9c\x93 ATTIVO]" RST);
        printf("\n");
        printf(GRY "       Parla via HTTP socket con llama-server (API OpenAI-compatibile)\n"
                   "       Nessun Ollama, ma serve avviare il server manualmente\n"
                   "       Avvia con: " YLW "llama-server -m modello.gguf --port 8080\n" RST);
        if (as)
            printf(GRY "       " CYN "%s:%d" GRY "  \xe2\x80\x94  modello: " CYN "%s\n" RST,
                   g_ctx.host, g_ctx.port,
                   g_ctx.model[0] ? g_ctx.model : "(nessuno \xe2\x80\x94 premi M)");

        /* ─── Modello ─────────────────────────────────────────────────── */
        printf(GRY "\n  \xe2\x94\x80\xe2\x94\x80\xe2\x94\x80 Modello"
               "  \xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
               "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
               "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
               "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
               "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
               "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
               "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
               "\n" RST);
        {
            const char *mod =
                (g_ctx.backend == BACKEND_LLAMA)
                ? (lw_is_loaded() ? lw_model_name()
                                  : (g_prof_llama_model[0] ? g_prof_llama_model : "(nessuno)"))
                : (g_ctx.model[0] ? g_ctx.model : "(nessuno)");
            printf("\n  " BLD "M" RST "   \xf0\x9f\x8e\xaf  Cambia modello"
                   GRY "   \xe2\x80\x94  attuale: " CYN "%s\n" RST, mod);
        }

        /* ─── Strumenti avanzati ──────────────────────────────────────── */
        printf(GRY "\n  \xe2\x94\x80\xe2\x94\x80\xe2\x94\x80 Strumenti avanzati"
               "  \xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
               "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
               "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
               "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
               "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\n" RST);
        printf("\n");
        printf("  " EM_4 "  \xf0\x9f\x94\xac  VRAM Benchmark    "
               GRY "(profila occupazione VRAM per ogni modello)" RST "\n");
        printf("  " EM_5 "  \xf0\x9f\xa6\x99  llama.cpp Studio  "
               GRY "(compila, gestisci, avvia)" RST "\n");
        printf("  " EM_6 "  \xe2\x9a\xa1  Cython Studio     "
               GRY "(ottimizza codice Python)" RST "\n");
        printf("  " EM_7 "  \xf0\x9f\x93\xa6  Compila Windows   "
               GRY "(.exe NVIDIA/AMD/Intel/CPU)" RST "\n");
        printf("  " EM_8 "  \xf0\x9f\x96\xbc\xef\xb8\x8f  GUI Qt (Linux)    "
               GRY "(cmake + Qt6, nativo)" RST "\n");
        printf("\n  " EM_0 "  Torna\n\n  " GRN "Scelta: " RST); fflush(stdout);

        int c = getch_single(); printf("%c\n", c);

        /* ── Selezione backend inline ──────────────────────────────────── */
        if (c == '1') {
            _sync_to_profile();
            g_ctx.backend = BACKEND_OLLAMA;
            strncpy(g_ctx.host,  g_prof_ollama_host,  sizeof g_ctx.host  - 1);
            g_ctx.port = g_prof_ollama_port;
            strncpy(g_ctx.model, g_prof_ollama_model, sizeof g_ctx.model - 1);
            printf(GRN "  Backend: Ollama @ %s:%d\n" RST, g_ctx.host, g_ctx.port);
            printf(GRY "  Personalizzare host/porta? [s/N] " RST); fflush(stdout);
            char yn[8];
            if (fgets(yn, sizeof yn, stdin) && (yn[0]=='s'||yn[0]=='S')) {
                char buf[128];
                printf("  Host [%s]: ", g_ctx.host); fflush(stdout);
                if (fgets(buf, sizeof buf, stdin)) {
                    buf[strcspn(buf,"\n")]='\0';
                    if (buf[0]) strncpy(g_ctx.host, buf, sizeof g_ctx.host - 1);
                }
                printf("  Porta [%d]: ", g_ctx.port); fflush(stdout);
                if (fgets(buf, sizeof buf, stdin)) {
                    buf[strcspn(buf,"\n")]='\0';
                    if (buf[0]) {
                        long _p = strtol(buf, NULL, 10);
                        if (_p >= 1 && _p <= 65535) g_ctx.port = (int)_p;
                        else printf(YLW "  Porta non valida (1-65535).\n" RST);
                    }
                }
            }
            salva_config();
        }
        else if (c == '2') {
            _sync_to_profile();
            g_ctx.backend = BACKEND_LLAMA; g_ctx.port = 0;
            if (g_prof_llama_model[0])
                strncpy(g_ctx.model, g_prof_llama_model, sizeof g_ctx.model - 1);
            if (!lw_is_loaded()) {
                char gguf_files[MAX_MODELS][256];
                int nm = scan_gguf_dir(g_models_dir, gguf_files, MAX_MODELS);
                if (nm == 1) {
                    printf(GRN "  Auto-caricamento: %s\n" RST, gguf_files[0]);
                    load_gguf_model(gguf_files[0]);
                } else if (nm > 1) {
                    printf(YLW "  %d modelli trovati \xe2\x80\x94 premi M per scegliere.\n" RST, nm);
                    printf(GRY "  [INVIO] " RST); fflush(stdout);
                    { char _b[4]; if(!fgets(_b,sizeof _b,stdin)){} }
                } else {
                    printf(YLW "  Nessun .gguf in %s/ \xe2\x80\x94 aggiungi un modello con M.\n" RST, g_models_dir);
                    printf(GRY "  [INVIO] " RST); fflush(stdout);
                    { char _b[4]; if(!fgets(_b,sizeof _b,stdin)){} }
                }
            }
            salva_config();
        }
        else if (c == '3') {
            _sync_to_profile();
            g_ctx.backend = BACKEND_LLAMASERVER;
            strncpy(g_ctx.host,  g_prof_lserver_host,  sizeof g_ctx.host  - 1);
            g_ctx.port = g_prof_lserver_port;
            strncpy(g_ctx.model, g_prof_lserver_model, sizeof g_ctx.model - 1);
            printf(GRN "  Backend: llama-server @ %s:%d\n" RST, g_ctx.host, g_ctx.port);
            printf(GRY "  Personalizzare host/porta? [s/N] " RST); fflush(stdout);
            char yn[8];
            if (fgets(yn, sizeof yn, stdin) && (yn[0]=='s'||yn[0]=='S')) {
                char buf[128];
                printf("  Host [%s]: ", g_ctx.host); fflush(stdout);
                if (fgets(buf, sizeof buf, stdin)) {
                    buf[strcspn(buf,"\n")]='\0';
                    if (buf[0]) strncpy(g_ctx.host, buf, sizeof g_ctx.host - 1);
                }
                printf("  Porta [%d]: ", g_ctx.port); fflush(stdout);
                if (fgets(buf, sizeof buf, stdin)) {
                    buf[strcspn(buf,"\n")]='\0';
                    if (buf[0]) {
                        long _p = strtol(buf, NULL, 10);
                        if (_p >= 1 && _p <= 65535) g_ctx.port = (int)_p;
                        else printf(YLW "  Porta non valida (1-65535).\n" RST);
                    }
                }
            }
            salva_config();
        }
        else if (c == 'm' || c == 'M') menu_modelli();
        else if (c == '4') {
            clear_screen(); print_header();
            printf(CYN BLD "\n  \xf0\x9f\x94\xac  VRAM Benchmark\n\n" RST);
            printf("  Esegui dall'esterno:\n\n");
            printf(YLW "    ./vram_bench" RST "                          # profila tutti i modelli Ollama\n");
            printf(YLW "    ./vram_bench --model mistral:7b" RST "       # solo un modello\n");
            printf(YLW "    ./vram_bench --output profilo.json" RST "\n\n");
            printf(GRY "  Compila con: " YLW "make vram_bench\n\n" RST);
            printf(GRY "  [INVIO per continuare] " RST); fflush(stdout);
            { char _b[4]; if(!fgets(_b,sizeof _b,stdin)){} }
        }
        else if (c == '5' || c == '6') {
            clear_screen(); print_header();
            printf(YLW "\n  \xf0\x9f\x8c\xab\xef\xb8\x8f  La nebbia copre la verit\xc3\xa0. Riprova pi\xc3\xb9 tardi.\n\n" RST);
            printf(GRY "  (funzionalit\xc3\xa0 in sviluppo)\n\n" RST);
            printf(GRY "  [INVIO per continuare] " RST); fflush(stdout);
            { char _b[4]; if(!fgets(_b,sizeof _b,stdin)){} }
        }
        else if (c == '7') menu_compila_tui();
        else if (c == '8') menu_compila_qt();
        else if (c == '0' || c == 'q' || c == 'Q' || c == 27) break;
    }
}

/* ══════════════════════════════════════════════════════════════
   Voce 5 — Manutenzione
   ══════════════════════════════════════════════════════════════ */
static void menu_manutenzione(void) {
    while(1){
        clear_screen(); print_header();
        printf(CYN BLD "\n  \xf0\x9f\x94\xa7  Manutenzione\n\n" RST);
        {
            const char *modname = (g_ctx.backend==BACKEND_LLAMA)
                ? (lw_is_loaded()?lw_model_name():"nessun modello")
                : (g_ctx.model[0]?g_ctx.model:"nessun modello");
            printf("  " EM_1 "  Modelli    " GRY "attuale: %.50s" RST "\n", modname);
        }
        printf("  " EM_2 "  Info Hardware    " GRY "CPU, GPU, RAM, VRAM" RST "\n");
        printf("  " EM_0 "  Torna\n\n  " GRN "Scelta: " RST); fflush(stdout);
        int c=getch_single(); printf("%c\n",c);
        if     (c=='1') menu_modelli();
        else if(c=='2'){
            clear_screen(); print_header();
            printf(CYN BLD "\n  \xf0\x9f\x96\xa5\xef\xb8\x8f  Info Hardware\n\n" RST);
            hw_print(&g_hw);
            printf(GRY "\n  [INVIO per continuare] " RST); fflush(stdout);
            { char _b[4]; if(!fgets(_b,sizeof _b,stdin)){} }
        }
        else if(c=='0'||c=='q'||c=='Q'||c==27) break;
    }
}

/* ══════════════════════════════════════════════════════════════
   SEZIONE CONFIG — Salvataggio/caricamento impostazioni
   ══════════════════════════════════════════════════════════════ */

static char g_config_path[512] = "";

/* Ricostruisce il percorso config usando l'estensione corretta per il formato */
static void config_path_init(void) {
    const char *ext = (strcmp(g_config_fmt, "toon") == 0) ? "toon" : "json";
    const char *home = getenv("HOME");
#ifdef _WIN32
    if(!home) home = getenv("USERPROFILE");
#endif
    if(home)
        snprintf(g_config_path, sizeof g_config_path,
                 "%s/.prismalux_config.%s", home, ext);
    else
        snprintf(g_config_path, sizeof g_config_path,
                 ".prismalux_config.%s", ext);
}

void salva_config(void) {
    /* Rigenera il percorso (il formato potrebbe essere cambiato a runtime) */
    g_config_path[0] = '\0';
    config_path_init();
    FILE *f = fopen(g_config_path, "w"); if(!f) return;
    const char *bk =
        (g_ctx.backend == BACKEND_OLLAMA)      ? "ollama"       :
        (g_ctx.backend == BACKEND_LLAMASERVER) ? "llama-server" : "llama";
    /* Aggiorna profilo corrente prima di scrivere */
    _sync_to_profile();
    const char *llm = lw_is_loaded() ? lw_model_name()
                    : (g_prof_llama_model[0] ? g_prof_llama_model : "");

    if (strcmp(g_config_fmt, "toon") == 0) {
        /* ── Formato TOON ────────────────────────────────────────── */
        toon_write_comment(f, "Prismalux config — TOON (Token-Oriented Object Notation)");
        toon_write_comment(f, "https://github.com/toon-format/toon");
        fprintf(f, "\n");
        toon_write_str(f, "backend",      bk);
        fprintf(f, "\n");
        toon_write_comment(f, "Profilo Ollama");
        toon_write_str(f, "ollama_host",  g_prof_ollama_host);
        toon_write_int(f, "ollama_port",  g_prof_ollama_port);
        toon_write_str(f, "ollama_model", g_prof_ollama_model);
        fprintf(f, "\n");
        toon_write_comment(f, "Profilo llama-server");
        toon_write_str(f, "lserver_host",  g_prof_lserver_host);
        toon_write_int(f, "lserver_port",  g_prof_lserver_port);
        toon_write_str(f, "lserver_model", g_prof_lserver_model);
        fprintf(f, "\n");
        toon_write_comment(f, "Profilo llama-statico");
        toon_write_str(f, "llama_model", llm);
        fprintf(f, "\n");
        toon_write_comment(f, "Interfaccia");
        toon_write_str(f, "gui_path",     g_gui_path);
        toon_write_str(f, "config_fmt",   g_config_fmt);
    } else {
        /* ── Formato JSON (default) ──────────────────────────────── */
        char e_om[256]="", e_lsm[256]="", e_llm[256]="", e_gui[512]="";
        js_encode(g_prof_ollama_model,  e_om,  sizeof e_om);
        js_encode(g_prof_lserver_model, e_lsm, sizeof e_lsm);
        js_encode(llm,                  e_llm, sizeof e_llm);
        js_encode(g_gui_path,           e_gui, sizeof e_gui);
        fprintf(f,
            "{\n"
            "  \"backend\": \"%s\",\n"
            "  \"ollama_host\": \"%s\",\n"
            "  \"ollama_port\": %d,\n"
            "  \"ollama_model\": \"%s\",\n"
            "  \"lserver_host\": \"%s\",\n"
            "  \"lserver_port\": %d,\n"
            "  \"lserver_model\": \"%s\",\n"
            "  \"llama_model\": \"%s\",\n"
            "  \"gui_path\": \"%s\",\n"
            "  \"config_fmt\": \"%s\"\n"
            "}\n",
            bk,
            g_prof_ollama_host, g_prof_ollama_port, e_om,
            g_prof_lserver_host, g_prof_lserver_port, e_lsm,
            e_llm, e_gui, g_config_fmt);
    }
    fclose(f);
}

static void carica_config(void) {
    /* Prova prima il formato corrente, poi l'altro (migrazione automatica) */
    config_path_init();
    FILE *f = fopen(g_config_path, "r");
    if (!f) {
        /* prova l'estensione alternativa */
        char alt[512];
        const char *home = getenv("HOME");
#ifdef _WIN32
        if (!home) home = getenv("USERPROFILE");
#endif
        const char *alt_ext = (strcmp(g_config_fmt, "toon") == 0) ? "json" : "toon";
        if (home) snprintf(alt, sizeof alt, "%s/.prismalux_config.%s", home, alt_ext);
        else      snprintf(alt, sizeof alt, ".prismalux_config.%s", alt_ext);
        f = fopen(alt, "r");
        if (!f) return;
        /* rilevato formato alternativo: aggiorna g_config_fmt di conseguenza */
        strncpy(g_config_fmt, alt_ext, sizeof g_config_fmt - 1);
        g_config_path[0] = '\0';
        config_path_init();
    }
    char buf[4096]; int n=(int)fread(buf,1,sizeof buf-1,f); fclose(f);
    if(n<=0) return;
    buf[n]='\0';

    const char *v;
    char vbk[32]="", vtmp[512]="";

    if (strcmp(g_config_fmt, "toon") == 0) {
        /* ── Lettura TOON ─────────────────────────────────────── */
        toon_get(buf, "backend", vbk, sizeof vbk);

        v = toon_get(buf, "ollama_host",  vtmp, sizeof vtmp);
        if(v&&v[0]) strncpy(g_prof_ollama_host,  v, sizeof g_prof_ollama_host-1);
        v = toon_get(buf, "ollama_model", vtmp, sizeof vtmp);
        if(v&&v[0]) strncpy(g_prof_ollama_model, v, sizeof g_prof_ollama_model-1);
        { int p = toon_get_int(buf, "ollama_port", 0);
          if(p>=1&&p<=65535) g_prof_ollama_port=p; }

        v = toon_get(buf, "lserver_host",  vtmp, sizeof vtmp);
        if(v&&v[0]) strncpy(g_prof_lserver_host,  v, sizeof g_prof_lserver_host-1);
        v = toon_get(buf, "lserver_model", vtmp, sizeof vtmp);
        if(v&&v[0]) strncpy(g_prof_lserver_model, v, sizeof g_prof_lserver_model-1);
        { int p = toon_get_int(buf, "lserver_port", 0);
          if(p>=1&&p<=65535) g_prof_lserver_port=p; }

        v = toon_get(buf, "llama_model", vtmp, sizeof vtmp);
        if(v&&v[0]) strncpy(g_prof_llama_model, v, sizeof g_prof_llama_model-1);

        v = toon_get(buf, "gui_path",   vtmp, sizeof vtmp);
        if(v&&v[0]) strncpy(g_gui_path, v, sizeof g_gui_path-1);
        v = toon_get(buf, "config_fmt", vtmp, sizeof vtmp);
        if(v&&(strcmp(v,"json")==0||strcmp(v,"toon")==0))
            strncpy(g_config_fmt, v, sizeof g_config_fmt-1);
    } else {
        /* ── Lettura JSON (default) ──────────────────────────── */
        v=js_str(buf,"backend"); if(v) strncpy(vbk, v, sizeof vbk-1);

        v=js_str(buf,"ollama_host");  if(v&&v[0]) strncpy(g_prof_ollama_host, v, sizeof g_prof_ollama_host-1);
        v=js_str(buf,"ollama_model"); if(v&&v[0]) strncpy(g_prof_ollama_model,v, sizeof g_prof_ollama_model-1);
        { const char *pp=strstr(buf,"\"ollama_port\":");
          if(pp){ pp+=14; while(*pp==' ')pp++;
                  long _p=strtol(pp,NULL,10); if(_p>=1&&_p<=65535) g_prof_ollama_port=(int)_p; } }

        v=js_str(buf,"lserver_host");  if(v&&v[0]) strncpy(g_prof_lserver_host, v, sizeof g_prof_lserver_host-1);
        v=js_str(buf,"lserver_model"); if(v&&v[0]) strncpy(g_prof_lserver_model,v, sizeof g_prof_lserver_model-1);
        { const char *pp=strstr(buf,"\"lserver_port\":");
          if(pp){ pp+=15; while(*pp==' ')pp++;
                  long _p=strtol(pp,NULL,10); if(_p>=1&&_p<=65535) g_prof_lserver_port=(int)_p; } }

        v=js_str(buf,"llama_model"); if(v&&v[0]) strncpy(g_prof_llama_model, v, sizeof g_prof_llama_model-1);
        v=js_str(buf,"gui_path");   if(v&&v[0]) strncpy(g_gui_path, v, sizeof g_gui_path-1);
        v=js_str(buf,"config_fmt");
        if(v&&(strcmp(v,"json")==0||strcmp(v,"toon")==0))
            strncpy(g_config_fmt, v, sizeof g_config_fmt-1);
    }

    /* ── Ripristina g_ctx dal backend attivo ── */
    if(vbk[0]){
        if     (strcmp(vbk,"ollama"      )==0) g_ctx.backend=BACKEND_OLLAMA;
        else if(strcmp(vbk,"llama-server")==0) g_ctx.backend=BACKEND_LLAMASERVER;
        else                                    g_ctx.backend=BACKEND_LLAMA;
    }
    if(g_ctx.backend==BACKEND_OLLAMA){
        strncpy(g_ctx.host,  g_prof_ollama_host,  sizeof g_ctx.host-1);
        g_ctx.port = g_prof_ollama_port;
        strncpy(g_ctx.model, g_prof_ollama_model, sizeof g_ctx.model-1);
    } else if(g_ctx.backend==BACKEND_LLAMASERVER){
        strncpy(g_ctx.host,  g_prof_lserver_host,  sizeof g_ctx.host-1);
        g_ctx.port = g_prof_lserver_port;
        strncpy(g_ctx.model, g_prof_lserver_model, sizeof g_ctx.model-1);
    } else {
        g_ctx.port = 0;
        strncpy(g_ctx.model, g_prof_llama_model, sizeof g_ctx.model-1);
    }
}

/* ══════════════════════════════════════════════════════════════
   SEZIONE 12 — Menu principale
   ══════════════════════════════════════════════════════════════ */

static void menu_principale(void) {
    while(1){
        _sync_hdr_ctx();
        clear_screen();
        print_header();

        int tw = term_width();
        int W  = tw - 2;
        if(W < 60)  W = 60;
        if(W > 130) W = 130;

        /* Macro locale per una riga del menu principale:
         * key=numero (EM_1..EM_5), icon=UTF-8 emoji, nome=etichetta, desc=sottotitolo */
#define MROW(key,icon,nome,desc) do { \
    char _b[320]; \
    snprintf(_b,sizeof _b, " %s  %s  " BLD "%-30s" RST "  " GRY "%s" RST, \
             key,icon,nome,desc); \
    box_lft(W,_b); \
} while(0)

        box_top(W);

        /* ── Voci menu ───────────────────────────────────────── */
        box_emp(W);
        MROW(EM_1, "\xf0\x9f\xa4\x96", "Agenti AI",
             "Pipeline 6 agenti | Motore Byzantino anti-allucinazione");
        MROW(EM_2, "\xf0\x9f\x9b\xa0\xef\xb8\x8f", "Strumento Pratico",
             "730, Partita IVA, Cerca Lavoro, CV Reader");
        MROW(EM_3, "\xf0\x9f\x93\x9a", "Impara con AI",
             "Tutor Oracolo, Quiz interattivi, Dashboard, Analisi dati");
        MROW(EM_4, "\xe2\x9a\x99\xef\xb8\x8f", "Impostazioni",
             "Backend AI, Modello, VRAM Bench, llama.cpp Studio, Cython");
        MROW(EM_5, "\xf0\x9f\x94\xa7", "Manutenzione",
             "Modelli, Info Hardware");
        box_emp(W);

        /* ── avviso RAM critica (se <512 MB liberi) ──────────── */
        {
#ifdef _WIN32
            MEMORYSTATUSEX ms; ms.dwLength = sizeof ms;
            GlobalMemoryStatusEx(&ms);
            long long ra = (long long)(ms.ullAvailPhys / (1024ULL * 1024ULL));
#else
            long long ra = 0;
            FILE* mf = fopen("/proc/meminfo", "r");
            if(mf) {
                char line[256];
                while(fgets(line, sizeof line, mf)) {
                    unsigned long long v;
                    if(sscanf(line, "MemAvailable: %llu", &v) == 1) { ra = (long long)(v/1024); break; }
                }
                fclose(mf);
            }
#endif
            if(ra>0 && ra<512){
                char w[160];
                snprintf(w,sizeof w,
                    " " RED "\xe2\x9a\xa0" RST "  RAM libera: %lld MB  \xe2\x80\x94  "
                    "scarica il modello corrente!", ra);
                box_lft(W,w);
            }
        }

        /* ── barra stato in basso ────────────────────────────── */
        box_sep(W);
        {
            const char *footer =
                "  " YLW "[Q]" RST " \xf0\x9f\x9a\xaa Esci  "
                "\xe2\x94\x82  " YLW "[ESC]" RST " esci subito  "
                "\xe2\x94\x82  Premi " EM_1 "\xe2\x80\x93" EM_5 " per selezionare  ";
            int dl=disp_len(footer);
            int pad=W-dl; if(pad<0)pad=0;
            printf(CYN "\xe2\x95\x91" RST "\033[7m%s",footer);
            for(int i=0;i<pad;i++) putchar(' ');
            printf("\033[27m" CYN "\xe2\x95\x91\n" RST);
        }
        box_bot(W);

        printf("\n"); fflush(stdout);

#undef MROW
        int c=getch_single();

        if(c==27){ /* ESC = esci subito */
            salva_config();
            clear_screen();
            printf(CYN BLD "\n  \xf0\x9f\x8d\xba Alla prossima libagione di sapere.\n\n" RST);
            return;
        }
        switch(c){
            case '1': menu_agenti();             break;
            case '2': menu_pratico();            break;
            case '3': menu_impara();             break;
            case '4': menu_impostazioni();        break;
            case '5': menu_manutenzione();       break;
            case 'q': case 'Q':
                salva_config();
                clear_screen();
                printf(CYN BLD "\n  \xf0\x9f\x8d\xba Alla prossima libagione di sapere.\n\n" RST);
                return;
        }
    }
}

static void sigint_handler(int sig) {
    (void)sig;
    salva_config();
#ifndef _WIN32
    term_restore();
#endif
    printf(RST "\n  " GRY "[Ctrl+C] Configurazione salvata. Uscita.\n" RST);
    exit(0);
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    /* Abilita ANSI escape codes e forza UTF-8 nella console Windows */
    HANDLE hOut=GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode=0; GetConsoleMode(hOut,&mode);
    SetConsoleMode(hOut,mode|ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    SetConsoleOutputCP(65001);   /* CP_UTF8 — emoji e caratteri speciali */
    SetConsoleCP(65001);

    /* Auto-rileva path GUI: Prismalux_GUI.exe nella stessa cartella dell'exe */
    {
        char exe_path[512] = "";
        if (GetModuleFileNameA(NULL, exe_path, sizeof(exe_path))) {
            char *sep = strrchr(exe_path, '\\');
            if (sep) {
                sep[1] = '\0';   /* tronca al separatore → directory exe */
                snprintf(g_gui_path, sizeof(g_gui_path),
                         "%sPrismalux_GUI.exe", exe_path);
            }
        }
    }
#endif

    signal(SIGINT, sigint_handler);

    /* ── Carica impostazioni salvate (le args la sovrascrivo) ─── */
    carica_config();

    /* ── Parsing argomenti ────────────────────────────────────── */
    for(int i=1;i<argc;i++){
        if(strcmp(argv[i],"--backend")==0&&i+1<argc){
            i++;
            if(strcmp(argv[i],"ollama")==0){
                g_ctx.backend=BACKEND_OLLAMA;
                if(!g_ctx.port)g_ctx.port=11434;
            }else if(strcmp(argv[i],"llama-server")==0){
                g_ctx.backend=BACKEND_LLAMASERVER;
                if(!g_ctx.port)g_ctx.port=8080;
            }else if(strcmp(argv[i],"llama")==0){
                g_ctx.backend=BACKEND_LLAMA;
            }else{
                fprintf(stderr,"Backend non valido: %s\n(usa: llama | ollama | llama-server)\n",argv[i]);
                return 1;
            }
        }
        else if(strcmp(argv[i],"--host")==0&&i+1<argc){
            strncpy(g_ctx.host,argv[++i],sizeof g_ctx.host-1);
        }
        else if(strcmp(argv[i],"--port")==0&&i+1<argc){
            long _p = strtol(argv[++i], NULL, 10);
            if (_p >= 1 && _p <= 65535) g_ctx.port = (int)_p;
            else { fprintf(stderr,"Porta non valida: %s (1-65535)\n", argv[i]); return 1; }
        }
        else if(strcmp(argv[i],"--model")==0&&i+1<argc){
            strncpy(g_ctx.model,argv[++i],sizeof g_ctx.model-1);
        }
        else if(strcmp(argv[i],"--models-dir")==0&&i+1<argc){
            strncpy(g_models_dir,argv[++i],sizeof g_models_dir-1);
        }
        else if(strcmp(argv[i],"--gui-path")==0&&i+1<argc){
            strncpy(g_gui_path,argv[++i],sizeof g_gui_path-1);
        }
        else if(strcmp(argv[i],"--config-format")==0&&i+1<argc){
            const char *fmt=argv[++i];
            if(strcmp(fmt,"json")==0||strcmp(fmt,"toon")==0){
                strncpy(g_config_fmt,fmt,sizeof g_config_fmt-1);
                g_config_path[0]='\0'; /* forza ricalcolo percorso */
            }else{
                fprintf(stderr,"Formato config non valido: %s (usa: json | toon)\n",fmt);
                return 1;
            }
        }
        else if(strcmp(argv[i],"--help")==0||strcmp(argv[i],"-h")==0){
            printf("Prismalux v" VERSION "\n\n"
                   "Uso: %s [opzioni]\n\n"
                   "  --backend   llama|ollama|llama-server\n"
                   "  --host      HOST  (default: 127.0.0.1)\n"
                   "  --port      PORT  (ollama: 11434, llama-server: 8080)\n"
                   "  --model     NOME  (es: llama3.2, mistral)\n"
                   "  --models-dir DIR  (default: ./models)\n"
                   "  --gui-path      PATH        (default: Qt_GUI/build/Prismalux_GUI)\n"
                   "  --config-format json|toon   (default: json)\n\n"
                   "Esempi:\n"
                   "  %s --backend ollama\n"
                   "  %s --backend llama-server --port 8080\n"
                   "  %s --backend ollama --model deepseek-coder:33b\n",
                   argv[0],argv[0],argv[0],argv[0]);
            return 0;
        }
    }

    /* ── Porta di default se non specificata ─────────────────── */
    if(g_ctx.port==0){
        g_ctx.port=(g_ctx.backend==BACKEND_LLAMASERVER)?8080:11434;
    }

    clear_screen();
    printf(CYN BLD "\n  \xf0\x9f\x8d\xba Invocazione riuscita. Gli dei ascoltano.\n\n" RST);
    printf(DIM "  Backend: %s", backend_name(g_ctx.backend));
    if(g_ctx.backend!=BACKEND_LLAMA) printf("  \xe2\x86\x92  %s:%d",g_ctx.host,g_ctx.port);
    printf("\n" RST);

    /* ── Init backend ────────────────────────────────────────── */
    if(g_ctx.backend==BACKEND_LLAMA){
        char gguf_files[MAX_MODELS][256];
        int nm=scan_gguf_dir(g_models_dir,gguf_files,MAX_MODELS);
        if(nm>0){
            printf(GRY "  Trovati %d modello/i in %s/\n" RST,nm,g_models_dir);
            if(nm==1){
                printf(GRY "  Auto-caricamento: %s\n" RST,gguf_files[0]);
                load_gguf_model(gguf_files[0]);
            }else{
                printf(GRY "  Usa Opzione 4 per scegliere il modello.\n" RST);
            }
        }else{
            printf(YLW "  Nessun .gguf in %s/\n" RST,g_models_dir);
            printf(DIM "  Usa --backend ollama per avvio senza compilazione.\n" RST);
        }
    }else{
        /* HTTP: testa connettivita' */
        sock_t test=tcp_connect(g_ctx.host,g_ctx.port);
        if(test!=SOCK_INV){
            printf(GRN "  \xe2\x9c\x93 Connesso a %s:%d\n" RST,g_ctx.host,g_ctx.port);
            sock_close(test);
            /* Auto-popola lista modelli */
            char http_models[MAX_MODELS][128]; int nm=0;
            if(g_ctx.backend==BACKEND_OLLAMA)
                nm=http_ollama_list(g_ctx.host,g_ctx.port,http_models,MAX_MODELS);
            else
                nm=http_llamaserver_list(g_ctx.host,g_ctx.port,http_models,MAX_MODELS);
            if(nm>0){
                printf(GRY "  Modelli disponibili: %d\n" RST,nm);
                if(!g_ctx.model[0]){
                    /* Preferenza: qwen2.5-coder > qwen > deepseek-coder > primo disponibile */
                    const char* pref[] = {"qwen2.5-coder","qwen2.5","qwen","deepseek-coder","codellama",NULL};
                    int found=0;
                    for(int pi=0; pref[pi] && !found; pi++){
                        for(int mi=0; mi<nm && !found; mi++){
                            if(strstr(http_models[mi], pref[pi])){
                                snprintf(g_ctx.model, sizeof g_ctx.model, "%s", http_models[mi]);
                                found=1;
                            }
                        }
                    }
                    if(!found)
                        snprintf(g_ctx.model, sizeof g_ctx.model, "%s", http_models[0]);
                }
                printf(GRY "  Modello attivo: %s\n" RST,g_ctx.model);
            }
        }else{
            printf(YLW "  \xe2\x9a\xa0  Impossibile connettersi a %s:%d\n" RST,g_ctx.host,g_ctx.port);
            if(g_ctx.backend==BACKEND_OLLAMA)
                printf(DIM "  Avvia Ollama con: ollama serve\n" RST);
            else
                printf(DIM "  Avvia llama-server con: llama-server -m model.gguf\n" RST);
        }
    }

    /* ── Rilevamento hardware ── */
    printf(CYN "  \xe2\x9a\x99  Rilevamento hardware...\n" RST); fflush(stdout);
    hw_detect(&g_hw);
    g_hw_ready = 1;
    hw_print(&g_hw);
    printf("\n");
    /* Auto-configura n_gpu_layers per llama backend */
    if(g_hw.primary >= 0 && g_hw.dev[g_hw.primary].type != DEV_CPU)
        printf(GRN "  \xe2\x9c\x93 GPU primaria: %s (%lld MB VRAM usabile, layers=%d)\n" RST,
               g_hw.dev[g_hw.primary].name,
               g_hw.dev[g_hw.primary].avail_mb,
               g_hw.dev[g_hw.primary].n_gpu_layers);
    else {
        /* Nessuna GPU dedicata rilevata: controlla se e' uno Xeon (nessun allarme) */
        const char* cpu_name = g_hw.dev[0].name;
        int is_xeon = (strstr(cpu_name, "Xeon") != NULL ||
                       strstr(cpu_name, "xeon") != NULL ||
                       strstr(cpu_name, "XEON") != NULL);
        if (is_xeon)
            printf(GRN "  \xe2\x9c\x93 Xeon rilevato \xe2\x80\x94 elaborazione CPU ad alta performance\n" RST);
        else
            printf(YLW "  \xe2\x9a\xa0  Nessuna GPU dedicata \xe2\x80\x94 elaborazione su CPU\n" RST);
    }
    /* ── Scelta interfaccia: GUI (default) o CLI ─────────────── */
#ifdef _WIN32
    int gui_ok = (_access(g_gui_path, 0) == 0);  /* 0 = F_OK: file esiste */
#else
    int gui_ok = (access(g_gui_path, X_OK) == 0);
#endif

    if (gui_ok) {
        printf(CYN BLD "\n  Scegli interfaccia:\n\n" RST);
        printf("  " GRN "[INVIO]" RST "  Grafica Qt6  " GRY "(GUI — default)\n" RST);
        printf("  " YLW "[c]" RST "      Testuale TUI " GRY "(CLI)\n\n" RST);
        printf(GRY "  GUI: %s\n" RST, g_gui_path);
        printf(GRN "  Scelta: " RST); fflush(stdout);
        int ch = getch_single();
        if (ch == 'c' || ch == 'C') {
            printf("CLI\n\n");
            /* continua al menu testuale */
        } else {
            printf("GUI\n\n");
            printf(GRY "  Avvio GUI...\n" RST); fflush(stdout);
#ifdef _WIN32
            /* Su Windows usa ShellExecute invece di execl (POSIX) */
            ShellExecuteA(NULL, "open", g_gui_path, NULL, NULL, SW_SHOWNORMAL);
#else
            execl(g_gui_path, g_gui_path, NULL);
#endif
            /* avvio GUI fallito: fallback CLI */
            printf(YLW "  GUI non avviabile — avvio CLI.\n\n" RST);
        }
    } else {
        /* GUI non trovata: mostra percorso per diagnostica */
        printf(GRY "\n  GUI non trovata: %s\n" RST, g_gui_path);
        printf(GRY "  (configura con --gui-path o nel menu Personalizzazione)\n" RST);
        printf(GRY "  [INVIO per continuare in CLI] " RST); fflush(stdout);
        { char _b[4]; if(!fgets(_b, sizeof _b, stdin)){} }
    }

    menu_principale();
    lw_free();
    return 0;
}
