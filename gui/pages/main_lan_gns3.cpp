/* ======================================================================
   main_lan_gns3.cpp — GNS3 MCP e ADB di LanWanPage
   Estratto da main_lan_wan.cpp per ridurne le dimensioni.
   ====================================================================== */
#include "main_lan_wan.h"
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

/* ══════════════════════════════════════════════════════════════
   GNS3 MCP — simulatore reti via REST API localhost:3080
   ══════════════════════════════════════════════════════════════ */
namespace {
static const char* kGNS3Sys[] = {
    /* CRITICO: ogni nodo DEVE avere compute_id='local' altrimenti la API
       risponde 400. I nodi built-in (vpcs, ethernet_switch, cloud, nat,
       ethernet_hub) non richiedono template_id. */
    "Sei un esperto di reti GNS3 e Python. "
    "Genera SOLO codice Python che usa la GNS3 REST API v2 (localhost:3080). "
    "import requests; BASE='http://localhost:3080/v2'. "
    "REGOLA CRITICA: ogni POST /nodes DEVE includere compute_id='local'. "
    "Esempio nodo: {\"name\":\"SW1\",\"node_type\":\"ethernet_switch\",\"compute_id\":\"local\",\"x\":0,\"y\":0}. "
    "Esempio VPCS: {\"name\":\"PC1\",\"node_type\":\"vpcs\",\"compute_id\":\"local\",\"x\":-150,\"y\":100}. "
    "Usa GET/POST/DELETE su /projects, /nodes, /links. "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```.",

    "Sei un esperto di GNS3 e routing. "
    "Genera SOLO codice Python GNS3 REST API per creare una topologia LAN con switch e PC VPCS. "
    "REGOLA CRITICA: ogni POST /nodes DEVE avere compute_id='local'. "
    "Usa node_type='ethernet_switch' per switch e node_type='vpcs' per host. "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```.",

    "Sei un esperto di GNS3 e firewall. "
    "Genera SOLO codice Python GNS3 REST API per configurare un firewall (pfSense o Cisco ASA). "
    "REGOLA CRITICA: ogni POST /nodes DEVE avere compute_id='local'. "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```.",

    "Sei un esperto di GNS3. "
    "Genera SOLO codice Python per analizzare la topologia di rete attiva via REST API: "
    "lista nodi, link, stato interfacce. Rispondi SOLO con codice Python tra ``` e ```.",

    "Sei un esperto di GNS3. "
    "Genera SOLO codice Python GNS3 REST API libero. "
    "REGOLA CRITICA: ogni POST /nodes DEVE avere compute_id='local'. "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```.",

    nullptr
};
static const char* kGNS3Actions[] = {
    "\xf0\x9f\x8c\x90  Nuova topologia",
    "\xf0\x9f\x93\xa1  Router & Switch LAN",
    "\xf0\x9f\x94\x92  Configura Firewall",
    "\xf0\x9f\x94\x8d  Analizza topologia",
    "\xf0\x9f\x90\x8d  Script libero",
    nullptr
};
} // namespace


void LanWanPage::gns3RunAi(const QString& sys, const QString& userMsg)
{
    if (m_ai->busy()) {
        m_gns3Output->append("\xe2\x9a\xa0  AI occupata, attendi o premi Stop.");
        return;
    }
    if (userMsg.trimmed().isEmpty()) {
        m_gns3Output->append("\xe2\x9a\xa0  Inserisci la richiesta prima di eseguire.");
        return;
    }

    if (m_gns3Model && m_gns3Model->count() > 0) {
        const QString sel = m_gns3Model->currentData().toString();
        if (!sel.isEmpty() && sel != m_ai->model())
            m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), sel);
    }

    m_gns3AiActive = true;
    m_gns3RunBtn->setEnabled(false);
    m_gns3StopBtn->setEnabled(true);
    m_gns3Output->append(
        "\n\xf0\x9f\x94\x84  Generazione in corso...\n"
        + QString(40, QChar(0x2500)));

    delete m_gns3TokenHolder;
    m_gns3TokenHolder = new QObject(this);

    connect(m_ai, &AiClient::token,    this, &LanWanPage::onGns3AiToken);
    connect(m_ai, &AiClient::finished, this, &LanWanPage::onGns3AiFinished);
    connect(m_ai, &AiClient::error,    this, &LanWanPage::onGns3AiError);

    m_ai->chat(sys, userMsg);
}

