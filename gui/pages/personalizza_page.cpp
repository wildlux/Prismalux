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
   Nota: per helper proc transitori usiamo onHelperProcReadyRead /
   onHelperProcFinished che recuperano log/btn via property del proc.
   ────────────────────────────────────────────────────────────── */
void PersonalizzaPage::runProcArgs(QProcess* proc,
                                   const QString& program,
                                   const QStringList& args,
                                   QTextEdit* log, QPushButton* btn)
{
    log->append(QString("\xe2\x9a\x99\xef\xb8\x8f  %1 %2\n").arg(program, args.join(' ')));
    proc->setProcessChannelMode(QProcess::MergedChannels);
    /* Salva log e btn come proprietà dinamiche per recuperarle nello slot */
    proc->setProperty("_log", QVariant::fromValue(static_cast<QObject*>(log)));
    proc->setProperty("_btn", QVariant::fromValue(static_cast<QObject*>(btn)));

    connect(proc, &QProcess::readyRead,
            this, &PersonalizzaPage::onHelperProcReadyRead);
    connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &PersonalizzaPage::onHelperProcFinished);
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
   ────────────────────────────────────────────────────────────── */
void PersonalizzaPage::runProc(QProcess* proc, const QString& cmd,
                                QTextEdit* log, QPushButton* btn) {
    log->append(QString("\xe2\x9a\x99\xef\xb8\x8f  %1\n").arg(cmd));
    proc->setProcessChannelMode(QProcess::MergedChannels);
    proc->setProperty("_log", QVariant::fromValue(static_cast<QObject*>(log)));
    proc->setProperty("_btn", QVariant::fromValue(static_cast<QObject*>(btn)));

    connect(proc, &QProcess::readyRead,
            this, &PersonalizzaPage::onHelperProcReadyRead);
    connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &PersonalizzaPage::onHelperProcFinished);
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

/* ── slot helper per proc transitori (runProcArgs/runProc) ── */
void PersonalizzaPage::onHelperProcReadyRead() {
    auto* proc = qobject_cast<QProcess*>(sender());
    if (!proc) return;
    auto* log = qobject_cast<QTextEdit*>(
        proc->property("_log").value<QObject*>());
    if (!log) return;
    log->moveCursor(QTextCursor::End);
    log->insertPlainText(QString::fromLocal8Bit(proc->readAll()));
    log->ensureCursorVisible();
}

