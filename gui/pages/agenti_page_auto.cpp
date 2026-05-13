/* ══════════════════════════════════════════════════════════════
   agenti_page_auto.cpp — Agente Autonomo con Pianificazione

   Implementa il pattern ReAct (Reasoning + Acting) che permette
   all'AI di pianificare, usare strumenti, osservare i risultati
   e iterare senza intervento dell'utente per ogni passo.

   Formato di risposta atteso dall'AI:
     THOUGHT: <ragionamento interno>
     ACTION: {"tool": "nome", "input": "valore"}
     (ripetuto N volte)
     FINAL_ANSWER: <risposta completa all'utente>

   Flusso:
     1. runAutonomousAgent() → invoca AI con history
     2. onToken() accumula in m_autoBuf
     3. onFinished() → _autoAdvance(resp)
     4. _autoAdvance(): parse THOUGHT/ACTION/FINAL_ANSWER
        - ACTION trovato → runToolCall() → inietta OBSERVATION → loop
        - FINAL_ANSWER trovato → mostra bolla finale, termina
        - step limit → termina con avviso
   ══════════════════════════════════════════════════════════════ */
#include "agenti_page.h"
#include "agenti_page_p.h"
#include "../prismalux_paths.h"
namespace P = PrismaluxPaths;
#include <QRegularExpression>
#include <QTextCursor>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

/* ══════════════════════════════════════════════════════════════
   _autoSystemPrompt — istruzioni ReAct per il modello
   ══════════════════════════════════════════════════════════════ */
QString AgentiPage::_autoSystemPrompt()
{
    return QString::fromUtf8(
        "Sei un agente autonomo di Prismalux. Risolvi il compito assegnato usando i "
        "seguenti strumenti in modo iterativo.\n\n"
        "FORMATO OBBLIGATORIO (una riga per tag, nessun testo fuori dai tag):\n"
        "THOUGHT: <ragionamento breve, max 2 righe>\n"
        "ACTION: {\"tool\": \"nome\", \"input\": \"valore\"}\n\n"
        "Quando hai la risposta definitiva:\n"
        "FINAL_ANSWER: <risposta completa e dettagliata all\xe2\x80\x99utente>\n\n"
        "STRUMENTI DISPONIBILI:\n"
        "- calc       : calcola espressioni matematiche (es. sqrt(144), 2**10+5)\n"
        "- ricerca    : cerca informazioni online via DuckDuckGo Instant Answer\n"
        "- python     : esegue codice Python in sandbox isolata\n"
        "- leggi_file : legge un file dal filesystem locale (input: percorso assoluto)\n"
        "- lista_file : elenca il contenuto di una cartella (input: percorso assoluto)\n"
        "- scrivi_file: scrive un file (input: \"percorso|||contenuto\")\n\n"
        "REGOLE:\n"
        "1. Scrivi SEMPRE THOUGHT prima di ACTION\n"
        "2. Usa solo UNO strumento per passo\n"
        "3. Quando ricevi OBSERVATION: analizza il risultato e continua\n"
        "4. Scrivi FINAL_ANSWER solo quando hai abbastanza informazioni\n"
        "5. Rispondi SEMPRE in italiano\n"
        "6. Sii conciso nel THOUGHT (max 2 righe)"
    );
}

/* ══════════════════════════════════════════════════════════════
   buildAutoStepHtml — card HTML per un passo del ciclo ReAct
   ══════════════════════════════════════════════════════════════ */
QString AgentiPage::buildAutoStepHtml(int step, const QString& thought,
                                       const QString& action, const QString& obs)
{
    QString html;
    html += QString(
        "<div style='"
        "background:#0f172a;border:1px solid #334155;"
        "border-radius:8px;padding:8px 12px;margin:4px 0;'>"
        "<p style='color:#64748b;font-size:11px;margin:0 0 6px 0;font-weight:bold;'>"
        "\xf0\x9f\xa4\x96  Step %1</p>").arg(step);

    if (!thought.isEmpty()) {
        html += QString(
            "<p style='color:#94a3b8;font-size:12px;margin:0 0 4px 0;font-style:italic;'>"
            "<b style='color:#64748b;'>THOUGHT:</b> %1</p>")
            .arg(thought.toHtmlEscaped());
    }
    if (!action.isEmpty()) {
        html += QString(
            "<p style='color:#fbbf24;font-family:monospace;font-size:11px;margin:0 0 4px 0;'>"
            "<b>ACTION:</b> <code style='background:#1c1917;padding:2px 4px;"
            "border-radius:3px;'>%1</code></p>")
            .arg(action.toHtmlEscaped());
    }
    if (!obs.isEmpty()) {
        html += QString(
            "<p style='color:#86efac;font-family:monospace;font-size:11px;margin:0;'>"
            "<b>OBSERVATION:</b> %1</p>")
            .arg(obs.left(400).toHtmlEscaped());
    }
    html += "</div>";
    return html;
}

