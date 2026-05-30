#include "agenti_multi_page.h"
#include "../prismalux_paths.h"
#include "../dpi_utils.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QListWidgetItem>
#include <QProgressBar>
#include <QScrollArea>
#include <QFrame>
#include <QComboBox>
#include <QTabWidget>
#include <QGroupBox>
#include <QTimer>
#include <QFileDialog>
#include <QApplication>
#include <QClipboard>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QStandardPaths>

namespace P = PrismaluxPaths;

/* Rimuove blocchi <think>...</think> prodotti da modelli reasoning
 * (qwen3, deepseek-r1, qwq). Il regex è non-greedy per gestire
 * risposte con più blocchi think consecutivi. */
static QString stripThinkTags(const QString& s)
{
    static const QRegularExpression re(
        "<think>[\\s\\S]*?</think>",
        QRegularExpression::CaseInsensitiveOption);
    QString out = s;
    out.remove(re);
    return out.trimmed();
}

/* ══════════════════════════════════════════════════════════════
   Costruttore
   ══════════════════════════════════════════════════════════════ */
AgentiMultiPage::AgentiMultiPage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    /* Apri GraphMemory su SQLite in ~/.prismalux/ */
    const QString gmPath = QStandardPaths::writableLocation(
                               QStandardPaths::HomeLocation)
                           + "/.prismalux/graph_memory.db";
    m_gm = new GraphMemory(gmPath, this);
    m_gm->open();
    connect(m_gm, &GraphMemory::changed, this, &AgentiMultiPage::onGraphMemoryChanged);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    /* Barra superiore: prompt + modello + bottoni */
    root->addWidget(buildInputBar());

    auto* sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Sunken);
    root->addWidget(sep);

    /* Splitter orizzontale: pannello task | output */
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setHandleWidth(4);
    splitter->addWidget(buildTaskPanel());
    splitter->addWidget(buildOutputPanel());
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);
    root->addWidget(splitter, 1);

    /* Barra memoria in fondo */
    auto* memSep = new QFrame(this);
    memSep->setFrameShape(QFrame::HLine);
    memSep->setFrameShadow(QFrame::Sunken);
    root->addWidget(memSep);
    root->addWidget(buildMemoryBar());

    /* Carica modelli al primo avvio */
    QTimer::singleShot(0, this, &AgentiMultiPage::onFillModels);
    if (m_ai)
        connect(m_ai, &AiClient::modelChanged, m_modelCombo,
                [this](const QString& m) {
                    const int idx = m_modelCombo->findData(m);
                    if (idx >= 0) m_modelCombo->setCurrentIndex(idx);
                });

    refreshMemoryStats();
    setStatus("\xf0\x9f\x95\xb8  Pronto. Descrivi un compito complesso e premi Decomponsi.");  /* 🕸️ */
}

AgentiMultiPage::~AgentiMultiPage()
{
    for (AiClient* c : m_aiPool) c->abort();
    delete m_decompHolder;
    delete m_synthHolder;
    for (auto* h : m_taskHolders) delete h;
}

/* ══════════════════════════════════════════════════════════════
   buildInputBar
   ══════════════════════════════════════════════════════════════ */
