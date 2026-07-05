// ============================================================================
//  widget_vision3d.cpp — Prismalux Vision3D (scansione 3D da telefono/tablet)
// ============================================================================
#include "widget_vision3d.h"
#include "../prismalux_paths.h"
#include "../dpi_utils.h"
#include "../lan_server.h"          // LanServer::_ensureCert (cert condiviso app)

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QGroupBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QListWidget>
#include <QSpinBox>
#include <QMessageBox>
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
#include <QSplitter>
#include <QSignalBlocker>
#include <QtMath>
#include <cmath>

namespace P = PrismaluxPaths;

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
               "Cono tratteggiato = scatto senza sensori (posizione indicativa).");
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

    // frustum (prima i non selezionati, il selezionato sopra)
    for (const V3dShotInfo& s : m_shots)
        if (s.key != m_selectedKey) drawFrustum(p, s, false);
    for (const V3dShotInfo& s : m_shots)
        if (s.key == m_selectedKey) drawFrustum(p, s, true);

    if (m_shots.isEmpty()) {
        p.setPen(palette().color(QPalette::PlaceholderText));
        p.drawText(rect(), Qt::AlignCenter,
                   "Gli scatti compariranno qui\ncome coni intorno all'oggetto");
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
    /* riapri l'ultima sessione presente su disco: galleria e scena 3D
       ripartono piene anche dopo un riavvio dell'app */
    populateSessions();
    if (m_sessionCombo && m_sessionCombo->count() > 0)
        loadSessionIntoUi(m_sessionCombo->currentText());
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
    if (m_log) m_log->appendPlainText(line);
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

    m_running = true;
    const QString url = "https://" + bindIp + ":" + QString::number(m_port);
    m_urlLabel->setText(url);
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
    appendLog("Server fermato.");
}

void Vision3DWidget::onToggleServerClicked()
{
    if (m_running) stop();
    else start(quint16(m_portEdit->text().toUShort()));
}

