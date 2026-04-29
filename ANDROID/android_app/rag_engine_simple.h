#pragma once
#include <QObject>
#include <QStringList>
#include <QVector>

/* ══════════════════════════════════════════════════════════════
   RagEngineSimple — RAG leggero per mobile senza embedding.

   Usa TF-IDF keyword scoring invece di vettori embedding:
   - Nessuna dipendenza da Ollama per l'indicizzazione
   - Nessuna RAM extra per la matrice JLT
   - Latenza zero: ricerca in <1ms su 1000 chunk
   - Sufficiente per RAG su documenti di testo strutturati

   Trade-off:
   - Meno preciso di cosine similarity su embedding semantici
   - Non capisce sinonimi o contesto semantico
   - Ideale per RAG su FAQ, manuali, note personali

   Flusso:
     1. addDocument(titolo, testo)  — divide in chunk e indicizza
     2. load(path)                  — carica da file .txt o cartella
     3. search(query, k)            — top-k chunk per score TF-IDF
     4. searchForPrompt(query, k)   — come search() ma con troncatura 800 char
   ══════════════════════════════════════════════════════════════ */
class RagEngineSimple : public QObject {
    Q_OBJECT
public:
    /* Lunghezza massima chunk iniettato nel prompt */
    static constexpr int kMaxChunkLen  = 800;
    /* Dimensione target di ogni chunk in caratteri */
    static constexpr int kChunkSize    = 400;
    /* Sovrapposizione tra chunk consecutivi (sliding window) */
    static constexpr int kChunkOverlap = 80;

    explicit RagEngineSimple(QObject* parent = nullptr);

    /* ── Indicizzazione ── */

    /** Aggiunge un documento: lo divide in chunk sovrapposti e indicizza. */
    void addDocument(const QString& title, const QString& text);

    /** Carica un file .txt o tutti i .txt di una cartella. */
    bool load(const QString& pathOrDir);

    void clear();

    int chunkCount() const { return m_chunks.size(); }

    /* ── Query ── */

    /** Top-k chunk per keyword scoring.
     *  Ritorna i testi grezzi (non troncati). */
    QStringList search(const QString& query, int k = 4) const;

    /** Come search() ma tronca ogni chunk a kMaxChunkLen caratteri.
     *  Da usare direttamente per costruire il system prompt RAG. */
    QStringList searchForPrompt(const QString& query, int k = 4) const;

signals:
    /** Emesso dopo load() con il numero di chunk caricati. */
    void loaded(int chunkCount);

private:
    struct Chunk {
        QString source;   ///< nome file / titolo documento
        QString text;     ///< testo del chunk
        QStringList terms; ///< termini normalizzati pre-computati
    };

    /* Normalizza un testo in lista di termini (lowercase, stopword removal) */
    static QStringList tokenize(const QString& text);

    /* Score TF-IDF semplificato: conta quanti termini query appaiono nel chunk */
    static float score(const QStringList& queryTerms, const Chunk& chunk);

    QVector<Chunk> m_chunks;
};
