/* ══════════════════════════════════════════════════════════════
   test_mcp_manager.cpp — McpManagerPage (pannello Gestione MCP)

   Categorie:
     CAT-A  Costruzione widget senza crash
     CAT-B  Helper puri: venv path, richieste smoke test, parsing risposta,
            scansione cartelle MCP
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QApplication>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>

#include "../pages/main_mcp_manager.h"

/* ── CAT-B: helper statici (nessuna UI necessaria) ───────────────────────── */
class TestMcpManagerHelpers : public QObject {
    Q_OBJECT
private slots:
    void venvPaths();
    void smokeRequestsWellFormed();
    void smokePassedTrueOnResult();
    void smokePassedFalseOnError();
    void smokePassedFalseOnGarbage();
    void scanFindsServerPy();
    void scanIgnoresDirsWithoutServer();
    void scanMissingRootIsEmpty();
};

void TestMcpManagerHelpers::venvPaths()
{
    QVERIFY(McpManagerPage::venvDir().contains("venv"));
    QVERIFY(McpManagerPage::venvPython().contains("python"));
    QVERIFY(McpManagerPage::venvPython().startsWith(McpManagerPage::venvDir()));
}

void TestMcpManagerHelpers::smokeRequestsWellFormed()
{
    const QByteArray r = McpManagerPage::smokeTestRequests();
    QVERIFY(r.contains("initialize"));
    QVERIFY(r.contains("tools/list"));
    QVERIFY(r.contains("jsonrpc"));
    QCOMPARE(r.count('\n'), 2);   // una richiesta per riga
}

void TestMcpManagerHelpers::smokePassedTrueOnResult()
{
    QVERIFY(McpManagerPage::smokeTestPassed(
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":{\"tools\":[]}}\n"));
    /* result anche se preceduto da righe non-JSON (log su stdout) */
    QVERIFY(McpManagerPage::smokeTestPassed(
        "avvio...\n{\"jsonrpc\":\"2.0\",\"id\":2,\"result\":{}}\n"));
}

void TestMcpManagerHelpers::smokePassedFalseOnError()
{
    QVERIFY(!McpManagerPage::smokeTestPassed(
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"error\":{\"code\":-32601,\"message\":\"x\"}}\n"));
}

void TestMcpManagerHelpers::smokePassedFalseOnGarbage()
{
    QVERIFY(!McpManagerPage::smokeTestPassed("ModuleNotFoundError: no module named foo\n"));
    QVERIFY(!McpManagerPage::smokeTestPassed(QByteArray()));
    QVERIFY(!McpManagerPage::smokeTestPassed("   \n\n"));
}

void TestMcpManagerHelpers::scanFindsServerPy()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QDir(tmp.path()).mkpath("good_mcp");
    QFile f(tmp.path() + "/good_mcp/server.py");
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("print('ok')");
    f.close();
    const QStringList found = McpManagerPage::scanMcpServers(tmp.path());
    QVERIFY(found.contains("good_mcp"));
}

void TestMcpManagerHelpers::scanIgnoresDirsWithoutServer()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QDir(tmp.path()).mkpath("empty_dir");
    const QStringList found = McpManagerPage::scanMcpServers(tmp.path());
    QVERIFY(!found.contains("empty_dir"));
}

void TestMcpManagerHelpers::scanMissingRootIsEmpty()
{
    const QStringList found = McpManagerPage::scanMcpServers("/path/che/non/esiste/xyz");
    QVERIFY(found.isEmpty());
}

/* ── CAT-A: costruzione del widget ───────────────────────────────────────── */
class TestMcpManagerWidget : public QObject {
    Q_OBJECT
private slots:
    void construction();
};

void TestMcpManagerWidget::construction()
{
    McpManagerPage page;     // scansiona MCPs/ reali e costruisce le righe
    QVERIFY(page.layout() != nullptr);
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    int status = 0;
    { TestMcpManagerHelpers t; status |= QTest::qExec(&t, argc, argv); }
    { TestMcpManagerWidget  t; status |= QTest::qExec(&t, argc, argv); }
    return status;
}

#include "test_mcp_manager.moc"
