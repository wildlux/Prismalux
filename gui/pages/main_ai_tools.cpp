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
#include "../widgets/qr_code_widget.h"
#include "main_simulator.h"
#include "pratico_calcs.h"
#include "../widgets/astro_calc.h"
#include "../widgets/formula_parser.h"
namespace P = PrismaluxPaths;
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QProcess>
#include <QTimer>
#include <QDateTime>
#include <QLocale>
#include <QMap>
#include <QHash>
#include <QSet>
#include <QVector>
#include <QTextCursor>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QTimeZone>
#include <QDialog>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QFont>
#include <random>
#include <cmath>

/* Dispatcher del tool "algoritmo" (D-33) e helper di validazione (IBAN
 * mod-97, Codice Fiscale D.M.1976) — definiti più sotto, dichiarati (non
 * più `static`) in main_ai_p.h da D-35: servono anche a
 * main_ai_tools_calls.cpp (onToolAlgoritmo/onToolValidaDocumento), oltre
 * che a _inject_finance qui in questo file. */

/* _icsEscapeText / _buildGoogleCalendarIntentUrl — dichiarate (linkage
 * esterna) in main_ai_p.h da D-35: servono anche a onToolEventoCalendario()
 * in main_ai_tools_calls.cpp, oltre che a crea_evento_calendario qui e ai
 * test diretti già esistenti (test_agenti_pipeline.cpp). Definite più sotto. */

/* ── Helper: tronca risultati tool lunghi con suffisso leggibile ── */
QString _truncateResult(const QString& s, int maxLen)
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

/* s_ddgCache (cache DuckDuckGo, TTL 5 min) spostata da file-static a
   static locale dentro AgentiPage::onToolRicerca() (main_ai_tools_calls.cpp,
   D-35) — era l'unico usо, nessun altro punto del file la referenzia. */

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

/* onToolMcpCall() resta qui (non nel nuovo file main_ai_tools_calls.cpp,
   D-35) perché usa _launchMcpProcess()/_attachMcpReader() qui sopra —
   quest'ultima è un template, va tenuta nella stessa unità di traduzione
   di dove viene istanziata senza spostarne la definizione in un header
   condiviso. */
void AgentiPage::onToolMcpCall(const QString& input, const std::function<void(QString)>& onDone)
{
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

    /* Data odierna iniettata direttamente nel system prompt: i modelli piccoli/
       locali non invocano sempre il tool get_datetime, quindi senza questa riga
       tendono a rispondere a memoria (data del training, spesso sbagliata). */
    const QString dateBlock = QString::fromUtf8("\n[DATA E ORA ATTUALI]: %1\n")
        .arg(QLocale(QLocale::Italian).toString(
            QDateTime::currentDateTime(), "dddd d MMMM yyyy, HH:mm"));

    return
        dateBlock +
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
            "TOOL_CALL: {\"tool\": \"crea_evento_calendario\", \"input\": "
            "\"{\\\"titolo\\\":\\\"...\\\",\\\"data\\\":\\\"YYYY-MM-DD\\\","
            "\\\"ora_inizio\\\":\\\"HH:MM\\\",\\\"ora_fine\\\":\\\"HH:MM\\\","
            "\\\"luogo\\\":\\\"...\\\",\\\"descrizione\\\":\\\"...\\\","
            "\\\"formato\\\":\\\"google|ics|entrambi\\\"}\"}\n"
            "crea_evento_calendario: genera un QR code per aggiungere un evento al "
            "calendario del telefono. PRIMA di chiamarlo, fai le domande necessarie "
            "una alla volta in italiano (titolo, data, orario, luogo — descrizione e "
            "formato QR sono opzionali, chiedi il formato solo se l'utente non lo "
            "specifica) — NON inventare mai titolo o data. Chiama il tool solo quando "
            "hai raccolto almeno titolo e data. Se il tool risponde con un messaggio "
            "che chiede altri dati, fai la domanda mancante e NON richiamare il tool "
            "finché l'utente non risponde.\n"
            "TOOL_CALL: {\"tool\": \"cambio_valuta\", \"input\": "
            "\"{\\\"importo\\\":100,\\\"da\\\":\\\"EUR\\\",\\\"a\\\":\\\"USD\\\"}\"}\n"
            "cambio_valuta: tasso di cambio reale aggiornato (fonte BCE) — usalo SEMPRE per "
            "conversioni tra valute, non calcolare mai un tasso a memoria (cambia ogni giorno).\n"
            "NOTA: IBAN, Codice Fiscale, sconti/IVA/percentuali, UUID, password e hash "
            "vengono riconosciuti e calcolati AUTOMATICAMENTE se il testo dell'utente li "
            "contiene — non serve nessun TOOL_CALL per questi.\n"
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
    if (tool == "calc" || tool == "calcolatrice" || tool == "math") { onToolCalc(input, onDone); return; }
    if (tool == "algoritmo" || tool == "algorithm") { onToolAlgoritmo(input, onDone); return; }
    if (tool == "codice_fiscale" || tool == "calcola_codice_fiscale") { onToolCodiceFiscale(input, onDone); return; }
    if (tool == "finanza_calcola") { onToolFinanzaCalcola(input, onDone); return; }
    if (tool == "valida_documento") { onToolValidaDocumento(input, onDone); return; }
    if (tool == "carta_astrale") { onToolCartaAstrale(input, onDone); return; }
    if (tool == "converti" || tool == "convert") { onToolConverti(input, onDone); return; }
    if (tool == "disegna_grafico" || tool == "grafico") { onToolDisegnaGrafico(input, onDone); return; }
    if (tool == "ricerca" || tool == "search" || tool == "web") { onToolRicerca(input, onDone); return; }
    if (tool == "cambio_valuta" || tool == "currency" || tool == "converti_valuta") { onToolCambioValuta(input, onDone); return; }
    if (tool == "fetch_url" || tool == "scarica_pagina" || tool == "fetch" || tool == "url") { onToolFetchUrl(input, onDone); return; }
    if (tool == "python" || tool == "py") { onToolPython(input, onDone); return; }
    if (tool == "leggi_file" || tool == "read_file" || tool == "leggi") { onToolLeggiFile(input, onDone); return; }
    if (tool == "lista_file" || tool == "list_files" || tool == "ls" || tool == "lista") { onToolListaFile(input, onDone); return; }
    if (tool == "scrivi_file" || tool == "write_file" || tool == "scrivi") { onToolScriviFile(input, onDone); return; }
    if (tool == "search_rag" || tool == "rag" || tool == "cerca_rag") { onToolSearchRag(input, onDone); return; }
    if (tool == "graph_memory" || tool == "memoria" || tool == "grafo_memoria") { onToolGraphMemory(input, onDone); return; }
    if (tool == "get_datetime" || tool == "datetime" || tool == "ora" || tool == "data_ora") { onToolGetDatetime(input, onDone); return; }
    if (tool == "date_calc" || tool == "calcola_date" || tool == "calcola_tempo" || tool == "converti_tempo" || tool == "durata") { onToolDateCalc(input, onDone); return; }
    if (tool == "crea_evento_calendario" || tool == "evento_calendario" || tool == "qr_evento") { onToolEventoCalendario(input, onDone); return; }
    if (tool == "get_knowledge" || tool == "knowledge" || tool == "conoscenza") { onToolGetKnowledge(input, onDone); return; }
    if (tool == "leggi_riassunto" || tool == "read_summary" || tool == "riassunto") { onToolLeggiRiassunto(input, onDone); return; }
    if (tool == "scrivi_riassunto" || tool == "write_summary" || tool == "salva_riassunto") { onToolScriviRiassunto(input, onDone); return; }
    if (tool == "spawn_agent" || tool == "sub_agent" || tool == "subagent") { onToolSpawnAgent(input, onDone); return; }
    if (tool == "mcp_call") { onToolMcpCall(input, onDone); return; }

    onDone(QString("strumento non riconosciuto: %1").arg(tool));
}

/* ══════════════════════════════════════════════════════════════
   _inject_date_calc — intercetta "quanti X mancano a/al DATA" e calcola
   localmente con QDate reale, invece di lasciare che l'LLM ci provi da
   solo: i modelli piccoli/locali sbagliano quasi sempre l'aritmetica di
   calendario (osservato: "quanti mesi mancano a dicembre" → risposta
   incoerente "31-28=3 giorni... 3 mesi"). Duplica la logica del Pattern 1
   del tool date_calc qui sopra (stesso regex) invece di chiamare onDone():
   ritorna una stringa, per essere usata nella catena _inject_* prima
   dell'invio all'LLM. Stesso stile di _inject_science/_inject_random.
   ══════════════════════════════════════════════════════════════ */
