#include "pipeline_page.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFont>
#include <QScroller>

static constexpr const char* kByzSysBase =
    "Sei un agente esperto. Analizza la domanda e rispondi in modo preciso e dettagliato.";

static constexpr const char* kByzSynthSys =
    "Sei un sintetizzatore. Ricevi le risposte di piu' agenti esperti sullo stesso quesito. "
    "Identifica il consenso, evidenzia disaccordi, produci una risposta finale bilanciata.";

static constexpr const char* kAutSys =
    "Sei un agente autonomo ReAct. Ad ogni turno puoi:\n"
    "- THINK: ragiona sul problema\n"
    "- ACT: esegui un'azione (es: calcola, cerca, elabora)\n"
    "- OBS: osserva il risultato\n"
    "- ANSWER: fornisci la risposta finale\n"
    "Usa questi tag esplicitamente. Se hai la risposta, concludi con ANSWER.";

/* ── UI ────────────────────────────────────────────────────────── */
QWidget* PipelinePage::buildByzantineTab()
{
    auto* w    = new QWidget;
    auto* vbox = new QVBoxLayout(w);
    vbox->setContentsMargins(8, 8, 8, 8);
    vbox->setSpacing(8);

    auto* note = new QLabel(
        QString::fromUtf8("\xf0\x9f\xa4\x94") +
        " N agenti rispondono indipendentemente \xe2\x80\x94 un sintetizzatore produce il consenso.", w);
    note->setWordWrap(true);
    note->setObjectName("StatusLabel");
    vbox->addWidget(note);

    auto* form = new QFormLayout;
    form->setSpacing(8);
    m_byzAgentsSpin = new QSpinBox(w);
    m_byzAgentsSpin->setRange(2, 5);
    m_byzAgentsSpin->setValue(3);
    m_byzAgentsSpin->setSuffix(" agenti");
    form->addRow("Numero agenti:", m_byzAgentsSpin);
    vbox->addLayout(form);

    m_byzInput = new QTextEdit(w);
    m_byzInput->setFixedHeight(80);
    m_byzInput->setPlaceholderText("Domanda o problema da analizzare...");
    m_byzInput->setObjectName("InputEdit");
    vbox->addWidget(m_byzInput);

    m_byzOutput = new QTextEdit(w);
    m_byzOutput->setReadOnly(true);
    m_byzOutput->setObjectName("OutputEdit");
    QScroller::grabGesture(m_byzOutput->viewport(), QScroller::TouchGesture);
    vbox->addWidget(m_byzOutput, 1);
    return w;
}

QWidget* PipelinePage::buildAutonomousTab()
{
    auto* w    = new QWidget;
    auto* vbox = new QVBoxLayout(w);
    vbox->setContentsMargins(8, 8, 8, 8);
    vbox->setSpacing(8);

    auto* note = new QLabel(
        QString::fromUtf8("\xf0\x9f\xa4\x96") +
        " Agente ReAct: ragiona e agisce in passi autonomi (max 6).", w);
    note->setWordWrap(true);
    note->setObjectName("StatusLabel");
    vbox->addWidget(note);

    m_autInput = new QTextEdit(w);
    m_autInput->setFixedHeight(80);
    m_autInput->setPlaceholderText("Obiettivo o compito per l'agente autonomo...");
    m_autInput->setObjectName("InputEdit");
    vbox->addWidget(m_autInput);

    m_autOutput = new QTextEdit(w);
    m_autOutput->setReadOnly(true);
    m_autOutput->setObjectName("OutputEdit");
    QScroller::grabGesture(m_autOutput->viewport(), QScroller::TouchGesture);
    vbox->addWidget(m_autOutput, 1);
    return w;
}

/* ── Costruttore ───────────────────────────────────────────────── */
PipelinePage::PipelinePage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(10, 8, 10, 8);
    vbox->setSpacing(8);

    auto* title = new QLabel(
        QString::fromUtf8("\xf0\x9f\x94\x81") + " Pipeline AI", this);
    {   QFont f = title->font(); f.setPointSize(14); f.setBold(true); title->setFont(f); }
    vbox->addWidget(title);

    m_modeCmb = new QComboBox(this);
    m_modeCmb->addItem(QString::fromUtf8("\xf0\x9f\xa4\x94") + " Byzantino (N agenti)");
    m_modeCmb->addItem(QString::fromUtf8("\xf0\x9f\xa4\x96") + " Agente Autonomo ReAct");
    m_modeCmb->setMinimumHeight(44);
    vbox->addWidget(m_modeCmb);

    m_modeStack = new QStackedWidget(this);
    m_modeStack->addWidget(buildByzantineTab());
    m_modeStack->addWidget(buildAutonomousTab());
    vbox->addWidget(m_modeStack, 1);

    auto* btnRow = new QHBoxLayout;
    m_runBtn = new QPushButton(
        QString::fromUtf8("\xe2\x96\xb6") + " Avvia", this);
    m_runBtn->setObjectName("PrimaryBtn");
    m_runBtn->setMinimumHeight(44);
    btnRow->addWidget(m_runBtn, 1);

    m_stopBtn = new QPushButton(
        QString::fromUtf8("\xe2\x8f\xb9") + " Stop", this);
    m_stopBtn->setObjectName("DangerBtn");
    m_stopBtn->setMinimumHeight(44);
    m_stopBtn->setVisible(false);
    btnRow->addWidget(m_stopBtn);
    vbox->addLayout(btnRow);

    m_status = new QLabel("", this);
    m_status->setObjectName("StatusLabel");
    vbox->addWidget(m_status);

    connect(m_modeCmb, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &PipelinePage::onModeChanged);
    connect(m_runBtn,  &QPushButton::clicked, this, &PipelinePage::onRunClicked);
    connect(m_stopBtn, &QPushButton::clicked, this, &PipelinePage::onStopClicked);
    connect(m_ai, &AiClient::token,    this, &PipelinePage::onToken);
    connect(m_ai, &AiClient::finished, this, &PipelinePage::onFinished);
    connect(m_ai, &AiClient::error,    this, &PipelinePage::onAiError);
    connect(m_ai, &AiClient::aborted,  this, &PipelinePage::onAborted);
}

