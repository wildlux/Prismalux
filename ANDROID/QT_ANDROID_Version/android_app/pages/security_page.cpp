#include "security_page.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QFrame>
#include <QFont>
#include <QListWidgetItem>
#include <QScroller>
#include <QScrollerProperties>

static const struct { const char* name; const char* icon; const char* prompt; } kAgents[] = {
    {
        "Injection",
        "\xf0\x9f\x92\x89",   /* 💉 */
        "Sei un esperto di sicurezza web. Analizza il codice seguente per vulnerabilità "
        "di INJECTION: SQL Injection, XSS (Cross-Site Scripting), Command Injection, "
        "LDAP Injection, Path Traversal, Template Injection. "
        "Per ogni vulnerabilità trovata: indica la riga (se visibile), il tipo, "
        "la gravità (CRITICA/ALTA/MEDIA/BASSA), e la correzione raccomandata. "
        "Se non trovi vulnerabilità, scrivi 'NESSUNA VULNERABILITÀ DI INJECTION TROVATA'. "
        "Rispondi in italiano con formato Markdown."
    },
    {
        "Segreti",
        "\xf0\x9f\x94\x91",   /* 🔑 */
        "Sei un esperto di sicurezza. Analizza il codice seguente per SEGRETI HARDCODED: "
        "password, token API, chiavi private, secret keys, credenziali di database, "
        "chiavi crittografiche, URL con credenziali incluse, AWS/GCP/Azure keys. "
        "Per ogni segreto trovato: tipo, posizione approssimativa, rischio. "
        "Se non trovi segreti, scrivi 'NESSUN SEGRETO HARDCODED TROVATO'. "
        "Rispondi in italiano con formato Markdown."
    },
    {
        "Memoria",
        "\xf0\x9f\x9a\xa8",   /* 🚨 */
        "Sei un esperto di sicurezza a basso livello. Analizza il codice per problemi di MEMORIA: "
        "buffer overflow, heap overflow, null pointer dereference, use-after-free, "
        "double free, integer overflow/underflow, format string vulnerabilities, "
        "race conditions su risorse condivise. "
        "Per ogni problema: tipo, posizione, gravità, correzione. "
        "Se non trovi problemi, scrivi 'NESSUN PROBLEMA DI MEMORIA TROVATO'. "
        "Rispondi in italiano con formato Markdown."
    },
    {
        "Config",
        "\xe2\x9a\x99\xef\xb8\x8f",  /* ⚙️ */
        "Sei un esperto di sicurezza applicativa. Analizza il codice per problemi di CONFIGURAZIONE: "
        "uso di HTTP invece di HTTPS, SSL/TLS disabilitato o non verificato, "
        "certificati self-signed non validati, valori di configurazione hardcoded, "
        "debug mode attivo, logging di dati sensibili, CORS mal configurato, "
        "header di sicurezza mancanti. "
        "Per ogni problema: tipo, posizione, impatto, soluzione. "
        "Se non trovi problemi, scrivi 'NESSUN PROBLEMA DI CONFIGURAZIONE TROVATO'. "
        "Rispondi in italiano con formato Markdown."
    },
};
static constexpr int kNumAgents = static_cast<int>(sizeof(kAgents) / sizeof(kAgents[0]));

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
   Costruttore
   ══════════════════════════════════════════════════════════════ */
