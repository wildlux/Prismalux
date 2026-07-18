/* ══════════════════════════════════════════════════════════════
   main_ai_hermes.cpp — Memoria persistente Hermes (AgentiPage)
   Usa m_log (QTextBrowser) di AgentiPage per output testo.
   ══════════════════════════════════════════════════════════════ */
#include "main_ai.h"
#include "main_ai_p.h"
#include "../prismalux_paths.h"
#include "../graph_memory.h"
#include <QDir>
#include <QDateTime>
#include <QSettings>

namespace P = PrismaluxPaths;

/* ══════════════════════════════════════════════════════════════
   hermesInit — apre/crea il DB SQLite per la memoria persistente
   DB separato da Multi-Agente e RagGraph: hermes_memory.db
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::hermesInit()
{
    const QString dbPath = QDir::homePath() + "/.prismalux/hermes_memory.db";
    m_hermesGm = new GraphMemory(dbPath, this);
    if (!m_hermesGm->open()) {
        delete m_hermesGm;
        m_hermesGm = nullptr;
        return;
    }
    m_hermesGm->pruneByImportance(500);   /* tieni top-500 nodi al lancio */
}

/* ══════════════════════════════════════════════════════════════
   hermesInjectContext — cerca nodi pertinenti alla query e li
   aggiunge al system prompt come "[Memoria da sessioni precedenti]"
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::hermesInjectContext(QString& sysPrompt, const QString& query)
{
    if (!m_hermesGm || query.trimmed().isEmpty()) return;

    const auto nodes = m_hermesGm->searchNodes(query, 5);
    if (nodes.isEmpty()) return;

    /* Raccoglie le etichette per la sezione "Fonti" a fine risposta */
    m_hermesLastSources.clear();
    for (const auto& n : nodes)
        if (!n.content.trimmed().isEmpty())
            m_hermesLastSources << n.label.left(60);

    /* Prefisso esplicito: il modello deve usare la memoria solo se
       strettamente pertinente alla domanda, non come fonte di esempi generici. */
    QString ctx =
        "\n\n[Contesto opzionale da sessioni precedenti — "
        "usalo SOLO se direttamente rilevante alla domanda attuale. "
        "Non citare questi dati come esempi in risposte a domande generali.]\n";
    for (const auto& n : nodes) {
        if (n.content.trimmed().isEmpty()) continue;
        ctx += QString("- [%1] %2\n").arg(n.label).arg(n.content.left(150));
    }
    ctx += "[fine contesto opzionale]\n";
    sysPrompt += ctx;
}

