/* ======================================================================
   main_wan_persist.cpp — WAN helpers, dispatch, persist di LanWanPage
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

/* ══════════════════════════════════════════════════════════════════════════
   WAN — Calcolo Distribuito (BOINC-like)
   Protocollo: JSON newline-delimited su TCP (porta 11600 di default)
   Tipi messaggio:
     hello   → registrazione nodo client
     welcome ← id assegnato
     poll    → richiesta task
     task    ← task da eseguire
     idle    ← nessun task disponibile
     result  → esito task
     ack     ← conferma ricezione risultato
   ══════════════════════════════════════════════════════════════════════════ */

/* ── Helpers ── */
/* HMAC-SHA256 hex del messaggio — usato per firmare task e risultati WAN.
 * Definita qui (prima di wanDispatch che la usa) oltre che nel blocco slot server. */
static QString wanHmac(const QString& token, const QString& data)
{
    return QMessageAuthenticationCode::hash(
        data.toUtf8(), token.toUtf8(), QCryptographicHash::Sha256).toHex();
}

QString LanWanPage::wanNextId() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
}

void LanWanPage::wanSendJson(QTcpSocket* sock, const QJsonObject& obj)
{
    if (!sock || sock->state() != QAbstractSocket::ConnectedState) return;
    sock->write(QJsonDocument(obj).toJson(QJsonDocument::Compact) + "\n");
}

void LanWanPage::wanCliSendJson(const QJsonObject& obj)
{
    if (m_wanCliSock)
        m_wanCliSock->write(QJsonDocument(obj).toJson(QJsonDocument::Compact) + "\n");
}

void LanWanPage::wanLogCron(const QString& msg)
{
    if (m_wanCronLog)
        m_wanCronLog->append(
            "[" + QDateTime::currentDateTime().toString("HH:mm:ss") + "] " + msg);
}

void LanWanPage::wanCliAppendLog(const QString& msg)
{
    if (m_wanCliLog)
        m_wanCliLog->append(
            "[" + QDateTime::currentDateTime().toString("HH:mm:ss") + "] " + msg);
}

/* Spedisce i task pending ai nodi idle — priority scheduler (0=bassa/1=normale/2=alta) */
void LanWanPage::wanDispatch()
{
    for (auto& node : m_wanNodes) {
        if (node.status != "idle") continue;
        if (!node.sock || node.sock->state() != QAbstractSocket::ConnectedState) continue;

        /* Cerca il task pending con priorità massima */
        WanTask* best = nullptr;
        for (auto& task : m_wanTasks) {
            if (task.status != "pending") continue;
            if (!best || task.priority > best->priority) best = &task;
        }
        if (!best) break;

        best->status    = "running";
        best->node      = node.name;
        best->startedAt = QDateTime::currentDateTime();
        node.status     = "working";
        const QString srvToken = m_wanTokenEdit ? m_wanTokenEdit->text().trimmed() : QString();
        QJsonObject taskMsg{
            {"t", "task"}, {"id", best->id},
            {"kind", best->kind}, {"payload", best->payload}
        };
        if (!srvToken.isEmpty())
            taskMsg["hmac"] = wanHmac(srvToken, best->id + "|" + best->payload);
        wanSendJson(node.sock, taskMsg);
    }
    wanRefreshTables();
    updateWanStats();
}

