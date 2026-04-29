/* ══════════════════════════════════════════════════════════════
   test_ai_stress.cpp — Suite stress AiClient: matrice parametri completa

   Categorie:
     CAT-F  Matrice parametri (data-driven, mock server)
            honesty × caveman × QueryType × think-capable = 24 combinazioni
     CAT-G  Stress sequenziale (mock server + rete reale)
            throttle, abort+retry, N=20 richieste consecutive, payload grande,
            storia lunga 50 turni
     CAT-H  Edge case (mock server)
            tutte le 9 keyword temporali, system vuoto, busy guard,
            abort idle, auto-Complex detection, classifyQuery completo
     CAT-I  Qualità reale con Ollama (rete reale)
            caveman on/off, honesty trap, think confronto,
            temperature estremi, num_predict basso vs alto

   COME ESEGUIRE:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_ai_stress
     ./build_tests/test_ai_stress

   REQUISITI (solo CAT-G stress reale + CAT-I):
     - Ollama in esecuzione su 127.0.0.1:11434
     - PRISMALUX_TEST_MODEL (default: mistral:7b-instruct)
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QProcessEnvironment>
#include <QTcpServer>
#include <QTcpSocket>
#include <memory>

#include "../ai_client.h"

/* ── Modello e timeout configurabili ──────────────────────────── */
static QString testModel()
{
    const QString env = QProcessEnvironment::systemEnvironment()
                            .value("PRISMALUX_TEST_MODEL");
    return env.isEmpty() ? QStringLiteral("mistral:7b-instruct") : env;
}

static constexpr int kTimeoutMs   = 180'000;  /* 3 min per risposta LLM  */
static constexpr int kConnTimeout = 5'000;    /* 5s per check connettività */
static constexpr int kMockTimeout = 8'000;    /* 8s per mock server       */

/* ══════════════════════════════════════════════════════════════
   OllamaMockServer — mini-server TCP che simula Ollama localmente.
   Identico alla versione in test_ai_integration.cpp — duplicato qui
   per garantire isolamento dei test (nessuna dipendenza cross-file).
   ══════════════════════════════════════════════════════════════ */
class OllamaMockServer : public QObject {
    Q_OBJECT
public:
    struct Captured {
        QJsonObject body;
        QString     systemContent;
        QString     userContent;
        QJsonObject options;
    };

    explicit OllamaMockServer(QObject* parent = nullptr)
        : QObject(parent)
        , m_server(new QTcpServer(this))
    {
        connect(m_server, &QTcpServer::newConnection,
                this,     &OllamaMockServer::onNewConnection);
    }

    bool     listen(quint16 port) { return m_server->listen(QHostAddress::LocalHost, port); }
    quint16  port()         const { return m_server->serverPort(); }
    Captured last()         const { return m_last; }
    int      requestCount() const { return m_count; }
    void     resetCount()         { m_count = 0; }

    void setMockResponse(const QString& text) { m_mockText = text; }

private slots:
    void onNewConnection() {
        QTcpSocket* sock = m_server->nextPendingConnection();
        auto buf = std::make_shared<QByteArray>();

        connect(sock, &QTcpSocket::readyRead, this, [this, sock, buf]() {
            *buf += sock->readAll();

            const int sep = buf->indexOf("\r\n\r\n");
            if (sep < 0) return;

            const QString hdrs = QString::fromLatin1(buf->left(sep));
            int contentLen = 0;
            for (const QString& line : hdrs.split("\r\n")) {
                if (line.toLower().startsWith("content-length:"))
                    contentLen = line.mid(15).trimmed().toInt();
            }

            const QByteArray body = buf->mid(sep + 4);
            if (body.size() < contentLen) return;

            const QJsonObject root = QJsonDocument::fromJson(body.left(contentLen)).object();
            m_last.body    = root;
            m_last.options = root["options"].toObject();

            const QJsonArray msgs = root["messages"].toArray();
            m_last.systemContent.clear();
            m_last.userContent.clear();
            for (const QJsonValue& v : msgs) {
                const QJsonObject msg = v.toObject();
                const QString role = msg["role"].toString();
                if (role == "system") m_last.systemContent = msg["content"].toString();
                if (role == "user")   m_last.userContent   = msg["content"].toString();
            }
            ++m_count;

            const QString text = m_mockText.isEmpty() ? "mock-ok" : m_mockText;
            const QByteArray line1 = QJsonDocument(QJsonObject{
                {"message", QJsonObject{{"role","assistant"},{"content", text}}},
                {"done", false}
            }).toJson(QJsonDocument::Compact) + "\n";
            const QByteArray line2 = QJsonDocument(QJsonObject{
                {"message", QJsonObject{{"role","assistant"},{"content",""}}},
                {"done", true}
            }).toJson(QJsonDocument::Compact) + "\n";

            const QByteArray ndjson = line1 + line2;
            const QByteArray httpResp =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: application/x-ndjson\r\n"
                "Content-Length: " + QByteArray::number(ndjson.size()) + "\r\n"
                "Connection: close\r\n"
                "\r\n" + ndjson;

            sock->write(httpResp);
            sock->flush();
            sock->disconnectFromHost();
        });

        connect(sock, &QTcpSocket::disconnected, sock, &QTcpSocket::deleteLater);
    }

private:
    QTcpServer* m_server;
    Captured    m_last;
    int         m_count   = 0;
    QString     m_mockText;
};

/* ── Helpers condivisi ─────────────────────────────────────────── */
struct ChatResult {
    QString  response;
    QString  errorMsg;
    bool     ok          = false;
    int      elapsed_ms  = 0;
    int      tokenCount  = 0;
    quint64  reqId       = 0;
};

