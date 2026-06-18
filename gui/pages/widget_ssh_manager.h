#pragma once
#include <QWidget>
#include <QProcess>
#include <QPointer>
#include <QCheckBox>

class QTableWidget;
class QTextEdit;
class QLineEdit;
class QSpinBox;
class QComboBox;
class QPushButton;
class QLabel;
class QProgressBar;

class SshManagerWidget : public QWidget {
    Q_OBJECT
public:
    explicit SshManagerWidget(QWidget* parent = nullptr);

private:
    /* -- Discovery -- */
    QTableWidget*      m_hostTable   = nullptr;
    QPushButton*       m_arpBtn      = nullptr;
    QPushButton*       m_nmapBtn     = nullptr;
    QPushButton*       m_stopBtn     = nullptr;
    QProgressBar*      m_scanBar     = nullptr;
    QLabel*            m_scanLbl     = nullptr;
    QPointer<QProcess> m_scanProc;
    QString            m_scanBuf;   ///< buffer righe parziali

    /* -- Connessione -- */
    QLineEdit*   m_hostEdit   = nullptr;
    QSpinBox*    m_portSpin   = nullptr;
    QLineEdit*   m_userEdit   = nullptr;
    QLineEdit*   m_keyEdit    = nullptr;
    QComboBox*   m_modeCombo  = nullptr;
    QCheckBox*   m_compCheck  = nullptr;
    QCheckBox*   m_agentCheck = nullptr;
    QTextEdit*   m_preview    = nullptr;
    QPushButton* m_connBtn    = nullptr;
    QPushButton* m_copyBtn    = nullptr;
    QLabel*      m_noteLbl    = nullptr;

    static QString findTerminal();
    QString        buildCmd() const;
    void           updatePreview();
    void           parseScanLines(const QString& lines);
    void           addOrUpdateHost(const QString& ip, const QString& host,
                                   const QString& mac,  const QString& iface);

private slots:
    void onArpClicked();
    void onNmapClicked();
    void onStopClicked();
    void onScanReadyRead();
    void onScanFinished(int code, QProcess::ExitStatus st);
    void onUseSelected();
    void onModeChanged(int idx);
    void onConnectClicked();
    void onCopyClicked();
    void onKeyBrowseClicked();
};
