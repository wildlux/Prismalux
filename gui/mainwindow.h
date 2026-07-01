#pragma once
/*
 * mainwindow.h — Finestra principale Prismalux GUI
 * =================================================
 * Contiene:
 *   - ResourceGauge: widget composto (label + barra + %) per CPU/RAM/GPU
 *   - MainWindow:    QMainWindow con header, sidebar, area contenuto
 *
 * LAYOUT:
 *   ┌─────────────────────────────────────────────┐
 *   │  Header: logo · backend · gauges · pulsanti  │
 *   ├──────────┬──────────────────────────────────┤
 *   │ Sidebar  │  QTabWidget (pagine)              │
 *   │ ChatList │  🤖 Agenti | 💰 Finanza | 📚 Impara│
 *   │ ─────── │                                   │
 *   │ ⚙️ Sett. │                                   │
 *   └──────────┴──────────────────────────────────┘
 *
 * RESPONSABILITÀ:
 *   - Costruisce e collega i componenti UI principali
 *   - Gestisce la navigazione tra pagine (navigateTo)
 *   - Avvia/ferma llama-server come processo figlio (m_serverProc)
 *   - Aggiorna le gauge hardware in tempo reale (onHWUpdated)
 *   - Gestisce il cambio backend AI (applyBackend)
 */

#include <QMainWindow>
#include <QStackedWidget>
#include <QTabWidget>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QFrame>
#include <QMenu>
#include <QCloseEvent>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTimer>
#include <QElapsedTimer>
#include <QListWidget>
#include <QCheckBox>
#include <QSplitter>
#include <QComboBox>
#include <QDialog>
#include <QTextEdit>
#include <QDateTime>

class QVBoxLayout;
class QHBoxLayout;

#include "hardware_monitor.h"
#include "ai_client.h"
#include "theme_manager.h"
#include "chat_history.h"
#include "widgets/spinner_widget.h"
#include "widgets/status_badge.h"
#include "monitor_panel.h"

/* ══════════════════════════════════════════════════════════════
   ResourceGauge — widget "CPU 45.0%" (label + barra + percentuale)
   Usato nell'header per mostrare CPU, RAM e GPU in tempo reale.
   ══════════════════════════════════════════════════════════════ */
class ResourceGauge : public QWidget {
    Q_OBJECT
public:
    /** Costruisce il gauge con l'etichetta fissa (es. "CPU"). */
    explicit ResourceGauge(const QString& label, QWidget* parent = nullptr);

    /**
     * update(pct, detail) — Aggiorna valore e colore.
     * @param pct     Percentuale 0-100.
     * @param detail  Testo opzionale per tooltip (non usato attualmente).
     * Cambia il colore della barra: verde < 70%, giallo 70-90%, rosso > 90%.
     */
    void update(double pct, const QString& detail = "");

private:
    QLabel*       m_lbl;  ///< Etichetta fissa a sinistra ("CPU", "RAM", "GPU")
    QProgressBar* m_bar;  ///< Barra orizzontale colorata
    QLabel*       m_pct;  ///< Valore percentuale a destra ("45.0%")

    /**
     * setLevel(pct) — Imposta la property QSS "level" per colorare la barra.
     * < 70% → "" (verde default), 70-90% → "warn" (giallo), > 90% → "crit" (rosso).
     * Chiama P::repolish() per forzare il ricalcolo dello stile.
     */
    void setLevel(double pct);
};

/* ══════════════════════════════════════════════════════════════
   MainWindow — finestra principale dell'applicazione
   ══════════════════════════════════════════════════════════════ */
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

    /** Accessori ai servizi condivisi (usati dalle pagine figlie). */
    AiClient*        aiClient()  { return m_ai; }
    HardwareMonitor* hwMonitor() { return m_hw; }

    enum LogCategory { LogSistema, LogAI };

    /**
     * appendLog — Aggiunge una riga al log messaggi con timestamp.
     * cat = LogSistema → tab "Sistema" (backend, server, Qt, errori)
     * cat = LogAI      → tab "AI"      (pipeline, inferenza, RAG, embedding)
     * Incrementa il badge non-letto se il dialog è chiuso.
     */
    void appendLog(const QString& msg, LogCategory cat = LogSistema);

