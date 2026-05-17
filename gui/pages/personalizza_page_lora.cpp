/* ══════════════════════════════════════════════════════════════
   personalizza_page_lora.cpp — Fine-tuning LoRA integrato

   Pipeline supportata:
     1. llama-finetune (llama.cpp) — CPU/GPU, nessuna dipendenza Python
     2. unsloth (Python, richiede GPU CUDA) — guida + link
     3. axolotl / LLaMA-Factory — guida + link

   Il dataset deve essere un file JSONL con righe {"text": "..."}
   Il risultato è un file .gguf fine-tunato (adapter + merge).
   ══════════════════════════════════════════════════════════════ */
#include "personalizza_page.h"
#include "../prismalux_paths.h"
namespace P = PrismaluxPaths;

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QTextEdit>
#include <QGroupBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QScrollArea>
#include <QFrame>
#include <QProcess>
#include <QStandardPaths>
#include <QCheckBox>
#include <QComboBox>
#include <QTabWidget>
#include <QFormLayout>

/* ══════════════════════════════════════════════════════════════
   buildLoraTab — UI fine-tuning LoRA
   ══════════════════════════════════════════════════════════════ */
QWidget* PersonalizzaPage::buildLoraTab()
{
    auto* page   = new QWidget;
    auto* outer  = new QScrollArea;
    outer->setWidgetResizable(true);
    outer->setFrameShape(QFrame::NoFrame);
    auto* inner  = new QWidget;
    auto* mainLay = new QVBoxLayout(inner);
    mainLay->setContentsMargins(16, 16, 16, 16);
    mainLay->setSpacing(12);
    outer->setWidget(inner);

    /* ── Titolo e descrizione ── */
    auto* title = new QLabel(
        "\xf0\x9f\xa7\xa0  <b>Fine-tuning LoRA</b> \xe2\x80\x94 "
        "Specializza un modello sul tuo dominio", inner);
    title->setObjectName("sectionTitle");
    title->setTextFormat(Qt::RichText);
    mainLay->addWidget(title);

    auto* desc = new QLabel(
        "Addestra un adattatore LoRA su dati personalizzati senza modificare i pesi originali.<br>"
        "Il modello risultante risponde meglio al tuo dominio (medico, legale, codice, ecc.).<br>"
        "<span style='color:#94a3b8;'>"
        "Dataset: file JSONL con righe <code>{\"text\": \"...\"}</code> &nbsp;|&nbsp; "
        "Risultato: adapter .bin mergeable con il modello base.</span>",
        inner);
    desc->setWordWrap(true);
    desc->setTextFormat(Qt::RichText);
    desc->setObjectName("hintLabel");
    mainLay->addWidget(desc);

    /* ── Sub-tabs: llama-finetune | unsloth | Guida ── */
    auto* subTabs = new QTabWidget(inner);
    subTabs->setObjectName("mathSubTabs");
    mainLay->addWidget(subTabs, 1);

    /* ════════════════════════ TAB 1: llama-finetune ══════════════════════ */
    {
        auto* tab   = new QWidget;
        auto* tLay  = new QVBoxLayout(tab);
        tLay->setContentsMargins(12, 12, 12, 8);
        tLay->setSpacing(10);

        auto* info = new QLabel(
            "\xf0\x9f\xa6\x99  <b>llama-finetune</b> \xe2\x80\x94 "
            "CPU+GPU nativo llama.cpp \xe2\x80\x94 nessuna GPU richiesta (ma consigliata).<br>"
            "Richiede: <code>llama-finetune</code> compilato in "
            "<code>Prismalux/whisper.cpp/../llama.cpp/build/bin/</code>",
            tab);
        info->setWordWrap(true);
        info->setTextFormat(Qt::RichText);
        info->setObjectName("hintLabel");
        tLay->addWidget(info);

        /* Modello base */
        auto* grpModel = new QGroupBox("\xf0\x9f\x93\x81  Modello base (.gguf)", tab);
        grpModel->setObjectName("cardGroup");
        auto* mLay = new QHBoxLayout(grpModel);
        m_loraModelEdit = new QLineEdit(grpModel);
        m_loraModelEdit->setPlaceholderText("Path assoluto al modello GGUF base...");
        auto* modelBtn  = new QPushButton("\xf0\x9f\x93\x82", grpModel);
        modelBtn->setFixedWidth(32);
        modelBtn->setToolTip("Seleziona file .gguf");
        mLay->addWidget(m_loraModelEdit, 1);
        mLay->addWidget(modelBtn);
        connect(modelBtn, &QPushButton::clicked,
                this, &PersonalizzaPage::onModelBtnClicked);
        tLay->addWidget(grpModel);

        /* Dataset */
        auto* grpData = new QGroupBox("\xf0\x9f\x93\x8a  Dataset di training (.jsonl)", tab);
        grpData->setObjectName("cardGroup");
        auto* dLay = new QHBoxLayout(grpData);
        m_loraDataEdit = new QLineEdit(grpData);
        m_loraDataEdit->setPlaceholderText("Path al file JSONL (righe: {\"text\": \"...\"})");
        auto* dataBtn  = new QPushButton("\xf0\x9f\x93\x82", grpData);
        dataBtn->setFixedWidth(32);
        dataBtn->setToolTip("Seleziona dataset JSONL");
        dLay->addWidget(m_loraDataEdit, 1);
        dLay->addWidget(dataBtn);
        connect(dataBtn, &QPushButton::clicked,
                this, &PersonalizzaPage::onDataBtnClicked);
        tLay->addWidget(grpData);

        /* Iperparametri */
        auto* grpHyper = new QGroupBox("\xe2\x9a\x99\xef\xb8\x8f  Iperparametri LoRA", tab);
        grpHyper->setObjectName("cardGroup");
        auto* fLay = new QFormLayout(grpHyper);
        fLay->setLabelAlignment(Qt::AlignRight);

        m_loraSpinEpochs = new QSpinBox(grpHyper);
        m_loraSpinEpochs->setRange(1, 100); m_loraSpinEpochs->setValue(3);
        m_loraSpinEpochs->setToolTip("Numero di passaggi sull'intero dataset");
        fLay->addRow("Epoche:", m_loraSpinEpochs);

        m_loraSpinR = new QSpinBox(grpHyper);
        m_loraSpinR->setRange(1, 256); m_loraSpinR->setValue(8);
        m_loraSpinR->setToolTip("Rango LoRA: valori bassi = meno parametri, addestramento più veloce");
        fLay->addRow("Rango LoRA (r):", m_loraSpinR);

        m_loraSpinAlpha = new QSpinBox(grpHyper);
        m_loraSpinAlpha->setRange(1, 512); m_loraSpinAlpha->setValue(32);
        m_loraSpinAlpha->setToolTip("Alpha LoRA: scala l'impatto dell'adapter (tipico: 2-4× rank)");
        fLay->addRow("Alpha LoRA:", m_loraSpinAlpha);

        m_loraDspLr = new QDoubleSpinBox(grpHyper);
        m_loraDspLr->setRange(1e-6, 1e-2); m_loraDspLr->setValue(1e-4);
        m_loraDspLr->setDecimals(7); m_loraDspLr->setSingleStep(1e-5);
        m_loraDspLr->setToolTip("Learning rate (tipico: 1e-4 – 3e-4)");
        fLay->addRow("Learning rate:", m_loraDspLr);

        m_loraSpinBatch = new QSpinBox(grpHyper);
        m_loraSpinBatch->setRange(1, 64); m_loraSpinBatch->setValue(4);
        m_loraSpinBatch->setToolTip("Batch size: su CPU limitare a 1-4");
        fLay->addRow("Batch size:", m_loraSpinBatch);

        m_loraSpinCtx = new QSpinBox(grpHyper);
        m_loraSpinCtx->setRange(128, 32768); m_loraSpinCtx->setValue(512);
        m_loraSpinCtx->setSingleStep(128);
        m_loraSpinCtx->setToolTip("Lunghezza contesto per ogni esempio");
        fLay->addRow("Context size:", m_loraSpinCtx);

        tLay->addWidget(grpHyper);

        /* Output */
        auto* grpOut = new QGroupBox("\xf0\x9f\x92\xbe  Cartella output", tab);
        grpOut->setObjectName("cardGroup");
        auto* oLay = new QHBoxLayout(grpOut);
        m_loraOutEdit = new QLineEdit(grpOut);
        m_loraOutEdit->setText(QDir::homePath() + "/lora_output");
        auto* outBtn  = new QPushButton("\xf0\x9f\x93\x82", grpOut);
        outBtn->setFixedWidth(32);
        oLay->addWidget(m_loraOutEdit, 1);
        oLay->addWidget(outBtn);
        connect(outBtn, &QPushButton::clicked,
                this, &PersonalizzaPage::onOutBtnClicked);
        tLay->addWidget(grpOut);

        /* Pulsanti avvio/stop */
        auto* btnRow = new QWidget(tab);
        auto* bLay   = new QHBoxLayout(btnRow);
        bLay->setContentsMargins(0, 0, 0, 0);
        m_loraStartBtn = new QPushButton(
            "\xf0\x9f\x9a\x80  Avvia Fine-tuning", btnRow);
        m_loraStartBtn->setObjectName("actionBtn");
        m_loraStopBtn  = new QPushButton(
            "\xe2\x9c\x8b  Interrompi", btnRow);
        m_loraStopBtn->setObjectName("actionBtn");
        m_loraStopBtn->setEnabled(false);
        bLay->addWidget(m_loraStartBtn);
        bLay->addWidget(m_loraStopBtn);
        bLay->addStretch(1);
        tLay->addWidget(btnRow);

        /* Log live */
        m_loraLog = makeLog(
            "\xf0\x9f\xa7\xa0  Il log del training apparir\xc3\xa0 qui...\n"
            "Ogni riga mostra la loss scendere man mano che il modello impara.");
        m_loraLog->setFixedHeight(180);
        tLay->addWidget(m_loraLog);

        connect(m_loraStartBtn, &QPushButton::clicked,
                this, &PersonalizzaPage::onLoraStartBtnClicked);
        connect(m_loraStopBtn,  &QPushButton::clicked,
                this, &PersonalizzaPage::onLoraStopBtnClicked);

        subTabs->addTab(tab, "\xf0\x9f\xa6\x99  llama-finetune");
    }

    /* ════════════════════════ TAB 2: unsloth / Python ════════════════════ */
    {
        auto* tab   = new QWidget;
        auto* tLay  = new QVBoxLayout(tab);
        tLay->setContentsMargins(12, 12, 12, 8);
        tLay->setSpacing(10);

        auto* info = new QLabel(
            "\xf0\x9f\x94\xa5  <b>unsloth</b> \xe2\x80\x94 "
            "Fine-tuning LoRA 2-5\xc3\x97 pi\xc3\xb9 veloce con meno RAM.<br>"
            "Richiede: GPU NVIDIA con CUDA, <code>pip install unsloth</code>", tab);
        info->setWordWrap(true);
        info->setTextFormat(Qt::RichText);
        info->setObjectName("hintLabel");
        tLay->addWidget(info);

        /* Genera script Python */
        auto* scriptEdit = new QTextEdit(tab);
        scriptEdit->setObjectName("chatLog");
        scriptEdit->setPlainText(
            "# Script unsloth — fine-tuning LoRA\n"
            "# pip install unsloth\n"
            "from unsloth import FastLanguageModel\n"
            "import torch\n\n"
            "MODEL = \"unsloth/llama-3-8b-bnb-4bit\"  # modello base\n"
            "DATASET = \"/path/to/dataset.jsonl\"       # tuo dataset\n"
            "OUTPUT  = \"./lora_model\"                 # cartella output\n\n"
            "model, tokenizer = FastLanguageModel.from_pretrained(\n"
            "    model_name=MODEL, max_seq_length=2048,\n"
            "    dtype=None, load_in_4bit=True)\n\n"
            "model = FastLanguageModel.get_peft_model(\n"
            "    model, r=8, target_modules=[\"q_proj\",\"v_proj\"],\n"
            "    lora_alpha=32, lora_dropout=0, bias=\"none\",\n"
            "    use_gradient_checkpointing=True)\n\n"
            "from datasets import load_dataset\n"
            "dataset = load_dataset(\"json\", data_files=DATASET, split=\"train\")\n\n"
            "from trl import SFTTrainer\n"
            "from transformers import TrainingArguments\n"
            "trainer = SFTTrainer(\n"
            "    model=model, tokenizer=tokenizer,\n"
            "    train_dataset=dataset,\n"
            "    args=TrainingArguments(\n"
            "        output_dir=OUTPUT, num_train_epochs=3,\n"
            "        per_device_train_batch_size=4,\n"
            "        learning_rate=2e-4, fp16=True))\n"
            "trainer.train()\n"
            "model.save_pretrained(OUTPUT)\n"
            "# Converti in GGUF: python convert_hf_to_gguf.py ./lora_model\n");
        tLay->addWidget(scriptEdit, 1);

        auto* installBtn = new QPushButton(
            "\xf0\x9f\x93\xa6  pip install unsloth trl datasets transformers", tab);
        installBtn->setObjectName("actionBtn");
        m_loraLog2 = makeLog("Log installazione...");
        m_loraLog2->setFixedHeight(100);

        connect(installBtn, &QPushButton::clicked,
                this, &PersonalizzaPage::onLoraInstallBtnClicked);
        tLay->addWidget(installBtn, 0, Qt::AlignLeft);
        tLay->addWidget(m_loraLog2);

        subTabs->addTab(tab, "\xf0\x9f\x94\xa5  unsloth");
    }

    /* ════════════════════════ TAB 3: Guida ══════════════════════════════ */
    {
        auto* tab   = new QWidget;
        auto* tLay  = new QVBoxLayout(tab);
        tLay->setContentsMargins(12, 12, 12, 8);
        tLay->setSpacing(8);

        auto* guide = new QLabel(
            "<h3>\xf0\x9f\x93\x96  Guida al Fine-tuning LoRA</h3>"
            "<h4>Cos\xe2\x80\x99\xc3\xa8 LoRA?</h4>"
            "<p>Low-Rank Adaptation addestra solo una piccola matrice di aggiornamento "
            "invece dei pesi completi. Risultato: adattamento rapido con poca RAM/VRAM.</p>"
            "<h4>Quando usarlo?</h4>"
            "<ul>"
            "<li>Dominio specifico: medico, legale, ingegneristico</li>"
            "<li>Stile di risposta personalizzato</li>"
            "<li>Vocabolario tecnico specifico</li>"
            "<li>Formato output fisso (JSON, report, ecc.)</li>"
            "</ul>"
            "<h4>Formato dataset (JSONL)</h4>"
            "<pre style='background:#1e293b;padding:8px;border-radius:4px;'>"
            "{\"text\": \"### Domanda: Cosa \xc3\xa8 Python?\\n### Risposta: Python \xc3\xa8...\"}\n"
            "{\"text\": \"### Domanda: Spiega LoRA...\\n### Risposta: LoRA \xc3\xa8...\"}"
            "</pre>"
            "<h4>Strumenti alternativi</h4>"
            "<ul>"
            "<li><b>llama.cpp finetune</b>: nessuna GPU richiesta, lento su CPU</li>"
            "<li><b>unsloth</b>: 2-5\xc3\x97 pi\xc3\xb9 veloce, richiede GPU NVIDIA</li>"
            "<li><b>axolotl</b>: produzione, multi-GPU, config YAML</li>"
            "<li><b>LLaMA-Factory</b>: UI web integrata, molti formati</li>"
            "</ul>"
            "<p style='color:#94a3b8;'>Dopo il training, converti l\xe2\x80\x99"
            "adapter in GGUF per usarlo con Ollama o llama-server.</p>",
            tab);
        guide->setWordWrap(true);
        guide->setTextFormat(Qt::RichText);
        guide->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        guide->setObjectName("hintLabel");
        auto* sc = new QScrollArea(tab);
        sc->setWidget(guide);
        sc->setWidgetResizable(true);
        sc->setFrameShape(QFrame::NoFrame);
        tLay->addWidget(sc, 1);

        subTabs->addTab(tab, "\xf0\x9f\x93\x96  Guida");
    }

    /* wrap in QWidget per compatibilità con addTab */
    auto* wrapper = new QWidget;
    auto* wl = new QVBoxLayout(wrapper);
    wl->setContentsMargins(0, 0, 0, 0);
    wl->addWidget(outer);
    return wrapper;
}

