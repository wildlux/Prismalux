/* ══════════════════════════════════════════════════════════════
   agenti_page_tools.cpp — Tool di pre-elaborazione per LLM

   Contiene "guardie strumento" che intercettano il task PRIMA di
   inviarlo all'AI, generano dati reali e li iniettano nel prompt
   come contesto, così l'LLM lavora su numeri veri e non allucina.

   Tool presenti:
     _inject_random(task) — genera numeri casuali con std::random_device
                            (equivalente alla randomizzazione C nativa)
                            e li inietta nel task come [DATI RANDOM: ...].

   Pattern d'uso in runPipeline():
     m_taskOriginal = _inject_random(_inject_math(_inject_science(task)));
   ══════════════════════════════════════════════════════════════ */
#include "ai_random_inject.cpp"   /* _inject_random e helpers puri, senza dep UI */
#include "main_ai.h"
#include "main_ai_p.h"
#include "dialog_agents_config.h"
#include "../prismalux_paths.h"
#include "../app_config.h"
#include "../graph_memory.h"
#include "../widgets/path_guard.h"
namespace P = PrismaluxPaths;
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QProcess>
#include <QTimer>
#include <QDateTime>
#include <QTextCursor>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QDialog>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QFont>
#include <random>
#include <cmath>

/* ── Helper: tronca risultati tool lunghi con suffisso leggibile ── */
static QString _truncateResult(const QString& s, int maxLen = 2000)
{
    if (s.size() <= maxLen) return s;
    return s.left(maxLen) + "\n[...troncato a " + QString::number(maxLen) + " caratteri]";
}

/* ── Helper: parse risposta JSON-RPC 2.0 tools/call (id=2) ── */
/* Cerca la prima riga JSON con "id":2 nel buffer raw e ne estrae il risultato.
 * Restituisce "" se il buffer non contiene ancora una risposta completa (JSON parziale
 * o nessuna riga con id=2) — il chiamante interpreta "" come "riprova / retry".
 * Non restituisce mai "" per errori MCP: quelli producono una stringa di errore non vuota. */
static QString _parseMcpOutput(const QByteArray& raw,
                               const QString& plugin, const QString& toolName)
{
    for (const QByteArray& line : raw.split('\n')) {
        const QByteArray l = line.trimmed();
        if (l.isEmpty()) continue;
        const QJsonObject obj = QJsonDocument::fromJson(l).object();
        if (obj.value("id").toInt() != 2) continue;
        if (obj.contains("error")) {
            return QString("[MCP %1::%2 errore] %3").arg(plugin, toolName,
                obj.value("error").toObject().value("message").toString("errore sconosciuto"));
        }
        const QJsonObject res = obj.value("result").toObject();
        QString text;
        for (const auto& c : res.value("content").toArray()) {
            if (c.toObject().value("type").toString() == "text")
                text += c.toObject().value("text").toString() + "\n";
        }
        if (text.isEmpty())
            text = QJsonDocument(res).toJson(QJsonDocument::Compact);
        return _truncateResult(text.trimmed());
    }
    return {};   /* stringa vuota = nessuna risposta valida trovata */
}

/* ── Cache DuckDuckGo: QHash<query, {risultato, timestamp_ms}> — TTL 5 min ── */
static QHash<QString, QPair<QString, qint64>> s_ddgCache;

/* ── MCP Tool Discovery cache: plugin → "tool1(desc), tool2(desc)" ── */
static QHash<QString, QString> s_mcpToolDescs;
static bool s_mcpDiscoveryDone = false;

/* ── MCP Keepalive: plugin → processo ancora vivo ── */
static QHash<QString, QPointer<QProcess>> s_mcpAlive;

/* Avvia un nuovo processo MCP, lo registra in s_mcpAlive e invia l'handshake iniziale.
 * Ritorna nullptr se waitForStarted fallisce (proc già distrutto). */
static QProcess* _launchMcpProcess(const QString& pythonExe,
                                    const QString& serverPath,
                                    const QString& plugin,
                                    const QByteArray& initMsg)
{
    auto* proc = new QProcess(qApp);
    proc->setProgram(pythonExe);
    proc->setArguments({ serverPath });
    proc->setProcessChannelMode(QProcess::SeparateChannels);
    proc->start();
    QObject::connect(proc, &QProcess::errorOccurred, proc, [proc](QProcess::ProcessError err) {
        if (err == QProcess::FailedToStart)
            qWarning() << "[main_ai_tools] processo MCP non avviato:" << proc->program();
    });
    if (!proc->waitForStarted(P::kProcessStartTimeoutMs)) {
        /* waitForStarted() può restituire false per timeout anche se il
         * processo è comunque partito (es. sistema sotto carico) — killare
         * prima di distruggere, altrimenti il figlio resta orfano. */
        if (proc->state() != QProcess::NotRunning) proc->kill();
        proc->deleteLater();
        return nullptr;
    }
    proc->write(initMsg);
    s_mcpAlive[plugin] = proc;
    QObject::connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                     proc, [plugin](int, QProcess::ExitStatus) { s_mcpAlive.remove(plugin); });
    return proc;
}

/* Collega readyRead + timeout a un processo MCP già avviato.
 * onDone(result) è chiamato esattamente una volta:
 *   - result non vuoto → risposta MCP (successo o errore JSON-RPC)
 *   - result vuoto    → timeout; il caller può fare retry oppure segnalare errore */
template<typename F>
static void _attachMcpReader(QProcess* proc, QObject* ctx,
                              const QString& plugin, const QString& toolName,
                              F onDone)
{
    auto buf    = QSharedPointer<QByteArray>::create();
    auto called = QSharedPointer<bool>::create(false);
    auto conn   = QSharedPointer<QMetaObject::Connection>::create();

    *conn = QObject::connect(proc, &QProcess::readyRead, ctx,
        [proc, buf, called, conn, onDone, plugin, toolName]() mutable {
            *buf += proc->readAllStandardOutput();
            if (*called) return;
            const QString r = _parseMcpOutput(*buf, plugin, toolName);
            if (r.isEmpty()) return;
            *buf = {};
            *called = true;
            QObject::disconnect(*conn);
            onDone(r);
        });

    QTimer::singleShot(P::mcpTimeoutMs(plugin), ctx,
        [proc, called, conn, onDone, plugin, toolName]() mutable {
            if (*called) return;
            *called = true;
            QObject::disconnect(*conn);
            s_mcpAlive.remove(plugin);
            proc->kill();
            onDone({});
        });
}

/* ══════════════════════════════════════════════════════════════
   Tool Use Nativo — implementazione
   Integrato nel pipeline esistente via hook in onFinished(Pipeline).
   ══════════════════════════════════════════════════════════════ */