QWidget* AgentiMultiPage::buildInputBar()
{
    auto* bar = new QWidget(this);
    bar->setObjectName("modelBarMath");
    auto* lay = new QVBoxLayout(bar);
    lay->setContentsMargins(12, 8, 12, 8);
    lay->setSpacing(6);

    /* Riga 1: titolo */
    auto* titleRow = new QHBoxLayout;
    auto* title = new QLabel(
        "\xf0\x9f\x95\xb8  <b>Multi-Agente</b> — Decomposizione + Memoria a Grafo", bar);
    title->setTextFormat(Qt::RichText);
    title->setObjectName("cardDesc");
    titleRow->addWidget(title, 1);

    m_modelCombo = new QComboBox(bar);
    m_modelCombo->setObjectName("settingCombo");
    m_modelCombo->setMinimumWidth(dpiScale(180));
    m_modelCombo->setToolTip("Modello LLM usato da master e sub-agenti");
    titleRow->addWidget(m_modelCombo);
    lay->addLayout(titleRow);

    /* Riga 2: prompt */
    m_promptInput = new QTextEdit(bar);
    m_promptInput->setObjectName("chatLog");
    m_promptInput->setPlaceholderText(
        "Descrivi un compito complesso che richiede pi\xc3\xb9 agenti specializzati...\n"
        "Es: \xc2\xabAnalizza le ultime ricerche su GraphRAG, compara gli approcci e scrivi un report con codice Python\xc2\xbb");
    m_promptInput->setMaximumHeight(dpiScale(70));
    lay->addWidget(m_promptInput);

    /* Riga 3: bottoni */
    auto* btnRow = new QHBoxLayout;

    m_btnDecompose = new QPushButton(
        "\xf0\x9f\x9a\x80  Decomponsi + Esegui", bar);  /* 🚀 */
    m_btnDecompose->setObjectName("actionBtn");
    m_btnDecompose->setProperty("highlight", "true");
    m_btnDecompose->setMinimumHeight(dpiScale(32));
    connect(m_btnDecompose, &QPushButton::clicked,
            this, &AgentiMultiPage::onDecomposeClicked);
    btnRow->addWidget(m_btnDecompose);

    m_btnStop = new QPushButton("\xe2\x96\xa0  Stop", bar);  /* ■ */
    m_btnStop->setObjectName("stopBtn");
    m_btnStop->setEnabled(false);
    connect(m_btnStop, &QPushButton::clicked, this, &AgentiMultiPage::onStopClicked);
    btnRow->addWidget(m_btnStop);

    m_btnClear = new QPushButton("\xf0\x9f\x97\x91  Reset", bar);  /* 🗑 */
    m_btnClear->setObjectName("navBtn");
    connect(m_btnClear, &QPushButton::clicked, this, &AgentiMultiPage::onClearClicked);
    btnRow->addWidget(m_btnClear);

    btnRow->addStretch(1);

    m_status = new QLabel("\xf0\x9f\x95\xb8  Pronto.", bar);
    m_status->setObjectName("statusLabel");
    btnRow->addWidget(m_status, 2);

    lay->addLayout(btnRow);
    return bar;
}

/* ══════════════════════════════════════════════════════════════
   buildTaskPanel — lista sub-task con stato
   ══════════════════════════════════════════════════════════════ */
QWidget* AgentiMultiPage::buildTaskPanel()
{
    auto* panel = new QWidget(this);
    auto* lay   = new QVBoxLayout(panel);
    lay->setContentsMargins(8, 6, 4, 6);
    lay->setSpacing(4);

    auto* hdr = new QLabel("<b>Sub-Task</b>", panel);
    hdr->setTextFormat(Qt::RichText);
    hdr->setObjectName("cardDesc");
    lay->addWidget(hdr);

    m_taskList = new QListWidget(panel);
    m_taskList->setObjectName("chatLog");
    m_taskList->setWordWrap(true);
    m_taskList->setAlternatingRowColors(true);
    connect(m_taskList, &QListWidget::itemClicked,
            this, &AgentiMultiPage::onTaskItemClicked);
    lay->addWidget(m_taskList, 1);

    auto* hint = new QLabel(
        "<small style='color:#64748b'>Clicca un task per vedere il risultato</small>",
        panel);
    hint->setTextFormat(Qt::RichText);
    lay->addWidget(hint);

    return panel;
}

/* ══════════════════════════════════════════════════════════════
   buildOutputPanel — tab Risultato / Grafo DOT / JSON
   ══════════════════════════════════════════════════════════════ */
QWidget* AgentiMultiPage::buildOutputPanel()
{
    m_outputTabs = new QTabWidget(this);
    m_outputTabs->setObjectName("mainTabs");

    /* Tab 1: Risultato / log */
    m_outputLog = new QPlainTextEdit(m_outputTabs);
    m_outputLog->setObjectName("chatLog");
    m_outputLog->setReadOnly(true);
    m_outputLog->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    m_outputLog->setPlaceholderText(
        "L'output dei sub-agenti e la sintesi finale appariranno qui...");
    m_outputTabs->addTab(m_outputLog, "\xf0\x9f\x93\x9d  Risultato");  /* 📝 */

    /* Tab 2: Grafo DOT */
    m_dotView = new QPlainTextEdit(m_outputTabs);
    m_dotView->setObjectName("chatLog");
    m_dotView->setReadOnly(true);
    m_dotView->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_dotView->setPlaceholderText("Il grafo Graphviz DOT della memoria condivisa...");
    m_outputTabs->addTab(m_dotView, "\xf0\x9f\x95\xb8  Grafo DOT");

    /* Tab 3: JSON memoria */
    m_jsonView = new QPlainTextEdit(m_outputTabs);
    m_jsonView->setObjectName("chatLog");
    m_jsonView->setReadOnly(true);
    m_jsonView->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_jsonView->setPlaceholderText("Dump JSON della GraphMemory...");
    m_outputTabs->addTab(m_jsonView, "\xf0\x9f\x93\x8b  JSON");

    return m_outputTabs;
}