void PersonalizzaPage::onHelperProcFinished(int code, QProcess::ExitStatus) {
    auto* proc = qobject_cast<QProcess*>(sender());
    if (!proc) return;
    auto* log = qobject_cast<QTextEdit*>(
        proc->property("_log").value<QObject*>());
    auto* btn = qobject_cast<QPushButton*>(
        proc->property("_btn").value<QObject*>());
    if (log) {
        if (code == 0) log->append("\n\xe2\x9c\x85  Completato con successo.");
        else           log->append(QString("\n\xe2\x9d\x8c  Uscito con codice %1.").arg(code));
    }
    if (btn) btn->setEnabled(true);
    proc->deleteLater();
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
        {binExists ? "\xf0\x9f\x94\x84" : "\xf0\x9f\x94\xa8",
         binExists ? "Aggiorna llama.cpp" : "Compila llama.cpp",
         binExists ? "Riesegue git pull e ricompila con gli stessi flag GPU auto-rilevati."
                   : "Scarica e compila llama.cpp (git clone + cmake). Richiede git, cmake, gcc/g++.",
         1},
        {"\xf0\x9f\x93\x82", "Gestisci Modelli .gguf",
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
        btn->setProperty("_targetPage", s.pg);
        connect(btn, &QPushButton::clicked, this, &PersonalizzaPage::onSubMenuBtnClicked);
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
    connect(backComp, &QPushButton::clicked, this, &PersonalizzaPage::onBackCompClicked);
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

    connect(m_llamaCompBtn, &QPushButton::clicked,
            this, &PersonalizzaPage::onLlamaCompBtnClicked);

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
    connect(backMod, &QPushButton::clicked, this, &PersonalizzaPage::onBackModClicked);
    auto* modTitle  = new QLabel("\xf0\x9f\x93\x82  Gestione Modelli .gguf", modPg);
    modTitle->setObjectName("cardTitle");
    m_modDirLbl = new QLabel("", modPg);
    m_modDirLbl->setObjectName("cardDesc");
    auto* refreshBtn = new QPushButton("\xf0\x9f\x94\x84", modPg);
    refreshBtn->setObjectName("actionBtn"); refreshBtn->setFixedWidth(32);
    refreshBtn->setToolTip("Aggiorna lista");
    modTopL->addWidget(backMod);
    modTopL->addWidget(modTitle, 1);
    modTopL->addWidget(m_modDirLbl);
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
    m_listContainer = new QWidget;
    auto* listContLay   = new QVBoxLayout(m_listContainer);
    listContLay->setSpacing(4);
    listContLay->setContentsMargins(0,0,0,0);
    listContLay->addStretch(1);
    scrollArea->setWidget(m_listContainer);
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
    m_modDlLog  = makeLog("Output wget appare qui...");
    m_modUrlEdit = new QLineEdit(modDlBox);
    m_modUrlEdit->setObjectName("chatInput");
    m_modUrlEdit->setPlaceholderText("URL HuggingFace .gguf personalizzato...");
    m_modDlBtn  = new QPushButton("\xe2\xac\x87  Scarica URL", modDlBox);
    m_modDlBtn->setObjectName("actionBtn");

    int modRow = 0, modCol = 0;
    for (const auto& pr : kPresets) {
        auto* btn = new QPushButton(pr.name, modPresetGrid);
        btn->setObjectName("actionBtn");
        btn->setProperty("_url", QString(pr.url));
        connect(btn, &QPushButton::clicked,
                this, &PersonalizzaPage::onPresetBtnClicked);
        modPresetLay->addWidget(btn, modRow, modCol);
        modCol++; if (modCol >= 2) { modCol = 0; modRow++; }
    }
    modDlLay->addWidget(modPresetGrid);

    auto* modUrlRow = new QWidget(modDlBox);
    auto* modUrlLay = new QHBoxLayout(modUrlRow);
    modUrlLay->setContentsMargins(0,0,0,0); modUrlLay->setSpacing(6);
    modUrlLay->addWidget(m_modUrlEdit, 1); modUrlLay->addWidget(m_modDlBtn);
    modDlLay->addWidget(modUrlRow);

    /* ── Barra di avanzamento download ── */
    m_dlProgress = new QProgressBar(modDlBox);
    m_dlProgress->setRange(0, 100);
    m_dlProgress->setValue(0);
    m_dlProgress->setVisible(false);
    m_dlProgress->setTextVisible(true);
    m_dlProgress->setFormat("%p%  (%v MB / %m MB)");
    m_dlProgress->setStyleSheet(
        "QProgressBar{background:#1e293b;border:1px solid #334155;border-radius:4px;height:18px;}"
        "QProgressBar::chunk{background:#0ea5e9;border-radius:3px;}");
    modDlLay->addWidget(m_dlProgress);

    modDlLay->addWidget(m_modDlLog, 1);

    splitter->addWidget(listBox);
    splitter->addWidget(modDlBox);
    splitter->setSizes({200, 300});
    modLay->addWidget(splitter, 1);

    connect(m_modDlBtn,  &QPushButton::clicked,
            this, &PersonalizzaPage::onModDlBtnClicked);
    connect(refreshBtn,  &QPushButton::clicked,
            this, &PersonalizzaPage::onRefreshBtnClicked);
    connect(m_llamaStack, &QStackedWidget::currentChanged,
            this, &PersonalizzaPage::onLlamaStackChanged);

    /* Popola la lista la prima volta */
    refreshModelList();

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
    connect(backDl, &QPushButton::clicked, this, &PersonalizzaPage::onBackDlClicked);
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
    m_mathDlLog = makeLog("Log download...");

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
        dlBtn->setProperty("_url",  mm.url);
        dlBtn->setProperty("_name", mm.name);
        connect(dlBtn, &QPushButton::clicked,
                this, &PersonalizzaPage::onMathDlBtnClicked);

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
    m_dlCustomUrlEdit = new QLineEdit(customCard);
    m_dlCustomUrlEdit->setPlaceholderText("https://huggingface.co/.../resolve/main/modello.gguf");
    m_dlCustomBtn = new QPushButton("\xe2\xac\x87 Scarica", customCard);
    m_dlCustomBtn->setObjectName("actionBtn"); m_dlCustomBtn->setFixedWidth(100);
    urlRowL->addWidget(m_dlCustomUrlEdit, 1); urlRowL->addWidget(m_dlCustomBtn);
    ccL->addWidget(urlRow);
    listL->addWidget(customCard);

    connect(m_dlCustomBtn, &QPushButton::clicked,
            this, &PersonalizzaPage::onDlCustomBtnClicked);

    listL->addStretch(1);
    scroll->setWidget(listW);
    dlLay->addWidget(scroll, 1);
    dlLay->addWidget(m_mathDlLog);

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
            m_firstLaunchBanner = new QFrame(subMenu);
            m_firstLaunchBanner->setStyleSheet(
                "QFrame{background:#1c1a08;border:2px solid #f59e0b;"
                "border-radius:8px;padding:10px;}");
            auto* bannerLay = new QHBoxLayout(m_firstLaunchBanner);
            bannerLay->setSpacing(12);
            auto* ico = new QLabel("\xf0\x9f\x94\xa8", m_firstLaunchBanner);
            ico->setStyleSheet("font-size:22px;");
            auto* txt = new QLabel(
                "<b style='color:#fbbf24;'>Primo avvio: llama.cpp non \xc3\xa8 compilato</b><br>"
                "<span style='color:#d1d5db;font-size:11px;'>"
                "Clicca <b>Compila ora</b> per scaricare e compilare llama.cpp automaticamente. "
                "Richiede: git, cmake, gcc/g++ (Linux) o MSYS2 (Windows). "
                "Operazione una tantum (~5-15 min)."
                "</span>", m_firstLaunchBanner);
            txt->setWordWrap(true); txt->setTextFormat(Qt::RichText);
            auto* compNowBtn = new QPushButton("\xf0\x9f\x94\xa8  Compila ora", m_firstLaunchBanner);
            compNowBtn->setObjectName("actionBtn");
            compNowBtn->setStyleSheet(
                "QPushButton{background:#d97706;color:#fff;border-radius:5px;"
                "font-weight:bold;padding:5px 12px;}"
                "QPushButton:hover{background:#b45309;}");
            connect(compNowBtn, &QPushButton::clicked,
                    this, &PersonalizzaPage::onCompNowBtnClicked);
            bannerLay->addWidget(ico);
            bannerLay->addWidget(txt, 1);
            bannerLay->addWidget(compNowBtn);
            /* Inserisci il banner in cima al subMenu layout (prima delle card) */
            qobject_cast<QVBoxLayout*>(subMenu->layout())->insertWidget(0, m_firstLaunchBanner);
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

/* ══════════════════════════════════════════════════════════════
   SLOT — navigazione stack
   ══════════════════════════════════════════════════════════════ */
void PersonalizzaPage::onSubMenuBtnClicked() {
    auto* btn = qobject_cast<QPushButton*>(sender());
    if (!btn || !m_llamaStack) return;
    m_llamaStack->setCurrentIndex(btn->property("_targetPage").toInt());
}

void PersonalizzaPage::onBackCompClicked() {
    if (m_llamaStack) m_llamaStack->setCurrentIndex(0);
}

void PersonalizzaPage::onBackModClicked() {
    if (m_llamaStack) m_llamaStack->setCurrentIndex(0);
}

void PersonalizzaPage::onBackDlClicked() {
    if (m_llamaStack) m_llamaStack->setCurrentIndex(0);
}

void PersonalizzaPage::onCompNowBtnClicked() {
    if (m_firstLaunchBanner) m_firstLaunchBanner->setVisible(false);
    if (m_llamaStack) m_llamaStack->setCurrentIndex(1);
    if (m_llamaCompBtn) m_llamaCompBtn->click();
}

/* ══════════════════════════════════════════════════════════════
   SLOT — compilazione llama.cpp (avvio)
   ══════════════════════════════════════════════════════════════ */
void PersonalizzaPage::onLlamaCompBtnClicked() {
    if (!m_llamaLog || !m_llamaCompBtn) return;
    m_llamaLog->clear();
    m_llamaCompBtn->setEnabled(false);

    const QString studio   = P::llamaStudioDir();
    m_buildCloneDir = studio + "/llama.cpp";
    m_buildDir      = m_buildCloneDir + "/build";

    /* ── Flag GPU ── */
    m_buildGpuArgs = { "-DGGML_NATIVE=ON" };
#ifdef _WIN32
    m_buildGpuArgs << "-DGGML_CUDA=ON";
#else
    if (QFileInfo::exists("/usr/local/cuda/bin/nvcc") ||
        QFileInfo::exists("/usr/bin/nvcc"))
        m_buildGpuArgs << "-DGGML_CUDA=ON";
    else if (QFileInfo::exists("/opt/rocm/bin/hipcc"))
        m_buildGpuArgs << "-DGGML_HIP=ON";
    else if (QFileInfo::exists("/usr/bin/vulkaninfo"))
        m_buildGpuArgs << "-DGGML_VULKAN=ON";
#endif
    m_llamaLog->append(QString("\xf0\x9f\x93\xa6  Flag GPU: %1\n")
                       .arg(m_buildGpuArgs.join(" ")));

    m_buildJobs = qMax(1, QThread::idealThreadCount());

    /* ── Step 1: git clone oppure git pull ── */
    QStringList gitArgs;
    if (QDir(m_buildCloneDir).exists()) {
        gitArgs = { "-C", m_buildCloneDir, "pull" };
        m_llamaLog->append("\xf0\x9f\x94\x84  git pull...\n");
    } else {
        gitArgs = { "clone", "--depth=1",
                    "https://github.com/ggerganov/llama.cpp",
                    m_buildCloneDir };
        m_llamaLog->append("\xf0\x9f\x93\xa5  git clone...\n");
    }

    m_proc1 = new QProcess(this);
    m_proc1->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_proc1, &QProcess::readyRead,
            this, &PersonalizzaPage::onProc1ReadyRead);
    connect(m_proc1, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &PersonalizzaPage::onProc1Finished);
    m_proc1->start("git", gitArgs);
    if (!m_proc1->waitForStarted(8000)) {
        m_llamaLog->append("\xe2\x9d\x8c  Impossibile avviare git. Verifica che git sia nel PATH.");
        m_llamaCompBtn->setEnabled(true);
        m_proc1->deleteLater();
        m_proc1 = nullptr;
    }
}

void PersonalizzaPage::onProc1ReadyRead() {
    if (!m_proc1 || !m_llamaLog) return;
    m_llamaLog->moveCursor(QTextCursor::End);
    m_llamaLog->insertPlainText(QString::fromLocal8Bit(m_proc1->readAll()));
    m_llamaLog->ensureCursorVisible();
}

void PersonalizzaPage::onProc1Finished(int code, QProcess::ExitStatus) {
    if (!m_proc1) return;
    m_proc1->deleteLater();
    m_proc1 = nullptr;
    if (code != 0) {
        if (m_llamaLog)
            m_llamaLog->append(QString("\n\xe2\x9d\x8c  git fallito (code %1).").arg(code));
        if (m_llamaCompBtn) m_llamaCompBtn->setEnabled(true);
        return;
    }
    if (m_llamaLog)
        m_llamaLog->append("\n\xe2\x9c\x85  git OK.\n\xe2\x9a\x99  cmake configure...\n");

    /* ── Step 2: cmake configure ── */
    QStringList cmakeConfigArgs = {
        "-B", m_buildDir,
        "-S", m_buildCloneDir,
        "-DCMAKE_BUILD_TYPE=Release",
    };
    cmakeConfigArgs += m_buildGpuArgs;

    m_proc2 = new QProcess(this);
    m_proc2->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_proc2, &QProcess::readyRead,
            this, &PersonalizzaPage::onProc2ReadyRead);
    connect(m_proc2, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &PersonalizzaPage::onProc2Finished);
    m_proc2->start("cmake", cmakeConfigArgs);
    if (!m_proc2->waitForStarted(5000)) {
        if (m_llamaLog)
            m_llamaLog->append("\xe2\x9d\x8c  Impossibile avviare cmake configure.");
        if (m_llamaCompBtn) m_llamaCompBtn->setEnabled(true);
        m_proc2->deleteLater();
        m_proc2 = nullptr;
    }
}

