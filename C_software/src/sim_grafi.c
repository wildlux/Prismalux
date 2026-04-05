/*
 * sim_grafi.c — Algoritmi su Grafi (7)
 * ======================================
 * BFS griglia, Dijkstra, DFS griglia, Topological Sort,
 * Bellman-Ford, Floyd-Warshall, Kruskal MST
 */
#include "sim_common.h"

/* ── BFS su griglia ──────────────────────────────────────────────────── */
int sim_bfs_griglia(void) {
    char grid[6][11] = {
        "..#.......",
        ".#.#.####.",
        "...#....#.",
        "#####.##..",
        "......#...",
        "......#...",
    };
    int rows=6, cols=10;
    int prev[6][10][2];
    memset(prev,-1,sizeof prev);
    int visited[6][10] = {{0}};
    int qr[100],qc[100],qh=0,qt=0;
    qr[qt]=0;qc[qt]=0;qt++;
    visited[0][0]=1;
    int dr[]={-1,1,0,0}, dc[]={0,0,-1,1};
    char msg[128];
    sim_log_reset();

    #define STAMPA_GRIGLIA(titolo) do { \
        clear_screen(); print_header(); \
        printf(CYN BLD "\n  %s\n\n" RST, (titolo)); \
        for(int _r=0;_r<rows;_r++){ \
            printf("  "); \
            for(int _c=0;_c<cols;_c++){ \
                if(_r==0&&_c==0) printf(GRN "S" RST); \
                else if(_r==5&&_c==9) printf(MAG "E" RST); \
                else if(grid[_r][_c]=='#') printf(GRY "#" RST); \
                else if(visited[_r][_c]==2) printf(CYN "\xe2\x96\x92" RST); \
                else if(visited[_r][_c]) printf(YLW "\xe2\x96\x91" RST); \
                else printf(" "); \
            } \
            printf("\n"); \
        } \
        printf(GRY "\n  S=partenza E=arrivo #=muro " YLW "\xe2\x96\x91" RST GRY "=visitato " CYN "\xe2\x96\x92" RST GRY "=percorso\n" RST); \
    } while(0)

    STAMPA_GRIGLIA("\xf0\x9f\x8c\x90 BFS — Ricerca su Griglia (S\xe2\x86\x92E)");
    printf(GRY "\n  [INVIO per iniziare] " RST); fflush(stdout);
    if (sim_aspetta_passo()) { return 0; }

    int bfs_ok = 1;
    while (qh < qt) {
        int r=qr[qh], c=qc[qh]; qh++;
        if (r==5 && c==9) break;
        for (int d=0;d<4;d++) {
            int nr=r+dr[d], nc=c+dc[d];
            if (nr<0||nr>=rows||nc<0||nc>=cols) continue;
            if (visited[nr][nc]||grid[nr][nc]=='#') continue;
            visited[nr][nc]=1;
            prev[nr][nc][0]=r; prev[nr][nc][1]=c;
            if (qt < 100) { qr[qt]=nr; qc[qt]=nc; qt++; }
        }
        snprintf(msg, sizeof msg, "BFS: visito (%d,%d)  coda=%d", r, c, qt-qh);
        sim_log_push(msg);
        STAMPA_GRIGLIA("\xf0\x9f\x8c\x90 BFS — esplorazione");
        for(int i=0;i<_sim_log_n;i++) printf(i<_sim_log_n-1?GRY "  %s\n" RST:CYN BLD "  %s\n" RST, _sim_log[i]);
        printf(GRY "\n  [INVIO=passo  ESC=esci] " RST); fflush(stdout);
        if (sim_aspetta_passo()) { bfs_ok=0; break; }
    }
    if (!bfs_ok) { return 0; }
    {
        int rr=5, cc=9;
        while (!(rr==0&&cc==0)&&prev[rr][cc][0]>=0) {
            visited[rr][cc]=2;
            int pr=prev[rr][cc][0],pc=prev[rr][cc][1];
            rr=pr; cc=pc;
        }
    }
    STAMPA_GRIGLIA("\xf0\x9f\x8c\x90 BFS — Percorso trovato!");
    printf(GRY "\n  [INVIO] " RST); fflush(stdout); sim_aspetta_passo();
    #undef STAMPA_GRIGLIA
    return 1;
}