SecurityPage::SecurityPage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    setObjectName("SecurityPage");

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
        QString::fromUtf8("\xf0\x9f\x94\x92  Analisi Sicurezza"), inner);
    QFont tf = title->font();
    tf.setPointSize(15); tf.setBold(true);
    title->setFont(tf);
    title->setAlignment(Qt::AlignCenter);
    vbox->addWidget(title);

    /* ── Stato agenti ── */
    auto* agentGroup = new QGroupBox(
        QString::fromUtf8("\xf0\x9f\xa4\x96  Agenti di analisi"), inner);
    agentGroup->setObjectName("SettingsGroup");
    auto* agentVbox = new QVBoxLayout(agentGroup);

    m_agentList = new QListWidget(inner);
    m_agentList->setFixedHeight(kNumAgents * 44 + 8);
    m_agentList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    for (int i = 0; i < kNumAgents; ++i) {
        const QString label = QString::fromUtf8(kAgents[i].icon)
                              + "  " + kAgents[i].name
                              + "   ⬜";
        auto* item = new QListWidgetItem(label, m_agentList);
        item->setSizeHint(QSize(0, 44));
        Q_UNUSED(item);
    }
    agentVbox->addWidget(m_agentList);
    vbox->addWidget(agentGroup);

    /* ── Input codice ── */
    auto* inputGroup = new QGroupBox(
        QString::fromUtf8("\xf0\x9f\x93\x9d  Codice da analizzare"), inner);
    inputGroup->setObjectName("SettingsGroup");
    auto* inputVbox = new QVBoxLayout(inputGroup);

    m_codeInput = new QTextEdit(inner);
    m_codeInput->setPlaceholderText(
        "Incolla qui il codice da analizzare (qualsiasi linguaggio).\n\n"
        "Esempio: funzione Python, endpoint REST, classe Java, script bash...");
    m_codeInput->setMinimumHeight(150);
    m_codeInput->setMaximumHeight(250);
    inputVbox->addWidget(m_codeInput);
    vbox->addWidget(inputGroup);

    /* ── Bottoni ── */
    auto* btnRow = new QHBoxLayout;
    m_analyzeBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x94\x8d  Analizza"), inner);  /* 🔍 */
    m_analyzeBtn->setObjectName("PrimaryBtn");
    m_analyzeBtn->setMinimumHeight(56);
    {
        QFont f = m_analyzeBtn->font();
        f.setBold(true); f.setPointSize(14);
        m_analyzeBtn->setFont(f);
    }
    m_stopBtn = new QPushButton(
        QString::fromUtf8("\xe2\x8f\xb9  Stop"), inner);
    m_stopBtn->setObjectName("StopBtn");
    m_stopBtn->setMinimumHeight(56);
    m_stopBtn->setEnabled(false);

    btnRow->addWidget(m_analyzeBtn, 3);
    btnRow->addWidget(m_stopBtn,    1);
    vbox->addLayout(btnRow);

    /* ── Progress + status ── */
    m_progressBar = new QProgressBar(inner);
    m_progressBar->setRange(0, kNumAgents);
    m_progressBar->setFixedHeight(6);
    m_progressBar->setVisible(false);
    vbox->addWidget(m_progressBar);

    m_statusLbl = new QLabel("", inner);
    m_statusLbl->setStyleSheet("color:#8890a8; font-size:12px;");
    m_statusLbl->setVisible(false);
    vbox->addWidget(m_statusLbl);

    /* ── Output risultati ── */
    auto* outGroup = new QGroupBox(
        QString::fromUtf8("\xf0\x9f\x93\x84  Rapporto di sicurezza"), inner);
    outGroup->setObjectName("SettingsGroup");
    auto* outVbox = new QVBoxLayout(outGroup);

    m_output = new QTextEdit(inner);
    m_output->setReadOnly(true);
    m_output->setMinimumHeight(300);
    m_output->setPlaceholderText(
        "Il rapporto di sicurezza apparirà qui dopo l'analisi.\n\n"
        "4 agenti analizzano in sequenza: Injection, Segreti, Memoria, Config.");
    outVbox->addWidget(m_output);
    vbox->addWidget(outGroup, 1);

    vbox->addStretch();

    /* ── Connessioni ── */
    connect(m_analyzeBtn, &QPushButton::clicked, this, &SecurityPage::onAnalyzeClicked);
    connect(m_stopBtn,    &QPushButton::clicked, this, &SecurityPage::onStopClicked);
}

/* ══════════════════════════════════════════════════════════════
   onAnalyzeClicked — avvia la pipeline dei 4 agenti sequenziali
   ══════════════════════════════════════════════════════════════ */
void SecurityPage::onAnalyzeClicked()
{
    const QString code = m_codeInput->toPlainText().trimmed();
    if (code.isEmpty()) {
        m_output->setPlainText(
            QString::fromUtf8("\xe2\x9a\xa0\xef\xb8\x8f")
            + "  Incolla del codice da analizzare prima di avviare.");
        return;
    }
    if (m_busy) return;

    m_busy = true;
    m_currentAgent = 0;
    m_output->clear();
    m_analyzeBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);
    m_progressBar->setValue(0);
    m_progressBar->setVisible(true);

    /* Reset icone agenti */
    for (int i = 0; i < m_agentList->count(); ++i) {
        auto* item = m_agentList->item(i);
        item->setText(QString::fromUtf8(kAgents[i].icon)
                      + "  " + kAgents[i].name + "   ⬜");
    }

    runNextAgent();
}