QString AgentiPage::toolSystemSuffix()
{
    const QString proj = P::root();

    /* Lista plugin MCP disponibili costruita dinamicamente */
    QStringList mcpPlugins;
    const QDir mcpDir(proj + "/MCPs");
    if (mcpDir.exists()) {
        for (const QString& d : mcpDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
            if (QFile::exists(proj + "/MCPs/" + d + "/server.py"))
                mcpPlugins << d;
    }
    /* Lista arricchita con tool names se discovery disponibile */
    QString mcpList;
    for (const QString& p : mcpPlugins) {
        if (!mcpList.isEmpty()) mcpList += ", ";
        const QString desc = s_mcpToolDescs.value(p);
        mcpList += desc.isEmpty() ? p : p + "[" + desc + "]";
    }
    const QString mcpBlock = mcpPlugins.isEmpty() ? QString() :
        QString::fromUtf8("\n[MCP PLUGIN disponibili via mcp_call]: ") +
        mcpList +
        QString::fromUtf8(
            "\nEsempio: TOOL_CALL: {\"tool\": \"mcp_call\", "
            "\"input\": \"{\\\"plugin\\\": \\\"knowledge_mcp\\\", "
            "\\\"tool_name\\\": \\\"get_knowledge\\\", \\\"args_json\\\": \\\"{}\\\"}\"}\n"
            "Usa mcp_call per accedere a funzionalita' avanzate non coperte dai tool built-in.");

    return
        QString::fromUtf8(
            "\n\n[STRUMENTI DISPONIBILI - usali solo se necessario]\n"
            "TOOL_CALL: {\"tool\": \"calc\", \"input\": \"sqrt(144)\"}\n"
            "TOOL_CALL: {\"tool\": \"ricerca\", \"input\": \"query\"}\n"
            "TOOL_CALL: {\"tool\": \"python\", \"input\": \"print(2+2)\"}\n"
            "TOOL_CALL: {\"tool\": \"get_datetime\", \"input\": \"\"}\n"
            "TOOL_CALL: {\"tool\": \"date_calc\", \"input\": \"quanti mesi mancano a dicembre\"}\n"
            "TOOL_CALL: {\"tool\": \"date_calc\", \"input\": \"convertimi 120 minuti in ore\"}\n"
            "TOOL_CALL: {\"tool\": \"date_calc\", \"input\": \"quanti secondi sono un anno solare\"}\n"
            "TOOL_CALL: {\"tool\": \"leggi_file\", \"input\": \"PERCORSO_ESPLICITO\"}\n"
            "TOOL_CALL: {\"tool\": \"lista_file\", \"input\": \"PERCORSO_ESPLICITO\"}\n"
            "TOOL_CALL: {\"tool\": \"leggi_riassunto\", \"input\": \"breve|dettagliato\"}\n"
            "TOOL_CALL: {\"tool\": \"scrivi_riassunto\", \"input\": \"breve|||testo\"}\n"
            "Scrivi UNA riga TOOL_CALL: {...} e attendi TOOL_RESULT.\n"
            "REGOLA FILE: usa leggi_file/lista_file SOLO con percorsi forniti dall'utente\n"
            "o dentro la cartella del progetto:\n") +
        proj + "/MCPs  oppure  " + proj + QString::fromUtf8(
            "/Tools\n"
            "NON inventare percorsi /home/... — per cultura generale rispondi direttamente.\n"
            "TOOL_CALL: {\"tool\": \"spawn_agent\", \"input\": \"Ruolo|||Compito completo\"}\n"
            "spawn_agent: crea un sub-agente specializzato (max 4 per sessione) — utile per sotto-task complessi.") +
        mcpBlock;
}

/* ══════════════════════════════════════════════════════════════
   startMcpDiscovery — esegue tools/list su ogni plugin MCP in sequenza
   (lambda ricorsiva via shared_ptr<function>) e popola s_mcpToolDescs.
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::startMcpDiscovery()
{
    if (s_mcpDiscoveryDone) return;
    s_mcpDiscoveryDone = true;

    auto plugins = std::make_shared<QStringList>();
    const QDir mcpDir(P::root() + "/MCPs");
    if (!mcpDir.exists()) return;
    for (const QString& d : mcpDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
        if (QFile::exists(P::root() + "/MCPs/" + d + "/server.py"))
            *plugins << d;
    if (plugins->isEmpty()) return;

    const QString python     = P::findPython();
    const QString sharedVenv = QDir::homePath() + "/.prismalux/venv/bin/python";
    const QString mcpRoot    = P::root() + "/MCPs";

    /* Lambda ricorsiva: si cattura da sola tramite shared_ptr<function> */
    auto fn = std::make_shared<std::function<void()>>();
    *fn = [this, fn, plugins, python, sharedVenv, mcpRoot]() mutable {
        if (plugins->isEmpty()) return;
        const QString plugin     = plugins->takeFirst();
        const QString serverPath = mcpRoot + "/" + plugin + "/server.py";
        const QString venvPy     = mcpRoot + "/" + plugin + "/venv/bin/python";
        const QString py         = QFile::exists(venvPy)    ? venvPy
                                 : QFile::exists(sharedVenv) ? sharedVenv
                                 : python;

        const QByteArray initMsg = QJsonDocument(QJsonObject{
            {"jsonrpc","2.0"},{"id",1},{"method","initialize"},
            {"params",QJsonObject{{"protocolVersion","2024-11-05"},
                {"capabilities",QJsonObject{}},
                {"clientInfo",QJsonObject{{"name","prismalux"},{"version","3.0"}}}}}
        }).toJson(QJsonDocument::Compact) + "\n";
        const QByteArray listMsg = QJsonDocument(QJsonObject{
            {"jsonrpc","2.0"},{"id",2},{"method","tools/list"},{"params",QJsonObject{}}
        }).toJson(QJsonDocument::Compact) + "\n";

        auto* proc = new QProcess(this);
        proc->setProgram(py);
        proc->setArguments({ serverPath });
        proc->setProcessChannelMode(QProcess::SeparateChannels);
        proc->start();
        connect(proc, &QProcess::errorOccurred, this, [proc](QProcess::ProcessError err) {
            if (err == QProcess::FailedToStart)
                qWarning() << "[main_ai_tools] discovery MCP non avviato:" << proc->program();
        });
        if (!proc->waitForStarted(2000)) {
            if (proc->state() != QProcess::NotRunning) proc->kill();
            proc->deleteLater();
            (*fn)();
            return;
        }
        proc->write(initMsg);
        proc->write(listMsg);
        proc->closeWriteChannel();

        connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, proc, plugin, fn](int, QProcess::ExitStatus) mutable {
            proc->deleteLater();
            for (const QByteArray& line : proc->readAllStandardOutput().split('\n')) {
                const QByteArray l = line.trimmed();
                if (l.isEmpty()) continue;
                const QJsonObject obj = QJsonDocument::fromJson(l).object();
                if (obj.value("id").toInt() != 2) continue;
                const QJsonArray tools = obj.value("result").toObject().value("tools").toArray();
                QStringList names;
                for (const auto& t : tools) {
                    const QString nm = t.toObject().value("name").toString();
                    if (!nm.isEmpty()) names << nm;
                }
                if (!names.isEmpty()) s_mcpToolDescs[plugin] = names.join("|");
                break;
            }
            (*fn)();
        });
        QTimer::singleShot(8000, proc, [proc]{
            if (proc->state() != QProcess::NotRunning) proc->kill();
        });
    };

    QTimer::singleShot(500, this, [fn]{ (*fn)(); });
}

QJsonObject AgentiPage::detectFirstToolCall(const QString& text)
{
    static const QRegularExpression re(
        "TOOL_CALL:\\s*(\\{[^\\n\\r]+\\})",
        QRegularExpression::CaseInsensitiveOption);
    const auto m = re.match(text);
    if (!m.hasMatch()) return {};
    return QJsonDocument::fromJson(m.captured(1).toUtf8()).object();
}

