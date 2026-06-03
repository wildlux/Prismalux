#include "main_lan_wan.h"
#include "../dpi_utils.h"
#include "../lan_server.h"
#include "../prismalux_paths.h"
#include "../app_config.h"
#include "../widgets/qr_code_widget.h"
#include "../widgets/model_combo_box.h"
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
#include <QScrollArea>
#include <QLineEdit>
#include <QTextEdit>
#include <QComboBox>
#include <QProcess>
#include <QTcpSocket>
#include <QTcpServer>
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

namespace P = PrismaluxPaths;

/* ── Token LAN — delega a LanServer::saveLanToken/loadLanToken ──────────────
   Con HAVE_QKEYCHAIN usa il keyring di sistema; senza: file 0600.
   Queste funzioni locali fanno solo la migrazione da QSettings e poi
   delegano alla versione centralizzata in LanServer.               */
static QString loadLanToken()
{
    /* Migrazione da QSettings (versioni precedenti) */
    const QString old = AppConfig::s().value(P::SK::kLanToken, "").toString();
    if (!old.isEmpty()) {
        AppConfig::s().remove(P::SK::kLanToken);
        LanServer::saveLanToken(old);   /* salva nel keyring / file 0600 */
        return old;
    }
    return LanServer::loadLanToken();
}

