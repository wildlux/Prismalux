#include "local_llm_client.h"
#include "thermal_monitor.h"

#include <QDir>
#include <QStandardPaths>
#include <QTimer>
#include <QMetaObject>

#ifdef HAVE_LOCAL_LLM
#include "llama.h"
#include <vector>
#include <string>
#include <cstring>
#endif

/* ══════════════════════════════════════════════════════════════
   InferenceWorker — gira sul thread worker separato.
   Riceve sys+msg via invokeMethod, emette token/finished/error.
   ══════════════════════════════════════════════════════════════ */
class InferenceWorker : public QObject {
    Q_OBJECT
public:
    explicit InferenceWorker(void* llamaModel,
                              InferenceControl* ctrl,
                              float temperature,
                              int   maxTokens)
        : m_model(llamaModel)
        , m_ctrl(ctrl)
        , m_temperature(temperature)
        , m_maxTokens(maxTokens)
    {}

public slots:
    void run(const QString& sys, const QString& msg);

signals:
    void token(const QString& chunk);
    void finished(const QString& full);
    void error(const QString& msg);
    void aborted();

private:
    void*             m_model;
    InferenceControl* m_ctrl;
    float             m_temperature;
    int               m_maxTokens;
};

/* ── Implementazione InferenceWorker::run ─────────────────────── */
void InferenceWorker::run(const QString& sys, const QString& msg)
{
#ifndef HAVE_LOCAL_LLM
    Q_UNUSED(sys) Q_UNUSED(msg)
    emit error("LLM locale non compilato (HAVE_LOCAL_LLM mancante)");
    return;
#else
    if (!m_model) { emit error("Modello non caricato"); return; }

    auto* model_ptr = static_cast<llama_model*>(m_model);
    const llama_vocab* vocab = llama_model_get_vocab(model_ptr);

    /* ── 1. Crea contesto per questa inferenza ── */
    llama_context_params cp = llama_context_default_params();
    cp.n_ctx   = 2048;
    cp.n_batch = 512;
    llama_context* ctx = llama_new_context_with_model(model_ptr, cp);
    if (!ctx) { emit error("Impossibile creare contesto LLM"); return; }

    /* ── 2. Formatta il prompt (ChatML/Qwen2 hardcoded — b9181 rimuove il
            parametro model da llama_chat_apply_template) ── */
    const std::string sys_s = sys.toStdString();
    const std::string msg_s = msg.toStdString();

    std::vector<llama_chat_message> msgs;
    if (!sys_s.empty())
        msgs.push_back({"system", sys_s.c_str()});
    msgs.push_back({"user", msg_s.c_str()});

    std::vector<char> tmpl_buf(16384);
    /* nullptr = usa il template integrato nel modello */
    int n_tmpl = llama_chat_apply_template(
        nullptr,
        msgs.data(), (size_t)msgs.size(), true,
        tmpl_buf.data(), (int32_t)tmpl_buf.size());
    if (n_tmpl < 0 || n_tmpl == 0) {
        /* Fallback: formato Qwen2/ChatML */
        const std::string fb =
            "<|im_start|>system\n" + sys_s + "<|im_end|>\n"
            "<|im_start|>user\n"   + msg_s + "<|im_end|>\n"
            "<|im_start|>assistant\n";
        n_tmpl = (int)fb.size();
        tmpl_buf.resize(n_tmpl + 1);
        memcpy(tmpl_buf.data(), fb.data(), n_tmpl);
    } else if (n_tmpl > (int)tmpl_buf.size()) {
        tmpl_buf.resize(n_tmpl);
        llama_chat_apply_template(
            nullptr,
            msgs.data(), (size_t)msgs.size(), true,
            tmpl_buf.data(), (int32_t)tmpl_buf.size());
    }
    const std::string prompt(tmpl_buf.data(), n_tmpl);

    /* ── 3. Tokenizza (b9181: llama_tokenize prende vocab, non model) ── */
    const int n_ctx = (int)llama_n_ctx(ctx);
    std::vector<llama_token> tokens(n_ctx);
    const int n_toks = llama_tokenize(
        vocab, prompt.c_str(), (int32_t)prompt.size(),
        tokens.data(), n_ctx, true, true);
    if (n_toks < 0) {
        llama_free(ctx);
        emit error("Errore tokenizzazione (prompt troppo lungo?)");
        return;
    }
    tokens.resize(n_toks);

    /* ── 4. Primo decode (prompt prefill) ── */
    llama_batch batch = llama_batch_get_one(tokens.data(), (int32_t)tokens.size());
    if (llama_decode(ctx, batch) != 0) {
        llama_free(ctx);
        emit error("Errore decode iniziale");
        return;
    }

    /* ── 5. Campionatore ── */
    llama_sampler* smpl = llama_sampler_chain_init(
        llama_sampler_chain_default_params());
    llama_sampler_chain_add(smpl, llama_sampler_init_top_k(40));
    llama_sampler_chain_add(smpl, llama_sampler_init_top_p(0.90f, 1));
    llama_sampler_chain_add(smpl, llama_sampler_init_temp(m_temperature));
    llama_sampler_chain_add(smpl, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

    /* ── 6. Loop di generazione ── */
    QString accum;
    char    piece_buf[256];

    for (int i = 0; i < m_maxTokens; ++i) {

        /* Pausa termica */
        while (m_ctrl->thermPaused.load(std::memory_order_relaxed)
               && !m_ctrl->abort.load(std::memory_order_relaxed)) {
            QThread::msleep(200);
        }
        if (m_ctrl->abort.load(std::memory_order_relaxed)) break;

        llama_token next = llama_sampler_sample(smpl, ctx, -1);
        llama_sampler_accept(smpl, next);

        /* b9181: llama_vocab_is_eog invece di llama_token_is_eog */
        if (llama_vocab_is_eog(vocab, next)) break;

        /* b9181: llama_token_to_piece prende vocab, non ctx */
        const int n_piece = llama_token_to_piece(
            vocab, next, piece_buf, (int32_t)sizeof(piece_buf) - 1, 0, true);
        if (n_piece > 0) {
            piece_buf[n_piece] = '\0';
            const QString chunk = QString::fromUtf8(piece_buf, n_piece);
            accum += chunk;
            emit token(chunk);
        }

        llama_batch next_batch = llama_batch_get_one(&next, 1);
        if (llama_decode(ctx, next_batch) != 0) break;
    }

    llama_sampler_free(smpl);
    llama_free(ctx);

    if (m_ctrl->abort.load()) emit aborted();
    else                       emit finished(accum);
#endif
}

/* ══════════════════════════════════════════════════════════════
   LocalLlmClient — implementazione
   ══════════════════════════════════════════════════════════════ */

LocalLlmClient::LocalLlmClient(QObject* parent)
    : QObject(parent)
    , m_thermal(new ThermalMonitor(this))
    , m_nam(new QNetworkAccessManager(this))
{
    connect(m_thermal, &ThermalMonitor::tempUpdated,
            this,      &LocalLlmClient::onThermalPause);
    connect(m_thermal, &ThermalMonitor::pauseRequired,
            this,      &LocalLlmClient::onThermalPause);
    connect(m_thermal, &ThermalMonitor::resumeAllowed,
            this,      &LocalLlmClient::onThermalResume);
    /* Ri-emetti la temperatura alla UI */
    connect(m_thermal, &ThermalMonitor::tempUpdated,
            this,      &LocalLlmClient::tempUpdated);
}

LocalLlmClient::~LocalLlmClient()
{
    abort();
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait(3000);
    }
    unloadModel();
}

