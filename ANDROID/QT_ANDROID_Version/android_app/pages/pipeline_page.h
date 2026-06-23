#pragma once
#include <QWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QSpinBox>
#include <QStackedWidget>
#include "../ai_client.h"

/* Pipeline AI mobile — modalità Byzantino (N agenti votano) + Agente Autonomo (ReAct). */
class PipelinePage : public QWidget {
    Q_OBJECT
public:
    explicit PipelinePage(AiClient* ai, QWidget* parent = nullptr);

private slots:
    void onModeChanged(int idx);
    void onRunClicked();
    void onStopClicked();
    void onToken(const QString& t);
    void onFinished(const QString& full);
    void onAiError(const QString& msg);
    void onAborted();
    void setBusy(bool busy);

private:
    QWidget* buildByzantineTab();
    QWidget* buildAutonomousTab();
    void runByzantine();
    void runAutonomous();

    AiClient*       m_ai          = nullptr;
    QStackedWidget* m_modeStack   = nullptr;
    QComboBox*      m_modeCmb     = nullptr;

    /* Byzantino */
    QSpinBox*   m_byzAgentsSpin  = nullptr;
    QTextEdit*  m_byzInput       = nullptr;
    QTextEdit*  m_byzOutput      = nullptr;
    int         m_byzRound       = 0;
    int         m_byzTotalAgents = 3;
    QStringList m_byzResponses;

    /* Autonomo */
    QTextEdit*  m_autInput       = nullptr;
    QTextEdit*  m_autOutput      = nullptr;
    int         m_autStep        = 0;
    static constexpr int kMaxAutSteps = 6;

    QPushButton* m_runBtn  = nullptr;
    QPushButton* m_stopBtn = nullptr;
    QLabel*      m_status  = nullptr;

    enum class Mode { Byzantine, Autonomous } m_mode = Mode::Byzantine;
};
