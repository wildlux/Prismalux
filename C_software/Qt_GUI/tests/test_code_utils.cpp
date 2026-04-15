/* ══════════════════════════════════════════════════════════════
   test_code_utils.cpp — Unit test per le funzioni statiche
   AgentiPage::extractPythonCode e AgentiPage::_sanitizePyCode.

   Build:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc)
     ./build_tests/test_code_utils

   Casi coperti:
     extractPythonCode:
       A. Nessun blocco → QString vuota
       B. Blocco ```python ... ``` → codice estratto
       C. Blocco ``` ... ``` senza hint linguaggio → estratto ugualmente
       D. Blocco vuoto → QString vuota (trimmed)
       E. Blocco non chiuso → nessun match, stringa vuota
       F. Più blocchi → solo il PRIMO (comportamento attuale regex greedy)
       G. Blocco con spazio tra ``` e python → estratto

     _sanitizePyCode:
       H. Guardia __name__ malformata "name == main" → corretta
       I. Guardia "if __name__ == 'main':" (senza doppio __) → corretta
       J. Codice senza guardia → invariato
       K. Codice con numpy/pandas NON aggiunge più il preamble pip
          (bug S0b rimosso — pip è solo via C2)
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include "../pages/agenti_page.h"

class TestCodeUtils : public QObject {
    Q_OBJECT

private slots:

    /* ════════════════════════════════════════════════════════════
       extractPythonCode
       ════════════════════════════════════════════════════════════ */

    /* A. Nessun blocco → vuota */
    void extract_noBlock() {
        const QString result = AgentiPage::extractPythonCode("testo senza codice");
        QVERIFY(result.isEmpty());
    }

    /* B. Blocco ```python ... ``` → codice estratto correttamente */
    void extract_withPythonHint() {
        const QString input =
            "Ecco il codice:\n"
            "```python\n"
            "print('hello')\n"
            "```";
        const QString result = AgentiPage::extractPythonCode(input);
        QCOMPARE(result, QString("print('hello')"));
    }

    /* C. Blocco ``` senza hint → estratto ugualmente */
    void extract_noHint() {
        const QString input = "```\nx = 42\n```";
        const QString result = AgentiPage::extractPythonCode(input);
        QCOMPARE(result, QString("x = 42"));
    }

    /* D. Blocco vuoto → stringa vuota (trim di whitespace) */
    void extract_emptyBlock() {
        const QString input = "```python\n\n```";
        const QString result = AgentiPage::extractPythonCode(input);
        QVERIFY(result.isEmpty());
    }

    /* E. Fence non chiusa → nessun match */
    void extract_unclosedFence() {
        const QString input = "```python\nprint('mai chiuso')";
        const QString result = AgentiPage::extractPythonCode(input);
        QVERIFY(result.isEmpty());
    }

    /* F. Più blocchi → solo il primo */
    void extract_multipleBlocks() {
        const QString input =
            "```python\nfirst = 1\n```\n"
            "```python\nsecond = 2\n```";
        const QString result = AgentiPage::extractPythonCode(input);
        QCOMPARE(result, QString("first = 1"));
    }

    /* G. Hint linguaggio minuscolo senza spazio — caso comune */
    void extract_pyHint() {
        const QString input = "```py\nx = 1\n```";
        const QString result = AgentiPage::extractPythonCode(input);
        QCOMPARE(result, QString("x = 1"));
    }

    /* ════════════════════════════════════════════════════════════
       _sanitizePyCode
       ════════════════════════════════════════════════════════════ */

    /* H. Guardia con _name_ (un underscore ciascun lato) al posto di __name__:
       Pattern 1 cattura _?name_? == ['"]__?main__?['"] */
    void sanitize_nameGuardMissing() {
        const QString input =
            "def foo(): pass\n"
            "if name == '__main__':\n"   /* LHS: 'name' senza __ */
            "    foo()\n";
        const QString result = AgentiPage::_sanitizePyCode(input);
        QVERIFY2(result.contains("if __name__ == \"__main__\":"),
                 qPrintable("Risultato: " + result));
    }

    /* I. Guardia senza doppio __: "if __name__ == 'main':" → corretta */
    void sanitize_nameGuardNoDoubleUnderscore() {
        const QString input =
            "if __name__ == \"main\":\n"
            "    print('run')\n";
        const QString result = AgentiPage::_sanitizePyCode(input);
        QVERIFY(result.contains("if __name__ == \"__main__\":"));
    }

    /* J. Codice corretto → invariato (non modifica quello che non deve) */
    void sanitize_correctCode_unchanged() {
        const QString input =
            "import math\n"
            "x = math.sqrt(2)\n"
            "if __name__ == \"__main__\":\n"
            "    print(x)\n";
        const QString result = AgentiPage::_sanitizePyCode(input);
        /* La guardia corretta non viene toccata */
        QVERIFY(result.contains("if __name__ == \"__main__\":"));
        /* Il codice non viene allungato con preamble pip */
        QVERIFY(!result.contains("pip"));
        QVERIFY(!result.contains("_prisma_ensure"));
    }

    /* K. Codice con numpy/pandas NON inietta più pip (fix S0b) */
    void sanitize_noPipInjection() {
        const QString input =
            "import numpy as np\n"
            "import pandas as pd\n"
            "print(np.array([1,2,3]))\n";
        const QString result = AgentiPage::_sanitizePyCode(input);
        /* Dopo la rimozione dell'auto-inject, nessun pip deve apparire */
        QVERIFY2(!result.contains("pip"),
            "Il preamble pip NON deve essere iniettato — usiamo C2 (ModuleNotFoundError)");
        QVERIFY(!result.contains("subprocess"));
        QVERIFY(!result.contains("_prisma_ensure"));
        /* Il codice originale deve restare intatto */
        QVERIFY(result.contains("import numpy as np"));
    }

    /* L. Input vuoto → output vuoto */
    void sanitize_emptyInput() {
        const QString result = AgentiPage::_sanitizePyCode("");
        QVERIFY(result.isEmpty());
    }
};

QTEST_MAIN(TestCodeUtils)
#include "test_code_utils.moc"
