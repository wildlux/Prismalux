// ============================================================================
//  widget_vision3d.h — Prismalux Vision3D (scansione 3D da telefono/tablet)
//
//  Scheda (QWidget) con server HTTPS integrato che riceve foto da PIÙ
//  telefoni/tablet su un unico progetto/sessione e le analizza sul PC:
//  descrizione VLM (Ollama) + box (OpenCV) + depth map (script Python MiDaS)
//  + scala reale automatica via marker ArUco (Tools/aruco/).
//
//  Multi-device:
//   - ogni telefono riceve un ID automatico (cookie persistente) es. "tel01"
//   - foto salvate in  scan_output/<sessione>/<deviceId>/  + ID nel nome file
//   - lock (QMutex) per evitare collisioni di numerazione tra device simultanei
//   - pannello "Device attivi" con conteggio foto per telefono
//
//  Dipendenze Qt:   Qt6::Core Qt6::Widgets Qt6::Network (QSslServer, Qt ≥ 6.4)
//  Dipendenze opz.: OpenCV (box) + opencv-contrib aruco (scala automatica)
//                   — vedi VISION3D_USE_OPENCV / VISION3D_USE_ARUCO nel CMake
// ============================================================================
#pragma once

#include <QWidget>
#include <QtNetwork/qtnetworkglobal.h>   // definisce QT_CONFIG(ssl) prima dell'#if
#if QT_CONFIG(ssl)
#  include <QSslServer>
#  include <QSslConfiguration>
#  include <QSslSocket>
#endif
#include <QNetworkAccessManager>
#include <QString>
#include <QByteArray>
#include <QHash>
#include <QMutex>
#include <QDateTime>
#include <QVector>
#include <QPoint>
#include <QProcess>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;
class QComboBox;
class QListWidget;
class QListWidgetItem;
class QNetworkReply;

// Risultato di una singola foto analizzata.
struct Vision3DResult {
    int     index = 0;
    QString session;
    QString deviceId;                    // es. "tel01"
    int     angle = 0;                   // heading/bussola (gradi)
    int     pitch = 0;                   // inclinazione avanti/indietro
    int     roll  = 0;                   // inclinazione laterale
    bool    hasSensors = false;          // true se i valori vengono dai sensori
    // scala reale via ArUco (0 se nessun marker rilevato)
    double  scaleMmPerUnit = 0.0;        // mm reali per unità immagine (px lato marker)
    int     arucoMarkersFound = 0;       // quanti marker visti in questa foto
    QString description;                 // dal VLM (vuoto se disabilitato)
    int     boxesCount = 0;
    QByteArray boxesJpeg;                // JPEG annotato (vuoto se disabilitato)
    QByteArray depthJpeg;                // JPEG depth colormap (vuoto se n/d)
    QString savedPath;                   // cartella su disco
    QString error;
};

// Stato di un device connesso (per il pannello UI).
struct Vision3DDevice {
    QString   id;
    int       photoCount = 0;
    QDateTime lastSeen;
    QString   userAgent;
};

// Uno scatto nella scena 3D (posa camera stimata da bussola + inclinazione).
struct V3dShotInfo {
    QString key;            // basePath del file (identifica lo scatto)
    QString device;
    int     index   = 0;
    int     heading = 0;    // bussola 0-359 (posizione sul cerchio)
    int     pitch   = 0;    // inclinazione (quota indicativa della camera)
    bool    hasSensors = false;
};

// Mini-scena 3D: oggetto al centro, un "cono" (frustum) per ogni scatto
// disposto sul cerchio all'angolo di bussola. Trascina = orbita, rotella = zoom.
// Senza sensori la disposizione è indicativa (spaziatura uniforme, tratteggio).
class Vision3DSceneCanvas : public QWidget {
    Q_OBJECT
public:
    explicit Vision3DSceneCanvas(QWidget* parent = nullptr);
    void addShot(const V3dShotInfo& s);
    void setSelectedKey(const QString& key);
    void clearShots();
    int  shotCount() const { return int(m_shots.size()); }

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;

private:
    QPointF project(double x, double y, double z) const;
    void drawFrustum(QPainter& p, const V3dShotInfo& s, bool selected) const;

    QVector<V3dShotInfo> m_shots;
    QString m_selectedKey;
    double  m_yawDeg  = 30.0;   // orbita orizzontale (drag)
    double  m_tiltDeg = 28.0;   // inclinazione vista (drag verticale)
    double  m_zoom    = 1.0;
    QPoint  m_lastDrag;
};

class Vision3DWidget : public QWidget {
    Q_OBJECT
public:
    explicit Vision3DWidget(QWidget* parent = nullptr);
    ~Vision3DWidget() override;

    bool start(quint16 port = 0,                     // 0 = P::kVision3DPort
               const QString& certPath = QString(),  // vuoto = cert condiviso
               const QString& keyPath  = QString()); //         ~/.prismalux/
    void stop();
    bool isRunning() const { return m_running; }

