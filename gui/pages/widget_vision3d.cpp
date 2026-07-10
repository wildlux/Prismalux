// ============================================================================
//  widget_vision3d.cpp — Prismalux Vision3D (scansione 3D da telefono/tablet)
// ============================================================================
#include "widget_vision3d.h"
#include "../prismalux_paths.h"
#include "../dpi_utils.h"
#include "../lan_server.h"          // LanServer::_ensureCert (cert condiviso app)
#include "../log_bus.h"             // LogBus::post — pannello "Messaggi" centralizzato
#include "../widgets/qr_code_widget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QUuid>
#include <QUrl>
#include <QComboBox>
#include <QPushButton>
#include <QGroupBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QListWidget>
#include <QSpinBox>
#include <QMessageBox>
#include <QInputDialog>
#include <QFileDialog>
#include <QShortcut>
#include <QMenu>
#include <QAction>
#include <QDialog>
#include <QDialogButtonBox>
#include <QTextBrowser>
#include <QDesktopServices>
#include <QSettings>
#include <QDir>
#include <QFile>
#include <QDateTime>
#include <QPixmap>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkInterface>
#include <QEventLoop>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QMutexLocker>
#if QT_CONFIG(ssl)
#  include <QSslKey>
#  include <QSslCertificate>
#endif

#ifdef VISION3D_USE_OPENCV
  #include <opencv2/opencv.hpp>
  // Il modulo aruco è in opencv_contrib. Se presente, il CMake definisce VISION3D_USE_ARUCO.
  #ifdef VISION3D_USE_ARUCO
    #include <opencv2/aruco.hpp>
  #endif
#endif

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QImageReader>
#include <QStandardPaths>
#include <QScrollArea>
#include <QTabWidget>
#include <QSignalBlocker>
#include <QtMath>
#include <cmath>

namespace P = PrismaluxPaths;

/* Tooltip statici dei due combo sessione: costanti condivise fra buildUi()
   e refreshSessionDescriptionTooltip(), che vi appende la descrizione
   salvata senza perdere questa spiegazione di base. */
static const char* kPrepSessionComboTip =
    "Sessione corrente — la stessa del combo nella tab \"Assembla punti e texture\".";
static const char* kSessionComboTip =
    "Cartella di scansione in scan_output/ — cambiala per\n"
    "rivedere in galleria e scena 3D una sessione precedente.";

// Tetto sulla dimensione di una singola richiesta HTTP (foto base64 + JSON).
// Oltre questo limite il socket viene chiuso: protegge la RAM da client rotti/ostili.
static constexpr qsizetype kMaxRequestBytes = 32 * 1024 * 1024;

// ═══════════════════════════════════════════════════════════════════════════
//  Vision3DSceneCanvas — mini scena 3D con i frustum delle camere
// ═══════════════════════════════════════════════════════════════════════════
namespace {
struct V3 { double x, y, z; };
inline V3 v3sub(const V3& a, const V3& b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
inline V3 v3cross(const V3& a, const V3& b) {
    return {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x};
}
inline V3 v3norm(const V3& a) {
    const double l = std::sqrt(a.x*a.x + a.y*a.y + a.z*a.z);
    return l > 1e-9 ? V3{a.x/l, a.y/l, a.z/l} : V3{0, 0, 1};
}
inline V3 v3add(const V3& a, const V3& b, double k = 1.0) {
    return {a.x + b.x*k, a.y + b.y*k, a.z + b.z*k};
}
} // namespace

Vision3DSceneCanvas::Vision3DSceneCanvas(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(dpiScale(220), dpiScale(220));
    setMouseTracking(false);
    setToolTip("Posizione stimata delle camere intorno all'oggetto.\n"
               "Trascina per ruotare la vista, rotella per lo zoom.\n"
               "Cono tratteggiato = scatto senza sensori (posizione indicativa).\n"
               "Pallino tratteggiato = punto ancora da coprire; pieno = già coperto.");
}

void Vision3DSceneCanvas::addShot(const V3dShotInfo& s)
{
    for (const V3dShotInfo& e : m_shots)
        if (e.key == s.key) return;        // già presente
    m_shots.append(s);
    update();
}

void Vision3DSceneCanvas::setSelectedKey(const QString& key)
{
    m_selectedKey = key;
    update();
}

void Vision3DSceneCanvas::clearShots()
{
    m_shots.clear();
    m_selectedKey.clear();
    update();
}

bool Vision3DSceneCanvas::setShotPose(const QString& key, int heading, int pitch)
{
    for (V3dShotInfo& s : m_shots) {
        if (s.key != key) continue;
        s.heading = heading;
        s.pitch = pitch;
        s.hasSensors = true;    // posa impostata dall'utente = affidabile (linea piena)
        update();
        return true;
    }
    return false;
}

/* Stessa formula di altezza usata da drawFrustum per posizionare il cono:
   fattorizzata qui per poterla riusare nel confronto scatto/bersaglio. */
double Vision3DSceneCanvas::shotHeight(const V3dShotInfo& s) const
{
    return s.hasSensors
        ? qBound(0.05, (90.0 - qBound(-90, s.pitch, 90)) / 90.0 * 0.9, 1.2)
        : 0.45;
}

/* Bersagli-guida: N anelli di quota (in base alla qualità) con i punti
   bussola distribuiti uniformemente, per un totale vicino al numero di
   foto minime richiesto da requiredPhotosFor(). Serve solo da guida
   visiva: la tolleranza nel confronto con gli scatti reali è volutamente
   larga, non è un controllo di copertura rigoroso. */
void Vision3DSceneCanvas::setTargetQuality(const QString& quality)
{
    m_targetQuality = quality;
    m_targets.clear();

    int rings = 0, required = 0;
    if (quality == QLatin1String("low"))         { rings = 1; required = 10; }
    else if (quality == QLatin1String("medium")) { rings = 2; required = 20; }
    else if (quality == QLatin1String("high"))   { rings = 3; required = 40; }
    else { update(); return; }   // vuoto/sconosciuto = nessun bersaglio

    const int perRing = int(std::ceil(double(required) / rings));
    for (int r = 0; r < rings; ++r)
        for (int i = 0; i < perRing; ++i)
            m_targets.append({ i * 360.0 / perRing, r });
    update();
}

void Vision3DSceneCanvas::drawTargets(QPainter& p) const
{
    if (m_targets.isEmpty()) return;
    static constexpr double kRingH[3]  = {0.35, 0.75, 1.05};   // basso/medio/alto sulla sfera
    static constexpr double kHeadTolDeg = 20.0;
    static constexpr double kHTol       = 0.22;

    for (const auto& t : m_targets) {
        const double heading = t.first;
        const double ringH   = kRingH[qBound(0, t.second, 2)];

        bool covered = false;
        for (const V3dShotInfo& s : m_shots) {
            if (!s.hasSensors) continue;   // bussola indicativa: non conta per la copertura
            double dh = std::fmod(std::abs(double(s.heading) - heading), 360.0);
            if (dh > 180.0) dh = 360.0 - dh;
            if (dh > kHeadTolDeg) continue;
            if (std::abs(shotHeight(s) - ringH) > kHTol) continue;
            covered = true;
            break;
        }

        const double a = qDegreesToRadians(heading);
        const QPointF sp = project(std::cos(a), std::sin(a), ringH);

        if (covered) {
            p.setPen(Qt::NoPen);
            QColor col = palette().color(QPalette::Highlight);
            col.setAlpha(110);
            p.setBrush(col);
            p.drawEllipse(sp, dpiScale(3), dpiScale(3));
        } else {
            QPen pen(palette().color(QPalette::PlaceholderText), 1.2, Qt::DotLine);
            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(sp, dpiScale(5), dpiScale(5));
        }
    }
}

QPointF Vision3DSceneCanvas::project(double x, double y, double z) const
{
    const double yaw  = qDegreesToRadians(m_yawDeg);
    const double tilt = qDegreesToRadians(m_tiltDeg);
    const double px = x*std::cos(yaw) - y*std::sin(yaw);
    const double py = x*std::sin(yaw) + y*std::cos(yaw);
    const double s = m_zoom * qMin(width(), height()) * 0.30;
    return QPointF(width()/2.0  + s*px,
                   height()/2.0 + s*(py*std::sin(tilt) - z*std::cos(tilt)));
}

void Vision3DSceneCanvas::drawFrustum(QPainter& p, const V3dShotInfo& s, bool selected) const
{
    /* posizione camera: sul cerchio unitario all'angolo di bussola; quota
       dall'inclinazione del telefono (90°=verticale→0, 0°=guarda giù→alta).
       Senza sensori: heading distribuito uniformemente, tratteggio. */
    const int nNoSens = qMax(1, int(m_shots.size()));
    double headingDeg = s.hasSensors
        ? double(s.heading)
        : double(((s.index - 1) * (360 / nNoSens)) % 360);
    /* più scatti allo stesso angolo (bussola ferma o assente): apri a
       ventaglio, altrimenti i coni si sovrappongono e se ne vede uno solo */
    int dup = 0;
    for (const V3dShotInfo& e : m_shots) {
        if (e.key == s.key) break;
        if (e.hasSensors == s.hasSensors && e.heading == s.heading) ++dup;
    }
    headingDeg += dup * 9.0;
    const double a = qDegreesToRadians(headingDeg);
    const double h = s.hasSensors
        ? qBound(0.05, (90.0 - qBound(-90, s.pitch, 90)) / 90.0 * 0.9, 1.2)
        : 0.45;

    const V3 pos = {std::cos(a), std::sin(a), h};
    const V3 dir = v3norm(v3sub(V3{0, 0, 0.15}, pos));      // guarda l'oggetto
    const V3 right = v3norm(v3cross(dir, V3{0, 0, 1}));
    const V3 up    = v3norm(v3cross(right, dir));

    const V3 base = v3add(pos, dir, 0.38);                   // base del cono
    const double hw = 0.15, hh = 0.11;
    const V3 c1 = v3add(v3add(base, right,  hw), up,  hh);
    const V3 c2 = v3add(v3add(base, right, -hw), up,  hh);
    const V3 c3 = v3add(v3add(base, right, -hw), up, -hh);
    const V3 c4 = v3add(v3add(base, right,  hw), up, -hh);

    const QPointF P0 = project(pos.x, pos.y, pos.z);
    const QPointF Q1 = project(c1.x, c1.y, c1.z), Q2 = project(c2.x, c2.y, c2.z);
    const QPointF Q3 = project(c3.x, c3.y, c3.z), Q4 = project(c4.x, c4.y, c4.z);

    QColor col = selected ? palette().color(QPalette::Highlight)
                          : palette().color(QPalette::Mid);
    QPen pen(col, selected ? 2.2 : 1.2);
    if (!s.hasSensors) pen.setStyle(Qt::DashLine);
    p.setPen(pen);
    if (selected) {
        QColor fill = col; fill.setAlpha(60);
        QPainterPath path;
        path.moveTo(Q1); path.lineTo(Q2); path.lineTo(Q3); path.lineTo(Q4);
        path.closeSubpath();
        p.fillPath(path, fill);
    }
    p.drawLine(P0, Q1); p.drawLine(P0, Q2); p.drawLine(P0, Q3); p.drawLine(P0, Q4);
    p.drawLine(Q1, Q2); p.drawLine(Q2, Q3); p.drawLine(Q3, Q4); p.drawLine(Q4, Q1);

    if (selected) {
        p.setPen(palette().color(QPalette::Text));
        p.drawText(P0 + QPointF(dpiScale(6), -dpiScale(6)),
                   QString("%1 #%2").arg(s.device).arg(s.index, 3, 10, QChar('0')));
    }
}

void Vision3DSceneCanvas::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), palette().color(QPalette::Base));

    // cerchio a terra + croce assi
    QPen ground(palette().color(QPalette::Mid), 1.0);
    ground.setStyle(Qt::DotLine);
    p.setPen(ground);
    QPointF prev = project(1, 0, 0);
    for (int i = 1; i <= 36; ++i) {
        const double a = i * 2.0 * M_PI / 36.0;
        const QPointF cur = project(std::cos(a), std::sin(a), 0);
        p.drawLine(prev, cur);
        prev = cur;
    }
    p.drawLine(project(-1, 0, 0), project(1, 0, 0));
    p.drawLine(project(0, -1, 0), project(0, 1, 0));
    p.setPen(palette().color(QPalette::PlaceholderText));
    p.drawText(project(1.12, 0, 0),  "0\xc2\xb0");
    p.drawText(project(0, 1.12, 0),  "90\xc2\xb0");
    p.drawText(project(-1.18, 0, 0), "180\xc2\xb0");
    p.drawText(project(0, -1.18, 0), "270\xc2\xb0");

    // oggetto al centro: cubetto wireframe
    p.setPen(QPen(palette().color(QPalette::Text), 1.4));
    const double c = 0.14;
    const QPointF b1 = project(-c,-c,0), b2 = project(c,-c,0),
                  b3 = project(c,c,0),   b4 = project(-c,c,0);
    const QPointF t1 = project(-c,-c,2*c), t2 = project(c,-c,2*c),
                  t3 = project(c,c,2*c),   t4 = project(-c,c,2*c);
    p.drawLine(b1,b2); p.drawLine(b2,b3); p.drawLine(b3,b4); p.drawLine(b4,b1);
    p.drawLine(t1,t2); p.drawLine(t2,t3); p.drawLine(t3,t4); p.drawLine(t4,t1);
    p.drawLine(b1,t1); p.drawLine(b2,t2); p.drawLine(b3,t3); p.drawLine(b4,t4);

    // tasselli-bersaglio (guida): sotto ai coni reali, così restano leggibili
    drawTargets(p);

    // frustum (prima i non selezionati, il selezionato sopra)
    for (const V3dShotInfo& s : m_shots)
        if (s.key != m_selectedKey) drawFrustum(p, s, false);
    for (const V3dShotInfo& s : m_shots)
        if (s.key == m_selectedKey) drawFrustum(p, s, true);

    if (m_shots.isEmpty()) {
        p.setPen(palette().color(QPalette::PlaceholderText));
        p.drawText(rect(), Qt::AlignCenter,
                   m_targets.isEmpty()
                       ? "Gli scatti compariranno qui\ncome coni intorno all'oggetto"
                       : "I pallini tratteggiati sono i punti da\ncoprire — scatta finché non diventano pieni");
    }
}

void Vision3DSceneCanvas::mousePressEvent(QMouseEvent* e)
{
    m_lastDrag = e->pos();
}

void Vision3DSceneCanvas::mouseMoveEvent(QMouseEvent* e)
{
    if (!(e->buttons() & Qt::LeftButton)) return;
    const QPoint d = e->pos() - m_lastDrag;
    m_lastDrag = e->pos();
    m_yawDeg  += d.x() * 0.5;
    m_tiltDeg  = qBound(5.0, m_tiltDeg + d.y() * 0.4, 85.0);
    update();
}

void Vision3DSceneCanvas::wheelEvent(QWheelEvent* e)
{
    m_zoom = qBound(0.4, m_zoom * (e->angleDelta().y() > 0 ? 1.1 : 0.9), 3.0);
    update();
}

// ---------------------------------------------------------------------------
Vision3DWidget::Vision3DWidget(QWidget* parent)
    : QWidget(parent)
{
    m_net = new QNetworkAccessManager(this);
    m_outputDir   = P::root() + "/scan_output";
    m_ollamaUrl   = QStringLiteral("http://localhost:%1").arg(P::kOllamaPort);
    /* default "moondream": vision, leggero (~1.7GB) e spesso già installato;
       l'utente lo cambia dal combo VLM (persistito in QSettings) */
    m_vlmModel    = QSettings("Prismalux", "GUI")
                        .value(P::SK::kVision3dVlmModel, "moondream").toString();
    m_depthScript = P::root() + "/Tools/scripts/depth_infer.py";
    m_pythonExe   = P::findPython();
    m_port        = quint16(P::kVision3DPort);
    buildUi();
    /* Elenco modelli VLM subito, non solo quando si avvia il server: prima
       il combo restava con un solo elemento finché non premevi "Avvia
       server", sembrando vuoto/non funzionante anche con Ollama già su. */
    fetchVlmModels();
    /* riapri l'ultima sessione presente su disco: galleria e scena 3D
       ripartono piene anche dopo un riavvio dell'app */
    populateSessions();
    if (m_sessionCombo && m_sessionCombo->count() > 0) {
        loadSessionIntoUi(m_sessionCombo->currentText());
        refreshSessionDescriptionTooltip(m_sessionCombo->currentText());
    }
}

Vision3DWidget::~Vision3DWidget()
{
    /* regola progetto: QProcess ancora vivo nel distruttore emette finished()
       sincrono durante deleteChildren() → slot su widget semi-distrutti = SEGV */
    if (m_reconProc) {
        m_reconProc->disconnect(this);
        m_reconProc->blockSignals(true);
        m_reconProc->kill();
        m_reconProc->waitForFinished(1000);
    }
    stop();
}

void Vision3DWidget::setOutputDir(const QString& d)   { m_outputDir = d; }
void Vision3DWidget::setOllamaUrl(const QString& u)   { m_ollamaUrl = u; }
void Vision3DWidget::setVlmModel(const QString& m)    { m_vlmModel = m; }
void Vision3DWidget::setDepthScript(const QString& p) { m_depthScript = p; }
void Vision3DWidget::setPythonExe(const QString& p)   { m_pythonExe = p; }
void Vision3DWidget::setArucoMarkerMm(double mm)      { m_arucoMarkerMm = mm; }
void Vision3DWidget::setBindIp(const QString& ip)     { m_bindIp = ip; }

void Vision3DWidget::appendLog(const QString& s)
{
    const QString line = QDateTime::currentDateTime().toString("HH:mm:ss") + "  " + s;
    /* Nel log centralizzato (finestra "Messaggi" → tab "3D"), non più in
       un pannello locale nello spazio di lavoro: si evita di duplicare lo
       stesso evento in due posti e si tiene tutto insieme agli altri log
       dell'app. LogBus aggiunge già il suo timestamp, qui si manda il testo
       "nudo" — il segnale logMessage (per eventuali embedder esterni)
       continua a includere il timestamp locale. */
    LogBus::post(s, "3d");
    emit logMessage(line);
}

/* IPv4 locali per il bind, ordinati per preferenza: prima la LAN domestica
 * 192.168.x, poi le altre reti private, loopback in coda. Escluse le
 * interfacce virtuali (docker/virbr/vnet/br-): esporle sarebbe inutile,
 * e il bridge di condivisione connessione (es. 10.42.0.1) non è la LAN. */
QStringList Vision3DWidget::listLocalIps()
{
    QStringList lan, other;
    const auto ifaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface& itf : ifaces) {
        const QString name = itf.name();
        if (name.startsWith("docker") || name.startsWith("virbr") ||
            name.startsWith("vnet")   || name.startsWith("br-"))
            continue;
        if (!(itf.flags() & QNetworkInterface::IsUp))
            continue;
        const auto entries = itf.addressEntries();
        for (const QNetworkAddressEntry& e : entries) {
            const QHostAddress a = e.ip();
            if (a.protocol() != QAbstractSocket::IPv4Protocol || a.isLoopback())
                continue;
            const QString label = a.toString() + " (" + name + ")";
            if (a.toString().startsWith("192.168.")) lan.append(label);
            else                                     other.append(label);
        }
    }
    QStringList out = lan + other;
    out.append(QStringLiteral("127.0.0.1 (solo questo PC)"));
    return out;
}

/* IP effettivo per bind/URL: setter esplicito > combo UI > loopback. */
QString Vision3DWidget::currentBindIp() const
{
    if (!m_bindIp.isEmpty()) return m_bindIp;
    if (m_ifaceCombo && m_ifaceCombo->count() > 0)
        return m_ifaceCombo->currentText().section(' ', 0, 0);
    return QStringLiteral("127.0.0.1");
}

// ---------------------------------------------------------------------------
bool Vision3DWidget::start(quint16 port, const QString& certPath, const QString& keyPath)
{
#if !QT_CONFIG(ssl)
    Q_UNUSED(port); Q_UNUSED(certPath); Q_UNUSED(keyPath);
    appendLog("Qt compilato senza SSL: server HTTPS non disponibile.");
    return false;
#else
    if (m_running) return true;
    if (port) m_port = port;
    QDir().mkpath(m_outputDir);

    // Cert self-signed condiviso con LanServer (~/.prismalux/server.crt):
    // il telefono accetta l'avviso del browser una sola volta per tutta l'app.
    QString cert = certPath, key = keyPath;
    if (cert.isEmpty() || key.isEmpty()) {
        if (!LanServer::_ensureCert(cert, key)) {
            appendLog("openssl non disponibile: passa cert/key a start().");
            return false;
        }
    }

    QFile cf(cert), kf(key);
    if (!cf.open(QIODevice::ReadOnly) || !kf.open(QIODevice::ReadOnly)) {
        appendLog("Impossibile leggere cert/key.");
        return false;
    }
    QSslCertificate certificate(&cf, QSsl::Pem);
    QSslKey sslKey(&kf, QSsl::Rsa, QSsl::Pem);
    cf.close(); kf.close();

    m_sslConf = QSslConfiguration::defaultConfiguration();
    m_sslConf.setLocalCertificate(certificate);
    m_sslConf.setPrivateKey(sslKey);
    m_sslConf.setPeerVerifyMode(QSslSocket::VerifyNone);

    m_server = new QSslServer(this);
    m_server->setSslConfiguration(m_sslConf);
    connect(m_server, &QTcpServer::pendingConnectionAvailable,
            this, &Vision3DWidget::onNewConnection);

    /* Bind SOLO sull'interfaccia scelta (mai 0.0.0.0): il server non deve
       essere raggiungibile da reti diverse dalla LAN selezionata — es. il
       bridge hotspot 10.42.0.1 esposto verso l'esterno. */
    const QString bindIp = currentBindIp();
    if (!m_server->listen(QHostAddress(bindIp), m_port)) {
        appendLog(QString("Bind fallito su %1:%2").arg(bindIp).arg(m_port));
        delete m_server; m_server = nullptr;
        return false;
    }

    /* Token di accesso — prima il server non ne aveva nessuno: qualunque
       dispositivo sulla stessa LAN/WiFi (non solo il telefono abbinato)
       poteva POST /upload (DoS spazio disco/CPU via VLM+OpenCV) o
       GET /download indovinando un nome sessione banale (default "scan1").
       Stesso pattern di LanServer::loadLanToken()/saveLanToken(): persistito
       con QKeychain o file 0600 fallback, generato una sola volta con
       QUuid (stesso schema usato in main.cpp per il token LAN headless). */
    m_token = LanServer::loadSecret(QStringLiteral("vision3d_token"));
    if (m_token.isEmpty()) {
        m_token = QUuid::createUuid().toString(QUuid::WithoutBraces).left(24);
        LanServer::saveSecret(QStringLiteral("vision3d_token"), m_token);
    }

    m_running = true;
    const QString url = "https://" + bindIp + ":" + QString::number(m_port) + "/?token=" + m_token;
    m_urlLabel->setText(url);
    if (m_prepQr) m_prepQr->setText(url);
    m_statusDot->setStyleSheet("color:#3fb950;font-size:18px;");
    m_toggleBtn->setText("Ferma server");
    appendLog("Server attivo: " + url + "  — connetti i client iOS, Android o Desktop (browser).");
    emit serverStarted(url);
    fetchVlmModels();   // riempi il combo VLM coi modelli Ollama installati
    return true;
#endif
}

void Vision3DWidget::stop()
{
    if (!m_running) return;
#if QT_CONFIG(ssl)
    if (m_server) { m_server->close(); m_server->deleteLater(); m_server = nullptr; }
#endif
    m_running = false;
    if (m_statusDot) m_statusDot->setStyleSheet("color:#8b949e;font-size:18px;");
    if (m_toggleBtn) m_toggleBtn->setText("Avvia server");
    if (m_urlLabel) m_urlLabel->setText("(server fermo)");
    if (m_prepQr) m_prepQr->setText(QString());
    appendLog("Server fermato.");
}

