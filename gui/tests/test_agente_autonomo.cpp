/* ══════════════════════════════════════════════════════════════
   test_agente_autonomo.cpp — Suite test Agente Autonomo ReAct

   Categorie:
     CAT-A  buildAutoStepHtml — card HTML step ReAct
     CAT-B  _autoSystemPrompt — istruzioni ReAct
     CAT-C  detectFirstToolCall — parsing JSON tool call
     CAT-D  Toggle UI Chat → Agente Autonomo (no Ollama)
     CAT-E  Parsing risposta ReAct via MockAiClient
     CAT-F  Step limit e gestione JSON malformato

   Build:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_agente_autonomo
     ./build_tests/test_agente_autonomo

   Requisiti: nessuna connessione di rete (tutto mock)
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QApplication>
#include <QPushButton>
#include <QTextBrowser>
#include <QTextEdit>
#include <QJsonObject>
#include <QJsonDocument>
#include <QSignalSpy>

#include "mock_ai_client.h"
#include "../pages/agenti_page.h"

/* ════════════════════════════════════════════════════════════════
   CAT-A — buildAutoStepHtml
   ════════════════════════════════════════════════════════════════ */
class TestBuildAutoStepHtml : public QObject {
    Q_OBJECT
private slots:

    /* A-1: numero step presente nell'HTML */
    void stepNumberPresente() {
        const QString html = AgentiPage::buildAutoStepHtml(3, "penso", "act", "obs");
        QVERIFY2(html.contains("Step 3"), "numero step assente nell'HTML");
    }

    /* A-2: sezione THOUGHT presente quando non vuota */
    void thoughtPresenteSeNonVuoto() {
        const QString html = AgentiPage::buildAutoStepHtml(1, "ragionamento", "", "");
        QVERIFY2(html.contains("THOUGHT"), "sezione THOUGHT assente");
        QVERIFY2(html.contains("ragionamento"), "testo thought assente");
    }

    /* A-3: sezione ACTION presente quando non vuota */
    void actionPresenteSeNonVuoto() {
        const QString html = AgentiPage::buildAutoStepHtml(1, "", "calc(2+2)", "");
        QVERIFY2(html.contains("ACTION"), "sezione ACTION assente");
        QVERIFY2(html.contains("calc(2+2)"), "testo action assente");
    }

    /* A-4: sezione OBSERVATION presente quando non vuota */
    void observationPresenteSeNonVuota() {
        const QString html = AgentiPage::buildAutoStepHtml(1, "", "", "risultato: 4");
        QVERIFY2(html.contains("OBSERVATION"), "sezione OBSERVATION assente");
        QVERIFY2(html.contains("risultato: 4"), "testo observation assente");
    }

    /* A-5: XSS escaping — < > & nel thought/action/obs */
    void xssEscaping() {
        const QString html = AgentiPage::buildAutoStepHtml(
            1, "<script>", "{\"k\":\"<v>\"}", "<b>obs</b>");
        QVERIFY2(!html.contains("<script>"),   "tag <script> non escaped nel thought");
        QVERIFY2(!html.contains("<b>obs</b>"), "tag <b> non escaped nell'observation");
        QVERIFY2(html.contains("&lt;script&gt;") || html.contains("&lt;"), "manca escaping lt");
    }

    /* A-6: sezioni vuote non appaiono nell'HTML */
    void sezioniVuoteAssenti() {
        const QString html = AgentiPage::buildAutoStepHtml(1, "", "", "");
        QVERIFY2(!html.contains("THOUGHT"),     "THOUGHT appare con stringa vuota");
        QVERIFY2(!html.contains("ACTION"),      "ACTION appare con stringa vuota");
        QVERIFY2(!html.contains("OBSERVATION"), "OBSERVATION appare con stringa vuota");
    }

    /* A-7: observation troncata a 400 caratteri */
    void observationTroncata() {
        const QString lungo = QString("x").repeated(600);
        const QString html  = AgentiPage::buildAutoStepHtml(1, "", "", lungo);
        /* la stringa orig era 600 char, nell'HTML deve comparire al max 400 di "x" */
        const int idx  = html.indexOf("OBSERVATION");
        QVERIFY2(idx >= 0, "sezione OBSERVATION assente");
        const QString dopo = html.mid(idx);
        QVERIFY2(!dopo.contains(QString("x").repeated(401)),
                 "observation non troncata a 400 char");
    }