/* ══════════════════════════════════════════════════════════════
   Slots — GNS3 AI
   ══════════════════════════════════════════════════════════════ */
void LanWanPage::onGns3AiToken(const QString& t)
{
    m_gns3Output->moveCursor(QTextCursor::End);
    m_gns3Output->insertPlainText(t);
}

void LanWanPage::onGns3AiFinished(const QString& full)
{
    disconnect(m_ai, &AiClient::token,    this, &LanWanPage::onGns3AiToken);
    disconnect(m_ai, &AiClient::finished, this, &LanWanPage::onGns3AiFinished);
    disconnect(m_ai, &AiClient::error,    this, &LanWanPage::onGns3AiError);
    m_gns3AiActive = false;
    m_gns3RunBtn->setEnabled(true);
    m_gns3StopBtn->setEnabled(false);
    m_gns3Output->append("\n" + QString(40, QChar(0x2500)));
    delete m_gns3TokenHolder;
    m_gns3TokenHolder = nullptr;

    static auto extract = [](const QString& text) -> QString {
        int start = text.indexOf("```python");
        if (start != -1) {
            start = text.indexOf('\n', start) + 1;
            int end = text.indexOf("```", start);
            if (end != -1) return text.mid(start, end - start).trimmed();
        }
        start = text.indexOf("```");
        if (start != -1) {
            start += 3;
            const int nl = text.indexOf('\n', start);
            if (nl != -1) {
                start = nl + 1;
                int end = text.indexOf("```", start);
                if (end != -1) return text.mid(start, end - start).trimmed();
            }
        }
        return {};
    };

    if (full.contains("```")) {
        const QString code = extract(full);
        if (!code.isEmpty()) {
            m_gns3Code = code;
            m_gns3ExecBtn->setEnabled(true);
            m_gns3StatusLbl->setText(
                tr("\xf0\x9f\x8c\x90  Codice pronto \xe2\x80\x94 premi Esegui su GNS3"));
        }
    }
}

void LanWanPage::onGns3AiError(const QString& msg)
{
    disconnect(m_ai, &AiClient::token,    this, &LanWanPage::onGns3AiToken);
    disconnect(m_ai, &AiClient::finished, this, &LanWanPage::onGns3AiFinished);
    disconnect(m_ai, &AiClient::error,    this, &LanWanPage::onGns3AiError);

    const QString sys     = (m_gns3Action && m_gns3Action->currentIndex() >= 0)
                            ? QString::fromUtf8(kGNS3Sys[m_gns3Action->currentIndex()])
                            : QString();
    const QString userMsg = m_gns3Input ? m_gns3Input->toPlainText() : QString();

    m_gns3AiActive = false;
    m_gns3RunBtn->setEnabled(true);
    m_gns3StopBtn->setEnabled(false);
    delete m_gns3TokenHolder;
    m_gns3TokenHolder = nullptr;
    m_gns3ErrorPanel->showError(msg, [this, sys, userMsg]{
        gns3RunAi(sys, userMsg);
    });
    LogBus::post("\xe2\x9d\x8c LAN WAN: GNS3 errore AI: " + msg);
}

/* ══════════════════════════════════════════════════════════════
   Slots — GNS3 ping
   ══════════════════════════════════════════════════════════════ */
void LanWanPage::onPingBtnClicked()
{
    const QString addr = m_gns3HostEdit->text().trimmed();
    const QString host = addr.contains(':') ? addr.section(':', 0, 0) : addr;
    const int port = addr.contains(':') ? addr.section(':', 1).toInt() : 3080;
    m_gns3StatusLbl->setText(tr("\xf0\x9f\x94\x84  Connessione..."));
    auto* sock = new QTcpSocket(this);
    sock->setProperty("gns3ping", true);
    connect(sock, &QTcpSocket::connected,
            this, &LanWanPage::onGns3SockConnected);
    connect(sock, &QAbstractSocket::errorOccurred,
            this, &LanWanPage::onGns3SockError);
    sock->connectToHost(host, static_cast<quint16>(port));
    QPointer<QTcpSocket> sockPtr(sock);
    QTimer::singleShot(3000, this, [this, sockPtr](){
        if (sockPtr && sockPtr->state() != QAbstractSocket::ConnectedState) {
            m_gns3StatusLbl->setText(tr("\xe2\x9d\x8c  Timeout"));
            LogBus::post("\xe2\x9d\x8c LAN WAN: GNS3 timeout connessione.");
            sockPtr->abort(); sockPtr->deleteLater();
        }
    });
}

