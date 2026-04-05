/* ══════════════════════════════════════════════════════════════════════════
   test_sim_smoke.c — Smoke test per tutte le 63 simulazioni del Simulatore
   ══════════════════════════════════════════════════════════════════════════
   Esegue ogni simulazione in un processo figlio isolato:
     - stdin  → /dev/null  (getch_single ritorna EOF → sim_aspetta_passo = 0)
     - stdout → pipe       (cattura output)
     - stderr → /dev/null
   Criteri di pass:
     [1] Il figlio termina con exit(0) — nessun crash / segfault
     [2] Output ≥ 10 caratteri        — la simulazione ha scritto qualcosa
     [3] Completamento entro 5 secondi

   Build (aggiunto nel Makefile come target test_sim_smoke):
     gcc -O2 -Iinclude -o test_sim_smoke test_sim_smoke.c \
         src/sim_common.c src/sim_sort.c src/sim_search.c src/sim_math.c \
         src/sim_dp.c src/sim_grafi.c src/sim_tech.c src/sim_stringhe.c \
         src/sim_strutture.c src/sim_greedy.c src/sim_backtrack.c src/sim_llm.c \
         src/terminal.c src/backend.c src/prismalux_ui.c src/hw_detect.c \
         src/http.c src/ai.c src/agent_scheduler.c -lpthread -lm

   Esecuzione: ./test_sim_smoke
   ══════════════════════════════════════════════════════════════════════════ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <sys/time.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

#include "include/sim_common.h"

/* ── Harness (colori da prismalux_ui.h via sim_common.h) ─────────────── */

static int _pass = 0, _fail = 0;
#define SECTION(t)  printf("\n" CYN BLD "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80 %s \xe2\x94\x80\xe2\x94\x80\xe2\x94\x80" RST "\n", t)
#define PASS(label, chars) do { \
    printf("  " GRN "[OK]" RST "   %-40s (%d chars)\n", label, chars); _pass++; } while(0)
#define FAIL(label, reason) do { \
    printf("  " RED "[FAIL]" RST " %-40s %s\n", label, reason); _fail++; } while(0)

/* ── Esegui una simulazione isolata ─────────────────────────────────────
   fn   — puntatore alla funzione sim_xxx(void) → int
   name — nome leggibile per il report
   ──────────────────────────────────────────────────────────────────────── */
#define TIMEOUT_SEC 5
#define MAX_READ    65536   /* 64 KB — più che sufficiente per qualsiasi sim */

static void run_sim_isolated(int (*fn)(void), const char *name)
{
    int pipefd[2];
    if (pipe(pipefd) < 0) { FAIL(name, "pipe() fallita"); return; }

    pid_t pid = fork();
    if (pid < 0) { close(pipefd[0]); close(pipefd[1]); FAIL(name, "fork() fallita"); return; }

    if (pid == 0) {
        /* ── Figlio: isola I/O e lancia la simulazione ── */
        int devnull_r = open("/dev/null", O_RDONLY);
        int devnull_w = open("/dev/null", O_WRONLY);

        if (devnull_r >= 0) dup2(devnull_r, STDIN_FILENO);
        dup2(pipefd[1], STDOUT_FILENO);
        if (devnull_w >= 0) dup2(devnull_w, STDERR_FILENO);

        close(pipefd[0]);
        close(pipefd[1]);
        if (devnull_r >= 0) close(devnull_r);
        if (devnull_w >= 0) close(devnull_w);

        /* Salta il nanosleep(200ms) in cpu_percent() — evita timeout per step */
        g_sim_headless = 1;

        /* Ignora SIGPIPE: se il genitore chiude la pipe prima di noi, non crashare */
        signal(SIGPIPE, SIG_IGN);

        fn();          /* esegui simulazione */
        fflush(stdout);
        _exit(0);
    }

    /* ── Genitore: leggi output con timeout ── */
    close(pipefd[1]);

    char *buf = malloc(MAX_READ + 1);
    if (!buf) { kill(pid, SIGKILL); waitpid(pid, NULL, 0); close(pipefd[0]); FAIL(name, "malloc fallita"); return; }

    int total   = 0;
    int timed   = 0;
    struct timeval deadline;
    gettimeofday(&deadline, NULL);
    deadline.tv_sec += TIMEOUT_SEC;

    while (total < MAX_READ) {
        struct timeval now;
        gettimeofday(&now, NULL);

        long rem_us = (deadline.tv_sec - now.tv_sec) * 1000000L
                    + (deadline.tv_usec - now.tv_usec);
        if (rem_us <= 0) { timed = 1; break; }

        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(pipefd[0], &rfds);
        struct timeval tv = { rem_us / 1000000L, rem_us % 1000000L };
        int ret = select(pipefd[0] + 1, &rfds, NULL, NULL, &tv);
        if (ret < 0 && errno == EINTR) continue;
        if (ret <= 0) { timed = 1; break; }

        ssize_t n = read(pipefd[0], buf + total, (size_t)(MAX_READ - total));
        if (n <= 0) break;  /* pipe chiusa (figlio ha terminato) */
        total += (int)n;
    }
    close(pipefd[0]);

    if (timed) {
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
        free(buf);
        FAIL(name, "(timeout >5s)");
        return;
    }

    /* Attendi la terminazione del figlio */
    int status = 0;
    waitpid(pid, &status, 0);
    free(buf);

    if (!WIFEXITED(status)) {
        char reason[64];
        snprintf(reason, sizeof reason, "(terminato da segnale %d)", WTERMSIG(status));
        FAIL(name, reason);
        return;
    }
    if (WEXITSTATUS(status) != 0) {
        char reason[64];
        snprintf(reason, sizeof reason, "(exit %d)", WEXITSTATUS(status));
        FAIL(name, reason);
        return;
    }
    if (total < 10) {
        char reason[64];
        snprintf(reason, sizeof reason, "(output solo %d byte)", total);
        FAIL(name, reason);
        return;
    }

    PASS(name, total);
}

