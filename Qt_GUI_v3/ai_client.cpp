#include "ai_client.h"
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>

AiClient::AiClient(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{}

/* ── Configurazione ───────────────────────────────────────────── */
void AiClient::setBackend(Backend b, const QString& host, int port, const QString& model) {
    /* Invalida cache se backend o porta cambiano (host diverso ≠ stesso server) */
    if (b != m_backend || port != m_port)
        m_cacheValid = false;
    m_backend = b; m_host = host; m_port = port; m_model = model;
}

void AiClient::setLocalBackend(const QString& llamaBin, const QString& modelPath) {
    m_backend    = LlamaLocal;
    m_llamaBin   = llamaBin;
    m_localModel = modelPath;
}

/* ── Abort ────────────────────────────────────────────────────── */
void AiClient::abort() {
    bool wasActive = m_reply != nullptr || m_localBusy;
    if (m_reply)     { m_reply->abort(); m_reply = nullptr; }
    if (m_localProc) { m_localProc->kill(); }
    m_busy_guard = false;
    m_localBusy  = false;
    m_accum.clear();
    if (wasActive) emit aborted();
}

/* ── fetchModels ─────────────────────────────────────────────── */
/*
 * Cache TTL 30s: se backend e porta non sono cambiati e l'ultimo
 * fetch è avvenuto meno di 30s fa, emette subito la lista già nota
 * senza fare un nuovo round-trip HTTP. Questo evita fetch ridondanti
 * quando più pagine (AgentiPage, Manutenzione…) chiamano fetchModels()
 * allo stesso backend entro pochi secondi.
 *
 * La cache viene invalidata (m_cacheValid = false) quando:
 *   - il backend cambia (setBackend con backend o porta diversi)
 *   - onModelsReply riceve una risposta con errore
 */
void AiClient::fetchModels() {
    if (m_backend == LlamaLocal) {
        emit modelsReady({m_localModel});
        return;
    }

    /* Controlla cache: stesso backend + stessa porta + entro TTL */
    if (m_cacheValid
        && m_backend == m_cachedBackend
        && m_port    == m_cachedPort
        && m_cacheTimer.isValid()
        && m_cacheTimer.elapsed() < kCacheTtlMs)
    {
        emit modelsReady(m_models);
        return;
    }

    /* Cache scaduta o non valida: fetch HTTP */
    QString url;
    if (m_backend == Ollama)
        url = QString("http://%1:%2/api/tags").arg(m_host).arg(m_port);
    else
        url = QString("http://%1:%2/v1/models").arg(m_host).arg(m_port);

    QNetworkReply* r = m_nam->get(QNetworkRequest(QUrl(url)));
    connect(r, &QNetworkReply::finished, this, &AiClient::onModelsReply);
}

void AiClient::onModelsReply() {
    auto* r = qobject_cast<QNetworkReply*>(sender());
    if (!r) return;
    r->deleteLater();
    if (r->error() != QNetworkReply::NoError) { emit error(r->errorString()); return; }

    QJsonDocument doc = QJsonDocument::fromJson(r->readAll());
    QJsonObject   obj = doc.object();
    QStringList   list;

    if (m_backend == Ollama) {
        for (auto v : obj["models"].toArray())
            list << v.toObject()["name"].toString();
    } else {
        for (auto v : obj["data"].toArray())
            list << v.toObject()["id"].toString();
    }
    m_models = list;
    if (!list.isEmpty() && m_model.isEmpty()) m_model = list.first();

    /* Aggiorna cache solo se il fetch è andato a buon fine (lista non vuota) */
    if (!list.isEmpty()) {
        m_cachedBackend = m_backend;
        m_cachedPort    = m_port;
        m_cacheValid    = true;
        m_cacheTimer.restart();
    }
    emit modelsReady(list);
}

/* ── unloadModel ─────────────────────────────────────────────── */
/*
 * Invia keep_alive=0 a Ollama per forzare lo scarico del modello dalla RAM.
 * Usa /api/generate con solo "model" e "keep_alive": Ollama interpreta questo
 * come "smetti di tenere il modello caricato".
 * Fire-and-forget: la reply viene eliminata subito senza leggere la risposta.
 */
void AiClient::unloadModel() {
    /* Se il modello non risulta caricato, Ollama lo caricherebb solo per
     * scaricarlo subito — causando oscillazioni RAM. Non fare nulla. */
    if (!m_modelLoaded) return;
    if (m_backend != Ollama || m_model.isEmpty()) return;
    m_modelLoaded = false;

    QJsonObject body;
    body["model"]      = m_model;
    body["keep_alive"] = 0;

    QString url = QString("http://%1:%2/api/generate").arg(m_host).arg(m_port);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QNetworkReply* r = m_nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(r, &QNetworkReply::finished, r, &QNetworkReply::deleteLater);
}

/* ── chat ────────────────────────────────────────────────────── */
void AiClient::chat(const QString& systemPrompt, const QString& userMsg) {
    if (busy()) return;

    /* Throttle: ignora richieste ravvicinate (<400ms) — evita doppio-clic
     * che saturano la RAM avviando due inference parallele */
    if (m_throttleTimer.isValid() && m_throttleTimer.elapsed() < 400) return;
    m_throttleTimer.restart();

    /* RAM critica: blocca la richiesta se la RAM libera è sotto il 10%
     * per evitare che il sistema si blocchi durante il caricamento del modello */
    if (m_ramFreePct < 10.0) {
        emit error(
            "\xe2\x9a\xa0  RAM critica (" +
            QString::number(100.0 - m_ramFreePct, 'f', 0) +
            "% usata). Chiudi altre applicazioni prima di continuare.");
        return;
    }

    /* Notifica MonitorPanel (e chiunque sia in ascolto) che sta partendo */
    {
        static const char* BKNAMES[] = {"Ollama", "llama-server", "llama-local"};
        emit requestStarted(m_backend == LlamaLocal ? m_localModel : m_model,
                            BKNAMES[m_backend]);
    }

    /* ── LlamaLocal: processo llama-cli ── */
    if (m_backend == LlamaLocal) {
        if (m_llamaBin.isEmpty() || m_localModel.isEmpty()) {
            emit error("Backend locale non configurato. Seleziona un modello .gguf.");
            return;
        }
        m_localBusy  = true;
        m_localAccum.clear();

        /* Costruisce il prompt ChatML */
        QString prompt;
        if (!systemPrompt.isEmpty())
            prompt += "<|im_start|>system\n" + systemPrompt + "\n<|im_end|>\n";
        prompt += "<|im_start|>user\n" + userMsg + "\n<|im_end|>\n<|im_start|>assistant\n";

        /* Argomenti llama-cli */
        QStringList args = {
            "-m", m_localModel,
            "-p", prompt,
            "-n", "1024",
            "--temp", "0.3",
            "--repeat-penalty", "1.2",
            "--no-display-prompt",
            "-no-cnv",
            "--log-disable"
        };

        if (m_localProc) { m_localProc->kill(); m_localProc->deleteLater(); }
        m_localProc = new QProcess(this);
        m_localProc->setProcessChannelMode(QProcess::SeparateChannels);

        connect(m_localProc, &QProcess::readyReadStandardOutput,
                this, &AiClient::onLocalReadyRead);
        connect(m_localProc,
                QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                this, &AiClient::onLocalFinished);

        m_localProc->start(m_llamaBin, args);
        if (!m_localProc->waitForStarted(4000)) {
            m_localBusy = false;
            emit error(QString("Impossibile avviare llama-cli: %1").arg(m_llamaBin));
            m_localProc->deleteLater();
            m_localProc = nullptr;
        }
        return;
    }

    /* ── HTTP backends ── */
    m_busy_guard = true;
    abort();
    m_accum.clear();

    QJsonObject body;
    body["model"]  = m_model;
    body["stream"] = true;
    QJsonArray messages;
    if (!systemPrompt.isEmpty()) {
        QJsonObject sys; sys["role"] = "system"; sys["content"] = systemPrompt;
        messages.append(sys);
    }
    QJsonObject usr; usr["role"] = "user"; usr["content"] = userMsg;
    messages.append(usr);
    body["messages"] = messages;

    /* keep_alive dinamico (solo Ollama): determina per quanto tempo il modello
     * rimane in memoria dopo la risposta, in base alla RAM libera attuale.
     *   RAM usata < 40% → default Ollama (-1 = ~5 min)
     *   RAM usata 40-70% → 30 secondi (poi Ollama scarica da solo)
     *   RAM usata > 70% → 0 (scarica immediatamente al termine)
     * Questo evita che modelli pesanti rimangano in RAM quando il sistema
     * è già sotto pressione. */
    if (m_backend == Ollama) {
        const double usedPct = 100.0 - m_ramFreePct;
        if (usedPct > 70.0)
            body["keep_alive"] = 0;   /* scarica subito */
        else if (usedPct > 40.0)
            body["keep_alive"] = 30;  /* tieni 30s poi scarica */
        /* else: nessun keep_alive → Ollama usa il suo default (~5 min) */
    }

    QString url = (m_backend == Ollama)
        ? QString("http://%1:%2/api/chat").arg(m_host).arg(m_port)
        : QString("http://%1:%2/v1/chat/completions").arg(m_host).arg(m_port);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    m_reply = m_nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(m_reply, &QNetworkReply::readyRead, this, &AiClient::onReadyRead);
    connect(m_reply, &QNetworkReply::finished,  this, &AiClient::onFinished);
}

void AiClient::onLocalReadyRead() {
    if (!m_localProc) return;
    QString chunk = QString::fromUtf8(m_localProc->readAllStandardOutput());
    if (!chunk.isEmpty()) {
        m_localAccum += chunk;
        emit token(chunk);
    }
}

void AiClient::onLocalFinished(int, QProcess::ExitStatus) {
    m_localBusy = false;
    if (m_localProc) { m_localProc->deleteLater(); m_localProc = nullptr; }
    emit finished(m_localAccum);
}

void AiClient::onReadyRead() {
    if (!m_reply) return;
    for (auto& raw : m_reply->readAll().split('\n')) {
        QByteArray line = raw.trimmed();
        if (line.isEmpty()) continue;
        if (line.startsWith("data: ")) { line = line.mid(6).trimmed(); if (line == "[DONE]") continue; }
        QJsonDocument doc = QJsonDocument::fromJson(line);
        if (doc.isNull()) continue;
        QJsonObject obj = doc.object();
        QString chunk;
        if (m_backend == Ollama)
            chunk = obj["message"].toObject()["content"].toString();
        else {
            auto ch = obj["choices"].toArray();
            if (!ch.isEmpty()) chunk = ch[0].toObject()["delta"].toObject()["content"].toString();
        }
        if (!chunk.isEmpty()) { m_accum += chunk; emit token(chunk); }
    }
}

void AiClient::onFinished() {
    m_busy_guard = false;
    if (!m_reply) return;
    if (m_reply->error() != QNetworkReply::NoError &&
        m_reply->error() != QNetworkReply::OperationCanceledError)
        emit error(m_reply->errorString());
    else
        m_modelLoaded = true;  /* inferenza HTTP completata → modello in RAM */
    m_reply->deleteLater(); m_reply = nullptr;
    emit finished(m_accum);
}
