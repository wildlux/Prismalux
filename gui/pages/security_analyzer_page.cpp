#include "security_analyzer_page.h"
#include "../prismalux_paths.h"
#include "../dpi_utils.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QProgressBar>
#include <QTabWidget>
#include <QTimer>
#include <QRegularExpression>

namespace P = PrismaluxPaths;

/* ══════════════════════════════════════════════════════════════
   AgentSpec — nome + system prompt per ciascuno dei 4 agenti
   ══════════════════════════════════════════════════════════════ */
struct AgentSpec {
    const char* name;
    const char* systemPrompt;
};

static constexpr int kNumAgentsLocal = 4;

static const AgentSpec kAgents[kNumAgentsLocal] = {
    {
        "Analizzatore Injection",
        "Sei un esperto di sicurezza offensiva e difensiva. "
        "Analizza il codice cercando INJECTION vulnerabilities: "
        "SQL injection, command injection, path traversal, XSS, SSRF, open redirect. "
        "Per ogni vulnerabilita' trovata usa il formato: "
        "SEVERITY: [critico|alto|medio|basso|info] | ISSUE: descrizione | "
        "LOCATION: riga/funzione | REMEDY: rimedio. "
        "Se non trovi nulla scrivi 'Nessuna vulnerabilita' di injection trovata.' "
        "Rispondi SOLO con i findings, nessun testo extra."
    },
    {
        "Analizzatore Segreti",
        "Sei un esperto di sicurezza. "
        "Analizza il codice cercando HARDCODED SECRETS: "
        "credenziali hardcoded, API key, token, password in chiaro, "
        "connection string con credenziali, private key incluse nel codice. "
        "Per ogni segreto trovato usa il formato: "
        "SEVERITY: [critico|alto|medio|basso|info] | ISSUE: descrizione | "
        "LOCATION: riga/funzione | REMEDY: rimedio. "
        "Se non trovi nulla scrivi 'Nessun segreto hardcoded trovato.' "
        "Rispondi SOLO con i findings, nessun testo extra."
    },
    {
        "Analizzatore Memoria",
        "Sei un esperto di sicurezza a basso livello. "
        "Analizza il codice cercando MEMORY SAFETY issues: "
        "buffer overflow, use-after-free, double free, format string bugs, "
        "integer overflow, race condition, uninitialized read. "
        "Per ogni problema trovato usa il formato: "
        "SEVERITY: [critico|alto|medio|basso|info] | ISSUE: descrizione | "
        "LOCATION: riga/funzione | REMEDY: rimedio. "
        "Se non trovi nulla scrivi 'Nessun problema di memoria trovato.' "
        "Rispondi SOLO con i findings, nessun testo extra."
    },
    {
        "Analizzatore Config",
        "Sei un esperto di security configuration review. "
        "Analizza il codice cercando CONFIGURATION issues: "
        "TLS/SSL disabilitato o versione insicura, CORS wildcard (*), "
        "autenticazione opzionale o bypassabile, porte esposte non necessarie, "
        "debug mode in produzione, insecure defaults, logging eccessivo di dati sensibili. "
        "Per ogni problema trovato usa il formato: "
        "SEVERITY: [critico|alto|medio|basso|info] | ISSUE: descrizione | "
        "LOCATION: riga/funzione | REMEDY: rimedio. "
        "Se non trovi nulla scrivi 'Nessun problema di configurazione trovato.' "
        "Rispondi SOLO con i findings, nessun testo extra."
    }
};

/* ══════════════════════════════════════════════════════════════
   Costruttore
   ══════════════════════════════════════════════════════════════ */
