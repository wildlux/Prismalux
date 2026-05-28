#include "chat_page.h"
#include "../ai_client.h"
#include "../rag_engine_simple.h"
#include "../local_llm_client.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollBar>
#include <QScrollArea>
#include <QFrame>
#include <QKeyEvent>
#include <QJsonObject>
#include <QJsonDocument>
#include <QTimer>
#include <QFont>
#include <QScreen>
#include <QGuiApplication>
#include <QClipboard>
#include <QApplication>
#include <QScroller>
#include <QScrollerProperties>
#include <QSettings>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QGroupBox>
#include <QDateTime>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>

/* ══════════════════════════════════════════════════════════════
   ChatBubbleWidget — bolla singola messaggio
   ══════════════════════════════════════════════════════════════ */
ChatBubbleWidget::ChatBubbleWidget(const QString& role,
                                   const QString& text,
                                   QWidget* parent)
    : QFrame(parent), m_fullText(text)
{
    const bool isAi   = (role == "ai");
    const bool isUser = (role == "user");

    setObjectName(isUser ? "UserBubble" : isAi ? "AiBubble" : "SystemBubble");
    setFrameShape(QFrame::StyledPanel);

    if (isUser)
        setMaximumWidth(300);
    else
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(10, 8, 10, isAi ? 2 : 8);
    vbox->setSpacing(2);

    const QString prefix = isAi
        ? QString::fromUtf8("\xf0\x9f\xa4\x96  ")  /* 🤖 */
        : QString();

    m_textLbl = new QLabel(prefix + text, this);
    m_textLbl->setWordWrap(true);
    m_textLbl->setTextFormat(Qt::PlainText);
    m_textLbl->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_textLbl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    {
        QFont f = m_textLbl->font();
        f.setPointSize(12);
        m_textLbl->setFont(f);
    }
    vbox->addWidget(m_textLbl);

    /* Azioni inline — solo per bolle AI, nascoste finché la risposta non è completa */
    if (isAi) {
        auto* actRow = new QHBoxLayout;
        actRow->setSpacing(2);
        actRow->setContentsMargins(0, 4, 0, 4);
        actRow->addStretch();

        m_copyBtn = new QPushButton(
            QString::fromUtf8("\xf0\x9f\x93\x8b"), this);   /* 📋 */
        m_copyBtn->setObjectName("BubbleActionBtn");
        m_copyBtn->setFixedSize(36, 36);
        m_copyBtn->setToolTip("Copia risposta");
        m_copyBtn->setVisible(false);

        m_speakBtn = new QPushButton(
            QString::fromUtf8("\xf0\x9f\x94\x8a"), this);   /* 🔊 */
        m_speakBtn->setObjectName("BubbleActionBtn");
        m_speakBtn->setFixedSize(36, 36);
        m_speakBtn->setToolTip("Leggi ad alta voce");
        m_speakBtn->setVisible(false);

        actRow->addWidget(m_copyBtn);
        actRow->addWidget(m_speakBtn);

        connect(m_copyBtn,  &QPushButton::clicked,
                this,       &ChatBubbleWidget::onCopyClicked);
        connect(m_speakBtn, &QPushButton::clicked,
                this,       &ChatBubbleWidget::onSpeakClicked);

        vbox->addLayout(actRow);
    }
}

void ChatBubbleWidget::appendText(const QString& chunk)
{
    m_fullText += chunk;
    m_textLbl->setText(
        QString::fromUtf8("\xf0\x9f\xa4\x96  ") + m_fullText);  /* 🤖 */
}

void ChatBubbleWidget::showActions()
{
    if (m_copyBtn)  m_copyBtn->setVisible(true);
    if (m_speakBtn) m_speakBtn->setVisible(true);
}

void ChatBubbleWidget::onCopyClicked()
{
    emit copyClicked(m_fullText);
    if (!m_copyBtn) return;
    m_copyBtn->setText(QString::fromUtf8("\xe2\x9c\x85"));   /* ✅ */
    m_copyBtn->setEnabled(false);
    QTimer::singleShot(2000, this, &ChatBubbleWidget::onCopyRestored);
}

void ChatBubbleWidget::onCopyRestored()
{
    if (!m_copyBtn) return;
    m_copyBtn->setText(QString::fromUtf8("\xf0\x9f\x93\x8b"));  /* 📋 */
    m_copyBtn->setEnabled(true);
}

void ChatBubbleWidget::onSpeakClicked()
{
    emit speakClicked(m_fullText);
}

/* ══════════════════════════════════════════════════════════════
   ModelPickerDialog — implementazione
   ══════════════════════════════════════════════════════════════ */
