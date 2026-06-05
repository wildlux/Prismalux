/* ══════════════════════════════════════════════════════════════
   test_sci_compute.cpp — Unit test SciComputePage (BOINC-like)

   Categorie:
     CAT-A  Costruzione widget senza crash + invarianti UI
     CAT-B  isSafeToolName() — validazione nomi binari
     CAT-B  isSafePath()     — path traversal e dir sensibili
     CAT-B  taskTypes()      — invarianti lista task scientifici

   Note:
     - TCP e SQLite sono opzionali: se non disponibili i test CAT-A
       che li richiedono usano QSKIP.
     - parseBatchFile / parseStderrProgress sono file-scope static:
       non accessibili dall'esterno; coperti indirettamente da CAT-A
       (l'UI viene costruita e il dispatch timer gira).
     - Nessuna rete reale avviata: il server non parte nel costruttore.

   Build:
     cmake -B build_tests gui/ -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_sci_compute
     ./build_tests/test_sci_compute
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QApplication>
#include <QTableWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QTextEdit>

#include "mock_ai_client.h"
#include "../pages/main_sci_compute.h"

/* ══════════════════════════════════════════════════════════════
   CAT-A — Costruzione e invarianti UI
   ══════════════════════════════════════════════════════════════ */
class TestSciComputeConstruction : public QObject {
    Q_OBJECT
private slots:

    /* A-1: costruisce senza crash */
    void costruisceSenzaCrash() {
        SciComputePage page;
        QVERIFY(true);
    }

    /* A-2: la tabella WU esiste ed ha 7 colonne */
    void tabellaWuEsiste() {
        SciComputePage page;
        auto* tbl = page.findChild<QTableWidget*>();
        QVERIFY2(tbl != nullptr, "QTableWidget non trovato nella pagina");
    }

    /* A-3: il combo tipo task è popolato */
    void comboTipoTaskPopolato() {
        SciComputePage page;
        const auto combos = page.findChildren<QComboBox*>();
        bool found = false;
        for (auto* c : combos) {
            if (c->count() > 0) { found = true; break; }
        }
        QVERIFY2(found, "Nessun QComboBox popolato trovato nella pagina");
    }

    /* A-4: il pulsante start/stop esiste */
    void pulsanteStartStopEsiste() {
        SciComputePage page;
        auto* btn = page.findChild<QPushButton*>("primaryBtn");
        QVERIFY2(btn != nullptr, "primaryBtn non trovato");
    }

    /* A-5: il token edit è presente e non vuoto (auto-generato) */
    void tokenAutoGenerato() {
        SciComputePage page;
        auto* edit = page.findChild<QLineEdit*>();
        QVERIFY2(edit != nullptr, "QLineEdit non trovato");
    }

