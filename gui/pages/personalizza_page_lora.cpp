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
        auto* modelEdit = new QLineEdit(grpModel);
        modelEdit->setPlaceholderText("Path assoluto al modello GGUF base...");
        auto* modelBtn  = new QPushButton("\xf0\x9f\x93\x82", grpModel);
        modelBtn->setFixedWidth(32);
        modelBtn->setToolTip("Seleziona file .gguf");
        mLay->addWidget(modelEdit, 1);
        mLay->addWidget(modelBtn);
        connect(modelBtn, &QPushButton::clicked, tab, [modelEdit, tab]{
            const QString f = QFileDialog::getOpenFileName(
                tab, "Seleziona modello base", P::root(),
                "Modelli GGUF (*.gguf)");
            if (!f.isEmpty()) modelEdit->setText(f);
        });
        tLay->addWidget(grpModel);

        /* Dataset */
        auto* grpData = new QGroupBox("\xf0\x9f\x93\x8a  Dataset di training (.jsonl)", tab);
        grpData->setObjectName("cardGroup");
        auto* dLay = new QHBoxLayout(grpData);
        auto* dataEdit = new QLineEdit(grpData);
        dataEdit->setPlaceholderText("Path al file JSONL (righe: {\"text\": \"...\"})");
        auto* dataBtn  = new QPushButton("\xf0\x9f\x93\x82", grpData);
        dataBtn->setFixedWidth(32);
        dataBtn->setToolTip("Seleziona dataset JSONL");
        dLay->addWidget(dataEdit, 1);
        dLay->addWidget(dataBtn);
        connect(dataBtn, &QPushButton::clicked, tab, [dataEdit, tab]{
            const QString f = QFileDialog::getOpenFileName(
                tab, "Seleziona dataset", QDir::homePath(),
                "Dataset JSONL (*.jsonl *.json);;Tutti i file (*)");
            if (!f.isEmpty()) dataEdit->setText(f);
        });
        tLay->addWidget(grpData);

        /* Iperparametri */
        auto* grpHyper = new QGroupBox("\xe2\x9a\x99\xef\xb8\x8f  Iperparametri LoRA", tab);
        grpHyper->setObjectName("cardGroup");
        auto* fLay = new QFormLayout(grpHyper);
        fLay->setLabelAlignment(Qt::AlignRight);

        auto* spinEpochs  = new QSpinBox(grpHyper);
        spinEpochs->setRange(1, 100); spinEpochs->setValue(3);
        spinEpochs->setToolTip("Numero di passaggi sull'intero dataset");
        fLay->addRow("Epoche:", spinEpochs);

        auto* spinR = new QSpinBox(grpHyper);
        spinR->setRange(1, 256); spinR->setValue(8);
        spinR->setToolTip("Rango LoRA: valori bassi = meno parametri, addestramento più veloce");
        fLay->addRow("Rango LoRA (r):", spinR);

        auto* spinAlpha = new QSpinBox(grpHyper);
        spinAlpha->setRange(1, 512); spinAlpha->setValue(32);
        spinAlpha->setToolTip("Alpha LoRA: scala l'impatto dell'adapter (tipico: 2-4× rank)");
        fLay->addRow("Alpha LoRA:", spinAlpha);

        auto* dspLr = new QDoubleSpinBox(grpHyper);
        dspLr->setRange(1e-6, 1e-2); dspLr->setValue(1e-4);
        dspLr->setDecimals(7); dspLr->setSingleStep(1e-5);
        dspLr->setToolTip("Learning rate (tipico: 1e-4 – 3e-4)");
        fLay->addRow("Learning rate:", dspLr);

        auto* spinBatch = new QSpinBox(grpHyper);
        spinBatch->setRange(1, 64); spinBatch->setValue(4);
        spinBatch->setToolTip("Batch size: su CPU limitare a 1-4");
        fLay->addRow("Batch size:", spinBatch);

        auto* spinCtx = new QSpinBox(grpHyper);
        spinCtx->setRange(128, 32768); spinCtx->setValue(512);
        spinCtx->setSingleStep(128);
        spinCtx->setToolTip("Lunghezza contesto per ogni esempio");
        fLay->addRow("Context size:", spinCtx);

        tLay->addWidget(grpHyper);

        /* Output */
        auto* grpOut = new QGroupBox("\xf0\x9f\x92\xbe  Cartella output", tab);
        grpOut->setObjectName("cardGroup");
        auto* oLay = new QHBoxLayout(grpOut);
        auto* outEdit = new QLineEdit(grpOut);
        outEdit->setText(QDir::homePath() + "/lora_output");
        auto* outBtn  = new QPushButton("\xf0\x9f\x93\x82", grpOut);
        outBtn->setFixedWidth(32);
        oLay->addWidget(outEdit, 1);
        oLay->addWidget(outBtn);
        connect(outBtn, &QPushButton::clicked, tab, [outEdit, tab]{
            const QString d = QFileDialog::getExistingDirectory(
                tab, "Cartella output", QDir::homePath());
            if (!d.isEmpty()) outEdit->setText(d);
        });
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

        /* ── Avvio fine-tuning ── */
        connect(m_loraStartBtn, &QPushButton::clicked, this,
        [this, modelEdit, dataEdit, outEdit,
         spinEpochs, spinR, spinAlpha, dspLr, spinBatch, spinCtx]
        {
            const QString model   = modelEdit->text().trimmed();
            const QString dataset = dataEdit->text().trimmed();
            const QString outDir  = outEdit->text().trimmed();

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
                "--ctx",           QString::number(spinCtx->value()),
                "--epochs",        QString::number(spinEpochs->value()),
                "--lora-r",        QString::number(spinR->value()),
                "--lora-alpha",    QString::number(spinAlpha->value()),
                "--adam-alpha",    QString::number(dspLr->value()),
                "--batch",         QString::number(spinBatch->value()),
                "--use-checkpointing",
            };

            m_loraLog->clear();
            m_loraLog->append(
                "\xf0\x9f\x9a\x80  Avvio fine-tuning LoRA...\n"
                "\xf0\x9f\x93\x81  Modello: " + QFileInfo(model).fileName() + "\n"
                "\xf0\x9f\x93\x8a  Dataset: " + QFileInfo(dataset).fileName() + "\n"
                "\xf0\x9f\x92\xbe  Output: " + adapterOut + "\n");

            m_loraProc = new QProcess(this);
            m_loraProc->setProcessChannelMode(QProcess::MergedChannels);
            connect(m_loraProc, &QProcess::readyRead, this, [this]{
                m_loraLog->moveCursor(QTextCursor::End);
                m_loraLog->insertPlainText(
                    QString::fromLocal8Bit(m_loraProc->readAll()));
                m_loraLog->ensureCursorVisible();
            });
            connect(m_loraProc,
                    QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                    this, [this, adapterOut](int code, QProcess::ExitStatus){
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
                m_loraStartBtn->setEnabled(true);
                m_loraStopBtn->setEnabled(false);
                m_loraProc->deleteLater();
                m_loraProc = nullptr;
            });

            m_loraProc->start(ftBin, args);
            if (!m_loraProc->waitForStarted(3000)) {
                m_loraLog->append("\xe2\x9d\x8c  Impossibile avviare llama-finetune.");
                m_loraProc->deleteLater();
                m_loraProc = nullptr;
                return;
            }
            m_loraStartBtn->setEnabled(false);
            m_loraStopBtn->setEnabled(true);
        });

        connect(m_loraStopBtn, &QPushButton::clicked, this, [this]{
            if (m_loraProc) {
                m_loraProc->terminate();
                m_loraLog->append("\n\xe2\x9a\xa0  Training interrotto.");
            }
        });

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
        auto* loraLog2 = makeLog("Log installazione...");
        loraLog2->setFixedHeight(100);

        connect(installBtn, &QPushButton::clicked, this, [this, loraLog2]{
            auto* proc = new QProcess(this);
            proc->setProcessChannelMode(QProcess::MergedChannels);
            connect(proc, &QProcess::readyRead, this, [proc, loraLog2]{
                loraLog2->moveCursor(QTextCursor::End);
                loraLog2->insertPlainText(
                    QString::fromLocal8Bit(proc->readAll()));
                loraLog2->ensureCursorVisible();
            });
            connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                    this, [proc, loraLog2](int code, QProcess::ExitStatus){
                loraLog2->append(code == 0 ? "\xe2\x9c\x85 OK" : "\xe2\x9d\x8c Errore");
                proc->deleteLater();
            });
            loraLog2->append("\xf0\x9f\x93\xa6  Installazione in corso...\n");
            proc->start("pip", {"install", "unsloth", "trl", "datasets", "transformers"});
            if (!proc->waitForStarted(3000)) {
                loraLog2->append("\xe2\x9d\x8c  pip non trovato.");
                proc->deleteLater();
            }
        });
        tLay->addWidget(installBtn, 0, Qt::AlignLeft);
        tLay->addWidget(loraLog2);

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
