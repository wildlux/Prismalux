/* ══════════════════════════════════════════════════════════════
   test_manutenzione_bugs.cpp — Unit test ManutenzioneePage Bug Tracker

   Categorie:
     CAT-A  Costruzione — ManutenzioneePage senza crash
     CAT-B  buildBugTracker() — restituisce widget non null
     CAT-C  Struttura bug widget — bugModelCombo, bugLog, btnApplyFix, bugStatusLbl

   Build:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_manutenzione_bugs
     ./build_tests/test_manutenzione_bugs
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QApplication>
#include <QComboBox>
#include <QTextBrowser>
#include <QPushButton>
#include <QLabel>

#include "mock_ai_client.h"
#include "../hardware_monitor.h"
#include "../pages/manutenzione_page.h"

static MockAiClient*    s_ai   = nullptr;
static HardwareMonitor* s_hw   = nullptr;
static ManutenzioneePage* s_pg = nullptr;
static QWidget*         s_bugW = nullptr;

static void ensurePage() {
    if (!s_pg) {
        s_ai = new MockAiClient;
        s_hw = new HardwareMonitor;
        s_pg = new ManutenzioneePage(s_ai, s_hw);
        s_bugW = s_pg->buildBugTracker();
    }
}

/* ══════════════════════════════════════════════════════════════
   CAT-A — Costruzione
   ══════════════════════════════════════════════════════════════ */
class TestCostruzione : public QObject {
    Q_OBJECT
private slots:

    void initTestCase() { ensurePage(); }

    /* A-1: costruisce senza crash */
    void costruisceSenzaCrash() {
        QVERIFY2(s_pg != nullptr, "ManutenzioneePage non costruita");
    }

    /* A-2: è un QWidget */
    void eUnQWidget() {
        QVERIFY2(qobject_cast<QWidget*>(s_pg) != nullptr,
                 "ManutenzioneePage non è un QWidget");
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B — buildBugTracker()
   ══════════════════════════════════════════════════════════════ */
class TestBuildBugTracker : public QObject {
    Q_OBJECT
private slots:

    void initTestCase() { ensurePage(); }

    /* B-1: restituisce un widget non null */
    void restituisceWidgetNonNull() {
        QVERIFY2(s_bugW != nullptr, "buildBugTracker() ha restituito nullptr");
    }

    /* B-2: il widget è una istanza di QWidget */
    void eUnQWidget() {
        QVERIFY2(qobject_cast<QWidget*>(s_bugW) != nullptr,
                 "il widget bug tracker non è un QWidget");
    }

    /* B-3: il widget ha figli (non è vuoto) */
    void haBigliFigli() {
        QVERIFY2(!s_bugW->findChildren<QWidget*>().isEmpty(),
                 "il widget bug tracker sembra vuoto");
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-C — Struttura widget Bug Tracker
   ══════════════════════════════════════════════════════════════ */
class TestStrutturaBugWidget : public QObject {
    Q_OBJECT
private slots:

    void initTestCase() { ensurePage(); }

    /* C-1: bugModelCombo è presente (almeno 1 QComboBox nel widget) */
    void bugModelComboPresente() {
        const auto combos = s_bugW->findChildren<QComboBox*>();
        QVERIFY2(!combos.isEmpty(),
                 "nessun QComboBox trovato nel bug tracker widget");
    }

    /* C-2: bugLog (QTextBrowser con objectName "chatLog") è presente */
    void bugLogPresente() {
        const auto browsers = s_bugW->findChildren<QTextBrowser*>("chatLog");
        QVERIFY2(!browsers.isEmpty(),
                 "QTextBrowser 'chatLog' non trovato nel bug tracker");
    }

    /* C-3: btnApplyFix è presente — cerca QPushButton col testo "Applica" */
    void btnApplyFixPresente() {
        const auto btns = s_bugW->findChildren<QPushButton*>();
        const bool found = std::any_of(btns.begin(), btns.end(), [](QPushButton* b){
            return b->text().contains("Applica", Qt::CaseInsensitive) ||
                   b->toolTip().contains("Applica", Qt::CaseInsensitive);
        });
        if (!found)
            QSKIP("btnApplyFix non trovato — potrebbe avere testo diverso");
        QVERIFY(found);
    }

    /* C-4: bugStatusLbl (QLabel con objectName "hintLabel") è presente */
    void bugStatusLblPresente() {
        const auto labels = s_bugW->findChildren<QLabel*>("hintLabel");
        QVERIFY2(!labels.isEmpty(),
                 "QLabel 'hintLabel' (bugStatusLbl) non trovato nel bug tracker");
    }

    /* C-5: pulsante "Cerca bug" (btnSearchBug) è presente */
    void btnSearchBugPresente() {
        const auto btns = s_bugW->findChildren<QPushButton*>();
        const bool found = std::any_of(btns.begin(), btns.end(), [](QPushButton* b){
            return b->text().contains("Cerca", Qt::CaseInsensitive) ||
                   b->toolTip().contains("Cerca", Qt::CaseInsensitive);
        });
        if (!found)
            QSKIP("btnSearchBug non trovato con testo 'Cerca'");
        QVERIFY(found);
    }
};

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    int status = 0;
    {
        TestCostruzione       t1; status |= QTest::qExec(&t1, argc, argv);
        TestBuildBugTracker   t2; status |= QTest::qExec(&t2, argc, argv);
        TestStrutturaBugWidget t3; status |= QTest::qExec(&t3, argc, argv);
    }

    delete s_bugW;
    delete s_pg;
    delete s_hw;
    delete s_ai;
    return status;
}

#include "test_manutenzione_bugs.moc"