static void saveLanToken(const QString& token)
{
    LanServer::saveLanToken(token);
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
    qrw->setFixedSize(dpiSize(280, 280));
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
    /* Blocca i segnali di tutti i QProcess figli PRIMA che Qt li distrugga.
     * QProcess::~QProcess() chiama waitForFinished() che può emettere finished()
     * → slot come onOllamaCheckDone accedono a widget già in fase di distruzione
     * → SIGSEGV. blockSignals+kill interrompe questo ciclo. */
    for (QProcess* p : findChildren<QProcess*>())
        if (p) { p->blockSignals(true); p->kill(); }
    /* Ferma LanServer esplicitamente PRIMA che Qt distrugga i figli. */
    if (m_lanServer) {
        m_lanServer->blockSignals(true);
        m_lanServer->stop();
    }
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

    /* onModelsReady rimosso: ModelComboBox gestisce il fetch autonomamente */

    tabs->addTab(buildWanComputeTab(), "\xf0\x9f\x96\xa7  WAN Compute");

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
/* onModelsReady rimosso: ModelComboBox gestisce il popolamento autonomamente */

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

void LanWanPage::onTokenAutoGenerated(const QString& token)
{
    if (m_lanTokenEdit && m_lanTokenEdit->text().trimmed().isEmpty()) {
        m_lanTokenEdit->blockSignals(true);
        m_lanTokenEdit->setText(token);
        m_lanTokenEdit->blockSignals(false);
    }
    saveLanToken(token);
    if (m_lanStatusLbl)
        m_lanStatusLbl->setToolTip(
            "\xf0\x9f\x94\x91 Token generato automaticamente \xe2\x80\x94 copialo nell'app Android");
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
    if (m_urlDisplayLbl)
        m_urlDisplayLbl->setText(QString("%1 : %2").arg(ip).arg(port));
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
            connect(m_lanServer, &LanServer::tokenAutoGenerated,
                    this, &LanWanPage::onTokenAutoGenerated);
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
        eyeBtn->setFixedWidth(dpiScale(28)); eyeBtn->setCheckable(true); eyeBtn->setFlat(true);
        connect(eyeBtn, &QPushButton::toggled, this, &LanWanPage::onEyeBtnToggled);
        auto* copyBtn = new QPushButton("\xf0\x9f\x93\x8b", tokenRow);
        copyBtn->setFixedWidth(dpiScale(28)); copyBtn->setFlat(true);
        connect(copyBtn, &QPushButton::clicked, this, &LanWanPage::onCopyTokenBtnClicked);
        auto* regenBtn = new QPushButton("\xf0\x9f\x94\x84", tokenRow);
        regenBtn->setFixedWidth(dpiScale(28)); regenBtn->setFlat(true);
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
    auto* rightLay = new QVBoxLayout(rightW);   /* QR+istruzioni in alto, URL+pulsanti sotto */
    rightLay->setContentsMargins(4, 0, 0, 0);
    rightLay->setSpacing(8);

    /* ── QR centrato + istruzioni sotto ── */
    m_qrInlineWidget = new QrCodeWidget(QString(), rightW);
    m_qrInlineWidget->setFixedSize(dpiSize(276, 276));
    m_qrInlineWidget->setToolTip(
        "QR di connessione rapida. Si aggiorna con IP, porta e token.");
    rightLay->addWidget(m_qrInlineWidget, 0, Qt::AlignHCenter | Qt::AlignTop);

    auto* qrInfoLbl = new QLabel(rightW);
    qrInfoLbl->setTextFormat(Qt::RichText);
    qrInfoLbl->setWordWrap(true);
    qrInfoLbl->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    qrInfoLbl->setText(
        "\n\n"
        "<span style='font-size:14px;'>"
        "<b>\xf0\x9f\x93\xb1  Connetti l\xe2\x80\x99" "app Android</b></span><br>"
        "<span style='font-size:13px;'>"
        "1. Avvia <b>PrismaluxMobile</b> sul telefono<br>"
        "2. Apri <b>Impostazioni</b> \xe2\x86\x92 "
        "<b>\xf0\x9f\x93\xb7 Scansiona QR dal PC</b><br>"
        "3. Punta la fotocamera su questo QR<br><br>"
        "<i>IP + Porta + Token vengono configurati in automatico.</i><br>"
        "Puoi scansionare anche con il server fermo "
        "per pre-configurare l\xe2\x80\x99" "app.</span>");
    rightLay->addWidget(qrInfoLbl);

    /* ── Sotto: URL + pulsanti ── */
    auto* scrollW   = new QWidget;
    auto* scrollLay = new QVBoxLayout(scrollW);
    scrollLay->setContentsMargins(0, 0, 0, 0);
    scrollLay->setSpacing(8);

    /* ── Riga URL stilata: 🌐 IP:porta  [📋 Copia] ── */
    auto* urlRow  = new QWidget(scrollW);
    auto* urlRowL = new QHBoxLayout(urlRow);
    urlRowL->setContentsMargins(0, 2, 0, 2);
    urlRowL->setSpacing(6);

    auto* urlIcon = new QLabel("\xf0\x9f\x8c\x90", urlRow);   /* 🌐 */
    urlIcon->setStyleSheet("font-size:14px;");

    m_urlDisplayLbl = new QLabel(urlRow);
    m_urlDisplayLbl->setTextFormat(Qt::RichText);
    m_urlDisplayLbl->setStyleSheet(
        "font-size:13px; font-weight:600; color: palette(highlight);");
    m_urlDisplayLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_urlDisplayLbl->setText(
        QString("%1 : %2").arg(ip).arg(m_lanPortSpin->value()));

    auto* urlCopyBtn = new QPushButton("\xf0\x9f\x93\x8b", urlRow);  /* 📋 */
    urlCopyBtn->setFixedSize(dpiSize(28, 24));
    urlCopyBtn->setToolTip("Copia URL negli appunti");
    urlCopyBtn->setObjectName("actionBtn");
    urlCopyBtn->setAccessibleName("Copia URL server LAN negli appunti");

    urlRowL->addWidget(urlIcon);
    urlRowL->addWidget(m_urlDisplayLbl, 1);
    urlRowL->addWidget(urlCopyBtn);
    scrollLay->addWidget(urlRow);

    connect(urlCopyBtn, &QPushButton::clicked, urlCopyBtn, [this, urlCopyBtn]{
        const QString url = QString("%1://%2")
            .arg(serverScheme()).arg(m_urlDisplayLbl->text().trimmed());
        QGuiApplication::clipboard()->setText(url);
        urlCopyBtn->setText("\xe2\x9c\x85");                  /* ✅ feedback */
        QTimer::singleShot(1500, urlCopyBtn, [urlCopyBtn]{
            urlCopyBtn->setText("\xf0\x9f\x93\x8b");
        });
    });

    /* Aggiorna QR a ogni modifica */
    connect(m_lanTokenEdit, &QLineEdit::textChanged,
            this, &LanWanPage::onUpdateQrInline);
    connect(m_lanPortSpin,  QOverload<int>::of(&QSpinBox::valueChanged),
            this, &LanWanPage::onUpdateQrInline);
    onUpdateQrInline();

    auto* sep = new QFrame(scrollW);
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Sunken);
    scrollLay->addWidget(sep);

    /* Pulsante QR Connetti (apre dialog QR più grande) */
    auto* qrConnectBtn = new QPushButton(
        "\xf0\x9f\x93\xb1  QR Connetti (schermo intero)", scrollW);
    qrConnectBtn->setObjectName("actionBtn");
    qrConnectBtn->setToolTip("Mostra il QR in un dialogo grande");
    connect(qrConnectBtn, &QPushButton::clicked,
            this, &LanWanPage::onQrConnectBtnClicked);
    scrollLay->addWidget(qrConnectBtn);

    /* QR APK + Pagina */
    auto* qrRow  = new QWidget(scrollW);
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
    scrollLay->addWidget(qrRow);

    m_lanWebBtn = new QPushButton(
        "\xf0\x9f\x8c\x90  Apri Chat Web nel browser", scrollW);
    m_lanWebBtn->setObjectName("actionBtn");
    m_lanWebBtn->setEnabled(false);
    scrollLay->addWidget(m_lanWebBtn);

    /* ── Installazione APK via USB (adb) ── */
    auto* adbSep = new QFrame(scrollW);
    adbSep->setFrameShape(QFrame::HLine);
    adbSep->setFrameShadow(QFrame::Sunken);
    scrollLay->addWidget(adbSep);

    const QString adbPath = findAdb();
    m_adbInstallBtn = new QPushButton(
        "\xf0\x9f\x94\x8c  Installa APK via USB  (adb)", scrollW);
    m_adbInstallBtn->setObjectName("primaryBtn");
    m_adbInstallBtn->setToolTip(
        adbPath.isEmpty()
            ? "adb non trovato — installa Android Platform Tools"
            : QString("adb: %1").arg(adbPath));
    m_adbInstallBtn->setEnabled(!adbPath.isEmpty());
    m_adbInstallBtn->setAccessibleName("Installa APK PrismaluxMobile sul telefono Android via USB");
    scrollLay->addWidget(m_adbInstallBtn);

    m_adbStatusLbl = new QLabel(
        adbPath.isEmpty()
            ? "\xe2\x9a\xa0\xef\xb8\x8f  adb non trovato. Installa: sudo apt install adb"
            : "\xe2\x84\xb9  Collega il telefono via USB con debug USB attivo, poi premi il pulsante.",
        scrollW);
    m_adbStatusLbl->setWordWrap(true);
    m_adbStatusLbl->setStyleSheet("color:#aaa;font-size:11px;");
    scrollLay->addWidget(m_adbStatusLbl);

    m_adbLog = new QTextEdit(scrollW);
    m_adbLog->setReadOnly(true);
    m_adbLog->setObjectName("chatLog");
    m_adbLog->setMaximumHeight(dpiScale(110));
    m_adbLog->setPlaceholderText("Output adb...");
    m_adbLog->hide();
    scrollLay->addWidget(m_adbLog);

    scrollLay->addStretch();

    auto* scrollArea = new QScrollArea(rightW);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setWidget(scrollW);
    rightLay->addWidget(scrollArea, 1);
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
    m_gns3HostEdit->setFixedWidth(dpiScale(150));
    auto* pingBtn = new QPushButton("\xf0\x9f\x94\x97  Verifica", connRow);
    pingBtn->setObjectName("actionBtn");
    pingBtn->setFixedWidth(dpiScale(100));
    m_gns3StatusLbl = new QLabel("\xe2\x9a\xaa  Non connesso", connRow);
    m_gns3StatusLbl->setObjectName("hintLabel");
    m_gns3ExecBtn = new QPushButton("\xf0\x9f\x8c\x90  Esegui su GNS3", connRow);
    m_gns3ExecBtn->setObjectName("actionBtn");
    m_gns3ExecBtn->setFixedWidth(dpiScale(160));
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
    m_gns3Model = new ModelComboBox(m_ai, toolRow);
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
    m_gns3Input->setFixedHeight(dpiScale(80));
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
    m_gns3Progress->setFixedHeight(dpiScale(4));
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
        wanSendJson(node.sock, QJsonObject{
            {"t", "task"}, {"id", best->id},
            {"kind", best->kind}, {"payload", best->payload}
        });
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

    /* ── Selezione modalità di esecuzione: Solo questo PC | Rete LAN ── */
    auto* execModeRow = new QWidget;
    auto* execModeLay = new QHBoxLayout(execModeRow);
    execModeLay->setContentsMargins(0,0,0,0); execModeLay->setSpacing(16);
    auto* localRb = new QRadioButton("\xf0\x9f\xa7\xa0  Solo questo PC");
    auto* lanRb   = new QRadioButton("\xf0\x9f\x8c\x90  Rete LAN (pi\xc3\xb9 PC insieme)");
    localRb->setChecked(true);
    auto* execGrp = new QButtonGroup(execModeRow);
    execGrp->addButton(localRb, 0);
    execGrp->addButton(lanRb,   1);
    execModeLay->addWidget(localRb);
    execModeLay->addWidget(lanRb);
    execModeLay->addStretch(1);
    vlay->addWidget(execModeRow);

    /* ── Stack esecuzione: 0=Multi-Agente locale, 1=Rete LAN ── */
    m_execModeStack = new QStackedWidget;

    /* index 0 — Multi-Agente locale (pool AiClient + GraphMemory SQLite) */
    m_multiAgentTab = new AgentiMultiPage(m_ai, root);
    m_execModeStack->addWidget(m_multiAgentTab);   /* index 0 */

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
    srvCtrlLay->addWidget(new QLabel("Porta:"));
    srvCtrlLay->addWidget(m_wanPortSpin);
    srvCtrlLay->addWidget(m_wanStartBtn);
    srvCtrlLay->addWidget(m_wanSimBtn);
    srvCtrlLay->addWidget(m_wanExposeAllCheck);
    srvCtrlLay->addWidget(m_wanSrvStatusLbl, 1);
    connect(m_wanSimBtn, &QPushButton::clicked, this, &LanWanPage::onWanSimBtnClicked);
    srvLay->addWidget(srvCtrlRow);

    /* Token auth server — i nodi devono presentarlo nell'hello */
    auto* srvTokenRow = new QWidget;
    auto* srvTokenLay = new QHBoxLayout(srvTokenRow);
    srvTokenLay->setContentsMargins(0,0,0,0); srvTokenLay->setSpacing(6);
    auto* srvTokenLbl = new QLabel("\xf0\x9f\x94\x91  Token server:", srvTokenRow);  /* 🔑 */
    m_wanTokenEdit = new QLineEdit(srvTokenRow);
    m_wanTokenEdit->setPlaceholderText("Lascia vuoto = accetta tutti i nodi (NON sicuro)");
    m_wanTokenEdit->setEchoMode(QLineEdit::Password);
    m_wanTokenEdit->setToolTip(
        "Token segreto condiviso tra server e nodi worker.\n"
        "Se impostato, ogni nodo deve presentarlo nel messaggio 'hello'.\n"
        "Consigliato: 16+ caratteri casuali.");
    srvTokenLay->addWidget(srvTokenLbl);
    srvTokenLay->addWidget(m_wanTokenEdit, 1);
    srvLay->addWidget(srvTokenRow);

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
        m_wanExportBtn = new QPushButton(
            "\xf0\x9f\x93\xa5  Esporta CSV", dashRow);
        m_wanExportBtn->setObjectName("actionBtn");
        m_wanExportBtn->setToolTip("Scarica CSV di tutti i task (id, tipo, payload, stato, nodo, durata, risultato)");
        dashLay->addWidget(m_wanThroughputLbl, 1);
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
    m_wanTaskPayload->setPlaceholderText("Seleziona un tipo per vedere il template\xe2\x80\xa6");
    m_wanPayloadStack->addWidget(m_wanTaskPayload);   // index 0

    m_agentFormFrame = new QFrame;
    m_agentFormFrame->setFrameShape(QFrame::StyledPanel);
    m_agentFormFrame->setAcceptDrops(true);
    m_agentFormFrame->installEventFilter(this);
    auto* formLay = new QVBoxLayout(m_agentFormFrame);
    formLay->setSpacing(4); formLay->setContentsMargins(6,4,6,4);

    m_agentRoleEdit = new QLineEdit;
    m_agentRoleEdit->setPlaceholderText("Ruolo: es. Ricercatore specializzato in fisica quantistica");
    formLay->addWidget(m_agentRoleEdit);

    m_agentPromptEdit = new QTextEdit;
    m_agentPromptEdit->setFixedHeight(dpiScale(52));
    m_agentPromptEdit->setPlaceholderText("Prompt: il compito specifico di questo agente\xe2\x80\xa6");
    formLay->addWidget(m_agentPromptEdit);

    m_agentContextEdit = new QTextEdit;
    m_agentContextEdit->setFixedHeight(dpiScale(34));
    m_agentContextEdit->setPlaceholderText("Contesto: da agenti precedenti (opzionale)");
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
    m_wanCronPayload->setPlaceholderText("Payload del task cron (ripetuto ad ogni tick)");
    m_wanCronPayload->setFixedHeight(dpiScale(48));
    m_wanCronLog = new QTextEdit;
    m_wanCronLog->setReadOnly(true);
    m_wanCronLog->setFixedHeight(dpiScale(56));
    m_wanCronLog->setPlaceholderText("Log cron\xe2\x80\xa6");
    cronLay->addWidget(cronRow1);
    cronLay->addWidget(m_wanCronPayload);
    cronLay->addWidget(m_wanCronLog);
    advLay->addWidget(cronBox);

    srvLay->addWidget(advPanel);
    m_wanModeStack->addWidget(srvPanel);   /* index 0 = server */

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
    m_wanCliHost->setPlaceholderText("IP server (es. 192.168.1.10)");
    m_wanCliPort = new QSpinBox;
    m_wanCliPort->setRange(1024, 65535); m_wanCliPort->setValue(P::kWanComputePort);
    m_wanCliPort->setFixedWidth(dpiScale(80));
    m_wanCliName = new QLineEdit;
    m_wanCliName->setPlaceholderText("Nome nodo (es. PC-Mario)");
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
    m_wanCliTokenEdit->setPlaceholderText("Token server (se impostato)");
    m_wanCliTokenEdit->setEchoMode(QLineEdit::Password);
    m_wanCliTokenEdit->setToolTip("Deve coincidere con il token impostato sul server.");
    m_wanCliShellCheck = new QCheckBox("\xe2\x9a\xa0\xef\xb8\x8f  Permetti shell (rischio RCE)", cliSecRow);
    m_wanCliShellCheck->setToolTip(
        "Se spuntato, questo nodo eseguirà comandi bash/python ricevuti dal server.\n"
        "Abilita SOLO su reti fidate con token auth impostato.");
    cliSecLay->addWidget(cliTokenLbl);
    cliSecLay->addWidget(m_wanCliTokenEdit, 1);
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
    vlay->addWidget(m_execModeStack, 1);

    /* ── Connessione radio esecuzione → stack ── */
    connect(execGrp, &QButtonGroup::idClicked, m_execModeStack, [this](int id){
        if (m_execModeStack) m_execModeStack->setCurrentIndex(id);
    });

    return root;
}

