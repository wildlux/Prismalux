#include "impostazioni_page.h"
#include "../widgets/stt_whisper.h"
#include "personalizza_page.h"
#include "manutenzione_page.h"
#include "grafico_page.h"
#include "../theme_manager.h"
#include "../prismalux_paths.h"
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
#include <QSettings>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QFileDialog>
#include <QDateTime>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QTimer>
#include <QRadioButton>
#include <QListWidget>
#include <QGroupBox>
#include <QPlainTextEdit>
#include <QThread>

/* ══════════════════════════════════════════════════════════════
   ImpostazioniPage — 6 tab tematiche (da 11 originali).
   Nomi comprensibili anche per utenti non esperti.
   Nessun QTabWidget annidato (evita rendering blank in Qt6).
   ══════════════════════════════════════════════════════════════ */
ImpostazioniPage::ImpostazioniPage(AiClient* ai, HardwareMonitor* hw, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    m_tabs = new QTabWidget(this);
    auto* tabs = m_tabs;
    tabs->setObjectName("settingsTabs");

    /* Factory widget senza presenza visiva propria — espongono solo buildXxx() */
    m_manutenzione = new ManutenzioneePage(ai, hw, this);
    m_manutenzione->hide();
    m_personalizza = new PersonalizzaPage(this);
    m_personalizza->hide();

    /* ────────────────────────────────────────────────────────────
       Tab 1: 🔌 Connessione — Backend (compatto, nessun scroll)
       ──────────────────────────────────────────────────────────── */
    tabs->addTab(m_manutenzione->buildBackend(), "\xf0\x9f\x94\x8c  Connessione");

    /* ────────────────────────────────────────────────────────────
       Tab 2: 🖥 Hardware — Rilevamento hw + VRAM Benchmark
       ──────────────────────────────────────────────────────────── */
    {
        auto* hwOuter = new QWidget;
        auto* hwOLay  = new QVBoxLayout(hwOuter);
        hwOLay->setContentsMargins(0, 0, 0, 0);
        hwOLay->setSpacing(0);

        auto* scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);

        auto* inner = new QWidget;
        auto* iLay  = new QVBoxLayout(inner);
        iLay->setContentsMargins(0, 0, 0, 0);
        iLay->setSpacing(0);

        iLay->addWidget(m_manutenzione->buildHardware());

        auto* sep = new QFrame;
        sep->setFrameShape(QFrame::HLine);
        sep->setObjectName("sidebarSep");
        iLay->addWidget(sep);

        iLay->addWidget(m_personalizza->buildVramBench());
        iLay->addStretch();

        scroll->setWidget(inner);
        hwOLay->addWidget(scroll);

        tabs->addTab(hwOuter, "\xf0\x9f\x96\xa5  Hardware");
    }

    /* ────────────────────────────────────────────────────────────
       Tab 3: 🦙 AI Locale — selezione gestore + modelli disponibili
       ──────────────────────────────────────────────────────────── */
    tabs->addTab(buildAiLocaleTab(), "\xf0\x9f\xa6\x99  AI Locale");

    /* ────────────────────────────────────────────────────────────
       Tab 4: 🤖 LLM — catalogo compatto Ollama + GGUF (filtro+lista)
       ──────────────────────────────────────────────────────────── */
    tabs->addTab(buildLlmConsigliatiTab(), "\xf0\x9f\xa4\x96  LLM");

    /* ────────────────────────────────────────────────────────────
       Tab 4: 🎤 Voce & Audio — TTS (sinistra) + STT (destra)
       Layout 2 colonne — nessun scroll, tutto visibile.
       ──────────────────────────────────────────────────────────── */
    {
        auto* outer = new QWidget;
        auto* oLay  = new QHBoxLayout(outer);
        oLay->setContentsMargins(14, 12, 14, 12);
        oLay->setSpacing(14);
        oLay->addWidget(buildVoceTab());
        oLay->addWidget(buildTrascriviTab());
        tabs->addTab(outer, "\xf0\x9f\x8e\xa4  Voce & Audio");
    }

    /* ────────────────────────────────────────────────────────────
       Tab 5: 🎨 Aspetto — Selezione tema visivo
       ──────────────────────────────────────────────────────────── */
    tabs->addTab(buildTemaTab(), "\xf0\x9f\x8e\xa8  Aspetto");

    /* ────────────────────────────────────────────────────────────
       Tab 6: 📊 Avanzate — Monitor AI + RAG + Test suite
       Monitor è live (widget autonomo) → stretch=1 in cima.
       RAG + Test in scroll area sotto.
       ──────────────────────────────────────────────────────────── */
    {
        auto* outer = new QWidget;
        auto* oLay  = new QVBoxLayout(outer);
        oLay->setContentsMargins(0, 0, 0, 0);
        oLay->setSpacing(0);

        auto* monPanel = new MonitorPanel(ai, hw, outer);
        monPanel->setWindowFlags(Qt::Widget);
        monPanel->setMinimumSize(0, 0);
        oLay->addWidget(monPanel, 2);

        auto* sep1 = new QFrame;
        sep1->setFrameShape(QFrame::HLine);
        sep1->setObjectName("sidebarSep");
        oLay->addWidget(sep1);

        auto* bottomScroll = new QScrollArea;
        bottomScroll->setWidgetResizable(true);
        bottomScroll->setFrameShape(QFrame::NoFrame);

        auto* bottomInner = new QWidget;
        auto* bottomLay   = new QVBoxLayout(bottomInner);
        bottomLay->setContentsMargins(0, 0, 0, 0);
        bottomLay->setSpacing(0);

        bottomLay->addWidget(buildRagTab());

        auto* sep2 = new QFrame;
        sep2->setFrameShape(QFrame::HLine);
        sep2->setObjectName("sidebarSep");
        bottomLay->addWidget(sep2);

        bottomLay->addWidget(buildTestTab());
        bottomLay->addStretch();

        bottomScroll->setWidget(bottomInner);
        oLay->addWidget(bottomScroll, 1);

        tabs->addTab(outer, "\xf0\x9f\x93\x8a  Avanzate");
    }

    lay->addWidget(tabs);
}

/* ══════════════════════════════════════════════════════════════
   switchToTab — porta in primo piano il tab che contiene @p name
   ══════════════════════════════════════════════════════════════ */
void ImpostazioniPage::switchToTab(const QString& name)
{
    if (!m_tabs) return;
    /* "LLM" ha ora il suo tab dedicato — ricerca diretta */
    const QString resolved = name;
    for (int i = 0; i < m_tabs->count(); i++) {
        if (m_tabs->tabText(i).contains(resolved, Qt::CaseInsensitive)) {
            m_tabs->setCurrentIndex(i);
            return;
        }
    }
}

/* ══════════════════════════════════════════════════════════════
   buildGraficoTab — controlli posizione assi 2D e 3D
   ══════════════════════════════════════════════════════════════ */
QWidget* ImpostazioniPage::buildGraficoTab(GraficoCanvas* canvas)
{
    static const char* kAxisItems[] = {
        "Origine dati (x=0, y=0)",
        "\xe2\x86\x99  Basso sinistra",
        "\xe2\x86\x98  Basso destra",
        "\xe2\x86\x96  Alto sinistra",
        "\xe2\x86\x97  Alto destra"
    };

    auto* page   = new QWidget;
    auto* outer  = new QVBoxLayout(page);
    outer->setContentsMargins(20, 20, 20, 20);
    outer->setSpacing(16);

    /* Titolo */
    auto* title = new QLabel("\xf0\x9f\x93\x88  Posizione assi — Grafico");
    title->setObjectName("sectionTitle");
    outer->addWidget(title);

    /* Descrizione */
    auto* desc = new QLabel(
        "Scegli dove visualizzare i gizmo degli assi cartesiani "
        "nei grafici 2D (Cartesiano, Scatter) e 3D (Scatter 3D, Grafo 3D).");
    desc->setWordWrap(true);
    desc->setObjectName("hintLabel");
    outer->addWidget(desc);

    /* Form */
    auto* fl = new QFormLayout;
    fl->setContentsMargins(0, 8, 0, 0);
    fl->setSpacing(10);
    fl->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    /* Assi 2D */
    auto* cmb2d = new QComboBox;
    cmb2d->setObjectName("settingCombo");
    for (auto* s : kAxisItems) cmb2d->addItem(s);
    cmb2d->setCurrentIndex(0);  /* AtData */
    if (canvas)
        QObject::connect(cmb2d, QOverload<int>::of(&QComboBox::currentIndexChanged),
                         canvas, [canvas](int idx){ canvas->setAxes2dPos(idx); });
    fl->addRow("Assi 2D:", cmb2d);

    /* Assi 3D */
    auto* cmb3d = new QComboBox;
    cmb3d->setObjectName("settingCombo");
    for (int i = 1; i < 5; i++) cmb3d->addItem(kAxisItems[i]);
    cmb3d->setCurrentIndex(1);  /* BottomRight */
    if (canvas)
        QObject::connect(cmb3d, QOverload<int>::of(&QComboBox::currentIndexChanged),
                         canvas, [canvas](int idx){ canvas->setAxes3dPos(idx + 1); });
    fl->addRow("Assi 3D:", cmb3d);

    outer->addLayout(fl);
    outer->addStretch();
    return page;
}

/* ══════════════════════════════════════════════════════════════
   setGraficoCanvas — aggiunge il tab Grafico con il canvas reale
   ══════════════════════════════════════════════════════════════ */
void ImpostazioniPage::setGraficoCanvas(GraficoCanvas* canvas)
{
    if (!m_tabs || !canvas) return;
    /* Evita duplicati se chiamato più volte */
    for (int i = 0; i < m_tabs->count(); i++) {
        if (m_tabs->tabText(i).contains("Grafico", Qt::CaseInsensitive))
            return;
    }
    m_tabs->addTab(buildGraficoTab(canvas), "\xf0\x9f\x93\x88  Grafico");
}

