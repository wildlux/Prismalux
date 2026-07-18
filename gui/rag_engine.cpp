#include "rag_engine.h"
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <algorithm>
#include <cmath>
#include <random>

/* ══════════════════════════════════════════════════════════════
   initMatrix — genera la matrice di proiezione R (m × d)
   con elementi ~ N(0, 1/√m) dal seed deterministico.
   Chiamata automaticamente la prima volta che project() riceve
   un vettore di dimensione d.
   ══════════════════════════════════════════════════════════════ */
void RagEngine::initMatrix(int d) {
    m_inputDim = d;
    m_R.resize(kTargetDim);
    std::mt19937                    gen(m_seed);
    std::normal_distribution<float> dist(0.0f, 1.0f / std::sqrt((float)kTargetDim));
    for (int i = 0; i < kTargetDim; ++i) {
        m_R[i].resize(d);
        for (int j = 0; j < d; ++j)
            m_R[i][j] = dist(gen);
    }
}

/* ══════════════════════════════════════════════════════════════
   project — proietta fullEmb (inputDim) → kTargetDim
   (ri-genera la matrice se la dimensione cambia)
   ══════════════════════════════════════════════════════════════ */
QVector<float> RagEngine::project(const QVector<float>& fullEmb) {
    if (fullEmb.isEmpty()) return {};
    int d = fullEmb.size();
    if (d != m_inputDim || m_R.isEmpty())
        initMatrix(d);

    QVector<float> out(kTargetDim, 0.0f);
    for (int i = 0; i < kTargetDim; ++i) {
        float s = 0.0f;
        const QVector<float>& row = m_R[i];
        for (int j = 0; j < d; ++j)
            s += row[j] * fullEmb[j];
        out[i] = s;
    }
    return out;
}

/* ══════════════════════════════════════════════════════════════
   addChunk
   ══════════════════════════════════════════════════════════════ */
void RagEngine::addChunk(const QString& text, const QVector<float>& fullEmb,
                          const QString& source, qint64 mtime) {
    RagChunk c;
    c.text   = text;
    c.source = source;
    c.mtime  = mtime;
    c.vec    = project(fullEmb);
    m_chunks.append(c);
}

bool RagEngine::isFileIndexed(const QString& source, qint64 mtime) const {
    if (source.isEmpty()) return false;
    for (const RagChunk& c : m_chunks)
        if (c.source == source && c.mtime == mtime)
            return true;
    return false;
}

void RagEngine::removeChunksForFile(const QString& source) {
    if (source.isEmpty()) return;
    m_chunks.erase(
        std::remove_if(m_chunks.begin(), m_chunks.end(),
            [&](const RagChunk& c){ return c.source == source; }),
        m_chunks.end());
}

int RagEngine::removeLegacyChunks() {
    const int before = m_chunks.size();
    m_chunks.erase(
        std::remove_if(m_chunks.begin(), m_chunks.end(),
            [](const RagChunk& c){ return c.source.isEmpty(); }),
        m_chunks.end());
    return before - m_chunks.size();
}

QHash<QString, qint64> RagEngine::indexedFileMap() const {
    QHash<QString, qint64> map;
    for (const RagChunk& c : m_chunks)
        if (!c.source.isEmpty())
            map.insert(c.source, c.mtime);
    return map;
}

/* ══════════════════════════════════════════════════════════════
   cosine — similarità coseno
   ══════════════════════════════════════════════════════════════ */
float RagEngine::cosine(const QVector<float>& a, const QVector<float>& b) const {
    float dot = 0.0f, na = 0.0f, nb = 0.0f;
    int n = qMin(a.size(), b.size());
    for (int i = 0; i < n; ++i) {
        dot += a[i] * b[i];
        na  += a[i] * a[i];
        nb  += b[i] * b[i];
    }
    float denom = std::sqrt(na) * std::sqrt(nb);
    return (denom > 1e-9f) ? (dot / denom) : 0.0f;
}

