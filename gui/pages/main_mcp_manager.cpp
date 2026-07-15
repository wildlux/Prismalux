#include "main_mcp_manager.h"
#include "../prismalux_paths.h"
#include "../dpi_utils.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QScrollArea>
#include <QFrame>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QTcpSocket>
#include <QHostAddress>
#include <QCryptographicHash>
#include <QSettings>
#include <QFile>
#include <QDialog>
#include <QTextBrowser>
#include <QDialogButtonBox>

namespace P = PrismaluxPaths;

/* Calcola SHA-256 hex di un file (max 4 MB — i server.py non lo superano mai). */
static QString fileHash(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QCryptographicHash::hash(f.readAll(), QCryptographicHash::Sha256).toHex();
}

/* Istruzioni di configurazione per MCP che richiedono app esterne.
 * Restituisce HTML pronto per QTextBrowser, stringa vuota se non serve. */
static QString mcpSetupGuide(const QString& name)
{
    struct Guide { const char* prefix; const char* html; };
    static const Guide kGuides[] = {
      { "anki", "<b>Anki + AnkiConnect (porta 8765)</b><ol>"
                "<li>Apri Anki.</li>"
                "<li>Vai in <b>Strumenti \xe2\x86\x92 Add-on \xe2\x86\x92 Sfoglia e installa</b> e incolla il codice "
                "<tt>2055492159</tt> (AnkiConnect).</li>"
                "<li>Riavvia Anki — il server parte in ascolto su <tt>http://localhost:8765</tt>.</li>"
                "<li>Torna in Prismalux e premi <b>Testa</b>.</li></ol>" },
      { "blender", "<b>Blender con MCP addon (porta 6789)</b><ol>"
                   "<li>Apri Blender.</li>"
                   "<li>Vai in <b>Modifica \xe2\x86\x92 Preferenze \xe2\x86\x92 Componenti aggiuntivi</b>, "
                   "installa <tt>MCPs/blender_mcp/blender_addon.py</tt>.</li>"
                   "<li>Attiva l'addon e premi <b>Avvia Server MCP</b> nel pannello N.</li>"
                   "<li>Il server parte su <tt>http://localhost:6789</tt>.</li></ol>" },
      { "obs", "<b>OBS Studio + obs-websocket (porta 4455)</b><ol>"
               "<li>Installa OBS Studio.</li>"
               "<li>Vai in <b>Strumenti \xe2\x86\x92 obs-websocket Settings</b> e abilita il server.</li>"
               "<li>Imposta la porta su <tt>4455</tt> e un token (lascialo vuoto per test).</li>"
               "<li>Avvia OBS e premi <b>Testa</b> in Prismalux.</li></ol>" },
      { "gns3", "<b>GNS3 (porta 3080)</b><ol>"
                "<li>Installa e avvia GNS3 (gns3server oppure GNS3 GUI).</li>"
                "<li>Il server parte automaticamente su <tt>http://localhost:3080</tt>.</li>"
                "<li>Premi <b>Testa</b> in Prismalux.</li></ol>" },
      { "cytoscape", "<b>Cytoscape (porta 1234)</b><ol>"
                     "<li>Installa e avvia Cytoscape.</li>"
                     "<li>Vai in <b>Apps \xe2\x86\x92 App Manager</b> e installa <b>CyREST</b>.</li>"
                     "<li>Cytoscape esporr\xc3\xa0 l\xe2\x80\x99 API su <tt>http://localhost:1234/v1</tt>.</li></ol>" },
      { "freecad", "<b>FreeCAD con FreeCAD-MCP (porta 12345)</b><ol>"
                   "<li>Apri FreeCAD.</li>"
                   "<li>Installa la macro <tt>MCPs/freecad_mcp/freecad_macro.py</tt> "
                   "da <b>Macro \xe2\x86\x92 Gestisci macro</b>.</li>"
                   "<li>Esegui la macro \xe2\x80\x94 avvia un mini-server TCP su porta 12345.</li></ol>" },
      { "kicad", "<b>KiCAD con scripting Python (porta 8765)</b><ol>"
                 "<li>Apri KiCAD.</li>"
                 "<li>Abilita la console Python da <b>Strumenti \xe2\x86\x92 Scripting Console</b>.</li>"
                 "<li>Esegui lo script <tt>MCPs/kicad_mcp/kicad_server.py</tt> dalla console.</li></ol>" },
      { "opencode", "<b>OpenCode (porta 8092)</b><ol>"
                    "<li>Installa OpenCode: <tt>go install github.com/opencodesdev/opencode@latest</tt></li>"
                    "<li>Avvia: <tt>opencode serve --port 8092</tt></li>"
                    "<li>Oppure usa il pulsante <b>OpenCode</b> nel tab AppController.</li></ol>" },
    };
    for (const auto& g : kGuides) {
        if (name.startsWith(QLatin1String(g.prefix)))
            return QString::fromUtf8(g.html);
    }
    return {};
}

