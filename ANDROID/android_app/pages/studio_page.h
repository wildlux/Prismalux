#pragma once
#include <QWidget>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QComboBox>
#include <QProgressBar>
#include <QStackedWidget>
#include <QDialog>
#include <QListWidget>
#include <QVBoxLayout>
#include <QMap>
#include <QSettings>
#include "../ai_client.h"
#include "quiz_ccna_db.h"

/* Dialog a tutto schermo per scegliere la materia (touch-friendly) */
class MateriaPickerDialog : public QDialog {
    Q_OBJECT
public:
    explicit MateriaPickerDialog(int currentIdx, QWidget* parent = nullptr);
signals:
    void materiaPicked(int idx);
private slots:
    void onItemClicked(QListWidgetItem* item);
private:
    QListWidget* m_list = nullptr;
};

/* --------------------------------------------------------------
   StudioPage -- Assistente studio AI per CCNA e tutte le materie.

   Layout interno usa QStackedWidget:
     0 — Menu principale (scroll: materia + modalità + output)
     1 — Schermata quiz a tutto schermo (solo domanda + risposte)
   -------------------------------------------------------------- */
class StudioPage : public QWidget {
    Q_OBJECT
public:
    explicit StudioPage(AiClient* ai, QWidget* parent = nullptr);

signals:
    void quizFullscreen(bool on);

private slots:
    void onMateriaChanged(int idx);
    void onModeClicked(int modeIdx);
    void onNextClicked();
    void onRevealClicked();
    void onToken(const QString& t);
    void onFinished(const QString& f);
    void onError(const QString& e);
    void onQuizAnswerClicked(int idx);
    void onQuizBack();
    void onTemaChanged(int idx);
    void onModeBtnClicked();
    void onQuizBtn0();
    void onQuizBtn1();
    void onQuizBtn2();
    void onQuizBtn3();
    void onAborted();
    void onMateriaBtnClicked();
    void onMateriaPickerResult(int idx);
    void onStatsClicked();

private:
    void runStudy(int modeIdx);
    void showQuizFullscreen(const QuizQuestion& q);
    QString buildSystemPrompt(int materiaIdx, int modeIdx) const;
    QString materiaName() const;
    void populateQuiz(const QString& raw);
    bool isCcnaSelected() const;

    /* ── Statistiche quiz ── */
    void loadStats();
    void saveStats();
    void showStats();

    struct QuizStat {
        int total   = 0;
        int correct = 0;
    };
    QMap<QString, QuizStat> m_stats;   ///< tema/materia → stat

    AiClient*     m_ai    = nullptr;
    QuizCcnaDb    m_ccnaDb;

    /* ── Stack principale ── */
    QStackedWidget* m_innerStack    = nullptr;

    /* ── Pagina 0: menu ── */
    QPushButton*  m_matBtn       = nullptr;  ///< bottone touch → apre MateriaPickerDialog
    QComboBox*    m_matCombo     = nullptr;  ///< nascosto, usato solo per la logica interna
    QLineEdit*    m_materiaEdit  = nullptr;
    QLineEdit*    m_argoEdit     = nullptr;
    QComboBox*    m_temaCombo    = nullptr;  ///< temi CCNA (solo CCNA)
    QWidget*      m_temaRow      = nullptr;  ///< riga tema (visibile solo CCNA)
    QTextEdit*    m_inputArea    = nullptr;
    QLabel*       m_inputLbl     = nullptr;
    QPushButton*  m_btnNext      = nullptr;
    QPushButton*  m_btnReveal    = nullptr;
    QPushButton*  m_btnStop      = nullptr;
    QLabel*       m_statusLbl    = nullptr;
    QProgressBar* m_progress     = nullptr;
    QTextEdit*    m_output       = nullptr;

    /* ── Pagina 1: quiz fullscreen ── */
    QWidget*     m_quizPage      = nullptr;
    QLabel*      m_quizNumLbl    = nullptr;  ///< "Domanda 1 — Tema: OSI"
    QLabel*      m_quizQLbl      = nullptr;
    QPushButton* m_quizBtns[4]   = {};
    QLabel*      m_quizFbLbl     = nullptr;
    QPushButton* m_quizNextBtn   = nullptr;
    QPushButton* m_quizBackBtn   = nullptr;

    /* ── Stato quiz ── */
    int     m_lastMode      = -1;
    bool    m_busy          = false;
    QString m_quizBuffer;
    int     m_quizCorrect   = -1;
    QString m_quizSpiega;
    int     m_quizCount     = 0;  ///< domande risposte in sessione corrente
};
