#pragma once
#include <QString>
#include <QList>
#include <QMetaType>

struct Offerta {
    QString azienda, ruolo, sede, tipo, livello, email, requisiti;
};
Q_DECLARE_METATYPE(Offerta)

/* ── Dati ── */
const QList<Offerta>& kOfferte();

/* ── Algoritmo di filtro (puro — nessuna dipendenza Qt Widgets) ── */
QList<Offerta> offerteFiltrate(const QString& tipo, const QString& livello);

/* ── Presentazione dati (mappatura campi → icone, testabile in isolamento) ── */
QString tipoIcon(const QString& tipo);
QString livLabel(const QString& livello);