/* Per ogni MCP che richiede un'app esterna, verifica se la porta è in ascolto
 * e restituisce un suggerimento leggibile. Stringa vuota = nessun suggerimento. */
static QString mcpExternalDiag(const QString& mcpName)
{
    struct Dep { const char* prefix; const char* app; quint16 port; };
    static const Dep kDeps[] = {
        { "anki",      "Anki con AnkiConnect",   8765 },
        { "obs",       "OBS con obs-websocket",   4455 },
        { "gns3",      "GNS3",                    3080 },
        { "opencode",  "OpenCode",                8092 },
        { "blender",   "Blender con addon",       6789 },
        { "cytoscape", "Cytoscape",               1234 },
    };
    for (const auto& d : kDeps) {
        if (!mcpName.startsWith(d.prefix)) continue;
        QTcpSocket sock;
        sock.connectToHost(QHostAddress::LocalHost, d.port);
        if (!sock.waitForConnected(300)) {
            return QString::fromUtf8(
                "\nCausa probabile: %1 non \xc3\xa8 in ascolto su porta %2 "
                "\xe2\x80\x94 aprilo prima di testare l'MCP.")
                .arg(d.app).arg(d.port);
        }
        break;
    }
    return {};
}

/* ════════════════════════════════════════════════════════════════════════
 *  Helper statici
 * ════════════════════════════════════════════════════════════════════════ */

QString McpManagerPage::venvDir()
{
    return QDir::homePath() + "/.prismalux/venv";
}

QString McpManagerPage::venvPython()
{
#ifdef Q_OS_WIN
    return venvDir() + "/Scripts/python.exe";
#else
    return venvDir() + "/bin/python";
#endif
}

bool McpManagerPage::venvExists()
{
    const QFileInfo fi(venvPython());
    return fi.exists() && fi.isExecutable();
}

QStringList McpManagerPage::scanMcpServers(const QString& mcpsRoot)
{
    QStringList out;
    QDir root(mcpsRoot);
    if (!root.exists()) return out;
    const QStringList subdirs = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString& d : subdirs) {
        if (QFileInfo::exists(mcpsRoot + "/" + d + "/server.py"))
            out << d;
    }
    return out;
}

QByteArray McpManagerPage::smokeTestRequests()
{
    /* Una richiesta JSON-RPC 2.0 per riga (gli MCP leggono via stdin.readline). */
    QByteArray b;
    b += "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{"
         "\"protocolVersion\":\"2024-11-05\",\"capabilities\":{},"
         "\"clientInfo\":{\"name\":\"prismalux-smoketest\",\"version\":\"1.0\"}}}\n";
    b += "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/list\",\"params\":{}}\n";
    return b;
}

bool McpManagerPage::smokeTestPassed(const QByteArray& stdoutData)
{
    /* Passa se almeno una riga è un JSON-RPC valido con campo "result". */
    const QList<QByteArray> lines = stdoutData.split('\n');
    for (const QByteArray& ln : lines) {
        const QByteArray t = ln.trimmed();
        if (t.isEmpty()) continue;
        QJsonParseError e{};
        const QJsonDocument doc = QJsonDocument::fromJson(t, &e);
        if (e.error != QJsonParseError::NoError || !doc.isObject()) continue;
        const QJsonObject o = doc.object();
        if (o.value("jsonrpc").toString() == "2.0" && o.contains("result"))
            return true;
    }
    return false;
}

/* ════════════════════════════════════════════════════════════════════════
 *  Costruzione UI
 * ════════════════════════════════════════════════════════════════════════ */

McpManagerPage::McpManagerPage(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
    rebuildList();
    refreshStatuses();
    updateVenvLabel();
}

McpManagerPage::~McpManagerPage()
{
    /* D-21 (stesso bug di D-20 in VoiceClonerWidget): m_venvProc/
     * m_installProc/m_testProc e i QProcess anonimi del test "mcp_call"
     * (creati con parent=this in onTestMcpCallClicked()) sono tutti figli
     * di questa pagina, con finished()/errorOccurred() connessi a slot o
     * lambda che toccano appendLog()/setBusy()/label di stato. Se la
     * pagina viene distrutta mentre uno di questi è ancora in esecuzione
     * (probe pip/venv possono durare secondi), il suo ~QProcess emette
     * finished() sincrono via waitForFinished() interno PRIMA che il
     * chiamante possa verificare che i widget UI siano ancora validi →
     * SEGV. findChildren<QProcess*>() è ricorsivo: copre tutti e 4 i
     * punti di creazione senza doverli elencare singolarmente. */
    const auto procs = findChildren<QProcess*>();
    for (QProcess* p : procs) {
        p->disconnect(this);
        p->blockSignals(true);
        if (p->state() != QProcess::NotRunning) {
            p->kill();
            p->waitForFinished(1000);
        }
    }
}

