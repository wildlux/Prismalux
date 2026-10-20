#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QProcess>
#include <QStringList>
#include <QVector>
#include <QJsonObject>
#include <QHash>
#include <functional>
#include <memory>

/* ══════════════════════════════════════════════════════════════
   DecisionEngine — strato di decisione, SEPARATO dagli LLM

   Un "decision model" non è un LLM conversazionale: riceve la
   richiesta (o la risposta) e restituisce solo una SCELTA. Serve a:

     1. classificare()  — prima della richiesta: che tipo di domanda è?
     2. verify()        — dopo la risposta: la risposta regge?

   Entrambe le operazioni sono opzionali e disattivate per default:
   l'utente sceglie SE usarle. Con enabled()==false l'intero strato è
   un no-op e non effettua alcuna chiamata di rete.

   ── Separazione dagli LLM (requisito esplicito) ──────────────────
   I decision model vivono in un registro proprio
   (~/.prismalux/decision_models.json). Quella lista, e non la lista
   dei modelli Ollama grezza, decide cosa è decisionale:
     · un modello nel registro non viene mai offerto come LLM;
     · un modello installato ma assente dal registro non viene mai
       offerto come decision model.
   installedModels() è quindi SEMPRE un sottoinsieme di catalog(),
   per costruzione — non per filtro applicato a posteriori.

   ── Backend ──────────────────────────────────────────────────────
   Implementati: Ollama (POST /api/chat), DwarfStar/ds4 e
   llama-server (POST /v1/chat/completions, OpenAI-compatible).
   ONNX è un runtime locale (non HTTP): usa due modelli lineari
   inclusi in Frameworks/onnx/models, tramite DecisionOnnx. Se ONNX
   Runtime o i modelli mancano, l'errore è esplicito: nessun
   fallback silenzioso su Ollama.

   ── Cache ────────────────────────────────────────────────────────
   SQLite (~/.prismalux/decision_cache.db). Una classificazione
   costa un'intera inferenza LLM; la cache la rende gratuita alla
   seconda occorrenza della stessa domanda, con TTL configurabile.
   Se Qt6::Sql non è disponibile la cache degrada silenziosamente a
   memoria: la feature resta funzionante, solo senza persistenza.
   ══════════════════════════════════════════════════════════════ */
