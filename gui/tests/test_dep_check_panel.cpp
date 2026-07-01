/* ══════════════════════════════════════════════════════════════
   test_dep_check_panel.cpp — Unit test per DepCheckPanel (T-2)

   Categorie:
     CAT-A  Costruzione — no crash, pulsanti presenti, stato iniziale
     CAT-B  Struttura righe — una per dep, label coerenti
     CAT-C  runAllChecks() — avvia QProcess, segnali allOk()/someMissing(int)

   Build:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_dep_check_panel
     ./build_tests/test_dep_check_panel
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QApplication>
#include <QSignalSpy>
#include <QPushButton>
#include <QLabel>
#include "../widgets/widget_dep_check.h"

/* ══════════════════════════════════════════════════════════════
   CAT-A — Costruzione
   ══════════════════════════════════════════════════════════════ */
class TestDepCheckConstruction : public QObject {
    Q_OBJECT
private slots:

    /* A-1: costruzione con lista dep normale, no crash */
    void buildNocrash() {
        QList<DepCheckPanel::Dep> deps = {
            { "faster-whisper", "faster_whisper", "faster-whisper", "", "STT veloce" },
            { "ffmpeg", "", "", "ffmpeg", "Mux/demux audio/video" },
        };
        auto* panel = new DepCheckPanel(deps);
        QVERIFY(panel != nullptr);
        delete panel;
    }

    /* A-2: costruzione con lista vuota, no crash */
    void buildEmptyDepsNocrash() {
        QList<DepCheckPanel::Dep> deps;
        auto* panel = new DepCheckPanel(deps);
        QVERIFY(panel != nullptr);
        delete panel;
    }

    /* A-3: pulsanti "Controlla tutto" e "Installa mancanti" trovabili via tooltip */
    void hasCheckAndInstallButtons() {
        QList<DepCheckPanel::Dep> deps = {
            { "webrtcvad", "webrtcvad", "webrtcvad", "", "" },
        };
        auto* panel = new DepCheckPanel(deps);
        bool foundCheck = false, foundInstall = false;
        for (auto* btn : panel->findChildren<QPushButton*>()) {
            if (btn->toolTip() == "Verifica lo stato di ogni dipendenza") foundCheck = true;
            if (btn->toolTip() == "Installa via pip le dipendenze mancanti") foundInstall = true;
        }
        QVERIFY2(foundCheck, "pulsante 'Controlla tutto' non trovato");
        QVERIFY2(foundInstall, "pulsante 'Installa mancanti' non trovato");
        delete panel;
    }

    /* A-4: pulsante "Installa mancanti" disabilitato prima di ogni check */
    void installAllDisabledInitially() {
        QList<DepCheckPanel::Dep> deps = {
            { "sh", "", "", "sh", "" },
        };
        auto* panel = new DepCheckPanel(deps);
        QPushButton* installAll = nullptr;
        for (auto* btn : panel->findChildren<QPushButton*>())
            if (btn->toolTip() == "Installa via pip le dipendenze mancanti") installAll = btn;
        QVERIFY(installAll != nullptr);
        QVERIFY2(!installAll->isEnabled(), "Installa mancanti deve partire disabilitato");
        delete panel;
    }

