#pragma once
#include <QWidget>
#include <QDialog>
#include <QFutureWatcher>
#include <QPair>
#include <QMap>
#include <QList>
#include <QScrollArea>
#include <QAbstractButton>
#include <QListWidgetItem>
#include <QProcess>
#include "../ai_client.h"
#include "../hardware_monitor.h"
#include "../monitor_panel.h"
#include "../rag_engine.h"
#include "../widgets/zoomable_image_view.h"
#ifdef HAVE_QT_MULTIMEDIA
#  include <QAudioSource>
#  include <QAudioFormat>
#  include <QAudioDevice>
#  include <QMediaDevices>
#endif

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
class QProgressBar;
class QComboBox;
class QButtonGroup;
class QTextEdit;
class QGroupBox;
class QVBoxLayout;
class QFrame;
class ToggleSwitch;

/* ══════════════════════════════════════════════════════════════
   ImpostazioniPage — Personalizzazione + Manutenzione
   Layout flat: QTabWidget con 2 tab, nessun menu intermedio.
   ══════════════════════════════════════════════════════════════ */
class ImpostazioniPage : public QWidget {
    Q_OBJECT
public:
    explicit ImpostazioniPage(AiClient* ai, HardwareMonitor* hw, QWidget* parent = nullptr);
    ~ImpostazioniPage() override;
    void onHWReady(HWInfo hw);

    /** Porta in primo piano il tab il cui titolo contiene @p name (case-insensitive) */
    void switchToTab(const QString& name);

    /** Apre il tab "Moduli Python" e, se specificato, evidenzia il pacchetto @p pipPkg */
    void jumpToPipInstall(const QString& pipPkg = {});

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


    /** Tipo risultato estrazione testo (chunk, sorgenti, mtime) — pubblico per il thread worker. */
    struct RagExtractResult {
        QStringList   chunks;   ///< testi dei chunk
        QStringList   sources;  ///< percorso file sorgente per ogni chunk (pieno)
        QList<qint64> mtimes;   ///< lastModified msecs del file sorgente per ogni chunk
    };

    /** Avvia l'indicizzazione RAG se l'indice è vuoto e la cartella default ha file.
     *  Pensato per essere chiamato una volta all'avvio (es. via QTimer::singleShot). */
    void autoIndexIfEmpty();

protected:
    /** Middle-click sulla lista modelli (Gestione LLM) → copia il nome
     *  del modello negli appunti (clipboard + selezione X11). */
    bool eventFilter(QObject* watched, QEvent* ev) override;

signals:
    /** Emesso quando l'utente cambia la modalità etichette tab (in tempo reale). */
    void tabModeChanged(const QString& mode);
    /** Emesso quando l'utente cambia lo stile di navigazione (in tempo reale). */
    void navStyleChanged(const QString& style);
    /** Emesso quando l'utente cambia la modalità dei pulsanti di esecuzione. */
    void execBtnModeChanged(const QString& mode);
    /** Emesso quando l'utente cambia il raggio delle bolle chat (applicazione in tempo reale). */
    void bubbleStyleChanged();
    /** Emesso ogni chunk durante l'indicizzazione (per progress bar in MainWindow). */
    void indexingProgress(int done, int total);
    /** Emesso al completamento o all'interruzione dell'indicizzazione. */
    void indexingFinished(int n, bool aborted);

private slots:
    /* ── ricerca tab ── */
    void onSearchChanged(const QString& text);
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
    void onLanguageChanged(int idx);
    void onRagDirChanged(const QString& t);
    void onRagMaxResultsChanged(int v);
    void onRagEmbedModelChanged(const QString& v);
    void onRagEmbedComboChanged(int idx);
    void onRagEmbedRefreshClicked();
    void onRagEmbedModelsReady(const QStringList& models);
    void onRagJlToggled(bool checked);
    void onRagFileListRefreshClicked();
    void onRagFileItemChanged(QListWidgetItem* item);
    void onRagFileSelectAllClicked();
    void onRagFileDeselectAllClicked();
    /* ── buildSmartRouterTab / buildJlTab ── */
    void onSmartRouterSaveClicked();
    void onStopIndexClicked();
    void onReindexBtnClicked();
    /* ── buildAiParamsTab ── */
    void onCtxSpinChanged(int ctx);
    void onPersonaBtnClicked(QAbstractButton* btn);
    void onMlockToggled(bool on);
    void onRpcToggled(bool on);
    void onRpcNodesEditFinished();
    void onRpcCheckClicked();
    void onRpcSshUserEditFinished();
    void onRpcPathEditFinished();
    void onRpcStartNodesClicked();
    void onRpcStopNodesClicked();
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
    void onDockerUnlockClicked();
    void onDockerUnlockProcFinished(int code, QProcess::ExitStatus status);
    void refreshDockerStatusCard();
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
    /* ── buildBenchmarkLocaleTab ── */
    void onBenchmarkRunClicked();
    void onBenchmarkProcReadyRead();
    void onBenchmarkProcFinished(int code, QProcess::ExitStatus status);

private:
    /* ── ricerca tab: struttura indice ── */
    struct SearchEntry {
        int     outer = -1;
        int     inner = -1;
        QString label;
    };
    void buildSearchIndex();
    QList<SearchEntry> m_searchIndex;

