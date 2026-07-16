/* ══════════════════════════════════════════════════════════════
   main_programming_agentica.cpp — ProgrammazionePage: Agentica
   ==================================================================
   Sub-tab "🤖 Agentica" — builder UI + slot. Split da
   main_programming.cpp/main_programming_slots.cpp (TODO D-8).
   ══════════════════════════════════════════════════════════════ */
#include "main_programming.h"
#include "main_programming_p.h"
#include "../prismalux_paths.h"
#include "../ai_utils.h"
#include "../log_bus.h"
#include "../dpi_utils.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QGroupBox>
#include <QFrame>
#include <QFont>
#include <QTimer>
#include <QMessageBox>
#include <QTextCursor>

namespace P = PrismaluxPaths;

/* ══════════════════════════════════════════════════════════════
   buildAgentica — sub-tab "🤖 Agentica"
   ══════════════════════════════════════════════════════════════ */
QWidget* ProgrammazionePage::buildAgentica(QWidget* parent)
{
    auto* w   = new QWidget(parent);
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(12, 12, 12, 12);
    lay->setSpacing(10);

    buildAgenticaHeader(lay, w);
    lay->addWidget(buildAgenticaToolbar(w));
    lay->addWidget(buildAgenticaTaskGroup(w));
    lay->addWidget(buildAgenticaOutputGroup(w), 1);
    setupAgenticaConnections(nullptr, nullptr);
    return w;
}

/* ── Header: descrizione + separatore ── */
void ProgrammazionePage::buildAgenticaHeader(QVBoxLayout* lay, QWidget* w)
{
    auto* desc = new QLabel(
        "\xf0\x9f\x8f\x97\xef\xb8\x8f <b>Architetta Software</b> \xe2\x86\x92 "
        "<i>\xe2\x80\x9c" "Costruisci un sistema\xe2\x80\x9d</i>"
        " \xe2\x80\x94 output da copiare nel progetto", w);
    desc->setObjectName("hintLabel");
    desc->setWordWrap(true);
    lay->addWidget(desc);

    auto* sep = new QFrame(w);
    sep->setFrameShape(QFrame::HLine);
    sep->setObjectName("sidebarSep");
    lay->addWidget(sep);
}

/* ── Toolbar: tipo agente + linguaggio + pulsanti ── */
QWidget* ProgrammazionePage::buildAgenticaToolbar(QWidget* parent)
{
    auto* toolRow = new QWidget(parent);
    auto* toolLay = new QHBoxLayout(toolRow);
    toolLay->setContentsMargins(0, 0, 0, 0);
    toolLay->setSpacing(8);

    toolLay->addWidget(new QLabel(tr("Tipo agente:"), toolRow));
    m_agentType = new QComboBox(toolRow);
    m_agentType->setObjectName("settingCombo");
    m_agentType->addItem(
        "Pipeline codice (3 agenti: Analisi \xe2\x86\x92 Impl \xe2\x86\x92 Test)",
        QString("pipeline"));
    m_agentType->addItem("RAG + Codice (ricerca documenti + generazione)",
                         QString("rag"));
    m_agentType->addItem("Refactoring AI (pattern + pulizia)",
                         QString("refactor"));
    m_agentType->addItem("Generatore test unitari",
                         QString("testgen"));
    m_agentType->addItem("Debugging agentico (analisi + fix + verifica)",
                         QString("debug"));
    m_agentType->addItem(
        "Motore Byzantino \xe2\x80\x94 anti-allucinazione (A"
        "\xe2\x86\x92" "B" "\xe2\x86\x92" "C" "\xe2\x86\x92" "D)",
        QString("byzantine"));
    m_agentType->setMinimumWidth(300);
    toolLay->addWidget(m_agentType);

    /* ── Selettore modello AI (a sinistra di "Linguaggio") ── */
    toolLay->addSpacing(12);
    auto* lblModel = new QLabel(tr("\xf0\x9f\xa4\x96  Modello AI:"), toolRow);
    lblModel->setObjectName("cardDesc");
    toolLay->addWidget(lblModel);

    m_agentModel = new QComboBox(toolRow);
    m_agentModel->setObjectName("settingCombo");
    m_agentModel->addItem(
        m_ai ? (m_ai->model().isEmpty() ? "(nessun modello)" : m_ai->model())
             : "(AI non disponibile)",
        m_ai ? m_ai->model() : QString());
    m_agentModel->setMinimumWidth(dpiScale(160));
    toolLay->addWidget(m_agentModel);

    auto* btnRefAgent = new QPushButton("\xf0\x9f\x94\x84", toolRow);
    btnRefAgent->setObjectName("actionBtn");
    btnRefAgent->setFixedWidth(dpiScale(32));
    btnRefAgent->setToolTip(tr("Aggiorna lista modelli disponibili"));
    toolLay->addWidget(btnRefAgent);
    connect(btnRefAgent, &QPushButton::clicked,
            this, &ProgrammazionePage::populateAgentModels);
    QTimer::singleShot(0, this, &ProgrammazionePage::populateAgentModels);

    /* ── Linguaggio ── */
    toolLay->addSpacing(12);
    toolLay->addWidget(new QLabel(tr("Linguaggio:"), toolRow));
    m_agentLang = new QComboBox(toolRow);
    m_agentLang->setObjectName("settingCombo");
    m_agentLang->addItems({"Python", "C", "C++", "JavaScript", "Bash"});
    m_agentLang->setFixedWidth(dpiScale(110));
    toolLay->addWidget(m_agentLang);
    toolLay->addStretch(1);

    m_btnAgentRun = new QPushButton(tr("\xe2\x96\xb6  Genera"), toolRow);
    m_btnAgentRun->setObjectName("actionBtn");
    m_btnAgentRun->setProperty("highlight", "true");
    m_btnAgentRun->setToolTip(tr("Genera il codice agente (F5)"));
    toolLay->addWidget(m_btnAgentRun);

    m_btnAgentStop = new QPushButton(tr("\xe2\x96\xa0  Stop"), toolRow);
    m_btnAgentStop->setObjectName("actionBtn");
    m_btnAgentStop->setProperty("danger", "true");
    m_btnAgentStop->setEnabled(false);
    m_btnAgentStop->setToolTip(tr("Interrompi la generazione"));
    toolLay->addWidget(m_btnAgentStop);

    return toolRow;
}

