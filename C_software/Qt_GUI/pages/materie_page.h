#pragma once
#include <QWidget>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QLineEdit>
#include <QSplitter>
#include <QLabel>
#include <memory>
#include "../ai_client.h"
#include "tutor_data.h"

/* ══════════════════════════════════════════════════════════════
   SubjectTutorWidget — pannello con albero argomenti + chat AI
   ══════════════════════════════════════════════════════════════ */
class SubjectTutorWidget : public QWidget {
    Q_OBJECT
public:
    SubjectTutorWidget(AiClient* ai, const QString& subject,
                       const SubjectData& data, QWidget* parent = nullptr);

private:
    AiClient*    m_ai;
    QString      m_subject;
    QTreeWidget* m_tree = nullptr;
    QTextEdit*   m_log  = nullptr;
    QLineEdit*   m_inp  = nullptr;
    QPushButton* m_send    = nullptr;
    QPushButton* m_stop    = nullptr;
    QLabel*      m_waitLbl = nullptr;  ///< ⏳ mostrato durante elaborazione AI

    void askTopic(const QString& topicName, const QString& topicDesc);
    void sendQuestion(const QString& question, const QString& sysSuffix = "");
};

/* ══════════════════════════════════════════════════════════════
   MateriePage — menu 6 materie + sub-stack
   ══════════════════════════════════════════════════════════════ */
class MateriePage : public QWidget {
    Q_OBJECT
public:
    explicit MateriePage(AiClient* ai, QWidget* parent = nullptr);

    void goBack() { m_stack->setCurrentIndex(0); }

private:
    AiClient*       m_ai;
    QStackedWidget* m_stack = nullptr;

    QWidget* buildMenu();
};