/* ══════════════════════════════════════════════════════════════
   buildMemoryBar — statistiche e controlli GraphMemory
   ══════════════════════════════════════════════════════════════ */
QWidget* AgentiMultiPage::buildMemoryBar()
{
    auto* bar = new QWidget(this);
    bar->setObjectName("modelBarMath");
    auto* lay = new QHBoxLayout(bar);
    lay->setContentsMargins(12, 4, 12, 4);
    lay->setSpacing(8);

    auto* lbl = new QLabel("\xf0\x9f\xa7\xa0  <b>GraphMemory:</b>", bar);  /* 🧠 */
    lbl->setTextFormat(Qt::RichText);
    lbl->setObjectName("cardDesc");
    lay->addWidget(lbl);

    m_memStats = new QLabel("0 nodi · 0 archi", bar);
    m_memStats->setObjectName("statusLabel");
    lay->addWidget(m_memStats, 1);

    m_btnExportTxt = new QPushButton(
        "\xf0\x9f\x93\x84  Esporta TXT", bar);  /* 📄 */
    m_btnExportTxt->setObjectName("navBtn");
    m_btnExportTxt->setFixedHeight(dpiScale(24));
    m_btnExportTxt->setToolTip("Salva snapshot testuale della memoria condivisa");
    connect(m_btnExportTxt, &QPushButton::clicked,
            this, &AgentiMultiPage::onExportTxtClicked);
    lay->addWidget(m_btnExportTxt);

    m_btnClearMem = new QPushButton(
        "\xf0\x9f\x97\x91  Svuota Grafo", bar);  /* 🗑 */
    m_btnClearMem->setObjectName("navBtn");
    m_btnClearMem->setFixedHeight(dpiScale(24));
    m_btnClearMem->setToolTip("Cancella tutta la memoria a grafo (irreversibile)");
    connect(m_btnClearMem, &QPushButton::clicked,
            this, &AgentiMultiPage::onClearMemClicked);
    lay->addWidget(m_btnClearMem);

    return bar;
}

/* ══════════════════════════════════════════════════════════════
   Slot: onDecomposeClicked
   ══════════════════════════════════════════════════════════════ */
void AgentiMultiPage::onDecomposeClicked()
{
    if (m_decomposeBusy || !m_ai) return;
    const QString prompt = m_promptInput->toPlainText().trimmed();
    if (prompt.isEmpty()) return;

    /* Seleziona modello */
    const QString sel = m_modelCombo->currentData().toString();
    if (!sel.isEmpty() && sel != m_ai->model())
        m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), sel);

    clearOutput();
    m_tasks.clear();
    m_taskList->clear();
    m_decomposeBusy = true;
    m_btnDecompose->setEnabled(false);
    m_btnStop->setEnabled(true);
    setStatus("\xf0\x9f\x94\x84  Master agent: decomposizione compito...");

    initPool();    /* sinc pool con le impostazioni correnti di m_ai */
    decompose(prompt);
}

/* ══════════════════════════════════════════════════════════════
   decompose — chiama il Master Agent per scomporre il compito
   ══════════════════════════════════════════════════════════════ */
