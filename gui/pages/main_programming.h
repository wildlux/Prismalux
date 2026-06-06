#pragma once
#include <QWidget>
#include <QRegularExpression>
#include "../ai_client.h"
#include "../widgets/chart_widget.h"
#include "../widgets/code_highlighter.h"
#include "../widgets/toggle_switch.h"

class AiErrorWidget;
class QComboBox;
class QPlainTextEdit;
class QTextEdit;
class QLineEdit;
class QPushButton;
class QLabel;
class QProcess;
class QSplitter;
class QGroupBox;
class QTabWidget;
class QSpinBox;
class QSlider;
class QTableWidget;
class QVBoxLayout;

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

   Sub-tab interni:
   ┌─────────────────────────────────────────────────────┐
   │  [💻 Programmazione]  [🤖 Agentica]                 │
   └─────────────────────────────────────────────────────┘
   ══════════════════════════════════════════════════════════════ */
class ProgrammazionePage : public QWidget {
    Q_OBJECT
public:
    explicit ProgrammazionePage(AiClient* ai, QWidget* parent = nullptr);
    ~ProgrammazionePage() override;

    static bool isIntentionalError(const QString& errorOut, const QString& source);
    static QVector<double> parseNumbers(const QString& text);

    bool hasUnsavedWork() const;
    void saveCurrentFile();

private:
    AiClient*       m_ai        = nullptr;

    /* Inner tab widget */
    QTabWidget*     m_innerTabs  = nullptr;

    /* Editor */
    QComboBox*        m_lang        = nullptr;
    QPlainTextEdit*   m_editor      = nullptr;
    CodeHighlighter*  m_highlighter = nullptr;

    /* Output */
    QPlainTextEdit* m_output    = nullptr;
    ChartWidget*    m_chart     = nullptr;
    QGroupBox*      m_chartGroup = nullptr; ///< gruppo grafico (show/hide in base all'output)
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

    AiErrorWidget*  m_fixErrPanel = nullptr;  ///< banner errore per il fix AI

    /* Connessioni AI per il pannello Coding/Fix — disconnesse esplicitamente prima
       di ogni nuova chat per garantire al massimo UNA connessione attiva. */
    QMetaObject::Connection m_aiTokenConn;
    QMetaObject::Connection m_aiFinishedConn;
    QMetaObject::Connection m_aiErrorConn;

    /* ── Agentica sub-tab ── */
    QComboBox*      m_agentType         = nullptr;
    QComboBox*      m_agentLang         = nullptr;
    QComboBox*      m_agentModel        = nullptr;
    QPlainTextEdit* m_agentTask         = nullptr;
    QTextEdit*      m_agentOutput       = nullptr;
    QPushButton*    m_btnAgentRun       = nullptr;
    QPushButton*    m_btnAgentStop      = nullptr;
    QPushButton*    m_btnAgentInsert    = nullptr;
    QMetaObject::Connection m_agentTokenConn;
    QMetaObject::Connection m_agentFinishedConn;
    QMetaObject::Connection m_agentErrorConn;

    QWidget* buildAgentica(QWidget* parent);
    /* buildAgentica helpers */
    void     buildAgenticaHeader(QVBoxLayout* lay, QWidget* w);
    QWidget* buildAgenticaToolbar(QWidget* parent);
    QWidget* buildAgenticaTaskGroup(QWidget* parent);
    QWidget* buildAgenticaModelRow(QWidget* parent);
    QWidget* buildAgenticaOutputGroup(QWidget* parent);
    void     setupAgenticaConnections(QPushButton* btnRefAgent,
                                      QPushButton* btnClearAgent);
    void     runAgente();

    /* ── Translitter sub-tab ── */
    QComboBox*      m_trSrcLang        = nullptr;
    QComboBox*      m_trDstLang        = nullptr;
    QComboBox*      m_trModel          = nullptr;
    QPlainTextEdit* m_trInput          = nullptr;
    QTextEdit*      m_trOutput         = nullptr;
    QPushButton*    m_btnTrRun         = nullptr;
    QPushButton*    m_btnTrStop        = nullptr;
    QPushButton*    m_btnTrInsert      = nullptr;
    QMetaObject::Connection m_trTokenConn;
    QMetaObject::Connection m_trFinishedConn;
    QMetaObject::Connection m_trErrorConn;

