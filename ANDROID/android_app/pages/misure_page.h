#pragma once
#include <QWidget>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QTextEdit>
#include <QProgressBar>
#include <QComboBox>
#include <QMouseEvent>
#include <QVector>
#include <QPointF>
#include <QRectF>
#include <QListWidget>
#include <QListWidgetItem>
#include <QSlider>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QPixmap>
#include "../ai_client.h"

/* --------------------------------------------------------------
   RoomVisualWidget -- vista 3D isometrica della stanza.

   Disegna con QPainter una proiezione isometrica della stanza
   (pavimento, parete destra, parete posteriore) con etichette
   delle aree di ogni superficie. Si aggiorna tramite setDimensions().
   -------------------------------------------------------------- */
class RoomVisualWidget : public QWidget {
    Q_OBJECT
public:
    explicit RoomVisualWidget(QWidget* parent = nullptr);
    void setDimensions(double L, double W, double H);
    QSize sizeHint() const override { return QSize(300, 220); }
    QSize minimumSizeHint() const override { return QSize(200, 160); }

protected:
    void paintEvent(QPaintEvent* e) override;

private:
    double m_L = 5.0;  ///< lunghezza (profondità, direzione Y isometrica)
    double m_W = 4.0;  ///< larghezza (direzione X isometrica)
    double m_H = 2.7;  ///< altezza
};

/* --------------------------------------------------------------
   RoomPlanWidget — canvas 2D touch-interattivo per planimetria.

   L'utente tocca i punti per definire il perimetro della stanza.
   La scala è configurabile (m per quadretto della griglia).
   L'area del poligono è calcolata con la formula di Shoelace.
   -------------------------------------------------------------- */
class RoomPlanWidget : public QWidget {
    Q_OBJECT
public:
    explicit RoomPlanWidget(QWidget* parent = nullptr);
    void   setScaleMetersPerCell(double m);
    void   reset();
    QString calcResults() const;
    int    pointCount() const { return m_points.size(); }
    QSize  sizeHint()        const override { return QSize(300, 300); }
    QSize  minimumSizeHint() const override { return QSize(240, 240); }

signals:
    void planChanged(const QString& results);

protected:
    void paintEvent(QPaintEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;

private:
    double  areaM2()       const;
    double  perimeterM()   const;
    QPointF pixToM(QPointF px) const;

    QVector<QPointF> m_points;
    double m_metersPerCell = 0.5;
    int    m_gridPx        = 36;   /* pixel per quadretto */
};

/* --------------------------------------------------------------
   PiantinaWidget -- canvas 2D drag-to-draw stanze.

   L'utente clicca e trascina per disegnare stanze come rettangoli.
   Ogni rettangolo viene etichettato con le misure in metri.
   Scala configurabile tramite slider (pixel per metro).
   -------------------------------------------------------------- */
class PiantinaWidget : public QWidget {
    Q_OBJECT
public:
    explicit PiantinaWidget(QWidget* parent = nullptr);

    void   setPixelsPerMeter(int ppm);
    void   clearAll();
    void   removeRoom(int idx);
    bool   exportPng(const QString& path);
    int    roomCount() const { return m_rooms.size(); }
    QString roomInfo(int idx) const;
    QSize  sizeHint()        const override { return QSize(400, 350); }
    QSize  minimumSizeHint() const override { return QSize(280, 250); }

signals:
    void roomsChanged();

protected:
    void paintEvent(QPaintEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;

private:
    QRectF  normalizedRect(QPointF a, QPointF b) const;
    QString mLabel(const QRectF& r) const;

    QVector<QRectF> m_rooms;          /* coordinate in pixel */
    QPointF         m_dragStart;
    QPointF         m_dragCurrent;
    bool            m_dragging = false;
    int             m_ppm      = 50;  /* pixel per metro, default 50px = 1m */
};

/* --------------------------------------------------------------
   MisurePage -- Calcolatrice stanza + vista 3D + AI + piantina.

   Quattro sezioni:
   1. Calcolatore manuale: lunghezza × larghezza × altezza
      → area pavimento, pareti, volume, vernice, piastrelle.
   2. Vista 3D isometrica: aggiornata automaticamente al calcolo.
   3. AI Analisi: invio testo descrittivo all'AI per stime.
   4. Piantina: drag-to-draw rettangoli con misure e export PNG.
   -------------------------------------------------------------- */
class MisurePage : public QWidget {
    Q_OBJECT
public:
    explicit MisurePage(AiClient* ai, QWidget* parent = nullptr);

private slots:
    void onCalcolaClicked();
    void onAiClicked();
    void onToken(const QString& t);
    void onFinished(const QString& f);
    void onError(const QString& e);
    void onAborted();
    void onPlanChanged(const QString& results);
    void onPlanResetClicked();
    void onPlanScaleChanged(double v);

    /* Piantina drag-to-draw */
    void onPiantinaClear();
    void onPiantinaExport();
    void onPiantinaZoomChanged(int value);
    void onPiantinaRoomsChanged();
    void onPiantinaDeleteRoom();

private:
    void calcola();
    void refreshRoomList();

    AiClient* m_ai = nullptr;

    /* Calcolatore */
    QDoubleSpinBox* m_lungSpin   = nullptr;
    QDoubleSpinBox* m_largSpin   = nullptr;
    QDoubleSpinBox* m_altSpin    = nullptr;
    QDoubleSpinBox* m_sovrasSpin = nullptr;
    QLabel*         m_resultLbl  = nullptr;

    /* Vista 3D */
    RoomVisualWidget* m_roomView = nullptr;

    /* Planimetria interattiva (punti) */
    RoomPlanWidget* m_planWidget   = nullptr;
    QLabel*         m_planResults  = nullptr;
    QDoubleSpinBox* m_planScaleSpin = nullptr;

    /* AI */
    QTextEdit*    m_aiInput   = nullptr;
    QComboBox*    m_aiAction  = nullptr;
    QPushButton*  m_btnAi     = nullptr;
    QPushButton*  m_btnStop   = nullptr;
    QLabel*       m_statusLbl = nullptr;
    QProgressBar* m_progress  = nullptr;
    QTextEdit*    m_aiOutput  = nullptr;
    bool          m_busy      = false;

    /* Piantina drag-to-draw */
    PiantinaWidget* m_piantinaWidget  = nullptr;
    QListWidget*    m_roomList        = nullptr;
    QSlider*        m_zoomSlider      = nullptr;
    QLabel*         m_zoomLabel       = nullptr;
    QPushButton*    m_piantinaExport  = nullptr;
};
