#pragma once
#include <QObject>
#include <QThread>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSslError>
#include <atomic>

class ThermalMonitor;
class InferenceWorker;   /* defined in local_llm_client.cpp */

/*
   LocalLlmClient -- inferenza LLM locale via llama.cpp (NEON arm64).

   Funzionalita:
     - Carica un modello GGUF (CPU layers, NEON ottimizzato)
     - Esegue chat streaming su thread worker separato
     - Monitora la temperatura CPU (ThermalMonitor)
       >= 72 C: pausa di almeno 5 s
       >= 78 C: pausa finche non scende sotto 65 C
     - Download modello via HTTPS con progress bar

   API compatibile con AiClient: stessi segnali token/finished/error.
*/
struct InferenceControl {
    std::atomic<bool> abort{false};
    std::atomic<bool> thermPaused{false};
};

class LocalLlmClient : public QObject {
    Q_OBJECT
public:
    explicit LocalLlmClient(QObject* parent = nullptr);
    ~LocalLlmClient() override;

    bool loadModel(const QString& modelPath);
    void unloadModel();
    bool isLoaded()  const { return m_loaded; }
    bool busy()      const { return m_busy; }

    /* Stessa API di AiClient */
    void chat(const QString& sys, const QString& msg);
    void abort();

    /* Temperatura istantanea dal ThermalMonitor */
    float currentTemp() const;

    /* Download modello in background */
    void downloadModel(const QString& url, const QString& destPath);
    void cancelDownload();

    /* Catalogo modelli scaricabili */
    struct ModelSpec {
        const char* displayName;
        const char* filename;
        const char* url;
        int         sizeMB;
    };
    static const ModelSpec kModels[];
    static constexpr int   kNumModels = 2;

    /* Percorso, URL e stato del file parziale */
    static QString defaultModelPath();           ///< percorso modello indice 1 (1.5B)
    static QString modelPath(int idx);           ///< percorso per modello idx
    static bool    modelExists();                ///< true se QUALSIASI modello esiste
    static bool    modelExists(int idx);
    static bool    hasPartialDownload(int idx);
    static qint64  partialDownloadSize(int idx);
    static QString firstAvailableModelPath();    ///< primo modello esistente, o ""
    static QString loadedModelName(const QString& path); ///< "0.5B" / "1.5B" / ...

    /* Mantenuti per retrocompatibilità */
    static constexpr const char* kModelFilename =
        "qwen2.5-1.5b-instruct-q4_k_m.gguf";

signals:
    void token(const QString& chunk);
    void finished(const QString& full);
    void error(const QString& msg);
    void aborted();

    void modelLoaded(const QString& path);
    void loadError(const QString& msg);

    void downloadProgress(qint64 received, qint64 total);
    void downloadFinished(const QString& path);
    void downloadError(const QString& msg);

    void thermalPause(float celsius);
    void thermalResume();
    void tempUpdated(float celsius);

private slots:
    void onWorkerToken(const QString& chunk);
    void onWorkerFinished(const QString& full);
    void onWorkerError(const QString& msg);
    void onWorkerAborted();
    void onWorkerThreadDone();

    void onDownloadReadyRead();
    void onDownloadProgress(qint64 received, qint64 total);
    void onDownloadDone();
    void onSslErrors(const QList<QSslError>& errors);

    void onThermalPause(float celsius);
    void onThermalPauseExpired();
    void onThermalResume();

private:
    void*            m_llamaModel   = nullptr;
    InferenceWorker* m_worker       = nullptr;
    QThread*         m_workerThread = nullptr;
    ThermalMonitor*  m_thermal      = nullptr;
    InferenceControl m_ctrl;

    QNetworkAccessManager* m_nam     = nullptr;
    QNetworkReply*         m_dlReply = nullptr;
    QFile*                 m_dlFile  = nullptr;

    bool    m_loaded         = false;
    bool    m_busy           = false;
    qint64  m_dlResumingFrom = 0;

    float m_temperature = 0.7f;
    int   m_maxTokens   = 1024;
};
