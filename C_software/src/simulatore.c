/*
 * simulatore.c — Menu principale del Simulatore Algoritmi
 * =========================================================
 * Contiene solo la struttura AlgoritmoSim, gli array _ALGOS_xxx[]
 * e la funzione menu_simulatore(). Le implementazioni sono nei moduli
 * sim_sort.c, sim_search.c, sim_math.c, sim_dp.c, sim_grafi.c,
 * sim_tech.c, sim_stringhe.c, sim_strutture.c, sim_greedy.c,
 * sim_backtrack.c e sim_common.c.
 */
#include "simulatore.h"
#include "sim_common.h"

/* ════════════════════════════════════════════════════════════════════════
   STRUTTURA MENU SIMULATORE
   ════════════════════════════════════════════════════════════════════════ */

typedef struct {
    const char *icona;
    const char *nome;
    const char *desc;
    int (*sim_fn)(void);   /* NULL = solo spiegazione AI */
} AlgoritmoSim;

/* ── ORDINAMENTO (15 algoritmi) ─────────────────────────────────────── */
static const AlgoritmoSim _ALGOS_SORT[] = {
    { "\xf0\x9f\x92\xa8", "Bubble Sort",     "confronto vicini, scambia — O(n\xc2\xb2)",            sim_bubble_sort    },
    { "\xf0\x9f\x8e\xaf", "Selection Sort",  "trova minimo, metti in testa — O(n\xc2\xb2)",          sim_selection_sort },
    { "\xf0\x9f\x83\x8f", "Insertion Sort",  "inserisci nel posto giusto — O(n\xc2\xb2)/O(n) best",  sim_insertion_sort },
    { "\xf0\x9f\x94\x80", "Merge Sort",      "dividi a met\xc3\xa0, fondi — O(n log n), stabile",    sim_merge_sort     },
    { "\xe2\x9a\xa1",     "Quick Sort",      "pivot e partiziona — O(n log n) medio",                sim_quick_sort     },
    { "\xf0\x9f\x94\xba", "Heap Sort",       "max-heap + estrai radice — O(n log n)",               sim_heap_sort      },
    { "\xf0\x9f\x90\x9a", "Shell Sort",      "Insertion con gap decrescente — O(n log\xc2\xb2 n)",   sim_shell_sort     },
    { "\xf0\x9f\x93\x8a", "Counting Sort",   "conta occorrenze — O(n+k), solo interi",              sim_counting_sort  },
    { "\xf0\x9f\x8d\xb9", "Cocktail Sort",   "Bubble bidirezionale — O(n\xc2\xb2)",                  sim_cocktail_sort  },
    { "\xf0\x9f\x8c\xbf", "Gnome Sort",      "avanza o scambia+torna — O(n\xc2\xb2)",               sim_gnome_sort     },
    { "\xf0\x9f\xa5\x9e", "Pancake Sort",    "flip dei prefissi — O(n\xc2\xb2)",                    sim_pancake_sort   },
    { "\xf0\x9f\x94\xa7", "Comb Sort",       "gap decrescente 1.3x, migliora Bubble",               sim_comb_sort      },
    { "\xf0\x9f\x94\xa2", "Radix Sort",      "ordina cifra per cifra LSD — O(d*n)",                 sim_radix_sort     },
    { "\xf0\x9f\xaa\xa3", "Bucket Sort",     "distribuisce in secchielli — O(n+k)",                 sim_bucket_sort    },
    { "\xe2\x8f\xb1",     "Tim Sort",        "ibrido insertion+merge — O(n log n), Python/Java",    sim_tim_sort       },
};
#define N_SORT ((int)(sizeof(_ALGOS_SORT)/sizeof(_ALGOS_SORT[0])))

/* ── RICERCA (4 algoritmi) ──────────────────────────────────────────── */
static const AlgoritmoSim _ALGOS_SEARCH[] = {
    { "\xf0\x9f\x94\x8d", "Ricerca Binaria",       "lista ordinata, dimezza — O(log n)",          sim_binary_search          },
    { "\xf0\x9f\x94\x8e", "Ricerca Lineare",       "scansione sequenziale — O(n)",                sim_ricerca_lineare        },
    { "\xf0\x9f\xa6\x98", "Jump Search",           "salti \xe2\x88\x9an poi scan — O(\xe2\x88\x9an)",            sim_jump_search            },
    { "\xf0\x9f\x8e\xaf", "Interpolation Search",  "stima posizione proporzionale — O(log log n)",sim_interpolation_search   },
};
#define N_SEARCH ((int)(sizeof(_ALGOS_SEARCH)/sizeof(_ALGOS_SEARCH[0])))

