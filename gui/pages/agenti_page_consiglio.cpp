#include "agenti_page.h"
#include "agenti_page_p.h"
#include "../prismalux_paths.h"
namespace P = PrismaluxPaths;
#include <QDialog>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QSet>
#include <QTextDocument>

/* ══════════════════════════════════════════════════════════════
   Consiglio Scientifico — query parallela multi-modello
   ══════════════════════════════════════════════════════════════ */
double AgentiPage::jaccardSim(const QString& a, const QString& b)
{
    QSet<QString> setA = QSet<QString>(a.split(' ', Qt::SkipEmptyParts).begin(),
                                       a.split(' ', Qt::SkipEmptyParts).end());
    QSet<QString> setB = QSet<QString>(b.split(' ', Qt::SkipEmptyParts).begin(),
                                       b.split(' ', Qt::SkipEmptyParts).end());
    if (setA.isEmpty() && setB.isEmpty()) return 1.0;
    QSet<QString> inter = setA;
    inter.intersect(setB);
    QSet<QString> uni = setA;
    uni.unite(setB);
    return uni.isEmpty() ? 0.0 : (double)inter.size() / (double)uni.size();
}

void AgentiPage::runConsiglioScientifico()
{
    QString task = m_input->toPlainText().trimmed();
    if (task.isEmpty()) {
        m_log->append("\xe2\x9a\xa0  Inserisci una domanda per il Consiglio Scientifico.");
        return;
    }
    if (m_modelInfos.isEmpty()) {
        m_log->append("\xe2\x9a\xa0  Nessun modello disponibile. Controlla la connessione al backend.");
        return;
    }

    if (!m_docContext.isEmpty()) task += "\n\n--- DOCUMENTO ALLEGATO ---\n" + m_docContext.left(8000);
    m_taskOriginal = task;

    /* Selezione strategia tramite dialog */
    {
        auto* dlg = new QDialog(this);
        dlg->setWindowTitle("\xf0\x9f\x8f\x9b  Consiglio Scientifico — Strategia");
        dlg->setFixedWidth(380);
        auto* lay = new QVBoxLayout(dlg);
        lay->addWidget(new QLabel("Scegli la strategia di aggregazione:", dlg));
        auto* cmb = new QComboBox(dlg);
        cmb->addItem("0  Aggregazione Pesata  (primaria + alternative)");
        cmb->addItem("1  Analisi Comparativa  (consenso Jaccard)");
        cmb->addItem("2  Sintesi AI  (un LLM sintetizza tutte le risposte)");
        cmb->setCurrentIndex(m_consiglioStrategy);
        lay->addWidget(cmb);
        auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
        lay->addWidget(bb);
        connect(bb, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
        connect(bb, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
        if (dlg->exec() != QDialog::Accepted) { dlg->deleteLater(); return; }
        m_consiglioStrategy = cmb->currentIndex();
        dlg->deleteLater();
    }

    /* Seleziona fino a 5 modelli */
    QVector<ModelInfo> selected;
    for (auto& mi : m_modelInfos) {
        selected.append(mi);
        if (selected.size() >= 5) break;
    }
    if (selected.isEmpty()) {
        m_log->append("\xe2\x9d\x8c  Nessun modello selezionabile.");
        return;
    }

    /* Libera eventuali peer precedenti */
    for (auto& p : m_peers) if (p.client) p.client->deleteLater();
    m_peers.clear();
    m_peersDone = 0;

    m_log->clear();
    m_log->append(QString("\xf0\x9f\x8f\x9b  <b>Consiglio Scientifico</b> — %1 modelli in parallelo\n")
                  .arg(selected.size()));
    m_log->append(QString("\xe2\x9d\x93  Domanda: <i>%1</i>\n").arg(task));
    m_log->append(QString(43, QChar(0x2500)));

    m_opMode = OpMode::ConsiglioScientifico;
    _setRunBusy(true);
    m_waitLbl->setText(QString("\xf0\x9f\x94\x84  %1 modelli in elaborazione...").arg(selected.size()));
    m_waitLbl->setVisible(true);

    const QString sys =
        "Sei un esperto scientifico del Consiglio Prismalux. "
        "Rispondi in modo preciso, strutturato e approfondito. "
        "Rispondi SEMPRE e SOLO in italiano.";

    for (const auto& mi : selected) {
        ConsiglioPeer peer;
        peer.client = new AiClient(this);
        peer.client->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), mi.name);
        peer.model  = mi.name;

        /* Peso basato sul nome del modello */
        QString nl = mi.name.toLower();
        if (nl.contains("math") || nl.contains("qwen"))        peer.weight = 1.4;
        else if (nl.contains("coder") || nl.contains("code"))  peer.weight = 1.3;
        else if (nl.contains("r1") || nl.contains("reason") || nl.contains("think")) peer.weight = 1.2;
        else                                                    peer.weight = 1.0;

        peer.done   = false;
        peer.accum  = "";

        int peerIdx = m_peers.size();
        m_log->append(QString("\xf0\x9f\xa4\x96  <b>%1</b>  [peso: %2]  \xf0\x9f\x94\x84 generando...")
                      .arg(mi.name).arg(peer.weight, 0, 'f', 1));

        /* Connetti token e finished */
        connect(peer.client, &AiClient::token, this, [this, peerIdx](const QString& t){
            if (peerIdx < m_peers.size()) m_peers[peerIdx].accum += t;
        });
        connect(peer.client, &AiClient::finished, this, [this, peerIdx](const QString&){
            if (peerIdx >= m_peers.size()) return;
            m_peers[peerIdx].done = true;
            m_peersDone++;
            m_log->append(QString("\xe2\x9c\x85  <b>%1</b> completato (%2/%3)")
                          .arg(m_peers[peerIdx].model)
                          .arg(m_peersDone)
                          .arg(m_peers.size()));
            if (m_peersDone >= m_peers.size()) aggregaConsiglio();
        });
        connect(peer.client, &AiClient::error, this, [this, peerIdx](const QString& err){
            if (peerIdx >= m_peers.size()) return;
            m_peers[peerIdx].done  = true;
            m_peers[peerIdx].accum = QString("[Errore: %1]").arg(err);
            m_peersDone++;
            if (m_peersDone >= m_peers.size()) aggregaConsiglio();
        });

        m_peers.append(peer);
    }

    /* Avvia tutte le chat in parallelo — adatta sys al modello specifico del peer */
    for (auto& p : m_peers)
        p.client->chat(_buildSys(task, sys, p.model, m_ai->backend()), task);
}

