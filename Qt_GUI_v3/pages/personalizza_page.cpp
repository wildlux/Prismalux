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

namespace P = PrismaluxPaths;

/* ──────────────────────────────────────────────────────────────
   Helper: barra superiore con titolo + pulsante ← Indietro
   ────────────────────────────────────────────────────────────── */
QWidget* PersonalizzaPage::makeBackBar(const QString& title) {
    auto* w   = new QWidget(this);
    auto* lay = new QHBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 8); lay->setSpacing(10);

    auto* back = new QPushButton("← Indietro", w);
    back->setObjectName("actionBtn");
    connect(back, &QPushButton::clicked, this, [this]{ m_stack->setCurrentIndex(0); });

    auto* lbl = new QLabel(title, w);
    lbl->setObjectName("pageTitle");

    lay->addWidget(back);
    lay->addWidget(lbl, 1);
    return w;
}

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
   PAGINA 0 — Menu principale
   ══════════════════════════════════════════════════════════════ */
QWidget* PersonalizzaPage::buildMenu() {
    auto* page = new QWidget(this);
    auto* lay  = new QVBoxLayout(page);
    lay->setContentsMargins(24, 20, 24, 16); lay->setSpacing(12);

    auto* title = new QLabel("🔩  Personalizzazione Avanzata Estrema", page);
    title->setObjectName("pageTitle");
    auto* sub = new QLabel("VRAM Benchmark · llama.cpp Studio · Cython Studio · Compila", page);
    sub->setObjectName("pageSubtitle");
    lay->addWidget(title); lay->addWidget(sub);
    auto* div = new QFrame(page); div->setObjectName("pageDivider");
    div->setFrameShape(QFrame::HLine); lay->addWidget(div);

    struct Item { QString num; QString ico; QString title; QString desc; int page; };
    QList<Item> items = {
        {"1️⃣", "🔬", "VRAM Benchmark",
         "Profila il consumo VRAM di ogni modello Ollama.\nProduce vram_profile.json per lo scheduler.", 1},
        {"2️⃣", "🦙", "llama.cpp Studio",
         "Compila llama.cpp con flag GPU ottimizzati.\nGestisci modelli .gguf e avvia il server.", 2},
        {"3️⃣", "⚡", "Cython Studio",
         "Ottimizza moduli Python con Cython.\nSeleziona un .py e compila in .so/.pyd.", 3},
        {"4️⃣", "📦", "Compila",
         "Produci prismalux (TUI) o Prismalux_GUI (Qt)\nper Linux e Windows da qui.", 4},
    };

    for (auto& it : items) {
        auto* card = new QFrame(page);
        card->setObjectName("actionCard");
        auto* cl = new QHBoxLayout(card);
        cl->setContentsMargins(16, 14, 16, 14); cl->setSpacing(14);

        auto* num = new QLabel(it.num, card); num->setFixedWidth(28);
        auto* ico = new QLabel(it.ico, card); ico->setObjectName("cardIcon"); ico->setFixedWidth(30);

        auto* txt  = new QWidget(card);
        auto* tl   = new QVBoxLayout(txt); tl->setContentsMargins(0,0,0,0); tl->setSpacing(3);
        auto* lttl = new QLabel(it.title, txt); lttl->setObjectName("cardTitle");
        auto* ldsc = new QLabel(it.desc,  txt); ldsc->setObjectName("cardDesc"); ldsc->setWordWrap(true);
        tl->addWidget(lttl); tl->addWidget(ldsc);

        auto* btn = new QPushButton("Apri →", card);
        btn->setObjectName("actionBtn"); btn->setFixedWidth(90);
        const int pg = it.page;
        connect(btn, &QPushButton::clicked, this, [this, pg]{ m_stack->setCurrentIndex(pg); });

        cl->addWidget(num); cl->addWidget(ico); cl->addWidget(txt, 1); cl->addWidget(btn);
        lay->addWidget(card);
    }

    lay->addStretch(1);

    auto* ver = new QLabel("Prismalux v2.1 — \"Costruito per i mortali\"", page);
    ver->setObjectName("cardDesc"); ver->setAlignment(Qt::AlignCenter);
    lay->addWidget(ver);
    return page;
}

/* ══════════════════════════════════════════════════════════════
   PAGINA 1 — VRAM Benchmark
   ══════════════════════════════════════════════════════════════ */
