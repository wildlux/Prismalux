#include "agenti_page.h"
#include "agenti_page_p.h"
#include "../prismalux_paths.h"
namespace P = PrismaluxPaths;
#include <QRegularExpression>
#include <QTextCursor>

/* ══════════════════════════════════════════════════════════════
   extractPythonCode — trova il primo blocco ```python...```
   ══════════════════════════════════════════════════════════════ */
QString AgentiPage::extractPythonCode(const QString& text)
{
    static QRegularExpression re(
        "```(?:python|py)?\\s*([\\s\\S]*?)```",
        QRegularExpression::CaseInsensitiveOption);
    auto m = re.match(text);
    return m.hasMatch() ? m.captured(1).trimmed() : QString();
}

/* ══════════════════════════════════════════════════════════════
   runPipelineController — Controller LLM dopo l'esecutore
   Valida task + output agente + output esecutore.
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::runPipelineController()
{
    if (m_agentOutputs.isEmpty()) { advancePipeline(); return; }

    m_opMode = OpMode::PipelineControl;
    m_ctrlAccum.clear();

    /* Segna posizione DOPO la tool strip (già inserita) per la bolla controller */
    {
        QTextCursor c(m_log->document());
        c.movePosition(QTextCursor::End);
        m_ctrlBlockStart = c.position();
    }

    /* System prompt: validator conciso */
    const QString sysPrompt =
        "Sei il Controller della pipeline Prismalux. "
        "Valida l'output dell'agente e l'esito del codice eseguito. "
        "Rispondi SOLO con una di queste righe:\n"
        "\xe2\x9c\x85 APPROVATO \xe2\x80\x94 [motivazione breve]\n"
        "\xe2\x9a\xa0\xef\xb8\x8f AVVISO \xe2\x80\x94 [problema non critico]\n"
        "\xe2\x9d\x8c ERRORE \xe2\x80\x94 [problema e correzione]\n"
        "Max 2 righe. Rispondi SOLO in italiano.";

    QString userMsg = QString(
        "Task: %1\n\n"
        "Output agente:\n%2\n\n"
        "Output esecutore (exit %3):\n%4")
        .arg(m_taskOriginal)
        .arg(m_agentOutputs.last())
        .arg(m_executorOutput.isEmpty() ? "N/A" : m_executorOutput)
        .arg(m_executorOutput.isEmpty() ? "(nessun output)" : m_executorOutput);

    m_ai->chat(sysPrompt, userMsg);
}