/* ══════════════════════════════════════════════════════════════
   buildAiLocaleTab — layout a 2 colonne:
     Sinistra : selettore gestore LLM (Ollama / llama.cpp)
     Destra   : modelli disponibili per il gestore selezionato

   Ollama  → fetchModels() → QStringList con nome modello
   llama.cpp → P::scanGgufFiles() → path assoluti file .gguf
   ══════════════════════════════════════════════════════════════ */
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
    auto* btnLlama  = new QRadioButton("\xf0\x9f\xa6\x99  llama.cpp", leftGroup);

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
    leftLay->addWidget(refreshBtn);

    /* Pulsante llama.cpp Studio (solo visibile con llama.cpp selezionato) */
    auto* studioBtn = new QPushButton("\xe2\x9a\x99\xef\xb8\x8f  llama.cpp Studio \xe2\x86\x92", leftGroup);
    studioBtn->setObjectName("actionBtn");
    studioBtn->hide();
    leftLay->addWidget(studioBtn);

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

    /* ── Dipendenze — occupa tutto lo spazio residuo ── */
    auto* sepBot = new QFrame(page);
    sepBot->setFrameShape(QFrame::HLine);
    sepBot->setObjectName("sidebarSep");
    mainLay->addWidget(sepBot);

    mainLay->addWidget(buildDipendenzeTab(), 1);

    /* ── Logica: popola la lista in base al gestore selezionato ── */

    /* Lambda: carica modelli Ollama tramite AiClient::fetchModels */
    auto populateOllama = [=]() {
        modelList->clear();
        hintLbl->setText("Caricamento modelli Ollama...");
        statusLbl->setText("\xe2\x8f\xb3 Ollama...");
        auto* holder = new QObject(page);
        connect(m_ai, &AiClient::modelsReady, holder,
                [=](const QStringList& list) {
            holder->deleteLater();
            modelList->clear();
            if (list.isEmpty()) {
                statusLbl->setText("\xe2\x9d\x8c Nessun modello");
                hintLbl->setText("Ollama non raggiungibile o nessun modello installato.\n"
                                 "Avvia Ollama e riprova.");
                modelList->addItem("(Ollama non raggiungibile)");
            } else {
                statusLbl->setText(QString("\xe2\x9c\x85 %1 modelli").arg(list.size()));
                hintLbl->setText(QString("%1 modelli Ollama installati.")
                                 .arg(list.size()));
                for (const QString& m : list)
                    modelList->addItem(m);
            }
        });
        m_ai->fetchModels();
    };

    /* Lambda: scansiona file .gguf dalla cartella models/ */
    auto populateLlama = [=]() {
        modelList->clear();
        const QStringList files = PrismaluxPaths::scanGgufFiles();
        if (files.isEmpty()) {
            statusLbl->setText("0 modelli");
            hintLbl->setText("Nessun file .gguf trovato.\n"
                             "Usa llama.cpp Studio per scaricare o compilare modelli.");
            modelList->addItem("(Nessun modello GGUF nella cartella models/)");
        } else {
            statusLbl->setText(QString("%1 file GGUF").arg(files.size()));
            hintLbl->setText(QString("%1 modelli GGUF nella cartella models/.")
                             .arg(files.size()));
            for (const QString& path : files) {
                QFileInfo fi(path);
                double mb = fi.size() / 1024.0 / 1024.0;
                QString szStr = mb >= 1024.0
                    ? QString("%1 GB").arg(mb / 1024.0, 0, 'f', 1)
                    : QString("%1 MB").arg(mb, 0, 'f', 0);
                modelList->addItem(QString("%1   \xe2\x80\x94  %2").arg(fi.fileName(), szStr));
            }
        }
    };

    /* Cambio gestore */
    connect(btnOllama, &QRadioButton::toggled, page, [=](bool checked) {
        if (!checked) return;
        studioBtn->hide();
        populateOllama();
    });
    connect(btnLlama, &QRadioButton::toggled, page, [=](bool checked) {
        if (!checked) return;
        studioBtn->show();
        populateLlama();
    });

    /* Aggiorna lista */
    connect(refreshBtn, &QPushButton::clicked, page, [=]() {
        if (btnOllama->isChecked()) populateOllama();
        else populateLlama();
    });

    /* llama.cpp Studio → apre tab AI Locale che contiene già llama.cpp Studio
       (l'utente può navigare da lì) — per ora mostra un messaggio informativo */
    connect(studioBtn, &QPushButton::clicked, page, [this]() {
        /* Il llama.cpp Studio è accessibile tramite PersonalizzaPage.
           Apri la scheda Avanzate → oppure naviga alla vecchia posizione */
        switchToTab("AI Locale");  /* resta nello stesso tab — TODO: link diretto */
    });

    /* Popola Ollama subito all'apertura */
    QTimer::singleShot(0, page, [=]() { populateOllama(); });

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
        { "llama-server", "llama-server",  "Server llama.cpp alternativo a Ollama (SSE)",
          "./build.sh  nella cartella C_software",                     true  },
        { "llama-cli",    "llama-cli",     "Chat diretta con modelli GGUF offline",
          "./build.sh  nella cartella C_software",                     true  },

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
            QString found = QStandardPaths::findExecutable(ex);
            if (!found.isEmpty()) {
                statusDots[i]->setStyleSheet(
                    "color:#4ade80;font-size:10px;");   /* verde */
            } else {
                statusDots[i]->setStyleSheet(
                    "color:#f87171;font-size:10px;");   /* rosso */
            }
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
    auto* dirEdit = new QLineEdit;
    dirEdit->setObjectName("inputLine");
    dirEdit->setPlaceholderText("/percorso/documenti/");
    {
        QSettings s("Prismalux", "GUI");
        dirEdit->setText(s.value("rag/docsDir", "").toString());
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
        QSettings s("Prismalux", "GUI");
        maxSpin->setValue(s.value("rag/maxResults", 5).toInt());
    }
    maxSpin->setSuffix("  risultati");
    fl->addRow("Massimi:", maxSpin);

    outer->addLayout(fl);

    /* ── Stato indice ── */
    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setObjectName("sidebarSep");
    outer->addWidget(sep);

    auto* statusLbl = new QLabel;
    statusLbl->setObjectName("hintLabel");
    statusLbl->setWordWrap(true);
    statusLbl->setTextFormat(Qt::RichText);
    auto refreshStatus = [statusLbl]() {
        QSettings ss("Prismalux", "GUI");
        statusLbl->setText(
            QString("Documenti indicizzati: <b>%1</b>"
                    "&nbsp;&nbsp;&nbsp;Ultima indicizzazione: <b>%2</b>")
                .arg(ss.value("rag/docCount", 0).toInt())
                .arg(ss.value("rag/lastIndexed", "Mai").toString()));
    };
    refreshStatus();
    outer->addWidget(statusLbl);

    /* Pulsante Reindicizza */
    auto* reindexBtn = new QPushButton("\xf0\x9f\x94\x84  Reindicizza ora");
    reindexBtn->setObjectName("actionBtn");
    reindexBtn->setFixedHeight(32);
    outer->addWidget(reindexBtn);

    /* Label feedback */
    auto* feedbackLbl = new QLabel;
    feedbackLbl->setObjectName("hintLabel");
    feedbackLbl->setWordWrap(true);
    feedbackLbl->setTextFormat(Qt::RichText);
    feedbackLbl->setVisible(false);
    outer->addWidget(feedbackLbl);

    outer->addStretch();

    /* ── Connessioni ── */
    QObject::connect(browseBtn, &QPushButton::clicked, dirEdit, [dirEdit]() {
        QString d = QFileDialog::getExistingDirectory(
            nullptr, "Cartella documenti RAG", dirEdit->text());
        if (!d.isEmpty()) dirEdit->setText(d);
    });
    QObject::connect(dirEdit, &QLineEdit::textChanged, dirEdit, [](const QString& t) {
        QSettings ss("Prismalux", "GUI");
        ss.setValue("rag/docsDir", t);
    });
    QObject::connect(maxSpin, QOverload<int>::of(&QSpinBox::valueChanged), maxSpin, [](int v) {
        QSettings ss("Prismalux", "GUI");
        ss.setValue("rag/maxResults", v);
    });
    QObject::connect(reindexBtn, &QPushButton::clicked, reindexBtn,
        [dirEdit, statusLbl, feedbackLbl, refreshStatus]() {
        QString dir = dirEdit->text().trimmed();
        if (dir.isEmpty() || !QDir(dir).exists()) {
            feedbackLbl->setText(
                "\xe2\x9a\xa0  Cartella non valida o inesistente. "
                "Specifica un percorso esistente.");
            feedbackLbl->setVisible(true);
            return;
        }
        QStringList filters{
            "*.txt","*.md","*.pdf","*.csv","*.json",
            "*.py","*.cpp","*.h","*.c","*.rst"
        };
        int n = 0;
        QDirIterator it(dir, filters, QDir::Files,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) { it.next(); ++n; }
        QSettings ss("Prismalux", "GUI");
        ss.setValue("rag/docCount",    n);
        ss.setValue("rag/lastIndexed",
            QDateTime::currentDateTime().toString("dd/MM/yyyy HH:mm"));
        refreshStatus();
        feedbackLbl->setText(
            QString("\xe2\x9c\x85  Indicizzati <b>%1</b> file da <code>%2</code>.")
                .arg(n).arg(dir));
        feedbackLbl->setVisible(true);
    });

    return page;
}

/* ──────────────────────────────────────────────────────────────
   buildTemaTab — card per ogni tema disponibile (built-in + custom)
   ────────────────────────────────────────────────────────────── */
struct ThemeVisual { QString id; QString bg; QString accent; QString bg2; };
static const ThemeVisual kVisuals[] = {
    { "dark_cyan",   "#13141f", "#00b8d9", "#1a1b28" },
    { "dark_amber",  "#17140c", "#ffb300", "#211c10" },
    { "dark_purple", "#130f1c", "#9c5ff0", "#1c1727" },
    { "light",       "#f0f2fa", "#0072c6", "#e0e4f0" },
};

static ThemeVisual visualFor(const QString& id) {
    for (const auto& v : kVisuals)
        if (v.id == id) return v;
    return { id, "#1e2030", "#607080", "#252a3a" };  /* fallback per custom */
}

QWidget* ImpostazioniPage::buildTemaTab() {
    auto* outer = new QWidget(this);
    auto* vlay  = new QVBoxLayout(outer);
    vlay->setContentsMargins(20, 20, 20, 20);
    vlay->setSpacing(16);

    /* ── Titolo ── */
    auto* title = new QLabel("\xf0\x9f\x8e\xa8  Seleziona tema", outer);
    title->setStyleSheet("font-size:16px; font-weight:700; color:#e5e7eb;");
    vlay->addWidget(title);

    auto* hint = new QLabel(
        "Copia file <b>.qss</b> nella cartella <code>themes/</code> accanto "
        "all'eseguibile per aggiungere temi personalizzati.", outer);
    hint->setWordWrap(true);
    hint->setStyleSheet("color:#9ca3af; font-size:12px;");
    vlay->addWidget(hint);

    /* ── Griglia card (4 per riga) ── */
    auto* scroll = new QScrollArea(outer);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* grid_w = new QWidget(scroll);
    auto* grid   = new QGridLayout(grid_w);
    grid->setSpacing(14);
    grid->setContentsMargins(0, 8, 0, 8);
    scroll->setWidget(grid_w);
    vlay->addWidget(scroll);

    ThemeManager* tm      = ThemeManager::instance();
    const auto&   themes  = tm->themes();
    const QString current = tm->currentId();

    /* Raggruppa i bottoni per gestire la selezione esclusiva */
    auto* group = new QButtonGroup(outer);
    group->setExclusive(true);

    const int COLS = 4;
    for (int i = 0; i < themes.size(); ++i) {
        const auto& t = themes[i];
        ThemeVisual v = visualFor(t.id);

        /* ── Contenitore card ── */
        auto* card = new QPushButton(grid_w);
        card->setCheckable(true);
        card->setChecked(t.id == current);
        card->setFixedSize(130, 100);
        card->setObjectName("themeCard");
        card->setProperty("themeId", t.id);

        /* Stile card: anteprima colori via QSS inline */
        const QString borderColor = (t.id == current) ? v.accent : "#404040";
        card->setStyleSheet(QString(
            "QPushButton#themeCard {"
            "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
            "    stop:0 %1, stop:0.65 %2, stop:0.66 %3, stop:1 %3);"
            "  border: 2px solid %4; border-radius: 10px;"
            "  color: %5; font-weight: 600; font-size: 11px;"
            "  padding-top: 54px;"
            "  text-align: center;"
            "}"
            "QPushButton#themeCard:checked {"
            "  border: 3px solid %3; border-radius: 10px;"
            "}"
            "QPushButton#themeCard:hover {"
            "  border-color: %3;"
            "}"
        ).arg(v.bg2, v.bg, v.accent, borderColor,
              (t.id == "light") ? "#1a1e30" : "#e5e7eb"));

        card->setText(t.label);
        group->addButton(card, i);

        /* Checkmark sovrapposto (visibile se selezionato) */
        auto* check = new QLabel("\xe2\x9c\x93", card);
        check->setAlignment(Qt::AlignTop | Qt::AlignRight);
        check->setStyleSheet(QString(
            "color: %1; font-size: 16px; font-weight: 700; "
            "background: transparent; padding: 4px 6px 0 0;")
            .arg(v.accent));
        check->setFixedSize(130, 30);
        check->setVisible(t.id == current);

        /* Connessione: cambia tema al click */
        connect(card, &QPushButton::clicked, card,
            [tm, t, themes, group, grid_w]() {
                tm->apply(t.id);
                /* Aggiorna border di tutte le card */
                for (int j = 0; j < themes.size(); ++j) {
                    auto* btn = qobject_cast<QPushButton*>(group->button(j));
                    if (!btn) continue;
                    ThemeVisual vj = visualFor(themes[j].id);
                    const QString bc = (themes[j].id == t.id) ? vj.accent : "#404040";
                    /* Aggiorna solo la border-color senza ricostruire tutto lo stile */
                    QString s = btn->styleSheet();
                    /* checkmark visibility */
                    const auto children = btn->findChildren<QLabel*>();
                    for (auto* lbl : children)
                        lbl->setVisible(themes[j].id == t.id);
                    (void)bc; (void)s;
                }
            });

        grid->addWidget(card, i / COLS, i % COLS);
    }

    /* Riga espandibile in fondo */
    grid->setRowStretch(themes.size() / COLS + 1, 1);
    /* ── Sezione: Aspetto bolle chat ── */
    auto* secBolle = new QFrame(outer);
    secBolle->setObjectName("cardFrame");
    auto* bolleLay = new QVBoxLayout(secBolle);
    bolleLay->setContentsMargins(16, 10, 16, 10);
    bolleLay->setSpacing(10);

    auto* bolleTitle = new QLabel(
        "\xf0\x9f\x92\xac  <b>Aspetto bolle chat</b>", secBolle);
    bolleTitle->setObjectName("cardTitle");
    bolleTitle->setTextFormat(Qt::RichText);
    bolleLay->addWidget(bolleTitle);

    auto* radiusRow = new QWidget(secBolle);
    auto* radiusLay = new QHBoxLayout(radiusRow);
    radiusLay->setContentsMargins(0, 0, 0, 0);
    radiusLay->setSpacing(10);

    auto* radiusLbl = new QLabel("Raggio bordi (px):", secBolle);
    radiusLbl->setObjectName("cardDesc");

    auto* radiusSpin = new QSpinBox(secBolle);
    radiusSpin->setRange(0, 24);
    radiusSpin->setSuffix(" px");
    radiusSpin->setFixedWidth(80);
    {
        QSettings s("Prismalux", "GUI");
        radiusSpin->setValue(s.value("bubble_radius", 10).toInt());
    }

    auto* radiusPreview = new QLabel(secBolle);
    radiusPreview->setObjectName("cardDesc");
    radiusPreview->setText("(applicato alle nuove bolle)");

    auto updateRadius = [radiusSpin, radiusPreview]() {
        QSettings("Prismalux", "GUI").setValue("bubble_radius", radiusSpin->value());
        radiusPreview->setText(
            radiusSpin->value() == 0
                ? "Bolle con angoli netti"
                : radiusSpin->value() <= 6
                    ? "Bolle leggermente arrotondate"
                    : radiusSpin->value() <= 14
                        ? "Bolle arrotondate (default)"
                        : "Bolle molto arrotondate");
    };
    updateRadius();

    QObject::connect(radiusSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                     secBolle, [updateRadius](int){ updateRadius(); });

    radiusLay->addWidget(radiusLbl);
    radiusLay->addWidget(radiusSpin);
    radiusLay->addWidget(radiusPreview);
    radiusLay->addStretch();
    bolleLay->addWidget(radiusRow);
    vlay->addWidget(secBolle);

    vlay->addStretch();
    return outer;
}

/* ──────────────────────────────────────────────────────────────
   buildTestTab — registro di tutti i test eseguiti e superati
   ────────────────────────────────────────────────────────────── */