/* ══════════════════════════════════════════════════════════════
   SLOT — LoRA: selezione file
   ══════════════════════════════════════════════════════════════ */
void PersonalizzaPage::onModelBtnClicked() {
    if (!m_loraModelEdit) return;
    const QString f = QFileDialog::getOpenFileName(
        this, "Seleziona modello base", P::root(),
        "Modelli GGUF (*.gguf)");
    if (!f.isEmpty()) m_loraModelEdit->setText(f);
}

void PersonalizzaPage::onDataBtnClicked() {
    if (!m_loraDataEdit) return;
    const QString f = QFileDialog::getOpenFileName(
        this, "Seleziona dataset", QDir::homePath(),
        "Dataset JSONL (*.jsonl *.json);;Tutti i file (*)");
    if (!f.isEmpty()) m_loraDataEdit->setText(f);
}

void PersonalizzaPage::onOutBtnClicked() {
    if (!m_loraOutEdit) return;
    const QString d = QFileDialog::getExistingDirectory(
        this, "Cartella output", QDir::homePath());
    if (!d.isEmpty()) m_loraOutEdit->setText(d);
}

/* ══════════════════════════════════════════════════════════════
   SLOT — LoRA: avvio / stop fine-tuning
   ══════════════════════════════════════════════════════════════ */
