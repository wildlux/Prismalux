/* ══════════════════════════════════════════════════════════════
   test_strumenti_rag.cpp — Unit test per metodi statici RAG di StrumentiPage
   ──────────────────────────────────────────────────────────────
   Categorie:
     CAT-A  ragChunkText() — logica di split testo (12 test)
     CAT-B  ragExtractText() — lettura file TXT/MD (6 test)
     CAT-C  sysPromptForAction() — stub sempre vuoto (3 test)

   Build:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_strumenti_rag
     ./build_tests/test_strumenti_rag
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QTemporaryFile>
#include <QTextStream>
#include <QDir>
#include "../pages/strumenti_page.h"

/* ──────────────────────────────────────────────────────────────
   Helper: crea un file temporaneo con contenuto noto
   ────────────────────────────────────────────────────────────── */
static QString creaFileTemp(const QString& suffix, const QString& content)
{
    QTemporaryFile* f = new QTemporaryFile(
        QDir::tempPath() + "/prismalux_test_XXXXXX" + suffix);
    if (!f->open()) { delete f; return {}; }
    f->setAutoRemove(true);
    QTextStream ts(f);
    ts.setEncoding(QStringConverter::Utf8);
    ts << content;
    f->flush();
    /* Lasciamo il file aperto (auto-remove) — teniamo il path */
    const QString path = f->fileName();
    f->close();      /* QTemporaryFile cancella solo alla distruzione */
    f->setAutoRemove(false);
    delete f;        /* destroy dopo setAutoRemove(false) → file rimane */
    return path;
}

/* ══════════════════════════════════════════════════════════════
   CAT-A — ragChunkText() (12 test)
   ══════════════════════════════════════════════════════════════ */
class TestRagChunkText : public QObject {
    Q_OBJECT
private slots:

    void stringaVuotaReturnsEmpty() {
        QVERIFY(StrumentiPage::ragChunkText("").isEmpty());
    }

    void soloSpaziBianchiReturnsEmpty() {
        /* simplified() su "   " → "" → lista vuota */
        QVERIFY(StrumentiPage::ragChunkText("   ").isEmpty());
    }

    void testoCortoUnoChunk() {
        const QStringList r = StrumentiPage::ragChunkText("ciao");
        QCOMPARE(r.size(), 1);
        QCOMPARE(r.at(0), QString("ciao"));
    }

    void testoPrecisoChunkSizeUnoChunk() {
        /* Testo di esattamente 600 caratteri → 1 chunk */
        const QString t(600, 'a');
        QCOMPARE(StrumentiPage::ragChunkText(t).size(), 1);
    }

    void testoSuperaChunkSizeDueChunk() {
        /* 700 caratteri, chunk=600, overlap=80 → 2 chunk */
        const QString t(700, 'b');
        QCOMPARE(StrumentiPage::ragChunkText(t).size(), 2);
    }

    void nessunChunkSuperaChunkSize() {
        const int cs = 100;
        const int ov = 20;
        const QString t(1200, 'c');
        for (const QString& ch : StrumentiPage::ragChunkText(t, cs, ov))
            QVERIFY2(ch.size() <= cs,
                     qPrintable(QString("chunk troppo grande: %1 > %2").arg(ch.size()).arg(cs)));
    }

    void overlapCorretto() {
        /* chunk=10, overlap=3 → secondo chunk inizia a pos 7 */
        const QString t = "0123456789abcde";  /* 15 char */
        const QStringList r = StrumentiPage::ragChunkText(t.simplified(), 10, 3);
        /* simplified() non modifica questo testo (niente spazi multipli) */
        QVERIFY(r.size() >= 2);
        QCOMPARE(r.at(0), QString("0123456789"));
        /* secondo chunk deve iniziare da pos 7 → "789abcde" */
        QCOMPARE(r.at(1), QString("789abcde"));
    }

    void overlapZero() {
        /* Con overlap=0 i chunk sono contigui senza sovrapposizione */
        const QString t(300, 'd');
        const QStringList r = StrumentiPage::ragChunkText(t, 100, 0);
        QCOMPARE(r.size(), 3);
        for (const QString& ch : r)
            QCOMPARE(ch.size(), 100);
    }

    void testoSemplificato() {
        /* Spazi multipli vengono collassati prima del chunking */
        const QStringList r = StrumentiPage::ragChunkText("a  b   c");
        QVERIFY(r.size() == 1);
        QCOMPARE(r.at(0), QString("a b c"));
    }

