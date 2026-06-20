/* ======================================================================
   lan_server_compute.cpp — Handler di calcolo di LanServer
   /api/math · /api/graphviz · /api/whisper · /katex/
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
#include <QUrl>
#include <cmath>
#include "prismalux_paths.h"
namespace P = PrismaluxPaths;

/* ── /api/math — calcoli matematici con Python/sympy/mpmath ─────────────── */

QString LanServer::buildMathPythonCode(const QString& action, const QJsonObject& req)
{
    if (action == "constant") {
        const QString key    = req["key"].toString();
        const int     digits = qBound(10, req["digits"].toInt(100), 100000);

        struct Map { const char* k; const char* expr; const char* label; };
        static const Map table[] = {
            { "pi",          "mp.pi",       "pi greco" },
            { "e",           "mp.e",        "e (numero di Eulero)" },
            { "phi",         "mp.phi",      "phi (sezione aurea)" },
            { "sqrt2",       "mp.sqrt(2)",  "sqrt(2)" },
            { "sqrt3",       "mp.sqrt(3)",  "sqrt(3)" },
            { "sqrt5",       "mp.sqrt(5)",  "sqrt(5)" },
            { "euler_gamma", "mp.euler",    "gamma (Eulero-Mascheroni)" },
            { "ln2",         "mp.log(2)",   "ln(2)" },
            { "catalan",     "mp.catalan",  "C (costante di Catalan)" },
        };
        QString mpExpr, label;
        for (const auto& m : table) {
            if (key == m.k) { mpExpr = m.expr; label = m.label; break; }
        }
        if (mpExpr.isEmpty()) return {};

        return QString(
            "from mpmath import mp\n"
            "mp.dps = %1 + 10\n"
            "val = %2\n"
            "s = mp.nstr(val, %1, strip_zeros=False)\n"
            "print('%3')\n"
            "print(s)\n"
            "print()\n"
            "print('Cifre richieste: %1')\n"
        ).arg(digits).arg(mpExpr).arg(label);
    }

    if (action == "all_constants") {
        const int digits = qBound(10, req["digits"].toInt(100), 1000);
        return QString(
            "from mpmath import mp\n"
            "mp.dps = %1 + 10\n"
            "consts = [\n"
            "    ('pi greco', mp.pi),\n"
            "    ('e (numero di Eulero)', mp.e),\n"
            "    ('phi (sezione aurea)', mp.phi),\n"
            "    ('sqrt(2)', mp.sqrt(2)),\n"
            "    ('sqrt(3)', mp.sqrt(3)),\n"
            "    ('gamma (Eulero-Mascheroni)', mp.euler),\n"
            "    ('ln(2)', mp.log(2)),\n"
            "    ('C (costante di Catalan)', mp.catalan),\n"
            "]\n"
            "for nome, val in consts:\n"
            "    s = mp.nstr(val, %1, strip_zeros=False)\n"
            "    print(f'{nome}')\n"
            "    print(f'  {s}')\n"
            "    print()\n"
        ).arg(digits);
    }

    if (action == "nth") {
        const QString type = req["type"].toString();
        bool ok = false;
        const long long N  = req["n"].toVariant().toLongLong(&ok);
        if (!ok || N < 1) return {};

        if (type == "pi_digit") {
            return QString(
                "from mpmath import mp\n"
                "mp.dps = %1 + 20\n"
                "s = mp.nstr(mp.pi, %1 + 10)\n"
                "digits = s.replace('3.', '').replace('.', '')\n"
                "if %1 <= len(digits):\n"
                "    d = digits[%1 - 1]\n"
                "    print(f'La {%1}-esima cifra decimale di \\u03c0 \\xe8: {d}')\n"
                "    ctx = digits[max(0,%1-6):%1+5]\n"
                "    pos_in_ctx = min(%1-1, 5)\n"
                "    print(f'Contesto: ...{ctx[:pos_in_ctx]}[{d}]{ctx[pos_in_ctx+1:]}...')\n"
                "else:\n"
                "    print('N troppo grande.')\n"
            ).arg(N);
        }
        if (type == "e_digit") {
            return QString(
                "from mpmath import mp\n"
                "mp.dps = %1 + 20\n"
                "s = mp.nstr(mp.e, %1 + 10)\n"
                "digits = s.replace('2.', '').replace('.', '')\n"
                "if %1 <= len(digits):\n"
                "    d = digits[%1 - 1]\n"
                "    print(f'La {%1}-esima cifra decimale di e \\xe8: {d}')\n"
                "    ctx = digits[max(0,%1-6):%1+5]\n"
                "    pos_in_ctx = min(%1-1, 5)\n"
                "    print(f'Contesto: ...{ctx[:pos_in_ctx]}[{d}]{ctx[pos_in_ctx+1:]}...')\n"
                "else:\n"
                "    print('N troppo grande.')\n"
            ).arg(N);
        }
        if (type == "prime") {
            return QString(
                "from sympy import prime\n"
                "import time\n"
                "t = time.time()\n"
                "p = prime(%1)\n"
                "elapsed = time.time() - t\n"
                "print(f'Il {%1}-esimo numero primo \\xe8:')\n"
                "print(f'  p({%1}) = {p}')\n"
                "print(f'  ({len(str(p))} cifre, calcolato in {elapsed:.3f}s)')\n"
            ).arg(N);
        }
        if (type == "fib") {
            return QString(
                "from mpmath import mp, fib\n"
                "mp.dps = 50\n"
                "import time\n"
                "t = time.time()\n"
                "f = int(fib(%1))\n"
                "elapsed = time.time() - t\n"
                "s = str(f)\n"
                "print(f'Il {%1}-esimo numero di Fibonacci:')\n"
                "if len(s) <= 200:\n"
                "    print(f'  F({%1}) = {s}')\n"
                "else:\n"
                "    print(f'  F({%1}) = {s[:80]}...')\n"
                "    print(f'  ...{s[-20:]}')\n"
                "print(f'  ({len(s)} cifre, calcolato in {elapsed:.3f}s)')\n"
            ).arg(N);
        }
        if (type == "fact") {
            return QString(
                "from sympy import factorial\n"
                "import time\n"
                "t = time.time()\n"
                "f = factorial(%1)\n"
                "elapsed = time.time() - t\n"
                "s = str(f)\n"
                "print(f'{%1}! =')\n"
                "if len(s) <= 300:\n"
                "    print(f'  {s}')\n"
                "else:\n"
                "    print(f'  {s[:100]}...')\n"
                "    print(f'  ...{s[-30:]}')\n"
                "print(f'  ({len(s)} cifre, calcolato in {elapsed:.3f}s)')\n"
            ).arg(N);
        }
        if (type == "pow2") {
            return QString(
                "import time\n"
                "t = time.time()\n"
                "v = 2 ** %1\n"
                "elapsed = time.time() - t\n"
                "s = str(v)\n"
                "print(f'2^{%1} =')\n"
                "if len(s) <= 300:\n"
                "    print(f'  {s}')\n"
                "else:\n"
                "    print(f'  {s[:100]}...')\n"
                "    print(f'  ...{s[-30:]}')\n"
                "print(f'  ({len(s)} cifre, calcolato in {elapsed:.3f}s)')\n"
            ).arg(N);
        }
        if (type == "pi_block") {
            return QString(
                "from mpmath import mp\n"
                "mp.dps = %1 + 10\n"
                "s = mp.nstr(mp.pi, %1 + 5)\n"
                "print(f'Le prime {%1} cifre di \\u03c0:')\n"
                "print(s[:%1+2])\n"
            ).arg(N);
        }
        if (type == "phi_block") {
            return QString(
                "from mpmath import mp\n"
                "mp.dps = %1 + 10\n"
                "s = mp.nstr(mp.phi, %1 + 5)\n"
                "print(f'Le prime {%1} cifre di \\u03c6 (sezione aurea):')\n"
                "print(s[:%1+2])\n"
            ).arg(N);
        }
        return {};
    }

    if (action == "expr") {
        const QString expr = req["expr"].toString().trimmed();
        if (expr.isEmpty()) return {};
        const int prec = qBound(10, req["prec"].toInt(50), 10000);
        /* L'espressione viene passata via sys.stdin — nessuna interpolazione nel codice */
        return QString(
            "import sys\n"
            "from sympy import *\n"
            "from sympy import N as Neval\n"
            "from mpmath import mp\n"
            "mp.dps = %1\n"
            "x, y, z, t = symbols('x y z t')\n"
            "n = symbols('n', positive=True, integer=True)\n"
            "_expr_str = sys.stdin.read().strip()\n"
            "try:\n"
            "    result = sympify(_expr_str)\n"
            "except Exception as ex:\n"
            "    print('Errore parsing espressione:', ex); sys.exit(1)\n"
            "print('Espressione:   ', result)\n"
            "try:\n"
            "    simp = simplify(result)\n"
            "    if simp != result: print('Semplificata:  ', simp)\n"
            "except: pass\n"
            "try:\n"
            "    num = Neval(result, %1)\n"
            "    print(f'Valore numerico ({%1} cifre):')\n"
            "    print(f'  {num}')\n"
            "except Exception as ex:\n"
            "    print(f'  (valore numerico non disponibile: {ex})')\n"
        ).arg(prec);
    }

    if (action == "simplify") {
        const QString expr = req["expr"].toString().trimmed();
        if (expr.isEmpty()) return {};
        const int prec = qBound(10, req["prec"].toInt(50), 10000);
        /* L'espressione viene passata via sys.stdin — nessuna interpolazione nel codice */
        return QString(
            "import sys\n"
            "from sympy import *\n"
            "from mpmath import mp\n"
            "mp.dps = %1\n"
            "x = symbols('x')\n"
            "_expr_str = sys.stdin.read().strip()\n"
            "try:\n"
            "    expr = sympify(_expr_str)\n"
            "except Exception as ex:\n"
            "    print('Errore parsing espressione:', ex); sys.exit(1)\n"
            "print('Espressione:  ', expr)\n"
            "print('Semplificata: ', simplify(expr))\n"
            "print('Fattorizzata: ', factor(expr))\n"
            "try:\n"
            "    print('Valore numerico:', N(expr, %1))\n"
            "except: pass\n"
        ).arg(prec);
    }

    if (action == "sequence_sympy") {
        const QJsonArray seqArr = req["seq"].toArray();
        if (seqArr.isEmpty()) return {};
        const int nxt = qBound(1, req["next"].toInt(5), 50);

        QString listStr = "[";
        for (int i = 0; i < seqArr.size(); ++i) {
            const double v = seqArr[i].toDouble();
            const long long iv = static_cast<long long>(v);
            if (static_cast<double>(iv) == v)
                listStr += QString::number(iv);
            else
                listStr += QString::number(v, 'g', 17);
            if (i < seqArr.size() - 1) listStr += ", ";
        }
        listStr += "]";

        return QString(
            "from sympy import symbols, interpolating_poly, factor, simplify, Integer, nsimplify\n"
            "from sympy import factorint, isprime, fibonacci as fib\n"
            "import sys\n"
            "seq = %1\n"
            "n = symbols('n')\n"
            "N = len(seq)\n"
            "print('Sequenza:', seq)\n"
            "print(f'Termini: {N}')\n"
            "print()\n"
            "try:\n"
            "    pts = list(enumerate(seq, 1))\n"
            "    poly = interpolating_poly(N, n, pts)\n"
            "    fpoly = factor(simplify(poly))\n"
            "    print('Formula polinomiale (interpolazione):')\n"
            "    print(f'  a(n) = {fpoly}')\n"
            "    print()\n"
            "    print('Termini successivi:')\n"
            "    for i in range(N+1, N+%2+1):\n"
            "        print(f'  a({i}) = {fpoly.subs(n, i)}')\n"
            "except Exception as e:\n"
            "    print(f'Interpolazione fallita: {e}')\n"
            "print()\n"
            "diffs = [seq[i+1]-seq[i] for i in range(len(seq)-1)]\n"
            "diffs2 = [diffs[i+1]-diffs[i] for i in range(len(diffs)-1)] if len(diffs)>1 else []\n"
            "print(f'Prime differenze:  {diffs}')\n"
            "if diffs2: print(f'Seconde differenze: {diffs2}')\n"
        ).arg(listStr).arg(nxt);
    }

    return {};
}