/* ── Dijkstra su grafo con 6 nodi ───────────────────────────────────── */
int sim_dijkstra(void) {
    int W[6][6] = {
        {0,7,9,0,0,14},
        {7,0,10,15,0,0},
        {9,10,0,11,0,2},
        {0,15,11,0,6,0},
        {0,0,0,6,0,9},
        {14,0,2,0,9,0},
    };
    int dist[6]; int vis[6]={0}; int N=6;
    for(int i=0;i<N;i++) dist[i]=9999;
    dist[0]=0;
    char nomi[] = "ABCDEF";
    sim_log_reset();
    char msg[128];
    for (int step=0;step<N;step++) {
        int u=-1;
        for(int i=0;i<N;i++) if(!vis[i]&&(u<0||dist[i]<dist[u])) u=i;
        if(u<0||dist[u]==9999) break;
        vis[u]=1;
        snprintf(msg, sizeof msg, "Visito %c (dist=%d)", nomi[u], dist[u]);
        sim_log_push(msg);
        for(int v=0;v<N;v++) {
            if(W[u][v]&&!vis[v]&&dist[u]+W[u][v]<dist[v]) {
                dist[v]=dist[u]+W[u][v];
                snprintf(msg, sizeof msg, "  Aggiorno dist[%c]=%d (via %c)", nomi[v],dist[v],nomi[u]);
                sim_log_push(msg);
            }
        }
        clear_screen(); print_header();
        printf(CYN BLD "\n  \xf0\x9f\x9b\xa4\xef\xb8\x8f  Dijkstra — sorgente: A\n\n" RST);
        printf(GRY "  Grafo: A-B(7) A-C(9) A-F(14) B-C(10) B-D(15) C-D(11) C-F(2) D-E(6) E-F(9)\n\n" RST);
        printf("  ");
        for(int i=0;i<N;i++) printf("  %c   ", nomi[i]);
        printf("\n  ");
        for(int i=0;i<N;i++) {
            if(dist[i]==9999) printf(GRY "  \xe2\x88\x9e   " RST);
            else if(vis[i]) printf(GRN "%-6d" RST, dist[i]);
            else printf(YLW "%-6d" RST, dist[i]);
        }
        printf("\n\n");
        for(int i=0;i<_sim_log_n;i++) printf(i<_sim_log_n-1?GRY "  %s\n" RST:CYN BLD "  %s\n" RST, _sim_log[i]);
        printf(GRY "\n  [INVIO=passo  ESC=esci] " RST); fflush(stdout);
        if (sim_aspetta_passo()) return 0;
    }
    printf(GRN BLD "\n  Distanze da A: " RST);
    for(int i=0;i<N;i++) printf("%c=%d  ", nomi[i], dist[i]);
    printf("\n\n" GRY "  [INVIO] " RST); fflush(stdout); sim_aspetta_passo(); return 1;
}

