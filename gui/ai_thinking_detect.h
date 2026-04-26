#pragma once
#include <QString>
#include <QJsonObject>

/* ══════════════════════════════════════════════════════════════
   ai_thinking_detect.h — rilevamento robusto del campo "thinking"
   nei chunk di streaming Ollama.

   Separato da ai_client.cpp per rendere la logica unit-testabile
   senza istanziare AiClient (che richiede Qt Network + event loop).

   Incluso da:
     • ai_client.cpp  — uso in produzione
     • tests/test_thinking_detect.cpp — unit test diretti
   ══════════════════════════════════════════════════════════════ */

/* ──────────────────────────────────────────────────────────────
   _extractThinkingToken
   Estrae il token di "thinking" da un oggetto message Ollama,
   indipendente dal nome esatto del campo.

   Strategia in due fasi:
     1. Name-based (priorità): scansiona tutti i campi stringa del
        message escluso "content" e "role"; se il nome contiene una
        keyword semantica (think/reason/thought/reflect/chain/cot/
        internal/ponder), ritorna il valore subito.
     2. Size-based (fallback): se nessun nome corrisponde, usa il
        campo stringa più lungo. Se supera kMinThinkingLen chars è
        quasi certamente un reasoning blob con nome sconosciuto.

   foundKey (out, opzionale): se non nullptr viene impostato al nome
   del campo trovato. Serve a fissare il campo per i chunk successivi
   dello stesso stream (anti-race: vedere m_thinkingKey in AiClient).
   ────────────────────────────────────────────────────────────── */
inline QString _extractThinkingToken(const QJsonObject& msg,
                                     QString* foundKey = nullptr)
{
    static constexpr int kMinThinkingLen = 15;

    static const QString kNameKW[] = {
        "think", "reason", "thought", "reflect",
        "chain",  "cot",   "internal", "ponder"
    };

    QString longestKey, longestVal;

    for (auto it = msg.constBegin(); it != msg.constEnd(); ++it) {
        const QString& key = it.key();
        if (key == "content" || key == "role") continue;
        if (!it.value().isString()) continue;

        const QString val = it.value().toString();
        if (val.isEmpty()) continue;

        /* Fase 1 — corrispondenza per nome */
        const QString kl = key.toLower();
        for (const QString& kw : kNameKW) {
            if (kl.contains(kw)) {
                if (foundKey) *foundKey = key;
                return val;
            }
        }

        /* Fase 2 — traccia il candidato più lungo */
        if (val.length() > longestVal.length()) {
            longestVal = val;
            longestKey = key;
        }
    }

    if (longestVal.length() >= kMinThinkingLen) {
        if (foundKey) *foundKey = longestKey;
        return longestVal;
    }
    return {};
}
