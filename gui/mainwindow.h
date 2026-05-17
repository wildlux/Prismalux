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
#include <QListWidget>
#include <QComboBox>
#include <QDialog>
#include <QTextEdit>
#include <QDateTime>

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

    /**
     * appendLog — Aggiunge una riga al log messaggi con timestamp.
     * Può essere chiamato da qualsiasi punto del codice (main thread).
     * Incrementa il badge non-letto se il dialog è chiuso.
     */
    void appendLog(const QString& msg);

private:
    /* ── Costruzione layout ──────────────────────────────────── */
    QWidget* buildHeader();   ///< Barra superiore: logo, gauges, pulsanti
    QWidget* buildSidebar();  ///< Colonna sinistra: bottoni di navigazione
    QWidget* buildContent();  ///< Area destra: QStackedWidget con le pagine

    /* ── Componenti UI ───────────────────────────────────────── */
    QTabWidget*     m_mainTabs       = nullptr;  ///< Tab principale: Agenti | Finanza | Impara
    QStringList     m_tabOrigLabels;            ///< Etichette originali "icona  testo" per applyTabMode()
    QWidget*        m_navMenuBar     = nullptr;  ///< Barra pulsanti alternativa (menù principale)
    QWidget*        m_cornerContainer = nullptr; ///< Container del pulsante backend (corner widget)
    QVector<QPushButton*> m_navBtns;            ///< Pulsanti nav per sincronizzare lo stato attivo
    ResourceGauge*  m_gCpu        = nullptr;  ///< Gauge CPU nell'header
    ResourceGauge*  m_gRam        = nullptr;  ///< Gauge RAM nell'header
    ResourceGauge*  m_gGpu        = nullptr;  ///< Gauge GPU dedicata nell'header
    ResourceGauge*  m_gIgpu       = nullptr;  ///< Gauge Intel iGPU (nascosto se assente)
    QLabel*         m_lblBackend  = nullptr;  ///< Testo "🦙 Ollama → 127.0.0.1:11434"
    QLabel*         m_lblModel    = nullptr;  ///< Nome modello AI attivo
    QPushButton*    m_settingsBtn   = nullptr;  ///< Pulsante ⚙️ header (accanto hamburger) → Impostazioni
    QPushButton*    m_logBtn        = nullptr;  ///< Pulsante 📋 header (accanto hamburger) → Messaggi/Log
    QLabel*         m_logBadge      = nullptr;  ///< Badge contatore messaggi non letti
    QDialog*        m_logDlg        = nullptr;  ///< Dialog log (creato lazy)
    QTextEdit*      m_logView       = nullptr;  ///< Area testo del log
    int             m_logUnread     = 0;        ///< Contatore messaggi non letti
    QWidget*        m_sidebarWidget = nullptr;  ///< Sidebar (mostra/nascondi con ☰)
    QPushButton*    m_btnUnload   = nullptr;  ///< Pulsante 🗑 Scarica LLM (diventa giallo sopra 40% RAM)
    QPushButton*    m_btnBackend  = nullptr;  ///< Backend AI: Ollama / avvia-ferma llama-server
    SpinnerWidget*  m_spinServer  = nullptr;  ///< Spinner animato durante polling /health
    StatusBadge*    m_badgeServer = nullptr;  ///< Dot colorato stato server (Offline/Starting/Online)
    QProgressBar*   m_statusProgress = nullptr;  ///< Barra progresso pipeline nella status bar

    /* ── Chat History (sidebar) ───────────────────────────────── */
    ChatHistory   m_chatHistory;                  ///< Persistenza sessioni in ~/.prismalux_chats/
    QListWidget*  m_chatList       = nullptr;     ///< Lista chat nella sidebar
    QLineEdit*    m_chatSearch     = nullptr;     ///< Filtro ricerca chat history
    QString       m_currentChatId;               ///< ID sessione chat corrente

    /* ── Gestione llama-server avviato dalla GUI ─────────────── */
    QProcess* m_serverProc  = nullptr;  ///< Processo llama-server (nullptr = fermo)
    int       m_serverPort  = 8081;     ///< Porta corrente del server
    QString   m_serverModel;            ///< Nome file modello caricato nel server

    /* ── Servizi di background ───────────────────────────────── */
    HardwareMonitor* m_hw = nullptr;  ///< Thread monitor CPU/RAM/GPU
    AiClient*        m_ai = nullptr;  ///< Client HTTP per Ollama/llama-server

    /* Timer auto-scarico: ogni 90s verifica se RAM > 40% e AI idle → scarica */
    QTimer* m_idleUnloadTimer = nullptr;

    /* Impostazioni — finestra separata non-modale (creata lazy) */
    QDialog*           m_impDlg      = nullptr;
    class ImpostazioniPage* m_impPage = nullptr;

    /* Strumenti — StrumentiPage riceve il pannello Cron reale via ensureSettingsDialog */
    class StrumentiPage* m_strumentiPage = nullptr;

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

    /** Invia SIGTERM al processo server e aggiorna il pulsante. */
    void stopLlamaServer();

    void showServerDialog();  ///< Dialog avvio llama-server (estratto dal lambda)

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

    /** Popola m_chatList con le sessioni salvate (ordine cronologico inverso). */
    void refreshChatList();