/* ══════════════════════════════════════════════════════════════
   runNextAgent — lancia il prossimo agente in coda
   ══════════════════════════════════════════════════════════════ */
void SecurityPage::runNextAgent()
{
    if (m_currentAgent >= kNumAgents) {
        /* Pipeline completata */
        m_busy = false;
        m_analyzeBtn->setEnabled(true);
        m_stopBtn->setEnabled(false);
        m_progressBar->setValue(kNumAgents);
        m_statusLbl->setText(
            QString::fromUtf8("\xe2\x9c\x85")
            + "  Analisi completata — " + QString::number(kNumAgents) + " agenti.");
        m_statusLbl->setVisible(true);
        return;
    }

    const int i = m_currentAgent;
    /* Segnala "in esecuzione" nell'elenco */
    if (auto* item = m_agentList->item(i))
        item->setText(QString::fromUtf8(kAgents[i].icon)
                      + "  " + kAgents[i].name + "   🔄");

    m_statusLbl->setText(
        QString::fromUtf8("\xf0\x9f\x94\x84")
        + QString("  Agente %1/%2: %3…").arg(i + 1).arg(kNumAgents).arg(kAgents[i].name));
    m_statusLbl->setVisible(true);
    m_progressBar->setValue(i);

    m_currentResult.clear();
    const QString code = m_codeInput->toPlainText().trimmed();
    const QString prompt = QString::fromUtf8(kAgents[i].prompt)
                           + "\n\n```\n" + code.left(4000) + "\n```";

    m_tokConn = connect(m_ai, &AiClient::token, this, &SecurityPage::onToken);
    m_finConn = connect(m_ai, &AiClient::finished, this, &SecurityPage::onFinished);
    m_errConn = connect(m_ai, &AiClient::error, this, &SecurityPage::onAiError);

    m_ai->chat(prompt, "Analizza il codice come da istruzioni.");
}

/* ── Token streaming ── */
void SecurityPage::onToken(const QString& t)
{
    m_currentResult += t;
}

/* ── Fine agente corrente ── */
void SecurityPage::onFinished(const QString& full)
{
    disconnect(m_tokConn);
    disconnect(m_finConn);
    disconnect(m_errConn);

    const QString result = full.isEmpty() ? m_currentResult : full;
    appendResult(kAgents[m_currentAgent].name, result);

    /* Segna ✅ */
    if (auto* item = m_agentList->item(m_currentAgent))
        item->setText(QString::fromUtf8(kAgents[m_currentAgent].icon)
                      + "  " + kAgents[m_currentAgent].name + "   \xe2\x9c\x85");

    ++m_currentAgent;
    m_progressBar->setValue(m_currentAgent);
    runNextAgent();
}

/* ── Errore agente ── */
void SecurityPage::onAiError(const QString& msg)
{
    disconnect(m_tokConn);
    disconnect(m_finConn);
    disconnect(m_errConn);

    if (auto* item = m_agentList->item(m_currentAgent))
        item->setText(QString::fromUtf8(kAgents[m_currentAgent].icon)
                      + "  " + kAgents[m_currentAgent].name + "   \xe2\x9d\x8c");

    m_output->append(
        "\n\n## ❌ Agente " + QString(kAgents[m_currentAgent].name) + " — Errore\n"
        + msg + "\n");

    ++m_currentAgent;
    runNextAgent();
}

/* ── Stop ── */
void SecurityPage::onStopClicked()
{
    m_ai->abort();
    disconnect(m_tokConn);
    disconnect(m_finConn);
    disconnect(m_errConn);
    m_busy = false;
    m_analyzeBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
    m_progressBar->setVisible(false);
    m_statusLbl->setText(
        QString::fromUtf8("\xe2\x9b\x94")   /* ⛔ */
        + "  Analisi interrotta dall'utente.");
}

/* ── appendResult — aggiunge sezione all'output ── */
void SecurityPage::appendResult(const QString& agentName, const QString& text)
{
    m_output->append(
        "\n---\n## " + QString::fromUtf8("\xf0\x9f\x94\x8d")   /* 🔍 */
        + " Agente: " + agentName + "\n\n" + text + "\n");
}
