#include "ai_client.h"
#include "prismalux_paths.h"
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QSettings>

/* ══════════════════════════════════════════════════════════════
   AiChatParams — unica fonte ~/.prismalux/ai_params.json
   ══════════════════════════════════════════════════════════════ */
QString AiChatParams::filePath()
{
    return QDir::homePath() + "/.prismalux/ai_params.json";
}

AiChatParams AiChatParams::load()
{
    AiChatParams p;   /* struct con i default Brutal Honesty come valori iniziali */

    QFile f(filePath());
    if (!f.exists()) {
        /* Prima esecuzione: crea il file con i default */
        save(p);
        return p;
    }
    if (!f.open(QIODevice::ReadOnly)) return p;

    const QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
    if (obj.contains("temperature"))    p.temperature    = obj["temperature"].toDouble(p.temperature);
    if (obj.contains("top_p"))          p.top_p          = obj["top_p"].toDouble(p.top_p);
    if (obj.contains("top_k"))          p.top_k          = obj["top_k"].toInt(p.top_k);
    if (obj.contains("repeat_penalty")) p.repeat_penalty = obj["repeat_penalty"].toDouble(p.repeat_penalty);
    if (obj.contains("num_predict"))    p.num_predict    = obj["num_predict"].toInt(p.num_predict);
    if (obj.contains("num_ctx"))        p.num_ctx        = obj["num_ctx"].toInt(p.num_ctx);
    if (obj.contains("honesty_prefix")) p.honesty_prefix = obj["honesty_prefix"].toBool(p.honesty_prefix);
    if (obj.contains("caveman_mode"))   p.caveman_mode   = obj["caveman_mode"].toBool(p.caveman_mode);
    return p;
}

