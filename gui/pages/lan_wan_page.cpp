#include "lan_wan_page.h"
#include "../lan_server.h"
#include "../prismalux_paths.h"
#include "../app_config.h"
#include "../widgets/qr_code_widget.h"
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
#include <QApplication>
#include <QClipboard>
#include <QUuid>
#include <QFrame>
#include <QLineEdit>
#include <QTextEdit>
#include <QComboBox>
#include <QProcess>
#include <QTcpSocket>
#include <QTimer>
#include <QPointer>
#include <QDesktopServices>
#include <QUrl>
#include <QFile>
#include <QDir>
#include <QTextCursor>

namespace P = PrismaluxPaths;

/* ── Token LAN su file dedicato (0600) ───────────────────────────────────── */
static QString loadLanToken()
{
    QFile f(P::lanTokenPath());
    if (f.open(QIODevice::ReadOnly))
        return QString::fromUtf8(f.readAll()).trimmed();
    /* Migrazione da QSettings */
    const QString old = AppConfig::s().value(P::SK::kLanToken, "").toString();
    if (!old.isEmpty())
        AppConfig::s().remove(P::SK::kLanToken);
    return old;
}

static void saveLanToken(const QString& token)
{
    QDir().mkpath(QDir::homePath() + "/.prismalux");
    const QString path = P::lanTokenPath();
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
    f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    f.write(token.toUtf8());
}

/* ── Helpers ── */
QString LanWanPage::localLanIp() const
{
    QString fallback10;
    for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
        if (iface.flags().testFlag(QNetworkInterface::IsLoopBack)) continue;
        if (!iface.flags().testFlag(QNetworkInterface::IsUp))      continue;
        for (const QNetworkAddressEntry& e : iface.addressEntries()) {
            if (e.ip().protocol() != QAbstractSocket::IPv4Protocol) continue;
            const QString s = e.ip().toString();
            if (s.startsWith("192.168.")) return s;
            if ((s.startsWith("10.") || s.startsWith("172.")) && fallback10.isEmpty())
                fallback10 = s;
        }
    }
    return fallback10.isEmpty() ? "127.0.0.1" : fallback10;
}

QString LanWanPage::serverScheme() const
{
    return (m_lanServer && m_lanServer->isTlsEnabled()) ? "https" : "http";
}

void LanWanPage::openQrDialog(QPushButton* parent, const QString& url,
                               const QString& title, const QString& subtitle,
                               const QString& note)
{
    auto* dlg = new QDialog(parent->window());
    dlg->setWindowTitle(title);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    auto* vl = new QVBoxLayout(dlg);
    vl->setSpacing(12);
    vl->setContentsMargins(20, 20, 20, 20);

    auto* hdr = new QLabel("<b>" + subtitle + "</b>", dlg);
    hdr->setTextFormat(Qt::RichText);
    hdr->setAlignment(Qt::AlignCenter);
    vl->addWidget(hdr);

    auto* qrw = new QrCodeWidget(url, dlg);
    qrw->setFixedSize(260, 260);
    vl->addWidget(qrw, 0, Qt::AlignCenter);

    auto* urlLbl = new QLabel(QString("<code>%1</code>").arg(url), dlg);
    urlLbl->setTextFormat(Qt::RichText);
    urlLbl->setAlignment(Qt::AlignCenter);
    urlLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
    vl->addWidget(urlLbl);

    auto* copyBtn = new QPushButton("\xf0\x9f\x93\x8b" "  Copia URL", dlg);
    connect(copyBtn, &QPushButton::clicked, dlg, [url, copyBtn]() {
        QApplication::clipboard()->setText(url);
        copyBtn->setText("\xe2\x9c\x85" "  Copiato!");
    });
    vl->addWidget(copyBtn);

    if (!note.isEmpty()) {
        auto* noteLbl2 = new QLabel("<small><i>" + note + "</i></small>", dlg);
        noteLbl2->setTextFormat(Qt::RichText);
        noteLbl2->setAlignment(Qt::AlignCenter);
        noteLbl2->setWordWrap(true);
        vl->addWidget(noteLbl2);
    }

    dlg->resize(320, 460);
    dlg->exec();
}

/* ══════════════════════════════════════════════════════════════
   Constructor
   ══════════════════════════════════════════════════════════════ */