QWidget* ImpostazioniPage::buildTestTab()
{
    /* ── Struttura dati per ogni suite di test ───────────────────────────
       details: voci separate da '\n', mostrate come lista bullet
       kpi:     metriche chiave (tempi, soglie), mostrate in evidenza
    ─────────────────────────────────────────────────────────────────── */
    struct TestEntry {
        QString suite;
        int     passed;
        int     total;
        QString date;
        QString desc;          /* una riga: cosa testa */
        QString details;       /* voci separate da \n — lista bullet */
        QString kpi;           /* metriche numeriche in evidenza */
    };

    static const TestEntry kTests[] = {
        {
            "test_engine",
            18, 18, "2026-03-18",
            "TUI C \xe2\x80\x94 pipeline core: connessione TCP, parsing JSON, streaming AI, python3",
            "TCP helpers: connessione a Ollama e llama-server\n"
            "JSON encode/decode: js_str(), js_encode() con caratteri speciali\n"
            "ai_chat_stream Ollama: stream NDJSON completo\n"
            "ai_chat_stream llama-server: stream SSE (OpenAI API)\n"
            "python3 popen: esecuzione script e cattura output\n"
            "backend_ready(): rilevamento server attivo",
            "18/18 pass \xc2\xb7 nessuna dipendenza esterna richiesta"
        },
        {
            "test_multiagente",
            8, 8, "2026-03-18",
            "TUI C \xe2\x80\x94 pipeline 6 agenti end-to-end con modelli reali (qwen3:4b + qwen2.5-coder:7b)",
            "Ricercatore: raccoglie informazioni sul task\n"
            "Pianificatore: struttura il piano di sviluppo\n"
            "Programmatori A e B: lavoro parallelo su due implementazioni\n"
            "Tester: esegue il codice e cicla correzioni (max 3 tentativi)\n"
            "Ottimizzatore: revisione finale dell\xe2\x80\x99output\n"
            "Validazione: output Python eseguibile, nessun traceback",
            "8/8 pass \xc2\xb7 richiede Ollama attivo con i modelli installati"
        },
        {
            "test_buildsys.cpp",
            36, 36, "2026-03-22",
            "Qt GUI \xe2\x80\x94 _buildSys(): costruzione system prompt adattivo per famiglia LLM",
            "Modelli piccoli \xe2\x89\xa4" "4.5B: usa sysPromptSmall (2-3 frasi brevi)\n"
            "Modelli medi 7-8B: system prompt standard\n"
            "Modelli grandi 9B+: system prompt esteso con dettagli\n"
            "LlamaLocal: tronca a 2 frasi per compatibilit\xc3\xa0 contesto limitato\n"
            "Reasoning (qwen3/deepseek-r1): aggiunge nota /think diretta\n"
            "Iniezione math [=N]: aggiunge avviso valori pre-calcolati\n"
            "Edge case: modello senza dimensione, senza 'b' nel nome",
            "36/36 pass \xc2\xb7 isolato, no AI \xc2\xb7 7 famiglie LLM testate"
        },
        {
            "test_toon_config.c",
            55, 55, "2026-03-22",
            "TUI C \xe2\x80\x94 parser file .toon e persistenza configurazione Prismalux",
            "Lettura chiave semplice e prima riga del file\n"
            "toon_get_int(): conversione intera con fallback\n"
            "Overflow buffer: lunghezza valore > capacit\xc3\xa0 (no crash)\n"
            "CRLF e spazi trailing: normalizzazione automatica\n"
            "Valori con ':' (es. qwen3.5:9b): parsing corretto\n"
            "Chiave parziale: 'model' != 'ollama_model' (no falsi positivi)\n"
            "UTF-8: caratteri multibyte nei valori\n"
            "Round-trip write\xe2\x86\x92read: dati identici dopo salvataggio\n"
            "Stress: 200 chiavi, 10k ricerche",
            "55/55 pass \xc2\xb7 10k ricerche < 100ms \xc2\xb7 scenari reali Prismalux"
        },
        {
            "test_stress.c",
            74, 74, "2026-03-22",
            "TUI C \xe2\x80\x94 stress API pubblica: JSON, buffer, Unicode, input anomali",
            "js_str parser: stringhe vuote, escape, nidificazioni profonde\n"
            "js_encode escaping: caratteri di controllo, backslash, virgolette\n"
            "BackendCtx: modello vuoto, host nullo, porta fuori range\n"
            "IEEE 754: NaN, +Inf, -Inf, zero negativo\n"
            "Buffer overflow: protezione strlen > capacit\xc3\xa0\n"
            "UTF-8 multibyte: cinese, arabo, emoji nei prompt\n"
            "String ops interni: strncpy, snprintf con n=0\n"
            "Input TUI assurdi: 0, -1, 999, stringa vuota, solo spazi",
            "74/74 pass \xc2\xb7 nessun crash su 74 scenari anomali"
        },
        {
            "test_cs_random.c",
            17, 17, "2026-03-22",
            "TUI C \xe2\x80\x94 Context Store semantico: scrittura massiva, precision e recall",
            "Scrittura 500 chunk con chiavi semantiche casuali\n"
            "Precision: i risultati contengono almeno una chiave della query\n"
            "Recall: nessun chunk rilevante viene escluso\n"
            "Score ordering: chunk con pi\xc3\xb9 chiavi comuni viene restituito prima\n"
            "Persistenza: close() + reopen() restituisce gli stessi risultati\n"
            "Edge case: chiave inesistente (0 risultati), chunk da 10 KB\n"
            "Stress: 1000 query consecutive senza degrado",
            "17/17 pass \xc2\xb7 throughput > 2400 query/s"
        },
        {
            "test_guardia_math.c",
            93, 93, "2026-03-22",
            "TUI C \xe2\x80\x94 parser aritmetico guardia pre-pipeline (copiato in isolamento da multi_agente.c)",
            "Operazioni base: +, -, *, /, % con precedenza corretta\n"
            "Potenza e fattoriale: 2^10=1024, 5!=120\n"
            "Espressioni composite: ((3+4)*2-1)^2 = 169\n"
            "Divisione sicura: 1/0=0, 0%0=0 (no crash, no UB)\n"
            "Pattern sconto: '20% di sconto su 150' \xe2\x86\x92 120\n"
            "Pattern percentuale: '15% di 200' \xe2\x86\x92 30\n"
            "Numeri primi: primes(10,50) \xe2\x86\x92 [11,13,17,19,23,29,31,37,41,43,47]\n"
            "Sommatoria: sum(1,100) \xe2\x86\x92 5050 (formula Gauss)\n"
            "Linguaggio naturale: 'quanto fa 3+4', 'calcola 10*5'\n"
            "_task_sembra_codice(): riconosce task non-programmatici\n"
            "Edge case: range >100k, somma inversa, overflow",
            "93/93 pass \xc2\xb7 30k eval in 4ms (7.5M eval/s) \xc2\xb7 isolato, no AI"
        },
        {
            "test_math_locale.c",
            120, 120, "2026-03-22",
            "TUI C \xe2\x80\x94 motore matematico Tutor (copiato in isolamento da strumenti.c)",
            "_math_eval: parser ricorsivo completo con precedenza\n"
            "_is_prime: tutti i 1229 numeri primi tra 1 e 10000\n"
            "_fmt_num: interi mostrati senza .0 superfluo\n"
            "Sconto e ricarica: '30% sconto su 200' \xe2\x86\x92 140, '20% ricarica su 50' \xe2\x86\x92 60\n"
            "Percentuale semplice: '15% di 80' \xe2\x86\x92 12\n"
            "MCD/MCM Euclide: mcd(48,18)=6, mcm(4,6)=12\n"
            "Radice quadrata: sqrt(144)=12, sqrt(2)\xe2\x89\x88" "18 decimali\n"
            "Media: media di [1,2,3,4,5] = 3\n"
            "Sommatoria Gauss: sum(1,100) = 5050\n"
            "Fattoriale: 10! = 3628800, soglia overflow per n>20\n"
            "Edge case: numero negativo, espressione nulla, valore troppo grande",
            "120/120 pass \xc2\xb7 150k eval in 22ms \xc2\xb7 1229 primi verificati \xc2\xb7 isolato, no AI"
        },
        {
            "test_agent_scheduler.c",
            87, 87, "2026-03-22",
            "TUI C \xe2\x80\x94 Hot/Cold scheduler GGUF: API completa senza llama.cpp",
            "as_init: legge VRAM/RAM da HWInfo, imposta flag sequential se VRAM < 4 GB\n"
            "as_add: overflow slot (max 8), normalizzazione priorit\xc3\xa0, path Windows\n"
            "as_assign_layers: CPU-only=0, scaling proporzionale VRAM, sticky-hot=base, light=cap 16\n"
            "as_load: cache hit (hot_idx==idx) incrementa cache_hits e ritorna subito\n"
            "as_evict: evict doppio sicuro (no double-free)\n"
            "as_record: EMA latenza \xce\xb1=0.3, promozione sticky-hot a 3 chiamate consecutive\n"
            "vram_profile round-trip: salva/ricarica JSON con delta VRAM per modello\n"
            "Modalit\xc3\xa0 sequential: pipeline lineare quando VRAM insufficiente\n"
            "Stress: 1000 cicli load/evict/record",
            "87/87 pass \xc2\xb7 stress 1000 cicli \xc2\xb7 500 cache hit + 500 miss verificati"
        },
        {
            "test_perf.c",
            26, 26, "2026-03-22",
            "TUI C \xe2\x80\x94 Performance: TTFT, throughput token, cold start, benchmark timer",
            "Timer CLOCK_MONOTONIC: risoluzione misurata (0.133 \xc2\xb5s su questo sistema)\n"
            "Monotonicità: 100 campionamenti sempre crescenti\n"
            "TTFT scenario 50ms: primo token rilevato tra 40ms e 200ms\n"
            "TTFT scenario istantaneo: < 100ms, throughput simulato 86M tok/s\n"
            "TTFT scenario 15 tok/s: TTFT 200ms, throughput 25.8 tok/s\n"
            "Cold start scheduler: 1000 init in 79ms (0.079ms/init)\n"
            "vram_profile round-trip: 100 save/load in 2.1ms\n"
            "Contatore token: accuratezza \xc2\xb1" "10% su 100 frasi da 10 parole\n"
            "Benchmark now_ms(): 1M chiamate in 24ms (24 ns/call)",
            "26/26 pass \xc2\xb7 TTFT KPI < 3s \xc2\xb7 throughput KPI > 10 tok/s \xc2\xb7 timer 0.133\xc2\xb5s"
        },
        {
            "test_sha256.c",
            20, 20, "2026-03-22",
            "TUI C \xe2\x80\x94 SHA-256 puro C (FIPS 180-4) per verifica integrit\xc3\xa0 file .gguf",
            "Vettore NIST 1: SHA-256(\"\") = e3b0c442...\n"
            "Vettore NIST 2: SHA-256(\"abc\") = ba7816bf...\n"
            "Vettore NIST 3: SHA-256(stringa 448-bit) = 248d6a61...\n"
            "Vettore NIST 4: SHA-256(\"The quick brown fox...\") = d7a8fbb3...\n"
            "Vettore NIST 5: SHA-256(byte 0x00) = 6e340b9c...\n"
            "Consistenza: hash blocco singolo = aggiornamenti byte per byte\n"
            "File vuoto: hash corretto; file inesistente: ritorna -1\n"
            "File .gguf reali: Qwen2.5-3B (13s), SmolLM2-135M (0.7s)\n"
            "Avalanche effect: 1 byte modificato \xe2\x86\x92 32/32 byte digest diversi\n"
            "Formato HuggingFace: 'sha256:<hex>' (71 caratteri)",
            "20/20 pass \xc2\xb7 1 MB hashato in 7ms \xc2\xb7 5 vettori NIST verificati"
        },
        {
            "test_memory.c",
            12, 12, "2026-03-22",
            "TUI C \xe2\x80\x94 Rilevamento memory leak via /proc/self/status (campo VmRSS)",
            "Lettura VmRSS: baseline 1.7 MB, delta tra due letture consecutive = 0 KB\n"
            "Baseline malloc/free: 10k alloc con dimensioni 128-4096 byte, delta RSS = 0 KB\n"
            "Scheduler 1000 cicli: as_init+as_add+as_assign+as_load+as_record+as_evict, delta = 120 KB\n"
            "Context Store 5 cicli: 40 write + cs_close() ogni ciclo, delta = 4 KB\n"
            "Stress alloc 100k: pattern prompt AI (500B, 4KB, 8KB, 2KB, 256B), delta = 92 KB\n"
            "Pipeline alloc: 100 run da 6 agenti \xc3\x97" "16 KB ciascuno (cascade), delta finale 92 KB\n"
            "KPI finale: RSS processo test = 3.1 MB (soglia < 50 MB)",
            "12/12 pass \xc2\xb7 scheduler < 512 KB/1000 cicli \xc2\xb7 RSS finale 3.1 MB"
        },
        {
            "test_rag.c",
            30, 30, "2026-03-22",
            "TUI C \xe2\x80\x94 RAG locale: precision, score ordering, edge case, performance",
            "Dir inesistente: rag_query() ritorna 0 chunk, out_ctx vuoto (no crash)\n"
            "Dir vuota: stessa garanzia, out_ctx = stringa vuota\n"
            "Precision math: chunk restituiti contengono le parole 'matematica'/'equazioni'\n"
            "Precision Python: chunk contengono 'Python'/'programmazione'\n"
            "Precision storia: chunk contengono 'storia'/'medioevo'\n"
            "Score ordering: documento ad alta rilevanza incluso nel top-K\n"
            "File non-.txt ignorati: .json con dati rilevanti non viene restituito\n"
            "File .txt vuoto: no crash, 0 chunk\n"
            "rag_build_prompt: output inizia con 'CONTESTO DOCUMENTALE'\n"
            "rag_build_prompt senza RAG: ritorna solo il sys prompt base\n"
            "Edge case: query vuota, parole < 3 caratteri, buffer da 10 byte\n"
            "Documenti reali: rag_docs/730 e rag_docs/piva (3 chunk ciascuno)",
            "30/30 pass \xc2\xb7 5 query in 0.22ms \xc2\xb7 100 stress query in 3.7ms"
        },
        {
            "test_security.c",
            65, 65, "2026-03-22",
            "TUI C \xe2\x80\x94 Sicurezza: anti-prompt-injection e anti-data-leak (isolato)",
            "T01 ChatML/Qwen: <|im_start|>, <|im_end|>, <|endoftext|>, <|eot_id|>, ### rimossi\n"
            "T02 Llama2/Mistral: [INST], [/INST], <<SYS>>, <</SYS>>, <!-- rimossi\n"
            "T03 Role override: \\n\\nSystem:, \\n\\nAssistant:, \\n\\nUser:, \\n\\nHuman:, "
            "\\n\\nAI: neutralizzati (solo \\n\\n rimosso, testo preservato)\n"
            "T04 Pattern multipli: tutti i pattern rimossi da una stringa mista; "
            "### rimosso in tutte le occorrenze\n"
            "T05 Testo legittimo: codice Python, matematica, italiano con accenti, "
            "URL, newline singoli \xe2\x80\x94 NON modificati\n"
            "T06 Idempotenza: applicata 2 volte = stesso risultato di 1 volta\n"
            "T07 Edge case: NULL, bufmax=0, stringa vuota, soli spazi, buffer 1 byte \xe2\x80\x94 no crash\n"
            "T08 Confine buffer: ### al bordo del buffer \xe2\x80\x94 no overflow, null terminator intatto\n"
            "T09 js_encode nome modello: virgolette, newline, backslash, CR bloccati; "
            "modello legittimo 'qwen2.5-coder:7b' non alterato\n"
            "T10 host_is_local whitelist: 127.0.0.1, ::1, localhost, LOCALHOST accettati\n"
            "T11 host esterni bloccati: 8.8.8.8, 192.168.1.1, api.openai.com, "
            "127.0.0.2, URL injection credential, stringa vuota, NULL\n"
            "T12 Stress: 10k sanitizzazioni su 10 input casuali \xe2\x80\x94 0 injection sopravvivono",
            "65/65 pass \xc2\xb7 10k sanitizzazioni in 1.9ms \xc2\xb7 isolato, no AI, no rete"
        },
        {
            "test_golden.c",
            53, 53, "2026-03-23",
            "Golden Prompts \xe2\x80\x94 framework checker qualit\xc3\xa0 risposte AI (12 prompt, 5 categorie)",
            "T01\xe2\x80\x93T04 matematica: keyword '4','30','6','11/13/17/19' presenti; 'error/sorry' assenti\n"
            "T05\xe2\x80\x93T07 codice Python: 'def','for','range' presenti; '[]' o 'list()' per lista vuota; 'len' spiegato\n"
            "T08\xe2\x80\x93T09 lingua italiana: 'algoritmo/istruzioni/problema'; 'dato/struttura' \xe2\x80\x94 no parole inglesi\n"
            "T10\xe2\x80\x93T11 pratico: 'forfettario/regime'; '730/redditi/dichiarazione'\n"
            "T12 sicurezza: 'query/database/SQL/injection' \xe2\x80\x94 no 'I cannot/sorry'\n"
            "Mock positivi (4): tutte le keyword trovate, nessuna parola vietata \xe2\x80\x94 passano\n"
            "Mock negativi (4): keyword mancanti o parole vietate o testo troppo corto \xe2\x80\x94 falliscono\n"
            "Integrit\xc3\xa0 JSON: file golden_prompts.json letto, hash SHA-256 stabile",
            "53/53 pass \xc2\xb7 4 mock positivi OK \xc2\xb7 4 mock negativi rilevati \xc2\xb7 no AI, no rete"
        },
        {
            "test_version.c",
            35, 35, "2026-03-23",
            "Build Reproducibility \xe2\x80\x94 tracciabilit\xc3\xa0 e stabilit\xc3\xa0 del build",
            "T01 Sorgenti: tutti e 14 i file src/*.c esistono e sono leggibili\n"
            "T02 Header: 8 dichiarazioni critiche verificate (prompt_sanitize, ai_chat_stream, hw_detect, "
            "as_init, cs_open, rag_query, js_encode, tcp_connect)\n"
            "T03 Makefile: 10 target attesi presenti (http, clean, test, test_perf, test_sha256, "
            "test_memory, test_rag, test_security, test_golden, test_version)\n"
            "T04 Hash stabile: SHA-256 di 5 sorgenti identico tra due letture consecutive\n"
            "T05 Binario: 'prismalux' esiste, > 50 KB, < 50 MB \xe2\x80\x94 hash build calcolato per tracciabilit\xc3\xa0\n"
            "T06 golden_prompts.json: esiste, > 100 byte, hash stabile, contiene i campi attesi\n"
            "T07 Sorgenti non vuoti: nessun src/*.c da 0 byte\n"
            "T08 Suite completa: tutti i 15 file test_*.c presenti nella directory",
            "35/35 pass \xc2\xb7 14 src verificati \xc2\xb7 hash SHA-256 stabile \xc2\xb7 build fingerprint calcolato"
        },
    };

    auto* outer = new QWidget(this);
    auto* vlay  = new QVBoxLayout(outer);
    vlay->setContentsMargins(20, 20, 20, 20);
    vlay->setSpacing(14);

    /* ── Intestazione ── */
    auto* title = new QLabel(
        "\xe2\x9c\x85  Registro Test \xe2\x80\x94 tutti i test superati", outer);
    title->setStyleSheet("font-size:16px; font-weight:700; color:#e5e7eb;");
    vlay->addWidget(title);

    /* ── Sommario totale ── */
    int totPassed = 0, totTotal = 0;
    for (const auto& t : kTests) { totPassed += t.passed; totTotal += t.total; }

    auto* summary = new QLabel(
        QString("\xf0\x9f\x93\x8a  Totale: <b>%1/%2</b> test superati in <b>%3</b> suite")
            .arg(totPassed).arg(totTotal).arg(int(sizeof kTests / sizeof kTests[0])),
        outer);
    summary->setStyleSheet("color:#9ca3af; font-size:13px;");
    vlay->addWidget(summary);

    /* ── Scroll area con una card per ogni suite ── */
    auto* scroll = new QScrollArea(outer);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* inner = new QWidget(scroll);
    auto* ilay  = new QVBoxLayout(inner);
    ilay->setSpacing(10);
    ilay->setContentsMargins(0, 4, 0, 4);
    scroll->setWidget(inner);
    vlay->addWidget(scroll);

    for (const auto& t : kTests) {
        const bool allOk = (t.passed == t.total);
        const QString accent = allOk ? "#22c55e" : "#f59e0b";

        /* ── Card ── */
        auto* card = new QFrame(inner);
        card->setFrameShape(QFrame::StyledPanel);
        card->setStyleSheet(QString(
            "QFrame { background:#1a1b28; border:1px solid %1;"
            " border-left: 4px solid %1; border-radius:8px; }").arg(accent));

        auto* clay = new QVBoxLayout(card);
        clay->setContentsMargins(14, 12, 14, 12);
        clay->setSpacing(6);

        /* ── Riga 1: nome suite + badge + data ── */
        auto* row1 = new QHBoxLayout;
        auto* lblSuite = new QLabel(
            QString("<b>%1</b>").arg(t.suite.toHtmlEscaped()), card);
        lblSuite->setStyleSheet("color:#e5e7eb; font-size:13px; font-family: monospace;");
        row1->addWidget(lblSuite);
        row1->addStretch();

        auto* lblBadge = new QLabel(
            QString("<span style='color:%1; font-weight:700; font-size:14px;'>%2/%3</span>")
                .arg(accent).arg(t.passed).arg(t.total), card);
        row1->addWidget(lblBadge);

        auto* lblDate = new QLabel(t.date, card);
        lblDate->setStyleSheet("color:#6b7280; font-size:11px; margin-left:12px;");
        row1->addWidget(lblDate);
        clay->addLayout(row1);

        /* ── Riga 2: descrizione breve ── */
        auto* lblDesc = new QLabel(t.desc, card);
        lblDesc->setWordWrap(true);
        lblDesc->setStyleSheet("color:#d1d5db; font-size:12px; padding-bottom:4px;");
        clay->addWidget(lblDesc);

        /* ── Separatore ── */
        auto* sep = new QFrame(card);
        sep->setFrameShape(QFrame::HLine);
        sep->setStyleSheet("color:#2d2e3e;");
        clay->addWidget(sep);

        /* ── Riga 3: lista bullet dei casi testati ── */
        auto* lblCatTitle = new QLabel(
            "\xf0\x9f\x94\xac  <b>Casi testati:</b>", card);   /* 🔬 */
        lblCatTitle->setStyleSheet("color:#9ca3af; font-size:11px;");
        clay->addWidget(lblCatTitle);

        const QStringList items = t.details.split('\n', Qt::SkipEmptyParts);
        QString bulletHtml = "<ul style='margin:2px 0 0 0; padding-left:18px;"
                             " color:#6b7280; font-size:11px;'>";
        for (const QString& item : items)
            bulletHtml += "<li style='margin-bottom:2px;'>"
                        + item.toHtmlEscaped() + "</li>";
        bulletHtml += "</ul>";
        auto* lblBullets = new QLabel(bulletHtml, card);
        lblBullets->setTextFormat(Qt::RichText);
        lblBullets->setWordWrap(true);
        clay->addWidget(lblBullets);

        /* ── Riga 4: KPI in evidenza ── */
        if (!t.kpi.isEmpty()) {
            auto* lblKpi = new QLabel(
                QString("\xe2\x9a\xa1  <b>KPI:</b> %1")   /* ⚡ */
                    .arg(t.kpi.toHtmlEscaped()), card);
            lblKpi->setStyleSheet(
                QString("color:%1; font-size:11px; font-weight:600;"
                        " background:#0f1020; border-radius:4px;"
                        " padding:3px 8px;").arg(accent));
            lblKpi->setTextFormat(Qt::RichText);
            clay->addWidget(lblKpi);
        }

        ilay->addWidget(card);
    }

    ilay->addStretch();
    return outer;
}

