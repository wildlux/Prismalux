#include "oracolo_page.h"
#include "../ai_client.h"
#include "../prismalux_paths.h"
namespace P = PrismaluxPaths;
#include "../widgets/chat_bubble.h"
#include "../widgets/chart_widget.h"
#include "../widgets/formula_parser.h"
#include "../widgets/tts_speak.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollBar>
#include <QTimer>
#include <QFileDialog>
#include <QKeyEvent>
#include <QFile>
#include <QImageReader>
#include <QBuffer>
#include <QPixmap>
#include <QRegularExpression>
#include <QProcess>
#include <QGuiApplication>
#include <QClipboard>
#include <QScrollArea>
#include <QDir>
#include <QSettings>
#include <QMessageBox>
#include <QJsonObject>
#include <QJsonArray>
#include <algorithm>
#include <cmath>

static QString ragIndexPath() {
    return QDir::homePath() + "/.prismalux_rag.json";
}

/* ══════════════════════════════════════════════════════════════
   Event filter: Enter=invia, Shift+Enter=nuova riga
   ══════════════════════════════════════════════════════════════ */
class InputFilter : public QObject {
public:
    explicit InputFilter(OracoloPage* page) : QObject(page), m_page(page) {}
protected:
    bool eventFilter(QObject*, QEvent* ev) override {
        if (ev->type() == QEvent::KeyPress) {
            auto* ke = static_cast<QKeyEvent*>(ev);
            if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
                if (!(ke->modifiers() & Qt::ShiftModifier)) {
                    QMetaObject::invokeMethod(m_page, "sendMessage", Qt::QueuedConnection);
                    return true;
                }
            }
        }
        return false;
    }
    OracoloPage* m_page;
};

/* ══════════════════════════════════════════════════════════════
   OracoloPage — costruttore
   Layout (alto→basso):
     1. [system prompt, hidden]
     2. [scroll chat area]       ← flex
     3. [wait label]
     4. [img preview]
     5. [textarea]  [⚙ Sys] [📷 Img]  [▶ Invia]  [👁 Nascondi]
     6. [quick bar pills]        ← toggle con Nascondi
   ══════════════════════════════════════════════════════════════ */
