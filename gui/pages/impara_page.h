#pragma once
#include <QWidget>
#include <QStackedWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QJsonArray>
#include "../ai_client.h"
#include "materie_page.h"
#include "simulatore_page.h"

/* ══════════════════════════════════════════════════════════════
   ImparaPage — Tutor AI, Quiz Interattivi, Dashboard Statistica
   ══════════════════════════════════════════════════════════════ */
class ImparaPage : public QWidget {
    Q_OBJECT
public:
    explicit ImparaPage(AiClient* ai, QWidget* parent = nullptr);

protected:
    /**
     * showEvent — riporta sempre al menu principale (index 0) quando
     * la pagina diventa visibile tramite il bottone sidebar.
     * Evita le "schermate fantasma": se l'utente era in MateriePage/Algoritmi
     * e naviga via sidebar, al ritorno vede il menu e non la vecchia sotto-pagina.
     */
    void showEvent(QShowEvent* ev) override;

private:
    /* ── costruzione ── */
    QWidget* buildMenu();
    QWidget* buildTutor();
    QWidget* buildQuiz();
    QWidget* buildDashboard();
    QWidget* buildModelBar(QWidget* parent);  /* barra selettore backend */

    /* ── quiz helpers ── */
    void generateQuestion();
    void parseAndShowQuestion(const QString& raw);
    void submitAnswer(int idx);
    void nextQuestion();
    void endSession();
    void saveSession();

    /* ── dashboard helpers ── */
    void loadDashboard();

    /* ── common ── */
    QString historyPath() const;

    /* ── state ── */
    AiClient*       m_ai;
    QStackedWidget* m_inner     = nullptr;
    bool            m_quizBusy  = false;  ///< guard: evita richieste sovrapposte

    /* ── tutor ── */
    QTextEdit*   m_tutorLog     = nullptr;
    QComboBox*   m_tutorSubj    = nullptr;
    QLineEdit*   m_tutorInp     = nullptr;
    QPushButton* m_tutorSend    = nullptr;
    QPushButton* m_tutorStop    = nullptr;
    QLabel*      m_tutorWaitLbl = nullptr;

    /* ── quiz state ── */
    struct QuizState {
        QString subject;
        QString difficulty;
        int totalQ    = 5;
        int currentQ  = 0;
        int correct   = 0;
        int wrong     = 0;
        QString correctLetter;   /* lettera risposta corretta: A/B/C/D */
        QString explanation;
        bool answered = false;
    } m_quiz;

    /* ── quiz widgets ── */
    QLabel*      m_quizProgress  = nullptr;
    QLabel*      m_quizScore     = nullptr;
    QLabel*      m_quizQuestion  = nullptr;
    QPushButton* m_quizOpts[4]   = {};
    QLabel*      m_quizFeedback  = nullptr;
    QPushButton* m_quizNext      = nullptr;
    QPushButton* m_quizGen       = nullptr;
    QComboBox*   m_quizSubj      = nullptr;
    QComboBox*   m_quizDiff      = nullptr;
    QComboBox*   m_quizNum       = nullptr;
    QTextEdit*   m_quizRaw       = nullptr;  /* debug buffer */

    /* ── quiz AI connections (nominali, evita static) ── */
    QMetaObject::Connection m_quizTokConn;
    QMetaObject::Connection m_quizFinConn;
    QMetaObject::Connection m_quizErrConn;

    /* ── dashboard widgets ── */
    QWidget*     m_dashContent   = nullptr;

    /* ── materie ── */
    MateriePage*     m_materiePage     = nullptr;
    /* ── simulatore ── */
    SimulatorePage*  m_simulatorePage  = nullptr;

private slots:
    void onBackToMenu();
    void onMenuCardClicked();
    void onTutorContextMenu(const QPoint& pos);
    void onTutorSendClicked();
    void onTutorToken(const QString& t);
    void onTutorFinished(const QString& full);
    void onTutorError(const QString& e);
    void onTutorAborted();
    void onQuizToken(const QString& t);
    void onQuizFinished(const QString& full);
    void onQuizError(const QString& e);
};
