/* ======================================================================
   lan_server_api.cpp — Handler API avanzati di LanServer
   /api/file · /api/repl · /api/finanza/cf · /api/graph · /bootstrap/
   Estratto da lan_server.cpp per ridurne le dimensioni.
   ====================================================================== */
#include "lan_server.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTemporaryFile>
#include <QRegularExpression>
#include <QFileInfo>
#include <QProcess>
#include <QTextStream>
#include <QUrl>
#include "prismalux_paths.h"
#include "pages/pratico_calcs.h"
#include "graph_memory.h"
#include "widgets/proc_helper.h"
#include <QUrlQuery>
namespace P = PrismaluxPaths;

void LanServer::handleFileApi(Session& s)
{
    /* Estrai Content-Type dal buffer grezzo della richiesta (stesso pattern di handleWhisper) */
    const QByteArray ct = [&]() -> QByteArray {
        const int cti = s.buf.indexOf("Content-Type:");
        if (cti < 0) return {};
        const int lf = s.buf.indexOf('\n', cti);
        return s.buf.mid(cti + 13, lf - cti - 13).trimmed();
    }();

    const int boundPos = ct.indexOf("boundary=");
    if (boundPos < 0) { sendError(s.socket, 400, "Missing boundary"); return; }
    const QByteArray boundary = "--" + ct.mid(boundPos + 9).trimmed();

    /* Estrai la parte "file" */
    const int partStart = s.body.indexOf(boundary);
    if (partStart < 0) { sendError(s.socket, 400, "No part found"); return; }
    const int headerEnd = s.body.indexOf("\r\n\r\n", partStart);
    if (headerEnd < 0) { sendError(s.socket, 400, "Malformed part"); return; }
    const QByteArray partHeader = s.body.mid(partStart, headerEnd - partStart);
    const int dataStart = headerEnd + 4;
    const int nextBound = s.body.indexOf("\r\n" + boundary, dataStart);
    const QByteArray fileData = (nextBound > 0)
        ? s.body.mid(dataStart, nextBound - dataStart)
        : s.body.mid(dataStart);

    /* Ricava il filename dall'header della parte */
    QString filename;
    {
        const QString ph = QString::fromUtf8(partHeader);
        const QRegularExpression re(R"re(filename="([^"]+)")re",
                                    QRegularExpression::CaseInsensitiveOption);
        const auto m = re.match(ph);
        if (m.hasMatch()) filename = m.captured(1);
    }

    const QString ext = QFileInfo(filename).suffix().toLower();

    /* Limite dimensione: evita di passare upload enormi ai processi esterni. */
    if (fileData.size() > 25 * 1024 * 1024) {
        sendError(s.socket, 413, "File too large (max 25 MB)");
        return;
    }
    /* Whitelist estensioni: solo formati che gli estrattori sanno gestire. */
    static const QSet<QString> kAllowedExt = {
        "pdf", "docx", "doc", "txt", "csv", "md", "rtf", "odt", "json", "xml", "html", "htm"
    };
    if (!kAllowedExt.contains(ext)) {
        sendError(s.socket, 415, "Unsupported file type: " + ext.toUtf8());
        return;
    }

    /* Salva su file temporaneo */
    const QString tmp = QDir::tempPath() + "/plx_upload_" +
                        QString::number(QDateTime::currentMSecsSinceEpoch()) + "." + ext;
    {
        QFile f(tmp);
        if (!f.open(QIODevice::WriteOnly)) { sendError(s.socket, 500, "Cannot write tmp"); return; }
        f.write(fileData);
    }

    QString text;
    if (ext == "pdf") {
        const auto r = ProcHelper::run("pdftotext", {tmp, "-"}, 15'000);
        text = r.ok ? r.out : r.err;
    } else if (ext == "docx") {
        const QString pyCode =
            "import sys, docx\n"
            "d = docx.Document(sys.argv[1])\n"
            "print('\\n'.join(p.text for p in d.paragraphs))\n";
        const auto r = ProcHelper::runWithInput(
            P::findPython(), QStringList{"-c", pyCode, tmp}, QByteArray{}, 10'000);
        text = r.ok ? r.out : ("Errore: " + r.err);
    } else {
        QFile f(tmp);
        if (f.open(QIODevice::ReadOnly))
            text = QString::fromUtf8(f.readAll());
    }
    QFile::remove(tmp);

    if (text.length() > 60'000)
        text = text.left(60'000) + "\n[... troncato a 60 000 caratteri ...]";

    QJsonObject obj;
    obj["text"]     = text;
    obj["filename"] = filename;
    obj["chars"]    = text.length();
    sendJson(s.socket, QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

/* ── /api/repl — REPL Python in sandbox bwrap ───────────────────────────────
 * Sandbox a due livelli:
 *   1. bwrap (bubblewrap): namespace isolation — no rete, no pid, no ipc, /tmp vuota
 *   2. ulimit interno: limiti CPU/memoria/processi/fd anche dentro il container
 * bash e node rimossi: superficie d'attacco non necessaria per il REPL web.
 * Fallback ulimit-only se bwrap non è installato (meno sicuro — loggato). */

void LanServer::handleReplApi(Session& s)
{
    const QJsonObject body = QJsonDocument::fromJson(s.body).object();
    const QString code = body["code"].toString().trimmed();
    const QString lang = body["lang"].toString().toLower().trimmed();

    if (code.isEmpty()) { sendError(s.socket, 400, "Empty code"); return; }
    if (code.length() > 16'000) { sendError(s.socket, 400, "Code too large"); return; }

    if (lang == "bash") {
        sendError(s.socket, 403,
            "bash REPL disabilitato per sicurezza. Usa lang=python.");
        return;
    }
    if (lang == "javascript" || lang == "node") {
        sendError(s.socket, 403,
            "node REPL disabilitato. Usa lang=python.");
        return;
    }
    if (lang != "python" && lang != "python3" && !lang.isEmpty()) {
        sendError(s.socket, 400, "Unsupported lang: " + lang.toUtf8());
        return;
    }

    // Ulimit applicato dentro il sandbox (secondo strato di difesa)
    static const QLatin1String kLimits("ulimit -v 262144 -t 10 -u 50 -n 100 2>/dev/null; ");

    ProcResult r;
    static const QString kBwrap = QStandardPaths::findExecutable("bwrap");

    if (!kBwrap.isEmpty()) {
        /* bwrap: namespace isolation completa.
         * --ro-bind / /    → tutto il filesystem host è read-only (no scrittura su /etc, /home…)
         * --tmpfs /tmp     → /tmp scrivibile ma isolato (non persiste tra chiamate)
         * --tmpfs /run     → /run scrivibile in sandbox
         * --unshare-net    → nessun accesso di rete dall'interno
         * --unshare-pid    → PID namespace separato (processo vede solo sé stesso; pid=2)
         * --unshare-ipc    → IPC namespace separato
         * --unshare-uts    → hostname non modificabile
         * --new-session    → nuovo session ID (no SIGINT dall'esterno)
         * --die-with-parent → il child viene killato se il parent muore
         * Secondo strato: ulimit dentro bwrap per CPU/vmem/processi/fd */
        /* Mount selettivo: solo le directory necessarie all'interprete Python.
         * NON monta /home, /root, /var/lib, /etc/shadow, /run/user.
         * Questo impedisce la lettura di file privati anche in read-only. */
        QStringList bwArgs = {
            "--unshare-pid",
            "--unshare-net",
            "--unshare-ipc",
            "--unshare-uts",
            "--new-session",
            "--die-with-parent",
            "--tmpfs",       "/tmp",
            "--tmpfs",       "/run",
            "--dev",         "/dev",
            "--proc",        "/proc",
        };
        /* Monta solo le directory necessarie a Python (lib, bin, usr) */
        for (const char* d : {"/usr", "/lib", "/lib64", "/bin", "/sbin"}) {
            if (QDir(d).exists())
                bwArgs << "--ro-bind" << d << d;
        }
        /* /etc limitato a sotto-directory safe (locale, DNS, timezone, librerie) */
        for (const char* d : {"/etc/ld.so.cache", "/etc/ld.so.conf", "/etc/ld.so.conf.d",
                               "/etc/alternatives",
                               "/etc/localtime", "/etc/timezone", "/etc/resolv.conf",
                               "/etc/nsswitch.conf", "/etc/hosts", "/etc/hostname",
                               "/etc/ssl", "/etc/ca-certificates"}) {
            if (QFileInfo::exists(d))
                bwArgs << "--ro-bind" << d << d;
        }
        bwArgs << "bash" << "-c" << kLimits + "exec python3 -";
        r = ProcHelper::runWithInput(kBwrap, bwArgs, code.toUtf8(), 15'000);
    } else {
        // Fallback senza bwrap: solo ulimit (meno sicuro)
        qWarning("[REPL] bwrap non trovato — sandbox ridotta a ulimit");
        r = ProcHelper::runWithInput(
            "bash", {"-c", kLimits + "exec python3 -"},
            code.toUtf8(), 15'000);
    }

    QJsonObject obj;
    obj["output"]    = r.out.left(20'000);
    obj["error"]     = r.err.left(4'000);
    obj["exit_code"] = r.code;
    obj["ok"]        = r.ok;
    obj["sandbox"]   = kBwrap.isEmpty() ? QLatin1String("ulimit") : QLatin1String("bwrap");
    sendJson(s.socket, QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

/* ── /api/finanza/cf — calcola Codice Fiscale (D.M. 1976) ───────────────── */

void LanServer::handleFinanzaCf(Session& s)
{
    const QJsonObject body = QJsonDocument::fromJson(s.body).object();
    const QString cognome = body["cognome"].toString().trimmed();
    const QString nome    = body["nome"].toString().trimmed();
    const QString data    = body["data"].toString().trimmed();   /* YYYY-MM-DD */
    const QString sesso   = body["sesso"].toString().trimmed().toUpper();
    const QString comune  = body["comune"].toString().trimmed();

    if (cognome.isEmpty() || nome.isEmpty() || data.isEmpty()) {
        sendError(s.socket, 400, "Campi obbligatori: cognome, nome, data, sesso, comune");
        return;
    }

    const QDate nascita = QDate::fromString(data, Qt::ISODate);
    if (!nascita.isValid()) { sendError(s.socket, 400, "Data non valida (usa YYYY-MM-DD)"); return; }

    const bool maschio = (sesso != "F");
    const QString belfiore = PraticoCalcs::cercaBelfiore(comune);

    QJsonObject obj;
    if (belfiore.isEmpty()) {
        obj["error"] = "Comune non trovato nel database Belfiore. Specifica il codice manualmente.";
        obj["belfiore_hint"] = QString();
    } else {
        const QString cf = PraticoCalcs::calcolaCodiceFiscale(cognome, nome, nascita, maschio, belfiore);
        obj["cf"]       = cf;
        obj["belfiore"] = belfiore;
    }
    sendJson(s.socket, QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

/* ── /api/graph — query GraphMemory ─────────────────────────────────────── */

void LanServer::handleGraphApi(Session& s)
{
    if (!m_graphMemory) {
        sendError(s.socket, 503, "GraphMemory non disponibile (avvia Multi-Agente nella GUI)");
        return;
    }

    /* /api/graph/dot → esporta DOT */
    if (s.path.endsWith("/dot")) {
        const QString dot = m_graphMemory->toDot("Prismalux Memory", 150);
        QJsonObject obj;
        obj["dot"] = dot;
        sendJson(s.socket, QJsonDocument(obj).toJson(QJsonDocument::Compact));
        return;
    }

    /* /api/graph/nodes?q=...&limit=... → cerca nodi */
    const QUrlQuery q(s.queryString);
    const QString query = q.queryItemValue("q", QUrl::FullyDecoded).trimmed();
    const int limit = qBound(1, q.queryItemValue("limit").toInt(), 200);
    const int lim   = (limit > 0) ? limit : 50;

    const QVector<GmNode> nodes = query.isEmpty()
        ? m_graphMemory->allNodes().mid(0, lim)
        : m_graphMemory->searchNodes(query, lim);

    QJsonArray arr;
    for (const GmNode& n : nodes) {
        QJsonObject o;
        o["id"]         = n.id;
        o["type"]       = n.type;
        o["label"]      = n.label;
        o["content"]    = n.content.left(400);
        o["importance"] = static_cast<double>(n.importance);
        arr.append(o);
    }
    QJsonObject obj;
    obj["nodes"] = arr;
    obj["total"] = arr.size();
    sendJson(s.socket, QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

/* ── /bootstrap/ — serve Bootstrap da gui/lan_web/ ───────────────────────── */

void LanServer::handleBootstrap(Session& s)
{
    const QString relPath = s.path.mid(11);  /* strip "/bootstrap/" */

    static const QRegularExpression kTraversal(QStringLiteral("\\.\\."));
    if (relPath.isEmpty() || kTraversal.match(relPath).hasMatch()) {
        sendError(s.socket, 400, "Bad Request");
        return;
    }

    const QString base     = P::root() + "/gui/lan_web";
    const QString filePath = QDir::cleanPath(base + "/" + relPath);
    if (!filePath.startsWith(base + "/")) { /* M-3: blocca path traversal post-clean */
        sendError(s.socket, 400, "Path traversal rilevato");
        return;
    }
    if (!QFileInfo::exists(filePath)) {
        sendError(s.socket, 404, "Bootstrap file not found");
        return;
    }

    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        sendError(s.socket, 500, "Cannot read Bootstrap file");
        return;
    }
    const QByteArray data = f.readAll();
    f.close();

    const QString ext = QFileInfo(filePath).suffix().toLower();
    const char* mime  = "application/octet-stream";
    if      (ext == "css") mime = "text/css; charset=utf-8";
    else if (ext == "js")  mime = "application/javascript; charset=utf-8";

    QByteArray resp = httpOkHeader(mime);
    resp += "Cache-Control: max-age=86400\r\n";
    resp += "Connection: close\r\n";
    resp += "\r\n";
    s.socket->write(resp);
    s.socket->write(data);
    while (s.socket->bytesToWrite() > 0
           && s.socket->state() == QAbstractSocket::ConnectedState)
        s.socket->waitForBytesWritten(5000);
    s.socket->disconnectFromHost();
    s.socket->waitForDisconnected(5000);
}

/* Header di sicurezza comuni a tutte le risposte HTTP */
