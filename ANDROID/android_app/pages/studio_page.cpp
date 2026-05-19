#include "studio_page.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QLabel>
#include <QFont>
#include <QTextCursor>
#include <QScrollBar>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QFrame>

/* ══════════════════════════════════════════════════════════════
   Materie disponibili
   ══════════════════════════════════════════════════════════════ */
struct Materia {
    const char* label;
    const char* context;
};

static const Materia kMaterie[] = {
    { "CCNA — Networking Cisco",
      "preparazione all'esame CCNA 200-301 (Cisco Certified Network Associate): "
      "modello OSI/TCP-IP, switching, VLAN, STP, routing statico/OSPF, IPv4/IPv6, "
      "NAT, ACL, DHCP, SSH, wireless 802.11, sicurezza di rete, "
      "automazione con Python/NETCONF. Usa comandi Cisco IOS reali dove pertinente." },
    { "CCNP — Networking Avanzato",
      "preparazione agli esami CCNP Enterprise/Security: OSPF avanzato, BGP, MPLS, "
      "SD-WAN, QoS, VPN IPsec/SSL, firewall Cisco ASA/FTD." },
    { "Linux (LPIC / CompTIA Linux+)",
      "sistema operativo Linux: filesystem, permessi, shell bash, gestione processi, "
      "systemd, networking, cron, SSH, sicurezza. Usa comandi reali con output tipico." },
    { "Cybersecurity (CEH / Security+)",
      "sicurezza informatica: threat landscape, crittografia, protocolli sicuri, "
      "penetration testing, OWASP Top 10, firewall, IDS/IPS, SIEM, incident response." },
    { "Docker & Kubernetes",
      "containerizzazione con Docker e orchestrazione con Kubernetes: Dockerfile, "
      "docker-compose, Pod, Deployment, Service, Ingress, Helm, kubectl." },
    { "Python Programming",
      "linguaggio Python: sintassi, strutture dati, OOP, librerie standard, "
      "gestione errori, I/O file, regex, requests, pandas, asyncio." },
    { "HTML / CSS / JavaScript",
      "sviluppo web: HTML5 semantico, CSS3 (flexbox/grid/animazioni), "
      "JavaScript ES6+ (arrow functions, promises, fetch, DOM manipulation, React basi)." },
    { "Matematica",
      "matematica: algebra, geometria, trigonometria, analisi matematica "
      "(limiti, derivate, integrali), probabilità e statistica, matrici." },
    { "Fisica",
      "fisica: meccanica classica, termodinamica, elettromagnetismo, ottica, "
      "relatività ristretta, fisica moderna. Usa formule e unità SI." },
    { "Chimica",
      "chimica: tavola periodica, legami chimici, stechiometria, "
      "chimica organica, equilibri, acidi/basi, elettrochimica." },
    { "Economia & Finanza",
      "economia e finanza: micro/macroeconomia, contabilità, bilancio, "
      "mercati finanziari, analisi fondamentale/tecnica, fiscalità italiana." },
    { "Storia",
      "storia: dalla preistoria all'età contemporanea, con focus su cause ed effetti, "
      "personaggi chiave, cronologia e contesto geopolitico." },
    { "Letteratura italiana",
      "letteratura italiana: autori (Dante, Petrarca, Boccaccio, Manzoni, Leopardi, "
      "Svevo, Calvino...), correnti letterarie, analisi del testo, figure retoriche." },
    { "Inglese (B2/C1 — IELTS/Cambridge)",
      "lingua inglese livello B2/C1: grammatica avanzata, phrasal verbs, "
      "vocabulary, writing formale, reading comprehension, preparazione IELTS/FCE." },
    { "Altro (specifica sotto)",
      "" },
};
static constexpr int kNMaterie = static_cast<int>(sizeof(kMaterie) / sizeof(kMaterie[0]));
static constexpr int kIdxAltro = kNMaterie - 1;
static constexpr int kIdxCcna  = 0;

/* ══════════════════════════════════════════════════════════════
   System prompt per modalità
   ══════════════════════════════════════════════════════════════ */
