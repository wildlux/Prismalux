#pragma once
#include <QWidget>
#include <QDoubleSpinBox>

class QComboBox;
class QSpinBox;
class QLabel;
class QGroupBox;

/* ══════════════════════════════════════════════════════════════════════════
   RamCalculatorWidget — Calcolatore RAM necessaria per LLM locali
   Parametri modello (miliardi) × bit/parametro (quantizzazione) + overhead
   runtime/contesto, confrontati con la RAM fisica disponibile sulla macchina.
   Formula: RAM pesi (GB) = parametri(B) × bit/8   [1 B param × 1 byte = 1 GB]
   ══════════════════════════════════════════════════════════════════════════ */
class RamCalculatorWidget : public QWidget {
    Q_OBJECT
public:
    explicit RamCalculatorWidget(QWidget* parent = nullptr);

private:
    QDoubleSpinBox* m_paramsSpin   = nullptr; ///< miliardi di parametri del modello
    QComboBox*      m_quantCombo   = nullptr; ///< preset di quantizzazione (bit/param)
    QDoubleSpinBox* m_bitsSpin     = nullptr; ///< bit per parametro, editabile a mano
    QSpinBox*       m_overheadSpin = nullptr; ///< overhead runtime/contesto/KV-cache (%)
    QDoubleSpinBox* m_ramSpin      = nullptr; ///< RAM fisica disponibile (GB)

    QGroupBox*  m_resBox        = nullptr;
    QLabel*     m_resModelLbl   = nullptr; ///< dimensione pesi grezza
    QLabel*     m_resTotalLbl   = nullptr; ///< RAM stimata totale (con overhead)
    QLabel*     m_resVerdictLbl = nullptr; ///< esito confronto con RAM disponibile
    QLabel*     m_resInstLbl    = nullptr; ///< quante istanze parallele ci stanno

    void addRamPresetButton(class QHBoxLayout* row, double gb);
    void recalc();

private slots:
    void onQuantPresetChanged(int idx);
};
