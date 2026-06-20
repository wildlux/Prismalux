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
        "*.py","*.cpp","*.h","*.c","*.java","*.json",
        /* immagini */
        "*.png","*.jpg","*.jpeg","*.webp","*.bmp","*.tiff","*.tif",
        /* video */
        "*.mp4","*.avi","*.mkv","*.mov","*.webm",
        /* documenti Office */
        "*.docx","*.odt","*.doc"
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
     * riscansiona ~/prismalux_rag_docs/ e P::ragDir(). */
    const QString ragDir  = P::ragDir();
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
    m_paused        = false;
    m_stats.running = false;
    delete m_llmHolder;
    m_llmHolder = nullptr;
    if (m_ai) m_ai->abort();
}

void RagGraph::pauseIngest()
{
    if (m_stats.running && !m_paused)
        m_paused = true;
}

void RagGraph::resumeIngest()
{
    if (m_stats.running && m_paused) {
        m_paused = false;
        processNextFile();
    }
}

/* ══════════════════════════════════════════════════════════════
   processNextFile — prende il prossimo file dalla coda
   ══════════════════════════════════════════════════════════════ */
void RagGraph::processNextFile()
{
    if (m_paused) return;   /* pausa: il prossimo file aspetta resumeIngest() */
    if (!m_stats.running || m_currentFileIdx >= m_fileQueue.size()) {
        m_stats.running = false;
        emit finished(m_stats);
        return;
    }

    const auto [path, label] = m_fileQueue[m_currentFileIdx];
    emit progressUpdated(m_currentFileIdx + 1, m_stats.totalFiles, label);

    /* Legge il testo del file */
    QString text;

    const QString lower = path.toLower();

    auto isImg = [&](const QString& p) {
        return p.endsWith(".png") || p.endsWith(".jpg") || p.endsWith(".jpeg")
            || p.endsWith(".webp") || p.endsWith(".bmp")
            || p.endsWith(".tiff") || p.endsWith(".tif");
    };
    auto isVideo = [&](const QString& p) {
        return p.endsWith(".mp4") || p.endsWith(".avi") || p.endsWith(".mkv")
            || p.endsWith(".mov") || p.endsWith(".webm");
    };
    auto isOffice = [&](const QString& p) {
        return p.endsWith(".docx") || p.endsWith(".odt") || p.endsWith(".doc");
    };

    if (lower.endsWith(".pdf")) {
        text = ProcHelper::run("pdftotext", {path, "-"}, 30000).out;

    } else if (isImg(lower)) {
        /* OCR via tesseract */
        const QString tmpBase = P::tmpDir() + "rag_ocr";
        const QString tmpTxt  = tmpBase + ".txt";
        QFile::remove(tmpTxt);
        ProcHelper::run("tesseract", {path, tmpBase, "txt"}, 20000);
        QFile f(tmpTxt);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text))
            text = QString::fromUtf8(f.readAll());
        QFile::remove(tmpTxt);

    } else if (isVideo(lower)) {
        /* Estrai audio con ffmpeg → WAV, poi trascrivi con whisper se disponibile */
        const QString wavTmp = P::tmpDir() + "rag_audio.wav";
        QFile::remove(wavTmp);
        ProcHelper::run("ffmpeg",
            {"-y","-i",path,"-ac","1","-ar","16000","-f","wav", wavTmp},
            60000);

        /* Prova whisper CLI, poi python3 -m whisper, poi fallback ffprobe */
        bool transcribed = false;
        const QString srtTmp = P::tmpDir() + "rag_audio.txt";
        QFile::remove(srtTmp);

        if (QFileInfo::exists(wavTmp)) {
            auto res = ProcHelper::run("whisper",
                {wavTmp,"--model","tiny","--output_format","txt",
                 "--output_dir", P::safeTempPath(),"--language","it"}, 120000);
            if (!res.out.trimmed().isEmpty()) {
                text = res.out;
                transcribed = true;
            } else {
                auto res2 = ProcHelper::run(P::findPython(),
                    {"-m","whisper", wavTmp,"--model","tiny",
                     "--output_format","txt","--output_dir","/tmp",
                     "--language","it"}, 120000);
                if (!res2.out.trimmed().isEmpty()) {
                    text = res2.out;
                    transcribed = true;
                }
                /* Leggi file .txt generato da whisper se stdout vuoto */
                if (!transcribed) {
                    QFile fTxt(srtTmp);
                    if (fTxt.open(QIODevice::ReadOnly | QIODevice::Text)) {
                        text = QString::fromUtf8(fTxt.readAll());
                        transcribed = !text.trimmed().isEmpty();
                    }
                }
            }
        }

        if (!transcribed) {
            /* Fallback: metadati video via ffprobe */
            auto meta = ProcHelper::run("ffprobe",
                {"-v","quiet","-print_format","json","-show_format",path}, 15000);
            text = QString("[VIDEO] %1\nMetadati: %2").arg(label, meta.out);
        }

        QFile::remove(wavTmp);
        QFile::remove(srtTmp);

    } else if (isOffice(lower)) {
        /* Conversione a testo via LibreOffice */
        ProcHelper::run("libreoffice",
            {"--headless","--convert-to","txt","--outdir","/tmp", path},
            60000);
        const QString baseName = QFileInfo(path).completeBaseName();
        const QString outTxt   = "/tmp/" + baseName + ".txt";
        QFile f(outTxt);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text))
            text = QString::fromUtf8(f.readAll());
        QFile::remove(outTxt);

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

    /* Tutti gli inserimenti di questo documento in una transazione atomica */
    m_gm->beginBatch();

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

    m_gm->endBatch();
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
