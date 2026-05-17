#include "impostazioni_page.h"
#include "../widgets/toggle_switch.h"
#include "../widgets/stt_whisper.h"
#include "personalizza_page.h"
#include "manutenzione_page.h"
#include "grafico_page.h"
#include "../theme_manager.h"
#include "../prismalux_paths.h"
namespace P = PrismaluxPaths;
#include "../app_config.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QTabWidget>
#include <QScrollArea>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QButtonGroup>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QSettings>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QFileDialog>
#include <QDateTime>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <functional>
#include <memory>
#include <QCoreApplication>
#include <QTimer>
#include <QRadioButton>
#include <QCheckBox>
#include <QListWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QGroupBox>
#include <QPlainTextEdit>
#include <QThread>
#include <QStackedWidget>
#include <QColorDialog>
#include <QPainter>
#include <QPen>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QGuiApplication>
#include <QClipboard>
#include <QMessageBox>
#include <QProgressBar>
#include <QTextEdit>
#include <QTextBrowser>
#include <QDialogButtonBox>
#include <QDialog>
#include <QRegularExpression>

QWidget* ImpostazioniPage::buildAiLocaleTab()
{
    auto* page    = new QWidget;
    auto* mainLay = new QVBoxLayout(page);
    mainLay->setContentsMargins(16, 16, 16, 12);
    mainLay->setSpacing(12);

    /* Titolo */
    auto* titleLbl = new QLabel("\xf0\x9f\xa6\x99  AI Locale \xe2\x80\x94 Gestori & Modelli", page);
    titleLbl->setObjectName("sectionTitle");
    mainLay->addWidget(titleLbl);

    /* ── 2 colonne ── */
    auto* colsRow = new QWidget(page);
    auto* colsLay = new QHBoxLayout(colsRow);
    colsLay->setContentsMargins(0, 0, 0, 0);
    colsLay->setSpacing(16);

    /* ── Colonna sinistra: selettore gestore ── */
    auto* leftGroup = new QGroupBox("\xf0\x9f\x94\xa7  Gestore LLM", colsRow);
    leftGroup->setObjectName("cardGroup");
    leftGroup->setFixedWidth(200);
    auto* leftLay = new QVBoxLayout(leftGroup);
    leftLay->setSpacing(10);

    auto* btnOllama = new QRadioButton("\xf0\x9f\x90\xb3  Ollama", leftGroup);
    btnOllama->setChecked(true);
    btnOllama->setAccessibleName("Backend Ollama");
    auto* btnLlama  = new QRadioButton("\xf0\x9f\xa6\x99  llama.cpp", leftGroup);
    btnLlama->setAccessibleName("Backend llama.cpp");

    leftLay->addWidget(btnOllama);
    leftLay->addWidget(btnLlama);

    auto* sepLeft = new QFrame(leftGroup);
    sepLeft->setFrameShape(QFrame::HLine);
    sepLeft->setObjectName("sidebarSep");
    leftLay->addWidget(sepLeft);

    auto* statusLbl = new QLabel("", leftGroup);
    statusLbl->setObjectName("cardDesc");
    statusLbl->setWordWrap(true);
    leftLay->addWidget(statusLbl);

    auto* refreshBtn = new QPushButton("\xf0\x9f\x94\x84  Aggiorna lista", leftGroup);
    refreshBtn->setObjectName("actionBtn");
    refreshBtn->setAccessibleName("Aggiorna lista modelli disponibili");
    leftLay->addWidget(refreshBtn);

    /* llama.cpp Studio rimosso: funzionalità integrate in scheda LLM */

    /* ── Sezione Ollama LAN ── */
    auto* sepLan = new QFrame(leftGroup);
    sepLan->setFrameShape(QFrame::HLine);
    sepLan->setObjectName("sidebarSep");
    leftLay->addWidget(sepLan);

    auto* lanTitleLbl = new QLabel("\xf0\x9f\x93\xb1  Ollama LAN", leftGroup);
    lanTitleLbl->setObjectName("cardDesc");
    leftLay->addWidget(lanTitleLbl);

    auto* lanStatusLbl = new QLabel("\xe2\x9a\xaa Inattivo", leftGroup);
    lanStatusLbl->setObjectName("hintLabel");
    lanStatusLbl->setWordWrap(true);
    leftLay->addWidget(lanStatusLbl);

    auto* lanBtn = new QPushButton("\xf0\x9f\x9f\xa2  Avvia Ollama LAN", leftGroup);
    lanBtn->setObjectName("actionBtn");
    lanBtn->setToolTip("Avvia ollama serve con OLLAMA_HOST=0.0.0.0:11434\n"
                       "cos\xc3\xac il telefono in LAN pu\xc3\xb2 connettersi");
    leftLay->addWidget(lanBtn);

    leftLay->addStretch(1);
    colsLay->addWidget(leftGroup);

    /* ── Colonna destra: lista modelli ── */
    auto* rightGroup = new QGroupBox("\xf0\x9f\x93\xa6  Modelli disponibili", colsRow);
    rightGroup->setObjectName("cardGroup");
    auto* rightLay = new QVBoxLayout(rightGroup);
    rightLay->setSpacing(8);

    /* Hint sopra la lista */
    auto* hintLbl = new QLabel("Seleziona un gestore per vedere i modelli installati.", rightGroup);
    hintLbl->setObjectName("hintLabel");
    hintLbl->setWordWrap(true);
    rightLay->addWidget(hintLbl);

    auto* modelList = new QListWidget(rightGroup);
    modelList->setObjectName("modelsList");
    modelList->setAlternatingRowColors(true);
    rightLay->addWidget(modelList, 1);

    colsLay->addWidget(rightGroup, 1);
    mainLay->addWidget(colsRow);

    /* ══════════════════════════════════════════════════
       Sezione Ragionamento AI
       ══════════════════════════════════════════════════ */
    {
        auto* thinkGroup = new QGroupBox(
            "\xf0\x9f\xa7\xa0  Ragionamento AI (Think Mode)", page);  /* 🧠 */
        thinkGroup->setObjectName("cardGroup");
        auto* thinkLay = new QVBoxLayout(thinkGroup);
        thinkLay->setSpacing(8);

        /* ── Riga superiore: modalità + budget ── */
        auto* topRow = new QHBoxLayout;
        topRow->setSpacing(16);

        /* Modalità: Off / Auto / On */
        auto* modeLbl = new QLabel("Modalit\xc3\xa0:", thinkGroup);
        modeLbl->setObjectName("cardDesc");
        topRow->addWidget(modeLbl);

        auto* btnOff  = new QPushButton("Off",  thinkGroup);
        auto* btnAuto = new QPushButton("Auto", thinkGroup);
        auto* btnOn   = new QPushButton("On",   thinkGroup);
        for (auto* b : {btnOff, btnAuto, btnOn}) {
            b->setCheckable(true);
            b->setFixedWidth(56);
            b->setObjectName("thinkModeBtn");
        }
        auto* modeGrp = new QButtonGroup(thinkGroup);
        modeGrp->setExclusive(true);
        modeGrp->addButton(btnOff,  1);  /* id=1 → thinkMode=1 (off) */
        modeGrp->addButton(btnAuto, 0);  /* id=0 → thinkMode=0 (auto) */
        modeGrp->addButton(btnOn,   2);  /* id=2 → thinkMode=2 (on)  */
        topRow->addWidget(btnOff);
        topRow->addWidget(btnAuto);
        topRow->addWidget(btnOn);

        topRow->addSpacing(24);

        /* Budget token (slider 1–4×) */
        auto* budgetLbl = new QLabel("Budget token:", thinkGroup);
        budgetLbl->setObjectName("cardDesc");
        topRow->addWidget(budgetLbl);

        auto* budgetSlider = new QSlider(Qt::Horizontal, thinkGroup);
        budgetSlider->setRange(1, 4);
        budgetSlider->setFixedWidth(120);
        budgetSlider->setTickInterval(1);
        budgetSlider->setTickPosition(QSlider::TicksBelow);
        topRow->addWidget(budgetSlider);

        auto* budgetValLbl = new QLabel("2\xc3\x97", thinkGroup);  /* 2× */
        budgetValLbl->setObjectName("cardDesc");
        budgetValLbl->setFixedWidth(28);
        topRow->addWidget(budgetValLbl);

        topRow->addStretch(1);
        thinkLay->addLayout(topRow);

        /* ── Avviso RAM bassa ── */
        const qint64 ramTot = P::totalRamBytes();
        auto* ramWarnLbl = new QLabel(
            "\xe2\x9a\xa0  Con poca RAM (\xe2\x89\xa4"  /* ≤ */
            " 12\xc2\xa0GB), il thinking pu\xc3\xb2 raddoppiare i token generati "
            "\xe2\x80\x94 potrebbe rallentare la risposta.",
            thinkGroup);
        ramWarnLbl->setObjectName("hintLabel");
        ramWarnLbl->setWordWrap(true);
        ramWarnLbl->setStyleSheet("color: #f59e0b;");
        ramWarnLbl->setVisible(ramTot > 0 && ramTot < 12LL * 1024 * 1024 * 1024);
        thinkLay->addWidget(ramWarnLbl);

        /* ── Hint descrittivo ── */
        auto* hintLbl = new QLabel(
            "<small>"
            "<b>Off</b>: nessun ragionamento interno (pi\xc3\xb9 veloce). "  /* più */
            "<b>Auto</b>: il classificatore decide per ogni domanda (default). "
            "<b>On</b>: ragionamento sempre attivo sui modelli compatibili "
            "(qwen3, deepseek-r1, qwq).<br>"
            "<b>Budget</b>: moltiplicatore dei token di risposta quando il thinking \xc3\xa8 attivo "  /* è */
            "(1\xc3\x97 = base, 2\xc3\x97 = default, 4\xc3\x97 = risposta molto elaborata)."
            "</small>",
            thinkGroup);
        hintLbl->setObjectName("hintLabel");
        hintLbl->setWordWrap(true);
        hintLbl->setTextFormat(Qt::RichText);
        thinkLay->addWidget(hintLbl);

        mainLay->addWidget(thinkGroup);

        /* ── Logica: ripristina stato dal file ── */
        const AiChatParams curP = AiChatParams::load();
        /* Seleziona il pulsante corretto in base a thinkMode */
        if (auto* btn = modeGrp->button(curP.thinkMode))
            btn->setChecked(true);
        else
            btnAuto->setChecked(true);

        budgetSlider->setValue(qBound(1, curP.thinkBudget, 4));
        budgetValLbl->setText(QString::number(budgetSlider->value()) + "\xc3\x97");

        /* Il budget è attivo solo quando thinking è abilitato (Auto o On) */
        const bool budgetActive = (curP.thinkMode != 1);
        budgetSlider->setEnabled(budgetActive);
        budgetValLbl->setEnabled(budgetActive);

        /* ── Salva quando cambia la modalità ── */
        m_thinkBudgetSlider = budgetSlider;
        m_thinkBudgetValLbl = budgetValLbl;
        m_thinkRamWarnLbl   = ramWarnLbl;
        connect(modeGrp, QOverload<int>::of(&QButtonGroup::idClicked),
                this, &ImpostazioniPage::onThinkModeIdClicked);

        /* ── Salva quando cambia il budget ── */
        connect(budgetSlider, &QSlider::valueChanged,
                this, &ImpostazioniPage::onThinkBudgetChanged);
    }

    /* ══════════════════════════════════════════════════
       Sezione Memoria persistente (Knowledge)
       ══════════════════════════════════════════════════ */
    {
        auto* knGroup = new QGroupBox(
            "\xf0\x9f\x93\x96  Memoria persistente (Knowledge)", page);  /* 📖 */
        knGroup->setObjectName("cardGroup");
        auto* knLay = new QVBoxLayout(knGroup);
        knLay->setSpacing(8);

        /* Riga: checkbox + pulsante Apri file */
        auto* chkRow = new QHBoxLayout;
        auto* chkKn  = new QCheckBox(
            "Inietta contesto utente nel prompt", knGroup);
        chkKn->setObjectName("cardDesc");
        {
            chkKn->setChecked(AppConfig::s().value(P::SK::kInjectUserKnowledge, true).toBool());
        }
        chkRow->addWidget(chkKn);

        auto* openFileBtn = new QPushButton("Apri file", knGroup);
        openFileBtn->setObjectName("actionBtn");
        openFileBtn->setFixedWidth(90);
        openFileBtn->setToolTip(P::userKnowledgePath());
        chkRow->addWidget(openFileBtn);
        chkRow->addStretch(1);
        knLay->addLayout(chkRow);

        auto* knHint = new QLabel(
            "<small>Se attivo, <b>KNOWLEDGE_USER/user_knowledge.md</b> viene preposto al "
            "system prompt di ogni richiesta AI — fornisce contesto persistente "
            "su chi sei, preferenze e progetto corrente.</small>",
            knGroup);
        knHint->setObjectName("hintLabel");
        knHint->setWordWrap(true);
        knHint->setTextFormat(Qt::RichText);
        knLay->addWidget(knHint);

        mainLay->addWidget(knGroup);

        /* Salva la preferenza e invalida la cache di lettura */
        connect(chkKn, &QCheckBox::toggled,
                this, &ImpostazioniPage::onKnowledgeInjectToggled);

        /* Apre user_knowledge.md nel text editor di sistema */
        connect(openFileBtn, &QPushButton::clicked,
                this, &ImpostazioniPage::onOpenKnowledgeFileClicked);
    }

    /* ── Dipendenze — occupa tutto lo spazio residuo ── */
    auto* sepBot = new QFrame(page);
    sepBot->setFrameShape(QFrame::HLine);
    sepBot->setObjectName("sidebarSep");
    mainLay->addWidget(sepBot);

    mainLay->addWidget(buildDipendenzeTab(), 1);

    /* ── Logica: popola la lista in base al gestore selezionato ── */

    /* Label che mostra il modello attivo corrente */
    auto* activeLbl = new QLabel(
        QString("\xf0\x9f\x9f\xa2  Modello attivo: <b>%1</b>").arg(
            m_ai->model().isEmpty() ? "(nessuno)" : m_ai->model()),
        leftGroup);
    activeLbl->setObjectName("cardDesc");
    activeLbl->setWordWrap(true);
    activeLbl->setTextFormat(Qt::RichText);
    leftLay->insertWidget(2, activeLbl);  /* sotto i radio button */

    /* Pulsante "Usa modello selezionato" */
    auto* useBtn = new QPushButton("\xe2\x9c\x94  Usa modello", leftGroup);
    useBtn->setObjectName("actionBtn");
    useBtn->setEnabled(false);
    useBtn->setToolTip("Imposta il modello selezionato come modello attivo");
    useBtn->setAccessibleName("Attiva modello selezionato");
    leftLay->insertWidget(3, useBtn);

    /* Salva i puntatori come member variables per i slot */
    m_btnOllamaRadio = btnOllama;
    m_btnLlamaRadio  = btnLlama;
    m_aiModelList    = modelList;
    m_aiStatusLbl    = statusLbl;
    m_aiHintLbl      = hintLbl;
    m_aiUseBtn       = useBtn;
    m_aiActiveLbl    = activeLbl;
    m_lanStatusLbl   = lanStatusLbl;
    m_lanBtn         = lanBtn;

    /* Cambio gestore */
    connect(btnOllama, &QRadioButton::toggled,
            this, &ImpostazioniPage::onOllamaRadioToggled);
    connect(btnLlama, &QRadioButton::toggled,
            this, &ImpostazioniPage::onLlamaRadioToggled);

    /* Aggiorna lista */
    connect(refreshBtn, &QPushButton::clicked,
            this, &ImpostazioniPage::onAiLocalRefreshClicked);

    /* Abilita "Usa modello" quando si seleziona un item */
    connect(modelList, &QListWidget::currentItemChanged,
            this, &ImpostazioniPage::onAiModelListItemChanged);

    /* Doppio clic = attiva subito */
    connect(modelList, &QListWidget::itemDoubleClicked,
            this, &ImpostazioniPage::onAiModelListDblClicked);

    /* Pulsante "Usa modello" */
    connect(useBtn, &QPushButton::clicked,
            this, &ImpostazioniPage::applySelectedModel);

    /* Aggiorna label modello attivo quando cambia dall'esterno */
    connect(m_ai, &AiClient::modelsReady,
            this, &ImpostazioniPage::onAiModelsReadyUpdateLabel);

    /* Popola la lista in base al backend attivo all'apertura */
    QTimer::singleShot(0, this, &ImpostazioniPage::onAiLocalTabInit);

    /* ── Logica Ollama LAN ── */
    m_ollamaLanProc = new QProcess(this);
    m_ollamaLanProc->setProcessChannelMode(QProcess::MergedChannels);

    connect(lanBtn, &QPushButton::clicked,
            this, &ImpostazioniPage::onOllamaLanBtnClicked);
    connect(m_ollamaLanProc, &QProcess::started,
            this, &ImpostazioniPage::onOllamaLanStarted);
    connect(m_ollamaLanProc, &QProcess::errorOccurred,
            this, &ImpostazioniPage::onOllamaLanError);
    connect(m_ollamaLanProc,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ImpostazioniPage::onOllamaLanFinished);

    return page;
}

