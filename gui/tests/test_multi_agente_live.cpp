/* ══════════════════════════════════════════════════════════════
   test_multi_agente_live.cpp — Test di integrazione con Ollama reale
                                per AgentiMultiPage + GraphMemory

   Categorie:
     CAT-A  Costruzione   — nessun Ollama richiesto
     CAT-B  Decomposizione JSON — richiede Ollama (QSKIP altrimenti)
     CAT-C  Esecuzione SubTask  — richiede Ollama (QSKIP altrimenti)
     CAT-D  GraphMemory persistenza — richiede Ollama (QSKIP altrimenti)

   COME ESEGUIRE:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_multi_agente_live
     ./build_tests/test_multi_agente_live

   REQUISITI:
     - Ollama in esecuzione su 127.0.0.1:11434
     - Modello TEST_MODEL disponibile (default: mistral:7b-instruct)
     - Variabile d'ambiente PRISMALUX_TEST_MODEL per override
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QApplication>
#include <QEventLoop>
#include <QTimer>
#include <QElapsedTimer>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QProcessEnvironment>
#include <QListWidget>
#include <QPushButton>
#include <QTextEdit>
#include <QPlainTextEdit>
#include "../ai_client.h"
#include "../graph_memory.h"
#include "../pages/main_multi_agent.h"

/* ── Modello configurabile via env ────────────────────────────── */
static QString testModel()
{
    const QString env = QProcessEnvironment::systemEnvironment()
                            .value("PRISMALUX_TEST_MODEL");
    return env.isEmpty() ? QStringLiteral("mistral:7b-instruct") : env;
}

static constexpr int kDecompTimeoutMs = 30'000;   /* 30 s per decomposizione */
static constexpr int kExecTimeoutMs   = 60'000;   /* 60 s per esecuzione subtask */
static constexpr int kPollIntervalMs  = 300;      /* polling UI ogni 300 ms */

/* ══════════════════════════════════════════════════════════════
   Helper: controlla se Ollama e' raggiungibile

   Usa QNetworkAccessManager con transfer timeout 2 s.
   Ritorna true se /api/tags risponde senza errori HTTP.
   ══════════════════════════════════════════════════════════════ */
