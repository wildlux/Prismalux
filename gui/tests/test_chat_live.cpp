/* ══════════════════════════════════════════════════════════════
   test_chat_live.cpp — E2E Chat singola (💬, la sezione più usata)
   con Ollama REALE, pilotando AgentiPage come farebbe l'utente:
   testo nell'input → click Invia → bolle nel log.

   Categorie:
     CAT-A  Guardie locali via UI (zero LLM): matematica e ora di
            sistema rispondono in bolla locale SENZA toccare Ollama
     CAT-B  Chat reale: risposta streamed del modello nel log
     CAT-C  Memoria conversazione (m_chatPairs): un fatto detto al
            turno 1 viene ricordato al turno 2 (history compressa)

   Requisiti (altrimenti QSKIP): Ollama su 127.0.0.1:11434.
   Modello: PRISMALUX_TEST_MODEL oppure il primo tra i preferiti
   (gemma3-evo del profilo utente, llama3.2, qwen3).

   ⚠ Suite LENTA (minuti): esclusa dal giro ctest standard come le
   altre *Live — eseguire a mano quando serve la verifica reale.
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QApplication>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPushButton>
#include <QRandomGenerator>
#include <QTextBrowser>
#include <QTextEdit>

#include "../ai_client.h"
#include "../pages/main_ai.h"
#include "../prismalux_paths.h"
namespace P = PrismaluxPaths;

/* ── Probe Ollama: lista modelli, vuota se irraggiungibile ── */
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

static QString pickModel(const QStringList& installed)
{
    const QString env = qEnvironmentVariable("PRISMALUX_TEST_MODEL");
    if (!env.isEmpty()) return env;
    static const QStringList kPref = {
        "antconsales/antonio-gemma3-evo-q4:latest",   /* preferito utente, piccolo=veloce */
        "llama3.2:3b", "llama3.2:latest", "qwen3:4b",
    };
    for (const QString& p : kPref)
        if (installed.contains(p)) return p;
    return {};
}

class TestChatLive : public QObject {
    Q_OBJECT

    QString       m_model;
    AiClient*     m_ai   = nullptr;
    AgentiPage*   m_page = nullptr;
    QTextEdit*    m_input = nullptr;
    QPushButton*  m_run   = nullptr;
    QTextBrowser* m_log   = nullptr;

    void invia(const QString& testo) {
        m_input->setPlainText(testo);
        m_run->click();
    }

private slots:

    void initTestCase() {
        const QStringList models = ollamaModels();
        if (models.isEmpty())
            QSKIP("Ollama non raggiungibile su 127.0.0.1:11434");
        m_model = pickModel(models);
        if (m_model.isEmpty())
            QSKIP("nessun modello di test installato");
        qInfo() << "Modello live:" << m_model;
    }

    void init() {
        m_ai = new AiClient;
        m_ai->setBackend(AiClient::Ollama, "127.0.0.1",
                         P::kOllamaPort, m_model);
        m_page = new AgentiPage(m_ai);
        m_page->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_page));

        m_log = m_page->findChild<QTextBrowser*>("chatLog");
        for (auto* e : m_page->findChildren<QTextEdit*>())
            if (!qobject_cast<QTextBrowser*>(e)) { m_input = e; break; }
        for (auto* b : m_page->findChildren<QPushButton*>())
            if (!b->isCheckable() && b->text().contains("Invia"))
                m_run = b;
        QVERIFY2(m_log,   "chatLog non trovato");
        QVERIFY2(m_input, "input non trovato");
        QVERIFY2(m_run,   "pulsante Invia non trovato");
    }

    void cleanup() {
        if (m_ai) m_ai->abort();
        QTest::qWait(100);
        delete m_page; m_page = nullptr;
        delete m_ai;   m_ai   = nullptr;
        m_input = nullptr; m_run = nullptr; m_log = nullptr;
    }

    /* A-1: guardia matematica via UI — risposta locale immediata,
       Ollama non deve nemmeno essere interpellato (m_ai mai busy) */
    void guardiaMathZeroLlm() {
        invia("quanto fa 25*18?");
        QTRY_VERIFY_WITH_TIMEOUT(m_log->toPlainText().contains("450"), 5000);
        QVERIFY2(!m_ai->busy(), "la guardia math ha interpellato l'LLM");
        QVERIFY2(m_log->toPlainText().contains("0 token"),
                 "manca l'etichetta 'Risposta locale ·· 0 token'");
    }

    /* A-2: guardia data/ora via UI — orologio di sistema, zero LLM */
    void guardiaOraZeroLlm() {
        invia("che ore sono?");
        const QString hh = QDateTime::currentDateTime().toString("HH:");
        QTRY_VERIFY_WITH_TIMEOUT(m_log->toPlainText().contains(hh), 5000);
        QVERIFY2(!m_ai->busy(), "la guardia ora ha interpellato l'LLM");
    }

    /* B-1: chat reale — il modello risponde e la bolla arriva nel log */
    void chatRealeRisposta() {
        const QString nonce = QString::number(
            QRandomGenerator::global()->bounded(100000, 999999));
        invia("Ripeti esattamente questo codice e nient'altro: ZX" + nonce);
        QTRY_VERIFY2_WITH_TIMEOUT(
            m_log->toPlainText().contains("ZX" + nonce),
            qPrintable("risposta del modello assente. Log:\n"
                       + m_log->toPlainText().right(1200)),
            300000);
        QTRY_VERIFY_WITH_TIMEOUT(!m_ai->busy(), 60000);
    }

    /* C-1: memoria conversazione — il fatto del turno 1 è nel turno 2
       (m_chatPairs → history compressa passata a chat()) */
    void memoriaConversazione() {
        const QString code = QString::number(
            QRandomGenerator::global()->bounded(1000, 9999));
        invia("Il mio numero fortunato \xc3\xa8 " + code +
              ". Rispondi solo: memorizzato.");
        QTRY_VERIFY2_WITH_TIMEOUT(
            m_log->toPlainText().contains("memorizzato", Qt::CaseInsensitive),
            qPrintable("conferma turno 1 assente. Log:\n"
                       + m_log->toPlainText().right(1200)),
            300000);
        QTRY_VERIFY_WITH_TIMEOUT(!m_ai->busy(), 60000);

        invia("Qual \xc3\xa8 il mio numero fortunato? Rispondi solo col numero.");
        QTRY_VERIFY2_WITH_TIMEOUT(
            m_log->toPlainText().count(code) >= 2,   /* turno 1 + risposta */
            qPrintable("il modello non ricorda " + code + ". Log:\n"
                       + m_log->toPlainText().right(1200)),
            300000);
    }
};

int main(int argc, char* argv[])
{
    /* Watchdog QTest default 5 min: troppo stretto per modelli locali
       lenti — va alzato PRIMA di qExec (vedi test_agente_autonomo_live) */
    qputenv("QTEST_FUNCTION_TIMEOUT", "900000");
    QApplication app(argc, argv);
    TestChatLive t;
    return QTest::qExec(&t, argc, argv);
}

#include "test_chat_live.moc"