ModelPickerDialog::ModelPickerDialog(const QStringList& models,
                                     const QString& current,
                                     const QString& srvEmoji,
                                     const QMap<QString, qint64>& sizes,
                                     QWidget* parent)
    : QDialog(parent, Qt::Dialog)
{
    setWindowTitle("Scegli modello LLM");
    showMaximized();

    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(16, 16, 16, 16);
    vbox->setSpacing(12);

    auto* lbl = new QLabel(
        QString::fromUtf8("\xf0\x9f\xa4\x96")
        + "  Seleziona il modello LLM\n"
          "(tap su un modello per sceglierlo)",
        this);
    lbl->setAlignment(Qt::AlignCenter);
    QFont f = lbl->font();
    f.setPointSize(13);
    lbl->setFont(f);
    vbox->addWidget(lbl);

    /* Legenda emoji */
    auto* legendLbl = new QLabel(
        QString::fromUtf8(
            "\xf0\x9f\x93\xb1") + " telefono   "  /* 📱 */
        + QString::fromUtf8("\xf0\x9f\x93\xb6") + " rete LAN   "  /* 📶 */
        + QString::fromUtf8("\xe2\x98\x81\xef\xb8\x8f") + " cloud",   /* ☁️ */
        this);
    legendLbl->setAlignment(Qt::AlignCenter);
    QFont lf = legendLbl->font();
    lf.setPointSize(11);
    legendLbl->setFont(lf);
    vbox->addWidget(legendLbl);

    m_list = new QListWidget(this);
    m_list->setObjectName("ModelList");

    /* Modelli locali sul telefono (scaricati con LocalLlmClient) */
    const QString phoneEmoji = QString::fromUtf8("\xf0\x9f\x93\xb1");  /* 📱 */
    for (int i = 0; i < LocalLlmClient::kNumModels; ++i) {
        if (!LocalLlmClient::modelExists(i)) continue;
        const QString name = QString::fromUtf8(LocalLlmClient::kModels[i].filename)
                                 .section('.', 0, -2);   /* rimuove .gguf */
        const QString display = phoneEmoji + "  " + name;
        auto* item = new QListWidgetItem(display, m_list);
        item->setData(Qt::UserRole, name);   /* nome pulito */
        item->setSizeHint(QSize(0, 56));
        if (name == current || current.contains(name)) {
            QFont bf = item->font(); bf.setBold(true); item->setFont(bf);
            item->setText(phoneEmoji + "  " + QString::fromUtf8("\xe2\x9c\x94 ") + name);
        }
    }

    /* Modelli dal server (LAN o cloud).
       size==0 significa modello cloud Ollama → forza ☁️ indipendentemente dal server. */
    const QString cloudEmoji = QString::fromUtf8("\xe2\x98\x81\xef\xb8\x8f");  /* ☁️ */
    for (const QString& m : models) {
        const bool isCloud   = (sizes.value(m, 1) == 0);
        const QString& emoji = isCloud ? cloudEmoji : srvEmoji;
        const QString display = emoji + "  " + m;
        auto* item = new QListWidgetItem(display, m_list);
        item->setData(Qt::UserRole, m);   /* nome pulito per AiClient */
        item->setSizeHint(QSize(0, 56));
        if (m == current) {
            QFont bf = item->font(); bf.setBold(true); item->setFont(bf);
            item->setText(emoji + "  " + QString::fromUtf8("\xe2\x9c\x94 ") + m);
        }
    }

    vbox->addWidget(m_list, 1);

    auto* btnCancel = new QPushButton(
        QString::fromUtf8("\xe2\x9c\x96  Annulla"), this);
    btnCancel->setObjectName("SecondaryBtn");
    btnCancel->setMinimumHeight(48);
    vbox->addWidget(btnCancel);

    connect(m_list,    &QListWidget::itemClicked,
            this,      &ModelPickerDialog::onItemClicked);
    connect(btnCancel, &QPushButton::clicked,
            this,      &QDialog::reject);
}

void ModelPickerDialog::onItemClicked(QListWidgetItem* item)
{
    if (!item) return;
    /* Il nome pulito è sempre in Qt::UserRole, indipendente dagli emoji */
    const QString name = item->data(Qt::UserRole).toString();
    if (name.isEmpty()) return;
    m_selected = name;
    emit modelPicked(name);
    accept();
}

/* ══════════════════════════════════════════════════════════════
   Costruttore
   ══════════════════════════════════════════════════════════════ */