/* ══════════════════════════════════════════════════════════════
   WAN — Slot SERVER
   ══════════════════════════════════════════════════════════════ */
void LanWanPage::onWanStartBtnClicked()
{
    if (!m_wanServer) {
        m_wanServer = new QTcpServer(this);
        connect(m_wanServer, &QTcpServer::newConnection,
                this, &LanWanPage::onWanNewConnection);
    }

    if (m_wanStartBtn->isChecked()) {
        const int port = m_wanPortSpin->value();
        const bool exposeAll = m_wanExposeAllCheck && m_wanExposeAllCheck->isChecked();
        const QHostAddress bindAddr = exposeAll ? QHostAddress::Any : QHostAddress::LocalHost;
        if (!m_wanServer->listen(bindAddr, static_cast<quint16>(port))) {
            m_wanStartBtn->setChecked(false);
            m_wanSrvStatusLbl->setText(
                "\xe2\x9d\x8c  Errore: " + m_wanServer->errorString());
            m_wanSrvStatusLbl->setStyleSheet("color:#f44336;");
            return;
        }
        /* Avvia heartbeat 30s */
        if (!m_wanHeartbeatTimer) {
            m_wanHeartbeatTimer = new QTimer(this);
            connect(m_wanHeartbeatTimer, &QTimer::timeout,
                    this, &LanWanPage::onWanHeartbeatTick);
        }
        m_wanHeartbeatTimer->start(30000);
        onCheckOllamaExposed();   /* controlla sempre, non solo in exposeAll */
        if (exposeAll) {
            m_wanSrvStatusLbl->setText(
                "\xe2\x9c\x85  In ascolto su " + localLanIp() +
                ":" + QString::number(port) +
                "  \xe2\x9a\xa0\xef\xb8\x8f  Attenzione: il server e' visibile a tutta la rete");
        } else {
            m_wanSrvStatusLbl->setText(
                "\xe2\x9c\x85  In ascolto su 127.0.0.1:" + QString::number(port));
        }
        m_wanSrvStatusLbl->setStyleSheet("color:#4caf50;");
        m_wanPortSpin->setEnabled(false);
        m_wanExposeAllCheck->setEnabled(false);
    } else {
        /* Stop */
        if (m_wanHeartbeatTimer) m_wanHeartbeatTimer->stop();
        m_wanServer->close();
        for (auto& node : m_wanNodes)
            if (node.sock) node.sock->disconnectFromHost();
        m_wanNodes.clear();
        wanRefreshTables();
        updateWanStats();
        m_wanStartBtn->setText("\xe2\x96\xb6  Avvia Server");
        m_wanSrvStatusLbl->setText("\xe2\x9a\xab  Server fermo");
        m_wanSrvStatusLbl->setStyleSheet("color:gray;");
        m_wanPortSpin->setEnabled(true);
        if (m_wanExposeAllCheck) m_wanExposeAllCheck->setEnabled(true);
    }
}

void LanWanPage::onCheckOllamaExposed()
{
    QProcess* proc = new QProcess(this);
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc](int, QProcess::ExitStatus){
                onOllamaCheckDone(proc);
            });
    proc->start("sh", QStringList{"-c", "ss -tlnp 2>/dev/null | grep 11434"});
}

void LanWanPage::onOllamaCheckDone(QProcess* proc)
{
    const QString out = proc->readAllStandardOutput();
    proc->deleteLater();

    /* Check 1: Ollama in ascolto su 0.0.0.0 (da ss -tlnp) */
    bool exposed = out.contains("0.0.0.0");

    /* Check 2: variabile d'ambiente OLLAMA_HOST pericolosa */
    const QString ollamaHost = qEnvironmentVariable("OLLAMA_HOST");
    if (!ollamaHost.isEmpty() && !ollamaHost.startsWith("127.") && ollamaHost != "localhost")
        exposed = true;

    if (exposed && m_wanSrvStatusLbl) {
        m_wanSrvStatusLbl->setText(
            m_wanSrvStatusLbl->text() +
            "\n\xe2\x9a\xa0\xef\xb8\x8f  Ollama esposto su rete — "
            "chiunque in LAN puo' usarlo senza token. "
            "Imposta OLLAMA_HOST=127.0.0.1 prima di avviare il server.");
        m_wanSrvStatusLbl->setStyleSheet("color:#ff9800;");
    }
}

void LanWanPage::onWanNewConnection()
{
    while (m_wanServer && m_wanServer->hasPendingConnections()) {
        QTcpSocket* sock = m_wanServer->nextPendingConnection();
        connect(sock, &QTcpSocket::readyRead,
                this, &LanWanPage::onWanNodeReadyRead);
        connect(sock, &QTcpSocket::disconnected,
                this, &LanWanPage::onWanNodeDisconnected);
    }
}

void LanWanPage::onWanNodeReadyRead()
{
    auto* sock = qobject_cast<QTcpSocket*>(sender());
    if (!sock) return;

    while (sock->canReadLine()) {
        const QByteArray line = sock->readLine().trimmed();
        QJsonParseError jerr;
        const QJsonDocument doc = QJsonDocument::fromJson(line, &jerr);
        if (!doc.isObject()) continue;
        const QJsonObject msg = doc.object();
        const QString type    = msg["t"].toString();

        if (type == "hello") {
            /* Verifica token server — rifiuta nodi non autenticati */
            const QString serverToken = m_wanTokenEdit ? m_wanTokenEdit->text().trimmed() : QString();
            if (!serverToken.isEmpty()) {
                const QString presented = msg["token"].toString();
                if (presented != serverToken) {
                    wanSendJson(sock, QJsonObject{{"t","error"},{"msg","auth_failed"}});
                    sock->disconnectFromHost();
                    continue;
                }
            }

            /* Registrazione nuovo nodo */
            WanNode node;
            node.id     = wanNextId();
            node.name   = msg["name"].toString("nodo-" + node.id);
            node.ip     = sock->peerAddress().toString();
            node.status = "idle";
            node.sock   = sock;
            const QJsonArray caps = msg["caps"].toArray();
            for (const auto& c : caps) node.caps.append(c.toString());
            /* Default: solo "ai" — "shell" è opt-in esplicito lato worker */
            if (node.caps.isEmpty()) node.caps << "ai";
            m_wanNodes.push_back(node);
            wanSendJson(sock, QJsonObject{{"t","welcome"},{"node_id", node.id}});
            wanRefreshTables();
            wanDispatch();

        } else if (type == "pong") {
            /* Risposta a heartbeat ping — aggiorna lastSeen */
            auto nodeIt = std::find_if(m_wanNodes.begin(), m_wanNodes.end(),
                [sock](const WanNode& n){ return n.sock == sock; });
            if (nodeIt != m_wanNodes.end()) {
                nodeIt->lastSeen    = QDateTime::currentDateTime();
                nodeIt->missedPings = 0;
            }

        } else if (type == "poll") {
            /* Client chiede un task — usa il priority scheduler */
            auto nodeIt = std::find_if(m_wanNodes.begin(), m_wanNodes.end(),
                [sock](const WanNode& n){ return n.sock == sock; });
            if (nodeIt == m_wanNodes.end()) { wanSendJson(sock, {{"t","idle"}}); continue; }
            nodeIt->status   = "idle";
            nodeIt->lastSeen = QDateTime::currentDateTime();
            wanDispatch();
            /* Se il nodo è rimasto idle (nessun task), notificalo */
            if (nodeIt->status == "idle")
                wanSendJson(sock, {{"t","idle"}});

        } else if (type == "result") {
            /* Risultato da un nodo */
            const QString id     = msg["id"].toString();
            const QString status = msg["status"].toString("done");
            const QString result = msg["result"].toString();
            auto taskIt = std::find_if(m_wanTasks.begin(), m_wanTasks.end(),
                [&id](const WanTask& t){ return t.id == id; });
            if (taskIt != m_wanTasks.end()) {
                taskIt->status = status;
                taskIt->result = result;
                if (status == "done") m_wanCompletedTs.append(QDateTime::currentDateTime());
            }
            auto nodeIt = std::find_if(m_wanNodes.begin(), m_wanNodes.end(),
                [sock](const WanNode& n){ return n.sock == sock; });
            if (nodeIt != m_wanNodes.end()) {
                nodeIt->status   = "idle";
                nodeIt->lastSeen = QDateTime::currentDateTime();
            }
            wanSendJson(sock, QJsonObject{{"t","ack"},{"id",id}});
            wanRefreshTables();
            updateWanStats();
            wanDispatch();

        } else if (type == "spawn_tasks") {
            /* Un agente llm_agent chiede di spawnare sub-agenti.
             * Crea un WanTask llm_agent per ogni entry nell'array "tasks". */
            const QString parentId = msg["parent_id"].toString();
            const QString chainId  = msg["chain_id"].toString();
            const QJsonArray tasks = msg["tasks"].toArray();
            int spawned = 0;
            for (const auto& v : tasks) {
                if (!v.isObject()) continue;
                const QJsonObject t = v.toObject();
                WanTask sub;
                sub.id      = wanNextId();
                sub.kind    = t["kind"].toString("llm_agent");
                sub.payload = t["payload"].toString();
                sub.status  = "pending";
                sub.created = QDateTime::currentDateTime();
                // Annotazione nella tabella: mostra relazione parent → child
                sub.node    = QString("spawned-by:%1").arg(parentId);
                m_wanTasks.push_back(sub);
                spawned++;
            }
            wanSendJson(sock, QJsonObject{{"t","ack"},{"id",parentId}});
            wanRefreshTables();
            wanDispatch();
            // Log nel cron se attivo
            if (spawned > 0)
                wanLogCron(QString("[chain:%1] %2 sub-agente/i spawnati da %3")
                           .arg(chainId).arg(spawned).arg(parentId));
        }
    }
}

