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
#include "../pages/agenti_page.h"

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

    /* A-2: testo normalizzato in minuscolo */
    void testoInMinuscolo() {
        const QString html = AgentiPage::buildUserBubble("HELLO WORLD");
        QVERIFY2(html.contains("hello world"),
                 "buildUserBubble deve normalizzare in minuscolo");
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

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    int status = 0;
    {
        TestUserBubble     t1; status |= QTest::qExec(&t1, argc, argv);
        TestAgentBubble    t2; status |= QTest::qExec(&t2, argc, argv);
        TestLocalBubble    t3; status |= QTest::qExec(&t3, argc, argv);
        TestMarkdownToHtml t4; status |= QTest::qExec(&t4, argc, argv);
    }
    return status;
}

#include "test_agenti_pipeline.moc"
