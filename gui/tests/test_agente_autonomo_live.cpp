/* ══════════════════════════════════════════════════════════════
   test_agente_autonomo_live.cpp — E2E Agente Autonomo (Agentico 👔)
   con Ollama REALE: verifica che la modalità agentica funzioni da
   LLM agentico vero — il modello pianifica (THOUGHT), chiama un
   tool (TOOL_CALL calcolatrice), riceve il risultato e chiude con
   FINAL_ANSWER contenente il valore calcolato.

   Categorie:
     CAT-A  ReAct end-to-end: task → TOOL_CALL calc → 37*43=1591 nel log
     CAT-B  Domanda diretta senza tool → FINAL_ANSWER nel log

   Requisiti (altrimenti QSKIP):
     - Ollama su 127.0.0.1:11434
     - un modello tool-capable installato (qwen3:4b, qwen3.5:4b,
       llama3.2, mistral:7b-instruct — primo trovato)
   Override modello:
     PRISMALUX_TEST_MODEL=qwen3:4b ./build_tests/test_agente_autonomo_live

   ⚠ Suite LENTA (minuti): esclusa dal giro ctest standard come le
   altre *Live — eseguire a mano quando serve la verifica reale.
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QApplication>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPushButton>
#include <QTextBrowser>
#include <QTextEdit>

#include "../ai_client.h"
#include "../pages/main_ai.h"
#include "../widgets/tri_mode_button.h"
#include "../prismalux_paths.h"
namespace P = PrismaluxPaths;

/* ── Probe Ollama: ritorna la lista modelli, vuota se irraggiungibile ── */
static QStringList ollamaModels()
{
    QNetworkAccessManager nam;
    QNetworkRequest req(QUrl(QString("http://127.0.0.1:%1/api/tags")
                                 .arg(P::kOllamaPort)));
    req.setTransferTimeout(2000);
    auto* r = nam.get(req);
    QEventLoop loop;
    QObject::connect(r, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    QStringList out;
    if (r->error() == QNetworkReply::NoError) {
        const auto arr = QJsonDocument::fromJson(r->readAll())
                             .object()["models"].toArray();
        for (const auto& v : arr)
            out << v.toObject()["name"].toString();
    }
    r->deleteLater();
    return out;
}

/* ── Modello per il test: env override oppure primo tool-capable ── */
static QString pickModel(const QStringList& installed)
{
    const QString env = qEnvironmentVariable("PRISMALUX_TEST_MODEL");
    if (!env.isEmpty()) return env;
    static const QStringList kPref = {
        "qwen3:4b", "qwen3.5:4b", "llama3.2:3b", "llama3.2:latest",
        "mistral:7b-instruct",
    };
    for (const QString& p : kPref)
        if (installed.contains(p)) return p;
    return {};
}

class TestAgenteAutonomoLive : public QObject {
    Q_OBJECT

    QString       m_model;
    AiClient*     m_ai   = nullptr;
    AgentiPage*   m_page = nullptr;
    QTextEdit*    m_input = nullptr;
    QPushButton*  m_run   = nullptr;
    QTextBrowser* m_log   = nullptr;

private slots:

    void initTestCase() {
        const QStringList models = ollamaModels();
        if (models.isEmpty())
            QSKIP("Ollama non raggiungibile su 127.0.0.1:11434");
        m_model = pickModel(models);
        if (m_model.isEmpty())
            QSKIP("nessun modello tool-capable installato "
                  "(qwen3:4b / llama3.2 / mistral:7b-instruct)");
        qInfo() << "Modello live:" << m_model;
    }

    void init() {
        m_ai = new AiClient;
        m_ai->setBackend(AiClient::Ollama, "127.0.0.1",
                         P::kOllamaPort, m_model);
        m_page = new AgentiPage(m_ai);
        m_page->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_page));

        /* Attiva la modalità Agentico dal TriModeButton (👔) */
        auto* tri = m_page->findChild<TriModeButton*>();
        QVERIFY2(tri, "TriModeButton non trovato");
        tri->setMode(TriModeButton::Agentico, /*emitSignal=*/true);
        QApplication::processEvents();

        m_log = m_page->findChild<QTextBrowser*>("chatLog");
        for (auto* e : m_page->findChildren<QTextEdit*>())
            if (!qobject_cast<QTextBrowser*>(e)) { m_input = e; break; }
        for (auto* b : m_page->findChildren<QPushButton*>())
            if (!b->isCheckable() && b->text().contains("Avvia Agente"))
                m_run = b;
        QVERIFY2(m_log,   "chatLog non trovato");
        QVERIFY2(m_input, "input non trovato");
        QVERIFY2(m_run,   "pulsante 'Avvia Agente' non trovato — "
                          "la modalità Agentico non si è attivata");
    }

    void cleanup() {
        if (m_ai) m_ai->abort();
        QTest::qWait(100);
        delete m_page; m_page = nullptr;
        delete m_ai;   m_ai   = nullptr;
        m_input = nullptr; m_run = nullptr; m_log = nullptr;
    }

    /* A-1: il ciclo ReAct usa davvero un tool — il risultato 1591
       può arrivare SOLO dal tool calcolatrice eseguito in locale */
    void reactConToolCalcolatrice() {
        m_input->setPlainText(
            "Usa il tool calcolatrice per calcolare 37*43 e poi "
            "rispondi con FINAL_ANSWER contenente solo il risultato.");
        m_run->click();

        /* qwen3:4b (think) su GTX 1050: ~76s a step misurati, ReAct = 2+
           step + caricamento modello → servono timeout generosi */
        QTRY_VERIFY2_WITH_TIMEOUT(
            m_log->toPlainText().contains("1591"),
            qPrintable("il log non contiene 1591 — ReAct non ha "
                       "eseguito il tool. Log:\n"
                       + m_log->toPlainText().right(1500)),
            600000);
    }

    /* B-1: domanda diretta → l'agente chiude con una risposta finale */
    void domandaDirettaFinalAnswer() {
        m_input->setPlainText(
            "Qual e' la capitale della Francia? Rispondi brevissimo.");
        m_run->click();

        QTRY_VERIFY2_WITH_TIMEOUT(
            m_log->toPlainText().contains("Parigi", Qt::CaseInsensitive),
            qPrintable("risposta finale assente. Log:\n"
                       + m_log->toPlainText().right(1500)),
            600000);
    }
};

int main(int argc, char* argv[])
{
    /* Watchdog QTest di default = 5 min per funzione: i modelli think su
       GPU piccola sforano (misurato ~76s/step). Va impostato PRIMA di
       qExec, altrimenti il test muore con "Received a fatal error". */
    qputenv("QTEST_FUNCTION_TIMEOUT", "900000");
    QApplication app(argc, argv);
    TestAgenteAutonomoLive t;
    return QTest::qExec(&t, argc, argv);
}

#include "test_agente_autonomo_live.moc"
