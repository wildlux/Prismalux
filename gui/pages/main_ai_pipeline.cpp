#include "main_ai.h"
#include "main_ai_p.h"
#include "../prismalux_paths.h"
#include "../log_bus.h"
namespace P = PrismaluxPaths;
#include "../app_config.h"
#include <QElapsedTimer>
#include <QTimer>
#include <QTextCursor>
#include <QRegularExpression>
#include <QMessageBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>
#include <QProcess>
#include <QTemporaryFile>
#include <QFile>
#include <QFileInfo>
#include <cmath>
#include <algorithm>
#include "../widgets/formula_parser.h"
#include "../widgets/chart_widget.h"
#include "../widgets/model_combo_helper.h"
#include "dialog_agents_config.h"

/* ══════════════════════════════════════════════════════════════
   _buildOllamaTools — array tools in formato Ollama function calling.
   Inviato nella chat() quando m_toolsEnabled + Ollama backend + CHAT single mode.
   Il modello chiama i tool via message.tool_calls → onNativeToolCall() esegue
   e chiama replyWithTool() per continuare la conversazione.
   ══════════════════════════════════════════════════════════════ */
static QJsonArray _buildOllamaTools()
{
    auto mkTool = [](const QString& name, const QString& desc,
                     const QJsonObject& params) -> QJsonObject {
        QJsonObject fn;
        fn["name"]        = name;
        fn["description"] = desc;
        fn["parameters"]  = params;
        QJsonObject t;
        t["type"]     = "function";
        t["function"] = fn;
        return t;
    };

    auto strParam = [](const QString& desc) -> QJsonObject {
        QJsonObject p;
        p["type"]        = "object";
        QJsonObject props;
        QJsonObject val; val["type"] = "string"; val["description"] = desc;
        props["value"] = val;
        p["properties"] = props;
        p["required"]   = QJsonArray{QString("value")};
        return p;
    };

    return QJsonArray {
        /* ── Tool generici ── */
        mkTool("calc",
               "Calcola un'espressione matematica. "
               "Supporta: +,-,*,/,**, sqrt, sin, cos, log, ecc.",
               strParam("Espressione matematica da calcolare")),
        mkTool("fetch_url",
               "Scarica il contenuto HTML o testo di una pagina web dato il suo URL. "
               "Usare quando si ha un URL diretto (http:// o https://).",
               strParam("URL completa della pagina da scaricare")),
        mkTool("ricerca",
               "Cerca informazioni testuali online via DuckDuckGo Instant Answer. "
               "NON usare per URL — usa fetch_url per scaricare pagine web.",
               strParam("Query di testo da cercare")),
        mkTool("leggi_file",
               "Legge il contenuto di un file locale.",
               strParam("Percorso assoluto del file")),
        mkTool("lista_file",
               "Elenca i file in una directory locale.",
               strParam("Percorso assoluto della cartella")),
        mkTool("python",
               "Esegue codice Python in sandbox e ritorna l'output.",
               strParam("Codice Python da eseguire")),
        /* ── Tool Prismalux-specifici ── */
        mkTool("search_rag",
               "Cerca testo nei documenti RAG indicizzati localmente "
               "(cartelle ~/prismalux_rag_docs/ e Prismalux/RAG/). "
               "Usare per trovare informazioni nei tuoi documenti personali.",
               strParam("Query di ricerca nei documenti RAG locali")),
        mkTool("graph_memory",
               "Cerca entità, concetti e fatti nella memoria a grafo di Prismalux "
               "(GraphMemory SQLite). Utile per trovare informazioni apprese in sessioni precedenti.",
               strParam("Query di ricerca nella memoria a grafo")),
        mkTool("get_knowledge",
               "Legge la Knowledge Base personale dell'utente "
               "(file user_knowledge.md aggiornato automaticamente da Prismalux).",
               strParam("(lascia vuoto per leggere tutta la Knowledge Base)")),
        /* ── Sub-agente ── */
        mkTool("spawn_agent",
               "Crea un sub-agente AI specializzato che esegue un sotto-compito e restituisce il risultato. "
               "Utile per parallelizzare analisi, delegare sotto-task complessi o ottenere un secondo parere. "
               "Massimo 4 sub-agenti per sessione. "
               "Il sub-agente non ha accesso alla cronologia corrente: fornigli tutto il contesto nel campo 'task'.",
               [&]() -> QJsonObject {
                   QJsonObject p;
                   p["type"] = QLatin1String("object");
                   QJsonObject props;
                   QJsonObject role; role["type"] = QLatin1String("string");
                   role["description"] = QLatin1String("Ruolo/persona del sub-agente (es. 'Esperto di finanza', 'Analista dati', 'Revisore critico')");
                   QJsonObject task; task["type"] = QLatin1String("string");
                   task["description"] = QLatin1String("Compito completo da eseguire, incluso tutto il contesto necessario");
                   props["role"] = role;
                   props["task"] = task;
                   p["properties"] = props;
                   p["required"]   = QJsonArray{ QLatin1String("role"), QLatin1String("task") };
                   return p;
               }()),
        /* ── Plugin MCP ── */
        mkTool("mcp_call",
               "Invoca un tool di un plugin MCP attivo in Prismalux. "
               "I plugin disponibili sono in MCPs/ (es. anki_mcp, ollama_mcp, devagent_mcp, knowledge_mcp). "
               "Usare per accedere a funzionalita' avanzate non coperte dagli altri tool built-in.",
               [&]() -> QJsonObject {
                   QJsonObject p; p["type"] = QLatin1String("object");
                   QJsonObject props;
                   QJsonObject plugin;   plugin["type"]    = QLatin1String("string");
                                         plugin["description"] = QLatin1String("Nome del plugin MCP (es. anki_mcp, ollama_mcp, knowledge_mcp)");
                   QJsonObject toolName; toolName["type"]  = QLatin1String("string");
                                         toolName["description"] = QLatin1String("Nome del tool da invocare nel plugin");
                   QJsonObject argsJson; argsJson["type"]  = QLatin1String("string");
                                         argsJson["description"] = QLatin1String("Argomenti come stringa JSON (es. {\"query\":\"...\"}); usa {} se nessun argomento");
                   props["plugin"]    = plugin;
                   props["tool_name"] = toolName;
                   props["args_json"] = argsJson;
                   p["properties"] = props;
                   p["required"]   = QJsonArray{ QLatin1String("plugin"), QLatin1String("tool_name") };
                   return p;
               }()),
    };
}

/* ══════════════════════════════════════════════════════════════
   _isChartRequest — rileva intento grafico nel linguaggio naturale.
   Controlla parole chiave italiane e inglesi comunemente usate quando
   l'utente vuole un grafico, senza richiedere una formula parsabile.
   ══════════════════════════════════════════════════════════════ */