void AgentiMultiPage::decompose(const QString& userPrompt)
{
    /* Recupera contesto dalla memoria condivisa */
    QString memCtx;
    if (m_gm && m_gm->nodeCount() > 0) {
        memCtx = "\n\nContesto dalla memoria condivisa (top-10 nodi):\n";
        const auto nodes = m_gm->allNodes();
        const int n = std::min(10, (int)nodes.size());
        for (int i = 0; i < n; ++i)
            memCtx += "- [" + nodes[i].type + "] " + nodes[i].label
                   + ": " + nodes[i].content.left(120) + "\n";
    }

    const QString sys =
        "Sei un orchestratore di agenti AI. Il tuo compito e' scomporre un problema"
        " complesso in sotto-task specializzati, ognuno assegnabile a un sub-agente diverso.\n\n"
        "REGOLE:\n"
        "- Rispondi SOLO con JSON valido, nessun testo fuori dal JSON.\n"
        "- Massimo 5 sub-task.\n"
        "- Ogni prompt deve essere autonomo e completo.\n"
        "- USA depends_on per ordinare i task (lista di id precedenti).\n\n"
        "FORMATO OUTPUT:\n"
        "{\n"
        "  \"task\": \"descrizione del compito principale\",\n"
        "  \"subtasks\": [\n"
        "    {\"id\":1,\"role\":\"Ricercatore\",\"prompt\":\"...\",\"depends_on\":[]},\n"
        "    {\"id\":2,\"role\":\"Analista\",\"prompt\":\"...\",\"depends_on\":[1]},\n"
        "    {\"id\":3,\"role\":\"Scrittore\",\"prompt\":\"...\",\"depends_on\":[1,2]}\n"
        "  ]\n"
        "}";

    const QString user = "Compito da scomporre:\n" + userPrompt + memCtx;

    delete m_decompHolder;
    m_decompHolder = new QObject(this);

    QString accumulated;
    connect(m_ai, &AiClient::token, m_decompHolder,
            [&accumulated](const QString& tok) { accumulated += tok; });

    connect(m_ai, &AiClient::finished, m_decompHolder,
            [this, accumulated](const QString& full) mutable {
                accumulated = full;
                delete m_decompHolder;
                m_decompHolder = nullptr;
                m_decomposeBusy = false;
                parsePlan(full);
            });

    connect(m_ai, &AiClient::error, m_decompHolder,
            [this](const QString& msg) {
                delete m_decompHolder;
                m_decompHolder = nullptr;
                m_decomposeBusy = false;
                m_btnDecompose->setEnabled(true);
                m_btnStop->setEnabled(false);
                setStatus("\xe2\x9d\x8c  Errore decomposizione: " + msg.left(80));
                appendOutput("<p style='color:#f87171'>\xe2\x9d\x8c Errore: " + msg + "</p>");
            });

    m_ai->chat(sys, user);
}

/* ══════════════════════════════════════════════════════════════
   parsePlan — interpreta il JSON dal Master Agent
   ══════════════════════════════════════════════════════════════ */
void AgentiMultiPage::parsePlan(const QString& jsonPlan)
{
    /* Rimuove <think>...</think> dei modelli reasoning prima di cercare il JSON.
     * Senza questo, il regex greedy cattura dal primo { dentro <think>
     * all'ultimo } del piano, producendo JSON invalido. */
    QString clean = stripThinkTags(jsonPlan);

    /* Rimuove anche eventuali code fence markdown: ```json ... ``` */
    static const QRegularExpression reFence(R"(```[a-z]*\n?([\s\S]*?)```)");
    const auto fence = reFence.match(clean);
    if (fence.hasMatch()) clean = fence.captured(1).trimmed();

    /* Estrai il blocco JSON dall'output (può contenere testo prima/dopo) */
    static const QRegularExpression reJson(R"(\{[\s\S]*\})");
    const auto match = reJson.match(clean);
    if (match.hasMatch()) clean = match.captured(0);

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(clean.toUtf8(), &err);

    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        appendOutput(
            "<p style='color:#fbbf24'>\xe2\x9a\xa0\xef\xb8\x8f"
            "  Piano non JSON — esecuzione diretta del prompt.</p>");
        /* Fallback: un solo sub-task con il prompt originale */
        SubTask t;
        t.id     = 1;
        t.role   = "Agente Universale";
        t.prompt = m_promptInput->toPlainText().trimmed();
        m_tasks.append(t);
        addTaskItem(t);
        setStatus("\xf0\x9f\x94\x84  Avvio agente...");
        runNextPendingTask();
        return;
    }

    const QJsonObject root = doc.object();
    const QString mainTask = root.value("task").toString();
    const QJsonArray subtasks = root.value("subtasks").toArray();

    /* Salva il compito principale in GraphMemory */
    if (m_gm) {
        const QString nodeId = m_gm->addNode("task", mainTask.left(80), mainTask, 1.0f,
                                              {{"source", "master_agent"}});
        Q_UNUSED(nodeId)
    }

    appendOutput("<h3 style='color:#818cf8'>\xf0\x9f\x9a\x80 Piano di esecuzione</h3>");
    appendOutput("<p><b>Compito:</b> " + mainTask + "</p>");
    appendOutput("<p><b>" + QString::number(subtasks.size()) + " sub-task rilevati</b></p><hr>");

    for (const QJsonValue& v : subtasks) {
        const QJsonObject st = v.toObject();
        SubTask t;
        t.id     = st.value("id").toInt();
        t.role   = st.value("role").toString();
        t.prompt = st.value("prompt").toString();

        for (const QJsonValue& d : st.value("depends_on").toArray())
            t.dependsOn.append(d.toInt());

        m_tasks.append(t);
        addTaskItem(t);
    }

    setStatus(QString("\xf0\x9f\x94\x84  Esecuzione %1 sub-task...").arg(m_tasks.size()));
    m_btnDecompose->setEnabled(false);
    m_btnStop->setEnabled(true);
    runNextPendingTask();
}

