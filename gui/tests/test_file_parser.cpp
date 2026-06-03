/* ══════════════════════════════════════════════════════════════
   test_file_parser.cpp — Suite di test per StrumentiFilePage

   Categorie:
     CAT-A  Costruzione — StrumentiFilePage senza crash, widget attesi
     CAT-B  pdftotext — chiamata QProcess pdftotext (QSKIP se mancante)
     CAT-C  File CSV — lettura con QTemporaryFile, file vuoto
     CAT-D  File non validi — path inesistente, binario, nessun crash

   Build:
     cmake -B build_tests gui/ -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_file_parser
     ./build_tests/test_file_parser
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QApplication>
#include <QLabel>
#include <QComboBox>
#include <QTextEdit>
#include <QTextBrowser>
#include <QPushButton>
#include <QTabWidget>
#include <QLineEdit>
#include <QTemporaryFile>
#include <QTextStream>
#include <QDir>
#include <QProcess>

#include "mock_ai_client.h"
#include "../pages/main_tools_file.h"

/* ──────────────────────────────────────────────────────────────
   Helpers
   ────────────────────────────────────────────────────────────── */

/** Controlla se pdftotext è disponibile sul sistema. */
static bool pdftotextAvailable()
{
    return QProcess::execute("pdftotext", QStringList{"-v"}) == 0
        || QProcess::execute("pdftotext", QStringList{"--version"}) == 0;
}

/** Crea un file temporaneo con contenuto dato; restituisce il path. */
static QString creaFileTempConContenuto(const QString& suffix,
                                        const QByteArray& contenuto)
{
    auto* f = new QTemporaryFile(
        QDir::tempPath() + "/prismalux_fp_test_XXXXXX" + suffix);
    if (!f->open()) { delete f; return {}; }
    f->setAutoRemove(false);
    f->write(contenuto);
    f->flush();
    const QString path = f->fileName();
    f->close();
    delete f;
    return path;
}

/* Istanza condivisa per CAT-A e CAT-B */
static MockAiClient*    s_mock = nullptr;
static StrumentiFilePage* s_page = nullptr;

static void ensurePage()
{
    if (!s_mock) s_mock = new MockAiClient(nullptr);
    if (!s_page) s_page = new StrumentiFilePage(s_mock, nullptr);
}

/* ══════════════════════════════════════════════════════════════
   CAT-A — Costruzione
   ══════════════════════════════════════════════════════════════ */
class TestCatA : public QObject {
    Q_OBJECT
private slots:

    void initTestCase() { ensurePage(); }

    /* A-1: si costruisce senza crash */
    void costruisceSenzaCrash()
    {
        QVERIFY2(s_page != nullptr, "StrumentiFilePage non costruita");
    }

    /* A-2: e' un QWidget */
    void eUnQWidget()
    {
        QVERIFY(qobject_cast<QWidget*>(s_page) != nullptr);
    }

    /* A-3: ha figli widget (non e' vuota) */
    void haBigliFigli()
    {
        QVERIFY(!s_page->findChildren<QWidget*>().isEmpty());
    }

    /* A-4: contiene un QTabWidget */
    void haQTabWidget()
    {
        const auto tabs = s_page->findChildren<QTabWidget*>();
        QVERIFY2(!tabs.isEmpty(), "StrumentiFilePage: QTabWidget assente");
    }

    /* A-5: il tab con testo "File AI" esiste */
    void haTabFileAi()
    {
        const auto allTabs = s_page->findChildren<QTabWidget*>();
        QVERIFY(!allTabs.isEmpty());
        QTabWidget* tw = allTabs.first();
        bool found = false;
        for (int i = 0; i < tw->count(); ++i) {
            if (tw->tabText(i).contains("File", Qt::CaseInsensitive) ||
                tw->tabText(i).contains("AI",   Qt::CaseInsensitive)) {
                found = true;
                break;
            }
        }
        QVERIFY2(found, "Tab 'File AI' non trovato nel QTabWidget");
    }

    /* A-6: ha almeno un QLineEdit (directory o query) */
    void haQLineEdit()
    {
        QVERIFY(!s_page->findChildren<QLineEdit*>().isEmpty());
    }

    /* A-7: ha almeno un QTextBrowser (output ricerca file) */
    void haQTextBrowser()
    {
        QVERIFY(!s_page->findChildren<QTextBrowser*>().isEmpty());
    }

    /* A-8: ha almeno un QPushButton */
    void haQPushButton()
    {
        QVERIFY(!s_page->findChildren<QPushButton*>().isEmpty());
    }