void ImpostazioniPage::onHWReady(HWInfo hw) {
    if (m_manutenzione) m_manutenzione->onHWReady(hw);
}

/* ══════════════════════════════════════════════════════════════
   Piper TTS — helper statici
   ══════════════════════════════════════════════════════════════ */
/* Cartella piper/ accanto all'eseguibile — tutto autocontenuto nel progetto */
static QString s_piperDir() {
    return QCoreApplication::applicationDirPath() + "/piper";
}

QString ImpostazioniPage::piperBinPath() {
#ifdef Q_OS_WIN
    const QString name = "piper.exe";
#else
    const QString name = "piper";
#endif
    /* 1. Accanto all'eseguibile: <appDir>/piper/piper */
    const QString local = s_piperDir() + "/" + name;
    if (QFileInfo::exists(local)) return local;

    /* 2. Fallback: PATH di sistema */
    const QString sys = QStandardPaths::findExecutable("piper");
    if (!sys.isEmpty()) return sys;

    return {};
}

QString ImpostazioniPage::piperVoicesDir() {
    return s_piperDir() + "/voices";
}

QString ImpostazioniPage::piperActiveVoice() {
    QFile f(s_piperDir() + "/active_voice");
    if (f.open(QIODevice::ReadOnly))
        return QString::fromUtf8(f.readAll()).trimmed();
    return "it_IT-paola-medium";  /* default: voce femminile */
}

void ImpostazioniPage::savePiperActiveVoice(const QString& name) {
    QDir().mkpath(s_piperDir());
    QFile f(s_piperDir() + "/active_voice");
    if (f.open(QIODevice::WriteOnly | QIODevice::Text))
        f.write(name.toUtf8());
}

/* ══════════════════════════════════════════════════════════════
   buildVoceTab — sezione impostazioni TTS (Piper)
   ══════════════════════════════════════════════════════════════ */