void LanWanPage::onGns3SockConnected()
{
    auto* sock = qobject_cast<QTcpSocket*>(sender());
    if (sock) { sock->disconnectFromHost(); sock->deleteLater(); }
    m_gns3StatusLbl->setText(tr("\xe2\x9c\x85  Server raggiungibile"));
    m_gns3ExecBtn->setEnabled(!m_gns3Code.isEmpty());
}

void LanWanPage::onGns3SockError(QAbstractSocket::SocketError)
{
    auto* sock = qobject_cast<QTcpSocket*>(sender());
    if (!sock) return;
    m_gns3StatusLbl->setText(tr("\xe2\x9d\x8c  ") + sock->errorString());
    LogBus::post("\xe2\x9d\x8c LAN WAN: GNS3 errore socket: " + sock->errorString());
    sock->deleteLater();
}

/* ══════════════════════════════════════════════════════════════
   Slots — GNS3 exec process
   ══════════════════════════════════════════════════════════════ */
void LanWanPage::onGns3ExecBtnClicked()
{
    if (m_gns3Code.isEmpty()) return;
    if (m_gns3ExecProc && m_gns3ExecProc->state() != QProcess::NotRunning) {
        m_gns3ExecProc->kill();
        m_gns3ExecProc->waitForFinished(P::kProcKillExtendedMs);
    }
    const QString tmpPath = QDir::tempPath() + "/prismalux_gns3_script.py";
    QFile f(tmpPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_gns3StatusLbl->setText(tr("\xe2\x9d\x8c  Impossibile creare script"));
        LogBus::post("\xe2\x9d\x8c LAN WAN: GNS3 impossibile creare script temporaneo.");
        return;
    }
    f.write(m_gns3Code.toUtf8());
    f.close();
    m_gns3ExecProc = new QProcess(this);
    m_gns3ExecProc->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_gns3ExecProc, &QProcess::readyRead,
            this, &LanWanPage::onGns3ProcReadyRead);
    connect(m_gns3ExecProc,
            QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &LanWanPage::onGns3ProcFinished);
    m_gns3ExecBtn->setEnabled(false);
    m_gns3StatusLbl->setText(tr("\xf0\x9f\x94\x84  Esecuzione script Python..."));
    if (m_gns3Progress) m_gns3Progress->show();
    m_gns3ExecProc->start(P::findPython(), {tmpPath});
}

void LanWanPage::onGns3ProcReadyRead()
{
    auto* proc = qobject_cast<QProcess*>(sender());
    if (proc)
        m_gns3Output->append(QString::fromUtf8(proc->readAll()).trimmed());
}

void LanWanPage::onGns3ProcFinished(int code, QProcess::ExitStatus)
{
    auto* proc = qobject_cast<QProcess*>(sender());
    if (m_gns3Progress) m_gns3Progress->hide();
    m_gns3StatusLbl->setText(code == 0
        ? "\xe2\x9c\x85  Completato"
        : "\xe2\x9d\x8c  Terminato con errore");
    if (code != 0) LogBus::post(QString("\xe2\x9d\x8c LAN WAN: GNS3 script terminato con errore (exit %1).").arg(code));
    m_gns3ExecBtn->setEnabled(true);
    if (m_gns3ExecProc == proc) m_gns3ExecProc = nullptr;
    if (proc) proc->deleteLater();
}

/* ══════════════════════════════════════════════════════════════
   Slots — GNS3 run / stop
   ══════════════════════════════════════════════════════════════ */
void LanWanPage::onGns3RunBtnClicked()
{
    const int idx = m_gns3Action->currentIndex();
    if (idx < 0 || !kGNS3Sys[idx]) return;
    gns3RunAi(QString::fromUtf8(kGNS3Sys[idx]),
              m_gns3Input->toPlainText());
}

void LanWanPage::onGns3StopBtnClicked()
{
    m_ai->abort();
    m_gns3RunBtn->setEnabled(true);
    m_gns3StopBtn->setEnabled(false);
}

/* ══════════════════════════════════════════════════════════════
   buildGNS3Tab
   ══════════════════════════════════════════════════════════════ */
