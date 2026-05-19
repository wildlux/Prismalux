#pragma once
#include <QWidget>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QTextEdit>
#include <QProgressBar>
#include <QComboBox>
#include "../ai_client.h"

/* --------------------------------------------------------------
   MisurePage -- Calcolatrice stanza + AI fotogrammetria.

   Due pannelli:
   1. Calcolatore manuale: lunghezza ? larghezza ? altezza ?
      area pavimento, pareti, volume, vernice, piastrelle.
   2. AI Analisi: invia testo descrittivo all'AI per stimare
      dimensioni / suggerire materiali / dare consigli.
   -------------------------------------------------------------- */
class MisurePage : public QWidget {
    Q_OBJECT
public:
    explicit MisurePage(AiClient* ai, QWidget* parent = nullptr);

private slots:
    void onCalcolaClicked();
    void onAiClicked();
    void onToken(const QString& t);
    void onFinished(const QString& f);
    void onError(const QString& e);
    void onAborted();

private:
    void calcola();

    AiClient* m_ai = nullptr;

    /* Calcolatore */
    QDoubleSpinBox* m_lungSpin  = nullptr;
    QDoubleSpinBox* m_largSpin  = nullptr;
    QDoubleSpinBox* m_altSpin   = nullptr;
    QDoubleSpinBox* m_sovrasSpin = nullptr;  ///< sovrapposizione piastrelle %
    QLabel*         m_resultLbl = nullptr;

    /* AI */
    QTextEdit*    m_aiInput   = nullptr;
    QComboBox*    m_aiAction  = nullptr;
    QPushButton*  m_btnAi     = nullptr;
    QPushButton*  m_btnStop   = nullptr;
    QLabel*       m_statusLbl = nullptr;
    QProgressBar* m_progress  = nullptr;
    QTextEdit*    m_aiOutput  = nullptr;
    bool          m_busy      = false;
};
