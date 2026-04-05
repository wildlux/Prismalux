/*
 * sim_common.h — Helper condivisi per il Simulatore Algoritmi
 * ============================================================
 * Dichiarazioni di tipi, costanti e funzioni helper usate da tutti
 * i moduli sim_xxx.c. Include anche le forward declarations di tutte
 * le funzioni di simulazione.
 */
#pragma once
#include "common.h"
#include "backend.h"
#include "terminal.h"
#include "ai.h"
#include "prismalux_ui.h"
#include <time.h>
#include <stdlib.h>
#include <math.h>

#define SIM_PI  3.14159265358979323846
#define LOG_MAX 6
#define BAR_MAX 42   /* larghezza massima barra (in caratteri █) */

/* ── Stato condiviso del log (definito in sim_common.c) ───────────────── */
extern char _sim_log[LOG_MAX][128];
extern int  _sim_log_n;

/* ── Helper condivisi ─────────────────────────────────────────────────── */
void sim_log_reset(void);
void sim_log_push(const char *msg);
int  sim_aspetta_passo(void);
void sim_mostra_array(const int *arr, int n, int c1, int c2, int s1, int s2,
                      int trovato, unsigned long long ordinati,
                      const char *msg, const char *titolo);
void sim_genera_array(int *arr, int n, int vmin, int vmax);
void sim_chiedi_ai(const char *nome_algo);
void sim_fine(const char *nome);

/* ── Forward declarations: Ordinamento (15) ───────────────────────────── */
int sim_bubble_sort(void);
int sim_selection_sort(void);
int sim_insertion_sort(void);
int sim_merge_sort(void);
int sim_quick_sort(void);
int sim_heap_sort(void);
int sim_shell_sort(void);
int sim_cocktail_sort(void);
int sim_counting_sort(void);
int sim_gnome_sort(void);
int sim_comb_sort(void);
int sim_pancake_sort(void);
int sim_radix_sort(void);
int sim_bucket_sort(void);
int sim_tim_sort(void);

/* ── Forward declarations: Ricerca (4) ────────────────────────────────── */
int sim_binary_search(void);
int sim_ricerca_lineare(void);
int sim_jump_search(void);
int sim_interpolation_search(void);

/* ── Forward declarations: Matematica (10) ────────────────────────────── */
int sim_gcd_euclide(void);
int sim_crivo(void);
int sim_potenza_modulare(void);
int sim_triangolo_pascal(void);
int sim_monte_carlo_pi(void);
int sim_fibonacci_dp(void);
int sim_torre_hanoi(void);
int sim_fib_ricorsivo(void);
int sim_miller_rabin(void);
int sim_horner_ruffini(void);

/* ── Forward declarations: Programmazione Dinamica (5) ────────────────── */
int sim_zaino(void);
int sim_lcs(void);
int sim_coin_change(void);
int sim_edit_distance(void);
int sim_lis(void);

/* ── Forward declarations: Grafi (7) ──────────────────────────────────── */
int sim_bfs_griglia(void);
int sim_dijkstra(void);
int sim_dfs_griglia(void);
int sim_topo_sort(void);
int sim_bellman_ford(void);
int sim_floyd_warshall(void);
int sim_kruskal(void);

/* ── Forward declarations: Tecniche + Visualizzazioni + Fisica + Chimica ─ */
int sim_two_pointers(void);
int sim_sliding_window(void);
int sim_bit_manipulation(void);
int sim_game_of_life(void);
int sim_sierpinski(void);
int sim_rule30(void);
int sim_caduta_libera(void);
int sim_ph(void);
int sim_gas_ideali(void);
int sim_stechiometria(void);

/* ── Forward declarations: Stringhe (3) ───────────────────────────────── */
int sim_kmp_search(void);
int sim_rabin_karp(void);
int sim_z_algorithm(void);

/* ── Forward declarations: Strutture Dati (5) ─────────────────────────── */
int sim_stack_queue(void);
int sim_linked_list(void);
int sim_bst(void);
int sim_hash_table(void);
int sim_min_heap(void);

/* ── Forward declarations: Greedy (3) ─────────────────────────────────── */
int sim_activity_selection(void);
int sim_zaino_frazionario(void);
int sim_huffman(void);

/* ── Forward declarations: Backtracking (2) ───────────────────────────── */
int sim_n_queens(void);
int sim_sudoku(void);

/* ── Forward declarations: LLM/AI (2) ────────────────────────────────── */
int sim_entropia_pesi(void);
int sim_gptq_sparsegpt(void);
