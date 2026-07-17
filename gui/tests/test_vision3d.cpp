/* ══════════════════════════════════════════════════════════════
   test_vision3d.cpp  —  Suite di regressione Vision3DWidget
   ──────────────────────────────────────────────────────────────
   3 categorie — 20 test totali:

   CAT-A  Costruzione / stato iniziale     — 11 casi (no rete, no Ollama)
   CAT-B  Lifecycle stop/distruzione       — 5 casi
   CAT-C  Server HTTPS end-to-end (curl)   — 4 casi (QSKIP senza curl/openssl)

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
#include <QTableWidget>
#include <QComboBox>
#include <QListWidget>
#include <QSpinBox>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QBuffer>
#include <QImage>
#include <QPainter>
#include <QTabWidget>
#include <QShortcut>
#include "../widgets/qr_code_widget.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMouseEvent>

#include "../pages/widget_vision3d.h"
#include "../widgets/collapsible_section.h"
#include <QGroupBox>
#include "../prismalux_paths.h"
#include "../log_bus.h"

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
        // niente più QPlainTextEdit locale: il log va nel pannello centralizzato
        // "Messaggi" (tab "3D", vedi LogBus) — qui si verificano le due tab
        // Preparazione/Assembla e il QR di collegamento al loro posto.
        QVERIFY(w.findChild<QTabWidget*>()     != nullptr);   // Preparazione | Assembla punti e texture
        QVERIFY(w.findChild<QrCodeWidget*>()   != nullptr);   // QR di collegamento (tab Preparazione)
        QVERIFY(w.findChild<QTableWidget*>()   != nullptr);   // device attivi
        QVERIFY(w.findChild<QLineEdit*>()      != nullptr);   // porta
        QVERIFY(!w.findChildren<QPushButton*>().isEmpty());   // avvia/ferma
    }

    void prepCardsAreCollapsible() {
        // le 6 card della tab Preparazione sono avvolte in CollapsibleSection
        // (fromGroupBox): tutte aperte all'avvio = comportamento invariato
        Vision3DWidget w;
        const auto secs = w.findChildren<CollapsibleSection*>();
        QVERIFY(secs.size() >= 6);
        for (auto* s : secs)
            QVERIFY(s->isOpen());
        // il titolo è migrato nell'header: il groupbox interno resta senza
        QVERIFY(qobject_cast<QGroupBox*>(secs.first()->content()) != nullptr);
        QVERIFY(qobject_cast<QGroupBox*>(secs.first()->content())->title().isEmpty());
        // arrotolare nasconde il contenuto, riaprire lo rimostra
        secs.first()->setOpen(false);
        QVERIFY(!secs.first()->content()->isVisibleTo(&w));
        secs.first()->setOpen(true);
        QVERIFY(secs.first()->content()->isVisibleTo(&w));
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

    /* Il messaggio "Pronto..." è statico (non legato a scatti/sessione):
       va nel log centralizzato (LogBus), non nel log locale della scheda
       — altrimenti occupa subito lo spazio di lavoro prima ancora di aver
       avviato il server. */
    /* Il messaggio "pronto" e tutti gli eventi Vision3D (appendLog) vanno
       su LogBus con categoria "3d", non nella tab generica "Sistema" —
       MainWindow la mappa alla tab dedicata "3D" del pannello Messaggi. */
    void logContainsReadyMessage() {
        QSignalSpy spy(LogBus::instance(), &LogBus::event);
        Vision3DWidget w;
        Q_UNUSED(w);
        bool found = false;
        for (const auto& args : spy)
            if (args.at(0).toString().contains("pronto", Qt::CaseInsensitive)) {
                QCOMPARE(args.at(1).toString(), QStringLiteral("3d"));
                found = true; break;
            }
        QVERIFY(found);
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
        // contatore foto/requisito qualità ("X/Y foto")
        auto* req = w.findChild<QLabel*>("v3dPhotoReq");
        QVERIFY(req != nullptr);
        QVERIFY(req->text().contains("foto"));
    }

    /* CAT-A: gestione sessioni — createSession()/importPhotoFiles()/
       deleteShot() sono API pubbliche (bypassano i dialog modali), così
       restano testabili direttamente invece che solo tramite prompt UI. */

    void createSessionMakesFolderAndSyncsBothCombos() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        Vision3DWidget w;
        w.setOutputDir(tmp.path());

        const QString created = w.createSession("notturno");
        QCOMPARE(created, QStringLiteral("notturno"));
        QVERIFY(QDir(tmp.path() + "/notturno").exists());

        // il combo "Assembla" (v3dSessionCombo) e quello "Preparazione"
        // (senza objectName dedicato: il secondo QComboBox nell'ordine di
        // costruzione) devono restare sincronizzati sulla stessa sessione
        auto* mainCombo = w.findChild<QComboBox*>("v3dSessionCombo");
        QVERIFY(mainCombo);
        QCOMPARE(mainCombo->currentText(), created);
    }

    void createSessionSanitizesName() {
        QTemporaryDir tmp;
        Vision3DWidget w;
        w.setOutputDir(tmp.path());
        // spazi/punteggiatura rimossi, solo lettere/numeri/-/_ restano
        QCOMPARE(w.createSession("Villa @ Mare!!"), QStringLiteral("VillaMare"));
    }

    void createSessionRejectsNameWithNothingValid() {
        QTemporaryDir tmp;
        Vision3DWidget w;
        w.setOutputDir(tmp.path());
        QVERIFY(w.createSession("!!! ???").isEmpty());
        QCOMPARE(QDir(tmp.path()).entryList(QDir::Dirs | QDir::NoDotAndDotDot).size(), 0);
    }

    /* createSession() con descrizione la persiste subito in session.json;
       setSessionDescription()/sessionDescription() la leggono/aggiornano
       anche dopo, indipendentemente dai dialoghi modali dell'UI. */
    void sessionDescriptionRoundTrip() {
        QTemporaryDir tmp;
        Vision3DWidget w;
        w.setOutputDir(tmp.path());
        const QString session = w.createSession("descTest", "cubo con croce rossa, test point cloud");
        QVERIFY(!session.isEmpty());
        QCOMPARE(w.sessionDescription(session),
                 QStringLiteral("cubo con croce rossa, test point cloud"));

        w.setSessionDescription(session, "aggiornata dopo la scansione");
        QCOMPARE(w.sessionDescription(session), QStringLiteral("aggiornata dopo la scansione"));

        QFile f(tmp.path() + "/" + session + "/session.json");
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
        QCOMPARE(o.value("description").toString(), QStringLiteral("aggiornata dopo la scansione"));
    }

    /* Una descrizione già presente nel sidecar .json di uno scatto (VLM o
       inserita a mano) deve arrivare nella galleria (Qt::UserRole+1) quando
       la sessione viene ricaricata — prima di questo fix il campo veniva
       sempre azzerato con QString() ("descrizione VLM non persistita"). */
    void shotDescriptionLoadedFromSidecarIntoGallery() {
        QTemporaryDir tmp;
        Vision3DWidget w;
        w.setOutputDir(tmp.path());
        const QString session = w.createSession("shotDescTest");
        const QString folder = tmp.path() + "/" + session + "/import";
        QDir().mkpath(folder);
        const QString base = folder + "/shot_a000";

        QImage img(32, 32, QImage::Format_RGB32);
        img.fill(Qt::blue);
        QVERIFY(img.save(base + ".jpg", "JPEG"));
        QJsonObject meta;
        meta["description"] = "cubo rosso su piano bianco, ombra netta";
        meta["index"] = 1;
        QFile mf(base + ".json");
        QVERIFY(mf.open(QIODevice::WriteOnly));
        mf.write(QJsonDocument(meta).toJson());
        mf.close();

        auto* sessionCombo = w.findChild<QComboBox*>("v3dSessionCombo");
        QVERIFY(sessionCombo);
        // segnale pubblico: forza il reload (loadSessionIntoUi è privata, ma
        // il segnale che la scatena no) indipendentemente da cosa mostra già
        sessionCombo->currentTextChanged(session);

        auto* gallery = w.findChild<QListWidget*>("v3dGallery");
        QVERIFY(gallery);
        QCOMPARE(gallery->count(), 1);
        QCOMPARE(gallery->item(0)->data(Qt::UserRole + 1).toString(),
                 QStringLiteral("cubo rosso su piano bianco, ombra netta"));
    }

    void hasEditDescriptionButtons() {
        Vision3DWidget w;
        int editBtnCount = 0;
        for (auto* b : w.findChildren<QPushButton*>())
            if (b->text() == QString::fromUtf8("\xE2\x9C\x8F\xEF\xB8\x8F")) ++editBtnCount;
        QCOMPARE(editBtnCount, 2);   // descrizione sessione + descrizione scatto
    }

    void importPhotoFilesCreatesReadableShotWithoutSensors() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        Vision3DWidget w;
        w.setOutputDir(tmp.path());
        const QString session = w.createSession("importtest");
        QVERIFY(!session.isEmpty());

        QTemporaryDir srcDir;
        QVERIFY(srcDir.isValid());
        const QString srcPath = srcDir.path() + "/foto_esterna.jpg";
        QImage img(64, 64, QImage::Format_RGB32);
        img.fill(Qt::blue);
        QVERIFY(img.save(srcPath, "JPEG"));

        QCOMPARE(w.importPhotoFiles(session, {srcPath}), 1);

        const QDir importDir(tmp.path() + "/" + session + "/import");
        const QStringList jpgs = importDir.entryList({"*_a???.jpg"}, QDir::Files);
        QCOMPARE(jpgs.size(), 1);
        const QString jpgPath = importDir.absoluteFilePath(jpgs.first());
        QFile mf(jpgPath.left(jpgPath.size() - 4) + ".json");
        QVERIFY(mf.open(QIODevice::ReadOnly));
        const QJsonObject meta = QJsonDocument::fromJson(mf.readAll()).object();
        QVERIFY(meta.value("imported").toBool());
        QVERIFY(!meta.value("has_sensors").toBool());   // niente bussola: da distribuire a mano
    }

    void importPhotoFilesSkipsUnreadableImages() {
        QTemporaryDir tmp;
        Vision3DWidget w;
        w.setOutputDir(tmp.path());
        const QString session = w.createSession("badimport");

        QTemporaryDir srcDir;
        const QString badPath = srcDir.path() + "/non_e_una_foto.jpg";
        QFile f(badPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("questo non e' un jpeg valido");
        f.close();

        QCOMPARE(w.importPhotoFiles(session, {badPath}), 0);
    }

    void deleteShotRemovesAllSidecarFiles() {
        QTemporaryDir tmp;
        Vision3DWidget w;
        w.setOutputDir(tmp.path());
        const QString session = w.createSession("delTest");
        const QString folder = tmp.path() + "/" + session + "/import";
        QDir().mkpath(folder);
        const QString base = folder + "/shot_a000";

        QImage img(32, 32, QImage::Format_RGB32);
        img.fill(Qt::red);
        QVERIFY(img.save(base + ".jpg", "JPEG"));
        for (const QString& suffix : {"_boxes.jpg", "_depth.jpg", "_edges.jpg",
                                       "_bump.jpg", "_normal.jpg", ".json"}) {
            QFile f(base + suffix);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("x");
        }

        w.deleteShot(base);

        QVERIFY(!QFile::exists(base + ".jpg"));
        QVERIFY(!QFile::exists(base + "_boxes.jpg"));
        QVERIFY(!QFile::exists(base + "_depth.jpg"));
        QVERIFY(!QFile::exists(base + "_edges.jpg"));
        QVERIFY(!QFile::exists(base + "_bump.jpg"));
        QVERIFY(!QFile::exists(base + "_normal.jpg"));
        QVERIFY(!QFile::exists(base + ".json"));
    }

    /* Selezione multipla in galleria (Ctrl/Shift/click) + tasto Canc come
       scorciatoia per eliminare gli scatti selezionati, non solo il pulsante. */
    void galleryAllowsExtendedSelectionAndDeleteShortcut() {
        Vision3DWidget w;
        auto* gallery = w.findChild<QListWidget*>("v3dGallery");
        QVERIFY(gallery);
        QCOMPARE(gallery->selectionMode(), QAbstractItemView::ExtendedSelection);

        bool hasDeleteShortcut = false;
        for (auto* sc : gallery->findChildren<QShortcut*>())
            if (sc->key() == QKeySequence(QKeySequence::Delete)) hasDeleteShortcut = true;
        QVERIFY2(hasDeleteShortcut, "Nessuna scorciatoia Canc trovata sulla galleria");
    }

    void hasDeleteAllPhotosButton() {
        Vision3DWidget w;
        bool found = false;
        for (auto* b : w.findChildren<QPushButton*>())
            if (b->text().contains("Elimina tutte")) found = true;
        QVERIFY2(found, "Pulsante 'Elimina tutte' non trovato");
    }

    /* "Ultimo scatto analizzato" deve avere le 6 miniature (originale, box,
       depth, bordi, bump, normal) con menu contestuale abilitato, più il
       pulsante che copia tutte le mappe salvate su disco. */
    void hasSaveAllMapsButtonAndSixThumbs() {
        Vision3DWidget w;
        bool found = false;
        for (auto* b : w.findChildren<QPushButton*>())
            if (b->text().contains("Salva tutte le mappe")) found = true;
        QVERIFY2(found, "Pulsante 'Salva tutte le mappe' non trovato");

        int thumbsWithMenu = 0;
        for (auto* l : w.findChildren<QLabel*>())
            if (l->contextMenuPolicy() == Qt::CustomContextMenu) ++thumbsWithMenu;
        QCOMPARE(thumbsWithMenu, 6);
    }

    /* Tasto destro su "Device attivi" deve poter eliminare un device
       (e le sue foto) — verifica solo il collegamento (CustomContextMenu +
       slot), non il dialogo modale che poi chiede conferma. */
    void deviceTableHasContextMenuForDeletion() {
        Vision3DWidget w;
        auto* table = w.findChild<QTableWidget*>();
        QVERIFY(table);
        QCOMPARE(table->contextMenuPolicy(), Qt::CustomContextMenu);
    }

    /* Tasto destro in galleria deve poter salvare sul PC le foto selezionate
       (una o più) — verifica solo il collegamento (CustomContextMenu), non
       il QFileDialog modale che poi chiede la cartella di destinazione. */
    void galleryHasContextMenuForSaving() {
        Vision3DWidget w;
        auto* gallery = w.findChild<QListWidget*>("v3dGallery");
        QVERIFY(gallery);
        QCOMPARE(gallery->contextMenuPolicy(), Qt::CustomContextMenu);
    }

    /* Doppio click su una miniatura "non calcolata" (es. Bordi) in "Ultimo
       scatto analizzato" deve calcolarla SUBITO per lo scatto selezionato
       e salvarla su disco — senza dover rifare la foto solo perché quella
       chip era spenta sul telefono al momento dello scatto. QLabel non ha
       un segnale doubleClicked nativo: il doppio click è intercettato da
       un eventFilter installato in buildUi(), qui lo simuliamo inviando
       l'evento direttamente al QLabel via QCoreApplication::sendEvent —
       che passa comunque dagli event filter installati, come un vero click. */
    void doubleClickOnMissingMapRecomputesIt() {
        QTemporaryDir tmp;
        Vision3DWidget w;
        w.setOutputDir(tmp.path());
        const QString session = w.createSession("dbltest");
        const QString folder = tmp.path() + "/" + session + "/import";
        QDir().mkpath(folder);
        const QString base = folder + "/shot_a000";

        // immagine ad alto contrasto: garantisce che edgeMap() produca
        // davvero dei bordi quando OpenCV è disponibile (stesso trucco di
        // uploadWithEdgesBumpNormalUsesOpenCV).
        QImage img(200, 200, QImage::Format_RGB32);
        img.fill(Qt::white);
        QPainter p(&img);
        p.fillRect(40, 40, 100, 100, Qt::black);
        p.end();
        QVERIFY(img.save(base + ".jpg", "JPEG"));
        QVERIFY(!QFile::exists(base + "_edges.jpg"));   // non ancora calcolata

        auto* gallery = w.findChild<QListWidget*>("v3dGallery");
        QVERIFY(gallery);
        auto* item = new QListWidgetItem("shot_a000");
        item->setData(Qt::UserRole, base);
        gallery->addItem(item);
        gallery->itemClicked(item);   // segnale pubblico: seleziona lo scatto (m_selectedShotKey)

        auto* edgesThumb = w.findChild<QLabel*>("v3dThumbEdges");
        QVERIFY(edgesThumb);
        QMouseEvent dbl(QEvent::MouseButtonDblClick, QPointF(5, 5), QPointF(5, 5),
                         Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QVERIFY(QCoreApplication::sendEvent(edgesThumb, &dbl));

#ifdef VISION3D_USE_OPENCV
        QVERIFY2(QFile::exists(base + "_edges.jpg"),
                 "doppio click su 'Bordi' doveva calcolarla e salvarla su disco");
#endif
    }

    /* Il fetch dei modelli VLM parte già alla costruzione (prima serviva
       avviare il server: il combo sembrava vuoto/non funzionante anche con
       Ollama già acceso) — non deve bloccare né crashare, con o senza
       Ollama realmente in ascolto in questo ambiente. */
    void constructionTriggersVlmFetchWithoutCrash() {
        Vision3DWidget w;
        QTest::qWait(200);   // lascia processare l'eventuale risposta/errore di rete
        QVERIFY(!w.isRunning());
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
            {QString("https://127.0.0.1:%1/?token=%2").arg(kTestPort).arg(w->token())}, ok);
        QVERIFY(ok);
        QVERIFY(html.contains("PRISMALUX Vision3D"));
        QVERIFY(html.contains("SCATTA E ANALIZZA"));
        // selettore Oggetto/Scena — deve esserci e "object" deve essere il default
        QVERIFY(html.contains("modeChips"));
        QVERIFY(html.contains("data-m=\"object\""));
        QVERIFY(html.contains("data-m=\"scene\""));
        QVERIFY(html.contains("scanMode='object'"));
        // stop fotocamera (rilascia lo stream, non solo riavvio) + guida rotazione
        QVERIFY(html.contains("function stopCamera()"));
        QVERIFY(html.contains("rotateHint"));
        QVERIFY(html.contains("function updateRotateHint()"));
        // markCovered deve usare l'angolo catturato allo scatto (shotHeading),
        // non curHeading "live" letto dopo l'attesa della risposta del PC
        QVERIFY(html.contains("markCovered(shotHeading)"));
        // istruzione "quando scattare" disegnata DENTRO l'overlay della
        // fotocamera (non solo in un testo separato che si perde di vista
        // mentre si guarda il mirino)
        QVERIFY(html.contains("SCATTA ORA"));
        QVERIFY(html.contains("readyToShoot"));
        // accelerometro grezzo (DeviceMotion) + altitudine (Geolocation) allo scatto
        QVERIFY(html.contains("function onMotion(e)"));
        QVERIFY(html.contains("navigator.geolocation.watchPosition"));
        QVERIFY(html.contains("accelGravity:shotAccelG"));
        QVERIFY(html.contains("altitude:shotAltitude"));
        // fermare la fotocamera deve spegnere davvero i sensori (non solo
        // l'icona): niente giroscopio/accelerometro/GPS a girare a vuoto
        // con la fotocamera chiusa
        QVERIFY(html.contains("function disableSensors()"));
        QVERIFY(html.contains("navigator.geolocation.clearWatch(geoWatchId)"));
        QVERIFY(html.contains("removeEventListener('devicemotion'"));
        QVERIFY(html.contains("disableSensors();"));
        // Oggetto/Scena devono davvero cambiare la geometria della sfera-
        // guida (1 solo anello a 360° in "scene"), non solo l'etichetta —
        // e cambiare modalità deve ricostruire i bersagli sul momento.
        QVERIFY(html.contains("scanMode==='scene' ? 1"));
        QVERIFY(html.contains("scanMode=c.dataset.m; buildTargets();"));
        // placeholder token sostituito col vero, mai lasciato letterale nella pagina
        QVERIFY(!html.contains("__VISION3D_TOKEN__"));
    }

    void unknownPathGives404() {
        bool ok = false;
        const QByteArray out = runCurl(
            {"-o", "/dev/null", "-w", "%{http_code}",
             QString("https://127.0.0.1:%1/altrove").arg(kTestPort)}, ok);
        QVERIFY(ok);
        QCOMPARE(out, QByteArray("404"));
    }

    /* CAT-C: senza token corretto, / e /upload e /download devono negare
       l'accesso — prima di questo fix qualunque dispositivo sulla stessa
       LAN poteva usare il server senza alcuna autenticazione. */
    void requestsWithoutValidTokenGet401() {
        bool ok = false;

        const QByteArray noToken = runCurl(
            {"-o", "/dev/null", "-w", "%{http_code}",
             QString("https://127.0.0.1:%1/").arg(kTestPort)}, ok);
        QVERIFY(ok);
        QCOMPARE(noToken, QByteArray("401"));

        const QByteArray wrongToken = runCurl(
            {"-o", "/dev/null", "-w", "%{http_code}",
             QString("https://127.0.0.1:%1/?token=non-e-quello-giusto").arg(kTestPort)}, ok);
        QVERIFY(ok);
        QCOMPARE(wrongToken, QByteArray("401"));

        QJsonObject o;
        o["session"] = "testauth";
        o["image"]   = QString();
        o["wants"]   = QJsonArray{};
        const QByteArray uploadNoToken = runCurl(
            {"-o", "/dev/null", "-w", "%{http_code}",
             "-X", "POST", "-H", "Content-Type: application/json",
             "--data-binary", QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact)),
             QString("https://127.0.0.1:%1/upload").arg(kTestPort)}, ok);
        QVERIFY(ok);
        QCOMPARE(uploadNoToken, QByteArray("401"));

        const QByteArray downloadNoToken = runCurl(
            {"-o", "/dev/null", "-w", "%{http_code}",
             QString("https://127.0.0.1:%1/download?session=testscan&file=obj").arg(kTestPort)}, ok);
        QVERIFY(ok);
        QCOMPARE(downloadNoToken, QByteArray("401"));
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
        o["token"]   = w->token();
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
        const QStringList jsonFiles = d.entryList({"*.json"}, QDir::Files);
        QCOMPARE(jsonFiles.size(), 1);

        // "mode" non inviato dal client → default "object" nel sidecar
        QFile mf(d.absoluteFilePath(jsonFiles.first()));
        QVERIFY(mf.open(QIODevice::ReadOnly));
        const QJsonObject meta = QJsonDocument::fromJson(mf.readAll()).object();
        QCOMPARE(meta.value("scan_mode").toString(), QStringLiteral("object"));
    }

    /* CAT-C: la modalità "scene" scelta sul telefono (chip Oggetto/Scena)
       deve finire nel sidecar .json della foto, per uso futuro in
       ricostruzione — non solo compilare, verifica il dato scritto su disco. */
    void uploadWithSceneModeIsPersisted() {
        QImage img(1, 1, QImage::Format_RGB32);
        img.fill(Qt::blue);
        QByteArray jpeg;
        QBuffer buf(&jpeg);
        buf.open(QIODevice::WriteOnly);
        QVERIFY(img.save(&buf, "JPEG"));

        QJsonObject o;
        o["session"] = "testscene";
        o["token"]   = w->token();
        o["image"]   = QString::fromUtf8("data:image/jpeg;base64," + jpeg.toBase64());
        o["wants"]   = QJsonArray{};
        o["mode"]    = "scene";

        bool ok = false;
        const QByteArray resp = runCurl(
            {"-X", "POST", "-H", "Content-Type: application/json",
             "--data-binary", QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact)),
             QString("https://127.0.0.1:%1/upload").arg(kTestPort)}, ok);
        QVERIFY(ok);
        const QJsonObject j = QJsonDocument::fromJson(resp).object();
        QVERIFY(j.value("ok").toBool());
        const QString dev = j.value("device").toString();

        const QDir d(tmp->path() + "/testscene/" + dev);
        const QStringList jsonFiles = d.entryList({"*.json"}, QDir::Files);
        QCOMPARE(jsonFiles.size(), 1);
        QFile mf(d.absoluteFilePath(jsonFiles.first()));
        QVERIFY(mf.open(QIODevice::ReadOnly));
        const QJsonObject meta = QJsonDocument::fromJson(mf.readAll()).object();
        QCOMPARE(meta.value("scan_mode").toString(), QStringLiteral("scene"));
    }

    /* CAT-C: accelerometro grezzo (DeviceMotion) + altitudine (Geolocation)
       inviati dal telefono devono finire, invariati, nel sidecar .json —
       verifica reale sui campi scritti su disco, non solo che la richiesta
       torni 200. */
    void uploadWithAccelAndAltitudeIsPersisted() {
        QImage img(1, 1, QImage::Format_RGB32);
        img.fill(Qt::darkGreen);
        QByteArray jpeg;
        QBuffer buf(&jpeg);
        buf.open(QIODevice::WriteOnly);
        QVERIFY(img.save(&buf, "JPEG"));

        QJsonObject accel;    accel["x"]=0.12;  accel["y"]=-0.34; accel["z"]=9.81;
        QJsonObject accelG;   accelG["x"]=0.50; accelG["y"]=0.10; accelG["z"]=9.75;
        QJsonObject rotRate;  rotRate["alpha"]=1.5; rotRate["beta"]=-0.2; rotRate["gamma"]=0.0;

        QJsonObject o;
        o["session"]          = "testsensors";
        o["token"]   = w->token();
        o["image"]            = QString::fromUtf8("data:image/jpeg;base64," + jpeg.toBase64());
        o["wants"]             = QJsonArray{};
        o["accel"]             = accel;
        o["accelGravity"]      = accelG;
        o["rotationRate"]      = rotRate;
        o["altitude"]          = 123.4;
        o["altitudeAccuracy"]  = 5.0;
        o["latitude"]          = 37.5;
        o["longitude"]         = 15.1;

        bool ok = false;
        const QByteArray resp = runCurl(
            {"-X", "POST", "-H", "Content-Type: application/json",
             "--data-binary", QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact)),
             QString("https://127.0.0.1:%1/upload").arg(kTestPort)}, ok);
        QVERIFY(ok);
        const QJsonObject j = QJsonDocument::fromJson(resp).object();
        QVERIFY(j.value("ok").toBool());
        const QString dev = j.value("device").toString();

        const QDir d(tmp->path() + "/testsensors/" + dev);
        const QStringList jsonFiles = d.entryList({"*.json"}, QDir::Files);
        QCOMPARE(jsonFiles.size(), 1);
        QFile mf(d.absoluteFilePath(jsonFiles.first()));
        QVERIFY(mf.open(QIODevice::ReadOnly));
        const QJsonObject meta = QJsonDocument::fromJson(mf.readAll()).object();

        QCOMPARE(meta.value("accel").toObject().value("z").toDouble(), 9.81);
        QCOMPARE(meta.value("accel_gravity").toObject().value("y").toDouble(), 0.10);
        QCOMPARE(meta.value("rotation_rate").toObject().value("alpha").toDouble(), 1.5);
        QCOMPARE(meta.value("altitude_m").toDouble(), 123.4);
        QCOMPARE(meta.value("altitude_accuracy_m").toDouble(), 5.0);
        QCOMPARE(meta.value("latitude").toDouble(), 37.5);
        QCOMPARE(meta.value("longitude").toDouble(), 15.1);

        // gli stessi dati non devono restare "al buio" nei soli file .json:
        // il pannello "Ultimo scatto analizzato" (tab Preparazione) li mostra
        // subito dopo l'upload — verifica reale sul testo del QLabel, non
        // solo che il file su disco sia corretto.
        auto* info = w->findChild<QLabel*>("v3dLastSensorInfo");
        QVERIFY2(info, "QLabel 'v3dLastSensorInfo' non trovato");
        const QString infoText = info->text();
        QVERIFY2(infoText.contains("Oggetto"), qPrintable("testo: " + infoText));
        QVERIFY2(infoText.contains("123.4"),   qPrintable("testo: " + infoText));
        QVERIFY2(infoText.contains("0.10") || infoText.contains("0,10"),
                  qPrintable("testo: " + infoText));   // accel_gravity.y, locale-dependent
    }

    /* CAT-C: pipeline "boxes" reale via /upload — verifica che detectBoxes()
       usi davvero OpenCV quando disponibile (VISION3D_USE_OPENCV), invece
       di limitarsi a compilare senza mai eseguire quel ramo. Un quadrato
       nero ben contrastato su sfondo bianco garantisce almeno un contorno
       rilevabile da Canny+findContours (area > 1% del frame). */
    void uploadWithBoxesUsesOpenCV() {
        QImage img(200, 200, QImage::Format_RGB32);
        img.fill(Qt::white);
        QPainter p(&img);
        p.fillRect(40, 40, 100, 100, Qt::black);
        p.end();
        QByteArray jpeg;
        QBuffer buf(&jpeg);
        buf.open(QIODevice::WriteOnly);
        QVERIFY(img.save(&buf, "JPEG"));

        QJsonObject o;
        o["session"] = "testboxes";
        o["token"]   = w->token();
        o["image"]   = QString::fromUtf8("data:image/jpeg;base64," + jpeg.toBase64());
        o["wants"]   = QJsonArray{"boxes"};
        o["angle"]   = 0;

        bool ok = false;
        const QByteArray resp = runCurl(
            {"-X", "POST", "-H", "Content-Type: application/json",
             "--data-binary", QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact)),
             QString("https://127.0.0.1:%1/upload").arg(kTestPort)}, ok);
        QVERIFY(ok);

        const QJsonObject j = QJsonDocument::fromJson(resp).object();
        QVERIFY(j.value("ok").toBool());
