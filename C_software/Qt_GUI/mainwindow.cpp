#include "mainwindow.h"
#include "widgets/whisper_autosetup.h"
#include "pages/agenti_page.h"
#include "pages/pratico_page.h"
#include "pages/impara_page.h"
#include "pages/impostazioni_page.h"
#include "pages/quiz_page.h"
#include "pages/strumenti_page.h"
#include "pages/grafico_page.h"
/* oracolo_page.h rimosso: OracoloPage sostituita da grafico integrato in AgentiPage */
#include "pages/programmazione_page.h"
#include "pages/matematica_page.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QSizePolicy>
#include <QFont>
#include <QStatusBar>
#include <QMenu>
#include <QAction>
#include <QProcess>
#include <QTimer>
#include <QDialog>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QLineEdit>
#include <QDir>
#include <QFileInfo>
#include <QSpinBox>
#include <QGroupBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QCheckBox>
#include "prismalux_paths.h"
#include "chat_history.h"
#include "widgets/spinner_widget.h"
#include <QShortcut>
#include <QListWidget>
#include <QListWidgetItem>
#include <QTextEdit>
#include <QTextCursor>
#include <QFileDialog>
#include <QTextDocument>
#include <QPdfWriter>
#include <QPageSize>
#include <QPageLayout>
#include <QMarginsF>
#include <QMessageBox>

namespace P = PrismaluxPaths;

/* ── Forward declaration helpers math (definiti più in basso) ── */
static bool isMathModel(const QString& filename);
static void showMathDownloadDialog(QWidget* parent, const QString& modelsDir);

/* ══════════════════════════════════════════════════════════════
   migrateLegacyChat — converte le chat pre-bolla nel nuovo formato.

   Estrae il testo grezzo dalla vecchia HTML (QTextDocument::toPlainText),
   identifica task / intestazioni agente / risposte tramite pattern,
   e ricostruisce il documento con le bolle buildUserBubble /
   buildAgentBubble / buildLocalBubble di AgentiPage.

   Se l'HTML è già nel nuovo formato (contiene il marker della bolla
   utente) lo restituisce invariato.
   ══════════════════════════════════════════════════════════════ */
static QString migrateLegacyChat(const QString& html)
{
    /* Già nel nuovo formato → niente da fare */
    if (html.contains("bgcolor='#162544'") || html.contains("bgcolor=\"#162544\""))
        return html;

    /* Estrai testo grezzo */
    QTextDocument doc;
    doc.setHtml(html);
    const QString plain = doc.toPlainText();

    /* ── Risposta locale (0 token) ── */
    static QRegularExpression localRe(
        "(?:Risposta locale|Calcolo locale)[^\\n]*\\n([^\\n]+)\\n"
        "[^\\n]*tempo:[^\\n]*(\\d+[.,]\\d+)\\s*ms");
    {
        auto m = localRe.match(plain);
        if (m.hasMatch()) {
            QString result = m.captured(1).trimmed();
            double ms = m.captured(2).replace(',', '.').toDouble();
            /* Recupera il task dalla riga "Task:" */
            static QRegularExpression taskRe("Task:\\s*(.+)");
            QString task;
            auto tm = taskRe.match(plain);
            if (tm.hasMatch()) task = tm.captured(1).trimmed();
            return AgentiPage::buildUserBubble(task.isEmpty() ? result : task)
                 + AgentiPage::buildLocalBubble(result, ms);
        }
    }

    /* ── Pipeline ── */
    /* Estrae task */
    QString task;
    {
        static QRegularExpression taskRe("Task:\\s*(.+)");
        auto m = taskRe.match(plain);
        if (m.hasMatch()) task = m.captured(1).trimmed();
    }

    /* Spezza il testo in blocchi agente:
       intestazione = riga con "[Agente N"  o "Agente N —"
       fine blocco  = riga con "completato" oppure "Pipeline completata" */
    struct AgentBlock { QString label, model, time, body; };
    QVector<AgentBlock> agents;

    const QStringList lines = plain.split('\n');
    static QRegularExpression hdrRe(
        R"(\[Agente\s+(\d+)\s*[\—\-]\s*([^\]]+)\]\s*(?:[\xf0\x9f\xa4\x96\x1f916]+)?\s*(\S[^\xf0\x9f\x95\x90\n]*)\s*(?:[\xf0\x9f\x95\x90]+)?\s*(\d{2}:\d{2}:\d{2})?)");
    /* Pattern più semplice: cerca linee con "Agente N" e un modello */
    static QRegularExpression simpleHdr(
        "Agente\\s+(\\d+)[^\\[\\]]*\\]?\\s*([a-zA-Z0-9_.:/-]+)\\s*(\\d{2}:\\d{2}:\\d{2})?");

    AgentBlock current;
    bool inAgent = false;

    for (const QString& line : lines) {
        const QString t = line.trimmed();

        /* Fine blocco agente */
        if (inAgent && (t.contains("completato") || t.contains("Pipeline completata"))) {
            agents.append(current);
            current = {};
            inAgent = false;
            continue;
        }

        /* Nuova intestazione agente */
        if (t.contains("[Agente") && t.contains("]")) {
            if (inAgent) agents.append(current);
            current = {};
            inAgent = true;

            /* Estrai role dal pattern [Agente N — Role] */
            static QRegularExpression roleRe(R"(\[Agente\s+(\d+)\s*[\u2014\-]\s*([^\]]+)\])");
            auto rm = roleRe.match(t);
            if (rm.hasMatch())
                current.label = "\xf0\x9f\xa4\x96  Agente " + rm.captured(1) + " \xe2\x80\x94 " + rm.captured(2).trimmed();

            /* Estrai modello (token dopo 🤖) */
            static QRegularExpression modelRe(R"([\xf0\x9f\xa4\x96]\s*(\S+))");
            auto mm = modelRe.match(t);
            if (mm.hasMatch()) current.model = mm.captured(1);

            /* Estrai orario HH:mm:ss */
            static QRegularExpression timeRe(R"((\d{2}:\d{2}:\d{2}))");
            auto tm = timeRe.match(t);
            if (tm.hasMatch()) current.time = tm.captured(1);
            continue;
        }

        if (inAgent) {
            /* Salta righe separatori o "generando..." */
            if (t.startsWith("\xe2\x94") || t.startsWith("\xe2\x80\x94") ||
                t.contains("generando") || t.isEmpty())
                continue;
            current.body += line + '\n';
        }
    }
    if (inAgent) agents.append(current);

    /* Costruisci l'HTML con le bolle */
    if (agents.isEmpty() && task.isEmpty()) {
        /* Formato sconosciuto: mostra il testo grezzo in un card neutro */
        QString safe = plain;
        safe.replace("&","&amp;").replace("<","&lt;").replace(">","&gt;");
        safe.replace("\n","<br>");
        return "<table width='100%' cellpadding='0' cellspacing='0'><tr>"
               "<td bgcolor='#111827' style='border:1px solid #374151;"
               "border-radius:8px;padding:12px;color:#e2e8f0;'>"
               "<p style='color:#6b7280;font-size:11px;margin:0 0 6px 0;'>"
               "\xf0\x9f\x93\x9c  Chat storica (formato precedente)</p>"
               + safe + "</td></tr></table>";
    }

    QString result = task.isEmpty() ? QString() : AgentiPage::buildUserBubble(task);
    for (const auto& ag : agents) {
        QString content = AgentiPage::markdownToHtml(ag.body.trimmed());
        if (content.isEmpty())
            content = "<p style='color:#6b7280;font-style:italic;'>Nessun contenuto salvato.</p>";
        result += AgentiPage::buildAgentBubble(
            ag.label.isEmpty() ? "\xf0\x9f\xa4\x96  Agente" : ag.label,
            ag.model.isEmpty() ? "—" : ag.model,
            ag.time.isEmpty()  ? "—" : ag.time,
            content);
    }
    return result;
}

/* ══════════════════════════════════════════════════════════════
   ResourceGauge
   ══════════════════════════════════════════════════════════════ */
ResourceGauge::ResourceGauge(const QString& label, QWidget* parent)
    : QWidget(parent)
{
    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(0,0,0,0);
    lay->setSpacing(5);

    m_lbl = new QLabel(label, this);
    m_lbl->setObjectName("gaugeLabel");
    m_lbl->setFixedWidth(34);

    m_bar = new QProgressBar(this);
    m_bar->setObjectName("resBar");
    m_bar->setRange(0,100);
    m_bar->setValue(0);
    m_bar->setTextVisible(false);
    m_bar->setFixedSize(70, 8);

    m_pct = new QLabel("  0.0%", this);
    m_pct->setObjectName("gaugePct");
    m_pct->setFixedWidth(42);

    lay->addWidget(m_lbl);
    lay->addWidget(m_bar);
    lay->addWidget(m_pct);
}

void ResourceGauge::update(double pct, const QString& /*detail*/) {
    m_bar->setValue(static_cast<int>(pct));
    m_pct->setText(QString("%1%").arg(pct, 5, 'f', 1));
    setLevel(pct);
}

void ResourceGauge::setLevel(double pct) {
    /* Soglie QSS: < 70% verde (default), 70-90% giallo, > 90% rosso */
    const QString lvl = (pct >= 90) ? "crit" : (pct >= 70) ? "warn" : "";
    m_bar->setProperty("level", lvl);
    P::repolish(m_bar);  /* forza ricalcolo stile dopo cambio property */
}