QWidget* LanWanPage::buildGNS3Tab()
{
    namespace P = PrismaluxPaths;
    auto* w   = new QWidget(this);
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(6);

    auto* descLbl = new QLabel(
        "\xf0\x9f\x94\x8c  <i>GNS3 \xe2\x80\x94 Simulatore di reti open-source per la progettazione, "
        "il test e il troubleshooting di topologie di rete reali. Supporta Cisco IOS, VyOS, Mikrotik e router/switch virtuali.</i>", w);
    descLbl->setObjectName("hintLabel");
    descLbl->setTextFormat(Qt::RichText);
    descLbl->setWordWrap(true);
    lay->addWidget(descLbl);

    /* Barra connessione */
    auto* connRow = new QWidget(w);
    auto* connLay = new QHBoxLayout(connRow);
    connLay->setContentsMargins(0, 0, 0, 0);
    connLay->setSpacing(8);

    auto* lbl = new QLabel(tr("GNS3 REST API:"), connRow);
    lbl->setObjectName("hintLabel");
    m_gns3HostEdit = new QLineEdit("localhost:3080", connRow);
    m_gns3HostEdit->setFixedWidth(dpiScale(150));
    auto* pingBtn = new QPushButton(tr("\xf0\x9f\x94\x97  Verifica"), connRow);
    pingBtn->setObjectName("actionBtn");
    pingBtn->setFixedWidth(dpiScale(100));
    m_gns3StatusLbl = new QLabel(tr("\xe2\x9a\xaa  Non connesso"), connRow);
    m_gns3StatusLbl->setObjectName("hintLabel");
    m_gns3ExecBtn = new QPushButton(tr("\xf0\x9f\x8c\x90  Esegui su GNS3"), connRow);
    m_gns3ExecBtn->setObjectName("actionBtn");
    m_gns3ExecBtn->setFixedWidth(dpiScale(160));
    m_gns3ExecBtn->setEnabled(false);

    connLay->addWidget(lbl);
    connLay->addWidget(m_gns3HostEdit);
    connLay->addWidget(pingBtn);
    connLay->addWidget(m_gns3StatusLbl, 1);
    connLay->addWidget(m_gns3ExecBtn);
    lay->addWidget(connRow);

    auto* hintRowGns = new QHBoxLayout;
    auto* hintLbl = new QLabel(
        "\xf0\x9f\x8c\x90 <b>GNS3 MCP:</b> simulatore reti. "
        "Avvia GNS3 (porta 3080 di default) e installa: "
        "<code>pip install gns3fy requests</code><br>"
        "Plugin: <a href='https://github.com/ChistokhinSV/gns3-mcp'>gns3-mcp</a>", w);
    hintLbl->setObjectName("hintLabel");
    hintLbl->setOpenExternalLinks(true);
    hintLbl->setWordWrap(true);
    auto* copyBtnGns = new QPushButton("\xf0\x9f\x93\x8b", w);  /* 📋 */
    copyBtnGns->setObjectName("actionBtn");
    copyBtnGns->setFixedWidth(dpiScale(28));
    copyBtnGns->setFixedHeight(dpiScale(24));
    copyBtnGns->setToolTip(tr("Copia comando pip negli appunti"));
    connect(copyBtnGns, &QPushButton::clicked, w, [=]() {
        QApplication::clipboard()->setText(tr("pip install gns3fy requests"));
    });
    hintRowGns->addWidget(hintLbl, 1);
    hintRowGns->addWidget(copyBtnGns);
    lay->addLayout(hintRowGns);

    /* Azione + Modello */
    auto* toolRow = new QWidget(w);
    auto* toolLay = new QHBoxLayout(toolRow);
    toolLay->setContentsMargins(0, 0, 0, 0);
    toolLay->setSpacing(8);
    m_gns3Action = new QComboBox(toolRow);
    for (int i = 0; kGNS3Actions[i]; i++)
        m_gns3Action->addItem(QString::fromUtf8(kGNS3Actions[i]));
    m_gns3Model = new ModelComboBox(m_ai, toolRow);
    toolLay->addWidget(new QLabel(tr("Azione:"), toolRow));
    toolLay->addWidget(m_gns3Action, 1);
    toolLay->addWidget(new QLabel(tr("Modello:"), toolRow));
    toolLay->addWidget(m_gns3Model, 1);
    lay->addWidget(toolRow);

    m_gns3Input = new QTextEdit(w);
    m_gns3Input->setPlaceholderText(
        "Descrivi la rete da simulare...\n"
        "Es: 'Crea una topologia con 2 router Cisco e 3 PC in una LAN'\n"
        "Es: 'Configura OSPF tra R1 e R2 con redistribuzione statica'");
    m_gns3Input->setFixedHeight(dpiScale(80));
    lay->addWidget(m_gns3Input);

    auto* btnRow = new QWidget(w);
    auto* btnLay = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(0, 0, 0, 0);
    m_gns3RunBtn  = new QPushButton(tr("\xf0\x9f\xa4\x96  Genera script GNS3"), btnRow);
    m_gns3RunBtn->setObjectName("actionBtn");
    m_gns3StopBtn = new QPushButton(tr("\xe2\x8f\xb9  Stop"), btnRow);
    m_gns3StopBtn->setObjectName("actionBtn");
    m_gns3StopBtn->setProperty("danger", true);
    m_gns3StopBtn->setEnabled(false);
    btnLay->addWidget(m_gns3RunBtn);
    btnLay->addWidget(m_gns3StopBtn);
    btnLay->addStretch();
    lay->addWidget(btnRow);

    m_gns3Progress = new QProgressBar(w);
    m_gns3Progress->setRange(0, 0);   /* indeterminate */
    m_gns3Progress->setFixedHeight(dpiScale(4));
    m_gns3Progress->setTextVisible(false);
    m_gns3Progress->hide();
    lay->addWidget(m_gns3Progress);

    m_gns3Output = new QTextEdit(w);
    m_gns3Output->setReadOnly(true);
    m_gns3Output->setObjectName("outputView");
    m_gns3Output->setPlaceholderText(tr("Script Python GNS3 REST API appare qui..."));
    lay->addWidget(m_gns3Output, 1);

    m_gns3ErrorPanel = new AiErrorWidget(w);
    lay->addWidget(m_gns3ErrorPanel);

    connect(pingBtn,       &QPushButton::clicked, this, &LanWanPage::onPingBtnClicked);
    connect(m_gns3ExecBtn, &QPushButton::clicked, this, &LanWanPage::onGns3ExecBtnClicked);
    connect(m_gns3RunBtn,  &QPushButton::clicked, this, &LanWanPage::onGns3RunBtnClicked);
    connect(m_gns3StopBtn, &QPushButton::clicked, this, &LanWanPage::onGns3StopBtnClicked);

    return w;
}