void LanServer::handleMath(QTcpSocket* sock, const Session& s)
{
    const QJsonObject req    = QJsonDocument::fromJson(s.body).object();
    const QString     action = req["action"].toString();

    const QString pyCode = buildMathPythonCode(action, req);
    if (pyCode.isEmpty()) {
        const QByteArray err = R"({"error":"action non riconosciuta o parametri mancanti"})";
        sendJson(sock, err);
        return;
    }

    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start(P::findPython(), QStringList{"-c", pyCode});
    if (!proc.waitForStarted(3000)) {
        sendJson(sock, R"({"error":"python3 non trovato o non avviato"})");
        return;
    }
    /* Per le action "expr"/"simplify" l'espressione viene passata via stdin
       (il codice Python usa sys.stdin.read()) invece di essere interpolata nel codice. */
    if (action == "expr" || action == "simplify") {
        const QString expr = req["expr"].toString().trimmed();
        proc.write(expr.toUtf8());
    }
    proc.closeWriteChannel();
    proc.waitForFinished(30000);
    const QString out  = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    const int     code = proc.exitCode();

    QJsonObject resp;
    if (code != 0)
        resp["error"] = out.isEmpty() ? "Errore Python (exit code non zero)" : out;
    else
        resp["result"] = out;
    sendJson(sock, QJsonDocument(resp).toJson(QJsonDocument::Compact));
}