private slots:
    /* ── HW monitor ─────────────────────────────────────────────── */
    void onHWUpdated(SysSnapshot snap);
    void onHWReady(HWInfo hw);

    /* ── Navigazione ─────────────────────────────────────────────── */
    void navigateTo(int idx);
    void onShortcutAlt1() { navigateTo(0); }
    void onShortcutAlt2() { navigateTo(1); }
    void onShortcutAlt3() { navigateTo(4); }
    void onShortcutAlt4() { navigateTo(5); }
    void onShortcutAlt5() { navigateTo(6); }
    void onShortcutAlt6() { navigateTo(7); }
    void onShortcutAlt7() { navigateTo(9); }

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

    /* ── Modelli AI ─────────────────────────────────────────────── */
    void onInitialModelsReady(const QStringList& list);
    void onModelChanged(const QString& model);
    void onApplyBackendModelsReady(const QStringList& list);
    void onQuizAiModelsReady(const QStringList& list);

    /* ── Header buttons ─────────────────────────────────────────── */
    void onHamburgerClicked();
    void onLogBtnClicked();
    void onUnloadModelClicked();
    void onBackendBtnClicked();
    void onOllamaActionTriggered();

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
    void onChatItemClicked(QListWidgetItem* item);
    void onChatContextMenuRequested(const QPoint& pos);
    void onChatActionPdf();
    void onChatActionDelete();

    /* ── Pagine contenuto ───────────────────────────────────────── */
    void onCronPanelFirstOpen();
    void onGraficoRequestSettings(const QString& tabName);
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

    /* ── Math download button (showServerDialog) ─────────────────── */
    void onMathDlBtnClicked();

    /* ── Impostazioni RAG progress ──────────────────────────────── */
    void onIndexingProgress(int done, int total);
    void onIndexingFinished(int n, bool aborted);

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
    QPointer<QPushButton> m_navBackendClone;    ///< Clone backend nella nav bar
    AiClient*            m_quizAi      = nullptr; ///< AiClient separato per QuizPage
    QString              m_pendingExecMode;     ///< Modalità pulsanti exec da applicare
    QString              m_pendingTheme;        ///< Tema da applicare via QueuedConnection
    QPushButton*         m_emergencyBtn = nullptr; ///< Pulsante emergenza RAM
    QProcess*            m_emergencyStopProc  = nullptr;
    QProcess*            m_emergencyCacheProc = nullptr;
    /* Onboarding wizard — puntatori ai combo per onOnboardingAccepted() */
    QComboBox* m_onbBackend = nullptr;
    QComboBox* m_onbModel   = nullptr;
    QComboBox* m_onbTheme   = nullptr;
    QDialog*   m_onbDlg     = nullptr;
};