/* ══════════════════════════════════════════════════════════════
   MainWindow — costruttore
   ══════════════════════════════════════════════════════════════ */
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("🍺 Prismalux v2.1 — Centro di Controllo");
    setMinimumSize(1060, 680);
    resize(1200, 760);

    /* ── Servizi di background ── */
    m_hw = new HardwareMonitor(this);
    m_ai = new AiClient(this);

    connect(m_hw, &HardwareMonitor::updated,     this, &MainWindow::onHWUpdated);
    connect(m_hw, &HardwareMonitor::hwInfoReady, this, &MainWindow::onHWReady);

    /* ── Layout principale: Header + [Sidebar | Content] ── */
    auto* root   = new QWidget(this);
    auto* rootLay = new QVBoxLayout(root);
    rootLay->setContentsMargins(0,0,0,0);
    rootLay->setSpacing(0);

    rootLay->addWidget(buildHeader());

    auto* body    = new QWidget(root);
    auto* bodyLay = new QHBoxLayout(body);
    bodyLay->setContentsMargins(0,0,0,0);
    bodyLay->setSpacing(0);
    bodyLay->addWidget(buildSidebar());
    bodyLay->addWidget(buildContent(), 1);
    rootLay->addWidget(body, 1);

    setCentralWidget(root);

    /* Status bar — barra progresso pipeline (permanente a destra) */
    m_statusProgress = new QProgressBar(this);
    m_statusProgress->setRange(0, 100);
    m_statusProgress->setValue(0);
    m_statusProgress->setFixedWidth(220);
    m_statusProgress->setFixedHeight(14);
    m_statusProgress->setTextVisible(true);
    m_statusProgress->setFormat("");
    m_statusProgress->setObjectName("statusProgress");
    m_statusProgress->setVisible(false);   /* nascosta finché non parte una pipeline */
    statusBar()->addPermanentWidget(m_statusProgress);
    statusBar()->showMessage("\xf0\x9f\x8d\xba  Invocazione riuscita. Gli dei ascoltano.");

    /* Avvia monitoraggio hardware */
    m_hw->start();

    /* Auto-setup whisper.cpp in background (non blocca UI).
       Controlla presenza binario + modello dentro il progetto;
       se mancano avvia: git clone → cmake build → download modello. */
    QTimer::singleShot(1500, this, [this]{
        auto* wsp = new WhisperAutoSetup(
            [this](const QString& msg){ statusBar()->showMessage(msg); }, this);
        connect(wsp, &WhisperAutoSetup::ready, this, [this]{
            statusBar()->showMessage(
                "\xe2\x9c\x85  Riconoscimento vocale pronto.");
            QTimer::singleShot(4000, this, [this]{
                statusBar()->showMessage(
                    "\xf0\x9f\x8d\xba  Invocazione riuscita. Gli dei ascoltano.");
            });
        });
        connect(wsp, &WhisperAutoSetup::failed, this, [this](const QString& err){
            statusBar()->showMessage("\xe2\x9a\xa0  Whisper setup: " + err);
        });
        wsp->run();
    });

    /* Timer auto-scarico modello: ogni 90s, se RAM > 40% e AI non occupato,
     * manda keep_alive=0 a Ollama per liberare RAM automaticamente. */
    m_idleUnloadTimer = new QTimer(this);
    m_idleUnloadTimer->setInterval(90'000);  /* 90 secondi */
    connect(m_idleUnloadTimer, &QTimer::timeout, this, [this]{
        if (!m_ai->busy() && m_ai->isModelLoaded() && m_ai->backend() == AiClient::Ollama) {
            const double freePct = m_ai->ramFreePct();
            if (freePct < 60.0) {  /* RAM usata > 40% */
                m_ai->unloadModel();
                statusBar()->showMessage(
                    "\xf0\x9f\x97\x91  Auto-scarico: modello rimosso dalla RAM "
                    "(RAM > 40% — scarico automatico dopo inattivit\xc3\xa0).");
                QTimer::singleShot(5000, this, [this]{
                    statusBar()->showMessage(
                        "\xf0\x9f\x8d\xba  Invocazione riuscita. Gli dei ascoltano.");
                });
            }
        }
    });
    m_idleUnloadTimer->start();

    /* Imposta backend Ollama di default e carica modelli */
    m_ai->setBackend(AiClient::Ollama, P::kLocalHost, P::kOllamaPort, "");
    m_ai->fetchModels();
    connect(m_ai, &AiClient::modelsReady, this, [this](const QStringList& list){
        if (!list.isEmpty()) {
            m_lblModel->setText(list.first());
            statusBar()->showMessage(
                QString("🍺  Backend Ollama | Modello: %1 | Modelli disponibili: %2")
                .arg(list.first()).arg(list.size()));
        }
        /* ── Auto VRAM Benchmark al primo avvio ── */
        maybeAutoVramBench();
    });

    /* Carica tema salvato */
    ThemeManager::instance()->loadSaved();

    /* Pagina iniziale */
    navigateTo(0);

    /* Scorciatoie da tastiera: Alt+1…7 = navigazione rapida
       0=Agenti  1=Strumenti  2=Grafico  3=Programmazione
       4=Matematica  5=Impara(+Finanza)  6=Sfida
       (Finanza non ha più shortcut separato: è sotto-scheda di Impara) */
    auto* sc1 = new QShortcut(QKeySequence("Alt+1"), this);
    auto* sc2 = new QShortcut(QKeySequence("Alt+2"), this);
    auto* sc3 = new QShortcut(QKeySequence("Alt+3"), this);
    auto* sc4 = new QShortcut(QKeySequence("Alt+4"), this);
    auto* sc5 = new QShortcut(QKeySequence("Alt+5"), this);
    auto* sc6 = new QShortcut(QKeySequence("Alt+6"), this);
    auto* sc7 = new QShortcut(QKeySequence("Alt+7"), this);
    connect(sc1, &QShortcut::activated, this, [this]{ navigateTo(0); }); /* Agenti AI */
    connect(sc2, &QShortcut::activated, this, [this]{ navigateTo(1); }); /* Strumenti */
    connect(sc3, &QShortcut::activated, this, [this]{ navigateTo(2); }); /* Grafico */
    connect(sc4, &QShortcut::activated, this, [this]{ navigateTo(3); }); /* Programmazione */
    connect(sc5, &QShortcut::activated, this, [this]{ navigateTo(4); }); /* Matematica */
    connect(sc6, &QShortcut::activated, this, [this]{ navigateTo(5); }); /* Impara (+Finanza) */
    connect(sc7, &QShortcut::activated, this, [this]{ navigateTo(6); }); /* Sfida */
}

/* ══════════════════════════════════════════════════════════════
   buildHeader — barra superiore con logo + gauges hardware
   ══════════════════════════════════════════════════════════════ */
