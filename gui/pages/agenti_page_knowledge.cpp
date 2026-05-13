#include "agenti_page.h"
#include "agenti_page_p.h"
#include "agents_config_dialog.h"
#include "../prismalux_paths.h"
namespace P = PrismaluxPaths;
#include <QProcess>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QSettings>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QStandardPaths>

/* ══════════════════════════════════════════════════════════════
   callKnowledgeMcp — chiama auto_extract_and_update via QProcess
   fire-and-forget: avvia il processo e non aspetta il risultato.
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::callKnowledgeMcp(const QString& summary, const QString& label)
{
    if (summary.trimmed().isEmpty()) return;

    const QString serverPy = P::root()
        + "/MCPs/knowledge_mcp/server.py";
    if (!QFileInfo::exists(serverPy)) return;

    /* JSON-RPC 2.0: tools/call → auto_extract_and_update */
    QJsonObject args;
    args["summary"] = summary.trimmed();
    if (!label.isEmpty())
        args["session_label"] = label;

    QJsonObject params;
    params["name"]      = "auto_extract_and_update";
    params["arguments"] = args;

    QJsonObject req;
    req["jsonrpc"] = "2.0";
    req["id"]      = 1;
    req["method"]  = "tools/call";
    req["params"]  = params;

    const QByteArray payload =
        QJsonDocument(req).toJson(QJsonDocument::Compact) + "\n";

    auto* proc = new QProcess(this);
    proc->setProgram(P::findPython());
    proc->setArguments({ serverPy });

    /* Pulizia automatica alla fine del processo */
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            proc, &QProcess::deleteLater);

    /* Timeout 5s: se il MCP non risponde, uccidi il processo */
    auto* watchdog = new QTimer(this);
    watchdog->setSingleShot(true);
    connect(watchdog, &QTimer::timeout, proc, [proc, watchdog]{
        watchdog->deleteLater();
        if (proc->state() != QProcess::NotRunning) proc->kill();
    });
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            watchdog, &QTimer::deleteLater);

    proc->start();
    if (proc->waitForStarted(2000)) {
        proc->write(payload);
        proc->closeWriteChannel();
        watchdog->start(5000);
        /* Invalida la cache di lettura: il file è stato aggiornato */
        P::invalidateKnowledgeCache();
    } else {
        watchdog->deleteLater();
        proc->deleteLater();
    }
}

/* ══════════════════════════════════════════════════════════════
   onSaveKnowledge — P4: dialog "Salva in Knowledge" dalla toolbar
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::onSaveKnowledge()
{
    /* Testo precompilato: ultimo output agente o plain text del log */
    QString preText;
    if (!m_agentOutputs.isEmpty() && !m_agentOutputs.last().trimmed().isEmpty()) {
        preText = m_agentOutputs.last().trimmed();
        /* Tronca a 1500 caratteri per non sprecare context window */
        if (preText.length() > 1500)
            preText = preText.left(1500) + "\n[... troncato ...]";
    } else if (m_log && !m_log->toPlainText().trimmed().isEmpty()) {
        preText = m_log->toPlainText().trimmed().right(800);
    }

    /* ── Dialog ── */
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle("\xf0\x9f\x93\x96  Salva in Knowledge");  /* 📖 */
    dlg->setMinimumWidth(540);
    dlg->setMinimumHeight(380);
    auto* lay = new QVBoxLayout(dlg);
    lay->setSpacing(10);

    /* Intestazione */
    auto* titleLbl = new QLabel(
        "<b>Salva in <code>user_knowledge.md</code></b><br>"
        "<small>Scegli sezione e inserisci il testo da memorizzare.</small>",
        dlg);
    titleLbl->setTextFormat(Qt::RichText);
    lay->addWidget(titleLbl);

    /* Riga: Sezione + Modalità */
    auto* row = new QHBoxLayout;
    auto* secLbl = new QLabel("Sezione:", dlg);
    auto* secCmb = new QComboBox(dlg);
    secCmb->addItems({
        "contesto",
        "ragionamenti",
        "progetto",
        "procedure",
        "preferenze",
        "chi_sono",
    });
    secCmb->setCurrentIndex(0);  /* default: contesto */

    auto* modLbl = new QLabel("Modalit\xc3\xa0:", dlg);  /* Modalità */
    auto* modCmb = new QComboBox(dlg);
    modCmb->addItems({ "append", "replace_section" });

    row->addWidget(secLbl);
    row->addWidget(secCmb, 1);
    row->addSpacing(12);
    row->addWidget(modLbl);
    row->addWidget(modCmb);
    lay->addLayout(row);

    /* Area testo */
    auto* edit = new QTextEdit(dlg);
    edit->setPlaceholderText(
        "Inserisci il testo da salvare. Puoi usare Markdown.\n"
        "Per 'ragionamenti' e 'contesto' includi una data:\n"
        "**2026-05-06** — ...");
    edit->setPlainText(preText);
    lay->addWidget(edit, 1);

    /* Hint sezione corrente */
    auto* hintLbl = new QLabel(dlg);
    hintLbl->setObjectName("hintLabel");
    hintLbl->setWordWrap(true);
    static const QHash<QString,QString> kHints = {
        {"contesto",      "Ultimi eventi rilevanti della sessione (rotazione max 10 voci)."},
        {"ragionamenti",  "Decisioni prese con motivazione breve (rotazione max 15 voci)."},
        {"progetto",      "Stato, stack e priorità correnti del progetto."},
        {"procedure",     "Algoritmi o flussi consolidati da ricordare."},
        {"preferenze",    "Stile di risposta, lingua, cosa evitare."},
        {"chi_sono",      "Ruolo, background, obiettivi e livello tecnico."},
    };
    auto updateHint = [hintLbl](const QString& sec){
        hintLbl->setText("<small>" + kHints.value(sec, "") + "</small>");
    };
    updateHint(secCmb->currentText());
    connect(secCmb, &QComboBox::currentTextChanged, dlg, updateHint);
    lay->addWidget(hintLbl);

    /* Bottoni */
    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch(1);
    auto* btnCancel = new QPushButton("Annulla", dlg);
    auto* btnSave   = new QPushButton("\xf0\x9f\x92\xbe  Salva", dlg);  /* 💾 */
    btnSave->setObjectName("actionBtn");
    btnRow->addWidget(btnCancel);
    btnRow->addWidget(btnSave);
    lay->addLayout(btnRow);

    connect(btnCancel, &QPushButton::clicked, dlg, &QDialog::reject);
    connect(btnSave, &QPushButton::clicked, dlg, [=]{
        const QString text = edit->toPlainText().trimmed();
        if (text.isEmpty()) { dlg->reject(); return; }

        const QString sec  = secCmb->currentText();
        const QString mode = modCmb->currentText();

        /* Chiama update_knowledge con sezione e modalità esplicite */
        const QString serverPy = P::root()
            + "/MCPs/knowledge_mcp/server.py";
        if (!QFileInfo::exists(serverPy)) { dlg->accept(); return; }

        QJsonObject args;
        args["section"] = sec;
        args["content"] = text;
        args["mode"]    = mode;

        QJsonObject params;
        params["name"]      = "update_knowledge";
        params["arguments"] = args;

        QJsonObject req;
        req["jsonrpc"] = "2.0";
        req["id"]      = 1;
        req["method"]  = "tools/call";
        req["params"]  = params;

        const QByteArray payload =
            QJsonDocument(req).toJson(QJsonDocument::Compact) + "\n";

        auto* proc = new QProcess(this);
        proc->setProgram(P::findPython());
        proc->setArguments({ serverPy });
        connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                proc, &QProcess::deleteLater);
        proc->start();
        if (proc->waitForStarted(2000)) {
            proc->write(payload);
            proc->closeWriteChannel();
            P::invalidateKnowledgeCache();
        } else {
            proc->deleteLater();
        }
        dlg->accept();
    });

    dlg->exec();
    dlg->deleteLater();
}