void PersonalizzaPage::onLoraStartBtnClicked() {
    if (!m_loraLog || !m_loraStartBtn || !m_loraStopBtn) return;
    if (!m_loraModelEdit || !m_loraDataEdit || !m_loraOutEdit) return;

    const QString model   = m_loraModelEdit->text().trimmed();
    const QString dataset = m_loraDataEdit->text().trimmed();
    const QString outDir  = m_loraOutEdit->text().trimmed();

    if (model.isEmpty() || !QFileInfo::exists(model)) {
        m_loraLog->append("\xe2\x9d\x8c  Seleziona un modello GGUF valido.");
        return;
    }
    if (dataset.isEmpty() || !QFileInfo::exists(dataset)) {
        m_loraLog->append("\xe2\x9d\x8c  Seleziona un dataset JSONL valido.");
        return;
    }

    /* Cerca llama-finetune nel build di llama.cpp */
    QStringList candidates = {
        P::root() + "/llama.cpp/build/bin/llama-finetune",
        P::root() + "/llama.cpp/build/llama-finetune",
        QStandardPaths::findExecutable("llama-finetune"),
    };
    QString ftBin;
    for (const QString& c : candidates)
        if (!c.isEmpty() && QFileInfo::exists(c)) { ftBin = c; break; }

    if (ftBin.isEmpty()) {
        m_loraLog->append(
            "\xe2\x9d\x8c  <b>llama-finetune</b> non trovato.<br>"
            "Compila llama.cpp con: <code>cmake -B build -DLLAMA_FINETUNE=ON && "
            "cmake --build build --target llama-finetune -j$(nproc)</code><br>"
            "Oppure usa la tab <b>unsloth</b> per training con GPU.");
        return;
    }

    QDir().mkpath(outDir);
    const QString adapterOut = outDir + "/lora_adapter.bin";

    QStringList args = {
        "--model-base",    model,
        "--train-data",    dataset,
        "--lora-out",      adapterOut,
        "--ctx",           QString::number(m_loraSpinCtx->value()),
        "--epochs",        QString::number(m_loraSpinEpochs->value()),
        "--lora-r",        QString::number(m_loraSpinR->value()),
        "--lora-alpha",    QString::number(m_loraSpinAlpha->value()),
        "--adam-alpha",    QString::number(m_loraDspLr->value()),
        "--batch",         QString::number(m_loraSpinBatch->value()),
        "--use-checkpointing",
    };

    m_loraLog->clear();
    m_loraLog->append(
        "\xf0\x9f\x9a\x80  Avvio fine-tuning LoRA...\n"
        "\xf0\x9f\x93\x81  Modello: " + QFileInfo(model).fileName() + "\n"
        "\xf0\x9f\x93\x8a  Dataset: " + QFileInfo(dataset).fileName() + "\n"
        "\xf0\x9f\x92\xbe  Output: " + adapterOut + "\n");
    /* Salva adapterOut come proprietà per recuperarla nel finished */
    m_loraProc = new QProcess(this);
    m_loraProc->setProcessChannelMode(QProcess::MergedChannels);
    m_loraProc->setProperty("_adapterOut", adapterOut);
    connect(m_loraProc, &QProcess::readyRead,
            this, &PersonalizzaPage::onLoraProcReadyRead);
    connect(m_loraProc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &PersonalizzaPage::onLoraProcFinished);

    m_loraProc->start(ftBin, args);
    if (!m_loraProc->waitForStarted(3000)) {
        m_loraLog->append("\xe2\x9d\x8c  Impossibile avviare llama-finetune.");
        m_loraProc->deleteLater();
        m_loraProc = nullptr;
        return;
    }
    m_loraStartBtn->setEnabled(false);
    m_loraStopBtn->setEnabled(true);
}