ChatPage::ChatPage(AiClient* ai, RagEngineSimple* rag, QWidget* parent)
    : QWidget(parent), m_ai(ai), m_rag(rag)
{
    setObjectName("ChatPage");

    /* ── Header ── */
    auto* header = new QHBoxLayout;
    const QString curModel = m_ai->model().isEmpty() ? "llama3.2:1b" : m_ai->model();
    m_modelList << curModel;
    m_modelBtn = new QPushButton(
        serverEmoji(m_ai->host(), m_ai->port()) + " " + curModel, this);
    m_modelBtn->setObjectName("ModelBtn");
    m_modelBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_modelBtn->setMinimumHeight(40);

    m_stopBtn         = new QPushButton("\xe2\x9c\x95  Stop", this);   // ✕
    m_clearBtn        = new QPushButton("\xf0\x9f\x97\x91", this);     // 🗑
    m_clearHistoryBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x97\x83"), this);                  // 🗃 Cancella cronologia DB
    m_stopBtn->setEnabled(false);
    m_stopBtn->setObjectName("StopBtn");
    m_clearBtn->setObjectName("IconBtn");
    m_clearHistoryBtn->setObjectName("IconBtn");
    m_clearHistoryBtn->setToolTip("Cancella cronologia chat dal database");
    header->addWidget(m_modelBtn, 1);
    header->addWidget(m_stopBtn);
    header->addWidget(m_clearBtn);
    header->addWidget(m_clearHistoryBtn);

    /* ── Selettore backend: Cloud ☁️ | Server 🌐 | Locale 📱 ── */
    m_cloudBtn  = new QPushButton(
        QString::fromUtf8("\xe2\x98\x81\xef\xb8\x8f") + " Cloud", this);          /* ☁️ */
    m_serverBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x8c\x90") + " Server locale", this);   /* 🌐 */
    m_localBtn  = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x93\xb1") + " Modello locale", this);  /* 📱 */
    m_cloudBtn->setObjectName("BackendCloudBtn");
    m_serverBtn->setObjectName("BackendServerBtn");
    m_localBtn->setObjectName("BackendLocalBtn");
    m_cloudBtn->setCheckable(true);
    m_serverBtn->setCheckable(true);
    m_localBtn->setCheckable(true);
    m_cloudBtn->setMinimumHeight(40);
    m_serverBtn->setMinimumHeight(40);
    m_localBtn->setMinimumHeight(40);
    {
        const bool localNow  = m_ai->isLocalMode();
        auto*      llm       = m_ai->localLlm();
        const bool llmReady  = llm && llm->isLoaded();
        m_serverBtn->setChecked(!localNow);
        m_localBtn->setChecked(localNow);
        m_localBtn->setEnabled(llmReady);
        if (!llmReady)
            m_localBtn->setToolTip(
                QString::fromUtf8("\xe2\x9a\xa0\xef\xb8\x8f")  /* ⚠️ */
                + " Scarica prima il modello locale dalle Impostazioni");
        if (localNow && llmReady)
            m_modelBtn->setText(
                QString::fromUtf8("\xf0\x9f\x93\xb1 ") + "qwen2.5-1.5b");
    }
    /* Gruppo esclusivo: uno solo attivo alla volta */
    m_backendGroup = new QButtonGroup(this);
    m_backendGroup->setExclusive(true);
    m_backendGroup->addButton(m_cloudBtn);
    m_backendGroup->addButton(m_serverBtn);
    m_backendGroup->addButton(m_localBtn);

    auto* backendRow = new QHBoxLayout;
    backendRow->setSpacing(4);
    backendRow->addWidget(m_cloudBtn,  1);
    backendRow->addWidget(m_serverBtn, 1);
    backendRow->addWidget(m_localBtn,  1);

    /* ── Banner backend attivo ── */
    m_backendStatusLbl = new QLabel(this);
    m_backendStatusLbl->setObjectName("BackendStatusLbl");
    m_backendStatusLbl->setAlignment(Qt::AlignCenter);
    m_backendStatusLbl->setFixedHeight(28);
    {
        const bool localNow = m_ai->isLocalMode();
        if (localNow) {
            m_backendStatusLbl->setText(
                QString::fromUtf8("\xf0\x9f\x93\xb1")   /* 📱 */
                + "  LLM LOCALE ATTIVO");
            m_backendStatusLbl->setProperty("backend", "local");
        } else {
            m_backendStatusLbl->setText(
                QString::fromUtf8("\xf0\x9f\x8c\x90")   /* 🌐 */
                + "  OLLAMA SERVER ATTIVO");
            m_backendStatusLbl->setProperty("backend", "server");
        }
    }

    /* ── Area chat a bolle ── */
    m_chatContainer = new QWidget;
    m_chatContainer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    m_chatLayout = new QVBoxLayout(m_chatContainer);
    m_chatLayout->setAlignment(Qt::AlignTop);
    m_chatLayout->setSpacing(8);
    m_chatLayout->setContentsMargins(6, 6, 6, 6);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setObjectName("ChatScrollArea");
    m_scrollArea->setWidget(m_chatContainer);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    /* Scroll kinetic a un dito (Material-like) */
    QScroller::grabGesture(m_scrollArea->viewport(), QScroller::TouchGesture);
    {
        QScroller* qs = QScroller::scroller(m_scrollArea->viewport());
        QScrollerProperties sp = qs->scrollerProperties();
        sp.setScrollMetric(QScrollerProperties::OvershootDragDistanceFactor, 0.0);
        sp.setScrollMetric(QScrollerProperties::OvershootScrollDistanceFactor, 0.0);
        sp.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy,
            QVariant::fromValue<QScrollerProperties::OvershootPolicy>(
                QScrollerProperties::OvershootAlwaysOff));
        sp.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy,
            QVariant::fromValue<QScrollerProperties::OvershootPolicy>(
                QScrollerProperties::OvershootAlwaysOff));
        qs->setScrollerProperties(sp);
    }

#ifdef HAVE_TTS
    m_tts = new QTextToSpeech(this);
    m_tts->setLocale(QLocale(QLocale::Italian, QLocale::Italy));