LanWanPage::LanWanPage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    auto* tabs = new QTabWidget(this);
    tabs->addTab(buildLanAndroidTab(), "\xf0\x9f\x93\xb1  LAN Android");  /* 📱 */
    tabs->addTab(buildGNS3Tab(),       "\xf0\x9f\x8c\x90  GNS3 MCP");     /* 🌐 */

    connect(m_ai, &AiClient::modelsReady, this, &LanWanPage::onModelsReady);

    /* Tab WAN — placeholder futuro */
    auto* wanTab = new QWidget;
    auto* wanLay = new QVBoxLayout(wanTab);
    wanLay->setContentsMargins(20, 20, 20, 20);
    auto* wanHint = new QLabel(
        "\xf0\x9f\x8c\x90  <b>WAN \xe2\x80\x94 accesso remoto</b><br><br>"  /* 🌐 */
        "Funzionalit\xc3\xa0 in arrivo: tunnel sicuro per accedere a Prismalux\n"
        "da fuori rete locale (VPN, reverse proxy, ngrok, ecc.).",
        wanTab);
    wanHint->setTextFormat(Qt::RichText);
    wanHint->setWordWrap(true);
    wanHint->setObjectName("hintLabel");
    wanLay->addWidget(wanHint);
    wanLay->addStretch();
    tabs->addTab(wanTab, "\xf0\x9f\x8c\x90  WAN");

    lay->addWidget(tabs);
}

/* ══════════════════════════════════════════════════════════════
   Slots — LAN Android tab
   ══════════════════════════════════════════════════════════════ */
void LanWanPage::onModelsReady(const QStringList& models)
{
    if (!m_gns3Model) return;
    const QString cur = m_gns3Model->currentData().toString();
    m_gns3Model->blockSignals(true);
    m_gns3Model->clear();
    namespace P = PrismaluxPaths;
    for (const auto& m : models) {
        const qint64 sz = m_ai->modelSizeBytes(m);
        m_gns3Model->addItem(P::modelIcon(sz, m) + m, m);
    }
    int idx = m_gns3Model->findData(cur.isEmpty() ? m_ai->model() : cur);
    if (idx >= 0) m_gns3Model->setCurrentIndex(idx);
    m_gns3Model->blockSignals(false);
}

void LanWanPage::onTokenTextChanged(const QString& t)
{
    saveLanToken(t);
}

void LanWanPage::onEyeBtnToggled(bool show)
{
    m_lanTokenEdit->setEchoMode(show ? QLineEdit::Normal : QLineEdit::Password);
}

void LanWanPage::onRegenBtnClicked()
{
    const QString t = QUuid::createUuid().toString(QUuid::WithoutBraces)
                      .replace("-","").left(32);
    m_lanTokenEdit->setText(t);
    saveLanToken(t);
}

void LanWanPage::onCopyTokenBtnClicked()
{
    QGuiApplication::clipboard()->setText(m_lanTokenEdit->text().trimmed());
}

void LanWanPage::onQrConnectBtnClicked()
{
    auto* btn = qobject_cast<QPushButton*>(sender());
    const QString url = QString("%1://%2:%3")
                            .arg(serverScheme())
                            .arg(localLanIp())
                            .arg(m_lanPortSpin->value());
    openQrDialog(btn, url,
                 "QR \xe2\x80\x94 Connetti Android",
                 "\xf0\x9f\x93\xb1" "  Scansiona per connettere l\xe2\x80\x99" "app",
                 "Nell\xe2\x80\x99" "app Android: Impostazioni \xe2\x86\x92 URL server.<br>"
                 "Poi avvia il server qui con il pulsante Server ON.");
}

void LanWanPage::onLanPortChanged(int v)
{
    if (m_qrConnectLbl)
        m_qrConnectLbl->setText(
            QString("<small>%1 : %2</small>").arg(m_lanConnectIp).arg(v));
}

void LanWanPage::onQrApkBtnClicked()
{
    if (!m_lanServer || !m_lanServer->isRunning()) return;
    const QString url = QString("%1://%2:%3/apk")
                            .arg(serverScheme()).arg(localLanIp()).arg(m_lanServer->port());
    openQrDialog(m_qrApkBtn, url,
                 "QR \xe2\x80\x94 Scarica APK",
                 "\xf0\x9f\x93\xb1" "  Scansiona per scaricare l'APK",
                 "Il server LAN deve rimanere attivo durante il download.<br>"
                 "Su Android: consenti installazione da sorgenti sconosciute.");
}

