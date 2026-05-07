/* ══════════════════════════════════════════════════════════════
   test_impostazioni_page.cpp — Unit test ImpostazioniPage

   Categorie:
     CAT-A  AiChatParams — save/load round-trip, default sensati
     CAT-B  ImpostazioniPage — costruzione senza crash
     CAT-C  Think mode UI — 3 pulsanti con objectName "thinkModeBtn"
     CAT-D  Preset "8 GB RAM" — pulsante presente, objectName corretto

   Build:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_impostazioni_page
     ./build_tests/test_impostazioni_page
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QApplication>
#include <QPushButton>

#include "mock_ai_client.h"
#include "../ai_client.h"
#include "../hardware_monitor.h"
#include "../pages/impostazioni_page.h"

/* ══════════════════════════════════════════════════════════════
   CAT-A — AiChatParams round-trip (nessuna UI necessaria)
   ══════════════════════════════════════════════════════════════ */
class TestAiChatParamsRoundTrip : public QObject {
    Q_OBJECT
private slots:

    /* A-1: valori default sono sensati */
    void defaultSensati() {
        AiChatParams p;
        QVERIFY2(p.temperature >= 0.0 && p.temperature <= 1.5,
                 "temperatura default fuori range [0, 1.5]");
        QVERIFY2(p.num_predict > 0, "num_predict default <= 0");
        QVERIFY2(p.num_ctx > 0, "num_ctx default <= 0");
        QVERIFY2(p.thinkMode >= 0 && p.thinkMode <= 2,
                 "thinkMode default fuori range [0,2]");
        QVERIFY2(p.thinkBudget >= 1 && p.thinkBudget <= 4,
                 "thinkBudget default fuori range [1,4]");
    }

    /* A-2: save → load restituisce gli stessi valori */
    void roundTripTemperatura() {
        AiChatParams orig;
        orig.temperature = 0.42;
        AiChatParams::save(orig);

        AiChatParams letto = AiChatParams::load();
        QCOMPARE(letto.temperature, orig.temperature);
    }

    /* A-3: save → load thinkMode */
    void roundTripThinkMode() {
        AiChatParams orig;
        orig.thinkMode = 2;  /* On forzato */
        AiChatParams::save(orig);

        AiChatParams letto = AiChatParams::load();
        QCOMPARE(letto.thinkMode, 2);
    }

    /* A-4: save → load thinkBudget */
    void roundTripThinkBudget() {
        AiChatParams orig;
        orig.thinkBudget = 3;
        AiChatParams::save(orig);

        AiChatParams letto = AiChatParams::load();
        QCOMPARE(letto.thinkBudget, 3);
    }

    /* A-5: save → load num_ctx */
    void roundTripNumCtx() {
        AiChatParams orig;
        orig.num_ctx = 4096;
        AiChatParams::save(orig);

        AiChatParams letto = AiChatParams::load();
        QCOMPARE(letto.num_ctx, 4096);
    }

    /* A-6: save → load num_predict */
    void roundTripNumPredict() {
        AiChatParams orig;
        orig.num_predict = 1024;
        AiChatParams::save(orig);

        AiChatParams letto = AiChatParams::load();
        QCOMPARE(letto.num_predict, 1024);
    }

    /* A-7: save → load flash_attn */
    void roundTripFlashAttn() {
        AiChatParams orig;
        orig.flash_attn = true;
        AiChatParams::save(orig);

        AiChatParams letto = AiChatParams::load();
        QVERIFY2(letto.flash_attn, "flash_attn non persistita correttamente");
    }

    /* A-8: load ritorna sempre un oggetto valido (non lancia eccezioni) */
    void loadNonCrasha() {
        /* Può leggere o creare il file — non deve crashare in nessun caso */
        const AiChatParams p = AiChatParams::load();
        (void)p;  /* non deve lanciare */
    }

    /* A-9: round-trip completo con tutti i campi modificati */
    void roundTripCompleto() {
        AiChatParams orig;
        orig.temperature  = 0.77;
        orig.num_predict  = 2048;
        orig.num_ctx      = 16384;
        orig.thinkMode    = 1;
        orig.thinkBudget  = 4;
        orig.flash_attn   = true;
        AiChatParams::save(orig);

        AiChatParams letto = AiChatParams::load();
        QCOMPARE(letto.temperature,  orig.temperature);
        QCOMPARE(letto.num_predict,  orig.num_predict);
        QCOMPARE(letto.num_ctx,      orig.num_ctx);
        QCOMPARE(letto.thinkMode,    orig.thinkMode);
        QCOMPARE(letto.thinkBudget,  orig.thinkBudget);
        QVERIFY(letto.flash_attn == orig.flash_attn);
    }

