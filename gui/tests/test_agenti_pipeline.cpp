/* ══════════════════════════════════════════════════════════════
   test_agenti_pipeline.cpp — Unit test metodi statici AgentiPage

   Categorie:
     CAT-A  buildUserBubble — HTML generato, XSS escaping, link azione
     CAT-B  buildAgentBubble — label, model, timestamp, htmlContent
     CAT-C  buildLocalBubble — result, timing ms/s
     CAT-D  markdownToHtml — bold, italic, code inline, code fence,
                             heading, lista, separatore, XSS protezione

   Build:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_agenti_pipeline
     ./build_tests/test_agenti_pipeline
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QApplication>
#include <QUrl>
#include <QUrlQuery>
#include <QSet>
#include "../pages/main_ai.h"
#include "../widgets/formula_parser.h"

/* ══════════════════════════════════════════════════════════════
   CAT-A — buildUserBubble
   ══════════════════════════════════════════════════════════════ */
class TestUserBubble : public QObject {
    Q_OBJECT
private slots:

    /* A-1: contiene identificatore "Tu" nella bolla */
    void contieneEtichettaUtente() {
        const QString html = AgentiPage::buildUserBubble("Ciao");
        QVERIFY2(html.contains("Tu"), "manca etichetta utente 'Tu'");
    }

    /* A-2: testo preservato nel contenuto della bolla */
    void testoPreservato() {
        const QString html = AgentiPage::buildUserBubble("HELLO WORLD");
        QVERIFY2(html.contains("HELLO WORLD"),
                 "buildUserBubble deve includere il testo originale");
    }

    /* A-3: XSS escaping — < > & devono essere escaped */
    void xssEscaping() {
        const QString html = AgentiPage::buildUserBubble("<script>alert(1)</script>");
        QVERIFY2(!html.contains("<script>"), "tag <script> non deve apparire nell'HTML");
        QVERIFY2(html.contains("&lt;script&gt;"), "manca escaping &lt;script&gt;");
    }

    /* A-4: & è escaped come &amp; */
    void ampersandEscaping() {
        const QString html = AgentiPage::buildUserBubble("a&b");
        QVERIFY2(html.contains("&amp;"), "& deve diventare &amp;");
    }

    /* A-5: senza bubbleIdx non c'è barra azioni */
    void senzaBubbleIdxNessunaBarra() {
        const QString html = AgentiPage::buildUserBubble("test");
        QVERIFY2(!html.contains("copy:"), "senza bubbleIdx non deve esserci link copy:");
    }

    /* A-6: con bubbleIdx=0 la barra azioni contiene i link copy/edit/tts/del */
    void conBubbleIdxHaBarraAzioni() {
        const QString html = AgentiPage::buildUserBubble("test", 0);
        QVERIFY2(html.contains("copy:0:"), "manca link copy:0:");
        QVERIFY2(html.contains("edit:0:"), "manca link edit:0:");
        QVERIFY2(html.contains("tts:0:"),  "manca link tts:0:");
        QVERIFY2(html.contains("del:0:"),  "manca link del:0:");
    }

