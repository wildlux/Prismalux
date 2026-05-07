/* ══════════════════════════════════════════════════════════════
   test_manutenzione_cron.cpp — Unit test per logica Cron e Config
   ──────────────────────────────────────────────────────────────
   Categorie:
     CAT-A  cronShouldRun() — tutte le modalità schedule (12 test)
     CAT-B  cronNextRun() — formato output human-readable (8 test)
     CAT-C  detectConfigFmt() — static, nessuna UI (3 test)

   Strategia: istanzia ManutenzioneePage una sola volta via
   friend struct CronAccess. Le funzioni cron non usano member
   variables (solo QDateTime::currentDateTime() + parametri).

   Build:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_manutenzione_cron
     ./build_tests/test_manutenzione_cron
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QDateTime>
#include <QFile>
#include <QDir>
#include "../pages/manutenzione_page.h"
#include "../hardware_monitor.h"
#include "mock_ai_client.h"

/* ──────────────────────────────────────────────────────────────
   Friend struct proxy — bypassa i private senza alterare la ABI
   ────────────────────────────────────────────────────────────── */
struct CronAccess {
    static bool cronShouldRun(ManutenzioneePage* p,
                              const QString& sched, const QString& lastRun) {
        return p->cronShouldRun(sched, lastRun);
    }
    static QString cronNextRun(ManutenzioneePage* p, const QString& sched) {
        return p->cronNextRun(sched);
    }
};

struct ManutAccess {
    static QString detectConfigFmt() {
        return ManutenzioneePage::detectConfigFmt();
    }
};

/* Helper globale — singola istanza per tutti i test */
static QApplication*      s_app  = nullptr;
static MockAiClient*       s_ai   = nullptr;
static HardwareMonitor*    s_hw   = nullptr;
static ManutenzioneePage*  s_page = nullptr;

static void ensurePage() {
    if (!s_page) {
        s_ai   = new MockAiClient(nullptr);
        s_hw   = new HardwareMonitor(nullptr);
        s_page = new ManutenzioneePage(s_ai, s_hw);
    }
}

/* ══════════════════════════════════════════════════════════════
   CAT-A — cronShouldRun() (12 test)
   ══════════════════════════════════════════════════════════════ */
class TestCronShouldRun : public QObject {
    Q_OBJECT
private slots:

    void initTestCase() { ensurePage(); }

    /* ── interval@ ─────────────────────────────────────────── */

    void intervalElaspsedSufficiente() {
        /* lastRun = 25 ore fa → interval@60 → deve eseguire */
        const QString lr = QDateTime::currentDateTime()
                               .addSecs(-25 * 3600).toString(Qt::ISODate);
        QVERIFY(CronAccess::cronShouldRun(s_page, "interval@60", lr));
    }

    void intervalElapsedInsufficiente() {
        /* lastRun = 30 secondi fa → interval@60 (min) → non eseguire */
        const QString lr = QDateTime::currentDateTime()
                               .addSecs(-30).toString(Qt::ISODate);
        QVERIFY(!CronAccess::cronShouldRun(s_page, "interval@60", lr));
    }

    void intervalLastRunVuoto() {
        /* Nessun lastRun → deve sempre eseguire */
        QVERIFY(CronAccess::cronShouldRun(s_page, "interval@30", ""));
    }

    void intervalMinsZero() {
        /* mins=0 → intervallo non valido → false */
        QVERIFY(!CronAccess::cronShouldRun(s_page, "interval@0", ""));
    }

    void intervalMinsNegativo() {
        QVERIFY(!CronAccess::cronShouldRun(s_page, "interval@-5", ""));
    }

    /* ── once@ ──────────────────────────────────────────────── */

    void oncePassatoSenzaLastRun() {
        /* Data passata, nessun lastRun → deve eseguire */
        const QString past = QDateTime::currentDateTime()
                                 .addDays(-30).toString(Qt::ISODate);
        QVERIFY(CronAccess::cronShouldRun(s_page, "once@" + past, ""));
    }

    void oncePassatoConLastRun() {
        /* Data passata, già eseguito → false */
        const QString past = QDateTime::currentDateTime()
                                 .addDays(-30).toString(Qt::ISODate);
        const QString lr   = QDateTime::currentDateTime()
                                 .addDays(-29).toString(Qt::ISODate);
        QVERIFY(!CronAccess::cronShouldRun(s_page, "once@" + past, lr));
    }

    void onceFuturo() {
        /* Data futura → false */
        const QString fut = QDateTime::currentDateTime()
                                .addDays(30).toString(Qt::ISODate);
        QVERIFY(!CronAccess::cronShouldRun(s_page, "once@" + fut, ""));
    }

    /* ── daily@ ─────────────────────────────────────────────── */

    void dailyOraPassataLastRunAnno() {
        /* daily@00:01 — sicuramente passata dopo mezzanotte;
         * lastRun = 1 anno fa → deve eseguire */
        const QString lr = QDateTime::currentDateTime()
                               .addDays(-365).toString(Qt::ISODate);
        QVERIFY(CronAccess::cronShouldRun(s_page, "daily@00:01", lr));
    }

    void dailyLastRunVuoto() {
        /* Nessun lastRun, orario 00:01 già passato → true */
        QVERIFY(CronAccess::cronShouldRun(s_page, "daily@00:01", ""));
    }

    /* ── edge case ──────────────────────────────────────────── */

    void scheduleVuoto() {
        QVERIFY(!CronAccess::cronShouldRun(s_page, "", ""));
    }