/* ── Task group: campo testo descrizione ── */
QWidget* ProgrammazionePage::buildAgenticaTaskGroup(QWidget* parent)
{
    QFont monoFont;
    monoFont.setFamily("JetBrains Mono");
    monoFont.setStyleHint(QFont::Monospace);
    monoFont.setPointSize(monoFontPt(11));

    auto* taskGroup = new QGroupBox(
        tr("\xf0\x9f\x93\x9d  Descrizione del task (cosa vuoi costruire?)"), parent);
    taskGroup->setObjectName("cardGroup");
    auto* taskLay = new QVBoxLayout(taskGroup);
    taskLay->setContentsMargins(4, 8, 4, 4);

    m_agentTask = new QPlainTextEdit(taskGroup);
    m_agentTask->setObjectName("agentTaskEditor");
    m_agentTask->setFont(monoFont);
    m_agentTask->setMaximumHeight(140);
    m_agentTask->setPlaceholderText(
        tr("Descrivi il sistema che vuoi costruire...\n\n"
        "Esempi:\n"
        "  \xe2\x80\xa2 \"Sistema RAG in Python che indicizza PDF e risponde a domande con citazioni\"\n"
        "  \xe2\x80\xa2 \"Pipeline 3 agenti: Analista \xe2\x86\x92 Programmatore \xe2\x86\x92 Tester per un parser CSV\"\n"
        "  \xe2\x80\xa2 \"Refactoring con pattern Strategy di questo codice C++\"\n"
        "  \xe2\x80\xa2 \"Test unitari completi (pytest) per una classe BankAccount\""));
    taskLay->addWidget(m_agentTask);
    return taskGroup;
}

/* ── Riga selezione modello ── */
QWidget* ProgrammazionePage::buildAgenticaModelRow(QWidget* parent)
{
    auto* modelRow = new QWidget(parent);
    auto* modelLay = new QHBoxLayout(modelRow);
    modelLay->setContentsMargins(0, 0, 0, 0);
    modelLay->setSpacing(8);

    auto* lblModel = new QLabel(tr("\xf0\x9f\xa4\x96  Modello AI:"), modelRow);
    lblModel->setObjectName("cardDesc");
    modelLay->addWidget(lblModel);

    m_agentModel = new QComboBox(modelRow);
    m_agentModel->setObjectName("settingCombo");
    m_agentModel->addItem(
        m_ai ? (m_ai->model().isEmpty() ? "(nessun modello)" : m_ai->model())
             : "(AI non disponibile)",
        m_ai ? m_ai->model() : QString());
    modelLay->addWidget(m_agentModel, 1);

    auto* btnRefAgent = new QPushButton("\xf0\x9f\x94\x84", modelRow);
    btnRefAgent->setObjectName("actionBtn");
    btnRefAgent->setFixedWidth(dpiScale(32));
    btnRefAgent->setToolTip(tr("Aggiorna lista modelli disponibili"));
    modelLay->addWidget(btnRefAgent);

    connect(btnRefAgent, &QPushButton::clicked,
            this, &ProgrammazionePage::populateAgentModels);
    QTimer::singleShot(0, this, &ProgrammazionePage::populateAgentModels);

    return modelRow;
}

