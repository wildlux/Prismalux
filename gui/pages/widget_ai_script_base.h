#pragma once
#include <QWidget>
#include <QMetaObject>
#include <QProcess>
class AiClient;
class AiErrorWidget;
class QTextEdit;
class QPushButton;
class QLabel;
class QComboBox;
class QProgressBar;

/*
 * AiScriptWidget — base class per tutti i widget "AI genera script + esegui"
 * Fornisce avviaSci() e runSciScript() riutilizzabili da ogni sotto-tab.
 * Il costruttore non crea widget: le sottoclassi costruiscono la propria UI
 * e assegnano m_errorPanel / m_progress se li vogliono.
 */
class AiScriptWidget : public QWidget {
    Q_OBJECT
public:
    explicit AiScriptWidget(AiClient* ai, QWidget* parent = nullptr);
    ~AiScriptWidget() override;

protected:
    AiClient*      m_ai;
    AiErrorWidget* m_errorPanel = nullptr; /* assegnato dalla sottoclasse */
    QProgressBar*  m_progress   = nullptr; /* assegnato dalla sottoclasse */

    void avviaSci(const QString& sys, const QString& userMsg,
                  QTextEdit* out, QPushButton* runBtn, QPushButton* stopBtn,
                  QComboBox* modelCombo,
                  QPushButton* execBtn, QString* codeRef, QLabel* statusLbl);

    static void runSciScript(const QString& code, bool isBash,
                             QLabel* statusLbl, QPushButton* execBtn,
                             QTextEdit* output, QProcess*& procRef,
                             QObject* parent);

private slots:
    void onSciToken(const QString& t);
    void onSciFinished(const QString& full);
    void onSciError(const QString& msg);

private:
    QTextEdit*   m_sciOut        = nullptr;
    QPushButton* m_sciRunBtn     = nullptr;
    QPushButton* m_sciStopBtn    = nullptr;
    QPushButton* m_sciExecBtn    = nullptr;
    QString*     m_sciCodeRef    = nullptr;
    QLabel*      m_sciStatusLbl  = nullptr;
    QComboBox*   m_sciModelCombo = nullptr;
    QString      m_sciSys;
    QString      m_sciUserMsg;
    QMetaObject::Connection m_sciTokenConn;
    QMetaObject::Connection m_sciFinishedConn;
    QMetaObject::Connection m_sciErrorConn;
};