void AiChatParams::save(const AiChatParams& p)
{
    /* Crea la cartella se non esiste */
    QDir().mkpath(QDir::homePath() + "/.prismalux");

    QJsonObject obj;
    obj["temperature"]    = p.temperature;
    obj["top_p"]          = p.top_p;
    obj["top_k"]          = p.top_k;
    obj["repeat_penalty"] = p.repeat_penalty;
    obj["num_predict"]    = p.num_predict;
    obj["num_ctx"]        = p.num_ctx;
    obj["honesty_prefix"] = p.honesty_prefix;
    obj["caveman_mode"]   = p.caveman_mode;

    QFile f(filePath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
}

/* ══════════════════════════════════════════════════════════════
   AiClient — costruttore
   ══════════════════════════════════════════════════════════════ */
AiClient::AiClient(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    /* Carica i parametri dall'unica fonte: ~/.prismalux/ai_params.json */
    m_params = AiChatParams::load();
}

/* ══════════════════════════════════════════════════════════════
   classifyQuery — euristica veloce per decidere think e num_predict
   Criteri:
   • ≤ 30 caratteri e nessuna keyword complessa  → Simple
   • > 200 caratteri                              → Complex
   • Keyword di analisi/spiegazione/codice        → Complex
   • Tutto il resto (30-200 chars, neutre)        → Auto  ← FIX T1 (era Simple)
   ══════════════════════════════════════════════════════════════ */
AiClient::QueryType AiClient::classifyQuery(const QString& text)
{
    const int len = text.length();
    if (len > 200) return QueryComplex;

    static const QString kComplexKW[] = {
        "spiega", "analizza", "confronta", "descri", "illustra",
        "approfondisci", "come funziona", "differenza", "pro e contro",
        "vantaggi", "svantaggi", "implementa", "algoritmo", "codice",
        "perch\xc3\xa9", "ragiona", "dimostra", "riassumi", "elabora",
        "architettura", "compara", "elenca tutto", "dimmi tutto"
    };
    const QString lower = text.toLower();
    for (const auto& kw : kComplexKW) {
        if (lower.contains(kw)) return QueryComplex;
    }

    return (len <= 30) ? QuerySimple : QueryAuto;  /* FIX T1: era QuerySimple:QuerySimple */
}

/* ── Anti-data-leak: verifica che l'host sia localhost ───────────────────
   Prismalux comunica solo con AI locali (Ollama / llama-server su loopback).
   Se il chiamante passa un host esterno (es. per errore di configurazione)
   viene silenziosamente rimpiazzato con 127.0.0.1 e un warning loggato. */
static QString _enforce_local_host(const QString& host) {
    if (host == "127.0.0.1" || host == "::1" ||
        host.compare("localhost", Qt::CaseInsensitive) == 0)
        return host;
    qWarning("[SECURITY] AiClient: host '%s' non è localhost — "
             "sostituito con 127.0.0.1 (data-leak prevention).",
             host.toUtf8().constData());
    return QStringLiteral("127.0.0.1");
}

/* ── Configurazione ───────────────────────────────────────────── */
void AiClient::setBackend(Backend b, const QString& host, int port, const QString& model) {
    /* Invalida cache se backend o porta cambiano (host diverso ≠ stesso server) */
    if (b != m_backend || port != m_port)
        m_cacheValid = false;
    m_backend = b;
    m_host    = _enforce_local_host(host);
    m_port    = port;
    if (m_model != model) {
        m_model = model;
        if (!model.isEmpty())
            emit modelChanged(model);   /* notifica UI (MainWindow label, Impostazioni, ecc.) */
    } else {
        m_model = model;
    }
}

void AiClient::setLocalBackend(const QString& llamaBin, const QString& modelPath) {
    m_backend    = LlamaLocal;
    m_llamaBin   = llamaBin;
    m_localModel = modelPath;
}

/* ── Abort ────────────────────────────────────────────────────── */
void AiClient::abort() {
    m_isGenerateMode = false;
    bool wasActive = m_reply != nullptr || m_localBusy;
    if (m_reply) {
        /* Azzera m_reply PRIMA di chiamare abort() su Windows:
         * abort() può emettere finished() in modo sincrono,
         * onFinished() chiamerebbe deleteLater() di nuovo sullo stesso puntatore. */
        QNetworkReply* r = m_reply;
        m_reply = nullptr;
        r->abort();
        r->deleteLater();
    }
    if (m_localProc) { m_localProc->kill(); }
    m_busy_guard     = false;
    m_localBusy      = false;
    m_accum.clear();
    m_thinkingAccum.clear();
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
        /* Scansiona la directory modelli per trovare tutti i .gguf disponibili */
        QStringList gguf = PrismaluxPaths::scanGgufFiles();
        /* Se la scansione è vuota ma abbiamo un modello già impostato, usalo come fallback */
        if (gguf.isEmpty() && !m_localModel.isEmpty())
            gguf << m_localModel;
        emit modelsReady(gguf);
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
    if (r->error() != QNetworkReply::NoError) {
        m_cacheValid = false;   /* invalida cache: backend non raggiungibile */
        emit modelsReady({});   /* aggiorna la UI (mostra "backend non raggiungibile") */
        return;                 /* NON emettere error(): fetchModels è operazione background */
    }

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

/* ── Prefisso di onestà assoluta (brutal honesty) ────────────────
   Viene preposto a TUTTI i system prompt quando honesty_prefix=true.
   Il modello riceve l'istruzione esplicita di ammettere incertezza
   invece di inventare fatti, numeri o citazioni.
   ──────────────────────────────────────────────────────────────── */
static const char* kHonestyPrefix =
    "REGOLA ASSOLUTA (non derogabile): rispondi SOLO con fatti verificati e certi. "
    "Se non conosci qualcosa con certezza, dì esplicitamente "
    "\"Non lo so\" o \"Non ne sono certo\". "
    "Non inventare MAI numeri, date, nomi, citazioni, studi o fonti. "
    "Se un dato manca, dì che manca. Preferisci una risposta incompleta ma vera "
    "a una risposta completa ma inventata. "
    "Non speculare. Non completare lacune con supposizioni. "
    "Rispondi SEMPRE e SOLO in italiano.\n\n";

/* ── Prefisso modalità Caveman: risposte dirette senza convenevoli ───────
   Viene preposto a tutti i system prompt quando caveman_mode=true.
   Riduce i token sprecati in introduzioni e riepiloghi inutili.
   ──────────────────────────────────────────────────────────────────────── */
static const char* kCavemanPrefix =
    "STILE RISPOSTA: sii diretto e conciso. "
    "Nessuna frase introduttiva ('Certamente!', 'Ottima domanda!', 'Certo, ecco', ecc.). "
    "Nessun riepilogo finale. Nessun 'Spero di averti aiutato'. "
    "Vai subito al contenuto richiesto, senza riempitivi. "
    "Preferisci elenchi puntati a paragrafi verbosi quando il contenuto lo permette.\n\n";

/* ── chat (wrapper legacy — compatibile con tutto il codice esistente) ─── */
void AiClient::chat(const QString& systemPrompt, const QString& userMsg) {
    chat(systemPrompt, userMsg, QJsonArray(), classifyQuery(userMsg));  /* FIX T2: era QueryAuto hardcoded */
}

/* ── chat (implementazione completa con storia e tipo query) ─────────── */
void AiClient::chat(const QString& systemPrompt, const QString& userMsg,
                    const QJsonArray& history, QueryType qt)
{
    if (busy()) return;

    /* Throttle: ignora richieste ravvicinate (<400ms) */
    if (m_throttleTimer.isValid() && m_throttleTimer.elapsed() < 400) return;
    m_throttleTimer.restart();

    /* RAM critica */
    if (m_ramFreePct < 10.0) {
        emit error(
            "\xe2\x9a\xa0  RAM critica (" +
            QString::number(100.0 - m_ramFreePct, 'f', 0) +
            "% usata). Chiudi altre applicazioni prima di continuare.");
        return;
    }

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

        QString effectiveSysLocal = systemPrompt;
        if (m_params.caveman_mode)
            effectiveSysLocal = QString(kCavemanPrefix) + effectiveSysLocal;
        if (m_params.honesty_prefix)
            effectiveSysLocal = QString(kHonestyPrefix) + effectiveSysLocal;

        /* Costruisce il prompt ChatML — inserisce la storia come turni precedenti */
        QString prompt;
        if (!effectiveSysLocal.isEmpty())
            prompt += "<|im_start|>system\n" + effectiveSysLocal + "\n<|im_end|>\n";
        /* Storia */
        for (const QJsonValue& v : history) {
            const QJsonObject m = v.toObject();
            const QString role = m["role"].toString();
            const QString cont = m["content"].toString();
            prompt += "<|im_start|>" + role + "\n" + cont + "\n<|im_end|>\n";
        }
        prompt += "<|im_start|>user\n" + userMsg + "\n<|im_end|>\n<|im_start|>assistant\n";

        const int numPredict = (qt == QuerySimple) ? 512 : m_params.num_predict;

        QStringList args = {
            "-m", m_localModel,
            "-p", prompt,
            "-n",              QString::number(numPredict),
            "--ctx-size",      QString::number(m_params.num_ctx),
            "--temp",          QString::number(m_params.temperature, 'f', 3),
            "--top-p",         QString::number(m_params.top_p,       'f', 3),
            "--top-k",         QString::number(m_params.top_k),
            "--repeat-penalty",QString::number(m_params.repeat_penalty, 'f', 3),
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
    abort();
    m_busy_guard     = true;
    m_isGenerateMode = false;   /* /api/chat, non /api/generate */
    m_accum.clear();

    QString effectiveSys = systemPrompt;
    if (m_params.caveman_mode)
        effectiveSys = QString(kCavemanPrefix) + effectiveSys;
    if (m_params.honesty_prefix)
        effectiveSys = QString(kHonestyPrefix) + effectiveSys;

    /* ── Costruzione array messaggi con storia ── */
    QJsonArray messages;
    if (!effectiveSys.isEmpty()) {
        QJsonObject sys; sys["role"] = "system"; sys["content"] = effectiveSys;
        messages.append(sys);
    }
    /* Inserisci i turni della storia (precedenti) prima del messaggio corrente */
    for (const QJsonValue& v : history)
        messages.append(v);

    QJsonObject usr; usr["role"] = "user"; usr["content"] = userMsg;
    messages.append(usr);

    QJsonObject body;
    body["model"]    = m_model;
    body["stream"]   = true;
    body["messages"] = messages;

    /* ── Parametri anti-allucinazione + think/num_predict dinamici ── */
    if (m_backend == Ollama) {
        QJsonObject opts;
        opts["temperature"]    = m_params.temperature;
        opts["top_p"]          = m_params.top_p;
        opts["top_k"]          = m_params.top_k;
        opts["repeat_penalty"] = m_params.repeat_penalty;
        opts["num_ctx"]        = m_params.num_ctx;

        /* FIX think budget: qwen3/deepseek-r1 consumano token nel blocco <think>
           → raddoppiare num_predict quando think è attivo per non troncare la risposta.
           FIX think:false su piccoli modelli: forzare think:false su qwen3/deepseek
           con QuerySimple produce m_accum vuoto (modello 0.8B-1.5B non genera nulla
           senza ragionamento). Per i modelli think-capable usiamo solo il cap num_predict,
           lasciando che Ollama gestisca il think naturalmente. */
        const bool thinkCapable = m_model.startsWith("qwen3")       ||
                                  m_model.startsWith("qwen3.5")     ||
                                  m_model.startsWith("deepseek-r1") ||
                                  m_model.startsWith("qwen2.5");
        if (qt == QuerySimple) {
            opts["num_predict"] = 512;
            if (!thinkCapable) opts["think"] = false;   /* solo modelli non-think */
        } else if (qt == QueryComplex) {
            /* Se il modello usa il blocco <think>, raddoppia il budget */
            opts["num_predict"] = thinkCapable ? m_params.num_predict * 2
                                               : m_params.num_predict;
            if (thinkCapable) opts["think"] = true;
        } else {
            /* QueryAuto: comportamento predefinito, nessun think esplicito */
            opts["num_predict"] = m_params.num_predict;
        }
        body["options"] = opts;
    } else {
        /* llama-server usa il formato OpenAI */
        body["temperature"]    = m_params.temperature;
        body["top_p"]          = m_params.top_p;
        body["repeat_penalty"] = m_params.repeat_penalty;
        body["max_tokens"]     = (qt == QuerySimple) ? 512 : m_params.num_predict;
    }

    /* keep_alive dinamico (solo Ollama) */
    if (m_backend == Ollama) {
        const double usedPct = 100.0 - m_ramFreePct;
        if (usedPct > 70.0)
            body["keep_alive"] = 0;
        else if (usedPct > 40.0)
            body["keep_alive"] = 30;
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

/* ── generate — /api/generate per query RAG single-shot ──────────────── */
/*
 * Usa /api/generate (Ollama) invece di /api/chat quando non c'è storia attiva.
 * Vantaggio: endpoint più leggero, campo "system" dedicato, no messages array.
 * Per backend non-Ollama fa fallback automatico a chat() single-turn.
 *
 * Formato streaming Ollama /api/generate:
 *   {"model":"...","response":"token","done":false}
 *   {"model":"...","response":"","done":true,"total_duration":...}  ← fine
 * onReadyRead() legge "response" invece di "message.content"
 * quando m_isGenerateMode == true.
 */
void AiClient::generate(const QString& systemPrompt, const QString& prompt, QueryType qt)
{
    /* Fallback per backend non-Ollama: usa chat() single-turn */
    if (m_backend != Ollama) {
        chat(systemPrompt, prompt, QJsonArray(), qt);
        return;
    }

    if (busy()) return;
    if (m_throttleTimer.isValid() && m_throttleTimer.elapsed() < 400) return;
    m_throttleTimer.restart();

    if (m_ramFreePct < 10.0) {
        emit error(
            "\xe2\x9a\xa0  RAM critica (" +
            QString::number(100.0 - m_ramFreePct, 'f', 0) +
            "% usata). Chiudi altre applicazioni prima di continuare.");
        return;
    }

    emit requestStarted(m_model, "Ollama");

    abort();
    m_busy_guard     = true;
    m_isGenerateMode = true;    /* onReadyRead() leggerà "response" */
    m_accum.clear();

    QString effectiveSys = systemPrompt;
    if (m_params.caveman_mode)
        effectiveSys = QString(kCavemanPrefix) + effectiveSys;
    if (m_params.honesty_prefix)
        effectiveSys = QString(kHonestyPrefix) + effectiveSys;

    QJsonObject body;
    body["model"]  = m_model;
    body["stream"] = true;
    body["prompt"] = prompt;
    if (!effectiveSys.isEmpty())
        body["system"] = effectiveSys;

    QJsonObject opts;
    opts["temperature"]    = m_params.temperature;
    opts["top_p"]          = m_params.top_p;
    opts["top_k"]          = m_params.top_k;
    opts["repeat_penalty"] = m_params.repeat_penalty;
    opts["num_ctx"]        = m_params.num_ctx;

    /* generate() — stessa logica di chat(): think:false solo per modelli non-think-capable */
    const bool thinkCapableGen = m_model.startsWith("qwen3")       ||
                                 m_model.startsWith("qwen3.5")     ||
                                 m_model.startsWith("deepseek-r1") ||
                                 m_model.startsWith("qwen2.5");
    if (qt == QuerySimple) {
        opts["num_predict"] = 512;
        if (!thinkCapableGen) opts["think"] = false;
    } else if (qt == QueryComplex) {
        opts["num_predict"] = m_params.num_predict;
        opts["think"]       = true;
    } else {
        opts["num_predict"] = m_params.num_predict;
    }
    body["options"] = opts;

    /* keep_alive dinamico */
    const double usedPct = 100.0 - m_ramFreePct;
    if (usedPct > 70.0)      body["keep_alive"] = 0;
    else if (usedPct > 40.0) body["keep_alive"] = 30;

    QString url = QString("http://%1:%2/api/generate").arg(m_host).arg(m_port);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    m_reply = m_nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(m_reply, &QNetworkReply::readyRead, this, &AiClient::onReadyRead);
    connect(m_reply, &QNetworkReply::finished,  this, &AiClient::onFinished);
}

/* ── chatWithImage ────────────────────────────────────────────── */
void AiClient::chatWithImage(const QString& systemPrompt, const QString& userMsg,
                              const QByteArray& imageBase64, const QString& mimeType)
{
    if (busy()) return;
    if (m_throttleTimer.isValid() && m_throttleTimer.elapsed() < 400) return;
    m_throttleTimer.restart();

    if (m_ramFreePct < 10.0) {
        emit error("\xe2\x9a\xa0  RAM critica. Chiudi altre applicazioni prima di continuare.");
        return;
    }

    static const char* BKNAMES[] = {"Ollama", "llama-server", "llama-local"};
    emit requestStarted(m_model, BKNAMES[m_backend]);

    m_busy_guard = true;
    m_accum.clear();

    QJsonObject body;
    body["model"]  = m_model;
    body["stream"] = true;

    /* Prependi il prefisso di onestà anche alle chiamate con immagine */
    QString effectiveSysImg = systemPrompt;
    if (m_params.honesty_prefix) {
        effectiveSysImg = QString(kHonestyPrefix) + effectiveSysImg;
    }

    QJsonArray messages;
    if (!effectiveSysImg.isEmpty()) {
        QJsonObject sys; sys["role"] = "system"; sys["content"] = effectiveSysImg;
        messages.append(sys);
    }

    /* Messaggio utente con immagine */
    QJsonObject usr;
    usr["role"] = "user";
    if (m_backend == Ollama) {
        /* Ollama: campo "images" = array di stringhe base64 */
        usr["content"] = userMsg;
        QJsonArray imgs;
        imgs.append(QString::fromLatin1(imageBase64));
        usr["images"] = imgs;
    } else {
        /* llama-server / OpenAI: content = array di oggetti {type, text/image_url} */
        QJsonArray content;
        QJsonObject textPart;
        textPart["type"] = "text";
        textPart["text"] = userMsg;
        content.append(textPart);
        QJsonObject imgPart;
        imgPart["type"] = "image_url";
        QJsonObject imgUrl;
        imgUrl["url"] = QString("data:%1;base64,%2")
                        .arg(mimeType)
                        .arg(QString::fromLatin1(imageBase64));
        imgPart["image_url"] = imgUrl;
        content.append(imgPart);
        usr["content"] = content;
    }
    messages.append(usr);
    body["messages"] = messages;

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

        /* ── Rilevamento errori JSON di Ollama ─────────────────────────
           Ollama restituisce {"error":"..."} come corpo JSON flat (non NDJSON)
           quando il modello non supporta immagini o la richiesta e' malformata.
           Esempi:
             {"error":"model \"deepseek-r1\" does not support vision"}
             {"error":"unexpected EOF"}
           ─────────────────────────────────────────────────────────────── */
        if (line.startsWith('{') && line.contains("\"error\"")) {
            const auto doc  = QJsonDocument::fromJson(line);
            const QString msg = doc.object().value("error").toString();
            if (!msg.isEmpty()) {
                /* Messaggio leggibile con suggerimento se il modello non e' vision */
                QString hint = msg;
                if (msg.contains("vision", Qt::CaseInsensitive) ||
                    msg.contains("does not support", Qt::CaseInsensitive) ||
                    msg.contains("multimodal", Qt::CaseInsensitive)) {
                    hint = "\xe2\x9d\x8c  " + msg +
                           "\n\n\xf0\x9f\x92\xa1  Il modello selezionato non supporta le immagini.\n"
                           "Usa un modello vision: llama3.2-vision, qwen2-vl:7b, minicpm-v:8b, llava:7b\n"
                           "\xe2\x9a\xa0  I modelli DeepSeek (r1, coder, janus) non supportano vision su Ollama.";
                }
                emit error(hint);
                return;
            }
        }
        QJsonDocument doc = QJsonDocument::fromJson(line);
        if (doc.isNull()) continue;
        QJsonObject obj = doc.object();
        QString chunk;
        if (m_isGenerateMode && m_backend == Ollama) {
            /* Formato /api/generate: {"response":"token","done":false} */
            chunk = obj["response"].toString();
        } else if (m_backend == Ollama) {
            chunk = obj["message"].toObject()["content"].toString();
        } else {
            auto ch = obj["choices"].toArray();
            if (!ch.isEmpty()) chunk = ch[0].toObject()["delta"].toObject()["content"].toString();
        }
        if (!chunk.isEmpty()) { m_accum += chunk; emit token(chunk); }

        /* Fallback per modelli come qwen3.5 che usano message.thinking invece di
         * includere <think>...</think> in message.content.
         * Accumuliamo il thinking silenziosamente: se a fine stream m_accum è vuoto,
         * onFinished() lo wrapperà in <think>...</think> come risposta di fallback. */
        if (chunk.isEmpty() && !m_isGenerateMode && m_backend == Ollama) {
            const QString thinking = obj["message"].toObject()["thinking"].toString();
            if (!thinking.isEmpty()) m_thinkingAccum += thinking;
        }
    }
}

void AiClient::onFinished() {
    m_busy_guard     = false;
    m_isGenerateMode = false;
    if (!m_reply) return;
    QNetworkReply* r = m_reply;
    m_reply = nullptr;

    /* ── Errore di rete (connessione rifiutata, timeout, ecc.) ── */
    if (r->error() != QNetworkReply::NoError &&
        r->error() != QNetworkReply::OperationCanceledError) {
        emit error(r->errorString());
        r->deleteLater();
        emit finished(m_accum);
        return;
    }

    /* ── HTTP 4xx / 5xx da Ollama / llama-server ─────────────────
       Quando il modello non supporta vision o la richiesta e' invalida,
       Ollama risponde con HTTP 400/422/500 e corpo JSON {"error":"..."}.
       QNetworkReply::error() e' NoError perche' la connessione TCP e'
       avvenuta, ma lo status HTTP indica un fallimento applicativo.
       ─────────────────────────────────────────────────────────── */
    const int httpStatus = r->attribute(
        QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (httpStatus >= 400 && m_accum.isEmpty()) {
        const QByteArray body = r->readAll();
        const auto doc = QJsonDocument::fromJson(body.isEmpty() ? "{}" : body);
        QString msg = doc.object().value("error").toString();
        if (msg.isEmpty())
            msg = QString("Errore HTTP %1 dal backend").arg(httpStatus);

        /* Arricchisce il messaggio per errori vision noti */
        if (msg.contains("vision", Qt::CaseInsensitive) ||
            msg.contains("does not support", Qt::CaseInsensitive)) {
            msg += "\n\n\xf0\x9f\x92\xa1  Usa un modello vision compatibile:\n"
                   "llama3.2-vision \xe2\x80\x94 qwen2-vl:7b \xe2\x80\x94 minicpm-v:8b \xe2\x80\x94 llava:7b\n"
                   "\xe2\x9a\xa0  I modelli DeepSeek (r1, coder, janus) non supportano immagini su Ollama.";
        }
        emit error(msg);
        r->deleteLater();
        emit finished(m_accum);
        return;
    }

    m_modelLoaded = true;
    r->deleteLater();

    /* Fallback thinking: se il modello ha usato solo message.thinking e
     * message.content è rimasto vuoto, usiamo il thinking wrappato in
     * <think>...</think> così agenti_page_stream lo gestisce con il regex reTh.
     *
     * Eccezione: modelli con bug noto (thinking loop infinito, content sempre
     * vuoto anche con num_predict alto) — emettiamo error() con suggerimento. */
    if (m_accum.isEmpty() && !m_thinkingAccum.isEmpty()) {
        static const QStringList s_knownBroken = { "qwen3.5:0.8b" };
        if (s_knownBroken.contains(m_model, Qt::CaseInsensitive)) {
            m_thinkingAccum.clear();
            emit error(
                "\xe2\x9a\xa0  " + m_model + " ha un bug noto su Ollama: "
                "genera solo pensiero interno (thinking loop) senza risposta finale, "
                "anche con num_predict alto.\n"
                "\xf0\x9f\x92\xa1  Sostituisci con: llama3.2:3b (~50 tok/s) "
                "oppure deepseek-r1:1.5b (~41 tok/s).");
            return;  /* non emettere finished() dopo error() — evita loop Byzantino */
        } else {
            m_accum = "<think>" + m_thinkingAccum + "</think>";
        }
    }
    m_thinkingAccum.clear();

    emit finished(m_accum);
}

/* ══════════════════════════════════════════════════════════════
   fetchEmbedding — richiede l'embedding vettoriale di @p text.
   Ollama:      POST /api/embeddings  { "model": ..., "prompt": ... }
                risposta: { "embedding": [f1, f2, ...] }
   llama-server: POST /v1/embeddings  { "model": ..., "input": ... }
                risposta: { "data": [{ "embedding": [...] }] }
   Non blocca il thread UI; emette embeddingReady o embeddingError.
   ══════════════════════════════════════════════════════════════ */
void AiClient::fetchEmbedding(const QString& text) {
    if (m_backend == LlamaLocal) {
        emit embeddingError("Embedding non disponibile con llama.cpp locale (serve Ollama o llama-server)");
        return;
    }

    QString urlStr;
    QJsonObject body;
    if (m_backend == Ollama) {
        urlStr = QString("http://%1:%2/api/embeddings").arg(m_host).arg(m_port);
        /* Usa il modello embedding dedicato se configurato (rag/embedModel),
         * altrimenti cade sul modello chat corrente — ma i modelli chat di solito
         * NON supportano /api/embeddings: installare nomic-embed-text. */
        QSettings ragS("Prismalux", "GUI");
        const QString embedModel = ragS.value("rag/embedModel", "").toString();
        body["model"]  = embedModel.isEmpty() ? m_model : embedModel;
        body["prompt"] = text;
    } else {
        urlStr = QString("http://%1:%2/v1/embeddings").arg(m_host).arg(m_port);
        body["model"] = m_model;
        body["input"] = text;
    }

    QUrl embedUrl(urlStr);
    QNetworkRequest req(embedUrl);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QNetworkReply* reply = m_nam->post(req,
        QJsonDocument(body).toJson(QJsonDocument::Compact));

    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit embeddingError(reply->errorString());
            return;
        }
        const QJsonObject obj =
            QJsonDocument::fromJson(reply->readAll()).object();

        /* Estrai il vettore — formato Ollama o OpenAI */
        QJsonArray arr;
        if (obj.contains("embedding")) {
            arr = obj["embedding"].toArray();
        } else if (obj.contains("data")) {
            const QJsonArray data = obj["data"].toArray();
            if (!data.isEmpty())
                arr = data.first().toObject()["embedding"].toArray();
        }

        if (arr.isEmpty()) {
            emit embeddingError("Nessun embedding nella risposta del backend");
            return;
        }

        QVector<float> vec;
        vec.reserve(arr.size());
        for (const QJsonValue& v : arr)
            vec.append((float)v.toDouble());
        emit embeddingReady(vec);
    });
}
