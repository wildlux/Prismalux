/* ══════════════════════════════════════════════════════════════
   test_streamlink_mcp.cpp — Unit test per MCPs/streamlink_mcp/server.py (T-6)

   Categorie:
     CAT-A  Protocollo JSON-RPC 2.0 — initialize/tools/list, errori
     CAT-B  _validate_url() — protezione SSRF (blocco IP privati RFC1918)

   Interprete: usa il venv MCP (~/.prismalux/venv) se presente, altrimenti
   fallback a python3 di sistema — stesso pattern di produzione in
   McpManagerPage (main_mcp_manager.cpp: "py = venvExists() ? venvPython()
   : P::findPython()"). streamlink_mcp/server.py non ha dipendenze esterne
   per initialize/tools/list/_validate_url (solo stdlib) — yt-dlp/ffmpeg
   servono solo per l'esecuzione reale dei tool, non testata qui.

   Build:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_streamlink_mcp
     ./build_tests/test_streamlink_mcp
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QProcess>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "../prismalux_paths.h"

namespace P = PrismaluxPaths;

namespace {

QString g_pythonBin;
QString g_serverPath;

/* Stesso fallback usato in produzione da McpManagerPage (main_mcp_manager.cpp) */
QString pickPython()
{
    const QString venvPy = QDir::homePath() + "/.prismalux/venv/bin/python"
#ifdef Q_OS_WIN
        ".exe"
#endif
        ;
    const QFileInfo fi(venvPy);
    if (fi.exists() && fi.isExecutable()) return venvPy;
    const QString sysPy = P::findPython();
    return sysPy.isEmpty() ? "python3" : sysPy;
}

/* Invia una sequenza di richieste JSON-RPC (una per riga) al server via
   stdin, chiude lo stdin e raccoglie tutte le righe di risposta stdout. */
QStringList runJsonRpc(const QStringList& requestLines, int timeoutMs = 15000)
{
    QProcess proc;
    proc.start(g_pythonBin, { g_serverPath });
    if (!proc.waitForStarted(5000)) return {};

    for (const QString& line : requestLines)
        proc.write((line + "\n").toUtf8());
    proc.closeWriteChannel();

    if (!proc.waitForFinished(timeoutMs)) {
        proc.kill();
        proc.waitForFinished(2000);
        return {};
    }

    const QString out = QString::fromUtf8(proc.readAllStandardOutput());
    QStringList lines;
    for (const QString& l : out.split('\n', Qt::SkipEmptyParts))
        lines << l;
    return lines;
}

} // namespace

/* ══════════════════════════════════════════════════════════════
   CAT-A — Protocollo JSON-RPC 2.0
   ══════════════════════════════════════════════════════════════ */
class TestStreamlinkMcpProtocol : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        g_pythonBin  = pickPython();
        g_serverPath = P::root() + "/MCPs/streamlink_mcp/server.py";
        if (!QFile::exists(g_serverPath))
            QSKIP("MCPs/streamlink_mcp/server.py non trovato");
    }

    /* A-1: server.py esiste nel path atteso */
    void scriptEsiste() {
        QVERIFY2(QFile::exists(g_serverPath),
                 qPrintable("script non trovato: " + g_serverPath));
    }

    /* A-2: initialize + tools/list → lista tool non vuota con i 3 attesi */
    void toolsListNonVuota() {
        const QStringList lines = runJsonRpc({
            R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})",
            R"({"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}})",
        });
        QVERIFY2(lines.size() >= 2, "attese almeno 2 righe di risposta");

        const QJsonObject initResp = QJsonDocument::fromJson(lines[0].toUtf8()).object();
        QCOMPARE(initResp["id"].toInt(), 1);
        QVERIFY2(initResp.contains("result"), "initialize deve avere 'result'");

        const QJsonObject listResp = QJsonDocument::fromJson(lines[1].toUtf8()).object();
        QCOMPARE(listResp["id"].toInt(), 2);
        const QJsonArray tools = listResp["result"].toObject()["tools"].toArray();
        QVERIFY2(!tools.isEmpty(), "'tools' non deve essere vuoto");

        QSet<QString> names;
        for (const QJsonValue& v : tools) names.insert(v.toObject()["name"].toString());
        QVERIFY(names.contains("stream_info"));
        QVERIFY(names.contains("stream_capture"));
        QVERIFY(names.contains("stream_download"));
    }

    /* A-3: ogni tool ha inputSchema con "url" tra le proprietà richieste */
    void ogniToolRichiedeUrl() {
        const QStringList lines = runJsonRpc({
            R"({"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}})",
        });
        QVERIFY(!lines.isEmpty());
        const QJsonObject resp = QJsonDocument::fromJson(lines[0].toUtf8()).object();
        const QJsonArray tools = resp["result"].toObject()["tools"].toArray();
        QVERIFY(!tools.isEmpty());
        for (const QJsonValue& v : tools) {
            const QJsonObject t = v.toObject();
            const QJsonArray required = t["inputSchema"].toObject()["required"].toArray();
            bool hasUrl = false;
            for (const QJsonValue& r : required) if (r.toString() == "url") hasUrl = true;
            QVERIFY2(hasUrl, qPrintable(t["name"].toString() + " non richiede 'url'"));
        }
    }

    /* A-4: metodo sconosciuto → errore JSON-RPC -32601 */
    void metodoSconosciutoErrore() {
        const QStringList lines = runJsonRpc({
            R"({"jsonrpc":"2.0","id":7,"method":"pippo_inesistente","params":{}})",
        });
        QVERIFY(!lines.isEmpty());
        const QJsonObject resp = QJsonDocument::fromJson(lines[0].toUtf8()).object();
        QCOMPARE(resp["error"].toObject()["code"].toInt(), -32601);
    }

    /* A-5: riga non-JSON → parse error -32700, id null, no crash del server */
    void jsonMalformatoParseError() {
        const QStringList lines = runJsonRpc({ "questo non e' json valido" });
        QVERIFY(!lines.isEmpty());
        const QJsonObject resp = QJsonDocument::fromJson(lines[0].toUtf8()).object();
        QCOMPARE(resp["error"].toObject()["code"].toInt(), -32700);
        QVERIFY(resp["id"].isNull());
    }

    /* A-6: tool_name inesistente in tools/call → errore -32601 */
    void toolCallInesistente() {
        const QStringList lines = runJsonRpc({
            R"({"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"tool_fantasma","arguments":{}}})",
        });
        QVERIFY(!lines.isEmpty());
        const QJsonObject resp = QJsonDocument::fromJson(lines[0].toUtf8()).object();
        QCOMPARE(resp["error"].toObject()["code"].toInt(), -32601);
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B — _validate_url(): protezione SSRF (IP privati RFC1918)
   ══════════════════════════════════════════════════════════════ */
