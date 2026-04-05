#include "materie_page.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QFrame>
#include <QPushButton>
#include <QTreeWidgetItem>
#include <QScrollArea>
#include <QSplitter>
#include <QTextCursor>
#include <memory>

/* ══════════════════════════════════════════════════════════════
   SubjectTutorWidget
   ══════════════════════════════════════════════════════════════ */
SubjectTutorWidget::SubjectTutorWidget(AiClient* ai, const QString& subject,
                                       const SubjectData& data, QWidget* parent)
    : QWidget(parent), m_ai(ai), m_subject(subject)
{
    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(0,0,0,0); lay->setSpacing(0);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);

    /* ── Pannello sinistro: albero argomenti ── */
    auto* leftW = new QWidget(splitter);
    auto* leftL = new QVBoxLayout(leftW);
    leftL->setContentsMargins(12,12,0,12); leftL->setSpacing(6);

    auto* treeTitle = new QLabel(QString("📖 Argomenti — %1").arg(subject), leftW);
    treeTitle->setObjectName("cardTitle");
    leftL->addWidget(treeTitle);

    auto* treeHint = new QLabel("Doppio clic per spiegazione AI", leftW);
    treeHint->setObjectName("pageSubtitle");
    leftL->addWidget(treeHint);

    m_tree = new QTreeWidget(leftW);
    m_tree->setObjectName("subjectTree");
    m_tree->setHeaderHidden(true);
    m_tree->setIndentation(16);

    for (const auto& section : data) {
        auto* secItem = new QTreeWidgetItem(m_tree, {section.first});
        secItem->setFlags(secItem->flags() & ~Qt::ItemIsSelectable);
        QFont f = secItem->font(0); f.setBold(true); secItem->setFont(0, f);
        for (const auto& topic : section.second) {
            auto* topicItem = new QTreeWidgetItem(secItem, {topic.first});
            topicItem->setData(0, Qt::UserRole,     topic.first);
            topicItem->setData(0, Qt::UserRole + 1, topic.second);
            topicItem->setToolTip(0, topic.second);
        }
    }
    m_tree->expandAll();
    leftL->addWidget(m_tree, 1);
    splitter->addWidget(leftW);

    /* ── Pannello destro: chat ── */
    auto* rightW = new QWidget(splitter);
    auto* rightL = new QVBoxLayout(rightW);
    rightL->setContentsMargins(12,12,12,12); rightL->setSpacing(8);

    m_log = new QTextEdit(rightW);
    m_log->setObjectName("chatLog");
    m_log->setReadOnly(true);
    m_log->setPlaceholderText(
        QString("🏛️  Tutor — %1\n\nClicca un argomento nell'albero a sinistra\n"
                "oppure scrivi una domanda libera qui sotto.").arg(subject));
    rightL->addWidget(m_log, 1);

    auto* inputRow = new QWidget(rightW);
    auto* inL      = new QHBoxLayout(inputRow);
    inL->setContentsMargins(0,0,0,0); inL->setSpacing(8);

    m_inp = new QLineEdit(inputRow);
    m_inp->setObjectName("chatInput");
    m_inp->setPlaceholderText("Domanda libera...");
    m_inp->setFixedHeight(38);

    m_send = new QPushButton("Chiedi \u25b6", inputRow);
    m_send->setObjectName("actionBtn");

    m_stop = new QPushButton("\u23f9", inputRow);
    m_stop->setObjectName("actionBtn");
    m_stop->setProperty("danger", true);
    m_stop->setFixedWidth(40);
    m_stop->setEnabled(false);

    inL->addWidget(m_inp, 1);
    inL->addWidget(m_send);
    inL->addWidget(m_stop);
    rightL->addWidget(inputRow);

    /* ⏳ Indicatore attesa AI — visibile solo durante elaborazione */
    m_waitLbl = new QLabel(
        "\xe2\x8f\xb3  Elaborazione in corso \xe2\x80\x94 il modello sta generando la risposta...",
        rightW);
    m_waitLbl->setStyleSheet("color: #E5C400; padding: 2px 0; font-style: italic;");
    m_waitLbl->setVisible(false);
    rightL->addWidget(m_waitLbl);
    splitter->addWidget(rightW);

    /* Proporzioni 30/70 */
    splitter->setStretchFactor(0, 30);
    splitter->setStretchFactor(1, 70);
    lay->addWidget(splitter);

    /* Flag per evitare segnali fantasma da altri pannelli */
    auto active = std::make_shared<bool>(false);

    /* Doppio clic su argomento → spiegazione automatica.
     * Usato itemDoubleClicked (non itemClicked) per evitare che ogni
     * singolo clic durante la navigazione dell'albero lanci una chiamata AI,
     * causando spike di RAM e richieste sovrapposte. */
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this,
            [this, active](QTreeWidgetItem* item, int) {
        if (!item->parent()) return; // sezione header, non argomento
        QString name = item->data(0, Qt::UserRole).toString();
        QString desc = item->data(0, Qt::UserRole + 1).toString();
        if (name.isEmpty()) return;
        askTopic(name, desc);
        *active = true;
    });

    connect(m_stop, &QPushButton::clicked, m_ai, &AiClient::abort);

    auto sendFn = [this, active]{
        QString msg = m_inp->text().trimmed();
        if (msg.isEmpty()) return;
        *active = true;
        m_log->append(QString("\n\U0001f464  Tu: %1\n").arg(msg));
        m_log->append(QString("\U0001f916  %1: ").arg(m_subject));
        m_inp->clear();
        m_send->setEnabled(false); m_stop->setEnabled(true);
        m_waitLbl->setVisible(true);
        QString sys = QString("Sei l'Oracolo di Prismalux, tutor esperto in %1. "
            "Spiega in modo chiaro con esempi pratici, analogie concrete e passi logici. "
            "Adatta la spiegazione al livello dell'utente (da scolastico ad avanzato). "
            "Rispondi SEMPRE e SOLO in italiano.").arg(m_subject);
        m_ai->chat(sys, msg);
    };

    connect(m_send, &QPushButton::clicked, this, sendFn);
    connect(m_inp,  &QLineEdit::returnPressed, this, sendFn);

    connect(m_ai, &AiClient::token, this, [this, active](const QString& t){
        if (!*active) return;
        m_waitLbl->setVisible(false);  /* nasconde ⏳ al primo token ricevuto */
        QTextCursor c(m_log->document()); c.movePosition(QTextCursor::End);
        c.insertText(t); m_log->ensureCursorVisible();
    });
    connect(m_ai, &AiClient::finished, this, [this, active](const QString&){
        if (!*active) return;
        *active = false;
        m_waitLbl->setVisible(false);
        m_log->append("\n\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500");
        m_send->setEnabled(true); m_stop->setEnabled(false);
    });
    connect(m_ai, &AiClient::error, this, [this, active](const QString& err){
        if (!*active) return;
        *active = false;
        m_waitLbl->setVisible(false);
        const QString hint = (m_ai->backend() == AiClient::Ollama)
            ? "\U0001f4a1  Verifica che Ollama sia in esecuzione: ollama serve"
            : "\U0001f4a1  Verifica che llama-server sia avviato sulla porta corretta.";
        m_log->append(QString("\n\u274c  Errore: %1\n%2").arg(err, hint));
        m_send->setEnabled(true); m_stop->setEnabled(false);
    });

    /* Ripristina i pulsanti anche quando l'utente preme Stop (segnale aborted).
     * Senza questa connessione, onFinished() ritorna early perché m_reply è già
     * nullptr dopo abort(), e finished() non viene mai emesso: i pulsanti restano
     * bloccati (Send disabilitato, Stop abilitato) fino al riavvio dell'app. */
    connect(m_ai, &AiClient::aborted, this, [this, active]{
        if (!*active) return;
        *active = false;
        m_waitLbl->setVisible(false);
        m_log->append("\n\u23f9  Interrotto.\n\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500");
        m_send->setEnabled(true); m_stop->setEnabled(false);
    });
}

