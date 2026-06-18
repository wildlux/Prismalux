#pragma once
#include <QWidget>
#include <QDoubleSpinBox>

class QComboBox;
class QSpinBox;
class QTableWidget;
class QLabel;
class QTextEdit;
class QPushButton;
class QGroupBox;

/* ══════════════════════════════════════════════════════════════════════════
   SolarCalcWidget — Calcolatore impianto fotovoltaico casalingo/van/off-grid
   Supporta due tipologie di regolatore: PWM e MPPT a confronto.
   ══════════════════════════════════════════════════════════════════════════ */
class SolarCalcWidget : public QWidget {
    Q_OBJECT
public:
    explicit SolarCalcWidget(QWidget* parent = nullptr);

private:
    /* ── Configurazione ── */
    QComboBox*      m_tipoCombo    = nullptr;
    QComboBox*      m_vSistCombo   = nullptr;
    QComboBox*      m_batCombo     = nullptr;
    QSpinBox*       m_autonomiaSpin = nullptr;
    QDoubleSpinBox* m_pshSpin      = nullptr;
    QSpinBox*       m_etaSpin      = nullptr;

    /* ── Tabella carichi ── */
    QTableWidget*   m_loadTable   = nullptr;
    QLabel*         m_totLbl      = nullptr;

    /* ── Parametri regolatore ── */
    QDoubleSpinBox* m_vocPwmSpin  = nullptr;  ///< Voc pannello singolo (PWM)
    QDoubleSpinBox* m_vMppSpin    = nullptr;  ///< Vmpp stringa (MPPT)
    QDoubleSpinBox* m_vocMpptSpin = nullptr;  ///< Voc stringa (MPPT)
    QSpinBox*       m_etaMpptSpin = nullptr;  ///< efficienza MPPT %
    QDoubleSpinBox* m_lCaviSpin   = nullptr;  ///< lunghezza cavi DC (m)

    /* ── Risultati ── */
    QGroupBox*  m_resBox       = nullptr;
    QLabel*     m_resConsLbl   = nullptr;
    QLabel*     m_resBatLbl    = nullptr;
    QLabel*     m_resPanLbl    = nullptr;
    QLabel*     m_resCaviLbl   = nullptr;
    QLabel*     m_resFusLbl    = nullptr;
    /* colonna PWM */
    QLabel*     m_pwmIregLbl   = nullptr;
    QLabel*     m_pwmTagliaLbl = nullptr;
    QLabel*     m_pwmNoteWarn  = nullptr;
    /* colonna MPPT */
    QLabel*     m_mpptIoutLbl  = nullptr;
    QLabel*     m_mpptTagliaLbl = nullptr;
    QLabel*     m_mpptGainLbl  = nullptr;
    /* dettaglio testuale */
    QTextEdit*  m_resDetail    = nullptr;
    QPushButton* m_copyBtn     = nullptr;

    void   addLoadRow(const QString& nome, double pw, double hrs);
    void   updateTotale();
    double selectedVoltage() const;
    double selectedDod()     const;

    static double ceilCavoMm2(double s);
    static int    ceilFusibile(double i);

private slots:
    void onAddRowClicked();
    void onRemoveRowClicked();
    void onPresetClicked();
    void onCellChanged(int row, int col);
    void onCalcolaClicked();
    void onCopyClicked();
    void onTipoChanged(int idx);
};
