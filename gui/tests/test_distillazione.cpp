#include <QtTest/QtTest>
#include <QApplication>
#include <QListWidget>
#include <QComboBox>
#include <QPushButton>
#include <QLineEdit>
#include <QSpinBox>
#include "../pages/main_distillazione.h"
#include "../ai_client.h"

/* ══════════════════════════════════════════════════════════════
   TestDistillazione — CAT-A: costruzione e stato iniziale
   Nessuna dipendenza da Ollama.
   ══════════════════════════════════════════════════════════════ */
class TestDistillazione : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void testCostruzione();
    void testStatoInizialeWidgets();
    void testPulsantiDisabilitatiAllAvvio();
    void testDistEntryDefault();
    void cleanupTestCase();

private:
    QApplication*    m_app  = nullptr;
    AiClient*        m_ai   = nullptr;
    DistillazionePage* m_page = nullptr;
};

void TestDistillazione::initTestCase()
{
    int argc = 0;
    m_app = new QApplication(argc, nullptr);
    m_ai  = new AiClient();
    m_page = new DistillazionePage(m_ai, nullptr);
    QVERIFY(m_page != nullptr);
}

void TestDistillazione::testCostruzione()
{
    QVERIFY(m_page != nullptr);
    /* La pagina deve essere istanziabile senza crash */
}

void TestDistillazione::testStatoInizialeWidgets()
{
    /* Cerca i widget principali */
    auto* dominio = m_page->findChild<QLineEdit*>();
    QVERIFY(dominio != nullptr);

    auto* spn = m_page->findChild<QSpinBox*>();
    QVERIFY(spn != nullptr);
    QVERIFY(spn->value() >= 3);
    QVERIFY(spn->value() <= 200);

    auto* list = m_page->findChild<QListWidget*>();
    QVERIFY(list != nullptr);

    auto* combo = m_page->findChild<QComboBox*>();
    QVERIFY(combo != nullptr);
}

void TestDistillazione::testPulsantiDisabilitatiAllAvvio()
{
    /* "Interroga Modelli" deve essere disabilitato finché non ci sono domande */
    auto btns = m_page->findChildren<QPushButton*>();
    QPushButton* btnInterroga = nullptr;
    for (auto* b : btns) {
        if (b->text().contains("Interroga") || b->text().contains("2."))
            btnInterroga = b;
    }
    QVERIFY(btnInterroga != nullptr);
    QVERIFY(!btnInterroga->isEnabled());
}

void TestDistillazione::testDistEntryDefault()
{
    DistEntry e;
    QVERIFY(e.domanda.isEmpty());
    QVERIFY(e.risposte.isEmpty());
    QVERIFY(e.migliorRisposta.isEmpty());
    QVERIFY(e.modelloMiglior.isEmpty());
}

void TestDistillazione::cleanupTestCase()
{
    delete m_page;  m_page = nullptr;
    delete m_ai;    m_ai   = nullptr;
    delete m_app;   m_app  = nullptr;
}

QTEST_APPLESS_MAIN(TestDistillazione)
#include "test_distillazione.moc"
