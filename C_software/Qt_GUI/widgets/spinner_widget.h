#pragma once
/*
 * spinner_widget.h — Indicatore di caricamento testuale animato
 * ==============================================================
 * Mostra un cursore rotante (⠋ ⠙ ⠸ ⠴ ⠦ ⠇) seguito da un testo
 * opzionale. Si avvia/ferma con start()/stop().
 *
 * Uso:
 *   auto* spin = new SpinnerWidget("Generazione AI...", parent);
 *   spin->start();   // avvia animazione
 *   spin->stop();    // ferma e mostra testo finale
 *   spin->hide();
 *
 * Non usa risorse grafiche: solo Unicode + QTimer.
 */

#include <QLabel>
#include <QTimer>
#include <QString>

class SpinnerWidget : public QLabel {
    Q_OBJECT

public:
    explicit SpinnerWidget(const QString& text = QString(),
                           QWidget* parent = nullptr)
        : QLabel(parent)
        , m_text(text)
        , m_frame(0)
    {
        /* Stile: colore accent, font leggermente ingrandito */
        setObjectName("spinnerLabel");
        hide();

        /* Timer aggiornamento frame: 100ms = ~10 fps, fluido senza caricare la CPU */
        m_timer = new QTimer(this);
        m_timer->setInterval(100);
        connect(m_timer, &QTimer::timeout, this, &SpinnerWidget::nextFrame);
    }

    /** Avvia lo spinner e rende il widget visibile. */
    void start(const QString& text = QString()) {
        if (!text.isEmpty()) m_text = text;
        m_frame = 0;
        show();
        nextFrame();     /* aggiorna subito senza aspettare il primo tick */
        m_timer->start();
    }

    /** Ferma lo spinner, mostra un messaggio finale e nasconde dopo N ms. */
    void stop(const QString& finalText = QString(), int hideAfterMs = 2000) {
        m_timer->stop();
        if (finalText.isEmpty()) {
            hide();
        } else {
            setText(finalText);
            if (hideAfterMs > 0)
                QTimer::singleShot(hideAfterMs, this, &QWidget::hide);
        }
    }

    /** true se l'animazione è attiva. */
    bool isSpinning() const { return m_timer->isActive(); }

private slots:
    void nextFrame() {
        /* Braille spinner — 8 frame per rotazione completa */
        static const char* FRAMES[] = {
            "\xe2\xa0\x8b", "\xe2\xa0\x99", "\xe2\xa0\xb8",
            "\xe2\xa0\xb4", "\xe2\xa0\xa6", "\xe2\xa0\x87",
            "\xe2\xa0\x8f", "\xe2\xa0\x9f"
        };
        constexpr int N = 8;
        const QString frame = QString::fromUtf8(FRAMES[m_frame % N]);
        m_frame = (m_frame + 1) % N;

        if (m_text.isEmpty())
            setText(frame);
        else
            setText(frame + "  " + m_text);
    }

private:
    QTimer* m_timer;
    QString m_text;
    int     m_frame;
};
