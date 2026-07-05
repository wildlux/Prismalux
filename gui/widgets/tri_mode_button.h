#pragma once
#ifdef HAVE_SVG
#  include <QSvgRenderer>
#endif
#include <QSettings>
/* ══════════════════════════════════════════════════════════════
   TriModeButton — ovale 3 settori selezionabili + hub azione
   Settori (ellisse esterna):
     0 = Chat     💬  top
     1 = Agentico 👔  lower-right
     2 = Conversa 🎙  lower-left
   Hub centrale ovale = pulsante azione (Invia / Dialoga / Avvia Agente)
   ══════════════════════════════════════════════════════════════ */
#include <QWidget>
#include <QPainter>
#include <QPainterPath>
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
        : QWidget(parent), m_mode(Chat), m_hovered(-1),
          m_actionText("\xf0\x9f\x93\xa4 Invia"),
          m_actionEnabled(true), m_actionDanger(false)
    {
        setMouseTracking(true);
        setCursor(Qt::PointingHandCursor);
        setToolTip(
            "Clicca al centro per eseguire l'azione corrente\n"
            "\xf0\x9f\x92\xac  Chat \xe2\x80\x94 risposta diretta\n"
            "\xf0\x9f\x91\x94  Agentico \xe2\x80\x94 pianifica e itera (ReAct)\n"
            "\xf0\x9f\x8e\x99  Conversazione \xe2\x80\x94 loop voce continuo");
    }

    Mode    currentMode()    const { return m_mode; }
    QString actionText()     const { return m_actionText; }
    bool    actionEnabled()  const { return m_actionEnabled; }

    void setMode(Mode m, bool emitSignal = false) {
        if (m_mode == m) return;
        m_mode = m;
        update();
        if (emitSignal) emit modeChanged((int)m);
    }

    void setActionText(const QString& t) {
        if (m_actionText == t) return;
        m_actionText = t;
        update();
    }

    void setActionEnabled(bool en) {
        if (m_actionEnabled == en) return;
        m_actionEnabled = en;
        update();
    }

    void setActionDanger(bool danger) {
        if (m_actionDanger == danger) return;
        m_actionDanger = danger;
        update();
    }

    QSize sizeHint()        const override { return { dpiScale(170), dpiScale(120) }; }
    QSize minimumSizeHint() const override { return { dpiScale(140), dpiScale(100) }; }

    /* Aggiorna la forma (chiamare dopo aver cambiato la setting) */
    void refreshShape() { updateGeometry(); update(); }

signals:
    void modeChanged(int mode);   /* 0=Chat, 1=Agentico, 2=Conversa */
    void actionClicked();         /* click hub centrale */

