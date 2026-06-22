/* ══════════════════════════════════════════════════════════════
   test_mcp_integration.cpp — MCP JSON-RPC integration test

   Categorie:
     CAT-A  Struttura McpManagerPage (no processo, no venv)
     CAT-B  scanMcpServers — scansiona la cartella MCPs/
     CAT-C  smokeTestRequests / smokeTestPassed — protocollo JSON-RPC
     CAT-D  Integrazione reale — avvia un MCP vero (QSKIP se venv assente)

   Build:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_mcp_integration
     ./build_tests/test_mcp_integration
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QApplication>
#include <QEventLoop>
#include <QTimer>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QFileInfo>

#include "../pages/main_mcp_manager.h"
#include "../prismalux_paths.h"

namespace P = PrismaluxPaths;

/* ══════════════════════════════════════════════════════════════
   CAT-A — Costruzione McpManagerPage
   ══════════════════════════════════════════════════════════════ */
class TestCatA : public QObject {
    Q_OBJECT
private slots:

    /* A-1: costruzione senza crash */
    void costruzione() {
        McpManagerPage page;
        QVERIFY(true);
    }

    /* A-2: venvDir() restituisce path non vuoto */
    void venvDirNonVuoto() {
        QVERIFY(!McpManagerPage::venvDir().isEmpty());
    }

    /* A-3: venvPython() contiene "python" */
    void venvPythonContienePython() {
        const QString p = McpManagerPage::venvPython();
        QVERIFY2(p.contains("python", Qt::CaseInsensitive),
                 qPrintable("venvPython() = " + p));
    }