#endif

    /* ── Allega file (📎) ── */
    m_ragBtn = new QPushButton("\xf0\x9f\x93\x8e", this);  // 📎
    m_ragBtn->setObjectName("RagToggle");
    m_ragBtn->setToolTip("Allega file al messaggio (max 5)");

    /* ── Input ── */
    m_input   = new QLineEdit(this);
    m_input->setPlaceholderText("Scrivi un messaggio…");
    m_input->setObjectName("ChatInput");
    m_sendBtn = new QPushButton("\xe2\x86\x91", this);  // ↑ (U+2191, universale)
    m_sendBtn->setObjectName("SendBtn");
    m_sendBtn->setFixedWidth(52);

    auto* inputRow = new QHBoxLayout;
    inputRow->addWidget(m_ragBtn);
    inputRow->addWidget(m_input, 1);
    inputRow->addWidget(m_sendBtn);

    /* ── Layout principale ── */
    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(8, 8, 8, 8);
    vbox->setSpacing(6);
    vbox->addLayout(header);
    vbox->addLayout(backendRow);
    vbox->addWidget(m_backendStatusLbl);
    vbox->addWidget(m_scrollArea, 1);
    vbox->addLayout(inputRow);

    /* ── Connessioni ── */
    connect(m_sendBtn,         &QPushButton::clicked,  this, &ChatPage::onSend);
    connect(m_stopBtn,         &QPushButton::clicked,  this, &ChatPage::onStop);
    connect(m_clearBtn,        &QPushButton::clicked,  this, &ChatPage::onClear);
    connect(m_clearHistoryBtn, &QPushButton::clicked,  this, &ChatPage::onClearHistory);
    connect(m_modelBtn, &QPushButton::clicked,  this, &ChatPage::onModelBtnClicked);
    connect(m_ragBtn,   &QPushButton::clicked,  this, &ChatPage::onAttachClicked);
    connect(m_input,    &QLineEdit::returnPressed, this, &ChatPage::onSend);

    connect(m_cloudBtn,  &QPushButton::clicked, this, &ChatPage::onCloudModeClicked);
    connect(m_serverBtn, &QPushButton::clicked, this, &ChatPage::onBackendServer);
    connect(m_localBtn,  &QPushButton::clicked, this, &ChatPage::onBackendLocal);

    connect(m_ai, &AiClient::token,            this, &ChatPage::onToken);
    connect(m_ai, &AiClient::finished,         this, &ChatPage::onFinished);
    connect(m_ai, &AiClient::error,            this, &ChatPage::onError);
    connect(m_ai, &AiClient::modelChanged,     this, &ChatPage::onExternalModelChanged);
    connect(m_ai, &AiClient::modelsReady,      this, &ChatPage::onModelsReady);
    connect(m_ai, &AiClient::localModeChanged, this, &ChatPage::onAiLocalModeChanged);

    /* Abilita il pulsante Locale quando il modello viene caricato */
    if (auto* llm = m_ai->localLlm())
        connect(llm, &LocalLlmClient::modelLoaded,
                this, &ChatPage::onLocalModelLoaded);

    /* Popola la lista modelli all'avvio (asincrono, non blocca la UI) */
    QTimer::singleShot(800, this, &ChatPage::fetchModels);

    /* Inizializza SQLite e carica la cronologia persistente */
    initDb();
    loadHistoryFromDb();
}

/* ══════════════════════════════════════════════════════════════
   buildSystemPrompt — costruisce il system prompt con RAG
   ══════════════════════════════════════════════════════════════ */
QString ChatPage::buildSystemPrompt(const QString& userMsg) const
{
    QString sys = "Sei un assistente AI locale. Rispondi in italiano, "
                  "in modo preciso e conciso.";

    /* Contesto da Camera (documento scansionato) */
    if (!m_pendingContext.isEmpty()) {
        sys += "\n\nCONTESTO DOCUMENTO SCANSIONATO:\n" + m_pendingContext;
        return sys;
    }

    /* File allegati dall'utente (📎) — hanno priorità sul RAG */
    if (!m_attachedContents.isEmpty()) {
        sys += "\n\nFILE ALLEGATI DALL'UTENTE:";
        for (int i = 0; i < m_attachedContents.size(); ++i) {
            sys += QString("\n\n--- File %1: %2 ---\n%3")
                   .arg(i + 1)
                   .arg(m_attachedNames[i])
                   .arg(m_attachedContents[i]);
        }
        sys += "\n\nAnalizza i file sopra e rispondi alla domanda dell'utente.";
        return sys;
    }

    /* RAG locale: inietta i chunk rilevanti se disponibili */
    if (m_rag->chunkCount() > 0) {
        const QStringList chunks = m_rag->searchForPrompt(userMsg, 4);
        if (!chunks.isEmpty()) {
            sys += "\n\nCONTESTO DAI TUOI DOCUMENTI:\n";
            for (int i = 0; i < chunks.size(); ++i)
                sys += QString("[%1] %2\n\n").arg(i + 1).arg(chunks[i]);
            sys += "Usa il contesto sopra per rispondere. "
                   "Se non è rilevante, ignora e rispondi comunque.";
        }
    }
    return sys;
}

/* ══════════════════════════════════════════════════════════════
   onSend
   ══════════════════════════════════════════════════════════════ */
void ChatPage::onSend()
{
    const QString msg = m_input->text().trimmed();
    if (msg.isEmpty() || m_streaming) return;

    m_input->clear();
    m_streaming = true;
    m_stopBtn->setEnabled(true);
    m_sendBtn->setEnabled(false);

    /* Bolla utente */
    appendBubble("user", msg);

    /* Spazio per la risposta in streaming */
    appendStreamBlock();

    /* Salva il messaggio per la history (usato in onFinished) */
    m_lastUserMsg = msg;

    /* Avvia la richiesta */
    const QString sys = buildSystemPrompt(msg);
    m_pendingContext.clear();   // consumato
    m_ai->chat(sys, msg, m_history);

    emit queryStarted();
}