private:
    /* ── Costruzione layout — livello 0 (costruttore) ───────── */
    void setupServices();       ///< Crea HardwareMonitor e AiClient, collega hw signals
    void setupLayout();         ///< Root widget, header, sidebar, content
    void setupStatusBar();      ///< Barra progresso pipeline nella status bar
    void setupAutoOptimizations(); ///< Preset RAM primo avvio + zRAM
    void setupTimers();         ///< Timer idle-unload e wizard primo avvio
    void setupBackend();        ///< Imposta backend Ollama e carica modelli iniziali
    void setupShortcuts();      ///< Alt+1…7 navigazione rapida
    void restoreWindowState();  ///< Ripristina geometry/state da QSettings

    /* ── Costruzione layout — livello 1 ─────────────────────── */
    QWidget* buildHeader();   ///< Barra superiore: logo, gauges, pulsanti
    QWidget* buildSidebar();  ///< Colonna sinistra: bottoni di navigazione
    QWidget* buildContent();  ///< Area destra: QStackedWidget con le pagine

    /* ── buildHeader — livello 2 ─────────────────────────────── */
    void buildHamburgerSection(QHBoxLayout* lay); ///< ☰ + 📋 + ⚙️
    void buildLogoSection(QHBoxLayout* lay);       ///< 🍺 + titolo PRISMALUX
    void buildGaugesSection(QHBoxLayout* lay);     ///< CPU · RAM · GPU gauges
    void buildActionButtons(QHBoxLayout* lay);     ///< 🚨 + Scarica LLM + backend toggle

    /* ── buildContent — livello 2 ───────────────────────────── */
    void buildAiTab();             ///< [0] EAGER — primo tab visibile
    void buildStrumentiTab();      ///< [1] EAGER — container Ricerca
    void buildProgrammazioneTab(); ///< [3] EAGER — container DevAgent + Security
    void buildRicercaTab();        ///< sub-tab Strumenti (singleShot dopo show)
    void buildLanWanTab();         ///< sub-tab Utility (chiamata da createUtilityWidget)
    void buildMultiAgentTab();     ///< cross-pollination Ricerca ↔ LanWan

    /** Primo clic su tab lazy → sostituisce il placeholder col widget reale */
    void ensureTabBuilt(int idx);

    /** Factory widget lazy — non chiamano addTab direttamente */
    QWidget* createMultimediaWidget();
    QWidget* createMatematicaWidget();
    QWidget* createUtilityWidget();
    QWidget* createBioinformaticaWidget();
    QWidget* createAppControllerWidget();

    void buildNavMenuBar(QWidget* wrapper, QVBoxLayout* wLay); ///< Barra menu alternativa
    void applyContentSettings();  ///< Applica nav style e exec btn mode da QSettings

    /* ── showServerDialog — livello 2 ───────────────────────── */
    QWidget* buildServerHwBanner(QWidget* parent); ///< Banner GPU/CPU rilevato
    QWidget* buildServerModelSection(QWidget* parent,
                                     QComboBox** outCombo,
                                     const QStringList& mathPaths,
                                     const QStringList& otherPaths); ///< Combo modello + porta
    QWidget* buildServerMathSection(QWidget* parent,
                                    QComboBox* cmbModel,
                                    const QStringList& mathPaths); ///< Checkbox math + status + download

    /* ── Componenti UI ───────────────────────────────────────── */
    QTabWidget*     m_mainTabs       = nullptr;  ///< Tab principale (9 tab: 0-8)
    QStringList     m_tabOrigLabels;            ///< Etichette originali "icona  testo" per applyTabMode()
    QWidget*        m_navMenuBar     = nullptr;  ///< Barra pulsanti alternativa (menù principale)
    QWidget*        m_cornerContainer = nullptr; ///< Container del pulsante backend (corner widget)
    QVector<QPushButton*> m_navBtns;            ///< Pulsanti nav per sincronizzare lo stato attivo
    ResourceGauge*  m_gCpu        = nullptr;  ///< Gauge CPU nell'header
    ResourceGauge*  m_gRam        = nullptr;  ///< Gauge RAM nell'header
    ResourceGauge*  m_gGpu        = nullptr;  ///< Gauge GPU dedicata nell'header
    ResourceGauge*  m_gIgpu       = nullptr;  ///< Gauge Intel iGPU (nascosto se assente)
    QLabel*         m_tempLbl     = nullptr;  ///< Indicatore temperatura CPU/GPU nell'header
    QLabel*         m_ttftLbl     = nullptr;  ///< TTFT ultimo token (header)
    QElapsedTimer   m_ttftTimer;              ///< Misura TTFT per ogni richiesta
    bool            m_ttftGotFirst = false;   ///< True dopo il primo token della richiesta corrente
    QLabel*         m_lblBackend  = nullptr;  ///< Testo "🦙 Ollama → 127.0.0.1:11434"
    QLabel*         m_lblModel    = nullptr;  ///< Nome modello AI attivo
    QPushButton*    m_settingsBtn   = nullptr;  ///< Pulsante ⚙️ header (accanto hamburger) → Impostazioni
    QPushButton*    m_logBtn        = nullptr;  ///< Pulsante 📋 header (accanto hamburger) → Messaggi/Log
    QLabel*         m_logBadge      = nullptr;  ///< Badge contatore messaggi non letti
    QLineEdit*      m_tabSearchEdit = nullptr;  ///< Ricerca schede nell'header

    /* ── Ricerca schede dinamica ── */
    struct TabSearchEntry { int mainIdx; QString subLabel; QString display; QString keywords; };
    QVector<TabSearchEntry> m_searchIndex;
    QFrame*      m_searchPopup = nullptr;
    QListWidget* m_searchList  = nullptr;

    QTimer*         m_modelRefreshTimer = nullptr; ///< Refresh lista modelli ogni 5 min
    QDialog*        m_logDlg        = nullptr;  ///< Dialog log (creato lazy)
    QTabWidget*     m_logTabs       = nullptr;  ///< Tab Sistema / AI
    QTextEdit*      m_logViewSis    = nullptr;  ///< Log sistema (backend, server, Qt)
    QTextEdit*      m_logViewAI     = nullptr;  ///< Log AI (pipeline, inferenza, RAG)
    int             m_logUnread     = 0;        ///< Contatore messaggi non letti
    QWidget*        m_sidebarWidget = nullptr;  ///< Sidebar (mostra/nascondi con ☰)
    QPushButton*    m_btnBackend  = nullptr;  ///< Backend AI: Ollama / avvia-ferma llama-server
    SpinnerWidget*  m_spinServer  = nullptr;  ///< Spinner animato durante polling /health
    StatusBadge*    m_badgeServer = nullptr;  ///< Dot colorato stato server (Offline/Starting/Online)
    QProgressBar*   m_statusProgress = nullptr;  ///< Barra progresso pipeline nella status bar
    QLabel*         m_dlStatusLbl   = nullptr;  ///< Indicatore download LLM (sempre visibile)
    QLabel*         m_zoomPctLbl    = nullptr;  ///< Label percentuale zoom (status bar)
    QTimer*         m_zoomDebounce   = nullptr;  ///< Debounce 200ms per riapplicare il tema
    int             m_zoomPct        = 100;       ///< Zoom corrente 50-200%
    bool            m_thermalCriticalWarned = false; ///< Evita spam avvisi temperatura critica

    /* ── Chat History (sidebar) ───────────────────────────────── */
    ChatHistory   m_chatHistory;                  ///< Persistenza sessioni in ~/.prismalux_chats/
    QSplitter*    m_bodySplitter    = nullptr;     ///< Splitter sidebar ↔ contenuto
    QListWidget*  m_chatList       = nullptr;     ///< Lista chat nella sidebar
    QLineEdit*    m_chatSearch     = nullptr;     ///< Filtro ricerca chat history
    QPushButton*  m_btnDeleteChats = nullptr;     ///< Cancella chat selezionate
    QCheckBox*    m_chkSelectAll   = nullptr;     ///< Seleziona/deseleziona tutte le chat visibili
    QString       m_currentChatId;               ///< ID sessione chat corrente
    AiClient*     m_summaryAi              = nullptr; ///< Client dedicato per titolo/riassunto LLM (non collide con m_ai)
    QString       m_pendingSummarySessionId;         ///< Sessione a cui appartiene la generazione titolo/riassunto in corso

    /* ── Gestione llama-server / ds4-server avviati dalla GUI ── */
    QProcess* m_serverProc  = nullptr;  ///< Processo server (nullptr = fermo)
    int       m_serverPort  = 8081;     ///< Porta corrente del server
    QString   m_serverModel;            ///< Nome file modello caricato nel server
    bool      m_serverIsDs4 = false;    ///< true quando il server è ds4-server, false = llama-server

    /* ── Servizi di background ───────────────────────────────── */
    HardwareMonitor*    m_hw           = nullptr;  ///< Thread monitor CPU/RAM/GPU
    AiClient*           m_ai           = nullptr;  ///< Client HTTP per Ollama/llama-server
    class OnnxEmbedder* m_onnxEmbedder = nullptr;  ///< Embedder ONNX locale (opzionale)

    /* Timer auto-scarico: ogni 90s verifica se RAM > 40% e AI idle → scarica */
    QTimer* m_idleUnloadTimer = nullptr;

    /* Impostazioni — finestra separata non-modale (creata lazy) */
    QDialog*           m_impDlg      = nullptr;
    class ImpostazioniPage* m_impPage = nullptr;

    /* Pagine con connessioni cross-modulo */
    class UtilityPage*        m_utilityPage      = nullptr;
    class RicercaPage*        m_ricercaPage      = nullptr;
    class AgentiMultiPage*    m_agentiMultiPage  = nullptr;
    class ProgrammazionePage* m_progPage         = nullptr;
    class LanWanPage*         m_lanWanPage       = nullptr;
    class StrumentiPage*      m_strumentiPage    = nullptr;

    /** Mappa "tab già costruito" — false = placeholder in attesa di primo clic */
    QVector<bool>             m_tabBuilt;

    /* Canvas del grafico — usato per collegare i controlli in Impostazioni */
    class GraficoCanvas* m_grafCanvas = nullptr;

    /* ── Helpers backend ─────────────────────────────────────── */
    /**
     * applyBackend — Cambia backend AI, aggiorna UI e carica la lista modelli.
     * Usa il pattern connHolder (QObject temporaneo) per creare una connessione
     * one-shot su modelsReady senza accumulare connessioni permanenti.
     */
    void applyBackend(AiClient::Backend b, const QString& host, int port);

    /** Aggiorna testo e colore del pulsante m_btnBackend dopo un cambio backend. */
    void refreshBackendBtn();

    /* ── Helpers server ──────────────────────────────────────── */
    /**
     * startLlamaServer — Avvia llama-server come processo figlio.
     * @param modelPath   Path assoluto al file .gguf
     * @param port        Porta TCP (default 8081)
     * @param mathProfile true = aggiunge --ctx-size 8192 e --no-mmap (Q4)
     *
     * Dopo l'avvio fa polling su /health ogni secondo (max 30s).
     * Quando il server risponde chiama applyBackend(LlamaServer) automaticamente.
     * Alla chiusura del server ripristina il backend Ollama.
     */
    void startLlamaServer(const QString& modelPath, int port, bool mathProfile = false);

    /** Avvia ds4-server come processo figlio sulla porta indicata. */
    void startDs4Server(const QString& modelPath, int port);

    /** Invia SIGTERM al processo server e aggiorna il pulsante. */
    void stopLlamaServer();

    void showServerDialog();      ///< Dialog avvio llama-server
    void showDwarfStarDialog();   ///< Dialog avvio ds4-server

    /**
     * ensureSettingsDialog — Crea il dialog Impostazioni la prima volta (lazy).
     * Elimina il codice triplicato; sicuro da chiamare più volte (no-op se già creato).
     * Non imposta Qt::Window: evita il crash Windows legato alla gestione parent-child
     * nella Windows API quando QDialog ha sia Qt::Window che un parent widget.
     */
    void ensureSettingsDialog();

    /** Apre il dialog Impostazioni (invocabile da AiErrorWidget via QMetaObject). */
    Q_INVOKABLE void openSettingsDialog();

    /** Wizard di benvenuto al primo avvio. */
    void showOnboardingWizard();

    /** Crea il dialog log la prima volta (lazy, non-modale). */
    void ensureLogDialog();

    /**
     * applyTabMode — Aggiorna le etichette di m_mainTabs in base alla modalità.
     * @param mode  "icons_only" | "icons_text" | "text_icons" | "text_only"
     * Usa m_tabOrigLabels come sorgente (formato "icona  testo").
     */
    void applyTabMode(const QString& mode);

    /**
     * applyNavStyle — Cambia stile navigazione principale.
     * @param style  "tabs_top" = schede in alto (default) | "menu_main" = menù orizzontale
     */
    void applyNavStyle(const QString& style);

    /**
     * applyExecBtnMode — Aggiorna il testo di tutti i pulsanti di esecuzione.
     * Trova tutti QPushButton con proprietà "execIcon" nel widget tree.
     * @param mode  "icon_only" | "text_only" | "icon_text" (default)
     */
    void applyExecBtnMode(const QString& mode);

    /**
     * maybeAutoVramBench — Avvia vram_bench in background al primo avvio.
     * Condizione: vram_profile.json assente + binario vram_bench presente +
     * backend Ollama attivo con almeno un modello. Non blocca la UI.
     */
    void maybeAutoVramBench();