QWidget* PersonalizzaPage::buildVramBench() {
    auto* page = new QWidget(this);
    auto* lay  = new QVBoxLayout(page);
    lay->setContentsMargins(24, 16, 24, 16); lay->setSpacing(10);
    lay->addWidget(makeBackBar("🔬  VRAM Benchmark"));

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
    lay->addWidget(makeBackBar("🦙  llama.cpp Studio"));

    /* ── Sotto-stack interno ── */
    m_llamaStack = new QStackedWidget(page);
    lay->addWidget(m_llamaStack, 1);

    /* ──── Sub-page 0: sotto-menu ───────────────────────────── */
    auto* subMenu  = new QWidget;
    auto* subLay   = new QVBoxLayout(subMenu);
    subLay->setContentsMargins(0,4,0,4); subLay->setSpacing(8);

    struct SubItem { QString ico; QString title; QString desc; int pg; };
    QList<SubItem> subs = {
        {"🔨", "Compila llama.cpp",
         "Clona e compila llama.cpp con flag CUDA/ROCm/Vulkan/CPU auto-rilevati.", 1},
        {"🖥️", "Info Hardware / Flag consigliati",
         "Mostra GPU rilevata e suggerisce i flag cmake ottimali.", 2},
        {"📂", "Gestisci Modelli .gguf",
         "Elenca, elimina, cerca modelli nella cartella models/.", 3},
        {"🚀", "Avvia Server llama.cpp",
         "Avvia llama-server o llama-cli su porta configurabile.", 4},
        {"\xf0\x9f\x93\xa5", "Scarica Modelli Matematica/Logica",
         "Download guidato di GGUF specializzati in matematica e ragionamento logico da HuggingFace.", 5},
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
    auto* compTitle = new QLabel("🔨  Compila llama.cpp", compPg);
    compTitle->setObjectName("cardTitle");
    m_llamaCompBtn = new QPushButton("▶  Compila", compPg);
    m_llamaCompBtn->setObjectName("actionBtn");
    compTopL->addWidget(backComp); compTopL->addWidget(compTitle, 1); compTopL->addWidget(m_llamaCompBtn);
    compLay->addWidget(compTop);

    auto* compInfo = new QLabel(
        "Esegue: <tt>git clone --depth=1 https://github.com/ggerganov/llama.cpp</tt> "
        "poi <tt>cmake -DGGML_NATIVE=ON ...</tt> nella cartella llama_cpp_studio/", compPg);
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

    /* ──── Sub-page 2: Info Hardware ────────────────────────── */
    auto* hwPg  = new QWidget;
    auto* hwLay = new QVBoxLayout(hwPg);
    hwLay->setContentsMargins(0,0,0,0); hwLay->setSpacing(8);

    auto* hwTop = new QWidget(hwPg);
    auto* hwTopL = new QHBoxLayout(hwTop);
    hwTopL->setContentsMargins(0,0,0,0);
    auto* backHW = new QPushButton("← Menu llama", hwPg);
    backHW->setObjectName("actionBtn");
    connect(backHW, &QPushButton::clicked, this, [=]{ m_llamaStack->setCurrentIndex(0); });
    auto* hwTitle = new QLabel("🖥️  Info Hardware / Flag consigliati", hwPg);
    hwTitle->setObjectName("cardTitle");
    hwTopL->addWidget(backHW); hwTopL->addWidget(hwTitle, 1);
    hwLay->addWidget(hwTop);

    auto* hwInfo = makeLog();
    hwLay->addWidget(hwInfo, 1);

    /* Popola info al momento della visualizzazione */
    connect(m_llamaStack, &QStackedWidget::currentChanged, this, [=](int idx){
        if (idx != 2) return;
        hwInfo->clear();
        hwInfo->append("🔍  Rilevamento hardware...\n");

        QString out;
#ifdef __linux__
        /* CPU */
        QProcess p1; p1.start("sh", {"-c", "grep 'model name' /proc/cpuinfo | head -1 | cut -d: -f2"});
        p1.waitForFinished(2000);
        out = QString::fromLocal8Bit(p1.readAllStandardOutput()).trimmed();
        hwInfo->append(QString("🖥️  CPU: %1").arg(out.isEmpty() ? "n/d" : out));

        /* GPU NVIDIA */
        QProcess p2; p2.start("sh", {"-c", "nvidia-smi --query-gpu=name,memory.total --format=csv,noheader 2>/dev/null"});
        p2.waitForFinished(3000);
        QString nv = QString::fromLocal8Bit(p2.readAllStandardOutput()).trimmed();
        if (!nv.isEmpty()) hwInfo->append(QString("🟢  NVIDIA: %1").arg(nv));

        /* GPU AMD */
        QProcess p3; p3.start("sh", {"-c", "rocm-smi --showproductname 2>/dev/null | head -3"});
        p3.waitForFinished(2000);
        QString amd = QString::fromLocal8Bit(p3.readAllStandardOutput()).trimmed();
        if (!amd.isEmpty()) hwInfo->append(QString("🔴  AMD: %1").arg(amd));

        /* Vulkan */
        QProcess p4; p4.start("sh", {"-c", "vulkaninfo 2>/dev/null | grep 'deviceName' | head -2"});
        p4.waitForFinished(2000);
        QString vk = QString::fromLocal8Bit(p4.readAllStandardOutput()).trimmed();
        if (!vk.isEmpty()) hwInfo->append(QString("🔷  Vulkan: %1").arg(vk));
#else
        QProcess p1; p1.start("cmd", {"/c", "wmic cpu get name /value"});
        p1.waitForFinished(3000);
        out = QString::fromLocal8Bit(p1.readAllStandardOutput()).trimmed();
        hwInfo->append(QString("🖥️  CPU: %1").arg(out));

        QProcess p2; p2.start("cmd", {"/c", "nvidia-smi --query-gpu=name,memory.total --format=csv,noheader"});
        p2.waitForFinished(3000);
        QString nv = QString::fromLocal8Bit(p2.readAllStandardOutput()).trimmed();
        if (!nv.isEmpty()) hwInfo->append(QString("🟢  NVIDIA: %1").arg(nv));
#endif
        hwInfo->append("\n─────────────────────────────────");
        hwInfo->append("📋  Flag cmake consigliati:");

        bool hasCuda =
#ifdef _WIN32
            true;  /* assume CUDA disponibile su Windows */
#else
            QFileInfo::exists("/usr/local/cuda/bin/nvcc") ||
            QFileInfo::exists("/usr/bin/nvcc");
#endif

        if (hasCuda)
            hwInfo->append("  -DGGML_CUDA=ON -DGGML_NATIVE=ON\n  -DCMAKE_CUDA_ARCHITECTURES=native");
        else if (QFileInfo::exists("/opt/rocm"))
            hwInfo->append("  -DGGML_HIP=ON -DGGML_NATIVE=ON");
        else if (QFileInfo::exists("/usr/bin/vulkaninfo"))
            hwInfo->append("  -DGGML_VULKAN=ON -DGGML_NATIVE=ON");
        else
            hwInfo->append("  -DGGML_NATIVE=ON  (solo CPU — AVX2/AVX512 se disponibili)");

        hwInfo->append("\n  Aggiungi -DCMAKE_BUILD_TYPE=Release -j$(nproc)");
    });

    /* ──── Sub-page 3: Gestisci Modelli ─────────────────────── */
    auto* modPg  = new QWidget;
    auto* modLay = new QVBoxLayout(modPg);
    modLay->setContentsMargins(0,0,0,0); modLay->setSpacing(8);

    auto* modTop = new QWidget(modPg);
    auto* modTopL = new QHBoxLayout(modTop);
    modTopL->setContentsMargins(0,0,0,0);
    auto* backMod = new QPushButton("← Menu llama", modPg);
    backMod->setObjectName("actionBtn");
    connect(backMod, &QPushButton::clicked, this, [=]{ m_llamaStack->setCurrentIndex(0); });
    auto* modTitle = new QLabel("📂  Modelli .gguf", modPg);
    modTitle->setObjectName("cardTitle");
    auto* refreshBtn = new QPushButton("🔄  Aggiorna", modPg);
    refreshBtn->setObjectName("actionBtn");
    modTopL->addWidget(backMod); modTopL->addWidget(modTitle, 1); modTopL->addWidget(refreshBtn);
    modLay->addWidget(modTop);

    auto* modInfo = new QLabel("", modPg);
    modInfo->setObjectName("cardDesc"); modInfo->setWordWrap(true);
    modLay->addWidget(modInfo);

    auto* modLog = makeLog("I file .gguf trovati appariranno qui...");
    modLay->addWidget(modLog, 1);

    auto refreshModels = [=]{
        modLog->clear();
        QString modelsDir = P::modelsDir();
        QDir d(modelsDir);
        modInfo->setText(QString("📁  %1").arg(modelsDir));
        if (!d.exists()) {
            modLog->append("⚠️  Cartella models/ non trovata.\nCreala con:\n  mkdir -p " + modelsDir);
            return;
        }
        auto files = d.entryInfoList({"*.gguf","*.bin"}, QDir::Files, QDir::Size | QDir::Reversed);
        if (files.isEmpty()) {
            modLog->append("🌫️  Nessun modello .gguf trovato.\n"
                           "Scarica un modello da: https://huggingface.co/models?library=gguf\n"
                           "e copialo nella cartella models/");
            return;
        }
        modLog->append(QString("📦  %1 modello/i trovati:\n").arg(files.size()));
        for (const auto& f : files) {
            double mb = f.size() / (1024.0 * 1024.0);
            modLog->append(QString("  %-50s  %6.0f MB")
                           .arg(f.fileName()).arg(mb));
        }
    };
    connect(refreshBtn, &QPushButton::clicked, this, refreshModels);
    connect(m_llamaStack, &QStackedWidget::currentChanged, this, [=](int idx){
        if (idx == 3) refreshModels();
    });

    /* ──── Sub-page 4: Avvia Server ─────────────────────────── */
    auto* servPg  = new QWidget;
    auto* servLay = new QVBoxLayout(servPg);
    servLay->setContentsMargins(0,0,0,0); servLay->setSpacing(8);

    auto* servTop = new QWidget(servPg);
    auto* servTopL = new QHBoxLayout(servTop);
    servTopL->setContentsMargins(0,0,0,0);
    auto* backServ = new QPushButton("← Menu llama", servPg);
    backServ->setObjectName("actionBtn");
    connect(backServ, &QPushButton::clicked, this, [=]{ m_llamaStack->setCurrentIndex(0); });
    auto* servTitle = new QLabel("🚀  Avvia llama-server", servPg);
    servTitle->setObjectName("cardTitle");
    servTopL->addWidget(backServ); servTopL->addWidget(servTitle, 1);
    servLay->addWidget(servTop);

    /* Config */
    auto* cfg = new QGroupBox("Configurazione server", servPg);
    auto* cfgL = new QGridLayout(cfg);
    cfgL->setSpacing(8);

    cfgL->addWidget(new QLabel("Modello (.gguf):"), 0, 0);
    m_llamaModelPath = new QLineEdit(servPg);
    m_llamaModelPath->setPlaceholderText("percorso/al/modello.gguf");
    auto* browseBtn = new QPushButton("…", servPg);
    browseBtn->setObjectName("actionBtn"); browseBtn->setFixedWidth(32);
    cfgL->addWidget(m_llamaModelPath, 0, 1);
    cfgL->addWidget(browseBtn, 0, 2);

    cfgL->addWidget(new QLabel("Porta:"), 1, 0);
    m_llamaPort = new QLineEdit(QString::number(P::kLlamaServerPort), servPg);
    m_llamaPort->setFixedWidth(80);
    cfgL->addWidget(m_llamaPort, 1, 1);

    servLay->addWidget(cfg);

    auto* servCtrl = new QWidget(servPg);
    auto* servCtrlL = new QHBoxLayout(servCtrl);
    servCtrlL->setContentsMargins(0,0,0,0); servCtrlL->setSpacing(10);
    m_llamaServBtn = new QPushButton("▶  Avvia Server", servPg);
    m_llamaServBtn->setObjectName("actionBtn");
    auto* stopServ = new QPushButton("■  Stop", servPg);
    stopServ->setObjectName("actionBtn"); stopServ->setProperty("danger","true");
    stopServ->setEnabled(false);
    servCtrlL->addWidget(m_llamaServBtn); servCtrlL->addWidget(stopServ); servCtrlL->addStretch(1);
    servLay->addWidget(servCtrl);

    auto* servLog = makeLog("Log server llama.cpp...");
    servLay->addWidget(servLog, 1);

    connect(browseBtn, &QPushButton::clicked, this, [=]{
        QString path = QFileDialog::getOpenFileName(this, "Seleziona modello .gguf",
            P::modelsDir(),
            "Modelli GGUF (*.gguf *.bin)");
        if (!path.isEmpty()) m_llamaModelPath->setText(path);
    });

    connect(m_llamaServBtn, &QPushButton::clicked, this, [=]{
        if (m_llamaModelPath->text().isEmpty()) {
            servLog->append("❌  Seleziona un file .gguf prima di avviare il server.");
            return;
        }
        servLog->clear();
        m_llamaServBtn->setEnabled(false);
        stopServ->setEnabled(true);

        QString server = P::llamaServerBin();
#ifdef _WIN32
        server += ".exe";
        server = QDir::toNativeSeparators(server);
#endif
        /* Sicurezza: bind solo su loopback, mai su tutte le interfacce */
        QString cmd = QString("\"%1\" -m \"%2\" --port %3 --host 127.0.0.1 -c 4096")
                      .arg(server)
                      .arg(m_llamaModelPath->text())
                      .arg(m_llamaPort->text());

        servLog->append(QString("🚀  %1\n").arg(cmd));

        m_llamaServProc = new QProcess(this);
        m_llamaServProc->setProcessChannelMode(QProcess::MergedChannels);
        connect(m_llamaServProc, &QProcess::readyRead, this, [=]{
            servLog->moveCursor(QTextCursor::End);
            servLog->insertPlainText(QString::fromLocal8Bit(m_llamaServProc->readAll()));
            servLog->ensureCursorVisible();
        });
        connect(m_llamaServProc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                this, [=](int code, QProcess::ExitStatus){
            servLog->append(QString("\n🔴  Server terminato (code %1).").arg(code));
            m_llamaServBtn->setEnabled(true);
            stopServ->setEnabled(false);
            m_llamaServProc = nullptr;
        });
#ifdef _WIN32
        m_llamaServProc->start("cmd", {"/c", cmd});
#else
        m_llamaServProc->start("sh", {"-c", cmd});
#endif
        if (!m_llamaServProc->waitForStarted(4000)) {
            servLog->append("❌  llama-server non trovato.\nCompilalo prima nella sezione \"Compila llama.cpp\".");
            m_llamaServBtn->setEnabled(true);
            stopServ->setEnabled(false);
            m_llamaServProc = nullptr;
        }
    });

    connect(stopServ, &QPushButton::clicked, this, [=]{
        if (m_llamaServProc) { m_llamaServProc->terminate(); }
        stopServ->setEnabled(false);
    });

    /* ──── Sub-page 5: Scarica Modelli Matematica/Logica ───────── */
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

    m_llamaStack->addWidget(subMenu);  /* 0 */
    m_llamaStack->addWidget(compPg);   /* 1 */
    m_llamaStack->addWidget(hwPg);     /* 2 */
    m_llamaStack->addWidget(modPg);    /* 3 */
    m_llamaStack->addWidget(servPg);   /* 4 */
    m_llamaStack->addWidget(dlPg);     /* 5 */

    return page;
}

/* ══════════════════════════════════════════════════════════════
   PAGINA 3 — Cython Studio
   ══════════════════════════════════════════════════════════════ */
QWidget* PersonalizzaPage::buildCythonStudio() {
    auto* page = new QWidget(this);
    auto* lay  = new QVBoxLayout(page);
    lay->setContentsMargins(24, 16, 24, 16); lay->setSpacing(10);
    lay->addWidget(makeBackBar("⚡  Cython Studio"));

    auto* sub = new QLabel(
        "Compila un modulo Python in codice C ottimizzato con Cython.\n"
        "Richiede: <tt>pip install cython setuptools</tt>", page);
    sub->setObjectName("cardDesc"); sub->setWordWrap(true);
    lay->addWidget(sub);

    /* ── Selezione file ── */
    auto* fileRow = new QWidget(page);
    auto* fileRowL = new QHBoxLayout(fileRow);
    fileRowL->setContentsMargins(0,0,0,0); fileRowL->setSpacing(8);

    m_cythoFile = new QLabel("(nessun file selezionato)", page);
    m_cythoFile->setObjectName("cardDesc");
    auto* browsePy = new QPushButton("📂  Seleziona .py", page);
    browsePy->setObjectName("actionBtn");
    m_cythoBtn = new QPushButton("▶  Compila", page);
    m_cythoBtn->setObjectName("actionBtn");
    m_cythoBtn->setEnabled(false);

    fileRowL->addWidget(browsePy); fileRowL->addWidget(m_cythoFile, 1); fileRowL->addWidget(m_cythoBtn);
    lay->addWidget(fileRow);

    m_cythoLog = makeLog(
        "Seleziona un file .py e premi Compila.\n\n"
        "Flusso:\n"
        "  1. cython --cplus -3 <file.py> → <file.cpp>\n"
        "  2. python setup.py build_ext --inplace\n"
        "  3. Produce <file>.so (Linux) o <file>.pyd (Windows)");
    lay->addWidget(m_cythoLog, 1);

    connect(browsePy, &QPushButton::clicked, this, [=]{
        QString path = QFileDialog::getOpenFileName(this, "Seleziona file Python",
            P::root() + "/Python_Software", "Python (*.py)");
        if (path.isEmpty()) return;
        m_cythoPath = path;
        m_cythoFile->setText(QFileInfo(path).fileName());
        m_cythoBtn->setEnabled(true);
    });

    connect(m_cythoBtn, &QPushButton::clicked, this, [=]{
        if (m_cythoPath.isEmpty()) return;
        m_cythoLog->clear();
        m_cythoBtn->setEnabled(false);

        QFileInfo fi(m_cythoPath);
        QString workDir = fi.absolutePath();
        QString baseName = fi.baseName();

        /* Crea setup.py temporaneo */
        QString setupPy = workDir + "/_cython_setup_tmp.py";
        QFile sf(setupPy);
        if (sf.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream ts(&sf);
            ts << "from setuptools import setup\n"
               << "from Cython.Build import cythonize\n"
               << "setup(ext_modules=cythonize('" << fi.fileName() << "', "
               << "compiler_directives={'language_level':'3'}))\n";
            sf.close();
        }

        QString cmd = QString("cd \"%1\" && python \"%2\" build_ext --inplace")
                      .arg(workDir).arg(setupPy);

        m_cythoLog->append(QString("📁  Dir: %1\n").arg(workDir));

        auto* proc = new QProcess(this);
        proc->setWorkingDirectory(workDir);
        connect(proc, &QProcess::readyRead, this, [=]{
            m_cythoLog->moveCursor(QTextCursor::End);
            m_cythoLog->insertPlainText(QString::fromLocal8Bit(proc->readAll()));
            m_cythoLog->ensureCursorVisible();
        });
        connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                this, [=](int code, QProcess::ExitStatus){
            QFile::remove(setupPy);  /* pulizia setup tmp */
            if (code == 0)
                m_cythoLog->append(QString(
                    "\n✅  Compilazione OK!\n"
                    "  Modulo: %1.cpython-*.so  (Linux)\n"
                    "         %1.pyd           (Windows)").arg(baseName));
            else
                m_cythoLog->append(QString("\n❌  Errore (code %1).\n"
                    "Verifica: pip install cython setuptools").arg(code));
            m_cythoBtn->setEnabled(true);
            proc->deleteLater();
        });
        proc->setProcessChannelMode(QProcess::MergedChannels);
#ifdef _WIN32
        proc->start("cmd", {"/c", cmd});
#else
        proc->start("sh", {"-c", cmd});
#endif
        if (!proc->waitForStarted(4000)) {
            m_cythoLog->append("❌  Impossibile avviare python.\n"
                               "  pip install cython setuptools");
            m_cythoBtn->setEnabled(true);
            proc->deleteLater();
        }
    });

    return page;
}

/* ══════════════════════════════════════════════════════════════
   PAGINA 4 — Compila (TUI + Qt, Linux + Windows)
   ══════════════════════════════════════════════════════════════ */
QWidget* PersonalizzaPage::buildCompila() {
    auto* page = new QWidget(this);
    auto* lay  = new QVBoxLayout(page);
    lay->setContentsMargins(24, 16, 24, 16); lay->setSpacing(10);
    lay->addWidget(makeBackBar("📦  Compila"));

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
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(buildMenu());          /* 0 — menu */
    m_stack->addWidget(buildVramBench());     /* 1 — VRAM */
    m_stack->addWidget(buildLlamaStudio());   /* 2 — llama */
    m_stack->addWidget(buildCythonStudio());   /* 3 — cython */
    m_stack->addWidget(buildCompila());       /* 4 — compila */

    lay->addWidget(m_stack);
}
