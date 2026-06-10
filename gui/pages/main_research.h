#pragma once
#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QProcess>
#include <QProgressBar>
#include <QTableWidget>
#include <QDoubleSpinBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QAbstractSocket>
#include <QTcpSocket>
#include <QButtonGroup>
#include <QListWidget>
#include "../ai_client.h"
#include "../widgets/ai_error_widget.h"
#include "../widgets/natal_chart_widget.h"
#include "../widgets/astro_calc.h"
#include "../widgets/world_map_widget.h"
#include "../blhm_engine.h"
#include "../graph_memory.h"
#include "../rag_graph.h"
#include "rab0l_canvas.h"
class QDateEdit;
class QTimeEdit;
class QTextBrowser;
class QFileSystemWatcher;

/* ══════════════════════════════════════════════════════════════
   RicercaPage — "Ricerca e Sviluppo"
   Tab: Paper Scientifico · Brevetto · Doc Tecnico · Cerca Lavoro
        + Bioinformatica: Cytoscape · RDKit · Bioconda
   ══════════════════════════════════════════════════════════════ */
class RicercaPage : public QWidget {
    Q_OBJECT
public:
    explicit RicercaPage(AiClient* ai, QWidget* parent = nullptr);
    ~RicercaPage();

    /** Cross-pollination: espone la GraphMemory del RagGraph per altri moduli. */
    GraphMemory* ragGraphMemory() const { return m_ragGm; }

    /** Auto-trigger: scatta dopo indexingFinished di ImpostazioniPage.
     *  Avvia il RagGraph solo se l'indicizzazione è andata a buon fine. */
    Q_SLOT void onAutoRagTrigger(int nChunks, bool aborted);

private:
    AiClient*    m_ai;
    QTextEdit*   m_outCurrent    = nullptr;
    QPushButton* m_btnGenAttivo  = nullptr;
    QPushButton* m_btnStopAttivo = nullptr;

    /* ── Cytoscape MCP ── */
    QLineEdit*   m_cytoHostEdit  = nullptr;
    QLabel*      m_cytoStatusLbl = nullptr;
    QPushButton* m_cytoExecBtn   = nullptr;
    QComboBox*   m_cytoAction    = nullptr;
    QComboBox*   m_cytoModel     = nullptr;
    QTextEdit*   m_cytoInput     = nullptr;
    QTextEdit*   m_cytoOutput    = nullptr;
    QPushButton* m_cytoRunBtn    = nullptr;
    QPushButton* m_cytoStopBtn   = nullptr;
    QString      m_cytoCode;
    QProcess*    m_cytoProc      = nullptr;
    QTcpSocket*  m_cytoSock      = nullptr;

    /* ── RDKit MCP ── */
    QLabel*      m_rdkitStatusLbl = nullptr;
    QPushButton* m_rdkitExecBtn   = nullptr;
    QComboBox*   m_rdkitAction    = nullptr;
    QComboBox*   m_rdkitModel     = nullptr;
    QTextEdit*   m_rdkitInput     = nullptr;
    QTextEdit*   m_rdkitOutput    = nullptr;
    QPushButton* m_rdkitRunBtn    = nullptr;
    QPushButton* m_rdkitStopBtn   = nullptr;
    QString      m_rdkitCode;
    QProcess*    m_rdkitProc      = nullptr;

    /* ── Bioconda MCP ── */
    QLabel*      m_bioStatusLbl  = nullptr;
    QPushButton* m_bioExecBtn    = nullptr;
    QComboBox*   m_bioAction     = nullptr;
    QComboBox*   m_bioModel      = nullptr;
    QTextEdit*   m_bioInput      = nullptr;
    QTextEdit*   m_bioOutput     = nullptr;
    QPushButton* m_bioRunBtn     = nullptr;
    QPushButton* m_bioStopBtn    = nullptr;
    QString      m_bioCode;
    QProcess*    m_bioProc       = nullptr;

    /* ── Avogadro MCP ── */
    QLabel*      m_avoStatusLbl  = nullptr;
    QPushButton* m_avoExecBtn    = nullptr;
    QComboBox*   m_avoAction     = nullptr;
    QComboBox*   m_avoModel      = nullptr;
    QTextEdit*   m_avoInput      = nullptr;
    QTextEdit*   m_avoOutput     = nullptr;
    QPushButton* m_avoRunBtn     = nullptr;
    QPushButton* m_avoStopBtn    = nullptr;
    QString      m_avoCode;
    QProcess*    m_avoProc       = nullptr;

