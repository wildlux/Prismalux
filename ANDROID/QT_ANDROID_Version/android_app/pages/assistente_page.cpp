#include "assistente_page.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QFont>
#include <QApplication>
#include <QClipboard>
#include <QTextCursor>
#include <QScroller>
#include <QScrollerProperties>

static void applyTouchScroll(QScrollArea* sa)
{
    QScroller::grabGesture(sa->viewport(), QScroller::TouchGesture);
    QScroller* qs = QScroller::scroller(sa->viewport());
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

/* ══════════════════════════════════════════════════════════════
   Tabella system prompt [categoria][azione]
   Copiata da main_tools.cpp kSysPrompts — identica al desktop.
   ══════════════════════════════════════════════════════════════ */
static const char* kSysPrompts[6][9] = {
    /* 0 — Studio */
    {
        "Sei un tutor esperto. Spiega in modo chiaro e strutturato con esempi pratici. Rispondi in italiano.",
        "Crea 10 flashcard nel formato Q: ... R: ... dal testo fornito. Rispondi in italiano.",
        "Crea un riassunto breve del testo. Rispondi in italiano.",
        "Genera 10 domande d'esame probabili con risposta sintetica. Rispondi in italiano.",
        "Crea una mappa concettuale in formato albero ASCII del concetto o testo fornito. Rispondi in italiano.",
        "Crea 5 esercizi pratici progressivi con soluzione. Rispondi in italiano.",
        nullptr, nullptr, nullptr
    },
    /* 1 — Scrittura Creativa */
    {
        "Sei uno scrittore creativo. Scrivi una storia coinvolgente basata sull'idea fornita. Rispondi in italiano.",
        "Continua la storia in modo coerente con lo stile e i personaggi esistenti. Rispondi in italiano.",
        "Crea un personaggio dettagliato: nome, aspetto, backstory, motivazioni, difetti e punti di forza. Rispondi in italiano.",
        "Scrivi una poesia originale sul tema indicato. Rispondi in italiano.",
        "Scrivi un dialogo naturale e coinvolgente tra personaggi in base alla scena descritta. Rispondi in italiano.",
        "Sviluppa una trama in 3 atti: setup, confronto, risoluzione. Rispondi in italiano.",
        nullptr, nullptr, nullptr
    },
    /* 2 — Ricerca */
    {
        "Fai una ricerca approfondita sull'argomento. Struttura: panoramica, punti chiave, dettagli, fonti consigliate. Rispondi in italiano.",
        "Confronta le opzioni indicate con una tabella pro/contro per ciascuna. Rispondi in italiano.",
        "Verifica la veridicita' dell'affermazione. Indica: VERO/FALSO/PARZIALMENTE VERO con spiegazione. Rispondi in italiano.",
        "Genera una bibliografia in formato APA sull'argomento con 5-10 fonti plausibili. Rispondi in italiano.",
        "Analizza il problema da almeno 4 prospettive diverse (economica, sociale, tecnica, etica). Rispondi in italiano.",
        "Crea una guida passo-passo dettagliata. Rispondi in italiano.",
        "Sei un esperto di proprieta' intellettuale e brevetti. Analizza la descrizione tecnica e verifica: 1) Brevetti simili 2) Brevettabilita' 3) Dove cercare (Google Patents, EPO, UIBM) 4) Rivendicazioni principali. Rispondi in italiano.",
        "Sei un esperto di ricerca accademica. Analizza il paper o abstract: 1) Metodologia e bias 2) Peer review e impact factor 3) Riproducibilita' 4) Citazioni chiave. Rispondi in italiano.",
        nullptr
    },
    /* 3 — Libri */
    {
        "Analizza il testo: temi principali, stile, struttura narrativa, simbolismi. Rispondi in italiano.",
        "Crea un riassunto strutturato per capitoli o sezioni del testo. Rispondi in italiano.",
        "Analizza i personaggi principali: caratterizzazione, archi narrativi, relazioni. Rispondi in italiano.",
        "Crea una scheda libro completa: titolo, autore, anno, genere, trama, temi, punti di forza/debolezza, voto /10. Rispondi in italiano.",
        "Genera 8 domande aperte per una discussione critica del libro. Rispondi in italiano.",
        "Suggerisci 5 libri simili per temi e stile, con breve motivazione. Rispondi in italiano.",
        nullptr, nullptr, nullptr
    },
    /* 4 — Produttivita' */
    {
        "Crea un piano progetto dettagliato: obiettivi, WBS, milestone, rischi, risorse. Rispondi in italiano.",
        "Organizza i task in Must/Should/Could/Won't (MoSCoW) con priorita' e tempi stimati. Rispondi in italiano.",
        "Scrivi un'email professionale chiara e convincente basata sul briefing fornito. Rispondi in italiano.",
        "Crea un'agenda dettagliata con punti di discussione, tempi e obiettivi per la riunione. Rispondi in italiano.",
        "Fai un brainstorming usando la tecnica dei 6 cappelli di de Bono sull'argomento. Rispondi in italiano.",
        "Trasforma l'obiettivo vago in 3-5 obiettivi SMART. Rispondi in italiano.",
        "Crea una matrice decisionale con criteri ponderati per le opzioni indicate. Rispondi in italiano.",
        "Sei un assistente per la comunicazione medica. Riscrivi il testo in modo professionale e formale, mantenendo le informazioni cliniche. Rispondi in italiano.",
        "Analizza il testo: identifica emozioni predominanti, livello stress, distorsioni cognitive, e dai una raccomandazione (agire/attendere/riflettere). Rispondi in italiano."
    },
    /* 5 — Documenti */
    {
        "Analizza il documento fornito: struttura, temi principali, argomenti chiave e conclusioni. Rispondi in italiano.",
        "Crea un riassunto conciso e chiaro del documento. Rispondi in italiano.",
        "Estrai le informazioni chiave: fatti, dati, date, nomi e numeri importanti. Rispondi in italiano.",
        "Rispondi alle domande poste usando SOLO le informazioni contenute nel documento. Rispondi in italiano.",
        "Identifica punti deboli, contraddizioni, lacune argomentative o affermazioni non supportate. Rispondi in italiano.",
        "Ricava una lista di punti di azione, raccomandazioni o prossimi passi dal documento. Rispondi in italiano.",
        nullptr, nullptr, nullptr
    }
};

/* Nomi azioni per categoria */
static const char* kActionLabels[6][9] = {
    /* Studio */
    { "\xf0\x9f\x93\x9a Spiega",       "\xf0\x9f\x83\x8f Flashcard",
      "\xe2\x9c\x82 Riassumi",          "\xe2\x9d\x93 Domande esame",
      "\xf0\x9f\x97\xba Mappa ASCII",   "\xe2\x9c\x8f Esercizi",
      nullptr, nullptr, nullptr },
    /* Scrittura */
    { "\xf0\x9f\x93\x96 Scrivi storia", "\xe2\x9e\xa1 Continua",
      "\xf0\x9f\xa7\x91 Personaggio",   "\xf0\x9f\x8c\xb8 Poesia",
      "\xf0\x9f\x92\xac Dialogo",       "\xf0\x9f\x8e\xad Trama 3 atti",
      nullptr, nullptr, nullptr },
    /* Ricerca */
    { "\xf0\x9f\x94\x8d Approfondisci", "\xe2\x9a\x96 Confronta",
      "\xe2\x9c\x85 Fact-check",        "\xf0\x9f\x93\x9a Bibliografia",
      "\xf0\x9f\x94\xad Analisi 4 angoli", "\xf0\x9f\x97\xba Guida step-by-step",
      "\xf0\x9f\x93\x9c Verifica brevetto", "\xf0\x9f\x94\xac Analizza paper",
      nullptr },
    /* Libri */
    { "\xf0\x9f\x93\x8a Analisi testo",  "\xf0\x9f\x93\x9d Riassunto capitoli",
      "\xf0\x9f\xa7\x91 Personaggi",     "\xf0\x9f\x93\x8b Scheda libro",
      "\xe2\x9d\x93 Domande discussione", "\xf0\x9f\x93\x9a Libri simili",
      nullptr, nullptr, nullptr },
    /* Produttivita' */
    { "\xf0\x9f\x93\x8b Piano progetto", "\xf0\x9f\x93\x8c MoSCoW",
      "\xe2\x9c\x89 Email prof.",         "\xf0\x9f\x93\x85 Agenda riunione",
      "\xf0\x9f\x8e\xa9 6 cappelli",      "\xf0\x9f\x8e\xaf SMART",
      "\xf0\x9f\x93\x8a Matrice decisione", "\xf0\x9f\x8f\xa5 Testo medico",
      "\xf0\x9f\xa7\xa0 Analisi emotiva" },
    /* Documenti */
    { "\xf0\x9f\x94\x8e Analizza doc",   "\xe2\x9c\x82 Riassumi doc",
      "\xf0\x9f\x93\x8c Estrai dati",     "\xe2\x9d\x93 Rispondi dal doc",
      "\xf0\x9f\x94\xad Critica",         "\xe2\x9c\x85 Azioni",
      nullptr, nullptr, nullptr }
};

static int actionCount(int cat) {
    int n = 0;
    while (n < 9 && kSysPrompts[cat][n] != nullptr) ++n;
    return n;
}

/* ══════════════════════════════════════════════════════════════
   Costruttore
   ══════════════════════════════════════════════════════════════ */
AssistentePage::AssistentePage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    setObjectName("AssistentePage");

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    applyTouchScroll(scroll);

    auto* inner = new QWidget;
    auto* vbox  = new QVBoxLayout(inner);
    vbox->setContentsMargins(8, 8, 8, 8);
    vbox->setSpacing(10);
    scroll->setWidget(inner);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(scroll);

    /* ── Titolo ── */
    auto* title = new QLabel(
        QString::fromUtf8("\xf0\x9f\x9b\xa0  Assistente AI"), inner);
    {
        QFont f = title->font();
        f.setPointSize(15); f.setBold(true);
        title->setFont(f);
        title->setAlignment(Qt::AlignCenter);
    }
    vbox->addWidget(title);

    /* ── Selezione categoria ── */
    auto* catGroup = new QGroupBox(
        QString::fromUtf8("\xf0\x9f\x93\x82  Categoria"), inner);
    catGroup->setObjectName("SettingsGroup");
    auto* catVbox = new QVBoxLayout(catGroup);
    m_catCombo = new QComboBox(inner);
    m_catCombo->setObjectName("SettingsInput");
    m_catCombo->setMinimumHeight(44);
    const char* catLabels[] = {
        "\xf0\x9f\x93\x9a  Studio",
        "\xe2\x9c\x8d\xef\xb8\x8f  Scrittura Creativa",
        "\xf0\x9f\x94\x8d  Ricerca",
        "\xf0\x9f\x93\x96  Libri",
        "\xe2\x9a\xa1  Produttivita'",
        "\xf0\x9f\x93\x84  Documenti"
    };
    for (const char* lbl : catLabels)
        m_catCombo->addItem(QString::fromUtf8(lbl));
    catVbox->addWidget(m_catCombo);
    vbox->addWidget(catGroup);

    /* ── Griglia azioni (una pagina per categoria) ── */
    auto* actGroup = new QGroupBox(
        QString::fromUtf8("\xf0\x9f\x92\xa1  Azione"), inner);
    actGroup->setObjectName("SettingsGroup");
    auto* actVbox = new QVBoxLayout(actGroup);
    m_gridStack = new QStackedWidget(inner);
    for (int cat = 0; cat < 6; ++cat) {
        auto* page = new QWidget;
        auto* grid = new QGridLayout(page);
        grid->setSpacing(6);
        grid->setContentsMargins(0, 0, 0, 0);
        const int cnt = actionCount(cat);
        for (int i = 0; i < cnt; ++i) {
            auto* btn = new QPushButton(
                QString::fromUtf8(kActionLabels[cat][i]), page);
            btn->setObjectName("SettingsGroup");
            btn->setMinimumHeight(44);
            btn->setProperty("catIdx",    cat);
            btn->setProperty("actionIdx", i);
            btn->setCheckable(true);
            btn->setAutoExclusive(false);
            connect(btn, &QPushButton::clicked, this, &AssistentePage::onActionClicked);
            grid->addWidget(btn, i / 2, i % 2);
        }
        m_gridStack->addWidget(page);
    }
    actVbox->addWidget(m_gridStack);
    vbox->addWidget(actGroup);

    /* ── Input ── */
    auto* inGroup = new QGroupBox(
        QString::fromUtf8("\xf0\x9f\x93\x9d  Input"), inner);
    inGroup->setObjectName("SettingsGroup");
    auto* inVbox = new QVBoxLayout(inGroup);
    m_input = new QTextEdit(inner);
    m_input->setObjectName("SettingsInput");
    m_input->setMinimumHeight(88);
    m_input->setMaximumHeight(180);
    m_input->setPlaceholderText(
        "Inserisci il testo su cui lavorare, poi scegli un'azione sopra.");
    inVbox->addWidget(m_input);
    vbox->addWidget(inGroup);

    /* ── Pulsante Avvia / Stop ── */
    auto* btnRow = new QHBoxLayout;
    m_runBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x9a\x80  Avvia"), inner);
    m_runBtn->setObjectName("PrimaryBtn");
    m_runBtn->setMinimumHeight(48);
    auto* copyBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x93\x8b  Copia"), inner);
    copyBtn->setObjectName("actionBtn");
    copyBtn->setMinimumHeight(48);
    auto* clearBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x97\x91  Pulisci"), inner);
    clearBtn->setObjectName("actionBtn");
    clearBtn->setMinimumHeight(48);
    btnRow->addWidget(m_runBtn, 2);
    btnRow->addWidget(copyBtn, 1);
    btnRow->addWidget(clearBtn, 1);
    vbox->addLayout(btnRow);

    /* ── Stato ── */
    m_statusLbl = new QLabel("", inner);
    m_statusLbl->setObjectName("StatusLabel");
    m_statusLbl->setAlignment(Qt::AlignCenter);
    vbox->addWidget(m_statusLbl);

    /* ── Output ── */
    auto* outGroup = new QGroupBox(
        QString::fromUtf8("\xf0\x9f\xa4\x96  Risposta"), inner);
    outGroup->setObjectName("SettingsGroup");
    auto* outVbox = new QVBoxLayout(outGroup);
    m_output = new QTextEdit(inner);
    m_output->setObjectName("SettingsInput");
    m_output->setReadOnly(true);
    m_output->setMinimumHeight(220);
    m_output->setPlaceholderText("Il risultato apparirà qui...");
    outVbox->addWidget(m_output);
    vbox->addWidget(outGroup);

    /* ── Connessioni ── */
    connect(m_catCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &AssistentePage::onCatChanged);

    connect(m_runBtn, &QPushButton::clicked, this, &AssistentePage::onRunClicked);

    connect(copyBtn, &QPushButton::clicked, this, [this] {
        if (!m_output->toPlainText().isEmpty())
            QApplication::clipboard()->setText(m_output->toPlainText());
    });

    connect(clearBtn, &QPushButton::clicked, this, [this] {
        m_input->clear();
        m_output->clear();
        m_statusLbl->clear();
    });

    connect(m_ai, &AiClient::token,    this, &AssistentePage::onToken);
    connect(m_ai, &AiClient::finished,      this, &AssistentePage::onFinished);
    connect(m_ai, &AiClient::error,         this, &AssistentePage::onAiError);
    connect(m_ai, &AiClient::aborted,       this, &AssistentePage::onAborted);

    /* Prima categoria già selezionata: evidenzia primo bottone */
    m_gridStack->setCurrentIndex(0);
}