void LanWanPage::wanReassignNodeTasks(const QString& nodeName)
{
    for (auto& task : m_wanTasks) {
        if (task.status == "running" && task.node == nodeName) {
            task.retryCount++;
            if (task.retryCount >= 3) {
                task.status = "failed";
                wanLogCron(QString("[fault] task %1 fallito dopo 3 tentativi").arg(task.id));
            } else {
                task.status = "pending";
                task.node.clear();
                wanLogCron(QString("[fault] task %1 riassegnato (tentativo %2/3)")
                           .arg(task.id).arg(task.retryCount));
            }
        }
    }
}

void LanWanPage::onWanNodeDisconnected()
{
    auto* sock = qobject_cast<QTcpSocket*>(sender());
    auto nodeIt = std::find_if(m_wanNodes.begin(), m_wanNodes.end(),
        [sock](const WanNode& n){ return n.sock == sock; });
    if (nodeIt != m_wanNodes.end()) {
        wanReassignNodeTasks(nodeIt->name);
        wanLogCron(QString("[nodo] %1 disconnesso").arg(nodeIt->name));
    }
    m_wanNodes.erase(
        std::remove_if(m_wanNodes.begin(), m_wanNodes.end(),
            [sock](const WanNode& n){ return n.sock == sock; }),
        m_wanNodes.end());
    wanRefreshTables();
    updateWanStats();
    wanDispatch();
}

void LanWanPage::onWanHeartbeatTick()
{
    const QDateTime now = QDateTime::currentDateTime();
    for (auto& node : m_wanNodes) {
        if (!node.sock || node.sock->state() != QAbstractSocket::ConnectedState) continue;
        wanSendJson(node.sock, {{"t", "ping"}});
        /* Se lastSeen non è mai stato settato, inizializzalo ora */
        if (!node.lastSeen.isValid()) { node.lastSeen = now; continue; }
        /* Nodo silenzioso per >90s (3 cicli da 30s) → considerato morto */
        if (node.lastSeen.secsTo(now) > 90) {
            node.missedPings++;
            if (node.missedPings >= 3) {
                wanLogCron(QString("[heartbeat] nodo %1 non risponde — rimosso").arg(node.name));
                node.sock->disconnectFromHost(); /* onWanNodeDisconnected gestisce riassegnazione */
            }
        }
    }
}

void LanWanPage::onWanDashTick()
{
    updateWanThroughput();
}

void LanWanPage::updateWanThroughput()
{
    if (!m_wanThroughputLbl) return;
    const QDateTime cutoff = QDateTime::currentDateTime().addSecs(-3600);
    /* Rimuovi timestamp vecchi */
    m_wanCompletedTs.erase(
        std::remove_if(m_wanCompletedTs.begin(), m_wanCompletedTs.end(),
            [&cutoff](const QDateTime& dt){ return dt < cutoff; }),
        m_wanCompletedTs.end());
    const int perHour = m_wanCompletedTs.size();

    /* ETA: stima durata media dei task completati con startedAt valido */
    double avgSec = 0.0;
    int counted = 0;
    for (const auto& t : m_wanTasks) {
        if ((t.status == "done" || t.status == "error") && t.startedAt.isValid()) {
            avgSec += t.startedAt.msecsTo(QDateTime::currentDateTime()) / 1000.0;
            counted++;
        }
    }
    int pending = 0;
    for (const auto& t : m_wanTasks) if (t.status == "pending") pending++;
    int working = 0;
    for (const auto& n : m_wanNodes) if (n.status == "working") working++;
    const int effective = qMax(1, working);

    QString etaTxt;
    if (counted > 0 && pending > 0) {
        avgSec /= counted;
        const double etaSec = (avgSec * pending) / effective;
        if (etaSec < 60)       etaTxt = QString("  ETA ~%1s").arg(qRound(etaSec));
        else if (etaSec < 3600) etaTxt = QString("  ETA ~%1min").arg(qRound(etaSec/60));
        else                   etaTxt = QString("  ETA ~%1h").arg(etaSec/3600, 0, 'f', 1);
    }

    m_wanThroughputLbl->setText(
        QString("\xf0\x9f\x93\x88  Throughput: <b>%1</b> task/ora%2")
        .arg(perHour).arg(etaTxt));
}

void LanWanPage::onWanExportCsvClicked()
{
    exportWanCsv();
}

void LanWanPage::exportWanCsv()
{
    const QString path = QFileDialog::getSaveFileName(
        this, "Esporta task WAN", QDir::homePath() + "/wan_tasks.csv",
        "CSV (*.csv)");
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream out(&f);
    out << "id,kind,stato,nodo,retry,priorita,created,startedAt,payload,result\n";
    for (const auto& t : m_wanTasks) {
        auto esc = [](const QString& s) -> QString {
            return "\"" + QString(s).replace("\"", "\"\"").replace("\n", " ") + "\"";
        };
        out << esc(t.id) << "," << esc(t.kind) << "," << esc(t.status)
            << "," << esc(t.node) << "," << t.retryCount << "," << t.priority
            << "," << esc(t.created.toString(Qt::ISODate))
            << "," << esc(t.startedAt.isValid() ? t.startedAt.toString(Qt::ISODate) : "")
            << "," << esc(t.payload.left(200))
            << "," << esc(t.result.left(500)) << "\n";
    }
}

void LanWanPage::updateWanStats()
{
    if (!m_wanStatsLbl) return;
    int idle = 0, working = 0;
    for (const auto& n : m_wanNodes) {
        if (n.status == "idle")    idle++;
        else if (n.status == "working") working++;
    }
    int pending = 0, running = 0, done = 0, failed = 0;
    for (const auto& t : m_wanTasks) {
        if      (t.status == "pending") pending++;
        else if (t.status == "running") running++;
        else if (t.status == "done")    done++;
        else if (t.status == "failed" || t.status == "error") failed++;
    }
    m_wanStatsLbl->setText(
        QString("\xf0\x9f\x96\xa5  Nodi: <b>%1</b> idle / <b>%2</b> working"
                "  \xe2\x80\x94  "
                "\xf0\x9f\x93\x8b  Task: <b>%3</b> in coda / <b>%4</b> running"
                " / <b>%5</b> completati / <b>%6</b> falliti")
        .arg(idle).arg(working).arg(pending).arg(running).arg(done).arg(failed));
}

void LanWanPage::onWanAddTaskBtnClicked()
{
    const QString kind = m_wanTaskKind ? m_wanTaskKind->currentData().toString() : "ai_query";
    QString payload;

    if (kind == "llm_agent" && m_wanPayloadStack && m_wanPayloadStack->currentIndex() == 1) {
        /* Costruisce JSON dai campi del form */
        const QString role   = m_agentRoleEdit    ? m_agentRoleEdit->text().trimmed()           : QString();
        const QString prompt = m_agentPromptEdit  ? m_agentPromptEdit->toPlainText().trimmed()   : QString();
        const QString ctx    = m_agentContextEdit ? m_agentContextEdit->toPlainText().trimmed()  : QString();
        if (role.isEmpty() || prompt.isEmpty()) return;
        QJsonObject obj;
        obj["role"]     = role;
        obj["prompt"]   = prompt;
        obj["context"]  = ctx;
        obj["depth"]    = 0;
        obj["chain_id"] = "";
        payload = QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    } else {
        payload = m_wanTaskPayload ? m_wanTaskPayload->toPlainText().trimmed() : QString();
    }

    if (payload.isEmpty()) return;
    WanTask t;
    t.id      = wanNextId();
    t.kind    = kind;
    t.payload = payload;
    t.status  = "pending";
    t.created = QDateTime::currentDateTime();
    m_wanTasks.push_back(t);
    wanRefreshTables();
    wanDispatch();
}

/* ── Cron ── */
void LanWanPage::onWanCronStartBtnClicked()
{
    const QString payload = m_wanCronPayload ? m_wanCronPayload->toPlainText().trimmed() : QString();
    if (payload.isEmpty()) {
        wanLogCron("Imposta prima il payload del task cron.");
        return;
    }
    if (!m_wanCronTimer) {
        m_wanCronTimer = new QTimer(this);
        connect(m_wanCronTimer, &QTimer::timeout,
                this, &LanWanPage::onWanCronFired);
    }
    const int ms = (m_wanCronInterval ? m_wanCronInterval->value() : 5) * 60 * 1000;
    m_wanCronTimer->start(ms);
    m_wanCronStartBtn->setEnabled(false);
    m_wanCronStopBtn->setEnabled(true);
    wanLogCron(QString("Cron avviato — ogni %1 min.")
               .arg(m_wanCronInterval ? m_wanCronInterval->value() : 5));
}