/* ── MATEMATICA (9 algoritmi) ───────────────────────────────────────── */
static const AlgoritmoSim _ALGOS_MATH[] = {
    { "\xf0\x9f\x94\xa2", "GCD — Euclide",      "MCD(a,b)=MCD(b,a mod b) — O(log n)",           sim_gcd_euclide        },
    { "\xf0\x9f\xa7\xae", "Crivo Eratostene",   "tutti i primi \xe2\x89\xa4 N — O(n log log n)",          sim_crivo              },
    { "\xe2\x9a\xa1",     "Potenza Modulare",   "a^b mod m, quadrato e moltiplica — O(log b)",   sim_potenza_modulare   },
    { "\xf0\x9f\x94\xba", "Triangolo Pascal",   "C(n,k) = C(n-1,k-1)+C(n-1,k)",                 sim_triangolo_pascal   },
    { "\xf0\x9f\x8e\xb2", "Monte Carlo Pi",     "stima \xcf\x80 con punti casuali",                       sim_monte_carlo_pi     },
    { "\xf0\x9f\x90\x87", "Fibonacci DP",       "memoization F(n)=F(n-1)+F(n-2)",               sim_fibonacci_dp       },
    { "\xf0\x9f\x97\xbc", "Torre di Hanoi",     "ricorsione n=4, 15 mosse — O(2^n)",             sim_torre_hanoi        },
    { "\xf0\x9f\x8c\xb3", "Fibonacci Ricorsivo","albero chiamate n=5 — O(2^n) vs O(n) DP",       sim_fib_ricorsivo      },
    { "\xf0\x9f\xa7\xae", "Miller-Rabin",       "test primalit\xc3\xa0 probabilistico — O(k log n)",      sim_miller_rabin       },
    { "\xf0\x9f\x93\x90", "Horner / Ruffini",  "valutazione polinomiale O(n) vs O(n\xc2\xb2) naive",      sim_horner_ruffini     },
};
#define N_MATH ((int)(sizeof(_ALGOS_MATH)/sizeof(_ALGOS_MATH[0])))

/* ── PROGRAMMAZIONE DINAMICA (5 algoritmi) ──────────────────────────── */
static const AlgoritmoSim _ALGOS_DP[] = {
    { "\xf0\x9f\x8e\x92", "Zaino 0/1",          "tabella DP peso\xc3\x97valore — O(nW)",             sim_zaino          },
    { "\xf0\x9f\x94\xa4", "LCS",                "Longest Common Subsequence — O(mn)",      sim_lcs            },
    { "\xf0\x9f\x92\xb0", "Coin Change",        "minimo numero monete — DP bottom-up",     sim_coin_change    },
    { "\xe2\x9c\x8f",     "Edit Distance",      "Levenshtein GATTO\xe2\x86\x92GRASSO — O(mn)",       sim_edit_distance  },
    { "\xf0\x9f\x93\x88", "LIS",                "Longest Increasing Subsequence — O(n^2)", sim_lis            },
};
#define N_DP ((int)(sizeof(_ALGOS_DP)/sizeof(_ALGOS_DP[0])))

/* ── GRAFI (7 algoritmi) ────────────────────────────────────────────── */
static const AlgoritmoSim _ALGOS_GRAFI[] = {
    { "\xf0\x9f\x8c\x90",             "BFS su Griglia",     "ricerca in ampiezza, percorso minimo",       sim_bfs_griglia    },
    { "\xf0\x9f\x9b\xa4\xef\xb8\x8f","Dijkstra",           "distanze minime su grafo pesato — O(V\xc2\xb2)",    sim_dijkstra       },
    { "\xf0\x9f\x94\x8d",             "DFS su Griglia",     "ricerca in profondit\xc3\xa0 con stack",             sim_dfs_griglia    },
    { "\xf0\x9f\x93\x8b",             "Topological Sort",   "Kahn BFS su DAG 6 nodi — O(V+E)",            sim_topo_sort      },
    { "\xf0\x9f\x9b\xa4",             "Bellman-Ford",       "distanze con pesi negativi — O(VE)",         sim_bellman_ford   },
    { "\xf0\x9f\x97\xba",             "Floyd-Warshall",     "tutte le coppie 4x4 — O(V^3)",               sim_floyd_warshall },
    { "\xf0\x9f\x8c\xb3",             "Kruskal MST",        "Minimum Spanning Tree Union-Find — O(E log E)",sim_kruskal      },
};
#define N_GRAFI ((int)(sizeof(_ALGOS_GRAFI)/sizeof(_ALGOS_GRAFI[0])))