/* ══════════════════════════════════════════════════════════════
   runNextPendingTask — avvia TUTTI i task eseguibili in parallelo
   (fino a kMaxParallel sub-agenti simultanei).
   ══════════════════════════════════════════════════════════════ */
void AgentiMultiPage::runNextPendingTask()
{
    /* Avvia ogni task pending i cui depends_on sono tutti Done,
       fin quando il pool ha client disponibili. */
    for (int i = 0; i < m_tasks.size(); ++i) {
        if (m_tasks[i].state != SubTask::State::Pending) continue;

        bool depsOk = true;
        for (int dep : m_tasks[i].dependsOn) {
            bool depDone = false;
            for (const auto& t : m_tasks) {
                if (t.id == dep) {
                    depDone = (t.state == SubTask::State::Done);
                    break;
                }
            }
            if (!depDone) { depsOk = false; break; }
        }

        if (!depsOk) continue;

        AiClient* c = takePoolClient();
        if (!c) break;   /* pool esaurito — riprova quando un task finisce */
        runTask(i, c);
    }

    /* Verifica completamento: tutti i task conclusi e nessuno in esecuzione */
    if (m_tasks.isEmpty() || !m_runningTasks.isEmpty()) return;

    const bool allDone = std::all_of(m_tasks.begin(), m_tasks.end(),
        [](const SubTask& t) {
            return t.state == SubTask::State::Done || t.state == SubTask::State::Error;
        });

    if (allDone)
        synthesizeFinal();
}

/* ══════════════════════════════════════════════════════════════
   runTask — esegue un singolo sub-agente usando il client del pool
   ══════════════════════════════════════════════════════════════ */
void AgentiMultiPage::runTask(int idx, AiClient* client)
{
    if (idx < 0 || idx >= m_tasks.size() || !client) return;

    SubTask& t = m_tasks[idx];
    t.state = SubTask::State::Running;
    m_runningTasks.insert(idx);
    m_taskClients[idx] = client;
    updateTaskItem(idx);

    setStatus(QString("\xf0\x9f\xa4\x96  Sub-agente %1 (%2) in esecuzione...")
              .arg(t.id).arg(t.role));  /* 🤖 */

    appendOutput(
        QString("<hr><h4 style='color:#34d399'>\xf0\x9f\xa4\x96 Sub-agente %1 — %2</h4>")
        .arg(t.id).arg(t.role.toHtmlEscaped()));

    /* Costruisce il contesto dai risultati dei task precedenti */
    QString ctx;
    for (int dep : t.dependsOn) {
        for (const auto& prev : m_tasks) {
            if (prev.id == dep && !prev.result.isEmpty()) {
                ctx += "\n## Risultato Sub-agente " + QString::number(dep)
                     + " (" + prev.role + "):\n" + prev.result + "\n";
            }
        }
    }

    /* Legge contesto dalla GraphMemory */
    if (m_gm && m_gm->nodeCount() > 0) {
        const auto relevant = m_gm->searchNodes(t.prompt.left(60), 5);
        if (!relevant.isEmpty()) {
            ctx += "\n## Contesto dalla memoria condivisa:\n";
            for (const auto& n : relevant)
                ctx += "- [" + n.type + "] " + n.label + ": "
                     + n.content.left(150) + "\n";
        }
    }

    const QString sys =
        QString("Sei uno specialista con il ruolo di '%1'.\n"
                "Rispondi in modo dettagliato e strutturato.\n"
                "Se hai risultati di altri agenti usa il contesto fornito.\n"
                "Lingua: italiano.").arg(t.role);

    const QString user = ctx.isEmpty()
        ? t.prompt
        : t.prompt + "\n\n---\nContesto dai task precedenti:\n" + ctx;

    /* Holder one-shot per questo task — context object = holder, fonte = client pool */
    delete m_taskHolders.value(idx, nullptr);
    m_taskHolders[idx] = new QObject(this);

    connect(client, &AiClient::token, m_taskHolders[idx],
            [this, idx](const QString& tok) { onTaskResultToken(idx, tok); });

    connect(client, &AiClient::finished, m_taskHolders[idx],
            [this, idx](const QString& full) { onTaskResultDone(idx, full); });

    connect(client, &AiClient::error, m_taskHolders[idx],
            [this, idx](const QString& msg) { onTaskResultError(idx, msg); });

    client->chat(sys, user);
}