    /* ── AI streaming per tab science ── */
    AiErrorWidget* m_sciErrorPanel   = nullptr;
    QProgressBar*  m_sciProgress     = nullptr;  ///< progress indeterminata durante AI

    /* stato corrente avviaSci — salvati per retry e slot nominati */
    QTextEdit*   m_sciOut       = nullptr;
    QPushButton* m_sciRunBtn    = nullptr;
    QPushButton* m_sciStopBtn   = nullptr;
    QPushButton* m_sciExecBtn   = nullptr;
    QString*     m_sciCodeRef   = nullptr;
    QLabel*      m_sciStatusLbl = nullptr;
    QComboBox*   m_sciModelCombo = nullptr;
    QString      m_sciSys;
    QString      m_sciUserMsg;

    /* ── Connessioni one-shot science AI ── */
    QMetaObject::Connection m_sciTokenConn;
    QMetaObject::Connection m_sciFinishedConn;
    QMetaObject::Connection m_sciErrorConn;

    /* ── Connessioni one-shot LitAI ── */
    QMetaObject::Connection m_litAiTokenConn;
    QMetaObject::Connection m_litAiFinishedConn;
    QMetaObject::Connection m_litAiErrorConn;

    QWidget* buildPaperTab();
    QWidget* buildBrevettoTab();
    QWidget* buildDocTecnicoTab();
    QWidget* buildCercaLetteraturaTab();
    QWidget* buildCytoscapeTab();
    QWidget* buildRDKitTab();
    QWidget* buildBiocondaTab();
    QWidget* buildAvogadroTab();
    QWidget* buildRab0lTab();
    QWidget* buildBlhmTab();
    QWidget* buildAnalisiPage();
    QWidget* buildAstraleTab();
    QWidget* buildRagGrafoTab();    ///< 🕸️ Grafo Conoscenza RAG
    QWidget* buildRagTesterTab();   ///< 🧪 Test comprensione documenti RAG

    /* ── Grafo RAG ── */
    GraphMemory*       m_ragGm          = nullptr;
    RagGraph*          m_ragGraph       = nullptr;
    QFileSystemWatcher* m_ragDirWatcher = nullptr; ///< auto-trigger su nuovi file (TODO #3)
    QTimer*            m_ragAutoDebounce = nullptr; ///< debounce 2s per il watcher
    QListWidget*    m_ragNodeList    = nullptr;   ///< lista nodi
    QTextEdit*      m_ragNodeDetail  = nullptr;   ///< dettaglio nodo selezionato
    QLabel*         m_ragImgLbl      = nullptr;   ///< immagine Graphviz
    QTextEdit*      m_ragDotView     = nullptr;   ///< DOT sorgente
    QLabel*         m_ragStatus      = nullptr;
    QLabel*         m_ragTempLbl     = nullptr;  ///< indicatore temperatura in-tab RAG
    QProgressBar*   m_ragProgress    = nullptr;
    QPushButton*    m_ragRunBtn      = nullptr;
    QPushButton*    m_ragPauseBtn    = nullptr;
    QPushButton*    m_ragStopBtn     = nullptr;
    QPushButton*    m_ragClearBtn    = nullptr;
    QComboBox*      m_ragModelCombo  = nullptr;
    QLineEdit*      m_ragSearchEdit  = nullptr;
    QProcess*       m_ragDotProc     = nullptr;
    QString         m_ragTmpPng;
    QString         m_ragTmpDot;

    /* ── RAG Tester ── */
    QListWidget* m_ragTesterDocList  = nullptr;
    QTextEdit*   m_ragTesterQuestions = nullptr;
    QTextEdit*   m_ragTesterAnswers   = nullptr;
    QLabel*      m_ragTesterStatus    = nullptr;
    QPushButton* m_ragTesterGenBtn    = nullptr;
    QPushButton* m_ragTesterRunBtn    = nullptr;
    QPushButton* m_ragTesterEvalBtn   = nullptr;
    QString      m_ragTesterDocText;  ///< testo grezzo del documento selezionato