void LanWanPage::onWanCronStopBtnClicked()
{
    if (m_wanCronTimer) m_wanCronTimer->stop();
    m_wanCronStartBtn->setEnabled(true);
    m_wanCronStopBtn->setEnabled(false);
    wanLogCron("Cron fermato.");
}

void LanWanPage::onWanCronFired()
{
    const QString payload = m_wanCronPayload ? m_wanCronPayload->toPlainText().trimmed() : QString();
    if (payload.isEmpty()) return;
    WanTask t;
    t.id      = wanNextId();
    t.kind    = m_wanCronKind ? m_wanCronKind->currentData().toString() : "ai_query";
    t.payload = payload;
    t.status  = "pending";
    t.created = QDateTime::currentDateTime();
    m_wanTasks.push_back(t);
    wanRefreshTables();
    wanDispatch();
    wanLogCron("Task " + t.id + " aggiunto automaticamente (" + t.kind + ").");
}

/* ══════════════════════════════════════════════════════════════
   WAN — Slot CLIENT
   ══════════════════════════════════════════════════════════════ */
void LanWanPage::onWanCliConBtnClicked()
{
    const QString host  = m_wanCliHost ? m_wanCliHost->text().trimmed() : QString();
    const int     port  = m_wanCliPort ? m_wanCliPort->value() : P::kWanComputePort;
    const int     nWork = m_wanCliWorkerSpin ? m_wanCliWorkerSpin->value() : 1;
    if (host.isEmpty()) {
        wanCliAppendLog("Inserisci l'IP del server WAN.");
        return;
    }

    /* Disconnetti eventuali worker esistenti prima di ricrearli */
    if (!m_wanWorkers.isEmpty()) {
        onWanCliDisconBtnClicked();
        return;
    }

    const QString baseName = (m_wanCliName && !m_wanCliName->text().trimmed().isEmpty())
                             ? m_wanCliName->text().trimmed()
                             : QSysInfo::machineHostName();
    const QString token    = m_wanCliTokenEdit ? m_wanCliTokenEdit->text().trimmed() : QString();
    const bool    shell    = m_wanCliShellCheck && m_wanCliShellCheck->isChecked();

    m_wanCliStatusLbl->setText("\xe2\x8f\xb3  Connessione\xe2\x80\xa6");
    m_wanCliStatusLbl->setStyleSheet("color:#E5C400;");
    m_wanCliConBtn->setEnabled(false);

    m_wanWorkers.resize(nWork);

    for (int i = 0; i < nWork; ++i) {
        WanWorker& w = m_wanWorkers[i];

        /* Socket */
        auto* sock = new QTcpSocket(this);
        w.sock = sock;
        connect(sock, &QTcpSocket::readyRead,
                this, &LanWanPage::onWanWorkerReadyRead);
        connect(sock, &QTcpSocket::disconnected,
                this, &LanWanPage::onWanWorkerDisconnected);
        connect(sock,
                QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
                this, &LanWanPage::onWanWorkerError);

        /* AiClient dedicato — copia configurazione backend dal m_ai principale */
        auto* ai = new AiClient(this);
        if (m_ai) {
            ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), m_ai->model());
        }
        w.ai = ai;

        /* Poll timer */
        auto* timer = new QTimer(this);
        w.pollTimer = timer;
        connect(timer, &QTimer::timeout, this, &LanWanPage::onWanWorkerPoll);

        /* Salva metadati nel worker: usati da onWanWorkerConnected */
        w.name         = (nWork == 1)
                         ? baseName
                         : baseName + "-worker" + QString::number(i + 1);
        w.token        = token;
        w.shellAllowed = shell;

        connect(sock, &QTcpSocket::connected,
                this, &LanWanPage::onWanWorkerConnected);

        sock->connectToHost(host, static_cast<quint16>(port));
    }

    /* Alias legacy → worker[0] per retrocompatibilità (es. onWanSimBtnClicked) */
    m_wanCliSock      = m_wanWorkers[0].sock.data();
    m_wanCliPollTimer = m_wanWorkers[0].pollTimer.data();
}

void LanWanPage::onWanCliDisconBtnClicked()
{
    /* Ferma e distrugge tutti i worker */
    for (WanWorker& w : m_wanWorkers) {
        if (w.pollTimer) { w.pollTimer->stop(); w.pollTimer->deleteLater(); }
        if (w.ai)        { w.ai->abort();       w.ai->deleteLater(); }
        if (w.sock)      { w.sock->disconnectFromHost(); w.sock->deleteLater(); }
        QObject::disconnect(w.tokConn);
        QObject::disconnect(w.finConn);
        QObject::disconnect(w.errConn);
    }
    m_wanWorkers.clear();

    /* Reset alias legacy */
    m_wanCliSock        = nullptr;
    m_wanCliPollTimer   = nullptr;
    m_wanCliCurrentTask.clear();
    m_wanCliAiActive  = false;

    m_wanCliConBtn->setEnabled(true);
    m_wanCliDisconBtn->setEnabled(false);
    m_wanCliStatusLbl->setText("\xe2\x9a\xab  Non connesso");
    m_wanCliStatusLbl->setStyleSheet("color:gray;");
}

void LanWanPage::onWanCliPoll()
{
    /* Legacy slot — usato dalla simulazione locale (worker 0) */
    if (m_wanCliAiActive) return;
    if (!m_wanCliCurrentTask.isEmpty()) return;
    wanCliSendJson({{"t","poll"}});
}

void LanWanPage::onWanCliSockReadyRead()
{
    if (!m_wanCliSock) return;
    while (m_wanCliSock->canReadLine()) {
        const QByteArray line = m_wanCliSock->readLine().trimmed();
        QJsonParseError jerr;
        const QJsonDocument doc = QJsonDocument::fromJson(line, &jerr);
        if (!doc.isObject()) continue;
        const QJsonObject msg = doc.object();
        const QString type    = msg["t"].toString();

        if (type == "welcome") {
            m_wanCliNodeId = msg["node_id"].toString();
            wanCliAppendLog("Registrato come nodo " + m_wanCliNodeId);

        } else if (type == "idle") {
            /* nessun task — polling automatico via timer */

        } else if (type == "task") {
            const QString id      = msg["id"].toString();
            const QString kind    = msg["kind"].toString();
            const QString payload = msg["payload"].toString();
            m_wanCliCurrentTask   = id;
            wanCliAppendLog(QString("Task ricevuto [%1] tipo: %2").arg(id, kind));
            wanCliHandleTask(id, kind, payload);

        } else if (type == "ack") {
            wanCliAppendLog("Server ha ricevuto risultato " + msg["id"].toString());
        }
    }
}

void LanWanPage::onWanCliSockDisconnected()
{
    if (m_wanCliPollTimer) m_wanCliPollTimer->stop();
    m_wanCliConBtn->setEnabled(true);
    m_wanCliDisconBtn->setEnabled(false);
    m_wanCliStatusLbl->setText("\xe2\x9a\xab  Disconnesso");
    m_wanCliStatusLbl->setStyleSheet("color:gray;");
    wanCliAppendLog("Disconnesso dal server.");
    m_wanCliCurrentTask.clear();
    m_wanCliAiActive = false;
}

void LanWanPage::onWanCliSockError(QAbstractSocket::SocketError)
{
    const QString msg = m_wanCliSock ? m_wanCliSock->errorString() : "errore sconosciuto";
    m_wanCliStatusLbl->setText("\xe2\x9d\x8c  " + msg);
    m_wanCliStatusLbl->setStyleSheet("color:#f44336;");
    m_wanCliConBtn->setEnabled(true);
    m_wanCliDisconBtn->setEnabled(false);
    wanCliAppendLog("Errore socket: " + msg);
}

/* ── AI execution sul client ── */
void LanWanPage::onWanCliAiToken(const QString& t)
{
    m_wanCliAiBuf.append(t);
}

