#pragma once
/*
 * mock_ollama_server.h — Server HTTP minimale che simula Ollama
 * ==============================================================
 * Usato nei test per disaccoppiare AiClient da Ollama reale.
 * Header-only senza Q_OBJECT per poter essere incluso ovunque.
 *
 * Simula:
 *   GET  /api/tags  → lista modelli JSON
 *   POST /api/chat  → NDJSON streaming con token prefissati
 *   POST /api/generate → NDJSON streaming
 *
 * Uso:
 *   MockOllamaServer srv;
 *   QVERIFY(srv.start());
 *   AiClient ai;
 *   ai.setBackend(AiClient::Ollama, "127.0.0.1", srv.port(), "mock:model");
 *   srv.setReply("Parola per parola.");
 *   // ... QEventLoop + ai.chat(...) ...
 *   srv.stop();
 */

#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>

struct MockOllamaServer
{
    explicit MockOllamaServer() : m_srv(new QTcpServer) {}
    ~MockOllamaServer() { stop(); delete m_srv; }

    bool start()
    {
        if (!m_srv->listen(QHostAddress::LocalHost, 0))
            return false;

        QObject::connect(m_srv, &QTcpServer::newConnection, m_srv, [this] {
            while (m_srv->hasPendingConnections()) {
                auto* sock = m_srv->nextPendingConnection();
                QObject::connect(sock, &QTcpSocket::readyRead, sock, [this, sock] {
                    handleRequest(sock);
                });
                QObject::connect(sock, &QTcpSocket::disconnected,
                                 sock, &QObject::deleteLater);
            }
        });
        return true;
    }

    void stop() { m_srv->close(); }

    quint16 port() const { return m_srv->serverPort(); }

    void setReply(const QString& text) { m_reply = text; }
    void setModels(const QStringList& models) { m_models = models; }

private:
    void handleRequest(QTcpSocket* sock)
    {
        const QByteArray raw = sock->readAll();
        const QString firstLine = QString::fromUtf8(raw).section('\n', 0, 0).trimmed();

        if (firstLine.contains("/api/tags")) {
            sendTags(sock);
        } else if (firstLine.contains("/api/chat") || firstLine.contains("/api/generate")) {
            sendStream(sock);
        } else {
            sock->write("HTTP/1.1 404 Not Found\r\nContent-Length: 9\r\n\r\nNot Found");
            sock->flush();
        }
    }

    void sendTags(QTcpSocket* sock)
    {
        QJsonArray arr;
        for (const QString& m : (m_models.isEmpty() ? QStringList{"mock:model"} : m_models)) {
            QJsonObject o;
            o["name"] = m; o["model"] = m; o["size"] = 1000000;
            arr.append(o);
        }
        QJsonObject root; root["models"] = arr;
        const QByteArray body = QJsonDocument(root).toJson(QJsonDocument::Compact);
        sock->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                    "Content-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body);
        sock->flush();
    }

    void sendStream(QTcpSocket* sock)
    {
        const QString reply = m_reply.isEmpty() ? "Risposta mock." : m_reply;
        const QStringList tokens = reply.split(' ', Qt::SkipEmptyParts);
        const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

        /* Costruiamo l'intero body NDJSON in memoria per poter usare Content-Length.
           QNAM con chunked encoding aspetta la chiusura del socket per segnalare
           il completamento — con Content-Length il completamento è immediato. */
        QByteArray body;
        for (int i = 0; i < tokens.size(); ++i) {
            QJsonObject msg;
            msg["role"]    = "assistant";
            msg["content"] = tokens[i] + (i < tokens.size() - 1 ? " " : "");
            QJsonObject frame;
            frame["model"] = "mock:model"; frame["created_at"] = now;
            frame["message"] = msg; frame["done"] = false;
            body += QJsonDocument(frame).toJson(QJsonDocument::Compact) + "\n";
        }

        QJsonObject done;
        done["model"] = "mock:model"; done["created_at"] = now;
        done["done"] = true; done["total_duration"] = 100000000;
        done["prompt_eval_count"] = 5; done["eval_count"] = tokens.size();
        body += QJsonDocument(done).toJson(QJsonDocument::Compact) + "\n";

        sock->write("HTTP/1.1 200 OK\r\n"
                    "Content-Type: application/x-ndjson\r\n"
                    "Connection: close\r\n"
                    "Content-Length: " + QByteArray::number(body.size()) + "\r\n\r\n"
                    + body);
        sock->flush();
        sock->disconnectFromHost();
    }

    QTcpServer* m_srv;
    QString     m_reply;
    QStringList m_models;
};