static bool _isChartRequest(const QString& task) {
    const QString t = task.toLower();
    static const QStringList kKeywords = {
        "grafico", "grafici", "disegna", "plotta", "visualizza",
        "chart", "plot", "draw", "graph",
        "curva", "curve", "andamento", "trend", "barchart", "istogramma",
        "scatter", "dispersione", "mostra.*dati", "mostra.*numeri",
        "crea.*grafico", "fai.*grafico", "fammi.*grafico", "genera.*grafico",
        "mostrami.*grafico", "voglio.*grafico"
    };
    static const QRegularExpression re(kKeywords.join("|"),
                                       QRegularExpression::CaseInsensitiveOption);
    return re.match(t).hasMatch();
}

/* ══════════════════════════════════════════════════════════════
   Pipeline sequenziale
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::runPipeline() {
    m_userScrolled = false;  /* nuovo task: torna in auto-scroll */
    QString task = _sanitize_prompt(m_input->toPlainText().trimmed());
    if (task.isEmpty()) { m_log->append("\xe2\x9a\xa0  Inserisci un task."); return; }

    /* Avviso se l'estrazione asincrona del file allegato non è ancora completata */
    if (m_docLoading) {
        m_log->append("\xe2\x8f\xb3  Attendi: estrazione del documento in corso...");
        return;
    }

    /* Grafici AI: pipeline risponde normalmente, tryShowChart() mostra inline.
       Il pulsante "Apri nel Grafico" nel panel consente di spostare poi. */

    {
        QElapsedTimer tmr; tmr.start();
        QString ris = guardiaMath(task);
        double ms = tmr.nsecsElapsed() / 1e6;
        if (!ris.isEmpty()) {
            m_log->clear();
            { int i = m_bubbleIdx++; m_bubbleTexts[i] = task;
              m_log->moveCursor(QTextCursor::End);
              m_log->insertHtml(buildUserBubble(task, i)); }
            m_log->append("");
            { int i = m_bubbleIdx++; m_bubbleTexts[i] = ris;
              m_log->moveCursor(QTextCursor::End);
              m_log->insertHtml(buildLocalBubble(ris, ms, i)); }
            m_input->clear();
            emit chatCompleted(task.left(40), m_log->toHtml());
            return;
        }
    }

    /* ── Guardia Grafico: formula rilevata → grafico locale, zero token AI ── */
    {
        const QString expr = FormulaParser::tryExtract(task);
        if (!expr.isEmpty()) {
            FormulaParser fp(expr);
            if (fp.ok()) {
                double xMin = -10.0, xMax = 10.0;
                FormulaParser::tryExtractXRange(task, xMin, xMax);
                const auto pts = fp.sample(xMin, xMax, 400);
                if (!pts.isEmpty()) {
                    /* In CHAT RAG i messaggi si accumulano (non clear); in Pipeline si resetta */
                    if (m_cfgDlg->numAgents() <= 1) {
                        /* CHAT RAG: non cancellare i messaggi precedenti */
                    } else {
                        m_log->clear();
                    }
                    m_taskOriginal = task;
                    { int i = m_bubbleIdx++; m_bubbleTexts[i] = task;
                      m_log->moveCursor(QTextCursor::End);
                      m_log->insertHtml(buildUserBubble(task, i)); }
                    m_log->append("");
                    /* Bolla chart: solo pulsante "Mostra grafico" senza testo formula */
                    {
                        const auto& _c = bc();
                        const int br = AppConfig::s().value(P::SK::kBubbleRadius,10).toInt();
                        const QString brs = QString::number(br) + "px";
                        const QString chartHtml =
                            "<p style='margin:6px 0;'></p>"
                            "<table width='100%' cellpadding='0' cellspacing='0'>"
                            "<tr>"
                              "<td bgcolor='" + QString(_c.lBg) + "' style='"
                                  "border:1px solid " + _c.lBdr + ";"
                                  "border-radius:" + brs + ";"
                                  "padding:10px 14px;"
                                  "color:" + _c.lTxt + ";"
                              "'>"
                                "<p style='color:" + _c.lHdr + ";font-size:11px;font-weight:bold;"
                                    "margin:0 0 8px 0;'>"
                                  "\xe2\x9a\xa1  Risposta locale  \xc2\xb7\xc2\xb7  0 token  \xc2\xb7\xc2\xb7  &lt;1 ms"
                                "</p>"
                                "<hr style='border:none;border-top:1px solid " + _c.lHr + ";margin:5px 0 8px 0;'>"
                                "<p align='center' style='margin:4px 0 8px 0;'>"
                                  "<a href='chart:show'"
                                     " style='color:" + _c.lBtn + ";font-size:14px;font-weight:bold;"
                                             "text-decoration:none;background:" + _c.lRes + ";"
                                             "border:1px solid " + _c.lBdr + ";"
                                             "border-radius:6px;padding:6px 20px;'>"
                                    "\xf0\x9f\x93\x88  Mostra grafico"
                                  "</a>"
                                "</p>"
                              "</td>"
                              "<td width='30'>&nbsp;</td>"
                            "</tr>"
                            "</table>"
                            "<p style='margin:4px 0;'></p>";
                        m_log->moveCursor(QTextCursor::End);
                        m_log->insertHtml(chartHtml);
                    }
                    tryShowChart(task);
                    m_input->clear();
                    emit chatCompleted(task.left(40), m_log->toHtml());
                    return;
                }
            }
        }
    }

    /* ── Guardia Calcoli Fisici: _inject_science ha trovato una conversione nota
       (CV↔W, km↔mi, kg↔lb, Ohm, ecc.) → risposta locale, zero token AI ── */
    {
        const QString injected = _inject_science(task);
        static const QString kTag = "[Calcolo locale:";
        if (injected.startsWith(kTag)) {
            const int close = injected.indexOf(']');
            const QString calcResult = close > 0
                ? injected.mid(kTag.length(), close - kTag.length()).trimmed()
                : injected.left(injected.indexOf('\n')).trimmed();
            QElapsedTimer tmr; tmr.start();
            const double ms = tmr.nsecsElapsed() / 1e6;
            { int i = m_bubbleIdx++; m_bubbleTexts[i] = task;
              m_log->moveCursor(QTextCursor::End);
              m_log->insertHtml(buildUserBubble(task, i)); }
            m_log->append("");
            { int i = m_bubbleIdx++; m_bubbleTexts[i] = calcResult;
              m_log->moveCursor(QTextCursor::End);
              m_log->insertHtml(buildLocalBubble(calcResult, ms, i)); }
            m_input->clear();
            emit chatCompleted(task.left(40), m_log->toHtml());
            return;
        }
    }

    int count = 0;
    for (int i = 0; i < MAX_AGENTS; i++)
        if (m_cfgDlg->enabledChk(i)->isChecked()) count++;
    if (count == 0) { m_log->append("\xe2\x9a\xa0  Abilita almeno un agente."); return; }

    /* Aggiunge il contesto documento se allegato, poi lo svuota (usa-e-getta) */
    if (!m_docContext.isEmpty()) {
        task += "\n\n--- DOCUMENTO ALLEGATO ---\n" + m_docContext.left(8000);
        m_docContext.clear();
        if (m_input)
            m_input->setPlaceholderText(tr("Scrivi un task o una domanda..."));
    }
    m_taskOriginal  = _inject_random(_inject_math(_inject_science(task)));
    m_agentOutputs.clear();
    m_spawnedAgents = 0;
    m_currentAgent  = 0;
    m_maxShots      = m_cfgDlg->numAgents();
    m_toolIteration = 0;
    m_opMode       = OpMode::Pipeline;

    for (int i = 0; i < MAX_AGENTS; i++)
        m_cfgDlg->enabledChk(i)->setStyleSheet("");
    emit pipelineStatus(0, "Avvio pipeline...");

    /* NON si cancella il log: le Q&A si accumulano nella stessa chat */
    /* Bolla utente — mostra il testo originale (senza prefissi inject_science/math) */
    { int i = m_bubbleIdx++; m_bubbleTexts[i] = task;
      m_log->moveCursor(QTextCursor::End);
      m_log->insertHtml(buildUserBubble(task, i)); }
    m_log->append("");

    /* ── Controllo RAM e dimensione modello pre-pipeline ── */
    if (!checkRam()) return;
    if (!checkModelSize(m_ai->model())) return;

    _setRunBusy(true);
    m_waitLbl->setVisible(true);
    m_input->clear();

    advancePipeline();
}

