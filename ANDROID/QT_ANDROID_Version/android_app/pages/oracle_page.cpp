#include "oracle_page.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QFont>
#include <QTextCursor>
#include <QScroller>

static const struct { const char* icon; const char* label; const char* prefix; } kPills[] = {
    { "\xf0\x9f\x94\xa2",              "Matematica", "Risolvi passo per passo: "             },
    { "\xf0\x9f\x92\xb0",              "Finanza",    "Calcola o spiega (finanza): "           },
    { "\xf0\x9f\x92\xbb",              "Dev",        "Scrivi codice per: "                    },
    { "\xe2\x9c\x8d\xef\xb8\x8f",     "Scrivi",     "Scrivi un testo su: "                   },
    { "\xe2\x9d\x93",                  "Aiuto",      "Spiegami in modo semplice: "            },
    { "\xf0\x9f\x94\x8d",              "Analizza",   "Analizza questo: "                      },
    { "\xf0\x9f\x93\x8b",              "Codice",     "Revisiona e migliora questo codice:\n"  },
    { "\xf0\x9f\x8c\x90",              "Traduci",    "Traduci in italiano: "                  },
    { "\xf0\x9f\xa7\xa0",              "Riassumi",   "Riassumi in 3 punti: "                  },
};
static constexpr int kPillCount = static_cast<int>(sizeof(kPills) / sizeof(kPills[0]));

static constexpr const char* kSysOracle =
    "Sei Oracle, assistente universale conciso e preciso. "
    "Rispondi in modo diretto, strutturato e chiaro. "
    "Usa markdown quando utile. Sii breve ma completo.";

OraclePage::OraclePage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(10, 8, 10, 8);
    vbox->setSpacing(8);

    /* Titolo */
    auto* title = new QLabel(
        QString::fromUtf8("\xe2\x9a\xa1") + " Oracle \xe2\x80\x94 Chat Rapida", this);
    {
        QFont f = title->font();
        f.setPointSize(14);
        f.setBold(true);
        title->setFont(f);
    }
    vbox->addWidget(title);

    /* Pillole: scrollabili orizzontalmente con swipe touch */
    auto* pillScroll = new QScrollArea(this);
    pillScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    pillScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    pillScroll->setFixedHeight(52);
    pillScroll->setWidgetResizable(true);
    QScroller::grabGesture(pillScroll->viewport(), QScroller::TouchGesture);

    auto* pillW   = new QWidget;
    auto* pillRow = new QHBoxLayout(pillW);
    pillRow->setContentsMargins(4, 4, 4, 4);
    pillRow->setSpacing(6);
    for (int i = 0; i < kPillCount; ++i) {
        auto* btn = new QPushButton(
            QString::fromUtf8(kPills[i].icon) + " " + kPills[i].label, pillW);
        btn->setObjectName("PillBtn");
        btn->setFixedHeight(36);
        btn->setProperty("pillIndex", i);
        connect(btn, &QPushButton::clicked, this, &OraclePage::onPillClicked);
        pillRow->addWidget(btn);
    }
    pillRow->addStretch();
    pillScroll->setWidget(pillW);
    vbox->addWidget(pillScroll);

    /* Output streaming */
    m_output = new QTextEdit(this);
    m_output->setReadOnly(true);
    m_output->setObjectName("OutputEdit");
    m_output->setPlaceholderText("Le risposte appariranno qui...");
    {
        QFont f = m_output->font();
        f.setPointSize(11);
        m_output->setFont(f);
    }
    QScroller::grabGesture(m_output->viewport(), QScroller::TouchGesture);
    vbox->addWidget(m_output, 1);

    /* Input + pulsanti Invia/Stop */
    auto* inputRow = new QHBoxLayout;
    inputRow->setSpacing(6);

    m_input = new QTextEdit(this);
    m_input->setFixedHeight(72);
    m_input->setPlaceholderText("Fai una domanda...");
    m_input->setObjectName("InputEdit");
    inputRow->addWidget(m_input, 1);

    auto* btnCol = new QVBoxLayout;
    btnCol->setSpacing(4);

    m_sendBtn = new QPushButton(
        QString::fromUtf8("\xe2\x9e\xa4") + " Invia", this);
    m_sendBtn->setObjectName("PrimaryBtn");
    m_sendBtn->setMinimumHeight(32);
    connect(m_sendBtn, &QPushButton::clicked, this, &OraclePage::onSendClicked);
    btnCol->addWidget(m_sendBtn);

    m_stopBtn = new QPushButton(
        QString::fromUtf8("\xe2\x8f\xb9") + " Stop", this);
    m_stopBtn->setObjectName("DangerBtn");
    m_stopBtn->setMinimumHeight(32);
    m_stopBtn->setVisible(false);
    connect(m_stopBtn, &QPushButton::clicked, m_ai, &AiClient::abort);
    btnCol->addWidget(m_stopBtn);

    inputRow->addLayout(btnCol);
    vbox->addLayout(inputRow);

    /* Status */
    m_statusLbl = new QLabel("", this);
    m_statusLbl->setObjectName("StatusLabel");
    vbox->addWidget(m_statusLbl);

    connect(m_ai, &AiClient::token,    this, &OraclePage::onToken);
    connect(m_ai, &AiClient::finished, this, &OraclePage::onFinished);
    connect(m_ai, &AiClient::error,    this, &OraclePage::onAiError);
    connect(m_ai, &AiClient::aborted,  this, &OraclePage::onAborted);
}

void OraclePage::onPillClicked()
{
    auto* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    const int idx = btn->property("pillIndex").toInt();
    if (idx < 0 || idx >= kPillCount) return;
    m_input->setPlainText(QString::fromUtf8(kPills[idx].prefix));
    m_input->moveCursor(QTextCursor::End);
    m_input->setFocus();
}

void OraclePage::onSendClicked()
{
    const QString msg = m_input->toPlainText().trimmed();
    if (msg.isEmpty()) return;
    setBusy(true);
    m_output->clear();
    m_ai->chat(kSysOracle, msg);
}

void OraclePage::onToken(const QString& t)
{
    m_output->moveCursor(QTextCursor::End);
    m_output->insertPlainText(t);
    m_output->moveCursor(QTextCursor::End);
}

void OraclePage::onFinished(const QString&)
{
    setBusy(false);
    m_statusLbl->setText("");
}

void OraclePage::onAiError(const QString& msg)
{
    setBusy(false);
    m_statusLbl->setText(QString::fromUtf8("\xe2\x9d\x8c") + " " + msg);
}

void OraclePage::onAborted()
{
    setBusy(false);
    m_statusLbl->setText("Risposta interrotta.");
}

void OraclePage::setBusy(bool busy)
{
    m_sendBtn->setEnabled(!busy);
    m_stopBtn->setVisible(busy);
    m_input->setEnabled(!busy);
    if (busy)
        m_statusLbl->setText(
            QString::fromUtf8("\xe2\x8f\xb3") + " Oracle sta pensando...");
}
