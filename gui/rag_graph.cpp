#include "rag_graph.h"
#include "prismalux_paths.h"
#include "widgets/proc_helper.h"
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QDebug>

namespace P = PrismaluxPaths;

/* ══════════════════════════════════════════════════════════════
   Costruttore
   ══════════════════════════════════════════════════════════════ */
RagGraph::RagGraph(AiClient* ai, GraphMemory* gm, QObject* parent)
    : QObject(parent), m_ai(ai), m_gm(gm)
{}

/* ══════════════════════════════════════════════════════════════
   addDirectory — scansiona ricorsivamente e aggiunge file alla coda
   ══════════════════════════════════════════════════════════════ */
void RagGraph::addDirectory(const QString& dir)
{
    static const QStringList kFilters{
        "*.txt","*.md","*.pdf","*.rst","*.csv",
        "*.py","*.cpp","*.h","*.c","*.java","*.json"
    };

    QDirIterator it(dir, kFilters, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString fp = it.next();
        const QString label = QFileInfo(fp).fileName();
        m_fileQueue.append({fp, label});
    }
    m_stats.totalFiles = m_fileQueue.size();
}

void RagGraph::addFile(const QString& path, const QString& label)
{
    const QString lbl = label.isEmpty() ? QFileInfo(path).fileName() : label;

    /* Auto-copia in RAG/ se il file è fuori dalle dir monitorate.
     * Garantisce che il documento sia ritrovato al riavvio quando RagGraph
     * riscansiona ~/prismalux_rag_docs/ e P::root()+"/RAG/". */
    const QString ragDir  = P::root() + "/RAG";
    const QString ragDocs = QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
                            + "/prismalux_rag_docs";

    QString actualPath = path;
    if (!path.startsWith(ragDir) && !path.startsWith(ragDocs)) {
        QDir().mkpath(ragDir);
        const QString dest = ragDir + "/" + QFileInfo(path).fileName();
        if (!QFileInfo::exists(dest)) {
            if (QFile::copy(path, dest)) {
                actualPath = dest;
                emit fileCopied(lbl, dest);
            }
            /* Se la copia fallisce, indicizza dalla posizione originale */
        } else {
            actualPath = dest;  /* usa la copia già presente */
        }
    }

    m_fileQueue.append({actualPath, lbl});
    m_stats.totalFiles = m_fileQueue.size();
}

/* ══════════════════════════════════════════════════════════════
   startIngest — avvia elaborazione asincrona
   ══════════════════════════════════════════════════════════════ */
void RagGraph::startIngest()
{
    if (m_stats.running || !m_ai || !m_gm) return;
    if (m_fileQueue.isEmpty()) { emit finished(m_stats); return; }

    m_stats.running       = true;
    m_stats.processedFiles = 0;
    m_stats.totalEntities  = 0;
    m_stats.totalRelations = 0;
    m_currentFileIdx = 0;

    processNextFile();
}

void RagGraph::stopIngest()
{
    if (!m_stats.running) return;
    m_stats.running = false;
    delete m_llmHolder;
    m_llmHolder = nullptr;
    if (m_ai) m_ai->abort();
}

/* ══════════════════════════════════════════════════════════════
   processNextFile — prende il prossimo file dalla coda
   ══════════════════════════════════════════════════════════════ */
void RagGraph::processNextFile()
{
    if (!m_stats.running || m_currentFileIdx >= m_fileQueue.size()) {
        m_stats.running = false;
        emit finished(m_stats);
        return;
    }

    const auto [path, label] = m_fileQueue[m_currentFileIdx];
    emit progressUpdated(m_currentFileIdx + 1, m_stats.totalFiles, label);

    /* Legge il testo del file */
    QString text;

    if (path.endsWith(".pdf", Qt::CaseInsensitive)) {
        text = ProcHelper::run("pdftotext", {path, "-"}, 30000).out;
    } else {
        QFile f(path);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text))
            text = QString::fromUtf8(f.readAll());
    }

    if (text.trimmed().isEmpty()) {
        emit fileError(label, "File vuoto o non leggibile");
        ++m_currentFileIdx;
        processNextFile();
        return;
    }

    /* Prende un campione rappresentativo (prime N*chunkSize chars + fine) */
    const int maxLen = m_maxChunksPerFile * m_chunkSize;
    if (text.size() > maxLen)
        text = text.left(maxLen / 2) + "\n[...]\n" + text.right(maxLen / 2);

    extractEntities(text, label, 0);
}