    QWidget* buildTemaTab();
    QWidget* buildTestTab();
    /* Sezioni di buildTestTab (D-35: estrazione meccanica) */
    void     buildTestRunSection(QFrame* leftPanel, QVBoxLayout* llay);
    QWidget* buildTestDetailPage(int i);
    QWidget* buildVoceTab();
    QWidget* buildTrascriviTab();
    /* Sezioni di buildTrascriviTab (D-35: estrazione meccanica, una per card) */
    void buildTrascriviModeSection(QGroupBox* inner, QVBoxLayout* ilay);
    void buildTrascriviBinSection(QGroupBox* inner, QVBoxLayout* ilay);
    void buildTrascriviModelSections(QGroupBox* inner, QVBoxLayout* ilay);
    void buildTrascriviHttpSection(QGroupBox* inner, QVBoxLayout* ilay);
    void buildTrascriviDiarizeSection(QGroupBox* inner, QVBoxLayout* ilay);
    void buildTrascriviNoteDepsSection(QGroupBox* inner, QVBoxLayout* ilay);
    void buildTrascriviMicSection(QGroupBox* inner, QVBoxLayout* ilay);
    QWidget* buildGraficoTab(GraficoCanvas* canvas);
    QWidget* buildAiLocaleTab();
    QWidget* buildRagTab();
    QWidget* buildSandboxTab();      ///< Docker sandbox per esecuzione codice AI
    QWidget* buildDipendenzeTab();
    QWidget* buildPythonDepsTab();
    QWidget* buildLlmConsigliatiTab();
    QWidget* buildLlmClassificaTab();  ///< ranking oggettivo open-weight (ArtificialAnalysis + benchmark locali)
    QWidget* buildBenchmarkLocaleTab(); ///< risultati benchmark temperatura "su strada" (2026-06-15)
    QWidget* buildAiParamsTab();   ///< parametri anti-allucinazione + preferenze modello
    QWidget* buildPuliziaTab();       ///< pulizia file temporanei, build, cache
    QWidget* buildMcpTab();           ///< configurazione Model Context Protocol
    QWidget* buildRingraziamentiTab(); ///< licenza MIT + crediti autore e librerie
    QWidget* buildFeedbackTab();        ///< analytics 👍/👎 + export DPO dataset
    QWidget* buildAiMemoryTab();      ///< AIMemory storia preferenze (gitLog + revertFile)
    QWidget* buildSistemaConsigliTab(); ///< consigli ottimizzazione hardware/temperatura
    QWidget* buildLinguaTab();          ///< selettore lingua interfaccia (it/en)
    QWidget* buildSicurezzaWanTab();  ///< TLS WAN cert + pin + WireGuard keys

    /* Gruppi lazy — costruiti solo al primo clic sulla tab esterna
       corrispondente (vedi LazyTabLoader in settings_main.cpp) */
    QWidget* buildGroupLlm();
    QWidget* buildGroupSistema();
    QWidget* buildMcpGroupTab(QTabWidget* parentTabs);      ///< "MCP": Configurazione + Gestione, 2 sotto-tab
    QWidget* buildProfiliGroupTab(QTabWidget* parentTabs);  ///< "Profili Modello": Profili + Le tue preferenze

    /* helper invocato da populateOllama (slot onOllamaRadioToggled / onAiLocalRefreshClicked) */
    void populateOllamaModels();
    /* helper invocato da populateLlama */
    void populateLlamaModels();
    /* helper: applica il modello selezionato in m_aiModelList */
    void applySelectedModel();
    /* helper RAG: aggiorna label stato */
    void refreshRagStatus();
    void refreshRagFileList();   ///< D-47: lista file cartella RAG con spunte
    void setRagFileChecksAll(bool checked);  ///< spunta/despunta tutti + persiste esclusioni
    void updateRagFileCardCounts();          ///< D-66: conteggi nel titolo card file RAG
    /* helper RAG: avvia la fase di embedding (chiamata dopo l'estrazione testo) */
    void startEmbeddingPhase(const QString& dir);

    PersonalizzaPage*  m_personalizza  = nullptr;
    ManutenzioneePage* m_manutenzione  = nullptr;
    QTabWidget*        m_tabs          = nullptr;
    QLineEdit*         m_searchEdit    = nullptr;
    QTimer*            m_searchTimer   = nullptr;
    QTabWidget*        m_tabAiLocale   = nullptr;  ///< inner tab: Connessione/HW/RAG/Voce/…
    QTabWidget*        m_tabLlm        = nullptr;  ///< inner tab: LLM/Classifica/Test
    QTabWidget*        m_tabVisuale    = nullptr;  ///< inner tab: Aspetto + Grafico (dinamico)
    QTabWidget*        m_tabSistema    = nullptr;  ///< inner tab: Pulizia/BugTracker/Cron
    QTabWidget*        m_tabMcp        = nullptr;  ///< inner tab: Configurazione/Gestione MCP
    QTabWidget*        m_tabProfili    = nullptr;  ///< inner tab: Profili/Le tue preferenze
    AiClient*          m_ai            = nullptr;

