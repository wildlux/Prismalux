/* ══════════════════════════════════════════════════════════════
   test_programmazione_page.cpp  —  Suite di regressione ProgrammazionePage
   ──────────────────────────────────────────────────────────────
   3 categorie — 40 test totali:

   CAT-A  isIntentionalError  — 15 casi
   CAT-B  parseNumbers        — 15 casi
   CAT-C  Widget / stato UI   — 10 casi

   COME ESEGUIRE:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_programmazione_page
     ./build_tests/test_programmazione_page
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QApplication>
#include <QTabWidget>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QPushButton>

#include "mock_ai_client.h"
#include "../pages/programmazione_page.h"

/* ══════════════════════════════════════════════════════════════
   CAT-A — isIntentionalError (15 casi)
   ══════════════════════════════════════════════════════════════ */
class TestIsIntentionalError : public QObject {
    Q_OBJECT
private slots:

    void emptyBoth() {
        QCOMPARE(ProgrammazionePage::isIntentionalError("", ""), false);
    }

    void noErrorInOutput() {
        QCOMPARE(ProgrammazionePage::isIntentionalError("Traceback only", "print(1)"), false);
    }

    void syntaxErrorOutput_withRaiseSyntaxError() {
        const QString src = "raise SyntaxError('intenzionale')\n";
        const QString err = "SyntaxError: intenzionale\n";
        QCOMPARE(ProgrammazionePage::isIntentionalError(err, src), true);
    }

    void syntaxErrorOutput_noRaiseInSource() {
        QCOMPARE(ProgrammazionePage::isIntentionalError("SyntaxError: invalid syntax", "print(1/0)"), false);
    }

    void syntaxErrorOutput_raiseOtherError() {
        QCOMPARE(ProgrammazionePage::isIntentionalError("SyntaxError: invalid syntax", "raise ValueError('ops')"), false);
    }

    void syntaxError_emptySource() {
        QCOMPARE(ProgrammazionePage::isIntentionalError("SyntaxError: ops", ""), false);
    }

    void raiseSyntaxError_noOutputError() {
        /* raise SyntaxError nel sorgente ma nessun SyntaxError nell'output */
        QCOMPARE(ProgrammazionePage::isIntentionalError("", "raise SyntaxError('test')"), false);
    }

    void syntaxError_raiseWithParenthesis() {
        const QString src = "raise SyntaxError(\"formato errato\")\n";
        const QString err = "SyntaxError: formato errato\n";
        QCOMPARE(ProgrammazionePage::isIntentionalError(err, src), true);
    }

    void customClass_defined_raised_inOutput() {
        const QString src =
            "class MioErrore(Exception): pass\n"
            "raise MioErrore('boom')\n";
        QCOMPARE(ProgrammazionePage::isIntentionalError("MioErrore: boom\n", src), true);
    }

    void customClass_defined_raised_notInOutput() {
        const QString src =
            "class MioErrore(Exception): pass\n"
            "raise MioErrore('boom')\n";
        QCOMPARE(ProgrammazionePage::isIntentionalError("Traceback only\n", src), false);
    }

    void customClass_raised_notDefined() {
        QCOMPARE(ProgrammazionePage::isIntentionalError("MioErrore: boom\n", "raise MioErrore('boom')\n"), false);
    }

    void customClass_defined_notRaised() {
        const QString src =
            "class MioErrore(Exception): pass\n"
            "print('tutto ok')\n";
        QCOMPARE(ProgrammazionePage::isIntentionalError("MioErrore: qualcosa\n", src), false);
    }

    void twoClasses_secondRaisedAndInOutput() {
        const QString src =
            "class ErrA(Exception): pass\n"
            "class ErrB(Exception): pass\n"
            "raise ErrB('test')\n";
        QCOMPARE(ProgrammazionePage::isIntentionalError("ErrB: test\n", src), true);
    }