void SubjectTutorWidget::askTopic(const QString& topicName, const QString& topicDesc)
{
    /* Annulla eventuale chiamata AI ancora in corso prima di iniziarne una nuova.
     * Senza questo abort(), chiamate rapide successive si accumulano e
     * saturano la RAM perché il modello viene caricato più volte. */
    m_ai->abort();

    m_log->append(QString("\n\U0001f4d6  Argomento: %1\n").arg(topicName));
    m_log->append(QString("\U0001f916  %1: ").arg(m_subject));
    m_send->setEnabled(false); m_stop->setEnabled(true);
    m_waitLbl->setVisible(true);

    QString sys = QString(
        "Sei l'Oracolo di Prismalux, tutor esperto in %1. "
        "Spiega in modo chiaro con esempi pratici e analogie concrete. "
        "Rispondi SEMPRE e SOLO in italiano.").arg(m_subject);

    QString prompt = QString("Spiega l'argomento '%1'.\nContesto: %2\n\n"
        "Struttura la risposta con:\n"
        "1) Cos'è (definizione semplice)\n"
        "2) Come funziona (spiegazione con esempio)\n"
        "3) Perché è importante (uso pratico)\n"
        "4) Errori comuni (se rilevante)").arg(topicName, topicDesc);

    m_ai->chat(sys, prompt);
}

void SubjectTutorWidget::sendQuestion(const QString& question, const QString& sysSuffix)
{
    Q_UNUSED(question)
    Q_UNUSED(sysSuffix)
    // Implementazione alternativa — usata internamente se necessario
}

/* ══════════════════════════════════════════════════════════════
   MateriePage
   ══════════════════════════════════════════════════════════════ */