/* ── /api/graphviz — renderizza DOT con graphviz dot -Tpng ──────────────── */

void LanServer::handleGraphviz(const Session& s)
{
    const QJsonObject req = QJsonDocument::fromJson(s.body).object();
    const QString dot = req["dot"].toString().trimmed();
    if (dot.isEmpty()) {
        sendError(s.socket, 400, "dot field required");
        return;
    }

    /* Blocca attributi Graphviz che referenziano file locali (disclosure) */
    static const QRegularExpression kDotDangerousAttr(
        R"(\b(image|shapefile|fontpath|imagepath)\s*=)",
        QRegularExpression::CaseInsensitiveOption);
    if (kDotDangerousAttr.match(dot).hasMatch()) {
        sendError(s.socket, 400, "DOT contiene attributi file non consentiti (image/shapefile/fontpath)");
        return;
    }

    QProcess proc;
    proc.setProcessChannelMode(QProcess::SeparateChannels);
    /* -Gimagepath= vuoto impedisce il caricamento di immagini da disco */
    proc.start("dot", QStringList{"-Tpng", "-Gimagepath="});
    if (!proc.waitForStarted(3000)) {
        QJsonObject err;
        err["error"] = "graphviz (dot) non trovato. Installa graphviz sul server desktop.";
        sendJson(s.socket, QJsonDocument(err).toJson(QJsonDocument::Compact));
        return;
    }
    proc.write(dot.toUtf8());
    proc.closeWriteChannel();
    proc.waitForFinished(15000);

    if (proc.exitCode() != 0) {
        const QString errMsg = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        QJsonObject err;
        err["error"] = errMsg.isEmpty() ? "Errore rendering Graphviz" : errMsg;
        sendJson(s.socket, QJsonDocument(err).toJson(QJsonDocument::Compact));
        return;
    }

    const QByteArray png = proc.readAllStandardOutput();
    QJsonObject resp;
    resp["png"] = QString::fromLatin1(png.toBase64());
    sendJson(s.socket, QJsonDocument(resp).toJson(QJsonDocument::Compact));
}