/* ── Output group: streaming + pulsanti ── */
QWidget* ProgrammazionePage::buildAgenticaOutputGroup(QWidget* parent)
{
    QFont monoFont;
    monoFont.setFamily("JetBrains Mono");
    monoFont.setStyleHint(QFont::Monospace);
    monoFont.setPointSize(monoFontPt(11));

    auto* outGroup = new QGroupBox(
        tr("\xf0\x9f\xa4\x96  Output agente (streaming)"), parent);
    outGroup->setObjectName("cardGroup");
    auto* outLay = new QVBoxLayout(outGroup);
    outLay->setContentsMargins(4, 8, 4, 4);
    outLay->setSpacing(6);

    m_agentOutput = new QTextEdit(outGroup);
    m_agentOutput->setObjectName("chatLog");
    m_agentOutput->setReadOnly(true);
    m_agentOutput->setFont(monoFont);
    m_agentOutput->setPlaceholderText(
        tr("L'output dell'agente appari\xc3\xa0 qui in streaming...\n\n"
        "Scrivi il task sopra e premi \xe2\x96\xb6 Genera."));
    outLay->addWidget(m_agentOutput, 1);

    auto* outBtnRow = new QWidget(outGroup);
    auto* outBtnLay = new QHBoxLayout(outBtnRow);
    outBtnLay->setContentsMargins(0, 2, 0, 0);
    outBtnLay->setSpacing(8);

    m_btnAgentInsert = new QPushButton(
        tr("\xe2\x86\x91  Apri in editor Programmazione"), outBtnRow);
    m_btnAgentInsert->setObjectName("actionBtn");
    m_btnAgentInsert->setEnabled(false);
    m_btnAgentInsert->setToolTip(
        tr("Estrae il primo blocco codice e lo apre nel tab \xf0\x9f\x92\xbb Programmazione"));
    outBtnLay->addWidget(m_btnAgentInsert);

    auto* btnClearAgent = new QPushButton(
        tr("\xf0\x9f\x97\x91  Pulisci output"), outBtnRow);
    btnClearAgent->setObjectName("actionBtn");
    outBtnLay->addWidget(btnClearAgent);
    outBtnLay->addStretch(1);

    outLay->addWidget(outBtnRow);

    connect(btnClearAgent, &QPushButton::clicked,
            this, &ProgrammazionePage::onBtnClearAgentClicked);
    connect(m_btnAgentInsert, &QPushButton::clicked,
            this, &ProgrammazionePage::onBtnAgentInsertClicked);

    return outGroup;
}

/* ── Connessioni Agentica ── */
void ProgrammazionePage::setupAgenticaConnections(QPushButton* /*btnRefAgent*/,
                                                   QPushButton* /*btnClearAgent*/)
{
    /* Model refresh and model population already wired in buildAgenticaModelRow. */
    /* Output button connections already wired in buildAgenticaOutputGroup. */
    connect(m_btnAgentRun,  &QPushButton::clicked,
            this, &ProgrammazionePage::runAgente);
    connect(m_btnAgentStop, &QPushButton::clicked,
            this, &ProgrammazionePage::onBtnAgentStopClicked);
}

/* ══════════════════════════════════════════════════════════════
   runAgente — esegue la generazione agentica AI
   Costruisce il system prompt in base al tipo di agente scelto
   e lancia la chiamata streaming a m_ai->chat().
   ══════════════════════════════════════════════════════════════ */
