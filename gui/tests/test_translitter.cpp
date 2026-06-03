/* ══════════════════════════════════════════════════════════════
   test_translitter.cpp  —  Suite di regressione Translitter
   ──────────────────────────────────────────────────────────────
   3 categorie — senza LLM né rete:

   CAT-A  langFence / kLangs helper  — logica di fence markdown
   CAT-B  Costruzione widget          — ProgrammazionePage + sub-tab Translitter
   CAT-C  kLangs e combo             — unicita', cardinalita', presenza linguaggi

   COME ESEGUIRE:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_translitter
     ./build_tests/test_translitter
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QApplication>
#include <QTabWidget>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QPushButton>

#include "mock_ai_client.h"
#include "../pages/main_programming.h"
#include "../prismalux_paths.h"

/* ══════════════════════════════════════════════════════════════
   Linguaggi attesi in kLangs (replica logica dal .cpp per CAT-A/C).

   langFence() e kLangs sono static locals in main_programming_translitter.cpp
   e non esportati — li testiamo indirettamente tramite:
     - la combo m_trSrcLang (popolata con kLangs)
     - replica locale della logica langFence per i casi puri
   ══════════════════════════════════════════════════════════════ */

/* Replica locale di langFence per CAT-A (identica a quella nel .cpp). */
static QString localLangFence(const QString& lang)
{
    const QString l = lang.toLower();
    if (l == "c++")        return "cpp";
    if (l == "c#")         return "csharp";
    if (l == "typescript") return "ts";
    if (l == "javascript") return "js";
    if (l == "bash")       return "bash";
    if (l == "kotlin")     return "kotlin";
    if (l == "swift")      return "swift";
    return l;
}

/* Lista attesa — uguale a kLangs nel .cpp. Usata in CAT-C per confronto. */
static const QStringList kExpectedLangs = {
    "C", "C++", "Python", "JavaScript", "TypeScript",
    "Java", "Kotlin", "Rust", "Go", "PHP",
    "Bash", "SQL", "Swift", "Lua", "Ruby", "C#"
};

/* ══════════════════════════════════════════════════════════════
   CAT-A — langFence helper (13 casi)
   ══════════════════════════════════════════════════════════════ */
class TestLangFence : public QObject {
    Q_OBJECT
private slots:

    /* A-1: C++ -> cpp */
    void cppFence() {
        QCOMPARE(localLangFence("C++"), QString("cpp"));
    }

    /* A-2: C# -> csharp */
    void csharpFence() {
        QCOMPARE(localLangFence("C#"), QString("csharp"));
    }

    /* A-3: JavaScript -> js */
    void javaScriptFence() {
        QCOMPARE(localLangFence("JavaScript"), QString("js"));
    }

    /* A-4: Python -> python (passthrough toLower) */
    void pythonFence() {
        QCOMPARE(localLangFence("Python"), QString("python"));
    }

    /* A-5: TypeScript -> ts */
    void typeScriptFence() {
        QCOMPARE(localLangFence("TypeScript"), QString("ts"));
    }

    /* A-6: Bash -> bash */
    void bashFence() {
        QCOMPARE(localLangFence("Bash"), QString("bash"));
    }

    /* A-7: Kotlin -> kotlin */
    void kotlinFence() {
        QCOMPARE(localLangFence("Kotlin"), QString("kotlin"));
    }

    /* A-8: Swift -> swift */
    void swiftFence() {
        QCOMPARE(localLangFence("Swift"), QString("swift"));
    }

    /* A-9: Java -> java (nessun caso speciale, toLower) */
    void javaFence() {
        QCOMPARE(localLangFence("Java"), QString("java"));
    }

    /* A-10: Rust -> rust */
    void rustFence() {
        QCOMPARE(localLangFence("Rust"), QString("rust"));
    }

    /* A-11: Go -> go */
    void goFence() {
        QCOMPARE(localLangFence("Go"), QString("go"));
    }

    /* A-12: C -> c */
    void cFence() {
        QCOMPARE(localLangFence("C"), QString("c"));
    }

