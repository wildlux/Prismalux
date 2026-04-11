#pragma once
/*
 * status_badge.h — Indicatore di stato compatto (dot colorato + etichetta)
 * =========================================================================
 * Mostra un cerchio ● colorato seguito da un testo descrittivo.
 * Usato per segnalare lo stato di backend/server/connessione.
 *
 * Stati disponibili:
 *   Offline  → grigio  #5a5f80  — servizio non raggiungibile
 *   Online   → verde   #69f0ae  — servizio attivo e pronto
 *   Starting → giallo  #ffd600  — avvio in corso
 *   Error    → rosso   #ff1744  — errore o timeout
 *
 * Uso:
 *   auto* badge = new StatusBadge("Server", parent);
 *   badge->setStatus(StatusBadge::Online, "Pronto :8081");
 *   badge->setStatus(StatusBadge::Starting, "Avvio...");
 */

#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>

class StatusBadge : public QWidget {
    Q_OBJECT

public:
    enum Status { Offline, Online, Starting, Error };

    explicit StatusBadge(const QString& label = QString(),
                         QWidget* parent = nullptr)
        : QWidget(parent)
    {
        auto* lay = new QHBoxLayout(this);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(5);

        /* Cerchio colorato U+25CF ● */
        m_dot = new QLabel("\xe2\x97\x8f", this);
        m_dot->setObjectName("statusDot");

        m_label = new QLabel(label, this);
        m_label->setObjectName("statusLabel");

        lay->addWidget(m_dot);
        lay->addWidget(m_label);

        setStatus(Offline);
    }

    /**
     * setStatus — Aggiorna colore del dot e testo dell'etichetta.
     * @param s     Nuovo stato (Offline / Online / Starting / Error)
     * @param text  Se non vuoto, sovrascrive l'etichetta corrente
     */
    void setStatus(Status s, const QString& text = QString()) {
        m_status = s;
        if (!text.isEmpty()) m_label->setText(text);

        /* Colore per stato: Offline=grigio, Online=verde, Starting=giallo, Error=rosso */
        static const char* COLORS[] = {
            "#5a5f80",   /* Offline  */
            "#69f0ae",   /* Online   */
            "#ffd600",   /* Starting */
            "#ff1744",   /* Error    */
        };
        m_dot->setStyleSheet(
            QString("color: %1; font-size: 9px;").arg(COLORS[s]));
    }

    /** Etichetta corrente. */
    QString text() const { return m_label->text(); }

    /** Stato corrente. */
    Status status() const { return m_status; }

private:
    QLabel* m_dot;
    QLabel* m_label;
    Status  m_status = Offline;
};
