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

namespace P = PrismaluxPaths;

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
    m_refreshBtn = new QPushButton(QString::fromUtf8("\xf0\x9f\x94\x84  Aggiorna stato"), this);
    topRow->addWidget(m_prepareBtn);
    topRow->addWidget(m_venvLbl, 1);
    topRow->addWidget(m_refreshBtn);
    root->addLayout(topRow);

    connect(m_prepareBtn, &QPushButton::clicked, this, &McpManagerPage::onPrepareVenvClicked);
    connect(m_refreshBtn, &QPushButton::clicked, this, &McpManagerPage::onRefreshClicked);

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
    m_log->setPlaceholderText(QString::fromUtf8("Log operazioni MCP\xe2\x80\xa6"));
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

        e.installBtn = new QPushButton(QString::fromUtf8("\xe2\xac\x87  Installa"), rowW);
        e.installBtn->setProperty("mcpName", name);
        e.installBtn->setEnabled(e.hasReq);
        e.installBtn->setToolTip(e.hasReq
            ? QString::fromUtf8("pip install -r requirements.txt nel venv")
            : QString::fromUtf8("Nessun requirements.txt"));

        e.testBtn = new QPushButton(QString::fromUtf8("\xf0\x9f\xa7\xaa  Testa"), rowW);
        e.testBtn->setProperty("mcpName", name);
        e.testBtn->setToolTip(QString::fromUtf8("Smoke test JSON-RPC (initialize + tools/list)"));

        row->addWidget(nameLbl);
        row->addWidget(e.statusLbl, 1);
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
        QMessageBox::information(this, QString::fromUtf8("Ambiente mancante"),
            QString::fromUtf8("Prepara prima l'ambiente isolato (pulsante \"Prepara ambiente\")."));
        return;
    }
    if (!e->hasReq) return;

    const QString req = e->dir + "/requirements.txt";
    const auto ret = QMessageBox::question(this, QString::fromUtf8("Installa dipendenze"),
        QString::fromUtf8("Installare le dipendenze di <b>%1</b> nel venv?<br><br>"
                          "Verr\xc3\xa0 eseguito:<br><code>pip install -r %2</code>")
            .arg(name, req));
    if (ret != QMessageBox::Yes) return;

    setBusy(true);
    m_busyName = name;
    if (e->statusLbl) {
        e->statusLbl->setText(QString::fromUtf8("\xe2\x8f\xb3 installazione in corso\xe2\x80\xa6"));
        e->statusLbl->setStyleSheet("color: #b26a00;");
    }
    appendLog(QString::fromUtf8("pip install -r %1 \xe2\x80\xa6").arg(req));

    m_installProc = new QProcess(this);
    m_installProc->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_installProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &McpManagerPage::onInstallFinished);
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
            e->statusLbl->setText(QString::fromUtf8("\xe2\x9c\x85 dipendenze installate"));
            e->statusLbl->setStyleSheet("color: #2e7d32;");
        }
    } else {
        appendLog(QString::fromUtf8("\xe2\x9d\x8c Installazione di %1 fallita (codice %2).")
                      .arg(m_busyName).arg(code));
        if (!out.trimmed().isEmpty())
            appendLog(P::sanitizeErrorOutput(out));
        if (e && e->statusLbl) {
            e->statusLbl->setText(QString::fromUtf8("\xe2\x9d\x8c installazione fallita"));
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
        e->statusLbl->setText(QString::fromUtf8("\xe2\x8f\xb3 test in corso\xe2\x80\xa6"));
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
    m_testProc->start(py, {e->serverPy});

    if (m_testProc->waitForStarted(3000)) {
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
            e->statusLbl->setText(QString::fromUtf8("\xe2\x9c\x85 funzionante"));
            e->statusLbl->setStyleSheet("color: #2e7d32;");
        } else {
            e->statusLbl->setText(QString::fromUtf8("\xe2\x9d\x8c non risponde"));
            e->statusLbl->setStyleSheet("color: #c62828;");
        }
    }
    appendLog((passed ? QString::fromUtf8("\xe2\x9c\x85 %1: ")
                      : QString::fromUtf8("\xe2\x9d\x8c %1: ")).arg(m_busyName) + detail);

    m_busyName.clear();
    setBusy(false);
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