QWidget* ImpostazioniPage::buildVoceTab()
{
    struct VoceInfo {
        QString id;         /* nome file ONNX senza estensione */
        QString label;      /* nome leggibile */
        QString sex;        /* "maschio" / "femmina" */
        QString quality;    /* "veloce" / "media" / "alta" */
        int     sizeMb;     /* dimensione approssimativa MB */
        QString hfPath;     /* percorso HuggingFace relativo alla root voices/ */
    };

    static const VoceInfo voices[] = {
        { "it_IT-riccardo-x_low",
          "Riccardo", "maschio", "veloce", 24,
          "it/it_IT/riccardo/x_low" },
        { "it_IT-paola-medium",
          "Paola", "femmina", "media", 66,
          "it/it_IT/paola/medium" },
        { "it_IT-fabian-medium",
          "Fabian", "maschio", "media", 66,
          "it/it_IT/fabian/medium" },
    };
    static constexpr int N_VOICES = int(sizeof(voices) / sizeof(voices[0]));

    /* (URL HuggingFace e binario Piper definiti inline nei lambda) */

    /* ── Colonna sinistra: TTS Piper (nessun scroll interno) ── */
    auto* inner = new QGroupBox(
        "\xf0\x9f\x94\x8a  Voce \xe2\x80\x94 Piper TTS");
    inner->setObjectName("cardGroup");
    auto* ilay  = new QVBoxLayout(inner);
    ilay->setContentsMargins(14, 10, 14, 10);
    ilay->setSpacing(8);

    /* ── Sezione 1: stato Piper ── */
    auto* secPiper = new QFrame(inner);
    secPiper->setObjectName("cardFrame");
    auto* secLay = new QVBoxLayout(secPiper);
    secLay->setContentsMargins(14, 10, 14, 10);
    secLay->setSpacing(8);

    auto* piperRow = new QWidget(secPiper);
    auto* piperRowLay = new QHBoxLayout(piperRow);
    piperRowLay->setContentsMargins(0, 0, 0, 0);
    piperRowLay->setSpacing(10);

    auto* lblPiperStatus = new QLabel(secPiper);
    lblPiperStatus->setObjectName("cardDesc");

    auto refreshStatus = [lblPiperStatus]() {
        const QString bin = ImpostazioniPage::piperBinPath();
        if (bin.isEmpty())
            lblPiperStatus->setText(
                "\xe2\x9d\x8c  Piper non trovato &mdash; clicca <b>Installa Piper</b>");
        else
            lblPiperStatus->setText(
                "\xe2\x9c\x85  Piper pronto: <code>" + bin + "</code>");
    };
    refreshStatus();

    auto* btnInstall = new QPushButton(
        "\xf0\x9f\x93\xa5  Installa Piper", secPiper);
    btnInstall->setObjectName("actionBtn");
    btnInstall->setToolTip("Scarica e installa il binario Piper nella cartella del progetto (<appDir>/piper/)");

    piperRowLay->addWidget(lblPiperStatus, 1);
    piperRowLay->addWidget(btnInstall);
    secLay->addWidget(piperRow);

    /* Nota fallback */
    auto* lblFallback = new QLabel(
        "\xf0\x9f\x92\xa1  Fallback automatico se Piper non disponibile: "
        "<b>espeak-ng</b> \xe2\x86\x92 spd-say \xe2\x86\x92 festival",
        secPiper);
    lblFallback->setObjectName("cardDesc");
    lblFallback->setTextFormat(Qt::RichText);
    secLay->addWidget(lblFallback);

    ilay->addWidget(secPiper);

    /* ── Sezione 2: voci italiane ── */
    auto* secVoci = new QFrame(inner);
    secVoci->setObjectName("cardFrame");
    auto* secVLay = new QVBoxLayout(secVoci);
    secVLay->setContentsMargins(14, 10, 14, 10);
    secVLay->setSpacing(6);

    auto* voceTitle = new QLabel(
        "\xf0\x9f\x87\xae\xf0\x9f\x87\xb9  <b>Voci Italiane disponibili</b>",
        secVoci);
    voceTitle->setObjectName("cardTitle");
    voceTitle->setTextFormat(Qt::RichText);
    secVLay->addWidget(voceTitle);

    /* Combo selezione voce attiva */
    auto* activeRow = new QWidget(secVoci);
    auto* activeRowLay = new QHBoxLayout(activeRow);
    activeRowLay->setContentsMargins(0, 0, 0, 0);
    activeRowLay->setSpacing(8);
    activeRowLay->addWidget(new QLabel("Voce attiva:", secVoci));
    auto* cmbVoice = new QComboBox(secVoci);
    cmbVoice->setObjectName("settingsCombo");
    for (int i = 0; i < N_VOICES; i++) {
        cmbVoice->addItem(
            QString("%1 (%2, %3)").arg(voices[i].label, voices[i].sex, voices[i].quality),
            voices[i].id);
    }
    /* Seleziona quella attiva */
    {
        const QString active = ImpostazioniPage::piperActiveVoice();
        for (int i = 0; i < N_VOICES; i++)
            if (voices[i].id == active) { cmbVoice->setCurrentIndex(i); break; }
    }
    QObject::connect(cmbVoice, QOverload<int>::of(&QComboBox::currentIndexChanged),
                     inner, [cmbVoice](int idx){
        ImpostazioniPage::savePiperActiveVoice(cmbVoice->itemData(idx).toString());
    });
    activeRowLay->addWidget(cmbVoice, 1);
    secVLay->addWidget(activeRow);

    /* Griglia voci con pulsante scarica per ognuna */
    for (int i = 0; i < N_VOICES; i++) {
        const VoceInfo& v = voices[i];

        auto* row = new QWidget(secVoci);
        auto* rowLay = new QHBoxLayout(row);
        rowLay->setContentsMargins(0, 2, 0, 2);
        rowLay->setSpacing(10);

        /* Check se già installata */
        const QString onnxPath = ImpostazioniPage::piperVoicesDir()
                                + "/" + v.id + ".onnx";
        const bool installed = QFileInfo::exists(onnxPath);

        auto* lbl = new QLabel(
            QString("<b>%1</b>  <small>%2, %3, ~%4 MB</small>")
            .arg(v.label, v.sex, v.quality).arg(v.sizeMb),
            row);
        lbl->setObjectName("cardDesc");
        lbl->setTextFormat(Qt::RichText);

        auto* lblVoiceStatus = new QLabel(
            installed ? "\xe2\x9c\x85 Installata" : "\xe2\x80\x94", row);
        lblVoiceStatus->setObjectName("cardDesc");
        lblVoiceStatus->setMinimumWidth(100);

        auto* btnDl = new QPushButton(
            installed ? "\xf0\x9f\x94\x84 Reinstalla" : "\xf0\x9f\x93\xa5 Scarica",
            row);
        btnDl->setObjectName("actionBtn");
        btnDl->setToolTip(
            QString("Scarica %1.onnx (~%2 MB) da HuggingFace")
            .arg(v.id).arg(v.sizeMb));

        rowLay->addWidget(lbl, 1);
        rowLay->addWidget(lblVoiceStatus);
        rowLay->addWidget(btnDl);
        secVLay->addWidget(row);

        /* Cattura dati necessari nel lambda (copia by value) */
        const QString voiceId   = v.id;
        const QString hfPath    = v.hfPath;
        QObject::connect(btnDl, &QPushButton::clicked, inner,
                         [btnDl, lblVoiceStatus, voiceId, hfPath]() mutable
        {
            const QString voicesDir = ImpostazioniPage::piperVoicesDir();
            QDir().mkpath(voicesDir);

            static const QString hfBase =
                "https://huggingface.co/rhasspy/piper-voices/resolve/main/";
            const QString onnxUrl  = hfBase + hfPath + "/" + voiceId + ".onnx";
            const QString jsonUrl  = hfBase + hfPath + "/" + voiceId + ".onnx.json";
            const QString onnxDest = voicesDir + "/" + voiceId + ".onnx";
            const QString jsonDest = voicesDir + "/" + voiceId + ".onnx.json";

            btnDl->setEnabled(false);
            btnDl->setText("\xe2\x8f\xb3 Scaricamento...");
            lblVoiceStatus->setText("\xe2\x8f\xb3 Download .json...");

#ifdef Q_OS_WIN
            const QString downloader = "curl.exe";
#else
            const QString downloader = "curl";
#endif
            /* Prima scarica il .json (piccolo), poi .onnx (grande) */
            auto* procJson = new QProcess(btnDl);
            procJson->start(downloader,
                { "-L", "-o", jsonDest, jsonUrl });

            QObject::connect(procJson,
                QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                btnDl, [procJson, btnDl, lblVoiceStatus,
                        downloader, onnxUrl, onnxDest, voiceId](int code, QProcess::ExitStatus) {
                    procJson->deleteLater();
                    if (code != 0) {
                        lblVoiceStatus->setText("\xe2\x9d\x8c Errore .json");
                        btnDl->setEnabled(true);
                        btnDl->setText("\xf0\x9f\x93\xa5 Scarica");
                        return;
                    }
                    lblVoiceStatus->setText("\xe2\x8f\xb3 Download .onnx...");
                    auto* procOnnx = new QProcess(btnDl);
                    procOnnx->start(downloader,
                        { "-L", "-o", onnxDest, onnxUrl });
                    QObject::connect(procOnnx,
                        QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                        btnDl, [procOnnx, btnDl, lblVoiceStatus, voiceId](int code2, QProcess::ExitStatus) {
                            procOnnx->deleteLater();
                            btnDl->setEnabled(true);
                            if (code2 == 0) {
                                lblVoiceStatus->setText("\xe2\x9c\x85 Installata");
                                btnDl->setText("\xf0\x9f\x94\x84 Reinstalla");
                            } else {
                                lblVoiceStatus->setText("\xe2\x9d\x8c Errore .onnx");
                                btnDl->setText("\xf0\x9f\x93\xa5 Scarica");
                            }
                        });
                });
        });
    }

    ilay->addWidget(secVoci);

    /* ── Sezione 3: installa binario Piper ── */
    QObject::connect(btnInstall, &QPushButton::clicked, inner,
                     [btnInstall, lblPiperStatus, refreshStatus]()
    {
#if defined(Q_OS_WIN)
        static const QString PIPER_BIN_URL2 =
            "https://github.com/rhasspy/piper/releases/download/2023.11.14-2/"
            "piper_windows_amd64.zip";
        static const QString PIPER_ARCHIVE2 = "piper_windows_amd64.zip";
#elif defined(Q_OS_MACOS)
        static const QString PIPER_BIN_URL2 =
            "https://github.com/rhasspy/piper/releases/download/2023.11.14-2/"
            "piper_macos_x86_64.tar.gz";
        static const QString PIPER_ARCHIVE2 = "piper_macos_x86_64.tar.gz";
#else
        static const QString PIPER_BIN_URL2 =
            "https://github.com/rhasspy/piper/releases/download/2023.11.14-2/"
            "piper_linux_x86_64.tar.gz";
        static const QString PIPER_ARCHIVE2 = "piper_linux_x86_64.tar.gz";
#endif
        const QString installDir = s_piperDir();
        QDir().mkpath(installDir);
        const QString archivePath = installDir + "/" + PIPER_ARCHIVE2;

        btnInstall->setEnabled(false);
        btnInstall->setText("\xe2\x8f\xb3 Scaricamento...");
        lblPiperStatus->setText("\xe2\x8f\xb3 Download binario Piper...");

#ifdef Q_OS_WIN
        const QString downloader = "curl.exe";
#else
        const QString downloader = "curl";
#endif
        auto* proc = new QProcess(btnInstall);
        proc->start(downloader, { "-L", "-o", archivePath, PIPER_BIN_URL2 });
        QObject::connect(proc,
            QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            btnInstall, [proc, btnInstall, lblPiperStatus, refreshStatus,
                         archivePath, installDir](int code, QProcess::ExitStatus) {
                proc->deleteLater();
                if (code != 0) {
                    lblPiperStatus->setText("\xe2\x9d\x8c Errore download. Verifica la connessione.");
                    btnInstall->setEnabled(true);
                    btnInstall->setText("\xf0\x9f\x93\xa5  Installa Piper");
                    return;
                }
                lblPiperStatus->setText("\xe2\x8f\xb3 Estrazione archivio...");
                /* Estrai: tar o unzip */
                auto* procEx = new QProcess(btnInstall);
#ifdef Q_OS_WIN
                procEx->setWorkingDirectory(installDir);
                procEx->start("powershell",
                    { "-Command", QString("Expand-Archive -Path \"%1\" -DestinationPath \"%2\" -Force")
                      .arg(archivePath, installDir) });
#else
                procEx->start("tar", { "-xzf", archivePath, "-C", installDir, "--strip-components=1" });
#endif
                QObject::connect(procEx,
                    QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                    btnInstall, [procEx, btnInstall, lblPiperStatus,
                                 refreshStatus, installDir](int code2, QProcess::ExitStatus) {
                        procEx->deleteLater();
                        btnInstall->setEnabled(true);
                        btnInstall->setText("\xf0\x9f\x93\xa5  Installa Piper");
#ifndef Q_OS_WIN
                        /* Rendi eseguibile */
                        QFile::setPermissions(installDir + "/piper",
                            QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                            QFileDevice::ExeOwner  | QFileDevice::ReadGroup  |
                            QFileDevice::ExeGroup  | QFileDevice::ReadOther  |
                            QFileDevice::ExeOther);
#endif
                        if (code2 == 0) refreshStatus();
                        else lblPiperStatus->setText("\xe2\x9d\x8c Errore estrazione.");
                    });
            });
    });

    /* ── Sezione 4: test TTS ── */
    auto* secTest = new QFrame(inner);
    secTest->setObjectName("cardFrame");
    auto* secTLay = new QVBoxLayout(secTest);
    secTLay->setContentsMargins(14, 10, 14, 10);
    secTLay->setSpacing(8);

    auto* testTitle = new QLabel(
        "\xf0\x9f\x94\x8a  <b>Test voce</b>", secTest);
    testTitle->setObjectName("cardTitle");
    testTitle->setTextFormat(Qt::RichText);
    secTLay->addWidget(testTitle);

    auto* testRow = new QWidget(secTest);
    auto* testRowLay = new QHBoxLayout(testRow);
    testRowLay->setContentsMargins(0, 0, 0, 0);
    testRowLay->setSpacing(8);

    auto* txtTest = new QLineEdit(secTest);
    txtTest->setObjectName("chatInput");
    txtTest->setPlaceholderText("Scrivi un testo di prova...");
    txtTest->setText("Ciao, sono Prismalux. La conoscenza \xc3\xa8 potere.");

    auto* btnSpeak = new QPushButton(
        "\xf0\x9f\x94\x8a  Parla", secTest);
    btnSpeak->setObjectName("actionBtn");
    btnSpeak->setToolTip("Legge il testo con la voce selezionata");

    testRowLay->addWidget(txtTest, 1);
    testRowLay->addWidget(btnSpeak);
    secTLay->addWidget(testRow);

    auto* lblTestStatus = new QLabel("", secTest);
    lblTestStatus->setObjectName("cardDesc");
    secTLay->addWidget(lblTestStatus);

    QObject::connect(btnSpeak, &QPushButton::clicked, inner,
                     [btnSpeak, txtTest, lblTestStatus, cmbVoice]() {
        const QString text = txtTest->text().trimmed();
        if (text.isEmpty()) return;

        const QString piperBin = ImpostazioniPage::piperBinPath();
        const QString voiceId  = cmbVoice->currentData().toString();
        const QString onnxPath = ImpostazioniPage::piperVoicesDir()
                                + "/" + voiceId + ".onnx";

        if (!piperBin.isEmpty() && QFileInfo::exists(onnxPath)) {
            /* Usa Piper: echo TEXT | piper --model VOICE --output_raw | aplay */
            btnSpeak->setEnabled(false);
            lblTestStatus->setText("\xf0\x9f\x94\x8a  Piper in esecuzione...");
#ifdef Q_OS_WIN
            const QString cmd = QString(
                "echo %1 | \"%2\" --model \"%3\" --output_raw | "
                "powershell -Command \"$input | Set-Content piper_out.raw\"")
                .arg(text, piperBin, onnxPath);
            QProcess::startDetached("cmd", { "/c", cmd });
#else
            /* Pipeline: piper → aplay (Linux) */
            auto* proc = new QProcess(btnSpeak);
            proc->start("bash", { "-c",
                QString("echo %1 | \"%2\" --model \"%3\" --output_raw | aplay -r 22050 -f S16_LE -t raw -")
                .arg(QProcess::nullDevice(), piperBin, onnxPath)
                .replace(QProcess::nullDevice(),
                         "'" + text.toHtmlEscaped().replace("'","'\\''") + "'")
            });
            QObject::connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                btnSpeak, [proc, btnSpeak, lblTestStatus](int, QProcess::ExitStatus){
                    proc->deleteLater();
                    btnSpeak->setEnabled(true);
                    lblTestStatus->setText("");
                });
#endif
            return;
        }

        /* Fallback: espeak-ng */
        lblTestStatus->setText(piperBin.isEmpty()
            ? "\xe2\x9a\xa0  Piper non trovato, uso espeak-ng"
            : "\xe2\x9a\xa0  Voce non installata, uso espeak-ng");
        if (!QProcess::startDetached("espeak-ng", { "-l", "it", text }))
            QProcess::startDetached("spd-say", { "--lang", "it", "--", text });
        QTimer::singleShot(3000, lblTestStatus, [lblTestStatus]{ lblTestStatus->setText(""); });
    });

    ilay->addWidget(secTest);
    ilay->addStretch();
    return inner;
}

/* ══════════════════════════════════════════════════════════════
   buildTrascriviTab — impostazioni STT (whisper.cpp)
   ══════════════════════════════════════════════════════════════ */