QString _inject_date_calc(const QString& task)
{
    const QString ql = task.toLower().trimmed();
    if (ql.length() > 300) return task;
    const QDate today = QDate::currentDate();

    /* ── "quanti X mancano a/al DATA" ── */
    {
        static const QRegularExpression reQ1(
            R"(quanti\s+(\w+)\s+mancano\s+(.+))",
            QRegularExpression::CaseInsensitiveOption);
        const auto m1 = reQ1.match(ql);
        if (m1.hasMatch()) {
            const QString unitWord = m1.captured(1);
            QString rest = m1.captured(2).trimmed();
            static const QRegularExpression rePrep(
                R"(^(?:a|al|alla|allo|agli|alle|all'?|fino\s+a|sino\s+a)(?:\s*evento\s+di|\s*appuntamento\s+di|\s*scadenza\s+di)?\s*)",
                QRegularExpression::CaseInsensitiveOption);
            rest.remove(rePrep);

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
            auto unitSec = [&](const QString& w) -> double {
                for (const auto& u : kU) if (u.n.contains(w)) return u.s;
                return 0.0;
            };
            const double uSec = unitSec(unitWord);
            static const QMap<QString,int> kMesi = {
                {"gennaio",1},{"febbraio",2},{"marzo",3},{"aprile",4},
                {"maggio",5},{"giugno",6},{"luglio",7},{"agosto",8},
                {"settembre",9},{"ottobre",10},{"novembre",11},{"dicembre",12},
                {"gen",1},{"feb",2},{"mar",3},{"apr",4},
                {"mag",5},{"giu",6},{"lug",7},{"ago",8},
                {"set",9},{"ott",10},{"nov",11},{"dic",12},
            };
            QDate target;
            if (uSec > 0) {
                static const QRegularExpression reGiorno(
                    R"(giorno\s+(\d{1,2})(?:\s+(?:di\s+)?(\w+))?)",
                    QRegularExpression::CaseInsensitiveOption);
                const auto mg = reGiorno.match(rest);
                if (mg.hasMatch()) {
                    const int day = mg.captured(1).toInt();
                    int mon = today.month(), yr = today.year();
                    if (!mg.captured(2).isEmpty()) {
                        const int m2 = kMesi.value(mg.captured(2).toLower(), 0);
                        if (m2 > 0) mon = m2;
                    }
                    target = QDate(yr, mon, day);
                    if (!target.isValid() || target <= today)
                        target = mg.captured(2).isEmpty() ? target.addMonths(1) : QDate(yr + 1, mon, day);
                }
                if (!target.isValid()) {
                    for (auto it = kMesi.cbegin(); it != kMesi.cend(); ++it) {
                        if (!rest.contains(it.key())) continue;
                        const int yr = today.year();
                        static const QRegularExpression reDM(R"((\d{1,2})\s+\w+)");
                        const auto dm = reDM.match(rest);
                        const int day = (dm.hasMatch() && dm.captured(1).toInt() >= 1) ? dm.captured(1).toInt() : 1;
                        target = QDate(yr, it.value(), day);
                        if (!target.isValid() || target <= today)
                            target = QDate(yr + 1, it.value(), day);
                        break;
                    }
                }
            }
            const qint64 diffDays = target.isValid() ? today.daysTo(target) : -1;
            if (target.isValid() && diffDays >= 0) {
                QString result;
                if (unitWord == "mesi" || unitWord == "mese") {
                    /* Se il conteggio grezzo supera il target di qualche giorno
                       (es. oggi 3 luglio → target 1 dicembre: "oggi + 5 mesi" =
                       3 dicembre, DOPO il target), va scalato di 1 mese e
                       ricalcolato il resto in giorni. */
                    int months = (target.year() - today.year()) * 12 + (target.month() - today.month());
                    int remD   = static_cast<int>(today.addMonths(months).daysTo(target));
                    if (remD < 0) {
                        --months;
                        remD = static_cast<int>(today.addMonths(months).daysTo(target));
                    }
                    result = QString("%1 mesi").arg(months);
                    if (remD > 0) result += QString(" e %1 giorni").arg(remD);
                } else if (unitWord == "giorni" || unitWord == "giorno") {
                    result = QString("%1 giorni").arg(diffDays);
                } else if (unitWord == "settimane" || unitWord == "settimana") {
                    result = QString("%1 settimane (%2 giorni)").arg(diffDays / 7.0, 0, 'f', 1).arg(diffDays);
                } else {
                    result = QString("%1 %2").arg(diffDays * 86400.0 / uSec, 0, 'f', 2).arg(unitWord);
                }
                return QString("[Calcolo locale: da oggi (%1) al %2 mancano %3]\n\n")
                    .arg(QLocale(QLocale::Italian).toString(today, "d MMMM yyyy"),
                         QLocale(QLocale::Italian).toString(target, "d MMMM yyyy"), result) + task;
            }
        }
    }

    /* ── Età esatta: "quanti anni ho se sono nato il DD/MM/YYYY" ── */
    {
        static const QRegularExpression re(
            R"((?:quanti\s+anni\s+(?:ho|ha|avr[àa])|che\s+et[àa]\s+(?:ho|ha)|et[àa]\s+di\s+chi\s+[èe]\s+nat[oa]).{0,15}nat[oa].{0,10}(\d{1,2})[\/\-\.](\d{1,2})[\/\-\.](\d{2,4}))",
            QRegularExpression::CaseInsensitiveOption);
        const auto m = re.match(ql);
        if (m.hasMatch()) {
            int yy = m.captured(3).toInt();
            if (yy < 100) yy += (yy > (today.year() % 100) ? 1900 : 2000);
            const QDate nascita(yy, m.captured(2).toInt(), m.captured(1).toInt());
            if (nascita.isValid() && nascita <= today) {
                int anni = today.year() - nascita.year();
                QDate compleanno(today.year(), nascita.month(), nascita.day());
                if (!compleanno.isValid() || today < compleanno) { --anni; compleanno = compleanno.addYears(-1); }
                const int giorniAlProssimo = compleanno.isValid()
                    ? static_cast<int>(today.daysTo(compleanno.addYears(1))) : -1;
                QString extra = giorniAlProssimo >= 0
                    ? QString(" (mancano %1 giorni al prossimo compleanno)").arg(giorniAlProssimo) : QString();
                return QString("[Calcolo locale: nato/a il %1, oggi ha %2 anni%3]\n\n")
                    .arg(QLocale(QLocale::Italian).toString(nascita, "d MMMM yyyy"))
                    .arg(anni).arg(extra) + task;
            }
        }
    }

    /* ── Giorno della settimana: "che giorno era/sarà il DD/MM/YYYY" ── */
    {
        static const QRegularExpression re(
            R"(che\s+giorno\s+(?:della\s+settimana\s+)?(?:era|cade|cadeva|sar[àa]|cadr[àa]|[èe])\s+(?:il\s+)?(\d{1,2})[\/\-\.](\d{1,2})[\/\-\.](\d{2,4}))",
            QRegularExpression::CaseInsensitiveOption);
        const auto m = re.match(ql);
        if (m.hasMatch()) {
            int yy = m.captured(3).toInt();
            if (yy < 100) yy += (yy > (today.year() % 100) ? 1900 : 2000);
            const QDate d(yy, m.captured(2).toInt(), m.captured(1).toInt());
            if (d.isValid()) {
                return QString("[Calcolo locale: il %1 \xc3\xa8/era %2]\n\n")
                    .arg(QLocale(QLocale::Italian).toString(d, "d MMMM yyyy"),
                         QLocale(QLocale::Italian).toString(d, "dddd")) + task;
            }
        }
    }

    /* ── Fuso orario: "che ora è a CITTÀ" ── */
    {
        /* NB: delimitatore raw string "rx" obbligatorio — con R"(...)" la
           sequenza finale `\s))` conteneva `)"` e troncava il pattern,
           rendendo la regex INVALIDA (guardia mai attiva). Il range degli
           accentati usa \x{..} Unicode (prima erano byte UTF-8 spezzati). */
        static const QRegularExpression re(
            R"rx(che\s+(?:ora|ore)\s+(?:[èe]|sono|fa)\s+(?:a|in|adesso\s+a)\s+([a-z\x{00e0}-\x{00ff}\s]{2,20}?)(?:\?|$|\s+se\s))rx",
            QRegularExpression::CaseInsensitiveOption);
        const auto m = re.match(ql);
        if (m.hasMatch()) {
            static const QMap<QString,QString> kTz = {
                {"new york",       "America/New_York"},   {"los angeles", "America/Los_Angeles"},
                {"san francisco",  "America/Los_Angeles"},{"chicago",     "America/Chicago"},
                {"londra",         "Europe/London"},      {"london",      "Europe/London"},
                {"parigi",         "Europe/Paris"},        {"berlino",     "Europe/Berlin"},
                {"madrid",         "Europe/Madrid"},       {"mosca",       "Europe/Moscow"},
                {"tokyo",          "Asia/Tokyo"},          {"pechino",     "Asia/Shanghai"},
                {"beijing",        "Asia/Shanghai"},       {"shanghai",    "Asia/Shanghai"},
                {"dubai",          "Asia/Dubai"},          {"sydney",      "Australia/Sydney"},
                {"hong kong",      "Asia/Hong_Kong"},      {"singapore",   "Asia/Singapore"},
                {"delhi",          "Asia/Kolkata"},        {"mumbai",      "Asia/Kolkata"},
                {"san paolo",      "America/Sao_Paulo"},   {"sao paulo",   "America/Sao_Paulo"},
                {"citt\xc3\xa0 del messico", "America/Mexico_City"},
            };
            const QString city = m.captured(1).trimmed();
            if (kTz.contains(city)) {
                const QTimeZone tz(kTz.value(city).toUtf8());
                if (tz.isValid()) {
                    const QDateTime there = QDateTime::currentDateTime().toTimeZone(tz);
                    return QString("[Calcolo locale: a %1 sono le %2 (%3, oggi qui sono le %4)]\n\n")
                        .arg(city, there.toString("HH:mm"), QString::fromUtf8(tz.id()),
                             QDateTime::currentDateTime().toString("HH:mm")) + task;
                }
            }
        }
    }

    /* ── Giorni lavorativi tra due date ──
     * Gap "[^\d]" (non cifra) invece di "." generico: con "." generico,
     * essendo greedy e "\d{1,2}" capace di accettare anche 1 sola cifra,
     * il gap può "rubare" la prima cifra della seconda data (es. "15" letto
     * come "5") senza che il motore regex abbia motivo di fare backtracking
     * — la corrispondenza risulta comunque valida, solo con un giorno
     * sbagliato. Bug reale trovato verificando T-D17 (TODO.md) con l'esempio
     * "giorni lavorativi tra 03/07/2026 e 15/08/2026" letto come 5 agosto. */
    {
        static const QRegularExpression re(
            R"(giorni\s+lavorativi[^\d]{0,15}(\d{1,2})[\/\-\.](\d{1,2})[\/\-\.](\d{2,4})[^\d]{0,10}(\d{1,2})[\/\-\.](\d{1,2})[\/\-\.](\d{2,4}))",
            QRegularExpression::CaseInsensitiveOption);
        const auto m = re.match(ql);
        if (m.hasMatch()) {
            auto mkYear = [&](int yy){ return yy < 100 ? yy + (yy > (today.year()%100) ? 1900 : 2000) : yy; };
            QDate d1(mkYear(m.captured(3).toInt()), m.captured(2).toInt(), m.captured(1).toInt());
            QDate d2(mkYear(m.captured(6).toInt()), m.captured(5).toInt(), m.captured(4).toInt());
            if (d1.isValid() && d2.isValid()) {
                if (d1 > d2) std::swap(d1, d2);
                int lavorativi = 0;
                for (QDate d = d1; d <= d2; d = d.addDays(1))
                    if (d.dayOfWeek() < 6) ++lavorativi;   /* 1=lun..5=ven, 6=sab 7=dom */
                return QString("[Calcolo locale: dal %1 al %2 ci sono %3 giorni lavorativi "
                                "(esclusi sabati/domeniche, non conta le festivit\xc3\xa0)]\n\n")
                    .arg(QLocale(QLocale::Italian).toString(d1, "d MMMM yyyy"),
                         QLocale(QLocale::Italian).toString(d2, "d MMMM yyyy"))
                    .arg(lavorativi) + task;
            }
        }
    }

    /* ── Data di Pasqua (D-16): "quando è pasqua nel 2027" / "data di
     * pasqua 2026" / "quando è pasqua" (anno corrente se omesso).
     * Algoritmo anonimo gregoriano (Meeus/Jones/Butcher, equivalente
     * matematicamente al metodo di Gauss ma senza le sue eccezioni
     * storiche) — verificato con Python standalone contro 7 date note
     * pubblicamente (2024=31/3, 2025=20/4, 2026=5/4, 2027=28/3, 2028=16/4,
     * 2000=23/4, 1994=3/4), tutte corrette prima di scrivere il C++. */
    if (ql.contains("pasqua")) {
        static const QRegularExpression reYear(R"(\b(\d{4})\b)");
        const auto my = reYear.match(ql);
        const int year = my.hasMatch() ? my.captured(1).toInt() : today.year();
        if (year >= 1583 && year <= 4099) {
            const int a = year % 19;
            const int b = year / 100;
            const int c = year % 100;
            const int d = b / 4;
            const int e = b % 4;
            const int f = (b + 8) / 25;
            const int g = (b - f + 1) / 3;
            const int h = (19 * a + b - d - g + 15) % 30;
            const int i2 = c / 4;
            const int k = c % 4;
            const int l = (32 + 2 * e + 2 * i2 - h - k) % 7;
            const int m2 = (a + 11 * h + 22 * l) / 451;
            const int month = (h + l - 7 * m2 + 114) / 31;
            const int day = (h + l - 7 * m2 + 114) % 31 + 1;
            const QDate pasqua(year, month, day);
            if (pasqua.isValid()) {
                return QString("[Calcolo locale: la Pasqua del %1 cade %2 (algoritmo di Gauss)]\n\n")
                    .arg(year).arg(QLocale(QLocale::Italian).toString(pasqua, "dddd d MMMM yyyy")) + task;
            }
        }
    }

    /* ── Fase lunare di una data (D-16): "fase lunare del 15/03/2026" /
     * "che fase lunare c'è oggi". Approssimazione sinodica (periodo medio
     * 29.53058867 giorni da un novilunio di riferimento noto, 6/1/2000
     * 18:14 UTC) — precisione ±1 giorno, dichiarata esplicitamente nella
     * risposta perché non è un calcolo esatto come le altre guardie. */
    if (ql.contains("fase lunare") || (ql.contains("luna") && (ql.contains("fase") || ql.contains("che luna")))) {
        QDate d = today;
        static const QRegularExpression reD(R"((\d{1,2})[\/\-\.](\d{1,2})[\/\-\.](\d{2,4}))");
        const auto md = reD.match(ql);
        if (md.hasMatch()) {
            int yy = md.captured(3).toInt();
            if (yy < 100) yy += (yy > (today.year() % 100) ? 1900 : 2000);
            const QDate parsed(yy, md.captured(2).toInt(), md.captured(1).toInt());
            if (parsed.isValid()) d = parsed;
        }
        const QDateTime epoch(QDate(2000, 1, 6), QTime(18, 14), QTimeZone::utc());
        const QDateTime target(d, QTime(12, 0), QTimeZone::utc());
        const double giorni = epoch.msecsTo(target) / 86400000.0;
        const double sinodico = 29.53058867;
        double fase = std::fmod(giorni, sinodico);
        if (fase < 0) fase += sinodico;
        const double frazione = fase / sinodico;
        static const QStringList kNomi = {
            "Luna nuova", "Falce crescente", "Primo quarto", "Gibbosa crescente",
            "Luna piena", "Gibbosa calante", "Ultimo quarto", "Falce calante"
        };
        const int idx = int(std::round(frazione * 8.0)) % 8;
        const double illuminazione = (1.0 - std::cos(2.0 * M_PI * frazione)) / 2.0 * 100.0;
        return QString("[Calcolo locale: il %1 la fase lunare (approssimata) \xc3\xa8 %2, "
                        "illuminazione ~%3%]\n\n")
            .arg(QLocale(QLocale::Italian).toString(d, "d MMMM yyyy"), kNomi.at(idx))
            .arg(QString::number(illuminazione, 'f', 0)) + task;
    }

    /* ── Scadenza "tra X anni/mesi/giorni/settimane da oggi" (D-16) —
     * direzione opposta di "quanti X mancano a DATA" sopra: qui si dà una
     * durata e si vuole la data risultante, non il contrario. */
    {
        static const QRegularExpression re(
            R"(tra\s+(\d+)\s+(ann[oi]|mes[ei]|settiman[ae]|giorn[oi])\s+da\s+oggi)",
            QRegularExpression::CaseInsensitiveOption);
        const auto m = re.match(ql);
        if (m.hasMatch()) {
            const int n = m.captured(1).toInt();
            const QString unit = m.captured(2);
            QDate target;
            QString unitLabel;
            if (unit.startsWith("ann"))      { target = today.addYears(n);  unitLabel = "anni"; }
            else if (unit.startsWith("mes")) { target = today.addMonths(n); unitLabel = "mesi"; }
            else if (unit.startsWith("settiman")) { target = today.addDays(n * 7); unitLabel = "settimane"; }
            else                              { target = today.addDays(n);  unitLabel = "giorni"; }
            if (target.isValid()) {
                return QString("[Calcolo locale: tra %1 %2 da oggi (%3) sar\xc3\xa0 %4]\n\n")
                    .arg(n).arg(unitLabel, QLocale(QLocale::Italian).toString(today, "d MMMM yyyy"),
                         QLocale(QLocale::Italian).toString(target, "dddd d MMMM yyyy")) + task;
            }
        }
    }

    return task;
}

