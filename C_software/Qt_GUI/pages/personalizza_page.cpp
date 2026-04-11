#include "personalizza_page.h"
#include "../prismalux_paths.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QFrame>
#include <QPushButton>
#include <QTextEdit>
#include <QLineEdit>
#include <QProcess>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QScrollArea>
#include <QGroupBox>
#include <QMessageBox>
#include <QSplitter>
#include <QUrl>
#include <QProgressBar>
#include <QRegularExpression>
#include <QSettings>
#include <functional>

namespace P = PrismaluxPaths;


/* ──────────────────────────────────────────────────────────────
   Helper: QTextEdit log con placeholder
   ────────────────────────────────────────────────────────────── */
QTextEdit* PersonalizzaPage::makeLog(const QString& placeholder) {
    auto* log = new QTextEdit(this);
    log->setObjectName("chatLog");
    log->setReadOnly(true);
    if (!placeholder.isEmpty()) log->setPlaceholderText(placeholder);
    return log;
}

/* ──────────────────────────────────────────────────────────────
   Helper: avvia QProcess con output live → log, riabilita btn
   ────────────────────────────────────────────────────────────── */
void PersonalizzaPage::runProc(QProcess* proc, const QString& cmd,
                                QTextEdit* log, QPushButton* btn) {
    log->append(QString("⚙️  %1\n").arg(cmd));
    proc->setProcessChannelMode(QProcess::MergedChannels);

    connect(proc, &QProcess::readyRead, this, [=]{
        log->moveCursor(QTextCursor::End);
        log->insertPlainText(QString::fromLocal8Bit(proc->readAll()));
        log->ensureCursorVisible();
    });
    connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, [=](int code, QProcess::ExitStatus){
        if (code == 0) log->append("\n✅  Completato con successo.");
        else           log->append(QString("\n❌  Uscito con codice %1.").arg(code));
        if (btn) btn->setEnabled(true);
        proc->deleteLater();
    });
#ifdef _WIN32
    proc->start("cmd", {"/c", cmd});
#else
    proc->start("sh", {"-c", cmd});
#endif
    if (!proc->waitForStarted(4000)) {
        log->append("❌  Impossibile avviare il processo. Controlla il PATH.");
        if (btn) btn->setEnabled(true);
        proc->deleteLater();
    }
}


/* ══════════════════════════════════════════════════════════════
   PAGINA 1 — VRAM Benchmark
   ══════════════════════════════════════════════════════════════ */
QWidget* PersonalizzaPage::buildVramBench() {
    auto* page = new QWidget(this);
    auto* lay  = new QVBoxLayout(page);
    lay->setContentsMargins(24, 16, 24, 16); lay->setSpacing(10);
    auto* _hdr = new QLabel("🔬  VRAM Benchmark", page); _hdr->setObjectName("pageTitle"); lay->addWidget(_hdr);

    auto* sub = new QLabel(
        "Misura la VRAM usata da ogni modello Ollama. "
        "Produce <b>vram_profile.json</b> usato dall'agent scheduler.", page);
    sub->setObjectName("cardDesc"); sub->setWordWrap(true);
    lay->addWidget(sub);

    /* ── Controlli ── */
    auto* ctrlW = new QWidget(page);
    auto* ctrlL = new QHBoxLayout(ctrlW);
    ctrlL->setContentsMargins(0,0,0,0); ctrlL->setSpacing(10);

    m_vramBtn = new QPushButton("\xe2\x96\xb6  Avvia Benchmark", page);
    m_vramBtn->setObjectName("actionBtn");

    auto* compileBtn = new QPushButton("\xf0\x9f\x94\xa8  Compila", page);
    compileBtn->setObjectName("actionBtn");
    compileBtn->setToolTip("Esegue: cd C_software && make vram_bench");

    auto* stopBtn = new QPushButton("\xe2\x96\xa0  Stop", page);
    stopBtn->setObjectName("actionBtn"); stopBtn->setProperty("danger","true");
    stopBtn->setEnabled(false);

    auto* info = new QLabel("Esegue <tt>./vram_bench</tt> nella cartella C_software/", page);
    info->setObjectName("cardDesc");

    ctrlL->addWidget(m_vramBtn); ctrlL->addWidget(compileBtn);
    ctrlL->addWidget(stopBtn); ctrlL->addWidget(info, 1);
    lay->addWidget(ctrlW);

    m_vramLog = makeLog(
        "L'output del benchmark appare qui.\n\n"
        "Premi \"\xf0\x9f\x94\xa8  Compila\" se vram_bench non \xc3\xa8 ancora compilato,\n"
        "poi \"\xe2\x96\xb6  Avvia Benchmark\" con Ollama in esecuzione.");
    lay->addWidget(m_vramLog, 1);

    /* ── Compila vram_bench ── */
    connect(compileBtn, &QPushButton::clicked, this, [=]{
        compileBtn->setEnabled(false);
        m_vramBtn->setEnabled(false);
        m_vramLog->clear();
        m_vramLog->append("\xf0\x9f\x94\xa8  Compilazione vram_bench in corso...\n");

        auto* proc = new QProcess(this);
        proc->setWorkingDirectory(P::root() + "/C_software");
        proc->setProcessChannelMode(QProcess::MergedChannels);

        connect(proc, &QProcess::readyRead, this, [=]{
            m_vramLog->moveCursor(QTextCursor::End);
            m_vramLog->insertPlainText(QString::fromLocal8Bit(proc->readAll()));
            m_vramLog->ensureCursorVisible();
        });
        connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                this, [=](int code, QProcess::ExitStatus){
            proc->deleteLater();
            compileBtn->setEnabled(true);
            m_vramBtn->setEnabled(true);
            if (code == 0)
                m_vramLog->append("\n\xe2\x9c\x85  vram_bench compilato! Ora premi Avvia Benchmark.");
            else
                m_vramLog->append(QString("\n\xe2\x9d\x8c  Compilazione fallita (codice %1).\n"
                    "Verifica che gcc sia installato: sudo apt install gcc").arg(code));
        });
        proc->start("make", {"vram_bench"});
        if (!proc->waitForStarted(4000)) {
            m_vramLog->append("\xe2\x9d\x8c  make non trovato. Installa build-essential:\n"
                              "  sudo apt install build-essential");
            compileBtn->setEnabled(true);
            m_vramBtn->setEnabled(true);
            proc->deleteLater();
        }
    });

    /* ── Avvia benchmark ── */
    connect(m_vramBtn, &QPushButton::clicked, this, [=]{
        m_vramLog->clear();
        m_vramBtn->setEnabled(false);
        compileBtn->setEnabled(false);
        stopBtn->setEnabled(true);

        QString bench = P::root() + "/C_software/vram_bench";
#ifdef _WIN32
        bench = QDir::toNativeSeparators(bench) + ".exe";
#endif
        m_vramLog->append(QString("\xf0\x9f\x94\x8d  Eseguo: %1\n").arg(bench));

        m_vramProc = new QProcess(this);
        m_vramProc->setWorkingDirectory(P::root() + "/C_software");
        m_vramProc->setProcessChannelMode(QProcess::MergedChannels);

        connect(m_vramProc, &QProcess::readyRead, this, [=]{
            m_vramLog->moveCursor(QTextCursor::End);
            m_vramLog->insertPlainText(QString::fromLocal8Bit(m_vramProc->readAll()));
            m_vramLog->ensureCursorVisible();
        });
        connect(m_vramProc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                this, [=](int code, QProcess::ExitStatus){
            if (code == 0)
                m_vramLog->append("\n\xe2\x9c\x85  Benchmark completato! Profilo salvato in vram_profile.json");
            else
                m_vramLog->append(QString("\n\xe2\x9d\x8c  Uscito con codice %1.").arg(code));
            m_vramBtn->setEnabled(true);
            compileBtn->setEnabled(true);
            stopBtn->setEnabled(false);
            m_vramProc = nullptr;
        });

        m_vramProc->start(bench, {});
        if (!m_vramProc->waitForStarted(4000)) {
            m_vramLog->append("\xe2\x9d\x8c  vram_bench non trovato.\n"
                              "Premi \"\xf0\x9f\x94\xa8  Compila\" per compilarlo.");
            m_vramBtn->setEnabled(true);
            compileBtn->setEnabled(true);
            stopBtn->setEnabled(false);
            m_vramProc = nullptr;
        }
    });

    connect(stopBtn, &QPushButton::clicked, this, [=]{
        if (m_vramProc) { m_vramProc->kill(); }
        stopBtn->setEnabled(false);
    });

    return page;
}

