/* ══════════════════════════════════════════════════════════════
   test_ai_knowledge_lookup.cpp — _knowledgeLookup() (D-26)
   ──────────────────────────────────────────────────────────────
   CAT-A  Match univoco trovato (4 test)
   CAT-B  Nessun trigger di domanda → vuoto (3 test)
   CAT-C  Troppo generico (<2 keyword) → vuoto (2 test)
   CAT-D  Match ambiguo (2+ righe candidate) → vuoto (2 test)
   CAT-E  Nessun match / knowledge vuoto (3 test)
   CAT-F  Edge case parsing (righe intestazione, ']', query lunga) (4 test)

   Usa SOLO testo di conoscenza fittizio passato come parametro — mai il
   vero user_knowledge.md dell'utente (dati personali, non deterministico).

   Build:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_ai_knowledge_lookup
     ./build_tests/test_ai_knowledge_lookup
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QString>

/* _knowledgeLookup() è definita in main_ai_tools.cpp e dichiarata in
 * main_ai_p.h — header interno (commento: "Non fare mai #include
 * main_ai_p.h da file esterni"). Forward-declaration diretta della stessa
 * firma invece di includere l'header (stessa tecnica di test_ai_math.cpp
 * per normalizeItFormats, D-24). */
QString _knowledgeLookup(const QString& task, const QString& knowledge);

static const QString kSampleKnowledge =
    "## Chi sono\n"
    "Paolo, sviluppatore del progetto Prismalux.\n"
    "\n"
    "## Progetto attuale\n"
    "La telecamera WIBY ha IP 192.168.1.222, protocollo Tuya v3.3.\n"
    "Il server Ollama gira sulla porta 11434.\n"
    "\n"
    "## Procedure e algoritmi consolidati\n"
    "La procedura di deploy: usare sempre aggiorna.sh dopo ogni build.\n";

/* ══════════════════════════════════════════════════════════════
   CAT-A — Match univoco trovato (4 test)
   ══════════════════════════════════════════════════════════════ */
class TestUniqueMatch : public QObject {
    Q_OBJECT
private slots:

    void findsCameraIp() {
        const QString r = _knowledgeLookup("qual è l'IP della telecamera?", kSampleKnowledge);
        QVERIFY(r.contains("192.168.1.222"));
    }

    void findsOllamaPort() {
        const QString r = _knowledgeLookup("qual è la porta di ollama?", kSampleKnowledge);
        QVERIFY(r.contains("11434"));
    }

    void dimmiTriggerWorks() {
        const QString r = _knowledgeLookup("dimmi la porta di ollama", kSampleKnowledge);
        QVERIFY(r.contains("11434"));
    }

