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

    /* ── Navigazione ─────────────────────────────────────────── */
    /** Attiva la pagina con indice idx (0-2: sidebar, 3: impostazioni). */
    void setActivePage(int idx);

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
    QDialog*           m_impDlg    = nullptr;
    class ImpostazioniPage* m_impPage = nullptr;

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
    /** Riceve aggiornamenti periodici CPU/RAM/GPU da HardwareMonitor. */
    void onHWUpdated(SysSnapshot snap);

    /** Riceve le info hardware complete al primo rilevamento (one-shot). */
    void onHWReady(HWInfo hw);

    /** Cambia pagina attiva e aggiorna lo stile dei bottoni sidebar. */
    void navigateTo(int idx);

    /** Ricevuto da AgentiPage quando una pipeline/Byzantine/MathTheory finisce. */
    void onChatCompleted(const QString& title, const QString& logHtml);

    /** Aggiorna la barra progresso pipeline nella status bar. */
    void onPipelineStatus(int pct, const QString& text);
};