void AgentiPage::advancePipeline() {
    while (m_currentAgent < MAX_AGENTS && !m_cfgDlg->enabledChk(m_currentAgent)->isChecked())
        m_currentAgent++;

    if (m_currentAgent >= MAX_AGENTS || m_currentAgent >= m_maxShots) {
        /* Rilevamento formula → mostra grafico se presente */
        if (!m_agentOutputs.isEmpty())
            tryShowChart(m_taskOriginal + "\n" + m_agentOutputs.last());

        /* P5 — Estrattore nascosto: aggiorna user_knowledge.md a fine risposta.
           - Pipeline multi-agente: si attiva se ≥2 agenti hanno prodotto output.
           - CHAT RAG singolo: si attiva ogni kChatExtractEvery scambi (default 4). */
        {
            QSettings s("Prismalux", "GUI");
            const bool injectOn = s.value(P::SK::kInjectUserKnowledge, true).toBool();
            const int  filled   = std::count_if(m_agentOutputs.begin(), m_agentOutputs.end(),
                [](const QString& o){ return !o.trimmed().isEmpty(); });

            /* Contatore scambi singolo (incrementato prima della verifica soglia) */
            if (!m_modePipeline && filled == 1)
                m_singleChatTurns++;

            const bool doPipeline = (filled >= 2);
            const bool doChat     = (!m_modePipeline && filled == 1
                                     && m_singleChatTurns >= kChatExtractEvery);

            if (injectOn && (doPipeline || doChat)) {
                if (doChat) m_singleChatTurns = 0; /* reset dopo ogni estrazione */
                runKnowledgeExtract();   /* cambia opMode → KnowledgeExtract */
                return;                  /* onFinished gestirà la chiusura */
            }
        }

        /* ── Ricerca online fallback: suggerisci se LLM esprime incertezza ── */
        if (!m_agentOutputs.isEmpty()) {
            const QString& lastOut = m_agentOutputs.last();
            const QString lo = lastOut.toLower();
            static const QStringList kUncertainPhrases = {
                "non so ", "non ho informazioni", "non posso rispondere",
                "non sono sicuro", "non ho conoscenza", "non ho dati",
                "la mia conoscenza", "il mio limite", "non sono in grado",
                "non conosco", "non ho dettagli", "non trovo informazioni",
                "potrebbe essere cambiato", "non ho aggiornamenti",
                "non ho accesso a", "informazioni aggiornate"
            };
            bool uncertain = false;
            for (const QString& ph : kUncertainPhrases)
                if (lo.contains(ph)) { uncertain = true; break; }

            if (uncertain && !m_taskOriginal.isEmpty()) {
                const QString q64 = m_taskOriginal.toUtf8()
                    .toBase64(QByteArray::Base64UrlEncoding);
                const QString searchHint =
                    "<p style='margin:6px 0 2px 0;"
                    "background:#1e293b;border-radius:6px;"
                    "border-left:3px solid #3b82f6;"
                    "padding:6px 10px;'>"
                    "<span style='color:#94a3b8;font-size:11px;'>"
                    "\xf0\x9f\x94\x8d  Il modello non conosce la risposta. "
                    "</span>"
                    "<a href='websearch:" + q64 + "' "
                    "style='color:#60a5fa;font-size:11px;text-decoration:none;'>"
                    "Cerca online e salva nel RAG &rarr;</a></p>";
                m_log->moveCursor(QTextCursor::End);
                m_log->insertHtml(searchHint);
            }
        }

        /* Conversazione vocale continua: auto-TTS risposta singolo agente */
        if (m_voiceLoopActive && !m_modePipeline && !m_agentOutputs.isEmpty()) {
            QString resp = m_agentOutputs.last().trimmed();
            QStringList words = resp.split(' ', Qt::SkipEmptyParts);
            if (words.size() > 400) words = words.mid(0, 400);
            const QString ttsText = words.join(" ");
            if (!ttsText.isEmpty())
                QTimer::singleShot(200, this, [this, ttsText]{ _ttsPlay(ttsText); });
        }

        emit pipelineStatus(100, "\xe2\x9c\x85  Lavoro completato");
        _setRunBusy(false);
        m_opMode = OpMode::Idle;

        /* Hermes: memorizza la conversazione al termine */
        if (m_hermesEnabled && !m_taskOriginal.isEmpty() && !m_agentOutputs.isEmpty())
            hermesStoreConversation(m_taskOriginal, m_agentOutputs.last());

        emit chatCompleted(m_taskOriginal.left(40), m_log->toHtml());
        return;
    }

    /* Check RAM inter-agente: non blocca con dialog, interrompe silenziosamente */
    if (m_currentAgent > 0) {
        QFile minfo("/proc/meminfo");
        if (minfo.open(QIODevice::ReadOnly)) {
            long long total_kb = 0, avail_kb = 0;
            while (!minfo.atEnd()) {
                const QByteArray line = minfo.readLine().trimmed();
                if (line.startsWith("MemTotal:"))
                    total_kb = line.split(' ').last().trimmed().toLongLong();
                else if (line.startsWith("MemAvailable:"))
                    avail_kb = line.split(' ').last().trimmed().toLongLong();
            }
            minfo.close();
            const double ramPct = (total_kb > 0)
                ? 100.0 * (total_kb - avail_kb) / total_kb : 0.0;
            if (ramPct >= 92.0) {
                m_log->append(QString(
                    "\xe2\x9a\xa0  <b>RAM al %1%</b> \xe2\x80\x94 pipeline interrotta "
                    "per proteggere il sistema. Chiudi altre applicazioni o scarica il modello.")
                    .arg(ramPct, 0, 'f', 0));
                emit pipelineStatus(-1, "");
                return;
            }
        }
    }

    int total = 0, done = 0;
    for (int i = 0; i < MAX_AGENTS; i++)
        if (m_cfgDlg->enabledChk(i)->isChecked()) total++;
    for (int i = 0; i < m_currentAgent; i++)
        if (m_cfgDlg->enabledChk(i)->isChecked()) done++;
    int pct = (total > 0) ? (done * 100 / total) : 0;

    auto roleList = AgentsConfigDialog::roles();
    int roleIdx = m_cfgDlg->roleCombo(m_currentAgent)->currentIndex();
    if (roleIdx < 0 || roleIdx >= roleList.size()) roleIdx = 0;
    QString roleName = roleList[roleIdx].icon + " " + roleList[roleIdx].name;

    m_tokenCount = 0;
    emit pipelineStatus(pct, QString("\xe2\x9a\x99\xef\xb8\x8f  Agente %1/%2 \xe2\x80\x94 %3...")
        .arg(done + 1).arg(total).arg(roleName));

    runAgent(m_currentAgent);
}

