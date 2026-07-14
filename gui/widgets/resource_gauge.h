#pragma once
#include <QWidget>
#include <QString>

class QLabel;
class QProgressBar;

/* ══════════════════════════════════════════════════════════════
   ResourceGauge — widget "CPU 45.0%" (label + barra + percentuale)
   Usato nell'header per mostrare CPU, RAM e GPU in tempo reale.
   Estratto da mainwindow.h/.cpp (TODO D-8).
   ══════════════════════════════════════════════════════════════ */
class ResourceGauge : public QWidget {
    Q_OBJECT
public:
    /** Costruisce il gauge con l'etichetta fissa (es. "CPU"). */
    explicit ResourceGauge(const QString& label, QWidget* parent = nullptr);

    /**
     * update(pct, detail) — Aggiorna valore e colore.
     * @param pct     Percentuale 0-100.
     * @param detail  Testo opzionale per tooltip (non usato attualmente).
     * Cambia il colore della barra: verde < 70%, giallo 70-90%, rosso > 90%.
     */
    void update(double pct, const QString& detail = "");

    /**
     * updateWithText(pct, valueText, detail) — come update() ma mostra
     * `valueText` (es. "12.8k") al posto della percentuale a destra.
     * Il riempimento e il colore della barra restano guidati da `pct`.
     */
    void updateWithText(double pct, const QString& valueText,
                        const QString& detail = "");

private:
    QLabel*       m_lbl;  ///< Etichetta fissa a sinistra ("CPU", "RAM", "GPU")
    QProgressBar* m_bar;  ///< Barra orizzontale colorata
    QLabel*       m_pct;  ///< Valore percentuale a destra ("45.0%")

    /**
     * setLevel(pct) — Imposta la property QSS "level" per colorare la barra.
     * < 70% → "" (verde default), 70-90% → "warn" (giallo), > 90% → "crit" (rosso).
     * Chiama P::repolish() per forzare il ricalcolo dello stile.
     */
    void setLevel(double pct);
};