/* ── TECNICHE (3 tecniche) ──────────────────────────────────────────── */
static const AlgoritmoSim _ALGOS_TECH[] = {
    { "\xf0\x9f\x91\x89", "Two Pointers",       "trova coppia con somma target — O(n)",    sim_two_pointers   },
    { "\xf0\x9f\xaa\x9f", "Sliding Window",     "massima somma k elementi — O(n)",         sim_sliding_window },
    { "\xf0\x9f\xa7\xa0", "Bit Manipulation",   "AND, OR, XOR, NOT, shift su byte",        sim_bit_manipulation},
};
#define N_TECH ((int)(sizeof(_ALGOS_TECH)/sizeof(_ALGOS_TECH[0])))

/* ── VISUALIZZAZIONI (3 animazioni) ─────────────────────────────────── */
static const AlgoritmoSim _ALGOS_VIS[] = {
    { "\xf0\x9f\xa7\xac", "Game of Life",       "automa cellulare Conway — 20 generazioni",sim_game_of_life },
    { "\xf0\x9f\x94\xba", "Sierpinski",         "triangolo frattale — 5 livelli ASCII",    sim_sierpinski   },
    { "\xf0\x9f\x8c\x80", "Rule 30 Wolfram",    "automa cellulare elementare",             sim_rule30       },
};
#define N_VIS ((int)(sizeof(_ALGOS_VIS)/sizeof(_ALGOS_VIS[0])))

/* ── FISICA (1 simulazione) ─────────────────────────────────────────── */
static const AlgoritmoSim _ALGOS_FIS[] = {
    { "\xe2\x9a\x9b\xef\xb8\x8f", "Caduta Libera",  "h(t)=h\xe2\x82\x80-\xc2\xbdgt\xc2\xb2, v(t)=gt — passo per passo",  sim_caduta_libera },
};
#define N_FIS ((int)(sizeof(_ALGOS_FIS)/sizeof(_ALGOS_FIS[0])))

/* ── CHIMICA (3 simulazioni) ────────────────────────────────────────── */
static const AlgoritmoSim _ALGOS_CHI[] = {
    { "\xf0\x9f\xa7\xaa", "Calcolo pH",         "pH=-log[H+] per acidi/basi — 5 esempi",  sim_ph           },
    { "\xf0\x9f\x92\xa8", "Gas Ideali PV=nRT",  "espansione isoterma, calcolo P,V,T",      sim_gas_ideali   },
    { "\xf0\x9f\x94\x97", "Stechiometria",      "2H2+O2\xe2\x86\x92" "2H2O — moli e masse passo passo", sim_stechiometria},
};
#define N_CHI ((int)(sizeof(_ALGOS_CHI)/sizeof(_ALGOS_CHI[0])))

/* ── STRINGHE (3 algoritmi) ─────────────────────────────────────────── */
static const AlgoritmoSim _ALGOS_STR[] = {
    { "\xf0\x9f\x94\x8d", "KMP Search",         "Knuth-Morris-Pratt, failure function — O(n+m)", sim_kmp_search   },
    { "\xf0\x9f\x94\x91", "Rabin-Karp",         "rolling hash — O(n+m) medio",                   sim_rabin_karp   },
    { "\xf0\x9f\x94\xa4", "Z-Algorithm",        "array Z, prefix matching — O(n+m)",             sim_z_algorithm  },
};
#define N_STR ((int)(sizeof(_ALGOS_STR)/sizeof(_ALGOS_STR[0])))

/* ── STRUTTURE DATI (5) ─────────────────────────────────────────────── */
static const AlgoritmoSim _ALGOS_STRUCT[] = {
    { "\xf0\x9f\x93\xa6", "Stack e Queue",      "LIFO push/pop — FIFO enqueue/dequeue",          sim_stack_queue  },
    { "\xf0\x9f\x94\x97", "Linked List",        "insert in testa, delete, traversal",            sim_linked_list  },
    { "\xf0\x9f\x8c\xb2", "BST",                "insert + inorder traversal — O(log n) medio",   sim_bst          },
    { "\xf0\x9f\x97\x82", "Hash Table",         "chaining h(k)=k mod 7 — O(1) medio",            sim_hash_table   },
    { "\xf0\x9f\x94\xba", "Min-Heap",           "insert+sift-up, extract-min+sift-down",         sim_min_heap     },
};
#define N_STRUCT ((int)(sizeof(_ALGOS_STRUCT)/sizeof(_ALGOS_STRUCT[0])))