/* ══════════════════════════════════════════════════════════════
   PAGINA 2 — llama.cpp Studio
   ══════════════════════════════════════════════════════════════ */
QWidget* PersonalizzaPage::buildLlamaStudio() {
    auto* page = new QWidget(this);
    auto* lay  = new QVBoxLayout(page);
    lay->setContentsMargins(24, 16, 24, 16); lay->setSpacing(10);
    auto* _hdr = new QLabel("🦙  llama.cpp Studio", page); _hdr->setObjectName("pageTitle"); lay->addWidget(_hdr);

    /* ── Sotto-stack interno ── */
    m_llamaStack = new QStackedWidget(page);
    lay->addWidget(m_llamaStack, 1);

    /* ──── Sub-page 0: sotto-menu ───────────────────────────── */
    auto* subMenu  = new QWidget;
    auto* subLay   = new QVBoxLayout(subMenu);
    subLay->setContentsMargins(0,4,0,4); subLay->setSpacing(8);

    /* Card "Compila/Aggiorna" contestuale: se il binario esiste → aggiornamento */
    bool binExists = QFileInfo::exists(P::llamaServerBin());

    struct SubItem { QString ico; QString title; QString desc; int pg; };
    QList<SubItem> subs = {
        { binExists ? "\xf0\x9f\x94\x84" : "\xf0\x9f\x94\xa8",
          binExists ? "Aggiorna llama.cpp"  : "Compila llama.cpp",
          binExists ? "Reclona/ricompila llama.cpp per aggiornarlo all'ultima versione."
                    : "Clona e compila llama.cpp — flag GPU (CUDA/ROCm/Vulkan) rilevati automaticamente.",
          1 },
        {"📂", "Gestisci Modelli .gguf",
         "Elenca, elimina, cerca modelli nella cartella models/.", 2},
        {"\xf0\x9f\x93\xa5", "Scarica Modelli Matematica/Logica",
         "Download guidato di GGUF specializzati in matematica e ragionamento logico da HuggingFace.", 3},
    };

    for (auto& s : subs) {
        auto* card = new QFrame(subMenu);
        card->setObjectName("actionCard");
        auto* cl = new QHBoxLayout(card);
        cl->setContentsMargins(14, 12, 14, 12); cl->setSpacing(12);
        auto* ico = new QLabel(s.ico, card); ico->setObjectName("cardIcon"); ico->setFixedWidth(30);
        auto* txt = new QWidget(card);
        auto* tl  = new QVBoxLayout(txt); tl->setContentsMargins(0,0,0,0); tl->setSpacing(3);
        auto* lt  = new QLabel(s.title, txt); lt->setObjectName("cardTitle");
        auto* ld  = new QLabel(s.desc,  txt); ld->setObjectName("cardDesc"); ld->setWordWrap(true);
        tl->addWidget(lt); tl->addWidget(ld);
        auto* btn = new QPushButton("Apri →", card);
        btn->setObjectName("actionBtn"); btn->setFixedWidth(90);
        const int pg = s.pg;
        connect(btn, &QPushButton::clicked, this, [=]{ m_llamaStack->setCurrentIndex(pg); });
        cl->addWidget(ico); cl->addWidget(txt, 1); cl->addWidget(btn);
        subLay->addWidget(card);
    }
    subLay->addStretch(1);

    /* ──── Sub-page 1: Compila llama.cpp ────────────────────── */
    auto* compPg  = new QWidget;
    auto* compLay = new QVBoxLayout(compPg);
    compLay->setContentsMargins(0,0,0,0); compLay->setSpacing(8);

    auto* compTop = new QWidget(compPg);
    auto* compTopL = new QHBoxLayout(compTop);
    compTopL->setContentsMargins(0,0,0,0); compTopL->setSpacing(10);
    auto* backComp = new QPushButton("← Menu llama", compPg);
    backComp->setObjectName("actionBtn");
    connect(backComp, &QPushButton::clicked, this, [=]{ m_llamaStack->setCurrentIndex(0); });
    auto* compTitle = new QLabel(binExists ? "\xf0\x9f\x94\x84  Aggiorna llama.cpp"
                                           : "\xf0\x9f\x94\xa8  Compila llama.cpp", compPg);
    compTitle->setObjectName("cardTitle");
    m_llamaCompBtn = new QPushButton(binExists ? "\xf0\x9f\x94\x84  Aggiorna" : "\xe2\x96\xb6  Compila", compPg);
    m_llamaCompBtn->setObjectName("actionBtn");
    compTopL->addWidget(backComp); compTopL->addWidget(compTitle, 1); compTopL->addWidget(m_llamaCompBtn);
    compLay->addWidget(compTop);

    auto* compInfo = new QLabel(
        binExists
            ? "Esegue <tt>git pull</tt> poi ricompila con gli stessi flag GPU auto-rilevati."
            : "Esegue: <tt>git clone --depth=1 https://github.com/ggerganov/llama.cpp</tt> "
              "poi <tt>cmake -DGGML_NATIVE=ON ...</tt> (flag GPU rilevati automaticamente).",
        compPg);
    compInfo->setObjectName("cardDesc"); compInfo->setWordWrap(true);
    compLay->addWidget(compInfo);

    m_llamaLog = makeLog("Output compilazione llama.cpp...\n\nPrerequisiti: git, cmake, gcc/g++");
    compLay->addWidget(m_llamaLog, 1);

    connect(m_llamaCompBtn, &QPushButton::clicked, this, [=]{
        m_llamaLog->clear();
        m_llamaCompBtn->setEnabled(false);

        QString studio = P::llamaStudioDir();
        QString cloneDir = studio + "/llama.cpp";
        QString buildDir = cloneDir + "/build";

        /* Clone se non esiste, altrimenti aggiorna */
        QString cloneCmd = QDir(cloneDir).exists()
            ? QString("cd \"%1\" && git pull").arg(cloneDir)
            : QString("git clone --depth=1 https://github.com/ggerganov/llama.cpp \"%1\"").arg(cloneDir);

        /* Flag cmake: rileva GPU dal sistema */
        QString gpuFlags = "-DGGML_NATIVE=ON";
#ifdef _WIN32
        gpuFlags += " -DGGML_CUDA=ON";
#else
        if (QFileInfo::exists("/usr/local/cuda/bin/nvcc") ||
            QFileInfo::exists("/usr/bin/nvcc"))
            gpuFlags += " -DGGML_CUDA=ON";
        else if (QFileInfo::exists("/opt/rocm/bin/hipcc"))
            gpuFlags += " -DGGML_HIP=ON";
        else if (QFileInfo::exists("/usr/bin/vulkaninfo"))
            gpuFlags += " -DGGML_VULKAN=ON";
#endif
        QString cmakeCmd = QString("cmake -B \"%1\" -S \"%2\" %3 -DCMAKE_BUILD_TYPE=Release")
                           .arg(buildDir).arg(cloneDir).arg(gpuFlags);
        QString buildCmd = QString("cmake --build \"%1\" --config Release -j$(nproc 2>/dev/null || echo 4)").arg(buildDir);
        QString fullCmd  = cloneCmd + " && " + cmakeCmd + " && " + buildCmd;

        m_llamaLog->append(QString("📦  Flag GPU: %1\n").arg(gpuFlags));
        auto* proc = new QProcess(this);
        connect(proc, &QProcess::readyRead, this, [=]{
            m_llamaLog->moveCursor(QTextCursor::End);
            m_llamaLog->insertPlainText(QString::fromLocal8Bit(proc->readAll()));
            m_llamaLog->ensureCursorVisible();
        });
        connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                this, [=](int code, QProcess::ExitStatus){
            if (code == 0)
                m_llamaLog->append("\n✅  llama.cpp compilato!\n"
                                   "  Binari in: llama_cpp_studio/llama.cpp/build/bin/");
            else
                m_llamaLog->append(QString("\n❌  Errore (code %1).").arg(code));
            m_llamaCompBtn->setEnabled(true);
            proc->deleteLater();
        });
        proc->setProcessChannelMode(QProcess::MergedChannels);
#ifdef _WIN32
        proc->start("cmd", {"/c", fullCmd});
#else
        proc->start("sh", {"-c", fullCmd});
#endif
        if (!proc->waitForStarted(5000)) {
            m_llamaLog->append("❌  Impossibile avviare. Verifica git e cmake nel PATH.");
            m_llamaCompBtn->setEnabled(true);
            proc->deleteLater();
        }
    });

    /* ──── Sub-page 2: Gestisci Modelli ─────────────────────── */
    auto* modPg  = new QWidget;
    auto* modLay = new QVBoxLayout(modPg);
    modLay->setContentsMargins(8, 8, 8, 8); modLay->setSpacing(8);

    /* Toolbar superiore */
    auto* modTop  = new QWidget(modPg);
    auto* modTopL = new QHBoxLayout(modTop);
    modTopL->setContentsMargins(0,0,0,0);
    auto* backMod   = new QPushButton("\xe2\x86\x90 Menu llama", modPg);
    backMod->setObjectName("actionBtn");
    connect(backMod, &QPushButton::clicked, this, [=]{ m_llamaStack->setCurrentIndex(0); });
    auto* modTitle  = new QLabel("\xf0\x9f\x93\x82  Gestione Modelli .gguf", modPg);
    modTitle->setObjectName("cardTitle");
    auto* modDirLbl = new QLabel("", modPg);
    modDirLbl->setObjectName("cardDesc");
    auto* refreshBtn = new QPushButton("\xf0\x9f\x94\x84", modPg);
    refreshBtn->setObjectName("actionBtn"); refreshBtn->setFixedWidth(32);
    refreshBtn->setToolTip("Aggiorna lista");
    modTopL->addWidget(backMod);
    modTopL->addWidget(modTitle, 1);
    modTopL->addWidget(modDirLbl);
    modTopL->addWidget(refreshBtn);
    modLay->addWidget(modTop);

    /* Splitter: lista modelli (sopra) | download (sotto) */
    auto* splitter = new QSplitter(Qt::Vertical, modPg);

    /* ── Lista modelli installati ── */
    auto* listBox     = new QGroupBox("\xf0\x9f\x93\xa6  Modelli installati", splitter);
    auto* listBoxLay  = new QVBoxLayout(listBox);
    auto* scrollArea  = new QScrollArea(listBox);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    auto* listContainer = new QWidget;
    auto* listContLay   = new QVBoxLayout(listContainer);
    listContLay->setSpacing(4);
    listContLay->setContentsMargins(0,0,0,0);
    listContLay->addStretch(1);
    scrollArea->setWidget(listContainer);
    listBoxLay->addWidget(scrollArea);

    /* ── Sezione download ── */
    auto* modDlBox  = new QGroupBox("\xe2\xac\x87  Installa / Aggiorna modello", splitter);
    auto* modDlLay  = new QVBoxLayout(modDlBox);
    modDlLay->setSpacing(6);

    struct PresetModel { const char* name; const char* url; };
    static const PresetModel kPresets[] = {
        {"SmolLM2-135M  (Q4_K_M, ~80 MB)",    "https://huggingface.co/bartowski/SmolLM2-135M-Instruct-GGUF/resolve/main/SmolLM2-135M-Instruct-Q4_K_M.gguf"},
        {"Qwen2.5-0.5B  (Q4_K_M, ~380 MB)",   "https://huggingface.co/Qwen/Qwen2.5-0.5B-Instruct-GGUF/resolve/main/qwen2.5-0.5b-instruct-q4_k_m.gguf"},
        {"Qwen2.5-3B    (Q4_K_M, ~1.9 GB)",   "https://huggingface.co/Qwen/Qwen2.5-3B-Instruct-GGUF/resolve/main/qwen2.5-3b-instruct-q4_k_m.gguf"},
        {"Qwen2.5-7B    (Q4_K_M, ~4.7 GB)",   "https://huggingface.co/Qwen/Qwen2.5-7B-Instruct-GGUF/resolve/main/qwen2.5-7b-instruct-q4_k_m.gguf"},
        {"Phi-3-mini    (Q4_K_M, ~2.2 GB)",   "https://huggingface.co/microsoft/Phi-3-mini-4k-instruct-gguf/resolve/main/Phi-3-mini-4k-instruct-q4.gguf"},
        {"Llama-3.2-3B  (Q4_K_M, ~2.0 GB)",   "https://huggingface.co/bartowski/Llama-3.2-3B-Instruct-GGUF/resolve/main/Llama-3.2-3B-Instruct-Q4_K_M.gguf"},
        {"Mistral-7B    (Q4_K_M, ~4.1 GB)",   "https://huggingface.co/TheBloke/Mistral-7B-Instruct-v0.2-GGUF/resolve/main/mistral-7b-instruct-v0.2.Q4_K_M.gguf"},
    };

    /* Griglia preset */
    auto* modPresetGrid = new QWidget(modDlBox);
    auto* modPresetLay  = new QGridLayout(modPresetGrid);
    modPresetLay->setSpacing(4); modPresetLay->setContentsMargins(0,0,0,0);
    auto* modDlLog  = makeLog("Output wget appare qui...");
    auto* modUrlEdit = new QLineEdit(modDlBox);
    modUrlEdit->setObjectName("chatInput");
    modUrlEdit->setPlaceholderText("URL HuggingFace .gguf personalizzato...");
    auto* modDlBtn  = new QPushButton("\xe2\xac\x87  Scarica URL", modDlBox);
    modDlBtn->setObjectName("actionBtn");

    int modRow = 0, modCol = 0;
    for (const auto& pr : kPresets) {
        auto* btn = new QPushButton(pr.name, modPresetGrid);
        btn->setObjectName("actionBtn");
        QString purl(pr.url);
        connect(btn, &QPushButton::clicked, this, [=]{ modUrlEdit->setText(purl); });
        modPresetLay->addWidget(btn, modRow, modCol);
        modCol++; if (modCol >= 2) { modCol = 0; modRow++; }
    }
    modDlLay->addWidget(modPresetGrid);

    auto* modUrlRow = new QWidget(modDlBox);
    auto* modUrlLay = new QHBoxLayout(modUrlRow);
    modUrlLay->setContentsMargins(0,0,0,0); modUrlLay->setSpacing(6);
    modUrlLay->addWidget(modUrlEdit, 1); modUrlLay->addWidget(modDlBtn);
    modDlLay->addWidget(modUrlRow);

    /* ── Barra di avanzamento download ── */
    auto* dlProgress = new QProgressBar(modDlBox);
    dlProgress->setRange(0, 100);
    dlProgress->setValue(0);
    dlProgress->setVisible(false);
    dlProgress->setTextVisible(true);
    dlProgress->setFormat("%p%  (%v MB / %m MB)");
    dlProgress->setStyleSheet(
        "QProgressBar{background:#1e293b;border:1px solid #334155;border-radius:4px;height:18px;}"
        "QProgressBar::chunk{background:#0ea5e9;border-radius:3px;}");
    modDlLay->addWidget(dlProgress);

    modDlLay->addWidget(modDlLog, 1);

    splitter->addWidget(listBox);
    splitter->addWidget(modDlBox);
    splitter->setSizes({200, 300});
    modLay->addWidget(splitter, 1);

    /* ── Funzione che ricostruisce la lista modelli ── */
    /* Nota: shared_ptr necessario — [=] non può auto-catturare una std::function
       mentre viene ancora inizializzata (stack overflow nel copy constructor) */
    auto refreshHolder = std::make_shared<std::function<void()>>();
    *refreshHolder = [=]{
        /* Rimuovi tutti i widget precedenti tranne lo stretch finale */
        while (listContLay->count() > 1) {
            auto* item = listContLay->takeAt(0);
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }
        QString modelsDir = P::modelsDir();
        modDirLbl->setText(QString("\xf0\x9f\x93\x81  %1").arg(modelsDir));
        QDir d(modelsDir);
        if (!d.exists()) {
            auto* lbl = new QLabel("\xe2\x9a\xa0  Cartella models/ non trovata.", listContainer);
            lbl->setObjectName("cardDesc");
            listContLay->insertWidget(0, lbl);
            return;
        }
        auto files = d.entryInfoList({"*.gguf","*.bin"}, QDir::Files, QDir::Size | QDir::Reversed);
        if (files.isEmpty()) {
            auto* lbl = new QLabel("\xf0\x9f\x8c\xab  Nessun modello trovato — scaricane uno dalla sezione sotto.", listContainer);
            lbl->setObjectName("cardDesc");
            listContLay->insertWidget(0, lbl);
            return;
        }
        for (int i = 0; i < files.size(); i++) {
            const auto& f = files[i];
            double mb = f.size() / (1024.0 * 1024.0);
            QString sizeStr = mb >= 1024.0
                ? QString("%1 GB").arg(mb / 1024.0, 0, 'f', 2)
                : QString("%1 MB").arg(mb, 0, 'f', 0);

            auto* card  = new QFrame(listContainer);
            card->setObjectName("actionCard");
            auto* cl    = new QHBoxLayout(card);
            cl->setContentsMargins(10, 6, 10, 6); cl->setSpacing(10);

            auto* ico  = new QLabel("\xf0\x9f\xa6\x99", card);
            auto* name = new QLabel(f.fileName(), card);
            name->setObjectName("cardTitle");
            auto* size = new QLabel(sizeStr, card);
            size->setObjectName("cardDesc"); size->setFixedWidth(72);
            auto* delBtn = new QPushButton("\xf0\x9f\x97\x91", card);
            delBtn->setObjectName("actionBtn"); delBtn->setFixedWidth(32);
            delBtn->setToolTip("Elimina modello");
            delBtn->setProperty("danger", true);
            QString filePath = f.absoluteFilePath();
            connect(delBtn, &QPushButton::clicked, this, [=]{
                auto ans = QMessageBox::question(this, "Elimina modello",
                    QString("Eliminare permanentemente:\n%1\n(%2)?").arg(f.fileName()).arg(sizeStr),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
                if (ans == QMessageBox::Yes) {
                    QFile::remove(filePath);
                    (*refreshHolder)();
                }
            });

            cl->addWidget(ico);
            cl->addWidget(name, 1);
            cl->addWidget(size);
            cl->addWidget(delBtn);
            listContLay->insertWidget(i, card);
        }
    };

    /* ── Download da URL con progress bar ── */
    connect(modDlBtn, &QPushButton::clicked, this, [=]{
        QString url = modUrlEdit->text().trimmed();
        if (url.isEmpty()) return;
        QString fname = QUrl(url).fileName();
        if (fname.isEmpty()) fname = "modello.gguf";
        const QString dest = P::modelsDir() + "/" + fname;
        QDir().mkpath(P::modelsDir());
        modDlLog->clear();
        modDlLog->append(QString("\xe2\xac\x87  Scaricando %1\n\xe2\x86\x92  %2\n").arg(fname).arg(dest));
        modDlBtn->setEnabled(false);
        dlProgress->setValue(0);
        dlProgress->setVisible(true);

        auto* proc = new QProcess(this);
        proc->setProcessChannelMode(QProcess::MergedChannels);

        /* Parser progress: wget emette righe come "  2% [ =>  ]  42,3M  5,12MB/s  eta 1m 23s"
           curl emette: " 42 19823k   42 8392k    0     0  2583k      0  0:02:07  0:00:03  0:02:04 2587k" */
        connect(proc, &QProcess::readyRead, this, [proc, modDlLog, dlProgress]{
            const QString raw = QString::fromUtf8(proc->readAll());
            /* Cerca percentuale nel formato wget/curl */
            static const QRegularExpression rePct(R"((\d{1,3})\s*%)");
            const auto match = rePct.match(raw);
            if (match.hasMatch()) {
                const int pct = match.captured(1).toInt();
                if (pct >= 0 && pct <= 100) dlProgress->setValue(pct);
            }
            /* Mostra ultima riga non vuota nel log */
            const QStringList lines = raw.split('\n', Qt::SkipEmptyParts);
            if (!lines.isEmpty()) {
                const QString last = lines.last().trimmed();
                if (!last.isEmpty()) modDlLog->append(last);
            }
        });

        connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                this, [=](int code, QProcess::ExitStatus){
            proc->deleteLater();
            modDlBtn->setEnabled(true);
            if (code == 0) {
                dlProgress->setValue(100);
                modDlLog->append("\n\xe2\x9c\x85  Download completato!");
                (*refreshHolder)();
            } else {
                dlProgress->setVisible(false);
                modDlLog->append("\n\xe2\x9d\x8c  Download fallito o interrotto.");
            }
        });

        proc->start("wget", {"-c", "--show-progress", "-O", dest, url});
        if (!proc->waitForStarted(3000)) {
            proc->deleteLater();
            /* Fallback curl */
            auto* curl = new QProcess(this);
            curl->setProcessChannelMode(QProcess::MergedChannels);
            connect(curl, &QProcess::readyRead, this, [curl, modDlLog, dlProgress]{
                const QString raw = QString::fromUtf8(curl->readAll());
                static const QRegularExpression rePct(R"(\s(\d{1,3})\s)");
                const auto match = rePct.match(raw);
                if (match.hasMatch()) {
                    const int pct = match.captured(1).toInt();
                    if (pct >= 0 && pct <= 100) dlProgress->setValue(pct);
                }
                const QStringList lines = raw.split('\n', Qt::SkipEmptyParts);
                if (!lines.isEmpty()) modDlLog->append(lines.last().trimmed());
            });
            connect(curl, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                    this, [=](int code, QProcess::ExitStatus){
                curl->deleteLater();
                modDlBtn->setEnabled(true);
                if (code == 0) {
                    dlProgress->setValue(100);
                    modDlLog->append("\n\xe2\x9c\x85  Download completato!");
                    (*refreshHolder)();
                } else {
                    dlProgress->setVisible(false);
                    modDlLog->append("\n\xe2\x9d\x8c  Errore. Installa wget o curl.");
                }
            });
            curl->start("curl", {"-L", "-C", "-", "--progress-bar", "-o", dest, url});
        }
    });

    connect(refreshBtn, &QPushButton::clicked, this, [refreshHolder]{ (*refreshHolder)(); });
    connect(m_llamaStack, &QStackedWidget::currentChanged, this, [=](int idx){
        if (idx == 3) (*refreshHolder)();
    });

    /* ──── Sub-page 3: Scarica Modelli Matematica/Logica ───────── */
    auto* dlPg  = new QWidget;
    auto* dlLay = new QVBoxLayout(dlPg);
    dlLay->setContentsMargins(0,0,0,0); dlLay->setSpacing(8);

    /* Header */
    auto* dlTop  = new QWidget(dlPg);
    auto* dlTopL = new QHBoxLayout(dlTop);
    dlTopL->setContentsMargins(0,0,0,0); dlTopL->setSpacing(10);
    auto* backDl = new QPushButton("\xe2\x86\x90 Menu llama", dlPg);
    backDl->setObjectName("actionBtn");
    connect(backDl, &QPushButton::clicked, this, [=]{ m_llamaStack->setCurrentIndex(0); });
    auto* dlTitle = new QLabel("\xf0\x9f\x93\xa5  Scarica Modelli Matematica / Logica", dlPg);
    dlTitle->setObjectName("cardTitle");
    dlTopL->addWidget(backDl); dlTopL->addWidget(dlTitle, 1);
    dlLay->addWidget(dlTop);

    auto* dlInfo = new QLabel(
        "I modelli vengono scaricati nella cartella <b>models/</b> di llama.cpp Studio "
        "e sono disponibili subito per l'avvio del server.",
        dlPg);
    dlInfo->setObjectName("cardDesc"); dlInfo->setWordWrap(true);
    dlLay->addWidget(dlInfo);

    /* Lista modelli curati */
    struct MathModel {
        QString name;   /* nome visualizzato */
        QString url;    /* URL diretto al .gguf */
        QString size;   /* dimensione approssimativa */
        QString desc;   /* nota breve */
    };
    const QList<MathModel> mathModels = {
        {
            "DeepSeek-R1-Distill-Qwen-1.5B Q4_K_M  (~1.1 GB)",
            "https://huggingface.co/bartowski/DeepSeek-R1-Distill-Qwen-1.5B-GGUF/resolve/main/DeepSeek-R1-Distill-Qwen-1.5B-Q4_K_M.gguf",
            "1.1 GB",
            "Piccolo e veloce, ragionamento base"
        },
        {
            "Qwen2.5-Math-7B-Instruct Q4_K_M  (~4.7 GB)",
            "https://huggingface.co/Qwen/Qwen2.5-Math-7B-Instruct-GGUF/resolve/main/qwen2.5-math-7b-instruct-q4_k_m.gguf",
            "4.7 GB",
            "Specializzato matematica (Qwen)"
        },
        {
            "DeepSeek-R1-Distill-Qwen-7B Q4_K_M  (~4.7 GB)",
            "https://huggingface.co/bartowski/DeepSeek-R1-Distill-Qwen-7B-GGUF/resolve/main/DeepSeek-R1-Distill-Qwen-7B-Q4_K_M.gguf",
            "4.7 GB",
            "Ragionamento avanzato, derivato DeepSeek-R1"
        },
        {
            "phi-4 Q4_K_M  (~8.4 GB)",
            "https://huggingface.co/bartowski/phi-4-GGUF/resolve/main/phi-4-Q4_K_M.gguf",
            "8.4 GB",
            "Microsoft phi-4 — eccellente su math/logica"
        },
        {
            "QwQ-32B Q4_K_M  (~19.9 GB)",
            "https://huggingface.co/bartowski/QwQ-32B-GGUF/resolve/main/QwQ-32B-Q4_K_M.gguf",
            "19.9 GB",
            "Ragionamento profondo (richiede molta RAM)"
        },
    };

    /* Scroll area per la lista */
    auto* scroll = new QScrollArea(dlPg);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* listW = new QWidget;
    auto* listL = new QVBoxLayout(listW);
    listL->setContentsMargins(0,0,0,4); listL->setSpacing(6);

    /* Log download (condiviso tra tutti i pulsanti) */
    auto* dlLog = makeLog("Log download...");

    for (const auto& mm : mathModels) {
        auto* card = new QFrame(listW);
        card->setObjectName("actionCard");
        auto* cl = new QHBoxLayout(card);
        cl->setContentsMargins(14,10,14,10); cl->setSpacing(10);

        auto* txtW = new QWidget(card);
        auto* tl   = new QVBoxLayout(txtW);
        tl->setContentsMargins(0,0,0,0); tl->setSpacing(2);
        auto* nameL = new QLabel(mm.name, txtW); nameL->setObjectName("cardTitle");
        auto* descL = new QLabel(mm.desc,  txtW); descL->setObjectName("cardDesc");
        tl->addWidget(nameL); tl->addWidget(descL);

        auto* dlBtn = new QPushButton("\xe2\xac\x87 Scarica", card);
        dlBtn->setObjectName("actionBtn"); dlBtn->setFixedWidth(100);

        const QString url  = mm.url;
        const QString name = mm.name;
        connect(dlBtn, &QPushButton::clicked, this, [=]{
            /* Ricava nome file dall'URL */
            QString fname = url.section('/', -1);
            if (fname.isEmpty()) fname = "modello.gguf";
            QString dest = P::modelsDir() + "/" + fname;
            dlLog->append(QString("\xf0\x9f\x93\xa5  Scarico: %1\n   \xe2\x86\x92 %2\n").arg(name).arg(dest));
            dlBtn->setEnabled(false);

            auto* proc = new QProcess(dlPg);
            proc->setProcessChannelMode(QProcess::MergedChannels);
            connect(proc, &QProcess::readyRead, dlPg, [=]{
                dlLog->moveCursor(QTextCursor::End);
                dlLog->insertPlainText(QString::fromLocal8Bit(proc->readAll()));
                dlLog->ensureCursorVisible();
            });
            connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                    dlPg, [=](int code, QProcess::ExitStatus){
                if (code == 0)
                    dlLog->append(QString("\n\xe2\x9c\x85  Download completato: %1").arg(fname));
                else
                    dlLog->append(QString("\n\xe2\x9d\x8c  Errore (codice %1). Controlla la connessione.").arg(code));
                dlBtn->setEnabled(true);
                proc->deleteLater();
            });
#ifdef _WIN32
            proc->start("cmd", {"/c",
                QString("curl -L -o \"%1\" \"%2\" --progress-bar").arg(dest).arg(url)});
#else
            proc->start("wget", {"-c", "-O", dest, "--show-progress", url});
#endif
            if (!proc->waitForStarted(4000)) {
                dlLog->append("\xe2\x9d\x8c  wget/curl non trovato. Installa wget (Linux) o curl (Windows).");
                dlBtn->setEnabled(true);
                proc->deleteLater();
            }
        });

        cl->addWidget(txtW, 1); cl->addWidget(dlBtn);
        listL->addWidget(card);
    }

    /* URL personalizzato */
    auto* customCard = new QFrame(listW);
    customCard->setObjectName("actionCard");
    auto* ccL = new QVBoxLayout(customCard);
    ccL->setContentsMargins(14,10,14,10); ccL->setSpacing(6);
    auto* customTitle = new QLabel("\xf0\x9f\x94\x97  URL personalizzato (HuggingFace o altro)", customCard);
    customTitle->setObjectName("cardTitle");
    ccL->addWidget(customTitle);
    auto* urlRow = new QWidget(customCard);
    auto* urlRowL = new QHBoxLayout(urlRow);
    urlRowL->setContentsMargins(0,0,0,0); urlRowL->setSpacing(8);
    auto* urlEdit = new QLineEdit(customCard);
    urlEdit->setPlaceholderText("https://huggingface.co/.../resolve/main/modello.gguf");
    auto* dlCustomBtn = new QPushButton("\xe2\xac\x87 Scarica", customCard);
    dlCustomBtn->setObjectName("actionBtn"); dlCustomBtn->setFixedWidth(100);
    urlRowL->addWidget(urlEdit, 1); urlRowL->addWidget(dlCustomBtn);
    ccL->addWidget(urlRow);
    listL->addWidget(customCard);

    connect(dlCustomBtn, &QPushButton::clicked, this, [=]{
        QString url = urlEdit->text().trimmed();
        if (url.isEmpty()) { dlLog->append("\xe2\x9d\x8c  Inserisci un URL valido."); return; }
        QString fname = url.section('/', -1);
        if (fname.isEmpty()) fname = "modello.gguf";
        QString dest = P::modelsDir() + "/" + fname;
        dlLog->append(QString("\xf0\x9f\x93\xa5  Scarico URL personalizzato:\n   %1\n   \xe2\x86\x92 %2\n").arg(url).arg(dest));
        dlCustomBtn->setEnabled(false);

        auto* proc = new QProcess(dlPg);
        proc->setProcessChannelMode(QProcess::MergedChannels);
        connect(proc, &QProcess::readyRead, dlPg, [=]{
            dlLog->moveCursor(QTextCursor::End);
            dlLog->insertPlainText(QString::fromLocal8Bit(proc->readAll()));
            dlLog->ensureCursorVisible();
        });
        connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                dlPg, [=](int code, QProcess::ExitStatus){
            if (code == 0)
                dlLog->append(QString("\n\xe2\x9c\x85  Download completato: %1").arg(fname));
            else
                dlLog->append(QString("\n\xe2\x9d\x8c  Errore (codice %1).").arg(code));
            dlCustomBtn->setEnabled(true);
            proc->deleteLater();
        });
#ifdef _WIN32
        proc->start("cmd", {"/c",
            QString("curl -L -o \"%1\" \"%2\" --progress-bar").arg(dest).arg(url)});
#else
        proc->start("wget", {"-c", "-O", dest, "--show-progress", url});
#endif
        if (!proc->waitForStarted(4000)) {
            dlLog->append("\xe2\x9d\x8c  wget/curl non trovato.");
            dlCustomBtn->setEnabled(true);
            proc->deleteLater();
        }
    });

    listL->addStretch(1);
    scroll->setWidget(listW);
    dlLay->addWidget(scroll, 1);
    dlLay->addWidget(dlLog);

    m_llamaStack->addWidget(subMenu);  /* 0 — sotto-menu */
    m_llamaStack->addWidget(compPg);   /* 1 — Compila / Aggiorna */
    m_llamaStack->addWidget(modPg);    /* 2 — Gestisci Modelli */
    m_llamaStack->addWidget(dlPg);     /* 3 — Scarica Modelli */

    /* ── Primo avvio: se llama-server non esiste, mostra banner e propone compilazione ── */
    if (!binExists) {
        QSettings cfg;
        const bool alreadyPrompted = cfg.value("llamacpp/firstLaunchPrompted", false).toBool();
        if (!alreadyPrompted) {
            cfg.setValue("llamacpp/firstLaunchPrompted", true);
            /* Banner visibile nella sotto-pagina 0 (sotto-menu) */
            auto* banner = new QFrame(subMenu);
            banner->setStyleSheet(
                "QFrame{background:#1c1a08;border:2px solid #f59e0b;"
                "border-radius:8px;padding:10px;}");
            auto* bannerLay = new QHBoxLayout(banner);
            bannerLay->setSpacing(12);
            auto* ico = new QLabel("\xf0\x9f\x94\xa8", banner);
            ico->setStyleSheet("font-size:22px;");
            auto* txt = new QLabel(
                "<b style='color:#fbbf24;'>Primo avvio: llama.cpp non \xc3\xa8 compilato</b><br>"
                "<span style='color:#d1d5db;font-size:11px;'>"
                "Clicca <b>Compila ora</b> per scaricare e compilare llama.cpp automaticamente. "
                "Richiede: git, cmake, gcc/g++ (Linux) o MSYS2 (Windows). "
                "Operazione una tantum (~5-15 min)."
                "</span>", banner);
            txt->setWordWrap(true); txt->setTextFormat(Qt::RichText);
            auto* compNowBtn = new QPushButton("\xf0\x9f\x94\xa8  Compila ora", banner);
            compNowBtn->setObjectName("actionBtn");
            compNowBtn->setStyleSheet(
                "QPushButton{background:#d97706;color:#fff;border-radius:5px;"
                "font-weight:bold;padding:5px 12px;}"
                "QPushButton:hover{background:#b45309;}");
            connect(compNowBtn, &QPushButton::clicked, this, [=]{
                banner->setVisible(false);
                m_llamaStack->setCurrentIndex(1);  /* vai alla pagina Compila */
                /* Avvia automaticamente la compilazione */
                if (m_llamaCompBtn) m_llamaCompBtn->click();
            });
            bannerLay->addWidget(ico);
            bannerLay->addWidget(txt, 1);
            bannerLay->addWidget(compNowBtn);
            /* Inserisci il banner in cima al subMenu layout (prima delle card) */
            qobject_cast<QVBoxLayout*>(subMenu->layout())->insertWidget(0, banner);
        }
    }

    return page;
}

