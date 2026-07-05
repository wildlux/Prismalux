#include "main_ai.h"
#include "main_ai_p.h"
#include "../prismalux_paths.h"
#include "../log_bus.h"
#include <QListWidget>
#include <QRegularExpression>
namespace P = PrismaluxPaths;
#include <QElapsedTimer>
#include <QTextCursor>
#include <QSettings>
#include <QDateTime>
#include <QMessageBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QTextBrowser>
#include <QPushButton>
#include <QProcess>
#include <QTemporaryFile>
#include <QRegularExpression>
#include <QFile>
#include <QFileInfo>
#include <QTimer>
#include <cmath>
#include "dialog_agents_config.h"
#include "../widgets/chart_widget.h"
#include "../widgets/formula_parser.h"

/* ══════════════════════════════════════════════════════════════
   Slot segnali AI
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::onToken(const QString& t) {
    if (m_opMode == OpMode::ConsiglioScientifico) {
        /* I peer gestiscono i loro token internamente via lambda — ignora */
        return;
    }
    /* In modalità Idle non siamo attivi: i token appartengono ad un'altra pagina.
       Ignorarli previene output fantasma nel log degli agenti. */
    if (m_opMode == OpMode::Idle) return;

    m_waitLbl->setVisible(false);

    /* Quando l'utente sta visualizzando una sessione diversa, accumuliamo
       i token in m_bgBuffer senza mostrarli nel log corrente. */
    if (m_bgMode) {
        m_bgBuffer += t;
        if (m_opMode == OpMode::Pipeline && !m_agentOutputs.isEmpty())
            m_agentOutputs.last() += t;
        else if (m_opMode == OpMode::Byzantine) {
            if      (m_byzStep == 0) m_byzA += t;
            else if (m_byzStep == 2) m_byzC += t;
        } else if (m_opMode == OpMode::MathTheory) {
            if      (m_byzStep == 0) m_byzA += t;
            else if (m_byzStep == 2) m_byzC += t;
        } else if (m_opMode == OpMode::PipelineControl) {
            m_ctrlAccum += t;
        } else if (m_opMode == OpMode::KnowledgeExtract) {
            m_knowledgeBuf += t;
        } else if (m_opMode == OpMode::AutonomousAgent) {
            m_autoBuf += t;
        }
        return;
    }

    QTextCursor cursor(m_log->document());
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(t);
    if (!m_userScrolled) {
        m_suppressScrollSig = true;
        m_log->ensureCursorVisible();
        m_suppressScrollSig = false;
    }

    if (m_opMode == OpMode::Pipeline && !m_agentOutputs.isEmpty()) {
        m_agentOutputs.last() += t;

        /* FEAT-2: registra TTFT (⚡Nms) al primo token — mostrato nello status */
        if (!m_ttftCaptured) {
            m_ttftCaptured = true;
            const int ttftMs = static_cast<int>(m_agentTimer.elapsed());
            emit pipelineStatus(-2,
                QString("\xe2\x9a\xa1 TTFT: %1 ms").arg(ttftMs));  /* ⚡ — -2 = solo status bar */
        }

        m_tokenCount++;
        int total = 0, done = 0;
        for (int i = 0; i < MAX_AGENTS; i++) if (m_cfgDlg->enabledChk(i)->isChecked()) total++;
        for (int i = 0; i < m_currentAgent; i++) if (m_cfgDlg->enabledChk(i)->isChecked()) done++;
        if (total > 0) {
            double share    = 100.0 / total;
            double startPct = done * share;
            double fill     = share * (1.0 - std::exp(-m_tokenCount / 150.0)) * 0.92;
            emit pipelineStatus(qBound(0, (int)(startPct + fill), 99), QString());
        }
    }
    else if (m_opMode == OpMode::Byzantine) {
        if      (m_byzStep == 0) m_byzA += t;
        else if (m_byzStep == 2) m_byzC += t;
    } else if (m_opMode == OpMode::MathTheory) {
        if      (m_byzStep == 0) m_byzA += t;
        else if (m_byzStep == 2) m_byzC += t;
    } else if (m_opMode == OpMode::PipelineControl) {
        m_ctrlAccum += t;
    } else if (m_opMode == OpMode::Translating) {
        m_translateBuf += t;
    } else if (m_opMode == OpMode::KnowledgeExtract) {
        /* Estrattore silenzioso: accumula senza mostrare nel log */
        m_knowledgeBuf += t;
        cursor.deletePreviousChar();
    } else if (m_opMode == OpMode::AutonomousAgent) {
        m_autoBuf += t;
    }
}

