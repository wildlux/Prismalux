/*
 * simulatore_page.cpp — Pagina Simulatore: dati, widget, UI
 * ===========================================================
 * kAlgos (tabella 110 algoritmi), BigOWidget, AlgoBarWidget,
 * e SimulatorePage UI (costruttore, buildSteps, showStep, ...).
 * Le implementazioni degli algoritmi sono in simulatore_algos.cpp.
 */
#include "pages/simulatore_page.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QTextCursor>
#include <QPainter>
#include <QSizePolicy>
#include <QPainterPath>
#include <QQueue>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <cmath>

/* ══════════════════════════════════════════════════════════════
   kAlgos — tabella unica: tutti i 110 algoritmi
   {nome, categoria, complessità, etichettaO, badge, descrizione}
   ══════════════════════════════════════════════════════════════ */
const AlgoEntry kAlgos[ALGO_COUNT] = {

/* ═══════════════════ ORDINAMENTO (19) ═════════════════════════ */
{ "Bubble Sort",    CAT_SORT, kON2,     "O(n\xc2\xb2)",        "LENTO",
  "Bubble Sort \xe2\x80\x94 O(n\xc2\xb2) | Stabile | In-place\n"
  "Confronta coppie adiacenti e le scambia se in ordine sbagliato. "
  "Originato da Iverson (1962). Ottimale solo per array piccoli o quasi ordinati." },

{ "Selection Sort", CAT_SORT, kON2,     "O(n\xc2\xb2)",        "LENTO",
  "Selection Sort \xe2\x80\x94 O(n\xc2\xb2) | Non stabile | In-place\n"
  "Trova il minimo e lo porta in testa; ripete n-1 volte. "
  "Esegue esattamente n\u22121 scambi: il minimo assoluto." },

{ "Insertion Sort", CAT_SORT, kON2,     "O(n\xc2\xb2)",        "LENTO",
  "Insertion Sort \xe2\x80\x94 O(n\xc2\xb2) worst / O(n) best | Stabile\n"
  "Inserisce ogni elemento nella giusta posizione nella parte gi\xc3\xa0 ordinata. "
  "Eccellente per array quasi ordinati o dati in streaming." },

{ "Shell Sort",     CAT_SORT, kONLog2N, "O(n log\xc2\xb2n)",   "BUONO",
  "Shell Sort \xe2\x80\x94 O(n log\xc2\xb2n) | Non stabile | In-place\n"
  "Generalizza Insertion Sort con gap decrescenti (sequenza Knuth). "
  "Donald Shell (1959). Molto pi\xc3\xb9 veloce su array medi (1k-100k)." },

{ "Cocktail Sort",  CAT_SORT, kON2,     "O(n\xc2\xb2)",        "LENTO",
  "Cocktail Shaker Sort \xe2\x80\x94 O(n\xc2\xb2) | Stabile | In-place\n"
  "Bolla in entrambe le direzioni. Riduce il problema delle 'tartarughe'. "
  "Alias: Bidirectional Bubble, Ripple Sort." },

{ "Comb Sort",      CAT_SORT, kONLogN,  "O(n log n)",           "BUONO",
  "Comb Sort \xe2\x80\x94 O(n log n) avg | Non stabile | In-place\n"
  "Usa gap > 1 all'inizio (fattore 1.3) per eliminare tartarughe lontane. "
  "Vlajko Lacko & Richard Box (1980). Simile a Shell Sort." },

{ "Gnome Sort",     CAT_SORT, kON2,     "O(n\xc2\xb2)",        "LENTO",
  "Gnome Sort \xe2\x80\x94 O(n\xc2\xb2) | Stabile | In-place\n"
  "Lo gnomo porta ogni fiore nella posizione giusta un passo alla volta. "
  "Equivalente a Insertion Sort ma implementato con scambi." },

{ "Odd-Even Sort",  CAT_SORT, kON2,     "O(n\xc2\xb2)",        "LENTO",
  "Odd-Even Sort \xe2\x80\x94 O(n\xc2\xb2) | Stabile | Parallelizzabile\n"
  "Alterna confronti pari/dispari. Versione parallela: tutti i confronti avvengono in simultanea. "
  "Alias: Brick Sort. Usato su architetture SIMD." },

{ "Cycle Sort",     CAT_SORT, kON2,     "O(n\xc2\xb2)",        "LENTO",
  "Cycle Sort \xe2\x80\x94 O(n\xc2\xb2) | Non stabile | MIN SCRITTURE\n"
  "Minimizza il numero di scritture su memoria (mai pi\xc3\xb9 di n-1). "
  "Ideale per EEPROM e flash dove le scritture hanno costo elevato." },

{ "Pancake Sort",   CAT_SORT, kON2,     "O(n\xc2\xb2)",        "LENTO",
  "Pancake Sort \xe2\x80\x94 O(n\xc2\xb2) | Non stabile | In-place\n"
  "Unica operazione: flip(k) che inverte i primi k elementi. "
  "Problema di Bill Gates (1979). Ogni passo usa al pi\xc3\xb9 2 flip." },

{ "Quick Sort",     CAT_SORT, kONLogN,  "O(n log n)",           "OTTIMO",
  "Quick Sort \xe2\x80\x94 O(n log n) avg | Non stabile | In-place\n"
  "Sceglie un pivot, partiziona, ricorre sulle due met\xc3\xa0. "
  "Tony Hoare (1959). Algoritmo pi\xc3\xb9 usato nella pratica (std::sort, Arrays.sort)." },

{ "Merge Sort",     CAT_SORT, kONLogN,  "O(n log n)",           "OTTIMO",
  "Merge Sort \xe2\x80\x94 O(n log n) sempre | Stabile | O(n) memoria\n"
  "Divide, ordina ricorsivamente, poi unisce. John von Neumann (1945). "
  "Garantito O(n log n) — ideale per file su disco e linked list." },

{ "Heap Sort",      CAT_SORT, kONLogN,  "O(n log n)",           "BUONO",
  "Heap Sort \xe2\x80\x94 O(n log n) sempre | Non stabile | In-place\n"
  "Costruisce un max-heap, poi estrae il massimo ripetutamente. "
  "Garantito O(n log n) senza memoria extra." },

{ "Bitonic Sort",   CAT_SORT, kONLog2N, "O(n log\xc2\xb2n)",   "BUONO",
  "Bitonic Sort \xe2\x80\x94 O(n log\xc2\xb2n) | Parallelo\n"
  "Costruisce sequenze bitoniche e le fonde. Completamente parallelizzabile su GPU. "
  "Richiede n = 2^k elementi." },

{ "Counting Sort",  CAT_SORT, kON,      "O(n+k)",               "VELOCE",
  "Counting Sort \xe2\x80\x94 O(n+k) | Stabile | O(k) memoria\n"
  "Conta le occorrenze, poi ricostruisce l'array. "
  "Imbattibile per interi in range piccolo. Non confronta mai due valori." },

{ "Radix Sort",     CAT_SORT, kON,      "O(d\xc2\xb7n)",        "VELOCE",
  "Radix Sort LSD \xe2\x80\x94 O(d\xc2\xb7n) | Stabile | O(n+k) memoria\n"
  "Ordina cifra per cifra dalla meno significativa. "
  "Imbattibile per interi a lunghezza fissa (IP, telefoni, codici postali)." },

{ "Bucket Sort",    CAT_SORT, kON,      "O(n+k)",               "VELOCE",
  "Bucket Sort \xe2\x80\x94 O(n+k) avg | Non stabile | O(n+k) memoria\n"
  "Distribuisce elementi in bucket, ordina ogni bucket, concatena. "
  "Ideale per distribuzione uniforme. Usato in: database, computer grafica." },

{ "Tim Sort",       CAT_SORT, kONLogN,  "O(n log n)",           "OTTIMO",
  "Tim Sort \xe2\x80\x94 O(n log n) | Stabile | O(n) memoria\n"
  "Tim Peters (2002). Ibrido Merge Sort + Insertion Sort. "
  "Algoritmo di default in Python (list.sort) e Java (Arrays.sort objects)." },

{ "Stooge Sort",    CAT_SORT, kON2,     "O(n^2.7)",             "LENTO",
  "Stooge Sort \xe2\x80\x94 O(n^2.7) | Non stabile | Ricorsivo\n"
  "Ricorsione curiosa: ordina i 2/3 iniziali, poi i 2/3 finali, poi i 2/3 iniziali di nuovo. "
  "Storicamente importante per teoria della complessit\xc3\xa0. MAI usato in produzione." },

/* ═══════════════════ RICERCA (10) ══════════════════════════════ */
{ "Linear Search",        CAT_SEARCH, kON,       "O(n)",          "LINEARE",
  "Linear Search \xe2\x80\x94 O(n) | Su array non ordinati\n"
  "Scorre ogni elemento finch\xc3\xa9 trova il target. "
  "Semplice ma lento; unica opzione per dati disordinati." },

{ "Binary Search",        CAT_SEARCH, kOLogN,    "O(log n)",      "VELOCE",
  "Binary Search \xe2\x80\x94 O(log n) | Richiede array ORDINATO\n"
  "Dimezza lo spazio di ricerca ad ogni confronto. "
  "20 confronti coprono oltre 1 milione di elementi!" },

{ "Jump Search",          CAT_SEARCH, kOSqrtN,   "O(\xe2\x88\x9an)",      "BUONO",
  "Jump Search \xe2\x80\x94 O(\xe2\x88\x9an) | Richiede array ORDINATO\n"
  "Salta blocchi di \xe2\x88\x9an elementi finch\xc3\xa9 supera il target, poi cerca linearmente. "
  "Tra Linear O(n) e Binary O(log n)." },

{ "Ternary Search",       CAT_SEARCH, kOLogN,    "O(log n)",      "VELOCE",
  "Ternary Search \xe2\x80\x94 O(log\xe2\x82\x83n) | Richiede array ORDINATO\n"
  "Divide lo spazio in tre parti. Ottimale per funzioni unimodali. "
  "Nella pratica Binary Search \xc3\xa8 pi\xc3\xb9 veloce." },

{ "Interpolation Search",  CAT_SEARCH, kOLogLogN, "O(log log n)", "OTTIMO",
  "Interpolation Search \xe2\x80\x94 O(log log n) avg | Array ORDINATO + UNIFORME\n"
  "Stima la posizione con interpolazione lineare. "
  "Supera Binary Search per distribuzioni uniformi (ID sequenziali, telefoni)." },

{ "Exponential Search",   CAT_SEARCH, kOLogN,    "O(log n)",      "VELOCE",
  "Exponential Search \xe2\x80\x94 O(log n) | Richiede array ORDINATO\n"
  "Raddoppia il range finch\xc3\xa9 supera il target, poi Binary Search. "
  "Ideale per array illimitati o di lunghezza sconosciuta." },

{ "Fibonacci Search",     CAT_SEARCH, kOLogN,    "O(log n)",      "VELOCE",
  "Fibonacci Search \xe2\x80\x94 O(log n) | Richiede array ORDINATO\n"
  "Usa i numeri di Fibonacci per dividere l'array senza divisioni. "
  "Proposto nel 1960: utile su sistemi senza FPU." },

{ "Two Pointers",         CAT_SEARCH, kON,       "O(n)",          "LINEARE",
  "Two Pointers \xe2\x80\x94 O(n) | Richiede array ORDINATO\n"
  "Due indici dagli estremi si avvicinano. Trova coppia con somma target. "
  "Pattern base di 3Sum, trapping rainwater, container with most water." },

{ "Boyer-Moore Voting",   CAT_SEARCH, kON,       "O(n)",          "LINEARE",
  "Boyer-Moore Majority Voting \xe2\x80\x94 O(n) | O(1) spazio\n"
  "Trova l'elemento maggioritario (>n/2 occorrenze) in un'unica passata. "
  "Robert Boyer & J Moore (1980). Usato in: elezioni, streaming, consensus." },

{ "Quickselect",          CAT_SEARCH, kON,       "O(n) avg",      "LINEARE",
  "Quickselect \xe2\x80\x94 O(n) avg / O(n\xc2\xb2) worst | In-place\n"
  "Trova il K-esimo elemento senza ordinare tutto. Tony Hoare (1961). "
  "Median-of-medians garantisce O(n) worst. Usato in: statistiche, k-NN." },

/* ═══════════════════ STRUTTURE DATI (8) ════════════════════════ */
{ "Stack (LIFO)",         CAT_STRUCT, kO1,    "O(1)",          "OTTIMO",
  "Stack \xe2\x80\x94 LIFO | O(1) push/pop | Struttura fondamentale\n"
  "Pila: l'ultimo inserito \xc3\xa8 il primo estratto. "
  "Usato in: call stack, undo/redo, bilanciamento parentesi, DFS iterativo." },

{ "Queue (FIFO)",         CAT_STRUCT, kO1,    "O(1)",          "OTTIMO",
  "Queue \xe2\x80\x94 FIFO | O(1) enqueue/dequeue\n"
  "Coda: il primo inserito \xc3\xa8 il primo estratto. "
  "Usato in: BFS, task scheduling, buffer I/O, stampanti." },

{ "Deque",                CAT_STRUCT, kO1,    "O(1)",          "OTTIMO",
  "Deque (Double-Ended Queue) \xe2\x80\x94 O(1) da entrambi i lati\n"
  "Inserimento/estrazione sia dalla testa che dalla coda. "
  "Usato in: sliding window max, palindromi, undo-redo bidirezionale." },

{ "Min-Heap Build",       CAT_STRUCT, kON,    "O(n)",          "LINEARE",
  "Min-Heap Build (Floyd) \xe2\x80\x94 O(n) | Struttura Heap\n"
  "Robert Floyd (1964): costruisce un min-heap dal basso. "
  "Controintuitivo: pi\xc3\xb9 veloce di n inserimenti O(n log n). "
  "Base di: Priority Queue, Dijkstra, A*, Prim, Heap Sort." },

{ "Hash Table",           CAT_STRUCT, kO1,    "O(1) avg",      "OTTIMO",
  "Hash Table \xe2\x80\x94 O(1) avg insert/search/delete\n"
  "Mappa chiave\xe2\x86\x92valore tramite funzione hash. Collisioni: chaining o open addressing. "
  "Usato ovunque: cache, database index, set/dict Python/JS/Java." },

{ "Segment Tree",         CAT_STRUCT, kONLogN,"O(n log n)",    "BUONO",
  "Segment Tree \xe2\x80\x94 O(log n) query/update | O(n) build\n"
  "Albero binario sulle posizioni dell'array per range query in O(log n). "
  "Usato in: range sum/min/max, interval update, problemi competitivi." },

{ "Fenwick Tree (BIT)",   CAT_STRUCT, kONLogN,"O(log n)",      "VELOCE",
  "Fenwick Tree (Binary Indexed Tree) \xe2\x80\x94 O(log n) update/prefix\n"
  "Peter Fenwick (1994). Pi\xc3\xb9 semplice e veloce del Segment Tree per prefix sum. "
  "Usato in: conteggio inversioni, frequenze cumulative, competitive programming." },

{ "LRU Cache",            CAT_STRUCT, kO1,    "O(1)",          "OTTIMO",
  "LRU Cache \xe2\x80\x94 O(1) get/put | Hash Map + Doubly Linked List\n"
  "Least Recently Used: espelle l'elemento meno usato di recente. "
  "Usato in: cache CPU, browser cache, Redis, CDN eviction policy." },

/* ═══════════════════ GRAFI (11) ════════════════════════════════ */
{ "BFS",                  CAT_GRAPH, kON,     "O(V+E)",        "LINEARE",
  "BFS \xe2\x80\x94 Breadth-First Search \xe2\x80\x94 O(V+E)\n"
  "Konrad Zuse (1945), reinvented by Moore (1959). Esplora per livelli. "
  "Garantisce il cammino pi\xc3\xb9 breve su grafi non pesati." },

{ "DFS",                  CAT_GRAPH, kON,     "O(V+E)",        "LINEARE",
  "DFS \xe2\x80\x94 Depth-First Search \xe2\x80\x94 O(V+E)\n"
  "Tarjan & Hopcroft (1971). Esplora in profondit\xc3\xa0 prima di tornare. "
  "Base di: topological sort, SCC, cicli, bipartiteness." },

{ "Dijkstra",             CAT_GRAPH, kONLogN, "O((V+E)logV)",  "BUONO",
  "Dijkstra \xe2\x80\x94 O((V+E) log V) con min-heap\n"
  "Edsger Dijkstra (1959), scritto in 20 minuti. Cammino minimo a pesi non negativi. "
  "Usato in: GPS, routing internet (OSPF), reti di trasporto." },

{ "Bellman-Ford",         CAT_GRAPH, kON2,    "O(V\xc2\xb7""E)",       "LENTO",
  "Bellman-Ford \xe2\x80\x94 O(VE) | Funziona con pesi NEGATIVI\n"
  "Bellman (1958), Ford (1956). Rilassa tutti gli archi V-1 volte. "
  "Rileva cicli negativi. Usato in: protocollo RIP, arbitraggio valute." },

{ "Floyd-Warshall",       CAT_GRAPH, kON3,    "O(V\xc2\xb3)",          "LENTO",
  "Floyd-Warshall \xe2\x80\x94 O(V\xc2\xb3) | Tutti i cammini minimi\n"
  "Floyd (1962), Warshall (1962). DP su tutti i nodi intermedi. "
  "Usato in: routing, chiusura transitiva, analisi di raggiungibilit\xc3\xa0." },

{ "Topological Sort",     CAT_GRAPH, kON,     "O(V+E)",        "LINEARE",
  "Topological Sort (Kahn BFS) \xe2\x80\x94 O(V+E) | Solo DAG\n"
  "Arthur Kahn (1962). Ordina nodi rispettando le dipendenze. "
  "Usato in: build systems (Make), npm/pip, compilatori." },

{ "Kruskal MST",          CAT_GRAPH, kONLogN, "O(E log E)",    "BUONO",
  "Kruskal MST \xe2\x80\x94 O(E log E) | Minimum Spanning Tree\n"
  "Joseph Kruskal (1956). Ordina archi per peso, aggiunge se non forma ciclo (Union-Find). "
  "Usato in: reti elettriche, cavi di rete, clustering." },

{ "Prim MST",             CAT_GRAPH, kONLogN, "O(E log V)",    "BUONO",
  "Prim MST \xe2\x80\x94 O(E log V) con min-heap | Minimum Spanning Tree\n"
  "Robert Prim (1957). Cresce l'albero un arco alla volta dal nodo pi\xc3\xb9 vicino. "
  "Pi\xc3\xb9 efficiente di Kruskal su grafi densi." },

{ "Union-Find",           CAT_GRAPH, kOAlpha, "O(\xce\xb1(n))",         "OTTIMO",
  "Union-Find (Disjoint Set) \xe2\x80\x94 O(\xce\xb1(n)) \xe2\x89\x88 O(1)\n"
  "Kruskal (1956) + Tarjan path compression (1975). "
  "\xce\xb1 = funzione inversa di Ackermann (praticamente costante)." },

{ "Tarjan SCC",           CAT_GRAPH, kON,     "O(V+E)",        "LINEARE",
  "Tarjan SCC \xe2\x80\x94 O(V+E) | Strongly Connected Components\n"
  "Robert Tarjan (1972). DFS + stack per trovare componenti fortemente connesse. "
  "Usato in: analisi dipendenze, dead code detection, 2-SAT." },

{ "A* Search",            CAT_GRAPH, kONLogN, "O(E log V)",    "BUONO",
  "A* Search \xe2\x80\x94 O(E log V) | Cammino minimo con euristica\n"
  "Hart, Nilsson, Raphael (1968). Dijkstra + euristica ammissibile h(n). "
  "Usato in: pathfinding giochi (Pac-Man, StarCraft), GPS, robotica." },

/* ═══════════════════ PROGRAMMAZIONE DINAMICA (10) ══════════════ */
{ "Coin Change",          CAT_DP, kON2W,  "O(n\xc2\xb7k)",        "MEDIO",
  "Coin Change \xe2\x80\x94 O(amount \xc3\x97 coins) | DP bottom-up\n"
  "Numero minimo di monete per formare un importo. "
  "La greedy fallisce su [1,3,4]/6. DP garantisce l'ottimo." },

{ "LIS",                  CAT_DP, kONLogN, "O(n log n)",       "BUONO",
  "LIS \xe2\x80\x94 Longest Increasing Subsequence \xe2\x80\x94 O(n log n)\n"
  "Dilworth (1950), ottimizzazione con Patience Sort. "
  "Applicazioni: git diff (usa LCS), sequenze DNA, scheduling." },

{ "0/1 Knapsack",         CAT_DP, kON2W,  "O(n\xc2\xb7W)",        "MEDIO",
  "0/1 Knapsack \xe2\x80\x94 O(n\xc2\xb7W) | DP classica\n"
  "Mathews (1897). Ogni oggetto: preso o non preso. "
  "NP-hard in generale, pseudo-polinomiale con DP." },

{ "LCS",                  CAT_DP, kON2,   "O(m\xc2\xb7n)",        "LENTO",
  "LCS \xe2\x80\x94 Longest Common Subsequence \xe2\x80\x94 O(m\xc2\xb7n)\n"
  "Hirschberg (1975) per versione O(n) spazio. "
  "Base di: diff unix, git, DNA alignment (BLAST)." },

{ "Edit Distance",        CAT_DP, kON2,   "O(m\xc2\xb7n)",        "LENTO",
  "Edit Distance (Levenshtein) \xe2\x80\x94 O(m\xc2\xb7n)\n"
  "Vladimir Levenshtein (1965). Min inserimenti/cancellazioni/sostituzioni. "
  "Usato in: correttori ortografici, fuzzy search, NLP." },

{ "Matrix Chain",         CAT_DP, kON3,   "O(n\xc2\xb3)",         "LENTO",
  "Matrix Chain Multiplication \xe2\x80\x94 O(n\xc2\xb3)\n"
  "Ordine ottimale per moltiplicare n matrici. "
  "Esempio classico di DP con sottoproblemi sovrapposti. "
  "Barre = dimensioni intermedie delle matrici." },

{ "Egg Drop",             CAT_DP, kON2,   "O(k\xc2\xb7n)",        "MEDIO",
  "Egg Drop \xe2\x80\x94 O(k\xc2\xb7n) | DP classico\n"
  "Con k uova e n piani: trovare il piano critico con meno tentativi. "
  "Problema con variante ottima O(n log n) con ricerca binaria." },

{ "Rod Cutting",          CAT_DP, kON2,   "O(n\xc2\xb2)",         "LENTO",
  "Rod Cutting \xe2\x80\x94 O(n\xc2\xb2) | DP bottom-up\n"
  "Taglia una sbarra di lunghezza n per massimizzare il profitto. "
  "Simile a Knapsack. Barre = profitto massimo per lunghezza." },

{ "Subset Sum DP",        CAT_DP, kON2,   "O(n\xc2\xb7S)",        "MEDIO",
  "Subset Sum \xe2\x80\x94 O(n\xc2\xb7S) | DP su boolean table\n"
  "Esiste un sottoinsieme che somma a S? "
  "NP-completo in generale. DP pseudo-polinomiale con S piccolo." },

{ "Max Product Subarray", CAT_DP, kON,    "O(n)",              "LINEARE",
  "Maximum Product Subarray \xe2\x80\x94 O(n) | DP greedy\n"
  "Massimo prodotto di un sottoarray contiguo. Trickier di Kadane (i negativi si invertono). "
  "Tiene traccia sia del massimo che del minimo corrente." },

/* ═══════════════════ GREEDY (6) ════════════════════════════════ */
{ "Activity Selection",   CAT_GREEDY, kONLogN, "O(n log n)",   "BUONO",
  "Activity Selection \xe2\x80\x94 O(n log n) | Greedy ottimale\n"
  "Seleziona il massimo numero di attivit\xc3\xa0 non sovrapposte. "
  "Ordina per fine, scegli la prima compatibile. Corretto per dimostrazione di scambio." },

{ "Fractional Knapsack",  CAT_GREEDY, kONLogN, "O(n log n)",   "BUONO",
  "Fractional Knapsack \xe2\x80\x94 O(n log n) | Greedy ottimale\n"
  "Si possono prendere frazioni degli oggetti. "
  "Greedy per valore/peso \xc3\xa8 ottimale (diversamente da 0/1 Knapsack)." },

{ "Huffman Coding",       CAT_GREEDY, kONLogN, "O(n log n)",   "BUONO",
  "Huffman Coding \xe2\x80\x94 O(n log n) | Compressione ottimale\n"
  "David Huffman (1952). Codifica a lunghezza variabile: simboli frequenti = codici corti. "
  "Base di: gzip, PNG, JPEG, MP3. Prefix-free code." },

{ "Job Scheduling",       CAT_GREEDY, kONLogN, "O(n log n)",   "BUONO",
  "Job Scheduling (EDF) \xe2\x80\x94 O(n log n) | Earliest Deadline First\n"
  "Assegna job a slot temporali per massimizzare profitto. "
  "Greedy: ordina per profitto decrescente, assegna all'ultimo slot disponibile." },

{ "Coin Change Greedy",   CAT_GREEDY, kON,    "O(n)",          "LINEARE",
  "Coin Change Greedy \xe2\x80\x94 O(n) | NON sempre ottimale\n"
  "Sceglie sempre la moneta pi\xc3\xb9 grande. Funziona per sistemi canonici (euro, dollaro). "
  "Fallisce su [1,3,4]/6 \xe2\x86\x92 confronta con la versione DP." },

{ "Min Platforms",        CAT_GREEDY, kONLogN, "O(n log n)",   "BUONO",
  "Minimum Platforms (Train Stations) \xe2\x80\x94 O(n log n)\n"
  "Numero minimo di binari per gestire tutti i treni senza conflitti. "
  "Greedy su eventi ordinati: +1 per arrivo, -1 per partenza." },

/* ═══════════════════ BACKTRACKING (5) ══════════════════════════ */
{ "N-Queens",             CAT_BACKTRACK, kO2N, "O(n!)",        "ESPONENZ.",
  "N-Queens \xe2\x80\x94 O(n!) | Backtracking con pruning\n"
  "Posiziona N regine su una scacchiera N\xc3\x97N senza attaccarsi. "
  "Gauss (1850). Backtracking classico: piazza, verifica, torna indietro." },

{ "Subset Sum BT",        CAT_BACKTRACK, kO2N, "O(2\xe2\x81\xbf)",     "ESPONENZ.",
  "Subset Sum (Backtracking) \xe2\x80\x94 O(2\xe2\x81\xbf)\n"
  "Cerca tutti i sottoinsiemi con somma target. "
  "Visualizza: includi/escludi ogni elemento. Confronta con versione DP." },

{ "Permutazioni",         CAT_BACKTRACK, kO2N, "O(n!)",        "ESPONENZ.",
  "Permutazioni \xe2\x80\x94 O(n!) | Backtracking\n"
  "Genera tutte le permutazioni di un array. "
  "Heap Algorithm (1963) o swap-ricorsivo. n! cresce rapidissimamente." },

{ "Flood Fill",           CAT_BACKTRACK, kON,  "O(n)",         "LINEARE",
  "Flood Fill \xe2\x80\x94 O(n) | BFS/DFS su griglia\n"
  "Colora una regione connessa (come il secchio di MS Paint). "
  "Usato in: paint programs, giochi (Go, Minesweeper), computer vision." },

{ "Rat in a Maze",        CAT_BACKTRACK, kO2N, "O(2\xe2\x81\xbf)",     "ESPONENZ.",
  "Rat in a Maze \xe2\x80\x94 O(2\xe2\x81\xbf) | Backtracking\n"
  "Trova il percorso da (0,0) a (n-1,n-1) in una griglia con ostacoli. "
  "Classico per introdurre backtracking e path finding." },

/* ═══════════════════ STRINGHE (5) ══════════════════════════════ */
{ "KMP Search",           CAT_STRING, kON,    "O(n+m)",        "LINEARE",
  "KMP \xe2\x80\x94 Knuth-Morris-Pratt \xe2\x80\x94 O(n+m)\n"
  "Knuth, Morris, Pratt (1977). Precalcola la failure function per evitare backtracking. "
  "Prima colonna: failure function f[]. Ricerca lineare garantita." },

{ "Rabin-Karp",           CAT_STRING, kON,    "O(n+m) avg",   "LINEARE",
  "Rabin-Karp \xe2\x80\x94 O(n+m) avg | Rolling Hash\n"
  "Rabin & Karp (1987). Usa rolling hash per trovare pattern. "
  "Eccellente per ricerca multipla di pattern. Base di plagiarism detection." },

{ "Z-Algorithm",          CAT_STRING, kON,    "O(n)",          "LINEARE",
  "Z-Algorithm \xe2\x80\x94 O(n) | Z-array\n"
  "Z[i] = lunghezza del pi\xc3\xb9 lungo prefisso di S[i..] uguale a un prefisso di S. "
  "Equivalente a KMP. Usato in: pattern matching, bioinformatica." },

{ "Longest Palindrome",   CAT_STRING, kON,    "O(n)",          "LINEARE",
  "Manacher's Algorithm \xe2\x80\x94 O(n) | Palindromo pi\xc3\xb9 lungo\n"
  "Glenn Manacher (1975). Trova il palindromo pi\xc3\xb9 lungo in O(n). "
  "Sfrutta la simmetria: se gi\xc3\xa0 dentro a un palindromo, usa le info precedenti." },

{ "Longest Common Prefix",CAT_STRING, kONLogN,"O(n log n)",    "BUONO",
  "Longest Common Prefix (LCP Array) \xe2\x80\x94 O(n log n)\n"
  "Costruisce il suffix array ordinando i suffissi, poi calcola LCP. "
  "Base di: full-text search, compressione, bioinformatica (DNA)." },

/* ═══════════════════ MATEMATICA (16) ═══════════════════════════ */
{ "Crivello Eratostene",  CAT_MATH, kONLogLogN,"O(n log log n)","OTTIMO",
  "Crivello di Eratostene \xe2\x80\x94 O(n log log n) | Tutti i primi \xe2\x89\xa4 n\n"
  "Eratostene di Cirene (~240 a.C.). Segna multipli di ogni primo. "
  "Ancora il metodo pi\xc3\xb9 rapido per generare tutti i primi fino a N." },

{ "Crivello di Sundaram", CAT_MATH, kONLogN,  "O(n log n)",    "BUONO",
  "Crivello di Sundaram \xe2\x80\x94 O(n log n)\n"
  "S.P. Sundaram (1934). Variante del crivello: elimina i numeri i+j+2ij. "
  "Genera tutti i primi dispari \xe2\x89\xa4 2n+1." },

{ "GCD Euclideo",         CAT_MATH, kOLogN,   "O(log n)",      "VELOCE",
  "GCD Euclideo \xe2\x80\x94 O(log min(a,b)) | Il pi\xc3\xb9 antico algoritmo ancora in uso\n"
  "Euclide: Elementi, Libro VII (~300 a.C.). "
  "Base di: RSA, riduzione frazioni, calcolo LCM." },

{ "GCD Esteso",           CAT_MATH, kOLogN,   "O(log n)",      "VELOCE",
  "Extended GCD \xe2\x80\x94 O(log min(a,b)) | Identit\xc3\xa0 di B\xc3\xa9zout\n"
  "Trova x,y tali che ax+by=GCD(a,b). "
  "Fondamentale per inverso modulare: base di RSA e crittografia ECC." },

{ "Fast Power",           CAT_MATH, kOLogN,   "O(log n)",      "VELOCE",
  "Esponenziazione Veloce \xe2\x80\x94 O(log n)\n"
  "Calcola a^n con O(log n) moltiplicazioni usando a^(2k)=(a^k)\xc2\xb2. "
  "India antica (~200 a.C.). Cruciale in RSA: (m^e) mod n." },

{ "Fattorizzazione Prima",CAT_MATH, kOSqrtN,  "O(\xe2\x88\x9an)",      "BUONO",
  "Fattorizzazione Prima (Trial Division) \xe2\x80\x94 O(\xe2\x88\x9an)\n"
  "Divide per ogni primo fino a \xe2\x88\x9an. "
  "Base della crittografia: fattorizzare grandi n \xc3\xa8 computazionalmente difficile." },

{ "Miller-Rabin",         CAT_MATH, kOLogN,   "O(k log\xc2\xb2n)",     "VELOCE",
  "Miller-Rabin Primality \xe2\x80\x94 O(k log\xc2\xb2n) | Test probabilistico\n"
  "Miller (1976), Rabin (1980). k testimoni: errore < 4^(-k). "
  "Usato in: RSA key generation, librerie crittografiche (OpenSSL)." },

{ "Pascal Triangle",      CAT_MATH, kON2,     "O(n\xc2\xb2)",          "LENTO",
  "Triangolo di Pascal \xe2\x80\x94 O(n\xc2\xb2)\n"
  "Blaise Pascal (1653), ma noto in India nel 200 a.C. "
  "C(n,k) = C(n-1,k-1)+C(n-1,k). Base di: coefficienti binomiali, probabilit\xc3\xa0." },

{ "Fibonacci DP",         CAT_MATH, kON,      "O(n)",          "LINEARE",
  "Fibonacci DP (Bottom-Up) \xe2\x80\x94 O(n) | O(1) spazio\n"
  "Leonardo da Pisa (1202). Da O(2\xe2\x81\xbf) naif a O(n) con memoization. "
  "Applicazioni: Fibonacci Heap, alberi AVL, pattern in natura." },

{ "Numeri di Catalan",    CAT_MATH, kON,      "O(n)",          "LINEARE",
  "Numeri di Catalan \xe2\x80\x94 O(n) | Sequenza combinatoria\n"
  "Catalan(n) = C(2n,n)/(n+1). Conta: alberi BST, parentesizzazioni, triangolazioni. "
  "Eugene Charles Catalan (1838)." },

{ "Monte Carlo Pi",       CAT_MATH, kON,      "O(n)",          "LINEARE",
  "Monte Carlo \xe2\x80\x94 Stima di \xcf\x80 \xe2\x80\x94 O(n)\n"
  "Punti casuali nel quadrato: quanti cadono nel cerchio? Ratio \xe2\x89\x88 \xcf\x80/4. "
  "John von Neumann & Ulam (1947). Tecnica fondamentale in simulazione numerica." },

{ "Congettura di Collatz", CAT_MATH, kON,     "O(n) (?)",      "???",
  "Congettura di Collatz \xe2\x80\x94 Lorenz Collatz (1937)\n"
  "n pari \xe2\x86\x92 n/2, n dispari \xe2\x86\x92 3n+1. La sequenza raggiunge sempre 1? "
  "Non dimostrata. Paul Erd\xc3\xb6s: 'Mathematics is not yet ready for such problems'." },

{ "Karatsuba",            CAT_MATH, kONLogN,  "O(n^1.585)",    "BUONO",
  "Moltiplicazione di Karatsuba \xe2\x80\x94 O(n^1.585)\n"
  "Anatoly Karatsuba (1960). Divide il numero in met\xc3\xa0, usa 3 invece di 4 moltiplicazioni. "
  "Base di: GMP bignum, Python long integers, RSA." },

{ "Prefix Sum",           CAT_MATH, kON,      "O(n)",          "LINEARE",
  "Prefix Sum \xe2\x80\x94 O(n) build, O(1) query\n"
  "Pre-calcola somme cumulative: range sum da O(n) a O(1). "
  "Base di: Fenwick Tree, Segment Tree, algoritmi di compressione." },

{ "Kadane (Max Subarray)", CAT_MATH, kON,     "O(n)",          "LINEARE",
  "Kadane's Algorithm \xe2\x80\x94 O(n) | Massimo sottovettore contiguo\n"
  "Joseph Kadane (1984). DP ottimale classico. "
  "Applicazioni: analisi finanziaria, image processing." },

{ "Metodo di Horner",     CAT_MATH, kON,      "O(n)",          "LINEARE",
  "Metodo di Horner \xe2\x80\x94 O(n) | Valutazione polinomio\n"
  "Valuta p(x) con n moltiplicazioni invece di O(n\xc2\xb2). "
  "William George Horner (1819). Usato in compilatori, shader GPU, FFT." },

/* ═══════════════════ PATTERN ARRAY (8) ════════════════════════ */
{ "Sliding Window",       CAT_PATTERN, kON,   "O(n)",          "LINEARE",
  "Sliding Window \xe2\x80\x94 O(n) | Finestra scorrevole\n"
  "Mantiene una finestra di k elementi, aggiorna in O(1) per passo. "
  "Pattern fondamentale: max/min subarray, substring senza ripetizioni." },

{ "Dutch National Flag",  CAT_PATTERN, kON,   "O(n)",          "LINEARE",
  "Dutch National Flag (Dijkstra, 1976) \xe2\x80\x94 O(n) | 3-way partition\n"
  "Ordina array con 3 valori in una passata. "
  "Base del Quick Sort 3-way (elimina O(n\xc2\xb2) su array con duplicati)." },

{ "Trapping Rain Water",  CAT_PATTERN, kON,   "O(n)",          "LINEARE",
  "Trapping Rain Water \xe2\x80\x94 O(n) | Two Pointers\n"
  "Quanta acqua rimane intrappolata tra le barre? "
  "Problema classico Google/Amazon. Two-pointer O(n) vs Prefix Max O(n) vs Stack O(n)." },

{ "Next Greater Element", CAT_PATTERN, kON,   "O(n)",          "LINEARE",
  "Next Greater Element \xe2\x80\x94 O(n) | Monotonic Stack\n"
  "Per ogni elemento trova il prossimo pi\xc3\xb9 grande usando uno stack decrescente. "
  "Appare in: span borsistico, histogram max area, parsing espressioni." },

{ "Fisher-Yates Shuffle", CAT_PATTERN, kON,   "O(n)",          "LINEARE",
  "Fisher-Yates Shuffle \xe2\x80\x94 O(n) | Permutazione uniforme\n"
  "Fisher & Yates (1938), versione moderna Knuth (1969). "
  "Genera ogni permutazione con probabilit\xc3\xa0 esatta 1/n!." },

{ "Stock Max Profit",     CAT_PATTERN, kON,   "O(n)",          "LINEARE",
  "Stock Max Profit \xe2\x80\x94 O(n) | Kadane sulle differenze\n"
  "Massimo profitto con una singola compravendita. "
  "Generalizza: k transazioni, cooldown, fee." },

{ "Max Circular Subarray",CAT_PATTERN, kON,   "O(n)",          "LINEARE",
  "Maximum Circular Subarray \xe2\x80\x94 O(n)\n"
  "Massimo subarray in un array circolare. "
  "Caso 1: dentro l'array (Kadane). Caso 2: avvolge il bordo = totale - min subarray." },

{ "Count Inversions",     CAT_PATTERN, kONLogN,"O(n log n)",   "BUONO",
  "Count Inversions \xe2\x80\x94 O(n log n) | Merge Sort modificato\n"
  "Numero di coppie (i,j) con i<j ma a[i]>a[j]. "
  "Misura quanto un array \xc3\xa8 lontano dall'ordinamento. Usato in: sorting networks, raccomandazioni." },

/* ═══════════════════ CLASSICI (7) ══════════════════════════════ */
{ "Reservoir Sampling",   CAT_CLASSIC, kON,   "O(n)",          "LINEARE",
  "Reservoir Sampling \xe2\x80\x94 O(n) | Campionamento uniforme su stream\n"
  "Vitter (1985). Mantiene k campioni da stream di lunghezza sconosciuta. "
  "Ogni elemento ha probabilit\xc3\xa0 esatta k/n di essere incluso." },

{ "Floyd Cycle Detection",CAT_CLASSIC, kON,   "O(n)",          "LINEARE",
  "Floyd's Cycle Detection (Tartaruga e Lepre) \xe2\x80\x94 O(n) | O(1) spazio\n"
  "Robert Floyd (1967). Rileva cicli senza memoria extra. "
  "Applicazioni: lista collegata, generatori pseudo-random, duplicati in array 1..n." },

{ "Torri di Hanoi",       CAT_CLASSIC, kO2N,  "O(2\xe2\x81\xbf)",     "ESPONENZ.",
  "Torri di Hanoi \xe2\x80\x94 O(2\xe2\x81\xbf) | Ricorsione pura\n"
  "Eduard Lucas (1883). 3 pioli, n dischi: spostali rispettando la regola. "
  "Dimostra che la ricorsione risolve problemi impossibili iterativamente." },

{ "Game of Life (1D)",    CAT_CLASSIC, kON,   "O(n)",          "LINEARE",
  "Game of Life 1D (Automa cellulare) \xe2\x80\x94 O(n)\n"
  "John Conway (1970) in versione 1D. Ogni cella nasce/muore secondo i vicini. "
  "Esempio di computazione emergente: strutture complesse da regole semplici." },

{ "Rule 30 (Wolfram)",    CAT_CLASSIC, kON,   "O(n)",          "LINEARE",
  "Rule 30 \xe2\x80\x94 Automa Cellulare di Wolfram \xe2\x80\x94 O(n)\n"
  "Stephen Wolfram (1983). Genera pseudo-casualit\xc3\xa0 da regole deterministiche. "
  "Usato in: Mathematica per numeri casuali, crittografia, arte generativa." },

{ "Spiral Matrix",        CAT_CLASSIC, kON2,  "O(n\xc2\xb2)",         "LINEARE",
  "Spiral Matrix \xe2\x80\x94 O(n\xc2\xb2) | Problema di navigazione 2D\n"
  "Riempi o scorri una matrice a spirale. "
  "Appare in: interviste tech (Amazon, Google). Barre = ordine di visita (appiattito)." },

{ "Sierpinski Triangle",  CAT_CLASSIC, kON,   "O(n)",          "LINEARE",
  "Triangolo di Sierpinski \xe2\x80\x94 O(n) per riga\n"
  "Waclaw Sierpinski (1915). Frattale: triangoli dentro triangoli. "
  "Riga n del triangolo di Pascal mod 2. Barre = bit della riga corrente." },
};

