#pragma once
#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QTabWidget>
#include <QLabel>
#include <QStackedWidget>
#include "ai_client.h"

/* ══════════════════════════════════════════════════════════════
   QuizPage — "Sfida te stesso!"
   Tab 0 (Genera): genera quiz via AI con streaming.
   Tab 1 (Gioca):  quiz interattivo domanda per domanda.
   Tab 2 (Storico): dashboard con statistiche da ~/.prismalux_quiz.json
   ══════════════════════════════════════════════════════════════ */

struct QuizQuestion {
    QString     text;
    QStringList options;   // max 4 (vuoto = domanda aperta)
    int         correctIdx = -1; // indice opzione corretta (0-3), -1 = aperta
};

class QuizPage : public QWidget {
    Q_OBJECT
public:
    explicit QuizPage(AiClient* ai, QWidget* parent = nullptr);

private:
    void startGeneration();
    void stopGeneration();
    void loadDashboard();

    /* ── Gioca helpers ── */
    QList<QuizQuestion> parseQuiz(const QString& text);
    void showQuestion(int idx);
    void showRiepilogo();
    void resetGiocaUI();

    AiClient*    m_ai          = nullptr;
    QTabWidget*  m_tabs        = nullptr;

    /* ── Tab Genera ── */
    QLineEdit*   m_topicEdit   = nullptr;
    QSpinBox*    m_nDomande    = nullptr;
    QComboBox*   m_cmbTipo     = nullptr;
    QComboBox*   m_cmbDiff     = nullptr;
    QPushButton* m_btnGenera   = nullptr;
    QPushButton* m_btnCopy     = nullptr;
    QPushButton* m_btnGioca    = nullptr;
    QTextEdit*   m_output      = nullptr;

    /* ── Tab Gioca ── */
    QWidget*        m_giocaPanel   = nullptr;
    QStackedWidget* m_giocaStack   = nullptr; // 0=quiz, 1=riepilogo
    QLabel*         m_progressLbl  = nullptr;
    QLabel*         m_questionLbl  = nullptr;
    QWidget*        m_optWidget    = nullptr; // contenitore pulsanti opzioni
    QList<QPushButton*> m_optBtns;
    QLabel*         m_feedbackLbl  = nullptr;
    QPushButton*    m_btnNext      = nullptr;
    /* campo risposta aperta */
    QWidget*        m_openWidget   = nullptr;
    QTextEdit*      m_openAnswer   = nullptr;
    /* riepilogo */
    QLabel*         m_riepilogoLbl = nullptr;

    int m_giocaTabIdx = -1;

    /* ── Tab Storico ── */
    QWidget*     m_dashContent = nullptr;

    /* ── Stato gioco ── */
    QList<QuizQuestion> m_parsedQuestions;
    int  m_currentQ  = 0;
    int  m_score     = 0;
    bool m_answered  = false; // domanda corrente già risposta?

    QString      m_fullText;
    bool         m_generating = false;

    void _setGenerateBusy(bool busy);

public slots:
    /* Gioca — pubblici perché connessi da buildGiocaTab (funzione libera) */
    void onOptionSelected();
    void onNextQuestion();
    void onOpenNext();

private slots:
    void onTabChanged(int idx);
    void onGeneraClicked();
    void onCopyClicked();
    void onCopyRestoreText();
    void onQuizToken(const QString& tok);
    void onQuizFinished(const QString& full);
    void onQuizError(const QString& msg);
    void onGiocaClicked();
};
