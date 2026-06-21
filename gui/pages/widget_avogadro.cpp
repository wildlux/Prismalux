#include "widget_avogadro.h"
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
namespace P = PrismaluxPaths;

static const char* kSys[] = {
    "Sei un esperto di Avogadro2 e chemioinformatica Python. "
    "Genera SOLO codice Python che usa avogadro (avogadro.core, avogadro.io, avogadro.calc). "
    "Oppure genera file molecolari (XYZ, SDF, PDB) ben formattati. "
    "Rispondi SOLO con il blocco codice tra ``` e ```.",
    "Sei un esperto di Avogadro2. "
    "Genera SOLO codice Python avogadro per caricare una molecola da SMILES o file "
    "e calcolarne energia con MMFF o UFF. Rispondi SOLO con codice Python tra ``` e ```.",
    "Sei un esperto di Avogadro2. "
    "Genera SOLO un file XYZ ben formato della molecola descritta, "
    "con coordinate ottimizzate approssimative. Rispondi SOLO con il blocco XYZ tra ``` e ```.",
    "Sei un esperto di chimica computazionale e Avogadro2. "
    "Genera SOLO codice Python per ottimizzare la geometria molecolare e salvare in SDF. "
    "Rispondi SOLO con codice Python tra ``` e ```.",
    "Sei un esperto di Avogadro2. "
    "Genera SOLO codice Python avogadro libero. "
    "Rispondi SOLO con il blocco codice tra ``` e ```.",
    nullptr
};
static const char* kActions[] = {
    "\xf0\x9f\xa7\xaa  Carica & calcola",
    "\xf0\x9f\x94\x8b  Energia MMFF/UFF",
    "\xf0\x9f\x93\x90  Genera file XYZ",
    "\xf0\x9f\x94\xa7  Ottimizza geometria",
    "\xf0\x9f\x90\x8d  Script libero",
    nullptr
};

AvogadroWidget::AvogadroWidget(AiClient* ai, QWidget* parent)
    : AiScriptWidget(ai, parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(6);

    auto* descLbl = new QLabel(
        "\xf0\x9f\xa7\xaa  <i>Avogadro \xe2\x80\x94 Editor molecolare 3D open-source.</i>", this);
    descLbl->setObjectName("hintLabel");
    descLbl->setTextFormat(Qt::RichText);
    descLbl->setWordWrap(true);
    lay->addWidget(descLbl);

    auto* connRow = new QWidget(this);
    auto* connLay = new QHBoxLayout(connRow);
    connLay->setContentsMargins(0, 0, 0, 0);
    connLay->setSpacing(8);
    m_statusLbl = new QLabel("\xe2\x9a\xaa  Avogadro locale", connRow);
    m_statusLbl->setObjectName("hintLabel");
    auto* checkBtn = new QPushButton("\xf0\x9f\x94\x8d  Verifica avogadro", connRow);
    checkBtn->setObjectName("actionBtn");
    checkBtn->setFixedWidth(dpiScale(150));
    m_execBtn = new QPushButton("\xf0\x9f\xa7\xaa  Esegui script", connRow);
    m_execBtn->setObjectName("actionBtn");
    m_execBtn->setFixedWidth(dpiScale(150));
    m_execBtn->setEnabled(false);
    connLay->addWidget(m_statusLbl, 1);
    connLay->addWidget(checkBtn);
    connLay->addWidget(m_execBtn);
    lay->addWidget(connRow);

    auto* hintLbl = new QLabel(
        "\xf0\x9f\xa7\xaa <b>Avogadro MCP:</b> modellazione molecolare 3D Python. "
        "Installa con: <code>pip install avogadro</code>", this);
    hintLbl->setObjectName("hintLabel");
    hintLbl->setWordWrap(true);
    lay->addWidget(hintLbl);

    auto* toolRow = new QWidget(this);
    auto* toolLay = new QHBoxLayout(toolRow);
    toolLay->setContentsMargins(0, 0, 0, 0);
    toolLay->setSpacing(8);
    m_action = new QComboBox(toolRow);
    for (int i = 0; kActions[i]; i++)
        m_action->addItem(QString::fromUtf8(kActions[i]));
    m_model = new ModelComboBox(m_ai, toolRow);
    toolLay->addWidget(new QLabel("Azione:", toolRow));
    toolLay->addWidget(m_action, 1);
    toolLay->addWidget(new QLabel("Modello:", toolRow));
    toolLay->addWidget(m_model, 1);
    lay->addWidget(toolRow);

    m_input = new QTextEdit(this);
    m_input->setPlaceholderText(
        "Descrivi la molecola da modellare...\n"
        "Es: 'Genera la struttura 3D ottimizzata dell'acido acetilsalicilico (aspirina)'");
    m_input->setFixedHeight(dpiScale(80));
    lay->addWidget(m_input);

    auto* btnRow = new QWidget(this);
    auto* btnLay = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(0, 0, 0, 0);
    m_runBtn  = new QPushButton("\xf0\x9f\xa4\x96  Genera script Avogadro", btnRow);
    m_runBtn->setObjectName("actionBtn");
    m_stopBtn = new QPushButton("\xe2\x8f\xb9  Stop", btnRow);
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
    m_output->setPlaceholderText(tr("Script Python Avogadro appare qui..."));
    lay->addWidget(m_output, 1);

    connect(checkBtn,  &QPushButton::clicked, this, &AvogadroWidget::onCheckClicked);
    connect(m_execBtn, &QPushButton::clicked, this, &AvogadroWidget::onExecClicked);
    connect(m_runBtn,  &QPushButton::clicked, this, &AvogadroWidget::onRunClicked);
    connect(m_stopBtn, &QPushButton::clicked, this, &AvogadroWidget::onStopClicked);
}

void AvogadroWidget::onCheckClicked()
{
    auto* proc = new QProcess(this);
    connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &AvogadroWidget::onCheckFinished);
    connect(proc, &QProcess::errorOccurred, this,
        [this](QProcess::ProcessError err) {
            if (err == QProcess::FailedToStart)
                m_statusLbl->setText(tr("\xe2\x9d\x8c  Python non trovato"));
        });
    proc->start(P::findPython(), {"-c", "import avogadro; print('avogadro OK')"});
}

void AvogadroWidget::onCheckFinished(int code, QProcess::ExitStatus)
{
    auto* proc = qobject_cast<QProcess*>(sender());
    if (!proc) return;
    m_statusLbl->setText(code == 0
        ? "\xe2\x9c\x85  avogadro disponibile"
        : "\xe2\x9d\x8c  avogadro non trovato \xe2\x80\x94 pip install avogadro");
    proc->deleteLater();
}

void AvogadroWidget::onExecClicked()
{
    runSciScript(m_code, false, m_statusLbl, m_execBtn, m_output, m_proc, this);
}

void AvogadroWidget::onRunClicked()
{
    const int idx = m_action->currentIndex();
    if (idx < 0 || !kSys[idx]) return;
    avviaSci(QString::fromUtf8(kSys[idx]), m_input->toPlainText(),
             m_output, m_runBtn, m_stopBtn, m_model, m_execBtn, &m_code, m_statusLbl);
}

void AvogadroWidget::onStopClicked()
{
    m_ai->abort();
    m_runBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
}