/* ── Slots ─────────────────────────────────────────────────────── */
void PipelinePage::onModeChanged(int idx)
{
    m_modeStack->setCurrentIndex(idx);
    m_mode = idx == 0 ? Mode::Byzantine : Mode::Autonomous;
}

void PipelinePage::onRunClicked()
{
    if (m_mode == Mode::Byzantine) runByzantine();
    else                           runAutonomous();
}

void PipelinePage::onStopClicked()
{
    m_ai->abort();
}

void PipelinePage::runByzantine()
{
    const QString q = m_byzInput ? m_byzInput->toPlainText().trimmed() : QString{};
    if (q.isEmpty()) return;
    m_byzTotalAgents = m_byzAgentsSpin ? m_byzAgentsSpin->value() : 3;
    m_byzRound = 0;
    m_byzResponses.clear();
    if (m_byzOutput) m_byzOutput->clear();
    setBusy(true);
    m_status->setText(QString("Agente 1/%1...").arg(m_byzTotalAgents));
    const QString agentSys = QString(kByzSysBase) +
        QString("\nSei l'Agente 1 di %1. Rispondi indipendentemente.").arg(m_byzTotalAgents);
    m_ai->chat(agentSys, q);
}

void PipelinePage::runAutonomous()
{
    const QString q = m_autInput ? m_autInput->toPlainText().trimmed() : QString{};
    if (q.isEmpty()) return;
    m_autStep = 0;
    if (m_autOutput) m_autOutput->clear();
    setBusy(true);
    m_status->setText("Passo 1 — THINK...");
    m_ai->chat(kAutSys, "Obiettivo: " + q + "\n\nInzia con THINK:");
}

void PipelinePage::onToken(const QString& t)
{
    QTextEdit* out = (m_mode == Mode::Byzantine) ? m_byzOutput : m_autOutput;
    if (!out) return;
    out->moveCursor(QTextCursor::End);
    out->insertPlainText(t);
    out->moveCursor(QTextCursor::End);
}

void PipelinePage::onFinished(const QString& full)
{
    if (m_mode == Mode::Byzantine) {
        m_byzResponses.append(full);
        m_byzRound++;
        if (m_byzRound < m_byzTotalAgents) {
            m_status->setText(QString("Agente %1/%2...").arg(m_byzRound + 1).arg(m_byzTotalAgents));
            const QString q = m_byzInput ? m_byzInput->toPlainText().trimmed() : QString{};
            const QString agentSys = QString(kByzSysBase) +
                QString("\nSei l'Agente %1 di %2. Rispondi indipendentemente.")
                    .arg(m_byzRound + 1).arg(m_byzTotalAgents);
            m_ai->chat(agentSys, q);
        } else {
            /* Sintesi finale */
            m_status->setText("Sintesi finale in corso...");
            if (m_byzOutput) {
                m_byzOutput->append("\n\n--- SINTESI FINALE ---\n");
            }
            QString synthUser = "Domanda originale: " +
                (m_byzInput ? m_byzInput->toPlainText().trimmed() : QString{}) +
                "\n\nRisposte agenti:\n";
            for (int i = 0; i < m_byzResponses.size(); ++i)
                synthUser += QString("Agente %1: %2\n\n").arg(i + 1).arg(m_byzResponses[i]);
            m_ai->chat(kByzSynthSys, synthUser);
            m_byzRound = -1; /* flag sintesi */
        }
    } else {
        m_autStep++;
        if (m_autStep < kMaxAutSteps && !full.contains("ANSWER")) {
            m_status->setText(QString("Passo %1...").arg(m_autStep + 1));
            const QString q = m_autInput ? m_autInput->toPlainText().trimmed() : QString{};
            m_ai->chat(kAutSys,
                "Obiettivo: " + q + "\n\n" +
                "Passi precedenti:\n" + full + "\n\nContinua (THINK/ACT/OBS/ANSWER):");
        } else {
            setBusy(false);
            m_status->setText(QString::fromUtf8("\xe2\x9c\x85") +
                              QString(" Completato in %1 passi.").arg(m_autStep));
        }
    }

    if (m_byzRound == -1) {
        /* Sintesi completata */
        setBusy(false);
        m_status->setText(QString::fromUtf8("\xe2\x9c\x85") +
                          QString(" Byzantino completato (%1 agenti).").arg(m_byzTotalAgents));
        m_byzRound = 0;
    }
}

void PipelinePage::onAiError(const QString& msg)
{
    setBusy(false);
    m_status->setText(QString::fromUtf8("\xe2\x9d\x8c") + " " + msg);
}

void PipelinePage::onAborted()
{
    setBusy(false);
    m_status->setText("Interrotto.");
}

void PipelinePage::setBusy(bool busy)
{
    if (m_runBtn)  m_runBtn->setEnabled(!busy);
    if (m_stopBtn) m_stopBtn->setVisible(busy);
    if (m_byzInput) m_byzInput->setEnabled(!busy);
    if (m_autInput) m_autInput->setEnabled(!busy);
}
