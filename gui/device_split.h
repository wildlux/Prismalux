#pragma once
#include <QString>
#include <QVector>
#include <QtGlobal>

/* ══════════════════════════════════════════════════════════════
   DeviceSplit — ripartizione layer tra più GPU + CPU

   D-54. Non esiste (e non è mai esistito) un modulo "agent_scheduler":
   la ripartizione è un calcolo puro, testabile senza hardware, che
   produce gli argomenti di llama-server:

     -ngl N
     --tensor-split p1,p2,...
     --split-mode layer

   I layer non assegnati alle GPU restano sulla CPU: è il normale
   comportamento di llama.cpp quando -ngl < numero di layer totali.

   Limite reale: --tensor-split segue l'ordine dei dispositivi
   enumerate da llama.cpp, non da HWInfo. Il dialog mostra l'output di
   `llama-server --list-devices` per permettere all'utente di
   verificare l'ordine. Mischiare NVIDIA e iGPU Intel/AMD richiede
   una build llama.cpp con entrambi i backend: il piano lo segnala
   come warning, non lo impedisce.
   ══════════════════════════════════════════════════════════════ */
namespace DeviceSplit {

constexpr qint64 kMbPerLayer  = 80;    ///< stima usata anche da hw_detect.c
constexpr qint64 kMinBudgetMb = 2048;  ///< sotto 2 GB il device non conviene
constexpr int    kMaxLayers   = 99;    ///< cap di llama.cpp per -ngl

struct Device {
    QString name;
    int     type = 0;          ///< HWDevType: 1=CPU, 2=NVIDIA, 3=AMD, 4=Intel, 5=Apple
    bool    isCpu = false;
    qint64  availMb = 0;
    qint64  budgetMb = 0;
    bool    enabled = false;
};

struct Plan {
    bool    enabled = false;
    int     nGpuLayers = 0;
    QString tensorSplit;       ///< "0.6668,0.3332" (vuoto con una sola GPU)
    QString summary;           ///< riga leggibile per status bar/banner
    QString warning;           ///< dispositivi eterogenei / budget insufficiente
    QVector<int> usedDeviceIndices;
};

/** Calcola il piano dai dispositivi selezionati. Non tocca QSettings. */
Plan compute(const QVector<Device>& devices, int maxLayers = kMaxLayers);

/** Persistenza (QSettings, chiavi P::SK::kLlamaSplit*). */
void   save(const Plan& plan, const QVector<Device>& devices);
Plan   load();
qint64 savedBudget(const QString& deviceName, qint64 fallbackMb);
void   clear();

} // namespace DeviceSplit
