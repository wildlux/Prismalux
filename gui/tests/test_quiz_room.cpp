#include <QtTest>
#include <QJsonArray>
#include <QJsonObject>

#include "quiz_room.h"

/* ══════════════════════════════════════════════════════════════
   test_quiz_room — stanza quiz live (senza rete)
   Copre: validazione domande, nomi unici, punteggio, modalità
   di reveal, classifica finale, ingressi fuori tempo.
   ══════════════════════════════════════════════════════════════ */

namespace {

QJsonObject makeQuestion(const QString& text, int correct)
{
    QJsonArray opts;
    opts.append(QStringLiteral("Risposta A"));
    opts.append(QStringLiteral("Risposta B"));
    opts.append(QStringLiteral("Risposta C"));
    opts.append(QStringLiteral("Risposta D"));
    QJsonObject q;
    q[QStringLiteral("q")]       = text;
    q[QStringLiteral("opts")]    = opts;
    q[QStringLiteral("correct")] = correct;
    q[QStringLiteral("exp")]     = QStringLiteral("Spiegazione di ") + text;
    return q;
}

QJsonObject createReq(const QString& reveal, int seconds, int n = 2)
{
    QJsonArray qs;
    for (int i = 0; i < n; ++i)
        qs.append(makeQuestion(QStringLiteral("Domanda %1").arg(i + 1), i % 4));
    QJsonObject req;
    req[QStringLiteral("title")]     = QStringLiteral("Test");
    req[QStringLiteral("questions")] = qs;
    req[QStringLiteral("reveal")]    = reveal;
    req[QStringLiteral("seconds")]   = seconds;
    return req;
}

QString joinPlayer(QuizRoom& room, const QString& code, const QString& name)
{
    QString err;
    const QJsonObject o = room.join(QJsonObject{
        {QStringLiteral("code"), code},
        {QStringLiteral("name"), name}}, &err);
    return o.value(QStringLiteral("playerId")).toString();
}

QJsonObject answerReq(const QString& code, const QString& player, int choice)
{
    return QJsonObject{
        {QStringLiteral("code"), code},
        {QStringLiteral("player"), player},
        {QStringLiteral("choice"), choice}};
}

} // namespace

class TestQuizRoom : public QObject {
    Q_OBJECT

private slots:
    /* ── creazione ── */
    void creaValida()
    {
        QuizRoom room;
        QString err;
        const QJsonObject o = room.create(createReq(QStringLiteral("entrambe"), 20), &err);
        QVERIFY2(!o.isEmpty(), qPrintable(err));
        QCOMPARE(o.value(QStringLiteral("code")).toString().size(), 5);
        QCOMPARE(o.value(QStringLiteral("key")).toString().size(), 32);
        QCOMPARE(o.value(QStringLiteral("total")).toInt(), 2);
        QVERIFY(room.active());
    }

    void rifiutaDomandeNonValide()
    {
        QuizRoom room;
        QString err;
        QJsonObject bad;
        bad[QStringLiteral("questions")] = QJsonArray{
            QJsonObject{{QStringLiteral("q"), QStringLiteral("Solo testo")}}};
        const QJsonObject o = room.create(bad, &err);
        QVERIFY(o.isEmpty());
        QVERIFY(!err.isEmpty());
        QVERIFY(!room.active());
    }

    /* ── nomi ── */
    void nomiUniciESanificati()
    {
        QuizRoom room;
        QString err;
        const QJsonObject r = room.create(createReq(QStringLiteral("entrambe"), 20), &err);
        const QString code = r.value(QStringLiteral("code")).toString();
        QVERIFY(!joinPlayer(room, code, QStringLiteral("Giulia")).isEmpty());

        QString e2;
        const QJsonObject j2 = room.join(QJsonObject{
            {QStringLiteral("code"), code},
            {QStringLiteral("name"), QStringLiteral("Giulia")}}, &e2);
        QCOMPARE(j2.value(QStringLiteral("name")).toString(), QStringLiteral("Giulia (2)"));

        QString e3;
        const QJsonObject j3 = room.join(QJsonObject{
            {QStringLiteral("code"), code},
            {QStringLiteral("name"), QStringLiteral("<b>  Marco  </b>")}}, &e3);
        QCOMPARE(j3.value(QStringLiteral("name")).toString(), QStringLiteral("b Marco /b"));
    }