void ProgrammazionePage::runAgente()
{
    if (!m_ai || !m_agentTask) return;

    const QString task = m_agentTask->toPlainText().trimmed();
    if (task.isEmpty()) {
        m_agentOutput->setPlainText(
            "\xe2\x9d\x8c  Scrivi una descrizione del task prima di premere Genera.");
        return;
    }
    if (m_ai->busy()) {
        m_agentOutput->setPlainText(
            "\xe2\x9a\xa0\xef\xb8\x8f  AI occupata. Attendi o premi Stop.");
        return;
    }

    const QString tipo = m_agentType ? m_agentType->currentData().toString() : "pipeline";
    const QString lang = m_agentLang ? m_agentLang->currentText() : "Python";

    /* Applica il modello scelto nel tab Agentica */
    if (m_agentModel) {
        const QString sel = m_agentModel->currentData().toString();
        if (!sel.isEmpty() && sel != m_ai->model())
            m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), sel);
    }

    /* ── Costruzione system prompt in base al tipo agente ── */
    QString sys;
    QString user;

    if (tipo == "pipeline") {
        sys = QString(
            "Sei un architetto software esperto in sistemi multi-agente. "
            "Il tuo compito \xc3\xa8 progettare e implementare una pipeline a 3 agenti in %1:\n"
            "  Agente 1 (Analista): analizza il problema, estrae requisiti\n"
            "  Agente 2 (Implementatore): scrive il codice completo\n"
            "  Agente 3 (Tester): genera test unitari per il codice\n\n"
            "Per ogni agente:\n"
            "1. Mostra il prompt/istruzione che riceve\n"
            "2. Mostra il codice che produce in un blocco ```%2 ... ```\n"
            "3. Breve spiegazione\n\n"
            "Usa commenti in italiano. Rispondi SEMPRE in italiano.").arg(lang, lang.toLower());
        user = QString("Task: %1").arg(task);

    } else if (tipo == "rag") {
        sys = QString(
            "Sei un esperto di sistemi RAG (Retrieval-Augmented Generation) in %1. "
            "Implementa un sistema RAG completo che:\n"
            "  1. Indicizza documenti (chunking + embedding)\n"
            "  2. Cerca i chunk rilevanti per una query (similarit\xc3\xa0 coseno)\n"
            "  3. Costruisce il prompt con i chunk trovati\n"
            "  4. Chiama il LLM e restituisce la risposta con citazioni\n\n"
            "Usa llama.cpp / Ollama HTTP API per l'inferenza locale. "
            "Struttura il codice in classi/funzioni chiare. "
            "Includi un blocco ```%2 ... ``` con il codice completo. "
            "Commenta in italiano. Rispondi SEMPRE in italiano.").arg(lang, lang.toLower());
        user = QString("Costruisci: %1").arg(task);

    } else if (tipo == "refactor") {
        sys = QString(
            "Sei un esperto di refactoring e design pattern in %1. "
            "Analizza il codice richiesto e:\n"
            "  1. Identifica i problemi (smell, violazioni SOLID, duplicazioni)\n"
            "  2. Proponi il design pattern pi\xc3\xb9 adatto\n"
            "  3. Scrivi il codice refactored completo in ```%2 ... ```\n"
            "  4. Lista le modifiche applicate (puntato)\n\n"
            "Commenta le decisioni architetturali. Rispondi SEMPRE in italiano.").arg(lang, lang.toLower());
        user = QString("Refactoring richiesto: %1").arg(task);

    } else if (tipo == "testgen") {
        sys = QString(
            "Sei un esperto di testing e TDD in %1. "
            "Genera una suite di test unitari completa che:\n"
            "  1. Copre tutti i casi normali (happy path)\n"
            "  2. Copra i casi limite (edge cases: valori vuoti, None/null, overflow)\n"
            "  3. Includa test negativi (input invalidi, eccezioni attese)\n"
            "  4. Usi il framework pi\xc3\xb9 appropriato (pytest per Python, etc.)\n\n"
            "Mostra i test in un blocco ```%2 ... ```. "
            "Aggiungi commenti esplicativi in italiano. Rispondi SEMPRE in italiano.").arg(lang, lang.toLower());
        user = QString("Genera test per: %1").arg(task);

    } else if (tipo == "debug") {
        sys = QString(
            "Sei un esperto debugger che usa un approccio a 3 step in %1:\n"
            "  Step 1 (Analisi): identifica tutti i potenziali bug e le cause radice\n"
            "  Step 2 (Fix): scrivi il codice corretto in ```%2 ... ```\n"
            "  Step 3 (Verifica): spiega come verificare che i bug siano risolti\n\n"
            "Per ogni bug trovato: descrivi il problema, la causa, e il fix applicato. "
            "Rispondi SEMPRE in italiano.").arg(lang, lang.toLower());
        user = QString("Analizza e correggi: %1").arg(task);

    } else { /* byzantine */
        sys = QString(
            "Sei un sistema a 4 agenti anti-allucinazione (Motore Byzantino) per codice %1:\n\n"
            "  Agente A (Originale): genera la soluzione\n"
            "  Agente B (Avvocato del Diavolo): trova attivamente errori in A\n"
            "  Agente C (Gemello indipendente): risolve lo stesso problema da zero\n"
            "  Agente D (Giudice): confronta A e C, valuta le critiche di B, emette la soluzione finale\n\n"
            "Per ogni agente mostra il suo ragionamento e l'output. "
            "La soluzione finale di D deve essere in un blocco ```%2 ... ```. "
            "Commenta in italiano. Rispondi SEMPRE in italiano.").arg(lang, lang.toLower());
        user = QString("Task da risolvere con Motore Byzantino: %1").arg(task);
    }

    /* ── Avvio streaming ── */
    m_agentOutput->clear();
    {
        const QString modelName = m_ai->model().isEmpty() ? "AI" : m_ai->model();
        const QString tipoLabel = m_agentType
            ? m_agentType->currentText()
            : tipo;
        m_agentOutput->setPlainText(
            QString("\xf0\x9f\xa4\x96  Modello: %1\n"
                    "\xf0\x9f\x94\xa7  Tipo: %2\n"
                    "\xf0\x9f\x92\xbb  Linguaggio: %3\n%4\n\n")
            .arg(modelName, tipoLabel, lang,
                 QString(qMax(modelName.length(), 20), '-')));
    }
    m_btnAgentRun->setEnabled(false);
    m_btnAgentStop->setEnabled(true);
    m_btnAgentInsert->setEnabled(false);

    disconnect(m_agentTokenConn);
    disconnect(m_agentFinishedConn);
    disconnect(m_agentErrorConn);
    m_agentTokenConn    = connect(m_ai, &AiClient::token,    this, &ProgrammazionePage::onAgentToken);
    m_agentFinishedConn = connect(m_ai, &AiClient::finished, this, &ProgrammazionePage::onAgentFinished);
    m_agentErrorConn    = connect(m_ai, &AiClient::error,    this, &ProgrammazionePage::onAgentError);
    m_ai->chat(P::prependKnowledge(sys), user);
}

