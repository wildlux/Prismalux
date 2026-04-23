#pragma once
#include <QWidget>
#include <QStackedWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QProcess>
#include <QLineEdit>

/* ══════════════════════════════════════════════════════════════
   PersonalizzaPage — Personalizzazione Avanzata Estrema
   Sotto-sezioni navigate via QStackedWidget interno
   ══════════════════════════════════════════════════════════════ */
class PersonalizzaPage : public QWidget {
    Q_OBJECT
public:
    explicit PersonalizzaPage(QWidget* parent = nullptr);

    /* Esposte per ImpostazioniPage che le aggiunge come tab dirette */
    QWidget* buildLlamaStudio();

private:
    /* ── helpers ── */
    QTextEdit*  makeLog(const QString& placeholder = "");
    /** runProcArgs — versione SICURA: arglist separata, nessuna shell. */
    void        runProcArgs(QProcess* proc, const QString& program,
                            const QStringList& args,
                            QTextEdit* log, QPushButton* btn);
    /** runProc — DEPRECATO: usa sh -c. Solo per path interni, non per input utente. */
    void        runProc(QProcess* proc, const QString& cmd,
                        QTextEdit* log, QPushButton* btn);

    /* ── llama.cpp Studio — sotto-stack ── */
    QStackedWidget* m_llamaStack    = nullptr;
    QTextEdit*      m_llamaLog      = nullptr;
    QPushButton*    m_llamaCompBtn  = nullptr;
    QPushButton*    m_llamaServBtn  = nullptr;
    QLineEdit*      m_llamaModelPath = nullptr;
    QLineEdit*      m_llamaPort      = nullptr;
    QProcess*       m_llamaServProc  = nullptr;

};
