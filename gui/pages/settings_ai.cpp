#include "settings_main.h"
#include "../lan_server.h"
#include "../log_bus.h"
#include "../dpi_utils.h"
#include "../widgets/toggle_switch.h"
#include "../widgets/stt_whisper.h"
#include "../widgets/collapsible_section.h"
#include "main_customize.h"
#include "main_maintenance.h"
#include "main_graph.h"
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
#include <QFont>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QGuiApplication>
#include <QClipboard>
#include <QMouseEvent>
#include <QToolTip>
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
    auto* titleLbl = new QLabel(tr("\xf0\x9f\xa6\x99  AI Locale \xe2\x80\x94 Gestori & Modelli"), page);
    titleLbl->setObjectName("sectionTitle");
    mainLay->addWidget(titleLbl);

    /* ── 2 colonne ── */
    auto* colsRow = new QWidget(page);
    auto* colsLay = new QHBoxLayout(colsRow);
    colsLay->setContentsMargins(0, 0, 0, 0);
    colsLay->setSpacing(16);

    /* ── Colonna sinistra: selettore gestore ── */
    auto* leftGroup = new QGroupBox(tr("\xf0\x9f\x94\xa7  Gestore LLM"), colsRow);
    leftGroup->setObjectName("cardGroup");
    leftGroup->setMinimumWidth(220);
    leftGroup->setMaximumWidth(280);
    auto* leftLay = new QVBoxLayout(leftGroup);
    leftLay->setSpacing(10);

    auto* btnOllama = new QRadioButton(tr("\xf0\x9f\x90\xb3  Ollama"), leftGroup);
    btnOllama->setChecked(true);
    btnOllama->setAccessibleName(tr("Backend Ollama"));
    auto* btnLlama  = new QRadioButton(tr("\xf0\x9f\xa6\x99  llama.cpp"), leftGroup);
    btnLlama->setAccessibleName(tr("Backend llama.cpp"));

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

    auto* refreshBtn = new QPushButton(tr("\xf0\x9f\x94\x84  Aggiorna lista"), leftGroup);
    refreshBtn->setObjectName("actionBtn");
    refreshBtn->setAccessibleName(tr("Aggiorna lista modelli disponibili"));
    leftLay->addWidget(refreshBtn);

    /* llama.cpp Studio rimosso: funzionalità integrate in scheda LLM */

    /* ── Sezione Ollama LAN ── */
    auto* sepLan = new QFrame(leftGroup);
    sepLan->setFrameShape(QFrame::HLine);
    sepLan->setObjectName("sidebarSep");
    leftLay->addWidget(sepLan);

    auto* lanTitleLbl = new QLabel(tr("\xf0\x9f\x93\xb1  Ollama LAN"), leftGroup);
    lanTitleLbl->setObjectName("cardDesc");
    leftLay->addWidget(lanTitleLbl);

    auto* lanStatusLbl = new QLabel(tr("\xe2\x9a\xaa Inattivo"), leftGroup);
    lanStatusLbl->setObjectName("hintLabel");
    lanStatusLbl->setWordWrap(true);
    leftLay->addWidget(lanStatusLbl);

    auto* lanBtn = new QPushButton(tr("\xf0\x9f\x9f\xa2  Avvia Ollama LAN"), leftGroup);
    lanBtn->setObjectName("actionBtn");
    lanBtn->setToolTip(tr("Avvia ollama serve con OLLAMA_HOST=0.0.0.0:11434\n"
                       "cos\xc3\xac il telefono in LAN pu\xc3\xb2 connettersi"));
    leftLay->addWidget(lanBtn);

    leftLay->addStretch(1);
    colsLay->addWidget(CollapsibleSection::fromGroupBox(leftGroup));

    /* ── Colonna destra: lista modelli ── */
    auto* rightGroup = new QGroupBox(tr("\xf0\x9f\x93\xa6  Modelli disponibili"), colsRow);
    rightGroup->setObjectName("cardGroup");
    auto* rightLay = new QVBoxLayout(rightGroup);
    rightLay->setSpacing(8);

    /* Hint sopra la lista */
    auto* hintLbl = new QLabel(tr("Seleziona un gestore per vedere i modelli installati."), rightGroup);
    hintLbl->setObjectName("hintLabel");
    hintLbl->setWordWrap(true);
    rightLay->addWidget(hintLbl);

    auto* modelList = new QListWidget(rightGroup);
    modelList->setObjectName("modelsList");
    modelList->setAlternatingRowColors(true);
    modelList->setToolTip(tr("Doppio clic: attiva il modello\n"
                             "Clic centrale: copia il nome negli appunti"));
    /* Middle-click → copia nome modello (gestito in eventFilter) */
    modelList->viewport()->installEventFilter(this);
    rightLay->addWidget(modelList, 1);

    colsLay->addWidget(CollapsibleSection::fromGroupBox(rightGroup), 1);

    /* ── Colonna destra extra: Think Mode + Knowledge ── */
    auto* rcW = new QWidget(colsRow);
    auto* rcL = new QVBoxLayout(rcW);
    rcL->setContentsMargins(0, 0, 0, 0);
    rcL->setSpacing(10);
    rcW->setMinimumWidth(280);
    rcW->setMaximumWidth(360);
    colsLay->addWidget(rcW);

    mainLay->addWidget(colsRow);

    /* ══════════════════════════════════════════════════
       Sezione Ragionamento AI  (colonna destra)
       ══════════════════════════════════════════════════ */
    {
        auto* thinkGroup = new QGroupBox(
            tr("\xf0\x9f\xa7\xa0  Ragionamento AI (Think Mode)"), page);  /* 🧠 */
        thinkGroup->setObjectName("cardGroup");
        auto* thinkLay = new QVBoxLayout(thinkGroup);
        thinkLay->setSpacing(8);

        /* ── Riga superiore: modalità + budget ── */
        auto* topRow = new QHBoxLayout;
        topRow->setSpacing(16);

        /* Modalità: Off / Auto / On */
        auto* modeLbl = new QLabel(tr("Modalit\xc3\xa0:"), thinkGroup);
        modeLbl->setObjectName("cardDesc");
        topRow->addWidget(modeLbl);

        auto* btnOff  = new QPushButton(tr("Off"),  thinkGroup);
        auto* btnAuto = new QPushButton(tr("Auto"), thinkGroup);
        auto* btnOn   = new QPushButton("On",   thinkGroup);
        for (auto* b : {btnOff, btnAuto, btnOn}) {
            b->setCheckable(true);
            b->setFixedWidth(dpiScale(56));
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
        auto* budgetLbl = new QLabel(tr("Budget token:"), thinkGroup);
        budgetLbl->setObjectName("cardDesc");
        topRow->addWidget(budgetLbl);

        auto* budgetSlider = new QSlider(Qt::Horizontal, thinkGroup);
        budgetSlider->setRange(1, 4);
        budgetSlider->setFixedWidth(dpiScale(120));
        budgetSlider->setTickInterval(1);
        budgetSlider->setTickPosition(QSlider::TicksBelow);
        topRow->addWidget(budgetSlider);

        auto* budgetValLbl = new QLabel("2\xc3\x97", thinkGroup);  /* 2× */
        budgetValLbl->setObjectName("cardDesc");
        budgetValLbl->setFixedWidth(dpiScale(28));
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
            tr("<small>"
            "<b>Off</b>: nessun ragionamento interno (pi\xc3\xb9 veloce). "  /* più */
            "<b>Auto</b>: il classificatore decide per ogni domanda (default). "
            "<b>On</b>: ragionamento sempre attivo sui modelli compatibili "
            "(qwen3, deepseek-r1, qwq).<br>"
            "<b>Budget</b>: moltiplicatore dei token di risposta quando il thinking \xc3\xa8 attivo "  /* è */
            "(1\xc3\x97 = base, 2\xc3\x97 = default, 4\xc3\x97 = risposta molto elaborata)."
            "</small>"),
            thinkGroup);
        hintLbl->setObjectName("hintLabel");
        hintLbl->setWordWrap(true);
        hintLbl->setTextFormat(Qt::RichText);
        thinkLay->addWidget(hintLbl);

        rcL->addWidget(thinkGroup);

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
            tr("\xf0\x9f\x93\x96  Memoria persistente (Knowledge)"), page);  /* 📖 */
        knGroup->setObjectName("cardGroup");
        auto* knLay = new QVBoxLayout(knGroup);
        knLay->setSpacing(8);

        /* Riga: checkbox + pulsante Apri file */
        auto* chkRow = new QHBoxLayout;
        auto* chkKn  = new QCheckBox(
            tr("Inietta contesto utente nel prompt"), knGroup);
        chkKn->setObjectName("cardDesc");
        {
            chkKn->setChecked(AppConfig::s().value(P::SK::kInjectUserKnowledge, true).toBool());
        }
        chkRow->addWidget(chkKn);

        auto* openFileBtn = new QPushButton(tr("Apri file"), knGroup);
        openFileBtn->setObjectName("actionBtn");
        openFileBtn->setFixedWidth(dpiScale(90));
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

        rcL->addWidget(knGroup);

        /* ── Costituzione Prismalux ──────────────────────────────────── */
        auto* constGroup = new QGroupBox(
            tr("\xf0\x9f\x93\x9c  Costituzione Prismalux"), rcW);
        constGroup->setObjectName("cardGroup");
        auto* constLay = new QVBoxLayout(constGroup);
        constLay->setSpacing(6);

        auto* constRow = new QHBoxLayout;
        auto* chkConst = new QCheckBox(
            tr("Inietta principi etici in ogni prompt"), constGroup);
        chkConst->setObjectName("cardDesc");
        chkConst->setChecked(AppConfig::s().value(P::SK::kConstitutionEnabled, true).toBool());
        constRow->addWidget(chkConst);

        auto* constFileBtn = new QPushButton(tr("Modifica"), constGroup);
        constFileBtn->setObjectName("actionBtn");
        constFileBtn->setFixedWidth(dpiScale(90));
        constFileBtn->setToolTip(P::constitutionPath());
        constRow->addWidget(constFileBtn);
        constRow->addStretch(1);
        constLay->addLayout(constRow);

        auto* constHint = new QLabel(
            "<small>Se attivo, <b>COSTITUZIONE_PRISMALUX.txt</b> viene inserita come primo "
            "blocco di ogni system prompt — guida il comportamento etico del modello "
            "indipendentemente dal task. Simile alla Constitutional AI di Anthropic.</small>",
            constGroup);
        constHint->setObjectName("hintLabel");
        constHint->setWordWrap(true);
        constHint->setTextFormat(Qt::RichText);
        constLay->addWidget(constHint);

        rcL->addWidget(constGroup);
        rcL->addStretch();

        /* Salva la preferenza e invalida la cache di lettura */
        connect(chkKn, &QCheckBox::toggled,
                this, &ImpostazioniPage::onKnowledgeInjectToggled);

        /* Apre user_knowledge.md nel text editor di sistema */
        connect(openFileBtn, &QPushButton::clicked,
                this, &ImpostazioniPage::onOpenKnowledgeFileClicked);

        /* Costituzione: toggle + apri editor */
        connect(chkConst, &QCheckBox::toggled, this, [chkConst](bool on) {
            AppConfig::s().setValue(P::SK::kConstitutionEnabled, on);
        });
        connect(constFileBtn, &QPushButton::clicked, this, []() {
            QDesktopServices::openUrl(QUrl::fromLocalFile(P::constitutionPath()));
            P::invalidateConstitutionCache();
        });
    }

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
    auto* useBtn = new QPushButton(tr("\xe2\x9c\x94  Usa modello"), leftGroup);
    useBtn->setObjectName("actionBtn");
    useBtn->setEnabled(false);
    useBtn->setToolTip(tr("Imposta il modello selezionato come modello attivo"));
    useBtn->setAccessibleName(tr("Attiva modello selezionato"));
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

    /* ══════════════════════════════════════════════════
       Smart Router — LOCAL/CLOUD automatico
       ══════════════════════════════════════════════════ */
    {
        auto* srGroup = new QGroupBox(
            tr("\xe2\x98\x81\xef\xb8\x8f  Smart Router \xe2\x80\x94 LOCAL / CLOUD automatico"), page);
        srGroup->setObjectName("cardGroup");
        auto* srLay = new QVBoxLayout(srGroup);
        srLay->setSpacing(8);

        /* Descrizione */
        auto* srDesc = new QLabel(
            "Se abilitato, le query lunghe o complesse vengono instradata al backend cloud "
            "configurato (OpenAI-compatible). Dati sensibili rimangono sempre in locale.", srGroup);
        srDesc->setObjectName("hintLabel");
        srDesc->setWordWrap(true);
        srLay->addWidget(srDesc);

        /* Checkbox abilitazione */
        m_smartRouterChk = new QCheckBox(tr("Abilita Smart Router (LOCAL \xe2\x86\x92 CLOUD)"), srGroup);
        QSettings s;
        m_smartRouterChk->setChecked(s.value(P::SK::kSmartRouterEnabled, false).toBool());
        srLay->addWidget(m_smartRouterChk);

        /* Form campi */
        auto* form = new QFormLayout;
        form->setSpacing(6);
        form->setContentsMargins(0, 4, 0, 0);

        m_cloudUrlEdit = new QLineEdit(
            s.value(P::SK::kCloudApiUrl,
                    "https://api.openai.com/v1/chat/completions").toString(), srGroup);
        m_cloudUrlEdit->setPlaceholderText("https://api.openai.com/v1/chat/completions");
        form->addRow("URL endpoint:", m_cloudUrlEdit);

        m_cloudModelEdit = new QLineEdit(
            s.value(P::SK::kCloudApiModel, "gpt-4o-mini").toString(), srGroup);
        m_cloudModelEdit->setPlaceholderText("gpt-4o-mini");
        form->addRow("Modello cloud:", m_cloudModelEdit);

        m_cloudApiKeyEdit = new QLineEdit(LanServer::loadSecret(QStringLiteral("cloud_api_key")), srGroup);
        m_cloudApiKeyEdit->setEchoMode(QLineEdit::Password);
        m_cloudApiKeyEdit->setPlaceholderText("sk-...");
        form->addRow("API key:", m_cloudApiKeyEdit);

        srLay->addLayout(form);

        /* Pulsante salva + status */
        auto* srRow = new QHBoxLayout;
        auto* srSaveBtn = new QPushButton(tr("\xf0\x9f\x92\xbe  Salva"), srGroup);
        srSaveBtn->setObjectName("actionBtn");
        srSaveBtn->setFixedWidth(dpiScale(120));
        m_smartRouterStatusLbl = new QLabel("", srGroup);
        m_smartRouterStatusLbl->setObjectName("hintLabel");
        srRow->addWidget(srSaveBtn);
        srRow->addWidget(m_smartRouterStatusLbl);
        srRow->addStretch(1);
        srLay->addLayout(srRow);

        mainLay->addWidget(srGroup);

        connect(srSaveBtn, &QPushButton::clicked,
                this, &ImpostazioniPage::onSmartRouterSaveClicked);
    }

    mainLay->addStretch(1);

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
          "Se non trovato: esegui build.bat (Windows) o aggiorna.sh (Linux/macOS) dalla root.",
