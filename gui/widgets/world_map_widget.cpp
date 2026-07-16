#include "world_map_widget.h"
#include "../dpi_utils.h"
#include "../prismalux_paths.h"
namespace P = PrismaluxPaths;
#include <QSettings>
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QContextMenuEvent>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QFont>
#include <QFontMetrics>
#include <QLineEdit>
#include <QToolButton>
#include <QLabel>
#include <QListWidget>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QUrlQuery>
#include <QMenu>
#include <QAction>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <cmath>

/* ── Coordinate math (Web Mercator / EPSG:3857) ─────────────────
   World pixel space: 256*2^z × 256*2^z, origin = NW corner      */

QPointF WorldMapWidget::latLonToWorld(double lat, double lon) const
{
    const double scale = kTileSize * (double)(1 << m_zoom);
    const double x = (lon + 180.0) / 360.0 * scale;
    const double lr = qBound(-85.05, lat, 85.05) * M_PI / 180.0;
    const double y = (1.0 - std::log(std::tan(lr) + 1.0 / std::cos(lr)) / M_PI) / 2.0 * scale;
    return QPointF(x, y);
}

void WorldMapWidget::worldToLatLon(double wx, double wy, double& lat, double& lon) const
{
    const double scale = kTileSize * (double)(1 << m_zoom);
    lon = wx / scale * 360.0 - 180.0;
    const double n = M_PI * (1.0 - 2.0 * wy / scale);
    lat = std::atan(std::sinh(n)) * 180.0 / M_PI;
}

QPointF WorldMapWidget::worldToScreen(double wx, double wy) const
{
    return QPointF(wx - m_centerX + width()  / 2.0,
                   wy - m_centerY + height() / 2.0);
}

QPointF WorldMapWidget::screenToWorld(double sx, double sy) const
{
    return QPointF(sx - width()  / 2.0 + m_centerX,
                   sy - height() / 2.0 + m_centerY);
}

/* ── Tile helpers ───────────────────────────────────────────────── */

QString WorldMapWidget::tileKey(int z, int x, int y) const
{
    return QString("%1/%2/%3").arg(z).arg(x).arg(y);
}

void WorldMapWidget::requestTile(int z, int x, int y)
{
    const int tilesN = 1 << z;
    x = ((x % tilesN) + tilesN) % tilesN;
    if (y < 0 || y >= tilesN) return;

    const QString key = tileKey(z, x, y);
    if (m_cache.contains(key) || m_pending.contains(key)) return;

    if (m_cache.size() >= kMaxCache)
        m_cache.remove(m_cache.firstKey());

    /* Controlla cache disco prima della rete */
    if (!m_tileDir.isEmpty()) {
        const QString diskPath = m_tileDir + QString("%1/%2/%3.png").arg(z).arg(x).arg(y);
        QPixmap pix;
        if (pix.load(diskPath)) {
            m_cache.insert(key, pix);
            update();
            return;
        }
    }

    m_pending.insert(key);
    const QUrl url(QString("https://tile.openstreetmap.org/%1/%2/%3.png").arg(z).arg(x).arg(y));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, "Prismalux/3.0 (educational desktop app)");
    req.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                     QNetworkRequest::PreferCache);
    m_net->get(req);
}

void WorldMapWidget::onDlTimerTick()
{
    if (m_dlQueue.isEmpty()) {
        m_dlTimer->stop();
        emit tileDownloadProgress(m_dlDone, m_dlDone);
        return;
    }
    const auto job = m_dlQueue.takeFirst();
    ++m_dlDone;
    requestTile(job.z, job.x, job.y);
    emit tileDownloadProgress(m_dlDone, m_dlDone + m_dlQueue.size());
}

