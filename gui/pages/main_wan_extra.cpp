/* ======================================================================
   main_wan_extra.cpp — WAN decompose, sim, eventFilter, tools di LanWanPage
   Estratto da main_lan_wan.cpp per ridurne le dimensioni.
   ====================================================================== */
#include "main_lan_wan.h"
#include "main_sci_compute.h"
#include "widget_ssh_manager.h"
#include "../dpi_utils.h"
#include "../lan_server.h"
#include "../prismalux_paths.h"
#include "../app_config.h"
#include "../widgets/qr_code_widget.h"
#include "../widgets/model_combo_box.h"
#include "../widgets/proc_helper.h"
#include "../log_bus.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QNetworkInterface>
#include <QHostAddress>
#include <QDialog>
#include <QMessageBox>
#include <QApplication>
#include <QClipboard>
#include <QUuid>
#include <QFrame>
#include <QScrollArea>
#include <QLineEdit>
#include <QTextEdit>
#include <QComboBox>
#include <QProcess>
#include <QTcpSocket>
#include <QTcpServer>
#if QT_CONFIG(ssl)
#  include <QSslServer>
#  include <QSslSocket>
#  include <QSslConfiguration>
#  include <QSslCertificate>
#  include <QSslKey>
#endif
#include <QMessageAuthenticationCode>
#include <QTimer>
#include <QPointer>
#include <QDesktopServices>
#include <QUrl>
#include <QFile>
#include <QDir>
#include <QTextCursor>
#include <QHeaderView>
#include <QDateTime>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStackedWidget>
#include <QFormLayout>
#include <QFileDialog>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QRandomGenerator>
#include <QRadioButton>
#include <QButtonGroup>
#include <QSplitter>
#include <QStandardItemModel>
#include <QSysInfo>
#include <QTextStream>
#include <QPainter>
#include <QPixmap>
#ifdef HAVE_QT_SQL
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#endif
namespace P = PrismaluxPaths;

void LanWanPage::onWanDecomposeBtnClicked()
{
    const QString userTask = m_wanDecomposeInput
        ? m_wanDecomposeInput->toPlainText().trimmed() : QString();
    if (userTask.isEmpty() || !m_ai) return;

    m_wanDecomposeBtn->setEnabled(false);
    if (m_wanDecomposeStatusLbl)
        m_wanDecomposeStatusLbl->setText(
            tr("\xe2\x8f\xb3  MasterAgent in elaborazione\xe2\x80\xa6"));

    /* Stesso system prompt di AgentiMultiPage::decompose() */
    const QString sys =
        "Sei un orchestratore di agenti AI. Il tuo compito e' scomporre un problema"
        " complesso in sotto-task specializzati, ognuno assegnabile a un sub-agente diverso.\n\n"
        "REGOLE:\n"
        "- Rispondi SOLO con JSON valido, nessun testo fuori dal JSON.\n"
        "- Massimo 5 sub-task.\n"
        "- Ogni prompt deve essere autonomo e completo (non fare riferimento ad altri agenti).\n"
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

    delete m_wanDecompHolder;
    m_wanDecompHolder = new QObject(this);

    connect(m_ai, &AiClient::finished, m_wanDecompHolder,
            [this](const QString& full) {
                delete m_wanDecompHolder;
                m_wanDecompHolder = nullptr;
                wanApplyDecomposedPlan(full);
            });
    connect(m_ai, &AiClient::error, m_wanDecompHolder,
            [this](const QString& msg) {
                delete m_wanDecompHolder;
                m_wanDecompHolder = nullptr;
                m_wanDecomposeBtn->setEnabled(true);
                if (m_wanDecomposeStatusLbl)
                    m_wanDecomposeStatusLbl->setText(
                        tr("\xe2\x9d\x8c  Errore: ") + msg.left(80));
                LogBus::post("\xe2\x9d\x8c LAN WAN: WAN decomposizione errore AI: " + msg.left(80));
            });

    m_ai->chat(sys, "Compito da scomporre:\n" + userTask);
}

/* ══════════════════════════════════════════════════════════════════════════
   wanApplyDecomposedPlan — parsifica il JSON del MasterAgent e crea i task
   ══════════════════════════════════════════════════════════════════════════ */