    void scheduleInvalido() {
        QVERIFY(!CronAccess::cronShouldRun(s_page, "TIPO_IGNOTO@123", ""));
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B — cronNextRun() (8 test)
   ══════════════════════════════════════════════════════════════ */
class TestCronNextRun : public QObject {
    Q_OBJECT
private slots:

    void initTestCase() { ensurePage(); }

    void intervalRestituisceDescrizioneLeggibile() {
        const QString r = CronAccess::cronNextRun(s_page, "interval@30");
        QCOMPARE(r, QString("ogni 30 min"));
    }

    void intervalDiversiValori() {
        QCOMPARE(CronAccess::cronNextRun(s_page, "interval@5"),   QString("ogni 5 min"));
        QCOMPARE(CronAccess::cronNextRun(s_page, "interval@120"), QString("ogni 120 min"));
    }

    void dailyFormatoData() {
        const QString r = CronAccess::cronNextRun(s_page, "daily@10:30");
        /* Formato atteso: "DD/MM  HH:mm" */
        QVERIFY2(r.contains("10:30"),
                 qPrintable("cronNextRun daily deve contenere l'ora: " + r));
        QVERIFY2(r.contains("/"),
                 qPrintable("cronNextRun daily deve contenere '/' per la data: " + r));
    }

    void hourlyFormatoOra() {
        const QString r = CronAccess::cronNextRun(s_page, "hourly@15");
        /* Formato atteso: "HH:mm" */
        QVERIFY2(r.contains(":"),
                 qPrintable("cronNextRun hourly deve contenere ':' nel formato ora: " + r));
        QVERIFY2(!r.isEmpty(), "cronNextRun hourly non deve essere vuoto");
    }

    void onceFormatoData() {
        const QDateTime dt = QDateTime::fromString("2030-06-15T14:30:00", Qt::ISODate);
        const QString r = CronAccess::cronNextRun(s_page, "once@2030-06-15T14:30:00");
        QVERIFY2(r.contains("15/06") || r.contains("14:30"),
                 qPrintable("cronNextRun once deve riportare la data: " + r));
    }

    void scheduleInvalidoRestituisceTrattino() {
        QCOMPARE(CronAccess::cronNextRun(s_page, "INVALIDO@xyz"), QString("-"));
    }

    void scheduleVuotoRestituisceTrattino() {
        QCOMPARE(CronAccess::cronNextRun(s_page, ""), QString("-"));
    }

    void nextRunNonVuoto() {
        /* Tutte le modalità valide restituiscono qualcosa di non vuoto */
        const QString lr = QDateTime::currentDateTime().addDays(-1).toString(Qt::ISODate);
        Q_UNUSED(lr);
        QVERIFY(!CronAccess::cronNextRun(s_page, "daily@08:00").isEmpty());
        QVERIFY(!CronAccess::cronNextRun(s_page, "hourly@0").isEmpty());
        QVERIFY(!CronAccess::cronNextRun(s_page, "interval@60").isEmpty());
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-C — detectConfigFmt() (3 test)
   Nota: usa file in ~/ — i test creano/rimuovono i file in modo
   sicuro preservando eventuali file già esistenti.
   ══════════════════════════════════════════════════════════════ */
class TestDetectConfigFmt : public QObject {
    Q_OBJECT

    QString m_pathJson;
    QString m_pathToon;
    bool    m_jsonEsisteva = false;
    bool    m_toonEsisteva = false;

private slots:

    void initTestCase() {
        m_pathJson = QDir::homePath() + "/.prismalux_config.json";
        m_pathToon = QDir::homePath() + "/.prismalux_config.toon";
        m_jsonEsisteva = QFile::exists(m_pathJson);
        m_toonEsisteva = QFile::exists(m_pathToon);
    }

    void nessunFilePresenteRestituisceJson() {
        if (m_jsonEsisteva || m_toonEsisteva)
            QSKIP("file config esistenti — salto per non sovrascrivere");
        QCOMPARE(ManutAccess::detectConfigFmt(), QString("json"));
    }

    void fileToonPresenteRestituisceToon() {
        if (m_jsonEsisteva || m_toonEsisteva)
            QSKIP("file config esistenti — salto per non sovrascrivere");

        /* Crea un file .toon vuoto */
        QFile f(m_pathToon);
        if (!f.open(QIODevice::WriteOnly)) QSKIP("impossibile creare file toon");
        f.close();

        const QString r = ManutAccess::detectConfigFmt();
        QFile::remove(m_pathToon);

        QCOMPARE(r, QString("toon"));
    }

    void fileJsonNonToonRestituisceJson() {
        if (m_jsonEsisteva || m_toonEsisteva)
            QSKIP("file config esistenti — salto per non sovrascrivere");

        QFile f(m_pathJson);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) QSKIP("impossibile creare file json");
        f.write(R"({"backend":"ollama"})");
        f.close();

        const QString r = ManutAccess::detectConfigFmt();
        QFile::remove(m_pathJson);

        QCOMPARE(r, QString("json"));
    }
};

/* ══════════════════════════════════════════════════════════════
   Runner
   ══════════════════════════════════════════════════════════════ */
int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    s_app = &app;
    int status = 0;
    { TestCronShouldRun t;    status |= QTest::qExec(&t, argc, argv); }
    { TestCronNextRun t;      status |= QTest::qExec(&t, argc, argv); }
    { TestDetectConfigFmt t;  status |= QTest::qExec(&t, argc, argv); }

    delete s_page; s_page = nullptr;
    delete s_hw;   s_hw   = nullptr;
    delete s_ai;   s_ai   = nullptr;
    return status;
}

#include "test_manutenzione_cron.moc"
