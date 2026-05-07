/* ══════════════════════════════════════════════════════════════
   test_agents_config_dialog.cpp — Unit test AgentsConfigDialog

   Categorie:
     CAT-A  Struttura widget — 6 roleCombo, modelCombo, enabledChk, ragWidget
     CAT-B  numAgents() — range SpinBox, valore default
     CAT-C  modeCombo() — non null, almeno 2 voci (Pipeline + Byzantino)
     CAT-D  RagDropWidget per-agente — hasContext() false all'avvio
     CAT-E  sharedRagWidget() — istanza unica, hasContext() false all'avvio

   Build:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_agents_config_dialog
     ./build_tests/test_agents_config_dialog
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QApplication>
#include <QComboBox>
#include <QCheckBox>
#include "../pages/agents_config_dialog.h"

static QApplication*       s_app = nullptr;
static AgentsConfigDialog* s_dlg = nullptr;

static void ensureDialog() {
    if (!s_dlg) {
        s_dlg = new AgentsConfigDialog(nullptr);
        /* Non chiamiamo show() — basta costruire il widget per testare la struttura */
    }
}

/* ══════════════════════════════════════════════════════════════
   CAT-A — Struttura widget: 6 combo/check/rag per ogni slot
   ══════════════════════════════════════════════════════════════ */
class TestStruttura : public QObject {
    Q_OBJECT
private slots:

    void initTestCase() { ensureDialog(); }

    /* A-1: roleCombo non null per tutti i 6 slot */
    void roleComboPresentePerTuttiGliSlot() {
        for (int i = 0; i < AgentsConfigDialog::MAX_AGENTS; ++i) {
            QVERIFY2(s_dlg->roleCombo(i) != nullptr,
                     qPrintable(QString("roleCombo(%1) è null").arg(i)));
        }
    }

    /* A-2: modelCombo non null per tutti i 6 slot */
    void modelComboPresentePerTuttiGliSlot() {
        for (int i = 0; i < AgentsConfigDialog::MAX_AGENTS; ++i) {
            QVERIFY2(s_dlg->modelCombo(i) != nullptr,
                     qPrintable(QString("modelCombo(%1) è null").arg(i)));
        }
    }

    /* A-3: enabledChk non null per tutti i 6 slot */
    void enabledChkPresentePerTuttiGliSlot() {
        for (int i = 0; i < AgentsConfigDialog::MAX_AGENTS; ++i) {
            QVERIFY2(s_dlg->enabledChk(i) != nullptr,
                     qPrintable(QString("enabledChk(%1) è null").arg(i)));
        }
    }

    /* A-4: ragWidget non null per tutti i 6 slot */
    void ragWidgetPresentePerTuttiGliSlot() {
        for (int i = 0; i < AgentsConfigDialog::MAX_AGENTS; ++i) {
            QVERIFY2(s_dlg->ragWidget(i) != nullptr,
                     qPrintable(QString("ragWidget(%1) è null").arg(i)));
        }
    }