void LanWanPage::wanApplyDecomposedPlan(const QString& jsonPlan)
{
    m_wanDecomposeBtn->setEnabled(true);

    /* Rimuove <think>...</think> dei modelli reasoning */
    static const QRegularExpression reThink(
        "<think>[\\s\\S]*?</think>",
        QRegularExpression::CaseInsensitiveOption);
    QString clean = jsonPlan.trimmed();
    clean.remove(reThink);
    clean = clean.trimmed();

    /* Rimuove code fence markdown */
    static const QRegularExpression reFence(R"(```[a-z]*\n?([\s\S]*?)```)");
    const auto fence = reFence.match(clean);
    if (fence.hasMatch()) clean = fence.captured(1).trimmed();

    /* Estrai blocco JSON */
    static const QRegularExpression reJson(R"(\{[\s\S]*\})");
    const auto match = reJson.match(clean);
    if (match.hasMatch()) clean = match.captured(0);

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(clean.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        if (m_wanDecomposeStatusLbl)
            m_wanDecomposeStatusLbl->setText(
                "\xe2\x9a\xa0\xef\xb8\x8f  Piano non JSON \xe2\x80\x94"
                " riprova o usa un modello diverso");
        return;
    }

    const QJsonObject root     = doc.object();
    const QString     mainTask = root["task"].toString();
    const QJsonArray  subtasks = root["subtasks"].toArray();

    if (subtasks.isEmpty()) {
        if (m_wanDecomposeStatusLbl)
            m_wanDecomposeStatusLbl->setText(
                tr("\xe2\x9a\xa0\xef\xb8\x8f  Nessun subtask nel piano \xe2\x80\x94 riprova"));
        return;
    }

    /* Crea un WanTask llm_agent per ogni subtask */
    int created = 0;
    for (const QJsonValue& v : subtasks) {
        const QJsonObject st     = v.toObject();
        const QString     role   = st["role"].toString();
        const QString     prompt = st["prompt"].toString();
        if (role.isEmpty() || prompt.isEmpty()) continue;

        QJsonObject agentPayload;
        agentPayload["role"]     = role;
        agentPayload["prompt"]   = prompt;
        agentPayload["context"]  = "";
        agentPayload["depth"]    = 0;
        agentPayload["chain_id"] = mainTask.left(40);

        WanTask t;
        t.id      = wanNextId();
        t.kind    = "llm_agent";
        t.payload = QString::fromUtf8(
            QJsonDocument(agentPayload).toJson(QJsonDocument::Compact));
        t.status  = "pending";
        t.node    = QString("piano: %1").arg(mainTask.left(30));
        t.created = QDateTime::currentDateTime();
        m_wanTasks.push_back(t);
        created++;
    }

    wanRefreshTables();
    wanDispatch();

    if (m_wanDecomposeStatusLbl)
        m_wanDecomposeStatusLbl->setText(
            QString("\xe2\x9c\x85  %1 agenti aggiunti alla coda \xe2\x80\x94 \"%2\"")
            .arg(created).arg(mainTask.left(50)));
}

/* ══════════════════════════════════════════════════════════════════════════
   onWanSimBtnClicked — avvia/ferma simulazione locale (server + nodo virtuale)

   Permette di testare WAN Compute su un solo PC senza altri nodi fisici.
   Il server TCP ascolta su 127.0.0.1:porta; il client (nodo virtuale) si
   connette allo stesso indirizzo e riceve/esegue i task normalmente.
   Il risultato appare nella tabella "Coda task" come se venisse da un nodo
   esterno — l'esperienza è identica a quella in produzione.
   ══════════════════════════════════════════════════════════════════════════ */
void LanWanPage::onWanSimBtnClicked()
{
    if (m_wanSimActive) {
        /* ── Ferma simulazione ── */
        if (m_wanCliPollTimer) m_wanCliPollTimer->stop();
        if (m_wanCliSock)      m_wanCliSock->disconnectFromHost();
        m_wanSimActive = false;
        if (m_wanSimBtn)
            m_wanSimBtn->setText(tr("\xe2\x9a\x97\xef\xb8\x8f  Prova in locale"));
        if (m_wanSrvStatusLbl)
            m_wanSrvStatusLbl->setStyleSheet("color:gray;");
        wanCliAppendLog("Simulazione locale fermata.");
        return;
    }

    /* ── Avvia simulazione ──
     * 1. Avvia il server se non è già in ascolto. */
    if (!m_wanServer || !m_wanServer->isListening()) {
        if (m_wanStartBtn) m_wanStartBtn->click();
        QCoreApplication::processEvents();   // lascia partire il server
    }
    if (!m_wanServer || !m_wanServer->isListening()) {
        if (m_wanSrvStatusLbl)
            m_wanSrvStatusLbl->setText(tr("\xe2\x9d\x8c  Impossibile avviare il server"));
        LogBus::post("\xe2\x9d\x8c LAN WAN: Impossibile avviare il server WAN.");
        return;
    }

    /* 2. Imposta il client per connettersi a localhost con nome "Nodo locale" */
    if (m_wanCliHost) m_wanCliHost->setText(tr("127.0.0.1"));
    if (m_wanCliPort) m_wanCliPort->setValue(m_wanPortSpin ? m_wanPortSpin->value()
                                                            : P::kWanComputePort);
    if (m_wanCliName) m_wanCliName->setText(tr("Nodo locale (simulazione)"));

    /* 3. Avvia la connessione client riusando lo slot esistente
     *    (non cambia il pannello visibile — rimane la vista server) */
    onWanCliConBtnClicked();

    m_wanSimActive = true;
    if (m_wanSimBtn)
        m_wanSimBtn->setText(tr("\xe2\x8f\xb9  Ferma simulazione"));
    if (m_wanSrvStatusLbl)
        m_wanSrvStatusLbl->setStyleSheet("color:#22c55e; font-weight:bold;");
    wanCliAppendLog("Simulazione locale avviata — nodo virtuale connesso a 127.0.0.1.");
}