    /* A-7: output non vuoto */
    void outputNonVuoto() {
        QVERIFY(!AgentiPage::buildUserBubble("x").isEmpty());
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B — buildAgentBubble
   ══════════════════════════════════════════════════════════════ */
class TestAgentBubble : public QObject {
    Q_OBJECT
private slots:

    /* B-1: contiene la label dell'agente */
    void contieneLabel() {
        const QString html = AgentiPage::buildAgentBubble(
            "Agente 1 — Ricercatore", "llama3:8b", "12:00:00", "<p>risposta</p>");
        QVERIFY2(html.contains("Agente 1"), "manca label agente");
    }

    /* B-2: contiene il nome del modello */
    void contieneModello() {
        const QString html = AgentiPage::buildAgentBubble(
            "Agente 2", "deepseek-r1:7b", "12:00:01", "<p>output</p>");
        QVERIFY2(html.contains("deepseek-r1:7b"), "manca nome modello");
    }

    /* B-3: contiene il timestamp */
    void contieneTimestamp() {
        const QString html = AgentiPage::buildAgentBubble(
            "Agente 3", "mistral", "01:23:45", "<p>ok</p>");
        QVERIFY2(html.contains("01:23:45"), "manca timestamp");
    }

    /* B-4: contiene l'htmlContent passato */
    void contieneHtmlContent() {
        const QString html = AgentiPage::buildAgentBubble(
            "A", "m", "t", "<p>contenuto_speciale</p>");
        QVERIFY2(html.contains("contenuto_speciale"), "manca htmlContent");
    }

    /* B-5: XSS nel nome modello — < > devono essere escaped */
    void xssNomeModello() {
        const QString html = AgentiPage::buildAgentBubble(
            "A", "<evil>", "t", "<p>x</p>");
        QVERIFY2(!html.contains("<evil>"), "XSS nel nome modello non bloccato");
        QVERIFY2(html.contains("&lt;evil&gt;"), "manca escaping nome modello");
    }

    /* B-6: XSS nella label — escaped */
    void xssLabel() {
        const QString html = AgentiPage::buildAgentBubble(
            "<b>inject</b>", "m", "t", "<p>x</p>");
        QVERIFY2(!html.contains("<b>inject</b>"),
                 "XSS nella label non bloccato (la label non è HTML intenzionale)");
    }

    /* B-7: output non vuoto */
    void outputNonVuoto() {
        QVERIFY(!AgentiPage::buildAgentBubble("A","m","t","<p>x</p>").isEmpty());
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-C — buildLocalBubble
   ══════════════════════════════════════════════════════════════ */
class TestLocalBubble : public QObject {
    Q_OBJECT
private slots:

    /* C-1: contiene il risultato */
    void contieneRisultato() {
        const QString html = AgentiPage::buildLocalBubble("42", 1.5);
        QVERIFY2(html.contains("42"), "manca risultato nella bolla locale");
    }

    /* C-2: timing < 1 ms → formato "N.NNN ms" */
    void timingInMillisecondi() {
        const QString html = AgentiPage::buildLocalBubble("ok", 0.123);
        QVERIFY2(html.contains("ms"), "manca unità ms per timing < 1ms");
    }

    /* C-3: timing >= 1 ms → formato "N.NNN s" */
    void timingInSecondi() {
        const QString html = AgentiPage::buildLocalBubble("ok", 1234.0);
        QVERIFY2(html.contains(" s"), "manca unità s per timing >= 1ms");
    }

    /* C-4: XSS nel risultato — escaped */
    void xssRisultato() {
        const QString html = AgentiPage::buildLocalBubble("<b>inject</b>", 1.0);
        QVERIFY2(!html.contains("<b>inject</b>"), "XSS nel risultato non bloccato");
        QVERIFY2(html.contains("&lt;b&gt;"), "manca escaping risultato");
    }

    /* C-5: senza bubbleIdx nessuna barra azioni */
    void senzaBubbleIdxNessunaBarra() {
        const QString html = AgentiPage::buildLocalBubble("x", 1.0);
        QVERIFY2(!html.contains("copy:"), "senza bubbleIdx non deve esserci copy:");
    }

    /* C-6: con bubbleIdx la barra azioni è presente */
    void conBubbleIdxHaBarraAzioni() {
        const QString html = AgentiPage::buildLocalBubble("x", 1.0, 3);
        QVERIFY2(html.contains("copy:3:"), "manca link copy:3:");
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-D — markdownToHtml
   ══════════════════════════════════════════════════════════════ */
class TestMarkdownToHtml : public QObject {
    Q_OBJECT
private slots:

    /* D-1: bold **testo** → <b>testo</b> */
    void bold() {
        const QString html = AgentiPage::markdownToHtml("**grassetto**");
        QVERIFY2(html.contains("<b>grassetto</b>"), "manca <b> per **testo**");
    }

    /* D-2: bold __testo__ → <b>testo</b> */
    void boldUnderscore() {
        const QString html = AgentiPage::markdownToHtml("__grassetto__");
        QVERIFY2(html.contains("<b>grassetto</b>"), "manca <b> per __testo__");
    }

    /* D-3: italic *testo* → <i>testo</i> */
    void italic() {
        const QString html = AgentiPage::markdownToHtml("*corsivo*");
        QVERIFY2(html.contains("<i>corsivo</i>"), "manca <i> per *testo*");
    }

    /* D-4: code inline `testo` → contiene <code> */
    void codeInline() {
        const QString html = AgentiPage::markdownToHtml("`codice`");
        QVERIFY2(html.contains("<code"), "manca <code> per `testo`");
        QVERIFY2(html.contains("codice"), "contenuto code inline mancante");
    }

    /* D-5: code fence ``` → contiene <pre> */
    void codeFence() {
        const QString html = AgentiPage::markdownToHtml("```\nx = 1\n```");
        QVERIFY2(html.contains("<pre"), "manca <pre> per code fence");
        QVERIFY2(html.contains("x = 1"), "contenuto code fence mancante");
    }

    /* D-6: code fence python → <pre> con contenuto */
    void codeFencePython() {
        const QString html = AgentiPage::markdownToHtml("```python\nprint('ok')\n```");
        QVERIFY2(html.contains("<pre"), "manca <pre> per code fence python");
        QVERIFY2(html.contains("print"), "contenuto print mancante");
    }

    /* D-7: heading # → <h2> */
    void headingH1() {
        const QString html = AgentiPage::markdownToHtml("# Titolo");
        QVERIFY2(html.contains("<h2"), "manca <h2> per # heading");
        QVERIFY2(html.contains("Titolo"), "testo heading mancante");
    }

    /* D-8: heading ## → <h3> */
    void headingH2() {
        const QString html = AgentiPage::markdownToHtml("## Sottotitolo");
        QVERIFY2(html.contains("<h3"), "manca <h3> per ## heading");
    }

    /* D-9: heading ### → <h4> */
    void headingH3() {
        const QString html = AgentiPage::markdownToHtml("### Terzo livello");
        QVERIFY2(html.contains("<h4"), "manca <h4> per ### heading");
    }

    /* D-10: lista puntata - item → <li> */
    void listaPuntata() {
        const QString html = AgentiPage::markdownToHtml("- elemento");
        QVERIFY2(html.contains("<li"), "manca <li> per lista puntata");
        QVERIFY2(html.contains("elemento"), "testo elemento lista mancante");
    }

    /* D-11: lista puntata * item → <li> */
    void listaPuntataAsterisco() {
        const QString html = AgentiPage::markdownToHtml("* voce");
        QVERIFY2(html.contains("<li"), "manca <li> per * lista");
    }

    /* D-12: separatore --- → <hr */
    void separatoreHr() {
        const QString html = AgentiPage::markdownToHtml("---");
        QVERIFY2(html.contains("<hr"), "manca <hr> per separatore ---");
    }

    /* D-13: XSS nell'input — < > & devono essere escaped FUORI dai code block */
    void xssProtezioneParagrafo() {
        const QString html = AgentiPage::markdownToHtml("<script>alert(1)</script>");
        QVERIFY2(!html.contains("<script>"), "XSS non bloccato in markdownToHtml");
        QVERIFY2(html.contains("&lt;script&gt;"), "manca escaping XSS");
    }

    /* D-14: XSS dentro code fence — anche lì escaped */
    void xssProtezioneCodeFence() {
        const QString html = AgentiPage::markdownToHtml("```\n<evil/>\n```");
        QVERIFY2(!html.contains("<evil/>"), "XSS non bloccato nel code fence");
    }

    /* D-15: stringa vuota → output vuoto o solo spazi/div vuoti */
    void inputVuoto() {
        const QString html = AgentiPage::markdownToHtml("");
        QVERIFY(!html.contains("<b>"));
        QVERIFY(!html.contains("<script>"));
    }

    /* D-16: code fence non chiusa → fallback <pre> con contenuto disponibile */
    void codeFenceNonChiusa() {
        const QString html = AgentiPage::markdownToHtml("```python\nprint('mai chiuso')");
        QVERIFY2(html.contains("<pre"), "code fence non chiusa deve avere fallback <pre>");
    }

    /* D-17 (TODO D-37): i blocchi codice devono usare white-space:pre-wrap,
       non pre — QTextBrowser non ha overflow-x per-blocco, una riga lunga
       non-wrappata forza la scrollbar orizzontale dell'intero log chat. */
    void codeFencePreWrap() {
        const QString html = AgentiPage::markdownToHtml(
            "```python\nx = 'riga molto lunga che non deve forzare lo scroll'\n```");
        QVERIFY2(html.contains("white-space:pre-wrap"),
                 "blocco codice senza pre-wrap: scrollbar orizzontale (D-37)");
        QVERIFY2(!html.contains("white-space:pre;"),
                 "white-space:pre residuo nel blocco codice (D-37)");
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-E — guardiaDataOra: ora/data dall'orologio di sistema,
   zero LLM e zero web. Full-match: frasi articolate passano all'AI.
   ══════════════════════════════════════════════════════════════ */
class TestGuardiaDataOra : public QObject {
    Q_OBJECT
private slots:

    /* E-1: domande secche sull'ora → risposta con HH:mm dall'orologio */
    void oraSecca() {
        const QStringList q = { "che ora sono?", "Che ore sono", "che ora e'?",
                                "che ora \xc3\xa8", "orario attuale", "ora attuale?" };
        const QString hh = QDateTime::currentDateTime().toString("HH:");
        for (const QString& s : q) {
            const QString r = AgentiPage::guardiaDataOra(s);
            QVERIFY2(!r.isEmpty(), qPrintable("non gestita: " + s));
            QVERIFY2(r.contains(hh), qPrintable("manca l'ora in: " + r));
        }
    }

    /* E-2: domande secche sulla data → risposta con l'anno corrente */
    void dataSecca() {
        const QStringList q = { "che giorno \xc3\xa8?", "che giorno e' oggi",
                                "data di oggi", "quando siamo?", "che anno \xc3\xa8" };
        const QString anno = QString::number(QDate::currentDate().year());
        for (const QString& s : q) {
            const QString r = AgentiPage::guardiaDataOra(s);
            QVERIFY2(!r.isEmpty(), qPrintable("non gestita: " + s));
            QVERIFY2(r.contains(anno), qPrintable("manca l'anno in: " + r));
        }
    }

    /* E-3: frasi articolate NON intercettate → vanno all'AI (con D-31) */
    void frasiArticolatePassano() {
        const QStringList q = {
            "che ora conviene partire per Roma?",
            "che giorno \xc3\xa8 meglio per il backup settimanale",
            "a che ora apre la farmacia",
            "dimmi che ore sono a New York",
            "meteo oggi", "quanto fa 5+5" };
        for (const QString& s : q)
            QVERIFY2(AgentiPage::guardiaDataOra(s).isEmpty(),
                     qPrintable("intercettata per errore: " + s));
    }

    /* E-4: input vuoto o solo punteggiatura → vuoto, nessun crash */
    void inputDegenere() {
        QVERIFY(AgentiPage::guardiaDataOra("").isEmpty());
        QVERIFY(AgentiPage::guardiaDataOra("???").isEmpty());
        QVERIFY(AgentiPage::guardiaDataOra("   ").isEmpty());
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-F — _inject_help: tabella comandi zero-LLM ("cosa sai fare?")
   con colonna ▶ Prova ({{PROVA:comando}} sostituito in runPipeline
   con link "prova:BASE64URL"). Free function di main_ai_tools.cpp.
   ══════════════════════════════════════════════════════════════ */
QString _inject_help(const QString& task);

class TestInjectHelp : public QObject {
    Q_OBJECT
private slots:

    /* F-1: domande secche sulle capacità → tabella HELP_MARKDOWN */
    void domandeSecche() {
        const QStringList q = { "Cosa sai fare?", "cosa posso fare",
                                "che cosa puoi fare?", "aiuto", "comandi",
                                "quali sono le tue funzioni",
                                "ciao, cosa sai fare?" };
        for (const QString& s : q) {
            const QString r = _inject_help(s);
            QVERIFY2(r.startsWith("HELP_MARKDOWN:"),
                     qPrintable("non gestita: " + s));
            QVERIFY2(r.contains("Cosa calcola"),
                     qPrintable("manca tabella in: " + s));
        }
    }

    /* F-2: frasi più lunghe NON intercettate → task invariato all'AI */
    void frasiLunghePassano() {
        const QStringList q = {
            "cosa posso fare per convertire un PDF in testo?",
            "cosa sai fare con i file CSV",
            "cosa sai della seconda guerra mondiale",
            "posso fare una domanda?" };
        for (const QString& s : q)
            QCOMPARE(_inject_help(s), s);
    }

    /* F-3: ogni riga della tabella ha il suo segnaposto ▶ Prova,
       incluso l'esempio citato dall'utente (area rettangolo) */
    void marcatoriProva() {
        const QString r = _inject_help("cosa sai fare");
        QVERIFY(r.contains(
            "{{PROVA:area di un rettangolo con base 5 e altezza 3}}"));
        QVERIFY2(r.count("{{PROVA:") >= 20,
                 qPrintable(QString("solo %1 segnaposto").arg(r.count("{{PROVA:"))));
        /* Il comando nel segnaposto non deve contenere & < > (attraversa
           escHtml prima della sostituzione col link) */
        static const QRegularExpression reBad(R"(\{\{PROVA:[^}]*[&<>][^}]*\}\})");
        QVERIFY(!reBad.match(r).hasMatch());
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-G — coerenza tabella help: OGNI esempio "▶ Prova" deve essere
   davvero gestito dalla catena di guardie locali sincrone di
   runPipeline (la tabella promette "zero token"). Replica la stessa
   sequenza: guardiaMath → guardiaDataOra → FormulaParser →
   _inject_science/date/finance/knowledge/generator/textstats/algo.
   ══════════════════════════════════════════════════════════════ */
QString _inject_science(const QString& task);
QString _inject_date_calc(const QString& task);
QString _inject_finance(const QString& task);
QString _inject_knowledge(const QString& task);
QString _inject_generator(const QString& task);
QString _inject_textstats(const QString& task);
QString _inject_algo(const QString& task);
QJsonObject _parseEventoRequest(const QString& task);
QString _icsEscapeText(QString s);
QString _buildGoogleCalendarIntentUrl(const QUrlQuery& query, const QUrl& httpsUrl);

class TestHelpExamplesLocal : public QObject {
    Q_OBJECT

    static QString whichGuard(const QString& cmd) {
        if (!AgentiPage::guardiaMath(cmd).isEmpty())    return "guardiaMath";
        if (!AgentiPage::guardiaDataOra(cmd).isEmpty()) return "guardiaDataOra";
        if (!FormulaParser::tryExtract(cmd).isEmpty())  return "FormulaParser";
        if (!_parseEventoRequest(cmd).isEmpty())        return "_parseEventoRequest";
        static const QString kTag = "[Calcolo locale:";
        if (_inject_science(cmd).startsWith(kTag))   return "_inject_science";
        if (_inject_date_calc(cmd).startsWith(kTag)) return "_inject_date_calc";
        if (_inject_finance(cmd).startsWith(kTag))   return "_inject_finance";
        if (_inject_knowledge(cmd).startsWith(kTag)) return "_inject_knowledge";
        if (_inject_generator(cmd).startsWith(kTag)) return "_inject_generator";
        if (_inject_textstats(cmd).startsWith(kTag)) return "_inject_textstats";
        if (_inject_algo(cmd).startsWith(kTag))      return "_inject_algo";
        return {};
    }

private slots:

    /* G-1: ogni {{PROVA:cmd}} della tabella zero-token risponde in locale.
       La sezione "Con l'aiuto del modello AI" (valuta, evento calendario)
       è esclusa: quei comandi passano legittimamente dal modello+tool. */
    void tuttiGliEsempiLocali() {
        QString md = _inject_help("cosa sai fare");
        QVERIFY(md.startsWith("HELP_MARKDOWN:"));
        const int cut = md.indexOf("Con l'aiuto del modello AI");
        QVERIFY2(cut > 0, "manca la sezione tool nella tabella help");
        md = md.left(cut);
        static const QRegularExpression reP(R"(\{\{PROVA:([^}]+)\}\})");
        auto it = reP.globalMatch(md);
        int checked = 0;
        QStringList falliti;
        while (it.hasNext()) {
            const QString cmd = it.next().captured(1);
            ++checked;
            if (whichGuard(cmd).isEmpty())
                falliti << cmd;
        }
        QVERIFY(checked >= 18);
        QVERIFY2(falliti.isEmpty(),
                 qPrintable("esempi NON gestiti in locale:\n  " + falliti.join("\n  ")));
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-H — _parseEventoRequest: parsing locale richiesta evento
   (titolo/data/orari/luogo) per il QR calendario zero-LLM.
   ══════════════════════════════════════════════════════════════ */
class TestParseEvento : public QObject {
    Q_OBJECT
private slots:

    /* H-1: richiesta completa → tutti i campi estratti */
    void richiestaCompleta() {
        const QJsonObject ev = _parseEventoRequest(
            "creami un evento festa in maschera il 31/10/2026 dalle 21 alle 23");
        QVERIFY(!ev.isEmpty());
        QCOMPARE(ev["titolo"].toString(), QString("Festa in maschera"));
        QCOMPARE(ev["data"].toString(), QString("2026-10-31"));
        QCOMPARE(ev["ora_inizio"].toString(), QString("21:00"));
        QCOMPARE(ev["ora_fine"].toString(), QString("23:00"));
    }

    /* H-2: compleanno con mese in lettere e luogo */
    void meseInLettereELuogo() {
        const QJsonObject ev = _parseEventoRequest(
            "genera un evento compleanno di Anna il 12 agosto 2026 alle 18:30 presso casa di Anna");
        QVERIFY(!ev.isEmpty());
        QCOMPARE(ev["data"].toString(), QString("2026-08-12"));
        QCOMPARE(ev["ora_inizio"].toString(), QString("18:30"));
        QCOMPARE(ev["luogo"].toString(), QString("casa di Anna"));
        QVERIFY(ev["titolo"].toString().startsWith("Compleanno di Anna"));
    }

    /* H-3: senza data → oggetto valido ma SENZA chiave "data" (si chiede) */
    void senzaData() {
        const QJsonObject ev = _parseEventoRequest("creami un evento per il compleanno");
        QVERIFY(!ev.isEmpty());
        QVERIFY(!ev.contains("data"));
        QCOMPARE(ev["titolo"].toString(), QString("Compleanno"));
    }

    /* H-4: data senza anno già passata quest'anno → anno prossimo */
    void annoProssimoSePassata() {
        const QDate ieri = QDate::currentDate().addDays(-1);
        const QJsonObject ev = _parseEventoRequest(
            QString("creami un evento test il %1/%2")
                .arg(ieri.day(), 2, 10, QChar('0'))
                .arg(ieri.month(), 2, 10, QChar('0')));
        QVERIFY(ev.contains("data"));
        const QDate d = QDate::fromString(ev["data"].toString(), "yyyy-MM-dd");
        QVERIFY2(d > QDate::currentDate(), "data nel passato non spostata avanti");
    }

    /* H-5: frasi NON evento → oggetto vuoto */
    void nonEvento() {
        QVERIFY(_parseEventoRequest("che eventi ci sono domani a Roma?").isEmpty());
        QVERIFY(_parseEventoRequest("quanto fa 5+5").isEmpty());
        QVERIFY(_parseEventoRequest("parlami degli eventi storici del 1946").isEmpty());
        QVERIFY(_parseEventoRequest("").isEmpty());
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-I — _icsEscapeText: escaping RFC 5545 §3.3.11 per i campi
   testo del file/QR .ics generato da crea_evento_calendario.
   ══════════════════════════════════════════════════════════════ */
class TestIcsEscapeText : public QObject {
    Q_OBJECT
private slots:

    /* I-1: caratteri semplici (nessuno dei 4 speciali) → invariati */
    void testoSemplicePassaInvariato() {
        QCOMPARE(_icsEscapeText("Compleanno di Marco"), QString("Compleanno di Marco"));
    }

    /* I-2: virgola e punto e virgola → escapati con backslash */
    void virgolaEPuntoEVirgola() {
        QCOMPARE(_icsEscapeText("Milano, sala A; ingresso B"),
                  QString("Milano\\, sala A\\; ingresso B"));
    }

    /* I-3: newline (sia \n che \r\n) → sequenza letterale \n */
    void newlineDiventaSequenzaLetterale() {
        QCOMPARE(_icsEscapeText("riga1\nriga2"), QString("riga1\\nriga2"));
        QCOMPARE(_icsEscapeText("riga1\r\nriga2"), QString("riga1\\nriga2"));
    }

    /* I-4: backslash letterale nel testo utente → raddoppiato, e va
       fatto PRIMA degli altri escape (altrimenti '\;' inserito per un
       ';' verrebbe ri-raddoppiato in '\\;' un secondo giro). */
    void backslashRaddoppiatoPrimaDegliAltri() {
        QCOMPARE(_icsEscapeText("C:\\Users\\Mario"), QString("C:\\\\Users\\\\Mario"));
        QCOMPARE(_icsEscapeText("a;b"), QString("a\\;b"));
    }

    /* I-5: stringa vuota → invariata, nessun crash */
    void stringaVuota() {
        QCOMPARE(_icsEscapeText(""), QString(""));
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-J — _buildGoogleCalendarIntentUrl: URL Android intent:// che apre
   l'app Google Calendar direttamente (invece del solo link https, che
   su Android spesso apre il browser anche con l'app installata).
   ══════════════════════════════════════════════════════════════ */
class TestGoogleCalendarIntentUrl : public QObject {
    Q_OBJECT
private slots:

    /* J-1: struttura base — prefisso intent://, pacchetto Android giusto,
       suffisso ;end, e la query prima di #Intent combacia con quella https */
    void strutturaIntentCorretta() {
        QUrlQuery q;
        q.addQueryItem("action", "TEMPLATE");
        q.addQueryItem("text", "Compleanno di Marco");
        q.addQueryItem("dates", "20270315T200000/20270315T210000");
        QUrl httpsUrl("https://calendar.google.com/calendar/render");
        httpsUrl.setQuery(q);

        const QString intent = _buildGoogleCalendarIntentUrl(q, httpsUrl);

        QVERIFY(intent.startsWith("intent://calendar.google.com/calendar/render?"));
        QVERIFY2(intent.contains("#Intent;scheme=https;package=com.google.android.calendar;"),
                 qPrintable(intent));
        QVERIFY(intent.endsWith(";end"));

        const QString query = intent.mid(
            QString("intent://calendar.google.com/calendar/render?").length(),
            intent.indexOf("#Intent") - QString("intent://calendar.google.com/calendar/render?").length());
        QCOMPARE(query, q.query(QUrl::FullyEncoded));
    }

    /* J-2: il fallback per Android (browser_fallback_url), decodificato,
       deve tornare ESATTAMENTE l'URL https originale — è la garanzia che
       chi non ha l'app finisce comunque sulla pagina giusta, non su un
       link rotto/troncato dall'escaping. */
    void fallbackDecodificaAllUrlOriginale() {
        QUrlQuery q;
        q.addQueryItem("action", "TEMPLATE");
        q.addQueryItem("text", "Riunione, sala A & B");   // caratteri che vanno percent-encoded
        QUrl httpsUrl("https://calendar.google.com/calendar/render");
        httpsUrl.setQuery(q);

        const QString intent = _buildGoogleCalendarIntentUrl(q, httpsUrl);
        static const QString marker = "S.browser_fallback_url=";
        const int start = intent.indexOf(marker) + marker.length();
        const int end   = intent.indexOf(";end");
        QVERIFY(start > 0 && end > start);
        const QString fallbackEncoded = intent.mid(start, end - start);

        QCOMPARE(QUrl::fromPercentEncoding(fallbackEncoded.toUtf8()), httpsUrl.toString());
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-K (D-16) — 7 nuove "domande classiche" zero-LLM: statistica su
   lista, data di Pasqua (Gauss), fase lunare, base numerica arbitraria,
   progressione aritmetica/geometrica, lira↔euro, scadenza "tra X anni".
   Valori attesi calcolati/verificati indipendentemente (Python standalone
   per Pasqua e lira/euro) PRIMA di scrivere questi assert, non dedotti
   dal codice C++ stesso.
   ══════════════════════════════════════════════════════════════ */
class TestNuoveDomandeD16 : public QObject {
    Q_OBJECT
private slots:

    /* K-1: media/mediana/moda/deviazione standard su [3,5,7,9,2] */
    void statisticaSuLista() {
        const QString r = _inject_science("media di 3,5,7,9,2");
        QVERIFY2(r.startsWith("[Calcolo locale:"), qPrintable(r));
        QVERIFY2(r.contains("media=5.2"), qPrintable(r));
        QVERIFY2(r.contains("mediana=5"), qPrintable(r));
        QVERIFY2(r.contains("nessuna (valori tutti distinti)"), qPrintable(r));
        QVERIFY2(r.contains("deviazione standard (popolazione)=2.5612"), qPrintable(r));
    }

    /* K-1b: lista con moda reale (valore ripetuto) */
    void statisticaConModa() {
        const QString r = _inject_science("moda di 1,2,2,3,4");
        QVERIFY2(r.startsWith("[Calcolo locale:"), qPrintable(r));
        QVERIFY2(r.contains("moda=2"), qPrintable(r));
    }

    /* K-1c: due soli numeri — NON deve scattare (ambiguo con un decimale
       tipo "3,5"), coerente con l'euristica già validata da D-24 */
    void statisticaDueNumeriNonScatta() {
        const QString r = _inject_science("media di 3,5");
        QVERIFY2(!r.startsWith("[Calcolo locale:"), qPrintable(r));
    }

    /* K-2: Pasqua — 7 anni noti pubblicamente, verificati con Python
       standalone (algoritmo Meeus/Jones/Butcher) prima di scrivere il C++ */
    void dataDiPasqua() {
        struct Caso { int anno; QString giorno; };
        static const QVector<Caso> kCasi = {
            {2024, "31 marzo 2024"}, {2025, "20 aprile 2025"}, {2026, "5 aprile 2026"},
            {2027, "28 marzo 2027"}, {2028, "16 aprile 2028"}, {2000, "23 aprile 2000"},
            {1994, "3 aprile 1994"},
        };
        for (const auto& c : kCasi) {
            const QString r = _inject_date_calc(QString("quando \xc3\xa8 pasqua nel %1").arg(c.anno));
            QVERIFY2(r.startsWith("[Calcolo locale:"), qPrintable(QString("anno %1: %2").arg(c.anno).arg(r)));
            QVERIFY2(r.contains(c.giorno), qPrintable(QString("anno %1 atteso '%2' in: %3").arg(c.anno).arg(c.giorno, r)));
        }
    }

    /* K-3: fase lunare — verifica solo che scatti e produca una delle 8
       fasi note + percentuale di illuminazione (approssimazione dichiarata
       esplicitamente in output, non un valore esatto da validare al giorno) */
    void faseLunare() {
        const QString r = _inject_date_calc("fase lunare del 15/03/2026");
        QVERIFY2(r.startsWith("[Calcolo locale:"), qPrintable(r));
        QVERIFY2(r.contains("approssimata"), qPrintable(r));
        static const QStringList kNomi = {
            "Luna nuova", "Falce crescente", "Primo quarto", "Gibbosa crescente",
            "Luna piena", "Gibbosa calante", "Ultimo quarto", "Falce calante"
        };
        bool trovata = false;
        for (const QString& n : kNomi) if (r.contains(n)) { trovata = true; break; }
        QVERIFY2(trovata, qPrintable(r));
        QVERIFY2(r.contains("illuminazione"), qPrintable(r));
    }

    /* K-4: conversione in base arbitraria — 255 in base 7 = 513
       (5×49+1×7+3 = 255, verificato a mano) */
    void baseNumericaArbitraria() {
        const QString r = _inject_science("converti 255 in base 7");
        QVERIFY2(r.startsWith("[Calcolo locale:"), qPrintable(r));
        QVERIFY2(r.contains("513"), qPrintable(r));
        QVERIFY2(r.contains("base 7"), qPrintable(r));
    }

    /* K-5: progressione aritmetica — a1=2, d=3, n=10 → S=155
       (n/2×(2a1+(n-1)d) = 5×(4+27) = 155) */
    void progressioneAritmetica() {
        const QString r = _inject_science("progressione aritmetica primo 2 ragione 3 termini 10");
        QVERIFY2(r.startsWith("[Calcolo locale:"), qPrintable(r));
        QVERIFY2(r.contains("= 155"), qPrintable(r));
    }

    /* K-6: progressione geometrica — a1=2, r=3, n=5 → S=242
       (2×(3^5-1)/(3-1) = 2×242/2 = 242) */
    void progressioneGeometrica() {
        const QString r = _inject_science("progressione geometrica primo 2 ragione 3 termini 5");
        QVERIFY2(r.startsWith("[Calcolo locale:"), qPrintable(r));
        QVERIFY2(r.contains("= 242"), qPrintable(r));
    }

    /* K-7: lira→euro, tasso fisso 1936.27 — 1 000 000 lire = 516.46 € */
    void liraEuro() {
        const QString r1 = _inject_finance("quanto sono 1000000 lire in euro");
        QVERIFY2(r1.startsWith("[Calcolo locale:"), qPrintable(r1));
        QVERIFY2(r1.contains("516.46"), qPrintable(r1));

        /* 516.46 non è l'inverso esatto di 1000000/1936.27 (troncato a 2
           decimali) — il valore atteso è quello ricalcolato indipendentemente
           con Python (516.46 × 1936.27 = 1000006.00), non un round-trip perfetto */
        const QString r2 = _inject_finance("quanto sono 516.46 euro in lire");
        QVERIFY2(r2.startsWith("[Calcolo locale:"), qPrintable(r2));
        QVERIFY2(r2.contains("1000006"), qPrintable(r2));
    }

    /* K-8: scadenza "tra X anni da oggi" — calcolato dinamicamente da
       QDate::currentDate(), mai una data fissa (il test deve restare
       valido indipendentemente da quando viene eseguito) */
    void scadenzaTraXAnni() {
        const QDate atteso = QDate::currentDate().addYears(3);
        const QString r = _inject_date_calc("tra 3 anni da oggi che data sar\xc3\xa0");
        QVERIFY2(r.startsWith("[Calcolo locale:"), qPrintable(r));
        QVERIFY2(r.contains(QLocale(QLocale::Italian).toString(atteso, "d MMMM yyyy")), qPrintable(r));
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-L — verifica TODO.md "DA TESTARE MANUALMENTE" T-D14a÷T-D14d e
   T-D17: tutte le frasi elencate sono gestite da guardie zero-LLM pure
   (nessuna GUI, nessuna richiesta reale a un modello AI) — verificabili
   chiamando le funzioni direttamente, stesso approccio di CAT-K. Valori
   attesi calcolati/verificati indipendentemente con Python PRIMA di
   scrivere questi assert (edit distance, LCS, giorno della settimana,
   giorni lavorativi, checksum P.IVA) o letti dal codice sorgente delle
   funzioni del Simulatore (Fibonacci/Catalan/fattorizzazione/profitto
   azioni/inversioni/ricerca lineare), non dedotti dal solo output atteso.
   ══════════════════════════════════════════════════════════════ */
class TestManualCoverageD14D17 : public QObject {
    Q_OBJECT
private slots:

    /* T-D14a: età (dinamica, calcolata da QDate::currentDate() come fa la
       guardia stessa — mai una data fissa), giorno della settimana
       (25/12/2020 = venerdì, verificato con Python datetime),
       fuso orario (dinamico, verifica solo struttura), giorni lavorativi
       (03/07/2026→15/08/2026 = 31, verificato con Python) */
    void td14a_etaGiornoFusoLavorativi() {
        {
            const QString r = _inject_date_calc("quanti anni ho se sono nato il 15/03/1990");
            QVERIFY2(r.startsWith("[Calcolo locale:"), qPrintable(r));
            QVERIFY2(r.contains("1990"), qPrintable(r));
            QVERIFY2(r.contains("anni"), qPrintable(r));
        }
        {
            const QString r = _inject_date_calc("che giorno era il 25/12/2020");
            QVERIFY2(r.startsWith("[Calcolo locale:"), qPrintable(r));
            QVERIFY2(r.contains("venerd\xc3\xac"), qPrintable(r));
        }
        {
            const QString r = _inject_date_calc("che ora \xc3\xa8 a New York");
            QVERIFY2(r.startsWith("[Calcolo locale:"), qPrintable(r));
            QVERIFY2(r.contains("New York"), qPrintable(r));
            QVERIFY2(QRegularExpression(R"(\d{2}:\d{2})").match(r).hasMatch(), qPrintable(r));
        }
        {
            const QString r = _inject_date_calc("giorni lavorativi tra 03/07/2026 e 15/08/2026");
            QVERIFY2(r.startsWith("[Calcolo locale:"), qPrintable(r));
            QVERIFY2(r.contains("31 giorni lavorativi"), qPrintable(r));
        }
    }

    /* T-D14b: numeri romani (round-trip 1994<->MCMXCIV), IMC (70kg/1.75m
       = 22.857..., fascia normopeso), geometria (rettangolo 5x3=15,
       cilindro r=3 h=5 -> pi*9*5=141.37) */
    void td14b_romaniImcGeometria() {
        {
            const QString r = _inject_science("quanto \xc3\xa8 MCMXCIV");
            QVERIFY2(r.startsWith("[Calcolo locale:"), qPrintable(r));
            QVERIFY2(r.contains("1994"), qPrintable(r));
        }
        {
            const QString r = _inject_science("1994 in romano");
            QVERIFY2(r.startsWith("[Calcolo locale:"), qPrintable(r));
            QVERIFY2(r.contains("MCMXCIV"), qPrintable(r));
        }
        {
            const QString r = _inject_science("imc per 70kg e 1.75m");
            QVERIFY2(r.startsWith("[Calcolo locale:"), qPrintable(r));
            QVERIFY2(r.contains("22.8571"), qPrintable(r));   /* _sci() non arrotonda a 2 decimali, ne tiene fino a 4 */
            QVERIFY2(r.contains("normopeso"), qPrintable(r));
        }
        {
            const QString r = _inject_science("area di un rettangolo con base 5 e altezza 3");
            QVERIFY2(r.startsWith("[Calcolo locale:"), qPrintable(r));
            QVERIFY2(r.contains("15"), qPrintable(r));
        }
        {
            const QString r = _inject_science("volume di un cilindro con raggio 3 e altezza 5");
            QVERIFY2(r.startsWith("[Calcolo locale:"), qPrintable(r));
            QVERIFY2(r.contains("141.37"), qPrintable(r));
        }
    }

    /* T-D14c: P.IVA 12345678903 valida (checksum verificato con Python),
       interesse composto 1000€/5%/10anni=1628.89€ (stesso valore già
       usato come esempio in D-33), mutuo 100000€/3%/20anni=554.60€/mese
       (idem) */
    void td14c_pivaInteresseMutuo() {
        {
            const QString r = _inject_finance("12345678903 \xc3\xa8 una p.iva valida?");
            QVERIFY2(r.startsWith("[Calcolo locale:"), qPrintable(r));
            QVERIFY2(r.contains("VALIDA"), qPrintable(r));
        }
        {
            const QString r = _inject_finance("quanto diventano 1000 euro al 5% in 10 anni");
            QVERIFY2(r.startsWith("[Calcolo locale:"), qPrintable(r));
            QVERIFY2(r.contains("1628.89"), qPrintable(r));
        }
        {
            const QString r = _inject_finance("rata di un mutuo da 100000 euro al 3% in 20 anni");
            QVERIFY2(r.startsWith("[Calcolo locale:"), qPrintable(r));
            QVERIFY2(r.contains("554.60"), qPrintable(r));
        }
    }

    /* T-D14d: conteggio parole — "Nel mezzo del cammin di nostra vita"
       = 7 parole (contate a mano: Nel/mezzo/del/cammin/di/nostra/vita) */
    void td14d_statisticheTesto() {
        const QString r = _inject_textstats("quante parole ha: Nel mezzo del cammin di nostra vita");
        QVERIFY2(r.startsWith("[Calcolo locale:"), qPrintable(r));
        QVERIFY2(r.contains("7 parole"), qPrintable(r));
    }

    /* T-D17: le 10 frasi elencate nel TODO, tutte con esito atteso letto
       dal codice sorgente delle funzioni SimulatorePage::gen* invocate da
       _inject_algo (non indovinato) o verificato con Python standalone
       (edit distance, LCS) prima di scrivere l'assert. Verifica anche che
       il regex riconosca DAVVERO la frase (il TODO segnalava "alta
       probabilit\xc3\xa0 che qualche formulazione non matchi al primo colpo"). */
    void td17_algoritmiSimulatore() {
        struct Caso { QString frase; QString atteso; };
        static const QVector<Caso> kCasi = {
            {"mcd tra 48 e 18", "MCD(48,18) = 6"},
            {"fattorizzazione di 360", QString::fromUtf8("360 = 2\xc3\x97" "2\xc3\x97" "2\xc3\x97" "3\xc3\x97" "3\xc3\x97" "5")},
            {"decimo numero di fibonacci", "= 55"},
            {"numero di catalan 5", "Catalan(5) = 42"},
            {"torre di hanoi con 8 dischi", "255 mosse"},
            {"profitto massimo con prezzi 7,1,5,3,6,4", "+5"},
            {"quante inversioni in 3,1,2", "2 inversioni"},
            {"in che posizione \xc3\xa8 7 in 3,7,9,2", "posizione [1]"},
            {"distanza di edit tra 'gatto' e 'catto'", "= 1"},
            {"sottosequenza comune pi\xc3\xb9 lunga tra 'ABCBDAB' e 'BDCABA'", "lunghezza 4"},
        };
        for (const auto& c : kCasi) {
            const QString r = _inject_algo(c.frase);
            QVERIFY2(r.startsWith("[Calcolo locale:"),
                     qPrintable(QString("frase NON riconosciuta da _inject_algo: '%1' — output: %2").arg(c.frase, r)));
            QVERIFY2(r.contains(c.atteso),
                     qPrintable(QString("frase '%1': atteso '%2' in: %3").arg(c.frase, c.atteso, r)));
        }
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-M — verifica TODO.md T-help: la tabella markdown di _inject_help()
   deve renderizzare come vera <table> HTML (non testo grezzo con pipe)
   e l'esempio di grafico deve cambiare ad ogni richiesta. Replica ESATTAMENTE
   il flusso di produzione (main_ai_pipeline.cpp: markdownToHtml(helpMd.mid(
   kHelpTag.length()))) — nessuna GUI necessaria, markdownToHtml() è statica.
   ══════════════════════════════════════════════════════════════ */
class TestHelpTableRendering : public QObject {
    Q_OBJECT
private slots:

    /* M-1: la tabella markdown diventa una vera <table> HTML, non pipe grezze */
    void tabellaRenderizzataComeHtml() {
        const QString helpMd = _inject_help("cosa sai fare");
        static const QString kHelpTag = "HELP_MARKDOWN:";
        QVERIFY2(helpMd.startsWith(kHelpTag), qPrintable(helpMd.left(50)));
        const QString html = AgentiPage::markdownToHtml(helpMd.mid(kHelpTag.length()));

        QVERIFY2(html.contains("<table"), "manca <table> — la tabella non è stata convertita");
        QVERIFY2(html.contains("<thead>") && html.contains("<tr>"),
                 "manca <thead>/<tr> — intestazione tabella non generata");
        /* Il markdown originale ha "|---|---|---|---|" per separare header e
           corpo — se questo sopravvive letteralmente nell'HTML, il rendering
           ha fallito e l'utente vedrebbe testo grezzo con pipe/trattini */
        QVERIFY2(!html.contains("|---|"), "riga separatore markdown '|---|' non convertita — testo grezzo");
        QVERIFY2(!html.contains("| \xf0\x9f\x94\xa2 Matematica"),
                 "riga pipe grezza sopravvissuta nell'HTML — tabella non renderizzata");
    }

    /* M-2: l'esempio di grafico cambia tra richieste diverse (QRandomGenerator,
       8 formule possibili) — su 30 chiamate ci si aspetta quasi certamente
       più di un esempio distinto (statisticamente: P(sempre lo stesso) = (1/8)^29) */
    void esempioGraficoCambia() {
        static const QString kHelpTag = "HELP_MARKDOWN:";
        static const QRegularExpression reGrafico(
            R"(\{\{PROVA:(grafico di [^}]+|y = [^}]+)\}\})");
        QSet<QString> esempi;
        for (int i = 0; i < 30; ++i) {
            const QString helpMd = _inject_help("cosa sai fare");
            const auto m = reGrafico.match(helpMd.mid(kHelpTag.length()));
            QVERIFY2(m.hasMatch(), "riga Grafico con segnaposto {{PROVA:...}} non trovata");
            esempi.insert(m.captured(1));
        }
        QVERIFY2(esempi.size() > 1,
                 qPrintable(QString("solo %1 esempio/i distinto/i su 30 chiamate — l'esempio non cambia")
                            .arg(esempi.size())));
    }
};

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    int status = 0;
    {
        TestUserBubble          t1; status |= QTest::qExec(&t1, argc, argv);
        TestAgentBubble         t2; status |= QTest::qExec(&t2, argc, argv);
        TestLocalBubble         t3; status |= QTest::qExec(&t3, argc, argv);
        TestMarkdownToHtml      t4; status |= QTest::qExec(&t4, argc, argv);
        TestGuardiaDataOra      t5; status |= QTest::qExec(&t5, argc, argv);
        TestInjectHelp          t6; status |= QTest::qExec(&t6, argc, argv);
        TestHelpExamplesLocal   t7; status |= QTest::qExec(&t7, argc, argv);
        TestParseEvento         t8; status |= QTest::qExec(&t8, argc, argv);
        TestIcsEscapeText       t9; status |= QTest::qExec(&t9, argc, argv);
        TestGoogleCalendarIntentUrl t10; status |= QTest::qExec(&t10, argc, argv);
        TestNuoveDomandeD16     t11; status |= QTest::qExec(&t11, argc, argv);
        TestManualCoverageD14D17 t12; status |= QTest::qExec(&t12, argc, argv);
        TestHelpTableRendering   t13; status |= QTest::qExec(&t13, argc, argv);
    }
    return status;
}

#include "test_agenti_pipeline.moc"
