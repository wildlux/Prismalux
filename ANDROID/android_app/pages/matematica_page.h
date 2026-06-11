#pragma once
#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QProgressBar>
#include "../ai_client.h"

/* ──────────────────────────────────────────────────────────────
   MatematicaPage mobile — "Laboratorio Matematico"

   Porting da gui/pages/matematica_page.h — adattato per touch mobile:
   - Sostituisce QProcess/python3 con AI pura (nessun subprocess su Android)
   - 4 modalità via ComboBox: Sequenza, Costanti, N-esimo, Espressione
   - Bottoni rapidi con esempi touch-friendly
   - Output streaming dall'AI
   ────────────────────────────────────────────────────────────── */
class MatematicaPage : public QWidget {
    Q_OBJECT
public:
    explicit MatematicaPage(AiClient* ai, QWidget* parent = nullptr);

private:
    AiClient*     m_ai;

    QComboBox*    m_modeCombo    = nullptr;
    QTextEdit*    m_inputEdit    = nullptr;
    QLabel*       m_inputHint    = nullptr;
    QPushButton*  m_calcBtn      = nullptr;
    QPushButton*  m_stopBtn      = nullptr;
    QPushButton*  m_copyBtn      = nullptr;
    QPushButton*  m_clearBtn     = nullptr;
    QTextEdit*    m_output       = nullptr;
    QLabel*       m_statusLbl    = nullptr;
    QProgressBar* m_progressBar  = nullptr;
    QWidget*      m_examplesRow  = nullptr;

    bool    m_busy    = false;
    QString m_fullText;

    void updateModeUI(int modeIdx);
    QString buildSystemPrompt(int modeIdx, const QString& input) const;

    QPushButton* m_solveRandomBtn = nullptr;
    QLabel*      m_solveInfoLbl   = nullptr;

    QMetaObject::Connection m_tokConn;
    QMetaObject::Connection m_finConn;
    QMetaObject::Connection m_errConn;

private slots:
    void onModeChanged(int idx);
    void onCalcClicked();
    void onStopClicked();
    void onCopyClicked();
    void onCopyRestore();
    void onClearClicked();
    void onExampleClicked();
    void onSolveRandomClicked();
    void onToken(const QString& t);
    void onFinished(const QString& full);
    void onError(const QString& e);
};
