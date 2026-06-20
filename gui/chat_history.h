#pragma once
/*
 * chat_history.h — Storico chat Prismalux
 * =========================================
 * Salva/carica sessioni di chat in ~/.prismalux_chats/
 * Ogni sessione = un file JSON con titolo, timestamp e messaggi.
 */
#include <QString>
#include <QVector>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QPair>
#include <QDir>

/* Singolo messaggio nella sessione */
struct ChatMessage {
    QString  role;       ///< "user" | "pipeline" | "system"
    QString  content;    ///< testo completo
    QDateTime timestamp;
};

/* Una sessione di chat completa */
struct ChatSession {
    QString              id;         ///< timestamp in ms come stringa (chiave univoca)
    QString              title;      ///< prime 40 lettere del primo task
    QDateTime            createdAt;
    QVector<ChatMessage> messages;
};

/* ── ChatHistory ────────────────────────────────────────────── */
class ChatHistory {
public:
    explicit ChatHistory();

    /** Lista sessioni, ordine decrescente per data (più recenti prima) */
    QVector<ChatSession> list() const;

    /** Crea nuova sessione vuota; ritorna l'id */
    QString newSession(const QString& firstTask);

    /** Aggiunge/aggiorna il log completo della sessione */
    void saveLog(const QString& sessionId, const QString& logHtml);

    /** Carica HTML log di una sessione */
    QString loadLog(const QString& sessionId) const;

    /** Salva la history multi-turno dell'agente autonomo (ReAct) */
    void saveAutoHistory(const QString& sessionId, const QJsonArray& history);

    /** Carica la history ReAct — vuota se non presente */
    QJsonArray loadAutoHistory(const QString& sessionId) const;

    /** Salva le mappe bubbleTexts e codeBlocks per la sessione.
     *  bubbleTexts : indice → testo plain (per TTS/copia legacy)
     *  codeBlocks  : indice → {lang, codice} (per code:copy/save) */
    void saveMaps(const QString& sessionId,
                  const QMap<int,QString>& bubbleTexts,
                  const QMap<int,QPair<QString,QString>>& codeBlocks);

    /** Carica le mappe — entrambe vuote se non presenti */
    void loadMaps(const QString& sessionId,
                  QMap<int,QString>& bubbleTexts,
                  QMap<int,QPair<QString,QString>>& codeBlocks) const;

    /** Elimina sessione */
    void remove(const QString& sessionId);

private:
    QDir    m_dir;
    QString sessionPath(const QString& id) const;
};