/* ── /api/whisper — trascrizione audio via whisper.cpp o whisper CLI ────── */

void LanServer::handleWhisper(const Session& s)
{
    static constexpr int kMaxWhisperBytes = 25 * 1024 * 1024;
    if (s.body.size() > kMaxWhisperBytes) {
        sendError(s.socket, 413, "File audio troppo grande (max 25 MB)");
        return;
    }

    {
        const int headerCt = [&]() -> int {
            const QByteArray lower = s.body.left(512).toLower();
            return lower.indexOf("content-type:");
        }();
        if (headerCt >= 0) {
            const int lf = s.body.indexOf('\n', headerCt);
            const QByteArray partCt = s.body.mid(headerCt + 13, lf - headerCt - 13).trimmed().toLower();
            const bool isAudio = partCt.startsWith("audio/")
                              || partCt.startsWith("video/")
                              || partCt.contains("octet-stream");
            if (!isAudio) {
                sendError(s.socket, 415, "Tipo file non supportato — richiesto audio/*");
                return;
            }
        }
    }

    /* Cerca il file audio nel multipart body.
       Per semplicità: il client invia multipart/form-data con campo "audio".
       Estraiamo i byte grezzi dal body cercando la sequenza dopo il doppio CRLF
       dell'header part e prima del boundary finale. */
    const QByteArray ct = [&]() -> QByteArray {
        /* cerca Content-Type nella sessione */
        const int cti = s.buf.indexOf("Content-Type:");
        if (cti < 0) return {};
        const int lf = s.buf.indexOf('\n', cti);
        return s.buf.mid(cti + 13, lf - cti - 13).trimmed();
    }();

    /* Estrai boundary */
    const int bi = ct.indexOf("boundary=");
    if (bi < 0) {
        sendError(s.socket, 400, "multipart boundary non trovato");
        return;
    }
    const QByteArray boundary = "--" + ct.mid(bi + 9).trimmed();

    /* Trova inizio dati audio (dopo doppio CRLF dell'header part) */
    const int partStart = s.body.indexOf(boundary);
    if (partStart < 0) {
        sendError(s.socket, 400, "multipart body non valido");
        return;
    }
    const int hdrEnd = s.body.indexOf("\r\n\r\n", partStart);
    if (hdrEnd < 0) {
        sendError(s.socket, 400, "header part non trovato");
        return;
    }
    const int dataStart = hdrEnd + 4;
    const QByteArray endBoundary = "\r\n" + boundary + "--";
    int dataEnd = s.body.indexOf(endBoundary, dataStart);
    if (dataEnd < 0) dataEnd = s.body.size();
    const QByteArray audioData = s.body.mid(dataStart, dataEnd - dataStart);

    if (audioData.isEmpty()) {
        sendError(s.socket, 400, "file audio vuoto");
        return;
    }

    /* M-6: QTemporaryFile con nome univoco (evita TOCTOU su timestamp) */
    QTemporaryFile tmpFile(QDir::tempPath() + "/prismalux_wsp_XXXXXX.wav");
    tmpFile.setAutoRemove(false); /* rimosso manualmente dopo l'uso da QFile::remove */
    if (!tmpFile.open()) {
        sendError(s.socket, 500, "impossibile scrivere file temporaneo");
        return;
    }
    const QString tmpPath = tmpFile.fileName();
    tmpFile.write(audioData);
    tmpFile.close();

    /* Prova prima whisper-cpp (whisper), poi whisper CLI Python */
    QStringList candidates = {"whisper-cpp", "whisper"};
    QString whisperBin;
    for (const QString& c : candidates) {
        QProcess test;
        test.start(c, {"--help"});
        if (test.waitForStarted(1000)) { whisperBin = c; test.kill(); break; }
    }

    if (whisperBin.isEmpty()) {
        QFile::remove(tmpPath);
        QJsonObject err;
        err["error"] = "whisper non trovato. Installa whisper.cpp o openai-whisper sul server desktop.";
        sendJson(s.socket, QJsonDocument(err).toJson(QJsonDocument::Compact));
        return;
    }

    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    if (whisperBin == "whisper") {
        /* openai-whisper: whisper file.wav --model tiny --output_format txt --output_dir /tmp */
        proc.start(whisperBin, QStringList{tmpPath, "--model", "tiny",
                                           "--output_format", "txt",
                                           "--output_dir", QDir::tempPath()});
    } else {
        /* whisper.cpp: whisper-cpp -m model.bin -f file.wav */
        proc.start(whisperBin, QStringList{"-f", tmpPath});
    }

    if (!proc.waitForStarted(5000)) {
        QFile::remove(tmpPath);
        sendError(s.socket, 500, "impossibile avviare whisper");
        return;
    }
    proc.waitForFinished(120000); /* max 2 minuti */
    QFile::remove(tmpPath);

    QString text = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();

    /* Se openai-whisper ha scritto un file .txt separato, leggiamolo */
    if (text.isEmpty() || whisperBin == "whisper") {
        const QString txtPath = QDir::tempPath() + "/" +
            QFileInfo(tmpPath).completeBaseName() + ".txt";
        QFile txtFile(txtPath);
        if (txtFile.open(QIODevice::ReadOnly)) {
            text = QString::fromUtf8(txtFile.readAll()).trimmed();
            txtFile.close();
            QFile::remove(txtPath);
        }
    }

    QJsonObject resp;
    if (text.isEmpty())
        resp["error"] = "Trascrizione vuota o fallita";
    else
        resp["text"] = text;
    sendJson(s.socket, QJsonDocument(resp).toJson(QJsonDocument::Compact));
}