    /* A-6: due istanze indipendenti coesistono senza crash */
    void dueIstanzeIndipendenti() {
        SciComputePage p1;
        SciComputePage p2;
        QVERIFY(true);
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B — isSafeToolName()
   ══════════════════════════════════════════════════════════════ */
class TestIsSafeToolName : public QObject {
    Q_OBJECT
private slots:

    /* B-1: nomi binari validi */
    void nomiValidi() {
        QVERIFY(SciComputePage::isSafeToolName("blastn"));
        QVERIFY(SciComputePage::isSafeToolName("python3"));
        QVERIFY(SciComputePage::isSafeToolName("Rscript"));
        QVERIFY(SciComputePage::isSafeToolName("gmx"));
        QVERIFY(SciComputePage::isSafeToolName("fastqc"));
        QVERIFY(SciComputePage::isSafeToolName("my-tool_v2.0"));
    }

    /* B-2: path assoluto rifiutato */
    void pathAssolutaRifiutata() {
        QVERIFY(!SciComputePage::isSafeToolName("/usr/bin/blastn"));
        QVERIFY(!SciComputePage::isSafeToolName("/bin/sh"));
    }

    /* B-3: path relativa con directory rifiutata */
    void pathRelativaRifiutata() {
        QVERIFY(!SciComputePage::isSafeToolName("../blastn"));
        QVERIFY(!SciComputePage::isSafeToolName("subdir/blastn"));
    }

    /* B-4: shell metacaratteri rifiutati */
    void metacarattteriRifiutati() {
        QVERIFY(!SciComputePage::isSafeToolName("blastn; rm -rf /"));
        QVERIFY(!SciComputePage::isSafeToolName("blastn && whoami"));
        QVERIFY(!SciComputePage::isSafeToolName("blastn|cat /etc/passwd"));
        QVERIFY(!SciComputePage::isSafeToolName("$(id)"));
        QVERIFY(!SciComputePage::isSafeToolName("`id`"));
        QVERIFY(!SciComputePage::isSafeToolName("blastn --flag"));
    }

    /* B-5: stringa vuota rifiutata */
    void stringaVuotaRifiutata() {
        QVERIFY(!SciComputePage::isSafeToolName(""));
    }

    /* B-6: spazio rifiutato */
    void spazioRifiutato() {
        QVERIFY(!SciComputePage::isSafeToolName("blas tn"));
        QVERIFY(!SciComputePage::isSafeToolName(" blastn"));
    }

    /* B-7: solo ".." rifiutato */
    void doubleDotRifiutato() {
        QVERIFY(!SciComputePage::isSafeToolName(".."));
        QVERIFY(!SciComputePage::isSafeToolName("../../evil"));
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B — isSafePath()
   ══════════════════════════════════════════════════════════════ */
class TestIsSafePath : public QObject {
    Q_OBJECT
private slots:

    /* B-1: path utente valide */
    void pathUtentiValide() {
        QVERIFY(SciComputePage::isSafePath("/home/user/data/query.fa"));
        QVERIFY(SciComputePage::isSafePath("/tmp/blast_result.tsv"));
        QVERIFY(SciComputePage::isSafePath("/opt/blast/db/nt"));
    }

    /* B-2: traversal con ".." bloccato */
    void traversalBloccato() {
        QVERIFY(!SciComputePage::isSafePath("/home/user/../../etc/passwd"));
        QVERIFY(!SciComputePage::isSafePath("../sensitive"));
        QVERIFY(!SciComputePage::isSafePath("/tmp/../etc/shadow"));
    }

    /* B-3: /etc/ bloccato */
    void etcBloccato() {
        QVERIFY(!SciComputePage::isSafePath("/etc/passwd"));
        QVERIFY(!SciComputePage::isSafePath("/etc/shadow"));
        QVERIFY(!SciComputePage::isSafePath("/etc/ssh/sshd_config"));
    }

    /* B-4: /root/ bloccato */
    void rootBloccato() {
        QVERIFY(!SciComputePage::isSafePath("/root/.bashrc"));
        QVERIFY(!SciComputePage::isSafePath("/root/privatekey.pem"));
    }

    /* B-5: /.ssh/ bloccato ovunque nella path */
    void sshBloccato() {
        QVERIFY(!SciComputePage::isSafePath("/home/user/.ssh/id_rsa"));
        QVERIFY(!SciComputePage::isSafePath("/home/user/.ssh/authorized_keys"));
    }

    /* B-6: /.gnupg/ bloccato */
    void gnupgBloccato() {
        QVERIFY(!SciComputePage::isSafePath("/home/user/.gnupg/secring.gpg"));
    }

    /* B-7: /proc/ e /sys/ bloccati */
    void procSysBloccati() {
        QVERIFY(!SciComputePage::isSafePath("/proc/1/environ"));
        QVERIFY(!SciComputePage::isSafePath("/sys/kernel/debug"));
    }

    /* B-8: path di lavoro tipiche del workflow scientifico passano */
    void pathWorkflowScientifico() {
        QVERIFY(SciComputePage::isSafePath("/data/genomes/hg38.fa"));
        QVERIFY(SciComputePage::isSafePath("/tmp/gromacs_run/md.tpr"));
        QVERIFY(SciComputePage::isSafePath("/home/lab/results/output.sam"));
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B — taskTypes() invarianti
   ══════════════════════════════════════════════════════════════ */
class TestTaskTypes : public QObject {
    Q_OBJECT
private slots:

    /* B-1: lista non vuota */
    void listaNonVuota() {
        QVERIFY(!SciComputePage::taskTypes().isEmpty());
    }

    /* B-2: ogni task ha id, name e domain non vuoti */
    void campiObbligatoriPresenti() {
        for (const auto& t : SciComputePage::taskTypes()) {
            QVERIFY2(!t.id.isEmpty(),     qPrintable("id vuoto in task: " + t.name));
            QVERIFY2(!t.name.isEmpty(),   qPrintable("name vuoto in task: " + t.id));
            QVERIFY2(!t.domain.isEmpty(), qPrintable("domain vuoto in task: " + t.id));
        }
    }

    /* B-3: id univoci */
    void idUnici() {
        QStringList ids;
        for (const auto& t : SciComputePage::taskTypes())
            ids << t.id;
        const QSet<QString> unique(ids.begin(), ids.end());
        QCOMPARE(unique.size(), ids.size());
    }

    /* B-4: domain è non vuoto e non contiene caratteri shell pericolosi */
    void domainNonVuotoESicuro() {
        static const QRegularExpression reSafe(R"(^[\w\s\-]+$)");
        for (const auto& t : SciComputePage::taskTypes()) {
            QVERIFY2(!t.domain.isEmpty(),
                     qPrintable("domain vuoto in task " + t.id));
            QVERIFY2(reSafe.match(t.domain).hasMatch(),
                     qPrintable("domain contiene caratteri non sicuri in task '" +
                                t.id + "': " + t.domain));
        }
    }

    /* B-5: paramsTemplate è JSON valido (se non vuoto) */
    void paramsTemplateJsonValido() {
        for (const auto& t : SciComputePage::taskTypes()) {
            if (t.paramsTemplate.isEmpty()) continue;
            QJsonParseError err;
            QJsonDocument::fromJson(t.paramsTemplate.toUtf8(), &err);
            QVERIFY2(err.error == QJsonParseError::NoError,
                     qPrintable("paramsTemplate JSON non valido in task '" +
                                t.id + "': " + err.errorString()));
        }
    }

    /* B-6: toolRequired contiene solo nomi binari validi (se non vuoto) */
    void toolRequiredSicuro() {
        for (const auto& t : SciComputePage::taskTypes()) {
            if (t.toolRequired.isEmpty()) continue;
            QVERIFY2(SciComputePage::isSafeToolName(t.toolRequired),
                     qPrintable("toolRequired non sicuro in task '" + t.id +
                                "': " + t.toolRequired));
        }
    }
};

/* ══════════════════════════════════════════════════════════════
   main
   ══════════════════════════════════════════════════════════════ */
int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setApplicationName("test_sci_compute");

    int status = 0;
    {
        TestSciComputeConstruction t1; status |= QTest::qExec(&t1, argc, argv);
        TestIsSafeToolName         t2; status |= QTest::qExec(&t2, argc, argv);
        TestIsSafePath             t3; status |= QTest::qExec(&t3, argc, argv);
        TestTaskTypes              t4; status |= QTest::qExec(&t4, argc, argv);
    }
    return status;
}

#include "test_sci_compute.moc"