/* ── DFS su Griglia ──────────────────────────────────────────────────── */
int sim_dfs_griglia(void) {
    int R = 5, C = 7;
    int vis[5][7] = {0};
    int blocked[5][7] = {
        {0,0,0,1,0,0,0},
        {0,1,0,1,0,1,0},
        {0,1,0,0,0,1,0},
        {0,0,0,1,0,0,0},
        {0,1,0,0,0,1,0},
    };
    int stack_r[35], stack_c[35], stk = 0;
    int sr = 0, sc = 0, er = 4, ec = 6;
    stack_r[stk] = sr; stack_c[stk] = sc; stk++;
    sim_log_reset();
    char msg[128];
    int step = 0;
    int dr[] = {0,1,0,-1}, dc[] = {1,0,-1,0};

    while (stk > 0) {
        int r = stack_r[--stk], c = stack_c[stk];
        if (vis[r][c]) continue;
        vis[r][c] = 1; step++;
        snprintf(msg, sizeof msg, "Passo %d: visito (%d,%d)", step, r, c);
        sim_log_push(msg);

        clear_screen(); print_header();
        printf(CYN BLD "\n  \xf0\x9f\x94\x8d  DFS su Griglia %dx%d\n" RST, R, C);
        printf(GRY "  S=start  E=end  #=muro  V=visitato  .=non visitato\n\n" RST);
        for (int ri = 0; ri < R; ri++) {
            printf("  ");
            for (int ci = 0; ci < C; ci++) {
                if (ri == sr && ci == sc) printf(GRN " S" RST);
                else if (ri == er && ci == ec) printf(YLW " E" RST);
                else if (blocked[ri][ci]) printf(GRY " #" RST);
                else if (ri == r && ci == c) printf(CYN " @" RST);
                else if (vis[ri][ci]) printf(MAG " V" RST);
                else printf(" .");
            }
            printf("\n");
        }
        printf("\n");
        for (int l = 0; l < _sim_log_n; l++) printf(GRY "  %s\n" RST, _sim_log[l]);
        printf("\n" GRY "  [INVIO=passo  ESC=esci] " RST); fflush(stdout);
        if (sim_aspetta_passo()) return 0;

        if (r == er && c == ec) {
            printf(GRN BLD "\n  \xe2\x9c\x94  Destinazione raggiunta!\n" RST);
            printf(GRY "  [INVIO] " RST); fflush(stdout); sim_aspetta_passo(); return 1;
        }
        for (int d = 0; d < 4; d++) {
            int nr = r + dr[d], nc = c + dc[d];
            if (nr >= 0 && nr < R && nc >= 0 && nc < C && !vis[nr][nc] && !blocked[nr][nc]) {
                stack_r[stk] = nr; stack_c[stk] = nc; stk++;
            }
        }
    }
    printf(GRY "  DFS completata. Nodi visitati: %d\n" RST, step);
    printf(GRY "  [INVIO] " RST); fflush(stdout); sim_aspetta_passo(); return 1;
}

/* ── Topological Sort (Kahn BFS, DAG 6 nodi) ────────────────────────── */
int sim_topo_sort(void) {
    int indeg[6] = {0};
    int adj[6][6] = {
        {0,0,1,1,0,0},
        {0,0,0,1,1,0},
        {0,0,0,0,0,1},
        {0,0,0,0,0,1},
        {0,0,0,0,0,1},
        {0,0,0,0,0,0},
    };
    for (int u = 0; u < 6; u++) for (int v = 0; v < 6; v++) if (adj[u][v]) indeg[v]++;

    sim_log_reset();
    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x93\x8b  Topological Sort — DAG 6 nodi (Kahn BFS)\n" RST);
    printf(GRY "  Archi: 0" "\xe2\x86\x92" "2, 0" "\xe2\x86\x92" "3, 1" "\xe2\x86\x92" "3, 1" "\xe2\x86\x92" "4, 2" "\xe2\x86\x92" "5, 3" "\xe2\x86\x92" "5, 4" "\xe2\x86\x92" "5\n\n" RST);
    printf(GRY "  [INVIO per iniziare] " RST); fflush(stdout);
    if (sim_aspetta_passo()) return 0;

    int queue[6], qf = 0, qb = 0;
    for (int i = 0; i < 6; i++) if (indeg[i] == 0) queue[qb++] = i;
    int order[6], ord_n = 0;
    char msg[128];

    while (qf < qb) {
        int u = queue[qf++];
        order[ord_n++] = u;
        snprintf(msg, sizeof msg, "Processo nodo %d (indeg=0), tolgo dai successori", u);
        sim_log_push(msg);

        clear_screen(); print_header();
        printf(CYN BLD "\n  \xf0\x9f\x93\x8b  Topological Sort\n\n" RST);
        printf("  Ordine finora: ");
        for (int i = 0; i < ord_n; i++) printf(GRN "%d " RST, order[i]);
        printf("\n  Coda: ");
        for (int i = qf; i < qb; i++) printf(YLW "%d " RST, queue[i]);
        printf("\n  Indegree: ");
        for (int i = 0; i < 6; i++) printf("[%d:%d] ", i, indeg[i]);
        printf("\n\n");
        for (int l = 0; l < _sim_log_n; l++) printf(GRY "  %s\n" RST, _sim_log[l]);
        printf("\n" GRY "  [INVIO=passo  ESC=esci] " RST); fflush(stdout);
        if (sim_aspetta_passo()) return 0;

        for (int v = 0; v < 6; v++) {
            if (adj[u][v]) { indeg[v]--; if (indeg[v] == 0) queue[qb++] = v; }
        }
    }
    clear_screen(); print_header();
    printf(GRN BLD "\n  \xe2\x9c\x94  Ordine topologico: ");
    for (int i = 0; i < ord_n; i++) printf("%d ", order[i]);
    printf("\n\n" RST);
    printf(GRY "  [INVIO] " RST); fflush(stdout); sim_aspetta_passo(); return 1;
}