void PersonalizzaPage::onProc2ReadyRead() {
    if (!m_proc2 || !m_llamaLog) return;
    m_llamaLog->moveCursor(QTextCursor::End);
    m_llamaLog->insertPlainText(QString::fromLocal8Bit(m_proc2->readAll()));
    m_llamaLog->ensureCursorVisible();
}

void PersonalizzaPage::onProc2Finished(int code, QProcess::ExitStatus) {
    if (!m_proc2) return;
    m_proc2->deleteLater();
    m_proc2 = nullptr;
    if (code != 0) {
        if (m_llamaLog)
            m_llamaLog->append(QString("\n\xe2\x9d\x8c  cmake configure fallito (code %1).").arg(code));
        if (m_llamaCompBtn) m_llamaCompBtn->setEnabled(true);
        return;
    }
    if (m_llamaLog)
        m_llamaLog->append(
            QString("\n\xe2\x9c\x85  cmake OK.\n\xf0\x9f\x94\xa8  cmake build (-j%1)...\n")
                .arg(m_buildJobs));

    /* ── Step 3: cmake build ── */
    m_proc3 = new QProcess(this);
    m_proc3->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_proc3, &QProcess::readyRead,
            this, &PersonalizzaPage::onProc3ReadyRead);
    connect(m_proc3, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &PersonalizzaPage::onProc3Finished);
    m_proc3->start("cmake", {
        "--build", m_buildDir,
        "--config", "Release",
        "-j", QString::number(m_buildJobs)
    });
    if (!m_proc3->waitForStarted(5000)) {
        if (m_llamaLog)
            m_llamaLog->append("\xe2\x9d\x8c  Impossibile avviare cmake build.");
        if (m_llamaCompBtn) m_llamaCompBtn->setEnabled(true);
        m_proc3->deleteLater();
        m_proc3 = nullptr;
    }
}