#ifdef Q_OS_WIN
          "gi\xc3\xa0 compilato: usa build.bat dalla root. Binario in gui\\build\\bin\\llama-server.exe",
#else
          "gi\xc3\xa0 compilato: usa aggiorna.sh dalla root. Binario in gui/build/bin/llama-server",
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
        tr("\xf0\x9f\x93\xa6  Dipendenze esterne"));
    title->setObjectName("sectionTitle");
    outer->addWidget(title);

    auto* desc = new QLabel(
        tr("Librerie e strumenti necessari o opzionali. "
        "Premi \xc2\xab Verifica tutto \xc2\xbb per controllare "
        "cosa \xc3\xa8 disponibile nel sistema."));
    desc->setWordWrap(true);
    desc->setObjectName("hintLabel");
    outer->addWidget(desc);

    auto* verifyBtn = new QPushButton(
        tr("\xf0\x9f\x94\x8d  Verifica tutto"));
    verifyBtn->setObjectName("actionBtn");
    verifyBtn->setFixedWidth(dpiScale(160));
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
            auto* l = new QLabel(tr("<b>") + t + "</b>");
            l->setObjectName("hintLabel");
            if (w > 0) l->setFixedWidth(w);
            return l;
        };
        hdrLay->addWidget(makeHdr("", 20));
        hdrLay->addWidget(makeHdr("Nome", 160));
        hdrLay->addWidget(makeHdr("Descrizione"), 1);
        hdrLay->addWidget(makeHdr("Installazione", 260));
        hdrLay->addWidget(makeHdr("", 90));   /* colonna pulsante Installa */
        listLayout->addWidget(hdr);
    }

    /* Separatore */
    auto* sepTop = new QFrame;
    sepTop->setFrameShape(QFrame::HLine);
    sepTop->setObjectName("sidebarSep");
    listLayout->addWidget(sepTop);

    /* Raccoglie i puntatori ai label di stato per aggiornarli */
    QVector<QLabel*>      statusDots;
    QVector<QString>      execs;
    QVector<QPushButton*> installBtns;

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
        dot->setFixedWidth(dpiScale(20));
        dot->setAlignment(Qt::AlignCenter);
        dot->setStyleSheet("color:#4a5568;font-size:10px;");

        /* Nome + "(opzionale)" */
        QString nameHtml = QString("<b>%1</b>").arg(d.name);
        if (d.optional)
            nameHtml += " <span style='color:#4a5568;font-size:10px;'>"
                        "(opzionale)</span>";
        auto* nameLbl = new QLabel(nameHtml);
        nameLbl->setTextFormat(Qt::RichText);
        nameLbl->setFixedWidth(dpiScale(160));

        /* Descrizione */
        auto* descLbl = new QLabel(d.desc);
        descLbl->setObjectName("hintLabel");

        /* Comando di installazione */
        auto* installLbl = new QLabel(
            QString("<code style='color:#4a5568;font-size:11px;'>%1</code>")
                .arg(d.install));
        installLbl->setTextFormat(Qt::RichText);
        installLbl->setFixedWidth(dpiScale(260));
        installLbl->setWordWrap(true);

        /* Pulsante Installa — visibile solo dopo verifica se rosso */
        auto* installBtn = new QPushButton(
            tr("\xf0\x9f\x92\xbe  " "Installa"), row);
        installBtn->setFixedWidth(dpiScale(90));
        installBtn->setToolTip(QString::fromUtf8(d.install));
        installBtn->setObjectName("actionBtn");
        installBtn->setVisible(false);

        /* Nasconde il bottone per dipendenze senza exec (Qt / sistema) */
        if (QByteArray(d.exec).isEmpty())
            installBtn->setVisible(false);   /* rimarrà nascosto sempre */

        rowLay->addWidget(dot);
        rowLay->addWidget(nameLbl);
        rowLay->addWidget(descLbl, 1);
        rowLay->addWidget(installLbl);
        rowLay->addWidget(installBtn);

        listLayout->addWidget(row);
        statusDots.append(dot);
        execs.append(QString::fromUtf8(d.exec));
        installBtns.append(installBtn);

        /* ── Comando di installazione per il dialog ── */
        QString installCmd = QString::fromUtf8(d.install);
        QString depName    = QString::fromUtf8(d.name);

        /* Casi speciali */
        if (QByteArray(d.exec) == "llama-cli") {
            installCmd = "cmake --build gui/build -j$(nproc)";
        } else if (QByteArray(d.exec) == "piper") {
            installCmd = "# Vedi https://github.com/rhasspy/piper per il binario";
        }

        /* Connessione click bottone Installa */
        QObject::connect(installBtn, &QPushButton::clicked,
                         installBtn, [installBtn, depName, installCmd]() {
            auto* dlg = new QDialog(installBtn->window());
            dlg->setWindowTitle(
                QString("Installa %1").arg(depName));
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            dlg->setMinimumWidth(480);

            auto* dlgLayout = new QVBoxLayout(dlg);
            dlgLayout->setSpacing(10);
            dlgLayout->setContentsMargins(16, 16, 16, 12);

            /* Descrizione */
            auto* cmdLbl = new QLabel(
                QString("<b>Comando di installazione per <i>%1</i>:</b>")
                    .arg(depName));
            cmdLbl->setTextFormat(Qt::RichText);
            dlgLayout->addWidget(cmdLbl);

            /* Campo read-only con il comando */
            auto* cmdEdit = new QLineEdit(installCmd, dlg);
            cmdEdit->setReadOnly(true);
            cmdEdit->setFont(QFont("monospace"));
            cmdEdit->setCursorPosition(0);
            dlgLayout->addWidget(cmdEdit);

            /* Riga pulsanti azione */
            auto* btnRow    = new QWidget(dlg);
            auto* btnRowLay = new QHBoxLayout(btnRow);
            btnRowLay->setContentsMargins(0, 0, 0, 0);
            btnRowLay->setSpacing(8);

            auto* copyBtn     = new QPushButton(
                tr("\xf0\x9f\x93\x8b " "Copia"), dlg);
            auto* terminalBtn = new QPushButton(
                tr("\xf0\x9f\x96\xa5 " "Apri terminale"), dlg);
            auto* closeBtn    = new QPushButton(tr("Chiudi"), dlg);

            copyBtn->setObjectName("actionBtn");
            terminalBtn->setObjectName("actionBtn");

            btnRowLay->addWidget(copyBtn);
            btnRowLay->addWidget(terminalBtn);
            btnRowLay->addStretch();
            btnRowLay->addWidget(closeBtn);
            dlgLayout->addWidget(btnRow);

            /* Copia negli appunti */
            QObject::connect(copyBtn, &QPushButton::clicked,
                             copyBtn, [installCmd, copyBtn]() {
                QGuiApplication::clipboard()->setText(installCmd);
                copyBtn->setText(tr("\xe2\x9c\x85 " "Copiato!"));
                QTimer::singleShot(1500, copyBtn, [copyBtn]() {
                    copyBtn->setText(tr("\xf0\x9f\x93\x8b " "Copia"));
                });
            });

            /* Apri terminale con il comando */
            QObject::connect(terminalBtn, &QPushButton::clicked,
                             terminalBtn, [installCmd]() {
                /* Salta i comandi-commento (es. piper) */
                if (installCmd.startsWith("#"))
                    return;
                /* Sostituisce le virgolette doppie con singole per sicurezza nella shell */
                QString safeCmd = QString(installCmd).replace('"', '\'');
                const QStringList terminals = {
                    "x-terminal-emulator",
                    "xterm",
                    "konsole",
                    "gnome-terminal"
                };
                for (const QString& term : terminals) {
                    if (!QStandardPaths::findExecutable(term).isEmpty()) {
                        QProcess::startDetached(
                            term,
                            QStringList() << "-e"
                                << ("bash -c \""
                                    + safeCmd
                                    + " ; exec bash\""));
                        return;
                    }
                }
            });

            /* Chiudi */
            QObject::connect(closeBtn, &QPushButton::clicked,
                             dlg, &QDialog::accept);

            dlg->exec();
        });
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
        [statusDots, execs, installBtns]() {
        for (int i = 0; i < statusDots.size(); ++i) {
            const QString& ex = execs[i];
            if (ex.isEmpty()) {
                /* Qt / sistema — sempre disponibile */
                statusDots[i]->setStyleSheet(
                    "color:#4ade80;font-size:10px;");   /* verde */
                installBtns[i]->setVisible(false);
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
            /* Mostra il bottone Installa solo se la dipendenza non è trovata */
            installBtns[i]->setVisible(!found);
        }
    });

    return page;
}