/* ── GREEDY (3 algoritmi) ───────────────────────────────────────────── */
static const AlgoritmoSim _ALGOS_GREEDY[] = {
    { "\xf0\x9f\x93\x85", "Activity Selection", "max attivit\xc3\xa0 non sovrapposte — O(n log n)",     sim_activity_selection },
    { "\xf0\x9f\x8e\x92", "Zaino Frazionario",  "frazioni di oggetti, v/p decrescente",          sim_zaino_frazionario  },
    { "\xf0\x9f\x93\x9d", "Huffman Coding",     "codifica ottimale prefisso-free",               sim_huffman            },
};
#define N_GREEDY ((int)(sizeof(_ALGOS_GREEDY)/sizeof(_ALGOS_GREEDY[0])))

/* ── BACKTRACKING (2 algoritmi) ─────────────────────────────────────── */
static const AlgoritmoSim _ALGOS_BT[] = {
    { "\xf0\x9f\x91\x91", "N-Queens (N=5)",     "5 regine senza conflitti — backtracking",       sim_n_queens },
    { "\xf0\x9f\x9f\xa8", "Sudoku 4x4",         "risolve griglia 4x4 con backtracking",          sim_sudoku   },
};
#define N_BT ((int)(sizeof(_ALGOS_BT)/sizeof(_ALGOS_BT[0])))

/* ── LLM / AI (2 simulazioni) ───────────────────────────────────────── */
static const AlgoritmoSim _ALGOS_LLM[] = {
    { "\xf0\x9f\x93\x8a", "Entropia Pesi",      "Shannon H per layer — guida pruning/quant",    sim_entropia_pesi  },
    { "\xf0\x9f\xa4\x96", "GPTQ + SparseGPT",  "quantizzazione con compensazione + pruning 50%",sim_gptq_sparsegpt },
};
#define N_LLM ((int)(sizeof(_ALGOS_LLM)/sizeof(_ALGOS_LLM[0])))

/* ── Esegui un algoritmo con loop riprova / chiedi AI / torna ───────── */
static void _esegui_algo(const AlgoritmoSim *a) {
    while (1) {
        if (a->sim_fn) a->sim_fn();
        sim_fine(a->nome);
        int c = getch_single(); printf("%c\n", c);
        if      (c == '1') sim_chiedi_ai(a->nome);
        else if (c == '2') continue;
        else               break;
    }
}

/* ── Sottomenu per una categoria ────────────────────────────────────── */
static void _menu_categoria_sim(const char *titolo, const char *icona,
                                 const AlgoritmoSim *algos, int n) {
    char buf[8];
    while (1) {
        clear_screen(); print_header();
        printf(CYN BLD "\n  %s  %s  " GRY "(%d algoritmi)\n\n" RST, icona, titolo, n);
        for (int i = 0; i < n; i++) {
            printf("  " YLW "[%2d]" RST "  %s  %-26s  " GRY "%s\n" RST,
                   i+1, algos[i].icona, algos[i].nome, algos[i].desc);
        }
        printf("\n" GRY "  [1-%d] Simula    [0] Torna\n\n" RST, n);
        printf(GRN "  Scelta: " RST); fflush(stdout);
        if (!fgets(buf, sizeof buf, stdin)) break;
        buf[strcspn(buf, "\n")] = '\0';
        if (buf[0]=='0'||buf[0]=='q'||buf[0]=='Q'||buf[0]==27) break;
        int s = atoi(buf);
        if (s >= 1 && s <= n) _esegui_algo(&algos[s-1]);
    }
}