void Vision3DWidget::onToggleServerClicked()
{
    if (m_running) { stop(); return; }

    /* Prima di avviare, se la sessione attuale non ha ancora foto chiedi
       quante scattarne: risponde alla domanda "minimo o di più?" subito,
       invece di scoprire dopo che ne servivano altre — e i tasselli-guida
       nella scena 3D si aggiornano di conseguenza. */
    const QString curSession = m_sessionCombo ? m_sessionCombo->currentText() : QString();
    const int already = curSession.isEmpty() ? 0 : countSessionPhotos(curSession);
    if (already == 0 && m_qualityCombo) {
        QMessageBox box(this);
        box.setWindowTitle("Vision3D \xe2\x80\x94 quante foto?");
        box.setText("Quante foto vuoi scattare per la fotogrammetria?");
        box.setInformativeText(
            "Il minimo copre l'oggetto con meno dettaglio; pi\xc3\xb9 foto "
            "danno un modello pi\xc3\xb9 preciso ma richiedono pi\xc3\xb9 scatti/tempo. "
            "Puoi comunque cambiare la qualit\xc3\xa0 in seguito dal menu accanto.");
        auto* minBtn  = box.addButton("\xf0\x9f\x93\xb7 Il minimo (~10 foto)", QMessageBox::AcceptRole);
        auto* moreBtn = box.addButton("\xf0\x9f\x93\xb8 Pi\xc3\xb9 foto (qualit\xc3\xa0 migliore, ~40)", QMessageBox::AcceptRole);
        box.addButton("Decido dopo", QMessageBox::RejectRole);
        box.exec();
        if (box.clickedButton() == minBtn) {
            const int idx = m_qualityCombo->findText("low");
            if (idx >= 0) m_qualityCombo->setCurrentIndex(idx);
        } else if (box.clickedButton() == moreBtn) {
            const int idx = m_qualityCombo->findText("high");
            if (idx >= 0) m_qualityCombo->setCurrentIndex(idx);
        }
    }

    start(quint16(m_portEdit->text().toUShort()));
}

/* Guida rapida richiamata dal pulsante "?" nell'intestazione. */
void Vision3DWidget::onHelpClicked()
{
    /* Il QR ha senso solo con un URL reale: se il server non è ancora
       avviato m_urlLabel mostra "(server fermo)", non un indirizzo — un
       QR con quel testo o col vecchio segnaposto non porta da nessuna
       parte. In quel caso il QR va nascosto, non generato lo stesso. */
    const bool hasUrl = m_running && m_urlLabel && m_urlLabel->text().startsWith("https://");
    const QString url = hasUrl ? m_urlLabel->text() : QString();
    const QString urlForGuide = hasUrl ? url
        : QStringLiteral("(avvia il server per vedere qui l'URL reale)");

    QDialog dlg(this);
    dlg.setWindowTitle("Vision3D — guida rapida");
    dlg.resize(dpiScale(560), dpiScale(600));
    auto* dlgLay = new QVBoxLayout(&dlg);

    auto* guide = new QTextBrowser(&dlg);
    guide->setOpenExternalLinks(true);
    guide->setHtml(QString(
        "<h3>1. Collegare i client (iOS, Android, Desktop)</h3>"
        "<ol>"
        "<li>Premi <b>Avvia server</b> (interfaccia LAN 192.168.x consigliata).</li>"
        "<li>Sul client, stessa rete WiFi, apri nel browser: <b>%1</b> — "
        "oppure inquadra il QR qui sotto con la fotocamera del telefono.</li>"
        "<li>Avviso certificato (self-signed, normale): <i>Avanzate → Procedi</i>.</li>"
        "<li><b>iOS</b>: Safari, poi tocca \xf0\x9f\xa7\xad Sensori e concedi il permesso "
        "(obbligatorio da iOS 13).</li>"
        "<li><b>Android</b>: se la bussola resta a 0\xc2\xb0 usa Firefox, oppure dopo gli "
        "scatti premi <i>\xe2\x86\x94 Distribuisci sul cerchio</i> e correggi le pose a mano.</li>"
        "<li><b>Desktop client</b>: qualunque browser del PC — utile con una webcam.</li>"
        "</ol>"
        "<h3>2. Installare OpenCV (box oggetti + scala ArUco)</h3>"
        "<p><code>sudo apt install libopencv-dev libopencv-contrib-dev</code></p>"
        "<p>Senza questi pacchetti la card <i>Box (OpenCV)</i> resta vuota con la scritta "
        "\"Box non disponibili\": sul sistema è spesso già installato solo OpenCV a runtime "
        "(<code>libopencv-core...</code>), ma <code>find_package(OpenCV)</code> in fase di "
        "build richiede anche gli header e i file CMake di <code>libopencv-dev</code>. "
        "<code>libopencv-contrib-dev</code> aggiunge il modulo <code>aruco</code> per la "
        "scala reale automatica (sezione 4). Dopo l'installazione va "
        "<b>ricompilato</b> Prismalux da capo (<code>cmake -B build_gui gui/</code>, non solo "
        "<code>cmake --build</code>): CMake deve rilevare OpenCV che prima mancava.</p>"
        "<h3>3. Installare COLMAP (nuvola di punti)</h3>"
        "<p><code>sudo apt install colmap libposelib</code></p>"
        "<p>Su alcune versioni di Ubuntu il pacchetto <code>colmap</code> non installa da "
        "solo <code>libposelib</code> (errore <i>libPoseLib.so</i>): il comando sopra li "
        "mette entrambi. Dopo l'installazione premi <b>\xe2\x9f\xb3 Ricontrolla</b> nella "
        "sezione Ricostruzione: la verifica avviene a caldo, senza riavviare Prismalux.</p>"
        "<h3>4. Scala reale — bersagli ArUco</h3>"
        "<p>Stampa i bersagli con i pulsanti qui sotto, sempre al 100% / "
        "\"dimensione reale\" (MAI \"adatta alla pagina\"), gira intorno all'oggetto "
        "con buona sovrapposizione, luce uniforme e sfondo fermo. Foto minime per "
        "qualit\xc3\xa0: "
        "<b>low \xe2\x89\xa5 10 &middot; medium \xe2\x89\xa5 20 &middot; high \xe2\x89\xa5 40</b> "
        "(il contatore accanto alla qualit\xc3\xa0 diventa verde quando bastano).</p>"
        "<h3>5. Modello 3D (scanner simulato)</h3>"
        "<p>La ricostruzione produce una <b>nuvola di punti colorata</b>: "
        "<code>modello.obj + modello.mtl</code> (Blender/MeshLab) e "
        "<code>nuvola_punti.ply</code> (CloudCompare). I client iOS/Android/Desktop la "
        "scaricano dai pulsanti \xe2\xac\x87 della card <i>Modello 3D</i>; per una mesh "
        "con superficie: MeshLab \xe2\x86\x92 Poisson.</p>").arg(urlForGuide));
    dlgLay->addWidget(guide, 1);

    /* ── QR per collegarsi dal telefono (solo a server avviato) + apertura
       PDF bersagli per stamparli ── */
    auto* bottomRow = new QHBoxLayout();

    if (hasUrl) {
        auto* qr = new QrCodeWidget(url, &dlg);
        qr->setFixedSize(dpiScale(120), dpiScale(120));
        qr->setToolTip("Inquadra con la fotocamera del telefono per aprire l'URL del server.");
        bottomRow->addWidget(qr);
    } else {
        auto* noQr = new QLabel(tr("Avvia il server\nper generare\nil QR"), &dlg);
        noQr->setFixedSize(dpiScale(120), dpiScale(120));
        noQr->setAlignment(Qt::AlignCenter);
        noQr->setWordWrap(true);
        noQr->setStyleSheet(
            "color: palette(placeholder-text);"
            "border: 1px dashed palette(mid);"
            "border-radius: 6px;");
        bottomRow->addWidget(noQr);
    }

    auto* printCol = new QVBoxLayout();
    printCol->addWidget(new QLabel("\xf0\x9f\x96\xa8 Bersagli ArUco (apri e stampa al 100%):", &dlg));

    auto* printArucoBtn = new QPushButton("Marker singoli (PDF)", &dlg);
    printArucoBtn->setObjectName("actionBtn");
    connect(printArucoBtn, &QPushButton::clicked, &dlg, [] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(P::root() + "/Tools/aruco/aruco_markers_A4.pdf"));
    });
    printCol->addWidget(printArucoBtn);

    auto* printBoardBtn = new QPushButton("Board CharUco (PDF)", &dlg);
    printBoardBtn->setObjectName("actionBtn");
    connect(printBoardBtn, &QPushButton::clicked, &dlg, [] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(P::root() + "/Tools/aruco/charuco_board_A4.pdf"));
    });
    printCol->addWidget(printBoardBtn);
    printCol->addStretch(1);

    bottomRow->addLayout(printCol, 1);
    dlgLay->addLayout(bottomRow);

    auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
    connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::close);
    dlgLay->addWidget(btnBox);

    dlg.exec();
}

void Vision3DWidget::onRefreshIfacesClicked()
{
    if (!m_ifaceCombo) return;
    const QString prev = m_ifaceCombo->currentText();
    m_ifaceCombo->clear();
    m_ifaceCombo->addItems(listLocalIps());
    const int idx = m_ifaceCombo->findText(prev);
    if (idx >= 0) m_ifaceCombo->setCurrentIndex(idx);
}

/* Carica un JPEG da disco in una label-anteprima; testo di ripiego se manca. */
static void loadV3dThumb(QLabel* l, const QString& path, const QString& fallback)
{
    if (!l) return;
    QPixmap px;
    if (QFile::exists(path) && px.load(path)) {
        l->setPixmap(px.scaled(dpiScale(160), dpiScale(160),
                               Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        l->setPixmap(QPixmap());
        l->setText(fallback);
    }
}

/* Riassunto leggibile dei dati salvati nel sidecar (modalità, quota GPS,
   accelerometro) per lo scatto selezionato in galleria — altrimenti
   restano "al buio" nei file .json, mai visibili nell'app. */
static QString v3dSensorInfoHtml(const QJsonObject& o)
{
    QStringList parts;

    const QString mode = o.value("scan_mode").toString();
    if (mode == QLatin1String("scene"))
        parts << QStringLiteral("\xf0\x9f\x8c\x84 Scena");
    else if (mode == QLatin1String("object"))
        parts << QStringLiteral("\xf0\x9f\x8e\xaf Oggetto");

    if (o.contains("altitude_m") && !o.value("altitude_m").isNull()) {
        QString alt = QString("\xe2\x9b\xb0 %1 m").arg(o.value("altitude_m").toDouble(), 0, 'f', 1);
        if (o.contains("altitude_accuracy_m") && !o.value("altitude_accuracy_m").isNull())
            alt += QString(" (\xc2\xb1%1)").arg(o.value("altitude_accuracy_m").toDouble(), 0, 'f', 1);
        parts << alt;
    }

    if (o.contains("accel_gravity") && o.value("accel_gravity").isObject()) {
        const QJsonObject a = o.value("accel_gravity").toObject();
        parts << QString("\xf0\x9f\x93\xa1 accel: %1, %2, %3 m/s\xc2\xb2")
            .arg(a.value("x").toDouble(), 0, 'f', 2)
            .arg(a.value("y").toDouble(), 0, 'f', 2)
            .arg(a.value("z").toDouble(), 0, 'f', 2);
    }

    if (parts.isEmpty())
        return QStringLiteral("<i>Nessun dato sensore extra per questo scatto.</i>");
    return parts.join("&nbsp;&nbsp;|&nbsp;&nbsp;");
}

void Vision3DWidget::onGalleryItemClicked(QListWidgetItem* item)
{
    if (!item) return;
    const QString basePath = item->data(Qt::UserRole).toString();
    const QString desc     = item->data(Qt::UserRole + 1).toString();
    loadV3dThumb(m_lastThumb,  basePath + ".jpg",        QStringLiteral("(file rimosso)"));
    loadV3dThumb(m_lastBoxes,  basePath + "_boxes.jpg",  QStringLiteral("(box non\ncalcolati\n2 click per farlo)"));
    loadV3dThumb(m_lastDepth,  basePath + "_depth.jpg",  QStringLiteral("(depth non\ncalcolata\n2 click per farlo)"));
    loadV3dThumb(m_lastEdges,  basePath + "_edges.jpg",  QStringLiteral("(bordi non\ncalcolati\n2 click per farlo)"));
    loadV3dThumb(m_lastBump,   basePath + "_bump.jpg",   QStringLiteral("(bump non\ncalcolata\n2 click per farlo)"));
    loadV3dThumb(m_lastNormal, basePath + "_normal.jpg", QStringLiteral("(normal non\ncalcolata\n2 click per farlo)"));
    if (m_lastDesc)
        m_lastDesc->setText(QString("[%1] %2").arg(item->text(),
            desc.isEmpty() ? QStringLiteral("(nessuna descrizione)") : desc));
    if (m_scene)
        m_scene->setSelectedKey(basePath);   // evidenzia il cono nella scena 3D

    // abilita la posa manuale con i valori correnti dal sidecar
    m_selectedShotKey = basePath;
    int heading = 0, pitch = 0;
    QFile mf(basePath + ".json");
    if (mf.open(QIODevice::ReadOnly)) {
        const QJsonObject o = QJsonDocument::fromJson(mf.readAll()).object();
        heading = o.value("heading_deg").toInt();
        pitch   = o.value("pitch_deg").toInt();
        if (m_lastSensorInfo) m_lastSensorInfo->setText(v3dSensorInfoHtml(o));
    } else if (m_lastSensorInfo) {
        m_lastSensorInfo->setText(QStringLiteral("<i>Sidecar .json non trovato per questo scatto.</i>"));
    }
    if (m_poseHeadSpin) {
        const QSignalBlocker b(m_poseHeadSpin);
        m_poseHeadSpin->setValue(((heading % 360) + 360) % 360);
        m_poseHeadSpin->setEnabled(true);
    }
    if (m_posePitchSpin) {
        const QSignalBlocker b(m_posePitchSpin);
        m_posePitchSpin->setValue(qBound(-90, pitch, 90));
        m_posePitchSpin->setEnabled(true);
    }
}

/* Persiste la posa nel sidecar .json: heading/pitch aggiornati + pose_manual,
 * così sopravvive al riavvio e resta rieditabile. */
void Vision3DWidget::writePoseSidecar(const QString& base, int heading, int pitch)
{
    QJsonObject o;
    QFile rf(base + ".json");
    if (rf.open(QIODevice::ReadOnly)) {
        o = QJsonDocument::fromJson(rf.readAll()).object();
        rf.close();
    }
    o["heading_deg"] = heading;
    o["pitch_deg"]   = pitch;
    o["pose_manual"] = true;
    QFile wf(base + ".json");
    if (wf.open(QIODevice::WriteOnly))
        wf.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
}

void Vision3DWidget::updateGalleryLabel(const QString& key, int heading)
{
    if (!m_gallery) return;
    for (int i = 0; i < m_gallery->count(); ++i) {
        QListWidgetItem* it = m_gallery->item(i);
        if (it->data(Qt::UserRole).toString() != key) continue;
        it->setText(QString("%1 #%2 %3\xc2\xb0")
                        .arg(it->data(Qt::UserRole + 2).toString())
                        .arg(it->data(Qt::UserRole + 3).toInt(), 3, 10, QChar('0'))
                        .arg(heading));
        return;
    }
}

void Vision3DWidget::onPoseSpinChanged()
{
    if (m_selectedShotKey.isEmpty() || !m_poseHeadSpin || !m_posePitchSpin) return;
    const int heading = m_poseHeadSpin->value();
    const int pitch   = m_posePitchSpin->value();
    writePoseSidecar(m_selectedShotKey, heading, pitch);
    if (m_scene) m_scene->setShotPose(m_selectedShotKey, heading, pitch);
    updateGalleryLabel(m_selectedShotKey, heading);
}

/* Angoli equidistanti per TUTTI gli scatti della sessione: il rimedio rapido
 * quando la bussola ha registrato sempre lo stesso valore. Ogni scatto resta
 * poi correggibile singolarmente con le caselle della posa manuale. */
void Vision3DWidget::onDistributeClicked()
{
    if (!m_gallery || m_gallery->count() == 0) return;
    const int n = m_gallery->count();
    for (int i = 0; i < n; ++i) {
        QListWidgetItem* it = m_gallery->item(i);
        const QString key = it->data(Qt::UserRole).toString();
        const int heading = (i * 360) / n;

        int pitch = 0;                       // preserva l'inclinazione esistente
        QFile mf(key + ".json");
        if (mf.open(QIODevice::ReadOnly))
            pitch = QJsonDocument::fromJson(mf.readAll()).object()
                        .value("pitch_deg").toInt();

        writePoseSidecar(key, heading, pitch);
        if (m_scene) m_scene->setShotPose(key, heading, pitch);
        updateGalleryLabel(key, heading);
    }
    // riallinea le caselle allo scatto selezionato, se ce n'è uno
    if (!m_selectedShotKey.isEmpty() && m_poseHeadSpin) {
        for (int i = 0; i < n; ++i)
            if (m_gallery->item(i)->data(Qt::UserRole).toString() == m_selectedShotKey) {
                const QSignalBlocker b(m_poseHeadSpin);
                m_poseHeadSpin->setValue((i * 360) / n);
                break;
            }
    }
    appendLog(QString("Posa distribuita: %1 scatti a %2\xc2\xb0 di distanza l'uno dall'altro.")
                  .arg(n).arg(360 / n));
}

/* Scansiona scan_output/ e riempie il combo sessioni (più recente prima). */
void Vision3DWidget::populateSessions(const QString& select)
{
    if (!m_sessionCombo) return;
    const QString prev = select.isEmpty() ? m_sessionCombo->currentText() : select;
    const QFileInfoList dirs = QDir(m_outputDir)
        .entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time);
    QStringList names;
    for (const QFileInfo& fi : dirs) names << fi.fileName();

    /* Due combo (Assembla + Preparazione) mostrano lo stesso elenco —
       aggiornati insieme così restano sempre coerenti indipendentemente
       da quale l'utente usa per cambiare sessione. */
    for (QComboBox* combo : {m_sessionCombo, m_prepSessionCombo}) {
        if (!combo) continue;
        const QSignalBlocker block(combo);   // niente reload durante il refill
        combo->clear();
        combo->addItems(names);
        const int idx = combo->findText(prev);
        if (idx >= 0) combo->setCurrentIndex(idx);
    }
}

/* Ricarica galleria + scena 3D leggendo foto e sidecar .json della sessione. */
void Vision3DWidget::loadSessionIntoUi(const QString& session)
{
    if (m_gallery) m_gallery->clear();
    if (m_scene)   m_scene->clearShots();
    m_selectedShotKey.clear();                       // selezione persa col reload
    if (m_poseHeadSpin)  m_poseHeadSpin->setEnabled(false);
    if (m_posePitchSpin) m_posePitchSpin->setEnabled(false);
    if (session.isEmpty() || !m_gallery) return;

    const QDir sess(m_outputDir + "/" + session);
    const QFileInfoList devs = sess.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot,
                                                  QDir::Name);
    for (const QFileInfo& dv : devs) {
        QDir d(dv.absoluteFilePath());
        const QStringList files = d.entryList({"*_a???.jpg"}, QDir::Files, QDir::Name);
        for (const QString& f : files) {
            const QString jpgPath = d.absoluteFilePath(f);
            const QString base = jpgPath.left(jpgPath.size() - 4);   // senza ".jpg"

            int heading = 0, pitch = 0, index = 0;
            bool hasSensors = false;
            QString description;
            QFile mf(base + ".json");
            if (mf.open(QIODevice::ReadOnly)) {
                const QJsonObject o = QJsonDocument::fromJson(mf.readAll()).object();
                heading    = o.value("heading_deg").toInt();
                pitch      = o.value("pitch_deg").toInt();
                index      = o.value("index").toInt();
                // posa impostata a mano dall'utente = affidabile quanto i sensori
                hasSensors = o.value("has_sensors").toBool()
                             || o.value("pose_manual").toBool();
                description = o.value("description").toString();
            }

            /* icona: decodifica direttamente scalata (veloce anche con molte foto) */
            QImageReader rd(jpgPath);
            rd.setAutoTransform(true);
            QSize sz = rd.size();
            if (sz.isValid()) {
                sz.scale(dpiScale(96), dpiScale(96), Qt::KeepAspectRatio);
                rd.setScaledSize(sz);
            }
            const QImage img = rd.read();

            auto* it = new QListWidgetItem(
                QIcon(QPixmap::fromImage(img)),
                QString("%1 #%2 %3\xc2\xb0").arg(dv.fileName())
                    .arg(index, 3, 10, QChar('0')).arg(heading));
            it->setData(Qt::UserRole,     base);
            it->setData(Qt::UserRole + 1, description);  // dal sidecar: VLM o inserita a mano
            it->setData(Qt::UserRole + 2, dv.fileName());
            it->setData(Qt::UserRole + 3, index);
            it->setToolTip(f);
            m_gallery->addItem(it);
            if (m_scene)
                m_scene->addShot({base, dv.fileName(), index, heading, pitch, hasSensors});
        }
    }
    updatePhotoRequirement();
}

void Vision3DWidget::onSessionComboChanged(const QString& session)
{
    if (m_prepSessionCombo) {
        const QSignalBlocker b(m_prepSessionCombo);
        const int idx = m_prepSessionCombo->findText(session);
        if (idx >= 0) m_prepSessionCombo->setCurrentIndex(idx);
    }
    loadSessionIntoUi(session);
    refreshSessionDescriptionTooltip(session);
}

/* Combo Preparazione: rispecchia la scelta su m_sessionCombo, che è
   l'unico a scatenare loadSessionIntoUi() (evita doppio caricamento). */
void Vision3DWidget::onPrepSessionComboChanged(const QString& session)
{
    if (!m_sessionCombo) return;
    const int idx = m_sessionCombo->findText(session);
    if (idx >= 0 && m_sessionCombo->currentIndex() != idx)
        m_sessionCombo->setCurrentIndex(idx);
}

/* Sanifica il nome e crea la cartella vuota della sessione. Ritorna il nome
   sanificato, o stringa vuota se non restava nulla di valido. Se viene data
   una descrizione, la persiste subito (vedi setSessionDescription()). */
QString Vision3DWidget::createSession(const QString& rawName, const QString& description)
{
    QString safe;
    for (QChar c : rawName)
        if (c.isLetterOrNumber() || c == '_' || c == '-') safe += c;
    if (safe.isEmpty()) return QString();

    QDir().mkpath(m_outputDir + "/" + safe);
    if (!description.trimmed().isEmpty())
        setSessionDescription(safe, description.trimmed());
    populateSessions(safe);
    return safe;
}

/* Nota libera sulla sessione (es. "cubo con croce rossa, test point
   cloud") — scan_output/<session>/session.json, un file di metadati a
   parte dai sidecar per-scatto (quelli restano solo dati misurati). */
QString Vision3DWidget::sessionDescription(const QString& session) const
{
    if (session.isEmpty()) return QString();
    QFile f(m_outputDir + "/" + session + "/session.json");
    if (!f.open(QIODevice::ReadOnly)) return QString();
    return QJsonDocument::fromJson(f.readAll()).object().value("description").toString();
}

