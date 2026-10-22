#include "lan_server.h"
#include "quiz_room.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QSet>
#include <QUrlQuery>

#include <algorithm>

/* ══════════════════════════════════════════════════════════════
   Quiz live (stile Kahoot) — /quiz + /api/quiz/*
   ──────────────────────────────────────────────────────────────
   Auth:
     • host  (webchat)  → Bearer token, azioni protette da key
     • player (telefono) → cookie p_session ottenuto con /quiz?token=
   La pagina /quiz è pubblica come guscio; i dati passano dalle API.
   ══════════════════════════════════════════════════════════════ */

namespace {

/* Banca domande embedded in qrc (generata da Tools/docker/quiz_ccna.db +
   domande dell'allenamento informatica + domande Prismalux). */
const QJsonArray& quizBank()
{
    static QJsonArray bank;
    static bool loaded = false;
    if (loaded)
        return bank;
    loaded = true;
    QFile f(":/lan/quiz_bank.json");
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "[Quiz] quiz_bank.json non trovato nelle risorse";
        return bank;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    bank = doc.object().value(QStringLiteral("questions")).toArray();
    return bank;
}

const QStringList& quizMaterie()
{
    static QStringList materie;
    if (!materie.isEmpty())
        return materie;
    QSet<QString> set;
    for (const QJsonValue& v : quizBank())
        set.insert(v.toObject().value(QStringLiteral("materia")).toString());
    materie = set.values();
    std::sort(materie.begin(), materie.end());
    return materie;
}

} // namespace

void LanServer::handleQuizPage(Session& s)
{
    QFile f(":/lan/quiz.html");
    if (!f.open(QIODevice::ReadOnly)) {
        sendError(s.socket, 500, "quiz.html not found in resources");
        return;
    }
    const QByteArray html = f.readAll();

    QByteArray resp = httpOkHeader("text/html; charset=utf-8");
    resp += "Cache-Control: no-cache, no-store, must-revalidate\r\n";
    resp += "Pragma: no-cache\r\n";
    resp += "Content-Security-Policy: default-src 'self'; "
            "script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; "
            "img-src 'self' data:; connect-src 'self'; frame-ancestors 'none'\r\n";
    resp += "Connection: close\r\n";
    resp += "\r\n";
    s.socket->write(resp);
    s.socket->write(html);
    while (s.socket->bytesToWrite() > 0
           && s.socket->state() == QAbstractSocket::ConnectedState)
        s.socket->waitForBytesWritten(5000);
    s.socket->disconnectFromHost();
    s.socket->waitForDisconnected(5000);
}

void LanServer::handleQuizApi(Session& s)
{
    if (!m_quiz) {
        sendError(s.socket, 500, "Quiz non inizializzato");
        return;
    }

    const QString sub = s.path.mid(QStringLiteral("/api/quiz/").size());
    const QJsonObject body = QJsonDocument::fromJson(s.body).object();
    /* I form HTML codificano lo spazio come '+': normalizziamolo prima di
       QUrlQuery, che da solo decodifica solo %XX. */
    QString rawQuery = s.queryString;
    rawQuery.replace(QLatin1Char('+'), QLatin1Char(' '));
    const QUrlQuery query(rawQuery);

    auto sendObj = [this, &s](const QJsonObject& o) {
        sendJson(s.socket, QJsonDocument(o).toJson(QJsonDocument::Compact));
    };
    auto fail = [this, &s](int code, const QString& msg) {
        sendError(s.socket, code, msg);
    };

    /* ── banca domande (host + allenamento autenticato) ── */
    if (sub == QStringLiteral("bank") && s.method == "GET") {
        const QString materia = query.queryItemValue(QStringLiteral("materia"));
        int n = query.queryItemValue(QStringLiteral("n")).toInt();
        if (n <= 0 || n > 30)
            n = 10;

        QList<QJsonObject> pool;
        for (const QJsonValue& v : quizBank()) {
            const QJsonObject q = v.toObject();
            if (!materia.isEmpty()
                && q.value(QStringLiteral("materia")).toString() != materia)
                continue;
            pool.append(q);
        }
        std::shuffle(pool.begin(), pool.end(), *QRandomGenerator::global());
        QJsonArray picked;
        for (int i = 0; i < pool.size() && i < n; ++i)
            picked.append(pool.at(i));

        QJsonArray materie;
        for (const QString& m : quizMaterie()) {
            int count = 0;
            for (const QJsonValue& v : quizBank())
                if (v.toObject().value(QStringLiteral("materia")).toString() == m)
                    ++count;
            QJsonObject mo;
            mo[QStringLiteral("nome")] = m;
            mo[QStringLiteral("n")]    = count;
            materie.append(mo);
        }

        QJsonObject o;
        o[QStringLiteral("ok")]       = true;
        o[QStringLiteral("materie")]  = materie;
        o[QStringLiteral("questions")] = picked;
        sendObj(o);
        return;
    }

    /* ── host: crea stanza ── */
    if (sub == QStringLiteral("create") && s.method == "POST") {
        QString err;
        const QJsonObject o = m_quiz->create(body, &err);
        if (o.isEmpty()) { fail(400, err); return; }
        sendObj(o);
        return;
    }

    /* ── host: stato (polling) ── */
    if (sub == QStringLiteral("host") && s.method == "GET") {
        QString err;
        const QJsonObject o = m_quiz->hostState(
            query.queryItemValue(QStringLiteral("code")),
            query.queryItemValue(QStringLiteral("key")), &err);
        if (o.isEmpty()) { fail(404, err); return; }
        sendObj(o);
        return;
    }

    /* ── host: start / reveal / next / end ── */
    if ((sub == QStringLiteral("start") || sub == QStringLiteral("reveal")
         || sub == QStringLiteral("next") || sub == QStringLiteral("end"))
        && s.method == "POST") {
        QString err;
        const QJsonObject o = m_quiz->hostAction(sub, body, &err);
        if (o.isEmpty()) { fail(400, err); return; }
        sendObj(o);
        return;
    }

    /* ── giocatore: entra ── */
    if (sub == QStringLiteral("join") && s.method == "POST") {
        QString err;
        const QJsonObject o = m_quiz->join(body, &err);
        if (o.isEmpty()) { fail(400, err); return; }
        sendObj(o);
        return;
    }

    /* ── giocatore: stato (polling) ── */
    if (sub == QStringLiteral("state") && s.method == "GET") {
        QString err;
        const QJsonObject o = m_quiz->playerState(
            query.queryItemValue(QStringLiteral("code")),
            query.queryItemValue(QStringLiteral("player")), &err);
        if (o.isEmpty()) { fail(404, err); return; }
        sendObj(o);
        return;
    }

    /* ── giocatore: esce dalla lobby ── */
    if (sub == QStringLiteral("leave") && s.method == "POST") {
        sendObj(m_quiz->leave(body));
        return;
    }

    /* ── giocatore: risposta ── */
    if (sub == QStringLiteral("answer") && s.method == "POST") {
        QString err;
        const QJsonObject o = m_quiz->answer(body, &err);
        if (o.isEmpty()) { fail(400, err); return; }
        sendObj(o);
        return;
    }

    fail(404, "Endpoint quiz sconosciuto");
}