MateriePage::MateriePage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0,0,0,0);

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(buildMenu()); /* 0 */

    struct SubjectInfo {
        QString icon, name;
        SubjectData (*dataFn)();
    };
    QList<SubjectInfo> subjects = {
        {"📐", "Matematica",            tutorDataMatematica},
        {"⚛️",  "Fisica",               tutorDataFisica},
        {"🧪", "Chimica",              tutorDataChimica},
        {"🔒", "Sicurezza Informatica", tutorDataSicurezza},
        {"🐍", "Informatica",           tutorDataInformatica},
        {"⚡", "Algoritmi",             tutorDataAlgoritmi},
    };

    for (int i = 0; i < subjects.size(); i++) {
        const auto& s = subjects[i];
        auto* w  = new QWidget(m_stack);
        auto* wl = new QVBoxLayout(w);
        wl->setContentsMargins(0,0,0,0); wl->setSpacing(0);

        /* Header con tasto "← Materie" */
        auto* hdr = new QWidget(w);
        hdr->setFixedHeight(52);
        hdr->setObjectName("subjectHeader");
        auto* hl = new QHBoxLayout(hdr);
        hl->setContentsMargins(16, 8, 16, 8); hl->setSpacing(12);

        auto* back = new QPushButton("\u2190 Materie", hdr);
        back->setObjectName("actionBtn");
        connect(back, &QPushButton::clicked, this, [this]{ m_stack->setCurrentIndex(0); });

        auto* lbl = new QLabel(QString("%1  %2").arg(s.icon, s.name), hdr);
        lbl->setObjectName("pageTitle");

        hl->addWidget(back); hl->addWidget(lbl, 1);
        wl->addWidget(hdr);

        auto* div = new QFrame(w); div->setObjectName("pageDivider");
        div->setFrameShape(QFrame::HLine); wl->addWidget(div);

        auto* tutor = new SubjectTutorWidget(m_ai, s.name, s.dataFn(), w);
        wl->addWidget(tutor, 1);

        m_stack->addWidget(w); /* 1..6 */
    }

    /* Collega pulsanti menu → pagine soggetto */
    auto* menu = m_stack->widget(0);
    auto btns = menu->findChildren<QPushButton*>("subjectBtn");
    for (int i = 0; i < btns.size(); i++) {
        const int page = i + 1;
        connect(btns[i], &QPushButton::clicked, this, [this, page]{ m_stack->setCurrentIndex(page); });
    }

    lay->addWidget(m_stack);
}

QWidget* MateriePage::buildMenu()
{
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(24, 20, 24, 16); lay->setSpacing(12);

    auto* title = new QLabel("📖  Materie — Tutor per argomento", w);
    title->setObjectName("pageTitle");
    auto* sub = new QLabel("Scegli una materia e seleziona un argomento dall'albero per ottenere una spiegazione guidata.", w);
    sub->setObjectName("pageSubtitle"); sub->setWordWrap(true);
    lay->addWidget(title); lay->addWidget(sub);

    auto* div = new QFrame(w); div->setObjectName("pageDivider");
    div->setFrameShape(QFrame::HLine); lay->addWidget(div);

    struct SubjectCard { QString icon, name, desc; };
    QList<SubjectCard> cards = {
        {"📐", "Matematica",            "Basi, Calcolo, Algebra Lineare, Metodi Numerici, Probabilità"},
        {"⚛️",  "Fisica",               "Meccanica, Termodinamica, Elettromagnetismo, Quantistica"},
        {"🧪", "Chimica",              "Inorganica, Organica, Biochimica, Chimica Fisica"},
        {"🔒", "Sicurezza Informatica", "OWASP, Crittografia, Linux, Reti, Pentest, Virtualizzazione"},
        {"🐍", "Informatica",           "Python basi→avanzato, OOP, Design Patterns, Architettura"},
        {"⚡", "Algoritmi",             "Sorting, Grafi, DP, Greedy, Backtracking, Strutture Dati"},
    };

    auto* grid = new QWidget(w);
    auto* gLay = new QGridLayout(grid);
    gLay->setSpacing(10); gLay->setContentsMargins(0,0,0,0);

    for (int i = 0; i < cards.size(); i++) {
        const auto& c = cards[i];
        auto* card = new QFrame(grid); card->setObjectName("actionCard");
        auto* cl = new QHBoxLayout(card);
        cl->setContentsMargins(16, 12, 16, 12); cl->setSpacing(14);

        auto* ico = new QLabel(c.icon, card); ico->setObjectName("cardIcon");
        ico->setFixedWidth(38); ico->setAlignment(Qt::AlignCenter);

        auto* txt  = new QWidget(card);
        auto* txtL = new QVBoxLayout(txt); txtL->setContentsMargins(0,0,0,0); txtL->setSpacing(3);
        auto* lt = new QLabel(c.name, txt); lt->setObjectName("cardTitle");
        auto* ld = new QLabel(c.desc, txt); ld->setObjectName("cardDesc"); ld->setWordWrap(true);
        txtL->addWidget(lt); txtL->addWidget(ld);

        auto* btn = new QPushButton("Studia \u2192", card);
        btn->setObjectName("subjectBtn");  // findChildren() lo trova dopo
        btn->setFixedWidth(88);

        cl->addWidget(ico); cl->addWidget(txt, 1); cl->addWidget(btn);
        gLay->addWidget(card, i / 2, i % 2);
    }

    lay->addWidget(grid, 1);
    return w;
}