/* ══════════════════════════════════════════════════════════════
   search — top-k chunk per coseno nello spazio JLT
   La query viene proiettata inline (matrice già inizializzata).
   ══════════════════════════════════════════════════════════════ */
QVector<QPair<RagChunk, float>> RagEngine::searchScored(const QVector<float>& queryEmb, int k) const {
    if (m_chunks.isEmpty() || m_R.isEmpty()) return {};

    /* Proiezione JLT della query (senza mutare stato: usa m_R già pronta) */
    int d = qMin(queryEmb.size(), m_inputDim);
    QVector<float> qVec(kTargetDim, 0.0f);
    for (int i = 0; i < kTargetDim; ++i) {
        float s = 0.0f;
        for (int j = 0; j < d; ++j)
            s += m_R[i][j] * queryEmb[j];
        qVec[i] = s;
    }

    /* Coseno con tutti i chunk, poi partial_sort */
    struct Scored { float score; int idx; };
    QVector<Scored> sc;
    sc.reserve(m_chunks.size());
    for (int i = 0; i < m_chunks.size(); ++i)
        sc.append({ cosine(qVec, m_chunks[i].vec), i });

    int topK = qMin(k, (int)sc.size());
    std::partial_sort(sc.begin(), sc.begin() + topK, sc.end(),
        [](const Scored& a, const Scored& b){ return a.score > b.score; });

    QVector<QPair<RagChunk, float>> result;
    result.reserve(topK);
    for (int i = 0; i < topK; ++i)
        result.append({ m_chunks[sc[i].idx], sc[i].score });
    return result;
}

QVector<RagChunk> RagEngine::search(const QVector<float>& queryEmb, int k) const {
    const auto scored = searchScored(queryEmb, k);
    QVector<RagChunk> result;
    result.reserve(scored.size());
    for (const auto& p : scored)
        result.append(p.first);
    return result;
}

/* ══════════════════════════════════════════════════════════════
   save — indice su JSON compact
   Formato: { seed, input_dim, jlt_dim, chunks:[{text, vec:[...]}] }
   ══════════════════════════════════════════════════════════════ */
bool RagEngine::save(const QString& path) const {
    QJsonObject root;
    root["seed"]      = (int)m_seed;
    root["input_dim"] = m_inputDim;
    root["jlt_dim"]   = kTargetDim;

    QJsonArray chunks;
    for (const RagChunk& c : m_chunks) {
        QJsonObject co;
        co["text"] = c.text;
        if (!c.source.isEmpty()) co["s"] = c.source;
        if (c.mtime)             co["m"] = c.mtime;
        QJsonArray va;
        for (float f : c.vec) va.append((double)f);
        co["vec"] = va;
        chunks.append(co);
    }
    root["chunks"] = chunks;

    /* D-59: ruota il salvataggio precedente in .bak prima di troncare —
       un crash o disco pieno a metà scrittura non distrugge l'unico indice. */
    if (QFileInfo::exists(path)) {
        const QString bak = path + ".bak";
        QFile::remove(bak);
        QFile::copy(path, bak);
    }

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    return true;
}

/* ══════════════════════════════════════════════════════════════
   load — carica indice da JSON
   ══════════════════════════════════════════════════════════════ */
bool RagEngine::load(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    if (root.isEmpty()) return false;

    m_seed = (uint32_t)root["seed"].toInt((int)m_seed);
    int d  = root["input_dim"].toInt(0);
    if (d > 0) initMatrix(d);

    m_chunks.clear();
    for (const QJsonValue& cv : root["chunks"].toArray()) {
        const QJsonObject co = cv.toObject();
        RagChunk c;
        c.text   = co["text"].toString();
        c.source = co["s"].toString();
        c.mtime  = (qint64)co["m"].toDouble(0);
        const QJsonArray va = co["vec"].toArray();
        c.vec.reserve(va.size());
        for (const QJsonValue& v : va)
            c.vec.append((float)v.toDouble());
        if (!c.text.isEmpty() && !c.vec.isEmpty())
            m_chunks.append(c);
    }
    return !m_chunks.isEmpty();
}
