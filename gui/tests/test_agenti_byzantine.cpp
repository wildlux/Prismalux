/* ══════════════════════════════════════════════════════════════
   test_agenti_byzantine.cpp — Unit test modalità Byzantine AgentiPage

   Categorie:
     CAT-A  AgentsConfigDialog — modalità Byzantine presente in modeCombo
     CAT-B  Numero agenti Byzantine — numAgents() >= 3 se impostato dispari
     CAT-C  MockAiClient — emitFullResponse usato come stub per pipeline
     CAT-D  AgentsConfigDialog — configurazione agenti per Byzantine

   Note: i test non usano Ollama reale — solo MockAiClient.
         Il consenso Byzantine reale richiede integration test separato.

   Build:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_agenti_byzantine
     ./build_tests/test_agenti_byzantine
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QApplication>
#include <QComboBox>
#include <QSpinBox>

#include "mock_ai_client.h"
#include "../pages/agents_config_dialog.h"

static AgentsConfigDialog* s_dlg = nullptr;

static void ensureDialog() {
    if (!s_dlg) {
        s_dlg = new AgentsConfigDialog(nullptr);
    }
}

/* ══════════════════════════════════════════════════════════════
   CAT-A — modeCombo ha voce Byzantine
   ══════════════════════════════════════════════════════════════ */
class TestByantineVoceCombo : public QObject {
    Q_OBJECT
private slots:

    void initTestCase() { ensureDialog(); }

    /* A-1: la voce "Byzantino" / "Byzantine" è presente */
    void voceByantinePresente() {
        QComboBox* mc = s_dlg->modeCombo();
        QVERIFY2(mc != nullptr, "modeCombo() è null");
        bool found = false;
        for (int i = 0; i < mc->count(); ++i) {
            if (mc->itemText(i).contains("Byzant", Qt::CaseInsensitive)) {
                found = true;
                break;
            }
        }
        QVERIFY2(found, "voce 'Byzantine/Byzantino' assente in modeCombo");
    }

    /* A-2: la voce Pipeline è prima di Byzantine */
    void pipelinePrimaDiByzantine() {
        QComboBox* mc = s_dlg->modeCombo();
        if (!mc || mc->count() < 2) QSKIP("modeCombo ha meno di 2 voci");
        const QString v0 = mc->itemText(0);
        QVERIFY2(v0.contains("Pipeline", Qt::CaseInsensitive),
                 qPrintable("voce 0 non è Pipeline: " + v0));
    }