protected:
    /**
     * closeEvent — Intercetta la chiusura.
     * Se AI occupato: chiede conferma e offre di scaricare il modello dalla RAM.
     * Termina llama-server se avviato dalla GUI.
     * Esegue cleanup Ollama best-effort senza bloccare la chiusura.
     */
    void closeEvent(QCloseEvent* ev) override;

    /** changeEvent — Invalida la cache modelli quando la finestra torna in primo piano. */
    void changeEvent(QEvent* ev) override;

    /**
     * eventFilter — Intercetta tasti su m_chatList.
     * Canc → conferma QMessageBox; Shift+Canc → elimina con undo 5s.
     */
    bool eventFilter(QObject* obj, QEvent* ev) override;

    /** Popola m_chatList con le sessioni salvate (ordine cronologico inverso). */
    void refreshChatList();

    /** Genera titolo + riassunto breve/lungo via LLM (istanza dedicata m_summaryAi,
     *  non collide con lo stream della chat visibile) e li salva su sessionId. */
    void requestSessionSummary(const QString& sessionId, const QString& logHtml);

private slots:
    /* ── HW monitor ─────────────────────────────────────────────── */
    void onHWUpdated(SysSnapshot snap);
    void onHWReady(HWInfo hw);

    /* ── Navigazione ─────────────────────────────────────────────── */
    void navigateTo(int idx);
    void buildSearchIndex();
    void showSearchPopup(const QString& query);
    void hideSearchPopup();
    void navigateToEntry(int idx);
    void onSearchItemActivated(QListWidgetItem* item);
    void onShortcutAlt1() { navigateTo(0); }  /* AI */
    void onShortcutAlt2() { navigateTo(1); }  /* Strumenti */
    void onShortcutAlt3() { navigateTo(3); }  /* Programmazione (File AI → sub-tab Strumenti) */
    void onShortcutAlt4() { navigateTo(4); }  /* Matematica */
    void onShortcutAlt5() { navigateTo(5); }  /* Ricerca */
    void onShortcutAlt6() { navigateTo(6); }  /* AppController */
    void onShortcutAlt7() { navigateTo(8); }  /* Multi-Agente */

    /* ── Agenti / Pipeline ──────────────────────────────────────── */
    void onChatCompleted(const QString& title, const QString& logHtml);
    void onPipelineStatus(int pct, const QString& text);

    /* ── Whisper setup ──────────────────────────────────────────── */
    void onStartWhisperTimer();
    void onWhisperReady();
    void onWhisperReadyStatus();
    void onWhisperFailed(const QString& err);

    /* ── Idle unload timer ───────────────────────────────────────── */
    void onIdleUnloadTimer();

    /* ── Pre-build tab lazy in background ──────────────────────── */
    void onPreBuildTab2();
    void onPreBuildTab4();
    void onPreBuildTab5();
    void onPreBuildTab6();
    void onPreBuildTab7();

    /* ── Modelli AI ─────────────────────────────────────────────── */
    void onInitialModelsReady(const QStringList& list);
    void onModelChanged(const QString& model);
    void onApplyBackendModelsReady(const QStringList& list);
    void onZoomMinusBtnClicked();
    void onZoomPlusBtnClicked();
    void onZoomResetBtnClicked();
    void onZoomPercentChanged(int pct);
    void onZoomApplyDebounced();
    /* ── Header buttons ─────────────────────────────────────────── */
    void onHamburgerClicked();
    void onLogBtnClicked();
    void onBackendBtnClicked();
    void onOllamaActionTriggered();
    void onDwarfStarActionTriggered();

    /* ── Emergenza RAM ──────────────────────────────────────────── */
    void onEmergencyRamClicked();
    void onEmergencyStopFinished(int code, QProcess::ExitStatus status);
    void onEmergencyCacheFinished(int code, QProcess::ExitStatus status);

    /* ── llama-server ───────────────────────────────────────────── */
    void onServerProcFinished(int code, QProcess::ExitStatus status);
    void onServerProcessError(QProcess::ProcessError err);
    void onHealthTick();
    void onHealthReply();

    /* ── Chat sidebar ───────────────────────────────────────────── */
    void onNewChatClicked();
    void onChatSearchChanged(const QString& q);
    void onChatSelectionChanged();
    void onSelectAllToggled(bool checked);
    void onTabSearchTextChanged(const QString& t);
    void onChatItemClicked(QListWidgetItem* item);
    void onChatContextMenuRequested(const QPoint& pos);
    void onChatActionPdf();
    void onChatActionDelete();
    void onDeleteSelectedChatsClicked(); ///< Pulsante Cancella: rimuove tutte le selezionate
    void onChatDeleteConfirm();        ///< Canc: QMessageBox question → elimina
    void onChatDeleteShift();          ///< Shift+Canc: elimina con undo 5s
    void onChatUndoDelete();           ///< Annulla la cancellazione pendente
    void onChatUndoTimeout();          ///< 5s scaduti: cancella definitivamente

    /* ── Pagine contenuto ───────────────────────────────────────── */
    void onCronPanelFirstOpen();
    void onGraficoRequestSettings(const QString& tabName);
    void onOpenSettingsDipendenze(const QString& pipPkg);
    void onMathSubTabChanged(int idx);
    void onMainTabChanged(int idx);
    void onSyncNavBackendClone();
    void onApplyExecBtnMode();

    /* ── Log dialog ─────────────────────────────────────────────── */
    void onClearLogClicked();

    /* ── VRAM bench ─────────────────────────────────────────────── */
    void onVramBenchFinished(int code, QProcess::ExitStatus status);

    /* ── Onboarding wizard ──────────────────────────────────────── */
    void onOnboardingAccepted();
    void onApplyPendingTheme();

    /* ── Status bar ─────────────────────────────────────────────── */
    void onRestoreDefaultStatus();

    /* ── zRAM setup ─────────────────────────────────────────────── */
    void onZramSetupTimer();

    /* ── Auto-update GitHub ─────────────────────────────────────── */
    void checkForUpdates();
    void onAutoUpdateReply();

    /* ── TTFT tracking ──────────────────────────────────────────── */
    void onTtftRequestStarted();
    void onTtftToken();

    /* ── Math download button (showServerDialog) ─────────────────── */
    void onMathDlBtnClicked();

    /* ── Impostazioni RAG progress ──────────────────────────────── */
    void onIndexingProgress(int done, int total);
    void onIndexingFinished(int n, bool aborted);
    void onAutoRagIndex();    ///< Auto-indicizza RAG all'avvio se vuoto

    /* ── Agenti → grafico ───────────────────────────────────────── */
    void onRequestShowInGrafico(const QString& formula, double xMin, double xMax,
                                const QVector<QPointF>& points);

