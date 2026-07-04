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

/* Dispatcher del tool "algoritmo" (D-33) — definito più sotto, vicino agli
 * helper _editDistance/_longestCommonSubsequence e a _inject_algo che
 * espone già gli stessi algoritmi via regex (D-17). */
static QString _execAlgoritmo(const QString& nome, const QJsonObject& p);

/* Helper di validazione (IBAN mod-97, Codice Fiscale D.M.1976) — definiti
 * più sotto vicino a _inject_finance (D-17/D-14) che li usa già, riusati
 * anche dal tool "valida_documento" (D-33) qui in cima al file. */
static bool    _ibanValid(const QString& ibanRaw);
static bool    _cfChecksumValid(const QString& cfUpper);
static QString _cfDecode(const QString& cfUpper);

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

    /* ── Algoritmi classici (D-33) — stessi algoritmi già esposti come
     * guardia regex da _inject_algo (D-17), ma richiamabili dal MODELLO
     * con argomenti strutturati: copre le formulazioni libere/multi-turno
     * che i regex a frase fissa non intercettano. ── */
    if (tool == "algoritmo" || tool == "algorithm") {
        const QJsonObject o = QJsonDocument::fromJson(input.toUtf8()).object();
        const QString nome = o.value("nome").toString().trimmed();
        if (nome.isEmpty()) { onDone("errore: campo 'nome' obbligatorio"); return; }
        onDone(_execAlgoritmo(nome, o));
        return;
    }

    /* ── Codice Fiscale (D-33, parte 2) — riusa PraticoCalcs::calcolaCodiceFiscale()
     * (D.M. 23/12/1976) e PraticoCalcs::cercaBelfiore(), stesso algoritmo già in
     * produzione nella Scheda TFR (main_finance.cpp) e nell'auto-fill RAG che usa
     * lo stesso schema JSON (nome/cognome/data_nascita "yyyy-MM-dd"/sesso/
     * comune_nascita) — qui esposto anche al modello come tool a sé stante. ── */
    if (tool == "codice_fiscale" || tool == "calcola_codice_fiscale") {
        const QJsonObject o = QJsonDocument::fromJson(input.toUtf8()).object();
        const QString cognome = o.value("cognome").toString().trimmed();
        const QString nome    = o.value("nome").toString().trimmed();
        const QDate nascita   = QDate::fromString(o.value("data_nascita").toString(), "yyyy-MM-dd");
        const QString sessoS  = o.value("sesso").toString().trimmed().toUpper();
        QString belfiore      = o.value("codice_belfiore").toString().trimmed().toUpper();
        const QString comune  = o.value("comune_nascita").toString().trimmed();

        if (cognome.isEmpty() || nome.isEmpty()) {
            onDone("errore: 'cognome' e 'nome' sono obbligatori"); return;
        }
        if (!nascita.isValid()) {
            onDone("errore: 'data_nascita' non valida (usa il formato AAAA-MM-GG)"); return;
        }
        if (sessoS != "M" && sessoS != "F") {
            onDone("errore: 'sesso' deve essere 'M' o 'F'"); return;
        }
        if (belfiore.length() != 4) {
            if (comune.isEmpty()) {
                onDone("errore: serve 'comune_nascita' oppure 'codice_belfiore' (4 caratteri)");
                return;
            }
            belfiore = PraticoCalcs::cercaBelfiore(comune);
            if (belfiore.isEmpty()) {
                onDone(QString("errore: comune '%1' non trovato nell'elenco Belfiore locale "
                               "— fornisci direttamente 'codice_belfiore'").arg(comune));
                return;
            }
        }
        const QString cf = PraticoCalcs::calcolaCodiceFiscale(
            cognome, nome, nascita, sessoS == "M", belfiore);
        if (cf.isEmpty()) { onDone("errore: dati insufficienti per calcolare il codice fiscale"); return; }
        onDone(QString("Codice Fiscale: %1").arg(cf));
        return;
    }

    /* ── Calcoli finanziari (D-33, parte 3) — stesse formule già in produzione:
     * interesse composto e rata mutuo (ammortamento francese) duplicate da
     * _inject_finance qui sotto (stesso stile "hanoi" in _execAlgoritmo — sono
     * formule chiuse a una riga, non funzioni condivise da richiamare); TFR con
     * rivalutazione duplicata da main_finance.cpp::buildTfrTab() (quota annua =
     * stipendio/13,5, tasso = 1,5% + 75%×inflazione, capitalizzato anno per anno,
     * netto semplificato 77%/88% in azienda/fondo pensione, stessa approssimazione
     * già mostrata nella Scheda TFR). 730/regime forfettario NON inclusi: nel
     * codice attuale il forfettario è solo una persona di chat LLM (righe ~996) e
     * il 730 non esiste come calcolo — servirebbe scrivere ex novo la logica
     * degli scaglioni IRPEF, fuori dallo scopo di D-33 ("esporre codice già
     * scritto", non produrre nuova logica fiscale non verificata). ── */
    if (tool == "finanza_calcola") {
        const QJsonObject o = QJsonDocument::fromJson(input.toUtf8()).object();
        const QString tipo = o.value("tipo").toString().trimmed().toLower();
        const int anni = o.value("anni").toInt();
        if (anni <= 0) { onDone("errore: 'anni' deve essere positivo"); return; }

        if (tipo == "interesse_composto") {
            const double capitale = o.value("capitale").toDouble();
            const double tasso = o.value("tasso_annuo_pct").toDouble();
            if (capitale <= 0) { onDone("errore: 'capitale' deve essere positivo"); return; }
            if (anni > 200) { onDone("errore: 'anni' troppo elevato (max 200)"); return; }
            const double montante = capitale * std::pow(1.0 + tasso / 100.0, anni);
            onDone(QString("%1 \xe2\x82\xac al %2%% annuo (interesse composto) per %3 anni "
                           "\xe2\x86\x92 %4 \xe2\x82\xac (interessi maturati: %5 \xe2\x82\xac)")
                .arg(QString::number(capitale, 'f', 2), QString::number(tasso, 'g', 6))
                .arg(anni)
                .arg(QString::number(montante, 'f', 2), QString::number(montante - capitale, 'f', 2)));
            return;
        }
        if (tipo == "rata_mutuo") {
            const double capitale = o.value("capitale").toDouble();
            const double tassoAnn = o.value("tasso_annuo_pct").toDouble();
            if (capitale <= 0) { onDone("errore: 'capitale' deve essere positivo"); return; }
            if (anni > 60) { onDone("errore: 'anni' troppo elevato per un mutuo (max 60)"); return; }
            const double i = tassoAnn / 100.0 / 12.0;
            const int n = anni * 12;
            const double rata = (i > 0) ? capitale * i / (1.0 - std::pow(1.0 + i, -n)) : capitale / n;
            const double totalePagato = rata * n;
            onDone(QString("mutuo di %1 \xe2\x82\xac al %2%% annuo in %3 anni "
                           "(ammortamento francese, rata costante) \xe2\x86\x92 rata mensile %4 \xe2\x82\xac, "
                           "totale pagato %5 \xe2\x82\xac (interessi %6 \xe2\x82\xac)")
                .arg(QString::number(capitale, 'f', 2), QString::number(tassoAnn, 'g', 6))
                .arg(anni)
                .arg(QString::number(rata, 'f', 2), QString::number(totalePagato, 'f', 2),
                     QString::number(totalePagato - capitale, 'f', 2)));
            return;
        }
        if (tipo == "tfr_rivalutazione") {
            const double stip = o.value("stipendio_lordo_annuo").toDouble();
            const double infl = o.value("inflazione_pct").toDouble() / 100.0;
            if (stip <= 0) { onDone("errore: 'stipendio_lordo_annuo' deve essere positivo"); return; }
            if (anni > 50) { onDone("errore: 'anni' troppo elevato (max 50)"); return; }
            const double quotaAnnua = stip / 13.5;
            const double tassoRival = 0.015 + 0.75 * infl;
            double fondo = 0.0;
            for (int y = 1; y <= anni; ++y) { fondo += quotaAnnua; fondo *= (1.0 + tassoRival); }
            const double tfrSemplice = quotaAnnua * anni;
            onDone(QString("TFR su %1 anni, stipendio lordo annuo %2 \xe2\x82\xac, inflazione %3%% "
                           "\xe2\x80\x94 quota annua %4 \xe2\x82\xac, tasso rivalutazione %5%% "
                           "\xe2\x86\x92 TFR rivalutato %6 \xe2\x82\xac (senza rivalutazione %7 \xe2\x82\xac). "
                           "Netto stimato: %8 \xe2\x82\xac in azienda (tassazione separata ~23%%) o "
                           "%9 \xe2\x82\xac in fondo pensione (imposta sostitutiva 15%%, calcolo indicativo art. 2120 c.c.)")
                .arg(anni)
                .arg(QString::number(stip, 'f', 0), QString::number(infl * 100.0, 'f', 1))
                .arg(QString::number(quotaAnnua, 'f', 2), QString::number(tassoRival * 100.0, 'f', 2))
                .arg(QString::number(fondo, 'f', 2), QString::number(tfrSemplice, 'f', 2))
                .arg(QString::number(fondo * 0.77, 'f', 2), QString::number(fondo * 0.88, 'f', 2)));
            return;
        }
        onDone("errore: 'tipo' deve essere 'interesse_composto', 'rata_mutuo' o 'tfr_rivalutazione'");
        return;
    }

    /* ── Validazione documenti (D-33, parte 4) — stessi validatori già usati
     * dalla guardia regex `_inject_finance` qui sotto (`_ibanValid`,
     * `_cfChecksumValid`, `_cfDecode`, tutte definite più sotto in questo
     * file), più il checksum P.IVA (duplicato dalla stessa guardia, 5 righe,
     * non estratto in funzione condivisa). ── */
    if (tool == "valida_documento") {
        const QJsonObject o = QJsonDocument::fromJson(input.toUtf8()).object();
        const QString tipo   = o.value("tipo").toString().trimmed().toLower();
        const QString valore = o.value("valore").toString().trimmed();
        if (valore.isEmpty()) { onDone("errore: 'valore' obbligatorio"); return; }

        if (tipo == "iban") {
            QString clean = valore; clean.remove(' '); clean = clean.toUpper();
            if (clean.length() < 15) { onDone("errore: IBAN troppo corto"); return; }
            const bool valid = _ibanValid(clean);
            onDone(QString("IBAN %1 \xe2\x80\x94 %2").arg(clean,
                valid ? "VALIDO (cifra di controllo corretta)"
                      : "NON VALIDO (cifra di controllo errata o formato non riconosciuto)"));
            return;
        }
        if (tipo == "codice_fiscale") {
            const QString cf = valore.toUpper().remove(' ');
            static const QRegularExpression reCf(
                R"(^[A-Z]{6}\d{2}[A-Z]\d{2}[A-Z]\d{3}[A-Z]$)");
            if (!reCf.match(cf).hasMatch()) {
                onDone(QString("Codice Fiscale %1 \xe2\x80\x94 formato non riconosciuto "
                               "(attese 16 posizioni: 6 lettere, 2 cifre, 1 lettera, 2 cifre, "
                               "1 lettera, 3 cifre, 1 lettera)").arg(cf));
                return;
            }
            const bool valid = _cfChecksumValid(cf);
            QString msg = QString("Codice Fiscale %1 \xe2\x80\x94 %2")
                .arg(cf, valid ? "VALIDO" : "NON VALIDO (cifra di controllo errata)");
            if (valid) {
                const QString decoded = _cfDecode(cf);
                if (!decoded.isEmpty()) msg += ", " + decoded;
            }
            onDone(msg);
            return;
        }
        if (tipo == "partita_iva") {
            const QString piva = QString(valore).remove(' ');
            static const QRegularExpression reNum(R"(^\d{11}$)");
            if (!reNum.match(piva).hasMatch()) {
                onDone("errore: la Partita IVA deve avere esattamente 11 cifre"); return;
            }
            int sum = 0;
            for (int i = 0; i < 10; ++i) {
                int d = piva.at(i).digitValue();
                if (i % 2 == 1) { d *= 2; if (d > 9) d -= 9; }
                sum += d;
            }
            const int expected = (10 - (sum % 10)) % 10;
            const bool valid = expected == piva.at(10).digitValue();
            onDone(QString("Partita IVA %1 \xe2\x80\x94 %2").arg(piva,
                valid ? "VALIDA (cifra di controllo corretta)" : "NON VALIDA (cifra di controllo errata)"));
            return;
        }
        onDone("errore: 'tipo' deve essere 'iban', 'partita_iva' o 'codice_fiscale'");
        return;
    }

    /* ── Carta astrale (D-33, parte 5) — riusa `AstroCalc::compute()` (Meeus,
     * `widgets/astro_calc.h`), lo stesso motore astronomico già in produzione
     * nel tab Ricerca → Carta Astrale. Formattazione posizione→segno identica
     * a `lonToSign()` di `main_research_astrale.cpp` (duplicata: è una lambda
     * di 4 righe, non una funzione condivisa). Restituisce solo pianeti/
     * ASC/MC (non le 12 case Placidus/aspetti, per restare un risultato
     * compatto da tool — l'analisi completa resta nel tab dedicato). ── */
    if (tool == "carta_astrale") {
        const QJsonObject o = QJsonDocument::fromJson(input.toUtf8()).object();
        const QDate data = QDate::fromString(o.value("data").toString(), "yyyy-MM-dd");
        const QTime ora  = QTime::fromString(o.value("ora").toString(), "HH:mm");
        const double lat = o.value("lat").toDouble();
        const double lon = o.value("lon").toDouble();

        if (!data.isValid()) { onDone("errore: 'data' non valida (usa il formato AAAA-MM-GG)"); return; }
        if (!ora.isValid())  { onDone("errore: 'ora' non valida (usa il formato HH:MM, 24 ore)"); return; }
        if (lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0) {
            onDone("errore: 'lat' deve essere tra -90/90 e 'lon' tra -180/180"); return;
        }

        const auto res = AstroCalc::compute(data.year(), data.month(), data.day(),
                                             ora.hour(), ora.minute(), lat, lon);
        if (!res.ok) {
            onDone(QString("errore calcolo carta astrale: %1")
                   .arg(res.error.isEmpty() ? "dati non validi" : res.error));
            return;
        }

        static const char* kSignNames[] = {
            "Ariete","Toro","Gemelli","Cancro","Leone","Vergine",
            "Bilancia","Scorpione","Sagittario","Capricorno","Acquario","Pesci"
        };
        auto lonToSign = [&](double l) -> QString {
            const int sign = static_cast<int>(l / 30.0) % 12;
            const double deg = std::fmod(l, 30.0);
            return QString("%1\xc2\xb0 %2").arg(static_cast<int>(deg)).arg(kSignNames[sign]);
        };

        QStringList parts;
        for (const auto& pl : res.planets)
            parts << (pl.name + ": " + lonToSign(pl.lon));
        parts << ("Ascendente: " + lonToSign(res.ascLon));
        parts << ("Medio Cielo: " + lonToSign(res.mcLon));
        onDone(parts.join("; "));
        return;
    }

    /* ── Conversioni scienza/cucina (D-33, parte 7) — a differenza degli altri
     * tool di questa sezione, qui NON si duplica/richiama una singola formula:
     * `_inject_science()` (main_ai_math.cpp, dichiarata in main_ai_p.h) copre
     * già decine di conversioni eterogenee (Ohm, velocità, temperatura forno,
     * ml↔grammi per 9 ingredienti, cucchiai/tazze, km/h↔mph, anni luce, UA...)
     * scegliere quali esporre una per una in parametri strutturati moltiplicherebbe
     * lo schema senza motivo: si richiama direttamente la stessa guardia zero-LLM
     * già usata nella catena principale, così il tool eredita ogni conversione
     * che la guardia riconosce oggi (e in futuro) senza bisogno di manutenzione
     * doppia. Utile quando il modello vuole convertire un valore emerso a metà
     * conversazione (multi-turno), non solo dalla frase originale dell'utente
     * (che la guardia intercetta già PRIMA di arrivare al modello). ── */
    if (tool == "converti" || tool == "convert") {
        const QJsonObject o = QJsonDocument::fromJson(input.toUtf8()).object();
        QString richiesta = o.value("richiesta").toString().trimmed();
        if (richiesta.isEmpty()) richiesta = input; /* fallback: input libero non-JSON */
        if (richiesta.isEmpty()) { onDone("errore: 'richiesta' obbligatoria"); return; }

        const QString injected = _inject_science(richiesta);
        static const QString kTag = "[Calcolo locale:";
        if (!injected.startsWith(kTag)) {
            onDone("conversione non riconosciuta — riformula specificando chiaramente le unità "
                   "(es. \"200 ml di farina in grammi\", \"180 gradi forno in fahrenheit\", "
                   "\"100 km/h in mph\")");
            return;
        }
        const int close = injected.indexOf(']');
        const QString result = close > 0
            ? injected.mid(kTag.length(), close - kTag.length()).trimmed()
            : injected.left(injected.indexOf('\n')).trimmed();
        onDone(result);
        return;
    }

    /* ── Disegna grafico (D-33, parte 6) — riusa `tryShowChart()` (già
     * chiamata dalla guardia Grafico in `runPipeline()` e dal Byzantino),
     * costruendo la stessa frase in linguaggio naturale che la guardia
     * riconoscerebbe da sola: `FormulaParser::tryExtract()` Pattern 2
     * ("grafico di ...") + `tryExtractXRange()` Pattern 4 ("per x da A a
     * B"). Si ricostruisce la frase invece di passare formula/xmin/xmax
     * direttamente a `tryShowChart()` perché quella funzione fa sempre e
     * solo parsing testuale — non ha un overload strutturato — e qui il
     * modello passa i tre argomenti già separati. Verifica di successo
     * fatta PRIMA di chiamare `tryShowChart()` (che è void, nessun segnale
     * di esito) per poter rispondere con un messaggio d'errore preciso. ── */
    if (tool == "disegna_grafico" || tool == "grafico") {
        const QJsonObject o = QJsonDocument::fromJson(input.toUtf8()).object();
        const QString formulaRaw = o.value("formula").toString().trimmed();
        if (formulaRaw.isEmpty()) { onDone("errore: 'formula' obbligatoria"); return; }
        const double xmin = o.contains("xmin") ? o.value("xmin").toDouble() : -10.0;
        const double xmax = o.contains("xmax") ? o.value("xmax").toDouble() : 10.0;
        if (!(xmax > xmin)) { onDone("errore: 'xmax' deve essere maggiore di 'xmin'"); return; }

        const QString text = QString("grafico di %1 per x da %2 a %3")
            .arg(formulaRaw).arg(xmin).arg(xmax);

        const QString expr = FormulaParser::tryExtract(text);
        if (expr.isEmpty()) {
            onDone(QString("errore: formula '%1' non riconosciuta").arg(formulaRaw));
            return;
        }
        FormulaParser fp(expr);
        if (!fp.ok() || fp.sample(xmin, xmax, 400).isEmpty()) {
            onDone(QString("errore: formula '%1' non valida o non campionabile nell'intervallo dato").arg(expr));
            return;
        }

        tryShowChart(text);
        onDone(QString("Grafico di %1 aperto nel pannello Grafico (x da %2 a %3).")
            .arg(expr).arg(xmin).arg(xmax));
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

    /* ── Cambio valuta — tasso reale via frankfurter.app (ECB, no API key).
     * Non è calcolabile localmente come date/percentuali: il tasso cambia
     * ogni giorno, serve una vera chiamata di rete — ma zero LLM comunque,
     * il modello non deve indovinare il tasso. Input JSON:
     *   {"importo": 100, "da": "EUR", "a": "USD"}
     * oppure testo libero tipo "100 EUR in USD" (fallback regex). ── */
    if (tool == "cambio_valuta" || tool == "currency" || tool == "converti_valuta") {
        const QJsonObject o = QJsonDocument::fromJson(input.toUtf8()).object();
        double importo = o.contains("importo") ? o.value("importo").toDouble() : 1.0;
        QString da = o.value("da").toString().toUpper().trimmed();
        QString a  = o.value("a").toString().toUpper().trimmed();

        if (da.isEmpty() || a.isEmpty()) {
            static const QRegularExpression re(
                R"((\d+(?:[.,]\d+)?)?\s*([A-Za-z]{3})\s*(?:in|to|verso|->|→)\s*([A-Za-z]{3}))",
                QRegularExpression::CaseInsensitiveOption);
            const auto m = re.match(input);
            if (m.hasMatch()) {
                if (!m.captured(1).isEmpty()) importo = m.captured(1).replace(',', '.').toDouble();
                da = m.captured(2).toUpper();
                a  = m.captured(3).toUpper();
            }
        }
        static const QRegularExpression reCode(R"(^[A-Z]{3}$)");
        if (!reCode.match(da).hasMatch() || !reCode.match(a).hasMatch()) {
            onDone("Servono valuta di partenza e destinazione come codici ISO a 3 lettere "
                   "(es. {\"importo\":100,\"da\":\"EUR\",\"a\":\"USD\"}). Chiedi all'utente quali valute.");
            return;
        }

        QJsonArray _a2; _a2.append(a);
        const QString aJson = QString::fromUtf8(
            QJsonDocument(_a2).toJson(QJsonDocument::Compact)).mid(1).chopped(1);
        const QString script =
            "import urllib.request,json\n"
            "try:\n"
            "    url='https://api.frankfurter.app/latest?amount=" + QString::number(importo, 'f', 4)
                + "&from=" + da + "&to=" + a + "'\n"
            "    req=urllib.request.Request(url,headers={'User-Agent':'Mozilla/5.0'})\n"
            "    with urllib.request.urlopen(req,timeout=7) as r: d=json.load(r)\n"
            "    print(f\"{d['amount']} " + da + " = {d['rates'][" + aJson + "]:.4f} " + a
                + " (tasso " + QDateTime::currentDateTime().toString("yyyy-MM-dd") + ", fonte BCE/frankfurter.app)\")\n"
            "except KeyError: print('ERRORE: codice valuta non riconosciuto (frankfurter.app copre le valute BCE)')\n"
            "except Exception as e: print('ERRORE:',e)\n";
        auto* proc = new QProcess(this);
        proc->setProcessChannelMode(QProcess::MergedChannels);
        connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                this, [proc, onDone](int, QProcess::ExitStatus) {
            const QString out = QString::fromUtf8(proc->readAll()).trimmed();
            proc->deleteLater();
            onDone(out.isEmpty() ? "errore: nessuna risposta dal servizio cambio" : out);
        });
        connect(proc, &QProcess::errorOccurred, this, [proc](QProcess::ProcessError err) {
            if (err == QProcess::FailedToStart)
                qWarning() << "[main_ai_tools] cambio_valuta non avviato:" << proc->program();
        });
        proc->start(P::findPython(), {"-c", script});
        QTimer::singleShot(10000, proc, [proc, onDone]{
            if (proc->state() != QProcess::NotRunning) { proc->kill(); onDone("timeout cambio valuta"); }
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
            .arg(QLocale(QLocale::Italian).toString(local.date(), "dddd"))
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
                    .arg(QLocale(QLocale::Italian).toString(target, "d MMMM yyyy"),
                         fmtVal(-diffSec / uSec, unitWord)));
                return;
            }

            // Calcolo "calendario" per mesi/giorni/settimane, secondi per le altre
            QString result;
            if (unitWord == "mesi" || unitWord == "mese") {
                // monthsTo non esiste in Qt6: calcolo manuale con anno/mese.
                // Se il giorno del mese di "target" è precedente a quello di
                // "oggi" (es. oggi 3 luglio → target 1 dicembre), il conteggio
                // grezzo (mesi interi) supera il target di qualche giorno:
                // va scalato di 1 mese e ricalcolato il resto in giorni,
                // altrimenti si arrotonda per eccesso in modo silenzioso.
                int months = (target.year() - now.date().year()) * 12
                             + (target.month() - now.date().month());
                int remD   = static_cast<int>(now.date().addMonths(months).daysTo(target));
                if (remD < 0) {
                    --months;
                    remD = static_cast<int>(now.date().addMonths(months).daysTo(target));
                }
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
                .arg(QLocale(QLocale::Italian).toString(now.date(), "d MMMM yyyy"),
                     QLocale(QLocale::Italian).toString(target, "d MMMM yyyy"), result));
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

    /* ── crea_evento_calendario — genera QR code per aggiungere un evento
     * al calendario dello smartphone. Input JSON:
     *   {"titolo","data" (YYYY-MM-DD),"ora_inizio" (HH:MM, opz.),
     *    "ora_fine" (HH:MM, opz.),"luogo" (opz.),"descrizione" (opz.),
     *    "formato": "google"|"ics"|"entrambi" (opz., default "google")}
     * Se mancano titolo/data ritorna un messaggio guida (NON un errore
     * secco) così l'LLM capisce di dover chiedere il campo mancante
     * all'utente invece di inventarlo.
     * Ritorna una stringa con prefisso QR_EVENTO_JSON: intercettata
     * direttamente da runPipeline() per inserire l'immagine nella bolla
     * — non si passa dall'LLM per il rendering (stessa logica del motivo
     * per cui _inject_date_calc bypassa l'LLM: niente affidabilità da
     * garantire su un output che deve restare byte-per-byte esatto). ── */
    if (tool == "crea_evento_calendario" || tool == "evento_calendario" || tool == "qr_evento") {
        const QJsonObject o = QJsonDocument::fromJson(input.toUtf8()).object();
        const QString titolo = o.value("titolo").toString().trimmed();
        const QString dataS  = o.value("data").toString().trimmed();
        if (titolo.isEmpty() || dataS.isEmpty()) {
            onDone("Mancano dati per creare l'evento: servono almeno 'titolo' e 'data' "
                   "(formato YYYY-MM-DD). Chiedi all'utente i campi mancanti prima di "
                   "richiamare questo tool — titolo, data, ora inizio, ora fine (opz.), "
                   "luogo (opz.), descrizione (opz.), formato QR (google/ics/entrambi).");
            return;
        }

        const QDate data = QDate::fromString(dataS, "yyyy-MM-dd");
        if (!data.isValid()) {
            onDone("Data non valida: '" + dataS + "'. Usa il formato YYYY-MM-DD (es. 2026-07-15).");
            return;
        }
        QTime oraInizio = QTime::fromString(o.value("ora_inizio").toString(), "HH:mm");
        if (!oraInizio.isValid()) oraInizio = QTime(9, 0);
        QTime oraFine = QTime::fromString(o.value("ora_fine").toString(), "HH:mm");
        if (!oraFine.isValid() || oraFine <= oraInizio) oraFine = oraInizio.addSecs(3600);

        const QString luogo       = o.value("luogo").toString();
        const QString descrizione = o.value("descrizione").toString();
        QString formato = o.value("formato").toString().toLower().trimmed();
        if (formato != "ics" && formato != "entrambi") formato = "google";

        const QDateTime start(data, oraInizio);
        const QDateTime end(data, oraFine);

        /* Cartella persistente per .ics + PNG generati */
        const QString dir = QDir::homePath() + "/.prismalux/calendar_events";
        QDir().mkpath(dir);
        const QString slug = titolo.toLower()
            .replace(QRegularExpression("[^a-z0-9]+"), "_")
            .left(40);
        const QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
        const QString base  = dir + "/" + slug + "_" + stamp;

        QJsonArray pngs, labels;

        /* ── Formato Google Calendar: URL con parametri, massima compatibilità ── */
        if (formato == "google" || formato == "entrambi") {
            QUrlQuery q;
            q.addQueryItem("action", "TEMPLATE");
            q.addQueryItem("text", titolo);
            q.addQueryItem("dates", start.toString("yyyyMMdd'T'HHmmss") + "/"
                                   + end.toString("yyyyMMdd'T'HHmmss"));
            if (!descrizione.isEmpty()) q.addQueryItem("details", descrizione);
            if (!luogo.isEmpty())       q.addQueryItem("location", luogo);
            QUrl url("https://calendar.google.com/calendar/render");
            url.setQuery(q);

            const QImage img = QrCodeWidget::renderImage(url.toString());
            if (!img.isNull()) {
                const QString path = base + "_google.png";
                if (img.save(path, "PNG")) {
                    pngs.append(path);
                    labels.append("Google Calendar");
                }
            }
        }

        /* ── Formato .ics universale: VEVENT embeddato nel QR (letto da molte
         * fotocamere native Android/iOS) + file .ics salvato per import
         * manuale su qualunque calendario (fallback se lo scanner non lo
         * riconosce). Orari in UTC con 'Z' per portabilità tra fusi. ── */
        QString icsPath;
        if (formato == "ics" || formato == "entrambi") {
            const QString uid = slug + "_" + stamp + "@prismalux";
            const QString ics = QString(
                "BEGIN:VCALENDAR\r\n"
                "VERSION:2.0\r\n"
                "PRODID:-//Prismalux//Evento//IT\r\n"
                "BEGIN:VEVENT\r\n"
                "UID:%1\r\n"
                "DTSTAMP:%2\r\n"
                "DTSTART:%3\r\n"
                "DTEND:%4\r\n"
                "SUMMARY:%5\r\n"
                "LOCATION:%6\r\n"
                "DESCRIPTION:%7\r\n"
                "END:VEVENT\r\n"
                "END:VCALENDAR\r\n")
                .arg(uid,
                     QDateTime::currentDateTimeUtc().toString("yyyyMMdd'T'HHmmss'Z'"),
                     start.toUTC().toString("yyyyMMdd'T'HHmmss'Z'"),
                     end.toUTC().toString("yyyyMMdd'T'HHmmss'Z'"),
                     titolo, luogo, descrizione);

            icsPath = base + ".ics";
            QFile f(icsPath);
            if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                f.write(ics.toUtf8());
                f.close();
            }

            const QImage img = QrCodeWidget::renderImage(ics);
            if (!img.isNull()) {
                const QString path = base + "_ics.png";
                if (img.save(path, "PNG")) {
                    pngs.append(path);
                    labels.append("Universale (.ics)");
                }
            }
        }

        if (pngs.isEmpty()) {
            onDone("Errore: non sono riuscito a generare il QR code per l'evento.");
            return;
        }

        QJsonObject res;
        res["pngs"]   = pngs;
        res["labels"] = labels;
        if (!icsPath.isEmpty()) res["ics_file"] = icsPath;
        res["testo"] = QString(
            "Evento \"%1\" pronto per il %2 dalle %3 alle %4%5. "
            "Scansiona il QR con la fotocamera del telefono per aggiungerlo al calendario.")
            .arg(titolo,
                 QLocale(QLocale::Italian).toString(data, "dddd d MMMM yyyy"),
                 oraInizio.toString("HH:mm"), oraFine.toString("HH:mm"),
                 luogo.isEmpty() ? QString() : (" presso " + luogo));

        onDone("QR_EVENTO_JSON:" + QString::fromUtf8(
            QJsonDocument(res).toJson(QJsonDocument::Compact)));
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
        static const QRegularExpression re(
            R"(che\s+(?:ora|ore)\s+(?:[èe]|sono|fa)\s+(?:a|in|adesso\s+a)\s+([a-z\xc3\xa0-\xc3\xbf\s]{2,20}?)(?:\?|$|\s+se\s)",
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

    /* ── Giorni lavorativi tra due date ── */
    {
        static const QRegularExpression re(
            R"(giorni\s+lavorativi.{0,15}(\d{1,2})[\/\-\.](\d{1,2})[\/\-\.](\d{2,4}).{0,10}(\d{1,2})[\/\-\.](\d{1,2})[\/\-\.](\d{2,4}))",
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

    return task;
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
        R"(^(?:cosa\s+sai\s+fare|cosa\s+puoi\s+fare|che\s+(?:funzioni|cose|comandi)\s+hai|aiuto|help|comandi(?:\s+disponibili)?|guida|elenco\s+(?:comandi|funzioni))\??\.?$)",
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

    return QString::fromUtf8(
        "HELP_MARKDOWN:"
        "**Ecco le domande che rispondo istantaneamente senza interpellare il modello AI "
        "(risposta locale, zero token):**\n\n"
        "| Categoria | Esempio | Cosa calcola |\n"
        "|---|---|---|\n"
        "| \xf0\x9f\x94\xa2 Matematica/Fisica | \"quanti watt con 12V e 2A\" | Ohm, RC, chimica, conversioni unit\xc3\xa0 |\n"
        "| \xf0\x9f\x93\x90 Geometria | \"area di un rettangolo con base 5 e altezza 3\" | Triangolo, rettangolo, cubo, cilindro |\n"
        "| \xe2\x9a\x96\xef\xb8\x8f IMC | \"imc per 70kg e 1.75m\" | Indice di massa corporea + fascia |\n"
        "| \xf0\x9f\x9b\x90 Numeri romani | \"quanto \xc3\xa8 MCMXCIV\" / \"1994 in romano\" | Conversione bidirezionale |\n"
        "| \xf0\x9f\x93\x85 Date | \"quanti mesi mancano a dicembre\" | Calendario reale, gg/mesi/anni |\n"
        "| \xf0\x9f\x8e\x82 Et\xc3\xa0 | \"quanti anni ho se sono nato il 15/03/1990\" | Et\xc3\xa0 esatta + giorni al compleanno |\n"
        "| \xf0\x9f\x93\x86 Giorno settimana | \"che giorno era il 25/12/2020\" | Calendario deterministico |\n"
        "| \xf0\x9f\x8c\x8d Fuso orario | \"che ora \xc3\xa8 a New York\" | Ora reale in altre citt\xc3\xa0 |\n"
        "| \xf0\x9f\x92\xbc Giorni lavorativi | \"giorni lavorativi tra 03/07/2026 e 15/08/2026\" | Esclusi sabati/domeniche |\n"
        "| \xf0\x9f\x8d\xb3 Cucina | \"quanti grammi sono 200ml di farina\" | ml\xe2\x86\x94grammi, forno, cucchiai/tazze |\n"
        "| \xf0\x9f\x92\xb3 IBAN | \"\xc3\xa8 valido questo IBAN: IT60...\" | Cifra di controllo (mod-97) |\n"
        "| \xf0\x9f\x86\x94 Codice Fiscale | \"RSSMRA85M01H501Q \xc3\xa8 valido?\" | Checksum + data nascita/sesso |\n"
        "| \xf0\x9f\x8f\xa2 Partita IVA | \"12345678903 \xc3\xa8 una p.iva valida?\" | Checksum ufficiale |\n"
        "| \xf0\x9f\x92\xb0 Sconti/IVA/% | \"sconto del 15% su 80 euro\" | Percentuali, sconti, scorporo IVA |\n"
        "| \xf0\x9f\x93\x88 Interesse/Mutuo | \"rata di un mutuo da 100000 euro al 3% in 20 anni\" | Composto, ammortamento francese |\n"
        "| \xf0\x9f\x94\x91 Generatori | \"genera una password di 20 caratteri\" | UUID, hash, password casuali |\n"
        "| \xf0\x9f\x92\xb1 Cambio valuta | \"100 EUR in USD\" | Tasso reale aggiornato (BCE) |\n"
        "| \xf0\x9f\x93\x86 Evento calendario | \"creami un evento per il compleanno\" | QR code Google Calendar/.ics |\n"
        "| \xf0\x9f\x93\x9d Statistiche testo | \"quante parole ha: <testo>\" | Parole/caratteri/frasi + tempo lettura |\n"
        "| \xf0\x9f\xa7\xae Algoritmi classici | \"mcd tra 48 e 18\", \"fattorizzazione di 360\", \"decimo numero di fibonacci\", "
        "\"torre di hanoi con 8 dischi\", \"profitto massimo con prezzi 7,1,5,3,6,4\" | MCD/MCM, fattori primi, Pascal, Fibonacci, "
        "Catalan, Collatz, Hanoi, profitto azioni, inversioni, posizione in array, edit distance/LCS, ricerca pattern |\n"
        "| \xf0\x9f\x93\x88 Grafico | \"") + chartExample + QString::fromUtf8(
        "\" | Plot Cartesiano istantaneo (prova questo!) |\n\n"
        "**Grafici**: scrivendo *\"grafico di FORMULA\"* oppure *\"y = FORMULA\"* disegno subito "
        "il plot cartesiano nella chat. Per torta, istogramma, radar, candlestick e le altre "
        "tipologie disponibili, usa il canvas dedicato nella tab Matematica/Grafico (richiedono "
        "dati strutturati, non una singola riga di chat).\n\n"
        "Per tutto il resto (spiegazioni, scrittura, codice, ricerca...) rispondo con il modello AI selezionato.");
}

/* ── Helper puri per _inject_finance: validazione IBAN (mod-97, ISO 7064)
 * e Codice Fiscale (D.M. 23/12/1976) — nessuna dipendenza UI. ── */
static bool _ibanValid(const QString& ibanRaw)
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

static bool _cfChecksumValid(const QString& cfUpper)
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
static QString _cfDecode(const QString& cfUpper)
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
                return QString("[Calcolo locale: %1 \xe2\x82\xac al %2%% annuo (interesse composto) per %3 anni "
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
                return QString("[Calcolo locale: mutuo di %1 \xe2\x82\xac al %2%% annuo in %3 anni "
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
static QString _execAlgoritmo(const QString& nome, const QJsonObject& p)
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

    /* ── N-esimo numero di Fibonacci ── */
    {
        static const QRegularExpression re(
            R"((\d+)\s*\xc2\xb0?\s*numero\s+di\s+fibonacci|fibonacci\s+(?:di|numero)\s+(\d+))",
            QRegularExpression::CaseInsensitiveOption);
        const auto m = re.match(lo);
        if (m.hasMatch()) {
            const int n = (!m.captured(1).isEmpty() ? m.captured(1) : m.captured(2)).toInt();
            if (n >= 1 && n <= 80) {
                if (n <= 2) {
                    return QString("[Calcolo locale: F(%1) = 1 (sequenza 1,1,2,3,5,8,13,21,...)]\n\n").arg(n) + task;
                }
                const auto steps = SimulatorePage::genFibonacciDP(n);
                if (!steps.isEmpty())
                    return QString("[Calcolo locale: %1]\n\n").arg(steps.last().msg) + task;
            }
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

    /* ── Posizione di un elemento in un array ── */
    {
        static const QRegularExpression re(
            R"((?:in\s+che\s+posizione\s+(?:si\s+trova|\xc3\xa8|e)|trova|indice\s+di)\s+(\-?\d+)\s+(?:in|nell['\xe2\x80\x99a]?\s*array|nella\s+lista)\s*[:\[]?\s*((?:\-?\d+[\s,]+)+\-?\d+)\]?)",
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