void PersonalizzaPage::onProc3ReadyRead() {
    if (!m_proc3 || !m_llamaLog) return;
    m_llamaLog->moveCursor(QTextCursor::End);
    m_llamaLog->insertPlainText(QString::fromLocal8Bit(m_proc3->readAll()));
    m_llamaLog->ensureCursorVisible();
}

void PersonalizzaPage::onProc3Finished(int code, QProcess::ExitStatus) {
    if (!m_proc3) return;
    m_proc3->deleteLater();
    m_proc3 = nullptr;
    if (m_llamaLog) {
        if (code == 0)
            m_llamaLog->append("\n\xe2\x9c\x85  llama.cpp compilato!\n"
                               "  Binari in: llama_cpp_studio/llama.cpp/build/bin/");
        else
            m_llamaLog->append(QString("\n\xe2\x9d\x8c  Build fallita (code %1).").arg(code));
    }
    if (m_llamaCompBtn) m_llamaCompBtn->setEnabled(true);
}

/* ══════════════════════════════════════════════════════════════
   SLOT — gestione modelli
   ══════════════════════════════════════════════════════════════ */
void PersonalizzaPage::onPresetBtnClicked() {
    auto* btn = qobject_cast<QPushButton*>(sender());
    if (!btn || !m_modUrlEdit) return;
    m_modUrlEdit->setText(btn->property("_url").toString());
}

