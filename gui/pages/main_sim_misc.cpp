/* main_sim_misc.cpp — DP, Greedy, Backtracking, String, Math */
#include "main_simulator.h"
#include <QQueue>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <cmath>

QVector<AlgoStep> SimulatorePage::genMatrixChain() {
    QVector<AlgoStep> st;
    /* dimensioni catena: p[i] x p[i+1] */
    QVector<int> p={10,30,5,60,10};
    int n=p.size()-1;
    QVector<int> dp(n*n,0);
    { AlgoStep s; s.arr=p; s.msg=QString("Matrix Chain: %1 matrici. p=%2x%3, %3x%4, %4x%5, %5x%6")
        .arg(n).arg(p[0]).arg(p[1]).arg(p[2]).arg(p[3]).arg(p[4]); st<<s; }
    for(int len=2;len<=n;len++){
        for(int i=0;i<n-len+1;i++){
            int j=i+len-1;
            dp[i*n+j]=INT_MAX;
            for(int k=i;k<j;k++){
                int cost=dp[i*n+k]+dp[(k+1)*n+j]+p[i]*p[k+1]*p[j+1];
                if(cost<dp[i*n+j]){
                    dp[i*n+j]=cost;
                    QVector<int> disp; for(int x=0;x<n;x++) disp<<(dp[x*n+qMin(x+len-1,n-1)]/100+1);
                    AlgoStep s; s.arr=disp; s.cmp<<i<<j;
                    s.msg=QString("dp[%1][%2]=min via k=%3: costo=%4").arg(i).arg(j).arg(k).arg(cost); st<<s;
                }
            }
        }
    }
    QVector<int> disp; for(int x=0;x<n;x++) disp<<(dp[x*n+qMin(x+n-1,n-1)]/100+1);
    AlgoStep sf; sf.arr=disp; sf.msg=QString("Costo ottimale Matrix Chain = %1 moltiplicazioni").arg(dp[0*n+n-1]); st<<sf;
    return st;
}

/* ── EggDrop ── */
QVector<AlgoStep> SimulatorePage::genEggDrop() {
    QVector<AlgoStep> st;
    int eggs=3, floors=8;
    QVector<int> dp((eggs+1)*(floors+1), 0);
    { AlgoStep s; s.arr=dp; s.msg=QString("Egg Drop: %1 uova, %2 piani. dp[e][f]=tentativi minimi").arg(eggs).arg(floors); st<<s; }
    for(int e=1;e<=eggs;e++){
        for(int f=1;f<=floors;f++){
            dp[e*(floors+1)+f]=f; /* worst case con 1 uovo */
            for(int x=1;x<=f;x++){
                int worst=1+qMax(dp[(e-1)*(floors+1)+(x-1)], dp[e*(floors+1)+(f-x)]);
                if(worst<dp[e*(floors+1)+f]){
                    dp[e*(floors+1)+f]=worst;
                    QVector<int> disp; for(int i=0;i<=eggs;i++) disp<<dp[i*(floors+1)+f];
                    AlgoStep s; s.arr=disp; s.cmp<<e;
                    s.msg=QString("dp[%1 uova][%2 piani]=%3 (provo piano x=%4)").arg(e).arg(f).arg(worst).arg(x); st<<s;
                }
            }
        }
    }
    QVector<int> disp; for(int e=0;e<=eggs;e++) disp<<dp[e*(floors+1)+floors];
    AlgoStep sf; sf.arr=disp; sf.found<<eggs;
    sf.msg=QString("Con %1 uova e %2 piani: servono min %3 tentativi").arg(eggs).arg(floors).arg(dp[eggs*(floors+1)+floors]); st<<sf;
    return st;
}

/* ── RodCutting ── */
QVector<AlgoStep> SimulatorePage::genRodCutting() {
    QVector<AlgoStep> st;
    QVector<int> price={0,1,5,8,9,10,17,17,20};
    int n=8;
    QVector<int> dp(n+1,0);
    { AlgoStep s; s.arr=price; s.msg="Rod Cutting: profitto massimo tagliando la sbarra in pezzi"; st<<s; }
    for(int i=1;i<=n;i++){
        for(int j=1;j<=i;j++){
            if(price[j]+dp[i-j]>dp[i]){
                dp[i]=price[j]+dp[i-j];
                AlgoStep s; s.arr=dp; s.cmp<<i; s.swp<<j;
                s.msg=QString("dp[%1]=max via taglio j=%2: prezzo[%3]=%4 + dp[%5]=%6 = %7")
                      .arg(i).arg(j).arg(j).arg(price[j]).arg(i-j).arg(dp[i-j]).arg(dp[i]); st<<s;
            }
        }
        QVector<int> srt; for(int k=0;k<=i;k++) srt<<k;
        AlgoStep s; s.arr=dp; s.sorted=srt; s.msg=QString("Sbarra lunghezza %1: profitto ottimale=%2").arg(i).arg(dp[i]); st<<s;
    }
    QVector<int> all; for(int i=0;i<=n;i++) all<<i;
    AlgoStep sf; sf.arr=dp; sf.sorted=all; sf.msg=QString("Rod Cutting: profitto max per sbarra n=%1 è %2").arg(n).arg(dp[n]); st<<sf;
    return st;
}

/* ── SubsetSumDP ── */
QVector<AlgoStep> SimulatorePage::genSubsetSumDP(QVector<int> arr) {
    QVector<AlgoStep> st;
    int n=qMin((int)arr.size(),6);
    arr.resize(n);
    int target=0; for(int v:arr) target+=v; target/=2;
    QVector<int> dp(target+2,0); dp[0]=1;
    { AlgoStep s; s.arr=arr; s.msg=QString("Subset Sum DP: target=%1, tabella booleana di size %2").arg(target).arg(target+1); st<<s; }
    for(int i=0;i<n;i++){
        QVector<int> ndp=dp;
        for(int j=target;j>=arr[i];j--){
            if(dp[j-arr[i]]){ ndp[j]=1;
                AlgoStep s; s.arr=ndp; s.cmp<<j; s.swp<<(j-arr[i]);
                s.msg=QString("arr[%1]=%2: dp[%3]=true (da dp[%4])").arg(i).arg(arr[i]).arg(j).arg(j-arr[i]); st<<s; }
        }
        dp=ndp;
    }
    QVector<int> found_; for(int j=0;j<=target;j++) if(dp[j]) found_<<j;
    AlgoStep sf; sf.arr=dp; sf.found=found_;
    sf.msg=QString("Subset Sum DP: somma target=%1 → %2").arg(target).arg(dp[target]?"TROVATA":"non trovata"); st<<sf;
    return st;
}