    void caseInsensitiveMatch() {
        const QString r = _knowledgeLookup("QUAL È L'IP DELLA TELECAMERA?", kSampleKnowledge);
        QVERIFY(r.contains("192.168.1.222"));
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B — Nessun verbo di domanda esplicito → vuoto, anche con
   keyword che altrimenti combacerebbero (3 test)
   ══════════════════════════════════════════════════════════════ */
class TestNoTrigger : public QObject {
    Q_OBJECT
private slots:

    void statementNotQuestion() {
        QVERIFY(_knowledgeLookup("la telecamera ha un ip", kSampleKnowledge).isEmpty());
    }

    void unrelatedQuestionWord() {
        QVERIFY(_knowledgeLookup("perché la telecamera ha quell'ip?", kSampleKnowledge).isEmpty());
    }

    void emptyTask() {
        QVERIFY(_knowledgeLookup("", kSampleKnowledge).isEmpty());
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-C — Query troppo generica (<2 keyword non-stopword) → vuoto (2 test)
   ══════════════════════════════════════════════════════════════ */
class TestTooGeneric : public QObject {
    Q_OBJECT
private slots:

    void singleKeywordRejected() {
        /* "qual è" + "ip" = una sola keyword ("ip") — sotto soglia */
        QVERIFY(_knowledgeLookup("qual è l'ip?", kSampleKnowledge).isEmpty());
    }

    void onlyStopwordsRejected() {
        QVERIFY(_knowledgeLookup("qual è la cosa?", kSampleKnowledge).isEmpty());
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-D — Match ambiguo: 2+ righe candidate → vuoto, mai indovinare (2 test)
   ══════════════════════════════════════════════════════════════ */
class TestAmbiguousMatch : public QObject {
    Q_OBJECT
private slots:

    void twoLinesMatchBothKeywords() {
        const QString knowledge =
            "## Progetto attuale\n"
            "Il server Ollama gira sulla porta 11434.\n"
            "Il server di backup Ollama gira sulla porta 11435.\n";
        /* entrambe le righe contengono "server" e "ollama" → ambiguo */
        QVERIFY(_knowledgeLookup("qual è il server ollama?", knowledge).isEmpty());
    }

    void genericKeywordMatchesEverySection() {
        const QString knowledge =
            "## Progetto attuale\n"
            "Il progetto principale è Prismalux.\n"
            "## Chi sono\n"
            "Progetto secondario: gestione candidature.\n";
        QVERIFY(_knowledgeLookup("qual è il progetto?", knowledge).isEmpty());
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-E — Nessun match / knowledge vuoto (3 test)
   ══════════════════════════════════════════════════════════════ */
class TestNoMatch : public QObject {
    Q_OBJECT
private slots:

    void emptyKnowledgeReturnsEmpty() {
        QVERIFY(_knowledgeLookup("qual è l'ip della telecamera?", "").isEmpty());
    }

    void noLineContainsKeywords() {
        QVERIFY(_knowledgeLookup("qual è il colore preferito?", kSampleKnowledge).isEmpty());
    }

    void whitespaceOnlyKnowledgeReturnsEmpty() {
        QVERIFY(_knowledgeLookup("qual è l'ip della telecamera?", "   \n\n  ").isEmpty());
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-F — Edge case: intestazioni escluse, ']' protetto, query lunga (4 test)
   ══════════════════════════════════════════════════════════════ */
class TestEdgeCases : public QObject {
    Q_OBJECT
private slots:

    void headingLinesExcludedFromMatch() {
        /* Il titolo "## Progetto attuale" contiene "progetto" ma è
         * un'intestazione — non deve mai essere restituito come risposta */
        const QString r = _knowledgeLookup("qual è il progetto attuale?", kSampleKnowledge);
        QVERIFY(r.isEmpty() || !r.startsWith("##"));
    }

    void closingBracketStrippedFromAnswer() {
        const QString knowledge = "## Nota\nLa telecamera [WIBY] ha IP 192.168.1.222.\n";
        const QString r = _knowledgeLookup("qual è l'ip della telecamera?", knowledge);
        QVERIFY(!r.contains(QLatin1Char(']')));
    }

    void overlyLongQueryRejected() {
        const QString longTask = "qual è l'IP della telecamera " + QString(200, QLatin1Char('a'));
        QVERIFY(_knowledgeLookup(longTask, kSampleKnowledge).isEmpty());
    }

    void deployProcedureFound() {
        const QString r = _knowledgeLookup("qual è la procedura di deploy?", kSampleKnowledge);
        QVERIFY(r.contains("aggiorna.sh"));
    }
};

int main(int argc, char** argv)
{
    int status = 0;
    { TestUniqueMatch     t; status |= QTest::qExec(&t, argc, argv); }
    { TestNoTrigger       t; status |= QTest::qExec(&t, argc, argv); }
    { TestTooGeneric      t; status |= QTest::qExec(&t, argc, argv); }
    { TestAmbiguousMatch  t; status |= QTest::qExec(&t, argc, argv); }
    { TestNoMatch         t; status |= QTest::qExec(&t, argc, argv); }
    { TestEdgeCases       t; status |= QTest::qExec(&t, argc, argv); }
    return status;
}

#include "test_ai_knowledge_lookup.moc"
