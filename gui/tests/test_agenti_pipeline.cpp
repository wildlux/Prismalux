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
    }
    return status;
}

#include "test_agenti_pipeline.moc"
