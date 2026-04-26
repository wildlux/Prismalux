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
        const QString prev = m_cmbLLM->currentText();
        m_cmbLLM->clear();

        const qint64 totalRam = P::totalRamBytes();
        for (const QString& m : models) {
            m_cmbLLM->addItem(m);
            {
                const int i = m_cmbLLM->count() - 1;
                /* Colora in rosso i modelli troppo grandi per la RAM disponibile
                 * (regola Shannon: serve almeno 2× la dimensione del file) */
                if (totalRam > 0) {
                    const qint64 sz = m_ai->modelSizeBytes(m);
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
        const int idx = m_cmbLLM->findText(want);
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
        /* Con llama.cpp un solo modello può girare alla volta — auto-assign non ha senso */
        m_btnAuto->setEnabled(false);
        m_btnAuto->setToolTip(
            "Auto-assegna non disponibile con llama.cpp:\n"
            "un solo modello GGUF \xc3\xa8 caricato alla volta.\n"
            "Seleziona manualmente il modello per ogni agente.");
        return;
    }

    /* ── Nessun modello: Ollama non e' in esecuzione ── */
    if (list_ref.isEmpty()) {
        /* Mostra placeholder nel combo invece di lasciarlo vuoto */
        if (m_cmbLLM) {
            m_cmbLLM->blockSignals(true);
            m_cmbLLM->clear();
            m_cmbLLM->addItem("\xe2\x9d\x8c  Ollama non attivo — avvia 'ollama serve'");
            m_cmbLLM->setEnabled(false);
            m_cmbLLM->blockSignals(false);
        }
        m_cfgDlg->setModels(QStringList{"\xe2\x9d\x8c  Ollama non attivo"});
        m_btnAuto->setEnabled(false);
        m_btnAuto->setToolTip(
            "Ollama non raggiungibile.\n"
            "Avvia Ollama con: ollama serve\n"
            "Poi riapri Prismalux o cambia backend.");
        return;
    }

    /* Se in precedenza era disabilitato per lista vuota, riabilitalo */
    if (m_cmbLLM) m_cmbLLM->setEnabled(true);

    m_cfgDlg->setModels(list_ref);
    populateLLMCombo(list_ref);

    /* ── Auto-assegna: sempre abilitato. ── */
    const bool canAutoAssign = list_ref.size() > 1;
    m_btnAuto->setEnabled(true);
    m_btnAuto->setToolTip(canAutoAssign
        ? "Auto-assegna ruoli e modelli ottimali tramite orchestratore AI"
        : "Auto-assegna non disponibile: un solo modello presente.\n"
          "llama-server carica un modello alla volta — usa Ollama per la selezione automatica.");
}

/* ══════════════════════════════════════════════════════════════
   autoAssignRoles
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::autoAssignRoles() {
    QString task = m_input->toPlainText().trimmed();
    if (task.isEmpty()) {
        m_log->append("\xe2\x9a\xa0  Inserisci prima un task per l'auto-assegnazione."); return;
    }

    m_autoLbl->setText("\xf0\x9f\x94\x84  Recupero modelli disponibili...");
    m_autoLbl->setVisible(true);
    m_btnAuto->setEnabled(false);

    const bool isOllama = (m_ai->backend() == AiClient::Ollama);
    QString url = QString("http://%1:%2%3")
                  .arg(m_ai->host()).arg(m_ai->port())
                  .arg(isOllama ? "/api/tags" : "/v1/models");
    auto* reply = m_namAuto->get(QNetworkRequest(QUrl(url)));

    connect(reply, &QNetworkReply::finished, this, [this, reply, task, isOllama]{
        reply->deleteLater();
        m_modelInfos.clear();

        if (reply->error() == QNetworkReply::NoError) {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            if (isOllama) {
                for (auto v : doc.object()["models"].toArray()) {
                    QJsonObject obj = v.toObject();
                    m_modelInfos.append({ obj["name"].toString(),
                                          obj["size"].toVariant().toLongLong() });
                }
            } else {
                for (auto v : doc.object()["data"].toArray()) {
                    m_modelInfos.append({ v.toObject()["id"].toString(), 0 });
                }
            }
        }

        if (m_modelInfos.isEmpty()) {
            m_autoLbl->setText("\xe2\x9d\x8c  Nessun modello trovato sul server corrente.");
            m_btnAuto->setEnabled(true);
            return;
        }

        std::sort(m_modelInfos.begin(), m_modelInfos.end(),
                  [](const ModelInfo& a, const ModelInfo& b){ return a.size > b.size; });

        const QString biggest = m_modelInfos.first().name;
        const qint64  bigSize = m_modelInfos.first().size;

        QStringList modelNames;
        for (const auto& mi : m_modelInfos)
            modelNames << mi.name;

        auto roleList = AgentsConfigDialog::roles();
        QStringList roleNames;
        for (const auto& r : roleList)
            roleNames << r.name;

        m_autoLbl->setText(QString(
            "\xf0\x9f\xa4\x96  Orchestratore: \xef\xbc\x88%1, %2 GB\xef\xbc\x89 — assegnazione ruoli...")
            .arg(biggest)
            .arg(bigSize / 1'000'000'000.0, 0, 'f', 1));

        m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), biggest);

        m_opMode = OpMode::AutoAssign;
        m_autoBuffer.clear();

        QString sys =
            "Sei un orchestratore AI. Analizza il task e assegna i ruoli pi\xc3\xb9 efficaci "
            "agli agenti della pipeline. Rispondi SOLO con JSON valido, senza altro testo.";

        QString prompt = QString(
            "Task dell'utente: \"%1\"\n\n"
            "Ruoli disponibili: %2\n\n"
            "Modelli disponibili: %3\n\n"
            "Assegna da 2 a 6 agenti. Per ogni agente scegli ruolo e modello ottimali.\n"
            "Rispondi ESATTAMENTE con questo JSON (nessun testo prima o dopo):\n"
            "[\n"
            "  {\"agente\": 1, \"ruolo\": \"NomeRuolo\", \"modello\": \"nomemodello\"},\n"
            "  {\"agente\": 2, \"ruolo\": \"NomeRuolo\", \"modello\": \"nomemodello\"}\n"
            "]")
            .arg(task)
            .arg(roleNames.join(", "))
            .arg(modelNames.join(", "));

        m_ai->chat(sys, prompt);
    });
}

/* ══════════════════════════════════════════════════════════════
   parseAutoAssign
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::parseAutoAssign(const QString& raw) {
    QString cleaned = raw;

    /* Rimuove <think>...</think> */
    {
        QRegularExpression re("<think>[\\s\\S]*?</think>",
                              QRegularExpression::CaseInsensitiveOption);
        cleaned.remove(re);
    }
    /* Estrae blocco ```json ... ``` */
    {
        QRegularExpression re("```(?:json)?\\s*([\\s\\S]*?)```");
        auto m = re.match(cleaned);
        if (m.hasMatch()) cleaned = m.captured(1).trimmed();
    }

    int start = cleaned.indexOf('[');
    int end   = cleaned.lastIndexOf(']');
    if (start < 0 || end < start) {
        m_autoLbl->setText("\xe2\x9d\x8c  Formato JSON non valido nella risposta dell'orchestratore.");
        m_btnAuto->setEnabled(true);
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(cleaned.mid(start, end - start + 1).toUtf8());
    if (!doc.isArray()) {
        m_autoLbl->setText("\xe2\x9d\x8c  JSON non analizzabile.");
        m_btnAuto->setEnabled(true);
        return;
    }

    auto roleList = AgentsConfigDialog::roles();
    QJsonArray arr = doc.array();
    int assigned = 0;

    for (int i = 0; i < MAX_AGENTS; i++)
        m_cfgDlg->enabledChk(i)->setChecked(false);

    for (int i = 0; i < arr.size() && i < MAX_AGENTS; i++) {
        QJsonObject obj = arr[i].toObject();
        QString roleName  = obj["ruolo"].toString();
        QString modelName = obj["modello"].toString();

        int roleIdx = -1;
        for (int r = 0; r < roleList.size(); r++) {
            if (roleList[r].name.compare(roleName, Qt::CaseInsensitive) == 0) {
                roleIdx = r; break;
            }
        }
        if (roleIdx < 0) {
            for (int r = 0; r < roleList.size(); r++) {
                if (roleList[r].name.contains(roleName, Qt::CaseInsensitive) ||
                    roleName.contains(roleList[r].name, Qt::CaseInsensitive)) {
                    roleIdx = r; break;
                }
            }
        }
        if (roleIdx < 0) roleIdx = i % roleList.size();

        m_cfgDlg->enabledChk(i)->setChecked(true);
        m_cfgDlg->roleCombo(i)->setCurrentIndex(roleIdx);

        int mIdx = m_cfgDlg->modelCombo(i)->findText(modelName, Qt::MatchContains);
        if (mIdx >= 0) m_cfgDlg->modelCombo(i)->setCurrentIndex(mIdx);

        assigned++;
    }

    if (m_cfgDlg->modelCombo(0)->count() > 0)
        m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(),
                         m_cfgDlg->modelCombo(0)->currentText());

    m_autoLbl->setText(QString("\xe2\x9c\x85  Assegnati %1 agenti — puoi modificarli in \xe2\x9a\x99\xef\xb8\x8f Configura Agenti.").arg(assigned));
    m_btnAuto->setEnabled(true);
}

