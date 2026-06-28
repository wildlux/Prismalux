#pragma once
/*
 * context_compressor.h — Headroom-style context compression per agenti AI
 * =========================================================================
 * Mantiene una finestra scorrevole di messaggi recenti (role/content) e
 * comprime in background i messaggi in overflow tramite LLM (summarizeAsync).
 *
 * Uso tipico:
 *   m_ctx = new ContextCompressor(m_ai, this);
 *   m_ctx->appendPair(userMsg, assistantMsg);   // pipeline/chat singola
 *   m_ctx->appendMessage("user", obs);          // ReAct observation
 *   QJsonArray h = m_ctx->buildContext();       // → AiClient::chat(..., h, ...)
 *
 * La compressione è asincrona: buildContext() restituisce immediatamente il
 * contesto corrente (con l'eventuale summary già disponibile).
 */
#include <QObject>
#include <QString>
#include <QJsonArray>
#include <QJsonObject>
#include <QVector>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class AiClient;

class ContextCompressor : public QObject {
    Q_OBJECT

public:
    explicit ContextCompressor(AiClient* ai, QObject* parent = nullptr);

    /* Numero massimo di messaggi raw da tenere senza comprimere.
       Default: 2 * kChatMaxTurns (letto da QSettings al costruttore).
       Un "turno" = 1 user + 1 assistant = 2 messaggi. */
    void setMaxRecentMessages(int n);
    int  maxRecentMessages() const { return m_maxMsgs; }

    /* Aggiunge un turno completo (user + assistant) — per chat/pipeline */
    void appendPair(const QString& user, const QString& assistant);

    /* Aggiunge un singolo messaggio — per ReAct (observation, step agente) */
    void appendMessage(const QString& role, const QString& content);

    /* Restituisce il contesto da passare come history ad AiClient::chat().
     * Struttura: [summary_user, summary_assistant] (se presente) + m_recent. */
    QJsonArray buildContext() const;

    int  messageCount() const { return m_recent.size(); }
    bool hasSummary()   const { return !m_summary.isEmpty(); }
    void clear();

    /* Serializzazione — salva m_recent + m_summary */
    QJsonObject toJson()   const;
    void        fromJson(const QJsonObject&);

    /* Carica da QJsonArray raw (es. m_autoHistory salvato precedentemente) */
    void fromJsonArray(const QJsonArray& arr);

signals:
    /* Emesso quando il summary asincrono è pronto — usato per UI opzionale */
    void summaryUpdated();

private:
    void compressIfNeeded();
    void summarizeAsync(const QJsonArray& overflow);

    AiClient*              m_ai;
    QNetworkAccessManager* m_nam             = nullptr;
    QString                m_pendingFallback;

    QJsonArray             m_recent;   ///< ultimi m_maxMsgs messaggi
    QString                m_summary;  ///< testo riassuntivo dei messaggi compressi
    int                    m_maxMsgs  = 6; ///< default: 3 turni × 2 messaggi

private slots:
    void onSummaryFinished();
};
