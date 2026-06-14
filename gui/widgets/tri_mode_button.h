#pragma once
/* ══════════════════════════════════════════════════════════════
   TriModeButton — cerchio diviso in 3 settori 120° selezionabili
   Settori (CW dall'alto):
     0 = Chat      💬  (top)
     1 = Agentico  👔  (bottom-right)
     2 = Conversa  🎙  (bottom-left)
   ══════════════════════════════════════════════════════════════ */
#include <QWidget>
#include <QPainter>
#include <QMouseEvent>
#include <QRectF>
#include <QtMath>
#include "../dpi_utils.h"

class TriModeButton : public QWidget
{
    Q_OBJECT
public:
    enum Mode { Chat = 0, Agentico = 1, Conversa = 2 };

    explicit TriModeButton(QWidget* parent = nullptr)
        : QWidget(parent), m_mode(Chat), m_hovered(-1)
    {
        setMouseTracking(true);
        setCursor(Qt::PointingHandCursor);
        setToolTip(
            "Seleziona la modalit\xc3\xa0 operativa:\n"
            "\xf0\x9f\x92\xac  Chat \xe2\x80\x94 risposta diretta\n"
            "\xf0\x9f\x91\x94  Agentico \xe2\x80\x94 pianifica e itera (ReAct)\n"
            "\xf0\x9f\x8e\x99  Conversazione \xe2\x80\x94 loop voce continuo");
    }

    Mode currentMode()  const { return m_mode; }

    void setMode(Mode m, bool emitSignal = false)
    {
        if (m_mode == m) return;
        m_mode = m;
        update();
        if (emitSignal) emit modeChanged((int)m);
    }

    QSize sizeHint()        const override { return { dpiScale(138), dpiScale(148) }; }
    QSize minimumSizeHint() const override { return { dpiScale(118), dpiScale(126) }; }

signals:
    void modeChanged(int mode);   /* 0=Chat, 1=Agentico, 2=Conversa */

protected:
    /* ── Disegno ─────────────────────────────────────────────── */
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

        const int w = width(), h = height();
        const double R  = qMin(w, h) / 2.0 - 1.5;
        const QPointF c(w / 2.0, h / 2.0);
        const QRectF  rc(c.x() - R, c.y() - R, R * 2.0, R * 2.0);

        /* Angoli di partenza settori (Qt: gradi CCW da ore 3, in 1/16°).
           span = -120*16 (senso orario) per ciascun settore.
           Settore 0 (Chat, top):      start=150°
           Settore 1 (Agentico, 4h):   start=30°
           Settore 2 (Conversa, 8h):   start=-90° (=270°) */
        static const int kStart[3] = { 150 * 16, 30 * 16, -90 * 16 };
        static const int kSpan     = -120 * 16;

        /* Colori */
        static const QColor kBg   (30, 41, 59);        /* sfondo settore normale  */
        static const QColor kHov  (45, 58, 82);        /* hover */
        static const QColor kAct[3] = {
            { 99, 102, 241 },   /* indigo  — Chat      */
            { 16, 185, 129 },   /* emerald — Agentico  */
            {239,  68,  68 },   /* red     — Conversa  */
        };

        /* 1) Settori */
        for (int i = 0; i < 3; ++i) {
            QColor fill = (m_mode == i) ? kAct[i]
                        : (m_hovered == i) ? kHov
                        : kBg;
            p.setBrush(fill);
            p.setPen(Qt::NoPen);
            p.drawPie(rc, kStart[i], kSpan);
        }

        /* 2) Linee divisorie tra settori (agli angoli 30°, 150°, 270° CCW da ore 3) */
        const double kDivRad[3] = {
            30.0  * M_PI / 180.0,
            150.0 * M_PI / 180.0,
            270.0 * M_PI / 180.0
        };
        p.setPen(QPen(QColor(15, 23, 42), dpiScale(2)));
        for (double a : kDivRad) {
            QPointF end(c.x() + R * cos(a), c.y() - R * sin(a));
            p.drawLine(c, end);
        }