void WorldMapWidget::onTileReady(QNetworkReply* reply)
{
    reply->deleteLater();

    if (reply->url().host() == "nominatim.openstreetmap.org") {
        handleNominatimReply(reply);
        return;
    }

    const QStringList parts = reply->url().path().split('/');
    if (parts.size() < 4) return;

    const int z = parts[1].toInt();
    const int x = parts[2].toInt();
    const int y = parts[3].left(parts[3].lastIndexOf('.')).toInt();
    const QString key = tileKey(z, x, y);
    m_pending.remove(key);

    if (reply->error() != QNetworkReply::NoError) {
        /* Download offline: conta come completato anche se errore */
        if (m_dlDone > 0) {
            ++m_dlDone;
            emit tileDownloadProgress(m_dlDone, m_dlQueue.size() + m_dlDone);
        }
        return;
    }

    const QByteArray data = reply->readAll();
    QPixmap pix;
    if (pix.loadFromData(data)) {
        m_cache.insert(key, pix);

        /* Salva su disco se in modalità download offline */
        if (!m_tileDir.isEmpty()) {
            const QString dir  = m_tileDir + QString("%1/%2").arg(z).arg(x);
            const QString path = dir + QString("/%1.png").arg(y);
            if (!QFile::exists(path)) {
                QDir().mkpath(dir);
                pix.save(path, "PNG");
            }
        }
        update();
    }
}

/* ── Constructor ────────────────────────────────────────────────── */

WorldMapWidget::WorldMapWidget(QWidget* parent) : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setCursor(Qt::CrossCursor);
    setToolTip(
        "Clicca: seleziona luogo  |  "
        "Rotella: zoom  |  "
        "Trascina: pan  |  "
        "Doppio clic: reset");

    m_net = new QNetworkAccessManager(this);
    m_net->setAutoDeleteReplies(true);
    connect(m_net, &QNetworkAccessManager::finished,
            this,  &WorldMapWidget::onTileReady);

    /* Cache tile su disco */
    m_tileDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                + "/osm_tiles/";
    QDir().mkpath(m_tileDir);

    /* Timer download offline (100ms tra un tile e l'altro — rispetta usage policy OSM) */
    m_dlTimer = new QTimer(this);
    m_dlTimer->setInterval(100);
    connect(m_dlTimer, &QTimer::timeout, this, &WorldMapWidget::onDlTimerTick);

    /* centra su Roma (la persistenza dell'ultima posizione è opt-in,
       vedi enableViewPersistence()) */
    const QPointF wp = latLonToWorld(41.9, 12.5);
    m_centerX = wp.x();
    m_centerY = wp.y();

    /* ── Search overlay ── */
    m_searchBar = new QWidget(this);
    m_searchBar->setStyleSheet(
        "background:rgba(255,255,255,210);"
        "border-radius:4px;"
        "border:1px solid rgba(0,0,0,40);");

    auto* searchLay = new QHBoxLayout(m_searchBar);
    searchLay->setContentsMargins(4, 2, 4, 2);
    searchLay->setSpacing(3);

    m_searchEdit = new QLineEdit(m_searchBar);
    m_searchEdit->setPlaceholderText(tr("Cerca citt\xc3\xa0..."));
    m_searchEdit->setFrame(false);
    m_searchEdit->setStyleSheet("background:transparent; font-size:11px;");

    m_searchBtn = new QToolButton(m_searchBar);
    m_searchBtn->setText("\xf0\x9f\x94\x8d");
    m_searchBtn->setStyleSheet("border:none; background:transparent; font-size:13px;");
    m_searchBtn->setCursor(Qt::ArrowCursor);

    searchLay->addWidget(m_searchEdit);
    searchLay->addWidget(m_searchBtn);

    m_resultList = new QListWidget(this);
    m_resultList->setMaximumHeight(140);
    m_resultList->setStyleSheet(
        "background:white;"
        "border:1px solid #aaa;"
        "border-top:none;"
        "font-size:11px;");
    m_resultList->setCursor(Qt::ArrowCursor);
    m_resultList->hide();

    connect(m_searchBtn,  &QToolButton::clicked,
            this,         &WorldMapWidget::onSearchClicked);
    connect(m_searchEdit, &QLineEdit::returnPressed,
            this,         &WorldMapWidget::onSearchClicked);
    connect(m_resultList, &QListWidget::itemActivated,
            this,         &WorldMapWidget::onResultActivated);

    /* ── Zoom controls (top-right) ── */
    const QString btnStyle =
        "QToolButton { border:none; background:transparent; font-size:14px; font-weight:bold; }"
        "QToolButton:hover { background:rgba(0,0,0,30); border-radius:3px; }";

    m_zoomBar = new QWidget(this);
    m_zoomBar->setStyleSheet(
        "background:rgba(255,255,255,210);"
        "border-radius:4px;"
        "border:1px solid rgba(0,0,0,40);");

    auto* zoomLay = new QHBoxLayout(m_zoomBar);
    zoomLay->setContentsMargins(4, 2, 4, 2);
    zoomLay->setSpacing(2);

    m_btnZoomOut = new QToolButton(m_zoomBar);
    m_btnZoomOut->setText("\xe2\x88\x92");   /* − */
    m_btnZoomOut->setStyleSheet(btnStyle);
    m_btnZoomOut->setCursor(Qt::ArrowCursor);
    m_btnZoomOut->setToolTip(tr("Zoom out"));

    m_zoomLbl = new QLabel("z3", m_zoomBar);
    m_zoomLbl->setStyleSheet("font-size:11px; color:#333; min-width:24px; text-align:center;");
    m_zoomLbl->setAlignment(Qt::AlignCenter);

    m_btnZoomIn = new QToolButton(m_zoomBar);
    m_btnZoomIn->setText("+");
    m_btnZoomIn->setStyleSheet(btnStyle);
    m_btnZoomIn->setCursor(Qt::ArrowCursor);
    m_btnZoomIn->setToolTip(tr("Zoom in"));

    m_btnReset = new QToolButton(m_zoomBar);
    m_btnReset->setText("\xe2\x8c\x82");   /* ⌂ */
    m_btnReset->setStyleSheet(btnStyle);
    m_btnReset->setCursor(Qt::ArrowCursor);
    m_btnReset->setToolTip(tr("Reimposta vista"));

    zoomLay->addWidget(m_btnZoomOut);
    zoomLay->addWidget(m_zoomLbl);
    zoomLay->addWidget(m_btnZoomIn);
    zoomLay->addWidget(m_btnReset);

    connect(m_btnZoomOut, &QToolButton::clicked, this, &WorldMapWidget::onZoomOutClicked);
    connect(m_btnZoomIn,  &QToolButton::clicked, this, &WorldMapWidget::onZoomInClicked);
    connect(m_btnReset,   &QToolButton::clicked, this, &WorldMapWidget::onResetViewClicked);

    positionOverlay();
    updateZoomLabel();
}

