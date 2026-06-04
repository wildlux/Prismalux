#pragma once
/* ══════════════════════════════════════════════════════════════
   ModelComboHelper — popola un QComboBox con la lista modelli Ollama
   in modo uniforme: icona dimensione + check modelli rotti + restore.

   Problema ripetuto: ogni pagina reimplementa ~15 righe identiche
   (blockSignals, clear, modelIcon, knownBroken, restore, blockSignals).
   Centralizzare garantisce comportamento e stile coerenti ovunque.

   Uso:
       #include "widgets/model_combo_helper.h"

       // Riempie il combo e ripristina la selezione precedente
       ModelComboHelper::populate(m_modelCombo, m_ai, models);

       // Versione con label separata da aggiornare
       ModelComboHelper::populate(m_modelCombo, m_ai, models, m_modelLabel);

       // Connette fetchModels() con holder one-shot (pattern completo)
       ModelComboHelper::connectFetch(m_ai, this, m_modelCombo);
   ══════════════════════════════════════════════════════════════ */

#include <QComboBox>
#include <QLabel>
#include <QObject>
#include <QStringList>
#include <QBrush>
#include <QColor>
#include "../prismalux_paths.h"
#include "../ai_client.h"

namespace P = PrismaluxPaths;

class ModelComboHelper {
public:
    /* ── Popola il combo con la lista modelli ─────────────────────
       - Salva e ripristina la selezione corrente
       - Aggiunge icona dimensione (P::modelIcon)
       - Colora in arancione i modelli noti come rotti
       - lbl: se non null viene aggiornata col modello corrente     */
    static void populate(QComboBox* combo,
                         AiClient*  ai,
                         const QStringList& models,
                         QLabel* lbl = nullptr)
    {
        if (!combo || !ai) return;

        /* Salva selezione corrente */
        const QString current = combo->currentData(Qt::UserRole).toString().isEmpty()
                              ? combo->currentText()
                              : combo->currentData(Qt::UserRole).toString();

        combo->blockSignals(true);
        combo->clear();

        for (const QString& m : models) {
            const qint64 sz = ai->modelSizeBytes(m);
            combo->addItem(P::modelIcon(sz, m) + m, m);

            if (P::isKnownBrokenModel(m)) {
                const int i = combo->count() - 1;
                combo->setItemData(i, QBrush(QColor("#ea580c")), Qt::ForegroundRole);
                combo->setItemData(i, QBrush(QColor("#fef08a")), Qt::BackgroundRole);
                combo->setItemData(i, P::knownBrokenModelTooltip(),   Qt::ToolTipRole);
            }
        }

        /* Ripristina selezione o fallback al primo */
        const int idx = combo->findData(current, Qt::UserRole);
        combo->setCurrentIndex(idx >= 0 ? idx : 0);
        combo->blockSignals(false);

        /* Aggiorna label opzionale */
        if (lbl) {
            const QString raw = combo->currentData(Qt::UserRole).toString();
            lbl->setText("\xf0\x9f\xa4\x96 " + (raw.isEmpty() ? combo->currentText() : raw));
        }
    }

    /* ── Versione "solo messaggio di errore" quando il fetch fallisce ──
       Aggiunge una voce grigia non selezionabile invece di lasciare
       il combo vuoto senza feedback.                                    */
    static void setError(QComboBox* combo, const QString& msg = {})
    {
        if (!combo) return;
        combo->blockSignals(true);
        combo->clear();
        combo->addItem(msg.isEmpty()
            ? "\xe2\x9d\x8c  Backend non raggiungibile"
            : msg);
        combo->setItemData(0, QBrush(QColor("#ef4444")), Qt::ForegroundRole);
        combo->model()->setData(
            combo->model()->index(0, 0),
            Qt::NoItemFlags,
            Qt::UserRole - 1);          /* rende la voce non selezionabile */
        combo->blockSignals(false);
    }

    /* ── Legge il nome raw del modello selezionato ─────────────────
       Il testo display può contenere icona o badge; UserRole = nome raw.
       Restituisce QString() se combo è null o vuoto.               */
    static QString currentModel(const QComboBox* combo)
    {
        if (!combo) return {};
        const QString raw = combo->currentData(Qt::UserRole).toString();
        return raw.isEmpty() ? combo->currentText() : raw;
    }

    /* ── Pattern completo: fetchModels + holder one-shot ──────────
       Crea un QObject holder (parent = ctx) che si auto-distrugge
       dopo la prima risposta (sia successo che errore).
       Questo disconnette automaticamente i segnali via Qt parent tracking.

       Uso tipico nel costruttore o in populateModels():
           ModelComboHelper::connectFetch(m_ai, this, m_modelCombo);

       lbl opzionale viene aggiornata come in populate().              */
    static void connectFetch(AiClient*  ai,
                             QObject*   ctx,
                             QComboBox* combo,
                             QLabel*    lbl = nullptr)
    {
        if (!ai || !ctx || !combo) return;

        auto* holder = new QObject(ctx);

        QObject::connect(ai, &AiClient::modelsReady, holder,
            [holder, ai, combo, lbl](const QStringList& models) {
                holder->deleteLater();
                ModelComboHelper::populate(combo, ai, models, lbl);
            });

        QObject::connect(ai, &AiClient::error, holder,
            [holder, combo](const QString& msg) {
                holder->deleteLater();
                ModelComboHelper::setError(combo,
                    "\xe2\x9d\x8c  " + msg.left(60));
            });

        ai->fetchModels();
    }
};
