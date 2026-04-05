#pragma once
#include <QWidget>
#include "../ai_client.h"

class QTabWidget;
class QPlainTextEdit;
class QLineEdit;
class QSpinBox;
class QComboBox;
class QPushButton;
class QLabel;
class QProcess;
class QSplitter;
class QFileInfo;

/* ══════════════════════════════════════════════════════════════
   MatematicaPage — Laboratorio matematico

   Layout:
   ┌────────────────────────────────────────────────────────────┐
   │  QTabWidget  (4 schede)                                    │
   │  ┌──────────┬───────────────┬──────────┬──────────────┐   │
   │  │🔢Sequenza│ π φ e √ const │ N-esimo  │ 🧮 Espressione│  │
   │  └──────────┴───────────────┴──────────┴──────────────┘   │
   ├────────────────────────────────────────────────────────────┤
   │  Output (QPlainTextEdit monosp. scrollabile)               │
   │  [📋 Copia]  [🗑 Cancella]                                 │
   └────────────────────────────────────────────────────────────┘

   Tutto il calcolo pesante avviene via subprocess python3
   (mpmath + sympy già installati).
   ══════════════════════════════════════════════════════════════ */
class MatematicaPage : public QWidget {
    Q_OBJECT
public:
    explicit MatematicaPage(AiClient* ai, QWidget* parent = nullptr);

private:
    AiClient*       m_ai       = nullptr;
    QTabWidget*     m_tabs     = nullptr;
    QPlainTextEdit* m_output   = nullptr;
    QLabel*         m_status   = nullptr;
    QProcess*       m_proc     = nullptr;
    bool            m_aiRunning = false;

    /* ── tab Sequenza ── */
    QLineEdit*  m_seqInput   = nullptr;   ///< "1, 4, 9, 16, 25"
    QSpinBox*   m_nextTerms  = nullptr;   ///< quanti termini successivi suggerire
    QLabel*     m_seqResult  = nullptr;   ///< formula rilevata localmente
    QComboBox*  m_modelCombo = nullptr;   ///< selezione modello AI per Analizza con AI

    /* ── tab Costanti ── */
    QComboBox*  m_constCombo = nullptr;   ///< π  e  φ  √2  √3  √5
    QSpinBox*   m_precSpin   = nullptr;   ///< cifre decimali (1-10000)

    /* ── tab N-esimo ── */
    QComboBox*  m_nthType    = nullptr;   ///< Primo | Fibonacci | Cifra di π | Cifra di e
    QLineEdit*  m_nthInput   = nullptr;   ///< N

    /* ── tab Espressione ── */
    QLineEdit*  m_exprInput  = nullptr;   ///< "sqrt(2) + sin(pi/4)"
    QSpinBox*   m_exprPrec   = nullptr;   ///< cifre di precisione (default 50)

    /* builder schede */
    QWidget* buildSeqTab();
    QWidget* buildConstTab();
    QWidget* buildNthTab();
    QWidget* buildExprTab();

    /* azioni */
    void runSequence();
    void runConstant();
    void runNth();
    void runExpr();
    void runAiSequence(const QString& seqStr, int nextN);

    /* helpers */
    void        appendOutput(const QString& text);
    void        clearOutput();
    void        setStatus(const QString& msg);
    void        runPython(const QString& code);   ///< lancia python3 -c CODE, output → m_output
    void        stopPython();

    /* import file → sequenza */
    void        importFromFile();
    QString     extractNumbersFromFile(const QString& path, QString& err);
    QString     numbersFromText(const QString& raw) const;
    QString     _runPythonSync(const QString& code, QString& err);

    /* rilevamento locale dei pattern (veloce, senza AI né subprocess) */
    QString     detectPatternLocal(const QVector<double>& seq) const;
    QVector<double> parseSeq(const QString& s, QString& err) const;
};