    /* ── Carta Astrale ── */
    QDateEdit*      m_astraleNascita    = nullptr;
    QTimeEdit*      m_astraleOra        = nullptr;
    WorldMapWidget* m_worldMap          = nullptr;
    QLineEdit*      m_astraleCustomCitta = nullptr;
    QLineEdit*      m_astraleCustomLat   = nullptr;
    QLineEdit*      m_astraleCustomLon   = nullptr;
    QButtonGroup*   m_astraleSessoGrp  = nullptr;
    QComboBox*      m_astraleDepth     = nullptr;
    QComboBox*      m_astraleModel     = nullptr;
    QTextEdit*      m_astraleDesc      = nullptr;
    QTextEdit*          m_astraleOutput    = nullptr;
    NatalChartWidget*   m_natalChart       = nullptr;
    QTextBrowser*       m_astraleDoBox     = nullptr;
    QTextBrowser*       m_astraleDontBox   = nullptr;
    QPushButton*        m_astraleRunBtn    = nullptr;
    QPushButton*        m_karmicaBtn       = nullptr;
    QTextEdit*          m_karmicaOutput    = nullptr;
    AstroCalc::Result   m_astroResult;
    QMetaObject::Connection m_astraleTokenConn;
    QMetaObject::Connection m_astraleFinishedConn;
    QMetaObject::Connection m_astraleErrorConn;

    /* ── Analisi Fenomeni ── */
    QButtonGroup*   m_analisiCatGroup    = nullptr;
    QTextEdit*      m_analisiEventEdit   = nullptr;
    QTextEdit*      m_analisiSrcEdit     = nullptr;
    QListWidget*    m_analisiFileList    = nullptr;  ///< file allegati
    QComboBox*      m_analisiModelCombo  = nullptr;
    QTextEdit*      m_analisiOutput      = nullptr;
    QPushButton*    m_analisiRunBtn      = nullptr;
    QPushButton*    m_analisiStopBtn     = nullptr;
    QProgressBar*   m_analisiProbBar     = nullptr;
    QLabel*         m_analisiProbLbl     = nullptr;
    QMetaObject::Connection m_analisiTokenConn;
    QMetaObject::Connection m_analisiFinishedConn;
    QMetaObject::Connection m_analisiErrorConn;

    /* ── RAB₀-L ── */
    Rab0lCanvas* m_rab0lCanvas  = nullptr;
    QLineEdit*   m_rab0lSeq1    = nullptr;
    QLineEdit*   m_rab0lSeq2    = nullptr;
    QLabel*      m_rab0lSimLbl  = nullptr;

    /* ── BLHM ── */
    QTableWidget* m_blhmTable   = nullptr;
    QLineEdit*    m_blhmQuery   = nullptr;
    QTextEdit*    m_blhmOutput  = nullptr;

    /* ── BLHM Note & DNA ── */
    QTextEdit*    m_blhmNoteEdit   = nullptr;
    Rab0lCanvas*  m_blhmDnaCanvas  = nullptr;
    QLineEdit*    m_blhmDnaSeq1    = nullptr;
    QLineEdit*    m_blhmDnaSeq2    = nullptr;
    QLabel*       m_blhmDnaSimLbl  = nullptr;

    /* ── BLHM Engine C (thread pool POSIX, ds4 pattern) ── */
    BLHMGraph*      m_blhmGraph             = nullptr;
    QLabel*         m_blhmEngineStatusLbl   = nullptr;
    QLabel*         m_blhmEngineLatencyLbl  = nullptr;
    QLabel*         m_blhmEngineFactoryLbl  = nullptr;
    QLabel*         m_blhmEngineLinkLbl     = nullptr;
    QLabel*         m_blhmEngineUserLbl     = nullptr;
    QLabel*         m_blhmEngineCombinedLbl = nullptr;
    QDoubleSpinBox* m_blhmEngineLrSpin      = nullptr;
    QTextEdit*      m_blhmEngineAutoftOut   = nullptr;

    /* ── Paper / Brevetto model selectors ── */
    QComboBox*            m_paperModel    = nullptr;
    QComboBox*            m_brevettoModel = nullptr;

    /* ── Cerca Letteratura ── */
    QLineEdit*            m_litQuery      = nullptr;
    QComboBox*            m_litSource     = nullptr;
    QTextEdit*            m_litResults    = nullptr;
    QPushButton*          m_litSearchBtn  = nullptr;
    QPushButton*          m_litAiBtn      = nullptr;
    QLabel*               m_litStatus     = nullptr;
    QNetworkAccessManager* m_litNet       = nullptr;