static const char* kModeInstructions[] = {
    /* 0 — Spiega */
    "Spiega l'argomento richiesto in modo chiaro e progressivo. "
    "Usa esempi concreti, analogie semplici e, dove pertinente, "
    "esempi di codice/comandi reali. Struttura la risposta con titoli brevi. "
    "Rispondi in italiano.",

    /* 1 — Quiz JSON strutturato */
    "Genera UNA sola domanda a risposta multipla, breve e diretta (max 15 parole).\n"
    "Rispondi SOLO con questo JSON (zero testo prima o dopo):\n"
    "{\"domanda\":\"[testo domanda]\","
    "\"risposta1\":\"[opzione A breve]\","
    "\"risposta2\":\"[opzione B breve]\","
    "\"risposta3\":\"[opzione C breve]\","
    "\"risposta4\":\"[opzione D breve]\","
    "\"corretta\":1,"
    "\"spiegazione\":\"[2-3 frasi che spiegano PERCHÉ la risposta è corretta "
    "e perché le altre sono sbagliate. Includi il concetto chiave da ricordare.]\"}\n"
    "Regole: sostituisci i placeholder; \"corretta\" = 1,2,3 o 4; lingua italiana.",

    /* 2 — Esercizio */
    "Proponi UN esercizio pratico sull'argomento, appropriato per un esame. "
    "Formato:\n"
    "**Esercizio:** [testo dettagliato del problema]\n\n"
    "**Soluzione passo-passo:**\n[step 1...]\n[step 2...]\n\n"
    "**Risposta finale:** [risultato]\n"
    "Rispondi in italiano.",

    /* 3 — Flashcard */
    "Genera 5 flashcard sull'argomento. Ogni flashcard ha:\n"
    "\xf0\x9f\x83\x8f **Fronte:** [domanda breve o termine]\n"  /* 🃏 */
    "\xe2\x9c\x85 **Retro:** [risposta o definizione concisa]\n\n"  /* ✅ */
    "Le flashcard devono coprire i concetti più importanti. "
    "Rispondi in italiano.",

    /* 4 — Mappa mentale */
    "Crea una mappa mentale testuale dell'argomento. "
    "Usa indentazione e simboli per rappresentare la gerarchia:\n"
    "\xf0\x9f\x8e\xaf [Argomento centrale]\n"  /* 🎯 */
    "  \xe2\x94\x9c\xe2\x94\x80 [Ramo 1]\n"
    "  \xe2\x94\x82   \xe2\x94\x9c\xe2\x94\x80 [Sotto-concetto]\n"
    "  \xe2\x94\x82   \xe2\x94\x94\xe2\x94\x80 [Sotto-concetto]\n"
    "  \xe2\x94\x94\xe2\x94\x80 [Ramo 2]\n"
    "Includi almeno 3 livelli di profondità. Rispondi in italiano.",

    /* 5 — Riassumi */
    "Riassumi il testo fornito in modo chiaro e strutturato. "
    "Identifica i concetti chiave, spiega le relazioni tra di essi "
    "e crea un riassunto gerarchico con punti elenco. "
    "Alla fine aggiungi 3 domande di verifica sull'argomento. "
    "Rispondi in italiano.",
};

/* ══════════════════════════════════════════════════════════════
   Helper — costruisce pulsante modalità uniforme
   ══════════════════════════════════════════════════════════════ */
static QPushButton* makeModeBtn(const char* icon, const char* label, QWidget* parent)
{
    auto* btn = new QPushButton(
        QString::fromUtf8(icon) + " " + QString::fromUtf8(label), parent);
    btn->setMinimumHeight(44);
    btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    return btn;
}

/* ══════════════════════════════════════════════════════════════
   Costruttore
   ══════════════════════════════════════════════════════════════ */
