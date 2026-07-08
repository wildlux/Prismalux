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
#include <QJsonObject>

class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QComboBox;
class QListWidget;
class QListWidgetItem;
class QNetworkReply;
class QSpinBox;
class QrCodeWidget;

// Quali analisi calcolare per uno scatto — bundle dei flag "wants" del
// telefono, per non far crescere all'infinito i parametri di analyze().
struct Vision3DWants {
    bool desc   = false;   // descrizione VLM (Ollama)
    bool boxes  = false;   // box oggetti (OpenCV, Canny+contorni)
    bool depth  = false;   // depth map (script Python MiDaS)
    bool edges  = false;   // bordi (OpenCV, Canny)
    bool bump   = false;   // bump map (luminanza equalizzata = altezza)
    bool normal = false;   // normal map (gradienti Sobel → normali tangent-space)
};

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
    QByteArray edgesJpeg;                // JPEG bordi (Canny, vuoto se disabilitato/no OpenCV)
    QByteArray bumpJpeg;                 // JPEG bump map (vuoto se disabilitato/no OpenCV)
    QByteArray normalJpeg;               // JPEG normal map (vuoto se disabilitato/no OpenCV)
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
// Oltre agli scatti reali, mostra dei tasselli-bersaglio semi-trasparenti
// distribuiti sulla sfera (setTargetQuality): pieni/evidenziati dove uno
// scatto già copre quella zona, tratteggiati dove serve ancora — una guida
// visiva a "quanti e dove" servono scatti per la qualità scelta.
class Vision3DSceneCanvas : public QWidget {
    Q_OBJECT
public:
    explicit Vision3DSceneCanvas(QWidget* parent = nullptr);
    void addShot(const V3dShotInfo& s);
    void setSelectedKey(const QString& key);
    void clearShots();
    int  shotCount() const { return int(m_shots.size()); }
    /** Aggiorna la posa di uno scatto (modifica manuale). Ritorna false se key ignota. */
    bool setShotPose(const QString& key, int heading, int pitch);
    /** Ricalcola i tasselli-bersaglio per "low"/"medium"/"high"; stringa
        vuota o sconosciuta = nessun bersaglio mostrato. */
    void setTargetQuality(const QString& quality);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;

private:
    QPointF project(double x, double y, double z) const;
    void drawFrustum(QPainter& p, const V3dShotInfo& s, bool selected) const;
    void drawTargets(QPainter& p) const;
    double shotHeight(const V3dShotInfo& s) const;   // quota sulla sfera (stessa formula di drawFrustum)

    QVector<V3dShotInfo> m_shots;
    QString m_selectedKey;
    QString m_targetQuality;                    // "low" | "medium" | "high" | vuoto
    QVector<QPair<double,int>> m_targets;        // (bussola gradi, indice anello 0..2)
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

    /* ── Gestione sessioni/foto — programmabile anche senza passare dai
       dialog UI (utile per embedder esterni o automazioni). ── */
    // Sanifica il nome e crea la cartella vuota; "" se il nome non è valido.
    // description (opzionale) → scan_output/<session>/session.json.
    QString createSession(const QString& rawName, const QString& description = QString());
    // Copia i file immagine in scan_output/<session>/import/ con la stessa
    // numerazione *_a???.jpg degli scatti da telefono (has_sensors=false).
    // Ritorna quante immagini sono state importate con successo.
    int importPhotoFiles(const QString& session, const QStringList& files);
    // Elimina foto + box + depth + sidecar di uno scatto (base = percorso
    // senza estensione, come Qt::UserRole in galleria).
    void deleteShot(const QString& base);
    // Nota libera sulla sessione (es. "cubo con croce rossa, test point
    // cloud"), persistita in scan_output/<session>/session.json.
    QString sessionDescription(const QString& session) const;
    void    setSessionDescription(const QString& session, const QString& description);

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
    void onPrepSessionComboChanged(const QString& session);   // combo Preparazione → sincronizza e ricarica
    void onNewSessionClicked();     // crea una sessione vuota col nome scelto (notturno, villa, ...)
    void onEditSessionDescriptionClicked();  // inserisce/modifica la descrizione della sessione corrente
    void onEditShotDescriptionClicked();     // inserisce/modifica la descrizione dello scatto selezionato
    void onImportPhotosClicked();   // importa foto esistenti da disco nella sessione corrente
    void onDeletePhotoClicked();    // elimina gli scatti selezionati in galleria (anche più di uno)
    void onDeleteAllPhotosClicked(); // elimina TUTTI gli scatti della sessione corrente
    void onThumbContextMenu(const QPoint& pos);  // tasto destro su una miniatura → salva quella immagine
    void onSaveAllMapsClicked();    // salva tutte le mappe dello scatto selezionato in una cartella
    void onDeviceTableContextMenu(const QPoint& pos);  // tasto destro su un device → elimina device+foto
    void onGalleryContextMenu(const QPoint& pos);  // tasto destro in galleria → "Salva sul PC" (anche multi-selezione)
    void onPoseSpinChanged();
    void onDistributeClicked();
    void onHelpClicked();
    void onQualityComboChanged(const QString& quality);
    void onReconRecheckClicked();
    void onReconStartClicked();
    void onReconProcOutput();
    void onReconProcFinished(int code, QProcess::ExitStatus status);

private:
    // doppio click su una miniatura di analisi (box/depth/bordi/bump/normal)
    // in "Ultimo scatto analizzato" → intercettato qui (QLabel non ha un
    // segnale doubleClicked nativo), vedi recomputeThumb().
    bool eventFilter(QObject* watched, QEvent* event) override;
    // ricalcola sul momento UNA sola mappa (suffix es. "_edges.jpg") per lo
    // scatto selezionato, la salva su disco e aggiorna solo quella miniatura
    void recomputeThumb(QLabel* thumb, const QString& suffix, const QString& label);

