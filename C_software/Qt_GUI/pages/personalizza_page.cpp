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
#include <QThread>
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
   runProcArgs — avvia QProcess con argv separati (SICURO)
   Preferire questa versione a runProc() perché non passa la
   stringa a una shell: nessun rischio di injection su path
   con caratteri speciali (spazi, virgolette, semicolon).
   ────────────────────────────────────────────────────────────── */
void PersonalizzaPage::runProcArgs(QProcess* proc,
                                   const QString& program,
                                   const QStringList& args,
                                   QTextEdit* log, QPushButton* btn)
{
    log->append(QString("\xe2\x9a\x99\xef\xb8\x8f  %1 %2\n").arg(program, args.join(' ')));
    proc->setProcessChannelMode(QProcess::MergedChannels);

    connect(proc, &QProcess::readyRead, this, [this, proc, log]{
        log->moveCursor(QTextCursor::End);
        log->insertPlainText(QString::fromLocal8Bit(proc->readAll()));
        log->ensureCursorVisible();
    });
    connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, [log, btn, proc](int code, QProcess::ExitStatus){
        if (code == 0) log->append("\n\xe2\x9c\x85  Completato con successo.");
        else           log->append(QString("\n\xe2\x9d\x8c  Uscito con codice %1.").arg(code));
        if (btn) btn->setEnabled(true);
        proc->deleteLater();
    });
    proc->start(program, args);
    if (!proc->waitForStarted(4000)) {
        log->append("\xe2\x9d\x8c  Impossibile avviare il processo. Controlla il PATH.");
        if (btn) btn->setEnabled(true);
        proc->deleteLater();
    }
}

/* ──────────────────────────────────────────────────────────────
   runProc — DEPRECATO: usa sh -c con stringa concatenata.
   Mantenuto per compatibilità ma non usare per nuovi chiamanti.
   SICUREZZA: i path in cmd devono essere solo interni (non da
   input utente). Preferire runProcArgs() per tutti i nuovi usi.
   ────────────────────────────────────────────────────────────── */
void PersonalizzaPage::runProc(QProcess* proc, const QString& cmd,
                                QTextEdit* log, QPushButton* btn) {
    log->append(QString("\xe2\x9a\x99\xef\xb8\x8f  %1\n").arg(cmd));
    proc->setProcessChannelMode(QProcess::MergedChannels);

    connect(proc, &QProcess::readyRead, this, [this, proc, log]{
        log->moveCursor(QTextCursor::End);
        log->insertPlainText(QString::fromLocal8Bit(proc->readAll()));
        log->ensureCursorVisible();
    });
    connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, [log, btn, proc](int code, QProcess::ExitStatus){
        if (code == 0) log->append("\n\xe2\x9c\x85  Completato con successo.");
        else           log->append(QString("\n\xe2\x9d\x8c  Uscito con codice %1.").arg(code));
        if (btn) btn->setEnabled(true);
        proc->deleteLater();
    });
#ifdef _WIN32
    proc->start("cmd", {"/c", cmd});
#else
    proc->start("sh", {"-c", cmd});
#endif
    if (!proc->waitForStarted(4000)) {
        log->append("\xe2\x9d\x8c  Impossibile avviare il processo. Controlla il PATH.");
        if (btn) btn->setEnabled(true);
        proc->deleteLater();
    }
}


/* ══════════════════════════════════════════════════════════════
   PAGINA 1 — llama.cpp Studio
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

    bool binExists = QFileInfo::exists(P::llamaServerBin());

    struct SubItem { QString ico; QString title; QString desc; int pg; };
    QList<SubItem> subs = {
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

    connect(m_llamaCompBtn, &QPushButton::clicked, this, [this]{
        m_llamaLog->clear();
        m_llamaCompBtn->setEnabled(false);

        const QString studio   = P::llamaStudioDir();
        const QString cloneDir = studio + "/llama.cpp";
        const QString buildDir = cloneDir + "/build";

        /* ── Flag GPU — rilevazione argomenti cmake ── */
        QStringList gpuArgs = { "-DGGML_NATIVE=ON" };
#ifdef _WIN32
        gpuArgs << "-DGGML_CUDA=ON";
#else
        if (QFileInfo::exists("/usr/local/cuda/bin/nvcc") ||
            QFileInfo::exists("/usr/bin/nvcc"))
            gpuArgs << "-DGGML_CUDA=ON";
        else if (QFileInfo::exists("/opt/rocm/bin/hipcc"))
            gpuArgs << "-DGGML_HIP=ON";
        else if (QFileInfo::exists("/usr/bin/vulkaninfo"))
            gpuArgs << "-DGGML_VULKAN=ON";