WorldMapWidget::~WorldMapWidget()
{
    saveLastView();
}

/* Opt-in (vedi header): ripristina subito l'ultima vista salvata e attiva
   il salvataggio automatico per le modifiche successive. */
void WorldMapWidget::enableViewPersistence()
{
    m_persistView = true;

    QSettings settings("Prismalux", "GUI");
    const double lat0 = settings.value(P::SK::kOsmMapLastLat, 41.9).toDouble();
    const double lon0 = settings.value(P::SK::kOsmMapLastLon, 12.5).toDouble();
    m_zoom = qBound(kMinZoom, settings.value(P::SK::kOsmMapLastZoom, m_zoom).toInt(), kMaxZoom);
    m_numericLabels = settings.value(P::SK::kOsmMapNumericLabels, false).toBool();
    const QPointF wp = latLonToWorld(lat0, lon0);
    m_centerX = wp.x();
    m_centerY = wp.y();
    updateZoomLabel();
    update();
}

/* Persiste centro (lat/lon) e zoom correnti: alla prossima apertura la
   mappa riparte da dove l'utente si era spostato invece che da Roma.
   No-op se enableViewPersistence() non è mai stata chiamata (vedi header). */
void WorldMapWidget::saveLastView() const
{
    if (!m_persistView) return;
    double lat, lon;
    worldToLatLon(m_centerX, m_centerY, lat, lon);
    QSettings settings("Prismalux", "GUI");
    settings.setValue(P::SK::kOsmMapLastLat,  lat);
    settings.setValue(P::SK::kOsmMapLastLon,  lon);
    settings.setValue(P::SK::kOsmMapLastZoom, m_zoom);
}