class DecisionEngine : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY(DecisionEngine)
public:
    /* ── Backend di inferenza ────────────────────────────────────── */
    enum DecisionBackend {
        BackendOllama,      ///< POST /api/chat (Ollama)
        BackendDwarfstar,   ///< POST /v1/chat/completions (ds4, OpenAI-compatible)
        BackendOnnx,        ///< ONNX Runtime locale (modelli inclusi)
        BackendLlamaServer, ///< POST /v1/chat/completions (llama-server)
    };

    /* ── Ruolo del modello nel registro ───────────────────────────── */
    enum Role {
        RoleClassifier,  ///< classifica l'intento della richiesta
        RoleVerifier,    ///< riverifica la risposta prodotta dall'LLM
    };

    /** Voce del registro decision model. Diversa dalle liste LLM per
     *  costruzione: questo tipo non ha nome "modello da usare per
     *  rispondere", ha un ruolo nel pipeline di decisione. */
    struct ModelSpec {
        QString name;    ///< nome Ollama (es. "tev1:4b") o alias ds4 (es. "deepseek-v4-flash")
        QString ref;     ///< percorso GGUF o URL (solo backend non-Ollama)
        DecisionBackend backend = BackendOllama;
        Role   role   = RoleClassifier;
        QString note;    ///< descrizione breve mostrata in Impostazioni
        qint64 sizeBytes = 0;  ///< 0 se non installato

        bool isInstalled() const { return sizeBytes > 0; }
    };

    /* ── Risultati ────────────────────────────────────────────────── */
    struct ClassifyResult {
        bool    ok       = false;  ///< la classificazione è avvenuta
        QString intent;            ///< coding|reasoning|chat|other
        double  confidence = 0.0;
        bool    fromCache  = false;
        QString error;             ///< popolata solo se ok==false
    };

    struct VerifyResult {
        bool    ok       = false;  ///< la verifica è avvenuta
        bool    approved = false;  ///< il revisore approva la risposta
        double  confidence = 0.0;
        QString reason;            ///< motivazione (per il retry e la UI)
        QString error;
    };

    /* Tassonomia dell'intento. Le lettere A/B/C/D sono il formato
     * atteso dal classificatore (una sola lettera, niente spiegazioni):
     * è più robusto che chiedere JSON a un modello piccolo. */
    static QVector<QPair<QString, QString>> taxonomy();

    explicit DecisionEngine(QObject* parent = nullptr);
    ~DecisionEngine() override;

    /* ── Configurazione (QSettings, chiavi in PrismaluxPaths) ─────── */
    void loadConfig();
    void saveConfig();

    bool    enabled()   const { return m_enabled; }
    QString model()     const { return m_model; }
    QString host()      const { return m_host; }
    int     port()      const { return m_port; }
    double  threshold() const { return m_threshold; }
    bool    verifyEnabled() const { return m_verify; }
    int     cacheTtlHours() const { return m_cacheTtlHours; }
    DecisionBackend backend() const { return m_backend; }
    /** Ultimo errore del runner ONNX (vuoto se non applicabile). */
    QString onnxError() const;

    void setEnabled(bool on)      { m_enabled = on; }
    void setModel(const QString& m) { m_model = m; }
    void setHostPort(const QString& h, int p);
    void setThreshold(double t)   { m_threshold = t; }
    void setVerifyEnabled(bool on) { m_verify = on; }
    void setCacheTtlHours(int h)  { m_cacheTtlHours = h; }
    void setBackend(DecisionBackend b) { m_backend = b; }

    /* ── Modello per tipologia ──────────────────────────────────────
       La classificazione usa sempre m_model (è il modello che decide
       l'intento). La verifica può invece usare un modello diverso per
       ogni tipologia: un modello può essere più accurato nel giudicare
       codice che conversazione, e questa scelta lo riflette.
       Stringa vuota = usa il modello predefinito. */
    QString modelForIntent(const QString& intent) const;
    void    setModelForIntent(const QString& intent, const QString& model);
    QString modelForIntentSetting(const QString& intent) const;

    /** Il modulo è utilizzabile solo se abilitato, con un modello
     *  scelto e quel modello effettivamente installato. */
    bool active() const { return m_enabled && !m_model.isEmpty() && isInstalled(m_model); }

    /* ── Registro ─────────────────────────────────────────────────
       Entrambe sonometodi statici puri: modificano il solo file JSON
       e non richiedono Ollama, così sono testabili senza backend. */
    static QVector<ModelSpec> catalog();                 ///< registro (letto da disco)
    static void               saveCatalog(const QVector<ModelSpec>&);
    /** Aggiunge (o aggiorna) un modello nel registro. @p backend serve
     *  ai backend non-Ollama: senza, un modello scaricato da ds4 o
     *  registrato per llama-server resterebbe marcato Ollama e non
     *  verrebbe mai riconosciuto da refreshInstalled(). */
    static bool               registerModel(const QString& name, Role role,
                                            const QString& note = {},
                                            DecisionBackend backend = BackendOllama);

    /** Decision model installati = intersezione registro ∩ backend.
     *  Non legge mai un modello fuori dal registro. */
    QVector<ModelSpec> installedModels() const { return m_installed; }

    /** Chiede al backend e aggiorna installedModels().
     *  Non bloccante: emette installedReady() al termine. */
    void refreshInstalled();

    /* ── Download ───────────────────────────────────────────────────
       Stesso meccanismo di ManutenzionePage (QProcess) ma su un
       canale separato: scaricare un decision model non deve toccare
       lo stato del download LLM. Per DwarfStar usa download_model.sh.
       Per llama-server non scarica: registra l'alias e verifica che
       sia esposto su /v1/models. Per ONNX verifica i modelli inclusi. */
    void downloadModel(const QString& name);
    void cancelDownload();
    bool downloadRunning() const { return m_dlProc && m_dlProc->state() != QProcess::NotRunning; }
    QString downloadingModel() const { return m_dlModel; }

    /* ── Operazioni ───────────────────────────────────────────────────
       Callback invece che segnali: le chiamate sono indipendenti e
       sovrapponibili, e un segnale condiviso sarebbe ambiguo su
       quale richiesta sia arrivata. La callback gira nel thread GUI. */
    using ClassifyFn = std::function<void(const ClassifyResult&)>;
    using VerifyFn   = std::function<void(const VerifyResult&)>;

    void classify(const QString& text, ClassifyFn cb);
    void verify(const QString& question, const QString& answer, VerifyFn cb);
    /** Variante che sceglie il revisore in base alla tipologia rilevata.
     *  @p intent vuoto o non configurato → modello predefinito. */
    void verify(const QString& question, const QString& answer,
                const QString& intent, VerifyFn cb);

    /* ── Cache ───────────────────────────────────────────────────────── */
    struct CacheStats { int entries = 0; qint64 bytes = 0; };
    CacheStats cacheStats() const;
    void clearCache();
    int  cacheSize() const { return m_memCache.size(); }

    /** Parsing della risposta del classificatore, esposto per i test:
     *  dato l'output grezso del modello, restituisce l'intento.
     *  Riconosce sia la lettera secca ("A") sia la forma rumorosa
     *  ("A) coding", "Risposta: B"), come nel riferimento Python. */
    static QString parseLetter(const QString& raw);