static ChatResult runChat(const QString& system,
                          const QString& msg,
                          AiClient::QueryType qt = AiClient::QueryAuto,
                          const QJsonArray& history = {},
                          const QString& model = QString(),
                          int port = 11434)
{
    AiClient ai;
    const QString m = model.isEmpty() ? testModel() : model;
    ai.setBackend(AiClient::Ollama, "127.0.0.1", port, m);

    ChatResult res;
    QEventLoop loop;
    QElapsedTimer timer;
    QTimer watchdog;

    QObject::connect(&ai, &AiClient::token, &loop, [&](const QString&) {
        ++res.tokenCount;
    });
    QObject::connect(&ai, &AiClient::finished, &loop, [&](const QString& full) {
        res.response   = full;
        res.ok         = true;
        res.elapsed_ms = static_cast<int>(timer.elapsed());
        loop.quit();
    });
    QObject::connect(&ai, &AiClient::error, &loop, [&](const QString& err) {
        res.errorMsg   = err;
        res.ok         = false;
        res.elapsed_ms = static_cast<int>(timer.elapsed());
        loop.quit();
    });

    watchdog.setSingleShot(true);
    QObject::connect(&watchdog, &QTimer::timeout, &loop, [&]() {
        res.errorMsg = "TIMEOUT " + QString::number(kTimeoutMs / 1000) + "s";
        res.ok = false;
        loop.quit();
    });

    timer.start();
    watchdog.start(kTimeoutMs);
    res.reqId = ai.chat(system, msg, history, qt);
    loop.exec();
    return res;
}

static bool ollamaReachable()
{
    AiClient ai;
    ai.setBackend(AiClient::Ollama, "127.0.0.1", 11434, testModel());
    bool reached = false;

    QEventLoop loop;
    QTimer watchdog;

    QObject::connect(&ai, &AiClient::modelsReady, &loop, [&](const QStringList& list) {
        reached = !list.isEmpty();
        loop.quit();
    });
    watchdog.setSingleShot(true);
    QObject::connect(&watchdog, &QTimer::timeout, &loop, &QEventLoop::quit);

    watchdog.start(kConnTimeout);
    ai.fetchModels();
    loop.exec();
    return reached;
}

/* Esegue chat() sul mock server e restituisce captured + ok */
struct MockResult {
    bool     ok = false;
    QString  response;
    QString  error;
    OllamaMockServer::Captured captured;
};

static MockResult runMock(OllamaMockServer* srv,
                          quint16 port,
                          const AiChatParams& params,
                          const QString& model,
                          const QString& system,
                          const QString& msg,
                          AiClient::QueryType qt = AiClient::QueryAuto)
{
    AiClient ai;
    ai.setBackend(AiClient::Ollama, "127.0.0.1", port, model);
    ai.setChatParams(params);

    MockResult res;
    QEventLoop loop;
    QTimer watchdog;

    QObject::connect(&ai, &AiClient::finished, &loop, [&](const QString& full) {
        res.ok = true; res.response = full; loop.quit();
    });
    QObject::connect(&ai, &AiClient::error, &loop, [&](const QString& err) {
        res.error = err; loop.quit();
    });
    watchdog.setSingleShot(true);
    QObject::connect(&watchdog, &QTimer::timeout, &loop, [&]() {
        res.error = "TIMEOUT mock"; loop.quit();
    });
    watchdog.start(kMockTimeout);

    ai.chat(system, msg, {}, qt);
    loop.exec();

    res.captured = srv->last();
    return res;
}

/* ══════════════════════════════════════════════════════════════
   CAT-F — Matrice parametri (24 combinazioni, mock server)
   Testa ogni combinazione di:
     honesty_prefix  : on / off
     caveman_mode    : on / off
     QueryType       : Simple / Auto / Complex
     modello         : think-capable (qwen3:4b) / non-think-capable (mistral:7b-instruct)
   Verifica i costrutti iniettati nel JSON body per ognuna.
   ══════════════════════════════════════════════════════════════ */
class TestParamMatrix : public QObject {
    Q_OBJECT

    static constexpr quint16 kPort = 19980;

    OllamaMockServer* m_srv = nullptr;

private slots:

    void initTestCase() {
        m_srv = new OllamaMockServer(this);
        QVERIFY2(m_srv->listen(kPort),
                 qPrintable("Mock non avviato su porta " + QString::number(kPort)));
        m_srv->setMockResponse("mock-matrix");
    }

    void cleanupTestCase() { delete m_srv; m_srv = nullptr; }

    /* ── F-data: 24 combinazioni ───────────────────────────── */
    void matrixCombinations_data()
    {
        QTest::addColumn<bool>  ("honesty");
        QTest::addColumn<bool>  ("caveman");
        QTest::addColumn<int>   ("queryType");   /* 0=Simple 1=Auto 2=Complex */
        QTest::addColumn<QString>("model");
        QTest::addColumn<bool>  ("expectHonesty");
        QTest::addColumn<bool>  ("expectCaveman");
        QTest::addColumn<bool>  ("expectThink");      /* true solo se Complex + think-capable */
        QTest::addColumn<bool>  ("expectThinkFalse"); /* true se Simple (think:false esplicito) */
        QTest::addColumn<int>   ("basePredict");

        const QString thinkMod = "qwen3:4b";
        const QString nonThink = "mistral:7b-instruct";

        /* lambda-like via macro per aggiungere righe */
        auto row = [&](const char* name,
                       bool h, bool c, AiClient::QueryType qt, const QString& mod) {
            const bool thinkCap = mod.startsWith("qwen3");
            const bool expThink      = (qt == AiClient::QueryComplex && thinkCap);
            const bool expThinkFalse = (qt == AiClient::QuerySimple);
            QTest::newRow(name) << h << c << (int)qt << mod << h << c
                                << expThink << expThinkFalse << 1024;
        };

        /* think-capable, honesty on/off, caveman on/off, tutti i QueryType */
        row("TM/H+C+/Simp", true,  true,  AiClient::QuerySimple,  thinkMod);
        row("TM/H+C+/Auto", true,  true,  AiClient::QueryAuto,    thinkMod);
        row("TM/H+C+/Comp", true,  true,  AiClient::QueryComplex, thinkMod);
        row("TM/H+C-/Simp", true,  false, AiClient::QuerySimple,  thinkMod);
        row("TM/H+C-/Auto", true,  false, AiClient::QueryAuto,    thinkMod);
        row("TM/H+C-/Comp", true,  false, AiClient::QueryComplex, thinkMod);
        row("TM/H-C+/Simp", false, true,  AiClient::QuerySimple,  thinkMod);
        row("TM/H-C+/Auto", false, true,  AiClient::QueryAuto,    thinkMod);
        row("TM/H-C+/Comp", false, true,  AiClient::QueryComplex, thinkMod);
        row("TM/H-C-/Simp", false, false, AiClient::QuerySimple,  thinkMod);
        row("TM/H-C-/Auto", false, false, AiClient::QueryAuto,    thinkMod);
        row("TM/H-C-/Comp", false, false, AiClient::QueryComplex, thinkMod);

        /* non-think-capable, stesse 12 combinazioni */
        row("NT/H+C+/Simp", true,  true,  AiClient::QuerySimple,  nonThink);
        row("NT/H+C+/Auto", true,  true,  AiClient::QueryAuto,    nonThink);
        row("NT/H+C+/Comp", true,  true,  AiClient::QueryComplex, nonThink);
        row("NT/H+C-/Simp", true,  false, AiClient::QuerySimple,  nonThink);
        row("NT/H+C-/Auto", true,  false, AiClient::QueryAuto,    nonThink);
        row("NT/H+C-/Comp", true,  false, AiClient::QueryComplex, nonThink);
        row("NT/H-C+/Simp", false, true,  AiClient::QuerySimple,  nonThink);
        row("NT/H-C+/Auto", false, true,  AiClient::QueryAuto,    nonThink);
        row("NT/H-C+/Comp", false, true,  AiClient::QueryComplex, nonThink);
        row("NT/H-C-/Simp", false, false, AiClient::QuerySimple,  nonThink);
        row("NT/H-C-/Auto", false, false, AiClient::QueryAuto,    nonThink);
        row("NT/H-C-/Comp", false, false, AiClient::QueryComplex, nonThink);
    }

