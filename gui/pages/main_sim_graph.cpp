/* main_sim_graph.cpp — Graph, BFS/DFS/Dijkstra, Data Structures, DP */
#include "main_simulator.h"
#include <QQueue>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <cmath>


static const int GRAPH_N = 8;
/* adj[u] = lista di (v, peso) */
static const int GRAPH_EDGES[][3] = {
    {0,1,4},{0,2,1},{1,3,2},{1,4,5},{2,3,8},{2,5,2},{3,6,3},{4,7,6},{5,6,1},{6,7,4}
};
static const int GRAPH_NE = 10;

/* Costruisce lista di adiacenza non pesata */
static QVector<QVector<int>> makeAdj() {
    QVector<QVector<int>> adj(GRAPH_N);
    for (int i = 0; i < GRAPH_NE; i++) {
        adj[GRAPH_EDGES[i][0]] << GRAPH_EDGES[i][1];
        adj[GRAPH_EDGES[i][1]] << GRAPH_EDGES[i][0];
    }
    return adj;
}


QVector<AlgoStep> SimulatorePage::genBFS() {
    QVector<AlgoStep> steps;
    QVector<int> dist(GRAPH_N, 99);
    dist[0] = 0;
    QVector<int> visited;
    auto adj = makeAdj();

    { AlgoStep s; s.arr = dist;
      s.msg = "BFS — 8 nodi. Barre = distanza dal nodo 0. Esplora per livelli.";
      steps << s; }

    QVector<int> queue = {0};
    while (!queue.isEmpty()) {
        int u = queue.takeFirst(); visited << u;
        AlgoStep s; s.arr = dist; s.cmp << u; s.sorted = visited;
        s.msg = QString("Visito nodo %1 (dist=%2)").arg(u).arg(dist[u]);
        steps << s;
        for (int v : adj[u]) {
            if (dist[v] == 99) {
                dist[v] = dist[u] + 1; queue << v;
                AlgoStep sv; sv.arr = dist; sv.found << v; sv.sorted = visited;
                sv.msg = QString("Scoperto nodo %1 (dist=%2) da %3").arg(v).arg(dist[v]).arg(u);
                steps << sv;
            }
        }
    }
    { AlgoStep sf; sf.arr = dist; sf.sorted = visited;
      sf.msg = "BFS completata! Distanze minime da nodo 0 calcolate.";
      steps << sf; }
    return steps;
}

QVector<AlgoStep> SimulatorePage::genDFS() {
    QVector<AlgoStep> steps;
    QVector<int> time_(GRAPH_N, 0);
    QVector<bool> vis(GRAPH_N, false);
    auto adj = makeAdj();
    QVector<int> visited;
    int t = 0;

    { AlgoStep s; s.arr = time_;
      s.msg = "DFS — 8 nodi. Barre = timestamp di scoperta. Esplora in profondità.";
      steps << s; }

    std::function<void(int)> dfs = [&](int u) {
        vis[u] = true; time_[u] = ++t; visited << u;
        AlgoStep s; s.arr = time_; s.cmp << u; s.sorted = visited;
        s.msg = QString("Scopro nodo %1 (time=%2)").arg(u).arg(t);
        steps << s;
        for (int v : adj[u]) {
            if (!vis[v]) {
                AlgoStep se; se.arr = time_; se.cmp << u << v; se.sorted = visited;
                se.msg = QString("Arco albero %1→%2").arg(u).arg(v);
                steps << se;
                dfs(v);
                AlgoStep sr; sr.arr = time_; sr.found << u; sr.sorted = visited;
                sr.msg = QString("Ritorno a %1 da %2").arg(u).arg(v);
                steps << sr;
            }
        }
    };
    dfs(0);
    { AlgoStep sf; sf.arr = time_; sf.sorted = visited;
      sf.msg = "DFS completata! Tutti i nodi raggiunti.";
      steps << sf; }
    return steps;
}

QVector<AlgoStep> SimulatorePage::genDijkstra() {
    QVector<AlgoStep> steps;
    const int INF = 99;
    QVector<int> dist(GRAPH_N, INF); dist[0] = 0;
    QVector<bool> vis(GRAPH_N, false);
    QVector<int> settled;

    { AlgoStep s; s.arr = dist;
      s.msg = "Dijkstra — 8 nodi pesati. Barre = distanza minima dal nodo 0.";
      steps << s; }

    for (int iter = 0; iter < GRAPH_N; iter++) {
        /* trova nodo non visitato con dist minima */
        int u = -1;
        for (int i = 0; i < GRAPH_N; i++)
            if (!vis[i] && (u == -1 || dist[i] < dist[u])) u = i;
        if (u == -1 || dist[u] == INF) break;
        vis[u] = true; settled << u;
        AlgoStep s; s.arr = dist; s.cmp << u; s.sorted = settled;
        s.msg = QString("Estrai nodo %1 (dist=%2) — definitivo").arg(u).arg(dist[u]);
        steps << s;

        for (int e = 0; e < GRAPH_NE; e++) {
            int a = GRAPH_EDGES[e][0], b = GRAPH_EDGES[e][1], w = GRAPH_EDGES[e][2];
            int v = (a == u) ? b : (b == u) ? a : -1;
            if (v == -1 || vis[v]) continue;
            if (dist[u] + w < dist[v]) {
                dist[v] = dist[u] + w;
                AlgoStep sv; sv.arr = dist; sv.found << v; sv.sorted = settled;
                sv.msg = QString("Rilassa %1→%2 (peso=%3): dist[%2]=%4").arg(u).arg(v).arg(w).arg(dist[v]);
                steps << sv;
            }
        }
    }
    { AlgoStep sf; sf.arr = dist; sf.sorted = settled;
      sf.msg = "Dijkstra completato! Tutti i cammini minimi da nodo 0.";
      steps << sf; }
    return steps;
}

QVector<AlgoStep> SimulatorePage::genBellmanFord() {
    QVector<AlgoStep> steps;
    const int INF = 99;
    QVector<int> dist(GRAPH_N, INF); dist[0] = 0;

    { AlgoStep s; s.arr = dist;
      s.msg = "Bellman-Ford — 8 nodi. Rilassa tutti gli archi V-1 = 7 volte.";
      steps << s; }

    for (int iter = 1; iter < GRAPH_N; iter++) {
        bool changed = false;
        for (int e = 0; e < GRAPH_NE; e++) {
            int u = GRAPH_EDGES[e][0], v = GRAPH_EDGES[e][1], w = GRAPH_EDGES[e][2];
            for (int dir = 0; dir < 2; dir++) {
                if (dir == 1) { std::swap(u, v); }
                if (dist[u] != INF && dist[u] + w < dist[v]) {
                    dist[v] = dist[u] + w; changed = true;
                    AlgoStep s; s.arr = dist; s.swp << v; s.cmp << u;
                    s.msg = QString("Iter %1: rilassa %2→%3 (w=%4) dist[%3]=%5")
                            .arg(iter).arg(u).arg(v).arg(w).arg(dist[v]);
                    steps << s;
                }
            }
        }
        if (!changed) {
            AlgoStep s; s.arr = dist;
            s.msg = QString("Iter %1: nessuna modifica — terminato in anticipo!").arg(iter);
            steps << s; break;
        }
    }
    QVector<int> all; for (int i = 0; i < GRAPH_N; i++) all << i;
    { AlgoStep sf; sf.arr = dist; sf.sorted = all;
      sf.msg = "Bellman-Ford completato! Nessun ciclo negativo rilevato.";
      steps << sf; }
    return steps;
}