private slots:
    void onTagsReply();
    void onClassifyReply();
    void onVerifyReply();
    void onDownloadReadyRead();
    void onDownloadFinished(int code, QProcess::ExitStatus);

private:
    bool isInstalled(const QString& name) const;

    /** Carica pigramente i modelli ONNX. Se un tentativo precedente è
     *  fallito, l'errore resta disponibile: non si ritenta a ogni
     *  classificazione (refreshInstalled() resetta il runner). */
    void _ensureOnnx();

    /** Scrive il catalogo di primo avvio solo se il file non esiste
     *  già: da lì in poi il registro appartiene all'utente. */
    void _seedCatalog();

    /** POST al backend. Ollama: /api/chat. DwarfStar e llama-server:
     *  /v1/chat/completions (OpenAI-compatible). @p think=false
     *  disattiva il reasoning dei modelli che lo supportano: qui il
     *  modello deve rispondere in pochi token, non ragionare.
     *  Restituisce il reply: la ownership resta a chi lo assegna. */
    QNetworkReply* _chatOnce(const QString& model,
                             const QString& systemPrompt,
                             const QString& userPayload,
                             int numPredict);

    /** Estrae il contenuto testuale dalla risposta del backend.
     *  Ollama: message.content. OpenAI/DwarfStar: choices[0].message.content. */
    static QString _extractContent(const QJsonObject& obj);

    QString _cacheKey(const QString& text, const QString& model) const;
    bool    _cacheGet(const QString& key, QString* intent, double* conf) const;
    void    _cachePut(const QString& key, const QString& intent, double conf);
    void    _cacheOpen();

    /* ── Configurazione ── */
    bool    m_enabled  = false;   ///< opt-in: default OFF
    QString m_model;
    QString m_host     = "127.0.0.1";
    int     m_port     = 11434;
    double  m_threshold = 0.70;
    bool    m_verify   = true;
    int     m_cacheTtlHours = 168;
    DecisionBackend m_backend = BackendOllama;
    QHash<QString, QString> m_modelByIntent;  ///< tipologia -> modello (vuoto = default)

    QVector<ModelSpec> m_installed;
    QNetworkAccessManager* m_nam = nullptr;
    std::unique_ptr<class DecisionOnnx> m_onnx;  ///< runner locale (solo BackendOnnx)

    /* ── Richieste in corso ── */
    QPointer<QNetworkReply> m_tagsReply;
    QPointer<QNetworkReply> m_classifyReply;
    QPointer<QNetworkReply> m_verifyReply;
    QString m_pendingClassifyText;
    QString m_pendingVerifyQuestion;
    QString m_pendingVerifyAnswer;
    ClassifyFn m_classifyCb;
    VerifyFn   m_verifyCb;

    /* ── Cache (SQLite con fallback in memoria) ── */
    bool m_dbOk       = false;   ///< SQLite disponibile
    int  m_dbTtlHours = 168;

    /** Voce della copia in memoria: la cache su disco tiene il
     *  timestamp in una colonna, qui serve accanto al risultato. */
    struct MemEntry {
        ClassifyResult res;
        qint64         ts = 0;
    };

    QHash<QString, MemEntry> m_memCache;  ///< fallback + copia calda
    qint64 m_memCacheEpoch = 0;           ///< ms epoch per il TTL in memoria

    /* ── Download ── */
    QProcess* m_dlProc = nullptr;
    QString   m_dlModel;
    DecisionBackend m_dlBackend = BackendOllama;  ///< backend al momento del download

signals:
    void installedReady(int count);
    void downloadProgress(const QString& model, const QString& status);
    void downloadFinished(const QString& model, bool ok);
    void error(const QString& msg);
};