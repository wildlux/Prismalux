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

    /* Performance: cliccare il tema già attivo non deve riapplicare il QSS.
       reapply() invece deve forzare (serve allo zoom). */
    void applyStessoTemaNonRiemette() {
        ThemeManager* tm = ThemeManager::instance();
        QSettings s("Prismalux", "GUI");
        const QString prevSaved = s.value(P::SK::kTheme).toString();

        tm->apply("dark_cyan", /*force=*/true);
        QCOMPARE(tm->currentId(), QStringLiteral("dark_cyan"));

        QSignalSpy spy(tm, &ThemeManager::changed);
        tm->apply("dark_cyan");
        QCOMPARE(spy.count(), 0);

        tm->reapply();
        QCOMPARE(spy.count(), 1);

        if (prevSaved.isEmpty()) s.remove(P::SK::kTheme);
        else                   s.setValue(P::SK::kTheme, prevSaved);
        if (!prevSaved.isEmpty())
            tm->apply(prevSaved, /*force=*/true);
    }

    void scanExternalThemesNoCrash() {
        /* Se non ci sono file .qss esterni → nessun crash */
        ThemeManager::instance()->scanExternalThemes();
        /* base.qss contiene solo regole strutturali: non è un tema. */
        for (const ThemeManager::Theme& t : ThemeManager::instance()->themes())
            QVERIFY2(t.id != "base", "base.qss non deve apparire come tema custom");
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
   CAT-C — Parità web ↔ desktop (LW-6)
   ──────────────────────────────────────────────────────────────
   La webchat deve rispecchiare la gerarchia desktop: sezioni,
   sotto-tab e decisioni "solo desktop" (LW-8 in TODO.md).
   Questo test è il guard anti-drift: aggiungere una tab desktop
   senza la controparte web (o viceversa) lo fa fallire.
   ══════════════════════════════════════════════════════════════ */
class TestWebParity : public QObject {
    Q_OBJECT

    static QString html() {
        QFile f(QStringLiteral(PRISMALUX_ROOT "/gui/lan_web/webchat.html"));
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return QString();
        return QString::fromUtf8(f.readAll());
    }

    /* Fonte di verità della mappa: TODO.md LW-1 + decisioni LW-8. */
    static QMap<QString, QSet<QString>> attese() {
        return {
            { "ai",        { "chat", "age", "aut", "rag", "grf", "knw" } },
            { "strumenti", { "imp", "sfd", "fil", "crn", "ric", "sys", "wik" } },
            { "media",     { "mul", "wsp", "osc", "gvz" } },
            { "prog",      { "cod", "repl", "git", "sec" } },
            { "math",      { "mat" } },
            { "util",      { "fin", "tfr", "irp", "piv", "lav", "apps" } },
            { "lanwan",    { "lan", "net", "wanro", "mag" } },
        };
    }

    static QMap<QString, QStringList> righe(const QString& h) {
        QMap<QString, QStringList> out;
        QRegularExpression re(
            QStringLiteral("<div class=\"tab-row[^\"]*\" data-sec-row=\"([a-z]+)\"[^>]*>(.*?)</div>"),
            QRegularExpression::DotMatchesEverythingOption);
        auto it = re.globalMatch(h);
        while (it.hasNext()) {
            const auto m = it.next();
            QStringList ids;
            QRegularExpression tab(QStringLiteral("data-tab=\"([a-z]+)\""));
            auto ti = tab.globalMatch(m.captured(2));
            while (ti.hasNext()) ids << ti.next().captured(1);
            out.insert(m.captured(1), ids);
        }
        return out;
    }

private slots:
    void webchatLeggibile() {
        QVERIFY2(!html().isEmpty(),
                 "webchat.html non trovata o vuota (PRISMALUX_ROOT)");
    }

    /* Ogni sezione dichiarata ha esattamente le sotto-tab attese. */
    void ogniSezioneHaLeSueTab() {
        const auto r = righe(html());
        const auto a = attese();
        QCOMPARE(r.keys(), a.keys());
        for (auto it = a.cbegin(); it != a.cend(); ++it) {
            QVERIFY2(r.contains(it.key()),
                     qPrintable("sezione web mancante: " + it.key()));
            const QStringList lst = r.value(it.key());
            const QSet<QString> web(lst.begin(), lst.end());
            QCOMPARE(web, it.value());
            QCOMPARE(lst.size(), it.value().size()); /* nessun duplicato */
        }
    }

    /* Nessuna tab fuori da una sezione e nessun ID duplicato. */
    void nessunaTabOrfana() {
        const auto r = righe(html());
        QSet<QString> viste;
        for (auto it = r.cbegin(); it != r.cend(); ++it) {
            for (const QString& id : it.value()) {
                QVERIFY2(!viste.contains(id),
                         qPrintable("tab duplicata tra sezioni: " + id));
                viste.insert(id);
            }
        }
        QRegularExpression all(QStringLiteral("data-tab=\"([a-z]+)\""));
        auto ai = all.globalMatch(html());
        QSet<QString> tutte;
        while (ai.hasNext()) tutte.insert(ai.next().captured(1));
        QCOMPARE(viste, tutte);
    }

    /* Bioinformatica e TeleComanda sono dichiarate "solo desktop" (LW-8). */
    void sezioniSoloDesktopAssenti() {
        const QString h = html();
        QVERIFY2(!h.contains("data-sec=\"bio\"") &&
                 !h.contains("data-sec=\"tele\""),
                 "sezioni dichiarate solo-desktop presenti nel web");
    }

    /* Deep-link e memoria di sezione devono restare implementati. */
    void deeplinkEMemoriaSezione() {
        const QString h = html();
        QVERIFY2(h.contains("hashchange"), "manca il listener hashchange");
        QVERIFY2(h.contains("LAST_SEC"), "manca la memoria ultima tab/sezione");
        QVERIFY2(h.contains("activateSection("), "manca activateSection()");
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
    { TestWebParity t; status |= QTest::qExec(&t, argc, argv); }
    return status;
}

#include "test_theme_manager.moc"
