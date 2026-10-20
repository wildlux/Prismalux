/* ══════════════════════════════════════════════════════════════
   test_decision_pipeline.cpp — pipeline Decision Model end-to-end
   con un backend HTTP finto (nessun Ollama / llama-server reale).

   Copre il bug 2026-10-20: la ri-entrata interna di AiClient::chat()
   veniva bloccata da busy() e abort() azzerava lo stato decisionale,
   rendendo la pipeline inerte. Qui si verifica il comportamento
   osservabile:

     CAT-A  classificazione → richiesta LLM (ordine delle chiamate)
     CAT-B  riverifica attiva → token bufferizzati, verdetto prima
     CAT-C  verdetto negativo → retry una volta con obiezione
     CAT-D  secondo verdetto negativo → risposta mostrata comunque
     CAT-E  riverifica spenta → streaming normale, nessun verdetto
     CAT-F  backend llama-server → OpenAI /v1/models + /v1/chat/completions
     CAT-G  backend ONNX senza modelli → errore esplicito, nessun fallback a Ollama

   Build:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests --target test_decision_pipeline
     QT_QPA_PLATFORM=offscreen ./build_tests/test_decision_pipeline
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QApplication>
#include <QSettings>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QEventLoop>
#include <QTimer>
#include <QSignalSpy>

#include "../ai_client.h"
#include "../decision_engine.h"
#include "../prismalux_paths.h"

/* ══════════════════════════════════════════════════════════════
   MockDecisionBackend — Ollama + OpenAI-compatible in un solo server

   Instrada in base al path e al contenuto della seconda domanda:
     · "State: ..."    → classificazione (risposta non-stream)
     · "Question: ..." → verifica (Y/N, sequenza configurabile)
     · altro           → risposta LLM (NDJSON streaming)

   Registra l'ordine delle chiamate in requests: "tipo|modello|path".
   ══════════════════════════════════════════════════════════════ */
struct MockDecisionBackend
{
    explicit MockDecisionBackend() : m_srv(new QTcpServer) {}
    ~MockDecisionBackend() { stop(); delete m_srv; }

    bool start()
    {
        if (!m_srv->listen(QHostAddress::LocalHost, 0))
            return false;
        QObject::connect(m_srv, &QTcpServer::newConnection, m_srv, [this] {
            while (m_srv->hasPendingConnections()) {
                QTcpSocket* sock = m_srv->nextPendingConnection();
                QObject::connect(sock, &QTcpSocket::readyRead, sock,
                                 [this, sock] { onReadyRead(sock); });
                QObject::connect(sock, &QTcpSocket::disconnected, sock,
                                 [this, sock] {
                                     m_buf.remove(sock);
                                     sock->deleteLater();
                                 });
            }
        });
        return true;
    }

    void stop() { m_srv->close(); }
    quint16 port() const { return m_srv->serverPort(); }

    void setClassifyLetter(const QString& l) { classifyLetter = l; }
    void setVerdicts(const QStringList& v) { verdicts = v; verdictIdx = 0; }
    void setAnswers(const QStringList& a) { answers = a; answerIdx = 0; }

    QString classifyLetter = QStringLiteral("A");
    QStringList verdicts;      ///< vuoto → sempre "Y"
    QStringList answers;       ///< vuoto → "Risposta mock."
    QStringList requests;      ///< "classify|mock-dec|/api/chat"

private:
    QTcpServer* m_srv;
    QHash<QTcpSocket*, QByteArray> m_buf;
    int verdictIdx = 0;
    int answerIdx  = 0;

    void onReadyRead(QTcpSocket* sock)
    {
        QByteArray& buf = m_buf[sock];
        buf += sock->readAll();

        const int hdrEnd = buf.indexOf("\r\n\r\n");
        if (hdrEnd < 0) return;

        const QByteArray head = buf.left(hdrEnd);
        int contentLength = 0;
        for (const QByteArray& line : head.split('\n')) {
            if (line.toLower().startsWith("content-length:"))
                contentLength = line.mid(15).trimmed().toInt();
        }
        if (buf.size() < hdrEnd + 4 + contentLength) return;   /* body incompleto */

        const QByteArray body = buf.mid(hdrEnd + 4, contentLength);
        buf.remove(0, hdrEnd + 4 + contentLength);
        route(sock, head, body);
    }