void AgentiPage::runToolCall(const QJsonObject& call,
                              std::function<void(QString)> onDone)
{
    const QString tool  = call["tool"].toString().toLower().trimmed();
    const QString input = call["input"].toString().trimmed();

    /* ── Calcolatrice ── */
    if (tool == "calc" || tool == "calcolatrice" || tool == "math") {
        const QString safeInput = input.left(500);
        /* Qt6: QJsonDocument non accetta QJsonValue direttamente — usa QJsonArray come wrapper */
        QJsonArray _ca; _ca.append(safeInput);
        const QString _safeJson = QString::fromUtf8(
            QJsonDocument(_ca).toJson(QJsonDocument::Compact)).mid(1).chopped(1);
        const QString script =
            "import math,statistics\n"
            "try:\n"
            "    g=dict(vars(math))\n"
            "    g.update(vars(statistics))\n"
            "    g['__builtins__']={}\n"
            "    print(eval(" + _safeJson + ",g,{}))\n"
            "except Exception as e:\n"
            "    print('ERRORE:',e)\n";
        auto* proc = new QProcess(this);
        proc->setProcessChannelMode(QProcess::MergedChannels);
        connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                this, [proc, onDone](int, QProcess::ExitStatus) {
            const QString out = QString::fromUtf8(proc->readAll()).trimmed();
            proc->deleteLater();
            onDone(out.isEmpty() ? "nessun risultato" : out);
        });
        connect(proc, &QProcess::errorOccurred, this, [proc](QProcess::ProcessError err) {
            if (err == QProcess::FailedToStart)
                qWarning() << "[main_ai_tools] calc non avviato:" << proc->program();
        });
        proc->start(P::findPython(), {"-c", script});
        QTimer::singleShot(5000, proc, [proc, onDone]{
            if (proc->state() != QProcess::NotRunning) { proc->kill(); onDone("timeout"); }
        });
        return;
    }

    /* ── Ricerca Web DuckDuckGo Instant Answer ── */
    if (tool == "ricerca" || tool == "search" || tool == "web") {
        if (input.isEmpty()) { onDone("errore: query di ricerca non specificata"); return; }
        /* Se l'input sembra un URL, reindirizza a fetch_url */
        const bool looksLikeUrl = input.startsWith("http://") || input.startsWith("https://")
                                  || input.startsWith("www.");
        if (looksLikeUrl) {
            const QString url = input.startsWith("www.") ? "https://" + input : input;
            QJsonObject fakeCall;
            fakeCall["tool"]  = QString("fetch_url");
            fakeCall["input"] = url;
            runToolCall(fakeCall, onDone);
            return;
        }
        /* Cache TTL 5 min: evita richieste duplicate nella stessa sessione */
        const QString cacheKey = input.left(200).toLower().trimmed();
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        if (s_ddgCache.contains(cacheKey)) {
            const auto& cached = s_ddgCache[cacheKey];
            if (nowMs - cached.second < 5 * 60 * 1000) {
                onDone(cached.first);
                return;
            }
            s_ddgCache.remove(cacheKey);
        }
        QJsonArray _ra; _ra.append(input.left(200));
        const QString _inputJson = QString::fromUtf8(
            QJsonDocument(_ra).toJson(QJsonDocument::Compact)).mid(1).chopped(1);
        const QString script =
            "import urllib.request,urllib.parse,json\n"
            "q=urllib.parse.quote_plus(" + _inputJson + ")\n"
            "url=f'https://api.duckduckgo.com/?q={q}&format=json&no_redirect=1&no_html=1'\n"
            "try:\n"
            "    req=urllib.request.Request(url,headers={'User-Agent':'Mozilla/5.0'})\n"
            "    with urllib.request.urlopen(req,timeout=7) as r: d=json.load(r)\n"
            "    out=[]\n"
            "    if d.get('AbstractText'): out.append(d['AbstractText'][:400])\n"
            "    elif d.get('Answer'): out.append(d['Answer'][:400])\n"
            "    for t in d.get('RelatedTopics',[])[:5]:\n"
            "        if isinstance(t,dict) and t.get('Text'): out.append(t['Text'][:200])\n"
            "    print('\\n'.join(out) if out else 'nessun risultato — prova fetch_url se hai un URL diretto')\n"
            "except Exception as e: print('ERRORE:',e)\n";
        auto* proc = new QProcess(this);
        proc->setProcessChannelMode(QProcess::MergedChannels);
        connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                this, [proc, onDone, cacheKey](int, QProcess::ExitStatus) {
            const QString out = _truncateResult(QString::fromUtf8(proc->readAll()).trimmed(), 600);
            proc->deleteLater();
            const QString result = out.isEmpty() ? "nessun risultato" : out;
            s_ddgCache[cacheKey] = { result, QDateTime::currentMSecsSinceEpoch() };
            onDone(result);
        });
        connect(proc, &QProcess::errorOccurred, this, [proc](QProcess::ProcessError err) {
            if (err == QProcess::FailedToStart)
                qWarning() << "[main_ai_tools] ricerca non avviata:" << proc->program();
        });
        proc->start(P::findPython(), {"-c", script});
        QTimer::singleShot(12000, proc, [proc, onDone]{
            if (proc->state() != QProcess::NotRunning) { proc->kill(); onDone("timeout ricerca"); }
        });
        return;
    }

    /* ── Scarica pagina web (fetch URL) ── */
    if (tool == "fetch_url" || tool == "scarica_pagina" || tool == "fetch" || tool == "url") {
        if (input.isEmpty()) { onDone("errore: URL non specificato"); return; }
        const QString url = (input.startsWith("http://") || input.startsWith("https://"))
                            ? input : "https://" + input;
        QJsonArray _ua; _ua.append(url.left(500));
        const QString _urlJson = QString::fromUtf8(
            QJsonDocument(_ua).toJson(QJsonDocument::Compact)).mid(1).chopped(1);
        /* SSRF guard: rifiuta host che risolvono a IP loopback/privati/link-local,
           sia per l'URL iniziale sia per ogni redirect. Mitiga prompt-injection→SSRF
           (es. http://127.0.0.1:11434 verso Ollama, o metadata cloud 169.254.169.254). */
        const QString script =
            "import urllib.request,urllib.parse,ipaddress,socket,html\n"
            "def _blocked(host):\n"
            "    if not host: return True\n"
            "    try: infos=socket.getaddrinfo(host,None)\n"
            "    except Exception: return True\n"
            "    for info in infos:\n"
            "        try: a=ipaddress.ip_address(info[4][0])\n"
            "        except Exception: return True\n"
            "        if a.is_private or a.is_loopback or a.is_link_local or a.is_reserved or a.is_multicast or a.is_unspecified: return True\n"
            "    return False\n"
            "class _SafeRedirect(urllib.request.HTTPRedirectHandler):\n"
            "    def redirect_request(self,req,fp,code,msg,headers,newurl):\n"
            "        if _blocked(urllib.parse.urlparse(newurl).hostname): return None\n"
            "        return super().redirect_request(req,fp,code,msg,headers,newurl)\n"
            "url=" + _urlJson + "\n"
            "try:\n"
            "    p=urllib.parse.urlparse(url)\n"
            "    if p.scheme not in ('http','https') or _blocked(p.hostname):\n"
            "        print('ERRORE: URL non consentito (host interno/privato o schema non valido)')\n"
            "    else:\n"
            "        req=urllib.request.Request(url,headers={"
            "'User-Agent':'Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36',"
            "'Accept':'text/html,application/xhtml+xml,*/*','Accept-Language':'it,en;q=0.9'})\n"
            "        opener=urllib.request.build_opener(_SafeRedirect)\n"
            "        with opener.open(req,timeout=10) as r:\n"
            "            raw=r.read()\n"
            "            enc=r.headers.get_content_charset('utf-8')\n"
            "            content=raw.decode(enc,errors='replace')\n"
            "        print(content[:4000])\n"
            "except Exception as e: print('ERRORE:',e)\n";
        auto* proc = new QProcess(this);
        proc->setProcessChannelMode(QProcess::MergedChannels);
        connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                this, [proc, onDone](int, QProcess::ExitStatus) {
            const QString out = _truncateResult(QString::fromUtf8(proc->readAll()).trimmed());
            proc->deleteLater();
            onDone(out.isEmpty() ? "nessun contenuto ricevuto" : out);
        });
        connect(proc, &QProcess::errorOccurred, this, [proc](QProcess::ProcessError err) {
            if (err == QProcess::FailedToStart)
                qWarning() << "[main_ai_tools] fetch_url non avviato:" << proc->program();
        });
        proc->start(P::findPython(), {"-c", script});
        QTimer::singleShot(15000, proc, [proc, onDone]{
            if (proc->state() != QProcess::NotRunning) { proc->kill(); onDone("timeout fetch_url"); }
        });
        return;
    }

    /* ── Python generico — sandboxed via Docker se disponibile ── */
    if (tool == "python" || tool == "py") {
        const QString code = input.left(2000);
        auto* proc = new QProcess(this);
        proc->setProcessChannelMode(QProcess::MergedChannels);

        connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                this, [proc, onDone](int, QProcess::ExitStatus) {
            const QString out = QString::fromUtf8(proc->readAll()).trimmed();
            proc->deleteLater();
            onDone(out.isEmpty() ? "(nessun output)" : out.left(600));
        });
        connect(proc, &QProcess::errorOccurred, this, [proc](QProcess::ProcessError err) {
            if (err == QProcess::FailedToStart)
                qWarning() << "[main_ai_tools] python non avviato:" << proc->program();
        });

        if (P::isSandboxReady()) {
            auto& ss = AppConfig::s();
            const QString img = ss.value(P::SK::kSandboxImage, "python:3.11-slim").toString();
            const QString mem = QString::number(ss.value(P::SK::kSandboxMemory, 256).toInt()) + "m";
            proc->start(P::findDocker(), {
                "run", "--rm",
                "--network", "none",
                "--memory",  mem,
                "--cpus",    "0.5",
                "--pids-limit", "64",
                "-i",
                img, "python3", "-"
            });
            if (proc->waitForStarted(P::kProcessHeavyStartMs)) {
                proc->write(code.toUtf8());
                proc->closeWriteChannel();
            }
        } else {
            proc->start(P::findPython(), {"-c", code});
        }

        QTimer::singleShot(15000, proc, [proc, onDone]{
            if (proc->state() != QProcess::NotRunning) { proc->kill(); onDone("timeout sandbox"); }
        });
        return;
    }

    /* ── Leggi file ── */
    if (tool == "leggi_file" || tool == "read_file" || tool == "leggi") {
        const QString safe = PathGuard::checkRead(input);
        if (safe.isEmpty()) { onDone(PathGuard::deniedMsg(input)); return; }
        QFile f(safe);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            onDone(QString("errore: impossibile aprire '%1' — %2").arg(safe, f.errorString()));
            return;
        }
        const QString content = QString::fromUtf8(f.readAll()).left(4000);
        f.close();
        onDone(content.isEmpty() ? "(file vuoto)" : content);
        return;
    }

    /* ── Lista file in cartella ── */
    if (tool == "lista_file" || tool == "list_files" || tool == "ls" || tool == "lista") {
        const QString safe = PathGuard::checkDir(input);
        if (safe.isEmpty()) { onDone(PathGuard::deniedMsg(input)); return; }
        QDir dir(safe);
        if (!dir.exists()) {
            onDone(QString("errore: cartella '%1' non trovata").arg(safe));
            return;
        }
        const QStringList entries = dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot,
                                                   QDir::Name | QDir::DirsFirst);
        if (entries.isEmpty()) { onDone("(cartella vuota)"); return; }
        QStringList annotated;
        annotated.reserve(entries.size());
        for (const QString& e : entries) {
            annotated << (QFileInfo(safe + "/" + e).isDir() ? e + "/" : e);
        }
        onDone(annotated.join("\n").left(2000));
        return;
    }

    /* ── Scrivi file (PathGuard write + conferma utente) ── */
    if (tool == "scrivi_file" || tool == "write_file" || tool == "scrivi") {
        const int sep = input.indexOf("|||");
        if (sep < 0) {
            onDone("errore: formato scrivi_file deve essere \"percorso|||contenuto\"");
            return;
        }
        const QString rawPath    = input.left(sep).trimmed();
        const QString fileContent = input.mid(sep + 3);
        if (rawPath.isEmpty()) { onDone("errore: percorso file non specificato"); return; }

        const QString safe = PathGuard::checkWrite(rawPath);
        if (safe.isEmpty()) { onDone(PathGuard::deniedMsg(rawPath)); return; }

        /* Conferma utente */
        {
            auto* dlg = new QDialog(this);
            dlg->setWindowTitle(tr("\xf0\x9f\x93\x9d  Scrivi file?"));
            dlg->setMinimumSize(480, 320);
            auto* lay = new QVBoxLayout(dlg);
            auto* lbl = new QLabel(
                QString("L\xe2\x80\x99" "agente autonomo vuole scrivere il file:\n"
                        "<b>%1</b>\n\nContenuto (%2 caratteri):").arg(
                    safe.toHtmlEscaped(),
                    QString::number(fileContent.size())), dlg);
            lbl->setTextFormat(Qt::RichText);
            lbl->setWordWrap(true);
            lay->addWidget(lbl);
            auto* preview = new QTextEdit(dlg);
            preview->setReadOnly(true);
            preview->setPlainText(fileContent.left(800));
            preview->setFont(QFont("JetBrains Mono,Consolas,monospace", 9));
            preview->setStyleSheet("background:#1e1e2e;color:#cdd6f4;"
                                   "border:1px solid #45475a;padding:4px;");
            lay->addWidget(preview, 1);
            auto* btnBox = new QDialogButtonBox(dlg);
            btnBox->addButton("\xf0\x9f\x93\x9d  Scrivi", QDialogButtonBox::AcceptRole);
            btnBox->addButton("\xe2\x9c\x96  Annulla", QDialogButtonBox::RejectRole);
            connect(btnBox, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
            connect(btnBox, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
            lay->addWidget(btnBox);
            const bool ok = (dlg->exec() == QDialog::Accepted);
            dlg->deleteLater();
            if (!ok) { onDone("scrittura annullata dall\xe2\x80\x99utente"); return; }
        }

        QFile f(safe);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            onDone(QString("errore: impossibile scrivere '%1' — %2").arg(safe, f.errorString()));
            return;
        }
        f.write(fileContent.toUtf8());
        f.close();
        onDone(QString("file '%1' scritto (%2 byte)").arg(safe).arg(fileContent.toUtf8().size()));
        return;
    }

    /* ── search_rag — ricerca testuale nei file RAG locali ── */
    if (tool == "search_rag" || tool == "rag" || tool == "cerca_rag") {
        const QString query = input.toLower().trimmed();
        if (query.isEmpty()) { onDone("errore: query vuota"); return; }

        /* Le directory RAG sono fisse e consentite — verifica con PathGuard comunque */
        QStringList ragDirs;
        for (const QString& d : {QDir::homePath() + "/prismalux_rag_docs",
                                  P::ragDir()}) {
            if (!PathGuard::checkDir(d).isEmpty())
                ragDirs << d;
        }

        QStringList hits;
        static const QStringList kExts = {"txt","md","pdf","csv","docx","doc","json","py","cpp","h"};

        for (const QString& dirPath : ragDirs) {
            QDir d(dirPath);
            if (!d.exists()) continue;
            const auto entries = d.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
            for (const QFileInfo& fi : entries) {
                if (!kExts.contains(fi.suffix().toLower())) continue;
                QFile f(fi.absoluteFilePath());
                if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
                const QString content = QString::fromUtf8(f.readAll()).toLower();
                if (!content.contains(query)) continue;
                /* Estrae finestre di contesto intorno ai match (rilegge con case originale) */
                f.seek(0);
                const QString origContent = QString::fromUtf8(f.readAll());
                int pos = origContent.toLower().indexOf(query);
                int count = 0;
                while (pos >= 0 && count < 3) {
                    int from = qMax(0, pos - 120);
                    int len  = qMin(320, origContent.size() - from);
                    hits << QString("[%1] ...%2...")
                                .arg(fi.fileName(), origContent.mid(from, len).trimmed());
                    pos = origContent.toLower().indexOf(query, pos + 1);
                    ++count;
                }
            }
        }

        if (hits.isEmpty())
            onDone(QString("Nessun documento RAG contiene '%1'.").arg(input));
        else
            onDone(hits.join("\n\n").left(2000));
        return;
    }

    /* ── graph_memory — ricerca nella memoria a grafo SQLite ── */
    if (tool == "graph_memory" || tool == "memoria" || tool == "grafo_memoria") {
        const QString query = input.trimmed();
        GraphMemory gm(QDir::homePath() + "/.prismalux/graph_memory.db");
        if (!gm.open()) {
            onDone("GraphMemory non disponibile o DB non trovato.");
            return;
        }
        const auto nodes = gm.searchNodes(query.isEmpty() ? "*" : query, 10);
        if (nodes.isEmpty()) {
            onDone(QString("Nessun nodo in GraphMemory corrisponde a '%1'.").arg(query));
            return;
        }
        QStringList out;
        for (const auto& n : nodes) {
            QString entry = QString("[%1] %2").arg(n.type, n.label);
            if (!n.content.isEmpty())
                entry += ": " + n.content.left(200);
            out << entry;
        }
        onDone(out.join("\n").left(2000));
        return;
    }

    /* ── get_knowledge — legge la Knowledge Base personale ── */
    /* ── get_datetime — data/ora locale + UTC/GMT + timezone + Unix timestamp ── */
    if (tool == "get_datetime" || tool == "datetime" || tool == "ora" || tool == "data_ora") {
        const QDateTime local = QDateTime::currentDateTime();
        const QDateTime utc   = local.toUTC();
        const int offsetSec   = local.timeZone().offsetFromUtc(local);
        const int offsetH     = qAbs(offsetSec) / 3600;
        const int offsetM     = (qAbs(offsetSec) % 3600) / 60;
        const QLatin1Char sign = (offsetSec >= 0) ? QLatin1Char('+') : QLatin1Char('-');
        onDone(QString(
            "Data/ora locale: %1\n"
            "UTC/GMT:         %2\n"
            "Timezone:        %3 (UTC%4%5:%6)\n"
            "Giorno:          %7, settimana %8\n"
            "Unix timestamp:  %9")
            .arg(local.toString("yyyy-MM-dd HH:mm:ss"))
            .arg(utc.toString("yyyy-MM-dd HH:mm:ss"))
            .arg(local.timeZone().abbreviation(local))
            .arg(sign).arg(offsetH, 2, 10, QLatin1Char('0')).arg(offsetM, 2, 10, QLatin1Char('0'))
            .arg(local.toString("dddd"))
            .arg(local.date().weekNumber())
            .arg(local.toSecsSinceEpoch()));
        return;
    }

    /* ── date_calc — calcolo durate e conversione unità di tempo ── */
    if (tool == "date_calc" || tool == "calcola_date" || tool == "calcola_tempo"
        || tool == "converti_tempo" || tool == "durata") {

        const QDateTime now = QDateTime::currentDateTime();
        const QString   q   = input.simplified();   // 'input' = call["input"] definito sopra
        const QString   ql  = q.toLower();

        // Tabella unità → secondi
        struct UDef { QStringList n; double s; };
        static const QVector<UDef> kU = {
            {{"secondo","secondi","sec"},         1.0},
            {{"minuto","minuti","min"},           60.0},
            {{"ora","ore"},                       3600.0},
            {{"giorno","giorni"},                 86400.0},
            {{"settimana","settimane"},            7.0*86400.0},
            {{"mese","mesi"},                      30.4375*86400.0},
            {{"anno","anni"},                      365.2422*86400.0},
        };
        static const QMap<QString,int> kMesi = {
            {"gennaio",1},{"febbraio",2},{"marzo",3},{"aprile",4},
            {"maggio",5},{"giugno",6},{"luglio",7},{"agosto",8},
            {"settembre",9},{"ottobre",10},{"novembre",11},{"dicembre",12},
            {"gen",1},{"feb",2},{"mar",3},{"apr",4},
            {"mag",5},{"giu",6},{"lug",7},{"ago",8},
            {"set",9},{"ott",10},{"nov",11},{"dic",12},
        };
        // Periodi nominati → secondi (ricercati come sottostringa — mettere i più lunghi prima)
        static const QVector<QPair<QString,double>> kPer = {
            {"anno gregoriano", 365.2425*86400.0},
            {"anno tropicale",  365.2422*86400.0},
            {"anno tropico",    365.2422*86400.0},
            {"anno bisestile",  366.0   *86400.0},
            {"anno solare",     365.2422*86400.0},
            {"anno civile",     365.0   *86400.0},
            {"anno comune",     365.0   *86400.0},
            {"semestre",        365.2422*86400.0/2.0},
            {"trimestre",       365.2422*86400.0/4.0},
        };

        auto unitSec = [&](const QString& w) -> double {
            for (const auto& u : kU) if (u.n.contains(w)) return u.s;
            return 0.0;
        };
        // Formatta numero con unità; usa intero se quasi-esatto, 6 cifre per valori < 0.01
        auto fmtVal = [](double v, const QString& unit) -> QString {
            if (v >= 1.0 && qAbs(v - qRound(v)) < 0.005)
                return QString("%1 %2").arg(qRound64(v)).arg(unit);
            if (qAbs(v) < 0.01)
                return QString("%1 %2").arg(v, 0, 'f', 6).arg(unit);
            return QString("%1 %2").arg(v, 0, 'f', 2).arg(unit);
        };

        // ── Pattern 1: "quanti X mancano [prep] TARGET" ─────────────
        static const QRegularExpression reQ1(
            R"(quanti\s+(\w+)\s+mancano\s+(.+))",
            QRegularExpression::CaseInsensitiveOption);
        const auto m1 = reQ1.match(ql);
        if (m1.hasMatch()) {
            const QString unitWord = m1.captured(1);
            QString rest           = m1.captured(2).trimmed();
            // Rimuove preposizioni e locuzioni introduttive comuni
            static const QRegularExpression rePrep(
                R"(^(?:a|al|alla|allo|agli|alle|all'?|fino\s+a|sino\s+a)(?:\s*evento\s+di|\s*appuntamento\s+di|\s*scadenza\s+di)?\s*)",
                QRegularExpression::CaseInsensitiveOption);
            rest.remove(rePrep);

            const double uSec = unitSec(unitWord);
            if (uSec <= 0) { onDone("Unità non riconosciuta: " + unitWord); return; }

            QDate target;
            // "giorno N [di mese]"
            static const QRegularExpression reGiorno(
                R"(giorno\s+(\d{1,2})(?:\s+(?:di\s+)?(\w+))?)",
                QRegularExpression::CaseInsensitiveOption);
            const auto mg = reGiorno.match(rest);
            if (mg.hasMatch()) {
                const int day = mg.captured(1).toInt();
                int mon = now.date().month(), yr = now.date().year();
                if (!mg.captured(2).isEmpty()) {
                    const int m2 = kMesi.value(mg.captured(2).toLower(), 0);
                    if (m2 > 0) mon = m2;
                }
                target = QDate(yr, mon, day);
                if (!target.isValid() || target <= now.date())
                    target = mg.captured(2).isEmpty() ? target.addMonths(1) : QDate(yr+1, mon, day);
            }
            // Nome mese (con giorno opzionale davanti: "15 dicembre", "dicembre")
            if (!target.isValid()) {
                for (auto it = kMesi.cbegin(); it != kMesi.cend(); ++it) {
                    if (!rest.contains(it.key())) continue;
                    const int yr = now.date().year();
                    static const QRegularExpression reDM(R"((\d{1,2})\s+\w+)");
                    const auto dm = reDM.match(rest);
                    const int day = (dm.hasMatch() && dm.captured(1).toInt() >= 1) ? dm.captured(1).toInt() : 1;
                    target = QDate(yr, it.value(), day);
                    if (!target.isValid() || target <= now.date())
                        target = QDate(yr+1, it.value(), day);
                    break;
                }
            }
            if (!target.isValid()) { onDone("Non riconosco la data: " + rest); return; }

            const QDateTime targetDt(target, QTime(0, 0));
            const double diffSec = static_cast<double>(now.secsTo(targetDt));
            if (diffSec < 0) {
                onDone(QString("La data %1 e' gia' passata (%2 fa).")
                    .arg(target.toString("d MMMM yyyy"), fmtVal(-diffSec / uSec, unitWord)));
                return;
            }

            // Calcolo "calendario" per mesi/giorni/settimane, secondi per le altre
            QString result;
            if (unitWord == "mesi" || unitWord == "mese") {
                // monthsTo non esiste in Qt6: calcolo manuale con anno/mese
                const int months = (target.year() - now.date().year()) * 12
                                   + (target.month() - now.date().month());
                const int remD   = now.date().addMonths(months).daysTo(target);
                result = QString("%1 mesi").arg(months);
                if (remD > 0) result += QString(" e %1 giorni").arg(remD);
            } else if (unitWord == "giorni" || unitWord == "giorno") {
                result = fmtVal(static_cast<double>(now.date().daysTo(target)), "giorni");
            } else if (unitWord == "settimane" || unitWord == "settimana") {
                const qint64 days = now.date().daysTo(target);
                result = fmtVal(days / 7.0, "settimane") + QString(" (%1 giorni)").arg(days);
            } else {
                result = fmtVal(diffSec / uSec, unitWord);
            }
            onDone(QString("Da oggi (%1) al %2: **%3**")
                .arg(now.toString("d MMMM yyyy"), target.toString("d MMMM yyyy"), result));
            return;
        }

        // ── Pattern 2: "converti[mi] [N] X in Y" ────────────────────
        static const QRegularExpression reConv(
            R"(converti(?:mi)?\s+([\d.,]+)?\s*(\w+)\s+in\s+(\w+))",
            QRegularExpression::CaseInsensitiveOption);
        const auto m2 = reConv.match(ql);
        if (m2.hasMatch()) {
            const double n  = m2.captured(1).isEmpty() ? 1.0
                              : m2.captured(1).replace(',', '.').toDouble();
            const double s1 = unitSec(m2.captured(2));
            const double s2 = unitSec(m2.captured(3));
            if (s1 <= 0 || s2 <= 0) { onDone("Unita' non riconosciuta."); return; }
            onDone(QString("%1 = **%2**")
                .arg(fmtVal(n, m2.captured(2)), fmtVal(n * s1 / s2, m2.captured(3))));
            return;
        }

        // ── Pattern 3: "quanti X sono [un/una] [periodo/N Y]" ────────
        static const QRegularExpression reQ3(
            R"(quanti\s+(\w+)\s+(?:sono|ci\s+sono\s+in|equivalgono\s+a)\s+(?:un[ao]?\s+)?(.+?)[\?!\.\s]*$)",
            QRegularExpression::CaseInsensitiveOption);
        const auto m3 = reQ3.match(ql);
        if (m3.hasMatch()) {
            const QString unitDest  = m3.captured(1);
            const QString per       = m3.captured(2).trimmed();
            const double  uDestSec  = unitSec(unitDest);
            if (uDestSec <= 0) { onDone("Unita' non riconosciuta: " + unitDest); return; }

            double totalSec = 0.0;
            for (const auto& [name, sec] : kPer)
                if (per.contains(name)) { totalSec = sec; break; }
            if (totalSec == 0.0) {
                // "N unità" (es. "3 giorni")
                static const QRegularExpression reNU(R"(([\d.,]+)\s+(\w+))");
                const auto mn = reNU.match(per);
                if (mn.hasMatch()) {
                    const double n2 = mn.captured(1).replace(',', '.').toDouble();
                    const double s2 = unitSec(mn.captured(2));
                    if (s2 > 0) totalSec = n2 * s2;
                }
            }
            if (totalSec == 0.0) totalSec = unitSec(per);  // singola unità es. "giorno"

            if (totalSec <= 0.0) { onDone("Non riconosco il periodo: " + per); return; }
            // "3 giorni" contiene già la quantità — non aggiungere "1" davanti
            static const QRegularExpression reStartsNum(R"(^\d)");
            const QString prefix = reStartsNum.match(per).hasMatch() ? per : ("1 " + per);
            onDone(QString("%1 = **%2**").arg(prefix, fmtVal(totalSec / uDestSec, unitDest)));
            return;
        }

        // Fallback: aiuto contestuale
        onDone(
            "Esempi:\n"
            "TOOL_CALL: {\"tool\": \"date_calc\", \"input\": \"quanti mesi mancano a dicembre\"}\n"
            "TOOL_CALL: {\"tool\": \"date_calc\", \"input\": \"quanti minuti mancano all'evento di giorno 5\"}\n"
            "TOOL_CALL: {\"tool\": \"date_calc\", \"input\": \"convertimi 120 minuti in ore\"}\n"
            "TOOL_CALL: {\"tool\": \"date_calc\", \"input\": \"converti minuti in giorni\"}\n"
            "TOOL_CALL: {\"tool\": \"date_calc\", \"input\": \"quanti secondi sono un anno solare\"}\n"
            "TOOL_CALL: {\"tool\": \"date_calc\", \"input\": \"quanti minuti sono 3 giorni\"}");
        return;
    }

    if (tool == "get_knowledge" || tool == "knowledge" || tool == "conoscenza") {
        const QString kb = P::readUserKnowledge();
        if (kb.isEmpty())
            onDone("La Knowledge Base personale è vuota.");
        else
            onDone(kb.left(3000));
        return;
    }

    /* ── leggi_riassunto — legge riassunto_breve.md e/o riassunto_dettagliato.md ──
       input: "breve" | "dettagliato" | "" (entrambi)                              */
    if (tool == "leggi_riassunto" || tool == "read_summary" || tool == "riassunto") {
        const QString which = input.toLower().trimmed();
        const bool wantBrief    = which.isEmpty() || which == "breve"      || which == "tutti";
        const bool wantDetailed = which.isEmpty() || which == "dettagliato"|| which == "tutti";

        QString out;
        auto readFile = [](const QString& path, const QString& label) -> QString {
            QFile f(path);
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
                return QString("### %1\n(nessun riassunto salvato)\n").arg(label);
            const QString c = QString::fromUtf8(f.readAll()).trimmed();
            return c.isEmpty()
                ? QString("### %1\n(vuoto)\n").arg(label)
                : QString("### %1\n%2\n").arg(label, c);
        };
        if (wantBrief)    out += readFile(P::summaryBriefPath(),    "Riassunto Breve");
        if (wantDetailed) out += readFile(P::summaryDetailedPath(), "Riassunto Dettagliato");
        onDone(out.trimmed().left(3000));
        return;
    }

    /* ── scrivi_riassunto — salva riassunto generato dall'LLM ──────────────────
       input: "breve|||contenuto"  oppure  "dettagliato|||contenuto"             */
    if (tool == "scrivi_riassunto" || tool == "write_summary" || tool == "salva_riassunto") {
        const int sep = input.indexOf("|||");
        if (sep < 0) {
            onDone("errore: formato atteso \"breve|||testo\" o \"dettagliato|||testo\"");
            return;
        }
        const QString tipo     = input.left(sep).toLower().trimmed();
        const QString testo    = input.mid(sep + 3).trimmed();
        if (testo.isEmpty()) { onDone("errore: contenuto riassunto vuoto"); return; }

        QString path;
        if (tipo == "breve")
            path = P::summaryBriefPath();
        else if (tipo == "dettagliato")
            path = P::summaryDetailedPath();
        else {
            onDone("errore: tipo deve essere \"breve\" o \"dettagliato\"");
            return;
        }

        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            onDone(QString("errore: impossibile scrivere '%1'").arg(path));
            return;
        }
        f.write(testo.toUtf8());
        f.close();
        onDone(QString("Riassunto %1 salvato (%2 caratteri) in %3")
               .arg(tipo).arg(testo.size()).arg(path));
        return;
    }

    /* ── spawn_agent — sub-agente AI (max 4 per sessione) ── */
    if (tool == "spawn_agent" || tool == "sub_agent" || tool == "subagent") {
        constexpr int kMaxSpawn = 4;
        if (m_spawnedAgents >= kMaxSpawn) {
            onDone(QString("limite raggiunto: massimo %1 sub-agenti per sessione.").arg(kMaxSpawn));
            return;
        }

        /* Formato input: "Ruolo|||Compito" oppure solo "Compito" */
        const int sep  = input.indexOf("|||");
        const QString role = (sep > 0) ? input.left(sep).trimmed()
                                       : "Assistente specializzato";
        const QString task = (sep > 0) ? input.mid(sep + 3).trimmed() : input.trimmed();

        if (task.isEmpty()) { onDone("errore: task vuoto per spawn_agent"); return; }

        ++m_spawnedAgents;
        const int agentNum = m_spawnedAgents;

        /* Contesto RAG: inline + condiviso (se presenti) */
        QString ragCtx;
        if (m_ragInline && m_ragInline->hasContext())
            ragCtx += m_ragInline->ragContext();
        if (m_cfgDlg && m_cfgDlg->sharedRagWidget() && m_cfgDlg->sharedRagWidget()->hasContext())
            ragCtx += m_cfgDlg->sharedRagWidget()->ragContext();

        /* Sub-AiClient configurato come il principale */
        auto* sub = new AiClient(this);
        sub->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), m_ai->model());

        /* Tool list senza spawn_agent (evita ricorsione) */
        auto mkTool = [](const QString& n, const QString& d, const QString& pd) -> QJsonObject {
            QJsonObject props; QJsonObject val;
            val["type"] = QLatin1String("string"); val["description"] = pd;
            props["value"] = val;
            QJsonObject params; params["type"] = QLatin1String("object");
            params["properties"] = props;
            params["required"] = QJsonArray{ QLatin1String("value") };
            QJsonObject fn; fn["name"] = n; fn["description"] = d; fn["parameters"] = params;
            QJsonObject t; t["type"] = QLatin1String("function"); t["function"] = fn;
            return t;
        };
        /* Tool list senza spawn_agent (evita ricorsione) — include mcp_call */
        auto mkMcpTool = [](const QString& n, const QString& d,
                            const QJsonObject& params) -> QJsonObject {
            QJsonObject fn; fn["name"] = n; fn["description"] = d; fn["parameters"] = params;
            QJsonObject t; t["type"] = QLatin1String("function"); t["function"] = fn;
            return t;
        };
        QJsonObject mcpParams; mcpParams["type"] = QLatin1String("object");
        {
            QJsonObject props;
            auto sp = [](const QString& d) { QJsonObject v; v["type"] = QLatin1String("string"); v["description"] = d; return v; };
            props["plugin"]    = sp("Nome del plugin MCP (es. anki_mcp, ollama_mcp, knowledge_mcp)");
            props["tool_name"] = sp("Nome del tool da invocare nel plugin");
            props["args_json"] = sp("Argomenti come stringa JSON (es. {\"key\":\"val\"}); usa {} se vuoto");
            mcpParams["properties"] = props;
            mcpParams["required"]   = QJsonArray{ QLatin1String("plugin"), QLatin1String("tool_name") };
        }

        const QJsonArray subTools {
            mkTool("calc",        "Calcola un'espressione matematica.",                    "Espressione matematica"),
            mkTool("fetch_url",   "Scarica il contenuto di una pagina web (URL diretta).", "URL completa"),
            mkTool("ricerca",     "Cerca informazioni online via DuckDuckGo.",              "Query di testo"),
            mkTool("leggi_file",  "Legge il contenuto di un file locale.",                 "Percorso assoluto del file"),
            mkTool("lista_file",  "Elenca i file in una directory locale.",                "Percorso assoluto della cartella"),
            mkTool("python",      "Esegue codice Python in sandbox.",                      "Codice Python da eseguire"),
            mkTool("search_rag",  "Cerca nei documenti RAG indicizzati.",                  "Query di ricerca nei documenti RAG"),
            mkTool("graph_memory","Cerca nella memoria a grafo di Prismalux.",             "Query di ricerca nella memoria"),
            mkTool("get_knowledge","Legge la Knowledge Base personale dell'utente.",       "(lascia vuoto per leggere tutto)"),
            mkMcpTool("mcp_call", "Invoca un tool di un plugin MCP attivo in Prismalux.", mcpParams),
        };
        sub->setActiveTools(subTools);

        const QString sysSub = QString::fromUtf8(
            "Sei un agente specializzato con il seguente ruolo: ") + role +
            QString::fromUtf8(
            ".\nEsegui il compito assegnato in modo preciso e conciso. "
            "Rispondi SOLO con il risultato del tuo compito, senza preamboli. "
            "Rispondi in italiano.");

        /* Gestione tool call del sub-agente — stesso pattern di onNativeToolCall */
        connect(sub, &AiClient::toolCallRequired, this,
                [this, sub](const QString& tName, const QJsonObject& tArgs) {
                    QString tInput;
                    for (auto it = tArgs.constBegin(); it != tArgs.constEnd(); ++it) {
                        const QJsonValue v = it.value();
                        if (v.isString())  { tInput = v.toString(); break; }
                        if (v.isDouble())  { tInput = QString::number(v.toDouble()); break; }
                        if (v.isObject())  { tInput = QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact); break; }
                    }
                    if (tInput.isEmpty())
                        tInput = QJsonDocument(tArgs).toJson(QJsonDocument::Compact);

                    QJsonObject call;
                    call["tool"]  = tName;
                    call["input"] = tInput;
                    runToolCall(call, [sub, tName](const QString& result) {
                        sub->replyWithTool(tName, result);
                    });
                });

        connect(sub, &AiClient::finished, this, [this, sub, onDone, agentNum, role](const QString& full) {
            const QString out = QString("[Sub-agente %1 \xe2\x80\x94 %2]\n%3")
                                .arg(agentNum).arg(role).arg(full.trimmed().left(2000));
            onDone(out);
            sub->deleteLater();
        });
        connect(sub, &AiClient::error, this, [this, sub, onDone, agentNum](const QString& err) {
            onDone(QString("[Sub-agente %1 errore]: %2").arg(agentNum).arg(err));
            sub->deleteLater();
        });

        /* Inietta RAG nel task se disponibile */
        const QString taskFinal = ragCtx.isEmpty() ? task : (task + ragCtx);
        sub->chat(sysSub, taskFinal);
        return;
    }

    /* ── mcp_call — invoca un plugin MCP attivo via JSON-RPC 2.0 stdio ── */
    if (tool == "mcp_call") {
        const QJsonObject jin = QJsonDocument::fromJson(input.toUtf8()).object();
        const QString plugin   = jin.value("plugin").toString().trimmed();
        const QString toolName = jin.value("tool_name").toString().trimmed();
        const QString argsJson = jin.value("args_json").toString("{}");

        if (plugin.isEmpty() || toolName.isEmpty()) {
            onDone("errore mcp_call: plugin e tool_name sono obbligatori");
            return;
        }

        /* Valida nome plugin: solo caratteri alfanumerici/trattini/underscore — evita path traversal.
           static OK: mcp_call è sempre invocato dal main thread (Qt event loop). */
        static const QRegularExpression kPluginNameRe(QStringLiteral("^[a-zA-Z0-9_\\-]{1,64}$"));
        if (!kPluginNameRe.match(plugin).hasMatch()) {
            onDone(QString("errore mcp_call: nome plugin non valido '%1'").arg(plugin));
            return;
        }

        /* Valida plugin: MCPs/<plugin>/server.py deve esistere */
        const QString serverPath = P::root() + "/MCPs/" + plugin + "/server.py";
        if (!QFile::exists(serverPath)) {
            QDir mcpDir(P::root() + "/MCPs/");
            QStringList valid;
            for (const QString& d : mcpDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
                if (QFile::exists(P::root() + "/MCPs/" + d + "/server.py"))
                    valid << d;
            onDone(QString("errore mcp_call: plugin '%1' non trovato.\nPlugin disponibili: %2")
                   .arg(plugin, valid.join(", ")));
            return;
        }

        /* Python: venv del plugin → venv condiviso ~/.prismalux/venv → python3 di sistema */
        const QString venvPy     = P::root() + "/MCPs/" + plugin + "/venv/bin/python";
        const QString sharedVenv = QDir::homePath() + "/.prismalux/venv/bin/python";
        const QString pythonExe  = QFile::exists(venvPy)    ? venvPy
                                 : QFile::exists(sharedVenv) ? sharedVenv
                                 : P::findPython();

        /* Valida args_json: se malformato o non-oggetto, usa {} come fallback */
        QJsonParseError argsErr;
        const QJsonDocument argsDoc = QJsonDocument::fromJson(argsJson.toUtf8(), &argsErr);
        const QJsonObject argsObj = (argsErr.error == QJsonParseError::NoError && argsDoc.isObject())
                                    ? argsDoc.object() : QJsonObject{};

        /* Messaggi JSON-RPC 2.0: initialize (id=1) + tools/call (id=2) */
        const QByteArray initMsg = QJsonDocument(QJsonObject{
            {"jsonrpc","2.0"}, {"id",1}, {"method","initialize"},
            {"params", QJsonObject{
                {"protocolVersion","2024-11-05"},
                {"capabilities", QJsonObject{}},
                {"clientInfo", QJsonObject{{"name","prismalux"},{"version","3.0"}}}
            }}
        }).toJson(QJsonDocument::Compact) + "\n";

        const QByteArray callMsg = QJsonDocument(QJsonObject{
            {"jsonrpc","2.0"}, {"id",2}, {"method","tools/call"},
            {"params", QJsonObject{{"name",toolName},{"arguments",argsObj}}}
        }).toJson(QJsonDocument::Compact) + "\n";

        /* ── Keepalive: riusa processo vivo o crea nuovo.
           Non chiudiamo stdin: il server MCP resta in ascolto per chiamate successive.
           Leggiamo via readyRead (non finished) per permettere reuse. ── */
        QProcess* proc = nullptr;
        if (s_mcpAlive.contains(plugin)) {
            QProcess* alive = s_mcpAlive.value(plugin).data();
            if (alive && alive->state() == QProcess::Running) {
                /* Svuota buffer prima della nuova chiamata: readyRead non garantisce
                   di esaurire il buffer in un solo segnale — frammenti stale del
                   keepalive precedente contaminerebbero la risposta JSON successiva. */
                alive->readAllStandardOutput();
                proc = alive;
            } else {
                s_mcpAlive.remove(plugin);
            }
        }

        if (!proc) {
            proc = _launchMcpProcess(pythonExe, serverPath, plugin, initMsg);
            if (!proc) {
                onDone(QString("errore mcp_call: impossibile avviare '%1'").arg(plugin));
                return;
            }
        }
        proc->write(callMsg);  /* NON closeWriteChannel(): mantiene stdin aperto */

        auto retried = QSharedPointer<bool>::create(false);

        _attachMcpReader(proc, this, plugin, toolName,
            [this, proc, plugin, toolName, pythonExe, serverPath,
             initMsg, callMsg, onDone, retried](const QString& result) mutable {
                if (!result.isEmpty()) { onDone(result); return; }
                /* Nessuna risposta (timeout) → un solo retry con processo fresco */
                if (*retried) {
                    onDone(QString("[MCP %1::%2] nessuna risposta").arg(plugin, toolName));
                    return;
                }
                *retried = true;
                s_mcpAlive.remove(plugin);
                proc->kill();

                QProcess* proc2 = _launchMcpProcess(pythonExe, serverPath, plugin, initMsg);
                if (!proc2) {
                    onDone(QString("[MCP %1::%2] avvio fallito dopo retry").arg(plugin, toolName));
                    return;
                }
                proc2->write(callMsg);
                _attachMcpReader(proc2, this, plugin, toolName, onDone);
            });
        return;
    }

    onDone(QString("strumento non riconosciuto: %1").arg(tool));
}

