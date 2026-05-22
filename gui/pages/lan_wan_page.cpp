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
#include <QHeaderView>
#include <QDateTime>
#include <QRegularExpression>
#include <QStandardPaths>

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
   Destructor — reset auto-start so server doesn't restart next launch
   ══════════════════════════════════════════════════════════════ */
LanWanPage::~LanWanPage()
{
    AppConfig::s().setValue(P::SK::kLanAutoStart, false);
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

    /* Ripristina porta salvata e, se era attivo, avvia automaticamente */
    const int savedPort = AppConfig::s().value(P::SK::kLanPort, 11500).toInt();
    if (m_lanPortSpin) m_lanPortSpin->setValue(savedPort);

    if (AppConfig::s().value(P::SK::kLanAutoStart, false).toBool()) {
        QTimer::singleShot(0, this, [this]() {
            if (m_lanToggleBtn && !m_lanToggleBtn->isChecked())
                m_lanToggleBtn->setChecked(true);
        });
    }
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
    const QString ip    = localLanIp();
    const int     port  = m_lanPortSpin->value();
    const QString token = m_lanTokenEdit ? m_lanTokenEdit->text().trimmed() : QString();

    /* URL formato http://IP:PORT/web?token=TOKEN
       - È un link reale: si apre nel browser del telefono con la web chat già autenticata
       - L'app Android lo scansiona e compila IP, porta e token automaticamente */
    QString url = QString("http://%1:%2/web").arg(ip).arg(port);
    if (!token.isEmpty()) {
        const QString enc = QString::fromLatin1(QUrl::toPercentEncoding(token));
        url += "?token=" + enc;
    }

    const QString note = token.isEmpty()
        ? "\xf0\x9f\x93\xb1" "  Impostazioni \xe2\x86\x92 \xf0\x9f\x93\xb7 Scansiona QR<br>"  /* 📱 📷 */
          "<small>Nessun token — aggiungi un token per la sicurezza</small>"
        : "\xf0\x9f\x93\xb1" "  Impostazioni \xe2\x86\x92 \xf0\x9f\x93\xb7 Scansiona QR<br>"
          "<small>IP + porta + token sono gi\xc3\xa0 inclusi nel QR</small>";

    openQrDialog(btn, url,
                 "QR \xe2\x80\x94 Connetti Prismalux Mobile",
                 "\xf0\x9f\x94\x91" "  Scansiona nell\xe2\x80\x99" "app per configurare tutto in un tap",
                 note);
}

void LanWanPage::onLanPortChanged(int v)
{
    Q_UNUSED(v)
    onUpdateQrInline();
}

void LanWanPage::onUpdateQrInline()
{
    if (!m_qrInlineWidget) return;
    const QString ip    = localLanIp();
    const int     port  = m_lanPortSpin ? m_lanPortSpin->value() : 11500;
    const QString token = m_lanTokenEdit ? m_lanTokenEdit->text().trimmed() : QString();
    QString url = QString("http://%1:%2/web").arg(ip).arg(port);
    if (!token.isEmpty())
        url += "?token=" + QString::fromLatin1(QUrl::toPercentEncoding(token));
    m_qrInlineWidget->setText(url);
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
    }
}

/* ── Helper: legge MAC da /proc/net/arp dato un IP ── */
QString LanWanPage::readMacForIp(const QString& ip)
{
    QFile f("/proc/net/arp");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return "\xe2\x80\x94";
    QTextStream ts(&f);
    ts.readLine(); // salta intestazione
    while (!ts.atEnd()) {
        const QString line = ts.readLine();
        const QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (parts.size() >= 4 && parts[0] == ip)
            return parts[3].toUpper();
    }
    return "\xe2\x80\x94";
}

/* ── Helper: aggiunge una riga alla tabella client ── */
void LanWanPage::clientTableAddRow(const QString& ip)
{
    if (!m_clientTable) return;
    const int row = m_clientTable->rowCount();
    m_clientTable->insertRow(row);
    m_clientTable->setItem(row, 0, new QTableWidgetItem(ip));
    m_clientTable->setItem(row, 1, new QTableWidgetItem(readMacForIp(ip)));
    m_clientTable->setItem(row, 2, new QTableWidgetItem(
        QDateTime::currentDateTime().toString("HH:mm:ss")));
}

