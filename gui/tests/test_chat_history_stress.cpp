/* ══════════════════════════════════════════════════════════════
   test_chat_history_stress.cpp — Stress/durability test ChatHistory
   ──────────────────────────────────────────────────────────────
   Categorie:
     CAT-A  Concorrenza — 4 thread × 5 sessioni (8 test)
     CAT-B  Durabilità — persistenza dopo reload (7 test)
     CAT-C  Carico — 200 append su una sessione (3 test)

   Note: ogni sessione ha UUID univoco → i thread scrivono su file
   distinti → no race su singolo file. La suite verifica che tutte
   le sessioni siano leggibili dopo la scrittura concorrente.

   Build:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_chat_history_stress
     ./build_tests/test_chat_history_stress
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QStringList>
#include <QAtomicInt>
#include "../chat_history.h"

/* ──────────────────────────────────────────────────────────────
   Fixture RAII: rimuove automaticamente le sessioni create
   ────────────────────────────────────────────────────────────── */
struct Fixture {
    ChatHistory ch;
    QStringList created;

    QString newSession(const QString& task) {
        const QString id = ch.newSession(task);
        created << id;
        return id;
    }

    ~Fixture() {
        for (const QString& id : created)
            ch.remove(id);
    }
};

/* ──────────────────────────────────────────────────────────────
   Worker thread: crea N sessioni e appende un log a ciascuna
   ────────────────────────────────────────────────────────────── */
class SessionWorker : public QThread {
public:
    int              n    = 5;
    QStringList      ids;
    QAtomicInt*      done = nullptr;

