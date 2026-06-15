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

/* ══════════════════════════════════════════════════════════════
   _parseRandomParams — estrae parametri dal testo naturale.

   Riconosce:
   • Conteggio: "10 numeri", "50 valori", "genera 30 dati"
   • Range:     "tra 1 e 100", "da -10 a 10", "min=0 max=255"
   • Tipo:      "decimali" / "float" / "interi" (default: intero)
   • Modalità chart: se il task chiede un grafico, usa count=50 default
   ══════════════════════════════════════════════════════════════ */
struct RandomParams {
    int    count   = 20;      ///< quanti numeri generare
    double rMin    = 1.0;     ///< estremo inferiore del range
    double rMax    = 100.0;   ///< estremo superiore del range
    bool   isFloat = false;   ///< true → decimali con 2 cifre
};

static RandomParams _parseRandomParams(const QString& task) {
    RandomParams p;
    const QString t = task.toLower();

    /* ── Conteggio: cerca "N numeri/valori/dati/campioni/punti" ── */
    {
        static const QRegularExpression reCount(
            R"((\d{1,4})\s*(?:numeri?|valori?|dati|campioni?|elementi?|punti?))",
            QRegularExpression::CaseInsensitiveOption);
        auto m = reCount.match(t);
        if (m.hasMatch()) {
            int n = m.captured(1).toInt();
            if (n >= 1 && n <= 10000) p.count = n;
        } else {
            /* "genera N" / "crea N" / "fammi N" */
            static const QRegularExpression reGen(
                R"((?:genera|crea|fammi|dai mi|dammi|producimi)\s+(\d{1,4}))",
                QRegularExpression::CaseInsensitiveOption);
            auto m2 = reGen.match(t);
            if (m2.hasMatch()) {
                int n = m2.captured(1).toInt();
                if (n >= 1 && n <= 10000) p.count = n;
            }
        }
    }

    /* Se viene richiesto un grafico, usa default 50 punti per densità visiva */
    if (t.contains("grafico") || t.contains("chart") || t.contains("plot"))
        if (p.count == 20) p.count = 50;

    /* ── Range: "tra A e B" / "da A a B" / "in [A, B]" / "min=A max=B" ── */
    {
        static const QRegularExpression reRange(
            R"((?:tra|da|from|in\s*\[?)\s*(-?[\d]+(?:[.,]\d+)?)\s*(?:e|a|to|,)\s*(-?[\d]+(?:[.,]\d+)?))",
            QRegularExpression::CaseInsensitiveOption);
        auto m = reRange.match(t);
        if (m.hasMatch()) {
            double a = m.captured(1).replace(',', '.').toDouble();
            double b = m.captured(2).replace(',', '.').toDouble();
            if (a < b) { p.rMin = a; p.rMax = b; }
        }
        /* "min=A" e "max=B" espliciti */
        static const QRegularExpression reMin(R"(min\s*[=:]\s*(-?[\d]+(?:[.,]\d+)?))",
            QRegularExpression::CaseInsensitiveOption);
        static const QRegularExpression reMax(R"(max\s*[=:]\s*(-?[\d]+(?:[.,]\d+)?))",
            QRegularExpression::CaseInsensitiveOption);
        auto mMin = reMin.match(t);
        auto mMax = reMax.match(t);
        if (mMin.hasMatch()) p.rMin = mMin.captured(1).replace(',', '.').toDouble();
        if (mMax.hasMatch()) p.rMax = mMax.captured(1).replace(',', '.').toDouble();
    }

    /* ── Tipo: float se menziona decimali / float / reali / con virgola ── */
    if (t.contains("decimal") || t.contains("float") ||
        t.contains("reali")   || t.contains("virgola") ||
        t.contains("frazion"))
        p.isFloat = true;

    return p;
}

/* ══════════════════════════════════════════════════════════════
   _isRandomRequest — rileva intento "numeri casuali" nel task.
   Separato da _inject_random per permettere screening rapido.
   ══════════════════════════════════════════════════════════════ */