/* ── MaxProductSubarray ── */
QVector<AlgoStep> SimulatorePage::genMaxProductSubarray(QVector<int> arr) {
    QVector<AlgoStep> st;
    int n=arr.size();
    /* mix di positivi e negativi */
    for(int i=0;i<n;i++) if(i%3==1) arr[i]=-arr[i];
    int maxP=arr[0], minP=arr[0], best=arr[0];
    { AlgoStep s; s.arr=arr; s.msg="Max Product Subarray: traccia max E min (negativi si invertono)"; st<<s; }
    for(int i=1;i<n;i++){
        int tmax=std::max({arr[i], maxP*arr[i], minP*arr[i]});
        int tmin=std::min({arr[i], maxP*arr[i], minP*arr[i]});
        maxP=tmax; minP=tmin;
        if(maxP>best) best=maxP;
        AlgoStep s; s.arr=arr; s.cmp<<i;
        if(maxP==best) s.found<<i;
        s.msg=QString("i=%1 arr[i]=%2: maxP=%3, minP=%4, best=%5")
              .arg(i).arg(arr[i]).arg(maxP).arg(minP).arg(best); st<<s;
    }
    AlgoStep sf; sf.arr=arr; sf.msg=QString("Max Product Subarray = %1").arg(best); st<<sf;
    return st;
}

/* ── ActivitySelection ── */
QVector<AlgoStep> SimulatorePage::genActivitySelection() {
    QVector<AlgoStep> st;
    struct Act { int s,e; };
    QVector<Act> acts={{1,3},{2,5},{4,7},{6,9},{5,8},{8,10},{9,11}};
    std::sort(acts.begin(),acts.end(),[](const Act& a,const Act& b){return a.e<b.e;});
    QVector<int> ends; for(auto& a:acts) ends<<a.e;
    { AlgoStep s; s.arr=ends; s.msg="Activity Selection: attività ordinate per fine. Greedy: scegli sempre quella che finisce prima."; st<<s; }
    QVector<int> sel;
    int lastEnd=-1;
    for(int i=0;i<(int)acts.size();i++){
        if(acts[i].s>=lastEnd){ sel<<i; lastEnd=acts[i].e;
            AlgoStep s; s.arr=ends; s.found=sel; s.cmp<<i;
            s.msg=QString("Seleziono att. %1 [%2..%3] (inizia dopo fine=%4)").arg(i).arg(acts[i].s).arg(acts[i].e).arg(lastEnd); st<<s;
        } else {
            AlgoStep s; s.arr=ends; s.swp<<i;
            s.msg=QString("Salto att. %1 [%2..%3]: sovrapposta (last end=%4)").arg(i).arg(acts[i].s).arg(acts[i].e).arg(lastEnd); st<<s;
        }
    }
    AlgoStep sf; sf.arr=ends; sf.sorted=sel;
    sf.msg=QString("Activity Selection: %1 attività non sovrapposte selezionate (ottimale)").arg(sel.size()); st<<sf;
    return st;
}

/* ── FractionalKnapsack ── */
QVector<AlgoStep> SimulatorePage::genFractionalKnapsack() {
    QVector<AlgoStep> st;
    struct Item { int v,w; };
    QVector<Item> items={{60,10},{100,20},{120,30},{80,25},{50,15}};
    int cap=50;
    std::sort(items.begin(),items.end(),[](const Item& a,const Item& b){
        return (double)a.v/a.w > (double)b.v/b.w;
    });
    QVector<int> vals; for(auto& it:items) vals<<it.v;
    { AlgoStep s; s.arr=vals; s.msg=QString("Fractional Knapsack: capacity=%1. Ordina per valore/peso decrescente.").arg(cap); st<<s; }
    double totalVal=0; int rem=cap;
    QVector<int> taken;
    for(int i=0;i<(int)items.size();i++){
        if(rem>=items[i].w){ rem-=items[i].w; totalVal+=items[i].v; taken<<i;
            AlgoStep s; s.arr=vals; s.found=taken; s.cmp<<i;
            s.msg=QString("Prendo intero item %1 (v=%2,w=%3) ratio=%.1f → tot=%.1f rem=%4")
                  .arg(i).arg(items[i].v).arg(items[i].w).arg((double)items[i].v/items[i].w).arg(totalVal).arg(rem); st<<s;
        } else if(rem>0){
            double frac=(double)rem/items[i].w;
            totalVal+=frac*items[i].v;
            AlgoStep s; s.arr=vals; s.cmp<<i; s.swp<<i;
            s.msg=QString("Prendo fraz. %1%% di item %2 (v=%3,w=%4) → val+=%.1f")
                  .arg((int)(frac*100)).arg(i).arg(items[i].v).arg(items[i].w).arg(frac*items[i].v); st<<s;
            rem=0; taken<<i; break;
        }
    }
    AlgoStep sf; sf.arr=vals; sf.sorted=taken;
    sf.msg=QString("Fractional Knapsack: valore massimo = %.1f (capacity=%2)").arg(totalVal).arg(cap); st<<sf;
    return st;
}

/* ── Huffman ── */
QVector<AlgoStep> SimulatorePage::genHuffman(QVector<int> freq) {
    QVector<AlgoStep> st;
    int n=qMin((int)freq.size(),8);
    freq.resize(n);
    using P=QPair<int,int>; /* peso, id */
    QVector<P> pq;
    for(int i=0;i<n;i++) pq.push_back({freq[i],i});
    std::sort(pq.begin(),pq.end());
    { AlgoStep s; s.arr=freq; s.msg="Huffman: min-heap di frequenze. Unisce i 2 minimi a ogni passo."; st<<s; }
    int nextId=n;
    while(pq.size()>1){
        std::sort(pq.begin(),pq.end());
        P a=pq.front(); pq.pop_front();
        P b=pq.front(); pq.pop_front();
        int merged=a.first+b.first;
        AlgoStep s; s.arr=freq;
        if(a.second<n) s.cmp<<a.second;
        if(b.second<n) s.swp<<b.second;
        s.msg=QString("Unisci freq=%1 + freq=%2 → nodo interno %3 (peso=%4)")
              .arg(a.first).arg(b.first).arg(nextId).arg(merged); st<<s;
        freq.append(merged);
        pq.push_back({merged,nextId++});
    }
    QVector<int> all; for(int i=0;i<freq.size();i++) all<<i;
    AlgoStep sf; sf.arr=freq; sf.found=all;
    sf.msg=QString("Huffman completato! Simboli frequenti = codici corti. Radice=%1").arg(pq.front().first); st<<sf;
    return st;
}

