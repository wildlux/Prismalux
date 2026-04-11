#pragma once
#include <QWidget>
#include <QStackedWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include "../ai_client.h"

class PraticoPage : public QWidget {
    Q_OBJECT
public:
    explicit PraticoPage(AiClient* ai, QWidget* parent = nullptr);
private:
    AiClient*       m_ai;
    QStackedWidget* m_inner;
    QWidget* buildMenu();
    QWidget* buildChat(const QString& title, const QString& sysPrompt, const QString& placeholder);
};
