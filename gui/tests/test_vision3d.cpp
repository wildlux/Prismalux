/* ══════════════════════════════════════════════════════════════
   test_vision3d.cpp  —  Suite di regressione Vision3DWidget
   ──────────────────────────────────────────────────────────────
   3 categorie — 19 test totali:

   CAT-A  Costruzione / stato iniziale     — 11 casi (no rete, no Ollama)
   CAT-B  Lifecycle stop/distruzione       — 5 casi
   CAT-C  Server HTTPS end-to-end (curl)   — 3 casi (QSKIP senza curl/openssl)

   COME ESEGUIRE:
     cmake -B build_tests -DBUILD_TESTS=ON
     cmake --build build_tests -j$(nproc) --target test_vision3d
     ./build_tests/test_vision3d
   ══════════════════════════════════════════════════════════════ */
#include <QtTest/QtTest>
#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QTableWidget>
#include <QComboBox>
#include <QListWidget>
#include <QSpinBox>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QBuffer>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "../pages/widget_vision3d.h"
#include "../prismalux_paths.h"

namespace P = PrismaluxPaths;

/* ══════════════════════════════════════════════════════════════
   CAT-A — Costruzione / stato iniziale (7 casi)
   ══════════════════════════════════════════════════════════════ */
class TestVision3DConstruction : public QObject {
    Q_OBJECT
private slots:

    void constructsWithoutCrash() {
        Vision3DWidget w;
        QVERIFY(!w.isRunning());
    }

    void hasCoreUiElements() {
        Vision3DWidget w;
        QVERIFY(w.findChild<QPlainTextEdit*>() != nullptr);   // log
        QVERIFY(w.findChild<QTableWidget*>()   != nullptr);   // device attivi
        QVERIFY(w.findChild<QLineEdit*>()      != nullptr);   // porta
        QVERIFY(!w.findChildren<QPushButton*>().isEmpty());   // avvia/ferma
    }

    void portEditDefaultsToVision3DPort() {
        Vision3DWidget w;
        auto* edit = w.findChild<QLineEdit*>();
        QVERIFY(edit != nullptr);
        QCOMPARE(edit->text().toInt(), P::kVision3DPort);
    }

    void deviceTableHasFourColumns() {
        Vision3DWidget w;
        auto* table = w.findChild<QTableWidget*>();
        QVERIFY(table != nullptr);
        QCOMPARE(table->columnCount(), 4);
        QCOMPARE(table->rowCount(), 0);   // nessun device alla costruzione
    }

    void settersDoNotCrash() {
        Vision3DWidget w;
        w.setOutputDir("/tmp/prismalux_test_scan");
        w.setOllamaUrl("http://localhost:1");
        w.setVlmModel("moondream");
        w.setDepthScript("/nonexistent/depth.py");
        w.setPythonExe("python3");
        w.setArucoMarkerMm(55.0);
        QVERIFY(!w.isRunning());
    }

    void toggleButtonSaysAvvia() {
        Vision3DWidget w;
        bool found = false;
        for (auto* b : w.findChildren<QPushButton*>())
            if (b->text().contains("Avvia")) found = true;
        QVERIFY(found);
    }

    void logContainsReadyMessage() {
        Vision3DWidget w;
        auto* log = w.findChild<QPlainTextEdit*>();
        QVERIFY(log != nullptr);
        QVERIFY(log->toPlainText().contains("Pronto"));
    }

    void ifaceComboEndsWithLoopback() {
        // l'ultima voce è sempre 127.0.0.1, così il fallback esiste ovunque
        Vision3DWidget w;
        auto* combo = w.findChild<QComboBox*>("v3dIfaceCombo");
        QVERIFY(combo != nullptr);
        QVERIFY(combo->count() >= 1);
        QVERIFY(combo->itemText(combo->count() - 1).startsWith("127.0.0.1"));
    }

    void ifaceComboPrefersLan() {
        // se esiste un IP 192.168.x deve stare in cima (default di bind);
        // le interfacce virtuali docker/virbr/vnet non devono comparire
        Vision3DWidget w;
        auto* combo = w.findChild<QComboBox*>("v3dIfaceCombo");
        QVERIFY(combo != nullptr);
        bool hasLan = false;
        for (int i = 0; i < combo->count(); ++i) {
            const QString t = combo->itemText(i);
            QVERIFY(!t.contains("docker"));
            QVERIFY(!t.contains("virbr"));
            QVERIFY(!t.contains("vnet"));
            if (t.startsWith("192.168.")) hasLan = true;
        }
        if (hasLan)
            QVERIFY(combo->itemText(0).startsWith("192.168."));
    }