    void route(QTcpSocket* sock, const QByteArray& head, const QByteArray& body)
    {
        const QByteArray firstLine = head.left(head.indexOf("\r\n"));
        const QList<QByteArray> parts = firstLine.split(' ');
        const QString path = QString::fromUtf8(parts.value(1));

        if (path == "/api/tags") {
            QJsonArray arr;
            for (const QString& m : { QStringLiteral("mock-dec"),
                                      QStringLiteral("mock-llm") }) {
                QJsonObject o;
                o["name"] = m; o["model"] = m; o["size"] = 1000000;
                arr.append(o);
            }
            QJsonObject root; root["models"] = arr;
            sendJson(sock, QJsonDocument(root).toJson(QJsonDocument::Compact));
            return;
        }

        if (path == "/v1/models") {
            /* Deliberatamente SENZA "size": l'API OpenAI non lo espone.
               Il test verifica che la presenza basti a marcare il
               modello come installato (sentinel in onTagsReply). */
            QJsonArray data;
            for (const QString& m : { QStringLiteral("mock-dec"),
                                      QStringLiteral("mock-llm") }) {
                QJsonObject o;
                o["id"] = m; o["object"] = "model";
                data.append(o);
            }
            QJsonObject root; root["object"] = "list"; root["data"] = data;
            sendJson(sock, QJsonDocument(root).toJson(QJsonDocument::Compact));
            return;
        }

        const QJsonObject req = QJsonDocument::fromJson(body).object();
        const QString model = req.value("model").toString();
        const QJsonArray messages = req.value("messages").toArray();
        const QString user = messages.size() > 1
            ? messages.at(1).toObject().value("content").toString()
            : QString();

        const bool openAi = path.startsWith(QStringLiteral("/v1/"));
        QString kind;
        QString reply;

        if (user.startsWith(QStringLiteral("State:"))) {
            kind = QStringLiteral("classify");
            reply = classifyLetter;
        } else if (user.startsWith(QStringLiteral("Question:"))) {
            kind = QStringLiteral("verify");
            reply = nextVerdict();
        } else {
            kind = QStringLiteral("llm");
            reply = nextAnswer();
        }
        requests << QStringLiteral("%1|%2|%3").arg(kind, model, path);

        if (kind == QStringLiteral("llm") && !openAi) {
            sendStream(sock, model, reply);
            return;
        }

        QJsonObject root;
        if (openAi) {
            QJsonObject msg;
            msg["role"] = "assistant"; msg["content"] = reply;
            QJsonObject choice;
            choice["message"] = msg; choice["finish_reason"] = "stop";
            root["model"] = model;
            root["choices"] = QJsonArray{ choice };
        } else {
            QJsonObject msg;
            msg["role"] = "assistant"; msg["content"] = reply;
            root["model"] = model;
            root["message"] = msg;
            root["done"] = true;
        }
        sendJson(sock, QJsonDocument(root).toJson(QJsonDocument::Compact));
    }

    QString nextVerdict()
    {
        if (verdicts.isEmpty()) return QStringLiteral("Y");
        const int i = qMin(verdictIdx++, verdicts.size() - 1);
        return verdicts.at(i);
    }

    QString nextAnswer()
    {
        if (answers.isEmpty()) return QStringLiteral("Risposta mock.");
        const int i = qMin(answerIdx++, answers.size() - 1);
        return answers.at(i);
    }

    void sendJson(QTcpSocket* sock, const QByteArray& body)
    {
        send(sock, body, "application/json");
    }

    void sendStream(QTcpSocket* sock, const QString& model, const QString& text)
    {
        QByteArray body;
        const QStringList toks = text.split(' ', Qt::SkipEmptyParts);
        for (int i = 0; i < toks.size(); ++i) {
            QJsonObject msg;
            msg["role"] = "assistant";
            msg["content"] = toks.at(i) + (i < toks.size() - 1 ? " " : "");
            QJsonObject frame;
            frame["model"] = model;
            frame["message"] = msg;
            frame["done"] = false;
            body += QJsonDocument(frame).toJson(QJsonDocument::Compact) + "\n";
        }
        QJsonObject done;
        done["model"] = model; done["done"] = true;
        body += QJsonDocument(done).toJson(QJsonDocument::Compact) + "\n";
        send(sock, body, "application/x-ndjson");
    }

