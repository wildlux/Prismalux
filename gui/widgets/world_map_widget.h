#pragma once
#include <QWidget>
#include <QPointF>
#include <QMap>
#include <QSet>
#include <QPixmap>
#include <QTimer>

class QNetworkAccessManager;
class QNetworkReply;
class QLineEdit;
class QToolButton;
class QLabel;
class QListWidget;
class QListWidgetItem;

/* ══════════════════════════════════════════════════════════════
   WorldMapWidget — OpenStreetMap tile widget (Qt6::Network)
   Click: imposta marcatore  |  Rotella: zoom  |  Trascina: pan
   Doppio clic: reset alla vista iniziale
   Ricerca città via Nominatim (overlay in alto a sinistra)
   ══════════════════════════════════════════════════════════════ */
class WorldMapWidget : public QWidget {
    Q_OBJECT
public:
    explicit WorldMapWidget(QWidget* parent = nullptr);
    ~WorldMapWidget() override;

    void   setCoords(double lat, double lon);
    double lat() const { return m_markerLat; }
    double lon() const { return m_markerLon; }
    bool   hasSelection() const { return m_hasMarker; }

    QSize sizeHint()        const override { return {420, 184}; }
    QSize minimumSizeHint() const override { return {200, 100}; }

    /** Opt-in: ripristina l'ultima posizione/zoom/stile etichette salvati
        (QSettings, chiave condivisa) e da qui in poi li persiste ad ogni
        spostamento. Disattivato di default — più istanze di questo widget
        (es. mappa routing vs. Carta Astrale) altrimenti si sovrascriverebbero
        a vicenda l'ultima posizione salvata. */
    void enableViewPersistence();

    /* ── Route / Itinerario ── */
    void setRouteMode(bool on);
    void addWaypoint(double lat, double lon, const QString& label = {});
    void insertStartWaypoint(double lat, double lon, const QString& label = {});
    void clearRoute();
    void setRouteLine(const QVector<QPair<double,double>>& pts);
    const QVector<QPair<double,double>>& waypoints()     const { return m_waypointCoords; }
    const QVector<QString>&             waypointLabels() const { return m_waypointLabels; }
    /** Rinomina una tappa esistente (0-based); l'etichetta resta fissa e non
        viene più sovrascritta dalla rietichettatura automatica di
        insertStartWaypoint. Ritorna false se idx fuori range o nome vuoto. */
    bool renameWaypoint(int idx, const QString& label);

    /* ── Etichette automatiche tappe ──
       "excel": A, B, ... Z, A1, B1, ... (di default)
       numeriche: 1, 2, 3, ...           */
    void setLabelStyle(bool numeric);
    bool numericLabelStyle() const { return m_numericLabels; }

    /* ── Tile offline ── */
    void downloadVisibleTiles();   // scarica i tile visibili nella cache disco

signals:
    void coordsChanged(double lat, double lon);
    void cityNameChanged(const QString& name);
    void waypointAdded(int idx, double lat, double lon);
    void waypointsReset();                               // lista waypoint ricostruita (insertStart)
    void tileDownloadProgress(int done, int total);      // avanzamento download offline

protected:
    void paintEvent(QPaintEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void contextMenuEvent(QContextMenuEvent*) override;

private slots:
    void onTileReady(QNetworkReply* reply);
    void onDlTimerTick();
    void onSearchClicked();
    void onResultActivated(QListWidgetItem* item);
    void onZoomInClicked();
    void onZoomOutClicked();
    void onResetViewClicked();

private:
    QPointF latLonToWorld(double lat, double lon) const;
    void    worldToLatLon(double wx, double wy, double& lat, double& lon) const;
    QPointF worldToScreen(double wx, double wy) const;
    QPointF screenToWorld(double sx, double sy) const;
    QString tileKey(int z, int x, int y) const;
    void    requestTile(int z, int x, int y);
    void    drawTiles(QPainter& p);
    void    drawMarker(QPainter& p);
    void    drawAttrib(QPainter& p);
    void    positionOverlay();
    void    updateZoomLabel();
    void    handleNominatimReply(QNetworkReply* reply);
    void    drawWaypoints(QPainter& p);
    void    drawRouteLine(QPainter& p);
    QString autoLabel(int idx) const;      // etichetta automatica per indice (stile excel o numerico)
    void    saveLastView() const;          // persiste centro+zoom correnti (QSettings)

    /* route */
    bool    m_routeMode = false;
    QVector<QPair<double,double>> m_waypointCoords;
    QVector<QString>              m_waypointLabels;
    QVector<bool>                 m_waypointCustomLabel;  // true = rinominata a mano, non toccare
    QVector<QPair<double,double>> m_routeLine;
    bool    m_numericLabels = false;
    bool    m_persistView   = false;   // vedi enableViewPersistence()

    /* context menu — coordinate right-click */
    double  m_ctxLat = 0.0;
    double  m_ctxLon = 0.0;

    /* tile offline download */
    struct TileJob { int z, x, y; };
    QVector<TileJob> m_dlQueue;
    QTimer*          m_dlTimer  = nullptr;
    int              m_dlDone   = 0;
    QString          m_tileDir; // ~/.local/share/Prismalux/osm_tiles/

    /* zoom/pan */
    int     m_zoom      = 3;
    double  m_centerX   = 0.0;
    double  m_centerY   = 0.0;
    QPointF m_dragStartScreen;
    double  m_dragStartCX = 0.0;
    double  m_dragStartCY = 0.0;
    bool    m_dragging    = false;
    QPointF m_pressPos;

    /* marcatore */
    double  m_markerLat = 41.9;
    double  m_markerLon = 12.5;
    bool    m_hasMarker = false;

    /* tile cache */
    QNetworkAccessManager* m_net = nullptr;
    QMap<QString, QPixmap> m_cache;
    QSet<QString>          m_pending;

    /* search overlay (top-left) */
    QWidget*     m_searchBar  = nullptr;
    QLineEdit*   m_searchEdit = nullptr;
    QToolButton* m_searchBtn  = nullptr;
    QListWidget* m_resultList = nullptr;

    /* zoom controls (top-right) */
    QWidget*     m_zoomBar    = nullptr;
    QToolButton* m_btnZoomOut = nullptr;
    QLabel*      m_zoomLbl    = nullptr;
    QToolButton* m_btnZoomIn  = nullptr;
    QToolButton* m_btnReset   = nullptr;

    static constexpr int kTileSize  = 256;
    static constexpr int kMinZoom   = 1;
    static constexpr int kMaxZoom   = 17;
    static constexpr int kMaxCache  = 400;
};