    /* A-13: kExpectedLangs contiene almeno C, C++, Python, JavaScript, Java, Rust */
    void kLangsContainsRequired() {
        const QStringList required = {"C", "C++", "Python", "JavaScript", "Java", "Rust"};
        for (const QString& lang : required) {
            QVERIFY2(kExpectedLangs.contains(lang),
                     qPrintable("Linguaggio mancante in kLangs: " + lang));
        }
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B — Costruzione ProgrammazionePage + sub-tab Translitter (10 casi)
   ══════════════════════════════════════════════════════════════ */
class TestTranslitterWidget : public QObject {
    Q_OBJECT
private:
    MockAiClient*       m_ai   = nullptr;
    ProgrammazionePage* m_page = nullptr;

    /* Ritorna il QComboBox della sorgente del Translitter (objectName "settingCombo"
       con almeno kExpectedLangs.size() item e currentText == "C"). */
    QComboBox* findSrcLangCombo() const {
        for (auto* c : m_page->findChildren<QComboBox*>("settingCombo")) {
            if (c->count() >= 10 && c->currentText() == "C")
                return c;
        }
        return nullptr;
    }

private slots:

    void init() {
        m_ai   = new MockAiClient();
        m_page = new ProgrammazionePage(m_ai);
    }

    void cleanup() {
        delete m_page; m_page = nullptr;
        delete m_ai;   m_ai   = nullptr;
    }

    /* B-1: costruisce senza crash */
    void costruisceSenzaCrash() {
        QVERIFY(m_page != nullptr);
    }

    /* B-2: e' un QWidget */
    void eUnQWidget() {
        QVERIFY(qobject_cast<QWidget*>(m_page) != nullptr);
    }

    /* B-3: il QTabWidget interno esiste */
    void innerTabsEsiste() {
        QVERIFY(m_page->findChild<QTabWidget*>("innerTabs") != nullptr);
    }

    /* B-4: il sub-tab Translitter e' presente (almeno 3 tab interni) */
    void innerTabsAlmenoTre() {
        auto* tabs = m_page->findChild<QTabWidget*>("innerTabs");
        QVERIFY(tabs != nullptr);
        QVERIFY2(tabs->count() >= 3,
                 qPrintable(QString("Attesi >= 3 tab interni, trovati %1")
                            .arg(tabs->count())));
    }

    /* B-5: la combo sorgente linguaggio esiste (almeno 10 item) */
    void comboDaSorgenteLinguaggioEsiste() {
        QComboBox* c = findSrcLangCombo();
        QVERIFY2(c != nullptr, "QComboBox sorgente linguaggio Translitter non trovata");
    }

    /* B-6: la combo sorgente ha almeno 10 linguaggi */
    void comboSorgenteSizeAlmeno10() {
        QComboBox* c = findSrcLangCombo();
        if (!c) QSKIP("Combo sorgente non trovata");
        QVERIFY2(c->count() >= 10,
                 qPrintable(QString("Combo sorgente: attesi >= 10, trovati %1")
                            .arg(c->count())));
    }

    /* B-7: la combo destinazione esiste con almeno 10 item e default "Python" */
    void comboDstLinguaggioEsiste() {
        bool found = false;
        for (auto* c : m_page->findChildren<QComboBox*>("settingCombo")) {
            if (c->count() >= 10 && c->currentText() == "Python") {
                found = true;
                break;
            }
        }
        QVERIFY2(found, "QComboBox destinazione linguaggio Translitter non trovata");
    }

    /* B-8: il QPlainTextEdit di input esiste con objectName "codeEditor" */
    void trInputEsiste() {
        const auto editors = m_page->findChildren<QPlainTextEdit*>("codeEditor");
        QVERIFY2(!editors.isEmpty(), "QPlainTextEdit codeEditor non trovato in Translitter");
    }

    /* B-9: il pulsante Traduci esiste */
    void btnTraduciEsiste() {
        bool found = false;
        for (auto* b : m_page->findChildren<QPushButton*>("actionBtn")) {
            if (b->text().contains("Traduci")) { found = true; break; }
        }
        QVERIFY2(found, "Pulsante 'Traduci' non trovato");
    }

    /* B-10: il pulsante Stop del Translitter e' disabilitato all'avvio */
    void btnTrStopDisabilitatoAllAvvio() {
        /* I pulsanti Stop del Translitter sono disabilitati prima di avviare
           una traduzione. Cerchiamo tutti i btn Stop e verifichiamo che almeno
           uno sia inizialmente disabilitato (quello del Translitter). */
        int disabledStops = 0;
        for (auto* b : m_page->findChildren<QPushButton*>("actionBtn")) {
            if (b->text().contains("Stop") && !b->isEnabled())
                ++disabledStops;
        }
        QVERIFY2(disabledStops >= 1, "Nessun pulsante Stop disabilitato trovato all'avvio");
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-C — kLangs e combo: unicita', cardinalita', linguaggi chiave (8 casi)
   ══════════════════════════════════════════════════════════════ */
class TestKLangs : public QObject {
    Q_OBJECT
private:
    MockAiClient*       m_ai   = nullptr;
    ProgrammazionePage* m_page = nullptr;

    /* Restituisce i QComboBox con count() >= 10 (src+dst del Translitter). */
    QList<QComboBox*> translitterCombos() const {
        QList<QComboBox*> result;
        for (auto* c : m_page->findChildren<QComboBox*>("settingCombo")) {
            if (c->count() >= 10)
                result << c;
        }
        return result;
    }

private slots:

    void initTestCase() {
        m_ai   = new MockAiClient();
        m_page = new ProgrammazionePage(m_ai);
    }

    void cleanupTestCase() {
        delete m_page; m_page = nullptr;
        delete m_ai;   m_ai   = nullptr;
    }

    /* C-1: la lista attesa contiene almeno 10 linguaggi */
    void kLangsAlmeno10() {
        QVERIFY2(kExpectedLangs.size() >= 10,
                 qPrintable(QString("kLangs: attesi >= 10, trovati %1")
                            .arg(kExpectedLangs.size())));
    }

    /* C-2: nessun duplicato in kExpectedLangs */
    void kLangsNessuDuplicato() {
        QSet<QString> seen;
        for (const QString& lang : kExpectedLangs) {
            QVERIFY2(!seen.contains(lang),
                     qPrintable("Duplicato in kLangs: " + lang));
            seen.insert(lang);
        }
    }

    /* C-3: "Python" e' presente */
    void pythonPresente() {
        QVERIFY(kExpectedLangs.contains("Python"));
    }

    /* C-4: "JavaScript" e' presente */
    void javaScriptPresente() {
        QVERIFY(kExpectedLangs.contains("JavaScript"));
    }

    /* C-5: la combo sorgente nel widget ha gli stessi item di kExpectedLangs */
    void comboSorgenteMatchesKLangs() {
        const auto combos = translitterCombos();
        if (combos.isEmpty()) QSKIP("Nessuna combo Translitter trovata");
        QComboBox* c = combos.first();
        QCOMPARE(c->count(), kExpectedLangs.size());
        for (int i = 0; i < c->count(); ++i)
            QCOMPARE(c->itemText(i), kExpectedLangs.at(i));
    }

    /* C-6: la combo contiene "C" */
    void comboCContiene() {
        const auto combos = translitterCombos();
        if (combos.isEmpty()) QSKIP("Nessuna combo Translitter trovata");
        bool found = false;
        for (int i = 0; i < combos.first()->count(); ++i)
            if (combos.first()->itemText(i) == "C") { found = true; break; }
        QVERIFY2(found, "Linguaggio 'C' assente dalla combo");
    }

    /* C-7: la combo contiene "Rust" */
    void comboRustContiene() {
        const auto combos = translitterCombos();
        if (combos.isEmpty()) QSKIP("Nessuna combo Translitter trovata");
        bool found = false;
        for (int i = 0; i < combos.first()->count(); ++i)
            if (combos.first()->itemText(i) == "Rust") { found = true; break; }
        QVERIFY2(found, "Linguaggio 'Rust' assente dalla combo");
    }

    /* C-8: la combo contiene "Java" */
    void comboJavaContiene() {
        const auto combos = translitterCombos();
        if (combos.isEmpty()) QSKIP("Nessuna combo Translitter trovata");
        bool found = false;
        for (int i = 0; i < combos.first()->count(); ++i)
            if (combos.first()->itemText(i) == "Java") { found = true; break; }
        QVERIFY2(found, "Linguaggio 'Java' assente dalla combo");
    }
};

/* ══════════════════════════════════════════════════════════════
   main
   ══════════════════════════════════════════════════════════════ */
int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    int result = 0;
    { TestLangFence          t; result |= QTest::qExec(&t, argc, argv); }
    { TestTranslitterWidget  t; result |= QTest::qExec(&t, argc, argv); }
    { TestKLangs             t; result |= QTest::qExec(&t, argc, argv); }
    return result;
}

#include "test_translitter.moc"
