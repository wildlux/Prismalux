#pragma once
/*
 * dpi_utils.h — Helper DPI-aware per la GUI Prismalux
 * =====================================================
 * Fornisce funzioni inline per scalare i pixel hardcoded in base al DPI
 * logico dello schermo primario e al fattore zoom utente.
 *
 * Utilizzo:
 *   #include "dpi_utils.h"
 *   setFixedHeight(dpiScale(36));       // 36 px → scalato per DPI
 *   setFixedWidth(dpiScale(130));
 *
 * NOTA: qApp deve essere costruito prima di chiamare queste funzioni.
 *       Sicuro usarle nel costruttore di QMainWindow e successivi.
 *
 * REGOLA: usa dpiScale() per dimensioni strutturali (altezze, larghezze
 *         di widget, bottoni, header). Il font-size nel QSS è già gestito
 *         da ThemeManager::apply() — non serve dpiScale() per font.
 */

#include <QtGlobal>
#include <QGuiApplication>
#include <QScreen>
#include <cmath>

namespace DpiUtils {

/*
 * screenScale() — fattore DPI schermo primario (1.0 su 96 dpi standard).
 * Restituisce 1.0 se QGuiApplication non è ancora costruita o non c'è
 * schermo primario, per evitare crash durante inizializzazioni statiche.
 *
 * La soglia 108 dpi (= 96 × 1.125) previene scaling inutile su schermi
 * leggermente fuori standard che vengono comunque trattati come 1× .
 */
inline qreal screenScale()
{
    if (!qApp) return 1.0;
    const QScreen* scr = QGuiApplication::primaryScreen();
    if (!scr) return 1.0;
    const qreal dpi = scr->logicalDotsPerInch();
    return (dpi > 108.0) ? (dpi / 96.0) : 1.0;
}

/*
 * dpiScale(px) — scala un valore intero di pixel in base al DPI logico.
 * Ritorna almeno 1 per evitare widget di dimensione zero.
 *
 * Esempio:
 *   setFixedHeight(dpiScale(52));   // header 52 px su 96dpi, 78px su 144dpi
 */
inline int dpiScale(int px)
{
    return qMax(1, qRound(px * screenScale()));
}

/*
 * dpiScaleF(px) — variante in virgola mobile, utile per layout factor e margins.
 */
inline qreal dpiScaleF(qreal px)
{
    return px * screenScale();
}

/*
 * dpiSize(w, h) — scala una coppia larghezza×altezza.
 * Convenienza per setFixedSize().
 *
 * Esempio:
 *   btnHamburger->setFixedSize(dpiSize(36, 36));
 */
inline QSize dpiSize(int w, int h)
{
    const qreal s = screenScale();
    return QSize(qMax(1, qRound(w * s)), qMax(1, qRound(h * s)));
}

} // namespace DpiUtils

/* Alias brevi per l'uso diretto nel sorgente senza prefisso namespace */
using DpiUtils::dpiScale;
using DpiUtils::dpiScaleF;
using DpiUtils::dpiSize;

/* ── setBtnRunning — helper centralizzato per pulsante Invia↔Stop ──────────
 * Cambia testo + proprietà "danger" in modo coerente in tutta la GUI.
 * Il chiamante deve chiamare P::repolish(btn) dopo se il QSS usa [danger].
 *
 * Esempio:
 *   setBtnRunning(m_btnSend, true,  "\xe2\x9c\x88  Invia", "\xe2\x8f\xb9 Stop");
 *   setBtnRunning(m_btnSend, false, "\xe2\x9c\x88  Invia", "\xe2\x8f\xb9 Stop");
 */
#include <QPushButton>
inline void setBtnRunning(QPushButton* btn, bool running,
                          const char* idleText, const char* stopText)
{
    if (!btn) return;
    btn->setText(running ? QString::fromUtf8(stopText)
                         : QString::fromUtf8(idleText));
    btn->setProperty("danger", running);
    btn->setEnabled(true);
}