/* ══════════════════════════════════════════════════════════════
   ADB — Installazione APK via USB
   ══════════════════════════════════════════════════════════════ */

/* Cerca adb: SDK Android, PATH di sistema, percorsi comuni */
QString LanWanPage::findAdb()
{
    /* 1. SDK Android nella home utente */
    const QString sdk = QDir::homePath() + "/Android/Sdk/platform-tools/adb";
    if (QFile::exists(sdk)) return sdk;

    /* 2. PATH di sistema via QStandardPaths */
    const QString inPath = QStandardPaths::findExecutable("adb");
    if (!inPath.isEmpty()) return inPath;

    /* 3. Percorsi comuni Linux/macOS */
    for (const QString& p : {
            QString("/usr/bin/adb"),
            QString("/usr/local/bin/adb"),
            QString("/opt/android-sdk/platform-tools/adb") })
        if (QFile::exists(p)) return p;

    return {};
}

void LanWanPage::onAdbInstallBtnClicked()
{
    if (m_adbProc && m_adbProc->state() != QProcess::NotRunning) {
        /* Secondo clic mentre è in corso → annulla */
        m_adbProc->kill();
        return;
    }

    const QString adb = findAdb();
    if (adb.isEmpty()) {
        m_adbStatusLbl->setText(
            tr("\xe2\x9d\x8c  adb non trovato. Installa: sudo apt install adb"));
        LogBus::post("\xe2\x9d\x8c LAN WAN: adb non trovato nel PATH.");
        return;
    }

    namespace P = PrismaluxPaths;
    const QString apk = P::root() + "/ANDROID/PrismaluxMobile.apk";
    if (!QFile::exists(apk)) {
        m_adbStatusLbl->setText(
            "\xe2\x9d\x8c  APK non trovato: " + apk);
        LogBus::post("\xe2\x9d\x8c LAN WAN: APK non trovato: " + apk);
        return;
    }

    m_adbLog->clear();
    m_adbLog->show();
    m_adbInstallBtn->setText(tr("\xe2\x8f\xb9  Annulla"));
    m_adbStatusLbl->setText(
        tr("\xe2\x8f\xb3  Installazione in corso... (attendi il telefono)"));

    if (m_adbProc) { m_adbProc->deleteLater(); m_adbProc = nullptr; }
    m_adbProc = new QProcess(this);
    m_adbProc->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_adbProc, &QProcess::readyRead,
            this, &LanWanPage::onAdbProcReadyRead);
    connect(m_adbProc,
            QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &LanWanPage::onAdbProcFinished);

    /* adb install -r <apk>
       -r = reinstalla se già presente (non cancella i dati) */
    m_adbProc->start(adb, {"install", "-r", apk});

    if (!m_adbProc->waitForStarted(P::kProcessStartTimeoutMs)) {
        m_adbStatusLbl->setText(
            tr("\xe2\x9d\x8c  Impossibile avviare adb. Controlla la connessione USB."));
        LogBus::post("\xe2\x9d\x8c LAN WAN: Impossibile avviare adb.");
        m_adbLog->append("Errore: adb non si avvia.");
        m_adbInstallBtn->setText(
            tr("\xf0\x9f\x94\x8c  Installa APK via USB  (adb)"));
        m_adbProc->deleteLater(); m_adbProc = nullptr;
    }
}