/* ══════════════════════════════════════════════════════════════
   BigOWidget — implementazione
   ══════════════════════════════════════════════════════════════ */
double BigOWidget::evalClass(BigOClass cls, double n) {
    if (n < 1.0) n = 1.0;
    switch (cls) {
        case kO1:       return 1.0;
        case kOAlpha:   return 1.1;
        case kOLogLogN: return std::log2(std::log2(n) + 1.0) + 1.0;
        case kOLogN:    return std::log2(n) + 1.0;
        case kOSqrtN:   return std::sqrt(n);
        case kON:       return n;
        case kONLogLogN:return n * (std::log2(std::log2(n) + 1.0) + 1.0);
        case kONLogN:   return n * (std::log2(n) + 1.0);
        case kONLog2N:  { double lg = std::log2(n)+1.0; return n * lg * lg; }
        case kON2:      return n * n;
        case kON2W:     return n * n * 0.6;
        case kON3:      return n * n * n;
        case kO2N:      return std::min(std::pow(2.0, n), 1e12);
    }
    return n;
}
double BigOWidget::logScale(double v, double ref) {
    if (ref <= 1.0) return 1.0;
    double norm = std::log(v + 1.0) / std::log(ref + 1.0);
    return std::cbrt(norm);
}
BigOWidget::BigOWidget(QWidget* parent) : QWidget(parent) {
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setToolTip("Grafico O-grande: curva evidenziata = algoritmo selezionato.\n"
               "Grigio chiaro = altre classi.\n"
               "Asse X = dimensione input n, Asse Y = operazioni (scala log-cubica).");
}
void BigOWidget::set(BigOClass cls, const char* label, const char* badge) {
    m_cls = cls; m_label = label; m_badge = badge;
    update();
}
void BigOWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const int ml = 30, mr = 8, mt = 10, mb = 20;
    const int pw = width() - ml - mr;
    const int ph = height() - mt - mb;
    p.fillRect(rect(), QColor(20, 20, 35));
    p.fillRect(ml, mt, pw, ph, QColor(14, 14, 26));
    p.setPen(QPen(QColor(45, 45, 65), 1, Qt::DotLine));
    for (int i = 1; i < 4; i++) p.drawLine(ml, mt + ph * i / 4, ml + pw, mt + ph * i / 4);
    const double N_MAX = 32.0;
    const double refVal = evalClass(kON2, N_MAX);
    const int SMP = 80;
    struct CurveSpec { BigOClass cls; QColor col; };
    static const CurveSpec kBg[] = {
        { kO1,     {60,180,100} }, { kOLogN,  {70,200,140} },
        { kOSqrtN, {90,190,90}  }, { kON,     {200,200,60} },
        { kONLogN, {220,140,50} }, { kON2,    {200,80,60}  },
        { kO2N,    {160,50,160} },
    };
    for (const auto& cs : kBg) {
        if (cs.cls == m_cls) continue;
        QColor c = cs.col; c.setAlpha(50);
        p.setPen(QPen(c, 1.2));
        QPainterPath path; bool first = true;
        for (int s = 0; s <= SMP; s++) {
            double n = 1.0 + (N_MAX-1.0)*s/SMP;
            double yN = std::min(logScale(evalClass(cs.cls,n), refVal), 1.0);
            double px_ = ml + pw*s/SMP, py_ = mt + ph*(1.0-yN);
            if (first) { path.moveTo(px_,py_); first=false; } else path.lineTo(px_,py_);
        }
        p.drawPath(path);
    }
    /* Curva evidenziata */
    QColor hiCol;
    if      (m_cls <= kOLogN)   hiCol = {50,230,120};
    else if (m_cls <= kOSqrtN)  hiCol = {80,220,80};
    else if (m_cls <= kON)      hiCol = {200,210,60};
    else if (m_cls <= kONLogN)  hiCol = {230,150,50};
    else if (m_cls <= kONLog2N) hiCol = {230,120,40};
    else if (m_cls <= kON2W)    hiCol = {220,70,50};
    else                         hiCol = {180,50,180};
    p.setPen(QPen(hiCol, 2.5));
    QPainterPath hiPath; bool first2=true; double lx=0,ly=0;
    for (int s = 0; s <= SMP; s++) {
        double n = 1.0 + (N_MAX-1.0)*s/SMP;
        double yN = std::min(logScale(evalClass(m_cls,n), refVal), 1.0);
        double px_ = ml + pw*s/SMP, py_ = mt + ph*(1.0-yN);
        if (first2) { hiPath.moveTo(px_,py_); first2=false; } else hiPath.lineTo(px_,py_);
        lx=px_; ly=py_;
    }
    p.drawPath(hiPath);
    QFont lf; lf.setPixelSize(10); lf.setBold(true); p.setFont(lf); p.setPen(hiCol);
    double ely = std::max((double)mt+4, std::min(ly-6, (double)(mt+ph-14)));
    p.drawText((int)lx-60,(int)ely,58,14,Qt::AlignRight|Qt::AlignVCenter,QString::fromUtf8(m_label));
    QFont bf; bf.setPixelSize(9); bf.setBold(true); p.setFont(bf);
    QRect br(ml+pw-62, mt+ph-16, 60, 14);
    p.fillRect(br, hiCol.darker(180)); p.setPen(hiCol);
    p.drawText(br, Qt::AlignCenter, QString::fromUtf8(m_badge));
    /* Assi */
    p.setPen(QPen(QColor(100,100,130),1));
    p.drawLine(ml,mt,ml,mt+ph); p.drawLine(ml,mt+ph,ml+pw,mt+ph);
    QFont axF; axF.setPixelSize(9); p.setFont(axF); p.setPen(QColor(110,110,150));
    for (int nv : {8,16,32}) {
        int xp=(int)(ml+pw*(nv-1)/(N_MAX-1));
        p.drawLine(xp,mt+ph,xp,mt+ph+3);
        p.drawText(xp-8,mt+ph+4,16,11,Qt::AlignCenter,QString::number(nv));
    }
    p.drawText(ml+pw-8,mt+ph+4,12,11,Qt::AlignCenter,"n");
    p.save(); p.translate(8,mt+ph/2+14); p.rotate(-90);
    p.drawText(0,0,"ops"); p.restore();
    QFont tF; tF.setPixelSize(9); p.setFont(tF); p.setPen(QColor(80,80,110));
    p.drawText(ml+2,mt-1,pw-4,10,Qt::AlignLeft,"Complessit\xc3\xa0 O-grande");
}


