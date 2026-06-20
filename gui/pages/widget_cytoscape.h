#pragma once
#include "widget_ai_script_base.h"
#include <QAbstractSocket>
class QLineEdit;
class QLabel;
class QPushButton;
class QTextEdit;
class QComboBox;
class QProcess;
class QTcpSocket;
class ModelComboBox;

class CytoscapeWidget : public AiScriptWidget {
    Q_OBJECT
public:
    explicit CytoscapeWidget(AiClient* ai, QWidget* parent = nullptr);

private:
    QLineEdit*   m_hostEdit  = nullptr;
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
    QTcpSocket*  m_sock      = nullptr;

private slots:
    void onPingClicked();
    void onSockConnected();
    void onSockError(QAbstractSocket::SocketError);
    void onPingTimeout();
    void onExecClicked();
    void onRunClicked();
    void onStopClicked();
};
