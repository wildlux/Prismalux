#pragma once
#include <QWidget>
#include <QTabWidget>
#include "../ai_client.h"
#include "../hardware_monitor.h"

/* ══════════════════════════════════════════════════════════════
   ImpostazioniPage — Personalizzazione + Manutenzione
   Layout: QTabWidget con 2 tab diretti (nessun menu intermedio)
   ══════════════════════════════════════════════════════════════ */
class PersonalizzaPage;
class ManutenzioneePage;

class ImpostazioniPage : public QWidget {
    Q_OBJECT
public:
    explicit ImpostazioniPage(AiClient* ai, HardwareMonitor* hw, QWidget* parent = nullptr);
    void onHWReady(HWInfo hw);

private:
    QTabWidget*        m_tabs        = nullptr;
    PersonalizzaPage*  m_personalizza = nullptr;
    ManutenzioneePage* m_manutenzione = nullptr;
};