StudioPage::StudioPage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    /* ── Stack principale: 0=menu, 1=quiz fullscreen ── */
    m_innerStack = new QStackedWidget(this);
    auto* outerVbox = new QVBoxLayout(this);
    outerVbox->setContentsMargins(0, 0, 0, 0);
    outerVbox->addWidget(m_innerStack);

    /* ═══════════════════════════════════════════════
       PAGINA 0 — Menu principale (scroll)
       ═══════════════════════════════════════════════ */
    auto* scrollPage = new QScrollArea;
    scrollPage->setWidgetResizable(true);
    scrollPage->setFrameShape(QFrame::NoFrame);

    auto* inner = new QWidget;
    auto* vbox  = new QVBoxLayout(inner);
    vbox->setContentsMargins(8, 8, 8, 8);
    vbox->setSpacing(8);

    /* Titolo */
    auto* title = new QLabel(
        QString::fromUtf8("\xf0\x9f\x93\x9a Studio AI"), inner);  /* 📚 */
    QFont tf = title->font();
    tf.setPointSize(14);
    tf.setBold(true);
    title->setFont(tf);
    title->setAlignment(Qt::AlignCenter);
    vbox->addWidget(title);

    /* Selezione materia */
    auto* matGroup = new QGroupBox("Materia", inner);
    auto* matVbox  = new QVBoxLayout(matGroup);
    matVbox->setSpacing(6);

    m_matCombo = new QComboBox(inner);
    for (int i = 0; i < kNMaterie; ++i)
        m_matCombo->addItem(QString::fromUtf8(kMaterie[i].label));
    matVbox->addWidget(m_matCombo);

    m_materiaEdit = new QLineEdit(inner);
    m_materiaEdit->setPlaceholderText("Scrivi la materia o il corso...");
    m_materiaEdit->setVisible(false);
    matVbox->addWidget(m_materiaEdit);

    m_argoEdit = new QLineEdit(inner);
    m_argoEdit->setPlaceholderText(
        "Argomento specifico (opzionale) — es. \"OSPF\", \"derivate\", \"VLAN trunking\"");
    matVbox->addWidget(m_argoEdit);

    /* Riga tema CCNA (visibile solo per CCNA + Quiz) */
    m_temaRow = new QWidget(inner);
    auto* temaRowLay = new QHBoxLayout(m_temaRow);
    temaRowLay->setContentsMargins(0, 0, 0, 0);
    auto* temaLbl = new QLabel(
        QString::fromUtf8("\xf0\x9f\x93\x8c Tema:"), m_temaRow);  /* 📌 */
    m_temaCombo = new QComboBox(m_temaRow);
    m_temaCombo->addItem("Tutti i temi");
    for (const QString& t : m_ccnaDb.temi())
        m_temaCombo->addItem(t);
    temaRowLay->addWidget(temaLbl);
    temaRowLay->addWidget(m_temaCombo, 1);
    m_temaRow->setVisible(false);
    matVbox->addWidget(m_temaRow);

    vbox->addWidget(matGroup);

    /* Modalità di studio */
    auto* modeGroup = new QGroupBox("Modalità di studio", inner);
    auto* modeGrid  = new QGridLayout(modeGroup);
    modeGrid->setSpacing(6);

    struct Mode { const char* icon; const char* label; };
    static const Mode kModes[] = {
        { "\xf0\x9f\x93\x96", "Spiega"    },
        { "\xe2\x9d\x93",     "Quiz"      },
        { "\xf0\x9f\x94\xa7", "Esercizio" },
        { "\xf0\x9f\x92\xa1", "Flashcard" },
        { "\xf0\x9f\x97\xba\xef\xb8\x8f", "Mappa"  },
        { "\xf0\x9f\x93\x9d", "Riassumi"  },
    };

    for (int i = 0; i < 6; ++i) {
        auto* btn = makeModeBtn(kModes[i].icon, kModes[i].label, inner);
        btn->setProperty("modeIdx", i);
        connect(btn, &QPushButton::clicked, this, &StudioPage::onModeBtnClicked);
        modeGrid->addWidget(btn, i / 2, i % 2);
    }
    vbox->addWidget(modeGroup);

    /* Area testo per "Riassumi" */
    m_inputLbl = new QLabel(
        QString::fromUtf8("\xf0\x9f\x93\x84")
        + " Testo da riassumere:", inner);
    m_inputLbl->setVisible(false);
    vbox->addWidget(m_inputLbl);

    m_inputArea = new QTextEdit(inner);
    m_inputArea->setPlaceholderText("Incolla qui il testo da riassumere...");
    m_inputArea->setFixedHeight(90);
    m_inputArea->setVisible(false);
    vbox->addWidget(m_inputArea);

    /* Status + progress */
    auto* statRow = new QHBoxLayout;
    m_statusLbl = new QLabel("", inner);
    m_statusLbl->setVisible(false);
    statRow->addWidget(m_statusLbl, 1);
    m_btnStop = new QPushButton(
        QString::fromUtf8("\xe2\x8f\xb9 Stop"), inner);  /* ⏹ */
    m_btnStop->setVisible(false);
    m_btnStop->setFixedWidth(72);
    statRow->addWidget(m_btnStop);
    vbox->addLayout(statRow);

    m_progress = new QProgressBar(inner);
    m_progress->setRange(0, 0);
    m_progress->setVisible(false);
    m_progress->setFixedHeight(4);
    vbox->addWidget(m_progress);

    /* Output (Spiega/Flashcard/Esercizio/Mappa/Riassumi) */
    m_output = new QTextEdit(inner);
    m_output->setReadOnly(true);
    m_output->setPlaceholderText(
        "La spiegazione / esercizio apparirà qui.\n\n"
        "Seleziona materia e argomento, poi scegli una modalità.\n\n"
        "Per il Quiz CCNA puoi scegliere il tema: usa il selettore sopra.");
    m_output->setMinimumHeight(160);
    vbox->addWidget(m_output);

    /* Pulsante continua (non Quiz) */
    m_btnReveal = new QPushButton(inner);
    m_btnReveal->setVisible(false);

    m_btnNext = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x94\x84 Prossima / Continua"), inner);
    m_btnNext->setMinimumHeight(44);
    m_btnNext->setEnabled(false);
    vbox->addWidget(m_btnNext);

    vbox->addStretch();
    scrollPage->setWidget(inner);
    m_innerStack->addWidget(scrollPage);   /* indice 0 */

    /* ═══════════════════════════════════════════════
       PAGINA 1 — Quiz a tutto schermo
       ═══════════════════════════════════════════════ */
    m_quizPage = new QWidget;
    m_quizPage->setObjectName("QuizFullPage");
    auto* qpVbox = new QVBoxLayout(m_quizPage);
    qpVbox->setContentsMargins(12, 12, 12, 12);
    qpVbox->setSpacing(10);

    /* Riga in cima: ← Torna + info tema */
    auto* qpHeader = new QHBoxLayout;
    m_quizBackBtn = new QPushButton(
        QString::fromUtf8("\xe2\x86\x90 Menu"), m_quizPage);  /* ← */
    m_quizBackBtn->setObjectName("SecondaryBtn");
    m_quizBackBtn->setFixedHeight(40);
    m_quizBackBtn->setFixedWidth(90);
    m_quizNumLbl = new QLabel("", m_quizPage);
    {
        QFont nf = m_quizNumLbl->font();
        nf.setPointSize(12);
        m_quizNumLbl->setFont(nf);
    }
    m_quizNumLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    qpHeader->addWidget(m_quizBackBtn);
    qpHeader->addWidget(m_quizNumLbl, 1);
    qpVbox->addLayout(qpHeader);

    /* Separatore */
    auto* sep = new QFrame(m_quizPage);
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Sunken);
    qpVbox->addWidget(sep);

    /* Domanda */
    m_quizQLbl = new QLabel(m_quizPage);
    m_quizQLbl->setWordWrap(true);
    m_quizQLbl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    {
        QFont f = m_quizQLbl->font();
        f.setPointSize(14);
        f.setBold(true);
        m_quizQLbl->setFont(f);
    }
    qpVbox->addWidget(m_quizQLbl);
    qpVbox->addSpacing(6);

    /* 4 bottoni risposta */
    for (int i = 0; i < 4; ++i) {
        m_quizBtns[i] = new QPushButton(m_quizPage);
        m_quizBtns[i]->setMinimumHeight(58);
        m_quizBtns[i]->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_quizBtns[i]->setObjectName("SecondaryBtn");
        m_quizBtns[i]->setEnabled(false);
        qpVbox->addWidget(m_quizBtns[i]);
    }
    connect(m_quizBtns[0], &QPushButton::clicked, this, &StudioPage::onQuizBtn0);
    connect(m_quizBtns[1], &QPushButton::clicked, this, &StudioPage::onQuizBtn1);
    connect(m_quizBtns[2], &QPushButton::clicked, this, &StudioPage::onQuizBtn2);
    connect(m_quizBtns[3], &QPushButton::clicked, this, &StudioPage::onQuizBtn3);

    /* Feedback (risposta corretta + spiegazione) */
    m_quizFbLbl = new QLabel(m_quizPage);
    m_quizFbLbl->setWordWrap(true);
    m_quizFbLbl->setVisible(false);
    {
        QFont f = m_quizFbLbl->font();
        f.setPointSize(12);
        m_quizFbLbl->setFont(f);
    }
    qpVbox->addWidget(m_quizFbLbl, 1);

    /* Pulsante prossima (in pagina quiz) */
    m_quizNextBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x94\x84 Prossima domanda"), m_quizPage);
    m_quizNextBtn->setObjectName("PrimaryBtn");
    m_quizNextBtn->setMinimumHeight(50);
    m_quizNextBtn->setEnabled(false);
    qpVbox->addWidget(m_quizNextBtn);

    m_innerStack->addWidget(m_quizPage);   /* indice 1 */

    /* ── Connessioni ── */
    connect(m_matCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &StudioPage::onMateriaChanged);
    connect(m_temaCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &StudioPage::onTemaChanged);
    connect(m_btnStop,     &QPushButton::clicked, m_ai,  &AiClient::abort);
    connect(m_btnNext,     &QPushButton::clicked, this,  &StudioPage::onNextClicked);
    connect(m_btnReveal,   &QPushButton::clicked, this,  &StudioPage::onRevealClicked);
    connect(m_quizBackBtn, &QPushButton::clicked, this,  &StudioPage::onQuizBack);
    connect(m_quizNextBtn, &QPushButton::clicked, this,  &StudioPage::onNextClicked);
    connect(m_ai, &AiClient::token,    this, &StudioPage::onToken);
    connect(m_ai, &AiClient::finished, this, &StudioPage::onFinished);
    connect(m_ai, &AiClient::error,    this, &StudioPage::onError);
    connect(m_ai, &AiClient::aborted,  this, &StudioPage::onAborted);
}