    void avvia(const QString& sys, const QString& msg,
               QTextEdit* out, QPushButton* btnGen, QPushButton* btnStop);
    void avviaSci(const QString& sys, const QString& userMsg,
                  QTextEdit* out, QPushButton* runBtn, QPushButton* stopBtn,
                  QComboBox* modelCombo,
                  QPushButton* execBtn, QString* codeRef, QLabel* statusLbl);
    void resetButtons();

public slots:
    /* Output bar (PDF / Markdown / Svuota) — accessibili da makeOutputBar() */
    void onOutputBarPdfClicked();
    void onOutputBarMdClicked();
    void onOutputBarClrClicked();
    /** Aggiorna display temperature nel tab RAG; auto-pausa/riprendi ingest termico. */
    void onThermalUpdate(double cpuTempC, double gpuTempC);

private slots:
    /* AI globali */
    void onSciToken(const QString& t);
    void onSciFinished(const QString& full);
    void onSciError(const QString& msg);
    void onAiToken(const QString& t);
    void onAiFinished(const QString& full);
    void onAiError(const QString& err);
    void onAiAborted();
    /* Cerca Letteratura */
    void onLitSearchClicked();
    void onLitReplyFinished();
    void onLitAiClicked();
    void onLitAiToken(const QString& t);
    void onLitAiFinished(const QString& full);
    void onLitAiError(const QString& e);
    /* Cytoscape */
    void onCytoPingClicked();
    void onCytoPingTimeout();
    void onCytoSockConnected();
    void onCytoSockError(QAbstractSocket::SocketError err);
    void onCytoExecClicked();
    void onCytoRunClicked();
    void onCytoStopClicked();
    /* RDKit */
    void onRdkitCheckClicked();
    void onRdkitCheckFinished(int code, QProcess::ExitStatus status);
    void onRdkitExecClicked();
    void onRdkitRunClicked();
    void onRdkitStopClicked();
    /* Bioconda */
    void onBioCheckClicked();
    void onBioCheckFinished(int code, QProcess::ExitStatus status);
    void onBioExecClicked();
    void onBioRunClicked();
    void onBioStopClicked();
    /* Avogadro */
    void onAvoCheckClicked();
    void onAvoCheckFinished(int code, QProcess::ExitStatus status);
    void onAvoExecClicked();
    void onAvoRunClicked();
    void onAvoStopClicked();
    /* RAB₀-L */
    void onRab0lAnalyzeClicked();
    /* BLHM */
    void onBlhmComputeClicked();
    void onBlhmNoteLoad();
    void onBlhmNoteSave();
    void onBlhmDnaAnalyzeClicked();
    /* Analisi Fenomeni */
    void onAnalisiRunClicked();
    void onAnalisiStopClicked();
    void onAnalisiToken(const QString& t);
    void onAnalisiFinished(const QString& full);
    void onAnalisiError(const QString& msg);
    void onAnalisiAddFilesClicked();
    void onAnalisiRemoveFileClicked();
    /* Carta Astrale */
    void onAstraleRunClicked();
    void onAstraleStopClicked();
    void onAstraleRunToggled();
    void onAstraleSavePdf();
    void onAstraleSaveMd();
    void onAstraleClear();
    void onAstraleToken(const QString& t);
    void onAstraleFinished(const QString& full);
    void onAstraleError(const QString& msg);
    void onKarmicaRunClicked();
    void onKarmicaSavePdf();
    void onKarmicaSaveMd();
    void onKarmicaClear();
    void onSalvaChartPng();
    /* RAB₀-L */
    void onRab0lClearClicked();
    /* BLHM */
    void onBlhmAddRowClicked();
    void onBlhmDeleteRowClicked();
    void onBlhmNotesClearClicked();
    void onBlhmDnaClearClicked();
    /* BLHM Engine C */
    void onBlhmEngineSyncFromCalcClicked();
    void onBlhmEngineRunClicked();
    void onBlhmEngineAutoftClicked();
    void onBlhmEngineSaveClicked();
    void onBlhmEngineLoadClicked();

    /* Grafo RAG */
    void onRagRunClicked();
    void onRagPauseClicked();
    void onRagStopClicked();
    void onRagClearClicked();
    void onRagSearchChanged(const QString& q);
    void onRagNodeClicked(QListWidgetItem* item);
    void onRagGraphProgress(int cur, int tot, const QString& file);
    void onRagGraphFinished(const RagGraphStats& stats);
    void onRagGraphMemChanged();
    void onRagDotProcFinished(int code, QProcess::ExitStatus status);
    void onRagRefreshDot();

public:
    static void esportaPdf(QTextEdit* editor,
                           const QString& titolo, QWidget* parent);
    static void salvaMarkdown(QTextEdit* editor,
                              const QString& titolo, QWidget* parent);
};
