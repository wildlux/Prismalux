/* ══════════════════════════════════════════════════════════════
   test_lan_server_queue.cpp — Test broadcastQueuePositions()

   Categoria:
     CAT-A  broadcastQueuePositions() scrive su ogni socket in coda una
            riga NDJSON {"status":"queued","position":N,"queue_size":M}
            con N 1-based nell'ordine FIFO — verificato con socket reali
            (via QTcpServer di appoggio) invece di forzare una vera race
            di rete sullo stato "busy" di AiClient, per evitare test
            fragili legati al timing.

   Build:
     cmake --build gui/build_tests -j4 --target test_lan_server_queue
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QApplication>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>

#include "mock_ai_client.h"
#include "../lan_server.h"

/* Accesso ai membri privati di LanServer per il test (friend struct,
 * dichiarato in lan_server.h). Costruisce PendingLlmRequest minimali
 * puntando a socket reali connessi tramite un QTcpServer di appoggio. */
struct LlmQueueTestAccess {
    static void enqueueFake(LanServer& s, QTcpSocket* sock, const QString& msg) {
        LanServer::PendingLlmRequest p;
        p.sock    = sock;
        p.userMsg = msg;
        s.m_llmQueue.enqueue(p);
    }
    static void broadcast(LanServer& s) { s.broadcastQueuePositions(); }
    static int  queueSize(LanServer& s) { return static_cast<int>(s.m_llmQueue.size()); }
};

class TestQueueBroadcast : public QObject {
    Q_OBJECT

private:
    /* Coppia socket connessi (client-side / server-side) tramite un
     * QTcpServer di appoggio — non passa dal parser HTTP di LanServer,
     * serve solo ad avere QTcpSocket* reali su cui scrivere/leggere. */
    struct SocketPair { QTcpSocket* client; QTcpSocket* serverSide; };

    QList<SocketPair> makePairs(int n) {
        QList<SocketPair> out;
        auto* helper = new QTcpServer(this);
        if (!helper->listen(QHostAddress::LocalHost)) return out;
        for (int i = 0; i < n; ++i) {
            auto* c = new QTcpSocket(this);
            c->connectToHost(QHostAddress::LocalHost, helper->serverPort());
            if (!c->waitForConnected(1000)) continue;
            if (!helper->waitForNewConnection(1000)) continue;
            auto* srvSide = helper->nextPendingConnection();
            if (!srvSide) continue;
            out.append(SocketPair{c, srvSide});
        }
        return out;
    }

private slots:

    /* A-1: 3 richieste in coda → ciascun client riceve la propria
     * posizione 1-based e la dimensione totale coda corretta. */
    void posizioniCorretteOrdineFifo() {
        MockAiClient ai;
        LanServer srv(&ai);

        const auto pairs = makePairs(3);
        if (pairs.size() < 3) QSKIP("impossibile creare le coppie socket di test");

        for (int i = 0; i < pairs.size(); ++i)
            LlmQueueTestAccess::enqueueFake(srv, pairs[i].serverSide, QString("msg%1").arg(i));

        QCOMPARE(LlmQueueTestAccess::queueSize(srv), 3);
        LlmQueueTestAccess::broadcast(srv);

        for (int i = 0; i < pairs.size(); ++i) {
            QVERIFY2(pairs[i].client->waitForReadyRead(1000),
                     qPrintable(QString("client %1 non ha ricevuto nulla").arg(i)));
            const QByteArray line = pairs[i].client->readAll().trimmed();
            const QJsonDocument doc = QJsonDocument::fromJson(line);
            QVERIFY2(doc.isObject(), qPrintable("JSON non valido: " + line));
            const QJsonObject obj = doc.object();
            QCOMPARE(obj["status"].toString(), QString("queued"));
            QCOMPARE(obj["position"].toInt(), i + 1);
            QCOMPARE(obj["queue_size"].toInt(), 3);
        }
    }

    /* A-2: un socket con puntatore nullo (client disconnesso mentre in
     * coda, QPointer già annullato) non deve far crashare il broadcast
     * né interrompere le notifiche agli altri. */
    void socketNulloNonCrasha() {
        MockAiClient ai;
        LanServer srv(&ai);

        const auto pairs = makePairs(2);
        if (pairs.size() < 2) QSKIP("impossibile creare le coppie socket di test");

        LlmQueueTestAccess::enqueueFake(srv, nullptr, "orfano");
        LlmQueueTestAccess::enqueueFake(srv, pairs[0].serverSide, "vivo");

        LlmQueueTestAccess::broadcast(srv);  /* non deve crashare */

        QVERIFY(pairs[0].client->waitForReadyRead(1000));
        const QJsonDocument doc = QJsonDocument::fromJson(pairs[0].client->readAll().trimmed());
        QVERIFY(doc.isObject());
        QCOMPARE(doc.object()["position"].toInt(), 2);  /* secondo in coda, il nullo è il primo */
    }
};

QTEST_MAIN(TestQueueBroadcast)
#include "test_lan_server_queue.moc"