/* ══════════════════════════════════════════════════════════════════════════
   eventFilter — drag-and-drop di file .json sul form llm_agent
   ══════════════════════════════════════════════════════════════════════════ */
bool LanWanPage::eventFilter(QObject* obj, QEvent* e)
{
    if (obj != m_agentFormFrame) return QWidget::eventFilter(obj, e);

    if (e->type() == QEvent::DragEnter) {
        auto* de = static_cast<QDragEnterEvent*>(e);
        if (de->mimeData()->hasUrls()) {
            for (const auto& url : de->mimeData()->urls()) {
                if (url.toLocalFile().endsWith(".json", Qt::CaseInsensitive)) {
                    de->acceptProposedAction();
                    m_agentFormFrame->setStyleSheet("QFrame{border:2px dashed #818cf8;}");
                    return true;
                }
            }
        }

    } else if (e->type() == QEvent::DragLeave) {
        m_agentFormFrame->setStyleSheet("");
        return true;

    } else if (e->type() == QEvent::Drop) {
        m_agentFormFrame->setStyleSheet("");
        auto* de = static_cast<QDropEvent*>(e);
        for (const auto& url : de->mimeData()->urls()) {
            const QString path = url.toLocalFile();
            if (!path.endsWith(".json", Qt::CaseInsensitive)) continue;
            QFile f(path);
            if (!f.open(QIODevice::ReadOnly)) continue;
            const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
            if (!doc.isObject()) continue;
            const QJsonObject o = doc.object();
            if (m_agentRoleEdit)    m_agentRoleEdit->setText(o["role"].toString());
            if (m_agentPromptEdit)  m_agentPromptEdit->setPlainText(o["prompt"].toString());
            if (m_agentContextEdit) m_agentContextEdit->setPlainText(o["context"].toString());
            break;
        }
        return true;
    }
    return QWidget::eventFilter(obj, e);
}

/* ══════════════════════════════════════════════════════════════════════════
   onAgentSaveBtnClicked — salva i campi del form come file .json
   ══════════════════════════════════════════════════════════════════════════ */
void LanWanPage::onAgentSaveBtnClicked()
{
    const QString role   = m_agentRoleEdit   ? m_agentRoleEdit->text().trimmed()          : QString();
    const QString prompt = m_agentPromptEdit ? m_agentPromptEdit->toPlainText().trimmed()  : QString();
    if (role.isEmpty() || prompt.isEmpty()) return;

    QJsonObject obj;
    obj["role"]     = role;
    obj["prompt"]   = prompt;
    obj["context"]  = m_agentContextEdit ? m_agentContextEdit->toPlainText().trimmed() : QString();
    obj["depth"]    = 0;
    obj["chain_id"] = "";

    const QString defaultName = role.left(30).replace(QRegularExpression("[^\\w\\s]"), "")
                                             .replace(' ', '_') + ".json";
    const QString path = QFileDialog::getSaveFileName(
        this, "Salva Agente LLM",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/" + defaultName,
        "JSON (*.json)");
    if (path.isEmpty()) return;

    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text))
        f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
}

/* ══════════════════════════════════════════════════════════════════════════
   onAgentLoadBtnClicked — carica un file .json nel form
   ══════════════════════════════════════════════════════════════════════════ */
void LanWanPage::onAgentLoadBtnClicked()
{
    const QString path = QFileDialog::getOpenFileName(
        this, "Carica Agente LLM",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        "JSON (*.json)");
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return;
    const QJsonObject o = doc.object();
    if (m_agentRoleEdit)    m_agentRoleEdit->setText(o["role"].toString());
    if (m_agentPromptEdit)  m_agentPromptEdit->setPlainText(o["prompt"].toString());
    if (m_agentContextEdit) m_agentContextEdit->setPlainText(o["context"].toString());
}

/* ══════════════════════════════════════════════════════════════════════════
   wanPopulateKindCombo — riempie il combo con tutti i tipi di task
   organizzati per categoria (separatori non selezionabili).
   ══════════════════════════════════════════════════════════════════════════ */