protected:
    bool isRect() const {
        return QSettings("Prismalux","GUI").value("ui/triModeShape","oval").toString() == "rect";
    }

    void paintEvent(QPaintEvent*) override
    {
        if (isRect()) { paintRect(); return; }
        QPainter p(this);
        p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

        const int    w = width(), h = height();
        const QPointF c(w / 2.0, h / 2.0);
        const double  A = w / 2.0 - 1.5;   /* semi-asse orizzontale */
        const double  B = h / 2.0 - 1.5;   /* semi-asse verticale   */
        const QRectF  rc(c.x()-A, c.y()-B, A*2, B*2);

        /* drawPie su QRectF non quadrato → archi ellittici automatici */
        static const int kStart[3] = { 150*16, 30*16, -90*16 };
        static const int kSpan     = -120 * 16;

        /* Colori dal palette del tema attivo */
        const QColor kBg  = palette().color(QPalette::Active, QPalette::Window);
        const QColor kHov = palette().color(QPalette::Active, QPalette::Button);
        /* Tre accenti: hue del Highlight + rotazioni 120° */
        const QColor hi   = palette().color(QPalette::Active, QPalette::Highlight);
        const int    hh   = hi.hsvHue() < 0 ? 220 : hi.hsvHue();
        const int    hs   = qMax(hi.hsvSaturation(), 160);
        const int    hv   = qMax(hi.value(), 180);
        const QColor kAct[3] = {
            QColor::fromHsv(hh,               hs, hv),   /* Chat      */
            QColor::fromHsv((hh + 120) % 360, hs, hv),   /* Agentico  */
            QColor::fromHsv((hh + 240) % 360, hs, hv),   /* Conversa  */
        };

        /* 1) Settori sull'ellisse */
        for (int i = 0; i < 3; ++i) {
            QColor fill = (m_mode == i) ? kAct[i]
                        : (m_hovered == i) ? kHov
                        : kBg;
            p.setBrush(fill);
            p.setPen(Qt::NoPen);
            p.drawPie(rc, kStart[i], kSpan);
        }

        /* Colori neutri dal palette — Mid invece di Shadow per non "annerire" il widget */
        const QColor divCol    = palette().color(QPalette::Active, QPalette::Mid);
        const QColor borderCol = palette().color(QPalette::Active, QPalette::Mid);
        const QColor hubBaseBg = palette().color(QPalette::Active, QPalette::Button);

        /* 2) Linee divisorie → bordo ellisse (angoli parametrici 30°,150°,270°) */
        static const double kDivDeg[3] = { 30.0, 150.0, 270.0 };
        p.setPen(QPen(divCol, 1.0));
        for (double deg : kDivDeg) {
            const double rad = deg * M_PI / 180.0;
            p.drawLine(c, QPointF(c.x() + A * qCos(rad), c.y() - B * qSin(rad)));
        }

        /* 3) Bordo esterno */
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(borderCol, 1.5));
        p.drawEllipse(rc);

        /* 4) Hub ovale centrale — pulsante azione */
        const double iA = dpiScale(52);   /* semi-asse hub orizzontale */
        const double iB = dpiScale(25);   /* semi-asse hub verticale   */
        const QRectF hubRc(c.x()-iA, c.y()-iB, iA*2, iB*2);

        /* Danger (Stop): hub pieno rosso + testo bianco — deve saltare
           all'occhio che c'è uno stream da interrompere */
        const QColor hubBorder = m_actionDanger
            ? QColor(185, 28, 28)
            : kAct[m_mode];
        const QColor hubBg = m_actionDanger
            ? ((m_hovered == -2 && m_actionEnabled) ? QColor(153, 27, 27)
                                                    : QColor(220, 38, 38))
            : ((m_hovered == -2 && m_actionEnabled) ? hubBorder.darker(160)
                                                    : hubBaseBg);
        p.setBrush(hubBg);
        p.setPen(QPen(hubBorder, m_actionEnabled ? 2.0 : 1.0));
        p.drawEllipse(hubRc);

        /* Testo azione: rimuovi emoji iniziale per leggibilità */
        {
            QString label = m_actionText;
            int i = 0;
            while (i < label.length() && !label.at(i).isLetter()) i++;
            if (i > 0 && i < label.length()) label = label.mid(i).trimmed();

            QFont hf = font();
            hf.setPixelSize(dpiScale(11));
            hf.setBold(true);
            p.setFont(hf);
            p.setPen(m_actionEnabled
                ? ((m_hovered == -2 || m_actionDanger) ? QColor(Qt::white)
                                                       : hubBorder)
                : QColor(71, 85, 105));
            p.drawText(hubRc, Qt::AlignCenter | Qt::TextWordWrap, label);
        }

        /* 5) Emoji nei settori — punto medio tra hub e bordo ellisse */
        static const double kCA[3] = { 90.0, -30.0, -150.0 };
        static const char*  kEmoji[3] = {
            "\xf0\x9f\x92\xac",   /* 💬  Chat      */
            "\xf0\x9f\x91\x94",   /* 👔  Agentico  */
            "\xf0\x9f\x8e\x99",   /* 🎙  Conversa  */
        };
        static const char* kSvgPath[3] = {
            ":/emoji/1F4AC.svg",
            ":/emoji/1F454.svg",
            ":/emoji/1F399.svg",
        };

        /* Legge lo stile emoji dalla impostazione (QSettings cache interna) */
        const bool useOpenMoji =
            QSettings("Prismalux", "GUI")
            .value("ui/triModeEmojiStyle", "system").toString() == "openmoji";

        const int    eSize = dpiScale(20);
        const double tA    = (iA + A) / 2.0;
        const double tB    = (iB + B) / 2.0;

        for (int i = 0; i < 3; ++i) {
            const double rad = kCA[i] * M_PI / 180.0;
            const QPointF tc(c.x() + tA * qCos(rad), c.y() - tB * qSin(rad));
            const QRectF  emojiRc(tc.x() - eSize, tc.y() - eSize / 2.0, eSize * 2, eSize);

#ifdef HAVE_SVG
            if (useOpenMoji) {
                /* Renderer SVG — inizializzato una sola volta per path */
                static QSvgRenderer* rend[3] = {nullptr, nullptr, nullptr};
                if (!rend[i]) rend[i] = new QSvgRenderer(QString::fromLatin1(kSvgPath[i]));
                if (rend[i]->isValid()) {
                    rend[i]->render(&p, emojiRc);
                    continue;
                }
            }
#else
            Q_UNUSED(useOpenMoji)
            Q_UNUSED(kSvgPath)
#endif
            /* Fallback: testo emoji con bordo nero per leggibilità */
            const QColor textCol = (m_mode == i)
                ? QColor(Qt::white)
                : palette().color(QPalette::Active, QPalette::WindowText);

            QFont ef = font();
            ef.setPixelSize(eSize);
            p.setFont(ef);

            const QString emojiStr = QString::fromUtf8(kEmoji[i]);
            p.setPen(QColor(0, 0, 0, 160));
            for (int dx = -1; dx <= 1; ++dx)
                for (int dy = -1; dy <= 1; ++dy)
                    if (dx || dy)
                        p.drawText(emojiRc.translated(dx, dy), Qt::AlignCenter, emojiStr);
            p.setPen(textCol);
            p.drawText(emojiRc, Qt::AlignCenter, emojiStr);
        }
    }

    void mousePressEvent(QMouseEvent* e) override
    {
        if (e->button() != Qt::LeftButton) return;
        const int z = zoneAt(e->pos());
        if (z == -1) return;
        if (z == -2) { if (m_actionEnabled) emit actionClicked(); return; }
        if (z == (int)m_mode) return;
        m_mode = (Mode)z;
        update();
        emit modeChanged(z);
    }

    void mouseMoveEvent(QMouseEvent* e) override
    {
        int z = zoneAt(e->pos());
        if (z != m_hovered) { m_hovered = z; update(); }
    }

    void leaveEvent(QEvent*) override { m_hovered = -1; update(); }