/* ══════════════════════════════════════════════════════════════
   onCatChanged — cambia griglia azioni
   ══════════════════════════════════════════════════════════════ */
void AssistentePage::onCatChanged(int idx)
{
    m_selCat    = idx;
    m_selAction = 0;
    m_gridStack->setCurrentIndex(idx);
    m_statusLbl->clear();
}

/* ══════════════════════════════════════════════════════════════
   onActionClicked — evidenzia l'azione selezionata
   ══════════════════════════════════════════════════════════════ */
void AssistentePage::onActionClicked()
{
    auto* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    m_selCat    = btn->property("catIdx").toInt();
    m_selAction = btn->property("actionIdx").toInt();

    /* Deseleziona tutti gli altri bottoni nella stessa pagina */
    auto* page = m_gridStack->currentWidget();
    for (auto* child : page->findChildren<QPushButton*>()) {
        const bool isThis = (child == btn);
        child->setChecked(isThis);
        child->setStyleSheet(isThis
            ? "background-color: #00c8ff; color: #0f1117; border-radius:6px;"
            : "");
    }

    m_statusLbl->setText(
        QString::fromUtf8("\xf0\x9f\x92\xa1 ") +
        QString::fromUtf8(kActionLabels[m_selCat][m_selAction]) +
        " selezionata");
}