void McpManagerPage::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(dpiScale(12), dpiScale(12), dpiScale(12), dpiScale(12));
    root->setSpacing(dpiScale(8));

    auto* title = new QLabel(QString::fromUtf8(
        "\xf0\x9f\x94\x8c  Gestione MCP"), this);
    QFont tf = title->font();
    tf.setPointSize(tf.pointSize() + 3);
    tf.setBold(true);
    title->setFont(tf);
    root->addWidget(title);

    auto* desc = new QLabel(QString::fromUtf8(
        "Installa le dipendenze in un ambiente isolato (venv) e verifica che ogni "
        "plugin risponda al protocollo JSON-RPC 2.0. Niente pacchetti nel Python di sistema."), this);
    desc->setWordWrap(true);
    desc->setStyleSheet("color: palette(mid);");
    root->addWidget(desc);

    /* Riga venv + azioni globali */
    auto* topRow = new QHBoxLayout;
    m_prepareBtn = new QPushButton(QString::fromUtf8("\xf0\x9f\x90\x8d  Prepara ambiente (venv)"), this);
    m_venvLbl    = new QLabel(this);
    m_venvLbl->setWordWrap(true);
    m_refreshBtn  = new QPushButton(QString::fromUtf8("\xf0\x9f\x94\x84  Aggiorna stato"), this);
    m_testAllBtn  = new QPushButton(QString::fromUtf8("\xf0\x9f\xa7\xaa  Testa tutti"), this);
    m_testAllBtn->setToolTip(QString::fromUtf8(
        "Esegue lo smoke test JSON-RPC su tutti i plugin MCP in sequenza.\n"
        "I risultati compaiono nel log centralizzato sottostante."));
    m_testMcpCallBtn = new QPushButton(QString::fromUtf8("\xf0\x9f\xa7\xaa  Testa mcp_call"), this);
    m_testMcpCallBtn->setToolTip(QString::fromUtf8(
        "Verifica che tutti i plugin siano raggiungibili con la stessa risoluzione\n"
        "python usata da mcp_call: venv plugin \xe2\x86\x92 venv condiviso \xe2\x86\x92 python3.\n"
        "Usa questo test dopo aver aggiunto nuovi plugin o cambiato il venv."));
    topRow->addWidget(m_prepareBtn);
    topRow->addWidget(m_venvLbl, 1);
    topRow->addWidget(m_testAllBtn);
    topRow->addWidget(m_testMcpCallBtn);
    topRow->addWidget(m_refreshBtn);
    root->addLayout(topRow);

    connect(m_prepareBtn,     &QPushButton::clicked, this, &McpManagerPage::onPrepareVenvClicked);
    connect(m_refreshBtn,     &QPushButton::clicked, this, &McpManagerPage::onRefreshClicked);
    connect(m_testAllBtn,     &QPushButton::clicked, this, &McpManagerPage::onTestAllClicked);
    connect(m_testMcpCallBtn, &QPushButton::clicked, this, &McpManagerPage::onTestMcpCallClicked);

    /* Lista MCP in area scrollabile */
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    auto* listHost = new QWidget(scroll);
    m_listLay = new QVBoxLayout(listHost);
    m_listLay->setContentsMargins(0, 0, 0, 0);
    m_listLay->setSpacing(dpiScale(4));
    m_listLay->addStretch(1);
    scroll->setWidget(listHost);
    root->addWidget(scroll, 1);

    /* Log */
    m_log = new QTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setFixedHeight(dpiScale(140));
    m_log->setPlaceholderText(tr("Log operazioni MCP\xe2\x80\xa6"));
    root->addWidget(m_log);
}