void PersonalizzaPage::refreshModelList() {
    if (!m_listContainer) return;
    auto* listContLay = qobject_cast<QVBoxLayout*>(m_listContainer->layout());
    if (!listContLay) return;

    /* Rimuovi tutti i widget precedenti tranne lo stretch finale */
    while (listContLay->count() > 1) {
        auto* item = listContLay->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
    const QString modelsDir = P::modelsDir();
    if (m_modDirLbl)
        m_modDirLbl->setText(QString("\xf0\x9f\x93\x81  %1").arg(modelsDir));
    QDir d(modelsDir);
    if (!d.exists()) {
        auto* lbl = new QLabel("\xe2\x9a\xa0  Cartella models/ non trovata.", m_listContainer);
        lbl->setObjectName("cardDesc");
        listContLay->insertWidget(0, lbl);
        return;
    }
    auto files = d.entryInfoList({"*.gguf","*.bin"}, QDir::Files, QDir::Size | QDir::Reversed);
    if (files.isEmpty()) {
        auto* lbl = new QLabel("\xf0\x9f\x8c\xab  Nessun modello trovato — scaricane uno dalla sezione sotto.", m_listContainer);
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

        auto* card  = new QFrame(m_listContainer);
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
        delBtn->setProperty("_filePath",  f.absoluteFilePath());
        delBtn->setProperty("_fileName",  f.fileName());
        delBtn->setProperty("_sizeStr",   sizeStr);
        connect(delBtn, &QPushButton::clicked,
                this, &PersonalizzaPage::onDeleteModelBtnClicked);

        cl->addWidget(ico);
        cl->addWidget(name, 1);
        cl->addWidget(size);
        cl->addWidget(delBtn);
        listContLay->insertWidget(i, card);
    }
}

void PersonalizzaPage::onDeleteModelBtnClicked() {
    auto* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    const QString filePath = btn->property("_filePath").toString();
    const QString fileName = btn->property("_fileName").toString();
    const QString sizeStr  = btn->property("_sizeStr").toString();
    auto ans = QMessageBox::question(this, "Elimina modello",
        QString("Eliminare permanentemente:\n%1\n(%2)?").arg(fileName).arg(sizeStr),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ans == QMessageBox::Yes) {
        QFile::remove(filePath);
        refreshModelList();
    }
}

void PersonalizzaPage::onModDlBtnClicked() {
    if (!m_modUrlEdit || !m_modDlLog || !m_modDlBtn || !m_dlProgress) return;
    const QString url = m_modUrlEdit->text().trimmed();
    if (url.isEmpty()) return;
    QString fname = QUrl(url).fileName();
    if (fname.isEmpty()) fname = "modello.gguf";
    const QString dest = P::modelsDir() + "/" + fname;
    QDir().mkpath(P::modelsDir());
    m_modDlLog->clear();
    m_modDlLog->append(QString("\xe2\xac\x87  Scaricando %1\n\xe2\x86\x92  %2\n").arg(fname).arg(dest));
    m_modDlBtn->setEnabled(false);
    m_dlProgress->setValue(0);
    m_dlProgress->setVisible(true);

    m_modDlProc = new QProcess(this);
    m_modDlProc->setProcessChannelMode(QProcess::MergedChannels);
    m_modDlProc->setProperty("_dest",  dest);
    m_modDlProc->setProperty("_fname", fname);
    m_modDlProc->setProperty("_url",   url);

    connect(m_modDlProc, &QProcess::readyRead,
            this, &PersonalizzaPage::onModDlProcReadyRead);
    connect(m_modDlProc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &PersonalizzaPage::onModDlProcFinished);

    m_modDlProc->start("wget", {"-c", "--show-progress", "-O", dest, url});
    if (!m_modDlProc->waitForStarted(3000)) {
        m_modDlProc->deleteLater();
        m_modDlProc = nullptr;
        /* Fallback curl */
        m_curlProc = new QProcess(this);
        m_curlProc->setProcessChannelMode(QProcess::MergedChannels);
        m_curlProc->setProperty("_dest",  dest);
        m_curlProc->setProperty("_fname", fname);
        connect(m_curlProc, &QProcess::readyRead,
                this, &PersonalizzaPage::onCurlReadyRead);
        connect(m_curlProc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                this, &PersonalizzaPage::onCurlFinished);
        m_curlProc->start("curl", {"-L", "-C", "-", "--progress-bar", "-o", dest, url});
    }
}

void PersonalizzaPage::onModDlProcReadyRead() {
    auto* proc = qobject_cast<QProcess*>(sender());
    if (!proc || !m_modDlLog || !m_dlProgress) return;
    const QString raw = QString::fromUtf8(proc->readAll());
    static const QRegularExpression rePct(R"((\d{1,3})\s*%)");
    const auto match = rePct.match(raw);
    if (match.hasMatch()) {
        const int pct = match.captured(1).toInt();
        if (pct >= 0 && pct <= 100) m_dlProgress->setValue(pct);
    }
    const QStringList lines = raw.split('\n', Qt::SkipEmptyParts);
    if (!lines.isEmpty()) {
        const QString last = lines.last().trimmed();
        if (!last.isEmpty()) m_modDlLog->append(last);
    }
}

void PersonalizzaPage::onModDlProcFinished(int code, QProcess::ExitStatus) {
    if (!m_modDlProc) return;
    m_modDlProc->deleteLater();
    m_modDlProc = nullptr;
    if (!m_modDlBtn || !m_dlProgress || !m_modDlLog) return;
    m_modDlBtn->setEnabled(true);
    if (code == 0) {
        m_dlProgress->setValue(100);
        m_modDlLog->append("\n\xe2\x9c\x85  Download completato!");
        refreshModelList();
    } else {
        m_dlProgress->setVisible(false);
        m_modDlLog->append("\n\xe2\x9d\x8c  Download fallito o interrotto.");
    }
}

void PersonalizzaPage::onCurlReadyRead() {
    auto* proc = qobject_cast<QProcess*>(sender());
    if (!proc || !m_modDlLog || !m_dlProgress) return;
    const QString raw = QString::fromUtf8(proc->readAll());
    static const QRegularExpression rePct(R"(\s(\d{1,3})\s)");
    const auto match = rePct.match(raw);
    if (match.hasMatch()) {
        const int pct = match.captured(1).toInt();
        if (pct >= 0 && pct <= 100) m_dlProgress->setValue(pct);
    }
    const QStringList lines = raw.split('\n', Qt::SkipEmptyParts);
    if (!lines.isEmpty()) m_modDlLog->append(lines.last().trimmed());
}

void PersonalizzaPage::onCurlFinished(int code, QProcess::ExitStatus) {
    if (!m_curlProc) return;
    m_curlProc->deleteLater();
    m_curlProc = nullptr;
    if (!m_modDlBtn || !m_dlProgress || !m_modDlLog) return;
    m_modDlBtn->setEnabled(true);
    if (code == 0) {
        m_dlProgress->setValue(100);
        m_modDlLog->append("\n\xe2\x9c\x85  Download completato!");
        refreshModelList();
    } else {
        m_dlProgress->setVisible(false);
        m_modDlLog->append("\n\xe2\x9d\x8c  Errore. Installa wget o curl.");
    }
}

void PersonalizzaPage::onRefreshBtnClicked() {
    refreshModelList();
}

void PersonalizzaPage::onLlamaStackChanged(int idx) {
    if (idx == 2) refreshModelList();
}

/* ══════════════════════════════════════════════════════════════
   SLOT — download modelli matematica/logica
   ══════════════════════════════════════════════════════════════ */
void PersonalizzaPage::onMathDlBtnClicked() {
    auto* btn = qobject_cast<QPushButton*>(sender());
    if (!btn || !m_mathDlLog) return;
    const QString url  = btn->property("_url").toString();
    const QString name = btn->property("_name").toString();

    QString fname = url.section('/', -1);
    if (fname.isEmpty()) fname = "modello.gguf";
    QString dest = P::modelsDir() + "/" + fname;
    m_mathDlLog->append(QString("\xf0\x9f\x93\xa5  Scarico: %1\n   \xe2\x86\x92 %2\n")
                        .arg(name).arg(dest));
    btn->setEnabled(false);

    auto* proc = new QProcess(this);
    proc->setProcessChannelMode(QProcess::MergedChannels);
    proc->setProperty("_dest",  dest);
    proc->setProperty("_fname", fname);
    proc->setProperty("_btn",   QVariant::fromValue(static_cast<QObject*>(btn)));
    connect(proc, &QProcess::readyRead,
            this, &PersonalizzaPage::onMathDlProcReadyRead);
    connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &PersonalizzaPage::onMathDlProcFinished);
#ifdef _WIN32
    proc->start("cmd", {"/c",
        QString("curl -L -o \"%1\" \"%2\" --progress-bar").arg(dest).arg(url)});
#else
    proc->start("wget", {"-c", "-O", dest, "--show-progress", url});
#endif
    if (!proc->waitForStarted(4000)) {
        m_mathDlLog->append("\xe2\x9d\x8c  wget/curl non trovato. Installa wget (Linux) o curl (Windows).");
        btn->setEnabled(true);
        proc->deleteLater();
    }
}