    void twoClasses_firstRaisedAndInOutput() {
        const QString src =
            "class ErrA(Exception): pass\n"
            "class ErrB(Exception): pass\n"
            "raise ErrA('test')\n";
        QCOMPARE(ProgrammazionePage::isIntentionalError("ErrA: test\n", src), true);
    }

    void customClass_noSyntaxError_stillDetected() {
        /* Pattern 2 scatta anche senza "SyntaxError" nell'output */
        const QString src =
            "class CustomBoom(Exception): pass\n"
            "raise CustomBoom('boom')\n";
        QCOMPARE(ProgrammazionePage::isIntentionalError("CustomBoom: boom\n", src), true);
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B — parseNumbers (15 casi)
   ══════════════════════════════════════════════════════════════ */
class TestParseNumbers : public QObject {
    Q_OBJECT
private slots:

    void emptyInput() {
        QVERIFY(ProgrammazionePage::parseNumbers("").isEmpty());
    }

    void oneNumber_returnsEmpty() {
        QVERIFY(ProgrammazionePage::parseNumbers("42").isEmpty());
    }

    void twoNumbers_returnsEmpty() {
        QVERIFY(ProgrammazionePage::parseNumbers("1\n2").isEmpty());
    }

    void exactlyThreeNumbers() {
        const auto v = ProgrammazionePage::parseNumbers("1\n2\n3");
        QCOMPARE(v.size(), 3);
        QCOMPARE(v[0], 1.0);
        QCOMPARE(v[1], 2.0);
        QCOMPARE(v[2], 3.0);
    }

    void tenNumericLines() {
        QString text;
        for (int i = 1; i <= 10; ++i) text += QString::number(i) + "\n";
        QCOMPARE(ProgrammazionePage::parseNumbers(text).size(), 10);
    }

    void floatNumbers() {
        const auto v = ProgrammazionePage::parseNumbers("1.5\n2.7\n3.14");
        QCOMPARE(v.size(), 3);
        QCOMPARE(v[0], 1.5);
    }

    void negativeNumbers() {
        const auto v = ProgrammazionePage::parseNumbers("-1\n-2\n-3");
        QCOMPARE(v.size(), 3);
        QCOMPARE(v[0], -1.0);
    }

    void commaSeparators() {
        const auto v = ProgrammazionePage::parseNumbers("1,2,3,4,5");
        QVERIFY(v.size() >= 3);
    }

    void semicolonSeparators() {
        const auto v = ProgrammazionePage::parseNumbers("10;20;30");
        QVERIFY(v.size() >= 3);
    }

    void skipsDollarLine() {
        /* Righe che iniziano con $ vengono ignorate (non contano come nonNumericLines) */
        const QString text = "$ python3 script.py\n1\n2\n3\n4\n5";
        const auto v = ProgrammazionePage::parseNumbers(text);
        QVERIFY(!v.isEmpty());
        QCOMPARE(v.size(), 5);
    }

    void tooMuchText_returnsEmpty() {
        /* Più righe di testo che numeri → {} */
        const QString text =
            "uno\ndue\ntre\nquattro\n"   /* 4 righe non numeriche */
            "1\n2\n3";                   /* 3 numeri — 4 > 3 → filtrato */
        QVERIFY(ProgrammazionePage::parseNumbers(text).isEmpty());
    }

    void multiColumnNumbers() {
        /* Ogni riga ha più colonne numeriche: 3 righe × 3 colonne = 9 valori */
        const auto v = ProgrammazionePage::parseNumbers("1 2 3\n4 5 6\n7 8 9");
        QCOMPARE(v.size(), 9);
    }

    void emptyLinesSkipped() {
        const auto v = ProgrammazionePage::parseNumbers("\n\n1\n\n2\n\n3\n\n");
        QCOMPARE(v.size(), 3);
    }

    void mixedLine_countsAsNonNumeric() {
        /* "ciao" → nonNumericLine++; poi 5 numeri → 1 < 5, domina i numeri */
        const QString text = "ciao\n1\n2\n3\n4\n5";
        const auto v = ProgrammazionePage::parseNumbers(text);
        QCOMPARE(v.size(), 5);
    }

    void equalTextAndNumbers_returnsEmpty() {
        /* 4 nonNumericLines vs 3 numeri → 4 > 3 → filtrato */
        const QString text =
            "alpha\nbeta\ngamma\ndelta\n"  /* 4 non numerici */
            "1\n2\n3";                     /* 3 numerici */
        QVERIFY(ProgrammazionePage::parseNumbers(text).isEmpty());
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-C — ProgrammazionePage widget / stato UI (10 casi)
   ══════════════════════════════════════════════════════════════ */
class TestProgrammazioneWidget : public QObject {
    Q_OBJECT
private:
    MockAiClient*       ai   = nullptr;
    ProgrammazionePage* page = nullptr;

private slots:

    void init() {
        ai   = new MockAiClient();
        page = new ProgrammazionePage(ai);
    }

    void cleanup() {
        delete page; page = nullptr;
        delete ai;   ai   = nullptr;
    }

    void widgetCreatedWithoutCrash() {
        QVERIFY(page != nullptr);
    }

    void innerTabsExist() {
        QVERIFY(page->findChild<QTabWidget*>("innerTabs") != nullptr);
    }

    void innerTabsAtLeastFive() {
        auto* tabs = page->findChild<QTabWidget*>("innerTabs");
        QVERIFY(tabs != nullptr);
        QVERIFY(tabs->count() >= 5);
    }

    void langComboHasFiveItems() {
        for (auto* c : page->findChildren<QComboBox*>("settingCombo")) {
            if (c->count() == 5 && c->itemData(0).toString() == "py") {
                QCOMPARE(c->count(), 5);
                return;
            }
        }
        QFAIL("Combo linguaggio non trovata con 5 item");
    }

    void langComboDefaultIsPython() {
        for (auto* c : page->findChildren<QComboBox*>("settingCombo")) {
            if (c->count() == 5 && c->itemData(0).toString() == "py") {
                QCOMPARE(c->currentData().toString(), QString("py"));
                return;
            }
        }
        QFAIL("Combo linguaggio non trovata");
    }

    void editorHasDefaultTemplate() {
        auto* ed = page->findChild<QPlainTextEdit*>("codeEditor");
        QVERIFY(ed != nullptr);
        QVERIFY(!ed->toPlainText().isEmpty());
    }

    void defaultTemplateIsFibonacci() {
        auto* ed = page->findChild<QPlainTextEdit*>("codeEditor");
        QVERIFY(ed != nullptr);
        QVERIFY(ed->toPlainText().contains("fibonacci", Qt::CaseInsensitive));
    }

    void btnRunExists() {
        bool found = false;
        for (auto* b : page->findChildren<QPushButton*>("actionBtn"))
            if (b->text().contains("Esegui")) { found = true; break; }
        QVERIFY(found);
    }

    void btnFixExists() {
        bool found = false;
        for (auto* b : page->findChildren<QPushButton*>("actionBtn"))
            if (b->text().contains("Correggi")) { found = true; break; }
        QVERIFY(found);
    }

    void btnStopDisabledInitially() {
        for (auto* b : page->findChildren<QPushButton*>("actionBtn"))
            if (b->text().contains("Stop"))
                QVERIFY(!b->isEnabled());
    }
};

/* ══════════════════════════════════════════════════════════════
   main
   ══════════════════════════════════════════════════════════════ */
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    int result = 0;
    { TestIsIntentionalError  t; result |= QTest::qExec(&t, argc, argv); }
    { TestParseNumbers        t; result |= QTest::qExec(&t, argc, argv); }
    { TestProgrammazioneWidget t; result |= QTest::qExec(&t, argc, argv); }
    return result;
}

#include "test_programmazione_page.moc"