/* ══════════════════════════════════════════════════════════════
   _icsEscapeText — escaping di un campo testo iCalendar (RFC 5545
   §3.3.11 TEXT value type). Backslash PRIMA di tutto, altrimenti
   raddoppierebbe gli escape appena inseriti per ; e ,. Senza questo,
   un titolo/luogo/descrizione con una virgola o un punto e virgola
   rompe il parsing per un lettore calendario rigoroso (campo
   troncato al separatore, o l'intero VCALENDAR scartato).
   ══════════════════════════════════════════════════════════════ */
QString _icsEscapeText(QString s)
{
    s.replace('\\', "\\\\");
    s.replace(';',  "\\;");
    s.replace(',',  "\\,");
    s.replace("\r\n", "\\n");
    s.replace('\n', "\\n");
    return s;
}

/* ══════════════════════════════════════════════════════════════
   _buildGoogleCalendarIntentUrl — variante Android del link Google
   Calendar: un https:// semplice quasi sempre apre il BROWSER anche
   con l'app installata (segnalato da un utente reale). Lo schema
   intent:// dice esplicitamente ad Android di aprire il pacchetto
   com.google.android.calendar, con S.browser_fallback_url = lo
   stesso link https come riserva se l'app non c'è. iOS non capisce
   intent:// — per questo NON sostituisce il link universale, viene
   generato come QR separato (vedi crea_evento_calendario).
   ══════════════════════════════════════════════════════════════ */
QString _buildGoogleCalendarIntentUrl(const QUrlQuery& query, const QUrl& httpsUrl)
{
    return "intent://calendar.google.com/calendar/render?"
        + query.query(QUrl::FullyEncoded)
        + "#Intent;scheme=https;package=com.google.android.calendar;"
          "S.browser_fallback_url=" + QString::fromUtf8(QUrl::toPercentEncoding(httpsUrl.toString()))
        + ";end";
}

/* ══════════════════════════════════════════════════════════════
   _parseEventoRequest — parsing locale (zero LLM) di una richiesta
   di evento calendario: "creami un evento festa in maschera il
   31/10/2026 dalle 21 alle 23 presso casa di Anna".
   Ritorna {} se il testo non è una richiesta di evento; altrimenti
   un oggetto pronto per il tool crea_evento_calendario. La chiave
   "data" è ASSENTE se nel testo non c'è una data riconoscibile: il
   chiamante risponde chiedendo la data (sempre in locale).
   ══════════════════════════════════════════════════════════════ */
QJsonObject _parseEventoRequest(const QString& task)
{
    const QString lo = task.toLower().trimmed();
    if (lo.length() > 220) return {};

    /* Intento: verbo di creazione + evento/appuntamento/promemoria */
    static const QRegularExpression reIntent(
        R"rx(\b(?:crea(?:mi)?|genera(?:mi)?|fammi|prepara(?:mi)?|salva(?:mi)?|aggiungi(?:mi)?)\s+(?:un[o']?\s+|un'\s*|il\s+|l')?(?:evento|appuntamento|promemoria)\b)rx");
    const auto mi = reIntent.match(lo);
    if (!mi.hasMatch()) return {};

    const QDate today = QDate::currentDate();
    QJsonObject ev;
    ev["formato"] = "entrambi";

    /* ── Data ── */
    QDate data;
    int dateAt = -1;                 /* posizione del marcatore data (per il titolo) */
    {   /* dd/mm[/yyyy] */
        static const QRegularExpression reD(
            R"((\d{1,2})[\/\-\.](\d{1,2})(?:[\/\-\.](\d{2,4}))?)");
        const auto m = reD.match(lo);
        if (m.hasMatch()) {
            int y = today.year(); bool explicitYear = false;
            if (!m.captured(3).isEmpty()) {
                y = m.captured(3).toInt();
                if (y < 100) y += 2000;
                explicitYear = true;
            }
            QDate d(y, m.captured(2).toInt(), m.captured(1).toInt());
            if (d.isValid()) {
                if (!explicitYear && d < today) d = d.addYears(1);
                data = d; dateAt = m.capturedStart();
            }
        }
    }
    if (!data.isValid()) {   /* "12 agosto [2026]" */
        static const QRegularExpression reM(
            R"((\d{1,2})\s+(gennaio|febbraio|marzo|aprile|maggio|giugno|luglio|agosto|settembre|ottobre|novembre|dicembre)(?:\s+(\d{4}))?)");
        const auto m = reM.match(lo);
        if (m.hasMatch()) {
            static const QStringList kMesi = {
                "gennaio","febbraio","marzo","aprile","maggio","giugno",
                "luglio","agosto","settembre","ottobre","novembre","dicembre" };
            int y = today.year(); bool explicitYear = false;
            if (!m.captured(3).isEmpty()) { y = m.captured(3).toInt(); explicitYear = true; }
            QDate d(y, kMesi.indexOf(m.captured(2)) + 1, m.captured(1).toInt());
            if (d.isValid()) {
                if (!explicitYear && d < today) d = d.addYears(1);
                data = d; dateAt = m.capturedStart();
            }
        }
    }
    if (!data.isValid()) {   /* domani / dopodomani */
        int p = lo.indexOf("dopodomani");
        if (p >= 0) { data = today.addDays(2); dateAt = p; }
        else if ((p = lo.indexOf("domani")) >= 0) { data = today.addDays(1); dateAt = p; }
    }
    if (data.isValid()) ev["data"] = data.toString("yyyy-MM-dd");

    /* ── Orari: "dalle 21[:30]" + "alle 23[:15]" (o solo "alle 21") ── */
    static const QRegularExpression reDalle(
        R"(\bdalle(?:\s+ore)?\s+(\d{1,2})(?:[:.](\d{2}))?)");
    static const QRegularExpression reAlle(
        R"(\balle(?:\s+ore)?\s+(\d{1,2})(?:[:.](\d{2}))?)");
    auto oraDa = [](const QRegularExpressionMatch& m) -> QTime {
        const int h = m.captured(1).toInt();
        const int mm = m.captured(2).isEmpty() ? 0 : m.captured(2).toInt();
        return (h <= 23 && mm <= 59) ? QTime(h, mm) : QTime();
    };
    const auto mDa = reDalle.match(lo);
    const auto mA  = reAlle.match(lo);
    int oraAt = -1;
    if (mDa.hasMatch()) {
        const QTime t = oraDa(mDa);
        if (t.isValid()) { ev["ora_inizio"] = t.toString("HH:mm"); oraAt = mDa.capturedStart(); }
        if (mA.hasMatch()) {
            const QTime tf = oraDa(mA);
            if (tf.isValid()) ev["ora_fine"] = tf.toString("HH:mm");
        }
    } else if (mA.hasMatch()) {
        const QTime t = oraDa(mA);
        if (t.isValid()) { ev["ora_inizio"] = t.toString("HH:mm"); oraAt = mA.capturedStart(); }
    }

    /* ── Luogo: "presso X" (fino a fine frase o alla data/ora) ── */
    int luogoAt = -1;
    {
        static const QRegularExpression reL(
            R"(\bpresso\s+(.{2,60}?)(?=\s+(?:il|dalle|alle)\b|,|\.|$))");
        const auto m = reL.match(lo);
        if (m.hasMatch()) {
            /* prende il testo con il case originale */
            ev["luogo"] = task.mid(m.capturedStart(1), m.capturedLength(1)).trimmed();
            luogoAt = m.capturedStart();
        }
    }

    /* ── Titolo: dal termine dell'intento fino al primo marcatore ── */
    int cut = lo.length();
    for (int p : { dateAt, oraAt, luogoAt })
        if (p > mi.capturedEnd() && p < cut) cut = p;
    QString titolo = task.mid(mi.capturedEnd(), cut - mi.capturedEnd()).trimmed();
    /* connettivi iniziali e residui finali ("... il", "... in data") */
    static const QRegularExpression reLead(
        R"rx(^(?:per\s+(?:il|la|lo|l')?\s*|di\s+|del(?:la|lo)?\s+|dal\s+titolo\s+|chiamato\s+|intitolato\s+|:\s*)+)rx",
        QRegularExpression::CaseInsensitiveOption);
    titolo.remove(reLead);
    static const QRegularExpression reTail(
        R"rx((?:\s+(?:il|lo|la|in\s+data|data|per|del))+$)rx",
        QRegularExpression::CaseInsensitiveOption);
    titolo.remove(reTail);
    titolo = titolo.trimmed();
    if (titolo.isEmpty()) titolo = "Evento";
    else titolo[0] = titolo[0].toUpper();
    ev["titolo"] = titolo;

    return ev;
}

/* ══════════════════════════════════════════════════════════════
   _inject_help — risponde a "cosa sai fare?"/"aiuto"/"comandi" con
   l'elenco (tabella Markdown) di tutte le domande rapide zero-LLM
   disponibili, invece di lasciare che il modello inventi/dimentichi
   funzionalità. Marcata con prefisso HELP_MARKDOWN: (non "[Calcolo
   locale:") perché il contenuto è multi-riga con una tabella — va
   fatto passare per markdownToHtml(), non per buildLocalBubble()
   (che tratta il testo come plain text ed escaperebbe le pipe della
   tabella). Intercettata a parte in runPipeline().
   ══════════════════════════════════════════════════════════════ */