    QWidget* buildTranslitter(QWidget* parent);
    /* buildTranslitter helpers */
    QWidget* buildTrControlRow(QWidget* parent,
                               QPushButton*& outBtnSwap,
                               QPushButton*& outBtnFromEditor);
    QWidget* buildTrSplitter(QWidget* parent);
    void     setupTrConnections(QPushButton* btnSwap,
                                QPushButton* btnFromEditor);
    void     runTranslitter();

    /* ── Reverse Engineering sub-tab ── */
    QLabel*         m_revFilePath       = nullptr;
    QPlainTextEdit* m_revPreview        = nullptr;
    QComboBox*      m_revTargetLang     = nullptr;
    QComboBox*      m_revDetail         = nullptr;
    QComboBox*      m_revModel          = nullptr;
    QTextEdit*      m_revOutput         = nullptr;
    QPushButton*    m_btnRevAnalyze     = nullptr;
    QPushButton*    m_btnRevStop        = nullptr;
    QPushButton*    m_btnRevInsert      = nullptr;
    QMetaObject::Connection m_revTokenConn;
    QMetaObject::Connection m_revFinishedConn;
    QMetaObject::Connection m_revErrorConn;
    QByteArray      m_revFileData;

    QWidget* buildReverseEngineering(QWidget* parent);
    /* buildReverseEngineering helpers */
    void     buildRevHeader(QVBoxLayout* lay, QWidget* w);
    QWidget* buildRevFileRow(QWidget* parent);
    QWidget* buildRevOptionsRow(QWidget* parent,
                                QPushButton*& outBtnRefRev);
    QWidget* buildRevPreviewGroup(QWidget* parent);
    QWidget* buildRevOutputGroup(QWidget* parent,
                                 QPushButton*& outBtnClearRev);
    void     setupRevConnections(QPushButton* btnLoad,
                                 QPushButton* btnRefRev,
                                 QPushButton* btnClearRev);
    void     runReverseEngineering();

    /* ── Git MCP sub-tab ── */
    QWidget*        m_gitActRow      = nullptr;
    QLineEdit*      m_gitRepoPath    = nullptr;
    QPlainTextEdit* m_gitOutput      = nullptr;
    QLineEdit*      m_gitCommitMsg   = nullptr;
    QComboBox*      m_gitAiModel     = nullptr;
    QPlainTextEdit* m_gitAiOutput    = nullptr;
    QMetaObject::Connection m_gitAiTokenConn;
    QMetaObject::Connection m_gitAiFinishedConn;
    QMetaObject::Connection m_gitAiErrorConn;
    QWidget*        m_gitAiPanel     = nullptr;
    QProcess*       m_gitProc        = nullptr;
    QPushButton*    m_btnGitStop     = nullptr;
    QString         m_gitPendingCommit;

    QWidget* buildGitMcp(QWidget* parent);
    /* buildGitMcp helpers */
    QWidget* buildGitRepoRow(QWidget* parent, QPushButton*& outBtnBrowse);
    QWidget* buildGitActionsRow(QWidget* parent);
    QWidget* buildGitCommitRow(QWidget* parent,
                               QPushButton*& outBtnAddCommit,
                               QPushButton*& outBtnPull,
                               QPushButton*& outBtnPush);
    QWidget* buildGitOutputGroup(QWidget* parent,
                                 QPushButton*& outBtnClearGit,
                                 QPushButton*& outBtnGitAi,
                                 QPushButton*& outBtnGenCommit);
    QWidget* buildGitAiPanel(QWidget* parent,
                             QPushButton*& outBtnRefGit,
                             QPushButton*& outBtnCloseGitAi);
    void     setupGitConnections(QPushButton* btnBrowse,
                                 QPushButton* btnStatus,
                                 QPushButton* btnDiff,
                                 QPushButton* btnDiffSt,
                                 QPushButton* btnLog,
                                 QPushButton* btnBranch,
                                 QPushButton* btnPull,
                                 QPushButton* btnAddCommit,
                                 QPushButton* btnPush,
                                 QPushButton* btnClearGit,
                                 QPushButton* btnGitAi,
                                 QPushButton* btnGenCommit,
                                 QPushButton* btnRefGit,
                                 QPushButton* btnCloseGitAi);
    void     gitRun(const QString& subcmd, const QStringList& args = {});
    void     gitAiRequest(const QString& request, const QString& context);

