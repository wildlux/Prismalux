#include "rag_engine.h"
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <algorithm>
#include <cmath>
#include <random>
#ifdef HAVE_QT_SQL
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDataStream>
#include <QDateTime>
#include <QUuid>
#include <QHash>
#endif

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

/* ══════════════════════════════════════════════════════════════
   saveDb — esporta l'indice in un unico file SQLite

   Formato:
     rag_meta   (k TEXT PRIMARY KEY, v TEXT)
       format_version, seed, input_dim, jlt_dim, chunk_count,
       exported_at, app
     rag_chunks (id INTEGER PRIMARY KEY, text TEXT, source TEXT,
                 mtime INTEGER, vec BLOB)

   I vettori sono float32 little-endian: il file resta leggibile su
   qualunque macchina, indipendentemente dall'endianness dell'host.
   Il JSON resta il formato di lavoro; il DB è pensato per il
   trasporto (un file solo, indicizzato, comprimibile).
   ══════════════════════════════════════════════════════════════ */
bool RagEngine::saveDb(const QString& path) const
{
#ifdef HAVE_QT_SQL
    if (path.isEmpty()) return false;

    /* Connessione dedicata: non riusare quella della cache decisionale
       né quella dei quiz, che possono avere un file diverso aperto. */
    const QString conn = QStringLiteral("prismalux_rag_export_%1")
                             .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    bool ok = false;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), conn);
        db.setDatabaseName(path);
        if (!db.open()) {
            QSqlDatabase::removeDatabase(conn);
            return false;
        }

        QSqlQuery q(db);
        ok = q.exec(QStringLiteral("DROP TABLE IF EXISTS rag_meta"))
          && q.exec(QStringLiteral("DROP TABLE IF EXISTS rag_chunks"))
          && q.exec(QStringLiteral(
                 "CREATE TABLE rag_meta (k TEXT PRIMARY KEY, v TEXT)"))
          && q.exec(QStringLiteral(
                 "CREATE TABLE rag_chunks ("
                 "  id INTEGER PRIMARY KEY,"
                 "  text TEXT NOT NULL,"
                 "  source TEXT,"
                 "  mtime INTEGER,"
                 "  vec BLOB NOT NULL)"));

        if (ok) {
            db.transaction();

            q.prepare(QStringLiteral("INSERT INTO rag_meta (k, v) VALUES (?, ?)"));
            const auto putMeta = [&q](const QString& k, const QString& v) {
                q.bindValue(0, k);
                q.bindValue(1, v);
                return q.exec();
            };
            ok = putMeta(QStringLiteral("format_version"), QStringLiteral("1"))
              && putMeta(QStringLiteral("seed"), QString::number(m_seed))
              && putMeta(QStringLiteral("input_dim"), QString::number(m_inputDim))
              && putMeta(QStringLiteral("jlt_dim"), QString::number(kTargetDim))
              && putMeta(QStringLiteral("chunk_count"), QString::number(m_chunks.size()))
              && putMeta(QStringLiteral("exported_at"),
                         QString::number(QDateTime::currentSecsSinceEpoch()))
              && putMeta(QStringLiteral("app"), QStringLiteral("Prismalux"));

            if (ok) {
                q.prepare(QStringLiteral(
                    "INSERT INTO rag_chunks (text, source, mtime, vec) "
                    "VALUES (?, ?, ?, ?)"));
                for (const RagChunk& c : m_chunks) {
                    QByteArray blob;
                    blob.reserve(c.vec.size() * int(sizeof(float)));
                    {
                        QDataStream ds(&blob, QIODevice::WriteOnly);
                        ds.setByteOrder(QDataStream::LittleEndian);
                        ds.setFloatingPointPrecision(QDataStream::SinglePrecision);
                        for (float f : c.vec) ds << f;
                    }
                    q.bindValue(0, c.text);
                    q.bindValue(1, c.source);
                    q.bindValue(2, c.mtime);
                    q.bindValue(3, blob);
                    if (!q.exec()) { ok = false; break; }
                }
            }

            if (ok) db.commit();
            else    db.rollback();
        }

        db.close();
    }
    QSqlDatabase::removeDatabase(conn);
    return ok;
#else
    Q_UNUSED(path);
    return false;
#endif
}

/* ══════════════════════════════════════════════════════════════
   loadDb — importa un indice esportato con saveDb()

   Il file viene aperto in sola lettura. Prima di sostituire l'indice
   in memoria si valida tutto: un DB corrotto non deve azzerare
   l'indice corrente. Per questo i chunk vengono accumulati a parte e
   assegnati solo alla fine.
   ══════════════════════════════════════════════════════════════ */
bool RagEngine::loadDb(const QString& path)
{
#ifdef HAVE_QT_SQL
    if (path.isEmpty() || !QFileInfo::exists(path)) return false;

    const QString conn = QStringLiteral("prismalux_rag_import_%1")
                             .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    bool ok = false;
    QVector<RagChunk> loaded;
    int  dim  = 0;
    uint32_t sd = m_seed;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), conn);
        db.setDatabaseName(path);
        if (!db.open()) {
            QSqlDatabase::removeDatabase(conn);
            return false;
        }

        QSqlQuery q(db);
        if (q.exec(QStringLiteral("SELECT k, v FROM rag_meta"))) {
            while (q.next()) {
                const QString k = q.value(0).toString();
                const QString v = q.value(1).toString();
                if (k == QLatin1String("input_dim")) dim = v.toInt();
                else if (k == QLatin1String("seed")) sd = uint32_t(v.toUInt());
            }
            ok = true;
        }

        if (ok && q.exec(QStringLiteral(
                "SELECT text, source, mtime, vec FROM rag_chunks ORDER BY id"))) {
            while (q.next()) {
                RagChunk c;
                c.text   = q.value(0).toString();
                c.source = q.value(1).toString();
                c.mtime  = q.value(2).toLongLong();
                const QByteArray blob = q.value(3).toByteArray();

                QDataStream ds(blob);
                ds.setByteOrder(QDataStream::LittleEndian);
                ds.setFloatingPointPrecision(QDataStream::SinglePrecision);
                const int n = blob.size() / int(sizeof(float));
                c.vec.reserve(n);
                for (int i = 0; i < n; ++i) {
                    float f = 0.0f;
                    ds >> f;
                    c.vec.append(f);
                }
                if (!c.text.isEmpty() && !c.vec.isEmpty())
                    loaded.append(c);
            }
        } else {
            ok = false;
        }

        db.close();
    }
    QSqlDatabase::removeDatabase(conn);

    /* input_dim mancante: lo si deduce dal primo vettore solo se
       coincide con la dimensione JLT attesa (i vettori salvati sono
       già proiettati, quindi != kTargetDim indica un file incoerente). */
    if (ok && dim <= 0 && !loaded.isEmpty())
        dim = kTargetDim;
    if (!ok || loaded.isEmpty() || dim <= 0) return false;

    m_seed = sd;
    initMatrix(dim);
    m_chunks = loaded;
    return true;
#else
    Q_UNUSED(path);
    return false;
#endif
}