void Vision3DWidget::setSessionDescription(const QString& session, const QString& description)
{
    if (session.isEmpty()) return;
    const QString path = m_outputDir + "/" + session + "/session.json";
    QJsonObject meta;
    QFile rf(path);
    if (rf.open(QIODevice::ReadOnly)) { meta = QJsonDocument::fromJson(rf.readAll()).object(); rf.close(); }
    meta["description"] = description;
    meta["updated"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    if (!meta.contains("created")) meta["created"] = meta.value("updated");
    QDir().mkpath(m_outputDir + "/" + session);
    QFile wf(path);
    if (wf.open(QIODevice::WriteOnly)) wf.write(QJsonDocument(meta).toJson(QJsonDocument::Indented));
}

/* Crea una sessione vuota col nome scelto (es. notturno, villa,
   oggettistico, natura...) — utile per prepararla PRIMA di andare a
   scattare, invece di scoprire il nome solo scrivendolo sul telefono.
   La descrizione è opzionale: Annulla su quel dialogo salta solo lei,
   non l'intera creazione (che dipende solo dal nome). */
void Vision3DWidget::onNewSessionClicked()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Nuova sessione"),
        tr("Nome sessione (es. notturno, villa, oggettistico, natura...):"),
        QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok || name.isEmpty()) return;

    bool descOk = false;
    const QString description = QInputDialog::getMultiLineText(this, tr("Descrizione (opzionale)"),
        tr("Note su questa sessione — cosa stai scansionando, condizioni di luce, ecc. "
           "(lascia vuoto e premi Annulla per saltare):"),
        QString(), &descOk).trimmed();

    const QString safe = createSession(name, descOk ? description : QString());
    if (safe.isEmpty()) {
        appendLog(tr("Nome sessione non valido: usa lettere, numeri, - o _."));
        return;
    }
    refreshSessionDescriptionTooltip(safe);
    appendLog(tr("Nuova sessione creata: %1 — usala come nome sessione anche sul telefono "
                 "per unire gli scatti.").arg(safe));
}

/* Inserisce/modifica la descrizione della sessione corrente in qualsiasi
   momento (non solo alla creazione) — prefilled col testo già salvato. */
void Vision3DWidget::onEditSessionDescriptionClicked()
{
    const QString session = m_prepSessionCombo ? m_prepSessionCombo->currentText() : QString();
    if (session.isEmpty()) {
        appendLog(tr("Nessuna sessione selezionata."));
        return;
    }
    bool ok = false;
    const QString description = QInputDialog::getMultiLineText(this, tr("Descrizione sessione"),
        tr("Note su \"%1\":").arg(session), sessionDescription(session), &ok).trimmed();
    if (!ok) return;

    setSessionDescription(session, description);
    refreshSessionDescriptionTooltip(session);
    appendLog(tr("Descrizione di \"%1\" aggiornata.").arg(session));
}

/* Mostra la descrizione salvata come tooltip sui due combo sessione (sono
   sincronizzati, ma il tooltip va impostato su entrambi) — evita di dover
   aggiungere un pannello dedicato solo per una nota facoltativa. */
void Vision3DWidget::refreshSessionDescriptionTooltip(const QString& session)
{
    const QString desc = sessionDescription(session);
    const QString suffix = desc.isEmpty() ? QString() : tr("\n\nDescrizione: %1").arg(desc);
    if (m_prepSessionCombo) m_prepSessionCombo->setToolTip(QString(kPrepSessionComboTip) + suffix);
    if (m_sessionCombo)     m_sessionCombo->setToolTip(QString(kSessionComboTip) + suffix);
}

/* Inserisce/modifica a mano la descrizione dello scatto selezionato in
   galleria — sostituisce (o integra, se non richiesta) quella del VLM.
   Prima di questo la descrizione non finiva mai nel sidecar: il telefono
   la vedeva nella risposta HTTP, ma sul PC andava persa. */
void Vision3DWidget::onEditShotDescriptionClicked()
{
    if (m_selectedShotKey.isEmpty()) {
        appendLog(tr("Seleziona prima uno scatto in galleria."));
        return;
    }
    const QString sidecarPath = m_selectedShotKey + ".json";
    QJsonObject meta;
    QFile rf(sidecarPath);
    if (rf.open(QIODevice::ReadOnly)) { meta = QJsonDocument::fromJson(rf.readAll()).object(); rf.close(); }

    bool ok = false;
    const QString description = QInputDialog::getMultiLineText(this, tr("Descrizione scatto"),
        tr("Descrizione di %1 (sostituisce quella eventualmente generata dal VLM):")
            .arg(QFileInfo(m_selectedShotKey).fileName()),
        meta.value("description").toString(), &ok).trimmed();
    if (!ok) return;

    meta["description"] = description;
    QFile wf(sidecarPath);
    if (wf.open(QIODevice::WriteOnly)) wf.write(QJsonDocument(meta).toJson(QJsonDocument::Indented));

    if (m_lastDesc)
        m_lastDesc->setText(description.isEmpty() ? tr("(nessuna descrizione)") : description);
    // aggiorna anche la cache in galleria: riselezionando lo stesso scatto
    // (senza ricaricare l'intera sessione da disco) resta coerente
    if (m_gallery)
        for (int i = 0; i < m_gallery->count(); ++i) {
            QListWidgetItem* it = m_gallery->item(i);
            if (it->data(Qt::UserRole).toString() == m_selectedShotKey)
                it->setData(Qt::UserRole + 1, description);
        }
    appendLog(tr("Descrizione aggiornata per %1.").arg(QFileInfo(m_selectedShotKey).fileName()));
}

/* Copia i file immagine in scan_output/<session>/import/ con la stessa
   numerazione *_a???.jpg degli scatti da telefono (has_sensors=false, così
   galleria/scena 3D li leggono senza alcuna modifica al parsing esistente).
   Ritorna quante immagini sono state importate con successo. */
int Vision3DWidget::importPhotoFiles(const QString& session, const QStringList& files)
{
    if (session.isEmpty() || files.isEmpty()) return 0;

    const QString folder = m_outputDir + "/" + session + "/import";
    QDir().mkpath(folder);
    int n;
    {
        QMutexLocker lock(&m_lock);
        n = int(QDir(folder).entryList({"*_a???.jpg"}, QDir::Files).size());
    }

    int imported = 0;
    for (const QString& srcPath : files) {
        const QImage img(srcPath);
        if (img.isNull()) continue;
        ++n;
        const QString base = QString("%1_import_%2_a000").arg(session).arg(n, 3, 10, QChar('0'));
        if (!img.save(folder + "/" + base + ".jpg", "JPEG", 92)) continue;

        QJsonObject meta;
        meta["file"] = base + ".jpg";
        meta["session"] = session;
        meta["device"] = "import";
        meta["index"] = n;
        meta["heading_deg"] = 0;
        meta["pitch_deg"] = 0;
        meta["roll_deg"] = 0;
        meta["has_sensors"] = false;
        meta["imported"] = true;
        meta["imported_from"] = srcPath;
        meta["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        QFile mf(folder + "/" + base + ".json");
        if (mf.open(QIODevice::WriteOnly))
            mf.write(QJsonDocument(meta).toJson(QJsonDocument::Indented));
        ++imported;
    }
    return imported;
}

/* Importa foto già esistenti su disco nella sessione corrente. Senza dati
   bussola: usa "Distribuisci sul cerchio" per assegnare angoli equidistanti. */
void Vision3DWidget::onImportPhotosClicked()
{
    const QString session = m_sessionCombo ? m_sessionCombo->currentText() : QString();
    if (session.isEmpty()) {
        appendLog(tr("Crea o seleziona prima una sessione."));
        return;
    }
    const QStringList files = QFileDialog::getOpenFileNames(this,
        tr("Importa foto nella sessione \"%1\"").arg(session), QString(),
        tr("Immagini (*.jpg *.jpeg *.png)"));
    if (files.isEmpty()) return;

    const int imported = importPhotoFiles(session, files);
    appendLog(tr("Importate %1/%2 foto in \"%3\" — senza dati bussola: usa "
                 "\"\xe2\x86\x94 Distribuisci sul cerchio\" nella tab Assembla per assegnare angoli.")
                  .arg(imported).arg(files.size()).arg(session));
    loadSessionIntoUi(session);
    updatePhotoRequirement();
}

/* Elimina foto + box + depth + sidecar di uno scatto. */
void Vision3DWidget::deleteShot(const QString& base)
{
    QFile::remove(base + ".jpg");
    QFile::remove(base + "_boxes.jpg");
    QFile::remove(base + "_depth.jpg");
    QFile::remove(base + "_edges.jpg");
    QFile::remove(base + "_bump.jpg");
    QFile::remove(base + "_normal.jpg");
    QFile::remove(base + ".json");
}

/* Elimina gli scatti selezionati in galleria (uno o più — selezione
   multipla con Ctrl/Shift/click o tasto Canc, previa conferma unica). */
void Vision3DWidget::onDeletePhotoClicked()
{
    if (!m_gallery) return;
    const QList<QListWidgetItem*> items = m_gallery->selectedItems();
    if (items.isEmpty()) {
        appendLog(tr("Seleziona prima una o più foto in galleria da eliminare."));
        return;
    }

    const QString msg = items.size() == 1
        ? tr("Eliminare definitivamente questo scatto (foto, box, depth e dati sensore)?")
        : tr("Eliminare definitivamente questi %1 scatti (foto, box, depth e dati sensore)?")
              .arg(items.size());
    // "Sì" come default (non "No"): chi arriva qui ha già scelto di
    // eliminare premendo il pulsante/Canc, il dialogo è solo la conferma
    // finale — con Invio si conclude subito senza dover puntare il mouse.
    const auto ret = QMessageBox::question(this, tr("Elimina foto"), msg,
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (ret != QMessageBox::Yes) return;

    int deleted = 0;
    for (QListWidgetItem* item : items) {
        const QString base = item->data(Qt::UserRole).toString();
        if (base.isEmpty()) continue;
        deleteShot(base);
        ++deleted;
    }
    appendLog(deleted == 1
        ? tr("1 scatto eliminato.")
        : tr("%1 scatti eliminati.").arg(deleted));

    const QString session = m_sessionCombo ? m_sessionCombo->currentText() : QString();
    if (!session.isEmpty()) loadSessionIntoUi(session);
    updatePhotoRequirement();
}

/* Elimina TUTTI gli scatti della sessione corrente (tutti i device),
   previa conferma con il conteggio esatto. */
void Vision3DWidget::onDeleteAllPhotosClicked()
{
    if (!m_gallery) return;
    const int total = m_gallery->count();
    if (total == 0) {
        appendLog(tr("Nessuna foto da eliminare in questa sessione."));
        return;
    }

    const auto ret = QMessageBox::question(this, tr("Elimina tutte le foto"),
        tr("Eliminare definitivamente TUTTE le %1 foto di questa sessione "
           "(foto, box, depth e dati sensore)? Operazione non annullabile.").arg(total),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (ret != QMessageBox::Yes) return;

    for (int i = 0; i < m_gallery->count(); ++i) {
        const QString base = m_gallery->item(i)->data(Qt::UserRole).toString();
        if (!base.isEmpty()) deleteShot(base);
    }
    appendLog(tr("Eliminate tutte le %1 foto della sessione.").arg(total));

    const QString session = m_sessionCombo ? m_sessionCombo->currentText() : QString();
    if (!session.isEmpty()) loadSessionIntoUi(session);
    updatePhotoRequirement();
}

/* Tasto destro su una delle miniature di "Ultimo scatto analizzato": salva
   quella singola immagine (originale, box, depth, bordi, bump o normal). */
void Vision3DWidget::onThumbContextMenu(const QPoint& pos)
{
    auto* label = qobject_cast<QLabel*>(sender());
    if (!label || label->pixmap().isNull()) return;

    QMenu menu(label);
    QAction* saveAct = menu.addAction(tr("\xF0\x9F\x92\xBE Salva questa immagine..."));
    QAction* chosen = menu.exec(label->mapToGlobal(pos));
    if (chosen != saveAct) return;

    const QString path = QFileDialog::getSaveFileName(this, tr("Salva immagine"),
        QDir::homePath() + "/scatto.jpg", tr("Immagine JPEG (*.jpg)"));
    if (path.isEmpty()) return;
    if (!label->pixmap().save(path, "JPEG", 92))
        appendLog(tr("Salvataggio immagine fallito: %1").arg(path));
    else
        appendLog(tr("Immagine salvata in %1").arg(path));
}

/* Copia tutte le mappe già salvate su disco per lo scatto selezionato
   (originale + box/depth/bordi/bump/normal, quelle che esistono) in una
   cartella scelta dall'utente. Non ricalcola nulla: analyze() le ha già
   scritte accanto alla foto durante lo scatto. */
void Vision3DWidget::onSaveAllMapsClicked()
{
    if (m_selectedShotKey.isEmpty()) {
        appendLog(tr("Seleziona prima uno scatto in galleria."));
        return;
    }
    const QString destDir = QFileDialog::getExistingDirectory(this,
        tr("Salva tutte le mappe in..."), QDir::homePath());
    if (destDir.isEmpty()) return;

    static const struct { const char* suffix; const char* label; } kMaps[] = {
        {".jpg",        "originale"},
        {"_boxes.jpg",  "box"},
        {"_depth.jpg",  "depth"},
        {"_edges.jpg",  "bordi"},
        {"_bump.jpg",   "bump"},
        {"_normal.jpg", "normal"},
    };
    const QString baseName = QFileInfo(m_selectedShotKey).fileName();
    int saved = 0;
    for (const auto& m : kMaps) {
        const QString src = m_selectedShotKey + m.suffix;
        if (!QFile::exists(src)) continue;
        const QString dst = destDir + "/" + baseName + "_" + QString::fromLatin1(m.label) + ".jpg";
        if (QFile::exists(dst)) QFile::remove(dst);
        if (QFile::copy(src, dst)) ++saved;
    }
    appendLog(tr("Salvate %1 mappe di \"%2\" in %3").arg(saved).arg(baseName, destDir));
}

/* Tasto destro su una riga di "Device attivi": elimina quel device e TUTTE
   le sue foto (la cartella scan_output/<sessione>/<deviceId>/ intera) —
   utile per pulire un telefono di test o riconnesso con un nuovo ID senza
   dover cancellare foto una per una in galleria. */
void Vision3DWidget::onDeviceTableContextMenu(const QPoint& pos)
{
    if (!m_deviceTable) return;
    const QTableWidgetItem* hit = m_deviceTable->itemAt(pos);
    if (!hit) return;
    const QTableWidgetItem* idItem = m_deviceTable->item(hit->row(), 0);
    if (!idItem) return;
    const QString deviceId = idItem->text();
    if (deviceId.isEmpty()) return;

    QMenu menu(m_deviceTable);
    QAction* delAct = menu.addAction(tr("\xF0\x9F\x97\x91 Elimina device \"%1\" (e le sue foto)").arg(deviceId));
    QAction* chosen = menu.exec(m_deviceTable->viewport()->mapToGlobal(pos));
    if (chosen != delAct) return;

    const auto ret = QMessageBox::question(this, tr("Elimina device"),
        tr("Eliminare definitivamente il device \"%1\" e TUTTE le sue foto "
           "(foto, box, depth, bordi, bump, normal e dati sensore)? "
           "Operazione non annullabile.").arg(deviceId),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (ret != QMessageBox::Yes) return;

    const QString session = m_sessionCombo ? m_sessionCombo->currentText() : QString();
    if (!session.isEmpty()) {
        QDir devDir(m_outputDir + "/" + session + "/" + deviceId);
        if (devDir.exists()) devDir.removeRecursively();
    }
    m_devices.remove(deviceId);
    refreshDeviceTable();
    appendLog(tr("Device \"%1\" eliminato.").arg(deviceId));

    if (!session.isEmpty()) loadSessionIntoUi(session);
    updatePhotoRequirement();
}

/* Tasto destro in galleria: "Salva sul PC" copia la foto originale di ogni
   scatto selezionato (uno o più — Ctrl/Shift+click, come "Elimina
   selezionate") in una cartella a scelta. Click destro su un elemento FUORI
   dalla selezione corrente si comporta come un file manager: seleziona solo
   quello invece di operare sulla selezione multipla precedente. */
void Vision3DWidget::onGalleryContextMenu(const QPoint& pos)
{
    if (!m_gallery) return;
    QListWidgetItem* hit = m_gallery->itemAt(pos);
    if (!hit) return;
    if (!hit->isSelected()) {
        m_gallery->clearSelection();
        hit->setSelected(true);
    }
    const QList<QListWidgetItem*> items = m_gallery->selectedItems();
    if (items.isEmpty()) return;

    QMenu menu(m_gallery);
    QAction* saveAct = menu.addAction(items.size() == 1
        ? tr("\xF0\x9F\x92\xBE Salva sul PC")
        : tr("\xF0\x9F\x92\xBE Salva sul PC (%1 foto)").arg(items.size()));
    QAction* chosen = menu.exec(m_gallery->viewport()->mapToGlobal(pos));
    if (chosen != saveAct) return;

    const QString destDir = QFileDialog::getExistingDirectory(this,
        tr("Salva foto selezionate in..."), QDir::homePath());
    if (destDir.isEmpty()) return;

    int saved = 0;
    for (QListWidgetItem* item : items) {
        const QString base = item->data(Qt::UserRole).toString();
        if (base.isEmpty()) continue;
        const QString src = base + ".jpg";
        if (!QFile::exists(src)) continue;

        const QString stem = QFileInfo(base).fileName();
        QString dst = destDir + "/" + stem + ".jpg";
        int n = 1;
        while (QFile::exists(dst))
            dst = destDir + "/" + stem + QString("_%1.jpg").arg(n++);
        if (QFile::copy(src, dst)) ++saved;
    }
    appendLog(tr("Salvate %1 foto in %2").arg(saved).arg(destDir));
}

/* Doppio click su QLabel non ha un segnale nativo: intercettato qui via
   eventFilter (installato in buildUi() sulle 5 miniature di analisi, non
   su "Originale" che non ha nulla da ricalcolare). */
bool Vision3DWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonDblClick) {
        if      (watched == m_lastBoxes)  { recomputeThumb(m_lastBoxes,  "_boxes.jpg",  tr("Box"));    return true; }
        else if (watched == m_lastDepth)  { recomputeThumb(m_lastDepth,  "_depth.jpg",  tr("Depth"));  return true; }
        else if (watched == m_lastEdges)  { recomputeThumb(m_lastEdges,  "_edges.jpg",  tr("Bordi"));  return true; }
        else if (watched == m_lastBump)   { recomputeThumb(m_lastBump,   "_bump.jpg",   tr("Bump"));   return true; }
        else if (watched == m_lastNormal) { recomputeThumb(m_lastNormal, "_normal.jpg", tr("Normal")); return true; }
    }
    return QWidget::eventFilter(watched, event);
}

/* Calcola SUBITO una sola mappa (es. "bordi non calcolati" → doppio click)
   per lo scatto attualmente selezionato, la salva su disco accanto alla
   foto (stesso nome usato da analyze()) e aggiorna solo quella miniatura —
   utile perché bordi/bump/normal sono chip OFF di default sul telefono, e
   box/depth potrebbero non essere state richieste per quello scatto:
   niente bisogno di rifare la foto per riavere quella mappa. */
void Vision3DWidget::recomputeThumb(QLabel* thumb, const QString& suffix, const QString& label)
{
    if (!thumb || m_selectedShotKey.isEmpty()) {
        appendLog(tr("Seleziona prima uno scatto in galleria."));
        return;
    }
    QFile jf(m_selectedShotKey + ".jpg");
    if (!jf.open(QIODevice::ReadOnly)) {
        appendLog(tr("Impossibile ricalcolare %1: foto originale non trovata.").arg(label));
        return;
    }
    const QByteArray jpeg = jf.readAll();
    jf.close();

    appendLog(tr("Ricalcolo %1 per %2\xe2\x80\xa6")
                  .arg(label, QFileInfo(m_selectedShotKey).fileName()));
    QByteArray out;
    if      (suffix == QLatin1String("_boxes.jpg"))  { int n = 0; out = detectBoxes(jpeg, n); }
    else if (suffix == QLatin1String("_depth.jpg"))  out = depthMap(jpeg);
    else if (suffix == QLatin1String("_edges.jpg"))  out = edgeMap(jpeg);
    else if (suffix == QLatin1String("_bump.jpg"))   out = bumpMap(jpeg);
    else if (suffix == QLatin1String("_normal.jpg")) out = normalMap(jpeg);

    if (out.isEmpty()) {
        appendLog(tr("%1 non calcolabile per questo scatto (serve OpenCV, o lo script depth).").arg(label));
        return;
    }
    QFile of(m_selectedShotKey + suffix);
    if (of.open(QIODevice::WriteOnly)) { of.write(out); of.close(); }
    loadV3dThumb(thumb, m_selectedShotKey + suffix, tr("(non disponibile)"));
    appendLog(tr("%1 calcolato.").arg(label));
}

void Vision3DWidget::onVlmModelChanged(const QString& model)
{
    const QString m = model.trimmed();
    if (m.isEmpty()) return;
    m_vlmModel = m;
    QSettings("Prismalux", "GUI").setValue(P::SK::kVision3dVlmModel, m);
}

/* Popola il combo VLM con i modelli installati in Ollama (asincrono). */
void Vision3DWidget::fetchVlmModels()
{
    if (m_tagsReply) return;   // fetch già in corso
    m_tagsReply = m_net->get(QNetworkRequest(QUrl(m_ollamaUrl + "/api/tags")));
    connect(m_tagsReply, &QNetworkReply::finished, this, &Vision3DWidget::onVlmTagsReady);
}

/* Nomi che di solito indicano un modello Ollama capace di vedere immagini
   (usato sia per proporre un default sia per ordinare/segnalare il combo).
   Delega alla lista canonica P::isVisionModel() (prismalux_paths.h) invece
   di una copia locale più corta — prima questo file, main_graph.cpp e
   prismalux_paths.h avevano 3 elenchi leggermente diversi, con lo stesso
   modello classificato "vision" in un punto della UI e non in un altro. */
static bool v3dLooksLikeVisionModel(const QString& name)
{
    return P::isVisionModel(name);
}

void Vision3DWidget::onVlmTagsReady()
{
    QNetworkReply* reply = m_tagsReply;
    m_tagsReply = nullptr;
    if (!reply) return;
    reply->deleteLater();
    if (!m_vlmCombo) return;
    if (reply->error() != QNetworkReply::NoError) {
        /* Ollama non raggiungibile: il combo resta con l'unica voce scritta
           a mano, ma senza questo messaggio sembra semplicemente "vuoto"
           invece che "Ollama spento" — un dubbio legittimo se il VLM
           funzioni davvero. */
        appendLog(tr("\xe2\x9a\xa0 Elenco modelli VLM non disponibile (%1) — "
                     "verifica che Ollama sia avviato su %2.")
                      .arg(reply->errorString(), m_ollamaUrl));
        return;
    }

    const QJsonArray models = QJsonDocument::fromJson(reply->readAll())
                                  .object().value("models").toArray();
    QStringList names;
    for (const auto& m : models) {
        const QString n = m.toObject().value("name").toString();
        if (!n.isEmpty()) names.append(n);
    }
    if (names.isEmpty()) {
        appendLog(tr("\xe2\x9a\xa0 Nessun modello installato in Ollama — "
                     "scarica un modello vision, es. \"ollama pull moondream\"."));
        return;
    }

    /* modelli vision prima nell'elenco (più facili da trovare/riconoscere),
       ordine relativo altrimenti invariato all'interno di ogni gruppo */
    QStringList visionNames, textNames;
    for (const QString& n : names)
        (v3dLooksLikeVisionModel(n) ? visionNames : textNames).append(n);
    const QStringList sorted = visionNames + textNames;

    /* mantieni la scelta corrente se ancora installata; altrimenti proponi
       il primo modello vision riconosciuto */
    const QString cur = m_vlmModel;
    m_vlmCombo->blockSignals(true);
    m_vlmCombo->clear();
    m_vlmCombo->addItems(sorted);
    for (int i = 0; i < sorted.size(); ++i)
        m_vlmCombo->setItemData(i,
            v3dLooksLikeVisionModel(sorted[i])
                ? tr("\xf0\x9f\x91\x81 Vision \xe2\x80\x94 legge davvero l'immagine")
                : tr("\xe2\x9a\xa0 Solo testo \xe2\x80\x94 risponde ma IGNORA l'immagine"),
            Qt::ToolTipRole);

    int idx = m_vlmCombo->findText(cur);
    if (idx < 0 && !visionNames.isEmpty())
        idx = m_vlmCombo->findText(visionNames.first());
    if (idx >= 0) m_vlmCombo->setCurrentIndex(idx);
    else          m_vlmCombo->setEditText(cur);
    m_vlmCombo->blockSignals(false);
    onVlmModelChanged(m_vlmCombo->currentText());

    if (visionNames.isEmpty())
        appendLog(tr("\xe2\x9a\xa0 Nessun modello vision riconosciuto tra quelli installati "
                     "(solo modelli di testo) — la descrizione VLM user\xc3\xa0 il "
                     "modello scelto ma ignorer\xc3\xa0 l'immagine. Scarica un modello "
                     "vision, es. \"ollama pull moondream\"."));
}

// ---------------------------------------------------------------------------
void Vision3DWidget::onNewConnection()
{
#if QT_CONFIG(ssl)
    while (m_server && m_server->hasPendingConnections()) {
        QTcpSocket* sock = m_server->nextPendingConnection();
        connect(sock, &QTcpSocket::readyRead,     this, &Vision3DWidget::onReadyRead);
        connect(sock, &QTcpSocket::disconnected,  this, &Vision3DWidget::onSocketDisconnected);
    }
#endif
}

void Vision3DWidget::onSocketDisconnected()
{
    QObject* sock = sender();
    if (!sock) return;
    m_rxBuffers.remove(sock);
    sock->deleteLater();
}

void Vision3DWidget::onReadyRead()
{
#if QT_CONFIG(ssl)
    auto* sock = qobject_cast<QSslSocket*>(sender());
    if (!sock) return;

    QByteArray& buf = m_rxBuffers[sock];   // buffer per-connessione (no static)
    buf += sock->readAll();
    if (buf.size() > kMaxRequestBytes) {   // anti-DoS: richiesta abnorme
        m_rxBuffers.remove(sock);
        sock->close();
        return;
    }

    const int headerEnd = buf.indexOf("\r\n\r\n");
    if (headerEnd < 0) return;

    const QByteArray header = buf.left(headerEnd);
    const QList<QByteArray> lines = header.split('\n');
    if (lines.isEmpty()) return;
    const QList<QByteArray> reqline = lines.first().trimmed().split(' ');
    if (reqline.size() < 2) { sock->close(); return; }

    ParsedRequest req;
    req.method = reqline[0];
    req.path   = reqline[1];

    int contentLength = 0;
    for (const QByteArray& l : lines) {
        const QByteArray low = l.toLower();
        if (low.startsWith("content-length:"))
            contentLength = low.mid(15).trimmed().toInt();
        else if (low.startsWith("cookie:")) {
            // cerca deviceId=... tra i cookie
            const QByteArray cookies = l.mid(l.indexOf(':') + 1).trimmed();
            const QList<QByteArray> parts = cookies.split(';');
            for (const QByteArray& kv : parts) {
                const QByteArray t = kv.trimmed();
                if (t.startsWith("deviceId="))
                    req.cookieDeviceId = t.mid(9);
            }
        }
        else if (low.startsWith("user-agent:"))
            req.userAgent = l.mid(l.indexOf(':') + 1).trimmed();
    }
    if (contentLength < 0 || contentLength > kMaxRequestBytes) { sock->close(); return; }

    const QByteArray body = buf.mid(headerEnd + 4);
    if (body.size() < contentLength) return;   // body incompleto: aspetta
    req.body = body.left(contentLength);

    handleRequest(sock, req);
#endif
}

// ---------------------------------------------------------------------------
QString Vision3DWidget::assignDeviceId(const QByteArray& existingCookie, const QByteArray& ua)
{
    QMutexLocker lock(&m_lock);
    // Riusa il cookie SOLO se ha la forma esatta "tel<cifre>" (max 8 char):
    // un valore arbitrario finirebbe in un path su disco (path traversal).
    QString id = QString::fromUtf8(existingCookie).trimmed();
    bool valid = id.startsWith(QLatin1String("tel")) && id.size() > 3 && id.size() <= 8;
    for (int i = 3; valid && i < id.size(); ++i)
        if (!id[i].isDigit()) valid = false;
    if (!valid) {
        m_deviceCounter++;
        id = QString("tel%1").arg(m_deviceCounter, 2, 10, QChar('0'));
    }
    Vision3DDevice& di = m_devices[id];
    di.id = id;
    di.lastSeen = QDateTime::currentDateTime();
    if (!ua.isEmpty()) di.userAgent = QString::fromUtf8(ua).left(60);
    return id;
}

void Vision3DWidget::refreshDeviceTable()
{
    if (!m_deviceTable) return;
    QMutexLocker lock(&m_lock);
    m_deviceTable->setRowCount(int(m_devices.size()));
    int row = 0;
    for (auto it = m_devices.constBegin(); it != m_devices.constEnd(); ++it, ++row) {
        const Vision3DDevice& d = it.value();
        m_deviceTable->setItem(row, 0, new QTableWidgetItem(d.id));
        m_deviceTable->setItem(row, 1, new QTableWidgetItem(QString::number(d.photoCount)));
        m_deviceTable->setItem(row, 2, new QTableWidgetItem(d.lastSeen.toString("HH:mm:ss")));
        m_deviceTable->setItem(row, 3, new QTableWidgetItem(d.userAgent));
    }
}

// ---------------------------------------------------------------------------
#if QT_CONFIG(ssl)
void Vision3DWidget::handleRequest(QSslSocket* sock, const ParsedRequest& req)
{
    if (req.method == "GET" && (req.path == "/" || req.path.startsWith("/?")
                                 || req.path.startsWith("/index"))) {
        /* Token nel QR (vedi start()): senza, chiunque sulla stessa LAN
           poteva caricare la pagina e usare il server come un dispositivo
           abbinato. m_token vuoto = server non avviato correttamente →
           nega comunque (fail-closed), mai fail-open. */
        if (m_token.isEmpty() || !LanServer::timingSafeEqual(queryParam(req.path, "token"), m_token)) {
            send401(sock); return;
        }
        // assegna/riusa deviceId e lo imposta come cookie persistente
        const QString id = assignDeviceId(req.cookieDeviceId, req.userAgent);
        emit deviceSeen(id);
        refreshDeviceTable();
        const QByteArray setCookie =
            "Set-Cookie: deviceId=" + id.toUtf8() + "; Max-Age=31536000; Path=/; SameSite=Lax\r\n";
        sendHtml(sock, htmlPage(m_token), setCookie);
        return;
    }

    if (req.method == "POST" && req.path == "/upload") {
        const QJsonDocument doc = QJsonDocument::fromJson(req.body);
        const QJsonObject o = doc.object();
        /* Token nel body JSON (la pagina lo include in ogni fetch, vedi
           htmlPage()) — un GET valido sulla pagina non implica che ogni
           upload successivo sia autorizzato se il token non viaggia con
           esso, quindi ricontrolliamo qui indipendentemente. */
        if (m_token.isEmpty() || !LanServer::timingSafeEqual(o.value("token").toString(), m_token)) {
            send401(sock); return;
        }
        const QString deviceId = assignDeviceId(req.cookieDeviceId, req.userAgent);
        const QString session = o.value("session").toString("scan1");
        const int angle = o.value("angle").toInt(0);
        const int pitch = o.value("pitch").toInt(0);
        const int roll  = o.value("roll").toInt(0);
        const bool hasSensors = o.value("hasSensors").toBool(false);
        const QString scanMode = o.value("mode").toString("object");   // "object" | "scene"
        const QJsonArray wantsArr = o.value("wants").toArray();
        Vision3DWants wants;
        for (const auto& w : wantsArr) {
            const QString s = w.toString();
            if      (s=="desc")   wants.desc   = true;
            else if (s=="boxes")  wants.boxes  = true;
            else if (s=="depth")  wants.depth  = true;
            else if (s=="edges")  wants.edges  = true;
            else if (s=="bump")   wants.bump   = true;
            else if (s=="normal") wants.normal = true;
        }
        const QString dataUrl = o.value("image").toString();
        const int comma = dataUrl.indexOf(',');
        const QByteArray jpeg = QByteArray::fromBase64(
            (comma>=0 ? dataUrl.mid(comma+1) : dataUrl).toUtf8());

        /* Sensori extra dal telefono (best-effort: chiavi assenti se il
           browser/permesso non li fornisce) — copiati nel sidecar .json
           senza reinterpretarli, vedi analyze(). Include anche il test
           flash (riflessi/trasparenza), stessa idea: dato grezzo passato
           così com'è. */
        QJsonObject extraSensors;
        static const char* kExtraKeys[] = {
            "accel", "accelGravity", "rotationRate",
            "altitude", "altitudeAccuracy", "latitude", "longitude",
            "flash", "motionStable", "motionMagnitude"
        };
        static const char* kExtraJsonKeys[] = {
            "accel", "accel_gravity", "rotation_rate",
            "altitude_m", "altitude_accuracy_m", "latitude", "longitude",
            "flash_on", "motion_stable", "motion_magnitude"
        };
        for (size_t i = 0; i < sizeof(kExtraKeys) / sizeof(kExtraKeys[0]); ++i)
            if (o.contains(kExtraKeys[i]))
                extraSensors[kExtraJsonKeys[i]] = o.value(kExtraKeys[i]);

        Vision3DResult r = analyze(session, deviceId, angle, pitch, roll, hasSensors,
                                   jpeg, wants, scanMode, extraSensors);

        QJsonObject out;
        out["ok"] = r.error.isEmpty();
        out["index"] = r.index;
        out["device"] = r.deviceId;
        out["aruco_markers"] = r.arucoMarkersFound;
        out["scale_mm_per_unit"] = r.scaleMmPerUnit;
        if (extraSensors.value("flash_on").toBool())
            out["flash_used"] = true;
        if (!r.error.isEmpty()) out["error"] = r.error;
        if (!r.description.isEmpty()) out["description"] = r.description;
        if (wants.boxes) {
            out["boxes_count"] = r.boxesCount;
            if (!r.boxesJpeg.isEmpty())
                out["boxes_image"] = "data:image/jpeg;base64," + QString::fromUtf8(r.boxesJpeg.toBase64());
        }
        if (wants.depth) {
            if (!r.depthJpeg.isEmpty())
                out["depth_image"] = "data:image/jpeg;base64," + QString::fromUtf8(r.depthJpeg.toBase64());
            else out["depth_unavailable"] = true;
        }
        if (wants.edges) {
            if (!r.edgesJpeg.isEmpty())
                out["edges_image"] = "data:image/jpeg;base64," + QString::fromUtf8(r.edgesJpeg.toBase64());
            else out["edges_unavailable"] = true;
        }
        if (wants.bump) {
            if (!r.bumpJpeg.isEmpty())
                out["bump_image"] = "data:image/jpeg;base64," + QString::fromUtf8(r.bumpJpeg.toBase64());
            else out["bump_unavailable"] = true;
        }
        if (wants.normal) {
            if (!r.normalJpeg.isEmpty())
                out["normal_image"] = "data:image/jpeg;base64," + QString::fromUtf8(r.normalJpeg.toBase64());
            else out["normal_unavailable"] = true;
        }
        const QByteArray setCookie =
            "Set-Cookie: deviceId=" + deviceId.toUtf8() + "; Max-Age=31536000; Path=/; SameSite=Lax\r\n";
        sendJson(sock, QJsonDocument(out).toJson(QJsonDocument::Compact), setCookie);
        emit analysisReady(r);
        refreshDeviceTable();
        return;
    }

    if (req.method == "GET" && req.path.startsWith("/download")) {
        /* Prima chiunque sulla LAN poteva scaricare la ricostruzione 3D di
           un'altra sessione indovinando un nome banale (default "scan1"):
           nessuna autenticazione su questo endpoint. */
        if (m_token.isEmpty() || !LanServer::timingSafeEqual(queryParam(req.path, "token"), m_token)) {
            send401(sock); return;
        }
        // /download?session=<nome>&file=obj|mtl|ply&token=... — scarica il modello sul client
        const QString q = QString::fromUtf8(req.path);
        QString session, fileKey;
        const int qm = q.indexOf('?');
        if (qm >= 0) {
            const QStringList parts = q.mid(qm + 1).split('&');
            for (const QString& kv : parts) {
                if (kv.startsWith(QLatin1String("session="))) session = kv.mid(8);
                else if (kv.startsWith(QLatin1String("file="))) fileKey = kv.mid(5);
            }
        }
        QString safe;                       // stessa sanificazione di analyze()
        for (QChar c : session)
            if (c.isLetterOrNumber() || c == '_' || c == '-') safe += c;
        QString fname;
        if      (fileKey == QLatin1String("obj")) fname = QStringLiteral("modello.obj");
        else if (fileKey == QLatin1String("mtl")) fname = QStringLiteral("modello.mtl");
        else if (fileKey == QLatin1String("ply")) fname = QStringLiteral("nuvola_punti.ply");
        if (safe.isEmpty() || fname.isEmpty()) { send404(sock); return; }

        const QString path = m_outputDir + "/" + safe + "/" + fname;
        if (!QFile::exists(path)) {
            sendHtml(sock, "<html><body style='font-family:sans-serif;padding:2em;'>"
                           "<h3>Modello non ancora pronto</h3><p>Avvia la ricostruzione "
                           "dal PC (\xf0\x9f\xa7\x8a Crea nuvola di punti) e riprova.</p>"
                           "</body></html>");
            return;
        }
        sendFile(sock, path, fname);
        return;
    }

    send404(sock);
}

void Vision3DWidget::sendFile(QSslSocket* sock, const QString& path, const QString& downloadName)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) { send404(sock); return; }
    const QByteArray data = f.readAll();
    QByteArray resp = "HTTP/1.1 200 OK\r\n"
                      "Content-Type: application/octet-stream\r\n"
                      "Content-Disposition: attachment; filename=\"" + downloadName.toUtf8() + "\"\r\n"
                      "Content-Length: " + QByteArray::number(data.size()) + "\r\n"
                      "Connection: close\r\n\r\n" + data;
    sock->write(resp); sock->flush(); sock->disconnectFromHost();
}
#endif // QT_CONFIG(ssl)