void LanWanPage::wanPopulateKindCombo(QComboBox* combo)
{
    if (!combo) return;
    combo->clear();
    auto* model = new QStandardItemModel(combo);

    auto addSep = [&](const QString& label) {
        auto* sep = new QStandardItem(label);
        sep->setFlags(Qt::NoItemFlags);
        sep->setData(QColor(Qt::gray), Qt::ForegroundRole);
        auto f = sep->font(); f.setBold(true); sep->setFont(f);
        model->appendRow(sep);
    };
    auto addItem = [&](const QString& label, const QString& data) {
        auto* item = new QStandardItem(label);
        item->setData(data, Qt::UserRole);
        model->appendRow(item);
    };

    // ── 🤖 AI & LLM ──
    addSep("── 🤖  AI & LLM ──────────────");
    addItem("\xf0\x9f\xa4\x96  Agente LLM (ruolo + prompt + spawn)",  "llm_agent");
    addItem("💬  Query AI generica",                  "ai_query");
    addItem("💻  Assistente codice",                  "code_assist");
    addItem("🔍  Revisione codice (code review)",     "code_review");
    addItem("🔀  Converti Codice (C\xe2\x86\x92" "Python, Py\xe2\x86\x92" "JS\xe2\x80\xa6)", "code_translate");
    addItem("🔍  Reverse Engineering",                "code_reverse");
    addItem("📐  Risolvi passo per passo (Matematica)","math_solve");
    addItem("🔢  Formula da sequenza (Matematica)",   "math_seq");
    addItem("📄  Genera paper scientifico",           "paper_gen");
    addItem("🌐  Ricerca web / web search",           "web_search");
    addItem("🎓  Tutoring su argomento",              "ai_tutor");
    addItem("📊  Analisi dati CSV/JSON",              "ai_data_analysis");
    addItem("🔬  Analisi fenomeno scientifico",       "ai_fenomeno");
    addItem("⚖️  Assistente 730 / fiscale",           "ai_730");
    addItem("💼  Assistente TFR / previdenziale",     "ai_tfr");
    // ── 💻 Codice & Shell ──
    addSep("── 💻  Codice & Shell ─────────");
    addItem("🐍  Python REPL (esegui snippet)",       "python_repl");
    addItem("📜  Script Python (file completo)",      "eval_script");
    addItem("🖥️  Comando shell (bash -c)",            "shell_cmd");
    addItem("🔧  Comando Git",                        "git_cmd");
    // ── 📐 Matematica ──
    addSep("── 📐  Matematica ─────────────");
    addItem("🧮  Valuta espressione (Python eval)",   "math_expr");
    addItem("➗  N-esimo termine sequenza",           "math_nth");
    // ── 📁 File & Sistema ──
    addSep("── 📁  File & Sistema ─────────");
    addItem("📂  Leggi file locale",                  "file_read");
    addItem("✏️  Scrivi file locale",                  "file_write");
    addItem("🖥️  Informazioni sistema (CPU/RAM/OS)",  "system_info");
    addItem("🌐  Interfacce di rete",                 "net_info");
    // ── 🎨 Grafica & Visualizzazione ──
    addSep("── 🎨  Grafica & Visualizzazione ─");
    addItem("🔗  Renderizza grafo Graphviz (DOT)",    "graphviz_render");
    addItem("📈  Genera grafico matplotlib (Python)", "matplotlib_plot");

    combo->setModel(model);
    // Seleziona il primo item valido (dopo il primo separatore)
    combo->setCurrentIndex(1);
}

/* ══════════════════════════════════════════════════════════════════════════
   wanKindTemplate — restituisce il template payload per ogni tipo di task
   ══════════════════════════════════════════════════════════════════════════ */
QString LanWanPage::wanKindTemplate(const QString& kind) const
{
    static const QMap<QString, QString> kTemplates {
        {"llm_agent",
            "{\n"
            "  \"role\": \"Ricercatore specializzato in fisica quantistica\",\n"
            "  \"prompt\": \"Analizza il fenomeno dell'entanglement e identifica le sue applicazioni pratiche emergenti.\",\n"
            "  \"context\": \"\",\n"
            "  \"depth\": 0,\n"
            "  \"chain_id\": \"\"\n"
            "}"},
        {"ai_query",        "Qual è la capitale della Francia?"},
        {"code_assist",     "{\n  \"lang\": \"Python\",\n  \"task\": \"Scrivi una funzione che calcola i numeri di Fibonacci fino a N\"\n}"},
        {"code_review",     "{\n  \"lang\": \"Python\",\n  \"code\": \"def somma(a, b):\\n    return a + b\"\n}"},
        {"code_translate",  "{\n  \"from_lang\": \"Python\",\n  \"to_lang\": \"C++\",\n  \"code\": \"def hello():\\n    print('Hello World')\"\n}"},
        {"code_reverse",    "{\n  \"lang\": \"Auto-rileva\",\n  \"code\": \"// incolla qui il codice decompilato o assembly\"\n}"},
        {"math_solve",      "Risolvi per x: x^2 - 5x + 6 = 0"},
        {"math_seq",        "1, 1, 2, 3, 5, 8, 13, 21"},
        {"paper_gen",       "{\n  \"titolo\": \"Applicazioni del Calcolo Distribuito nella Ricerca Scientifica\",\n  \"sezioni\": \"Introduzione, Metodi, Risultati, Conclusioni\"\n}"},
        {"web_search",      "intelligenza artificiale distribuita BOINC"},
        {"ai_tutor",        "{\n  \"argomento\": \"Algoritmi di ordinamento\",\n  \"livello\": \"universitario\"\n}"},
        {"ai_data_analysis","{\n  \"dati\": \"anno,valore\\n2020,100\\n2021,120\\n2022,115\\n2023,140\",\n  \"richiesta\": \"Trova trend e anomalie\"\n}"},
        {"ai_fenomeno",     "{\n  \"categoria\": \"Fisico\",\n  \"evento\": \"Descrizione del fenomeno osservato\",\n  \"fonti\": \"URL o testo delle fonti\"\n}"},
        {"ai_730",          "Posso detrarre le spese mediche per mio figlio a carico?"},
        {"ai_tfr",          "Ho lavorato 15 anni con stipendio lordo 35000€/anno. Quanto TFR ho maturato?"},
        {"python_repl",     "import math\nprint(f'π = {math.pi:.10f}')\nprint(f'e = {math.e:.10f}')"},
        {"eval_script",     "import sys, platform\nprint('Python', sys.version)\nprint('OS:', platform.system(), platform.release())"},
        {"shell_cmd",       "df -h && free -h && uptime"},
        {"git_cmd",         "git log --oneline -10"},
        {"math_expr",       "2**10 + sum(range(1, 101))"},
        {"math_nth",        "{\n  \"sequenza\": \"Fibonacci\",\n  \"n\": 20\n}"},
        {"file_read",       "/etc/os-release"},
        {"file_write",      "{\n  \"path\": \"/tmp/wan_output.txt\",\n  \"content\": \"Scritto da WAN Compute node\"\n}"},
        {"system_info",     ""},
        {"net_info",        ""},
        {"graphviz_render", "digraph G {\n  rankdir=LR;\n  Server -> Client1;\n  Server -> Client2;\n  Server -> Client3;\n}"},
        {"matplotlib_plot", "import matplotlib\nmatplotlib.use('Agg')\nimport matplotlib.pyplot as plt\nimport numpy as np\nx = np.linspace(0, 2*np.pi, 100)\nplt.plot(x, np.sin(x))\nplt.title('Seno')\nplt.savefig('/tmp/wan_plot.png')\nprint('Salvato /tmp/wan_plot.png')"},
    };
    return kTemplates.value(kind, "");
}