OracoloPage::OracoloPage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    /* ── 1. System prompt (collassabile) ── */
    m_sysEdit = new QTextEdit(this);
    m_sysEdit->setObjectName("chatInput");
    m_sysEdit->setPlaceholderText(
        "System prompt — definisce il comportamento dell'AI...");
    m_sysEdit->setFixedHeight(64);
    m_sysEdit->setVisible(false);
    lay->addWidget(m_sysEdit);

    /* ── 2. ScrollArea chat ── */
    m_scroll = new QScrollArea(this);
    m_scroll->setObjectName("chatScroll");
    m_scroll->setWidgetResizable(true);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_chatContainer = new QWidget(m_scroll);
    m_chatContainer->setObjectName("chatContainer");
    m_chatLay = new QVBoxLayout(m_chatContainer);
    m_chatLay->setContentsMargins(0, 8, 0, 8);
    m_chatLay->setSpacing(2);
    m_chatLay->addStretch();

    m_scroll->setWidget(m_chatContainer);
    lay->addWidget(m_scroll, 1);

    /* ── 3. Label attesa ── */
    m_waitLbl = new QLabel(
        "\xe2\x8f\xb3  L'AI sta elaborando...", this);
    m_waitLbl->setObjectName("cardDesc");
    m_waitLbl->setAlignment(Qt::AlignCenter);
    m_waitLbl->setVisible(false);
    lay->addWidget(m_waitLbl);

    /* ── 4. Anteprima immagine allegata ── */
    m_imgPreview = new QLabel(this);
    m_imgPreview->setObjectName("imgPreview");
    m_imgPreview->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_imgPreview->setVisible(false);
    connect(m_imgPreview, &QLabel::linkActivated, this, [this] {
        m_pendingImg.clear();
        m_pendingImgMime.clear();
        m_imgPreview->setVisible(false);
    });
    lay->addWidget(m_imgPreview);

    /* ── 5. Input row: [Sys][Img] [textarea] [Invia] [Nascondi] ── */
    auto* inputRow = new QWidget(this);
    inputRow->setObjectName("inputRow");
    auto* inputLay = new QHBoxLayout(inputRow);
    inputLay->setContentsMargins(10, 8, 10, 8);
    inputLay->setSpacing(6);

    /* Pulsanti controllo a sinistra */
    m_btnSys = new QPushButton("\xe2\x9a\x99", inputRow);  /* ⚙ */
    m_btnSys->setObjectName("actionBtn");
    m_btnSys->setToolTip("Mostra/nascondi system prompt");
    m_btnSys->setFixedSize(32, 32);

    m_btnImg = new QPushButton("\xf0\x9f\x93\xb7", inputRow);  /* 📷 */
    m_btnImg->setObjectName("actionBtn");
    m_btnImg->setToolTip("Allega immagine");
    m_btnImg->setFixedSize(32, 32);

    /* Textarea principale */
    m_input = new QPlainTextEdit(inputRow);
    m_input->setObjectName("chatInput");
    m_input->setPlaceholderText(
        "Scrivi il tuo messaggio qui... (Enter per inviare, Shift+Enter per andare a capo, \xf0\x9f\x8e\xa4 per parlare)");
    m_input->setFixedHeight(70);
    m_input->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    m_input->installEventFilter(new InputFilter(this));

    /* Invia */
    m_btnSend = new QPushButton(
        "\xe2\x9c\x88  Invia", inputRow);   /* ✈ Invia */
    m_btnSend->setObjectName("actionBtn");
    m_btnSend->setProperty("primary", true);
    m_btnSend->setMinimumWidth(90);

    /* Nascondi (toggle quick bar) */
    m_btnNascondi = new QPushButton(
        "\xf0\x9f\x91\x81  Nascondi", inputRow);   /* 👁 Nascondi */
    m_btnNascondi->setObjectName("actionBtn");
    m_btnNascondi->setToolTip("Mostra/nascondi le azioni rapide");
    m_btnNascondi->setCheckable(true);
    m_btnNascondi->setChecked(true);   /* quick bar visibile di default */

    /* Stop */
    m_btnStop = new QPushButton(
        "\xe2\x96\xa0  Stop", inputRow);
    m_btnStop->setObjectName("actionBtn");
    m_btnStop->setProperty("danger", true);
    m_btnStop->setEnabled(false);

    /* RAG toggle */
    m_btnRag = new QPushButton("\xf0\x9f\x93\x9a", inputRow);  /* 📚 */
    m_btnRag->setObjectName("actionBtn");
    m_btnRag->setCheckable(true);
    m_btnRag->setFixedSize(32, 32);
    m_btnRag->setToolTip("Attiva/disattiva RAG (Retrieval-Augmented Generation)\n"
                          "Inietta automaticamente i chunk pi\xc3\xb9 rilevanti dalla base di conoscenza.");

    /* Label conteggio chunk RAG */
    m_ragLbl = new QLabel("", inputRow);
    m_ragLbl->setObjectName("hintLabel");

    inputLay->addWidget(m_btnSys);
    inputLay->addWidget(m_btnImg);
    inputLay->addWidget(m_btnRag);
    inputLay->addWidget(m_ragLbl);
    inputLay->addWidget(m_input, 1);
    inputLay->addWidget(m_btnSend);
    inputLay->addWidget(m_btnNascondi);
    inputLay->addWidget(m_btnStop);
    lay->addWidget(inputRow);

    /* Carica indice RAG esistente */
    if (m_rag.load(ragIndexPath()))
        updateRagBtn();

    /* ── 6. Quick bar — pillole azione rapida ── */
    m_quickBar = new QWidget(this);
    m_quickBar->setObjectName("quickBar");
    {
        /* Scroll orizzontale se lo spazio è stretto */
        auto* scroll = new QScrollArea(m_quickBar);
        scroll->setObjectName("quickBarScroll");
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidgetResizable(true);

        auto* inner = new QWidget(scroll);
        auto* pillLay = new QHBoxLayout(inner);
        pillLay->setContentsMargins(10, 6, 10, 8);
        pillLay->setSpacing(8);

        /* ─── Definizione pillole ─────────────────────────── */
        struct Pill {
            const char* label;
            const char* prompt;    /* "" = azione speciale */
        };
        static const Pill kPills[] = {
            { "\xf0\x9f\x93\x90 Matematica",
              "Risolvi questo problema matematico passo passo: " },
            { "\xe2\x82\xac Finanza",
              "Analisi finanziaria: " },
            { "</>  Sviluppo",
              "Scrivi il codice (con commenti in italiano) per: " },
            { "\xf0\x9f\x93\x8a Grafico",
              "Crea un grafico di y = " },
            { "\xf0\x9f\x8c\x8a Sinusoide 3D",
              "Crea un grafico 3D parametrico di x=cos(t), y=sin(t), z=t" },
            { "\xf0\x9f\x97\x91 Pulisci",   "" },       /* azione: clear */
            { "\xf0\x9f\x94\x87 Muto",      "" },       /* azione: toggle mute */
            { "\xf0\x9f\x8e\xa4 Test Voce", "" },       /* azione: test TTS */
            { "\xf0\x9f\x93\x8a Grafici",
              "Mostrami un esempio di grafico: plotta y = sin(x) * cos(x/2) per x in [-4\xcf\x80, 4\xcf\x80]" },
            { "\xe2\x9d\x93 Aiuto",          "" },       /* azione: help */
        };
        /* Salva riferimento al pulsante Muto per aggiornarne il testo */
        m_btnMute = nullptr;

        for (const auto& pill : kPills) {
            auto* btn = new QPushButton(QLatin1String(pill.label), inner);
            btn->setObjectName("quickPill");
            btn->setCheckable(false);

            const QString prompt = QString::fromLatin1(pill.prompt);
            const QString label  = QString::fromLatin1(pill.label);

            if (label.contains("Muto")) {
                m_btnMute = btn;
                connect(btn, &QPushButton::clicked, this, [this] {
                    m_muted = !m_muted;
                    if (m_btnMute)
                        m_btnMute->setText(m_muted
                            ? "\xf0\x9f\x94\x87 Attivo"     /* 🔇 Attivo = muto ON */
                            : "\xf0\x9f\x94\x87 Muto");
                });
            } else if (label.contains("Pulisci")) {
                connect(btn, &QPushButton::clicked, this, &OracoloPage::clearChat);
            } else if (label.contains("Test Voce")) {
                connect(btn, &QPushButton::clicked, this, [] {
                    TtsSpeak::speak(
                        "Ciao! Il sistema di sintesi vocale funziona correttamente.");
                });
            } else if (label.contains("Aiuto")) {
                connect(btn, &QPushButton::clicked, this, [this] {
                    auto* b = addAIBubble("\xf0\x9f\x93\x96  Aiuto");
                    b->appendToken(
                        "Ciao! Sono l'Oracolo di Prismalux.\n\n"
                        "• Scrivi una domanda e premi Invia (o Enter)\n"
                        "• Shift+Enter = nuova riga\n"
                        "• Le pillole in basso inseriscono prompt predefiniti\n"
                        "• 📊 Grafico appare se la risposta contiene una formula\n"
                        "• 📷 Allega un'immagine per i modelli vision\n"
                        "• ⚙ Imposta un system prompt personalizzato\n"
                        "• 🔇 Muto disattiva la lettura vocale");
                    b->finalizeStream();
                    scrollToBottom();
                });
            } else {
                /* Pillola normale: inserisce il prompt nell'input */
                connect(btn, &QPushButton::clicked, this, [this, prompt, label] {
                    /* "Sinusoide 3D" → invia direttamente */
                    if (label.contains("Sinusoide 3D")) {
                        m_input->setPlainText(prompt);
                        sendMessage();
                    } else if (label.contains("Grafici")) {
                        m_input->setPlainText(prompt);
                        sendMessage();
                    } else {
                        m_input->setPlainText(prompt);
                        m_input->setFocus();
                        /* Posiziona cursore alla fine */
                        QTextCursor cur = m_input->textCursor();
                        cur.movePosition(QTextCursor::End);
                        m_input->setTextCursor(cur);
                    }
                });
            }

            pillLay->addWidget(btn);
        }
        pillLay->addStretch();

        scroll->setWidget(inner);
        auto* qbLay = new QVBoxLayout(m_quickBar);
        qbLay->setContentsMargins(0, 0, 0, 0);
        qbLay->addWidget(scroll);
    }
    lay->addWidget(m_quickBar);

    /* ══════════════════════════════════════════════════════════
       Connessioni UI
       ══════════════════════════════════════════════════════════ */
    connect(m_btnSend, &QPushButton::clicked,
            this, &OracoloPage::sendMessage);

    connect(m_btnStop, &QPushButton::clicked,
            this, [this] { m_ai->abort(); });

    connect(m_btnNascondi, &QPushButton::toggled, this, [this](bool checked) {
        m_quickBar->setVisible(checked);
        m_btnNascondi->setText(checked
            ? "\xf0\x9f\x91\x81  Nascondi"
            : "\xf0\x9f\x91\x81  Mostra");
    });

    connect(m_btnSys, &QPushButton::clicked, this, [this] {
        m_sysEdit->setVisible(!m_sysEdit->isVisible());
    });

    connect(m_btnImg, &QPushButton::clicked,
            this, &OracoloPage::attachImage);

    connect(m_btnRag, &QPushButton::toggled, this, [this](bool on) {
        m_ragEnabled = on;
        updateRagBtn();
    });

    /* RAG: embedding pronto → cerca context → avvia chat/generate */
    connect(m_ai, &AiClient::embeddingReady, this, [this](const QVector<float>& vec) {
        if (m_pendingMsg.isEmpty()) return;
        QString msg = m_pendingMsg;
        m_pendingMsg.clear();
        m_lastUserMsg = msg;    /* per addToHistory() nel finished handler */

        /* Ricerca top-k chunk rilevanti */
        QSettings ss("Prismalux", "GUI");
        int k = ss.value(P::SK::kRagMaxResults, 5).toInt();
        const QVector<RagChunk> hits = m_rag.search(vec, k);

        /* Costruisce il contesto da iniettare nel system prompt */
        QString ctx;
        if (!hits.isEmpty()) {
            ctx = "\n\n--- CONTESTO DALLA BASE DI CONOSCENZA ---\n";
            for (int i = 0; i < hits.size(); ++i)
                ctx += QString("[%1] %2\n").arg(i + 1).arg(hits[i].text.left(400));
            ctx += "--- FINE CONTESTO ---\n"
                   "Usa SOLO le informazioni nel contesto quando rispondi alla domanda.\n";
        }

        QString sys = m_sysEdit->toPlainText().trimmed();
        if (sys.isEmpty())
            sys = "Sei un assistente AI utile e preciso. "
                  "Rispondi SEMPRE e SOLO in italiano. "
                  "Quando fornisci formule matematiche usa la notazione y = f(x).";
        sys += ctx;

        /* Endpoint: /api/generate se non c'è storia, /api/chat se c'è storia */
        const AiClient::QueryType qt = AiClient::classifyQuery(msg);
        const bool hasHistory = !m_history.isEmpty() || !m_historySummary.isEmpty();

        if (hasHistory) {
            /* Storia attiva: usa /api/chat con storia + contesto RAG */
            m_ai->chat(sys, msg, buildHistoryArray(), qt);
        } else {
            /* Query RAG singola: usa /api/generate (più leggero) */
            m_ai->generate(sys, msg, qt);
        }
    });

    connect(m_ai, &AiClient::embeddingError, this, [this](const QString& err) {
        if (m_pendingMsg.isEmpty()) return;
        QString msg = m_pendingMsg;
        m_pendingMsg.clear();
        /* Fallback: chat senza contesto RAG */
        startChatWithContext(msg);
        Q_UNUSED(err);
    });

    /* ══════════════════════════════════════════════════════════
       AiClient — segnali streaming
       ══════════════════════════════════════════════════════════ */
    connect(m_ai, &AiClient::token, this, [this](const QString& tok) {
        if (!m_streaming || !m_activeBubble) return;
        m_activeBubble->appendToken(tok);
        scrollToBottom();
    });

    connect(m_ai, &AiClient::finished, this, [this](const QString& full) {
        if (!m_streaming) return;
        m_streaming = false;
        if (m_activeBubble) m_activeBubble->finalizeStream();
        m_activeBubble = nullptr;
        m_btnSend->setEnabled(true);
        m_btnStop->setEnabled(false);
        m_waitLbl->setVisible(false);

        /* Registra il turno nella storia conversazionale */
        if (!m_lastUserMsg.isEmpty() && !full.isEmpty())
            addToHistory(m_lastUserMsg, full);
        m_lastUserMsg.clear();

        scrollToBottom();
    });

    connect(m_ai, &AiClient::aborted, this, [this] {
        if (!m_streaming) return;
        m_streaming = false;
        if (m_activeBubble) {
            m_activeBubble->appendToken("\n\n\xe2\x9b\x94  Interrotto.");
            m_activeBubble->finalizeStream();
        }
        m_activeBubble = nullptr;
        m_btnSend->setEnabled(true);
        m_btnStop->setEnabled(false);
        m_waitLbl->setVisible(false);
    });

    connect(m_ai, &AiClient::error, this, [this](const QString& msg) {
        if (!m_streaming) return;
        m_streaming = false;
        if (m_activeBubble)
            m_activeBubble->appendToken("\n\n\xe2\x9d\x8c  Errore: " + msg);
        m_activeBubble = nullptr;
        m_btnSend->setEnabled(true);
        m_btnStop->setEnabled(false);
        m_waitLbl->setVisible(false);
    });
}