/* ══════════════════════════════════════════════════════════════
   AlgoBarWidget — disegna array come barre colorate
   ══════════════════════════════════════════════════════════════ */
AlgoBarWidget::AlgoBarWidget(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(230);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAttribute(Qt::WA_StyledBackground, false);
}

void AlgoBarWidget::setStep(const AlgoStep& step) { m_step = step; update(); }

void AlgoBarWidget::paintEvent(QPaintEvent*) {
    if (m_step.arr.isEmpty()) return;
    QPainter p(this);

    const int n = m_step.arr.size();
    int maxVal  = 1;
    for (int v : m_step.arr) if (v > maxVal) maxVal = v;

    const int gap    = 4;
    const int yTop   = 16;
    const int H      = height() - 56;
    const int barW   = qMax(8, (width() - gap * (n + 1)) / n);
    const int totalW = barW * n + gap * (n + 1);
    const int xOff   = (width() - totalW) / 2;

    auto col = [&](int i) -> QColor {
        if (m_step.inactive.contains(i)) return {50,  50,  65};
        if (m_step.found.contains(i))    return {0,   210, 230};
        if (m_step.swp.contains(i))      return {210, 60,  60};
        if (m_step.cmp.contains(i))      return {220, 175, 0};
        if (m_step.sorted.contains(i))   return {70,  200, 100};
        return {80, 130, 200};
    };

    QFont fVal; fVal.setPixelSize(10);
    QFont fIdx; fIdx.setPixelSize(9);

    for (int i = 0; i < n; i++) {
        const int x    = xOff + gap + i * (barW + gap);
        const int barH = qMax(4, (int)((double)m_step.arr[i] / maxVal * H));
        const int y    = yTop + H - barH;
        const QColor c = col(i);

        p.fillRect(x, y, barW, barH, c);

        /* valore sopra */
        p.setPen({210, 210, 210});
        p.setFont(fVal);
        p.drawText(x - 2, y - 14, barW + 4, 13, Qt::AlignCenter,
                   QString::number(m_step.arr[i]));

        /* indice sotto */
        p.setPen({110, 110, 110});
        p.setFont(fIdx);
        p.drawText(x - 2, yTop + H + 4, barW + 4, 13, Qt::AlignCenter,
                   QString("[%1]").arg(i));
    }

    /* Legenda in basso */
    struct { QColor c; QString l; } leg[] = {
        {{80,130,200},  "normale"  },
        {{220,175,0},   "confronto"},
        {{210,60,60},   "scambio"  },
        {{70,200,100},  "ordinato" },
        {{0,210,230},   "trovato"  },
        {{50,50,65},    "eliminato"},
    };
    QFont fLeg; fLeg.setPixelSize(9);
    p.setFont(fLeg);
    int lx = 8, ly = height() - 14;
    for (auto& l : leg) {
        p.fillRect(lx, ly, 10, 10, l.c);
        p.setPen({130, 130, 130});
        p.drawText(lx + 13, ly + 10, l.l);
        lx += 68;
    }
}

