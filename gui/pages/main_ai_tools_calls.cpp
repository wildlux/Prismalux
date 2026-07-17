/* ══════════════════════════════════════════════════════════════
   main_ai_tools_calls.cpp — Implementazione dei tool AgentiPage::onTool*()

   Estratto da main_ai_tools.cpp (D-35, 2026-07-10): AgentiPage::runToolCall()
   era un unico if/else-if di 1455 righe che dispatchava per nome-tool —
   ogni ramo è ora un metodo AgentiPage::onTool<Nome>() a sé, dichiarato in
   main_ai.h, richiamato da runToolCall() (rimasta in main_ai_tools.cpp,
   ora ~30 righe) con "if (tool == \"...\") { onTool...(input, onDone); return; }".

   Comportamento identico all'originale: estrazione meccanica riga per
   riga, nessuna logica toccata. onToolMcpCall() è l'unica eccezione, resta
   in main_ai_tools.cpp perché usa _launchMcpProcess()/_attachMcpReader()
   (quest'ultima è un template, non spostabile qui senza portarne anche la
   definizione in un header condiviso).
   ══════════════════════════════════════════════════════════════ */
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

void AgentiPage::onToolCalc(const QString& input, const std::function<void(QString)>& onDone)
{
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
        connect(proc, &QProcess::errorOccurred, this, [proc, onDone](QProcess::ProcessError err) {
            /* FailedToStart: finished() non verrà mai emesso e il timeout non
               scatta (stato già NotRunning) — senza onDone() qui il chiamante
               resterebbe appeso (es. pulsante bloccato su "Stop"). */
            if (err == QProcess::FailedToStart) {
                qWarning() << "[main_ai_tools] calc non avviato:" << proc->program();
                proc->deleteLater();
                onDone("errore: interprete Python non avviato");
            }
        });
        proc->start(P::findPython(), {"-c", script});
        QTimer::singleShot(5000, proc, [proc, onDone]{
            if (proc->state() != QProcess::NotRunning) { proc->kill(); onDone("timeout"); }
        });
        return;
}

void AgentiPage::onToolAlgoritmo(const QString& input, const std::function<void(QString)>& onDone)
{
        const QJsonObject o = QJsonDocument::fromJson(input.toUtf8()).object();
        const QString nome = o.value("nome").toString().trimmed();
        if (nome.isEmpty()) { onDone("errore: campo 'nome' obbligatorio"); return; }
        onDone(_execAlgoritmo(nome, o));
        return;
}