/* ══════════════════════════════════════════════════════════════
   onNativeToolCall — handler per Ollama function calling nativo.
   Adatta il formato Ollama {name, arguments:{...}} al formato
   runToolCall {tool, input} e riprende la conversazione con replyWithTool().
   ══════════════════════════════════════════════════════════════ */
/* ── Mini-bolla HTML per un tool call ─────────────────────────────────────
   Stato: "running" (grigio) oppure "done" (verde) / "error" (rosso).
   elapsedSecs > 0 mostra il timer durante l'attesa (per MCP lenti).       */
static QString _toolBubble(const QString& name, const QString& input,
                            const QString& result, bool done,
                            bool error = false, int elapsedSecs = 0)
{
    const QString bg      = done ? (error ? "#3b1a1a" : "#1a2e1a") : "#1e2030";
    const QString border  = done ? (error ? "#ef4444" : "#22c55e") : "#6366f1";
    const QString icon    = done ? (error ? "\xe2\x9d\x8c" : "\xe2\x9c\x85") : "\xf0\x9f\x94\xa7";
    QString status;
    if (done)
        status = error ? "Errore" : "Completato";
    else if (elapsedSecs > 0)
        status = QString("In esecuzione\xe2\x80\xa6 %1s").arg(elapsedSecs);
    else
        status = "In esecuzione\xe2\x80\xa6";
    const QString nameEsc = name.toHtmlEscaped();
    const QString inEsc   = input.toHtmlEscaped().left(120);
    const QString resEsc  = result.toHtmlEscaped().left(300);

    QString html =
        "<div style='"
          "background:" + bg + ";"
          "border-left:3px solid " + border + ";"
          "border-radius:6px;"
          "padding:6px 10px;"
          "margin:4px 0 4px 20px;"
          "font-family:monospace;font-size:11px;"
        "'>"
        + icon + "&nbsp;<b>" + nameEsc + "</b>"
        "&nbsp;<span style='color:#94a3b8;font-size:10px;'>" + status + "</span>";

    if (!input.isEmpty())
        html += "<br><span style='color:#94a3b8;'>&#x2192; </span>"
                "<code style='color:#e2e8f0;'>" + inEsc + "</code>";

    if (done && !result.isEmpty())
        html += "<br><span style='color:#86efac;'>"
                + resEsc + "</span>";

    html += "</div>";
    return html;
}