/* ══════════════════════════════════════════════════════════════
   _finishedTranslating — traduzione completata, riavvia modalità pending
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::_finishedTranslating(const QString& /*full*/) {
    QString translated = m_translateBuf.trimmed();
    if (translated.isEmpty()) translated = m_taskOriginal;
    m_taskOriginal = _inject_math(translated);
    m_log->append(QString("\n\xf0\x9f\x8c\x90  Traduzione: <i>%1</i>\n").arg(m_taskOriginal));
    m_log->append(QString(43, QChar(0x2500)));
    OpMode next = m_pendingMode;
    m_pendingMode = OpMode::Idle;
    m_opMode = OpMode::Idle;
    if (next == OpMode::Pipeline) {
        m_agentOutputs.clear();
        m_currentAgent = 0;
        m_maxShots = m_cfgDlg->numAgents();
        m_opMode   = OpMode::Pipeline;
        for (int i = 0; i < MAX_AGENTS; i++) m_cfgDlg->enabledChk(i)->setStyleSheet("");
        emit pipelineStatus(0, "Avvio pipeline...");
        { int idx = m_bubbleIdx++; m_bubbleTexts[idx] = m_taskOriginal;
          m_log->moveCursor(QTextCursor::End);
          m_log->insertHtml(buildUserBubble(m_taskOriginal, idx, m_taskHtml)); }
        m_log->append("");
        m_waitLbl->setVisible(true);
        advancePipeline();
    } else if (next == OpMode::Byzantine) {
        m_byzStep = 0; m_byzA = m_byzC = "";
        m_opMode = OpMode::Byzantine;
        m_log->append("\xf0\x9f\x94\xae  Motore Byzantino — verifica a 4 agenti\n");
        m_log->append("\xf0\x9f\x85\x90  [Agente A — Originale]\n");
        m_waitLbl->setVisible(true);
        m_ai->chat(_buildSys(m_taskOriginal, QString(
            "Sei l'Agente A del Motore Byzantino di Prismalux. "
            "Fornisci una risposta diretta e ben argomentata. "
            "Rispondi SEMPRE e SOLO in italiano."),
            m_ai->model(), m_ai->backend()), m_taskOriginal);
    } else if (next == OpMode::MathTheory) {
        m_byzStep = 0; m_byzA = m_byzC = "";
        m_opMode = OpMode::MathTheory;
        m_log->append("\xf0\x9f\xa7\xae  Matematico Teorico — esplorazione a 4 agenti\n");
        m_log->append("\xf0\x9f\x85\xb0  [Agente 1 — Enunciatore]\n");
        m_waitLbl->setVisible(true);
        m_ai->chat(_buildSys(m_taskOriginal, QString(
            "Sei l'Enunciatore matematico. Riformula il problema in forma rigorosa. "
            "Rispondi SEMPRE e SOLO in italiano."),
            m_ai->model(), m_ai->backend()), m_taskOriginal);
    } else {
        /* next == Idle: traduzione esplicita senza pipeline successiva */
        m_log->append("\n" + m_translateBuf.trimmed());
        if (!m_preTranslateModel.isEmpty() && m_preTranslateModel != m_ai->model())
            m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), m_preTranslateModel);
        m_preTranslateModel.clear();
        _setRunBusy(false);
        if (m_btnTranslate) m_btnTranslate->setEnabled(true);
    }
}

/* ══════════════════════════════════════════════════════════════
   onFinished — dispatcher: delega a handler per-modalità
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::onFinished(const QString& full) {
    m_waitLbl->setVisible(false);
    m_ai->clearActiveTools();

    if (m_opMode == OpMode::Idle) return;
    if (m_opMode == OpMode::Translating)      { _finishedTranslating(full); return; }
    if (m_opMode == OpMode::AutonomousAgent)  {
        _autoAdvance((full.isEmpty() ? m_autoBuf : full).trimmed()); return;
    }
    if (m_opMode == OpMode::KnowledgeExtract) { _finishedKnowledgeExtract(); return; }
    if (m_opMode == OpMode::PipelineControl)  { _finishedPipelineControl(); return; }
    if (m_opMode == OpMode::Pipeline)         { _finishedPipeline(full); return; }
    if (m_opMode == OpMode::MathTheory)       { _finishedMathTheory(); return; }
    _finishedByzantine();
}

/* ── _categorizeError ─────────────────────────────────────────────────────────
 * Classifica l'errore e restituisce un messaggio localizzato con suggerimento.
 * Categorie (in ordine di priorità):
 *   ram       → RAM critica (già nel messaggio, nessun hint aggiuntivo)
 *   abort     → operazione annullata (silenzioso)
 *   network   → Connection refused / ECONNREFUSED
 *   closed    → RemoteHostClosed (crash server o VRAM esaurita)
 *   timeout   → socket timeout / nessuna risposta
 *   json      → risposta malformata / JSON non valido
 *   other     → errore generico con hint backend
 * ──────────────────────────────────────────────────────────────────────────── */