void AgentiPage::onToolCodiceFiscale(const QString& input, const std::function<void(QString)>& onDone)
{
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

void AgentiPage::onToolFinanzaCalcola(const QString& input, const std::function<void(QString)>& onDone)
{
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
            onDone(QString("%1 \xe2\x82\xac al %2% annuo (interesse composto) per %3 anni "
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
            onDone(QString("mutuo di %1 \xe2\x82\xac al %2% annuo in %3 anni "
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
            onDone(QString("TFR su %1 anni, stipendio lordo annuo %2 \xe2\x82\xac, inflazione %3% "
                           "\xe2\x80\x94 quota annua %4 \xe2\x82\xac, tasso rivalutazione %5% "
                           "\xe2\x86\x92 TFR rivalutato %6 \xe2\x82\xac (senza rivalutazione %7 \xe2\x82\xac). "
                           "Netto stimato: %8 \xe2\x82\xac in azienda (tassazione separata ~23%) o "
                           "%9 \xe2\x82\xac in fondo pensione (imposta sostitutiva 15%, calcolo indicativo art. 2120 c.c.)")
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

void AgentiPage::onToolValidaDocumento(const QString& input, const std::function<void(QString)>& onDone)
{
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

void AgentiPage::onToolCartaAstrale(const QString& input, const std::function<void(QString)>& onDone)
{
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

void AgentiPage::onToolConverti(const QString& input, const std::function<void(QString)>& onDone)
{
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

void AgentiPage::onToolDisegnaGrafico(const QString& input, const std::function<void(QString)>& onDone)
{
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

void AgentiPage::onToolRicerca(const QString& input, const std::function<void(QString)>& onDone)
{
                /* Cache DuckDuckGo, TTL 5 min — prima file-static in main_ai_tools.cpp
           (unico uso), ora locale a questa funzione (D-35). */
        static QHash<QString, QPair<QString, qint64>> s_ddgCache;
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
        /* Tre fonti in cascata: instant-answer API (abstract enciclopedico,
           spesso vuoto per query fattuali) + primi 10 risultati organici
           (titolo+snippet) da lite.duckduckgo.com, con fallback Bing RSS se
           DDG risponde col bot-challenge (html.duckduckgo.com dà 202 vuoto,
           verificato 2026-07-13 — lite passa, ma potrebbe cambiare) — servono
           al retry automatico "LLM incerto" come contesto per la 2ª chiamata. */
        const QString script =
            "import urllib.request,urllib.parse,json,re,html as H\n"
            "q=urllib.parse.quote_plus(" + _inputJson + ")\n"
            "out=[]\n"
            "strip=lambda s:H.unescape(re.sub(r'<[^>]+>','',s)).strip()\n"
            "def fetch(url,t):\n"
            "    req=urllib.request.Request(url,headers={'User-Agent':'Mozilla/5.0'})\n"
            "    with urllib.request.urlopen(req,timeout=t) as r:\n"
            "        return r.read().decode('utf-8','replace')\n"
            "try:\n"
            "    d=json.loads(fetch(f'https://api.duckduckgo.com/?q={q}&format=json&no_redirect=1&no_html=1',7))\n"
            "    if d.get('AbstractText'): out.append(d['AbstractText'][:400])\n"
            "    elif d.get('Answer'): out.append(d['Answer'][:400])\n"
            "    for t in d.get('RelatedTopics',[])[:5]:\n"
            "        if isinstance(t,dict) and t.get('Text'): out.append(t['Text'][:200])\n"
            "except Exception: pass\n"
            "res=[]\n"
            "try:\n"
            "    page=fetch(f'https://lite.duckduckgo.com/lite/?q={q}',8)\n"
            "    titles=re.findall(r\"class='result-link'>(.*?)</a>\",page,re.S)\n"
            "    snips=re.findall(r\"class='result-snippet'>(.*?)</td>\",page,re.S)\n"
            "    res=[(strip(t),strip(snips[i]) if i<len(snips) else '')\n"
            "         for i,t in enumerate(titles[:10])]\n"
            "except Exception: pass\n"
            "if not res:\n"
            "    try:\n"
            "        rss=fetch(f'https://www.bing.com/search?q={q}&format=rss',8)\n"
            "        items=re.findall(r'<item><title>(.*?)</title>.*?<description>(.*?)</description>',rss,re.S)\n"
            "        res=[(strip(t),strip(d)) for t,d in items[:10]]\n"
            "    except Exception: pass\n"
            "for i,(t,sn) in enumerate(res):\n"
            "    row=t[:120]\n"
            "    if sn: row+=' — '+sn[:220]\n"
            "    out.append(f'{i+1}. {row}')\n"
            "print('\\n'.join(out) if out else 'nessun risultato — prova fetch_url se hai un URL diretto')\n";
        auto* proc = new QProcess(this);
        proc->setProcessChannelMode(QProcess::MergedChannels);
        connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                this, [proc, onDone, cacheKey](int, QProcess::ExitStatus) {
            /* 2800 (era 600): 10 risultati organici ~230 char l'uno — con 600
               il contesto del retry automatico si riduceva a 2-3 righe. */
            const QString out = _truncateResult(QString::fromUtf8(proc->readAll()).trimmed(), 2800);
            proc->deleteLater();
            const QString result = out.isEmpty() ? "nessun risultato" : out;
            s_ddgCache[cacheKey] = { result, QDateTime::currentMSecsSinceEpoch() };
            onDone(result);
        });
        connect(proc, &QProcess::errorOccurred, this, [proc, onDone](QProcess::ProcessError err) {
            /* FailedToStart: vedi nota nel tool calc — senza onDone() il
               chiamante (es. runWebSearchAgent) resterebbe su "Stop". */
            if (err == QProcess::FailedToStart) {
                qWarning() << "[main_ai_tools] ricerca non avviata:" << proc->program();
                proc->deleteLater();
                onDone("errore: interprete Python non avviato");
            }
        });
        proc->start(P::findPython(), {"-c", script});
        QTimer::singleShot(12000, proc, [proc, onDone]{
            if (proc->state() != QProcess::NotRunning) { proc->kill(); onDone("timeout ricerca"); }
        });
        return;
}

void AgentiPage::onToolCambioValuta(const QString& input, const std::function<void(QString)>& onDone)
{
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

void AgentiPage::onToolFetchUrl(const QString& input, const std::function<void(QString)>& onDone)
{
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

void AgentiPage::onToolPython(const QString& input, const std::function<void(QString)>& onDone)
{
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

void AgentiPage::onToolLeggiFile(const QString& input, const std::function<void(QString)>& onDone)
{
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

void AgentiPage::onToolListaFile(const QString& input, const std::function<void(QString)>& onDone)
{
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

void AgentiPage::onToolScriviFile(const QString& input, const std::function<void(QString)>& onDone)
{
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
            btnBox->addButton(tr("\xf0\x9f\x93\x9d  Scrivi"), QDialogButtonBox::AcceptRole);
            btnBox->addButton(tr("\xe2\x9c\x96  Annulla"), QDialogButtonBox::RejectRole);
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

void AgentiPage::onToolSearchRag(const QString& input, const std::function<void(QString)>& onDone)
{
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

void AgentiPage::onToolGraphMemory(const QString& input, const std::function<void(QString)>& onDone)
{
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

void AgentiPage::onToolGetDatetime(const QString& input, const std::function<void(QString)>& onDone)
{
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

void AgentiPage::onToolDateCalc(const QString& input, const std::function<void(QString)>& onDone)
{

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

void AgentiPage::onToolEventoCalendario(const QString& input, const std::function<void(QString)>& onDone)
{
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
        /* D-50: default "entrambi" — senza il QR .ics un iPhone resterebbe
           fuori (intent:// è solo Android e il link https apre il browser);
           la fotocamera iOS riconosce nativamente il VEVENT nel QR. */
        if (formato != "ics" && formato != "google") formato = "entrambi";

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

        /* ── Formato Google Calendar: URL universale (funziona ovunque, ma
         * su Android il link https semplice quasi sempre apre il BROWSER
         * invece dell'app, anche con Google Calendar installato — segnalato
         * da Paolo). Aggiunta una seconda variante SOLO Android con schema
         * intent:// (S.browser_fallback_url = lo stesso URL https, se
         * l'app non è installata) che dice esplicitamente al telefono di
         * aprire com.google.android.calendar — su iPhone intent:// non
         * significa nulla, per questo resta un QR SEPARATO e non sostituisce
         * quello universale. ── */
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

            const QImage img = QrCodeWidget::renderImage(url.toString(QUrl::FullyEncoded));
            if (!img.isNull()) {
                const QString path = base + "_google.png";
                if (img.save(path, "PNG")) {
                    pngs.append(path);
                    labels.append("Google Calendar");
                }
            }

            const QImage imgAndroid = QrCodeWidget::renderImage(_buildGoogleCalendarIntentUrl(q, url));
            if (!imgAndroid.isNull()) {
                const QString path = base + "_google_android.png";
                if (imgAndroid.save(path, "PNG")) {
                    pngs.append(path);
                    labels.append("Google Calendar (Android)");
                }
            }
        }

        /* ── Formato .ics universale: VEVENT embeddato nel QR (letto da molte
         * fotocamere native Android/iOS) + file .ics salvato per import
         * manuale su qualunque calendario (fallback se lo scanner non lo
         * riconosce). Orari in UTC con 'Z' per portabilità tra fusi. Campi
         * testo escapati con _icsEscapeText() (RFC 5545 §3.3.11): virgola,
         * punto e virgola e ritorno a capo non escapati rompono il parsing
         * per un lettore calendario rigoroso (campo troncato o intero
         * VCALENDAR scartato) — mai stato un problema con titoli semplici,
         * ma un luogo/descrizione con una virgola bastava a farlo. ── */
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
                     end.toUTC().toString("yyyyMMdd'T'HHmmss'Z'"))
                .arg(_icsEscapeText(titolo), _icsEscapeText(luogo), _icsEscapeText(descrizione));

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
                    labels.append("\xf0\x9f\x8d\x8e Apple iPhone / universale (.ics)");
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
            "Scansiona il QR con la fotocamera del telefono: "
            "\xf0\x9f\xa4\x96 Android \xe2\x86\x92 QR \"Google Calendar (Android)\"; "
            "\xf0\x9f\x8d\x8e iPhone \xe2\x86\x92 QR \"Apple iPhone / universale\" "
            "(la fotocamera iOS propone subito \"Aggiungi al calendario\").")
            .arg(titolo,
                 QLocale(QLocale::Italian).toString(data, "dddd d MMMM yyyy"),
                 oraInizio.toString("HH:mm"), oraFine.toString("HH:mm"),
                 luogo.isEmpty() ? QString() : (" presso " + luogo));

        onDone("QR_EVENTO_JSON:" + QString::fromUtf8(
            QJsonDocument(res).toJson(QJsonDocument::Compact)));
        return;
}

void AgentiPage::onToolGetKnowledge(const QString& input, const std::function<void(QString)>& onDone)
{
        const QString kb = P::readUserKnowledge();
        if (kb.isEmpty())
            onDone("La Knowledge Base personale è vuota.");
        else
            onDone(kb.left(3000));
        return;
}

void AgentiPage::onToolLeggiRiassunto(const QString& input, const std::function<void(QString)>& onDone)
{
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

void AgentiPage::onToolScriviRiassunto(const QString& input, const std::function<void(QString)>& onDone)
{
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

void AgentiPage::onToolSpawnAgent(const QString& input, const std::function<void(QString)>& onDone)
{
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