    /* ── Python REPL sub-tab ── */
    QPlainTextEdit* m_replOutput  = nullptr;
    QLineEdit*      m_replInput   = nullptr;
    QPushButton*    m_btnSendRepl = nullptr;  ///< "Invia ▶" nel REPL (abilitato solo con proc attivo)
    QProcess*       m_replProc    = nullptr;
    QLabel*         m_replStatus  = nullptr;
    QStringList     m_replHistory;
    int             m_replHistIdx = -1;

    QWidget* buildPythonRepl(QWidget* parent);
    /* buildPythonRepl helpers */
    QWidget* buildReplHeader(QWidget* parent,
                             QPushButton*& outBtnRestart,
                             QPushButton*& outBtnImport);
    QWidget* buildReplOutputGroup(QWidget* parent,
                                  QPushButton*& outBtnClearRepl);
    QWidget* buildReplInputRow(QWidget* parent);
    void     setupReplConnections(QPushButton* btnRestart,
                                  QPushButton* btnImport,
                                  QPushButton* btnClearRepl);
    void     replStart();
    void     replSend();

    /* ── Network Analyzer sub-tab (tshark/tcpdump) ── */
    QComboBox*      m_netIface       = nullptr;   ///< interfaccia di cattura
    QComboBox*      m_netProto       = nullptr;   ///< protocollo: any/tcp/udp/icmp/arp/dns/http/https
    QSpinBox*       m_netPort        = nullptr;   ///< porta (0 = qualsiasi)
    QSpinBox*       m_netMaxPkts     = nullptr;   ///< numero max pacchetti
    QPlainTextEdit* m_netLog         = nullptr;   ///< log pacchetti live
    QLabel*         m_netStatus      = nullptr;
    QPushButton*    m_btnNetStart    = nullptr;
    QPushButton*    m_btnNetStop     = nullptr;
    QPushButton*    m_btnNetAnalyze  = nullptr;   ///< AI analizza il traffico catturato
    QPushButton*    m_btnNetClear    = nullptr;
    QPushButton*    m_btnNetFixPerms = nullptr;   ///< applica cap_net_raw via pkexec
    QProcess*       m_netProc        = nullptr;
    QMetaObject::Connection m_netAiTokenConn;
    QMetaObject::Connection m_netAiFinishedConn;
    QMetaObject::Connection m_netAiErrorConn;
    QTextEdit*      m_netAiOutput    = nullptr;
    QString         m_netTool;                    ///< "tshark" o "tcpdump"

    QWidget* buildNetworkAnalyzer(QWidget* parent);
    /* buildNetworkAnalyzer helpers */
    void     buildNetToolbar(QVBoxLayout* lay, QWidget* w);
    QWidget* buildNetLogSplitter(QWidget* parent);
    void     setupNetConnections();
    void     netStart();
    void     netStop();
    void     netAiAnalyze();
    void     netFixPermissions();

    /* ── Rete LAN Scanner sub-tab ── */
    QLabel*         m_lanInfoLbl    = nullptr;  ///< info interfacce locali
    QTableWidget*   m_lanTable      = nullptr;  ///< IP | MAC | Hostname | Stato
    QLabel*         m_lanStatusLbl  = nullptr;  ///< stato scan / subnet info
    QPushButton*    m_lanScanArp    = nullptr;
    QPushButton*    m_lanScanNmap   = nullptr;
    QPushButton*    m_lanStopBtn    = nullptr;
    QProcess*       m_lanProc       = nullptr;
    QString         m_lanBuf;

    QWidget* buildReteLan(QWidget* parent);
    void     lanRefreshInfo();
    QPushButton*    m_btnSend   = nullptr;   ///< "Invia ▶" nel pannello AI (disabilitato durante streaming)