/* ══════════════════════════════════════════════════════════════
   onNativeToolCall — accumula tool calls del turno corrente.
   Ollama può emettere più tool_calls in una risposta; tutti arrivano
   in un loop sincrono, poi l'event loop riprende. Con singleShot(0)
   ci assicuriamo di processarli tutti insieme in processToolBatch().
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::onNativeToolCall(const QString& name, const QJsonObject& args)
{
    m_incomingToolBatch.append({ name, args });
    if (!m_toolBatchScheduled) {
        m_toolBatchScheduled = true;
        QTimer::singleShot(0, this, &AgentiPage::processToolBatch);
    }
}

/* ══════════════════════════════════════════════════════════════
   processToolBatch — esegue tutti i tool del turno in parallelo
   (async) e chiama replyWithAllTools() quando tutti completano.
   Gestisce: spinner per MCP lenti, keepalive processo MCP.
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::processToolBatch()
{
    m_toolBatchScheduled = false;
    if (m_incomingToolBatch.isEmpty()) return;

    const int n = m_incomingToolBatch.size();
    m_toolBatchResults.clear();
    m_toolBatchResults.resize(n);
    m_toolBatchTotal = n;
    m_toolBatchDone  = 0;

    for (int i = 0; i < n; ++i) {
        const QString      name = m_incomingToolBatch[i].first;
        const QJsonObject& args = m_incomingToolBatch[i].second;

        /* ── Costruisce input canonico per runToolCall ── */
        QString input;
        if (name == QLatin1String("spawn_agent")) {
            input = args.value("role").toString().trimmed() + "|||"
                  + args.value("task").toString().trimmed();
        } else if (name == QLatin1String("mcp_call")) {
            QJsonObject jo;
            jo["plugin"]    = args.value("plugin").toString().trimmed();
            jo["tool_name"] = args.value("tool_name").toString().trimmed();
            const QString aj = args.value("args_json").toString().trimmed();
            jo["args_json"] = aj.isEmpty() ? "{}" : aj;
            input = QJsonDocument(jo).toJson(QJsonDocument::Compact);
        } else {
            for (auto it = args.constBegin(); it != args.constEnd(); ++it) {
                const QJsonValue v = it.value();
                if (v.isString()) { input = v.toString(); break; }
                if (v.isDouble()) { input = QString::number(v.toDouble()); break; }
                if (v.isObject()) { input = QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact); break; }
            }
            if (input.isEmpty())
                input = QJsonDocument(args).toJson(QJsonDocument::Compact);
        }

        /* ── Mini-bolla "in esecuzione" ── */
        m_log->moveCursor(QTextCursor::End);
        const int anchorPos = m_log->textCursor().position();
        m_log->insertHtml(_toolBubble(name, input, {}, false));
        m_log->append({});

        /* ── Spinner per MCP lenti (aggiorna bolla ogni secondo) ── */
        QTimer* spinnerTimer = nullptr;
        if (name == QLatin1String("mcp_call")) {
            spinnerTimer = new QTimer(this);
            spinnerTimer->setInterval(1000);
            auto elapsed = QSharedPointer<int>::create(0);
            connect(spinnerTimer, &QTimer::timeout, this,
                    [this, name, input, anchorPos, elapsed]() {
                ++(*elapsed);
                QTextCursor cur(m_log->document());
                cur.setPosition(anchorPos);
                cur.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
                cur.removeSelectedText();
                cur.insertHtml(_toolBubble(name, input, {}, false, false, *elapsed));
                m_log->append({});
            });
            spinnerTimer->start();
        }

        QJsonObject call;
        call["tool"]  = name;
        call["input"] = input;

        runToolCall(call, [this, i, n, name, input, anchorPos, spinnerTimer](const QString& result) {
            /* Ferma spinner se presente */
            if (spinnerTimer) { spinnerTimer->stop(); spinnerTimer->deleteLater(); }

            const bool isErr = result.startsWith("errore:") || result.startsWith("Nessun")
                             || result.startsWith("[MCP");

            /* Aggiorna bolla finale */
            QTextCursor cur(m_log->document());
            cur.setPosition(anchorPos);
            cur.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
            cur.removeSelectedText();
            cur.insertHtml(_toolBubble(name, input, result, true, isErr));
            m_log->append({});

            m_toolBatchResults[i] = { name, result };
            ++m_toolBatchDone;

            if (m_toolBatchDone == m_toolBatchTotal) {
                /* Tutti completati: invia risultati in un solo round-trip */
                if (m_toolBatchResults.size() == 1) {
                    m_ai->replyWithTool(m_toolBatchResults[0].first,
                                        m_toolBatchResults[0].second);
                } else {
                    m_ai->replyWithAllTools(m_toolBatchResults);
                }
                m_incomingToolBatch.clear();
            }
        });
    }
}