    /* RAG indexing state (usato da buildRagTab) */
    QFutureWatcher<RagExtractResult>* m_extractWatcher = nullptr;
    RagEngine       m_rag;
    QStringList     m_ragQueue;           ///< chunk da indicizzare
    QStringList     m_ragQueueSource;     ///< percorso file sorgente per ogni chunk (pieno)
    QList<qint64>   m_ragQueueMtime;      ///< mtime del file per ogni chunk (parallelo a m_ragQueue)
    int             m_ragQueuePos = 0;    ///< posizione corrente nel queue
    QLabel*         m_ragFileCardTitle = nullptr;  ///< D-66: titolo card file RAG con conteggi
    QLabel*         m_ragFeedbackLbl    = nullptr;
    QProgressBar*   m_ragProgressBar    = nullptr;
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

    /* ── Smart Router member state ── */
    QCheckBox*      m_smartRouterChk         = nullptr;
    QLineEdit*      m_cloudUrlEdit           = nullptr;
    QLineEdit*      m_cloudModelEdit         = nullptr;
    QLineEdit*      m_cloudApiKeyEdit        = nullptr;
    QLabel*         m_smartRouterStatusLbl   = nullptr;

    /* ── buildRagTab member state ── */
    QLineEdit*      m_ragDirEdit        = nullptr;
    QLabel*         m_ragStatusLbl      = nullptr;
    QPushButton*    m_ragReindexBtn     = nullptr;
    QListWidget*    m_ragFileList       = nullptr;  ///< D-47: file cartella RAG con spunte
    QComboBox*      m_ragEmbedCombo     = nullptr;  ///< selettore modello embedding
    QComboBox*      m_langCombo         = nullptr;  ///< selettore lingua interfaccia (it/en)
    QLabel*         m_langRestartHint   = nullptr;  ///< avviso "riavvia per applicare la lingua"

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
    QCheckBox*      m_rpcCb             = nullptr;
    QLineEdit*      m_rpcNodesEdit      = nullptr;
    QLabel*         m_rpcStatusLbl      = nullptr;
    QLineEdit*      m_rpcSshUserEdit    = nullptr;
    QLineEdit*      m_rpcPathEdit       = nullptr;
    QPushButton*    m_rpcStartBtn       = nullptr;
    QPushButton*    m_rpcStopBtn        = nullptr;

    /* ── buildSandboxTab member state ── */
    QLineEdit*      m_sandboxImgEdit    = nullptr;
    QSpinBox*       m_sandboxMemSpin    = nullptr;
    QPushButton*    m_sandboxPullBtn    = nullptr;
    QLabel*         m_sandboxPullStatus = nullptr;
    QLabel*         m_dockerStatusIcon  = nullptr;
    QLabel*         m_dockerStatusDesc  = nullptr;
    QPushButton*    m_dockerUnlockBtn   = nullptr;
    QProcess*       m_dockerUnlockProc  = nullptr;

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

    /* ── buildBenchmarkLocaleTab member state ── */
    QProcess*       m_benchmarkProc         = nullptr;
    ZoomableImageView* m_benchmarkImgLbl    = nullptr;
    QPushButton*    m_benchmarkRunBtn       = nullptr;
    QLabel*         m_benchmarkStatusLbl    = nullptr;

    /* ── buildPythonDepsTab member state ── */
    QMap<QString, QLabel*> m_pipDots;    ///< pipName → pallino stato
    QScrollArea*           m_pipScroll     = nullptr; ///< scroll area della lista pip
    QTabWidget*            m_pipModuliTabs = nullptr; ///< inner tabs di "Moduli Python"

    /* ── buildMcpTab member state ── */
    QString         m_mcpClaudeCfgPath;

    /* ── buildTemaTab (visuale) member state ── */
    /* nav mode value captured from static array — stored as string */
    QString         m_pendingNavTabMode;
    QString         m_pendingNavStyle;
    QString         m_pendingExecBtnMode;

    /* ── Livello microfono reale (buildTrascriviTab) ── */
#ifdef HAVE_QT_MULTIMEDIA
    QAudioSource*   m_micSource  = nullptr;   ///< sorgente audio QAudioSource
    QIODevice*      m_micDevice  = nullptr;   ///< device I/O aperto da m_micSource->start()
#endif
    QProgressBar*   m_micLevelBar = nullptr;  ///< barra livello 0–100
    QPushButton*    m_micToggleBtn = nullptr; ///< Start/Stop monitoraggio

private slots:
    /** Legge i campioni audio e aggiorna m_micLevelBar con il valore RMS 0-100. */
    void onMicAudioData();
    /** Avvia o ferma il monitoraggio livello microfono. */
    void onMicToggleBtnClicked();
};
