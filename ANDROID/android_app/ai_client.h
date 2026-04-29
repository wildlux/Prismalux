#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QStringList>
#include <QElapsedTimer>
#include <QJsonArray>

/* ══════════════════════════════════════════════════════════════
   AiClient (Mobile) — client HTTP per Ollama su LAN locale.

   Differenze rispetto alla versione desktop Prismalux:
   - Solo backend Ollama via HTTP (no llama-server, no LlamaLocal)
   - Nessun prefisso honesty/caveman (configurabile via Settings)
   - API semplificata: setServer() unico punto di configurazione
   - Stesso protocollo streaming NDJSON /api/chat + /api/generate
   ══════════════════════════════════════════════════════════════ */
class AiClient : public QObject {
    Q_OBJECT
public:
    explicit AiClient(QObject* parent = nullptr);

    /* ── Configurazione ── */
    void setServer(const QString& host, int port, const QString& model);
    void setServer(const QString& host, int port) { setServer(host, port, m_model); }

    QString host()  const { return m_host; }
    int     port()  const { return m_port; }
    QString model() const { return m_model; }
    bool    busy()  const { return m_reply != nullptr; }

    /* ── Parametri sampilng (opzionali) ── */
    void setTemperature(double t)   { m_temperature = t; }
    void setNumPredict(int n)       { m_numPredict = n; }
    void setNumCtx(int ctx)         { m_numCtx = ctx; }

    /* ── Streaming chat ──────────────────────────────────────────
       Invia un messaggio a /api/chat con history opzionale.
       Emette token() per ogni chunk, finished() al termine.
       @param sys     System prompt (può essere vuoto)
       @param msg     Messaggio utente
       @param history Turni precedenti [{role,content},...] (può essere vuoto)
       @return        Request ID incrementale */
    quint64 chat(const QString& sys, const QString& msg,
                 const QJsonArray& history = QJsonArray());

    /* ── Generate single-shot (per RAG, senza history) ── */
    quint64 generate(const QString& sys, const QString& prompt);

    void abort();

    /* ── Lista modelli dal server ── */
    void fetchModels();

    /* ── Request ID corrente ── */
    quint64 currentReqId() const { return m_reqId; }

signals:
    void token(const QString& chunk);
    void finished(const QString& full);
    void aborted();
    void error(const QString& msg);
    void modelsReady(const QStringList& models);
    void modelChanged(const QString& model);

private slots:
    void onReadyRead();
    void onFinished();
    void onModelsReply();

private:
    QNetworkAccessManager* m_nam   = nullptr;
    QNetworkReply*         m_reply = nullptr;

    QString m_host        = "192.168.1.100";
    int     m_port        = 11434;
    QString m_model       = "llama3.2:1b";
    QString m_accum;
    bool    m_generateMode = false;

    /* Parametri sampling */
    double  m_temperature = 0.1;
    int     m_numPredict  = 1024;
    int     m_numCtx      = 4096;

    /* Throttle */
    QElapsedTimer m_throttle;

    /* Request ID */
    quint64 m_reqId = 0;
};
