#include "agenti_page.h"
#include "agenti_page_p.h"
#include "../prismalux_paths.h"
namespace P = PrismaluxPaths;
#include <QElapsedTimer>
#include <QRegularExpression>

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

    _setRunBusy(true);
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

    _setRunBusy(true);
    m_waitLbl->setVisible(true);

    m_ai->chat(
        "Sei l'Enunciatore matematico. "
        "Riformula il problema in forma rigorosa: definizioni, dominio, ipotesi. "
        "Sii conciso (max 150 parole). Rispondi SOLO in italiano.", problem);
}


/* ══════════════════════════════════════════════════════════════
   _finishedMathTheory — step motore Matematico Teorico completato
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::_finishedMathTheory() {
    {
        /* stripThink: rimuove <think>...</think>; se rimane vuoto usa il contenuto think */
        auto stripThink = [](QString& s) {
            static const QRegularExpression reCap("<think>([\\s\\S]*?)</think>",
                QRegularExpression::CaseInsensitiveOption);
            static const QRegularExpression reRem("<think>[\\s\\S]*?</think>",
                QRegularExpression::CaseInsensitiveOption);
            const QString orig = s;
            s.remove(reRem);
            s = s.trimmed();
            if (s.isEmpty()) {
                auto m = reCap.match(orig);
                if (m.hasMatch()) s = m.captured(1).trimmed();
            }
        };
        if (m_byzStep == 0) stripThink(m_byzA);
        if (m_byzStep == 2) stripThink(m_byzC);
    }
    m_byzStep++;
    static const char* mathLabels[] = {
        "\xf0\x9f\x94\xad  [Agente 2 \xe2\x80\x94 Esploratore]\n",
        "\xf0\x9f\x93\x90  [Agente 3 \xe2\x80\x94 Dimostratore]\n",
        "\xe2\x9c\xa8  [Agente 4 \xe2\x80\x94 Sintetizzatore]\n",
    };
    if (m_byzStep <= 3) {
        m_log->append(QString("\n") + QString(43, QChar(0x2500)));
        m_log->append(mathLabels[m_byzStep - 1]);
    }
    QString ctx;
    switch (m_byzStep) {
        case 1:
            ctx = QString("Problema originale: %1\n\nFormulazione rigorosa:\n%2").arg(m_taskOriginal, m_byzA);
            m_ai->chat("Sei l'Esploratore matematico. Individua teoremi o metodi applicabili. "
                       "Sii conciso (max 150 parole). Rispondi SOLO in italiano.", ctx);
            break;
        case 2:
            ctx = QString("Problema: %1\n\nFormulazione rigorosa:\n%2").arg(m_taskOriginal, m_byzA);
            m_ai->chat("Sei il Dimostratore matematico. Fornisci la soluzione passo per passo. "
                       "Sii conciso (max 200 parole). Rispondi SOLO in italiano.", ctx);
            break;
        case 3:
            ctx = QString("Problema: %1\n\nSoluzione:\n%2").arg(m_taskOriginal, m_byzC);
            m_ai->chat("Sei il Sintetizzatore matematico. Riassumi il risultato finale. "
                       "Sii conciso (max 150 parole). Rispondi SOLO in italiano.", ctx);
            break;
        default:
            m_log->append("\n\n\xe2\x9c\x85  Esplorazione matematica completata.");
            _setRunBusy(false);
            m_opMode = OpMode::Idle;
            tryShowChart(m_taskOriginal + "\n" + m_byzA + "\n" + m_byzC);
            emit chatCompleted(m_taskOriginal.left(40), m_log->toHtml());
            break;
    }
    return;
}

/* ══════════════════════════════════════════════════════════════
   _finishedByzantine — step Motore Byzantino completato
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::_finishedByzantine() {
/* Byzantino — strip think tags prima di usare l'output come contesto */
{
    auto stripThink = [](QString& s) {
        static const QRegularExpression reCap("<think>([\\s\\S]*?)</think>",
            QRegularExpression::CaseInsensitiveOption);
        static const QRegularExpression reRem("<think>[\\s\\S]*?</think>",
            QRegularExpression::CaseInsensitiveOption);
        const QString orig = s;
        s.remove(reRem);
        s = s.trimmed();
        if (s.isEmpty()) {
            auto m = reCap.match(orig);
            if (m.hasMatch()) s = m.captured(1).trimmed();
        }
    };
    if (m_byzStep == 0) stripThink(m_byzA);
    if (m_byzStep == 2) stripThink(m_byzC);
}
m_byzStep++;
static const char* labels[] = {
    "\xf0\x9f\x85\xb1  [Agente B \xe2\x80\x94 Avvocato del Diavolo]\n",
    "\xf0\x9f\x85\x92  [Agente C \xe2\x80\x94 Gemello Indipendente]\n",
    "\xf0\x9f\x85\x93  [Agente D \xe2\x80\x94 Giudice]\n",
};
if (m_byzStep <= 3) {
    m_log->append(QString("\n") + QString(43, QChar(0x2500)));
    m_log->append(labels[m_byzStep - 1]);
}
QString userMsg;
switch (m_byzStep) {
    case 1:
        userMsg = QString("Domanda: %1\n\nRisposta A:\n%2").arg(m_taskOriginal, m_byzA);
        m_ai->chat(_buildSys(m_taskOriginal, QString(
                   "Sei l'Agente B del Motore Byzantino, l'Avvocato del Diavolo. "
                   "Cerca ATTIVAMENTE errori e contraddizioni nella risposta A. "
                   "Rispondi SEMPRE e SOLO in italiano."),
                   m_ai->model(), m_ai->backend()), userMsg);
        break;
    case 2:
        m_ai->chat(_buildSys(m_taskOriginal, QString(
                   "Sei l'Agente C del Motore Byzantino, il Gemello Indipendente. "
                   "Rispondi alla domanda originale da un angolo diverso. "
                   "Rispondi SEMPRE e SOLO in italiano."),
                   m_ai->model(), m_ai->backend()), m_taskOriginal);
        break;
    case 3:
        userMsg = QString("Domanda: %1\n\nRisposta A:\n%2\n\nRisposta C:\n%3")
                  .arg(m_taskOriginal, m_byzA, m_byzC);
        m_ai->chat(_buildSys(m_taskOriginal, QString(
                   "Sei l'Agente D del Motore Byzantino, il Giudice. "
                   "Se A e C concordano e B non trova errori validi: conferma. "
                   "Altrimenti segnala l'incertezza. Produci il verdetto finale. "
                   "Rispondi SEMPRE e SOLO in italiano."),
                   m_ai->model(), m_ai->backend()), userMsg);
        break;
    default:
        m_log->append("\n\n\xe2\x9c\x85  Verifica Byzantina completata.");
        m_log->append("\xe2\x9c\xa8 Verit\xc3\xa0 rivelata. Bevi la conoscenza.");
        _setRunBusy(false);
        m_opMode = OpMode::Idle;
        tryShowChart(m_taskOriginal + "\n" + m_byzA + "\n" + m_byzC);
        emit chatCompleted(m_taskOriginal.left(40), m_log->toHtml());
        break;
}
}
