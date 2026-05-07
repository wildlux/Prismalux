/* ══════════════════════════════════════════════════════════════
   test_theme_manager.cpp — Unit test per ThemeManager
   ──────────────────────────────────────────────────────────────
   Categorie:
     CAT-A  Singleton + lista temi (9 test)
     CAT-B  apply() + operazioni (5 test)

   Note: apply() tenta di aprire file .qss da disco. In ambiente
   test (build_tests/) i file temi non esistono → apply() ritorna
   senza modificare currentId() e senza emettere changed(). I test
   CAT-B verificano solo che le chiamate non crashino.

   Build:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_theme_manager
     ./build_tests/test_theme_manager
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include <QCoreApplication>
#include "../theme_manager.h"

/* ══════════════════════════════════════════════════════════════
   CAT-A — Singleton + lista temi (9 test)
   ══════════════════════════════════════════════════════════════ */
class TestThemeList : public QObject {
    Q_OBJECT
private slots:

    void instanceNonNull() {
        QVERIFY(ThemeManager::instance() != nullptr);
    }

    void stessoPointereDueChiamate() {
        QCOMPARE(ThemeManager::instance(), ThemeManager::instance());
    }

    void temiNonVuota() {
        QVERIFY(!ThemeManager::instance()->themes().isEmpty());
    }

    void minimoVentiTemi() {
        QVERIFY2(ThemeManager::instance()->themes().size() >= 20,
                 qPrintable(QString("temi trovati: %1 (attesi >= 20)")
                            .arg(ThemeManager::instance()->themes().size())));
    }

    void tuttiIDNonVuoti() {
        for (const ThemeManager::Theme& t : ThemeManager::instance()->themes())
            QVERIFY2(!t.id.isEmpty(),
                     qPrintable("tema con id vuoto: label=" + t.label));
    }

    void tuttiLabelNonVuoti() {
        for (const ThemeManager::Theme& t : ThemeManager::instance()->themes())
            QVERIFY2(!t.label.isEmpty(),
                     qPrintable("tema con label vuota: id=" + t.id));
    }

    void idUnivoci() {
        QSet<QString> visti;
        for (const ThemeManager::Theme& t : ThemeManager::instance()->themes()) {
            QVERIFY2(!visti.contains(t.id),
                     qPrintable("id tema duplicato: " + t.id));
            visti.insert(t.id);
        }
    }

    void darkCyanPresente() {
        bool trovato = false;
        for (const ThemeManager::Theme& t : ThemeManager::instance()->themes())
            if (t.id == "dark_cyan") trovato = true;
        QVERIFY2(trovato, "dark_cyan deve essere nella lista dei temi built-in");
    }

    void lightPresente() {
        bool trovato = false;
        for (const ThemeManager::Theme& t : ThemeManager::instance()->themes())
            if (t.id == "light") trovato = true;
        QVERIFY2(trovato, "tema 'light' deve essere nella lista dei temi built-in");
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B — apply() + operazioni (5 test)
   ══════════════════════════════════════════════════════════════ */
class TestThemeOps : public QObject {
    Q_OBJECT
private slots:

    void applyIDNonEsistente_NoCrash() {
        /* Tema inesistente → ritorno immediato, nessun crash */
        ThemeManager::instance()->apply("TEMA_INESISTENTE_XYZ_123");
    }

    void applyStringaVuota_NoCrash() {
        ThemeManager::instance()->apply("");
    }

    void currentIdDopoApplyBogus_Invariato() {
        const QString before = ThemeManager::instance()->currentId();
        ThemeManager::instance()->apply("NON_ESISTE");
        QCOMPARE(ThemeManager::instance()->currentId(), before);
    }

    void scanExternalThemesNoCrash() {
        /* Se non ci sono file .qss esterni → nessun crash */
        ThemeManager::instance()->scanExternalThemes();
    }

    void loadSavedNoCrash() {
        /* loadSaved() usa QSettings → potrebbe non trovare nulla in test */
        ThemeManager::instance()->loadSaved();
    }
};

/* ══════════════════════════════════════════════════════════════
   Runner
   ══════════════════════════════════════════════════════════════ */
int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    int status = 0;
    { TestThemeList t; status |= QTest::qExec(&t, argc, argv); }
    { TestThemeOps t;  status |= QTest::qExec(&t, argc, argv); }
    return status;
}

#include "test_theme_manager.moc"