/* ══════════════════════════════════════════════════════════════
   onRunClicked — avvia o ferma la generazione
   ══════════════════════════════════════════════════════════════ */
void AssistentePage::onRunClicked()
{
    if (m_runBtn->property("busy").toBool()) {
        m_ai->abort();
        return;
    }

    const QString input = m_input->toPlainText().trimmed();
    if (input.isEmpty()) {
        m_statusLbl->setText(
            QString::fromUtf8("\xe2\x9a\xa0 Inserisci del testo nell'input."));
        return;
    }

    const char* sys = sysPrompt(m_selCat, m_selAction);
    if (!sys) {
        m_statusLbl->setText(
            QString::fromUtf8("\xe2\x9a\xa0 Seleziona un'azione dalla griglia."));
        return;
    }

    m_output->clear();
    setBusy(true);
    m_ai->chat(QString::fromUtf8(sys), input);
}

/* ══════════════════════════════════════════════════════════════
   Slot AI
   ══════════════════════════════════════════════════════════════ */
void AssistentePage::onToken(const QString& t)
{
    m_output->moveCursor(QTextCursor::End);
    m_output->insertPlainText(t);
    m_output->ensureCursorVisible();
}

void AssistentePage::onFinished(const QString&)
{
    setBusy(false);
    m_statusLbl->setText(
        QString::fromUtf8("\xe2\x9c\x85 Completato."));
}

