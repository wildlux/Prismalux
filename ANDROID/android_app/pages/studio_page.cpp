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

/* ══════════════════════════════════════════════════════════════
   Materie disponibili
   ══════════════════════════════════════════════════════════════ */
struct Materia {
    const char* label;
    const char* context; ///< breve descrizione per il system prompt
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

/* ══════════════════════════════════════════════════════════════
   System prompt per modalità
   ══════════════════════════════════════════════════════════════ */
static const char* kModeInstructions[] = {
    /* 0 — Spiega */
    "Spiega l'argomento richiesto in modo chiaro e progressivo. "
    "Usa esempi concreti, analogie semplici e, dove pertinente, "
    "esempi di codice/comandi reali. Struttura la risposta con titoli brevi. "
    "Rispondi in italiano.",

    /* 1 — Quiz */
    "Genera UNA domanda a risposta multipla (A/B/C/D) sull'argomento. "
    "Formato richiesto:\n"
    "**Domanda:** [testo]\n"
    "A) ...\nB) ...\nC) ...\nD) ...\n\n"
    "Poi mostra: **Risposta corretta:** X) [testo]\n"
    "**Spiegazione:** [perché è corretta e perché le altre sono sbagliate]\n"
    "Rispondi in italiano.",

    /* 2 — Esercizio */
    "Proponi UN esercizio pratico sull'argomento, appropriato per un esame. "
    "Formato:\n"
    "**Esercizio:** [testo dettagliato del problema]\n\n"
    "**Soluzione passo-passo:**\n[step 1...]\n[step 2...]\n\n"
    "**Risposta finale:** [risultato]\n"
    "Rispondi in italiano.",

    /* 3 — Flashcard */
    "Genera 5 flashcard sull'argomento. Ogni flashcard ha:\n"
    "🃏 **Fronte:** [domanda breve o termine]\n"
    "✅ **Retro:** [risposta o definizione concisa]\n\n"
    "Le flashcard devono coprire i concetti più importanti. "
    "Rispondi in italiano.",

    /* 4 — Mappa mentale */
    "Crea una mappa mentale testuale dell'argomento. "
    "Usa indentazione e simboli per rappresentare la gerarchia:\n"
    "🎯 [Argomento centrale]\n"
    "  ├─ [Ramo 1]\n"
    "  │   ├─ [Sotto-concetto]\n"
    "  │   └─ [Sotto-concetto]\n"
    "  └─ [Ramo 2]\n"
    "Includi almeno 3 livelli di profondità. Rispondi in italiano.",

    /* 5 — Riassumi */
    "Riassumi il testo fornito in modo chiaro e strutturato. "
    "Identifica i concetti chiave, spiega le relazioni tra di essi "
    "e crea un riassunto gerarchico con punti elenco. "
    "Alla fine aggiungi 3 domande di verifica sull'argomento. "
    "Rispondi in italiano.",
};

/* ══════════════════════════════════════════════════════════════
   Costruttore
   ══════════════════════════════════════════════════════════════ */
StudioPage::StudioPage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* inner = new QWidget;
    auto* vbox  = new QVBoxLayout(inner);
    vbox->setContentsMargins(8, 8, 8, 8);
    vbox->setSpacing(8);

    /* ── Titolo ── */
    auto* title = new QLabel(
        QString::fromUtf8("\xf0\x9f\x93\x9a Studio AI"), inner);
    QFont tf = title->font();
    tf.setPointSize(14);
    tf.setBold(true);
    title->setFont(tf);
    title->setAlignment(Qt::AlignCenter);
    vbox->addWidget(title);

    /* ── Selezione materia ── */
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
    vbox->addWidget(matGroup);

    /* ── Modalità ── */
    auto* modeGroup = new QGroupBox("Modalità di studio", inner);
    auto* modeGrid  = new QGridLayout(modeGroup);
    modeGrid->setSpacing(6);

    struct Mode { const char* icon; const char* label; };
    static const Mode kModes[] = {
        { "\xf0\x9f\x93\x96", "Spiega"    },
        { "\xe2\x9d\x93",     "Quiz"      },
        { "\xf0\x9f\x94\xa7", "Esercizio" },
        { "\xf0\x9f\x92\xa1", "Flashcard" },
        { "\xf0\x9f\x97\xba\xef\xb8\x8f", "Mappa"    },
        { "\xf0\x9f\x93\x9d", "Riassumi"  },
    };

    for (int i = 0; i < 6; ++i) {
        auto* btn = new QPushButton(
            QString::fromUtf8(kModes[i].icon) + " " + kModes[i].label, inner);
        btn->setMinimumHeight(44);
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        const int mode = i;
        connect(btn, &QPushButton::clicked, this, [this, mode]{ onModeClicked(mode); });
        modeGrid->addWidget(btn, i / 2, i % 2);
    }
    vbox->addWidget(modeGroup);

