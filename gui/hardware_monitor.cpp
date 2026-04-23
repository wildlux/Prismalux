#include "hardware_monitor.h"
#include <QThread>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
/* popen/_popen: MinGW li espone come popen, MSVC come _popen */
#  ifndef popen
#    define popen  _popen
#    define pclose _pclose
#  endif
#else
#  include <unistd.h>
#  include <sys/statvfs.h>
#  include <time.h>
#endif

/* ── Lettura CPU cross-platform ─────────────────────────────── */
#ifdef _WIN32
static double read_cpu_pct() {
    FILETIME i1,k1,u1,i2,k2,u2;
    GetSystemTimes(&i1,&k1,&u1);
    Sleep(150);
    GetSystemTimes(&i2,&k2,&u2);
    auto ft2u = [](FILETIME ft) -> quint64 {
        return ((quint64)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    };
    quint64 di = ft2u(i2)-ft2u(i1);
    quint64 dk = ft2u(k2)-ft2u(k1);
    quint64 du = ft2u(u2)-ft2u(u1);
    quint64 dt = dk+du;
    return dt ? 100.0*(1.0-(double)di/(double)dt) : 0.0;
}
static void read_ram(double* used, double* total) {
    MEMORYSTATUSEX ms; ms.dwLength = sizeof ms;
    GlobalMemoryStatusEx(&ms);
    *total = (double)ms.ullTotalPhys / 1073741824.0;
    *used  = *total - (double)ms.ullAvailPhys / 1073741824.0;
}
#else
static double read_cpu_pct() {
    auto readStat = [](unsigned long long* idle, unsigned long long* tot) {
        FILE* f = fopen("/proc/stat","r");
        if (!f) { *idle=*tot=0; return; }
        unsigned long long u,n,s,id,iow,irq,sirq,st=0;
        fscanf(f,"cpu %llu %llu %llu %llu %llu %llu %llu %llu",
               &u,&n,&s,&id,&iow,&irq,&sirq,&st);
        fclose(f);
        *idle = id+iow;
        *tot  = u+n+s+id+iow+irq+sirq+st;
    };
    unsigned long long i1,t1,i2,t2;
    readStat(&i1,&t1);
    struct timespec ts={0,150000000L}; nanosleep(&ts,NULL);
    readStat(&i2,&t2);
    unsigned long long dt=t2-t1, di=i2-i1;
    return (dt==0)?0.0 : 100.0*(1.0-(double)di/(double)dt);
}
static void read_ram(double* used, double* total) {
    *used=*total=0;
    FILE* f = fopen("/proc/meminfo","r"); if(!f) return;
    char line[256];
    unsigned long long tot=0,avail=0;
    while(fgets(line,sizeof line,f)){
        unsigned long long v;
        if(sscanf(line,"MemTotal: %llu",&v)==1)     tot=v;
        else if(sscanf(line,"MemAvailable: %llu",&v)==1) avail=v;
    }
    fclose(f);
    *total = tot/1048576.0;
    *used  = (tot-avail)/1048576.0;
}
#endif

/* ── Lettura VRAM live NVIDIA ────────────────────────────────── */
static void read_vram_nvidia(int idx, double* used, double* total) {
    *used=*total=0;
    char cmd[256];
#ifdef _WIN32
    snprintf(cmd,sizeof cmd,
        "nvidia-smi --id=%d --query-gpu=memory.used,memory.total"
        " --format=csv,noheader,nounits 2>nul", idx);
#else
    snprintf(cmd,sizeof cmd,
        "nvidia-smi --id=%d --query-gpu=memory.used,memory.total"
        " --format=csv,noheader,nounits 2>/dev/null", idx);
#endif
    FILE* f = popen(cmd,"r");
    if(!f) return;
    long long u=0,t=0;
    fscanf(f,"%lld, %lld",&u,&t);
    pclose(f);
    *used  = u/1024.0;
    *total = t/1024.0;
}

/* ── HWDetectThread ──────────────────────────────────────────── */
HWDetectThread::HWDetectThread(QObject* parent)
    : QThread(parent) {}

void HWDetectThread::run() {
    HWInfo hw;
    hw_detect(&hw);
    emit detected(hw);
}

/* ── HardwareMonitor ─────────────────────────────────────────── */
HardwareMonitor::HardwareMonitor(QObject* parent)
    : QObject(parent)
{
    m_timer  = new QTimer(this);
    m_thread = new HWDetectThread(this);

    connect(m_timer,  &QTimer::timeout,          this, &HardwareMonitor::onTimer);
    connect(m_thread, &HWDetectThread::detected, this, &HardwareMonitor::onHWDetected);
}

HardwareMonitor::~HardwareMonitor() {
    m_timer->stop();
    if(m_thread->isRunning()) { m_thread->quit(); m_thread->wait(2000); }
}

void HardwareMonitor::start() {
    /* hw_detect() in background (chiama popen → lento) */
    m_thread->start();
    /* Polling risorse ogni 2s */
    m_timer->start(2000);
    /* Prima lettura immediata (senza GPU — hw non ancora pronta) */
    onTimer();
}

void HardwareMonitor::onHWDetected(HWInfo hw) {
    m_hw      = hw;
    m_hwReady = true;
    emit hwInfoReady(hw);
    /* Aggiorna subito con le info GPU */
    onTimer();
}

void HardwareMonitor::onTimer() {
    emit updated(readSnapshot());
}

SysSnapshot HardwareMonitor::readSnapshot() const {
    SysSnapshot s;

    /* CPU */
    s.cpu_pct = read_cpu_pct();

    /* RAM */
    read_ram(&s.ram_used, &s.ram_total);

    /* GPU (solo se hw_detect ha già girato) */
    if (m_hwReady) {
        const HWDevice* pd = &m_hw.dev[m_hw.primary];
        s.gpu_name = QString::fromLocal8Bit(pd->name);
        if (pd->type == DEV_NVIDIA) {
            read_vram_nvidia(pd->gpu_index, &s.vram_used, &s.vram_total);
            s.gpu_pct  = (s.vram_total > 0)
                         ? s.vram_used/s.vram_total*100.0 : 0.0;
        }
        /* AMD/Intel: lasciamo gpu_pct=0, mostriamo solo il nome */
        s.gpu_ready = true;
    }

    /* CPU name */
    if (m_hwReady)
        s.cpu_name = QString::fromLocal8Bit(m_hw.dev[0].name);

    return s;
}