/* ══════════════════════════════════════════════════════════════
   runAutonomousAgent — avvia/continua un ciclo ReAct.
   Chiamata: alla prima invocazione e dopo ogni OBSERVATION.
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::runAutonomousAgent()
{
    if (m_ai->busy()) return;

    m_autoBuf.clear();
    m_waitLbl->setVisible(true);

    /* Marca la posizione di inizio per la sostituzione post-streaming */
    QTextCursor cur(m_log->document());
    cur.movePosition(QTextCursor::End);
    m_agentBlockStart = cur.position();

    /* Inserisci indicatore "in elaborazione" */
    cur.insertHtml(QString(
        "<p style='color:#94a3b8;font-size:11px;margin:4px 0;'>"
        "\xf0\x9f\xa4\x96&nbsp;<b>Step %1/%2</b>&nbsp;"
        "\xe2\x80\x94&nbsp;in elaborazione...</p>")
        .arg(m_autoStep + 1).arg(m_autoMaxSteps));
    m_agentTextStart = cur.position();

    m_opMode = OpMode::AutonomousAgent;

    const QString sys = _autoSystemPrompt();

    if (m_autoHistory.isEmpty()) {
        /* Prima chiamata: solo il task dell'utente, nessuna storia */
        m_ai->chat(sys, m_autoLastUserMsg);
    } else {
        /* Chiamate successive: invia la history completa.
           L'ultimo elemento è il messaggio "OBSERVATION: ..." dell'utente. */
        const QString lastMsg = m_autoHistory.last()["content"].toString();
        m_ai->chat(sys, lastMsg, m_autoHistory, AiClient::QueryComplex);
    }
}