QWidget* MainWindow::buildHeader() {
    auto* hdr = new QWidget(this);
    hdr->setObjectName("header");
    hdr->setFixedHeight(52);

    auto* lay = new QHBoxLayout(hdr);
    lay->setContentsMargins(16, 8, 16, 8);
    lay->setSpacing(12);

    /* ── Hamburger: mostra/nascondi sidebar ── */
    auto* btnHamburger = new QPushButton("\xe2\x98\xb0", hdr);  /* ☰ */
    btnHamburger->setObjectName("hamburgerBtn");
    btnHamburger->setFixedSize(36, 36);
    btnHamburger->setToolTip("Mostra / Nascondi la colonna sinistra");
    connect(btnHamburger, &QPushButton::clicked, this, [this]{
        if (m_sidebarWidget)
            m_sidebarWidget->setVisible(!m_sidebarWidget->isVisible());
    });
    lay->addWidget(btnHamburger);

    /* Logo */
    auto* beer = new QLabel("🍺", hdr);
    beer->setObjectName("headerBeer");
    lay->addWidget(beer);

    /* Titolo */
    auto* title = new QLabel("PRISMALUX", hdr);
    title->setObjectName("headerTitle");
    /* m_lblBackend tenuto per aggiornamenti interni (applyBackend) ma non mostrato */
    m_lblBackend = new QLabel("", hdr);
    m_lblBackend->hide();
    /* m_lblModel tenuto per i setText interni, non mostrato nell'header */
    m_lblModel = new QLabel(this);
    m_lblModel->hide();
    lay->addWidget(title);

    lay->addStretch(1);

    /* Gauges hardware — una sola riga: CPU · RAM · GPU */
    m_gCpu = new ResourceGauge("CPU ", hdr);
    m_gRam = new ResourceGauge("RAM ", hdr);
    m_gGpu = new ResourceGauge("GPU ", hdr);
    lay->addWidget(m_gCpu);
    lay->addWidget(m_gRam);
    lay->addWidget(m_gGpu);

    /* ── Pulsante emergenza RAM 🚨 ── */
    auto* btnEmergency = new QPushButton("🚨", hdr);
    btnEmergency->setObjectName("emergencyBtn");
    btnEmergency->setToolTip(
        "EMERGENZA RAM\n"
        "1. Ferma tutti i modelli Ollama\n"
        "2. Libera cache kernel (richiede password admin)");
    btnEmergency->setFixedSize(42, 36);

    connect(btnEmergency, &QPushButton::clicked, this, [this, btnEmergency]{
        btnEmergency->setEnabled(false);
        statusBar()->showMessage("🚨  Emergenza RAM — arresto modelli Ollama...");

        /* Passo 1: ferma tutti i modelli Ollama via CLI */
        auto* stopProc = new QProcess(this);
        stopProc->start("bash", {"-c",
            "ollama ps --no-trunc 2>/dev/null | awk 'NR>1{print $1}' | "
            "xargs -r -I{} ollama stop {}"});

        connect(stopProc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, btnEmergency, stopProc](int, QProcess::ExitStatus){
            stopProc->deleteLater();
            statusBar()->showMessage("🚨  Modelli fermati — liberazione cache kernel...");

            /* Passo 2: drop_caches con dialogo grafico KDE */
            auto* cacheProc = new QProcess(this);
#ifdef Q_OS_WIN
            cacheProc->start("rundll32.exe", {"advapi32.dll,ProcessIdleTasks"});
#else
            cacheProc->start("pkexec",
                {"sh", "-c", "sync && echo 3 > /proc/sys/vm/drop_caches"});
#endif
            connect(cacheProc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                    this, [this, btnEmergency, cacheProc](int code, QProcess::ExitStatus){
                cacheProc->deleteLater();
                if (code == 0)
                    statusBar()->showMessage(
                        "✅  Emergenza RAM completata — modelli fermati + cache liberata.");
                else
                    statusBar()->showMessage(
                        "⚠️  Modelli fermati. Cache non liberata (password annullata o pkexec mancante).");
                btnEmergency->setEnabled(true);
                /* Ripristina messaggio normale dopo 6 secondi */
                QTimer::singleShot(6000, this, [this]{
                    statusBar()->showMessage("🍺  Invocazione riuscita. Gli dei ascoltano.");
                });
            });
        });
    });

    lay->addWidget(btnEmergency);

    /* ── Pulsante "Scarica LLM" — scarica il modello dalla RAM via API Ollama ── */
    m_btnUnload = new QPushButton("\xf0\x9f\x97\x91  Scarica LLM", hdr);
    m_btnUnload->setObjectName("unloadBtn");
    m_btnUnload->setFixedHeight(36);
    m_btnUnload->setToolTip(
        "Scarica il modello dalla RAM (keep_alive=0)\n"
        "Utile quando Ollama tiene il modello caricato\n"
        "anche dopo che hai finito di usarlo.\n"
        "Diventa giallo se RAM > 40%, rosso se > 75%.");
    connect(m_btnUnload, &QPushButton::clicked, this, [this]{
        m_ai->unloadModel();
        statusBar()->showMessage(
            "\xf0\x9f\x97\x91  Richiesta scarico modello inviata a Ollama — "
            "il modello verr\xc3\xa0 rimosso dalla RAM.");
        QTimer::singleShot(4000, this, [this]{
            statusBar()->showMessage("\xf0\x9f\x8d\xba  Invocazione riuscita. Gli dei ascoltano.");
        });
    });
    lay->addWidget(m_btnUnload);

    /* ── Pulsante toggle backend (sempre visibile) ── */
    m_btnBackend = new QPushButton("\xf0\x9f\xa6\x99  Ollama", hdr);
    m_btnBackend->setObjectName("backendBtn");
    m_btnBackend->setToolTip("Cambia backend AI — un click per passare da Ollama a llama-server e viceversa");
    m_btnBackend->setFixedHeight(36);
    m_btnBackend->setMinimumWidth(130);

    connect(m_btnBackend, &QPushButton::clicked, this, [this]{
        auto* menu = new QMenu(m_btnBackend);
        menu->setObjectName("backendMenu");

        const bool serverRunning = m_serverProc &&
                                   m_serverProc->state() != QProcess::NotRunning;
        const bool isOllama = (m_ai->backend() == AiClient::Ollama);

        /* ── Sezione backend attivo ── */
        auto* actOllama = menu->addAction("\xf0\x9f\xa6\x99  Ollama  \xe2\x80\x94  localhost:11434");
        actOllama->setCheckable(true);
        actOllama->setChecked(isOllama);
        connect(actOllama, &QAction::triggered, this, [this]{
            applyBackend(AiClient::Ollama, P::kLocalHost, P::kOllamaPort);
        });

        menu->addSeparator();

        /* ── Sezione llama-server ── */
        if (serverRunning) {
            /* Server in esecuzione */
            auto* actInfo = menu->addAction(
                QString("\xf0\x9f\xa6\x99\xe2\x98\x81\xef\xb8\x8f  llama-server :%1  \xe2\x97\x8f in esecuzione")
                .arg(m_serverPort));
            actInfo->setEnabled(false); /* solo informativo */

            auto* actStop = menu->addAction(
                QString("\xf0\x9f\x94\xb4  Ferma server  :%1").arg(m_serverPort));
            connect(actStop, &QAction::triggered, this, &MainWindow::stopLlamaServer);
        } else {
            auto* actLSrv = menu->addAction("\xf0\x9f\xa6\x99\xe2\x98\x81\xef\xb8\x8f  Avvia llama-server...");
            actLSrv->setCheckable(true);
            actLSrv->setChecked(!isOllama);
            connect(actLSrv, &QAction::triggered, this, &MainWindow::showServerDialog);
        }

        menu->exec(m_btnBackend->mapToGlobal(
            QPoint(0, m_btnBackend->height())));
        menu->deleteLater();
    });

    /* m_btnBackend viene aggiunto come corner widget del tab principale in buildContent() */
    m_btnBackend->setParent(this);  /* reparent temporaneo — buildContent lo riposiziona */

    /*
     * StatusBadge — dot colorato per stato llama-server:
     *   ● grigio   Offline  → server non avviato
     *   ● giallo   Starting → avvio in corso (polling /health)
     *   ● verde    Online   → server pronto e backend commutato
     * Aggiornato da startLlamaServer(), stopLlamaServer(), polling timer.
     */
    /* Badge e spinner mantenuti come puntatori null — lo stato viene mostrato
     * direttamente nel testo di m_btnBackend per tenere l'header pulito. */
    m_badgeServer = nullptr;
    m_spinServer  = nullptr;

    return hdr;
}

/* ══════════════════════════════════════════════════════════════
   showServerDialog — dialog avvio llama-server
   Estratto dal lambda di m_btnServer per essere richiamato
   anche dal menu contestuale di m_btnBackend.
   ══════════════════════════════════════════════════════════════ */
void MainWindow::showServerDialog()
{
    const QStringList modelPaths = P::scanGgufFiles();

    auto* dlg = new QDialog(this);
    dlg->setWindowTitle("\xf0\x9f\xa6\x99\xe2\x98\x81\xef\xb8\x8f  Avvia llama-server");
    dlg->setFixedWidth(460);
    auto* lay = new QVBoxLayout(dlg);
    lay->setSpacing(10);

    lay->addWidget(new QLabel(
        "<b>Seleziona modello</b> \xe2\x80\x94 il server parte in background,<br>"
        "il backend viene commutato automaticamente.", dlg));

    /* ── Banner hardware: mostra il device più veloce selezionato automaticamente ── */
    {
        QString hwLine;
        if (m_hw && m_hw->hwReady()) {
            const HWInfo& hw = m_hw->hwInfo();
            /* Cerca la GPU con più VRAM disponibile — sempre preferita sulla CPU */
            int bestGpu = -1;
            for (int i = 0; i < hw.count; i++) {
                if (hw.dev[i].type != DEV_CPU) {
                    if (bestGpu < 0 || hw.dev[i].avail_mb > hw.dev[bestGpu].avail_mb)
                        bestGpu = i;
                }
            }
            if (bestGpu >= 0) {
                const HWDevice& g = hw.dev[bestGpu];
                int ngl = (g.n_gpu_layers > 0) ? g.n_gpu_layers : 99;
                hwLine = QString(
                    "<span style='color:#16a34a;'>"
                    "\xf0\x9f\x8e\xae  <b>GPU rilevata:</b> %1 &mdash; %2 MB VRAM liberi "
                    "&rarr; <b>-ngl %3</b> (accelerazione GPU attiva)</span>")
                    .arg(QString::fromUtf8(g.name)).arg(g.avail_mb).arg(ngl);
            } else {
                const HWDevice& c = hw.dev[hw.primary];
                hwLine = QString(
                    "<span style='color:#b45309;'>"
                    "\xf0\x9f\x96\xa5  <b>CPU:</b> %1 &mdash; nessuna GPU rilevata "
                    "&rarr; <b>-ngl 0</b> (inferenza RAM)</span>")
                    .arg(QString::fromUtf8(c.name));
            }
        } else {
            hwLine = "<span style='color:#6b7280;'>\xe2\x8f\xb3  Rilevamento hardware in corso...</span>";
        }
        auto* hwLbl = new QLabel(hwLine, dlg);
        hwLbl->setWordWrap(true);
        lay->addWidget(hwLbl);
    }

    QStringList mathPaths, otherPaths;
    for (const QString& p : modelPaths) {
        if (isMathModel(QFileInfo(p).fileName())) mathPaths << p;
        else                                        otherPaths << p;
    }

    auto* cmbModel = new QComboBox(dlg);

    auto populateCombo = [&](bool mathOnly) {
        cmbModel->clear();
        if (mathPaths.isEmpty() && otherPaths.isEmpty()) {
            cmbModel->addItem("(nessun .gguf trovato in models/)");
            cmbModel->setEnabled(false);
            return;
        }
        cmbModel->setEnabled(true);
        for (const QString& p : mathPaths)
            cmbModel->addItem("\xf0\x9f\x93\x90 " + QFileInfo(p).fileName(), p);
        if (!mathOnly)
            for (const QString& p : otherPaths)
                cmbModel->addItem(QFileInfo(p).fileName(), p);
    };
    populateCombo(false);
    lay->addWidget(cmbModel);

    auto* rowPort = new QHBoxLayout;
    rowPort->addWidget(new QLabel("Porta:", dlg));
    auto* spPort = new QSpinBox(dlg);
    spPort->setRange(1024, 65535);
    spPort->setValue(P::kLlamaServerPort);
    spPort->setFixedWidth(90);
    rowPort->addWidget(spPort);
    rowPort->addStretch();
    lay->addLayout(rowPort);

    auto* chkMath = new QCheckBox("\xf0\x9f\x93\x90  Profilo matematico (Xeon 64 GB)", dlg);
    chkMath->setToolTip(
        "Abilita flag ottimali per calcolo scientifico:\n"
        "  --ctx-size 8192  (dimostrazioni lunghe)\n"
        "  --no-mmap        (Q4_K_M: carica tutto in RAM, pi\xc3\xb9 veloce)\n"
        "  mmap attivo      (Q8_0: legge dal SSD on-demand, auto)");
    lay->addWidget(chkMath);

    auto* lblMathStatus = new QLabel(dlg);
    lblMathStatus->setWordWrap(true);
    lay->addWidget(lblMathStatus);

    auto* btnMathDl = new QPushButton(
        "\xe2\xac\x87  Scarica modello matematico da Hugging Face", dlg);
    btnMathDl->setObjectName("actionBtn");
    btnMathDl->setVisible(false);
    lay->addWidget(btnMathDl);

    auto updateMathUI = [&](bool mathOn) {
        if (!mathOn) { populateCombo(false); lblMathStatus->hide(); btnMathDl->hide(); return; }
        populateCombo(false);
        if (mathPaths.isEmpty()) {
            lblMathStatus->setText(
                "<span style='color:#ff5252;'>"
                "\xe2\x9a\xa0  Nessun modello matematico trovato in models/.<br>"
                "Scarica Qwen2.5-Math-72B (Q4_K_M ~40 GB) o Qwen2.5-Math-7B (Q4_K_M ~4.7 GB).</span>");
            lblMathStatus->show(); btnMathDl->show();
        } else {
            lblMathStatus->setText(
                QString("<span style='color:#69f0ae;'>"
                        "\xe2\x9c\x85  %1 modello/i matematico/i trovato/i (in cima con \xf0\x9f\x93\x90)."
                        "</span>").arg(mathPaths.size()));
            lblMathStatus->show(); btnMathDl->hide();
            cmbModel->setCurrentIndex(0);
        }
    };

    connect(chkMath, &QCheckBox::toggled, dlg, updateMathUI);
    connect(cmbModel, QOverload<int>::of(&QComboBox::currentIndexChanged), dlg,
            [chkMath, cmbModel](int){
        const QString name = cmbModel->currentText().toLower();
        chkMath->setChecked(name.contains("72b") || name.contains("70b") || name.contains("math"));
    });
    connect(btnMathDl, &QPushButton::clicked, dlg, [dlg]{
        showMathDownloadDialog(dlg, P::modelsDir());
    });

    {
        const QString name = cmbModel->currentText().toLower();
        bool initMath = !mathPaths.isEmpty() || name.contains("math") || name.contains("72b");
        chkMath->setChecked(initMath);
        updateMathUI(initMath);
    }

    lay->addWidget(new QLabel(
        "<span style='color:#5a5f80;font-size:11px;'>"
        "Binario cercato in: llama_cpp_studio/llama.cpp/build/bin/llama-server<br>"
        "Usa <i>avvia_qt.sh</i> dalla cartella Prismalux se il server non parte.</span>",
        dlg));

    auto* bb = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    bb->button(QDialogButtonBox::Ok)->setText("\xe2\x96\xb6  Avvia");
    bb->button(QDialogButtonBox::Ok)->setEnabled(!modelPaths.isEmpty());
    lay->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, dlg, &QDialog::reject);

    if (dlg->exec() == QDialog::Accepted && !modelPaths.isEmpty()) {
        QString modelPath = cmbModel->currentData().toString();
        startLlamaServer(modelPath, spPort->value(), chkMath->isChecked());
    }
    dlg->deleteLater();
}

