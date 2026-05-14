#pragma once
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QComboBox>
#include <QProcess>
#include <QProgressBar>
#include "../ai_client.h"
#include "../widgets/ai_error_widget.h"

class LanServer;

class LanWanPage : public QWidget {
    Q_OBJECT
public:
    explicit LanWanPage(AiClient* ai, QWidget* parent = nullptr);
private:
    AiClient*    m_ai           = nullptr;
    LanServer*   m_lanServer    = nullptr;
    QPushButton* m_lanToggleBtn = nullptr;
    QSpinBox*    m_lanPortSpin  = nullptr;
    QLabel*      m_lanStatusLbl = nullptr;
    QLabel*      m_lanClientsLbl= nullptr;
    QPushButton* m_lanWebBtn    = nullptr;  ///< "🌐 Chat Web" — apre browser
    QLineEdit*   m_lanTokenEdit = nullptr;  ///< token Bearer opzionale

    /* ── GNS3 MCP ── */
    QLineEdit*     m_gns3HostEdit    = nullptr;
    QLabel*        m_gns3StatusLbl   = nullptr;
    QPushButton*   m_gns3ExecBtn     = nullptr;
    QComboBox*     m_gns3Action      = nullptr;
    QComboBox*     m_gns3Model       = nullptr;
    QTextEdit*     m_gns3Input       = nullptr;
    QTextEdit*     m_gns3Output      = nullptr;
    QPushButton*   m_gns3RunBtn      = nullptr;
    QPushButton*   m_gns3StopBtn     = nullptr;
    QString        m_gns3Code;
    QObject*       m_gns3TokenHolder = nullptr;
    bool           m_gns3AiActive    = false;
    AiErrorWidget* m_gns3ErrorPanel  = nullptr;
    QProcess*      m_gns3ExecProc    = nullptr;  ///< processo Python GNS3 in esecuzione
    QProgressBar*  m_gns3Progress   = nullptr;

    QWidget* buildLanAndroidTab();
    QWidget* buildGNS3Tab();
    void     gns3RunAi(const QString& sys, const QString& userMsg);
    void     gns3PopulateModels(QComboBox* combo);
};