/* ══════════════════════════════════════════════════════════════
   Slot
   ══════════════════════════════════════════════════════════════ */
bool StudioPage::isCcnaSelected() const
{
    return m_matCombo->currentIndex() == kIdxCcna;
}

void StudioPage::onMateriaChanged(int idx)
{
    const bool isAltro = (idx == kIdxAltro);
    m_materiaEdit->setVisible(isAltro);
    /* Riga tema: visibile solo per CCNA + ultima modalità = Quiz */
    const bool showTema = (idx == kIdxCcna && m_lastMode == 1);
    m_temaRow->setVisible(showTema);
    const bool showInput = (m_lastMode == 5);
    m_inputLbl->setVisible(showInput);
    m_inputArea->setVisible(showInput);
}

void StudioPage::onTemaChanged(int) { /* senza effetti immediati */ }

void StudioPage::onModeClicked(int modeIdx)
{
    const bool isRiassumi = (modeIdx == 5);
    m_inputLbl->setVisible(isRiassumi);
    m_inputArea->setVisible(isRiassumi);

    /* Mostra riga tema CCNA solo per Quiz CCNA */
    const bool showTema = (modeIdx == 1 && isCcnaSelected());
    m_temaRow->setVisible(showTema);

    const bool isQuiz = (modeIdx == 1);
    if (!isQuiz) {
        m_innerStack->setCurrentIndex(0);
        m_output->setVisible(true);
        m_output->clear();
    } else {
        /* Quiz CCNA: usa il database locale se disponibile */
        if (isCcnaSelected() && m_ccnaDb.count() > 0) {
            const int temaIdx = m_temaCombo->currentIndex();
            QuizQuestion q;
            if (temaIdx == 0) {
                q = m_ccnaDb.randomQuestion();
            } else {
                const QString tema = m_temaCombo->itemText(temaIdx);
                q = m_ccnaDb.randomByTema(tema);
            }
            m_lastMode  = 1;
            m_quizCount = 1;
            showQuizFullscreen(q);
            return;
        }
        /* Altre materie: genera via AI */
        m_output->setPlainText(
            QString::fromUtf8("\xe2\x8f\xb3")
            + " Generazione domanda in corso\xe2\x80\xa6");
    }

    runStudy(modeIdx);
}

