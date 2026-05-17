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
#include <QAbstractSocket>
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
    QPushButton* m_lanWebBtn    = nullptr;
    QLineEdit*   m_lanTokenEdit = nullptr;
    QPushButton* m_qrApkBtn     = nullptr;
    QPushButton* m_qrPageBtn    = nullptr;
    QLabel*      m_qrConnectLbl = nullptr;
    QString      m_lanConnectIp;

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
    QProcess*      m_gns3ExecProc    = nullptr;
    QProgressBar*  m_gns3Progress    = nullptr;

    QString  localLanIp() const;
    QString  serverScheme() const;
    void     openQrDialog(QPushButton* parent, const QString& url,
                          const QString& title, const QString& subtitle,
                          const QString& note);

    QWidget* buildLanAndroidTab();
    QWidget* buildGNS3Tab();
    void     gns3RunAi(const QString& sys, const QString& userMsg);
    void     gns3PopulateModels(QComboBox* combo);

private slots:
    void onModelsReady(const QStringList& models);
    void onTokenTextChanged(const QString& t);
    void onEyeBtnToggled(bool show);
    void onRegenBtnClicked();
    void onCopyTokenBtnClicked();
    void onQrConnectBtnClicked();
    void onLanPortChanged(int v);
    void onQrApkBtnClicked();
    void onQrPageBtnClicked();
    void onLanToggleBtnToggled(bool on);
    void onLanServerStatusChanged(bool running);
    void onLanClientConnected(const QString& addr);
    void onLanClientDisconnected(const QString& addr);
    void onLanWebBtnClicked();
    void onGns3AiToken(const QString& t);
    void onGns3AiFinished(const QString& full);
    void onGns3AiError(const QString& msg);
    void onPingBtnClicked();
    void onGns3SockConnected();
    void onGns3SockError(QAbstractSocket::SocketError err);
    void onGns3ExecBtnClicked();
    void onGns3ProcReadyRead();
    void onGns3ProcFinished(int code, QProcess::ExitStatus status);
    void onGns3RunBtnClicked();
    void onGns3StopBtnClicked();
};