    /* A-8: multipli step producono HTML distinto */
    void multipleStepHtmlDistinto() {
        const QString h1 = AgentiPage::buildAutoStepHtml(1, "t1", "a1", "o1");
        const QString h2 = AgentiPage::buildAutoStepHtml(2, "t2", "a2", "o2");
        QVERIFY2(h1 != h2, "due step diversi producono HTML identico");
        QVERIFY2(h1.contains("Step 1") && h2.contains("Step 2"),
                 "numeri step non distinti");
    }
};

/* ════════════════════════════════════════════════════════════════
   CAT-B — _autoSystemPrompt
   ════════════════════════════════════════════════════════════════ */
class TestAutoSystemPrompt : public QObject {
    Q_OBJECT
private slots:

    /* B-1: contiene il tag THOUGHT */
    void contieneTHOUGHT() {
        QVERIFY2(AgentiPage::_autoSystemPrompt().contains("THOUGHT"),
                 "THOUGHT assente dal system prompt");
    }

    /* B-2: contiene il tag ACTION */
    void contieneACTION() {
        QVERIFY2(AgentiPage::_autoSystemPrompt().contains("ACTION"),
                 "ACTION assente dal system prompt");
    }

    /* B-3: contiene FINAL_ANSWER */
    void contieneFINAL_ANSWER() {
        QVERIFY2(AgentiPage::_autoSystemPrompt().contains("FINAL_ANSWER"),
                 "FINAL_ANSWER assente dal system prompt");
    }

    /* B-4: lista tutti e 6 gli strumenti */
    void contieneTuttiGliStrumenti() {
        const QString sys = AgentiPage::_autoSystemPrompt();
        QVERIFY2(sys.contains("calc"),        "strumento 'calc' assente");
        QVERIFY2(sys.contains("ricerca"),     "strumento 'ricerca' assente");
        QVERIFY2(sys.contains("python"),      "strumento 'python' assente");
        QVERIFY2(sys.contains("leggi_file"),  "strumento 'leggi_file' assente");
        QVERIFY2(sys.contains("lista_file"),  "strumento 'lista_file' assente");
        QVERIFY2(sys.contains("scrivi_file"), "strumento 'scrivi_file' assente");
    }

    /* B-5: formato JSON dell'ACTION specificato */
    void contieneFormatoJsonAction() {
        const QString sys = AgentiPage::_autoSystemPrompt();
        /* Il prompt deve mostrare {"tool": ... } come formato atteso */
        QVERIFY2(sys.contains("tool") && sys.contains("input"),
                 "formato JSON {tool/input} assente dal system prompt");
    }
};

/* ════════════════════════════════════════════════════════════════
   CAT-C — detectFirstToolCall
   Nota: questa funzione riconosce il tag "TOOL_CALL:" usato dalla
   modalità Tool Use (m_toolsEnabled). Il ciclo ReAct (Agente Autonomo)
   usa invece il parser regex inline in _autoAdvance con "ACTION:".
   ════════════════════════════════════════════════════════════════ */
class TestDetectFirstToolCall : public QObject {
    Q_OBJECT
private slots:

    /* C-1: rileva un tool call JSON valido con tag TOOL_CALL: */
    void rilevaToolCallValido() {
        const QString testo =
            "Ecco la risposta:\n"
            "TOOL_CALL: {\"tool\": \"calc\", \"input\": \"sqrt(144)\"}";
        const QJsonObject tc = AgentiPage::detectFirstToolCall(testo);
        QVERIFY2(!tc.isEmpty(), "TOOL_CALL non rilevata");
        QCOMPARE(tc["tool"].toString(), QStringLiteral("calc"));
        QCOMPARE(tc["input"].toString(), QStringLiteral("sqrt(144)"));
    }

    /* C-2: JSON malformato → oggetto vuoto */
    void jsonMalformatoRitornaVuoto() {
        const QString testo = "TOOL_CALL: {tool: calc input: sqrt}";
        const QJsonObject tc = AgentiPage::detectFirstToolCall(testo);
        QVERIFY2(tc.isEmpty(), "JSON malformato non deve restituire tool call");
    }

    /* C-3: testo senza TOOL_CALL → oggetto vuoto */
    void nessunToolCallRitornaVuoto() {
        const QString testo = "Solo una risposta normale senza tool call";
        const QJsonObject tc = AgentiPage::detectFirstToolCall(testo);
        QVERIFY2(tc.isEmpty(), "testo senza TOOL_CALL non deve restituire risultato");
    }