/* ══════════════════════════════════════════════════════════════
   extractEntities — chiede all'LLM di estrarre entità e relazioni
   ══════════════════════════════════════════════════════════════ */
void RagGraph::extractEntities(const QString& text, const QString& source, int chunkIdx)
{
    const QString sys =
        "Sei un sistema di estrazione della conoscenza. Analizza il testo fornito"
        " e restituisci SOLO JSON valido (nessun testo extra, nessun markdown).\n\n"
        "Formato OBBLIGATORIO:\n"
        "{\n"
        "  \"entities\": [\n"
        "    {\"label\":\"nome\",\"type\":\"tipo\",\"importance\":0.8,\"description\":\"breve\"}\n"
        "  ],\n"
        "  \"relations\": [\n"
        "    {\"from\":\"nome_entita1\",\"to\":\"nome_entita2\","
        "     \"type\":\"tipo_relazione\",\"weight\":0.9}\n"
        "  ]\n"
        "}\n\n"
        "Tipi di entita' validi: concetto | framework | libreria | algoritmo |"
        " formula | persona | organizzazione | luogo | documento | tecnologia | altro\n"
        "Tipi di relazione validi: usa | basato_su | definisce | implementa |"
        " parte_di | causa | contraddice | estende | dipende_da\n\n"
        "Regole:\n"
        "- Massimo " + QString::number(m_maxEntities) + " entita'\n"
        "- Solo entita' significative (no articoli, no parole comuni)\n"
        "- importance: 0.0 (poco importante) - 1.0 (cruciale)\n"
        "- Restituisci JSON puro, senza ```json e senza testo";

    const QString user =
        "Sorgente: " + source + "\n\nTesto da analizzare:\n" + text.left(3000);

    m_accumJson.clear();

    delete m_llmHolder;
    m_llmHolder = new QObject(this);

    connect(m_ai, &AiClient::token,    m_llmHolder,
            [this](const QString& t)  { onLlmToken(t); });
    connect(m_ai, &AiClient::finished, m_llmHolder,
            [this, source, chunkIdx](const QString& f) {
                onLlmFinished(f);
                /* Salva chunk sorgente in mappa per searchChunks() */
                Q_UNUSED(chunkIdx)
                Q_UNUSED(source)
            });
    connect(m_ai, &AiClient::error,    m_llmHolder,
            [this](const QString& e)  { onLlmError(e); });

    /* Passa source e chunkIdx al parseAndStore via m_fileQueue */
    m_ai->chat(sys, user);
}

void RagGraph::onLlmToken(const QString& tok)
{
    m_accumJson += tok;
}

void RagGraph::onLlmFinished(const QString& full)
{
    delete m_llmHolder;
    m_llmHolder = nullptr;

    const auto [path, label] = m_fileQueue[m_currentFileIdx];
    parseAndStore(full, label);

    ++m_currentFileIdx;
    ++m_stats.processedFiles;
    processNextFile();
}

void RagGraph::onLlmError(const QString& msg)
{
    delete m_llmHolder;
    m_llmHolder = nullptr;

    const auto [path, label] = m_fileQueue[m_currentFileIdx];
    emit fileError(label, msg);

    ++m_currentFileIdx;
    processNextFile();
}

/* ══════════════════════════════════════════════════════════════
   parseAndStore — interpreta JSON e scrive in GraphMemory
   ══════════════════════════════════════════════════════════════ */