/* ══════════════════════════════════════════════════════════════
   buildDipendenzeTab — lista dipendenze esterne con verifica
   ══════════════════════════════════════════════════════════════ */
QWidget* ImpostazioniPage::buildDipendenzeTab()
{
    struct Dep {
        const char* name;
        const char* exec;     /* eseguibile da cercare — vuoto = sempre OK */
        const char* desc;
        const char* install;
        bool        optional;
    };

    static const Dep kDeps[] = {
        /* ── Backend AI ── */
        { "Qt6 / Qt5",    "",              "Framework GUI — Core, Widgets, Network",
          "sudo apt install qt6-base-dev libqt6network6-dev",         false },
        { "Ollama",       "ollama",        "Server LLM locale (NDJSON streaming)",
          "curl -fsSL https://ollama.com/install.sh | sh",            false },
        { "llama-server", "llama-server",
          "Server llama.cpp (OpenAI/SSE) \xe2\x80\x94 "
          "gi\xc3\xa0 compilato in gui/build/bin/ (Linux) o gui\\build\\bin\\ (Windows). "
          "Se non trovato: esegui build.sh (Linux) o build.bat (Windows) dalla root.",
#ifdef Q_OS_WIN
          "gi\xc3\xa0 compilato: usa build.bat dalla root. Binario in gui\\build\\bin\\llama-server.exe",
#else
          "gi\xc3\xa0 compilato: usa build.sh dalla root. Binario in gui/build/bin/llama-server",
#endif
          true  },
        { "llama-cli",    "llama-cli",
          "Chat diretta con modelli GGUF \xe2\x80\x94 compilato insieme a llama-server "
          "in gui/build/bin/ (Linux) o gui\\build\\bin\\ (Windows).",
#ifdef Q_OS_WIN
          "gi\xc3\xa0 compilato: binario in gui\\build\\bin\\llama-cli.exe (stesso processo di llama-server)",
#else
          "gi\xc3\xa0 compilato: binario in gui/build/bin/llama-cli (stesso processo di llama-server)",
#endif
          true  },

        /* ── Documenti ── */
        { "pdftotext",    "pdftotext",     "Estrae testo da PDF (Strumenti > Documenti)",
          "sudo apt install poppler-utils",                            false },

        /* ── Download modelli ── */
        { "wget",         "wget",          "Scarica modelli GGUF da HuggingFace",
          "sudo apt install wget",                                     false },

        /* ── Audio — TTS ── */
        { "aplay",        "aplay",         "Riproduce audio PCM raw (sintesi vocale)",
          "sudo apt install alsa-utils",                               false },
        { "piper",        "piper",         "Text-to-Speech offline ONNX (voce italiana)",
          "https://github.com/rhasspy/piper  — scarica il binario",   true  },
        { "espeak-ng",    "espeak-ng",     "TTS fallback leggero (se Piper assente)",
          "sudo apt install espeak-ng",                                true  },
        { "spd-say",      "spd-say",       "TTS fallback via speech-dispatcher",
          "sudo apt install speech-dispatcher",                        true  },

        /* ── Audio — STT ── */
        { "arecord",      "arecord",       "Registra audio per riconoscimento vocale",
          "sudo apt install alsa-utils",                               false },
        { "whisper-cli",  "whisper-cli",   "Riconoscimento vocale offline (whisper.cpp)",
          "sudo apt install whisper-cpp",                              true  },

        /* ── GPU / Hardware ── */
        { "nvidia-smi",   "nvidia-smi",    "Info GPU NVIDIA e VRAM benchmark",
          "Installare i driver NVIDIA proprietari",                    true  },
    };

    /* ── Costruzione pagina ── */
    auto* page  = new QWidget;
    auto* outer = new QVBoxLayout(page);
    outer->setContentsMargins(16, 16, 16, 16);
    outer->setSpacing(10);

    auto* title = new QLabel(
        "\xf0\x9f\x93\xa6  Dipendenze esterne");
    title->setObjectName("sectionTitle");
    outer->addWidget(title);

    auto* desc = new QLabel(
        "Librerie e strumenti necessari o opzionali. "
        "Premi \xc2\xab Verifica tutto \xc2\xbb per controllare "
        "cosa \xc3\xa8 disponibile nel sistema.");
    desc->setWordWrap(true);
    desc->setObjectName("hintLabel");
    outer->addWidget(desc);

    auto* verifyBtn = new QPushButton(
        "\xf0\x9f\x94\x8d  Verifica tutto");
    verifyBtn->setObjectName("actionBtn");
    verifyBtn->setFixedWidth(160);
    outer->addWidget(verifyBtn, 0, Qt::AlignLeft);

    /* ── Scroll area ── */
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* container  = new QWidget;
    auto* listLayout = new QVBoxLayout(container);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->setSpacing(2);

    /* Intestazione colonne */
    {
        auto* hdr    = new QWidget;
        auto* hdrLay = new QHBoxLayout(hdr);
        hdrLay->setContentsMargins(8, 2, 8, 2);
        hdrLay->setSpacing(10);
        auto makeHdr = [](const QString& t, int w = -1) {
            auto* l = new QLabel("<b>" + t + "</b>");
            l->setObjectName("hintLabel");
            if (w > 0) l->setFixedWidth(w);
            return l;
        };
        hdrLay->addWidget(makeHdr("", 20));
        hdrLay->addWidget(makeHdr("Nome", 160));
        hdrLay->addWidget(makeHdr("Descrizione"), 1);
        hdrLay->addWidget(makeHdr("Installazione", 260));
        listLayout->addWidget(hdr);
    }

    /* Separatore */
    auto* sepTop = new QFrame;
    sepTop->setFrameShape(QFrame::HLine);
    sepTop->setObjectName("sidebarSep");
    listLayout->addWidget(sepTop);

    /* Raccoglie i puntatori ai label di stato per aggiornarli */
    QVector<QLabel*>  statusDots;
    QVector<QString>  execs;

    const int nDeps = static_cast<int>(sizeof(kDeps) / sizeof(kDeps[0]));
    for (int i = 0; i < nDeps; ++i) {
        const Dep& d = kDeps[i];

        auto* row    = new QWidget;
        auto* rowLay = new QHBoxLayout(row);
        rowLay->setContentsMargins(8, 5, 8, 5);
        rowLay->setSpacing(10);
        row->setObjectName((i % 2 == 0) ? "depRowEven" : "depRowOdd");

        /* Pallino stato */
        auto* dot = new QLabel("\xe2\x97\x8f");   /* ● grigio */
        dot->setFixedWidth(20);
        dot->setAlignment(Qt::AlignCenter);
        dot->setStyleSheet("color:#4a5568;font-size:10px;");

        /* Nome + "(opzionale)" */
        QString nameHtml = QString("<b>%1</b>").arg(d.name);
        if (d.optional)
            nameHtml += " <span style='color:#4a5568;font-size:10px;'>"
                        "(opzionale)</span>";
        auto* nameLbl = new QLabel(nameHtml);
        nameLbl->setTextFormat(Qt::RichText);
        nameLbl->setFixedWidth(160);

        /* Descrizione */
        auto* descLbl = new QLabel(d.desc);
        descLbl->setObjectName("hintLabel");

        /* Comando di installazione */
        auto* installLbl = new QLabel(
            QString("<code style='color:#4a5568;font-size:11px;'>%1</code>")
                .arg(d.install));
        installLbl->setTextFormat(Qt::RichText);
        installLbl->setFixedWidth(260);
        installLbl->setWordWrap(true);

        rowLay->addWidget(dot);
        rowLay->addWidget(nameLbl);
        rowLay->addWidget(descLbl, 1);
        rowLay->addWidget(installLbl);

        listLayout->addWidget(row);
        statusDots.append(dot);
        execs.append(QString::fromUtf8(d.exec));
    }
    listLayout->addStretch();
    scroll->setWidget(container);
    outer->addWidget(scroll, 1);

    /* Legenda */
    auto* legend = new QLabel(
        "\xe2\x97\x8f  Non verificato \xc2\xa0\xc2\xa0"
        "<span style='color:#4ade80;'>\xe2\x97\x8f</span>  Trovato \xc2\xa0\xc2\xa0"
        "<span style='color:#f87171;'>\xe2\x97\x8f</span>  Non trovato");
    legend->setTextFormat(Qt::RichText);
    legend->setObjectName("hintLabel");
    outer->addWidget(legend);

    /* ── Connessione verifica ── */
    QObject::connect(verifyBtn, &QPushButton::clicked, verifyBtn,
        [statusDots, execs]() {
        for (int i = 0; i < statusDots.size(); ++i) {
            const QString& ex = execs[i];
            if (ex.isEmpty()) {
                /* Qt / sistema — sempre disponibile */
                statusDots[i]->setStyleSheet(
                    "color:#4ade80;font-size:10px;");   /* verde */
                continue;
            }
            /* Per llama-server/llama-cli: cerca anche nel build locale di Prismalux */
            bool found = !QStandardPaths::findExecutable(ex).isEmpty();
            if (!found && (ex == "llama-server" || ex == "llama-cli")) {
                const QString localBin = ex == "llama-server"
                    ? P::llamaServerBin()
                    : P::llamaCliBin();
                found = QFileInfo::exists(localBin);
            }
            statusDots[i]->setStyleSheet(found
                ? "color:#4ade80;font-size:10px;"   /* verde */
                : "color:#f87171;font-size:10px;");  /* rosso */
        }
    });

    return page;
}