    /* A-5: MAX_AGENTS è 6 */
    void maxAgentsE6() {
        QCOMPARE(AgentsConfigDialog::MAX_AGENTS, 6);
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B — numAgents() e SpinBox
   ══════════════════════════════════════════════════════════════ */
class TestNumAgents : public QObject {
    Q_OBJECT
private slots:

    void initTestCase() { ensureDialog(); }

    /* B-1: numAgents() di default == MAX_AGENTS (6) */
    void defaultEMaxAgents() {
        QCOMPARE(s_dlg->numAgents(), AgentsConfigDialog::MAX_AGENTS);
    }

    /* B-2: SpinBox non null */
    void spinBoxNonNull() {
        QVERIFY2(s_dlg->numAgentsSpinBox() != nullptr, "numAgentsSpinBox() è null");
    }

    /* B-3: valore SpinBox == numAgents() */
    void spinBoxCoerente() {
        QCOMPARE(s_dlg->numAgentsSpinBox()->value(), s_dlg->numAgents());
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-C — modeCombo
   ══════════════════════════════════════════════════════════════ */
class TestModeCombo : public QObject {
    Q_OBJECT
private slots:

    void initTestCase() { ensureDialog(); }

    /* C-1: modeCombo() non null */
    void modeComboNonNull() {
        QVERIFY2(s_dlg->modeCombo() != nullptr, "modeCombo() è null");
    }

    /* C-2: almeno 2 voci (Pipeline + Byzantino) */
    void almenoduevoci() {
        QVERIFY2(s_dlg->modeCombo()->count() >= 2,
                 "modeCombo deve avere almeno 2 voci");
    }

    /* C-3: voce 0 contiene "Pipeline" */
    void voce0ContienePipeline() {
        const QString txt = s_dlg->modeCombo()->itemText(0);
        QVERIFY2(txt.contains("Pipeline", Qt::CaseInsensitive),
                 qPrintable("voce 0 modeCombo: " + txt));
    }

    /* C-4: voce 1 contiene "Byzantino" o "Byzantine" */
    void voce1ContieneByzan() {
        const QString txt = s_dlg->modeCombo()->itemText(1);
        QVERIFY2(txt.contains("Byzant", Qt::CaseInsensitive),
                 qPrintable("voce 1 modeCombo: " + txt));
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-D — RagDropWidget per-agente
   ══════════════════════════════════════════════════════════════ */
class TestRagPerAgente : public QObject {
    Q_OBJECT
private slots:

    void initTestCase() { ensureDialog(); }

    /* D-1: hasContext() false all'avvio per tutti gli slot */
    void hasContextFalseAllAvvio() {
        for (int i = 0; i < AgentsConfigDialog::MAX_AGENTS; ++i) {
            QVERIFY2(!s_dlg->ragWidget(i)->hasContext(),
                     qPrintable(QString("ragWidget(%1)->hasContext() deve essere false").arg(i)));
        }
    }

    /* D-2: ragContext() vuoto all'avvio */
    void ragContextVuotoAllAvvio() {
        for (int i = 0; i < AgentsConfigDialog::MAX_AGENTS; ++i) {
            QVERIFY2(s_dlg->ragWidget(i)->ragContext().isEmpty(),
                     qPrintable(QString("ragWidget(%1)->ragContext() deve essere vuoto").arg(i)));
        }
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-E — sharedRagWidget (RAG condiviso)
   ══════════════════════════════════════════════════════════════ */
class TestSharedRag : public QObject {
    Q_OBJECT
private slots:

    void initTestCase() { ensureDialog(); }

    /* E-1: sharedRagWidget() non null */
    void sharedRagNonNull() {
        QVERIFY2(s_dlg->sharedRagWidget() != nullptr, "sharedRagWidget() è null");
    }

    /* E-2: hasContext() false all'avvio */
    void sharedRagHasContextFalse() {
        QVERIFY2(!s_dlg->sharedRagWidget()->hasContext(),
                 "sharedRagWidget()->hasContext() deve essere false all'avvio");
    }

    /* E-3: ragContext() vuoto all'avvio */
    void sharedRagContextVuoto() {
        QVERIFY2(s_dlg->sharedRagWidget()->ragContext().isEmpty(),
                 "sharedRagWidget()->ragContext() deve essere vuoto all'avvio");
    }

    /* E-4: shared RAG è un'istanza diversa da quelli per-agente */
    void sharedRagDistintoDaPerAgente() {
        for (int i = 0; i < AgentsConfigDialog::MAX_AGENTS; ++i) {
            QVERIFY2(s_dlg->sharedRagWidget() != s_dlg->ragWidget(i),
                     qPrintable(QString("sharedRag coincide con ragWidget(%1)").arg(i)));
        }
    }
};

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    s_app = &app;

    int status = 0;
    {
        TestStruttura    t1; status |= QTest::qExec(&t1, argc, argv);
        TestNumAgents    t2; status |= QTest::qExec(&t2, argc, argv);
        TestModeCombo    t3; status |= QTest::qExec(&t3, argc, argv);
        TestRagPerAgente t4; status |= QTest::qExec(&t4, argc, argv);
        TestSharedRag    t5; status |= QTest::qExec(&t5, argc, argv);
    }

    delete s_dlg;
    return status;
}

#include "test_agents_config_dialog.moc"