void LanWanPage::onWanCliAiFinished(const QString&)
{
    QObject::disconnect(m_wanCliTokenConn);
    QObject::disconnect(m_wanCliFinishedConn);
    QObject::disconnect(m_wanCliErrorConn);
    m_wanCliTokenConn = m_wanCliFinishedConn = m_wanCliErrorConn = {};
    m_wanCliAiActive = false;

    const QString raw = m_wanCliAiBuf.trimmed();

    /* ── llm_agent: prova a parsare JSON con "result" + "spawn" opzionale ── */
    if (m_wanCliIsAgentTask) {
        m_wanCliIsAgentTask = false;
        const int     depth   = m_wanCliAgentDepth;
        const QString chainId = m_wanCliAgentChain;
        m_wanCliAgentDepth = 0;
        m_wanCliAgentChain.clear();

        // Estrai il blocco JSON dalla risposta (l'LLM può aggiungere testo prima/dopo)
        QString jsonCandidate;
        const int braceOpen  = raw.indexOf('{');
        const int braceClose = raw.lastIndexOf('}');
        if (braceOpen != -1 && braceClose > braceOpen)
            jsonCandidate = raw.mid(braceOpen, braceClose - braceOpen + 1);

        QJsonParseError jerr;
        const QJsonDocument doc = QJsonDocument::fromJson(jsonCandidate.toUtf8(), &jerr);
        const bool hasSpawn = doc.isObject()
                           && doc.object().contains("result")
                           && doc.object()["spawn"].isArray()
                           && !doc.object()["spawn"].toArray().isEmpty();

        const QString result = hasSpawn
            ? doc.object()["result"].toString(raw)
            : raw;

        // Invia risultato al master
        wanCliSendJson(QJsonObject{
            {"t","result"}, {"id", m_wanCliCurrentTask},
            {"status","done"}, {"result", result}
        });

        // Se ci sono sub-agenti da spawnare, informa il master
        if (hasSpawn) {
            const QJsonArray spawnArr = doc.object()["spawn"].toArray();
            QJsonArray tasks;
            for (const auto& v : spawnArr) {
                if (!v.isObject()) continue;
                const QJsonObject s = v.toObject();
                QJsonObject taskPayload{
                    {"role",     s["role"].toString("Assistente")},
                    {"prompt",   s["prompt"].toString()},
                    {"context",  result},
                    {"depth",    depth + 1},
                    {"chain_id", chainId}
                };
                tasks.append(QJsonObject{
                    {"kind",    "llm_agent"},
                    {"payload", QString::fromUtf8(
                        QJsonDocument(taskPayload).toJson(QJsonDocument::Compact))}
                });
            }
            wanCliSendJson(QJsonObject{
                {"t",          "spawn_tasks"},
                {"parent_id",  m_wanCliCurrentTask},
                {"chain_id",   chainId},
                {"tasks",      tasks}
            });
            wanCliAppendLog(QString("Agente ha richiesto %1 sub-agente/i.").arg(tasks.size()));
        }

        wanCliAppendLog("Task " + m_wanCliCurrentTask + " completato (agente).");
        m_wanCliCurrentTask.clear();
        m_wanCliAiBuf.clear();
        return;
    }

    wanCliSendJson(QJsonObject{
        {"t","result"},
        {"id", m_wanCliCurrentTask},
        {"status","done"},
        {"result", raw}
    });
    wanCliAppendLog("Task " + m_wanCliCurrentTask + " completato (AI).");
    m_wanCliCurrentTask.clear();
    m_wanCliAiBuf.clear();
}

void LanWanPage::onWanCliAiError(const QString& msg)
{
    QObject::disconnect(m_wanCliTokenConn);
    QObject::disconnect(m_wanCliFinishedConn);
    QObject::disconnect(m_wanCliErrorConn);
    m_wanCliTokenConn = m_wanCliFinishedConn = m_wanCliErrorConn = {};
    m_wanCliAiActive     = false;
    m_wanCliIsAgentTask  = false;
    m_wanCliAgentDepth   = 0;
    m_wanCliAgentChain.clear();

    wanCliSendJson(QJsonObject{
        {"t","result"},
        {"id", m_wanCliCurrentTask},
        {"status","error"},
        {"result", msg}
    });
    wanCliAppendLog("Task " + m_wanCliCurrentTask + " errore AI: " + msg);
    m_wanCliCurrentTask.clear();
}

/* ══════════════════════════════════════════════════════════════════════════
   Multi-worker helper — trova l'indice del worker dato sender()
   ══════════════════════════════════════════════════════════════════════════ */
int LanWanPage::wanWorkerIndex(QObject* obj) const
{
    for (int i = 0; i < m_wanWorkers.size(); ++i) {
        const WanWorker& w = m_wanWorkers[i];
        if (w.sock    && static_cast<QObject*>(w.sock.data())    == obj) return i;
        if (w.pollTimer && static_cast<QObject*>(w.pollTimer.data()) == obj) return i;
        if (w.ai      && static_cast<QObject*>(w.ai.data())      == obj) return i;
    }
    return -1;
}

void LanWanPage::wanWorkerSendJson(int idx, const QJsonObject& obj)
{
    if (idx < 0 || idx >= m_wanWorkers.size()) return;
    QTcpSocket* s = m_wanWorkers[idx].sock;
    if (s) s->write(QJsonDocument(obj).toJson(QJsonDocument::Compact) + "\n");
}

void LanWanPage::wanWorkerAppendLog(int idx, const QString& msg)
{
    const QString prefix = (m_wanWorkers.size() > 1)
        ? QString("[W%1]").arg(idx + 1) : QString();
    wanCliAppendLog(prefix + msg);
}

/* ── Per-worker connected — invia hello e avvia polling ── */
void LanWanPage::onWanWorkerConnected()
{
    const int idx = wanWorkerIndex(sender());
    if (idx < 0 || idx >= m_wanWorkers.size()) return;
    WanWorker& w = m_wanWorkers[idx];
    if (!w.sock) return;

    /* Invia hello con nome, token e caps */
    QJsonArray caps{"ai"};
    if (w.shellAllowed) caps.append("shell");
    wanWorkerSendJson(idx, QJsonObject{
        {"t",     "hello"},
        {"name",  w.name},
        {"token", w.token},
        {"caps",  caps}
    });

    /* Solo worker 0 aggiorna lo stato UI generale */
    if (idx == 0) {
        m_wanCliStatusLbl->setText("\xe2\x9c\x85  Connesso — in attesa task");
        m_wanCliStatusLbl->setStyleSheet("color:#4caf50;");
        m_wanCliConBtn->setEnabled(false);
        m_wanCliDisconBtn->setEnabled(true);
        wanCliAppendLog(QString("Pool %1 worker connesso a %2:%3")
            .arg(m_wanWorkers.size())
            .arg(w.sock->peerAddress().toString())
            .arg(w.sock->peerPort()));
    } else {
        wanWorkerAppendLog(idx, QString("Worker %1 connesso.").arg(idx + 1));
    }

    /* Avvia il poll timer di questo worker */
    if (w.pollTimer) w.pollTimer->start(5000);
}

/* ── Per-worker poll ── */
void LanWanPage::onWanWorkerPoll()
{
    const int idx = wanWorkerIndex(sender());
    if (idx < 0 || idx >= m_wanWorkers.size()) return;
    const WanWorker& w = m_wanWorkers[idx];
    if (w.aiActive)            return;  // occupato
    if (!w.currentTask.isEmpty()) return;
    wanWorkerSendJson(idx, {{"t","poll"}});
}

/* ── Per-worker readyRead ── */
void LanWanPage::onWanWorkerReadyRead()
{
    const int idx = wanWorkerIndex(sender());
    if (idx < 0 || idx >= m_wanWorkers.size()) return;
    WanWorker& w = m_wanWorkers[idx];
    if (!w.sock) return;

    while (w.sock->canReadLine()) {
        const QByteArray line = w.sock->readLine().trimmed();
        QJsonParseError jerr;
        const QJsonDocument doc = QJsonDocument::fromJson(line, &jerr);
        if (!doc.isObject()) continue;
        const QJsonObject msg  = doc.object();
        const QString     type = msg["t"].toString();

        if (type == "welcome") {
            w.nodeId = msg["node_id"].toString();
            wanWorkerAppendLog(idx, "Registrato come nodo " + w.nodeId);

        } else if (type == "idle") {
            /* nessun task — polling automatico via timer */

        } else if (type == "task") {
            const QString id      = msg["id"].toString();
            const QString kind    = msg["kind"].toString();
            const QString payload = msg["payload"].toString();
            w.currentTask = id;
            wanWorkerAppendLog(idx,
                QString("Task ricevuto [%1] tipo: %2").arg(id, kind));
            wanWorkerHandleTask(idx, id, kind, payload);

        } else if (type == "ack") {
            wanWorkerAppendLog(idx,
                "Server ha ricevuto risultato " + msg["id"].toString());
        }
    }

    /* Aggiorna alias legacy dal worker 0 */
    if (idx == 0) {
        m_wanCliCurrentTask = m_wanWorkers[0].currentTask;
        m_wanCliAiActive    = m_wanWorkers[0].aiActive;
        m_wanCliNodeId      = m_wanWorkers[0].nodeId;
    }
}

/* ── Per-worker disconnected ── */
void LanWanPage::onWanWorkerDisconnected()
{
    const int idx = wanWorkerIndex(sender());
    if (idx < 0 || idx >= m_wanWorkers.size()) return;
    WanWorker& w = m_wanWorkers[idx];

    if (w.pollTimer) w.pollTimer->stop();
    w.currentTask.clear();
    w.aiActive = false;
    wanWorkerAppendLog(idx, "Disconnesso dal server.");

    /* Se tutti i worker sono disconnessi, aggiorna la UI */
    bool anyConnected = false;
    for (const WanWorker& ww : std::as_const(m_wanWorkers)) {
        if (ww.sock && ww.sock->state() == QAbstractSocket::ConnectedState) {
            anyConnected = true;
            break;
        }
    }
    if (!anyConnected) {
        m_wanCliConBtn->setEnabled(true);
        m_wanCliDisconBtn->setEnabled(false);
        m_wanCliStatusLbl->setText("\xe2\x9a\xab  Disconnesso");
        m_wanCliStatusLbl->setStyleSheet("color:gray;");
    }
}

/* ── Per-worker socket error ── */
void LanWanPage::onWanWorkerError(QAbstractSocket::SocketError)
{
    const int idx = wanWorkerIndex(sender());
    const QString errMsg = (idx >= 0 && idx < m_wanWorkers.size() && m_wanWorkers[idx].sock)
        ? m_wanWorkers[idx].sock->errorString()
        : "errore sconosciuto";

    if (idx == 0) {
        /* Solo il worker 0 aggiorna la UI di stato */
        m_wanCliStatusLbl->setText("\xe2\x9d\x8c  " + errMsg);
        m_wanCliStatusLbl->setStyleSheet("color:#f44336;");
        m_wanCliConBtn->setEnabled(true);
        m_wanCliDisconBtn->setEnabled(false);
    }
    wanWorkerAppendLog(idx, "Errore socket: " + errMsg);
}