/* ── JobScheduling ── */
QVector<AlgoStep> SimulatorePage::genJobScheduling() {
    QVector<AlgoStep> st;
    struct Job { int p,d; }; /* profit, deadline */
    QVector<Job> jobs={{20,2},{15,2},{10,1},{5,3},{1,3}};
    std::sort(jobs.begin(),jobs.end(),[](const Job& a,const Job& b){return a.p>b.p;});
    int maxD=0; for(auto& j:jobs) maxD=qMax(maxD,j.d);
    QVector<int> jobSlots(maxD, -1);
    QVector<int> profits; for(auto& j:jobs) profits<<j.p;
    { AlgoStep s; s.arr=profits; s.msg=QString("Job Scheduling EDF: %1 job, ordina per profitto desc. %2 slot.").arg(jobs.size()).arg(maxD); st<<s; }
    int total=0;
    QVector<int> sel;
    for(int i=0;i<(int)jobs.size();i++){
        for(int k=qMin(jobs[i].d-1,maxD-1);k>=0;k--){
            if(jobSlots[k]<0){ jobSlots[k]=i; total+=jobs[i].p; sel<<i;
                QVector<int> disp(maxD,0); for(int m=0;m<maxD;m++) if(jobSlots[m]>=0) disp[m]=jobs[jobSlots[m]].p;
                AlgoStep as; as.arr=disp; as.found=sel; as.cmp<<k;
                as.msg=QString("Job %1 (p=%2,d=%3) \xe2\x86\x92 slot %4").arg(i).arg(jobs[i].p).arg(jobs[i].d).arg(k); st<<as;
                break; }
        }
    }
    QVector<int> disp(maxD,0); for(int k=0;k<maxD;k++) if(jobSlots[k]>=0) disp[k]=jobs[jobSlots[k]].p;
    AlgoStep sf; sf.arr=disp; sf.msg=QString("Job Scheduling: profitto massimo=%1 (%2 job)").arg(total).arg(sel.size()); st<<sf;
    return st;
}

/* ── CoinGreedy ── */
QVector<AlgoStep> SimulatorePage::genCoinGreedy(QVector<int> coins, int target) {
    QVector<AlgoStep> st;
    std::sort(coins.begin(),coins.end(),std::greater<int>());
    QVector<int> used(coins.size(),0);
    int rem=target;
    { AlgoStep s; s.arr=coins; s.msg=QString("Coin Change Greedy: target=%1, monete decrescenti").arg(target); st<<s; }
    for(int i=0;i<(int)coins.size()&&rem>0;i++){
        if(coins[i]<=rem){ int cnt=rem/coins[i]; used[i]=cnt; rem-=cnt*coins[i];
            AlgoStep s; s.arr=used; s.found<<i;
            s.msg=QString("Uso %1x moneta %2 (resto=%3)").arg(cnt).arg(coins[i]).arg(rem); st<<s; }
    }
    QVector<int> all; for(int i=0;i<(int)coins.size();i++) all<<i;
    AlgoStep sf; sf.arr=used; sf.sorted=all;
    sf.msg=QString("Greedy: target=%1, resto=%2 (resto≠0 = greedy non ottimale su questo insieme)").arg(target).arg(rem); st<<sf;
    return st;
}

/* ── MinPlatforms ── */
QVector<AlgoStep> SimulatorePage::genMinPlatforms() {
    QVector<AlgoStep> st;
    QVector<int> arr={900,940,950,1100,1500,1800};
    QVector<int> dep={910,1200,1120,1130,1900,2000};
    int n=arr.size();
    std::sort(arr.begin(),arr.end()); std::sort(dep.begin(),dep.end());
    { AlgoStep s; s.arr=arr; s.msg="Min Platforms: orari di arrivo e partenza (in centinaia). Massimo treni contemporanei."; st<<s; }
    int plat=1, maxPlat=1, i=1, j=0;
    QVector<int> display(n,0);
    while(i<n&&j<n){
        if(arr[i]<=dep[j]){ plat++; if(plat>maxPlat) maxPlat=plat;
            display[i]=plat;
            AlgoStep s; s.arr=display; s.cmp<<i;
            s.msg=QString("Arrivo %1 ≤ partenza %2 → +1 piattaforma = %3").arg(arr[i]).arg(dep[j]).arg(plat); st<<s;
            i++;
        } else { plat--;
            display[j]=0;
            AlgoStep s; s.arr=display; s.swp<<j;
            s.msg=QString("Partenza %1 < arrivo %2 → -1 piattaforma = %3").arg(dep[j]).arg(arr[i<n?i:n-1]).arg(plat); st<<s;
            j++;
        }
    }
    AlgoStep sf; sf.arr=display;
    sf.msg=QString("Min Platforms = %1 (massimo treni contemporanei)").arg(maxPlat); st<<sf;
    return st;
}

/* ── NQueens ── */
void SimulatorePage::_nqSolve(int col, QVector<int>& board, QVector<AlgoStep>& st, int& found_) {
    int n=board.size();
    if(col==n){ found_++;
        AlgoStep s; s.arr=board; s.found=board;
        s.msg=QString("SOLUZIONE #%1 trovata! Regina in ogni colonna senza attacchi.").arg(found_); st<<s;
        return; }
    for(int row=0;row<n;row++){
        bool ok=true;
        for(int c=0;c<col;c++){
            if(board[c]==row||qAbs(board[c]-row)==qAbs(c-col)){ ok=false; break; }
        }
        if(ok){ board[col]=row;
            AlgoStep s; s.arr=board; s.cmp<<col;
            s.msg=QString("Piazzo regina col=%1, riga=%2").arg(col).arg(row); st<<s;
            if(found_<3) _nqSolve(col+1,board,st,found_);
            if(found_>=3) return;
            board[col]=-1;
            AlgoStep sb; sb.arr=board; sb.swp<<col;
            sb.msg=QString("Backtrack: colonna %1 libera").arg(col); st<<sb;
        }
    }
}
QVector<AlgoStep> SimulatorePage::genNQueens(int n) {
    QVector<AlgoStep> st;
    n=qMin(n,5);
    QVector<int> board(n,-1);
    { AlgoStep s; s.arr=board; s.msg=QString("N-Queens %1x%1: backtracking. -1=libero, val=riga della regina.").arg(n).arg(n); st<<s; }
    int found_=0;
    _nqSolve(0,board,st,found_);
    AlgoStep sf; sf.arr=board; sf.msg=QString("N-Queens %1: trovate ≥%2 soluzioni (totale=%3)").arg(n).arg(found_).arg(n<=6?QStringList({"1","0","0","2","10","4"})[n-1]:"?"); st<<sf;
    return st;
}

/* ── SubsetSum backtracking ── */
QVector<AlgoStep> SimulatorePage::genSubsetSum(QVector<int> arr, int target) {
    QVector<AlgoStep> st;
    int n=qMin((int)arr.size(),7); arr.resize(n);
    if(target<=0||target>100) target=arr[0]+arr[1]+arr[2];
    { AlgoStep s; s.arr=arr; s.msg=QString("Subset Sum BT: target=%1. Include/escludi ogni elemento.").arg(target); st<<s; }
    int found_=0;
    std::function<void(int,int,QVector<int>)> bt=[&](int i,int cur,QVector<int> chosen){
        if(found_>=4) return;
        if(cur==target){ found_++;
            AlgoStep s; s.arr=arr; s.found=chosen;
            s.msg=QString("TROVATO subset #%1 con somma=%2").arg(found_).arg(target); st<<s; return; }
        if(i>=n||cur>target) return;
        /* includi */
        QVector<int> ch2=chosen; ch2<<i;
        AlgoStep s1; s1.arr=arr; s1.cmp<<i; s1.found=ch2;
        s1.msg=QString("Includo arr[%1]=%2 → somma=%3").arg(i).arg(arr[i]).arg(cur+arr[i]); st<<s1;
        bt(i+1,cur+arr[i],ch2);
        /* escludi */
        AlgoStep s2; s2.arr=arr; s2.swp<<i; s2.found=chosen;
        s2.msg=QString("Escludo arr[%1]=%2 → somma=%3").arg(i).arg(arr[i]).arg(cur); st<<s2;
        bt(i+1,cur,chosen);
    };
    bt(0,0,{});
    AlgoStep sf; sf.arr=arr;
    sf.msg=QString("Subset Sum BT: trovati %1 sottoinsiemi con somma=%2").arg(found_).arg(target); st<<sf;
    return st;
}