#endif
        m_llamaLog->append(QString("\xf0\x9f\x93\xa6  Flag GPU: %1\n")
                           .arg(gpuArgs.join(" ")));

        /* Numero di job paralleli — senza shell: QThread::idealThreadCount()
         * è equivalente a nproc ma senza richiedere una shell. */
        const int jobs = qMax(1, QThread::idealThreadCount());

        /* ── SICUREZZA: 3 QProcess separati con arglist — nessuna shell ───────
           L'approccio precedente usava `sh -c "cmd && cmd && cmd"` con path
           costruiti a runtime. Se un path contenesse caratteri speciali (es. ")
           il comando shell si spezzava, aprendo a injection arbitraria.
           Ora ogni step è un QProcess::start() con argv esplicito: nessuna
           interpolazione di shell, nessun carattere di escape da gestire. */

        /* ── Step 1: git clone oppure git pull ── */
        QStringList gitArgs;
        if (QDir(cloneDir).exists()) {
            /* git -C <dir> pull — non richiede cd, non richiede shell */
            gitArgs = { "-C", cloneDir, "pull" };
            m_llamaLog->append("\xf0\x9f\x94\x84  git pull...\n");
        } else {
            gitArgs = { "clone", "--depth=1",
                        "https://github.com/ggerganov/llama.cpp",
                        cloneDir };
            m_llamaLog->append("\xf0\x9f\x93\xa5  git clone...\n");
        }

        auto* proc1 = new QProcess(this);
        proc1->setProcessChannelMode(QProcess::MergedChannels);
        connect(proc1, &QProcess::readyRead, this, [this, proc1]{
            m_llamaLog->moveCursor(QTextCursor::End);
            m_llamaLog->insertPlainText(QString::fromLocal8Bit(proc1->readAll()));
            m_llamaLog->ensureCursorVisible();
        });
        connect(proc1, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, proc1, cloneDir, buildDir, gpuArgs, jobs](int code, QProcess::ExitStatus){
            proc1->deleteLater();
            if (code != 0) {
                m_llamaLog->append(QString("\n\xe2\x9d\x8c  git fallito (code %1).").arg(code));
                m_llamaCompBtn->setEnabled(true);
                return;
            }
            m_llamaLog->append("\n\xe2\x9c\x85  git OK.\n\xe2\x9a\x99  cmake configure...\n");

            /* ── Step 2: cmake configure ── */
            QStringList cmakeConfigArgs = {
                "-B", buildDir,
                "-S", cloneDir,
                "-DCMAKE_BUILD_TYPE=Release",
            };
            cmakeConfigArgs += gpuArgs;

            auto* proc2 = new QProcess(this);
            proc2->setProcessChannelMode(QProcess::MergedChannels);
            connect(proc2, &QProcess::readyRead, this, [this, proc2]{
                m_llamaLog->moveCursor(QTextCursor::End);
                m_llamaLog->insertPlainText(QString::fromLocal8Bit(proc2->readAll()));
                m_llamaLog->ensureCursorVisible();
            });
            connect(proc2, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                    this, [this, proc2, buildDir, jobs](int code, QProcess::ExitStatus){
                proc2->deleteLater();
                if (code != 0) {
                    m_llamaLog->append(QString("\n\xe2\x9d\x8c  cmake configure fallito (code %1).").arg(code));
                    m_llamaCompBtn->setEnabled(true);
                    return;
                }
                m_llamaLog->append(QString("\n\xe2\x9c\x85  cmake OK.\n\xf0\x9f\x94\xa8  cmake build (-j%1)...\n").arg(jobs));

                /* ── Step 3: cmake build ── */
                auto* proc3 = new QProcess(this);
                proc3->setProcessChannelMode(QProcess::MergedChannels);
                connect(proc3, &QProcess::readyRead, this, [this, proc3]{
                    m_llamaLog->moveCursor(QTextCursor::End);
                    m_llamaLog->insertPlainText(QString::fromLocal8Bit(proc3->readAll()));
                    m_llamaLog->ensureCursorVisible();
                });
                connect(proc3, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                        this, [this, proc3](int code, QProcess::ExitStatus){
                    proc3->deleteLater();
                    if (code == 0)
                        m_llamaLog->append("\n\xe2\x9c\x85  llama.cpp compilato!\n"
                                           "  Binari in: llama_cpp_studio/llama.cpp/build/bin/");
                    else
                        m_llamaLog->append(QString("\n\xe2\x9d\x8c  Build fallita (code %1).").arg(code));
                    m_llamaCompBtn->setEnabled(true);
                });
                proc3->start("cmake", {
                    "--build", buildDir,
                    "--config", "Release",
                    "-j", QString::number(jobs)
                });
                if (!proc3->waitForStarted(5000)) {
                    m_llamaLog->append("\xe2\x9d\x8c  Impossibile avviare cmake build.");
                    m_llamaCompBtn->setEnabled(true);
                    proc3->deleteLater();
                }
            });
            proc2->start("cmake", cmakeConfigArgs);
            if (!proc2->waitForStarted(5000)) {
                m_llamaLog->append("\xe2\x9d\x8c  Impossibile avviare cmake configure.");
                m_llamaCompBtn->setEnabled(true);
                proc2->deleteLater();
            }
        });
        proc1->start("git", gitArgs);
        if (!proc1->waitForStarted(8000)) {
            m_llamaLog->append("\xe2\x9d\x8c  Impossibile avviare git. Verifica che git sia nel PATH.");
            m_llamaCompBtn->setEnabled(true);
            proc1->deleteLater();
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
   Costruttore principale
   ══════════════════════════════════════════════════════════════ */
PersonalizzaPage::PersonalizzaPage(QWidget* parent)
    : QWidget(parent)
{
    /* I widget vengono costruiti lazy da ImpostazioniPage via build*() */
}