/* ══════════════════════════════════════════════════════════════
   buildPythonDepsTab — moduli Python richiesti dagli MCP
   ══════════════════════════════════════════════════════════════ */
QWidget* ImpostazioniPage::buildPythonDepsTab()
{
    struct PyPkg {
        const char* pip;
        const char* importN;
        const char* mcp;
        const char* desc;
        const char* where;
    };
    static const PyPkg kPkgs[] = {
        { "obsws-python", "obsws_python",
          "OBS MCP",
          "Client WebSocket per OBS Studio (porta 4455)",
          "AppController \xe2\x86\x92 OBS" },
        { "pyserial", "serial",
          "TinyMCP",
          "Comunicazione USB/seriale per Arduino / ESP32",
          "AppController \xe2\x86\x92 TinyMCP" },
        { "rdkit", "rdkit",
          "RDKit MCP",
          "Chemioinformatica: strutture e propriet\xc3\xa0 molecolari",
          "Ricerca \xe2\x86\x92 RDKit" },
        { "avogadro", "avogadro",
          "Avogadro MCP",
          "Visualizzazione molecole 3D (Avogadro2)",
          "Ricerca \xe2\x86\x92 Avogadro" },
        { "python-telegram-bot", "telegram.ext; from telegram.ext import Application",
          "Telegram Bot",
          "Bot Telegram locale con risposta AI (v20+ richiesta)",
          "AppController \xe2\x86\x92 Telegram" },
    };
    const int nPkgs = static_cast<int>(sizeof(kPkgs) / sizeof(kPkgs[0]));

    auto* page  = new QWidget;
    auto* outer = new QVBoxLayout(page);
    outer->setContentsMargins(16, 16, 16, 16);
    outer->setSpacing(10);

    auto* titleLbl = new QLabel(tr("\xf0\x9f\x90\x8d  Moduli Python per MCP"), page);
    titleLbl->setObjectName("sectionTitle");
    outer->addWidget(titleLbl);

    auto* descLbl = new QLabel(
        tr("Installa solo i moduli che servono al tuo flusso di lavoro. "
        "Premi <b>Verifica stato</b> per controllare cosa manca."), page);
    descLbl->setWordWrap(true);
    descLbl->setObjectName("hintLabel");
    outer->addWidget(descLbl);

    /* ── Pulsanti azioni globali ── */
    auto* btnRow = new QWidget(page);
    auto* btnLay = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(0, 0, 0, 0);
    btnLay->setSpacing(8);

    auto* verifyBtn     = new QPushButton(tr("\xf0\x9f\x94\x8d  Verifica stato"), btnRow);
    auto* installAllBtn = new QPushButton(tr("\xf0\x9f\x9a\x80  Installa tutti i mancanti"), btnRow);
    verifyBtn->setObjectName("actionBtn");
    installAllBtn->setObjectName("actionBtn");
    btnLay->addWidget(verifyBtn);
    btnLay->addWidget(installAllBtn);
    btnLay->addStretch();
    outer->addWidget(btnRow);

    /* ── Scroll area con righe pacchetti ── */
    auto* scroll = new QScrollArea(page);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    m_pipScroll = scroll;

    auto* container = new QWidget;
    auto* listLay   = new QVBoxLayout(container);
    listLay->setContentsMargins(0, 0, 0, 0);
    listLay->setSpacing(2);

    /* Intestazione colonne */
    {
        auto* hdr    = new QWidget;
        auto* hdrLay = new QHBoxLayout(hdr);
        hdrLay->setContentsMargins(8, 2, 8, 2);
        hdrLay->setSpacing(10);
        auto mk = [](const QString& t, int w = -1) {
            auto* l = new QLabel(tr("<b>") + t + "</b>");
            l->setObjectName("hintLabel");
            if (w > 0) l->setFixedWidth(w);
            return l;
        };
        hdrLay->addWidget(mk("",                    20));
        hdrLay->addWidget(mk("Pacchetto (pip)",     155));
        hdrLay->addWidget(mk("MCP",                 110));
        hdrLay->addWidget(mk("Descrizione"),         1 );
        hdrLay->addWidget(mk("Usato in",            195));
        hdrLay->addWidget(mk("",                     90));
        listLay->addWidget(hdr);
    }
    {
        auto* sep = new QFrame; sep->setFrameShape(QFrame::HLine);
        sep->setObjectName("sidebarSep");
        listLay->addWidget(sep);
    }

    /* ── Raccoglie vettori per closure ── */
    QVector<QLabel*>      dots;
    QVector<QString>      importNames;
    QVector<QString>      pipNames;
    QVector<QPushButton*> installBtns;

    for (int i = 0; i < nPkgs; ++i) {
        const PyPkg& p = kPkgs[i];

        auto* row    = new QWidget;
        auto* rowLay = new QHBoxLayout(row);
        rowLay->setContentsMargins(8, 5, 8, 5);
        rowLay->setSpacing(10);
        row->setObjectName((i % 2 == 0) ? "depRowEven" : "depRowOdd");

        auto* dot = new QLabel("\xe2\x97\x8f", row);
        dot->setFixedWidth(dpiScale(20));
        dot->setAlignment(Qt::AlignCenter);
        dot->setStyleSheet("color:#4a5568;font-size:10px;");

        auto* pkgLbl = new QLabel(
            QString("<b><code>%1</code></b>").arg(QString::fromUtf8(p.pip)), row);
        pkgLbl->setTextFormat(Qt::RichText);
        pkgLbl->setFixedWidth(dpiScale(155));

        auto* mcpLbl = new QLabel(QString::fromUtf8(p.mcp), row);
        mcpLbl->setFixedWidth(dpiScale(110));
        mcpLbl->setObjectName("hintLabel");

        auto* descL = new QLabel(QString::fromUtf8(p.desc), row);
        descL->setObjectName("hintLabel");

        auto* whereL = new QLabel(QString::fromUtf8(p.where), row);
        whereL->setFixedWidth(dpiScale(195));
        whereL->setObjectName("hintLabel");

        auto* btn = new QPushButton(tr("\xf0\x9f\x92\xbe  Installa"), row);
        btn->setFixedWidth(dpiScale(90));
        btn->setObjectName("actionBtn");

        rowLay->addWidget(dot);
        rowLay->addWidget(pkgLbl);
        rowLay->addWidget(mcpLbl);
        rowLay->addWidget(descL, 1);
        rowLay->addWidget(whereL);
        rowLay->addWidget(btn);
        listLay->addWidget(row);

        const QString pipN    = QString::fromUtf8(p.pip);
        const QString importN = QString::fromUtf8(p.importN);
        m_pipDots.insert(pipN, dot);

        dots.append(dot);
        importNames.append(importN);
        pipNames.append(pipN);
        installBtns.append(btn);
    }
    listLay->addStretch();
    scroll->setWidget(container);
    outer->addWidget(scroll, 1);

    /* ── Output pip (nascosto finché non si installa) ── */
    auto* logLbl = new QLabel(tr("Output installazione:"), page);
    logLbl->setObjectName("hintLabel");
    logLbl->hide();
    outer->addWidget(logLbl);

    auto* logView = new QTextEdit(page);
    logView->setReadOnly(true);
    logView->setMaximumHeight(110);
    logView->setObjectName("codeView");
    logView->hide();
    outer->addWidget(logView);

    /* ── Legenda ── */
    auto* legendLbl = new QLabel(
        "\xe2\x97\x8f  Non verificato \xc2\xa0"
        "<span style='color:#4ade80;'>\xe2\x97\x8f</span>  Installato \xc2\xa0"
        "<span style='color:#f87171;'>\xe2\x97\x8f</span>  Mancante",
        page);
    legendLbl->setTextFormat(Qt::RichText);
    legendLbl->setObjectName("hintLabel");
    outer->addWidget(legendLbl);

    /* ── Helper: verifica importabilità modulo Python ── */
    auto checkImport = [](const QString& importN) -> bool {
        return QProcess::execute(P::findPython(), {"-c", "import " + importN}) == 0;
    };

    /* ── Helper: aggiorna stile pallino ── */
    auto setDot = [](QLabel* dot, bool found) {
        dot->setStyleSheet(found
            ? "color:#4ade80;font-size:10px;"
            : "color:#f87171;font-size:10px;");
    };

    /* ── Helper: avvia pip install asincrono ── */
    auto runPip = [page, logLbl, logView](
            const QString& pipPkg,
            QPushButton*   btn,
            QLabel*        dot) {
        logLbl->show();
        logView->show();
        btn->setEnabled(false);
        btn->setText(tr("\xe2\x8f\xb3 ..."));
        const QString logTarget = (pipPkg == "python-telegram-bot")
            ? "python-telegram-bot>=20"
            : pipPkg;
        logView->append(
            QString("<span style='color:#94a3b8;'>$ pip install %1</span>")
                .arg(logTarget));

        auto* proc = new QProcess(page);
        proc->setProcessChannelMode(QProcess::MergedChannels);

        QObject::connect(proc, &QProcess::readyRead, logView,
            [proc, logView]() {
                const QString out = QString::fromUtf8(proc->readAll()).trimmed();
                if (!out.isEmpty()) logView->append(out);
            });

        QObject::connect(
            proc,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            page,
            [proc, btn, dot](int code, QProcess::ExitStatus) {
                const bool ok = (code == 0);
                dot->setStyleSheet(ok
                    ? "color:#4ade80;font-size:10px;"
                    : "color:#f87171;font-size:10px;");
                btn->setText(ok ? "\xe2\x9c\x85  OK" : "\xf0\x9f\x92\xbe  Installa");
                btn->setEnabled(!ok);
                proc->deleteLater();
            });

        const QString installTarget = (pipPkg == "python-telegram-bot")
            ? "python-telegram-bot>=20"
            : pipPkg;
        proc->start(P::findPython(), {"-m", "pip", "install", installTarget,
                                "--no-input", "--break-system-packages"});
    };

    /* ── Verifica stato ── */
    QObject::connect(verifyBtn, &QPushButton::clicked, page,
        [dots, importNames, installBtns, checkImport, setDot]() {
        for (int i = 0; i < dots.size(); ++i) {
            const bool found = checkImport(importNames[i]);
            setDot(dots[i], found);
            installBtns[i]->setText(found ? "\xe2\x9c\x85  OK" : "\xf0\x9f\x92\xbe  Installa");
            installBtns[i]->setEnabled(!found);
        }
    });

    /* ── Install singolo per ogni riga ── */
    for (int i = 0; i < nPkgs; ++i) {
        QObject::connect(installBtns[i], &QPushButton::clicked, page,
            [i, dots, pipNames, installBtns, runPip]() {
                runPip(pipNames[i], installBtns[i], dots[i]);
            });
    }

    /* ── Install tutti i mancanti ── */
    QObject::connect(installAllBtn, &QPushButton::clicked, page,
        [dots, importNames, pipNames, installBtns,
         installAllBtn, page, logLbl, logView, checkImport, setDot]() {

        QStringList missing;
        QVector<int> missingIdx;
        for (int i = 0; i < dots.size(); ++i) {
            if (!checkImport(importNames[i])) {
                missing << pipNames[i];
                missingIdx << i;
            }
        }

        if (missing.isEmpty()) {
            installAllBtn->setText(tr("\xe2\x9c\x85  Tutto gi\xc3\xa0 installato!"));
            QTimer::singleShot(2000, installAllBtn, [installAllBtn]() {
                installAllBtn->setText(tr("\xf0\x9f\x9a\x80  Installa tutti i mancanti"));
            });
            return;
        }

        installAllBtn->setEnabled(false);
        installAllBtn->setText(tr("\xe2\x8f\xb3 Installazione in corso..."));
        logLbl->show();
        logView->show();
        QStringList logTargets;
        for (const QString& pkg : missing)
            logTargets << (pkg == "python-telegram-bot"
                           ? "python-telegram-bot>=20" : pkg);
        logView->append(
            QString("<span style='color:#94a3b8;'>$ pip install %1</span>")
                .arg(logTargets.join(" ")));

        auto* proc = new QProcess(page);
        proc->setProcessChannelMode(QProcess::MergedChannels);

        QObject::connect(proc, &QProcess::readyRead, logView,
            [proc, logView]() {
                const QString out = QString::fromUtf8(proc->readAll()).trimmed();
                if (!out.isEmpty()) logView->append(out);
            });

        QStringList installTargets;
        for (const QString& pkg : missing)
            installTargets << (pkg == "python-telegram-bot"
                               ? "python-telegram-bot>=20" : pkg);
        QStringList args = {"-m", "pip", "install"};
        args += installTargets;
        args << "--no-input" << "--break-system-packages";

        QObject::connect(
            proc,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            page,
            [proc, installAllBtn, dots, installBtns,
             missingIdx, setDot](int code, QProcess::ExitStatus) {
                const bool ok = (code == 0);
                for (int idx : missingIdx) {
                    setDot(dots[idx], ok);
                    installBtns[idx]->setText(ok ? "\xe2\x9c\x85  OK" : "\xf0\x9f\x92\xbe  Installa");
                    installBtns[idx]->setEnabled(!ok);
                }
                installAllBtn->setText(tr("\xf0\x9f\x9a\x80  Installa tutti i mancanti"));
                installAllBtn->setEnabled(true);
                proc->deleteLater();
            });

        proc->start(P::findPython(), args);
    });

    /* ── Auto-verifica al primo caricamento ── */
    QTimer::singleShot(400, verifyBtn, [verifyBtn]() { verifyBtn->click(); });

    /* ── Wrapper QTabWidget: Moduli MCP + Dipendenze esterne ── */
    auto* innerTabs = new QTabWidget;
    innerTabs->setObjectName("innerTabs");
    innerTabs->addTab(page,                tr("\xf0\x9f\x90\x8d  Moduli MCP"));
    innerTabs->addTab(buildDipendenzeTab(), "\xf0\x9f\x93\xa6  Dipendenze esterne");
    m_pipModuliTabs = innerTabs;
    return innerTabs;
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
    auto* title = new QLabel(tr("\xf0\x9f\x94\x8d  RAG \xe2\x80\x94 Recupero Documenti Aumentato"));
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
        tr("\xe2\x9a\xa0\xef\xb8\x8f  <b>Attenzione privacy</b>: i chunk indicizzati (frammenti di testo dai tuoi "
        "documenti) vengono salvati in chiaro in <code>~/.prismalux_rag.json</code>. "
        "Non indicizzare documenti riservati (dichiarazioni fiscali, dati medici) "
        "se non sei l'unico utente di questo sistema."));
    privacyLbl->setWordWrap(true);
    privacyLbl->setTextFormat(Qt::RichText);
    privacyLbl->setObjectName("hintLabel");
    privacyLbl->setStyleSheet("color: #e8a020;");   /* amber di avviso */
    outer->addWidget(privacyLbl);

    auto* noSaveChk = new QCheckBox(
        tr("Non salvare su disco (solo RAM \xe2\x80\x94 indice perso alla chiusura)"));
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
    dirEdit->setPlaceholderText(tr("/percorso/documenti/"));
    {
        /* Default: cartella RAG nella home dell'utente corrente.
           Non usare P::root() che potrebbe contenere il path dell'utente
           che ha compilato (es. /home/wildlux/…) invece dell'utente attuale. */
        const QString defaultRagDir =
            QDir::homePath() + "/prismalux_rag_docs";
        auto& cfg = AppConfig::s();
        QString saved = cfg.value(P::SK::kRagDocsDir, "").toString();
        if (saved.isEmpty()) {
            saved = defaultRagDir;
            cfg.setValue(P::SK::kRagDocsDir, saved);
        }
        dirEdit->setText(saved);
    }
    auto* browseBtn = new QPushButton(tr("Sfoglia..."));
    browseBtn->setObjectName("actionBtn");
    browseBtn->setFixedWidth(dpiScale(90));
    browseBtn->setToolTip(tr("Scegli la cartella dove metti i tuoi documenti RAG"));

    auto* openDirBtn = new QPushButton("\xf0\x9f\x93\x82");   /* 📂 */
    openDirBtn->setObjectName("actionBtn");
    openDirBtn->setFixedWidth(dpiScale(36));
    openDirBtn->setToolTip(tr("Apri la cartella nel file manager"));
    QObject::connect(openDirBtn, &QPushButton::clicked, dirEdit, [dirEdit] {
        const QString d = dirEdit->text().trimmed();
        if (!d.isEmpty() && QDir(d).exists())
            QDesktopServices::openUrl(QUrl::fromLocalFile(d));
        else
            QDesktopServices::openUrl(QUrl::fromLocalFile(QDir::homePath()));
    });

    dirLay->addWidget(dirEdit, 1);
    dirLay->addWidget(browseBtn);
    dirLay->addWidget(openDirBtn);
    fl->addRow("Cartella RAG:", dirRow);

    /* Avviso se la cartella non esiste */
    auto* ragWarnLbl = new QLabel;
    ragWarnLbl->setObjectName("warnLabel");
    ragWarnLbl->setVisible(false);
    ragWarnLbl->setWordWrap(true);
    ragWarnLbl->setTextFormat(Qt::RichText);
    auto updateRagWarn = [ragWarnLbl, dirEdit] {
        const QString d = dirEdit->text().trimmed();
        const bool bad = d.isEmpty() || !QDir(d).exists();
        ragWarnLbl->setText(
            bad ? "\xe2\x9a\xa0\xef\xb8\x8f  La cartella non esiste. "
                  "Cliccla su \xe2\x80\x9cSfoglia...\xe2\x80\x9d per scegliere "
                  "dove salvare i tuoi documenti RAG, oppure creala con:"
                  "<br><code>mkdir -p " + (d.isEmpty() ? "~/prismalux_rag_docs" : d.toHtmlEscaped()) + "</code>"
                : QString());
        ragWarnLbl->setVisible(bad);
    };
    QObject::connect(dirEdit, &QLineEdit::textChanged, ragWarnLbl, updateRagWarn);
    updateRagWarn();
    fl->addRow("", ragWarnLbl);

    /* Max risultati */
    auto* maxSpin = new QSpinBox;
    maxSpin->setRange(1, 20);
    {
        maxSpin->setValue(AppConfig::s().value(P::SK::kRagMaxResults, 5).toInt());
    }
    maxSpin->setSuffix("  risultati");
    maxSpin->setToolTip(
        "Numero massimo di chunk RAG restituiti per ogni query.\n"
        "Ogni chunk e' un frammento di documento (~400 caratteri).\n"
        "Valore piu' alto = piu' contesto, ma piu' token usati.\n"
        "Consigliato: 3-5.");
    fl->addRow("Massimi:", maxSpin);
    fl->labelForField(maxSpin)->setToolTip(maxSpin->toolTip());

    /* ── Layout a 2 colonne ── */
    auto* columns   = new QHBoxLayout;
    auto* leftCol   = new QVBoxLayout;
    auto* rightCol  = new QVBoxLayout;
    columns->setSpacing(20);
    leftCol->setSpacing(12);
    rightCol->setSpacing(12);

    leftCol->addLayout(fl);

    /* ── Trasformata Johnson-Lindenstrauss ── */
    {
        auto* jlFrame = new QGroupBox(tr("Ottimizzazione vettori RAG"), page);
        jlFrame->setObjectName("cardGroup");
        auto* jlLay = new QVBoxLayout(jlFrame);
        jlLay->setSpacing(6);

        auto* jlChk = new QCheckBox(
            tr("Trasformata di Johnson\xe2\x80\x93Lindenstrauss (JL)"), jlFrame);
        jlChk->setObjectName("cardDesc");
        {
            jlChk->setChecked(AppConfig::s().value(P::SK::kRagJlTransform, true).toBool());
        }
        jlChk->setToolTip(
            tr("Riduce la dimensionalit\xc3\xa0 dei vettori di embedding mantenendo le distanze.\n"
            "Migliora le performance di ricerca RAG con grandi basi di conoscenza.\n"
            "Consigliata: attiva."));

        auto* jlHint = new QLabel(
            "Comprime i vettori di embedding riducendo i tempi di ricerca senza "
            "perdita significativa di accuratezza. Consigliata con pi\xc3\xb9 di 1000 documenti.", jlFrame);
        jlHint->setObjectName("hintLabel");
        jlHint->setWordWrap(true);

        jlLay->addWidget(jlChk);
        jlLay->addWidget(jlHint);

        connect(jlChk, &QCheckBox::toggled,
                this, &ImpostazioniPage::onRagJlToggled);

        leftCol->addWidget(jlFrame);
    }
    /* Lo stretch della colonna sinistra viene aggiunto in coda, dopo la
       card "Modelli embedding consigliati" (arrotolabile, vedi sotto). */

    /* ── Colonna destra: stato + download + embedding ── */
    m_ragStatusLbl = new QLabel;
    m_ragStatusLbl->setObjectName("hintLabel");
    m_ragStatusLbl->setWordWrap(true);
    m_ragStatusLbl->setTextFormat(Qt::RichText);
    auto* statusLbl = m_ragStatusLbl;

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
    rightCol->addWidget(m_ragStatusLbl);

    /* ── Card: file nella cartella RAG con spunte (D-47).
       Sostituisce il pulsante "Scarica documenti AdE 2026" (rimosso:
       URL destinati a diventare obsoleti). La lista È lo stato desiderato
       dell'indice: spuntato = nell'indice (o da indicizzare), despuntato =
       fuori dall'indice; "Reindicizza ora" allinea l'indice alle spunte. ── */
    {
        auto* fileCard = new QFrame(page);
        fileCard->setObjectName("cardFrame");
        auto* fileCardLay = new QVBoxLayout(fileCard);
        fileCardLay->setContentsMargins(12, 10, 12, 10);
        fileCardLay->setSpacing(6);

        auto* fileHdrRow = new QHBoxLayout;
        /* D-66: membro — refreshRagFileList() aggiorna i conteggi nel titolo */
        m_ragFileCardTitle = new QLabel(
            tr("\xf0\x9f\x93\x82  <b>File nella cartella RAG</b> "
               "<span style='color:#94a3b8;font-size:11px;font-weight:normal;'>"
               "\xe2\x9c\x85 indicizzato \xc2\xb7 \xf0\x9f\x94\x84 modificato \xc2\xb7 "
               "\xe2\xac\x9c da indicizzare</span>"),
            fileCard);
        m_ragFileCardTitle->setTextFormat(Qt::RichText);
        m_ragFileCardTitle->setToolTip(
            tr("\xe2\x9c\x85 indicizzato e aggiornato \xc2\xb7 "
               "\xf0\x9f\x94\x84 indicizzato ma modificato su disco \xc2\xb7 "
               "\xe2\xac\x9c mai indicizzato"));
        fileHdrRow->addWidget(m_ragFileCardTitle, 1);

        auto* fileAllBtn = new QPushButton("\xe2\x98\x91", fileCard);   /* ☑ */
        fileAllBtn->setObjectName("navBtn");
        fileAllBtn->setFixedSize(dpiScale(28), dpiScale(24));
        fileAllBtn->setToolTip(tr("Seleziona tutti i file (nessuna esclusione)"));
        QObject::connect(fileAllBtn, &QPushButton::clicked,
                         this, &ImpostazioniPage::onRagFileSelectAllClicked);
        fileHdrRow->addWidget(fileAllBtn);

        auto* fileNoneBtn = new QPushButton("\xe2\x98\x90", fileCard);   /* ☐ */
        fileNoneBtn->setObjectName("navBtn");
        fileNoneBtn->setFixedSize(dpiScale(28), dpiScale(24));
        fileNoneBtn->setToolTip(tr("Deseleziona tutti i file "
                                   "(tutti esclusi al prossimo Reindicizza)"));
        QObject::connect(fileNoneBtn, &QPushButton::clicked,
                         this, &ImpostazioniPage::onRagFileDeselectAllClicked);
        fileHdrRow->addWidget(fileNoneBtn);

        auto* fileRefreshBtn = new QPushButton("\xf0\x9f\x94\x84", fileCard);
        fileRefreshBtn->setObjectName("navBtn");
        fileRefreshBtn->setFixedSize(dpiScale(28), dpiScale(24));
        fileRefreshBtn->setToolTip(tr("Aggiorna la lista dei file"));
        QObject::connect(fileRefreshBtn, &QPushButton::clicked,
                         this, &ImpostazioniPage::onRagFileListRefreshClicked);
        fileHdrRow->addWidget(fileRefreshBtn);
        fileCardLay->addLayout(fileHdrRow);

        m_ragFileList = new QListWidget(fileCard);
        m_ragFileList->setObjectName("chatLog");
        m_ragFileList->setAlternatingRowColors(true);
        m_ragFileList->setMinimumHeight(dpiScale(110));
        m_ragFileList->setMaximumHeight(dpiScale(210));
        m_ragFileList->setToolTip(
            tr("Togli la spunta ai file che NON vuoi nell'indice RAG:\n"
               "al prossimo \"Reindicizza ora\" verranno esclusi (e rimossi\n"
               "dall'indice se gi\xc3\xa0 presenti). I file spuntati vengono\n"
               "indicizzati solo se nuovi o modificati."));
        fileCardLay->addWidget(m_ragFileList);
        rightCol->addWidget(fileCard);
        QObject::connect(m_ragFileList, &QListWidget::itemChanged,
                         this, &ImpostazioniPage::onRagFileItemChanged);
        refreshRagFileList();
    }

    /* ── Modello embedding ── */
    {
        auto* embedRow = new QHBoxLayout;
        auto* embedLbl = new QLabel(tr("Modello embedding:"));
        embedLbl->setObjectName("hintLabel");

        m_ragEmbedCombo = new QComboBox;
        m_ragEmbedCombo->setFixedHeight(dpiScale(28));
        m_ragEmbedCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_ragEmbedCombo->setToolTip(
            "Modello Ollama dedicato per gli embedding (vettorizzazione del testo).\n"
            "I modelli chat (qwen3, llama, ecc.) NON supportano /api/embeddings.\n"
            "Modelli compatibili con RAG sono marcati con un segno di spunta.");
        {
            /* Pre-popola con il valore salvato + modelli statici noti */
            const QString saved = AppConfig::s().value(P::SK::kRagEmbedModel, "").toString();
            const QString cur = saved.isEmpty() ? "embeddinggemma" : saved;
            static const char* kKnown[] = {
                "embeddinggemma", "nomic-embed-text", "all-minilm", "mxbai-embed-large",
                "bge-large", "bge-base", "bge-small", "e5-large", "e5-base",
                "e5-small", "jina-embeddings-v2-base-en", "stella-en-1.5b-v5", nullptr
            };
            for (int i = 0; kKnown[i]; ++i) {
                m_ragEmbedCombo->addItem(
                    QString("\xe2\x9c\x94 %1").arg(kKnown[i]), QString(kKnown[i]));
            }
            /* Assicura che il modello salvato sia presente */
            int idx = m_ragEmbedCombo->findData(cur);
            if (idx < 0) {
                m_ragEmbedCombo->addItem(QString("\xe2\x9c\x94 %1").arg(cur), cur);
                idx = m_ragEmbedCombo->count() - 1;
            }
            m_ragEmbedCombo->setCurrentIndex(idx);
        }
        QObject::connect(m_ragEmbedCombo,
                         QOverload<int>::of(&QComboBox::currentIndexChanged),
                         this, &ImpostazioniPage::onRagEmbedComboChanged);

        auto* embedRefreshBtn = new QPushButton("\xf0\x9f\x94\x84");
        embedRefreshBtn->setObjectName("navBtn");
        embedRefreshBtn->setFixedSize(28, 28);
        embedRefreshBtn->setToolTip(tr("Aggiorna lista modelli da Ollama"));
        QObject::connect(embedRefreshBtn, &QPushButton::clicked,
                         this, &ImpostazioniPage::onRagEmbedRefreshClicked);

        embedRow->addWidget(embedLbl);
        embedRow->addWidget(m_ragEmbedCombo, 1);
        embedRow->addWidget(embedRefreshBtn);
        rightCol->addLayout(embedRow);

        /* ── Card: modelli embedding consigliati ── */
        struct EmbedRec { const char* name; const char* size; const char* note; };
        static const EmbedRec kEmbed[] = {
            { "embeddinggemma",    "~621 MB", "Consigliato. Google Gemma3 300M, 100+ lingue. SOTA per dimensione, ottimo per RAG multilingua." },
            { "all-minilm",        "~46 MB",  "Ultraleggero. Ideale su PC con poca RAM. Qualita' sufficiente per RAG su documenti brevi." },
            { "nomic-embed-text",  "~274 MB", "Bilanciato. Buon rapporto qualita'/peso. Alternativa consolidata." },
            { "mxbai-embed-large", "~670 MB", "Alta qualita'. Ottimo per documenti tecnici lunghi e multilingua." },
        };

        auto* embedCard = new QFrame(page);
        embedCard->setObjectName("cardFrame");
        auto* embedCardLay = new QVBoxLayout(embedCard);
        embedCardLay->setContentsMargins(12, 10, 12, 10);
        embedCardLay->setSpacing(6);

        /* Titolo migrato nell'header della CollapsibleSection (vedi sotto);
           qui resta solo il sottotitolo. */
        auto* embedCardTitle = new QLabel(
            "<span style='color:#94a3b8;font-size:11px;'>"
            "piccoli — non pesano sulla RAM del modello chat</span>",
            embedCard);
        embedCardTitle->setTextFormat(Qt::RichText);
        embedCardLay->addWidget(embedCardTitle);

        auto* embedGrid = new QGridLayout;
        embedGrid->setSpacing(4);
        embedGrid->setColumnStretch(2, 1);

        int embedRow2 = 0;
        for (const EmbedRec& r : kEmbed) {
            auto* nameLbl = new QLabel(
                QString("<code style='color:#60a5fa;'>%1</code>").arg(r.name), embedCard);
            nameLbl->setTextFormat(Qt::RichText);
            auto* sizeLbl = new QLabel(
                QString("<span style='color:#a3e635;'>%1</span>").arg(r.size), embedCard);
            sizeLbl->setTextFormat(Qt::RichText);
            sizeLbl->setFixedWidth(dpiScale(70));
            auto* noteLbl = new QLabel(r.note, embedCard);
            noteLbl->setObjectName("cardDesc");
            noteLbl->setWordWrap(true);

            auto* useBtn = new QPushButton(tr("Usa"), embedCard);
            useBtn->setObjectName("navBtn");
            useBtn->setFixedSize(46, 22);
            const QString modelName = r.name;
            QObject::connect(useBtn, &QPushButton::clicked, m_ragEmbedCombo,
                [this, modelName] {
                    int i = m_ragEmbedCombo->findData(modelName);
                    if (i >= 0) m_ragEmbedCombo->setCurrentIndex(i);
                    else {
                        m_ragEmbedCombo->addItem(
                            QString("\xe2\x9c\x94 %1").arg(modelName), modelName);
                        m_ragEmbedCombo->setCurrentIndex(m_ragEmbedCombo->count()-1);
                    }
                });

            embedGrid->addWidget(nameLbl, embedRow2, 0);
            embedGrid->addWidget(sizeLbl, embedRow2, 1);
            embedGrid->addWidget(noteLbl, embedRow2, 2);
            embedGrid->addWidget(useBtn,  embedRow2, 3);
            ++embedRow2;
        }
        embedCardLay->addLayout(embedGrid);

        auto* embedHint = new QLabel(
            tr("\xe2\x9a\xa0\xef\xb8\x8f  I modelli chat (qwen3, llama, ecc.) NON supportano /api/embeddings "
            "e non devono essere usati qui."), embedCard);
        embedHint->setObjectName("hintLabel");
        embedHint->setWordWrap(true);
        embedHint->setStyleSheet("color:#f59e0b;");
        embedCardLay->addWidget(embedHint);

        /* Card arrotolabile (stile Blender) nella colonna SINISTRA: è
           materiale di consultazione — chiusa all'avvio, la colonna destra
           si accorcia e i pulsanti di indicizzazione salgono. */
        leftCol->addWidget(new CollapsibleSection(
            tr("\xf0\x9f\x9b\x96 Modelli embedding consigliati"),
            embedCard, /*startOpen=*/false, page));
    }

    /* Riga pulsanti: [Ferma indicizzazione]  [Reindicizza ora] —
       colonna destra, subito sotto il combo embedding */
    auto* btnRow = new QHBoxLayout;
    btnRow->setSpacing(8);

    m_btnStopIndex = new QPushButton(tr("\xe2\x8f\xb9  Ferma indicizzazione"));
    m_btnStopIndex->setObjectName("actionBtn");
    m_btnStopIndex->setProperty("danger", true);
    m_btnStopIndex->setFixedHeight(dpiScale(32));
    m_btnStopIndex->setEnabled(false);   /* abilitato solo durante indexing */
    m_btnStopIndex->setToolTip(
        tr("Interrompe l'indicizzazione in corso dopo il chunk attuale.\n"
        "I chunk già completati vengono salvati."));
    btnRow->addWidget(m_btnStopIndex);

    m_ragReindexBtn = new QPushButton(tr("\xf0\x9f\x94\x84  Reindicizza ora"));
    auto* reindexBtn = m_ragReindexBtn;  /* alias locale */
    reindexBtn->setObjectName("actionBtn");
    reindexBtn->setFixedHeight(dpiScale(32));
    reindexBtn->setToolTip(
        QString("Indicizza i documenti dalla cartella selezionata.\n"
                "L'indicizzazione continua in background anche cambiando finestra.\n"
                "Indice salvato in: %1\n"
                "(disabilitabile con la checkbox \"Non salvare su disco\" sopra)")
            .arg(QDir::homePath() + "/.prismalux_rag.json"));
    btnRow->addWidget(reindexBtn);

    rightCol->addLayout(btnRow);

    /* Barra progresso indicizzazione */
    auto* progressBar = new QProgressBar;
    progressBar->setObjectName("ragProgressBar");
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    progressBar->setTextVisible(true);
    progressBar->setFormat("%p%  (%v / %m chunk)");
    progressBar->setFixedHeight(dpiScale(22));
    progressBar->setVisible(false);
    rightCol->addWidget(progressBar);
    m_ragProgressBar = progressBar;

    /* Label feedback */
    auto* feedbackLbl = new QLabel;
    feedbackLbl->setObjectName("hintLabel");
    feedbackLbl->setWordWrap(true);
    feedbackLbl->setTextFormat(Qt::RichText);
    feedbackLbl->setVisible(false);
    rightCol->addWidget(feedbackLbl);

    leftCol->addStretch();
    rightCol->addStretch();

    columns->addLayout(leftCol, 1);
    columns->addLayout(rightCol, 1);
    outer->addLayout(columns);

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
    outer->setContentsMargins(16, 16, 16, 16);
    outer->setSpacing(12);

    const AiChatParams cur = AiChatParams::load();

    /* ── Header ── */
    auto* title = new QLabel(tr("\xe2\x9a\x99\xef\xb8\x8f  Parametri AI"));
    title->setObjectName("sectionTitle");
    outer->addWidget(title);

    auto* fileLbl = new QLabel(
        "\xf0\x9f\x93\x84  <code>" + AiChatParams::filePath() + "</code>");
    fileLbl->setWordWrap(true);
    fileLbl->setObjectName("hintLabel");
    fileLbl->setTextFormat(Qt::RichText);
    outer->addWidget(fileLbl);

    /* Due colonne: sinistra = parametri di generazione (Campionamento,
       Contesto e memoria), destra = comportamento e infrastruttura
       (Comportamento AI, Ottimizzazione hardware, RPC Cluster). */
    auto* colsRow = new QWidget(page);
    auto* colsLay = new QHBoxLayout(colsRow);
    colsLay->setContentsMargins(0, 0, 0, 0);
    colsLay->setSpacing(16);
    auto* leftLay  = new QVBoxLayout;
    auto* rightLay = new QVBoxLayout;
    leftLay->setSpacing(12);
    rightLay->setSpacing(12);
    colsLay->addLayout(leftLay);
    colsLay->addLayout(rightLay);
    outer->addWidget(colsRow);

    /* ═══════════════════════════════════════════════
       1. CAMPIONAMENTO
       ═══════════════════════════════════════════════ */
    {
        auto* grp = new QGroupBox(tr("\xe2\x9a\x97\xef\xb8\x8f  Campionamento"));
        auto* gl  = new QVBoxLayout(grp);
        auto* fl  = new QFormLayout;
        fl->setSpacing(10);
        fl->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

        auto* tempSpin = new QDoubleSpinBox;
        tempSpin->setRange(0.0, 1.5); tempSpin->setSingleStep(0.05); tempSpin->setDecimals(2);
        tempSpin->setValue(cur.temperature);
        tempSpin->setToolTip(tr("0 = deterministico puro\n0.05 = Brutal Honesty (default)\n0.3+ = creativo"));
        fl->addRow("Temperatura:", tempSpin);

        auto* topPSpin = new QDoubleSpinBox;
        topPSpin->setRange(0.1, 1.0); topPSpin->setSingleStep(0.05); topPSpin->setDecimals(2);
        topPSpin->setValue(cur.top_p);
        topPSpin->setToolTip(tr("Nucleus sampling — 0.85 = conservativo (Brutal Honesty)"));
        fl->addRow("Top-P:", topPSpin);

        auto* topKSpin = new QSpinBox;
        topKSpin->setRange(1, 200); topKSpin->setValue(cur.top_k);
        topKSpin->setToolTip(tr("Limita ai K token pi\xc3\xb9 probabili — 20 = molto conservativo"));
        fl->addRow("Top-K:", topKSpin);

        auto* repSpin = new QDoubleSpinBox;
        repSpin->setRange(1.0, 2.0); repSpin->setSingleStep(0.05); repSpin->setDecimals(2);
        repSpin->setValue(cur.repeat_penalty);
        repSpin->setToolTip(tr("Penalizza token gi\xc3\xa0 generati — 1.20 = riduce loop"));
        fl->addRow("Penalità ripetizioni:", repSpin);

        auto* hint = new QLabel(
            tr("\xe2\x84\xb9  T 0.0\xe2\x80\x93" "0.1 = risposte fattuali e ripetibili  \xe2\x80\xa2  T 0.3+ = creativo/inventivo"));
        hint->setObjectName("hintLabel");

        gl->addLayout(fl);
        gl->addWidget(hint);
        leftLay->addWidget(CollapsibleSection::fromGroupBox(grp));

        m_tempSpin = tempSpin; m_topPSpin = topPSpin;
        m_topKSpin = topKSpin; m_repSpin  = repSpin;

        connect(tempSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ImpostazioniPage::onAiParamsSave);
        connect(topPSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ImpostazioniPage::onAiParamsSave);
        connect(topKSpin, QOverload<int>::of(&QSpinBox::valueChanged),          this, &ImpostazioniPage::onAiParamsSave);
        connect(repSpin,  QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ImpostazioniPage::onAiParamsSave);
    }

    /* ═══════════════════════════════════════════════
       2. CONTESTO E MEMORIA
       ═══════════════════════════════════════════════ */
    {
        auto* grp = new QGroupBox(tr("\xf0\x9f\x92\xac  Contesto e memoria"));
        auto* gl  = new QVBoxLayout(grp);
        auto* fl  = new QFormLayout;
        fl->setSpacing(10);
        fl->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

        auto* predSpin = new QSpinBox;
        predSpin->setRange(256, 16384); predSpin->setSingleStep(256);
        predSpin->setValue(cur.num_predict); predSpin->setSuffix("  token");
        predSpin->setToolTip(tr("Max token generati per risposta\n2048 = completo  |  512 = breve e veloce"));
        fl->addRow("Max token risposta:", predSpin);

        auto* ctxSpin = new QSpinBox;
        ctxSpin->setRange(1024, 65536); ctxSpin->setSingleStep(1024);
        ctxSpin->setValue(cur.num_ctx); ctxSpin->setSuffix("  token");
        ctxSpin->setToolTip(
            "Token mantenuti in memoria per la chat.\n\n"
            "KV-cache stimata (modello 4B):\n"
            "  2048 → ~0.7 GB   4096 → ~1.3 GB\n"
            "  8192 → ~2.7 GB  16384 → ~5.4 GB");
        fl->addRow("Finestra contesto:", ctxSpin);

        auto* ctxHint = new QLabel;
        ctxHint->setObjectName("hintLabel"); ctxHint->setWordWrap(true);
        fl->addRow("", ctxHint);

        auto updateCtxHint = [ctxHint](int ctx) {
            const double kvGb = ctx * 0.00033;
            QString icon;
            if      (kvGb < 1.0) icon = "\xe2\x9c\x85";
            else if (kvGb < 2.0) icon = "\xf0\x9f\x9f\xa1";
            else if (kvGb < 4.0) icon = "\xf0\x9f\x9f\xa0";
            else                 icon = "\xf0\x9f\x94\xb4";
            ctxHint->setText(icon + QString("  KV-cache stimata: ~%1 GB").arg(kvGb, 0, 'f', 1));
        };
        updateCtxHint(ctxSpin->value());
        QObject::connect(ctxSpin, QOverload<int>::of(&QSpinBox::valueChanged), ctxHint, updateCtxHint);

        auto* turnsSpin = new QSpinBox;
        turnsSpin->setRange(1, 20); turnsSpin->setSuffix("  turni");
        turnsSpin->setValue(qBound(1, QSettings("Prismalux","GUI")
                                          .value(P::SK::kChatMaxTurns, 3).toInt(), 20));
        turnsSpin->setToolTip(
            "Turni recenti in formato grezzo nel contesto AI.\n"
            "I turni pi\xc3\xb9 vecchi vengono compressi via AI.\n"
            "Consigliato: 3-6.");
        fl->addRow("Turni in memoria:", turnsSpin);

        auto* memHint = new QLabel(
            "\xe2\x84\xb9  I turni eccedenti vengono riassunti e iniettati come contesto compatto. "
            "Aumenta se l'AI \xe2\x80\x9c" "dimentica\xe2\x80\x9d troppo presto.");
        memHint->setObjectName("hintLabel"); memHint->setWordWrap(true);

        gl->addLayout(fl);
        gl->addWidget(memHint);
        leftLay->addWidget(CollapsibleSection::fromGroupBox(grp));

        m_predSpin = predSpin; m_ctxSpin = ctxSpin; m_ctxHint = ctxHint;

        connect(predSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &ImpostazioniPage::onAiParamsSave);
        connect(ctxSpin,  QOverload<int>::of(&QSpinBox::valueChanged), this, &ImpostazioniPage::onAiParamsSave);
        connect(turnsSpin, QOverload<int>::of(&QSpinBox::valueChanged), turnsSpin, [](int v) {
            QSettings("Prismalux","GUI").setValue(P::SK::kChatMaxTurns, qBound(1,v,20));
        });
    }

    /* ═══════════════════════════════════════════════
       3. COMPORTAMENTO AI
       ═══════════════════════════════════════════════ */
    {
        auto* grp = new QGroupBox(tr("\xf0\x9f\xa7\xa0  Comportamento AI"));
        auto* gl  = new QVBoxLayout(grp);
        gl->setSpacing(10);

        /* Brutal Honesty */
        auto* honestyCb = new QCheckBox(
            tr("\xf0\x9f\x94\x92  Prefisso Brutal Honesty (consigliato)"));
        honestyCb->setChecked(cur.honesty_prefix);
        honestyCb->setToolTip(
            "Aggiunge al system prompt: \"Se non conosci qualcosa, dillo.\n"
            "Non inventare mai fatti, numeri o citazioni.\"");
        gl->addWidget(honestyCb);

        auto* honestyHint = new QLabel(
            "\xe2\x84\xb9  Il modello risponde \xe2\x80\x9cNon lo so\xe2\x80\x9d "
            "invece di inventare. Disattiva solo per risposte pi\xc3\xb9 fluide.");
        honestyHint->setObjectName("hintLabel"); honestyHint->setWordWrap(true);
        gl->addWidget(honestyHint);

        /* Caveman */
        auto* cavemanRow = new QWidget;
        auto* cavemanLay = new QHBoxLayout(cavemanRow);
        cavemanLay->setContentsMargins(0, 4, 0, 0); cavemanLay->setSpacing(10);

        const bool cavemanOn = AiChatParams::load().caveman_mode;
        auto* cavemanToggle = new ToggleSwitch({}, this);
        cavemanToggle->setChecked(cavemanOn); cavemanToggle->setFixedHeight(dpiScale(26));

        auto* cavemanBadge = new QLabel(cavemanOn ? "  ON " : "  OFF");
        cavemanBadge->setObjectName(cavemanOn ? "badgeActive" : "badgeInactive");
        cavemanBadge->setFixedWidth(dpiScale(44)); cavemanBadge->setAlignment(Qt::AlignCenter);

        auto* cavemanLbl = new QLabel(
            tr("\xf0\x9f\xa6\x96  <b>Modalit\xc3\xa0 Caveman</b> \xe2\x80\x94 risposte dirette, zero convenevoli"));
        cavemanLay->addWidget(cavemanToggle);
        cavemanLay->addWidget(cavemanBadge);
        cavemanLay->addWidget(cavemanLbl);
        cavemanLay->addStretch();
        gl->addWidget(cavemanRow);

        auto* cavemanDesc = new QLabel(
            "\xe2\x84\xb9  Elimina \xe2\x80\x9c" "Certamente!\xe2\x80\x9d e \xe2\x80\x9cSpero di averti aiutato\xe2\x80\x9d. "
            "Meno token sprecati = risposte pi\xc3\xb9 veloci.");
        cavemanDesc->setObjectName("hintLabel"); cavemanDesc->setWordWrap(true);
        gl->addWidget(cavemanDesc);

        /* Personalità */
        auto* personaLbl = new QLabel(
            tr("\xf0\x9f\x8e\xad  <b>Personalit\xc3\xa0</b> \xe2\x80\x94 stile di risposta"));
        personaLbl->setTextFormat(Qt::RichText);
        gl->addWidget(personaLbl);

        struct PersonaDef { const char* key; const char* label; };
        static const PersonaDef kPersonas[] = {
            {"nessuna", "\xf0\x9f\x9a\xab  Nessuna"},
            {"jarvis",  "\xf0\x9f\xa4\x96  Jarvis"},
            {"kitt",    "\xf0\x9f\x9a\x97  KITT"},
            {"yoda",    "\xf0\x9f\x8c\xbf  Yoda"},
            {"snake",   "\xf0\x9f\x8e\xae  Snake"},
            {"sonic",   "\xf0\x9f\x92\xa8  Sonic"},
            {"mario",   "\xf0\x9f\x8d\x84  Mario"},
        };
        const QString curPersona = AppConfig::s().value(P::SK::kAiPersonality, "nessuna").toString();
        auto* personaRow = new QWidget;
        auto* personaLay = new QHBoxLayout(personaRow);
        personaLay->setContentsMargins(0,0,0,0); personaLay->setSpacing(6);
        QButtonGroup* grpBtns = new QButtonGroup(page);
        for (const auto& pd : kPersonas) {
            auto* btn = new QPushButton(pd.label, page);
            btn->setCheckable(true);
            btn->setChecked(QString(pd.key) == curPersona);
            btn->setObjectName("actionBtn");
            btn->setProperty("persona_key", QString(pd.key));
            grpBtns->addButton(btn);
            personaLay->addWidget(btn);
        }
        personaLay->addStretch();
        gl->addWidget(personaRow);

        rightLay->addWidget(CollapsibleSection::fromGroupBox(grp));

        m_honestyCb    = honestyCb;
        m_cavemanToggle = cavemanToggle;
        m_cavemanBadge = cavemanBadge;

        connect(honestyCb,    &QCheckBox::toggled,       this, &ImpostazioniPage::onAiParamsSave);
        connect(cavemanToggle,&QAbstractButton::toggled, this, &ImpostazioniPage::onCavemanToggled);
        connect(grpBtns,      &QButtonGroup::buttonClicked, this, &ImpostazioniPage::onPersonaBtnClicked);
    }

    /* ═══════════════════════════════════════════════
       4. OTTIMIZZAZIONE HARDWARE
       ═══════════════════════════════════════════════ */
    {
        auto* grp = new QGroupBox(tr("\xe2\x9a\xa1  Ottimizzazione hardware"));
        auto* gl  = new QVBoxLayout(grp);
        gl->setSpacing(8);

        auto* flashCb = new QCheckBox(
            tr("\xe2\x9a\xa1  Flash Attention — riduce KV-cache RAM/VRAM del 30-50%"));
        flashCb->setObjectName("cardDesc");
        flashCb->setChecked(AiChatParams::load().flash_attn);
        flashCb->setToolTip(tr("Consigliato con \xe2\x89\xa4 8 GB RAM. Ignorato se il modello non lo supporta."));
        gl->addWidget(flashCb);

        auto* mlockCb = new QCheckBox(
            tr("\xf0\x9f\x94\x92  Blocca modello in RAM (--mlock, solo llama-server)"));
        mlockCb->setObjectName("cardDesc");
        mlockCb->setChecked(AppConfig::s().value(P::SK::kMlockModel, false).toBool());
        mlockCb->setToolTip(tr("Impedisce lo swap su disco. Utile con \xe2\x89\xa5 16 GB RAM. Richiede riavvio server."));
        gl->addWidget(mlockCb);

        auto* hwHint = new QLabel(
            "\xe2\x84\xb9  Flash Attention funziona su Ollama e llama-server. "
            "mlock solo su llama-server.");
        hwHint->setObjectName("hintLabel"); hwHint->setWordWrap(true);
        gl->addWidget(hwHint);

        rightLay->addWidget(CollapsibleSection::fromGroupBox(grp));

        m_flashCb = flashCb;

        connect(flashCb, &QCheckBox::toggled, this, &ImpostazioniPage::onAiParamsSave);
        connect(mlockCb, &QCheckBox::toggled, this, &ImpostazioniPage::onMlockToggled);
    }

    /* ═══════════════════════════════════════════════
       5. RPC CLUSTER
       ═══════════════════════════════════════════════ */
    {
        auto* grp = new QGroupBox(tr("\xf0\x9f\x96\xa7  RPC Cluster — calcolo distribuito"));
        auto* gl  = new QVBoxLayout(grp);
        gl->setSpacing(8);

        auto* rpcCb = new QCheckBox(tr("Abilita RPC Cluster (--rpc, llama-server)"));
        rpcCb->setObjectName("cardDesc");
        rpcCb->setChecked(AppConfig::s().value(P::SK::kRpcEnabled, false).toBool());
        gl->addWidget(rpcCb);

        auto* fl = new QFormLayout;
        fl->setSpacing(8);
        fl->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

        auto* rpcNodesEdit = new QLineEdit;
        rpcNodesEdit->setPlaceholderText("192.168.1.10:50052,192.168.1.11:50052");
        rpcNodesEdit->setText(AppConfig::s().value(P::SK::kRpcNodes, "").toString());
        rpcNodesEdit->setObjectName("settingsInput");

        auto* rpcCheckBtn = new QPushButton(tr("\xf0\x9f\x94\x8d  Verifica nodi"));
        rpcCheckBtn->setObjectName("actionBtn");

        auto* nodesRow = new QWidget;
        auto* nodesLay = new QHBoxLayout(nodesRow);
        nodesLay->setContentsMargins(0,0,0,0); nodesLay->setSpacing(6);
        nodesLay->addWidget(rpcNodesEdit, 1);
        nodesLay->addWidget(rpcCheckBtn);
        fl->addRow("Nodi (host:porta):", nodesRow);

        auto* rpcSshUserEdit = new QLineEdit;
        rpcSshUserEdit->setPlaceholderText(qgetenv("USER"));
        rpcSshUserEdit->setText(AppConfig::s().value(P::SK::kRpcSshUser,
            QString::fromLocal8Bit(qgetenv("USER"))).toString());
        rpcSshUserEdit->setObjectName("settingsInput");
        fl->addRow("Utente SSH:", rpcSshUserEdit);

        auto* rpcPathEdit = new QLineEdit;
        rpcPathEdit->setPlaceholderText(tr("~/llama.cpp/build/bin/rpc-server"));
        rpcPathEdit->setText(AppConfig::s().value(P::SK::kRpcServerPath, "").toString());
        rpcPathEdit->setObjectName("settingsInput");
        fl->addRow("Path rpc-server:", rpcPathEdit);

        gl->addLayout(fl);

        auto* rpcStatusLbl = new QLabel;
        rpcStatusLbl->setObjectName("hintLabel"); rpcStatusLbl->setWordWrap(true);
        gl->addWidget(rpcStatusLbl);

        auto* ctrlRow = new QWidget;
        auto* ctrlLay = new QHBoxLayout(ctrlRow);
        ctrlLay->setContentsMargins(0,0,0,0); ctrlLay->setSpacing(8);
        auto* rpcStartBtn = new QPushButton(tr("\xf0\x9f\x9a\x80  Avvia nodi RPC"));
        rpcStartBtn->setObjectName("actionBtn");
        rpcStartBtn->setToolTip(tr("SSH su ogni nodo → avvia rpc-server in background"));
        auto* rpcStopBtn = new QPushButton(tr("\xe2\x8f\xb9  Ferma nodi RPC"));
        rpcStopBtn->setObjectName("actionBtn");
        rpcStopBtn->setToolTip(tr("SSH su ogni nodo → pkill rpc-server"));
        ctrlLay->addWidget(rpcStartBtn); ctrlLay->addWidget(rpcStopBtn); ctrlLay->addStretch();
        gl->addWidget(ctrlRow);

        auto* rpcDesc = new QLabel(
            "\xe2\x84\xb9  Richiede SSH senza password e llama.cpp compilato con "
            "<b>-DGGML_RPC=ON</b> su ogni nodo remoto.");
        rpcDesc->setObjectName("hintLabel"); rpcDesc->setWordWrap(true);
        rpcDesc->setTextFormat(Qt::RichText);
        gl->addWidget(rpcDesc);

        rightLay->addWidget(CollapsibleSection::fromGroupBox(grp));

        m_rpcCb = rpcCb; m_rpcNodesEdit = rpcNodesEdit; m_rpcStatusLbl = rpcStatusLbl;
        m_rpcSshUserEdit = rpcSshUserEdit; m_rpcPathEdit = rpcPathEdit;
        m_rpcStartBtn = rpcStartBtn; m_rpcStopBtn = rpcStopBtn;

        connect(rpcCb,          &QCheckBox::toggled,         this, &ImpostazioniPage::onRpcToggled);
        connect(rpcNodesEdit,   &QLineEdit::editingFinished,  this, &ImpostazioniPage::onRpcNodesEditFinished);
        connect(rpcCheckBtn,    &QPushButton::clicked,        this, &ImpostazioniPage::onRpcCheckClicked);
        connect(rpcSshUserEdit, &QLineEdit::editingFinished,  this, &ImpostazioniPage::onRpcSshUserEditFinished);
        connect(rpcPathEdit,    &QLineEdit::editingFinished,  this, &ImpostazioniPage::onRpcPathEditFinished);
        connect(rpcStartBtn,    &QPushButton::clicked,        this, &ImpostazioniPage::onRpcStartNodesClicked);
        connect(rpcStopBtn,     &QPushButton::clicked,        this, &ImpostazioniPage::onRpcStopNodesClicked);
    }

    leftLay->addStretch();
    rightLay->addStretch();

    /* ── Preset + Salva/Reset ── */
    {
        auto* presetRow = new QWidget;
        auto* presetLay = new QHBoxLayout(presetRow);
        presetLay->setContentsMargins(0, 4, 0, 0); presetLay->setSpacing(8);

        auto* presetLbl = new QLabel(tr("\xf0\x9f\x8e\x9b  Preset rapidi:"));
        presetLbl->setObjectName("cardDesc");
        presetLay->addWidget(presetLbl);

        auto* preset8gb = new QPushButton(tr("8 GB RAM"));
        preset8gb->setObjectName("actionBtn");
        preset8gb->setToolTip(tr("num_ctx=4096  num_predict=1024  T=0.1  Flash Attention ON"));
        presetLay->addWidget(preset8gb);

        auto* presetLong = new QPushButton(tr("\xf0\x9f\x93\x9c  Contesto lungo"));
        presetLong->setObjectName("actionBtn");
        presetLong->setToolTip(tr("num_ctx=16384  Flash Attention ON\nRichiede \xe2\x89\xa5 16 GB RAM"));
        presetLay->addWidget(presetLong);
        presetLay->addStretch();
        outer->addWidget(presetRow);

        auto* btnRow = new QWidget;
        auto* btnLay = new QHBoxLayout(btnRow);
        btnLay->setContentsMargins(0, 0, 0, 0); btnLay->setSpacing(8);

        auto* resetBtn = new QPushButton(tr("\xf0\x9f\x94\x84  Ripristina default"));
        resetBtn->setObjectName("actionBtn");
        resetBtn->setToolTip(tr("Ripristina i valori ottimali anti-allucinazione"));

        auto* saveBtn = new QPushButton(tr("\xe2\x9c\x85  Salva"));
        saveBtn->setObjectName("actionBtn");

        auto* saveStatus = new QLabel;
        saveStatus->setObjectName("hintLabel");

        btnLay->addWidget(resetBtn);
        btnLay->addStretch();
        btnLay->addWidget(saveStatus);
        btnLay->addWidget(saveBtn);
        outer->addWidget(btnRow);

        m_saveStatus = saveStatus;

        connect(saveBtn,    &QPushButton::clicked, this, &ImpostazioniPage::onAiParamsSave);
        connect(resetBtn,   &QPushButton::clicked, this, &ImpostazioniPage::onAiParamsReset);
        connect(preset8gb,  &QPushButton::clicked, this, &ImpostazioniPage::onAiPreset8GbClicked);
        connect(presetLong, &QPushButton::clicked, this, &ImpostazioniPage::onAiPresetLongClicked);
    }

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
        tr("\xf0\x9f\x90\xb3  Sandbox Docker \xe2\x80\x94 Esecuzione Codice Isolata"), page);
    titleLbl->setObjectName("sectionTitle");
    lay->addWidget(titleLbl);

    /* ── Stato Docker ── */
    auto* statusCard = new QFrame(page);
    statusCard->setObjectName("cardFrame");
    auto* statusLay = new QVBoxLayout(statusCard);
    statusLay->setContentsMargins(12, 10, 12, 10);

    auto* statusRow = new QHBoxLayout;
    m_dockerStatusIcon = new QLabel(page);
    m_dockerStatusDesc = new QLabel(page);
    m_dockerStatusDesc->setObjectName("cardDesc");
    m_dockerStatusDesc->setWordWrap(true);
    statusRow->addWidget(m_dockerStatusIcon);
    statusRow->addWidget(m_dockerStatusDesc, 1);
    statusLay->addLayout(statusRow);

    m_dockerUnlockBtn = new QPushButton(
        tr("\xf0\x9f\x94\x93  Sblocca e avvia Docker"), page);
    m_dockerUnlockBtn->setObjectName("actionBtn");
    m_dockerUnlockBtn->setToolTip(
        "Esegue (con autorizzazione amministratore):\n"
        "systemctl unmask docker.service docker.socket\n"
        "systemctl start docker.service");
    m_dockerUnlockBtn->hide();
    connect(m_dockerUnlockBtn, &QPushButton::clicked,
            this, &ImpostazioniPage::onDockerUnlockClicked);
    statusLay->addWidget(m_dockerUnlockBtn);

    refreshDockerStatusCard();
    const QString docker = P::findDocker();
    lay->addWidget(statusCard);

    /* ── Toggle sandbox abilitato ── */
    {
        auto* row = new QHBoxLayout;
        auto* chk = new QCheckBox(
            tr("Usa sandbox Docker per il codice generato dall\xe2\x80\x99" "AI"), page);
        chk->setObjectName("settingsCheck");
        chk->setChecked(AppConfig::s().value(P::SK::kSandboxEnabled, true).toBool());
        chk->setEnabled(!docker.isEmpty());
        chk->setToolTip(tr("Se disabilitato, il codice AI viene eseguito localmente con pip install retry.\n"
                        "Con sandbox: rete disabilitata, nessun accesso al filesystem host."));
        connect(chk,  &QCheckBox::toggled,
                this, &ImpostazioniPage::onSandboxEnabledToggled);
        row->addWidget(chk);
        row->addStretch();
        lay->addLayout(row);
    }

    /* ── Immagine Docker ── */
    {
        auto* grp = new QGroupBox(tr("\xe2\x9a\x99\xef\xb8\x8f  Configurazione container"), page);
        grp->setObjectName("cardGroup");
        auto* grpLay = new QFormLayout(grp);
        grpLay->setSpacing(10);

        auto* imgEdit = new QLineEdit(page);
        imgEdit->setObjectName("settingsEdit");
        imgEdit->setPlaceholderText(tr("python:3.11-slim"));
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
        memSpin->setToolTip(tr("Limite RAM del container Docker.\n"
                            "256 MB \xc3\xa8 sufficiente per la maggior parte dei task.\n"
                            "Aumenta a 512+ per elaborazioni numeriche intensive."));
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
            tr("\xf0\x9f\x93\xa5  Scarica immagine ora (docker pull)"), page);
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
            pullStatus->setText(tr("\xf0\x9f\x94\x84  Pull in corso..."));
            auto* proc = new QProcess(pullBtn->window());
            proc->setProcessChannelMode(QProcess::MergedChannels);
            connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                    pullBtn, [proc, pullBtn, pullStatus, img](int rc, QProcess::ExitStatus){
                const QString out = QString::fromUtf8(proc->readAll()).trimmed();
                proc->deleteLater();
                pullBtn->setEnabled(true);
                if (rc == 0) {
                    pullBtn->setVisible(false);
                    pullStatus->setText(
                        QString("\xe2\x9c\x85  %1 scaricata con successo.").arg(img));
                } else {
                    pullStatus->setText(
                        tr("\xe2\x9d\x8c  Errore: %1").arg(out.left(120)));
                    LogBus::post("\xe2\x9d\x8c Sandbox: docker pull fallito: " + out.left(120));
                }
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
            tr("\xf0\x9f\x94\x92  <b>Isolamento garantito dal container:</b><br>"
            "\xe2\x80\xa2 <b>Rete disabilitata</b> — nessuna connessione a internet<br>"
            "\xe2\x80\xa2 <b>Nessun volume</b> — il codice non pu\xc3\xb2 leggere/scrivere file locali<br>"
            "\xe2\x80\xa2 <b>Limite processi</b> — max 64 PID (anti fork-bomb)<br>"
            "\xe2\x80\xa2 <b>Container effimero</b> — distrutto automaticamente al termine (--rm)<br>"
            "\xe2\x80\xa2 <b>pip install non disponibile</b> con sandbox attivo (rete assente)"));
        info->setTextFormat(Qt::RichText);
        lay->addWidget(info);
    }

    lay->addStretch();
    return page;
}

