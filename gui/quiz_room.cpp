#include "quiz_room.h"

#include <QDateTime>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSet>
#include <QtMath>

#include <algorithm>
#include <climits>

namespace {
constexpr int kMaxQuestions = 50;
constexpr int kMaxPlayers   = 80;
constexpr int kGraceMs      = 750;   ///< tolleranza rete sullo scadere del tempo

QString normalizedCode(const QString& raw)
{
    return raw.trimmed().toUpper();
}
} // namespace

QuizRoom::QuizRoom(QObject* parent) : QObject(parent) {}

void QuizRoom::clear()
{
    m_code.clear();
    m_key.clear();
    m_title.clear();
    m_questions = QJsonArray();
    m_reveal    = QStringLiteral("entrambe");
    m_seconds   = 20;
    m_state     = QStringLiteral("lobby");
    m_index     = -1;
    m_qStart    = 0;
    m_createdAt = 0;
    m_players.clear();
}

QString QuizRoom::newCode() const
{
    static const QString alphabet = QStringLiteral("ABCDEFGHJKLMNPQRSTUVWXYZ23456789");
    QString out;
    for (int i = 0; i < 5; ++i)
        out += alphabet.at(QRandomGenerator::global()->bounded(alphabet.size()));
    return out;
}

QString QuizRoom::newId(int bytes)
{
    QString out;
    for (int i = 0; i < bytes; ++i) {
        out += QStringLiteral("%1").arg(QRandomGenerator::system()->generate() & 0xFF,
                                        2, 16, QLatin1Char('0'));
    }
    return out;
}

QString QuizRoom::cleanName(const QString& raw)
{
    QString s = raw;
    s.remove(QRegularExpression(QStringLiteral("[\\x00-\\x1F\\x7F<>]")));
    s = s.simplified().trimmed();
    if (s.isEmpty())
        s = QStringLiteral("Giocatore");
    return s.left(18);
}

QString QuizRoom::uniqueName(const QString& base) const
{
    QSet<QString> taken;
    for (const Player& p : m_players)
        taken.insert(p.name.toLower());
    if (!taken.contains(base.toLower()))
        return base;
    for (int i = 2; i < 100; ++i) {
        QString candidate = QStringLiteral("%1 (%2)").arg(base).arg(i);
        if (!taken.contains(candidate.toLower()))
            return candidate;
    }
    return base;
}

QJsonObject QuizRoom::create(const QJsonObject& req, QString* err)
{
    QJsonArray in = req.value(QStringLiteral("questions")).toArray();
    QJsonArray valid;
    for (const QJsonValue& v : in) {
        const QJsonObject q = v.toObject();
        const QJsonArray opts = q.value(QStringLiteral("opts")).toArray();
        const int correct = q.value(QStringLiteral("correct")).toInt(-1);
        if (q.value(QStringLiteral("q")).toString().trimmed().isEmpty())
            continue;
        if (opts.size() != 4 || correct < 0 || correct > 3)
            continue;
        QJsonArray cleaned;
        bool ok = true;
        for (const QJsonValue& o : opts) {
            const QString s = o.toString().trimmed();
            if (s.isEmpty()) { ok = false; break; }
            cleaned.append(s);
        }
        if (!ok)
            continue;
        QJsonObject item;
        item[QStringLiteral("q")]       = q.value(QStringLiteral("q")).toString().trimmed();
        item[QStringLiteral("opts")]    = cleaned;
        item[QStringLiteral("correct")] = correct;
        item[QStringLiteral("exp")]     = q.value(QStringLiteral("exp")).toString().trimmed();
        valid.append(item);
        if (valid.size() >= kMaxQuestions)
            break;
    }
    if (valid.isEmpty()) {
        if (err) *err = QStringLiteral("Nessuna domanda valida (servono q, 4 opts e correct 0-3)");
        return {};
    }

    clear();
    m_code    = newCode();
    m_key     = newId(16);
    m_title   = req.value(QStringLiteral("title")).toString().trimmed().left(60);
    if (m_title.isEmpty())
        m_title = QStringLiteral("Sfida in classe");
    m_questions = valid;
    m_reveal    = req.value(QStringLiteral("reveal")).toString();
    if (m_reveal != "subito" && m_reveal != "fine" && m_reveal != "entrambe")
        m_reveal = QStringLiteral("entrambe");
    m_seconds   = qBound(5, req.value(QStringLiteral("seconds")).toInt(20), 120);
    m_state     = QStringLiteral("lobby");
    m_index     = -1;
    m_createdAt = QDateTime::currentMSecsSinceEpoch();

    QJsonObject o;
    o[QStringLiteral("ok")]        = true;
    o[QStringLiteral("code")]      = m_code;
    o[QStringLiteral("key")]       = m_key;
    o[QStringLiteral("title")]     = m_title;
    o[QStringLiteral("total")]     = total();
    o[QStringLiteral("reveal")]    = m_reveal;
    o[QStringLiteral("seconds")]   = m_seconds;
    return o;
}

