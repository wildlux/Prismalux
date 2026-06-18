#pragma once
#include <QWidget>

class QComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QTableWidget;
class QLabel;
class QTextEdit;

/* ════════════════════════════════════════════════════════════════════════════
   IdroWidget — Calcolatore / diario per impianti idroponici
   Supporta: NFT, DWC, Ebb&Flow, Aeroponico, Kratky, Acquaponica
   ════════════════════════════════════════════════════════════════════════════ */
class IdroWidget : public QWidget {
    Q_OBJECT
public:
    explicit IdroWidget(QWidget* parent = nullptr);

private:
    /* ── Sistema ── */
    QComboBox*      m_sistemaCombo  = nullptr;
    QSpinBox*       m_volumeSpin    = nullptr;
    QSpinBox*       m_nPianteSpin   = nullptr;
    QWidget*        m_pompaRow      = nullptr;  // visibile solo se sistema con cicli
    QSpinBox*       m_pompaOnSpin   = nullptr;
    QSpinBox*       m_pompaOffSpin  = nullptr;

    /* ── Coltura e fase ── */
    QComboBox*      m_colturaCombo  = nullptr;
    QComboBox*      m_faseCombo     = nullptr;
    QLabel*         m_phRangeLbl    = nullptr;
    QLabel*         m_ecRangeLbl    = nullptr;
    QLabel*         m_tAcquaLbl     = nullptr;
    QLabel*         m_tAriaLbl      = nullptr;
    QLabel*         m_luceLbl       = nullptr;

    /* ── Calcolatore dosi ── */
    QDoubleSpinBox* m_volSolSpin    = nullptr;
    QDoubleSpinBox* m_nutADoseSpin  = nullptr;
    QDoubleSpinBox* m_nutBDoseSpin  = nullptr;
    QDoubleSpinBox* m_nutCDoseSpin  = nullptr;
    QLabel*         m_nutAResLbl    = nullptr;
    QLabel*         m_nutBResLbl    = nullptr;
    QLabel*         m_nutCResLbl    = nullptr;

    /* ── Log misurazioni ── */
    QTableWidget*   m_logTable      = nullptr;
    QLabel*         m_lastMeasLbl   = nullptr;

    /* ── Note ── */
    QTextEdit*      m_noteEdit      = nullptr;

    void updateParametriConsigliati();
    void updateLastMeas();
    void addLogRow(const QString& dt, const QString& ph,
                   const QString& ec, const QString& tAc,
                   const QString& tAr, const QString& note);
    void loadLog();
    void saveLog();
    QString logFilePath() const;

private slots:
    void onSistemaChanged(int idx);
    void onColturaFaseChanged();
    void onCalcolaDosiClicked();
    void onAddMeasurementClicked();
    void onRemoveMeasurementClicked();
    void onExportCsvClicked();
    void onLogCellChanged(int row, int col);
    void onNoteChanged();
};