/* ══════════════════════════════════════════════════════════════
   applyBackend — cambia backend, aggiorna label + pulsante
   ══════════════════════════════════════════════════════════════ */
void MainWindow::applyBackend(AiClient::Backend b, const QString& host, int port) {
    m_ai->setBackend(b, host, port, "");
    m_ai->fetchModels();

    refreshBackendBtn();

    const QString bkName = (b == AiClient::Ollama) ? "Ollama" : "llama.cpp";
    const QString bkIcon = (b == AiClient::Ollama)
        ? "\xf0\x9f\xa6\x99  Ollama"
        : "\xf0\x9f\xa6\x99  llama.cpp";
    m_lblBackend->setText(bkIcon + "  \xe2\x86\x92  " + host + ":" + QString::number(port));
    /* Lo stato viene mostrato nel testo di m_btnBackend — nessun widget extra. */
    m_lblModel->setText("(caricamento modelli...)");

    statusBar()->showMessage(
        QString("🔄  Backend cambiato: %1 @ %2:%3 — recupero modelli...")
        .arg(bkName, host, QString::number(port)));

    /* Quando arrivano i modelli, seleziona il primo e aggiorna status (connessione unica) */
    auto* connHolder = new QObject(this);
    connect(m_ai, &AiClient::modelsReady, connHolder, [this, bkName, connHolder](const QStringList& list){
        if (!list.isEmpty()) {
            m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), list.first());
            m_lblModel->setText(list.first());
            statusBar()->showMessage(
                QString("✅  %1 | Modello: %2 | %3 disponibili")
                .arg(bkName, list.first(), QString::number(list.size())));
        } else {
            m_lblModel->setText("(server non raggiungibile)");
            statusBar()->showMessage(
                QString("⚠️  %1 non risponde — avvialo prima di usare l'AI").arg(bkName));
        }
        connHolder->deleteLater(); /* rimuove la connessione dopo il primo segnale */
    });
}

/* ══════════════════════════════════════════════════════════════
   Helper — rilevamento modelli matematici
   ══════════════════════════════════════════════════════════════ */
static bool isMathModel(const QString& filename) {
    const QString n = filename.toLower();
    return n.contains("math") || n.contains("numina") ||
           n.contains("mathstral") || n.contains("minerva") ||
           n.contains("deepseek-math");
}

/* Struttura per la lista curata HF di modelli matematici */
struct MathModelEntry {
    QString name;
    QString description;
    QString urlQ4, fileQ4, sizeQ4, flagsQ4;
    QString urlQ8, fileQ8, sizeQ8, flagsQ8;
};

static QVector<MathModelEntry> mathModelCatalog() {
    return {
        {
            "Qwen2.5-Math-72B-Instruct",
            "Matematica universitaria/ricerca — ottimale per Xeon 64 GB",
            "https://huggingface.co/bartowski/Qwen2.5-Math-72B-Instruct-GGUF/resolve/main/Qwen2.5-Math-72B-Instruct-Q4_K_M.gguf",
            "Qwen2.5-Math-72B-Instruct-Q4_K_M.gguf", "~40 GB",
            "--no-mmap --ctx-size 8192 --threads 24",
            "https://huggingface.co/bartowski/Qwen2.5-Math-72B-Instruct-GGUF/resolve/main/Qwen2.5-Math-72B-Instruct-Q8_0.gguf",
            "Qwen2.5-Math-72B-Instruct-Q8_0.gguf", "~74 GB",
            "--ctx-size 8192 --threads 24"
        },
        {
            "Qwen2.5-Math-7B-Instruct",
            "Matematica avanzata — leggero, per test o RAM < 16 GB",
            "https://huggingface.co/bartowski/Qwen2.5-Math-7B-Instruct-GGUF/resolve/main/Qwen2.5-Math-7B-Instruct-Q4_K_M.gguf",
            "Qwen2.5-Math-7B-Instruct-Q4_K_M.gguf", "~4.7 GB",
            "--no-mmap --ctx-size 4096",
            "https://huggingface.co/bartowski/Qwen2.5-Math-7B-Instruct-GGUF/resolve/main/Qwen2.5-Math-7B-Instruct-Q8_0.gguf",
            "Qwen2.5-Math-7B-Instruct-Q8_0.gguf", "~7.7 GB",
            "--ctx-size 4096"
        },
    };
}