    void esceDallaLobby()
    {
        QuizRoom room;
        QString err;
        const QJsonObject r = room.create(createReq(QStringLiteral("entrambe"), 20), &err);
        const QString code = r.value(QStringLiteral("code")).toString();
        const QString pid = joinPlayer(room, code, QStringLiteral("Anna"));
        room.leave(QJsonObject{{QStringLiteral("code"), code},
                               {QStringLiteral("player"), pid}});
        const QJsonObject st = room.playerState(code, pid, &err);
        QVERIFY(st.isEmpty());   /* non più riconosciuto */
    }

    /* ── gioco e punteggio ── */
    void rispostaCorrettaPunteggioMassimo()
    {
        QuizRoom room;
        QString err;
        const QJsonObject r = room.create(createReq(QStringLiteral("entrambe"), 20), &err);
        const QString code = r.value(QStringLiteral("code")).toString();
        const QString key  = r.value(QStringLiteral("key")).toString();
        const QString pid  = joinPlayer(room, code, QStringLiteral("Luca"));
        room.hostAction(QStringLiteral("start"),
                        QJsonObject{{QStringLiteral("code"), code}, {QStringLiteral("key"), key}}, &err);

        const QJsonObject a = room.answer(answerReq(code, pid, 0), &err);
        QVERIFY2(!a.isEmpty(), qPrintable(err));
        const int points = a.value(QStringLiteral("points")).toInt();
        QVERIFY(points >= 900 && points <= 1000);   /* risposta quasi immediata */
        QCOMPARE(a.value(QStringLiteral("correct")).toInt(), 0);

        const QJsonObject st = room.playerState(code, pid, &err);
        QVERIFY(st.value(QStringLiteral("answered")).toBool());
        QCOMPARE(st.value(QStringLiteral("you")).toObject()
                     .value(QStringLiteral("score")).toInt(), points);
    }

    void rispostaSbagliataZeroPunti()
    {
        QuizRoom room;
        QString err;
        const QJsonObject r = room.create(createReq(QStringLiteral("subito"), 20), &err);
        const QString code = r.value(QStringLiteral("code")).toString();
        const QString key  = r.value(QStringLiteral("key")).toString();
        const QString pid  = joinPlayer(room, code, QStringLiteral("Sara"));
        room.hostAction(QStringLiteral("start"),
                        QJsonObject{{QStringLiteral("code"), code}, {QStringLiteral("key"), key}}, &err);
        const QJsonObject a = room.answer(answerReq(code, pid, 3), &err);
        QVERIFY(!a.isEmpty());
        QCOMPARE(a.value(QStringLiteral("points")).toInt(), 0);
        QCOMPARE(a.value(QStringLiteral("okAnswer")).toBool(), false);
    }

    void doppiaRispostaRifiutata()
    {
        QuizRoom room;
        QString err;
        const QJsonObject r = room.create(createReq(QStringLiteral("entrambe"), 20), &err);
        const QString code = r.value(QStringLiteral("code")).toString();
        const QString key  = r.value(QStringLiteral("key")).toString();
        const QString pid  = joinPlayer(room, code, QStringLiteral("Eva"));
        room.hostAction(QStringLiteral("start"),
                        QJsonObject{{QStringLiteral("code"), code}, {QStringLiteral("key"), key}}, &err);
        QVERIFY(!room.answer(answerReq(code, pid, 0), &err).isEmpty());
        const QJsonObject second = room.answer(answerReq(code, pid, 1), &err);
        QVERIFY(second.isEmpty());
        QVERIFY(err.contains(QStringLiteral("già risposto")));
    }