void ImpostazioniPage::onSmartRouterSaveClicked()
{
    QSettings s2;
    s2.setValue(P::SK::kSmartRouterEnabled, m_smartRouterChk->isChecked());
    s2.setValue(P::SK::kCloudApiUrl,        m_cloudUrlEdit->text().trimmed());
    s2.setValue(P::SK::kCloudApiModel,      m_cloudModelEdit->text().trimmed());
    /* API key via QKeychain (o file 0600 fallback), NON in QSettings */
    LanServer::saveSecret(QStringLiteral("cloud_api_key"),
                          m_cloudApiKeyEdit->text().trimmed());
    m_ai->setSmartRouter(
        m_smartRouterChk->isChecked(),
        m_cloudUrlEdit->text().trimmed(),
        m_cloudModelEdit->text().trimmed(),
        m_cloudApiKeyEdit->text().trimmed());
    m_smartRouterStatusLbl->setText(tr("\xe2\x9c\x85  Salvato"));
    QTimer::singleShot(2000, m_smartRouterStatusLbl,
                       [this]{ m_smartRouterStatusLbl->setText(""); });
}



/* ══════════════════════════════════════════════════════════════
   eventFilter — middle-click sulla lista modelli (Gestione LLM):
   copia il nome del modello (Ollama) o il percorso GGUF (llama-server)
   negli appunti. Su X11/Wayland scrive anche la selezione primaria,
   così si incolla direttamente col tasto centrale in un terminale.
   ══════════════════════════════════════════════════════════════ */
bool ImpostazioniPage::eventFilter(QObject* watched, QEvent* ev)
{
    if (m_aiModelList && watched == m_aiModelList->viewport()
        && ev->type() == QEvent::MouseButtonRelease) {
        auto* me = static_cast<QMouseEvent*>(ev);
        if (me->button() == Qt::MiddleButton) {
            QListWidgetItem* item = m_aiModelList->itemAt(me->position().toPoint());
            if (item) {
                const QString name = item->data(Qt::UserRole).toString();
                /* Placeholder "(Ollama non raggiungibile)" ecc. non hanno UserRole */
                if (!name.isEmpty()) {
                    QClipboard* cb = QGuiApplication::clipboard();
                    cb->setText(name);
                    if (cb->supportsSelection())
                        cb->setText(name, QClipboard::Selection);
                    QToolTip::showText(me->globalPosition().toPoint(),
                                       QString::fromUtf8("\xf0\x9f\x93\x8b Copiato: %1")
                                           .arg(name),
                                       m_aiModelList);
                }
            }
            return true;   /* consumato: niente altri effetti dal middle-click */
        }
    }
    return QWidget::eventFilter(watched, ev);
}
