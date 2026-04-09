#pragma once
#include <QAbstractButton>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QMouseEvent>

/* ══════════════════════════════════════════════════════════════
   ToggleSwitch — interruttore stile "modalità aereo" (mobile)

   Aspetto: pillola colorata con cerchio scorrevole.
   Animazione: 150ms ease-in-out tra stato ON e OFF.

   Uso:
       auto* t = new ToggleSwitch("Auto-Correggi AI", this);
       connect(t, &ToggleSwitch::toggled, this, [](bool on){ ... });
       t->setChecked(true);
   ══════════════════════════════════════════════════════════════ */
class ToggleSwitch : public QAbstractButton {
    Q_OBJECT
    Q_PROPERTY(int thumbPos READ thumbPos WRITE setThumbPos)

public:
    explicit ToggleSwitch(const QString& label = {}, QWidget* parent = nullptr)
        : QAbstractButton(parent), m_label(label)
    {
        setCheckable(true);
        setFixedHeight(26);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

        m_anim = new QPropertyAnimation(this, "thumbPos", this);
        m_anim->setDuration(150);
        m_anim->setEasingCurve(QEasingCurve::InOutQuad);

        m_thumbPos = 0;

        connect(this, &QAbstractButton::toggled, this, [this](bool on) {
            m_anim->stop();
            m_anim->setStartValue(m_thumbPos);
            m_anim->setEndValue(on ? kTrackW - kThumbD - kPad : kPad);
            m_anim->start();
        });
    }

    QSize sizeHint() const override {
        int tw = kTrackW + (m_label.isEmpty() ? 0 : 8 + fontMetrics().horizontalAdvance(m_label));
        return { tw, 26 };
    }

    int  thumbPos() const { return m_thumbPos; }
    void setThumbPos(int v) { m_thumbPos = v; update(); }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        /* ── Track (pillola) ── */
        const bool on = isChecked();
        QColor trackColor = on
            ? QColor("#00b8d9")   /* accento cyan — coerente col tema principale */
            : QColor("#4b5563");  /* grigio neutro per OFF */

        QPainterPath track;
        QRectF tr(0, (height() - kTrackH) / 2.0, kTrackW, kTrackH);
        track.addRoundedRect(tr, kTrackH / 2.0, kTrackH / 2.0);
        p.fillPath(track, trackColor);

        /* ── Thumb (cerchio bianco scorrevole) ── */
        int ty = (height() - kThumbD) / 2;
        QRectF thumb(m_thumbPos, ty, kThumbD, kThumbD);
        p.setBrush(Qt::white);
        p.setPen(Qt::NoPen);
        p.drawEllipse(thumb);

        /* ── Label ── */
        if (!m_label.isEmpty()) {
            p.setPen(palette().windowText().color());
            p.drawText(kTrackW + 8, 0, width() - kTrackW - 8, height(),
                       Qt::AlignVCenter | Qt::AlignLeft, m_label);
        }
    }

    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton) toggle();
    }

private:
    static constexpr int kTrackW  = 44;
    static constexpr int kTrackH  = 22;
    static constexpr int kThumbD  = 18;
    static constexpr int kPad     = 2;

    QString             m_label;
    int                 m_thumbPos = kPad;
    QPropertyAnimation* m_anim    = nullptr;
};