static bool _isRandomRequest(const QString& task) {
    const QString t = task.toLower();
    /* Una singola alternanza senza split su più raw-literal:
       i `"` all'interno di raw string interlineate vengono inclusi
       letteralmente nel pattern — meglio usare una stringa normale. */
    static const QRegularExpression re(
        "numeri?\\s+casua|valori?\\s+casua|dati\\s+casua"
        "|random|casuali|casuale|randomici|randomico"
        "|genera\\s+numeri|crea\\s+numeri|fammi\\s+numeri"
        "|sample|campion(?:a|i)|genera\\s+dati|valori\\s+random",
        QRegularExpression::CaseInsensitiveOption);
    return re.match(t).hasMatch();
}

/* ══════════════════════════════════════════════════════════════
   _inject_random — tool randomize per LLM.

   Se il task contiene intento "numeri casuali":
     1. Estrae parametri (count, range, tipo)
     2. Genera con std::random_device + std::mt19937 (= C rand() con
        seeding crittografico — equivalente alla randomizzazione C nativa
        ma senza race condition né stato globale)
     3. Prepende "[DATI RANDOM generati localmente — N interi/float
        in [min, max]: x1, x2, ..., xN]" al task
     4. Restituisce il task modificato (con contesto iniettato)

   Se il task NON contiene intento random → restituisce il task invariato.
   ══════════════════════════════════════════════════════════════ */
QString _inject_random(const QString& task) {
    if (!_isRandomRequest(task)) return task;

    const RandomParams p = _parseRandomParams(task);

    /* Generatore: random_device per seed, mt19937 per sequenza */
    std::random_device rd;
    std::mt19937 gen(rd());

    QString numStr;
    if (p.isFloat) {
        std::uniform_real_distribution<double> dist(p.rMin, p.rMax);
        for (int i = 0; i < p.count; ++i) {
            if (i) numStr += ", ";
            numStr += QString::number(dist(gen), 'f', 2);
        }
    } else {
        const long long lo = static_cast<long long>(std::ceil(p.rMin));
        const long long hi = static_cast<long long>(std::floor(p.rMax));
        std::uniform_int_distribution<long long> dist(lo, hi);
        for (int i = 0; i < p.count; ++i) {
            if (i) numStr += ", ";
            numStr += QString::number(dist(gen));
        }
    }

    /* Statistiche rapide da iniettare come contesto aggiuntivo */
    const QString tipo = p.isFloat ? "float" : "interi";
    const QString header = QString(
        "[DATI RANDOM generati localmente con std::random_device+mt19937 "
        "(%1 %2 in [%3, %4]):\n%5]\n\n")
        .arg(p.count)
        .arg(tipo)
        .arg(p.isFloat ? QString::number(p.rMin, 'f', 2) : QString::number((long long)p.rMin))
        .arg(p.isFloat ? QString::number(p.rMax, 'f', 2) : QString::number((long long)p.rMax))
        .arg(numStr);

    return header + task;
}

/* ══════════════════════════════════════════════════════════════
   Tool Use Nativo — implementazione
   Integrato nel pipeline esistente via hook in onFinished(Pipeline).
   ══════════════════════════════════════════════════════════════ */