    /* C-4: tool ricerca */
    void rilevaToolRicerca() {
        const QString testo =
            "TOOL_CALL: {\"tool\": \"ricerca\", \"input\": \"Qt6 tutorial\"}";
        const QJsonObject tc = AgentiPage::detectFirstToolCall(testo);
        QVERIFY2(!tc.isEmpty(), "tool ricerca non rilevata");
        QCOMPARE(tc["tool"].toString(), QStringLiteral("ricerca"));
    }

    /* C-5: tool python */
    void rilevaToolPython() {
        const QString testo =
            "TOOL_CALL: {\"tool\": \"python\", \"input\": \"print(2+2)\"}";
        const QJsonObject tc = AgentiPage::detectFirstToolCall(testo);
        QVERIFY2(!tc.isEmpty(), "tool python non rilevata");
        QCOMPARE(tc["tool"].toString(), QStringLiteral("python"));
    }

    /* C-6: prende la PRIMA TOOL_CALL se ce ne sono più di una */
    void primaToolCallSeMultiple() {
        const QString testo =
            "TOOL_CALL: {\"tool\": \"calc\",    \"input\": \"1+1\"}\n"
            "TOOL_CALL: {\"tool\": \"ricerca\", \"input\": \"secondo\"}";
        const QJsonObject tc = AgentiPage::detectFirstToolCall(testo);
        QVERIFY2(!tc.isEmpty(), "nessuna tool call trovata");
        QCOMPARE(tc["tool"].toString(), QStringLiteral("calc"));
    }

    /* C-7: ACTION: senza TOOL_CALL: non è riconosciuta da questa funzione */
    void actionTagNonRiconosciuto() {
        const QString testo =
            "ACTION: {\"tool\": \"calc\", \"input\": \"2+2\"}";
        const QJsonObject tc = AgentiPage::detectFirstToolCall(testo);
        /* detectFirstToolCall usa TOOL_CALL:, NON ACTION: (quello è del ReAct) */
        QVERIFY2(tc.isEmpty(), "ACTION: non deve essere riconosciuto da detectFirstToolCall");
    }
};

/* ════════════════════════════════════════════════════════════════
   CAT-D — Toggle UI Chat ↔ Agente Autonomo (no Ollama)
   ════════════════════════════════════════════════════════════════ */
class TestToggleUiAgente : public QObject {
    Q_OBJECT
private:
    MockAiClient* m_ai  = nullptr;
    AgentiPage*   m_page = nullptr;
    QPushButton*  m_toggle = nullptr;
    QPushButton*  m_run    = nullptr;
    QTextBrowser* m_log    = nullptr;

    void init() {
        m_ai   = new MockAiClient;
        m_page = new AgentiPage(m_ai);
        m_page->show();
        QTest::qWaitForWindowExposed(m_page);

        /* Toggle Chat/Agente: checkable e contiene "Chat" all'avvio */
        const auto btns = m_page->findChildren<QPushButton*>();
        for (auto* b : btns) {
            if (b->isCheckable() && b->text().contains("Chat"))
                m_toggle = b;
            /* Run button: non checkable, testo con "CHAT" o "Avvia" */
            if (!b->isCheckable() && (b->text().contains("CHAT") || b->text().contains("Avvia")))
                m_run = b;
        }
        m_log = m_page->findChild<QTextBrowser*>("chatLog");
        QVERIFY2(m_toggle, "pulsante toggle Chat/Agente non trovato");
        QVERIFY2(m_run,    "pulsante Run (CHAT/Avvia) non trovato");
    }

    void cleanup() {
        delete m_page;
        delete m_ai;
        m_toggle = nullptr;
        m_run    = nullptr;
        m_log    = nullptr;
    }

private slots:

    /* D-1: un solo click → testo diventa "Agente Autonomo" */
    void unClickAttivaAgente() {
        init();
        QVERIFY2(!m_toggle->isChecked(), "toggle deve essere OFF all'avvio");
        m_toggle->click();
        QApplication::processEvents();
        QVERIFY2(m_toggle->isChecked(), "toggle non checked dopo click");
        QVERIFY2(m_toggle->text().contains("Agente") || m_toggle->text().contains("Auto"),
                 "testo toggle non aggiornato: " + m_toggle->text().toUtf8());
        cleanup();
    }

    /* D-2: secondo click → torna a "Chat" */
    void doppioClickTornaChat() {
        init();
        m_toggle->click();
        QApplication::processEvents();
        m_toggle->click();
        QApplication::processEvents();
        QVERIFY2(!m_toggle->isChecked(), "toggle ancora checked dopo doppio click");
        QVERIFY2(m_toggle->text().contains("Chat"),
                 "testo toggle non torna a Chat: " + m_toggle->text().toUtf8());
        cleanup();
    }

