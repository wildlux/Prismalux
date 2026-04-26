#pragma once
#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QTabWidget>
#include "ai_client.h"

/* ══════════════════════════════════════════════════════════════
   QuizPage — "Sfida te stesso!"
   Tab 0 (Genera): genera quiz via AI con streaming.
   Tab 1 (Storico): dashboard con statistiche da ~/.prismalux_quiz.json
   ══════════════════════════════════════════════════════════════ */
class QuizPage : public QWidget {
    Q_OBJECT
public:
    explicit QuizPage(AiClient* ai, QWidget* parent = nullptr);

private:
    void startGeneration();
    void stopGeneration();
    void loadDashboard();

    AiClient*    m_ai          = nullptr;
    QTabWidget*  m_tabs        = nullptr;

    /* ── Tab Genera ── */
    QLineEdit*   m_topicEdit   = nullptr;
    QSpinBox*    m_nDomande    = nullptr;
    QComboBox*   m_cmbTipo     = nullptr;
    QComboBox*   m_cmbDiff     = nullptr;
    QPushButton* m_btnGenera   = nullptr;
    QPushButton* m_btnCopy     = nullptr;
    QTextEdit*   m_output      = nullptr;

    /* ── Tab Storico ── */
    QWidget*     m_dashContent = nullptr;

    QString      m_fullText;               /* testo accumulato per copia */
    bool         m_generating = false;    ///< guard per segnali token/finished/error

    void _setGenerateBusy(bool busy);
};
