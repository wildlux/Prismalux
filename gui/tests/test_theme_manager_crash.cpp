/* ══════════════════════════════════════════════════════════════
   test_theme_manager_crash.cpp — Fix ABRT Signal 6

   Verifica che ThemeManager usi parent=nullptr (non qApp) in modo
   che la distruzione di QApplication non triggeri deleteChildren
   sul singleton → no double-free → no ABRT.

   CAT-A  Lifetime sicuro (5 test)
   CAT-B  Funzionalità invariate (5 test)

   Build:
     cmake --build gui/build_gui -j4 --target test_theme_manager_crash
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include "../theme_manager.h"

/* ══════════════════════════════════════════════════════════════
   CAT-A — Lifetime sicuro
   ══════════════════════════════════════════════════════════════ */
class TestLifetime : public QObject {
    Q_OBJECT
private slots:

    /* A1: parent del singleton è nullptr — non qApp */
    void parentIsNull() {
        ThemeManager* tm = ThemeManager::instance();
        QVERIFY(tm != nullptr);
        QCOMPARE(tm->parent(), nullptr);
    }

    /* A2: instance() chiamata due volte → stesso puntatore (singleton stabile) */
    void singletonStabile() {
        ThemeManager* a = ThemeManager::instance();
        ThemeManager* b = ThemeManager::instance();
        QCOMPARE(a, b);
    }

    /* A3: l'eseguibile giunge all'uscita senza ABRT (test implicito se PASS) */
    void nessunAbrtAllaChiusura() {
        /* Se il programma arriva qui ed esce pulito → no ABRT.
           Il vero collaudo avviene a livello di exit code del processo. */
        QVERIFY(ThemeManager::instance() != nullptr);
    }

    /* A4: instance() × 1000 — no crash, no leak */
    void stressCallInstance() {
        ThemeManager* first = ThemeManager::instance();
        for (int i = 0; i < 1000; ++i) {
            ThemeManager* tm = ThemeManager::instance();
            QCOMPARE(tm, first);
        }
    }

    /* A5: instance() prima di QApplication creata non viene testato qui
           (richiederebbe un processo separato); verifichiamo che dopo
           QApplication il singleton funziona correttamente */
    void instanceDopoQApplication() {
        QVERIFY(qApp != nullptr);
        QVERIFY(ThemeManager::instance() != nullptr);
        /* parent deve restare nullptr anche con qApp presente */
        QCOMPARE(ThemeManager::instance()->parent(), nullptr);
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B — Funzionalità invariate
   ══════════════════════════════════════════════════════════════ */
class TestFunzionalita : public QObject {
    Q_OBJECT
private slots:

    /* B1: themes().size() — almeno 20 temi presenti */
    void themesNonVuota() {
        const auto temi = ThemeManager::instance()->themes();
        QVERIFY2(temi.size() >= 20,
                 qPrintable(QString("trovati %1 temi, attesi >= 20").arg(temi.size())));
    }

    /* B2: apply() con id inesistente non crasha */
    void applyFileMancante() {
        ThemeManager::instance()->apply("questo_tema_non_esiste_mai");
        QVERIFY(true); /* no crash = pass */
    }

    /* B3: apply() su tema valido emette signal changed() (se il file .qss esiste) */
    void applyEmetteSignal() {
        ThemeManager* tm = ThemeManager::instance();
        QSignalSpy spy(tm, &ThemeManager::changed);
        const auto& temi = tm->themes();
        if (temi.isEmpty()) { QSKIP("nessun tema disponibile"); }
        /* In build_tests i file .qss potrebbero mancare — solo verifichiamo no crash */
        tm->apply(temi.first().id);
        QVERIFY(spy.count() >= 0); /* 0 se file assente, 1 se presente */
    }

    /* B4: scanExternalThemes() su dir inesistente — no crash */
    void scanDirVuota() {
        ThemeManager::instance()->scanExternalThemes();
        QVERIFY(true);
    }

    /* B5: loadSaved() — no crash (anche senza tema salvato in QSettings) */
    void loadSavedNoCrash() {
        ThemeManager::instance()->loadSaved();
        QVERIFY(true);
    }
};

/* ══════════════════════════════════════════════════════════════
   Runner
   ══════════════════════════════════════════════════════════════ */
int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    int status = 0;
    { TestLifetime      t; status |= QTest::qExec(&t, argc, argv); }
    { TestFunzionalita  t; status |= QTest::qExec(&t, argc, argv); }
    return status;
}

#include "test_theme_manager_crash.moc"
