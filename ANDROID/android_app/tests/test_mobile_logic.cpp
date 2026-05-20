/* test_mobile_logic.cpp — Qt Test unit per la logica di PrismaluxMobile
 *
 * Compila e lancia:
 *   cd ANDROID/android_app/tests
 *   mkdir build && cd build
 *   cmake .. && cmake --build . && ./test_mobile_logic
 *
 * Non richiede Android: testa la logica pura (niente UI, niente NDK).
 */
#include <QtTest/QtTest>
#include <QStringList>
#include <QFile>
#include <QTemporaryFile>
#include <QDir>
#include <QSettings>

/* ──────────────────────────────────────────────────────────────────
   Logica file attachment (estratta da chat_page.cpp) — testata
   senza istanziare ChatPage (che richiede QApplication + UI)
   ────────────────────────────────────────────────────────────────── */
struct AttachState {
    QStringList contents;
    QStringList names;
    static constexpr int kMax = 5;
    static constexpr qint64 kMaxBytes = 64 * 1024;

    bool attach(const QString& path) {
        if (contents.size() >= kMax) return false;
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
        QByteArray raw = f.read(kMaxBytes);
        if (raw.isEmpty()) return false;
        contents << QString::fromUtf8(raw);
        names    << QFileInfo(path).fileName();
        return true;
    }

    void clear() { contents.clear(); names.clear(); }

    QString buildContext() const {
        if (contents.isEmpty()) return {};
        QString s = "\n\nFILE ALLEGATI DALL'UTENTE:";
        for (int i = 0; i < contents.size(); ++i)
            s += QString("\n\n--- File %1: %2 ---\n%3")
                 .arg(i+1).arg(names[i]).arg(contents[i]);
        return s;
    }
};

/* ──────────────────────────────────────────────────────────────────
   Logica backend switching — replica semplificata
   ────────────────────────────────────────────────────────────────── */
struct BackendState {
    enum Mode { Server, Cloud, Local };
    Mode mode = Server;
    QString host = "192.168.1.165";
    int port = 11434;
    QString model = "llama3.2:1b";
    QString cloudHost, cloudModel;

    void switchToServer(const QString& h, int p, const QString& m) {
        mode = Server; host = h; port = p; model = m;
    }
    void switchToCloud(const QString& h, const QString& m) {
        mode = Cloud; cloudHost = h; cloudModel = m;
    }
    void switchToLocal() { mode = Local; }
};

/* ──────────────────────────────────────────────────────────────────
   Test suite
   ────────────────────────────────────────────────────────────────── */
class TestMobileLogic : public QObject {
    Q_OBJECT

private slots:
    /* ── File attachment ── */

    void attachSingleFile() {
        QTemporaryFile f;
        QVERIFY(f.open());
        f.write("Contenuto di test per Prismalux.\n");
        f.flush();

        AttachState s;
        QVERIFY(s.attach(f.fileName()));
        QCOMPARE(s.contents.size(), 1);
        QVERIFY(s.contents[0].contains("Contenuto di test"));
        QVERIFY(!s.names[0].isEmpty());
    }

    void attachMaxFiveFiles() {
        AttachState s;
        for (int i = 0; i < AttachState::kMax; ++i) {
            QTemporaryFile f;
            QVERIFY(f.open());
            f.write(QByteArray("file") + QByteArray::number(i));
            f.flush();
            QVERIFY(s.attach(f.fileName()));
        }
        QCOMPARE(s.contents.size(), AttachState::kMax);

        /* Il sesto file deve essere rifiutato */
        QTemporaryFile extra;
        QVERIFY(extra.open());
        extra.write("extra");
        extra.flush();
        QVERIFY(!s.attach(extra.fileName()));
        QCOMPARE(s.contents.size(), AttachState::kMax);
    }

    void attachTruncatesLargeFiles() {
        QTemporaryFile f;
        QVERIFY(f.open());
        /* Scrivi 70 KB > limite 64 KB */
        f.write(QByteArray(70 * 1024, 'X'));
        f.flush();

        AttachState s;
        QVERIFY(s.attach(f.fileName()));
        QCOMPARE(s.contents.size(), 1);
        /* Il contenuto deve essere ≤ 64 KB */
        QVERIFY(s.contents[0].size() <= (int)(AttachState::kMaxBytes));
    }

    void attachNonExistentFile() {
        AttachState s;
        QVERIFY(!s.attach("/non/esiste/mai.txt"));
        QCOMPARE(s.contents.size(), 0);
    }

    void clearResetsState() {
        QTemporaryFile f;
        QVERIFY(f.open());
        f.write("dati");
        f.flush();

        AttachState s;
        s.attach(f.fileName());
        QCOMPARE(s.contents.size(), 1);
        s.clear();
        QCOMPARE(s.contents.size(), 0);
        QCOMPARE(s.names.size(), 0);
    }

    void buildContextEmpty() {
        AttachState s;
        QVERIFY(s.buildContext().isEmpty());
    }

    void buildContextWithFiles() {
        QTemporaryFile f;
        QVERIFY(f.open());
        f.write("Hello world");
        f.flush();

        AttachState s;
        s.attach(f.fileName());
        const QString ctx = s.buildContext();
        QVERIFY(ctx.contains("FILE ALLEGATI"));
        QVERIFY(ctx.contains("Hello world"));
        QVERIFY(ctx.contains("File 1:"));
    }