/* Guida rapida richiamata dal pulsante "?" nell'intestazione. */
void Vision3DWidget::onHelpClicked()
{
    const QString url = m_running && m_urlLabel ? m_urlLabel->text()
                                                : QStringLiteral("https://IP-del-PC:8443");
    QMessageBox box(this);
    box.setWindowTitle("Vision3D — guida rapida");
    box.setTextFormat(Qt::RichText);
    box.setText(QString(
        "<h3>1. Collegare i client (iOS, Android, Desktop)</h3>"
        "<ol>"
        "<li>Premi <b>Avvia server</b> (interfaccia LAN 192.168.x consigliata).</li>"
        "<li>Sul client, stessa rete WiFi, apri nel browser: <b>%1</b></li>"
        "<li>Avviso certificato (self-signed, normale): <i>Avanzate → Procedi</i>.</li>"
        "<li><b>iOS</b>: Safari, poi tocca \xf0\x9f\xa7\xad Sensori e concedi il permesso "
        "(obbligatorio da iOS 13).</li>"
        "<li><b>Android</b>: se la bussola resta a 0\xc2\xb0 usa Firefox, oppure dopo gli "
        "scatti premi <i>\xe2\x86\x94 Distribuisci sul cerchio</i> e correggi le pose a mano.</li>"
        "<li><b>Desktop client</b>: qualunque browser del PC — utile con una webcam.</li>"
        "</ol>"
        "<h3>2. Installare COLMAP (nuvola di punti)</h3>"
        "<p><code>sudo apt install colmap libposelib</code></p>"
        "<p>Su alcune versioni di Ubuntu il pacchetto <code>colmap</code> non installa da "
        "solo <code>libposelib</code> (errore <i>libPoseLib.so</i>): il comando sopra li "
        "mette entrambi. Dopo l'installazione premi <b>\xe2\x9f\xb3 Ricontrolla</b> nella "
        "sezione Ricostruzione: la verifica avviene a caldo, senza riavviare Prismalux.</p>"
        "<h3>3. Consigli per la scansione</h3>"
        "<p>Stampa i bersagli in <code>Tools/aruco/</code> al 100% (la scala reale del "
        "modello arriva da l\xc3\xac), gira intorno all'oggetto con buona sovrapposizione, "
        "luce uniforme e sfondo fermo. Foto minime per qualit\xc3\xa0: "
        "<b>low \xe2\x89\xa5 10 &middot; medium \xe2\x89\xa5 20 &middot; high \xe2\x89\xa5 40</b> "
        "(il contatore accanto alla qualit\xc3\xa0 diventa verde quando bastano).</p>"
        "<h3>4. Modello 3D (scanner simulato)</h3>"
        "<p>La ricostruzione produce una <b>nuvola di punti colorata</b>: "
        "<code>modello.obj + modello.mtl</code> (Blender/MeshLab) e "
        "<code>nuvola_punti.ply</code> (CloudCompare). I client iOS/Android/Desktop la "
        "scaricano dai pulsanti \xe2\xac\x87 della card <i>Modello 3D</i>; per una mesh "
        "con superficie: MeshLab \xe2\x86\x92 Poisson.</p>").arg(url));
    box.exec();
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

void Vision3DWidget::onGalleryItemClicked(QListWidgetItem* item)
{
    if (!item) return;
    const QString basePath = item->data(Qt::UserRole).toString();
    const QString desc     = item->data(Qt::UserRole + 1).toString();
    loadV3dThumb(m_lastThumb, basePath + ".jpg",        QStringLiteral("(file rimosso)"));
    loadV3dThumb(m_lastBoxes, basePath + "_boxes.jpg",  QStringLiteral("(box non\ncalcolati)"));
    loadV3dThumb(m_lastDepth, basePath + "_depth.jpg",  QStringLiteral("(depth non\ncalcolata)"));
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
    const QSignalBlocker block(m_sessionCombo);   // niente reload durante il refill
    const QString prev = select.isEmpty() ? m_sessionCombo->currentText() : select;
    m_sessionCombo->clear();
    const QFileInfoList dirs = QDir(m_outputDir)
        .entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time);
    for (const QFileInfo& fi : dirs)
        m_sessionCombo->addItem(fi.fileName());
    const int idx = m_sessionCombo->findText(prev);
    if (idx >= 0) m_sessionCombo->setCurrentIndex(idx);
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
            QFile mf(base + ".json");
            if (mf.open(QIODevice::ReadOnly)) {
                const QJsonObject o = QJsonDocument::fromJson(mf.readAll()).object();
                heading    = o.value("heading_deg").toInt();
                pitch      = o.value("pitch_deg").toInt();
                index      = o.value("index").toInt();
                // posa impostata a mano dall'utente = affidabile quanto i sensori
                hasSensors = o.value("has_sensors").toBool()
                             || o.value("pose_manual").toBool();
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
            it->setData(Qt::UserRole + 1, QString());   // descrizione VLM non persistita
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
    loadSessionIntoUi(session);
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

void Vision3DWidget::onVlmTagsReady()
{
    QNetworkReply* reply = m_tagsReply;
    m_tagsReply = nullptr;
    if (!reply) return;
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError || !m_vlmCombo) return;

    const QJsonArray models = QJsonDocument::fromJson(reply->readAll())
                                  .object().value("models").toArray();
    QStringList names;
    for (const auto& m : models) {
        const QString n = m.toObject().value("name").toString();
        if (!n.isEmpty()) names.append(n);
    }
    if (names.isEmpty()) return;

    /* mantieni la scelta corrente se ancora installata; altrimenti prova
       a proporre un modello vision noto */
    const QString cur = m_vlmModel;
    m_vlmCombo->blockSignals(true);
    m_vlmCombo->clear();
    m_vlmCombo->addItems(names);
    int idx = m_vlmCombo->findText(cur);
    if (idx < 0) {
        static const char* kVisionHints[] =
            {"moondream", "-vl", "llava", "vision", "minicpm-v", "bakllava"};
        const int nHints = int(sizeof(kVisionHints) / sizeof(kVisionHints[0]));
        for (int i = 0; idx < 0 && i < nHints; ++i)
            for (int j = 0; j < names.size(); ++j)
                if (names[j].contains(QLatin1String(kVisionHints[i]), Qt::CaseInsensitive))
                    { idx = j; break; }
    }
    if (idx >= 0) m_vlmCombo->setCurrentIndex(idx);
    else          m_vlmCombo->setEditText(cur);
    m_vlmCombo->blockSignals(false);
    onVlmModelChanged(m_vlmCombo->currentText());
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
    if (req.method == "GET" && (req.path == "/" || req.path.startsWith("/index"))) {
        // assegna/riusa deviceId e lo imposta come cookie persistente
        const QString id = assignDeviceId(req.cookieDeviceId, req.userAgent);
        emit deviceSeen(id);
        refreshDeviceTable();
        const QByteArray setCookie =
            "Set-Cookie: deviceId=" + id.toUtf8() + "; Max-Age=31536000; Path=/; SameSite=Lax\r\n";
        sendHtml(sock, htmlPage(), setCookie);
        return;
    }

    if (req.method == "POST" && req.path == "/upload") {
        const QString deviceId = assignDeviceId(req.cookieDeviceId, req.userAgent);

        const QJsonDocument doc = QJsonDocument::fromJson(req.body);
        const QJsonObject o = doc.object();
        const QString session = o.value("session").toString("scan1");
        const int angle = o.value("angle").toInt(0);
        const int pitch = o.value("pitch").toInt(0);
        const int roll  = o.value("roll").toInt(0);
        const bool hasSensors = o.value("hasSensors").toBool(false);
        const QJsonArray wants = o.value("wants").toArray();
        bool doDesc=false, doBoxes=false, doDepth=false;
        for (const auto& w : wants) {
            const QString s = w.toString();
            if (s=="desc") doDesc=true; else if (s=="boxes") doBoxes=true;
            else if (s=="depth") doDepth=true;
        }
        const QString dataUrl = o.value("image").toString();
        const int comma = dataUrl.indexOf(',');
        const QByteArray jpeg = QByteArray::fromBase64(
            (comma>=0 ? dataUrl.mid(comma+1) : dataUrl).toUtf8());

        Vision3DResult r = analyze(session, deviceId, angle, pitch, roll, hasSensors,
                                   jpeg, doDesc, doBoxes, doDepth);

        QJsonObject out;
        out["ok"] = r.error.isEmpty();
        out["index"] = r.index;
        out["device"] = r.deviceId;
        out["aruco_markers"] = r.arucoMarkersFound;
        out["scale_mm_per_unit"] = r.scaleMmPerUnit;
        if (!r.error.isEmpty()) out["error"] = r.error;
        if (!r.description.isEmpty()) out["description"] = r.description;
        if (doBoxes) {
            out["boxes_count"] = r.boxesCount;
            if (!r.boxesJpeg.isEmpty())
                out["boxes_image"] = "data:image/jpeg;base64," + QString::fromUtf8(r.boxesJpeg.toBase64());
        }
        if (doDepth) {
            if (!r.depthJpeg.isEmpty())
                out["depth_image"] = "data:image/jpeg;base64," + QString::fromUtf8(r.depthJpeg.toBase64());
            else out["depth_unavailable"] = true;
        }
        const QByteArray setCookie =
            "Set-Cookie: deviceId=" + deviceId.toUtf8() + "; Max-Age=31536000; Path=/; SameSite=Lax\r\n";
        sendJson(sock, QJsonDocument(out).toJson(QJsonDocument::Compact), setCookie);
        emit analysisReady(r);
        refreshDeviceTable();
        return;
    }

    if (req.method == "GET" && req.path.startsWith("/download")) {
        // /download?session=<nome>&file=obj|mtl|ply — scarica il modello sul client
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
                                       const QByteArray& jpeg,
                                       bool doDesc, bool doBoxes, bool doDepth)
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

    if (doDesc) {
        appendLog(QString("[%1]  → VLM…").arg(deviceId));
        r.description = vlmDescribe(jpeg);
    }
    if (doBoxes) {
        appendLog(QString("[%1]  → OpenCV box…").arg(deviceId));
        r.boxesJpeg = detectBoxes(jpeg, r.boxesCount);
        if (!r.boxesJpeg.isEmpty()) {
            QFile bf(folder + "/" + base + "_boxes.jpg");
            if (bf.open(QIODevice::WriteOnly)) { bf.write(r.boxesJpeg); bf.close(); }
        }
    }
    if (doDepth) {
        appendLog(QString("[%1]  → depth…").arg(deviceId));
        r.depthJpeg = depthMap(jpeg);
        if (!r.depthJpeg.isEmpty()) {
            QFile df(folder + "/" + base + "_depth.jpg");
            if (df.open(QIODevice::WriteOnly)) { df.write(r.depthJpeg); df.close(); }
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
    if (m_lastBoxes) {
        if (!r.boxesJpeg.isEmpty()) {
            QPixmap b; b.loadFromData(r.boxesJpeg, "JPEG");
            m_lastBoxes->setPixmap(b.scaled(dpiScale(160), dpiScale(160),
                                            Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else if (doBoxes) {
            m_lastBoxes->setPixmap(QPixmap());
            m_lastBoxes->setText("Box non disponibili:\nserve OpenCV\ndi sistema\n(libopencv-dev)");
        }
    }
    if (m_lastDepth) {
        if (!r.depthJpeg.isEmpty()) {
            QPixmap dp; dp.loadFromData(r.depthJpeg, "JPEG");
            m_lastDepth->setPixmap(dp.scaled(dpiScale(160), dpiScale(160),
                                             Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else if (doDepth) {
            m_lastDepth->setPixmap(QPixmap());
            m_lastDepth->setText("Depth non riuscita\n(vedi Log:\ndi solito manca\ntorch/timm)");
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
#endif // QT_CONFIG(ssl)

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

    /* ── due colonne: sinistra device/scatti/anteprime (scrollabile),
       destra scena 3D + ricostruzione fotogrammetrica ── */
    auto* split = new QSplitter(Qt::Horizontal);
    auto* leftCol = new QWidget;
    auto* leftLay = new QVBoxLayout(leftCol);
    leftLay->setContentsMargins(0, 0, 0, 0);

    // pannello device attivi
    auto* devBox = new QGroupBox("Device attivi");
    auto* devLay = new QVBoxLayout(devBox);
    m_deviceTable = new QTableWidget(0, 4);
    m_deviceTable->setHorizontalHeaderLabels({"Device", "Foto", "Ultimo", "Dispositivo"});
    m_deviceTable->horizontalHeader()->setStretchLastSection(true);
    m_deviceTable->verticalHeader()->setVisible(false);
    m_deviceTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_deviceTable->setMaximumHeight(dpiScale(140));
    devLay->addWidget(m_deviceTable);
    leftLay->addWidget(devBox);

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
    m_gallery->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_gallery, &QListWidget::itemClicked,
            this, &Vision3DWidget::onGalleryItemClicked);
    galLay->addWidget(m_gallery);
    leftLay->addWidget(galBox);

    auto* prevBox = new QGroupBox("Ultimo scatto analizzato");
    auto* grid = new QGridLayout(prevBox);
    m_lastThumb = makeVision3dThumb();
    m_lastBoxes = makeVision3dThumb();
    m_lastDepth = makeVision3dThumb();
    grid->addWidget(new QLabel("Originale"), 0,0);    grid->addWidget(m_lastThumb, 1,0);
    grid->addWidget(new QLabel("Box (OpenCV)"), 0,1); grid->addWidget(m_lastBoxes, 1,1);
    grid->addWidget(new QLabel("Depth"), 0,2);        grid->addWidget(m_lastDepth, 1,2);
    m_lastDesc = new QLabel("La descrizione VLM apparirà qui.");
    m_lastDesc->setWordWrap(true);
    m_lastDesc->setStyleSheet("background:palette(base);border:1px solid palette(mid);"
                              "border-radius:8px;padding:10px;");
    grid->addWidget(m_lastDesc, 2,0,1,3);
    leftLay->addWidget(prevBox);
    leftLay->addStretch();

    /* colonna sinistra scrollabile: con finestre basse niente sovrapposizioni */
    auto* leftScroll = new QScrollArea;
    leftScroll->setWidgetResizable(true);
    leftScroll->setFrameShape(QFrame::NoFrame);
    leftScroll->setWidget(leftCol);
    split->addWidget(leftScroll);

    /* ── colonna destra: scena 3D + ricostruzione ── */
    auto* rightCol = new QWidget;
    auto* rightLay = new QVBoxLayout(rightCol);
    rightLay->setContentsMargins(0, 0, 0, 0);

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
    m_sessionCombo->setToolTip("Cartella di scansione in scan_output/ — cambiala per\n"
                               "rivedere in galleria e scena 3D una sessione precedente.");
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

    split->addWidget(rightCol);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 2);
    root->addWidget(split, 1);   // stretch: lo spazio extra va alle colonne

    m_log = new QPlainTextEdit; m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(500);
    m_log->setStyleSheet("font-family:monospace;");
    m_log->setMaximumHeight(dpiScale(140));
    root->addWidget(new QLabel("Log:"));
    root->addWidget(m_log);

    appendLog("Pronto. Avvia il server e apri l'URL da OGNI telefono (ognuno riceve un ID).");
    appendLog("Bersagli ArUco stampabili (scala reale): " + P::root() + "/Tools/aruco/");
}

// ---------------------------------------------------------------------------
QByteArray Vision3DWidget::htmlPage()
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
.b-shoot{background:linear-gradient(135deg,var(--acc),var(--acc2));width:100%;font-size:1.05rem;padding:15px;margin-top:10px;}
.b-alt{background:#21262d;}
.chips{display:flex;gap:8px;flex-wrap:wrap;}
.chip{background:#0d1117;border:1px solid #30363d;border-radius:20px;padding:8px 14px;font-size:.82rem;cursor:pointer;}
.chip.on{border-color:var(--acc);background:#132a3d;color:var(--acc);}
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
<div class="card"><label>Cosa calcolare</label>
<div class="chips">
<div class="chip on" data-w="desc">&#129504; Descrizione (VLM)</div>
<div class="chip on" data-w="boxes">&#9634; Box (OpenCV)</div>
<div class="chip on" data-w="depth">&#127752; Depth</div>
</div>
<div class="hint">Usa lo <b>stesso nome sessione</b> su ogni telefono per unire gli scatti nello stesso progetto. Le foto di ognuno finiscono in una sottocartella per device.</div></div>
<div class="card">
<video id="video" autoplay playsinline muted></video><canvas id="canvas"></canvas>
<div class="chips" style="margin-top:10px;">
<button class="b-alt" id="startCam" style="flex:1;">&#128247; Attiva fotocamera</button>
<button class="b-alt" id="flipCam" style="display:none;">&#128260;</button>
<button class="b-alt" id="enableSensor" style="display:none;">&#129517; Sensori</button></div>
<div id="sensorReadout" class="hint" style="display:none;">Orientamento: <b id="soAngle">--</b>&#176; &nbsp;|&nbsp; inclinazione <b id="soPitch">--</b>&#176;</div>
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
const $=id=>document.getElementById(id);
const video=$('video'),canvas=$('canvas'),toast=$('toast');
let stream=null,facing='environment',wants={desc:true,boxes:true,depth:true};
// --- sensori di movimento (giroscopio/bussola) ---
let sensorOn=false, curHeading=null, curPitch=null, curRoll=null;
function onOrient(e){
  let a = e.webkitCompassHeading!=null ? e.webkitCompassHeading : (e.alpha!=null?360-e.alpha:null);
  curHeading = a!=null ? Math.round(((a%360)+360)%360) : null;
  curPitch = e.beta!=null ? Math.round(e.beta) : null;
  curRoll  = e.gamma!=null ? Math.round(e.gamma) : null;
  if(sensorOn){
    $('soAngle').textContent = curHeading!=null?curHeading:'--';
    $('soPitch').textContent = curPitch!=null?curPitch:'--';
  }
}
async function enableSensors(){
  if(typeof DeviceOrientationEvent!=='undefined' && typeof DeviceOrientationEvent.requestPermission==='function'){
    try{ const p=await DeviceOrientationEvent.requestPermission(); if(p!=='granted'){showToast('Permesso sensori negato','#d29922');return;} }
    catch(e){ showToast('Sensori non disponibili','#d29922'); return; }
  }
  if(!window.DeviceOrientationEvent){ showToast('Nessun sensore su questo device','#d29922'); return; }
  window.addEventListener('deviceorientation', onOrient, true);
  sensorOn=true; $('enableSensor').textContent='\u{1F9ED} Attivi'; $('sensorReadout').style.display='block';
  showToast('Sensori attivi ✓');
}
// mostra il proprio deviceId dal cookie (impostato dal server)
function readDevId(){const m=document.cookie.match(/deviceId=([^;]+)/);return m?m[1]:'?';}
$('devId').textContent=readDevId();
document.querySelectorAll('.chip').forEach(c=>{c.onclick=()=>{c.classList.toggle('on');wants[c.dataset.w]=c.classList.contains('on');};});
function showToast(m,col){toast.textContent=m;toast.style.background=col||'var(--ok)';toast.classList.add('show');setTimeout(()=>toast.classList.remove('show'),1600);}
async function startCamera(){if(stream)stream.getTracks().forEach(t=>t.stop());
try{stream=await navigator.mediaDevices.getUserMedia({video:{facingMode:facing,width:{ideal:2560},height:{ideal:1440}},audio:false});
video.srcObject=stream;video.style.display='block';$('flipCam').style.display='block';$('shoot').style.display='block';$('enableSensor').style.display='block';$('startCam').textContent='\u{1F4F7} Attiva';}
catch(e){showToast('Camera: '+e.message,'#f85149');}}
function dl(k){window.location='/download?session='+encodeURIComponent($('session').value.trim()||'scan1')+'&file='+k;}
$('dlObj').onclick=()=>dl('obj');$('dlMtl').onclick=()=>dl('mtl');$('dlPly').onclick=()=>dl('ply');
$('startCam').onclick=startCamera;
$('enableSensor').onclick=enableSensors;
$('flipCam').onclick=()=>{facing=facing==='environment'?'user':'environment';startCamera();};
$('shoot').onclick=async()=>{if(!stream)return;
const w=video.videoWidth,h=video.videoHeight;canvas.width=w;canvas.height=h;
canvas.getContext('2d').drawImage(video,0,0,w,h);
const dataURL=canvas.toDataURL('image/jpeg',0.92);
const list=Object.keys(wants).filter(k=>wants[k]);
if(!list.length){showToast('Seleziona almeno una analisi','#f85149');return;}
$('resultCard').style.display='block';
$('status').innerHTML='<span class="spinner"></span> Il PC sta elaborando ('+list.join(', ')+')…';
$('descBox').style.display='none';$('imgBox').innerHTML='';
$('shoot').textContent='Analisi in corso…';$('shoot').disabled=true;
try{const res=await fetch('/upload',{method:'POST',headers:{'Content-Type':'application/json'},
body:JSON.stringify({session:$('session').value.trim()||'scan1',wants:list,angle:(curHeading!=null?curHeading:0),pitch:(curPitch!=null?curPitch:0),roll:(curRoll!=null?curRoll:0),heading:curHeading,hasSensors:sensorOn,image:dataURL})});
const j=await res.json();
$('devId').textContent=readDevId();
if(!j.ok){$('status').textContent='Errore: '+j.error;}
else{$('status').textContent='✓ ['+(j.device||'?')+'] foto #'+j.index+' analizzata.'+(j.aruco_markers>0?' \u{1F4CF} scala OK ('+j.aruco_markers+' marker)':' ⚠ nessun marker');
if(j.description){$('descBox').style.display='block';$('descText').textContent=j.description;}
const ib=$('imgBox');
if(j.boxes_image){ib.insertAdjacentHTML('beforeend','<figure><img src="'+j.boxes_image+'"><figcaption>Box: '+(j.boxes_count||0)+' regioni</figcaption></figure>');}
if(j.depth_image){ib.insertAdjacentHTML('beforeend','<figure><img src="'+j.depth_image+'"><figcaption>Depth (vicino=rosso)</figcaption></figure>');}
if(j.depth_unavailable){ib.insertAdjacentHTML('beforeend','<figcaption style="grid-column:1/-1;color:#d29922">Depth non disponibile sul PC</figcaption>');}
showToast('Analisi pronta ✓');}}
catch(e){$('status').textContent='Rete: '+e.message;}
$('shoot').textContent='SCATTA E ANALIZZA';$('shoot').disabled=false;};
</script></body></html>)HTML";
    return QByteArray(html);
}