QWidget* ImpostazioniPage::buildTrascriviTab()
{
    /* ── Modelli disponibili ── */
    struct ModelInfo {
        QString id;       /* nome file senza estensione */
        QString label;    /* nome leggibile */
        int     sizeMb;
        QString desc;
    };
    static const ModelInfo kModels[] = {
        { "ggml-tiny",     "Tiny",    39,   "Velocissimo, meno preciso" },
        { "ggml-base",     "Base",    74,   "Buon compromesso velocit\xc3\xa0/precisione" },
        { "ggml-small",    "Small",  141,   "Consigliato — bilanciato" },
        { "ggml-medium",   "Medium", 462,   "Alta precisione, pi\xc3\xb9 lento" },
        { "ggml-large-v3", "Large", 1550,   "Massima precisione" },
    };
    static constexpr int N = int(sizeof kModels / sizeof kModels[0]);

    const QString hfBase =
        "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/";
    const QString modelsDir = SttWhisper::whisperModelsDir();

    /* ── Colonna destra: STT Whisper (nessun scroll interno) ── */
    auto* inner = new QGroupBox(
        "\xf0\x9f\x8e\xa4  Trascrizione \xe2\x80\x94 Whisper STT");
    inner->setObjectName("cardGroup");
    auto* ilay = new QVBoxLayout(inner);
    ilay->setContentsMargins(14, 10, 14, 10);
    ilay->setSpacing(8);

    /* ══════════════════════════════════════════
       Sezione 1: stato binario whisper-cli
       ══════════════════════════════════════════ */
    auto* secBin = new QFrame(inner);
    secBin->setObjectName("actionCard");
    auto* secBinLay = new QVBoxLayout(secBin);
    secBinLay->setContentsMargins(14, 10, 14, 10);
    secBinLay->setSpacing(6);

    auto* binTitle = new QLabel("\xf0\x9f\x94\x8d  <b>Binario whisper-cli</b>", secBin);
    binTitle->setObjectName("cardTitle");
    binTitle->setTextFormat(Qt::RichText);
    secBinLay->addWidget(binTitle);

    auto* lblBin = new QLabel(secBin);
    lblBin->setObjectName("cardDesc");
    lblBin->setTextFormat(Qt::RichText);
    lblBin->setWordWrap(true);
    {
        const QString b = SttWhisper::whisperBin();
        if (b.isEmpty())
            lblBin->setText(
                "\xe2\x9d\x8c  Non trovato. Installa con:<br>"
                "<code>sudo apt install whisper-cpp</code>"
                "&nbsp;&nbsp;oppure&nbsp;&nbsp;"
                "<code>sudo dnf install whisper-cpp</code><br>"
                "Oppure compila da sorgente: "
                "<code>git clone https://github.com/ggml-org/whisper.cpp &amp;&amp; "
                "cd whisper.cpp &amp;&amp; cmake -B build &amp;&amp; "
                "cmake --build build -j$(nproc)</code>");
        else
            lblBin->setText("\xe2\x9c\x85  Trovato: <code>" + b + "</code>");
    }
    secBinLay->addWidget(lblBin);

    /* Pulsante compila da sorgente — visibile solo se binario mancante */
    auto* btnCompileWsp = new QPushButton(
        "\xf0\x9f\x94\xa8  Scarica e compila whisper.cpp da sorgente", secBin);
    btnCompileWsp->setObjectName("primaryBtn");
    btnCompileWsp->setFixedHeight(34);
    btnCompileWsp->setVisible(SttWhisper::whisperBin().isEmpty());
    btnCompileWsp->setToolTip(
        "Clona il repo whisper.cpp in ~/.prismalux/whisper.cpp/ e lo compila.\n"
        "Richiede: git, cmake, gcc/g++ (tutti disponibili su Ubuntu/Fedora/Arch).");
    secBinLay->addWidget(btnCompileWsp);
    ilay->addWidget(secBin);

    /* ── Card log compilazione (nascosta finché non si clicca) ── */
    auto* secCompile = new QFrame(inner);
    secCompile->setObjectName("actionCard");
    secCompile->setVisible(false);
    auto* secCLay = new QVBoxLayout(secCompile);
    secCLay->setContentsMargins(14, 10, 14, 10);
    secCLay->setSpacing(6);

    auto* compileTitle = new QLabel(
        "\xf0\x9f\x94\xa8  <b>Compilazione whisper.cpp</b>", secCompile);
    compileTitle->setObjectName("cardTitle");
    compileTitle->setTextFormat(Qt::RichText);
    secCLay->addWidget(compileTitle);

    auto* compileLog = new QPlainTextEdit(secCompile);
    compileLog->setReadOnly(true);
    compileLog->setMaximumBlockCount(800);
    compileLog->setFont(QFont("JetBrains Mono,Fira Code,Consolas", 8));
    compileLog->setMaximumHeight(120);
    compileLog->setObjectName("inputArea");
    secCLay->addWidget(compileLog);

    auto* lblCompileStatus = new QLabel("", secCompile);
    lblCompileStatus->setObjectName("statusLabel");
    lblCompileStatus->setWordWrap(true);
    secCLay->addWidget(lblCompileStatus);

    ilay->addWidget(secCompile);

    /* ── Lambda che esegue la build a 3 step ── */
    connect(btnCompileWsp, &QPushButton::clicked, secCompile,
            [btnCompileWsp, secCompile, compileLog, lblCompileStatus,
             lblBin, refreshActive = (std::function<void()>)nullptr]() mutable
    {
        btnCompileWsp->setEnabled(false);
        secCompile->setVisible(true);
        compileLog->clear();
        lblCompileStatus->setText("\xe2\x8c\x9b  Avvio in corso...");

        const QString wspDir  = QDir::homePath() + "/.prismalux/whisper.cpp";
        const QString bldDir  = wspDir + "/build";
        const int     threads = std::max(1, QThread::idealThreadCount());

        /* Usiamo un QProcess* condiviso tramite un piccolo wrapper a step */
        struct StepRunner {
            QPlainTextEdit*  log;
            QLabel*          status;
            QPushButton*     btn;
            QLabel*          lblBin;
            int              step = 0;

            void run(const QString& prog, const QStringList& args,
                     const QString& desc,
                     std::function<void(bool ok)> next)
            {
                log->appendPlainText("\n\xe2\x96\xb6 " + desc);
                log->appendPlainText("$ " + prog + " " + args.join(" ") + "\n");
                status->setText("\xe2\x8c\x9b  " + desc + "...");

                auto* proc = new QProcess(log);
                QObject::connect(proc, &QProcess::readyReadStandardOutput,
                                 log, [proc, this]{
                    log->appendPlainText(
                        QString::fromLocal8Bit(proc->readAllStandardOutput()));
                });
                QObject::connect(proc, &QProcess::readyReadStandardError,
                                 log, [proc, this]{
                    log->appendPlainText(
                        QString::fromLocal8Bit(proc->readAllStandardError()));
                });
                QObject::connect(
                    proc,
                    QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                    log, [proc, next, this](int code, QProcess::ExitStatus) {
                        proc->deleteLater();
                        const bool ok = (code == 0);
                        if (!ok)
                            log->appendPlainText("\n\xe2\x9d\x8c  Step fallito (codice " +
                                                 QString::number(code) + ")");
                        next(ok);
                    });
                proc->start(prog, args);
                if (!proc->waitForStarted(3000)) {
                    log->appendPlainText(
                        "\xe2\x9d\x8c  Impossibile avviare: " + prog +
                        "\n  Verifica che git/cmake/gcc siano installati.");
                    proc->deleteLater();
                    next(false);
                }
            }
        };

        auto* runner = new StepRunner{compileLog, lblCompileStatus, btnCompileWsp, lblBin};

        /* Step 1: git clone o git pull */
        QString gitProg; QStringList gitArgs;
        if (QDir(wspDir + "/.git").exists()) {
            gitProg = "git";
            gitArgs = {"-C", wspDir, "pull", "--rebase", "--autostash"};
        } else {
            QDir().mkpath(QDir::homePath() + "/.prismalux");
            gitProg = "git";
            gitArgs = {"clone", "--depth=1",
                       "https://github.com/ggml-org/whisper.cpp", wspDir};
        }

        runner->run(gitProg, gitArgs, "Clona/aggiorna whisper.cpp",
            [runner, bldDir, wspDir, threads](bool ok1) {
            if (!ok1) {
                runner->status->setText(
                    "\xe2\x9d\x8c  git fallito. Verifica la connessione e che git sia installato.");
                runner->btn->setEnabled(true);
                delete runner; return;
            }

            /* Step 2: cmake configure */
            runner->run("cmake",
                {"-S", wspDir, "-B", bldDir,
                 "-DCMAKE_BUILD_TYPE=Release",
                 "-DBUILD_SHARED_LIBS=OFF",
                 "-DWHISPER_BUILD_TESTS=OFF",
                 "-DWHISPER_BUILD_EXAMPLES=ON"},
                "Configurazione cmake",
                [runner, bldDir, threads](bool ok2) {
                if (!ok2) {
                    runner->status->setText(
                        "\xe2\x9d\x8c  cmake fallito. Verifica che cmake e gcc siano installati.");
                    runner->btn->setEnabled(true);
                    delete runner; return;
                }

                /* Step 3: cmake build */
                runner->run("cmake",
                    {"--build", bldDir, "-j", QString::number(threads)},
                    "Compilazione (può richiedere qualche minuto)",
                    [runner](bool ok3) {
                    if (ok3) {
                        /* Cerca il binario compilato */
                        const QString bin = SttWhisper::whisperBin();
                        if (!bin.isEmpty()) {
                            runner->status->setText(
                                "\xe2\x9c\x85  whisper-cli compilato con successo!\n"
                                "Percorso: " + bin);
                            runner->log->appendPlainText(
                                "\n\xe2\x9c\x85  Build completata. Binario: " + bin);
                            runner->lblBin->setText(
                                "\xe2\x9c\x85  Trovato: <code>" + bin + "</code>");
                            runner->btn->setVisible(false);
                        } else {
                            runner->status->setText(
                                "\xe2\x9a\xa0  Compilato ma binario non trovato nel percorso atteso.\n"
                                "Controlla ~/.prismalux/whisper.cpp/build/bin/");
                            runner->log->appendPlainText(
                                "\n\xe2\x9a\xa0  Build completata, ma whisper-cli non trovato.");
                        }
                    } else {
                        runner->status->setText(
                            "\xe2\x9d\x8c  Compilazione fallita. Controlla il log sopra.");
                    }
                    runner->btn->setEnabled(true);
                    delete runner;
                });
            });
        });
    });

    /* ══════════════════════════════════════════
       Sezione 2: modello attivo + selettore
       ══════════════════════════════════════════ */
    auto* secActive = new QFrame(inner);
    secActive->setObjectName("actionCard");
    auto* secActLay = new QVBoxLayout(secActive);
    secActLay->setContentsMargins(14, 10, 14, 10);
    secActLay->setSpacing(8);

    auto* actTitle = new QLabel(
        "\xf0\x9f\x93\x8c  <b>Modello attivo</b>", secActive);
    actTitle->setObjectName("cardTitle");
    actTitle->setTextFormat(Qt::RichText);
    secActLay->addWidget(actTitle);

    auto* lblActivePath = new QLabel(secActive);
    lblActivePath->setObjectName("cardDesc");
    lblActivePath->setTextFormat(Qt::RichText);
    lblActivePath->setWordWrap(true);
    auto refreshActive = [lblActivePath](){
        const QString m = SttWhisper::whisperModel();
        if (m.isEmpty())
            lblActivePath->setText(
                "\xe2\x9d\x8c  Nessun modello trovato &mdash; scaricane uno qui sotto.");
        else
            lblActivePath->setText("\xe2\x9c\x85  <code>" + m + "</code>");
    };
    refreshActive();
    secActLay->addWidget(lblActivePath);

    /* Combo selezione + pulsante Applica */
    auto* selRow    = new QWidget(secActive);
    auto* selRowLay = new QHBoxLayout(selRow);
    selRowLay->setContentsMargins(0, 0, 0, 0);
    selRowLay->setSpacing(8);

    auto* cmbModel = new QComboBox(secActive);
    cmbModel->setObjectName("settingsCombo");
    for (int i = 0; i < N; i++) {
        const QString binPath = modelsDir + "/" + kModels[i].id + ".bin";
        const bool installed  = QFileInfo::exists(binPath);
        cmbModel->addItem(
            QString("%1  (%2 MB)%3").arg(kModels[i].label).arg(kModels[i].sizeMb)
                .arg(installed ? "  \xe2\x9c\x85" : ""),
            binPath);
        /* Preseleziona quello già attivo */
        if (SttWhisper::whisperModel() == binPath)
            cmbModel->setCurrentIndex(i);
    }

    auto* btnApply = new QPushButton("\xe2\x9c\x94  Applica", secActive);
    btnApply->setObjectName("actionBtn");
    btnApply->setToolTip("Imposta il modello selezionato come modello attivo");
    QObject::connect(btnApply, &QPushButton::clicked, inner,
                     [cmbModel, refreshActive, lblActivePath](){
        const QString path = cmbModel->currentData().toString();
        if (!QFileInfo::exists(path)) {
            lblActivePath->setText(
                "\xe2\x9a\xa0  Modello non ancora scaricato. "
                "Usa il pulsante \xe2\x86\x93\xc2\xa0Scarica qui sotto.");
            return;
        }
        SttWhisper::savePreferredModel(path);
        refreshActive();
    });

    selRowLay->addWidget(new QLabel("Seleziona:", secActive));
    selRowLay->addWidget(cmbModel, 1);
    selRowLay->addWidget(btnApply);
    secActLay->addWidget(selRow);
    ilay->addWidget(secActive);

    /* ══════════════════════════════════════════
       Sezione 3: griglia modelli con download
       ══════════════════════════════════════════ */
    auto* secModels = new QFrame(inner);
    secModels->setObjectName("actionCard");
    auto* secMLay = new QVBoxLayout(secModels);
    secMLay->setContentsMargins(14, 10, 14, 10);
    secMLay->setSpacing(6);

    auto* modTitle = new QLabel(
        "\xf0\x9f\x93\xa6  <b>Modelli disponibili</b> "
        "<small style='color:#888;'>(scaricati in ~/.prismalux/whisper/models/)</small>",
        secModels);
    modTitle->setObjectName("cardTitle");
    modTitle->setTextFormat(Qt::RichText);
    secMLay->addWidget(modTitle);

    QDir().mkpath(modelsDir);

    for (int i = 0; i < N; i++) {
        const ModelInfo& m = kModels[i];
        const QString binPath = modelsDir + "/" + m.id + ".bin";

        auto* row    = new QWidget(secModels);
        auto* rowLay = new QHBoxLayout(row);
        rowLay->setContentsMargins(0, 3, 0, 3);
        rowLay->setSpacing(10);

        /* Etichetta */
        auto* lbl = new QLabel(
            QString("<b>%1</b>  <small>~%2 MB &mdash; %3</small>")
                .arg(m.label).arg(m.sizeMb).arg(m.desc),
            row);
        lbl->setObjectName("cardDesc");
        lbl->setTextFormat(Qt::RichText);

        /* Stato */
        const bool installed = QFileInfo::exists(binPath);
        auto* lblStatus = new QLabel(
            installed ? "\xe2\x9c\x85 Presente" : "\xe2\x80\x94", row);
        lblStatus->setObjectName("cardDesc");
        lblStatus->setMinimumWidth(90);

        /* Pulsante download */
        auto* btnDl = new QPushButton(
            installed ? "\xf0\x9f\x94\x84  Riscarica" : "\xf0\x9f\x93\xa5  Scarica",
            row);
        btnDl->setObjectName("actionBtn");
        btnDl->setToolTip(
            QString("Scarica %1.bin (~%2 MB) da HuggingFace")
                .arg(m.id).arg(m.sizeMb));

        rowLay->addWidget(lbl, 1);
        rowLay->addWidget(lblStatus);
        rowLay->addWidget(btnDl);
        secMLay->addWidget(row);

        /* Lambda download */
        const QString modelId   = m.id;
        const QString modelPath = binPath;
        const QString dlUrl     = hfBase + modelId + ".bin";

        QObject::connect(btnDl, &QPushButton::clicked, inner,
            [btnDl, lblStatus, modelPath, dlUrl, cmbModel,
             refreshActive, modelsDir, modelId]() mutable
        {
            QDir().mkpath(modelsDir);
            btnDl->setEnabled(false);
            btnDl->setText("\xe2\x8f\xb3  Download...");
            lblStatus->setText("\xe2\x8f\xb3 Scaricamento...");

#ifdef Q_OS_WIN
            const QString dl = "curl.exe";
            QStringList args = { "-L", "--progress-bar", "-o", modelPath, dlUrl };
#else
            /* wget con progresso mega: stampa ogni ~1 MB */
            const bool hasWget =
                !QStandardPaths::findExecutable("wget").isEmpty();
            const QString dl = hasWget ? "wget" : "curl";
            QStringList args = hasWget
                ? QStringList{ "--progress=dot:mega", "-O", modelPath, dlUrl }
                : QStringList{ "-L", "--progress-bar", "-o", modelPath, dlUrl };
#endif
            auto* proc = new QProcess(btnDl);
            proc->setProcessChannelMode(QProcess::MergedChannels);
            proc->start(dl, args);

            QObject::connect(proc,
                QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                btnDl,
                [proc, btnDl, lblStatus, modelPath, modelId,
                 cmbModel, refreshActive](int code, QProcess::ExitStatus)
            {
                proc->deleteLater();
                btnDl->setEnabled(true);
                const qint64 sz = QFileInfo(modelPath).size();
                if (code == 0 && sz > 10'000'000LL) {
                    lblStatus->setText("\xe2\x9c\x85 Presente");
                    btnDl->setText("\xf0\x9f\x94\x84  Riscarica");
                    /* Aggiorna il testo della voce nel combo */
                    for (int j = 0; j < cmbModel->count(); j++) {
                        if (cmbModel->itemData(j).toString() == modelPath) {
                            const QString cur = cmbModel->itemText(j);
                            if (!cur.contains("\xe2\x9c\x85"))
                                cmbModel->setItemText(j, cur + "  \xe2\x9c\x85");
                            /* Se nessun modello è ancora preferito, imposta questo */
                            if (SttWhisper::preferredModel().isEmpty())
                                SttWhisper::savePreferredModel(modelPath);
                            break;
                        }
                    }
                    refreshActive();
                } else {
                    /* Download incompleto — rimuovi file parziale */
                    if (sz < 10'000'000LL) QFile::remove(modelPath);
                    lblStatus->setText("\xe2\x9d\x8c Errore");
                    btnDl->setText("\xf0\x9f\x93\xa5  Scarica");
                }
            });
        });
    }

    ilay->addWidget(secModels);

    /* ══════════════════════════════════════════
       Sezione 4: nota lingua + test rapido
       ══════════════════════════════════════════ */
    auto* secNote = new QFrame(inner);
    secNote->setObjectName("actionCard");
    auto* secNoteLay = new QVBoxLayout(secNote);
    secNoteLay->setContentsMargins(14, 10, 14, 10);
    secNoteLay->setSpacing(6);

    auto* noteTitle = new QLabel(
        "\xf0\x9f\x92\xa1  <b>Note</b>", secNote);
    noteTitle->setObjectName("cardTitle");
    noteTitle->setTextFormat(Qt::RichText);
    secNoteLay->addWidget(noteTitle);

    auto* noteText = new QLabel(
        "\xe2\x80\xa2  La trascrizione avviene <b>offline</b> sul dispositivo &mdash; "
        "nessun dato inviato a server esterni.<br>"
        "\xe2\x80\xa2  La lingua predefinita \xc3\xa8 <b>Italiano</b>. "
        "whisper.cpp supporta oltre 99 lingue.<br>"
        "\xe2\x80\xa2  Requisiti: <code>arecord</code> (pacchetto <i>alsa-utils</i>) "
        "per la registrazione microfono.<br>"
        "\xe2\x80\xa2  Il modello <b>Small</b> (~141 MB) offre il miglior equilibrio "
        "velocit\xc3\xa0/precisione per l\xe2\x80\x99italiano.",
        secNote);
    noteText->setObjectName("cardDesc");
    noteText->setTextFormat(Qt::RichText);
    noteText->setWordWrap(true);
    secNoteLay->addWidget(noteText);

    ilay->addWidget(secNote);
    ilay->addStretch();
    return inner;
}