    /* Toolbar buttons */
    QPushButton*    m_btnRun    = nullptr;
    QPushButton*    m_btnAi     = nullptr;
    QPushButton*    m_btnFix    = nullptr; ///< "🔧 Correggi con AI" — invia il codice + errore al coder
    QPushButton*    m_btnLint   = nullptr; ///< "🔍 Analizza" — linting statico pre-AI
    ToggleSwitch*   m_toggleAutoFix = nullptr; ///< toggle stile aereo: loop fix automatico

    /* Loop Auto-Fix */
    bool m_loopActive = false;
    int  m_loopCount  = 0;
    int  m_loopMax    = 6;
    QSlider* m_fixSlider    = nullptr;
    QLabel*  m_fixSliderLbl = nullptr;

    /* Pannello diff (mostrato dopo ogni correzione AI) */
    QGroupBox*      m_diffGroup = nullptr;
    QTextEdit*      m_diffView  = nullptr;

    QString         m_currentFilePath;    ///< path file aperto (vuoto = non salvato)

    /* Stato esecuzione e lint */
    int             m_lastExitCode = 0;
    QString         m_lastError;           ///< errore esecuzione o output lint (passato al fix)
    QString         m_originalCode;        ///< codice salvato prima del fix AI (per il diff)

    /* Constructor helpers — coding tab */
    QWidget* buildCodingTab(QWidget* parent);
    QWidget* buildCodingToolbar(QWidget* parent, QPushButton*& outBtnClear);
    QWidget* buildEditorColumn(QWidget* parent);
    QWidget* buildOutputColumn(QWidget* parent);
    QWidget* buildAiPanel(QWidget* parent,
                          QPushButton*& outBtnRefreshMod,
                          QPushButton*& outBtnCloseAi);
    void     setupCodingConnections(QPushButton* btnClear,
                                    QPushButton* btnRefreshMod,
                                    QPushButton* btnCloseAi);
    void     buildInnerTabs();

    QString tempFilePath(const QString& ext) const;
    QString buildRunCommand(const QString& filePath) const;
    QString currentTemplate() const;
    void    appendOutput(const QString& text);
    void    setRunning(bool running);
    void    tryShowChart();
    void    runCode();                ///< esegue il codice nell'editor (logica estratta dal click Esegui)
    QString extractCodeBlock() const;
    void    selectCoderModel();
    void    triggerFix(bool includeError);
    void    _doFix(bool includeError, const QString& codice,
                   const QString& lang, const QString& ext);
    void    runLint();                ///< analisi statica con linter del linguaggio corrente
    void    showDiff(const QString& before, const QString& after); ///< diff colorato pre/post fix

    /* ── Slot estratti da lambda — Coding tab (costruttore) ── */
    void onAutoFixToggled(bool on);
    void onFixSliderChanged(int v);
    void onLangChanged(int idx);
    void onBtnClearClicked();
    void onBtnAiToggled(bool on);
    void onBtnCloseAiClicked();
    void sendToAi();
    void onBtnInsertClicked();
    void onBtnRunClicked();
    void onBtnFixClicked();
    void onModelChanged(const QString& newModel);
    void populateAiModels();

    /* Slot per i token/finished/error dell'AI nel pannello Coding */
    void onAiToken(const QString& tok);
    void onAiFinished(const QString& full);
    void onAiError(const QString& msg);

    /* Slot per i token/finished/error dell'AI durante Fix */
    void onFixToken(const QString& tok);
    void onFixFinished(const QString& full);
    void onFixError(const QString& msg);

    /* Stato modello originale salvato prima del fix (per ripristino) */
    QString m_fixOriginalModel;

    /* Path del file temporaneo in esecuzione (passato al proc finished) */
    QString m_procFilePath;

    /* Parametri pending per triggerFix one-shot (usati da onModelsReadyForFix) */
    bool    m_pendingFixIncludeError = false;
    QString m_pendingFixCodice;
    QString m_pendingFixLang;
    QString m_pendingFixExt;
    QMetaObject::Connection m_modelsReadyForFixConn;

