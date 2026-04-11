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
 *   │ Sidebar  │  QStackedWidget (pagine)          │
 *   │ navBtn×3 │  0: Agenti  1: Pratico  2: Impara │
 *   │          │  3: Impostazioni                  │
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
#include <QDialog>
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
#include "hardware_monitor.h"
class ImpostazioniPage;  /* forward declaration — definita in pages/impostazioni_page.h */
class AgentiPage;        /* forward declaration — definita in pages/agenti_page.h */
#include "ai_client.h"
#include "theme_manager.h"
#include "widgets/spinner_widget.h"
#include "widgets/status_badge.h"
#include "monitor_panel.h"
#include "chat_history.h"

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

private:
    /* ── Costruzione layout ──────────────────────────────────── */
    QWidget* buildHeader();   ///< Barra superiore: logo, gauges, pulsanti
    QWidget* buildSidebar();  ///< Colonna sinistra: bottoni di navigazione
    QWidget* buildContent();  ///< Area destra: QStackedWidget con le pagine

    /* ── Navigazione ─────────────────────────────────────────── */
    /** Attiva la pagina con indice idx (0-2: sidebar, 3: impostazioni). */
    void setActivePage(int idx);

    /* ── Componenti UI ───────────────────────────────────────── */
    QStackedWidget* m_stack       = nullptr;  ///< Contenitore pagine
    ResourceGauge*  m_gCpu        = nullptr;  ///< Gauge CPU nell'header
    ResourceGauge*  m_gRam        = nullptr;  ///< Gauge RAM nell'header
    ResourceGauge*  m_gGpu        = nullptr;  ///< Gauge GPU nell'header
    QLabel*         m_lblBackend  = nullptr;  ///< Testo "Ollama → 127.0.0.1:11434"
    QLabel*         m_lblModel    = nullptr;  ///< Nome modello AI attivo
    QPushButton*    m_navBtns[4]  = {};       ///< Bottoni sidebar (0=Agenti,1=Pratico,2=Impara,3=Programmazione)
    QPushButton*    m_themeBtn    = nullptr;  ///< Pulsante ⚙️ → pagina Impostazioni
    QPushButton*    m_btnUnload   = nullptr;  ///< Pulsante 🗑 Scarica LLM (diventa giallo sopra 40% RAM)
    QPushButton*    m_btnBackend  = nullptr;  ///< Backend AI: Ollama / avvia-ferma llama-server
    SpinnerWidget*  m_spinServer  = nullptr;  ///< Spinner animato durante polling /health
    StatusBadge*    m_badgeServer = nullptr;  ///< Dot colorato stato server (Offline/Starting/Online)

    /* ── Gestione llama-server avviato dalla GUI ─────────────── */
    QProcess* m_serverProc  = nullptr;  ///< Processo llama-server (nullptr = fermo)
    int       m_serverPort  = 8081;     ///< Porta corrente del server
    QString   m_serverModel;            ///< Nome file modello caricato nel server

    /* ── Servizi di background ───────────────────────────────── */
    HardwareMonitor* m_hw = nullptr;  ///< Thread monitor CPU/RAM/GPU
    AiClient*        m_ai = nullptr;  ///< Client HTTP per Ollama/llama-server

    int m_activePage = 0;  ///< Indice pagina corrente nel QStackedWidget

    /* Timer auto-scarico: ogni 90s verifica se RAM > 40% e AI idle → scarica */
    QTimer* m_idleUnloadTimer = nullptr;

    /* Pannello diagnostica — creato lazy al primo click su 📊 */
    MonitorPanel* m_monitorPanel = nullptr;

    /* Dialog Impostazioni — finestra separata, non-modal */
    QDialog*          m_settingsDlg = nullptr;
    ImpostazioniPage* m_impPage     = nullptr;  ///< puntatore diretto per onHWReady

    /* ── Cronologia chat (SQLite) ────────────────────────────── */
    ChatHistoryDb* m_chatDb   = nullptr;
    QListWidget*   m_chatList = nullptr;

    /** Ricarica la lista chat nella sidebar dal DB. */
    void refreshChatSidebar();
    /** Richiama la chat con id dal DB nella pagina Agenti. */
    void recallChat(int id);

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

protected:
    /**
     * closeEvent — Intercetta la chiusura.
     * Se AI occupato: chiede conferma e offre di scaricare il modello dalla RAM.
     * Termina llama-server se avviato dalla GUI.
     * Esegue cleanup Ollama best-effort senza bloccare la chiusura.
     */
    void closeEvent(QCloseEvent* ev) override;

private slots:
    /** Riceve aggiornamenti periodici CPU/RAM/GPU da HardwareMonitor. */
    void onHWUpdated(SysSnapshot snap);

    /** Riceve le info hardware complete al primo rilevamento (one-shot). */
    void onHWReady(HWInfo hw);

    /** Cambia pagina attiva e aggiorna lo stile dei bottoni sidebar. */
    void navigateTo(int idx);
};
