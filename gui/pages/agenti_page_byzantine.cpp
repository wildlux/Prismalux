#include "agenti_page.h"
#include "agenti_page_p.h"
#include "../prismalux_paths.h"
namespace P = PrismaluxPaths;
#include <QElapsedTimer>

/* ══════════════════════════════════════════════════════════════
   Motore Byzantino
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::runByzantine() {
    m_userScrolled = false;  /* nuovo task: torna in auto-scroll */
    QString fact = _sanitize_prompt(m_input->toPlainText().trimmed());
    if (fact.isEmpty()) { m_log->append("\xe2\x9a\xa0  Inserisci una domanda."); return; }

    {
        QElapsedTimer tmr; tmr.start();
        QString ris = guardiaMath(fact);
        double ms = tmr.nsecsElapsed() / 1e6;
        if (!ris.isEmpty()) {
            m_log->clear();
            { int i = m_bubbleIdx++; m_bubbleTexts[i] = fact;
              m_log->moveCursor(QTextCursor::End);
              m_log->insertHtml(buildUserBubble(fact, i)); }
            m_log->append("");
            { int i = m_bubbleIdx++; m_bubbleTexts[i] = ris;
              m_log->moveCursor(QTextCursor::End);
              m_log->insertHtml(buildLocalBubble(ris, ms, i)); }
            emit chatCompleted(fact.left(40), m_log->toHtml());
            return;
        }
    }

    /* ── Controllo RAM e dimensione modello pre-Byzantine ── */
    if (!checkRam()) return;
    if (!checkModelSize(m_ai->model())) return;

    /* Aggiunge il contesto documento se allegato */
    if (!m_docContext.isEmpty()) {
        fact += "\n\n--- DOCUMENTO ALLEGATO ---\n" + m_docContext.left(8000);
    }
    m_byzStep = 0; m_byzA = m_byzC = "";
    m_taskOriginal = _inject_math(fact);
    m_opMode = OpMode::Byzantine;

    m_log->clear();
    m_log->append(QString(
        "\xf0\x9f\x94\xae  Motore Byzantino \xe2\x80\x94 verifica a 4 agenti\n"
        "\xe2\x9d\x93  Domanda: %1\n").arg(m_taskOriginal));
    m_log->append(QString(43, QChar(0x2500)));
    m_log->append("\xf0\x9f\x85\x90  [Agente A \xe2\x80\x94 Originale]\n");

    m_btnRun->setEnabled(false);
    m_btnStop->setEnabled(true);
    m_waitLbl->setVisible(true);

    m_ai->chat(
        _buildSys(m_taskOriginal, QString(
            "Sei l'Agente A del Motore Byzantino di Prismalux. "
            "Fornisci una risposta diretta e ben argomentata. "
            "Rispondi SEMPRE e SOLO in italiano."),
            m_ai->model(), m_ai->backend()),
        m_taskOriginal);
}

/* ══════════════════════════════════════════════════════════════
   Matematico Teorico
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::runMathTheory() {
    m_userScrolled = false;  /* nuovo task: torna in auto-scroll */
    QString problem = _sanitize_prompt(m_input->toPlainText().trimmed());
    if (problem.isEmpty()) { m_log->append("\xe2\x9a\xa0  Inserisci un problema matematico."); return; }

    {
        QElapsedTimer tmr; tmr.start();
        QString ris = guardiaMath(problem);
        double ms = tmr.nsecsElapsed() / 1e6;
        if (!ris.isEmpty()) {
            m_log->clear();
            { int i = m_bubbleIdx++; m_bubbleTexts[i] = problem;
              m_log->moveCursor(QTextCursor::End);
              m_log->insertHtml(buildUserBubble(problem, i)); }
            m_log->append("");
            { int i = m_bubbleIdx++; m_bubbleTexts[i] = ris;
              m_log->moveCursor(QTextCursor::End);
              m_log->insertHtml(buildLocalBubble(ris, ms, i)); }
            emit chatCompleted(problem.left(40), m_log->toHtml());
            return;
        }
    }

    /* ── Controllo RAM e dimensione modello pre-MathTheory ── */
    if (!checkRam()) return;
    if (!checkModelSize(m_ai->model())) return;

    m_byzStep = 0; m_byzA = m_byzC = "";
    m_taskOriginal = _inject_math(problem);
    m_opMode = OpMode::MathTheory;

    m_log->clear();
    m_log->append(QString(
        "\xf0\x9f\xa7\xae  Matematico Teorico \xe2\x80\x94 esplorazione a 4 agenti\n"
        "\xe2\x9d\x93  Problema: %1\n").arg(m_taskOriginal));
    m_log->append(QString(43, QChar(0x2500)));
    m_log->append("\xf0\x9f\x85\xb0  [Agente 1 \xe2\x80\x94 Enunciatore]\n");

    m_btnRun->setEnabled(false);
    m_btnStop->setEnabled(true);
    m_waitLbl->setVisible(true);

    m_ai->chat(
        "Sei l'Enunciatore matematico. "
        "Riformula il problema in forma rigorosa: definizioni, dominio, ipotesi. "
        "Sii conciso (max 150 parole). Rispondi SOLO in italiano.", problem);
}

