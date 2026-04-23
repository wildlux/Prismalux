#pragma once
#include <QWidget>
#include <QLabel>
#include <QComboBox>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QProcess>
#include "../hardware_monitor.h"
#include "../ai_client.h"

class ManutenzioneePage : public QWidget {
    Q_OBJECT
public:
    explicit ManutenzioneePage(AiClient* ai, HardwareMonitor* hw, QWidget* parent = nullptr);
    void onHWReady(const HWInfo& hw);

    /** Costruiscono i widget di sezione — chiamati da ImpostazioniPage per tab piatte */
    QWidget* buildBackend();
    QWidget* buildConfigFmt();
    QWidget* buildHardware();

private:
    AiClient*        m_ai;
    HardwareMonitor* m_hw;
    QLabel*          m_hwLabel = nullptr;
    QComboBox*       m_cmbModel;
    QComboBox*       m_cmbBackend;
    QLabel*          m_ramStatusLbl = nullptr;  ///< stato corrente zRAM / Windows MC
    QTextEdit*       m_ramLog       = nullptr;  ///< log operazioni RAM
    QComboBox*       m_cmbFmt       = nullptr;  ///< formato config: JSON / TOON
    QLabel*          m_fmtStatus    = nullptr;  ///< feedback operazione conversione

    /* ── Avvia llama-server (scheda Backend) ────────────────────────── */
    QLineEdit*   m_srvModelPath = nullptr;  ///< percorso .gguf
    QLineEdit*   m_srvPort      = nullptr;  ///< porta (default kLlamaServerPort)
    QPushButton* m_srvStartBtn  = nullptr;  ///< ▶ Avvia
    QPushButton* m_srvStopBtn   = nullptr;  ///< ■ Stop
    QTextEdit*   m_srvLog       = nullptr;  ///< log output server
    QProcess*    m_srvProc      = nullptr;  ///< processo llama-server

    void updateHWLabel(const HWInfo& hw);

    /* ── helpers formato config ─────────────────────────────────────── */
    /** Legge il formato attivo da ~/.prismalux_config.json o .toon */
    static QString detectConfigFmt();
    /** Converte il config nel nuovo formato e restituisce "" se ok, errore altrimenti */
    static QString convertConfig(const QString& newFmt);
};