/* ── Per-worker AI token ── */
void LanWanPage::onWanWorkerAiToken(const QString& t)
{
    const int idx = wanWorkerIndex(sender());
    if (idx < 0 || idx >= m_wanWorkers.size()) return;
    m_wanWorkers[idx].aiBuf.append(t);
}

/* ── Per-worker AI finished ── */
void LanWanPage::onWanWorkerAiFinished(const QString&)
{
    const int idx = wanWorkerIndex(sender());
    if (idx < 0 || idx >= m_wanWorkers.size()) return;
    WanWorker& w = m_wanWorkers[idx];

    QObject::disconnect(w.tokConn);
    QObject::disconnect(w.finConn);
    QObject::disconnect(w.errConn);
    w.tokConn = w.finConn = w.errConn = {};
    w.aiActive = false;

    const QString raw = w.aiBuf.trimmed();

    /* ── llm_agent: prova a parsare JSON con "result" + "spawn" opzionale ── */
    if (w.isAgentTask) {
        w.isAgentTask = false;
        const int     depth   = w.agentDepth;
        const QString chainId = w.agentChain;
        w.agentDepth = 0;
        w.agentChain.clear();

        QString jsonCandidate;
        const int braceOpen  = raw.indexOf('{');
        const int braceClose = raw.lastIndexOf('}');
        if (braceOpen != -1 && braceClose > braceOpen)
            jsonCandidate = raw.mid(braceOpen, braceClose - braceOpen + 1);

        QJsonParseError jerr;
        const QJsonDocument doc = QJsonDocument::fromJson(jsonCandidate.toUtf8(), &jerr);
        const bool hasSpawn = doc.isObject()
                           && doc.object().contains("result")
                           && doc.object()["spawn"].isArray()
                           && !doc.object()["spawn"].toArray().isEmpty();

        const QString result = hasSpawn
            ? doc.object()["result"].toString(raw)
            : raw;

        wanWorkerSendJson(idx, QJsonObject{
            {"t","result"}, {"id", w.currentTask},
            {"status","done"}, {"result", result}
        });

        if (hasSpawn) {
            const QJsonArray spawnArr = doc.object()["spawn"].toArray();
            QJsonArray tasks;
            for (const auto& v : spawnArr) {
                if (!v.isObject()) continue;
                const QJsonObject s = v.toObject();
                QJsonObject taskPayload{
                    {"role",     s["role"].toString("Assistente")},
                    {"prompt",   s["prompt"].toString()},
                    {"context",  result},
                    {"depth",    depth + 1},
                    {"chain_id", chainId}
                };
                tasks.append(QJsonObject{
                    {"kind",    "llm_agent"},
                    {"payload", QString::fromUtf8(
                        QJsonDocument(taskPayload).toJson(QJsonDocument::Compact))}
                });
            }
            wanWorkerSendJson(idx, QJsonObject{
                {"t",          "spawn_tasks"},
                {"parent_id",  w.currentTask},
                {"chain_id",   chainId},
                {"tasks",      tasks}
            });
            wanWorkerAppendLog(idx,
                QString("Agente ha richiesto %1 sub-agente/i.").arg(tasks.size()));
        }

        wanWorkerAppendLog(idx, "Task " + w.currentTask + " completato (agente).");
        w.currentTask.clear();
        w.aiBuf.clear();
        return;
    }

    wanWorkerSendJson(idx, QJsonObject{
        {"t","result"}, {"id", w.currentTask},
        {"status","done"}, {"result", raw}
    });
    wanWorkerAppendLog(idx, "Task " + w.currentTask + " completato (AI).");
    w.currentTask.clear();
    w.aiBuf.clear();

    /* Aggiorna alias legacy */
    if (idx == 0) {
        m_wanCliCurrentTask = QString();
        m_wanCliAiActive    = false;
        m_wanCliAiBuf.clear();
    }
}

/* ── Per-worker AI error ── */
void LanWanPage::onWanWorkerAiError(const QString& msg)
{
    const int idx = wanWorkerIndex(sender());
    if (idx < 0 || idx >= m_wanWorkers.size()) return;
    WanWorker& w = m_wanWorkers[idx];

    QObject::disconnect(w.tokConn);
    QObject::disconnect(w.finConn);
    QObject::disconnect(w.errConn);
    w.tokConn = w.finConn = w.errConn = {};
    w.aiActive    = false;
    w.isAgentTask = false;
    w.agentDepth  = 0;
    w.agentChain.clear();

    wanWorkerSendJson(idx, QJsonObject{
        {"t","result"}, {"id", w.currentTask},
        {"status","error"}, {"result", msg}
    });
    wanWorkerAppendLog(idx, "Task " + w.currentTask + " errore AI: " + msg);
    w.currentTask.clear();

    if (idx == 0) {
        m_wanCliCurrentTask = QString();
        m_wanCliAiActive    = false;
    }
}

/* ── wanWorkerHandleTask — dispatcher per il worker idx ── */
void LanWanPage::wanWorkerHandleTask(int idx, const QString& id,
                                     const QString& kind, const QString& payload)
{
    if (idx < 0 || idx >= m_wanWorkers.size()) return;
    WanWorker& w = m_wanWorkers[idx];

    /* Mappa kind → system prompt AI (condivisa con wanCliHandleTask) */
    static const QHash<QString, QString> kAiPrompts {
        {"ai_query",
            "Sei un assistente AI preciso e conciso. Rispondi SEMPRE in italiano."},
        {"code_assist",
            "Sei un esperto programmatore. Scrivi codice pulito, commentato e funzionante. "
            "Il payload e' JSON con chiavi 'lang' e 'task'. Rispondi in italiano."},
        {"code_review",
            "Sei un esperto revisore di codice. Analizza il codice fornito: "
            "trova bug, vulnerabilita', inefficienze e suggerisci miglioramenti. "
            "Rispondi in italiano con sezioni: Bug, Sicurezza, Performance, Stile."},
        {"math_solve",
            "Sei un professore di matematica. Risolvi il problema passo per passo. "
            "Rispondi in italiano."},
        {"paper_gen",
            "Sei un ricercatore accademico. Genera un paper scientifico completo. "
            "Rispondi in italiano con formattazione Markdown."},
        {"code_translate",
            "Sei un esperto di traduzione tra linguaggi di programmazione. "
            "Traduci il codice preservando la logica. Rispondi solo con il codice + breve spiegazione."},
        {"math_seq",
            "Sei un matematico esperto. Data la sequenza, trova la formula generale. "
            "Rispondi in italiano."},
        {"ai_tutor",
            "Sei un tutor esperto. Spiega l'argomento in modo chiaro. Rispondi in italiano."},
        {"ai_data_analysis",
            "Sei un analista dati. Analizza i dati e trova pattern. Rispondi in italiano."},
        {"ai_730",
            "Sei un esperto fiscalista italiano specializzato nel modello 730. "
            "Rispondi SOLO in italiano."},
        {"ai_tfr",
            "Sei un esperto di diritto del lavoro. Calcola e spiega il TFR. Rispondi in italiano."},
    };

    /* ── llm_agent ── */
    if (kind == "llm_agent") {
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
        if (chainId.isEmpty()) chainId = id;

        QString userMsg = prompt;
        if (!context.isEmpty())
            userMsg = "=== CONTESTO DALL'AGENTE PRECEDENTE ===\n" + context
                    + "\n\n=== IL TUO COMPITO ===\n" + prompt;

        constexpr int kMaxDepth = 4;
        QString sysPrompt = QString("Sei: %1\n\nEsegui il compito con precisione. "
                                    "Rispondi in italiano.\n").arg(role);
        if (depth < kMaxDepth) {
            sysPrompt +=
                "\nSe il tuo risultato richiede analisi aggiuntive puoi rispondere in JSON:\n"
                "{ \"result\": \"<risultato>\", \"spawn\": [{\"role\":\"...\","
                "\"prompt\":\"...\"}] }\n"
                "Se NON hai bisogno di altri agenti rispondi in testo libero normale.\n";
        }

        w.isAgentTask = true;
        w.agentDepth  = depth;
        w.agentChain  = chainId;
        w.aiActive    = true;
        w.aiBuf.clear();

        QObject::disconnect(w.tokConn);
        QObject::disconnect(w.finConn);
        QObject::disconnect(w.errConn);
        AiClient* ai = w.ai;
        if (!ai) return;
        w.tokConn = connect(ai, &AiClient::token,    this, &LanWanPage::onWanWorkerAiToken);
        w.finConn = connect(ai, &AiClient::finished, this, &LanWanPage::onWanWorkerAiFinished);
        w.errConn = connect(ai, &AiClient::error,    this, &LanWanPage::onWanWorkerAiError);
        wanWorkerAppendLog(idx,
            QString("Agente [depth=%1] ruolo: %2").arg(depth).arg(role));
        ai->chat(sysPrompt, userMsg);
        return;
    }

    /* ── Task sincroni (subprocess) ── */
    QString result;
    QString status = "done";

    if (kind == "python_repl" || kind == "eval_script") {
        if (!w.shellAllowed) {
            result = "[SICUREZZA] Esecuzione Python disabilitata.";
            status = "error";
        } else {
            QProcess proc;
            proc.start("python3", {"-c", payload});
            proc.waitForFinished(30000);
            result = proc.readAllStandardOutput() + proc.readAllStandardError();
        }

    } else if (kind == "shell_cmd" || kind == "git_cmd") {
        if (!w.shellAllowed) {
            result = "[SICUREZZA] Esecuzione shell disabilitata.";
            status = "error";
        } else {
            QProcess proc;
            proc.start("bash", {"-c", payload});
            proc.waitForFinished(20000);
            result = proc.readAllStandardOutput() + proc.readAllStandardError();
        }

    } else if (kind == "math_expr") {
        static const QString kMathExprScript =
            "import math,cmath,statistics,sys\n"
            "expr=sys.argv[1]\n"
            "g=dict(vars(math))\n"
            "g.update(vars(cmath))\n"
            "g.update(vars(statistics))\n"
            "g['__builtins__']={}\n"
            "try:\n"
            "    print(eval(expr,g,{}))\n"
            "except Exception as e:\n"
            "    print('Errore:', e)\n";
        QProcess proc;
        proc.start("python3", {"-c", kMathExprScript, "--", payload.trimmed().left(500)});
        proc.waitForFinished(5000);
        result = proc.readAllStandardOutput().trimmed();

    } else if (kind == "matplotlib_plot") {
        if (!w.shellAllowed) {
            result = "[SICUREZZA] Esecuzione Python disabilitata.";
            status = "error";
        } else {
            QProcess proc;
            proc.start("python3", {"-c", payload});
            proc.waitForFinished(30000);
            result = proc.readAllStandardOutput() + proc.readAllStandardError();
        }

    } else if (kind == "graphviz_render") {
        QProcess proc;
        proc.start("dot", {"-Tsvg"});
        proc.write(payload.toUtf8());
        proc.closeWriteChannel();
        proc.waitForFinished(15000);
        const QString svg = proc.readAllStandardOutput();
        result = svg.isEmpty()
            ? "Errore Graphviz: " + proc.readAllStandardError() : svg.left(8000);

    } else if (kind == "system_info") {
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
        result = QString("OS: %1\nKernel: %2\nArch: %3\nHostname: %4\nQt: %5\n%6")
            .arg(QSysInfo::prettyProductName(), QSysInfo::kernelVersion(),
                 QSysInfo::currentCpuArchitecture(), QSysInfo::machineHostName(),
                 qVersion(), ram.trimmed());

    } else if (kind == "net_info") {
        QStringList lines;
        for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
            if (iface.flags() & QNetworkInterface::IsLoopBack) continue;
            for (const QNetworkAddressEntry& e : iface.addressEntries()) {
                if (e.ip().protocol() != QAbstractSocket::IPv4Protocol) continue;
                lines << QString("%1  %2").arg(iface.name(), -12).arg(e.ip().toString());
            }
        }
        result = lines.isEmpty() ? "Nessuna interfaccia IPv4." : lines.join("\n");

    } else {
        /* AI task noto o fallback generico */
        const QString sysPrompt = kAiPrompts.contains(kind)
            ? kAiPrompts[kind]
            : "Sei un assistente AI. Esegui il task. Rispondi in italiano.";

        w.aiActive = true;
        w.aiBuf.clear();
        QObject::disconnect(w.tokConn);
        QObject::disconnect(w.finConn);
        QObject::disconnect(w.errConn);
        AiClient* ai = w.ai;
        if (!ai) return;
        w.tokConn = connect(ai, &AiClient::token,    this, &LanWanPage::onWanWorkerAiToken);
        w.finConn = connect(ai, &AiClient::finished, this, &LanWanPage::onWanWorkerAiFinished);
        w.errConn = connect(ai, &AiClient::error,    this, &LanWanPage::onWanWorkerAiError);
        ai->chat(sysPrompt, kind == "ai_query" ? payload
                          : QString("[kind: %1]\n%2").arg(kind, payload));
        return;
    }

    /* Invia risultato sincrono */
    wanWorkerSendJson(idx, QJsonObject{
        {"t","result"}, {"id", id},
        {"status", status}, {"result", result.trimmed()}
    });
    wanWorkerAppendLog(idx, QString("Task %1 completato (%2).").arg(id, kind));
    w.currentTask.clear();
}

