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
   MisurePage -- Calcolatrice stanza + vista 3D + AI.

   Tre sezioni:
   1. Calcolatore manuale: lunghezza × larghezza × altezza
      → area pavimento, pareti, volume, vernice, piastrelle.
   2. Vista 3D isometrica: aggiornata automaticamente al calcolo.
   3. AI Analisi: invio testo descrittivo all'AI per stime.
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

private:
    void calcola();

    AiClient* m_ai = nullptr;

    /* Calcolatore */
    QDoubleSpinBox* m_lungSpin   = nullptr;
    QDoubleSpinBox* m_largSpin   = nullptr;
    QDoubleSpinBox* m_altSpin    = nullptr;
    QDoubleSpinBox* m_sovrasSpin = nullptr;
    QLabel*         m_resultLbl  = nullptr;

    /* Vista 3D */
    RoomVisualWidget* m_roomView = nullptr;

    /* Planimetria interattiva */
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
};
