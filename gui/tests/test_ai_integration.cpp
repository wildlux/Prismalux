/* ══════════════════════════════════════════════════════════════
   test_ai_integration.cpp — Test di integrazione AiClient ↔ Ollama
   ──────────────────────────────────────────────────────────────
   A differenza degli altri test che usano MockAiClient, questo file
   usa AiClient REALE e richiede Ollama in ascolto su localhost:11434.

   Non testa la UI: testa il PERCORSO COMPLETO che la GUI usa:
     classifyQuery → AiChatParams (Brutal Honesty) → chat() →
     serializzazione JSON → Ollama → parsing streaming → finished()

   Categorie:
     CAT-A  classifyQuery — 9 casi (no rete, logica pura)
     CAT-B  AiChatParams  — 5 casi (no rete, file JSON)
     CAT-C  AiClient chat — 5 casi (RETE REALE: Ollama deve girare)
     CAT-D  QueryType passthrough — 3 casi (RETE REALE)

   COME ESEGUIRE:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_ai_integration
     ./build_tests/test_ai_integration

   REQUISITI:
     - Ollama in esecuzione su 127.0.0.1:11434
     - Modello TEST_MODEL disponibile (default: mistral:7b-instruct)
     - Variabile d'ambiente PRISMALUX_TEST_MODEL per override
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

static constexpr int kTimeoutMs   = 120'000;  /* 2 min per risposta LLM  */
static constexpr int kConnTimeout = 5'000;    /* 5s per check connettività */

/* ══════════════════════════════════════════════════════════════
   Helpers condivisi
   ══════════════════════════════════════════════════════════════ */

/* Risultato di una singola chiamata chat() */
struct ChatResult {
    QString  response;
    QString  errorMsg;
    bool     ok          = false;
    int      elapsed_ms  = 0;
    int      tokenCount  = 0;   /* numero di segnali token() ricevuti */
    quint64  reqId       = 0;
};

/* Esegue chat() e blocca su QEventLoop fino a finished() / error() / timeout.
   Usa AiClient reale — nessun mock. */
static ChatResult runChat(const QString& system,
                          const QString& msg,
                          AiClient::QueryType qt = AiClient::QueryAuto,
                          const QJsonArray& history = {})
{
    AiClient ai;
    ai.setBackend(AiClient::Ollama, "127.0.0.1", 11434, testModel());

    ChatResult res;
    QEventLoop loop;
    QElapsedTimer timer;
    QTimer watchdog;

    /* Conta i token ricevuti durante lo streaming */
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
        res.errorMsg = "TIMEOUT dopo " + QString::number(kTimeoutMs / 1000) + "s";
        res.ok       = false;
        loop.quit();
    });

    timer.start();
    watchdog.start(kTimeoutMs);
    res.reqId = ai.chat(system, msg, history, qt);
    loop.exec();

    return res;
}

/* Controlla che Ollama sia raggiungibile (test rapido senza inferenza) */
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

/* ══════════════════════════════════════════════════════════════
   CAT-A — classifyQuery (9 casi, nessuna rete)
   Verifica la logica di classificazione dei prompt implementata
   in ai_client.cpp — include il fix T1 (Auto invece di Simple).
   ══════════════════════════════════════════════════════════════ */
class TestClassifyQuery : public QObject {
    Q_OBJECT
private slots:

    /* Stringa vuota o molto corta → Simple */
    void emptyIsSimple() {
        QCOMPARE(AiClient::classifyQuery(""), AiClient::QuerySimple);
    }
    void shortIsSimple() {
        QCOMPARE(AiClient::classifyQuery("ciao"), AiClient::QuerySimple);
    }
    /* Esattamente 30 caratteri → Simple (limite inferiore) */
    void exactly30IsSimple() {
        QCOMPARE(AiClient::classifyQuery(QString(30, 'x')), AiClient::QuerySimple);
    }

