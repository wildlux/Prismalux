#include "pratico_page.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QGridLayout>
#include <QPushButton>
#include <memory>
/* ══════════════════════════════════════════════════════════════
   Chat panel riutilizzabile
   ══════════════════════════════════════════════════════════════ */
QWidget* PraticoPage::buildChat(const QString& title,
                                 const QString& sysPrompt,
                                 const QString& placeholder) {
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(24, 20, 24, 16);
    lay->setSpacing(10);

    auto* hdr = new QWidget(w);
    auto* hdrL = new QHBoxLayout(hdr);
    hdrL->setContentsMargins(0,0,0,0);
    auto* back = new QPushButton("← Torna", hdr);
    back->setObjectName("actionBtn");
    auto* lbl  = new QLabel(title, hdr);
    lbl->setObjectName("pageTitle");
    hdrL->addWidget(back);
    hdrL->addWidget(lbl, 1);
    lay->addWidget(hdr);

    auto* div = new QFrame(w);
    div->setObjectName("pageDivider"); div->setFrameShape(QFrame::HLine);
    lay->addWidget(div);

    auto* log = new QTextEdit(w);
    log->setObjectName("chatLog");
    log->setReadOnly(true);
    log->setPlaceholderText(placeholder);
    lay->addWidget(log, 1);

    auto* inputRow = new QWidget(w);
    auto* inL      = new QHBoxLayout(inputRow);
    inL->setContentsMargins(0,0,0,0); inL->setSpacing(8);
    auto* inp = new QLineEdit(inputRow);
    inp->setObjectName("chatInput");
    inp->setPlaceholderText("Scrivi la tua domanda...");
    inp->setFixedHeight(38);
    auto* send = new QPushButton("Invia ▶", inputRow);
    send->setObjectName("actionBtn");
    auto* stop = new QPushButton("⏹", inputRow);
    stop->setObjectName("actionBtn");
    stop->setProperty("danger", true);
    stop->setFixedWidth(40);
    stop->setEnabled(false);
    inL->addWidget(inp, 1);
    inL->addWidget(send);
    inL->addWidget(stop);
    lay->addWidget(inputRow);

    /* Flag per ignorare segnali non destinati a questo panel */
    auto active = std::make_shared<bool>(false);

    /* Connessioni */
    connect(back, &QPushButton::clicked, this, [this]{ m_inner->setCurrentIndex(0); });
    connect(stop, &QPushButton::clicked, m_ai, &AiClient::abort);

    auto sendFn = [=]{
        QString msg = inp->text().trimmed();
        if (msg.isEmpty()) return;
        log->append(QString("\n👤  Tu: %1\n").arg(msg));
        log->append("🤖  AI: ");
        inp->clear();
        send->setEnabled(false); stop->setEnabled(true);
        *active = true;
        m_ai->chat(sysPrompt, msg);
    };

    connect(send, &QPushButton::clicked, this, sendFn);
    connect(inp,  &QLineEdit::returnPressed, this, sendFn);

    connect(m_ai, &AiClient::token, this, [=](const QString& t){
        if (!*active) return;
        QTextCursor c(log->document()); c.movePosition(QTextCursor::End);
        c.insertText(t); log->ensureCursorVisible();
    });
    connect(m_ai, &AiClient::finished, this, [=](const QString&){
        if (!*active) return;
        *active = false;
        log->append("\n──────────");
        send->setEnabled(true); stop->setEnabled(false);
    });
    connect(m_ai, &AiClient::error, this, [=](const QString& err){
        if (!*active) return;
        *active = false;
        const QString hint = (m_ai->backend() == AiClient::Ollama)
            ? "\xf0\x9f\x92\xa1  Verifica che Ollama sia in esecuzione: ollama serve"
            : "\xf0\x9f\x92\xa1  Verifica che llama-server sia avviato sulla porta corretta.";
        log->append(QString("\n\xe2\x9d\x8c  Errore: %1\n%2").arg(err, hint));
        send->setEnabled(true); stop->setEnabled(false);
    });
    connect(m_ai, &AiClient::aborted, this, [=]{
        if (!*active) return;
        *active = false;
        log->append("\n\xe2\x8f\xb9  Interrotto.\n\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80");
        send->setEnabled(true); stop->setEnabled(false);
    });

    return w;
}

/* ══════════════════════════════════════════════════════════════
   Menu principale Pratico
   ══════════════════════════════════════════════════════════════ */