void McpManagerPage::rebuildList()
{
    m_entries.clear();
    const QString mcpsRoot = P::root() + "/MCPs";
    const QStringList names = scanMcpServers(mcpsRoot);

    for (const QString& name : names) {
        McpEntry e;
        e.name     = name;
        e.dir      = mcpsRoot + "/" + name;
        e.serverPy = e.dir + "/server.py";
        e.hasReq   = QFileInfo::exists(e.dir + "/requirements.txt");

        auto* rowW = new QFrame;
        rowW->setFrameShape(QFrame::StyledPanel);
        auto* row = new QHBoxLayout(rowW);
        row->setContentsMargins(dpiScale(8), dpiScale(4), dpiScale(8), dpiScale(4));

        auto* nameLbl = new QLabel(name, rowW);
        nameLbl->setMinimumWidth(dpiScale(160));
        QFont nf = nameLbl->font();
        nf.setBold(true);
        nameLbl->setFont(nf);

        e.statusLbl = new QLabel(rowW);
        e.statusLbl->setWordWrap(true);

        /* Verifica integrità SHA-256 del server.py */
        e.hashLbl = new QLabel(rowW);
        e.hashLbl->setToolTip(QString::fromUtf8(
            "SHA-256 di server.py. Verde = invariato dal primo avvio; "
            "Arancio = hash sconosciuto; Rosso = file modificato."));
        {
            const QString currentHash = fileHash(e.serverPy);
            const QString settingsKey = QLatin1String("mcp_hash/") + name;
            QSettings ss("Prismalux", "GUI");
            if (currentHash.isEmpty()) {
                e.hashLbl->setText(tr("\xe2\x9d\x93"));
                e.hashLbl->setStyleSheet("color: palette(mid);");
            } else {
                const QString stored = ss.value(settingsKey).toString();
                if (stored.isEmpty()) {
                    ss.setValue(settingsKey, currentHash);
                    e.hashLbl->setText(tr("\xf0\x9f\x93\x9d"));
                    e.hashLbl->setStyleSheet("color: #b26a00;");
                } else if (stored == currentHash) {
                    e.hashLbl->setText(tr("\xe2\x9c\x85"));
                    e.hashLbl->setStyleSheet("color: #2e7d32;");
                } else {
                    e.hashLbl->setText(tr("\xe2\x9a\xa0\xef\xb8\x8f"));
                    e.hashLbl->setStyleSheet("color: #c62828;");
                    appendLog(QString::fromUtf8("\xe2\x9a\xa0\xef\xb8\x8f <b>") + name +
                              QString::fromUtf8("</b>: server.py modificato (hash cambiato dal primo avvio)!"));
                }
            }
        }

        e.installBtn = new QPushButton(QString::fromUtf8("\xe2\xac\x87  Installa"), rowW);
        e.installBtn->setProperty("mcpName", name);
        e.installBtn->setEnabled(e.hasReq);
        e.installBtn->setToolTip(e.hasReq
            ? QString::fromUtf8("pip install -r requirements.txt nel venv")
            : QString::fromUtf8("Nessun requirements.txt"));

        e.testBtn = new QPushButton(QString::fromUtf8("\xf0\x9f\xa7\xaa  Testa"), rowW);
        e.testBtn->setProperty("mcpName", name);
        e.testBtn->setToolTip(tr("Smoke test JSON-RPC (initialize + tools/list)"));

        row->addWidget(nameLbl);
        row->addWidget(e.statusLbl, 1);
        row->addWidget(e.hashLbl);

        /* Pulsante guida configurazione (solo per MCP con app esterne) */
        const QString guide = mcpSetupGuide(name);
        if (!guide.isEmpty()) {
            auto* guideBtn = new QPushButton(QString::fromUtf8("\xe2\x84\xb9"), rowW);
            guideBtn->setFlat(true);
            guideBtn->setFixedWidth(dpiScale(28));
            guideBtn->setToolTip(QString::fromUtf8(
                "Mostra istruzioni di configurazione per questo MCP"));
            guideBtn->setProperty("mcpName", name);
            guideBtn->setProperty("mcpGuide", guide);
            connect(guideBtn, &QPushButton::clicked,
                    this, &McpManagerPage::onMcpGuideClicked);
            row->addWidget(guideBtn);
        }

        row->addWidget(e.installBtn);
        row->addWidget(e.testBtn);

        /* inserisci prima dello stretch finale */
        m_listLay->insertWidget(m_listLay->count() - 1, rowW);

        connect(e.installBtn, &QPushButton::clicked, this, &McpManagerPage::onInstallClicked);
        connect(e.testBtn,    &QPushButton::clicked, this, &McpManagerPage::onTestClicked);

        m_entries.push_back(e);
    }

    if (m_entries.isEmpty())
        appendLog(QString::fromUtf8("\xe2\x9a\xa0\xef\xb8\x8f Nessun MCP con server.py trovato in ") + mcpsRoot);
    else
        appendLog(QString::fromUtf8("Trovati %1 plugin MCP.").arg(m_entries.size()));
}