void AgentiPage::runAgent(int idx) {
    auto roleList = AgentsConfigDialog::roles();
    int  roleIdx  = m_cfgDlg->roleCombo(idx)->currentIndex();
    if (roleIdx < 0 || roleIdx >= roleList.size()) roleIdx = 0;
    const auto& role = roleList[roleIdx];

    /* Modalità singola (CHAT RAG, m_maxShots==1): usa sempre il combo LLM principale.
       In pipeline multi-agente usa il modello assegnato nel dialog Configura Agenti. */
    QString selectedModel;
    if (m_maxShots == 1 && m_cmbLLM) {
        selectedModel = ModelComboHelper::currentModel(m_cmbLLM);
    } else {
        selectedModel = m_cfgDlg->modelCombo(idx)->currentData().toString();
        if (selectedModel.isEmpty()) selectedModel = m_cfgDlg->modelCombo(idx)->currentText();
    }

    /* Sicurezza: se il modello è di embedding (non-chat) o non valido,
       ricade sul modello selezionato nel combo LLM principale. */
    if (selectedModel.isEmpty()
        || selectedModel == "(nessun modello)"
        || _isEmbeddingModel(selectedModel))
    {
        const QString fallback = ModelComboHelper::currentModel(m_cmbLLM);
        if (!fallback.isEmpty() && !_isEmbeddingModel(fallback))
            selectedModel = fallback;
    }

    if (!selectedModel.isEmpty()
        && selectedModel != "(nessun modello)"
        && !_isEmbeddingModel(selectedModel))
    {
        m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), selectedModel);
    }

    QString ts = QTime::currentTime().toString("HH:mm:ss");

    /* Salva posizione PRIMA dell'header: l'intera bolla (header+contenuto) sarà sostituita su finish */
    {
        QTextCursor c(m_log->document());
        c.movePosition(QTextCursor::End);
        m_agentBlockStart = c.position();
    }

    /* Metadati per la bolla finale */
    const bool isSingleChat = (m_maxShots == 1);
    const bool isTeamMode   = isSingleChat && m_btnTeam && m_btnTeam->isChecked();
    m_currentAgentLabel = isTeamMode
        ? "\xf0\x9f\x91\xa5  Team di agenti"
        : (isSingleChat
            ? "\xf0\x9f\x93\xa4  Invia"
            : role.icon + QString("  Agente %1 \xe2\x80\x94 %2").arg(idx + 1).arg(role.name));
    m_currentAgentModel = selectedModel;
    m_currentAgentTime  = ts;

    /* Indicatore streaming temporaneo (sarà sostituito dalla bolla su onFinished) */
    if (isSingleChat) {
        const QString backendTag = (m_ai->backend() == AiClient::Ollama)
            ? "Ollama" : "llama-server";
        const QString modelTag   = selectedModel.isEmpty() ? "?" : selectedModel;
        m_log->append(QString("\n\xf0\x9f\x93\xa4  [Invia]  \xf0\x9f\xa4\x96 %1 \xc2\xb7 %2  \xf0\x9f\x94\x84 generando...\n")
                      .arg(backendTag, modelTag));
    } else
        m_log->append(QString("\n%1  [Agente %2 \xe2\x80\x94 %3]  \xf0\x9f\x94\x84 generando...\n")
                      .arg(role.icon).arg(idx + 1).arg(role.name));
    m_agentTextStart = m_log->document()->characterCount() - 1;

    QString userPrompt;

    /* In modalità pipeline multi-agente, ogni agente vede l'obiettivo comune esplicito */
    if (!isSingleChat) {
        userPrompt  = "\xf0\x9f\x8e\xaf OBIETTIVO DEL TEAM: ";
        userPrompt += m_taskOriginal;
        userPrompt += "\n\n";
    } else {
        userPrompt = m_taskOriginal;
    }

    /* RAG inline (dal tab principale) + RAG condiviso (da AgentsConfigDialog) */
    if (m_ragInline && m_ragInline->hasContext())
        userPrompt += _sanitize_prompt(m_ragInline->ragContext());
    if (m_cfgDlg->sharedRagWidget() && m_cfgDlg->sharedRagWidget()->hasContext())
        userPrompt += _sanitize_prompt(m_cfgDlg->sharedRagWidget()->ragContext());
    /* RAG specifico per questo agente */
    if (m_cfgDlg->ragWidget(idx) && m_cfgDlg->ragWidget(idx)->hasContext())
        userPrompt += _sanitize_prompt(m_cfgDlg->ragWidget(idx)->ragContext());
    if (!m_agentOutputs.isEmpty()) {
        userPrompt += "\n\n\xe2\x80\x94\xe2\x80\x94 Output agenti precedenti \xe2\x80\x94\xe2\x80\x94\n";
        for (int i = 0; i < m_agentOutputs.size(); i++) {
            int prevRole = m_cfgDlg->roleCombo(i)->currentIndex();
            QString prevName = (prevRole >= 0 && prevRole < roleList.size())
                               ? roleList[prevRole].name : "Agente";
            userPrompt += QString("\n[Agente %1 \xe2\x80\x94 %2]:\n%3\n")
                          .arg(i + 1).arg(prevName).arg(m_agentOutputs[i]);
        }
        userPrompt += "\n" + QString(16, QChar(0x2500)) + "\n";
        userPrompt += QString("Ora esegui il tuo ruolo di %1.").arg(role.name);
    } else if (!isSingleChat) {
        /* Primo agente della pipeline: nessun output precedente, esplicita il ruolo */
        userPrompt += QString("Il tuo ruolo in questo team: %1.").arg(role.name);
    }

    /* In pipeline multi-agente arricchisce il system prompt con l'obiettivo del team:
       ogni agente conosce il task concreto nel suo contesto "permanente" (system). */
    const QString teamGoalFull  = isSingleChat ? QString()
        : QString("\n\n\xf0\x9f\x8e\xaf Obiettivo globale del team: ") + m_taskOriginal.left(200);
    const QString teamGoalSmall = isSingleChat ? QString()
        : QString(" Task: ") + m_taskOriginal.left(80);
    const QString toolSuffix = (m_toolsEnabled && isSingleChat) ? toolSystemSuffix() : QString();

    /* Team di agenti: sovrascrive il system prompt con istruzioni multi-ruolo */
    static const QString kTeamSys =
        "Sei un team di esperti multidisciplinari. Rispondi strutturando la risposta "
        "in sezioni, ognuna scritta da uno specialista diverso tra quelli pertinenti: "
        "[Ricercatore], [Analista], [Consulente], [Critico]. "
        "Usa solo le sezioni utili per la domanda. "
        "Termina sempre con [Sintesi] concisa. Rispondi in italiano.";
    QString sysFullMut  = isTeamMode ? kTeamSys + toolSuffix
                                     : role.sysPrompt      + teamGoalFull  + toolSuffix;
    QString sysSmallMut = isTeamMode ? kTeamSys + toolSuffix
                                     : role.sysPromptSmall + teamGoalSmall + toolSuffix;

    /* Hermes: inietta memoria da sessioni precedenti (solo chat singola) */
    if (m_hermesEnabled && isSingleChat && !m_taskOriginal.isEmpty())
        hermesInjectContext(sysFullMut, m_taskOriginal);

    const QString sysFull  = sysFullMut;
    const QString sysSmall = sysSmallMut;

    m_agentTimer.restart();
    m_agentOutputs.append("");

    /* Tool use nativo Ollama: attiva solo in CHAT singola con tool abilitati
       e backend Ollama (llama-server non supporta tool_calls nel formato Ollama). */
    if (m_toolsEnabled && isSingleChat && m_ai->backend() == AiClient::Ollama)
        m_ai->setActiveTools(buildEnabledTools());
    else
        m_ai->clearActiveTools();

    /* Usa chatWithImage per il primo agente se c'è un'immagine allegata */
    if (idx == m_currentAgent && !m_imgBase64.isEmpty()) {
        m_ai->chatWithImage(_buildSys(m_taskOriginal, sysFull, sysSmall, m_ai->model(), m_ai->backend()), userPrompt,
                            m_imgBase64, m_imgMime);
    } else {
        m_ai->chat(_buildSys(m_taskOriginal, sysFull, sysSmall, m_ai->model(), m_ai->backend()), userPrompt);
    }
}