/* ══════════════════════════════════════════════════════════════
   buildRagTab — configurazione RAG (Retrieval-Augmented Generation)
   ══════════════════════════════════════════════════════════════ */
QWidget* ImpostazioniPage::buildRagTab()
{
    auto* page  = new QWidget;
    auto* outer = new QVBoxLayout(page);
    outer->setContentsMargins(20, 20, 20, 20);
    outer->setSpacing(14);

    /* Titolo */
    auto* title = new QLabel("\xf0\x9f\x94\x8d  RAG \xe2\x80\x94 Recupero Documenti Aumentato");
    title->setObjectName("sectionTitle");
    outer->addWidget(title);

    /* Descrizione */
    auto* desc = new QLabel(
        "Base di conoscenza locale: configura la cartella da cui l'AI attinge "
        "per rispondere con fatti precisi dai tuoi documenti.");
    desc->setWordWrap(true);
    desc->setObjectName("hintLabel");
    outer->addWidget(desc);

    /* ── [M2] Warning privacy ── */
    auto* privacyLbl = new QLabel(
        "\xe2\x9a\xa0\xef\xb8\x8f  <b>Attenzione privacy</b>: i chunk indicizzati (frammenti di testo dai tuoi "
        "documenti) vengono salvati in chiaro in <code>~/.prismalux_rag.json</code>. "
        "Non indicizzare documenti riservati (dichiarazioni fiscali, dati medici) "
        "se non sei l'unico utente di questo sistema.");
    privacyLbl->setWordWrap(true);
    privacyLbl->setTextFormat(Qt::RichText);
    privacyLbl->setObjectName("hintLabel");
    privacyLbl->setStyleSheet("color: #e8a020;");   /* amber di avviso */
    outer->addWidget(privacyLbl);

    auto* noSaveChk = new QCheckBox(
        "Non salvare su disco (solo RAM \xe2\x80\x94 indice perso alla chiusura)");
    noSaveChk->setObjectName("cardDesc");
    noSaveChk->setToolTip(
        "Quando attivo, l'indice RAG viene costruito in memoria ma non scritto in\n"
        "~/.prismalux_rag.json. Utile per documenti riservati.\n"
        "L'indice va ricostruito ad ogni avvio dell'applicazione.");
    {
        noSaveChk->setChecked(AppConfig::s().value(P::SK::kRagNoSave, false).toBool());
        m_ragNoSave = noSaveChk->isChecked();
    }
    connect(noSaveChk, &QCheckBox::toggled,
            this, &ImpostazioniPage::onRagNoSaveToggled);
    outer->addWidget(noSaveChk);

    /* ── Form ── */
    auto* fl = new QFormLayout;
    fl->setContentsMargins(0, 8, 0, 0);
    fl->setSpacing(10);
    fl->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    /* Cartella documenti */
    auto* dirRow = new QWidget;
    auto* dirLay = new QHBoxLayout(dirRow);
    dirLay->setContentsMargins(0, 0, 0, 0);
    dirLay->setSpacing(6);
    m_ragDirEdit = new QLineEdit;
    auto* dirEdit = m_ragDirEdit;   /* alias locale */
    dirEdit->setObjectName("inputLine");
    dirEdit->setPlaceholderText("/percorso/documenti/");
    {
        /* Default: cartella RAG nella root del progetto Prismalux */
        const QString defaultRagDir =
            QDir::cleanPath(P::root() + "/../RAG");
        auto& cfg = AppConfig::s();
        QString saved = cfg.value(P::SK::kRagDocsDir, "").toString();
        if (saved.isEmpty()) {
            saved = defaultRagDir;
            cfg.setValue(P::SK::kRagDocsDir, saved);
        }
        dirEdit->setText(saved);
    }
    auto* browseBtn = new QPushButton("Sfoglia...");
    browseBtn->setObjectName("actionBtn");
    browseBtn->setFixedWidth(90);
    dirLay->addWidget(dirEdit, 1);
    dirLay->addWidget(browseBtn);
    fl->addRow("Cartella:", dirRow);

    /* Max risultati */
    auto* maxSpin = new QSpinBox;
    maxSpin->setRange(1, 20);
    {
        maxSpin->setValue(AppConfig::s().value(P::SK::kRagMaxResults, 5).toInt());
    }
    maxSpin->setSuffix("  risultati");
    fl->addRow("Massimi:", maxSpin);

    outer->addLayout(fl);

    /* ── Trasformata Johnson-Lindenstrauss ── */
    {
        auto* jlFrame = new QGroupBox("Ottimizzazione vettori RAG", page);
        jlFrame->setObjectName("cardGroup");
        auto* jlLay = new QVBoxLayout(jlFrame);
        jlLay->setSpacing(6);

        auto* jlChk = new QCheckBox(
            "Trasformata di Johnson\xe2\x80\x93Lindenstrauss (JL)", jlFrame);
        jlChk->setObjectName("cardDesc");
        {
            jlChk->setChecked(AppConfig::s().value(P::SK::kRagJlTransform, true).toBool());
        }
        jlChk->setToolTip(
            "Riduce la dimensionalit\xc3\xa0 dei vettori di embedding mantenendo le distanze.\n"
            "Migliora le performance di ricerca RAG con grandi basi di conoscenza.\n"
            "Consigliata: attiva.");

        auto* jlHint = new QLabel(
            "Comprime i vettori di embedding riducendo i tempi di ricerca senza "
            "perdita significativa di accuratezza. Consigliata con pi\xc3\xb9 di 1000 documenti.", jlFrame);
        jlHint->setObjectName("hintLabel");
        jlHint->setWordWrap(true);

        jlLay->addWidget(jlChk);
        jlLay->addWidget(jlHint);

        connect(jlChk, &QCheckBox::toggled,
                this, &ImpostazioniPage::onRagJlToggled);

        outer->addWidget(jlFrame);
    }

    /* ── Stato indice ── */
    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setObjectName("sidebarSep");
    outer->addWidget(sep);

    m_ragStatusLbl = new QLabel;
    m_ragStatusLbl->setObjectName("hintLabel");
    m_ragStatusLbl->setWordWrap(true);
    m_ragStatusLbl->setTextFormat(Qt::RichText);
    auto* statusLbl = m_ragStatusLbl;  /* alias locale per il codice seguente */
    /* refreshStatus → ora è il metodo membro refreshRagStatus() */

    /* Migrazione one-time: se kRagDocCount è 0 ma l'indice su disco esiste
     * (es. indicizzato con versione precedente che non salvava il conteggio,
     * oppure embeddings parzialmente falliti), carica l'engine da disco e
     * aggiorna QSettings con il conteggio reale. */
    {
        auto& cfg = AppConfig::s();
        if (cfg.value(P::SK::kRagDocCount, 0).toInt() == 0 &&
                m_rag.chunkCount() == 0) {
            const QString ragPath = QDir::homePath() + "/.prismalux_rag.json";
            if (QFileInfo::exists(ragPath)) {
                m_rag.load(ragPath);
                const int realN = m_rag.chunkCount();
                if (realN > 0)
                    cfg.setValue(P::SK::kRagDocCount, realN);
            }
        }
    }
    refreshRagStatus();
    outer->addWidget(m_ragStatusLbl);

    /* ── Pulsante: scarica documenti AdE ufficiali ── */
    auto* downloadBtn = new QPushButton(
        "\xf0\x9f\x93\xa5  Scarica documenti ufficiali consigliati (AdE 2026)");
    downloadBtn->setObjectName("actionBtn");
    downloadBtn->setFixedHeight(32);
    downloadBtn->setToolTip(
        "Scarica automaticamente da Agenzia delle Entrate:\n"
        "  \xe2\x80\xa2 Istruzioni 730/2026\n"
        "  \xe2\x80\xa2 Fascicolo 2 Persone Fisiche 2026\n"
        "Salvati in ~/prismalux_rag_docs/ e pronti per il RAG.");
    outer->addWidget(downloadBtn);

    /* ── Modello embedding ── */
    {
        auto* embedRow = new QHBoxLayout;
        auto* embedLbl = new QLabel("Modello embedding:");
        embedLbl->setObjectName("hintLabel");
        auto* embedEdit = new QLineEdit;
        embedEdit->setPlaceholderText("nomic-embed-text");
        embedEdit->setFixedHeight(28);
        {
            const QString saved = AppConfig::s().value(P::SK::kRagEmbedModel, "").toString();
            embedEdit->setText(saved.isEmpty() ? "nomic-embed-text" : saved);
        }
        embedEdit->setToolTip(
            "Modello Ollama dedicato per gli embedding (vettorizzazione del testo).\n"
            "I modelli chat (qwen3, llama, ecc.) NON supportano /api/embeddings.\n"
            "Modello consigliato: nomic-embed-text\n"
            "Installa con: ollama pull nomic-embed-text");
        QObject::connect(embedEdit, &QLineEdit::textChanged,
                         this, &ImpostazioniPage::onRagEmbedModelChanged);
        embedRow->addWidget(embedLbl);
        embedRow->addWidget(embedEdit, 1);
        outer->addLayout(embedRow);
    }

    /* Riga pulsanti: [Ferma indicizzazione]  [Reindicizza ora] */
    auto* btnRow = new QHBoxLayout;
    btnRow->setSpacing(8);

    m_btnStopIndex = new QPushButton("\xe2\x8f\xb9  Ferma indicizzazione");
    m_btnStopIndex->setObjectName("actionBtn");
    m_btnStopIndex->setProperty("danger", true);
    m_btnStopIndex->setFixedHeight(32);
    m_btnStopIndex->setEnabled(false);   /* abilitato solo durante indexing */
    m_btnStopIndex->setToolTip(
        "Interrompe l'indicizzazione in corso dopo il chunk attuale.\n"
        "I chunk già completati vengono salvati.");
    btnRow->addWidget(m_btnStopIndex);

    m_ragReindexBtn = new QPushButton("\xf0\x9f\x94\x84  Reindicizza ora");
    auto* reindexBtn = m_ragReindexBtn;  /* alias locale */
    reindexBtn->setObjectName("actionBtn");
    reindexBtn->setFixedHeight(32);
    reindexBtn->setToolTip(
        QString("Indicizza i documenti dalla cartella selezionata.\n"
                "L'indicizzazione continua in background anche cambiando finestra.\n"
                "Indice salvato in: %1\n"
                "(disabilitabile con la checkbox \"Non salvare su disco\" sopra)")
            .arg(QDir::homePath() + "/.prismalux_rag.json"));
    btnRow->addWidget(reindexBtn);

    outer->addLayout(btnRow);

    /* Label feedback */
    auto* feedbackLbl = new QLabel;
    feedbackLbl->setObjectName("hintLabel");
    feedbackLbl->setWordWrap(true);
    feedbackLbl->setTextFormat(Qt::RichText);
    feedbackLbl->setVisible(false);
    outer->addWidget(feedbackLbl);

    outer->addStretch();

    /* ── Connessioni ── */
    QObject::connect(browseBtn, &QPushButton::clicked,
                     this, &ImpostazioniPage::onRagBrowseBtnClicked);
    QObject::connect(dirEdit, &QLineEdit::textChanged,
                     this, &ImpostazioniPage::onRagDirChanged);
    QObject::connect(maxSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                     this, &ImpostazioniPage::onRagMaxResultsChanged);
    /* Label feedback globale */
    m_ragFeedbackLbl = feedbackLbl;
    m_ragDownloadBtn = downloadBtn;

    /* ── Download documenti AdE consigliati ── */
    QObject::connect(downloadBtn, &QPushButton::clicked,
                     this, &ImpostazioniPage::onRagDownloadBtnClicked);

    /* Pulsante "Ferma indicizzazione" — setta il flag, il prossimo ciclo si ferma */
    QObject::connect(m_btnStopIndex, &QPushButton::clicked,
                     this, &ImpostazioniPage::onStopIndexClicked);

    QObject::connect(reindexBtn, &QPushButton::clicked,
                     this, &ImpostazioniPage::onReindexBtnClicked);

    return page;
}