/* ══════════════════════════════════════════════════════════════
   PAGINA 3 — Compila Prismalux (TUI C + Qt GUI, Linux + Windows)
   ══════════════════════════════════════════════════════════════ */
QWidget* PersonalizzaPage::buildCompila() {
    auto* page = new QWidget(this);
    auto* lay  = new QVBoxLayout(page);
    lay->setContentsMargins(24, 16, 24, 16); lay->setSpacing(10);
    auto* _hdr = new QLabel("📦  Compila", page); _hdr->setObjectName("pageTitle"); lay->addWidget(_hdr);

    /* ── 4 card in griglia 2x2 ── */
    auto* grid  = new QWidget(page);
    auto* gridL = new QGridLayout(grid);
    gridL->setSpacing(10);

    struct CompItem {
        QString ico; QString title; QString desc;
        QPushButton** btnPtr;
        QString tag;  /* "WIN_TUI" | "LIN_TUI" | "WIN_QT" | "LIN_QT" */
    };
    QList<CompItem> items = {
        {"🪟", "TUI Windows (.exe)",
         "prismalux.exe — cross-compila da Linux (MinGW)\no compila nativo su Windows (MSYS2).",
         &m_btnWinTUI, "WIN_TUI"},
        {"🐧", "TUI Linux (nativo)",
         "prismalux — gcc nativo.\nOttimizzato per la CPU locale.",
         &m_btnLinTUI, "LIN_TUI"},
        {"🪟🖼️", "GUI Qt Windows (.exe)",
         "Prismalux_GUI.exe — cmake MinGW.\nRichiede Qt6 MinGW nel PATH.",
         &m_btnWinQt, "WIN_QT"},
        {"🐧🖼️", "GUI Qt Linux (nativo)",
         "Prismalux_GUI — cmake + Qt6 nativi.\nProto per la tua macchina.",
         &m_btnLinQt, "LIN_QT"},
    };

    int row = 0, col = 0;
    for (auto& it : items) {
        auto* card = new QFrame(grid);
        card->setObjectName("actionCard");
        auto* cl = new QVBoxLayout(card);
        cl->setContentsMargins(14, 12, 14, 12); cl->setSpacing(6);

        auto* hdr = new QWidget(card);
        auto* hdrl = new QHBoxLayout(hdr);
        hdrl->setContentsMargins(0,0,0,0); hdrl->setSpacing(8);
        auto* ico = new QLabel(it.ico, card); ico->setObjectName("cardIcon");
        auto* ttl = new QLabel(it.title, card); ttl->setObjectName("cardTitle");
        hdrl->addWidget(ico); hdrl->addWidget(ttl, 1);
        cl->addWidget(hdr);

        auto* desc = new QLabel(it.desc, card);
        desc->setObjectName("cardDesc"); desc->setWordWrap(true);
        cl->addWidget(desc);

        auto* btn = new QPushButton("▶  Compila", card);
        btn->setObjectName("actionBtn");
        *it.btnPtr = btn;
        cl->addWidget(btn);

        gridL->addWidget(card, row, col);
        col++; if (col > 1) { col = 0; row++; }
    }
    lay->addWidget(grid);

    /* ── Log condiviso ── */
    auto* logLbl = new QLabel("📜  Output:", page);
    logLbl->setObjectName("cardDesc");
    lay->addWidget(logLbl);

    m_compLog = makeLog(
        "L'output del compilatore apparirà qui.\n\n"
        "Prerequisiti TUI Windows:  sudo apt install gcc-mingw-w64-x86-64\n"
        "Prerequisiti TUI Linux:    gcc, make\n"
        "Prerequisiti GUI Windows:  cmake, Qt6 MinGW\n"
        "Prerequisiti GUI Linux:    cmake, qt6-base-dev");
    lay->addWidget(m_compLog, 1);

    /* ── Helper lambda compila TUI ── */
    auto compileTUI = [=](bool forWindows){
        m_compLog->clear();
        auto* btn = forWindows ? m_btnWinTUI : m_btnLinTUI;
        btn->setEnabled(false);

        QString csrc = P::root() + "/C_software/src";
        QString cinc = P::root() + "/C_software/include";

        auto S = [&](const QString& f){ return csrc + "/" + f; };
        QStringList srcList = {
            S("main.c"), S("backend.c"), S("terminal.c"), S("http.c"), S("ai.c"),
            S("modelli.c"), S("output.c"), S("multi_agente.c"), S("strumenti.c"),
            S("hw_detect.c"), S("agent_scheduler.c"), S("prismalux_ui.c")
        };

        QString compiler, outFile, extraFlags;
        if (forWindows) {
#ifdef _WIN32
            compiler   = "gcc";
#else
            compiler   = "x86_64-w64-mingw32-gcc";
#endif
            outFile    = P::root() + "/prismalux.exe";
            extraFlags = "-lws2_32";
            m_compLog->append("🪟  Target: Windows (.exe)\n");
        } else {
            compiler   = "gcc";
            outFile    = P::root() + "/prismalux";
            extraFlags = "-lpthread";
            m_compLog->append("🐧  Target: Linux (nativo)\n");
        }

#ifdef _WIN32
        csrc   = QDir::toNativeSeparators(csrc);
        cinc   = QDir::toNativeSeparators(cinc);
        outFile = QDir::toNativeSeparators(outFile);
#endif
        QStringList nativeSrcs;
        for (auto& s : srcList)
            nativeSrcs << QDir::toNativeSeparators(s);

        QString cmd = QString("\"%1\" -O2 -Wall -I\"%2\" -o \"%3\" %4 %5 -lm")
                      .arg(compiler).arg(QDir::toNativeSeparators(cinc))
                      .arg(outFile).arg(nativeSrcs.join(" ")).arg(extraFlags);

        m_compLog->append(QString("⚙️  Comando:\n%1\n").arg(cmd));
        auto* proc = new QProcess(this);
        proc->setProcessChannelMode(QProcess::MergedChannels);
        connect(proc, &QProcess::readyRead, this, [=]{
            m_compLog->moveCursor(QTextCursor::End);
            m_compLog->insertPlainText(QString::fromLocal8Bit(proc->readAll()));
            m_compLog->ensureCursorVisible();
        });
        connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                this, [=](int code, QProcess::ExitStatus){
            if (code == 0) {
                m_compLog->append(QString("\n✅  Compilato: %1").arg(outFile));
                if (forWindows)
                    m_compLog->append("💡  Copia il file su Windows e avvia con:\n"
                                      "    prismalux.exe --backend ollama");
            } else {
                m_compLog->append(QString("\n❌  Errore (code %1).").arg(code));
                if (forWindows && !QProcess::execute("which x86_64-w64-mingw32-gcc", {}))
                    m_compLog->append("🌫️  Installa: sudo apt install gcc-mingw-w64-x86-64");
            }
            btn->setEnabled(true);
            proc->deleteLater();
        });
#ifdef _WIN32
        proc->start("cmd", {"/c", cmd});
#else
        proc->start("sh", {"-c", cmd});
#endif
        if (!proc->waitForStarted(4000)) {
            m_compLog->append(QString("❌  Compilatore '%1' non trovato.").arg(compiler));
            if (forWindows)
                m_compLog->append("  sudo apt install gcc-mingw-w64-x86-64");
            btn->setEnabled(true);
            proc->deleteLater();
        }
    };

    /* ── Helper lambda compila Qt GUI ── */
    auto compileQt = [=](bool forWindows){
        m_compLog->clear();
        auto* btn = forWindows ? m_btnWinQt : m_btnLinQt;
        btn->setEnabled(false);

        QString qtSrc = P::root() + "/Qt_GUI";
        QString qtBld = P::root() + (forWindows ? "/Qt_GUI/build_win" : "/Qt_GUI/build_lin");

#ifdef _WIN32
        qtSrc = QDir::toNativeSeparators(qtSrc);
        qtBld = QDir::toNativeSeparators(qtBld);
#endif
        QString generator = forWindows ? "-G \"MinGW Makefiles\"" : "-G \"Unix Makefiles\"";
        QString cmakeConf = QString("cmake -B \"%1\" -S \"%2\" %3 -DCMAKE_BUILD_TYPE=Release")
                            .arg(qtBld).arg(qtSrc).arg(generator);
        QString cmakeBuild = QString("cmake --build \"%1\" --config Release -j4").arg(qtBld);
        QString fullCmd = cmakeConf + " && " + cmakeBuild;

        m_compLog->append(forWindows ? "🪟  Target: GUI Qt Windows\n" : "🐧  Target: GUI Qt Linux\n");
        m_compLog->append(QString("⚙️  %1\n").arg(fullCmd));

        auto* proc = new QProcess(this);
        proc->setProcessChannelMode(QProcess::MergedChannels);
        connect(proc, &QProcess::readyRead, this, [=]{
            m_compLog->moveCursor(QTextCursor::End);
            m_compLog->insertPlainText(QString::fromLocal8Bit(proc->readAll()));
            m_compLog->ensureCursorVisible();
        });
        connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                this, [=](int code, QProcess::ExitStatus){
            if (code == 0)
                m_compLog->append(QString("\n✅  Prismalux_GUI%1 → %2/")
                                  .arg(forWindows ? ".exe" : "").arg(qtBld));
            else
                m_compLog->append(QString("\n❌  Errore cmake (code %1).\n"
                    "Controlla: cmake e Qt6 nel PATH.").arg(code));
            btn->setEnabled(true);
            proc->deleteLater();
        });
#ifdef _WIN32
        proc->start("cmd", {"/c", fullCmd});
#else
        proc->start("sh", {"-c", fullCmd});
#endif
        if (!proc->waitForStarted(5000)) {
            m_compLog->append("❌  cmake non trovato.\n"
                "  Linux:   sudo apt install cmake qt6-base-dev\n"
                "  Windows: installa cmake + Qt MINGW64");
            btn->setEnabled(true);
            proc->deleteLater();
        }
    };

    connect(m_btnWinTUI, &QPushButton::clicked, this, [=]{ compileTUI(true);  });
    connect(m_btnLinTUI, &QPushButton::clicked, this, [=]{ compileTUI(false); });
    connect(m_btnWinQt,  &QPushButton::clicked, this, [=]{ compileQt(true);   });
    connect(m_btnLinQt,  &QPushButton::clicked, this, [=]{ compileQt(false);  });

    return page;
}

/* ══════════════════════════════════════════════════════════════
   Costruttore principale
   ══════════════════════════════════════════════════════════════ */
PersonalizzaPage::PersonalizzaPage(QWidget* parent)
    : QWidget(parent)
{
    /* I widget vengono costruiti lazy da ImpostazioniPage via build*() */
}
