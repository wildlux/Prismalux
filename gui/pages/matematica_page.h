#pragma once
#include <QWidget>
#include "../ai_client.h"
#include "grafico_page.h"
#include "../widgets/latex_view.h"
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
class QToolButton;
class QScrollArea;
class QStackedWidget;

/* ══════════════════════════════════════════════════════════════
   MatematicaPage — Laboratorio matematico

   Layout:
   ┌────────────────────────────────────────────────────────────┐
   │  🤖 Modello AI: [combo condivisa] [🔄]                     │
   ├────────────────────────────────────────────────────────────┤
   │  QTabWidget  (5 schede)                                    │
   │  ┌──────────┬───────────────┬──────────┬──────┬─────────┐  │
   │  │🔢Sequenza│ π φ e √ const │ N-esimo  │ 🧮   │📐 Risolvi│ │
   │  └──────────┴───────────────┴──────────┴──────┴─────────┘  │
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

    /* ── barra modello condivisa (sopra tutte le tab) ── */
    QComboBox*  m_modelCombo = nullptr;   ///< selezione modello AI — condivisa tra tutte le schede

    /* ── tab Sequenza ── */
    QLineEdit*  m_seqInput   = nullptr;   ///< "1, 4, 9, 16, 25"
    QSpinBox*   m_nextTerms  = nullptr;   ///< quanti termini successivi suggerire
    QLabel*     m_seqResult  = nullptr;   ///< formula rilevata localmente

    /* ── tab Costanti ── */
    QComboBox*  m_constCombo = nullptr;   ///< π  e  φ  √2  √3  √5
    QSpinBox*   m_precSpin   = nullptr;   ///< cifre decimali (1-10000)

    /* ── tab N-esimo ── */
    QComboBox*  m_nthType    = nullptr;   ///< Primo | Fibonacci | Cifra di π | Cifra di e
    QLineEdit*  m_nthInput   = nullptr;   ///< N

    /* ── tab N-esimo ── */
    QLabel*     m_nthDescLbl = nullptr;   ///< descrizione dinamica tipo selezionato

    /* ── tab Espressione ── */
    QLineEdit*  m_exprInput  = nullptr;   ///< "sqrt(2) + sin(pi/4)"
    QSpinBox*   m_exprPrec   = nullptr;   ///< cifre di precisione (default 50)

    /* ── tab Risolvi Passi ── */
    QLineEdit*   m_solveInput   = nullptr;
    QComboBox*   m_solveCmb     = nullptr;
    QPushButton* m_btnSolve     = nullptr;
    QPushButton* m_btnSolveCopy = nullptr;
    QPushButton* m_btnSolveAi   = nullptr;  ///< "Spiega con AI" (appare dopo SymPy)
    bool         m_solveBusy    = false;
    bool         m_solvePyMode  = false;    ///< true quando proc SymPy è in esecuzione
    QString      m_solveFullText;           ///< cattura output SymPy (letto da onSolveAiClicked)
    int          m_solveTabIdx  = -1;

    /* ── tab Analisi 1 ── */
    QComboBox*     m_a1TopicCmb  = nullptr;
    LatexView*     m_a1Theory    = nullptr;
    QLineEdit*     m_a1Input     = nullptr;
    QComboBox*     m_a1TypeCmb   = nullptr;
    QLineEdit*     m_a1PlotInput = nullptr;  ///< espressione per il grafico
    GraficoCanvas* m_a1Canvas    = nullptr;  ///< canvas interattivo (zoom/pan)
    QPushButton*   m_btnA1Expand = nullptr;  ///< apri in finestra separata

    /* ── highlight zona limite attiva ── */
    GraficoCanvas* m_limitCanvas = nullptr;  ///< canvas su cui è attivo il highlight limite

    /* ── tab Analisi 2 ── */
    QComboBox*     m_a2TopicCmb  = nullptr;
    LatexView*     m_a2Theory    = nullptr;
    QLineEdit*     m_a2Input     = nullptr;
    QComboBox*     m_a2TypeCmb   = nullptr;
    QLineEdit*     m_a2PlotInput = nullptr;
    GraficoCanvas* m_a2Canvas    = nullptr;
    QComboBox*     m_a2RenderCmb = nullptr;  ///< Punti / Wireframe / Superficie
    QPushButton*   m_btnA2Expand = nullptr;  ///< apri in finestra separata

    /* builder schede */
    QWidget* buildSeqTab();
    QWidget* buildConstTab();
    QWidget* buildNthTab();
    QWidget* buildExprTab();
    QWidget* buildSolveTab();
    QWidget* buildAnalisi1Tab();
    QWidget* buildAnalisi2Tab();
    QWidget* buildSymbolBar();

    /* grafici — usa GraficoCanvas nativo */
    static QString sympyToCanvas(const QString& expr);
    static QVector<GraficoCanvas::Pt3D> buildSurface3D(const QString& exprSym);

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

    /* combo modelli matematici */
    void        fillMathCombo(const QStringList& list, const QString& cur);
    void        fetchAndFillMathModels();  ///< fetchModels() + fillMathCombo via one-shot holder

    /* pannello LaTeX output (rendering risposta AI) */
    LatexView*      m_latexOut   = nullptr;

    /* barra simboli LaTeX */
    QLineEdit*      m_symTarget  = nullptr;  ///< input attivo per inserimento simbolo
    QStackedWidget* m_symStack   = nullptr;