void McpManagerPage::refreshStatuses()
{
    for (McpEntry& e : m_entries) {
        if (!e.statusLbl) continue;
        QString s;
        s += e.hasReq ? QString::fromUtf8("requirements \xe2\x9c\x93")
                      : QString::fromUtf8("nessun requirements");
        e.statusLbl->setText(s);
        e.statusLbl->setStyleSheet("color: palette(mid);");
    }
}

void McpManagerPage::updateVenvLabel()
{
    if (!m_venvLbl) return;
    if (venvExists()) {
        m_venvLbl->setText(QString::fromUtf8("\xe2\x9c\x85 Ambiente pronto: ") + venvDir());
        m_venvLbl->setStyleSheet("color: #2e7d32;");
    } else {
        m_venvLbl->setText(QString::fromUtf8(
            "\xe2\x9a\xa0\xef\xb8\x8f Ambiente non creato \xe2\x80\x94 premi \"Prepara ambiente\"."));
        m_venvLbl->setStyleSheet("color: #b26a00;");
    }
}

/* ════════════════════════════════════════════════════════════════════════
 *  venv
 * ════════════════════════════════════════════════════════════════════════ */

void McpManagerPage::onPrepareVenvClicked()
{
    if (m_busy) return;
    if (venvExists()) {
        appendLog(QString::fromUtf8("Ambiente venv gi\xc3\xa0 presente."));
        updateVenvLabel();
        return;
    }
    setBusy(true);
    appendLog(QString::fromUtf8("Creo l'ambiente isolato in ") + venvDir() + QString::fromUtf8(" \xe2\x80\xa6"));
    QDir().mkpath(QDir::homePath() + "/.prismalux");

    m_venvProc = new QProcess(this);
    m_venvProc->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_venvProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &McpManagerPage::onVenvProcFinished);
    connect(m_venvProc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError err) {
        if (err == QProcess::FailedToStart)
            qWarning() << "[main_mcp_manager] m_venvProc non avviato:" << m_venvProc->program();
    });
    m_venvProc->start(P::findPython(), {"-m", "venv", venvDir()});
}

void McpManagerPage::onVenvProcFinished(int code, QProcess::ExitStatus)
{
    const QString out = m_venvProc ? QString::fromUtf8(m_venvProc->readAll()) : QString();
    if (m_venvProc) { m_venvProc->deleteLater(); m_venvProc = nullptr; }
    setBusy(false);

    if (code == 0 && venvExists()) {
        appendLog(QString::fromUtf8("\xe2\x9c\x85 Ambiente venv creato."));
    } else {
        appendLog(QString::fromUtf8("\xe2\x9d\x8c Creazione venv fallita (codice %1).").arg(code));
        if (!out.trimmed().isEmpty())
            appendLog(P::sanitizeErrorOutput(out));
    }
    updateVenvLabel();
}

/* ════════════════════════════════════════════════════════════════════════
 *  Installazione dipendenze
 * ════════════════════════════════════════════════════════════════════════ */

void McpManagerPage::onInstallClicked()
{
    if (m_busy) return;
    auto* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    const QString name = btn->property("mcpName").toString();
    McpEntry* e = entryByName(name);
    if (!e) return;

    if (!venvExists()) {
        QMessageBox::information(this, tr("Ambiente mancante"),
            QString::fromUtf8("Prepara prima l'ambiente isolato (pulsante \"Prepara ambiente\")."));
        return;
    }
    if (!e->hasReq) return;

    const QString req = e->dir + "/requirements.txt";
    const auto ret = QMessageBox::question(this, tr("Installa dipendenze"),
        QString::fromUtf8("Installare le dipendenze di <b>%1</b> nel venv?<br><br>"
                          "Verr\xc3\xa0 eseguito:<br><code>pip install -r %2</code>")
            .arg(name, req));
    if (ret != QMessageBox::Yes) return;

    setBusy(true);
    m_busyName = name;
    if (e->statusLbl) {
        e->statusLbl->setText(tr("\xe2\x8f\xb3 installazione in corso\xe2\x80\xa6"));
        e->statusLbl->setStyleSheet("color: #b26a00;");
    }
    appendLog(QString::fromUtf8("pip install -r %1 \xe2\x80\xa6").arg(req));

    m_installProc = new QProcess(this);
    m_installProc->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_installProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &McpManagerPage::onInstallFinished);
    connect(m_installProc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError err) {
        if (err == QProcess::FailedToStart)
            qWarning() << "[main_mcp_manager] m_installProc non avviato:" << m_installProc->program();
    });
    m_installProc->start(venvPython(),
        {"-m", "pip", "install", "-r", req});
}

