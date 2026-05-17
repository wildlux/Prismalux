#pragma once
#include <QWidget>
#include <QDialog>
#include <QAbstractButton>
#include <QListWidgetItem>
#include <QProcess>
#include "../ai_client.h"
#include "../hardware_monitor.h"
#include "../monitor_panel.h"
#include "../rag_engine.h"

class PersonalizzaPage;
class ManutenzioneePage;
class GraficoCanvas;
class QTableWidget;
class QListWidget;
class QLabel;
class QSlider;
class QRadioButton;
class QPushButton;
class QCheckBox;
class QLineEdit;
class QSpinBox;
class QDoubleSpinBox;
class QComboBox;
class QButtonGroup;
class QTextEdit;
class ToggleSwitch;

/* ══════════════════════════════════════════════════════════════
   ImpostazioniPage — Personalizzazione + Manutenzione
   Layout flat: QTabWidget con 2 tab, nessun menu intermedio.
   ══════════════════════════════════════════════════════════════ */
class ImpostazioniPage : public QWidget {
    Q_OBJECT
public:
    explicit ImpostazioniPage(AiClient* ai, HardwareMonitor* hw, QWidget* parent = nullptr);
    void onHWReady(HWInfo hw);

    /** Porta in primo piano il tab il cui titolo contiene @p name (case-insensitive) */
    void switchToTab(const QString& name);

    /** Aggiunge il tab "Grafico" con i controlli di posizione degli assi.
     *  Deve essere chiamato DOPO la costruzione, quando il canvas è disponibile. */
    void setGraficoCanvas(GraficoCanvas* canvas);

    /** Percorso del binario Piper installato localmente (o stringa vuota se assente). */
    static QString piperBinPath();
    /** Cartella voci ~/.prismalux/piper/voices/ */
    static QString piperVoicesDir();
    /** Voce attiva salvata dall'utente (nome file ONNX senza estensione). */
    static QString piperActiveVoice();
    /** Salva la voce attiva. */
    static void    savePiperActiveVoice(const QString& name);

