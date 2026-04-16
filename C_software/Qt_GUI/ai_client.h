#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProcess>
#include <QStringList>
#include <QElapsedTimer>
#include <QJsonArray>

/* ══════════════════════════════════════════════════════════════
   AiChatParams — parametri campionamento che riducono le allucinazioni.
   Valori di default conservativi (bassa temperatura = maggiore coerenza).
   ══════════════════════════════════════════════════════════════ */
/* ══════════════════════════════════════════════════════════════
   AiChatParams — unica fonte di verità per i parametri AI.
   Tutti i valori vengono letti da ~/.prismalux/ai_params.json.
   Se il file non esiste viene creato con i default Brutal Honesty.
   Nessun valore è hardcoded nel codice sorgente.
   ══════════════════════════════════════════════════════════════ */
struct AiChatParams {
    double temperature    = 0.05;
    double top_p          = 0.85;
    int    top_k          = 20;
    double repeat_penalty = 1.20;
    int    num_predict    = 2048;
    int    num_ctx        = 8192;
    bool   honesty_prefix = true;
    bool   caveman_mode   = false;  ///< risposte dirette, senza convenevoli né riepiloghi

    /* ── Unica fonte: ~/.prismalux/ai_params.json ── */
    static QString     filePath();               ///< percorso del file
    static AiChatParams load();                  ///< legge il file (lo crea se mancante)
    static void         save(const AiChatParams&); ///< scrive il file
};

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

    /* ── parametri di campionamento (anti-allucinazione) ── */
    void setChatParams(const AiChatParams& p) { m_params = p; }
    AiChatParams chatParams() const { return m_params; }

    /* ── Classificazione query ─────────────────────────────────────
       Determina se una query è semplice o complessa per decidere
       automaticamente think=true/false e num_predict dinamico.
       QueryAuto = comportamento precedente (compatibilità con codice esistente). */
    enum QueryType { QueryAuto, QuerySimple, QueryComplex };
    static QueryType classifyQuery(const QString& text);

    /* ── chat ── */

    /** Overload legacy — compatibile con tutto il codice esistente.
     *  Equivale a chat(sys, msg, {}, QueryAuto): nessuna storia, nessun think. */
    void chat(const QString& systemPrompt, const QString& userMsg);

    /** Chat con storia compressa e tipo query.
     *  @param history  turni precedenti come array JSON [{role,content},...]
     *                  costruito da OracoloPage::buildHistoryArray()
     *  @param qt       QuerySimple → think=false, num_predict=512
     *                  QueryComplex → think=true, num_predict dal config
     *                  QueryAuto    → nessun think, num_predict dal config */
    void chat(const QString& systemPrompt, const QString& userMsg,
              const QJsonArray& history, QueryType qt);

    /** Genera risposta singola via /api/generate (Ollama).
     *  Preferito per query RAG senza storia attiva: più leggero di /api/chat
     *  perché non gestisce messaggi multipli.
     *  Fallback automatico a chat() se backend != Ollama. */
    void generate(const QString& systemPrompt, const QString& prompt, QueryType qt);

    void chatWithImage(const QString& systemPrompt, const QString& userMsg,
                       const QByteArray& imageBase64,
                       const QString& mimeType = "image/jpeg");
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

    /**
     * fetchEmbedding — richiede l'embedding vettoriale di @p text al backend.
     * Supportato: Ollama (/api/embeddings) e llama-server (/v1/embeddings).
     * Non bloccante: emette embeddingReady o embeddingError al termine.
     * Usa un QNetworkReply separato: non interferisce con la chat in corso.
     */
    void fetchEmbedding(const QString& text);

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
    /** Emesso quando il modello attivo cambia (es. l'utente seleziona un altro modello in Impostazioni). */
    void modelChanged(const QString& newModel);
    /** Emesso quando fetchEmbedding() ha ottenuto il vettore dal backend. */
    void embeddingReady(const QVector<float>& vec);
    /** Emesso in caso di errore durante fetchEmbedding(). */
    void embeddingError(const QString& msg);

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
    bool      m_busy_guard    = false;
    /** true se l'ultima richiesta attiva usa /api/generate invece di /api/chat.
     *  Usato da onReadyRead() per estrarre il campo "response" anziché
     *  "message.content". Azzerato in abort() e onFinished(). */
    bool      m_isGenerateMode = false;

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

    /* Parametri campionamento */
    AiChatParams  m_params;

    /* true dopo la prima inferenza completata; false dopo unloadModel().
     * Evita che il timer auto-scarico faccia caricare il modello solo per scaricarlo. */
    bool          m_modelLoaded    = false;

    /* Accumula il blocco <thinking> per i modelli (es. qwen3.5) che usano il campo
     * separato "message.thinking" invece di includere <think>...</think> nel content.
     * Se a fine stream m_accum è vuoto ma questo non lo è, viene wrappato in
     * <think>...</think> e passato a finished() come fallback. */
    QString       m_thinkingAccum;
};