/* ── /katex/ — serve KaTeX da disco locale ───────────────────────────────── */

void LanServer::handleKatex(Session& s)
{
    const QString relPath = s.path.mid(7);  /* strip "/katex/" */

    static const QRegularExpression kTraversal(QStringLiteral("\\.\\."));
    if (relPath.isEmpty() || kTraversal.match(relPath).hasMatch()) {
        sendError(s.socket, 400, "Bad Request");
        return;
    }

    static const QStringList kSearchDirs = {
        QStringLiteral("/usr/share/javascript/katex"),
        QStringLiteral("/usr/share/katex"),
    };

    QString filePath;
    for (const QString& base : kSearchDirs) {
        const QString candidate = QDir::cleanPath(base + "/" + relPath);
        if (!candidate.startsWith(base + "/")) continue; /* M-3: blocca path traversal post-clean */
        if (QFileInfo::exists(candidate)) {
            filePath = candidate;
            break;
        }
    }

    if (filePath.isEmpty()) {
        sendError(s.socket, 404, "KaTeX file not found");
        return;
    }

    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        sendError(s.socket, 500, "Cannot read KaTeX file");
        return;
    }
    const QByteArray data = f.readAll();
    f.close();

    const QString ext = QFileInfo(filePath).suffix().toLower();
    const char* mime  = "application/octet-stream";
    if      (ext == "css")  mime = "text/css; charset=utf-8";
    else if (ext == "js")   mime = "application/javascript; charset=utf-8";
    else if (ext == "woff") mime = "font/woff";
    else if (ext == "woff2")mime = "font/woff2";
    else if (ext == "ttf")  mime = "font/ttf";

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

/* ── /api/file — estrai testo da PDF/DOCX/TXT/CSV/codice ────────────────── */