/* ══════════════════════════════════════════════════════════════
   onTaskResultToken/Done/Error
   ══════════════════════════════════════════════════════════════ */
void AgentiMultiPage::onTaskResultToken(int idx, const QString& tok)
{
    if (idx < 0 || idx >= m_tasks.size()) return;
    m_tasks[idx].result += tok;

    /* Streaming nel log */
    m_outputLog->moveCursor(QTextCursor::End);
    m_outputLog->insertPlainText(tok);
    m_outputLog->ensureCursorVisible();
}

void AgentiMultiPage::onTaskResultDone(int idx, const QString& full)
{
    delete m_taskHolders.value(idx, nullptr);
    m_taskHolders.remove(idx);

    /* Restituisce il client al pool */
    returnPoolClient(m_taskClients.take(idx));
    m_runningTasks.remove(idx);

    if (idx < 0 || idx >= m_tasks.size()) return;
    SubTask& t = m_tasks[idx];
    t.state  = SubTask::State::Done;
    /* Salva il testo senza thinking tokens — il risultato visibile all'utente
     * e quello memorizzato in GraphMemory non devono contenere <think>. */
    t.result = stripThinkTags(full);

    /* Salva risultato in GraphMemory locale (Multi-Agente) */
    if (m_gm) {
        const QString nodeId = m_gm->addNode(
            "result",
            QString("SubAgent-%1 (%2)").arg(t.id).arg(t.role),
            t.result.left(800),
            0.9f,
            {{"role", t.role}, {"task_id", t.id}}
        );
        t.resultNodeId = nodeId;

        /* Collega al nodo task principale se esiste */
        const auto mainNode = m_gm->searchNodes(m_promptInput->toPlainText().left(60), 1);
        if (!mainNode.isEmpty())
            m_gm->addEdge(nodeId, mainNode.first().id, "task_of");
    }

    /* Cross-pollination: scrive anche nella GraphMemory del RagGraph */
    if (m_extRagGm) {
        m_extRagGm->addNode(
            "fact",
            QString("Agente-%1/%2").arg(t.id).arg(t.role),
            full.left(600),
            0.75f,
            {{"source", "multi_agent"}, {"role", t.role}}
        );
    }

    updateTaskItem(idx);
    appendOutput("<p style='color:#6ee7b7'>&#10003; Sub-agente " +
                 QString::number(t.id) + " completato.</p>");

    setStatus(QString("\xe2\x9c\x85  Sub-agente %1 completato.").arg(t.id));

    /* Avanza al prossimo task */
    runNextPendingTask();
}

void AgentiMultiPage::onTaskResultError(int idx, const QString& msg)
{
    delete m_taskHolders.value(idx, nullptr);
    m_taskHolders.remove(idx);

    returnPoolClient(m_taskClients.take(idx));
    m_runningTasks.remove(idx);

    if (idx < 0 || idx >= m_tasks.size()) return;
    m_tasks[idx].state = SubTask::State::Error;
    updateTaskItem(idx);
    appendOutput("<p style='color:#f87171'>\xe2\x9d\x8c Sub-agente "
                 + QString::number(m_tasks[idx].id) + " errore: " + msg + "</p>");

    runNextPendingTask();
}

/* ══════════════════════════════════════════════════════════════
   synthesizeFinal — agente di sintesi finale
   ══════════════════════════════════════════════════════════════ */
