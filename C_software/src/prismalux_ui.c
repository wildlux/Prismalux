/*
 * prismalux_ui.c — Implementazione header dinamico e primitive UI
 */
#include "prismalux_ui.h"

#include <stdlib.h>
#include <stdarg.h>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#else
#  include <sys/statvfs.h>
#  include <time.h>
#endif

/* Modalità headless — se != 0, cpu_percent() salta nanosleep (usato dai test) */
int g_sim_headless = 0;

/* ══════════════════════════════════════════════════════════════
   Larghezza terminale
   ══════════════════════════════════════════════════════════════ */

int term_width(void) {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi))
        return csbi.srWindow.Right - csbi.srWindow.Left + 1;
    return 80;
#else
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
        return (int)ws.ws_col;
    return 80;
#endif
}

/* ══════════════════════════════════════════════════════════════
   Lettore risorse sistema (CPU / RAM / Swap / Disco / VRAM)
   ══════════════════════════════════════════════════════════════ */

#ifndef _WIN32
static void read_cpu_stat(unsigned long long *idle, unsigned long long *total) {
    FILE *f = fopen("/proc/stat","r"); if(!f){*idle=*total=0;return;}
    char line[256]; unsigned long long u,n,s,id,iow,irq,sirq,st=0;
    if(fgets(line,sizeof line,f))
        sscanf(line,"cpu %llu %llu %llu %llu %llu %llu %llu %llu",
               &u,&n,&s,&id,&iow,&irq,&sirq,&st);
    *idle=id+iow; *total=u+n+s+id+iow+irq+sirq+st;
    fclose(f);
}
static double cpu_percent(void) {
    if (g_sim_headless) return 0.0;  /* skip nanosleep in headless/test mode */
    unsigned long long i1,t1,i2,t2;
    read_cpu_stat(&i1,&t1);
    struct timespec ts={0,200000000L}; nanosleep(&ts,NULL);
    read_cpu_stat(&i2,&t2);
    unsigned long long dt=t2-t1, di=i2-i1;
    return (dt==0)?0.0: 100.0*(1.0-(double)di/(double)dt);
}
static void read_ram(double *u,double *t) {
    *u=*t=0; FILE *f=fopen("/proc/meminfo","r"); if(!f)return;
    char line[256]; unsigned long long tot=0,avail=0;
    while(fgets(line,sizeof line,f)){
        unsigned long long v;
        if(sscanf(line,"MemTotal: %llu",&v)==1) tot=v;
        else if(sscanf(line,"MemAvailable: %llu",&v)==1) avail=v;
    }
    fclose(f);
    *t=tot/1048576.0; *u=(tot-avail)/1048576.0;
}
static void read_swap(double *u,double *t) {
    *u=*t=0; FILE *f=fopen("/proc/meminfo","r"); if(!f)return;
    char line[256]; unsigned long long tot=0,free=0;
    while(fgets(line,sizeof line,f)){
        unsigned long long v;
        if(sscanf(line,"SwapTotal: %llu",&v)==1) tot=v;
        else if(sscanf(line,"SwapFree: %llu",&v)==1) free=v;
    }
    fclose(f);
    *t=tot/1048576.0; *u=(tot-free)/1048576.0;
}
static void read_disk(double *u,double *t) {
    *u=*t=0; struct statvfs sv;
    if(statvfs("/",&sv)!=0)return;
    unsigned long long blk=(unsigned long long)sv.f_frsize;
    unsigned long long tot=(unsigned long long)sv.f_blocks*blk;
    unsigned long long avail=(unsigned long long)sv.f_bfree*blk;
    *t=tot/1073741824.0; *u=(tot-avail)/1073741824.0;
}
static void read_cpu_name(char *out,size_t n) {
    strncpy(out,"CPU?",n-1); out[n-1]='\0';
    FILE *f=fopen("/proc/cpuinfo","r"); if(!f)return;
    char line[512];
    while(fgets(line,sizeof line,f)){
        if(strncmp(line,"model name",10)!=0)continue;
        char *p=strchr(line,':'); if(!p)break; p++;
        while(*p==' ')p++;
        char *nl=strchr(p,'\n'); if(nl)*nl='\0';
        const char *pats[]={"i3-","i5-","i7-","i9-","Ryzen","Xeon","Atom",NULL};
        for(int i=0;pats[i];i++){
            char *found=strstr(p,pats[i]);
            if(found){ strncpy(out,found,n-1); out[n-1]='\0';
                for(int j=(int)strlen(out)-1;j>=0&&out[j]==' ';j--)out[j]='\0';
                fclose(f); return; }
        }
        strncpy(out,p,n-1); out[n-1]='\0'; break;
    }
    fclose(f);
}
#else /* _WIN32 */
static double cpu_percent(void) {
    if (g_sim_headless) return 0.0;  /* skip Sleep in headless/test mode */
    FILETIME i1,k1,u1,i2,k2,u2;
    GetSystemTimes(&i1,&k1,&u1); Sleep(200); GetSystemTimes(&i2,&k2,&u2);
    ULARGE_INTEGER ui;
#define FT2U(ft) (ui.LowPart=(ft).dwLowDateTime,ui.HighPart=(ft).dwHighDateTime,ui.QuadPart)
    ULONGLONG di=FT2U(i2)-FT2U(i1),dk=FT2U(k2)-FT2U(k1),du=FT2U(u2)-FT2U(u1);
#undef FT2U
    ULONGLONG dt=dk+du; return dt?100.0*(1.0-(double)di/(double)dt):0.0;
}
static void read_ram(double *u,double *t) {
    MEMORYSTATUSEX ms; ms.dwLength=sizeof ms; GlobalMemoryStatusEx(&ms);
    *t=ms.ullTotalPhys/1073741824.0; *u=(*t)-(double)ms.ullAvailPhys/1073741824.0;
}
static void read_swap(double *u,double *t) {
    MEMORYSTATUSEX ms; ms.dwLength=sizeof ms; GlobalMemoryStatusEx(&ms);
    *t=(double)ms.ullTotalPageFile/1073741824.0;
    *u=*t-(double)ms.ullAvailPageFile/1073741824.0;
}
static void read_disk(double *u,double *t) {
    ULARGE_INTEGER fr,to,tf;
    GetDiskFreeSpaceExA("C:\\",&fr,&to,&tf);
    *t=to.QuadPart/1073741824.0; *u=(*t)-(double)fr.QuadPart/1073741824.0;
}
static void read_cpu_name(char *out,size_t n) {
    strncpy(out,"CPU Windows",n-1); out[n-1]='\0';
}
#endif /* _WIN32 */

