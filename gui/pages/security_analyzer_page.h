#pragma once
#include <QWidget>
#include <QVector>
#include <QMap>
#include "../ai_client.h"

class QComboBox;
class QTextEdit;
class QPushButton;
class QLabel;
class QProgressBar;
class QTabWidget;
class QPlainTextEdit;

/* ══════════════════════════════════════════════════════════════
   SecurityFinding — singola vulnerabilità trovata da un agente
   ══════════════════════════════════════════════════════════════ */
struct SecurityFinding {
    QString agent;
    QString severity;   ///< critico | alto | medio | basso | info
    QString issue;
    QString location;
    QString remedy;
};

/* ══════════════════════════════════════════════════════════════
   SecurityAnalyzerPage — analizzatore difensivo con 4 agenti Ollama
   in parallelo, ciascuno specializzato in una categoria di rischio.

   Layout:
   ┌─────────────────────────────────────────────────────────────┐
   │ Titolo + Modello + [Analizza] [Stop]                        │
   ├──────────────────────┬──────────────────────────────────────┤
   │  Input codice (40%)  │  Tab Report | Dettagli (60%)         │
   ├──────────────────────┴──────────────────────────────────────┤
   │  Status + ProgressBar                                       │
   └─────────────────────────────────────────────────────────────┘
   ══════════════════════════════════════════════════════════════ */
class SecurityAnalyzerPage : public QWidget {
    Q_OBJECT
public:
    explicit SecurityAnalyzerPage(AiClient* ai, QWidget* parent = nullptr);
    ~SecurityAnalyzerPage() override;

private:
    static constexpr int kNumAgents = 4;

    AiClient*          m_ai   = nullptr;
    QVector<AiClient*> m_pool;

    /* UI */
    QComboBox*      m_modelCombo  = nullptr;
    QTextEdit*      m_codeInput   = nullptr;
    QPushButton*    m_btnAnalyze  = nullptr;
    QPushButton*    m_btnStop     = nullptr;
    QLabel*         m_status      = nullptr;
    QProgressBar*   m_progress    = nullptr;
    QTabWidget*     m_outputTabs  = nullptr;
    QPlainTextEdit* m_reportOutput = nullptr;  ///< tab "Report"
    QPlainTextEdit* m_rawOutput    = nullptr;  ///< tab "Dettagli"

    /* State */
    int              m_pendingCount = 0;
    QVector<QString> m_agentResults;
    QMap<int, QObject*> m_holders;

    /* helpers */
    void initPool();
    void runAgents(const QString& code);
    void runAgent(int idx, AiClient* client, const QString& code);
    void onAgentFinished(int idx, const QString& result);
    void onAgentError(int idx, const QString& msg);
    void synthesize();
    void setStatus(const QString& s);

private slots:
    void onAnalyzeClicked();
    void onStopClicked();
    void onFillModels();
};
