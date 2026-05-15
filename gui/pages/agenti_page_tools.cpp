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
     m_taskOriginal = _inject_random(_inject_math(task));
   ══════════════════════════════════════════════════════════════ */
#include "agenti_page.h"
#include "agenti_page_p.h"
#include "../prismalux_paths.h"
#include "../app_config.h"
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
    return QString::fromLatin1(
        "\n\n[STRUMENTI DISPONIBILI - usali se necessario]\n"
        "TOOL_CALL: {\"tool\": \"calc\", \"input\": \"sqrt(144)\"}\n"
        "TOOL_CALL: {\"tool\": \"ricerca\", \"input\": \"query\"}\n"
        "TOOL_CALL: {\"tool\": \"python\", \"input\": \"print(2+2)\"}\n"
        "TOOL_CALL: {\"tool\": \"leggi_file\", \"input\": \"/percorso/file.txt\"}\n"
        "TOOL_CALL: {\"tool\": \"lista_file\", \"input\": \"/percorso/cartella/\"}\n"
        "Scrivi UNA riga TOOL_CALL: {...} e attendi TOOL_RESULT. "
        "Se non ti servono strumenti, rispondi normalmente in italiano.");
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
            "    if d.get('AbstractText'): out.append(d['AbstractText'][:300])\n"
            "    elif d.get('Answer'): out.append(d['Answer'][:300])\n"
            "    for t in d.get('RelatedTopics',[])[:3]:\n"
            "        if isinstance(t,dict) and t.get('Text'): out.append(t['Text'][:150])\n"
            "    print('\\n'.join(out) if out else 'nessun risultato')\n"
            "except Exception as e: print('ERRORE:',e)\n";
        auto* proc = new QProcess(this);
        proc->setProcessChannelMode(QProcess::MergedChannels);
        connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                this, [proc, onDone](int, QProcess::ExitStatus) {
            const QString out = QString::fromUtf8(proc->readAll()).trimmed();
            proc->deleteLater();
            onDone(out.isEmpty() ? "nessun risultato" : out.left(500));
        });
        proc->start(P::findPython(), {"-c", script});
        QTimer::singleShot(12000, proc, [proc, onDone]{
            if (proc->state() != QProcess::NotRunning) { proc->kill(); onDone("timeout ricerca"); }
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
        const QString path = input.trimmed();
        if (path.isEmpty()) { onDone("errore: percorso file non specificato"); return; }
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            onDone(QString("errore: impossibile aprire '%1' — %2").arg(path, f.errorString()));
            return;
        }
        const QString content = QString::fromUtf8(f.readAll()).left(4000);
        f.close();
        onDone(content.isEmpty() ? "(file vuoto)" : content);
        return;
    }

    /* ── Lista file in cartella ── */
    if (tool == "lista_file" || tool == "list_files" || tool == "ls" || tool == "lista") {
        const QString dirPath = input.trimmed();
        if (dirPath.isEmpty()) { onDone("errore: percorso cartella non specificato"); return; }
        QDir dir(dirPath);
        if (!dir.exists()) {
            onDone(QString("errore: cartella '%1' non trovata").arg(dirPath));
            return;
        }
        const QStringList entries = dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot,
                                                   QDir::Name | QDir::DirsFirst);
        if (entries.isEmpty()) { onDone("(cartella vuota)"); return; }
        /* Aggiunge '/' ai nomi di directory per distinguerli dai file */
        QStringList annotated;
        annotated.reserve(entries.size());
        for (const QString& e : entries) {
            const QString full = dirPath + "/" + e;
            annotated << (QFileInfo(full).isDir() ? e + "/" : e);
        }
        onDone(annotated.join("\n").left(2000));
        return;
    }

    /* ── Scrivi file (richiede conferma utente) ── */
    if (tool == "scrivi_file" || tool == "write_file" || tool == "scrivi") {
        /* Formato input: "percorso|||contenuto" */
        const int sep = input.indexOf("|||");
        if (sep < 0) {
            onDone("errore: formato scrivi_file deve essere \"percorso|||contenuto\"");
            return;
        }
        const QString filePath = input.left(sep).trimmed();
        const QString fileContent = input.mid(sep + 3);
        if (filePath.isEmpty()) { onDone("errore: percorso file non specificato"); return; }

        /* Conferma utente: sicurezza — non scriviamo file silenziosamente */
        {
            auto* dlg = new QDialog(this);
            dlg->setWindowTitle("\xf0\x9f\x93\x9d  Scrivi file?");
            dlg->setMinimumSize(480, 320);
            auto* lay = new QVBoxLayout(dlg);
            auto* lbl = new QLabel(
                QString("L\xe2\x80\x99" "agente autonomo vuole scrivere il file:\n"
                        "<b>%1</b>\n\nContenuto (%2 caratteri):").arg(
                    filePath.toHtmlEscaped(),
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

        QFile f(filePath);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            onDone(QString("errore: impossibile scrivere '%1' — %2").arg(filePath, f.errorString()));
            return;
        }
        f.write(fileContent.toUtf8());
        f.close();
        onDone(QString("file '%1' scritto con successo (%2 byte)")
               .arg(filePath).arg(fileContent.toUtf8().size()));
        return;
    }

    onDone(QString("strumento non riconosciuto: %1").arg(tool));
}

/* ══════════════════════════════════════════════════════════════
   onNativeToolCall — handler per Ollama function calling nativo.
   Adatta il formato Ollama {name, arguments:{...}} al formato
   runToolCall {tool, input} e riprende la conversazione con replyWithTool().
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::onNativeToolCall(const QString& name, const QJsonObject& args)
{
    /* Mostra nel log che un tool è stato richiesto */
    m_log->append(QString(
        "\n\xf0\x9f\x94\xa7  <b>Tool:</b> <code>%1</code> "
        "\xe2\x80\x94 esecuzione in corso...\n").arg(name.toHtmlEscaped()));

    /* Converti args Ollama → formato runToolCall {tool, input} */
    QJsonObject call;
    call["tool"] = name;

    /* Estrae il valore del primo argomento come input stringa.
       Ollama invia args con nomi specifici (expression, query, path, code, ecc.). */
    QString input;
    for (auto it = args.constBegin(); it != args.constEnd(); ++it) {
        const QJsonValue v = it.value();
        if (v.isString())       { input = v.toString(); break; }
        if (v.isDouble())       { input = QString::number(v.toDouble()); break; }
        if (v.isObject())       { input = QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact); break; }
    }
    if (input.isEmpty())
        input = QJsonDocument(args).toJson(QJsonDocument::Compact);
    call["input"] = input;

    runToolCall(call, [this, name](const QString& result) {
        m_log->append(QString(
            "\xe2\x9c\x85  <b>Risultato</b> <code>%1</code>: %2\n")
            .arg(name.toHtmlEscaped(), result.toHtmlEscaped().left(200)));
        m_ai->replyWithTool(name, result);
    });
}