/* ══════════════════════════════════════════════════════════════
   sendMessage
   ══════════════════════════════════════════════════════════════ */
void OracoloPage::sendMessage() {
    const QString text = m_input->toPlainText().trimmed();
    if (text.isEmpty() || m_ai->busy()) return;

    m_input->clear();

    /* ── Rilevamento grafico cartesiano: "x=N, y=M" ── */
    const QVector<QPointF> coordPts = FormulaParser::tryExtractPoints(text);
    if (!coordPts.isEmpty()) {
        addUserBubble(text);
        showCartesianChart(coordPts);
        return;   /* non serve mandare all'AI — il grafico è già mostrato */
    }

    m_streaming = true;

    addUserBubble(text + (!m_pendingImg.isEmpty()
                         ? "\n\xf0\x9f\x93\xb7 [immagine allegata]"
                         : ""));

    const QString model = m_ai->model().isEmpty() ? "AI" : m_ai->model();
    m_activeBubble = addAIBubble("\xf0\x9f\xa4\x96  " + model);
    m_btnSend->setEnabled(false);
    m_btnStop->setEnabled(true);
    m_waitLbl->setVisible(true);

    /* ── Se RAG attivo e indice non vuoto: prima ottieni embedding query ── */
    if (m_ragEnabled && m_rag.chunkCount() > 0 && m_pendingImg.isEmpty()) {
        m_pendingMsg = text;
        m_ai->fetchEmbedding(text);   /* → embeddingReady/embeddingError */
        return;
    }

    startChatWithContext(text);
}