void StudioPage::onNextClicked()
{
    if (m_lastMode == 1 && isCcnaSelected() && m_ccnaDb.count() > 0) {
        /* Prossima domanda dal DB locale */
        const int temaIdx = m_temaCombo->currentIndex();
        QuizQuestion q;
        if (temaIdx == 0) {
            q = m_ccnaDb.randomQuestion();
        } else {
            const QString tema = m_temaCombo->itemText(temaIdx);
            q = m_ccnaDb.randomByTema(tema);
        }
        ++m_quizCount;
        showQuizFullscreen(q);
        return;
    }
    if (m_lastMode >= 0)
        runStudy(m_lastMode);
}

void StudioPage::onQuizBack()
{
    m_innerStack->setCurrentIndex(0);
}

/* ══════════════════════════════════════════════════════════════
   showQuizFullscreen — popola la pagina 1 con la domanda e mostra
   ══════════════════════════════════════════════════════════════ */
void StudioPage::showQuizFullscreen(const QuizQuestion& q)
{
    m_quizCorrect = q.corretta;
    m_quizSpiega  = q.spiegazione;

    /* Header info */
    const QString temaInfo = q.tema.isEmpty()
        ? QString("Domanda %1").arg(m_quizCount)
        : QString("Domanda %1  \xe2\x80\x94  %2").arg(m_quizCount).arg(q.tema);
    m_quizNumLbl->setText(temaInfo);

    /* Domanda */
    m_quizQLbl->setText(q.domanda);

    /* Risposte */
    static const char* kLet[] = {"A", "B", "C", "D"};
    for (int i = 0; i < 4; ++i) {
        m_quizBtns[i]->setText(QString("%1)  %2").arg(kLet[i]).arg(q.risposte[i]));
        m_quizBtns[i]->setEnabled(true);
        m_quizBtns[i]->setStyleSheet(QString());
    }

    /* Reset feedback */
    m_quizFbLbl->setText(QString());
    m_quizFbLbl->setVisible(false);
    m_quizNextBtn->setEnabled(false);

    /* Vai alla pagina quiz */
    m_innerStack->setCurrentIndex(1);
}