/* Dialog download modelli matematici da Hugging Face */
static void showMathDownloadDialog(QWidget* parent, const QString& modelsDir) {
    const auto catalog = mathModelCatalog();

    auto* dlg = new QDialog(parent);
    dlg->setWindowTitle("📐  Scarica modello matematico da Hugging Face");
    dlg->setMinimumWidth(620);
    auto* lay = new QVBoxLayout(dlg);
    lay->setSpacing(12);

    lay->addWidget(new QLabel(
        "<b>Seleziona modello e variante di quantizzazione:</b>", dlg));

    /* Una riga per ogni modello con radio Q4 / Q8 */
    QVector<QPair<QRadioButton*, QRadioButton*>> rows;
    auto* grp = new QButtonGroup(dlg);

    for (const auto& m : catalog) {
        auto* box = new QGroupBox(m.name, dlg);
        box->setToolTip(m.description);
        auto* bl = new QVBoxLayout(box);
        bl->addWidget(new QLabel(
            "<span style='color:#5a5f80;font-size:11px;'>" + m.description + "</span>", box));

        auto* rq4 = new QRadioButton(
            QString("Q4_K_M  %1  — carica tutto in RAM%2")
            .arg(m.sizeQ4)
            .arg(m.name.contains("72B") ? "  ✅ consigliato per Xeon 64 GB" : ""), box);
        auto* rq8 = new QRadioButton(
            QString("Q8_0    %1  — qualità massima, richiede NVMe SSD%2")
            .arg(m.sizeQ8)
            .arg(m.name.contains("72B") ? " (mmap automatico)" : ""), box);

        grp->addButton(rq4);
        grp->addButton(rq8);
        bl->addWidget(rq4);
        bl->addWidget(rq8);
        lay->addWidget(box);
        rows.append(qMakePair(rq4, rq8));
    }

    /* Seleziona Q4_K_M del 72B come default */
    if (!rows.isEmpty()) rows[0].first->setChecked(true);

    auto* bb = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    bb->button(QDialogButtonBox::Ok)->setText("⬇  Scarica");
    lay->addWidget(bb);
    QObject::connect(bb, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
    QObject::connect(bb, &QDialogButtonBox::rejected, dlg, &QDialog::reject);

    if (dlg->exec() != QDialog::Accepted) { dlg->deleteLater(); return; }

    /* Trova la selezione */
    QString url, fname, flags;
    for (int i = 0; i < rows.size() && i < catalog.size(); ++i) {
        if (rows[i].first->isChecked()) {
            url = catalog[i].urlQ4; fname = catalog[i].fileQ4; flags = catalog[i].flagsQ4;
        } else if (rows[i].second->isChecked()) {
            url = catalog[i].urlQ8; fname = catalog[i].fileQ8; flags = catalog[i].flagsQ8;
        }
    }

    dlg->deleteLater();
    if (url.isEmpty()) return;

    QString dest = modelsDir + "/" + fname;

    /* Avvia wget in background — args separati: immune a injection da caratteri
       speciali in url/dest, e startDetached è già asincrono (non serve &). */
    QProcess::startDetached("wget", {"-c", "--progress=bar:force", url, "-O", dest});

    QMessageBox info(parent);
    info.setWindowTitle("Download avviato");
    info.setIcon(QMessageBox::Information);
    info.setText(QString("<b>Download avviato in background:</b><br><code>%1</code>").arg(fname));
    info.setInformativeText(
        QString("Destinazione: <code>%1</code><br><br>"
                "Flag consigliati dopo il download:<br>"
                "<code>llama-server -m %2 --port 8081 --host 127.0.0.1 %3</code>")
        .arg(dest, dest, flags));
    info.exec();
}

/* ══════════════════════════════════════════════════════════════
   startLlamaServer — avvia llama-server in background poi
   commuta il backend automaticamente quando il /health risponde
   ══════════════════════════════════════════════════════════════ */
void MainWindow::startLlamaServer(const QString& modelPath, int port, bool mathProfile) {
    /* Percorso binario — rilevato dinamicamente da PrismaluxPaths */
    const QString bin = P::llamaServerBin();

    if (!QFileInfo::exists(bin)) {
        statusBar()->showMessage(
            "❌  llama-server non trovato. Compilalo in Impostazioni → llama.cpp Studio → Compila.");
        return;
    }

    m_serverPort  = port;
    m_serverModel = QFileInfo(modelPath).fileName();

    /* Variabile d'ambiente per le librerie condivise (.so nella stessa dir del binario) */
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("LD_LIBRARY_PATH",
        P::llamaLibDir() + ":" + env.value("LD_LIBRARY_PATH"));

    m_serverProc = new QProcess(this);
    m_serverProc->setProcessEnvironment(env);
    m_serverProc->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_serverProc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int code, QProcess::ExitStatus) {
        /* Il testo del pulsante verrà ripristinato da applyBackend(Ollama) sotto */
        statusBar()->showMessage(
            QString("\xf0\x9f\x94\xb4  llama-server terminato (code %1). Backend tornato a Ollama.").arg(code));
        m_serverProc->deleteLater();
        m_serverProc = nullptr;
        /* Ripristina Ollama come backend */
        applyBackend(AiClient::Ollama, P::kLocalHost, P::kOllamaPort);
    });

    /* ── Rilevamento GPU/CPU per n_gpu_layers ─────────────────────────────
     * Logica:
     *   • GPU NVIDIA/AMD/Intel con VRAM disponibile ≥ 2 GB → -ngl <valore ottimale>
     *     (usa hw_gpu_layers() già calcolato da hw_detect)
     *   • CPU-only (es. Xeon) o GPU con VRAM < 2 GB → -ngl 0 (tutto in RAM)
     * Il valore n_gpu_layers è pre-calcolato da hw_detect.c tenendo conto
     * dell'80% della VRAM disponibile al netto del sistema operativo.
     * ───────────────────────────────────────────────────────────────────── */
    int ngl = 0;
    QString hwDesc = "CPU (RAM)";
    if (m_hw && m_hw->hwReady()) {
        const HWInfo& hw = m_hw->hwInfo();
        /* Cerca la GPU con più VRAM disponibile tra tutti i device.
         * La GPU è SEMPRE preferita alla CPU per l'inferenza, anche se
         * hw.primary punta alla CPU (es. Xeon con 64 GB batte GPU con 8 GB
         * per memoria totale, ma non per velocità di inferenza). */
        int bestGpu = -1;
        for (int i = 0; i < hw.count; i++) {
            if (hw.dev[i].type != DEV_CPU) {
                if (bestGpu < 0 || hw.dev[i].avail_mb > hw.dev[bestGpu].avail_mb)
                    bestGpu = i;
            }
        }
        if (bestGpu >= 0) {
            const HWDevice& g = hw.dev[bestGpu];
            ngl = (g.n_gpu_layers > 0) ? g.n_gpu_layers : 99;
            hwDesc = QString("%1 GPU: %2 — %3 MB VRAM liberi → -ngl %4")
                     .arg(hw_dev_type_str(g.type))
                     .arg(QString::fromUtf8(g.name))
                     .arg(g.avail_mb)
                     .arg(ngl);
        } else {
            const HWDevice& c = hw.dev[hw.primary];
            hwDesc = QString("CPU: %1 → -ngl 0 (RAM)").arg(QString::fromUtf8(c.name));
        }
    }

    /* Determina se il modello è Q4 (per --no-mmap) o Q8/più grande */
    bool isQ4 = QFileInfo(modelPath).fileName().contains("Q4", Qt::CaseInsensitive);

    QStringList args = {
        "-m", modelPath,
        "--port", QString::number(port),
        "--host", "127.0.0.1",
        "--log-disable",
        "-ngl", QString::number(ngl)
    };
    if (mathProfile) {
        args << "--ctx-size" << "8192";
        /* Q4_K_M (~40 GB): carica tutto in RAM — più veloce senza mmap */
        if (isQ4 && ngl == 0) args << "--no-mmap";
        /* Q8_0 (~74 GB): mmap attivo per default (llama.cpp legge dal SSD on-demand) */
    }

    statusBar()->showMessage(
        QString("\xe2\x8f\xb3  Avvio llama-server — %1%2 — porta %3")
        .arg(hwDesc)
        .arg(mathProfile ? " | profilo matematico" : "")
        .arg(port));

    m_serverProc->start(bin, args);

    /*
     * Usiamo il segnale errorOccurred invece di waitForStarted() bloccante:
     * waitForStarted congela il thread UI per fino a 4s.
     * errorOccurred viene emesso immediatamente se il processo non parte.
     */
    connect(m_serverProc, &QProcess::errorOccurred,
            this, [this](QProcess::ProcessError err){
        if (err == QProcess::FailedToStart) {
            if (m_btnBackend) {
                m_btnBackend->setText("\xe2\x9d\x8c  Errore avvio");
                QTimer::singleShot(3000, this, [this]{ refreshBackendBtn(); });
            }
            statusBar()->showMessage(
                "❌  Impossibile avviare llama-server. Verifica il percorso binario.");
            m_serverProc->deleteLater();
            m_serverProc = nullptr;
        }
    });

    statusBar()->showMessage(
        QString("⏳  llama-server avviato — attendo che sia pronto (porta %1)...").arg(port));

    /* Mostra stato caricamento direttamente nel pulsante backend */
    if (m_btnBackend) m_btnBackend->setText("\xe2\x8f\xb3  Caricamento...");

    /*
     * Polling /health ogni 1s, max 180 tentativi (3 minuti).
     * I modelli grandi (13B+) possono richiedere 60-120s al primo caricamento
     * (lettura da disco, allocazione RAM/VRAM, mmap). 30s era troppo poco.
     *
     * Usiamo un QTimer con un singolo QNetworkAccessManager creato fuori
     * dal ciclo (non uno nuovo per ogni tick: ogni NAM porta overhead di
     * thread e pool di connessioni). Il counter è un int membro del QTimer
     * tramite setProperty per evitare new int() sul heap con delete manuale.
     */
    static constexpr int MAX_HEALTH_TICKS = 180;   /* 3 minuti */
    auto* timer = new QTimer(this);
    auto* nam   = new QNetworkAccessManager(this); /* un solo NAM per tutti i tick */
    timer->setProperty("ticks", 0);

    connect(timer, &QTimer::timeout, this, [this, timer, nam, port]{
        const int ticks = timer->property("ticks").toInt() + 1;
        timer->setProperty("ticks", ticks);

        /* Aggiorna status bar con conteggio secondi per dare feedback visivo */
        if (ticks % 5 == 0)   /* ogni 5s per non spammare */
            statusBar()->showMessage(
                QString("\xe2\x8f\xb3  Caricamento modello... %1s / %2s (porta %3)")
                .arg(ticks).arg(MAX_HEALTH_TICKS).arg(port));

        /* Timeout: 3 minuti senza risposta */
        if (ticks > MAX_HEALTH_TICKS || !m_serverProc) {
            timer->stop();
            timer->deleteLater();
            nam->deleteLater();
            if (m_btnBackend) {
                m_btnBackend->setText("\xe2\x9d\x8c  Timeout");
                QTimer::singleShot(3000, this, [this]{ refreshBackendBtn(); });
            }
            if (m_serverProc)
                statusBar()->showMessage(
                    QString("\xe2\x9a\xa0\xef\xb8\x8f  llama-server non risponde dopo %1s sulla porta %2 "
                            "— il modello potrebbe essere troppo grande per la RAM disponibile.")
                    .arg(MAX_HEALTH_TICKS).arg(port));
            return;
        }

        /* GET /health — timeout breve per non sovrapporre i tick */
        QNetworkRequest req(QUrl(
            QString("http://%1:%2/health").arg(P::kLocalHost).arg(port)));
        req.setTransferTimeout(900);
        auto* reply = nam->get(req);

        connect(reply, &QNetworkReply::finished, this, [this, timer, nam, port, reply]{
            const bool ok = (reply->error() == QNetworkReply::NoError);
            reply->deleteLater();
            if (ok) {
                /* Server pronto: ferma il polling, aggiorna badge e commuta il backend */
                timer->stop();
                timer->deleteLater();
                nam->deleteLater();
                /* "✅ Pronto" per 2s poi refreshBackendBtn() aggiorna il testo finale */
                if (m_btnBackend) {
                    m_btnBackend->setText("\xe2\x9c\x85  Pronto");
                    QTimer::singleShot(2000, this, [this]{ refreshBackendBtn(); });
                }
                statusBar()->showMessage(
                    QString("✅  llama-server pronto su porta %1 — backend commutato.").arg(port));
                applyBackend(AiClient::LlamaServer, P::kLocalHost, port);
            }
            /* Se non ok: il timer ritenterà al prossimo tick */
        });
    });
    timer->start(1000);
}

/* ── Ferma llama-server avviato dalla GUI ── */
void MainWindow::stopLlamaServer() {
    if (!m_serverProc) return;
    m_serverProc->terminate();
    statusBar()->showMessage("🛑  Arresto llama-server in corso...");
}

/* ── Aggiorna testo e colore del pulsante backend ── */
void MainWindow::refreshBackendBtn() {
    if (!m_btnBackend) return;
    if (m_ai->backend() == AiClient::Ollama) {
        m_btnBackend->setText("\xf0\x9f\xa6\x99  Ollama");
        m_btnBackend->setProperty("backendActive", "ollama");
    } else {
        m_btnBackend->setText("\xf0\x9f\xa6\x99\xe2\x98\x81\xef\xb8\x8f  llama-server");
        m_btnBackend->setProperty("backendActive", "llama");
    }
    P::repolish(m_btnBackend);
}

/* ══════════════════════════════════════════════════════════════
   buildSidebar — colonna sinistra con bottoni navigazione
   ══════════════════════════════════════════════════════════════ */
