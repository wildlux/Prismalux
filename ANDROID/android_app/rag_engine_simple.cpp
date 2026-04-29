#include "rag_engine_simple.h"
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QStringList>
#include <algorithm>

/* ── Stopword italiane e inglesi da ignorare nel matching ──── */
static const QSet<QString> kStopwords = {
    "il","lo","la","i","gli","le","un","uno","una","di","a","da","in",
    "con","su","per","tra","fra","e","o","ma","se","che","non","si",
    "mi","ti","ci","vi","ne","ho","ha","hanno","è","era","sono","the",
    "a","an","is","in","on","at","to","of","and","or","not","be","was"
};

/* ══════════════════════════════════════════════════════════════
   tokenize — lowercase + rimozione stopword + punteggiatura
   ══════════════════════════════════════════════════════════════ */
QStringList RagEngineSimple::tokenize(const QString& text)
{
    QStringList out;
    /* Divide su spazi e punteggiatura */
    const QStringList raw = text.toLower()
        .replace(QRegularExpression("[^a-zàèéìòùa-z0-9\\s]"), " ")
        .split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    for (const QString& w : raw) {
        if (w.length() > 2 && !kStopwords.contains(w))
            out.append(w);
    }
    return out;
}

/* ══════════════════════════════════════════════════════════════
   score — TF semplice: termini query trovati nel chunk / totale termini query
   Bonus +0.5 per ogni termine che appare più di una volta (TF boost).
   ══════════════════════════════════════════════════════════════ */
float RagEngineSimple::score(const QStringList& queryTerms, const Chunk& chunk)
{
    if (queryTerms.isEmpty()) return 0.0f;
    float s = 0.0f;
    for (const QString& qt : queryTerms) {
        const int cnt = chunk.terms.count(qt);
        if (cnt > 0) s += 1.0f + (cnt > 1 ? 0.5f : 0.0f);
    }
    return s / (float)queryTerms.size();
}

/* ══════════════════════════════════════════════════════════════
   Costruttore
   ══════════════════════════════════════════════════════════════ */
RagEngineSimple::RagEngineSimple(QObject* parent)
    : QObject(parent)
{}

/* ══════════════════════════════════════════════════════════════
   addDocument — divide in chunk con sliding window e indicizza
   ══════════════════════════════════════════════════════════════ */
void RagEngineSimple::addDocument(const QString& title, const QString& text)
{
    if (text.trimmed().isEmpty()) return;

    int pos = 0;
    while (pos < text.length()) {
        const QString chunkText = text.mid(pos, kChunkSize);
        Chunk c;
        c.source = title;
        c.text   = chunkText.trimmed();
        c.terms  = tokenize(c.text);
        if (!c.text.isEmpty()) m_chunks.append(c);
        pos += kChunkSize - kChunkOverlap;
    }
}

/* ══════════════════════════════════════════════════════════════
   load — carica un file .txt o tutti i .txt di una cartella
   ══════════════════════════════════════════════════════════════ */
bool RagEngineSimple::load(const QString& pathOrDir)
{
    QFileInfo fi(pathOrDir);
    QStringList files;

    if (fi.isDir()) {
        for (const QFileInfo& f : QDir(pathOrDir).entryInfoList({"*.txt","*.md"},
                                                                  QDir::Files))
            files << f.absoluteFilePath();
    } else if (fi.isFile()) {
        files << pathOrDir;
    } else {
        return false;
    }

    int prev = m_chunks.size();
    for (const QString& fp : files) {
        QFile f(fp);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
        QTextStream in(&f);
        in.setEncoding(QStringConverter::Utf8);
        addDocument(QFileInfo(fp).fileName(), in.readAll());
    }

    const int added = m_chunks.size() - prev;
    if (added > 0) emit loaded(m_chunks.size());
    return added > 0;
}

/* ══════════════════════════════════════════════════════════════
   clear
   ══════════════════════════════════════════════════════════════ */
void RagEngineSimple::clear()
{
    m_chunks.clear();
}

/* ══════════════════════════════════════════════════════════════
   search — top-k chunk per TF score
   ══════════════════════════════════════════════════════════════ */
QStringList RagEngineSimple::search(const QString& query, int k) const
{
    if (m_chunks.isEmpty()) return {};
    const QStringList qTerms = tokenize(query);
    if (qTerms.isEmpty()) return {};

    struct Scored { float s; int idx; };
    QVector<Scored> sc;
    sc.reserve(m_chunks.size());
    for (int i = 0; i < m_chunks.size(); ++i)
        sc.append({ score(qTerms, m_chunks[i]), i });

    const int topK = qMin(k, (int)sc.size());
    std::partial_sort(sc.begin(), sc.begin() + topK, sc.end(),
        [](const Scored& a, const Scored& b){ return a.s > b.s; });

    QStringList out;
    out.reserve(topK);
    for (int i = 0; i < topK; ++i)
        if (sc[i].s > 0.0f)           // escludi chunk con score zero
            out << m_chunks[sc[i].idx].text;
    return out;
}

/* ══════════════════════════════════════════════════════════════
   searchForPrompt — come search() ma tronca a kMaxChunkLen
   ══════════════════════════════════════════════════════════════ */
QStringList RagEngineSimple::searchForPrompt(const QString& query, int k) const
{
    QStringList raw = search(query, k);
    QStringList out;
    out.reserve(raw.size());
    for (const QString& t : raw) {
        if (t.length() > kMaxChunkLen)
            out << t.left(kMaxChunkLen) + QStringLiteral("\u2026");
        else
            out << t;
    }
    return out;
}
