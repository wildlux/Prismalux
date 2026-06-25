#pragma once
#include <QWidget>
#include <QMetaObject>
#include "../ai_client.h"

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPushButton;
class QPlainTextEdit;
class QTextEdit;
class QLabel;
class QProcess;
class QListWidget;
class QStackedWidget;
class QGroupBox;
class QSplitter;
class QTabWidget;

/* ══════════════════════════════════════════════════════════════
   WebDevWidget — Avvio server web (Django/Flask/FastAPI/…)
                + Analisi LLM agentica del progetto

   Layout:
   ┌──────────────────────────────────────────────────────────┐
   │ 📁 [path progetto ....................................] [📂]│
   │     🐍 Django 4.2 — manage.py rilevato                  │
   ├─────────────────────────┬────────────────────────────────┤
   │ 🌐 Server               │ 🤖 Analisi AI                  │
   │ Framework [Django ▼]    │ Modello  [qwen2.5 ▼]          │
   │ Porta     [8000    ]    │ Focus    [Architettura ▼]      │
   │ Args      [        ]    │                                │
   │ [▶ Avvia] [■ Ferma]     │ [🔍 Analizza] [❓ Cosa fare?] │
   │ ● Avviato su :8000      │                                │
   ├─────────────────────────┴────────────────────────────────┤
   │ [Server ▌] [AI]  — output / streaming                   │
   │  log / risposta AI                                       │
   └──────────────────────────────────────────────────────────┘

   Logica agentica:
   - Progetto piccolo (<40 file .py): analisi diretta → LLM
   - Progetto grande (≥40 file): prima mostra struttura e
     lista moduli → utente sceglie → "Analizza selezione"
   ══════════════════════════════════════════════════════════════ */
class WebDevWidget : public QWidget {
    Q_OBJECT
public:
    explicit WebDevWidget(AiClient* ai, QWidget* parent = nullptr);
    ~WebDevWidget() override;

private:
    AiClient* m_ai = nullptr;

    /* ── Selezione progetto ── */
    QLineEdit*  m_projectPath = nullptr;
    QLabel*     m_detectLbl   = nullptr;
    QString     m_detectedFw;                  // "django"|"flask"|"fastapi"|"generic"

    /* ── Server ── */
    QComboBox*      m_fwCombo      = nullptr;
    QLineEdit*      m_portEdit     = nullptr;
    QLineEdit*      m_argsEdit     = nullptr;
    QPushButton*    m_btnStart     = nullptr;
    QPushButton*    m_btnStop      = nullptr;
    QPushButton*    m_btnOpen      = nullptr;
    QCheckBox*      m_chkDebug     = nullptr;
    QLabel*         m_debugBadge   = nullptr;
    QLabel*         m_serverStatus = nullptr;
    QProcess*       m_serverProc   = nullptr;

    /* ── AI ── */
    QComboBox*   m_modelCombo    = nullptr;
    QComboBox*   m_focusCombo    = nullptr;
    QPushButton* m_btnAnalyze    = nullptr;
    QPushButton* m_btnWhatToDo   = nullptr;

    /* ── Output ── */
    QTabWidget*     m_outTabs       = nullptr;
    QPlainTextEdit* m_serverLog     = nullptr;
    QTextEdit*      m_aiOutput      = nullptr;

    /* ── Pannello selezione moduli (progetto grande) ── */
    QGroupBox*   m_moduleBox     = nullptr;
    QListWidget* m_moduleList    = nullptr;
    QPushButton* m_btnAnalyzeSel = nullptr;

    /* ── Stato analisi ── */
    bool        m_largeProject = false;
    QStringList m_projectFiles;

    /* ── Connessioni AI (una sola attiva alla volta) ── */
    QMetaObject::Connection m_aiTokenConn;
    QMetaObject::Connection m_aiFinishedConn;
    QMetaObject::Connection m_aiErrorConn;

    /* ── Build (Step-Down Rule) ── */
    void     buildLayout();
    QWidget* buildProjectRow(QWidget* parent);
    QWidget* buildControlSplitter(QWidget* parent);
    QGroupBox* buildServerGroup(QWidget* parent);
    QGroupBox* buildAiGroup(QWidget* parent);
    QWidget*   buildOutputArea(QWidget* parent);
    void     setupConnections();

    /* ── Logica ── */
    void        detectFramework(const QString& path);
    void        populateModels();
    QStringList buildRunCommand() const;
    void        scanProject(const QString& path);
    void        runAnalysis(const QStringList& files);
    void        runWhatToDo();
    void        appendServerLog(const QString& text);
    void        setServerRunning(bool running);
    void        setAiBusy(bool busy);
    void        disconnectAi();

    /* ── Slot ── */
    void onBrowseClicked();
    void onStartClicked();
    void onStopClicked();
    void onServerStopTimer();
    void onServerReadyRead();
    void onServerReadyReadStderr();
    void onServerFinished(int code, QProcess::ExitStatus status);
    void onServerErrorOccurred(QProcess::ProcessError err);
    void onOpenBrowser();
    void onAnalyzeClicked();
    void onWhatToDoClicked();
    void onAnalyzeSelClicked();
    void onAiToken(const QString& tok);
    void onAiFinished(const QString& full);
    void onAiError(const QString& msg);
    void onFwComboChanged(int idx);
    void onDebugToggled(bool checked);
    void onPopulateModels();
};