QVector<AlgoStep> SimulatorePage::genTopologicalSort() {
    QVector<AlgoStep> steps;
    /* DAG: 0→1, 0→2, 1→3, 1→4, 2→4, 2→5, 3→6, 4→6, 5→7, 6→7 */
    QVector<QVector<int>> dag(GRAPH_N);
    int dagEdges[][2] = {{0,1},{0,2},{1,3},{1,4},{2,4},{2,5},{3,6},{4,6},{5,7},{6,7}};
    QVector<int> inDeg(GRAPH_N, 0);
    for (auto& e : dagEdges) { dag[e[0]] << e[1]; inDeg[e[1]]++; }

    QVector<int> topoOrder(GRAPH_N, 0);
    int pos = 0;
    QVector<int> queue, settled;
    for (int i = 0; i < GRAPH_N; i++) if (inDeg[i] == 0) queue << i;

    { AlgoStep s; s.arr = inDeg;
      s.msg = "Topological Sort (Kahn) — barre = in-degree. DAG: 0→1→3→6→7...";
      steps << s; }

    while (!queue.isEmpty()) {
        int u = queue.takeFirst(); topoOrder[u] = pos++;
        settled << u;
        AlgoStep s; s.arr = inDeg; s.found << u; s.sorted = settled;
        s.msg = QString("Elaboro nodo %1 (in-degree=0, pos=%2)").arg(u).arg(pos-1);
        steps << s;
        for (int v : dag[u]) {
            inDeg[v]--;
            AlgoStep sv; sv.arr = inDeg; sv.cmp << v; sv.sorted = settled;
            sv.msg = QString("Rimuovi arco %1→%2: in-degree[%2]=%3").arg(u).arg(v).arg(inDeg[v]);
            steps << sv;
            if (inDeg[v] == 0) queue << v;
        }
    }
    { AlgoStep sf; sf.arr = inDeg; sf.sorted = settled;
      sf.msg = "Topological Sort completato! Ordine: 0,1,2,3,4,5,6,7";
      steps << sf; }
    return steps;
}