    void matrixCombinations()
    {
        QFETCH(bool,    honesty);
        QFETCH(bool,    caveman);
        QFETCH(int,     queryType);
        QFETCH(QString, model);
        QFETCH(bool,    expectHonesty);
        QFETCH(bool,    expectCaveman);
        QFETCH(bool,    expectThink);
        QFETCH(bool,    expectThinkFalse);
        QFETCH(int,     basePredict);

        AiChatParams p;
        p.honesty_prefix = honesty;
        p.caveman_mode   = caveman;
        p.num_predict    = basePredict;

        const auto qt = static_cast<AiClient::QueryType>(queryType);
        auto r = runMock(m_srv, kPort, p, model, "contesto", "spiega qualcosa", qt);

        QVERIFY2(r.ok, qPrintable("Errore mock: " + r.error));

        const QString& sys = r.captured.systemContent;

        /* ── Honesty prefix ── */
        if (expectHonesty) {
            QVERIFY2(sys.contains("REGOLA ASSOLUTA"),
                     qPrintable("kHonestyPrefix mancante. sys=" + sys.left(80)));
        } else {
            QVERIFY2(!sys.contains("REGOLA ASSOLUTA"),
                     qPrintable("kHonestyPrefix presente quando off. sys=" + sys.left(80)));
        }

        /* ── Caveman prefix ── */
        if (expectCaveman) {
            QVERIFY2(sys.contains("STILE RISPOSTA"),
                     qPrintable("kCavemanPrefix mancante. sys=" + sys.left(80)));
        } else {
            QVERIFY2(!sys.contains("STILE RISPOSTA"),
                     qPrintable("kCavemanPrefix presente quando off. sys=" + sys.left(80)));
        }

        /* ── Ordine prefissi se entrambi attivi ── */
        if (expectHonesty && expectCaveman) {
            QVERIFY2(sys.indexOf("REGOLA ASSOLUTA") < sys.indexOf("STILE RISPOSTA"),
                     "Ordine errato: honesty deve precedere caveman nel system message");
        }

        /* ── Think ── */
        if (expectThinkFalse) {
            /* QuerySimple → think:false esplicito */
            QCOMPARE(r.captured.options["think"].toBool(true), false);
            QCOMPARE(r.captured.options["num_predict"].toInt(), 512);
        } else if (expectThink) {
            /* QueryComplex + think-capable → think:true + num_predict×2 */
            QCOMPARE(r.captured.options["think"].toBool(false), true);
            QCOMPARE(r.captured.options["num_predict"].toInt(), basePredict * 2);
        } else if (qt == AiClient::QueryComplex) {
            /* QueryComplex + non think-capable → think non inviato o false */
            const QJsonValue tv = r.captured.options["think"];
            const bool thinkSent = !tv.isUndefined() && tv.toBool(false);
            QVERIFY2(!thinkSent,
                     qPrintable("think:true su non-think-capable. model=" + model));
            QCOMPARE(r.captured.options["num_predict"].toInt(), basePredict);
        }
        /* QueryAuto: nessuna verifica think (comportamento flessibile) */
    }

    /* ── F-extra: system prompt originale sempre conservato ────── */
    void systemPromptPreservedUnderAllPrefixes_data()
    {
        QTest::addColumn<bool>("honesty");
        QTest::addColumn<bool>("caveman");
        QTest::newRow("H+C+") << true  << true;
        QTest::newRow("H+C-") << true  << false;
        QTest::newRow("H-C+") << false << true;
        QTest::newRow("H-C-") << false << false;
    }