/* VRAM usata/totale in MB per la GPU indicata (lettura live) */
void read_vram_mb(int gpu_index, DevType gpu_type,
                         long long *used_out, long long *total_out) {
    *used_out = *total_out = 0;
#ifdef _WIN32
    if (gpu_type == DEV_NVIDIA) {
        char cmd[256];
        snprintf(cmd,sizeof cmd,
            "nvidia-smi --id=%d --query-gpu=memory.used,memory.total"
            " --format=csv,noheader,nounits 2>nul", gpu_index);
        FILE* f = popen(cmd,"r");
        if (f) { long long u=0,t=0;
                 { int _r = fscanf(f,"%lld, %lld",&u,&t); (void)_r; }
                 pclose(f); *used_out=u; *total_out=t; }
    }
#else
    if (gpu_type == DEV_NVIDIA) {
        char cmd[256];
        snprintf(cmd,sizeof cmd,
            "nvidia-smi --id=%d --query-gpu=memory.used,memory.total"
            " --format=csv,noheader,nounits 2>/dev/null", gpu_index);
        FILE* f = popen(cmd,"r");
        if (f) { long long u=0,t=0;
                 { int _r = fscanf(f,"%lld, %lld",&u,&t); (void)_r; }
                 pclose(f); *used_out=u; *total_out=t; }
    } else if (gpu_type == DEV_AMD || gpu_type == DEV_INTEL) {
        /* AMD e Intel Arc usano gli stessi path DRM per VRAM */
        char path[256];
        unsigned long long total=0, used_b=0;
        snprintf(path,sizeof path,
            "/sys/class/drm/card%d/device/mem_info_vram_total",gpu_index);
        FILE* ft = fopen(path,"r");
        if (ft) { { int _r = fscanf(ft,"%llu",&total); (void)_r; } fclose(ft); }
        snprintf(path,sizeof path,
            "/sys/class/drm/card%d/device/mem_info_vram_used",gpu_index);
        FILE* fu = fopen(path,"r");
        if (fu) { { int _r = fscanf(fu,"%llu",&used_b); (void)_r; } fclose(fu); }
        *total_out = (long long)(total  / (1024ULL*1024ULL));
        *used_out  = (long long)(used_b / (1024ULL*1024ULL));
    }
#endif
}