/* ── Macro di comodo per registrare ogni simulazione ──────────────────── */
#define SIM(fn)  run_sim_isolated(fn, #fn)

/* ══════════════════════════════════════════════════════════════════════════
   main — esegue tutte le 63 simulazioni
   ══════════════════════════════════════════════════════════════════════════ */
int main(void)
{
    printf(CYN BLD "\n  Smoke Test Simulatore Algoritmi — 63 simulazioni\n" RST);
    printf(GRY "  stdin=/dev/null  stdout=pipe  timeout=5s\n" RST);

    /* 1. Ordinamento (15) */
    SECTION("Ordinamento (15)");
    SIM(sim_bubble_sort);
    SIM(sim_selection_sort);
    SIM(sim_insertion_sort);
    SIM(sim_merge_sort);
    SIM(sim_quick_sort);
    SIM(sim_heap_sort);
    SIM(sim_shell_sort);
    SIM(sim_cocktail_sort);
    SIM(sim_counting_sort);
    SIM(sim_gnome_sort);
    SIM(sim_comb_sort);
    SIM(sim_pancake_sort);
    SIM(sim_radix_sort);
    SIM(sim_bucket_sort);
    SIM(sim_tim_sort);

    /* 2. Ricerca (4) */
    SECTION("Ricerca (4)");
    SIM(sim_binary_search);
    SIM(sim_ricerca_lineare);
    SIM(sim_jump_search);
    SIM(sim_interpolation_search);

    /* 3. Matematica (10) */
    SECTION("Matematica (10)");
    SIM(sim_gcd_euclide);
    SIM(sim_crivo);
    SIM(sim_potenza_modulare);
    SIM(sim_triangolo_pascal);
    SIM(sim_monte_carlo_pi);
    SIM(sim_fibonacci_dp);
    SIM(sim_torre_hanoi);
    SIM(sim_fib_ricorsivo);
    SIM(sim_miller_rabin);
    SIM(sim_horner_ruffini);

    /* 4. Programmazione Dinamica (5) */
    SECTION("Programmazione Dinamica (5)");
    SIM(sim_zaino);
    SIM(sim_lcs);
    SIM(sim_coin_change);
    SIM(sim_edit_distance);
    SIM(sim_lis);

    /* 5. Grafi (7) */
    SECTION("Grafi (7)");
    SIM(sim_bfs_griglia);
    SIM(sim_dijkstra);
    SIM(sim_dfs_griglia);
    SIM(sim_topo_sort);
    SIM(sim_bellman_ford);
    SIM(sim_floyd_warshall);
    SIM(sim_kruskal);

    /* 6. Tecniche + Visualizzazioni + Fisica + Chimica (10) */
    SECTION("Tecniche + Visualizzazioni + Fisica + Chimica (10)");
    SIM(sim_two_pointers);
    SIM(sim_sliding_window);
    SIM(sim_bit_manipulation);
    SIM(sim_game_of_life);
    SIM(sim_sierpinski);
    SIM(sim_rule30);
    SIM(sim_caduta_libera);
    SIM(sim_ph);
    SIM(sim_gas_ideali);
    SIM(sim_stechiometria);

    /* 7. Stringhe (3) */
    SECTION("Stringhe (3)");
    SIM(sim_kmp_search);
    SIM(sim_rabin_karp);
    SIM(sim_z_algorithm);

    /* 8. Strutture Dati (5) */
    SECTION("Strutture Dati (5)");
    SIM(sim_stack_queue);
    SIM(sim_linked_list);
    SIM(sim_bst);
    SIM(sim_hash_table);
    SIM(sim_min_heap);

    /* 9. Greedy (3) */
    SECTION("Greedy (3)");
    SIM(sim_activity_selection);
    SIM(sim_zaino_frazionario);
    SIM(sim_huffman);

    /* 10. Backtracking (2) */
    SECTION("Backtracking (2)");
    SIM(sim_n_queens);
    SIM(sim_sudoku);

    /* 11. LLM/AI (2) */
    SECTION("LLM/AI (2)");
    SIM(sim_entropia_pesi);
    SIM(sim_gptq_sparsegpt);

    /* ── Riepilogo ── */
    int total = _pass + _fail;
    printf("\n" CYN BLD "══════════════════════════════════════\n" RST);
    printf(BLD "  Simulazioni: %d/%d pass" RST "\n", _pass, total);
    if (_fail == 0)
        printf(GRN BLD "  Tutti i test superati!\n" RST);
    else
        printf(RED BLD "  %d test falliti.\n" RST, _fail);
    printf(CYN BLD "══════════════════════════════════════\n\n" RST);

    return (_fail == 0) ? 0 : 1;
}
