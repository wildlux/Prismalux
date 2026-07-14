#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QSettings>
#include <QCoreApplication>
#include <QTranslator>
#include <QLocale>
#include <QUuid>
#include <QLocalServer>
#include <QLocalSocket>
#include <QSplashScreen>
#include <QPainter>
#include <QWindow>
#include <csignal>
#include <cstdio>
#include "mainwindow.h"
#include "lan_server.h"
#include "ai_client.h"
#include "prismalux_paths.h"
#include "dpi_utils.h"
#include "widgets/log_utils.h"

namespace P = PrismaluxPaths;

/* ── Single-instance (TODO D-18) ─────────────────────────────────────────────
 * Socket locale per-utente: la seconda istanza si connette, la prima riceve
 * newConnection e porta la finestra in primo piano invece di duplicarsi. */
static QString instanceSocketName() {
    return QStringLiteral("Prismalux_GUI_")
         + qEnvironmentVariable("USER", QStringLiteral("default"));
}

class SingleInstanceGuard : public QObject {
    Q_OBJECT
public:
    SingleInstanceGuard(QLocalServer* srv, QWidget* win)
        : QObject(win), m_srv(srv), m_win(win) {
        connect(srv, &QLocalServer::newConnection,
                this, &SingleInstanceGuard::onNewConnection);
        /* connessioni arrivate durante il boot (prima del connect) */
        if (srv->hasPendingConnections())
            onNewConnection();
    }
private slots:
    void onNewConnection() {
        while (QLocalSocket* c = m_srv->nextPendingConnection())
            c->deleteLater();
        /* NON showNormal(): de-massimizza una finestra già visibile a tutto
         * schermo → su Wayland appare come scomparsa+ricomparsa (flicker).
         * Togli solo lo stato minimized, preserva maximized/fullscreen. */
        m_win->setWindowState(m_win->windowState() & ~Qt::WindowMinimized);
        m_win->show();
        m_win->raise();
        m_win->activateWindow();
    }
private:
    QLocalServer* m_srv;
    QWidget*      m_win;
};

/* ── Splash — feedback immediato durante l'init pesante (QtWebEngine ecc.) ── */
static QPixmap buildSplashPixmap() {
    const int w = dpiScale(460);
    const int h = dpiScale(240);
    QPixmap pm(w, h);
    pm.fill(QColor("#0f172a"));
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);

    QFont f = qApp->font();
    f.setPointSize(26);
    f.setBold(true);
    p.setFont(f);
    p.setPen(QColor("#38bdf8"));
    p.drawText(QRect(0, dpiScale(55), w, dpiScale(50)), Qt::AlignHCenter,
               QString::fromUtf8("\xf0\x9f\x8d\xba Prismalux"));

    f.setPointSize(10);
    f.setBold(false);
    p.setFont(f);
    p.setPen(QColor("#94a3b8"));
    p.drawText(QRect(0, dpiScale(115), w, dpiScale(24)), Qt::AlignHCenter,
               "v" + QCoreApplication::applicationVersion()
               + QString::fromUtf8(" \xe2\x80\x94 Costruito per i mortali che aspirano alla saggezza."));

    p.setPen(QColor("#1e293b"));
    p.drawRect(0, 0, w - 1, h - 1);
    return pm;
}

/* Corregge i permessi dei file sensibili in ~/.prismalux/ e ~/.prismalux_chats/.
 * Eseguita una volta all'avvio, prima di qualsiasi I/O. */
