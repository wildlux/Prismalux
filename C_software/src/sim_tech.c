/*
 * sim_tech.c — Tecniche + Visualizzazioni + Fisica + Chimica (10)
 * =================================================================
 * Two Pointers, Sliding Window, Bit Manipulation,
 * Game of Life, Sierpinski, Rule 30,
 * Caduta Libera, pH, Gas Ideali, Stechiometria
 */
#include "sim_common.h"

/* ── Two Pointers ────────────────────────────────────────────────────── */
int sim_two_pointers(void) {
    int arr[] = {1,3,5,7,9,11,14,17,20,24};
    int n = 10;
    srand((unsigned)time(NULL));
    int target = 20 + rand() % 10;
    sim_log_reset();
    char intro[128];
    snprintf(intro, sizeof intro, "Two Pointers: trova coppia con somma=%d in array ordinato — O(n)", target);
    sim_mostra_array(arr, n,-1,-1,-1,-1,-1,0,intro,"\xf0\x9f\x91\x89 Two Pointers");
    if (sim_aspetta_passo()) return 0;
    int lo=0, hi=n-1;
    char msg[128];
    int trovato=-1;
    while (lo < hi) {
        int sum = arr[lo]+arr[hi];
        snprintf(msg, sizeof msg, "lo=%d(%d) hi=%d(%d) somma=%d %s %d",
                 lo,arr[lo],hi,arr[hi],sum,sum==target?"==":sum<target?"<":">",target);
        sim_log_push(msg);
        sim_mostra_array(arr, n, lo, hi,-1,-1,-1, 0, msg, "\xf0\x9f\x91\x89 Two Pointers");
        if (sim_aspetta_passo()) return 0;
        if (sum==target) { trovato=1; break; }
        else if (sum<target) lo++;
        else hi--;
    }
    if (trovato>0) {
        snprintf(msg, sizeof msg, "\xe2\x9c\x94 Trovato: arr[%d]=%d + arr[%d]=%d = %d", lo,arr[lo],hi,arr[hi],target);
        sim_log_push(msg);
        sim_mostra_array(arr, n,-1,-1,-1,-1,-1,(1ULL<<lo)|(1ULL<<hi),msg,"\xf0\x9f\x91\x89 Two Pointers — TROVATO");
    } else {
        printf(YLW "\n  Nessuna coppia con somma %d\n" RST, target);
    }
    printf(GRY "\n  [INVIO] " RST); fflush(stdout); sim_aspetta_passo(); return 1;
}

/* ── Sliding Window ──────────────────────────────────────────────────── */
int sim_sliding_window(void) {
    int arr[10]; sim_genera_array(arr, 10, 1, 20);
    int n = 10, k = 3;
    sim_log_reset();
    char intro[128];
    snprintf(intro, sizeof intro, "Sliding Window: somma massima di k=%d elementi consecutivi — O(n)", k);
    sim_mostra_array(arr, n,-1,-1,-1,-1,-1,0,intro,"\xf0\x9f\xaa\x9f Sliding Window");
    if (sim_aspetta_passo()) return 0;
    int win=0;
    for(int i=0;i<k;i++) win+=arr[i];
    int max_sum=win;
    char msg[128];
    snprintf(msg, sizeof msg, "Finestra iniziale [0..%d] somma=%d", k-1, win);
    sim_log_push(msg);
    sim_mostra_array(arr, n, 0, k-1,-1,-1,-1, 0, msg, "\xf0\x9f\xaa\x9f Sliding Window");
    if (sim_aspetta_passo()) return 0;
    int best_l=0;
    for(int i=k;i<n;i++) {
        win += arr[i]-arr[i-k];
        if(win>max_sum){max_sum=win;best_l=i-k+1;}
        snprintf(msg, sizeof msg, "Finestra [%d..%d] somma=%d  max=%d", i-k+1,i,win,max_sum);
        sim_log_push(msg);
        sim_mostra_array(arr, n, i-k+1, i,-1,-1,-1, 0, msg, "\xf0\x9f\xaa\x9f Sliding Window");
        if (sim_aspetta_passo()) return 0;
    }
    unsigned long long bitmask=0;
    for(int i=best_l;i<best_l+k;i++) bitmask|=(1ULL<<i);
    snprintf(msg, sizeof msg, "\xe2\x9c\x94 Finestra migliore [%d..%d] somma=%d", best_l, best_l+k-1, max_sum);
    sim_log_push(msg);
    sim_mostra_array(arr, n,-1,-1,-1,-1,-1,bitmask,msg,"\xf0\x9f\xaa\x9f Sliding Window — MASSIMO");
    sim_aspetta_passo(); return 1;
}