/* ── Helper: rimuove la riga con quell'IP dalla tabella ── */
void LanWanPage::clientTableRemoveRow(const QString& ip)
{
    if (!m_clientTable) return;
    for (int r = m_clientTable->rowCount() - 1; r >= 0; --r) {
        auto* it = m_clientTable->item(r, 0);
        if (it && it->text() == ip) { m_clientTable->removeRow(r); break; }
    }
}

void LanWanPage::onLanClientConnected(const QString& addr)
{
    clientTableAddRow(addr);
}

void LanWanPage::onLanClientDisconnected(const QString& addr)
{
    clientTableRemoveRow(addr);
}

void LanWanPage::onKickBtnClicked()
{
    if (!m_lanServer || !m_clientTable) return;
    const int row = m_clientTable->currentRow();
    if (row < 0) return;
    auto* it = m_clientTable->item(row, 0);
    if (it) m_lanServer->kickClient(it->text());
}

void LanWanPage::onKickAllBtnClicked()
{
    if (!m_lanServer || !m_clientTable) return;
    QStringList ips;
    for (int r = 0; r < m_clientTable->rowCount(); ++r) {
        auto* it = m_clientTable->item(r, 0);
        if (it) ips << it->text();
    }
    for (const QString& ip : ips)
        m_lanServer->kickClient(ip);
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
            AppConfig::s().setValue(P::SK::kLanAutoStart, true);
            AppConfig::s().setValue(P::SK::kLanPort, (int)port);
        } else {
            m_lanToggleBtn->blockSignals(true);
            m_lanToggleBtn->setChecked(false);
            m_lanToggleBtn->blockSignals(false);
            m_lanStatusLbl->setText("\xe2\x9d\x8c  Impossibile aprire la porta");
            m_lanStatusLbl->setStyleSheet("color: #f44336;");
            AppConfig::s().setValue(P::SK::kLanAutoStart, false);
        }
    } else {
        if (m_lanServer) m_lanServer->stop();
        m_lanToggleBtn->setText("\xe2\x97\x8b  Server OFF");
        m_lanPortSpin->setEnabled(true);
        m_qrApkBtn->setEnabled(false);
        m_qrPageBtn->setEnabled(false);
        m_lanWebBtn->setEnabled(false);
        AppConfig::s().setValue(P::SK::kLanAutoStart, false);
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

    auto* rootLay = new QVBoxLayout(tab);
    rootLay->setContentsMargins(12, 10, 12, 10);
    rootLay->setSpacing(8);

    auto* titleLbl = new QLabel(
        "<b>\xf0\x9f\x93\xb1  Server LAN per Android</b>", tab);
    titleLbl->setTextFormat(Qt::RichText);
    rootLay->addWidget(titleLbl);

    /* ── Layout orizzontale fisso 50/50: SINISTRA = controllo + client  DESTRA = QR ── */
    auto* split    = new QWidget(tab);
    auto* splitLay = new QHBoxLayout(split);
    splitLay->setContentsMargins(0, 0, 0, 0);
    splitLay->setSpacing(0);

    /* ══════════════════════════════════════════════════════════
       SINISTRA: controllo server + tabella client connessi
       ══════════════════════════════════════════════════════════ */
    auto* leftW   = new QWidget(split);
    auto* leftLay = new QVBoxLayout(leftW);
    leftLay->setContentsMargins(0, 0, 4, 0);
    leftLay->setSpacing(8);

    /* Controllo server */
    auto* srvGroup = new QGroupBox("\xf0\x9f\x94\xa7  Controllo server", leftW);
    auto* srvLay   = new QVBoxLayout(srvGroup);
    srvLay->setSpacing(6);

    auto* ctrlRow = new QHBoxLayout;
    m_lanToggleBtn = new QPushButton("\xe2\x97\x8b  Server OFF", srvGroup);
    m_lanToggleBtn->setCheckable(true);
    m_lanToggleBtn->setObjectName("LanToggleBtn");
    m_lanPortSpin = new QSpinBox(srvGroup);
    m_lanPortSpin->setRange(1024, 65535);
    m_lanPortSpin->setValue(11500);
    m_lanPortSpin->setPrefix("Porta ");
    m_lanPortSpin->setObjectName("LanPortSpin");
    ctrlRow->addWidget(m_lanToggleBtn, 1);
    ctrlRow->addWidget(m_lanPortSpin);
    srvLay->addLayout(ctrlRow);

    /* Token */
    {
        auto* tokenRow = new QWidget(srvGroup);
        auto* tokenLay = new QHBoxLayout(tokenRow);
        tokenLay->setContentsMargins(0, 0, 0, 0); tokenLay->setSpacing(4);
        auto* tokenLbl = new QLabel("\xf0\x9f\x94\x91  Token:", tokenRow);
        m_lanTokenEdit = new QLineEdit(tokenRow);
        m_lanTokenEdit->setPlaceholderText("Auto-generato all\xe2\x80\x99" "avvio");
        m_lanTokenEdit->setEchoMode(QLineEdit::Password);
        QString saved = loadLanToken();
        if (saved.isEmpty()) {
            saved = QUuid::createUuid().toString(QUuid::WithoutBraces).replace("-","").left(32);
        }
        saveLanToken(saved);
        m_lanTokenEdit->setText(saved);
        connect(m_lanTokenEdit, &QLineEdit::textChanged,
                this, &LanWanPage::onTokenTextChanged);
        auto* eyeBtn = new QPushButton("\xf0\x9f\x91\x81", tokenRow);
        eyeBtn->setFixedWidth(28); eyeBtn->setCheckable(true); eyeBtn->setFlat(true);
        connect(eyeBtn, &QPushButton::toggled, this, &LanWanPage::onEyeBtnToggled);
        auto* copyBtn = new QPushButton("\xf0\x9f\x93\x8b", tokenRow);
        copyBtn->setFixedWidth(28); copyBtn->setFlat(true);
        connect(copyBtn, &QPushButton::clicked, this, &LanWanPage::onCopyTokenBtnClicked);
        auto* regenBtn = new QPushButton("\xf0\x9f\x94\x84", tokenRow);
        regenBtn->setFixedWidth(28); regenBtn->setFlat(true);
        connect(regenBtn, &QPushButton::clicked, this, &LanWanPage::onRegenBtnClicked);
        tokenLay->addWidget(tokenLbl);
        tokenLay->addWidget(m_lanTokenEdit, 1);
        tokenLay->addWidget(eyeBtn);
        tokenLay->addWidget(copyBtn);
        tokenLay->addWidget(regenBtn);
        srvLay->addWidget(tokenRow);
    }

    m_lanStatusLbl = new QLabel("\xe2\x97\x8b  Fermo", srvGroup);
    m_lanStatusLbl->setStyleSheet("color: #9e9e9e;");
    srvLay->addWidget(m_lanStatusLbl);

    srvLay->addWidget(new QLabel(
        QString("IP del PC: <b>%1</b>").arg(ip), srvGroup));

    connect(m_lanPortSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &LanWanPage::onLanPortChanged);

    leftLay->addWidget(srvGroup);

    /* Tabella client connessi */
    auto* clientGroup = new QGroupBox(
        "\xf0\x9f\x94\x92  Controllo accessi al Server Locale", leftW);
    auto* clientLay   = new QVBoxLayout(clientGroup);
    clientLay->setSpacing(4);

    m_clientTable = new QTableWidget(0, 3, clientGroup);
    m_clientTable->setHorizontalHeaderLabels({"Indirizzo IP", "MAC Address", "Connesso alle"});
    m_clientTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_clientTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_clientTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_clientTable->verticalHeader()->setVisible(false);
    m_clientTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_clientTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_clientTable->setAlternatingRowColors(true);
    clientLay->addWidget(m_clientTable, 1);

    auto* kickRow = new QWidget(clientGroup);
    auto* kickLay = new QHBoxLayout(kickRow);
    kickLay->setContentsMargins(0, 0, 0, 0); kickLay->setSpacing(6);
    m_kickBtn = new QPushButton("\xf0\x9f\x9a\xab  Disconnetti selezionato", kickRow);
    m_kickBtn->setObjectName("actionBtn");
    m_kickBtn->setProperty("danger", true);
    m_kickBtn->setToolTip("Chiude la connessione del client selezionato");
    m_kickAllBtn = new QPushButton("\xf0\x9f\x9a\xab  Disconnetti tutti", kickRow);
    m_kickAllBtn->setObjectName("actionBtn");
    m_kickAllBtn->setProperty("danger", true);
    m_kickAllBtn->setToolTip("Chiude tutte le connessioni client attive");
    kickLay->addWidget(m_kickBtn, 1);
    kickLay->addWidget(m_kickAllBtn, 1);
    clientLay->addWidget(kickRow);

    connect(m_kickBtn,    &QPushButton::clicked, this, &LanWanPage::onKickBtnClicked);
    connect(m_kickAllBtn, &QPushButton::clicked, this, &LanWanPage::onKickAllBtnClicked);

    leftLay->addWidget(clientGroup, 1);

    /* divisore verticale visibile */
    auto* divider = new QFrame(split);
    divider->setFrameShape(QFrame::VLine);
    divider->setFrameShadow(QFrame::Sunken);

    splitLay->addWidget(leftW, 1);
    splitLay->addWidget(divider);

    /* ══════════════════════════════════════════════════════════
       DESTRA: QR + istruzioni + pulsanti
       ══════════════════════════════════════════════════════════ */
    auto* rightW   = new QWidget(split);
    auto* rightLay = new QVBoxLayout(rightW);
    rightLay->setContentsMargins(4, 0, 0, 0);
    rightLay->setSpacing(8);

    /* QR inline — verticale: QR sopra, testo sotto */
    m_qrInlineWidget = new QrCodeWidget(QString(), rightW);
    m_qrInlineWidget->setFixedSize(290, 290);
    m_qrInlineWidget->setToolTip(
        "QR di connessione rapida. Si aggiorna con IP, porta e token.");
    rightLay->addWidget(m_qrInlineWidget, 0, Qt::AlignHCenter);

    auto* qrInfoLbl = new QLabel(rightW);
    qrInfoLbl->setTextFormat(Qt::RichText);
    qrInfoLbl->setWordWrap(true);
    qrInfoLbl->setAlignment(Qt::AlignCenter);
    qrInfoLbl->setText(
        "<span style='font-size:14px;'>"
        "<b>\xf0\x9f\x93\xb1  Connetti l\xe2\x80\x99" "app Android</b></span><br>"
        "<span style='font-size:13px;'>"
        "1. Avvia <b>PrismaluxMobile</b> sul telefono<br>"
        "2. Apri <b>Impostazioni</b> \xe2\x86\x92 "
        "<b>\xf0\x9f\x93\xb7 Scansiona QR dal PC</b><br>"
        "3. Punta la fotocamera su questo QR<br><br>"
        "<i>IP + Porta + Token vengono configurati in automatico.</i><br>"
        "<span style='color:#9e9e9e;'>Puoi scansionare anche con il server fermo "
        "per pre-configurare l\xe2\x80\x99" "app.</span></span>");
    rightLay->addWidget(qrInfoLbl);

    /* URL corrente */
    auto* urlLbl = new QLabel(rightW);
    urlLbl->setObjectName("hintLabel");
    urlLbl->setTextFormat(Qt::RichText);
    urlLbl->setText(QString("<small><b>%1</b> : %2</small>")
                        .arg(ip).arg(m_lanPortSpin->value()));
    rightLay->addWidget(urlLbl);

    /* Aggiorna QR a ogni modifica */
    connect(m_lanTokenEdit, &QLineEdit::textChanged,
            this, &LanWanPage::onUpdateQrInline);
    connect(m_lanPortSpin,  QOverload<int>::of(&QSpinBox::valueChanged),
            this, &LanWanPage::onUpdateQrInline);
    onUpdateQrInline();

    auto* sep = new QFrame(rightW);
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Sunken);
    rightLay->addWidget(sep);

    /* Pulsante QR Connetti (apre dialog QR più grande) */
    auto* qrConnectBtn = new QPushButton(
        "\xf0\x9f\x93\xb1  QR Connetti (schermo intero)", rightW);
    qrConnectBtn->setObjectName("actionBtn");
    qrConnectBtn->setToolTip("Mostra il QR in un dialogo grande");
    connect(qrConnectBtn, &QPushButton::clicked,
            this, &LanWanPage::onQrConnectBtnClicked);
    rightLay->addWidget(qrConnectBtn);

    /* QR APK + Pagina */
    auto* qrRow  = new QWidget(rightW);
    auto* qrRowL = new QHBoxLayout(qrRow);
    qrRowL->setContentsMargins(0, 0, 0, 0); qrRowL->setSpacing(6);
    m_qrApkBtn  = new QPushButton("\xf0\x9f\x93\xa6  QR APK",  qrRow);
    m_qrApkBtn->setObjectName("actionBtn");
    m_qrApkBtn->setEnabled(false);
    m_qrPageBtn = new QPushButton("\xf0\x9f\x8c\x90  QR Pagina", qrRow);
    m_qrPageBtn->setObjectName("actionBtn");
    m_qrPageBtn->setEnabled(false);
    qrRowL->addWidget(m_qrApkBtn, 1);
    qrRowL->addWidget(m_qrPageBtn, 1);
    rightLay->addWidget(qrRow);

    m_lanWebBtn = new QPushButton(
        "\xf0\x9f\x8c\x90  Apri Chat Web nel browser", rightW);
    m_lanWebBtn->setObjectName("actionBtn");
    m_lanWebBtn->setEnabled(false);
    rightLay->addWidget(m_lanWebBtn);

    /* ── Installazione APK via USB (adb) ── */
    auto* adbSep = new QFrame(rightW);
    adbSep->setFrameShape(QFrame::HLine);
    adbSep->setFrameShadow(QFrame::Sunken);
    rightLay->addWidget(adbSep);

    const QString adbPath = findAdb();
    m_adbInstallBtn = new QPushButton(
        "\xf0\x9f\x94\x8c  Installa APK via USB  (adb)", rightW);
    m_adbInstallBtn->setObjectName("primaryBtn");
    m_adbInstallBtn->setToolTip(
        adbPath.isEmpty()
            ? "adb non trovato — installa Android Platform Tools"
            : QString("adb: %1").arg(adbPath));
    m_adbInstallBtn->setEnabled(!adbPath.isEmpty());
    m_adbInstallBtn->setAccessibleName("Installa APK PrismaluxMobile sul telefono Android via USB");
    rightLay->addWidget(m_adbInstallBtn);

    m_adbStatusLbl = new QLabel(
        adbPath.isEmpty()
            ? "\xe2\x9a\xa0\xef\xb8\x8f  adb non trovato. Installa: sudo apt install adb"
            : "\xe2\x84\xb9  Collega il telefono via USB con debug USB attivo, poi premi il pulsante.",
        rightW);
    m_adbStatusLbl->setWordWrap(true);
    m_adbStatusLbl->setStyleSheet("color:#aaa;font-size:11px;");
    rightLay->addWidget(m_adbStatusLbl);

    m_adbLog = new QTextEdit(rightW);
    m_adbLog->setReadOnly(true);
    m_adbLog->setObjectName("chatLog");
    m_adbLog->setMaximumHeight(110);
    m_adbLog->setPlaceholderText("Output adb...");
    m_adbLog->hide();
    rightLay->addWidget(m_adbLog);

    rightLay->addStretch();
    splitLay->addWidget(rightW, 1);
    rootLay->addWidget(split, 1);

    connect(m_qrApkBtn,      &QPushButton::clicked, this, &LanWanPage::onQrApkBtnClicked);
    connect(m_qrPageBtn,     &QPushButton::clicked, this, &LanWanPage::onQrPageBtnClicked);
    connect(m_lanToggleBtn,  &QPushButton::toggled, this, &LanWanPage::onLanToggleBtnToggled);
    connect(m_lanWebBtn,     &QPushButton::clicked, this, &LanWanPage::onLanWebBtnClicked);
    connect(m_adbInstallBtn, &QPushButton::clicked, this, &LanWanPage::onAdbInstallBtnClicked);

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
            "\xe2\x9d\x8c  adb non trovato. Installa: sudo apt install adb");
        return;
    }

    namespace P = PrismaluxPaths;
    const QString apk = P::root() + "/ANDROID/PrismaluxMobile.apk";
    if (!QFile::exists(apk)) {
        m_adbStatusLbl->setText(
            "\xe2\x9d\x8c  APK non trovato: " + apk);
        return;
    }

    m_adbLog->clear();
    m_adbLog->show();
    m_adbInstallBtn->setText("\xe2\x8f\xb9  Annulla");
    m_adbStatusLbl->setText(
        "\xe2\x8f\xb3  Installazione in corso... (attendi il telefono)");

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

    if (!m_adbProc->waitForStarted(3000)) {
        m_adbStatusLbl->setText(
            "\xe2\x9d\x8c  Impossibile avviare adb. Controlla la connessione USB.");
        m_adbLog->append("Errore: adb non si avvia.");
        m_adbInstallBtn->setText(
            "\xf0\x9f\x94\x8c  Installa APK via USB  (adb)");
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
            "\xf0\x9f\x94\x8c  Installa APK via USB  (adb)");

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
            "\xe2\x9d\x8c  Nessun dispositivo. Abilita USB Debugging e accetta il popup sul telefono.");
        m_adbStatusLbl->setStyleSheet("color:#f44336;font-size:11px;");
    } else {
        m_adbStatusLbl->setText(
            QString("\xe2\x9d\x8c  Installazione fallita (exit %1). Vedi log.").arg(code));
        m_adbStatusLbl->setStyleSheet("color:#f44336;font-size:11px;");
    }
}