QString _inject_help(const QString& task)
{
    const QString lo = task.toLower().trimmed();
    if (lo.length() > 60) return task;   /* frase di aiuto: sempre breve */

    static const QRegularExpression re(
        R"(^(?:ciao[, ]+)?(?:che\s+)?(?:cosa\s+(?:sai|puoi|posso|si\s+pu(?:o'|ò))\s+fare|che\s+(?:funzioni|cose|comandi)\s+hai|quali\s+sono\s+le\s+tue\s+funzioni|aiuto|help|comandi(?:\s+disponibili)?|guida|elenco\s+(?:comandi|funzioni))\??\.?$)",
        QRegularExpression::CaseInsensitiveOption);
    if (!re.match(lo).hasMatch()) return task;

    /* Esempio di grafico scelto a caso ad ogni richiesta — cambia la
     * formula proposta così l'utente ne vede una diversa ogni volta.
     * Riconosciute da FormulaParser::tryExtract() nella chat (zero token,
     * plot Cartesiano istantaneo). Le altre 16 tipologie di ChartType
     * (Torta, Istogramma, Radar, Candlestick, ecc.) esistono nel canvas
     * dedicato di Matematica/Grafico ma richiedono dati strutturati dal
     * form, non una singola riga di chat — non le elenco qui per non
     * promettere una scorciatoia chat che non esiste ancora (vedi TODO D-13). */
    static const QStringList kChartExamples = {
        "grafico di sin(x)", "grafico di x^2 - 4", "grafico di cos(x)*exp(-x/5)",
        "y = 1/x", "grafico di sqrt(abs(x))", "grafico di tan(x)",
        "y = x^3 - 3x", "grafico di log(x+10)",
    };
    const QString chartExample =
        kChartExamples.at(QRandomGenerator::global()->bounded(kChartExamples.size()));

    /* Colonna "{{PROVA:comando}}": segnaposto sostituito in runPipeline
     * (DOPO markdownToHtml) con un link <a href='prova:BASE64URL'>▶ Prova</a>
     * che inserisce il comando nella casella e lo invia. Il comando dentro
     * il segnaposto deve evitare & < > * ` _ (attraversa escHtml/inlineFmt). */
    return QString::fromUtf8(
        "HELP_MARKDOWN:"
        "**Ecco le domande che rispondo istantaneamente senza interpellare il modello AI "
        "(risposta locale, zero token):**\n\n"
        "| Categoria | Esempio | Prova | Cosa calcola |\n"
        "|---|---|---|---|\n"
        "| \xf0\x9f\x94\xa2 Matematica/Fisica | \"quanti watt con 12V e 2A\" | {{PROVA:quanti watt con 12V e 2A}} | Ohm, RC, chimica, conversioni unit\xc3\xa0 |\n"
        "| \xf0\x9f\x93\x90 Geometria | \"area di un rettangolo con base 5 e altezza 3\" | {{PROVA:area di un rettangolo con base 5 e altezza 3}} | Triangolo, rettangolo, cubo, cilindro |\n"
        "| \xe2\x9a\x96\xef\xb8\x8f IMC | \"imc per 70kg e 1.75m\" | {{PROVA:imc per 70kg e 1.75m}} | Indice di massa corporea + fascia |\n"
        "| \xf0\x9f\x9b\x90 Numeri romani | \"quanto \xc3\xa8 MCMXCIV\" / \"1994 in romano\" | {{PROVA:quanto \xc3\xa8 MCMXCIV}} | Conversione bidirezionale |\n"
        "| \xf0\x9f\x93\x85 Date | \"quanti mesi mancano a dicembre\" | {{PROVA:quanti mesi mancano a dicembre}} | Calendario reale, gg/mesi/anni |\n"
        "| \xf0\x9f\x8e\x82 Et\xc3\xa0 | \"quanti anni ho se sono nato il 15/03/1990\" | {{PROVA:quanti anni ho se sono nato il 15/03/1990}} | Et\xc3\xa0 esatta + giorni al compleanno |\n"
        "| \xf0\x9f\x93\x86 Giorno settimana | \"che giorno era il 25/12/2020\" | {{PROVA:che giorno era il 25/12/2020}} | Calendario deterministico |\n"
        "| \xf0\x9f\x8c\x8d Fuso orario | \"che ora \xc3\xa8 a New York\" | {{PROVA:che ora \xc3\xa8 a New York}} | Ora reale in altre citt\xc3\xa0 |\n"
        "| \xf0\x9f\x92\xbc Giorni lavorativi | \"giorni lavorativi tra 03/07/2026 e 15/08/2026\" | {{PROVA:giorni lavorativi tra 03/07/2026 e 15/08/2026}} | Esclusi sabati/domeniche |\n"
        "| \xf0\x9f\x8d\xb3 Cucina | \"quanti grammi sono 200ml di farina\" | {{PROVA:quanti grammi sono 200ml di farina}} | ml\xe2\x86\x94grammi, forno, cucchiai/tazze |\n"
        "| \xf0\x9f\x93\x8f Conversioni | \"quanti mph sono 100 km/h\" | {{PROVA:quanti mph sono 100 km/h}} | Velocit\xc3\xa0, Ohm, anni luce, unit\xc3\xa0 astronomiche |\n"
        "| \xf0\x9f\x92\xb3 IBAN | \"\xc3\xa8 valido questo IBAN: IT60...\" | {{PROVA:\xc3\xa8 valido questo IBAN: IT60X0542811101000000123456}} | Cifra di controllo (mod-97) |\n"
        "| \xf0\x9f\x86\x94 Codice Fiscale | \"RSSMRA85M01H501Q \xc3\xa8 valido?\" | {{PROVA:RSSMRA85M01H501Q \xc3\xa8 valido?}} | Checksum + data nascita/sesso |\n"
        "| \xf0\x9f\x8f\xa2 Partita IVA | \"12345678903 \xc3\xa8 una p.iva valida?\" | {{PROVA:12345678903 \xc3\xa8 una p.iva valida?}} | Checksum ufficiale |\n"
        "| \xf0\x9f\x92\xb0 Sconti/IVA/% | \"sconto del 15% su 80 euro\" | {{PROVA:sconto del 15% su 80 euro}} | Percentuali, sconti, scorporo IVA |\n"
        "| \xf0\x9f\x93\x88 Interesse/Mutuo | \"rata di un mutuo da 100000 euro al 3% in 20 anni\" | {{PROVA:rata di un mutuo da 100000 euro al 3% in 20 anni}} | Composto, ammortamento francese |\n"
        "| \xf0\x9f\x94\x91 Generatori | \"genera una password di 20 caratteri\" | {{PROVA:genera una password di 20 caratteri}} | UUID, hash, password casuali |\n"
        "| \xf0\x9f\x93\x86 Evento calendario | \"creami un evento festa in maschera il 31/10/2026 dalle 21 alle 23\" | {{PROVA:creami un evento festa in maschera il 31/10/2026 dalle 21 alle 23}} | QR da scansionare col telefono \xe2\x86\x92 salva l'evento sul calendario |\n"
        "| \xf0\x9f\x8e\x82 Compleanno (QR) | \"crea un evento compleanno di Marco il 15/03 alle 20\" | {{PROVA:crea un evento compleanno di Marco il 15/03 alle 20}} | Stesso QR degli eventi, funziona anche per i compleanni |\n"
        "| \xf0\x9f\x93\x9d Statistiche testo | \"quante parole ha: <testo>\" | {{PROVA:quante parole ha: Nel mezzo del cammin di nostra vita}} | Parole/caratteri/frasi + tempo lettura |\n"
        "| \xf0\x9f\xa7\xae Algoritmi classici | \"mcd tra 48 e 18\", \"fattorizzazione di 360\", \"decimo numero di fibonacci\", "
        "\"torre di hanoi con 8 dischi\", \"profitto massimo con prezzi 7,1,5,3,6,4\", \"quante soluzioni ha il problema delle 8 regine\" "
        "| {{PROVA:mcd tra 48 e 18}} | MCD/MCM, fattori primi, Pascal, Fibonacci, "
        "Catalan, Collatz, Hanoi, profitto azioni, inversioni, posizione in array, edit distance/LCS, ricerca pattern, N Regine |\n"
        "| \xf0\x9f\x93\x8a Statistica su lista | \"media di 3,5,7,9,2\" | {{PROVA:media di 3,5,7,9,2}} | Media, mediana, moda, deviazione standard |\n"
        "| \xe2\x9c\x9d\xef\xb8\x8f Data di Pasqua | \"quando \xc3\xa8 pasqua nel 2026\" | {{PROVA:quando \xc3\xa8 pasqua nel 2026}} | Algoritmo di Gauss, qualsiasi anno |\n"
        "| \xf0\x9f\x8c\x99 Fase lunare | \"fase lunare del 15/03/2026\" | {{PROVA:fase lunare del 15/03/2026}} | Approssimata (\xc2\xb11 giorno), con % illuminazione |\n"
        "| \xf0\x9f\x94\xa2 Base numerica | \"converti 255 in base 7\" | {{PROVA:converti 255 in base 7}} | Qualsiasi base da 2 a 36 |\n"
        "| \xf0\x9f\x93\x90 Progressioni | \"progressione aritmetica primo 2 ragione 3 termini 10\", \"progressione geometrica primo 2 ragione 3 termini 5\" "
        "| {{PROVA:progressione aritmetica primo 2 ragione 3 termini 10}} | Somma di progressione aritmetica/geometrica |\n"
        "| \xf0\x9f\x87\xae\xf0\x9f\x87\xb9 Lira \xe2\x86\x94 Euro | \"quanto sono 1000000 lire in euro\" | {{PROVA:quanto sono 1000000 lire in euro}} | Tasso fisso ufficiale 1936.27 |\n"
        "| \xf0\x9f\x93\x84 Scadenza tra X | \"tra 3 anni da oggi\" | {{PROVA:tra 3 anni da oggi}} | Data risultante (contratti/garanzie) |\n"
        "| \xf0\x9f\x93\x88 Grafico | \"") + chartExample + QString::fromUtf8(
        "\" | {{PROVA:") + chartExample + QString::fromUtf8(
        "}} | Plot Cartesiano istantaneo (prova questo!) |\n\n"
        "**Con l'aiuto del modello AI** (il modello selezionato chiama un tool \xe2\x80\x94 "
        "serve un modello tool-capable e, per la valuta, la rete):\n\n"
        "| Categoria | Esempio | Prova | Cosa fa |\n"
        "|---|---|---|---|\n"
        "| \xf0\x9f\x92\xb1 Cambio valuta | \"100 EUR in USD\" | {{PROVA:100 EUR in USD}} | Tasso reale aggiornato (BCE, via tool) |\n"
        "| \xf0\x9f\x86\x94 Genera Codice Fiscale | \"calcola il codice fiscale di Mario Rossi, nato il 01/05/1985 a Milano, maschio\" | {{PROVA:calcola il codice fiscale di Mario Rossi, nato il 01/05/1985 a Milano, maschio}} | Checksum ufficiale, decodifica comune |\n"
        "| \xf0\x9f\x92\xbc TFR rivalutato | \"TFR con stipendio lordo 30000 euro, inflazione 2% per 10 anni\" | {{PROVA:TFR con stipendio lordo 30000 euro, inflazione 2% per 10 anni}} | Quota annua + rivalutazione |\n\n"
        "**Grafici**: scrivendo *\"grafico di FORMULA\"* oppure *\"y = FORMULA\"* disegno subito "
        "il plot cartesiano nella chat. Per torta, istogramma, radar, candlestick e le altre "
        "tipologie disponibili, usa il canvas dedicato nella tab Matematica/Grafico (richiedono "
        "dati strutturati, non una singola riga di chat).\n\n"
        "Per tutto il resto (spiegazioni, scrittura, codice, ricerca...) rispondo con il modello AI selezionato.");
}

/* ── Helper puri per _inject_finance: validazione IBAN (mod-97, ISO 7064)
 * e Codice Fiscale (D.M. 23/12/1976) — nessuna dipendenza UI. ── */
bool _ibanValid(const QString& ibanRaw)
{
    QString iban = ibanRaw.toUpper();
    iban.remove(' ');
    if (iban.length() < 15 || iban.length() > 34) return false;
    static const QRegularExpression reShape(R"(^[A-Z]{2}\d{2}[A-Z0-9]+$)");
    if (!reShape.match(iban).hasMatch()) return false;

    const QString rearranged = iban.mid(4) + iban.left(4);
    qint64 remainder = 0;
    for (const QChar& ch : rearranged) {
        int val;
        if (ch.isDigit())      val = ch.digitValue();
        else if (ch.isUpper()) val = ch.unicode() - 'A' + 10;
        else return false;
        if (val >= 10) {
            remainder = (remainder * 10 + val / 10) % 97;
            remainder = (remainder * 10 + val % 10) % 97;
        } else {
            remainder = (remainder * 10 + val) % 97;
        }
    }
    return remainder == 1;
}

bool _cfChecksumValid(const QString& cfUpper)
{
    if (cfUpper.length() != 16) return false;
    static const QHash<QChar,int> kOdd = {
        {'0',1},{'1',0},{'2',5},{'3',7},{'4',9},{'5',13},{'6',15},{'7',17},{'8',19},{'9',21},
        {'A',1},{'B',0},{'C',5},{'D',7},{'E',9},{'F',13},{'G',15},{'H',17},{'I',19},{'J',21},
        {'K',2},{'L',4},{'M',18},{'N',20},{'O',11},{'P',3},{'Q',6},{'R',8},{'S',12},{'T',14},
        {'U',16},{'V',10},{'W',22},{'X',25},{'Y',24},{'Z',23}
    };
    static const QHash<QChar,int> kEven = {
        {'0',0},{'1',1},{'2',2},{'3',3},{'4',4},{'5',5},{'6',6},{'7',7},{'8',8},{'9',9},
        {'A',0},{'B',1},{'C',2},{'D',3},{'E',4},{'F',5},{'G',6},{'H',7},{'I',8},{'J',9},
        {'K',10},{'L',11},{'M',12},{'N',13},{'O',14},{'P',15},{'Q',16},{'R',17},{'S',18},{'T',19},
        {'U',20},{'V',21},{'W',22},{'X',23},{'Y',24},{'Z',25}
    };
    int sum = 0;
    for (int i = 0; i < 15; ++i) {
        const QChar c = cfUpper.at(i);
        const bool oddPos = (i % 2 == 0);   /* i=0 → posizione 1 (dispari) */
        if (oddPos) { if (!kOdd.contains(c))  return false; sum += kOdd[c];  }
        else        { if (!kEven.contains(c)) return false; sum += kEven[c]; }
    }
    return cfUpper.at(15) == QChar('A' + (sum % 26));
}

/* Decodifica data di nascita + sesso dal Codice Fiscale (già validato).
 * L'anno a 2 cifre è ambiguo tra 19XX/20XX: si sceglie il secolo che rende
 * la data non futura rispetto a oggi (euristica standard, non certezza). */
QString _cfDecode(const QString& cfUpper)
{
    static const QHash<QChar,int> kMese = {
        {'A',1},{'B',2},{'C',3},{'D',4},{'E',5},{'H',6},
        {'L',7},{'M',8},{'P',9},{'R',10},{'S',11},{'T',12}
    };
    const QChar meseC = cfUpper.at(8);
    if (!kMese.contains(meseC)) return QString();
    bool ok1, ok2;
    const int yy  = cfUpper.mid(6, 2).toInt(&ok1);
    int day       = cfUpper.mid(9, 2).toInt(&ok2);
    if (!ok1 || !ok2) return QString();
    const bool femmina = day > 40;
    if (femmina) day -= 40;
    const int mese = kMese[meseC];
    if (day < 1 || day > 31) return QString();

    const int curYY  = QDate::currentDate().year() % 100;
    const int secolo = (yy > curYY) ? 1900 : 2000;
    const QDate nascita(secolo + yy, mese, day);
    const QString belfiore = cfUpper.mid(11, 4);

    return QString("nato/a il %1 (%2), Belfiore luogo di nascita: %3 "
                    "(anno a 2 cifre ambiguo tra %4 e %5, mostrato quello più probabile)")
        .arg(nascita.isValid() ? QLocale(QLocale::Italian).toString(nascita, "d MMMM yyyy") : QString("%1/%2/%3xx").arg(day).arg(mese).arg(yy))
        .arg(femmina ? "sesso femminile" : "sesso maschile")
        .arg(belfiore)
        .arg(1900 + yy).arg(2000 + yy);
}

/* ══════════════════════════════════════════════════════════════
   _inject_finance — valida IBAN/Codice Fiscale e calcola sconti/IVA/
   percentuali direttamente nel testo dell'utente, senza passare
   dall'LLM (stesso motivo di _inject_date_calc: sono calcoli
   deterministici, un modello può sbagliare cifra di controllo o
   percentuale). Stile identico a _inject_science: "[Calcolo locale:
   ...]" antepone il task se riconosce un pattern, altrimenti lo
   ritorna invariato.
   ══════════════════════════════════════════════════════════════ */