/* ── Bellman-Ford (5 nodi) ───────────────────────────────────────────── */
int sim_bellman_ford(void) {
    struct { int u, v, w; } edges[] = {
        {0,1,6},{0,2,7},{1,2,8},{1,3,-4},{1,4,5},{2,4,-3},{3,0,2},{4,3,7}
    };
    int ne = 8, nv = 5, src = 0;
    int dist[5];
    for (int i = 0; i < nv; i++) dist[i] = 99999;
    dist[src] = 0;
    sim_log_reset();
    char msg[128];

    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x9b\xa4  Bellman-Ford — 5 nodi, 8 archi (con pesi negativi)\n" RST);
    printf(GRY "  Ripassa tutti gli archi V-1 = 4 volte\n\n" RST);
    printf(GRY "  [INVIO per iniziare] " RST); fflush(stdout);
    if (sim_aspetta_passo()) return 0;

    for (int iter = 0; iter < nv-1; iter++) {
        int updated = 0;
        for (int e = 0; e < ne; e++) {
            int u = edges[e].u, v = edges[e].v, w = edges[e].w;
            if (dist[u] != 99999 && dist[u] + w < dist[v]) {
                dist[v] = dist[u] + w;
                updated = 1;
                snprintf(msg, sizeof msg, "Iter %d: dist[%d]=%d via %d (arco %d\xe2\x86\x92%d w=%d)", iter+1, v, dist[v], u, u, v, w);
                sim_log_push(msg);
            }
        }
        clear_screen(); print_header();
        printf(CYN BLD "\n  \xf0\x9f\x9b\xa4  Bellman-Ford — Iterazione %d\n\n" RST, iter+1);
        printf("  Distanze: ");
        for (int i = 0; i < nv; i++) {
            if (dist[i] >= 99999) printf("[%d:\xe2\x88\x9e] ", i);
            else printf(GRN "[%d:%d] " RST, i, dist[i]);
        }
        printf("\n\n");
        for (int l = 0; l < _sim_log_n; l++) printf(GRY "  %s\n" RST, _sim_log[l]);
        printf("\n" GRY "  [INVIO=passo  ESC=esci] " RST); fflush(stdout);
        if (sim_aspetta_passo()) return 0;
        if (!updated) break;
    }
    clear_screen(); print_header();
    printf(GRN BLD "\n  \xe2\x9c\x94  Distanze minime da nodo 0:\n\n" RST);
    for (int i = 0; i < nv; i++) printf("  dist[%d] = %d\n", i, dist[i]);
    printf("\n" GRY "  [INVIO] " RST); fflush(stdout); sim_aspetta_passo(); return 1;
}

/* ── Floyd-Warshall (4x4) ────────────────────────────────────────────── */
int sim_floyd_warshall(void) {
    int INF = 9999;
    int dist[4][4] = {
        {0,   3,   9999, 7  },
        {8,   0,   2,   9999},
        {5,   9999, 0,   1  },
        {2,   9999, 9999, 0  },
    };
    sim_log_reset();
    char msg[128];

    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x97\xba  Floyd-Warshall — 4 nodi, tutte le coppie\n" RST);
    printf(GRY "  Per ogni k: dist[i][j] = min(dist[i][j], dist[i][k]+dist[k][j])\n\n" RST);
    printf(GRY "  [INVIO per iniziare] " RST); fflush(stdout);
    if (sim_aspetta_passo()) return 0;

    for (int k = 0; k < 4; k++) {
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                if (dist[i][k] != INF && dist[k][j] != INF &&
                    dist[i][k] + dist[k][j] < dist[i][j]) {
                    dist[i][j] = dist[i][k] + dist[k][j];
                }
            }
        }
        snprintf(msg, sizeof msg, "k=%d: aggiornate distanze via nodo %d", k, k);
        sim_log_push(msg);
        clear_screen(); print_header();
        printf(CYN BLD "\n  \xf0\x9f\x97\xba  Floyd-Warshall — k=%d (via nodo %d)\n\n" RST, k, k);
        printf("     ");
        for (int j = 0; j < 4; j++) printf("  [%d]", j);
        printf("\n");
        for (int i = 0; i < 4; i++) {
            printf("  [%d] ", i);
            for (int j = 0; j < 4; j++) {
                if (dist[i][j] == INF) printf(GRY "   \xe2\x88\x9e " RST);
                else printf(GRN "  %3d" RST, dist[i][j]);
            }
            printf("\n");
        }
        printf("\n");
        for (int l = 0; l < _sim_log_n; l++) printf(GRY "  %s\n" RST, _sim_log[l]);
        printf("\n" GRY "  [INVIO=passo  ESC=esci] " RST); fflush(stdout);
        if (sim_aspetta_passo()) return 0;
    }
    printf(GRN BLD "\n  \xe2\x9c\x94  Matrice distanze minime completata!\n\n" RST);
    printf(GRY "  [INVIO] " RST); fflush(stdout); sim_aspetta_passo(); return 1;
}