SysRes get_resources(void) {
    SysRes r; memset(&r,0,sizeof r);
    r.cpu_pct=cpu_percent();
    read_ram (&r.ram_used, &r.ram_total);
    read_swap(&r.swp_used, &r.swp_total);
    read_disk(&r.dsk_used, &r.dsk_total);
    read_cpu_name(r.cpu_name, sizeof r.cpu_name);
    /* VRAM live dalla GPU primaria */
    if (g_hw_ready) {
        const HWDevice* pd = &g_hw.dev[g_hw.primary];
        if (pd->type != DEV_CPU) {
            long long vu=0, vt=0;
            read_vram_mb(pd->gpu_index, pd->type, &vu, &vt);
            r.vram_used  = (double)vu / 1024.0;
            r.vram_total = (double)vt / 1024.0;
            strncpy(r.vram_name, pd->name, sizeof r.vram_name - 1);
        }
    }
    return r;
}

/* ══════════════════════════════════════════════════════════════
   Primitive UI: disp_len, barre, box
   ══════════════════════════════════════════════════════════════ */

int disp_len(const char *s) {
    int n = 0;
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        if (*p == '\033') {          /* sequenza ANSI: salta fino a 'm' */
            p++;
            while (*p && *p != 'm') p++;
            if (*p) p++;
            continue;
        }
        unsigned long cp = 0;
        int bytes = 1;
        if      ((*p & 0x80) == 0x00) { cp = *p;        bytes = 1; }
        else if ((*p & 0xE0) == 0xC0) { cp = *p & 0x1F; bytes = 2; }
        else if ((*p & 0xF0) == 0xE0) { cp = *p & 0x0F; bytes = 3; }
        else if ((*p & 0xF8) == 0xF0) { cp = *p & 0x07; bytes = 4; }
        for (int i = 1; i < bytes && p[i]; i++)
            cp = (cp << 6) | (p[i] & 0x3F);
        p += bytes;
        /* U+FE00–U+FE0F: variation selectors — zero width */
        if (cp >= 0xFE00 && cp <= 0xFE0F) continue;
        /* U+20E3: combining enclosing keycap — non aggiunge larghezza extra
         * (il digit base già conta 1; il keycap porta il totale a 2) */
        if (cp == 0x20E3) { n += 1; continue; }
        n += (cp >= 0x1F000) ? 2 : 1;
    }
    return n;
}

void print_bar(double pct, int len) {
    if(pct<0) pct=0;
    if(pct>100) pct=100;
    int f=(int)(pct/100.0*len);
    const char *col=(pct<70)?GRN:(pct<90)?YLW:RED;
    printf("%s",col);
    for(int i=0;i<len;i++) printf("%s",(i<f)?BAR_FULL:BAR_EMPTY);
    printf(RST);
}

void box_top(int W){ printf(CYN "╔"); for(int i=0;i<W;i++)printf("═"); printf("╗\n" RST); }
void box_bot(int W){ printf(CYN "╚"); for(int i=0;i<W;i++)printf("═"); printf("╝\n" RST); }
void box_sep(int W){ printf(CYN "╠"); for(int i=0;i<W;i++)printf("═"); printf("╣\n" RST); }

void box_ctr(int W, const char *ansi_text) {
    int dlen = disp_len(ansi_text);
    int lp=(W-dlen)/2; if(lp<0)lp=0;
    int rp=W-dlen-lp;  if(rp<0)rp=0;
    printf(CYN "║" RST);
    for(int i=0;i<lp;i++)putchar(' ');
    printf("%s",ansi_text);
    for(int i=0;i<rp;i++)putchar(' ');
    printf(CYN "║\n" RST);
}

void box_lft(int W, const char *text) {
    int dlen = disp_len(text);
    int pad  = W - dlen;
    if(pad<0)pad=0;
    printf(CYN "║" RST "%s", text);
    for(int i=0;i<pad;i++)putchar(' ');
    printf(CYN "║\n" RST);
}

void box_emp(int W){
    printf(CYN "║" RST);
    for(int i=0;i<W;i++)putchar(' ');
    printf(CYN "║\n" RST);
}

void box_res(int W, const char *lbl4, const char *info,
             double pct, const char *warn) {
    char i20[21]; snprintf(i20,sizeof i20,"%-20s",info);
    int bar=W-39; if(bar<12)bar=12; if(bar>40)bar=40;
    int wlen=(warn&&warn[0])?disp_len(warn)+1:0;
    int pad=W-37-bar-wlen; if(pad<0)pad=0;
    printf(CYN "║" RST " %-4s  %s  ",lbl4,i20);
    print_bar(pct,bar);
    printf("  %5.1f%%",pct);
    if(warn&&warn[0]) printf(" %s",warn);
    for(int i=0;i<pad;i++)putchar(' ');
    printf(CYN "║\n" RST);
}