void PersonalizzaPage::onLoraStopBtnClicked() {
    if (m_loraProc) {
        m_loraProc->terminate();
        if (m_loraLog)
            m_loraLog->append("\n\xe2\x9a\xa0  Training interrotto.");
    }
}

void PersonalizzaPage::onLoraProcReadyRead() {
    if (!m_loraProc || !m_loraLog) return;
    m_loraLog->moveCursor(QTextCursor::End);
    m_loraLog->insertPlainText(QString::fromLocal8Bit(m_loraProc->readAll()));
    m_loraLog->ensureCursorVisible();
}

void PersonalizzaPage::onLoraProcFinished(int code, QProcess::ExitStatus) {
    if (!m_loraProc) return;
    const QString adapterOut = m_loraProc->property("_adapterOut").toString();
    m_loraProc->deleteLater();
    m_loraProc = nullptr;
    if (m_loraLog) {
        if (code == 0) {
            m_loraLog->append(
                "\n\xe2\x9c\x85  Fine-tuning completato!\n"
                "\xf0\x9f\x92\xbe  Adapter salvato: " + adapterOut + "\n"
                "\xe2\x84\xb9  Per usarlo: llama-cli --model BASE.gguf "
                "--lora " + adapterOut);
        } else {
            m_loraLog->append(
                "\n\xe2\x9d\x8c  Processo terminato con codice: " +
                QString::number(code));
        }
    }
    if (m_loraStartBtn) m_loraStartBtn->setEnabled(true);
    if (m_loraStopBtn)  m_loraStopBtn->setEnabled(false);
}

