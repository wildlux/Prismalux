#include "agenti_page.h"
#include "agenti_page_p.h"
#include "../prismalux_paths.h"
namespace P = PrismaluxPaths;
#include <QComboBox>
#include <QBrush>
#include <QColor>
#include <algorithm>
#include "agents_config_dialog.h"

/* ══════════════════════════════════════════════════════════════
   _isEmbeddingModel — restituisce true se il modello è di embedding
   (non supporta /api/chat e non deve essere usato dalla pipeline).
   Pattern coperti: nomic-embed-text, mxbai-embed, all-minilm,
   bge-*, snowflake-arctic-embed, granite-embedding, *-embed-*, ecc.
   ══════════════════════════════════════════════════════════════ */
bool _isEmbeddingModel(const QString& name) {
    const QString lo = name.toLower();
    return lo.contains("embed")    ||
           lo.contains("minilm")   ||
           lo.startsWith("bge-")   ||
           lo.contains("rerank")   ||
           lo.contains("colbert");
}

/* ══════════════════════════════════════════════════════════════
   onModelsReady — aggiorna combo modelli nel dialog
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::onModelsReady(const QStringList& list) {
    /* Rimuove i modelli di embedding dalla lista: non supportano /api/chat
       e causano errore "does not support chat" se selezionati. */
    QStringList chatOnly;
    for (const QString& m : list)
        if (!_isEmbeddingModel(m)) chatOnly << m;
    /* Se il filtro elimina tutto (improbabile), usa la lista originale */
    const QStringList& list_ref = chatOnly.isEmpty() ? list : chatOnly;
    /* Con llama.cpp (server) i modelli da configurare sono i file GGUF locali,
       non il singolo modello correntemente caricato nel server.
       L'utente sceglie quale GGUF assegnare ad ogni agente;
       la pipeline userà quello già caricato (o in futuro riavvierà il server). */
    /* ── Helper lambda: popola m_cmbLLM con la lista data ── */
    auto populateLLMCombo = [this](const QStringList& models) {
        if (!m_cmbLLM) return;
        /* Blocca il segnale per evitare cambio modello mentre si ricarica */
        m_cmbLLM->blockSignals(true);
        /* Legge il modello corrente dal UserRole (nome raw senza icona) */
        const QString prev = m_cmbLLM->currentData(Qt::UserRole).toString().isEmpty()
                           ? m_cmbLLM->currentText()
                           : m_cmbLLM->currentData(Qt::UserRole).toString();
        m_cmbLLM->clear();

        const qint64 totalRam = P::totalRamBytes();
        for (const QString& m : models) {
            /* ☁️ cloud (size=0) — 🌍📍 locale (size>0) */
            const qint64 sz = m_ai->modelSizeBytes(m);
            const QString icon = (sz == 0)
                ? QString::fromUtf8("\xe2\x98\x81\xef\xb8\x8f  ")   /* ☁️  */
                : QString::fromUtf8("\xf0\x9f\x8c\x8d\xf0\x9f\x93\x8d  ");  /* 🌍📍  */
            m_cmbLLM->addItem(icon + m);
            {
                const int i = m_cmbLLM->count() - 1;
                /* Salva il nome raw nel UserRole per recuperarlo senza parsing */
                m_cmbLLM->setItemData(i, m, Qt::UserRole);
                /* Colora in rosso i modelli troppo grandi per la RAM disponibile
                 * (regola Shannon: serve almeno 2× la dimensione del file) */
                if (totalRam > 0) {
                    if (sz > 0 && sz * 2 > totalRam) {
                        m_cmbLLM->setItemData(i, QBrush(QColor("#ef4444")), Qt::ForegroundRole);
                        m_cmbLLM->setItemData(i,
                            QString("Attenzione: richiede ~%1 GB di RAM (regola 2\xc3\x97) "
                                    "— il sistema ha %2 GB totali. "
                                    "Potrebbe non avviarsi o essere molto lento.")
                                .arg(sz * 2 / 1e9, 0, 'f', 1)
                                .arg(totalRam / 1e9, 0, 'f', 1),
                            Qt::ToolTipRole);
                    }
                }
                /* Colora in arancione i modelli con bug noto (sfondo giallo) */
                if (P::isKnownBrokenModel(m)) {
                    m_cmbLLM->setItemData(i, QBrush(QColor("#ea580c")), Qt::ForegroundRole);
                    m_cmbLLM->setItemData(i, QBrush(QColor("#fef08a")), Qt::BackgroundRole);
                    m_cmbLLM->setItemData(i,
                        P::knownBrokenModelTooltip(),
                        Qt::ToolTipRole);
                }
            }
        }

        /* Ripristina selezione: priorità a m_pageModel (scelta esplicita dell'utente),
           poi al prev, poi al primo della lista.
           Se m_pageModel non è nella nuova lista (cambio backend), lo resettiamo
           in modo che modelChanged possa aggiornare la combo d'ora in poi. */
        const QString want = !m_pageModel.isEmpty() ? m_pageModel : prev;
        /* Cerca per nome raw (Qt::UserRole) prima di cercare per testo display */
        int idx = m_cmbLLM->findData(want, Qt::UserRole);
        if (idx < 0) idx = m_cmbLLM->findText(want);
        if (idx >= 0) {
            m_cmbLLM->setCurrentIndex(idx);
        } else {
            m_cmbLLM->setCurrentIndex(0);
            m_pageModel.clear();   /* modello preferito non disponibile → reset */
        }
        m_cmbLLM->blockSignals(false);
    };

    if (m_ai->backend() == AiClient::LlamaServer) {
        QStringList ggufNames;
        for (const QString& path : P::scanGgufFiles())
            ggufNames << QFileInfo(path).fileName();
        const QStringList effective = ggufNames.isEmpty() ? list_ref : ggufNames;
        m_cfgDlg->setModels(effective);
        populateLLMCombo(effective);
        /* Aggiorna m_modelInfos per Consiglio Scientifico */
        m_modelInfos.clear();
        for (const QString& m : effective)
            m_modelInfos.append({ m, m_ai->modelSizeBytes(m) });
        return;
    }

    /* ── Nessun modello: Ollama non e' in esecuzione ── */
    if (list_ref.isEmpty()) {
        if (m_cmbLLM) {
            m_cmbLLM->blockSignals(true);
            m_cmbLLM->clear();
            m_cmbLLM->addItem("\xe2\x9d\x8c  Ollama non attivo — avvia 'ollama serve'");
            m_cmbLLM->setEnabled(false);
            m_cmbLLM->blockSignals(false);
        }
        m_cfgDlg->setModels(QStringList{"\xe2\x9d\x8c  Ollama non attivo"});
        m_modelInfos.clear();
        return;
    }

    if (m_cmbLLM) m_cmbLLM->setEnabled(true);

    m_cfgDlg->setModels(list_ref);
    populateLLMCombo(list_ref);

    /* Aggiorna m_modelInfos per Consiglio Scientifico */
    m_modelInfos.clear();
    for (const QString& m : list_ref)
        m_modelInfos.append({ m, m_ai->modelSizeBytes(m) });
}


