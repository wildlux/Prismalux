#pragma once
#include <QString>
#include <QList>
#include <QMetaType>

struct Offerta {
    QString azienda, ruolo, sede, tipo, livello, email, requisiti;
};
Q_DECLARE_METATYPE(Offerta)

const QList<Offerta>& kOfferte();
