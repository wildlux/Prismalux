#include "widget_rdkit.h"
#include "../dpi_utils.h"
#include "../prismalux_paths.h"
#include "../widgets/model_combo_box.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QComboBox>
#include <QProcess>
#include <QFrame>
namespace P = PrismaluxPaths;

static const char* kSys[] = {
    "Sei un esperto di RDKit. "
    "Genera SOLO codice Python RDKit per analizzare una molecola SMILES: "
    "peso molecolare, formula, anelli, stereocentri, descrittori chimici. "
    "Rispondi SOLO con codice Python tra ``` e ```.",
    "Sei un esperto di RDKit e similarit\xc3\xa0 molecolare. "
    "Genera SOLO codice Python RDKit per calcolare fingerprint Morgan e Tanimoto similarity. "
    "Rispondi SOLO con codice Python tra ``` e ```.",
    "Sei un esperto di RDKit. "
    "Genera SOLO codice Python RDKit per ottimizzare la geometria 3D (MMFF94) "
    "e salvare in SDF. Rispondi SOLO con codice Python tra ``` e ```.",
    "Sei un esperto di RDKit. "
    "Genera SOLO codice Python RDKit libero. "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```.",
    nullptr
};
static const char* kActions[] = {
    "\xe2\x9a\x97  Analisi molecola",
    "\xf0\x9f\x94\x8d  Similarit\xc3\xa0 Tanimoto",
    "\xf0\x9f\x94\xac  Ottimizza geometria 3D",
    "\xf0\x9f\x90\x8d  Script libero",
    nullptr
};

RDKitWidget::RDKitWidget(AiClient* ai, QWidget* parent)
    : AiScriptWidget(ai, parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(6);

    /* Banner medgemma */
    auto* medFrame = new QFrame(this);
    medFrame->setFrameShape(QFrame::StyledPanel);
    medFrame->setStyleSheet(
        "QFrame { background:#0d2a1a; border:2px solid #2e7d52; border-radius:6px; padding:2px; }");
    auto* medLay = new QHBoxLayout(medFrame);
    medLay->setContentsMargins(10, 8, 10, 8);
    auto* medLbl = new QLabel(
        "\xf0\x9f\xa7\xac <b>Per analisi bioinformatiche usa modelli specializzati:</b> "
        "<code>ollama run medgemma</code> | <code>ollama run medgemma1.5</code>",
        medFrame);
    medLbl->setTextFormat(Qt::RichText);
    medLbl->setWordWrap(true);
    medLbl->setStyleSheet("color:#c8f0d8; background:transparent; border:none;");
    medLay->addWidget(medLbl);
    lay->addWidget(medFrame);

    auto* descLbl = new QLabel(
        tr("\xf0\x9f\x94\xac  <i>RDKit \xe2\x80\x94 Libreria open-source di chemioinformatica.</i>"), this);
    descLbl->setObjectName("hintLabel");
    descLbl->setTextFormat(Qt::RichText);
    descLbl->setWordWrap(true);
    lay->addWidget(descLbl);

    /* ── Riga stato + azioni ── */
    auto* connRow = new QWidget(this);
    auto* connLay = new QHBoxLayout(connRow);
    connLay->setContentsMargins(0, 0, 0, 0);
    connLay->setSpacing(8);
    m_statusLbl = new QLabel(tr("\xe2\x9a\xaa  RDKit locale"), connRow);
    m_statusLbl->setObjectName("hintLabel");
    auto* checkBtn = new QPushButton(tr("\xf0\x9f\x94\x8d  Verifica rdkit"), connRow);
    checkBtn->setObjectName("actionBtn");
    checkBtn->setFixedWidth(dpiScale(130));
    m_execBtn = new QPushButton(tr("\xf0\x9f\x94\xac  Esegui script RDKit"), connRow);
    m_execBtn->setObjectName("actionBtn");
    m_execBtn->setFixedWidth(dpiScale(170));
    m_execBtn->setEnabled(false);
    connLay->addWidget(m_statusLbl, 1);
    connLay->addWidget(checkBtn);
    connLay->addWidget(m_execBtn);
    lay->addWidget(connRow);

    auto* hintLbl = new QLabel(
        "\xf0\x9f\xa7\xaa <b>RDKit MCP:</b> chemioinformatica Python. "
        "Installa con: <code>pip install rdkit</code>", this);
    hintLbl->setObjectName("hintLabel");
    hintLbl->setWordWrap(true);
    lay->addWidget(hintLbl);

    /* ── Riga azione/modello ── */
    auto* toolRow = new QWidget(this);
    auto* toolLay = new QHBoxLayout(toolRow);
    toolLay->setContentsMargins(0, 0, 0, 0);
    toolLay->setSpacing(8);
    m_action = new QComboBox(toolRow);
    for (int i = 0; kActions[i]; i++)
        m_action->addItem(QString::fromUtf8(kActions[i]));
    m_model = new ModelComboBox(m_ai, toolRow);
    toolLay->addWidget(new QLabel(tr("Analisi:"), toolRow));
    toolLay->addWidget(m_action, 1);
    toolLay->addWidget(new QLabel(tr("Modello:"), toolRow));
    toolLay->addWidget(m_model, 1);
    lay->addWidget(toolRow);

    m_input = new QTextEdit(this);
    m_input->setPlaceholderText(
        tr("Descrivi la molecola o l'analisi...\n"
        "Es: 'Calcola MW, LogP per la caffeina (SMILES: Cn1cnc2c1c(=O)n(c(=O)n2C)C)'"));
    m_input->setFixedHeight(dpiScale(80));
    lay->addWidget(m_input);

    auto* btnRow = new QWidget(this);
    auto* btnLay = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(0, 0, 0, 0);
    m_runBtn  = new QPushButton(tr("\xf0\x9f\xa4\x96  Genera script RDKit"), btnRow);
    m_runBtn->setObjectName("actionBtn");
    m_stopBtn = new QPushButton(tr("\xe2\x8f\xb9  Stop"), btnRow);
    m_stopBtn->setObjectName("actionBtn");
    m_stopBtn->setProperty("danger", true);
    m_stopBtn->setEnabled(false);
    btnLay->addWidget(m_runBtn);
    btnLay->addWidget(m_stopBtn);
    btnLay->addStretch();
    lay->addWidget(btnRow);

    m_output = new QTextEdit(this);
    m_output->setReadOnly(true);
    m_output->setObjectName("outputView");
    m_output->setPlaceholderText(tr("Script Python RDKit appare qui..."));
    lay->addWidget(m_output, 1);

    connect(checkBtn,  &QPushButton::clicked, this, &RDKitWidget::onCheckClicked);
    connect(m_execBtn, &QPushButton::clicked, this, &RDKitWidget::onExecClicked);
    connect(m_runBtn,  &QPushButton::clicked, this, &RDKitWidget::onRunClicked);
    connect(m_stopBtn, &QPushButton::clicked, this, &RDKitWidget::onStopClicked);
}

void RDKitWidget::onCheckClicked()
{
    auto* proc = new QProcess(this);
    connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &RDKitWidget::onCheckFinished);
    connect(proc, &QProcess::errorOccurred, this,
        [this](QProcess::ProcessError err) {
            if (err == QProcess::FailedToStart)
                m_statusLbl->setText(tr("\xe2\x9d\x8c  Python non trovato"));
        });
    proc->start(P::findPython(), {"-c", "import rdkit; print('rdkit', rdkit.__version__)"});
}

void RDKitWidget::onCheckFinished(int code, QProcess::ExitStatus)
{
    auto* proc = qobject_cast<QProcess*>(sender());
    if (!proc) return;
    const QString out = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
    m_statusLbl->setText(code == 0
        ? "\xe2\x9c\x85  " + out
        : "\xe2\x9d\x8c  rdkit non trovato \xe2\x80\x94 pip install rdkit");
    proc->deleteLater();
}

void RDKitWidget::onExecClicked()
{
    runSciScript(m_code, false, m_statusLbl, m_execBtn, m_output, m_proc, this);
}

void RDKitWidget::onRunClicked()
{
    const int idx = m_action->currentIndex();
    if (idx < 0 || !kSys[idx]) return;
    avviaSci(QString::fromUtf8(kSys[idx]), m_input->toPlainText(),
             m_output, m_runBtn, m_stopBtn, m_model, m_execBtn, &m_code, m_statusLbl);
}

void RDKitWidget::onStopClicked()
{
    m_ai->abort();
    m_runBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
}