/* ── Bit Manipulation ────────────────────────────────────────────────── */
int sim_bit_manipulation(void) {
    srand((unsigned)time(NULL));
    unsigned int A = rand() % 256;
    unsigned int B = rand() % 256;
    sim_log_reset();
    #define STAMPA_BIT(titolo, val, risultato) do { \
        clear_screen(); print_header(); \
        printf(CYN BLD "\n  \xf0\x9f\xa7\xa0  Bit Manipulation\n\n" RST); \
        printf("  A = %3u  = ", A); for(int _i=7;_i>=0;_i--) printf("%d",(A>>_i)&1); printf("\n"); \
        printf("  B = %3u  = ", B); for(int _i=7;_i>=0;_i--) printf("%d",(B>>_i)&1); printf("\n\n"); \
        printf(CYN "  %-20s: %3u = " RST, (titolo), (risultato)); \
        for(int _i=7;_i>=0;_i--) printf("%d",((risultato)>>_i)&1); printf("\n"); \
        for(int _i=0;_i<_sim_log_n;_i++) printf(GRY "  %s\n" RST, _sim_log[_i]); \
        printf(GRY "\n  [INVIO=passo  ESC=esci] " RST); fflush(stdout); \
    } while(0)

    char msg[128];
    snprintf(msg, sizeof msg, "AND: bit a 1 solo se entrambi 1");  sim_log_push(msg);
    STAMPA_BIT("A AND B", val, A & B); if(sim_aspetta_passo()){return 0;}
    snprintf(msg, sizeof msg, "OR: bit a 1 se almeno uno 1");       sim_log_push(msg);
    STAMPA_BIT("A OR B",  val, A | B); if(sim_aspetta_passo()){return 0;}
    snprintf(msg, sizeof msg, "XOR: bit a 1 se diversi");           sim_log_push(msg);
    STAMPA_BIT("A XOR B", val, A ^ B); if(sim_aspetta_passo()){return 0;}
    snprintf(msg, sizeof msg, "NOT A: inverte tutti i bit");         sim_log_push(msg);
    STAMPA_BIT("NOT A",   val, (~A)&0xFF); if(sim_aspetta_passo()){return 0;}
    snprintf(msg, sizeof msg, "A << 1: moltiplica per 2 = %u", (A<<1)&0xFF); sim_log_push(msg);
    STAMPA_BIT("A << 1",  val, (A<<1)&0xFF); if(sim_aspetta_passo()){return 0;}
    snprintf(msg, sizeof msg, "A >> 1: divide per 2 = %u", A>>1);  sim_log_push(msg);
    STAMPA_BIT("A >> 1",  val, A>>1); sim_aspetta_passo();
    #undef STAMPA_BIT
    return 1;
}

/* ── Game of Life (Conway) ───────────────────────────────────────────── */
int sim_game_of_life(void) {
    int R=12, C=22;
    int g[12][22]={0}, ng[12][22]={0};
    int seed[][2] = {
        {1,1},{1,2},{2,1},{2,3},{3,2},
        {5,5},{5,6},{6,5},{6,6},
        {8,10},{8,11},{8,12},
    };
    for(int i=0;i<(int)(sizeof seed/sizeof seed[0]);i++) g[seed[i][0]][seed[i][1]]=1;
    sim_log_reset();
    char msg[128];
    for(int gen=0;gen<20;gen++){
        int vivi=0; for(int r=0;r<R;r++) for(int c=0;c<C;c++) vivi+=g[r][c];
        clear_screen(); print_header();
        printf(CYN BLD "\n  \xf0\x9f\xa7\xac  Game of Life — Generazione %d   Vivi: %d\n\n" RST,gen,vivi);
        for(int r=0;r<R;r++){
            printf("  ");
            for(int c=0;c<C;c++){
                if(g[r][c]) printf(GRN "\xe2\x96\x88\xe2\x96\x88" RST);
                else printf(GRY "\xc2\xb7\xc2\xb7" RST);
            }
            printf("\n");
        }
        snprintf(msg, sizeof msg, "Gen %2d: %d cellule vive", gen, vivi);
        sim_log_push(msg);
        for(int i=0;i<_sim_log_n;i++) printf(GRY "  %s\n" RST, _sim_log[i]);
        printf(GRY "\n  [INVIO=generazione  ESC=esci] " RST); fflush(stdout);
        if(sim_aspetta_passo()) return 0;
        for(int r=0;r<R;r++) for(int c=0;c<C;c++){
            int nb=0;
            for(int dr=-1;dr<=1;dr++) for(int dc=-1;dc<=1;dc++){
                if(!dr&&!dc) continue;
                int nr=r+dr,nc=c+dc;
                if(nr>=0&&nr<R&&nc>=0&&nc<C) nb+=g[nr][nc];
            }
            ng[r][c]=(g[r][c]?(nb==2||nb==3):nb==3)?1:0;
        }
        for(int r=0;r<R;r++) for(int c=0;c<C;c++) g[r][c]=ng[r][c];
    }
    return 1;
}

