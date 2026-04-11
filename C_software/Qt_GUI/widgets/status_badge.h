#pragma once
/*
 * status_badge.h — Pill indicatore backend (chip colorata, no dot)
 * ================================================================
 * Mostra il nome del backend attivo come una pill/chip arrotondata.
 * Il colore di sfondo cambia in base allo stato — stessa estetica
 * per Ollama e llama.cpp.
 *
 * Stati:
 *   Offline  → grigio  #2a2a2a / #6b7280  — non in uso / server fermo
 *   Online   → verde   #0d2d1f / #10a37f  — attivo e pronto
 *   Starting → giallo  #2d2500 / #eab308  — avvio in corso
 *   Error    → rosso   #2d0f0f / #ef4444  — errore o timeout
 *
 * Uso:
 *   auto* badge = new StatusBadge("Ollama", parent);
 *   badge->setStatus(StatusBadge::Online, "Ollama");
 *   badge->setStatus(StatusBadge::Starting, "Avvio...");
 */

#include <QLabel>

class StatusBadge : public QLabel {
    Q_OBJECT

public:
    enum Status { Offline, Online, Starting, Error };

    explicit StatusBadge(const QString& text = QString(),
                         QWidget* parent = nullptr)
        : QLabel(text, parent)
    {
        setObjectName("statusBadge");
        setAlignment(Qt::AlignCenter);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        setStatus(Offline, text);
    }

    void setStatus(Status s, const QString& text = QString()) {
        m_status = s;
        if (!text.isEmpty()) QLabel::setText(text);

        /* bg / text per ogni stato */
        struct Palette { const char* bg; const char* fg; };
        static constexpr Palette PAL[] = {
            { "#252525", "#6b7280" },   /* Offline  — grigio neutro */
            { "#0d2d1f", "#10a37f" },   /* Online   — verde scuro  */
            { "#2d2500", "#eab308" },   /* Starting — giallo scuro */
            { "#2d0f0f", "#ef4444" },   /* Error    — rosso scuro  */
        };
        setStyleSheet(
            QString("QLabel {"
                    " background-color: %1;"
                    " color: %2;"
                    " border-radius: 10px;"
                    " padding: 3px 10px;"
                    " font-size: 11px;"
                    " font-weight: 600;"
                    "}")
            .arg(PAL[s].bg, PAL[s].fg));
    }

    Status status() const { return m_status; }

private:
    Status m_status = Offline;
};