void WorldMapWidget::setCoords(double lat, double lon)
{
    m_markerLat = qBound(-85.05, lat, 85.05);
    m_markerLon = qBound(-180.0, lon, 180.0);
    m_hasMarker = true;
    const QPointF wp = latLonToWorld(m_markerLat, m_markerLon);
    m_centerX = wp.x();
    m_centerY = wp.y();
    update();
}

/* ── Overlay: posiziona barra ricerca, lista risultati e zoom bar ── */

void WorldMapWidget::updateZoomLabel()
{
    m_zoomLbl->setText(QString("z%1").arg(m_zoom));
    m_btnZoomOut->setEnabled(m_zoom > kMinZoom);
    m_btnZoomIn ->setEnabled(m_zoom < kMaxZoom);
}

void WorldMapWidget::positionOverlay()
{
    const int margin  = 6;
    const int barH    = 28;
    /* search bar: larghezza max 220px per non sovrapporsi allo zoom bar */
    const int searchW = qMin(width() - 2 * margin, 220);
    m_searchBar->setGeometry(margin, margin, searchW, barH);
    m_resultList->setGeometry(margin, margin + barH,
                               searchW, m_resultList->maximumHeight());

    /* zoom bar: in alto a destra, larghezza fissa */
    const int zbW = 110;   /* larghezza fissa: − | z3 | + | ⌂ */
    const int zbH = barH;
    if (width() >= zbW + margin * 2)
        m_zoomBar->setGeometry(width() - zbW - margin, margin, zbW, zbH);
    m_zoomBar->raise();
}

/* ── Pulsanti zoom ──────────────────────────────────────────────── */

void WorldMapWidget::onZoomInClicked()
{
    const int newZ = qBound(kMinZoom, m_zoom + 1, kMaxZoom);
    if (newZ == m_zoom) return;
    const double factor = std::pow(2.0, newZ - m_zoom);
    m_centerX *= factor;
    m_centerY *= factor;
    m_zoom = newZ;
    updateZoomLabel();
    update();
    saveLastView();
}

void WorldMapWidget::onZoomOutClicked()
{
    const int newZ = qBound(kMinZoom, m_zoom - 1, kMaxZoom);
    if (newZ == m_zoom) return;
    const double factor = std::pow(2.0, newZ - m_zoom);
    m_centerX *= factor;
    m_centerY *= factor;
    m_zoom = newZ;
    updateZoomLabel();
    update();
    saveLastView();
}

void WorldMapWidget::onResetViewClicked()
{
    m_zoom = 3;
    const double lat = m_hasMarker ? m_markerLat : 41.9;
    const double lon = m_hasMarker ? m_markerLon : 12.5;
    const QPointF wp = latLonToWorld(lat, lon);
    m_centerX = wp.x();
    m_centerY = wp.y();
    updateZoomLabel();
    update();
    saveLastView();
}

void WorldMapWidget::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    positionOverlay();
}

/* ── Ricerca città (Nominatim) ──────────────────────────────────── */

void WorldMapWidget::onSearchClicked()
{
    const QString q = m_searchEdit->text().trimmed();
    if (q.isEmpty()) return;
    m_resultList->clear();
    m_resultList->hide();

    QUrl url("https://nominatim.openstreetmap.org/search");
    QUrlQuery params;
    params.addQueryItem("q",      q);
    params.addQueryItem("format", "json");
    params.addQueryItem("limit",  "6");
    url.setQuery(params);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  "Prismalux/2.8 (educational desktop app)");
    m_net->get(req);
}