/* ══════════════════════════════════════════════════════════════
   onToken — aggiorna la bolla AI in tempo reale (streaming)
   ══════════════════════════════════════════════════════════════ */
void ChatPage::onToken(const QString& chunk)
{
    if (!m_streamBubble) return;
    m_streamBubble->appendText(chunk);
    QTimer::singleShot(0, this, &ChatPage::scrollToBottom);
}

/* ══════════════════════════════════════════════════════════════
   onFinished
   ══════════════════════════════════════════════════════════════ */
void ChatPage::onFinished(const QString& full)
{
    m_streaming = false;
    m_stopBtn->setEnabled(false);
    m_sendBtn->setEnabled(true);

    if (!full.isEmpty() && !m_lastUserMsg.isEmpty()) {
        QJsonObject u; u["role"] = "user";      u["content"] = m_lastUserMsg;
        QJsonObject a; a["role"] = "assistant"; a["content"] = full;
        m_history.append(u);
        m_history.append(a);
        while (m_history.size() > kMaxHistoryTurns * 2)
            m_history.removeAt(0);
        /* Persiste entrambi i messaggi su SQLite */
        saveMessageToDb("user",      m_lastUserMsg);
        saveMessageToDb("assistant", full);
        /* Mostra azioni inline nella bolla corrente */
        if (m_streamBubble) m_streamBubble->showActions();
    }
    m_lastUserMsg.clear();
    m_streamBubble = nullptr;
    emit queryFinished();
}

/* ══════════════════════════════════════════════════════════════
   onError
   ══════════════════════════════════════════════════════════════ */
void ChatPage::onError(const QString& msg)
{
    m_streaming = false;
    m_stopBtn->setEnabled(false);
    m_sendBtn->setEnabled(true);
    m_lastUserMsg.clear();
    appendBubble("error",
        "\xe2\x9d\x8c  Errore: " + msg +          // ❌
        "\n\xf0\x9f\x92\xa1  Verifica che Ollama sia attivo su " +  // 💡
        m_ai->host() + ":" + QString::number(m_ai->port()));
    m_streamBubble = nullptr;
}

/* ── onStop ─────────────────────────────────────────────────── */
void ChatPage::onStop()
{
    m_ai->abort();
    m_streaming = false;
    m_stopBtn->setEnabled(false);
    m_sendBtn->setEnabled(true);
}

/* ── onClear ────────────────────────────────────────────────── */
void ChatPage::onClear()
{
    while (QLayoutItem* item = m_chatLayout->takeAt(0)) {
        if (QWidget* w = item->widget()) w->deleteLater();
        delete item;
    }
    m_streamBubble = nullptr;
    m_history = QJsonArray();
    m_pendingContext.clear();
    m_attachedContents.clear();
    m_attachedNames.clear();
    m_ragBtn->setText(QString::fromUtf8("\xf0\x9f\x93\x8e"));  /* 📎 reset */
}

/* ── onBubbleCopyClicked — copia testo dalla bolla ────────── */
void ChatPage::onBubbleCopyClicked(const QString& text)
{
    QApplication::clipboard()->setText(text);
}

/* ── onBubbleSpeakClicked — legge testo dalla bolla ─────── */
void ChatPage::onBubbleSpeakClicked(const QString& text)
{
#ifdef HAVE_TTS
    if (m_tts) {
        if (m_tts->state() == QTextToSpeech::Speaking)
            m_tts->stop();
        else
            m_tts->say(text);
        return;
    }
#endif
    QApplication::clipboard()->setText(text);
}

/* ── prependContext (da CameraPage) ─────────────────────────── */
void ChatPage::prependContext(const QString& text)
{
    m_pendingContext = text;
    m_input->setPlaceholderText("Fai una domanda sul documento…");
    appendBubble("system",
        "\xf0\x9f\x93\xb7  Documento acquisito. Scrivi una domanda.");  // 📷
}

/* ── onRagReloaded ──────────────────────────────────────────── */
void ChatPage::onRagReloaded()
{
    appendBubble("system",
        "\xf0\x9f\x93\x9a  Indice RAG aggiornato: " +           // 📚
        QString::number(m_rag->chunkCount()) + " chunk.");
}

/* ══════════════════════════════════════════════════════════════
   appendBubble — aggiunge una bolla al layout chat
   ══════════════════════════════════════════════════════════════ */
void ChatPage::appendBubble(const QString& role, const QString& text)
{
    const QString bRole = (role == "user") ? "user"
                        : (role == "ai")   ? "ai"
                        :                    "system";

    const QString display = (role == "error")
        ? QString::fromUtf8("\xe2\x9d\x8c  ") + text   /* ❌ */
        : text;

    auto* bubble = new ChatBubbleWidget(bRole, display, m_chatContainer);
    connect(bubble, &ChatBubbleWidget::copyClicked,
            this,   &ChatPage::onBubbleCopyClicked);
    connect(bubble, &ChatBubbleWidget::speakClicked,
            this,   &ChatPage::onBubbleSpeakClicked);

    if (role == "user")
        m_chatLayout->addWidget(bubble, 0, Qt::AlignRight);
    else
        m_chatLayout->addWidget(bubble);

    QTimer::singleShot(0, this, &ChatPage::scrollToBottom);
}