    struct ParsedRequest {
        QByteArray method, path, body;
        QByteArray cookieDeviceId;       // da header Cookie
        QByteArray userAgent;
    };

#if QT_CONFIG(ssl)
    void handleRequest(QSslSocket* sock, const ParsedRequest& req);
    void sendHtml(QSslSocket* sock, const QByteArray& html, const QByteArray& setCookie = QByteArray());
    void sendJson(QSslSocket* sock, const QByteArray& json, const QByteArray& setCookie = QByteArray());
    void sendFile(QSslSocket* sock, const QString& path, const QString& downloadName);
    void send404(QSslSocket* sock);
#endif

    // multi-device
    QString assignDeviceId(const QByteArray& existingCookie, const QByteArray& ua);
    void    refreshDeviceTable();

    // pipeline
    // extraSensors: campi opzionali dal telefono copiati così come sono nel
    // sidecar .json (accelerometro grezzo, giroscopio, altitudine GPS,
    // test flash) — vedi analyze() per le chiavi accettate.
    Vision3DResult analyze(const QString& session, const QString& deviceId, int angle,
                           int pitch, int roll, bool hasSensors,
                           const QByteArray& jpeg, const Vision3DWants& wants,
                           const QString& scanMode = QStringLiteral("object"),
                           const QJsonObject& extraSensors = QJsonObject());
    QString    vlmDescribe(const QByteArray& jpeg);
    QByteArray detectBoxes(const QByteArray& jpeg, int& nBoxes);
    QByteArray depthMap(const QByteArray& jpeg);
    QByteArray edgeMap(const QByteArray& jpeg);    // bordi (OpenCV Canny)
    QByteArray bumpMap(const QByteArray& jpeg);    // bump map (luminanza equalizzata)
    QByteArray normalMap(const QByteArray& jpeg);  // normal map (gradienti Sobel)
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
    void writePoseSidecar(const QString& base, int heading, int pitch);  // persiste posa
    void updateGalleryLabel(const QString& key, int heading);
    void updateReconHint();          // ricontrolla COLMAP a caldo (exe + librerie)
    static int requiredPhotosFor(const QString& quality);  // foto minime per qualità
    int  countSessionPhotos(const QString& session) const;
    void updatePhotoRequirement();   // contatore "X/Y foto" accanto alla qualità
    void refreshSessionDescriptionTooltip(const QString& session);  // combo → tooltip descrizione

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
    QLabel*         m_lastDesc   = nullptr;
    QLabel*         m_lastSensorInfo = nullptr;  // modalità/quota GPS/accelerometro dello scatto in galleria
    QLabel*         m_lastThumb  = nullptr;
    QLabel*         m_lastDepth  = nullptr;
    QLabel*         m_lastBoxes  = nullptr;
    QLabel*         m_lastEdges  = nullptr;   // bordi (Canny)
    QLabel*         m_lastBump   = nullptr;   // bump map
    QLabel*         m_lastNormal = nullptr;   // normal map
    QLineEdit*      m_portEdit   = nullptr;
    QComboBox*      m_ifaceCombo = nullptr;
    QComboBox*      m_vlmCombo   = nullptr;
    QListWidget*    m_gallery    = nullptr;   // tutti gli scatti, clic = rivedi
    QTableWidget*   m_deviceTable = nullptr;
    QNetworkReply*  m_tagsReply  = nullptr;   // fetch /api/tags in corso
    QrCodeWidget*   m_prepQr     = nullptr;   // tab Preparazione: QR di collegamento, aggiornato da start()/stop()

    // scena 3D + ricostruzione
    Vision3DSceneCanvas* m_scene = nullptr;
    QSpinBox*       m_poseHeadSpin  = nullptr;  // posa manuale: angolo bussola
    QSpinBox*       m_posePitchSpin = nullptr;  // posa manuale: inclinazione
    QPushButton*    m_distribBtn    = nullptr;  // distribuisci scatti sul cerchio
    QString         m_selectedShotKey;          // basePath dello scatto selezionato
    QComboBox*      m_sessionCombo = nullptr;
    QComboBox*      m_prepSessionCombo = nullptr;   // stesso elenco sessioni, selettore in tab Preparazione
    QComboBox*      m_qualityCombo = nullptr;
    QPushButton*    m_reconBtn   = nullptr;
    QLabel*         m_reconHint  = nullptr;   // stato COLMAP, aggiornabile a caldo
    QLabel*         m_photoReqLabel = nullptr; // contatore foto vs requisito qualità
    QProcess*       m_reconProc  = nullptr;
    int             m_reconStep  = 0;         // 0=reconstructor 1=export PLY
    QString         m_reconSessionDir;

    bool m_running = false;
};