/* ══════════════════════════════════════════════════════════════
   _autoAdvance — processa la risposta del modello e avanza.

   Parsing robusto: cerca THOUGHT:, ACTION:, FINAL_ANSWER: con
   regex case-insensitive, tolera spazi e varianti minori.
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::_autoAdvance(const QString& resp)
{
    m_opMode = OpMode::Idle;   /* temporaneamente Idle durante il parsing */

    /* Rimuove il blocco <think>...</think> prima del parsing ReAct.
       I modelli think-capable (qwen3, deepseek-r1) emettono il reasoning
       nel tag <think> prima della risposta strutturata THOUGHT/ACTION/FINAL_ANSWER.
       Lo strip evita che i tag raw appaiano nelle bolle o confondano le regex. */
    auto stripThinkBlock = [](const QString& s) -> QString {
        const int end = s.indexOf("</think>");
        return (end != -1) ? s.mid(end + 8).trimmed() : s;
    };
    const QString parsed = stripThinkBlock(resp);

    /* ── Estrai THOUGHT, ACTION, FINAL_ANSWER ── */
    static const QRegularExpression reThought(
        "THOUGHT:\\s*(.+?)(?=\\n(?:ACTION|FINAL_ANSWER):|$)",
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression reAction(
        "ACTION:\\s*(\\{[^\\n\\r]+\\})",
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression reFinal(
        "FINAL_ANSWER:\\s*([\\s\\S]+)$",
        QRegularExpression::CaseInsensitiveOption);

    const auto mThought = reThought.match(parsed);
    const auto mAction  = reAction.match(parsed);
    const auto mFinal   = reFinal.match(parsed);

    const QString thought = mThought.hasMatch() ? mThought.captured(1).trimmed() : QString();
    const QString action  = mAction.hasMatch()  ? mAction.captured(1).trimmed()  : QString();
    const QString finalAns = mFinal.hasMatch()  ? mFinal.captured(1).trimmed()   : QString();

    /* ── Rimuove l'indicatore di streaming e lo sostituisce con la card ── */
    QTextCursor sel(m_log->document());
    sel.setPosition(m_agentBlockStart);
    sel.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    sel.removeSelectedText();

    /* ── FINAL_ANSWER: risposta completa — termina il ciclo ── */
    if (!finalAns.isEmpty() || (!mAction.hasMatch() && !thought.isEmpty())) {
        const QString answer = finalAns.isEmpty() ? parsed.trimmed() : finalAns;

        /* Bolla step finale con il thought (se c'è) e la risposta */
        if (!thought.isEmpty() && finalAns.isEmpty() == false) {
            sel.insertHtml(buildAutoStepHtml(m_autoStep + 1, thought, QString(), QString()));
        }

        /* Bolla risposta finale */
        { int idx = m_bubbleIdx++;
          m_bubbleTexts[idx] = answer;
          sel.insertHtml(buildAgentBubble(
              "\xf0\x9f\xa4\x96  Agente Autonomo",
              m_ai->model(),
              QString("Step %1/%2").arg(m_autoStep + 1).arg(m_autoMaxSteps),
              markdownToHtml(answer), idx)); }

        m_autoHistory.append(QJsonObject{{"role","assistant"},{"content",resp}});

        emit pipelineStatus(100, "\xe2\x9c\x85  Agente autonomo completato");
        _setRunBusy(false);
        m_opMode = OpMode::Idle;
        emit chatCompleted(m_autoLastUserMsg.left(40), m_log->toHtml());
        return;
    }

    /* ── ACTION: esegui strumento ── */
    if (!action.isEmpty()) {
        const QJsonObject tc = QJsonDocument::fromJson(action.toUtf8()).object();
        if (tc.isEmpty()) {
            /* JSON malformato — tratta come risposta finale */
            sel.insertHtml(buildAutoStepHtml(m_autoStep + 1, thought, action, "[JSON malformato]"));
            { int idx = m_bubbleIdx++;
              m_bubbleTexts[idx] = parsed;
              sel.insertHtml(buildAgentBubble(
                  "\xf0\x9f\xa4\x96  Agente Autonomo",
                  m_ai->model(), QString("Step %1").arg(m_autoStep + 1),
                  markdownToHtml(parsed), idx)); }
            _setRunBusy(false);
            emit chatCompleted(m_autoLastUserMsg.left(40), m_log->toHtml());
            return;
        }

        /* Mostra la card del passo senza OBSERVATION (arriverà dopo il tool) */
        sel.insertHtml(buildAutoStepHtml(m_autoStep + 1, thought, action, QString()));

        /* Aggiunge il turno dell'agente alla history */
        m_autoHistory.append(QJsonObject{{"role","assistant"},{"content",resp}});
        m_autoStep++;

        /* Verifica limite step */
        if (m_autoStep >= m_autoMaxSteps) {
            QTextCursor endCur(m_log->document());
            endCur.movePosition(QTextCursor::End);
            endCur.insertHtml(
                "<p style='color:#f87171;font-size:12px;margin:4px 0;'>"
                "\xe2\x9a\xa0  Limite massimo di step raggiunto ("
                + QString::number(m_autoMaxSteps) +
                "). Agente terminato senza risposta finale.</p>");
            _setRunBusy(false);
            emit chatCompleted(m_autoLastUserMsg.left(40), m_log->toHtml());
            return;
        }

        /* Esegui lo strumento in modo asincrono */
        m_waitLbl->setVisible(true);
        const int capturedStep = m_autoStep;
        runToolCall(tc, [this, tc, capturedStep](const QString& toolResult) {
            /* Aggiunge OBSERVATION come turno utente nella history */
            const QString obs = QString("OBSERVATION: %1").arg(toolResult);
            m_autoHistory.append(QJsonObject{{"role","user"},{"content",obs}});

            /* Aggiorna la card del passo con l'OBSERVATION */
            QTextCursor endCur(m_log->document());
            endCur.movePosition(QTextCursor::End);
            endCur.insertHtml(
                "<p style='color:#86efac;font-family:monospace;font-size:11px;"
                "margin:2px 0 6px 16px;'>"
                "\xe2\x9c\x85&nbsp;<b>OBSERVATION:</b>&nbsp;"
                + toolResult.left(300).toHtmlEscaped() + "</p>");

            if (!m_userScrolled) {
                m_suppressScrollSig = true;
                m_log->ensureCursorVisible();
                m_suppressScrollSig = false;
            }

            /* Continua il ciclo */
            runAutonomousAgent();
        });
        return;
    }

    /* ── Risposta senza tag riconoscibili: tratta come risposta finale ── */
    { int idx = m_bubbleIdx++;
      m_bubbleTexts[idx] = parsed;
      sel.insertHtml(buildAgentBubble(
          "\xf0\x9f\xa4\x96  Agente Autonomo",
          m_ai->model(), QString("Step %1").arg(m_autoStep + 1),
          markdownToHtml(parsed), idx)); }

    _setRunBusy(false);
    emit chatCompleted(m_autoLastUserMsg.left(40), m_log->toHtml());
}