SecurityAnalyzerPage::SecurityAnalyzerPage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    m_agentResults.resize(kNumAgents);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    /* ── Barra superiore ── */
    auto* topBar = new QWidget(this);
    topBar->setObjectName("modelBarMath");
    auto* topLay = new QHBoxLayout(topBar);
    topLay->setContentsMargins(12, 8, 12, 8);
    topLay->setSpacing(8);

    auto* title = new QLabel(
        "\xf0\x9f\x94\x90  <b>Security Analyzer</b>"
        " \xe2\x80\x94 4 agenti specializzati in parallelo", topBar);  /* 🔐 — */
    title->setTextFormat(Qt::RichText);
    title->setObjectName("cardDesc");
    topLay->addWidget(title, 1);

    m_modelCombo = new ModelComboBox(m_ai, topBar);
    m_modelCombo->setToolTip("Modello LLM usato dagli agenti di analisi");
    topLay->addWidget(m_modelCombo);

    m_btnAnalyze = new QPushButton(
        "\xf0\x9f\x94\x8d  Analizza", topBar);  /* 🔍 */
    m_btnAnalyze->setObjectName("actionBtn");
    m_btnAnalyze->setProperty("highlight", "true");
    m_btnAnalyze->setMinimumHeight(dpiScale(32));
    connect(m_btnAnalyze, &QPushButton::clicked,
            this, &SecurityAnalyzerPage::onAnalyzeClicked);
    topLay->addWidget(m_btnAnalyze);

    m_btnStop = new QPushButton("\xe2\x96\xa0  Stop", topBar);  /* ■ */
    m_btnStop->setObjectName("stopBtn");
    m_btnStop->setEnabled(false);
    connect(m_btnStop, &QPushButton::clicked,
            this, &SecurityAnalyzerPage::onStopClicked);
    topLay->addWidget(m_btnStop);

    root->addWidget(topBar);

    /* ── Splitter input | output ── */
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setHandleWidth(4);

    /* Pannello sinistro: input codice */
    m_codeInput = new QTextEdit(splitter);
    m_codeInput->setObjectName("chatLog");
    m_codeInput->setPlaceholderText(
        "Incolla qui il codice da analizzare...\n\n"
        "Supportati: Python, C/C++, JavaScript, Java, PHP, Go, Rust, ecc.");
    m_codeInput->setAcceptRichText(false);
    /* font monospaced per il codice */
    QFont mono("Monospace");
    mono.setStyleHint(QFont::TypeWriter);
    mono.setPointSize(10);
    m_codeInput->setFont(mono);
    splitter->addWidget(m_codeInput);

    /* Pannello destro: tab output */
    m_outputTabs = new QTabWidget(splitter);
    m_outputTabs->setObjectName("mainTabs");

    m_reportOutput = new QPlainTextEdit(m_outputTabs);
    m_reportOutput->setObjectName("chatLog");
    m_reportOutput->setReadOnly(true);
    m_reportOutput->setPlaceholderText(
        "Il report di sicurezza apparir\xc3\xa0 qui dopo l'analisi...");  /* à */
    m_outputTabs->addTab(m_reportOutput, "Report");

    m_rawOutput = new QPlainTextEdit(m_outputTabs);
    m_rawOutput->setObjectName("chatLog");
    m_rawOutput->setReadOnly(true);
    m_rawOutput->setPlaceholderText(
        "Output grezzo di ciascun agente...");
    m_outputTabs->addTab(m_rawOutput, "Dettagli");

    splitter->addWidget(m_outputTabs);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 3);
    root->addWidget(splitter, 1);

    /* ── Barra inferiore: status + progress ── */
    auto* botBar = new QWidget(this);
    botBar->setObjectName("modelBarMath");
    auto* botLay = new QHBoxLayout(botBar);
    botLay->setContentsMargins(12, 4, 12, 4);
    botLay->setSpacing(8);

    m_status = new QLabel(
        "\xf0\x9f\x94\x90  Pronto. Incolla il codice e premi Analizza.", botBar);  /* 🔐 */
    m_status->setObjectName("statusLabel");
    botLay->addWidget(m_status, 1);

    m_progress = new QProgressBar(botBar);
    m_progress->setRange(0, 0);       /* indeterminata */
    m_progress->setFixedWidth(dpiScale(120));
    m_progress->setFixedHeight(dpiScale(14));
    m_progress->setVisible(false);
    botLay->addWidget(m_progress);

    root->addWidget(botBar);

    /* Carica modelli al primo avvio */
    /* ModelComboBox gestisce fetch e modelChanged autonomamente */
}

SecurityAnalyzerPage::~SecurityAnalyzerPage()
{
    for (AiClient* c : m_pool) c->abort();
    for (auto* h : m_holders) delete h;
}