void LanWanPage::onQrPageBtnClicked()
{
    if (!m_lanServer || !m_lanServer->isRunning()) return;
    const QString url = QString("%1://%2:%3/")
                            .arg(serverScheme()).arg(localLanIp()).arg(m_lanServer->port());
    openQrDialog(m_qrPageBtn, url,
                 "QR \xe2\x80\x94 Pagina Download",
                 "\xf0\x9f\x8c\x90" "  Scansiona per aprire la pagina di download",
                 "Si apre nel browser del telefono.<br>"
                 "Da l\xc3\xac puoi scaricare l'APK con un tap.");
}

void LanWanPage::onLanServerStatusChanged(bool running)
{
    m_qrApkBtn->setEnabled(running);
    m_qrPageBtn->setEnabled(running);
    m_lanWebBtn->setEnabled(running);
    if (running) {
        const QString proto = m_lanServer->isTlsEnabled()
            ? "\xf0\x9f\x94\x92 HTTPS" : "\xf0\x9f\x94\x93 HTTP";
        m_lanStatusLbl->setText(
            "\xe2\x97\x8f  Attivo — " + proto + " — porta " +
            QString::number(m_lanServer->port()));
        m_lanStatusLbl->setStyleSheet("color: #4caf50; font-weight: bold;");
    } else {
        m_lanStatusLbl->setText("\xe2\x97\x8b  Fermo");
        m_lanStatusLbl->setStyleSheet("color: #9e9e9e;");
        m_lanClientsLbl->setText("Client connessi: 0");
    }
}

void LanWanPage::onLanClientConnected(const QString&)
{
    m_lanClientsLbl->setText(
        "Client connessi: " + QString::number(m_lanServer->clientCount()));
}

void LanWanPage::onLanClientDisconnected(const QString&)
{
    m_lanClientsLbl->setText(
        "Client connessi: " + QString::number(m_lanServer->clientCount()));
}

void LanWanPage::onLanToggleBtnToggled(bool on)
{
    if (on) {
        if (!m_lanServer) {
            m_lanServer = new LanServer(m_ai, this);
            connect(m_lanServer, &LanServer::statusChanged,
                    this, &LanWanPage::onLanServerStatusChanged);
            connect(m_lanServer, &LanServer::clientConnected,
                    this, &LanWanPage::onLanClientConnected);
            connect(m_lanServer, &LanServer::clientDisconnected,
                    this, &LanWanPage::onLanClientDisconnected);
        }
        const quint16 port = static_cast<quint16>(m_lanPortSpin->value());
        {
            QString tok = m_lanTokenEdit->text().trimmed();
            if (tok.isEmpty()) {
                tok = QUuid::createUuid().toString(QUuid::WithoutBraces)
                      .replace("-","").left(32);
                m_lanTokenEdit->setText(tok);
                saveLanToken(tok);
            }
            m_lanServer->setAccessToken(tok);
        }
        if (m_lanServer->start(port)) {
            m_lanToggleBtn->setText("\xe2\x97\x8f  Server ON");
            m_lanPortSpin->setEnabled(false);
        } else {
            m_lanToggleBtn->blockSignals(true);
            m_lanToggleBtn->setChecked(false);
            m_lanToggleBtn->blockSignals(false);
            m_lanStatusLbl->setText("\xe2\x9d\x8c  Impossibile aprire la porta");
            m_lanStatusLbl->setStyleSheet("color: #f44336;");
        }
    } else {
        if (m_lanServer) m_lanServer->stop();
        m_lanToggleBtn->setText("\xe2\x97\x8b  Server OFF");
        m_lanPortSpin->setEnabled(true);
        m_qrApkBtn->setEnabled(false);
        m_qrPageBtn->setEnabled(false);
        m_lanWebBtn->setEnabled(false);
    }
}

void LanWanPage::onLanWebBtnClicked()
{
    if (!m_lanServer || !m_lanServer->isRunning()) return;
    const QString url = QString("%1://%2:%3/web")
                            .arg(serverScheme()).arg(localLanIp()).arg(m_lanServer->port());
    QDesktopServices::openUrl(QUrl(url));
}

