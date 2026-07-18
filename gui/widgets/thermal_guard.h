#pragma once
/* ══════════════════════════════════════════════════════════════
   ThermalGuard — sentinella termica condivisa (D-67).

   MainWindow::onHWUpdated() la alimenta ogni 2s con le temperature del
   HardwareMonitor; i flussi pesanti (indicizzazione RAG, pipeline agenti,
   Agente Autonomo, Multi-Agente) la interrogano PRIMA di lanciare il
   prossimo passo e, se il sistema scotta, rimandano con un retry.

   Uso tipico in un punto di concatenamento (tra un passo e il successivo,
   mai a interrompere una generazione già in corso):

     if (ThermalGuard::s().isHot()) {
         setStatus(ThermalGuard::pauseMessage(ThermalGuard::s().maxTempC()));
         QTimer::singleShot(ThermalGuard::kRetryMs, this, &Foo::riprovaStep);
         return;
     }

   Isteresi: hot da >= kHotC (82°C), torna fresco solo sotto kCoolC (75°C)
   — evita oscillazioni on/off al limite della soglia. Sensori assenti
   (temp -1) → mai hot.

   Header-only con Q_OBJECT: elencato in CPP_SRCS (AUTOMOC), come
   collapsible_section.h. Singleton con parent nullptr (pattern
   ThemeManager — MAI qApp come parent: ABRT allo shutdown).
   ══════════════════════════════════════════════════════════════ */
#include <QObject>
#include <QString>

class ThermalGuard : public QObject {
    Q_OBJECT
public:
    static constexpr double kHotC    = 82.0;  ///< soglia ingresso pausa
    static constexpr double kCoolC   = 75.0;  ///< soglia uscita pausa (isteresi)
    static constexpr int    kRetryMs = 3000;  ///< intervallo retry consigliato

    static ThermalGuard& s() {
        static ThermalGuard inst(nullptr);
        return inst;
    }

    /** Alimentata da MainWindow::onHWUpdated (unica sorgente). */
    void update(double cpuC, double gpuC) {
        m_maxC = qMax(cpuC, gpuC);
        const bool hot = m_hot ? (m_maxC >= kCoolC) : (m_maxC >= kHotC);
        if (hot != m_hot) {
            m_hot = hot;
            emit hotChanged(hot);
        }
    }

    double maxTempC() const { return m_maxC; }
    bool   isHot()    const { return m_hot;  }

    /** Messaggio standard di pausa termica, uguale in tutti i moduli. */
    static QString pauseMessage(double maxC) {
        return tr("\xf0\x9f\x8c\xa1  %1\xc2\xb0""C — pausa termica, "
                  "riprendo appena la temperatura scende...")
                   .arg((int)maxC);
    }

signals:
    void hotChanged(bool hot);

private:
    explicit ThermalGuard(QObject* parent) : QObject(parent) {}
    double m_maxC = 0.0;
    bool   m_hot  = false;
};