/* ══════════════════════════════════════════════════════════════
   Costruttore — UI
   ══════════════════════════════════════════════════════════════ */
SimulatorePage::SimulatorePage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    ::srand((unsigned)::time(nullptr));

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(24, 16, 24, 12); lay->setSpacing(8);

    /* ── Header ── */
    auto* hdrW = new QWidget(this);
    auto* hdrL = new QHBoxLayout(hdrW); hdrL->setContentsMargins(0,0,0,0);
    auto* back  = new QPushButton("\u2190 Torna", hdrW); back->setObjectName("actionBtn");
    auto* title = new QLabel("\u26a1  Simulatore Algoritmi", hdrW); title->setObjectName("pageTitle");
    hdrL->addWidget(back); hdrL->addWidget(title, 1);
    lay->addWidget(hdrW);

    auto* div0 = new QFrame(this); div0->setObjectName("pageDivider"); div0->setFrameShape(QFrame::HLine);
    lay->addWidget(div0);

    /* ── Riga 1: Categoria + Algoritmo ── */
    auto* cfgW = new QWidget(this);
    auto* cfgL = new QHBoxLayout(cfgW); cfgL->setContentsMargins(0,0,0,0); cfgL->setSpacing(10);

    cfgL->addWidget(new QLabel("Categoria:", cfgW));
    m_catCmb = new QComboBox(cfgW);
    static const char* kCatNames[CAT_COUNT+1] = {
        "Tutti", "Ordinamento", "Ricerca", "Strutture Dati",
        "Grafi", "Prog. Dinamica", "Greedy",
        "Backtracking", "Stringhe", "Matematica",
        "Pattern Array", "Classici"
    };
    for (int c = -1; c < (int)CAT_COUNT; c++)
        m_catCmb->addItem(QString::fromUtf8(c < 0 ? kCatNames[0] : kCatNames[c+1]));
    cfgL->addWidget(m_catCmb);

    cfgL->addWidget(new QLabel("Algoritmo:", cfgW));
    m_algoCmb = new QComboBox(cfgW);
    m_algoCmb->setMinimumWidth(200);
    cfgL->addWidget(m_algoCmb, 2);

    cfgL->addWidget(new QLabel("N:", cfgW));
    m_sizeCmb = new QComboBox(cfgW);
    m_sizeCmb->addItems({"6","8","10","12"});
    m_sizeCmb->setCurrentIndex(1);
    cfgL->addWidget(m_sizeCmb);

    auto* newBtn = new QPushButton("\U0001f504  Nuovo Array", cfgW); newBtn->setObjectName("actionBtn");
    cfgL->addWidget(newBtn);

    /* ── Grafico O-grande a destra del pulsante ── */
    m_bigO = new BigOWidget(cfgW);
    cfgL->addWidget(m_bigO);
    cfgL->addStretch(1);
    lay->addWidget(cfgW);

    /* ── Descrizione ── */
    m_descLbl = new QLabel("", this);
    m_descLbl->setObjectName("cardDesc"); m_descLbl->setWordWrap(true);
    lay->addWidget(m_descLbl);

    auto* div1 = new QFrame(this); div1->setObjectName("pageDivider"); div1->setFrameShape(QFrame::HLine);
    lay->addWidget(div1);

    /* ── Visualizzatore ── */
    m_vis = new AlgoBarWidget(this);
    lay->addWidget(m_vis, 1);

    /* ── Messaggio passo ── */
    m_msgLbl = new QLabel("", this);
    m_msgLbl->setObjectName("cardTitle"); m_msgLbl->setWordWrap(true);
    m_msgLbl->setAlignment(Qt::AlignCenter);
    lay->addWidget(m_msgLbl);

    auto* div2 = new QFrame(this); div2->setObjectName("pageDivider"); div2->setFrameShape(QFrame::HLine);
    lay->addWidget(div2);

    /* ── Navigazione ── */
    auto* navW = new QWidget(this);
    auto* navL = new QHBoxLayout(navW); navL->setContentsMargins(0,0,0,0); navL->setSpacing(8);
    m_prevBtn = new QPushButton("\u25c4 Prec", navW); m_prevBtn->setObjectName("actionBtn");
    m_stepLbl = new QLabel("Passo 0/0", navW); m_stepLbl->setObjectName("cardDesc");
    m_stepLbl->setAlignment(Qt::AlignCenter); m_stepLbl->setMinimumWidth(110);
    m_nextBtn = new QPushButton("Succ \u25ba", navW); m_nextBtn->setObjectName("actionBtn");
    m_autoBtn = new QPushButton("\u25b6\u25b6 Auto", navW); m_autoBtn->setObjectName("actionBtn");
    navL->addWidget(m_prevBtn); navL->addWidget(m_stepLbl); navL->addWidget(m_nextBtn);
    navL->addStretch(1);
    navL->addWidget(m_autoBtn);
    navL->addWidget(new QLabel("Velocit\xc3\xa0:", navW));
    m_speedSlider = new QSlider(Qt::Horizontal, navW);
    m_speedSlider->setRange(200, 1500); m_speedSlider->setValue(700);
    m_speedSlider->setFixedWidth(120);
    m_speedSlider->setToolTip("Sinistra = lento   Destra = veloce");
    navL->addWidget(m_speedSlider);
    lay->addWidget(navW);

    /* ── Pannello AI ── */
    auto* div3 = new QFrame(this); div3->setObjectName("pageDivider"); div3->setFrameShape(QFrame::HLine);
    lay->addWidget(div3);

    auto* aiRow = new QWidget(this);
    auto* aiRL  = new QHBoxLayout(aiRow); aiRL->setContentsMargins(0,0,0,0); aiRL->setSpacing(8);
    auto* aiAskBtn = new QPushButton("\U0001f916  Chiedi all'AI sull'algoritmo", aiRow);
    aiAskBtn->setObjectName("actionBtn");
    m_aiStopBtn = new QPushButton("\u23f9", aiRow);
    m_aiStopBtn->setObjectName("actionBtn"); m_aiStopBtn->setProperty("danger", true);
    m_aiStopBtn->setFixedWidth(40); m_aiStopBtn->setEnabled(false);
    aiRL->addWidget(aiAskBtn); aiRL->addWidget(m_aiStopBtn); aiRL->addStretch(1);
    lay->addWidget(aiRow);

    m_aiLog = new QTextEdit(this);
    m_aiLog->setObjectName("chatLog"); m_aiLog->setReadOnly(true);
    m_aiLog->setMaximumHeight(160); m_aiLog->setVisible(false);
    lay->addWidget(m_aiLog);

    m_aiWaitLbl = new QLabel("\xe2\x8f\xb3  Elaborazione in corso \xe2\x80\x94 il modello sta generando la risposta...", this);
    m_aiWaitLbl->setObjectName("cardDesc");
    m_aiWaitLbl->setStyleSheet("color: #E5C400; padding: 4px 0;");
    m_aiWaitLbl->setVisible(false);
    lay->addWidget(m_aiWaitLbl);

    /* ── Timer ── */
    m_timer = new QTimer(this);

    /* ── Connessioni ── */
    connect(back,    &QPushButton::clicked, this, &SimulatorePage::backRequested);
    connect(newBtn,  &QPushButton::clicked, this, [this]{ buildSteps(); });
    connect(m_algoCmb, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int i){
        if (i >= 0 && i < m_filteredIdx.size()) m_globalIdx = m_filteredIdx[i];
        if (!m_filteredIdx.isEmpty()) {
            m_descLbl->setText(QString::fromUtf8(kAlgos[m_globalIdx].desc));
            m_bigO->set(kAlgos[m_globalIdx].complexity,
                        kAlgos[m_globalIdx].bigOLabel,
                        kAlgos[m_globalIdx].badge);
        }
        buildSteps();
    });
    connect(m_catCmb, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int catIdx){
        rebuildAlgoCmb(catIdx);
        buildSteps();
    });
    connect(m_sizeCmb, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]{ buildSteps(); });
    connect(m_prevBtn, &QPushButton::clicked, this, [this]{
        stopAuto(); if (m_curStep > 0) showStep(m_curStep - 1);
    });
    connect(m_nextBtn, &QPushButton::clicked, this, [this]{
        stopAuto(); if (m_curStep < m_steps.size()-1) showStep(m_curStep + 1);
    });
    connect(m_autoBtn, &QPushButton::clicked, this, [this]{
        m_timer->isActive() ? stopAuto() : startAuto();
    });
    connect(m_timer, &QTimer::timeout, this, [this]{
        if (m_curStep < m_steps.size()-1) showStep(m_curStep + 1);
        else stopAuto();
    });
    connect(m_speedSlider, &QSlider::valueChanged, this, [this](int v){
        if (m_timer->isActive()) m_timer->setInterval(1600 - v);
    });
    connect(m_aiStopBtn, &QPushButton::clicked, m_ai, &AiClient::abort);

    connect(aiAskBtn, &QPushButton::clicked, this, [this, aiAskBtn]{
        disconnect(m_aiCTok); disconnect(m_aiCFin); disconnect(m_aiCErr); disconnect(m_aiCAbo);
        m_aiLog->setVisible(true);
        m_aiLog->clear();
        const QString algo = m_algoCmb->currentText();
        m_aiLog->append(QString("\U0001f916  %1 — spiegazione AI:\n").arg(algo));
        aiAskBtn->setEnabled(false); m_aiStopBtn->setEnabled(true);
        m_aiWaitLbl->setVisible(true);

        const QString sys =
            "Sei un esperto di algoritmi e strutture dati. Spiega in modo chiaro "
            "con esempi pratici e casi d'uso reali. Rispondi SEMPRE e SOLO in italiano.";
        const QString usr = QString(
            "Spiega %1 in modo approfondito:\n"
            "1) Idea centrale e intuizione\n"
            "2) Complessit\xc3\xa0 temporale e spaziale (best/avg/worst)\n"
            "3) Quando usarlo e quando evitarlo\n"
            "4) Confronto con gli algoritmi pi\xc3\xb9 simili\n"
            "5) Un esempio concreto di applicazione reale"
        ).arg(algo);

        m_aiCTok = connect(m_ai, &AiClient::token, this, [this](const QString& t){
            m_aiWaitLbl->setVisible(false);   /* nasconde "in elaborazione" al primo token */
            QTextCursor c(m_aiLog->document()); c.movePosition(QTextCursor::End);
            c.insertText(t); m_aiLog->ensureCursorVisible();
        });
        auto done = [this, aiAskBtn]{
            disconnect(m_aiCTok); disconnect(m_aiCFin); disconnect(m_aiCErr); disconnect(m_aiCAbo);
            aiAskBtn->setEnabled(true); m_aiStopBtn->setEnabled(false);
            m_aiWaitLbl->setVisible(false);
            m_aiLog->append("\n\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500");
        };
        m_aiCFin = connect(m_ai, &AiClient::finished, this, [done](const QString&){ done(); });
        m_aiCErr = connect(m_ai, &AiClient::error,    this, [this, done](const QString& e){
            m_aiLog->append(QString("\n\xe2\x9d\x8c %1").arg(e)); done();
        });
        m_aiCAbo = connect(m_ai, &AiClient::aborted,  this, [this, done]{
            m_aiLog->append("\n\xe2\x8f\xb9 Interrotto."); done();
        });
        m_ai->chat(sys, usr);
    });

    /* Build iniziale */
    rebuildAlgoCmb(0);
    buildSteps();

    /* ── Click su grafico barre o BigO → apre Monitor ── */
    connect(m_vis,  &AlgoBarWidget::clicked, this, [this]{ emit openMonitorRequested(); });
    connect(m_bigO, &BigOWidget::clicked,    this, [this]{ emit openMonitorRequested(); });
}