void LanWanPage::wanRefreshTables()
{
    /* --- Nodi --- */
    if (m_wanNodeTable) {
        m_wanNodeTable->setRowCount(static_cast<int>(m_wanNodes.size()));
        for (int i = 0; i < (int)m_wanNodes.size(); ++i) {
            const auto& n = m_wanNodes[i];
            m_wanNodeTable->setItem(i, 0, new QTableWidgetItem(n.name));
            m_wanNodeTable->setItem(i, 1, new QTableWidgetItem(n.ip));
            m_wanNodeTable->setItem(i, 2, new QTableWidgetItem(n.status));
            m_wanNodeTable->setItem(i, 3, new QTableWidgetItem(n.caps.join(", ")));
        }
    }
    /* --- Task --- */
    if (m_wanTaskTable) {
        m_wanTaskTable->setRowCount(static_cast<int>(m_wanTasks.size()));
        static const QString priLabel[] = {"bassa","normale","alta"};
        for (int i = 0; i < (int)m_wanTasks.size(); ++i) {
            const auto& t = m_wanTasks[i];
            m_wanTaskTable->setItem(i, 0, new QTableWidgetItem(t.id));
            m_wanTaskTable->setItem(i, 1, new QTableWidgetItem(t.kind));
            m_wanTaskTable->setItem(i, 2, new QTableWidgetItem(
                t.payload.left(48) + (t.payload.size() > 48 ? "\xe2\x80\xa6" : "")));
            const QString statusTxt = t.status
                + (t.retryCount > 0 ? QString(" [retry %1]").arg(t.retryCount) : "");
            m_wanTaskTable->setItem(i, 3, new QTableWidgetItem(statusTxt));
            const QString nodeAndPri = t.node
                + (t.priority != 1 ? QString(" [%1]").arg(
                    priLabel[qBound(0, t.priority, 2)]) : "");
            m_wanTaskTable->setItem(i, 4, new QTableWidgetItem(nodeAndPri));
        }
    }
    wanSchedulePersist();
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Persistenza coda WU su SQLite — la coda sopravvive a chiusura/crash.
 * ══════════════════════════════════════════════════════════════════════════ */

void LanWanPage::wanSchedulePersist()
{
#ifdef HAVE_QT_SQL
    if (!m_wanPersistTimer) {
        m_wanPersistTimer = new QTimer(this);
        m_wanPersistTimer->setSingleShot(true);
        m_wanPersistTimer->setInterval(800);  // debounce: salva 0,8s dopo l'ultima modifica
        connect(m_wanPersistTimer, &QTimer::timeout, this, &LanWanPage::wanPersistTasks);
    }
    m_wanPersistTimer->start();
#endif
}

#ifdef HAVE_QT_SQL
static QString wanDbPath()
{
    return QDir::homePath() + "/.prismalux/wan_tasks.db";
}
#endif

void LanWanPage::wanLoadTasks()
{
#ifdef HAVE_QT_SQL
    QDir().mkpath(QDir::homePath() + "/.prismalux");
    if (m_wanDbConn.isEmpty())
        m_wanDbConn = QString("wan_tasks_%1").arg(reinterpret_cast<quintptr>(this));

    QSqlDatabase db = QSqlDatabase::contains(m_wanDbConn)
        ? QSqlDatabase::database(m_wanDbConn)
        : QSqlDatabase::addDatabase("QSQLITE", m_wanDbConn);
    db.setDatabaseName(wanDbPath());
    if (!db.open()) return;
    QFile::setPermissions(wanDbPath(), QFile::ReadOwner | QFile::WriteOwner);

    QSqlQuery q(db);
    q.exec("CREATE TABLE IF NOT EXISTS wan_tasks ("
           "  id TEXT PRIMARY KEY, kind TEXT, payload TEXT, status TEXT, node TEXT,"
           "  result TEXT, created TEXT, started_at TEXT, retry_count INTEGER, priority INTEGER)");

    m_wanTasks.clear();
    if (q.exec("SELECT id,kind,payload,status,node,result,created,started_at,retry_count,priority "
               "FROM wan_tasks")) {
        while (q.next()) {
            WanTask t;
            t.id         = q.value(0).toString();
            t.kind       = q.value(1).toString();
            t.payload    = q.value(2).toString();
            t.status     = q.value(3).toString();
            t.node       = q.value(4).toString();
            t.result     = q.value(5).toString();
            t.created    = QDateTime::fromString(q.value(6).toString(), Qt::ISODate);
            t.startedAt  = QDateTime::fromString(q.value(7).toString(), Qt::ISODate);
            t.retryCount = q.value(8).toInt();
            t.priority   = q.value(9).toInt();
            /* Un task "running" al ripristino significa che il coordinatore si è chiuso
               mentre era in esecuzione: rimettilo in coda (con un retry in più). */
            if (t.status == "running") {
                t.status = "pending";
                t.node.clear();
                t.startedAt = QDateTime();
                t.retryCount += 1;
            }
            m_wanTasks.push_back(t);
        }
    }
#endif
}

void LanWanPage::wanPersistTasks()
{
#ifdef HAVE_QT_SQL
    if (m_wanDbConn.isEmpty() || !QSqlDatabase::contains(m_wanDbConn)) return;
    QSqlDatabase db = QSqlDatabase::database(m_wanDbConn);
    if (!db.isOpen()) return;

    db.transaction();
    QSqlQuery q(db);
    q.exec("DELETE FROM wan_tasks");
    q.prepare("INSERT INTO wan_tasks "
              "(id,kind,payload,status,node,result,created,started_at,retry_count,priority) "
              "VALUES (?,?,?,?,?,?,?,?,?,?)");
    for (const WanTask& t : m_wanTasks) {
        q.addBindValue(t.id);
        q.addBindValue(t.kind);
        q.addBindValue(t.payload);
        q.addBindValue(t.status);
        q.addBindValue(t.node);
        q.addBindValue(t.result);
        q.addBindValue(t.created.toString(Qt::ISODate));
        q.addBindValue(t.startedAt.toString(Qt::ISODate));
        q.addBindValue(t.retryCount);
        q.addBindValue(t.priority);
        q.exec();
    }
    db.commit();
#endif
}

/* ══════════════════════════════════════════════════════════════
   buildWanComputeTab — pannello WAN calcolo distribuito
   ══════════════════════════════════════════════════════════════ */
QWidget* LanWanPage::buildWanComputeTab()
{
    auto* root = new QWidget;
    auto* vlay = new QVBoxLayout(root);
    vlay->setContentsMargins(12, 10, 12, 10);
    vlay->setSpacing(10);

    /* Distinzione WAN Compute vs Sci Compute */
    {
        auto* diffLbl = new QLabel(root);
        diffLbl->setObjectName("hintLabel");
        diffLbl->setTextFormat(Qt::RichText);
        diffLbl->setWordWrap(true);
        diffLbl->setText(
            "<b>\xf0\x9f\x96\xa7 WAN Compute</b> (porta 11600) \xe2\x80\x94 "
            "distribuisce <b>task generici</b> (shell, Python, file I/O, matplotlib, LLM) "
            "su macchine qualsiasi della rete. "
            "Usa per pipeline AI/automatizzazione, batch script, job creativi.<br>"
            "<b>\xf0\x9f\x94\xac Sci Compute</b> (porta 11601) \xe2\x80\x94 "
            "distribuisce <b>Work Unit scientifiche</b> (BLAST, GROMACS, SymPy, R, SciPy) "
            "con heartbeat, credit counter e aggregatore risultati BOINC-style. "
            "Usa per ricerca computazionale e bioinformatica.");
        vlay->addWidget(diffLbl);
    }

    /* ── Selezione modalità di esecuzione ── */
    auto* execModeRow = new QWidget;
    auto* execModeLay = new QHBoxLayout(execModeRow);
    execModeLay->setContentsMargins(0,0,0,0); execModeLay->setSpacing(16);
    auto* localRb = new QRadioButton("\xf0\x9f\xa7\xa0  Solo questo PC");
    auto* lanRb   = new QRadioButton("\xf0\x9f\x8c\x90  Rete LAN (pi\xc3\xb9 PC insieme)");
    auto* sciRb   = new QRadioButton(
        "\xf0\x9f\x94\xac  Calcolo Scientifico (BOINC-like)");
    sciRb->setToolTip(
        "Distribuisci task scientifici (BLAST, GROMACS, R, Python SciPy...)\n"
        "sui nodi della rete VPN (Tailscale consigliato)");
    localRb->setChecked(true);
    auto* execGrp = new QButtonGroup(execModeRow);
    execGrp->addButton(localRb, 0);
    execGrp->addButton(lanRb,   1);
    execGrp->addButton(sciRb,   2);
    execModeLay->addWidget(localRb);
    execModeLay->addWidget(lanRb);
    execModeLay->addWidget(sciRb);
    execModeLay->addStretch(1);
    vlay->addWidget(execModeRow);

    /* ── Stack esecuzione: 0=Multi-Agente, 1=Rete LAN, 2=Scientifico ── */
    m_execModeStack = new QStackedWidget;

    /* index 0 — Multi-Agente locale */
    m_multiAgentTab = new AgentiMultiPage(m_ai, root);
    m_execModeStack->addWidget(m_multiAgentTab);   /* index 0 */
    if (m_lanServer)
        m_lanServer->setGraphMemory(m_multiAgentTab->graphMemory());

    /* index 1 — Pannello LAN/WAN (tutto il sistema TCP distribuito) */
    auto* lanWidget = new QWidget;
    auto* lanVlay   = new QVBoxLayout(lanWidget);
    lanVlay->setContentsMargins(0,0,0,0); lanVlay->setSpacing(10);

    /* ── Header LAN ── */
    auto* hdrLbl = new QLabel(
        "\xf0\x9f\x96\xa7  <b>WAN Calcolo Distribuito</b>"
        "  <span style='color:gray;font-size:11px;'>"
        "Rete di nodi AI \xe2\x80\x94 server/client su TCP porta 11600</span>");
    hdrLbl->setTextFormat(Qt::RichText);
    hdrLbl->setObjectName("pageHeader");
    lanVlay->addWidget(hdrLbl);

    /* ── Selezione modalità Server/Client ── */
    auto* modeRow = new QWidget;
    auto* modeLay = new QHBoxLayout(modeRow);
    modeLay->setContentsMargins(0,0,0,0); modeLay->setSpacing(16);
    auto* srvRb = new QRadioButton("\xf0\x9f\x96\xa7  Modalit\xc3\xa0 Server");
    auto* cliRb = new QRadioButton("\xf0\x9f\x92\xbb  Modalit\xc3\xa0 Client");
    srvRb->setChecked(true);
    auto* modeGrp = new QButtonGroup(modeRow);
    modeGrp->addButton(srvRb, 0);
    modeGrp->addButton(cliRb, 1);
    modeLay->addWidget(srvRb);
    modeLay->addWidget(cliRb);
    modeLay->addStretch(1);
    lanVlay->addWidget(modeRow);

    /* ── Stack SERVER / CLIENT ── */
    m_wanModeStack = new QStackedWidget;

    /* ═══════════ PANNELLO SERVER ═══════════
     * Layout semplificato: solo 3 sezioni visibili + "Avanzato" nascosto.
     *   1. Controllo server (porta + avvia)
     *   2. Decomponi compito con AI  ← main feature
     *   3. Monitor (nodi + coda task) compact
     *   4. [▶ Avanzato] nascosto: aggiungi task singolo + cron
     * ══════════════════════════════════════ */
    auto* srvPanel = new QWidget;
    auto* srvLay   = new QVBoxLayout(srvPanel);
    srvLay->setContentsMargins(0,0,0,0); srvLay->setSpacing(6);

    /* 1 — Controllo server */
    auto* srvCtrlRow = new QWidget;
    auto* srvCtrlLay = new QHBoxLayout(srvCtrlRow);
    srvCtrlLay->setContentsMargins(0,0,0,0); srvCtrlLay->setSpacing(8);
    m_wanPortSpin = new QSpinBox;
    m_wanPortSpin->setRange(1024, 65535);
    m_wanPortSpin->setValue(P::kWanComputePort);
    m_wanPortSpin->setFixedWidth(dpiScale(80));
    m_wanStartBtn = new QPushButton("\xe2\x96\xb6  Avvia Server");
    m_wanStartBtn->setObjectName("actionBtn");
    m_wanStartBtn->setCheckable(true);
    m_wanStartBtn->setEnabled(false);   /* abilitato solo dopo token >= 8 char */
    m_wanSrvStatusLbl = new QLabel("\xe2\x9a\xab  Server fermo");
    m_wanSrvStatusLbl->setStyleSheet("color:gray;");
    m_wanSimBtn = new QPushButton("\xe2\x9a\x97\xef\xb8\x8f  Prova in locale");  // ⚗️
    m_wanSimBtn->setToolTip(
        "Avvia server + connette un nodo virtuale sullo stesso PC.\n"
        "Permette di testare il sistema senza altri computer.");
    m_wanExposeAllCheck = new QCheckBox("\xe2\x9a\xa0\xef\xb8\x8f  Esponi su tutte le interfacce");
    m_wanExposeAllCheck->setChecked(false);
    m_wanExposeAllCheck->setToolTip(
        "OFF (default): il server accetta connessioni solo da questo PC (127.0.0.1).\n"
        "ON: bind su 0.0.0.0 — visibile a tutta la rete LAN/WAN.\n"
        "Abilita solo con token auth impostato e su reti fidate.");
    m_wanTlsCheck = new QCheckBox("\xf0\x9f\x94\x92  TLS");
#if QT_CONFIG(ssl)
    m_wanTlsCheck->setChecked(true);
    m_wanTlsCheck->setToolTip(
        "Cifra il traffico WAN con TLS (certificato self-signed in ~/.prismalux/).\n"
        "Obbligatorio per uso su internet. I worker devono avere TLS attivo.\n"
        "Disattiva solo su LAN locale pienamente fidata.");
#else
    m_wanTlsCheck->setChecked(false);
    m_wanTlsCheck->setEnabled(false);
    m_wanTlsCheck->setToolTip("TLS non disponibile: Qt compilato senza supporto SSL.");
#endif
    srvCtrlLay->addWidget(new QLabel("Porta:"));
    srvCtrlLay->addWidget(m_wanPortSpin);
    srvCtrlLay->addWidget(m_wanStartBtn);
    srvCtrlLay->addWidget(m_wanSimBtn);
    srvCtrlLay->addWidget(m_wanExposeAllCheck);
    srvCtrlLay->addWidget(m_wanTlsCheck);
    srvCtrlLay->addWidget(m_wanSrvStatusLbl, 1);
    connect(m_wanSimBtn, &QPushButton::clicked, this, &LanWanPage::onWanSimBtnClicked);
    srvLay->addWidget(srvCtrlRow);

    /* Token auth server — i nodi devono presentarlo nell'hello */
    auto* srvTokenRow = new QWidget;
    auto* srvTokenLay = new QHBoxLayout(srvTokenRow);
    srvTokenLay->setContentsMargins(0,0,0,0); srvTokenLay->setSpacing(6);
    auto* srvTokenLbl = new QLabel("\xf0\x9f\x94\x91  Token server:", srvTokenRow);  /* 🔑 */
    m_wanTokenEdit = new QLineEdit(srvTokenRow);
    m_wanTokenEdit->setPlaceholderText(tr("Obbligatorio: min 8 caratteri per avviare il server"));
    m_wanTokenEdit->setEchoMode(QLineEdit::Password);
    m_wanTokenEdit->setToolTip(
        "Token segreto condiviso tra server e nodi worker.\n"
        "Obbligatorio: il server non si avvia senza un token di almeno 8 caratteri.\n"
        "Consigliato: 16+ caratteri casuali.");
    connect(m_wanTokenEdit, &QLineEdit::textChanged, this, [this](const QString& t) {
        const bool ok = t.trimmed().length() >= 8;
        if (m_wanStartBtn) m_wanStartBtn->setEnabled(ok);
        m_wanTokenEdit->setStyleSheet(
            t.trimmed().isEmpty() ? "" :
            ok ? "border:1px solid #4caf50;" : "border:1px solid #f44336;");
    });

    auto* wanEyeBtn = new QPushButton("\xf0\x9f\x91\x81", srvTokenRow);
    wanEyeBtn->setFixedWidth(dpiScale(28)); wanEyeBtn->setCheckable(true); wanEyeBtn->setFlat(true);
    wanEyeBtn->setToolTip(tr("Mostra/nascondi token"));
    connect(wanEyeBtn, &QPushButton::toggled, m_wanTokenEdit, [this](bool show) {
        m_wanTokenEdit->setEchoMode(show ? QLineEdit::Normal : QLineEdit::Password);
    });

    auto* wanCopyBtn = new QPushButton("\xf0\x9f\x93\x8b", srvTokenRow);
    wanCopyBtn->setFixedWidth(dpiScale(28)); wanCopyBtn->setFlat(true);
    wanCopyBtn->setToolTip(tr("Copia token negli appunti"));
    connect(wanCopyBtn, &QPushButton::clicked, m_wanTokenEdit, [this] {
        QApplication::clipboard()->setText(m_wanTokenEdit->text().trimmed());
    });

    auto* wanRegenBtn = new QPushButton("\xf0\x9f\x94\x84", srvTokenRow);
    wanRegenBtn->setFixedWidth(dpiScale(28)); wanRegenBtn->setFlat(true);
    wanRegenBtn->setToolTip(tr("Genera nuovo token casuale (32 caratteri)"));
    connect(wanRegenBtn, &QPushButton::clicked, m_wanTokenEdit, [this] {
        m_wanTokenEdit->setText(
            QUuid::createUuid().toString(QUuid::WithoutBraces).replace("-", "").left(32));
    });

    srvTokenLay->addWidget(srvTokenLbl);
    srvTokenLay->addWidget(m_wanTokenEdit, 1);
    srvTokenLay->addWidget(wanEyeBtn);
    srvTokenLay->addWidget(wanCopyBtn);
    srvTokenLay->addWidget(wanRegenBtn);
    srvLay->addWidget(srvTokenRow);

    /* Nota WireGuard — visibile solo quando exposeAll è ON */
    m_wanVpnNoteLbl = new QLabel(srvPanel);
    m_wanVpnNoteLbl->setWordWrap(true);
    m_wanVpnNoteLbl->setTextFormat(Qt::RichText);
    m_wanVpnNoteLbl->setText(
        "<span style='color:#60a5fa;font-size:11px;'>"
        "\xf0\x9f\x94\x92 <b>Consiglio sicurezza per reti con pi\xc3\xb9 PC</b><br>"
        "Usa <b>WireGuard</b> su tutti i worker: cifra il traffico WAN e "
        "limita l\xe2\x80\x99" "accesso ai soli peer autorizzati.<br>"
        "Configura il server WAN su <code>10.0.0.1</code> (IP WireGuard) invece di <code>0.0.0.0</code>, "
        "cos\xc3\xac la porta 11600 \xc3\xa8 raggiungibile solo dai PC nella VPN.<br>"
        "Con token WAN + WireGuard la sicurezza \xc3\xa8 doppia: "
        "prima il tunnel cifrato, poi l\xe2\x80\x99" "autenticazione."
        "</span>"
    );
    m_wanVpnNoteLbl->setVisible(false);
    srvLay->addWidget(m_wanVpnNoteLbl);

    connect(m_wanExposeAllCheck, &QCheckBox::toggled,
            m_wanVpnNoteLbl,     &QLabel::setVisible);

    /* 2 — Decomponi compito: textarea sinistra, bottone destra */
    auto* decompBox = new QGroupBox(
        "\xf0\x9f\xa7\xa0  Scrivi un compito \xe2\x80\x94 l\xe2\x80\x99" "AI lo divide in agenti automaticamente");
    auto* decompLay = new QHBoxLayout(decompBox);
    decompLay->setSpacing(8); decompLay->setContentsMargins(6,4,6,4);

    /* Sinistra: textarea (stretch = 1, prende tutto lo spazio orizzontale) */
    m_wanDecomposeInput = new QTextEdit;
    m_wanDecomposeInput->setMinimumHeight(dpiScale(56));
    m_wanDecomposeInput->setPlaceholderText(
        "es. \"Analizza il mercato delle app fitness in Italia e crea una strategia di lancio\"\n"
        "es. \"Scrivi un articolo scientifico sull\xe2\x80\x99intelligenza artificiale distribuita\"");
    decompLay->addWidget(m_wanDecomposeInput, 1);

    /* Destra: bottone + stato (colonna fissa) */
    auto* decompRight = new QWidget;
    auto* decompRightLay = new QVBoxLayout(decompRight);
    decompRightLay->setContentsMargins(0,0,0,0); decompRightLay->setSpacing(4);
    m_wanDecomposeBtn = new QPushButton("\xf0\x9f\xa7\xa0  Crea agenti");
    m_wanDecomposeBtn->setObjectName("actionBtn");
    m_wanDecomposeStatusLbl = new QLabel("Scrivi il compito\ne premi \"Crea agenti\"");
    m_wanDecomposeStatusLbl->setStyleSheet("color:gray; font-size:11px;");
    m_wanDecomposeStatusLbl->setWordWrap(true);
    m_wanDecomposeStatusLbl->setFixedWidth(dpiScale(150));
    auto* shuffleBtn = new QPushButton("\xf0\x9f\x94\x80  Esempio");   /* 🔀 */
    shuffleBtn->setToolTip("Carica un compito d\xe2\x80\x99" "esempio casuale per ispirarti.");
    shuffleBtn->setFlat(true);
    shuffleBtn->setStyleSheet("font-size:11px; color:#818cf8;");

    decompRightLay->addWidget(m_wanDecomposeBtn);
    decompRightLay->addWidget(shuffleBtn);
    decompRightLay->addWidget(m_wanDecomposeStatusLbl);
    decompRightLay->addStretch();
    decompLay->addWidget(decompRight);

    connect(m_wanDecomposeBtn, &QPushButton::clicked,
            this, &LanWanPage::onWanDecomposeBtnClicked);

    /* Prompt d'esempio: coprono ricerca, codice, business, scienza, creatività */
    static const QStringList kEsempi {
        "Analizza il mercato delle app fitness in Italia: competitor, fasce di prezzo, gap di mercato e strategia di lancio per una nuova app.",
        "Scrivi un articolo scientifico completo sull\xe2\x80\x99impatto dell\xe2\x80\x99intelligenza artificiale distribuita nelle piccole imprese italiane.",
        "Crea un piano di studio per imparare Python in 3 mesi partendo da zero, con esercizi pratici e progetti reali.",
        "Progetta un\xe2\x80\x99" "architettura microservizi per un e-commerce: servizi necessari, comunicazione tra essi, database e deploy su Docker.",
        "Analizza i pro e contro delle principali energie rinnovabili (solare, eolico, idroelettrico) per l\xe2\x80\x99Italia al 2030.",
        "Scrivi un business plan per aprire una gelateria artigianale biologica a Milano: costi, ricavi, marketing e break-even.",
        "Crea una guida completa sulla dieta mediterranea: principi scientifici, menu settimanale, lista della spesa e ricette.",
        "Analizza il fenomeno del remote working: effetti sulla produttivit\xc3\xa0, salute mentale, mercato immobiliare e futuro del lavoro.",
        "Progetta un\xe2\x80\x99" "app mobile per la gestione del tempo (time-blocking): funzionalit\xc3\xa0 chiave, UX/UI, stack tecnologico e piano di sviluppo.",
        "Studia la storia e l\xe2\x80\x99" "evoluzione del calcio italiano: dalle origini a oggi, tattiche, campioni e impatto culturale.",
        "Crea un corso online su machine learning: struttura dei moduli, esercizi, dataset da usare e piattaforma di erogazione.",
        "Analizza la crisi climatica: cause principali, impatti sull\xe2\x80\x99Italia, soluzioni tecnologiche e politiche necessarie entro il 2050.",
        "Scrivi una guida al marketing digitale per piccole imprese: SEO, social media, email marketing e budget consigliato.",
        "Progetta un sistema domotico per una casa intelligente: sensori, automazioni, protocolli (Zigbee/Matter) e privacy.",
        "Analizza il mercato degli NFT e Web3 in Italia: stato attuale, casi d\xe2\x80\x99uso reali, rischi e prospettive future.",
        "Crea un piano di allenamento per correre una mezza maratona in 4 mesi: tabella settimanale, nutrizione e recupero.",
        "Scrivi una serie di 5 episodi di un podcast sulla storia della tecnologia italiana: temi, ospiti e scaletta.",
        "Progetta un chatbot per il customer service di un negozio online: flussi conversazionali, integrazioni e metriche KPI.",
        "Analizza i meccanismi della memoria umana e crea tecniche di studio basate sulla neuroscienza cognitiva.",
        "Crea un\xe2\x80\x99" "analisi SWOT completa per lanciare un servizio di consegna pasti a domicilio in una citt\xc3\xa0 di 100.000 abitanti.",
    };

    connect(shuffleBtn, &QPushButton::clicked, m_wanDecomposeInput,
            [this]() {
                if (!m_wanDecomposeInput) return;
                static int lastIdx = -1;
                int idx;
                do { idx = QRandomGenerator::global()->bounded(kEsempi.size()); }
                while (idx == lastIdx && kEsempi.size() > 1);
                lastIdx = idx;
                m_wanDecomposeInput->setPlainText(kEsempi[idx]);
            });

    srvLay->addWidget(decompBox);

    /* Guida aggiunta nodo worker */
    {
        auto* workerGuide = new QLabel;
        workerGuide->setObjectName("hintLabel");
        workerGuide->setTextFormat(Qt::RichText);
        workerGuide->setWordWrap(true);
        workerGuide->setText(
            "<b>\xf0\x9f\x96\xa5  Come aggiungere un nodo worker:</b>"
            "<ol style='margin:4px 0 0 16px; padding:0;'>"
            "<li>Installa Python 3.10+ sul nodo remoto.</li>"
            "<li>Copia <b>MCPs/wan_worker/wan_worker.py</b> sul nodo "
            "oppure clona il repo Prismalux.</li>"
            "<li>Avvia il worker: "
            "<tt>python3 wan_worker.py --host IP_SERVER --port 11600 "
            "--token TOKEN --name NomeNodo</tt><br>"
            "(IP_SERVER = IP della macchina con Prismalux; TOKEN = token "
            "mostrato nella sezione Autenticazione sopra).</li>"
            "<li>Il nodo appare automaticamente nella tabella qui sotto "
            "una volta connesso.</li>"
            "<li>Per nodi su reti diverse (internet/ufficio) attiva la VPN "
            "nella tab Programmazione \xe2\x86\x92 Rete.</li>"
            "</ol>");
        srvLay->addWidget(workerGuide);
    }

    /* 3 — Monitor nodi + coda: stretch per occupare lo spazio liberato */
    auto* tableSplit = new QSplitter(Qt::Horizontal);

    auto* nodeBox = new QGroupBox("\xf0\x9f\x92\xbb  Nodi connessi");
    auto* nodeBLay = new QVBoxLayout(nodeBox);
    nodeBLay->setContentsMargins(4,4,4,4);
    m_wanNodeTable = new QTableWidget(0, 4);
    m_wanNodeTable->setHorizontalHeaderLabels({"Nome","IP","Stato","Capacit\xc3\xa0"});
    m_wanNodeTable->horizontalHeader()->setStretchLastSection(true);
    m_wanNodeTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_wanNodeTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_wanNodeTable->setMinimumHeight(dpiScale(120));
    nodeBLay->addWidget(m_wanNodeTable);
    tableSplit->addWidget(nodeBox);

    auto* taskBox = new QGroupBox("\xf0\x9f\x93\x8b  Coda task");
    auto* taskBLay = new QVBoxLayout(taskBox);
    taskBLay->setContentsMargins(4,4,4,4);
    m_wanTaskTable = new QTableWidget(0, 5);
    m_wanTaskTable->setHorizontalHeaderLabels({"ID","Tipo","Payload\xe2\x80\xa6","Stato","Nodo"});
    m_wanTaskTable->horizontalHeader()->setStretchLastSection(false);
    m_wanTaskTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_wanTaskTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_wanTaskTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_wanTaskTable->setMinimumHeight(dpiScale(120));
    taskBLay->addWidget(m_wanTaskTable);
    tableSplit->addWidget(taskBox);
    srvLay->addWidget(tableSplit, 1);

    /* Stats BOINC-style */
    m_wanStatsLbl = new QLabel("\xf0\x9f\x96\xa5  Nodi: 0 idle / 0 working  \xe2\x80\x94  "
                               "\xf0\x9f\x93\x8b  Task: 0 in coda / 0 running / 0 completati / 0 falliti");
    m_wanStatsLbl->setObjectName("cardDesc");
    m_wanStatsLbl->setTextFormat(Qt::RichText);
    srvLay->addWidget(m_wanStatsLbl);

    /* Throughput + export */
    {
        auto* dashRow = new QWidget;
        auto* dashLay = new QHBoxLayout(dashRow);
        dashLay->setContentsMargins(0, 0, 0, 0); dashLay->setSpacing(12);
        m_wanThroughputLbl = new QLabel("\xf0\x9f\x93\x88  Throughput: \xe2\x80\x94", dashRow);
        m_wanThroughputLbl->setObjectName("cardDesc");
        m_wanThroughputLbl->setTextFormat(Qt::RichText);
        m_wanChartWidget = new QLabel(dashRow);
        m_wanChartWidget->setFixedSize(dpiScale(300), dpiScale(60));
        m_wanChartWidget->setToolTip(tr("Istogramma throughput — ultimi 60 min (bin 5 min)"));
        m_wanChartWidget->setObjectName("cardDesc");
        m_wanExportBtn = new QPushButton(
            "\xf0\x9f\x93\xa5  Esporta CSV", dashRow);
        m_wanExportBtn->setObjectName("actionBtn");
        m_wanExportBtn->setToolTip(tr("Scarica CSV di tutti i task (id, tipo, payload, stato, nodo, durata, risultato)"));
        dashLay->addWidget(m_wanThroughputLbl, 1);
        dashLay->addWidget(m_wanChartWidget);
        dashLay->addWidget(m_wanExportBtn);
        srvLay->addWidget(dashRow);
        connect(m_wanExportBtn, &QPushButton::clicked,
                this, &LanWanPage::onWanExportCsvClicked);
    }

    /* Dashboard timer 5s */
    m_wanDashTimer = new QTimer(this);
    connect(m_wanDashTimer, &QTimer::timeout, this, &LanWanPage::onWanDashTick);
    m_wanDashTimer->start(5000);

    /* 4 — Avanzato (nascosto di default) */
    auto* advToggle = new QPushButton("\xe2\x96\xb6  Avanzato — aggiungi task singolo, cron");
    advToggle->setCheckable(true);
    advToggle->setChecked(false);
    advToggle->setFlat(true);
    advToggle->setStyleSheet("text-align:left; color:gray; font-size:11px;");
    srvLay->addWidget(advToggle);

    auto* advPanel = new QWidget;
    advPanel->setVisible(false);
    auto* advLay = new QVBoxLayout(advPanel);
    advLay->setContentsMargins(0,0,0,0); advLay->setSpacing(6);

    connect(advToggle, &QPushButton::toggled, advPanel, &QWidget::setVisible);
    connect(advToggle, &QPushButton::toggled, advToggle, [advToggle](bool on){
        advToggle->setText(on
            ? "\xe2\x96\xbc  Avanzato — aggiungi task singolo, cron"
            : "\xe2\x96\xb6  Avanzato — aggiungi task singolo, cron");
    });

    /* Aggiungi task manuale */
    auto* addTaskBox = new QGroupBox("\xe2\x9e\x95  Aggiungi task singolo");
    auto* addTaskLay = new QVBoxLayout(addTaskBox);
    auto* addTaskRow1 = new QWidget;
    auto* addTaskLay1 = new QHBoxLayout(addTaskRow1);
    addTaskLay1->setContentsMargins(0,0,0,0); addTaskLay1->setSpacing(8);
    m_wanTaskKind = new QComboBox;
    wanPopulateKindCombo(m_wanTaskKind);
    m_wanAddTaskBtn = new QPushButton("\xe2\x9e\x95  Aggiungi");
    m_wanAddTaskBtn->setObjectName("actionBtn");
    addTaskLay1->addWidget(new QLabel("Tipo:"));
    addTaskLay1->addWidget(m_wanTaskKind, 1);
    addTaskLay1->addWidget(m_wanAddTaskBtn);

    /* Stack payload: 0 = textarea raw, 1 = form llm_agent */
    m_wanPayloadStack = new QStackedWidget;

    m_wanTaskPayload = new QTextEdit;
    m_wanTaskPayload->setFixedHeight(dpiScale(70));
    m_wanTaskPayload->setPlaceholderText(tr("Seleziona un tipo per vedere il template\xe2\x80\xa6"));
    m_wanPayloadStack->addWidget(m_wanTaskPayload);   // index 0

    m_agentFormFrame = new QFrame;
    m_agentFormFrame->setFrameShape(QFrame::StyledPanel);
    m_agentFormFrame->setAcceptDrops(true);
    m_agentFormFrame->installEventFilter(this);
    auto* formLay = new QVBoxLayout(m_agentFormFrame);
    formLay->setSpacing(4); formLay->setContentsMargins(6,4,6,4);

    m_agentRoleEdit = new QLineEdit;
    m_agentRoleEdit->setPlaceholderText(tr("Ruolo: es. Ricercatore specializzato in fisica quantistica"));
    formLay->addWidget(m_agentRoleEdit);

    m_agentPromptEdit = new QTextEdit;
    m_agentPromptEdit->setFixedHeight(dpiScale(52));
    m_agentPromptEdit->setPlaceholderText(tr("Prompt: il compito specifico di questo agente\xe2\x80\xa6"));
    formLay->addWidget(m_agentPromptEdit);

    m_agentContextEdit = new QTextEdit;
    m_agentContextEdit->setFixedHeight(dpiScale(34));
    m_agentContextEdit->setPlaceholderText(tr("Contesto: da agenti precedenti (opzionale)"));
    formLay->addWidget(m_agentContextEdit);

    auto* agentBtnRow = new QWidget;
    auto* agentBtnLay = new QHBoxLayout(agentBtnRow);
    agentBtnLay->setContentsMargins(0,0,0,0); agentBtnLay->setSpacing(6);
    m_agentSaveBtn = new QPushButton("\xf0\x9f\x92\xbe  Salva JSON");
    m_agentLoadBtn = new QPushButton("\xf0\x9f\x93\x82  Carica JSON");
    auto* dropHintLbl = new QLabel("oppure trascina un file .json");
    dropHintLbl->setStyleSheet("color:gray; font-size:11px;");
    agentBtnLay->addWidget(m_agentSaveBtn);
    agentBtnLay->addWidget(m_agentLoadBtn);
    agentBtnLay->addWidget(dropHintLbl, 1);
    formLay->addWidget(agentBtnRow);
    m_wanPayloadStack->addWidget(m_agentFormFrame);   // index 1

    addTaskLay->addWidget(addTaskRow1);
    addTaskLay->addWidget(m_wanPayloadStack);
    advLay->addWidget(addTaskBox);

    connect(m_agentSaveBtn, &QPushButton::clicked, this, &LanWanPage::onAgentSaveBtnClicked);
    connect(m_agentLoadBtn, &QPushButton::clicked, this, &LanWanPage::onAgentLoadBtnClicked);

    connect(m_wanTaskKind, QOverload<int>::of(&QComboBox::currentIndexChanged),
            m_wanTaskKind, [this](int){
        const QString kind = m_wanTaskKind->currentData().toString();
        if (kind.isEmpty()) return;
        const bool isAgent = (kind == "llm_agent");
        if (m_wanPayloadStack) m_wanPayloadStack->setCurrentIndex(isAgent ? 1 : 0);
        if (!isAgent && m_wanTaskPayload)
            m_wanTaskPayload->setPlainText(wanKindTemplate(kind));
    });
    {
        const QString initKind = m_wanTaskKind->currentData().toString();
        const bool isAgent = (initKind == "llm_agent");
        if (m_wanPayloadStack) m_wanPayloadStack->setCurrentIndex(isAgent ? 1 : 0);
        if (!isAgent && m_wanTaskPayload && !initKind.isEmpty())
            m_wanTaskPayload->setPlainText(wanKindTemplate(initKind));
    }

    /* Cron */
    auto* cronBox = new QGroupBox("\xe2\x8f\xb0  Cron — ripeti task automaticamente");
    auto* cronLay = new QVBoxLayout(cronBox);
    auto* cronRow1 = new QWidget;
    auto* cronLay1 = new QHBoxLayout(cronRow1);
    cronLay1->setContentsMargins(0,0,0,0); cronLay1->setSpacing(8);
    m_wanCronInterval = new QSpinBox;
    m_wanCronInterval->setRange(1, 1440); m_wanCronInterval->setValue(5);
    m_wanCronInterval->setSuffix(" min");
    m_wanCronKind = new QComboBox;
    wanPopulateKindCombo(m_wanCronKind);
    m_wanCronStartBtn = new QPushButton("\xe2\x96\xb6  Avvia");
    m_wanCronStartBtn->setObjectName("actionBtn");
    m_wanCronStopBtn  = new QPushButton("\xe2\x8f\xb9  Stop");
    m_wanCronStopBtn->setObjectName("actionBtn");
    m_wanCronStopBtn->setEnabled(false);
    cronLay1->addWidget(new QLabel("Ogni:"));
    cronLay1->addWidget(m_wanCronInterval);
    cronLay1->addWidget(new QLabel("Tipo:"));
    cronLay1->addWidget(m_wanCronKind, 1);
    cronLay1->addWidget(m_wanCronStartBtn);
    cronLay1->addWidget(m_wanCronStopBtn);
    m_wanCronPayload = new QTextEdit;
    m_wanCronPayload->setPlaceholderText(tr("Payload del task cron (ripetuto ad ogni tick)"));
    m_wanCronPayload->setFixedHeight(dpiScale(48));
    m_wanCronLog = new QTextEdit;
    m_wanCronLog->setReadOnly(true);
    m_wanCronLog->setFixedHeight(dpiScale(56));
    m_wanCronLog->setPlaceholderText(tr("Log cron\xe2\x80\xa6"));
    cronLay->addWidget(cronRow1);
    cronLay->addWidget(m_wanCronPayload);
    cronLay->addWidget(m_wanCronLog);
    advLay->addWidget(cronBox);

    srvLay->addWidget(advPanel);

    auto* srvScroll = new QScrollArea;
    srvScroll->setWidgetResizable(true);
    srvScroll->setFrameShape(QFrame::NoFrame);
    srvScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    srvScroll->setWidget(srvPanel);
    m_wanModeStack->addWidget(srvScroll);   /* index 0 = server */

    /* ═══════════ PANNELLO CLIENT ═══════════
     * Semplificato: una sola riga di connessione + log.
     * ══════════════════════════════════════ */
    auto* cliPanel = new QWidget;
    auto* cliLay   = new QVBoxLayout(cliPanel);
    cliLay->setContentsMargins(0,0,0,0); cliLay->setSpacing(8);

    /* Connessione — tutto su una riga */
    auto* cliConRow = new QWidget;
    auto* cliConLay = new QHBoxLayout(cliConRow);
    cliConLay->setContentsMargins(0,0,0,0); cliConLay->setSpacing(6);
    m_wanCliHost = new QLineEdit;
    m_wanCliHost->setPlaceholderText(tr("IP server (es. 192.168.1.10)"));
    m_wanCliPort = new QSpinBox;
    m_wanCliPort->setRange(1024, 65535); m_wanCliPort->setValue(P::kWanComputePort);
    m_wanCliPort->setFixedWidth(dpiScale(80));
    m_wanCliName = new QLineEdit;
    m_wanCliName->setPlaceholderText(tr("Nome nodo (es. PC-Mario)"));
    m_wanCliName->setFixedWidth(dpiScale(130));
    m_wanCliWorkerSpin = new QSpinBox;
    m_wanCliWorkerSpin->setRange(1, 4);
    m_wanCliWorkerSpin->setValue(1);
    m_wanCliWorkerSpin->setPrefix("Worker: ");
    m_wanCliWorkerSpin->setFixedWidth(dpiScale(100));
    m_wanCliWorkerSpin->setToolTip(
        "Numero di worker simultanei (1-4).\n"
        "Ogni worker ha un socket TCP e un AiClient propri.");
    m_wanCliConBtn    = new QPushButton("\xf0\x9f\x94\x8c  Connetti");
    m_wanCliConBtn->setObjectName("actionBtn");
    m_wanCliDisconBtn = new QPushButton("Disconnetti");
    m_wanCliDisconBtn->setEnabled(false);
    m_wanCliStatusLbl = new QLabel("\xe2\x9a\xab  Non connesso");
    m_wanCliStatusLbl->setStyleSheet("color:gray;");
    cliConLay->addWidget(m_wanCliHost, 2);
    cliConLay->addWidget(m_wanCliPort);
    cliConLay->addWidget(m_wanCliName);
    cliConLay->addWidget(m_wanCliWorkerSpin);
    cliConLay->addWidget(m_wanCliConBtn);
    cliConLay->addWidget(m_wanCliDisconBtn);
    cliConLay->addWidget(m_wanCliStatusLbl, 1);
    cliLay->addWidget(cliConRow);

    /* Token client + opt-in shell */
    auto* cliSecRow = new QWidget;
    auto* cliSecLay = new QHBoxLayout(cliSecRow);
    cliSecLay->setContentsMargins(0,0,0,0); cliSecLay->setSpacing(8);
    auto* cliTokenLbl = new QLabel("\xf0\x9f\x94\x91  Token:", cliSecRow);  /* 🔑 */
    m_wanCliTokenEdit = new QLineEdit(cliSecRow);
    m_wanCliTokenEdit->setPlaceholderText(tr("Token server (se impostato)"));
    m_wanCliTokenEdit->setEchoMode(QLineEdit::Password);
    m_wanCliTokenEdit->setToolTip(tr("Deve coincidere con il token impostato sul server."));
    m_wanCliTlsCheck = new QCheckBox("\xf0\x9f\x94\x92  TLS", cliSecRow);
#if QT_CONFIG(ssl)
    m_wanCliTlsCheck->setChecked(true);
    m_wanCliTlsCheck->setToolTip(
        "Connetti al server WAN con TLS cifrato.\n"
        "Deve corrispondere all'impostazione del server.\n"
        "Obbligatorio per connessioni su internet.");
#else
    m_wanCliTlsCheck->setChecked(false);
    m_wanCliTlsCheck->setEnabled(false);
    m_wanCliTlsCheck->setToolTip("TLS non disponibile: Qt compilato senza supporto SSL.");
#endif
    m_wanCliShellCheck = new QCheckBox("\xe2\x9a\xa0\xef\xb8\x8f  Permetti shell (rischio RCE)", cliSecRow);
    m_wanCliShellCheck->setToolTip(
        "Se spuntato, questo nodo eseguirà comandi bash/python ricevuti dal server.\n"
        "Abilita SOLO su reti fidate con token auth impostato.");
    cliSecLay->addWidget(cliTokenLbl);
    cliSecLay->addWidget(m_wanCliTokenEdit, 1);
    cliSecLay->addWidget(m_wanCliTlsCheck);
    cliSecLay->addWidget(m_wanCliShellCheck);
    cliLay->addWidget(cliSecRow);

    /* Log task eseguiti */
    m_wanCliLog = new QTextEdit;
    m_wanCliLog->setReadOnly(true);
    m_wanCliLog->setPlaceholderText(
        "Log task ricevuti ed eseguiti da questo nodo.\n"
        "Connettiti al server per iniziare a ricevere lavoro.");
    cliLay->addWidget(m_wanCliLog, 1);

    m_wanModeStack->addWidget(cliPanel);   /* index 1 = client */

    lanVlay->addWidget(m_wanModeStack, 1);

    /* ── Connessioni modo Server/Client ── */
    connect(modeGrp, &QButtonGroup::idClicked, m_wanModeStack, [this](int id){
        if (m_wanModeStack) m_wanModeStack->setCurrentIndex(id);
    });

    /* ── Connessioni server ── */
    connect(m_wanStartBtn,    &QPushButton::clicked, this, &LanWanPage::onWanStartBtnClicked);
    connect(m_wanAddTaskBtn,  &QPushButton::clicked, this, &LanWanPage::onWanAddTaskBtnClicked);
    connect(m_wanCronStartBtn,&QPushButton::clicked, this, &LanWanPage::onWanCronStartBtnClicked);
    connect(m_wanCronStopBtn, &QPushButton::clicked, this, &LanWanPage::onWanCronStopBtnClicked);

    /* ── Connessioni client ── */
    connect(m_wanCliConBtn,   &QPushButton::clicked, this, &LanWanPage::onWanCliConBtnClicked);
    connect(m_wanCliDisconBtn,&QPushButton::clicked, this, &LanWanPage::onWanCliDisconBtnClicked);

    /* ── Chiude il pannello LAN e lo aggiunge allo stack exec ── */
    m_execModeStack->addWidget(lanWidget);   /* index 1 */

    /* index 2 — Calcolo Scientifico BOINC-like */
    m_sciComputeTab = new SciComputePage(root);
    m_execModeStack->addWidget(m_sciComputeTab);   /* index 2 */

    vlay->addWidget(m_execModeStack, 1);

    /* ── Connessione radio esecuzione → stack ── */
    connect(execGrp, &QButtonGroup::idClicked, m_execModeStack, [this](int id){
        if (m_execModeStack) m_execModeStack->setCurrentIndex(id);
    });

    /* Ripristina la coda WU persistita (task "running" → "pending" dopo un crash). */
    wanLoadTasks();
    wanRefreshTables();

    return root;
}