/* ── Permutations ── */
QVector<AlgoStep> SimulatorePage::genPermutations(QVector<int> arr) {
    QVector<AlgoStep> st;
    int n=qMin((int)arr.size(),5); arr.resize(n);
    int count=0;
    { AlgoStep s; s.arr=arr; s.msg=QString("Permutazioni di %1 elementi: genera tutte le %2! = %3 permutazioni").arg(n).arg(n).arg([](int k){int r=1;for(int i=2;i<=k;i++)r*=i;return r;}(n)); st<<s; }
    std::function<void(int)> perm=[&](int start){
        if(count>=12) return;
        if(start==n){ count++;
            AlgoStep s; s.arr=arr; s.found=QVector<int>(n); for(int i=0;i<n;i++) s.found[i]=i;
            s.msg=QString("Permutazione #%1: [%2]").arg(count).arg([&](){QString r; for(int v:arr) r+=QString::number(v)+" "; return r.trimmed();}()); st<<s; return; }
        for(int i=start;i<n;i++){
            std::swap(arr[start],arr[i]);
            AlgoStep s; s.arr=arr; s.swp<<start<<i;
            s.msg=QString("Scambio pos %1↔%2").arg(start).arg(i); st<<s;
            perm(start+1);
            std::swap(arr[start],arr[i]);
        }
    };
    perm(0);
    AlgoStep sf; sf.arr=arr; sf.msg=QString("Permutazioni: generate %1 permutazioni (O(n!))").arg(count); st<<sf;
    return st;
}

/* ── FloodFill ── */
QVector<AlgoStep> SimulatorePage::genFloodFill(QVector<int> arr) {
    QVector<AlgoStep> st;
    /* griglia 4x4 con valori 0..2 */
    const int W=4, H=4;
    QVector<int> grid(W*H);
    for(int i=0;i<W*H;i++) grid[i]=arr[i%arr.size()]%3;
    int startColor=grid[0], fillColor=9;
    if(startColor==fillColor) fillColor=8;
    { AlgoStep s; s.arr=grid; s.cmp<<0; s.msg=QString("Flood Fill: riempi da (0,0) colore=%1 → nuovo=%2 (BFS)").arg(startColor).arg(fillColor); st<<s; }
    QQueue<int> q; q.enqueue(0); grid[0]=fillColor;
    QVector<int> filled; filled<<0;
    int dx[]={1,-1,0,0}, dy[]={0,0,1,-1};
    while(!q.isEmpty()){
        int pos=q.dequeue(); int x=pos%W, y=pos/W;
        AlgoStep s; s.arr=grid; s.found=filled; s.cmp<<pos;
        s.msg=QString("Visita (%1,%2): colora con %3").arg(x).arg(y).arg(fillColor); st<<s;
        for(int d=0;d<4;d++){
            int nx=x+dx[d], ny=y+dy[d];
            if(nx<0||nx>=W||ny<0||ny>=H) continue;
            int np=ny*W+nx;
            if(grid[np]==startColor){ grid[np]=fillColor; q.enqueue(np); filled<<np; }
        }
    }
    AlgoStep sf; sf.arr=grid; sf.found=filled;
    sf.msg=QString("Flood Fill completato: %1 celle colorate").arg(filled.size()); st<<sf;
    return st;
}

/* ── RatInMaze ── */
QVector<AlgoStep> SimulatorePage::genRatInMaze() {
    QVector<AlgoStep> st;
    const int N=4;
    int maze[N][N]={{1,0,0,0},{1,1,0,1},{0,1,0,0},{0,1,1,1}};
    QVector<int> display(N*N,0);
    for(int i=0;i<N;i++) for(int j=0;j<N;j++) display[i*N+j]=maze[i][j];
    { AlgoStep s; s.arr=display; s.cmp<<0; s.msg="Rat in a Maze: da (0,0) a (3,3). 1=libero, 0=muro."; st<<s; }
    QVector<int> sol(N*N,0);
    std::function<bool(int,int)> solve=[&](int r,int c)->bool{
        if(r==N-1&&c==N-1){ sol[r*N+c]=2;
            AlgoStep s; s.arr=sol; QVector<int> path; for(int i=0;i<N*N;i++) if(sol[i]) path<<i; s.found=path;
            s.msg="PERCORSO TROVATO! Il topo ha raggiunto (3,3)."; st<<s; return true; }
        if(r<0||r>=N||c<0||c>=N||!maze[r][c]) return false;
        sol[r*N+c]=1;
        AlgoStep s; s.arr=sol; s.cmp<<r*N+c;
        s.msg=QString("Provo (%1,%2)").arg(r).arg(c); st<<s;
        int dr[]={1,0,0,-1}, dc[]={0,1,-1,0};
        for(int d=0;d<4;d++) if(solve(r+dr[d],c+dc[d])) return true;
        sol[r*N+c]=0;
        AlgoStep sb; sb.arr=sol; sb.swp<<r*N+c;
        sb.msg=QString("Backtrack da (%1,%2)").arg(r).arg(c); st<<sb;
        return false;
    };
    solve(0,0);
    return st;
}

/* ── KMP ── */
QVector<AlgoStep> SimulatorePage::genKMP(const QString& pattern, const QString& text) {
    QVector<AlgoStep> st;
    int m=pattern.size(), n=text.size();
    QVector<int> fail(m,0);
    /* costruisci failure function */
    for(int i=1;i<m;i++){
        int j=fail[i-1];
        while(j>0&&pattern[i]!=pattern[j]) j=fail[j-1];
        if(pattern[i]==pattern[j]) j++;
        fail[i]=j;
    }
    QVector<int> fdisp(m); for(int i=0;i<m;i++) fdisp[i]=fail[i];
    { AlgoStep s; s.arr=fdisp; s.msg=QString("KMP failure function per pattern='%1'. Evita backtracking.").arg(pattern); st<<s; }
    /* ricerca */
    QVector<int> textV(n), patV(m);
    for(int i=0;i<n;i++) textV[i]=text[i].unicode()%30+1;
    for(int i=0;i<m;i++) patV[i]=pattern[i].unicode()%30+1;
    int j=0;
    QVector<int> found_;
    for(int i=0;i<n;){
        AlgoStep s; s.arr=textV; s.cmp<<i; if(j>0&&j-1<textV.size()) s.swp<<j-1;
        s.msg=QString("text[%1]='%2' vs pattern[%3]='%4'").arg(i).arg(text[i]).arg(j).arg(j<m?pattern[j]:QChar('?')); st<<s;
        if(text[i]==pattern[j]){ i++; j++;
            if(j==m){ found_<<(i-m);
                AlgoStep sf; sf.arr=textV; sf.found=found_;
                sf.msg=QString("TROVATO pattern a pos %1!").arg(i-m); st<<sf; j=fail[j-1]; }
        } else if(j>0) j=fail[j-1];
        else i++;
    }
    AlgoStep sfin; sfin.arr=textV; sfin.found=found_;
    sfin.msg=QString("KMP completato: pattern '%1' trovato %2 volta/e").arg(pattern).arg(found_.size()); st<<sfin;
    return st;
}