QString AgentiPage::toolSystemSuffix()
{
    const QString proj = P::root();
    return
        QString::fromUtf8(
            "\n\n[STRUMENTI DISPONIBILI - usali solo se necessario]\n"
            "TOOL_CALL: {\"tool\": \"calc\", \"input\": \"sqrt(144)\"}\n"
            "TOOL_CALL: {\"tool\": \"ricerca\", \"input\": \"query\"}\n"
            "TOOL_CALL: {\"tool\": \"python\", \"input\": \"print(2+2)\"}\n"
            "TOOL_CALL: {\"tool\": \"leggi_file\", \"input\": \"PERCORSO_ESPLICITO\"}\n"
            "TOOL_CALL: {\"tool\": \"lista_file\", \"input\": \"PERCORSO_ESPLICITO\"}\n"
            "Scrivi UNA riga TOOL_CALL: {...} e attendi TOOL_RESULT.\n"
            "REGOLA FILE: usa leggi_file/lista_file SOLO con percorsi forniti dall'utente\n"
            "o dentro la cartella del progetto:\n") +
        proj + "/MCPs  oppure  " + proj + QString::fromUtf8(
            "/Tools\n"
            "NON inventare percorsi /home/... — per cultura generale rispondi direttamente.\n"
            "TOOL_CALL: {\"tool\": \"spawn_agent\", \"input\": \"Ruolo|||Compito completo\"}\n"
            "spawn_agent: crea un sub-agente specializzato (max 4 per sessione) — utile per sotto-task complessi.");
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
                this, [proc, onDone](int, QProcess::ExitStatus) {
            const QString out = QString::fromUtf8(proc->readAll()).trimmed();
            proc->deleteLater();
            onDone(out.isEmpty() ? "nessun risultato" : out.left(600));
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
            const QString out = QString::fromUtf8(proc->readAll()).trimmed();
            proc->deleteLater();
            onDone(out.isEmpty() ? "nessun contenuto ricevuto" : out.left(4000));
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
            if (proc->waitForStarted(4000)) {
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
    if (tool == "get_knowledge" || tool == "knowledge" || tool == "conoscenza") {
        const QString kb = P::readUserKnowledge();
        if (kb.isEmpty())
            onDone("La Knowledge Base personale è vuota.");
        else
            onDone(kb.left(3000));
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

        /* Python: venv del plugin se presente, altrimenti system python3 */
        const QString venvPy = P::root() + "/MCPs/" + plugin + "/venv/bin/python";
        const QString pythonExe = QFile::exists(venvPy) ? venvPy : "python3";

        const QJsonObject argsObj = QJsonDocument::fromJson(argsJson.toUtf8()).object();

        /* Messaggi JSON-RPC 2.0: initialize (id=1) + tools/call (id=2) */
        const QByteArray initMsg = QJsonDocument(QJsonObject{
            {"jsonrpc","2.0"}, {"id",1}, {"method","initialize"},
            {"params", QJsonObject{
                {"protocolVersion","2024-11-05"},
                {"capabilities", QJsonObject{}},
                {"clientInfo", QJsonObject{{"name","prismalux"},{"version","2.9"}}}
            }}
        }).toJson(QJsonDocument::Compact) + "\n";

        const QByteArray callMsg = QJsonDocument(QJsonObject{
            {"jsonrpc","2.0"}, {"id",2}, {"method","tools/call"},
            {"params", QJsonObject{{"name",toolName},{"arguments",argsObj}}}
        }).toJson(QJsonDocument::Compact) + "\n";

        auto* proc = new QProcess(this);
        proc->setProgram(pythonExe);
        proc->setArguments({ serverPath });
        proc->setProcessChannelMode(QProcess::SeparateChannels);
        proc->start();

        if (!proc->waitForStarted(3000)) {
            proc->deleteLater();
            onDone(QString("errore mcp_call: impossibile avviare '%1'").arg(plugin));
            return;
        }
        proc->write(initMsg);
        proc->write(callMsg);
        proc->closeWriteChannel();

        /* Flag condiviso per evitare double-call tra finished e timer */
        auto called = QSharedPointer<bool>::create(false);

        connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                proc, [proc, onDone, called, plugin, toolName](int, QProcess::ExitStatus) {
            proc->deleteLater();
            if (*called) return;
            *called = true;
            const QByteArray raw = proc->readAllStandardOutput();
            /* Cerca riga JSON con id=2 (risposta tools/call) */
            for (const QByteArray& line : raw.split('\n')) {
                const QByteArray l = line.trimmed();
                if (l.isEmpty()) continue;
                const QJsonObject obj = QJsonDocument::fromJson(l).object();
                if (obj.value("id").toInt() != 2) continue;
                if (obj.contains("error")) {
                    const QString msg = obj.value("error").toObject()
                                           .value("message").toString("errore sconosciuto");
                    onDone(QString("[MCP %1::%2 errore] %3").arg(plugin, toolName, msg));
                } else {
                    const QJsonObject res = obj.value("result").toObject();
                    QString text;
                    for (const auto& c : res.value("content").toArray()) {
                        if (c.toObject().value("type").toString() == "text")
                            text += c.toObject().value("text").toString() + "\n";
                    }
                    if (text.isEmpty())
                        text = QJsonDocument(res).toJson(QJsonDocument::Compact);
                    onDone(text.trimmed().left(3000));
                }
                return;
            }
            onDone(QString("[MCP %1::%2] nessuna risposta valida ricevuta").arg(plugin, toolName));
        });

        QTimer::singleShot(30000, proc, [proc, onDone, called, plugin, toolName]() {
            if (*called || proc->state() == QProcess::NotRunning) return;
            *called = true;
            proc->kill();
            onDone(QString("[MCP %1::%2] timeout 30s").arg(plugin, toolName));
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
   Stato: "running" (grigio) oppure "done" (verde) / "error" (rosso).     */
static QString _toolBubble(const QString& name, const QString& input,
                            const QString& result, bool done, bool error = false)
{
    const QString bg      = done ? (error ? "#3b1a1a" : "#1a2e1a") : "#1e2030";
    const QString border  = done ? (error ? "#ef4444" : "#22c55e") : "#6366f1";
    const QString icon    = done ? (error ? "\xe2\x9d\x8c" : "\xe2\x9c\x85") : "\xf0\x9f\x94\xa7";
    const QString status  = done ? (error ? "Errore" : "Completato") : "In esecuzione\xe2\x80\xa6";
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
        html += "<br><span style='color:#94a3b8;'>→ </span>"
                "<code style='color:#e2e8f0;'>" + inEsc + "</code>";

    if (done && !result.isEmpty())
        html += "<br><span style='color:#86efac;'>"
                + resEsc + "</span>";

    html += "</div>";
    return html;
}

void AgentiPage::onNativeToolCall(const QString& name, const QJsonObject& args)
{
    /* spawn_agent ha due parametri (role + task) — li combiniamo con ||| */
    /* mcp_call ha tre parametri (plugin + tool_name + args_json) — JSON compatto */
    QString input;
    if (name == QLatin1String("spawn_agent")) {
        const QString role = args.value("role").toString().trimmed();
        const QString task = args.value("task").toString().trimmed();
        input = role + "|||" + task;
    } else if (name == QLatin1String("mcp_call")) {
        QJsonObject jo;
        jo["plugin"]    = args.value("plugin").toString().trimmed();
        jo["tool_name"] = args.value("tool_name").toString().trimmed();
        const QString aj = args.value("args_json").toString().trimmed();
        jo["args_json"] = aj.isEmpty() ? "{}" : aj;
        input = QJsonDocument(jo).toJson(QJsonDocument::Compact);
    } else {
        /* Tutti gli altri tool: estrae il valore del primo argomento */
        for (auto it = args.constBegin(); it != args.constEnd(); ++it) {
            const QJsonValue v = it.value();
            if (v.isString())  { input = v.toString(); break; }
            if (v.isDouble())  { input = QString::number(v.toDouble()); break; }
            if (v.isObject())  { input = QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact); break; }
        }
        if (input.isEmpty())
            input = QJsonDocument(args).toJson(QJsonDocument::Compact);
    }

    /* Inserisce la mini-bolla "in esecuzione" e salva il cursore per aggiornarla */
    m_log->moveCursor(QTextCursor::End);
    const int anchorPos = m_log->textCursor().position();
    m_log->insertHtml(_toolBubble(name, input, {}, false));
    m_log->append({});

    QJsonObject call;
    call["tool"]  = name;
    call["input"] = input;

    runToolCall(call, [this, name, input, anchorPos](const QString& result) {
        const bool isErr = result.startsWith("errore:") || result.startsWith("Nessun");

        /* Aggiorna la mini-bolla sostituendo il testo dal cursore salvato */
        QTextCursor cur(m_log->document());
        cur.setPosition(anchorPos);
        cur.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
        cur.removeSelectedText();
        cur.insertHtml(_toolBubble(name, input, result, true, isErr));
        m_log->append({});

        m_ai->replyWithTool(name, result);
    });
}
