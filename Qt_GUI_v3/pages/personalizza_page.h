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

private:
    /* ── layout builder ── */
    QWidget* buildMenu();
    QWidget* buildVramBench();
    QWidget* buildLlamaStudio();
    QWidget* buildCythonStudio();
    QWidget* buildCompila();

    /* ── helpers ── */
    QWidget*    makeBackBar(const QString& title);
    QTextEdit*  makeLog(const QString& placeholder = "");
    void        runProc(QProcess* proc, const QString& cmd,
                        QTextEdit* log, QPushButton* btn);

    QStackedWidget* m_stack = nullptr;

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

    /* ── Cython Studio ── */
    QTextEdit*   m_cythoLog = nullptr;
    QPushButton* m_cythoBtn = nullptr;
    QLabel*      m_cythoFile = nullptr;
    QString      m_cythoPath;

    /* ── Compila ── */
    QTextEdit*   m_compLog    = nullptr;
    QPushButton* m_btnWinTUI  = nullptr;
    QPushButton* m_btnLinTUI  = nullptr;
    QPushButton* m_btnWinQt   = nullptr;
    QPushButton* m_btnLinQt   = nullptr;
};