/* ── appendStreamBlock — bolla vuota per lo streaming ────────── */
void ChatPage::appendStreamBlock()
{
    auto* bubble = new ChatBubbleWidget("ai", "", m_chatContainer);
    connect(bubble, &ChatBubbleWidget::copyClicked,
            this,   &ChatPage::onBubbleCopyClicked);
    connect(bubble, &ChatBubbleWidget::speakClicked,
            this,   &ChatPage::onBubbleSpeakClicked);
    m_chatLayout->addWidget(bubble);
    m_streamBubble = bubble;
    QTimer::singleShot(0, this, &ChatPage::scrollToBottom);
}

/* ── onAttachClicked — apre file picker Android (max 5 file) ── */
void ChatPage::onAttachClicked()
{
    constexpr int kMaxFiles = 5;
    if (m_attachedContents.size() >= kMaxFiles) {
        appendBubble("system",
            QString::fromUtf8("\xe2\x9a\xa0")  /* ⚠ */
            + "  Massimo 5 file allegabili. Premi \xf0\x9f\x97\x91 per resettare la chat.");
        return;
    }

    QFileDialog dlg(this, "Allega file");
    dlg.setFileMode(QFileDialog::ExistingFiles);
    dlg.setNameFilter(
        "Testo / Codice (*.txt *.md *.csv *.json *.xml "
        "*.py *.js *.ts *.cpp *.h *.java *.kt *.log *.sh *.yaml *.toml);;"
        "Tutti i file (*)");
    dlg.setDirectory(
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));

    if (dlg.exec() != QDialog::Accepted) return;

    const QStringList chosen = dlg.selectedFiles();
    int added = 0;
    QStringList tooLarge, unreadable;

    for (const QString& path : chosen) {
        if (m_attachedContents.size() >= kMaxFiles) break;

        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            unreadable << QFileInfo(path).fileName();
            continue;
        }
        const QByteArray raw = f.read(64 * 1024);   /* max 64 KB per file */
        f.close();

        const QString name = QFileInfo(path).fileName();
        if (raw.isEmpty()) {
            unreadable << name;
            continue;
        }

        bool wasTruncated = (f.size() > 64 * 1024);
        m_attachedContents << QString::fromUtf8(raw)
                              + (wasTruncated ? "\n[... troncato a 64 KB ...]" : "");
        m_attachedNames    << name;
        if (wasTruncated) tooLarge << name;
        ++added;
    }

    if (added > 0) {
        /* Aggiorna etichetta pulsante con contatore */
        m_ragBtn->setText(
            QString::fromUtf8("\xf0\x9f\x93\x8e")  /* 📎 */
            + " (" + QString::number(m_attachedContents.size()) + ")");

        QString msg = QString::fromUtf8("\xf0\x9f\x93\x8e ")  /* 📎 */
                    + QString::number(added) + " file allegati: "
                    + m_attachedNames.join(", ")
                    + "\nVerranno inclusi nel contesto del prossimo messaggio.";
        if (!tooLarge.isEmpty())
            msg += "\n" + QString::fromUtf8("\xe2\x9a\xa0")  /* ⚠ */
                 + " Troncati a 64 KB: " + tooLarge.join(", ");
        appendBubble("system", msg);
    }
    if (!unreadable.isEmpty())
        appendBubble("system",
            QString::fromUtf8("\xe2\x9d\x8c")  /* ❌ */
            + " File non leggibili: " + unreadable.join(", "));
}

/* ── fetchModels ─────────────────────────────────────────────── */
void ChatPage::fetchModels()
{
    m_ai->fetchModels();
}

/* ── onExternalModelChanged — modello cambiato da Impostazioni ─ */
void ChatPage::onExternalModelChanged(const QString& model)
{
    if (!m_modelList.contains(model))
        m_modelList.prepend(model);
    if (!m_ai->isLocalMode())
        m_modelBtn->setText(serverEmoji(m_ai->host(), m_ai->port()) + " " + model);
}

/* ── onModelsReady — lista modelli arrivata dal server ─────────── */
void ChatPage::onModelsReady(const QStringList& models)
{
    if (models.isEmpty()) return;
    m_modelList = models;
    if (!m_ai->isLocalMode()) {
        const QString cur = m_ai->model();
        m_modelBtn->setText(
            serverEmoji(m_ai->host(), m_ai->port()) + " "
            + (cur.isEmpty() ? models.first() : cur));
    }
}

/* ── isLanHost / serverEmoji ─────────────────────────────────── */
bool ChatPage::isLanHost(const QString& host, int port)
{
    if (port == 11500) return true;   /* Prismalux LAN server */
    if (host == "localhost" || host == "127.0.0.1" || host == "::1") return true;
    if (host.startsWith("192.168.")) return true;
    if (host.startsWith("10."))      return true;
    /* 172.16.0.0 – 172.31.255.255 */
    if (host.startsWith("172.")) {
        const QStringList p = host.split('.');
        if (p.size() >= 2) {
            bool ok; const int n = p[1].toInt(&ok);
            if (ok && n >= 16 && n <= 31) return true;
        }
    }
    return false;
}

QString ChatPage::serverEmoji(const QString& host, int port)
{
    return isLanHost(host, port)
        ? QString::fromUtf8("\xf0\x9f\x93\xb6")    /* 📶 LAN */
        : QString::fromUtf8("\xe2\x98\x81\xef\xb8\x8f");  /* ☁️ cloud */
}