QString _inject_finance(const QString& task)
{
    const QString raw = task.trimmed();
    if (raw.length() > 300) return task;
    const QString lo = raw.toLower();

    /* ── IBAN: cerca un token che rispetti la forma IBAN, spazi inclusi
     * (l'utente spesso lo scrive raggruppato a blocchi di 4) ── */
    {
        static const QRegularExpression reIban(
            R"(\b([A-Za-z]{2}\d{2}(?:[ ]?[A-Za-z0-9]{1,4}){2,7})\b)");
        const auto m = reIban.match(raw);
        if (m.hasMatch()) {
            const QString token = m.captured(1);
            const QString clean = QString(token).remove(' ').toUpper();
            if (clean.length() >= 15) {
                const bool valid = _ibanValid(clean);
                return QString("[Calcolo locale: IBAN %1 \xe2\x80\x94 %2]\n\n")
                    .arg(clean, valid ? "VALIDO (cifra di controllo corretta)"
                                       : "NON VALIDO (cifra di controllo errata o formato non riconosciuto)")
                    + task;
            }
        }
    }

    /* ── Codice Fiscale: 16 caratteri nella forma esatta prevista ── */
    {
        static const QRegularExpression reCf(
            R"(\b([A-Za-z]{6}\d{2}[A-Za-z]\d{2}[A-Za-z]\d{3}[A-Za-z])\b)");
        const auto m = reCf.match(raw);
        if (m.hasMatch()) {
            const QString cf = m.captured(1).toUpper();
            const bool valid = _cfChecksumValid(cf);
            QString msg = QString("[Calcolo locale: Codice Fiscale %1 \xe2\x80\x94 %2")
                .arg(cf, valid ? "VALIDO" : "NON VALIDO (cifra di controllo errata)");
            if (valid) {
                const QString decoded = _cfDecode(cf);
                if (!decoded.isEmpty()) msg += ", " + decoded;
            }
            return msg + "]\n\n" + task;
        }
    }

    /* ── Percentuale di un valore: "quanto è il 20% di 150" ── */
    {
        static const QRegularExpression re(
            R"((\d+(?:[.,]\d+)?)\s*%\s+di\s+(\d+(?:[.,]\d+)?))",
            QRegularExpression::CaseInsensitiveOption);
        const auto m = re.match(lo);
        if (m.hasMatch()) {
            const double pct = m.captured(1).replace(',', '.').toDouble();
            const double val = m.captured(2).replace(',', '.').toDouble();
            const double r = val * pct / 100.0;
            return QString("[Calcolo locale: il %1% di %2 = %3]\n\n")
                .arg(QString::number(pct, 'g', 6), QString::number(val, 'g', 6), QString::number(r, 'f', 2))
                + task;
        }
    }

    /* ── Sconto: "sconto del 15% su 80 euro" ── */
    {
        static const QRegularExpression re(
            R"(sconto\w*\s+(?:del\s+)?(\d+(?:[.,]\d+)?)\s*%.{0,15}(?:su|di)\s+(\d+(?:[.,]\d+)?))",
            QRegularExpression::CaseInsensitiveOption);
        const auto m = re.match(lo);
        if (m.hasMatch()) {
            const double pct = m.captured(1).replace(',', '.').toDouble();
            const double val = m.captured(2).replace(',', '.').toDouble();
            const double risparmio = val * pct / 100.0;
            return QString("[Calcolo locale: sconto %1% su %2 \xe2\x86\x92 prezzo finale %3, risparmio %4]\n\n")
                .arg(QString::number(pct, 'g', 6), QString::number(val, 'f', 2),
                     QString::number(val - risparmio, 'f', 2), QString::number(risparmio, 'f', 2))
                + task;
        }
    }

    /* ── Aumento/maggiorazione: "aumento del 10% su 200" ── */
    {
        static const QRegularExpression re(
            R"((?:aument\w*|maggiorazione)\s+(?:del\s+)?(\d+(?:[.,]\d+)?)\s*%.{0,15}(?:su|di)\s+(\d+(?:[.,]\d+)?))",
            QRegularExpression::CaseInsensitiveOption);
        const auto m = re.match(lo);
        if (m.hasMatch()) {
            const double pct = m.captured(1).replace(',', '.').toDouble();
            const double val = m.captured(2).replace(',', '.').toDouble();
            const double r = val * (1.0 + pct / 100.0);
            return QString("[Calcolo locale: aumento %1% su %2 \xe2\x86\x92 %3]\n\n")
                .arg(QString::number(pct, 'g', 6), QString::number(val, 'f', 2), QString::number(r, 'f', 2))
                + task;
        }
    }

    /* ── Scorporo IVA: "scorporo iva 122 al 22%" / "togli iva 22% da 122" ── */
    {
        static const QRegularExpression re(
            R"((?:scorpor\w*|togli)\s+(?:l['\x27]?)?iva.{0,20}?(\d+(?:[.,]\d+)?).{0,20}?(?:(\d+(?:[.,]\d+)?)\s*%)?)",
            QRegularExpression::CaseInsensitiveOption);
        const auto m = re.match(lo);
        if (m.hasMatch() && !m.captured(1).isEmpty()) {
            const double totale = m.captured(1).replace(',', '.').toDouble();
            const double iva    = m.captured(2).isEmpty() ? 22.0 : m.captured(2).replace(',', '.').toDouble();
            const double imponibile = totale / (1.0 + iva / 100.0);
            return QString("[Calcolo locale: scorporo IVA %1% da %2 \xe2\x86\x92 imponibile %3, IVA %4]\n\n")
                .arg(QString::number(iva, 'g', 6), QString::number(totale, 'f', 2),
                     QString::number(imponibile, 'f', 2), QString::number(totale - imponibile, 'f', 2))
                + task;
        }
    }

    /* ── Aggiunta IVA: "100 + iva al 22%" / "prezzo con iva 22% su 100" ── */
    {
        static const QRegularExpression re(
            R"((\d+(?:[.,]\d+)?)\s*(?:\+|e|con)?\s*iva.{0,15}?(?:(\d+(?:[.,]\d+)?)\s*%)?)",
            QRegularExpression::CaseInsensitiveOption);
        const auto m = re.match(lo);
        if (m.hasMatch() && lo.contains("iva") && !lo.contains("scorpor") && !lo.contains("togli")) {
            const double imponibile = m.captured(1).replace(',', '.').toDouble();
            const double iva = m.captured(2).isEmpty() ? 22.0 : m.captured(2).replace(',', '.').toDouble();
            const double totale = imponibile * (1.0 + iva / 100.0);
            return QString("[Calcolo locale: %1 + IVA %2% \xe2\x86\x92 totale %3 (IVA %4)]\n\n")
                .arg(QString::number(imponibile, 'f', 2), QString::number(iva, 'g', 6),
                     QString::number(totale, 'f', 2), QString::number(totale - imponibile, 'f', 2))
                + task;
        }
    }

    /* ── Partita IVA: 11 cifre, checksum ufficiale (somma pesata mod 10) ──
     * Verificato indipendentemente: somma le prime 10 cifre (dispari 1-based
     * = valore diretto, pari = raddoppiato, -9 se >9), cifra di controllo
     * attesa = (10 - somma mod 10) mod 10, confrontata con l'11a cifra. */
    {
        static const QRegularExpression rePiva(
            R"(\b(?:p\.?\s?iva|partita\s+iva)\b.{0,15}?(\d{11})\b|\b(\d{11})\b.{0,15}?\b(?:p\.?\s?iva|partita\s+iva)\b)",
            QRegularExpression::CaseInsensitiveOption);
        const auto m = rePiva.match(lo);
        const QString piva = m.hasMatch() ? (m.captured(1).isEmpty() ? m.captured(2) : m.captured(1)) : QString();
        if (!piva.isEmpty()) {
            int sum = 0;
            for (int i = 0; i < 10; ++i) {
                int d = piva.at(i).digitValue();
                if (i % 2 == 1) { d *= 2; if (d > 9) d -= 9; }
                sum += d;
            }
            const int expected = (10 - (sum % 10)) % 10;
            const bool valid = expected == piva.at(10).digitValue();
            return QString("[Calcolo locale: Partita IVA %1 \xe2\x80\x94 %2]\n\n")
                .arg(piva, valid ? "VALIDA (cifra di controllo corretta)"
                                  : "NON VALIDA (cifra di controllo errata)")
                + task;
        }
    }

    /* ── Interesse composto: "quanto diventano 1000 euro al 5% in 10 anni" ── */
    {
        static const QRegularExpression re(
            R"((\d+(?:[.,]\d+)?)\s*(?:€|euro)?.{0,20}?(\d+(?:[.,]\d+)?)\s*%.{0,20}?(\d+)\s*ann)",
            QRegularExpression::CaseInsensitiveOption);
        const auto m = re.match(lo);
        if (m.hasMatch() && (lo.contains("diventano") || lo.contains("interesse composto")
                              || lo.contains("capitalizzazione"))) {
            const double capitale = m.captured(1).replace(',', '.').toDouble();
            const double tasso    = m.captured(2).replace(',', '.').toDouble();
            const int    anni     = m.captured(3).toInt();
            if (capitale > 0 && anni > 0 && anni <= 200) {
                const double montante = capitale * std::pow(1.0 + tasso / 100.0, anni);
                return QString("[Calcolo locale: %1 \xe2\x82\xac al %2% annuo (interesse composto) per %3 anni "
                                "\xe2\x86\x92 %4 \xe2\x82\xac (interessi maturati: %5 \xe2\x82\xac)]\n\n")
                    .arg(QString::number(capitale, 'f', 2), QString::number(tasso, 'g', 6))
                    .arg(anni)
                    .arg(QString::number(montante, 'f', 2), QString::number(montante - capitale, 'f', 2))
                    + task;
            }
        }
    }

    /* ── Rata mutuo/prestito (ammortamento francese, rata costante) ──
     * "rata di un mutuo da 100000 euro al 3% in 20 anni" */
    {
        static const QRegularExpression re(
            R"((?:rata|mutuo|prestito).{0,30}?(\d+(?:[.,]\d+)?)\s*(?:€|euro)?.{0,20}?(\d+(?:[.,]\d+)?)\s*%.{0,20}?(\d+)\s*ann)",
            QRegularExpression::CaseInsensitiveOption);
        const auto m = re.match(lo);
        if (m.hasMatch() && (lo.contains("rata") || lo.contains("mutuo") || lo.contains("prestito"))) {
            const double capitale  = m.captured(1).replace(',', '.').toDouble();
            const double tassoAnn  = m.captured(2).replace(',', '.').toDouble();
            const int    anni      = m.captured(3).toInt();
            if (capitale > 0 && anni > 0 && anni <= 60) {
                const double i = tassoAnn / 100.0 / 12.0;
                const int    n = anni * 12;
                const double rata = (i > 0)
                    ? capitale * i / (1.0 - std::pow(1.0 + i, -n))
                    : capitale / n;   /* tasso 0%: rata = capitale/n */
                const double totalePagato = rata * n;
                return QString("[Calcolo locale: mutuo di %1 \xe2\x82\xac al %2% annuo in %3 anni "
                                "(ammortamento francese, rata costante) \xe2\x86\x92 rata mensile %4 \xe2\x82\xac, "
                                "totale pagato %5 \xe2\x82\xac (interessi %6 \xe2\x82\xac)]\n\n")
                    .arg(QString::number(capitale, 'f', 2), QString::number(tassoAnn, 'g', 6))
                    .arg(anni)
                    .arg(QString::number(rata, 'f', 2), QString::number(totalePagato, 'f', 2),
                         QString::number(totalePagato - capitale, 'f', 2))
                    + task;
            }
        }
    }

    /* ── Lira \xe2\x86\x94 Euro (D-16), tasso fisso ufficiale di conversione
     * 1936.27 lire = 1 euro (Regolamento CE 2866/98, in vigore dal 1999) ── */
    {
        static const QRegularExpression reLireEuro(
            R"((\d+(?:[.,]\d+)?)\s*(?:lire|£)\b.{0,15}?\bin\s+euro)",
            QRegularExpression::CaseInsensitiveOption);
        auto m = reLireEuro.match(lo);
        if (m.hasMatch()) {
            const double lire = m.captured(1).replace(',', '.').toDouble();
            const double euro = lire / 1936.27;
            return QString("[Calcolo locale: %1 lire \xc3\xb7 1936.27 (tasso fisso ufficiale) = %2 \xe2\x82\xac]\n\n")
                .arg(QString::number(lire, 'f', 0), QString::number(euro, 'f', 2)) + task;
        }
        static const QRegularExpression reEuroLire(
            R"((\d+(?:[.,]\d+)?)\s*euro\b.{0,15}?\bin\s+lire\b)",
            QRegularExpression::CaseInsensitiveOption);
        auto m2 = reEuroLire.match(lo);
        if (m2.hasMatch()) {
            const double euro = m2.captured(1).replace(',', '.').toDouble();
            const double lire = euro * 1936.27;
            return QString("[Calcolo locale: %1 \xe2\x82\xac \xc3\x97 1936.27 (tasso fisso ufficiale) = %2 lire]\n\n")
                .arg(QString::number(euro, 'f', 2), QString::number(lire, 'f', 0)) + task;
        }
    }

    return task;
}

