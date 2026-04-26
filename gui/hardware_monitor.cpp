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

/* ── Lettura frequenza Intel iGPU da sysfs ───────────────────── */
/* gt_cur_freq_mhz / gt_max_freq_mhz esistono solo sul driver i915.
 * Su kernel ≥6 il path è gt/gt0/cur_freq_mhz.
 * Ritorna true se trovata almeno una coppia di file. */
#ifndef _WIN32
static bool read_igpu_freq(double* freq_pct) {
    *freq_pct = 0.0;
    char cur_p[256], max_p[256];
    for (int i = 0; i < 4; i++) {
        /* Kernel 5.x */
        snprintf(cur_p, sizeof cur_p, "/sys/class/drm/card%d/gt_cur_freq_mhz", i);
        snprintf(max_p, sizeof max_p, "/sys/class/drm/card%d/gt_max_freq_mhz", i);
        FILE* fc = fopen(cur_p, "r");
        if (!fc) {
            /* Kernel 6.x */
            snprintf(cur_p, sizeof cur_p,
                     "/sys/class/drm/card%d/gt/gt0/cur_freq_mhz", i);
            snprintf(max_p, sizeof max_p,
                     "/sys/class/drm/card%d/gt/gt0/rps_max_freq_mhz", i);
            fc = fopen(cur_p, "r");
        }
        if (!fc) continue;
        unsigned int cur = 0, max = 1;
        fscanf(fc, "%u", &cur);
        fclose(fc);
        FILE* fm = fopen(max_p, "r");
        if (fm) { fscanf(fm, "%u", &max); fclose(fm); }
        if (max > 0) *freq_pct = (double)cur / (double)max * 100.0;
        return true;
    }
    return false;
}
#endif

/* ── Lettura VRAM + compute utilization NVIDIA ───────────────── */
static void read_vram_nvidia(int idx, double* used, double* total, double* util_pct) {
    *used = *total = *util_pct = 0;
    char cmd[256];
#ifdef _WIN32
    snprintf(cmd, sizeof cmd,
        "nvidia-smi --id=%d --query-gpu=memory.used,memory.total,utilization.gpu"
        " --format=csv,noheader,nounits 2>nul", idx);
#else
    snprintf(cmd, sizeof cmd,
        "nvidia-smi --id=%d --query-gpu=memory.used,memory.total,utilization.gpu"
        " --format=csv,noheader,nounits 2>/dev/null", idx);
#endif
    FILE* f = popen(cmd, "r");
    if (!f) return;
    long long u = 0, t = 0, util = 0;
    fscanf(f, "%lld, %lld, %lld", &u, &t, &util);
    pclose(f);
    *used     = u    / 1024.0;
    *total    = t    / 1024.0;
    *util_pct = (double)util;
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

    s.cpu_pct = read_cpu_pct();
    read_ram(&s.ram_used, &s.ram_total);

    if (!m_hwReady) return s;

    /* CPU name dal device primario */
    s.cpu_name = QString::fromLocal8Bit(m_hw.dev[m_hw.primary].name);

    /* GPU: usa il device SECONDARIO (la vera GPU, non la CPU).
     * m_hw.primary = CPU, m_hw.secondary = miglior GPU rilevata.
     * Bug precedente: leggeva da m_hw.primary → gpu_pct sempre 0. */
    if (m_hw.secondary >= 0) {
        const HWDevice& gpu = m_hw.dev[m_hw.secondary];
        s.gpu_name = QString::fromLocal8Bit(gpu.name);

        if (gpu.type == DEV_NVIDIA) {
            read_vram_nvidia(gpu.gpu_index,
                             &s.vram_used, &s.vram_total, &s.gpu_pct);
            s.vram_pct = (s.vram_total > 0)
                         ? s.vram_used / s.vram_total * 100.0 : 0.0;
        }
        /* AMD: lettura da sysfs — /sys/class/drm/card[N]/device/mem_info_vram_used */
        else if (gpu.type == DEV_AMD) {
            FILE* fu = fopen("/sys/class/drm/card0/device/mem_info_vram_used", "r");
            FILE* ft = fopen("/sys/class/drm/card0/device/mem_info_vram_total", "r");
            if (fu && ft) {
                unsigned long long vu = 0, vt = 0;
                fscanf(fu, "%llu", &vu);
                fscanf(ft, "%llu", &vt);
                s.vram_used  = vu / 1073741824.0;
                s.vram_total = vt / 1073741824.0;
                s.vram_pct   = (vt > 0) ? (double)vu / (double)vt * 100.0 : 0.0;
                s.gpu_pct    = s.vram_pct;   /* proxy: VRAM% come attività */
            }
            if (fu) fclose(fu);
            if (ft) fclose(ft);
        }
        s.gpu_ready = true;
    }

#ifndef _WIN32
    /* Intel iGPU — frequenza come proxy utilizzo */
    if (m_hw.igpu >= 0) {
        s.igpu_name = QString::fromLocal8Bit(m_hw.dev[m_hw.igpu].name);
        double fp = 0.0;
        if (read_igpu_freq(&fp)) {
            s.igpu_freq_pct = fp;
            s.igpu_ready    = true;
        }
    }
#endif

    return s;
}