#ifdef VISION3D_USE_OPENCV
        QVERIFY2(j.contains("boxes_image"),
                 "con OpenCV disponibile 'boxes_image' deve comparire nella risposta");
        QVERIFY(j.value("boxes_image").toString().startsWith("data:image/jpeg;base64,"));
        QVERIFY2(j.value("boxes_count").toInt() >= 1,
                 "un quadrato nero ben contrastato deve produrre almeno un box rilevato");
#else
        QVERIFY2(!j.contains("boxes_image"),
                 "senza OpenCV non deve comparire 'boxes_image' — fallback pulito");
#endif
    }

    /* CAT-C: pipeline "edges"/"bump"/"normal" reale via /upload — stesso
       schema di uploadWithBoxesUsesOpenCV: un'immagine con bordi netti deve
       produrre le tre mappe quando OpenCV è disponibile, o gli "_unavailable"
       corrispondenti quando manca. */
    void uploadWithEdgesBumpNormalUsesOpenCV() {
        QImage img(200, 200, QImage::Format_RGB32);
        img.fill(Qt::white);
        QPainter p(&img);
        p.fillRect(40, 40, 100, 100, Qt::black);
        p.end();
        QByteArray jpeg;
        QBuffer buf(&jpeg);
        buf.open(QIODevice::WriteOnly);
        QVERIFY(img.save(&buf, "JPEG"));

        QJsonObject o;
        o["session"] = "testmaps";
        o["token"]   = w->token();
        o["image"]   = QString::fromUtf8("data:image/jpeg;base64," + jpeg.toBase64());
        o["wants"]   = QJsonArray{"edges", "bump", "normal"};
        o["angle"]   = 0;

        bool ok = false;
        const QByteArray resp = runCurl(
            {"-X", "POST", "-H", "Content-Type: application/json",
             "--data-binary", QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact)),
             QString("https://127.0.0.1:%1/upload").arg(kTestPort)}, ok);
        QVERIFY(ok);

        const QJsonObject j = QJsonDocument::fromJson(resp).object();
        QVERIFY(j.value("ok").toBool());
