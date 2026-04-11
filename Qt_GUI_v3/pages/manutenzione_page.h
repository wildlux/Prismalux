#pragma once
#include <QWidget>
#include <QLabel>
#include <QComboBox>
#include <QTextEdit>
#include "../hardware_monitor.h"
#include "../ai_client.h"

class ManutenzioneePage : public QWidget {
    Q_OBJECT
public:
    explicit ManutenzioneePage(AiClient* ai, HardwareMonitor* hw, QWidget* parent = nullptr);
    void onHWReady(const HWInfo& hw);

private:
    AiClient*        m_ai;
    HardwareMonitor* m_hw;
    QLabel*          m_hwLabel;
    QComboBox*       m_cmbModel;
    QComboBox*       m_cmbBackend;
    QLabel*          m_ramStatusLbl = nullptr;  ///< stato corrente zRAM / Windows MC
    QTextEdit*       m_ramLog       = nullptr;  ///< log operazioni RAM
    QComboBox*       m_cmbFmt       = nullptr;  ///< formato config: JSON / TOON
    QLabel*          m_fmtStatus    = nullptr;  ///< feedback operazione conversione

    void updateHWLabel(const HWInfo& hw);

    /* ── helpers formato config ─────────────────────────────────────── */
    /** Legge il formato attivo da ~/.prismalux_config.json o .toon */
    static QString detectConfigFmt();
    /** Converte il config nel nuovo formato e restituisce "" se ok, errore altrimenti */
    static QString convertConfig(const QString& newFmt);
};