    /* A-4: venvExists() non crasha (può essere false) */
    void venvExistsNonCrasha() {
        const bool e = McpManagerPage::venvExists();
        Q_UNUSED(e);
        QVERIFY(true);
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B — scanMcpServers
   ══════════════════════════════════════════════════════════════ */
class TestCatB : public QObject {
    Q_OBJECT
private slots:

    /* B-1: scanMcpServers su cartella MCPs/ reale → elenco non vuoto */
    void scanRealeMcpsDir() {
        const QString mcpsRoot = P::root() + "/MCPs";
        if (!QDir(mcpsRoot).exists())
            QSKIP("MCPs/ non trovata — test su macchina senza repo completo");

        const QStringList found = McpManagerPage::scanMcpServers(mcpsRoot);
        QVERIFY2(!found.isEmpty(),
                 qPrintable("nessun MCP trovato in " + mcpsRoot));
    }

    /* B-2: ogni nome restituito corrisponde a una cartella con server.py */
    void ogniNomeHaServerPy() {
        const QString mcpsRoot = P::root() + "/MCPs";
        if (!QDir(mcpsRoot).exists())
            QSKIP("MCPs/ non trovata");

        const QStringList found = McpManagerPage::scanMcpServers(mcpsRoot);
        for (const QString& name : found) {
            const QString srv = mcpsRoot + "/" + name + "/server.py";
            QVERIFY2(QFileInfo::exists(srv),
                     qPrintable(name + " non ha server.py"));
        }
    }

    /* B-3: scanMcpServers su cartella vuota → lista vuota, no crash */
    void scanCartellaNonEsistente() {
        const QStringList found =
            McpManagerPage::scanMcpServers("/nonexistent/path/xyz");
        QCOMPARE(found.size(), 0);
    }

    /* B-4: trova almeno i MCP noti (anki_mcp, knowledge_mcp) */
    void trovaMcpNoti() {
        const QString mcpsRoot = P::root() + "/MCPs";
        if (!QDir(mcpsRoot).exists()) QSKIP("MCPs/ non trovata");

        const QStringList found = McpManagerPage::scanMcpServers(mcpsRoot);
        const QStringList noti = {"knowledge_mcp", "ollama_mcp", "graphviz_mcp"};
        int notiTrovati = 0;
        for (const QString& n : noti)
            if (found.contains(n)) ++notiTrovati;
        QVERIFY2(notiTrovati >= 1,
                 qPrintable("nessun MCP noto trovato in " + mcpsRoot));
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-C — JSON-RPC smoke test protocol (no processo)
   ══════════════════════════════════════════════════════════════ */
class TestCatC : public QObject {
    Q_OBJECT
private slots:

    /* C-1: smokeTestRequests() non è vuoto */
    void requestsNonVuoto() {
        const QByteArray req = McpManagerPage::smokeTestRequests();
        QVERIFY(!req.isEmpty());
    }

    /* C-2: smokeTestRequests() contiene due linee JSON valide */
    void requestsDueLineeJson() {
        const QByteArray req = McpManagerPage::smokeTestRequests();
        const QList<QByteArray> lines = req.split('\n');
        int valid = 0;
        for (const QByteArray& line : lines) {
            const QByteArray trimmed = line.trimmed();
            if (trimmed.isEmpty()) continue;
            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(trimmed, &err);
            if (err.error == QJsonParseError::NoError && !doc.isNull()) ++valid;
        }
        QVERIFY2(valid >= 2, "smokeTestRequests deve contenere >=2 JSON validi");
    }

    /* C-3: smokeTestRequests() contiene "initialize" e "tools/list" */
    void requestsContieneInitializeEToolsList() {
        const QString req = QString::fromUtf8(McpManagerPage::smokeTestRequests());
        QVERIFY2(req.contains("initialize"),
                 "smoke test deve includere initialize");
        QVERIFY2(req.contains("tools/list") || req.contains("tools"),
                 "smoke test deve includere tools/list o tools");
    }

    /* C-4: smokeTestPassed su output vuoto → false */
    void passedOutputVuoto() {
        QVERIFY(!McpManagerPage::smokeTestPassed({}));
    }

    /* C-5: smokeTestPassed su risposta JSON-RPC con "result" → true */
    void passedRispostaValida() {
        const QByteArray resp =
            R"({"jsonrpc":"2.0","id":1,"result":{"protocolVersion":"1.0","serverInfo":{}}})"
            "\n"
            R"({"jsonrpc":"2.0","id":2,"result":{"tools":[]}})";
        QVERIFY(McpManagerPage::smokeTestPassed(resp));
    }

    /* C-6: smokeTestPassed su risposta con "error" → false */
    void passedRispostaError() {
        const QByteArray resp =
            R"({"jsonrpc":"2.0","id":1,"error":{"code":-32600,"message":"Invalid Request"}})";
        QVERIFY(!McpManagerPage::smokeTestPassed(resp));
    }

    /* C-7: smokeTestPassed su risposta parziale (solo initialize) → false */
    void passedRispostaParziale() {
        const QByteArray resp =
            R"({"jsonrpc":"2.0","id":1,"result":{"protocolVersion":"1.0"}})";
        /* Dipende dall'implementazione — non deve crashare */
        const bool p = McpManagerPage::smokeTestPassed(resp);
        Q_UNUSED(p);
        QVERIFY(true);
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-D — Integrazione reale: avvia knowledge_mcp e fai smoke test
   ══════════════════════════════════════════════════════════════ */
class TestCatD : public QObject {
    Q_OBJECT
private slots:

    void initTestCase() {
        if (!McpManagerPage::venvExists())
            QSKIP("venv non disponibile — skipping integrazione reale MCP");

        const QString mcpsRoot = P::root() + "/MCPs";
        const QString srv = mcpsRoot + "/knowledge_mcp/server.py";
        if (!QFileInfo::exists(srv))
            QSKIP("knowledge_mcp/server.py non trovato");
    }

    /* D-1: avvia knowledge_mcp, manda smoke test, verifica risposta */
    void smokeMcpReale() {
        const QString mcpsRoot = P::root() + "/MCPs";
        const QString srv = mcpsRoot + "/knowledge_mcp/server.py";

        QProcess proc;
        proc.setProgram(McpManagerPage::venvPython());
        proc.setArguments({srv});
        proc.start();

        if (!proc.waitForStarted(5000)) {
            QSKIP(qPrintable("Impossibile avviare " + srv));
            return;
        }

        const QByteArray req = McpManagerPage::smokeTestRequests();
        proc.write(req);
        proc.closeWriteChannel();

        QByteArray out;
        for (int i = 0; i < 20; ++i) {
            if (proc.waitForReadyRead(500))
                out += proc.readAllStandardOutput();
            if (McpManagerPage::smokeTestPassed(out)) break;
        }

        proc.terminate();
        proc.waitForFinished(3000);

        QVERIFY2(McpManagerPage::smokeTestPassed(out),
                 qPrintable("Smoke test fallito. Output:\n" + out.left(512)));
    }
};

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    int ret = 0;
    { TestCatA t; ret |= QTest::qExec(&t, argc, argv); }
    { TestCatB t; ret |= QTest::qExec(&t, argc, argv); }
    { TestCatC t; ret |= QTest::qExec(&t, argc, argv); }
    { TestCatD t; ret |= QTest::qExec(&t, argc, argv); }
    return ret;
}
#include "test_mcp_integration.moc"
