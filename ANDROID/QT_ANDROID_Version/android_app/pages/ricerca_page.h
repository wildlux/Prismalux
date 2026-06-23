#pragma once
#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QProgressBar>
#include <QDateEdit>
#include <QStackedWidget>
#include "../ai_client.h"

/* ──────────────────────────────────────────────────────────────
   RicercaPage mobile — "Ricerca & Sviluppo"

   Layout: ComboBox tipo + QTextEdit argomento + Genera + output streaming.
   Tipi: Paper Scientifico, Brevetto, Doc Tecnico, Analisi Letteratura, Web AI.

   Porting semplificato da gui/pages/ricerca_page.h:
   - Rimosse dipendenze: Cytoscape, RDKit, Bioconda, Avogadro (QProcess)
   - Rimosse dipendenze: RAB₀-L canvas, BLHM tabella
   - Mantenute: generazione AI di documenti di ricerca + export testo
   ────────────────────────────────────────────────────────────── */
class RicercaPage : public QWidget {
    Q_OBJECT
public:
    explicit RicercaPage(AiClient* ai, QWidget* parent = nullptr);

private:
    AiClient*    m_ai;

    QComboBox*     m_tipoCombo     = nullptr;
    QTextEdit*     m_argEdit       = nullptr;
    QLineEdit*     m_keywordEdit   = nullptr;
    QPushButton*   m_generateBtn   = nullptr;
    QPushButton*   m_stopBtn       = nullptr;
    QPushButton*   m_copyBtn       = nullptr;
    QTextEdit*     m_output        = nullptr;
    QLabel*        m_statusLbl     = nullptr;
    QProgressBar*  m_progressBar   = nullptr;
    QLabel*        m_hintLbl       = nullptr;
    QStackedWidget* m_extraStack   = nullptr;

    /* B5 — Carta Astrale (tipo 6) */
    QLineEdit*   m_astraleNome    = nullptr;
    QDateEdit*   m_astraleData    = nullptr;
    QLineEdit*   m_astraleLat     = nullptr;
    QLineEdit*   m_astraleLon     = nullptr;
    QPushButton* m_gpsBtn         = nullptr;

    /* B5 — Analisi Fenomeni (tipo 5) */
    QLabel*      m_fenomeniFileLbl = nullptr;
    QPushButton* m_fenomeniFileBtn = nullptr;
    QString      m_fenomeniFileContent;

    bool         m_busy      = false;
    QString      m_fullText;

    void updateHint(int tipoIdx);

    QMetaObject::Connection m_tokConn;
    QMetaObject::Connection m_finConn;
    QMetaObject::Connection m_errConn;

private slots:
    void onTipoChanged(int idx);
    void onGenerateClicked();
    void onStopClicked();
    void onCopyClicked();
    void onCopyRestore();
    void onToken(const QString& t);
    void onFinished(const QString& full);
    void onError(const QString& e);
    void onGpsBtnClicked();
    void onFenomeniFileBtnClicked();
};
