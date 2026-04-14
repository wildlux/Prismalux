#include "agenti_page.h"
#include "agenti_page_p.h"
#include "../prismalux_paths.h"
namespace P = PrismaluxPaths;
#include <QElapsedTimer>
#include <QMessageBox>
#include <QSettings>
#include <QTextCursor>
#include "../widgets/formula_parser.h"
#include "../widgets/chart_widget.h"
#include "agents_config_dialog.h"

/* ══════════════════════════════════════════════════════════════
   Pipeline sequenziale
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::runPipeline() {
    m_userScrolled = false;  /* nuovo task: torna in auto-scroll */
    QString task = _sanitize_prompt(m_input->toPlainText().trimmed());
    if (task.isEmpty()) { m_log->append("\xe2\x9a\xa0  Inserisci un task."); return; }

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
                    m_log->clear();
                    m_taskOriginal = task;
                    { int i = m_bubbleIdx++; m_bubbleTexts[i] = task;
                      m_log->moveCursor(QTextCursor::End);
                      m_log->insertHtml(buildUserBubble(task, i)); }
                    m_log->append("");
                    /* Bolla chart: solo pulsante "Mostra grafico" senza testo formula */
                    {
                        const auto& _c = bc();
                        const int br = QSettings("Prismalux","GUI").value(P::SK::kBubbleRadius,10).toInt();
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

    int count = 0;
    for (int i = 0; i < MAX_AGENTS; i++)
        if (m_cfgDlg->enabledChk(i)->isChecked()) count++;
    if (count == 0) { m_log->append("\xe2\x9a\xa0  Abilita almeno un agente."); return; }

    /* Aggiunge il contesto documento se allegato */
    if (!m_docContext.isEmpty()) {
        task += "\n\n--- DOCUMENTO ALLEGATO ---\n" + m_docContext.left(8000);
    }
    m_taskOriginal = _inject_math(task);
    m_agentOutputs.clear();
    m_currentAgent = 0;
    m_maxShots     = m_cfgDlg->numAgents();
    m_opMode       = OpMode::Pipeline;

    for (int i = 0; i < MAX_AGENTS; i++)
        m_cfgDlg->enabledChk(i)->setStyleSheet("");
    emit pipelineStatus(0, "Avvio pipeline...");

    /* NON si cancella il log: le Q&A si accumulano nella stessa chat */
    /* Bolla utente — mostra il task come messaggio "Tu" */
    { int i = m_bubbleIdx++; m_bubbleTexts[i] = m_taskOriginal;
      m_log->moveCursor(QTextCursor::End);
      m_log->insertHtml(buildUserBubble(m_taskOriginal, i)); }
    m_log->append("");

    /* ── Controllo RAM e dimensione modello pre-pipeline ── */
    if (!checkRam()) return;
    if (!checkModelSize(m_ai->model())) return;

    m_btnRun->setEnabled(false);
    m_btnStop->setEnabled(true);
    m_waitLbl->setVisible(true);

    advancePipeline();
}

void AgentiPage::advancePipeline() {
    while (m_currentAgent < MAX_AGENTS && !m_cfgDlg->enabledChk(m_currentAgent)->isChecked())
        m_currentAgent++;

    if (m_currentAgent >= MAX_AGENTS || m_currentAgent >= m_maxShots) {
        m_log->append("\n\xe2\x9c\x85  Pipeline completata. \xe2\x9c\xa8 Verit\xc3\xa0 rivelata.");
        emit pipelineStatus(100, "\xe2\x9c\x85  Completata!");
        m_btnRun->setEnabled(true);
        m_btnStop->setEnabled(false);
        m_opMode = OpMode::Idle;
        /* Rilevamento formula → mostra grafico se presente */
        if (!m_agentOutputs.isEmpty())
            tryShowChart(m_taskOriginal + "\n" + m_agentOutputs.last());
        /* Salva storico chat */
        emit chatCompleted(m_taskOriginal.left(40), m_log->toHtml());
        return;
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

    QString selectedModel = m_cfgDlg->modelCombo(idx)->currentData().toString();
    if (selectedModel.isEmpty()) selectedModel = m_cfgDlg->modelCombo(idx)->currentText();

    /* Sicurezza: se il modello è di embedding (non-chat) o non valido,
       ricade sul modello selezionato nel combo LLM principale. */
    if (selectedModel.isEmpty()
        || selectedModel == "(nessun modello)"
        || _isEmbeddingModel(selectedModel))
    {
        const QString fallback = m_cmbLLM ? m_cmbLLM->currentText() : QString();
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
    m_currentAgentLabel = role.icon + QString("  Agente %1 \xe2\x80\x94 %2").arg(idx + 1).arg(role.name);
    m_currentAgentModel = selectedModel;
    m_currentAgentTime  = ts;

    /* Indicatore streaming temporaneo (sarà sostituito dalla bolla su onFinished) */
    m_log->append(QString("\n%1  [Agente %2 \xe2\x80\x94 %3]  \xf0\x9f\x94\x84 generando...\n")
                  .arg(role.icon).arg(idx + 1).arg(role.name));
    m_agentTextStart = m_log->document()->characterCount() - 1;

    QString userPrompt = m_taskOriginal;

    if (m_cfgDlg->ragWidget(idx) && m_cfgDlg->ragWidget(idx)->hasContext())
        userPrompt += _sanitize_prompt(m_cfgDlg->ragWidget(idx)->ragContext());
    if (!m_agentOutputs.isEmpty()) {
        userPrompt += "\n\n\xe2\x80\x94\xe2\x80\x94 Output agenti precedenti \xe2\x80\x94\xe2\x80\x94\n";
        for (int i = 0; i < m_agentOutputs.size(); i++) {
            int prevRole = m_cfgDlg->roleCombo(i)->currentIndex();
            QString prevName = (prevRole >= 0 && prevRole < roleList.size())
                               ? roleList[prevRole].name : "Agente";
            userPrompt += QString("\n[Agente %1 — %2]:\n%3\n")
                          .arg(i + 1).arg(prevName).arg(m_agentOutputs[i]);
        }
        userPrompt += "\n" + QString(16, QChar(0x2500)) + "\n";
        userPrompt += QString("Ora esegui il tuo ruolo di %1.").arg(role.name);
    }

    m_agentOutputs.append("");
    /* Usa chatWithImage per il primo agente se c'è un'immagine allegata */
    if (idx == m_currentAgent && !m_imgBase64.isEmpty()) {
        m_ai->chatWithImage(_buildSys(m_taskOriginal, role.sysPrompt, role.sysPromptSmall, m_ai->model(), m_ai->backend()), userPrompt,
                            m_imgBase64, m_imgMime);
    } else {
        m_ai->chat(_buildSys(m_taskOriginal, role.sysPrompt, role.sysPromptSmall, m_ai->model(), m_ai->backend()), userPrompt);
    }
}