    /* D-3: testo pulsante Run cambia in modalità Agente */
    void runBtnTestoAgenteMode() {
        init();
        const QString testoBefore = m_run->text();
        m_toggle->click();
        QApplication::processEvents();
        const QString testoAfter = m_run->text();
        QVERIFY2(testoAfter != testoBefore || testoAfter.contains("Agente"),
                 "testo Run non cambia in modalita Agente: " + testoAfter.toUtf8());
        cleanup();
    }

    /* D-4: feedback nel log quando si attiva */
    void logMostraFeedbackAttivazione() {
        init();
        if (!m_log) { cleanup(); QSKIP("chatLog non trovato"); }
        m_toggle->click();
        QApplication::processEvents();
        const QString logText = m_log->toPlainText();
        QVERIFY2(logText.contains("Agente") || logText.contains("autonomo") ||
                 logText.contains("attivat"),
                 "log non mostra feedback attivazione agente");
        cleanup();
    }

    /* D-5: feedback nel log quando si disattiva */
    void logMostraFeedbackDisattivazione() {
        init();
        if (!m_log) { cleanup(); QSKIP("chatLog non trovato"); }
        m_toggle->click();
        QApplication::processEvents();
        const QString logBefore = m_log->toPlainText();
        m_toggle->click();
        QApplication::processEvents();
        const QString logAfter = m_log->toPlainText();
        QVERIFY2(logAfter.length() > logBefore.length() ||
                 logAfter.contains("Chat"),
                 "log non mostra feedback disattivazione");
        cleanup();
    }
};

/* ════════════════════════════════════════════════════════════════
   CAT-E — Parsing risposta ReAct tramite AgentiPage
   Usa un'unica istanza per categoria (init/cleanup Qt standard).
   ════════════════════════════════════════════════════════════════ */
class TestReActParsing : public QObject {
    Q_OBJECT
private:
    MockAiClient* m_ai     = nullptr;
    AgentiPage*   m_page   = nullptr;
    QPushButton*  m_toggle = nullptr;
    QTextEdit*    m_input  = nullptr;
    QPushButton*  m_run    = nullptr;
    QTextBrowser* m_log    = nullptr;

private slots:

    void init() {
        m_ai   = new MockAiClient;
        m_page = new AgentiPage(m_ai);
        m_page->show();
        QTest::qWaitForWindowExposed(m_page);

        for (auto* b : m_page->findChildren<QPushButton*>()) {
            if (b->isCheckable() && b->text().contains("Chat"))
                m_toggle = b;
            if (!b->isCheckable() && b->text().contains("CHAT"))
                m_run = b;
        }
        m_log = m_page->findChild<QTextBrowser*>("chatLog");
        const auto edits = m_page->findChildren<QTextEdit*>();
        for (auto* e : edits) {
            if (!qobject_cast<QTextBrowser*>(e)) { m_input = e; break; }
        }
        if (!m_toggle || !m_run) return;

        /* Attiva Agente Autonomo — m_run ora dice "Avvia Agente" */
        m_toggle->click();
        QApplication::processEvents();
        for (auto* b : m_page->findChildren<QPushButton*>()) {
            if (!b->isCheckable() && b->text().contains("Avvia"))
                m_run = b;
        }
    }

    void cleanup() {
        QTest::qWait(50);   /* consente drain eventi prima della distruzione */
        delete m_page; m_page = nullptr;
        delete m_ai;   m_ai   = nullptr;
        m_toggle = nullptr; m_run = nullptr; m_input = nullptr; m_log = nullptr;
    }

    /* E-1: FINAL_ANSWER → bolla risposta nel log */
    void finalAnswerGeneraBolla() {
        if (!m_toggle || !m_run || !m_log) QSKIP("widget non trovato");
        if (!m_input) QSKIP("input non trovato");

        m_input->setPlainText("Qual e' la capitale d'Italia?");
        m_run->click();
        QApplication::processEvents();

        m_ai->emitFinished(
            "THOUGHT: conosco la risposta\n"
            "FINAL_ANSWER: Roma e' la capitale d'Italia.");
        QApplication::processEvents();

        QVERIFY2(m_log->toPlainText().contains("Roma"),
                 "FINAL_ANSWER non appare nel log");
    }