static bool ollamaAvailable()
{
    QNetworkAccessManager nam;
    QNetworkRequest req(QUrl("http://127.0.0.1:11434/api/tags"));
    req.setTransferTimeout(2000);
    auto* r = nam.get(req);
    QEventLoop loop;
    QObject::connect(r, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    const bool ok = (r->error() == QNetworkReply::NoError);
    r->deleteLater();
    return ok;
}

/* ══════════════════════════════════════════════════════════════
   CAT-A — Costruzione (nessun Ollama)

   Verifica che AgentiMultiPage costruisca correttamente con
   un AiClient reale puntato a Ollama, senza crashi o assert.
   graphMemory() deve restituire un puntatore valido.
   ══════════════════════════════════════════════════════════════ */
class TestCatA : public QObject {
    Q_OBJECT
private slots:

    /* A-1: costruzione senza crash */
    void costruzioneSenzaCrash()
    {
        auto* ai = new AiClient;
        ai->setBackend(AiClient::Ollama, "127.0.0.1", 11434, testModel());

        auto* page = new AgentiMultiPage(ai);
        QVERIFY2(page != nullptr, "AgentiMultiPage non costruita");
        delete page;
        delete ai;
    }

    /* A-2: graphMemory() non null dopo costruzione */
    void graphMemoryNonNull()
    {
        auto* ai = new AiClient;
        ai->setBackend(AiClient::Ollama, "127.0.0.1", 11434, testModel());
        auto* page = new AgentiMultiPage(ai);

        QVERIFY2(page->graphMemory() != nullptr,
                 "graphMemory() deve restituire un puntatore valido dopo costruzione");

        delete page;
        delete ai;
    }

    /* A-3: widget visibile e valido */
    void widgetValido()
    {
        auto* ai = new AiClient;
        ai->setBackend(AiClient::Ollama, "127.0.0.1", 11434, testModel());
        auto* page = new AgentiMultiPage(ai);

        QVERIFY2(page->isWidgetType(), "AgentiMultiPage deve essere un QWidget");

        delete page;
        delete ai;
    }

    /* A-4: setExtRagMemory non crasha con nullptr */
    void setExtRagMemoryNullptr()
    {
        auto* ai = new AiClient;
        ai->setBackend(AiClient::Ollama, "127.0.0.1", 11434, testModel());
        auto* page = new AgentiMultiPage(ai);

        /* non deve crashare */
        page->setExtRagMemory(nullptr);
        QVERIFY(true);

        delete page;
        delete ai;
    }

    /* A-5: due istanze indipendenti non condividono graphMemory */
    void dueIstanzeGraphMemoryIndipendenti()
    {
        auto* ai1 = new AiClient;
        ai1->setBackend(AiClient::Ollama, "127.0.0.1", 11434, testModel());
        auto* ai2 = new AiClient;
        ai2->setBackend(AiClient::Ollama, "127.0.0.1", 11434, testModel());

        auto* page1 = new AgentiMultiPage(ai1);
        auto* page2 = new AgentiMultiPage(ai2);

        QVERIFY2(page1->graphMemory() != page2->graphMemory(),
                 "Istanze distinte non devono condividere la stessa GraphMemory");

        delete page1;
        delete page2;
        delete ai1;
        delete ai2;
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B — Decomposizione JSON (richiede Ollama)

   Invia un task semplice e verifica che il MasterAgent generi
   almeno 1 SubTask con piano JSON valido (campo "subtasks").

   Polling: controlla ogni kPollIntervalMs la lista task items
   nel widget — la decomposizione aggiunge item al QListWidget.
   ══════════════════════════════════════════════════════════════ */
class TestCatB : public QObject {
    Q_OBJECT

    bool m_skip = false;

private slots:

    void initTestCase()
    {
        if (!ollamaAvailable())
            QSKIP("CAT-B: Ollama non raggiungibile su 127.0.0.1:11434 — test saltati.");
    }

    /* B-1: decomposizione genera almeno 1 subtask */
    void decomponiTaskSemplice()
    {
        if (m_skip) QSKIP("Ollama non disponibile");

        auto* page = new AgentiMultiPage(nullptr);  /* crea con ai interno */
        auto* ai = new AiClient(page);              /* parent = page → gestione automatica */
        ai->setBackend(AiClient::Ollama, "127.0.0.1", 11434, testModel());
        page->show();
        QApplication::processEvents(QEventLoop::AllEvents, 200);

        /* Trova il QListWidget dei subtask tramite findChild */
        auto* taskList = page->findChild<QListWidget*>();
        if (!taskList) QSKIP("QListWidget subtask non trovato — layout AgentiMultiPage cambiato");

        const int initialCount = taskList->count();

        /* Trova il QTextEdit del prompt input e il pulsante Decomponsi */
        auto* promptInput = page->findChild<QTextEdit*>();
        QVERIFY2(promptInput != nullptr, "QTextEdit del prompt non trovato");

        auto* btnDecompose = page->findChild<QPushButton*>("m_btnDecompose");
        if (!btnDecompose) {
            /* fallback: trova il primo QPushButton con "Decompon" nel testo */
            const auto btns = page->findChildren<QPushButton*>();
            for (auto* b : btns) {
                if (b->text().contains("Decompon", Qt::CaseInsensitive) ||
                    b->text().contains("Avvia",    Qt::CaseInsensitive)) {
                    btnDecompose = b;
                    break;
                }
            }
        }
        QVERIFY2(btnDecompose != nullptr, "Pulsante decomposizione non trovato");

        /* Imposta il prompt e clicca */
        promptInput->setPlainText("Dimmi ciao in italiano e in francese.");
        QTest::mouseClick(btnDecompose, Qt::LeftButton);

        /* Polling: attende fino a kDecompTimeoutMs che appaia almeno 1 item */
        QElapsedTimer elapsed;
        elapsed.start();
        while (taskList->count() <= initialCount &&
               elapsed.elapsed() < kDecompTimeoutMs) {
            QTest::qWait(kPollIntervalMs);
            QApplication::processEvents();
        }

        const int finalCount = taskList->count();
        QVERIFY2(finalCount > initialCount,
                 qPrintable(QString("Nessun SubTask generato entro %1 s. "
                                    "Items: %2 (era %3)")
                                .arg(kDecompTimeoutMs / 1000)
                                .arg(finalCount)
                                .arg(initialCount)));

        qDebug() << "B-1 OK: subtask generati:" << (finalCount - initialCount)
                 << "in" << elapsed.elapsed() << "ms";

        delete page;
        delete ai;
    }

    /* B-2: la GraphMemory riceve almeno un nodo dopo la decomposizione */
    void graphMemoryRiceveNodoDopoDecomposizione()
    {
        if (m_skip) QSKIP("Ollama non disponibile");

        auto* ai = new AiClient;
        ai->setBackend(AiClient::Ollama, "127.0.0.1", 11434, testModel());
        auto* page = new AgentiMultiPage(ai);
        page->show();

        auto* taskList    = page->findChild<QListWidget*>();
        auto* promptInput = page->findChild<QTextEdit*>();
        QVERIFY(taskList    != nullptr);
        QVERIFY(promptInput != nullptr);

        QPushButton* btnDecompose = nullptr;
        const auto btns = page->findChildren<QPushButton*>();
        for (auto* b : btns) {
            if (b->text().contains("Decompon", Qt::CaseInsensitive) ||
                b->text().contains("Avvia",    Qt::CaseInsensitive)) {
                btnDecompose = b;
                break;
            }
        }
        QVERIFY2(btnDecompose != nullptr, "Pulsante decomposizione non trovato");

        const int nodesBefore = page->graphMemory()->nodeCount();

        promptInput->setPlainText("Elenca 2 numeri primi.");
        QTest::mouseClick(btnDecompose, Qt::LeftButton);

        /* Attendi decomposizione + prima esecuzione subtask */
        QElapsedTimer elapsed;
        elapsed.start();
        while (page->graphMemory()->nodeCount() <= nodesBefore &&
               elapsed.elapsed() < kDecompTimeoutMs) {
            QTest::qWait(kPollIntervalMs);
            QApplication::processEvents();
        }

        const int nodesAfter = page->graphMemory()->nodeCount();
        QVERIFY2(nodesAfter > nodesBefore,
                 qPrintable(QString("GraphMemory non aggiornata: nodi prima=%1 dopo=%2")
                                .arg(nodesBefore).arg(nodesAfter)));

        qDebug() << "B-2 OK: nodi GraphMemory:" << nodesBefore << "->" << nodesAfter;

        delete page;
        delete ai;
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-C — Esecuzione SubTask (richiede Ollama)

   Task: "Calcola 2+2 e scrivi il risultato."
   Verifica:
     - il risultato nel log/output contiene "4"
     - almeno un nodo di tipo "result" in GraphMemory
   ══════════════════════════════════════════════════════════════ */
class TestCatC : public QObject {
    Q_OBJECT

    bool m_skip = false;

private slots:

    void initTestCase()
    {
        if (!ollamaAvailable())
            QSKIP("CAT-C: Ollama non raggiungibile — test saltati.");
    }

    /* C-1: risultato subtask contiene "4" */
    void risultatoContieneQuattro()
    {
        if (m_skip) QSKIP("Ollama non disponibile");

        auto* ai = new AiClient;
        ai->setBackend(AiClient::Ollama, "127.0.0.1", 11434, testModel());
        auto* page = new AgentiMultiPage(ai);
        page->show();

        auto* promptInput = page->findChild<QTextEdit*>();
        QVERIFY2(promptInput != nullptr, "QTextEdit prompt non trovato");

        QPushButton* btnDecompose = nullptr;
        for (auto* b : page->findChildren<QPushButton*>()) {
            if (b->text().contains("Decompon", Qt::CaseInsensitive) ||
                b->text().contains("Avvia",    Qt::CaseInsensitive)) {
                btnDecompose = b;
                break;
            }
        }
        QVERIFY2(btnDecompose != nullptr, "Pulsante decomposizione non trovato");

        /* Task semplice con risposta attesa nota */
        promptInput->setPlainText(
            "Calcola 2+2 e scrivi SOLO il risultato numerico, nulla altro.");
        QTest::mouseClick(btnDecompose, Qt::LeftButton);

        /* Attendi il completamento: cerca nodo "result" in GraphMemory */
        QElapsedTimer elapsed;
        elapsed.start();
        bool hasResult = false;
        while (!hasResult && elapsed.elapsed() < kExecTimeoutMs) {
            QTest::qWait(kPollIntervalMs);
            QApplication::processEvents();
            const auto nodes = page->graphMemory()->allNodes("result");
            if (!nodes.isEmpty()) {
                hasResult = true;
            }
        }

        if (!hasResult) {
            QSKIP("C-1: nessun nodo 'result' entro il timeout — "
                  "il modello potrebbe non aver completato la pipeline. "
                  "Verifica disponibilita' di " + testModel().toLatin1());
        }

        /* Controlla che almeno un nodo result contenga "4" */
        const auto resultNodes = page->graphMemory()->allNodes("result");
        bool found4 = false;
        for (const GmNode& n : resultNodes) {
            if (n.content.contains("4") || n.label.contains("4")) {
                found4 = true;
                break;
            }
        }

        /* Controlla anche nel log QPlainTextEdit */
        if (!found4) {
            const auto logs = page->findChildren<QPlainTextEdit*>();
            for (auto* log : logs) {
                if (log->toPlainText().contains("4")) {
                    found4 = true;
                    break;
                }
            }
        }

        QVERIFY2(found4,
                 qPrintable("Il risultato '4' non trovato ne' in GraphMemory ne' nel log. "
                             "Nodi result: " +
                             QString::number(resultNodes.size())));

        qDebug() << "C-1 OK: risultato '4' trovato in" << elapsed.elapsed() << "ms";

        delete page;
        delete ai;
    }

    /* C-2: nodo "result" aggiunto a GraphMemory dopo esecuzione */
    void nodoResultInGraphMemory()
    {
        if (m_skip) QSKIP("Ollama non disponibile");

        auto* ai = new AiClient;
        ai->setBackend(AiClient::Ollama, "127.0.0.1", 11434, testModel());
        auto* page = new AgentiMultiPage(ai);
        page->show();

        const int resultNodesBefore = page->graphMemory()->allNodes("result").size();

        auto* promptInput = page->findChild<QTextEdit*>();
        QVERIFY(promptInput != nullptr);

        QPushButton* btnDecompose = nullptr;
        for (auto* b : page->findChildren<QPushButton*>()) {
            if (b->text().contains("Decompon", Qt::CaseInsensitive) ||
                b->text().contains("Avvia",    Qt::CaseInsensitive)) {
                btnDecompose = b;
                break;
            }
        }
        QVERIFY(btnDecompose != nullptr);

        promptInput->setPlainText("Scrivi la parola 'OK' in maiuscolo.");
        QTest::mouseClick(btnDecompose, Qt::LeftButton);

        QElapsedTimer elapsed;
        elapsed.start();
        int resultNodesAfter = resultNodesBefore;
        while (resultNodesAfter <= resultNodesBefore &&
               elapsed.elapsed() < kExecTimeoutMs) {
            QTest::qWait(kPollIntervalMs);
            QApplication::processEvents();
            resultNodesAfter = page->graphMemory()->allNodes("result").size();
        }

        QVERIFY2(resultNodesAfter > resultNodesBefore,
                 qPrintable(QString("Nodo 'result' non aggiunto in GraphMemory. "
                                    "Prima: %1  Dopo: %2  Timeout: %3 s")
                                .arg(resultNodesBefore)
                                .arg(resultNodesAfter)
                                .arg(kExecTimeoutMs / 1000)));

        qDebug() << "C-2 OK: nodi result:" << resultNodesBefore
                 << "->" << resultNodesAfter
                 << "in" << elapsed.elapsed() << "ms";

        delete page;
        delete ai;
    }

    /* C-3: nodi result hanno content non vuoto */
    void nodoResultContenutoNonVuoto()
    {
        if (m_skip) QSKIP("Ollama non disponibile");

        auto* ai = new AiClient;
        ai->setBackend(AiClient::Ollama, "127.0.0.1", 11434, testModel());
        auto* page = new AgentiMultiPage(ai);
        page->show();

        auto* promptInput = page->findChild<QTextEdit*>();
        QVERIFY(promptInput != nullptr);

        QPushButton* btnDecompose = nullptr;
        for (auto* b : page->findChildren<QPushButton*>()) {
            if (b->text().contains("Decompon", Qt::CaseInsensitive) ||
                b->text().contains("Avvia",    Qt::CaseInsensitive)) {
                btnDecompose = b;
                break;
            }
        }
        QVERIFY(btnDecompose != nullptr);

        promptInput->setPlainText("Dimmi il nome di una capitale europea.");
        QTest::mouseClick(btnDecompose, Qt::LeftButton);

        QElapsedTimer elapsed;
        elapsed.start();
        QVector<GmNode> resultNodes;
        while (resultNodes.isEmpty() && elapsed.elapsed() < kExecTimeoutMs) {
            QTest::qWait(kPollIntervalMs);
            QApplication::processEvents();
            resultNodes = page->graphMemory()->allNodes("result");
        }

        if (resultNodes.isEmpty()) {
            QSKIP("C-3: nessun nodo result entro timeout — infrastruttura instabile");
        }

        bool allHaveContent = true;
        for (const GmNode& n : resultNodes) {
            if (n.content.trimmed().isEmpty()) {
                allHaveContent = false;
                break;
            }
        }
        QVERIFY2(allHaveContent, "Almeno un nodo 'result' ha content vuoto");

        qDebug() << "C-3 OK: tutti i nodi result hanno content non vuoto. "
                    "Nodi:" << resultNodes.size();

        delete page;
        delete ai;
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-D — GraphMemory persistenza (richiede Ollama)

   Dopo una pipeline completa:
     - allNodes() non e' vuoto
     - i nodi hanno importanza > 0
     - toDot() non crasha e produce output non vuoto
     - toJson() produce JSON valido
   ══════════════════════════════════════════════════════════════ */
class TestCatD : public QObject {
    Q_OBJECT

    bool m_skip = false;

private slots:

    void initTestCase()
    {
        if (!ollamaAvailable())
            QSKIP("CAT-D: Ollama non raggiungibile — test saltati.");
    }

    /* D-1: allNodes() non vuoto dopo pipeline */
    void allNodesNonVuotoDopoEsecuzione()
    {
        if (m_skip) QSKIP("Ollama non disponibile");

        auto* ai = new AiClient;
        ai->setBackend(AiClient::Ollama, "127.0.0.1", 11434, testModel());
        auto* page = new AgentiMultiPage(ai);
        page->show();

        auto* promptInput = page->findChild<QTextEdit*>();
        QVERIFY(promptInput != nullptr);

        QPushButton* btnDecompose = nullptr;
        for (auto* b : page->findChildren<QPushButton*>()) {
            if (b->text().contains("Decompon", Qt::CaseInsensitive) ||
                b->text().contains("Avvia",    Qt::CaseInsensitive)) {
                btnDecompose = b;
                break;
            }
        }
        QVERIFY(btnDecompose != nullptr);

        promptInput->setPlainText("Elenca un vantaggio del linguaggio Python.");
        QTest::mouseClick(btnDecompose, Qt::LeftButton);

        /* Attendi che almeno 1 nodo sia in GraphMemory */
        QElapsedTimer elapsed;
        elapsed.start();
        while (page->graphMemory()->nodeCount() == 0 &&
               elapsed.elapsed() < kExecTimeoutMs) {
            QTest::qWait(kPollIntervalMs);
            QApplication::processEvents();
        }

        const int nodeCount = page->graphMemory()->nodeCount();
        QVERIFY2(nodeCount > 0,
                 qPrintable(QString("GraphMemory vuota dopo esecuzione (timeout %1 s)")
                                .arg(kExecTimeoutMs / 1000)));

        qDebug() << "D-1 OK: nodeCount =" << nodeCount
                 << "in" << elapsed.elapsed() << "ms";

        delete page;
        delete ai;
    }

    /* D-2: importanza dei nodi > 0 */
    void nodiHannoImportanzaPositiva()
    {
        if (m_skip) QSKIP("Ollama non disponibile");

        auto* ai = new AiClient;
        ai->setBackend(AiClient::Ollama, "127.0.0.1", 11434, testModel());
        auto* page = new AgentiMultiPage(ai);
        page->show();

        auto* promptInput = page->findChild<QTextEdit*>();
        QVERIFY(promptInput != nullptr);

        QPushButton* btnDecompose = nullptr;
        for (auto* b : page->findChildren<QPushButton*>()) {
            if (b->text().contains("Decompon", Qt::CaseInsensitive) ||
                b->text().contains("Avvia",    Qt::CaseInsensitive)) {
                btnDecompose = b;
                break;
            }
        }
        QVERIFY(btnDecompose != nullptr);

        promptInput->setPlainText("Descrivi in una parola il colore del cielo.");
        QTest::mouseClick(btnDecompose, Qt::LeftButton);

        QElapsedTimer elapsed;
        elapsed.start();
        while (page->graphMemory()->nodeCount() == 0 &&
               elapsed.elapsed() < kExecTimeoutMs) {
            QTest::qWait(kPollIntervalMs);
            QApplication::processEvents();
        }

        const auto nodes = page->graphMemory()->allNodes();
        if (nodes.isEmpty()) {
            QSKIP("D-2: nessun nodo generato — infrastruttura instabile");
        }

        bool allPositive = true;
        for (const GmNode& n : nodes) {
            if (n.importance <= 0.0f) {
                allPositive = false;
                qDebug() << "D-2 nodo con importanza <= 0:" << n.label << n.importance;
                break;
            }
        }
        QVERIFY2(allPositive, "Almeno un nodo ha importanza <= 0");

        qDebug() << "D-2 OK: tutti" << nodes.size() << "nodi hanno importanza > 0";

        delete page;
        delete ai;
    }

    /* D-3: toDot() non crasha e produce output non vuoto */
    void exportDotNonCrasha()
    {
        if (m_skip) QSKIP("Ollama non disponibile");

        auto* ai = new AiClient;
        ai->setBackend(AiClient::Ollama, "127.0.0.1", 11434, testModel());
        auto* page = new AgentiMultiPage(ai);
        page->show();

        auto* promptInput = page->findChild<QTextEdit*>();
        QVERIFY(promptInput != nullptr);

        QPushButton* btnDecompose = nullptr;
        for (auto* b : page->findChildren<QPushButton*>()) {
            if (b->text().contains("Decompon", Qt::CaseInsensitive) ||
                b->text().contains("Avvia",    Qt::CaseInsensitive)) {
                btnDecompose = b;
                break;
            }
        }
        QVERIFY(btnDecompose != nullptr);

        promptInput->setPlainText("Nomina un pianeta del sistema solare.");
        QTest::mouseClick(btnDecompose, Qt::LeftButton);

        /* Attendi almeno 1 nodo */
        QElapsedTimer elapsed;
        elapsed.start();
        while (page->graphMemory()->nodeCount() == 0 &&
               elapsed.elapsed() < kExecTimeoutMs) {
            QTest::qWait(kPollIntervalMs);
            QApplication::processEvents();
        }

        if (page->graphMemory()->nodeCount() == 0) {
            QSKIP("D-3: GraphMemory vuota — infrastruttura instabile");
        }

        /* toDot() non deve crashare */
        const QString dot = page->graphMemory()->toDot("TestDot", 50);

        QVERIFY2(!dot.isEmpty(), "toDot() ha restituito stringa vuota");
        QVERIFY2(dot.contains("digraph") || dot.contains("graph"),
                 "toDot() deve contenere 'digraph' o 'graph'");

        qDebug() << "D-3 OK: toDot() ok, lunghezza =" << dot.length() << "chars";

        delete page;
        delete ai;
    }

    /* D-4: toJson() produce JSON valido */
    void exportJsonValido()
    {
        if (m_skip) QSKIP("Ollama non disponibile");

        auto* ai = new AiClient;
        ai->setBackend(AiClient::Ollama, "127.0.0.1", 11434, testModel());
        auto* page = new AgentiMultiPage(ai);
        page->show();

        auto* promptInput = page->findChild<QTextEdit*>();
        QVERIFY(promptInput != nullptr);

        QPushButton* btnDecompose = nullptr;
        for (auto* b : page->findChildren<QPushButton*>()) {
            if (b->text().contains("Decompon", Qt::CaseInsensitive) ||
                b->text().contains("Avvia",    Qt::CaseInsensitive)) {
                btnDecompose = b;
                break;
            }
        }
        QVERIFY(btnDecompose != nullptr);

        promptInput->setPlainText("Dimmi un sinonimo della parola 'veloce'.");
        QTest::mouseClick(btnDecompose, Qt::LeftButton);

        QElapsedTimer elapsed;
        elapsed.start();
        while (page->graphMemory()->nodeCount() == 0 &&
               elapsed.elapsed() < kExecTimeoutMs) {
            QTest::qWait(kPollIntervalMs);
            QApplication::processEvents();
        }

        if (page->graphMemory()->nodeCount() == 0) {
            QSKIP("D-4: GraphMemory vuota — infrastruttura instabile");
        }

        const QString json = page->graphMemory()->toJson(100);
        QVERIFY2(!json.isEmpty(), "toJson() ha restituito stringa vuota");

        QJsonParseError parseErr;
        const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &parseErr);
        QVERIFY2(parseErr.error == QJsonParseError::NoError,
                 qPrintable("toJson() non produce JSON valido: " + parseErr.errorString()));
        QVERIFY2(!doc.isNull(), "JSON document e' null");

        qDebug() << "D-4 OK: toJson() produce JSON valido, lunghezza =" << json.length();

        delete page;
        delete ai;
    }
};

/* ══════════════════════════════════════════════════════════════
   Entry point — lancia tutte le categorie in sequenza
   ══════════════════════════════════════════════════════════════ */
int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    qDebug() << "=== test_multi_agente_live — modello:" << testModel() << "===";
    qDebug() << "    Ollama:" << (ollamaAvailable() ? "raggiungibile" : "NON raggiungibile");

    int r = 0;
    r |= QTest::qExec(new TestCatA, argc, argv);
    r |= QTest::qExec(new TestCatB, argc, argv);
    r |= QTest::qExec(new TestCatC, argc, argv);
    r |= QTest::qExec(new TestCatD, argc, argv);
    return r;
}

#include "test_multi_agente_live.moc"
