#pragma once
/* ══════════════════════════════════════════════════════════════
   chat_history.h — Persistenza cronologia chat (SQLite via Qt6::Sql)
   ══════════════════════════════════════════════════════════════
   Schema tabella:
     chats(id, mode, model, task, response, timestamp, images)
   - images: testo JSON con lista base64 (attualmente vuoto se nessuna immagine)
   - timestamp: formato ISO 8601 UTC
   ══════════════════════════════════════════════════════════════ */
#include <QObject>
#include <QString>
#include <QVector>
#include <QDateTime>
#include <QSqlDatabase>

struct ChatEntry {
    int     id        = 0;
    QString mode;       /* "Pipeline", "Byzantino", "MathTheory", "Singolo" */
    QString model;      /* nome modello usato */
    QString task;       /* prompt utente originale */
    QString response;   /* output HTML finale (dal m_log toHtml) */
    QString timestamp;  /* ISO 8601 UTC */
    QString images;     /* JSON array base64 — vuoto se no immagini */
};

class ChatHistoryDb : public QObject {
    Q_OBJECT
public:
    explicit ChatHistoryDb(QObject* parent = nullptr);
    ~ChatHistoryDb() override;

    /** Salva una nuova chat; ritorna l'id assegnato (-1 se errore). */
    int  saveChat(const QString& mode, const QString& model,
                  const QString& task, const QString& response,
                  const QString& images = "");

    /** Carica tutte le chat ordinate per timestamp DESC (più recenti prima). */
    QVector<ChatEntry> loadAll() const;

    /** Carica una singola chat per id; ritorna ChatEntry con id=0 se non trovata. */
    ChatEntry loadChat(int id) const;

    /** Elimina la chat con l'id dato; ritorna true se almeno 1 riga eliminata. */
    bool deleteChat(int id);

    /** True se il db è aperto e pronto. */
    bool isReady() const { return m_ready; }

private:
    QSqlDatabase m_db;
    bool         m_ready = false;
    QString      m_connName;

    void initSchema();
};