const QuizRoom::Player* QuizRoom::player(const QString& id) const
{
    auto it = m_players.constFind(id);
    return it == m_players.constEnd() ? nullptr : &it.value();
}

QJsonObject QuizRoom::leaderboard() const
{
    QList<Player> list = m_players.values();
    std::sort(list.begin(), list.end(), [](const Player& a, const Player& b) {
        if (a.score != b.score) return a.score > b.score;
        if (a.correct != b.correct) return a.correct > b.correct;
        return a.name.toLower() < b.name.toLower();
    });
    QJsonArray arr;
    int rank = 0;
    int lastScore = INT_MIN;
    for (const Player& p : list) {
        if (p.score != lastScore) { ++rank; lastScore = p.score; }
        QJsonObject o;
        o[QStringLiteral("rank")]    = rank;
        o[QStringLiteral("name")]    = p.name;
        o[QStringLiteral("score")]   = p.score;
        o[QStringLiteral("correct")] = p.correct;
        arr.append(o);
    }
    QJsonObject out;
    out[QStringLiteral("players")] = arr;
    return out;
}

QJsonObject QuizRoom::questionPublic() const
{
    if (m_index < 0 || m_index >= total())
        return {};
    const QJsonObject q = m_questions.at(m_index).toObject();
    QJsonObject o;
    o[QStringLiteral("q")]    = q.value(QStringLiteral("q"));
    o[QStringLiteral("opts")] = q.value(QStringLiteral("opts"));
    return o;
}

QJsonObject QuizRoom::counts() const
{
    QJsonArray counts;
    if (m_index < 0 || m_index >= total())
        return QJsonObject{{QStringLiteral("counts"), counts}};
    const int nOpts = m_questions.at(m_index).toObject()
                          .value(QStringLiteral("opts")).toArray().size();
    QVector<int> c(nOpts, 0);
    for (const Player& p : m_players) {
        auto it = p.answers.constFind(m_index);
        if (it != p.answers.constEnd() && it->choice >= 0 && it->choice < nOpts)
            c[it->choice]++;
    }
    for (int v : c)
        counts.append(v);
    return QJsonObject{{QStringLiteral("counts"), counts}};
}

QJsonArray QuizRoom::recapFor(const Player& p) const
{
    QJsonArray arr;
    for (int i = 0; i < total(); ++i) {
        const QJsonObject q = m_questions.at(i).toObject();
        auto it = p.answers.constFind(i);
        const int choice = it == p.answers.constEnd() ? -1 : it->choice;
        QJsonObject o;
        o[QStringLiteral("q")]       = q.value(QStringLiteral("q"));
        o[QStringLiteral("opts")]    = q.value(QStringLiteral("opts"));
        o[QStringLiteral("correct")] = q.value(QStringLiteral("correct"));
        o[QStringLiteral("exp")]     = q.value(QStringLiteral("exp"));
        o[QStringLiteral("your")]    = choice;
        o[QStringLiteral("ok")]      = (choice == q.value(QStringLiteral("correct")).toInt());
        arr.append(o);
    }
    return arr;
}

QJsonObject QuizRoom::hostState(const QString& code, const QString& key, QString* err) const
{
    if (!active() || normalizedCode(code) != m_code) {
        if (err) *err = QStringLiteral("Stanza non trovata");
        return {};
    }
    if (key != m_key) {
        if (err) *err = QStringLiteral("Chiave host non valida");
        return {};
    }

    QJsonObject o;
    o[QStringLiteral("code")]      = m_code;
    o[QStringLiteral("title")]     = m_title;
    o[QStringLiteral("state")]     = m_state;
    o[QStringLiteral("index")]     = m_index;
    o[QStringLiteral("total")]     = total();
    o[QStringLiteral("reveal")]    = m_reveal;
    o[QStringLiteral("seconds")]   = m_seconds;
    o[QStringLiteral("startedAt")] = m_qStart;
    o[QStringLiteral("serverNow")] = QDateTime::currentMSecsSinceEpoch();
    o[QStringLiteral("count")]     = m_players.size();

    QJsonArray players;
    for (const Player& p : m_players) {
        QJsonObject po;
        po[QStringLiteral("name")]     = p.name;
        po[QStringLiteral("score")]    = p.score;
        po[QStringLiteral("correct")]  = p.correct;
        po[QStringLiteral("answered")] = (m_index >= 0 && p.answers.contains(m_index));
        players.append(po);
    }
    o[QStringLiteral("players")] = players;
    o[QStringLiteral("counts")]  = counts().value(QStringLiteral("counts"));

    if (m_index >= 0 && m_index < total()) {
        const QJsonObject q = m_questions.at(m_index).toObject();
        o[QStringLiteral("question")] = questionPublic();
        o[QStringLiteral("correct")]  = q.value(QStringLiteral("correct"));
        o[QStringLiteral("exp")]      = q.value(QStringLiteral("exp"));
    }
    if (m_state == QStringLiteral("ended"))
        o[QStringLiteral("leaderboard")] = leaderboard().value(QStringLiteral("players"));
    return o;
}