static void fixPrismaluxPermissions() {
    const QString base = QDir::homePath() + "/.prismalux";
    QDir().mkpath(base);

    /* File di configurazione: 0640 (owner rw, group r, others niente) */
    static const char* kConfigFiles[] = {
        "ai_params.json", "cron_jobs.json", "bh_history.json",
        "prompt_level_results.json", "access.log", nullptr
    };
    for (int i = 0; kConfigFiles[i]; ++i) {
        const QString path = base + "/" + kConfigFiles[i];
        if (QFile::exists(path))
            QFile::setPermissions(path,
                QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                QFileDevice::ReadGroup);
    }

    /* Directory chat e tutti i file JSON al suo interno: 0700 / 0600 */
    const QString chatsDir = QDir::homePath() + "/.prismalux_chats";
    QDir().mkpath(chatsDir);
    QFile::setPermissions(chatsDir,
        QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    for (const QFileInfo& fi : QDir(chatsDir).entryInfoList({"*.json"}, QDir::Files)) {
        QFile::setPermissions(fi.absoluteFilePath(),
            QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    }
}

static void loadBundledFonts() {
    const QString fontsDir = QCoreApplication::applicationDirPath() + "/fonts";
    const QStringList filters{ "*.ttf", "*.otf", "*.TTF", "*.OTF" };
    for (const QFileInfo& fi : QDir(fontsDir).entryInfoList(filters, QDir::Files))
        QFontDatabase::addApplicationFont(fi.absoluteFilePath());
}

/* ── Emoji fallback font ─────────────────────────────────────────────────────
   Registra NotoColorEmoji (o equivalente) e lo imposta come famiglia
   di fallback globale per tutti i widget, garantendo la visualizzazione
   delle emoji Unicode su qualsiasi sistema (Linux, Windows, macOS).       */
static void setupEmojiFallback() {
    /* Percorsi in ordine di priorità: bundle app → sistema Linux → Windows */
    const QString appDir = QCoreApplication::applicationDirPath();
    static const QStringList kCandidates = {
        appDir + "/fonts/NotoColorEmoji.ttf",
        "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf",
        "/usr/share/fonts/noto/NotoColorEmoji.ttf",
        "/usr/share/fonts/noto-color-emoji/NotoColorEmoji.ttf",
        "C:/Windows/Fonts/seguiemj.ttf",
    };

    for (const QString& p : kCandidates) {
        if (QFile::exists(p)) {
            QFontDatabase::addApplicationFont(p);
            break;
        }
    }

    /* Aggiunge le famiglie emoji disponibili come fallback per il font di app */
    static const QStringList kEmojiFamilies = {
        "Noto Color Emoji",
        "Segoe UI Emoji",
        "Apple Color Emoji",
        "Twemoji Mozilla",
        "Emoji One",
        "EmojiOne Color",
    };

    const QStringList available = QFontDatabase::families();
    QFont appFont = qApp->font();
    QStringList families = appFont.families();
    if (families.isEmpty()) families << appFont.family();

    for (const QString& ef : kEmojiFamilies)
        if (available.contains(ef) && !families.contains(ef))
            families << ef;

    appFont.setFamilies(families);
    qApp->setFont(appFont);
}

static void handleSigInt(int) {
    QCoreApplication::quit();
}

static int runHeadlessServer(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    app.setApplicationName("Prismalux");
    app.setApplicationVersion("3.0");
    app.setOrganizationName("Prismalux");

    const QStringList args = app.arguments();

    quint16 port = static_cast<quint16>(P::SK::kLanPort
        ? QSettings("Prismalux", "GUI").value(P::SK::kLanPort, 11500).toInt()
        : 11500);

    {
        const int portIdx = args.indexOf("--port");
        if (portIdx != -1 && portIdx + 1 < args.size()) {
            bool ok = false;
            const int p = args.at(portIdx + 1).toInt(&ok);
            if (ok && p > 0 && p < 65536)
                port = static_cast<quint16>(p);
        }
    }

    QString token;
    {
        const int tokIdx = args.indexOf("--token");
        if (tokIdx != -1 && tokIdx + 1 < args.size())
            token = args.at(tokIdx + 1);
    }

    if (token.isEmpty())
        token = LanServer::loadLanToken();
    if (token.isEmpty())
        token = QUuid::createUuid().toString(QUuid::WithoutBraces).left(24);

    LanServer::saveLanToken(token);

    auto* ai  = new AiClient(&app);
    auto* srv = new LanServer(ai, &app);
    srv->setAccessToken(token);
    srv->setHeadless(true);           /* disabilita /api/launch in headless */
    srv->setBindAddress(QHostAddress::LocalHost); /* bind 127.0.0.1 di default in headless */

    if (!srv->start(port)) {
        std::fprintf(stderr, "Prismalux server: impossibile aprire la porta %d\n",
                     static_cast<int>(port));
        return 1;
    }

    std::printf("Prismalux server avviato su porta %d -- token: %s\n",
                static_cast<int>(port), token.toUtf8().constData());
    std::fflush(stdout);

    QObject::connect(&app, &QCoreApplication::aboutToQuit, srv, &LanServer::stop);

    std::signal(SIGINT,  handleSigInt);
    std::signal(SIGTERM, handleSigInt);

    return app.exec();
}

int main(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        if (QByteArray(argv[i]) == "--server")
            return runHeadlessServer(argc, argv);
    }

    QApplication app(argc, argv);

    /* ── Log centralizzato con prefisso [CAT][LVL] emoji ─────────────────
     * Intercetta tutti i qWarning/qCritical/qDebug e aggiunge categoria
     * (QT/PY/LLM/TOOL/MCP/IO/NET/STT/RAG/SYS/SEC) dedotta dal testo.
     * Debug visibile solo con PRISMALUX_DEBUG=1 nell'ambiente.          */
    PLog::installMessageHandler();

    /* ── Corregge permessi file sensibili prima di qualsiasi I/O ── */
    fixPrismaluxPermissions();

    /* ── Metadata applicazione (usato da QSettings in ThemeManager) ── */
    app.setApplicationName("Prismalux");
    app.setApplicationVersion("3.0");
    app.setOrganizationName("Prismalux");

    /* ── Single-instance (D-18): se un'altra istanza è già in ascolto,
     *    le chiede il raise() e termina subito — niente processi duplicati
     *    da doppio click impaziente durante il boot lento.              */
    QLocalServer* instanceServer = nullptr;
    {
        QLocalSocket probe;
        probe.connectToServer(instanceSocketName());
        if (probe.waitForConnected(300)) {
            probe.disconnectFromServer();
            return 0;
        }
        /* socket stale da crash precedente → rimuovi e prendi il lock */
        QLocalServer::removeServer(instanceSocketName());
        instanceServer = new QLocalServer(&app);
        if (!instanceServer->listen(instanceSocketName()))
            qWarning() << "Single-instance: listen fallito —"
                       << instanceServer->errorString();
    }

    /* ── Splash subito visibile: l'init di MainWindow (QtWebEngine, tab
     *    eager, temi) può richiedere decine di secondi a freddo.        */
    QSplashScreen splash(buildSplashPixmap());
    splash.show();
    splash.showMessage(QObject::tr("Caricamento interfaccia\xe2\x80\xa6"),
                       Qt::AlignBottom | Qt::AlignHCenter, QColor("#e2e8f0"));
    /* Wayland: la finestra è visibile solo dopo il primo commit del buffer,
     * che richiede l'evento expose. Un singolo processEvents() non basta:
     * pompa l'event loop (max ~1.2s) finché lo splash è davvero esposto,
     * altrimenti resta invisibile durante il ctor bloccante di MainWindow. */
    for (int i = 0; i < 60; ++i) {
        app.processEvents(QEventLoop::AllEvents | QEventLoop::WaitForMoreEvents, 20);
        if (splash.windowHandle() && splash.windowHandle()->isExposed())
            break;
    }

    /* ── Font professionali dalla cartella fonts/ (facoltativo) ── */
    loadBundledFonts();

    /* ── Fallback emoji globale (NotoColorEmoji o equivalente) ── */
    setupEmojiFallback();

    /* ── Internazionalizzazione (i18n) ────────────────────────────────────────
     * Carica la traduzione corrispondente alla locale di sistema se disponibile.
     * File .qm generati da: cmake --build build_gui (richiede Qt6::LinguistTools)
     * Per aggiungere una lingua: copia i18n/prismalux_it.ts → prismalux_XX.ts,
     * traduci le stringhe, esegui cmake per rigenerare i .qm.
     * La lingua predefinita è italiano (hardcoded): il traduttore è caricato
     * solo se la locale di sistema è diversa dall'italiano.             */
    static QTranslator s_translator;
    /* Preferenza utente (Impostazioni → Sistema → Lingua): "it" | "en" | "system".
     * "system" = segue la locale del sistema operativo (comportamento storico). */
    const QString langPref = QSettings("Prismalux", "GUI")
                             .value(P::SK::kLanguage, "system").toString();
    const QString lang = (langPref == "system")
                         ? QLocale::system().name()   /* es. "en_US", "de_DE" */
                         : langPref;                  /* "it" | "en" */
    /* L'italiano è la lingua sorgente (stringhe già in italiano): nessun
     * traduttore da installare. Per le altre lingue carica il .qm. */
    if (!lang.startsWith("it")) {
        const QString appDir = QCoreApplication::applicationDirPath();
        /* I .qm possono stare in i18n/ (pacchetti/AppImage) o accanto
         * all'eseguibile (build cmake in build_gui/). Prova entrambi. */
        const QStringList qmBases = {
            appDir + "/i18n/prismalux_",
            appDir + "/prismalux_",
        };
        for (const QString& qmBase : qmBases) {
            /* Prova prima la locale completa (prismalux_en_US.qm),
             * poi solo la lingua (prismalux_en.qm). */
            if (s_translator.load(qmBase + lang + ".qm")
                || s_translator.load(qmBase + lang.left(2) + ".qm")) {
                app.installTranslator(&s_translator);
                break;
            }
        }
    }

    /* ── Variabile di processo: modalità calcolo LLM ──────────────────────
     * Impostata UNA VOLTA qui, prima che qualsiasi componente venga creato.
     * Sovrascrive la configurazione falsa causata dalla race condition tra
     * il costruttore di ManutenzioneePage (senza hw-detect) e updateHWLabel.
     * Tutti i componenti leggono PRISMALUX_COMPUTE_MODE come fonte unica. */
    {
        QSettings s("Prismalux", "GUI");
        const QString cm = s.value("ai/computeMode", "").toString();
        if (!cm.isEmpty())
            qputenv("PRISMALUX_COMPUTE_MODE", cm.toUtf8());
    }

    /* ── Disabilita dialog nativi KDE/GNOME — evita crash KIO "unknown protocol"
     * che si manifesta con Dolphin quando kioworker non trova il protocollo file://.
     * Qt usa il proprio dialog embedded che non dipende da kio-extras.           */
    app.setAttribute(Qt::AA_DontUseNativeDialogs);

    /* ── Finestra principale (carica il tema saved via ThemeManager) ── */
    MainWindow w;
    w.show();
    w.raise();
    w.activateWindow();
    splash.finish(&w);

    if (instanceServer)
        new SingleInstanceGuard(instanceServer, &w);   /* parent: w */

    return app.exec();
}

#include "main.moc"