/* ══════════════════════════════════════════════════════════════
   startChatWithContext — avvia la chat (senza RAG o come fallback)

   Logica endpoint:
   • Immagine allegata       → chatWithImage() (sempre /api/chat)
   • Storia attiva           → chat() con history + QueryType
   • Nessuna storia          → generate() via /api/generate (più leggero)
   ══════════════════════════════════════════════════════════════ */
void OracoloPage::startChatWithContext(const QString& userMsg) {
    m_lastUserMsg = userMsg;    /* salvato per addToHistory() nel finished handler */

    QString sys = m_sysEdit->toPlainText().trimmed();
    if (sys.isEmpty())
        sys = "Sei un assistente AI utile e preciso. "
              "Rispondi SEMPRE e SOLO in italiano. "
              "Quando fornisci formule matematiche usa la notazione y = f(x).";

    if (!m_pendingImg.isEmpty()) {
        m_ai->chatWithImage(sys, userMsg, m_pendingImg, m_pendingImgMime);
        m_pendingImg.clear();
        m_pendingImgMime.clear();
        m_imgPreview->setVisible(false);
        return;
    }

    const AiClient::QueryType qt = AiClient::classifyQuery(userMsg);
    const bool hasHistory = !m_history.isEmpty() || !m_historySummary.isEmpty();

    if (hasHistory) {
        /* Storia attiva: usa /api/chat con i turni precedenti */
        m_ai->chat(sys, userMsg, buildHistoryArray(), qt);
    } else {
        /* Prima query o storia svuotata: usa /api/generate (single-shot) */
        m_ai->generate(sys, userMsg, qt);
    }
}

