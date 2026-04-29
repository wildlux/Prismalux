#include "chat_page.h"
#include "../ai_client.h"
#include "../rag_engine_simple.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollBar>
#include <QKeyEvent>

/* ══════════════════════════════════════════════════════════════
   Costruttore
   ══════════════════════════════════════════════════════════════ */
ChatPage::ChatPage(AiClient* ai, RagEngineSimple* rag, QWidget* parent)
    : QWidget(parent), m_ai(ai), m_rag(rag)
{
    setObjectName("ChatPage");

    /* ── Header ── */
    auto* header = new QHBoxLayout;
    m_modelLbl = new QLabel(m_ai->model(), this);
    m_modelLbl->setObjectName("ModelLabel");
    m_stopBtn  = new QPushButton("\xe2\x9c\x95  Stop", this);   // ✕
    m_clearBtn = new QPushButton("\xf0\x9f\x97\x91", this);     // 🗑
    m_stopBtn->setEnabled(false);
    m_stopBtn->setObjectName("StopBtn");
    m_clearBtn->setObjectName("IconBtn");
    header->addWidget(m_modelLbl, 1);
    header->addWidget(m_stopBtn);
    header->addWidget(m_clearBtn);

    /* ── Log chat ── */
    m_log = new QTextBrowser(this);
    m_log->setObjectName("ChatLog");
    m_log->setOpenLinks(false);
    m_log->setReadOnly(true);

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
    vbox->addWidget(m_log, 1);
    vbox->addLayout(inputRow);

    /* ── Connessioni ── */
    connect(m_sendBtn,  &QPushButton::clicked, this, &ChatPage::onSend);
    connect(m_stopBtn,  &QPushButton::clicked, this, &ChatPage::onStop);
    connect(m_clearBtn, &QPushButton::clicked, this, &ChatPage::onClear);
    connect(m_ragBtn,   &QPushButton::toggled, this, [this](bool on){
        m_ragEnabled = on;
    });
    connect(m_input, &QLineEdit::returnPressed, this, &ChatPage::onSend);

    connect(m_ai, &AiClient::token,    this, &ChatPage::onToken);
    connect(m_ai, &AiClient::finished, this, &ChatPage::onFinished);
    connect(m_ai, &AiClient::error,    this, &ChatPage::onError);
    connect(m_ai, &AiClient::modelChanged, m_modelLbl, &QLabel::setText);
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

    /* Salva nella history (sliding window) */
    if (!full.isEmpty()) {
        QJsonObject u; u["role"] = "user";
        u["content"] = m_log->toPlainText().split('\n').filter("Tu:").last()
                       .mid(3).trimmed();
        QJsonObject a; a["role"] = "assistant"; a["content"] = full;
        m_history.append(u);
        m_history.append(a);
        /* Mantieni solo gli ultimi kMaxHistoryTurns*2 messaggi */
        while (m_history.size() > kMaxHistoryTurns * 2)
            m_history.removeAt(0);
    }
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