static QString _categorizeError(const QString& msg, AiClient::Backend backend) {
    const QString ml = msg.toLower();

    /* RAM critica — il messaggio è già autoesplicativo */
    if (ml.contains("ram critica")) return msg;

    /* Operazione annullata dall'utente o dal sistema — nessun output */
    if (ml.contains("operation canceled") || ml.contains("operazione annullata") ||
        ml.contains("canceled") || ml.contains("aborted"))
        return QString();   /* stringa vuota = silenzioso */

    /* Connessione rifiutata: backend non avviato */
    if (ml.contains("connection refused") || ml.contains("econnrefused") ||
        ml.contains("refused") || ml.contains("could not connect")) {
        return (backend == AiClient::Ollama)
            ? "\xe2\x9d\x8c  Backend non raggiungibile. Avvia Ollama: <code>ollama serve</code>"
            : "\xe2\x9d\x8c  Backend non raggiungibile. Avvia llama-server dalla tab \xf0\x9f\x94\x8c Backend.";
    }

    /* Connessione chiusa improvvisamente: crash del server o VRAM esaurita */
    if (ml.contains("remotehostclosed") || ml.contains("remote host closed") ||
        ml.contains("connection reset") || ml.contains("broken pipe")) {
        return "\xe2\x9a\xa0  Connessione chiusa improvvisamente.\n"
               "\xf0\x9f\x92\xbe  Possibili cause: VRAM esaurita, crash del server, modello troppo grande.";
    }

    /* Timeout: modello lento o in caricamento */
    if (ml.contains("timed out") || ml.contains("timeout") || ml.contains("socket timeout")) {
        return "\xe2\x8f\xb1  Timeout: nessuna risposta dal backend.\n"
               "\xf0\x9f\xa4\x94  Il modello potrebbe essere in caricamento o troppo grande per la RAM disponibile.";
    }

    /* RAM insufficiente — Ollama restituisce 500 con "more system memory" */
    if (ml.contains("system memory") || ml.contains("more system memory") ||
        ml.contains("requires more") || ml.contains("not enough memory") ||
        ml.contains("out of memory") || ml.contains("cannot allocate")) {
        return
            "<div style='border:1px solid #f87171;border-radius:6px;"
            "background:#1e1e2e;padding:8px 12px;margin:4px 0;'>"
            "<p style='color:#f87171;font-weight:bold;margin:0 0 6px 0;'>"
            "\xe2\x9d\x8c  RAM insufficiente per questo modello</p>"
            "<p style='color:#e2e8f0;margin:0 0 8px 0;font-size:11px;'>" +
            msg +
            "</p>"
            "<p style='color:#94a3b8;font-size:11px;margin:0 0 4px 0;'>"
            "\xf0\x9f\x92\xa1  <b>Ottimizzazioni disponibili:</b></p>"
            "<table style='font-size:11px;border-spacing:0 4px;'>"
            "<tr><td style='color:#4ade80;padding-right:8px;'>\xf0\x9f\x9a\x80</td>"
            "<td><b>Modalit\xc3\xa0 GPU</b> \xe2\x80\x94 usa la VRAM invece della RAM &nbsp;"
            "<a href='settings:hardware' style='color:#60a5fa;'>"
            "\xe2\x86\x92 Apri Impostazioni Hardware</a></td></tr>"
            "<tr><td style='color:#a78bfa;padding-right:8px;'>\xe2\x9a\x96\xef\xb8\x8f</td>"
            "<td><b>Modalit\xc3\xa0 Misto GPU+CPU</b> \xe2\x80\x94 divide i layer tra VRAM e RAM &nbsp;"
            "<a href='settings:hardware' style='color:#60a5fa;'>"
            "\xe2\x86\x92 Apri Impostazioni Hardware</a></td></tr>"
            "<tr><td style='color:#fbbf24;padding-right:8px;'>\xf0\x9f\x92\xbe</td>"
            "<td><b>Comprimi RAM con zRAM</b> \xe2\x80\x94 lz4 o zstd (Meta/Facebook) &nbsp;"
            "<a href='settings:hardware' style='color:#60a5fa;'>"
            "\xe2\x86\x92 Attiva da Impostazioni Hardware</a></td></tr>"
            "<tr><td style='color:#94a3b8;padding-right:8px;'>\xf0\x9f\xa4\x96</td>"
            "<td><b>Usa un modello pi\xc3\xb9 piccolo</b> \xe2\x80\x94 es. "
            "<code>qwen3:4b</code> (~2.5 GB) o <code>deepseek-r1:1.5b</code> (~1 GB)</td></tr>"
            "</table></div>";
    }

    /* Internal Server Error generico: potrebbe essere RAM o altro */
    if (ml.contains("internal server error") || ml.contains("server error")) {
        return
            "<p style='color:#f87171;'>\xe2\x9d\x8c  " + msg + "</p>"
            "<p style='color:#94a3b8;font-size:11px;'>"
            "\xf0\x9f\x92\xa1  Se il modello non entra in memoria, prova <b>Modalit\xc3\xa0 GPU</b> o <b>Misto</b> "
            "oppure attiva la <b>compressione zRAM</b> (lz4/zstd) &nbsp;"
            "<a href='settings:hardware' style='color:#60a5fa;'>"
            "\xe2\x86\x92 Impostazioni Hardware</a></p>";
    }

    /* Risposta JSON malformata */
    if (ml.contains("json") || ml.contains("parse") || ml.contains("malform") ||
        ml.contains("invalid content")) {
        return "\xe2\x9a\xa0  Risposta malformata dal backend. Riprova.";
    }

    /* Errore generico con hint backend */
    QString out = "\xe2\x9d\x8c  " + msg;
    out += (backend == AiClient::Ollama)
        ? "\n\xf0\x9f\x8c\xab  La nebbia copre la verit\xc3\xa0. Controlla che Ollama sia in esecuzione: <code>ollama serve</code>"
        : "\n\xf0\x9f\x8c\xab  La nebbia copre la verit\xc3\xa0. Controlla che llama-server sia avviato.";
    return out;
}

