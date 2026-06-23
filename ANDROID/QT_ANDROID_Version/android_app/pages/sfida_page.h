#pragma once
#include <QWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QProgressBar>
#include "../ai_client.h"

/* Sfida! — quiz gamificato AI-generated per mobile. */
class SfidaPage : public QWidget {
    Q_OBJECT
public:
    explicit SfidaPage(AiClient* ai, QWidget* parent = nullptr);

private slots:
    void onGenerateClicked();
    void onAnswerA(); void onAnswerB(); void onAnswerC(); void onAnswerD();
    void onToken(const QString& t);
    void onFinished(const QString& full);
    void onAiError(const QString& msg);
    void setBusy(bool busy);

private:
    void parseQuiz(const QString& raw);
    void showQuestion();
    void checkAnswer(int selected);
    void nextQuestion();

    AiClient*    m_ai       = nullptr;
    QComboBox*   m_topicCmb = nullptr;
    QComboBox*   m_diffCmb  = nullptr;
    QLabel*      m_scoreLabel = nullptr;
    QProgressBar* m_progress = nullptr;
    QLabel*      m_questionLabel = nullptr;
    QPushButton* m_answerBtns[4] = {};
    QPushButton* m_genBtn    = nullptr;
    QPushButton* m_nextBtn   = nullptr;
    QLabel*      m_feedback  = nullptr;
    QLabel*      m_status    = nullptr;

    struct Question {
        QString text;
        QStringList options; /* A, B, C, D */
        int correct = 0;    /* 0-based */
        QString explanation;
    };

    QList<Question> m_questions;
    int m_currentQ  = 0;
    int m_score     = 0;
    int m_total     = 0;
    bool m_answered = false;
    QString m_rawBuffer;
};