/* ======================================================================
   Sezione 5 — Agentica slots
   ====================================================================== */

void ProgrammazionePage::populateAgentModels()
{
    if (m_ai && m_agentModel) AiUtils::populateModelCombo(m_ai, m_agentModel, this);
}

void ProgrammazionePage::onBtnAgentStopClicked()
{
    if (m_ai) m_ai->abort();
    if (m_btnAgentRun)  m_btnAgentRun->setEnabled(true);
    if (m_btnAgentStop) m_btnAgentStop->setEnabled(false);
}

void ProgrammazionePage::onBtnClearAgentClicked()
{
    if (m_agentOutput) m_agentOutput->clear();
}

void ProgrammazionePage::onBtnAgentInsertClicked()
{
    if (!m_agentOutput || !m_editor) return;
    const QString text = m_agentOutput->toPlainText();
    static const QRegularExpression re(
        "```(?:\\w+)?\\n([\\s\\S]*?)```");
    const auto m = re.match(text);
    const QString code = m.hasMatch() ? m.captured(1).trimmed() : text.trimmed();
    if (code.isEmpty()) return;
    if (!m_editor->toPlainText().trimmed().isEmpty()) {
        if (QMessageBox::question(this,
                "Sovrascrivere il codice?",
                "L'editor contiene codice.\nVuoi sostituirlo con il codice generato dall'agente?",
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
            return;
    }
    m_editor->setPlainText(code);
    if (m_innerTabs) m_innerTabs->setCurrentIndex(0);
}

void ProgrammazionePage::onAgentToken(const QString& tok)
{
    if (!m_agentOutput) return;
    m_agentOutput->moveCursor(QTextCursor::End);
    m_agentOutput->insertPlainText(tok);
    m_agentOutput->ensureCursorVisible();
}

void ProgrammazionePage::onAgentFinished(const QString& full)
{
    disconnect(m_agentTokenConn);
    disconnect(m_agentFinishedConn);
    disconnect(m_agentErrorConn);
    if (m_btnAgentRun)    m_btnAgentRun->setEnabled(true);
    if (m_btnAgentStop)   m_btnAgentStop->setEnabled(false);
    const bool hasCode = full.contains("```");
    if (m_btnAgentInsert) m_btnAgentInsert->setEnabled(hasCode);
}

void ProgrammazionePage::onAgentError(const QString& msg)
{
    disconnect(m_agentTokenConn);
    disconnect(m_agentFinishedConn);
    disconnect(m_agentErrorConn);
    if (m_btnAgentRun)  m_btnAgentRun->setEnabled(true);
    if (m_btnAgentStop) m_btnAgentStop->setEnabled(false);
    if (m_agentOutput) {
        m_agentOutput->moveCursor(QTextCursor::End);
        m_agentOutput->insertPlainText(
            QString("\n\xe2\x9d\x8c  Errore: %1").arg(msg));
    }
    LogBus::post("\xe2\x9d\x8c Programmazione: Agentica errore AI: " + msg);
}

