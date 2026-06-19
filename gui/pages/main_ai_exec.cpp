#include "main_ai.h"
#include "main_ai_p.h"
#include "../prismalux_paths.h"
namespace P = PrismaluxPaths;
#include <QRegularExpression>
#include <QTextCursor>

/* ══════════════════════════════════════════════════════════════
   extractExecutableCode — trova il primo blocco eseguibile:
   python/py, c (non cpp), cpp/c++.
   Priorità: python > c/cpp (per evitare di compilare snippet da appendici).
   ══════════════════════════════════════════════════════════════ */
AgentiPage::ExecCode AgentiPage::extractExecutableCode(const QString& text)
{
    /* Python ha la precedenza */
    static QRegularExpression rePy(
        "```(?:python|py)\\s*\\n([\\s\\S]*?)```",
        QRegularExpression::CaseInsensitiveOption);
    auto mpy = rePy.match(text);
    if (mpy.hasMatch()) return { "python", mpy.captured(1).trimmed() };

    /* C++ prima di C (```c potrebbe matchare ```cpp) */
    static QRegularExpression reCpp(
        "```(?:cpp|c\\+\\+|cxx)\\s*\\n([\\s\\S]*?)```",
        QRegularExpression::CaseInsensitiveOption);
    auto mcpp = reCpp.match(text);
    if (mcpp.hasMatch()) return { "cpp", mcpp.captured(1).trimmed() };

    /* C puro — usa word boundary per non catturare ```cmake, ```css, ecc. */
    static QRegularExpression reC(
        "```c\\s*\\n([\\s\\S]*?)```",
        QRegularExpression::CaseInsensitiveOption);
    auto mc = reC.match(text);
    if (mc.hasMatch()) return { "c", mc.captured(1).trimmed() };

    return {};
}

/* ══════════════════════════════════════════════════════════════
   extractPythonCode — compatibilità con i test esistenti
   ══════════════════════════════════════════════════════════════ */
QString AgentiPage::extractPythonCode(const QString& text)
{
    static QRegularExpression re(
        "```(?:python|py)\\s*\\n([\\s\\S]*?)```",
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