    /** Espone ManutenzioneePage per permettere a StrumentiPage di usare buildCronTab(). */
    ManutenzioneePage* manutenzione() { return m_manutenzione; }

signals:
    /** Emesso quando l'utente cambia la modalità etichette tab (in tempo reale). */
    void tabModeChanged(const QString& mode);
    /** Emesso quando l'utente cambia lo stile di navigazione (in tempo reale). */
    void navStyleChanged(const QString& style);
    /** Emesso quando l'utente cambia la modalità dei pulsanti di esecuzione. */
    void execBtnModeChanged(const QString& mode);
    /** Emesso ogni chunk durante l'indicizzazione (per progress bar in MainWindow). */
    void indexingProgress(int done, int total);
    /** Emesso al completamento o all'interruzione dell'indicizzazione. */
    void indexingFinished(int n, bool aborted);

private slots:
    /* ── buildAiLocaleTab ── */
    void onThinkModeIdClicked(int id);
    void onThinkBudgetChanged(int v);
    void onKnowledgeInjectToggled(bool checked);
    void onOpenKnowledgeFileClicked();
    void onOllamaRadioToggled(bool checked);
    void onLlamaRadioToggled(bool checked);
    void onAiLocalRefreshClicked();
    void onAiModelListItemChanged(QListWidgetItem* cur, QListWidgetItem* prev);
    void onAiModelListDblClicked(QListWidgetItem* item);
    void onAiModelsReadyUpdateLabel(const QStringList& list);
    void onAiLocalTabInit();
    void onOllamaLanBtnClicked();
    void onOllamaLanStarted();
    void onOllamaLanError(QProcess::ProcessError err);
    void onOllamaLanFinished(int code, QProcess::ExitStatus status);
    /* ── buildRagTab ── */
    void onRagNoSaveToggled(bool on);
    void onRagBrowseBtnClicked();
    void onRagDirChanged(const QString& t);
    void onRagMaxResultsChanged(int v);
    void onRagEmbedModelChanged(const QString& v);
    void onRagDownloadBtnClicked();
    void onRagJlToggled(bool checked);
    void onStopIndexClicked();
    void onReindexBtnClicked();
    /* ── buildAiParamsTab ── */
    void onCtxSpinChanged(int ctx);
    void onPersonaBtnClicked(QAbstractButton* btn);
    void onMlockToggled(bool on);
    void onAiParamsSave();
    void onAiParamsSaveStatusClear();
    void onAiParamsReset();
    void onAiPreset8GbClicked();
    void onAiPresetLongClicked();
    void onCavemanToggled(bool on);
    /* ── buildSandboxTab ── */
    void onSandboxEnabledToggled(bool on);
    void onSandboxImageEditFinished();
    void onSandboxMemSpinChanged(int v);
    void onSandboxPullBtnClicked();
    void onSandboxPullProcFinished(int code, QProcess::ExitStatus status);
    /* ── buildTestTab (llm) ── */
    void onTestProcReadyRead();
    void onTestProcFinished(int code, QProcess::ExitStatus status);
    void onTestBuildClicked();
    void onTestRunClicked();
    void onTestStopClicked();
    /* ── buildLlmConsigliatiTab ── */
    void onLlmConsModelChanged(QListWidgetItem* cur, QListWidgetItem* prev);
    void onLlmConsInstallClicked();
    void onLlmConsOllamaToggled(bool on);
    void onLlmConsGgufToggled(bool on);
    void onLlmConsCatToggled(bool on);
    void onLlmConsPopulate();
    void onLlmCustomDlClicked();
    /* ── buildLlmClassificaTab ── */
    void onLlmRankPopulate();
    void onLlmRankCellChanged(int row, int col, int prevRow, int prevCol);
    void onLlmRankInstallClicked();
    /* ── buildTemaTab (visuale) ── */
    void onNavTabModeToggled(bool checked);
    void onNavStyleToggled(bool checked);
    void onExecBtnModeToggled(bool checked);
    /* ── buildMcpTab (altro) ── */
    void onMcpOpenFileClicked();
    void onMcpCopyOllamaCmdClicked();

private:
    QWidget* buildTemaTab();
    QWidget* buildTestTab();
    QWidget* buildVoceTab();
    QWidget* buildTrascriviTab();
    QWidget* buildGraficoTab(GraficoCanvas* canvas);
    QWidget* buildAiLocaleTab();
    QWidget* buildRagTab();
    QWidget* buildSandboxTab();      ///< Docker sandbox per esecuzione codice AI
    QWidget* buildDipendenzeTab();
    QWidget* buildLlmConsigliatiTab();
    QWidget* buildLlmClassificaTab();  ///< ranking oggettivo open-weight (ArtificialAnalysis + benchmark locali)
    QWidget* buildAiParamsTab();   ///< parametri anti-allucinazione + preferenze modello
    QWidget* buildPuliziaTab();       ///< pulizia file temporanei, build, cache
    QWidget* buildMcpTab();           ///< configurazione Model Context Protocol
    QWidget* buildRingraziamentiTab(); ///< licenza MIT + crediti autore e librerie

    /* helper invocato da populateOllama (slot onOllamaRadioToggled / onAiLocalRefreshClicked) */
    void populateOllamaModels();
    /* helper invocato da populateLlama */
    void populateLlamaModels();
    /* helper: applica il modello selezionato in m_aiModelList */
    void applySelectedModel();
    /* helper RAG: aggiorna label stato */
    void refreshRagStatus();

    PersonalizzaPage*  m_personalizza  = nullptr;
    ManutenzioneePage* m_manutenzione  = nullptr;
    QTabWidget*        m_tabs          = nullptr;
    QTabWidget*        m_tabAiLocale   = nullptr;  ///< inner tab: Connessione/HW/RAG/Voce/…
    QTabWidget*        m_tabLlm        = nullptr;  ///< inner tab: LLM/Classifica/Test
    QTabWidget*        m_tabVisuale    = nullptr;  ///< inner tab: Aspetto + Grafico (dinamico)
    QTabWidget*        m_tabSistema    = nullptr;  ///< inner tab: Pulizia/BugTracker/Cron
    AiClient*          m_ai            = nullptr;

    /* RAG indexing state (usato da buildRagTab) */
    RagEngine       m_rag;
    QStringList     m_ragQueue;           ///< chunk da indicizzare
    QStringList     m_ragQueueSource;     ///< nome file sorgente per ogni chunk (parallelo a m_ragQueue)
    int             m_ragQueuePos = 0;    ///< posizione corrente nel queue
    QLabel*         m_ragFeedbackLbl = nullptr;
    bool            m_ragNoSave      = false;  ///< se true, non salva l'indice su disco (M2)
    QPushButton*    m_btnStopIndex   = nullptr; ///< "Ferma indicizzazione" — abilitato durante indexing
    bool            m_indexAborted   = false;   ///< flag settato da "Ferma" per interrompere indexNext