/* ══════════════════════════════════════════════════════════════
   Logica principale (AI)
   ══════════════════════════════════════════════════════════════ */
void StudioPage::runStudy(int modeIdx)
{
    if (m_busy || m_ai->busy()) {
        m_output->setPlainText(
            QString::fromUtf8("\xe2\x9a\xa0\xef\xb8\x8f")
            + " L'AI sta elaborando. Attendi oppure premi Stop.");
        return;
    }

    if (m_matCombo->currentIndex() == kIdxAltro
        && m_materiaEdit->text().trimmed().isEmpty()) {
        m_output->setPlainText(
            QString::fromUtf8("\xe2\x9a\xa0\xef\xb8\x8f")
            + " Scrivi la materia nel campo «Altro».");
        return;
    }
    if (modeIdx == 5 && m_inputArea->toPlainText().trimmed().isEmpty()) {
        m_output->setPlainText(
            QString::fromUtf8("\xe2\x9a\xa0\xef\xb8\x8f")
            + " Incolla il testo da riassumere nella casella sopra.");
        return;
    }

    m_lastMode   = modeIdx;
    m_busy       = true;
    m_quizBuffer = {};
    m_btnReveal->setVisible(false);
    m_btnNext->setEnabled(false);

    if (modeIdx == 1) {
        m_output->setPlainText(
            QString::fromUtf8("\xe2\x8f\xb3") + " Generazione domanda\xe2\x80\xa6");
    } else {
        m_output->clear();
    }

    static const char* kModeNames[] = {
        "Spiegazione", "Quiz", "Esercizio", "Flashcard", "Mappa mentale", "Riassunto"
    };
    m_statusLbl->setText(
        QString::fromUtf8("\xe2\x8f\xb3 ")
        + kModeNames[modeIdx] + " in corso...");
    m_statusLbl->setVisible(true);
    m_progress->setVisible(true);
    m_btnStop->setVisible(true);

    const QString sys  = buildSystemPrompt(m_matCombo->currentIndex(), modeIdx);
    const QString user = (modeIdx == 5)
        ? m_inputArea->toPlainText().trimmed()
        : (m_argoEdit->text().trimmed().isEmpty()
           ? QString("Introduzione generale a: ") + materiaName()
           : m_argoEdit->text().trimmed());

    m_ai->chat(sys, user);
}