void LanWanPage::onAdbProcReadyRead()
{
    if (!m_adbProc || !m_adbLog) return;
    const QString out = QString::fromLocal8Bit(m_adbProc->readAll()).trimmed();
    if (out.isEmpty()) return;
    m_adbLog->moveCursor(QTextCursor::End);
    m_adbLog->append(out);
    m_adbLog->moveCursor(QTextCursor::End);
}

void LanWanPage::onAdbProcFinished(int code, QProcess::ExitStatus)
{
    /* Leggi eventuale output residuo */
    if (m_adbProc) {
        const QString tail = QString::fromLocal8Bit(m_adbProc->readAll()).trimmed();
        if (!tail.isEmpty() && m_adbLog) m_adbLog->append(tail);
        m_adbProc->deleteLater(); m_adbProc = nullptr;
    }

    if (m_adbInstallBtn)
        m_adbInstallBtn->setText(
            tr("\xf0\x9f\x94\x8c  Installa APK via USB  (adb)"));

    if (!m_adbStatusLbl) return;

    /* adb exit 0 + "Success" nel log → installazione riuscita */
    const QString log = m_adbLog ? m_adbLog->toPlainText() : QString();
    const bool success = (code == 0) || log.contains("Success", Qt::CaseInsensitive);

    if (success) {
        m_adbStatusLbl->setText(
            "\xe2\x9c\x85  APK installato con successo! "
            "Cerca \"Prismalux\" nel cassetto app.");
        m_adbStatusLbl->setStyleSheet("color:#4caf50;font-size:11px;");
    } else if (log.contains("INSTALL_FAILED_UPDATE_INCOMPATIBLE")) {
        m_adbStatusLbl->setText(
            "\xe2\x9a\xa0\xef\xb8\x8f  Versione incompatibile. "
            "Disinstalla la precedente: adb uninstall com.prismalux.mobile");
        m_adbStatusLbl->setStyleSheet("color:#ff9800;font-size:11px;");
    } else if (log.contains("no devices") || log.contains("unauthorized")) {
        m_adbStatusLbl->setText(
            tr("\xe2\x9d\x8c  Nessun dispositivo. Abilita USB Debugging e accetta il popup sul telefono."));
        LogBus::post("\xe2\x9d\x8c LAN WAN: adb nessun dispositivo USB.");
        m_adbStatusLbl->setStyleSheet("color:#f44336;font-size:11px;");
    } else {
        m_adbStatusLbl->setText(
            QString("\xe2\x9d\x8c  Installazione fallita (exit %1). Vedi log.").arg(code));
        LogBus::post(QString("\xe2\x9d\x8c LAN WAN: Installazione APK fallita (exit %1).").arg(code));
        m_adbStatusLbl->setStyleSheet("color:#f44336;font-size:11px;");
    }
}