void AgentiPage::aggregaConsiglio()
{
    m_waitLbl->setVisible(false);
    _setRunBusy(false);
    m_opMode = OpMode::Idle;

    m_log->append("\n" + QString(43, QChar(0x2500)));
    m_log->append("\xf0\x9f\x8f\x9b  <b>Aggregazione Consiglio Scientifico</b>");

    if (m_consiglioStrategy == 0) {
        /* Strategia 0: Aggregazione Pesata */
        m_log->append("\n\xf0\x9f\x94\xac  <b>Strategia: Aggregazione Pesata</b>");
        /* Mostra la risposta con peso maggiore come primaria */
        int bestIdx = 0;
        for (int i = 1; i < m_peers.size(); i++)
            if (m_peers[i].weight > m_peers[bestIdx].weight) bestIdx = i;
        QString html = markdownToHtml(m_peers[bestIdx].accum.trimmed());
        { int idx = m_bubbleIdx++; m_bubbleTexts[idx] = m_peers[bestIdx].accum.trimmed();
          m_log->moveCursor(QTextCursor::End);
          m_log->insertHtml(buildAgentBubble(
              "\xf0\x9f\x8f\x9b  Risposta Primaria",
              m_peers[bestIdx].model,
              QTime::currentTime().toString("HH:mm:ss"),
              html, idx)); }
        m_log->append("");
        /* Alternative */
        for (int i = 0; i < m_peers.size(); i++) {
            if (i == bestIdx) continue;
            m_log->append(QString("\n\xf0\x9f\x94\x97  <b>%1</b> (peso %2):")
                          .arg(m_peers[i].model).arg(m_peers[i].weight, 0, 'f', 1));
            m_log->append(m_peers[i].accum.trimmed().left(500) +
                          (m_peers[i].accum.length() > 500 ? "..." : ""));
        }
    } else if (m_consiglioStrategy == 1) {
        /* Strategia 1: Analisi Comparativa (Jaccard) */
        m_log->append("\n\xf0\x9f\x94\xac  <b>Strategia: Analisi Comparativa (consenso Jaccard)</b>");
        /* Calcola similarità pairwise, trova il peer con Jaccard medio più alto */
        int best = 0; double bestScore = -1.0;
        for (int i = 0; i < m_peers.size(); i++) {
            double sum = 0.0;
            for (int j = 0; j < m_peers.size(); j++) {
                if (i == j) continue;
                sum += jaccardSim(m_peers[i].accum, m_peers[j].accum);
            }
            double avg = m_peers.size() > 1 ? sum / (m_peers.size() - 1) : 0.0;
            if (avg > bestScore) { bestScore = avg; best = i; }
        }
        m_log->append(QString("\xf0\x9f\x93\x8a  Risposta con consenso massimo: <b>%1</b> (Jaccard medio: %2)")
                      .arg(m_peers[best].model).arg(bestScore, 0, 'f', 3));
        QString html = markdownToHtml(m_peers[best].accum.trimmed());
        { int idx = m_bubbleIdx++; m_bubbleTexts[idx] = m_peers[best].accum.trimmed();
          m_log->moveCursor(QTextCursor::End);
          m_log->insertHtml(buildAgentBubble(
              "\xf0\x9f\x8f\x9b  Risposta Consenso",
              m_peers[best].model,
              QTime::currentTime().toString("HH:mm:ss"),
              html, idx)); }
        m_log->append("");
    } else {
        /* Strategia 2: Sintesi tramite LLM principale */
        m_log->append("\n\xf0\x9f\x94\xac  <b>Strategia: Sintesi AI</b> — invio all'AI per sintesi...");

        QString combined = QString("Domanda originale: %1\n\n").arg(m_taskOriginal);
        for (int i = 0; i < m_peers.size(); i++) {
            combined += QString("=== Risposta da %1 ===\n%2\n\n")
                        .arg(m_peers[i].model)
                        .arg(m_peers[i].accum.trimmed().left(2000));
        }

        const QString sysSintesi =
            "Sei un sintetizzatore scientifico del Consiglio Prismalux. "
            "Hai ricevuto le risposte di %1 modelli AI alla stessa domanda. "
            "Crea una sintesi coerente, evidenziando:\n"
            "1. I punti di accordo\n"
            "2. Le divergenze significative\n"
            "3. La risposta più completa e accurata\n"
            "Rispondi SEMPRE e SOLO in italiano.";

        m_opMode = OpMode::Pipeline;  /* usa il flusso normale per streaming */
        m_agentOutputs.clear();
        m_agentOutputs.append("");
        m_currentAgent = 0;
        m_currentAgentLabel = "\xf0\x9f\x8f\x9b  Sintetizzatore Consiglio";
        m_currentAgentModel = m_ai->model();
        m_currentAgentTime  = QTime::currentTime().toString("HH:mm:ss");
        {
            QTextCursor c(m_log->document());
            c.movePosition(QTextCursor::End);
            m_agentBlockStart = c.position();
        }
        m_log->append("\n\xf0\x9f\x8f\x9b  [Sintetizzatore] \xf0\x9f\x94\x84 generando...\n");
        m_agentTextStart = m_log->document()->characterCount() - 1;
        _setRunBusy(true);
        m_waitLbl->setVisible(true);
        m_ai->chat(_buildSys(m_taskOriginal, QString(sysSintesi).arg(m_peers.size()),
                             m_ai->model(), m_ai->backend()), combined);
    }

    /* Pulizia peer */
    for (auto& p : m_peers) if (p.client) { p.client->deleteLater(); p.client = nullptr; }
    m_peers.clear();
    m_peersDone = 0;

    emit chatCompleted(m_taskOriginal.left(40), m_log->toHtml());
}