void WorldMapWidget::handleNominatimReply(QNetworkReply* reply)
{
    if (reply->error() != QNetworkReply::NoError) return;
    const QJsonArray arr = QJsonDocument::fromJson(reply->readAll()).array();
    if (arr.isEmpty()) {
        m_resultList->addItem("Nessun risultato");
        m_resultList->setEnabled(false);
    } else {
        m_resultList->setEnabled(true);
        for (const QJsonValue& v : arr) {
            const QJsonObject o    = v.toObject();
            const double      lat  = o["lat"].toString().toDouble();
            const double      lon  = o["lon"].toString().toDouble();
            const QString     name = o["display_name"].toString();
            const QString     shortName = name.section(',', 0, 1).trimmed();
            auto* item = new QListWidgetItem(shortName, m_resultList);
            item->setData(Qt::UserRole,     lat);
            item->setData(Qt::UserRole + 1, lon);
            item->setData(Qt::UserRole + 2, shortName);
            item->setToolTip(name);
        }
    }
    m_resultList->show();
    m_resultList->raise();
}

void WorldMapWidget::onResultActivated(QListWidgetItem* item)
{
    if (!item || !(item->flags() & Qt::ItemIsEnabled)) return;
    const double lat  = item->data(Qt::UserRole).toDouble();
    const double lon  = item->data(Qt::UserRole + 1).toDouble();
    const QString nm  = item->data(Qt::UserRole + 2).toString();
    m_zoom = 11;
    setCoords(lat, lon);
    emit coordsChanged(m_markerLat, m_markerLon);
    emit cityNameChanged(nm);
    m_resultList->hide();
    m_searchEdit->clear();
    saveLastView();   // città cercata esplicitamente: riparti da qui la prossima volta

    /* In modalità percorso: propone menu "Aggiungi alla rotta" */
    if (m_routeMode) {
        QMenu menu(this);
        menu.addAction(
            "\xf0\x9f\x9f\xa2  Imposta come Partenza: " + nm,
            this, [this, lat, lon] {
                insertStartWaypoint(lat, lon);
            });
        menu.addAction(
            tr("\xe2\x9e\x95  Aggiungi come Tappa: ") + nm,
            this, [this, lat, lon] {
                addWaypoint(lat, lon);
                emit waypointAdded(m_waypointCoords.size() - 1, lat, lon);
            });
        /* Posiziona il menu sotto la barra di ricerca */
        const QPoint pos = m_searchBar->mapToGlobal(
            QPoint(0, m_searchBar->height() + 2));
        menu.exec(pos);
    }
}

/* ── paintEvent ─────────────────────────────────────────────────── */

void WorldMapWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    /* sfondo oceano (visibile prima che arrivino i tile) */
    p.fillRect(rect(), QColor(170, 210, 240));

    drawTiles(p);
    drawRouteLine(p);
    drawWaypoints(p);
    if (m_hasMarker && !m_routeMode) drawMarker(p);
    drawAttrib(p);
}

void WorldMapWidget::drawTiles(QPainter& p)
{
    const int tilesN = 1 << m_zoom;
    const double tlX = m_centerX - width()  / 2.0;
    const double tlY = m_centerY - height() / 2.0;

    const int tx0 = qMax(0, (int)std::floor(tlX / kTileSize));
    const int tx1 = qMin(tilesN - 1, (int)std::floor((tlX + width())  / kTileSize));
    const int ty0 = qMax(0, (int)std::floor(tlY / kTileSize));
    const int ty1 = qMin(tilesN - 1, (int)std::floor((tlY + height()) / kTileSize));

    for (int tx = tx0; tx <= tx1; ++tx) {
        for (int ty = ty0; ty <= ty1; ++ty) {
            const QString key = tileKey(m_zoom, tx, ty);
            const QPointF sp = worldToScreen(tx * kTileSize, ty * kTileSize);
            const QRect tileRect(sp.toPoint(), QSize(kTileSize + 1, kTileSize + 1));

            if (m_cache.contains(key)) {
                p.drawPixmap(tileRect, m_cache[key]);
            } else {
                p.fillRect(tileRect, QColor(200, 205, 215));
                p.setPen(QColor(150, 155, 165));
                QFont f = font(); f.setPixelSize(dpiScale(11)); p.setFont(f);
                p.drawText(tileRect, Qt::AlignCenter, "\xf0\x9f\x97\xba");
                requestTile(m_zoom, tx, ty);
            }
        }
    }
}