void RagGraph::parseAndStore(const QString& jsonText, const QString& source)
{
    if (!m_gm) return;

    /* Estrai JSON: potrebbe esserci testo attorno */
    QString clean = jsonText.trimmed();
    static const QRegularExpression reJson(R"(\{[\s\S]*\})");
    const auto m = reJson.match(clean);
    if (m.hasMatch()) clean = m.captured(0);

    /* Rimuovi blocchi markdown se presenti */
    clean.remove(QRegularExpression("```[a-z]*"));
    clean = clean.trimmed();

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(clean.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        emit fileError(source, "JSON non valido: " + err.errorString());
        return;
    }

    const QJsonObject root = doc.object();
    const QJsonArray entities  = root.value("entities").toArray();
    const QJsonArray relations = root.value("relations").toArray();

    /* Mappa label → nodeId per costruire gli archi */
    QMap<QString, QString> labelToId;

    /* Crea nodo sorgente documento */
    const QString docNodeId = m_gm->addNode(
        "documento", source, "Documento RAG indicizzato", 0.5f,
        {{"source_file", source}, {"rag_source", true}});
    labelToId[source] = docNodeId;

    /* Crea nodi entità */
    for (const QJsonValue& v : entities) {
        const QJsonObject e = v.toObject();
        const QString label = e.value("label").toString().trimmed();
        const QString type  = e.value("type").toString("concetto");
        const float  imp    = (float)e.value("importance").toDouble(0.7);
        const QString desc  = e.value("description").toString();

        if (label.isEmpty() || label.size() < 2) continue;

        /* Verifica se esiste già un nodo con questo label */
        auto existing = m_gm->nodeByLabel(label);
        QString nodeId;
        if (existing.has_value()) {
            /* Aumenta importanza del nodo esistente */
            nodeId = existing->id;
            const float newImp = std::min(1.0f, existing->importance + 0.1f);
            m_gm->updateNode(nodeId, desc.isEmpty() ? existing->content : desc, newImp);
        } else {
            nodeId = m_gm->addNode(type, label, desc, imp,
                                   {{"source", source}, {"rag_entity", true}});
            ++m_stats.totalEntities;
        }
        labelToId[label] = nodeId;

        /* Collega entità al documento sorgente */
        m_gm->addEdge(docNodeId, nodeId, "contiene", 0.6f);
    }

    /* Crea archi relazioni */
    for (const QJsonValue& v : relations) {
        const QJsonObject r = v.toObject();
        const QString from  = r.value("from").toString().trimmed();
        const QString to    = r.value("to").toString().trimmed();
        const QString type  = r.value("type").toString("usa");
        const float  weight = (float)r.value("weight").toDouble(0.8);

        if (from.isEmpty() || to.isEmpty()) continue;

        /* Risolvi etichette a id */
        QString fromId = labelToId.value(from);
        QString toId   = labelToId.value(to);

        /* Fallback: cerca nel grafo */
        if (fromId.isEmpty()) {
            auto n = m_gm->nodeByLabel(from);
            if (n.has_value()) fromId = n->id;
            else fromId = m_gm->addNode("concetto", from, {}, 0.5f,
                                        {{"source", source}, {"auto_created", true}});
            labelToId[from] = fromId;
        }
        if (toId.isEmpty()) {
            auto n = m_gm->nodeByLabel(to);
            if (n.has_value()) toId = n->id;
            else toId = m_gm->addNode("concetto", to, {}, 0.5f,
                                      {{"source", source}, {"auto_created", true}});
            labelToId[to] = toId;
        }

        if (!fromId.isEmpty() && !toId.isEmpty()) {
            m_gm->addEdge(fromId, toId, type, weight,
                          {{"source", source}});
            ++m_stats.totalRelations;
        }
    }
}

/* ══════════════════════════════════════════════════════════════
   searchChunks — restituisce chunk sorgente rilevanti per query
   ══════════════════════════════════════════════════════════════ */
QVector<RagGraphChunk> RagGraph::searchChunks(const QString& query, int limit) const
{
    QVector<RagGraphChunk> result;
    if (!m_gm) return result;

    /* Cerca nodi nel grafo */
    const auto nodes = m_gm->searchNodes(query, limit);
    for (const auto& n : nodes) {
        /* Recupera chunk sorgente */
        if (m_chunkMap.contains(n.id))
            result.append(m_chunkMap[n.id]);
        else {
            /* Fallback: crea chunk sintetico dal nodo */
            RagGraphChunk c;
            c.text   = "[" + n.type + "] " + n.label + "\n" + n.content;
            c.source = n.meta.value("source").toString();
            c.chunkIdx = 0;
            result.append(c);
        }
    }
    return result;
}

QVector<GmNode> RagGraph::searchNodes(const QString& query, int limit) const
{
    if (!m_gm) return {};
    return m_gm->searchNodes(query, limit);
}