    /* Hardware snapshot — aggiornato da onHWReady() */
    HWInfo          m_hwInfo         = {};
    QTableWidget*   m_rankTable      = nullptr;   ///< ptr tabella classifica (per refreshLlmColors)
    QListWidget*    m_consigliatiList = nullptr;   ///< ptr lista consigliati (per refreshLlmColors)

    void refreshLlmColors();  ///< colora verde/giallo/rosso i modelli in base all'HW rilevato

    /* ── buildAiLocaleTab member state ── */
    QSlider*        m_thinkBudgetSlider = nullptr;
    QLabel*         m_thinkBudgetValLbl = nullptr;
    QLabel*         m_thinkRamWarnLbl   = nullptr;
    QLabel*         m_aiActiveLbl       = nullptr;
    QLabel*         m_aiStatusLbl       = nullptr;
    QLabel*         m_aiHintLbl         = nullptr;
    QListWidget*    m_aiModelList       = nullptr;
    QPushButton*    m_aiUseBtn          = nullptr;
    QRadioButton*   m_btnOllamaRadio    = nullptr;
    QRadioButton*   m_btnLlamaRadio     = nullptr;
    QLabel*         m_lanStatusLbl      = nullptr;
    QPushButton*    m_lanBtn            = nullptr;
    QProcess*       m_ollamaLanProc     = nullptr;

    /* ── buildRagTab member state ── */
    QLineEdit*      m_ragDirEdit        = nullptr;
    QLabel*         m_ragStatusLbl      = nullptr;
    QPushButton*    m_ragReindexBtn     = nullptr;
    QPushButton*    m_ragDownloadBtn    = nullptr;

    /* ── buildAiParamsTab member state ── */
    QDoubleSpinBox* m_tempSpin          = nullptr;
    QDoubleSpinBox* m_topPSpin          = nullptr;
    QSpinBox*       m_topKSpin          = nullptr;
    QDoubleSpinBox* m_repSpin           = nullptr;
    QSpinBox*       m_predSpin          = nullptr;
    QSpinBox*       m_ctxSpin           = nullptr;
    QLabel*         m_ctxHint           = nullptr;
    QCheckBox*      m_honestyCb         = nullptr;
    ToggleSwitch*   m_cavemanToggle     = nullptr;
    QLabel*         m_cavemanBadge      = nullptr;
    QCheckBox*      m_flashCb           = nullptr;
    QLabel*         m_saveStatus        = nullptr;

    /* ── buildSandboxTab member state ── */
    QLineEdit*      m_sandboxImgEdit    = nullptr;
    QSpinBox*       m_sandboxMemSpin    = nullptr;
    QPushButton*    m_sandboxPullBtn    = nullptr;
    QLabel*         m_sandboxPullStatus = nullptr;

    /* ── buildTestTab member state ── */
    QProcess*       m_testProc          = nullptr;
    QTextEdit*      m_testRunOut        = nullptr;
    QPushButton*    m_testBtnBuild      = nullptr;
    QPushButton*    m_testBtnRun        = nullptr;
    QPushButton*    m_testBtnStop       = nullptr;
    QString         m_testBuildDir;
    QString         m_testQtGuiDir;

    /* ── buildLlmConsigliatiTab member state ── */
    QLabel*         m_llmConsDetailLbl  = nullptr;
    QPushButton*    m_llmConsInstallBtn = nullptr;
    QLabel*         m_llmConsLogOut     = nullptr;
    QLineEdit*      m_llmCustomEdit     = nullptr;
    QPushButton*    m_llmCustomDlBtn    = nullptr;
    QRadioButton*   m_llmConsBtnOllama  = nullptr;
    QRadioButton*   m_llmConsBtnGguf    = nullptr;
    QButtonGroup*   m_llmConsCatBtnGrp  = nullptr;

    /* ── buildLlmClassificaTab member state ── */
    QComboBox*      m_llmRankFilterCombo    = nullptr;
    QComboBox*      m_llmRankFilterCatCombo = nullptr;
    QComboBox*      m_llmRankSortCombo      = nullptr;
    QLabel*         m_llmRankDetailLbl      = nullptr;
    QPushButton*    m_llmRankInstallBtn     = nullptr;
    QLabel*         m_llmRankLogLbl         = nullptr;

    /* ── buildMcpTab member state ── */
    QString         m_mcpClaudeCfgPath;

    /* ── buildTemaTab (visuale) member state ── */
    /* nav mode value captured from static array — stored as string */
    QString         m_pendingNavTabMode;
    QString         m_pendingNavStyle;
    QString         m_pendingExecBtnMode;
};