void WorldMapWidget::drawMarker(QPainter& p)
{
    const QPointF wp = latLonToWorld(m_markerLat, m_markerLon);
    const QPointF sp = worldToScreen(wp.x(), wp.y());

    /* ombra */
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 60));
    p.drawEllipse(sp + QPointF(1, 1), 6, 6);

    /* cerchio rosso */
    p.setPen(QPen(Qt::white, 1.5));
    p.setBrush(QColor(220, 40, 40));
    p.drawEllipse(sp, 6, 6);

    /* croce */
    p.setPen(QPen(QColor(220, 40, 40, 180), 1.0));
    p.drawLine(sp + QPointF(-12, 0), sp + QPointF(-7, 0));
    p.drawLine(sp + QPointF(  7, 0), sp + QPointF(12, 0));
    p.drawLine(sp + QPointF(0, -12), sp + QPointF(0, -7));
    p.drawLine(sp + QPointF(0,   7), sp + QPointF(0, 12));
}

void WorldMapWidget::drawAttrib(QPainter& p)
{
    const QString txt = "\xa9 OpenStreetMap contributors";
    QFont f = font(); f.setPixelSize(dpiScale(9)); p.setFont(f);
    const QFontMetrics fm(f);
    const int tw = fm.horizontalAdvance(txt);
    const QRectF bg(width() - tw - 8, height() - 14, tw + 6, 12);
    p.fillRect(bg, QColor(255, 255, 255, 190));
    p.setPen(QColor(30, 30, 30));
    p.drawText(bg, Qt::AlignCenter, txt);
}

/* ── Zoom ───────────────────────────────────────────────────────── */

void WorldMapWidget::wheelEvent(QWheelEvent* e)
{
    const int delta  = e->angleDelta().y() > 0 ? 1 : -1;
    const int newZ   = qBound(kMinZoom, m_zoom + delta, kMaxZoom);
    if (newZ == m_zoom) { e->accept(); return; }

    /* zoom verso il cursore */
    const double sx = e->position().x();
    const double sy = e->position().y();
    const double factor = std::pow(2.0, newZ - m_zoom);

    m_centerX = m_centerX * factor + (sx - width()  / 2.0) * (factor - 1.0);
    m_centerY = m_centerY * factor + (sy - height() / 2.0) * (factor - 1.0);
    m_zoom = newZ;

    updateZoomLabel();
    update();
    e->accept();
}

/* ── Pan + click ────────────────────────────────────────────────── */

void WorldMapWidget::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton) {
        m_dragging      = true;
        m_dragStartScreen = e->pos();
        m_dragStartCX   = m_centerX;
        m_dragStartCY   = m_centerY;
        m_pressPos      = e->pos();
        setCursor(Qt::ClosedHandCursor);
    }
}

void WorldMapWidget::mouseMoveEvent(QMouseEvent* e)
{
    if (m_dragging) {
        const QPointF d = e->pos() - m_dragStartScreen;
        m_centerX = m_dragStartCX - d.x();
        m_centerY = m_dragStartCY - d.y();
        update();
    }
}

void WorldMapWidget::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton) return;
    setCursor(Qt::CrossCursor);
    m_dragging = false;
    saveLastView();   // fine di un eventuale trascinamento: persiste la nuova vista

    if ((QPointF(e->pos()) - m_pressPos).manhattanLength() <= 4) {
        const QPointF w = screenToWorld(e->pos().x(), e->pos().y());
        double lat, lon;
        worldToLatLon(w.x(), w.y(), lat, lon);
        lat = qBound(-85.05, lat, 85.05);
        lon = qBound(-180.0, lon, 180.0);

        if (m_routeMode) {
            addWaypoint(lat, lon);
            emit waypointAdded(m_waypointCoords.size() - 1, lat, lon);
        } else {
            m_markerLat = lat;
            m_markerLon = lon;
            m_hasMarker = true;
            update();
            emit coordsChanged(m_markerLat, m_markerLon);
        }
    }
}