/* ── buildRagTab end ── */

QWidget* ImpostazioniPage::buildAiParamsTab()
{
    auto* page  = new QWidget;
    auto* outer = new QVBoxLayout(page);
    outer->setContentsMargins(20, 20, 20, 20);
    outer->setSpacing(16);

    /* Titolo */
    auto* title = new QLabel("\xe2\x9a\x99\xef\xb8\x8f  Parametri AI \xe2\x80\x94 Anti-allucinazione e precisione");
    title->setObjectName("sectionTitle");
    outer->addWidget(title);

    auto* desc = new QLabel(
        "Modalit\xc3\xa0 <b>Brutal Honesty</b>: il modello ammette l'incertezza invece di inventare. "
        "Temperatura vicina a 0 = risposte deterministiche e ripetibili. "
        "Il prefisso di onest\xc3\xa0 istruisce il modello a dire "
        "\xe2\x80\x9cNon lo so\xe2\x80\x9d invece di inventare fatti, numeri o citazioni.");
    desc->setWordWrap(true);
    desc->setObjectName("hintLabel");
    outer->addWidget(desc);

    auto* fl = new QFormLayout;
    fl->setContentsMargins(0, 8, 0, 0);
    fl->setSpacing(12);
    fl->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    /* Legge dall'unica fonte: ~/.prismalux/ai_params.json */
    const AiChatParams cur = AiChatParams::load();

    /* Mostra percorso file sotto il titolo */
    auto* fileLbl = new QLabel(
        "\xf0\x9f\x93\x84  Sorgente: <code>" + AiChatParams::filePath() + "</code> "
        "(modificabile anche con un editor di testo)");
    fileLbl->setWordWrap(true);
    fileLbl->setObjectName("hintLabel");
    outer->addWidget(fileLbl);

    auto* sep0b = new QFrame; sep0b->setFrameShape(QFrame::HLine); sep0b->setObjectName("sidebarSep");
    outer->addWidget(sep0b);

    /* ── Temperatura ── */
    auto* tempSpin = new QDoubleSpinBox;
    tempSpin->setRange(0.0, 1.5);
    tempSpin->setSingleStep(0.05);
    tempSpin->setDecimals(2);
    tempSpin->setValue(cur.temperature);
    tempSpin->setToolTip("0 = deterministico puro (risposta identica ogni volta)\n"
                         "0.05 = quasi deterministico — Brutal Honesty (default)\n"
                         "0.3+ = creativo ma meno affidabile per fatti precisi");
    fl->addRow("Temperatura:", tempSpin);

    auto* tempHint = new QLabel(
        "\xe2\x84\xb9  0.0" "\xe2\x80\x93" "0.1 = fatti certi e ripetibili (Brutal Honesty)  |  0.3+ = creativo/inventivo");
    tempHint->setObjectName("hintLabel");
    fl->addRow("", tempHint);

    /* ── Top-P ── */
    auto* topPSpin = new QDoubleSpinBox;
    topPSpin->setRange(0.1, 1.0);
    topPSpin->setSingleStep(0.05);
    topPSpin->setDecimals(2);
    topPSpin->setValue(cur.top_p);
    topPSpin->setToolTip("Nucleus sampling: include solo i token pi\xc3\xb9 probabili.\n"
                         "0.85 = scelte conservative (Brutal Honesty)");
    fl->addRow("Top-P:", topPSpin);

    /* ── Top-K ── */
    auto* topKSpin = new QSpinBox;
    topKSpin->setRange(1, 200);
    topKSpin->setValue(cur.top_k);
    topKSpin->setToolTip("Limita la scelta ai K token pi\xc3\xb9 probabili.\n"
                         "20 = molto conservativo — favorisce i fatti sicuri.");
    fl->addRow("Top-K:", topKSpin);

    /* ── Penalità ripetizioni ── */
    auto* repSpin = new QDoubleSpinBox;
    repSpin->setRange(1.0, 2.0);
    repSpin->setSingleStep(0.05);
    repSpin->setDecimals(2);
    repSpin->setValue(cur.repeat_penalty);
    repSpin->setToolTip("Penalizza i token gi\xc3\xa0 generati per ridurre le ripetizioni.\n"
                        "1.20 = riduce loop e riempitivi.");
    fl->addRow("Penalità ripetizioni:", repSpin);

    /* ── Max token risposta ── */
    auto* predSpin = new QSpinBox;
    predSpin->setRange(256, 16384);
    predSpin->setSingleStep(256);
    predSpin->setValue(cur.num_predict);
    predSpin->setSuffix("  token");
    predSpin->setToolTip("Numero massimo di token generati per risposta.\n"
                         "2048 = risposte lunghe complete  |  512 = risposte brevi e veloci");
    fl->addRow("Max token risposta:", predSpin);

    /* ── Context window ── */
    auto* ctxSpin = new QSpinBox;
    ctxSpin->setRange(1024, 65536);
    ctxSpin->setSingleStep(1024);
    ctxSpin->setValue(cur.num_ctx);
    ctxSpin->setSuffix("  token");
    ctxSpin->setToolTip(
        "Finestra di contesto: quanti token la chat tiene in memoria.\n"
        "\n"
        "Impatto VRAM (KV-cache, stima per modello 4B):\n"
        "  2048 token  \xe2\x86\x92  ~0.7 GB  \xe2\x86\x92  modello 4B entra quasi tutto in GPU 4 GB\n"
        "  4096 token  \xe2\x86\x92  ~1.3 GB  \xe2\x86\x92  necessario Misto (GPU+CPU)\n"
        "  8192 token  \xe2\x86\x92  ~2.7 GB  \xe2\x86\x92  Misto obbligatorio, ~46% su CPU\n"
        " 16384 token  \xe2\x86\x92  ~5.4 GB  \xe2\x86\x92  quasi tutto su CPU (GPU insufficiente)\n"
        "\n"
        "Regola: abbassa il contesto per massimizzare i layer su NVIDIA VRAM.");
    fl->addRow("Finestra contesto:", ctxSpin);

    /* Hint dinamico: stima KV-cache VRAM al cambio del valore */
    auto* ctxHint = new QLabel("", outer->parentWidget());
    ctxHint->setObjectName("hintLabel");
    ctxHint->setWordWrap(true);
    fl->addRow("", ctxHint);

    auto updateCtxHint = [ctxHint](int ctx) {
        /* Stima KV-cache: ~0.33 MB/token per modello 4B (GQA 32 layer, fp16) */
        const double kvGb = ctx * 0.00033;
        QString level, advice;
        if (kvGb < 1.0) {
            level  = "\xe2\x9c\x85";
            advice = "modello 4B quasi interamente su GPU 4 GB";
        } else if (kvGb < 2.0) {
            level  = "\xf0\x9f\x9f\xa1";
            advice = "Misto GPU+CPU consigliato";
        } else if (kvGb < 4.0) {
            level  = "\xf0\x9f\x9f\xa0";
            advice = "Misto obbligatorio, parte significativa su CPU/RAM";
        } else {
            level  = "\xf0\x9f\x94\xb4";
            advice = "GPU 4 GB insufficiente — quasi tutto su CPU";
        }
        ctxHint->setText(QString("%1  KV-cache stimata: ~%2 GB  \xe2\x80\x94  %3")
                         .arg(level).arg(kvGb, 0, 'f', 1).arg(advice));
    };
    updateCtxHint(ctxSpin->value());
    QObject::connect(ctxSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                     ctxHint, updateCtxHint);

    outer->addLayout(fl);

    /* ── Prefisso di onestà assoluta ── */
    auto* honestyCb = new QCheckBox(
        "\xf0\x9f\x94\x92  Prefisso Brutal Honesty (consigliato)");
    honestyCb->setChecked(cur.honesty_prefix);
    honestyCb->setToolTip(
        "Aggiunge all'inizio di ogni system prompt la regola:\n"
        "\"Se non conosci qualcosa, dillo. Non inventare mai fatti, numeri o citazioni.\"\n"
        "Questo \xc3\xa8 il metodo pi\xc3\xb9 efficace per ridurre le allucinazioni.");
    outer->addWidget(honestyCb);

    auto* honestyHint = new QLabel(
        "\xe2\x84\xb9  Con questa opzione attiva il modello risponde "
        "\xe2\x80\x9cNon lo so\xe2\x80\x9d invece di inventare. Disattiva solo se vuoi risposte pi\xc3\xb9 fluide.");
    honestyHint->setWordWrap(true);
    honestyHint->setObjectName("hintLabel");
    outer->addWidget(honestyHint);

    /* ── Modalità Caveman (risposte dirette, zero riempitivi) ── */
    auto* sepCaveman = new QFrame; sepCaveman->setFrameShape(QFrame::HLine);
    sepCaveman->setObjectName("sidebarSep");
    outer->addWidget(sepCaveman);

    /* Riga: toggle + etichetta */
    auto* cavemanRow = new QWidget;
    auto* cavemanLay = new QHBoxLayout(cavemanRow);
    cavemanLay->setContentsMargins(0, 4, 0, 4);
    cavemanLay->setSpacing(12);

    const bool cavemanOn = AiChatParams::load().caveman_mode;
    auto* cavemanToggle = new ToggleSwitch({}, this);
    cavemanToggle->setChecked(cavemanOn);
    cavemanToggle->setFixedHeight(26);

    auto* cavemanLbl = new QLabel(
        "\xf0\x9f\xa6\x96  <b>Modalit\xc3\xa0 Caveman</b> \xe2\x80\x94 risposte dirette, senza convenevoli");
    cavemanLbl->setWordWrap(false);

    /* Badge stato ON/OFF visivo accanto al toggle */
    auto* cavemanBadge = new QLabel(cavemanOn ? "  ON " : "  OFF");
    cavemanBadge->setObjectName(cavemanOn ? "badgeActive" : "badgeInactive");
    cavemanBadge->setFixedWidth(44);
    cavemanBadge->setAlignment(Qt::AlignCenter);

    cavemanLay->addWidget(cavemanToggle);
    cavemanLay->addWidget(cavemanBadge);
    cavemanLay->addWidget(cavemanLbl);
    cavemanLay->addStretch();
    outer->addWidget(cavemanRow);

    auto* cavemanDesc = new QLabel(
        "\xe2\x84\xb9  Elimina frasi come \xe2\x80\x9c" "Certamente!\xe2\x80\x9d o \xe2\x80\x9c" "Spero di averti aiutato\xe2\x80\x9d. "
        "Il modello va dritto al contenuto. Utile per pipeline agenti e query rapide "
        "(meno token sprecati = risposte pi\xc3\xb9 veloci).");
    cavemanDesc->setWordWrap(true);
    cavemanDesc->setObjectName("hintLabel");
    outer->addWidget(cavemanDesc);

    /* ── Personalità AI ── */
    auto* sepPersona = new QFrame; sepPersona->setFrameShape(QFrame::HLine);
    sepPersona->setObjectName("sidebarSep");
    outer->addWidget(sepPersona);

    auto* personaTitleRow = new QWidget;
    auto* personaTitleLay = new QHBoxLayout(personaTitleRow);
    personaTitleLay->setContentsMargins(0, 4, 0, 2);
    personaTitleLay->setSpacing(8);
    auto* personaLbl = new QLabel(
        "\xf0\x9f\x8e\xad  <b>Personalit\xc3\xa0 AI</b> \xe2\x80\x94 stile di risposta del modello");
    personaLbl->setWordWrap(false);
    personaTitleLay->addWidget(personaLbl);
    personaTitleLay->addStretch();
    outer->addWidget(personaTitleRow);

    struct PersonaDef { const char* key; const char* label; };
    static const PersonaDef kPersonas[] = {
        {"nessuna", "\xf0\x9f\x9a\xab  Nessuna"},
        {"jarvis",  "\xf0\x9f\xa4\x96  Jarvis  (Tony Stark)"},
        {"kitt",    "\xf0\x9f\x9a\x97  KITT  (Knight Rider)"},
        {"yoda",    "\xf0\x9f\x8c\xbf  Yoda  (Star Wars)"},
        {"snake",   "\xf0\x9f\x8e\xae  Snake  (Metal Gear)"},
        {"sonic",   "\xf0\x9f\x92\xa8  Sonic"},
        {"mario",   "\xf0\x9f\x8d\x84  Super Mario"},
    };

    {
        const QString curPersona = AppConfig::s().value(P::SK::kAiPersonality, "nessuna").toString();

        /* Prima riga: Nessuna + Jarvis + KITT */
        auto* row1 = new QWidget; auto* lay1 = new QHBoxLayout(row1);
        lay1->setContentsMargins(0, 0, 0, 0); lay1->setSpacing(6);
        /* Seconda riga: Yoda + Snake + Sonic + Mario */
        auto* row2 = new QWidget; auto* lay2 = new QHBoxLayout(row2);
        lay2->setContentsMargins(0, 0, 0, 4); lay2->setSpacing(6);

        QButtonGroup* grp = new QButtonGroup(page);
        int idx = 0;
        for (const auto& pd : kPersonas) {
            auto* btn = new QPushButton(pd.label, page);
            btn->setCheckable(true);
            btn->setChecked(QString(pd.key) == curPersona);
            btn->setObjectName("actionBtn");
            btn->setProperty("persona_key", QString(pd.key));
            grp->addButton(btn);
            (idx < 3 ? lay1 : lay2)->addWidget(btn);
            idx++;
        }
        lay1->addStretch(); lay2->addStretch();
        outer->addWidget(row1);
        outer->addWidget(row2);

        connect(grp, &QButtonGroup::buttonClicked,
                this, &ImpostazioniPage::onPersonaBtnClicked);
    }

    auto* personaDesc = new QLabel(
        "\xe2\x84\xb9  La personalit\xc3\xa0 modifica lo stile di risposta del modello e il testo del "
        "pulsante Conversa. Si applica a tutte le chat.");
    personaDesc->setWordWrap(true);
    personaDesc->setObjectName("hintLabel");
    outer->addWidget(personaDesc);

    /* ── Flash Attention ── */
    auto* flashRow = new QWidget;
    auto* flashLay = new QHBoxLayout(flashRow);
    flashLay->setContentsMargins(0, 4, 0, 4);
    flashLay->setSpacing(12);

    auto* flashCb = new QCheckBox(
        "\xe2\x9a\xa1  Flash Attention (riduce RAM/VRAM KV cache ~30-50%)", page);
    flashCb->setObjectName("cardDesc");
    flashCb->setChecked(AiChatParams::load().flash_attn);
    flashLay->addWidget(flashCb);
    flashLay->addStretch();
    outer->addWidget(flashRow);

    auto* flashDesc = new QLabel(
        "\xe2\x84\xb9  Consigliato su macchine con \xe2\x89\xa4 8 GB RAM. "
        "Riduce la memoria usata dalla KV cache durante la generazione. "
        "Ollama e llama-server lo ignorano se il modello non lo supporta.");
    flashDesc->setWordWrap(true);
    flashDesc->setObjectName("hintLabel");
    outer->addWidget(flashDesc);

    /* ── mlock: blocca modello in RAM (solo llama-server) ── */
    auto* mlockRow = new QWidget;
    auto* mlockLay = new QHBoxLayout(mlockRow);
    mlockLay->setContentsMargins(0, 4, 0, 0);
    mlockLay->setSpacing(12);

    auto* mlockCb = new QCheckBox(
        "\xf0\x9f\x94\x92  Blocca modello in RAM (--mlock, llama-server)", page);  /* 🔒 */
    mlockCb->setObjectName("cardDesc");
    {
        mlockCb->setChecked(AppConfig::s().value(P::SK::kMlockModel, false).toBool());
    }
    mlockLay->addWidget(mlockCb);
    mlockLay->addStretch();
    outer->addWidget(mlockRow);

    auto* mlockDesc = new QLabel(
        "\xe2\x84\xb9  Impedisce all'OS di fare swap delle pagine del modello su disco. "
        "Riduce i picchi di latenza su sistemi con RAM sufficiente (>= 16 GB). "
        "Solo llama-server — Ollama lo ignora. Richiede riavvio del server.");
    mlockDesc->setWordWrap(true);
    mlockDesc->setObjectName("hintLabel");
    outer->addWidget(mlockDesc);

    connect(mlockCb, &QCheckBox::toggled,
            this,    &ImpostazioniPage::onMlockToggled);

    /* ── Preset "8 GB RAM" ── */
    auto* presetRow = new QWidget;
    auto* presetLay = new QHBoxLayout(presetRow);
    presetLay->setContentsMargins(0, 4, 0, 4);
    presetLay->setSpacing(8);

    auto* presetLbl = new QLabel("\xf0\x9f\x8e\x9b  Preset:", page);  /* 🎛 */
    presetLbl->setObjectName("cardDesc");
    presetLay->addWidget(presetLbl);

    auto* preset8gb = new QPushButton("8 GB RAM", page);
    preset8gb->setObjectName("actionBtn");
    preset8gb->setToolTip(
        "Applica impostazioni ottimali per macchine con 8 GB RAM:\n"
        "num_ctx=4096  num_predict=1024  temperature=0.05  Flash Attention ON");
    presetLay->addWidget(preset8gb);

    auto* presetLong = new QPushButton("\xf0\x9f\x93\x9c  Contesto lungo", page);  /* 📜 */
    presetLong->setObjectName("actionBtn");
    presetLong->setToolTip(
        "Aumenta la finestra di contesto per documenti e conversazioni lunghe.\n"
        "num_ctx=16384  Flash Attention ON\n\n"
        "Richiede \xe2\x89\xa5 16 GB RAM o VRAM adeguata.\n"
        "Su Ollama: invia num_ctx=16384 in ogni richiesta.\n"
        "Su llama-server: riavviare il server dopo aver applicato.");
    presetLay->addWidget(presetLong);
    presetLay->addStretch();
    outer->addWidget(presetRow);

    auto* sep1 = new QFrame; sep1->setFrameShape(QFrame::HLine); sep1->setObjectName("sidebarSep");
    outer->addWidget(sep1);

    /* ── Bottoni reset / salva ── */
    auto* btnRow  = new QWidget;
    auto* btnLay  = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(0, 0, 0, 0);
    btnLay->setSpacing(8);

    auto* resetBtn = new QPushButton("\xf0\x9f\x94\x84  Ripristina default");
    resetBtn->setObjectName("actionBtn");
    resetBtn->setToolTip("Ripristina i valori ottimali anti-allucinazione");

    auto* saveBtn  = new QPushButton("\xe2\x9c\x85  Salva");
    saveBtn->setObjectName("actionBtn");

    auto* saveStatus = new QLabel("");
    saveStatus->setObjectName("hintLabel");

    btnLay->addWidget(resetBtn);
    btnLay->addStretch();
    btnLay->addWidget(saveStatus);
    btnLay->addWidget(saveBtn);
    outer->addWidget(btnRow);

    /* Salva puntatori come member variables per i slot */
    m_tempSpin     = tempSpin;
    m_topPSpin     = topPSpin;
    m_topKSpin     = topKSpin;
    m_repSpin      = repSpin;
    m_predSpin     = predSpin;
    m_ctxSpin      = ctxSpin;
    m_ctxHint      = ctxHint;
    m_honestyCb    = honestyCb;
    m_cavemanToggle = cavemanToggle;
    m_cavemanBadge = cavemanBadge;
    m_flashCb      = flashCb;
    m_saveStatus   = saveStatus;

    connect(saveBtn,  &QPushButton::clicked, this, &ImpostazioniPage::onAiParamsSave);
    connect(resetBtn, &QPushButton::clicked, this, &ImpostazioniPage::onAiParamsReset);
    connect(flashCb,  &QCheckBox::toggled,   this, &ImpostazioniPage::onAiParamsSave);
    connect(preset8gb,  &QPushButton::clicked, this, &ImpostazioniPage::onAiPreset8GbClicked);
    connect(presetLong, &QPushButton::clicked, this, &ImpostazioniPage::onAiPresetLongClicked);
    connect(cavemanToggle, &QAbstractButton::toggled, this, &ImpostazioniPage::onCavemanToggled);

    /* Auto-salva su ogni variazione di parametro */
    connect(tempSpin,  QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ImpostazioniPage::onAiParamsSave);
    connect(topPSpin,  QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ImpostazioniPage::onAiParamsSave);
    connect(topKSpin,  QOverload<int>::of(&QSpinBox::valueChanged),          this, &ImpostazioniPage::onAiParamsSave);
    connect(repSpin,   QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ImpostazioniPage::onAiParamsSave);
    connect(predSpin,  QOverload<int>::of(&QSpinBox::valueChanged),          this, &ImpostazioniPage::onAiParamsSave);
    connect(ctxSpin,   QOverload<int>::of(&QSpinBox::valueChanged),          this, &ImpostazioniPage::onAiParamsSave);
    connect(honestyCb, &QCheckBox::toggled,                                  this, &ImpostazioniPage::onAiParamsSave);

    outer->addStretch();
    return page;
}