void AssistentePage::onAiError(const QString& msg)
{
    setBusy(false);
    m_statusLbl->setText(
        QString::fromUtf8("\xe2\x9d\x8c ") + msg);
}

void AssistentePage::onAborted()
{
    setBusy(false);
    m_statusLbl->setText(
        QString::fromUtf8("\xe2\x8f\xb9 Interrotto."));
}

void AssistentePage::setBusy(bool busy)
{
    m_runBtn->setProperty("busy", busy);
    m_runBtn->setText(busy
        ? QString::fromUtf8("\xe2\x8f\xb9  Stop")
        : QString::fromUtf8("\xf0\x9f\x9a\x80  Avvia"));
    m_runBtn->setObjectName(busy ? "actionBtn" : "PrimaryBtn");
    m_runBtn->style()->unpolish(m_runBtn);
    m_runBtn->style()->polish(m_runBtn);
    m_catCombo->setEnabled(!busy);
    m_input->setReadOnly(busy);
}

/* ══════════════════════════════════════════════════════════════
   sysPrompt — accesso alla tabella
   ══════════════════════════════════════════════════════════════ */
const char* AssistentePage::sysPrompt(int cat, int action)
{
    if (cat < 0 || cat > 5) return nullptr;
    if (action < 0 || action > 8) return nullptr;
    return kSysPrompts[cat][action];
}