void AgentiPage::onError(const QString& msg) {
    m_waitLbl->setVisible(false);

    /* KnowledgeExtract è opzionale: il lavoro principale è già concluso.
       Tratta il fallimento come completamento parziale (senza aggiornare la KB). */
    if (m_opMode == OpMode::KnowledgeExtract) {
        m_knowledgeBuf.clear();
        m_opMode = OpMode::Idle;
        _setRunBusy(false);
        emit pipelineStatus(100, "\xe2\x9c\x85  Lavoro completato");
        emit chatCompleted(m_taskOriginal.left(40), m_log->toHtml());
        return;
    }

    /* Reset stato agente autonomo */
    if (m_opMode == OpMode::AutonomousAgent) {
        m_ctxAuto->clear();
        m_autoStep    = 0;
        m_autoBuf.clear();
    }

    /* In pipeline: colora in rosso il checkbox dell'agente che ha fallito */
    if (m_opMode == OpMode::Pipeline && m_currentAgent < MAX_AGENTS)
        m_cfgDlg->enabledChk(m_currentAgent)->setStyleSheet(
            "QCheckBox { color: #ef4444; }"
            "QCheckBox::indicator:checked { background-color: #ef4444;"
            " border: 2px solid #b91c1c; border-radius: 3px; }");

    const QString categorized = _categorizeError(msg, m_ai->backend());
    if (!categorized.isEmpty()) {
        m_log->append("\n" + categorized);
        LogBus::post("\xe2\x9d\x8c AI Stream: " + msg.left(120));
    }

    /* Aggiorna la barra di stato: abort silenzioso → nascondi, errore reale → mostra */
    if (categorized.isEmpty())
        emit pipelineStatus(-1, "");
    else
        emit pipelineStatus(100, "\xe2\x9d\x8c  " + msg.left(60));

    _setRunBusy(false);
    m_opMode = OpMode::Idle;
}