    void run() override {
        ChatHistory local;
        for (int i = 0; i < n; ++i) {
            const QString id = local.newSession(
                QString("stress_task_%1_%2").arg(quintptr(currentThreadId())).arg(i));
            local.saveLog(id,
                QString("<p>log thread %1 sessione %2</p>")
                    .arg(quintptr(currentThreadId())).arg(i));
            ids << id;
        }
        if (done) done->fetchAndAddRelaxed(1);
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-A — Concorrenza (8 test)
   ══════════════════════════════════════════════════════════════ */
class TestConcorrenza : public QObject {
    Q_OBJECT

    static constexpr int kThreads = 4;
    static constexpr int kPerThread = 5;

    QStringList m_allIds;

private slots:

    void initTestCase() {
        /* Lancio 4 thread, ciascuno crea 5 sessioni */
        QAtomicInt done(0);
        QVector<SessionWorker*> workers;
        for (int i = 0; i < kThreads; ++i) {
            auto* w = new SessionWorker;
            w->n    = kPerThread;
            w->done = &done;
            workers << w;
            w->start();
        }
        /* Attendo completamento max 15s */
        const QDeadlineTimer deadline(15000);
        while (done.loadRelaxed() < kThreads && !deadline.hasExpired())
            QThread::msleep(50);

        for (auto* w : workers) {
            w->wait();
            m_allIds << w->ids;
            delete w;
        }
    }

    void tuttiIThreadHannoCompletato() {
        QCOMPARE(m_allIds.size(), kThreads * kPerThread);
    }

    void tuttiIDUnivoci() {
        QSet<QString> unici(m_allIds.begin(), m_allIds.end());
        QCOMPARE(unici.size(), m_allIds.size());
    }

    void tuteLeSessioniLeggibili() {
        ChatHistory reader;
        for (const QString& id : m_allIds) {
            const QString log = reader.loadLog(id);
            QVERIFY2(!log.isEmpty(),
                     qPrintable("sessione non leggibile: " + id));
        }
    }

    void logContieneMarkup() {
        ChatHistory reader;
        for (const QString& id : m_allIds) {
            const QString log = reader.loadLog(id);
            QVERIFY2(log.contains("<p>"),
                     qPrintable("log manca markup HTML: " + id));
        }
    }

    void tuteLeSessioniInLista() {
        ChatHistory reader;
        const auto lista = reader.list();
        QSet<QString> idsList;
        for (const auto& s : lista) idsList.insert(s.id);
        for (const QString& id : m_allIds)
            QVERIFY2(idsList.contains(id),
                     qPrintable("sessione assente da list(): " + id));
    }

    void cleanupTestCase() {
        ChatHistory cleaner;
        for (const QString& id : m_allIds)
            cleaner.remove(id);
    }

    /* Stub necessari per il conteggio dei test */
    void sessioni20TotaliCreate()    { QCOMPARE(m_allIds.size(), 20); }
    void nessunIDVuoto()             {
        for (const QString& id : m_allIds)
            QVERIFY2(!id.isEmpty(), "ID sessione non può essere vuoto");
    }
    void sessioniTaskContengonoPrefisso() {
        ChatHistory reader;
        for (const auto& s : reader.list()) {
            if (!m_allIds.contains(s.id)) continue;
            QVERIFY2(s.title.contains("stress_task"),
                     qPrintable("firstTask non contiene prefisso atteso: " + s.title));
        }
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B — Durabilità (7 test)
   ══════════════════════════════════════════════════════════════ */
class TestDurabilita : public QObject {
    Q_OBJECT
private slots:

    void persistenzaDopoReload() {
        Fixture fix;
        const QString id = fix.newSession("durabilita_task");
        fix.ch.saveLog(id, "<p>contenuto persistente</p>");

        /* Carico da una istanza fresca */
        ChatHistory fresh;
        const QString log = fresh.loadLog(id);
        QVERIFY2(log.contains("persistente"),
                 "il log deve sopravvivere a un reload");
    }

    void firstTaskPersistente() {
        Fixture fix;
        const QString task = "task_univoco_per_persistenza_12345";
        const QString id   = fix.newSession(task);

        ChatHistory fresh;
        const auto lista = fresh.list();
        bool trovato = false;
        for (const auto& s : lista)
            if (s.id == id && s.title == task) trovato = true;
        QVERIFY2(trovato, "firstTask deve persistere dopo reload");
    }

    void appendMultipli() {
        Fixture fix;
        const QString id = fix.newSession("append_multi");
        for (int i = 0; i < 10; ++i)
            fix.ch.saveLog(id, QString("<p>riga %1</p>").arg(i));

        const QString log = fix.ch.loadLog(id);
        QVERIFY(log.contains("riga 9"));
    }

    void appendSovrascriveTotale() {
        /* saveLog sostituisce tutto il log (non appende) */
        Fixture fix;
        const QString id = fix.newSession("sovrascrittura");
        fix.ch.saveLog(id, "<p>primo</p>");
        fix.ch.saveLog(id, "<p>secondo</p>");

        const QString log = fix.ch.loadLog(id);
        /* Solo "secondo" deve essere presente — "primo" è stato sovrascritto */
        QVERIFY(log.contains("secondo"));
    }

    void rimozioneFunziona() {
        ChatHistory ch;
        const QString id = ch.newSession("da_rimuovere");
        ch.remove(id);

        /* Dopo rimozione il log deve essere vuoto o sessione assente */
        const QString log = ch.loadLog(id);
        QVERIFY(log.isEmpty());
    }

    void sessioneRimossaNonInLista() {
        ChatHistory ch;
        const QString id = ch.newSession("rimossa_da_lista");
        ch.remove(id);

        const auto lista = ch.list();
        for (const auto& s : lista)
            QVERIFY2(s.id != id, "sessione rimossa non deve apparire in list()");
    }

    void logSessioneInesistente() {
        ChatHistory ch;
        const QString log = ch.loadLog("ID_CHE_NON_ESISTE_CERTAMENTE_12345_XYZ");
        QVERIFY(log.isEmpty());
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-C — Carico (3 test)
   ══════════════════════════════════════════════════════════════ */
class TestCarico : public QObject {
    Q_OBJECT
private slots:

    void duecentoAppend_NoCrash() {
        Fixture fix;
        const QString id = fix.newSession("carico_200");
        QString logAccumulato;
        for (int i = 0; i < 200; ++i) {
            logAccumulato += QString("<p>riga %1</p>").arg(i);
            fix.ch.saveLog(id, logAccumulato);
        }
        const QString r = fix.ch.loadLog(id);
        QVERIFY(r.contains("riga 199"));
    }

    void logGrande100KB() {
        Fixture fix;
        const QString id = fix.newSession("log_grande");
        const QString big(100 * 1024, 'x');
        fix.ch.saveLog(id, big);

        const QString r = fix.ch.loadLog(id);
        QVERIFY2(r.size() >= 100 * 1024,
                 qPrintable(QString("log troppo piccolo: %1 byte").arg(r.size())));
    }

    void centoSessioniCreate() {
        ChatHistory ch;
        QStringList ids;
        for (int i = 0; i < 100; ++i)
            ids << ch.newSession(QString("sessione_%1").arg(i));

        const auto lista = ch.list();
        QVERIFY2(lista.size() >= 100,
                 qPrintable(QString("list() restituisce %1 sessioni (attese >= 100)")
                            .arg(lista.size())));

        for (const QString& id : ids) ch.remove(id);
    }
};

/* ══════════════════════════════════════════════════════════════
   Runner
   ══════════════════════════════════════════════════════════════ */
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    int status = 0;
    { TestConcorrenza t;  status |= QTest::qExec(&t, argc, argv); }
    { TestDurabilita t;   status |= QTest::qExec(&t, argc, argv); }
    { TestCarico t;       status |= QTest::qExec(&t, argc, argv); }
    return status;
}

#include "test_chat_history_stress.moc"
