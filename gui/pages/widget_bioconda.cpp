#include "widget_bioconda.h"
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
    "Sei un esperto di bioinformatica e pipeline Bioconda. "
    "Genera SOLO script Bash o Snakemake eseguibili. "
    "Usa tool Bioconda: bwa, samtools, gatk, blast, fastqc, trimmomatic, bowtie2. "
    "Rispondi SOLO con il blocco script tra ``` e ```.",
    "Sei un esperto di analisi genomica. "
    "Genera SOLO script Bash per una pipeline di allineamento NGS: "
    "FastQC \xe2\x86\x92 Trimmomatic \xe2\x86\x92 BWA mem \xe2\x86\x92 Samtools sort/index. "
    "Rispondi SOLO con codice Bash tra ``` e ```.",
    "Sei un esperto di BLAST e banche dati genomiche. "
    "Genera SOLO script Bash per eseguire BLAST (blastn/blastp) su sequenze FASTA. "
    "Rispondi SOLO con codice Bash tra ``` e ```.",
    "Sei un esperto di variant calling GATK. "
    "Genera SOLO script Bash per variant calling: "
    "HaplotypeCaller \xe2\x86\x92 GenotypeGVCFs \xe2\x86\x92 VQSR/hard filter. "
    "Rispondi SOLO con codice Bash tra ``` e ```.",
    "Sei un esperto di bioinformatica Python (Biopython). "
    "Genera SOLO codice Python con Biopython per parsing FASTA/GenBank/PDB. "
    "Rispondi SOLO con codice Python tra ``` e ```.",
    nullptr
};
static const char* kActions[] = {
    "\xf0\x9f\x8c\xbf  Pipeline NGS alignment",
    "\xf0\x9f\x94\xac  BLAST search",
    "\xf0\x9f\xa7\xac  Variant calling GATK",
    "\xf0\x9f\x90\x8d  Biopython parsing",
    "\xf0\x9f\x94\xa7  Script libero",
    nullptr
};

BiocondaWidget::BiocondaWidget(AiClient* ai, QWidget* parent)
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
        "\xf0\x9f\x8c\xbf  <i>Bioconda \xe2\x80\x94 Repository specializzato di software bioinformatico.</i>", this);
    descLbl->setObjectName("hintLabel");
    descLbl->setTextFormat(Qt::RichText);
    descLbl->setWordWrap(true);
    lay->addWidget(descLbl);

    auto* connRow = new QWidget(this);
    auto* connLay = new QHBoxLayout(connRow);
    connLay->setContentsMargins(0, 0, 0, 0);
    connLay->setSpacing(8);
    m_statusLbl = new QLabel(tr("\xe2\x9a\xaa  Bioconda/conda"), connRow);
    m_statusLbl->setObjectName("hintLabel");
    auto* checkBtn = new QPushButton(tr("\xf0\x9f\x94\x8d  Verifica conda"), connRow);
    checkBtn->setObjectName("actionBtn");
    checkBtn->setFixedWidth(dpiScale(130));
    m_execBtn = new QPushButton(tr("\xf0\x9f\x8c\xbf  Esegui pipeline"), connRow);
    m_execBtn->setObjectName("actionBtn");
    m_execBtn->setFixedWidth(dpiScale(150));
    m_execBtn->setEnabled(false);
    connLay->addWidget(m_statusLbl, 1);
    connLay->addWidget(checkBtn);
    connLay->addWidget(m_execBtn);
    lay->addWidget(connRow);

    auto* hintLbl = new QLabel(
        "\xf0\x9f\x8c\xbf <b>Bioconda MCP:</b> bioinformatica pipeline. "
        "Richiede conda/mamba + canale Bioconda:<br>"
        "<code>conda config --add channels bioconda</code>", this);
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
    toolLay->addWidget(new QLabel(tr("Pipeline:"), toolRow));
    toolLay->addWidget(m_action, 1);
    toolLay->addWidget(new QLabel(tr("Modello:"), toolRow));
    toolLay->addWidget(m_model, 1);
    lay->addWidget(toolRow);

    m_input = new QTextEdit(this);
    m_input->setPlaceholderText(
        "Descrivi la pipeline bioinformatica da creare...\n"
        "Es: 'Pipeline di allineamento WGS: FASTQ input, output BAM sorted e indexed'");
    m_input->setFixedHeight(dpiScale(80));
    lay->addWidget(m_input);

    auto* btnRow = new QWidget(this);
    auto* btnLay = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(0, 0, 0, 0);
    m_runBtn  = new QPushButton(tr("\xf0\x9f\xa4\x96  Genera pipeline"), btnRow);
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
    m_output->setPlaceholderText(tr("Script Bash/Python bioinformatica appare qui..."));
    lay->addWidget(m_output, 1);

    connect(checkBtn,  &QPushButton::clicked, this, &BiocondaWidget::onCheckClicked);
    connect(m_execBtn, &QPushButton::clicked, this, &BiocondaWidget::onExecClicked);
    connect(m_runBtn,  &QPushButton::clicked, this, &BiocondaWidget::onRunClicked);
    connect(m_stopBtn, &QPushButton::clicked, this, &BiocondaWidget::onStopClicked);
}

void BiocondaWidget::onCheckClicked()
{
    auto* proc = new QProcess(this);
    connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &BiocondaWidget::onCheckFinished);
    connect(proc, &QProcess::errorOccurred, this,
        [this](QProcess::ProcessError err) {
            if (err == QProcess::FailedToStart)
                m_statusLbl->setText(tr("\xe2\x9d\x8c  conda non trovato \xe2\x80\x94 installa Miniforge"));
        });
    proc->start("conda", {"--version"});
}

void BiocondaWidget::onCheckFinished(int code, QProcess::ExitStatus)
{
    auto* proc = qobject_cast<QProcess*>(sender());
    if (!proc) return;
    const QString out = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
    m_statusLbl->setText(code == 0
        ? "\xe2\x9c\x85  " + out + " disponibile"
        : "\xe2\x9d\x8c  conda non trovato \xe2\x80\x94 installa Miniforge");
    proc->deleteLater();
}

void BiocondaWidget::onExecClicked()
{
    const bool isBash = m_code.contains("bwa ") || m_code.contains("samtools ")
                     || m_code.contains("gatk ") || m_code.contains("blast")
                     || m_code.startsWith("#!");
    runSciScript(m_code, isBash, m_statusLbl, m_execBtn, m_output, m_proc, this);
}

void BiocondaWidget::onRunClicked()
{
    const int idx = m_action->currentIndex();
    if (idx < 0 || !kSys[idx]) return;
    avviaSci(QString::fromUtf8(kSys[idx]), m_input->toPlainText(),
             m_output, m_runBtn, m_stopBtn, m_model, m_execBtn, &m_code, m_statusLbl);
}

void BiocondaWidget::onStopClicked()
{
    m_ai->abort();
    m_runBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
}