/* ── Catalogo modelli ────────────────────────────────────────── */
const LocalLlmClient::ModelSpec LocalLlmClient::kModels[LocalLlmClient::kNumModels] = {
    { "Qwen2.5-0.5B Q4_K_M  (~395 MB)  \xe2\x86\x90 test",
      "qwen2.5-0.5b-instruct-q4_k_m.gguf",
      "https://huggingface.co/Qwen/Qwen2.5-0.5B-Instruct-GGUF/resolve/main/"
      "qwen2.5-0.5b-instruct-q4_k_m.gguf",
      395 },
    { "Qwen2.5-1.5B Q4_K_M  (~940 MB)",
      "qwen2.5-1.5b-instruct-q4_k_m.gguf",
      "https://huggingface.co/Qwen/Qwen2.5-1.5B-Instruct-GGUF/resolve/main/"
      "qwen2.5-1.5b-instruct-q4_k_m.gguf",
      940 },
};

/* ── defaultModelPath ─────────────────────────────────────────── */
QString LocalLlmClient::modelPath(int idx)
{
    const QString base = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation) + "/models/";
    if (idx < 0 || idx >= kNumModels) return base + kModelFilename;
    return base + kModels[idx].filename;
}

QString LocalLlmClient::defaultModelPath()
{
    return modelPath(1);   /* 1.5B — default storico */
}