// ---------------------------------------------------------------------------
Vision3DResult Vision3DWidget::analyze(const QString& session, const QString& deviceId,
                                       int angle, int pitch, int roll, bool hasSensors,
                                       const QByteArray& jpeg, const Vision3DWants& wants,
                                       const QString& scanMode,
                                       const QJsonObject& extraSensors)
{
    Vision3DResult r;
    r.session = session; r.deviceId = deviceId;
    r.angle = angle; r.pitch = pitch; r.roll = roll; r.hasSensors = hasSensors;

    QString safe; for (QChar c : session)
        if (c.isLetterOrNumber() || c=='_' || c=='-') safe += c;
    if (safe.isEmpty()) safe = "scan1";

    // sottocartella per device: scan_output/<sessione>/<deviceId>/
    const QString folder = m_outputDir + "/" + safe + "/" + deviceId;
    QDir().mkpath(folder);

    // numerazione protetta da lock: due telefoni non collidono
    int n;
    {
        QMutexLocker lock(&m_lock);
        QDir d(folder);
        /* pattern a 3 cifre esatte: NON deve contare _boxes.jpg/_depth.jpg,
           altrimenti l'indice salta e la numerazione si buca */
        n = int(d.entryList({"*_a???.jpg"}, QDir::Files).size()) + 1;
    }
    r.index = n;

    // nome file con ID device incluso
    const QString base = QString("%1_%2_%3_a%4")
        .arg(safe).arg(deviceId)
        .arg(n, 3, 10, QChar('0')).arg(angle, 3, 10, QChar('0'));
    r.savedPath = folder;

    QFile f(folder + "/" + base + ".jpg");
    if (f.open(QIODevice::WriteOnly)) { f.write(jpeg); f.close(); }

    // scala reale via ArUco (veloce; indipendente dalle chip di analisi)
    r.scaleMmPerUnit = detectAruco(jpeg, r.arucoMarkersFound);

    // sidecar .json con i metadati dei sensori (per allineare/ordinare gli scatti)
    {
        QJsonObject meta;
        meta["file"] = base + ".jpg";
        meta["session"] = safe;
        meta["device"] = deviceId;
        meta["index"] = n;
        meta["heading_deg"] = angle;      // bussola: 0-359
        meta["pitch_deg"] = pitch;        // inclinazione avanti/indietro
        meta["roll_deg"] = roll;          // inclinazione laterale
        meta["has_sensors"] = hasSensors; // false = valori non affidabili
        meta["scan_mode"] = (scanMode == QLatin1String("scene")) ? QStringLiteral("scene")
                                                                  : QStringLiteral("object");
        /* Dati sensori extra dal telefono, copiati così come arrivano:
           accel/accel_gravity/rotation_rate (DeviceMotion, accelerometro e
           giroscopio grezzi) + altitude_m/altitude_accuracy_m/latitude/
           longitude (Geolocation GPS — "quanti metri sei rispetto al livello
           del mare", non al suolo locale: il browser non ha un sensore di
           altezza-da-terra) + motion_stable/motion_magnitude (calcolati in
           JS da onMotion(): false = movimento sopra soglia al momento dello
           scatto, foto potenzialmente sfocata — solo un avviso, non blocca
           mai lo scatto). Chiave assente = quel sensore non ha ancora dato
           un valore per questo scatto. */
        for (auto it = extraSensors.constBegin(); it != extraSensors.constEnd(); ++it)
            meta[it.key()] = it.value();
        meta["aruco_markers"] = r.arucoMarkersFound;
        meta["scale_mm_per_unit"] = r.scaleMmPerUnit;  // 0 = nessun marker
        meta["aruco_marker_mm"] = m_arucoMarkerMm;     // lato reale impostato
        meta["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        QFile mf(folder + "/" + base + ".json");
        if (mf.open(QIODevice::WriteOnly))
            { mf.write(QJsonDocument(meta).toJson(QJsonDocument::Indented)); mf.close(); }
    }

    // aggiorna conteggio device
    {
        QMutexLocker lock(&m_lock);
        m_devices[deviceId].photoCount++;
        m_devices[deviceId].lastSeen = QDateTime::currentDateTime();
    }

    appendLog(QString("[%1] ricevuta %2.jpg (%3 KB)%4%5")
              .arg(deviceId, base).arg(jpeg.size()/1024)
              .arg(hasSensors ? QString(" — heading=%1° pitch=%2°").arg(angle).arg(pitch)
                              : QString(" — sensori off"))
              .arg(r.arucoMarkersFound > 0
                   ? QString(" — ArUco×%1 scala=%2mm/px")
                        .arg(r.arucoMarkersFound).arg(r.scaleMmPerUnit, 0, 'f', 4)
                   : QString()));
    emit photoReceived(n, safe, deviceId);

    if (wants.desc) {
        appendLog(QString("[%1]  → VLM…").arg(deviceId));
        r.description = vlmDescribe(jpeg);
        // il sidecar è già su disco (scritto sopra prima di sapere la
        // descrizione, che richiede la chiamata VLM): la aggiunge qui con
        // un read-modify-write, altrimenti si perde non appena la risposta
        // HTTP raggiunge il telefono — prima di questo fix non veniva mai
        // salvata da nessuna parte sul PC.
        if (!r.description.isEmpty()) {
            const QString sidecarPath = folder + "/" + base + ".json";
            QJsonObject meta;
            QFile rf(sidecarPath);
            if (rf.open(QIODevice::ReadOnly)) { meta = QJsonDocument::fromJson(rf.readAll()).object(); rf.close(); }
            meta["description"] = r.description;
            QFile wf(sidecarPath);
            if (wf.open(QIODevice::WriteOnly)) { wf.write(QJsonDocument(meta).toJson(QJsonDocument::Indented)); wf.close(); }
        }
    }
    if (wants.boxes) {
        appendLog(QString("[%1]  → OpenCV box…").arg(deviceId));
        r.boxesJpeg = detectBoxes(jpeg, r.boxesCount);
        if (!r.boxesJpeg.isEmpty()) {
            QFile bf(folder + "/" + base + "_boxes.jpg");
            if (bf.open(QIODevice::WriteOnly)) { bf.write(r.boxesJpeg); bf.close(); }
        }
    }
    if (wants.depth) {
        appendLog(QString("[%1]  → depth…").arg(deviceId));
        r.depthJpeg = depthMap(jpeg);
        if (!r.depthJpeg.isEmpty()) {
            QFile df(folder + "/" + base + "_depth.jpg");
            if (df.open(QIODevice::WriteOnly)) { df.write(r.depthJpeg); df.close(); }
        }
    }
    if (wants.edges) {
        appendLog(QString("[%1]  → bordi…").arg(deviceId));
        r.edgesJpeg = edgeMap(jpeg);
        if (!r.edgesJpeg.isEmpty()) {
            QFile ef(folder + "/" + base + "_edges.jpg");
            if (ef.open(QIODevice::WriteOnly)) { ef.write(r.edgesJpeg); ef.close(); }
        }
    }
    if (wants.bump) {
        appendLog(QString("[%1]  → bump map…").arg(deviceId));
        r.bumpJpeg = bumpMap(jpeg);
        if (!r.bumpJpeg.isEmpty()) {
            QFile bmf(folder + "/" + base + "_bump.jpg");
            if (bmf.open(QIODevice::WriteOnly)) { bmf.write(r.bumpJpeg); bmf.close(); }
        }
    }
    if (wants.normal) {
        appendLog(QString("[%1]  → normal map…").arg(deviceId));
        r.normalJpeg = normalMap(jpeg);
        if (!r.normalJpeg.isEmpty()) {
            QFile nmf(folder + "/" + base + "_normal.jpg");
            if (nmf.open(QIODevice::WriteOnly)) { nmf.write(r.normalJpeg); nmf.close(); }
        }
    }

    // anteprima UI (ultimo scatto, con etichetta device)
    QPixmap px; px.loadFromData(jpeg, "JPEG");
    if (!px.isNull() && m_lastThumb)
        m_lastThumb->setPixmap(px.scaled(dpiScale(160), dpiScale(160),
                                         Qt::KeepAspectRatio, Qt::SmoothTransformation));
    if (m_lastDesc) {
        const QString d = r.description.isEmpty() ? "(descrizione non richiesta)" : r.description;
        m_lastDesc->setText(QString("[%1] %2").arg(deviceId, d));
    }
    if (m_lastSensorInfo) {
        QJsonObject info = extraSensors;
        info["scan_mode"] = (scanMode == QLatin1String("scene")) ? QStringLiteral("scene")
                                                                   : QStringLiteral("object");
        m_lastSensorInfo->setText(v3dSensorInfoHtml(info));
    }
    if (m_lastBoxes) {
        if (!r.boxesJpeg.isEmpty()) {
            QPixmap b; b.loadFromData(r.boxesJpeg, "JPEG");
            m_lastBoxes->setPixmap(b.scaled(dpiScale(160), dpiScale(160),
                                            Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else if (wants.boxes) {
            m_lastBoxes->setPixmap(QPixmap());
            m_lastBoxes->setText("Box non disponibili:\nserve OpenCV\ndi sistema\n(libopencv-dev)");
        }
    }
    if (m_lastDepth) {
        if (!r.depthJpeg.isEmpty()) {
            QPixmap dp; dp.loadFromData(r.depthJpeg, "JPEG");
            m_lastDepth->setPixmap(dp.scaled(dpiScale(160), dpiScale(160),
                                             Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else if (wants.depth) {
            m_lastDepth->setPixmap(QPixmap());
            m_lastDepth->setText("Depth non riuscita\n(vedi Log:\ndi solito manca\ntorch/timm)");
        }
    }
    if (m_lastEdges) {
        if (!r.edgesJpeg.isEmpty()) {
            QPixmap e; e.loadFromData(r.edgesJpeg, "JPEG");
            m_lastEdges->setPixmap(e.scaled(dpiScale(160), dpiScale(160),
                                            Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else if (wants.edges) {
            m_lastEdges->setPixmap(QPixmap());
            m_lastEdges->setText("Bordi non disponibili:\nserve OpenCV\ndi sistema\n(libopencv-dev)");
        }
    }
    if (m_lastBump) {
        if (!r.bumpJpeg.isEmpty()) {
            QPixmap bm; bm.loadFromData(r.bumpJpeg, "JPEG");
            m_lastBump->setPixmap(bm.scaled(dpiScale(160), dpiScale(160),
                                            Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else if (wants.bump) {
            m_lastBump->setPixmap(QPixmap());
            m_lastBump->setText("Bump non disponibile:\nserve OpenCV\ndi sistema\n(libopencv-dev)");
        }
    }
    if (m_lastNormal) {
        if (!r.normalJpeg.isEmpty()) {
            QPixmap nm; nm.loadFromData(r.normalJpeg, "JPEG");
            m_lastNormal->setPixmap(nm.scaled(dpiScale(160), dpiScale(160),
                                              Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else if (wants.normal) {
            m_lastNormal->setPixmap(QPixmap());
            m_lastNormal->setText("Normal non disponibile:\nserve OpenCV\ndi sistema\n(libopencv-dev)");
        }
    }

    // galleria + scena 3D: gli scatti si ACCODANO, un clic li rivede.
    // Se la sessione dello scatto non è quella mostrata, ricarica tutto da disco.
    const bool sameSession = m_sessionCombo && m_sessionCombo->currentText() == safe;
    if (!sameSession && m_sessionCombo) {
        populateSessions(safe);       // segnali bloccati internamente
        loadSessionIntoUi(safe);      // include anche la foto appena salvata
    } else if (m_gallery && !px.isNull()) {
        auto* it = new QListWidgetItem(
            QIcon(px.scaled(dpiScale(96), dpiScale(96),
                            Qt::KeepAspectRatio, Qt::SmoothTransformation)),
            QString("%1 #%2 %3\xc2\xb0").arg(deviceId).arg(n, 3, 10, QChar('0')).arg(angle));
        it->setData(Qt::UserRole,     folder + "/" + base);   // base path per rivedere
        it->setData(Qt::UserRole + 1, r.description);
        it->setData(Qt::UserRole + 2, deviceId);
        it->setData(Qt::UserRole + 3, n);
        it->setToolTip(base + ".jpg");
        m_gallery->addItem(it);
        m_gallery->scrollToItem(it);
        if (m_scene) {
            m_scene->addShot({folder + "/" + base, deviceId, n, angle, pitch, hasSensors});
            m_scene->setSelectedKey(folder + "/" + base);
        }
    }
    updatePhotoRequirement();
    appendLog(QString("[%1]  done.").arg(deviceId));
    return r;
}

// ---------------------------------------------------------------------------
QString Vision3DWidget::vlmDescribe(const QByteArray& jpeg)
{
    QJsonObject payload;
    payload["model"]  = m_vlmModel;
    payload["prompt"] = "Descrivi in italiano, in modo conciso, l'oggetto o la "
                        "scena principale in questa foto. Indica: cosa e', materiale "
                        "probabile, colore, e a cosa serve. Massimo 3 frasi.";
    payload["stream"] = false;
    QJsonArray imgs; imgs.append(QString::fromUtf8(jpeg.toBase64()));
    payload["images"] = imgs;

    QNetworkRequest req(QUrl(m_ollamaUrl + "/api/generate"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // Chiamata bloccante con event loop locale: il resto della UI resta reattivo,
    // il timeout evita attese infinite se Ollama si blocca.
    QEventLoop loop;
    QTimer timeout; timeout.setSingleShot(true);
    QNetworkReply* reply = m_net->post(req, QJsonDocument(payload).toJson());
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(120000);
    loop.exec();

    if (!reply->isFinished()) {          // timeout scaduto: annulla la richiesta
        reply->abort();
        reply->deleteLater();
        return QStringLiteral("(VLM: timeout dopo 120s — modello troppo lento o bloccato)");
    }
    if (reply->error() != QNetworkReply::NoError) {
        const int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QString e = reply->errorString();
        reply->deleteLater();
        if (http == 404)   // Ollama risponde 404 quando il modello non è installato
            return QString("(modello '%1' non installato in Ollama: scaricalo con "
                           "'ollama pull %1' oppure scegli dal menu VLM un modello "
                           "vision gia' presente, es. moondream)").arg(m_vlmModel);
        return QString("(VLM non disponibile: %1. Avvia 'ollama serve' e scarica %2)")
               .arg(e, m_vlmModel);
    }
    const QByteArray resp = reply->readAll();
    reply->deleteLater();
    const QJsonObject o = QJsonDocument::fromJson(resp).object();
    const QString text = o.value("response").toString().trimmed();
    return text.isEmpty() ? "(nessuna risposta dal VLM)" : text;
}

// ---------------------------------------------------------------------------
QByteArray Vision3DWidget::detectBoxes(const QByteArray& jpeg, int& nBoxes)
{
    nBoxes = 0;
#ifdef VISION3D_USE_OPENCV
    std::vector<uchar> data(jpeg.begin(), jpeg.end());
    cv::Mat img = cv::imdecode(data, cv::IMREAD_COLOR);
    if (img.empty()) return QByteArray();

    cv::Mat gray, edges;
    cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, gray, {5,5}, 0);
    cv::Canny(gray, edges, 50, 150);
    cv::dilate(edges, edges, cv::getStructuringElement(cv::MORPH_RECT, {5,5}), {-1,-1}, 2);

    std::vector<std::vector<cv::Point>> cnts;
    cv::findContours(edges, cnts, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    const double areaMin = double(img.cols) * img.rows * 0.01;
    std::vector<cv::Rect> boxes;
    for (auto& c : cnts) {
        cv::Rect b = cv::boundingRect(c);
        if (double(b.width) * b.height > areaMin) boxes.push_back(b);
    }
    std::sort(boxes.begin(), boxes.end(),
              [](const cv::Rect& a, const cv::Rect& b){ return a.area() > b.area(); });
    if (boxes.size() > 8) boxes.resize(8);

    int i = 1;
    for (const auto& b : boxes) {
        cv::rectangle(img, b, cv::Scalar(63,92,255), 3);
        cv::putText(img, "#" + std::to_string(i++), {b.x+4, b.y+26},
                    cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(63,92,255), 2);
    }
    nBoxes = int(boxes.size());

    std::vector<uchar> outbuf;
    cv::imencode(".jpg", img, outbuf, {cv::IMWRITE_JPEG_QUALITY, 88});
    return QByteArray(reinterpret_cast<const char*>(outbuf.data()), int(outbuf.size()));
#else
    Q_UNUSED(jpeg);
    return QByteArray();
#endif
}

// ---------------------------------------------------------------------------
// Bordi (Canny) — visualizzazione a sé, non solo passo interno di detectBoxes().
QByteArray Vision3DWidget::edgeMap(const QByteArray& jpeg)
{
#ifdef VISION3D_USE_OPENCV
    std::vector<uchar> data(jpeg.begin(), jpeg.end());
    cv::Mat img = cv::imdecode(data, cv::IMREAD_COLOR);
    if (img.empty()) return QByteArray();

    cv::Mat gray, edges, out;
    cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, gray, {5,5}, 0);
    cv::Canny(gray, edges, 50, 150);   // stesse soglie di detectBoxes(), verificate anche a contrasto moderato
    cv::cvtColor(edges, out, cv::COLOR_GRAY2BGR);   // bianco su nero, leggibile su schermi piccoli

    std::vector<uchar> outbuf;
    cv::imencode(".jpg", out, outbuf, {cv::IMWRITE_JPEG_QUALITY, 88});
    return QByteArray(reinterpret_cast<const char*>(outbuf.data()), int(outbuf.size()));
#else
    Q_UNUSED(jpeg);
    return QByteArray();
#endif
}

// ---------------------------------------------------------------------------
// Bump map "naif" da una singola foto: luminanza equalizzata (CLAHE) usata
// come mappa di altezza — non è una vera misura di rilievo (servirebbe una
// depth map reale o più scatti con luce radente), ma è la stessa tecnica
// comune per ricavare un bump/normal approssimati da UNA foto sola.
QByteArray Vision3DWidget::bumpMap(const QByteArray& jpeg)
{
#ifdef VISION3D_USE_OPENCV
    std::vector<uchar> data(jpeg.begin(), jpeg.end());
    cv::Mat img = cv::imdecode(data, cv::IMREAD_COLOR);
    if (img.empty()) return QByteArray();

    cv::Mat gray, eq, out;
    cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, gray, {3,3}, 0);
    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(2.0, cv::Size(8,8));
    clahe->apply(gray, eq);
    cv::cvtColor(eq, out, cv::COLOR_GRAY2BGR);

    std::vector<uchar> outbuf;
    cv::imencode(".jpg", out, outbuf, {cv::IMWRITE_JPEG_QUALITY, 88});
    return QByteArray(reinterpret_cast<const char*>(outbuf.data()), int(outbuf.size()));
#else
    Q_UNUSED(jpeg);
    return QByteArray();
#endif
}

// ---------------------------------------------------------------------------
// Normal map da gradienti Sobel della luminanza (tecnica "normal from
// heightmap/photo" standard nei tool di texturing): non sostituisce una
// normal map da geometria reale, ma dà un'anteprima immediata senza dover
// aspettare la ricostruzione 3D completa.
QByteArray Vision3DWidget::normalMap(const QByteArray& jpeg)
{
#ifdef VISION3D_USE_OPENCV
    std::vector<uchar> data(jpeg.begin(), jpeg.end());
    cv::Mat img = cv::imdecode(data, cv::IMREAD_COLOR);
    if (img.empty()) return QByteArray();

    cv::Mat gray, gray32;
    cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, gray, {5,5}, 0);
    gray.convertTo(gray32, CV_32F, 1.0/255.0);

    cv::Mat sobelX, sobelY;
    cv::Sobel(gray32, sobelX, CV_32F, 1, 0, 3);
    cv::Sobel(gray32, sobelY, CV_32F, 0, 1, 3);

    const float strength = 3.0f;   // quanto marcato appare il rilievo
    cv::Mat out(gray.size(), CV_8UC3);
    for (int y = 0; y < gray.rows; ++y) {
        for (int x = 0; x < gray.cols; ++x) {
            const float dx = sobelX.at<float>(y, x) * strength;
            const float dy = sobelY.at<float>(y, x) * strength;
            float nx = -dx, ny = -dy, nz = 1.0f;
            const float len = std::sqrt(nx*nx + ny*ny + nz*nz);
            nx /= len; ny /= len; nz /= len;
            // convenzione tangent-space: canale = componente*0.5+0.5;
            // OpenCV è BGR quindi B=nz (quasi sempre alto → immagine bluastra)
            out.at<cv::Vec3b>(y, x) = cv::Vec3b(
                uchar(qBound(0.0f, (nz*0.5f+0.5f)*255.0f, 255.0f)),
                uchar(qBound(0.0f, (ny*0.5f+0.5f)*255.0f, 255.0f)),
                uchar(qBound(0.0f, (nx*0.5f+0.5f)*255.0f, 255.0f)));
        }
    }

    std::vector<uchar> outbuf;
    cv::imencode(".jpg", out, outbuf, {cv::IMWRITE_JPEG_QUALITY, 92});
    return QByteArray(reinterpret_cast<const char*>(outbuf.data()), int(outbuf.size()));
#else
    Q_UNUSED(jpeg);
    return QByteArray();
#endif
}

// ---------------------------------------------------------------------------
// ArUco: rileva marker DICT_4X4_50 e calcola mm reali per unità-immagine.
// Ritorna la mediana di (marker_mm / lato_marker_in_px) sui marker visti.
// Questo dà la SCALA: quanti mm reali corrisponde 1 px alla distanza del marker.
// found = numero di marker rilevati (0 = nessuno, scala non disponibile).
double Vision3DWidget::detectAruco(const QByteArray& jpeg, int& found)
{
    found = 0;
#if defined(VISION3D_USE_OPENCV) && defined(VISION3D_USE_ARUCO)
    std::vector<uchar> data(jpeg.begin(), jpeg.end());
    cv::Mat img = cv::imdecode(data, cv::IMREAD_GRAYSCALE);
    if (img.empty()) return 0.0;

    cv::aruco::Dictionary dict =
        cv::aruco::getPredefinedDictionary(cv::aruco::DICT_4X4_50);
    cv::aruco::DetectorParameters params;
    cv::aruco::ArucoDetector detector(dict, params);

    std::vector<int> ids;
    std::vector<std::vector<cv::Point2f>> corners;
    detector.detectMarkers(img, corners, ids);
    if (ids.empty()) return 0.0;

    // per ogni marker: lato medio in px = media dei 4 lati del quadrilatero
    std::vector<double> mmPerPx;
    for (const auto& c : corners) {
        if (c.size() != 4) continue;
        double perim = 0.0;
        for (int i = 0; i < 4; ++i)
            perim += cv::norm(c[i] - c[(i+1)%4]);
        const double sidePx = perim / 4.0;
        if (sidePx > 1.0) mmPerPx.push_back(m_arucoMarkerMm / sidePx);
    }
    if (mmPerPx.empty()) return 0.0;
    std::sort(mmPerPx.begin(), mmPerPx.end());
    found = int(mmPerPx.size());
    return mmPerPx[mmPerPx.size()/2];   // mediana: robusta agli outlier
#else
    Q_UNUSED(jpeg);
    return 0.0;   // aruco non compilato → scala automatica non disponibile
#endif
}

// ---------------------------------------------------------------------------
// Depth map via script Python esterno (MiDaS): stdin JPEG → stdout JPEG colormap.
// Processo separato: niente torch linkato in C++, modello sostituibile senza ricompilare.
QByteArray Vision3DWidget::depthMap(const QByteArray& jpeg)
{
    if (m_depthScript.isEmpty() || !QFile::exists(m_depthScript))
        return QByteArray();

    QProcess proc;
    proc.start(m_pythonExe, {m_depthScript});
    if (!proc.waitForStarted(5000)) { appendLog("  depth: python non avviato"); return QByteArray(); }
    proc.write(jpeg);
    proc.closeWriteChannel();
    if (!proc.waitForFinished(120000)) { proc.kill(); appendLog("  depth: timeout"); return QByteArray(); }
    if (proc.exitCode() != 0) {
        appendLog("  depth: " + QString::fromUtf8(proc.readAllStandardError()).left(200));
        return QByteArray();
    }
    return proc.readAllStandardOutput();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Ricostruzione 3D — COLMAP in 2 passi: mappa sparsa → export PLY
// ═══════════════════════════════════════════════════════════════════════════
static const char* kReconBtnIdle = "\xf0\x9f\xa7\x8a Crea nuvola di punti (COLMAP)"; /* 🧊 */
static const char* kReconBtnBusy = "\xe2\x8f\xb9 Ferma ricostruzione";               /* ⏹ */

/* Sonda COLMAP A CALDO: non basta trovare l'eseguibile, deve anche partire
 * (caso reale: colmap installato ma libPoseLib.so assente → exit 127).
 * ok=true solo se 'colmap help' esce con 0. Nessun riavvio necessario:
 * si richiama dopo ogni 'sudo apt install' col pulsante Ricontrolla. */
static QString probeColmap(bool& ok)
{
    ok = false;
    const QString exe = QStandardPaths::findExecutable("colmap");
    if (exe.isEmpty())
        return QStringLiteral(
            "COLMAP non installato: sudo apt install colmap libposelib — poi premi "
            "\xe2\x9f\xb3 Ricontrolla (senza riavviare Prismalux). In alternativa le "
            "foto in scan_output/ sono pronte per Meshroom.");
    QProcess p;
    p.start(exe, {"help"});
    if (!p.waitForStarted(3000) || !p.waitForFinished(5000)) {
        p.kill();
        return QStringLiteral("COLMAP presente ma non risponde (timeout).");
    }
    if (p.exitCode() != 0) {
        const QString err = QString::fromUtf8(p.readAllStandardError());
        // "error while loading shared libraries: libPoseLib.so: cannot open…"
        if (err.contains(QLatin1String("libPoseLib"), Qt::CaseInsensitive))
            return QStringLiteral(
                "COLMAP installato ma manca libPoseLib.so: sudo apt install libposelib "
                "— poi premi \xe2\x9f\xb3 Ricontrolla (senza riavviare).");
        if (err.contains(QLatin1String("shared libraries"), Qt::CaseInsensitive))
            return QStringLiteral("COLMAP installato ma manca una libreria: %1")
                   .arg(err.trimmed().left(160));
        return QStringLiteral("COLMAP presente ma non parte (exit %1): %2")
               .arg(p.exitCode()).arg(err.trimmed().left(120));
    }
    ok = true;
    return QStringLiteral(
        "COLMAP pronto. Nuvola di punti sparsa dalla sessione selezionata; il PLY "
        "finale si apre con CloudCompare/MeshLab. Servono \xe2\x89\xa5 10-20 foto "
        "con buona sovrapposizione.");
}

void Vision3DWidget::updateReconHint()
{
    if (!m_reconHint) return;
    bool ok = false;
    m_reconHint->setText(probeColmap(ok));
}

/* Foto minime consigliate per livello di qualità COLMAP. */
int Vision3DWidget::requiredPhotosFor(const QString& quality)
{
    if (quality == QLatin1String("low"))  return 10;
    if (quality == QLatin1String("high")) return 40;
    return 20;   // medium
}

/* Conta le foto della sessione su disco (tutti i device, solo originali). */
int Vision3DWidget::countSessionPhotos(const QString& session) const
{
    int count = 0;
    const QDir sess(m_outputDir + "/" + session);
    const QFileInfoList devs = sess.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& dv : devs)
        count += int(QDir(dv.absoluteFilePath())
                         .entryList({"*_a???.jpg"}, QDir::Files).size());
    return count;
}

/* Contatore "X/Y foto" accanto alla qualità: verde se bastano, rosso se no. */
void Vision3DWidget::updatePhotoRequirement()
{
    if (!m_photoReqLabel || !m_sessionCombo || !m_qualityCombo) return;
    const int have = countSessionPhotos(m_sessionCombo->currentText());
    const int need = requiredPhotosFor(m_qualityCombo->currentText());
    m_photoReqLabel->setText(QString("%1/%2 foto").arg(have).arg(need));
    m_photoReqLabel->setStyleSheet(have >= need
        ? QStringLiteral("color:#3fb950;font-weight:bold;")
        : QStringLiteral("color:#d29922;font-weight:bold;"));
    if (m_scene) m_scene->setTargetQuality(m_qualityCombo->currentText());
}

void Vision3DWidget::onQualityComboChanged(const QString& quality)
{
    Q_UNUSED(quality);
    updatePhotoRequirement();
}

void Vision3DWidget::onReconRecheckClicked()
{
    bool ok = false;
    const QString status = probeColmap(ok);
    if (m_reconHint) m_reconHint->setText(status);
    appendLog(QString("Controllo COLMAP: %1").arg(ok ? "pronto \xe2\x9c\x93" : status));
}

void Vision3DWidget::onReconStartClicked()
{
    if (m_reconProc) {                      // già in corso → ferma
        appendLog("Ricostruzione interrotta dall'utente.");
        m_reconProc->disconnect(this);
        m_reconProc->kill();
        m_reconProc->waitForFinished(2000);
        m_reconProc->deleteLater();
        m_reconProc = nullptr;
        if (m_reconBtn) m_reconBtn->setText(kReconBtnIdle);
        return;
    }

    bool colmapOk = false;
    const QString status = probeColmap(colmapOk);
    if (!colmapOk) {                        // sonda a caldo: exe + librerie
        if (m_reconHint) m_reconHint->setText(status);
        appendLog(status);
        return;
    }
    const QString colmap = QStandardPaths::findExecutable("colmap");
    const QString session = m_sessionCombo ? m_sessionCombo->currentText() : QString();
    if (session.isEmpty()) {
        appendLog("Nessuna sessione da ricostruire: scatta prima qualche foto.");
        return;
    }

    // requisito foto per qualità: sotto soglia si può proseguire, ma avvisati
    const QString quality0 = m_qualityCombo ? m_qualityCombo->currentText()
                                            : QStringLiteral("medium");
    const int have = countSessionPhotos(session);
    const int need = requiredPhotosFor(quality0);
    if (have < need) {
        const auto ans = QMessageBox::question(this, tr("Poche foto"),
            tr("Per la qualit\xc3\xa0 '%1' servono almeno %2 foto: la sessione '%3' "
               "ne ha %4.\nCon poche foto la nuvola pu\xc3\xb2 uscire vuota o a pezzi.\n\n"
               "Continuare comunque?").arg(quality0).arg(need).arg(session).arg(have));
        if (ans != QMessageBox::Yes) return;
    }

    m_reconSessionDir = m_outputDir + "/" + session;
    const QString ws = m_reconSessionDir + "/colmap_ws";
    QDir().mkpath(ws);

    m_reconProc = new QProcess(this);
    m_reconProc->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_reconProc, &QProcess::readyReadStandardOutput,
            this, &Vision3DWidget::onReconProcOutput);
    connect(m_reconProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &Vision3DWidget::onReconProcFinished);

    m_reconStep = 0;
    const QString quality = m_qualityCombo ? m_qualityCombo->currentText()
                                           : QStringLiteral("medium");
    appendLog(QString("Ricostruzione COLMAP di '%1' (qualita' %2) — puo' richiedere "
                      "parecchi minuti, il log scorre qui sotto…").arg(session, quality));
    /* --dense 0: la densificazione richiede CUDA; la mappa sparsa basta per
       la nuvola di punti. --use_gpu 0: SIFT su CPU, funziona ovunque. */
    m_reconProc->start(colmap, {"automatic_reconstructor",
                                "--workspace_path", ws,
                                "--image_path",     m_reconSessionDir,
                                "--quality",        quality,
                                "--dense",          "0",
                                "--use_gpu",        "0"});
    if (m_reconBtn) m_reconBtn->setText(kReconBtnBusy);
}

void Vision3DWidget::onReconProcOutput()
{
    if (!m_reconProc) return;
    const QList<QByteArray> lines = m_reconProc->readAllStandardOutput().split('\n');
    for (const QByteArray& l : lines) {
        const QByteArray t = l.trimmed();
        if (!t.isEmpty()) appendLog("  colmap: " + QString::fromUtf8(t.left(160)));
    }
}

void Vision3DWidget::onReconProcFinished(int code, QProcess::ExitStatus status)
{
    if (!m_reconProc) return;
    const bool ok = (status == QProcess::NormalExit && code == 0);

    if (m_reconStep == 0 && ok) {           // passo 2: esporta la nuvola in PLY
        m_reconStep = 1;
        appendLog("Mappa sparsa completata — esporto la nuvola di punti in PLY…");
        m_reconProc->start(QStandardPaths::findExecutable("colmap"),
                           {"model_converter",
                            "--input_path",  m_reconSessionDir + "/colmap_ws/sparse/0",
                            "--output_path", m_reconSessionDir + "/nuvola_punti.ply",
                            "--output_type", "PLY"});
        return;
    }

    if (m_reconStep == 1 && ok) {           // passo 3: PLY → OBJ + MTL (punti colorati)
        m_reconStep = 2;
        appendLog("PLY pronto — converto in OBJ + MTL (nuvola di punti colorata)…");
        m_reconProc->start(m_pythonExe,
                           {P::root() + "/Tools/scripts/ply_to_obj.py",
                            m_reconSessionDir + "/nuvola_punti.ply",
                            m_reconSessionDir + "/modello"});
        return;
    }

    m_reconProc->deleteLater();
    m_reconProc = nullptr;
    if (m_reconBtn) m_reconBtn->setText(kReconBtnIdle);

    if (ok && m_reconStep == 2)
        appendLog("Modello pronto in " + m_reconSessionDir + ": nuvola_punti.ply + "
                  "modello.obj + modello.mtl (punti colorati). Dal client iOS/Android/"
                  "Desktop: pulsanti \xe2\xac\x87 nella card 'Modello 3D'. Sul PC: "
                  "CloudCompare/MeshLab (l\xc3\xac puoi anche generare la mesh: "
                  "Filters \xe2\x86\x92 Poisson).");
    else if (m_reconStep == 2)
        appendLog(QString("Nuvola PLY pronta (%1/nuvola_punti.ply) ma conversione "
                          "OBJ fallita (exit %2) — controlla che python3 sia disponibile.")
                      .arg(m_reconSessionDir).arg(code));
    else
        appendLog(QString("Ricostruzione terminata con errore (exit %1). Servono "
                          "10-20+ foto nitide con buona sovrapposizione tra scatti "
                          "vicini; la board CharUco sotto l'oggetto aiuta molto.").arg(code));
}

// ---------------------------------------------------------------------------
#if QT_CONFIG(ssl)
void Vision3DWidget::sendHtml(QSslSocket* sock, const QByteArray& html, const QByteArray& setCookie)
{
    QByteArray resp = "HTTP/1.1 200 OK\r\n"
                      "Content-Type: text/html; charset=utf-8\r\n"
                      + setCookie +
                      "Content-Length: " + QByteArray::number(html.size()) + "\r\n"
                      "Connection: close\r\n\r\n" + html;
    sock->write(resp); sock->flush(); sock->disconnectFromHost();
}
void Vision3DWidget::sendJson(QSslSocket* sock, const QByteArray& json, const QByteArray& setCookie)
{
    QByteArray resp = "HTTP/1.1 200 OK\r\n"
                      "Content-Type: application/json\r\n"
                      + setCookie +
                      "Content-Length: " + QByteArray::number(json.size()) + "\r\n"
                      "Connection: close\r\n\r\n" + json;
    sock->write(resp); sock->flush(); sock->disconnectFromHost();
}
void Vision3DWidget::send404(QSslSocket* sock)
{
    QByteArray resp = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    sock->write(resp); sock->flush(); sock->disconnectFromHost();
}

void Vision3DWidget::send401(QSslSocket* sock)
{
    QByteArray resp = "HTTP/1.1 401 Unauthorized\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    sock->write(resp); sock->flush(); sock->disconnectFromHost();
}
#endif // QT_CONFIG(ssl)

QString Vision3DWidget::queryParam(const QByteArray& pathWithQuery, const QByteArray& key)
{
    const int qm = pathWithQuery.indexOf('?');
    if (qm < 0) return QString();
    const QList<QByteArray> parts = pathWithQuery.mid(qm + 1).split('&');
    const QByteArray prefix = key + "=";
    for (const QByteArray& kv : parts)
        if (kv.startsWith(prefix))
            return QUrl::fromPercentEncoding(kv.mid(prefix.size()));
    return QString();
}

// ---------------------------------------------------------------------------
static QLabel* makeVision3dThumb()
{
    auto* l = new QLabel;
    l->setFixedSize(dpiScale(160), dpiScale(160));
    l->setAlignment(Qt::AlignCenter);
    /* palette(): segue il tema attivo (chiaro o scuro), niente nero fisso */
    l->setStyleSheet("background:palette(base);border:1px solid palette(mid);"
                     "border-radius:8px;color:palette(text);");
    l->setText("—");
    return l;
}

void Vision3DWidget::buildUi()
{
    auto* root = new QVBoxLayout(this);

    auto* head = new QHBoxLayout;
    m_statusDot = new QLabel("●"); m_statusDot->setStyleSheet("color:#8b949e;font-size:18px;");
    auto* title = new QLabel("<b>Prismalux Vision3D</b> — client iOS, Android e Desktop");
    m_urlLabel = new QLabel("(server fermo)");
    m_urlLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_urlLabel->setStyleSheet("color:#3fb0ff;");
    head->addWidget(m_statusDot); head->addWidget(title); head->addStretch();
    head->addWidget(new QLabel("URL:")); head->addWidget(m_urlLabel);
    auto* helpBtn = new QPushButton("?");
    helpBtn->setObjectName("v3dHelpBtn");
    helpBtn->setFixedSize(dpiScale(28), dpiScale(28));
    helpBtn->setToolTip("Guida rapida: installazione COLMAP e collegamento dei client");
    connect(helpBtn, &QPushButton::clicked, this, &Vision3DWidget::onHelpClicked);
    head->addWidget(helpBtn);
    root->addLayout(head);

    auto* ctrl = new QHBoxLayout;
    ctrl->addWidget(new QLabel("Interfaccia:"));
    m_ifaceCombo = new QComboBox;
    m_ifaceCombo->setObjectName("v3dIfaceCombo");
    m_ifaceCombo->addItems(listLocalIps());
    m_ifaceCombo->setToolTip("Il server ascolta SOLO su questo indirizzo.\n"
                             "Scegli la tua LAN (192.168.x) — mai il bridge hotspot.");
    ctrl->addWidget(m_ifaceCombo);
    auto* ifaceRefreshBtn = new QPushButton("\xe2\x9f\xb3");   /* ⟳ */
    ifaceRefreshBtn->setFixedWidth(dpiScale(28));
    ifaceRefreshBtn->setToolTip("Aggiorna elenco interfacce di rete");
    connect(ifaceRefreshBtn, &QPushButton::clicked, this, &Vision3DWidget::onRefreshIfacesClicked);
    ctrl->addWidget(ifaceRefreshBtn);
    ctrl->addSpacing(dpiScale(12));
    ctrl->addWidget(new QLabel("Porta:"));
    m_portEdit = new QLineEdit(QString::number(m_port));
    m_portEdit->setFixedWidth(dpiScale(70));
    ctrl->addWidget(m_portEdit);
    m_toggleBtn = new QPushButton("Avvia server");
    connect(m_toggleBtn, &QPushButton::clicked, this, &Vision3DWidget::onToggleServerClicked);
    ctrl->addWidget(m_toggleBtn);
    ctrl->addSpacing(dpiScale(12));
    ctrl->addWidget(new QLabel("VLM:"));
    m_vlmCombo = new QComboBox;
    m_vlmCombo->setObjectName("v3dVlmCombo");
    m_vlmCombo->setEditable(true);
    m_vlmCombo->addItem(m_vlmModel);
    m_vlmCombo->setMinimumWidth(dpiScale(180));
    m_vlmCombo->setToolTip("Modello Ollama per la descrizione delle foto.\n"
                           "Serve un modello VISION (moondream, llava, qwen2.5-vl, minicpm-v...):\n"
                           "i modelli solo-testo rispondono ma ignorano l'immagine.\n"
                           "All'avvio del server l'elenco si riempie con i modelli installati.");
    connect(m_vlmCombo, &QComboBox::currentTextChanged,
            this, &Vision3DWidget::onVlmModelChanged);
    ctrl->addWidget(m_vlmCombo);
    ctrl->addStretch();
    root->addLayout(ctrl);

    /* ── due sotto-schede: prima "Preparazione" (collegamento, bersagli
       ArUco, monitoraggio scatti in arrivo) — va fatta bene PRIMA di tutto
       il resto — poi "Assembla punti e texture" (scena 3D + ricostruzione
       COLMAP), che parte dalle foto già preparate. ── */
    auto* tabs = new QTabWidget;

    /* ═══ Tab 1: Preparazione ═══ */
    auto* prepScroll = new QScrollArea;
    prepScroll->setWidgetResizable(true);
    prepScroll->setFrameShape(QFrame::NoFrame);
    auto* leftCol = new QWidget;
    auto* leftLay = new QHBoxLayout(leftCol);
    leftLay->setContentsMargins(0, 0, 0, 0);

    /* Due colonne per ridurre lo scroll verticale: a sinistra la
       preparazione vera e propria (connessione, ArUco, sessione, device),
       a destra ciò che arriva/si produce durante lo scatto (galleria,
       ultimo scatto analizzato) — che è anche la parte più "alta". */
    auto* colA = new QWidget;
    auto* colALay = new QVBoxLayout(colA);
    colALay->setContentsMargins(0, 0, 0, 0);
    auto* colB = new QWidget;
    auto* colBLay = new QVBoxLayout(colB);
    colBLay->setContentsMargins(0, 0, 0, 0);

    // Connessione dal telefono: QR (aggiornato da start()/stop()) + istruzioni
    auto* connBox = new QGroupBox("Connessione dal telefono");
    auto* connLay = new QHBoxLayout(connBox);
    m_prepQr = new QrCodeWidget(QString());
    m_prepQr->setFixedSize(dpiScale(110), dpiScale(110));
    m_prepQr->setToolTip("Inquadra con la fotocamera del telefono per aprire l'URL del server.");
    m_prepQr->setEmptyHint("Non è possibile mostrare il QR code perché il server non è avviato");
    connLay->addWidget(m_prepQr);
    // Testo con tag HTML (<b>/<i>) → Qt lo rende in rich text, dove "\n"
    // letterale NON va a capo (viene collassato come uno spazio, a
    // differenza del plain text): serve <br> esplicito fra i punti.
    auto* connHint = new QLabel(
        "1. Scegli interfaccia/porta e VLM qui sopra, poi premi <b>Avvia server</b>.<br>"
        "2. Sullo stesso WiFi, inquadra questo QR col telefono (o apri l'URL a mano).<br>"
        "3. Certificato self-signed: <i>Avanzate \xe2\x86\x92 Procedi</i>.");
    connHint->setWordWrap(true);
    connLay->addWidget(connHint, 1);
    colALay->addWidget(connBox);

    // Bersagli ArUco: scala reale del modello — vanno stampati PRIMA di scattare
    auto* arucoBox = new QGroupBox("Bersagli ArUco \xe2\x80\x94 scala reale");
    auto* arucoLay = new QVBoxLayout(arucoBox);
    arucoLay->addWidget(new QLabel(
        "Stampa sempre al 100% / \"dimensione reale\" (MAI \"adatta alla pagina\"):"));
    auto* arucoBtnRow = new QHBoxLayout;
    auto* printArucoBtn = new QPushButton("\xf0\x9f\x96\xa8 Marker singoli (PDF)");
    printArucoBtn->setObjectName("actionBtn");
    connect(printArucoBtn, &QPushButton::clicked, this, [] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(P::root() + "/Tools/aruco/aruco_markers_A4.pdf"));
    });
    auto* printBoardBtn = new QPushButton("\xf0\x9f\x96\xa8 Board CharUco (PDF)");
    printBoardBtn->setObjectName("actionBtn");
    connect(printBoardBtn, &QPushButton::clicked, this, [] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(P::root() + "/Tools/aruco/charuco_board_A4.pdf"));
    });
    arucoBtnRow->addWidget(printArucoBtn);
    arucoBtnRow->addWidget(printBoardBtn);
    arucoLay->addLayout(arucoBtnRow);
    colALay->addWidget(arucoBox);

    // Sessione: più sessioni nominabili (notturno, villa, oggettistico,
    // natura...) — crea qui il nome PRIMA di andare a scattare, poi usa
    // lo stesso nome nel campo "Nome sessione" sul telefono per unirle.
    auto* sessBox = new QGroupBox("Sessione");
    auto* sessLay = new QHBoxLayout(sessBox);
    m_prepSessionCombo = new QComboBox;
    m_prepSessionCombo->setToolTip(kPrepSessionComboTip);
    connect(m_prepSessionCombo, &QComboBox::currentTextChanged,
            this, &Vision3DWidget::onPrepSessionComboChanged);
    sessLay->addWidget(m_prepSessionCombo, 1);
    auto* newSessBtn = new QPushButton("\xe2\x9e\x95 Nuova");
    newSessBtn->setObjectName("actionBtn");
    newSessBtn->setToolTip("Crea una sessione vuota con un nome a scelta "
                           "(es. notturno, villa, oggettistico, natura...)");
    connect(newSessBtn, &QPushButton::clicked, this, &Vision3DWidget::onNewSessionClicked);
    sessLay->addWidget(newSessBtn);
    auto* editSessDescBtn = new QPushButton("\xe2\x9c\x8f\xef\xb8\x8f");   /* ✏️ */
    editSessDescBtn->setObjectName("actionBtn");
    editSessDescBtn->setFixedWidth(dpiScale(36));
    editSessDescBtn->setToolTip(tr("Inserisci o modifica una descrizione libera per questa "
                                   "sessione (es. cosa stai scansionando, condizioni di luce)."));
    connect(editSessDescBtn, &QPushButton::clicked,
            this, &Vision3DWidget::onEditSessionDescriptionClicked);
    sessLay->addWidget(editSessDescBtn);
    colALay->addWidget(sessBox);

    // pannello device attivi
    auto* devBox = new QGroupBox("Device attivi");
    auto* devLay = new QVBoxLayout(devBox);
    m_deviceTable = new QTableWidget(0, 4);
    m_deviceTable->setHorizontalHeaderLabels({"Device", "Foto", "Ultimo", "Dispositivo"});
    m_deviceTable->horizontalHeader()->setStretchLastSection(true);
    m_deviceTable->verticalHeader()->setVisible(false);
    m_deviceTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_deviceTable->setMaximumHeight(dpiScale(140));
    m_deviceTable->setContextMenuPolicy(Qt::CustomContextMenu);
    m_deviceTable->setToolTip(tr("Tasto destro su un device per eliminarlo (e le sue foto)."));
    connect(m_deviceTable, &QTableWidget::customContextMenuRequested,
            this, &Vision3DWidget::onDeviceTableContextMenu);
    devLay->addWidget(m_deviceTable);
    colALay->addWidget(devBox);
    colALay->addStretch();

    // galleria scatti: si ACCODANO tutti qui, un clic li rivede nei pannelli sotto
    auto* galBox = new QGroupBox("Scatti ricevuti (clic per rivedere)");
    auto* galLay = new QVBoxLayout(galBox);
    m_gallery = new QListWidget;
    m_gallery->setObjectName("v3dGallery");
    m_gallery->setViewMode(QListView::IconMode);
    m_gallery->setFlow(QListView::LeftToRight);
    m_gallery->setWrapping(false);
    m_gallery->setIconSize(QSize(dpiScale(96), dpiScale(96)));
    m_gallery->setFixedHeight(dpiScale(140));
    /* Selezione multipla (Ctrl/Shift/click, come un file manager): un clic
       normale seleziona e mostra l'anteprima, Ctrl+click aggiunge alla
       selezione per eliminarne più di uno insieme. */
    m_gallery->setSelectionMode(QAbstractItemView::ExtendedSelection);
    connect(m_gallery, &QListWidget::itemClicked,
            this, &Vision3DWidget::onGalleryItemClicked);
    m_gallery->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_gallery, &QListWidget::customContextMenuRequested,
            this, &Vision3DWidget::onGalleryContextMenu);
    galLay->addWidget(m_gallery);

    // tasto Canc con la galleria attiva = elimina gli scatti selezionati
    auto* deleteShortcut = new QShortcut(QKeySequence::Delete, m_gallery);
    deleteShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(deleteShortcut, &QShortcut::activated, this, &Vision3DWidget::onDeletePhotoClicked);

    auto* galBtnRow = new QHBoxLayout;
    auto* importBtn = new QPushButton("\xf0\x9f\x93\xa5 Importa foto");
    importBtn->setObjectName("actionBtn");
    importBtn->setToolTip("Aggiungi foto già esistenti su disco alla sessione corrente "
                          "(senza dati bussola: poi \"Distribuisci sul cerchio\").");
    connect(importBtn, &QPushButton::clicked, this, &Vision3DWidget::onImportPhotosClicked);
    galBtnRow->addWidget(importBtn);
    auto* deletePhotoBtn = new QPushButton("\xf0\x9f\x97\x91 Elimina selezionate");
    deletePhotoBtn->setObjectName("actionBtn");
    deletePhotoBtn->setProperty("danger", true);
    deletePhotoBtn->setToolTip("Elimina definitivamente le foto selezionate in galleria "
                               "(Ctrl/Shift+click per selezionarne più di una, oppure tasto Canc).");
    connect(deletePhotoBtn, &QPushButton::clicked, this, &Vision3DWidget::onDeletePhotoClicked);
    galBtnRow->addWidget(deletePhotoBtn);
    auto* deleteAllBtn = new QPushButton("\xf0\x9f\x97\x91\xf0\x9f\x97\x91 Elimina tutte");
    deleteAllBtn->setObjectName("actionBtn");
    deleteAllBtn->setProperty("danger", true);
    deleteAllBtn->setToolTip("Elimina definitivamente TUTTE le foto di questa sessione.");
    connect(deleteAllBtn, &QPushButton::clicked, this, &Vision3DWidget::onDeleteAllPhotosClicked);
    galBtnRow->addWidget(deleteAllBtn);
    galLay->addLayout(galBtnRow);

    colBLay->addWidget(galBox);

    auto* prevBox = new QGroupBox("Ultimo scatto analizzato");
    auto* grid = new QGridLayout(prevBox);
    m_lastThumb  = makeVision3dThumb(); m_lastThumb->setObjectName("v3dThumbOriginal");
    m_lastBoxes  = makeVision3dThumb(); m_lastBoxes->setObjectName("v3dThumbBoxes");
    m_lastDepth  = makeVision3dThumb(); m_lastDepth->setObjectName("v3dThumbDepth");
    m_lastEdges  = makeVision3dThumb(); m_lastEdges->setObjectName("v3dThumbEdges");
    m_lastBump   = makeVision3dThumb(); m_lastBump->setObjectName("v3dThumbBump");
    m_lastNormal = makeVision3dThumb(); m_lastNormal->setObjectName("v3dThumbNormal");
    grid->addWidget(new QLabel("Originale"), 0,0);    grid->addWidget(m_lastThumb, 1,0);
    grid->addWidget(new QLabel("Box (OpenCV)"), 0,1); grid->addWidget(m_lastBoxes, 1,1);
    grid->addWidget(new QLabel("Depth"), 0,2);        grid->addWidget(m_lastDepth, 1,2);
    grid->addWidget(new QLabel("Bordi"), 2,0);        grid->addWidget(m_lastEdges, 3,0);
    grid->addWidget(new QLabel("Bump"), 2,1);         grid->addWidget(m_lastBump, 3,1);
    grid->addWidget(new QLabel("Normal"), 2,2);       grid->addWidget(m_lastNormal, 3,2);

    // tasto destro su una miniatura = salva quella singola immagine
    for (QLabel* thumb : {m_lastThumb, m_lastBoxes, m_lastDepth,
                           m_lastEdges, m_lastBump, m_lastNormal}) {
        thumb->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(thumb, &QLabel::customContextMenuRequested,
                this, &Vision3DWidget::onThumbContextMenu);
    }
    // doppio click su una delle 5 mappe (non "Originale", non c'è nulla da
    // ricalcolare) = falla ADESSO per lo scatto selezionato, vedi eventFilter()
    for (QLabel* thumb : {m_lastBoxes, m_lastDepth, m_lastEdges, m_lastBump, m_lastNormal}) {
        thumb->installEventFilter(this);
        thumb->setToolTip(tr("Doppio click per calcolare questa mappa adesso "
                             "(per lo scatto selezionato in galleria)."));
    }

    auto* saveAllMapsBtn = new QPushButton("\xf0\x9f\x92\xbe Salva tutte le mappe...");
    saveAllMapsBtn->setObjectName("actionBtn");
    saveAllMapsBtn->setToolTip("Copia originale, box, depth, bordi, bump e normal di questo "
                               "scatto (quelli calcolati) in una cartella a scelta.");
    connect(saveAllMapsBtn, &QPushButton::clicked, this, &Vision3DWidget::onSaveAllMapsClicked);
    grid->addWidget(saveAllMapsBtn, 4,0,1,3);

    auto* descRow = new QWidget;
    auto* descRowLay = new QHBoxLayout(descRow);
    descRowLay->setContentsMargins(0, 0, 0, 0);
    m_lastDesc = new QLabel("La descrizione VLM apparirà qui.");
    m_lastDesc->setWordWrap(true);
    m_lastDesc->setStyleSheet("background:palette(base);border:1px solid palette(mid);"
                              "border-radius:8px;padding:10px;");
    descRowLay->addWidget(m_lastDesc, 1);
    auto* editDescBtn = new QPushButton("\xe2\x9c\x8f\xef\xb8\x8f");   /* ✏️ */
    editDescBtn->setObjectName("actionBtn");
    editDescBtn->setFixedWidth(dpiScale(36));
    editDescBtn->setToolTip(tr("Inserisci o modifica a mano la descrizione di questo scatto "
                               "(sostituisce quella del VLM)."));
    connect(editDescBtn, &QPushButton::clicked, this, &Vision3DWidget::onEditShotDescriptionClicked);
    descRowLay->addWidget(editDescBtn, 0, Qt::AlignTop);
    grid->addWidget(descRow, 5,0,1,3);
    m_lastSensorInfo = new QLabel;
    m_lastSensorInfo->setObjectName("v3dLastSensorInfo");
    m_lastSensorInfo->setWordWrap(true);
    m_lastSensorInfo->setTextFormat(Qt::RichText);
    grid->addWidget(m_lastSensorInfo, 6,0,1,3);
    colBLay->addWidget(prevBox);
    colBLay->addStretch();

    leftLay->addWidget(colA, 1);
    leftLay->addWidget(colB, 1);

    /* colonna scrollabile: con finestre basse niente sovrapposizioni */
    prepScroll->setWidget(leftCol);
    tabs->addTab(prepScroll, "\xf0\x9f\x93\xb7 Preparazione");

    /* ═══ Tab 2: Assembla punti e texture (scena 3D + ricostruzione) ═══ */
    auto* rightCol = new QWidget;
    auto* rightLay = new QVBoxLayout(rightCol);
    rightLay->setContentsMargins(dpiScale(8), dpiScale(8), dpiScale(8), dpiScale(8));

    auto* sceneBox = new QGroupBox("Scena 3D — posizione delle camere");
    auto* sceneLay = new QVBoxLayout(sceneBox);
    m_scene = new Vision3DSceneCanvas;
    m_scene->setObjectName("v3dScene");
    sceneLay->addWidget(m_scene, 1);

    /* posa manuale: quando la bussola non dà rotazioni diverse (o sbaglia),
       l'angolo/inclinazione dello scatto selezionato si impostano a mano.
       Persistite nel sidecar .json → rieditabili anche dopo un riavvio. */
    auto* poseRow = new QHBoxLayout;
    poseRow->addWidget(new QLabel("Posa manuale \xe2\x80\x94 Angolo:"));
    m_poseHeadSpin = new QSpinBox;
    m_poseHeadSpin->setObjectName("v3dPoseHead");
    m_poseHeadSpin->setRange(0, 359);
    m_poseHeadSpin->setWrapping(true);              // 359° + 1 = 0°
    m_poseHeadSpin->setSuffix("\xc2\xb0");
    m_poseHeadSpin->setEnabled(false);              // finché nessuno scatto è selezionato
    m_poseHeadSpin->setToolTip("Posizione sul cerchio (bussola) dello scatto selezionato.\n"
                               "Seleziona uno scatto in galleria, poi regola qui.");
    connect(m_poseHeadSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &Vision3DWidget::onPoseSpinChanged);
    poseRow->addWidget(m_poseHeadSpin);
    poseRow->addWidget(new QLabel("Inclin.:"));
    m_posePitchSpin = new QSpinBox;
    m_posePitchSpin->setObjectName("v3dPosePitch");
    m_posePitchSpin->setRange(-90, 90);
    m_posePitchSpin->setSuffix("\xc2\xb0");
    m_posePitchSpin->setEnabled(false);
    m_posePitchSpin->setToolTip("Inclinazione del telefono (quota della camera nella scena).");
    connect(m_posePitchSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &Vision3DWidget::onPoseSpinChanged);
    poseRow->addWidget(m_posePitchSpin);
    m_distribBtn = new QPushButton("\xe2\x86\x94 Distribuisci sul cerchio");   /* ↔ */
    m_distribBtn->setObjectName("v3dDistribBtn");
    m_distribBtn->setToolTip("Assegna a TUTTI gli scatti della sessione angoli equidistanti\n"
                             "(utile quando la bussola ha registrato sempre lo stesso valore).\n"
                             "Poi puoi correggere i singoli scatti con le caselle qui accanto.");
    connect(m_distribBtn, &QPushButton::clicked, this, &Vision3DWidget::onDistributeClicked);
    poseRow->addWidget(m_distribBtn);
    poseRow->addStretch();
    sceneLay->addLayout(poseRow);
    rightLay->addWidget(sceneBox, 1);

    auto* reconBox = new QGroupBox("Ricostruzione 3D (fotogrammetria)");
    auto* reconLay = new QVBoxLayout(reconBox);
    auto* sesRow = new QHBoxLayout;
    sesRow->addWidget(new QLabel("Sessione:"));
    m_sessionCombo = new QComboBox;
    m_sessionCombo->setObjectName("v3dSessionCombo");
    m_sessionCombo->setToolTip(kSessionComboTip);
    connect(m_sessionCombo, &QComboBox::currentTextChanged,
            this, &Vision3DWidget::onSessionComboChanged);
    sesRow->addWidget(m_sessionCombo, 1);
    sesRow->addWidget(new QLabel("Qualit\xc3\xa0:"));
    m_qualityCombo = new QComboBox;
    m_qualityCombo->addItems({"low", "medium", "high"});
    m_qualityCombo->setCurrentIndex(1);
    m_qualityCombo->setToolTip("Foto minime consigliate: low \xe2\x89\xa5 10, medium \xe2\x89\xa5 20, "
                               "high \xe2\x89\xa5 40.\nPi\xc3\xb9 foto (con sovrapposizione) = "
                               "nuvola pi\xc3\xb9 densa e completa.");
    connect(m_qualityCombo, &QComboBox::currentTextChanged,
            this, &Vision3DWidget::onQualityComboChanged);
    sesRow->addWidget(m_qualityCombo);
    m_photoReqLabel = new QLabel;
    m_photoReqLabel->setObjectName("v3dPhotoReq");
    m_photoReqLabel->setToolTip("Foto nella sessione / minimo consigliato per la qualit\xc3\xa0 scelta");
    sesRow->addWidget(m_photoReqLabel);
    reconLay->addLayout(sesRow);
    auto* reconBtnRow = new QHBoxLayout;
    m_reconBtn = new QPushButton("\xf0\x9f\xa7\x8a Crea nuvola di punti (COLMAP)");  /* 🧊 */
    m_reconBtn->setObjectName("v3dReconBtn");
    connect(m_reconBtn, &QPushButton::clicked, this, &Vision3DWidget::onReconStartClicked);
    reconBtnRow->addWidget(m_reconBtn, 1);
    auto* recheckBtn = new QPushButton("\xe2\x9f\xb3 Ricontrolla");   /* ⟳ */
    recheckBtn->setObjectName("v3dRecheckBtn");
    recheckBtn->setToolTip("Riverifica COLMAP a caldo (eseguibile + librerie), senza\n"
                           "riavviare Prismalux: premilo dopo un 'sudo apt install'.");
    connect(recheckBtn, &QPushButton::clicked, this, &Vision3DWidget::onReconRecheckClicked);
    reconBtnRow->addWidget(recheckBtn);
    reconLay->addLayout(reconBtnRow);
    m_reconHint = new QLabel;
    m_reconHint->setObjectName("v3dReconHint");
    m_reconHint->setWordWrap(true);
    reconLay->addWidget(m_reconHint);
    updateReconHint();          // sonda subito exe + librerie (aggiornabile a caldo)
    rightLay->addWidget(reconBox);
    tabs->addTab(rightCol, "\xf0\x9f\xa7\xb1 Assembla punti e texture");

    root->addWidget(tabs, 1);   // stretch: lo spazio extra va alle tab

    /* Nessun pannello "Log:" locale: tutti gli eventi Vision3D (server,
       scatti, ricostruzione — via appendLog()) vanno nel log centralizzato
       (finestra "Messaggi" → tab "3D"), per non occupare spazio di lavoro
       con un'altra vista di quello che è già visibile altrove. */
    LogBus::post("\xf0\x9f\x93\xb7 Vision3D pronto. Avvia il server e apri l'URL da OGNI telefono (ognuno riceve un ID).", "3d");
    LogBus::post("\xf0\x9f\x93\xb7 Vision3D — bersagli ArUco stampabili (scala reale): " + P::root() + "/Tools/aruco/", "3d");
}

// ---------------------------------------------------------------------------
QByteArray Vision3DWidget::htmlPage(const QString& token)
{
    static const char* html = R"HTML(<!DOCTYPE html>
<html lang="it"><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
<title>Prismalux Vision3D</title>
<style>
:root{--bg:#0d1117;--panel:#161b22;--acc:#3fb0ff;--acc2:#7c5cff;--txt:#e6edf3;--mut:#8b949e;--ok:#3fb950;}
*{box-sizing:border-box;margin:0;padding:0;-webkit-tap-highlight-color:transparent;}
body{background:var(--bg);color:var(--txt);font-family:system-ui,-apple-system,sans-serif;min-height:100vh;}
header{padding:12px 16px;background:linear-gradient(135deg,var(--acc2),var(--acc));font-weight:700;}
header small{display:block;font-weight:400;font-size:.7rem;opacity:.85;margin-top:2px;}
.wrap{padding:14px;max-width:720px;margin:0 auto;}
.card{background:var(--panel);border:1px solid #21262d;border-radius:14px;padding:14px;margin-bottom:12px;}
label{font-size:.78rem;color:var(--mut);display:block;margin-bottom:5px;}
input[type=text]{width:100%;padding:10px;background:#0d1117;border:1px solid #30363d;border-radius:8px;color:var(--txt);font-size:.95rem;}
video{width:100%;border-radius:12px;background:#000;aspect-ratio:3/4;object-fit:cover;display:none;}
canvas{display:none;}
button{border:none;border-radius:10px;padding:13px 16px;font-size:.95rem;font-weight:600;cursor:pointer;color:#fff;}
button:active{transform:scale(.97);}
.b-shoot{background:linear-gradient(135deg,var(--acc),var(--acc2));position:fixed;left:14px;right:14px;bottom:14px;z-index:80;font-size:1.05rem;padding:15px;box-shadow:0 4px 20px rgba(0,0,0,.5);}
.wrap.cam-active{padding-bottom:90px;}
.b-alt{background:#21262d;}
.b-alt.on{background:var(--acc);color:#04121d;}
.chips{display:flex;gap:8px;flex-wrap:wrap;}
.chip,.tchip,.mchip{background:#0d1117;border:1px solid #30363d;border-radius:20px;padding:8px 14px;font-size:.82rem;cursor:pointer;}
.chip.on,.tchip.on,.mchip.on{border-color:var(--acc);background:#132a3d;color:var(--acc);}
.result{margin-top:10px;}
.result h4{font-size:.8rem;color:var(--acc);margin-bottom:6px;text-transform:uppercase;letter-spacing:.5px;}
.desc{background:#0d1117;border:1px solid #30363d;border-radius:8px;padding:12px;font-size:.92rem;line-height:1.5;}
.imgs{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-top:8px;}
.imgs figure{border:1px solid #30363d;border-radius:8px;overflow:hidden;}
.imgs img{width:100%;display:block;}
.imgs figcaption{font-size:.72rem;color:var(--mut);padding:4px 8px;text-align:center;}
.spinner{display:inline-block;width:16px;height:16px;border:2px solid #30363d;border-top-color:var(--acc);border-radius:50%;animation:spin .7s linear infinite;vertical-align:middle;}
@keyframes spin{to{transform:rotate(360deg);}}
.toast{position:fixed;bottom:16px;left:50%;transform:translateX(-50%);background:var(--ok);color:#000;padding:9px 18px;border-radius:20px;font-weight:600;opacity:0;transition:.3s;pointer-events:none;font-size:.88rem;}
.toast.show{opacity:1;}
.hint{font-size:.76rem;color:var(--mut);line-height:1.5;margin-top:6px;}
.devbadge{display:inline-block;background:#132a3d;border:1px solid var(--acc);color:var(--acc);padding:3px 10px;border-radius:20px;font-size:.78rem;}
</style></head><body>
<header>&#9670; PRISMALUX Vision3D <small>Client iOS, Android e Desktop &#8212; un unico progetto</small></header>
<div class="wrap">
<div class="card" style="display:flex;justify-content:space-between;align-items:center;">
<div><label style="margin:0">Questo dispositivo</label><span class="devbadge" id="devId">assegnazione&#8230;</span></div>
</div>
<div class="card"><label>Nome sessione (uguale su tutti i telefoni!)</label><input type="text" id="session" value="scan1"></div>
<div class="card"><label>Cosa stai scansionando</label>
<div class="chips" id="modeChips">
<div class="mchip on" data-m="object">&#127919; Oggetto (giro io intorno)</div>
<div class="mchip" data-m="scene">&#127754; Scena (giro su me stesso)</div>
</div>
<div class="hint">Oggetto: l'oggetto resta fermo al centro, sei tu a girargli intorno. Scena: resti fermo in un punto e ruoti te/il telefono per inquadrare intorno (paesaggio, ambiente).</div></div>
<div class="card"><label>Cosa calcolare</label>
<div class="chips">
<div class="chip on" data-w="desc">&#129504; Descrizione (VLM)</div>
<div class="chip on" data-w="boxes">&#9634; Box (OpenCV)</div>
<div class="chip on" data-w="depth">&#127752; Depth</div>
<div class="chip on" data-w="edges">&#9979; Bordi</div>
<div class="chip" data-w="bump">&#9968; Bump map</div>
<div class="chip" data-w="normal">&#127752; Normal map</div>
</div>
<div class="hint">Usa lo <b>stesso nome sessione</b> su ogni telefono per unire gli scatti nello stesso progetto. Le foto di ognuno finiscono in una sottocartella per device.</div></div>
<div class="card"><label>Test riflessione superficie</label>
<div class="chips"><div class="chip" id="flashChip">&#128293; Test flash a ogni scatto</div></div>
<div class="hint" id="flashHint">Accende brevemente il flash prima dello scatto: se la foto risulta molto più chiara/con riflessi, la superficie è probabilmente riflettente o trasparente. Richiede un telefono/browser che supporti il flash come torcia (non tutti lo fanno).</div></div>
<div class="card">
<div style="position:relative;"><video id="video" autoplay playsinline muted></video>
<canvas id="targetRing" width="300" height="400" style="display:none;position:absolute;inset:0;width:100%;height:100%;pointer-events:none;"></canvas>
</div><canvas id="canvas"></canvas>
<div class="chips" style="margin-top:10px;">
<button class="b-alt" id="startCam" style="flex:1;">&#128247; Attiva fotocamera</button>
<button class="b-alt" id="flipCam" style="display:none;">&#128260;</button>
<button class="b-alt" id="enableSensor" style="display:none;">&#129517; Sensori</button>
<button class="b-alt" id="autoShoot" style="display:none;">&#129302; Auto OFF</button></div>
<div id="sensorReadout" class="hint" style="display:none;">Orientamento: <b id="soAngle">--</b>&#176; &nbsp;|&nbsp; inclinazione <b id="soPitch">--</b>&#176;</div>
<div id="sensorWarn" class="hint" style="display:none;color:#f85149;"></div>
<div id="motionWarn" class="hint" style="display:none;color:#f85149;font-weight:700;">&#128241; Movimento rilevato &mdash; tieni fermo il telefono per uno scatto nitido</div>
<div id="targetChips" class="chips" style="display:none;margin-top:8px;">
<div class="tchip" data-q="low">&#127919; Pochi (10)</div>
<div class="tchip on" data-q="medium">&#127919; Medio (20)</div>
<div class="tchip" data-q="high">&#127919; Tanti (40)</div>
</div>
<div class="hint" id="targetHint" style="display:none;">La sfera sovrapposta all'oggetto mostra i punti da coprire (pieno = gi&#224; scattato). L'anello acceso segue l'inclinazione del telefono (accelerometro), i puntini seguono la bussola. Con &#129302; <b>Auto ON</b> (richiede Sensori attivi) scatta da sola quando ti allinei con un pallino scoperto ruotando a destra o sinistra.</div>
<div id="rotateHint" style="display:none;font-weight:700;text-align:center;margin-top:6px;padding:8px;border-radius:8px;background:#0d1117;"></div>
<button class="b-shoot" id="shoot" style="display:none;">SCATTA E ANALIZZA</button></div>
<div class="card"><label>Modello 3D (dopo la ricostruzione sul PC)</label>
<div class="chips">
<button class="b-alt" id="dlObj" style="flex:1;">&#11015; OBJ</button>
<button class="b-alt" id="dlMtl" style="flex:1;">&#11015; MTL</button>
<button class="b-alt" id="dlPly" style="flex:1;">&#11015; PLY</button></div>
<div class="hint">Scarica la nuvola di punti colorata della sessione corrente: OBJ+MTL (per Blender/MeshLab) oppure PLY (CloudCompare).</div></div>
<div class="card" id="resultCard" style="display:none;">
<div id="status" style="font-size:.85rem;color:var(--mut);margin-bottom:8px;"></div>
<div class="result" id="descBox" style="display:none;"><h4>Cos'&#232; (VLM)</h4><div class="desc" id="descText"></div></div>
<div class="imgs" id="imgBox"></div></div>
</div>
<div class="toast" id="toast"></div>
<script>
const TOKEN="__VISION3D_TOKEN__";
const $=id=>document.getElementById(id);
const video=$('video'),canvas=$('canvas'),toast=$('toast');
let stream=null,facing='environment',wants={desc:true,boxes:true,depth:true,edges:true,bump:false,normal:false};
let flashTestOn=false,flashWarned=false;
// "object" (l'oggetto resta fermo, io giro intorno) o "scene" (io resto
// fermo, giro su me stesso per inquadrare intorno) — scelto PRIMA di
// scattare, salvato in ogni sidecar .json per uso futuro in ricostruzione.
let scanMode='object';
document.querySelectorAll('#modeChips .mchip').forEach(c=>{c.onclick=()=>{
  document.querySelectorAll('#modeChips .mchip').forEach(x=>x.classList.remove('on'));
  c.classList.add('on'); scanMode=c.dataset.m; buildTargets();
};});
// --- sensori di movimento (giroscopio/bussola) ---
let sensorOn=false, curHeading=null, curPitch=null, curRoll=null;
// Accelerometro/giroscopio grezzi (DeviceMotion) + altitudine (Geolocation
// GPS, livello del mare — il browser non ha un sensore di altezza-da-terra).
// Tutti best-effort: restano null se il device/permesso non li fornisce,
// e vengono comunque inviati così (l'assenza è un dato in sé).
let curAccel=null, curAccelG=null, curRotRate=null;
let curAltitude=null, curAltAccuracy=null, curLat=null, curLon=null;
let geoWatchId=null;
function round2(n){ return n==null?null:Math.round(n*100)/100; }
// --- rilevamento movimento (avviso, non blocco — vedi doShoot/checkAutoShoot) ---
// Soglie empiriche, da calibrare sul campo: accelerazione lineare in m/s²
// (già senza gravità se il device la fornisce, altrimenti stimata dallo
// scarto di accelerationIncludingGravity da 9.81), velocità di rotazione
// in gradi/secondo (unità nativa di DeviceMotionEvent.rotationRate).
const MOTION_ACCEL_THRESH = 1.5;
const MOTION_ROT_THRESH = 25;
let motionStable=true, motionMagnitude=0;
function updateMotionUi(){
  const w=$('motionWarn');
  if(w) w.style.display = (sensorOn && !motionStable) ? 'block' : 'none';
}
function onMotion(e){
  if(e.acceleration && e.acceleration.x!=null)
    curAccel = {x:round2(e.acceleration.x), y:round2(e.acceleration.y), z:round2(e.acceleration.z)};
  if(e.accelerationIncludingGravity && e.accelerationIncludingGravity.x!=null)
    curAccelG = {x:round2(e.accelerationIncludingGravity.x), y:round2(e.accelerationIncludingGravity.y), z:round2(e.accelerationIncludingGravity.z)};
  if(e.rotationRate && e.rotationRate.alpha!=null)
    curRotRate = {alpha:round2(e.rotationRate.alpha), beta:round2(e.rotationRate.beta), gamma:round2(e.rotationRate.gamma)};
  // Magnitudo accelerazione lineare: preferisci "acceleration" (già senza
  // gravità); se il device non la fornisce, stima dallo scarto rispetto a
  // 9.81 m/s² del vettore con gravità inclusa (fermo e in piano = ~9.81).
  let accelMag = 0;
  if(curAccel) accelMag = Math.sqrt(curAccel.x**2 + curAccel.y**2 + curAccel.z**2);
  else if(curAccelG) accelMag = Math.abs(Math.sqrt(curAccelG.x**2 + curAccelG.y**2 + curAccelG.z**2) - 9.81);
  const rotMag = curRotRate ? Math.sqrt(curRotRate.alpha**2 + curRotRate.beta**2 + curRotRate.gamma**2) : 0;
  motionMagnitude = round2(Math.max(accelMag, 0));
  const wasStable = motionStable;
  motionStable = accelMag <= MOTION_ACCEL_THRESH && rotMag <= MOTION_ROT_THRESH;
  if(wasStable !== motionStable) updateMotionUi();
}
function onGeoSuccess(pos){
  curAltitude=pos.coords.altitude; curAltAccuracy=pos.coords.altitudeAccuracy;
  curLat=pos.coords.latitude; curLon=pos.coords.longitude;
}
function onGeoError(){ /* silenzioso: l'altitudine resta non disponibile, non blocca il resto */ }
// --- sfera-guida (overlay sovrapposto alla ripresa): bersagli da coprire ---
// Modalità "object" (oggetto fermo, si gira intorno): N anelli orizzontali
// (bande di inclinazione, come le fasce di quota del visualizzatore 3D sul
// PC), l'anello "acceso" è quello che corrisponde all'inclinazione attuale
// del telefono (curPitch), i puntini seguono la bussola (curHeading).
// Modalità "scene" (si resta fermi, si gira su se stessi): un solo anello
// a 360° completo — non ha senso orbitare in quota intorno a un paesaggio,
// la copertura utile è tutto il giro d'orizzonte da un punto fisso.
// Indicativo, non un calcolo di posa reale — stesso principio della scena
// 3D lato PC.
let targetQuality='medium', targetRings=[];
function buildTargets(){
  const nRings  = scanMode==='scene' ? 1
                 : targetQuality==='low' ? 1 : targetQuality==='high' ? 3 : 2;
  const total   = targetQuality==='low' ? 10 : targetQuality==='high' ? 40 : 20;
  const perRing = Math.ceil(total / nRings);
  targetRings = Array.from({length:nRings}, () => ({
    headings: Array.from({length:perRing}, (_,i)=>({deg:360*i/perRing, covered:false}))
  }));
  const th=$('targetHint');
  if(th) th.innerHTML = (scanMode==='scene'
    ? 'Un solo giro a 360&#176; intorno a te (pieno = gi&#224; scattato): resta fermo e ruota su te stesso.'
    : 'La sfera sovrapposta all\'oggetto mostra i punti da coprire (pieno = gi&#224; scattato), su pi&#249; fasce di quota mentre gli giri intorno.')
    + ' L\'anello acceso segue l\'inclinazione del telefono (accelerometro), i puntini seguono la bussola. <b>Ogni puntino ha un numero</b>: parti dal <b>punto 1</b>, quello pi&#249; vicino a te, e segui l\'ordine indicato in basso ("ruota verso il punto N") finch&#233; non li accendi tutti di verde. Con &#129302; <b>Auto ON</b> (richiede Sensori attivi) scatta da sola quando ti allinei con un pallino scoperto ruotando a destra o sinistra.';
  drawTargetSphere();
}
// Fascia attiva in base all'inclinazione attuale: indicativo (non calibrato),
// stessa idea del beta del giroscopio usato per il pitch nel sidecar .json.
function activeRingIndex(){
  if(targetRings.length<=1) return 0;
  const t = Math.max(-60, Math.min(60, curPitch==null?0:curPitch));
  const idx = Math.floor(((t+60)/120) * targetRings.length);
  return Math.max(0, Math.min(targetRings.length-1, idx));
}
function markCovered(heading){
  if(heading==null || !targetRings.length) return;
  const ring = targetRings[activeRingIndex()];
  let best=null, bestDiff=999;
  ring.headings.forEach(t=>{
    const diff = Math.abs(((t.deg-heading+540)%360)-180);
    if(diff<bestDiff){bestDiff=diff; best=t;}
  });
  const tol = (360/ring.headings.length)/2 + 6;
  if(best && bestDiff<=tol) best.covered=true;
}
function drawTargetSphere(){
  const c=$('targetRing'); if(!c) return;
  const ctx=c.getContext('2d'), w=c.width, h=c.height;
  ctx.clearRect(0,0,w,h);
  // ellisse volutamente piatta (ry molto minore di rx): anello ben
  // orizzontale, non un cerchio pieno.
  const cx=w/2, cy=h*0.46, rx=w*0.40, ry=rx*0.24;
  const active=activeRingIndex();
  // Rotazione del disegno legata al rollio reale del telefono (gamma
  // dell'accelerometro, curRoll): niente giro autonomo/infinito — l'anello
  // segue di poco quanto il telefono viene inclinato lateralmente, come una
  // bolla/orizzonte artificiale. Solo estetica: NON entra nel confronto con
  // curHeading/t.deg usato da markCovered/activeRingIndex, quindi bersagli
  // e lancetta restano allineati fra loro mentre ruotano insieme.
  const rollTilt = curRoll!=null ? Math.max(-45, Math.min(45, curRoll)) : 0;
  // Fase dell'anello: PHASE_DEG=0 → bussola=0° proietta a destra
  // (cos0,sin0)=(1,0). Prima c'era un -90° che mandava bussola=0° in alto;
  // rimuovendolo (equivalente a +90° extra rispetto a prima) tutto lo
  // schema ruota di 90° in senso orario, così il punto di partenza cade
  // in basso — vicino a chi tiene il telefono, dove punta la freccia —
  // invece che verso l'alto/lontano dall'inquadratura.
  const PHASE_DEG = 0;
  // Quanto manca (gradi, con verso) e QUALE bersaglio numerato è il
  // prossimo scoperto sull'anello attivo — null se non c'è bussola o è già
  // tutto coperto.
  const nearestTarget = curHeading!=null ? nearestUncoveredTarget() : null;
  const nearestDiff   = nearestTarget ? nearestTarget.diff : null;
  const readyToShoot  = nearestDiff!=null && Math.abs(nearestDiff) <= 8;
  targetRings.forEach((ring,r)=>{
    const spread  = targetRings.length>1 ? (r-(targetRings.length-1)/2) : 0;
    const bandCy  = cy + spread*ry*1.9;
    const isActive= r===active;
    ctx.strokeStyle = isActive ? 'rgba(63,176,255,.9)' : 'rgba(255,255,255,.20)';
    ctx.lineWidth = isActive?2.2:1;
    ctx.beginPath(); ctx.ellipse(cx,bandCy,rx,ry,0,0,Math.PI*2); ctx.stroke();
    ring.headings.forEach((t,i)=>{
      const rad=(t.deg+rollTilt+PHASE_DEG)*Math.PI/180;
      const x=cx+rx*Math.cos(rad), y=bandCy+ry*Math.sin(rad);
      ctx.beginPath(); ctx.arc(x,y,isActive?5:3.5,0,Math.PI*2);
      ctx.fillStyle = t.covered ? '#3fb950' : (isActive?'rgba(255,255,255,.85)':'rgba(255,255,255,.25)');
      ctx.fill();
      if(!t.covered && isActive){ ctx.strokeStyle='#8b949e'; ctx.lineWidth=1; ctx.stroke(); }
      /* Numero del punto (1 = quello da cui si parte, in basso vicino a chi
         tiene il telefono — stesso riferimento di PHASE_DEG sopra), SOLO
         sull'anello attivo: aiuta chi non ha capito il "giro" a sapere
         esattamente quale pallino sta inseguendo, non solo "un pallino
         qualsiasi ancora spento". Alone nero per leggerlo sopra qualunque
         sfondo ripreso dalla fotocamera. */
      if(isActive){
        const ly = y + (Math.sin(rad)>=0 ? 14 : -14);
        ctx.font='bold 11px system-ui,sans-serif';
        ctx.textAlign='center'; ctx.textBaseline='middle';
        ctx.lineWidth=3; ctx.strokeStyle='rgba(0,0,0,.85)';
        ctx.strokeText(String(i+1), x, ly);
        ctx.fillStyle = t.covered ? '#3fb950' : '#e6edf3';
        ctx.fillText(String(i+1), x, ly);
      }
    });
    if(isActive && curHeading!=null){
      const rad=(curHeading+rollTilt+PHASE_DEG)*Math.PI/180;
      // verde = sei allineato con un bersaglio scoperto, è il momento di
      // scattare; blu = devi ancora ruotare per arrivarci.
      ctx.fillStyle = readyToShoot ? '#3fb950' : '#3fb0ff';
      ctx.beginPath(); ctx.arc(cx+(rx+16)*Math.cos(rad), bandCy+(ry+16)*Math.sin(rad),
                               readyToShoot?6:4.5, 0, Math.PI*2); ctx.fill();
    }
  });
  const done=targetRings.reduce((s,rg)=>s+rg.headings.filter(t=>t.covered).length,0);
  const tot =targetRings.reduce((s,rg)=>s+rg.headings.length,0);
  ctx.fillStyle='#e6edf3'; ctx.font='bold 15px system-ui,sans-serif';
  ctx.textAlign='center'; ctx.textBaseline='alphabetic';
  ctx.fillText(done+'/'+tot, cx, h*0.07);

  /* Istruzione "quando scattare" disegnata DENTRO l'inquadratura (non in un
     testo separato sotto, che mentre guardi il mirino non si vede): grande,
     leggibile, colorata come il puntino sopra. */
  if(curHeading!=null){
    ctx.font='bold 22px system-ui,sans-serif';
    ctx.textAlign='center'; ctx.textBaseline='alphabetic';
    if(nearestDiff==null){
      ctx.fillStyle='#3fb950';
      ctx.fillText('✅ fascia completata', cx, h*0.90);
    } else if(readyToShoot){
      ctx.fillStyle='#3fb950';
      ctx.fillText('✅ SCATTA ORA! (punto '+nearestTarget.idx+')', cx, h*0.90);
    } else {
      ctx.fillStyle='#3fb0ff';
      const arrow = nearestDiff>0 ? '↻' : '↺';
      ctx.fillText(arrow+' punto '+nearestTarget.idx+': ruota di '+Math.round(Math.abs(nearestDiff))+'\xb0', cx, h*0.90);
    }
  }
}
document.querySelectorAll('#targetChips .tchip').forEach(c=>{c.onclick=()=>{
  document.querySelectorAll('#targetChips .tchip').forEach(x=>x.classList.remove('on'));
  c.classList.add('on'); targetQuality=c.dataset.q; buildTargets();
};});
buildTargets();
// gotOrientData: true solo alla prima lettura con un valore REALE (non
// solo l'evento vuoto che alcuni browser sparano comunque, es. Brave con
// gli Shield sensori attivi — vedi watchdog in enableSensors()).
let gotOrientData=false;
function onOrient(e){
  let a = e.webkitCompassHeading!=null ? e.webkitCompassHeading : (e.alpha!=null?360-e.alpha:null);
  curHeading = a!=null ? Math.round(((a%360)+360)%360) : null;
  curPitch = e.beta!=null ? Math.round(e.beta) : null;
  curRoll  = e.gamma!=null ? Math.round(e.gamma) : null;
  if(curHeading!=null || curPitch!=null){
    gotOrientData=true;
    $('sensorWarn').style.display='none';
  }
  if(sensorOn){
    $('soAngle').textContent = curHeading!=null?curHeading:'--';
    $('soPitch').textContent = curPitch!=null?curPitch:'--';
    drawTargetSphere();
    checkAutoShoot();
    updateRotateHint();
  }
}
// Testo-guida: quanti gradi mancano (e in che verso) per arrivare al
// prossimo bersaglio scoperto sull'anello attivo — risponde a "ruota di
// tot gradi per prendere l'oggetto girandoci intorno".
/* Come nearestUncoveredDiff, ma restituisce anche l'indice (1-based, stesso
   numero disegnato sul pallino da drawTargetSphere) del bersaglio più
   vicino ancora scoperto — per dire "Punto 7", non solo "ruota di N°". */
function nearestUncoveredTarget(){
  if(!targetRings.length || curHeading==null) return null;
  const ring = targetRings[activeRingIndex()];
  let bestAbs=999, bestSigned=null, bestIdx=null;
  ring.headings.forEach((t,i)=>{
    if(t.covered) return;
    const diff = ((t.deg - curHeading + 540) % 360) - 180;   // firmato, (-180,180]
    if(Math.abs(diff) < bestAbs){ bestAbs=Math.abs(diff); bestSigned=diff; bestIdx=i+1; }
  });
  return bestSigned==null ? null : {diff:bestSigned, idx:bestIdx};
}
function nearestUncoveredDiff(){
  const t = nearestUncoveredTarget();
  return t ? t.diff : null;
}
function updateRotateHint(){
  const el=$('rotateHint'); if(!el) return;
  if(!targetRings.length || curHeading==null){ el.style.display='none'; return; }
  const target = nearestUncoveredTarget();
  if(target==null){
    el.textContent='✅ Fascia corrente completata — cambia inclinazione o passa oltre.';
    el.style.color='#3fb950';
  } else if(Math.abs(target.diff) <= 8){
    el.textContent='✅ Punto '+target.idx+' pronto — scatta!';
    el.style.color='#3fb950';
  } else {
    el.textContent=(target.diff>0 ? '↻ Ruota in senso orario di ' : '↺ Ruota in senso antiorario di ')
                   + Math.round(Math.abs(target.diff)) + '\xb0 verso il punto '+target.idx;
    el.style.color='#3fb0ff';
  }
  el.style.display='block';
}
// Rileva il browser per dare l'istruzione giusta (i menu di permessi
// cambiano nome/posto da un browser all'altro). Brave non si annuncia
// nello User-Agent (per anti-fingerprinting) — si riconosce solo tramite
// l'oggetto navigator.brave che espone lui stesso.
async function detectBrowserName(){
  if(navigator.brave && typeof navigator.brave.isBrave==='function'){
    try{ if(await navigator.brave.isBrave()) return 'brave'; }catch(e){}
  }
  const ua = navigator.userAgent || '';
  if(/SamsungBrowser/i.test(ua)) return 'samsung';
  if(/EdgA|Edge|Edg\//i.test(ua)) return 'edge';
  if(/OPR|Opera/i.test(ua))      return 'opera';
  if(/Firefox|FxiOS/i.test(ua))  return 'firefox';
  if(/Chrome|CriOS/i.test(ua))   return 'chrome';
  if(/Safari/i.test(ua))         return 'safari';
  return 'unknown';
}
function sensorBlockedHint(browser){
  const generic = 'tocca l’icona del lucchetto o della (i) accanto all’indirizzo &#8594; '+
    'Autorizzazioni/Permessi del sito &#8594; cerca "Sensori di movimento" e verifica che non sia bloccato.';
  const perBrowser = {
    brave:   'Su <b>Brave</b>: tocca l’icona del leone \u{1F981} nella barra indirizzo &#8594; Impostazioni sito &#8594; assicurati che "Sensori" non sia bloccato.',
    chrome:  'Su <b>Chrome</b>: '+generic,
    samsung: 'Su <b>Samsung Internet</b>: '+generic,
    edge:    'Su <b>Edge</b>: '+generic,
    opera:   'Su <b>Opera</b>: '+generic,
    firefox: 'Su <b>Firefox</b>: apri <code>about:config</code>, cerca "sensors" e verifica che <code>device.sensors.enabled</code> sia <code>true</code>.',
    safari:  'Su <b>Safari</b>: Impostazioni iOS &#8594; Safari &#8594; Motion e orientamento &#8594; deve essere consentito per questo sito.',
    unknown: generic
  };
  return perBrowser[browser] || generic;
}
async function enableSensors(){
  if(sensorOn) return;   // idempotente: startCamera() la richiama ad ogni avvio/flip
  if(typeof DeviceOrientationEvent!=='undefined' && typeof DeviceOrientationEvent.requestPermission==='function'){
    try{ const p=await DeviceOrientationEvent.requestPermission(); if(p!=='granted'){showToast('Permesso sensori negato','#d29922');return;} }
    catch(e){ showToast('Sensori non disponibili','#d29922'); return; }
  }
  if(!window.DeviceOrientationEvent){ showToast('Nessun sensore su questo device','#d29922'); return; }
  // 'deviceorientationabsolute' (Chrome/Android) dà spesso una bussola più
  // affidabile del semplice 'deviceorientation'; li registriamo entrambi
  // sullo stesso handler, tenendo l'ultimo valore che arriva.
  window.addEventListener('deviceorientationabsolute', onOrient, true);
  window.addEventListener('deviceorientation', onOrient, true);

  // Accelerometro/giroscopio grezzi (DeviceMotion) — permesso SEPARATO da
  // DeviceOrientation su iOS 13+; se negato o non disponibile non blocca
  // il resto, semplicemente quei campi restano vuoti negli scatti.
  if(typeof DeviceMotionEvent!=='undefined' && typeof DeviceMotionEvent.requestPermission==='function'){
    try{
      const pm=await DeviceMotionEvent.requestPermission();
      if(pm==='granted') window.addEventListener('devicemotion', onMotion, true);
    }catch(e){ /* accelerometro grezzo non disponibile su questo device */ }
  } else if(window.DeviceMotionEvent){
    window.addEventListener('devicemotion', onMotion, true);
  }

  // Altitudine (Geolocation GPS) — anche questa best-effort e non bloccante.
  if(navigator.geolocation)
    geoWatchId = navigator.geolocation.watchPosition(onGeoSuccess, onGeoError, {enableHighAccuracy:true, maximumAge:5000});

  sensorOn=true; $('enableSensor').textContent='\u{1F9ED} Attivi'; $('sensorReadout').style.display='block';
  $('autoShoot').style.display='block';
  setTimeout(async()=>{
    if(!gotOrientData){
      const browser = await detectBrowserName();
      $('sensorWarn').style.display='block';
      $('sensorWarn').innerHTML='&#9888; Nessun dato dai sensori dopo qualche secondo — il browser probabilmente li blocca. '+
        sensorBlockedHint(browser)+
        ' Se il problema persiste, muovi/ruota il telefono un momento: alcuni device iniziano a inviare dati solo dopo il primo movimento.';
    }
  }, 3000);
  showToast('Sensori attivi ✓');
}
// Spegne davvero i sensori (non solo l'indicatore): rimuove i listener,
// interrompe il GPS (clearWatch) e nasconde tutta l'interfaccia che dipende
// da sensorOn. Richiamata da stopCamera() — senza fotocamera i sensori non
// servono a niente, restare accesi era solo consumo di batteria "invisibile"
// (il pulsante diceva "Attiva fotocamera" ma bussola/inclinazione/Auto
// continuavano ad aggiornarsi lo stesso).
function disableSensors(){
  window.removeEventListener('deviceorientationabsolute', onOrient, true);
  window.removeEventListener('deviceorientation', onOrient, true);
  window.removeEventListener('devicemotion', onMotion, true);
  if(geoWatchId!=null && navigator.geolocation){ navigator.geolocation.clearWatch(geoWatchId); geoWatchId=null; }
  sensorOn=false; gotOrientData=false;
  curHeading=null; curPitch=null; curRoll=null;
  curAccel=null; curAccelG=null; curRotRate=null;
  curAltitude=null; curAltAccuracy=null; curLat=null; curLon=null;
  motionStable=true; motionMagnitude=0; updateMotionUi();
  autoShoot=false; autoArmed=true;
  $('enableSensor').textContent='\u{1F9ED} Sensori';
  $('sensorReadout').style.display='none';
  $('sensorWarn').style.display='none';
  $('autoShoot').style.display='none';
  $('autoShoot').classList.remove('on');
  $('autoShoot').textContent='\u{1F916} Auto OFF';
}
// mostra il proprio deviceId dal cookie (impostato dal server)
function readDevId(){const m=document.cookie.match(/deviceId=([^;]+)/);return m?m[1]:'?';}
$('devId').textContent=readDevId();
document.querySelectorAll('.chip[data-w]').forEach(c=>{c.onclick=()=>{c.classList.toggle('on');wants[c.dataset.w]=c.classList.contains('on');};});
// Test flash: chiede conferma solo quando lo si ACCENDE (non a ogni scatto,
// per non essere fastidioso su una sessione di 20-40 foto).
$('flashChip').onclick=()=>{
  if(!flashTestOn){
    if(!confirm('Il test riflessione accende brevemente il flash prima di ogni scatto, per capire se la superficie è riflettente, rifrangente o trasparente (confrontando la foto con/senza flash). Continuare?'))
      return;
  }
  flashTestOn=!flashTestOn;
  $('flashChip').classList.toggle('on',flashTestOn);
};
function showToast(m,col){toast.textContent=m;toast.style.background=col||'var(--ok)';toast.classList.add('show');setTimeout(()=>toast.classList.remove('show'),1600);}
async function startCamera(){if(stream)stream.getTracks().forEach(t=>t.stop());
// Scansione "intelligente": bussola/inclinazione servono sempre (guida a
// puntini numerati, auto-scatto, sidecar), non sono più un extra opzionale
// da ricordarsi di accendere. Richiesta avviata QUI, nello stesso gesto
// utente del click su "Attiva fotocamera" (non dopo l'await della camera):
// iOS/Safari lega il permesso sensori al gesto originale, aspettare
// rischia di farlo cadere nel vuoto senza errore visibile.
const sensorsPromise = enableSensors();
try{stream=await navigator.mediaDevices.getUserMedia({video:{facingMode:facing,width:{ideal:2560},height:{ideal:1440}},audio:false});
video.srcObject=stream;video.style.display='block';$('flipCam').style.display='block';$('shoot').style.display='block';$('enableSensor').style.display='block';$('startCam').textContent='\u{23F9} Ferma fotocamera';
document.querySelector('.wrap').classList.add('cam-active');
$('targetRing').style.display='block';$('targetChips').style.display='flex';$('targetHint').style.display='block';drawTargetSphere();}
catch(e){showToast('Camera: '+e.message,'#f85149');}
await sensorsPromise;}
// Ferma la fotocamera quando non serve più (batteria/privacy): rilascia lo
// stream, spegne anche i sensori (senza fotocamera non servono, vedi
// disableSensors()) e riporta l'interfaccia allo stato "non attiva".
function stopCamera(){
  if(stream){ stream.getTracks().forEach(t=>t.stop()); stream=null; }
  video.style.display='none'; video.srcObject=null;
  document.querySelector('.wrap').classList.remove('cam-active');
  $('flipCam').style.display='none';
  $('shoot').style.display='none';
  $('targetRing').style.display='none';
  $('targetChips').style.display='none';
  $('targetHint').style.display='none';
  $('rotateHint').style.display='none';
  $('startCam').textContent='\u{1F4F7} Attiva fotocamera';
  disableSensors();
}
function dl(k){window.location='/download?session='+encodeURIComponent($('session').value.trim()||'scan1')+'&file='+k+'&token='+encodeURIComponent(TOKEN);}
$('dlObj').onclick=()=>dl('obj');$('dlMtl').onclick=()=>dl('mtl');$('dlPly').onclick=()=>dl('ply');
$('startCam').onclick=()=>{ stream ? stopCamera() : startCamera(); };
$('enableSensor').onclick=enableSensors;
$('flipCam').onclick=()=>{facing=facing==='environment'?'user':'environment';startCamera();};
/* Accende la torcia del telefono (se il browser/hardware la espone come
   MediaStreamTrack constraint) per un breve test di riflessione: una
   superficie molto più chiara/con riflessi col flash è probabilmente
   riflettente o trasparente. Non tutti i device la supportano: in quel
   caso avvisa UNA sola volta invece di interrompere ogni scatto. */
async function tryTorch(on){
  const track=stream&&stream.getVideoTracks()[0];
  if(!track) return false;
  const caps=track.getCapabilities?track.getCapabilities():{};
  if(!caps.torch){
    if(on && !flashWarned){flashWarned=true;showToast('Flash/torcia non supportato su questo dispositivo','#d29922');}
    return false;
  }
  try{await track.applyConstraints({advanced:[{torch:on}]});return true;}
  catch(e){if(on && !flashWarned){flashWarned=true;showToast('Flash non attivabile: '+e.message,'#d29922');}return false;}
}
async function doShoot(){
if(!stream || $('shoot').disabled) return;
const w=video.videoWidth,h=video.videoHeight;canvas.width=w;canvas.height=h;
let shotFlash=false;
if(flashTestOn){
  shotFlash=await tryTorch(true);
  if(shotFlash) await new Promise(r=>setTimeout(r,250));
}
canvas.getContext('2d').drawImage(video,0,0,w,h);
if(shotFlash) await tryTorch(false);
const dataURL=canvas.toDataURL('image/jpeg',0.92);
const list=Object.keys(wants).filter(k=>wants[k]);
if(!list.length){showToast('Seleziona almeno una analisi','#f85149');return;}
/* Blocca angolo/inclinazione ADESSO (momento dello scatto): la risposta
   del PC arriva dopo VLM/depth (anche secondi) e nel frattempo il telefono
   può aver continuato a ruotare. Usare curHeading "live" dopo l'attesa
   marcherebbe coperto il pallino sbagliato — bug corretto qui. */
const shotHeading=curHeading, shotPitch=curPitch, shotRoll=curRoll, shotHasSensors=sensorOn;
const shotAccel=curAccel, shotAccelG=curAccelG, shotRotRate=curRotRate;
const shotAltitude=curAltitude, shotAltAcc=curAltAccuracy, shotLat=curLat, shotLon=curLon;
// Congelato come gli altri sensori sopra: il movimento durante l'attesa
// della risposta PC (VLM/depth, anche secondi) non deve retroattivamente
// far sembrare instabile uno scatto che era fermo al momento giusto.
const shotMotionStable=(!sensorOn || motionStable), shotMotionMagnitude=motionMagnitude;
if(sensorOn && !motionStable) showToast('\u{1F4F3} Scatto con movimento rilevato — verifica se è sfocato','#d29922');
$('resultCard').style.display='block';
$('status').innerHTML='<span class="spinner"></span> Il PC sta elaborando ('+list.join(', ')+')…';
$('descBox').style.display='none';$('imgBox').innerHTML='';
$('shoot').textContent='Analisi in corso…';$('shoot').disabled=true;
try{const res=await fetch('/upload',{method:'POST',headers:{'Content-Type':'application/json'},
body:JSON.stringify({token:TOKEN,session:$('session').value.trim()||'scan1',wants:list,mode:scanMode,angle:(shotHeading!=null?shotHeading:0),pitch:(shotPitch!=null?shotPitch:0),roll:(shotRoll!=null?shotRoll:0),heading:shotHeading,hasSensors:shotHasSensors,accel:shotAccel,accelGravity:shotAccelG,rotationRate:shotRotRate,altitude:shotAltitude,altitudeAccuracy:shotAltAcc,latitude:shotLat,longitude:shotLon,flash:shotFlash,motionStable:shotMotionStable,motionMagnitude:shotMotionMagnitude,image:dataURL})});
const j=await res.json();
$('devId').textContent=readDevId();
if(!j.ok){$('status').textContent='Errore: '+j.error;}
else{$('status').textContent='✓ ['+(j.device||'?')+'] foto #'+j.index+' analizzata.'+(j.aruco_markers>0?' \u{1F4CF} scala OK ('+j.aruco_markers+' marker)':' ⚠ nessun marker');
if(j.description){$('descBox').style.display='block';$('descText').textContent=j.description;}
const ib=$('imgBox');
if(j.boxes_image){ib.insertAdjacentHTML('beforeend','<figure><img src="'+j.boxes_image+'"><figcaption>Box: '+(j.boxes_count||0)+' regioni</figcaption></figure>');}
if(j.depth_image){ib.insertAdjacentHTML('beforeend','<figure><img src="'+j.depth_image+'"><figcaption>Depth (vicino=rosso)</figcaption></figure>');}
if(j.depth_unavailable){ib.insertAdjacentHTML('beforeend','<figcaption style="grid-column:1/-1;color:#d29922">Depth non disponibile sul PC</figcaption>');}
if(j.edges_image){ib.insertAdjacentHTML('beforeend','<figure><img src="'+j.edges_image+'"><figcaption>Bordi</figcaption></figure>');}
if(j.edges_unavailable){ib.insertAdjacentHTML('beforeend','<figcaption style="grid-column:1/-1;color:#d29922">Bordi non disponibili sul PC (serve OpenCV)</figcaption>');}
if(j.bump_image){ib.insertAdjacentHTML('beforeend','<figure><img src="'+j.bump_image+'"><figcaption>Bump map</figcaption></figure>');}
if(j.bump_unavailable){ib.insertAdjacentHTML('beforeend','<figcaption style="grid-column:1/-1;color:#d29922">Bump map non disponibile sul PC (serve OpenCV)</figcaption>');}
if(j.normal_image){ib.insertAdjacentHTML('beforeend','<figure><img src="'+j.normal_image+'"><figcaption>Normal map</figcaption></figure>');}
if(j.normal_unavailable){ib.insertAdjacentHTML('beforeend','<figcaption style="grid-column:1/-1;color:#d29922">Normal map non disponibile sul PC (serve OpenCV)</figcaption>');}
if(j.flash_used){ib.insertAdjacentHTML('beforeend','<figcaption style="grid-column:1/-1;color:#3fb0ff">\u{1F4A1} Scatto con test flash: confronta con uno scatto senza per capire se riflette</figcaption>');}
markCovered(shotHeading);drawTargetSphere();updateRotateHint();
showToast('Analisi pronta ✓');}}
catch(e){$('status').textContent='Rete: '+e.message;}
$('shoot').textContent='SCATTA E ANALIZZA';$('shoot').disabled=false;}
$('shoot').onclick=doShoot;
// --- scatto automatico: ruotando il telefono a destra/sinistra (bussola),
// quando ci si allinea con un pallino ancora scoperto sull'anello attivo
// scatta da sola. "autoArmed" evita raffiche: si riarma solo dopo essersi
// allontanati dal bersaglio appena coperto. ---
let autoShoot=false, autoArmed=true;
function checkAutoShoot(){
  // Instabile: salta questo giro senza scattare. Niente riarmo qui — quando
  // il telefono torna fermo, il prossimo tick di questo stesso loop riprova
  // regolarmente (nessun timer separato da gestire).
  if(!autoShoot || !stream || $('shoot').disabled || curHeading==null || !targetRings.length || !motionStable) return;
  const ring = targetRings[activeRingIndex()];
  let best=null, bestDiff=999;
  ring.headings.forEach(t=>{
    if(t.covered) return;
    const diff = Math.abs(((t.deg-curHeading+540)%360)-180);
    if(diff<bestDiff){bestDiff=diff; best=t;}
  });
  const tolFire = (360/ring.headings.length)/2 * 0.55;   // stretta: scatta solo ben centrato
  const tolRearm= (360/ring.headings.length)/2 * 1.3;     // larga: riarma solo se ci si è allontanati
  if(best && bestDiff<=tolFire && autoArmed){
    autoArmed=false;
    doShoot();
  } else if(!best || bestDiff>tolRearm){
    autoArmed=true;
  }
}
$('autoShoot').onclick=()=>{
  autoShoot=!autoShoot; autoArmed=true;
  $('autoShoot').classList.toggle('on', autoShoot);
  $('autoShoot').textContent = autoShoot ? '\u{1F916} Auto ON' : '\u{1F916} Auto OFF';
  showToast(autoShoot ? 'Scatto automatico attivo — ruota intorno all’oggetto' : 'Scatto automatico disattivato');
};
</script></body></html>)HTML";
    QByteArray out(html);
    /* Token generato da QUuid (solo cifre/lettere/trattini, mai virgolette
       o backslash): sicuro da iniettare letteralmente in una stringa JS
       senza escaping aggiuntivo. */
    out.replace("__VISION3D_TOKEN__", token.toUtf8());
    return out;
}