/* ══════════════════════════════════════════════════════════════════════
   buildLlmClassificaTab — ranking oggettivo modelli open-weight
   ═══════════════════════════════════════════════════════════════════════
   Fonte dati: ArtificialAnalysis.ai (intelligence index) + benchmark
   Prismalux locali (test_prompt_levels.py, sessione 2026-04-15).
   Colonne: Rank | Modello | Parametri | RAM (Q4) | Score | Velocità | 64GB | Categoria
   ══════════════════════════════════════════════════════════════════════ */

QWidget* ImpostazioniPage::buildSandboxTab()
{
    auto* page = new QWidget;
    auto* lay  = new QVBoxLayout(page);
    lay->setContentsMargins(16, 16, 16, 12);
    lay->setSpacing(16);

    /* ── Titolo ── */
    auto* titleLbl = new QLabel(
        "\xf0\x9f\x90\xb3  Sandbox Docker \xe2\x80\x94 Esecuzione Codice Isolata", page);
    titleLbl->setObjectName("sectionTitle");
    lay->addWidget(titleLbl);

    /* ── Stato Docker ── */
    auto* statusCard = new QFrame(page);
    statusCard->setObjectName("cardFrame");
    auto* statusLay = new QHBoxLayout(statusCard);
    statusLay->setContentsMargins(12, 10, 12, 10);

    auto* statusIcon = new QLabel(page);
    auto* statusDesc = new QLabel(page);
    statusDesc->setObjectName("cardDesc");
    statusDesc->setWordWrap(true);

    const QString docker = P::findDocker();
    if (!docker.isEmpty()) {
        statusIcon->setText("\xf0\x9f\x9f\xa2  Docker disponibile");
        statusIcon->setStyleSheet("color:#4ade80;font-weight:bold;");
        statusDesc->setText(
            QString("Daemon raggiungibile: <code>%1</code><br>"
                    "Il codice AI verr\xc3\xa0 eseguito in un container effimero "
                    "(rete disabilitata, nessun volume, max RAM configurabile).")
            .arg(docker));
    } else {
        statusIcon->setText("\xf0\x9f\x94\xb4  Docker non disponibile");
        statusIcon->setStyleSheet("color:#f87171;font-weight:bold;");
        statusDesc->setText(
            "Docker non trovato o il daemon non \xc3\xa8 avviato.<br>"
            "Il codice sar\xc3\xa0 eseguito con Python locale (permessi utente).<br>"
            "<b>Per installare Docker:</b> "
            "<code>sudo apt install docker.io &amp;&amp; sudo systemctl start docker</code>");
    }
    statusLay->addWidget(statusIcon);
    statusLay->addWidget(statusDesc, 1);
    lay->addWidget(statusCard);

    /* ── Toggle sandbox abilitato ── */
    {
        auto* row = new QHBoxLayout;
        auto* chk = new QCheckBox(
            "Usa sandbox Docker per il codice generato dall\xe2\x80\x99" "AI", page);
        chk->setObjectName("settingsCheck");
        chk->setChecked(AppConfig::s().value(P::SK::kSandboxEnabled, true).toBool());
        chk->setEnabled(!docker.isEmpty());
        chk->setToolTip("Se disabilitato, il codice AI viene eseguito localmente con pip install retry.\n"
                        "Con sandbox: rete disabilitata, nessun accesso al filesystem host.");
        connect(chk,  &QCheckBox::toggled,
                this, &ImpostazioniPage::onSandboxEnabledToggled);
        row->addWidget(chk);
        row->addStretch();
        lay->addLayout(row);
    }

    /* ── Immagine Docker ── */
    {
        auto* grp = new QGroupBox("\xe2\x9a\x99\xef\xb8\x8f  Configurazione container", page);
        grp->setObjectName("cardGroup");
        auto* grpLay = new QFormLayout(grp);
        grpLay->setSpacing(10);

        auto* imgEdit = new QLineEdit(page);
        imgEdit->setObjectName("settingsEdit");
        imgEdit->setPlaceholderText("python:3.11-slim");
        {
            imgEdit->setText(AppConfig::s().value(P::SK::kSandboxImage, "python:3.11-slim").toString());
        }
        imgEdit->setToolTip(
            "Immagine Docker da usare.\n"
            "python:3.11-slim — leggera, include pip.\n"
            "python:3.12-slim — versione pi\xc3\xb9 recente.\n"
            "Prima esecuzione: Docker scarica l\xe2\x80\x99immagine automaticamente (~50 MB).");
        connect(imgEdit, &QLineEdit::editingFinished,
                this,    &ImpostazioniPage::onSandboxImageEditFinished);
        grpLay->addRow("Immagine:", imgEdit);

        auto* memSpin = new QSpinBox(page);
        memSpin->setRange(64, 2048);
        memSpin->setSuffix(" MB");
        memSpin->setObjectName("settingsSpin");
        {
            memSpin->setValue(AppConfig::s().value(P::SK::kSandboxMemory, 256).toInt());
        }
        memSpin->setToolTip("Limite RAM del container Docker.\n"
                            "256 MB \xc3\xa8 sufficiente per la maggior parte dei task.\n"
                            "Aumenta a 512+ per elaborazioni numeriche intensive.");
        connect(memSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                this,    &ImpostazioniPage::onSandboxMemSpinChanged);
        grpLay->addRow("Limite RAM:", memSpin);

        /* Salva puntatori come member variables per i slot */
        m_sandboxImgEdit  = imgEdit;
        m_sandboxMemSpin  = memSpin;

        lay->addWidget(grp);
    }

    /* ── Pull immagine ── */
    if (!docker.isEmpty()) {
        auto* pullRow = new QHBoxLayout;
        auto* pullBtn = new QPushButton(
            "\xf0\x9f\x93\xa5  Scarica immagine ora (docker pull)", page);
        pullBtn->setObjectName("actionBtn");
        pullBtn->setToolTip(
            "Esegue 'docker pull <immagine>' per scaricare subito l\xe2\x80\x99immagine.\n"
            "Opzionale: Docker la scarica anche automaticamente alla prima esecuzione.");
        auto* pullStatus = new QLabel("", page);
        pullStatus->setObjectName("cardDesc");

        connect(pullBtn, &QPushButton::clicked, page,
                [pullBtn, pullStatus, imgEdit = static_cast<QLineEdit*>(nullptr)]() mutable {
            /* Rilega l'immagine dal QSettings (aggiornata da imgEdit) */
            const QString img = AppConfig::s()
                .value(P::SK::kSandboxImage, "python:3.11-slim").toString();
            pullBtn->setEnabled(false);
            pullStatus->setText("\xf0\x9f\x94\x84  Pull in corso...");
            auto* proc = new QProcess(pullBtn->window());
            proc->setProcessChannelMode(QProcess::MergedChannels);
            connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                    pullBtn, [proc, pullBtn, pullStatus, img](int rc, QProcess::ExitStatus){
                const QString out = QString::fromUtf8(proc->readAll()).trimmed();
                proc->deleteLater();
                pullBtn->setEnabled(true);
                if (rc == 0)
                    pullStatus->setText(
                        QString("\xe2\x9c\x85  %1 scaricata con successo.").arg(img));
                else
                    pullStatus->setText(
                        QString("\xe2\x9d\x8c  Errore: %1").arg(out.left(120)));
            });
            proc->start(P::findDocker(), {"pull", img});
        });

        m_sandboxPullBtn    = pullBtn;
        m_sandboxPullStatus = pullStatus;

        pullRow->addWidget(pullBtn);
        pullRow->addWidget(pullStatus, 1);
        lay->addLayout(pullRow);
    }

    /* ── Riepilogo sicurezza ── */
    {
        auto* info = new QLabel(page);
        info->setObjectName("hintLabel");
        info->setWordWrap(true);
        info->setText(
            "\xf0\x9f\x94\x92  <b>Isolamento garantito dal container:</b><br>"
            "\xe2\x80\xa2 <b>Rete disabilitata</b> — nessuna connessione a internet<br>"
            "\xe2\x80\xa2 <b>Nessun volume</b> — il codice non pu\xc3\xb2 leggere/scrivere file locali<br>"
            "\xe2\x80\xa2 <b>Limite processi</b> — max 64 PID (anti fork-bomb)<br>"
            "\xe2\x80\xa2 <b>Container effimero</b> — distrutto automaticamente al termine (--rm)<br>"
            "\xe2\x80\xa2 <b>pip install non disponibile</b> con sandbox attivo (rete assente)");
        info->setTextFormat(Qt::RichText);
        lay->addWidget(info);
    }

    lay->addStretch();
    return page;
}