/* ── Sierpinski (ASCII art) ──────────────────────────────────────────── */
int sim_sierpinski(void) {
    sim_log_reset();
    for(int level=1;level<=5;level++){
        int sz = 1<<level;
        clear_screen(); print_header();
        printf(CYN BLD "\n  \xf0\x9f\x94\xba  Triangolo di Sierpinski — Livello %d\n\n" RST, level);
        for(int r=0;r<sz;r++){
            printf("  ");
            for(int sp=0;sp<(sz-r);sp++) printf(" ");
            for(int c=0;c<=r;c++){
                if((r & c)==0) printf(GRN "\xe2\x96\xb2 " RST);
                else printf("  ");
            }
            printf("\n");
        }
        printf(GRY "\n  [INVIO=prossimo livello  ESC=esci] " RST); fflush(stdout);
        if(sim_aspetta_passo()) return 0;
    }
    return 1;
}

/* ── Rule 30 Wolfram ─────────────────────────────────────────────────── */
int sim_rule30(void) {
    int W=41, gen=20;
    int cur[41]={0}, nxt[41]={0};
    cur[W/2]=1;
    sim_log_reset();
    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x8c\x80  Rule 30 — Automa Cellulare di Wolfram\n" RST);
    printf(GRY "  Regola: 111=0 110=0 101=0 100=1 011=1 010=1 001=1 000=0\n\n" RST);
    printf(GRY "  [INVIO per iniziare] " RST); fflush(stdout);
    if(sim_aspetta_passo()) return 0;
    char buf[42];
    for(int g=0;g<gen;g++){
        buf[W]='\0';
        for(int i=0;i<W;i++) buf[i]=cur[i]?'#':' ';
        clear_screen(); print_header();
        printf(CYN BLD "\n  \xf0\x9f\x8c\x80  Rule 30 — Generazione %d\n\n" RST, g);
        char msg[48]; snprintf(msg,sizeof msg,"  |%s|",buf); sim_log_push(msg);
        for(int i=0;i<_sim_log_n;i++) printf(GRN "%s\n" RST, _sim_log[i]);
        printf(GRY "\n  [INVIO=passo  ESC=esci] " RST); fflush(stdout);
        if(sim_aspetta_passo()) return 0;
        for(int i=0;i<W;i++){
            int l=i>0?cur[i-1]:0, c=cur[i], r=i<W-1?cur[i+1]:0;
            int pattern=(l<<2)|(c<<1)|r;
            int rule30[]={0,1,1,1,1,0,0,0};
            nxt[i]=rule30[pattern];
        }
        for(int i=0;i<W;i++) cur[i]=nxt[i];
    }
    return 1;
}