void McpManagerPage::onInstallFinished(int code, QProcess::ExitStatus)
{
    const QString out = m_installProc ? QString::fromUtf8(m_installProc->readAll()) : QString();
    if (m_installProc) { m_installProc->deleteLater(); m_installProc = nullptr; }

    McpEntry* e = entryByName(m_busyName);
    if (code == 0) {
        appendLog(QString::fromUtf8("\xe2\x9c\x85 Dipendenze di %1 installate.").arg(m_busyName));
        if (e && e->statusLbl) {
            e->statusLbl->setText(tr("\xe2\x9c\x85 dipendenze installate"));
            e->statusLbl->setStyleSheet("color: #2e7d32;");
        }
    } else {
        appendLog(QString::fromUtf8("\xe2\x9d\x8c Installazione di %1 fallita (codice %2).")
                      .arg(m_busyName).arg(code));
        if (!out.trimmed().isEmpty())
            appendLog(P::sanitizeErrorOutput(out));
        if (e && e->statusLbl) {
            e->statusLbl->setText(tr("\xe2\x9d\x8c installazione fallita"));
            e->statusLbl->setStyleSheet("color: #c62828;");
        }
    }
    m_busyName.clear();
    setBusy(false);
}

/* ════════════════════════════════════════════════════════════════════════
 *  Smoke test JSON-RPC
 * ════════════════════════════════════════════════════════════════════════ */

void McpManagerPage::onTestClicked()
{
    if (m_busy) return;
    auto* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    const QString name = btn->property("mcpName").toString();
    McpEntry* e = entryByName(name);
    if (!e || e->serverPy.isEmpty()) return;

    setBusy(true);
    m_busyName = name;
    m_testBuf.clear();
    if (e->statusLbl) {
        e->statusLbl->setText(tr("\xe2\x8f\xb3 test in corso\xe2\x80\xa6"));
        e->statusLbl->setStyleSheet("color: #b26a00;");
    }
    appendLog(QString::fromUtf8("Smoke test JSON-RPC su %1 \xe2\x80\xa6").arg(name));

    const QString py = venvExists() ? venvPython() : P::findPython();

    m_testProc = new QProcess(this);
    m_testProc->setProcessChannelMode(QProcess::SeparateChannels);
    m_testProc->setWorkingDirectory(e->dir);
    connect(m_testProc, &QProcess::readyReadStandardOutput,
            this, &McpManagerPage::onTestReadyRead);
    connect(m_testProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &McpManagerPage::onTestFinished);
    connect(m_testProc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError err) {
        if (err == QProcess::FailedToStart)
            qWarning() << "[main_mcp_manager] m_testProc non avviato:" << m_testProc->program();
    });
    m_testProc->start(py, {e->serverPy});

    if (m_testProc->waitForStarted(P::kProcessStartTimeoutMs)) {
        m_testProc->write(smokeTestRequests());
    } else {
        finishCurrentTest(false, QString::fromUtf8("Impossibile avviare il server (python o file mancante)."));
        return;
    }

    QTimer::singleShot(6000, this, &McpManagerPage::onTestTimeout);
}

void McpManagerPage::onTestReadyRead()
{
    if (!m_testProc) return;
    m_testBuf += m_testProc->readAllStandardOutput();
    /* Se ha già risposto correttamente, concludi subito senza aspettare il timeout. */
    if (smokeTestPassed(m_testBuf))
        finishCurrentTest(true, QString::fromUtf8("Risponde a initialize + tools/list."));
}

void McpManagerPage::onTestTimeout()
{
    if (!m_testProc) return;  // già concluso
    const bool ok = smokeTestPassed(m_testBuf);
    finishCurrentTest(ok, ok
        ? QString::fromUtf8("Risponde a initialize + tools/list.")
        : QString::fromUtf8("Nessuna risposta JSON-RPC valida entro 6s."));
}

void McpManagerPage::onTestFinished(int code, QProcess::ExitStatus)
{
    if (!m_testProc) return;  // già concluso
    /* Il processo è uscito da solo: un MCP sano resterebbe in ascolto, quindi
       un'uscita precoce indica di solito un errore (es. modulo mancante). */
    const QString err = m_testProc ? QString::fromUtf8(m_testProc->readAllStandardError()) : QString();
    const bool ok = smokeTestPassed(m_testBuf);
    QString detail;
    if (ok) {
        detail = QString::fromUtf8("Risponde a initialize + tools/list.");
    } else {
        detail = QString::fromUtf8("Il server è uscito (codice %1) senza rispondere.").arg(code);
        if (!err.trimmed().isEmpty())
            detail += "\n" + P::sanitizeErrorOutput(err, 12);
        detail += mcpExternalDiag(m_busyName);
    }
    finishCurrentTest(ok, detail);
}