/* ══════════════════════════════════════════════════════════════
   runKnowledgeExtract — P5: agente Estrattore nascosto
   Chiamato da advancePipeline() al termine della pipeline multi-agente.
   ══════════════════════════════════════════════════════════════ */
void AgentiPage::runKnowledgeExtract()
{
    m_knowledgeBuf.clear();
    m_opMode = OpMode::KnowledgeExtract;

    QString ctx;

    /* ── Modalità CHAT RAG (singolo agente): usa il testo del log ── */
    if (!m_modePipeline && m_agentOutputs.size() <= 1) {
        /* Ultimi 3000 caratteri del log (plain text senza HTML) */
        const QString logText = m_log ? m_log->toPlainText() : QString();
        ctx = "Conversazione recente:\n" + logText.right(3000);
    } else {
        /* ── Modalità Pipeline: task + output per agente ── */
        ctx = "Task: " + m_taskOriginal.left(300);
        auto roleList = AgentsConfigDialog::roles();
        for (int i = 0; i < m_agentOutputs.size(); i++) {
            int roleIdx = m_cfgDlg->roleCombo(i)->currentIndex();
            const QString roleName = (roleIdx >= 0 && roleIdx < roleList.size())
                ? roleList[roleIdx].name : QString("Agente %1").arg(i + 1);
            const QString out = m_agentOutputs[i].trimmed().left(600);
            if (!out.isEmpty())
                ctx += QString("\n\n[%1]:\n%2").arg(roleName, out);
        }
    }

    /* Prompt deterministico — NON lascia libertà all'AI */
    static const char* kSysExtract =
        "Sei un estrattore di memoria per Prismalux. "
        "Analizza la conversazione fornita. "
        "Riassumi in massimo 5 righe le informazioni NUOVE emerse su utente, preferenze o progetto. "
        "Usa SOLO questi prefissi (uno per riga, ometti se non hai informazioni nuove):\n"
        "  PREFERENZE: <stile o preferenza utente emersa>\n"
        "  PROGETTO: <aggiornamento stato o decisione tecnica>\n"
        "  PROCEDURA: <algoritmo o flusso consolidato>\n"
        "  DECISIONE: <decisione presa con motivazione breve>\n"
        "  CONTESTO: <elemento rilevante per le prossime sessioni>\n"
        "Se la conversazione non contiene nulla di nuovo o rilevante, rispondi solo: NULLA\n"
        "Rispondi SOLO con le righe prefissate. Nessun'altra parola. Italiano.";

    m_ai->chat(QString::fromLatin1(kSysExtract), ctx);
}