/* ── Simulazione Caduta Libera ───────────────────────────────────────── */
int sim_caduta_libera(void) {
    double g = 9.81, h0 = 100.0, dt = 0.5;
    sim_log_reset();
    clear_screen(); print_header();
    printf(CYN BLD "\n  \xe2\x9a\x9b\xef\xb8\x8f  Fisica: Caduta Libera\n" RST);
    printf(GRY "  h\xe2\x82\x80 = %.1f m,  g = %.2f m/s\xc2\xb2\n" RST, h0, g);
    printf(GRY "  h(t) = h\xe2\x82\x80 - \xc2\xbd g t\xc2\xb2     v(t) = g t\n\n" RST);
    printf(GRY "  %-8s  %-12s  %-12s\n" RST, "t (s)", "h (m)", "v (m/s)");
    printf(GRY "  "); for(int i=0;i<36;i++) printf("-"); printf("\n" RST);
    printf(GRY "  [INVIO per procedere] " RST); fflush(stdout);
    if(sim_aspetta_passo()) return 0;
    for(double t=0; t<=5.0; t+=dt){
        double h = h0 - 0.5*g*t*t;
        double v = g*t;
        if(h<0){h=0;v=sqrt(2*g*h0);}
        int bar=(int)(h/h0*30);
        clear_screen(); print_header();
        printf(CYN BLD "\n  \xe2\x9a\x9b\xef\xb8\x8f  Caduta Libera\n\n" RST);
        printf(GRY "  t=%.1fs  h=%.1fm  v=%.1fm/s\n\n" RST, t, h, v);
        printf("  Altezza: " GRN);
        for(int i=0;i<bar;i++) printf("\xe2\x96\x88"); printf(RST "\n");
        printf("  Velocit\xc3\xa0: " YLW);
        int vbar=(int)(v/50*20); if(vbar>20)vbar=20;
        for(int i=0;i<vbar;i++) printf("\xe2\x96\x88"); printf(RST "\n\n");
        printf(GRY "  [INVIO=passo  ESC=esci] " RST); fflush(stdout);
        if(sim_aspetta_passo()) return 0;
        if(h<=0) break;
    }
    printf(GRN BLD "\n  v_impatto = %.2f m/s\n\n" RST, sqrt(2*g*h0));
    printf(GRY "  [INVIO] " RST); fflush(stdout); sim_aspetta_passo(); return 1;
}

/* ── Calcolo pH ──────────────────────────────────────────────────────── */
int sim_ph(void) {
    sim_log_reset();
    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\xa7\xaa  Chimica: Calcolo del pH\n" RST);
    printf(GRY "  pH = -log\xe2\x82\x81\xe2\x82\x80[H\xe2\x81\xba]   (scala 0-14)\n\n" RST);
    printf(GRY "  [INVIO per procedere] " RST); fflush(stdout);
    if(sim_aspetta_passo()) return 0;
    struct { const char *nome; double conc; int acido; const char *formula; } esempi[] = {
        {"HCl forte",    0.1,   1, "pH = -log(0.1) = 1.0"},
        {"HCl diluito",  0.001, 1, "pH = -log(0.001) = 3.0"},
        {"Acqua pura",   1e-7,  1, "pH = -log(1e-7) = 7.0"},
        {"NaOH 0.1M",   0.1,   0, "pOH=1.0  pH=14-1=13.0"},
        {"Aceto 1%",    0.017, 1, "Ka=1.8e-5  pH\xe2\x89\x88" "4.7"},
    };
    for(int i=0;i<5;i++){
        double pH;
        if(esempi[i].acido) pH = -log10(esempi[i].conc);
        else pH = 14.0 + log10(esempi[i].conc);
        int bar=(int)(pH/14.0*30);
        const char *col = pH<7?MAG:pH>7?CYN:GRN;
        clear_screen(); print_header();
        printf(CYN BLD "\n  \xf0\x9f\xa7\xaa  pH — %s\n\n" RST, esempi[i].nome);
        printf("  Formula: " GRY "%s\n\n" RST, esempi[i].formula);
        printf("  pH = %s%.2f" RST "\n\n", col, pH);
        printf("  Scala:  [" MAG "0" RST "..............." GRN "7" RST "..............." CYN "14" RST "]\n  Valore: ");
        printf("[");
        for(int j=0;j<bar;j++) printf("%s\xe2\x96\x88" RST, j<(int)(7.0/14*30)?MAG:j<(int)(7.0/14*30)+1?GRN:CYN);
        for(int j=bar;j<30;j++) printf(".");
        printf("]\n\n");
        printf(GRY "  [INVIO=prossimo  ESC=esci] " RST); fflush(stdout);
        if(sim_aspetta_passo()) return 0;
    }
    return 1;
}