/* ══════════════════════════════════════════════════════════════
   initPool — crea/aggiorna il pool di kNumAgents AiClient
   ══════════════════════════════════════════════════════════════ */
void SecurityAnalyzerPage::initPool()
{
    if (!m_ai) return;
    while (m_pool.size() < kNumAgents)
        m_pool.append(new AiClient(this));
    for (AiClient* c : m_pool)
        c->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), m_ai->model());
}

/* ══════════════════════════════════════════════════════════════
   onAnalyzeClicked
   ══════════════════════════════════════════════════════════════ */
void SecurityAnalyzerPage::onAnalyzeClicked()
{
    if (!m_ai) return;
    const QString code = m_codeInput->toPlainText().trimmed();
    if (code.isEmpty()) {
        setStatus("\xe2\x9a\xa0\xef\xb8\x8f  Incolla del codice prima di avviare l'analisi.");  /* ⚠️ */
        return;
    }

    /* Aggiorna modello se l'utente ne ha scelto uno diverso */
    const QString sel = m_modelCombo->currentData().toString();
    if (!sel.isEmpty() && sel != m_ai->model())
        m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), sel);

    m_reportOutput->clear();
    m_rawOutput->clear();
    m_agentResults.fill(QString(), kNumAgents);

    m_btnAnalyze->setEnabled(false);
    m_btnStop->setEnabled(true);
    m_progress->setVisible(true);

    initPool();
    runAgents(code);
}

/* ══════════════════════════════════════════════════════════════
   onStopClicked
   ══════════════════════════════════════════════════════════════ */
void SecurityAnalyzerPage::onStopClicked()
{
    m_ai->abort();
    for (AiClient* c : m_pool) c->abort();
    for (auto* h : m_holders) {
        delete h;
    }
    m_holders.clear();
    m_pendingCount = 0;
    m_btnAnalyze->setEnabled(true);
    m_btnStop->setEnabled(false);
    m_progress->setVisible(false);
    setStatus("\xe2\x96\xa0  Analisi interrotta.");  /* ■ */
}

/* ══════════════════════════════════════════════════════════════
   runAgents — avvia tutti e 4 gli agenti in parallelo
   ══════════════════════════════════════════════════════════════ */
void SecurityAnalyzerPage::runAgents(const QString& code)
{
    m_pendingCount = kNumAgents;
    setStatus("\xf0\x9f\x94\x84  "
              "Analisi in corso: 4 agenti in parallelo...");  /* 🔄 */

    for (int i = 0; i < kNumAgents; ++i)
        runAgent(i, m_pool[i], code);
}

/* ══════════════════════════════════════════════════════════════
   runAgent — avvia il singolo agente idx con holder one-shot
   ══════════════════════════════════════════════════════════════ */
void SecurityAnalyzerPage::runAgent(int idx, AiClient* client, const QString& code)
{
    const QString agentName = QString::fromUtf8(kAgents[idx].name);
    const QString sys       = QString::fromUtf8(kAgents[idx].systemPrompt);
    const QString user      = "Codice da analizzare:\n```\n" + code + "\n```";

    m_rawOutput->appendPlainText(
        "── " + agentName + " ── avviato\n");

    delete m_holders.value(idx, nullptr);
    m_holders[idx] = new QObject(this);

    connect(client, &AiClient::token, m_holders[idx],
            [this, idx](const QString& tok) {
                m_agentResults[idx] += tok;
            });

    connect(client, &AiClient::finished, m_holders[idx],
            [this, idx](const QString& full) {
                onAgentFinished(idx, full);
            });

    connect(client, &AiClient::error, m_holders[idx],
            [this, idx](const QString& msg) {
                onAgentError(idx, msg);
            });

    client->chat(sys, user);
}

/* ══════════════════════════════════════════════════════════════
   onAgentFinished — chiamato quando un agente completa
   ══════════════════════════════════════════════════════════════ */