/* ══════════════════════════════════════════════════════════════════════════
   wanCliHandleTask — dispatcher universale lato client
   Mappa ogni kind → tool locale (AI, subprocess, QNetworkInterface, ecc.)
   ══════════════════════════════════════════════════════════════════════════ */
void LanWanPage::wanCliHandleTask(const QString& id, const QString& kind, const QString& payload)
{
    /* ── Mappa kind → system prompt AI ── */
    static const QHash<QString, QString> kAiPrompts {
        {"ai_query",
            "Sei un assistente AI preciso e conciso. Rispondi SEMPRE in italiano."},
        {"code_assist",
            "Sei un esperto programmatore. Scrivi codice pulito, commentato e funzionante. "
            "Il payload è JSON con chiavi 'lang' e 'task'. Rispondi in italiano."},
        {"code_review",
            "Sei un esperto revisore di codice. Analizza il codice fornito: "
            "trova bug, vulnerabilità, inefficienze e suggerisci miglioramenti. "
            "Il payload può essere JSON con 'lang' e 'code', oppure testo diretto. "
            "Rispondi in italiano con sezioni: Bug, Sicurezza, Performance, Stile."},
        {"code_translate",
            "Sei un esperto di traduzione tra linguaggi di programmazione. "
            "Il payload è JSON con 'from_lang', 'to_lang' e 'code'. "
            "Traduci il codice preservando la logica. Rispondi solo con il codice tradotto + breve spiegazione."},
        {"code_reverse",
            "Sei un esperto di reverse engineering e analisi del codice. "
            "Il payload è JSON con 'lang' e 'code' (decompilato/assembly). "
            "Spiega la logica, ricostruisci il codice sorgente probabile, identifica pattern. "
            "Rispondi in italiano."},
        {"math_solve",
            "Sei un professore di matematica. Risolvi il problema passo per passo, "
            "mostrando ogni passaggio con spiegazione. Usa notazione LaTeX se utile. "
            "Rispondi in italiano."},
        {"math_seq",
            "Sei un matematico esperto. Data la sequenza di numeri, "
            "trova la formula generale (chiusa o ricorsiva), il termine n-esimo e la natura della sequenza. "
            "Rispondi in italiano."},
        {"math_nth",
            "Sei un matematico. Il payload è JSON con 'sequenza' (nome, es: Fibonacci) e 'n'. "
            "Calcola l'n-esimo termine e spiega il metodo. Rispondi in italiano."},
        {"paper_gen",
            "Sei un ricercatore accademico. Genera un paper scientifico completo e strutturato. "
            "Il payload può essere JSON con 'titolo' e 'sezioni', oppure solo il titolo. "
            "Struttura: Abstract, Introduzione, Metodi, Risultati, Discussione, Conclusioni, Riferimenti. "
            "Rispondi in italiano con formattazione Markdown."},
        {"web_search",
            "Sei un assistente di ricerca. Dato il termine di ricerca, "
            "fornisci un riassunto esaustivo delle informazioni principali sull'argomento, "
            "come se avessi consultato le fonti più autorevoli. Rispondi in italiano."},
        {"ai_tutor",
            "Sei un tutor esperto. Il payload è JSON con 'argomento' e 'livello'. "
            "Spiega l'argomento in modo chiaro e progressivo per il livello indicato. "
            "Includi esempi pratici ed esercizi. Rispondi in italiano."},
        {"ai_data_analysis",
            "Sei un analista dati. Il payload è JSON con 'dati' (CSV o testo) e 'richiesta'. "
            "Analizza i dati, trova pattern, trend e anomalie. "
            "Rispondi con tabelle e grafici ASCII se utile. Rispondi in italiano."},
        {"ai_fenomeno",
            "Sei un analista scientifico critico. Il payload è JSON con 'categoria', 'evento' e 'fonti'. "
            "Valuta la probabilità che il fenomeno sia realmente accaduto. "
            "Struttura: PROBABILITÀ (0-100%), Evidenze, Elementi contraddittori, Spiegazione scientifica, Verdetto. "
            "Rispondi in italiano."},
        {"ai_730",
            "Sei un esperto fiscalista italiano specializzato nel modello 730. "
            "Fornisci guide chiare, cita gli articoli di legge rilevanti. Rispondi SOLO in italiano."},
        {"ai_tfr",
            "Sei un esperto di diritto del lavoro e previdenza. "
            "Calcola e spiega il TFR (art. 2120 c.c.) in base ai dati forniti. "
            "Mostra: quota annua, rivalutazione, totale lordo, tassazione separata. Rispondi in italiano."},
    };

    /* ── llm_agent: agente con ruolo personalizzato + supporto spawn ── */
    if (kind == "llm_agent") {
        // Parsing payload JSON
        QString role    = "Assistente AI";
        QString prompt  = payload;
        QString context;
        int     depth   = 0;
        QString chainId;

        QJsonParseError jerr;
        const QJsonDocument doc = QJsonDocument::fromJson(payload.toUtf8(), &jerr);
        if (doc.isObject()) {
            const QJsonObject obj = doc.object();
            role    = obj["role"].toString(role);
            prompt  = obj["prompt"].toString(payload);
            context = obj["context"].toString();
            depth   = obj["depth"].toInt(0);
            chainId = obj["chain_id"].toString();
        }
        if (chainId.isEmpty()) chainId = id;   // primo agente della catena

        // Messaggio utente: contesto agente precedente + compito
        QString userMsg = prompt;
        if (!context.isEmpty())
            userMsg = "=== CONTESTO DALL'AGENTE PRECEDENTE ===\n" + context
                    + "\n\n=== IL TUO COMPITO ===\n" + prompt;

        // System prompt: ruolo + istruzioni spawn (solo se non al limite di profondità)
        constexpr int kMaxDepth = 4;
        QString sysPrompt = QString(
            "Sei: %1\n\n"
            "Esegui il compito con precisione. Rispondi in italiano.\n"
        ).arg(role);

        if (depth < kMaxDepth) {
            sysPrompt +=
                "\nSe il tuo risultato richiede analisi specialistiche aggiuntive "
                "da parte di altri agenti, puoi richiederle rispondendo in JSON:\n"
                "{\n"
                "  \"result\": \"<il tuo risultato completo qui>\",\n"
                "  \"spawn\": [\n"
                "    {\"role\": \"<ruolo agente>\", \"prompt\": \"<compito specifico>\"}\n"
                "  ]\n"
                "}\n"
                "Se NON hai bisogno di altri agenti, rispondi in testo libero normale "
                "(senza JSON). Non inventare JSON se non serve davvero.\n";
        }

        // Salva metadati per onWanCliAiFinished
        m_wanCliIsAgentTask  = true;
        m_wanCliAgentDepth   = depth;
        m_wanCliAgentChain   = chainId;

        m_wanCliAiActive = true;
        m_wanCliAiBuf.clear();
        QObject::disconnect(m_wanCliTokenConn);
        QObject::disconnect(m_wanCliFinishedConn);
        QObject::disconnect(m_wanCliErrorConn);
        m_wanCliTokenConn    = connect(m_ai, &AiClient::token,
                                       this, &LanWanPage::onWanCliAiToken);
        m_wanCliFinishedConn = connect(m_ai, &AiClient::finished,
                                       this, &LanWanPage::onWanCliAiFinished);
        m_wanCliErrorConn    = connect(m_ai, &AiClient::error,
                                       this, &LanWanPage::onWanCliAiError);
        wanCliAppendLog(QString("Agente [depth=%1] ruolo: %2").arg(depth).arg(role));
        m_ai->chat(sysPrompt, userMsg);
        return;
    }

    /* ── Task AI: delega a m_ai->chat() ── */
    if (kAiPrompts.contains(kind)) {
        m_wanCliAiActive = true;
        m_wanCliAiBuf.clear();
        QObject::disconnect(m_wanCliTokenConn);
        QObject::disconnect(m_wanCliFinishedConn);
        QObject::disconnect(m_wanCliErrorConn);
        m_wanCliTokenConn    = connect(m_ai, &AiClient::token,
                                       this, &LanWanPage::onWanCliAiToken);
        m_wanCliFinishedConn = connect(m_ai, &AiClient::finished,
                                       this, &LanWanPage::onWanCliAiFinished);
        m_wanCliErrorConn    = connect(m_ai, &AiClient::error,
                                       this, &LanWanPage::onWanCliAiError);
        m_ai->chat(kAiPrompts[kind], payload);
        return;
    }

    /* ── Task sincroni (subprocess / Qt API) ── */
    QString result;
    QString status = "done";

    /* Esecuzione codice/shell: richiede consenso esplicito utente (opt-in checkbox) */
    const bool shellAllowed = m_wanCliShellCheck && m_wanCliShellCheck->isChecked();

    if (kind == "python_repl" || kind == "eval_script") {
        if (!shellAllowed) {
            result = "[SICUREZZA] Esecuzione Python disabilitata. "
                     "Abilita 'Permetti shell' nelle opzioni nodo WAN.";
            status = "error";
        } else {
            const auto r = ProcHelper::run(P::findPython(), {"-c", payload}, 30000);
            result = r.out + r.err;
        }

    } else if (kind == "shell_cmd" || kind == "git_cmd") {
        if (!shellAllowed) {
            result = "[SICUREZZA] Esecuzione shell disabilitata. "
                     "Abilita 'Permetti shell' nelle opzioni nodo WAN.";
            status = "error";
        } else {
            const auto r = ProcHelper::run("bash", {"-c", payload}, 20000);
            result = r.out + r.err;
        }

    } else if (kind == "math_expr") {
        /* NON usa eval() Python: "g['__builtins__']={}" è un sandbox noto per
           essere aggirabile con tecniche solo-attributi (es.
           "().__class__.__base__.__subclasses__()"), che risalgono a classi
           come subprocess.Popen senza name-lookup nei builtins — RCE per
           chiunque gestisca (o abbia compromesso/spoofato) il coordinatore
           WAN a cui questo nodo si connette, senza bisogno di alcun opt-in
           (a differenza di shell_cmd/python_repl). Fix: interprete AST
           scritto a mano che cammina l'albero sintattico e ammette SOLO
           Constant/BinOp/UnaryOp/Call-a-funzione-in-whitelist/Name-in-whitelist
           — mai un vero eval()/exec(), quindi Attribute/Subscript/Call verso
           nomi non in whitelist sono respinti dalla whitelist stessa, non da
           un builtins svuotato aggirabile. */
        static const QString kMathExprScript =
            "import ast,math,cmath,statistics,operator,sys\n"
            "_names={}\n"
            "for _m in (math,cmath,statistics):\n"
            "    _names.update({k:v for k,v in vars(_m).items() if not k.startswith('_')})\n"
            "_binops={ast.Add:operator.add,ast.Sub:operator.sub,ast.Mult:operator.mul,"
            "ast.Div:operator.truediv,ast.FloorDiv:operator.floordiv,"
            "ast.Mod:operator.mod,ast.Pow:operator.pow}\n"
            "_unops={ast.UAdd:operator.pos,ast.USub:operator.neg}\n"
            "def _ev(n):\n"
            "    if isinstance(n,ast.Expression): return _ev(n.body)\n"
            "    if isinstance(n,ast.Constant):\n"
            "        if isinstance(n.value,(int,float,complex)): return n.value\n"
            "        raise ValueError('costante non numerica')\n"
            "    if isinstance(n,ast.BinOp) and type(n.op) in _binops:\n"
            "        return _binops[type(n.op)](_ev(n.left),_ev(n.right))\n"
            "    if isinstance(n,ast.UnaryOp) and type(n.op) in _unops:\n"
            "        return _unops[type(n.op)](_ev(n.operand))\n"
            "    if isinstance(n,ast.Call):\n"
            "        if not isinstance(n.func,ast.Name) or n.func.id not in _names:\n"
            "            raise ValueError('funzione non ammessa')\n"
            "        fn=_names[n.func.id]\n"
            "        if not callable(fn): raise ValueError('non e una funzione')\n"
            "        return fn(*[_ev(a) for a in n.args])\n"
            "    if isinstance(n,ast.Name):\n"
            "        if n.id in _names and not callable(_names[n.id]): return _names[n.id]\n"
            "        raise ValueError('nome non ammesso: '+n.id)\n"
            "    if isinstance(n,ast.List): return [_ev(e) for e in n.elts]\n"
            "    if isinstance(n,ast.Tuple): return tuple(_ev(e) for e in n.elts)\n"
            "    raise ValueError('espressione non ammessa: '+type(n).__name__)\n"
            "try:\n"
            "    print(_ev(ast.parse(sys.argv[1],mode='eval')))\n"
            "except Exception as e:\n"
            "    print('Errore:', e)\n";
        result = ProcHelper::readOutput(
            "python3", {"-c", kMathExprScript, "--", payload.trimmed().left(500)}, 5000);

    } else if (kind == "matplotlib_plot") {
        if (!shellAllowed) {
            result = "[SICUREZZA] Esecuzione Python disabilitata. "
                     "Abilita 'Permetti shell' nelle opzioni nodo WAN.";
            status = "error";
        } else {
            const auto r = ProcHelper::run(P::findPython(), {"-c", payload}, 30000);
            result = r.out + r.err;
        }

    } else if (kind == "graphviz_render") {
        const auto r = ProcHelper::runWithInput("dot", {"-Tsvg"}, payload.toUtf8(), 15000);
        result = r.out.isEmpty() ? "Errore Graphviz: " + r.err : r.out.left(8000);

    } else if (kind == "file_read") {
        if (!shellAllowed) {
            result = "file_read richiede 'Permetti shell' abilitato su questo nodo.";
            status = "error";
        } else if (!SciComputePage::isSafePath(payload.trimmed())) {
            /* Stesso allowlist già usato da SciComputePage (porta 11601) —
               prima mancava qui: il coordinatore WAN remoto poteva leggere
               ~/.ssh/id_rsa o il token LAN col solo checkbox "Permetti
               shell" già attivo per altri motivi. */
            result = "[SICUREZZA] Percorso non autorizzato: " + payload.trimmed();
            status = "error";
        } else {
            QFile f(payload.trimmed());
            if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream ts(&f);
                result = ts.readAll().left(12000);
            } else {
                result = "File non trovato o non leggibile: " + payload;
                status = "error";
            }
        }

    } else if (kind == "file_write") {
        /* payload = JSON {"path":"...","content":"..."} */
        if (!shellAllowed) {
            result = "file_write richiede 'Permetti shell' abilitato su questo nodo.";
            status = "error";
        } else {
            QJsonParseError jerr;
            const QJsonDocument doc = QJsonDocument::fromJson(payload.toUtf8(), &jerr);
            if (doc.isObject()) {
                const QString path    = doc.object()["path"].toString();
                const QString content = doc.object()["content"].toString();
                if (!SciComputePage::isSafePath(path)) {
                    result = "[SICUREZZA] Percorso non autorizzato: " + path;
                    status = "error";
                } else {
                    QFile f(path);
                    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                        QTextStream ts(&f); ts << content;
                        result = "Scritto: " + path;
                    } else {
                        result = "Impossibile scrivere: " + path;
                        status = "error";
                    }
                }
            } else {
                result = "Payload non valido: atteso JSON {\"path\":\"...\",\"content\":\"...\"}";
                status = "error";
            }
        }

    } else if (kind == "system_info") {
        /* Raccoglie info sistema via QSysInfo + /proc */
        QString ram;
        QFile meminfo("/proc/meminfo");
        if (meminfo.open(QIODevice::ReadOnly)) {
            QTextStream ts(&meminfo);
            QString line;
            while (ts.readLineInto(&line)) {
                if (line.startsWith("MemTotal:") || line.startsWith("MemAvailable:"))
                    ram += line + "\n";
            }
        }
        result = QString(
            "OS:          %1\n"
            "Kernel:      %2\n"
            "Arch:        %3\n"
            "CPU (Qt):    %4\n"
            "Hostname:    %5\n"
            "Qt version:  %6\n"
            "%7")
            .arg(QSysInfo::prettyProductName())
            .arg(QSysInfo::kernelVersion())
            .arg(QSysInfo::currentCpuArchitecture())
            .arg(QSysInfo::buildCpuArchitecture())
            .arg(QSysInfo::machineHostName())
            .arg(qVersion())
            .arg(ram.trimmed());

    } else if (kind == "net_info") {
        QStringList lines;
        for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
            if (iface.flags() & QNetworkInterface::IsLoopBack) continue;
            for (const QNetworkAddressEntry& e : iface.addressEntries()) {
                if (e.ip().protocol() != QAbstractSocket::IPv4Protocol) continue;
                lines << QString("%1  %2  (MAC: %3)")
                         .arg(iface.name(), -12)
                         .arg(e.ip().toString(), -18)
                         .arg(iface.hardwareAddress());
            }
        }
        result = lines.isEmpty() ? "Nessuna interfaccia IPv4 trovata." : lines.join("\n");

    } else {
        /* Tipo sconosciuto: fallback AI generico */
        m_wanCliAiActive = true;
        m_wanCliAiBuf.clear();
        QObject::disconnect(m_wanCliTokenConn);
        QObject::disconnect(m_wanCliFinishedConn);
        QObject::disconnect(m_wanCliErrorConn);
        m_wanCliTokenConn    = connect(m_ai, &AiClient::token,
                                       this, &LanWanPage::onWanCliAiToken);
        m_wanCliFinishedConn = connect(m_ai, &AiClient::finished,
                                       this, &LanWanPage::onWanCliAiFinished);
        m_wanCliErrorConn    = connect(m_ai, &AiClient::error,
                                       this, &LanWanPage::onWanCliAiError);
        m_ai->chat(
            "Sei un assistente AI. Esegui il task specificato. Rispondi in italiano.",
            QString("[kind: %1]\n%2").arg(kind, payload));
        return;
    }

    /* Invia risultato sincrono */
    wanCliSendJson(QJsonObject{
        {"t","result"}, {"id",id},
        {"status", status}, {"result", result.trimmed()}
    });
    wanCliAppendLog(QString("Task %1 completato (%2).").arg(id, kind));
    m_wanCliCurrentTask.clear();
}
