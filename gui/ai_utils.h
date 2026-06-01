#pragma once
/* ════════════════════════════════════════════════════════════════
   ai_utils.h  —  Helper per popolare una QComboBox con i modelli.

   Preferire il widget riusabile:
       #include "widgets/model_combo_box.h"
       auto* combo = new ModelComboBox(m_ai, this);

   Usare populateModelCombo() solo quando si deve riempire un
   QComboBox già esistente senza cambiarne il tipo (es. widget
   generici non di proprietà diretta).

   Pattern one-shot: si connette a modelsReady/error una sola
   volta, poi si auto-disconnette tramite h->deleteLater().
   ════════════════════════════════════════════════════════════════ */
#include "ai_client.h"
#include "prismalux_paths.h"

#include <QComboBox>
#include <QObject>
#include <QString>
#include <QStringList>

namespace AiUtils {

inline void populateModelCombo(AiClient* ai, QComboBox* combo, QObject* context)
{
    if (!ai || !combo) return;
    namespace P = PrismaluxPaths;

    auto* h = new QObject(context);

    QObject::connect(ai, &AiClient::modelsReady, h,
        [combo, ai, h](const QStringList& models) {
            h->deleteLater();
            const QString cur = combo->currentData().toString();
            combo->blockSignals(true);
            combo->clear();
            for (const QString& m : models)
                combo->addItem(P::modelIcon(ai->modelSizeBytes(m), m) + m, m);
            int idx = combo->findData(cur);
            if (idx < 0) idx = 0;
            combo->setCurrentIndex(idx);
            combo->blockSignals(false);
        });

    QObject::connect(ai, &AiClient::error, h,
        [combo, h](const QString& msg) {
            h->deleteLater();
            if (combo->count() > 0) return;
            combo->addItem("\xe2\x9a\xa0  " + msg, QString());
        });

    ai->fetchModels();
}

/* Emoji per nome interfaccia di rete Linux */
inline QString ifaceEmoji(const QString& name)
{
    if (name.startsWith("wl") || name.startsWith("wifi"))
        return "\xf0\x9f\x93\xa1 ";   /* 📡 Wi-Fi */
    if (name.startsWith("eth") || name.startsWith("en") || name.startsWith("em"))
        return "\xf0\x9f\x94\x8c ";   /* 🔌 Ethernet */
    if (name.startsWith("tun") || name.startsWith("vpn") || name.startsWith("wg"))
        return "\xf0\x9f\x94\x92 ";   /* 🔒 VPN/Tunnel */
    if (name.startsWith("bt") || name.startsWith("bnep"))
        return "\xf0\x9f\x94\xb5 ";   /* 🔵 Bluetooth */
    if (name.startsWith("docker") || name.startsWith("veth")
        || name.startsWith("virbr") || name.startsWith("br"))
        return "\xf0\x9f\x90\xb3 ";   /* 🐳 Docker/Bridge */
    return "\xf0\x9f\x94\xa7 ";       /* 🔧 Altro */
}

} // namespace AiUtils