QWidget* MainWindow::buildSidebar() {
    auto* bar = new QWidget(this);
    bar->setObjectName("sidebar");

    auto* lay = new QVBoxLayout(bar);
    lay->setContentsMargins(0, 8, 0, 8);
    lay->setSpacing(2);

    /* ── Pulsante Nuova chat ── */
    auto* newChatBtn = new QPushButton("\xe2\x9c\x8f\xef\xb8\x8f  Nuova chat", bar);
    newChatBtn->setObjectName("actionBtn");
    newChatBtn->setFixedHeight(30);
    newChatBtn->setToolTip("Inizia una nuova conversazione (reset log)");
    connect(newChatBtn, &QPushButton::clicked, this, [this]{
        /* Se il log ha contenuto, chiedi se salvare prima di cancellare */
        QTextEdit* log = nullptr;
        if (auto* ap = m_mainTabs ? m_mainTabs->widget(0) : nullptr)
            log = ap->findChild<QTextEdit*>();

        const bool hasContent = log && !log->toPlainText().trimmed().isEmpty();

        if (hasContent) {
            QMessageBox dlg(this);
            dlg.setWindowTitle("\xf0\x9f\x93\xbc  Salva chat");
            dlg.setText(
                "<b>\xf0\x9f\x93\xbc  Vuoi salvare questa chat?</b><br><br>"
                "La conversazione attuale verr\xc3\xa0 persa se non la salvi.");
            dlg.setIcon(QMessageBox::Question);
            QPushButton* btnSalva   = dlg.addButton("\xf0\x9f\x93\xbc  Salva", QMessageBox::AcceptRole);
            QPushButton* btnScarta  = dlg.addButton("Scarta", QMessageBox::DestructiveRole);
            dlg.addButton("Annulla", QMessageBox::RejectRole);
            dlg.setDefaultButton(btnSalva);
            dlg.exec();

            if (dlg.clickedButton() == btnSalva) {
                /* Salva immediatamente con titolo "(salvato manualmente)" */
                const QString html = log->toHtml();
                if (m_currentChatId.isEmpty())
                    m_currentChatId = m_chatHistory.newSession("(salvato manualmente)");
                m_chatHistory.saveLog(m_currentChatId, html);
                refreshChatList();
            } else if (dlg.clickedButton() != btnScarta) {
                return; /* Annulla — non fare nulla */
            }
        }

        /* Resetta la sessione corrente e pulisce il log */
        m_currentChatId.clear();
        if (log) log->clear();
        navigateTo(0);
    });
    lay->addWidget(newChatBtn);

    /* ── Lista chat history ── */
    m_chatList = new QListWidget(bar);
    m_chatList->setObjectName("chatList");
    m_chatList->setFrameShape(QFrame::NoFrame);
    m_chatList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_chatList->setSpacing(2);
    lay->addWidget(m_chatList, 1);   /* stretch=1: occupa lo spazio disponibile */

    connect(m_chatList, &QListWidget::itemClicked, this, [this](QListWidgetItem* item){
        const QString id = item->data(Qt::UserRole).toString();
        if (id.isEmpty()) return;
        const QString rawHtml = m_chatHistory.loadLog(id);
        if (rawHtml.isEmpty()) return;
        /* Migra al formato bolla se è una chat storica */
        const QString html = migrateLegacyChat(rawHtml);
        /* Mostra il log nella pagina Agenti */
        if (auto* ap = m_mainTabs ? m_mainTabs->widget(0) : nullptr) {
            if (auto* log = ap->findChild<QTextEdit*>()) {
                log->setHtml(html);
                log->moveCursor(QTextCursor::End);
            }
        }
        m_currentChatId = id;
        navigateTo(0);
    });

    /* ── Context menu tasto destro sulle chat ── */
    m_chatList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_chatList, &QListWidget::customContextMenuRequested,
            this, [this](const QPoint& pos){
        auto* item = m_chatList->itemAt(pos);
        if (!item) return;
        const QString id    = item->data(Qt::UserRole).toString();
        const QString title = item->text();

        auto* menu = new QMenu(m_chatList);

        auto* actPdf = menu->addAction("\xf0\x9f\x93\x84  Salva come PDF");
        connect(actPdf, &QAction::triggered, this, [this, id, title]{
            const QString html = m_chatHistory.loadLog(id);
            if (html.isEmpty()) { statusBar()->showMessage("\xe2\x9a\xa0  Chat vuota, nessun PDF."); return; }

            const QString def = QDir::homePath() + "/" + title + ".pdf";
            const QString path = QFileDialog::getSaveFileName(
                this, "Salva conversazione come PDF", def, "PDF (*.pdf)");
            if (path.isEmpty()) return;

            QPdfWriter writer(path);
            writer.setPageSize(QPageSize(QPageSize::A4));
            writer.setPageMargins(QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter);

            QTextDocument doc;
            doc.setDefaultStyleSheet(
                "body { font-family: 'Segoe UI', Arial, sans-serif; font-size: 11pt; color:#111; }"
                "p    { margin: 4px 0; }");
            doc.setHtml(html);
            doc.print(&writer);

            statusBar()->showMessage("\xe2\x9c\x85  PDF salvato: " + path);
        });

        menu->addSeparator();

        auto* actDel = menu->addAction("\xf0\x9f\x97\x91  Elimina");
        connect(actDel, &QAction::triggered, this, [this, id, title]{
            auto btn = QMessageBox::question(this, "Elimina chat",
                QString("Eliminare la chat \"%1\"?").arg(title),
                QMessageBox::Yes | QMessageBox::No);
            if (btn != QMessageBox::Yes) return;
            m_chatHistory.remove(id);
            refreshChatList();
        });

        menu->exec(m_chatList->viewport()->mapToGlobal(pos));
        menu->deleteLater();
    });

    refreshChatList();

    m_sidebarWidget = bar;

    /* ── Separatore inferiore ── */
    auto* sepBot = new QFrame(bar);
    sepBot->setFrameShape(QFrame::HLine);
    sepBot->setObjectName("sidebarSep");
    lay->addWidget(sepBot);

    /* ── Bottone Impostazioni (stile ChatGPT — in fondo alla sidebar) ── */
    m_settingsBtn = new QPushButton(bar);
    m_settingsBtn->setObjectName("navBtn");
    m_settingsBtn->setFlat(true);
    m_settingsBtn->setFixedHeight(44);
    m_settingsBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_settingsBtn->setToolTip("Impostazioni — Backend, Hardware, Monitor AI, llama.cpp");

    auto* sw  = new QWidget(m_settingsBtn);
    auto* swl = new QHBoxLayout(sw);
    swl->setContentsMargins(12, 0, 8, 0);
    swl->setSpacing(10);

    auto* sico = new QLabel("\xe2\x9a\x99\xef\xb8\x8f", sw);  /* ⚙️ */
    sico->setObjectName("cardIcon");
    sico->setFixedWidth(24);
    sico->setAlignment(Qt::AlignCenter);

    auto* stitle = new QLabel("Impostazioni", sw);
    stitle->setObjectName("cardTitle");

    swl->addWidget(sico);
    swl->addWidget(stitle, 1);

    auto* sbtnLay = new QHBoxLayout(m_settingsBtn);
    sbtnLay->setContentsMargins(0, 0, 0, 0);
    sbtnLay->addWidget(sw);

    connect(m_settingsBtn, &QPushButton::clicked, this, [this]{
        ensureSettingsDialog();
        m_impDlg->show();
        m_impDlg->raise();
        m_impDlg->activateWindow();
    });
    lay->addWidget(m_settingsBtn);

    return bar;
}

/* ══════════════════════════════════════════════════════════════
   buildContent — QStackedWidget con le 5 pagine
   ══════════════════════════════════════════════════════════════ */