bool LocalLlmClient::modelExists(int idx)
{
    return QFile::exists(modelPath(idx));
}

bool LocalLlmClient::modelExists()
{
    for (int i = 0; i < kNumModels; ++i)
        if (modelExists(i)) return true;
    return false;
}

bool LocalLlmClient::hasPartialDownload(int idx)
{
    return QFile::exists(modelPath(idx) + ".tmp");
}

qint64 LocalLlmClient::partialDownloadSize(int idx)
{
    const QFileInfo fi(modelPath(idx) + ".tmp");
    return fi.exists() ? fi.size() : 0;
}

QString LocalLlmClient::firstAvailableModelPath()
{
    for (int i = 0; i < kNumModels; ++i)
        if (modelExists(i)) return modelPath(i);
    return {};
}

QString LocalLlmClient::loadedModelName(const QString& path)
{
    for (int i = 0; i < kNumModels; ++i)
        if (path.endsWith(kModels[i].filename))
            return QString(kModels[i].filename)
                       .section('-', 1, 2)   /* "0.5b" o "1.5b" */
                       .toUpper();
    return "locale";
}

/* ── loadModel ───────────────────────────────────────────────── */
bool LocalLlmClient::loadModel(const QString& modelPath)
{
#ifndef HAVE_LOCAL_LLM
    Q_UNUSED(modelPath)
    emit loadError("LLM locale non disponibile in questa build");
    return false;
#else
    unloadModel();

    llama_model_params mp = llama_model_default_params();
    mp.n_gpu_layers = 0;    /* CPU-only: NEON arm64 (Vulkan NDK headers non compilabili) */

    auto* m = llama_load_model_from_file(modelPath.toLocal8Bit().constData(), mp);
    if (!m) {
        emit loadError("Impossibile caricare: " + modelPath);
        return false;
    }
    m_llamaModel = m;
    m_loaded     = true;
    /* Avvia il monitor termico solo quando il modello è attivo */
    m_thermal->start(3000);
    emit modelLoaded(modelPath);
    return true;
#endif
}

/* ── unloadModel ─────────────────────────────────────────────── */
void LocalLlmClient::unloadModel()
{
#ifdef HAVE_LOCAL_LLM
    if (m_llamaModel) {
        llama_model_free(static_cast<llama_model*>(m_llamaModel));
        m_llamaModel = nullptr;
    }
#endif
    m_loaded = false;
    m_thermal->stop();
}

/* ── chat ────────────────────────────────────────────────────── */
void LocalLlmClient::chat(const QString& sys, const QString& msg)
{
    if (m_busy || !m_loaded) {
        emit error("LLM locale occupato o modello non caricato");
        return;
    }
    m_busy = true;
    m_ctrl.abort.store(false);
    m_ctrl.thermPaused.store(false);

    /* Crea worker e thread */
    m_worker       = new InferenceWorker(m_llamaModel, &m_ctrl,
                                          m_temperature, m_maxTokens);
    m_workerThread = new QThread(this);
    m_worker->moveToThread(m_workerThread);

    connect(m_worker, &InferenceWorker::token,
            this,     &LocalLlmClient::onWorkerToken);
    connect(m_worker, &InferenceWorker::finished,
            this,     &LocalLlmClient::onWorkerFinished);
    connect(m_worker, &InferenceWorker::error,
            this,     &LocalLlmClient::onWorkerError);
    connect(m_worker, &InferenceWorker::aborted,
            this,     &LocalLlmClient::onWorkerAborted);
    connect(m_workerThread, &QThread::finished,
            this,           &LocalLlmClient::onWorkerThreadDone);

    m_workerThread->start();
    /* Avvia l'inferenza nel thread worker */
    QMetaObject::invokeMethod(m_worker, "run",
                               Qt::QueuedConnection,
                               Q_ARG(QString, sys),
                               Q_ARG(QString, msg));
}