/* ══════════════════════════════════════════════════════════════
   updateRagBtn — aggiorna aspetto bottone RAG e label chunk
   ══════════════════════════════════════════════════════════════ */
void OracoloPage::updateRagBtn() {
    int n = m_rag.chunkCount();
    m_ragLbl->setText(n > 0 ? QString("%1\xf0\x9f\x93\x84").arg(n) : "");
    m_btnRag->setEnabled(n > 0 || m_ragEnabled);
    if (n == 0 && m_ragEnabled) {
        m_ragEnabled = false;
        m_btnRag->setChecked(false);
    }
}

/* ══════════════════════════════════════════════════════════════
   addToHistory — registra il turno e comprime se necessario
   ══════════════════════════════════════════════════════════════ */
void OracoloPage::addToHistory(const QString& user, const QString& assistant) {
    /* Tronca risposte molto lunghe (es. codice sorgente 5000 chars)
     * per non gonfiare il contesto nelle chiamate successive. */
    m_history.append({ user, assistant.left(800) });
    compressHistory();
}

/* ══════════════════════════════════════════════════════════════
   compressHistory — sposta i turni eccedenti nel summary locale
   Il summary ha il formato:
     "U: <testo utente>\nA: <risposta AI>\n"
   per ciascun turno compresso. Questo viene poi iniettato come
   turno sintetico "assistant" all'inizio della storia per dare
   contesto senza mandare tutti i raw.
   ══════════════════════════════════════════════════════════════ */