    void send(QTcpSocket* sock, const QByteArray& body, const char* contentType)
    {
        sock->write("HTTP/1.1 200 OK\r\nContent-Type: ");
        sock->write(contentType);
        sock->write("\r\nContent-Length: ");
        sock->write(QByteArray::number(body.size()));
        sock->write("\r\nConnection: close\r\n\r\n");
        sock->write(body);
        sock->flush();
        sock->disconnectFromHost();
    }
};

/* ══════════════════════════════════════════════════════════════
   Suite
   ══════════════════════════════════════════════════════════════ */
class TestDecisionPipeline : public QObject
{
    Q_OBJECT

private:
    MockDecisionBackend* m_srv = nullptr;

    void writeCatalog()
    {
        DecisionEngine::ModelSpec s;
        s.name = QStringLiteral("mock-dec");
        s.backend = DecisionEngine::BackendOllama;
        DecisionEngine::saveCatalog({ s });
    }

    bool waitInstalled(DecisionEngine& dec)
    {
        QEventLoop loop;
        QObject::connect(&dec, &DecisionEngine::installedReady,
                         &loop, &QEventLoop::quit);
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        dec.refreshInstalled();
        loop.exec();
        return !dec.installedModels().isEmpty();
    }

    void configure(AiClient& ai, DecisionEngine& dec, bool verify,
                   DecisionEngine::DecisionBackend backend
                       = DecisionEngine::BackendOllama)
    {
        dec.setHostPort(QStringLiteral("127.0.0.1"), m_srv->port());
        dec.setBackend(backend);
        dec.setEnabled(true);
        dec.setVerifyEnabled(verify);
        dec.setModel(QStringLiteral("mock-dec"));
        dec.setThreshold(0.5);
        ai.setDecisionEngine(&dec);
        ai.setBackend(AiClient::Ollama, QStringLiteral("127.0.0.1"),
                      m_srv->port(), QStringLiteral("mock-llm"));
    }

private slots:

    void initTestCase()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("Prismalux"));
        QCoreApplication::setApplicationName(QStringLiteral("GUI"));
    }

    void init()
    {
        QDir().mkpath(QDir::homePath() + QStringLiteral("/.prismalux"));
        writeCatalog();
        m_srv = new MockDecisionBackend;
        QVERIFY(m_srv->start());
    }

    void cleanup()
    {
        delete m_srv;
        m_srv = nullptr;
    }

    /* ── CAT-A ──────────────────────────────────────────────────
       La classificazione avviene PRIMA della richiesta al modello
       conversazionale, e la ri-entrata interna non viene bloccata. */
    void classificazione_prima_della_richiesta()
    {
        DecisionEngine dec;
        AiClient ai;
        configure(ai, dec, /*verify=*/false);
        QVERIFY(waitInstalled(dec));
        QVERIFY(dec.active());

        QSignalSpy classified(&ai, &AiClient::decisionClassified);
        QSignalSpy finished(&ai, &AiClient::finished);
        QSignalSpy errors(&ai, &AiClient::error);

        ai.chat(QStringLiteral("Sei un assistente."),
                QStringLiteral("domanda unica pipeline base"));
        QTRY_VERIFY_WITH_TIMEOUT(finished.count() + errors.count() > 0, 5000);

        QCOMPARE(errors.count(), 0);
        QCOMPARE(finished.count(), 1);
        QCOMPARE(classified.count(), 1);
        QCOMPARE(classified.at(0).at(0).toString(), QStringLiteral("coding"));

        QCOMPARE(m_srv->requests.size(), 2);
        QVERIFY2(m_srv->requests.at(0).startsWith(
                     QStringLiteral("classify|mock-dec|/api/chat")),
                 qPrintable(m_srv->requests.at(0)));
        QVERIFY2(m_srv->requests.at(1).startsWith(
                     QStringLiteral("llm|mock-llm|/api/chat")),
                 qPrintable(m_srv->requests.at(1)));
        QCOMPARE(finished.at(0).at(0).toString(), QStringLiteral("Risposta mock."));
    }

    /* ── CAT-B ──────────────────────────────────────────────────
       Con riverifica attiva i token non arrivano durante la
       generazione: il verdetto precede l'unico token in blocco. */
    void verifica_bufferizza_i_token()
    {
        DecisionEngine dec;
        AiClient ai;
        configure(ai, dec, /*verify=*/true);
        QVERIFY(waitInstalled(dec));
        QVERIFY(dec.active());

        m_srv->setVerdicts({ QStringLiteral("Y") });
        m_srv->setAnswers({ QStringLiteral("Risposta verificata.") });

        QStringList events;
        connect(&ai, &AiClient::token, this,
                [&events](const QString&) { events << QStringLiteral("token"); });
        connect(&ai, &AiClient::decisionVerified, this,
                [&events](bool, const QString&) { events << QStringLiteral("verified"); });

        QSignalSpy finished(&ai, &AiClient::finished);
        QSignalSpy errors(&ai, &AiClient::error);
        QSignalSpy retries(&ai, &AiClient::decisionRetry);

        ai.chat(QStringLiteral("Sei un assistente."),
                QStringLiteral("spiegami questa funzione di ordinamento"));
        QTRY_VERIFY_WITH_TIMEOUT(finished.count() + errors.count() > 0, 5000);

        QCOMPARE(errors.count(), 0);
        QCOMPARE(retries.count(), 0);
        QCOMPARE(finished.at(0).at(0).toString(), QStringLiteral("Risposta verificata."));
        QCOMPARE(events.count(QStringLiteral("token")), 1);
        QVERIFY(events.contains(QStringLiteral("verified")));
        QVERIFY2(events.indexOf(QStringLiteral("verified"))
                     < events.indexOf(QStringLiteral("token")),
                 "il verdetto deve precedere l'emissione del token");
    }

    /* ── CAT-C ──────────────────────────────────────────────────
       Verdetto negativo → un retry con l'obiezione nel prompt e una
       sola risposta visibile (la seconda). */
    void verdetto_negativo_fa_un_retry()
    {
        DecisionEngine dec;
        AiClient ai;
        configure(ai, dec, /*verify=*/true);
        QVERIFY(waitInstalled(dec));
        QVERIFY(dec.active());

        m_srv->setVerdicts({ QStringLiteral("N"), QStringLiteral("Y") });
        m_srv->setAnswers({ QStringLiteral("Prima risposta."),
                            QStringLiteral("Seconda risposta.") });

        QStringList tokens;
        connect(&ai, &AiClient::token, this,
                [&tokens](const QString& t) { tokens << t; });

        QSignalSpy finished(&ai, &AiClient::finished);
        QSignalSpy errors(&ai, &AiClient::error);
        QSignalSpy retries(&ai, &AiClient::decisionRetry);
        QSignalSpy classifications(&ai, &AiClient::decisionClassified);

        ai.chat(QStringLiteral("Sei un assistente."),
                QStringLiteral("scrivi una funzione di parsing"));
        QTRY_VERIFY_WITH_TIMEOUT(finished.count() + errors.count() > 0, 5000);

        QCOMPARE(errors.count(), 0);
        QCOMPARE(retries.count(), 1);
        QCOMPARE(classifications.count(), 1);   /* il retry non riclassifica */
        QCOMPARE(finished.at(0).at(0).toString(), QStringLiteral("Seconda risposta."));
        QCOMPARE(tokens, QStringList{ QStringLiteral("Seconda risposta.") });

        /* classify, llm, verify, llm, verify */
        QCOMPARE(m_srv->requests.size(), 5);
    }

    /* ── CAT-D ──────────────────────────────────────────────────
       Secondo verdetto negativo: nessun terzo tentativo, la risposta
       viene mostrata comunque (con esito in chiaro). */
    void secondo_verdetto_negativo_mostra_la_risposta()
    {
        DecisionEngine dec;
        AiClient ai;
        configure(ai, dec, /*verify=*/true);
        QVERIFY(waitInstalled(dec));
        QVERIFY(dec.active());

        m_srv->setVerdicts({ QStringLiteral("N"), QStringLiteral("N") });
        m_srv->setAnswers({ QStringLiteral("Prima risposta."),
                            QStringLiteral("Seconda risposta.") });

        QStringList tokens;
        connect(&ai, &AiClient::token, this,
                [&tokens](const QString& t) { tokens << t; });
        QSignalSpy verified(&ai, &AiClient::decisionVerified);
        QSignalSpy retries(&ai, &AiClient::decisionRetry);
        QSignalSpy finished(&ai, &AiClient::finished);
        QSignalSpy errors(&ai, &AiClient::error);

        ai.chat(QStringLiteral("Sei un assistente."),
                QStringLiteral("analizza questo algoritmo"));
        QTRY_VERIFY_WITH_TIMEOUT(finished.count() + errors.count() > 0, 5000);

        QCOMPARE(errors.count(), 0);
        QCOMPARE(retries.count(), 1);
        QCOMPARE(verified.count(), 1);
        QCOMPARE(verified.at(0).at(0).toBool(), false);
        QCOMPARE(finished.at(0).at(0).toString(), QStringLiteral("Seconda risposta."));
        QCOMPARE(tokens, QStringList{ QStringLiteral("Seconda risposta.") });
    }

    /* ── CAT-E ──────────────────────────────────────────────────
       Riverifica spenta: streaming normale, nessun verdetto. */
    void verifica_spenta_mantiene_lo_streaming()
    {
        DecisionEngine dec;
        AiClient ai;
        configure(ai, dec, /*verify=*/false);
        QVERIFY(waitInstalled(dec));
        QVERIFY(dec.active());

        m_srv->setAnswers({ QStringLiteral("Tre parole qui.") });

        QStringList tokens;
        connect(&ai, &AiClient::token, this,
                [&tokens](const QString& t) { tokens << t; });
        QSignalSpy verified(&ai, &AiClient::decisionVerified);
        QSignalSpy finished(&ai, &AiClient::finished);
        QSignalSpy errors(&ai, &AiClient::error);

        ai.chat(QStringLiteral("Sei un assistente."),
                QStringLiteral("ciao come va oggi"));
        QTRY_VERIFY_WITH_TIMEOUT(finished.count() + errors.count() > 0, 5000);

        QCOMPARE(errors.count(), 0);
        QCOMPARE(verified.count(), 0);
        QCOMPARE(tokens.size(), 3);   /* "Tre ", "parole ", "qui." */
        QCOMPARE(finished.at(0).at(0).toString(), QStringLiteral("Tre parole qui."));
    }

    /* ── CAT-F ──────────────────────────────────────────────────
       llama-server: /v1/models senza "size" deve comunque contare
       come installato, e la chat usa /v1/chat/completions. */
    void llama_server_usa_endpoint_openai()
    {
        DecisionEngine dec;
        AiClient ai;
        configure(ai, dec, /*verify=*/false,
                  DecisionEngine::BackendLlamaServer);

        QVERIFY2(waitInstalled(dec),
                 "un modello presente in /v1/models deve risultare installato "
                 "anche senza il campo size");
        QVERIFY(dec.active());
        QCOMPARE(dec.installedModels().at(0).name, QStringLiteral("mock-dec"));

        m_srv->setClassifyLetter(QStringLiteral("B"));
        bool done = false;
        DecisionEngine::ClassifyResult res;
        dec.classify(QStringLiteral("dimostra che la radice di 2 è irrazionale"),
                     [&done, &res](const DecisionEngine::ClassifyResult& r) {
                         res = r; done = true;
                     });
        QTRY_VERIFY_WITH_TIMEOUT(done, 5000);

        QVERIFY(res.ok);
        QCOMPARE(res.intent, QStringLiteral("reasoning"));
        QVERIFY(!m_srv->requests.isEmpty());
        QVERIFY2(m_srv->requests.at(0).contains(
                     QStringLiteral("/v1/chat/completions")),
                 qPrintable(m_srv->requests.at(0)));
    }

    /* ── CAT-F2 ─────────────────────────────────────────────────
       registerModel(..., backend) deve persistere il backend: senza,
       un alias registrato per llama-server verrebbe marcato Ollama e
       refreshInstalled() non lo riconoscerebbe mai. */
    void registrazione_persiste_il_backend()
    {
        QVERIFY(DecisionEngine::registerModel(
            QStringLiteral("alias-llama"), DecisionEngine::RoleClassifier,
            QStringLiteral("test"), DecisionEngine::BackendLlamaServer));

        bool found = false;
        for (const DecisionEngine::ModelSpec& s : DecisionEngine::catalog()) {
            if (s.name == QStringLiteral("alias-llama")) {
                found = true;
                QCOMPARE(s.backend, DecisionEngine::BackendLlamaServer);
            }
        }
        QVERIFY(found);
    }

    /* ── CAT-G ──────────────────────────────────────────────────
       ONNX senza modelli: errore esplicito, nessun fallback
       silenzioso su Ollama. La verifica fallisce "aperta". */
    void onnx_senza_modelli_fallisce_esplicitamente()
    {
        /* Forza una cartella vuota: i modelli inclusi nella repo non
           devono mascherare il caso di installazione incompleta. */
        const QString emptyDir = QDir::tempPath()
            + QStringLiteral("/prismalux_onnx_missing_%1")
                  .arg(QCoreApplication::applicationPid());
        QDir(emptyDir).removeRecursively();
        qputenv("PRISMALUX_DECISION_ONNX_DIR", emptyDir.toLocal8Bit());

        DecisionEngine dec;
        dec.setBackend(DecisionEngine::BackendOnnx);
        dec.setEnabled(true);
        dec.setModel(QStringLiteral("onnx-decision"));
        dec.setVerifyEnabled(true);
        dec.refreshInstalled();

        QVERIFY2(dec.installedModels().isEmpty(),
                 "senza file ONNX non deve risultare installato nulla");
        QVERIFY(dec.onnxError().contains(QStringLiteral("ONNX")));

        bool classifyDone = false;
        DecisionEngine::ClassifyResult cr;
        dec.classify(QStringLiteral("test onnx"),
                     [&classifyDone, &cr](const DecisionEngine::ClassifyResult& r) {
                         cr = r; classifyDone = true;
                     });
        QVERIFY(classifyDone);
        QVERIFY(!cr.ok);
        QVERIFY2(cr.error.contains(QStringLiteral("ONNX")),
                 qPrintable(cr.error));
        QVERIFY(m_srv->requests.isEmpty());   /* nessuna rete */

        bool verifyDone = false;
        DecisionEngine::VerifyResult vr;
        dec.verify(QStringLiteral("q"), QStringLiteral("a"),
                   QStringLiteral("coding"),
                   [&verifyDone, &vr](const DecisionEngine::VerifyResult& r) {
                       vr = r; verifyDone = true;
                   });
        QVERIFY(verifyDone);
        QVERIFY(!vr.ok);
        QVERIFY(vr.approved);   /* fail-open: la risposta si mostra comunque */
        QVERIFY2(vr.error.contains(QStringLiteral("ONNX")),
                 qPrintable(vr.error));
        QVERIFY(m_srv->requests.isEmpty());

        qunsetenv("PRISMALUX_DECISION_ONNX_DIR");
    }
};

/* ══════════════════════════════════════════════════════════════
   main — HOME temporanea PRIMA di QApplication, così QSettings,
   registro e cache SQLite non toccano la configurazione reale.
   ══════════════════════════════════════════════════════════════ */
int main(int argc, char** argv)
{
    const QString home = QDir::tempPath()
        + QStringLiteral("/prismalux_pipe_test_%1")
              .arg(QCoreApplication::applicationPid());
    QDir(home).removeRecursively();
    QDir().mkpath(home + QStringLiteral("/.prismalux"));
    QDir().mkpath(home + QStringLiteral("/.config"));
    qputenv("HOME", home.toLocal8Bit());
    qputenv("XDG_CONFIG_HOME", (home + QStringLiteral("/.config")).toLocal8Bit());

    QApplication app(argc, argv);

    TestDecisionPipeline t;
    const int status = QTest::qExec(&t, argc, argv);

    QDir(home).removeRecursively();
    return status;
}

#include "test_decision_pipeline.moc"