/* ── RabinKarp ── */
QVector<AlgoStep> SimulatorePage::genRabinKarp(const QString& pattern, const QString& text) {
    QVector<AlgoStep> st;
    int m=pattern.size(), n=text.size();
    if(m>n){ AlgoStep s; s.arr={0}; s.msg="Pattern più lungo del testo!"; st<<s; return st; }
    const int BASE=31, MOD=101;
    int pH=0, tH=0, pw=1;
    for(int i=0;i<m-1;i++) pw=pw*BASE%MOD;
    for(int i=0;i<m;i++){
        pH=(pH*BASE+pattern[i].unicode())%MOD;
        tH=(tH*BASE+text[i].unicode())%MOD;
    }
    QVector<int> textV(n); for(int i=0;i<n;i++) textV[i]=text[i].unicode()%30+1;
    { AlgoStep s; s.arr=textV; s.msg=QString("Rabin-Karp: hash pattern='%1' =%2. Rolling hash su testo.").arg(pattern).arg(pH); st<<s; }
    QVector<int> found_;
    for(int i=0;i<=n-m;i++){
        AlgoStep s; s.arr=textV; QVector<int> window; for(int k=i;k<i+m;k++) window<<k; s.cmp=window;
        s.msg=QString("Finestra [%1..%2]: hash=%3 (pattern hash=%4) → %5").arg(i).arg(i+m-1).arg(tH).arg(pH).arg(tH==pH?"HIT":"miss"); st<<s;
        if(tH==pH){ /* verifica */
            bool ok=true; for(int k=0;k<m;k++) if(text[i+k]!=pattern[k]){ok=false;break;}
            if(ok){ found_<<i;
                AlgoStep sf; sf.arr=textV; sf.found=found_;
                sf.msg=QString("TROVATO a pos %1 (hash match + verifica char OK)").arg(i); st<<sf; }
        }
        if(i<n-m) tH=(BASE*(tH-text[i].unicode()*pw%MOD+MOD)+text[i+m].unicode())%MOD;
    }
    AlgoStep sfin; sfin.arr=textV; sfin.found=found_;
    sfin.msg=QString("Rabin-Karp: trovato %1 volta/e").arg(found_.size()); st<<sfin;
    return st;
}

/* ── ZAlgorithm ── */
QVector<AlgoStep> SimulatorePage::genZAlgorithm(const QString& s) {
    QVector<AlgoStep> st;
    int n=s.size();
    QVector<int> Z(n,0); Z[0]=n;
    int l=0, r=0;
    { QVector<int> disp(n,0); disp[0]=n;
      AlgoStep s0; s0.arr=disp; s0.msg=QString("Z-Algorithm su '%1': Z[i]=lunghezza prefisso comune con s[i..]").arg(s); st<<s0; }
    for(int i=1;i<n;i++){
        if(i<r) Z[i]=qMin(r-i, Z[i-l]);
        while(i+Z[i]<n && s[Z[i]]==s[i+Z[i]]) Z[i]++;
        if(i+Z[i]>r){ l=i; r=i+Z[i]; }
        AlgoStep snap; snap.arr=Z; snap.cmp<<i;
        snap.msg=QString("Z[%1]=%2 (s[%1..]='%3' vs prefisso '%4')")
                 .arg(i).arg(Z[i]).arg(s.mid(i,qMin(Z[i],4))).arg(s.left(qMin(Z[i],4))); st<<snap;
    }
    QVector<int> all; for(int i=0;i<n;i++) all<<i;
    AlgoStep sf; sf.arr=Z; sf.sorted=all; sf.msg=QString("Z-Array completato per '%1'").arg(s); st<<sf;
    return st;
}

/* ── Manacher ── */
QVector<AlgoStep> SimulatorePage::genManacher(const QString& s) {
    QVector<AlgoStep> st;
    /* trasforma in #a#b#a# */
    QString t="#";
    for(QChar c:s){ t+=c; t+="#"; }
    int n=t.size();
    QVector<int> P(n,0);
    int C=0, R=0;
    { AlgoStep s0; s0.arr=QVector<int>(s.size(),0); s0.msg=QString("Manacher su '%1' → trasformato in '%2'").arg(s).arg(t); st<<s0; }
    for(int i=0;i<n;i++){
        if(i<R) P[i]=qMin(R-i, P[2*C-i]);
        while(i-P[i]-1>=0 && i+P[i]+1<n && t[i-P[i]-1]==t[i+P[i]+1]) P[i]++;
        if(i+P[i]>R){ C=i; R=i+P[i]; }
        if(P[i]>0){
            QVector<int> disp(s.size(),0);
            int center=(i-1)/2, half=P[i]/2;
            if(center>=0&&center<s.size()) disp[qMax(0,center-half)]=P[i];
            AlgoStep snap; snap.arr=disp; snap.cmp<<qMax(0,(center-half>0?center-half:0));
            snap.msg=QString("i=%1 ('%2'): P[i]=%3, palindromo di raggio %4").arg(i).arg(t[i]).arg(P[i]).arg(P[i]); st<<snap;
        }
    }
    int bestR=*std::max_element(P.begin(),P.end());
    int bestC=P.indexOf(bestR);
    int lo=(bestC-bestR)/2, len=bestR;
    AlgoStep sf; sf.arr=P; sf.found<<bestC;
    sf.msg=QString("Palindromo più lungo: '%1' (len=%2, posizione=%3)")
           .arg(s.mid(lo,len)).arg(len).arg(lo); st<<sf;
    return st;
}