void AgentiMultiPage::synthesizeFinal()
{
    if (m_synthBusy || !m_ai) return;
    m_synthBusy = true;
    setStatus("\xf0\x9f\x93\x9d  Sintesi finale...");
    appendOutput("<hr><h3 style='color:#c084fc'>\xf0\x9f\x93\x9d Sintesi Finale</h3>");

    /* Raccoglie tutti i risultati */
    QString allResults;
    for (const auto& t : m_tasks) {
        if (t.state != SubTask::State::Done) continue;
        allResults += "## " + t.role + " (Task " + QString::number(t.id) + ")\n"
                   + t.result + "\n\n---\n\n";
    }

    /* Estrae anche snapshot GraphMemory come contesto */
    QString graphCtx;
    if (m_gm && m_gm->nodeCount() > 0) {
        m_gm->exportTxt("/tmp/gm_snapshot.txt", 30);
        graphCtx = m_gm->toJson(30);
    }

    const QString sys =
        "Sei un sintetizzatore finale. Hai ricevuto i risultati di diversi sub-agenti "
        "specializzati. Il tuo compito e':\n"
        "1. Integrare tutti i contributi in modo coerente\n"
        "2. Eliminare ridondanze\n"
        "3. Produrre un documento finale ben strutturato in italiano\n"
        "4. Includere una sezione 'Conclusioni' e una 'Prossimi passi'";

    const QString user =
        "Sintetizza questi risultati in un documento finale:\n\n"
        + allResults
        + (graphCtx.isEmpty() ? QString() : "\n\nContesto memoria grafo:\n" + graphCtx.left(500));

    delete m_synthHolder;
    m_synthHolder = new QObject(this);

    connect(m_ai, &AiClient::token, m_synthHolder,
            [this](const QString& tok) {
                m_outputLog->moveCursor(QTextCursor::End);
                m_outputLog->insertPlainText(tok);
                m_outputLog->ensureCursorVisible();
            });

    connect(m_ai, &AiClient::finished, m_synthHolder,
            [this](const QString& full) {
                delete m_synthHolder;
                m_synthHolder = nullptr;
                m_synthBusy   = false;
                m_btnDecompose->setEnabled(true);
                m_btnStop->setEnabled(false);
                setStatus("\xe2\x9c\x85  Elaborazione completata.");

                /* Salva sintesi in GraphMemory */
                if (m_gm)
                    m_gm->addNode("result", "Sintesi Finale", full.left(1000), 1.0f,
                                  {{"source", "synthesizer"}});

                appendOutput("<p style='color:#a78bfa'><b>Sintesi completata.</b></p>");
            });

    connect(m_ai, &AiClient::error, m_synthHolder,
            [this](const QString& msg) {
                delete m_synthHolder;
                m_synthHolder = nullptr;
                m_synthBusy   = false;
                m_btnDecompose->setEnabled(true);
                m_btnStop->setEnabled(false);
                setStatus("\xe2\x9d\x8c  Errore sintesi: " + msg.left(80));
            });

    m_ai->chat(sys, user);
}

/* ══════════════════════════════════════════════════════════════
   UI helpers
   ══════════════════════════════════════════════════════════════ */
void AgentiMultiPage::addTaskItem(const SubTask& t)
{
    auto* item = new QListWidgetItem(m_taskList);
    item->setData(Qt::UserRole, t.id);
    /* Trova l'indice del task per id */
    for (int i = 0; i < m_tasks.size(); ++i) {
        if (m_tasks[i].id == t.id) { updateTaskItem(i); break; }
    }
}

void AgentiMultiPage::updateTaskItem(int idx)
{
    if (idx < 0 || idx >= m_tasks.size() || !m_taskList) return;
    const SubTask& t = m_tasks[idx];

    QListWidgetItem* item = nullptr;
    for (int i = 0; i < m_taskList->count(); ++i) {
        if (m_taskList->item(i)->data(Qt::UserRole).toInt() == t.id) {
            item = m_taskList->item(i);
            break;
        }
    }
    if (!item) return;

    QString icon;
    switch (t.state) {
        case SubTask::State::Pending: icon = "\xf0\x9f\x95\x90"; break;  /* 🕐 */
        case SubTask::State::Running: icon = "\xf0\x9f\x94\x84"; break;  /* 🔄 */
        case SubTask::State::Done:    icon = "\xe2\x9c\x85"; break;       /* ✅ */
        case SubTask::State::Error:   icon = "\xe2\x9d\x8c"; break;       /* ❌ */
    }
    item->setText(QString("%1 #%2 %3").arg(icon).arg(t.id).arg(t.role));
}

void AgentiMultiPage::appendOutput(const QString& html)
{
    if (!m_outputLog) return;
    QString plain = html;
    plain.remove(QRegularExpression("<[^>]*>"));   /* strip HTML semplice */
    m_outputLog->appendPlainText(plain);
}

void AgentiMultiPage::clearOutput()
{
    if (m_outputLog)  m_outputLog->clear();
    if (m_dotView)    m_dotView->clear();
    if (m_jsonView)   m_jsonView->clear();
}

void AgentiMultiPage::setStatus(const QString& msg)
{
    if (m_status) m_status->setText(msg);
}

void AgentiMultiPage::refreshMemoryStats()
{
    if (!m_memStats || !m_gm) return;
    m_memStats->setText(
        QString("%1 nodi \xc2\xb7 %2 archi")   /* · */
        .arg(m_gm->nodeCount()).arg(m_gm->edgeCount()));
}