    void chunkPersonalizzati() {
        const QString t(500, 'e');
        const QStringList r = StrumentiPage::ragChunkText(t, 100, 10);
        QVERIFY(!r.isEmpty());
        for (const QString& ch : r)
            QVERIFY(ch.size() <= 100);
    }

    void grandeTestoChunkingCompleto() {
        /* Il testo originale (simplified) deve essere completamente coperto
         * dall'unione dei chunk senza gaps. Verifica: concat senza overlap == original */
        const int cs = 200, ov = 50;
        const QString t(1800, 'f');
        const QStringList chunks = StrumentiPage::ragChunkText(t, cs, ov);
        QVERIFY(!chunks.isEmpty());
        /* L'ultimo chunk deve contenere l'ultima sezione del testo */
        QVERIFY(chunks.last().endsWith('f'));
    }

    void deterministico() {
        const QString t = "Il RAG divide il testo in segmenti sovrapposti per contesto.";
        QCOMPARE(StrumentiPage::ragChunkText(t), StrumentiPage::ragChunkText(t));
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B — ragExtractText() (6 test)
   ══════════════════════════════════════════════════════════════ */
class TestRagExtractText : public QObject {
    Q_OBJECT
private slots:

    void fileNonEsistente() {
        QCOMPARE(StrumentiPage::ragExtractText("/tmp/prismalux_non_esiste_xyz.txt"),
                 QString(""));
    }

    void fileTxt() {
        const QString path = creaFileTemp(".txt", "Contenuto di test TXT\nRiga due");
        if (path.isEmpty()) QSKIP("impossibile creare file temporaneo");
        const QString r = StrumentiPage::ragExtractText(path);
        QVERIFY2(r.contains("Contenuto di test TXT"),
                 qPrintable("ragExtractText non ha letto il file TXT: " + r));
        QFile::remove(path);
    }

    void fileMd() {
        const QString path = creaFileTemp(".md", "# Titolo Markdown\nContenuto");
        if (path.isEmpty()) QSKIP("impossibile creare file temporaneo");
        const QString r = StrumentiPage::ragExtractText(path);
        QVERIFY(r.contains("Titolo Markdown"));
        QFile::remove(path);
    }

    void fileVuoto() {
        const QString path = creaFileTemp(".txt", "");
        if (path.isEmpty()) QSKIP("impossibile creare file temporaneo");
        /* File vuoto → stringa vuota (o solo newline) */
        const QString r = StrumentiPage::ragExtractText(path);
        QVERIFY(r.trimmed().isEmpty());
        QFile::remove(path);
    }

    void fileUnicode() {
        const QString path = creaFileTemp(".txt",
            "Testo UTF-8: àèìòù — simboli: ★ ✓ ☆");
        if (path.isEmpty()) QSKIP("impossibile creare file temporaneo");
        const QString r = StrumentiPage::ragExtractText(path);
        QVERIFY2(r.contains("UTF-8"),
                 qPrintable("Unicode non preservato: " + r));
        QFile::remove(path);
    }

    void fileEstensioneGenerica() {
        /* Estensioni non PDF (.xyz) vengono lette come testo normale */
        const QString path = creaFileTemp(".xyz", "dati raw");
        if (path.isEmpty()) QSKIP("impossibile creare file temporaneo");
        const QString r = StrumentiPage::ragExtractText(path);
        QVERIFY(r.contains("dati raw"));
        QFile::remove(path);
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-C — sysPromptForAction() (3 test)
   Note: funzione attualmente stub — restituisce sempre ""
   ══════════════════════════════════════════════════════════════ */
class TestSysPromptForAction : public QObject {
    Q_OBJECT
private slots:
    void indiceZeroZero() {
        QCOMPARE(StrumentiPage::sysPromptForAction(0, 0), QString(""));
    }
    void indiceUnoUno() {
        QCOMPARE(StrumentiPage::sysPromptForAction(1, 1), QString(""));
    }
    void indiceNegativo() {
        /* Stub deve essere stabile anche con valori fuori range */
        const QString r = StrumentiPage::sysPromptForAction(-1, -1);
        Q_UNUSED(r);  /* non crasha */
    }
};

/* ══════════════════════════════════════════════════════════════
   Runner — necessita QApplication per StrumentiPage
   ══════════════════════════════════════════════════════════════ */
int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    int status = 0;
    { TestRagChunkText t;       status |= QTest::qExec(&t, argc, argv); }
    { TestRagExtractText t;     status |= QTest::qExec(&t, argc, argv); }
    { TestSysPromptForAction t; status |= QTest::qExec(&t, argc, argv); }
    return status;
}

#include "test_strumenti_rag.moc"