void WorldMapWidget::mouseDoubleClickEvent(QMouseEvent*)
{
    onResetViewClicked();
}

/* ── Route / Itinerario ─────────────────────────────────────────── */

void WorldMapWidget::setRouteMode(bool on)
{
    m_routeMode = on;
    setToolTip(on
        ? "Clicca sulla mappa per aggiungere waypoint  |  Rotella: zoom  |  Trascina: pan"
        : "Clicca: seleziona luogo  |  Rotella: zoom  |  Trascina: pan  |  Doppio clic: reset");
}

/* Etichetta automatica per indice 0-based: stile "excel" A, B, ... Z, poi
   A1, B1, ... Z1, A2, ... (non torna mai ad A da sola oltre la 26esima
   tappa) oppure numerica progressiva 1, 2, 3, ..., a scelta dell'utente
   (setLabelStyle). */
QString WorldMapWidget::autoLabel(int idx) const
{
    if (m_numericLabels) return QString::number(idx + 1);
    const int letter = idx % 26;
    const int cycle  = idx / 26;
    QString s(QChar('A' + letter));
    if (cycle > 0) s += QString::number(cycle);
    return s;
}

void WorldMapWidget::setLabelStyle(bool numeric)
{
    m_numericLabels = numeric;
    QSettings("Prismalux", "GUI").setValue(P::SK::kOsmMapNumericLabels, numeric);
}

void WorldMapWidget::addWaypoint(double lat, double lon, const QString& label)
{
    const int idx = m_waypointCoords.size();
    m_waypointCoords.append({lat, lon});
    m_waypointLabels.append(label.isEmpty() ? autoLabel(idx) : label);
    m_waypointCustomLabel.append(!label.isEmpty());
    update();
}

bool WorldMapWidget::renameWaypoint(int idx, const QString& label)
{
    const QString trimmed = label.trimmed();
    if (idx < 0 || idx >= m_waypointLabels.size() || trimmed.isEmpty())
        return false;
    m_waypointLabels[idx] = trimmed;
    m_waypointCustomLabel[idx] = true;
    update();
    return true;
}

void WorldMapWidget::clearRoute()
{
    m_waypointCoords.clear();
    m_waypointLabels.clear();
    m_waypointCustomLabel.clear();
    m_routeLine.clear();
    update();
}

void WorldMapWidget::setRouteLine(const QVector<QPair<double,double>>& pts)
{
    m_routeLine = pts;
    update();
}

void WorldMapWidget::drawWaypoints(QPainter& p)
{
    for (int i = 0; i < m_waypointCoords.size(); ++i) {
        const QPointF wp = latLonToWorld(m_waypointCoords[i].first,
                                         m_waypointCoords[i].second);
        const QPointF sp = worldToScreen(wp.x(), wp.y());

        /* Colore: A=verde, B=rosso, intermedi=arancio */
        QColor col = (i == 0) ? QColor(34, 197, 94)
                   : (i == m_waypointCoords.size() - 1) ? QColor(239, 68, 68)
                   : QColor(249, 115, 22);

        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 50));
        p.drawEllipse(sp + QPointF(1.5, 1.5), 9, 9);

        p.setPen(QPen(Qt::white, 2));
        p.setBrush(col);
        p.drawEllipse(sp, 9, 9);

        /* Etichetta */
        const QString lbl = m_waypointLabels.value(i, QString::fromLatin1("%1").arg(QChar('A' + i % 26)));
        QFont f = font(); f.setPixelSize(dpiScale(10)); f.setBold(true); p.setFont(f);
        p.setPen(Qt::white);
        p.drawText(QRectF(sp.x() - 9, sp.y() - 9, 18, 18), Qt::AlignCenter, lbl);
    }
}