QVector<AlgoStep> SimulatorePage::genUnionFind() {
    QVector<AlgoStep> steps;
    /* parent[i] = i all'inizio */
    QVector<int> parent(GRAPH_N);
    QVector<int> rank_(GRAPH_N, 0);
    for (int i = 0; i < GRAPH_N; i++) parent[i] = i;

    { AlgoStep s; s.arr = parent;
      s.msg = "Union-Find — barre = parent[i]. Inizialmente ogni nodo è radice di sé stesso.";
      steps << s; }

    std::function<int(int)> find = [&](int x) -> int {
        while (parent[x] != x) x = parent[x] = parent[parent[x]]; /* path compression */
        return x;
    };

    for (int e = 0; e < GRAPH_NE && e < 6; e++) {
        int u = GRAPH_EDGES[e][0], v = GRAPH_EDGES[e][1];
        int ru = find(u), rv = find(v);
        AlgoStep s; s.arr = parent; s.cmp << u << v;
        s.msg = QString("Union(%1,%2): radici %3 e %4").arg(u).arg(v).arg(ru).arg(rv);
        steps << s;
        if (ru != rv) {
            if (rank_[ru] < rank_[rv]) std::swap(ru, rv);
            parent[rv] = ru;
            if (rank_[ru] == rank_[rv]) rank_[ru]++;
            AlgoStep su; su.arr = parent; su.swp << rv;
            su.msg = QString("parent[%1] = %2 (union by rank)").arg(rv).arg(ru);
            steps << su;
        } else {
            AlgoStep sc; sc.arr = parent; sc.found << ru;
            sc.msg = QString("Stesso componente! %1 e %2 già connessi.").arg(u).arg(v);
            steps << sc;
        }
    }
    { AlgoStep sf; sf.arr = parent;
      sf.msg = "Union-Find completato! parent[i] = rappresentante del componente.";
      steps << sf; }
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   COIN CHANGE DP
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genCoinChange() {
    QVector<AlgoStep> steps;
    QVector<int> coins = {1, 3, 4};
    const int amount = 12;
    QVector<int> dp(amount + 1, 99);
    dp[0] = 0;

    { AlgoStep s; s.arr = dp.mid(0, qMin(13, dp.size()));
      s.msg = QString("Coin Change — monete {1,3,4}, importo=%1. dp[i]=min monete per i.").arg(amount);
      steps << s; }

    QVector<int> srt = {0};
    for (int i = 1; i <= amount; i++) {
        for (int c : coins) {
            if (c <= i && dp[i-c] + 1 < dp[i]) {
                dp[i] = dp[i-c] + 1;
                AlgoStep s; s.arr = dp.mid(0,13); s.cmp << i; s.sorted = srt;
                s.msg = QString("dp[%1] = dp[%2]+1 = %3 (moneta %4)").arg(i).arg(i-c).arg(dp[i]).arg(c);
                steps << s;
            }
        }
        srt << i;
        AlgoStep s; s.arr = dp.mid(0,13); s.sorted = srt;
        s.msg = QString("dp[%1] = %2 monete").arg(i).arg(dp[i] == 99 ? -1 : dp[i]);
        steps << s;
    }
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   LIS — Longest Increasing Subsequence (DP + BinarySearch)
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genLIS(QVector<int> arr) {
    QVector<AlgoStep> steps;
    const int n = arr.size();
    { AlgoStep s; s.arr = arr;
      s.msg = "LIS — Longest Increasing Subsequence. Patience sorting O(n log n).";
      steps << s; }

    QVector<int> piles; /* piles[i] = top della pila i */
    QVector<int> lisIdx; /* per visualizzazione */
    for (int i = 0; i < n; i++) {
        /* Binary search per trovare la pila giusta */
        int lo = 0, hi = (int)piles.size();
        while (lo < hi) {
            int mid = (lo + hi) / 2;
            if (piles[mid] < arr[i]) lo = mid + 1; else hi = mid;
        }
        if (lo == (int)piles.size()) piles << arr[i];
        else piles[lo] = arr[i];

        QVector<int> pilesVis = piles;
        while ((int)pilesVis.size() < n) pilesVis << 0;
        AlgoStep s; s.arr = pilesVis; s.cmp << lo;
        s.msg = QString("[%1]=%2 → pila %3 (LIS lunghezza=%4)").arg(i).arg(arr[i]).arg(lo).arg(piles.size());
        steps << s;
    }
    QVector<int> finalSrt; for (int i = 0; i < (int)piles.size(); i++) finalSrt << i;
    QVector<int> pilesVis = piles; while ((int)pilesVis.size() < n) pilesVis << 0;
    { AlgoStep sf; sf.arr = pilesVis; sf.sorted = finalSrt;
      sf.msg = QString("LIS completato! Lunghezza = %1").arg(piles.size());
      steps << sf; }
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   0/1 KNAPSACK
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genKnapsack() {
    QVector<AlgoStep> steps;
    QVector<int> weights = {2, 3, 4, 5};
    QVector<int> values  = {3, 4, 5, 8};
    const int W = 8, n = 4;
    /* dp[w] = max valore con capacità w */
    QVector<int> dp(W+1, 0);

    { AlgoStep s; s.arr = dp;
      s.msg = "0/1 Knapsack — 4 oggetti, capacità=8. dp[w]=valore massimo.";
      steps << s; }

    QVector<int> srt;
    for (int i = 0; i < n; i++) {
        /* Scorri a ritroso per evitare doppio uso */
        for (int w = W; w >= weights[i]; w--) {
            int newVal = dp[w - weights[i]] + values[i];
            if (newVal > dp[w]) {
                dp[w] = newVal;
                AlgoStep s; s.arr = dp; s.cmp << w; s.sorted = srt;
                s.msg = QString("Oggetto %1 (w=%2,v=%3): dp[%4]=%5")
                        .arg(i+1).arg(weights[i]).arg(values[i]).arg(w).arg(newVal);
                steps << s;
            }
        }
        srt << i+1;
        AlgoStep s; s.arr = dp;
        s.msg = QString("Dopo oggetto %1: valore massimo = %2").arg(i+1).arg(*std::max_element(dp.begin(), dp.end()));
        steps << s;
    }
    int maxV = *std::max_element(dp.begin(), dp.end());
    QVector<int> all; for (int i = 0; i <= W; i++) all << i;
    { AlgoStep sf; sf.arr = dp; sf.sorted = all;
      sf.msg = QString("Knapsack completato! Valore massimo = %1").arg(maxV);
      steps << sf; }
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   LCS — Longest Common Subsequence
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genLCS() {
    QVector<AlgoStep> steps;
    QString s1 = "ABCBDAB", s2 = "BDCAB";
    const int m = s1.size(), n = s2.size();
    /* Mostra la riga corrente della tabella DP */
    QVector<int> prev(n+1, 0), curr(n+1, 0);
    QVector<int> srt;

    { QVector<int> arr(n+1, 0); AlgoStep s; s.arr = arr;
      s.msg = QString("LCS(\"%1\", \"%2\"). Tabella DP riga per riga.").arg(s1).arg(s2);
      steps << s; }

    for (int i = 1; i <= m; i++) {
        for (int j = 1; j <= n; j++) {
            if (s1[i-1] == s2[j-1]) {
                curr[j] = prev[j-1] + 1;
                AlgoStep s; s.arr = curr; s.found << j;
                s.msg = QString("s1[%1]='%2'=s2[%3]='%4': dp[%5][%6]=%7 ✓")
                        .arg(i-1).arg(s1[i-1]).arg(j-1).arg(s2[j-1]).arg(i).arg(j).arg(curr[j]);
                steps << s;
            } else {
                curr[j] = qMax(prev[j], curr[j-1]);
                AlgoStep s; s.arr = curr; s.cmp << j;
                s.msg = QString("s1[%1]='%2'≠s2[%3]='%4': dp=%5")
                        .arg(i-1).arg(s1[i-1]).arg(j-1).arg(s2[j-1]).arg(curr[j]);
                steps << s;
            }
        }
        prev = curr; std::fill(curr.begin(), curr.end(), 0); curr[0] = 0;
        AlgoStep s; s.arr = prev;
        s.msg = QString("Riga %1 ('%2') completata. LCS parziale=%3").arg(i).arg(s1[i-1]).arg(prev[n]);
        steps << s;
    }
    QVector<int> all; for (int j = 0; j <= n; j++) all << j;
    { AlgoStep sf; sf.arr = prev; sf.sorted = all;
      sf.msg = QString("LCS(\"%1\",\"%2\") = %3 caratteri").arg(s1).arg(s2).arg(prev[n]);
      steps << sf; }
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   EDIT DISTANCE (Levenshtein)
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genEditDistance() {
    QVector<AlgoStep> steps;
    QString s1 = "KITTEN", s2 = "SITTING";
    const int m = s1.size(), n = s2.size();
    QVector<int> prev(n+1), curr(n+1);
    for (int j = 0; j <= n; j++) prev[j] = j;

    { AlgoStep s; s.arr = prev;
      s.msg = QString("Edit Distance(\"%1\"→\"%2\"). Operazioni: ins, del, sost.").arg(s1).arg(s2);
      steps << s; }

    for (int i = 1; i <= m; i++) {
        curr[0] = i;
        for (int j = 1; j <= n; j++) {
            int cost = (s1[i-1] == s2[j-1]) ? 0 : 1;
            curr[j] = qMin(prev[j]+1, qMin(curr[j-1]+1, prev[j-1]+cost));
            AlgoStep s; s.arr = curr; s.cmp << j;
            s.msg = QString("s1[%1]='%2' vs s2[%3]='%4': cost=%5, dp=%6")
                    .arg(i-1).arg(s1[i-1]).arg(j-1).arg(s2[j-1]).arg(cost).arg(curr[j]);
            steps << s;
        }
        prev = curr;
        AlgoStep s; s.arr = prev;
        s.msg = QString("Riga '%1' completata — dist parziale=%2").arg(s1[i-1]).arg(prev[n]);
        steps << s;
    }
    QVector<int> all; for (int j = 0; j <= n; j++) all << j;
    { AlgoStep sf; sf.arr = prev; sf.sorted = all;
      sf.msg = QString("Edit Distance(\"%1\",\"%2\") = %3 operazioni").arg(s1).arg(s2).arg(prev[n]);
      steps << sf; }
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   RESERVOIR SAMPLING
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genReservoirSampling(QVector<int> arr) {
    QVector<AlgoStep> steps;
    const int n = arr.size(), k = 3;
    { AlgoStep s; s.arr = arr;
      s.msg = QString("Reservoir Sampling — stream di %1 elem, mantieni k=%2 campioni uniformi.").arg(n).arg(k);
      steps << s; }

    QVector<int> reservoir;
    for (int i = 0; i < k && i < n; i++) { reservoir << arr[i]; }
    QVector<int> resIdx; for (int i = 0; i < k; i++) resIdx << i;
    { AlgoStep s; s.arr = arr; s.sorted = resIdx;
      s.msg = QString("Reservoir iniziale: elementi 0..%1").arg(k-1);
      steps << s; }

    for (int i = k; i < n; i++) {
        int j = rand() % (i + 1);
        AlgoStep s; s.arr = arr; s.cmp << i;
        s.msg = QString("Elemento [%1]=%2: j=%3 (casuale in [0..%4])").arg(i).arg(arr[i]).arg(j).arg(i);
        steps << s;
        if (j < k) {
            resIdx[j] = i;
            AlgoStep sw; sw.arr = arr; sw.swp << i; sw.sorted = resIdx;
            sw.msg = QString("j=%1 < k=%2: sostituisce reservoir[%3] con [%4]=%5").arg(j).arg(k).arg(j).arg(i).arg(arr[i]);
            steps << sw;
        } else {
            AlgoStep sk; sk.arr = arr; sk.inactive << i; sk.sorted = resIdx;
            sk.msg = QString("j=%1 >= k=%2: scartato").arg(j).arg(k);
            steps << sk;
        }
    }
    { AlgoStep sf; sf.arr = arr; sf.found = resIdx;
      sf.msg = QString("Campionamento completato! %1 elementi con prob. esatta k/n = %2/%3")
               .arg(k).arg(k).arg(n);
      steps << sf; }
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   FLOYD'S CYCLE DETECTION (Tortoise and Hare)
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genFloydCycle() {
    QVector<AlgoStep> steps;
    /* Lista come array: next[i] = prossimo nodo. Ciclo in posizione 3 */
    QVector<int> next_ = {1, 2, 3, 4, 5, 3, 3, 0}; /* ciclo: 3→4→5→3 */
    const int n = next_.size();
    { AlgoStep s; s.arr = next_;
      s.msg = "Floyd Cycle Detection — barre = next[i]. Ciclo: 3→4→5→3.";
      steps << s; }

    int tortoise = next_[0], hare = next_[next_[0]];
    int step = 1;
    while (tortoise != hare) {
        AlgoStep s; s.arr = next_; s.cmp << tortoise << hare;
        s.msg = QString("Step %1: tartaruga=%2, lepre=%3").arg(step++).arg(tortoise).arg(hare);
        steps << s;
        tortoise = next_[tortoise];
        hare     = next_[next_[hare]];
    }
    { AlgoStep s; s.arr = next_; s.found << tortoise;
      s.msg = QString("Incontro nel nodo %1 — ciclo rilevato!").arg(tortoise);
      steps << s; }

    /* Trova inizio ciclo */
    int mu = 0; tortoise = 0;
    while (tortoise != hare) {
        tortoise = next_[tortoise]; hare = next_[hare]; mu++;
        AlgoStep s; s.arr = next_; s.cmp << tortoise << hare;
        s.msg = QString("Cerca inizio ciclo: t=%1 h=%2").arg(tortoise).arg(hare);
        steps << s;
    }
    { AlgoStep sf; sf.arr = next_; sf.sorted << tortoise;
      sf.msg = QString("Inizio ciclo: nodo %1, lunghezza ciclo calcolata.").arg(tortoise);
      steps << sf; }
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   TORRE DI HANOI (ricorsiva, n dischi, 3 pioli come barre)
   ══════════════════════════════════════════════════════════════ */
QVector<AlgoStep> SimulatorePage::genTowerOfHanoi(int n) {
    QVector<AlgoStep> steps;
    /* Codifica stato: barre = [top piolo A, top piolo B, top piolo C, mosse, dischi totali, ...]*/
    QVector<int> peg(3, 0);
    /* Conta dischi per piolo (come altezza barra) */
    QVector<int> pegH = {n, 0, 0};
    { QVector<int> arr = {n*10, 0, 0}; AlgoStep s; s.arr = arr;
      s.msg = QString("Torri di Hanoi — %1 dischi. Barre = dischi su ogni piolo.").arg(n);
      steps << s; }

    int moves = 0;
    std::function<void(int,int,int,int)> hanoi = [&](int discs, int from, int to, int aux) {
        if (discs == 0) return;
        hanoi(discs-1, from, aux, to);
        pegH[from]--; pegH[to]++;
        moves++;
        QVector<int> arr = {pegH[0]*10, pegH[1]*10, pegH[2]*10};
        AlgoStep s; s.arr = arr; s.swp << from << to;
        s.msg = QString("Mossa %1: disco da piolo %2 → piolo %3 (rimangono: A=%4 B=%5 C=%6)")
                .arg(moves).arg(from).arg(to).arg(pegH[0]).arg(pegH[1]).arg(pegH[2]);
        steps << s;
        hanoi(discs-1, aux, to, from);
    };
    hanoi(n, 0, 2, 1);
    { QVector<int> arr = {0, 0, pegH[2]*10}; AlgoStep sf; sf.arr = arr; sf.found << 2;
      sf.msg = QString("Completato! %1 mosse totali (= 2^%2 - 1).").arg(moves).arg(n);
      steps << sf; }
    return steps;
}

/* ══════════════════════════════════════════════════════════════
   NUOVI ALGORITMI — implementazioni gen*
   ══════════════════════════════════════════════════════════════ */

/* ── BucketSort ── */
QVector<AlgoStep> SimulatorePage::genBucketSort(QVector<int> arr) {
    QVector<AlgoStep> st;
    int n = arr.size();
    int mx = *std::max_element(arr.begin(), arr.end()) + 1;
    int nb = qMax(2, n/2);
    auto snap = [&](QVector<int> cmp, QVector<int> swp, QVector<int> srt, QString msg){
        AlgoStep s; s.arr=arr; s.cmp=cmp; s.swp=swp; s.sorted=srt; s.msg=msg; st<<s;
    };
    /* distribuzione nei bucket */
    QVector<QVector<int>> buckets(nb);
    for(int i=0;i<n;i++){
        int bi = arr[i]*nb/mx;
        if(bi>=nb) bi=nb-1;
        snap({i},{},{}, QString("Elemento arr[%1]=%2 → bucket %3").arg(i).arg(arr[i]).arg(bi));
        buckets[bi] << arr[i];
    }
    /* insertion sort su ogni bucket + raccolta */
    QVector<int> res;
    for(int b=0;b<nb;b++){
        std::sort(buckets[b].begin(), buckets[b].end());
        for(int v : buckets[b]) res<<v;
        if(!buckets[b].isEmpty()){
            QVector<int> srt; for(int i=0;i<res.size();i++) srt<<i;
            snap({},{},srt, QString("Bucket %1 ordinato e raccolto (%2 elementi)").arg(b).arg(buckets[b].size()));
        }
    }
    arr = res;
    QVector<int> all; for(int i=0;i<n;i++) all<<i;
    AlgoStep sf; sf.arr=arr; sf.sorted=all; sf.msg="BucketSort completato!"; st<<sf;
    return st;
}

/* ── TimSort ── */
QVector<AlgoStep> SimulatorePage::genTimSort(QVector<int> arr) {
    QVector<AlgoStep> st;
    int n = arr.size();
    const int RUN = 4;
    auto snap = [&](QVector<int> cmp, QVector<int> srt, QString msg){
        AlgoStep s; s.arr=arr; s.cmp=cmp; s.sorted=srt; s.msg=msg; st<<s;
    };
    /* insertion sort su ogni run */
    for(int i=0;i<n;i+=RUN){
        int r = qMin(i+RUN-1, n-1);
        for(int j=i+1;j<=r;j++){
            int key=arr[j], k=j-1;
            snap({j},{}, QString("Insertion Sort: run [%1..%2], inserisco %3").arg(i).arg(r).arg(key));
            while(k>=i && arr[k]>key){ arr[k+1]=arr[k]; k--; }
            arr[k+1]=key;
        }
        QVector<int> srt; for(int x=i;x<=r;x++) srt<<x;
        snap({},srt, QString("Run [%1..%2] ordinata").arg(i).arg(r));
    }
    /* merge dei run */
    for(int sz=RUN;sz<n;sz*=2){
        for(int lo=0;lo<n;lo+=2*sz){
            int mid=qMin(lo+sz-1, n-1);
            int hi=qMin(lo+2*sz-1, n-1);
            if(mid>=hi) continue;
            QVector<int> cmp; for(int x=lo;x<=hi;x++) cmp<<x;
            snap(cmp,{}, QString("Merge [%1..%2] con [%3..%4]").arg(lo).arg(mid).arg(mid+1).arg(hi));
            QVector<int> tmp;
            int a=lo, b=mid+1;
            while(a<=mid&&b<=hi){ if(arr[a]<=arr[b]) tmp<<arr[a++]; else tmp<<arr[b++]; }
            while(a<=mid) tmp<<arr[a++];
            while(b<=hi)  tmp<<arr[b++];
            for(int x=lo;x<=hi;x++) arr[x]=tmp[x-lo];
            QVector<int> srt; for(int x=lo;x<=hi;x++) srt<<x;
            snap({},srt, QString("Merge [%1..%2] completato").arg(lo).arg(hi));
        }
    }
    QVector<int> all; for(int i=0;i<n;i++) all<<i;
    AlgoStep sf; sf.arr=arr; sf.sorted=all; sf.msg="TimSort completato (ibrido Insertion+Merge)!"; st<<sf;
    return st;
}

/* ── StoogeSort ── */
void SimulatorePage::_stoogeRec(QVector<int>& a, int l, int r, QVector<AlgoStep>& st) {
    if(a[l]>a[r]){ std::swap(a[l],a[r]);
        AlgoStep s; s.arr=a; s.swp<<l<<r;
        s.msg=QString("Scambio a[%1]=%2 ↔ a[%3]=%4").arg(l).arg(a[r]).arg(r).arg(a[l]); st<<s; }
    if(r-l+1 > 2){
        int t=(r-l+1)/3;
        { AlgoStep s; s.arr=a; s.cmp<<l<<r;
          s.msg=QString("Stooge: ordina 2/3 iniziali [%1..%2]").arg(l).arg(r-t); st<<s; }
        _stoogeRec(a,l,r-t,st);
        { AlgoStep s; s.arr=a; s.cmp<<l<<r;
          s.msg=QString("Stooge: ordina 2/3 finali [%1..%2]").arg(l+t).arg(r); st<<s; }
        _stoogeRec(a,l+t,r,st);
        { AlgoStep s; s.arr=a; s.cmp<<l<<r;
          s.msg=QString("Stooge: ordina di nuovo 2/3 iniziali [%1..%2]").arg(l).arg(r-t); st<<s; }
        _stoogeRec(a,l,r-t,st);
    }
}
QVector<AlgoStep> SimulatorePage::genStoogeSort(QVector<int> arr) {
    QVector<AlgoStep> st;
    { AlgoStep s; s.arr=arr; s.msg="StoogeSort: ricorsione su 2/3 dell'array (3 chiamate per livello)"; st<<s; }
    int n = qMin((int)arr.size(), 7); /* limita per evitare troppi passi */
    arr.resize(n);
    _stoogeRec(arr,0,n-1,st);
    QVector<int> all; for(int i=0;i<n;i++) all<<i;
    AlgoStep sf; sf.arr=arr; sf.sorted=all; sf.msg="StoogeSort completato! O(n^2.7) — MAI usare in produzione."; st<<sf;
    return st;
}

/* ── BoyerMooreVoting ── */
QVector<AlgoStep> SimulatorePage::genBoyerMooreVoting(QVector<int> arr) {
    QVector<AlgoStep> st;
    int n=arr.size();
    /* crea array con un elemento maggioritario garantito */
    int maj = arr[rand()%n];
    for(int i=0;i<n*2/3;i++) arr[i]=maj;
    arr.resize(n);
    for(int i=n-1;i>0;i--){ int j=rand()%(i+1); std::swap(arr[i],arr[j]); }
    int candidate=arr[0], count=1;
    { AlgoStep s; s.arr=arr; s.cmp<<0; s.found<<0;
      s.msg=QString("Inizio: candidato=%1, count=1").arg(candidate); st<<s; }
    for(int i=1;i<n;i++){
        if(count==0){ candidate=arr[i]; count=1;
            AlgoStep s; s.arr=arr; s.cmp<<i;
            s.msg=QString("count=0 → nuovo candidato=%1").arg(candidate); st<<s; }
        else if(arr[i]==candidate){ count++;
            QVector<int> fi; for(int j=0;j<=i;j++) if(arr[j]==candidate) fi<<j;
            AlgoStep s; s.arr=arr; s.cmp<<i; s.found=fi;
            s.msg=QString("arr[%1]=%2 = candidato → count=%3").arg(i).arg(arr[i]).arg(count); st<<s; }
        else { count--;
            AlgoStep s; s.arr=arr; s.cmp<<i; s.swp<<i;
            s.msg=QString("arr[%1]=%2 ≠ candidato → count=%3").arg(i).arg(arr[i]).arg(count); st<<s; }
    }
    QVector<int> fi; for(int i=0;i<n;i++) if(arr[i]==candidate) fi<<i;
    AlgoStep sf; sf.arr=arr; sf.found=fi;
    sf.msg=QString("Maggioritario trovato: %1 (%2 volte, >n/2=%3)").arg(candidate).arg(fi.size()).arg(n/2); st<<sf;
    return st;
}

/* ── Stack ── */
QVector<AlgoStep> SimulatorePage::genStack(QVector<int> arr) {
    QVector<AlgoStep> st;
    QVector<int> stk;
    int n=qMin((int)arr.size(),8);
    { AlgoStep s; s.arr=arr; s.msg="Stack LIFO: visualizzazione push/pop (cima a destra)"; st<<s; }
    for(int i=0;i<n;i++){
        stk<<arr[i];
        AlgoStep s; s.arr=stk;
        if(!stk.isEmpty()) s.cmp<<(stk.size()-1);
        s.msg=QString("PUSH %1 → top=%1 (size=%2)").arg(arr[i]).arg(stk.size()); st<<s;
    }
    for(int i=0;i<n/2;i++){
        int top=stk.last(); stk.pop_back();
        AlgoStep s; s.arr=stk;
        if(!stk.isEmpty()) s.found<<(stk.size()-1);
        s.msg=QString("POP → rimosso %1, nuovo top=%2").arg(top).arg(stk.isEmpty()?-1:stk.last()); st<<s;
    }
    AlgoStep sf; sf.arr=stk; sf.msg=QString("Stack finale: %1 elementi rimasti.").arg(stk.size()); st<<sf;
    return st;
}

/* ── Queue ── */
QVector<AlgoStep> SimulatorePage::genQueue(QVector<int> arr) {
    QVector<AlgoStep> st;
    QVector<int> q;
    int n=qMin((int)arr.size(),8);
    { AlgoStep s; s.arr=arr; s.msg="Queue FIFO: testa a sinistra, coda a destra"; st<<s; }
    for(int i=0;i<n;i++){
        q<<arr[i];
        AlgoStep s; s.arr=q; s.cmp<<(q.size()-1);
        s.msg=QString("ENQUEUE %1 in coda (size=%2, front=%3)").arg(arr[i]).arg(q.size()).arg(q.front()); st<<s;
    }
    for(int i=0;i<n/2;i++){
        int front=q.front(); q.pop_front();
        AlgoStep s; s.arr=q;
        if(!q.isEmpty()) s.found<<0;
        s.msg=QString("DEQUEUE %1 dalla testa, nuovo front=%2").arg(front).arg(q.isEmpty()?-1:q.front()); st<<s;
    }
    AlgoStep sf; sf.arr=q; sf.msg=QString("Queue finale: %1 elementi rimasti.").arg(q.size()); st<<sf;
    return st;
}

/* ── Deque ── */
QVector<AlgoStep> SimulatorePage::genDeque(QVector<int> arr) {
    QVector<AlgoStep> st;
    QVector<int> dq;
    int n=qMin((int)arr.size(),8);
    { AlgoStep s; s.arr=arr; s.msg="Deque: push/pop da entrambe le estremità"; st<<s; }
    for(int i=0;i<n;i++){
        if(i%2==0){ dq.push_back(arr[i]);
            AlgoStep s; s.arr=dq; if(!dq.isEmpty()) s.cmp<<(dq.size()-1);
            s.msg=QString("push_back(%1) → [%2 ... %3]").arg(arr[i]).arg(dq.front()).arg(dq.back()); st<<s; }
        else { dq.push_front(arr[i]);
            AlgoStep s; s.arr=dq; s.cmp<<0;
            s.msg=QString("push_front(%1) → [%2 ... %3]").arg(arr[i]).arg(dq.front()).arg(dq.back()); st<<s; }
    }
    for(int i=0;i<n/2;i++){
        if(i%2==0 && !dq.isEmpty()){ int v=dq.back(); dq.pop_back();
            AlgoStep s; s.arr=dq; s.msg=QString("pop_back → rimosso %1").arg(v); st<<s; }
        else if(!dq.isEmpty()){ int v=dq.front(); dq.pop_front();
            AlgoStep s; s.arr=dq; s.msg=QString("pop_front → rimosso %1").arg(v); st<<s; }
    }
    AlgoStep sf; sf.arr=dq; sf.msg="Deque: operazioni O(1) su entrambi i lati."; st<<sf;
    return st;
}

/* ── MinHeapBuild (Floyd) ── */
QVector<AlgoStep> SimulatorePage::genMinHeapBuild(QVector<int> arr) {
    QVector<AlgoStep> st;
    int n=arr.size();
    { AlgoStep s; s.arr=arr; s.msg="Min-Heap Build (Floyd): heapify dal basso — O(n)"; st<<s; }
    std::function<void(int)> heapify=[&](int i){
        int smallest=i, l=2*i+1, r=2*i+2;
        if(l<n && arr[l]<arr[smallest]) smallest=l;
        if(r<n && arr[r]<arr[smallest]) smallest=r;
        if(smallest!=i){
            AlgoStep s; s.arr=arr; s.cmp<<i<<smallest;
            s.msg=QString("Heapify: arr[%1]=%2 > arr[%3]=%4 → scambio").arg(i).arg(arr[i]).arg(smallest).arg(arr[smallest]); st<<s;
            std::swap(arr[i],arr[smallest]);
            AlgoStep s2; s2.arr=arr; s2.swp<<i<<smallest; s2.msg="Scambio eseguito"; st<<s2;
            heapify(smallest);
        } else {
            AlgoStep s; s.arr=arr; s.found<<i;
            s.msg=QString("Heapify nodo %1=%2: soddisfatto (min-heap property OK)").arg(i).arg(arr[i]); st<<s;
        }
    };
    for(int i=n/2-1;i>=0;i--) heapify(i);
    QVector<int> all; for(int i=0;i<n;i++) all<<i;
    AlgoStep sf; sf.arr=arr; sf.sorted=all;
    sf.msg=QString("Min-Heap costruito! Radice (min) = %1").arg(arr[0]); st<<sf;
    return st;
}

/* ── HashTable ── */
QVector<AlgoStep> SimulatorePage::genHashTable(QVector<int> arr) {
    QVector<AlgoStep> st;
    int n=arr.size();
    int SZ=n+3;
    QVector<int> table(SZ, -1);
    { AlgoStep s; s.arr=table; s.msg=QString("Hash Table size=%1: inserimento con h(k)=k%%SIZE, chaining").arg(SZ); st<<s; }
    QVector<QVector<int>> chains(SZ);
    for(int i=0;i<n;i++){
        int h=arr[i]%SZ;
        { AlgoStep s; s.arr=table; s.cmp<<h;
          s.msg=QString("Insert %1: h(%1)=%1%%(%2)=%3").arg(arr[i]).arg(SZ).arg(h); st<<s; }
        chains[h]<<arr[i];
        table[h]=chains[h].size();
        AlgoStep s2; s2.arr=table; s2.swp<<h;
        s2.msg=QString("Bucket[%1] ora ha %2 elemento/i (chaining)").arg(h).arg(chains[h].size()); st<<s2;
    }
    /* ricerca */
    int key=arr[rand()%n], hk=key%SZ;
    AlgoStep sf; sf.arr=table; sf.found<<hk;
    sf.msg=QString("Ricerca %1: h=%2, trovato in bucket[%3] — O(1) avg").arg(key).arg(hk).arg(hk); st<<sf;
    return st;
}

/* ── SegmentTree ── */
QVector<AlgoStep> SimulatorePage::genSegmentTree(QVector<int> arr) {
    QVector<AlgoStep> st;
    int n=arr.size();
    QVector<int> seg(4*n, 0);
    std::function<void(int,int,int)> build=[&](int node,int l,int r){
        if(l==r){ seg[node]=arr[l];
            AlgoStep s; s.arr=seg; s.cmp<<node;
            s.msg=QString("Foglia seg[%1] = arr[%2] = %3").arg(node).arg(l).arg(arr[l]); st<<s;
            return; }
        int mid=(l+r)/2;
        build(2*node,l,mid);
        build(2*node+1,mid+1,r);
        seg[node]=seg[2*node]+seg[2*node+1];
        AlgoStep s; s.arr=seg; s.swp<<node<<2*node<<2*node+1;
        s.msg=QString("seg[%1] = seg[%2]+seg[%3] = %4 (range [%5..%6])")
              .arg(node).arg(2*node).arg(2*node+1).arg(seg[node]).arg(l).arg(r); st<<s;
    };
    build(1,0,n-1);
    /* range query */
    int ql=1, qr=n-2;
    AlgoStep sf; sf.arr=seg; sf.found<<1;
    sf.msg=QString("Segment Tree costruito! Query range [%1..%2] in O(log n)").arg(ql).arg(qr); st<<sf;
    return st;
}

/* ── FenwickTree ── */
QVector<AlgoStep> SimulatorePage::genFenwickTree(QVector<int> arr) {
    QVector<AlgoStep> st;
    int n=arr.size();
    QVector<int> bit(n+1, 0);
    { AlgoStep s; s.arr=bit; s.msg="Fenwick Tree (BIT): build tramite n update — O(n log n)"; st<<s; }
    for(int i=0;i<n;i++){
        int v=arr[i], pos=i+1;
        { AlgoStep s; s.arr=bit; s.cmp<<(pos-1);
          s.msg=QString("Update pos=%1, val=%2").arg(pos).arg(v); st<<s; }
        for(int j=pos;j<=n;j+=j&(-j)){
            bit[j]+=v;
            AlgoStep s; s.arr=bit; s.swp<<(j-1);
            s.msg=QString("bit[%1] += %2 → %3  (lowbit=%4)").arg(j).arg(v).arg(bit[j]).arg(j&(-j)); st<<s;
        }
    }
    /* prefix query */
    int qpos=n/2; int sum=0;
    for(int j=qpos;j>0;j-=j&(-j)) sum+=bit[j];
    AlgoStep sf; sf.arr=bit; sf.found<<(qpos-1);
    sf.msg=QString("Prefix sum [1..%1] = %2  (in O(log n))").arg(qpos).arg(sum); st<<sf;
    return st;
}

/* ── LRUCache ── */
QVector<AlgoStep> SimulatorePage::genLRUCache(QVector<int> arr) {
    QVector<AlgoStep> st;
    int cap=4, n=qMin((int)arr.size()+2, 10);
    QVector<int> cache; /* sinistra=LRU, destra=MRU */
    { AlgoStep s; s.arr=arr; s.msg=QString("LRU Cache capacity=%1: sinistra=LRU, destra=MRU").arg(cap); st<<s; }
    QVector<int> accesses;
    for(int i=0;i<n;i++) accesses<<arr[i%arr.size()];
    accesses[n/2]=accesses[0]; /* forza un hit */
    for(int i=0;i<n;i++){
        int key=accesses[i]%10+1;
        int pos=cache.indexOf(key);
        if(pos>=0){ /* hit */
            cache.remove(pos); cache.append(key);
            AlgoStep s; s.arr=cache; s.found<<(cache.size()-1);
            s.msg=QString("HIT key=%1 → sposta in MRU (destra)").arg(key); st<<s;
        } else { /* miss */
            if(cache.size()>=cap){
                int evicted=cache.front(); cache.pop_front();
                AlgoStep s; s.arr=cache; s.swp<<0;
                s.msg=QString("MISS key=%1: evict LRU=%2").arg(key).arg(evicted); st<<s;
            }
            cache.append(key);
            AlgoStep s; s.arr=cache; s.cmp<<(cache.size()-1);
            s.msg=QString("MISS key=%1: inserito (cache=%2/%3)").arg(key).arg(cache.size()).arg(cap); st<<s;
        }
    }
    QVector<int> all; for(int i=0;i<cache.size();i++) all<<i;
    AlgoStep sf; sf.arr=cache; sf.sorted=all; sf.msg="LRU Cache finale (O(1) get/put con HashMap+DList)"; st<<sf;
    return st;
}

/* ── FloydWarshall ── */
QVector<AlgoStep> SimulatorePage::genFloydWarshall() {
    QVector<AlgoStep> st;
    const int V=5;
    const int INF=999;
    int g[V][V]={
        {0,  3,INF,  7,INF},
        {8,  0,  2,INF,INF},
        {5,INF,  0,  1,INF},
        {2,INF,INF,  0,  3},
        {INF,INF,INF, 6,  0}
    };
    QVector<int> dist(V*V);
    for(int i=0;i<V;i++) for(int j=0;j<V;j++) dist[i*V+j]=g[i][j];
    { AlgoStep s; s.arr=dist; s.msg=QString("Floyd-Warshall: matrice distanze %1x%1 (INF=%2)").arg(V).arg(INF); st<<s; }
    for(int k=0;k<V;k++){
        for(int i=0;i<V;i++) for(int j=0;j<V;j++){
            if(dist[i*V+k]<INF && dist[k*V+j]<INF){
                int newD=dist[i*V+k]+dist[k*V+j];
                if(newD<dist[i*V+j]){
                    AlgoStep s; s.arr=dist; s.cmp<<i*V+j<<i*V+k<<k*V+j;
                    s.msg=QString("k=%1: dist[%2][%3]=%4 → via %5 = %6 (miglioramento!)")
                          .arg(k).arg(i).arg(j).arg(dist[i*V+j]).arg(k).arg(newD); st<<s;
                    dist[i*V+j]=newD;
                    AlgoStep s2; s2.arr=dist; s2.swp<<i*V+j; s2.msg="Distanza aggiornata"; st<<s2;
                }
            }
        }
        AlgoStep s; s.arr=dist;
        s.msg=QString("Iterazione k=%1 completata: nodo %1 come intermedio").arg(k); st<<s;
    }
    AlgoStep sf; sf.arr=dist; sf.msg="Floyd-Warshall: tutti i cammini minimi calcolati! O(V³)"; st<<sf;
    return st;
}

/* ── Kruskal MST ── */
QVector<AlgoStep> SimulatorePage::genKruskal() {
    QVector<AlgoStep> st;
    /* grafi come archi pesati: {u,v,w} */
    struct Edge { int u,v,w; };
    QVector<Edge> edges={{0,1,4},{0,2,3},{1,2,1},{1,3,2},{2,3,4},{3,4,2},{2,4,5}};
    int V=5;
    QVector<int> parent(V); for(int i=0;i<V;i++) parent[i]=i;
    std::function<int(int)> find=[&](int x)->int{
        return parent[x]==x?x:parent[x]=find(parent[x]);
    };
    std::sort(edges.begin(),edges.end(),[](const Edge& a,const Edge& b){return a.w<b.w;});
    QVector<int> display(edges.size()); for(int i=0;i<edges.size();i++) display[i]=edges[i].w;
    { AlgoStep s; s.arr=display; s.msg="Kruskal: archi ordinati per peso. MST crescerà aggiungendo il minimo senza cicli."; st<<s; }
    int mstW=0; int added=0;
    QVector<int> inMST;
    for(int i=0;i<(int)edges.size();i++){
        int pu=find(edges[i].u), pv=find(edges[i].v);
        if(pu!=pv){ parent[pu]=pv; mstW+=edges[i].w; inMST<<i; added++;
            AlgoStep s; s.arr=display; s.found=inMST; s.cmp<<i;
            s.msg=QString("Aggiungo arco (%1-%2) w=%3 al MST (ciclo: NO) — totale=%4")
                  .arg(edges[i].u).arg(edges[i].v).arg(edges[i].w).arg(mstW); st<<s;
            if(added==V-1) break;
        } else {
            AlgoStep s; s.arr=display; s.swp<<i;
            s.msg=QString("Salto arco (%1-%2) w=%3: formerebbe un ciclo")
                  .arg(edges[i].u).arg(edges[i].v).arg(edges[i].w); st<<s;
        }
    }
    AlgoStep sf; sf.arr=display; sf.sorted=inMST;
    sf.msg=QString("Kruskal MST completato! Peso totale=%1 (%2 archi)").arg(mstW).arg(added); st<<sf;
    return st;
}

/* ── Prim MST ── */
QVector<AlgoStep> SimulatorePage::genPrim() {
    QVector<AlgoStep> st;
    const int V=6;
    int adj[V][V]={
        {0,2,0,6,0,0},{2,0,3,8,5,0},{0,3,0,0,7,0},
        {6,8,0,0,9,0},{0,5,7,9,0,1},{0,0,0,0,1,0}
    };
    QVector<int> key(V,999), parent(V,-1); QVector<bool> inMST(V,false);
    key[0]=0;
    QVector<int> display(V,0);
    { AlgoStep s; s.arr=display; s.msg=QString("Prim MST: partenza dal nodo 0. Cresce aggiungendo l'arco minimo."); st<<s; }
    for(int cnt=0;cnt<V;cnt++){
        int u=-1;
        for(int v=0;v<V;v++) if(!inMST[v]&&(u==-1||key[v]<key[u])) u=v;
        inMST[u]=true; display[u]=key[u];
        QVector<int> srt; for(int i=0;i<V;i++) if(inMST[i]) srt<<i;
        AlgoStep s; s.arr=display; s.sorted=srt; s.cmp<<u;
        s.msg=QString("Aggiungo nodo %1 (peso arco=%2, padre=%3) al MST")
              .arg(u).arg(key[u]).arg(parent[u]); st<<s;
        for(int v=0;v<V;v++) if(adj[u][v]&&!inMST[v]&&adj[u][v]<key[v]){
            key[v]=adj[u][v]; parent[v]=u;
            AlgoStep s2; s2.arr=display; s2.swp<<v;
            s2.msg=QString("Aggiorna key[%1]=%2 via nodo %3").arg(v).arg(key[v]).arg(u); st<<s2;
        }
    }
    int tot=0; for(int i=1;i<V;i++) tot+=key[i];
    QVector<int> all; for(int i=0;i<V;i++) all<<i;
    AlgoStep sf; sf.arr=display; sf.sorted=all;
    sf.msg=QString("Prim MST completato! Peso totale=%1").arg(tot); st<<sf;
    return st;
}

/* ── TarjanSCC ── */
QVector<AlgoStep> SimulatorePage::genTarjanSCC() {
    QVector<AlgoStep> st;
    const int V=7;
    QVector<QVector<int>> adj={{1},{2},{0,3},{4},{3,5},{6},{4}};
    QVector<int> disc(V,-1), low(V,-1), stkArr(V,0);
    QVector<bool> onStk(V,false);
    int timer=0;
    QVector<QVector<int>> sccs;
    QVector<int> stkV;
    std::function<void(int)> dfs=[&](int u){
        disc[u]=low[u]=timer++;
        stkV<<u; onStk[u]=true;
        QVector<int> disp(V,0); for(int i=0;i<V;i++) disp[i]=disc[i]<0?0:disc[i];
        AlgoStep s; s.arr=disp; s.cmp<<u;
        s.msg=QString("DFS nodo %1: disc=%2, low=%3").arg(u).arg(disc[u]).arg(low[u]); st<<s;
        for(int v : adj[u]){
            if(disc[v]<0){ dfs(v); low[u]=qMin(low[u],low[v]); }
            else if(onStk[v]) low[u]=qMin(low[u],disc[v]);
        }
        if(low[u]==disc[u]){
            QVector<int> scc;
            while(true){ int w=stkV.last(); stkV.pop_back(); onStk[w]=false; scc<<w; if(w==u) break; }
            sccs<<scc;
            QVector<int> disp2(V,0); for(auto& sc:sccs) for(int x:sc) disp2[x]=(int)sccs.size();
            AlgoStep s2; s2.arr=disp2; s2.found=scc;
            s2.msg=QString("SCC trovata: {%1}").arg([&](){QString r; for(int x:scc) r+=QString::number(x)+","; r.chop(1); return r;}()); st<<s2;
        }
    };
    for(int i=0;i<V;i++) if(disc[i]<0) dfs(i);
    QVector<int> final(V,0); for(int s=0;s<(int)sccs.size();s++) for(int x:sccs[s]) final[x]=s+1;
    QVector<int> all; for(int i=0;i<V;i++) all<<i;
    AlgoStep sf; sf.arr=final; sf.sorted=all;
    sf.msg=QString("Tarjan SCC: trovate %1 componenti fortemente connesse!").arg(sccs.size()); st<<sf;
    return st;
}

/* ── A* Search ── */
QVector<AlgoStep> SimulatorePage::genAStar() {
    QVector<AlgoStep> st;
    /* griglia 4x4, 0=libero 1=muro */
    const int W=5, H=4;
    int grid[H][W]={{0,0,1,0,0},{0,0,1,0,0},{0,0,0,0,0},{0,1,1,0,0}};
    int sx=0,sy=0, gx=4,gy=3;
    auto h=[&](int x,int y){ return qAbs(x-gx)+qAbs(y-gy); };
    QVector<int> display(W*H,0);
    display[sy*W+sx]=1; display[gy*W+gx]=2;
    { AlgoStep s; s.arr=display; s.cmp<<sy*W+sx; s.found<<gy*W+gx;
      s.msg=QString("A*: start=(%1,%2) goal=(%3,%4). Euristica=Manhattan.").arg(sx).arg(sy).arg(gx).arg(gy); st<<s; }
    struct Node{ int x,y,g,f; };
    QVector<Node> open={{sx,sy,0,h(sx,sy)}};
    QVector<int> visited(W*H,0);
    int itr=0;
    while(!open.isEmpty() && itr<20){
        auto it=std::min_element(open.begin(),open.end(),[](const Node&a,const Node&b){return a.f<b.f;});
        Node cur=*it; open.erase(it);
        if(cur.x==gx&&cur.y==gy){ break; }
        visited[cur.y*W+cur.x]=1;
        display[cur.y*W+cur.x]=3;
        QVector<int> cmp; cmp<<cur.y*W+cur.x;
        AlgoStep s; s.arr=display; s.cmp=cmp;
        s.msg=QString("Espando (%1,%2): g=%3, h=%4, f=%5").arg(cur.x).arg(cur.y).arg(cur.g).arg(h(cur.x,cur.y)).arg(cur.f); st<<s;
        int dx[]={1,-1,0,0}, dy[]={0,0,1,-1};
        for(int d=0;d<4;d++){
            int nx=cur.x+dx[d], ny=cur.y+dy[d];
            if(nx<0||nx>=W||ny<0||ny>=H||grid[ny][nx]||visited[ny*W+nx]) continue;
            int ng=cur.g+1, nf=ng+h(nx,ny);
            open.push_back({nx,ny,ng,nf});
            display[ny*W+nx]=4;
            AlgoStep s2; s2.arr=display; s2.swp<<ny*W+nx;
            s2.msg=QString("Aggiungo (%1,%2) all'open list: g=%3 h=%4 f=%5").arg(nx).arg(ny).arg(ng).arg(h(nx,ny)).arg(nf); st<<s2;
        }
        itr++;
    }
    display[gy*W+gx]=5;
    QVector<int> all; for(int i=0;i<W*H;i++) all<<i;
    AlgoStep sf; sf.arr=display; sf.found<<gy*W+gx;
    sf.msg=QString("A* completato! Percorso trovato da (%1,%2) a (%3,%4)").arg(sx).arg(sy).arg(gx).arg(gy); st<<sf;
    return st;
}

/* ── MatrixChain ── */