const char *mem_warn(double pct) {
    if(pct>=95) return RED BLD "\xe2\x9b\x94 CRIT" RST;  /* ⛔ */
    if(pct>=85) return YLW     "\xe2\x9a\xa0  ALTA" RST;  /* ⚠  */
    return "";
}

/* ──────────────────────────────────────────────────────────────
   Helper interni per il nuovo header compatto a 3 colonne
   ────────────────────────────────────────────────────────────── */

/* Barra colorata come stringa ANSI (len blocchi da 1 display col ciascuno) */
static void bar_to_str(char *buf, int sz, double pct, int len) {
    if(pct<0) pct=0;
    if(pct>100) pct=100;
    int f=(int)(pct/100.0*len);
    const char *col=(pct<70)?GRN:(pct<90)?YLW:RED;
    char *p = buf + snprintf(buf, sz, "%s", col);
    for(int i=0; i<len && (buf+sz-p)>4; i++, p+=3)
        memcpy(p, (i<f)?BAR_FULL:BAR_EMPTY, 3);
    *p='\0';
    int rem=(int)(sz-(p-buf));
    if(rem>0) strncat(buf, RST, (size_t)(rem-1));
}

/*
 * Riga a 3 colonne dentro il box:  ║ left [pad] center [pad] right ║
 *   left   — allineato a sinistra
 *   center — centrato sull'asse del box
 *   right  — allineato a destra
 * ldw/cdw/rdw = display width (senza sequenze ANSI)
 */
static void hdr_3col(int W,
                     const char *left,   int ldw,
                     const char *center, int cdw,
                     const char *right,  int rdw) {
    int c_pos = (W - cdw) / 2;               /* posizione di partenza del centro */
    int r_pos = W - rdw;                      /* posizione di partenza del destro */
    if(c_pos < ldw + 1) c_pos = ldw + 1;    /* evita sovrapposizione left/center */
    if(r_pos < c_pos + cdw + 1) r_pos = c_pos + cdw + 1; /* center/right */
    if(r_pos + rdw > W) r_pos = W - rdw;    /* non uscire dal box */

    printf(CYN "\xe2\x95\x91" RST);          /* ║ */
    printf("%s", left);
    int col = ldw;
    while(col < c_pos) { putchar(' '); col++; }
    printf("%s", center); col += cdw;
    while(col < r_pos)  { putchar(' '); col++; }
    printf("%s", right);  col += rdw;
    while(col < W)       { putchar(' '); col++; }
    printf(CYN "\xe2\x95\x91\n" RST);        /* ║\n */
}

/* ══════════════════════════════════════════════════════════════
   print_header — layout compatto a 3 colonne (hardware live)
   ╔══════════════════════════════════════════════════════════╗
   ║ Ollama │ model   🍺 P R I S M A L U X v2.1 🍺  CPU ██  42.5% ║
   ║                                                  RAM ██  51.3% ║
   ║                                                  DSK ██  24.0% ║
   [╠══╣ ⛔ RISORSE CRITICHE — libera RAM/VRAM         (se >92%)   ]
   ╚══════════════════════════════════════════════════════════╝
   ══════════════════════════════════════════════════════════════ */