/* ══════════════════════════════════════════════════════════════════════════
/* ══════════════════════════════════════════════════════════════════════════
   onWanDecomposeBtnClicked — invia il compito al MasterAgent e crea task
   ══════════════════════════════════════════════════════════════════════════ */
void LanWanPage::onWanDecomposeBtnClicked()
{
    const QString userTask = m_wanDecomposeInput
        ? m_wanDecomposeInput->toPlainText().trimmed() : QString();
    if (userTask.isEmpty() || !m_ai) return;

    m_wanDecomposeBtn->setEnabled(false);
    if (m_wanDecomposeStatusLbl)
        m_wanDecomposeStatusLbl->setText(
            "\xe2\x8f\xb3  MasterAgent in elaborazione\xe2\x80\xa6");

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
                        "\xe2\x9d\x8c  Errore: " + msg.left(80));
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
                "\xe2\x9a\xa0\xef\xb8\x8f  Nessun subtask nel piano \xe2\x80\x94 riprova");
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
            m_wanSimBtn->setText("\xe2\x9a\x97\xef\xb8\x8f  Prova in locale");
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
            m_wanSrvStatusLbl->setText("\xe2\x9d\x8c  Impossibile avviare il server");
        return;
    }

    /* 2. Imposta il client per connettersi a localhost con nome "Nodo locale" */
    if (m_wanCliHost) m_wanCliHost->setText("127.0.0.1");
    if (m_wanCliPort) m_wanCliPort->setValue(m_wanPortSpin ? m_wanPortSpin->value()
                                                            : P::kWanComputePort);
    if (m_wanCliName) m_wanCliName->setText("Nodo locale (simulazione)");

    /* 3. Avvia la connessione client riusando lo slot esistente
     *    (non cambia il pannello visibile — rimane la vista server) */
    onWanCliConBtnClicked();

    m_wanSimActive = true;
    if (m_wanSimBtn)
        m_wanSimBtn->setText("\xe2\x8f\xb9  Ferma simulazione");
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
    addItem("🔀  Traduci codice (Translitter)",       "code_translate");
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
            QProcess proc;
            proc.start("python3", {"-c", payload});
            proc.waitForFinished(30000);
            result = proc.readAllStandardOutput() + proc.readAllStandardError();
        }

    } else if (kind == "shell_cmd" || kind == "git_cmd") {
        if (!shellAllowed) {
            result = "[SICUREZZA] Esecuzione shell disabilitata. "
                     "Abilita 'Permetti shell' nelle opzioni nodo WAN.";
            status = "error";
        } else {
            QProcess proc;
            proc.start("bash", {"-c", payload});
            proc.waitForFinished(20000);
            result = proc.readAllStandardOutput() + proc.readAllStandardError();
        }

    } else if (kind == "math_expr") {
        /* Valuta via Python: espressione passata come sys.argv[1] — nessuna interpolazione */
        static const QString kMathExprScript =
            "import math,cmath,statistics,sys\n"
            "expr=sys.argv[1]\n"
            "g=dict(vars(math))\n"
            "g.update(vars(cmath))\n"
            "g.update(vars(statistics))\n"
            "g['__builtins__']={}\n"
            "try:\n"
            "    print(eval(expr,g,{}))\n"
            "except Exception as e:\n"
            "    print('Errore:', e)\n";
        QProcess proc;
        proc.start("python3", {"-c", kMathExprScript, "--", payload.trimmed().left(500)});
        proc.waitForFinished(5000);
        result = proc.readAllStandardOutput().trimmed();

    } else if (kind == "matplotlib_plot") {
        if (!shellAllowed) {
            result = "[SICUREZZA] Esecuzione Python disabilitata. "
                     "Abilita 'Permetti shell' nelle opzioni nodo WAN.";
            status = "error";
        } else {
            QProcess proc;
            proc.start("python3", {"-c", payload});
            proc.waitForFinished(30000);
            result = proc.readAllStandardOutput() + proc.readAllStandardError();
        }

    } else if (kind == "graphviz_render") {
        /* Renderizza DOT → SVG */
        QProcess proc;
        proc.start("dot", {"-Tsvg"});
        proc.write(payload.toUtf8());
        proc.closeWriteChannel();
        proc.waitForFinished(15000);
        const QString svg = proc.readAllStandardOutput();
        result = svg.isEmpty() ? "Errore Graphviz: " + proc.readAllStandardError() : svg.left(8000);

    } else if (kind == "file_read") {
        QFile f(payload.trimmed());
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream ts(&f);
            result = ts.readAll().left(12000);
        } else {
            result = "File non trovato o non leggibile: " + payload;
            status = "error";
        }

    } else if (kind == "file_write") {
        /* payload = JSON {"path":"...","content":"..."} */
        QJsonParseError jerr;
        const QJsonDocument doc = QJsonDocument::fromJson(payload.toUtf8(), &jerr);
        if (doc.isObject()) {
            const QString path    = doc.object()["path"].toString();
            const QString content = doc.object()["content"].toString();
            QFile f(path);
            if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream ts(&f); ts << content;
                result = "Scritto: " + path;
            } else {
                result = "Impossibile scrivere: " + path;
                status = "error";
            }
        } else {
            result = "Payload non valido: atteso JSON {\"path\":\"...\",\"content\":\"...\"}";
            status = "error";
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