/* ══════════════════════════════════════════════════════════════
   tryShowChart — rileva formula o coppie di punti nel testo AI.
   Mostra sempre inline nella chat; il pulsante "Apri nel Grafico" del panel
   consente di spostarlo nella sezione Grafico per zoom/export avanzati.
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::tryShowChart(const QString& text) {
    if (!m_chartPanel) return;

    double xMin = -10.0, xMax = 10.0;
    FormulaParser::tryExtractXRange(text, xMin, xMax);

    /* 1. Formula "y = expr" / "f(x) = expr" */
    const QString expr = FormulaParser::tryExtract(text);
    if (!expr.isEmpty()) {
        FormulaParser fp(expr);
        if (fp.ok()) {
            const auto pts = fp.sample(xMin, xMax, 400);
            if (!pts.isEmpty()) {
                m_lastChartExpr = expr;
                m_lastChartXMin = xMin; m_lastChartXMax = xMax;
                m_lastChartPts.clear();
                const auto oldCharts = m_chartPanel->findChildren<ChartWidget*>();
                for (auto* c : oldCharts) { c->setParent(nullptr); c->deleteLater(); }
                auto* chart = new ChartWidget(m_chartPanel);
                chart->setTitle(expr);
                chart->setAxisLabels("x", "y");
                ChartWidget::Series s; s.name = expr; s.pts = pts;
                chart->addSeries(s);
                m_chartPanel->layout()->addWidget(chart);
                m_chartPanel->setVisible(true);
                return;
            }
        }
    }

    /* 2. Coppie di coordinate "(x, y)" */
    const auto pts = FormulaParser::tryExtractPoints(text);
    if (pts.size() >= 2) {
        m_lastChartExpr.clear();
        m_lastChartXMin = xMin; m_lastChartXMax = xMax;
        m_lastChartPts  = pts;
        const auto oldCharts = m_chartPanel->findChildren<ChartWidget*>();
        for (auto* c : oldCharts) { c->setParent(nullptr); c->deleteLater(); }
        auto* chart = new ChartWidget(m_chartPanel);
        chart->setAxisLabels("x", "y");
        ChartWidget::Series s; s.name = "Punti"; s.pts = pts; s.mode = ChartWidget::Scatter;
        chart->addSeries(s);
        m_chartPanel->layout()->addWidget(chart);
        m_chartPanel->setVisible(true);
    }
}

/* ══════════════════════════════════════════════════════════════
   Storia Chat — salvataggio + navigazione
   ══════════════════════════════════════════════════════════════ */

void AgentiPage::onChatCompletedSave(const QString& title, const QString& logHtml)
{
    /* Crea sessione se non esiste ancora */
    if (m_sessionId.isEmpty())
        m_sessionId = m_chatHistory.newSession(title);

    if (m_bgMode) {
        /* L'utente stava visualizzando un'altra sessione: salva il contenuto
           accumulato in background (m_bgHtmlSave + m_bgBuffer come blocco testo). */
        const QString bgFull = m_bgHtmlSave
            + (m_bgBuffer.isEmpty() ? QString()
               : "<p style='color:#94a3b8;font-size:11px;margin:4px 8px;'>"
                 + m_bgBuffer.toHtmlEscaped()
                 + "</p>");
        m_chatHistory.saveLog(m_sessionId, bgFull);
        m_bgMode = false;
        m_bgBuffer.clear();
        m_bgHtmlSave.clear();
        if (m_log) {
            m_log->moveCursor(QTextCursor::End);
            m_log->insertHtml(
                "<p style='color:#4ade80;font-size:11px;text-align:center;margin:4px 0;'>"
                "\xe2\x9c\x85  Risposta completata e salvata nella sessione precedente.</p>");
        }
    } else {
        /* Salvataggio normale */
        m_chatHistory.saveLog(m_sessionId, logHtml);
    }

}

/* ══════════════════════════════════════════════════════════════
   _detectWebIntent — pattern matching senza LLM.
   Rileva domande che richiedono dati in tempo reale o ricerca online.
   ══════════════════════════════════════════════════════════════ */