/* ── LongestCommonPrefix ── */
QVector<AlgoStep> SimulatorePage::genLongestCommonPrefix(QVector<int> arr) {
    QVector<AlgoStep> st;
    int n=arr.size();
    /* usa l'array come suffissi di un vettore numerico */
    QVector<QVector<int>> suffixes;
    for(int i=0;i<n;i++){
        QVector<int> s;
        for(int j=i;j<n;j++) s<<arr[j];
        suffixes<<s;
    }
    /* ordina lessicograficamente */
    std::sort(suffixes.begin(),suffixes.end());
    QVector<int> lcp(n,0);
    for(int i=1;i<n;i++){
        int k=0;
        while(k<(int)suffixes[i-1].size()&&k<(int)suffixes[i].size()&&suffixes[i-1][k]==suffixes[i][k]) k++;
        lcp[i]=k;
    }
    { AlgoStep s; s.arr=lcp; s.msg="LCP Array su suffix array. lcp[i]=prefisso comune tra suffisso i e i-1."; st<<s; }
    for(int i=1;i<n;i++){
        AlgoStep s; s.arr=lcp; s.cmp<<i;
        s.msg=QString("lcp[%1]=%2: i primi %3 elementi in comune tra suff%1 e suff%4").arg(i).arg(lcp[i]).arg(lcp[i]).arg(i-1); st<<s;
    }
    int maxLcp=*std::max_element(lcp.begin(),lcp.end());
    QVector<int> all; for(int i=0;i<n;i++) all<<i;
    AlgoStep sf; sf.arr=lcp; sf.sorted=all; sf.msg=QString("LCP Array completato. Prefisso comune massimo = %1").arg(maxLcp); st<<sf;
    return st;
}

/* ── SieveSundaram ── */
QVector<AlgoStep> SimulatorePage::genSieveSundaram(int limit) {
    QVector<AlgoStep> st;
    QVector<int> a(limit+1, 1);
    { AlgoStep s; s.arr=a; s.msg=QString("Crivello di Sundaram: elimina i+j+2ij per 1≤i≤j, produce primi dispari ≤ %1").arg(2*limit+1); st<<s; }
    for(int i=1;i<=limit;i++){
        for(int j=i;i+j+2*i*j<=limit;j++){
            int idx=i+j+2*i*j;
            if(a[idx]){ a[idx]=0;
                AlgoStep s; s.arr=a; s.swp<<idx;
                s.msg=QString("Elimina %1 (i=%2,j=%3): 2*%1+1=%4 non è primo").arg(idx).arg(i).arg(j).arg(2*idx+1); st<<s; }
        }
    }
    QVector<int> primes={2};
    for(int i=1;i<=limit;i++) if(a[i]) primes<<2*i+1;
    AlgoStep sf; sf.arr=a;
    for(int i=1;i<=limit;i++) if(a[i]) sf.sorted<<i;
    sf.msg=QString("Sundaram: trovati %1 primi (incluso 2)").arg(primes.size()); st<<sf;
    return st;
}

/* ── MillerRabin ── */
QVector<AlgoStep> SimulatorePage::genMillerRabin(int n) {
    QVector<AlgoStep> st;
    if(n%2==0) n++;
    auto mulmod=[](long long a,long long b,long long m)->long long{
        long long r=0; a%=m;
        while(b>0){ if(b&1) r=(r+a)%m; a=a*2%m; b>>=1; }
        return r;
    };
    auto powmod=[&](long long a,long long b,long long m)->long long{
        long long r=1; a%=m;
        while(b>0){ if(b&1) r=mulmod(r,a,m); a=mulmod(a,a,m); b>>=1; }
        return r;
    };
    /* scrivi n-1 = 2^s * d */
    int s=0; long long d=n-1;
    while(d%2==0){ d/=2; s++; }
    QVector<int> witnesses={2,3,5,7};
    QVector<int> display(witnesses.size(),0);
    { AlgoStep s0; s0.arr=display; s0.msg=QString("Miller-Rabin test su n=%1: n-1=2^%2 * %3").arg(n).arg(s).arg(d); st<<s0; }
    bool composite=false;
    for(int wi=0;wi<(int)witnesses.size();wi++){
        long long a=witnesses[wi]; if(a>=n) continue;
        long long x=powmod(a,d,n);
        display[wi]=(int)(x%50+1);
        bool ok=(x==1||x==n-1);
        if(!ok){
            for(int r=0;r<s-1;r++){ x=mulmod(x,x,n); if(x==n-1){ok=true;break;} }
        }
        AlgoStep snap; snap.arr=display; snap.cmp<<wi;
        snap.msg=QString("Testimone a=%1: x=%2 → %3").arg(a).arg(x%100).arg(ok?"PASS (probabilmente primo)":"COMPOSITE!"); st<<snap;
        if(!ok){ composite=true; display[wi]=0; break; }
    }
    AlgoStep sf; sf.arr=display;
    sf.msg=QString("Miller-Rabin: n=%1 è %2 (k=%3 testimoni, errore<4^(-k))").arg(n).arg(composite?"COMPOSTO":"probabilmente PRIMO").arg(witnesses.size()); st<<sf;
    return st;
}

/* ── PascalTriangle ── */
QVector<AlgoStep> SimulatorePage::genPascalTriangle(int n) {
    QVector<AlgoStep> st;
    n=qMin(n,8);
    QVector<int> prev={1};
    { AlgoStep s; s.arr=prev; s.msg=QString("Triangolo di Pascal: %1 righe. C(n,k) = C(n-1,k-1) + C(n-1,k)").arg(n); st<<s; }
    for(int row=1;row<n;row++){
        QVector<int> cur={1};
        for(int k=1;k<row;k++) cur<<prev[k-1]+prev[k];
        cur<<1;
        AlgoStep s; s.arr=cur; for(int i=0;i<(int)cur.size();i++) s.cmp<<i;
        s.msg=QString("Riga %1: coefficienti binomiali C(%1,0)..C(%1,%1)").arg(row); st<<s;
        prev=cur;
    }
    QVector<int> all; for(int i=0;i<(int)prev.size();i++) all<<i;
    AlgoStep sf; sf.arr=prev; sf.sorted=all; sf.msg=QString("Pascal riga %1 completata").arg(n-1); st<<sf;
    return st;
}

/* ── Catalan ── */
QVector<AlgoStep> SimulatorePage::genCatalan(int n) {
    QVector<AlgoStep> st;
    n=qMin(n,10);
    QVector<int> cat(n+1,0); cat[0]=cat[1]=1;
    { AlgoStep s; s.arr=cat; s.msg="Numeri di Catalan: C(n) = sum C(i)*C(n-1-i) per i=0..n-1"; st<<s; }
    for(int i=2;i<=n;i++){
        for(int j=0;j<i;j++) cat[i]+=cat[j]*cat[i-1-j];
        AlgoStep s; s.arr=cat; s.cmp<<i;
        s.msg=QString("C(%1) = %2 (alberi BST con %1 nodi, parentesizzazioni di %2 fattori)").arg(i).arg(cat[i]); st<<s;
    }
    QVector<int> all; for(int i=0;i<=n;i++) all<<i;
    AlgoStep sf; sf.arr=cat; sf.sorted=all; sf.msg=QString("Catalan(%1) = %2. Sequenza: 1,1,2,5,14,42,132...").arg(n).arg(cat[n]); st<<sf;
    return st;
}