QWidget* MainWindow::buildContent() {
    /* ── Container wrapper: navMenuBar (nascosta) + m_mainTabs ── */
    auto* wrapper = new QWidget(this);
    auto* wLay    = new QVBoxLayout(wrapper);
    wLay->setContentsMargins(0, 0, 0, 0);
    wLay->setSpacing(0);

    /* ── Barra navigazione alternativa (modalità "Menù principale") ── */
    m_navMenuBar = new QFrame(wrapper);
    m_navMenuBar->setObjectName("navMenuBar");
    m_navMenuBar->setFixedHeight(40);
    m_navMenuBar->hide();
    auto* nmLay = new QHBoxLayout(m_navMenuBar);
    nmLay->setContentsMargins(8, 2, 8, 2);
    nmLay->setSpacing(2);

    m_mainTabs = new QTabWidget(wrapper);
    m_mainTabs->setObjectName("mainTabs");
    m_mainTabs->setTabPosition(QTabWidget::North);
    m_mainTabs->setMovable(false);

    /* Pulsante Ollama come corner widget sinistro — a sinistra di "Agenti AI".
       Avvolto in un container con margine destro per evitare sovrapposizione coi tab. */
    if (m_btnBackend) {
        m_btnBackend->setFixedHeight(28);
        m_btnBackend->setMinimumWidth(110);

        m_cornerContainer = new QWidget(m_mainTabs);
        auto* cornerLay = new QHBoxLayout(m_cornerContainer);
        cornerLay->setContentsMargins(4, 0, 8, 0);
        cornerLay->setSpacing(0);
        cornerLay->addWidget(m_btnBackend);

        m_mainTabs->setCornerWidget(m_cornerContainer, Qt::TopLeftCorner);
    }

    auto* agentiPage = new AgentiPage(m_ai, this);
    connect(agentiPage, &AgentiPage::chatCompleted,
            this,       &MainWindow::onChatCompleted);
    connect(agentiPage, &AgentiPage::pipelineStatus,
            this,       &MainWindow::onPipelineStatus);
    connect(agentiPage, &AgentiPage::requestOpenSettings,
            this, [this](const QString& tabName){
        /* Apre il dialog Impostazioni e naviga al tab richiesto */
        ensureSettingsDialog();
        m_impDlg->show();
        m_impDlg->raise();
        m_impDlg->activateWindow();
        if (m_impPage) m_impPage->switchToTab(tabName);
    });

    m_mainTabs->addTab(agentiPage,                      "\xf0\x9f\xa4\x96  Agenti AI");          /* 0 */
    m_mainTabs->addTab(new StrumentiPage(m_ai, this),   "\xf0\x9f\x9b\xa0  Strumenti AI");     /* 1 */
    auto* grafPage = new GraficoPage(m_ai, this);
    m_grafCanvas = grafPage->canvas();
    /* Propaga il canvas al dialog Impostazioni se già aperto prima di buildContent() */
    if (m_impPage) m_impPage->setGraficoCanvas(m_grafCanvas);
    /* Quando l'utente clicca "Installa modello vision", apri Impostazioni → LLM Consigliati */
    connect(grafPage, &GraficoPage::requestOpenSettings,
            this, [this](const QString& tabName){
        ensureSettingsDialog();
        m_impDlg->show();
        m_impDlg->raise();
        m_impDlg->activateWindow();
        if (m_impPage) m_impPage->switchToTab(tabName);
    });
    m_mainTabs->addTab(grafPage,                              "\xf0\x9f\x93\x88  Grafico");          /* 2 */

    /* Quando il tab Grafico diventa attivo, forza repaint con le impostazioni correnti
       (necessario se l'utente ha cambiato la posizione assi mentre il tab era nascosto). */
    connect(m_mainTabs, &QTabWidget::currentChanged, this, [this](int idx){
        if (idx == 2 && m_grafCanvas)
            m_grafCanvas->repaint();
    });
    m_mainTabs->addTab(new ProgrammazionePage(m_ai, this),"\xf0\x9f\x92\xbb  Programmazione");  /* 3 */
    m_mainTabs->addTab(new MatematicaPage(m_ai, this),    "\xcf\x80  Matematica");               /* 4 */
    /* Finanza (PraticoPage) spostata DENTRO il tab Impara come sotto-scheda */
    {
        auto* imparaContainer = new QWidget(m_mainTabs);
        auto* ilay = new QVBoxLayout(imparaContainer);
        ilay->setContentsMargins(0, 0, 0, 0);
        ilay->setSpacing(0);

        auto* imparaTabs = new QTabWidget(imparaContainer);
        imparaTabs->setObjectName("imparaSubTabs");
        imparaTabs->setTabPosition(QTabWidget::North);
        imparaTabs->addTab(new ImparaPage(m_ai, imparaContainer),  "\xf0\x9f\x93\x9a  Impara");
        imparaTabs->addTab(new PraticoPage(m_ai, imparaContainer), "\xf0\x9f\x92\xb0  Finanza");
        ilay->addWidget(imparaTabs);

        m_mainTabs->addTab(imparaContainer, "\xf0\x9f\x93\x9a  Impara");                   /* 5 */
    }
    /* QuizPage usa un AiClient SEPARATO per evitare cross-talk con AgentiPage
       (signal token/finished condivisi causano output fantasma nelle bolle agenti). */
    {
        auto* quizAi = new AiClient(this);
        quizAi->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), m_ai->model());
        /* Sincronizza backend/modello quando l'utente cambia le impostazioni principali */
        connect(m_ai, &AiClient::modelsReady, quizAi, [this, quizAi](const QStringList&){
            quizAi->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), m_ai->model());
        });
        m_mainTabs->addTab(new QuizPage(quizAi, this),     "\xf0\x9f\x8e\xaf  Sfida te stesso!");/* 7 */
    }

    /* ── Salva etichette originali e applica modalità da QSettings ── */
    for (int i = 0; i < m_mainTabs->count(); i++)
        m_tabOrigLabels << m_mainTabs->tabText(i);
    {
        QSettings s("Prismalux", "GUI");
        applyTabMode(s.value("nav/tabMode", "icons_text").toString());
    }

    /* ── Costruisci pulsanti barra navigazione ───────────────────
       Separatore di categoria dopo tab 4 (Matematica) → gruppo AI+Crea | Impara+Sfida */
    {
        auto* btnGroup = new QButtonGroup(m_navMenuBar);
        btnGroup->setExclusive(true);
        for (int i = 0; i < m_mainTabs->count(); i++) {
            if (i == 5) {           /* separatore prima di "Impara" */
                auto* sep = new QFrame(m_navMenuBar);
                sep->setFrameShape(QFrame::VLine);
                sep->setObjectName("navMenuSep");
                sep->setFixedWidth(1);
                nmLay->addWidget(sep);
            }
            auto* btn = new QPushButton(m_tabOrigLabels.at(i), m_navMenuBar);
            btn->setObjectName("navMenuBtn");
            btn->setCheckable(true);
            btn->setChecked(i == 0);
            btn->setFlat(true);
            const int tabIdx = i;
            connect(btn, &QPushButton::clicked, this, [this, tabIdx]{
                m_mainTabs->setCurrentIndex(tabIdx);
            });
            btnGroup->addButton(btn);
            m_navBtns << btn;
            nmLay->addWidget(btn);
        }
        nmLay->addStretch();

        /* Backend button all'estrema destra della nav bar (in menu mode) */
        if (m_btnBackend) {
            auto* backendClone = new QPushButton(m_navMenuBar);
            backendClone->setObjectName("navMenuBackend");
            backendClone->setFlat(true);
            backendClone->setFixedHeight(30);
            /* Sincronizza testo con il pulsante principale */
            auto syncText = [this, backendClone]{
                backendClone->setText(m_btnBackend->text());
                backendClone->setStyleSheet(m_btnBackend->styleSheet());
            };
            connect(m_btnBackend, &QPushButton::clicked, backendClone, syncText);
            syncText();
            connect(backendClone, &QPushButton::clicked,
                    m_btnBackend, &QPushButton::click);
            nmLay->addWidget(backendClone);
        }

        /* Sincronizza pulsante attivo quando cambia tab */
        connect(m_mainTabs, &QTabWidget::currentChanged, this, [this](int idx){
            if (idx >= 0 && idx < m_navBtns.size())
                m_navBtns[idx]->setChecked(true);
        });
    }

    wLay->addWidget(m_navMenuBar);
    wLay->addWidget(m_mainTabs, 1);

    /* Applica stile navigazione e modalità pulsanti da QSettings */
    {
        QSettings s("Prismalux", "GUI");
        applyNavStyle(s.value("nav/navStyle", "tabs_top").toString());
        /* Differito: i pulsanti di esecuzione vengono creati nelle pagine
           durante addTab(); aspettiamo che il widget tree sia completo */
        const QString execMode = s.value("nav/execBtnMode", "icon_text").toString();
        if (execMode != "icon_text")
            QTimer::singleShot(0, this, [this, execMode]{ applyExecBtnMode(execMode); });
    }

    return wrapper;
}

/* ══════════════════════════════════════════════════════════════
   ensureSettingsDialog — crea il dialog Impostazioni la prima volta (lazy).
   Sicuro da chiamare più volte (no-op se già creato).
   IMPORTANTE: nessun Qt::Window — evita il crash Windows nella
   gestione parent-child quando QDialog ha sia Qt::Window che un parent.
   ══════════════════════════════════════════════════════════════ */
void MainWindow::ensureSettingsDialog()
{
    if (m_impDlg) return;
    m_impDlg = new QDialog(this);
    m_impDlg->setWindowTitle("\xe2\x9a\x99\xef\xb8\x8f  Impostazioni \xe2\x80\x94 Prismalux");
    /* NO Qt::Window flag — QDialog default flags funzionano correttamente
       su tutte le piattaforme senza scatenare bug Windows API parent-child */
    m_impDlg->setAttribute(Qt::WA_DeleteOnClose, false);
    m_impDlg->resize(1050, 680);
    m_impPage = new ImpostazioniPage(m_ai, m_hw, m_impDlg);
    m_impPage->setGraficoCanvas(m_grafCanvas);
    auto* dl = new QVBoxLayout(m_impDlg);
    dl->setContentsMargins(0, 0, 0, 0);
    dl->addWidget(m_impPage);
    if (m_hw && m_hw->hwReady())
        m_impPage->onHWReady(m_hw->hwInfo());
    connect(m_impPage, &ImpostazioniPage::tabModeChanged,
            this,      &MainWindow::applyTabMode);
    connect(m_impPage, &ImpostazioniPage::navStyleChanged,
            this,      &MainWindow::applyNavStyle);
    connect(m_impPage, &ImpostazioniPage::execBtnModeChanged,
            this,      &MainWindow::applyExecBtnMode);
}

/* ══════════════════════════════════════════════════════════════
   applyTabMode — aggiorna le etichette di m_mainTabs in tempo reale.
   Formato originale: "icona  testo" (separatore = 2 spazi).
   ══════════════════════════════════════════════════════════════ */
void MainWindow::applyTabMode(const QString& mode)
{
    if (!m_mainTabs || m_tabOrigLabels.isEmpty()) return;
    const int n = qMin(m_mainTabs->count(), m_tabOrigLabels.size());
    for (int i = 0; i < n; i++) {
        const QString& orig = m_tabOrigLabels.at(i);
        const int sep = orig.indexOf("  ");   /* 2 spazi tra icona e testo */
        if (sep < 0) { m_mainTabs->setTabText(i, orig); continue; }
        const QString icon = orig.left(sep);
        const QString text = orig.mid(sep + 2);
        QString label;
        if      (mode == "icons_only") label = icon;
        else if (mode == "text_icons") label = text + "  " + icon;
        else if (mode == "text_only")  label = text;
        else                           label = orig;  /* icons_text = default */
        m_mainTabs->setTabText(i, label);
    }
}

/* ══════════════════════════════════════════════════════════════
   applyExecBtnMode — aggiorna il testo di tutti i pulsanti di esecuzione.
   Scansiona l'albero dei widget cercando QPushButton con proprietà execIcon.
   ══════════════════════════════════════════════════════════════ */
void MainWindow::applyExecBtnMode(const QString& mode)
{
    const auto btns = findChildren<QPushButton*>();
    for (auto* btn : btns) {
        const QVariant iconVar = btn->property("execIcon");
        if (!iconVar.isValid() || iconVar.isNull()) continue;
        const QString icon = iconVar.toString();
        const QString text = btn->property("execText").toString();
        const QString full = btn->property("execFull").toString();
        if (mode == "icon_only") btn->setText(icon);
        else if (mode == "text_only") btn->setText(text);
        else btn->setText(full.isEmpty() ? icon + "  " + text : full);
    }
}

/* ══════════════════════════════════════════════════════════════
   applyNavStyle — alterna tra schede in alto e menù orizzontale.
   ══════════════════════════════════════════════════════════════ */