/* ── onModelBtnClicked — apre il selettore a tutto schermo ───── */
void ChatPage::onModelBtnClicked()
{
    if (m_ai->busy() || m_ai->isLocalMode()) return;

    QStringList list = m_modelList;
    if (list.isEmpty()) {
        list << "llama3.2:1b" << "deepseek-r1:1.5b" << "llama3.2:3b"
             << "phi3:mini"   << "mistral:7b-instruct";
    }

    const QString emoji = serverEmoji(m_ai->host(), m_ai->port());
    auto* dlg = new ModelPickerDialog(list, m_ai->model(), emoji, m_ai->modelSizes(), this);
    connect(dlg, &ModelPickerDialog::modelPicked, this, &ChatPage::onModelPicked);
    dlg->exec();
    dlg->deleteLater();
}

/* ── onModelPicked — modello scelto dall'utente ─────────────── */
void ChatPage::onModelPicked(const QString& model)
{
    if (model.isEmpty()) return;
    /* Modello locale (📱): attiva local mode */
    if (LocalLlmClient::firstAvailableModelPath().contains(model)) {
        m_ai->setLocalMode(true);
        return;
    }
    if (model == m_ai->model()) return;
    m_modelBtn->setText(serverEmoji(m_ai->host(), m_ai->port()) + " " + model);
    m_ai->setServer(m_ai->host(), m_ai->port(), model);
}

/* ══════════════════════════════════════════════════════════════
   Selettore backend — Server / Locale
   ══════════════════════════════════════════════════════════════ */
void ChatPage::onBackendServer()
{
    /* Ripristina le impostazioni del server LAN salvate in Impostazioni */
    QSettings s("Prismalux", "Mobile");
    const QString host  = s.value("server/host",  "192.168.1.165").toString();
    const int     port  = s.value("server/port",  11434).toInt();
    const QString model = s.value("server/model", "llama3.2:1b").toString();
    const QString token = s.value("server/token", "").toString();

    m_ai->setLocalMode(false);
    m_ai->setServer(host, port, model);
    if (!token.isEmpty()) m_ai->setToken(token);

    m_serverBtn->setChecked(true);

    m_modelBtn->setText(serverEmoji(host, port) + "  " + model);
    m_modelBtn->setEnabled(true);

    m_backendStatusLbl->setText(
        QString::fromUtf8("\xf0\x9f\x8c\x90")   /* 🌐 */
        + "  OLLAMA SERVER ATTIVO");
    m_backendStatusLbl->setProperty("backend", "server");
    m_backendStatusLbl->style()->unpolish(m_backendStatusLbl);
    m_backendStatusLbl->style()->polish(m_backendStatusLbl);
}

void ChatPage::onBackendLocal()
{
    m_ai->setLocalMode(true);
}

void ChatPage::onAiLocalModeChanged(bool on)
{
    /* Il QButtonGroup gestisce l'esclusività: basta attivare il pulsante corretto */
    if (on) m_localBtn->setChecked(true);
    else    m_serverBtn->setChecked(true);

    if (on) {
        /* Locale attivo */
        const QString p    = LocalLlmClient::firstAvailableModelPath();
        const QString name = p.isEmpty()
            ? "locale"
            : LocalLlmClient::loadedModelName(p);
        m_modelBtn->setText(
            QString::fromUtf8("\xf0\x9f\x93\xb1 ") + name);
        m_modelBtn->setEnabled(false);
        m_modelBtn->setToolTip(
            QString::fromUtf8("\xf0\x9f\x93\xb1") + " Modello locale: " + name);

        m_backendStatusLbl->setText(
            QString::fromUtf8("\xf0\x9f\x93\xb1")   /* 📱 */
            + "  LLM LOCALE ATTIVO");
        m_backendStatusLbl->setProperty("backend", "local");
    } else {
        /* Server attivo */
        const QString cur = m_ai->model().isEmpty() ? "llama3.2:1b" : m_ai->model();
        m_modelBtn->setText(QString::fromUtf8("\xf0\x9f\xa4\x96 ") + cur);
        m_modelBtn->setEnabled(true);
        m_modelBtn->setToolTip("");

        m_backendStatusLbl->setText(
            QString::fromUtf8("\xf0\x9f\x8c\x90")   /* 🌐 */
            + "  OLLAMA SERVER ATTIVO");
        m_backendStatusLbl->setProperty("backend", "server");
    }

    /* Forza il ricalcolo dello stile Qt (property cambiata) */
    m_backendStatusLbl->style()->unpolish(m_backendStatusLbl);
    m_backendStatusLbl->style()->polish(m_backendStatusLbl);
}

void ChatPage::onLocalModelLoaded(const QString& path)
{
    m_localBtn->setEnabled(true);
    m_localBtn->setToolTip("");
    if (m_ai->isLocalMode()) {
        const QString name = LocalLlmClient::loadedModelName(path);
        m_modelBtn->setText(QString::fromUtf8("\xf0\x9f\x93\xb1 ") + name);
    }
}

/* ── scrollToBottom ─────────────────────────────────────────── */
void ChatPage::scrollToBottom()
{
    QScrollBar* sb = m_scrollArea->verticalScrollBar();
    sb->setValue(sb->maximum());
}

/* ══════════════════════════════════════════════════════════════
   SQLite — cronologia persistente
   ══════════════════════════════════════════════════════════════ */