bool AgentiPage::_detectWebIntent(const QString& msg)
{
    static const QStringList kPatterns = {
        /* Orario/data: NON qui — il web non conosce l'ora locale del PC.
           Gestiti da guardiaDataOra() (zero LLM) o dalla direttiva D-31. */
        /* Richiesta esplicita di connessione online */
        "collegati online", "vai online", "cerca online", "cerca su internet",
        "cerca in rete", "cerca sul web", "cerca su google", "cerca su bing",
        "naviga", "apri internet", "connettiti",
        /* Notizie / eventi recenti */
        "ultime notizie", "notizie di oggi", "notizie recenti", "cosa \xc3\xa8 successo",
        "cosa sta succedendo", "aggiornamento", "novit\xc3\xa0 di oggi",
        /* Meteo */
        "meteo", "previsioni", "tempo oggi", "temperatura fuori", "piove",
        /* Finanza */
        "prezzo attuale", "quotazione", "cambio valuta", "borsa oggi",
        "bitcoin", "criptovaluta",
        /* Ricerca generica */
        "cerca informazioni su", "trovami informazioni", "dimmi qualcosa su",
        "wikipedia", "cosa sai di recente", "in tempo reale",
    };
    const QString lower = msg.toLower();
    for (const QString& p : kPatterns)
        if (lower.contains(p))
            return true;
    return false;
}

/* ══════════════════════════════════════════════════════════════
   runWebSearchAgent — agente di ricerca online.
   1. Bolla utente  2. "Cerco online..."  3. DuckDuckGo  4. Sintesi LLM
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::runWebSearchAgent(const QString& query)
{
    /* ── Bolla utente ── */
    const int bubbleId = m_bubbleIdx++;
    m_bubbleTexts[bubbleId] = query;
    m_log->moveCursor(QTextCursor::End);
    m_log->insertHtml(buildUserBubble(query, bubbleId));
    m_log->append("");

    /* ── Bolla AI placeholder "Cerco online..." ── */
    const int aiBubbleId = m_bubbleIdx++;
    m_bubbleTexts[aiBubbleId] = "";
    m_log->moveCursor(QTextCursor::End);
    m_log->insertHtml(
        "<p style='margin:4px 0 4px 8px;padding:10px 14px;"
        "background:#1e293b;border-radius:12px;color:#94a3b8;"
        "font-style:italic;max-width:85%;'>"
        "\xf0\x9f\x94\x8d  Cerco online: <b>" + query.toHtmlEscaped() + "</b>...</p>");
    m_log->append("");

    emit pipelineStatus(10, "\xf0\x9f\x94\x8d  Ricerca online in corso...");
    _setRunBusy(true);

    /* ── Chiamata DuckDuckGo tramite runToolCall ── */
    QJsonObject call;
    call["tool"]  = "ricerca";
    call["input"] = query;

    runToolCall(call, [this, query, aiBubbleId](const QString& searchResult) {
        /* ── Sintesi LLM con i risultati ── */
        const QString now = QDateTime::currentDateTime()
                                .toString("dddd d MMMM yyyy, HH:mm");
        const QString sys = _buildSys(
            "Sei un assistente che risponde a domande usando risultati di ricerca web. "
            "Data e ora attuale: " + now + ". "
            "Rispondi in italiano, in modo conciso e diretto. "
            "Se i risultati non contengono la risposta, dillo chiaramente.",
            QString(), m_ai->model(), m_ai->backend());

        const QString userWithContext =
            "Domanda originale: " + query + "\n\n"
            "Risultati ricerca online:\n" + searchResult + "\n\n"
            "Rispondi alla domanda originale usando questi risultati.";

        /* Rimuovi il placeholder e aggiungi bolla AI reale */
        m_log->moveCursor(QTextCursor::End);
        m_log->insertHtml(
            "<p style='margin:4px 0 4px 8px;padding:10px 14px;"
            "background:#1e293b;border-radius:12px;color:#64748b;"
            "font-size:11px;font-style:italic;max-width:85%;'>"
            "\xf0\x9f\x94\x8d  Fonte: DuckDuckGo  "
            "<span style='color:#475569;'>&mdash; elaborazione in corso...</span></p>");

        emit pipelineStatus(40, "\xf0\x9f\xa4\x96  Elaboro risultati...");
        m_ai->chat(sys, userWithContext);
        /* chat() può rifiutare la richiesta (throttle <400ms, RAM critica,
           già occupato): in quel caso nessun finished()/error() arriverà —
           senza questo reset il pulsante resterebbe su "Stop" per sempre. */
        if (!m_ai->busy()) _setRunBusy(false);
    });
}