void AgentiMultiPage::refreshDot()
{
    if (!m_gm) return;
    if (m_dotView)  m_dotView->setPlainText(m_gm->toDot("GraphMemory multi-agente"));
    if (m_jsonView) m_jsonView->setPlainText(m_gm->toJson());
}

/* ══════════════════════════════════════════════════════════════
   Slots
   ══════════════════════════════════════════════════════════════ */
void AgentiMultiPage::onStopClicked()
{
    /* Disconnetti prima i holder, poi abortisci i client */
    delete m_decompHolder; m_decompHolder = nullptr;
    delete m_synthHolder;  m_synthHolder  = nullptr;
    for (auto* h : m_taskHolders) delete h;
    m_taskHolders.clear();

    if (m_ai) m_ai->abort();
    for (AiClient* c : m_aiPool) c->abort();
    m_busyClients.clear();
    m_taskClients.clear();
    m_runningTasks.clear();

    m_decomposeBusy = false;
    m_synthBusy     = false;
    m_btnDecompose->setEnabled(true);
    m_btnStop->setEnabled(false);
    setStatus("\xe2\x96\xa0  Interrotto.");
}

void AgentiMultiPage::onClearClicked()
{
    onStopClicked();
    m_tasks.clear();
    if (m_taskList) m_taskList->clear();
    clearOutput();
    setStatus("\xf0\x9f\x95\xb8  Pronto.");
}

void AgentiMultiPage::onExportTxtClicked()
{
    if (!m_gm) return;
    const QString path = QFileDialog::getSaveFileName(
        this, "Esporta memoria", QDir::homePath() + "/graph_memory.txt",
        "Testo (*.txt)");
    if (path.isEmpty()) return;
    m_gm->exportTxt(path);
}

void AgentiMultiPage::onClearMemClicked()
{
    if (!m_gm) return;
    m_gm->clearAll();
    refreshMemoryStats();
    if (m_dotView)  m_dotView->clear();
    if (m_jsonView) m_jsonView->clear();
    setStatus("\xf0\x9f\x97\x91  Memoria grafo svuotata.");
}

void AgentiMultiPage::onGraphMemoryChanged()
{
    refreshMemoryStats();
    /* Aggiorna viste solo se il tab grafo/json è visibile */
    if (m_outputTabs && m_outputTabs->currentIndex() > 0)
        refreshDot();
}

void AgentiMultiPage::onTaskItemClicked(QListWidgetItem* item)
{
    if (!item) return;
    const int taskId = item->data(Qt::UserRole).toInt();
    for (const auto& t : m_tasks) {
        if (t.id == taskId && !t.result.isEmpty()) {
            m_outputLog->setPlainText(t.result);
            return;
        }
    }
}

/* ══════════════════════════════════════════════════════════════
   Pool parallelo — initPool / takePoolClient / returnPoolClient
   ══════════════════════════════════════════════════════════════ */
void AgentiMultiPage::initPool()
{
    if (!m_ai) return;
    /* Crea i client mancanti */
    while (m_aiPool.size() < kMaxParallel)
        m_aiPool.append(new AiClient(this));
    /* Aggiorna le impostazioni di tutti i client con quelle correnti di m_ai */
    for (AiClient* c : m_aiPool)
        c->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), m_ai->model());
    m_busyClients.clear();
}

AiClient* AgentiMultiPage::takePoolClient()
{
    for (AiClient* c : m_aiPool) {
        if (!m_busyClients.contains(c)) {
            m_busyClients.insert(c);
            return c;
        }
    }
    return nullptr;  /* pool esaurito */
}

void AgentiMultiPage::returnPoolClient(AiClient* c)
{
    if (c) m_busyClients.remove(c);
}

void AgentiMultiPage::onFillModels()
{
    if (!m_ai) return;
    const QString cur = m_ai->model();
    m_modelCombo->clear();
    m_modelCombo->addItem(cur.isEmpty() ? "(caricamento...)" : cur, cur);

    auto* holder = new QObject(this);
    connect(m_ai, &AiClient::modelsReady, holder,
            [this, holder](const QStringList& list) {
                holder->deleteLater();
                const QString cur = m_ai->model();
                m_modelCombo->clear();
                for (const auto& m : list) {
                    m_modelCombo->addItem(P::modelIcon(0, m) + m, m);
                    if (m == cur)
                        m_modelCombo->setCurrentIndex(m_modelCombo->count() - 1);
                }
            });
    connect(m_ai, &AiClient::error, holder, [holder](const QString&) {
        holder->deleteLater();
    });
    m_ai->fetchModels();
}