    /* E-2: risposta senza tag → trattata come risposta finale */
    void rispostaSenzaTagMostrata() {
        if (!m_toggle || !m_run || !m_log) QSKIP("widget non trovato");
        if (!m_input) QSKIP("input non trovato");

        m_input->setPlainText("Salutami");
        m_run->click();
        QApplication::processEvents();

        m_ai->emitFinished("Ciao! Come posso aiutarti?");
        QApplication::processEvents();

        QVERIFY2(m_log->toPlainText().contains("Ciao"),
                 "risposta senza tag non mostrata nel log");
    }

    /* E-3: dopo FINAL_ANSWER il pulsante Run torna disponibile */
    void dopoFinalAnswerRunDisponibile() {
        if (!m_toggle || !m_run) QSKIP("widget non trovato");
        if (!m_input) QSKIP("input non trovato");

        m_input->setPlainText("Test stop");
        m_run->click();
        QApplication::processEvents();

        m_ai->emitFinished("FINAL_ANSWER: completato.");
        QApplication::processEvents();

        QVERIFY2(m_run->isEnabled(), "run non abilitato dopo FINAL_ANSWER");
        QVERIFY2(!m_run->text().contains("Stop"),
                 "run ancora in stato Stop dopo FINAL_ANSWER");
    }
};

/* ════════════════════════════════════════════════════════════════
   CAT-F — Limit step e JSON malformato
   ════════════════════════════════════════════════════════════════ */
class TestEdgeCases : public QObject {
    Q_OBJECT
private slots:

    /* F-1: buildAutoStepHtml step=0 non crashsa */
    void stepZeroNonCrascia() {
        /* step 0 è valido (0-indexed prima di ++), non deve lanciare */
        const QString html = AgentiPage::buildAutoStepHtml(0, "t", "a", "o");
        QVERIFY2(!html.isEmpty(), "step 0 produce HTML vuoto");
    }

    /* F-2: detectFirstToolCall con stringa vuota → oggetto vuoto */
    void stringaVuotaRitornaVuoto() {
        const QJsonObject tc = AgentiPage::detectFirstToolCall(QString());
        QVERIFY2(tc.isEmpty(), "stringa vuota non deve produrre tool call");
    }

    /* F-3: detectFirstToolCall con JSON incompleto → oggetto vuoto */
    void jsonIncompletoRitornaVuoto() {
        const QJsonObject tc = AgentiPage::detectFirstToolCall(
            "ACTION: {\"tool\": \"calc\"");   /* manca } e input */
        QVERIFY2(tc.isEmpty(), "JSON incompleto non deve produrre tool call");
    }

    /* F-4: buildAutoStepHtml step molto grande non crashsa */
    void stepGrandeNonCrascia() {
        const QString html = AgentiPage::buildAutoStepHtml(9999, "t", "a", "o");
        QVERIFY2(html.contains("9999"), "step grande non nel HTML");
    }

    /* F-5: system prompt non vuoto */
    void systemPromptNonVuoto() {
        QVERIFY2(!AgentiPage::_autoSystemPrompt().isEmpty(),
                 "system prompt vuoto");
    }

    /* F-6: detectFirstToolCall con tool non noto → lo restituisce comunque */
    void toolSconosciutoRestituito() {
        const QString testo =
            "TOOL_CALL: {\"tool\": \"strumento_ignoto\", \"input\": \"valore\"}";
        const QJsonObject tc = AgentiPage::detectFirstToolCall(testo);
        /* il parser non filtra per nome tool: lo restituisce anche se sconosciuto */
        QVERIFY2(!tc.isEmpty(), "tool sconosciuto non restituito");
        QCOMPARE(tc["tool"].toString(), QStringLiteral("strumento_ignoto"));
    }
};

/* ════════════════════════════════════════════════════════════════
   Entry point
   ════════════════════════════════════════════════════════════════ */
int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setApplicationName("test_agente_autonomo");

    int status = 0;
    {   TestBuildAutoStepHtml t; status |= QTest::qExec(&t, argc, argv); }
    {   TestAutoSystemPrompt  t; status |= QTest::qExec(&t, argc, argv); }
    {   TestDetectFirstToolCall t; status |= QTest::qExec(&t, argc, argv); }
    {   TestToggleUiAgente    t; status |= QTest::qExec(&t, argc, argv); }
    {   TestReActParsing      t; status |= QTest::qExec(&t, argc, argv); }
    {   TestEdgeCases         t; status |= QTest::qExec(&t, argc, argv); }
    return status;
}

#include "test_agente_autonomo.moc"