void OracoloPage::compressHistory() {
    if (m_history.size() <= kMaxRecentTurns) return;

    const int toCompress = m_history.size() - kMaxRecentTurns;
    QString addition;
    for (int i = 0; i < toCompress; ++i) {
        const ConvTurn& t = m_history[i];
        /* Tronca ulteriormente per il summary */
        addition += "U: " + t.user.left(200) + "\n";
        addition += "A: " + t.assistant.left(300) + "\n";
    }

    if (!m_historySummary.isEmpty())
        m_historySummary += "\n";
    m_historySummary += addition;

    m_history = m_history.mid(toCompress);
}

/* ══════════════════════════════════════════════════════════════
   buildHistoryArray — costruisce il QJsonArray per AiClient::chat()
   Struttura:
   - Se c'è un summary: 1 turno sintetico (user="[riepilogo]", assistant=summary)
   - Poi i turni recenti nella forma [{role:user,...},{role:assistant,...}]
   ══════════════════════════════════════════════════════════════ */
QJsonArray OracoloPage::buildHistoryArray() const {
    QJsonArray arr;

    if (!m_historySummary.isEmpty()) {
        QJsonObject u; u["role"] = "user";
        u["content"] = "[Riepilogo dei turni precedenti della conversazione]";
        QJsonObject a; a["role"] = "assistant";
        a["content"] = m_historySummary;
        arr.append(u);
        arr.append(a);
    }

    for (const ConvTurn& t : m_history) {
        QJsonObject u; u["role"] = "user";      u["content"] = t.user;
        QJsonObject a; a["role"] = "assistant"; a["content"] = t.assistant;
        arr.append(u);
        arr.append(a);
    }

    return arr;
}

/* ══════════════════════════════════════════════════════════════
   attachImage
   ══════════════════════════════════════════════════════════════ */