void MainWindow::applyNavStyle(const QString& style)
{
    if (!m_mainTabs) return;
    const bool isMenu = (style == "menu_main");
    m_mainTabs->tabBar()->setVisible(!isMenu);
    if (m_navMenuBar) m_navMenuBar->setVisible(isMenu);
}

/* ══════════════════════════════════════════════════════════════
   Navigazione
   ══════════════════════════════════════════════════════════════ */
void MainWindow::navigateTo(int idx) {
    if (idx < 0 || idx > 6) return;
    /* Interrompe qualsiasi richiesta AI in corso prima di cambiare pagina:
       evita segnali fantasma (token/finished) che arrivano alla pagina precedente. */
    if (m_ai && m_ai->busy()) m_ai->abort();
    if (m_mainTabs) m_mainTabs->setCurrentIndex(idx);
}

/* ══════════════════════════════════════════════════════════════
   Slot hardware
   ══════════════════════════════════════════════════════════════ */
void MainWindow::onHWUpdated(SysSnapshot snap) {
    m_gCpu->update(snap.cpu_pct);
    double rp = snap.ram_total > 0 ? snap.ram_used/snap.ram_total*100.0 : 0;
    m_gRam->update(rp, QString("%1/%2 GB")
                   .arg(snap.ram_used,0,'f',1)
                   .arg(snap.ram_total,0,'f',1));

    /* Aggiorna percentuale RAM libera in AiClient (usata per throttle pre-chat) */
    double freePct = snap.ram_total > 0 ? (snap.ram_total - snap.ram_used) / snap.ram_total * 100.0 : 100.0;
    m_ai->setRamFreePct(freePct);

    /* Colora "Scarica LLM" via property QSS: normale→warn→high */
    if (m_btnUnload) {
        const char* urgency = rp > 75.0 ? "high" : rp > 40.0 ? "warn" : "";
        if (m_btnUnload->property("urgency").toString() != urgency) {
            m_btnUnload->setProperty("urgency", urgency);
            P::repolish(m_btnUnload);
        }
    }

    /* Auto-abort: se RAM >90% usata e AI occupato, interrompe subito.
     * Evita che il sistema si blocchi completamente durante l'inference. */
    if (rp > 90.0 && m_ai->busy()) {
        m_ai->abort();
        statusBar()->showMessage(
            "\xe2\x9a\xa0  RAM critica — inference AI interrotta automaticamente per proteggere il sistema.");
    }
    if (snap.gpu_ready)
        m_gGpu->update(snap.gpu_pct,
                       snap.gpu_name.isEmpty() ? "" : snap.gpu_name);
    else
        m_gGpu->update(0, "Rilevamento...");
}

/* ══════════════════════════════════════════════════════════════
   maybeAutoVramBench — benchmark VRAM automatico al primo avvio
   ══════════════════════════════════════════════════════════════ */
void MainWindow::maybeAutoVramBench() {
    /* Già eseguito in precedenza → skip */
    if (QFileInfo::exists(P::vramProfilePath())) return;

    /* Binario non compilato → skip silenzioso */
    const QString bench = P::vramBenchBin();
    if (!QFileInfo::exists(bench)) return;

    /* Solo con Ollama attivo (il benchmark interroga /api/tags) */
    if (m_ai->backend() != AiClient::Ollama) return;

    statusBar()->showMessage(
        "\xf0\x9f\x94\xac  Primo avvio: benchmark VRAM in corso (background)...");

    auto* proc = new QProcess(this);
    proc->setWorkingDirectory(P::root() + "/C_software");
    proc->setProcessChannelMode(QProcess::MergedChannels);

    connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc](int code, QProcess::ExitStatus){
        proc->deleteLater();
        if (code == 0 && QFileInfo::exists(P::vramProfilePath()))
            statusBar()->showMessage(
                "\xe2\x9c\x85  VRAM benchmark completato \xe2\x80\x94 profilo salvato in vram_profile.json");
        else
            statusBar()->showMessage(
                "\xe2\x9a\xa0  VRAM benchmark fallito (codice " +
                QString::number(code) + ") \xe2\x80\x94 verrà riprovato al prossimo avvio");

        QTimer::singleShot(6000, this, [this]{
            statusBar()->showMessage(
                "\xf0\x9f\x8d\xba  Invocazione riuscita. Gli dei ascoltano.");
        });
    });

    proc->start(bench, {});
    if (!proc->waitForStarted(3000)) {
        proc->deleteLater();
        statusBar()->showMessage(
            "\xf0\x9f\x8d\xba  Invocazione riuscita. Gli dei ascoltano.");
    }
}

/* ── Chiusura finestra — pulizia RAM residua ── */
void MainWindow::closeEvent(QCloseEvent* ev) {

    /* Se l'AI sta elaborando, chiedi se fermare e scaricare il modello */
    if (m_ai->busy()) {
        QMessageBox dlg(this);
        dlg.setWindowTitle("Prismalux — Chiusura");
        dlg.setIcon(QMessageBox::Question);
        dlg.setText("<b>Un agente AI \xc3\xa8 ancora in elaborazione.</b><br>"
                    "Vuoi fermare la generazione e scaricare il modello dalla RAM?");
        dlg.setInformativeText(
            "Modello attivo: <b>" + m_ai->model() + "</b><br>"
            "Se non lo scarichi rimarr\xc3\xa0 in memoria anche dopo la chiusura.");
        auto* btnUnload = dlg.addButton("Ferma e scarica dalla RAM", QMessageBox::AcceptRole);
        dlg.addButton("Chiudi comunque",                               QMessageBox::DestructiveRole);
        auto* btnCancel = dlg.addButton("Annulla",                     QMessageBox::RejectRole);
        dlg.setDefaultButton(btnCancel);
        dlg.exec();

        if (dlg.clickedButton() == btnCancel || dlg.clickedButton() == nullptr) {
            ev->ignore();   /* Annulla — non chiudere */
            return;
        }

        /* Ferma la generazione corrente */
        m_ai->abort();

        if (dlg.clickedButton() == btnUnload) {
            /* Ollama: DELETE /api/delete scarica il modello dalla VRAM/RAM */
            QString host  = m_ai->host();
            int     port  = m_ai->port();
            QString model = m_ai->model();
            if (!model.isEmpty()) {
                QNetworkAccessManager* nam = new QNetworkAccessManager(this);
                QNetworkRequest req(QUrl(
                    QString("http://%1:%2/api/delete").arg(host).arg(port)));
                req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
                QJsonObject body; body["name"] = model;
                QNetworkReply* reply = nam->sendCustomRequest(
                    req, "DELETE", QJsonDocument(body).toJson());
                /* Attendi al massimo 2 secondi poi chiudi comunque */
                QTimer::singleShot(2000, reply, &QNetworkReply::abort);
            }
        }
    }

    /* Ferma llama-server avviato dalla GUI (se in esecuzione) */
    if (m_serverProc && m_serverProc->state() != QProcess::NotRunning) {
        m_serverProc->terminate();
        m_serverProc->waitForFinished(3000);
    }

    /* Libera cache RAM residua (best-effort, senza dialogo) */
#ifndef Q_OS_WIN
    QProcess::startDetached("bash", {"-c",
        "ollama ps --no-trunc 2>/dev/null | awk 'NR>1{print $1}' | "
        "xargs -r -I{} ollama stop {} 2>/dev/null; "
        "sync 2>/dev/null"});
#else
    QProcess::startDetached("cmd.exe", {"/c",
        "for /f \"skip=1 tokens=1\" %m in ('ollama ps --no-trunc 2^>nul') do ollama stop %m"});
#endif

    ev->accept();
}

void MainWindow::onHWReady(HWInfo hw) {
    const HWDevice& pd = hw.dev[hw.primary];

    /* Aggiorna label GPU nell'header */
    if (pd.type != DEV_CPU) {
        QString gpuName = QString::fromLocal8Bit(pd.name);
        m_lblModel->setText(gpuName);  /* temporaneo, sovrascritto dal modello AI */
    } else {
        /* Xeon → nessun allarme */
        QString cpuName = QString::fromLocal8Bit(hw.dev[0].name);
        if (cpuName.contains("Xeon", Qt::CaseInsensitive))
            statusBar()->showMessage("✓ Xeon rilevato — elaborazione CPU ad alta performance");
    }

    /* Notifica la pagina impostazioni se già creata */
    if (m_impPage) m_impPage->onHWReady(hw);
}

/* ══════════════════════════════════════════════════════════════
   Chat History — salvataggio e lista
   ══════════════════════════════════════════════════════════════ */
void MainWindow::onChatCompleted(const QString& title, const QString& logHtml) {
    /* Se siamo già in una sessione attiva, aggiorna quella (chat continua).
       Se la sessione è vuota o non esiste più, crea una nuova voce. */
    bool sessionValid = !m_currentChatId.isEmpty()
        && !m_chatHistory.loadLog(m_currentChatId).isEmpty();

    if (sessionValid) {
        m_chatHistory.saveLog(m_currentChatId, logHtml);
    } else {
        m_currentChatId = m_chatHistory.newSession(title);
        m_chatHistory.saveLog(m_currentChatId, logHtml);
    }
    refreshChatList();
}

void MainWindow::refreshChatList() {
    if (!m_chatList) return;
    m_chatList->clear();

    const auto sessions = m_chatHistory.list();
    for (const auto& s : sessions) {
        auto* item = new QListWidgetItem(s.title.isEmpty() ? "(senza titolo)" : s.title);
        item->setData(Qt::UserRole, s.id);
        item->setToolTip(s.createdAt.toString("dd/MM/yyyy HH:mm"));
        m_chatList->addItem(item);
    }
}

void MainWindow::onPipelineStatus(int pct, const QString& text) {
    if (!m_statusProgress) return;
    if (pct < 0) {
        /* Resetta: pipeline terminata — nascondi la barra */
        m_statusProgress->setValue(0);
        m_statusProgress->setFormat("");
        m_statusProgress->setVisible(false);
        return;
    }
    m_statusProgress->setVisible(true);
    m_statusProgress->setValue(pct);
    if (!text.isEmpty()) {
        m_statusProgress->setFormat(text);
        statusBar()->showMessage(text, 8000);
    }
}