    void setOutputDir(const QString& dir);          // default: P::root()/scan_output
    void setOllamaUrl(const QString& url);          // default: localhost:P::kOllamaPort
    void setVlmModel(const QString& model);         // default: qwen2.5-vl:3b
    void setDepthScript(const QString& pyPath);     // default: Tools/scripts/depth_infer.py
    void setPythonExe(const QString& py);           // default: P::findPython()
    void setArucoMarkerMm(double mm);               // lato reale marker (default 40)
    void setBindIp(const QString& ip);              // default: primo IP LAN 192.168.x

signals:
    void serverStarted(const QString& url);
    void photoReceived(int index, const QString& session, const QString& deviceId);
    void analysisReady(const Vision3DResult& result);
    void deviceSeen(const QString& deviceId);
    void logMessage(const QString& line);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onSocketDisconnected();
    void onToggleServerClicked();
    void onRefreshIfacesClicked();
    void onGalleryItemClicked(QListWidgetItem* item);
    void onVlmModelChanged(const QString& model);
    void onVlmTagsReady();
    void onSessionComboChanged(const QString& session);
    void onReconStartClicked();
    void onReconProcOutput();
    void onReconProcFinished(int code, QProcess::ExitStatus status);

private:
    struct ParsedRequest {
        QByteArray method, path, body;
        QByteArray cookieDeviceId;       // da header Cookie
        QByteArray userAgent;
    };

#if QT_CONFIG(ssl)
    void handleRequest(QSslSocket* sock, const ParsedRequest& req);
    void sendHtml(QSslSocket* sock, const QByteArray& html, const QByteArray& setCookie = QByteArray());
    void sendJson(QSslSocket* sock, const QByteArray& json, const QByteArray& setCookie = QByteArray());
    void send404(QSslSocket* sock);
#endif

    // multi-device
    QString assignDeviceId(const QByteArray& existingCookie, const QByteArray& ua);
    void    refreshDeviceTable();

    // pipeline
    Vision3DResult analyze(const QString& session, const QString& deviceId, int angle,
                           int pitch, int roll, bool hasSensors,
                           const QByteArray& jpeg,
                           bool doDesc, bool doBoxes, bool doDepth);
    QString    vlmDescribe(const QByteArray& jpeg);
    QByteArray detectBoxes(const QByteArray& jpeg, int& nBoxes);
    QByteArray depthMap(const QByteArray& jpeg);
    // rileva marker ArUco: ritorna mm/unità (0 se nessuno) e n. marker in 'found'
    double     detectAruco(const QByteArray& jpeg, int& found);
    // IPv4 locali ordinati per preferenza LAN (192.168.x prima; escluse
    // interfacce virtuali docker/virbr/vnet e loopback in coda)
    static QStringList listLocalIps();
    QString    currentBindIp() const;
    static QByteArray htmlPage();

    void buildUi();
    void appendLog(const QString& s);
    void fetchVlmModels();          // popola il combo VLM da Ollama /api/tags
    void populateSessions(const QString& select = QString());  // scansiona scan_output/
    void loadSessionIntoUi(const QString& session);  // galleria + scena 3D da disco

#if QT_CONFIG(ssl)
    QSslServer*       m_server = nullptr;
    QSslConfiguration m_sslConf;
#endif
    QNetworkAccessManager* m_net = nullptr;

    QString m_outputDir;
    QString m_ollamaUrl;
    QString m_vlmModel;
    QString m_depthScript;
    QString m_pythonExe;
    double  m_arucoMarkerMm = 40.0;           // lato reale dei marker stampati
    quint16 m_port = 0;
    QString m_bindIp;                         // set esplicito (test/API); vuoto = combo

    // stato multi-device (protetto da m_lock perché più connessioni concorrenti)
    QMutex                        m_lock;
    QHash<QString, Vision3DDevice> m_devices;   // deviceId -> info
    int                           m_deviceCounter = 0;
    QHash<QObject*, QByteArray>   m_rxBuffers;  // buffer HTTP per socket

    // UI
    QLabel*         m_urlLabel   = nullptr;
    QLabel*         m_statusDot  = nullptr;
    QPushButton*    m_toggleBtn  = nullptr;
    QPlainTextEdit* m_log        = nullptr;
    QLabel*         m_lastDesc   = nullptr;
    QLabel*         m_lastThumb  = nullptr;
    QLabel*         m_lastDepth  = nullptr;
    QLabel*         m_lastBoxes  = nullptr;
    QLineEdit*      m_portEdit   = nullptr;
    QComboBox*      m_ifaceCombo = nullptr;
    QComboBox*      m_vlmCombo   = nullptr;
    QListWidget*    m_gallery    = nullptr;   // tutti gli scatti, clic = rivedi
    QTableWidget*   m_deviceTable = nullptr;
    QNetworkReply*  m_tagsReply  = nullptr;   // fetch /api/tags in corso

    // scena 3D + ricostruzione
    Vision3DSceneCanvas* m_scene = nullptr;
    QComboBox*      m_sessionCombo = nullptr;
    QComboBox*      m_qualityCombo = nullptr;
    QPushButton*    m_reconBtn   = nullptr;
    QProcess*       m_reconProc  = nullptr;
    int             m_reconStep  = 0;         // 0=reconstructor 1=export PLY
    QString         m_reconSessionDir;

    bool m_running = false;
};