/* ══════════════════════════════════════════════════════════════
   buildLanAndroidTab
   ══════════════════════════════════════════════════════════════ */
QWidget* LanWanPage::buildLanAndroidTab()
{
    auto* tab = new QWidget(this);

    m_lanConnectIp = localLanIp();
    const QString& ip = m_lanConnectIp;

    auto* vbox = new QVBoxLayout(tab);
    vbox->setContentsMargins(12, 12, 12, 12);
    vbox->setSpacing(10);

    auto* titleLbl = new QLabel(
        "<b>\xf0\x9f\x93\xb1  Server LAN per Android</b>", tab);
    titleLbl->setTextFormat(Qt::RichText);
    vbox->addWidget(titleLbl);

    auto* group = new QGroupBox(tab);
    group->setObjectName("LanServerGroup");
    auto* gl = new QVBoxLayout(group);
    gl->setSpacing(8);

    auto* ctrlRow = new QHBoxLayout;
    m_lanToggleBtn = new QPushButton(
        "\xe2\x97\x8b  Server OFF", group);
    m_lanToggleBtn->setCheckable(true);
    m_lanToggleBtn->setObjectName("LanToggleBtn");

    m_lanPortSpin = new QSpinBox(group);
    m_lanPortSpin->setRange(1024, 65535);
    m_lanPortSpin->setValue(11500);
    m_lanPortSpin->setPrefix("Porta ");
    m_lanPortSpin->setObjectName("LanPortSpin");

    ctrlRow->addWidget(m_lanToggleBtn, 1);
    ctrlRow->addWidget(m_lanPortSpin);
    gl->addLayout(ctrlRow);

    /* ── Token di accesso (opzionale) ── */
    {
        auto* tokenRow = new QWidget(group);
        auto* tokenLay = new QHBoxLayout(tokenRow);
        tokenLay->setContentsMargins(0, 0, 0, 0);
        tokenLay->setSpacing(6);

        auto* tokenLbl = new QLabel(
            "\xf0\x9f\x94\x91" "  Token:", tokenRow);   /* 🔑 */
        tokenLbl->setToolTip(
            "Token di accesso Bearer opzionale.\n"
            "Se impostato, l\xe2\x80\x99" "app Android e la Chat Web devono inviare\n"
            "l\xe2\x80\x99" "header: Authorization: Bearer <token>\n"
            "Lascia vuoto per non richiedere autenticazione.");

        m_lanTokenEdit = new QLineEdit(tokenRow);
        m_lanTokenEdit->setPlaceholderText(
            "Auto-generato all\xe2\x80\x99" "avvio se lasci vuoto");
        m_lanTokenEdit->setEchoMode(QLineEdit::Password);
        m_lanTokenEdit->setToolTip(
            "Token di accesso Bearer (obbligatorio).\n"
            "Se lasci vuoto viene generato automaticamente all\xe2\x80\x99" "avvio.\n"
            "L\xe2\x80\x99" "app Android deve inviare:\n"
            "  Authorization: Bearer <token>");

        /* Carica da ~/.prismalux/lan_token.key (0600); genera se non esiste */
        {
            QString saved = loadLanToken();
            if (saved.isEmpty()) {
                saved = QUuid::createUuid().toString(QUuid::WithoutBraces)
                        .replace("-","").left(32);
            }
            saveLanToken(saved);
            m_lanTokenEdit->setText(saved);
        }

        connect(m_lanTokenEdit, &QLineEdit::textChanged,
                this, &LanWanPage::onTokenTextChanged);

        auto* eyeBtn = new QPushButton("\xf0\x9f\x91\x81", tokenRow);   /* 👁 */
        eyeBtn->setFixedWidth(32);
        eyeBtn->setCheckable(true);
        eyeBtn->setToolTip("Mostra/nascondi token");
        eyeBtn->setFlat(true);
        connect(eyeBtn, &QPushButton::toggled,
                this, &LanWanPage::onEyeBtnToggled);

        auto* regenBtn = new QPushButton("\xf0\x9f\x94\x84", tokenRow);  /* 🔄 */
        regenBtn->setFixedWidth(32);
        regenBtn->setFlat(true);
        regenBtn->setToolTip("Genera nuovo token casuale");
        connect(regenBtn, &QPushButton::clicked,
                this, &LanWanPage::onRegenBtnClicked);

        auto* copyBtn = new QPushButton("\xf0\x9f\x93\x8b", tokenRow);   /* 📋 */
        copyBtn->setFixedWidth(32);
        copyBtn->setFlat(true);
        copyBtn->setToolTip("Copia token negli appunti");
        connect(copyBtn, &QPushButton::clicked,
                this, &LanWanPage::onCopyTokenBtnClicked);

        tokenLay->addWidget(tokenLbl);
        tokenLay->addWidget(m_lanTokenEdit, 1);
        tokenLay->addWidget(eyeBtn);
        tokenLay->addWidget(copyBtn);
        tokenLay->addWidget(regenBtn);
        gl->addWidget(tokenRow);
    }

    m_lanStatusLbl = new QLabel("\xe2\x97\x8b  Fermo", group);
    m_lanStatusLbl->setStyleSheet("color: #9e9e9e;");
    gl->addWidget(m_lanStatusLbl);

    m_lanClientsLbl = new QLabel("Client connessi: 0", group);
    gl->addWidget(m_lanClientsLbl);

    auto* ipLbl = new QLabel(
        QString("IP del PC: <b>%1</b>").arg(ip), group);
    ipLbl->setTextFormat(Qt::RichText);
    gl->addWidget(ipLbl);

    auto* noteLbl = new QLabel(
        "<small>Nell\xe2\x80\x99" "app Android: IP = <b>" + ip + "</b>"
        ", Porta = <b>11500</b></small>", group);
    noteLbl->setTextFormat(Qt::RichText);
    noteLbl->setWordWrap(true);
    gl->addWidget(noteLbl);

    /* ── QR Connetti (sempre visibile) ── */
    {
        auto* connectRow  = new QWidget(group);
        auto* connectLay  = new QHBoxLayout(connectRow);
        connectLay->setContentsMargins(0, 0, 0, 0);
        connectLay->setSpacing(8);

        auto* qrConnectBtn = new QPushButton(
            "\xf0\x9f\x93\xb1" "  QR Connetti", connectRow);   /* 📱 */
        qrConnectBtn->setObjectName("actionBtn");
        qrConnectBtn->setToolTip(
            "Mostra il QR con l\xe2\x80\x99" "indirizzo del server.\n"
            "Scansiona dall\xe2\x80\x99" "app Android per configurare automaticamente l\xe2\x80\x99" "IP.\n"
            "Puoi scansionarlo anche prima di avviare il server.");

        m_qrConnectLbl = new QLabel(
            QString("<small>%1 : %2</small>")
                .arg(ip).arg(m_lanPortSpin->value()), connectRow);
        m_qrConnectLbl->setObjectName("hintLabel");
        m_qrConnectLbl->setTextFormat(Qt::RichText);

        connectLay->addWidget(qrConnectBtn);
        connectLay->addWidget(m_qrConnectLbl, 1);
        gl->addWidget(connectRow);

        connect(qrConnectBtn, &QPushButton::clicked,
                this, &LanWanPage::onQrConnectBtnClicked);

        connect(m_lanPortSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                this, &LanWanPage::onLanPortChanged);
    }

    /* ── Due bottoni QR affiancati (APK e Pagina) ── */
    auto* qrRow  = new QWidget(group);
    auto* qrRowL = new QHBoxLayout(qrRow);
    qrRowL->setContentsMargins(0, 0, 0, 0);
    qrRowL->setSpacing(8);

    m_qrApkBtn = new QPushButton(
        "\xf0\x9f\x93\xa6" "  QR Scarica APK", qrRow);          /* 📦 */
    m_qrApkBtn->setObjectName("actionBtn");
    m_qrApkBtn->setToolTip("QR code per scaricare direttamente PrismaluxMobile.apk (server ON richiesto)");
    m_qrApkBtn->setEnabled(false);

    m_qrPageBtn = new QPushButton(
        "\xf0\x9f\x8c\x90" "  QR Pagina Download", qrRow);      /* 🌐 */
    m_qrPageBtn->setObjectName("actionBtn");
    m_qrPageBtn->setToolTip("QR code per aprire la pagina di download nel browser del telefono (server ON richiesto)");
    m_qrPageBtn->setEnabled(false);

    qrRowL->addWidget(m_qrApkBtn, 1);
    qrRowL->addWidget(m_qrPageBtn, 1);
    gl->addWidget(qrRow);

    /* ── Pulsante Chat Web ── */
    m_lanWebBtn = new QPushButton(
        "\xf0\x9f\x8c\x90  Chat Web (altri PC nella LAN)", group);  /* 🌐 */
    m_lanWebBtn->setObjectName("actionBtn");
    m_lanWebBtn->setToolTip(
        "Apre nel browser l\xe2\x80\x99" "interfaccia chat web servita da Prismalux.\n"
        "Gli altri PC della rete locale possono connettersi all\xe2\x80\x99" "indirizzo mostrato\n"
        "e chattare con il modello AI senza installare nulla.");
    m_lanWebBtn->setEnabled(false);
    gl->addWidget(m_lanWebBtn);

    vbox->addWidget(group);
    vbox->addStretch();

    connect(m_qrApkBtn,  &QPushButton::clicked, this, &LanWanPage::onQrApkBtnClicked);
    connect(m_qrPageBtn, &QPushButton::clicked, this, &LanWanPage::onQrPageBtnClicked);
    connect(m_lanToggleBtn, &QPushButton::toggled, this, &LanWanPage::onLanToggleBtnToggled);
    connect(m_lanWebBtn,    &QPushButton::clicked, this, &LanWanPage::onLanWebBtnClicked);

    return tab;
}

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