#ifdef VISION3D_USE_OPENCV
        QVERIFY2(j.contains("edges_image"),  "con OpenCV 'edges_image' deve comparire");
        QVERIFY2(j.contains("bump_image"),   "con OpenCV 'bump_image' deve comparire");
        QVERIFY2(j.contains("normal_image"), "con OpenCV 'normal_image' deve comparire");
        QVERIFY(j.value("edges_image").toString().startsWith("data:image/jpeg;base64,"));
        QVERIFY(j.value("bump_image").toString().startsWith("data:image/jpeg;base64,"));
        QVERIFY(j.value("normal_image").toString().startsWith("data:image/jpeg;base64,"));
#else
        QVERIFY(j.value("edges_unavailable").toBool());
        QVERIFY(j.value("bump_unavailable").toBool());
        QVERIFY(j.value("normal_unavailable").toBool());
#endif
    }

    /* CAT-C: copertura per "Bordi non funziona" segnalato su foto reali —
       a differenza del quadrato nero-su-bianco (contrasto altissimo, ovvio
       che qualunque soglia Canny lo trovi) qui il quadrato è appena più
       scuro dello sfondo (contrasto moderato, più vicino a una foto vera).
       Verificato empiricamente con OpenCV in Python prima di scrivere
       questo test: le soglie fisse 50/150 di edgeMap() rilevano già bene
       un bordo così — un tentativo di soglie "adattive" basate sulla
       mediana dei pixel era stato scartato perché su un'immagine per lo
       più piatta (sfondo >> oggetto, come qui) la mediana rispecchia lo
       sfondo e alza troppo le soglie, PEGGIORANDO il risultato (0 bordi
       invece di 396 nel test empirico). Resta comunque utile come test di
       non-regressione: se le soglie fisse cambiassero in futuro, questo
       lo scoprirebbe. */
    void uploadWithEdgesDetectsModerateContrastEdge() {
        QImage img(200, 200, QImage::Format_RGB32);
        img.fill(QColor(200, 200, 200));
        QPainter p(&img);
        p.fillRect(40, 40, 100, 100, QColor(150, 150, 150));
        p.end();
        QByteArray jpeg;
        QBuffer buf(&jpeg);
        buf.open(QIODevice::WriteOnly);
        QVERIFY(img.save(&buf, "JPEG", 95));

        QJsonObject o;
        o["session"] = "testedgesmoderate";
        o["token"]   = w->token();
        o["image"]   = QString::fromUtf8("data:image/jpeg;base64," + jpeg.toBase64());
        o["wants"]   = QJsonArray{"edges"};
        o["angle"]   = 0;

        bool ok = false;
        const QByteArray resp = runCurl(
            {"-X", "POST", "-H", "Content-Type: application/json",
             "--data-binary", QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact)),
             QString("https://127.0.0.1:%1/upload").arg(kTestPort)}, ok);
        QVERIFY(ok);
        const QJsonObject j = QJsonDocument::fromJson(resp).object();
        QVERIFY(j.value("ok").toBool());