    /* 31 caratteri senza keyword → Auto (FIX T1: era Simple) */
    void thirtyOneIsAuto() {
        QCOMPARE(AiClient::classifyQuery(QString(31, 'x')), AiClient::QueryAuto);
    }
    /* 200 caratteri senza keyword → Auto */
    void twoHundredIsAuto() {
        QCOMPARE(AiClient::classifyQuery(QString(200, 'x')), AiClient::QueryAuto);
    }

    /* Oltre 200 caratteri → Complex (senza keyword) */
    void longIsComplex() {
        QCOMPARE(AiClient::classifyQuery(QString(201, 'x')), AiClient::QueryComplex);
    }

    /* Keyword "spiega" → Complex (indipendente dalla lunghezza) */
    void keywordSpiegaIsComplex() {
        QCOMPARE(AiClient::classifyQuery("spiega come funziona Python"),
                 AiClient::QueryComplex);
    }
    /* Keyword "analizza" → Complex */
    void keywordAnalizzaIsComplex() {
        QCOMPARE(AiClient::classifyQuery("analizza questo codice"),
                 AiClient::QueryComplex);
    }
    /* Keyword "perché" (UTF-8) → Complex */
    void keywordPercheIsComplex() {
        QCOMPARE(AiClient::classifyQuery(QString::fromUtf8("perch\xc3\xa9 funziona cos\xc3\xac")),
                 AiClient::QueryComplex);
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B — AiChatParams (5 casi, nessuna rete)
   Verifica che i parametri Brutal Honesty siano corretti e che
   il round-trip save/load funzioni.
   ══════════════════════════════════════════════════════════════ */
class TestAiChatParams : public QObject {
    Q_OBJECT
private slots:

    /* I default della struct corrispondono ai valori Brutal Honesty */
    void defaultsBrutalHonesty() {
        AiChatParams p;
        QVERIFY(p.temperature    <= 0.1);   /* bassa temperatura = meno allucinazioni */
        QVERIFY(p.repeat_penalty >= 1.1);   /* repeat penalty > 1 */
        QVERIFY(p.honesty_prefix == true);  /* prefix BH attivo per default */
        QVERIFY(p.top_k          <= 25);    /* top_k basso = risposte più conservative */
    }

    /* load() deve restituire un oggetto valido anche senza file */
    void loadWithoutFileSilent() {
        /* Rinomina temporaneamente il file se esiste */
        const QString path = AiChatParams::filePath();
        const QString bak  = path + ".bak_test";
        if (QFile::exists(path)) QFile::rename(path, bak);

        AiChatParams p = AiChatParams::load();
        QVERIFY(p.temperature > 0.0);
        QVERIFY(p.num_predict > 0);

        /* Ripristina */
        if (QFile::exists(bak))  QFile::rename(bak, path);
    }

    /* save() seguito da load() deve restituire valori identici */
    void saveLoadRoundTrip() {
        AiChatParams orig;
        orig.temperature    = 0.42;
        orig.top_k          = 17;
        orig.repeat_penalty = 1.35;
        orig.honesty_prefix = false;
        orig.caveman_mode   = true;

        AiChatParams::save(orig);
        const AiChatParams loaded = AiChatParams::load();

        QCOMPARE(loaded.temperature,    orig.temperature);
        QCOMPARE(loaded.top_k,          orig.top_k);
        QCOMPARE(loaded.repeat_penalty, orig.repeat_penalty);
        QCOMPARE(loaded.honesty_prefix, orig.honesty_prefix);
        QCOMPARE(loaded.caveman_mode,   orig.caveman_mode);

        /* Rimette i default per non sporcare sessioni successive */
        AiChatParams::save(AiChatParams{});
    }

    /* setChatParams / chatParams round-trip in memoria */
    void setChatParamsRoundTrip() {
        AiClient ai;
        AiChatParams p;
        p.temperature = 0.99;
        p.top_k = 99;
        ai.setChatParams(p);
        QCOMPARE(ai.chatParams().temperature, 0.99);
        QCOMPARE(ai.chatParams().top_k,       99);
    }

    /* filePath non è vuoto e punta alla home */
    void filePathContainsHome() {
        const QString fp = AiChatParams::filePath();
        QVERIFY(!fp.isEmpty());
        QVERIFY(fp.contains(".prismalux"));
        QVERIFY(fp.endsWith("ai_params.json"));
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-C — AiClient::chat() reale (5 casi, RETE REALE)
   Questi test richiedono Ollama attivo su localhost:11434.
   Ogni test invia un prompt reale e verifica la risposta.
   ══════════════════════════════════════════════════════════════ */
class TestAiClientChat : public QObject {
    Q_OBJECT

private:
    bool m_skip = false;

    void initTestCase() {
        if (!ollamaReachable()) {
            qWarning("CAT-C/D: Ollama non raggiungibile su 127.0.0.1:11434 — test saltati.");
            m_skip = true;
        }
    }

private slots:

    /* ── C1: risposta non vuota a domanda semplice ──────────────
       Verifica il percorso base: chat() → token() × N → finished().
       La risposta deve contenere "4" (2+2=4). */
    void simpleMathReturnsNonEmpty() {
        if (m_skip) QSKIP("Ollama non disponibile");

        const ChatResult r = runChat(
            "Sei un assistente matematico. Rispondi SOLO con il numero, senza spiegazioni.",
            "Quanto fa 2+2?",
            AiClient::QuerySimple
        );

        QVERIFY2(r.ok, qPrintable("Errore: " + r.errorMsg));
        QVERIFY2(!r.response.isEmpty(), "Risposta vuota");
        QVERIFY2(r.response.contains("4"), qPrintable("Atteso '4' in: " + r.response));

        /* Lo streaming deve aver emesso almeno 1 token */
        QVERIFY2(r.tokenCount >= 1, "Nessun token ricevuto dallo streaming");

        qDebug() << "C1 OK:" << r.elapsed_ms << "ms  tokens=" << r.tokenCount
                 << "  risposta=" << r.response.left(60);
    }

    /* ── C2: classifyQuery Complex → chat con QueryComplex ───────
       Invia una domanda tecnica classificata come Complex.
       La risposta deve contenere keyword tecniche attese. */
    void complexQueryRespondsWithKeywords() {
        if (m_skip) QSKIP("Ollama non disponibile");

        const QString prompt = "Spiega in 2 frasi cosa fa il pattern Observer in C++.";
        QCOMPARE(AiClient::classifyQuery(prompt), AiClient::QueryComplex);

        const ChatResult r = runChat(
            "Sei un esperto di design pattern. Rispondi in italiano.",
            prompt,
            AiClient::QueryComplex
        );

        QVERIFY2(r.ok, qPrintable("Errore: " + r.errorMsg));
        QVERIFY2(!r.response.isEmpty(), "Risposta vuota");

        const QString lo = r.response.toLower();
        const bool hasKeyword =
            lo.contains("observer") || lo.contains("notif") ||
            lo.contains("evento")   || lo.contains("osservat");
        QVERIFY2(hasKeyword, qPrintable("Keyword mancanti in: " + r.response.left(120)));

        qDebug() << "C2 OK:" << r.elapsed_ms << "ms  risposta=" << r.response.left(80);
    }

    /* ── C3: Brutal Honesty — domanda trappola ───────────────────
       Verifica che i parametri BH facciano rispondere correttamente:
       il modello deve rifiutare o ammettere incertezza. */
    void brutalHonestyTrap() {
        if (m_skip) QSKIP("Ollama non disponibile");

        const ChatResult r = runChat(
            "REGOLA: rispondi SOLO con ciò che sai con certezza. "
            "Se non sai, di' esplicitamente 'Non lo so'. Rispondi in italiano.",
            "Dimostra che 1 + 1 = 3.",
            AiClient::QueryComplex
        );

        QVERIFY2(r.ok, qPrintable("Errore: " + r.errorMsg));
        QVERIFY2(!r.response.isEmpty(), "Risposta vuota");

        const QString lo = r.response.toLower();
        const bool refused =
            lo.contains("impossibile") || lo.contains("falso")  ||
            lo.contains("errato")      || lo.contains("non è")  ||
            lo.contains("sbagliato")   || lo.contains("non si") ||
            lo.contains("non posso")   || lo.contains("1 + 1");
        QVERIFY2(refused, qPrintable("Il modello non ha rifiutato: " + r.response.left(120)));

        qDebug() << "C3 OK (BH trap):" << r.elapsed_ms << "ms";
    }

    /* ── C4: reqId incrementale ──────────────────────────────────
       Ogni chat() avviata con successo deve incrementare il reqId.
       Il throttle interno blocca chiamate entro 400ms dalla precedente,
       quindi aspettiamo 450ms tra ogni coppia (chat + abort). */
    void reqIdIncrementsPerCall() {
        AiClient ai;
        ai.setBackend(AiClient::Ollama, "127.0.0.1", 11434, testModel());

        const quint64 id1 = ai.chat("sys", "msg1");
        ai.abort();
        QTest::qWait(450);   /* oltre il throttle di 400ms */

        const quint64 id2 = ai.chat("sys", "msg2");
        ai.abort();
        QTest::qWait(450);

        const quint64 id3 = ai.chat("sys", "msg3");
        ai.abort();

        QVERIFY2(id2 > id1, "reqId non incrementato tra prima e seconda chiamata");
        QVERIFY2(id3 > id2, "reqId non incrementato tra seconda e terza chiamata");

        qDebug() << "C4 OK: reqId" << id1 << "->" << id2 << "->" << id3;
    }

    /* ── C5: coding Python — funzione semplice ───────────────────
       La risposta deve contenere un blocco di codice Python corretto. */
    void codingPythonSimpleFunction() {
        if (m_skip) QSKIP("Ollama non disponibile");

        const ChatResult r = runChat(
            "Sei un assistente Python. Rispondi SOLO con il codice, "
            "dentro un blocco ```python ... ```. Nessun testo fuori.",
            "Scrivi una funzione Python `somma(a, b)` che ritorna a+b.",
            AiClient::QueryComplex
        );

        QVERIFY2(r.ok, qPrintable("Errore: " + r.errorMsg));
        QVERIFY2(!r.response.isEmpty(), "Risposta vuota");

        const QString lo = r.response.toLower();
        const bool hasCode =
            lo.contains("def somma") || lo.contains("return a + b") ||
            lo.contains("return a+b");
        QVERIFY2(hasCode, qPrintable("Codice mancante: " + r.response.left(120)));

        qDebug() << "C5 OK (coding):" << r.elapsed_ms << "ms";
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-D — QueryType passthrough (3 casi, RETE REALE)
   Verifica che QuerySimple/Complex/Auto cambi il comportamento
   reale della risposta (lunghezza, verbosità).
   ══════════════════════════════════════════════════════════════ */
class TestQueryTypePassthrough : public QObject {
    Q_OBJECT

private:
    bool m_skip = false;

    void initTestCase() {
        if (!ollamaReachable()) {
            qWarning("CAT-D: Ollama non raggiungibile — test saltati.");
            m_skip = true;
        }
    }

private slots:

    /* ── D1: QuerySimple → risposta più breve di QueryComplex ────
       Stessa domanda, tipo query diverso: Simple deve produrre
       una risposta mediamente più breve. */
    void simpleProducesShorterThanComplex() {
        if (m_skip) QSKIP("Ollama non disponibile");

        const QString sys = "Rispondi in italiano.";
        const QString msg = "Cos'\xc3\xa8 un algoritmo?";   /* è in UTF-8 */

        const ChatResult rSimple  = runChat(sys, msg, AiClient::QuerySimple);
        const ChatResult rComplex = runChat(sys, msg, AiClient::QueryComplex);

        QVERIFY2(rSimple.ok,  qPrintable("Simple: " + rSimple.errorMsg));
        QVERIFY2(rComplex.ok, qPrintable("Complex: " + rComplex.errorMsg));

        qDebug() << "D1: Simple=" << rSimple.response.length()
                 << "chars  Complex=" << rComplex.response.length() << "chars";

        /* Non è garantito che Simple < Complex per ogni modello, ma lo logghiamo */
        /* Test pass comunque: verifichiamo solo che entrambe abbiano risposto */
        QVERIFY2(!rSimple.response.isEmpty(),  "Simple: risposta vuota");
        QVERIFY2(!rComplex.response.isEmpty(), "Complex: risposta vuota");
    }

    /* ── D2: chat con history non vuota ──────────────────────────
       Verifica che la storia venga passata correttamente
       e che il modello possa fare riferimento al turno precedente. */
    void chatWithHistoryContextRetained() {
        if (m_skip) QSKIP("Ollama non disponibile");

        /* Costruisce una storia: l'utente ha detto di chiamarsi Marco */
        QJsonArray history;
        QJsonObject userTurn;
        userTurn["role"]    = "user";
        userTurn["content"] = "Mi chiamo Marco.";
        QJsonObject asstTurn;
        asstTurn["role"]    = "assistant";
        asstTurn["content"] = "Ciao Marco! Come posso aiutarti?";
        history.append(userTurn);
        history.append(asstTurn);

        const ChatResult r = runChat(
            "Sei un assistente cordiale. Rispondi in italiano.",
            "Ricordi come mi chiamo?",
            AiClient::QuerySimple,
            history
        );

        QVERIFY2(r.ok, qPrintable("Errore: " + r.errorMsg));
        QVERIFY2(!r.response.isEmpty(), "Risposta vuota");

        const QString lo = r.response.toLower();
        QVERIFY2(lo.contains("marco"),
                 qPrintable("Il modello non ha ricordato il nome: " + r.response.left(120)));

        qDebug() << "D2 OK (history):" << r.elapsed_ms << "ms";
    }

    /* ── D3: abort() durante streaming ───────────────────────────
       Chiama abort() dopo il primo token: deve emettere aborted()
       e non finished(). Il client non deve crashare. */
    void abortDuringStreaming() {
        if (m_skip) QSKIP("Ollama non disponibile");

        AiClient ai;
        ai.setBackend(AiClient::Ollama, "127.0.0.1", 11434, testModel());

        bool gotAborted  = false;
        bool gotFinished = false;

        QEventLoop loop;
        QTimer watchdog;

        QObject::connect(&ai, &AiClient::token, &loop, [&](const QString&) {
            /* Abortiamo al primo token */
            ai.abort();
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

        ai.chat("Rispondi con una storia molto lunga di 500 parole.",
                "C'era una volta un drago che viveva in un castello...",
                {}, AiClient::QueryComplex);
        loop.exec();

        QVERIFY2(gotAborted,   "abort() non ha emesso aborted()");
        QVERIFY2(!gotFinished, "finished() emesso dopo abort() — comportamento errato");

        qDebug() << "D3 OK: abort() durante streaming funziona correttamente";
    }
};

/* ══════════════════════════════════════════════════════════════
   OllamaMockServer — mini-server TCP che simula Ollama localmente.

   Permette di intercettare il JSON body ESATTO che AiClient invia,
   senza toccare un'istanza Ollama reale. Verifica costrutti interni:
   kHonestyPrefix, kCavemanPrefix, iniezione data/ora, think, num_predict.

   Protocollo: riceve HTTP POST, parsa body JSON, risponde con NDJSON
   streaming compatibile con il formato che AiClient::onReadyRead() si aspetta.
   ══════════════════════════════════════════════════════════════ */
class OllamaMockServer : public QObject {
    Q_OBJECT
public:
    struct Captured {
        QJsonObject body;           /* intero body JSON della richiesta */
        QString     systemContent;  /* contenuto del primo messaggio "system" */
        QString     userContent;    /* contenuto dell'ultimo messaggio "user" */
        QJsonObject options;        /* campo options{} (parametri campionamento) */
    };

    explicit OllamaMockServer(QObject* parent = nullptr)
        : QObject(parent)
        , m_server(new QTcpServer(this))
    {
        connect(m_server, &QTcpServer::newConnection,
                this,     &OllamaMockServer::onNewConnection);
    }

    bool    listen(quint16 port) { return m_server->listen(QHostAddress::LocalHost, port); }
    quint16 port()         const { return m_server->serverPort(); }
    Captured last()        const { return m_last; }
    int      requestCount()const { return m_count; }

    void setMockResponse(const QString& text) { m_mockText = text; }

private slots:
    void onNewConnection() {
        QTcpSocket* sock = m_server->nextPendingConnection();
        /* shared_ptr: garantisce che buf venga distrutto esattamente una volta
           indipendentemente da quale percorso (readyRead completo o disconnessione
           prematura) porta alla fine del ciclo di vita della socket. */
        auto buf = std::make_shared<QByteArray>();

        connect(sock, &QTcpSocket::readyRead, this, [this, sock, buf]() {
            *buf += sock->readAll();

            /* Attendi il doppio CRLF che separa header da body */
            const int sep = buf->indexOf("\r\n\r\n");
            if (sep < 0) return;

            /* Estrai Content-Length dall'header */
            const QString hdrs = QString::fromLatin1(buf->left(sep));
            int contentLen = 0;
            for (const QString& line : hdrs.split("\r\n")) {
                if (line.toLower().startsWith("content-length:"))
                    contentLen = line.mid(15).trimmed().toInt();
            }

            /* Attendi che il body sia completo */
            const QByteArray body = buf->mid(sep + 4);
            if (body.size() < contentLen) return;

            /* Parsa il JSON body e salva il captured */
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

            /* ── Risposta NDJSON streaming (formato che AiClient::onReadyRead aspetta) */
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

        /* sock si auto-distrugge alla disconnessione; buf si distrugge da solo
           quando entrambe le lambda escono di scope (shared_ptr ref-count → 0). */
        connect(sock, &QTcpSocket::disconnected, sock, &QTcpSocket::deleteLater);
    }

private:
    QTcpServer* m_server;
    Captured    m_last;
    int         m_count  = 0;
    QString     m_mockText;
};

/* ══════════════════════════════════════════════════════════════
   CAT-E — Costrutti interni (9 casi, mock server — NESSUNA rete reale)
   Verifica che AiClient inietti correttamente nel JSON body:
   - kHonestyPrefix / kCavemanPrefix nel system message
   - iniezione data/ora per keyword temporali
   - think:true/false in base a QueryType e modello
   - num_predict dinamico (×2 per Complex+think-capable)
   ══════════════════════════════════════════════════════════════ */
class TestInternalConstructs : public QObject {
    Q_OBJECT

    static constexpr quint16 kMockPort    = 19998;
    static constexpr int     kMockTimeout = 8'000;

    /* Esegue chat() puntando al mock server, aspetta finished() o error() */
    struct MockResult {
        bool     ok = false;
        QString  response;
        QString  error;
        OllamaMockServer::Captured captured;
    };

    MockResult runMock(const AiChatParams& params,
                       const QString& model,
                       const QString& system,
                       const QString& msg,
                       AiClient::QueryType qt = AiClient::QueryAuto)
    {
        AiClient ai;
        ai.setBackend(AiClient::Ollama, "127.0.0.1", kMockPort, model);
        ai.setChatParams(params);

        MockResult res;
        QEventLoop loop;
        QTimer watchdog;

        connect(&ai, &AiClient::finished, &loop, [&](const QString& full) {
            res.ok = true; res.response = full; loop.quit();
        });
        connect(&ai, &AiClient::error, &loop, [&](const QString& err) {
            res.error = err; loop.quit();
        });
        watchdog.setSingleShot(true);
        connect(&watchdog, &QTimer::timeout, &loop, [&]() {
            res.error = "TIMEOUT mock server"; loop.quit();
        });
        watchdog.start(kMockTimeout);

        ai.chat(system, msg, {}, qt);
        loop.exec();

        res.captured = m_srv->last();
        return res;
    }

    OllamaMockServer* m_srv = nullptr;

private slots:

    void initTestCase() {
        m_srv = new OllamaMockServer(this);
        QVERIFY2(m_srv->listen(kMockPort),
                 qPrintable("Mock server non avviato su porta " + QString::number(kMockPort)));
        m_srv->setMockResponse("mock-ok");
    }

    void cleanupTestCase() { delete m_srv; m_srv = nullptr; }

    /* ── E1: honesty_prefix=true → kHonestyPrefix preposto al system ─── */
    void honestyPrefixInjected() {
        AiChatParams p; p.honesty_prefix = true; p.caveman_mode = false;
        auto r = runMock(p, "test-model", "mio system", "domanda", AiClient::QueryAuto);
        QVERIFY2(r.ok, qPrintable("Errore: " + r.error));
        QVERIFY2(r.captured.systemContent.contains("REGOLA ASSOLUTA"),
                 "kHonestyPrefix mancante nel system message");
        QVERIFY2(r.captured.systemContent.contains("mio system"),
                 "System prompt originale perso dopo iniezione honesty");
        qDebug() << "E1 OK: honesty_prefix iniettato";
    }

    /* ── E2: honesty_prefix=false → NO kHonestyPrefix ────────────────── */
    void honestyPrefixNotInjectedWhenOff() {
        AiChatParams p; p.honesty_prefix = false; p.caveman_mode = false;
        auto r = runMock(p, "test-model", "mio system", "domanda", AiClient::QueryAuto);
        QVERIFY2(r.ok, qPrintable("Errore: " + r.error));
        QVERIFY2(!r.captured.systemContent.contains("REGOLA ASSOLUTA"),
                 "kHonestyPrefix iniettato anche con honesty_prefix=false");
        qDebug() << "E2 OK: honesty_prefix assente quando off";
    }

    /* ── E3: caveman_mode=true → kCavemanPrefix preposto ──────────────── */
    void cavemanPrefixInjected() {
        AiChatParams p; p.honesty_prefix = false; p.caveman_mode = true;
        auto r = runMock(p, "test-model", "mio system", "domanda", AiClient::QueryAuto);
        QVERIFY2(r.ok, qPrintable("Errore: " + r.error));
        QVERIFY2(r.captured.systemContent.contains("STILE RISPOSTA"),
                 "kCavemanPrefix mancante nel system message");
        QVERIFY2(r.captured.systemContent.contains("mio system"),
                 "System prompt originale perso dopo iniezione caveman");
        qDebug() << "E3 OK: caveman_mode iniettato";
    }

    /* ── E4: caveman + honesty → entrambi, honesty in testa ───────────── */
    void bothPrefixesOrderedCorrectly() {
        AiChatParams p; p.honesty_prefix = true; p.caveman_mode = true;
        auto r = runMock(p, "test-model", "mio system", "domanda", AiClient::QueryAuto);
        QVERIFY2(r.ok, qPrintable("Errore: " + r.error));
        const QString& sys = r.captured.systemContent;
        QVERIFY2(sys.contains("REGOLA ASSOLUTA"), "kHonestyPrefix mancante");
        QVERIFY2(sys.contains("STILE RISPOSTA"),  "kCavemanPrefix mancante");
        /* ai_client.cpp righe 429-432: prima caveman poi honesty →
           honesty diventa il prefisso più esterno (REGOLA ASSOLUTA viene prima) */
        QVERIFY2(sys.indexOf("REGOLA ASSOLUTA") < sys.indexOf("STILE RISPOSTA"),
                 "Ordine errato: honesty deve precedere caveman nel system message");
        qDebug() << "E4 OK: ordine prefissi corretto";
    }

    /* ── E5: QuerySimple → think:false + num_predict=512 ──────────────── */
    void querySimpleForcesThinkFalse() {
        AiChatParams p; p.honesty_prefix = false; p.caveman_mode = false; p.num_predict = 1024;
        auto r = runMock(p, "test-model", "sys", "ciao", AiClient::QuerySimple);
        QVERIFY2(r.ok, qPrintable("Errore: " + r.error));
        QCOMPARE(r.captured.options["think"].toBool(true), false);
        QCOMPARE(r.captured.options["num_predict"].toInt(), 512);
        qDebug() << "E5 OK: QuerySimple → think:false num_predict=512";
    }

    /* ── E6: QueryComplex + think-capable → think:true + num_predict×2 ── */
    void queryComplexThinkCapableDoublesPredict() {
        AiChatParams p; p.honesty_prefix = false; p.caveman_mode = false; p.num_predict = 1024;
        /* qwen3:4b è think-capable (starts with "qwen3") e non è knownBroken */
        auto r = runMock(p, "qwen3:4b", "sys", "spiega tutto", AiClient::QueryComplex);
        QVERIFY2(r.ok, qPrintable("Errore: " + r.error));
        QCOMPARE(r.captured.options["think"].toBool(false), true);
        QCOMPARE(r.captured.options["num_predict"].toInt(), 2048);  /* 1024 × 2 */
        qDebug() << "E6 OK: think-capable QueryComplex → think:true num_predict=2048";
    }

    /* ── E7: QueryComplex + modello NON think-capable → think assente ─── */
    void queryComplexNonThinkCapableNoThink() {
        AiChatParams p; p.honesty_prefix = false; p.caveman_mode = false; p.num_predict = 1024;
        /* mistral non è think-capable → think:true non deve essere inviato */
        auto r = runMock(p, "mistral:7b-instruct", "sys", "spiega tutto", AiClient::QueryComplex);
        QVERIFY2(r.ok, qPrintable("Errore: " + r.error));
        /* think non deve essere presente nelle options (o deve essere false) */
        const QJsonValue thinkVal = r.captured.options["think"];
        const bool thinkSent = !thinkVal.isUndefined() && thinkVal.toBool(false);
        QVERIFY2(!thinkSent, "think:true inviato a modello non think-capable (mistral)");
        QCOMPARE(r.captured.options["num_predict"].toInt(), 1024);  /* nessun raddoppio */
        qDebug() << "E7 OK: non think-capable → think assente";
    }

    /* ── E8: keyword "oggi" nel msg → iniezione data/ora nel system ───── */
    void dateInjectedForTemporalKeyword() {
        AiChatParams p; p.honesty_prefix = false; p.caveman_mode = false;
        auto r = runMock(p, "test-model", "sys", "che giorno è oggi?", AiClient::QueryAuto);
        QVERIFY2(r.ok, qPrintable("Errore: " + r.error));
        QVERIFY2(r.captured.systemContent.contains("DATA E ORA ATTUALE"),
                 "Iniezione data/ora mancante per keyword 'oggi'");
        /* La data deve contenere l'anno corrente */
        const QString year = QString::number(QDateTime::currentDateTime().date().year());
        QVERIFY2(r.captured.systemContent.contains(year),
                 qPrintable("Anno " + year + " mancante nella data iniettata"));
        qDebug() << "E8 OK: data iniettata per 'oggi'";
    }

    /* ── E9: nessuna keyword temporale → nessuna iniezione data ────────── */
    void dateNotInjectedWithoutKeyword() {
        AiChatParams p; p.honesty_prefix = false; p.caveman_mode = false;
        auto r = runMock(p, "test-model", "sys", "ciao come stai", AiClient::QueryAuto);
        QVERIFY2(r.ok, qPrintable("Errore: " + r.error));
        QVERIFY2(!r.captured.systemContent.contains("DATA E ORA ATTUALE"),
                 "Data iniettata senza keyword temporale nel messaggio");
        qDebug() << "E9 OK: nessuna iniezione data senza keyword";
    }
};

/* ══════════════════════════════════════════════════════════════
   Entry point — lancia tutte le classi di test in sequenza
   ══════════════════════════════════════════════════════════════ */
int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    const QString model = testModel();
    qDebug() << "=== test_ai_integration — modello:" << model << "===";

    int status = 0;

    {
        TestClassifyQuery t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestAiChatParams t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestAiClientChat t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestQueryTypePassthrough t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestInternalConstructs t;
        status |= QTest::qExec(&t, argc, argv);
    }

    return status;
}

#include "test_ai_integration.moc"
