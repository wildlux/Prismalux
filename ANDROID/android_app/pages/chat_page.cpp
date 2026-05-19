#include "chat_page.h"
#include "../ai_client.h"
#include "../rag_engine_simple.h"
#include "../local_llm_client.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollBar>
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
#include <QInputDialog>
#include <QSettings>

/* ══════════════════════════════════════════════════════════════
   ModelPickerDialog — implementazione
   ══════════════════════════════════════════════════════════════ */
ModelPickerDialog::ModelPickerDialog(const QStringList& models,
                                     const QString& current,
                                     const QString& srvEmoji,
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

    /* Modelli dal server (LAN o cloud) */
    for (const QString& m : models) {
        const QString display = srvEmoji + "  " + m;
        auto* item = new QListWidgetItem(display, m_list);
        item->setData(Qt::UserRole, m);   /* nome pulito per AiClient */
        item->setSizeHint(QSize(0, 56));
        if (m == current) {
            QFont bf = item->font(); bf.setBold(true); item->setFont(bf);
            item->setText(srvEmoji + "  " + QString::fromUtf8("\xe2\x9c\x94 ") + m);
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

    m_stopBtn  = new QPushButton("\xe2\x9c\x95  Stop", this);   // ✕
    m_clearBtn = new QPushButton("\xf0\x9f\x97\x91", this);     // 🗑
    m_stopBtn->setEnabled(false);
    m_stopBtn->setObjectName("StopBtn");
    m_clearBtn->setObjectName("IconBtn");
    header->addWidget(m_modelBtn, 1);
    header->addWidget(m_stopBtn);
    header->addWidget(m_clearBtn);

    /* ── Selettore backend: Cloud ☁️ | Server 🌐 | Locale 📱 ── */
    m_cloudBtn  = new QPushButton(
        QString::fromUtf8("\xe2\x98\x81\xef\xb8\x8f") + " Cloud", this);   /* ☁️ */
    m_serverBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x8c\x90") + " Server", this);   /* 🌐 */
    m_localBtn  = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x93\xb1") + " Locale", this);   /* 📱 */
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

    /* ── Log chat ── */
    m_log = new QTextBrowser(this);
    m_log->setObjectName("ChatLog");
    m_log->setOpenLinks(false);
    m_log->setReadOnly(true);
    /* Disabilita la selezione testo nativa: su Android il cursore di selezione
       intercetta i touch e rende impossibile scorrere la chat. */
    m_log->setTextInteractionFlags(Qt::NoTextInteraction);
    /* Scroll kinetic a un dito (Material-like) */
    QScroller::grabGesture(m_log->viewport(), QScroller::TouchGesture);
    {
        QScroller* qs = QScroller::scroller(m_log->viewport());
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
    m_log->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_log->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    /* ── Barra azioni chat (copia + leggi ad alta voce) ── */
    m_copyBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x93\x8b") + " Copia", this);  /* 📋 */
    m_copyBtn->setObjectName("SecondaryBtn");
    m_copyBtn->setMinimumHeight(36);
    m_copyBtn->setEnabled(false);

    m_speakBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x94\x8a") + " Leggi", this);  /* 🔊 */
    m_speakBtn->setObjectName("SecondaryBtn");
    m_speakBtn->setMinimumHeight(36);
    m_speakBtn->setEnabled(false);

#ifdef HAVE_TTS
    m_tts = new QTextToSpeech(this);
    m_tts->setLocale(QLocale(QLocale::Italian, QLocale::Italy));
#endif

    auto* actRow = new QHBoxLayout;
    actRow->setSpacing(6);
    actRow->addWidget(m_copyBtn,  1);
    actRow->addWidget(m_speakBtn, 1);

    /* ── RAG toggle ── */
    m_ragBtn = new QPushButton("\xf0\x9f\x93\x9a RAG", this);  // 📚
    m_ragBtn->setCheckable(true);
    m_ragBtn->setChecked(m_ragEnabled);
    m_ragBtn->setObjectName("RagToggle");
    m_ragBtn->setToolTip("Attiva/disattiva contesto RAG locale");

    /* ── Input ── */
    m_input   = new QLineEdit(this);
    m_input->setPlaceholderText("Scrivi un messaggio…");
    m_input->setObjectName("ChatInput");
    m_sendBtn = new QPushButton("\xe2\x9e\xa4", this);  // ➤
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
    vbox->addWidget(m_log, 1);
    vbox->addLayout(actRow);
    vbox->addLayout(inputRow);

    /* ── Connessioni ── */
    connect(m_sendBtn,  &QPushButton::clicked,  this, &ChatPage::onSend);
    connect(m_stopBtn,  &QPushButton::clicked,  this, &ChatPage::onStop);
    connect(m_clearBtn, &QPushButton::clicked,  this, &ChatPage::onClear);
    connect(m_copyBtn,  &QPushButton::clicked,  this, &ChatPage::onCopyLast);
    connect(m_modelBtn, &QPushButton::clicked,  this, &ChatPage::onModelBtnClicked);
    connect(m_ragBtn,   &QPushButton::toggled,  this, &ChatPage::onRagToggled);
    connect(m_input,    &QLineEdit::returnPressed, this, &ChatPage::onSend);

    connect(m_cloudBtn,  &QPushButton::clicked, this, &ChatPage::onCloudModeClicked);
    connect(m_serverBtn, &QPushButton::clicked, this, &ChatPage::onBackendServer);
    connect(m_localBtn,  &QPushButton::clicked, this, &ChatPage::onBackendLocal);
    connect(m_speakBtn,  &QPushButton::clicked, this, &ChatPage::onSpeakLast);

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
        return sys;  // la camera fornisce già il contesto, non serve RAG
    }

    /* RAG locale: inietta i chunk rilevanti */
    if (m_ragEnabled && m_rag->chunkCount() > 0) {
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
   onToken — aggiorna il blocco streaming in tempo reale
   ══════════════════════════════════════════════════════════════ */
void ChatPage::onToken(const QString& chunk)
{
    m_streamAccum += chunk;
    /* Aggiorna l'ultimo paragrafo del log (l'AI response block) */
    QTextCursor cur = m_log->textCursor();
    cur.movePosition(QTextCursor::End);
    cur.select(QTextCursor::BlockUnderCursor);
    cur.removeSelectedText();
    cur.insertText(m_streamAccum);
    m_log->verticalScrollBar()->setValue(m_log->verticalScrollBar()->maximum());
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
        /* Abilita copia e lettura ad alta voce */
        m_lastAssistantMsg = full;
        m_copyBtn->setEnabled(true);
        m_copyBtn->setText(
            QString::fromUtf8("\xf0\x9f\x93\x8b")  /* 📋 */
            + " Copia");
        m_speakBtn->setEnabled(true);
    }
    m_lastUserMsg.clear();
    m_streamAccum.clear();
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
    m_streamAccum.clear();
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
    m_log->clear();
    m_history = QJsonArray();
    m_lastAssistantMsg.clear();
    m_copyBtn->setEnabled(false);
    m_copyBtn->setText(QString::fromUtf8("\xf0\x9f\x93\x8b") + " Copia");  /* 📋 */
    m_speakBtn->setEnabled(false);
}

/* ── onCopyLast ─────────────────────────────────────────────── */
void ChatPage::onCopyLast()
{
    if (m_lastAssistantMsg.isEmpty()) return;
    QApplication::clipboard()->setText(m_lastAssistantMsg);
    /* Feedback visivo temporaneo: ✅ Copiato! per 2 s poi ripristino */
    m_copyBtn->setText(
        QString::fromUtf8("\xe2\x9c\x85")  /* ✅ */
        + "  Copiato!");
    m_copyBtn->setEnabled(false);
    QTimer::singleShot(2000, this, &ChatPage::onCopyBtnRestore);
}

void ChatPage::onCopyBtnRestore()
{
    if (!m_copyBtn) return;
    m_copyBtn->setText(
        QString::fromUtf8("\xf0\x9f\x93\x8b")  /* 📋 */
        + "  Copia risposta");
    m_copyBtn->setEnabled(!m_lastAssistantMsg.isEmpty());
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
   appendBubble — aggiunge una bolla al log
   ══════════════════════════════════════════════════════════════ */
void ChatPage::appendBubble(const QString& role, const QString& text)
{
    QString prefix;
    if      (role == "user")   prefix = "Tu: ";
    else if (role == "error")  prefix = "";
    else if (role == "system") prefix = "";
    else                       prefix = "\xf0\x9f\xa4\x96  ";  // 🤖

    m_log->moveCursor(QTextCursor::End);
    if (!m_log->toPlainText().isEmpty())
        m_log->insertPlainText("\n");
    m_log->insertPlainText(prefix + text);
    m_log->verticalScrollBar()->setValue(m_log->verticalScrollBar()->maximum());
}

/* ── appendStreamBlock — riga vuota per lo streaming ────────── */
void ChatPage::appendStreamBlock()
{
    m_streamAccum.clear();
    m_log->moveCursor(QTextCursor::End);
    m_log->insertPlainText("\n\xf0\x9f\xa4\x96  ");  // 🤖 + spazio iniziale
}

/* ── onRagToggled ───────────────────────────────────────────── */
void ChatPage::onRagToggled(bool on)
{
    m_ragEnabled = on;
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
    auto* dlg = new ModelPickerDialog(list, m_ai->model(), emoji, this);
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
    m_ai->setLocalMode(false);  /* emette localModeChanged → onAiLocalModeChanged */
}

void ChatPage::onBackendLocal()
{
    m_ai->setLocalMode(true);
}

void ChatPage::onAiLocalModeChanged(bool on)
{
    m_serverBtn->setChecked(!on);
    m_localBtn->setChecked(on);

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

/* ── onSpeakLast — legge l'ultima risposta AI con TTS ────────── */
void ChatPage::onSpeakLast()
{
    if (m_lastAssistantMsg.isEmpty()) return;
#ifdef HAVE_TTS
    if (m_tts) {
        if (m_tts->state() == QTextToSpeech::Speaking)
            m_tts->stop();
        else
            m_tts->say(m_lastAssistantMsg);
        return;
    }
#endif
    /* Fallback senza TTS: copia negli appunti e avvisa */
    QApplication::clipboard()->setText(m_lastAssistantMsg);
    m_speakBtn->setText(
        QString::fromUtf8("\xf0\x9f\x93\x8b") + " (Copiato)");
    QTimer::singleShot(2000, this, &ChatPage::onCopyBtnRestore);
}

/* ── onCloudModeClicked — configura backend cloud API ─────────── */
void ChatPage::onCloudModeClicked()
{
    QSettings s("Prismalux", "Mobile");
    const QString savedHost  = s.value("cloud/host",  "api.openai.com").toString();
    const QString savedKey   = s.value("cloud/key",   "").toString();
    const QString savedModel = s.value("cloud/model", "gpt-3.5-turbo").toString();
    const int     savedPort  = s.value("cloud/port",  443).toInt();

    /* Dialog semplice per configurare l'endpoint cloud */
    bool ok = false;
    const QString endpoint = QInputDialog::getText(
        this,
        QString::fromUtf8("\xe2\x98\x81\xef\xb8\x8f Cloud API — Host"),
        "Host API (es. api.openai.com, ollama.mioserver.com):",
        QLineEdit::Normal, savedHost, &ok);
    if (!ok || endpoint.trimmed().isEmpty()) return;

    const QString apiKey = QInputDialog::getText(
        this,
        QString::fromUtf8("\xf0\x9f\x94\x91 Cloud API — Chiave"),
        "API Key (lascia vuoto se non richiesta):",
        QLineEdit::Password, savedKey, &ok);
    if (!ok) return;

    const QString model = QInputDialog::getText(
        this,
        QString::fromUtf8("\xf0\x9f\xa4\x96 Cloud API — Modello"),
        "Nome modello (es. gpt-3.5-turbo, mistral-large):",
        QLineEdit::Normal, savedModel, &ok);
    if (!ok || model.trimmed().isEmpty()) return;

    s.setValue("cloud/host",  endpoint.trimmed());
    s.setValue("cloud/key",   apiKey.trimmed());
    s.setValue("cloud/model", model.trimmed());
    s.setValue("cloud/port",  savedPort);

    /* Aggiorna AiClient per usare il server cloud */
    m_ai->setLocalMode(false);
    m_ai->setServer(endpoint.trimmed(), savedPort, model.trimmed());
    if (!apiKey.trimmed().isEmpty())
        m_ai->setToken(apiKey.trimmed());

    m_cloudBtn->setChecked(true);
    m_serverBtn->setChecked(false);
    m_localBtn->setChecked(false);

    m_backendStatusLbl->setText(
        QString::fromUtf8("\xe2\x98\x81\xef\xb8\x8f")   /* ☁️ */
        + "  CLOUD API ATTIVO: " + endpoint.trimmed());
    m_backendStatusLbl->setProperty("backend", "cloud");
    m_backendStatusLbl->style()->unpolish(m_backendStatusLbl);
    m_backendStatusLbl->style()->polish(m_backendStatusLbl);

    m_modelBtn->setText(
        QString::fromUtf8("\xe2\x98\x81\xef\xb8\x8f ") + model.trimmed());
    m_modelBtn->setEnabled(true);

    appendBubble("system",
        QString::fromUtf8("\xe2\x98\x81\xef\xb8\x8f")  /* ☁️ */
        + " Cloud API configurato: " + endpoint.trimmed()
        + " | Modello: " + model.trimmed());
}