    /* A-3: possiamo selezionare la modalità Byzantine senza crash */
    void selezioneByzantineNonCrasha() {
        QComboBox* mc = s_dlg->modeCombo();
        if (!mc) QSKIP("modeCombo è null");
        for (int i = 0; i < mc->count(); ++i) {
            if (mc->itemText(i).contains("Byzant", Qt::CaseInsensitive)) {
                mc->setCurrentIndex(i);
                QCOMPARE(mc->currentIndex(), i);
                mc->setCurrentIndex(0);  /* ripristina */
                return;
            }
        }
        QSKIP("voce Byzantine non trovata per selezione");
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B — numAgents per Byzantine (dispari >= 3)
   ══════════════════════════════════════════════════════════════ */
class TestNumAgentiByzantine : public QObject {
    Q_OBJECT
private slots:

    void initTestCase() { ensureDialog(); }

    /* B-1: lo SpinBox numAgents supporta valori >= 3 */
    void spinBoxSupport3() {
        QSpinBox* sb = s_dlg->numAgentsSpinBox();
        QVERIFY2(sb != nullptr, "numAgentsSpinBox() è null");
        QVERIFY2(sb->maximum() >= 3, "SpinBox max < 3 — Byzantine richiede almeno 3");
    }

    /* B-2: impostare 3 agenti → numAgents() == 3 */
    void setTreAgenti() {
        QSpinBox* sb = s_dlg->numAgentsSpinBox();
        if (!sb || sb->maximum() < 3) QSKIP("SpinBox non supporta 3");
        sb->setValue(3);
        QApplication::processEvents();
        QVERIFY2(s_dlg->numAgents() >= 3,
                 "numAgents() < 3 anche dopo setValue(3)");
        sb->setValue(AgentsConfigDialog::MAX_AGENTS);  /* ripristina */
    }

    /* B-3: impostare 5 agenti (dispari) → numAgents() == 5 */
    void setCinqueAgenti() {
        QSpinBox* sb = s_dlg->numAgentsSpinBox();
        if (!sb || sb->maximum() < 5) QSKIP("SpinBox non supporta 5");
        sb->setValue(5);
        QApplication::processEvents();
        QVERIFY2(s_dlg->numAgents() >= 5,
                 "numAgents() < 5 dopo setValue(5)");
        sb->setValue(AgentsConfigDialog::MAX_AGENTS);
    }

    /* B-4: Byzantine con MAX_AGENTS (6) è ammesso — non deve forzare dispari */
    void maxAgentsByzantineNonCrasha() {
        QComboBox* mc = s_dlg->modeCombo();
        if (!mc) QSKIP("modeCombo null");
        /* Seleziona Byzantine */
        for (int i = 0; i < mc->count(); ++i) {
            if (mc->itemText(i).contains("Byzant", Qt::CaseInsensitive)) {
                mc->setCurrentIndex(i);
                QApplication::processEvents();
                /* deve funzionare senza crash anche con numero agenti pari */
                mc->setCurrentIndex(0);  /* ripristina */
                return;
            }
        }
        QSKIP("voce Byzantine non trovata");
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-C — MockAiClient compatibilità (usato come stub Byzantine)
   ══════════════════════════════════════════════════════════════ */
class TestMockAiClientStub : public QObject {
    Q_OBJECT
private slots:

    /* C-1: emitFullResponse emette token + finished */
    void emitFullResponseEmetteFinished() {
        MockAiClient ai;
        QSignalSpy spyFin(&ai, &AiClient::finished);
        const QString full = ai.emitFullResponse({"risposta", " di", " test"});
        QVERIFY2(!spyFin.isEmpty(), "finished non emesso da emitFullResponse");
        QVERIFY2(full.contains("risposta"), "risposta non presente nell'output");
    }

    /* C-2: emitError emette error signal */
    void emitErrorEmetteError() {
        MockAiClient ai;
        QSignalSpy spyErr(&ai, &AiClient::error);
        ai.emitError("errore test");
        QVERIFY2(!spyErr.isEmpty(), "error signal non emesso");
        QCOMPARE(spyErr.first().first().toString(), QString("errore test"));
    }

    /* C-3: fetchModels override — restituisce lista test predefinita */
    void fetchModelsPredefinita() {
        MockAiClient ai;
        QSignalSpy spy(&ai, &AiClient::modelsReady);
        ai.fetchModels();
        QApplication::processEvents();
        QVERIFY2(!spy.isEmpty(), "modelsReady non emesso da MockAiClient");
        const QStringList models = spy.first().first().toStringList();
        QVERIFY2(!models.isEmpty(), "lista modelli mock è vuota");
    }

    /* C-4: abort() non crasha senza richiesta attiva */
    void abortIdleNonCrasha() {
        MockAiClient ai;
        ai.abort();  /* senza chat attiva: nessun crash, nessun segnale richiesto */
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-D — Configurazione agenti per Byzantine
   ══════════════════════════════════════════════════════════════ */
class TestConfigAgentiByzantine : public QObject {
    Q_OBJECT
private slots:

    void initTestCase() { ensureDialog(); }

    /* D-1: roleCombo disponibili per tutti i 6 slot */
    void roleComboProntoPerByzantine() {
        for (int i = 0; i < AgentsConfigDialog::MAX_AGENTS; ++i) {
            QVERIFY2(s_dlg->roleCombo(i) != nullptr,
                     qPrintable(QString("roleCombo(%1) null").arg(i)));
        }
    }

    /* D-2: modelCombo disponibili per tutti i 6 slot */
    void modelComboProntoPerByzantine() {
        for (int i = 0; i < AgentsConfigDialog::MAX_AGENTS; ++i) {
            QVERIFY2(s_dlg->modelCombo(i) != nullptr,
                     qPrintable(QString("modelCombo(%1) null").arg(i)));
        }
    }

    /* D-3: enabledChk disponibili per tutti i 6 slot */
    void enabledChkProntoPerByzantine() {
        for (int i = 0; i < AgentsConfigDialog::MAX_AGENTS; ++i) {
            QVERIFY2(s_dlg->enabledChk(i) != nullptr,
                     qPrintable(QString("enabledChk(%1) null").arg(i)));
        }
    }

    /* D-4: RAG condiviso non interferisce con Byzantine */
    void sharedRagNonNullPerByzantine() {
        QVERIFY2(s_dlg->sharedRagWidget() != nullptr,
                 "sharedRagWidget() null — il RAG condiviso è richiesto anche in Byzantine");
    }
};

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    int status = 0;
    {
        TestByantineVoceCombo      t1; status |= QTest::qExec(&t1, argc, argv);
        TestNumAgentiByzantine     t2; status |= QTest::qExec(&t2, argc, argv);
        TestMockAiClientStub       t3; status |= QTest::qExec(&t3, argc, argv);
        TestConfigAgentiByzantine  t4; status |= QTest::qExec(&t4, argc, argv);
    }

    delete s_dlg;
    return status;
}

#include "test_agenti_byzantine.moc"