/* ── Menu principale simulatore ───────────────────────────────────────── */
void menu_simulatore(void) {
    while (1) {
        clear_screen(); print_header();
        printf(CYN BLD "\n  \xf0\x9f\x8e\xae  Simulatore Algoritmi\n" RST);
        printf(GRY "  INVIO=passo  ESC/q=interrompi  Dopo: 1=chiedi AI  2=riprova\n\n" RST);
        printf("  " EM_1 "  \xe2\x9a\xa1  Ordinamento     " GRY "(%2d algo) Bubble,Radix,Bucket,Tim...\n" RST,     N_SORT);
        printf("  " EM_2 "  \xf0\x9f\x94\x8d  Ricerca         " GRY "(%2d algo) Binaria,Lineare,Jump,Interpolation\n" RST, N_SEARCH);
        printf("  " EM_3 "  \xf0\x9f\x94\xa2  Matematica      " GRY "(%2d algo) GCD,Hanoi,Miller-Rabin...\n" RST,  N_MATH);
        printf("  " EM_4 "  \xf0\x9f\xa7\xa9  Prog. Dinamica  " GRY "(%2d algo) Zaino,CoinChange,EditDist,LIS\n" RST, N_DP);
        printf("  " EM_5 "  \xf0\x9f\x8c\x90  Grafi           " GRY "(%2d algo) BFS,DFS,Dijkstra,Floyd,Kruskal\n" RST, N_GRAFI);
        printf("  " EM_6 "  \xf0\x9f\x8e\xaf  Tecniche        " GRY "(%2d algo) TwoPtr, SlidingWin, Bit\n" RST,   N_TECH);
        printf("  " EM_7 "  \xf0\x9f\x8e\xa8  Visualizzazioni " GRY "(%2d algo) GameOfLife,Sierpinski,Rule30\n" RST, N_VIS);
        printf("  " EM_8 "  \xe2\x9a\x9b\xef\xb8\x8f  Fisica          " GRY "(%2d sim)  Caduta libera\n" RST,     N_FIS);
        printf("  " EM_9 "  \xf0\x9f\xa7\xaa  Chimica         " GRY "(%2d sim)  pH, Gas ideali, Stechiometria\n" RST, N_CHI);
        printf("  [a]  \xf0\x9f\x94\xa4  Stringhe        " GRY "(%2d algo) KMP, Rabin-Karp, Z-Algorithm\n" RST,   N_STR);
        printf("  [b]  \xf0\x9f\x93\xa6  Strutture Dati  " GRY "(%2d algo) Stack,Queue,List,BST,Heap,Hash\n" RST, N_STRUCT);
        printf("  [c]  \xf0\x9f\x93\x85  Greedy          " GRY "(%2d algo) Activity,ZainoFraz,Huffman\n" RST,     N_GREEDY);
        printf("  [d]  \xf0\x9f\x91\x91  Backtracking    " GRY "(%2d algo) N-Queens(5), Sudoku 4x4\n" RST,        N_BT);
        printf("  [e]  \xf0\x9f\xa4\x96  LLM / AI        " GRY "(%2d sim)  Entropia pesi, GPTQ+SparseGPT\n" RST,  N_LLM);
        printf("  " EM_0 "  Torna\n\n  " GRN "Scelta: " RST); fflush(stdout);

        int c = getch_single(); printf("%c\n", c);
        if      (c=='1') _menu_categoria_sim("Ordinamento",     "\xe2\x9a\xa1",            _ALGOS_SORT,   N_SORT);
        else if (c=='2') _menu_categoria_sim("Ricerca",         "\xf0\x9f\x94\x8d",        _ALGOS_SEARCH, N_SEARCH);
        else if (c=='3') _menu_categoria_sim("Matematica",      "\xf0\x9f\x94\xa2",        _ALGOS_MATH,   N_MATH);
        else if (c=='4') _menu_categoria_sim("Prog. Dinamica",  "\xf0\x9f\xa7\xa9",        _ALGOS_DP,     N_DP);
        else if (c=='5') _menu_categoria_sim("Grafi",           "\xf0\x9f\x8c\x90",        _ALGOS_GRAFI,  N_GRAFI);
        else if (c=='6') _menu_categoria_sim("Tecniche",        "\xf0\x9f\x8e\xaf",        _ALGOS_TECH,   N_TECH);
        else if (c=='7') _menu_categoria_sim("Visualizzazioni", "\xf0\x9f\x8e\xa8",        _ALGOS_VIS,    N_VIS);
        else if (c=='8') _menu_categoria_sim("Fisica",          "\xe2\x9a\x9b\xef\xb8\x8f",_ALGOS_FIS,    N_FIS);
        else if (c=='9') _menu_categoria_sim("Chimica",         "\xf0\x9f\xa7\xaa",        _ALGOS_CHI,    N_CHI);
        else if (c=='a'||c=='A') _menu_categoria_sim("Stringhe",       "\xf0\x9f\x94\xa4", _ALGOS_STR,    N_STR);
        else if (c=='b'||c=='B') _menu_categoria_sim("Strutture Dati", "\xf0\x9f\x93\xa6", _ALGOS_STRUCT, N_STRUCT);
        else if (c=='c'||c=='C') _menu_categoria_sim("Greedy",         "\xf0\x9f\x93\x85", _ALGOS_GREEDY, N_GREEDY);
        else if (c=='d'||c=='D') _menu_categoria_sim("Backtracking",   "\xf0\x9f\x91\x91", _ALGOS_BT,     N_BT);
        else if (c=='e'||c=='E') _menu_categoria_sim("LLM / AI",       "\xf0\x9f\xa4\x96", _ALGOS_LLM,    N_LLM);
        else if (c=='0'||c=='q'||c=='Q'||c==27) break;
    }
}