/* ── abort ───────────────────────────────────────────────────── */
void LocalLlmClient::abort()
{
    if (!m_busy) return;
    m_ctrl.abort.store(true);
    m_ctrl.thermPaused.store(false);  /* sblocca pausa termica */
}

float LocalLlmClient::currentTemp() const
{
    return m_thermal->currentTemp();
}

/* ── Slot worker ─────────────────────────────────────────────── */
void LocalLlmClient::onWorkerToken(const QString& chunk)
{
    emit token(chunk);
}

void LocalLlmClient::onWorkerFinished(const QString& full)
{
    m_busy = false;
    if (m_workerThread) m_workerThread->quit();
    emit finished(full);
}

void LocalLlmClient::onWorkerError(const QString& msg)
{
    m_busy = false;
    if (m_workerThread) m_workerThread->quit();
    emit error(msg);
}

void LocalLlmClient::onWorkerAborted()
{
    m_busy = false;
    if (m_workerThread) m_workerThread->quit();
    emit aborted();
}

void LocalLlmClient::onWorkerThreadDone()
{
    if (m_worker) {
        m_worker->deleteLater();
        m_worker = nullptr;
    }
    if (m_workerThread) {
        m_workerThread->deleteLater();
        m_workerThread = nullptr;
    }
}

/* ── Slot termici ────────────────────────────────────────────── */
void LocalLlmClient::onThermalPause(float celsius)
{
    if (celsius < ThermalMonitor::kPauseTemp) return;
    if (m_ctrl.thermPaused.load()) return;   /* già in pausa */

    m_ctrl.thermPaused.store(true);
    emit thermalPause(celsius);
    /* Pausa minima garantita di 5 secondi */
    QTimer::singleShot(5000, this, &LocalLlmClient::onThermalPauseExpired);
}

void LocalLlmClient::onThermalPauseExpired()
{
    /* Dopo 5 s: riprendi solo se la temp è tornata sotto kWarnTemp */
    if (!m_thermal->isPaused()) {
        m_ctrl.thermPaused.store(false);
        emit thermalResume();
    }
    /* Altrimenti resta in pausa finché ThermalMonitor emette resumeAllowed */
}

void LocalLlmClient::onThermalResume()
{
    if (!m_ctrl.thermPaused.load()) return;
    m_ctrl.thermPaused.store(false);
    emit thermalResume();
}

/* ── Download modello ────────────────────────────────────────── */
void LocalLlmClient::downloadModel(const QString& url, const QString& destPath)
{
    if (m_dlReply) { emit downloadError("Download già in corso"); return; }

    QDir().mkpath(QFileInfo(destPath).absolutePath());
    const QString tmpPath = destPath + ".tmp";
    m_dlResumingFrom = 0;

    /* Controlla se esiste un file parziale (ripresa download) */
    const QFileInfo tmpInfo(tmpPath);
    if (tmpInfo.exists() && tmpInfo.size() > 0) {
        m_dlResumingFrom = tmpInfo.size();
        m_dlFile = new QFile(tmpPath, this);
        if (!m_dlFile->open(QIODevice::Append)) {
            emit downloadError("Impossibile aprire file parziale per ripresa");
            m_dlFile->deleteLater(); m_dlFile = nullptr;
            return;
        }
    } else {
        m_dlFile = new QFile(tmpPath, this);
        if (!m_dlFile->open(QIODevice::WriteOnly)) {
            emit downloadError("Impossibile aprire file destinazione: " + destPath);
            m_dlFile->deleteLater(); m_dlFile = nullptr;
            return;
        }
    }

    QNetworkRequest req{QUrl{url}};
    /* NoLessSafe: HTTPS→HTTPS redirect automatico (CDN HuggingFace) */
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setRawHeader("User-Agent", "PrismaluxMobile/1.0 Qt/" QT_VERSION_STR);
    if (m_dlResumingFrom > 0)
        req.setRawHeader("Range",
            "bytes=" + QByteArray::number(m_dlResumingFrom) + "-");
    m_dlReply = m_nam->get(req);
    connect(m_dlReply, &QNetworkReply::readyRead,
            this, &LocalLlmClient::onDownloadReadyRead);
    connect(m_dlReply, &QNetworkReply::downloadProgress,
            this, &LocalLlmClient::onDownloadProgress);
    connect(m_dlReply, &QNetworkReply::finished,
            this, &LocalLlmClient::onDownloadDone);
#ifndef QT_NO_SSL
    connect(m_dlReply, &QNetworkReply::sslErrors,
            this, &LocalLlmClient::onSslErrors);
#endif
}