    void joinDopoStartRifiutato()
    {
        QuizRoom room;
        QString err;
        const QJsonObject r = room.create(createReq(QStringLiteral("entrambe"), 20), &err);
        const QString code = r.value(QStringLiteral("code")).toString();
        const QString key  = r.value(QStringLiteral("key")).toString();
        joinPlayer(room, code, QStringLiteral("Primo"));
        room.hostAction(QStringLiteral("start"),
                        QJsonObject{{QStringLiteral("code"), code}, {QStringLiteral("key"), key}}, &err);
        const QJsonObject late = room.join(QJsonObject{
            {QStringLiteral("code"), code},
            {QStringLiteral("name"), QStringLiteral("Ritardatario")}}, &err);
        QVERIFY(late.isEmpty());
        QVERIFY(err.contains(QStringLiteral("già iniziata")));
    }

    /* ── modalità reveal ── */
    void revealFineNonTrapela()
    {
        QuizRoom room;
        QString err;
        const QJsonObject r = room.create(createReq(QStringLiteral("fine"), 20), &err);
        const QString code = r.value(QStringLiteral("code")).toString();
        const QString key  = r.value(QStringLiteral("key")).toString();
        const QString pid  = joinPlayer(room, code, QStringLiteral("Marta"));
        room.hostAction(QStringLiteral("start"),
                        QJsonObject{{QStringLiteral("code"), code}, {QStringLiteral("key"), key}}, &err);
        const QJsonObject a = room.answer(answerReq(code, pid, 0), &err);
        QVERIFY(!a.isEmpty());
        QVERIFY(!a.contains(QStringLiteral("correct")));
        QVERIFY(!a.contains(QStringLiteral("exp")));

        room.hostAction(QStringLiteral("reveal"),
                        QJsonObject{{QStringLiteral("code"), code}, {QStringLiteral("key"), key}}, &err);
        const QJsonObject st = room.playerState(code, pid, &err);
        QCOMPARE(st.value(QStringLiteral("state")).toString(), QStringLiteral("reveal"));
        QVERIFY(!st.contains(QStringLiteral("correct")));

        room.hostAction(QStringLiteral("end"),
                        QJsonObject{{QStringLiteral("code"), code}, {QStringLiteral("key"), key}}, &err);
        const QJsonObject end = room.playerState(code, pid, &err);
        QCOMPARE(end.value(QStringLiteral("state")).toString(), QStringLiteral("ended"));
        QVERIFY(end.contains(QStringLiteral("recap")));
        QVERIFY(end.value(QStringLiteral("recap")).toArray().size() >= 1);
    }

    void revealSubitoSenzaRecapFinale()
    {
        QuizRoom room;
        QString err;
        const QJsonObject r = room.create(createReq(QStringLiteral("subito"), 20), &err);
        const QString code = r.value(QStringLiteral("code")).toString();
        const QString key  = r.value(QStringLiteral("key")).toString();
        const QString pid  = joinPlayer(room, code, QStringLiteral("Paolo"));
        room.hostAction(QStringLiteral("start"),
                        QJsonObject{{QStringLiteral("code"), code}, {QStringLiteral("key"), key}}, &err);
        const QJsonObject a = room.answer(answerReq(code, pid, 0), &err);
        QVERIFY(a.contains(QStringLiteral("correct")));
        QVERIFY(a.contains(QStringLiteral("exp")));

        room.hostAction(QStringLiteral("end"),
                        QJsonObject{{QStringLiteral("code"), code}, {QStringLiteral("key"), key}}, &err);
        const QJsonObject end = room.playerState(code, pid, &err);
        QVERIFY(!end.contains(QStringLiteral("recap")));
        QVERIFY(end.contains(QStringLiteral("leaderboard")));
    }

    void chiaveHostErrata()
    {
        QuizRoom room;
        QString err;
        const QJsonObject r = room.create(createReq(QStringLiteral("entrambe"), 20), &err);
        const QString code = r.value(QStringLiteral("code")).toString();
        const QJsonObject o = room.hostAction(QStringLiteral("start"),
            QJsonObject{{QStringLiteral("code"), code}, {QStringLiteral("key"), QStringLiteral("nope")}}, &err);
        QVERIFY(o.isEmpty());
        QVERIFY(err.contains(QStringLiteral("Chiave")));
    }
};

QTEST_MAIN(TestQuizRoom)
#include "test_quiz_room.moc"
