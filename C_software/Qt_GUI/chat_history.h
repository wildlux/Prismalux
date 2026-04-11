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

    /** Elimina sessione */
    void remove(const QString& sessionId);

private:
    QDir    m_dir;
    QString sessionPath(const QString& id) const;
};