        /* 3) Bordo esterno */
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(71, 85, 105), 1.5));
        p.drawEllipse(c, R - 0.75, R - 0.75);

        /* 4) Hub centrale con nome modalità corrente */
        const double innerR = dpiScale(27);
        p.setBrush(QColor(15, 23, 42));
        p.setPen(QPen(kAct[m_mode], 1.5));
        p.drawEllipse(c, innerR, innerR);

        {   /* Testo nome modalità nel hub */
            static const char* kNames[] = { "Chat", "Agen\xc2\xadtico", "Conversa" };
            QFont hf = font();
            hf.setPixelSize(dpiScale(12));
            hf.setBold(true);
            p.setFont(hf);
            p.setPen(kAct[m_mode]);
            p.drawText(QRectF(c.x() - innerR, c.y() - innerR, innerR * 2, innerR * 2),
                       Qt::AlignCenter, QString::fromUtf8(kNames[m_mode]));
        }

        /* 5) Emoji + etichetta in ciascun settore.
              Angoli centrali (CCW da ore 3): 90° (top), -30° (4h), -150° (8h) */
        static const double kCA[3] = { 90.0, -30.0, -150.0 };
        static const char*  kEmoji[3] = {
            "\xf0\x9f\x92\xac",   /* 💬 */
            "\xf0\x9f\x91\x94",   /* 👔 */
            "\xf0\x9f\x8e\x99",   /* 🎙 */
        };

        const int    eSize  = dpiScale(22);
        /* Centro emoji: punto medio tra hub e bordo esterno → margine uguale da entrambi i lati */
        const double textR  = (innerR + R) / 2.0;

        QFont ef = font();
        ef.setPixelSize(eSize);
        p.setFont(ef);
        p.setPen(Qt::white);

        for (int i = 0; i < 3; ++i) {
            const double rad = kCA[i] * M_PI / 180.0;
            QPointF tc(c.x() + textR * cos(rad), c.y() - textR * sin(rad));
            const QRectF er(tc.x() - eSize / 2.0, tc.y() - eSize / 2.0, eSize, eSize);
            p.drawText(er, Qt::AlignCenter, QString::fromUtf8(kEmoji[i]));
        }
    }

    /* ── Input ─────────────────────────────────────────────── */
    void mousePressEvent(QMouseEvent* e) override
    {
        if (e->button() != Qt::LeftButton) return;
        int s = sectorAt(e->pos());
        if (s < 0) return;
        if (s == (int)m_mode) return;   /* click sul settore già attivo: no-op */
        m_mode = (Mode)s;
        update();
        emit modeChanged(s);
    }

    void mouseMoveEvent(QMouseEvent* e) override
    {
        int s = sectorAt(e->pos());
        if (s != m_hovered) { m_hovered = s; update(); }
    }

    void leaveEvent(QEvent*) override
    {
        m_hovered = -1;
        update();
    }

private:
    Mode m_mode;
    int  m_hovered;

    /* Restituisce 0/1/2 se il click cade in un settore, -1 altrimenti. */
    int sectorAt(QPoint pt) const
    {
        const double cx = width()  / 2.0;
        const double cy = height() / 2.0;
        const double dx = pt.x() - cx;
        const double dy = cy - pt.y();   /* y invertita (coord. matematiche) */
        const double R  = qMin(width(), height()) / 2.0 - 1.5;
        const double ir = dpiScale(27);
        const double d2 = dx * dx + dy * dy;
        if (d2 > R * R || d2 < ir * ir) return -1;  /* fuori cerchio o nel hub (22px) */

        /* Angolo CCW da ore 3, range [0°, 360°) */
        double angle = atan2(dy, dx) * 180.0 / M_PI;
        if (angle < 0) angle += 360.0;

        /* Converti in "CW dall'alto" (0° = ore 12) */
        double fromTop = fmod(90.0 - angle + 360.0, 360.0);

        /* Confini settori: 60°, 180°, 300° CW dall'alto */
        if (fromTop < 60.0 || fromTop >= 300.0) return 0;  /* Chat (top) */
        if (fromTop < 180.0)                     return 1;  /* Agentico (bott-dx) */
        return 2;                                            /* Conversa (bott-sx) */
    }
};