void LocalLlmClient::cancelDownload()
{
    if (m_dlReply) { m_dlReply->abort(); }
}

void LocalLlmClient::onDownloadReadyRead()
{
    if (m_dlFile && m_dlReply)
        m_dlFile->write(m_dlReply->readAll());
}

void LocalLlmClient::onDownloadProgress(qint64 received, qint64 total)
{
    /* Aggiunge l'offset del file parziale per mostrare il progresso reale */
    const qint64 adjReceived = received + m_dlResumingFrom;
    const qint64 adjTotal    = total > 0 ? total + m_dlResumingFrom : -1;
    emit downloadProgress(adjReceived, adjTotal);
}

void LocalLlmClient::onDownloadDone()
{
    if (!m_dlReply) return;
    const bool ok        = (m_dlReply->error() == QNetworkReply::NoError);
    const bool cancelled = (m_dlReply->error() == QNetworkReply::OperationCanceledError);
    const QString errStr = m_dlReply->errorString();
    const int httpCode   = m_dlReply->attribute(
        QNetworkRequest::HttpStatusCodeAttribute).toInt();
    m_dlReply->deleteLater(); m_dlReply = nullptr;

    if (!m_dlFile) return;
    m_dlFile->flush();
    m_dlFile->close();
    const QString tmpPath  = m_dlFile->fileName();
    const QString destPath = tmpPath.chopped(4);  /* rimuove ".tmp" */
    m_dlFile->deleteLater(); m_dlFile = nullptr;

    if (!ok) {
        if (cancelled) return;  /* mantieni .tmp per riprendere */
        const bool isHttpError = (httpCode >= 400 && httpCode < 600);
        if (isHttpError) {
            QFile::remove(tmpPath);  /* errore HTTP: file parziale inutile */
            emit downloadError(QString("HTTP %1 — %2").arg(httpCode).arg(errStr));
        } else {
            /* Errore di rete: mantieni .tmp, l'utente può riprendere */
            emit downloadError("Errore di rete: " + errStr +
                               "\nPuoi riprendere il download.");
        }
        return;
    }

    /* Il server ha ignorato Range e risposto 200: il .tmp è corrotto */
    if (m_dlResumingFrom > 0 && httpCode == 200) {
        QFile::remove(tmpPath);
        emit downloadError(
            "Il server non supporta la ripresa. "
            "Premi di nuovo per ricominciare dall\xe2\x80\x99inizio.");
        return;
    }

    QFile::remove(destPath);
    QFile::rename(tmpPath, destPath);
    m_dlResumingFrom = 0;
    emit downloadFinished(destPath);
}

void LocalLlmClient::onSslErrors(const QList<QSslError>& errors)
{
    /* Su Android Qt usa OpenSSL proprio (ignora network_security_config).
       I warning SSL dal CDN HuggingFace vanno ignorati per consentire il download;
       errori gravi (hostname mismatch, cert scaduto) appaiono nei log. */
    QStringList fatals;
    for (const QSslError& e : errors) {
        if (e.error() != QSslError::NoError)
            fatals << e.errorString();
    }
    if (m_dlReply)
        m_dlReply->ignoreSslErrors();
    if (!fatals.isEmpty())
        qWarning() << "[LocalLlmClient] SSL warning:" << fatals.join("; ");
}

/* ── MOC include per classi definite nel .cpp ─────────────────── */
#include "local_llm_client.moc"