void OracoloPage::attachImage() {
    const QString path = QFileDialog::getOpenFileName(
        this, "Seleziona immagine", QString(),
        "Immagini (*.png *.jpg *.jpeg *.bmp *.webp *.gif);;Tutti i file (*)");
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return;
    const QByteArray raw = f.readAll();

    const QString lower = path.toLower();
    QString mime = "image/jpeg";
    if      (lower.endsWith(".png"))  mime = "image/png";
    else if (lower.endsWith(".gif"))  mime = "image/gif";
    else if (lower.endsWith(".webp")) mime = "image/webp";
    else if (lower.endsWith(".bmp"))  mime = "image/bmp";

    m_pendingImg     = raw.toBase64();
    m_pendingImgMime = mime;

    QPixmap px;
    px.loadFromData(raw);
    if (!px.isNull()) {
        m_imgPreview->setPixmap(
            px.scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        m_imgPreview->setVisible(true);
    }
}

/* ══════════════════════════════════════════════════════════════
   clearChat
   ══════════════════════════════════════════════════════════════ */
void OracoloPage::clearChat() {
    while (m_chatLay->count() > 1) {
        QLayoutItem* item = m_chatLay->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
    m_activeBubble = nullptr;
    m_pendingImg.clear();
    m_pendingImgMime.clear();
    m_imgPreview->setVisible(false);
    /* Reset storia: una nuova conversazione parte senza contesto precedente */
    m_history.clear();
    m_historySummary.clear();
    m_lastUserMsg.clear();
}

/* ══════════════════════════════════════════════════════════════
   addUserBubble / addAIBubble
   ══════════════════════════════════════════════════════════════ */
ChatBubble* OracoloPage::addUserBubble(const QString& text) {
    auto* bubble = new ChatBubble(ChatBubble::User, "Tu", text, m_chatContainer);
    connect(bubble, &ChatBubble::editRequested, this, [this](const QString& txt) {
        if (m_ai->busy()) return;   /* non modificare mentre l'AI risponde */
        m_input->setPlainText(txt);
        m_input->setFocus();
        QTextCursor cur = m_input->textCursor();
        cur.movePosition(QTextCursor::End);
        m_input->setTextCursor(cur);
    });
    m_chatLay->insertWidget(m_chatLay->count() - 1, bubble);
    scrollToBottom();
    return bubble;
}

ChatBubble* OracoloPage::addAIBubble(const QString& senderName) {
    auto* bubble = new ChatBubble(ChatBubble::AI, senderName, {}, m_chatContainer);
    connect(bubble, &ChatBubble::chartRequested,
            this, [this, bubble](const QString& formula) {
        onChartRequested(bubble, formula);
    });
    m_chatLay->insertWidget(m_chatLay->count() - 1, bubble);
    scrollToBottom();
    return bubble;
}

/* ══════════════════════════════════════════════════════════════
   _oracoloAskChartDest — chiede dove mostrare il grafico.
   Ritorna true per "Sezione Grafico", false per inline nella bolla.
   ══════════════════════════════════════════════════════════════ */
static bool _oracoloAskChartDest(QWidget* parent) {
    QMessageBox dlg(parent);
    dlg.setWindowTitle("Dove mostrare il grafico?");
    dlg.setText(
        "\xf0\x9f\x93\x88  \xc3\x88 stata rilevata una formula o un insieme di punti.\n\n"
        "Vuoi mostrare il grafico <b>qui</b> nella bolla oppure aprire la sezione "
        "<b>Grafico</b> dove puoi zoomare, esportare e personalizzarlo?");
    dlg.setTextFormat(Qt::RichText);
    dlg.setIcon(QMessageBox::Question);
    auto* btnQui  = dlg.addButton("  \xf0\x9f\x96\xbc  Questa scheda  ", QMessageBox::AcceptRole);
    auto* btnGraf = dlg.addButton("  \xf0\x9f\x93\x88  Sezione Grafico  ", QMessageBox::RejectRole);
    dlg.setDefaultButton(btnQui);
    Q_UNUSED(btnGraf)
    dlg.exec();
    return (dlg.clickedButton() == btnGraf);
}

/* ══════════════════════════════════════════════════════════════
   showCartesianChart — piano cartesiano con punti discreti
   ══════════════════════════════════════════════════════════════ */
void OracoloPage::showCartesianChart(const QVector<QPointF>& points) {
    if (_oracoloAskChartDest(this)) {
        /* → tab Grafico */
        emit requestShowInGrafico({}, 0.0, 0.0, points);
        return;
    }

    /* → inline nella bolla */
    auto* bubble = addAIBubble("\xf0\x9f\x93\x8a  Piano Cartesiano");

    /* Descrizione testuale dei punti */
    QString desc = "Grafico cartesiano con ";
    if (points.size() == 1)
        desc += QString("il punto P(%1, %2).")
            .arg(points[0].x(), 0, 'g').arg(points[0].y(), 0, 'g');
    else {
        desc += QString("%1 punti: ").arg(points.size());
        for (int i = 0; i < points.size(); ++i) {
            if (i) desc += ", ";
            desc += QString("P%1(%2, %3)").arg(i + 1)
                .arg(points[i].x(), 0, 'g').arg(points[i].y(), 0, 'g');
        }
        desc += ".";
    }
    bubble->appendToken(desc);
    bubble->finalizeStream();

    /* Calcola range includendo sempre l'origine (0,0) per un piano completo */
    double xMin = 0, xMax = 0, yMin = 0, yMax = 0;
    for (const auto& pt : points) {
        xMin = std::min(xMin, pt.x());
        xMax = std::max(xMax, pt.x());
        yMin = std::min(yMin, pt.y());
        yMax = std::max(yMax, pt.y());
    }
    const double padX = std::max(std::abs(xMax - xMin) * 0.25, 1.0);
    const double padY = std::max(std::abs(yMax - yMin) * 0.25, 1.0);

    auto* chart = new ChartWidget;
    chart->setTitle("\xf0\x9f\x93\x8d  Piano Cartesiano");
    chart->setAxisLabels("x", "y");
    chart->setRange(xMin - padX, xMax + padX, yMin - padY, yMax + padY);

    ChartWidget::Series s;
    s.name       = "Punti";
    s.pts        = points;
    s.mode       = ChartWidget::Scatter;
    s.showLabels = true;
    s.color      = QColor("#0ea5e9");
    chart->addSeries(s);

    bubble->embedChart(chart);
    scrollToBottom();
}

/* ══════════════════════════════════════════════════════════════
   onChartRequested — genera e incorpora il grafico nella bolla
   ══════════════════════════════════════════════════════════════ */
void OracoloPage::onChartRequested(ChatBubble* bubble, const QString& formula) {
    FormulaParser fp(formula);
    if (!fp.ok()) return;

    double xMin = -10.0, xMax = 10.0;
    const QString plain = bubble->plainText();
    {
        static const QRegularExpression reRange(
            R"(\[\s*(-?[\d.]+)\s*,\s*(-?[\d.]+)\s*\])");
        auto m = reRange.match(plain);
        if (m.hasMatch()) {
            xMin = m.captured(1).toDouble();
            xMax = m.captured(2).toDouble();
        }
    }

    const auto pts = fp.sample(xMin, xMax, 500);
    if (pts.isEmpty()) return;

    if (_oracoloAskChartDest(this)) {
        /* → tab Grafico */
        emit requestShowInGrafico(formula, xMin, xMax, {});
        return;
    }

    /* → inline nella bolla */
    auto* chart = new ChartWidget;
    chart->setTitle(QString("y = %1").arg(formula));
    chart->setAxisLabels("x", "y");
    ChartWidget::Series s;
    s.name = formula;
    s.pts  = pts;
    chart->addSeries(s);

    bubble->embedChart(chart);
}

/* ══════════════════════════════════════════════════════════════
   scrollToBottom
   ══════════════════════════════════════════════════════════════ */
void OracoloPage::scrollToBottom() {
    QTimer::singleShot(30, m_scroll, [this] {
        auto* sb = m_scroll->verticalScrollBar();
        if (sb) sb->setValue(sb->maximum());
    });
}