void McpManagerPage::finishCurrentTest(bool passed, const QString& detail)
{
    if (!m_testProc) return;  // guard: una sola conclusione

    if (m_testProc->state() != QProcess::NotRunning) {
        m_testProc->kill();
        m_testProc->waitForFinished(1000);
    }
    m_testProc->deleteLater();
    m_testProc = nullptr;

    McpEntry* e = entryByName(m_busyName);
    if (e && e->statusLbl) {
        if (passed) {
            e->statusLbl->setText(tr("\xe2\x9c\x85 funzionante"));
            e->statusLbl->setStyleSheet("color: #2e7d32;");
        } else {
            e->statusLbl->setText(tr("\xe2\x9d\x8c non risponde"));
            e->statusLbl->setStyleSheet("color: #c62828;");
        }
    }
    appendLog((passed ? QString::fromUtf8("\xe2\x9c\x85 %1: ")
                      : QString::fromUtf8("\xe2\x9d\x8c %1: ")).arg(m_busyName) + detail);

    m_busyName.clear();
    setBusy(false);

    /* Se c'è una coda "Testa tutti" in corso, avanza al prossimo */
    advanceTestQueue();
}

void McpManagerPage::onTestAllClicked()
{
    if (m_busy) {
        appendLog(QString::fromUtf8("Operazione in corso, attendi\xe2\x80\xa6"));
        return;
    }
    m_testQueue.clear();
    for (const McpEntry& e : m_entries)
        m_testQueue.append(e.name);

    if (m_testQueue.isEmpty()) {
        appendLog(QString::fromUtf8("Nessun MCP trovato."));
        return;
    }
    const QString ts = QDateTime::currentDateTime().toString("HH:mm:ss");
    appendLog(QString::fromUtf8("[%1] \xf0\x9f\xa7\xaa Inizio test batch: %2 plugin\xe2\x80\xa6")
              .arg(ts).arg(m_testQueue.size()));
    advanceTestQueue();
}

void McpManagerPage::advanceTestQueue()
{
    if (m_testQueue.isEmpty()) {
        if (m_testAllBtn) m_testAllBtn->setEnabled(true);
        return;
    }
    if (m_testAllBtn) m_testAllBtn->setEnabled(false);

    const QString name = m_testQueue.takeFirst();
    McpEntry* e = entryByName(name);
    if (!e || e->testBtn == nullptr) {
        advanceTestQueue();  /* salta MCP senza testBtn, prova il prossimo */
        return;
    }
    /* Simula il click del tasto "Testa" per quell'MCP */
    e->testBtn->click();
}

/* ════════════════════════════════════════════════════════════════════════
 *  Utility
 * ════════════════════════════════════════════════════════════════════════ */

void McpManagerPage::onRefreshClicked()
{
    refreshStatuses();
    updateVenvLabel();
    appendLog(QString::fromUtf8("Stato aggiornato."));
}

McpEntry* McpManagerPage::entryByName(const QString& name)
{
    for (McpEntry& e : m_entries)
        if (e.name == name) return &e;
    return nullptr;
}

void McpManagerPage::setBusy(bool busy)
{
    m_busy = busy;
    if (m_prepareBtn) m_prepareBtn->setEnabled(!busy);
    if (m_refreshBtn) m_refreshBtn->setEnabled(!busy);
    for (McpEntry& e : m_entries) {
        if (e.installBtn) e.installBtn->setEnabled(!busy && e.hasReq);
        if (e.testBtn)    e.testBtn->setEnabled(!busy);
    }
}

void McpManagerPage::appendLog(const QString& text)
{
    if (!m_log) return;
    const QString ts = QDateTime::currentDateTime().toString("HH:mm:ss");
    m_log->append("[" + ts + "] " + text);
}

void McpManagerPage::onMcpGuideClicked()
{
    auto* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    const QString name  = btn->property("mcpName").toString();
    const QString guide = btn->property("mcpGuide").toString();
    if (guide.isEmpty()) return;

    auto* dlg = new QDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle(name + QString::fromUtf8(" \xe2\x80\x94 Configurazione"));
    dlg->resize(dpiScale(480), dpiScale(320));

    auto* lay = new QVBoxLayout(dlg);
    auto* tb = new QTextBrowser(dlg);
    tb->setOpenExternalLinks(false);
    tb->setHtml("<html><body style='font-size:13px;'>" + guide + "</body></html>");
    lay->addWidget(tb);

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok, dlg);
    connect(bb, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
    lay->addWidget(bb);

    dlg->open();
}

