#pragma once
/* ══════════════════════════════════════════════════════════════
   QuizRoom — stanza quiz live in memoria (stile Kahoot)
   ──────────────────────────────────────────────────────────────
   Una sola stanza attiva per processo. Il docente (webchat)
   crea la stanza, i telefoni entrano con codice + nome e
   rispondono; lo stato si legge in polling.

   Nessuna persistenza: alla chiusura dell'app la stanza sparisce.
   Le domande arrivano dal host (banca dati) e restano in memoria.
   ══════════════════════════════════════════════════════════════ */
#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QString>

class QuizRoom : public QObject {
    Q_OBJECT
public:
    struct Answer {
        int    choice   = -1;
        qint64 elapsedMs = 0;
        int    points   = 0;
    };
    struct Player {
        QString id;
        QString name;
        int     score   = 0;
        int     correct = 0;
        qint64  joinedAt = 0;
        QMap<int, Answer> answers;   ///< indice domanda → risposta
    };

    explicit QuizRoom(QObject* parent = nullptr);

    bool active() const { return !m_code.isEmpty(); }
    void clear();

    /* ── host (webchat, Bearer) ── */
    QJsonObject create(const QJsonObject& req, QString* err);
    QJsonObject hostState(const QString& code, const QString& key, QString* err) const;
    QJsonObject hostAction(const QString& action, const QJsonObject& req, QString* err);

    /* ── giocatori (telefono, cookie di sessione) ── */
    QJsonObject join(const QJsonObject& req, QString* err);
    QJsonObject playerState(const QString& code, const QString& playerId, QString* err) const;
    QJsonObject answer(const QJsonObject& req, QString* err);
    QJsonObject leave(const QJsonObject& req);

private:
    /* "subito" | "fine" | "entrambe" */
    bool revealImmediate() const { return m_reveal != "fine"; }
    bool revealFinal()     const { return m_reveal != "subito"; }
    int  total()           const { return m_questions.size(); }
    int  scoringMs()       const { return qMax(5, m_seconds) * 1000; }

    QString newCode() const;
    static QString newId(int bytes);
    static QString cleanName(const QString& raw);
    QString uniqueName(const QString& base) const;
    QJsonObject leaderboard() const;
    QJsonArray  recapFor(const Player& p) const;
    QJsonObject questionPublic() const;
    QJsonObject counts() const;
    const Player* player(const QString& id) const;

    QString     m_code;
    QString     m_key;
    QString     m_title;
    QJsonArray  m_questions;
    QString     m_reveal = "entrambe";
    int         m_seconds = 20;
    QString     m_state = "lobby";   ///< lobby | question | reveal | ended
    int         m_index = -1;
    qint64      m_qStart = 0;
    qint64      m_createdAt = 0;
    QMap<QString, Player> m_players; ///< playerId → player
};
