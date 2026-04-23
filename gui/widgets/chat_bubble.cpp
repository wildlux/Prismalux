#include "chat_bubble.h"
#include "chart_widget.h"
#include "formula_parser.h"
#include "tts_speak.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QTextDocument>
#include <QTextCursor>
#include <QScrollBar>
#include <QClipboard>
#include <QGuiApplication>
#include <QProcess>
#include <QSizePolicy>
#include <QTimer>

/* ══════════════════════════════════════════════════════════════
   ChatBubble — costruttore
   ══════════════════════════════════════════════════════════════ */
ChatBubble::ChatBubble(Role role, const QString& sender,
                       const QString& text, QWidget* parent)
    : QFrame(parent), m_role(role)
{
    /* ── Frame esterno: trasparente (solo layout) ── */
    setObjectName("chatBubbleRow");
    setFrameStyle(QFrame::NoFrame);

    auto* outerLay = new QHBoxLayout(this);
    outerLay->setContentsMargins(12, 3, 12, 3);
    outerLay->setSpacing(0);

    /* ── Frame interno: la bubble vera ── */
    auto* inner = new QFrame(this);
    inner->setObjectName(role == User ? "bubbleUser" : "bubbleAI");
    inner->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    /* Max 78% della larghezza disponibile — aggiornato da resizeEvent se necessario */
    inner->setMaximumWidth(700);

    auto* vlay = new QVBoxLayout(inner);
    vlay->setContentsMargins(12, 8, 12, 8);
    vlay->setSpacing(4);

    /* ── Sender label ── */
    auto* senderLbl = new QLabel(sender, inner);
    senderLbl->setObjectName(role == User ? "bubbleSenderUser" : "bubbleSenderAI");
    vlay->addWidget(senderLbl);

    /* ── Testo ── */
    m_text = new QTextBrowser(inner);
    m_text->setObjectName("bubbleText");
    m_text->setReadOnly(true);
    m_text->setOpenExternalLinks(false);
    m_text->setFrameStyle(QFrame::NoFrame);
    m_text->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_text->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_text->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    m_text->document()->setDocumentMargin(2);
    /* Auto-resize in altezza man mano che il contenuto cresce */
    connect(m_text->document(), &QTextDocument::contentsChanged, m_text, [this] {
        int h = qMax(22, (int)m_text->document()->size().height() + 6);
        m_text->setFixedHeight(h);
    });
    vlay->addWidget(m_text);

    /* ── Contenitore grafico (nascosto inizialmente) ── */
    m_chartContainer = new QWidget(inner);
    m_chartContainer->setVisible(false);
    auto* chartLay = new QVBoxLayout(m_chartContainer);
    chartLay->setContentsMargins(0, 6, 0, 4);
    chartLay->setSpacing(0);
    vlay->addWidget(m_chartContainer);

    /* ── Barra azioni ── */
    auto* actionBar = new QWidget(inner);
    auto* btnLay    = new QHBoxLayout(actionBar);
    btnLay->setContentsMargins(0, 2, 0, 0);
    btnLay->setSpacing(4);

    /* Pulsante Grafico (visibile solo se formula rilevata) */
    m_btnChart = new QPushButton("\xf0\x9f\x93\x8a  Grafico", actionBar);
    m_btnChart->setObjectName("bubbleBtn");
    m_btnChart->setToolTip("Genera il grafico dalla formula rilevata");
    m_btnChart->setVisible(false);
    btnLay->addWidget(m_btnChart);

    btnLay->addStretch();

    m_btnCopy = new QPushButton("\xf0\x9f\x97\x82", actionBar);
    m_btnCopy->setObjectName("bubbleBtn");
    m_btnCopy->setToolTip("Copia il testo negli appunti");

    m_btnTts = new QPushButton("\xf0\x9f\x8e\x99", actionBar);
    m_btnTts->setObjectName("bubbleBtn");
    m_btnTts->setToolTip("Leggi ad alta voce");

    btnLay->addWidget(m_btnCopy);
    btnLay->addWidget(m_btnTts);

    /* Pulsante Modifica — solo per le bubble dell'utente */
    if (role == User) {
        m_btnEdit = new QPushButton("\xe2\x9c\x8f", actionBar);  /* ✏ */
        m_btnEdit->setObjectName("bubbleBtn");
        m_btnEdit->setToolTip("Modifica e rimanda questo messaggio");
        btnLay->addWidget(m_btnEdit);
    }

    vlay->addWidget(actionBar);

    /* ── Posiziona la bubble a sinistra (AI) o destra (Utente) ── */
    if (role == User) {
        outerLay->addStretch(1);
        outerLay->addWidget(inner, 0, Qt::AlignRight);
    } else {
        outerLay->addWidget(inner, 0, Qt::AlignLeft);
        outerLay->addStretch(1);
    }

    /* ── Testo iniziale ── */
    if (!text.isEmpty()) {
        m_plain = text;
        m_text->setPlainText(text);
    }

    /* ── Connessioni ── */
    connect(m_btnCopy,  &QPushButton::clicked, this, &ChatBubble::onCopy);
    connect(m_btnTts,   &QPushButton::clicked, this, &ChatBubble::onTTS);
    connect(m_btnChart, &QPushButton::clicked, this, [this] {
        QString f = FormulaParser::tryExtract(m_plain);
        if (!f.isEmpty()) emit chartRequested(f);
    });
    if (m_btnEdit) {
        connect(m_btnEdit, &QPushButton::clicked, this, [this] {
            emit editRequested(m_plain);
        });
    }
}