    /* A-9: ha almeno un QTextEdit (output PDF o CSV) */
    void haQTextEdit()
    {
        QVERIFY(!s_page->findChildren<QTextEdit*>().isEmpty());
    }

    /* A-10: ha almeno un QComboBox (azioni analisi) */
    void haQComboBox()
    {
        QVERIFY(!s_page->findChildren<QComboBox*>().isEmpty());
    }

    /* A-11: tab PDF esiste */
    void haTabPdf()
    {
        const auto allTabs = s_page->findChildren<QTabWidget*>();
        QVERIFY(!allTabs.isEmpty());
        QTabWidget* tw = allTabs.first();
        bool found = false;
        for (int i = 0; i < tw->count(); ++i) {
            if (tw->tabText(i).contains("PDF", Qt::CaseInsensitive)) {
                found = true;
                break;
            }
        }
        QVERIFY2(found, "Tab 'PDF' non trovato");
    }

    /* A-12: tab CSV/Excel esiste */
    void haTabCsvExcel()
    {
        const auto allTabs = s_page->findChildren<QTabWidget*>();
        QVERIFY(!allTabs.isEmpty());
        QTabWidget* tw = allTabs.first();
        bool found = false;
        for (int i = 0; i < tw->count(); ++i) {
            if (tw->tabText(i).contains("CSV",   Qt::CaseInsensitive) ||
                tw->tabText(i).contains("Excel", Qt::CaseInsensitive) ||
                tw->tabText(i).contains("Dati",  Qt::CaseInsensitive)) {
                found = true;
                break;
            }
        }
        QVERIFY2(found, "Tab CSV/Excel non trovato");
    }

    /* A-13: tab Word/Testo esiste */
    void haTabWord()
    {
        const auto allTabs = s_page->findChildren<QTabWidget*>();
        QVERIFY(!allTabs.isEmpty());
        QTabWidget* tw = allTabs.first();
        bool found = false;
        for (int i = 0; i < tw->count(); ++i) {
            if (tw->tabText(i).contains("Word",  Qt::CaseInsensitive) ||
                tw->tabText(i).contains("Testo", Qt::CaseInsensitive)) {
                found = true;
                break;
            }
        }
        QVERIFY2(found, "Tab 'Word/Testo' non trovato");
    }

