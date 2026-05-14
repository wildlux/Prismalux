#include "lavoro_page.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QFont>
#include <QTextCursor>
#include <QScrollBar>

/* ══════════════════════════════════════════════════════════════
   LavoroPage — Assistente AI per il lavoro
   ══════════════════════════════════════════════════════════════ */
LavoroPage::LavoroPage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(8, 8, 8, 8);
    vbox->setSpacing(8);

    auto* title = new QLabel(QString::fromUtf8("\xf0\x9f\x92\xbc Assistente Lavoro AI"), this);
    QFont tf = title->font();
    tf.setPointSize(14);
    tf.setBold(true);
    title->setFont(tf);
    title->setAlignment(Qt::AlignCenter);
    vbox->addWidget(title);

    auto* inputLbl = new QLabel("CV / offerta / domanda:", this);
    vbox->addWidget(inputLbl);

    m_input = new QTextEdit(this);
    m_input->setPlaceholderText(
        "Incolla il CV, l'offerta di lavoro, le tue competenze...\n"
        "Poi premi una delle azioni qui sotto.");
    m_input->setFixedHeight(100);
    vbox->addWidget(m_input);

    /* Griglia azioni 2×3 */
    struct Action { const char* icon; const char* label; const char* sys; };
    static const Action kActions[] = {
        { "\xf0\x9f\x93\x84", "Analizza CV",
          "Sei un esperto recruiter HR. Analizza il CV fornito: evidenzia punti di forza, "
          "lacune, competenze chiave e suggerisci miglioramenti specifici. Rispondi in italiano." },
        { "\xe2\x9c\x89\xef\xb8\x8f", "Lettera presentazione",
          "Sei un esperto di ricerca lavoro. Scrivi una lettera di presentazione professionale "
          "e convincente basata sulle informazioni fornite. Sii formale e personalizzato. Rispondi in italiano." },
        { "\xf0\x9f\x8e\xa4", "Prepara colloquio",
          "Sei un coach per colloqui di lavoro. Elenca le domande probabili per questo ruolo, "
          "suggerisci le risposte migliori con esempi e dai consigli pratici. Rispondi in italiano." },
        { "\xf0\x9f\x94\x8d", "Cerca offerte",
          "Sei un consulente per la job search. Suggerisci le piattaforme migliori, parole chiave "
          "per la ricerca, settori adatti al profilo e strategie pratiche. Rispondi in italiano." },
        { "\xf0\x9f\x92\xb0", "Stima stipendio",
          "Sei un esperto del mercato del lavoro italiano. Stima la RAL (Reddito Annuo Lordo) "
          "realistica per il profilo/ruolo/settore fornito, spiegando i fattori che incidono. Rispondi in italiano." },
        { "\xf0\x9f\x8c\x90", "Traduci CV (EN)",
          "Sei un traduttore HR professionale. Traduci il CV in inglese professionale, "
          "adattando la terminologia alle convenzioni del CV anglosassone. Mantieni struttura chiara." },
    };

    auto* grid = new QGridLayout;
    grid->setSpacing(6);
    int row = 0, col = 0;
    for (const auto& a : kActions) {
        auto* btn = new QPushButton(
            QString::fromUtf8(a.icon) + " " + a.label, this);
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        btn->setMinimumHeight(44);
        const QString sys = a.sys;
        connect(btn, &QPushButton::clicked, this, [this, sys]{ runAction(sys); });
        grid->addWidget(btn, row, col);
        if (++col == 2) { col = 0; ++row; }
    }
    vbox->addLayout(grid);

    /* Status + progress */
    auto* statRow = new QHBoxLayout;
    m_status = new QLabel("", this);
    m_status->setVisible(false);
    statRow->addWidget(m_status, 1);
    m_btnStop = new QPushButton(QString::fromUtf8("\xe2\x8f\xb9 Stop"), this);
    m_btnStop->setVisible(false);
    m_btnStop->setFixedWidth(72);
    statRow->addWidget(m_btnStop);
    vbox->addLayout(statRow);

    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 0);
    m_progress->setVisible(false);
    m_progress->setFixedHeight(4);
    vbox->addWidget(m_progress);

    /* Output */
    m_output = new QTextEdit(this);
    m_output->setReadOnly(true);
    m_output->setPlaceholderText("La risposta AI apparirà qui...");
    vbox->addWidget(m_output);

    connect(m_btnStop, &QPushButton::clicked, m_ai, &AiClient::abort);
    connect(m_ai, &AiClient::token,    this, &LavoroPage::onToken);
    connect(m_ai, &AiClient::finished, this, &LavoroPage::onFinished);
    connect(m_ai, &AiClient::error,    this, &LavoroPage::onError);
    connect(m_ai, &AiClient::aborted,  this, [this]{ onFinished(""); });
}

void LavoroPage::runAction(const QString& sys)
{
    if (m_busy || m_ai->busy()) {
        m_output->setPlainText(
            QString::fromUtf8("\xe2\x9a\xa0\xef\xb8\x8f")
            + " L'AI sta elaborando. Attendi il termine oppure premi Stop.");
        return;
    }
    const QString userMsg = m_input->toPlainText().trimmed();
    if (userMsg.isEmpty()) {
        m_output->setPlainText(
            QString::fromUtf8("\xe2\x9a\xa0\xef\xb8\x8f")
            + " Inserisci prima le informazioni nel campo di testo sopra.");
        return;
    }
    m_busy = true;
    m_output->clear();
    m_status->setText(QString::fromUtf8("\xe2\x8f\xb3") + " Elaborazione in corso...");
    m_status->setVisible(true);
    m_progress->setVisible(true);
    m_btnStop->setVisible(true);
    m_ai->chat(sys, userMsg);
}

void LavoroPage::onToken(const QString& t)
{
    if (!m_busy) return;
    m_output->moveCursor(QTextCursor::End);
    m_output->insertPlainText(t);
    m_output->verticalScrollBar()->setValue(m_output->verticalScrollBar()->maximum());
}

void LavoroPage::onFinished(const QString&)
{
    if (!m_busy) return;
    m_busy = false;
    m_status->setVisible(false);
    m_progress->setVisible(false);
    m_btnStop->setVisible(false);
}

void LavoroPage::onError(const QString& e)
{
    if (!m_busy) return;
    m_busy = false;
    m_status->setVisible(false);
    m_progress->setVisible(false);
    m_btnStop->setVisible(false);
    m_output->setPlainText(
        QString::fromUtf8("\xe2\x9d\x8c") + " Errore: " + e);
}