/* ══════════════════════════════════════════════════════════════
   appendToken — aggiunge un token in streaming
   Usa QTextCursor per non ricreare il documento ad ogni token.
   ══════════════════════════════════════════════════════════════ */
void ChatBubble::appendToken(const QString& token) {
    m_plain += token;
    QTextCursor cur(m_text->document());
    cur.movePosition(QTextCursor::End);
    cur.insertText(token);
}

/* ══════════════════════════════════════════════════════════════
   finalizeStream — chiamato alla fine dello stream:
   controlla se c'è una formula valida e mostra il pulsante.
   ══════════════════════════════════════════════════════════════ */
void ChatBubble::finalizeStream() {
    const QString formula = FormulaParser::tryExtract(m_plain);
    if (!formula.isEmpty()) {
        FormulaParser fp(formula);
        if (fp.ok()) m_btnChart->setVisible(true);
    }
}

/* ══════════════════════════════════════════════════════════════
   embedChart — incorpora un grafico già pronto sotto il testo
   ══════════════════════════════════════════════════════════════ */
void ChatBubble::embedChart(ChartWidget* chart) {
    auto* lay = qobject_cast<QVBoxLayout*>(m_chartContainer->layout());
    if (!lay) return;
    /* Rimuovi grafico precedente */
    while (lay->count()) {
        QLayoutItem* item = lay->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
    lay->addWidget(chart);
    m_chartContainer->setVisible(true);
    m_btnChart->setVisible(false);  /* sostituito dal grafico inline */
}

/* ══════════════════════════════════════════════════════════════
   onCopy — copia il testo negli appunti
   ══════════════════════════════════════════════════════════════ */
void ChatBubble::onCopy() {
    QGuiApplication::clipboard()->setText(m_plain);
    /* Feedback visivo temporaneo */
    m_btnCopy->setText("\xe2\x9c\x85");
    QTimer::singleShot(1500, m_btnCopy, [this] {
        m_btnCopy->setText("\xf0\x9f\x97\x82");
    });
}

/* ══════════════════════════════════════════════════════════════
   onTTS — legge il testo con espeak-ng (con fallback a spd-say)
   ══════════════════════════════════════════════════════════════ */
void ChatBubble::onTTS() {
    TtsSpeak::speak(m_plain);
}