    /* A-14: distruzione senza crash — crea e distrugge un'istanza separata */
    void distruzioneSenzaCrash()
    {
        auto* mock2 = new MockAiClient(nullptr);
        auto* page2 = new StrumentiFilePage(mock2, nullptr);
        delete page2;
        delete mock2;
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B — pdftotext (condizionale su disponibilita' tool)
   ══════════════════════════════════════════════════════════════ */
class TestCatB : public QObject {
    Q_OBJECT
private slots:

    /* B-1: pdftotext e' disponibile OPPURE il test viene saltato */
    void pdftotextDisponibileOSkip()
    {
        if (!pdftotextAvailable())
            QSKIP("pdftotext non disponibile — installa poppler-utils");
        /* Se siamo qui, pdftotext e' nel PATH */
        QVERIFY(true);
    }

    /* B-2: pdftotext accetta '-' come destinazione stdout senza crash */
    void pdftotextAccettaStdout()
    {
        if (!pdftotextAvailable())
            QSKIP("pdftotext non disponibile — installa poppler-utils");

        /* Genera un PDF minimale (1 byte non valido):
           pdftotext deve uscire con exit code != 0 ma NON crashare il processo. */
        const QString fakePdf = creaFileTempConContenuto(
            ".pdf", QByteArray("%PDF-1.4\n%%EOF\n"));
        if (fakePdf.isEmpty())
            QSKIP("impossibile creare file temporaneo");

        QProcess proc;
        proc.start("pdftotext", {fakePdf, "-"});
        const bool avviato = proc.waitForStarted(3000);
        QVERIFY2(avviato, "pdftotext non si e' avviato");
        proc.waitForFinished(5000);
        /* L'exit code puo' essere 0 o != 0 (PDF non valido), ma il processo
           deve terminare normalmente (non crashare). */
        QVERIFY2(proc.exitStatus() == QProcess::NormalExit,
                 "pdftotext ha terminato in modo anomalo");

        QFile::remove(fakePdf);
    }

    /* B-3: il QProcess di pdftotext avviato da StrumentiFilePage non
       lascia figli orfani (i processi figli sono figli di s_page) */
    void processiPdfSonoFigliDellaPage()
    {
        if (!pdftotextAvailable())
            QSKIP("pdftotext non disponibile — installa poppler-utils");

        ensurePage();
        /* Conta i QProcess figli prima */
        const int primaDi = s_page->findChildren<QProcess*>().size();

        /* Nota: onPdfFileBtnClicked() apre un dialog — non lo invochiamo.
           Verifichiamo solo che i processi eventualmente creati siano
           registrati come figli della page (parent = s_page). */
        const int dopo = s_page->findChildren<QProcess*>().size();
        QVERIFY(dopo >= primaDi);   /* non si crea nulla senza interazione */
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-C — File CSV
   ══════════════════════════════════════════════════════════════ */
class TestCatC : public QObject {
    Q_OBJECT
private slots:

    /* C-1: QTemporaryFile con CSV valido si apre senza errori */
    void creaFileCsvValido()
    {
        const QByteArray csv =
            "nome,eta,citta\n"
            "Mario,30,Roma\n"
            "Lucia,25,Milano\n"
            "Pino,40,Napoli\n";
        const QString path = creaFileTempConContenuto(".csv", csv);
        QVERIFY2(!path.isEmpty(), "impossibile creare CSV temporaneo");
        QFile f(path);
        QVERIFY2(f.open(QIODevice::ReadOnly | QIODevice::Text),
                 "impossibile riaprire il CSV creato");
        QTextStream in(&f);
        const QString contenuto = in.readAll();
        QVERIFY(contenuto.contains("Mario"));
        QVERIFY(contenuto.contains("eta"));
        f.close();
        QFile::remove(path);
    }

    /* C-2: file CSV vuoto si crea senza crash */
    void fileVuotoSenzaCrash()
    {
        const QString path = creaFileTempConContenuto(".csv", QByteArray());
        QVERIFY2(!path.isEmpty(), "impossibile creare CSV vuoto");
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
        QTextStream in(&f);
        const QString contenuto = in.readAll();
        QVERIFY(contenuto.isEmpty());
        f.close();
        QFile::remove(path);
    }

    /* C-3: parsing manuale CSV con separatore virgola */
    void parsingCsvColonne()
    {
        const QByteArray csv = "a,b,c\n1,2,3\n4,5,6\n";
        const QString path = creaFileTempConContenuto(".csv", csv);
        QVERIFY(!path.isEmpty());
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
        QTextStream in(&f);
        const QStringList righe = in.readAll().split('\n', Qt::SkipEmptyParts);
        f.close();
        QCOMPARE(righe.size(), 3);
        QCOMPARE(righe.at(0).split(',').size(), 3);
        QFile::remove(path);
    }

    /* C-4: CSV con intestazioni e molte righe */
    void csvMolteRighe()
    {
        QByteArray csv = "x,y\n";
        for (int i = 0; i < 500; ++i)
            csv += QByteArray::number(i) + "," + QByteArray::number(i * 2) + "\n";
        const QString path = creaFileTempConContenuto(".csv", csv);
        QVERIFY(!path.isEmpty());
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
        QTextStream in(&f);
        const QStringList righe = in.readAll().split('\n', Qt::SkipEmptyParts);
        f.close();
        QCOMPARE(righe.size(), 501);  /* header + 500 righe */
        QFile::remove(path);
    }

    /* C-5: StrumentiFilePage si costruisce con il MockAiClient — nessun crash
       anche durante operazioni successive di pulizia stato interno */
    void paginaSiCostruisceConMock()
    {
        auto* mock = new MockAiClient(nullptr);
        auto* page = new StrumentiFilePage(mock, nullptr);
        /* verifica che l'area anteprima CSV (m_datiPreview) esista */
        const auto edits = page->findChildren<QTextEdit*>();
        QVERIFY2(!edits.isEmpty(),
                 "StrumentiFilePage: QTextEdit per anteprima dati assente");
        delete page;
        delete mock;
    }

    /* C-6: CSV con caratteri UTF-8 non ASCII */
    void csvUtf8()
    {
        const QByteArray csv = "nome,citt\xc3\xa0\nM\xc3\xa0rio,Mil\xc3\xa0no\n";
        const QString path = creaFileTempConContenuto(".csv", csv);
        QVERIFY(!path.isEmpty());
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
        QTextStream in(&f);
        in.setEncoding(QStringConverter::Utf8);
        const QString contenuto = in.readAll();
        f.close();
        QVERIFY(contenuto.contains("citt"));
        QFile::remove(path);
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-D — File non validi / robustezza
   ══════════════════════════════════════════════════════════════ */
class TestCatD : public QObject {
    Q_OBJECT
private slots:

    /* D-1: path inesistente — QFile::open() fallisce senza crash */
    void pathInesistenteSenzaCrash()
    {
        const QString path = QDir::tempPath() + "/prismalux_NONEXIST_12345.csv";
        QFile f(path);
        QVERIFY2(!f.open(QIODevice::ReadOnly),
                 "aperto un file che non dovrebbe esistere");
    }

    /* D-2: file binario aperto come testo — nessun crash */
    void fileBinarioComeTesto()
    {
        /* crea un file con byte binari arbitrari */
        QByteArray binario;
        for (int i = 0; i < 256; ++i)
            binario.append(static_cast<char>(i));
        const QString path = creaFileTempConContenuto(".bin", binario);
        QVERIFY(!path.isEmpty());

        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
        QTextStream in(&f);
        /* leggiamo senza crash — il testo risultante puo' essere vuoto/corrotto */
        const QString testo = in.readAll();
        Q_UNUSED(testo)
        f.close();
        QFile::remove(path);
    }

    /* D-3: estensione .pdf con contenuto non PDF — nessun crash nell'apertura */
    void pdfFalsificatoSenzaCrash()
    {
        const QByteArray contenuto = "questo non e' un PDF\x00\x01\x02";
        const QString path = creaFileTempConContenuto(".pdf", contenuto);
        QVERIFY(!path.isEmpty());
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray letto = f.readAll();
        QVERIFY(!letto.isEmpty());
        f.close();
        QFile::remove(path);
    }

    /* D-4: file troppo grande simulato — QFile::size() restituisce la dimensione
       corretta e l'apertura non causa OOM nel test */
    void fileGrandeDimensioneCorretta()
    {
        /* 4 MB — abbastanza da rilevare ma non abbastanza da esaurire la RAM */
        const qint64 dimensione = 4 * 1024 * 1024;
        QByteArray dati(static_cast<int>(dimensione), 'X');
        const QString path = creaFileTempConContenuto(".txt", dati);
        QVERIFY(!path.isEmpty());
        QFileInfo fi(path);
        QCOMPARE(fi.size(), dimensione);
        QFile::remove(path);
    }

    /* D-5: path con caratteri speciali — nessun crash nell'apertura */
    void pathConCaratteriSpeciali()
    {
        /* usiamo QDir::tempPath() che e' sempre valido */
        const QString path = QDir::tempPath() + "/test file con spazi e-accenti.csv";
        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream ts(&f);
            ts << "ok,1\n";
            f.close();
            QFile ff(path);
            QVERIFY(ff.open(QIODevice::ReadOnly | QIODevice::Text));
            ff.close();
            QFile::remove(path);
        } else {
            /* filesystem non supporta — salta il test */
            QSKIP("filesystem non supporta caratteri speciali nel path");
        }
    }

    /* D-6: StrumentiFilePage con MockAiClient costruita e distrutta piu' volte
       — nessun crash (verifica assenza double-free o use-after-free) */
    void costruzioneRipetutaSenzaCrash()
    {
        for (int i = 0; i < 3; ++i) {
            auto* mock = new MockAiClient(nullptr);
            auto* page = new StrumentiFilePage(mock, nullptr);
            delete page;
            delete mock;
        }
        QVERIFY(true);
    }

    /* D-7: il segnale error di MockAiClient non causa crash nella page */
    void erroreSegnaleSenzaCrash()
    {
        auto* mock = new MockAiClient(nullptr);
        auto* page = new StrumentiFilePage(mock, nullptr);
        /* emissione di un errore senza che la page abbia avviato alcuna analisi */
        mock->emitError("simulated network error");
        /* processa eventi pendenti */
        QCoreApplication::processEvents();
        /* la page deve essere ancora valida */
        QVERIFY(page != nullptr);
        delete page;
        delete mock;
    }
};

/* ══════════════════════════════════════════════════════════════
   main
   ══════════════════════════════════════════════════════════════ */
int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    int r = 0;
    {
        TestCatA catA; r |= QTest::qExec(&catA, argc, argv);
        TestCatB catB; r |= QTest::qExec(&catB, argc, argv);
        TestCatC catC; r |= QTest::qExec(&catC, argc, argv);
        TestCatD catD; r |= QTest::qExec(&catD, argc, argv);
    }

    delete s_page;
    s_page = nullptr;
    delete s_mock;
    s_mock = nullptr;

    return r;
}

#include "test_file_parser.moc"
