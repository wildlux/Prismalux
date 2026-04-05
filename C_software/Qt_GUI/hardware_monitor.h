#pragma once
#include <QObject>
#include <QThread>
#include <QTimer>

/* Includi il backend C per il rilevamento GPU */
extern "C" {
#include "hw_detect.h"
}

/* ══════════════════════════════════════════════════════════════
   Snapshot risorse aggiornato ogni poll
   ══════════════════════════════════════════════════════════════ */
struct SysSnapshot {
    double cpu_pct   = 0.0;
    double ram_used  = 0.0;   /* GiB */
    double ram_total = 0.0;   /* GiB */
    double gpu_pct   = 0.0;   /* % VRAM usata (0 se non leggibile) */
    double vram_used = 0.0;   /* GiB */
    double vram_total= 0.0;   /* GiB */
    QString gpu_name;
    QString cpu_name;
    bool    gpu_ready = false;
};

/* ══════════════════════════════════════════════════════════════
   Thread che esegue hw_detect() una volta all'avvio (operazione
   lenta: chiama popen/nvidia-smi/wmic) e poi segnala completamento
   ══════════════════════════════════════════════════════════════ */
class HWDetectThread : public QThread {
    Q_OBJECT
public:
    explicit HWDetectThread(QObject* parent = nullptr);
signals:
    void detected(HWInfo hw);
protected:
    void run() override;
};

/* ══════════════════════════════════════════════════════════════
   HardwareMonitor — polling CPU/RAM ogni 2s, GPU più lento
   ══════════════════════════════════════════════════════════════ */
class HardwareMonitor : public QObject {
    Q_OBJECT
public:
    explicit HardwareMonitor(QObject* parent = nullptr);
    ~HardwareMonitor();

    void start();
    const HWInfo& hwInfo() const { return m_hw; }
    bool  hwReady()        const { return m_hwReady; }

signals:
    /* Emesso ogni 2s con i dati aggiornati */
    void updated(SysSnapshot snap);
    /* Emesso una volta quando hw_detect() completa */
    void hwInfoReady(HWInfo hw);

private slots:
    void onTimer();
    void onHWDetected(HWInfo hw);

private:
    SysSnapshot readSnapshot() const;

    QTimer*        m_timer   = nullptr;
    HWDetectThread* m_thread = nullptr;
    HWInfo         m_hw      = {};
    bool           m_hwReady = false;
};