QString StudioPage::materiaName() const
{
    const int idx = m_matCombo->currentIndex();
    if (idx == kIdxAltro)
        return m_materiaEdit->text().trimmed();
    return QString::fromUtf8(kMaterie[idx].label);
}

QString StudioPage::buildSystemPrompt(int materiaIdx, int modeIdx) const
{
    const QString matContext =
        (materiaIdx == kIdxAltro)
        ? QString("Materia: ") + m_materiaEdit->text().trimmed()
        : QString("Sei un docente esperto di: ") + QString::fromUtf8(kMaterie[materiaIdx].context);

    const QString modeInstr = QString::fromUtf8(kModeInstructions[modeIdx]);
    const QString diff = (modeIdx >= 1 && modeIdx <= 2)
        ? " Difficoltà: media (adatta a un esame certificativo)."
        : "";

    return matContext + ". " + modeInstr + diff;
}

/* ══════════════════════════════════════════════════════════════
   Slot AI
   ══════════════════════════════════════════════════════════════ */
void StudioPage::onToken(const QString& t)
{
    if (!m_busy) return;
    if (m_lastMode == 1) {
        m_quizBuffer += t;
        return;
    }
    m_output->moveCursor(QTextCursor::End);
    m_output->insertPlainText(t);
    m_output->verticalScrollBar()->setValue(m_output->verticalScrollBar()->maximum());
}

void StudioPage::onFinished(const QString&)
{
    if (!m_busy) return;
    m_busy = false;
    m_statusLbl->setVisible(false);
    m_progress->setVisible(false);
    m_btnStop->setVisible(false);

    if (m_lastMode == 1) {
        const QString raw = m_quizBuffer.trimmed();
        m_quizBuffer.clear();
        populateQuiz(raw);
    } else {
        m_btnReveal->setVisible(false);
        m_btnNext->setEnabled(m_lastMode >= 0 && m_lastMode != 4 && m_lastMode != 5);
    }
}

void StudioPage::onRevealClicked() { /* non usato */ }

void StudioPage::onError(const QString& e)
{
    if (!m_busy) return;
    m_busy = false;
    m_statusLbl->setVisible(false);
    m_progress->setVisible(false);
    m_btnStop->setVisible(false);
    m_innerStack->setCurrentIndex(0);
    m_output->setVisible(true);
    m_output->setPlainText(
        QString::fromUtf8("\xe2\x9d\x8c") + " Errore: " + e);
}

/* ══════════════════════════════════════════════════════════════
   populateQuiz — per quiz AI (materie diverse da CCNA)
   ══════════════════════════════════════════════════════════════ */
