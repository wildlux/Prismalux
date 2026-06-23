#pragma once
#include <QWidget>
#include <QStackedWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QProgressBar>
#include <QJsonArray>
#include "../ai_client.h"

/* ──────────────────────────────────────────────────────────────
   ImparaPage — "Impara con AI"

   Layout interno (QStackedWidget m_inner):
     0 = Menu principale  → 3 card grandi touch-friendly
     1 = Tutor AI         → chat con docente AI su materia scelta
     2 = Quiz AI          → domande A/B/C/D generate da AI

   Porting da gui/pages/impara_page.h — adattato per touch mobile:
   - Nessun QProcess / subprocess
   - Nessun file system per quiz history (sessione in memoria)
   - QStackedWidget invece di QTabWidget
   - Bottoni touch ≥ 52px
   ────────────────────────────────────────────────────────────── */
class ImparaPage : public QWidget {
    Q_OBJECT
public:
    explicit ImparaPage(AiClient* ai, QWidget* parent = nullptr);

protected:
    void showEvent(QShowEvent* ev) override;

signals:
    void sendToChat(const QString& text);

private:
    /* ── costruzione sotto-pagine ── */
    QWidget* buildMenu();
    QWidget* buildTutor();
    QWidget* buildQuiz();

    /* ── quiz helpers ── */
    void generateQuestion();
    void parseAndShowQuestion(const QString& raw);
    void submitAnswer(int optionIdx);
    void showFeedback(bool correct);
    void resetQuizButtons();

    /* ── stato ── */
    AiClient*       m_ai;
    QStackedWidget* m_inner       = nullptr;

    /* Tutor */
    QTextEdit*   m_tutorLog     = nullptr;
    QComboBox*   m_tutorSubj    = nullptr;
    QLineEdit*   m_tutorInp     = nullptr;
    QPushButton* m_tutorSend    = nullptr;
    QPushButton* m_tutorStop    = nullptr;
    QLabel*      m_tutorStatus  = nullptr;
    bool         m_tutorBusy    = false;
    QJsonArray   m_tutorHistory;

    /* Quiz */
    struct QuizState {
        QString subject;
        QString difficulty;
        int correct     = 0;
        int wrong       = 0;
        QString correctLetter;
        QString explanation;
        QString rawBuf;
        bool answered   = false;
        bool generating = false;
    } m_quiz;

    QLabel*       m_quizSubjLbl   = nullptr;
    QLabel*       m_quizScore     = nullptr;
    QLabel*       m_quizQuestion  = nullptr;
    QPushButton*  m_quizOpts[4]   = {};
    QLabel*       m_quizFeedback  = nullptr;
    QLabel*       m_quizExpLbl    = nullptr;
    QPushButton*  m_quizNext      = nullptr;
    QPushButton*  m_quizGen       = nullptr;
    QPushButton*  m_quizStop      = nullptr;
    QComboBox*    m_quizSubj      = nullptr;
    QComboBox*    m_quizDiff      = nullptr;
    QProgressBar* m_quizBar       = nullptr;

    /* One-shot connections tutor */
    QMetaObject::Connection m_tutorTokConn;
    QMetaObject::Connection m_tutorFinConn;
    QMetaObject::Connection m_tutorErrConn;

    /* One-shot connections quiz */
    QMetaObject::Connection m_quizTokConn;
    QMetaObject::Connection m_quizFinConn;
    QMetaObject::Connection m_quizErrConn;

private slots:
    void onBackToMenu();
    void onTutorCardClicked();
    void onQuizCardClicked();

    /* Tutor */
    void onTutorSendClicked();
    void onTutorStopClicked();
    void onTutorToken(const QString& t);
    void onTutorFinished(const QString& full);
    void onTutorError(const QString& e);
    void onTutorAborted();

    /* Quiz */
    void onQuizGeneraClicked();
    void onQuizStopClicked();
    void onQuizToken(const QString& t);
    void onQuizFinished(const QString& full);
    void onQuizError(const QString& e);
    void onQuizOpt0Clicked();
    void onQuizOpt1Clicked();
    void onQuizOpt2Clicked();
    void onQuizOpt3Clicked();
    void onQuizNextClicked();
};