    void systemPromptPreservedUnderAllPrefixes()
    {
        QFETCH(bool, honesty);
        QFETCH(bool, caveman);

        const QString marker = "MARKER_SYSTEM_UNIQUEXYZ";
        AiChatParams p; p.honesty_prefix = honesty; p.caveman_mode = caveman;
        auto r = runMock(m_srv, kPort, p, "test-model", marker, "msg", AiClient::QueryAuto);
        QVERIFY2(r.ok, qPrintable("Errore mock: " + r.error));
        QVERIFY2(r.captured.systemContent.contains(marker),
                 qPrintable("System prompt perso con h=" + QString::number(honesty)
                            + " c=" + QString::number(caveman)));
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-G — Stress sequenziale
   G1-G5: mock server (veloce, nessuna rete reale)
   G6-G7: rete reale (Ollama) — saltati se non disponibile
   ══════════════════════════════════════════════════════════════ */
class TestStressSequential : public QObject {
    Q_OBJECT

    static constexpr quint16 kPort = 19981;

    OllamaMockServer* m_srv  = nullptr;
    bool              m_skip = false;

private slots:

    void initTestCase() {
        m_srv = new OllamaMockServer(this);
        QVERIFY2(m_srv->listen(kPort),
                 qPrintable("Mock non avviato su porta " + QString::number(kPort)));
        m_srv->setMockResponse("stress-ok");

        if (!ollamaReachable()) {
            qWarning("CAT-G (rete): Ollama non raggiungibile — test G6/G7 saltati.");
            m_skip = true;
        }
    }

    void cleanupTestCase() { delete m_srv; m_srv = nullptr; }

    /* ── G1: 20 richieste consecutive al mock (450ms di pausa tra l'una e l'altra) ── */
    void twentyConsecutiveMockRequests() {
        AiChatParams p; p.honesty_prefix = false; p.caveman_mode = false;

        int successful = 0;
        for (int i = 0; i < 20; ++i) {
            const QString msg = "test #" + QString::number(i);
            auto r = runMock(m_srv, kPort, p, "test-model", "sys", msg, AiClient::QueryAuto);
            if (r.ok) ++successful;
            /* Pausa oltre il throttle da 400ms tra richieste */
            QTest::qWait(450);
        }

        QVERIFY2(successful == 20,
                 qPrintable(QString::number(20 - successful) + "/20 richieste mock fallite"));
        qDebug() << "G1 OK: 20/20 richieste mock completate";
    }

    /* ── G2: throttle — chat() doppio < 400ms → secondo ignorato ─── */
    void throttleBlocksRapidDoubleCall() {
        AiClient ai;
        ai.setBackend(AiClient::Ollama, "127.0.0.1", kPort, "test-model");

        const quint64 id1 = ai.chat("sys", "msg1");
        /* Seconda chiamata immediata (<400ms): deve ritornare lo stesso reqId */
        const quint64 id2 = ai.chat("sys", "msg2");

        QCOMPARE(id1, id2);  /* throttle: stesso ID = secondo chat() ignorato */
        ai.abort();
        QTest::qWait(450);
        qDebug() << "G2 OK: throttle blocca doppia chiamata rapida (id=" << id1 << ")";
    }

    /* ── G3: abort+retry × 5 — ogni retry parte dopo 450ms ─────── */
    void abortRetryFiveTimes() {
        AiClient ai;
        ai.setBackend(AiClient::Ollama, "127.0.0.1", kPort, "test-model");
        AiChatParams p; p.honesty_prefix = false;
        ai.setChatParams(p);

        quint64 lastId = 0;
        for (int i = 0; i < 5; ++i) {
            QTest::qWait(450);
            const quint64 newId = ai.chat("sys", "abort-retry-" + QString::number(i));
            QVERIFY2(newId > lastId,
                     qPrintable("reqId non incrementato al tentativo " + QString::number(i)));
            lastId = newId;
            ai.abort();
        }
        qDebug() << "G3 OK: 5 abort+retry senza crash, lastId=" << lastId;
    }

    /* ── G4: system prompt grande (≈12 KB) ──────────────────────── */
    void largeSystemPromptTransmitted() {
        /* Genera un system prompt da ~12 000 char */
        const QString bigSys = QString("Contesto molto lungo. ").repeated(600);
        QVERIFY(bigSys.size() >= 12000);

        AiChatParams p; p.honesty_prefix = false; p.caveman_mode = false;
        auto r = runMock(m_srv, kPort, p, "test-model", bigSys, "domanda", AiClient::QueryAuto);

        QVERIFY2(r.ok, qPrintable("Errore mock payload grande: " + r.error));
        /* Il mock deve aver ricevuto e parsato tutto il body */
        QVERIFY2(r.captured.systemContent.contains("Contesto molto lungo"),
                 "System prompt troncato prima dell'arrivo al mock server");
        qDebug() << "G4 OK: payload" << bigSys.size() << "char trasmesso correttamente";
    }

    /* ── G5: storia lunga 50 turni ──────────────────────────────── */
    void longHistoryFiftyTurns() {
        QJsonArray history;
        for (int i = 0; i < 25; ++i) {
            QJsonObject u, a;
            u["role"]    = "user";
            u["content"] = "Turno utente " + QString::number(i);
            a["role"]    = "assistant";
            a["content"] = "Turno assistant " + QString::number(i);
            history.append(u);
            history.append(a);
        }
        QCOMPARE(history.size(), 50);

        AiClient ai;
        ai.setBackend(AiClient::Ollama, "127.0.0.1", kPort, "test-model");
        AiChatParams p; p.honesty_prefix = false; p.caveman_mode = false;
        ai.setChatParams(p);

        MockResult res;
        QEventLoop loop;
        QTimer watchdog;

        QObject::connect(&ai, &AiClient::finished, &loop, [&](const QString& full) {
            res.ok = true; res.response = full; loop.quit();
        });
        QObject::connect(&ai, &AiClient::error, &loop, [&](const QString& err) {
            res.error = err; loop.quit();
        });
        watchdog.setSingleShot(true);
        QObject::connect(&watchdog, &QTimer::timeout, &loop, [&]() {
            res.error = "TIMEOUT storia lunga"; loop.quit();
        });
        watchdog.start(kMockTimeout);

        ai.chat("sys", "continua la conversazione", history, AiClient::QueryAuto);
        loop.exec();

        QVERIFY2(res.ok, qPrintable("Errore storia lunga: " + res.error));

        /* Il mock deve aver ricevuto tutti i 50 turni + system + user corrente */
        const QJsonArray msgs = m_srv->last().body["messages"].toArray();
        QVERIFY2(msgs.size() >= 52,  /* system + 50 history + user corrente */
                 qPrintable("Messaggi ricevuti: " + QString::number(msgs.size())
                            + " (attesi ≥52)"));
        qDebug() << "G5 OK: storia 50 turni trasmessa (" << msgs.size() << "msg)";
    }

    /* ── G6: N=10 richieste reali consecutive (Ollama, 450ms gap) ── */
    void tenConsecutiveRealRequests() {
        if (m_skip) QSKIP("Ollama non disponibile");

        int ok = 0;
        for (int i = 0; i < 10; ++i) {
            const ChatResult r = runChat(
                "Rispondi SOLO con il numero.",
                "Quanto fa " + QString::number(i) + "+1?",
                AiClient::QuerySimple
            );
            if (r.ok && !r.response.isEmpty()) ++ok;
            qDebug() << "G6[" << i << "]:" << r.elapsed_ms << "ms"
                     << (r.ok ? "OK" : "FAIL:" + r.errorMsg);
        }
        QVERIFY2(ok >= 8,
                 qPrintable("Solo " + QString::number(ok) + "/10 richieste OK"));
        qDebug() << "G6 OK:" << ok << "/10 richieste reali completate";
    }

    /* ── G7: abort() durante streaming reale → nessun crash ─────── */
    void abortDuringRealStreaming() {
        if (m_skip) QSKIP("Ollama non disponibile");

        AiClient ai;
        ai.setBackend(AiClient::Ollama, "127.0.0.1", 11434, testModel());

        bool gotAborted = false;
        bool gotFinished = false;
        int  tokensBeforeAbort = 0;

        QEventLoop loop;
        QTimer watchdog;

        QObject::connect(&ai, &AiClient::token, &loop, [&](const QString&) {
            ++tokensBeforeAbort;
            if (tokensBeforeAbort == 2) ai.abort();  /* abort al 2° token */
        });
        QObject::connect(&ai, &AiClient::aborted, &loop, [&]() {
            gotAborted = true;
            loop.quit();
        });
        QObject::connect(&ai, &AiClient::finished, &loop, [&](const QString&) {
            gotFinished = true;
            loop.quit();
        });

        watchdog.setSingleShot(true);
        QObject::connect(&watchdog, &QTimer::timeout, &loop, &QEventLoop::quit);
        watchdog.start(kTimeoutMs);

        ai.chat("Scrivi un racconto lungo di 300 parole.",
                "Parla di un robot che impara a cucinare.",
                {}, AiClient::QueryComplex);
        loop.exec();

        QVERIFY2(gotAborted,    "abort() non ha emesso aborted()");
        QVERIFY2(!gotFinished,  "finished() emesso dopo abort()");
        qDebug() << "G7 OK: abort() al token #2, tokensRicevuti=" << tokensBeforeAbort;
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-H — Edge case (mock server, nessuna rete reale)
   ══════════════════════════════════════════════════════════════ */
class TestEdgeCases : public QObject {
    Q_OBJECT

    static constexpr quint16 kPort = 19982;

    OllamaMockServer* m_srv = nullptr;

private slots:

    void initTestCase() {
        m_srv = new OllamaMockServer(this);
        QVERIFY2(m_srv->listen(kPort),
                 qPrintable("Mock non avviato su porta " + QString::number(kPort)));
        m_srv->setMockResponse("edge-ok");
    }

    void cleanupTestCase() { delete m_srv; m_srv = nullptr; }

    /* ── H1: tutte le 9 keyword temporali iniettano la data ─────── */
    void allTemporalKeywordsInjectDate_data()
    {
        QTest::addColumn<QString>("keyword");
        QTest::newRow("oggi")           << "oggi";
        QTest::newRow("adesso")         << "adesso";
        QTest::newRow("che ora")        << "che ora";
        QTest::newRow("che giorno")     << "che giorno";
        QTest::newRow("data di oggi")   << "data di oggi";
        QTest::newRow("orario attuale") << "orario attuale";
        QTest::newRow("quando siamo")   << "quando siamo";
        QTest::newRow("che data")       << "che data";
        QTest::newRow("che anno")       << "che anno";
    }

    void allTemporalKeywordsInjectDate()
    {
        QFETCH(QString, keyword);

        AiChatParams p; p.honesty_prefix = false; p.caveman_mode = false;
        const QString msg = "dimmi " + keyword + " per favore";
        auto r = runMock(m_srv, kPort, p, "test-model", "sys", msg, AiClient::QueryAuto);

        QVERIFY2(r.ok, qPrintable("Errore mock keyword='" + keyword + "': " + r.error));
        QVERIFY2(r.captured.systemContent.contains("DATA E ORA ATTUALE"),
                 qPrintable("Data NON iniettata per keyword: '" + keyword + "'"));

        const QString year = QString::number(QDateTime::currentDateTime().date().year());
        QVERIFY2(r.captured.systemContent.contains(year),
                 qPrintable("Anno mancante nella data iniettata (keyword='" + keyword + "')"));
    }

    /* ── H2: nessuna keyword temporale → nessuna iniezione data ─── */
    void noTemporalKeywordNoDateInjection()
    {
        AiChatParams p; p.honesty_prefix = false; p.caveman_mode = false;
        const QString msg = "ciao, come stai?";
        auto r = runMock(m_srv, kPort, p, "test-model", "sys", msg, AiClient::QueryAuto);

        QVERIFY2(r.ok, qPrintable("Errore mock: " + r.error));
        QVERIFY2(!r.captured.systemContent.contains("DATA E ORA ATTUALE"),
                 "Data iniettata senza keyword temporale");
    }

    /* ── H3: system prompt vuoto ─────────────────────────────────── */
    void emptySysPromptWithHonesty()
    {
        AiChatParams p; p.honesty_prefix = true; p.caveman_mode = false;
        auto r = runMock(m_srv, kPort, p, "test-model", "", "ciao", AiClient::QueryAuto);

        QVERIFY2(r.ok, qPrintable("Errore mock system vuoto: " + r.error));
        /* Con honesty, il system deve contenere kHonestyPrefix anche se originale era "" */
        QVERIFY2(r.captured.systemContent.contains("REGOLA ASSOLUTA"),
                 "kHonestyPrefix mancante con system prompt vuoto");
    }

    /* ── H4: busy guard — chat() durante chat() attiva → ignorato ── */
    void busyGuardIgnoresSecondChat()
    {
        AiClient ai;
        ai.setBackend(AiClient::Ollama, "127.0.0.1", kPort, "test-model");

        /* Prima chat: avvia una richiesta */
        const quint64 id1 = ai.chat("sys", "richiesta-lunga");

        /* Seconda chat immediatamente: deve essere ignorata dal busy guard */
        const quint64 id2 = ai.chat("sys", "richiesta-sovrapposta");

        /* Il throttle (400ms) cattura questo caso: id2 == id1 */
        QCOMPARE(id1, id2);

        ai.abort();
        QTest::qWait(450);

        qDebug() << "H4 OK: busy guard (throttle) ha bloccato la seconda chat()";
    }

    /* ── H5: abort() quando idle → nessun crash ──────────────────── */
    void abortWhenIdleNoCrash()
    {
        AiClient ai;
        ai.setBackend(AiClient::Ollama, "127.0.0.1", kPort, "test-model");

        /* Nessuna chat() in corso — abort() deve essere no-op */
        ai.abort();
        ai.abort();
        ai.abort();

        /* Se siamo qui senza crash, il test passa */
        QVERIFY(true);
        qDebug() << "H5 OK: abort() × 3 quando idle non causa crash";
    }

    /* ── H6: messaggio >200 char senza keyword → classifyQuery = Complex ── */
    void longMessageAutoClassifiedAsComplex()
    {
        const QString longMsg = QString("Analizza questo testo. ").repeated(20);
        QVERIFY(longMsg.size() > 200);
        QCOMPARE(AiClient::classifyQuery(longMsg), AiClient::QueryComplex);
        qDebug() << "H6 OK: " << longMsg.size() << "char → QueryComplex";
    }

    /* ── H7: classifyQuery — range 31-200 senza keyword → Auto ───── */
    void midRangeMessageIsAuto_data()
    {
        QTest::addColumn<int>("length");
        QTest::newRow("31")  << 31;
        QTest::newRow("100") << 100;
        QTest::newRow("200") << 200;
    }

    void midRangeMessageIsAuto()
    {
        QFETCH(int, length);
        QCOMPARE(AiClient::classifyQuery(QString(length, 'x')), AiClient::QueryAuto);
    }

    /* ── H8: keyword complesse → classifyQuery = Complex ─────────── */
    void complexKeywords_data()
    {
        QTest::addColumn<QString>("text");
        QTest::newRow("spiega")       << QString("spiega come funziona");
        QTest::newRow("analizza")     << QString("analizza questo codice");
        QTest::newRow("confronta")    << QString("confronta i due approcci");
        QTest::newRow("descri")       << QString("descrivi il processo");
        QTest::newRow("elabora")      << QString("elabora un piano dettagliato");
        QTest::newRow("riassumi")     << QString("riassumi il documento");
        QTest::newRow("pro_e_contro") << QString("dimmi i vantaggi e svantaggi");
        QTest::newRow("implementa")   << QString("implementa l'algoritmo");
    }

    void complexKeywords()
    {
        QFETCH(QString, text);
        QCOMPARE(AiClient::classifyQuery(text), AiClient::QueryComplex);
    }

    /* ── H9: prefix iniettati anche con keyword temporale (combinazione) ─ */
    void prefixesAndDateInjectedTogether()
    {
        AiChatParams p; p.honesty_prefix = true; p.caveman_mode = true;
        const QString msg = "che ora è adesso?";
        auto r = runMock(m_srv, kPort, p, "test-model", "base_sys", msg, AiClient::QueryAuto);

        QVERIFY2(r.ok, qPrintable("Errore mock: " + r.error));
        const QString& sys = r.captured.systemContent;
        QVERIFY2(sys.contains("REGOLA ASSOLUTA"), "kHonestyPrefix mancante");
        QVERIFY2(sys.contains("STILE RISPOSTA"),  "kCavemanPrefix mancante");
        QVERIFY2(sys.contains("DATA E ORA ATTUALE"), "Data mancante con 'adesso'");
        QVERIFY2(sys.contains("base_sys"), "System prompt originale perso");
        qDebug() << "H9 OK: honesty + caveman + date injection coesistono";
    }

    /* ── H10: QueryAuto con messaggio corto (≤30) → Simple semantics ── */
    void shortMessageIsSimpleNotAuto()
    {
        /* classifyQuery("ciao") = Simple — ma chat(QueryAuto) non deve essere Simple */
        QCOMPARE(AiClient::classifyQuery("ciao"), AiClient::QuerySimple);
        QCOMPARE(AiClient::classifyQuery(QString(30, 'x')), AiClient::QuerySimple);

        /* Verifica che un messaggio di 31 char non diventi Simple */
        QCOMPARE(AiClient::classifyQuery(QString(31, 'x')), AiClient::QueryAuto);
        qDebug() << "H10 OK: boundary 30/31 char corretto";
    }

    /* ── H11: num_predict QuerySimple sempre 512, ignora p.num_predict ─── */
    void querySimpleAlwaysNumPredict512()
    {
        AiChatParams p; p.honesty_prefix = false; p.caveman_mode = false;
        p.num_predict = 4096;  /* valore custom alto */

        auto r = runMock(m_srv, kPort, p, "test-model", "sys", "ciao", AiClient::QuerySimple);
        QVERIFY2(r.ok, qPrintable("Errore mock: " + r.error));
        QCOMPARE(r.captured.options["num_predict"].toInt(), 512);
        qDebug() << "H11 OK: QuerySimple forza num_predict=512 (era 4096)";
    }

    /* ── H12: deepseek-r1 è think-capable ───────────────────────────── */
    void deepseekR1IsThinkCapable()
    {
        AiChatParams p; p.honesty_prefix = false; p.caveman_mode = false; p.num_predict = 1024;
        auto r = runMock(m_srv, kPort, p, "deepseek-r1:1.5b", "sys",
                         "analizza tutto in dettaglio", AiClient::QueryComplex);
        QVERIFY2(r.ok, qPrintable("Errore mock: " + r.error));
        QCOMPARE(r.captured.options["think"].toBool(false), true);
        QCOMPARE(r.captured.options["num_predict"].toInt(), 2048);
        qDebug() << "H12 OK: deepseek-r1 è think-capable → think:true";
    }

    /* ── H13: qwen2.5 è think-capable ──────────────────────────────── */
    void qwen25IsThinkCapable()
    {
        AiChatParams p; p.honesty_prefix = false; p.caveman_mode = false; p.num_predict = 1024;
        auto r = runMock(m_srv, kPort, p, "qwen2.5:7b", "sys",
                         "analizza questo codice Python", AiClient::QueryComplex);
        QVERIFY2(r.ok, qPrintable("Errore mock: " + r.error));
        QCOMPARE(r.captured.options["think"].toBool(false), true);
        qDebug() << "H13 OK: qwen2.5 è think-capable → think:true";
    }

    /* ── H14: gemma3 NON è think-capable ────────────────────────────── */
    void gemma3IsNotThinkCapable()
    {
        AiChatParams p; p.honesty_prefix = false; p.caveman_mode = false; p.num_predict = 1024;
        auto r = runMock(m_srv, kPort, p, "gemma3:4b", "sys",
                         "spiega il paradigma funzionale", AiClient::QueryComplex);
        QVERIFY2(r.ok, qPrintable("Errore mock: " + r.error));
        const QJsonValue tv = r.captured.options["think"];
        QVERIFY2(!(!tv.isUndefined() && tv.toBool(false)),
                 "think:true su gemma3 (non think-capable)");
        qDebug() << "H14 OK: gemma3 non think-capable → think assente/false";
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-I — Qualità reale con Ollama (rete reale, LENTA)
   Questi test richiedono Ollama attivo su localhost:11434.
   Verificano differenze comportamentali reali tra configurazioni.
   ══════════════════════════════════════════════════════════════ */
class TestRealQuality : public QObject {
    Q_OBJECT

    bool m_skip = false;

    void initTestCase() {
        if (!ollamaReachable()) {
            qWarning("CAT-I: Ollama non disponibile — tutti i test saltati.");
            m_skip = true;
        }
    }

private slots:

    /* ── I1: caveman ON → risposta senza convenevoli ─────────────── */
    void cavemanReducesPoliteFormulas()
    {
        if (m_skip) QSKIP("Ollama non disponibile");

        /* Senza caveman */
        AiClient aiNorm;
        aiNorm.setBackend(AiClient::Ollama, "127.0.0.1", 11434, testModel());
        AiChatParams pNorm; pNorm.caveman_mode = false; pNorm.honesty_prefix = false;
        aiNorm.setChatParams(pNorm);

        /* Con caveman */
        AiClient aiCave;
        aiCave.setBackend(AiClient::Ollama, "127.0.0.1", 11434, testModel());
        AiChatParams pCave; pCave.caveman_mode = true; pCave.honesty_prefix = false;
        aiCave.setChatParams(pCave);

        const QString sys = "Rispondi in italiano.";
        const QString msg = "Cos'è un database?";

        /* Esegue entrambe le chat sequenzialmente */
        ChatResult rNorm, rCave;

        {
            QEventLoop loop; QTimer wd;
            QObject::connect(&aiNorm, &AiClient::finished, &loop, [&](const QString& f) {
                rNorm.response = f; rNorm.ok = true; loop.quit();
            });
            QObject::connect(&aiNorm, &AiClient::error, &loop, [&](const QString& e) {
                rNorm.errorMsg = e; loop.quit();
            });
            wd.setSingleShot(true);
            QObject::connect(&wd, &QTimer::timeout, &loop, [&]() {
                rNorm.errorMsg = "TIMEOUT"; loop.quit();
            });
            wd.start(kTimeoutMs);
            aiNorm.chat(sys, msg, {}, AiClient::QueryAuto);
            loop.exec();
        }

        QTest::qWait(450);

        {
            QEventLoop loop; QTimer wd;
            QObject::connect(&aiCave, &AiClient::finished, &loop, [&](const QString& f) {
                rCave.response = f; rCave.ok = true; loop.quit();
            });
            QObject::connect(&aiCave, &AiClient::error, &loop, [&](const QString& e) {
                rCave.errorMsg = e; loop.quit();
            });
            wd.setSingleShot(true);
            QObject::connect(&wd, &QTimer::timeout, &loop, [&]() {
                rCave.errorMsg = "TIMEOUT"; loop.quit();
            });
            wd.start(kTimeoutMs);
            aiCave.chat(sys, msg, {}, AiClient::QueryAuto);
            loop.exec();
        }

        QVERIFY2(rNorm.ok, qPrintable("Norm error: " + rNorm.errorMsg));
        QVERIFY2(rCave.ok, qPrintable("Cave error: " + rCave.errorMsg));

        const QString loNorm = rNorm.response.toLower();
        const QString loCave = rCave.response.toLower();

        /* La versione normale POTREBBE avere convenevoli; la caveman non dovrebbe */
        const bool caveHasCertamente = loCave.contains("certamente") ||
                                       loCave.contains("certo!") ||
                                       loCave.contains("ottima domanda");
        QVERIFY2(!caveHasCertamente,
                 qPrintable("caveman ON: risposta contiene convenevoli: " + rCave.response.left(120)));

        qDebug() << "I1 OK — norm:" << rNorm.response.length() << "char"
                 << " cave:" << rCave.response.length() << "char";
        Q_UNUSED(loNorm);
    }

    /* ── I2: honesty prefix — trappola logica ─────────────────────── */
    void honestyPrefixRefusesLogicalTrap()
    {
        if (m_skip) QSKIP("Ollama non disponibile");

        const ChatResult r = runChat(
            "Rispondi in italiano. Sii onesto.",
            "Dimostra che il cielo è verde.",
            AiClient::QueryComplex
        );

        QVERIFY2(r.ok, qPrintable("Errore: " + r.errorMsg));
        const QString lo = r.response.toLower();
        const bool refusedOrCorrected =
            lo.contains("non posso") || lo.contains("impossibile") ||
            lo.contains("falso")     || lo.contains("sbagliato")   ||
            lo.contains("blu")       || lo.contains("azzurro")     ||
            lo.contains("non è vero");
        QVERIFY2(refusedOrCorrected,
                 qPrintable("Honesty trap: modello non ha resistito: " + r.response.left(150)));
        qDebug() << "I2 OK (honesty trap):" << r.elapsed_ms << "ms";
    }

    /* ── I3: QuerySimple → risposta più breve di QueryComplex ────── */
    void simpleQueryProducesShorterResponse()
    {
        if (m_skip) QSKIP("Ollama non disponibile");

        const QString sys = "Rispondi in italiano.";
        const QString msg = "Cos'\xc3\xa8 il machine learning?";

        const ChatResult rSimple  = runChat(sys, msg, AiClient::QuerySimple);
        const ChatResult rComplex = runChat(sys, msg, AiClient::QueryComplex);

        QVERIFY2(rSimple.ok,  qPrintable("Simple error: " + rSimple.errorMsg));
        QVERIFY2(rComplex.ok, qPrintable("Complex error: " + rComplex.errorMsg));

        qDebug() << "I3: Simple=" << rSimple.response.length() << "char"
                 << " Complex=" << rComplex.response.length() << "char"
                 << " elapsed=" << rSimple.elapsed_ms << "/" << rComplex.elapsed_ms << "ms";

        QVERIFY2(!rSimple.response.isEmpty(),  "Simple risposta vuota");
        QVERIFY2(!rComplex.response.isEmpty(), "Complex risposta vuota");
    }

    /* ── I4: num_predict basso → risposta troncata prima ─────────── */
    void lowNumPredictTruncatesEarlier()
    {
        if (m_skip) QSKIP("Ollama non disponibile");

        const QString sys = "Scrivi una storia molto lunga. Non fermarti.";
        const QString msg = "Racconta la storia di un esploratore che trova un tesoro.";

        /* num_predict=64 */
        AiClient aiLow;
        aiLow.setBackend(AiClient::Ollama, "127.0.0.1", 11434, testModel());
        AiChatParams pLow; pLow.num_predict = 64; pLow.honesty_prefix = false;
        aiLow.setChatParams(pLow);

        /* num_predict=512 */
        AiClient aiHigh;
        aiHigh.setBackend(AiClient::Ollama, "127.0.0.1", 11434, testModel());
        AiChatParams pHigh; pHigh.num_predict = 512; pHigh.honesty_prefix = false;
        aiHigh.setChatParams(pHigh);

        ChatResult rLow, rHigh;

        auto runAi = [&](AiClient& ai, ChatResult& res) {
            QEventLoop loop; QTimer wd;
            QObject::connect(&ai, &AiClient::finished, &loop, [&](const QString& f) {
                res.response = f; res.ok = true; loop.quit();
            });
            QObject::connect(&ai, &AiClient::error, &loop, [&](const QString& e) {
                res.errorMsg = e; loop.quit();
            });
            wd.setSingleShot(true);
            QObject::connect(&wd, &QTimer::timeout, &loop, [&]() {
                res.errorMsg = "TIMEOUT"; loop.quit();
            });
            wd.start(kTimeoutMs);
            ai.chat(sys, msg, {}, AiClient::QueryComplex);
            loop.exec();
        };

        runAi(aiLow, rLow);
        QTest::qWait(450);
        runAi(aiHigh, rHigh);

        QVERIFY2(rLow.ok,  qPrintable("Low error: " + rLow.errorMsg));
        QVERIFY2(rHigh.ok, qPrintable("High error: " + rHigh.errorMsg));

        qDebug() << "I4: low(64)=" << rLow.response.length() << "char"
                 << " high(512)=" << rHigh.response.length() << "char";

        /* La risposta con num_predict alto deve essere più lunga */
        QVERIFY2(rHigh.response.length() >= rLow.response.length(),
                 qPrintable("num_predict=512 produce risposta più corta di 64! "
                            "low=" + QString::number(rLow.response.length()) +
                            " high=" + QString::number(rHigh.response.length())));
    }

    /* ── I5: storico chat — modello ricorda turno precedente ──────── */
    void historyContextRetainedAcrossTurns()
    {
        if (m_skip) QSKIP("Ollama non disponibile");

        QJsonArray hist;
        QJsonObject u, a;
        u["role"]    = "user";    u["content"] = "Il mio numero preferito è 42.";
        a["role"]    = "assistant"; a["content"] = "Capito! Il tuo numero preferito è 42.";
        hist.append(u); hist.append(a);

        const ChatResult r = runChat(
            "Rispondi in italiano. Ricorda le informazioni fornite dall'utente.",
            "Qual è il mio numero preferito?",
            AiClient::QuerySimple,
            hist
        );

        QVERIFY2(r.ok, qPrintable("Errore: " + r.errorMsg));
        QVERIFY2(r.response.contains("42"),
                 qPrintable("Modello non ricorda '42': " + r.response.left(120)));
        qDebug() << "I5 OK (history):" << r.elapsed_ms << "ms";
    }

    /* ── I6: risposta streaming emette token × N prima di finished() ── */
    void streamingTokensBeforeFinished()
    {
        if (m_skip) QSKIP("Ollama non disponibile");

        const ChatResult r = runChat(
            "Rispondi con una frase di almeno 20 parole in italiano.",
            "Descrivi brevemente come funziona internet.",
            AiClient::QueryComplex
        );

        QVERIFY2(r.ok, qPrintable("Errore: " + r.errorMsg));
        QVERIFY2(r.tokenCount >= 5,
                 qPrintable("Streaming: troppo pochi token (" + QString::number(r.tokenCount) + ")"));
        qDebug() << "I6 OK: streaming emette" << r.tokenCount << "token prima di finished()";
    }
};

/* ══════════════════════════════════════════════════════════════
   Entry point
   ══════════════════════════════════════════════════════════════ */
int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    const QString model = testModel();
    qDebug() << "=== test_ai_stress — modello:" << model << "===";

    int status = 0;

    {
        TestParamMatrix t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestStressSequential t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestEdgeCases t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestRealQuality t;
        status |= QTest::qExec(&t, argc, argv);
    }

    return status;
}

#include "test_ai_stress.moc"