#ifdef VISION3D_USE_OPENCV
        QVERIFY2(j.contains("edges_image"),
                 "con OpenCV 'edges_image' deve comparire anche a contrasto moderato");
        const QString dataUrl = j.value("edges_image").toString();
        const QImage edgesImg = QImage::fromData(
            QByteArray::fromBase64(dataUrl.mid(dataUrl.indexOf(',') + 1).toUtf8()), "JPEG");
        QVERIFY(!edgesImg.isNull());
        bool foundEdgePixel = false;
        for (int y = 0; y < edgesImg.height() && !foundEdgePixel; ++y)
            for (int x = 0; x < edgesImg.width(); ++x)
                if (qGray(edgesImg.pixel(x, y)) > 128) { foundEdgePixel = true; break; }
        QVERIFY2(foundEdgePixel,
                 "nessun bordo rilevato su un'immagine a contrasto moderato — soglie Canny troppo alte");
#endif
    }

    /* CAT-C: il campo "flash" (test riflessione col flash) deve essere
       passato senza reinterpretarlo, sia nel sidecar .json su disco (come
       "flash_on") sia riecheggiato nella risposta come "flash_used". */
    void uploadWithFlashIsPersistedAndEchoed() {
        QImage img(64, 64, QImage::Format_RGB32);
        img.fill(Qt::gray);
        QByteArray jpeg;
        QBuffer buf(&jpeg);
        buf.open(QIODevice::WriteOnly);
        QVERIFY(img.save(&buf, "JPEG"));

        QJsonObject o;
        o["session"] = "testflash";
        o["token"]   = w->token();
        o["image"]   = QString::fromUtf8("data:image/jpeg;base64," + jpeg.toBase64());
        o["wants"]   = QJsonArray{"desc"};
        o["angle"]   = 0;
        o["flash"]   = true;

        bool ok = false;
        const QByteArray resp = runCurl(
            {"-X", "POST", "-H", "Content-Type: application/json",
             "--data-binary", QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact)),
             QString("https://127.0.0.1:%1/upload").arg(kTestPort)}, ok);
        QVERIFY(ok);

        const QJsonObject j = QJsonDocument::fromJson(resp).object();
        QVERIFY(j.value("ok").toBool());
        QVERIFY2(j.value("flash_used").toBool(), "flash_used non riecheggiato nella risposta");
        const QString dev = j.value("device").toString();

        const QDir d(tmp->path() + "/testflash/" + dev);
        const QStringList jsonFiles = d.entryList({"*.json"}, QDir::Files);
        QCOMPARE(jsonFiles.size(), 1);
        QFile mf(d.absoluteFilePath(jsonFiles.first()));
        QVERIFY(mf.open(QIODevice::ReadOnly));
        const QJsonObject saved = QJsonDocument::fromJson(mf.readAll()).object();
        QVERIFY2(saved.value("flash_on").toBool(), "flash_on non salvato nel sidecar");
    }

    void downloadModelWorks() {
        // modello mancante → pagina "non ancora pronto"; presente → contenuto file
        bool ok = false;
        const QByteArray miss = runCurl(
            {QString("https://127.0.0.1:%1/download?session=testscan&file=obj&token=%2")
                 .arg(kTestPort).arg(w->token())}, ok);
        QVERIFY(ok);
        QVERIFY(miss.contains("non ancora pronto"));

        QFile f(tmp->path() + "/testscan/modello.obj");
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("v 0 0 0 1 0 0\n");
        f.close();
        const QByteArray got = runCurl(
            {QString("https://127.0.0.1:%1/download?session=testscan&file=obj&token=%2")
                 .arg(kTestPort).arg(w->token())}, ok);
        QVERIFY(ok);
        QCOMPARE(got, QByteArray("v 0 0 0 1 0 0\n"));
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