    /* A-5: un pulsante install individuale per ogni dep (tooltip "Installa <label>") */
    void oneInstallButtonPerDep() {
        QList<DepCheckPanel::Dep> deps = {
            { "Dep1", "", "", "sh", "" },
            { "Dep2", "", "", "sh", "" },
            { "Dep3", "", "", "sh", "" },
        };
        auto* panel = new DepCheckPanel(deps);
        int found = 0;
        for (auto* btn : panel->findChildren<QPushButton*>())
            if (btn->toolTip().startsWith("Installa Dep")) ++found;
        QCOMPARE(found, 3);
        delete panel;
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B — Struttura righe (una per dep, label coerenti)
   ══════════════════════════════════════════════════════════════ */
class TestDepCheckRows : public QObject {
    Q_OBJECT
private slots:

    /* B-1: ogni dep produce una QLabel col proprio nome */
    void labelPerDepPresente() {
        QList<DepCheckPanel::Dep> deps = {
            { "AlphaDep", "", "", "sh", "" },
            { "BetaDep",  "", "", "sh", "" },
        };
        auto* panel = new DepCheckPanel(deps);
        bool foundAlpha = false, foundBeta = false;
        for (auto* lbl : panel->findChildren<QLabel*>()) {
            if (lbl->text().contains("AlphaDep")) foundAlpha = true;
            if (lbl->text().contains("BetaDep"))  foundBeta = true;
        }
        QVERIFY2(foundAlpha, "label AlphaDep non trovata");
        QVERIFY2(foundBeta, "label BetaDep non trovata");
        delete panel;
    }

    /* B-2: la descrizione, se presente, compare come QLabel separata */
    void descrizioneOpzionale() {
        QList<DepCheckPanel::Dep> deps = {
            { "ConDesc", "", "", "sh", "Descrizione unica XYZ" },
        };
        auto* panel = new DepCheckPanel(deps);
        bool found = false;
        for (auto* lbl : panel->findChildren<QLabel*>())
            if (lbl->text().contains("Descrizione unica XYZ")) found = true;
        QVERIFY2(found, "descrizione non trovata come QLabel");
        delete panel;
    }

    /* B-3: N dep → N label di stato (icona ⏳ iniziale) */
    void statusLabelPerDep() {
        QList<DepCheckPanel::Dep> deps = {
            { "D1", "", "", "sh", "" },
            { "D2", "", "", "sh", "" },
            { "D3", "", "", "sh", "" },
            { "D4", "", "", "sh", "" },
        };
        auto* panel = new DepCheckPanel(deps);
        int pending = 0;
        for (auto* lbl : panel->findChildren<QLabel*>())
            if (lbl->text().contains(QString::fromUtf8("\xe2\x8f\xb3"))) ++pending;
        QCOMPARE(pending, 4);
        delete panel;
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-C — runAllChecks() e segnali allOk()/someMissing(int)
   ══════════════════════════════════════════════════════════════ */
class TestDepCheckRunAll : public QObject {
    Q_OBJECT
private slots:

    /* C-1: lista vuota → allOk() emesso sincrono, nessun someMissing */
    void listaVuotaEmetteAllOk() {
        QList<DepCheckPanel::Dep> deps;
        auto* panel = new DepCheckPanel(deps);
        QSignalSpy spyOk(panel, &DepCheckPanel::allOk);
        QSignalSpy spyMissing(panel, &DepCheckPanel::someMissing);
        panel->runAllChecks();
        QCOMPARE(spyOk.count(), 1);
        QCOMPARE(spyMissing.count(), 0);
        delete panel;
    }

    /* C-2: dep con binario sicuramente presente ("sh") → allOk() entro 5s */
    void depPresenteEmetteAllOk() {
        QList<DepCheckPanel::Dep> deps = {
            { "Shell", "", "", "sh", "" },
        };
        auto* panel = new DepCheckPanel(deps);
        QSignalSpy spyOk(panel, &DepCheckPanel::allOk);
        panel->runAllChecks();
        QVERIFY2(spyOk.wait(5000), "allOk() non emesso entro 5s per dep presente");
        delete panel;
    }

    /* C-3: dep con binario sicuramente assente → someMissing(1) entro 5s */
    void depAssenteEmetteSomeMissing() {
        QList<DepCheckPanel::Dep> deps = {
            { "Fake", "", "", "definitely_not_a_real_binary_xyz123", "" },
        };
        auto* panel = new DepCheckPanel(deps);
        QSignalSpy spyMissing(panel, &DepCheckPanel::someMissing);
        panel->runAllChecks();
        QVERIFY2(spyMissing.wait(5000), "someMissing() non emesso entro 5s per dep assente");
        QCOMPARE(spyMissing.count(), 1);
        QCOMPARE(spyMissing.takeFirst().at(0).toInt(), 1);
        delete panel;
    }

    /* C-4: dopo il check con dep mancante e pipPkg valorizzato, "Installa mancanti" si abilita */
    void installAllAbilitatoDopoMissing() {
        QList<DepCheckPanel::Dep> deps = {
            { "Fake", "", "fake-pip-pkg", "definitely_not_a_real_binary_xyz123", "" },
        };
        auto* panel = new DepCheckPanel(deps);
        QSignalSpy spyMissing(panel, &DepCheckPanel::someMissing);
        panel->runAllChecks();
        QVERIFY(spyMissing.wait(5000));

        QPushButton* installAll = nullptr;
        for (auto* btn : panel->findChildren<QPushButton*>())
            if (btn->toolTip() == "Installa via pip le dipendenze mancanti") installAll = btn;
        QVERIFY(installAll != nullptr);
        QVERIFY2(installAll->isEnabled(), "Installa mancanti deve abilitarsi dopo il check");
        delete panel;
    }

    /* C-5: runAllChecks() chiamato due volte di seguito non crasha */
    void runAllChecksDueVolteNoCrash() {
        QList<DepCheckPanel::Dep> deps = {
            { "Shell", "", "", "sh", "" },
        };
        auto* panel = new DepCheckPanel(deps);
        QSignalSpy spyOk(panel, &DepCheckPanel::allOk);
        panel->runAllChecks();
        QVERIFY(spyOk.wait(5000));
        spyOk.clear();
        panel->runAllChecks();
        QVERIFY2(spyOk.wait(5000), "secondo runAllChecks() non ha completato entro 5s");
        delete panel;
    }
};

/* ══════════════════════════════════════════════════════════════
   Runner
   ══════════════════════════════════════════════════════════════ */
int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    int status = 0;
    { TestDepCheckConstruction t; status |= QTest::qExec(&t, argc, argv); }
    { TestDepCheckRows         t; status |= QTest::qExec(&t, argc, argv); }
    { TestDepCheckRunAll       t; status |= QTest::qExec(&t, argc, argv); }
    return status;
}

#include "test_dep_check_panel.moc"