    void galleryAndVlmComboExist() {
        // galleria scatti (si accodano, non si sovrascrivono) + combo VLM
        Vision3DWidget w;
        QVERIFY(w.findChild<QListWidget*>("v3dGallery") != nullptr);
        auto* vlm = w.findChild<QComboBox*>("v3dVlmCombo");
        QVERIFY(vlm != nullptr);
        QVERIFY(vlm->isEditable());
        QVERIFY(!vlm->currentText().isEmpty());   // default da QSettings/moondream
    }

    void sceneCanvasAndReconExist() {
        // scena 3D (frustum camere) + sezione ricostruzione fotogrammetrica
        Vision3DWidget w;
        auto* scene = w.findChild<Vision3DSceneCanvas*>("v3dScene");
        QVERIFY(scene != nullptr);
        const int before = scene->shotCount();
        scene->addShot({"k1", "tel01", 1, 42, 30, true});
        scene->addShot({"k1", "tel01", 1, 42, 30, true});   // duplicato: ignorato
        QCOMPARE(scene->shotCount(), before + 1);
        scene->setSelectedKey("k1");                        // non deve crashare
        scene->clearShots();
        QCOMPARE(scene->shotCount(), 0);
        QVERIFY(w.findChild<QPushButton*>("v3dReconBtn") != nullptr);
        QVERIFY(w.findChild<QComboBox*>("v3dSessionCombo") != nullptr);
        // posa manuale: spinbox disabilitati finché nessuno scatto è selezionato
        auto* head = w.findChild<QSpinBox*>("v3dPoseHead");
        auto* pit  = w.findChild<QSpinBox*>("v3dPosePitch");
        QVERIFY(head != nullptr && pit != nullptr);
        QVERIFY(!head->isEnabled() && !pit->isEnabled());
        QVERIFY(head->wrapping());                          // 359°+1 = 0°
        QVERIFY(w.findChild<QPushButton*>("v3dDistribBtn") != nullptr);
        // guida "?" + ricontrollo COLMAP a caldo + hint di stato
        QVERIFY(w.findChild<QPushButton*>("v3dHelpBtn") != nullptr);
        QVERIFY(w.findChild<QPushButton*>("v3dRecheckBtn") != nullptr);
        auto* hint = w.findChild<QLabel*>("v3dReconHint");
        QVERIFY(hint != nullptr);
        QVERIFY(!hint->text().isEmpty());   // la sonda ha già scritto lo stato
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-B — Lifecycle stop / distruzione (5 casi)
   ══════════════════════════════════════════════════════════════ */
class TestVision3DLifecycle : public QObject {
    Q_OBJECT
private slots:

    void stopWithoutStartIsNoop() {
        Vision3DWidget w;
        w.stop();                      // server mai avviato: non deve crashare
        QVERIFY(!w.isRunning());
    }

    void doubleStopIsIdempotent() {
        Vision3DWidget w;
        w.stop();
        w.stop();
        QVERIFY(!w.isRunning());
    }

    void destructionAfterStopIsClean() {
        auto* w = new Vision3DWidget;
        w->stop();
        delete w;                      // nessun SEGV su distruzione
        QVERIFY(true);
    }

    void destructionWithoutStopIsClean() {
        auto* w = new Vision3DWidget;
        delete w;                      // il distruttore chiama stop() da solo
        QVERIFY(true);
    }

    void startOnPrivilegedPortFails() {
        // porta 1 richiede root: start() deve fallire pulito, non crashare.
        // Se openssl manca, fallisce ancora prima (sempre false, mai crash).
        Vision3DWidget w;
        const bool ok = w.start(1);
        QVERIFY(!ok);
        QVERIFY(!w.isRunning());
    }
};

/* ══════════════════════════════════════════════════════════════
   CAT-C — Server HTTPS end-to-end con curl esterno (3 casi)
   Client esterno (QProcess curl) e non QNetworkAccessManager di
   proposito: evita il bug Qt 6.10.2 readyRead perso su loopback
   data+FIN quando ANCHE il client è nell'event loop Qt.
   ══════════════════════════════════════════════════════════════ */
class TestVision3DServerE2E : public QObject {
    Q_OBJECT

    static constexpr quint16 kTestPort = 18443;

    /* Esegue curl mantenendo vivo l'event loop Qt (il server gira lì). */
    static QByteArray runCurl(const QStringList& args, bool& ok) {
        QProcess p;
        p.start("curl", QStringList{"--insecure", "--silent", "--max-time", "10"} + args);
        ok = p.waitForStarted(5000);
        if (!ok) return {};
        for (int i = 0; i < 150 && p.state() != QProcess::NotRunning; ++i)
            QTest::qWait(100);   // processa gli eventi: il server risponde qui
        ok = (p.state() == QProcess::NotRunning && p.exitCode() == 0);
        return p.readAllStandardOutput();
    }

    Vision3DWidget* w = nullptr;
    QTemporaryDir* tmp = nullptr;

private slots:
    void initTestCase() {
        if (QStandardPaths::findExecutable("curl").isEmpty())
            QSKIP("curl non disponibile");
        if (QStandardPaths::findExecutable("openssl").isEmpty())
            QSKIP("openssl non disponibile (serve per il cert self-signed)");
        tmp = new QTemporaryDir;
        QVERIFY(tmp->isValid());
        w = new Vision3DWidget;
        w->setOutputDir(tmp->path());
        w->setBindIp("127.0.0.1");   // il server ora lega SOLO l'IP scelto
        if (!w->start(kTestPort))
            QSKIP("start() fallito (porta occupata o cert non generabile)");
    }

    void getServesHtmlPage() {
        bool ok = false;
        const QByteArray html = runCurl(
            {QString("https://127.0.0.1:%1/").arg(kTestPort)}, ok);
        QVERIFY(ok);
        QVERIFY(html.contains("PRISMALUX Vision3D"));
        QVERIFY(html.contains("SCATTA E ANALIZZA"));
    }

    void unknownPathGives404() {
        bool ok = false;
        const QByteArray out = runCurl(
            {"-o", "/dev/null", "-w", "%{http_code}",
             QString("https://127.0.0.1:%1/altrove").arg(kTestPort)}, ok);
        QVERIFY(ok);
        QCOMPARE(out, QByteArray("404"));
    }

    void uploadSavesPhotoAndJson() {
        // JPEG minimo 1x1 (header JFIF valido) in base64
        QImage img(1, 1, QImage::Format_RGB32);
        img.fill(Qt::red);
        QByteArray jpeg;
        QBuffer buf(&jpeg);
        buf.open(QIODevice::WriteOnly);
        QVERIFY(img.save(&buf, "JPEG"));

        QJsonObject o;
        o["session"] = "testscan";
        o["image"]   = QString::fromUtf8("data:image/jpeg;base64," + jpeg.toBase64());
        o["wants"]   = QJsonArray{};     // nessuna analisi: solo salvataggio
        o["angle"]   = 42;

        bool ok = false;
        const QByteArray resp = runCurl(
            {"-X", "POST", "-H", "Content-Type: application/json",
             "--data-binary", QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact)),
             QString("https://127.0.0.1:%1/upload").arg(kTestPort)}, ok);
        QVERIFY(ok);

        const QJsonObject j = QJsonDocument::fromJson(resp).object();
        QVERIFY(j.value("ok").toBool());
        QCOMPARE(j.value("index").toInt(), 1);
        const QString dev = j.value("device").toString();
        QVERIFY(dev.startsWith("tel"));

        // foto + sidecar .json su disco nella sottocartella device
        const QDir d(tmp->path() + "/testscan/" + dev);
        QCOMPARE(d.entryList({"*.jpg"},  QDir::Files).size(), 1);
        QCOMPARE(d.entryList({"*.json"}, QDir::Files).size(), 1);
    }

    void cleanupTestCase() {
        if (w)   { w->stop(); delete w; w = nullptr; }
        delete tmp; tmp = nullptr;
    }
};

/* ══════════════════════════════════════════════════════════════
   main
   ══════════════════════════════════════════════════════════════ */
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    int result = 0;
    { TestVision3DConstruction t; result |= QTest::qExec(&t, argc, argv); }
    { TestVision3DLifecycle    t; result |= QTest::qExec(&t, argc, argv); }
    { TestVision3DServerE2E    t; result |= QTest::qExec(&t, argc, argv); }
    return result;
}

#include "test_vision3d.moc"