void SecurityAnalyzerPage::onAgentFinished(int idx, const QString& result)
{
    delete m_holders.value(idx, nullptr);
    m_holders.remove(idx);

    m_agentResults[idx] = result;

    const QString agentName = QString::fromUtf8(kAgents[idx].name);
    m_rawOutput->appendPlainText(
        "\n── " + agentName + " ── completato\n" + result + "\n");

    --m_pendingCount;
    setStatus(QString("\xf0\x9f\x94\x84  Agenti completati: %1/%2")  /* 🔄 */
              .arg(kNumAgents - m_pendingCount)
              .arg(kNumAgents));

    if (m_pendingCount == 0)
        synthesize();
}

/* ══════════════════════════════════════════════════════════════
   onAgentError — chiamato su errore agente
   ══════════════════════════════════════════════════════════════ */
void SecurityAnalyzerPage::onAgentError(int idx, const QString& msg)
{
    delete m_holders.value(idx, nullptr);
    m_holders.remove(idx);

    const QString agentName = QString::fromUtf8(kAgents[idx].name);
    m_agentResults[idx] = "[Errore: " + msg + "]";

    m_rawOutput->appendPlainText(
        "\n── " + agentName + " ── ERRORE: " + msg + "\n");

    --m_pendingCount;
    if (m_pendingCount == 0)
        synthesize();
}

/* ══════════════════════════════════════════════════════════════
   synthesize — agente di sintesi su m_ai che produce il report finale
   ══════════════════════════════════════════════════════════════ */
void SecurityAnalyzerPage::synthesize()
{
    setStatus("\xf0\x9f\x93\x9d  Sintesi report in corso...");  /* 📝 */

    const QString sys =
        "Sei un esperto di security review. Ricevi i risultati di 4 agenti specializzati"
        " che hanno analizzato lo stesso codice. Produci un report Markdown strutturato"
        " ordinato per severita' decrescente.\n\n"
        "FORMATO RICHIESTO:\n"
        "## Riepilogo Criticita'\n"
        "(tabella o lista conteggio per severita')\n\n"
        "## Critici\n"
        "(lista findings critici con issue, location, remedy)\n\n"
        "## Alti\n"
        "(lista findings alti)\n\n"
        "## Medi\n"
        "(lista findings medi)\n\n"
        "## Bassi\n"
        "(lista findings bassi)\n\n"
        "## Conclusioni\n"
        "(valutazione complessiva del rischio e raccomandazioni principali)\n\n"
        "Se una sezione e' vuota scrivi: 'Nessun finding.'";

    QString user = "Risultati degli agenti:\n\n";
    for (int i = 0; i < kNumAgents; ++i) {
        user += "### " + QString::fromUtf8(kAgents[i].name) + "\n";
        user += m_agentResults[i] + "\n\n";
    }

    auto* holder = new QObject(this);

    connect(m_ai, &AiClient::token, holder,
            [this](const QString& tok) {
                /* Accoda i token nel report direttamente */
                QTextCursor cur = m_reportOutput->textCursor();
                cur.movePosition(QTextCursor::End);
                m_reportOutput->setTextCursor(cur);
                m_reportOutput->insertPlainText(tok);
            });

    connect(m_ai, &AiClient::finished, holder,
            [this, holder](const QString& /*full*/) {
                delete holder;
                m_pendingCount = 0;
                m_btnAnalyze->setEnabled(true);
                m_btnStop->setEnabled(false);
                m_progress->setVisible(false);
                setStatus("\xe2\x9c\x85  Analisi completata.");  /* ✅ */
            });

    connect(m_ai, &AiClient::error, holder,
            [this, holder](const QString& msg) {
                delete holder;
                m_btnAnalyze->setEnabled(true);
                m_btnStop->setEnabled(false);
                m_progress->setVisible(false);
                setStatus("\xe2\x9d\x8c  Errore sintesi: " + msg.left(80));  /* ❌ */
                m_reportOutput->appendPlainText(
                    "\n[Errore durante la sintesi: " + msg + "]");
            });

    m_ai->chat(sys, user);
}

/* ══════════════════════════════════════════════════════════════
   setStatus
   ══════════════════════════════════════════════════════════════ */
void SecurityAnalyzerPage::setStatus(const QString& s)
{
    if (m_status) m_status->setText(s);
}