/* ══════════════════════════════════════════════════════════════
   _finishedPipelineControl — controller LLM completato
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::_finishedPipelineControl() {
    m_opMode = OpMode::Pipeline;

    QString ctrlHtml = markdownToHtml(m_ctrlAccum.trimmed());
    QTextCursor selCtrl(m_log->document());
    selCtrl.setPosition(m_ctrlBlockStart);
    selCtrl.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    selCtrl.removeSelectedText();
    selCtrl.insertHtml(buildControllerBubble(ctrlHtml));

    advancePipeline();
}

/* ══════════════════════════════════════════════════════════════
   _finishedPipeline — risposta agente pipeline completata
   ══════════════════════════════════════════════════════════════ */
/* ══════════════════════════════════════════════════════════════
   _finishedPipeline — risposta agente pipeline completata
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::_finishedPipeline(const QString& full) {
    if (m_currentAgent < MAX_AGENTS)
        m_cfgDlg->enabledChk(m_currentAgent)->setStyleSheet(
            "QCheckBox { color: #4caf50; font-weight: bold; }"
            "QCheckBox::indicator:checked { background-color: #4caf50; border: 2px solid #388e3c; border-radius: 3px; }");

    m_currentAgent++;

    int total = 0, done = 0;
    for (int i = 0; i < MAX_AGENTS; i++) if (m_cfgDlg->enabledChk(i)->isChecked()) total++;
    for (int i = 0; i < m_currentAgent; i++) if (m_cfgDlg->enabledChk(i)->isChecked()) done++;
    int pct = (total > 0) ? qMin(done * 100 / total, 99) : 0;
    emit pipelineStatus(pct, QString("\xe2\x9c\x85  Agente %1/%2 completato  (%3%)")
        .arg(done).arg(qMin(total, m_maxShots)).arg(pct));

    /* Sostituisce l'indicatore streaming + testo grezzo con la bolla AI completa */
    QString rawResp;
    if (m_currentAgent > 0 && !m_agentOutputs.isEmpty()) {
        rawResp = m_agentOutputs[m_currentAgent - 1];
        /* Fallback: modelli thinking-only (qwen3, deepseek-r1) rispondono tramite
           message.thinking invece di message.content → nessun token emesso →
           m_agentOutputs resta vuota. AiClient avvolge il thinking in <think>...</think>
           e lo passa via finished(). Usiamo quel valore se il buffer locale è vuoto. */
        if (rawResp.isEmpty() && !full.isEmpty())
            rawResp = m_agentOutputs[m_currentAgent - 1] = full;
        /* Fix toggle ▶️: thinking via message.thinking (campo separato da content).
           AiClient prepone <think>...</think> solo in finished(), non nei token.
           rawResp (dai token) non ha il think block; full sì.
           Usiamo full per estrarre il thinking e mostrare il toggle. */
        else if (!rawResp.isEmpty() && !full.isEmpty()
                 && full.contains("<think>", Qt::CaseInsensitive)
                 && !rawResp.contains("<think>", Qt::CaseInsensitive)) {
            rawResp = m_agentOutputs[m_currentAgent - 1] = full;
        }
        /* Rimuove blocchi <think>...</think> (reasoning models: qwen3, deepseek-r1, qwq...)
           Se dopo lo strip rimane vuoto (modelli piccoli che producono SOLO thinking
           senza risposta finale), si usa il contenuto del <think> come fallback.
           Il contenuto del <think> viene salvato in m_thinkTexts per il toggle collassabile. */
        QString extractedThink;  /* contenuto <think> estratto — salvato per la bolla */
        {
            QRegularExpression reTh("<think>([\\s\\S]*?)</think>",
                                    QRegularExpression::CaseInsensitiveOption);
            /* Salva l'originale prima di rimuovere */
            const QString original = rawResp;
            auto thinkMatch = reTh.match(original);
            if (thinkMatch.hasMatch())
                extractedThink = thinkMatch.captured(1).trimmed();

            rawResp.remove(QRegularExpression("<think>[\\s\\S]*?</think>",
                QRegularExpression::CaseInsensitiveOption));
            rawResp = rawResp.trimmed();

            /* <think> senza </think>: budget ragionamento esaurito a metà.
               Tutto ciò che segue un <think> non chiuso è thinking non terminato. */
            rawResp.remove(QRegularExpression("<think>[\\s\\S]*$",
                QRegularExpression::CaseInsensitiveOption));
            rawResp = rawResp.trimmed();

            /* Fallback: se il modello ha prodotto solo reasoning (o output vuoto),
             * mostra il contenuto del <think> se non vuoto.
             * Caso comune con modelli 0.8-1.5B: generano <think>...</think>
             * ma poi non aggiungono risposta finale. */
            if (rawResp.isEmpty()) {
                auto m = reTh.match(original);
                if (m.hasMatch()) {
                    /* Prova prima il contenuto trimmed, poi raw (per preservare newline) */
                    const QString thinkTrimmed = m.captured(1).trimmed();
                    rawResp = thinkTrimmed.isEmpty() ? m.captured(1) : thinkTrimmed;
                    if (rawResp.isEmpty()) {
                        /* Modello troppo piccolo: think vuoto + nessuna risposta */
                        rawResp = "[Il modello non ha prodotto risposta. "
                                  "Usa un modello più grande (≥3B) o premi "
                                  "Singolo per domande semplici.]";
                    }
                    /* Non aggiornare m_agentOutputs con il think: passiamo ai prossimi
                       agenti un placeholder chiaro invece di contenuto di ragionamento */
                    m_agentOutputs[m_currentAgent - 1] =
                        "[Modello ha prodotto solo ragionamento interno — nessuna risposta finale]";
                    extractedThink = "";  /* il think diventa la risposta: non serve toggle */
                } else {
                    /* Nessun tag <think> e risposta vuota — modello non ha risposto */
                    rawResp = "[Nessuna risposta dal modello. "
                              "Verifica che Ollama sia avviato e il modello selezionato "
                              "sia disponibile.]";
                    m_agentOutputs[m_currentAgent - 1] = rawResp;
                }
            } else {
                m_agentOutputs[m_currentAgent - 1] = rawResp;
            }
        }
        /* Strip ragionamento italiano inline: i modelli spesso iniziano con frasi-spia
           tipo "L'utente chiede...", "Devo rispondere...", "Sto analizzando..." prima
           della risposta vera. Rimuove le righe iniziali che corrispondono ai pattern. */
        {
            static const QRegularExpression reThinkLine(
                QString::fromUtf8(
                    "^(l['\xE2\x80\x99]utente\\s+(chiede|vuole|ha\\s+chiesto|sta|intende|sembra|desidera|ha\\s+fatto)|"
                    "devo\\s+rispondere|voglio\\s+rispondere|prima\\s+di\\s+rispondere|"
                    "sto\\s+(pensando|analizzando|considerando|riflettendo|cercando)|"
                    "analizziamo|penso\\s+di\\s+dover|capisco\\s+che|vediamo\\s+(cosa|come)|"
                    "mi\\s+viene\\s+chiesto|ho\\s+capito\\s+che|la\\s+domanda\\s+\\xC3\\xA8)"),
                QRegularExpression::CaseInsensitiveOption);
            QStringList lines = rawResp.split('\n');
            int firstReal = 0;
            for (int i = 0; i < lines.size(); i++) {
                const QString trimmed = lines[i].trimmed();
                if (trimmed.isEmpty() || reThinkLine.match(trimmed).hasMatch())
                    firstReal = i + 1;
                else
                    break;
            }
            if (firstReal > 0 && firstReal < lines.size()) {
                const QString stripped = QStringList(lines.mid(firstReal)).join('\n').trimmed();
                if (!stripped.isEmpty()) {
                    rawResp = stripped;
                    m_agentOutputs[m_currentAgent - 1] = rawResp;
                }
            }
        }

        /* ── Tool Use Nativo: intercetta TOOL_CALL prima di costruire la bolla ── */
        if (m_toolsEnabled && m_maxShots == 1 && m_toolIteration < 2 && !rawResp.isEmpty()) {
            const QJsonObject tc = detectFirstToolCall(rawResp);
            if (!tc.isEmpty()) {
                m_toolIteration++;
                const int agentIdx = m_currentAgent - 1;

                /* Rimuove testo grezzo dello streaming */
                QTextCursor selTool(m_log->document());
                selTool.setPosition(m_agentBlockStart);
                selTool.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
                selTool.removeSelectedText();

                /* Mostra indicatore compatto "tool in esecuzione" */
                const QString tn  = tc["tool"].toString().toHtmlEscaped();
                const QString tin = tc["input"].toString().left(120).toHtmlEscaped();
                m_log->moveCursor(QTextCursor::End);
                m_log->insertHtml(
                    "<p style='color:#94a3b8;font-size:11px;margin:4px 0;'>"
                    "\xf0\x9f\x94\xa7&nbsp;<b>Tool:</b>&nbsp;" + tn +
                    "&nbsp;\xe2\x80\x94&nbsp;<code>" + tin +
                    "</code>&nbsp;&nbsp;\xe2\x8f\xb3 in esecuzione...</p>");

                /* Rimuove l'ultima voce di m_agentOutputs: runAgent la re-appenderà */
                if (!m_agentOutputs.isEmpty())
                    m_agentOutputs.removeLast();

                runToolCall(tc, [this, agentIdx, tc](const QString& result) {
                    /* Aggiorna il log con il risultato del tool */
                    m_log->moveCursor(QTextCursor::End);
                    m_log->insertHtml(
                        "<p style='color:#86efac;font-size:11px;margin:4px 0;'>"
                        "\xe2\x9c\x85&nbsp;<b>Risultato:</b>&nbsp;"
                        + result.left(300).toHtmlEscaped() + "</p>");

                    /* Inietta il risultato nel contesto del task */
                    m_taskOriginal += QString(
                        "\n\n[TOOL_RESULT: %1]\n%2\n\n"
                        "Rispondi ora all'utente in italiano.")
                        .arg(tc["tool"].toString(), result);

                    /* Re-run dello stesso agente con il contesto arricchito */
                    m_currentAgent = agentIdx;
                    runAgent(agentIdx);
                });
                return;
            }
        }

        /* Aggiunge il tempo di risposta all'header della bolla */
        {
            const double elapsedMs = static_cast<double>(m_agentTimer.elapsed());
            const QString elapsedStr = elapsedMs < 1000.0
                ? QString::number(qRound(elapsedMs)) + " ms"
                : QString::number(elapsedMs / 1000.0, 'f', 1) + " s";
            m_currentAgentTime += "  \xc2\xb7\xc2\xb7  " + elapsedStr;
        }

        QString htmlContent = rawResp.isEmpty()
            ? "<p style='color:#6b7280;font-style:italic;margin:0;'>Nessun output.</p>"
            : markdownToHtml(rawResp);

        QTextCursor sel(m_log->document());
        sel.setPosition(m_agentBlockStart);
        sel.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
        sel.removeSelectedText();
        { int idx = m_bubbleIdx++; m_bubbleTexts[idx] = rawResp;
          if (!extractedThink.isEmpty()) m_thinkTexts[idx] = extractedThink;
          sel.insertHtml(buildAgentBubble(m_currentAgentLabel,
                                         m_currentAgentModel,
                                         m_currentAgentTime,
                                         htmlContent, idx,
                                         extractedThink)); }
    }

    /* ── Tool Executor: estrae ed esegue codice Python, poi avvia il Controller ── */
    QString pyCode = extractPythonCode(rawResp);
    if (!pyCode.isEmpty()) {
        pyCode = _sanitizePyCode(pyCode);
        const bool useSandbox = P::isSandboxReady();

        /* [C1] Dialog conferma — testo e colore variano in base alla sandbox */
        {
            auto* dlg = new QDialog(this);
            dlg->setWindowTitle(useSandbox
                ? "\xf0\x9f\x90\xb3  Esegui codice in sandbox Docker?"
                : "\xe2\x9a\xa0  Esegui codice generato dall\xe2\x80\x99" "AI?");
            dlg->setMinimumSize(660, 460);
            auto* lay = new QVBoxLayout(dlg);

            auto* warnLbl = new QLabel(useSandbox
                ? "\xf0\x9f\x90\xb3  Il codice verr\xc3\xa0 eseguito in un container Docker isolato.\n"
                  "Nessun accesso a file locali, rete disabilitata, max 256\xc2\xa0MB RAM.\n"
                  "Verifica il codice, poi clicca Esegui."
                : "\xe2\x9a\xa0  Stai per eseguire codice Python generato dall\xe2\x80\x99"
                  "AI con i tuoi permessi utente.\n"
                  "Verifica che non faccia operazioni indesiderate prima di procedere.",
                dlg);
            warnLbl->setWordWrap(true);
            warnLbl->setStyleSheet(useSandbox
                ? "color:#86efac;font-weight:bold;padding:6px;"
                  "background:#052e16;border-radius:4px;"
                : "color:#facc15;font-weight:bold;padding:6px;"
                  "background:#292524;border-radius:4px;");
            lay->addWidget(warnLbl);

            auto* codeView = new QTextEdit(dlg);
            codeView->setReadOnly(true);
            codeView->setPlainText(pyCode);
            codeView->setFont(QFont("JetBrains Mono,Fira Code,Consolas,monospace", 10));
            codeView->setStyleSheet("background:#1e1e2e;color:#cdd6f4;"
                                    "border:1px solid #45475a;padding:4px;");
            lay->addWidget(codeView, 1);

            auto* btnBox = new QDialogButtonBox(dlg);
            auto* btnRun = btnBox->addButton(
                "\xe2\x96\xb6  Esegui", QDialogButtonBox::AcceptRole);
            btnBox->addButton("\xe2\x9c\x96  Annulla", QDialogButtonBox::RejectRole);
            btnRun->setStyleSheet(useSandbox
                ? "background:#16a34a;color:#fff;font-weight:bold;padding:4px 18px;"
                : "background:#ef4444;color:#fff;font-weight:bold;padding:4px 18px;");
            connect(btnBox, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
            connect(btnBox, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
            lay->addWidget(btnBox);

            const bool accepted = (dlg->exec() == QDialog::Accepted);
            dlg->deleteLater();
            if (!accepted) { advancePipeline(); return; }
        }

        m_executorOutput.clear();
        if (m_execProc) { m_execProc->kill(); m_execProc->deleteLater(); m_execProc = nullptr; }
        m_execProc = new QProcess(this);
        m_execProc->setProcessChannelMode(QProcess::MergedChannels);
        auto tmr = QSharedPointer<QElapsedTimer>::create();
        tmr->start();

        if (useSandbox) {
            /* ── Sandbox Docker: stdin piping, rete/filesystem isolati ─────── */
            const QSettings ss("Prismalux", "GUI");
            const QString img = ss.value(P::SK::kSandboxImage,   "python:3.11-slim").toString();
            const QString mem = QString::number(
                ss.value(P::SK::kSandboxMemory, 256).toInt()) + "m";

            connect(m_execProc,
                    QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                    this, [this, tmr](int exitCode, QProcess::ExitStatus) {
                const double ms = tmr->elapsed();
                const QString out = QString::fromUtf8(m_execProc->readAll());
                m_execProc->deleteLater();
                m_execProc = nullptr;
                m_executorOutput = out;
                const QString outDisplay = PrismaluxPaths::sanitizeErrorOutput(out);
                QTextCursor c(m_log->document());
                c.movePosition(QTextCursor::End);
                c.insertHtml(buildToolStrip(QString(), outDisplay, exitCode, ms));
                if (!m_userScrolled) {
                    m_suppressScrollSig = true;
                    m_log->ensureCursorVisible();
                    m_suppressScrollSig = false;
                }
                if (m_cfgDlg->controllerEnabled()) runPipelineController();
                else advancePipeline();
            });

            connect(m_execProc, &QProcess::errorOccurred,
                    this, [this](QProcess::ProcessError err){
                if (err == QProcess::FailedToStart) {
                    m_execProc->deleteLater(); m_execProc = nullptr;
                    advancePipeline();
                }
            });

            m_execProc->start(P::findDocker(), {
                "run", "--rm", "--network", "none",
                "--memory", mem, "--cpus", "0.5",
                "--pids-limit", "64", "-i",
                img, "python3", "-"
            });
            if (m_execProc->waitForStarted(5000)) {
                m_execProc->write(pyCode.toUtf8());
                m_execProc->closeWriteChannel();
            }
            QTimer::singleShot(30000, this, [this]{
                if (m_execProc && m_execProc->state() != QProcess::NotRunning) {
                    m_execProc->kill();
                    m_executorOutput = "[timeout sandbox 30s]";
                    if (m_cfgDlg->controllerEnabled()) runPipelineController();
                    else advancePipeline();
                }
            });

        } else {
            /* ── Python locale: file temporaneo + pip install retry ─────────
               [B1] QSharedPointer evita memory leak se il processo viene
               distrutto prima che finished() scatti. */
            QTemporaryFile execTmp(
                PrismaluxPaths::safeTempPath() + "/prisma_exec_XXXXXX.py");
            execTmp.setAutoRemove(false);
            if (!execTmp.open()) { advancePipeline(); return; }
            execTmp.write(pyCode.toUtf8());
            execTmp.close();
            const QString tmpPath = execTmp.fileName();

            connect(m_execProc,
                    QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                    this, [this, tmpPath, tmr](int exitCode, QProcess::ExitStatus) {
                const double ms = tmr->elapsed();
                QString out = QString::fromUtf8(m_execProc->readAll());
                m_execProc->deleteLater();
                m_execProc = nullptr;

                /* [C2] Auto-install modulo mancante con conferma utente */
                static QRegularExpression reModule(
                    "ModuleNotFoundError: No module named '([^']+)'");
                auto mMatch = reModule.match(out);
                if (exitCode != 0 && mMatch.hasMatch()) {
                    const QString pkg = mMatch.captured(1).split('.').first();
                    const int ans = QMessageBox::warning(this,
                        "\xe2\x9a\xa0  Installa pacchetto Python?",
                        QString("Il codice richiede il pacchetto <b>%1</b> non installato."
                                "<br><br>"
                                "\xe2\x9a\xa0  <b>Attenzione</b>: il nome viene da un"
                                " suggerimento dell\xe2\x80\x99" "AI.<br>"
                                "Verifica che <code>%1</code> sia il pacchetto corretto"
                                " su pypi.org prima di procedere.<br><br>"
                                "Eseguire <code>pip install %1</code>?").arg(pkg),
                        QMessageBox::Yes | QMessageBox::No,
                        QMessageBox::No);
                    if (ans != QMessageBox::Yes) {
                        QFile::remove(tmpPath);
                        QTextCursor logC(m_log->document());
                        logC.movePosition(QTextCursor::End);
                        logC.insertHtml(QString(
                            "<div style='color:#f87171;margin:4px 0'>"
                            "\xe2\x9d\x8c  Installazione di \xe2\x80\x98%1\xe2\x80\x99"
                            " annullata.</div>").arg(pkg));
                        m_executorOutput = out;
                        if (m_cfgDlg->controllerEnabled()) runPipelineController();
                        else advancePipeline();
                        return;
                    }
                    QTextCursor logC(m_log->document());
                    logC.movePosition(QTextCursor::End);
                    logC.insertHtml(QString(
                        "<div style='color:#facc15;font-style:italic;margin:4px 0'>"
                        "\xf0\x9f\x93\xa6  Installo '%1' via pip...</div>").arg(pkg));
                    if (!m_userScrolled) m_log->ensureCursorVisible();
                    auto* pip = new QProcess(this);
                    pip->setProcessChannelMode(QProcess::MergedChannels);
                    connect(pip, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                            this, [this, pip, tmpPath, pkg](int rc, QProcess::ExitStatus) {
                        const QString pipOut = QString::fromUtf8(pip->readAll()).trimmed();
                        pip->deleteLater();
                        QTextCursor logC2(m_log->document());
                        logC2.movePosition(QTextCursor::End);
                        if (rc == 0) {
                            logC2.insertHtml(QString(
                                "<div style='color:#4ade80;margin:4px 0'>"
                                "\xe2\x9c\x85  '%1' installato. Riprovo...</div>").arg(pkg));
                            if (!m_userScrolled) m_log->ensureCursorVisible();
                            auto* retry = new QProcess(this);
                            retry->setProcessChannelMode(QProcess::MergedChannels);
                            auto t2 = QSharedPointer<QElapsedTimer>::create();
                            t2->start();
                            connect(retry, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                                    this, [this, retry, tmpPath, t2](int rc2, QProcess::ExitStatus) {
                                const double ms2 = t2->elapsed();
                                QString out2 = QString::fromUtf8(retry->readAll());
                                retry->deleteLater();
                                QFile::remove(tmpPath);
                                m_executorOutput = out2;
                                QTextCursor c2(m_log->document());
                                c2.movePosition(QTextCursor::End);
                                c2.insertHtml(buildToolStrip(QString(),
                                    PrismaluxPaths::sanitizeErrorOutput(out2), rc2, ms2));
                                if (!m_userScrolled) m_log->ensureCursorVisible();
                                if (m_cfgDlg->controllerEnabled()) runPipelineController();
                                else advancePipeline();
                            });
                            retry->start(PrismaluxPaths::findPython(), {tmpPath});
                        } else {
                            logC2.insertHtml(QString(
                                "<div style='color:#f87171;margin:4px 0'>"
                                "\xe2\x9d\x8c  pip install '%1' fallito.<br>"
                                "<code>pip install %1</code></div>").arg(pkg));
                            LogBus::post(QString("\xe2\x9d\x8c AI Pipeline: pip install '%1' fallito.").arg(pkg));
                            QFile::remove(tmpPath);
                            m_executorOutput = pipOut;
                            if (m_cfgDlg->controllerEnabled()) runPipelineController();
                            else advancePipeline();
                        }
                    });
                    pip->start(PrismaluxPaths::findPython(), {"-m", "pip", "install", pkg,
                        "--quiet", "--break-system-packages",
                        "--trusted-host", "pypi.org",
                        "--trusted-host", "files.pythonhosted.org"});
                    return;
                }

                QFile::remove(tmpPath);
                m_executorOutput = out;
                const QString outDisplay = PrismaluxPaths::sanitizeErrorOutput(out);
                QTextCursor c(m_log->document());
                c.movePosition(QTextCursor::End);
                c.insertHtml(buildToolStrip(QString(), outDisplay, exitCode, ms));
                if (!m_userScrolled) {
                    m_suppressScrollSig = true;
                    m_log->ensureCursorVisible();
                    m_suppressScrollSig = false;
                }
                if (m_cfgDlg->controllerEnabled()) runPipelineController();
                else advancePipeline();
            });

            connect(m_execProc, &QProcess::errorOccurred,
                    this, [this](QProcess::ProcessError err){
                if (err == QProcess::FailedToStart) {
                    m_execProc->deleteLater(); m_execProc = nullptr;
                    advancePipeline();
                }
            });
            m_execProc->start(PrismaluxPaths::findPython(), {tmpPath});
        }

    } else {
        /* Nessun codice trovato: avanza direttamente */
        advancePipeline();
    }
}

/* ══════════════════════════════════════════════════════════════
   buildEnabledTools — array tools filtrato dalle checkbox del pannello.
   Restituisce solo i tool con nome presente in m_enabledTools.
   Se m_enabledTools è vuoto, restituisce array vuoto (tool use disattivato).
   ══════════════════════════════════════════════════════════════ */
QJsonArray AgentiPage::buildEnabledTools() const
{
    const QJsonArray all = _buildOllamaTools();
    if (m_enabledTools.isEmpty()) return QJsonArray{};

    QJsonArray out;
    for (const QJsonValue& v : all) {
        const QString name = v.toObject()["function"].toObject()["name"].toString();
        if (m_enabledTools.contains(name))
            out.append(v);
    }
    return out;
}