void LanWanPage::gns3PopulateModels(QComboBox* combo)
{
    namespace P = PrismaluxPaths;
    combo->clear();
    const QString cur = m_ai->model();
    if (!cur.isEmpty()) combo->addItem(cur, cur);
    combo->setCurrentIndex(0);
    m_ai->fetchModels();
}

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
                "\xf0\x9f\x8c\x90  Codice pronto \xe2\x80\x94 premi Esegui su GNS3");
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
}

/* ══════════════════════════════════════════════════════════════
   Slots — GNS3 ping
   ══════════════════════════════════════════════════════════════ */
void LanWanPage::onPingBtnClicked()
{
    const QString addr = m_gns3HostEdit->text().trimmed();
    const QString host = addr.contains(':') ? addr.section(':', 0, 0) : addr;
    const int port = addr.contains(':') ? addr.section(':', 1).toInt() : 3080;
    m_gns3StatusLbl->setText("\xf0\x9f\x94\x84  Connessione...");
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
            m_gns3StatusLbl->setText("\xe2\x9d\x8c  Timeout");
            sockPtr->abort(); sockPtr->deleteLater();
        }
    });
}

void LanWanPage::onGns3SockConnected()
{
    auto* sock = qobject_cast<QTcpSocket*>(sender());
    if (sock) { sock->disconnectFromHost(); sock->deleteLater(); }
    m_gns3StatusLbl->setText("\xe2\x9c\x85  Server raggiungibile");
    m_gns3ExecBtn->setEnabled(!m_gns3Code.isEmpty());
}