void WorldMapWidget::drawRouteLine(QPainter& p)
{
    if (m_routeLine.size() < 2) return;

    QVector<QPointF> screen;
    screen.reserve(m_routeLine.size());
    for (const auto& ll : m_routeLine) {
        const QPointF wp = latLonToWorld(ll.first, ll.second);
        screen.append(worldToScreen(wp.x(), wp.y()));
    }

    p.setPen(QPen(QColor(0, 0, 0, 60), 5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawPolyline(screen.data(), screen.size());

    p.setPen(QPen(QColor(37, 99, 235), 3.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawPolyline(screen.data(), screen.size());
}

/* ── Menu contestuale (tasto destro) ───────────────────────────── */

void WorldMapWidget::contextMenuEvent(QContextMenuEvent* e)
{
    if (!m_routeMode) { e->ignore(); return; }

    const QPointF w = screenToWorld(e->pos().x(), e->pos().y());
    worldToLatLon(w.x(), w.y(), m_ctxLat, m_ctxLon);

    QMenu menu(this);
    menu.addAction(
        tr("\xf0\x9f\x9f\xa2  Imposta come Partenza"),
        this, [this] {
            insertStartWaypoint(m_ctxLat, m_ctxLon);
        });
    menu.addAction(
        tr("\xe2\x9e\x95  Aggiungi Tappa"),
        this, [this] {
            addWaypoint(m_ctxLat, m_ctxLon);
            emit waypointAdded(m_waypointCoords.size() - 1, m_ctxLat, m_ctxLon);
        });
    menu.exec(e->globalPos());
    e->accept();
}

/* ── insertStartWaypoint — inserisce in posizione 0 e ri-etichetta ── */

void WorldMapWidget::insertStartWaypoint(double lat, double lon, const QString& label)
{
    m_waypointCoords.prepend({lat, lon});
    m_waypointLabels.prepend(label.isEmpty() ? autoLabel(0) : label);
    m_waypointCustomLabel.prepend(!label.isEmpty());
    /* Ri-assegna le etichette automatiche dall'inizio, ma senza toccare
       quelle rinominate a mano dall'utente (renameWaypoint). */
    for (int i = 0; i < m_waypointLabels.size(); ++i)
        if (!m_waypointCustomLabel[i])
            m_waypointLabels[i] = autoLabel(i);
    update();
    emit waypointsReset();
}

/* ── downloadVisibleTiles — scarica i tile visibili nella cache disco ── */

void WorldMapWidget::downloadVisibleTiles()
{
    if (m_dlTimer->isActive()) return;  /* download già in corso */

    m_dlQueue.clear();
    m_dlDone = 0;

    /* Scarica zoom corrente + zoom-1 per avere un livello overview */
    const int zMin = qMax(kMinZoom, m_zoom - 1);
    const int zMax = m_zoom;

    for (int z = zMin; z <= zMax; ++z) {
        const int tilesN = 1 << z;
        const double scale = kTileSize * (double)tilesN;

        /* Centro attuale ricampionato allo zoom z */
        const double factor = (double)tilesN / (double)(1 << m_zoom);
        const double cx = m_centerX * factor;
        const double cy = m_centerY * factor;

        /* Area visibile in coordinate tile */
        const double halfW = width()  / 2.0;
        const double halfH = height() / 2.0;
        const int tx0 = qMax(0,          (int)std::floor((cx - halfW) / kTileSize));
        const int tx1 = qMin(tilesN - 1, (int)std::floor((cx + halfW) / kTileSize));
        const int ty0 = qMax(0,          (int)std::floor((cy - halfH) / kTileSize));
        const int ty1 = qMin(tilesN - 1, (int)std::floor((cy + halfH) / kTileSize));

        for (int tx = tx0; tx <= tx1 && m_dlQueue.size() < 300; ++tx)
            for (int ty = ty0; ty <= ty1 && m_dlQueue.size() < 300; ++ty) {
                const QString diskPath = m_tileDir +
                    QString("%1/%2/%3.png").arg(z).arg(tx).arg(ty);
                if (!QFile::exists(diskPath))
                    m_dlQueue.append({z, tx, ty});
            }
    }

    if (m_dlQueue.isEmpty()) {
        emit tileDownloadProgress(0, 0);  /* tutto già in cache */
        return;
    }
    m_dlTimer->start();
    emit tileDownloadProgress(0, m_dlQueue.size());
}