/* ── MonteCarloPi ── */
QVector<AlgoStep> SimulatorePage::genMonteCarloPi(int n) {
    QVector<AlgoStep> st;
    n=qMin(n,60);
    int inside=0;
    QVector<int> display(10,0); /* barre: % dentro cerchio per 10 gruppi */
    { AlgoStep s; s.arr=display; s.msg=QString("Monte Carlo Pi: %1 punti casuali. Punti dentro cerchio / totale ≈ π/4").arg(n); st<<s; }
    for(int i=1;i<=n;i++){
        double x=(double)rand()/RAND_MAX*2-1;
        double y=(double)rand()/RAND_MAX*2-1;
        if(x*x+y*y<=1.0) inside++;
        if(i%(n/10+1)==0||i==n){
            int gi=qMin(9,(i-1)*10/n);
            display[gi]=(int)(4.0*inside/i*10);
            double pi=4.0*inside/i;
            AlgoStep s; s.arr=display; s.cmp<<gi;
            s.msg=QString("Dopo %1 punti: dentro=%2, stima π=%3").arg(i).arg(inside).arg(QString::number(pi,'f',4)); st<<s;
        }
    }
    double pi=4.0*inside/n;
    AlgoStep sf; sf.arr=display; sf.msg=QString("Monte Carlo Pi ≈ %1 (valore reale 3.14159...)").arg(QString::number(pi,'f',5)); st<<sf;
    return st;
}

/* ── Collatz ── */
QVector<AlgoStep> SimulatorePage::genCollatz(int n) {
    QVector<AlgoStep> st;
    QVector<int> seq; seq<<n;
    { AlgoStep s; s.arr=seq; s.msg=QString("Congettura di Collatz: n=%1. Pari→n/2, dispari→3n+1. Raggiunge 1?").arg(n); st<<s; }
    int steps=0;
    while(n!=1&&steps<100){
        if(n%2==0){ n/=2;
            AlgoStep s; seq<<n; s.arr=seq.mid(qMax(0,(int)seq.size()-10)); s.found<<((int)seq.size()-1)%10;
            s.msg=QString("Pari → n/2 = %1 (passo %2)").arg(n).arg(++steps); st<<s; }
        else { n=3*n+1;
            AlgoStep s; seq<<n; s.arr=seq.mid(qMax(0,(int)seq.size()-10)); s.swp<<((int)seq.size()-1)%10;
            s.msg=QString("Dispari → 3n+1 = %1 (passo %2)").arg(n).arg(++steps); st<<s; }
    }
    QVector<int> disp=seq.mid(qMax(0,(int)seq.size()-10));
    AlgoStep sf; sf.arr=disp; sf.msg=QString("Collatz: %1 passi per raggiungere 1 (congettura non dimostrata!)").arg(steps); st<<sf;
    return st;
}

/* ── Karatsuba ── */
QVector<AlgoStep> SimulatorePage::genKaratsuba(int a, int b) {
    QVector<AlgoStep> st;
    { QVector<int> disp={a/100,a%100,b/100,b%100};
      AlgoStep s; s.arr=disp; s.msg=QString("Karatsuba: %1 × %2. Divide ogni numero in 2 metà.").arg(a).arg(b); st<<s; }
    /* a = a1*100 + a0, b = b1*100 + b0 */
    int a1=a/100, a0=a%100, b1=b/100, b0=b%100;
    int z0=a0*b0, z2=a1*b1;
    { QVector<int> disp={z0,z2,a0+a1,b0+b1};
      AlgoStep s; s.arr=disp; s.cmp<<0<<1;
      s.msg=QString("z0=a0*b0=%1*%2=%3, z2=a1*b1=%4*%5=%6").arg(a0).arg(b0).arg(z0).arg(a1).arg(b1).arg(z2); st<<s; }
    int z1=(a0+a1)*(b0+b1)-z0-z2;
    { QVector<int> disp={z0,z1,z2,0};
      AlgoStep s; s.arr=disp; s.swp<<1;
      s.msg=QString("z1=(a0+a1)(b0+b1)-z0-z2=%1*%2-%3-%4=%5").arg(a0+a1).arg(b0+b1).arg(z0).arg(z2).arg(z1); st<<s; }
    long long result=(long long)z2*10000 + (long long)z1*100 + z0;
    { QVector<int> disp={(int)(result/10000),(int)((result/100)%100),(int)(result%100),(int)(a*b==result?1:0)};
      AlgoStep sf; sf.arr=disp; sf.found<<0<<1<<2;
      sf.msg=QString("Risultato: %1*10000 + %2*100 + %3 = %4 (check=%5*%6=%7)")
             .arg(z2).arg(z1).arg(z0).arg(result).arg(a).arg(b).arg(a*b); st<<sf; }
    return st;
}

/* ── MaxCircularSubarray ── */
QVector<AlgoStep> SimulatorePage::genMaxCircularSubarray(QVector<int> arr) {
    QVector<AlgoStep> st;
    int n=arr.size();
    /* mix neg/pos */
    for(int i=0;i<n;i++) if(i%2==0&&arr[i]>3) arr[i]-=arr[i]/2;
    { AlgoStep s; s.arr=arr; s.msg="Max Circular Subarray: Kadane normale E Kadane sul min subarray (caso wrap-around)"; st<<s; }
    /* kadane normale */
    int maxS=arr[0], cur=arr[0], total=arr[0];
    for(int i=1;i<n;i++){
        cur=qMax(arr[i], cur+arr[i]);
        maxS=qMax(maxS,cur); total+=arr[i];
        AlgoStep s; s.arr=arr; s.cmp<<i;
        s.msg=QString("Kadane normale: i=%1 arr[i]=%2 maxS=%3").arg(i).arg(arr[i]).arg(maxS); st<<s;
    }
    /* kadane sul minimo */
    int minS=arr[0]; cur=arr[0];
    for(int i=1;i<n;i++){
        cur=qMin(arr[i], cur+arr[i]);
        minS=qMin(minS,cur);
        AlgoStep s; s.arr=arr; s.swp<<i;
        s.msg=QString("Kadane minimo: i=%1 arr[i]=%2 minS=%3").arg(i).arg(arr[i]).arg(minS); st<<s;
    }
    int maxCircular=total-minS;
    int best=qMax(maxS, maxCircular);
    AlgoStep sf; sf.arr=arr;
    sf.msg=QString("Max Circular Subarray = max(lineare=%1, circolare=%2) = %3").arg(maxS).arg(maxCircular).arg(best); st<<sf;
    return st;
}