QJsonObject QuizRoom::hostAction(const QString& action, const QJsonObject& req, QString* err)
{
    if (!active() || normalizedCode(req.value(QStringLiteral("code")).toString()) != m_code) {
        if (err) *err = QStringLiteral("Stanza non trovata");
        return {};
    }
    if (req.value(QStringLiteral("key")).toString() != m_key) {
        if (err) *err = QStringLiteral("Chiave host non valida");
        return {};
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (action == QStringLiteral("start")) {
        if (m_state != QStringLiteral("lobby")) {
            if (err) *err = QStringLiteral("Partita già avviata");
            return {};
        }
        m_state  = QStringLiteral("question");
        m_index  = 0;
        m_qStart = now;
    } else if (action == QStringLiteral("reveal")) {
        if (m_state != QStringLiteral("question")) {
            if (err) *err = QStringLiteral("Nessuna domanda aperta");
            return {};
        }
        m_state = QStringLiteral("reveal");
    } else if (action == QStringLiteral("next")) {
        if (m_state != QStringLiteral("question") && m_state != QStringLiteral("reveal")) {
            if (err) *err = QStringLiteral("Partita non in corso");
            return {};
        }
        if (m_index + 1 >= total()) {
            m_state = QStringLiteral("ended");
        } else {
            ++m_index;
            m_state  = QStringLiteral("question");
            m_qStart = now;
        }
    } else if (action == QStringLiteral("end")) {
        m_state = QStringLiteral("ended");
    } else {
        if (err) *err = QStringLiteral("Azione sconosciuta");
        return {};
    }

    QJsonObject o = hostState(m_code, m_key, err);
    o[QStringLiteral("ok")] = true;
    return o;
}

QJsonObject QuizRoom::join(const QJsonObject& req, QString* err)
{
    if (!active() || normalizedCode(req.value(QStringLiteral("code")).toString()) != m_code) {
        if (err) *err = QStringLiteral("Codice stanza non valido");
        return {};
    }
    if (m_state != QStringLiteral("lobby")) {
        if (err) *err = QStringLiteral("La partita è già iniziata: chiedi il nuovo codice");
        return {};
    }
    if (m_players.size() >= kMaxPlayers) {
        if (err) *err = QStringLiteral("Stanza piena");
        return {};
    }

    Player p;
    p.id       = newId(12);
    p.name     = uniqueName(cleanName(req.value(QStringLiteral("name")).toString()));
    p.joinedAt = QDateTime::currentMSecsSinceEpoch();
    m_players.insert(p.id, p);

    QJsonObject o;
    o[QStringLiteral("ok")]       = true;
    o[QStringLiteral("playerId")] = p.id;
    o[QStringLiteral("name")]     = p.name;
    o[QStringLiteral("code")]     = m_code;
    o[QStringLiteral("title")]    = m_title;
    o[QStringLiteral("state")]    = m_state;
    return o;
}

QJsonObject QuizRoom::playerState(const QString& code, const QString& playerId, QString* err) const
{
    if (!active() || normalizedCode(code) != m_code) {
        if (err) *err = QStringLiteral("Stanza non trovata");
        return {};
    }
    const Player* p = player(playerId);
    if (!p) {
        if (err) *err = QStringLiteral("Giocatore non riconosciuto: rientra con il codice");
        return {};
    }

    QJsonObject you;
    you[QStringLiteral("name")]    = p->name;
    you[QStringLiteral("score")]   = p->score;
    you[QStringLiteral("correct")] = p->correct;

    QJsonObject o;
    o[QStringLiteral("code")]      = m_code;
    o[QStringLiteral("title")]     = m_title;
    o[QStringLiteral("state")]     = m_state;
    o[QStringLiteral("index")]     = m_index;
    o[QStringLiteral("total")]     = total();
    o[QStringLiteral("reveal")]    = m_reveal;
    o[QStringLiteral("seconds")]   = m_seconds;
    o[QStringLiteral("startedAt")] = m_qStart;
    o[QStringLiteral("serverNow")] = QDateTime::currentMSecsSinceEpoch();
    o[QStringLiteral("you")]       = you;

    if (m_state == QStringLiteral("lobby")) {
        QJsonArray names;
        for (const Player& pl : m_players)
            names.append(pl.name);
        o[QStringLiteral("players")] = names;
        o[QStringLiteral("count")]   = names.size();
        return o;
    }

    if (m_state == QStringLiteral("question") || m_state == QStringLiteral("reveal")) {
        const QJsonObject q = m_questions.at(m_index).toObject();
        o[QStringLiteral("question")] = questionPublic();
        o[QStringLiteral("counts")]   = counts().value(QStringLiteral("counts"));

        const bool answered = p->answers.contains(m_index);
        o[QStringLiteral("answered")] = answered;
        if (answered) {
            const Answer& a = p->answers[m_index];
            QJsonObject r;
            r[QStringLiteral("choice")] = a.choice;
            r[QStringLiteral("points")] = a.points;
            if (revealImmediate() || m_state == QStringLiteral("reveal")) {
                r[QStringLiteral("correct")] = q.value(QStringLiteral("correct")).toInt();
                r[QStringLiteral("ok")]      = (a.choice == q.value(QStringLiteral("correct")).toInt());
                r[QStringLiteral("exp")]     = q.value(QStringLiteral("exp"));
            }
            o[QStringLiteral("result")] = r;
        }
        /* La soluzione ai telefoni solo se la modalità la prevede subito
           ("subito"/"entrambe"); con "fine" resta nascosta fino alla classifica. */
        if (m_state == QStringLiteral("reveal") && revealImmediate()) {
            o[QStringLiteral("correct")] = q.value(QStringLiteral("correct")).toInt();
            o[QStringLiteral("exp")]     = q.value(QStringLiteral("exp"));
        }
        return o;
    }

    /* ended */
    o[QStringLiteral("leaderboard")] = leaderboard().value(QStringLiteral("players"));
    if (revealFinal())
        o[QStringLiteral("recap")] = recapFor(*p);
    return o;
}

QJsonObject QuizRoom::leave(const QJsonObject& req)
{
    const QString code = normalizedCode(req.value(QStringLiteral("code")).toString());
    const QString id   = req.value(QStringLiteral("player")).toString();
    /* In lobby togliamo il giocatore (niente nomi fantasma in classifica);
       a partita iniziata lo teniamo per non falsare i punteggi. */
    if (active() && code == m_code && m_state == QStringLiteral("lobby"))
        m_players.remove(id);
    return QJsonObject{{QStringLiteral("ok"), true}};
}

QJsonObject QuizRoom::answer(const QJsonObject& req, QString* err)
{
    if (!active() || normalizedCode(req.value(QStringLiteral("code")).toString()) != m_code) {
        if (err) *err = QStringLiteral("Stanza non trovata");
        return {};
    }
    const QString playerId = req.value(QStringLiteral("player")).toString();
    auto it = m_players.find(playerId);
    if (it == m_players.end()) {
        if (err) *err = QStringLiteral("Giocatore non riconosciuto");
        return {};
    }
    if (m_state != QStringLiteral("question")) {
        if (err) *err = QStringLiteral("Domanda chiusa");
        return {};
    }
    if (it->answers.contains(m_index)) {
        if (err) *err = QStringLiteral("Hai già risposto");
        return {};
    }

    const QJsonObject q = m_questions.at(m_index).toObject();
    const int nOpts = q.value(QStringLiteral("opts")).toArray().size();
    const int choice = req.value(QStringLiteral("choice")).toInt(-1);
    if (choice < 0 || choice >= nOpts) {
        if (err) *err = QStringLiteral("Risposta non valida");
        return {};
    }

    const qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - m_qStart;
    if (elapsed > scoringMs() + kGraceMs) {
        if (err) *err = QStringLiteral("Tempo scaduto");
        return {};
    }

    const int correct = q.value(QStringLiteral("correct")).toInt();
    Answer a;
    a.choice    = choice;
    a.elapsedMs = qMax<qint64>(0, elapsed);
    if (choice == correct) {
        const double ratio = qBound(0.0, 1.0 - double(a.elapsedMs) / double(scoringMs()), 1.0);
        a.points = 500 + int(qRound(500.0 * ratio));
        it->correct++;
    }
    it->answers.insert(m_index, a);
    it->score += a.points;

    QJsonObject o;
    o[QStringLiteral("ok")]      = true;
    o[QStringLiteral("points")]  = a.points;
    o[QStringLiteral("score")]   = it->score;
    o[QStringLiteral("answered")] = true;
    if (revealImmediate()) {
        o[QStringLiteral("correct")] = correct;
        o[QStringLiteral("okAnswer")] = (choice == correct);
        o[QStringLiteral("exp")]     = q.value(QStringLiteral("exp"));
    }
    return o;
}
