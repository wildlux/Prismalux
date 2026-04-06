#pragma once
#include <QWidget>
#include "../ai_client.h"
#include "../widgets/chart_widget.h"

class QComboBox;
class QPlainTextEdit;
class QTextEdit;
class QLineEdit;
class QPushButton;
class QLabel;
class QProcess;
class QSplitter;
class QGroupBox;

/* ══════════════════════════════════════════════════════════════
   ProgrammazionePage — Editor codice + esecuzione + grafico + AI

   Layout verticale:
   ┌─────────────────────────────────────────────────────┐
   │  Toolbar: [lang] [▶ Esegui] [■ Stop] [🗑] [📄] [🤖]│
   ├───────────────────────────┬─────────────────────────┤
   │  📝 Codice (editor)        │  📟 Output (testo)      │
   │                            ├─────────────────────────┤
   │                            │  📈 Grafico (auto-show)  │
   ├────────────────────────────┴─────────────────────────┤
   │  🤖 AI: [richiesta ......] [Invia ▶] [↑ in editor]  │
   │  (risposta AI streaming)               [✕ Chiudi]   │
   └─────────────────────────────────────────────────────┘

   Il pannello AI è nascosto finché l'utente non clicca [🤖].
   Il grafico è nascosto finché l'output non contiene numeri.
   ══════════════════════════════════════════════════════════════ */
class ProgrammazionePage : public QWidget {
    Q_OBJECT
public:
    explicit ProgrammazionePage(AiClient* ai, QWidget* parent = nullptr);

private:
    AiClient*       m_ai        = nullptr;

    /* Editor */
    QComboBox*      m_lang      = nullptr;
    QPlainTextEdit* m_editor    = nullptr;

    /* Output */
    QPlainTextEdit* m_output    = nullptr;
    ChartWidget*    m_chart     = nullptr;
    QLabel*         m_status    = nullptr;
    QString         m_fullOutput;        ///< tutto l'output dell'ultima esecuzione

    /* AI assistant panel */
    QWidget*        m_aiPanel    = nullptr;
    QLineEdit*      m_aiInput    = nullptr;
    QPlainTextEdit* m_aiOutput   = nullptr;
    QPushButton*    m_btnInsert  = nullptr; ///< "↑ Inserisci codice in editor"
    QComboBox*      m_modelCombo = nullptr; ///< selezione modello AI per la sessione coding

    /* Process */
    QProcess*       m_proc      = nullptr;
    bool            m_aiMode    = false;

    /* Token connection holder — singolo, sostituisce il pattern holder locale.
       Garantisce che ci sia sempre al massimo UNA connessione a m_ai->token.
       Viene resettato esplicitamente prima di creare una nuova connessione. */
    QObject*        m_tokenHolder = nullptr;
    QPushButton*    m_btnSend   = nullptr;   ///< "Invia ▶" nel pannello AI (disabilitato durante streaming)

    /* Toolbar buttons */
    QPushButton*    m_btnRun    = nullptr;
    QPushButton*    m_btnStop   = nullptr;
    QPushButton*    m_btnAi     = nullptr;
    QPushButton*    m_btnFix    = nullptr; ///< "🔧 Correggi con AI" — invia il codice + errore al coder
    QPushButton*    m_btnLint   = nullptr; ///< "🔍 Analizza" — linting statico pre-AI

    /* Pannello diff (mostrato dopo ogni correzione AI) */
    QGroupBox*      m_diffGroup = nullptr;
    QTextEdit*      m_diffView  = nullptr;

    /* Stato esecuzione e lint */
    int             m_lastExitCode = 0;
    QString         m_lastError;           ///< errore esecuzione o output lint (passato al fix)
    QString         m_originalCode;        ///< codice salvato prima del fix AI (per il diff)

    QString tempFilePath(const QString& ext) const;
    QString buildRunCommand(const QString& filePath) const;
    QString currentTemplate() const;
    void    appendOutput(const QString& text);
    void    setRunning(bool running);
    void    tryShowChart();
    QString extractCodeBlock() const;
    void    selectCoderModel();
    void    triggerFix(bool includeError);
    void    _doFix(bool includeError, const QString& codice,
                   const QString& lang, const QString& ext);
    void    runLint();                ///< analisi statica con linter del linguaggio corrente
    void    showDiff(const QString& before, const QString& after); ///< diff colorato pre/post fix
    static QVector<double> parseNumbers(const QString& text);
};