/* ── CountInversions ── */
QVector<AlgoStep> SimulatorePage::genCountInversions(QVector<int> arr) {
    QVector<AlgoStep> st;
    int n=arr.size(); int totalInv=0;
    { AlgoStep s; s.arr=arr; s.msg="Count Inversions con Merge Sort: conta coppie (i,j) con i<j ma a[i]>a[j]"; st<<s; }
    std::function<int(QVector<int>&,int,int)> mergeCount=[&](QVector<int>& a,int lo,int hi)->int{
        if(hi<=lo) return 0;
        int mid=(lo+hi)/2;
        int inv=mergeCount(a,lo,mid)+mergeCount(a,mid+1,hi);
        QVector<int> tmp;
        int i=lo, j=mid+1;
        while(i<=mid&&j<=hi){
            if(a[i]<=a[j]) tmp<<a[i++];
            else { inv+=mid-i+1; tmp<<a[j++];
                QVector<int> cmp_; for(int k=i;k<=mid;k++) cmp_<<k;
                AlgoStep s; s.arr=a; s.cmp=cmp_; s.swp<<j-1;
                s.msg=QString("a[%1]=%2 > a[%3]=%4: %5 inversioni (da pos %6..%7)")
                      .arg(j-1).arg(a[j-1]).arg(i).arg(a[i]).arg(mid-i+1).arg(i).arg(mid); st<<s; }
        }
        while(i<=mid) tmp<<a[i++];
        while(j<=hi) tmp<<a[j++];
        for(int k=lo;k<=hi;k++) a[k]=tmp[k-lo];
        totalInv+=inv;
        return inv;
    };
    mergeCount(arr,0,n-1);
    QVector<int> all; for(int i=0;i<n;i++) all<<i;
    AlgoStep sf; sf.arr=arr; sf.sorted=all; sf.msg=QString("Count Inversions: %1 inversioni totali").arg(totalInv); st<<sf;
    return st;
}

/* ── GameOfLife1D ── */
QVector<AlgoStep> SimulatorePage::genGameOfLife1D(QVector<int> arr) {
    QVector<AlgoStep> st;
    int n=qMin((int)arr.size(),12);
    QVector<int> cells(n);
    for(int i=0;i<n;i++) cells[i]=(arr[i]>arr.size()/2)?1:0;
    cells[n/2]=1; /* assicura almeno una cella viva */
    { AlgoStep s; s.arr=cells; s.msg=QString("Game of Life 1D: %1 celle. 1=viva 0=morta. Regola: nasce se esattamente 1 vicino vivo.").arg(n); st<<s; }
    for(int gen=0;gen<10;gen++){
        QVector<int> next(n,0);
        for(int i=1;i<n-1;i++){
            int alive=(cells[i-1]+cells[i+1]);
            next[i]=(alive==1)?1:0;
        }
        QVector<int> cmp_,srt_;
        for(int i=0;i<n;i++){
            if(next[i]!=cells[i]) cmp_<<i;
            if(next[i]) srt_<<i;
        }
        cells=next;
        AlgoStep s; s.arr=cells; s.cmp=cmp_; s.sorted=srt_;
        s.msg=QString("Generazione %1: %2 cellule vive").arg(gen+1).arg(srt_.size()); st<<s;
    }
    return st;
}

/* ── Rule30 ── */
QVector<AlgoStep> SimulatorePage::genRule30(QVector<int> arr) {
    QVector<AlgoStep> st;
    int n=qMin((int)arr.size(),12);
    QVector<int> cells(n,0); cells[n/2]=1;
    { AlgoStep s; s.arr=cells; s.msg="Rule 30 (Wolfram): automa cellulare deterministico che genera pseudo-casualità. Centro=1."; st<<s; }
    for(int gen=0;gen<12;gen++){
        QVector<int> next(n,0);
        for(int i=1;i<n-1;i++){
            int pattern=(cells[i-1]<<2)|(cells[i]<<1)|cells[i+1];
            /* Rule 30: 00011110 */
            next[i]=(30>>pattern)&1;
        }
        cells=next;
        QVector<int> srt_; for(int i=0;i<n;i++) if(cells[i]) srt_<<i;
        AlgoStep s; s.arr=cells; s.sorted=srt_;
        s.msg=QString("Gen %1: %2 celle attive (Rule 30 = pseudo-random)").arg(gen+1).arg(srt_.size()); st<<s;
    }
    return st;
}

/* ── SpiralMatrix ── */
QVector<AlgoStep> SimulatorePage::genSpiralMatrix(int n) {
    QVector<AlgoStep> st;
    n=qMin(n,4);
    QVector<int> mat(n*n,0);
    { AlgoStep s; s.arr=mat; s.msg=QString("Spiral Matrix %1x%1: riempi a spirale. Barre = ordine di visita.").arg(n); st<<s; }
    int top=0,bot=n-1,left=0,right=n-1,val=1;
    QVector<int> order;
    while(top<=bot&&left<=right){
        for(int c=left;c<=right;c++){ mat[top*n+c]=val++; order<<top*n+c;
            AlgoStep s; s.arr=mat; s.cmp<<top*n+c;
            s.msg=QString("Spirale: mat[%1][%2]=%3 (→)").arg(top).arg(c).arg(val-1); st<<s; }
        top++;
        for(int r=top;r<=bot;r++){ mat[r*n+right]=val++; order<<r*n+right;
            AlgoStep s; s.arr=mat; s.cmp<<r*n+right;
            s.msg=QString("Spirale: mat[%1][%2]=%3 (↓)").arg(r).arg(right).arg(val-1); st<<s; }
        right--;
        if(top<=bot){ for(int c=right;c>=left;c--){ mat[bot*n+c]=val++; order<<bot*n+c;
            AlgoStep s; s.arr=mat; s.cmp<<bot*n+c;
            s.msg=QString("Spirale: mat[%1][%2]=%3 (←)").arg(bot).arg(c).arg(val-1); st<<s; } bot--; }
        if(left<=right){ for(int r=bot;r>=top;r--){ mat[r*n+left]=val++; order<<r*n+left;
            AlgoStep s; s.arr=mat; s.cmp<<r*n+left;
            s.msg=QString("Spirale: mat[%1][%2]=%3 (↑)").arg(r).arg(left).arg(val-1); st<<s; } left++; }
    }
    AlgoStep sf; sf.arr=mat; sf.sorted=order; sf.msg=QString("Spiral Matrix %1x%1 completata! %2 celle.").arg(n).arg(n*n); st<<sf;
    return st;
}

/* ── SierpinskiRow ── */
QVector<AlgoStep> SimulatorePage::genSierpinskiRow(int n) {
    QVector<AlgoStep> st;
    n=qMin(n,10);
    { QVector<int> row={1};
      AlgoStep s; s.arr=row; s.msg="Triangolo di Sierpinski: riga n del triangolo di Pascal mod 2. Frattale binario."; st<<s; }
    for(int r=1;r<n;r++){
        /* calcola riga r del triangolo di Pascal mod 2 */
        QVector<int> row(r+1,0); row[0]=row[r]=1;
        for(int k=1;k<r;k++){
            /* C(r,k) mod 2 via regola di Lucas */
            int rr=r,kk=k,ok=1;
            while(rr>0||kk>0){ if((kk%2)>(rr%2)){ok=0;break;} rr/=2; kk/=2; }
            row[k]=ok;
        }
        QVector<int> srt; for(int k=0;k<=r;k++) if(row[k]) srt<<k;
        AlgoStep s; s.arr=row; s.sorted=srt;
        s.msg=QString("Riga %1: %2 bit = 1 (Sierpinski: 1 se C(%1,k) mod 2 ≠ 0)").arg(r).arg(srt.size()); st<<s;
    }
    return st;
}