    void buildContextMultipleFiles() {
        AttachState s;
        for (int i = 0; i < 3; ++i) {
            QTemporaryFile f;
            QVERIFY(f.open());
            f.write(("contenuto_" + QString::number(i)).toUtf8());
            f.flush();
            s.attach(f.fileName());
        }
        const QString ctx = s.buildContext();
        QVERIFY(ctx.contains("File 1:"));
        QVERIFY(ctx.contains("File 2:"));
        QVERIFY(ctx.contains("File 3:"));
        QVERIFY(!ctx.contains("File 4:"));
    }

    /* ── Backend switching ── */

    void backendDefaultIsServer() {
        BackendState b;
        QCOMPARE(b.mode, BackendState::Server);
        QCOMPARE(b.port, 11434);
    }

    void backendSwitchToCloud() {
        BackendState b;
        b.switchToCloud("api.openai.com", "gpt-3.5-turbo");
        QCOMPARE(b.mode, BackendState::Cloud);
        QCOMPARE(b.cloudHost, QString("api.openai.com"));
        QCOMPARE(b.cloudModel, QString("gpt-3.5-turbo"));
    }

    void backendSwitchCloudThenServer() {
        BackendState b;
        b.switchToCloud("api.openai.com", "gpt-4");
        QCOMPARE(b.mode, BackendState::Cloud);

        /* Torna a server locale — deve ripristinare host/porta */
        b.switchToServer("192.168.1.165", 11434, "llama3.2:1b");
        QCOMPARE(b.mode, BackendState::Server);
        QCOMPARE(b.host, QString("192.168.1.165"));
        QCOMPARE(b.port, 11434);
        QCOMPARE(b.model, QString("llama3.2:1b"));
    }

    void backendSwitchToLocal() {
        BackendState b;
        b.switchToLocal();
        QCOMPARE(b.mode, BackendState::Local);
    }

    void backendModeExclusive() {
        BackendState b;
        b.switchToCloud("host", "model");
        b.switchToLocal();
        /* Solo uno attivo */
        QCOMPARE(b.mode, BackendState::Local);
        QVERIFY(b.mode != BackendState::Cloud);
        QVERIFY(b.mode != BackendState::Server);
    }

    /* ── QSettings chiavi server ── */

    void settingsServerKeys() {
        QSettings s("PrismaluxTest", "MobileTest");
        s.setValue("server/host",  "10.0.0.1");
        s.setValue("server/port",  11500);
        s.setValue("server/model", "mistral:7b");
        s.setValue("server/token", "mytoken");

        QCOMPARE(s.value("server/host").toString(),  QString("10.0.0.1"));
        QCOMPARE(s.value("server/port").toInt(),     11500);
        QCOMPARE(s.value("server/model").toString(), QString("mistral:7b"));
        QCOMPARE(s.value("server/token").toString(), QString("mytoken"));

        s.remove("server/host");
        s.remove("server/port");
        s.remove("server/model");
        s.remove("server/token");
    }

    void settingsCloudKeys() {
        QSettings s("PrismaluxTest", "MobileTest");
        s.setValue("cloud/host",  "api.openai.com");
        s.setValue("cloud/key",   "sk-test");
        s.setValue("cloud/model", "gpt-4");
        s.setValue("cloud/port",  443);

        QCOMPARE(s.value("cloud/host").toString(),  QString("api.openai.com"));
        QCOMPARE(s.value("cloud/key").toString(),   QString("sk-test"));
        QCOMPARE(s.value("cloud/model").toString(), QString("gpt-4"));
        QCOMPARE(s.value("cloud/port").toInt(),     443);

        s.remove("cloud/host");
        s.remove("cloud/key");
        s.remove("cloud/model");
        s.remove("cloud/port");
    }

    /* ── Limiti file attachment (boundary) ── */

    void attachZeroFilesContext() {
        AttachState s;
        QVERIFY(s.buildContext().isEmpty());
        QCOMPARE(s.contents.size(), 0);
    }

    void attachExactlyFiveFiles() {
        AttachState s;
        int ok = 0;
        for (int i = 0; i < 5; ++i) {
            QTemporaryFile f;
            f.open();
            f.write("x");
            f.flush();
            if (s.attach(f.fileName())) ++ok;
        }
        QCOMPARE(ok, 5);
        QCOMPARE(s.contents.size(), 5);
    }

    void attachAfterClearAllowsNewFiles() {
        AttachState s;
        for (int i = 0; i < 5; ++i) {
            QTemporaryFile f; f.open(); f.write("x"); f.flush();
            s.attach(f.fileName());
        }
        QCOMPARE(s.contents.size(), 5);

        s.clear();
        QCOMPARE(s.contents.size(), 0);

        QTemporaryFile f2; f2.open(); f2.write("nuovo"); f2.flush();
        QVERIFY(s.attach(f2.fileName()));
        QCOMPARE(s.contents.size(), 1);
    }
};

QTEST_MAIN(TestMobileLogic)
#include "test_mobile_logic.moc"
