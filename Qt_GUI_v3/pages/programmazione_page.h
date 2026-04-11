#pragma once
#include <QWidget>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QProcess>
#include <QSplitter>
#include <QFrame>
#include <QEvent>
#include <QKeyEvent>
#include <QCheckBox>
#include <QSpinBox>
#include "../ai_client.h"

/* ══════════════════════════════════════════════════════════════
   ProgrammazionePage — Genera · Esegui · Salva
   ══════════════════════════════════════════════════════════════
   Flusso:
     1. Utente descrive il task nel campo "Cosa vuoi fare"
     2. Seleziona il linguaggio (o lascia Auto)
     3. Clicca "Genera" → AI produce il codice
     4. Il codice viene estratto e mostrato nel pannello centrale
     5. "▶ Esegui" → QProcess compila/interpreta + mostra output
     6. "💾 Salva" → sceglie cosa esportare (codice / output / prompt / risposta AI)

   Linguaggi supportati per l'esecuzione:
     Python   → python3 /tmp/prisma_prog.py
     C        → gcc -o /tmp/prisma_out /tmp/prisma_prog.c -lm && ./...
     C++      → g++ -std=c++17 -o /tmp/prisma_out /tmp/prisma_prog.cpp && ./...
     JS       → node /tmp/prisma_prog.js
     Bash     → bash /tmp/prisma_prog.sh
   ══════════════════════════════════════════════════════════════ */
class ProgrammazionePage : public QWidget {
    Q_OBJECT
public:
    explicit ProgrammazionePage(AiClient* ai, QWidget* parent = nullptr);

private:
    /* ── Servizi ── */
    AiClient* m_ai;
    QProcess* m_proc = nullptr;   ///< processo compilazione o esecuzione attivo

    /* ── UI pannello superiore (input) ── */
    QPlainTextEdit* m_taskInput  = nullptr;   ///< "cosa vuoi fare"
    QComboBox*      m_cmbLang    = nullptr;   ///< linguaggio (Auto/Python/C/C++/JS/Bash)
    QComboBox*      m_cmbModel   = nullptr;   ///< modello AI per Genera (coding models in cima)
    QPushButton*    m_btnGen     = nullptr;   ///< Genera codice (AI)
    QPushButton*    m_btnStop    = nullptr;   ///< Interrompi AI

    /* ── UI pannello centrale (codice) ── */
    QTextEdit*   m_codeView    = nullptr;   ///< codice estratto (read-only, monospace)
    QPushButton* m_btnRun      = nullptr;   ///< ▶ Esegui
    QPushButton* m_btnKill     = nullptr;   ///< ⬛ Ferma processo
    QPushButton* m_btnSave     = nullptr;   ///< 💾 Salva...
    QPushButton* m_btnCopyCode = nullptr;   ///< 📋 Copia codice
    QLabel*      m_langBadge   = nullptr;   ///< linguaggio rilevato ("Python 3.x")
    QPushButton* m_btnFix      = nullptr;   ///< 🔧 Correggi con AI
    QComboBox*   m_cmbFixModel = nullptr;   ///< modello AI scelto per il fix
    QSpinBox*    m_spinFixIter = nullptr;   ///< "fino a:" N tentativi (0=∞)
    QLabel*      m_fixStatus   = nullptr;   ///< "Sto risolvendo bug..."

    /* ── UI pannello inferiore (output) ── */
    QTextEdit*   m_outputView  = nullptr;   ///< stdout + stderr esecuzione

    /* ── Stato ── */
    QString m_lastCode;        ///< codice estratto dall'ultima risposta AI
    QString m_lastPrompt;      ///< prompt inviato all'AI
    QString m_lastAiResponse;  ///< risposta completa AI (per salvataggio)
    QString m_detectedLang;    ///< linguaggio rilevato dal blocco ```lang
    QString m_plotPath;        ///< path plot matplotlib (/tmp/prisma_plot.png)
    QString m_procOutput;      ///< stdout+stderr accumulato del processo corrente

    /* ── Auto-fix ── */
    int  m_fixAttempts = 0;    ///< tentativi di fix effettuati nella sessione corrente
    bool m_isFix       = false;///< true: onFinished() deve rilanciare runCode()
    /* Il massimo tentativi è ora m_spinFixIter->value() (0 = ∞ = 99 pratico) */

    /* ── Build UI ── */
    QWidget* buildInputPanel();
    QWidget* buildCodePanel();
    QWidget* buildOutputPanel();

    /* ── Logica ── */
    void generateCode();
    /** Estrae il primo blocco ```lang…``` dalla risposta AI */
    void extractCode(const QString& response);
    void runCode();
    void killProcess();
    void saveOutput();
    void appendOutput(const QString& text, bool isError = false);
    void setRunning(bool running);   ///< abilita/disabilita pulsanti durante esecuzione

    /** Costruisce il system prompt per il linguaggio selezionato */
    QString buildSystemPrompt() const;
    /** Invia il codice + errore all'AI per la correzione automatica */
    void autoFixCode(const QString& errOut);
    /** Popola i due combo modello, mettendo i coding-model in cima */
    void populateModelCombos(const QStringList& models);
    /** True se il nome del modello suggerisce specializzazione per codice */
    static bool isCodingModel(const QString& name);

    bool eventFilter(QObject* obj, QEvent* ev) override;

private slots:
    void onToken(const QString& t);
    void onFinished(const QString& full);
    void onError(const QString& msg);
};