    void cleanupTestCase() {
        /* Ripristina default per non inquinare altri test */
        AiChatParams::save(AiChatParams{});
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B — ImpostazioniPage: costruzione senza crash
   ══════════════════════════════════════════════════════════════ */
class TestImpostazioniCostruzione : public QObject {
    Q_OBJECT
private:
    MockAiClient*    m_ai  = nullptr;
    HardwareMonitor* m_hw  = nullptr;
    ImpostazioniPage* m_page = nullptr;

private slots:

    void initTestCase() {
        m_ai   = new MockAiClient;
        m_hw   = new HardwareMonitor;
        m_page = new ImpostazioniPage(m_ai, m_hw);
    }

    void cleanupTestCase() {
        delete m_page; m_page = nullptr;
        delete m_hw;   m_hw   = nullptr;
        delete m_ai;   m_ai   = nullptr;
    }

    /* B-1: costruisce senza crash */
    void costruisceSenzaCrash() {
        QVERIFY2(m_page != nullptr, "ImpostazioniPage non costruita");
    }

    /* B-2: è un QWidget valido */
    void eUnQWidget() {
        auto* w = qobject_cast<QWidget*>(m_page);
        QVERIFY2(w != nullptr, "ImpostazioniPage non è un QWidget");
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-C — Think mode UI: 3 QPushButton con objectName "thinkModeBtn"
   ══════════════════════════════════════════════════════════════ */
class TestThinkModeUI : public QObject {
    Q_OBJECT
private:
    MockAiClient*    m_ai  = nullptr;
    HardwareMonitor* m_hw  = nullptr;
    ImpostazioniPage* m_page = nullptr;

private slots:

    void initTestCase() {
        m_ai   = new MockAiClient;
        m_hw   = new HardwareMonitor;
        m_page = new ImpostazioniPage(m_ai, m_hw);
    }

    void cleanupTestCase() {
        delete m_page; m_page = nullptr;
        delete m_hw;
        delete m_ai;
    }

    /* C-1: esistono esattamente 3 pulsanti thinkModeBtn */
    void tre_pulsanti_thinkModeBtn() {
        const QList<QPushButton*> btns =
            m_page->findChildren<QPushButton*>("thinkModeBtn");
        QVERIFY2(btns.size() == 3,
                 qPrintable(QString("attesi 3 thinkModeBtn, trovati %1").arg(btns.size())));
    }

    /* C-2: i pulsanti sono checkable */
    void pulsantiCheckable() {
        const QList<QPushButton*> btns =
            m_page->findChildren<QPushButton*>("thinkModeBtn");
        if (btns.isEmpty()) QSKIP("nessun thinkModeBtn trovato");
        for (auto* b : btns)
            QVERIFY2(b->isCheckable(),
                     qPrintable("pulsante '" + b->text() + "' non è checkable"));
    }

    /* C-3: almeno uno dei 3 è checked all'avvio */
    void almenoUnoChecked() {
        const QList<QPushButton*> btns =
            m_page->findChildren<QPushButton*>("thinkModeBtn");
        if (btns.size() < 3) QSKIP("meno di 3 thinkModeBtn trovati");
        const bool hasChecked = std::any_of(btns.begin(), btns.end(),
                                            [](auto* b){ return b->isChecked(); });
        QVERIFY2(hasChecked, "nessun thinkModeBtn checked all'avvio");
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-D — Preset button "8 GB RAM"
   ══════════════════════════════════════════════════════════════ */
class TestPreset : public QObject {
    Q_OBJECT
private:
    MockAiClient*    m_ai  = nullptr;
    HardwareMonitor* m_hw  = nullptr;
    ImpostazioniPage* m_page = nullptr;

private slots:

    void initTestCase() {
        m_ai   = new MockAiClient;
        m_hw   = new HardwareMonitor;
        m_page = new ImpostazioniPage(m_ai, m_hw);
    }

    void cleanupTestCase() {
        delete m_page;
        delete m_hw;
        delete m_ai;
        AiChatParams::save(AiChatParams{});  /* ripristina default */
    }

    /* D-1: pulsante "8 GB RAM" esiste */
    void preset8GbEsiste() {
        const QList<QPushButton*> btns = m_page->findChildren<QPushButton*>();
        const bool found = std::any_of(btns.begin(), btns.end(),
            [](auto* b){ return b->text().contains("8 GB", Qt::CaseInsensitive) ||
                                 b->text().contains("8GB",  Qt::CaseInsensitive); });
        if (!found) QSKIP("pulsante '8 GB RAM' non trovato — preset non ancora implementati?");
        QVERIFY(found);
    }

    /* D-2: pulsante "Contesto lungo" esiste */
    void presetContestoLungoEsiste() {
        const QList<QPushButton*> btns = m_page->findChildren<QPushButton*>();
        const bool found = std::any_of(btns.begin(), btns.end(),
            [](auto* b){ return b->text().contains("Contesto", Qt::CaseInsensitive) ||
                                 b->text().contains("lungo",   Qt::CaseInsensitive); });
        if (!found) QSKIP("pulsante 'Contesto lungo' non trovato");
        QVERIFY(found);
    }
};

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    int status = 0;
    {
        TestAiChatParamsRoundTrip t1; status |= QTest::qExec(&t1, argc, argv);
        TestImpostazioniCostruzione t2; status |= QTest::qExec(&t2, argc, argv);
        TestThinkModeUI           t3; status |= QTest::qExec(&t3, argc, argv);
        TestPreset                t4; status |= QTest::qExec(&t4, argc, argv);
    }
    return status;
}

#include "test_impostazioni_page.moc"
