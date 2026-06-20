#pragma once
#include "widget_ai_script_base.h"
class QLabel;
class QPushButton;
class QTextEdit;
class QComboBox;
class QProcess;
class ModelComboBox;

class BiocondaWidget : public AiScriptWidget {
    Q_OBJECT
public:
    explicit BiocondaWidget(AiClient* ai, QWidget* parent = nullptr);

private:
    QLabel*      m_statusLbl = nullptr;
    QPushButton* m_execBtn   = nullptr;
    QComboBox*   m_action    = nullptr;
    ModelComboBox* m_model   = nullptr;
    QTextEdit*   m_input     = nullptr;
    QTextEdit*   m_output    = nullptr;
    QPushButton* m_runBtn    = nullptr;
    QPushButton* m_stopBtn   = nullptr;
    QString      m_code;
    QProcess*    m_proc      = nullptr;

private slots:
    void onCheckClicked();
    void onCheckFinished(int code, QProcess::ExitStatus);
    void onExecClicked();
    void onRunClicked();
    void onStopClicked();
};