private slots:
    void onSymBtnClicked();
    void onSymCatChanged(int idx);

    /* output bar */
    void onCopyClicked();
    void onClearOutputClicked();
    void onStopClicked();

    /* sincronizzazione modello */
    void onAiModelChanged(const QString& newModel);

    /* tab Sequenza */
    void onRefreshModelsClicked();
    void onLoadModelsOnce();
    void onLocalPatternClicked();
    void onSympyClicked();
    void onAnalyzeAiClicked();

    /* tab Costanti */
    void onConstantCalcClicked();
    void onAllConstantsClicked();

    /* tab N-esimo */
    void onNthCalcClicked();
    void onNthTypeChanged();

    /* tab Espressione */
    void onExampleClicked();          ///< sender()->property("mathExpr")
    void onExprEvalClicked();
    void onSimplifyClicked();
    void onExprReturnPressed();

    /* AI sequenza (holder one-shot) */
    void onAiSeqToken(const QString& tok);
    void onAiSeqFinished(const QString& full);
    void onAiSeqError(const QString& msg);

    /* tab Risolvi Passi */
    void onSolveClicked();
    void onSolveRandomClicked();    ///< 🔀 pesca formula casuale dall'archivio
    void onSolveStopClicked();
    void onSolveCopyClicked();
    void onSolveRestoreCopyBtn();
    void onSolveAiClicked();
    void onSolveToken(const QString& t);
    void onSolveFinished(const QString& full);
    void onSolveError(const QString& msg);

    /* tab Analisi 1 */
    void onA1TopicChanged();
    void onA1TryClicked();
    void onA1AiClicked();
    void onA1PlotClicked();
    void onA1ExpandClicked();

    /* tab Analisi 2 */
    void onA2TopicChanged();
    void onA2TryClicked();
    void onA2AiClicked();
    void onA2PlotClicked();
    void onA2RenderChanged(int idx);
    void onA2ExpandClicked();


    /* AI Analisi (one-shot shared) */
    void onAnalisiAiToken(const QString& tok);
    void onAnalisiAiFinished(const QString& full);
    void onAnalisiAiError(const QString& msg);

    /* QProcess Python */
    void onProcReadyRead();
    void onProcFinished(int code, QProcess::ExitStatus status);

private:
    /* holder one-shot per i segnali AI durante runAiSequence */
    QObject* m_aiSeqHolder    = nullptr;
    /* holder one-shot per i segnali AI durante Risolvi Passi */
    QObject* m_aiSolveHolder  = nullptr;
    /* holder one-shot per i segnali AI durante Analisi 1/2 */
    QObject* m_aiAnalisiHolder = nullptr;
};