/* ══════════════════════════════════════════════════════════════
   _inject_knowledge (D-26) — lookup diretto su user_knowledge.md
   ──────────────────────────────────────────────────────────────
   P::prependKnowledge() inietta già l'INTERO file nel system prompt ad
   ogni richiesta: il modello lo vede sempre. Questa guardia non aggiunge
   informazione che il modello non abbia già — serve solo a rispondere
   con zero token/latenza sui lookup diretti e inequivocabili ("qual è
   l'IP della telecamera?"), lasciando comunque scorrere alla via normale
   (LLM con memoria intera nel contesto) qualunque domanda più articolata
   o ambigua. Per questo il trigger è deliberatamente stretto:
     1. la frase contiene un verbo di domanda esplicito da lookup
        ("qual è", "dimmi", "ricordami", "cos'è"...) — esclude domande
        generiche/discorsive, che meritano il modello;
     2. almeno 2 parole chiave non-stopword nella domanda;
     3. ESATTAMENTE UNA riga di user_knowledge.md contiene tutte quelle
        parole chiave — se le righe candidate sono 0 o >1 (ambiguo), non
        si intercetta: mai nascondere informazione al modello per un
        match incerto, il fallback sicuro è "lascia decidere all'LLM".
   ══════════════════════════════════════════════════════════════ */
/* Nucleo puro/testabile: 'knowledge' passato esplicitamente (nessun I/O),
 * separato da _inject_knowledge() per poter testare il matching senza
 * dipendere dal vero user_knowledge.md (dati personali dell'utente, non
 * deterministico). Ritorna la riga trovata (senza wrapping), o stringa
 * vuota se non c'è un match univoco e sicuro. */