QWidget* PraticoPage::buildMenu() {
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(24, 20, 24, 16);
    lay->setSpacing(12);

    auto* title = new QLabel("\xf0\x9f\x92\xb0  Finanza Personale", w);
    title->setObjectName("pageTitle");
    auto* sub   = new QLabel("Assistenti AI per fiscalit\xc3\xa0, gestione finanziaria e mercato del lavoro", w);
    sub->setObjectName("pageSubtitle");
    lay->addWidget(title);
    lay->addWidget(sub);
    auto* div = new QFrame(w); div->setObjectName("pageDivider");
    div->setFrameShape(QFrame::HLine); lay->addWidget(div);

    struct Item { QString icon, title, desc; int page; };
    QList<Item> items = {
        {"📄", "Assistente 730",
         "Guida alla compilazione del 730, detrazioni, rimborsi fiscali.", 1},
        {"💼", "Partita IVA",
         "Regime forfettario, calcolo imposte, obblighi contributivi.", 2},
        {"🔍", "Cerca Lavoro",
         "Ricerca offerte, consigli CV, preparazione colloquio.", 3},
    };

    auto* grid = new QWidget(w);
    auto* gLay = new QVBoxLayout(grid);
    gLay->setSpacing(10);
    gLay->setContentsMargins(0,0,0,0);

    for (auto& it : items) {
        /* Card con bottone "Apri →" integrato */
        auto* card = new QFrame(grid);
        card->setObjectName("actionCard");
        auto* cl = new QHBoxLayout(card);
        cl->setContentsMargins(16, 14, 16, 14); cl->setSpacing(14);

        auto* ico = new QLabel(it.icon, card);
        ico->setObjectName("cardIcon"); ico->setFixedWidth(32);

        auto* txt  = new QWidget(card);
        auto* txtL = new QVBoxLayout(txt);
        txtL->setContentsMargins(0,0,0,0); txtL->setSpacing(3);
        auto* lt = new QLabel(it.title, txt); lt->setObjectName("cardTitle");
        auto* ld = new QLabel(it.desc,  txt); ld->setObjectName("cardDesc");
        ld->setWordWrap(true);
        txtL->addWidget(lt); txtL->addWidget(ld);

        int pg = it.page;
        auto* goBtn = new QPushButton("Apri →", card);
        goBtn->setObjectName("actionBtn"); goBtn->setFixedWidth(80);
        connect(goBtn, &QPushButton::clicked, this, [this, pg]{ m_inner->setCurrentIndex(pg); });

        cl->addWidget(ico); cl->addWidget(txt, 1); cl->addWidget(goBtn);
        gLay->addWidget(card);
    }

    gLay->addStretch(1);
    lay->addWidget(grid, 1);
    return w;
}

PraticoPage::PraticoPage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0,0,0,0);

    m_inner = new QStackedWidget(this);

    /* 0 = menu */
    m_inner->addWidget(buildMenu());

    /* 1 = 730 */
    m_inner->addWidget(buildChat(
        "📄  Assistente 730",
        "Sei un esperto fiscalista italiano specializzato nel modello 730. "
        "Fornisci guide chiare, cita gli articoli di legge quando rilevante. "
        "Rispondi SEMPRE e SOLO in italiano.",
        "Es: Posso detrarre le spese mediche per mio figlio?\n"
        "Es: Qual è la differenza tra 730 precompilato e ordinario?"));

    /* 2 = P.IVA */
    m_inner->addWidget(buildChat(
        "💼  Partita IVA — Regime Forfettario",
        "Sei un commercialista italiano esperto in regime forfettario e partita IVA. "
        "Fornisci calcoli precisi e consigli pratici. "
        "Rispondi SEMPRE e SOLO in italiano.",
        "Es: Come calcolo le tasse nel regime forfettario al 15%?\n"
        "Es: Quali sono i limiti di fatturato per restare nel forfettario?"));

    /* 3 = Cerca Lavoro */
    m_inner->addWidget(buildChat(
        "🔍  Cerca Lavoro",
        "Sei un career coach italiano esperto in ricerca del lavoro, CV e colloqui. "
        "Aiuta con strategie concrete e moderne. "
        "Rispondi SEMPRE e SOLO in italiano.",
        "Es: Come migliorare il mio CV per una posizione da sviluppatore?\n"
        "Es: Quali domande fanno di solito nei colloqui tecnici?"));

    lay->addWidget(m_inner);
}