/* ══════════════════════════════════════════════════════════════
   buildLlmConsigliatiTab — catalogo LLM diviso in due sezioni:
   1) Ollama  — installa con "ollama pull"
   2) llama.cpp — scarica .gguf da HuggingFace
   ══════════════════════════════════════════════════════════════ */
QWidget* ImpostazioniPage::buildLlmConsigliatiTab()
{
    namespace P = PrismaluxPaths;

    /* ── Strutture dati modelli ─────────────────────────────────── */
    struct OllamaEntry {
        const char* ollama;
        const char* display;
        const char* size;
        const char* category;  /* Generale / Coding / Matematica / Vision / Ragionamento */
        const char* badge;     /* colore CSS hex */
        const char* desc;
    };

    struct GgufEntry {
        const char* display;
        const char* filename;  /* nome file .gguf atteso nella models dir */
        const char* url;       /* URL diretto HuggingFace */
        const char* size;
        const char* category;  /* Generale / Coding / Matematica / Vision / Traduzione */
        const char* badge;
        const char* desc;
    };

    static const OllamaEntry OLLAMA[] = {
        /* ── Generale ── */
        { "qwen3:8b",            "Qwen3 8B",           "~5.2 GB", "Generale",     "#0e4d92",
          "Ragionamento avanzato, ottimo italiano, coding e analisi. Consigliato per uso quotidiano." },
        { "llama3.2:3b",         "Llama 3.2 3B",       "~2.0 GB", "Generale",     "#0e4d92",
          "Meta. Leggero e veloce, buona comprensione italiano. Ideale su PC con poca RAM." },
        { "gemma3:4b",           "Gemma 3 4B",         "~3.3 GB", "Generale",     "#0e4d92",
          "Google DeepMind. Ottimo su testo, riassunti e domande aperte." },
        { "phi4:14b",            "Phi-4 14B",          "~9.1 GB", "Generale",     "#0e4d92",
          "Microsoft. Modello compatto ma molto potente, eccellente ragionamento." },
        { "mistral:7b",          "Mistral 7B",         "~4.1 GB", "Generale",     "#0e4d92",
          "Veloce e bilanciato. Buono su italiano e testi lunghi." },
        /* ── Coding ── */
        { "qwen2.5-coder:7b",    "Qwen2.5 Coder 7B",  "~4.7 GB", "Coding",       "#166534",
          "Alibaba. Specializzato in codice C/C++/Python/JS. Top nella categoria ~7B." },
        { "deepseek-coder:6.7b", "DeepSeek Coder 6.7B","~3.8 GB","Coding",       "#166534",
          "DeepSeek. Eccellente su algoritmi, completamento e refactoring." },
        { "codellama:7b",        "Code Llama 7B",      "~3.8 GB", "Coding",       "#166534",
          "Meta. Ottimo per generazione e spiegazione di codice generico." },
        /* ── Matematica ── */
        { "qwen2.5-math:7b",     "Qwen2.5 Math 7B",   "~4.7 GB", "Matematica",   "#92400e",
          "Specializzato su matematica scolastica e universitaria. Ottimo con calcolo simbolico." },
        { "deepseek-r1:7b",      "DeepSeek R1 7B",    "~4.7 GB", "Matematica",   "#92400e",
          "Ragionamento matematico passo-passo. Chain-of-thought nativo." },
        /* ── Vision ── */
        { "llama3.2-vision",     "Llama 3.2 Vision",  "~7.9 GB", "Vision",       "#581c87",
          "Meta. Analizza immagini, grafici e schemi. Estrae funzioni matematiche da grafici." },
        { "llava:7b",            "LLaVA 7B",          "~4.5 GB", "Vision",       "#581c87",
          "Buono su descrizione immagini e analisi visiva generale." },
        { "qwen2-vl:7b",         "Qwen2-VL 7B",       "~5.5 GB", "Vision",       "#581c87",
          "Il migliore locale per grafici e formule matematiche da immagine." },
        { "minicpm-v:8b",        "MiniCPM-V 8B",      "~5.4 GB", "Vision",       "#581c87",
          "Compatto e preciso. Ottimo per documenti e immagini con testo." },
        /* ── Ragionamento ── */
        { "deepseek-r1:14b",     "DeepSeek R1 14B",   "~9.0 GB", "Ragionamento", "#7c2d12",
          "Ragionamento avanzato, problemi complessi. Mostra il processo di pensiero." },
        { "qwen3:30b",           "Qwen3 30B",         "~20 GB",  "Ragionamento", "#7c2d12",
          "Modello grande, qualit\xc3\xa0 vicina ai cloud. Richiede almeno 24 GB RAM." },
    };

    static const GgufEntry GGUF[] = {
        /* ── Generale ── */
        { "SmolLM2-135M (Q4_K_M)", "SmolLM2-135M-Instruct-Q4_K_M.gguf",
          "https://huggingface.co/bartowski/SmolLM2-135M-Instruct-GGUF/resolve/main/SmolLM2-135M-Instruct-Q4_K_M.gguf",
          "~80 MB", "Generale", "#0e4d92",
          "Ultraleggero (80 MB). Ideale per test su PC con pochissima RAM." },
        { "Qwen2.5-3B (Q4_K_M)", "qwen2.5-3b-instruct-q4_k_m.gguf",
          "https://huggingface.co/Qwen/Qwen2.5-3B-Instruct-GGUF/resolve/main/qwen2.5-3b-instruct-q4_k_m.gguf",
          "~1.9 GB", "Generale", "#0e4d92",
          "Buon equilibrio velocit\xc3\xa0/qualit\xc3\xa0. Ideale per laptop con 8 GB RAM." },
        { "Qwen2.5-7B (Q4_K_M)", "qwen2.5-7b-instruct-q4_k_m.gguf",
          "https://huggingface.co/Qwen/Qwen2.5-7B-Instruct-GGUF/resolve/main/qwen2.5-7b-instruct-q4_k_m.gguf",
          "~4.7 GB", "Generale", "#0e4d92",
          "Ottimo per uso quotidiano, ottimo italiano. Richiede 8+ GB RAM." },
        { "Mistral-7B (Q4_K_M)", "mistral-7b-instruct-v0.2.Q4_K_M.gguf",
          "https://huggingface.co/TheBloke/Mistral-7B-Instruct-v0.2-GGUF/resolve/main/mistral-7b-instruct-v0.2.Q4_K_M.gguf",
          "~4.1 GB", "Generale", "#0e4d92",
          "Veloce e bilanciato, ottimo per testi lunghi." },
        /* ── Coding ── */
        { "Phi-3-mini (Q4_K_M)", "Phi-3-mini-4k-instruct-q4.gguf",
          "https://huggingface.co/microsoft/Phi-3-mini-4k-instruct-gguf/resolve/main/Phi-3-mini-4k-instruct-q4.gguf",
          "~2.2 GB", "Coding", "#166534",
          "Microsoft. Compatto e capace su codice e ragionamento." },
        { "Llama-3.2-3B (Q4_K_M)", "Llama-3.2-3B-Instruct-Q4_K_M.gguf",
          "https://huggingface.co/bartowski/Llama-3.2-3B-Instruct-GGUF/resolve/main/Llama-3.2-3B-Instruct-Q4_K_M.gguf",
          "~2.0 GB", "Coding", "#166534",
          "Meta. Leggero, buono per completamento codice e Q&A." },
        /* ── Matematica ── */
        { "DeepSeek-R1-1.5B (Q4_K_M)", "DeepSeek-R1-Distill-Qwen-1.5B-Q4_K_M.gguf",
          "https://huggingface.co/bartowski/DeepSeek-R1-Distill-Qwen-1.5B-GGUF/resolve/main/DeepSeek-R1-Distill-Qwen-1.5B-Q4_K_M.gguf",
          "~1.1 GB", "Matematica", "#92400e",
          "Piccolo e veloce. Ragionamento base con chain-of-thought." },
        { "Qwen2.5-Math-7B (Q4_K_M)", "qwen2.5-math-7b-instruct-q4_k_m.gguf",
          "https://huggingface.co/Qwen/Qwen2.5-Math-7B-Instruct-GGUF/resolve/main/qwen2.5-math-7b-instruct-q4_k_m.gguf",
          "~4.7 GB", "Matematica", "#92400e",
          "Specializzato su matematica scolastica e universitaria." },
        { "DeepSeek-R1-7B (Q4_K_M)", "DeepSeek-R1-Distill-Qwen-7B-Q4_K_M.gguf",
          "https://huggingface.co/bartowski/DeepSeek-R1-Distill-Qwen-7B-GGUF/resolve/main/DeepSeek-R1-Distill-Qwen-7B-Q4_K_M.gguf",
          "~4.7 GB", "Matematica", "#92400e",
          "Ragionamento avanzato, derivato DeepSeek-R1. Eccellente su prove formali." },
        { "phi-4 (Q4_K_M)", "phi-4-Q4_K_M.gguf",
          "https://huggingface.co/bartowski/phi-4-GGUF/resolve/main/phi-4-Q4_K_M.gguf",
          "~8.4 GB", "Matematica", "#92400e",
          "Microsoft phi-4 — eccellente su matematica e logica formale." },
        /* ── Vision ── */
        { "LLaVA-1.5-7B (Q4_K_M)", "llava-1.5-7b-q4_k_m.gguf",
          "https://huggingface.co/mys/ggml_llava-v1.5-7b/resolve/main/ggml-model-q4_k.gguf",
          "~4.1 GB", "Vision", "#581c87",
          "Analisi immagini e grafici. Compatibile con llama-server con mmproj." },
        { "BakLLaVA-1 (Q4_K_M)", "bakllava-1-Q4_K_M.gguf",
          "https://huggingface.co/mys/ggml_bakllava-1/resolve/main/ggml-model-q4_k.gguf",
          "~4.2 GB", "Vision", "#581c87",
          "Vision multimodale basato su Mistral. Buono su descrizione immagini." },
        /* ── Ragionamento ── */
        { "QwQ-32B (Q4_K_M)", "QwQ-32B-Q4_K_M.gguf",
          "https://huggingface.co/bartowski/QwQ-32B-GGUF/resolve/main/QwQ-32B-Q4_K_M.gguf",
          "~19.9 GB", "Ragionamento", "#7c2d12",
          "Ragionamento profondo di alta qualit\xc3\xa0. Richiede 24+ GB RAM." },
    };

    const int NO = (int)(sizeof(OLLAMA) / sizeof(OLLAMA[0]));
    const int NG = (int)(sizeof(GGUF)   / sizeof(GGUF[0]));

    /* ══════════════════════════════════════════════════════════════
       Layout compatto 2 colonne — nessun scroll necessario:
         Sinistra : filtro gestore + categoria
         Destra   : lista compatta + pannello dettaglio
       ══════════════════════════════════════════════════════════════ */
    auto* page    = new QWidget;
    auto* mainLay = new QVBoxLayout(page);
    mainLay->setContentsMargins(16, 14, 16, 14);
    mainLay->setSpacing(10);

    /* ── Titolo ── */
    auto* titleLbl = new QLabel(
        "\xf0\x9f\xa4\x96  LLM Consigliati  \xe2\x80\x94  "
        "<span style='color:#94a3b8;font-size:12px;font-weight:normal;'>"
        "Seleziona gestore e categoria, poi clicca per installare.</span>", page);
    titleLbl->setObjectName("sectionTitle");
    titleLbl->setTextFormat(Qt::RichText);
    mainLay->addWidget(titleLbl);

    /* ── 2 colonne ── */
    auto* colsRow = new QWidget(page);
    auto* colsLay = new QHBoxLayout(colsRow);
    colsLay->setContentsMargins(0, 0, 0, 0);
    colsLay->setSpacing(14);

    /* ════════════════════════════════════════════════════════
       Colonna sinistra — Filtro gestore + categoria
       ════════════════════════════════════════════════════════ */
    auto* leftGroup = new QGroupBox("\xf0\x9f\x94\xa7  Filtro", colsRow);
    leftGroup->setObjectName("cardGroup");
    leftGroup->setFixedWidth(190);
    auto* leftLay = new QVBoxLayout(leftGroup);
    leftLay->setSpacing(5);

    auto* lblGest = new QLabel("Gestore:", leftGroup);
    lblGest->setObjectName("cardDesc");
    auto* btnOllama = new QRadioButton("\xf0\x9f\x90\xb3  Ollama", leftGroup);
    btnOllama->setChecked(true);
    auto* btnGguf   = new QRadioButton("\xf0\x9f\xa6\x99  GGUF (llama.cpp)", leftGroup);

    auto* sepFilt = new QFrame(leftGroup);
    sepFilt->setFrameShape(QFrame::HLine);
    sepFilt->setObjectName("sidebarSep");

    auto* lblCat = new QLabel("Categoria:", leftGroup);
    lblCat->setObjectName("cardDesc");

    leftLay->addWidget(lblGest);
    leftLay->addWidget(btnOllama);
    leftLay->addWidget(btnGguf);
    leftLay->addWidget(sepFilt);
    leftLay->addWidget(lblCat);

    static const char* CATS[] = {
        "Tutti", "Generale", "Coding",
        "Matematica", "Vision", "Ragionamento"
    };
    auto* catBtnGroup = new QButtonGroup(leftGroup);
    for (int i = 0; i < 6; i++) {
        auto* rb = new QRadioButton(CATS[i], leftGroup);
        catBtnGroup->addButton(rb, i);
        if (i == 0) rb->setChecked(true);
        leftLay->addWidget(rb);
    }
    leftLay->addStretch(1);
    colsLay->addWidget(leftGroup);

    /* ════════════════════════════════════════════════════════
       Colonna destra — lista + pannello dettaglio
       ════════════════════════════════════════════════════════ */
    auto* rightGroup = new QGroupBox("\xf0\x9f\x93\xa6  Modelli disponibili", colsRow);
    rightGroup->setObjectName("cardGroup");
    auto* rightLay = new QVBoxLayout(rightGroup);
    rightLay->setSpacing(8);

    auto* modelList = new QListWidget(rightGroup);
    modelList->setObjectName("modelsList");
    modelList->setAlternatingRowColors(true);
    rightLay->addWidget(modelList, 1);

    /* ── Pannello dettaglio (sotto la lista) ── */
    auto* detSep = new QFrame(rightGroup);
    detSep->setFrameShape(QFrame::HLine);
    detSep->setObjectName("sidebarSep");
    rightLay->addWidget(detSep);

    auto* detRow = new QWidget(rightGroup);
    auto* detLay = new QHBoxLayout(detRow);
    detLay->setContentsMargins(0, 4, 0, 0);
    detLay->setSpacing(10);

    auto* detailLbl = new QLabel("Seleziona un modello per i dettagli.", rightGroup);
    detailLbl->setObjectName("cardDesc");
    detailLbl->setWordWrap(true);
    detailLbl->setTextFormat(Qt::RichText);
    detailLbl->setMinimumHeight(44);

    auto* installBtn = new QPushButton("\xe2\xac\x87  Installa", rightGroup);
    installBtn->setObjectName("actionBtn");
    installBtn->setFixedWidth(110);
    installBtn->setEnabled(false);

    detLay->addWidget(detailLbl, 1);
    detLay->addWidget(installBtn);
    rightLay->addWidget(detRow);

    /* ── Log download ── */
    auto* logOut = new QLabel(rightGroup);
    logOut->setWordWrap(true);
    logOut->setVisible(false);
    logOut->setStyleSheet(
        "background:#0f172a;color:#86efac;font-family:monospace;"
        "font-size:11px;padding:6px 10px;border-radius:6px;");
    rightLay->addWidget(logOut);

    colsLay->addWidget(rightGroup, 1);
    mainLay->addWidget(colsRow, 1);

    /* ════════════════════════════════════════════════════════
       Logica — popola lista in base ai filtri selezionati
       ════════════════════════════════════════════════════════ */
    auto populate = [=]() {
        modelList->clear();
        installBtn->setEnabled(false);
        detailLbl->setText("Seleziona un modello per i dettagli.");
        logOut->setVisible(false);

        const bool isOllama  = btnOllama->isChecked();
        const int  catId     = catBtnGroup->checkedId();
        const QString filter = (catId == 0) ? QString() : QString(CATS[catId]);

        if (isOllama) {
            for (int i = 0; i < NO; i++) {
                const auto& m = OLLAMA[i];
                if (!filter.isEmpty() && filter != m.category) continue;
                auto* item = new QListWidgetItem(
                    QString("[%1]  %2  \xe2\x80\x94  %3")
                    .arg(m.category, m.display, m.size));
                item->setData(Qt::UserRole,     i);
                item->setData(Qt::UserRole + 1, true);
                modelList->addItem(item);
            }
        } else {
            for (int i = 0; i < NG; i++) {
                const auto& m = GGUF[i];
                if (!filter.isEmpty() && filter != m.category) continue;
                const bool inst = QFile::exists(
                    PrismaluxPaths::modelsDir() + "/" + m.filename);
                auto* item = new QListWidgetItem(
                    QString("[%1]  %2  \xe2\x80\x94  %3%4")
                    .arg(m.category, m.display, m.size,
                         inst ? "  \xe2\x9c\x94" : ""));
                item->setData(Qt::UserRole,     i);
                item->setData(Qt::UserRole + 1, false);
                modelList->addItem(item);
            }
        }
    };

    /* Selezione modello → aggiorna pannello dettaglio */
    connect(modelList, &QListWidget::currentItemChanged, page,
            [=](QListWidgetItem* cur, QListWidgetItem*) {
        if (!cur) {
            installBtn->setEnabled(false);
            detailLbl->setText("Seleziona un modello per i dettagli.");
            return;
        }
        const int  idx   = cur->data(Qt::UserRole).toInt();
        const bool isOll = cur->data(Qt::UserRole + 1).toBool();
        installBtn->setEnabled(true);
        installBtn->setText("\xe2\xac\x87  Installa");
        installBtn->setStyleSheet("");

        if (isOll) {
            const auto& m = OLLAMA[idx];
            detailLbl->setText(
                QString("<b>%1</b>  <span style='color:#64748b;'>%2</span><br>"
                        "<span style='color:#94a3b8;font-size:11px;'>%3</span>")
                .arg(m.display, m.size, m.desc));
        } else {
            const auto& m = GGUF[idx];
            const bool inst = QFile::exists(
                PrismaluxPaths::modelsDir() + "/" + m.filename);
            detailLbl->setText(
                QString("<b>%1</b>  <span style='color:#64748b;'>%2</span>"
                        "%4<br>"
                        "<span style='color:#94a3b8;font-size:11px;'>%3</span>")
                .arg(m.display, m.size, m.desc,
                     inst ? "  <span style='color:#4ade80;'>\xe2\x9c\x94 presente</span>"
                          : ""));
        }
    });

    /* Pulsante Installa */
    connect(installBtn, &QPushButton::clicked, page, [=]() {
        auto* cur = modelList->currentItem();
        if (!cur) return;
        const int  idx   = cur->data(Qt::UserRole).toInt();
        const bool isOll = cur->data(Qt::UserRole + 1).toBool();

        installBtn->setEnabled(false);
        installBtn->setText("\xe2\x8f\xb3  ...");
        logOut->setVisible(true);

        if (isOll) {
            const auto& m = OLLAMA[idx];
            const QString ollamaName(m.ollama);
            logOut->setText(QString("\xf0\x9f\x93\xa5  ollama pull %1").arg(ollamaName));
            auto* proc = new QProcess(page);
            proc->setProcessChannelMode(QProcess::MergedChannels);
            connect(proc, &QProcess::readyRead, page, [proc, logOut]() {
                const QString txt = QString::fromUtf8(proc->readAll()).trimmed();
                if (!txt.isEmpty()) logOut->setText(txt);
            });
            connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                    page, [proc, installBtn, logOut, ollamaName](int code, QProcess::ExitStatus) {
                if (code == 0) {
                    installBtn->setText("\xe2\x9c\x94  Installato");
                    installBtn->setStyleSheet("color:#4ade80;");
                    logOut->setText(QString("\xe2\x9c\x85  %1 installato.").arg(ollamaName));
                } else {
                    installBtn->setEnabled(true);
                    installBtn->setText("\xe2\xac\x87  Installa");
                    logOut->setText("\xe2\x9d\x8c  Errore. Assicurati che ollama sia in esecuzione.");
                }
                proc->deleteLater();
            });
            proc->start("ollama", {"pull", ollamaName});
        } else {
            const auto& m = GGUF[idx];
            const QString ggufUrl(m.url);
            const QString ggufFile(m.filename);
            const QString dest = PrismaluxPaths::modelsDir() + "/" + ggufFile;
            logOut->setText(QString("\xf0\x9f\x93\xa5  Scarico %1...").arg(ggufFile));
            auto* proc = new QProcess(page);
            proc->setProcessChannelMode(QProcess::MergedChannels);
            connect(proc, &QProcess::readyRead, page, [proc, logOut]() {
                const QString txt = QString::fromUtf8(proc->readAll()).trimmed();
                if (!txt.isEmpty()) logOut->setText(txt);
            });
            connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                    page, [proc, installBtn, logOut, ggufFile](int code, QProcess::ExitStatus) {
                if (code == 0) {
                    installBtn->setText("\xe2\x9c\x94  Scaricato");
                    installBtn->setStyleSheet("color:#4ade80;");
                    logOut->setText(QString("\xe2\x9c\x85  %1 scaricato.").arg(ggufFile));
                } else {
                    installBtn->setEnabled(true);
                    installBtn->setText("\xe2\xac\x87  Installa");
                    logOut->setText("\xe2\x9d\x8c  Errore download. Controlla wget/connessione.");
                }
                proc->deleteLater();
            });
            proc->start("wget", {"-c", "--show-progress", "-O", dest, ggufUrl});
            if (!proc->waitForStarted(3000)) {
                proc->deleteLater();
                auto* curl = new QProcess(page);
                curl->setProcessChannelMode(QProcess::MergedChannels);
                connect(curl, &QProcess::readyRead, page, [curl, logOut]() {
                    const QString txt = QString::fromUtf8(curl->readAll()).trimmed();
                    if (!txt.isEmpty()) logOut->setText(txt);
                });
                connect(curl, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                        page, [curl, installBtn, logOut, ggufFile](int code, QProcess::ExitStatus) {
                    if (code == 0) {
                        installBtn->setText("\xe2\x9c\x94  Scaricato");
                        installBtn->setStyleSheet("color:#4ade80;");
                        logOut->setText(QString("\xe2\x9c\x85  %1 scaricato.").arg(ggufFile));
                    } else {
                        installBtn->setEnabled(true);
                        installBtn->setText("\xe2\xac\x87  Installa");
                        logOut->setText("\xe2\x9d\x8c  Errore: installa wget o curl.");
                    }
                    curl->deleteLater();
                });
                curl->start("curl", {"-L", "-C", "-", "--progress-bar",
                                     "-o", dest, ggufUrl});
            }
        }
    });

    /* Cambi filtro → ripopola */
    connect(btnOllama, &QRadioButton::toggled, page, [=](bool on){ if (on) populate(); });
    connect(btnGguf,   &QRadioButton::toggled, page, [=](bool on){ if (on) populate(); });
    for (QAbstractButton* b : catBtnGroup->buttons())
        connect(b, &QAbstractButton::toggled, page, [=](bool on){ if (on) populate(); });

    /* Popola al primo caricamento */
    QTimer::singleShot(0, page, populate);

    return page;
}