void PersonalizzaPage::onMathDlProcReadyRead() {
    auto* proc = qobject_cast<QProcess*>(sender());
    if (!proc || !m_mathDlLog) return;
    m_mathDlLog->moveCursor(QTextCursor::End);
    m_mathDlLog->insertPlainText(QString::fromLocal8Bit(proc->readAll()));
    m_mathDlLog->ensureCursorVisible();
}

void PersonalizzaPage::onMathDlProcFinished(int code, QProcess::ExitStatus) {
    auto* proc = qobject_cast<QProcess*>(sender());
    if (!proc || !m_mathDlLog) return;
    const QString fname = proc->property("_fname").toString();
    auto* btn = qobject_cast<QPushButton*>(proc->property("_btn").value<QObject*>());
    if (code == 0)
        m_mathDlLog->append(QString("\n\xe2\x9c\x85  Download completato: %1").arg(fname));
    else
        m_mathDlLog->append(QString("\n\xe2\x9d\x8c  Errore (codice %1). Controlla la connessione.").arg(code));
    if (btn) btn->setEnabled(true);
    proc->deleteLater();
}

void PersonalizzaPage::onDlCustomBtnClicked() {
    if (!m_dlCustomUrlEdit || !m_mathDlLog || !m_dlCustomBtn) return;
    const QString url = m_dlCustomUrlEdit->text().trimmed();
    if (url.isEmpty()) { m_mathDlLog->append("\xe2\x9d\x8c  Inserisci un URL valido."); return; }
    QString fname = url.section('/', -1);
    if (fname.isEmpty()) fname = "modello.gguf";
    QString dest = P::modelsDir() + "/" + fname;
    m_mathDlLog->append(QString("\xf0\x9f\x93\xa5  Scarico URL personalizzato:\n   %1\n   \xe2\x86\x92 %2\n")
                        .arg(url).arg(dest));
    m_dlCustomBtn->setEnabled(false);

    m_dlCustomProc = new QProcess(this);
    m_dlCustomProc->setProcessChannelMode(QProcess::MergedChannels);
    m_dlCustomProc->setProperty("_dest",  dest);
    m_dlCustomProc->setProperty("_fname", fname);
    connect(m_dlCustomProc, &QProcess::readyRead,
            this, &PersonalizzaPage::onDlCustomProcReadyRead);
    connect(m_dlCustomProc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &PersonalizzaPage::onDlCustomProcFinished);
#ifdef _WIN32
    m_dlCustomProc->start("cmd", {"/c",
        QString("curl -L -o \"%1\" \"%2\" --progress-bar").arg(dest).arg(url)});
#else
    m_dlCustomProc->start("wget", {"-c", "-O", dest, "--show-progress", url});
#endif
    if (!m_dlCustomProc->waitForStarted(4000)) {
        m_mathDlLog->append("\xe2\x9d\x8c  wget/curl non trovato.");
        m_dlCustomBtn->setEnabled(true);
        m_dlCustomProc->deleteLater();
        m_dlCustomProc = nullptr;
    }
}

void PersonalizzaPage::onDlCustomProcReadyRead() {
    if (!m_dlCustomProc || !m_mathDlLog) return;
    m_mathDlLog->moveCursor(QTextCursor::End);
    m_mathDlLog->insertPlainText(QString::fromLocal8Bit(m_dlCustomProc->readAll()));
    m_mathDlLog->ensureCursorVisible();
}

void PersonalizzaPage::onDlCustomProcFinished(int code, QProcess::ExitStatus) {
    if (!m_dlCustomProc || !m_mathDlLog) return;
    const QString fname = m_dlCustomProc->property("_fname").toString();
    m_dlCustomProc->deleteLater();
    m_dlCustomProc = nullptr;
    if (!m_dlCustomBtn) return;
    if (code == 0)
        m_mathDlLog->append(QString("\n\xe2\x9c\x85  Download completato: %1").arg(fname));
    else
        m_mathDlLog->append(QString("\n\xe2\x9d\x8c  Errore (codice %1).").arg(code));
    m_dlCustomBtn->setEnabled(true);
}