/* ══════════════════════════════════════════════════════════════
   hermesStoreConversation — salva un nodo "conversazione"
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::hermesStoreConversation(const QString& userMsg, const QString& aiResp)
{
    if (!m_hermesGm) return;
    if (userMsg.trimmed().isEmpty() || aiResp.trimmed().isEmpty()) return;

    const QString label   = userMsg.trimmed().left(60).simplified();
    const QString content = QString("D: %1\nR: %2")
                                .arg(userMsg.trimmed().left(300))
                                .arg(aiResp.trimmed().left(500));

    QVariantMap meta;
    meta["ts"]    = QDateTime::currentDateTime().toString(Qt::ISODate);
    meta["model"] = m_ai ? m_ai->model() : QString();

    m_hermesGm->addNode("conversation", label, content, 0.7f, meta);
    m_hermesGm->pruneByImportance(200);
}

/* ══════════════════════════════════════════════════════════════
   hermesReflect — analisi AI delle conversazioni memorizzate
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::hermesReflect()
{
    if (!m_hermesGm || !m_ai) return;

    const auto nodes = m_hermesGm->allNodes("conversation");
    if (nodes.isEmpty()) {
        if (m_log) m_log->append(
            "\xf0\x9f\xa7\xa0 <i>Hermes: nessuna conversazione memorizzata ancora.</i>");
        return;
    }

    const int lim = qMin(20, (int)nodes.size());
    QString summary = "Ecco le ultime conversazioni memorizzate:\n\n";
    for (int i = 0; i < lim; ++i)
        summary += QString("[%1] D: %2\n")
                       .arg(i + 1)
                       .arg(nodes[i].content.left(150));

    const QString sysReflect =
        "Sei un analista metacognitivo. Analizza le seguenti conversazioni "
        "memorizzate e identifica: (1) pattern ricorrenti nelle domande dell'utente, "
        "(2) eventuali errori o imprecisioni nelle risposte, "
        "(3) argomenti su cui l'utente torna spesso. "
        "Produci un breve report strutturato in italiano.";

    auto* reflectAi = new AiClient(this);
    reflectAi->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), m_ai->model());

    if (m_log) {
        m_log->moveCursor(QTextCursor::End);
        m_log->insertHtml(
            "<div style='background:#1e3a5f;padding:8px;border-radius:6px;margin:4px 0;'>"
            "\xf0\x9f\xa7\xa0 <b>Hermes — Riflessione</b> <span style='color:gray;font-size:11px;'>"
            "in corso...</span></div>");
    }
    if (m_waitLbl) m_waitLbl->setVisible(true);

    auto* accum = new QString;  /* buffer accumulazione token */

    connect(reflectAi, &AiClient::token, this,
            [this, accum](const QString& t) {
                *accum += t;
            });

    connect(reflectAi, &AiClient::finished, this,
            [this, reflectAi, accum](const QString& full) {
                reflectAi->deleteLater();
                if (m_waitLbl) m_waitLbl->setVisible(false);
                if (m_log && !accum->isEmpty()) {
                    m_log->moveCursor(QTextCursor::End);
                    m_log->insertHtml(
                        QString("<div style='background:#0f2a1f;padding:8px;border-radius:6px;"
                                "margin:4px 0;white-space:pre-wrap;'>%1</div>")
                            .arg(accum->toHtmlEscaped()));
                }
                delete accum;
                if (!m_hermesGm) return;
                m_hermesGm->addNode("reflection",
                    "Riflessione " + QDateTime::currentDateTime().toString("yyyy-MM-dd"),
                    full.trimmed().left(800), 0.95f);
                callKnowledgeMcp("CONTESTO: " + full.trimmed().left(400),
                                 "Hermes Reflection");
            });

    connect(reflectAi, &AiClient::error, this,
            [this, reflectAi, accum](const QString& err) {
                reflectAi->deleteLater();
                delete accum;
                if (m_waitLbl) m_waitLbl->setVisible(false);
                if (m_log)
                    m_log->append("\xe2\x9d\x8c  Riflessione fallita: " + err);
            });

    reflectAi->generate(sysReflect, summary, AiClient::QueryComplex);
}

/* ══════════════════════════════════════════════════════════════
   Slot UI
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::onHermesToggled(bool on)
{
    m_hermesEnabled = on;
    /* Persisti la scelta: al prossimo avvio la memoria riparte com'era
       (ripristino in buildBottomBar). */
    QSettings("Prismalux", "GUI").setValue(P::SK::kHermesEnabled, on);
    if (m_hermesToggle) {
        m_hermesToggle->setStyleSheet(
            on ? "QPushButton{background:#1e3a5f;color:#60a5fa;border-radius:4px;padding:0 4px;}"
               : "");
        m_hermesToggle->setToolTip(
            on ? "\xf0\x9f\xa7\xa0 Memoria Hermes ATTIVA — le conversazioni vengono memorizzate e usate come contesto"
               : "\xf0\x9f\xa7\xa0 Memoria Hermes DISATTIVA — clicca per abilitare la memoria persistente tra sessioni");
    }

    if (on && m_hermesGm && m_log) {
        const int n = m_hermesGm->allNodes().size();
        if (n == 0) return;
        m_log->append(
            QString("\xf0\x9f\xa7\xa0 Memoria Hermes attivata \xe2\x80\x94 %1 nodi memorizzati.\n"
                    "Le prossime risposte arricchiranno la memoria.\n"
                    "Usa il pulsante \"\xf0\x9f\x94\x84 Rifletti\" per un'analisi delle conversazioni.")
                .arg(n));
    }
}

void AgentiPage::onHermesReflectClicked()
{
    hermesReflect();
}
