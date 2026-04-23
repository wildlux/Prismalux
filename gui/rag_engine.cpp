#include "rag_engine.h"
#include <QFile>
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
void RagEngine::addChunk(const QString& text, const QVector<float>& fullEmb) {
    RagChunk c;
    c.text = text;
    c.vec  = project(fullEmb);
    m_chunks.append(c);
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
QVector<RagChunk> RagEngine::search(const QVector<float>& queryEmb, int k) const {
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

    QVector<RagChunk> result;
    result.reserve(topK);
    for (int i = 0; i < topK; ++i)
        result.append(m_chunks[sc[i].idx]);
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
        QJsonArray va;
        for (float f : c.vec) va.append((double)f);
        co["vec"] = va;
        chunks.append(co);
    }
    root["chunks"] = chunks;

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
        c.text = co["text"].toString();
        const QJsonArray va = co["vec"].toArray();
        c.vec.reserve(va.size());
        for (const QJsonValue& v : va)
            c.vec.append((float)v.toDouble());
        if (!c.text.isEmpty() && !c.vec.isEmpty())
            m_chunks.append(c);
    }
    return !m_chunks.isEmpty();
}
