#pragma once
#include <QVector>
#include <QString>
#include <QPair>
#include <cstdint>

/* ══════════════════════════════════════════════════════════════
   RagEngine — Indice RAG con compressione Johnson-Lindenstrauss

   Flusso tipico:
     1. addChunk(text, fullEmb)  — proietta via JLT e memorizza
     2. search(queryEmb, k)      — top-k chunk per coseno
     3. save(path) / load(path)  — persiste su ~/.prismalux_rag.json

   JLT (Johnson–Lindenstrauss Transform):
     Proietta embedding da inputDim (es. 4096) → kTargetDim (256)
     con matrice casuale R generata da seed deterministico.
     Garantisce preservazione distanze con ε≈0.10 fino a ~10.000 chunk
     (dal lemma: m = Ω(ε⁻² log n) → 256 = Ω(100·log(10000)) ✓ ).
     Vantaggio concreto: 16× meno RAM, ricerca ~16× più veloce.
   ══════════════════════════════════════════════════════════════ */

struct RagChunk {
    QString        text;          ///< testo originale del chunk
    QString        source;        ///< percorso file sorgente (canonico)
    qint64         mtime = 0;     ///< lastModified in msecs since epoch
    QVector<float> vec;           ///< embedding compresso: kTargetDim float
};

class RagEngine {
public:
    static constexpr int kTargetDim = 256;  ///< dimensione output JLT

    RagEngine() = default;

    /* ── Costruzione indice ────────────────────────────────────── */

    /** Proietta fullEmb (d float) → kTargetDim float via JLT. */
    QVector<float> project(const QVector<float>& fullEmb);

    /** Aggiunge un chunk: proietta e memorizza nell'indice. */
    void addChunk(const QString& text, const QVector<float>& fullEmb,
                  const QString& source = {}, qint64 mtime = 0);

    /** True se il file è già nell'indice con lo stesso mtime (non modificato). */
    bool isFileIndexed(const QString& source, qint64 mtime) const;

    /** Rimuove tutti i chunk del file indicato (file eliminato o modificato). */
    void removeChunksForFile(const QString& source);

    /** Rimuove i chunk senza source (indice creato prima dell'attribuzione
     *  file): non è possibile sapere a quale file appartengono, quindi i loro
     *  file risultano "da indicizzare" e verrebbero duplicati a ogni giro.
     *  Ritorna quanti chunk sono stati rimossi. */
    int removeLegacyChunks();

    /** Mappa path→mtime di tutti i file attualmente nell'indice. */
    QHash<QString, qint64> indexedFileMap() const;

    /* ── Query ─────────────────────────────────────────────────── */

    /** Ritorna i top-k chunk per similarità coseno nello spazio JLT.
     *  queryEmb deve avere la stessa dimensione degli embedding indicizzati. */
    QVector<RagChunk> search(const QVector<float>& queryEmb, int k = 5) const;

    /** Come search(), ma espone anche il coseno di ogni hit — usato per il
     *  gating a soglia (inietta solo i chunk davvero pertinenti alla query). */
    QVector<QPair<RagChunk, float>> searchScored(const QVector<float>& queryEmb, int k = 5) const;

    /* ── Persistenza ────────────────────────────────────────────── */

    bool save(const QString& path) const;
    bool load(const QString& path);

    /* ── Info ───────────────────────────────────────────────────── */

    int  chunkCount() const { return m_chunks.size(); }
    int  inputDim()   const { return m_inputDim; }
    void clear()            { m_chunks.clear(); m_inputDim = 0; m_R.clear(); }

private:
    void  initMatrix(int d);
    float cosine(const QVector<float>& a, const QVector<float>& b) const;

    uint32_t                m_seed     = 20240328u;  ///< seed deterministico
    int                     m_inputDim = 0;
    QVector<QVector<float>> m_R;        ///< matrice JLT: kTargetDim × inputDim
    QVector<RagChunk>       m_chunks;
};