private:
    /* ── Nuovi membri per slot senza lambda ─────────────────────── */
    QString              m_pendingBkName;       ///< Backend name per onApplyBackendModelsReady
    QTimer*              m_healthTimer  = nullptr; ///< Timer polling /health llama-server
    QNetworkAccessManager* m_healthNam  = nullptr; ///< NAM riusato per tutti i tick
    int                  m_healthTicks  = 0;    ///< Contatore tick polling
    QString              m_ctxChatId;           ///< ID chat per context menu PDF/delete
    QString              m_ctxChatTitle;        ///< Titolo chat per context menu
    /* Shift+Canc undo-delete */
    QString              m_undoChatId;          ///< ID chat in attesa di cancellazione (undo)
    QLabel*              m_undoLabel = nullptr; ///< Label "Annulla (5s)" nella status bar
    QTimer*              m_undoTimer = nullptr; ///< Timer 5s per cancellazione definitiva
    QPointer<QPushButton> m_navBackendClone;    ///< Clone backend nella nav bar
    QString              m_pendingExecMode;     ///< Modalità pulsanti exec da applicare
    QString              m_pendingTheme;        ///< Tema da applicare via QueuedConnection
    QPushButton*         m_emergencyBtn = nullptr; ///< Pulsante emergenza RAM
    QProcess*            m_emergencyStopProc  = nullptr;
    QProcess*            m_emergencyCacheProc = nullptr;
    /* Onboarding wizard — puntatori ai combo per onOnboardingAccepted() */
    QComboBox* m_onbBackend = nullptr;
    QComboBox* m_onbModel   = nullptr;
    QComboBox* m_onbTheme   = nullptr;
    QCheckBox* m_onbNoShow  = nullptr;
    QDialog*   m_onbDlg     = nullptr;
};
