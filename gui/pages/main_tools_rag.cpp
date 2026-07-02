/* ══════════════════════════════════════════════════════════════
   main_tools_rag.cpp — StrumentiPage: RAG in-page
   ==================================================================
   Indicizzazione e retrieval keyword-based (PDF/TXT/MD) — builder +
   slot. Split da main_tools.cpp (TODO D-8).
   ══════════════════════════════════════════════════════════════ */
#include "main_tools.h"
#include "../prismalux_paths.h"
#include "../log_bus.h"
#include "../widgets/proc_helper.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QTextStream>
#include <QIODevice>

namespace P = PrismaluxPaths;

/* ══════════════════════════════════════════════════════════════
   RAG in-page — indicizzazione e retrieval keyword-based
   ══════════════════════════════════════════════════════════════ */

/* Estrae testo grezzo da PDF (pdftotext), TXT o MD */
QString StrumentiPage::ragExtractText(const QString& path)
{
    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == "pdf") {
        const auto r = ProcHelper::run("pdftotext", {path, "-"}, 15000);
        return r.ok ? r.out : QString();
    }
    /* TXT / MD / CSV / RST — lettura diretta */
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    QTextStream ts(&f);
    return ts.readAll();
}

/* Divide il testo in chunk di ~chunkSize caratteri con overlap */
QStringList StrumentiPage::ragChunkText(const QString& text,
                                        int chunkSize, int overlap)
{
    QStringList chunks;
    const QString t = text.simplified();
    if (t.isEmpty()) return chunks;
    int pos = 0;
    while (pos < t.size()) {
        chunks << t.mid(pos, chunkSize);
        if (pos + chunkSize >= t.size()) break;
        pos += chunkSize - overlap;
    }
    return chunks;
}

/* Carica un file e aggiunge i chunk all'indice */
void StrumentiPage::ragAddFile(const QString& path)
{
    const QString text = ragExtractText(path);
    if (text.trimmed().isEmpty()) {
        m_ragInfoLbl->setText(
            QString("\xe2\x9d\x8c  Impossibile estrarre testo da: %1")
            .arg(QFileInfo(path).fileName()));
        LogBus::post("\xe2\x9d\x8c Strumenti: Impossibile estrarre testo da: " + QFileInfo(path).fileName());
        return;
    }
    const QStringList newChunks = ragChunkText(text);
    m_ragChunks << newChunks;
    const QString fname = QFileInfo(path).fileName();
    if (!m_ragFileNames.contains(fname))
        m_ragFileNames << fname;
    m_ragCheck->setChecked(true);
    m_ragInfoLbl->setText(
        QString("\xf0\x9f\x93\x9a  %1 chunk  \xe2\x80\x94  %2")
        .arg(m_ragChunks.size())
        .arg(m_ragFileNames.count() <= 3
            ? m_ragFileNames.join(", ")
            : m_ragFileNames.first() +
              QString(" +%1 altri").arg(m_ragFileNames.count() - 1)));
}

/* Restituisce i top-k chunk per keyword scoring rispetto alla query */
QString StrumentiPage::ragBuildContext(const QString& query, int topK) const
{
    if (m_ragChunks.isEmpty()) return {};

    /* Parole chiave della query (len > 3, no stopword banali) */
    static const QSet<QString> stopwords = {
        "che","con","del","della","delle","dei","gli","per","una",
        "uno","non","nel","nei","sul","sui","dalla","dalle","sulle"
    };
    const QStringList words = query.toLower()
        .split(QRegularExpression("[\\W_]+"), Qt::SkipEmptyParts);
    QStringList keywords;
    for (const QString& w : words)
        if (w.length() > 3 && !stopwords.contains(w))
            keywords << w;

    if (keywords.isEmpty()) {
        /* Nessuna keyword utile: restituisci i primi topK chunk */
        QStringList out;
        for (int i = 0; i < qMin(topK, m_ragChunks.size()); ++i)
            out << m_ragChunks[i];
        return out.join("\n\n---\n\n");
    }

    /* Punteggio: quante keyword appaiono nel chunk */
    QVector<QPair<int,int>> scores; /* (score, idx) */
    for (int i = 0; i < m_ragChunks.size(); ++i) {
        const QString low = m_ragChunks[i].toLower();
        int score = 0;
        for (const QString& kw : keywords)
            if (low.contains(kw)) score++;
        if (score > 0) scores.append({score, i});
    }

    /* Ordina per score decrescente */
    std::sort(scores.begin(), scores.end(),
              [](const QPair<int,int>& a, const QPair<int,int>& b){
                  return a.first > b.first; });

    QStringList out;
    const int n = qMin(topK, static_cast<int>(scores.size()));
    for (int i = 0; i < n; ++i)
        out << m_ragChunks[scores[i].second];

    /* Fallback se nessun chunk ha score > 0 */
    if (out.isEmpty())
        for (int i = 0; i < qMin(topK, m_ragChunks.size()); ++i)
            out << m_ragChunks[i];

    return out.join("\n\n---\n\n");
}

/* ══════════════════════════════════════════════════════════════
   Slot: RAG aggiungi documenti
   ══════════════════════════════════════════════════════════════ */
void StrumentiPage::onRagAddBtnClicked()
{
    const QStringList paths = QFileDialog::getOpenFileNames(
        this,
        "Seleziona documenti per RAG",
        "",
        "Documenti (*.pdf *.txt *.md *.csv *.rst);;"
        "Tutti i file (*)");
    for (const QString& p : paths)
        ragAddFile(p);
}

/* ══════════════════════════════════════════════════════════════
   Slot: RAG svuota indice
   ══════════════════════════════════════════════════════════════ */
void StrumentiPage::onRagClearBtnClicked()
{
    m_ragChunks.clear();
    m_ragFileNames.clear();
    m_ragCheck->setChecked(false);
    m_ragInfoLbl->setText(tr("Nessun documento caricato"));
}