    /* ── Slot estratti da lambda — runCode() ── */
    void onProcReadyRead();
    void onProcFinished(int code, QProcess::ExitStatus status);
    void onProcErrorOccurred(QProcess::ProcessError err);
    void onLoopFixTimer();           ///< QTimer::singleShot → triggerFix(true)
    void onLoopRunTimer();           ///< QTimer::singleShot → runCode()
    void onModelsReadyForFix(const QStringList&); ///< one-shot: modelli pronti → lancia fix

    /* ── Slot estratti da lambda — Agentica ── */
    void populateAgentModels();
    void onBtnAgentStopClicked();
    void onBtnClearAgentClicked();
    void onBtnAgentInsertClicked();
    void onAgentToken(const QString& tok);
    void onAgentFinished(const QString& full);
    void onAgentError(const QString& msg);

    /* ── Slot estratti da lambda — Reverse Engineering ── */
    void populateRevModels();
    void onBtnRevLoadClicked();
    void onBtnRevStopClicked();
    void onBtnClearRevClicked();
    void onBtnRevInsertClicked();
    void onRevToken(const QString& tok);
    void onRevFinished(const QString& full);
    void onRevError(const QString& msg);

    /* ── Slot estratti da lambda — Network Analyzer ── */
    void onBtnNetClearClicked();
    bool m_netUseTshark = false; ///< impostato in netStart(), usato da onNetReadyRead()
    void onNetReadyRead();
    void onNetReadyReadStderr();
    void onNetFinished(int code, QProcess::ExitStatus status);
    void onNetStopTimer();       ///< QTimer::singleShot fallback kill in netStop()
    void onNetAiToken(const QString& tok);
    void onNetAiFinished(const QString& full);
    void onNetAiError(const QString& msg);

    /* ── Slot estratti da lambda — Rete LAN ── */
    void onLanScanArpClicked();
    void onLanScanNmapClicked();
    void onLanStopBtnClicked();
    /* Slot interni usati dai processi LAN (ARP) */
    void onLanArpError(QProcess::ProcessError err);
    void onLanArpReadyRead();
    void onLanArpFinished(int code, QProcess::ExitStatus status);
    /* Slot interni usati dai processi LAN (nmap) */
    void onLanNmapError(QProcess::ProcessError err);
    void onLanNmapReadyRead();
    void onLanNmapFinished(int code, QProcess::ExitStatus status);
    void onLanStopTimer();  ///< QTimer::singleShot fallback kill in onLanStopBtnClicked()
    void lanAddRow(const QString& ip, const QString& mac,
                   const QString& host, const QString& stato);
    void lanResetBtns();
    void enrichMacFromNeigh(); ///< arricchisce MAC mancanti dalla ARP cache del kernel (ip neigh show)
    QString m_lanNmapSubnet; ///< subnet corrente per nmap (usata da onLanNmapFinished)

    /* ── Slot estratti da lambda — Git MCP ── */
    void populateGitModels();
    void onBtnGitBrowseClicked();
    void onBtnGitStopClicked();
    void onBtnClearGitClicked();
    void onBtnCloseGitAiClicked();
    void onBtnGitPullClicked();
    void onBtnAddCommitClicked();
    void onBtnPushClicked();
    void onBtnGitAiClicked();
    void onBtnGenCommitClicked();
    /* Azioni rapide git (status/diff/log/branch) */
    void onBtnGitStatusClicked();
    void onBtnGitDiffClicked();
    void onBtnGitDiffStagedClicked();
    void onBtnGitLogClicked();
    void onBtnGitBranchClicked();
    void onGitReadyRead();
    void onGitFinished(int exitCode, QProcess::ExitStatus status);
    void onGitErrorOccurred(QProcess::ProcessError err);
    void onGitAiToken(const QString& tok);
    void onGitAiFinished(const QString& full);
    void onGitAiError(const QString& msg);

    /* ── Slot estratti da lambda — REPL ── */
    void onReplReadyRead();
    void onReplStarted();
    void onReplFinished(int code, QProcess::ExitStatus status);
    void onReplErrorOccurred(QProcess::ProcessError err);
    void onReplTabChanged(int idx);
    void onBtnReplRestartClicked();
    void onBtnReplClearClicked();
    void onBtnReplImportClicked();
    void sendReplLine();