/* ── Kruskal MST (6 nodi) ────────────────────────────────────────────── */
static int _kruskal_find(int parent[], int i) {
    if (parent[i] != i) parent[i] = _kruskal_find(parent, parent[i]);
    return parent[i];
}
static void _kruskal_union(int parent[], int rank[], int x, int y) {
    int rx = _kruskal_find(parent, x), ry = _kruskal_find(parent, y);
    if (rank[rx] < rank[ry]) parent[rx] = ry;
    else if (rank[rx] > rank[ry]) parent[ry] = rx;
    else { parent[ry] = rx; rank[rx]++; }
}

typedef struct { int u, v, w; } KruskalEdge;

int sim_kruskal(void) {
    KruskalEdge edges[] = {
        {0,1,4},{0,2,3},{1,2,1},{1,3,2},{2,3,4},{3,4,2},{4,5,6},{3,5,5}
    };
    int ne = 8, nv = 6;
    for (int i = 1; i < ne; i++) {
        int j = i;
        while (j > 0 && edges[j].w < edges[j-1].w) {
            KruskalEdge tmp = edges[j]; edges[j] = edges[j-1]; edges[j-1] = tmp;
            j--;
        }
    }
    int parent[6], rank2[6];
    for (int i = 0; i < nv; i++) { parent[i] = i; rank2[i] = 0; }
    sim_log_reset();
    char msg[128];

    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x8c\xb3  Kruskal MST — 6 nodi, 8 archi\n" RST);
    printf(GRY "  Ordina archi, aggiungi se non crea ciclo (Union-Find)\n\n" RST);
    printf(GRY "  [INVIO per iniziare] " RST); fflush(stdout);
    if (sim_aspetta_passo()) return 0;

    int mst_w = 0, mst_e = 0;
    for (int e = 0; e < ne && mst_e < nv-1; e++) {
        int u = edges[e].u, v = edges[e].v, w = edges[e].w;
        int ru = _kruskal_find(parent, u), rv = _kruskal_find(parent, v);
        if (ru != rv) {
            _kruskal_union(parent, rank2, u, v);
            mst_w += w; mst_e++;
            snprintf(msg, sizeof msg, "Aggiungo arco %d-%d (peso %d) al MST. Peso totale: %d", u, v, w, mst_w);
            sim_log_push(msg);
        } else {
            snprintf(msg, sizeof msg, "Salto arco %d-%d (creerebbe ciclo)", u, v);
            sim_log_push(msg);
        }
        clear_screen(); print_header();
        printf(CYN BLD "\n  \xf0\x9f\x8c\xb3  Kruskal MST — arco %d/%d\n\n" RST, e+1, ne);
        for (int l = 0; l < _sim_log_n; l++) {
            if (l == _sim_log_n-1) printf(CYN BLD "  %s\n" RST, _sim_log[l]);
            else printf(GRY "  %s\n" RST, _sim_log[l]);
        }
        printf("\n" GRY "  [INVIO=passo  ESC=esci] " RST); fflush(stdout);
        if (sim_aspetta_passo()) return 0;
    }
    clear_screen(); print_header();
    printf(GRN BLD "\n  \xe2\x9c\x94  MST completato! Peso totale = %d\n\n" RST, mst_w);
    printf(GRY "  [INVIO] " RST); fflush(stdout); sim_aspetta_passo(); return 1;
}
