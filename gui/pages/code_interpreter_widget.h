#pragma once
#include <QWidget>
#include <QRegularExpression>
#include "../ai_client.h"

class QTextEdit;
class QPlainTextEdit;
class QPushButton;
class QLabel;
class QTimer;
class QSplitter;
class QGroupBox;
class QComboBox;

/* ══════════════════════════════════════════════════════════════
   CodeInterpreterWidget — Code Interpreter sandboxato

   Flusso:
   1. Utente descrive cosa vuole → "⚡ Genera e Esegui"
   2. AI genera codice Python puro (sistema: no markdown)
   3. PythonExec esegue il codice (timeout 30s)
   4. Se exit != 0 e tentativi < 3 → AI corregge → ri-esegui
   5. Se matplotlib: immagine PNG mostrata inline

   Usa PythonExec (python_exec.h) per l'esecuzione sandboxata.
   ══════════════════════════════════════════════════════════════ */
class CodeInterpreterWidget : public QWidget {
    Q_OBJECT
public:
    explicit CodeInterpreterWidget(AiClient* ai, QWidget* parent = nullptr);

private:
    void runGenerate();
    void executeStep(const QString& code, int attempt);
    void requestFix(const QString& code, const QString& errSnippet, int attempt);
    void connectAI(std::function<void(const QString&)> onDone);
    void showImage(const QString& path);
    void appendOutput(const QString& text);
    void setRunning(bool running);

    static QString extractCode(const QString& response);
    static QString stripThink(const QString& text);
    QString        patchCode(const QString& code) const;

    AiClient*       m_ai           = nullptr;
    QObject*        m_tokenHolder  = nullptr;

    QComboBox*      m_modelCombo   = nullptr;
    QTextEdit*      m_input        = nullptr;
    QPlainTextEdit* m_codeView     = nullptr;
    QTextEdit*      m_output       = nullptr;
    QLabel*         m_imageLabel   = nullptr;
    QLabel*         m_status       = nullptr;
    QPushButton*    m_btnRun       = nullptr;
    QPushButton*    m_btnStop      = nullptr;
    QPushButton*    m_btnClear     = nullptr;
    QPushButton*    m_btnCopyCode  = nullptr;

    QString         m_aiAccum;
    QString         m_imagePath;
    bool            m_busy         = false;

    static constexpr int kMaxAttempts = 3;
    static constexpr int kTimeoutMs   = 30000;
};
