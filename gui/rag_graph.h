#pragma once
#include <QObject>
#include <QString>
#include <QVector>
#include <QMap>
#include "graph_memory.h"
#include "ai_client.h"

/* ══════════════════════════════════════════════════════════════
   RagGraph — Grafo della conoscenza auto-costruito dai documenti RAG

   Flusso:
     1. scanDirectory(dir)        → lista file da analizzare
     2. extractFromChunk(text)    → LLM estrae entità + relazioni JSON
     3. ingestAll()               → processa tutti i file, aggiorna GraphMemory
     4. search(query)             → combina ricerca testuale sul grafo
     5. toDot()                   → Graphviz DOT per visualizzazione

   Formato risposta LLM (extractFromChunk):
   {
     "entities": [
       {"label":"Qt6","type":"framework","importance":0.8},
       {"label":"KaTeX","type":"libreria","importance":0.7}
     ],
     "relations": [
       {"from":"Qt6","to":"C++","type":"usa","weight":0.9},
       {"from":"Prismalux","to":"Qt6","type":"basato_su","weight":1.0}
     ]
   }

   I tipi di nodo riconosciuti:
     framework | libreria | concetto | persona | luogo |
     organizzazione | formula | algoritmo | documento | altro
   ══════════════════════════════════════════════════════════════ */

struct RagGraphChunk {
    QString text;       ///< testo del chunk
    QString source;     ///< nome file sorgente
    int     chunkIdx;   ///< indice del chunk nel file
};

struct RagGraphStats {
    int totalFiles   = 0;
    int processedFiles = 0;
    int totalEntities  = 0;
    int totalRelations = 0;
    bool running = false;
};

class RagGraph : public QObject {
    Q_OBJECT
public:
    explicit RagGraph(AiClient* ai, GraphMemory* gm, QObject* parent = nullptr);

    /* ── Configurazione ─────────────────────────────────────── */
    void setChunkSize(int chars)       { m_chunkSize = chars; }
    void setMaxChunksPerFile(int n)    { m_maxChunksPerFile = n; }
    void setMaxEntitiesPerChunk(int n) { m_maxEntities = n; }

    /* ── Indicizzazione ─────────────────────────────────────── */

    /** Scansiona una directory e aggiunge i file alla coda. */
    void addDirectory(const QString& dir);

    /** Aggiunge un singolo file alla coda. */
    void addFile(const QString& path, const QString& label = {});

    /** Avvia l'elaborazione asincrona di tutti i file in coda.
     *  Emette progressUpdated() durante l'elaborazione e
     *  finished() quando completa. */
    void startIngest();

    /** Ferma l'elaborazione. */
    void stopIngest();

    bool isRunning() const { return m_stats.running; }

    /* ── Query ──────────────────────────────────────────────── */

    /** Cerca nel grafo (textual) + restituisce chunk sorgente. */
    QVector<RagGraphChunk> searchChunks(const QString& query, int limit = 10) const;

    /** Nodi del grafo che corrispondono alla query. */
    QVector<GmNode> searchNodes(const QString& query, int limit = 20) const;

    /* ── Statistiche ─────────────────────────────────────────── */
    const RagGraphStats& stats() const { return m_stats; }

signals:
    /** Progresso elaborazione: file corrente, totale file, entità trovate. */
    void progressUpdated(int current, int total, const QString& currentFile);
    /** Elaborazione completata. */
    void finished(const RagGraphStats& stats);
    /** Errore su un file. */
    void fileError(const QString& file, const QString& error);
    /** File copiato automaticamente in RAG/ per persistenza. */
    void fileCopied(const QString& filename, const QString& destPath);

private:
    void processNextFile();
    void extractEntities(const QString& text, const QString& source, int chunkIdx);
    void onLlmToken(const QString& tok);
    void onLlmFinished(const QString& full);
    void onLlmError(const QString& msg);
    void parseAndStore(const QString& jsonText, const QString& source);

    AiClient*    m_ai = nullptr;
    GraphMemory* m_gm = nullptr;

    QVector<QPair<QString,QString>> m_fileQueue;   ///< (path, label)
    int m_currentFileIdx = 0;

    QString m_accumJson;    ///< accumulatore risposta LLM
    QObject* m_llmHolder = nullptr;

    /* Config */
    int m_chunkSize          = 600;
    int m_maxChunksPerFile   = 8;
    int m_maxEntities        = 12;

    RagGraphStats m_stats;

    /* Mappa nodeId→testo chunk sorgente per searchChunks() */
    QMap<QString, RagGraphChunk> m_chunkMap;   ///< nodeId → chunk
};