QString _knowledgeLookup(const QString& task, const QString& knowledge)
{
    if (task.length() > 150) return {}; /* i lookup diretti sono domande brevi */

    /* UseUnicodePropertiesOption è necessaria: senza, PCRE tratta \b come
     * confine ASCII-only e "è"/"é" non contano come caratteri di parola —
     * il \b finale dopo "qual è"/"cos'è" non scatterebbe mai (bug trovato
     * e verificato con test standalone durante lo sviluppo di D-26). */
    static const QRegularExpression reTrigger(
        R"(\b(qual\s*è|quale\s*è|quali\s+sono|dimmi|ricorda(mi)?|cos['’]?è|che\s+cos['’]?è|come\s+si\s+chiama)\b)",
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
    if (!reTrigger.match(task).hasMatch()) return {};

    if (knowledge.trimmed().isEmpty()) return {};

    /* Stopword italiane comuni — escluse dalle parole chiave della domanda.
     * Deliberatamente non esaustiva: meglio un falso negativo (non
     * intercetta, va all'LLM) che un falso positivo. */
    static const QSet<QString> kStop = {
        "qual","quale","quali","sono","dimmi","ricorda","ricordami","cos","che",
        "come","si","chiama","il","lo","la","i","gli","le","un","uno","una",
        "di","del","dello","della","dei","degli","delle","a","al","allo","alla",
        "ai","agli","alle","e","è","in","con","su","per","tra","fra","mio","mia",
        "miei","mie","tuo","tua","questo","questa","questi","queste","dell",
    };
    static const QRegularExpression reWord(
        "[\\p{L}\\p{N}]+", QRegularExpression::UseUnicodePropertiesOption);

    QStringList keywords;
    auto itW = reWord.globalMatch(task.toLower());
    while (itW.hasNext()) {
        const QString w = itW.next().captured(0);
        if (w.length() >= 2 && !kStop.contains(w)) keywords.append(w);
    }
    if (keywords.size() < 2) return {}; /* troppo generico per un lookup sicuro */

    QString bestLine; int matchCount = 0;
    const QStringList lines = knowledge.split(QLatin1Char('\n'));
    for (const QString& line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#'))) continue;
        const QString lineLow = trimmed.toLower();
        bool allFound = true;
        for (const QString& kw : keywords)
            if (!lineLow.contains(kw)) { allFound = false; break; }
        if (allFound) {
            ++matchCount;
            bestLine = trimmed;
            if (matchCount > 1) break; /* ambiguo — inutile continuare */
        }
    }
    if (matchCount != 1) return {}; /* 0 o ambiguo → lascia decidere al modello */

    QString answer = bestLine;
    answer.remove(QLatin1Char(']')); /* mai rompere il parsing "[Calcolo locale: ...]" */
    return answer;
}

/* ══════════════════════════════════════════════════════════════
   _inject_knowledge (D-26) — lookup diretto su user_knowledge.md
   ──────────────────────────────────────────────────────────────
   P::prependKnowledge() inietta già l'INTERO file nel system prompt ad
   ogni richiesta: il modello lo vede sempre. Questa guardia non aggiunge
   informazione che il modello non abbia già — serve solo a rispondere
   con zero token/latenza sui lookup diretti e inequivocabili ("qual è
   l'IP della telecamera?"), lasciando comunque scorrere alla via normale
   (LLM con memoria intera nel contesto) qualunque domanda più articolata
   o ambigua. Logica di matching in _knowledgeLookup() (testabile, nessun
   I/O) — qui solo il wiring col vero file utente.
   ══════════════════════════════════════════════════════════════ */
QString _inject_knowledge(const QString& task)
{
    const QString answer = _knowledgeLookup(task, P::readUserKnowledge());
    if (answer.isEmpty()) return task;
    return "[Calcolo locale: dalla tua memoria \xe2\x80\x94 " + answer + "]\n\n";
}

/* ══════════════════════════════════════════════════════════════
   _inject_generator — UUID/password/hash generati localmente invece
   che "inventati" dall'LLM (un modello non ha accesso a un generatore
   crittografico vero, e per un hash deve semplicemente calcolarlo:
   se glielo chiedi in chat rischia di allucinare cifre plausibili
   ma sbagliate). Stesso stile "[Calcolo locale: ...]" degli altri
   _inject_*.
   ══════════════════════════════════════════════════════════════ */
QString _inject_generator(const QString& task)
{
    const QString raw = task.trimmed();
    if (raw.length() > 300) return task;
    const QString lo = raw.toLower();

    /* ── UUID v4 ── */
    if (lo.contains("uuid")
        && (lo.contains("genera") || lo.contains("crea") || lo.contains("dammi") || lo.contains("nuovo"))) {
        const QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
        return QString("[Calcolo locale: nuovo UUID v4 = %1]\n\n").arg(uuid) + task;
    }

    /* ── Hash (MD5/SHA-1/SHA-256/SHA-512) di un testo esplicito ── */
    {
        static const QRegularExpression re(
            R"((md5|sha ?1|sha ?256|sha ?512)\s+(?:di|del(?:la)?|hash\s+di)?\s*[:\-]?\s*[\"']?([^\"'\n]{1,200})[\"']?)",
            QRegularExpression::CaseInsensitiveOption);
        const auto m = re.match(raw);
        if (m.hasMatch()) {
            const QString algo  = m.captured(1).toLower().remove(' ');
            const QString testo = m.captured(2).trimmed();
            if (!testo.isEmpty()) {
                QCryptographicHash::Algorithm alg = QCryptographicHash::Md5;
                QString algoName = "MD5";
                if      (algo == "sha1")   { alg = QCryptographicHash::Sha1;   algoName = "SHA-1"; }
                else if (algo == "sha256") { alg = QCryptographicHash::Sha256; algoName = "SHA-256"; }
                else if (algo == "sha512") { alg = QCryptographicHash::Sha512; algoName = "SHA-512"; }
                const QString digest = QCryptographicHash::hash(testo.toUtf8(), alg).toHex();
                return QString("[Calcolo locale: %1(\"%2\") = %3]\n\n").arg(algoName, testo, digest) + task;
            }
        }
    }

    /* ── Password casuale ── */
    if ((lo.contains("password") || lo.contains("chiave"))
        && (lo.contains("genera") || lo.contains("crea") || lo.contains("dammi")
            || lo.contains("casual") || lo.contains("random"))) {
        int len = 16;
        static const QRegularExpression reLen(
            R"((\d{1,3})\s*(?:caratteri|char|cifre))", QRegularExpression::CaseInsensitiveOption);
        const auto ml = reLen.match(lo);
        if (ml.hasMatch()) {
            const int n = ml.captured(1).toInt();
            if (n >= 4 && n <= 128) len = n;
        }

        const bool noSymbols = lo.contains("senza simboli") || lo.contains("solo lettere")
                             || lo.contains("alfanumerica");
        /* Esclude caratteri ambigui (0/O, 1/I/l) per facilitare la lettura manuale */
        QString charset = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789";
        if (!noSymbols) charset += "!@#$%^&*()-_=+";

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dist(0, static_cast<int>(charset.length()) - 1);
        QString pwd;
        for (int i = 0; i < len; ++i) pwd += charset.at(dist(gen));

        return QString("[Calcolo locale: password casuale (%1 caratteri, std::random_device) = %2]\n\n")
            .arg(len).arg(pwd) + task;
    }

    return task;
}

/* ══════════════════════════════════════════════════════════════
   _inject_textstats — "quante parole ha: <testo>" / "conta le parole
   in: <testo>" / "quanto ci vuole a leggere: <testo>" — conteggio
   parole/caratteri/frasi + stima tempo di lettura (~200 parole/min),
   zero LLM. Richiede il testo dopo ":" — a differenza degli altri
   _inject_* non limita la lunghezza a 300 char (qui serve analizzare
   testo lungo, è proprio lo scopo della funzione).
   ══════════════════════════════════════════════════════════════ */
QString _inject_textstats(const QString& task)
{
    static const QRegularExpression re(
        R"(^(?:quante\s+parole\s+ha|conta\s+le\s+parole(?:\s+(?:di|in))?|statistiche\s+(?:di\s+)?(?:questo\s+)?testo|quanto\s+(?:tempo\s+)?ci\s+vuole\s+a\s+leggere)\s*[:\-]\s*(.+)$)",
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    const auto m = re.match(task.trimmed());
    if (!m.hasMatch()) return task;

    const QString testo = m.captured(1).trimmed();
    if (testo.isEmpty()) return task;

    const int caratteri = testo.length();
    const int caratteriNoSpazi = QString(testo).remove(QRegularExpression(R"(\s)")).length();
    const QStringList parole = testo.split(QRegularExpression(R"(\s+)"), Qt::SkipEmptyParts);
    const int numParole = parole.size();
    const int numFrasi = qMax(1, static_cast<int>(
        testo.count(QRegularExpression(R"([.!?]+)"))));
    const double minutiLettura = numParole / 200.0;   /* ~200 parole/minuto, lettura media */
    const QString tempoStr = minutiLettura < 1.0
        ? QString("%1 secondi").arg(qMax(1, qRound(minutiLettura * 60)))
        : QString("%1 minuti").arg(minutiLettura, 0, 'f', 1);

    return QString("[Calcolo locale: %1 parole, %2 caratteri (%3 senza spazi), circa %4 frasi, "
                    "tempo di lettura stimato ~%5]\n\n")
        .arg(numParole).arg(caratteri).arg(caratteriNoSpazi).arg(numFrasi).arg(tempoStr)
        + task;
}

/* ── Edit Distance (Levenshtein) e LCS — DP standalone, non riusa
 * SimulatorePage::genLCS()/genEditDistance() perché quelle prendono
 * ZERO parametri (operano su stringhe fisse hardcoded per la demo
 * visiva, non sull'input reale dell'utente). ── */
static int _editDistance(const QString& a, const QString& b)
{
    const int n = a.length(), m = b.length();
    QVector<QVector<int>> dp(n + 1, QVector<int>(m + 1, 0));
    for (int i = 0; i <= n; ++i) dp[i][0] = i;
    for (int j = 0; j <= m; ++j) dp[0][j] = j;
    for (int i = 1; i <= n; ++i)
        for (int j = 1; j <= m; ++j)
            dp[i][j] = (a[i-1] == b[j-1])
                ? dp[i-1][j-1]
                : 1 + std::min({dp[i-1][j], dp[i][j-1], dp[i-1][j-1]});
    return dp[n][m];
}

static QString _longestCommonSubsequence(const QString& a, const QString& b)
{
    const int n = a.length(), m = b.length();
    QVector<QVector<int>> dp(n + 1, QVector<int>(m + 1, 0));
    for (int i = 1; i <= n; ++i)
        for (int j = 1; j <= m; ++j)
            dp[i][j] = (a[i-1] == b[j-1]) ? dp[i-1][j-1] + 1 : std::max(dp[i-1][j], dp[i][j-1]);
    QString lcs;
    int i = n, j = m;
    while (i > 0 && j > 0) {
        if (a[i-1] == b[j-1]) { lcs.prepend(a[i-1]); --i; --j; }
        else if (dp[i-1][j] >= dp[i][j-1]) --i;
        else --j;
    }
    return lcs;
}

/* ── N-Queens: backtracking reale (D-33, punto 8) — a differenza di
 * `SimulatorePage::genNQueens()` (usata SOLO dal Simulatore visivo, non
 * toccata qui) questa conta TUTTE le soluzioni per N invece di fermarsi
 * a 3 (limite pensato per l'animazione) e invece di leggere il conteggio
 * finale da una tabella hardcoded (`{"1","0","0","2","10","4"}`, valida
 * solo fino a N=6). Nessuna traccia dei passi: come tool serve il
 * conteggio esatto + un esempio di soluzione, non l'animazione. ── */
static void _nQueensCount(int n, QVector<int>& board, int col, qint64& count, QVector<int>& firstSolution)
{
    if (col == n) {
        ++count;
        if (firstSolution.isEmpty()) firstSolution = board;
        return;
    }
    for (int row = 0; row < n; ++row) {
        bool ok = true;
        for (int c = 0; c < col; ++c)
            if (board[c] == row || qAbs(board[c] - row) == qAbs(c - col)) { ok = false; break; }
        if (ok) {
            board[col] = row;
            _nQueensCount(n, board, col + 1, count, firstSolution);
            board[col] = -1;
        }
    }
}

/* ══════════════════════════════════════════════════════════════
   _execAlgoritmo — dispatcher del tool "algoritmo" (D-33, prima parte).
   Stessi algoritmi già collegati alle guardie regex a frase fissa da
   _inject_algo (D-17), qui richiamabili dal MODELLO con argomenti
   strutturati (JSON): copre formulazioni libere/multi-turno che i regex
   non intercettano ("qual è l'MCD di questi due numeri che ti ho appena
   dato?", componibile con altri tool). Stessi limiti sui parametri delle
   guardie regex (es. n<=80 per Fibonacci) per restare coerenti.
   ══════════════════════════════════════════════════════════════ */
QString _execAlgoritmo(const QString& nome, const QJsonObject& p)
{
    const QString n = nome.toLower().trimmed();

    if (n == "mcd" || n == "gcd") {
        const int a = p.value("a").toInt(), b = p.value("b").toInt();
        const auto steps = SimulatorePage::genGCD(a, b);
        if (steps.isEmpty() || steps.last().arr.isEmpty())
            return "errore: parametri 'a'/'b' mancanti o non validi";
        return QString("MCD(%1,%2) = %3").arg(a).arg(b).arg(steps.last().arr[0]);
    }
    if (n == "mcm" || n == "lcm") {
        const int a = p.value("a").toInt(), b = p.value("b").toInt();
        const auto steps = SimulatorePage::genGCD(a, b);
        if (steps.isEmpty() || steps.last().arr.isEmpty() || steps.last().arr[0] <= 0)
            return "errore: parametri 'a'/'b' mancanti o non validi";
        const int g = steps.last().arr[0];
        const qint64 lcm = static_cast<qint64>(a) / g * b;
        return QString("MCM(%1,%2) = %3 (via MCD=%4)").arg(a).arg(b).arg(lcm).arg(g);
    }
    if (n == "fattorizzazione" || n == "fattorizza") {
        const int v = p.value("n").toInt();
        if (v < 2 || v > 1000000000) return "errore: 'n' deve essere tra 2 e 1000000000";
        const auto steps = SimulatorePage::genPrimeFactors(v);
        if (steps.isEmpty()) return "errore: fattorizzazione fallita";
        return steps.last().msg;
    }
    if (n == "pascal") {
        const int riga = p.value("n").toInt();
        if (riga < 0 || riga > 7) return "errore: 'n' (riga) deve essere tra 0 e 7";
        const auto steps = SimulatorePage::genPascalTriangle(riga + 1);
        if (steps.isEmpty()) return "errore: generazione fallita";
        QStringList vals;
        for (int v2 : steps.last().arr) vals << QString::number(v2);
        return QString("Triangolo di Pascal riga %1 = %2").arg(riga).arg(vals.join(", "));
    }
    if (n == "fibonacci") {
        const int v = p.value("n").toInt();
        if (v < 1 || v > 80) return "errore: 'n' deve essere tra 1 e 80";
        if (v <= 2) return QString("F(%1) = 1 (sequenza 1,1,2,3,5,8,13,21,...)").arg(v);
        const auto steps = SimulatorePage::genFibonacciDP(v);
        if (steps.isEmpty()) return "errore: calcolo fallito";
        return steps.last().msg;
    }
    if (n == "catalan") {
        const int v = p.value("n").toInt();
        if (v < 0 || v > 10) return "errore: 'n' deve essere tra 0 e 10";
        const auto steps = SimulatorePage::genCatalan(v);
        if (steps.isEmpty()) return "errore: calcolo fallito";
        return steps.last().msg;
    }
    if (n == "collatz") {
        const int v = p.value("n").toInt();
        if (v < 1 || v > 1000000) return "errore: 'n' deve essere tra 1 e 1000000";
        const auto steps = SimulatorePage::genCollatz(v);
        if (steps.isEmpty()) return "errore: calcolo fallito";
        return QString("Collatz(%1) \xe2\x80\x94 %2").arg(v).arg(steps.last().msg);
    }
    if (n == "hanoi") {
        /* Formula chiusa 2^n-1, non genTowerOfHanoi() che genera un passo
         * per OGNI mossa via ricorsione — con n grande esploderebbe
         * (stesso motivo per cui _inject_algo non la usa). */
        const int v = p.value("n").toInt();
        if (v < 1 || v > 60) return "errore: 'n' (dischi) deve essere tra 1 e 60";
        const qint64 moves = (1LL << v) - 1;
        return QString("Torre di Hanoi con %1 dischi \xe2\x86\x92 %2 mosse minime (2^%1 - 1)").arg(v).arg(moves);
    }
    if (n == "hanoi_passi" || n == "hanoi_steps") {
        /* Elenco mosse reale (D-33 punto 8) — a differenza di "hanoi" sopra
         * (solo conteggio via formula chiusa), qui si riusa DAVVERO
         * `SimulatorePage::genTowerOfHanoi()` (resa static+public sopra) per
         * ottenere la sequenza passo-passo. Limite N<=10 (1023 mosse) per
         * restare un output leggibile in chat — oltre, usare "hanoi". */
        const int discs = p.value("n").toInt();
        if (discs < 1 || discs > 10)
            return "errore: 'n' (dischi) deve essere tra 1 e 10 per l'elenco passo-passo "
                   "(oltre 10 usa l'algoritmo 'hanoi' per il solo conteggio mosse)";
        const auto steps = SimulatorePage::genTowerOfHanoi(discs);
        /* Primo step = intestazione, ultimo = riepilogo finale: le mosse
         * vere sono quelle centrali. */
        if (steps.size() < 3) return "errore: generazione fallita";
        QStringList moves;
        for (int i = 1; i < steps.size() - 1; ++i) moves << steps[i].msg;
        return QString("Torre di Hanoi con %1 dischi \xe2\x80\x94 %2 mosse:\n%3")
            .arg(discs).arg(moves.size()).arg(_truncateResult(moves.join("\n"), 1500));
    }
    if (n == "nqueens" || n == "n_regine" || n == "n-regine") {
        const int size = p.value("n").toInt();
        if (size < 1 || size > 12)
            return "errore: 'n' deve essere tra 1 e 12 (oltre 12 il backtracking "
                   "diventa troppo lento per una risposta immediata)";
        QVector<int> board(size, -1);
        qint64 count = 0;
        QVector<int> firstSolution;
        _nQueensCount(size, board, 0, count, firstSolution);
        if (count == 0)
            return QString("N-Queens %1x%1: nessuna soluzione esiste per N=%2 "
                           "(N=2 e N=3 non hanno soluzione)").arg(size).arg(size);
        QStringList rows;
        for (int r : firstSolution) rows << QString::number(r);
        return QString("N-Queens %1x%1: %2 soluzioni totali (conteggio esatto via "
                       "backtracking, non una tabella). Esempio di soluzione "
                       "(riga della regina per ogni colonna 0..%3): %4")
            .arg(size).arg(count).arg(size - 1).arg(rows.join(", "));
    }
    if (n == "profitto_azioni" || n == "stock_profit") {
        QVector<int> prezzi;
        for (const QString& tok : p.value("array").toString().split(QRegularExpression(R"([,\s]+)"), Qt::SkipEmptyParts))
            prezzi << tok.toInt();
        if (prezzi.size() < 2 || prezzi.size() > 200)
            return "errore: 'array' deve avere tra 2 e 200 prezzi separati da virgola";
        const auto steps = SimulatorePage::genStockProfit(prezzi);
        if (steps.isEmpty()) return "errore: calcolo fallito";
        return steps.last().msg;
    }
    if (n == "inversioni") {
        QVector<int> arr;
        for (const QString& tok : p.value("array").toString().split(QRegularExpression(R"([,\s]+)"), Qt::SkipEmptyParts))
            arr << tok.toInt();
        if (arr.size() < 2 || arr.size() > 500)
            return "errore: 'array' deve avere tra 2 e 500 numeri separati da virgola";
        const auto steps = SimulatorePage::genCountInversions(arr);
        if (steps.isEmpty()) return "errore: calcolo fallito";
        return steps.last().msg;
    }
    if (n == "posizione_array" || n == "ricerca_lineare") {
        const int target = p.value("target").toInt();
        QVector<int> arr;
        for (const QString& tok : p.value("array").toString().split(QRegularExpression(R"([,\s]+)"), Qt::SkipEmptyParts))
            arr << tok.toInt();
        if (arr.isEmpty() || arr.size() > 1000) return "errore: 'array' non valido (max 1000 elementi)";
        const auto steps = SimulatorePage::genLinearSearch(arr, target);
        if (steps.isEmpty()) return "errore: calcolo fallito";
        return steps.last().msg;
    }
    if (n == "edit_distance" || n == "distanza_edit") {
        const QString s1 = p.value("s1").toString(), s2 = p.value("s2").toString();
        if (s1.isEmpty() || s2.isEmpty()) return "errore: 's1' e 's2' sono obbligatori";
        return QString("distanza di edit tra \"%1\" e \"%2\" = %3")
            .arg(s1, s2).arg(_editDistance(s1, s2));
    }
    if (n == "lcs" || n == "sottosequenza_comune") {
        const QString s1 = p.value("s1").toString(), s2 = p.value("s2").toString();
        if (s1.isEmpty() || s2.isEmpty()) return "errore: 's1' e 's2' sono obbligatori";
        const QString lcs = _longestCommonSubsequence(s1, s2);
        return QString("sottosequenza comune pi\xc3\xb9 lunga tra \"%1\" e \"%2\" = \"%3\" (lunghezza %4)")
            .arg(s1, s2, lcs).arg(lcs.length());
    }
    if (n == "kmp" || n == "ricerca_pattern") {
        const QString pattern = p.value("pattern").toString(), testo = p.value("testo").toString();
        if (pattern.isEmpty() || testo.isEmpty()) return "errore: 'pattern' e 'testo' sono obbligatori";
        const auto steps = SimulatorePage::genKMP(pattern, testo);
        if (steps.isEmpty()) return "errore: ricerca fallita";
        return steps.last().msg;
    }

    return QString("errore: algoritmo '%1' non riconosciuto. Disponibili: mcd, mcm, "
                   "fattorizzazione, pascal, fibonacci, catalan, collatz, hanoi, "
                   "hanoi_passi, nqueens, profitto_azioni, inversioni, posizione_array, "
                   "edit_distance, lcs, kmp")
        .arg(nome);
}

/* ══════════════════════════════════════════════════════════════
   _inject_algo — riusa gli algoritmi già scritti (e testati) del
   Simulatore Algoritmi (main_simulator.h, resi static perché sono
   funzioni pure — non toccano mai membri di SimulatorePage) per
   rispondere a domande matematiche/algoritmiche classiche senza
   passare dall'LLM. Selezione curata: solo gli algoritmi con una
   domanda naturale a risposta unica (non le 19 varianti di
   ordinamento — rispondono tutte alla stessa domanda banale
   "ordina questa lista", il loro valore è il procedimento
   passo-passo già mostrato dal Simulatore visivo — né gli
   algoritmi su grafo, che richiedono un intero grafo come input
   strutturato). Primalità/somme/potenze/fattoriale sono già
   coperti da guardiaMath() — non duplicati qui.
   ══════════════════════════════════════════════════════════════ */
QString _inject_algo(const QString& task)
{
    const QString lo = task.toLower().trimmed();
    if (lo.length() > 300) return task;

    /* ── MCD / MCM ── */
    {
        static const QRegularExpression reMCD(
            R"((?:mcd|massimo\s+comun\s+divisore)\s+(?:tra|fra|di)\s+(\d+)\s+(?:e|,)\s+(\d+))",
            QRegularExpression::CaseInsensitiveOption);
        const auto m = reMCD.match(lo);
        if (m.hasMatch()) {
            const int a = m.captured(1).toInt(), b = m.captured(2).toInt();
            const auto steps = SimulatorePage::genGCD(a, b);
            if (!steps.isEmpty() && !steps.last().arr.isEmpty())
                return QString("[Calcolo locale: MCD(%1,%2) = %3]\n\n")
                    .arg(a).arg(b).arg(steps.last().arr[0]) + task;
        }
        static const QRegularExpression reMCM(
            R"((?:mcm|minimo\s+comune\s+multiplo)\s+(?:tra|fra|di)\s+(\d+)\s+(?:e|,)\s+(\d+))",
            QRegularExpression::CaseInsensitiveOption);
        const auto m2 = reMCM.match(lo);
        if (m2.hasMatch()) {
            const int a = m2.captured(1).toInt(), b = m2.captured(2).toInt();
            const auto steps = SimulatorePage::genGCD(a, b);
            if (!steps.isEmpty() && !steps.last().arr.isEmpty() && steps.last().arr[0] > 0) {
                const int g = steps.last().arr[0];
                const qint64 lcm = static_cast<qint64>(a) / g * b;
                return QString("[Calcolo locale: MCM(%1,%2) = %3 (via MCD=%4)]\n\n")
                    .arg(a).arg(b).arg(lcm).arg(g) + task;
            }
        }
    }

    /* ── Fattorizzazione in fattori primi ── */
    {
        static const QRegularExpression re(
            R"((?:fattorizzazione\s+(?:di|del\s+numero)?|scomponi\s+in\s+fattori\s+primi(?:\s+di)?|fattori\s+primi\s+di)\s*(\d+))",
            QRegularExpression::CaseInsensitiveOption);
        const auto m = re.match(lo);
        if (m.hasMatch()) {
            const int n = m.captured(1).toInt();
            if (n >= 2 && n <= 1000000000) {
                const auto steps = SimulatorePage::genPrimeFactors(n);
                if (!steps.isEmpty())
                    return QString("[Calcolo locale: %1]\n\n").arg(steps.last().msg) + task;
            }
        }
    }

    /* ── Triangolo di Pascal, riga N ── */
    {
        static const QRegularExpression re(
            R"(triangolo\s+di\s+pascal.{0,10}?riga\s+(\d+)|riga\s+(\d+).{0,15}?triangolo\s+di\s+pascal)",
            QRegularExpression::CaseInsensitiveOption);
        const auto m = re.match(lo);
        if (m.hasMatch()) {
            const int n = (!m.captured(1).isEmpty() ? m.captured(1) : m.captured(2)).toInt();
            if (n >= 0 && n <= 7) {
                const auto steps = SimulatorePage::genPascalTriangle(n + 1);
                if (!steps.isEmpty()) {
                    QStringList vals;
                    for (int v : steps.last().arr) vals << QString::number(v);
                    return QString("[Calcolo locale: Triangolo di Pascal riga %1 = %2]\n\n")
                        .arg(n).arg(vals.join(", ")) + task;
                }
            }
        }
    }

    /* ── N-esimo numero di Fibonacci ──
     * Bug reale trovato verificando T-D17 (TODO.md): la frase-esempio del
     * TODO stesso, "decimo numero di fibonacci", non veniva MAI riconosciuta
     * — la regex accettava solo la cifra ("10 numero di fibonacci"), non
     * l'ordinale scritto a parole. Aggiunto un controllo separato PRIMA
     * della regex numerica: se il token subito prima di "numero di
     * fibonacci" è un ordinale noto (primo..ventesimo), usa quello; altrimenti
     * prova la regex esistente invariata. */
    {
        static const QMap<QString,int> kOrdinali = {
            {"primo",1},{"secondo",2},{"terzo",3},{"quarto",4},{"quinto",5},
            {"sesto",6},{"settimo",7},{"ottavo",8},{"nono",9},{"decimo",10},
            {"undicesimo",11},{"dodicesimo",12},{"tredicesimo",13},{"quattordicesimo",14},
            {"quindicesimo",15},{"sedicesimo",16},{"diciassettesimo",17},{"diciottesimo",18},
            {"diciannovesimo",19},{"ventesimo",20},
        };
        int n = -1;
        static const QRegularExpression reOrd(
            R"((\w+)\s+numero\s+di\s+fibonacci)", QRegularExpression::CaseInsensitiveOption);
        const auto mOrd = reOrd.match(lo);
        if (mOrd.hasMatch() && kOrdinali.contains(mOrd.captured(1)))
            n = kOrdinali.value(mOrd.captured(1));
        if (n < 0) {
            static const QRegularExpression re(
                R"((\d+)\s*\xc2\xb0?\s*numero\s+di\s+fibonacci|fibonacci\s+(?:di|numero)\s+(\d+))",
                QRegularExpression::CaseInsensitiveOption);
            const auto m = re.match(lo);
            if (m.hasMatch())
                n = (!m.captured(1).isEmpty() ? m.captured(1) : m.captured(2)).toInt();
        }
        if (n >= 1 && n <= 80) {
            if (n <= 2) {
                return QString("[Calcolo locale: F(%1) = 1 (sequenza 1,1,2,3,5,8,13,21,...)]\n\n").arg(n) + task;
            }
            const auto steps = SimulatorePage::genFibonacciDP(n);
            if (!steps.isEmpty())
                return QString("[Calcolo locale: %1]\n\n").arg(steps.last().msg) + task;
        }
    }

    /* ── N-esimo numero di Catalan ── */
    {
        static const QRegularExpression re(
            R"((?:numero\s+di\s+catalan|catalan)\s*(?:di|numero)?\s*(\d+))",
            QRegularExpression::CaseInsensitiveOption);
        const auto m = re.match(lo);
        if (m.hasMatch()) {
            const int n = m.captured(1).toInt();
            if (n >= 0 && n <= 10) {
                const auto steps = SimulatorePage::genCatalan(n);
                if (!steps.isEmpty())
                    return QString("[Calcolo locale: %1]\n\n").arg(steps.last().msg) + task;
            }
        }
    }

    /* ── Congettura di Collatz: quanti passi per arrivare a 1 ── */
    {
        static const QRegularExpression re(
            R"(collatz.{0,15}?(\d+)|(\d+).{0,15}?collatz)",
            QRegularExpression::CaseInsensitiveOption);
        const auto m = re.match(lo);
        if (m.hasMatch()) {
            const int n = (!m.captured(1).isEmpty() ? m.captured(1) : m.captured(2)).toInt();
            if (n >= 1 && n <= 1000000) {
                const auto steps = SimulatorePage::genCollatz(n);
                if (!steps.isEmpty())
                    return QString("[Calcolo locale: Collatz(%1) \xe2\x80\x94 %2]\n\n")
                        .arg(n).arg(steps.last().msg) + task;
            }
        }
    }

    /* ── Torre di Hanoi: mosse minime (formula chiusa 2^n-1, non usa
     * SimulatorePage::genTowerOfHanoi() che genera un passo per OGNI
     * mossa via ricorsione — con n grande esploderebbe in memoria/tempo
     * solo per estrarne il conteggio finale). ── */
    {
        static const QRegularExpression re(
            R"(torr[ei]\s+di\s+hanoi.{0,25}?(\d+)\s*disch)",
            QRegularExpression::CaseInsensitiveOption);
        const auto m = re.match(lo);
        if (m.hasMatch()) {
            const int n = m.captured(1).toInt();
            if (n >= 1 && n <= 60) {
                const qint64 moves = (1LL << n) - 1;
                return QString("[Calcolo locale: Torre di Hanoi con %1 dischi \xe2\x86\x92 %2 mosse minime (2^%1 - 1)]\n\n")
                    .arg(n).arg(moves) + task;
            }
        }
    }

    /* ── Profitto massimo comprando/vendendo azioni ── */
    {
        static const QRegularExpression re(
            R"((?:profitto\s+massimo|massimo\s+profitto).{0,30}?((?:\-?\d+[\s,]+)+\-?\d+))",
            QRegularExpression::CaseInsensitiveOption);
        const auto m = re.match(lo);
        if (m.hasMatch()) {
            QVector<int> prezzi;
            for (const QString& tok : m.captured(1).split(QRegularExpression(R"([,\s]+)"), Qt::SkipEmptyParts))
                prezzi << tok.toInt();
            if (prezzi.size() >= 2 && prezzi.size() <= 200) {
                const auto steps = SimulatorePage::genStockProfit(prezzi);
                if (!steps.isEmpty())
                    return QString("[Calcolo locale: %1]\n\n").arg(steps.last().msg) + task;
            }
        }
    }

    /* ── Conteggio inversioni in una lista ── */
    {
        static const QRegularExpression re(
            R"(inversioni.{0,25}?((?:\-?\d+[\s,]+)+\-?\d+))",
            QRegularExpression::CaseInsensitiveOption);
        const auto m = re.match(lo);
        if (m.hasMatch()) {
            QVector<int> arr;
            for (const QString& tok : m.captured(1).split(QRegularExpression(R"([,\s]+)"), Qt::SkipEmptyParts))
                arr << tok.toInt();
            if (arr.size() >= 2 && arr.size() <= 500) {
                const auto steps = SimulatorePage::genCountInversions(arr);
                if (!steps.isEmpty())
                    return QString("[Calcolo locale: %1]\n\n").arg(steps.last().msg) + task;
            }
        }
    }

    /* ── Posizione di un elemento in un array ──
     * Bug reale trovato verificando T-D17 (TODO.md): "in che posizione è 7
     * in 3,7,9,2" non veniva MAI riconosciuta. Causa: `\xc3\xa8`/`\xe2\x80\x99`
     * scritti dentro un raw string R"(...)" — il compilatore C++ NON
     * processa gli escape dentro i raw string, quindi la stringa risultante
     * contiene i caratteri letterali '\','x','c','3',... Il motore PCRE
     * (QRegularExpression) li reinterpreta poi come DUE caratteri Latin-1
     * separati (\xc3='Ã', \xa8='¨') invece del singolo carattere UTF-8 'è'
     * inteso — l'alternativa non può mai combaciare con una vera 'è'
     * digitata dall'utente. Fix: caratteri Unicode letterali diretti nel
     * raw string (stesso pattern già usato correttamente altrove in questo
     * file, es. `[èe]` nella regex del fuso orario). */
    {
        static const QRegularExpression re(
            R"((?:in\s+che\s+posizione\s+(?:si\s+trova|è|e)|trova|indice\s+di)\s+(\-?\d+)\s+(?:in|nell['’a]?\s*array|nella\s+lista)\s*[:\[]?\s*((?:\-?\d+[\s,]+)+\-?\d+)\]?)",
            QRegularExpression::CaseInsensitiveOption);
        const auto m = re.match(lo);
        if (m.hasMatch()) {
            const int target = m.captured(1).toInt();
            QVector<int> arr;
            for (const QString& tok : m.captured(2).split(QRegularExpression(R"([,\s]+)"), Qt::SkipEmptyParts))
                arr << tok.toInt();
            if (!arr.isEmpty() && arr.size() <= 1000) {
                const auto steps = SimulatorePage::genLinearSearch(arr, target);
                if (!steps.isEmpty())
                    return QString("[Calcolo locale: %1]\n\n").arg(steps.last().msg) + task;
            }
        }
    }

    /* ── Edit distance / sottosequenza comune più lunga tra due stringhe
     * tra virgolette. Usa `task` (non `lo`) per preservare le maiuscole
     * nelle stringhe originali. ── */
    {
        static const QRegularExpression reStrings(R"(['\"]([^'\"]{1,50})['\"].{0,15}?['\"]([^'\"]{1,50})['\"])");
        if (lo.contains("edit distance") || lo.contains("distanza di edit") || lo.contains("distanza di modifica")) {
            const auto m = reStrings.match(task);
            if (m.hasMatch()) {
                const QString s1 = m.captured(1), s2 = m.captured(2);
                return QString("[Calcolo locale: distanza di edit tra \"%1\" e \"%2\" = %3]\n\n")
                    .arg(s1, s2).arg(_editDistance(s1, s2)) + task;
            }
        }
        if (lo.contains("sottosequenza comune") || lo.contains(" lcs ") || lo.startsWith("lcs")) {
            const auto m = reStrings.match(task);
            if (m.hasMatch()) {
                const QString s1 = m.captured(1), s2 = m.captured(2);
                const QString lcs = _longestCommonSubsequence(s1, s2);
                return QString("[Calcolo locale: sottosequenza comune pi\xc3\xb9 lunga tra \"%1\" e \"%2\" = \"%3\" (lunghezza %4)]\n\n")
                    .arg(s1, s2, lcs).arg(lcs.length()) + task;
            }
        }
    }

    /* ── Ricerca di un pattern testuale in un testo (KMP) ── */
    {
        if ((lo.contains("cerca") || lo.contains("trova"))
            && (lo.contains("pattern") || lo.contains("nel testo") || lo.contains("nella stringa"))) {
            static const QRegularExpression reStrings(R"(['\"]([^'\"]{1,50})['\"].{0,25}?['\"]([^'\"]{1,300})['\"])");
            const auto m = reStrings.match(task);
            if (m.hasMatch()) {
                const QString pattern = m.captured(1), text = m.captured(2);
                if (!pattern.isEmpty()) {
                    const auto steps = SimulatorePage::genKMP(pattern, text);
                    if (!steps.isEmpty())
                        return QString("[Calcolo locale: %1]\n\n").arg(steps.last().msg) + task;
                }
            }
        }
    }

    return task;
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