/* initDb — crea/apre il database e la tabella messages */
void ChatPage::initDb()
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "chat_db");
    const QString dir = QStandardPaths::writableLocation(
                            QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);   // crea la cartella se non esiste
    db.setDatabaseName(dir + "/chat_history.db");
    if (!db.open()) return;
    QSqlQuery q(db);
    q.exec("CREATE TABLE IF NOT EXISTS messages ("
           "id        INTEGER PRIMARY KEY AUTOINCREMENT,"
           "role      TEXT    NOT NULL,"
           "content   TEXT    NOT NULL,"
           "timestamp INTEGER NOT NULL)");
}

/* saveMessageToDb — inserisce un singolo messaggio */
void ChatPage::saveMessageToDb(const QString& role, const QString& content)
{
    QSqlDatabase db = QSqlDatabase::database("chat_db");
    if (!db.isOpen()) return;
    QSqlQuery q(db);
    q.prepare("INSERT INTO messages (role, content, timestamp) VALUES (?, ?, ?)");
    q.addBindValue(role);
    q.addBindValue(content);
    q.addBindValue(QDateTime::currentSecsSinceEpoch());
    q.exec();
}

/* loadHistoryFromDb — carica gli ultimi 50 messaggi e li mostra come bolle */
void ChatPage::loadHistoryFromDb()
{
    QSqlDatabase db = QSqlDatabase::database("chat_db");
    if (!db.isOpen()) return;
    QSqlQuery q(db);
    /* Recupera gli ultimi 50 messaggi in ordine cronologico */
    q.exec("SELECT role, content FROM ("
           "  SELECT role, content, timestamp FROM messages"
           "  ORDER BY id DESC LIMIT 50"
           ") ORDER BY timestamp ASC");
    while (q.next()) {
        const QString role    = q.value(0).toString();
        const QString content = q.value(1).toString();
        /* Mostra come bolle nella UI */
        if (role == "user" || role == "assistant") {
            const QString uiRole = (role == "assistant") ? "ai" : "user";
            appendBubble(uiRole, content);
        }
        /* Ricostruisce anche m_history in-memory (finestra scorrevole) */
        if (role == "user" || role == "assistant") {
            QJsonObject obj;
            obj["role"]    = role;
            obj["content"] = content;
            m_history.append(obj);
        }
    }
    /* Tronca m_history alla finestra massima */
    while (m_history.size() > kMaxHistoryTurns * 2)
        m_history.removeAt(0);
    /* Nota informativa se c'era cronologia */
    if (m_history.size() > 0)
        appendBubble("system",
            QString::fromUtf8("\xf0\x9f\x97\x83")  /* 🗃 */
            + "  Cronologia ripristinata dal database.");
}

/* onClearHistory — cancella la cronologia dal DB e dalla UI */
void ChatPage::onClearHistory()
{
    /* Svuota la tabella SQLite */
    QSqlDatabase db = QSqlDatabase::database("chat_db");
    if (db.isOpen()) {
        QSqlQuery q(db);
        q.exec("DELETE FROM messages");
    }
    /* Svuota anche la UI e la history in-memory (riusa onClear) */
    onClear();
    appendBubble("system",
        QString::fromUtf8("\xf0\x9f\x97\x83")  /* 🗃 */
        + "  Cronologia cancellata dal database.");
}

/* ── onCloudModeClicked — attiva Cloud con le impostazioni salvate ── */
void ChatPage::onCloudModeClicked()
{
    /* Attiva direttamente senza dialog: le credenziali cloud si configurano
       nella pagina Impostazioni. Il bottone fa solo lo switch di backend. */
    QSettings s("Prismalux", "Mobile");
    const QString host  = s.value("cloud/host",  "api.openai.com").toString();
    const QString key   = s.value("cloud/key",   "").toString();
    const QString model = s.value("cloud/model", "gpt-3.5-turbo").toString();
    const int     port  = s.value("cloud/port",  443).toInt();

    /* Preserva le impostazioni LAN prima che setServer() le sovrascriva */
    const QString lanHost  = s.value("server/host",  "192.168.1.165").toString();
    const int     lanPort  = s.value("server/port",  11434).toInt();
    const QString lanModel = s.value("server/model", "llama3.2:1b").toString();
    const QString lanToken = s.value("server/token", "").toString();

    m_ai->setLocalMode(false);
    m_ai->setServer(host, port, model);
    if (!key.isEmpty())
        m_ai->setToken(key);

    /* Ripristina i valori LAN che setServer() ha sovrascritto */
    s.setValue("server/host",  lanHost);
    s.setValue("server/port",  lanPort);
    s.setValue("server/model", lanModel);
    s.setValue("server/token", lanToken);

    m_backendStatusLbl->setText(
        QString::fromUtf8("\xe2\x98\x81\xef\xb8\x8f")
        + "  CLOUD API ATTIVO: " + host);
    m_backendStatusLbl->setProperty("backend", "cloud");
    m_backendStatusLbl->style()->unpolish(m_backendStatusLbl);
    m_backendStatusLbl->style()->polish(m_backendStatusLbl);

    m_modelBtn->setText(
        QString::fromUtf8("\xe2\x98\x81\xef\xb8\x8f ") + model);
    m_modelBtn->setEnabled(true);
}
