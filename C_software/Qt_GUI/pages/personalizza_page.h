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
    QWidget* buildVramBench();
    QWidget* buildLlamaStudio();
    QWidget* buildCompila();

private:
    /* ── helpers ── */
    QTextEdit*  makeLog(const QString& placeholder = "");
    void        runProc(QProcess* proc, const QString& cmd,
                        QTextEdit* log, QPushButton* btn);

    /* ── VRAM Bench ── */
    QTextEdit*  m_vramLog  = nullptr;
    QPushButton* m_vramBtn = nullptr;
    QProcess*   m_vramProc = nullptr;

    /* ── llama.cpp Studio — sotto-stack ── */
    QStackedWidget* m_llamaStack    = nullptr;
    QTextEdit*      m_llamaLog      = nullptr;
    QPushButton*    m_llamaCompBtn  = nullptr;
    QPushButton*    m_llamaServBtn  = nullptr;
    QLineEdit*      m_llamaModelPath = nullptr;
    QLineEdit*      m_llamaPort      = nullptr;
    QProcess*       m_llamaServProc  = nullptr;

    /* ── Compila ── */
    QTextEdit*   m_compLog    = nullptr;
    QPushButton* m_btnWinTUI  = nullptr;
    QPushButton* m_btnLinTUI  = nullptr;
    QPushButton* m_btnWinQt   = nullptr;
    QPushButton* m_btnLinQt   = nullptr;
};