void StudioPage::populateQuiz(const QString& raw)
{
    QString json = raw;
    const int s = json.indexOf('{');
    const int e = json.lastIndexOf('}');
    if (s >= 0 && e > s) json = json.mid(s, e - s + 1);

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &err);
    QJsonObject obj = doc.object();

    auto extractField = [&](const QString& key) -> QString {
        if (!obj.value(key).isUndefined())
            return obj.value(key).toString();
        const QRegularExpression re(
            "\"" + key + "\"\\s*:\\s*\"([^\"]+)\"",
            QRegularExpression::CaseInsensitiveOption);
        const auto m = re.match(raw);
        return m.hasMatch() ? m.captured(1) : QString();
    };
    auto extractInt = [&](const QString& key) -> int {
        if (!obj.value(key).isUndefined())
            return obj.value(key).toInt();
        const QRegularExpression re(
            "\"" + key + "\"\\s*:\\s*([1-4])",
            QRegularExpression::CaseInsensitiveOption);
        const auto m = re.match(raw);
        return m.hasMatch() ? m.captured(1).toInt() : 0;
    };

    const QString domanda = extractField("domanda");
    const QString risposte[4] = {
        extractField("risposta1"), extractField("risposta2"),
        extractField("risposta3"), extractField("risposta4")
    };
    const int corretta = extractInt("corretta") - 1;
    m_quizSpiega = extractField("spiegazione");

    if (domanda.isEmpty()) {
        m_output->setVisible(true);
        m_output->setPlainText(raw);
        m_btnNext->setEnabled(true);
        m_innerStack->setCurrentIndex(0);
        return;
    }

    QuizQuestion q;
    q.domanda    = domanda;
    q.spiegazione = m_quizSpiega;
    q.corretta   = (corretta >= 0 && corretta < 4) ? corretta : 0;
    for (int i = 0; i < 4; ++i)
        q.risposte[i] = risposte[i];

    ++m_quizCount;
    showQuizFullscreen(q);
}

/* ══════════════════════════════════════════════════════════════
   Slot ausiliari senza lambda
   ══════════════════════════════════════════════════════════════ */
void StudioPage::onModeBtnClicked()
{
    auto* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    onModeClicked(btn->property("modeIdx").toInt());
}

void StudioPage::onQuizBtn0() { onQuizAnswerClicked(0); }
void StudioPage::onQuizBtn1() { onQuizAnswerClicked(1); }
void StudioPage::onQuizBtn2() { onQuizAnswerClicked(2); }
void StudioPage::onQuizBtn3() { onQuizAnswerClicked(3); }
void StudioPage::onAborted()  { onFinished(""); }

/* ══════════════════════════════════════════════════════════════
   onQuizAnswerClicked — feedback con colori e spiegazione
   ══════════════════════════════════════════════════════════════ */
void StudioPage::onQuizAnswerClicked(int idx)
{
    for (int i = 0; i < 4; ++i)
        m_quizBtns[i]->setEnabled(false);

    const bool correct = (idx == m_quizCorrect);

    m_quizBtns[m_quizCorrect]->setStyleSheet(
        "background-color:#4CAF50;color:#ffffff;border:none;"
        "border-radius:10px;padding:10px;font-weight:700;");
    if (!correct)
        m_quizBtns[idx]->setStyleSheet(
            "background-color:#f44336;color:#ffffff;border:none;"
            "border-radius:10px;padding:10px;font-weight:700;");

    static const char* kLet[] = {"A", "B", "C", "D"};
    QString fb;
    if (correct) {
        fb = QString::fromUtf8("\xe2\x9c\x85  Corretta!\n\n");  /* ✅ */
    } else {
        fb = QString::fromUtf8("\xe2\x9d\x8c  Sbagliata \xe2\x80\x94 ")  /* ❌ */
           + QString("La risposta corretta è: <b>%1)</b>\n\n").arg(kLet[m_quizCorrect]);
    }
    if (!m_quizSpiega.isEmpty()) {
        fb += QString::fromUtf8("\xf0\x9f\x93\x96  ")  /* 📖 */
           + m_quizSpiega;
    }
    m_quizFbLbl->setTextFormat(Qt::RichText);
    m_quizFbLbl->setText(fb);
    m_quizFbLbl->setVisible(true);
    m_quizNextBtn->setEnabled(true);
}