class TestValidateUrlSsrf : public QObject {
    Q_OBJECT
private:
    /* Chiama _validate_url(url) importando server.py come modulo (il blocco
       `if __name__ == "__main__":` evita che parta il loop stdin) e ne
       ritorna il valore repr() (None o messaggio d'errore). */
    QString callValidateUrl(const QString& url) {
        const QString mcpDir = P::root() + "/MCPs/streamlink_mcp";
        const QString script =
            "import sys; sys.path.insert(0, sys.argv[1])\n"
            "import server\n"
            "r = server._validate_url(sys.argv[2])\n"
            "print(r if r is not None else '<<NONE>>')\n";
        QProcess proc;
        proc.start(g_pythonBin, { "-c", script, mcpDir, url });
        if (!proc.waitForFinished(10000)) return "<<TIMEOUT>>";
        return QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    }

private slots:
    void initTestCase() {
        g_pythonBin = pickPython();
        const QString srv = P::root() + "/MCPs/streamlink_mcp/server.py";
        if (!QFile::exists(srv))
            QSKIP("MCPs/streamlink_mcp/server.py non trovato");
    }

    /* B-1: loopback (127.0.0.1) bloccato */
    void bloccaLoopback() {
        const QString r = callValidateUrl("http://127.0.0.1/admin");
        QVERIFY2(r != "<<NONE>>", "127.0.0.1 deve essere bloccato");
        QVERIFY(r.contains("interno") || r.contains("privato"));
    }

    /* B-2: localhost bloccato */
    void bloccaLocalhost() {
        const QString r = callValidateUrl("http://localhost:8080/x");
        QVERIFY2(r != "<<NONE>>", "localhost deve essere bloccato");
    }

    /* B-3: rete privata 192.168.0.0/16 bloccata */
    void bloccaRete192() {
        const QString r = callValidateUrl("http://192.168.1.1/x");
        QVERIFY2(r != "<<NONE>>", "192.168.x.x deve essere bloccato");
    }

    /* B-4: rete privata 10.0.0.0/8 bloccata */
    void bloccaRete10() {
        const QString r = callValidateUrl("http://10.0.0.5/x");
        QVERIFY2(r != "<<NONE>>", "10.x.x.x deve essere bloccato");
    }

    /* B-5: rete privata 172.16.0.0/12 bloccata */
    void bloccaRete172() {
        const QString r = callValidateUrl("http://172.16.0.1/x");
        QVERIFY2(r != "<<NONE>>", "172.16.x.x deve essere bloccato");
    }

    /* B-6: URL senza schema http/https → rifiutato con messaggio dedicato */
    void schemaNonHttpRifiutato() {
        const QString r = callValidateUrl("ftp://example.com/file");
        QVERIFY2(r != "<<NONE>>", "schema ftp:// deve essere rifiutato");
        QVERIFY(r.contains("http"));
    }

    /* B-7: URL pubblico legittimo (YouTube) NON bloccato */
    void urlPubblicoConsentito() {
        const QString r = callValidateUrl("https://www.youtube.com/watch?v=dQw4w9WgXcQ");
        QCOMPARE(r, QString("<<NONE>>"));
    }

    /* B-8: integrazione end-to-end — tools/call stream_info su URL privato
       viene rifiutato prima di invocare yt-dlp (nessun processo esterno
       lanciato, risposta immediata con messaggio di blocco) */
    void toolCallStreamInfoUrlPrivato() {
        const QStringList lines = runJsonRpc({
            R"({"jsonrpc":"2.0","id":9,"method":"tools/call","params":{"name":"stream_info","arguments":{"url":"http://192.168.1.1/x"}}})",
        }, 10000);
        QVERIFY(!lines.isEmpty());
        const QJsonObject resp = QJsonDocument::fromJson(lines[0].toUtf8()).object();
        const QString text = resp["result"].toObject()["content"].toArray()[0]
                                  .toObject()["text"].toString();
        QVERIFY2(text.contains("non consentito"),
                 qPrintable("testo inatteso: " + text));
    }
};

/* ══════════════════════════════════════════════════════════════
   Runner
   ══════════════════════════════════════════════════════════════ */
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    int status = 0;
    { TestStreamlinkMcpProtocol t; status |= QTest::qExec(&t, argc, argv); }
    { TestValidateUrlSsrf       t; status |= QTest::qExec(&t, argc, argv); }
    return status;
}

#include "test_streamlink_mcp.moc"