private:
    Mode    m_mode;
    int     m_hovered;        /* -1=fuori, -2=hub, 0-2=settori */
    QString m_actionText;
    bool    m_actionEnabled;
    bool    m_actionDanger;

    /* ── Disegno forma rettangolare ───────────────────────────────── */
    void paintRect()
    {
        QPainter p(this);
        p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

        const int w = width(), h = height();
        const double r = 10.0;

        /* Palette colori identica alla forma ovale */
        const QColor kBg  = palette().color(QPalette::Active, QPalette::Window);
        const QColor kHov = palette().color(QPalette::Active, QPalette::Button);
        const QColor hi   = palette().color(QPalette::Active, QPalette::Highlight);
        const int    hh   = hi.hsvHue() < 0 ? 220 : hi.hsvHue();
        const int    hs   = qMax(hi.hsvSaturation(), 160);
        const int    hv   = qMax(hi.value(), 180);
        const QColor kAct[3] = {
            QColor::fromHsv(hh,               hs, hv),
            QColor::fromHsv((hh + 120) % 360, hs, hv),
            QColor::fromHsv((hh + 240) % 360, hs, hv),
        };
        const QColor divCol    = palette().color(QPalette::Active, QPalette::Mid);
        const QColor borderCol = palette().color(QPalette::Active, QPalette::Mid);

        /* Clip su rettangolo arrotondato (stesso guscio dell'ovale ma rettangolare) */
        QPainterPath clip;
        clip.addRoundedRect(QRectF(0, 0, w, h), r, r);
        p.setClipPath(clip);

        /* ── 3 settori: top=Chat, basso-sx=Conversa, basso-dx=Agentico ── */
        /* Come l'ovale ma con bordi retti anziché curvi:
           - zona 0 (Chat):    rettangolo top, tutta la larghezza, metà altezza
           - zona 2 (Conversa):rettangolo basso-sinistra
           - zona 1 (Agentico):rettangolo basso-destra                          */
        const int halfH = h / 2;
        const int halfW = w / 2;

        /* Settore top: Chat */
        {
            QColor fill = (m_mode == 0) ? kAct[0] : (m_hovered == 0) ? kHov : kBg;
            p.setBrush(fill); p.setPen(Qt::NoPen);
            p.drawRect(0, 0, w, halfH);
        }
        /* Settore basso-sx: Conversa (modo 2) */
        {
            QColor fill = (m_mode == 2) ? kAct[2] : (m_hovered == 2) ? kHov : kBg;
            p.setBrush(fill); p.setPen(Qt::NoPen);
            p.drawRect(0, halfH, halfW, h - halfH);
        }
        /* Settore basso-dx: Agentico (modo 1) */
        {
            QColor fill = (m_mode == 1) ? kAct[1] : (m_hovered == 1) ? kHov : kBg;
            p.setBrush(fill); p.setPen(Qt::NoPen);
            p.drawRect(halfW, halfH, w - halfW, h - halfH);
        }

        /* ── Linee divisorie ── */
        p.setClipping(false);
        p.setPen(QPen(divCol, 1.0));
        p.drawLine(1,     halfH, w - 1, halfH);    /* orizzontale centrale */
        p.drawLine(halfW, halfH, halfW, h - 1);    /* verticale bassa      */

        /* ── Hub ovale centrale (identico all'originale) ── */
        const double iA = dpiScale(52);
        const double iB = dpiScale(25);
        const QPointF c(w / 2.0, h / 2.0);
        const QRectF hubRc(c.x()-iA, c.y()-iB, iA*2, iB*2);

        /* Danger (Stop): hub pieno rosso + testo bianco (come variante ovale) */
        const QColor hubBorder = m_actionDanger
            ? QColor(185, 28, 28)
            : kAct[m_mode];
        const QColor hubBaseBg = palette().color(QPalette::Active, QPalette::Button);
        const QColor hubBg = m_actionDanger
            ? ((m_hovered == -2 && m_actionEnabled) ? QColor(153, 27, 27)
                                                    : QColor(220, 38, 38))
            : ((m_hovered == -2 && m_actionEnabled) ? hubBorder.darker(160)
                                                    : hubBaseBg);
        p.setBrush(hubBg);
        p.setPen(QPen(hubBorder, m_actionEnabled ? 2.0 : 1.0));
        p.drawEllipse(hubRc);

        /* Testo hub */
        {
            QString label = m_actionText;
            int i2 = 0;
            while (i2 < label.length() && !label.at(i2).isLetter()) i2++;
            if (i2 > 0 && i2 < label.length()) label = label.mid(i2).trimmed();
            QFont hf = font(); hf.setPixelSize(dpiScale(11)); hf.setBold(true);
            p.setFont(hf);
            p.setPen(m_actionEnabled
                ? ((m_hovered == -2 || m_actionDanger) ? QColor(Qt::white)
                                                       : hubBorder)
                : QColor(71, 85, 105));
            p.drawText(hubRc, Qt::AlignCenter | Qt::TextWordWrap, label);
        }

        /* ── Emoji nei settori: stesse coordinate dell'ovale (angolo parametrico) ── */
        static const char* kEmoji[3] = {
            "\xf0\x9f\x92\xac", "\xf0\x9f\x91\x94", "\xf0\x9f\x8e\x99"
        };
        static const double kCA[3] = { 90.0, -30.0, -150.0 };
        /* Punto medio tra bordo hub e bordo rettangolo — stesso calcolo dell'ovale */
        const double A  = w / 2.0 - 2.0;
        const double B  = h / 2.0 - 2.0;
        const double tA = (iA + A) / 2.0;
        const double tB = (iB + B) / 2.0;
        const int eSize = dpiScale(20);
        for (int i = 0; i < 3; ++i) {
            const double rad = kCA[i] * M_PI / 180.0;
            const double extraY = (i == 0) ? 0.0 : dpiScale(14);
            const QPointF tc(c.x() + tA * qCos(rad), c.y() - tB * qSin(rad) + extraY);
            const QRectF er(tc.x() - eSize, tc.y() - eSize / 2.0, eSize * 2, eSize);
            const QColor tx = (m_mode == i) ? QColor(Qt::white)
                : palette().color(QPalette::Active, QPalette::WindowText);
            const QString es = QString::fromUtf8(kEmoji[i]);
            QFont ef = font(); ef.setPixelSize(eSize); p.setFont(ef);
            p.setPen(QColor(0,0,0,140));
            for (int dx=-1;dx<=1;++dx) for (int dy=-1;dy<=1;++dy)
                if (dx||dy) p.drawText(er.translated(dx,dy), Qt::AlignCenter, es);
            p.setPen(tx);
            p.drawText(er, Qt::AlignCenter, es);
        }

        /* ── Bordo esterno rettangolare ── */
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(borderCol, 1.5));
        p.drawRoundedRect(QRectF(0.75, 0.75, w-1.5, h-1.5), r, r);
    }

    /* -1=fuori widget  -2=hub  0/1/2=settore */
    int zoneAt(QPoint pt) const
    {
        if (isRect()) {
            if (pt.x() < 0 || pt.x() >= width() || pt.y() < 0 || pt.y() >= height())
                return -1;
            /* Hub ovale al centro */
            const double cx = width() / 2.0, cy = height() / 2.0;
            const double dx = pt.x() - cx, dy = pt.y() - cy;
            const double iA = dpiScale(52), iB = dpiScale(25);
            if (dx*dx/(iA*iA) + dy*dy/(iB*iB) < 1.0) return -2;
            /* Settori: top=Chat(0), basso-dx=Agentico(1), basso-sx=Conversa(2) */
            if (pt.y() < height() / 2) return 0;
            return (pt.x() >= width() / 2) ? 1 : 2;
        }

        const double cx = width()  / 2.0,  cy = height() / 2.0;
        const double dx = pt.x() - cx,     dy = cy - pt.y();
        const double A  = width()  / 2.0 - 1.5;
        const double B  = height() / 2.0 - 1.5;
        const double iA = dpiScale(52),     iB = dpiScale(23);

        if (dx*dx/(A*A)   + dy*dy/(B*B)   > 1.0) return -1;   /* fuori */
        if (dx*dx/(iA*iA) + dy*dy/(iB*iB) < 1.0) return -2;   /* hub   */

        /* Angolo parametrico normalizzato → stesso settore di drawPie */
        double angle = qAtan2(dy/B, dx/A) * 180.0 / M_PI;
        if (angle < 0) angle += 360.0;
        const double fromTop = fmod(90.0 - angle + 360.0, 360.0);

        if (fromTop < 60.0 || fromTop >= 300.0) return 0;
        if (fromTop < 180.0) return 1;
        return 2;
    }
};