void print_header(void) {
    SysRes r   = get_resources();
    int tw     = term_width();
    int W      = tw - 2;
    if(W < 60)  W = 60;
    if(W > 130) W = 130;

    double rp = (r.ram_total >0)?r.ram_used /r.ram_total *100:0;
    double sp = (r.swp_total >0)?r.swp_used /r.swp_total *100:0;
    double dp = (r.dsk_total >0)?r.dsk_used /r.dsk_total *100:0;
    double vp = (r.vram_total>0)?r.vram_used/r.vram_total*100:0;
    int crit  = (rp>=92) || (vp>=92 && r.vram_total>0.01);
    int alta  = !crit && ((rp>=80) || (vp>=80 && r.vram_total>0.01));

    /* ── Titolo centrato ─────────────────────────────────────── */
    char title_buf[128];
    snprintf(title_buf, sizeof title_buf,
             BLD CYN "\xf0\x9f\x8d\xba  P R I S M A L U X  v" VERSION
             "  \xf0\x9f\x8d\xba" RST);
    int title_dw = disp_len(title_buf);   /* ~31 display cols */

    /* ── Sinistra: backend │ modello ─────────────────────────── */
    char left_buf[160];
    left_buf[0] = '\0';
    if(g_hdr_backend[0]) {
        const char *md = g_hdr_model[0] ? g_hdr_model : "\xe2\x80\x94"; /* — */
        snprintf(left_buf, sizeof left_buf,
                 " " CYN "%s" RST " \xe2\x94\x82 " DIM "%.38s" RST,
                 g_hdr_backend, md);
    }
    int left_dw = disp_len(left_buf);

    /* ── Colonna destra: righe risorse ───────────────────────── */
    /* Formato: " LBL [bar10]  42.5%"  →  display = 1+4+1+10+2+6 = 24 cols */
#define MAX_RR 6
    char rrow[MAX_RR][192];
    int  n_res = 0;

#define ADD_RR(lbl4) do { \
    char _b[96]; bar_to_str(_b, sizeof _b, _pct, 10); \
    snprintf(rrow[n_res++], 192, " " BLD lbl4 RST " %s  " YLW "%5.1f%%" RST, _b, _pct); \
} while(0)

    { double _pct = r.cpu_pct;                               ADD_RR("CPU "); }
    { double _pct = rp;                                      ADD_RR("RAM "); }
    if(r.swp_total > 0.01) { double _pct = sp;               ADD_RR("SWP "); }
    if(r.vram_total > 0.01) {
        char _lbl[8];
        snprintf(_lbl, sizeof _lbl, "%-4.4s",
                 hw_dev_type_str(g_hw_ready?g_hw.dev[g_hw.primary].type:DEV_CPU));
        double _pct = vp;
        char _b[96]; bar_to_str(_b, sizeof _b, _pct, 10);
        snprintf(rrow[n_res++], 192,
                 " " BLD "%s" RST " %s  " YLW "%5.1f%%" RST, _lbl, _b, _pct);
    } else if(g_hw_ready && g_hw.dev[g_hw.primary].type!=DEV_CPU) {
        /* GPU presente ma VRAM non leggibile: mostra solo label a 0% */
        const char *_lbl = hw_dev_type_str(g_hw.dev[g_hw.primary].type);
        double _pct = 0.0;
        char _b[96]; bar_to_str(_b, sizeof _b, _pct, 10);
        snprintf(rrow[n_res++], 192,
                 " " BLD "%-4.4s" RST " %s  " DIM "  N/D  " RST, _lbl, _b);
    }
    { double _pct = dp;                                      ADD_RR("DSK "); }
#undef ADD_RR
#undef MAX_RR
    int rrdw = 24; /* display width fissa per ogni riga risorsa */

    /* ── Disegno box ─────────────────────────────────────────── */
    box_top(W);
    for(int i=0; i<n_res; i++) {
        const char *lft = (i==0) ? left_buf : "";
        int          ldw = (i==0) ? left_dw  : 0;
        const char *ctr = (i==0) ? title_buf : "";
        int          cdw = (i==0) ? title_dw  : 0;
        hdr_3col(W, lft, ldw, ctr, cdw, rrow[i], rrdw);
    }

    /* ── Banner allerta ──────────────────────────────────────── */
    if(crit) {
        box_sep(W);
        box_lft(W, RED BLD " \xe2\x9b\x94  RISORSE CRITICHE \xe2\x80\x94 "
                "libera RAM/VRAM prima di avviare modelli AI" RST);
    } else if(alta) {
        box_sep(W);
        box_lft(W, YLW " \xe2\x9a\xa0  Risorse alte \xe2\x80\x94 "
                "il caricamento del modello potrebbe essere lento" RST);
    }
    box_bot(W);
    printf("\n");
}

/* ══════════════════════════════════════════════════════════════
   input_line — prompt ─── ❯ ─── a singola riga espandibile
   ══════════════════════════════════════════════════════════════ */

int input_line(const char *label, char *buf, int sz) {
    int W = term_width();
    if(W < 4) W = 4;

    /* ── riga separatore superiore ── */
    printf("\n ");
    for(int i = 1; i < W - 1; i++) printf("\xe2\x94\x80");  /* ─ */
    printf("\n \xe2\x9d\xaf ");  /* ❯ */
    if(label && label[0]) printf("%s", label);
    fflush(stdout);

    if(!fgets(buf, sz, stdin)) { buf[0] = '\0'; return 0; }
    buf[buf[0] ? (int)(strcspn(buf,"\n")) : 0] = '\0';

    /* ── riga separatore inferiore ── */
    printf(" ");
    for(int i = 1; i < W - 1; i++) printf("\xe2\x94\x80");
    printf("\n");

    return 1;
}