void LanWanPage::onGns3SockError(QAbstractSocket::SocketError)
{
    auto* sock = qobject_cast<QTcpSocket*>(sender());
    if (!sock) return;
    m_gns3StatusLbl->setText("\xe2\x9d\x8c  " + sock->errorString());
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
        m_gns3ExecProc->waitForFinished(500);
    }
    const QString tmpPath = QDir::tempPath() + "/prismalux_gns3_script.py";
    QFile f(tmpPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_gns3StatusLbl->setText("\xe2\x9d\x8c  Impossibile creare script");
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
    m_gns3StatusLbl->setText("\xf0\x9f\x94\x84  Esecuzione script Python...");
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

    auto* lbl = new QLabel("GNS3 REST API:", connRow);
    lbl->setObjectName("hintLabel");
    m_gns3HostEdit = new QLineEdit("localhost:3080", connRow);
    m_gns3HostEdit->setFixedWidth(150);
    auto* pingBtn = new QPushButton("\xf0\x9f\x94\x97  Verifica", connRow);
    pingBtn->setObjectName("actionBtn");
    pingBtn->setFixedWidth(100);
    m_gns3StatusLbl = new QLabel("\xe2\x9a\xaa  Non connesso", connRow);
    m_gns3StatusLbl->setObjectName("hintLabel");
    m_gns3ExecBtn = new QPushButton("\xf0\x9f\x8c\x90  Esegui su GNS3", connRow);
    m_gns3ExecBtn->setObjectName("actionBtn");
    m_gns3ExecBtn->setFixedWidth(160);
    m_gns3ExecBtn->setEnabled(false);

    connLay->addWidget(lbl);
    connLay->addWidget(m_gns3HostEdit);
    connLay->addWidget(pingBtn);
    connLay->addWidget(m_gns3StatusLbl, 1);
    connLay->addWidget(m_gns3ExecBtn);
    lay->addWidget(connRow);

    auto* hintLbl = new QLabel(
        "\xf0\x9f\x8c\x90 <b>GNS3 MCP:</b> simulatore reti. "
        "Avvia GNS3 (porta 3080 di default) e installa: "
        "<code>pip install gns3fy requests</code><br>"
        "Plugin: <a href='https://github.com/ChistokhinSV/gns3-mcp'>gns3-mcp</a>", w);
    hintLbl->setObjectName("hintLabel");
    hintLbl->setOpenExternalLinks(true);
    hintLbl->setWordWrap(true);
    lay->addWidget(hintLbl);

    /* Azione + Modello */
    auto* toolRow = new QWidget(w);
    auto* toolLay = new QHBoxLayout(toolRow);
    toolLay->setContentsMargins(0, 0, 0, 0);
    toolLay->setSpacing(8);
    m_gns3Action = new QComboBox(toolRow);
    for (int i = 0; kGNS3Actions[i]; i++)
        m_gns3Action->addItem(QString::fromUtf8(kGNS3Actions[i]));
    m_gns3Model = new QComboBox(toolRow);
    m_gns3Model->setMinimumWidth(180);
    gns3PopulateModels(m_gns3Model);
    toolLay->addWidget(new QLabel("Azione:", toolRow));
    toolLay->addWidget(m_gns3Action, 1);
    toolLay->addWidget(new QLabel("Modello:", toolRow));
    toolLay->addWidget(m_gns3Model, 1);
    lay->addWidget(toolRow);

    m_gns3Input = new QTextEdit(w);
    m_gns3Input->setPlaceholderText(
        "Descrivi la rete da simulare...\n"
        "Es: 'Crea una topologia con 2 router Cisco e 3 PC in una LAN'\n"
        "Es: 'Configura OSPF tra R1 e R2 con redistribuzione statica'");
    m_gns3Input->setFixedHeight(80);
    lay->addWidget(m_gns3Input);

    auto* btnRow = new QWidget(w);
    auto* btnLay = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(0, 0, 0, 0);
    m_gns3RunBtn  = new QPushButton("\xf0\x9f\xa4\x96  Genera script GNS3", btnRow);
    m_gns3RunBtn->setObjectName("actionBtn");
    m_gns3StopBtn = new QPushButton("\xe2\x8f\xb9  Stop", btnRow);
    m_gns3StopBtn->setObjectName("actionBtn");
    m_gns3StopBtn->setProperty("danger", true);
    m_gns3StopBtn->setEnabled(false);
    btnLay->addWidget(m_gns3RunBtn);
    btnLay->addWidget(m_gns3StopBtn);
    btnLay->addStretch();
    lay->addWidget(btnRow);

    m_gns3Progress = new QProgressBar(w);
    m_gns3Progress->setRange(0, 0);   /* indeterminate */
    m_gns3Progress->setFixedHeight(4);
    m_gns3Progress->setTextVisible(false);
    m_gns3Progress->hide();
    lay->addWidget(m_gns3Progress);

    m_gns3Output = new QTextEdit(w);
    m_gns3Output->setReadOnly(true);
    m_gns3Output->setObjectName("outputView");
    m_gns3Output->setPlaceholderText("Script Python GNS3 REST API appare qui...");
    lay->addWidget(m_gns3Output, 1);

    m_gns3ErrorPanel = new AiErrorWidget(w);
    lay->addWidget(m_gns3ErrorPanel);

    connect(pingBtn,       &QPushButton::clicked, this, &LanWanPage::onPingBtnClicked);
    connect(m_gns3ExecBtn, &QPushButton::clicked, this, &LanWanPage::onGns3ExecBtnClicked);
    connect(m_gns3RunBtn,  &QPushButton::clicked, this, &LanWanPage::onGns3RunBtnClicked);
    connect(m_gns3StopBtn, &QPushButton::clicked, this, &LanWanPage::onGns3StopBtnClicked);

    return w;
}