/* ── Gas Ideali PV=nRT ───────────────────────────────────────────────── */
int sim_gas_ideali(void) {
    double R = 8.314;
    sim_log_reset();
    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x92\xa8  Legge dei Gas Ideali: PV = nRT\n" RST);
    printf(GRY "  P=pressione(Pa) V=volume(m\xc2\xb3) n=moli T=temperatura(K) R=8.314\n\n" RST);
    printf(GRY "  [INVIO per procedere] " RST); fflush(stdout);
    if(sim_aspetta_passo()) return 0;
    double n=1.0, T=300.0, V=0.001;
    char msg[128];
    for(int step=0;step<10;step++){
        V += 0.001;
        double P = n*R*T/V;
        snprintf(msg, sizeof msg, "V=%.4f m\xc2\xb3  P=%.1f Pa  T=%.0f K  n=%.1f mol", V, P, T, n);
        sim_log_push(msg);
        clear_screen(); print_header();
        printf(CYN BLD "\n  \xf0\x9f\x92\xa8  Gas Ideali — espansione isoterma\n\n" RST);
        int vbar=(int)(V/0.011*30), pbar=(int)(P/248000*30);
        printf("  Volume:    " GRN); for(int i=0;i<vbar;i++) printf("\xe2\x96\x88"); printf(RST " %.4f m\xc2\xb3\n", V);
        printf("  Pressione: " YLW); for(int i=0;i<pbar;i++) printf("\xe2\x96\x88"); printf(RST " %.0f Pa\n\n", P);
        for(int i=0;i<_sim_log_n;i++) printf(GRY "  %s\n" RST, _sim_log[i]);
        printf(GRY "\n  [INVIO=passo  ESC=esci] " RST); fflush(stdout);
        if(sim_aspetta_passo()) return 0;
    }
    return 1;
}

/* ── Stechiometria semplice ───────────────────────────────────────────── */
int sim_stechiometria(void) {
    sim_log_reset();
    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\xa7\xaa  Stechiometria: 2H\xe2\x82\x82 + O\xe2\x82\x82 \xe2\x86\x92 2H\xe2\x82\x82O\n" RST);
    printf(GRY "  Dati: 4g di H\xe2\x82\x82 e 32g di O\xe2\x82\x82 (masse molari: H\xe2\x82\x82=2, O\xe2\x82\x82=32, H\xe2\x82\x82O=18)\n\n" RST);
    printf(GRY "  [INVIO per procedere passo per passo] " RST); fflush(stdout);
    if(sim_aspetta_passo()) return 0;
    struct { const char *titolo; const char *corpo; } passi[] = {
        {"Passo 1: Calcola le moli dei reagenti",
         "  n(H\xe2\x82\x82) = massa/M_mol = 4g / 2 g/mol = 2 mol\n"
         "  n(O\xe2\x82\x82) = massa/M_mol = 32g / 32 g/mol = 1 mol"},
        {"Passo 2: Trova il reagente limitante",
         "  Rapporto stechiometrico: 2 mol H\xe2\x82\x82 : 1 mol O\xe2\x82\x82\n"
         "  Abbiamo 2 mol H\xe2\x82\x82 e 1 mol O\xe2\x82\x82 \xe2\x86\x92 rapporto esatto!\n"
         "  Entrambi si consumano completamente"},
        {"Passo 3: Calcola i prodotti",
         "  2 mol H\xe2\x82\x82 + 1 mol O\xe2\x82\x82 \xe2\x86\x92 2 mol H\xe2\x82\x82O\n"
         "  massa(H\xe2\x82\x82O) = 2 mol \xc3\x97 18 g/mol = 36 g"},
        {"Risultato finale",
         "  4g H\xe2\x82\x82 + 32g O\xe2\x82\x82 \xe2\x86\x92 36g H\xe2\x82\x82O\n"
         "  Verifica: 4 + 32 = 36 \xe2\x9c\x94 (conservazione della massa)"},
    };
    for(int i=0;i<4;i++){
        clear_screen(); print_header();
        printf(CYN BLD "\n  \xf0\x9f\xa7\xaa  Stechiometria — %s\n\n" RST, passi[i].titolo);
        printf(GRY "%s\n" RST, passi[i].corpo);
        printf(GRY "\n  [INVIO=passo  ESC=esci] " RST); fflush(stdout);
        if(sim_aspetta_passo()) return 0;
    }
    return 1;
}