    /* ── Area testo per "Riassumi" ── */
    m_inputLbl = new QLabel(
        QString::fromUtf8("\xf0\x9f\x93\x84")
        + " Testo da riassumere (per modalità Riassumi):", inner);
    m_inputLbl->setVisible(false);
    vbox->addWidget(m_inputLbl);

    m_inputArea = new QTextEdit(inner);
    m_inputArea->setPlaceholderText("Incolla qui il testo da riassumere...");
    m_inputArea->setFixedHeight(90);
    m_inputArea->setVisible(false);
    vbox->addWidget(m_inputArea);

    /* ── Status + progress ── */
    auto* statRow = new QHBoxLayout;
    m_statusLbl = new QLabel("", inner);
    m_statusLbl->setVisible(false);
    statRow->addWidget(m_statusLbl, 1);
    m_btnStop = new QPushButton(
        QString::fromUtf8("\xe2\x8f\xb9 Stop"), inner);
    m_btnStop->setVisible(false);
    m_btnStop->setFixedWidth(72);
    statRow->addWidget(m_btnStop);
    vbox->addLayout(statRow);

    m_progress = new QProgressBar(inner);
    m_progress->setRange(0, 0);
    m_progress->setVisible(false);
    m_progress->setFixedHeight(4);
    vbox->addWidget(m_progress);

    /* ── Output ── */
    m_output = new QTextEdit(inner);
    m_output->setReadOnly(true);
    m_output->setPlaceholderText(
        "La spiegazione / quiz / esercizio apparirà qui.\n\n"
        "Seleziona materia e argomento, poi scegli una modalità.");
    m_output->setMinimumHeight(200);
    vbox->addWidget(m_output);

    /* ── Pulsante "Prossimo" ── */
    m_btnNext = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x94\x84 Prossima domanda / Continua"), inner);
    m_btnNext->setMinimumHeight(44);
    m_btnNext->setEnabled(false);
    vbox->addWidget(m_btnNext);

    vbox->addStretch();
    scroll->setWidget(inner);

    auto* outerVbox = new QVBoxLayout(this);
    outerVbox->setContentsMargins(0, 0, 0, 0);
    outerVbox->addWidget(scroll);

    /* ── Connessioni ── */
    connect(m_matCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &StudioPage::onMateriaChanged);
    connect(m_btnStop, &QPushButton::clicked, m_ai, &AiClient::abort);
    connect(m_btnNext, &QPushButton::clicked, this, &StudioPage::onNextClicked);
    connect(m_ai, &AiClient::token,    this, &StudioPage::onToken);
    connect(m_ai, &AiClient::finished, this, &StudioPage::onFinished);
    connect(m_ai, &AiClient::error,    this, &StudioPage::onError);
    connect(m_ai, &AiClient::aborted,  this, [this]{ onFinished(""); });
}

/* ══════════════════════════════════════════════════════════════
   Slot
   ══════════════════════════════════════════════════════════════ */
void StudioPage::onMateriaChanged(int idx)
{
    const bool isAltro = (idx == kIdxAltro);
    m_materiaEdit->setVisible(isAltro);
    /* Mostra campo input testo solo per Riassumi (ultima modalità usata = 5) */
    const bool showInput = (m_lastMode == 5);
    m_inputLbl->setVisible(showInput);
    m_inputArea->setVisible(showInput);
}

void StudioPage::onModeClicked(int modeIdx)
{
    /* Mostra/nascondi l'area testo per la modalità Riassumi */
    const bool isRiassumi = (modeIdx == 5);
    m_inputLbl->setVisible(isRiassumi);
    m_inputArea->setVisible(isRiassumi);
    runStudy(modeIdx);
}

void StudioPage::onNextClicked()
{
    if (m_lastMode >= 0)
        runStudy(m_lastMode);
}

/* ══════════════════════════════════════════════════════════════
   Logica principale
   ══════════════════════════════════════════════════════════════ */
void StudioPage::runStudy(int modeIdx)
{
    if (m_busy || m_ai->busy()) {
        m_output->setPlainText(
            QString::fromUtf8("\xe2\x9a\xa0\xef\xb8\x8f")
            + " L'AI sta elaborando. Attendi oppure premi Stop.");
        return;
    }

    /* Validazione */
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

    m_lastMode = modeIdx;
    m_busy     = true;
    m_output->clear();
    m_btnNext->setEnabled(false);

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

    /* Per Quiz/Esercizio/Flashcard, aggiungi livello difficoltà medio */
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
    /* Abilita "Prossimo" solo per modalità che ha senso ripetere */
    m_btnNext->setEnabled(m_lastMode >= 0 && m_lastMode != 4 && m_lastMode != 5);
}

void StudioPage::onError(const QString& e)
{
    if (!m_busy) return;
    m_busy = false;
    m_statusLbl->setVisible(false);
    m_progress->setVisible(false);
    m_btnStop->setVisible(false);
    m_output->setPlainText(
        QString::fromUtf8("\xe2\x9d\x8c") + " Errore: " + e);
}
