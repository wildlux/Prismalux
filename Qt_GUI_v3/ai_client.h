#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProcess>
#include <QStringList>
#include <QElapsedTimer>

/* ══════════════════════════════════════════════════════════════
   AiClient — Ollama / llama-server (HTTP) + llama.cpp locale (QProcess)
   ══════════════════════════════════════════════════════════════ */
class AiClient : public QObject {
    Q_OBJECT
public:
    enum Backend { Ollama, LlamaServer, LlamaLocal };

    explicit AiClient(QObject* parent = nullptr);

    /* ── configurazione backend HTTP ── */
    void setBackend(Backend b, const QString& host, int port, const QString& model);

    /* ── configurazione backend locale ── */
    void setLocalBackend(const QString& llamaBin, const QString& modelPath);

    /* ── chat ── */
    void chat(const QString& systemPrompt, const QString& userMsg);
    void abort();

    /* ── accessori ── */
    Backend     backend()    const { return m_backend; }
    QString     host()       const { return m_host; }
    int         port()       const { return m_port; }
    QString     model()      const { return m_model; }
    QString     localModel() const { return m_localModel; }
    QStringList models()     const { return m_models; }
    bool        busy()       const { return m_reply != nullptr || m_localBusy; }

    void fetchModels();

    /** Chiamato da MainWindow::onHWUpdated ogni 2s per aggiornare il dato RAM. */
    void   setRamFreePct(double pct) { m_ramFreePct = pct; }
    double ramFreePct()        const { return m_ramFreePct; }

    /**
     * unloadModel() — Scarica il modello dalla RAM di Ollama inviando
     * una richiesta POST /api/generate con keep_alive=0.
     * Fire-and-forget: non emette segnali, ignora errori.
     * Non fa nulla se il modello non risulta caricato (m_modelLoaded == false),
     * per evitare che Ollama lo carichi solo per scaricarlo subito.
     */
    void unloadModel();

    /**
     * isModelLoaded() — true se Ollama ha ricevuto almeno una risposta di inferenza
     * con questo client e il modello non è stato ancora scaricato.
     * Usato dal timer auto-scarico per evitare cicli inutili load→unload.
     */
    bool isModelLoaded() const { return m_modelLoaded; }

signals:
    void token(const QString& chunk);
    void finished(const QString& full);
    void aborted();                        /* emesso da abort() — UI può resettarsi */
    void error(const QString& msg);
    void modelsReady(const QStringList& list);

    /**
     * requestStarted — emesso all'inizio di chat(), prima che parta la richiesta.
     * Usato dal MonitorPanel per misurare TTFT e durata totale.
     * @param model    Nome del modello attivo al momento della richiesta
     * @param backend  "Ollama" | "llama-server" | "llama-local"
     */
    void requestStarted(const QString& model, const QString& backend);

private slots:
    void onReadyRead();
    void onFinished();
    void onModelsReply();
    void onLocalReadyRead();
    void onLocalFinished(int exitCode, QProcess::ExitStatus);

private:
    /* HTTP */
    QNetworkAccessManager* m_nam   = nullptr;
    QNetworkReply*         m_reply = nullptr;
    Backend   m_backend   = Ollama;
    QString   m_host      = "127.0.0.1";
    int       m_port      = 11434;
    QString   m_model;
    QStringList m_models;
    QString   m_accum;
    bool      m_busy_guard = false;

    /* LlamaLocal */
    QProcess* m_localProc  = nullptr;
    QString   m_llamaBin;
    QString   m_localModel;
    bool      m_localBusy  = false;
    QString   m_localAccum;

    /*
     * Cache modelli — evita fetch ridondanti quando più pagine chiamano
     * fetchModels() sullo stesso backend entro 30 secondi.
     * La cache viene invalidata se il backend o la porta cambiano.
     */
    static constexpr qint64 kCacheTtlMs = 30'000;  ///< 30 secondi
    QElapsedTimer m_cacheTimer;     ///< misura il tempo dall'ultimo fetch andato a buon fine
    Backend       m_cachedBackend  = Ollama;  ///< backend dell'ultima cache valida
    int           m_cachedPort     = 11434;   ///< porta dell'ultima cache valida
    bool          m_cacheValid     = false;   ///< true se la cache è usabile

    /* Throttle: evita doppio-clic che lancia due richieste in <400ms */
    QElapsedTimer m_throttleTimer;

    /* Percentuale RAM libera (aggiornata da MainWindow ogni 2s) */
    double        m_ramFreePct     = 100.0;

    /* true dopo la prima inferenza completata; false dopo unloadModel().
     * Evita che il timer auto-scarico faccia caricare il modello solo per scaricarlo. */
    bool          m_modelLoaded    = false;
};