/* ── Testa mcp_call ───────────────────────────────────────────────────────
 * Usa la stessa risoluzione python di mcp_call (venv-plugin → venv-condiviso
 * → python3) per avviare ogni server e inviare initialize + tools/list.
 * Verifica che il path python scelto sia corretto prima ancora di chiamare
 * il tool in chat. */
void McpManagerPage::onTestMcpCallClicked()
{
    if (m_busy) return;
    const QString mcpsRoot = P::root() + "/MCPs";
    const QStringList names = scanMcpServers(mcpsRoot);
    if (names.isEmpty()) {
        appendLog(QString::fromUtf8("\xf0\x9f\xa7\xaa  Testa mcp_call: nessun plugin trovato."));
        return;
    }
    appendLog(QString::fromUtf8("\xf0\x9f\xa7\xaa  Testa mcp_call — avvio test su %1 plugin...")
              .arg(names.size()));
    setBusy(true);
    if (m_testMcpCallBtn) m_testMcpCallBtn->setEnabled(false);

    /* Usa QTimer::singleShot a cascata per eseguire i test in sequenza
       senza bloccare il thread UI. */
    struct Runner {
        McpManagerPage* page;
        QStringList     queue;
        QString         mcpsRoot;
        int             passed = 0, failed = 0;

        void next() {
            if (queue.isEmpty()) {
                page->appendLog(
                    QString::fromUtf8(
                        "\xf0\x9f\xa7\xaa  mcp_call test completato: "
                        "<span style='color:#22c55e;'>%1 OK</span>  "
                        "<span style='color:#ef4444;'>%2 KO</span>")
                    .arg(passed).arg(failed));
                page->setBusy(false);
                if (page->m_testMcpCallBtn) page->m_testMcpCallBtn->setEnabled(true);
                delete this;
                return;
            }
            const QString name = queue.takeFirst();
            const QString serverPy = mcpsRoot + "/" + name + "/server.py";
            /* Stessa priorità venv di mcp_call */
            const QString venvPy     = mcpsRoot + "/" + name + "/venv/bin/python";
            const QString sharedVenv = QDir::homePath() + "/.prismalux/venv/bin/python";
            const QString pyExe = QFile::exists(venvPy)    ? venvPy
                                : QFile::exists(sharedVenv) ? sharedVenv
                                : "python3";

            auto* proc = new QProcess(page);
            proc->setProgram(pyExe);
            proc->setArguments({serverPy});
            proc->setProcessChannelMode(QProcess::SeparateChannels);
            proc->start();
            QObject::connect(proc, &QProcess::errorOccurred, page, [proc](QProcess::ProcessError err) {
                if (err == QProcess::FailedToStart)
                    qWarning() << "[main_mcp_manager] Runner proc non avviato:" << proc->program();
            });
            if (!proc->waitForStarted(P::kProcessStartTimeoutMs)) {
                if (proc->state() != QProcess::NotRunning) proc->kill();
                proc->deleteLater();
                page->appendLog(
                    QString::fromUtf8("  \xe2\x9d\x8c [mcp_call] %1 — avvio fallito (%2)")
                    .arg(name, pyExe));
                ++failed;
                QTimer::singleShot(0, page, [this]{ next(); });
                return;
            }
            proc->write(McpManagerPage::smokeTestRequests());
            proc->closeWriteChannel();

            /* Timeout 8s per plugin (test leggero, nessuna tools/call) */
            auto* timer = new QTimer(proc);
            timer->setSingleShot(true);
            timer->start(8000);

            connect(timer, &QTimer::timeout, proc, [proc]{ proc->kill(); });

            connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                    proc, [this, proc, name, pyExe](int, QProcess::ExitStatus) {
                proc->deleteLater();
                const bool ok = McpManagerPage::smokeTestPassed(proc->readAllStandardOutput());
                if (ok) {
                    page->appendLog(
                        QString::fromUtf8("  \xe2\x9c\x85 [mcp_call] %1 (%2)").arg(name, pyExe));
                    ++passed;
                } else {
                    const QString diag = mcpExternalDiag(name);
                    page->appendLog(
                        QString::fromUtf8("  \xe2\x9d\x8c [mcp_call] %1 — nessuna risposta%2")
                        .arg(name, diag));
                    ++failed;
                }
                QTimer::singleShot(0, page, [this]{ next(); });
            });
        }
    };

    auto* runner = new Runner{ this, names, mcpsRoot };
    runner->next();
}
