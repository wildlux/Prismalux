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
#include <QRegularExpression>
#include <QCoreApplication>
#include <QApplication>
#include <QSettings>
#include <QVBoxLayout>
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QTextEdit>
#include "../theme_manager.h"
#include "../prismalux_paths.h"

namespace P = PrismaluxPaths;

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

    /* D-75/W-9: il set di ID deve essere identico tra desktop e webchat LAN.
       La webchat è la fonte dei temi disponibili in LAN; ogni tema web deve
       esistere come QSS Qt e viceversa. Le preferenze restano separate
       (QSettings desktop vs localStorage web). */
    void paritaTemiConWebchat() {
        const QString path = QStringLiteral(PRISMALUX_ROOT "/gui/lan_web/webchat.html");
        QFile f(path);
        QVERIFY2(f.open(QIODevice::ReadOnly | QIODevice::Text),
                 qPrintable("webchat non trovata: " + path));
        const QString html = QString::fromUtf8(f.readAll());

        QRegularExpression block(R"(THEMES\s*=\s*\{(.*?)\};)",
                                 QRegularExpression::DotMatchesEverythingOption);
        const auto bm = block.match(html);
        QVERIFY2(bm.hasMatch(), "blocco THEMES non trovato nella webchat");

        QSet<QString> web;
        QRegularExpression key(R"((?:^|[,{])\s*([a-z0-9_]+)\s*:\s*\{)");
        auto it = key.globalMatch(bm.captured(1));
        while (it.hasNext())
            web.insert(it.next().captured(1));
        QVERIFY2(web.size() >= 20,
                 qPrintable(QString("temi web trovati: %1").arg(web.size())));

        QSet<QString> qt;
        for (const ThemeManager::Theme& t : ThemeManager::instance()->themes())
            qt.insert(t.id);

        const QSet<QString> soloWeb = web - qt;
        const QSet<QString> soloQt  = qt - web;
        QVERIFY2(soloWeb.isEmpty(),
                 qPrintable("temi web assenti in Qt: "
                            + QStringList(soloWeb.values()).join(", ")));
        QVERIFY2(soloQt.isEmpty(),
                 qPrintable("temi Qt assenti nel web: "
                            + QStringList(soloQt.values()).join(", ")));
    }

    /* Ogni tema built-in deve avere il file QSS corrispondente: un ID
       registrato senza file farebbe fallire apply() in silenzio. */
    void ogniTemaHaFileQss() {
        for (const ThemeManager::Theme& t : ThemeManager::instance()->themes()) {
            const QString p = QStringLiteral(PRISMALUX_ROOT "/gui/themes/")
                              + t.id + ".qss";
            QVERIFY2(QFile::exists(p), qPrintable("QSS mancante: " + p));
        }
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

    /* Parità visiva D-75: applica i tre temi aggiunti e salva un'anteprima
       (solo con PRISMALUX_THEME_SCREENSHOT impostata). Verifica anche che il
       QSS venga letto da disco: un file mancante lascerebbe currentId()
       invariato e lo stylesheet vuoto. */
    void applyNuoviTemiConScreenshot() {
        const QString shot = qEnvironmentVariable("PRISMALUX_THEME_SCREENSHOT");
        if (shot.isEmpty())
            QSKIP("PRISMALUX_THEME_SCREENSHOT non impostata");

        QSettings s("Prismalux", "GUI");
        const QString prev = s.value(P::SK::kTheme).toString();

        QWidget w;
        auto* lay = new QVBoxLayout(&w);
        auto* title = new QLabel("Prismalux — anteprima tema", &w);
        title->setObjectName("pageTitle");
        lay->addWidget(title);
        auto* btn = new QPushButton("Pulsante azione", &w);
        btn->setObjectName("actionBtn");
        lay->addWidget(btn);
        auto* input = new QLineEdit("Testo di esempio", &w);
        lay->addWidget(input);
        auto* log = new QTextEdit("Bolla AI di esempio", &w);
        log->setObjectName("chatLog");
        lay->addWidget(log);
        w.resize(520, 360);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        const QStringList ids{ "dark", "dracula", "nord" };
        for (const QString& id : ids) {
            ThemeManager::instance()->apply(id);
            QCOMPARE(ThemeManager::instance()->currentId(), id);
            QVERIFY2(!qApp->styleSheet().isEmpty(),
                     qPrintable("QSS vuoto per " + id));
            QVERIFY2(w.grab().save(shot + "_" + id + ".png"),
                     qPrintable("screenshot fallito per " + id));
        }

        /* apply() scrive la preferenza: ripristina quella precedente. */
        ThemeManager::instance()->apply(prev.isEmpty()
                                        ? QStringLiteral("dark_ocean") : prev);
        s.setValue(P::SK::kTheme, prev);
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