/* ══════════════════════════════════════════════════════════════
   SLOT — unsloth: installa dipendenze
   ══════════════════════════════════════════════════════════════ */
void PersonalizzaPage::onLoraInstallBtnClicked() {
    if (!m_loraLog2) return;
    m_loraInstallProc = new QProcess(this);
    m_loraInstallProc->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_loraInstallProc, &QProcess::readyRead,
            this, &PersonalizzaPage::onLoraInstallReadyRead);
    connect(m_loraInstallProc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &PersonalizzaPage::onLoraInstallFinished);
    m_loraLog2->append("\xf0\x9f\x93\xa6  Installazione in corso...\n");
    m_loraInstallProc->start("pip", {"install", "unsloth", "trl", "datasets", "transformers"});
    if (!m_loraInstallProc->waitForStarted(3000)) {
        m_loraLog2->append("\xe2\x9d\x8c  pip non trovato.");
        m_loraInstallProc->deleteLater();
        m_loraInstallProc = nullptr;
    }
}

void PersonalizzaPage::onLoraInstallReadyRead() {
    if (!m_loraInstallProc || !m_loraLog2) return;
    m_loraLog2->moveCursor(QTextCursor::End);
    m_loraLog2->insertPlainText(QString::fromLocal8Bit(m_loraInstallProc->readAll()));
    m_loraLog2->ensureCursorVisible();
}

void PersonalizzaPage::onLoraInstallFinished(int code, QProcess::ExitStatus) {
    if (!m_loraInstallProc || !m_loraLog2) return;
    m_loraLog2->append(code == 0 ? "\xe2\x9c\x85 OK" : "\xe2\x9d\x8c Errore");
    m_loraInstallProc->deleteLater();
    m_loraInstallProc = nullptr;
}