    /* ── Slot estratti da lambda — Translitter ── */
    void onBtnSwapLangsClicked();
    void onBtnFromEditorClicked();
    void onBtnTrStopClicked();
    void onBtnTrInsertClicked();
    void onTrOutputTextChanged();
    void onTrModelChanged(const QString& newModel);
    void populateTrModels();
    /* btnCopyOut è locale — per il suo slot usiamo un QObject* holder salvato */
    QPushButton*    m_btnTrCopy     = nullptr; ///< salvato per slot onBtnTrCopyClicked
    QString         m_trCopyOrigTxt;           ///< testo originale del btn prima del "Copiato!"
    void onBtnTrCopyClicked();
    void onTrCopyRestoreText();  ///< QTimer::singleShot → ripristina m_trCopyOrigTxt
    void onTrModelActivated(int idx);
    void onTrToken(const QString& tok);
    void onTrFinished(const QString& full);
    void onTrError(const QString& msg);

    /* ── Driver & Kernel sub-tab ── */
    QTextEdit*   m_driverOutput       = nullptr;  ///< output NVIDIA
    QTextEdit*   m_driverAmdOutput    = nullptr;  ///< output AMD
    QTextEdit*   m_driverKernelOutput = nullptr;  ///< output Kernel
    QProcess*    m_driverProcess      = nullptr;
    bool         m_driverAiBusy       = false;
    QTextEdit*   m_driverAiActive     = nullptr;  ///< punta all'output corrente per lo streaming AI
    QMetaObject::Connection m_driverAiTokenConn;
    QMetaObject::Connection m_driverAiFinishedConn;
    QMetaObject::Connection m_driverAiErrorConn;

    QWidget* buildDriverKernelTab(QWidget* parent);
    void     onDriverRunCmd(const QString& cmd);
    void     onDriverCmdFinished(int exitCode, QProcess::ExitStatus status);
    void     onDriverCmdOutput();
    void     onDriverAiGuide(const QString& topic);
    void     onDriverAiToken(const QString& t);
    void     onDriverAiFinished(const QString& full);
    void     onDriverAiError(const QString& msg);
    void     onNvidiaDetectClicked();
    void     onNvidiaDownloadClicked();
    void     onNvidiaDkmsClicked();
    void     onNvidiaGuideClicked();
    void     onAmdDetectClicked();
    void     onAmdDownloadClicked();
    void     onAmdGuideClicked();
    void     onKernelVersionClicked();
    void     onKernelListClicked();
    void     onKernelGuideClicked();
    void     onKernelSafetyClicked();

    /* ── VPN & Tunnel sub-tab ── */
    QComboBox*   m_vpnTypeCombo    = nullptr;  ///< WireGuard / OpenVPN / SSH / Hotspot / n2n
    QTextEdit*   m_vpnConfig       = nullptr;  ///< editor config/script generato
    QTextEdit*   m_vpnLog          = nullptr;  ///< output comandi applicati
    QLabel*      m_vpnStatusLbl    = nullptr;
    QProcess*    m_vpnProc         = nullptr;
    QPushButton* m_vpnGenKeysBtn   = nullptr;  ///< visibile solo per n2n (idx >= 4)
    QPushButton* m_vpnValidateBtn  = nullptr;  ///< simulazione/validazione senza root
    bool         m_vpnValidating   = false;
    QMetaObject::Connection m_vpnAiTokenConn;
    QMetaObject::Connection m_vpnAiFinishedConn;
    QMetaObject::Connection m_vpnAiErrorConn;

    QWidget* buildVpnTab(QWidget* parent);
    void     onVpnTypeChanged(int idx);
    void     onVpnGenerateClicked();
    void     onVpnApplyClicked();
    void     onVpnStopClicked();
    void     onVpnGenN2nKeys();
    void     onVpnValidateClicked();
    void     onVpnImportClicked();
    void     onVpnAiToken(const QString& t);
    void     onVpnAiFinished(const QString& full);
    void     onVpnAiError(const QString& msg);
    void     onVpnProcReadyRead();
    void     onVpnProcFinished(int code, QProcess::ExitStatus status);
};
